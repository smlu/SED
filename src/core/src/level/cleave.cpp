#include "sed/level/cleave.hpp"

#include "sed/level/geometry.hpp"
#include "sed/level/uv.hpp"
#include "sed/level/cleave_stroke_reach.hpp"
#include "geometry_precision.hpp"
#include "cleave_index_utils.hpp"
#include "cog_reference_utils.hpp"
#include "cleave_projection_utils.hpp"
#include "extrude_arrangement_context.hpp"
#include "extrude_contained_ring.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sed::level
{
namespace
{
struct CleaveProfileContext;

struct CutPlaneDescriptor
{
    TVector normal{};
    TVector anchor{};
};

std::optional<double> coordinateBoundaryDeviationLimit(
    const Sector& sector,
    const Surface& surface) noexcept;
std::optional<double> coordinateBoundaryResidual(
    const Sector& sector,
    const Surface& surface,
    double admissibilityLimit);
double coordinateEdgeParameterError(
    const Vertex& lineStart,
    const Vertex& lineEnd,
    double deviationLimit) noexcept;
bool coordinateLineParameter(
    const Vertex& point,
    const Vertex& lineStart,
    const Vertex& lineEnd,
    double& parameter,
    double deviationLimit) noexcept;
}

static bool deleteSectorGeometry(Level&, std::int32_t) noexcept;

static bool levelCleaveSectorImpl(
    Level& level,
    std::int32_t sector,
    const TVector& planeNormal,
    double px,
    double py,
    double pz,
    bool preserveOriginalIdentity,
    CleaveProfileContext* context = nullptr);

static bool levelCleaveSurfaceImpl(Level&, SurfaceRef, const TVector&, double, double, double,
    CleaveProfileContext* = nullptr, detail::ExtrudeArrangementContext* = nullptr);
static bool levelConnectSurfacesImpl(Level&, SurfaceRef, SurfaceRef, CleaveProfileContext* = nullptr);

namespace
{

// Scoped Surface Cleave may deliberately leave an authored face out of the
// validating profile set. Keep a separate immutable contour-only snapshot
// for incident-chain proofs. It carries indexed topology and source positions
// only; UVs, cached normals, and other authored metadata stay out of this
// evidence.
struct CleaveTopologyFace;
using CleaveTopologyFacePtr = std::shared_ptr<const CleaveTopologyFace>;
using CleaveTopologyCollection = std::vector<CleaveTopologyFacePtr>;
using CleaveTopologyCollectionPtr = std::shared_ptr<const CleaveTopologyCollection>;

// A face profile is immutable for the whole edit. It stores only source
// corners, never a descendant's newly measured error. Clipping is affine on
// every source edge, so these fixed supporting and planar intervals also
// bound all generated convex combinations without accumulating a tolerance.
struct CleaveFaceProfile
{
    Sector geometry;
    std::vector<std::int32_t> originalCornerIds;
    TVector normal;
    TVector anchor;
    double minimum{};
    double maximum{};
    double supportMinimum{};
    bool cap{};
    bool certifiedReflex{};
    bool originalUnsetNormal{};
    double boundaryDeviation{};
    CleaveTopologyCollectionPtr incidentTopologies;
};

struct CleaveTopologyFace
{
    std::vector<std::int32_t> indices;
    std::vector<TVector> points;
};

CleaveTopologyFacePtr captureTopologyFace(
    const Sector& sector,
    const Surface& face)
{
    if ( face.vertices.empty() )
    {
        return {};
    }
    auto snapshot = std::make_shared<CleaveTopologyFace>();
    snapshot->indices.reserve(face.vertices.size());
    snapshot->points.reserve(face.vertices.size());
    for ( const std::int32_t index : face.vertices )
    {
        if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
        {
            return {};
        }
        snapshot->indices.push_back(index);
        snapshot->points.push_back(precision::position(
            sector.vertices[static_cast<std::size_t>(index)]));
    }
    return snapshot;
}

struct CleaveSectorProfile
{
    std::vector<std::size_t> faces;
    std::vector<std::size_t> supports;
    std::vector<CleaveTopologyFacePtr> topologyFaces;
    std::vector<bool> validateFaces;
    bool closed{};
    bool inheritedRounding{};
};

struct CleaveProfileContext
{
    std::vector<CleaveFaceProfile> originals;
    std::map<std::int32_t, CleaveSectorProfile> sectors;
    std::optional<detail::GeometryCogReferences> cogReferences;

    bool capture(const Level& level, const std::vector<std::int32_t>& affected, const bool requireClosed,
        const bool captureReferences = true,
        const std::map<std::int32_t, std::vector<bool>>* const validationScope = nullptr)
    {
        if ( captureReferences && !cogReferences )
        {
            cogReferences.emplace(level);
        }
        for ( const auto index : affected )
        {
            const Sector& sector = level.sectors[static_cast<std::size_t>(index)];
            CleaveSectorProfile state;
            state.closed = precision::closedBoundary(sector);
            const std::vector<bool>* scope = nullptr;
            if ( validationScope != nullptr )
            {
                const auto found = validationScope->find(index);
                if ( found != validationScope->end() )
                {
                    scope = &found->second;
                }
            }
            state.inheritedRounding = validationScope == nullptr
                && state.closed && !sectorIsConvex(sector);
            if ( (requireClosed && !state.closed)
                || (validationScope == nullptr && state.closed && !precision::sourceSectorIsConvex(sector)) )
            {
                return false;
            }
            if ( validationScope != nullptr )
            {
                state.validateFaces.reserve(sector.surfaces.size());
            }
            // Capture contour-only evidence for every face sharing an
            // indexed vertex with a scoped face. This leaves validateFaces
            // unchanged: the extra snapshot cannot make an unrelated face
            // participate in validation or normalization.
            std::vector<bool> captureTopology(
                sector.surfaces.size(), validationScope == nullptr);
            if ( validationScope != nullptr && scope != nullptr )
            {
                std::vector<bool> scopedVertices(sector.vertices.size(), false);
                for ( std::size_t scopedIndex = 0;
                    scopedIndex < sector.surfaces.size(); ++scopedIndex )
                {
                    if ( scopedIndex >= scope->size() || !(*scope)[scopedIndex] )
                    {
                        continue;
                    }
                    captureTopology[scopedIndex] = true;
                    const Surface& scopedFace = sector.surfaces[scopedIndex];
                    for ( const std::int32_t scopedVertex : scopedFace.vertices )
                    {
                        if ( scopedVertex >= 0
                            && static_cast<std::size_t>(scopedVertex) < scopedVertices.size() )
                        {
                            scopedVertices[static_cast<std::size_t>(scopedVertex)] = true;
                        }
                    }
                }
                for ( std::size_t candidateIndex = 0;
                    candidateIndex < sector.surfaces.size(); ++candidateIndex )
                {
                    const Surface& candidate = sector.surfaces[candidateIndex];
                    if ( std::any_of(
                            candidate.vertices.begin(), candidate.vertices.end(),
                            [&scopedVertices](const std::int32_t vertex) noexcept
                            {
                                return vertex >= 0
                                    && static_cast<std::size_t>(vertex) < scopedVertices.size()
                                    && scopedVertices[static_cast<std::size_t>(vertex)];
                            }) )
                    {
                        captureTopology[candidateIndex] = true;
                    }
                }
            }
            state.topologyFaces.reserve(sector.surfaces.size());
            for ( std::size_t surfaceIndex = 0;
                surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
            {
                state.topologyFaces.push_back(captureTopology[surfaceIndex]
                    ? captureTopologyFace(sector, sector.surfaces[surfaceIndex])
                    : CleaveTopologyFacePtr{});
            }
            const CleaveTopologyCollectionPtr incidentTopologies =
                std::make_shared<const CleaveTopologyCollection>(state.topologyFaces);
            for ( std::size_t surfaceIndex = 0;
                surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
            {
                const Surface& face = sector.surfaces[surfaceIndex];
                const bool validateFace = validationScope == nullptr
                    || (scope != nullptr && surfaceIndex < scope->size() && (*scope)[surfaceIndex]);
                if ( validationScope != nullptr )
                {
                    state.validateFaces.push_back(validateFace);
                }
                if ( !validateFace )
                {
                    // Surface Cleave may leave unrelated authored faces with
                    // legacy cached data. Keep a stable profile slot for
                    // index remapping, but do not inspect or normalize any of
                    // that face's metadata.
                    state.faces.push_back(originals.size());
                    CleaveFaceProfile profile;
                    profile.incidentTopologies = incidentTopologies;
                    originals.push_back(std::move(profile));
                    continue;
                }
                std::optional<Surface> resolved;
                if ( face.normal.x == 0.0 && face.normal.y == 0.0 && face.normal.z == 0.0 )
                {
                    // Connect accepts an unset original normal on solids and
                    // standalone polygons. Derive it only for inspection;
                    // leave the authored cache unchanged.
                    resolved = face;
                    resolved->normal = surfaceCalcNormalNewell(sector, face);
                }
                const Surface& input = resolved ? *resolved : face;
                if ( !precision::sourceSurfaceIsPlanar(sector, input)
                    || !precision::sourceSurfaceIsConvex(sector, input) )
                {
                    return false;
                }
                state.inheritedRounding = state.inheritedRounding
                    || !surfaceIsPlanar(sector, input) || !surfaceIsConvex(sector, input);
                CleaveFaceProfile profile;
                profile.incidentTopologies = incidentTopologies;
                profile.originalUnsetNormal = resolved.has_value();
                profile.normal = surfaceCalcNormalNewell(sector, face);
                // Admission already checked finite, same-facing cached
                // normals. Rounded caches need not encode the reconstructed
                // plane exactly; the immutable frame below is geometric.
                if ( input.normal.dot(profile.normal) <= 0.0 )
                {
                    return false;
                }
                Surface polygon;
                polygon.normal = profile.normal;
                polygon.texVertices = face.texVertices;
                profile.originalCornerIds.reserve(face.vertices.size());
                for ( const auto vertex : face.vertices )
                {
                    polygon.vertices.push_back(static_cast<std::int32_t>(profile.geometry.vertices.size()));
                    profile.originalCornerIds.push_back(vertex);
                    profile.geometry.vertices.push_back(sector.vertices[static_cast<std::size_t>(vertex)]);
                }
                profile.geometry.surfaces.push_back(std::move(polygon));
                const double admissibilityLimit = coordinateBoundaryDeviationLimit(
                    profile.geometry, profile.geometry.surfaces.front()).value_or(0.0);
                profile.boundaryDeviation = coordinateBoundaryResidual(
                    profile.geometry, profile.geometry.surfaces.front(), admissibilityLimit).value_or(0.0);
                profile.anchor = precision::position(profile.geometry.vertices.front());
                for ( const Vertex& vertex : profile.geometry.vertices )
                {
                    const double distance = profile.normal.dot(precision::position(vertex) - profile.anchor);
                    if ( !std::isfinite(distance) )
                    {
                        return false;
                    }
                    profile.minimum = std::min(profile.minimum, distance);
                    profile.maximum = std::max(profile.maximum, distance);
                }
                for ( const Vertex& vertex : sector.vertices )
                {
                    const double distance = profile.normal.dot(precision::position(vertex) - profile.anchor);
                    if ( !std::isfinite(distance) )
                    {
                        return false;
                    }
                    profile.supportMinimum = std::min(profile.supportMinimum, distance);
                }
                state.faces.push_back(originals.size());
                state.supports.push_back(originals.size());
                originals.push_back(std::move(profile));
            }
            sectors.emplace(index, std::move(state));
        }
        return true;
    }

    bool validatesFace(const std::int32_t sectorIndex, const std::size_t surfaceIndex) const noexcept
    {
        const auto sector = sectors.find(sectorIndex);
        if ( sector == sectors.end() || sector->second.validateFaces.empty() )
        {
            return true;
        }
        return surfaceIndex >= sector->second.validateFaces.size()
            || sector->second.validateFaces[surfaceIndex];
    }

    void splitFace(const SurfaceRef source, const SurfaceRef added)
    {
        auto& faces = sectors.at(source.sector).faces;
        auto& topologyFaces = sectors.at(source.sector).topologyFaces;
        auto& validateFaces = sectors.at(source.sector).validateFaces;
        const auto original = faces.at(static_cast<std::size_t>(source.surface));
        faces.resize(static_cast<std::size_t>(added.surface) + 1);
        faces[static_cast<std::size_t>(added.surface)] = original;
        topologyFaces.resize(static_cast<std::size_t>(added.surface) + 1);
        topologyFaces[static_cast<std::size_t>(added.surface)] =
            topologyFaces.at(static_cast<std::size_t>(source.surface));
        if ( !validateFaces.empty() )
        {
            validateFaces.resize(static_cast<std::size_t>(added.surface) + 1, true);
            validateFaces[static_cast<std::size_t>(added.surface)] =
                validateFaces.at(static_cast<std::size_t>(source.surface));
        }
    }

    void splitSector(const std::int32_t original, const std::int32_t added,
        const std::vector<std::optional<SurfaceRef>>& remap)
    {
        const auto oldFaces = sectors.at(original).faces;
        const auto oldTopologyFaces = sectors.at(original).topologyFaces;
        const auto oldValidateFaces = sectors.at(original).validateFaces;
        CleaveSectorProfile next = sectors.at(original);
        auto& retained = sectors.at(original);
        retained.faces.clear();
        retained.topologyFaces.clear();
        if ( !retained.validateFaces.empty() )
        {
            retained.validateFaces.clear();
        }
        next.faces.clear();
        next.topologyFaces.clear();
        if ( !next.validateFaces.empty() )
        {
            next.validateFaces.clear();
        }
        sectors.emplace(added, std::move(next));
        for ( std::size_t face = 0; face < remap.size(); ++face )
        {
            if ( !remap[face] )
            {
                continue;
            }
            auto& destination = sectors.at(remap[face]->sector).faces;
            destination.resize(std::max(destination.size(), static_cast<std::size_t>(remap[face]->surface) + 1));
            destination[static_cast<std::size_t>(remap[face]->surface)] = oldFaces.at(face);
            auto& destinationTopologyFaces = sectors.at(remap[face]->sector).topologyFaces;
            destinationTopologyFaces.resize(
                std::max(destinationTopologyFaces.size(),
                    static_cast<std::size_t>(remap[face]->surface) + 1));
            if ( face < oldTopologyFaces.size() )
            {
                destinationTopologyFaces[static_cast<std::size_t>(remap[face]->surface)] =
                    oldTopologyFaces[face];
            }
            auto& destinationValidateFaces = sectors.at(remap[face]->sector).validateFaces;
            if ( !oldValidateFaces.empty() )
            {
                destinationValidateFaces.resize(destination.size(), true);
                destinationValidateFaces[static_cast<std::size_t>(remap[face]->surface)] = oldValidateFaces.at(face);
            }
        }
        if ( cogReferences )
        {
            cogReferences->splitSector(original, remap);
        }
    }

    void eraseSectors(const std::vector<std::int32_t>& deletedAscending)
    {
        std::map<std::int32_t, CleaveSectorProfile> retained;
        for ( auto& [index, profile] : sectors )
        {
            if ( std::binary_search(deletedAscending.begin(), deletedAscending.end(), index) )
            {
                continue;
            }
            const auto removedBefore = std::lower_bound(deletedAscending.begin(), deletedAscending.end(), index)
                - deletedAscending.begin();
            retained.emplace(index - static_cast<std::int32_t>(removedBefore), std::move(profile));
        }
        sectors = std::move(retained);
        if ( cogReferences )
        {
            for ( auto it = deletedAscending.rbegin(); it != deletedAscending.rend(); ++it )
            {
                cogReferences->eraseSector(*it);
            }
        }
    }

    void addCap(const Sector& sector, const std::int32_t sectorIndex,
        const TVector& normal, const TVector& anchor, const bool certifiedBoundary)
    {
        CleaveFaceProfile profile;
        profile.normal = normal;
        profile.anchor = anchor;
        profile.cap = true;
        profile.certifiedReflex = certifiedBoundary && sectors.at(sectorIndex).inheritedRounding;
        const Surface& face = sector.surfaces.back();
        Surface polygon;
        polygon.normal = normal;
        for ( const auto vertex : face.vertices )
        {
            polygon.vertices.push_back(static_cast<std::int32_t>(profile.geometry.vertices.size()));
            profile.geometry.vertices.push_back(sector.vertices[static_cast<std::size_t>(vertex)]);
        }
        profile.geometry.surfaces.push_back(std::move(polygon));
        const double admissibilityLimit = coordinateBoundaryDeviationLimit(
            profile.geometry, profile.geometry.surfaces.front()).value_or(0.0);
        profile.boundaryDeviation = coordinateBoundaryResidual(
            profile.geometry, profile.geometry.surfaces.front(), admissibilityLimit).value_or(0.0);
        auto& state = sectors.at(sectorIndex);
        state.faces.push_back(originals.size());
        state.supports.push_back(originals.size());
        state.topologyFaces.emplace_back();
        if ( !state.validateFaces.empty() )
        {
            state.validateFaces.push_back(true);
        }
        originals.push_back(std::move(profile));
    }

    std::optional<double> sourceBoundaryDeviation(const SurfaceRef ref) const noexcept
    {
        const auto sector = sectors.find(ref.sector);
        if ( sector == sectors.end() || ref.surface < 0
            || static_cast<std::size_t>(ref.surface) >= sector->second.faces.size() )
        {
            return std::nullopt;
        }
        const std::size_t profileIndex = sector->second.faces[static_cast<std::size_t>(ref.surface)];
        if ( profileIndex >= originals.size() || !(originals[profileIndex].boundaryDeviation > 0.0) )
        {
            return std::nullopt;
        }
        return originals[profileIndex].boundaryDeviation;
    }

    const CleaveFaceProfile* sourceFaceProfile(const SurfaceRef ref) const noexcept
    {
        const auto sector = sectors.find(ref.sector);
        if ( sector == sectors.end() || ref.surface < 0
            || static_cast<std::size_t>(ref.surface) >= sector->second.faces.size() )
        {
            return nullptr;
        }
        const std::size_t profileIndex = sector->second.faces[static_cast<std::size_t>(ref.surface)];
        if ( profileIndex >= originals.size() || originals[profileIndex].geometry.vertices.empty() )
        {
            return nullptr;
        }
        return &originals[profileIndex];
    }

    const CleaveTopologyFace* sourceTopologyFace(const SurfaceRef ref) const noexcept
    {
        const auto sector = sectors.find(ref.sector);
        if ( sector == sectors.end() || ref.surface < 0
            || static_cast<std::size_t>(ref.surface) >= sector->second.topologyFaces.size() )
        {
            return nullptr;
        }
        return sector->second.topologyFaces[static_cast<std::size_t>(ref.surface)].get();
    }

    double errorAt(const CleaveFaceProfile& profile, const TVector& point) const noexcept
    {
        if ( profile.cap )
        {
            return precision::planeError(profile.normal, point, profile.anchor);
        }
        const auto error = precision::surfacePlaneError(profile.geometry, profile.geometry.surfaces.front());
        return error.at(profile.normal, point, profile.anchor);
    }

    bool planarFace(const Sector& sector, const Surface& face, const std::size_t id,
        const bool allowUnsetNormal) const noexcept
    {
        const auto& profile = originals[id];
        const TVector normal = (allowUnsetNormal || profile.originalUnsetNormal)
            && face.normal.x == 0.0 && face.normal.y == 0.0 && face.normal.z == 0.0
            ? surfaceCalcNormalNewell(sector, face) : face.normal;
        if ( !precision::finite(normal) || normal.dot(profile.normal) <= 0.0 )
        {
            return false;
        }
        for ( const auto vertex : face.vertices )
        {
            const auto point = precision::position(sector.vertices[static_cast<std::size_t>(vertex)]);
            const double distance = profile.normal.dot(point - profile.anchor);
            const double error = errorAt(profile, point);
            if ( !std::isfinite(distance) || !std::isfinite(error) || error < 0.0
                || distance < profile.minimum - error || distance > profile.maximum + error )
            {
                return false;
            }
        }
        return !profile.cap || surfaceIsPlanar(sector, face);
    }

    bool convexFace(const Sector& sector, const Surface& face, const std::size_t id) const noexcept
    {
        const auto& profile = originals[id];
        if ( profile.cap && !precision::surfaceHasSimpleBoundary(sector, face) )
        {
            return false;
        }
        const auto& points = profile.geometry.vertices;
        for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
        {
            const auto count = face.vertices.size();
            const auto a = precision::position(sector.vertices[face.vertices[(corner + count - 1) % count]]);
            const auto b = precision::position(sector.vertices[face.vertices[corner]]);
            const auto c = precision::position(sector.vertices[face.vertices[(corner + 1) % count]]);
            const double turn = (b - a).cross(c - b).dot(profile.normal);
            if ( !std::isfinite(turn) )
            {
                return false;
            }
            if ( turn >= -precision::cornerTurnError(a, b, c, profile.normal) )
            {
                continue;
            }
            // Only the same source corner, with adjacent output points on its
            // incident edges, can inherit a reflex. For a cap, those corners
            // were independently certified against the cut's source edges and
            // both side contours; no measured angular budget was captured.
            // A later clipping corner cannot spend an earlier corner's proof.
            bool inherited = false;
            if ( !profile.cap || profile.certifiedReflex )
            {
                for ( std::size_t original = 0; original < points.size(); ++original )
                {
                    const auto before = precision::position(points[(original + points.size() - 1) % points.size()]);
                    const auto current = precision::position(points[original]);
                    const auto after = precision::position(points[(original + 1) % points.size()]);
                    double incoming{};
                    double outgoing{};
                    if ( precision::samePoint(b, current) && precision::onSegment(a, before, current, incoming)
                        && precision::onSegment(c, current, after, outgoing) )
                    {
                        inherited = true;
                        break;
                    }
                }
            }
            if ( !inherited )
            {
                return false;
            }
        }
        return !profile.cap || profile.certifiedReflex || surfaceIsConvex(sector, face);
    }

    bool convexSector(const Sector& sector, const CleaveSectorProfile& state) const noexcept
    {
        for ( const auto id : state.supports )
        {
            const auto& profile = originals[id];
            for ( const Vertex& vertex : sector.vertices )
            {
                const auto point = precision::position(vertex);
                const double distance = profile.normal.dot(point - profile.anchor);
                const double error = errorAt(profile, point);
                if ( !std::isfinite(distance) || !std::isfinite(error) || error < 0.0
                    || distance < profile.supportMinimum - error )
                {
                    return false;
                }
            }
        }
        return true;
    }
};


struct Plane
{
    TVector normal{};
    double x{};
    double y{};
    double z{};
};

struct SectorPlaneSideCounts
{
    std::int32_t negative{};
    std::int32_t positive{};
};

struct TrackedSectorFragment
{
    std::int32_t sector{};
    bool insideAllPlanes{};
};

enum class SectorPlaneFilter
{
    AllSurfaces,
    SolidSurfacesOnly,
};

bool isValidSurfaceRef(const Level& level, const SurfaceRef ref) noexcept
{
    if (ref.sector < 0 || ref.surface < 0)
    {
        return false;
    }

    const std::size_t sectorIndex = static_cast<std::size_t>(ref.sector);
    if (sectorIndex >= level.sectors.size())
    {
        return false;
    }

    const Sector& sector = level.sectors[sectorIndex];
    const std::size_t surfaceIndex = static_cast<std::size_t>(ref.surface);
    return surfaceIndex < sector.surfaces.size();
}

bool sectorIsOnPositivePlaneSide(
    const Sector& sector,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz) noexcept
{
    for (const Vertex& vertex : sector.vertices)
    {
        const double distance = planeNormal.dot(
            vertex.x - px,
            vertex.y - py,
            vertex.z - pz);
        if (std::abs(distance) > precision::planeError(planeNormal,
                precision::position(vertex), {px, py, pz}))
        {
            return distance > 0.0;
        }
    }
    return false;
}

bool vxEq(const Vertex& a, const Vertex& b) noexcept
{
    return precision::samePoint(precision::position(a), precision::position(b));
}

bool surfaceHasTextureCorners(const Surface& surface) noexcept
{
    return surface.vertices.size() == surface.texVertices.size();
}

bool surfaceHasFiniteUvValues(const Surface& surface) noexcept
{
    return std::isfinite(surface.uScale)
        && std::isfinite(surface.vScale)
        && std::all_of(
            surface.texVertices.begin(),
            surface.texVertices.end(),
            [](const TexVertex& textureVertex)
            {
                return std::isfinite(textureVertex.u)
                    && std::isfinite(textureVertex.v);
            });
}

bool surfaceHasValidVertexTopology(const Sector& sector, const Surface& surface) noexcept
{
    return std::all_of(
        surface.vertices.begin(),
        surface.vertices.end(),
        [&sector](const std::int32_t vertex)
        {
            return vertex >= 0 && static_cast<std::size_t>(vertex) < sector.vertices.size();
        });
}

bool surfaceHasValidTextureTopology(const Sector& sector, const Surface& surface) noexcept
{
    return surfaceHasTextureCorners(surface)
        && surfaceHasFiniteUvValues(surface)
        && surfaceHasValidVertexTopology(sector, surface);
}

bool surfaceContainsIndexedEdge(
    const Surface& surface,
    const std::int32_t edgeA,
    const std::int32_t edgeB) noexcept
{
    for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
    {
        const std::int32_t a = surface.vertices[corner];
        const std::int32_t b = surface.vertices[(corner + 1) % surface.vertices.size()];
        if ( (a == edgeA && b == edgeB) || (a == edgeB && b == edgeA) )
        {
            return true;
        }
    }
    return false;
}

struct CoordinateEdgeInterval
{
    double first{};
    double second{};
    double minimum{};
    double maximum{};
};

// Imported portal contours may carry a small, bounded in-plane decimal
// residual even when their source edge is straight. This admission budget is
// derived from each immutable face's own feature width and is used only to
// certify a complete boundary chain. It is never used to weld vertices or to
// relax generated-plane validation.
constexpr double kSourceBoundaryRelativeRounding = 0.003;
constexpr double kSourceBoundaryAbsoluteRounding = 0.001;

std::optional<double> coordinateBoundaryDeviationLimit(
    const Sector& sector,
    const Surface& surface) noexcept
{
    if ( surface.vertices.size() < 3 || !surfaceHasValidVertexTopology(sector, surface) )
    {
        return std::nullopt;
    }
    const precision::SurfaceArea area = precision::surfaceArea(sector, surface, false);
    if ( !area.valid )
    {
        return std::nullopt;
    }
    const long double magnitude = std::hypot(area.value[0], area.value[1], area.value[2]);
    long double perimeter = 0.0L;
    for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
    {
        const std::size_t next = (corner + 1) % surface.vertices.size();
        const TVector first = precision::position(
            sector.vertices[static_cast<std::size_t>(surface.vertices[corner])]);
        const TVector second = precision::position(
            sector.vertices[static_cast<std::size_t>(surface.vertices[next])]);
        const TVector edge = second - first;
        const long double length = std::hypot(edge.x, edge.y, edge.z);
        if ( !std::isfinite(length) )
        {
            return std::nullopt;
        }
        perimeter += length;
    }
    if ( !std::isfinite(magnitude) || !std::isfinite(perimeter)
        || !(magnitude > 0.0L) || !(perimeter > 0.0L) )
    {
        return std::nullopt;
    }
    const long double width = magnitude / perimeter;
    const double limit = std::min(
        kSourceBoundaryAbsoluteRounding,
        kSourceBoundaryRelativeRounding * static_cast<double>(width));
    if ( !std::isfinite(width) || !std::isfinite(limit) || !(limit > 0.0) )
    {
        return std::nullopt;
    }
    return limit;
}

std::optional<double> coordinateBoundaryResidual(
    const Sector& sector,
    const Surface& surface,
    const double admissibilityLimit)
{
    if ( !std::isfinite(admissibilityLimit) || admissibilityLimit < 0.0
        || surface.vertices.size() < 3 || !surfaceHasValidVertexTopology(sector, surface) )
    {
        return std::nullopt;
    }

    const std::size_t count = surface.vertices.size();
    std::vector<TVector> originalPoints;
    originalPoints.reserve(count);
    for ( const std::int32_t vertexIndex : surface.vertices )
    {
        if ( vertexIndex < 0
            || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size() )
        {
            return std::nullopt;
        }
        originalPoints.push_back(precision::position(
            sector.vertices[static_cast<std::size_t>(vertexIndex)]));
    }

    std::vector<bool> removed(count, false);
    std::size_t remaining = count;
    for ( std::size_t index = 0; index < count && remaining > 3; ++index )
    {
        const TVector& previous = originalPoints[(index + count - 1) % count];
        const TVector& current = originalPoints[index];
        const TVector& next = originalPoints[(index + 1) % count];
        const Vertex previousVertex{ previous.x, previous.y, previous.z, 0 };
        const Vertex currentVertex{ current.x, current.y, current.z, 0 };
        const Vertex nextVertex{ next.x, next.y, next.z, 0 };
        double parameter{};
        const double parameterError = coordinateEdgeParameterError(
            previousVertex, nextVertex, admissibilityLimit);
        if ( !std::isfinite(parameterError)
            || !coordinateLineParameter(
                currentVertex, previousVertex, nextVertex,
                parameter, admissibilityLimit)
            || !(parameter > parameterError)
            || !(parameter < 1.0 - parameterError) )
        {
            continue;
        }
        removed[index] = true;
        --remaining;
    }

    std::vector<bool> visited(count, false);
    double maximum = 0.0;
    for ( std::size_t runIndex = 0; runIndex < count; ++runIndex )
    {
        if ( !removed[runIndex] || visited[runIndex] )
        {
            continue;
        }

        std::size_t start = runIndex;
        for ( std::size_t steps = 0; steps + 1 < count; ++steps )
        {
            const std::size_t previous = (start + count - 1) % count;
            if ( !removed[previous] || visited[previous] )
            {
                break;
            }
            start = previous;
        }
        std::size_t end = runIndex;
        for ( std::size_t steps = 0; steps + 1 < count; ++steps )
        {
            const std::size_t next = (end + 1) % count;
            if ( !removed[next] || visited[next] )
            {
                break;
            }
            end = next;
        }

        const std::size_t chordStart = (start + count - 1) % count;
        const std::size_t chordEnd = (end + 1) % count;
        const Vertex firstVertex{
            originalPoints[chordStart].x,
            originalPoints[chordStart].y,
            originalPoints[chordStart].z,
            0 };
        const Vertex lastVertex{
            originalPoints[chordEnd].x,
            originalPoints[chordEnd].y,
            originalPoints[chordEnd].z,
            0 };
        const double finalParameterError = coordinateEdgeParameterError(
            firstVertex, lastVertex, admissibilityLimit);
        if ( !std::isfinite(finalParameterError) )
        {
            return std::nullopt;
        }

        double previousParameter = 0.0;
        for ( std::size_t steps = 0; steps < count; ++steps )
        {
            const std::size_t position = (chordStart + steps) % count;
            const TVector& point = originalPoints[position];
            double parameter = 0.0;
            if ( position != chordStart && position != chordEnd )
            {
                const Vertex pointVertex{ point.x, point.y, point.z, 0 };
                if ( !coordinateLineParameter(
                        pointVertex, firstVertex, lastVertex,
                        parameter, admissibilityLimit) )
                {
                    return std::nullopt;
                }
            }
            else if ( position == chordEnd )
            {
                parameter = 1.0;
            }
            const TVector chord = originalPoints[chordEnd]
                - originalPoints[chordStart];
            const TVector projected{
                std::fma(parameter, chord.x, originalPoints[chordStart].x),
                std::fma(parameter, chord.y, originalPoints[chordStart].y),
                std::fma(parameter, chord.z, originalPoints[chordStart].z) };
            const TVector residual = point - projected;
            const double scale = std::max({ 1.0,
                std::abs(point.x), std::abs(point.y), std::abs(point.z),
                std::abs(projected.x), std::abs(projected.y),
                std::abs(projected.z) });
            const double deviation = std::hypot(
                residual.x, residual.y, residual.z);
            if ( !std::isfinite(parameter)
                || !precision::finite(residual)
                || !std::isfinite(deviation)
                || (steps > 0
                    && !(parameter > previousParameter + finalParameterError))
                || deviation > admissibilityLimit
                    + precision::kRoundoff * scale )
            {
                return std::nullopt;
            }
            previousParameter = parameter;
            maximum = std::max(maximum, deviation);
            if ( position == chordEnd )
            {
                break;
            }
        }

        for ( std::size_t position = start;; position = (position + 1) % count )
        {
            visited[position] = true;
            if ( position == end )
            {
                break;
            }
        }
    }
    return std::isfinite(maximum) ? std::optional{maximum} : std::nullopt;
}

double coordinateEdgeParameterError(
    const Vertex& lineStart,
    const Vertex& lineEnd,
    const double deviationLimit) noexcept
{
    const TVector edge = precision::position(lineEnd) - precision::position(lineStart);
    const double magnitude = std::max({ std::abs(edge.x), std::abs(edge.y), std::abs(edge.z) });
    if ( !std::isfinite(magnitude) || !(magnitude > 0.0)
        || !std::isfinite(deviationLimit) || deviationLimit < 0.0 )
    {
        return std::numeric_limits<double>::infinity();
    }
    return deviationLimit / magnitude
        + precision::kRoundoff * std::max({ 1.0, std::abs(lineStart.x), std::abs(lineStart.y),
            std::abs(lineStart.z), std::abs(lineEnd.x), std::abs(lineEnd.y), std::abs(lineEnd.z) });
}

bool coordinateLineParameter(
    const Vertex& point,
    const Vertex& lineStart,
    const Vertex& lineEnd,
    double& parameter,
    const double deviationLimit = 0.0) noexcept
{
    const TVector pointPosition = precision::position(point);
    const TVector startPosition = precision::position(lineStart);
    const TVector edge = precision::position(lineEnd) - startPosition;
    const double magnitude = std::max({ std::abs(edge.x), std::abs(edge.y), std::abs(edge.z) });
    if ( !precision::finite(pointPosition) || !precision::finite(startPosition)
        || !precision::finite(edge) || !(magnitude > 0.0) )
    {
        return false;
    }

    const bool useX = std::abs(edge.x) == magnitude;
    const bool useY = !useX && std::abs(edge.y) == magnitude;
    const double delta = useX ? edge.x : useY ? edge.y : edge.z;
    const double offset = useX ? pointPosition.x - startPosition.x
        : useY ? pointPosition.y - startPosition.y
               : pointPosition.z - startPosition.z;
    if ( !(delta != 0.0) )
    {
        return false;
    }
    parameter = offset / delta;
    if ( !std::isfinite(parameter) )
    {
        return false;
    }

    if ( !std::isfinite(deviationLimit) || deviationLimit < 0.0 )
    {
        return false;
    }

    // A nonzero limit is admitted only by a source-scoped chain caller. It
    // measures the point's perpendicular residual against the immutable line;
    // no coordinate is changed and exact callers retain the old certificate.
    if ( deviationLimit > 0.0 )
    {
        const TVector projected{
            std::fma(parameter, edge.x, startPosition.x),
            std::fma(parameter, edge.y, startPosition.y),
            std::fma(parameter, edge.z, startPosition.z) };
        const TVector residual = pointPosition - projected;
        const double scale = std::max({ 1.0,
            std::abs(startPosition.x), std::abs(startPosition.y), std::abs(startPosition.z),
            std::abs(pointPosition.x), std::abs(pointPosition.y), std::abs(pointPosition.z),
            std::abs(projected.x), std::abs(projected.y), std::abs(projected.z) });
        const double error = deviationLimit + precision::kRoundoff * scale;
        return precision::finite(residual) && std::isfinite(error)
            && std::hypot(residual.x, residual.y, residual.z) <= error;
    }

    // This is the same component-wise arithmetic certificate used by
    // precision::onSegment, without its [0,1] restriction. It recognizes a
    // collinear edge that covers the requested segment from either side while
    // rejecting a merely nearby portal edge.
    const auto matches = [parameter](
        const double value,
        const double component,
        const double start,
        const double end) noexcept
    {
        return std::abs(value - parameter * component) <= precision::kRoundoff
            * (std::abs(start) + std::abs(end) + std::abs(value)
                + std::abs(parameter * component));
    };
    const TVector relative = pointPosition - startPosition;
    return matches(relative.x, edge.x, startPosition.x, startPosition.x + edge.x)
        && matches(relative.y, edge.y, startPosition.y, startPosition.y + edge.y)
        && matches(relative.z, edge.z, startPosition.z, startPosition.z + edge.z);
}

struct CoordinateEdgeSupport
{
    std::int32_t firstIndex{-1};
    std::int32_t secondIndex{-1};
    Vertex first{};
    Vertex second{};
    double deviation{};
};

std::optional<CoordinateEdgeSupport> sourceRoundedEdgeSupport(
    const Sector& sector,
    const Surface& surface,
    const std::int32_t edgeA,
    const std::int32_t edgeB)
{
    if ( edgeA < 0 || edgeB < 0
        || edgeA == edgeB
        || !surfaceHasValidVertexTopology(sector, surface)
        || surface.vertices.size() < 3 )
    {
        return std::nullopt;
    }
    const auto boundaryLimit = coordinateBoundaryDeviationLimit(sector, surface);
    if ( !boundaryLimit || !(*boundaryLimit > 0.0)
        || !std::isfinite(*boundaryLimit) )
    {
        return std::nullopt;
    }

    const std::size_t count = surface.vertices.size();
    std::vector<TVector> originalPoints;
    originalPoints.reserve(count);
    for ( const std::int32_t vertexIndex : surface.vertices )
    {
        if ( vertexIndex < 0
            || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size() )
        {
            return std::nullopt;
        }
        originalPoints.push_back(precision::position(
            sector.vertices[static_cast<std::size_t>(vertexIndex)]));
    }

    // Mirror the one-pass immutable reduction used by
    // simplifySourceRoundedBoundary. A later chord never spends residual
    // admitted by an earlier removal.
    std::vector<bool> removed(count, false);
    std::size_t remaining = count;
    for ( std::size_t index = 0; index < count && remaining > 3; ++index )
    {
        const TVector& previous = originalPoints[(index + count - 1) % count];
        const TVector& current = originalPoints[index];
        const TVector& next = originalPoints[(index + 1) % count];
        const Vertex previousVertex{ previous.x, previous.y, previous.z, 0 };
        const Vertex currentVertex{ current.x, current.y, current.z, 0 };
        const Vertex nextVertex{ next.x, next.y, next.z, 0 };
        double parameter{};
        const double parameterError = coordinateEdgeParameterError(
            previousVertex, nextVertex, *boundaryLimit);
        if ( !std::isfinite(parameterError)
            || !coordinateLineParameter(
                currentVertex, previousVertex, nextVertex,
                parameter, *boundaryLimit)
            || !(parameter > parameterError)
            || !(parameter < 1.0 - parameterError) )
        {
            continue;
        }
        removed[index] = true;
        --remaining;
    }

    std::vector<std::size_t> positionsA;
    std::vector<std::size_t> positionsB;
    for ( std::size_t index = 0; index < count; ++index )
    {
        if ( surface.vertices[index] == edgeA )
        {
            positionsA.push_back(index);
        }
        if ( surface.vertices[index] == edgeB )
        {
            positionsB.push_back(index);
        }
    }
    if ( positionsA.empty() || positionsB.empty() )
    {
        return std::nullopt;
    }

    const auto positionDistance = [count](
        const std::size_t from, const std::size_t to) noexcept
    {
        return (to + count - from) % count;
    };
    const auto walkCandidate = [&](
        const std::size_t positionA,
        const std::size_t positionB,
        const int direction) -> std::optional<CoordinateEdgeSupport>
    {
        if ( direction != 1 && direction != -1 )
        {
            return std::nullopt;
        }
        const std::size_t adjacentDistance = direction == 1
            ? positionDistance(positionA, positionB)
            : positionDistance(positionB, positionA);
        if ( adjacentDistance != 1 )
        {
            return std::nullopt;
        }

        const auto advance = [count](
            const std::size_t position,
            const std::size_t steps,
            const int movementDirection) noexcept
        {
            if ( movementDirection == 1 )
            {
                return (position + steps) % count;
            }
            return (position + count - (steps % count)) % count;
        };
        std::size_t start = positionA;
        std::size_t end = positionB;
        bool hasRemoved = removed[start] || removed[end];

        if ( removed[start] )
        {
            for ( std::size_t steps = 1; steps < count; ++steps )
            {
                const std::size_t previous = advance(start, 1, -direction);
                if ( removed[previous] )
                {
                    start = previous;
                    hasRemoved = true;
                    continue;
                }
                start = previous;
                break;
            }
        }
        if ( removed[end] )
        {
            for ( std::size_t steps = 1; steps < count; ++steps )
            {
                const std::size_t next = advance(end, 1, direction);
                if ( removed[next] )
                {
                    end = next;
                    hasRemoved = true;
                    continue;
                }
                end = next;
                break;
            }
        }
        if ( !hasRemoved || start == end )
        {
            return std::nullopt;
        }

        const TVector firstPoint = originalPoints[start];
        const TVector lastPoint = originalPoints[end];
        const std::int32_t firstIndex = surface.vertices[start];
        const std::int32_t lastIndex = surface.vertices[end];
        const Vertex& firstVertex = sector.vertices[
            static_cast<std::size_t>(firstIndex)];
        const Vertex& lastVertex = sector.vertices[
            static_cast<std::size_t>(lastIndex)];
        const double parameterError = coordinateEdgeParameterError(
            firstVertex, lastVertex, *boundaryLimit);
        if ( !std::isfinite(parameterError) )
        {
            return std::nullopt;
        }

        double previousParameter = 0.0;
        double maximumDeviation = 0.0;
        std::size_t pointCount = 0;
        for ( std::size_t steps = 0; steps < count; ++steps )
        {
            const std::size_t position = advance(start, steps, direction);
            const TVector& point = originalPoints[position];
            double parameter = 0.0;
            if ( position != start && position != end )
            {
                const Vertex pointVertex{ point.x, point.y, point.z, 0 };
                if ( !coordinateLineParameter(
                        pointVertex, firstVertex, lastVertex,
                        parameter, *boundaryLimit) )
                {
                    return std::nullopt;
                }
            }
            else if ( position == end )
            {
                parameter = 1.0;
            }
            const TVector chord = lastPoint - firstPoint;
            const TVector projected{
                std::fma(parameter, chord.x, firstPoint.x),
                std::fma(parameter, chord.y, firstPoint.y),
                std::fma(parameter, chord.z, firstPoint.z) };
            const TVector residual = point - projected;
            const double scale = std::max({ 1.0,
                std::abs(point.x), std::abs(point.y), std::abs(point.z),
                std::abs(projected.x), std::abs(projected.y),
                std::abs(projected.z) });
            const double deviation = std::hypot(
                residual.x, residual.y, residual.z);
            if ( !std::isfinite(parameter)
                || !precision::finite(residual)
                || !std::isfinite(deviation)
                || (pointCount > 0
                    && !(parameter > previousParameter + parameterError))
                || deviation > *boundaryLimit
                    + precision::kRoundoff * scale )
            {
                return std::nullopt;
            }
            previousParameter = parameter;
            maximumDeviation = std::max(maximumDeviation, deviation);
            ++pointCount;
            if ( position == end )
            {
                break;
            }
        }
        if ( pointCount < 3 || start == end )
        {
            return std::nullopt;
        }
        return CoordinateEdgeSupport{
            firstIndex, lastIndex,
            sector.vertices[static_cast<std::size_t>(firstIndex)],
            sector.vertices[static_cast<std::size_t>(lastIndex)],
            maximumDeviation };
    };

    std::optional<CoordinateEdgeSupport> result;
    for ( const std::size_t positionA : positionsA )
    {
        for ( const std::size_t positionB : positionsB )
        {
            const auto candidate = positionDistance(positionA, positionB) == 1
                ? walkCandidate(positionA, positionB, 1)
                : positionDistance(positionB, positionA) == 1
                    ? walkCandidate(positionA, positionB, -1)
                    : std::nullopt;
            if ( !candidate )
            {
                continue;
            }
            if ( !result )
            {
                result = *candidate;
                continue;
            }
            const bool sameEndpoints =
                ( result->firstIndex == candidate->firstIndex
                    && result->secondIndex == candidate->secondIndex )
                || ( result->firstIndex == candidate->secondIndex
                    && result->secondIndex == candidate->firstIndex );
            if ( !sameEndpoints )
            {
                return std::nullopt;
            }
            result->deviation = std::max(result->deviation, candidate->deviation);
        }
    }
    return result;
}

std::optional<CoordinateEdgeSupport> sourceRoundedEdgeSupportForCoordinates(
    const Sector& sector,
    const Surface& surface,
    const Vertex& first,
    const Vertex& second)
{
    std::optional<CoordinateEdgeSupport> result;
    for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
    {
        const std::size_t next = (corner + 1) % surface.vertices.size();
        const std::int32_t edgeA = surface.vertices[corner];
        const std::int32_t edgeB = surface.vertices[next];
        if ( edgeA < 0 || edgeB < 0
            || static_cast<std::size_t>(edgeA) >= sector.vertices.size()
            || static_cast<std::size_t>(edgeB) >= sector.vertices.size() )
        {
            continue;
        }
        const Vertex& pointA = sector.vertices[static_cast<std::size_t>(edgeA)];
        const Vertex& pointB = sector.vertices[static_cast<std::size_t>(edgeB)];
        const auto exactPoint = [](const Vertex& lhs, const Vertex& rhs) noexcept
        {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
        };
        if ( (exactPoint(pointA, first) && exactPoint(pointB, second))
            || (exactPoint(pointA, second) && exactPoint(pointB, first)) )
        {
            const auto candidate = sourceRoundedEdgeSupport(
                sector, surface, edgeA, edgeB);
            if ( !candidate )
            {
                continue;
            }
            if ( !result )
            {
                result = *candidate;
                continue;
            }
            const bool sameSupport =
                (result->firstIndex == candidate->firstIndex
                    && result->secondIndex == candidate->secondIndex)
                || (result->firstIndex == candidate->secondIndex
                    && result->secondIndex == candidate->firstIndex);
            if ( !sameSupport )
            {
                return std::nullopt;
            }
            result->deviation = std::max(result->deviation, candidate->deviation);
        }
    }
    return result;
}


std::optional<CoordinateEdgeInterval> coordinateEdgeInterval(
    const Sector& sector,
    const Surface& surface,
    const std::size_t corner,
    const Vertex& lineStart,
    const Vertex& lineEnd,
    const double deviationLimit = 0.0) noexcept
{
    if ( surface.vertices.size() < 2 || corner >= surface.vertices.size() )
    {
        return std::nullopt;
    }
    const std::size_t next = (corner + 1) % surface.vertices.size();
    const std::int32_t firstIndex = surface.vertices[corner];
    const std::int32_t secondIndex = surface.vertices[next];
    if ( firstIndex < 0 || secondIndex < 0
        || static_cast<std::size_t>(firstIndex) >= sector.vertices.size()
        || static_cast<std::size_t>(secondIndex) >= sector.vertices.size() )
    {
        return std::nullopt;
    }

    const Vertex& first = sector.vertices[static_cast<std::size_t>(firstIndex)];
    const Vertex& second = sector.vertices[static_cast<std::size_t>(secondIndex)];
    CoordinateEdgeInterval result{};
    if ( !coordinateLineParameter(first, lineStart, lineEnd, result.first, deviationLimit)
        || !coordinateLineParameter(second, lineStart, lineEnd, result.second, deviationLimit) )
    {
        return std::nullopt;
    }
    const double low = std::min(result.first, result.second);
    const double high = std::max(result.first, result.second);
    result.minimum = std::max(0.0, low);
    result.maximum = std::min(1.0, high);
    if ( !std::isfinite(result.minimum) || !std::isfinite(result.maximum)
        || !(result.maximum > result.minimum) )
    {
        return std::nullopt;
    }
    return result;
}

bool coordinateEdgeChainCoversSegment(
    const Sector& sector,
    const Surface& surface,
    const Vertex& lineStart,
    const Vertex& lineEnd,
    const double deviationLimit = 0.0) noexcept
{
    if ( !surfaceHasValidVertexTopology(sector, surface)
        || surface.vertices.size() < 2 )
    {
        return false;
    }

    const auto hasInterval = [&](const std::size_t corner) noexcept
    {
        return coordinateEdgeInterval(
            sector, surface, corner, lineStart, lineEnd, deviationLimit).has_value();
    };
    const double parameterError = coordinateEdgeParameterError(
        lineStart, lineEnd, deviationLimit);
    const double endpointParameterError = coordinateEdgeParameterError(
        lineStart, lineEnd, 0.0);
    if ( !std::isfinite(parameterError) || !std::isfinite(endpointParameterError) )
    {
        return false;
    }
    const auto runCovers = [&](const std::size_t start) noexcept
    {
        const std::size_t count = surface.vertices.size();
        std::size_t corner = start;
        std::size_t matched = 0;
        int direction = 0;
        double rawMinimum = 1.0;
        double rawMaximum = 0.0;
        do
        {
            const auto interval = coordinateEdgeInterval(
                sector, surface, corner, lineStart, lineEnd, deviationLimit);
            if ( !interval )
            {
                break;
            }
            const int currentDirection = interval->second > interval->first ? 1 : -1;
            if ( direction == 0 )
            {
                direction = currentDirection;
            }
            else if ( direction != currentDirection )
            {
                return false;
            }
            if ( matched > 0 )
            {
                const auto previous = coordinateEdgeInterval(
                    sector, surface, (corner + count - 1) % count,
                    lineStart, lineEnd, deviationLimit);
                if ( !previous )
                {
                    return false;
                }
                const double gap = direction > 0
                    ? interval->first - previous->second
                    : previous->second - interval->first;
                if ( !std::isfinite(gap) || gap > parameterError )
                {
                    return false;
                }
            }
            rawMinimum = std::min(rawMinimum, std::min(interval->first, interval->second));
            rawMaximum = std::max(rawMaximum, std::max(interval->first, interval->second));
            ++matched;
            corner = (corner + 1) % count;
        }
        while ( corner != start && hasInterval(corner) );

        if ( matched == 0 )
        {
            return false;
        }
        const std::size_t last = (corner + count - 1) % count;
        const auto firstInterval = coordinateEdgeInterval(
            sector, surface, start, lineStart, lineEnd, deviationLimit);
        const auto lastInterval = coordinateEdgeInterval(
            sector, surface, last, lineStart, lineEnd, deviationLimit);
        if ( !firstInterval || !lastInterval )
        {
            return false;
        }
        // Parameters are normalized to the requested edge. The source limit
        // contributes only the projected uncertainty needed to certify a
        // contiguous authored chain; it is never a vertex weld distance.
        const double endpointBound = endpointParameterError
            * std::max({ 1.0, std::abs(rawMinimum), std::abs(rawMaximum) });
        const double firstParameter = firstInterval->first;
        const double lastParameter = lastInterval->second;
        const bool forward = firstParameter <= endpointBound
            && lastParameter >= 1.0 - endpointBound;
        const bool reverse = firstParameter >= 1.0 - endpointBound
            && lastParameter <= endpointBound;
        if ( !(rawMinimum <= endpointBound) || !(rawMaximum >= 1.0 - endpointBound)
            || (!forward && !reverse) )
        {
            return false;
        }

        const std::int32_t firstIndex = surface.vertices[start];
        const std::int32_t lastIndex = surface.vertices[(last + 1) % count];
        if ( firstIndex < 0 || lastIndex < 0
            || static_cast<std::size_t>(firstIndex) >= sector.vertices.size()
            || static_cast<std::size_t>(lastIndex) >= sector.vertices.size() )
        {
            return false;
        }
        double firstExactParameter{};
        double lastExactParameter{};
        const bool exactEndpoints = coordinateLineParameter(
                sector.vertices[static_cast<std::size_t>(firstIndex)],
                lineStart, lineEnd, firstExactParameter, 0.0)
            && coordinateLineParameter(
                sector.vertices[static_cast<std::size_t>(lastIndex)],
                lineStart, lineEnd, lastExactParameter, 0.0)
            && ((firstExactParameter <= endpointBound
                    && lastExactParameter >= 1.0 - endpointBound)
                || (firstExactParameter >= 1.0 - endpointBound
                    && lastExactParameter <= endpointBound));
        return exactEndpoints;
    };

    const std::size_t count = surface.vertices.size();
    bool allMatched = true;
    bool coveringRunFound = false;
    for ( std::size_t corner = 0; corner < count; ++corner )
    {
        const bool matched = hasInterval(corner);
        allMatched = allMatched && matched;
        const std::size_t previous = corner == 0 ? count - 1 : corner - 1;
        if ( !matched || hasInterval(previous) )
        {
            continue;
        }
        if ( runCovers(corner) )
        {
            if ( coveringRunFound )
            {
                // Two separate collinear runs covering the requested edge
                // are ambiguous; do not choose one based on face ordering.
                return false;
            }
            coveringRunFound = true;
        }
    }

    if ( allMatched && runCovers(0) )
    {
        if ( coveringRunFound )
        {
            return false;
        }
        coveringRunFound = true;
    }
    return coveringRunFound;
}

bool surfaceContainsCoordinateEdge(
    const Sector& sector,
    const Surface& surface,
    const Vertex& edgeA,
    const Vertex& edgeB,
    const double deviationLimit = 0.0) noexcept
{
    return coordinateEdgeChainCoversSegment(
        sector, surface, edgeA, edgeB, deviationLimit);
}

struct CoordinateEdgeChainCertificate
{
    std::vector<std::pair<std::int32_t, std::int32_t>> edges;
    double maximumDeviation{};
};

bool coordinateEdgeChainHasExactEndpoints(
    const Sector& sector,
    const Surface& surface,
    const Vertex& lineStart,
    const Vertex& lineEnd) noexcept
{
    bool startFound = false;
    bool endFound = false;
    for ( const std::int32_t vertexIndex : surface.vertices )
    {
        if ( vertexIndex < 0
            || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size() )
        {
            return false;
        }
        const Vertex& point = sector.vertices[static_cast<std::size_t>(vertexIndex)];
        const bool matchesStart = point.x == lineStart.x
            && point.y == lineStart.y && point.z == lineStart.z;
        const bool matchesEnd = point.x == lineEnd.x
            && point.y == lineEnd.y && point.z == lineEnd.z;
        startFound = startFound || matchesStart;
        endFound = endFound || matchesEnd;
    }
    return startFound && endFound;
}

std::optional<CoordinateEdgeChainCertificate> coordinateEdgeChainCertificate(
    const Sector& sector,
    const Surface& surface,
    const Vertex& lineStart,
    const Vertex& lineEnd,
    const double admissibilityLimit)
{
    if ( !std::isfinite(admissibilityLimit) || admissibilityLimit < 0.0
        || !coordinateEdgeChainCoversSegment(
            sector, surface, lineStart, lineEnd, admissibilityLimit) )
    {
        return std::nullopt;
    }

    CoordinateEdgeChainCertificate result{};
    result.edges.reserve(surface.vertices.size());
    for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
    {
        const std::size_t next = (corner + 1) % surface.vertices.size();
        const std::int32_t firstIndex = surface.vertices[corner];
        const std::int32_t secondIndex = surface.vertices[next];
        const auto interval = coordinateEdgeInterval(
            sector, surface, corner, lineStart, lineEnd, admissibilityLimit);
        if ( !interval )
        {
            continue;
        }
        result.edges.emplace_back(firstIndex, secondIndex);
        for ( const std::int32_t vertexIndex : { firstIndex, secondIndex } )
        {
            const Vertex& point = sector.vertices[static_cast<std::size_t>(vertexIndex)];
            double parameter{};
            if ( !coordinateLineParameter(
                    point, lineStart, lineEnd, parameter, admissibilityLimit) )
            {
                return std::nullopt;
            }
            const TVector pointPosition = precision::position(point);
            const TVector startPosition = precision::position(lineStart);
            const TVector edge = precision::position(lineEnd) - startPosition;
            const TVector projected{
                std::fma(parameter, edge.x, startPosition.x),
                std::fma(parameter, edge.y, startPosition.y),
                std::fma(parameter, edge.z, startPosition.z) };
            const TVector residual = pointPosition - projected;
            const double deviation = std::hypot(residual.x, residual.y, residual.z);
            if ( !precision::finite(residual) || !std::isfinite(deviation) )
            {
                return std::nullopt;
            }
            result.maximumDeviation = std::max(result.maximumDeviation, deviation);
        }
    }
    if ( result.edges.empty() || !std::isfinite(result.maximumDeviation) )
    {
        return std::nullopt;
    }
    return result;
}

struct RoundedBoundary
{
    std::vector<TVector> points;
    bool removedRedundantCorner{};
};

RoundedBoundary simplifySourceRoundedBoundary(
    const Sector& sector,
    const Surface& surface,
    const double deviationLimit)
{
    RoundedBoundary result{};
    if ( deviationLimit < 0.0 || !std::isfinite(deviationLimit)
        || surface.vertices.size() < 3 || !surfaceHasValidVertexTopology(sector, surface) )
    {
        return result;
    }

    result.points.reserve(surface.vertices.size());
    for ( const auto vertexIndex : surface.vertices )
    {
        result.points.push_back(precision::position(
            sector.vertices[static_cast<std::size_t>(vertexIndex)]));
    }

    // Remove only an interior point that lies on its original adjacent chord
    // within the immutable source admission budget. Evaluate every point
    // against the untouched input contour in one pass; a chord formed by an
    // earlier removal cannot spend the same residual a second time. The
    // forward parameter check keeps a short reversal or a real corner from
    // being hidden by the boundary allowance. This is a comparison view;
    // authored topology and vertex coordinates are never changed.
    const std::vector<TVector> originalPoints = result.points;
    std::vector<bool> remove(originalPoints.size(), false);
    std::size_t remaining = originalPoints.size();
    for ( std::size_t index = 0; index < originalPoints.size(); ++index )
    {
        if ( remaining <= 3 )
        {
            break;
        }
        const std::size_t count = originalPoints.size();
        const TVector& previous = originalPoints[(index + count - 1) % count];
        const TVector& current = originalPoints[index];
        const TVector& next = originalPoints[(index + 1) % count];
        const Vertex previousVertex{ previous.x, previous.y, previous.z, 0 };
        const Vertex nextVertex{ next.x, next.y, next.z, 0 };
        const Vertex currentVertex{ current.x, current.y, current.z, 0 };
        double parameter{};
        const double parameterError = coordinateEdgeParameterError(
            previousVertex, nextVertex, deviationLimit);
        if ( !std::isfinite(parameterError)
            || !coordinateLineParameter(
                currentVertex, previousVertex, nextVertex, parameter, deviationLimit)
            || !(parameter > parameterError)
            || !(parameter < 1.0 - parameterError) )
        {
            continue;
        }
        remove[index] = true;
        --remaining;
        result.removedRedundantCorner = true;
    }

    if ( result.removedRedundantCorner )
    {
        std::vector<TVector> compact;
        compact.reserve(remaining);
        for ( std::size_t index = 0; index < originalPoints.size(); ++index )
        {
            if ( !remove[index] )
            {
                compact.push_back(originalPoints[index]);
            }
        }
        result.points = std::move(compact);

        // Validate every removed point against the final chord formed by its
        // surviving neighbors. This keeps a run of rounded points from
        // accumulating separate residual allowances as points are removed.
        for ( std::size_t index = 0; index < originalPoints.size(); ++index )
        {
            if ( !remove[index] )
            {
                continue;
            }
            std::size_t previous = (index + originalPoints.size() - 1) % originalPoints.size();
            while ( remove[previous] )
            {
                previous = (previous + originalPoints.size() - 1) % originalPoints.size();
            }
            std::size_t next = (index + 1) % originalPoints.size();
            while ( remove[next] )
            {
                next = (next + 1) % originalPoints.size();
            }
            const Vertex lineStart{
                originalPoints[previous].x, originalPoints[previous].y,
                originalPoints[previous].z, 0 };
            const Vertex lineEnd{
                originalPoints[next].x, originalPoints[next].y,
                originalPoints[next].z, 0 };
            const Vertex removed{
                originalPoints[index].x, originalPoints[index].y,
                originalPoints[index].z, 0 };
            double parameter{};
            if ( !coordinateLineParameter(
                    removed, lineStart, lineEnd, parameter, deviationLimit)
                || !(parameter > 0.0 && parameter < 1.0) )
            {
                result.points = originalPoints;
                result.removedRedundantCorner = false;
                return result;
            }
        }

        // A contiguous run may contain more than one removed vertex. Check
        // the complete run against one immutable chord, in parameter order.
        for ( std::size_t start = 0; start < originalPoints.size(); ++start )
        {
            const std::size_t previous =
                (start + originalPoints.size() - 1) % originalPoints.size();
            if ( !remove[start] || remove[previous] )
            {
                continue;
            }
            std::size_t end = start;
            while ( remove[(end + 1) % originalPoints.size()] )
            {
                end = (end + 1) % originalPoints.size();
            }
            const std::size_t following = (end + 1) % originalPoints.size();
            const Vertex lineStart{
                originalPoints[previous].x, originalPoints[previous].y,
                originalPoints[previous].z, 0 };
            const Vertex lineEnd{
                originalPoints[following].x, originalPoints[following].y,
                originalPoints[following].z, 0 };
            double priorParameter = 0.0;
            std::size_t current = start;
            while ( true )
            {
                const Vertex removed{
                    originalPoints[current].x, originalPoints[current].y,
                    originalPoints[current].z, 0 };
                double parameter{};
                if ( !coordinateLineParameter(
                        removed, lineStart, lineEnd, parameter, deviationLimit)
                    || !(parameter > priorParameter) )
                {
                    result.points = originalPoints;
                    result.removedRedundantCorner = false;
                    return result;
                }
                priorParameter = parameter;
                if ( current == end )
                {
                    break;
                }
                current = (current + 1) % originalPoints.size();
            }
        }
    }
    return result;
}

bool nativePointExact(const TVector& first, const TVector& second) noexcept
{
    return first.x == second.x && first.y == second.y && first.z == second.z;
}

struct NativePairedCorner
{
    TVector first{};
    TVector second{};
    double residual{};
    std::int32_t firstId{-1};
    std::int32_t secondId{-1};
};

struct NativePairCertificate
{
    double delta{};
    std::vector<NativePairedCorner> pairedCorners;
};

std::optional<double> nativeIncidentContourPairResidual(
    const std::vector<CleaveTopologyFacePtr>& firstContours,
    const std::vector<CleaveTopologyFacePtr>& secondContours,
    const NativePairedCorner& mismatch);

std::optional<NativePairCertificate> sourceNativePairMatch(
    const CleaveFaceProfile& firstProfile,
    const CleaveFaceProfile& secondProfile)
{
    if ( firstProfile.geometry.surfaces.empty()
        || secondProfile.geometry.surfaces.empty() )
    {
        return std::nullopt;
    }
    const Surface& firstSurface = firstProfile.geometry.surfaces.front();
    const Surface& secondSurface = secondProfile.geometry.surfaces.front();
    const RoundedBoundary first = simplifySourceRoundedBoundary(
        firstProfile.geometry, firstSurface, firstProfile.boundaryDeviation);
    const RoundedBoundary second = simplifySourceRoundedBoundary(
        secondProfile.geometry, secondSurface, secondProfile.boundaryDeviation);
    if ( first.points.size() < 3 || first.points.size() != second.points.size() )
    {
        return std::nullopt;
    }
    const double firstEligibility = coordinateBoundaryDeviationLimit(
        firstProfile.geometry, firstSurface).value_or(0.0);
    const double secondEligibility = coordinateBoundaryDeviationLimit(
        secondProfile.geometry, secondSurface).value_or(0.0);
    const double mismatchLimit = std::min(firstEligibility, secondEligibility);
    if ( !std::isfinite(mismatchLimit) || !(mismatchLimit > 0.0) )
    {
        return std::nullopt;
    }

    const auto originalCornerId = [](
        const CleaveFaceProfile& profile,
        const TVector& point) -> std::optional<std::int32_t>
    {
        if ( profile.geometry.vertices.size() != profile.originalCornerIds.size() )
        {
            return std::nullopt;
        }
        std::optional<std::int32_t> result;
        for ( std::size_t index = 0; index < profile.geometry.vertices.size(); ++index )
        {
            if ( !nativePointExact(
                    precision::position(profile.geometry.vertices[index]), point) )
            {
                continue;
            }
            const std::int32_t originalId = profile.originalCornerIds[index];
            if ( originalId < 0 || result )
            {
                return std::nullopt;
            }
            result = originalId;
        }
        return result;
    };

    std::optional<NativePairCertificate> selectedCertificate;
    for ( std::size_t start = 0; start < second.points.size(); ++start )
    {
        std::size_t exactAnchors = 0;
        std::size_t mismatchCount = 0;
        std::size_t mismatchCorner = 0;
        double mismatchDelta = 0.0;
        bool valid = true;
        std::vector<NativePairedCorner> mismatches;
        mismatches.reserve(first.points.size());
        for ( std::size_t corner = 0; corner < first.points.size(); ++corner )
        {
            const TVector& firstPoint = first.points[corner];
            const TVector& secondPoint = second.points[
                (start + second.points.size() - corner) % second.points.size()];
            if ( firstPoint.x == secondPoint.x
                && firstPoint.y == secondPoint.y
                && firstPoint.z == secondPoint.z )
            {
                ++exactAnchors;
                continue;
            }
            ++mismatchCount;
            mismatchCorner = corner;
            const TVector delta = firstPoint - secondPoint;
            mismatchDelta = std::hypot(delta.x, delta.y, delta.z);
            if ( !precision::finite(delta) || !std::isfinite(mismatchDelta) )
            {
                valid = false;
                break;
            }
            mismatches.push_back({firstPoint, secondPoint, mismatchDelta});
        }
        if ( !valid || exactAnchors < 2 || mismatches.empty() )
        {
            continue;
        }

        // Keep the existing one-mismatch certificate and its admission and
        // line checks byte-for-byte in behavior. Multi-corner evidence below
        // is an explicitly narrower fallback.
        if ( mismatchCount == 1 )
        {
            if ( !(mismatchDelta <= mismatchLimit
                    + precision::kRoundoff * std::max(1.0, mismatchLimit)) )
            {
                continue;
            }
            const std::size_t count = first.points.size();
            const std::size_t previous = (mismatchCorner + count - 1) % count;
            const std::size_t next = (mismatchCorner + 1) % count;
            const TVector& mismatchFirst = first.points[mismatchCorner];
            const TVector& mismatchSecond = second.points[
                (start + count - mismatchCorner) % count];
            bool lineCertified = false;
            for ( const std::size_t anchorCorner : { previous, next } )
            {
                const TVector& anchorFirst = first.points[anchorCorner];
                const std::size_t secondAnchorCorner =
                    (start + count - anchorCorner) % count;
                const TVector& anchorSecond = second.points[secondAnchorCorner];
                if ( anchorFirst.x != anchorSecond.x
                    || anchorFirst.y != anchorSecond.y
                    || anchorFirst.z != anchorSecond.z )
                {
                    continue;
                }
                const Vertex lineStart{ anchorFirst.x, anchorFirst.y, anchorFirst.z, 0 };
                const Vertex lineEnd{ mismatchFirst.x, mismatchFirst.y, mismatchFirst.z, 0 };
                const Vertex secondPointVertex{
                    mismatchSecond.x, mismatchSecond.y, mismatchSecond.z, 0 };
                double firstParameter{};
                double secondParameter{};
                if ( coordinateLineParameter(
                        lineEnd, lineStart, lineEnd, firstParameter, 0.0)
                    && coordinateLineParameter(
                        secondPointVertex, lineStart, lineEnd, secondParameter, 0.0)
                    && firstParameter > 0.0 && secondParameter > 0.0 )
                {
                    lineCertified = true;
                    break;
                }
            }
            if ( !lineCertified )
            {
                continue;
            }
            NativePairCertificate certificate{};
            certificate.delta = mismatchDelta;
            certificate.pairedCorners = std::move(mismatches);
            if ( selectedCertificate )
            {
                return std::nullopt;
            }
            selectedCertificate = std::move(certificate);
            continue;
        }

        if ( mismatchCount < 2 || !firstProfile.incidentTopologies
            || !secondProfile.incidentTopologies )
        {
            continue;
        }
        const double mismatchGate = mismatchLimit
            + precision::kRoundoff * std::max(1.0, mismatchLimit);
        bool multiCornerValid = std::isfinite(mismatchGate);
        double maximumResidual = 0.0;
        const std::size_t count = first.points.size();
        for ( NativePairedCorner& paired : mismatches )
        {
            if ( !std::isfinite(paired.residual) || paired.residual > mismatchGate )
            {
                multiCornerValid = false;
                break;
            }
            std::optional<std::size_t> pairedCorner;
            for ( std::size_t corner = 0; corner < count; ++corner )
            {
                if ( !nativePointExact(first.points[corner], paired.first) )
                {
                    continue;
                }
                if ( pairedCorner )
                {
                    multiCornerValid = false;
                    break;
                }
                const std::size_t secondCorner =
                    (start + count - corner) % count;
                if ( !nativePointExact(second.points[secondCorner], paired.second) )
                {
                    multiCornerValid = false;
                    break;
                }
                pairedCorner = corner;
            }
            if ( !multiCornerValid || !pairedCorner )
            {
                multiCornerValid = false;
                break;
            }

            const auto firstId = originalCornerId(firstProfile, paired.first);
            const auto secondId = originalCornerId(secondProfile, paired.second);
            if ( !firstId || !secondId )
            {
                multiCornerValid = false;
                break;
            }
            paired.firstId = *firstId;
            paired.secondId = *secondId;

            const std::size_t previous = (*pairedCorner + count - 1) % count;
            const std::size_t next = (*pairedCorner + 1) % count;
            bool lineCertified = false;
            for ( const std::size_t anchorCorner : { previous, next } )
            {
                const std::size_t secondAnchorCorner =
                    (start + count - anchorCorner) % count;
                if ( !nativePointExact(
                        first.points[anchorCorner], second.points[secondAnchorCorner]) )
                {
                    continue;
                }
                const Vertex lineStart{
                    first.points[anchorCorner].x,
                    first.points[anchorCorner].y,
                    first.points[anchorCorner].z,
                    0 };
                const Vertex lineEnd{
                    paired.first.x, paired.first.y, paired.first.z, 0 };
                const Vertex targetPoint{
                    paired.second.x, paired.second.y, paired.second.z, 0 };
                double sourceParameter{};
                double targetParameter{};
                    if ( coordinateLineParameter(
                        lineEnd, lineStart, lineEnd, sourceParameter, 0.0)
                    && coordinateLineParameter(
                        targetPoint, lineStart, lineEnd, targetParameter, 0.0)
                    && sourceParameter > 0.0 && targetParameter > 0.0 )
                {
                    lineCertified = true;
                    break;
                }
            }
            if ( !lineCertified )
            {
                multiCornerValid = false;
                break;
            }

            const auto measuredResidual = nativeIncidentContourPairResidual(
                *firstProfile.incidentTopologies,
                *secondProfile.incidentTopologies,
                paired);
            if ( !measuredResidual )
            {
                multiCornerValid = false;
                break;
            }
            paired.residual = std::max(paired.residual, *measuredResidual);
            if ( !std::isfinite(paired.residual) )
            {
                multiCornerValid = false;
                break;
            }
            maximumResidual = std::max(maximumResidual, paired.residual);
        }
        if ( !multiCornerValid )
        {
            continue;
        }
        NativePairCertificate certificate{};
        certificate.delta = maximumResidual;
        certificate.pairedCorners = std::move(mismatches);
        if ( selectedCertificate )
        {
            return std::nullopt;
        }
        selectedCertificate = std::move(certificate);
    }
    return selectedCertificate;
}

struct NativeIncidentContourGeometry
{
    Sector sector;
    Surface surface;
};

std::optional<NativeIncidentContourGeometry> makeNativeIncidentContourGeometry(
    const CleaveTopologyFace& contour)
{
    if ( contour.points.size() < 3
        || contour.points.size() != contour.indices.size()
        || contour.points.size() > static_cast<std::size_t>(
            std::numeric_limits<std::int32_t>::max()) )
    {
        return std::nullopt;
    }

    NativeIncidentContourGeometry result{};
    result.sector.vertices.reserve(contour.points.size());
    result.surface.vertices.reserve(contour.points.size());
    for ( std::size_t index = 0; index < contour.points.size(); ++index )
    {
        const TVector& point = contour.points[index];
        if ( contour.indices[index] < 0 || !precision::finite(point) )
        {
            return std::nullopt;
        }
        // The captured source IDs belong to their original Sector. This
        // private topology view deliberately remaps every contour locally.
        result.sector.vertices.push_back(Vertex{point.x, point.y, point.z, 0});
        result.surface.vertices.push_back(static_cast<std::int32_t>(index));
    }
    return result;
}

std::optional<double> nativeIncidentContourPairResidual(
    const std::vector<CleaveTopologyFacePtr>& firstContours,
    const std::vector<CleaveTopologyFacePtr>& secondContours,
    const NativePairedCorner& mismatch)
{
    if ( firstContours.empty() || secondContours.empty()
        || !precision::finite(mismatch.first)
        || !precision::finite(mismatch.second)
        || mismatch.firstId < 0 || mismatch.secondId < 0 )
    {
        return std::nullopt;
    }

    const TVector mismatchDelta = mismatch.first - mismatch.second;
    const double mismatchDistance = std::hypot(
        mismatchDelta.x, mismatchDelta.y, mismatchDelta.z);
    if ( !precision::finite(mismatchDelta) || !std::isfinite(mismatchDistance) )
    {
        return std::nullopt;
    }

    const auto uniquePoint = [](
        const std::vector<TVector>& points,
        const TVector& target) -> std::optional<std::size_t>
    {
        std::optional<std::size_t> result;
        for ( std::size_t index = 0; index < points.size(); ++index )
        {
            if ( !nativePointExact(points[index], target) )
            {
                continue;
            }
            if ( result )
            {
                return std::nullopt;
            }
            result = index;
        }
        return result;
    };

    const auto sameUnorderedEndpoints = [](
        const TVector& firstA,
        const TVector& firstB,
        const TVector& secondA,
        const TVector& secondB) noexcept
    {
        return (nativePointExact(firstA, secondA)
                && nativePointExact(firstB, secondB))
            || (nativePointExact(firstA, secondB)
                && nativePointExact(firstB, secondA));
    };

    struct AcceptedRunPair
    {
        TVector firstA{};
        TVector firstB{};
        double measuredResidual{};
    };
    std::vector<AcceptedRunPair> accepted;

    for ( const CleaveTopologyFacePtr& firstContour : firstContours )
    {
        if ( !firstContour )
        {
            continue;
        }
        const auto firstGeometry = makeNativeIncidentContourGeometry(*firstContour);
        if ( !firstGeometry )
        {
            return std::nullopt;
        }
        const auto firstMember = uniquePoint(
            firstContour->points, mismatch.first);
        if ( !firstMember
            || *firstMember >= firstContour->indices.size()
            || firstContour->indices[*firstMember] != mismatch.firstId )
        {
            continue;
        }

        const std::size_t count = firstGeometry->surface.vertices.size();
        for ( const std::size_t edgeCorner : {
                (*firstMember + count - 1) % count, *firstMember } )
        {
            const std::size_t nextCorner = (edgeCorner + 1) % count;
            const auto sourceSupport = sourceRoundedEdgeSupport(
                firstGeometry->sector,
                firstGeometry->surface,
                firstGeometry->surface.vertices[edgeCorner],
                firstGeometry->surface.vertices[nextCorner]);
            if ( !sourceSupport
                || !std::isfinite(sourceSupport->deviation)
                || sourceSupport->deviation < 0.0 )
            {
                continue;
            }

            const TVector sourceA = precision::position(sourceSupport->first);
            const TVector sourceB = precision::position(sourceSupport->second);
            if ( !uniquePoint(firstContour->points, sourceA)
                || !uniquePoint(firstContour->points, sourceB)
                || nativePointExact(sourceA, sourceB) )
            {
                continue;
            }
            const auto sourceLimit = coordinateBoundaryDeviationLimit(
                firstGeometry->sector, firstGeometry->surface);
            if ( !sourceLimit )
            {
                continue;
            }
            const auto sourceCertificate = coordinateEdgeChainCertificate(
                firstGeometry->sector,
                firstGeometry->surface,
                sourceSupport->first,
                sourceSupport->second,
                *sourceLimit);
            if ( !sourceCertificate
                || !std::isfinite(sourceCertificate->maximumDeviation)
                || sourceCertificate->maximumDeviation < 0.0
                || sourceSupport->firstIndex < 0 || sourceSupport->secondIndex < 0
                || static_cast<std::size_t>(sourceSupport->firstIndex) >= count
                || static_cast<std::size_t>(sourceSupport->secondIndex) >= count )
            {
                continue;
            }
            bool hasFirstEndpoint = false;
            bool hasSecondEndpoint = false;
            bool hasMismatchMember = false;
            for ( const auto edge : sourceCertificate->edges )
            {
                hasFirstEndpoint = hasFirstEndpoint
                    || edge.first == sourceSupport->firstIndex
                    || edge.second == sourceSupport->firstIndex;
                hasSecondEndpoint = hasSecondEndpoint
                    || edge.first == sourceSupport->secondIndex
                    || edge.second == sourceSupport->secondIndex;
                hasMismatchMember = hasMismatchMember
                    || edge.first == static_cast<std::int32_t>(*firstMember)
                    || edge.second == static_cast<std::int32_t>(*firstMember);
            }
            if ( !hasFirstEndpoint || !hasSecondEndpoint || !hasMismatchMember )
            {
                continue;
            }
            const double sourceRunResidual = sourceCertificate->maximumDeviation;
            const Vertex mismatchFirst{
                mismatch.first.x, mismatch.first.y, mismatch.first.z, 0 };
            double sourceParameter{};
            if ( !coordinateLineParameter(
                    mismatchFirst,
                    sourceSupport->first,
                    sourceSupport->second,
                    sourceParameter,
                    sourceRunResidual)
                || !(sourceParameter > 0.0 && sourceParameter < 1.0) )
            {
                continue;
            }

            for ( const CleaveTopologyFacePtr& secondContour : secondContours )
            {
                if ( !secondContour )
                {
                    continue;
                }
                const auto secondGeometry = makeNativeIncidentContourGeometry(*secondContour);
                if ( !secondGeometry )
                {
                    return std::nullopt;
                }
                const auto secondMember = uniquePoint(
                    secondContour->points, mismatch.second);
                const auto secondA = uniquePoint(secondContour->points, sourceA);
                const auto secondB = uniquePoint(secondContour->points, sourceB);
                if ( !secondMember
                    || *secondMember >= secondContour->indices.size()
                    || secondContour->indices[*secondMember] != mismatch.secondId )
                {
                    continue;
                }
                if ( !secondA || !secondB )
                {
                    continue;
                }

                const auto targetLimit = coordinateBoundaryDeviationLimit(
                    secondGeometry->sector, secondGeometry->surface);
                if ( !targetLimit )
                {
                    continue;
                }
                const auto targetCertificate = coordinateEdgeChainCertificate(
                    secondGeometry->sector,
                    secondGeometry->surface,
                    sourceSupport->first,
                    sourceSupport->second,
                    *targetLimit);
                if ( !targetCertificate
                    || !std::isfinite(targetCertificate->maximumDeviation)
                    || targetCertificate->maximumDeviation < 0.0 )
                {
                    continue;
                }

                bool hasFirstEndpoint = false;
                bool hasSecondEndpoint = false;
                bool hasMismatchMember = false;
                for ( const auto edge : targetCertificate->edges )
                {
                    hasFirstEndpoint = hasFirstEndpoint
                        || edge.first == static_cast<std::int32_t>(*secondA)
                        || edge.second == static_cast<std::int32_t>(*secondA);
                    hasSecondEndpoint = hasSecondEndpoint
                        || edge.first == static_cast<std::int32_t>(*secondB)
                        || edge.second == static_cast<std::int32_t>(*secondB);
                    hasMismatchMember = hasMismatchMember
                        || edge.first == static_cast<std::int32_t>(*secondMember)
                        || edge.second == static_cast<std::int32_t>(*secondMember);
                }
                if ( !hasFirstEndpoint || !hasSecondEndpoint || !hasMismatchMember )
                {
                    continue;
                }

                const Vertex mismatchSecond{
                    mismatch.second.x, mismatch.second.y, mismatch.second.z, 0 };
                double targetParameter{};
                if ( !coordinateLineParameter(
                        mismatchSecond,
                        sourceSupport->first,
                        sourceSupport->second,
                        targetParameter,
                        targetCertificate->maximumDeviation)
                    || !(targetParameter > 0.0 && targetParameter < 1.0) )
                {
                    continue;
                }

                const double scale = std::max({
                    1.0,
                    std::abs(sourceA.x), std::abs(sourceA.y), std::abs(sourceA.z),
                    std::abs(sourceB.x), std::abs(sourceB.y), std::abs(sourceB.z),
                    std::abs(mismatch.first.x), std::abs(mismatch.first.y),
                    std::abs(mismatch.first.z), std::abs(mismatch.second.x),
                    std::abs(mismatch.second.y), std::abs(mismatch.second.z) });
                const double arithmetic = precision::kRoundoff * scale;
                const double measuredBound = sourceRunResidual
                    + targetCertificate->maximumDeviation + arithmetic;
                if ( !std::isfinite(scale) || !std::isfinite(arithmetic)
                    || !std::isfinite(measuredBound)
                    || mismatchDistance > measuredBound )
                {
                    continue;
                }
                const double measuredResidual = std::max({
                    mismatchDistance,
                    sourceRunResidual,
                    targetCertificate->maximumDeviation });
                if ( !std::isfinite(measuredResidual) )
                {
                    continue;
                }

                bool duplicate = false;
                for ( AcceptedRunPair& existing : accepted )
                {
                    if ( sameUnorderedEndpoints(
                            existing.firstA, existing.firstB, sourceA, sourceB) )
                    {
                        existing.measuredResidual = std::max(
                            existing.measuredResidual, measuredResidual);
                        duplicate = true;
                        break;
                    }
                }
                if ( duplicate )
                {
                    continue;
                }
                if ( !accepted.empty() )
                {
                    // Two different exact A/B runs would make the incident
                    // evidence ambiguous; do not choose by contour order.
                    return std::nullopt;
                }
                accepted.push_back({sourceA, sourceB, measuredResidual});
            }
        }
    }

    if ( accepted.size() != 1 )
    {
        return std::nullopt;
    }
    return accepted.front().measuredResidual;
}

std::optional<CoordinateEdgeChainCertificate> nativePairMappedTargetEdge(
    const Vertex& sourceStart,
    const Vertex& sourceEnd,
    const Sector& targetSector,
    const Surface& targetSurface,
    const CleaveFaceProfile& sourceProfile,
    const CleaveFaceProfile& targetProfile)
{
    const auto pair = sourceNativePairMatch(sourceProfile, targetProfile);
    if ( !pair )
    {
        return std::nullopt;
    }

    const auto samePosition = [](const Vertex& point, const TVector& native) noexcept
    {
        return point.x == native.x && point.y == native.y && point.z == native.z;
    };
    const auto asVertex = [](const TVector& point) noexcept
    {
        return Vertex{point.x, point.y, point.z, 0};
    };
    Vertex mappedStart = sourceStart;
    Vertex mappedEnd = sourceEnd;
    bool mappedStartPoint = false;
    bool mappedEndPoint = false;
    for ( const NativePairedCorner& paired : pair->pairedCorners )
    {
        if ( samePosition(sourceStart, paired.first) )
        {
            if ( mappedStartPoint && !samePosition(mappedStart, paired.second) )
            {
                return std::nullopt;
            }
            mappedStart = asVertex(paired.second);
            mappedStartPoint = true;
        }
        if ( samePosition(sourceEnd, paired.first) )
        {
            if ( mappedEndPoint && !samePosition(mappedEnd, paired.second) )
            {
                return std::nullopt;
            }
            mappedEnd = asVertex(paired.second);
            mappedEndPoint = true;
        }
    }
    if ( !mappedStartPoint && !mappedEndPoint )
    {
        return std::nullopt;
    }

    // The target gets only the measured immutable discrepancy. The larger
    // feature-width admission limit remains reserved for proving the pair,
    // never for accepting an unrelated target edge.
    const double targetResidual = std::max(
        std::max(0.0, targetProfile.boundaryDeviation), pair->delta);
    if ( !std::isfinite(targetResidual) )
    {
        return std::nullopt;
    }
    return coordinateEdgeChainCertificate(
        targetSector, targetSurface, mappedStart, mappedEnd, targetResidual);
}

bool sourceRoundedBoundaryMatch(
    const Sector& firstSector,
    const Surface& first,
    const Sector& secondSector,
    const Surface& second,
    const double deviationLimit,
    const CleaveFaceProfile* const firstProfile,
    const CleaveFaceProfile* const secondProfile,
    const bool requireRedundantCorner = true,
    const NativePairCertificate* const nativePairCertificate = nullptr)
{
    if ( deviationLimit < 0.0 || !std::isfinite(deviationLimit) )
    {
        return false;
    }
    const RoundedBoundary firstBoundary = simplifySourceRoundedBoundary(
        firstSector, first, deviationLimit);
    const RoundedBoundary secondBoundary = simplifySourceRoundedBoundary(
        secondSector, second, deviationLimit);
    if ( requireRedundantCorner
        && !firstBoundary.removedRedundantCorner
        && !secondBoundary.removedRedundantCorner )
    {
        return false;
    }
    if ( firstBoundary.points.size() < 3
        || firstBoundary.points.size() != secondBoundary.points.size() )
    {
        return false;
    }

    for ( std::size_t start = 0; start < secondBoundary.points.size(); ++start )
    {
        bool matches = true;
        for ( std::size_t corner = 0; corner < firstBoundary.points.size(); ++corner )
        {
            const TVector& firstPoint = firstBoundary.points[corner];
            const TVector& secondPoint = secondBoundary.points[
                (start + secondBoundary.points.size() - corner) % secondBoundary.points.size()];
            if ( precision::samePoint(firstPoint, secondPoint) )
            {
                continue;
            }
            const auto isSourceAnchor = [](const CleaveFaceProfile* const profile,
                const TVector& point) noexcept
            {
                if ( !profile )
                {
                    return false;
                }
                return std::any_of(
                    profile->geometry.vertices.begin(), profile->geometry.vertices.end(),
                    [&point](const Vertex& sourcePoint)
                    {
                        return sourcePoint.x == point.x
                            && sourcePoint.y == point.y
                            && sourcePoint.z == point.z;
                    });
            };
            // Authored corners are immutable anchors. A generated cut point
            // may spend the measured chain residual, but an endpoint that is
            // already present in either source profile must match exactly.
            const bool isValidatedNativePair = nativePairCertificate != nullptr
                && std::any_of(
                    nativePairCertificate->pairedCorners.begin(),
                    nativePairCertificate->pairedCorners.end(),
                    [&firstPoint, &secondPoint](const NativePairedCorner& paired)
                    {
                        return nativePointExact(firstPoint, paired.first)
                            && nativePointExact(secondPoint, paired.second);
                    });
            if ( (isSourceAnchor(firstProfile, firstPoint)
                    || isSourceAnchor(secondProfile, secondPoint))
                && !isValidatedNativePair )
            {
                matches = false;
                break;
            }
            const TVector delta = firstPoint - secondPoint;
            const double scale = std::max({ 1.0,
                std::abs(firstPoint.x), std::abs(firstPoint.y), std::abs(firstPoint.z),
                std::abs(secondPoint.x), std::abs(secondPoint.y), std::abs(secondPoint.z) });
            const double error = deviationLimit + precision::kRoundoff * scale;
            if ( !precision::finite(delta) || !std::isfinite(error)
                || std::hypot(delta.x, delta.y, delta.z) > error )
            {
                matches = false;
                break;
            }
        }
        if ( matches )
        {
            return true;
        }
    }
    return false;
}

std::int32_t classifyPlaneSide(
    const Vertex& vertex,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz) noexcept
{
    const double distance = planeNormal.dot(vertex.x - px, vertex.y - py, vertex.z - pz);
    if ( !std::isfinite(distance) )
    {
        return 2;
    }
    if ( std::abs(distance) <= precision::planeError(planeNormal,
            precision::position(vertex), {px, py, pz}) )
    {
        return 0;
    }
    return distance < 0.0 ? -1 : 1;
}

bool surfaceCanSplitOnPlane(
    const Sector& sector,
    const Surface& surface,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz) noexcept
{
    if ( surface.vertices.size() < 3 || !surfaceHasValidTextureTopology(sector, surface) )
    {
        return false;
    }

    bool hasNegative = false;
    bool hasPositive = false;
    for ( const std::int32_t vertexIndex : surface.vertices )
    {
        const std::int32_t side = classifyPlaneSide(
            sector.vertices[static_cast<std::size_t>(vertexIndex)],
            planeNormal,
            px,
            py,
            pz);
        if ( side == 2 )
        {
            return false;
        }
        hasNegative = hasNegative || side < 0;
        hasPositive = hasPositive || side > 0;
    }
    return hasNegative && hasPositive;
}

bool preflightRelatedEdgeSurfaces(
    const Level& level,
    const std::int32_t sectorIndex,
    const std::int32_t edgeA,
    const std::int32_t edgeB,
    std::vector<std::int32_t>* const mirrorSectors,
    const CleaveProfileContext* const context = nullptr,
    const bool linkedTargetOnly = false)
{
    if ( sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size() )
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    if ( edgeA < 0 || edgeB < 0
        || static_cast<std::size_t>(edgeA) >= sector.vertices.size()
        || static_cast<std::size_t>(edgeB) >= sector.vertices.size() )
    {
        return false;
    }

    const Vertex& sourceA = sector.vertices[static_cast<std::size_t>(edgeA)];
    const Vertex& sourceB = sector.vertices[static_cast<std::size_t>(edgeB)];
    for ( std::size_t sourceSurfaceIndex = 0;
        sourceSurfaceIndex < sector.surfaces.size(); ++sourceSurfaceIndex )
    {
        const Surface& surface = sector.surfaces[sourceSurfaceIndex];
        if ( !surfaceContainsIndexedEdge(surface, edgeA, edgeB) )
        {
            continue;
        }
        if ( !surfaceHasValidTextureTopology(sector, surface) )
        {
            return false;
        }
        if ( !surface.adjoin )
        {
            continue;
        }
        if ( !isValidSurfaceRef(level, *surface.adjoin) )
        {
            return false;
        }

        const std::int32_t mirrorSectorIndex = surface.adjoin->sector;
        const Sector& mirrorSector = level.sectors[static_cast<std::size_t>(mirrorSectorIndex)];
        const auto sourceSupport = sourceRoundedEdgeSupport(
            sector, surface, edgeA, edgeB);
        const CleaveFaceProfile* const sourceProfile = context
            ? context->sourceFaceProfile(
                SurfaceRef{sectorIndex, static_cast<std::int32_t>(sourceSurfaceIndex)})
            : nullptr;
        bool foundMirrorEdge = false;
        for ( std::size_t mirrorSurfaceIndex = 0;
            mirrorSurfaceIndex < mirrorSector.surfaces.size(); ++mirrorSurfaceIndex )
        {
            const bool isLinkedMirrorSurface =
                static_cast<std::int32_t>(mirrorSurfaceIndex)
                    == surface.adjoin->surface;
            if ( linkedTargetOnly && !isLinkedMirrorSurface )
            {
                continue;
            }
            const Surface& mirrorSurface = mirrorSector.surfaces[mirrorSurfaceIndex];
            const double mirrorLimit = coordinateBoundaryDeviationLimit(
                mirrorSector, mirrorSurface).value_or(0.0);
            // The mirror's own bounded feature allowance certifies its
            // simplified chain once the immutable source anchors are known.
            // The source residual is measured separately and is combined only
            // after both chains have been certified; it is not used as a
            // blanket limit for the target boundary.
            const double deviationLimit = mirrorLimit > 0.0
                ? mirrorLimit : 0.0;
            // Preserve the requested native subedge whenever the linked
            // mirror contains it. A full immutable removal run is only a
            // fallback for a target that has no direct certificate.
            auto strictCertificate = coordinateEdgeChainCertificate(
                mirrorSector, mirrorSurface,
                sourceA, sourceB, deviationLimit);
            if ( strictCertificate && linkedTargetOnly
                && !coordinateEdgeChainHasExactEndpoints(
                    mirrorSector, mirrorSurface, sourceA, sourceB) )
            {
                strictCertificate.reset();
            }
            auto fullRunCertificate = !strictCertificate && sourceSupport
                ? coordinateEdgeChainCertificate(
                    mirrorSector, mirrorSurface,
                    sourceSupport->first, sourceSupport->second, deviationLimit)
                : std::optional<CoordinateEdgeChainCertificate>{};
            if ( fullRunCertificate && linkedTargetOnly
                && !coordinateEdgeChainHasExactEndpoints(
                    mirrorSector, mirrorSurface,
                    sourceSupport->first, sourceSupport->second) )
            {
                fullRunCertificate.reset();
            }
            const CleaveFaceProfile* const mirrorProfile = context
                ? context->sourceFaceProfile(
                    SurfaceRef{mirrorSectorIndex,
                        static_cast<std::int32_t>(mirrorSurfaceIndex)})
                : nullptr;
            // A source-pair certificate is meaningful only for the surface
            // explicitly linked by the adjoin record.  Strict geometric
            // matching remains available for every candidate surface; the
            // rounded fallback must never select an unrelated same-shaped
            // face in the mirror sector.
            const Vertex& pairingSourceStart = sourceSupport
                ? sourceSupport->first : sourceA;
            const Vertex& pairingSourceEnd = sourceSupport
                ? sourceSupport->second : sourceB;
            const bool completeImmutableEdgeEvidence = !linkedTargetOnly
                || coordinateEdgeChainHasExactEndpoints(
                    mirrorSector, mirrorSurface,
                    pairingSourceStart, pairingSourceEnd);
            auto pairCertificate = !strictCertificate
                && !fullRunCertificate
                && isLinkedMirrorSurface
                && completeImmutableEdgeEvidence
                && sourceProfile != nullptr && mirrorProfile != nullptr
                ? nativePairMappedTargetEdge(
                    sourceA, sourceB,
                    mirrorSector, mirrorSurface,
                    *sourceProfile, *mirrorProfile)
                : std::optional<CoordinateEdgeChainCertificate>{};
            if ( !pairCertificate && !strictCertificate && !fullRunCertificate
                && sourceSupport && isLinkedMirrorSurface
                && (!linkedTargetOnly || completeImmutableEdgeEvidence)
                && sourceProfile != nullptr && mirrorProfile != nullptr )
            {
                pairCertificate = nativePairMappedTargetEdge(
                    sourceSupport->first, sourceSupport->second,
                    mirrorSector, mirrorSurface,
                    *sourceProfile, *mirrorProfile);
            }
            if ( !strictCertificate && !fullRunCertificate && !pairCertificate )
            {
                continue;
            }
            foundMirrorEdge = true;
            if ( !surfaceHasValidTextureTopology(mirrorSector, mirrorSurface) )
            {
                return false;
            }
        }
        if ( !foundMirrorEdge )
        {
            return false;
        }

        if ( mirrorSectors != nullptr
            && std::find(mirrorSectors->begin(), mirrorSectors->end(), mirrorSectorIndex) == mirrorSectors->end() )
        {
            mirrorSectors->push_back(mirrorSectorIndex);
        }
    }
    return true;
}

bool preflightSurfaceCleave(
    const Level& level,
    const SurfaceRef surfaceRef,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz,
    const CleaveProfileContext* const context = nullptr)
{
    if ( !isValidSurfaceRef(level, surfaceRef) )
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(surfaceRef.sector)];
    const Surface& surface = sector.surfaces[static_cast<std::size_t>(surfaceRef.surface)];
    if ( !surfaceCanSplitOnPlane(sector, surface, planeNormal, px, py, pz) )
    {
        return false;
    }

    if ( surface.adjoin )
    {
        if ( !isValidSurfaceRef(level, *surface.adjoin) )
        {
            return false;
        }
        const Sector& mirrorSector = level.sectors[static_cast<std::size_t>(surface.adjoin->sector)];
        const Surface& mirrorSurface =
            mirrorSector.surfaces[static_cast<std::size_t>(surface.adjoin->surface)];
        if ( !surfaceCanSplitOnPlane(mirrorSector, mirrorSurface, planeNormal, px, py, pz) )
        {
            return false;
        }
    }

    for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
    {
        const std::int32_t edgeA = surface.vertices[corner];
        const std::int32_t edgeB = surface.vertices[(corner + 1) % surface.vertices.size()];
        const std::int32_t sideA = classifyPlaneSide(
            sector.vertices[static_cast<std::size_t>(edgeA)],
            planeNormal,
            px,
            py,
            pz);
        const std::int32_t sideB = classifyPlaneSide(
            sector.vertices[static_cast<std::size_t>(edgeB)],
            planeNormal,
            px,
            py,
            pz);
        if ( sideA == 0 || sideB == 0 || sideA != -sideB )
        {
            continue;
        }
        if ( !preflightRelatedEdgeSurfaces(
                level, surfaceRef.sector, edgeA, edgeB, nullptr, context) )
        {
            return false;
        }
    }
    return true;
}

TexVertex interpolateTexVertex(
    const Vertex& start,
    const TexVertex& startTexVertex,
    const Vertex& end,
    const TexVertex& endTexVertex,
    const Vertex& inserted) noexcept
{
    const TVector edge{
        end.x - start.x,
        end.y - start.y,
        end.z - start.z
    };
    const double magnitude = std::max({std::abs(edge.x), std::abs(edge.y), std::abs(edge.z)});
    if ( !precision::finite(edge) || !(magnitude > 0.0) )
    {
        return startTexVertex;
    }

    const TVector offset{
        inserted.x - start.x,
        inserted.y - start.y,
        inserted.z - start.z
    };
    // A cut point belongs to this edge. The dominant coordinate supplies its
    // affine parameter without squaring a short edge or overflowing a long one.
    const bool useX = std::abs(edge.x) == magnitude;
    const bool useY = !useX && std::abs(edge.y) == magnitude;
    const double startOffset = useX ? offset.x : useY ? offset.y : offset.z;
    const double endOffset = useX ? inserted.x - end.x : useY ? inserted.y - end.y : inserted.z - end.z;
    const double component = useX ? edge.x : useY ? edge.y : edge.z;
    const bool fromEnd = std::abs(endOffset) < std::abs(startOffset);
    const double parameter = fromEnd ? -endOffset / component : startOffset / component;
    if ( !std::isfinite(parameter) )
    {
        return startTexVertex;
    }
    const double amount = std::clamp(parameter, 0.0, 1.0);
    const auto lerp = [amount](const float a, const float b) noexcept
    {
        // Evaluate the endpoint difference in double precision. Two valid
        // finite float endpoints may differ by more than FLT_MAX even though
        // every convex interpolation result remains float-representable.
        return static_cast<float>(
            static_cast<double>(a)
            + (static_cast<double>(b) - static_cast<double>(a)) * amount);
    };

    // Start at the closer endpoint to retain a small authored endpoint value
    // instead of recovering it by subtracting two nearly equal larger values.
    const TexVertex& first = fromEnd ? endTexVertex : startTexVertex;
    const TexVertex& last = fromEnd ? startTexVertex : endTexVertex;
    return TexVertex{
        lerp(first.u, last.u),
        lerp(first.v, last.v),
        ColorF{
            lerp(first.color.r, last.color.r),
            lerp(first.color.g, last.color.g),
            lerp(first.color.b, last.color.b),
            lerp(first.color.a, last.color.a)
        }
    };
}

TexVertex texVertexForInsertedEdgePoint(
    const Sector& sector,
    const Surface& surface,
    const std::size_t startCorner,
    const std::size_t endCorner,
    const Vertex& inserted) noexcept
{
    if ( startCorner >= surface.vertices.size()
        || endCorner >= surface.vertices.size()
        || startCorner >= surface.texVertices.size()
        || endCorner >= surface.texVertices.size() )
    {
        return TexVertex{};
    }

    const std::int32_t startIndex = surface.vertices[startCorner];
    const std::int32_t endIndex = surface.vertices[endCorner];
    if ( startIndex < 0 || endIndex < 0
        || static_cast<std::size_t>(startIndex) >= sector.vertices.size()
        || static_cast<std::size_t>(endIndex) >= sector.vertices.size() )
    {
        return TexVertex{};
    }

    return interpolateTexVertex(
        sector.vertices[static_cast<std::size_t>(startIndex)],
        surface.texVertices[startCorner],
        sector.vertices[static_cast<std::size_t>(endIndex)],
        surface.texVertices[endCorner],
        inserted);
}

void renumberSectorSurfaces(Sector& sector) noexcept
{
    for (std::size_t i = 0; i < sector.surfaces.size(); ++i)
    {
        sector.surfaces[i].num = static_cast<std::int32_t>(i);
    }
}

void splitVertices(Sector& sector, const std::vector<std::int32_t>& vertexIndices, const TVector& planeNormal, const double px, const double py, const double pz) noexcept
{
    for (const std::int32_t vi : vertexIndices)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
        {
            continue;
        }

        Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
        v.mark = classifyPlaneSide(v, planeNormal, px, py, pz);
    }
}

void assignSurfaceParams(const Surface& src, Surface& dst)
{
    dst.surfaceFlags = src.surfaceFlags;
    dst.faceFlags    = src.faceFlags;
    dst.geo          = src.geo;
    dst.light        = src.light;
    dst.tex          = src.tex;
    dst.extraLight   = src.extraLight;
    dst.adjoinFlags  = src.adjoinFlags;
    dst.material     = src.material;
    dst.uScale       = src.uScale;
    dst.vScale       = src.vScale;

    dst.adjoin.reset();
    dst.nAdj = -1;
    dst.nMat = src.nMat;
}

std::vector<std::int32_t> buildSurfaceCutVertices(
    Sector& sector,
    const std::vector<std::int32_t>& originalVertices,
    const TVector& planeNormal,
    const TVector& planePoint,
    const SurfaceRef surfaceRef,
    detail::ExtrudeArrangementContext* const arrangement)
{
    std::vector<std::int32_t> cutVertices(originalVertices.size(), -1);

    // Step 1: walk every original edge and cut only edges whose endpoints are on opposite sides.
    for ( std::size_t i = 0; i < originalVertices.size(); ++i )
    {
        const std::size_t next = (i + 1) % originalVertices.size();

        const std::int32_t v1Index = originalVertices[i];
        const std::int32_t v2Index = originalVertices[next];
        if (v1Index < 0 || v2Index < 0 || static_cast<std::size_t>(v1Index) >= sector.vertices.size()
            || static_cast<std::size_t>(v2Index) >= sector.vertices.size())
        {
            continue;
        }

        // Canonical endpoint order makes the same shared edge produce exactly
        // the same intersection when visited in the reverse face winding.
        const Vertex& v1 = sector.vertices[static_cast<std::size_t>(std::min(v1Index, v2Index))];
        const Vertex& v2 = sector.vertices[static_cast<std::size_t>(std::max(v1Index, v2Index))];

        if ((v1.mark == 0) || (v1.mark != -v2.mark))
        {
            continue;
        }

        if ( arrangement )
        {
            const auto index = arrangement->cutVertex(sector,surfaceRef,v1Index,v2Index);
            if ( !index )
            {
                cutVertices[i] = -2;
                return cutVertices;
            }
            cutVertices[i] = *index;
            continue;
        }

        // Step 2: create one new sector vertex at the plane/edge intersection.
        double ax = 0.0;
        double ay = 0.0;
        double az = 0.0;
        if (!planeLineIntersection(planeNormal, planePoint.x, planePoint.y, planePoint.z,
                v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, ax, ay, az))
        {
            continue;
        }

        cutVertices[i] = precision::addVertex(sector, ax, ay, az);
    }

    return cutVertices;
}

std::pair<std::int32_t, std::int32_t> countSurfaceSplitSides(
    const Sector& sector,
    const std::vector<std::int32_t>& originalVertices) noexcept
{
    std::int32_t nleft = 0;
    std::int32_t nright = 0;

    // Step 1: only original vertices decide whether the cleave actually splits the surface.
    for (const std::int32_t vi : originalVertices)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
        {
            continue;
        }

        const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
        if (v.mark == 1)
        {
            ++nright;
        }
        else if (v.mark == -1)
        {
            ++nleft;
        }
    }

    return { nleft, nright };
}

bool buildSplitSurfaceContours(
    const Sector& sector,
    const std::vector<std::int32_t>& originalVertices,
    const std::vector<TexVertex>& originalTexVertices,
    const std::vector<std::int32_t>& cutVertices,
    Surface& rightSurface,
    std::vector<std::int32_t>& leftVertices,
    std::vector<TexVertex>& leftTexVertices,
    const SurfaceRef surfaceRef,
    detail::ExtrudeArrangementContext* const arrangement)
{
    // Step 1: copy original vertices into the side that matches their plane mark; plane vertices go to both sides.
    for (std::size_t i = 0; i < originalVertices.size(); ++i)
    {
        const std::int32_t vi = originalVertices[i];
        const TexVertex tv = (i < originalTexVertices.size()) ? originalTexVertices[i] : TexVertex{};

        if ( vi >= 0 && static_cast<std::size_t>(vi) < sector.vertices.size() )
        {
            const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
            if ( v.mark == 1 || v.mark == 0 )
            {
                rightSurface.vertices.push_back(vi);
                rightSurface.texVertices.push_back(tv);
            }
            if ( v.mark == -1 || v.mark == 0 )
            {
                leftVertices.push_back(vi);
                leftTexVertices.push_back(tv);
            }
        }

        // Step 2: insert every generated cut vertex into both output contours at the cut edge.
        const std::int32_t cv = cutVertices[i];
        if ( cv >= 0 )
        {
            TexVertex cutTexVertex = tv;
            const std::size_t next = (i + 1) % originalVertices.size();
            if ( static_cast<std::size_t>(cv) < sector.vertices.size()
                && i < originalTexVertices.size()
                && next < originalTexVertices.size() )
            {
                const std::int32_t startIndex = originalVertices[i];
                const std::int32_t endIndex = originalVertices[next];
                if ( startIndex >= 0 && endIndex >= 0
                    && static_cast<std::size_t>(startIndex) < sector.vertices.size()
                    && static_cast<std::size_t>(endIndex) < sector.vertices.size() )
                {
                    cutTexVertex = interpolateTexVertex(
                        sector.vertices[static_cast<std::size_t>(startIndex)],
                        originalTexVertices[i],
                        sector.vertices[static_cast<std::size_t>(endIndex)],
                        originalTexVertices[next],
                        sector.vertices[static_cast<std::size_t>(cv)]);
                }
            }

            if ( arrangement && arrangement->isSource(surfaceRef) )
            {
                const auto tuple = arrangement->sourceTuple(surfaceRef,cv);
                if ( !tuple )
                {
                    return false;
                }
                cutTexVertex = *tuple;
            }

            rightSurface.vertices.push_back(cv);
            rightSurface.texVertices.push_back(cutTexVertex);

            leftVertices.push_back(cv);
            leftTexVertices.push_back(cutTexVertex);
        }
    }
    return true;
}

struct ProjectedPoint
{
    double x{};
    double y{};
};

bool collectSurfacePoints(const Sector& sector, const Surface& surface, std::vector<TVector>& out)
{
    out.clear();
    out.reserve(surface.vertices.size());

    for (const std::int32_t vertexIndex : surface.vertices)
    {
        if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
        {
            return false;
        }

        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
        out.push_back(TVector{ vertex.x, vertex.y, vertex.z });
    }

    return out.size() >= 3;
}

bool buildSurfaceProjectionBasis(
    const std::vector<TVector>& points,
    const TVector& normal,
    TVector& axisU,
    TVector& axisV) noexcept
{
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        axisU = points[i] - points[0];
        if (axisU.normalize() != 0.0)
        {
            break;
        }
    }

    if (axisU.length() == 0.0)
    {
        return false;
    }

    axisV = normal.cross(axisU);
    return axisV.normalize() != 0.0;
}

std::vector<ProjectedPoint> projectSurfacePolygon(
    const std::vector<TVector>& points,
    const TVector& axisU,
    const TVector& axisV)
{
    std::vector<ProjectedPoint> projected{};
    projected.reserve(points.size());

    for (const TVector& point : points)
    {
        projected.push_back(ProjectedPoint{ point.dot(axisU), point.dot(axisV) });
    }

    return projected;
}

bool projectedBoxesOverlap(const std::vector<ProjectedPoint>& a, const std::vector<ProjectedPoint>& b) noexcept
{
    constexpr double kOverlapEpsilon = 0.001;

    const auto bounds = [](const std::vector<ProjectedPoint>& polygon)
    {
        ProjectedPoint min{ polygon.front() };
        ProjectedPoint max{ polygon.front() };
        for (const ProjectedPoint& point : polygon)
        {
            min.x = std::min(min.x, point.x);
            min.y = std::min(min.y, point.y);
            max.x = std::max(max.x, point.x);
            max.y = std::max(max.y, point.y);
        }

        return std::pair{ min, max };
    };

    const auto [aMin, aMax] = bounds(a);
    const auto [bMin, bMax] = bounds(b);
    return !(aMax.x < bMin.x - kOverlapEpsilon
        || aMin.x > bMax.x + kOverlapEpsilon
        || aMax.y < bMin.y - kOverlapEpsilon
        || aMin.y > bMax.y + kOverlapEpsilon);
}

double projectedCross(const ProjectedPoint& a, const ProjectedPoint& b, const ProjectedPoint& c) noexcept
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool projectedPointOnSegment(const ProjectedPoint& p, const ProjectedPoint& a, const ProjectedPoint& b) noexcept
{
    constexpr double kOverlapEpsilon = 0.001;
    if (std::abs(projectedCross(a, b, p)) > kOverlapEpsilon)
    {
        return false;
    }

    return p.x >= std::min(a.x, b.x) - kOverlapEpsilon
        && p.x <= std::max(a.x, b.x) + kOverlapEpsilon
        && p.y >= std::min(a.y, b.y) - kOverlapEpsilon
        && p.y <= std::max(a.y, b.y) + kOverlapEpsilon;
}

bool projectedSegmentsIntersect(
    const ProjectedPoint& a0,
    const ProjectedPoint& a1,
    const ProjectedPoint& b0,
    const ProjectedPoint& b1) noexcept
{
    constexpr double kOverlapEpsilon = 0.001;
    const double d0 = projectedCross(a0, a1, b0);
    const double d1 = projectedCross(a0, a1, b1);
    const double d2 = projectedCross(b0, b1, a0);
    const double d3 = projectedCross(b0, b1, a1);

    if (((d0 > kOverlapEpsilon && d1 < -kOverlapEpsilon) || (d0 < -kOverlapEpsilon && d1 > kOverlapEpsilon))
        && ((d2 > kOverlapEpsilon && d3 < -kOverlapEpsilon) || (d2 < -kOverlapEpsilon && d3 > kOverlapEpsilon)))
    {
        return true;
    }

    return projectedPointOnSegment(b0, a0, a1)
        || projectedPointOnSegment(b1, a0, a1)
        || projectedPointOnSegment(a0, b0, b1)
        || projectedPointOnSegment(a1, b0, b1);
}

bool projectedPointInPolygon(const ProjectedPoint& p, const std::vector<ProjectedPoint>& polygon) noexcept
{
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
    {
        const ProjectedPoint& a = polygon[j];
        const ProjectedPoint& b = polygon[i];
        if (projectedPointOnSegment(p, a, b))
        {
            return true;
        }

        const bool crosses = ((a.y > p.y) != (b.y > p.y))
            && (p.x < (b.x - a.x) * (p.y - a.y) / ((b.y - a.y) + 1.0e-12) + a.x);
        if (crosses)
        {
            inside = !inside;
        }
    }

    return inside;
}

bool projectedPolygonsOverlap(const std::vector<ProjectedPoint>& a, const std::vector<ProjectedPoint>& b) noexcept
{
    if (a.size() < 3 || b.size() < 3 || !projectedBoxesOverlap(a, b))
    {
        return false;
    }

    for (const ProjectedPoint& point : a)
    {
        if (projectedPointInPolygon(point, b))
        {
            return true;
        }
    }

    for (const ProjectedPoint& point : b)
    {
        if (projectedPointInPolygon(point, a))
        {
            return true;
        }
    }

    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const ProjectedPoint& a0 = a[i];
        const ProjectedPoint& a1 = a[(i + 1) % a.size()];
        for (std::size_t j = 0; j < b.size(); ++j)
        {
            if (projectedSegmentsIntersect(a0, a1, b[j], b[(j + 1) % b.size()]))
            {
                return true;
            }
        }
    }

    return false;
}

bool surfacesProjectedOverlapLocal(
    const Sector& srcSector,
    const Surface& srcSurface,
    const Sector& dstSector,
    const Surface& dstSurface,
    const TVector& srcNormal)
{
    std::vector<TVector> srcPoints{};
    std::vector<TVector> dstPoints{};
    if (!collectSurfacePoints(srcSector, srcSurface, srcPoints)
        || !collectSurfacePoints(dstSector, dstSurface, dstPoints))
    {
        return false;
    }

    TVector axisU{};
    TVector axisV{};
    if (!buildSurfaceProjectionBasis(srcPoints, srcNormal, axisU, axisV))
    {
        return false;
    }

    return projectedPolygonsOverlap(
        projectSurfacePolygon(srcPoints, axisU, axisV),
        projectSurfacePolygon(dstPoints, axisU, axisV));
}

bool surfacesMayNeedConnectFallbackLocal(
    const Sector& srcSector,
    const Surface& srcSurface,
    const Sector& dstSector,
    const Surface& dstSurface)
{
    if (srcSurface.vertices.empty() || dstSurface.vertices.empty())
    {
        return false;
    }

    TVector srcNormal = srcSurface.normal;
    if (srcNormal.length() == 0.0)
    {
        srcNormal = surfaceCalcNormalNewell(srcSector, srcSurface);
    }

    TVector dstNormal = dstSurface.normal;
    if (dstNormal.length() == 0.0)
    {
        dstNormal = surfaceCalcNormalNewell(dstSector, dstSurface);
    }

    if (srcNormal.length() == 0.0 || dstNormal.length() == 0.0)
    {
        return false;
    }

    srcNormal.normalize();
    dstNormal.normalize();

    if (srcNormal.dot(dstNormal) > -0.995)
    {
        return false;
    }

    const std::int32_t srcV = srcSurface.vertices.front();
    const std::int32_t dstV = dstSurface.vertices.front();
    if (srcV < 0 || dstV < 0
        || static_cast<std::size_t>(srcV) >= srcSector.vertices.size()
        || static_cast<std::size_t>(dstV) >= dstSector.vertices.size())
    {
        return false;
    }

    const Vertex& sv = srcSector.vertices[static_cast<std::size_t>(srcV)];
    const Vertex& dv = dstSector.vertices[static_cast<std::size_t>(dstV)];
    const double planeDist = std::abs(srcNormal.dot(dv.x - sv.x, dv.y - sv.y, dv.z - sv.z));
    if (planeDist > 0.02)
    {
        return false;
    }

    return surfacesProjectedOverlapLocal(srcSector, srcSurface, dstSector, dstSurface, srcNormal);
}

bool sectorsMayIntersectForConnect(const Sector& sector1, const Sector& sector2)
{
    if (sector1.vertices.empty() || sector2.vertices.empty())
    {
        return false;
    }

    if (!doBoxesIntersect(sectorCalcBBox(sector1), sectorCalcBBox(sector2)))
    {
        return false;
    }

    if (sectorDoOverlap(sector1, sector2))
    {
        return true;
    }

    for (const Surface& surface1 : sector1.surfaces)
    {
        for (const Surface& surface2 : sector2.surfaces)
        {
            if (surfacesMayNeedConnectFallbackLocal(sector1, surface1, sector2, surface2))
            {
                return true;
            }
        }
    }

    return true;
}

std::vector<SurfaceRef> collectSurfaceCandidates(
    const Level& level,
    const std::int32_t sectorIndex,
    const std::int32_t originalSurface,
    const std::int32_t originalSurfaceCount)
{
    std::vector<SurfaceRef> out{};
    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        return out;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    if (originalSurface >= 0 && static_cast<std::size_t>(originalSurface) < sector.surfaces.size())
    {
        out.push_back(SurfaceRef{ sectorIndex, originalSurface });
    }

    const std::int32_t start = std::max<std::int32_t>(0, originalSurfaceCount);
    for (std::int32_t surfaceIndex = start; surfaceIndex < static_cast<std::int32_t>(sector.surfaces.size()); ++surfaceIndex)
    {
        out.push_back(SurfaceRef{ sectorIndex, surfaceIndex });
    }

    return out;
}

bool surfaceBoundariesMatchWithResolvedNormals(const Sector& firstSector, const Surface& first,
    const Sector& secondSector, const Surface& second,
    const bool allowSourceRounding = false,
    const CleaveProfileContext* const context = nullptr,
    const SurfaceRef firstRef = {},
    const SurfaceRef secondRef = {})
{
    // The matcher requires facing normals. Public editing also accepts unset
    // original caches, which are derived for inspection without authoring them.
    std::optional<Surface> resolvedFirst;
    std::optional<Surface> resolvedSecond;
    if ( first.normal.x == 0.0 && first.normal.y == 0.0 && first.normal.z == 0.0 )
    {
        resolvedFirst = first;
        resolvedFirst->normal = surfaceCalcNormalNewell(firstSector, first);
    }
    if ( second.normal.x == 0.0 && second.normal.y == 0.0 && second.normal.z == 0.0 )
    {
        resolvedSecond = second;
        resolvedSecond->normal = surfaceCalcNormalNewell(secondSector, second);
    }
    const Surface& checkedFirst = resolvedFirst ? *resolvedFirst : first;
    const Surface& checkedSecond = resolvedSecond ? *resolvedSecond : second;
    if ( precision::surfaceBoundariesMatchChecked(
            firstSector, checkedFirst, secondSector, checkedSecond) )
    {
        return true;
    }
    if ( !allowSourceRounding || !context )
    {
        return false;
    }

    TVector firstNormal = checkedFirst.normal;
    TVector secondNormal = checkedSecond.normal;
    if ( !precision::finite(firstNormal) || firstNormal.isZero() )
    {
        firstNormal = surfaceCalcNormalNewell(firstSector, first);
    }
    if ( !precision::finite(secondNormal) || secondNormal.isZero() )
    {
        secondNormal = surfaceCalcNormalNewell(secondSector, second);
    }
    if ( !precision::finite(firstNormal) || !precision::finite(secondNormal)
        || firstNormal.isZero() || secondNormal.isZero()
        || firstNormal.dot(secondNormal) > -0.995 )
    {
        return false;
    }

    const double firstLimit = context->sourceBoundaryDeviation(firstRef).value_or(0.0);
    const double secondLimit = context->sourceBoundaryDeviation(secondRef).value_or(0.0);
    const double deviationLimit = std::max(firstLimit, secondLimit);
    if ( deviationLimit < 0.0 || !std::isfinite(deviationLimit) )
    {
        return false;
    }
    const CleaveFaceProfile* const firstProfile = context->sourceFaceProfile(firstRef);
    const CleaveFaceProfile* const secondProfile = context->sourceFaceProfile(secondRef);
    if ( firstProfile == nullptr || secondProfile == nullptr
        || firstProfile->geometry.surfaces.empty()
        || secondProfile->geometry.surfaces.empty() )
    {
        return false;
    }

    const bool originalRoundedMatch = sourceRoundedBoundaryMatch(
        firstProfile->geometry,
        firstProfile->geometry.surfaces.front(),
        secondProfile->geometry,
        secondProfile->geometry.surfaces.front(),
        deviationLimit,
        firstProfile,
        secondProfile,
        true);
    std::optional<NativePairCertificate> nativePairCertificate;
    if ( !originalRoundedMatch )
    {
        nativePairCertificate = sourceNativePairMatch(*firstProfile, *secondProfile);
        if ( !nativePairCertificate )
        {
            return false;
        }
    }

    // The immutable profiles prove that this is one authored boundary with a
    // redundant rounded corner. Generated descendants may have exactly the
    // same number of corners after the cut, so compare their current points
    // against the measured source residual without requiring another removal.
    const double descendantLimit = nativePairCertificate
        ? std::max(deviationLimit, nativePairCertificate->delta)
        : deviationLimit;
    return sourceRoundedBoundaryMatch(
        firstSector, first, secondSector, second,
        descendantLimit,
        firstProfile,
        secondProfile,
        false,
        nativePairCertificate ? &*nativePairCertificate : nullptr);
}

bool tryAdjoinSurfaceCandidatePairs(
    Level& level,
    const std::vector<SurfaceRef>& candidates1,
    const std::vector<SurfaceRef>& candidates2,
    CleaveProfileContext* const context = nullptr)
{
    // Step 1: try every valid, unadjoined candidate pair and accept the first overlapping pair.
    for (const SurfaceRef candidate1 : candidates1)
    {
        if (!isValidSurfaceRef(level, candidate1))
        {
            continue;
        }

        Sector& sec1 = level.sectors[static_cast<std::size_t>(candidate1.sector)];
        Surface& surf1 = sec1.surfaces[static_cast<std::size_t>(candidate1.surface)];
        if (surf1.adjoin)
        {
            continue;
        }
        if (surf1.normal.length() == 0.0)
        {
            surf1.normal = surfaceCalcNormalNewell(sec1, surf1);
        }

        for (const SurfaceRef candidate2 : candidates2)
        {
            if (!isValidSurfaceRef(level, candidate2))
            {
                continue;
            }

            Sector& sec2 = level.sectors[static_cast<std::size_t>(candidate2.sector)];
            Surface& surf2 = sec2.surfaces[static_cast<std::size_t>(candidate2.surface)];
            if (surf2.adjoin)
            {
                continue;
            }
            if (surf2.normal.length() == 0.0)
            {
                surf2.normal = surfaceCalcNormalNewell(sec2, surf2);
            }

            if ( !(context ? surfaceBoundariesMatchWithResolvedNormals(
                            sec1, surf1, sec2, surf2, true, context, candidate1, candidate2)
                           : precision::surfaceDoOverlapChecked(sec1, surf1, sec2, surf2)) )
            {
                continue;
            }

            return levelSysAdjoinSurfaces(level, candidate1, candidate2, true);
        }
    }

    return false;
}

bool levelMakeAdjoinFromSectorCandidates(
    Level& level,
    const SurfaceRef surface,
    std::vector<std::int32_t> candidateSectors,
    const bool reset,
    CleaveProfileContext* const context = nullptr,
    const bool allowAdjoinedExactCandidate = false)
{
    if (!isValidSurfaceRef(level, surface))
    {
        return false;
    }

    Sector& srcSector = level.sectors[static_cast<std::size_t>(surface.sector)];
    Surface& srcSurf = srcSector.surfaces[static_cast<std::size_t>(surface.surface)];
    if (srcSurf.adjoin)
    {
        return false;
    }

    std::sort(candidateSectors.begin(), candidateSectors.end());
    candidateSectors.erase(std::unique(candidateSectors.begin(), candidateSectors.end()), candidateSectors.end());

    for (const std::int32_t sectorIndex : candidateSectors)
    {
        if (sectorIndex == surface.sector || sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            continue;
        }

        Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(sector.surfaces.size()); ++sf)
        {
            const SurfaceRef otherRef{ sectorIndex, sf };
            Surface& other = sector.surfaces[static_cast<std::size_t>(sf)];
            if ( other.adjoin && !allowAdjoinedExactCandidate )
            {
                continue;
            }

            if ( context ? surfaceBoundariesMatchWithResolvedNormals(
                            srcSector, srcSurf, sector, other, true, context, surface, otherRef)
                         : precision::surfaceDoOverlapChecked(srcSector, srcSurf, sector, other) )
            {
                return levelSysAdjoinSurfaces(level, surface, otherRef, reset);
            }
        }
    }

    for (const std::int32_t sectorIndex : candidateSectors)
    {
        if (sectorIndex == surface.sector || sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            continue;
        }

        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(sector.surfaces.size()); ++sf)
        {
            const SurfaceRef otherRef{ sectorIndex, sf };
            const Surface& other = sector.surfaces[static_cast<std::size_t>(sf)];

            if (other.adjoin)
            {
                continue;
            }

            if (!surfacesMayNeedConnectFallbackLocal(srcSector, srcSurf, sector, other))
            {
                continue;
            }

            Level candidate = level;
            std::optional<CleaveProfileContext> candidateProfiles;
            if ( context )
            {
                candidateProfiles = *context;
            }
            if ( !levelConnectSurfacesImpl(candidate, surface, otherRef,
                    candidateProfiles ? &*candidateProfiles : nullptr) )
            {
                continue;
            }

            level = std::move(candidate);
            if ( context )
            {
                *context = std::move(*candidateProfiles);
            }
            return true;
        }
    }

    return false;
}

bool connectSectorSurfacesToTargetSectors(
    Level& level,
    const std::vector<std::int32_t>& sourceSectors,
    const std::vector<std::int32_t>& targetSectors,
    bool* committedConnection = nullptr,
    CleaveProfileContext* const context = nullptr)
{
    constexpr int kMaxConnectPasses = 2048;

    const auto isValidSector = [&level](const std::int32_t sectorIndex) noexcept
    {
        return sectorIndex >= 0 && static_cast<std::size_t>(sectorIndex) < level.sectors.size();
    };

    bool anyConnected = false;

    // Step 1: each successful fallback can split surfaces and append new candidates. Restart the
    // scan after every connection so remaining pieces from the same cutter face are not skipped.
    for (int pass = 0; pass < kMaxConnectPasses; ++pass)
    {
        bool progress = false;
        for (const std::int32_t sourceSector : sourceSectors)
        {
            for (std::int32_t sf = 0;
                 isValidSector(sourceSector)
                 && sf < static_cast<std::int32_t>(level.sectors[static_cast<std::size_t>(sourceSector)].surfaces.size());
                 ++sf)
            {
                if (!levelMakeAdjoinFromSectorCandidates(level, SurfaceRef{ sourceSector, sf }, targetSectors, true, context))
                {
                    continue;
                }

                anyConnected = true;
                if ( committedConnection )
                {
                    *committedConnection = true;
                }
                progress = true;
                break;
            }

            if (progress)
            {
                break;
            }
        }

        if (!progress)
        {
            break;
        }
    }

    return anyConnected;
}

bool sectorsHaveAdjoinBetween(
    const Level& level,
    const std::vector<std::int32_t>& sourceSectors,
    const std::vector<std::int32_t>& targetSectors) noexcept
{
    const auto isValidSector = [&level](const std::int32_t sectorIndex) noexcept
    {
        return sectorIndex >= 0 && static_cast<std::size_t>(sectorIndex) < level.sectors.size();
    };

    for (const std::int32_t sourceSector : sourceSectors)
    {
        if (!isValidSector(sourceSector))
        {
            continue;
        }

        const Sector& sector = level.sectors[static_cast<std::size_t>(sourceSector)];
        for (const Surface& surface : sector.surfaces)
        {
            if (!surface.adjoin)
            {
                continue;
            }

            if (std::find(targetSectors.begin(), targetSectors.end(), surface.adjoin->sector) != targetSectors.end())
            {
                return true;
            }
        }
    }

    return false;
}

void assignSectorParams(const Sector& src, Sector& dst)
{
    dst.flags       = src.flags;
    dst.ambient     = src.ambient;
    dst.extraLight  = src.extraLight;
    dst.pointLight  = src.pointLight;
    dst.colormap    = src.colormap;
    dst.tint        = src.tint;
    dst.sound       = src.sound;
    dst.soundVolume = src.soundVolume;
    dst.thrust      = src.thrust;
    dst.collideBox  = src.collideBox;
    dst.layer       = src.layer;
}

bool isValidSectorIndex(const Level& level, const std::int32_t sectorIndex) noexcept
{
    if (sectorIndex < 0)
    {
        return false;
    }

    return static_cast<std::size_t>(sectorIndex) < level.sectors.size();
}

TVector resolvedSurfaceNormal(const Sector& sector, const Surface& surface) noexcept
{
    TVector normal = surface.normal;
    if (normal.length() == 0.0)
    {
        normal = surfaceCalcNormalNewell(sector, surface);
    }

    if (normal.normalize() == 0.0)
    {
        return TVector{};
    }

    return normal;
}

std::vector<Plane> collectSectorPlanes(
    const Level& level,
    const std::int32_t sectorIndex,
    const SectorPlaneFilter filter = SectorPlaneFilter::AllSurfaces)
{
    std::vector<Plane> planes{};
    if (!isValidSectorIndex(level, sectorIndex))
    {
        return planes;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    planes.reserve(sector.surfaces.size());

    // Step 1: each non-degenerate surface contributes one clipping plane at its first vertex.
    for (const Surface& surface : sector.surfaces)
    {
        if (filter == SectorPlaneFilter::SolidSurfacesOnly && surface.adjoin)
        {
            continue;
        }

        if (surface.vertices.empty())
        {
            continue;
        }

        const TVector normal = resolvedSurfaceNormal(sector, surface);
        if (normal.length() == 0.0)
        {
            continue;
        }

        const std::int32_t v0Index = surface.vertices[0];
        if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
        {
            continue;
        }

        const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];
        planes.push_back(Plane{ normal, v0.x, v0.y, v0.z });
    }

    return planes;
}

bool cleaveSurfaceByBoundary(
    Level& level,
    const SurfaceRef target,
    const Sector& bySector,
    const std::vector<std::int32_t>& byVertices,
    const TVector& byNormal,
    CleaveProfileContext* const context = nullptr)
{
    const std::size_t count = byVertices.size();
    if (count < 2)
    {
        return true;
    }

    // Step 1: every boundary edge creates a clipping plane perpendicular to the source surface.
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::size_t next = (i + 1) % count;
        const std::int32_t aIndex = byVertices[i];
        const std::int32_t bIndex = byVertices[next];

        if (aIndex < 0 || bIndex < 0 || static_cast<std::size_t>(aIndex) >= bySector.vertices.size()
            || static_cast<std::size_t>(bIndex) >= bySector.vertices.size())
        {
            continue;
        }

        const Vertex& a = bySector.vertices[static_cast<std::size_t>(aIndex)];
        const Vertex& b = bySector.vertices[static_cast<std::size_t>(bIndex)];

        TVector edge{};
        edge.setCoords(b.x - a.x, b.y - a.y, b.z - a.z);
        if (edge.normalize() == 0.0)
        {
            continue;
        }

        const TVector clippingNormal = edge.cross(byNormal);
        // Surface Connect admits only unadjoined targets and pairs portals
        // after both clipping loops. A boundary plane that does not split its
        // target is expected here; mirrored-portal repair cannot fail here.
        if ( context )
        {
            const Sector& targetSector = level.sectors[static_cast<std::size_t>(target.sector)];
            const Surface& targetFace = targetSector.surfaces[static_cast<std::size_t>(target.surface)];
            if ( surfaceCanSplitOnPlane(targetSector, targetFace, clippingNormal, a.x, a.y, a.z)
                && !levelCleaveSurfaceImpl(level, target, clippingNormal, a.x, a.y, a.z, context) )
            {
                return false;
            }
        }
        else
        {
            (void)levelCleaveSurfaceImpl(level, target, clippingNormal, a.x, a.y, a.z);
        }
    }
    return true;
}

bool sectorInteriorOverlaps(const Sector& sector, const Sector& container) noexcept
{
    if (sector.vertices.empty() || container.vertices.empty())
    {
        return false;
    }

    if (!doBoxesIntersect(sectorCalcBBox(sector), sectorCalcBBox(container)))
    {
        return false;
    }

    const auto containsInteriorPoint = [](const Sector& testSector, const TVector& point) noexcept
    {
        for (const Surface& surface : testSector.surfaces)
        {
            if (surface.vertices.empty())
            {
                continue;
            }

            const std::int32_t v0Index = surface.vertices[0];
            if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= testSector.vertices.size())
            {
                return false;
            }

            const TVector normal = resolvedSurfaceNormal(testSector, surface);
            if (normal.length() == 0.0)
            {
                continue;
            }

            const Vertex& v0 = testSector.vertices[static_cast<std::size_t>(v0Index)];
            if (normal.dot(point.x - v0.x, point.y - v0.y, point.z - v0.z) <= 0.001)
            {
                return false;
            }
        }

        return true;
    };

    const auto sectorsOverlap = [](const Sector& a, const Sector& b) noexcept
    {
        const auto surfacesDoNotSeparate = [](const Sector& source, const Sector& other) noexcept
        {
            for (const Surface& surface : source.surfaces)
            {
                if (surface.vertices.empty())
                {
                    continue;
                }

                const std::int32_t v0Index = surface.vertices[0];
                if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= source.vertices.size())
                {
                    return false;
                }

                const TVector normal = resolvedSurfaceNormal(source, surface);
                if (normal.length() == 0.0)
                {
                    continue;
                }

                const Vertex& v0 = source.vertices[static_cast<std::size_t>(v0Index)];

                bool hasVertexInsidePlane = false;
                for (const Vertex& vertex : other.vertices)
                {
                    if (normal.dot(vertex.x - v0.x, vertex.y - v0.y, vertex.z - v0.z) > 0.01)
                    {
                        hasVertexInsidePlane = true;
                        break;
                    }
                }

                if (!hasVertexInsidePlane)
                {
                    return false;
                }
            }

            return true;
        };

        return surfacesDoNotSeparate(a, b) && surfacesDoNotSeparate(b, a);
    };

    if (sectorsOverlap(sector, container))
    {
        return true;
    }

    const auto sectorHasInteriorSampleInside = [&containsInteriorPoint](const Sector& source, const Sector& testSector) noexcept
    {
        // Step 1: start with the centroid; clipped knife remnants usually still have a reliable center.
        if (containsInteriorPoint(testSector, sectorCalcCenter(source)))
        {
            return true;
        }

        // Step 2: then try vertices, edge midpoints, and surface centroids. Rotated cutters can cross
        // a complex target without leaving a source vertex deep enough for the exact overlap test.
        for (const Surface& surface : source.surfaces)
        {
            TVector surfaceSum{};
            std::int32_t validVertexCount = 0;

            for (std::size_t i = 0; i < surface.vertices.size(); ++i)
            {
                const std::int32_t currentIndex = surface.vertices[i];
                if (currentIndex < 0 || static_cast<std::size_t>(currentIndex) >= source.vertices.size())
                {
                    continue;
                }

                const Vertex& currentVertex = source.vertices[static_cast<std::size_t>(currentIndex)];
                const TVector current{ currentVertex.x, currentVertex.y, currentVertex.z };
                surfaceSum = surfaceSum + current;
                ++validVertexCount;

                if (containsInteriorPoint(testSector, current))
                {
                    return true;
                }

                if (surface.vertices.size() < 2)
                {
                    continue;
                }

                const std::int32_t nextIndex = surface.vertices[(i + 1) % surface.vertices.size()];
                if (nextIndex < 0 || static_cast<std::size_t>(nextIndex) >= source.vertices.size())
                {
                    continue;
                }

                const Vertex& nextVertex = source.vertices[static_cast<std::size_t>(nextIndex)];
                const TVector midpoint{
                    (currentVertex.x + nextVertex.x) * 0.5,
                    (currentVertex.y + nextVertex.y) * 0.5,
                    (currentVertex.z + nextVertex.z) * 0.5,
                };
                if (containsInteriorPoint(testSector, midpoint))
                {
                    return true;
                }
            }

            if (validVertexCount <= 0)
            {
                continue;
            }

            const TVector surfaceCenter = surfaceSum * (1.0 / static_cast<double>(validVertexCount));
            if (containsInteriorPoint(testSector, surfaceCenter))
            {
                return true;
            }
        }

        return false;
    };

    return sectorHasInteriorSampleInside(sector, container)
        || sectorHasInteriorSampleInside(container, sector);
}

std::vector<TrackedSectorFragment> validUniqueTrackedSectorFragments(
    const Level& level,
    std::vector<TrackedSectorFragment> fragments)
{
    std::erase_if(
        fragments,
        [&level](const TrackedSectorFragment& fragment)
        {
            return !isValidSectorIndex(level, fragment.sector);
        });

    std::sort(
        fragments.begin(),
        fragments.end(),
        [](const TrackedSectorFragment& a, const TrackedSectorFragment& b)
        {
            return a.sector < b.sector;
        });

    std::vector<TrackedSectorFragment> merged{};
    merged.reserve(fragments.size());
    for (const TrackedSectorFragment& fragment : fragments)
    {
        if (!merged.empty() && merged.back().sector == fragment.sector)
        {
            merged.back().insideAllPlanes = merged.back().insideAllPlanes && fragment.insideAllPlanes;
            continue;
        }

        merged.push_back(fragment);
    }

    return merged;
}

SectorPlaneSideCounts countSectorSidesForPlane(const Sector& sector, const Plane& plane) noexcept
{
    SectorPlaneSideCounts counts{};
    for (const Vertex& vertex : sector.vertices)
    {
        const double dist = plane.normal.dot(vertex.x - plane.x, vertex.y - plane.y, vertex.z - plane.z);
        if (std::abs(dist) <= precision::planeError(plane.normal,
                precision::position(vertex), {plane.x, plane.y, plane.z}))
        {
            continue;
        }

        if (dist < 0.0)
        {
            ++counts.negative;
        }
        else
        {
            ++counts.positive;
        }
    }

    return counts;
}

std::optional<std::vector<TrackedSectorFragment>> clipTrackedSectorFamilyByPlanes(
    Level& level,
    std::vector<TrackedSectorFragment> sectorFamily,
    const std::vector<Plane>& planes,
    CleaveProfileContext& context)
{
    sectorFamily = validUniqueTrackedSectorFragments(level, std::move(sectorFamily));

    // Step 1: each plane must be applied to every fragment produced by earlier planes.
    for (const Plane& plane : planes)
    {
        const std::size_t sectorsAtPlaneStart = sectorFamily.size();
        for (std::size_t i = 0; i < sectorsAtPlaneStart; ++i)
        {
            const std::int32_t sectorIndex = sectorFamily[i].sector;
            if (!isValidSectorIndex(level, sectorIndex))
            {
                continue;
            }

            const SectorPlaneSideCounts sideCounts =
                countSectorSidesForPlane(level.sectors[static_cast<std::size_t>(sectorIndex)], plane);
            if (sideCounts.negative == 0)
            {
                // Already fully on the target-interior side of this plane.
                continue;
            }
            if (sideCounts.positive == 0)
            {
                // Outside this plane; this fragment can never be inside the target volume.
                sectorFamily[i].insideAllPlanes = false;
                continue;
            }

            const std::int32_t firstCreatedSector = static_cast<std::int32_t>(level.sectors.size());
            const bool parentInsideAllPlanes = sectorFamily[i].insideAllPlanes;
            if (!levelCleaveSectorImpl(
                    level,
                    sectorIndex,
                    plane.normal,
                    plane.x,
                    plane.y,
                    plane.z,
                    true, &context))
            {
                return std::nullopt;
            }

            // levelCleaveSector may exchange the two bodies to preserve the
            // original sector identity. Track the geometrically positive
            // half rather than assuming it remained in sectorIndex.
            sectorFamily[i].insideAllPlanes = parentInsideAllPlanes
                && sectorIsOnPositivePlaneSide(
                    level.sectors[static_cast<std::size_t>(sectorIndex)],
                    plane.normal, plane.x, plane.y, plane.z);

            // Step 2: newly split pieces are not re-cleaved by the same plane, but they must
            // participate in every subsequent plane so rotated/complex cuts do not leave remnants.
            for (std::int32_t created = firstCreatedSector; created < static_cast<std::int32_t>(level.sectors.size()); ++created)
            {
                sectorFamily.push_back(TrackedSectorFragment{
                    created,
                    parentInsideAllPlanes
                        && sectorIsOnPositivePlaneSide(
                            level.sectors[static_cast<std::size_t>(created)],
                            plane.normal, plane.x, plane.y, plane.z),
                });
            }
        }

        sectorFamily = validUniqueTrackedSectorFragments(level, std::move(sectorFamily));
    }

    return sectorFamily;
}

std::int32_t remapSectorAfterDeletes(std::int32_t sectorIndex, const std::vector<std::int32_t>& deletedAscending) noexcept
{
    return detail::remapSectorAfterDeletes(sectorIndex, deletedAscending);
}

std::vector<std::int32_t> remapSectorsAfterDeletes(
    const std::vector<std::int32_t>& sectors,
    const std::vector<std::int32_t>& deletedAscending)
{
    std::vector<std::int32_t> remapped{};
    remapped.reserve(sectors.size());

    for (const std::int32_t sectorIndex : sectors)
    {
        const std::int32_t mapped = remapSectorAfterDeletes(sectorIndex, deletedAscending);
        if (mapped >= 0)
        {
            remapped.push_back(mapped);
        }
    }

    std::sort(remapped.begin(), remapped.end());
    remapped.erase(std::unique(remapped.begin(), remapped.end()), remapped.end());
    return remapped;
}

bool deleteSectorsDescending(Level& level, std::vector<std::int32_t> sectors) noexcept
{
    std::sort(sectors.begin(), sectors.end());
    sectors.erase(std::unique(sectors.begin(), sectors.end()), sectors.end());

    for (auto it = sectors.rbegin(); it != sectors.rend(); ++it)
    {
        if (!deleteSectorGeometry(level, *it))
        {
            return false;
        }
    }

    return true;
}

void insertVertexAt(
    Surface& surface,
    const std::size_t pos,
    const std::int32_t vertexIndex,
    const TexVertex& texVertex)
{
    const std::size_t insertPos = std::min(pos, surface.vertices.size());
    surface.vertices.insert(surface.vertices.begin() + static_cast<std::ptrdiff_t>(insertPos), vertexIndex);

    const std::size_t insertTvPos = std::min(insertPos, surface.texVertices.size());
    surface.texVertices.insert(
        surface.texVertices.begin() + static_cast<std::ptrdiff_t>(insertTvPos),
        texVertex);
}

void insertVertexAfter(
    Surface& surface,
    const std::size_t vertexIndex,
    const std::int32_t insert,
    const TexVertex& texVertex)
{
    const std::size_t count = surface.vertices.size();
    if ( count == 0 )
    {
        return;
    }

    std::size_t insertPos = (vertexIndex + 1) % count;
    if ( insertPos == 0 )
    {
        insertPos = count;
    }

    insertVertexAt(surface, insertPos, insert, texVertex);
}

void removePlaneEdges(Sector& sector, Surface& surface, const TVector& rvec, const bool removeIfPositive) noexcept
{
    if (surface.vertices.size() != surface.texVertices.size())
    {
        return;
    }

    const std::int32_t count = static_cast<std::int32_t>(surface.vertices.size());
    if (count < 3)
    {
        return;
    }

    for (std::int32_t i = count - 1; i >= 0; --i)
    {
        const std::int32_t prev = prevIndex(i, count);
        const std::int32_t next = nextIndex(i, count);

        const std::int32_t v0Index = surface.vertices[static_cast<std::size_t>(prev)];
        const std::int32_t v1Index = surface.vertices[static_cast<std::size_t>(i)];
        const std::int32_t v2Index = surface.vertices[static_cast<std::size_t>(next)];

        if (v0Index < 0 || v1Index < 0 || v2Index < 0)
        {
            continue;
        }
        if (static_cast<std::size_t>(v0Index) >= sector.vertices.size()
            || static_cast<std::size_t>(v1Index) >= sector.vertices.size()
            || static_cast<std::size_t>(v2Index) >= sector.vertices.size())
        {
            continue;
        }

        const Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];
        const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1Index)];
        const Vertex& v2 = sector.vertices[static_cast<std::size_t>(v2Index)];

        if (v0.mark != 0 || v1.mark != 0 || v2.mark != 0)
        {
            continue;
        }

        const double dotNext = rvec.dot(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z);
        const double dotPrev = rvec.dot(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);

        const bool shouldRemove = removeIfPositive ? (dotNext > 0.0 || dotPrev > 0.0) : (dotNext < 0.0 || dotPrev < 0.0);
        if (!shouldRemove)
        {
            continue;
        }

        surface.vertices.erase(surface.vertices.begin() + static_cast<std::ptrdiff_t>(i));
        surface.texVertices.erase(surface.texVertices.begin() + static_cast<std::ptrdiff_t>(i));
    }
}

bool insertCoordinateEdgePointIntoSector(
    Level& level,
    const std::int32_t sectorIndex,
    const Vertex& v1,
    const Vertex& v2,
    const Vertex& vx,
    const double sourceAdmissibilityLimit = 0.0,
    const Sector* const sourceSector = nullptr,
    const Surface* const sourceSurface = nullptr,
    const CutPlaneDescriptor* const cutPlane = nullptr,
    const CleaveFaceProfile* const sourceProfile = nullptr,
    const CleaveFaceProfile* const targetProfile = nullptr,
    const std::int32_t targetSurfaceIndex = -1)
{
    if ( sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size() )
    {
        return false;
    }

    Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    if ( targetSurfaceIndex >= 0
        && static_cast<std::size_t>(targetSurfaceIndex) >= sector.surfaces.size() )
    {
        return false;
    }
    // The inputs may refer to this sector. Snapshot them before the first
    // insertion can reallocate its vertex pool.
    const Vertex sourceFirst = v1;
    const Vertex sourceSecond = v2;
    const Vertex inserted = vx;
    const bool hasSourceContext = sourceSector != nullptr && sourceSurface != nullptr;
    const auto sourceSupport = hasSourceContext
        ? sourceRoundedEdgeSupportForCoordinates(
            *sourceSector, *sourceSurface, sourceFirst, sourceSecond)
        : std::optional<CoordinateEdgeSupport>{};
    // Keep the requested edge as the primary source line. A rounded
    // removal-run chord is considered only when no target surface contains
    // that direct edge; otherwise an adjacent portal wall could consume the
    // broader chord instead of the native subedge being propagated.
    std::int32_t newVertex = -1;
    bool foundPoint = false;

    // First certify a complete portal chain. Only its native indexed edges
    // may receive the propagated point; a nearby wall with merely similar
    // coordinates is never selected. The returned residual is the measured
    // immutable source discrepancy, rather than the larger admission budget.
    std::vector<std::pair<std::int32_t, std::int32_t>> certifiedEdges;
    double certifiedDeviation = 0.0;
    bool usingRoundedSourceSupport = false;
    Vertex targetLineStart = sourceFirst;
    Vertex targetLineEnd = sourceSecond;

    const auto collectCertifiedEdges = [&](
        const Vertex& lineStart,
        const Vertex& lineEnd,
        const double sourceLimit)
    {
        certifiedEdges.clear();
        certifiedDeviation = 0.0;
        for ( std::size_t candidateIndex = 0;
            candidateIndex < sector.surfaces.size(); ++candidateIndex )
        {
            if ( targetSurfaceIndex >= 0
                && static_cast<std::int32_t>(candidateIndex) != targetSurfaceIndex )
            {
                continue;
            }
            const Surface& candidate = sector.surfaces[candidateIndex];
            const auto candidateLimit = coordinateBoundaryDeviationLimit(sector, candidate);
            if ( !candidateLimit )
            {
                continue;
            }
            const double admissibilityLimit = hasSourceContext
                ? *candidateLimit
                : (sourceLimit > 0.0
                    ? std::min(sourceLimit, *candidateLimit) : 0.0);
            const auto certificate = coordinateEdgeChainCertificate(
                sector, candidate, lineStart, lineEnd, admissibilityLimit);
            if ( !certificate )
            {
                continue;
            }
            if ( targetSurfaceIndex >= 0
                && !coordinateEdgeChainHasExactEndpoints(
                    sector, candidate, lineStart, lineEnd) )
            {
                continue;
            }
            certifiedDeviation = std::max(
                certifiedDeviation, certificate->maximumDeviation);
            for ( const auto edge : certificate->edges )
            {
                const bool alreadyRecorded = std::any_of(
                    certifiedEdges.begin(), certifiedEdges.end(),
                    [edge](const auto existing)
                    {
                        return (existing.first == edge.first && existing.second == edge.second)
                            || (existing.first == edge.second && existing.second == edge.first);
                    });
                if ( !alreadyRecorded )
                {
                    certifiedEdges.push_back(edge);
                }
            }
        }
    };

    collectCertifiedEdges(sourceFirst, sourceSecond, sourceAdmissibilityLimit);
    if ( certifiedEdges.empty() && sourceSupport )
    {
        usingRoundedSourceSupport = true;
        targetLineStart = sourceSupport->first;
        targetLineEnd = sourceSupport->second;
        collectCertifiedEdges(
            targetLineStart, targetLineEnd, sourceSupport->deviation);
    }
    if ( certifiedEdges.empty()
        && sourceProfile != nullptr && targetProfile != nullptr
        && targetSurfaceIndex >= 0
        && static_cast<std::size_t>(targetSurfaceIndex) < sector.surfaces.size() )
    {
        const Surface& targetSurface = sector.surfaces[static_cast<std::size_t>(targetSurfaceIndex)];
        bool pairUsesRoundedSourceSupport = false;
        auto pairCertificate = nativePairMappedTargetEdge(
            sourceFirst, sourceSecond, sector, targetSurface,
            *sourceProfile, *targetProfile);
        if ( !pairCertificate && sourceSupport )
        {
            usingRoundedSourceSupport = true;
            targetLineStart = sourceSupport->first;
            targetLineEnd = sourceSupport->second;
            pairCertificate = nativePairMappedTargetEdge(
                targetLineStart, targetLineEnd, sector, targetSurface,
                *sourceProfile, *targetProfile);
            pairUsesRoundedSourceSupport = pairCertificate.has_value();
        }
        if ( pairCertificate )
        {
            if ( !pairUsesRoundedSourceSupport )
            {
                usingRoundedSourceSupport = false;
                targetLineStart = sourceFirst;
                targetLineEnd = sourceSecond;
            }
            certifiedEdges = pairCertificate->edges;
            certifiedDeviation = pairCertificate->maximumDeviation;
            const auto pair = sourceNativePairMatch(*sourceProfile, *targetProfile);
            if ( pair )
            {
                certifiedDeviation = std::max(certifiedDeviation, pair->delta);
            }
        }
    }
    if ( certifiedEdges.empty() )
    {
        return false;
    }
    double sourceParameter{};
    const double propagationDeviation = std::max(
        certifiedDeviation,
        usingRoundedSourceSupport && sourceSupport
            ? sourceSupport->deviation : 0.0);
    if ( !coordinateLineParameter(
            inserted, targetLineStart, targetLineEnd, sourceParameter, propagationDeviation)
        || sourceParameter < 0.0 || sourceParameter > 1.0 )
    {
        // A generated point may spend the measured immutable chain residual,
        // but it cannot spend that residual twice by being close to a rounded
        // segment on the opposite side of the source line.
        return false;
    }

    struct NativeTargetHit
    {
        TVector point{};
        std::int32_t endpoint{-1};
        bool exactEndpoint{};
    };
    std::optional<NativeTargetHit> nativeHit;
    if ( cutPlane != nullptr )
    {
        for ( const auto edge : certifiedEdges )
        {
            if ( edge.first < 0 || edge.second < 0
                || static_cast<std::size_t>(edge.first) >= sector.vertices.size()
                || static_cast<std::size_t>(edge.second) >= sector.vertices.size() )
            {
                return false;
            }
            const std::int32_t firstIndex = std::min(edge.first, edge.second);
            const std::int32_t secondIndex = std::max(edge.first, edge.second);
            const Vertex& first = sector.vertices[static_cast<std::size_t>(firstIndex)];
            const Vertex& second = sector.vertices[static_cast<std::size_t>(secondIndex)];
            const std::int32_t sideFirst = classifyPlaneSide(
                first, cutPlane->normal, cutPlane->anchor.x,
                cutPlane->anchor.y, cutPlane->anchor.z);
            const std::int32_t sideSecond = classifyPlaneSide(
                second, cutPlane->normal, cutPlane->anchor.x,
                cutPlane->anchor.y, cutPlane->anchor.z);
            if ( sideFirst == 2 || sideSecond == 2
                || (sideFirst == 0 && sideSecond == 0) )
            {
                // A whole target edge on the cut plane has no unique native
                // intersection. Never choose one by face or vertex order.
                return false;
            }

            NativeTargetHit candidate{};
            const bool firstMatchesSource = first.x == sourceFirst.x
                && first.y == sourceFirst.y
                && first.z == sourceFirst.z;
            const bool secondMatchesSource = second.x == sourceSecond.x
                && second.y == sourceSecond.y
                && second.z == sourceSecond.z;
            const bool firstMatchesSecondSource = first.x == sourceSecond.x
                && first.y == sourceSecond.y
                && first.z == sourceSecond.z;
            const bool secondMatchesFirstSource = second.x == sourceFirst.x
                && second.y == sourceFirst.y
                && second.z == sourceFirst.z;
            const bool exactSourceEdge = (firstMatchesSource && secondMatchesSource)
                || (firstMatchesSecondSource && secondMatchesFirstSource);
            if ( exactSourceEdge )
            {
                const Vertex sourcePoint = inserted;
                const std::int32_t sourceSide = classifyPlaneSide(
                    sourcePoint, cutPlane->normal, cutPlane->anchor.x,
                    cutPlane->anchor.y, cutPlane->anchor.z);
                if ( sourceSide != 0 )
                {
                    return false;
                }
                candidate.point = precision::position(sourcePoint);
                if ( sourcePoint.x == first.x && sourcePoint.y == first.y
                    && sourcePoint.z == first.z )
                {
                    candidate.endpoint = firstIndex;
                    candidate.exactEndpoint = true;
                }
                else if ( sourcePoint.x == second.x && sourcePoint.y == second.y
                    && sourcePoint.z == second.z )
                {
                    candidate.endpoint = secondIndex;
                    candidate.exactEndpoint = true;
                }
            }
            else if ( sideFirst == 0 )
            {
                candidate.point = precision::position(first);
                candidate.endpoint = firstIndex;
                candidate.exactEndpoint = true;
            }
            else if ( sideSecond == 0 )
            {
                candidate.point = precision::position(second);
                candidate.endpoint = secondIndex;
                candidate.exactEndpoint = true;
            }
            else if ( sideFirst == -sideSecond )
            {
                double x = 0.0;
                double y = 0.0;
                double z = 0.0;
                if ( !planeLineIntersection(
                        cutPlane->normal,
                        cutPlane->anchor.x, cutPlane->anchor.y, cutPlane->anchor.z,
                        first.x, first.y, first.z,
                        second.x, second.y, second.z,
                        x, y, z) )
                {
                    return false;
                }
                candidate.point = {x, y, z};
            }
            else
            {
                continue;
            }
            if ( !precision::finite(candidate.point) )
            {
                return false;
            }

            if ( !nativeHit )
            {
                nativeHit = candidate;
                continue;
            }
            if ( !precision::samePoint(nativeHit->point, candidate.point) )
            {
                // A certified chain with two distinct plane hits is
                // ambiguous; do not publish a point on an arbitrary segment.
                return false;
            }
            if ( nativeHit->exactEndpoint && candidate.exactEndpoint
                && nativeHit->endpoint != candidate.endpoint )
            {
                // Two authored endpoint identities at the same place are
                // not a reason to weld them during propagation.
                return false;
            }
            if ( candidate.exactEndpoint && !nativeHit->exactEndpoint )
            {
                nativeHit = candidate;
            }
        }
        if ( !nativeHit )
        {
            return false;
        }
        double targetParameter{};
        if ( !coordinateLineParameter(
                Vertex{nativeHit->point.x, nativeHit->point.y, nativeHit->point.z, 0},
                targetLineStart, targetLineEnd,
                targetParameter, propagationDeviation)
            || targetParameter < 0.0 || targetParameter > 1.0 )
        {
            return false;
        }
    }

    const Vertex nativeInserted = nativeHit
        ? Vertex{nativeHit->point.x, nativeHit->point.y, nativeHit->point.z, 0}
        : inserted;
    if ( nativeHit )
    {
        newVertex = nativeHit->endpoint;
    }

    for ( Surface& surface : sector.surfaces )
    {
        if ( surface.vertices.size() < 2 )
        {
            continue;
        }

        const bool usesCertifiedEdge = std::any_of(
            certifiedEdges.begin(), certifiedEdges.end(),
            [&surface](const auto edge)
            {
                return surfaceContainsIndexedEdge(surface, edge.first, edge.second);
            });
        if ( !usesCertifiedEdge )
        {
            continue;
        }
        if ( !surfaceHasValidTextureTopology(sector, surface) )
        {
            if ( targetSurfaceIndex >= 0 )
            {
                return false;
            }
            continue;
        }

        bool changed = false;

        for ( std::int32_t i = static_cast<std::int32_t>(surface.vertices.size()) - 1; i >= 0; --i )
        {
            const std::size_t cur = static_cast<std::size_t>(i);
            const std::size_t next = (cur + 1) % surface.vertices.size();

            const std::int32_t vi1 = surface.vertices[cur];
            const std::int32_t vi2 = surface.vertices[next];

            if ( vi1 < 0 || vi2 < 0
                || static_cast<std::size_t>(vi1) >= sector.vertices.size()
                || static_cast<std::size_t>(vi2) >= sector.vertices.size() )
            {
                continue;
            }
            const bool isCertifiedEdge = std::any_of(
                certifiedEdges.begin(), certifiedEdges.end(),
                [vi1, vi2](const auto edge)
                {
                    return (edge.first == vi1 && edge.second == vi2)
                        || (edge.first == vi2 && edge.second == vi1);
                });
            if ( !isCertifiedEdge )
            {
                continue;
            }

            // Copy these values before adding a vertex can reallocate the
            // sector's vertex vector. Select exactly one chain segment that
            // contains the requested point, preserving that segment's own
            // authored UV interpolation.
            const Vertex segmentFirst = sector.vertices[static_cast<std::size_t>(vi1)];
            const Vertex segmentSecond = sector.vertices[static_cast<std::size_t>(vi2)];
            double parameter{};
            bool onSegment = false;
            if ( nativeHit )
            {
                const bool exactFirst = nativeInserted.x == segmentFirst.x
                    && nativeInserted.y == segmentFirst.y
                    && nativeInserted.z == segmentFirst.z;
                const bool exactSecond = nativeInserted.x == segmentSecond.x
                    && nativeInserted.y == segmentSecond.y
                    && nativeInserted.z == segmentSecond.z;
                if ( exactFirst )
                {
                    newVertex = vi1;
                    foundPoint = true;
                    break;
                }
                if ( exactSecond )
                {
                    newVertex = vi2;
                    foundPoint = true;
                    break;
                }
                onSegment = precision::onSegment(
                    precision::position(nativeInserted),
                    precision::position(segmentFirst),
                    precision::position(segmentSecond),
                    parameter);
            }
            else if ( certifiedDeviation > 0.0 )
            {
                const double parameterError = coordinateEdgeParameterError(
                    segmentFirst, segmentSecond, certifiedDeviation);
                onSegment = std::isfinite(parameterError)
                    && coordinateLineParameter(
                        inserted, segmentFirst, segmentSecond, parameter, certifiedDeviation)
                    && parameter > parameterError
                    && parameter < 1.0 - parameterError;
            }
            else
            {
                onSegment = precision::onSegment(
                    precision::position(inserted),
                    precision::position(segmentFirst),
                    precision::position(segmentSecond),
                    parameter);
            }
            if ( !onSegment )
            {
                continue;
            }

            if ( !nativeHit
                && precision::samePoint(
                    precision::position(inserted), precision::position(segmentFirst)) )
            {
                newVertex = vi1;
                foundPoint = true;
                break;
            }
            if ( !nativeHit
                && precision::samePoint(
                    precision::position(inserted), precision::position(segmentSecond)) )
            {
                newVertex = vi2;
                foundPoint = true;
                break;
            }
            if ( !(parameter > 0.0 && parameter < 1.0) )
            {
                continue;
            }

            if ( newVertex < 0 )
            {
                newVertex = precision::addVertex(
                    sector, nativeInserted.x, nativeInserted.y, nativeInserted.z);
            }
            const TexVertex texVertex = texVertexForInsertedEdgePoint(
                sector,
                surface,
                cur,
                next,
                nativeInserted);
            insertVertexAfter(surface, cur, newVertex, texVertex);
            changed = true;
            foundPoint = true;
            break;
        }

        if ( changed )
        {
            surface.normal = surfaceCalcNormalNewell(sector, surface);
        }
    }
    return foundPoint && newVertex >= 0;
}

bool insertVxtIntoSector(
    Level& level,
    const SurfaceRef surfRef,
    const Vertex& v1,
    const Vertex& v2,
    const Vertex& vx,
    const double sourceAdmissibilityLimit = 0.0,
    const Sector* const sourceSector = nullptr,
    const Surface* const sourceSurface = nullptr,
    const CutPlaneDescriptor* const cutPlane = nullptr,
    const CleaveFaceProfile* const sourceProfile = nullptr,
    const CleaveFaceProfile* const targetProfile = nullptr)
{
    if ( !isValidSurfaceRef(level, surfRef) )
    {
        return false;
    }
    return insertCoordinateEdgePointIntoSector(
        level, surfRef.sector, v1, v2, vx, sourceAdmissibilityLimit,
        sourceSector, sourceSurface, cutPlane,
        sourceProfile, targetProfile, surfRef.surface);
}

bool insertCutVertexIntoMatchingEdges(
    Level& level,
    Sector& sector,
    Surface& surface,
    const std::int32_t edgeA,
    const std::int32_t edgeB,
    const Vertex& v1,
    const Vertex& v2,
    const Vertex& vx,
    const std::int32_t cutVertex,
    const double sourceAdmissibilityLimit = 0.0,
    const Sector* const sourceSector = nullptr,
    const Surface* const sourceSurface = nullptr,
    const CutPlaneDescriptor* const cutPlane = nullptr,
    const CleaveProfileContext* const context = nullptr,
    const SurfaceRef sourceSurfaceRef = {})
{
    if ( surface.vertices.size() < 2 )
    {
        return true;
    }

    if ( !surfaceHasTextureCorners(surface) )
    {
        return true;
    }
    bool changed = false;
    bool propagationFailed = false;

    // Step 1: find all matching orientations of the original edge on this surface.
    for (std::int32_t j = static_cast<std::int32_t>(surface.vertices.size()) - 1; j >= 0; --j)
    {
        const std::size_t cur = static_cast<std::size_t>(j);
        const std::size_t n = (cur + 1) % surface.vertices.size();

        const std::int32_t e1 = surface.vertices[cur];
        const std::int32_t e2 = surface.vertices[n];

        if ( (e1 != edgeA || e2 != edgeB) && (e1 != edgeB || e2 != edgeA) )
        {
            continue;
        }

        // Step 2: insert the shared cut vertex and mirror the edit through any existing adjoin.
        const TexVertex texVertex = texVertexForInsertedEdgePoint(
            sector,
            surface,
            cur,
            n,
            vx);
        insertVertexAfter(surface, cur, cutVertex, texVertex);
        if ( surface.adjoin )
        {
            const SurfaceRef targetSurfaceRef = *surface.adjoin;
            const Sector* propagationSourceSector = sourceSector;
            const Surface* propagationSourceSurface = sourceSurface;
            const CleaveFaceProfile* sourceProfile = nullptr;
            const CleaveFaceProfile* targetProfile = nullptr;
            if ( context != nullptr )
            {
                sourceProfile = context->sourceFaceProfile(sourceSurfaceRef);
                targetProfile = context->sourceFaceProfile(targetSurfaceRef);
                if ( sourceProfile != nullptr && !sourceProfile->geometry.surfaces.empty() )
                {
                    propagationSourceSector = &sourceProfile->geometry;
                    propagationSourceSurface = &sourceProfile->geometry.surfaces.front();
                }
            }
            if ( !insertVxtIntoSector(
                level, targetSurfaceRef, v1, v2, vx, sourceAdmissibilityLimit,
                propagationSourceSector, propagationSourceSurface, cutPlane,
                sourceProfile, targetProfile) )
            {
                propagationFailed = true;
            }
        }

        changed = true;
    }

    // Step 3: only the inserted corner is new authored data; retain every
    // existing corner byte-for-byte and refresh derived geometry.
    if ( changed )
    {
        surface.normal = surfaceCalcNormalNewell(sector, surface);
    }
    return !propagationFailed;
}

bool propagateCutVerticesToRelatedSurfaces(
    Level& level,
    const SurfaceRef originalSurfaceRef,
    Sector& sector,
    const std::vector<std::int32_t>& originalVertices,
    const std::vector<std::int32_t>& cutVertices,
    const double sourceAdmissibilityLimit,
    const Sector* const sourceSector = nullptr,
    const Surface* const sourceSurface = nullptr,
    const CutPlaneDescriptor* const cutPlane = nullptr,
    const CleaveProfileContext* const context = nullptr)
{
    // Step 1: for each generated cut vertex, find every same-sector surface that uses the cut edge.
    for (std::size_t i = 0; i < originalVertices.size(); ++i)
    {
        const std::int32_t cv = cutVertices[i];
        if (cv < 0)
        {
            continue;
        }

        const std::size_t next = (i + 1) % originalVertices.size();
        const std::int32_t v1Index = originalVertices[i];
        const std::int32_t v2Index = originalVertices[next];

        if (v1Index < 0 || v2Index < 0)
        {
            continue;
        }

        if (static_cast<std::size_t>(v1Index) >= sector.vertices.size()
            || static_cast<std::size_t>(v2Index) >= sector.vertices.size()
            || static_cast<std::size_t>(cv) >= sector.vertices.size())
        {
            continue;
        }

        const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1Index)];
        const Vertex& v2 = sector.vertices[static_cast<std::size_t>(v2Index)];
        const Vertex& vx = sector.vertices[static_cast<std::size_t>(cv)];

        // Step 2: skip the original surface because its two output contours were already rebuilt explicitly.
        for (std::size_t sfi = 0; sfi < sector.surfaces.size(); ++sfi)
        {
            if (sfi == static_cast<std::size_t>(originalSurfaceRef.surface))
            {
                continue;
            }

            if ( !insertCutVertexIntoMatchingEdges(
                level, sector, sector.surfaces[sfi], v1Index, v2Index,
                v1, v2, vx, cv, sourceAdmissibilityLimit,
                sourceSector, sourceSurface, cutPlane, context,
                SurfaceRef{originalSurfaceRef.sector, static_cast<std::int32_t>(sfi)}) )
            {
                return false;
            }
        }
    }
    return true;
}

bool insertVertexIntoMatchingSurfaceEdge(
    const Sector& sector,
    Surface& surface,
    const std::int32_t edgeA,
    const std::int32_t edgeB,
    const std::int32_t newVertex,
    const Vertex& inserted)
{
    if ( surface.vertices.size() < 2 )
    {
        return false;
    }

    if ( !surfaceHasTextureCorners(surface) )
    {
        return false;
    }
    bool changed = false;

    // Step 1: add the new vertex after every matching orientation of the target edge.
    for ( std::size_t i = 0; i < surface.vertices.size(); ++i )
    {
        const std::size_t next = (i + 1) % surface.vertices.size();
        const std::int32_t e1 = surface.vertices[i];
        const std::int32_t e2 = surface.vertices[next];

        if ( (e1 == edgeA && e2 == edgeB) || (e1 == edgeB && e2 == edgeA) )
        {
            const TexVertex texVertex = texVertexForInsertedEdgePoint(
                sector,
                surface,
                i,
                next,
                inserted);
            insertVertexAfter(surface, i, newVertex, texVertex);
            changed = true;
        }
    }

    return changed;
}

void recalcAndRenumberSectorSurfaces(Sector& sector) noexcept
{
    // Step 1: keep texture vertices aligned, preserve every authored corner,
    // and recalculate only geometry-derived surface data.
    for ( Surface& surface : sector.surfaces )
    {
        surface.normal = surfaceCalcNormalNewell(sector, surface);
    }

    // Step 2: restore canonical per-sector surface indices after topology edits.
    for ( std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex )
    {
        sector.surfaces[faceIndex].num = static_cast<std::int32_t>(faceIndex);
    }
}

std::optional<SurfaceRef> sysCleaveSurface(
    Level& level,
    const SurfaceRef surfaceRef,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz,
    CleaveProfileContext* const context,
    detail::ExtrudeArrangementContext* const arrangement = nullptr)
{
    if (!isValidSurfaceRef(level, surfaceRef))
    {
        return std::nullopt;
    }

    Sector& sector = level.sectors[static_cast<std::size_t>(surfaceRef.sector)];
    const std::size_t surfaceIndex = static_cast<std::size_t>(surfaceRef.surface);
    if (surfaceIndex >= sector.surfaces.size())
    {
        return std::nullopt;
    }

    if (sector.surfaces[surfaceIndex].vertices.size() < 3)
    {
        return std::nullopt;
    }

    // Step 1: require a complete authored corner array and snapshot the
    // pre-cleave topology. A malformed source is rejected without mutation.
    if ( !surfaceHasTextureCorners(sector.surfaces[surfaceIndex]) )
    {
        return std::nullopt;
    }

    const std::vector<std::int32_t> originalVertices = sector.surfaces[surfaceIndex].vertices;
    const std::vector<TexVertex> originalTexVertices = sector.surfaces[surfaceIndex].texVertices;
    // Keep the immutable source boundary available while propagation visits
    // descendant/adjoined surfaces. The live source surface is rewritten
    // below, so deriving a rounded-chain certificate from it would be order
    // dependent after the first split.
    const Surface sourceSurfaceSnapshot = sector.surfaces[surfaceIndex];
    const double sourceAdmissibilityLimit = coordinateBoundaryDeviationLimit(
        sector, sector.surfaces[surfaceIndex]).value_or(0.0);
    const CutPlaneDescriptor cutPlane{planeNormal, {px, py, pz}};

    // Step 2: create one cut vertex for each original edge that crosses the cleave plane.
    const std::vector<std::int32_t> cutVertices = buildSurfaceCutVertices(
        sector, originalVertices, planeNormal, {px, py, pz}, surfaceRef, arrangement);
    if ( std::find(cutVertices.begin(),cutVertices.end(),-2) != cutVertices.end() )
    {
        return std::nullopt;
    }

    // Step 3: reject planes that touch the polygon but do not leave vertices on both sides.
    const auto [nleft, nright] = countSurfaceSplitSides(sector, originalVertices);
    if (nleft == 0 || nright == 0)
    {
        return std::nullopt;
    }

    Surface newSurface{};
    assignSurfaceParams(sector.surfaces[surfaceIndex], newSurface);

    newSurface.vertices.reserve(originalVertices.size() + 2);
    newSurface.texVertices.reserve(originalVertices.size() + 2);

    std::vector<std::int32_t> leftVertices{};
    std::vector<TexVertex> leftTexVertices{};

    leftVertices.reserve(originalVertices.size() + 2);
    leftTexVertices.reserve(originalVertices.size() + 2);

    // Step 4: build the original-side and added-side contours from the snapshot and cut vertices.
    if ( !buildSplitSurfaceContours(
        sector,
        originalVertices,
        originalTexVertices,
        cutVertices,
        newSurface,
        leftVertices,
        leftTexVertices,
        surfaceRef,
        arrangement) )
    {
        return std::nullopt;
    }

    const std::int32_t newSurfaceIndex = sector.addSurface(std::move(newSurface));

    SurfaceRef newSurfaceRef{};
    newSurfaceRef.sector = surfaceRef.sector;
    newSurfaceRef.surface = newSurfaceIndex;

    Surface& surface = sector.surfaces[surfaceIndex];
    surface.vertices = leftVertices;
    surface.texVertices = leftTexVertices;

    Surface& addedSurface = sector.surfaces[static_cast<std::size_t>(newSurfaceIndex)];

    surface.normal = surfaceCalcNormalNewell(sector, surface);
    addedSurface.normal = surfaceCalcNormalNewell(sector, addedSurface);
    surfaceCheckIfFloor(addedSurface);

    const TVector rvec = surface.normal.cross(planeNormal);

    // Step 5: remove redundant edges that lie exactly on the cleave plane, then propagate shared cut vertices.
    if ( !arrangement )
    {
        removePlaneEdges(sector, surface, rvec, false);
        removePlaneEdges(sector, addedSurface, rvec, true);
    }

    if ( !propagateCutVerticesToRelatedSurfaces(
            level, surfaceRef, sector, originalVertices, cutVertices,
            sourceAdmissibilityLimit, &sector, &sourceSurfaceSnapshot, &cutPlane, context) )
    {
        return std::nullopt;
    }

    // Step 6: original corners retained their exact authored values and only
    // generated intersection corners were interpolated above.
    renumberSectorSurfaces(sector);

    if ( context )
    {
        context->splitFace(surfaceRef, newSurfaceRef);
    }
    if ( arrangement )
    {
        arrangement->splitFace(surfaceRef,newSurfaceRef);
        // Propagation can add this intersection to a neighboring source ear.
        // Its new tuple belongs to that ear's immutable original basis, not
        // an interpolation between previously rounded descendant tuples.
        for ( std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex )
        {
            const SurfaceRef ref{surfaceRef.sector,static_cast<std::int32_t>(faceIndex)};
            if ( !arrangement->tracksSource(ref) )
            {
                continue;
            }
            auto& face = sector.surfaces[faceIndex];
            for ( const auto index : cutVertices )
            {
                if ( index < 0 )
                {
                    continue;
                }
                for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                {
                    if ( face.vertices[corner] != index )
                    {
                        continue;
                    }
                    const auto tuple = arrangement->sourceTuple(ref,index);
                    if ( !tuple )
                    {
                        return std::nullopt;
                    }
                    face.texVertices[corner] = *tuple;
                }
            }
        }
    }
    return newSurfaceRef;
}

/**
 * Keeps portal/adjoin relationships valid when one side of an adjoined surface
 * is cleaved. The source surface has already been split; this function splits
 * the opposite surface with the same plane, clears stale portal links, and then
 * requires a unique complete pairing of the resulting overlapping surfaces.
 * Failure leaves rollback to the enclosing edit transaction.
 */
bool cleaveAdjoin(
    Level& level,
    const SurfaceRef surfaceRef,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz,
    const SurfaceRef newSurfaceRef,
    CleaveProfileContext* const context)
{
    // Step 1: validate both halves of the already-cleaved source surface.
    if (!isValidSurfaceRef(level, surfaceRef) || !isValidSurfaceRef(level, newSurfaceRef))
    {
        return false;
    }

    Surface& surface = level.sectors[static_cast<std::size_t>(surfaceRef.sector)].surfaces[static_cast<std::size_t>(surfaceRef.surface)];
    if (!surface.adjoin)
    {
        return true;
    }

    // Step 2: validate the existing reciprocal portal before clearing it.
    // A failed mirror operation must roll back, never become an unadjoin.
    const SurfaceRef adjoinRef = *surface.adjoin;
    if (!isValidSurfaceRef(level, adjoinRef))
    {
        return false;
    }

    Surface& asurf = level.sectors[static_cast<std::size_t>(adjoinRef.sector)].surfaces[static_cast<std::size_t>(adjoinRef.surface)];
    if ( asurf.adjoin != std::optional<SurfaceRef>{surfaceRef} )
    {
        return false;
    }

    // Step 3: disconnect both old portal ends before cleaving the opposite
    // surface. This prevents the old one-to-one portal from surviving while
    // surfaces are temporarily in a half-split state.
    surface.adjoin.reset();
    asurf.adjoin.reset();
    surface.nAdj = -1;
    asurf.nAdj = -1;

    // Step 4: apply the same plane to the opposite surface so both sectors have
    // compatible portal boundaries after the split.
    Sector& asec = level.sectors[static_cast<std::size_t>(adjoinRef.sector)];
    splitVertices(asec, asurf.vertices, planeNormal, px, py, pz);

    const std::optional<SurfaceRef> added = sysCleaveSurface(level, adjoinRef, planeNormal, px, py, pz, context);
    if ( !added )
    {
        return false;
    }

    const Surface& asurfAfter = asec.surfaces[static_cast<std::size_t>(adjoinRef.surface)];

    Sector& ssec = level.sectors[static_cast<std::size_t>(surfaceRef.sector)];
    const Surface& surf = ssec.surfaces[static_cast<std::size_t>(surfaceRef.surface)];
    const Surface& nsurf = ssec.surfaces[static_cast<std::size_t>(newSurfaceRef.surface)];

    // Step 5: prove one complete pairing before publishing either new portal.
    // Every caller still requires two complete pairs. Profiled edits prove
    // inherited geometry separately and match the exact ordered boundaries;
    // other callers retain the existing strict overlap predicate.
    const Sector& asec2 = level.sectors[static_cast<std::size_t>(added->sector)];
    const Surface& ansurf = asec2.surfaces[static_cast<std::size_t>(added->surface)];
    const auto matches = [context](
        const SurfaceRef firstRef, const Sector& first, const Surface& a,
        const SurfaceRef secondRef, const Sector& second, const Surface& b)
    {
        return context ? surfaceBoundariesMatchWithResolvedNormals(
                            first, a, second, b, true, context, firstRef, secondRef)
                       : precision::surfaceDoOverlapChecked(first, a, second, b);
    };
    const bool direct = matches(surfaceRef, ssec, surf, adjoinRef, asec, asurfAfter)
        && matches(newSurfaceRef, ssec, nsurf, *added, asec2, ansurf);
    const bool crossed = matches(surfaceRef, ssec, surf, *added, asec2, ansurf)
        && matches(newSurfaceRef, ssec, nsurf, adjoinRef, asec, asurfAfter);
    if ( direct == crossed )
    {
        return false;
    }
    const SurfaceRef originalMate = direct ? adjoinRef : *added;
    const SurfaceRef addedMate = direct ? *added : adjoinRef;
    return levelSysAdjoinSurfaces(level, surfaceRef, originalMate, false)
        && levelSysAdjoinSurfaces(level, newSurfaceRef, addedMate, false);
}

std::vector<std::int32_t> collectSectorVertexIndices(const Sector& sector)
{
    std::vector<std::int32_t> vertexIndices{};
    vertexIndices.reserve(sector.vertices.size());

    // Step 1: build a dense index list so splitVertices can mark every sector vertex.
    for (std::size_t i = 0; i < sector.vertices.size(); ++i)
    {
        vertexIndices.push_back(static_cast<std::int32_t>(i));
    }

    return vertexIndices;
}

std::pair<std::int32_t, std::int32_t> countMarkedSectorSides(const Sector& sector) noexcept
{
    std::int32_t nleft = 0;
    std::int32_t nright = 0;

    // Step 1: a sector split is valid only when at least one vertex is on each side of the plane.
    for (const Vertex& v : sector.vertices)
    {
        if (v.mark == -1)
        {
            ++nleft;
        }
        else if (v.mark == 1)
        {
            ++nright;
        }
    }

    return { nleft, nright };
}

bool splitSectorSurfacesByPlane(
    Level& level,
    const std::int32_t sectorIndex,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz,
    CleaveProfileContext* const context)
{
    Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];

    // Step 1: mark all sector vertices relative to the cleave plane.
    const std::vector<std::int32_t> vertexIndices = collectSectorVertexIndices(sector);
    splitVertices(sector, vertexIndices, planeNormal, px, py, pz);

    // Step 2: reject planes that do not actually divide the sector volume.
    const auto [nleft, nright] = countMarkedSectorSides(sector);
    if (nleft == 0 || nright == 0)
    {
        return false;
    }

    // Step 3: cleave each original surface once; newly added surfaces are handled as split outputs.
    const std::size_t initialSurfaceCount = sector.surfaces.size();
    for (std::size_t sf = 0; sf < initialSurfaceCount; ++sf)
    {
        const SurfaceRef surfRef{ sectorIndex, static_cast<std::int32_t>(sf) };
        const std::optional<SurfaceRef> added = sysCleaveSurface(level, surfRef, planeNormal, px, py, pz, context);
        if ( added && !cleaveAdjoin(level, surfRef, planeNormal, px, py, pz, *added, context) )
        {
            return false;
        }
    }

    return true;
}

void addUniquePlaneEdge(
    std::vector<std::pair<std::int32_t, std::int32_t>>& planeEdges,
    const std::int32_t a,
    const std::int32_t b)
{
    // Step 1: plane edges are undirected for cap construction, so ignore reversed duplicates.
    for (const auto& e : planeEdges)
    {
        if ((e.first == a && e.second == b) || (e.first == b && e.second == a))
        {
            return;
        }
    }

    planeEdges.emplace_back(a, b);
}

bool surfaceHasNonPlaneVertex(const Sector& sector, const Surface& surface) noexcept
{
    // Step 1: only surfaces with at least one off-plane vertex contribute cap boundary edges.
    for (const std::int32_t vi : surface.vertices)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
        {
            continue;
        }

        if (sector.vertices[static_cast<std::size_t>(vi)].mark != 0)
        {
            return true;
        }
    }

    return false;
}

std::vector<std::pair<std::int32_t, std::int32_t>> collectPlaneBoundaryEdges(const Sector& sector)
{
    std::vector<std::pair<std::int32_t, std::int32_t>> planeEdges{};

    // Step 1: scan only surfaces that still have vertices off the plane. Pure
    // plane-only surfaces are already caps/degenerates and would duplicate the
    // boundary loop we are trying to build.
    for (const Surface& surface : sector.surfaces)
    {
        if (!surfaceHasNonPlaneVertex(sector, surface))
        {
            continue;
        }

        const std::size_t vertexCount = surface.vertices.size();
        for (std::size_t i = 0; i < vertexCount; ++i)
        {
            const std::int32_t v1Index = surface.vertices[i];
            const std::int32_t v2Index = surface.vertices[(i + 1) % vertexCount];

            if (v1Index < 0 || v2Index < 0)
            {
                continue;
            }
            if (static_cast<std::size_t>(v1Index) >= sector.vertices.size()
                || static_cast<std::size_t>(v2Index) >= sector.vertices.size())
            {
                continue;
            }

            const Vertex& v1 = sector.vertices[static_cast<std::size_t>(v1Index)];
            const Vertex& v2 = sector.vertices[static_cast<std::size_t>(v2Index)];
            if (v1.mark == 0 && v2.mark == 0)
            {
                // Step 2: edges whose endpoints lie on the cleave plane form
                // the cap boundary. Store them as undirected edges so adjacent
                // surfaces do not add the same boundary twice.
                addUniquePlaneEdge(planeEdges, v1Index, v2Index);
            }
        }
    }

    return planeEdges;
}

std::vector<std::int32_t> orderPlaneBoundaryLoop(std::vector<std::pair<std::int32_t, std::int32_t>> planeEdges)
{
    if (planeEdges.empty())
    {
        return {};
    }

    std::vector<std::int32_t> ordered{};
    ordered.reserve(planeEdges.size());

    // Step 1: seed the loop from the first edge and consume connected edges until no continuation remains.
    ordered.push_back(planeEdges[0].first);
    ordered.push_back(planeEdges[0].second);

    std::int32_t current = planeEdges[0].second;
    planeEdges.erase(planeEdges.begin());

    while (true)
    {
        const std::size_t before = planeEdges.size();

        for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(planeEdges.size()) - 1; i >= 0; --i)
        {
            const auto& e = planeEdges[static_cast<std::size_t>(i)];

            if (e.first == current)
            {
                current = e.second;
                ordered.push_back(current);
                planeEdges.erase(planeEdges.begin() + i);
                break;
            }

            if (e.second == current)
            {
                current = e.first;
                ordered.push_back(current);
                planeEdges.erase(planeEdges.begin() + i);
                break;
            }
        }

        if (planeEdges.size() == before)
        {
            break;
        }
    }

    // Step 2: the cap polygon should not repeat the start vertex at the end.
    if (ordered.size() >= 2 && ordered.back() == ordered.front())
    {
        ordered.pop_back();
    }

    return ordered;
}

bool certifyCutBoundary(const Sector& splitSector, const std::vector<std::int32_t>& loop,
    const std::vector<std::pair<TVector, TVector>>& sourceEdges,
    const TVector& normal, const TVector& anchor) noexcept
{
    if ( loop.size() < 3 )
    {
        return false;
    }
    for ( std::size_t corner = 0; corner < loop.size(); ++corner )
    {
        const auto first = loop[corner];
        const auto next = loop[(corner + 1) % loop.size()];
        if ( first < 0 || next < 0 || static_cast<std::size_t>(first) >= splitSector.vertices.size()
            || static_cast<std::size_t>(next) >= splitSector.vertices.size() )
        {
            return false;
        }
        const TVector point = precision::position(splitSector.vertices[static_cast<std::size_t>(first)]);
        const double distance = normal.dot(point - anchor);
        if ( !std::isfinite(distance) || std::abs(distance) > precision::planeError(normal, point, anchor) )
        {
            return false;
        }
        bool onSourceEdge = false;
        for ( const auto& [a, b] : sourceEdges )
        {
            double amount{};
            if ( precision::onSegment(point, a, b, amount) )
            {
                onSourceEdge = true;
                break;
            }
        }
        if ( !onSourceEdge )
        {
            return false;
        }
        std::size_t forward = 0;
        std::size_t backward = 0;
        for ( const Surface& face : splitSector.surfaces )
        {
            for ( std::size_t edge = 0; edge < face.vertices.size(); ++edge )
            {
                const auto a = face.vertices[edge];
                const auto b = face.vertices[(edge + 1) % face.vertices.size()];
                forward += a == first && b == next;
                backward += a == next && b == first;
            }
        }
        if ( forward != 1 || backward != 1 )
        {
            return false;
        }
    }
    return true;
}

void splitSectorVertexPools(
    const std::vector<Vertex>& oldVertices,
    Sector& sec0,
    Sector& sec1,
    std::vector<std::int32_t>& map0,
    std::vector<std::int32_t>& map1)
{
    std::vector<Vertex> vertices0{};
    vertices0.reserve(oldVertices.size());

    std::vector<Vertex> vertices1{};
    vertices1.reserve(oldVertices.size());

    // Step 1: partition the original vertex pool according to split marks.
    // Negative-marked vertices move to the new sector; plane/right vertices
    // stay in the original so existing sector identity remains as stable as
    // possible.
    for (std::size_t i = 0; i < oldVertices.size(); ++i)
    {
        const Vertex& v = oldVertices[i];
        if (v.mark == -1)
        {
            map1[i] = static_cast<std::int32_t>(vertices1.size());
            vertices1.push_back(v);
        }
        else
        {
            map0[i] = static_cast<std::int32_t>(vertices0.size());
            vertices0.push_back(v);
        }
    }

    // Step 2: replace both sector vertex arrays only after all old-to-new maps
    // have been filled. Surface remapping depends on those maps being complete.
    sec0.vertices = std::move(vertices0);
    sec1.vertices = std::move(vertices1);
}

bool surfaceMovesToNewSector(
    const Sector& oldSector,
    const Surface& surface,
    const TVector& planeNormal) noexcept
{
    // Step 1: surfaces with a non-plane vertex follow that vertex's side.
    for (const std::int32_t vi : surface.vertices)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= oldSector.vertices.size())
        {
            continue;
        }

        const std::int32_t mark = oldSector.vertices[static_cast<std::size_t>(vi)].mark;
        if (mark == -1)
        {
            return true;
        }
        if (mark == 1)
        {
            return false;
        }
    }

    // Step 2: fully on-plane surfaces use normal orientation to decide which side owns them.
    TVector normal = surface.normal;
    if (normal.length() == 0.0)
    {
        normal = surfaceCalcNormalNewell(oldSector, surface);
    }

    return planeNormal.dot(normal) < 0.0;
}

std::vector<bool> classifySplitSectorSurfaces(
    const std::vector<Vertex>& oldVertices,
    const std::vector<Surface>& oldSurfaces,
    const TVector& planeNormal)
{
    Sector oldSector{};
    oldSector.vertices = oldVertices;

    std::vector<bool> moveToSec1(oldSurfaces.size(), false);

    // Step 1: classify each old surface before remapping mutates vertex indices.
    for (std::size_t sf = 0; sf < oldSurfaces.size(); ++sf)
    {
        moveToSec1[sf] = surfaceMovesToNewSector(oldSector, oldSurfaces[sf], planeNormal);
    }

    return moveToSec1;
}

bool remapSurfaceVerticesExisting(Surface& surface, const std::vector<std::int32_t>& map)
{
    // Step 1: original-sector surfaces must map entirely through vertices that
    // were kept in that sector. A negative map entry means the surface was
    // classified into the wrong side or would need a duplicate, so fail closed.
    for (std::int32_t& vi : surface.vertices)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= map.size())
        {
            return false;
        }

        const std::int32_t mapped = map[static_cast<std::size_t>(vi)];
        if (mapped < 0)
        {
            return false;
        }
        vi = mapped;
    }

    return true;
}

bool remapSurfaceVerticesCreating(
    Surface& surface,
    std::vector<std::int32_t>& map,
    const std::vector<Vertex>& oldVertices,
    Sector& dst)
{
    // Step 1: new-sector surfaces may need a duplicate of a plane vertex that only existed in the original pool.
    for (std::int32_t& vi : surface.vertices)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= map.size())
        {
            return false;
        }

        std::int32_t mapped = map[static_cast<std::size_t>(vi)];
        if (mapped < 0)
        {
            const Vertex& v = oldVertices[static_cast<std::size_t>(vi)];
            mapped = precision::addVertex(dst, v.x, v.y, v.z);
            map[static_cast<std::size_t>(vi)] = mapped;
        }
        vi = mapped;
    }

    return true;
}

bool assignSplitSectorSurfaces(
    Sector& sec0,
    Sector& sec1,
    const std::vector<Surface>& oldSurfaces,
    const std::vector<Vertex>& oldVertices,
    const std::vector<bool>& moveToSec1,
    std::vector<std::int32_t>& map0,
    std::vector<std::int32_t>& map1,
    std::vector<std::optional<SurfaceRef>>& surfaceRemap,
    const std::int32_t sectorIndex,
    const std::int32_t newSectorIndex)
{
    std::vector<Surface> surfaces0{};
    surfaces0.reserve(oldSurfaces.size());

    // Step 1: rebuild the original sector surfaces in original order.
    for (std::size_t sf = 0; sf < oldSurfaces.size(); ++sf)
    {
        if (moveToSec1[sf])
        {
            continue;
        }

        Surface surface = oldSurfaces[sf];
        if (!remapSurfaceVerticesExisting(surface, map0))
        {
            return false;
        }

        const std::int32_t newIndex = static_cast<std::int32_t>(surfaces0.size());
        surface.num = newIndex;
        surfaces0.push_back(std::move(surface));
        surfaceRemap[sf] = SurfaceRef{ sectorIndex, newIndex };
    }

    sec0.surfaces = std::move(surfaces0);

    std::vector<Surface> surfaces1{};
    surfaces1.reserve(oldSurfaces.size());

    // Step 2: rebuild moved surfaces in reverse order, matching legacy index behavior.
    for (std::ptrdiff_t sf = static_cast<std::ptrdiff_t>(oldSurfaces.size()) - 1; sf >= 0; --sf)
    {
        const std::size_t idx = static_cast<std::size_t>(sf);
        if (!moveToSec1[idx])
        {
            continue;
        }

        Surface surface = oldSurfaces[idx];
        if (!remapSurfaceVerticesCreating(surface, map1, oldVertices, sec1))
        {
            return false;
        }

        const std::int32_t newIndex = static_cast<std::int32_t>(surfaces1.size());
        surface.num = newIndex;
        surfaces1.push_back(std::move(surface));
        surfaceRemap[idx] = SurfaceRef{ newSectorIndex, newIndex };
    }

    sec1.surfaces = std::move(surfaces1);
    return true;
}

void rewriteAdjoinsAfterSectorSplit(
    Level& level,
    const std::int32_t sectorIndex,
    const std::vector<std::optional<SurfaceRef>>& surfaceRemap) noexcept
{
    // Step 1: every adjoin targeting the old sector must be redirected to that
    // old surface's post-split destination. The destination may be in either
    // the original sector or the newly created sector.
    for (Sector& sector : level.sectors)
    {
        for (Surface& surface : sector.surfaces)
        {
            if (!surface.adjoin || surface.adjoin->sector != sectorIndex)
            {
                continue;
            }

            const std::int32_t oldSurface = surface.adjoin->surface;
            if (oldSurface < 0 || static_cast<std::size_t>(oldSurface) >= surfaceRemap.size()
                || !surfaceRemap[static_cast<std::size_t>(oldSurface)])
            {
                // Step 2: missing remap data means the target no longer exists
                // as a valid portal surface, so remove the adjoin instead of
                // leaving a dangling reference.
                surface.adjoin.reset();
                surface.nAdj = -1;
                continue;
            }

            surface.adjoin = *surfaceRemap[static_cast<std::size_t>(oldSurface)];
        }
    }
}

bool buildAndAdjoinSplitCaps(
    Level& level,
    Sector& sec0,
    Sector& sec1,
    const std::int32_t sectorIndex,
    const std::int32_t newSectorIndex,
    const TVector& planeNormal,
    const std::vector<std::int32_t>& capLoop,
    const std::vector<Vertex>& oldVertices,
    std::vector<std::int32_t>& map0,
    std::vector<std::int32_t>& map1)
{
    Surface capSec{};
    Surface capSec1{};

    // Step 1: inherit surface display/collision parameters from an existing original-sector surface when possible.
    if (!sec0.surfaces.empty())
    {
        assignSurfaceParams(sec0.surfaces[0], capSec);
        assignSurfaceParams(sec0.surfaces[0], capSec1);
    }

    Surface capRef{};
    capRef.vertices = capLoop;

    Sector tempSec{};
    tempSec.vertices = oldVertices;

    const TVector capNormal = surfaceCalcNormalNewell(tempSec, capRef);
    const bool normalMatches = planeNormal.dot(capNormal) > 0.0;

    std::vector<std::int32_t> order = capLoop;
    std::vector<std::int32_t> reverse = capLoop;
    std::reverse(reverse.begin(), reverse.end());

    const std::vector<std::int32_t>& secOrder = normalMatches ? order : reverse;
    const std::vector<std::int32_t>& sec1Order = normalMatches ? reverse : order;

    capSec.vertices.reserve(secOrder.size());
    capSec.texVertices = std::vector<TexVertex>(secOrder.size());

    // Step 2: map cap vertices into the original sector.
    for (const std::int32_t vi : secOrder)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= map0.size())
        {
            return false;
        }

        const std::int32_t mapped = map0[static_cast<std::size_t>(vi)];
        if (mapped < 0)
        {
            return false;
        }

        capSec.vertices.push_back(mapped);
    }

    capSec1.vertices.reserve(sec1Order.size());
    capSec1.texVertices = std::vector<TexVertex>(sec1Order.size());

    // Step 3: map cap vertices into the new sector, duplicating plane vertices there when needed.
    for (const std::int32_t vi : sec1Order)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= map1.size())
        {
            return false;
        }

        std::int32_t mapped = map1[static_cast<std::size_t>(vi)];
        if (mapped < 0)
        {
            const Vertex& v = oldVertices[static_cast<std::size_t>(vi)];
            mapped = precision::addVertex(sec1, v.x, v.y, v.z);
            map1[static_cast<std::size_t>(vi)] = mapped;
        }

        capSec1.vertices.push_back(mapped);
    }

    // Step 4: append both caps, adjoin them, then recalculate derived geometry.
    const std::int32_t capIndex0 = static_cast<std::int32_t>(sec0.surfaces.size());
    sec0.surfaces.push_back(std::move(capSec));

    const std::int32_t capIndex1 = static_cast<std::int32_t>(sec1.surfaces.size());
    sec1.surfaces.push_back(std::move(capSec1));

    (void)levelSysAdjoinSurfaces(level, SurfaceRef{ sectorIndex, capIndex0 }, SurfaceRef{ newSectorIndex, capIndex1 }, true);

    const double ppu = levelPixelPerUnit(level);
    surfaceRecalcNewAll(ppu, sec0, sec0.surfaces[static_cast<std::size_t>(capIndex0)], 0);
    surfaceRecalcNewAll(ppu, sec1, sec1.surfaces[static_cast<std::size_t>(capIndex1)], 0);
    renumberSectorSurfaces(sec0);
    renumberSectorSurfaces(sec1);

    return true;
}

void relayerThingsAfterSectorSplit(Level& level, const std::int32_t sectorIndex) noexcept
{
    // Step 1: only things that used to belong to the split sector need
    // containment-based reassignment. Things in other sectors should keep their
    // existing sector IDs because the split only appended one new sector.
    for (Thing& thing : level.things)
    {
        if (thing.sector != sectorIndex)
        {
            continue;
        }

        thing.sector = levelFindSectorForThing(level, thing);
    }
}

void swapSplitSectorBodiesPreservingIdentity(
    Level& level,
    const std::int32_t originalSectorIndex,
    const std::int32_t appendedSectorIndex) noexcept
{
    if (!isValidSectorIndex(level, originalSectorIndex)
        || !isValidSectorIndex(level, appendedSectorIndex)
        || originalSectorIndex == appendedSectorIndex)
    {
        return;
    }

    Sector& originalSlot =
        level.sectors[static_cast<std::size_t>(originalSectorIndex)];
    Sector& appendedSlot =
        level.sectors[static_cast<std::size_t>(appendedSectorIndex)];
    const std::int32_t originalId = originalSlot.id;
    const std::int32_t originalNum = originalSlot.num;
    const std::int32_t originalMark = originalSlot.mark;
    const std::int32_t appendedId = appendedSlot.id;
    const std::int32_t appendedNum = appendedSlot.num;
    const std::int32_t appendedMark = appendedSlot.mark;

    // Exchange the geometric bodies, but keep identity-bearing slot metadata.
    // Sector COG values intentionally are not remapped: a COG authored against
    // the old sector number must continue to target the retained main body.
    std::swap(originalSlot, appendedSlot);
    originalSlot.id = originalId;
    originalSlot.num = originalNum;
    originalSlot.mark = originalMark;
    appendedSlot.id = appendedId;
    appendedSlot.num = appendedNum;
    appendedSlot.mark = appendedMark;

    const auto swapSectorReference = [originalSectorIndex, appendedSectorIndex](
                                         std::int32_t& sector) noexcept
    {
        if (sector == originalSectorIndex)
        {
            sector = appendedSectorIndex;
        }
        else if (sector == appendedSectorIndex)
        {
            sector = originalSectorIndex;
        }
    };

    // Adjoins and spatial ownership follow the exchanged geometry.
    for (Sector& sector : level.sectors)
    {
        for (Surface& surface : sector.surfaces)
        {
            if (surface.adjoin)
            {
                swapSectorReference(surface.adjoin->sector);
            }
        }
    }
    for (Thing& thing : level.things)
    {
        swapSectorReference(thing.sector);
    }
    for (SedLight& light : level.lights)
    {
        // mark is a derived containment cache rather than an authored Sector
        // link. Resolve it from position so unrelated stale/default marks are
        // not exchanged as though they were authored references.
        light.mark = levelFindSectorForXYZ(
            level, light.position.x, light.position.y, light.position.z);
    }
}

void clearSurfaceAdjoin(Surface& surface) noexcept
{
    surface.adjoin.reset();
    surface.nAdj = -1;
    surfaceClearAdjoinParams(surface);
}

void clearAdjoinsTargetingSector(Level& level, const std::int32_t sectorIndex) noexcept
{
    // Step 1: clear every portal that directly targets the sector being deleted
    // before the sector vector is erased. This separates "invalid now" from
    // "needs index shift later" and avoids ambiguous post-erase references.
    for (Sector& sector : level.sectors)
    {
        for (Surface& surface : sector.surfaces)
        {
            if (surface.adjoin && surface.adjoin->sector == sectorIndex)
            {
                clearSurfaceAdjoin(surface);
            }
        }
    }
}

void shiftAdjoinsAfterSectorDelete(Level& level, const std::int32_t sectorIndex) noexcept
{
    // Step 1: after sector vector erase, decrement every adjoin that pointed
    // past the deleted sector. A defensive same-sector check remains here in
    // case callers skipped the pre-erase clear step.
    for (Sector& sector : level.sectors)
    {
        for (Surface& surface : sector.surfaces)
        {
            if (!surface.adjoin)
            {
                continue;
            }

            if (surface.adjoin->sector == sectorIndex)
            {
                clearSurfaceAdjoin(surface);
            }
            else if (surface.adjoin->sector > sectorIndex)
            {
                surface.adjoin->sector -= 1;
            }
        }
    }
}

void clearThingSectorRefs(Level& level, const std::int32_t sectorIndex) noexcept
{
    // Step 1: things inside the deleted sector become unassigned before sector indices shift.
    for (Thing& thing : level.things)
    {
        if (thing.sector == sectorIndex)
        {
            thing.sector = -1;
        }
    }
}

void shiftThingSectorRefsAfterDelete(Level& level, const std::int32_t sectorIndex) noexcept
{
    // Step 1: things in sectors after the deleted one follow the shifted sector indices.
    for (Thing& thing : level.things)
    {
        if (thing.sector > sectorIndex)
        {
            thing.sector -= 1;
        }
    }
}

void clearAdjoinsTargetingSurface(Level& level, const SurfaceRef surfaceRef) noexcept
{
    // Step 1: clear every portal that directly targets the surface being deleted.
    for (Sector& sector : level.sectors)
    {
        for (Surface& surface : sector.surfaces)
        {
            if (surface.adjoin
                && surface.adjoin->sector == surfaceRef.sector
                && surface.adjoin->surface == surfaceRef.surface)
            {
                clearSurfaceAdjoin(surface);
            }
        }
    }
}

void shiftAdjoinsAfterSurfaceDelete(Level& level, const SurfaceRef surfaceRef) noexcept
{
    // Step 1: after surface vector erase, decrement every adjoin that pointed past the deleted surface.
    for (Sector& sector : level.sectors)
    {
        for (Surface& surface : sector.surfaces)
        {
            if (!surface.adjoin || surface.adjoin->sector != surfaceRef.sector)
            {
                continue;
            }

            if (surface.adjoin->surface == surfaceRef.surface)
            {
                clearSurfaceAdjoin(surface);
            }
            else if (surface.adjoin->surface > surfaceRef.surface)
            {
                surface.adjoin->surface -= 1;
            }
        }
    }
}

struct SectorCleaveGeometryValidation
{
    SectorCleaveBatchError error{ SectorCleaveBatchError::None };
    std::int32_t sector{ -1 };
    std::int32_t surface{ -1 };
};

bool isFiniteVector(const TVector& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool surfaceHasUniqueVertices(const Surface& surface) noexcept
{
    for (std::size_t i = 0; i < surface.vertices.size(); ++i)
    {
        if (std::find(surface.vertices.begin() + static_cast<std::ptrdiff_t>(i + 1), surface.vertices.end(), surface.vertices[i])
            != surface.vertices.end())
        {
            return false;
        }
    }

    return true;
}

SectorCleaveGeometryValidation validateSectorCleaveGeometry(
    const Level& level,
    const std::int32_t sectorIndex,
    const CleaveProfileContext* const context = nullptr,
    const bool compatibleInput = false)
{
    SectorCleaveGeometryValidation result{};
    result.sector = sectorIndex;

    if (!isValidSectorIndex(level, sectorIndex))
    {
        result.error = SectorCleaveBatchError::InvalidResultTopology;
        return result;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    const bool closed = !context || context->sectors.at(sectorIndex).closed;
    if (closed && (sector.vertices.size() < 4 || sector.surfaces.size() < 4))
    {
        result.error = SectorCleaveBatchError::InvalidResultTopology;
        return result;
    }

    for (const Vertex& vertex : sector.vertices)
    {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z))
        {
            result.error = SectorCleaveBatchError::InvalidResultTopology;
            return result;
        }
    }

    for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
    {
        const Surface& surface = sector.surfaces[surfaceIndex];
        result.surface = static_cast<std::int32_t>(surfaceIndex);
        if ( context && !context->validatesFace(sectorIndex, surfaceIndex) )
        {
            // A scoped Surface Cleave deliberately leaves unrelated faces
            // untouched. Their legacy cached fields and topology are not
            // evidence about the generated fragments being certified here.
            continue;
        }

        if (surface.vertices.size() < 3 || surface.vertices.size() != surface.texVertices.size()
            || !surfaceHasUniqueVertices(surface))
        {
            result.error = SectorCleaveBatchError::InvalidResultTopology;
            return result;
        }

        if (!isFiniteVector(surface.normal))
        {
            result.error = SectorCleaveBatchError::InvalidResultTopology;
            return result;
        }

        for (const std::int32_t vertexIndex : surface.vertices)
        {
            if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
            {
                result.error = SectorCleaveBatchError::InvalidResultTopology;
                return result;
            }
        }

        const bool planar = context
            ? context->planarFace(sector, surface, context->sectors.at(sectorIndex).faces.at(surfaceIndex), !closed)
            : (compatibleInput ? precision::sourceSurfaceIsPlanar(sector, surface) : surfaceIsPlanar(sector, surface));
        if ( !planar )
        {
            result.error = SectorCleaveBatchError::NonPlanarResultSurface;
            return result;
        }

        const bool convex = context
            ? context->convexFace(sector, surface, context->sectors.at(sectorIndex).faces.at(surfaceIndex))
            : (compatibleInput ? precision::sourceSurfaceIsConvex(sector, surface) : surfaceIsConvex(sector, surface));
        if ( !convex )
        {
            result.error = SectorCleaveBatchError::NonConvexResultSurface;
            return result;
        }
    }

    result.surface = -1;
    if (closed && !precision::closedBoundary(sector))
    {
        result.error = SectorCleaveBatchError::InvalidResultTopology;
        return result;
    }
    const bool convex = !closed || (context
        ? context->convexSector(sector, context->sectors.at(sectorIndex))
        : (compatibleInput ? precision::sourceSectorIsConvex(sector) : sectorIsConvex(sector)));
    if ( !convex )
    {
        result.error = SectorCleaveBatchError::NonConvexResultSector;
        return result;
    }

    for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
    {
        const Surface& surface = sector.surfaces[surfaceIndex];
        if (!surface.adjoin)
        {
            continue;
        }
        if ( context && !context->validatesFace(sectorIndex, surfaceIndex) )
        {
            continue;
        }

        result.surface = static_cast<std::int32_t>(surfaceIndex);
        if (!isValidSurfaceRef(level, *surface.adjoin))
        {
            result.error = SectorCleaveBatchError::InvalidResultAdjoin;
            return result;
        }

        const Sector& otherSector = level.sectors[static_cast<std::size_t>(surface.adjoin->sector)];
        const Surface& otherSurface = otherSector.surfaces[static_cast<std::size_t>(surface.adjoin->surface)];
        const SurfaceRef expectedMirror{ sectorIndex, static_cast<std::int32_t>(surfaceIndex) };
        if (!otherSurface.adjoin || *otherSurface.adjoin != expectedMirror
            || !((context || compatibleInput)
                ? surfaceBoundariesMatchWithResolvedNormals(
                    sector, surface, otherSector, otherSurface,
                    context != nullptr || compatibleInput,
                    context,
                    SurfaceRef{sectorIndex, static_cast<std::int32_t>(surfaceIndex)},
                    *surface.adjoin)
                : precision::surfaceDoOverlapChecked(sector, surface, otherSector, otherSurface)))
        {
            result.error = SectorCleaveBatchError::InvalidResultAdjoin;
            return result;
        }
    }

    result.surface = -1;
    return result;
}

void appendUniqueSector(std::vector<std::int32_t>& sectors, const std::int32_t sectorIndex)
{
    if (std::find(sectors.begin(), sectors.end(), sectorIndex) == sectors.end())
    {
        sectors.push_back(sectorIndex);
    }
}

std::vector<std::int32_t> collectCleaveAffectedSectors(
    const Level& level,
    const std::int32_t sectorIndex)
{
    std::vector<std::int32_t> sectors{};
    if (!isValidSectorIndex(level, sectorIndex))
    {
        return sectors;
    }

    appendUniqueSector(sectors, sectorIndex);
    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    for (const Surface& surface : sector.surfaces)
    {
        if (surface.adjoin && isValidSectorIndex(level, surface.adjoin->sector))
        {
            appendUniqueSector(sectors, surface.adjoin->sector);
        }
    }

    return sectors;
}

using CleaveSurfaceValidationScope = std::map<std::int32_t, std::vector<bool>>;

struct CleaveSurfaceEdgeWork
{
    std::int32_t sector{};
    std::int32_t edgeA{-1};
    std::int32_t edgeB{-1};
    // first/second are always the requested native edge. The fallback chord
    // is retained separately so an adjacent rounded run cannot shadow a
    // valid direct portal edge.
    Vertex first{};
    Vertex second{};
    Vertex fallbackFirst{};
    Vertex fallbackSecond{};
    bool hasFallback{};
    bool coordinate{};
};

bool markSurfaceValidationScope(
    const Level& level,
    CleaveSurfaceValidationScope& scope,
    const SurfaceRef ref)
{
    if ( !isValidSurfaceRef(level, ref) )
    {
        return false;
    }
    const auto& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    auto& faces = scope[ref.sector];
    if ( faces.empty() )
    {
        faces.resize(sector.surfaces.size(), false);
    }
    const auto surfaceIndex = static_cast<std::size_t>(ref.surface);
    if ( surfaceIndex >= faces.size() || faces[surfaceIndex] )
    {
        return false;
    }
    faces[surfaceIndex] = true;
    return true;
}

std::vector<std::pair<SurfaceRef, SurfaceRef>> collectLinkedEdgeSurfacePairs(
    const Level& level,
    const SurfaceRef source,
    const std::int32_t edge)
{
    std::vector<std::pair<SurfaceRef, SurfaceRef>> result;
    if ( !isValidSurfaceRef(level, source) )
    {
        return result;
    }
    const Sector& sector = level.sectors[static_cast<std::size_t>(source.sector)];
    if ( edge < 0 || static_cast<std::size_t>(edge) >= sector.surfaces[
            static_cast<std::size_t>(source.surface)].vertices.size() )
    {
        return result;
    }
    const Surface& selected = sector.surfaces[static_cast<std::size_t>(source.surface)];
    const std::int32_t edgeA = selected.vertices[static_cast<std::size_t>(edge)];
    const std::int32_t edgeB = selected.vertices[
        static_cast<std::size_t>(nextIndex(edge, static_cast<std::int32_t>(selected.vertices.size())))];
    for ( std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
    {
        const Surface& surface = sector.surfaces[surfaceIndex];
        if ( !surfaceContainsIndexedEdge(surface, edgeA, edgeB)
            || !surface.adjoin || !isValidSurfaceRef(level, *surface.adjoin) )
        {
            continue;
        }
        const std::pair<SurfaceRef, SurfaceRef> pair{
            SurfaceRef{source.sector, static_cast<std::int32_t>(surfaceIndex)},
            *surface.adjoin};
        if ( std::find(result.begin(), result.end(), pair) == result.end() )
        {
            result.push_back(pair);
        }
    }
    return result;
}

CleaveSurfaceValidationScope collectCleaveSurfaceValidationScope(
    const Level& level,
    const SurfaceRef source,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz)
{
    CleaveSurfaceValidationScope scope;
    if ( !isValidSurfaceRef(level, source) )
    {
        return scope;
    }
    (void)markSurfaceValidationScope(level, scope, source);

    std::vector<CleaveSurfaceEdgeWork> work;
    const auto appendCrossingEdges = [&](const SurfaceRef ref)
    {
        if ( !isValidSurfaceRef(level, ref) )
        {
            return;
        }
        const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
        if ( surface.vertices.size() < 2 || !surfaceHasValidVertexTopology(sector, surface) )
        {
            return;
        }
        for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
        {
            const auto next = (corner + 1) % surface.vertices.size();
            const auto edgeA = surface.vertices[corner];
            const auto edgeB = surface.vertices[next];
            const auto sideA = classifyPlaneSide(
                sector.vertices[static_cast<std::size_t>(edgeA)], planeNormal, px, py, pz);
            const auto sideB = classifyPlaneSide(
                sector.vertices[static_cast<std::size_t>(edgeB)], planeNormal, px, py, pz);
            if ( sideA == 0 || sideB == 0 || sideA == 2 || sideB == 2 || sideA != -sideB )
            {
                continue;
            }
            const auto support = sourceRoundedEdgeSupport(
                sector, surface, edgeA, edgeB);
            work.push_back({
                ref.sector,
                edgeA,
                edgeB,
                sector.vertices[static_cast<std::size_t>(edgeA)],
                sector.vertices[static_cast<std::size_t>(edgeB)],
                support ? support->first : Vertex{},
                support ? support->second : Vertex{},
                support.has_value(),
                false});
        }
    };

    appendCrossingEdges(source);
    const Surface& sourceSurface = level.sectors[static_cast<std::size_t>(source.sector)]
        .surfaces[static_cast<std::size_t>(source.surface)];
    if ( sourceSurface.adjoin && isValidSurfaceRef(level, *sourceSurface.adjoin) )
    {
        (void)markSurfaceValidationScope(level, scope, *sourceSurface.adjoin);
        appendCrossingEdges(*sourceSurface.adjoin);
    }

    for ( std::size_t workIndex = 0; workIndex < work.size(); ++workIndex )
    {
        const CleaveSurfaceEdgeWork edgeWork = work[workIndex];
        if ( edgeWork.sector < 0 || static_cast<std::size_t>(edgeWork.sector) >= level.sectors.size() )
        {
            continue;
        }
        const Sector& sector = level.sectors[static_cast<std::size_t>(edgeWork.sector)];
        std::vector<std::pair<std::int32_t, std::int32_t>> certifiedEdges;
        if ( edgeWork.coordinate )
        {
            const auto collectCertifiedEdges = [&](
                const Vertex& lineStart, const Vertex& lineEnd)
            {
                certifiedEdges.clear();
                for ( const Surface& candidate : sector.surfaces )
                {
                    const double candidateLimit = coordinateBoundaryDeviationLimit(
                        sector, candidate).value_or(0.0);
                    const double admissibilityLimit = candidateLimit > 0.0
                        ? candidateLimit : 0.0;
                    const auto certificate = coordinateEdgeChainCertificate(
                        sector, candidate, lineStart, lineEnd, admissibilityLimit);
                    if ( !certificate )
                    {
                        continue;
                    }
                    for ( const auto edge : certificate->edges )
                    {
                        const bool alreadyRecorded = std::any_of(
                            certifiedEdges.begin(), certifiedEdges.end(),
                            [edge](const auto existing)
                            {
                                return (existing.first == edge.first && existing.second == edge.second)
                                    || (existing.first == edge.second && existing.second == edge.first);
                            });
                        if ( !alreadyRecorded )
                        {
                            certifiedEdges.push_back(edge);
                        }
                    }
                }
            };

            // A direct native edge has priority. Only when no affected face
            // contains that edge do we use its certified rounded fallback.
            collectCertifiedEdges(edgeWork.first, edgeWork.second);
            if ( certifiedEdges.empty() && edgeWork.hasFallback )
            {
                collectCertifiedEdges(edgeWork.fallbackFirst, edgeWork.fallbackSecond);
            }
        }
        for ( std::size_t candidateIndex = 0;
              candidateIndex < sector.surfaces.size(); ++candidateIndex )
        {
            const Surface& candidate = sector.surfaces[candidateIndex];
            if ( candidate.vertices.size() < 2 )
            {
                continue;
            }
            const bool sharesEdge = edgeWork.coordinate
                ? std::any_of(
                    certifiedEdges.begin(), certifiedEdges.end(),
                    [&candidate](const auto edge)
                    {
                        return surfaceContainsIndexedEdge(candidate, edge.first, edge.second);
                    })
                : surfaceContainsIndexedEdge(candidate, edgeWork.edgeA, edgeWork.edgeB);
            if ( !sharesEdge )
            {
                continue;
            }
            const SurfaceRef candidateRef{
                edgeWork.sector, static_cast<std::int32_t>(candidateIndex) };
            (void)markSurfaceValidationScope(level, scope, candidateRef);
            if ( !candidate.adjoin )
            {
                continue;
            }
            if ( !isValidSurfaceRef(level, *candidate.adjoin) )
            {
                continue;
            }
            (void)markSurfaceValidationScope(level, scope, *candidate.adjoin);
            if ( edgeWork.coordinate )
            {
                continue;
            }
            const bool directlySplit = candidateRef == source
                || (sourceSurface.adjoin && candidateRef == *sourceSurface.adjoin);
            if ( directlySplit )
            {
                // propagateCutVerticesToRelatedSurfaces deliberately skips
                // the face that sysCleaveSurface split explicitly. Its
                // opposite portal is handled by cleaveAdjoin and its own
                // directly split edge work.
                continue;
            }
            const auto fallback = edgeWork.coordinate
                ? sourceRoundedEdgeSupportForCoordinates(
                    sector, candidate, edgeWork.first, edgeWork.second)
                : std::optional<CoordinateEdgeSupport>{};
            work.push_back({
                candidate.adjoin->sector,
                -1,
                -1,
                edgeWork.first,
                edgeWork.second,
                fallback ? fallback->first : Vertex{},
                fallback ? fallback->second : Vertex{},
                fallback.has_value(),
                true});
        }
    }
    return scope;
}

bool cleaveAffectedSectorsHaveFiniteUvValues(
    const Level& level,
    const std::vector<std::int32_t>& sectorIndices) noexcept
{
    for (const std::int32_t sectorIndex : sectorIndices)
    {
        if (!isValidSectorIndex(level, sectorIndex))
        {
            return false;
        }
        const Sector& sector =
            level.sectors[static_cast<std::size_t>(sectorIndex)];
        if (!std::all_of(
                sector.surfaces.begin(),
                sector.surfaces.end(),
                surfaceHasFiniteUvValues))
        {
            return false;
        }
    }
    return true;
}

SectorCleaveGeometryValidation validateCleaveAffectedSectors(
    const Level& level,
    const std::vector<std::int32_t>& sectorIndices,
    const CleaveProfileContext* const context = nullptr,
    const bool compatibleInput = false)
{
    for (const std::int32_t sectorIndex : sectorIndices)
    {
        const SectorCleaveGeometryValidation result = validateSectorCleaveGeometry(level, sectorIndex, context, compatibleInput);
        if (result.error != SectorCleaveBatchError::None)
        {
            return result;
        }
    }

    return {};
}

bool planeSplitsSector(
    const Sector& sector,
    const TVector& planeNormal,
    const TVector& planePoint) noexcept
{
    bool hasNegative = false;
    bool hasPositive = false;

    for (const Vertex& vertex : sector.vertices)
    {
        const double distance = planeNormal.dot(
            vertex.x - planePoint.x,
            vertex.y - planePoint.y,
            vertex.z - planePoint.z);

        if (std::abs(distance) <= precision::planeError(planeNormal,
                precision::position(vertex), planePoint))
        {
            continue;
        }

        hasNegative = hasNegative || distance < 0.0;
        hasPositive = hasPositive || distance > 0.0;
        if (hasNegative && hasPositive)
        {
            return true;
        }
    }

    return false;
}

} // namespace

namespace
{
class CleaveSectorRollback
{
public:
    CleaveSectorRollback(Level& level, const std::vector<std::int32_t>& affected) : level_(level)
    {
        snapshots_.reserve(affected.size());
        for ( const auto index : affected )
        {
            snapshots_.emplace_back(index, level.sectors[static_cast<std::size_t>(index)]);
        }
    }

    ~CleaveSectorRollback() noexcept
    {
        static_assert(std::is_nothrow_swappable_v<Sector>);
        if ( !committed_ )
        {
            for ( auto& [index, sector] : snapshots_ )
            {
                std::swap(level_.sectors[static_cast<std::size_t>(index)], sector);
            }
        }
    }

    const Sector* originalSector(const std::int32_t index) const noexcept
    {
        const auto found = std::find_if(snapshots_.begin(),snapshots_.end(),[index](const auto& snapshot)
        {
            return snapshot.first == index;
        });
        return found == snapshots_.end() ? nullptr : &found->second;
    }

    void commit() noexcept { committed_ = true; }

private:
    Level& level_;
    std::vector<std::pair<std::int32_t, Sector>> snapshots_;
    bool committed_{};
};
}

static bool levelCleaveSurfaceImpl(
    Level& level,
    const SurfaceRef surfaceRef,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz,
    CleaveProfileContext* const context,
    detail::ExtrudeArrangementContext* const arrangement)
{
    // Step 1: mark the selected surface vertices against the requested cleave plane.
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || !isValidSurfaceRef(level, surfaceRef))
    {
        return false;
    }

    if ( !arrangement && !preflightSurfaceCleave(
            level, surfaceRef, planeNormal, px, py, pz, context) )
    {
        return false;
    }

    Sector& sector = level.sectors[static_cast<std::size_t>(surfaceRef.sector)];
    Surface& surface = sector.surfaces[static_cast<std::size_t>(surfaceRef.surface)];

    if ( arrangement )
    {
        if ( surface.adjoin || !surfaceHasValidTextureTopology(sector,surface) )
        {
            return false;
        }
        for ( const auto index : surface.vertices )
        {
            const auto side = arrangement->classify(surfaceRef.sector,index);
            if ( side == 2 )
            {
                return false;
            }
            sector.vertices[index].mark = side;
        }
        for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
        {
            const auto a = surface.vertices[corner];
            const auto b = surface.vertices[(corner+1)%surface.vertices.size()];
            if ( sector.vertices[a].mark != 0 && sector.vertices[a].mark == -sector.vertices[b].mark
                && !preflightRelatedEdgeSurfaces(
                    level, surfaceRef.sector, a, b, nullptr, context) )
            {
                return false;
            }
        }
    }
    else
    {
        splitVertices(sector, surface.vertices, planeNormal, px, py, pz);
    }

    // Step 2: split the surface and then mirror the split through any existing adjoin.
    const std::optional<SurfaceRef> added = sysCleaveSurface(level, surfaceRef, planeNormal, px, py, pz, context, arrangement);
    if (!added)
    {
        return false;
    }

    return cleaveAdjoin(level, surfaceRef, planeNormal, px, py, pz, *added, context);
}

static bool levelCleaveEdgeImpl(
    Level& level,
    const SurfaceRef surfaceRef,
    const std::int32_t edge,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz,
    CleaveProfileContext* const context)
{
    // Step 1: validate the target surface and requested edge.
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || !isValidSurfaceRef(level, surfaceRef))
    {
        return false;
    }

    Sector& sector = level.sectors[static_cast<std::size_t>(surfaceRef.sector)];
    Surface& surface = sector.surfaces[static_cast<std::size_t>(surfaceRef.surface)];

    if (edge < 0 || static_cast<std::size_t>(edge) >= surface.vertices.size())
    {
        return false;
    }
    if ( !surfaceHasTextureCorners(surface) )
    {
        return false;
    }

    const std::int32_t v1Index = surface.vertices[static_cast<std::size_t>(edge)];
    const std::int32_t v2Index = surface.vertices[static_cast<std::size_t>(nextIndex(edge, static_cast<std::int32_t>(surface.vertices.size())))];

    if (v1Index < 0 || v2Index < 0 || static_cast<std::size_t>(v1Index) >= sector.vertices.size() || static_cast<std::size_t>(v2Index) >= sector.vertices.size())
    {
        return false;
    }

    const Vertex v1 = sector.vertices[static_cast<std::size_t>(v1Index)];
    const Vertex v2 = sector.vertices[static_cast<std::size_t>(v2Index)];
    const double sourceAdmissibilityLimit = coordinateBoundaryDeviationLimit(
        sector, surface).value_or(0.0);
    const std::int32_t side1 = classifyPlaneSide(v1, planeNormal, px, py, pz);
    const std::int32_t side2 = classifyPlaneSide(v2, planeNormal, px, py, pz);
    if ( side1 == 0 || side2 == 0 || side1 == 2 || side2 == 2 || side1 != -side2 )
    {
        return false;
    }

    const std::vector<std::pair<SurfaceRef, SurfaceRef>> linkedPairs =
        collectLinkedEdgeSurfacePairs(level, surfaceRef, edge);
    std::vector<std::int32_t> mirrorSectors{};
    if ( !preflightRelatedEdgeSurfaces(
            level,
            surfaceRef.sector,
            v1Index,
            v2Index,
            &mirrorSectors,
            context,
            true) )
    {
        return false;
    }

    // Step 2: resolve the cut point before changing source or portal topology.
    double ax = 0.0;
    double ay = 0.0;
    double az = 0.0;
    if (!planeLineIntersection(planeNormal, px, py, pz,
            v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, ax, ay, az))
    {
        return false;
    }

    splitVertices(sector, surface.vertices, planeNormal, px, py, pz);

    // Step 3: add the cut vertex at the edge/plane intersection.
    const Vertex insertedVertex{ ax, ay, az, 0 };
    const std::size_t startCorner = static_cast<std::size_t>(edge);
    const std::size_t endCorner = static_cast<std::size_t>(
        nextIndex(edge, static_cast<std::int32_t>(surface.vertices.size())));
    const TexVertex insertedTexVertex = texVertexForInsertedEdgePoint(
        sector,
        surface,
        startCorner,
        endCorner,
        insertedVertex);
    const std::int32_t newVx = precision::addVertex(sector, ax, ay, az);

    insertVertexAfter(surface, startCorner, newVx, insertedTexVertex);

    // Step 4: insert the same cut vertex into every same-sector surface that shares the edge.
    for (Surface& asurf : sector.surfaces)
    {
        (void)insertVertexIntoMatchingSurfaceEdge(
            sector,
            asurf,
            v1Index,
            v2Index,
            newVx,
            insertedVertex);
    }

    // Step 5: propagate one shared split vertex into every mirrored Sector.
    // Each mirrored surface interpolates the corner in its own authored UV
    // and baked-light frame.
    const CutPlaneDescriptor cutPlane{planeNormal, {px, py, pz}};
    for ( const auto& linkedPair : linkedPairs )
    {
        const CleaveFaceProfile* const sourceProfile = context
            ? context->sourceFaceProfile(linkedPair.first) : nullptr;
        const CleaveFaceProfile* const targetProfile = context
            ? context->sourceFaceProfile(linkedPair.second) : nullptr;
        if ( sourceProfile == nullptr || targetProfile == nullptr
            || sourceProfile->geometry.surfaces.empty()
            || targetProfile->geometry.surfaces.empty() )
        {
            return false;
        }
        if ( !insertVxtIntoSector(
                level,
                linkedPair.second,
                v1,
                v2,
                insertedVertex,
                sourceAdmissibilityLimit,
                &sourceProfile->geometry,
                &sourceProfile->geometry.surfaces.front(),
                &cutPlane,
                sourceProfile,
                targetProfile) )
        {
            return false;
        }
    }

    // Step 6: refresh affected surface normals and per-sector surface numbers.
    // Existing texture corners stay exact; only the inserted corner was
    // interpolated from the selected edge above.
    recalcAndRenumberSectorSurfaces(sector);

    return true;
}

bool levelCleaveSurface(Level& level, const SurfaceRef ref, const TVector& normal,
    const double px, const double py, const double pz) noexcept
{
    if ( !isValidSurfaceRef(level, ref) ) return false;
    try
    {
        std::vector<std::int32_t> affected = collectCleaveAffectedSectors(level, ref.sector);
        const auto mirror = level.sectors[static_cast<std::size_t>(ref.sector)]
            .surfaces[static_cast<std::size_t>(ref.surface)].adjoin;
        if ( mirror && isValidSurfaceRef(level, *mirror) )
        {
            // Splitting the opposite portal can insert its new corners into
            // that owner's other adjoined edge neighbors as well.
            for ( const auto index : collectCleaveAffectedSectors(level, mirror->sector) )
            {
                appendUniqueSector(affected, index);
            }
        }
        const CleaveSurfaceValidationScope validationScope = collectCleaveSurfaceValidationScope(
            level, ref, normal, px, py, pz);
        CleaveProfileContext profiles;
        if ( !profiles.capture(level, affected, false, true, &validationScope) )
        {
            return false;
        }
        CleaveSectorRollback rollback(level, affected);
        if ( !levelCleaveSurfaceImpl(level, ref, normal, px, py, pz, &profiles)
            || validateCleaveAffectedSectors(level, affected, &profiles).error != SectorCleaveBatchError::None )
        {
            return false;
        }
        profiles.cogReferences->publish(level);
        rollback.commit();
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool levelCleaveEdge(Level& level, const SurfaceRef ref, const std::int32_t edge,
    const TVector& normal, const double px, const double py, const double pz) noexcept
{
    if ( !isValidSurfaceRef(level, ref) ) return false;
    try
    {
        const auto affected = collectCleaveAffectedSectors(level, ref.sector);
        const auto linkedPairs = collectLinkedEdgeSurfacePairs(level, ref, edge);
        CleaveSurfaceValidationScope edgeScope;
        if ( !markSurfaceValidationScope(level, edgeScope, ref) )
        {
            return false;
        }
        for ( const auto& linkedPair : linkedPairs )
        {
            (void)markSurfaceValidationScope(level, edgeScope, linkedPair.first);
            (void)markSurfaceValidationScope(level, edgeScope, linkedPair.second);
        }
        CleaveProfileContext profiles;
        if ( !profiles.capture(level, affected, false, false, &edgeScope) )
        {
            return false;
        }
        detail::GeometryCogReferences references(level);
        CleaveSectorRollback rollback(level, affected);
        if ( !levelCleaveEdgeImpl(level, ref, edge, normal, px, py, pz, &profiles) ) return false;
        references.publish(level);
        rollback.commit();
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static bool levelConnectSurfacesImpl(
    Level& level,
    const SurfaceRef surf1,
    const SurfaceRef surf2,
    CleaveProfileContext* const context)
{
    // Step 1: validate that two different sectors expose unadjoined, non-empty surfaces.
    if (!isValidSurfaceRef(level, surf1) || !isValidSurfaceRef(level, surf2))
    {
        return false;
    }

    if (surf1.sector == surf2.sector)
    {
        return false;
    }

    Sector& sec1 = level.sectors[static_cast<std::size_t>(surf1.sector)];
    Sector& sec2 = level.sectors[static_cast<std::size_t>(surf2.sector)];

    if (sec1.surfaces[static_cast<std::size_t>(surf1.surface)].adjoin || sec2.surfaces[static_cast<std::size_t>(surf2.surface)].adjoin)
    {
        return false;
    }

    if (sec1.surfaces[static_cast<std::size_t>(surf1.surface)].vertices.empty() || sec2.surfaces[static_cast<std::size_t>(surf2.surface)].vertices.empty())
    {
        return false;
    }

    if (sec1.surfaces[static_cast<std::size_t>(surf1.surface)].normal.length() == 0.0)
    {
        Surface& s1 = sec1.surfaces[static_cast<std::size_t>(surf1.surface)];
        s1.normal = surfaceCalcNormalNewell(sec1, s1);
    }

    if (sec2.surfaces[static_cast<std::size_t>(surf2.surface)].normal.length() == 0.0)
    {
        Surface& s2 = sec2.surfaces[static_cast<std::size_t>(surf2.surface)];
        s2.normal = surfaceCalcNormalNewell(sec2, s2);
    }

    const Surface& s1 = sec1.surfaces[static_cast<std::size_t>(surf1.surface)];
    const Surface& s2 = sec2.surfaces[static_cast<std::size_t>(surf2.surface)];
    const std::int32_t sec1SurfaceCountBefore = static_cast<std::int32_t>(sec1.surfaces.size());
    const std::int32_t sec2SurfaceCountBefore = static_cast<std::int32_t>(sec2.surfaces.size());

    // Step 2: surfaces must be opposite-facing and coplanar enough to form a portal.
    const std::int32_t v1Index = s1.vertices[0];
    const std::int32_t v2Index = s2.vertices[0];

    if (v1Index < 0 || v2Index < 0 || static_cast<std::size_t>(v1Index) >= sec1.vertices.size() || static_cast<std::size_t>(v2Index) >= sec2.vertices.size())
    {
        return false;
    }

    const double dot = s1.normal.dot(s2.normal);
    if (dot > -0.995)
    {
        return false;
    }

    const Vertex& v1 = sec1.vertices[static_cast<std::size_t>(v1Index)];
    const Vertex& v2 = sec2.vertices[static_cast<std::size_t>(v2Index)];

    const double dist = std::abs(s1.normal.dot(v2.x - v1.x, v2.y - v1.y, v2.z - v1.z));
    if (dist > 0.02)
    {
        return false;
    }

    // Step 3: cut each surface by the other surface's boundary so only the overlap remains as candidates.
    std::optional<Sector> secondBoundary;
    if ( context )
    {
        // Retain only the second input boundary before the first clipping pass.
        // Subsequent clipping must not use an already-trimmed intermediate.
        secondBoundary.emplace();
        Surface boundary;
        boundary.normal = s2.normal;
        for ( const auto index : s2.vertices )
        {
            boundary.vertices.push_back(static_cast<std::int32_t>(secondBoundary->vertices.size()));
            secondBoundary->vertices.push_back(sec2.vertices[static_cast<std::size_t>(index)]);
        }
        secondBoundary->surfaces.push_back(std::move(boundary));
    }
    if ( !cleaveSurfaceByBoundary(level, surf2, sec1, s1.vertices, s1.normal, context) )
    {
        return false;
    }

    if (!isValidSurfaceRef(level, surf2))
    {
        return false;
    }

    Sector& sec2After = level.sectors[static_cast<std::size_t>(surf2.sector)];
    if (sec2After.surfaces[static_cast<std::size_t>(surf2.surface)].normal.length() == 0.0)
    {
        Surface& s2m = sec2After.surfaces[static_cast<std::size_t>(surf2.surface)];
        s2m.normal = surfaceCalcNormalNewell(sec2After, s2m);
    }

    const Surface& s2After = sec2After.surfaces[static_cast<std::size_t>(surf2.surface)];
    if ( secondBoundary )
    {
        const auto& boundary = secondBoundary->surfaces.front();
        if ( !cleaveSurfaceByBoundary(level, surf1, *secondBoundary, boundary.vertices, boundary.normal, context) )
        {
            return false;
        }
    }
    else
    {
        (void)cleaveSurfaceByBoundary(level, surf1, sec2After, s2After.vertices, s2After.normal);
    }

    if (!isValidSurfaceRef(level, surf1) || !isValidSurfaceRef(level, surf2))
    {
        return false;
    }

    // Step 4: rounded source planes can partition one overlap into several
    // exact cells. The first clipping phase already partitioned the second
    // side by the immutable first boundary. Apply those frozen descendants to
    // the first side so both retain every cell of the same arrangement.
    if ( context && (level.sectors[static_cast<std::size_t>(surf1.sector)].surfaces.size()
            > static_cast<std::size_t>(sec1SurfaceCountBefore)
        || level.sectors[static_cast<std::size_t>(surf2.sector)].surfaces.size()
            > static_cast<std::size_t>(sec2SurfaceCountBefore)) )
    {
        const auto captureContours = [&](const SurfaceRef source, const std::int32_t countBefore)
        {
            std::vector<Sector> contours;
            const auto& owner = level.sectors[static_cast<std::size_t>(source.sector)];
            for ( const auto ref : collectSurfaceCandidates(level, source.sector, source.surface, countBefore) )
            {
                const auto& face = owner.surfaces[static_cast<std::size_t>(ref.surface)];
                Sector contour;
                Surface boundary;
                boundary.normal = face.normal;
                for ( const auto index : face.vertices )
                {
                    boundary.vertices.push_back(static_cast<std::int32_t>(contour.vertices.size()));
                    contour.vertices.push_back(owner.vertices[static_cast<std::size_t>(index)]);
                }
                contour.surfaces.push_back(std::move(boundary));
                contours.push_back(std::move(contour));
            }
            return contours;
        };
        const auto secondContours = captureContours(surf2, sec2SurfaceCountBefore);
        const auto refine = [&](const SurfaceRef target, const std::int32_t countBefore,
            const std::vector<Sector>& contours)
        {
            for ( const auto& contour : contours )
            {
                const auto& boundary = contour.surfaces.front();
                const auto targets = collectSurfaceCandidates(level, target.sector, target.surface, countBefore);
                for ( const auto ref : targets )
                {
                    if ( !cleaveSurfaceByBoundary(level, ref, contour, boundary.vertices, boundary.normal, context) )
                    {
                        return false;
                    }
                }
            }
            return true;
        };
        if ( !refine(surf1, sec1SurfaceCountBefore, secondContours) )
        {
            return false;
        }
    }

    const auto sec1Candidates = collectSurfaceCandidates(level, surf1.sector, surf1.surface, sec1SurfaceCountBefore);
    const auto sec2Candidates = collectSurfaceCandidates(level, surf2.sector, surf2.surface, sec2SurfaceCountBefore);
    bool connected = false;
    while ( tryAdjoinSurfaceCandidatePairs(level, sec1Candidates, sec2Candidates, context) )
    {
        connected = true;
        if ( !context )
        {
            break;
        }
    }
    return connected;
}

bool precision::connectSurfacesChecked(Level& level, const SurfaceRef surf1, const SurfaceRef surf2)
{
    if ( !isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || !isValidSurfaceRef(level, surf1) || !isValidSurfaceRef(level, surf2)
        || surf1.sector == surf2.sector )
    {
        return false;
    }
    auto affected = collectCleaveAffectedSectors(level, surf1.sector);
    for ( const auto index : collectCleaveAffectedSectors(level, surf2.sector) )
    {
        appendUniqueSector(affected, index);
    }
    CleaveProfileContext profiles;
    if ( !cleaveAffectedSectorsHaveFiniteUvValues(level, affected)
        || !profiles.capture(level, affected, false)
        || validateCleaveAffectedSectors(level, affected, &profiles).error != SectorCleaveBatchError::None )
    {
        return false;
    }
    Level candidate = level;
    if ( !levelConnectSurfacesImpl(candidate, surf1, surf2, &profiles)
        || !cleaveAffectedSectorsHaveFiniteUvValues(candidate, affected)
        || validateCleaveAffectedSectors(candidate, affected, &profiles).error != SectorCleaveBatchError::None )
    {
        return false;
    }
    profiles.cogReferences->publish(candidate);
    level = std::move(candidate);
    return true;
}

namespace
{
TVector projectExtrudePoint(const TVector& point, const TVector& normal, const TVector& anchor) noexcept
{
    return point - normal * normal.dot(point - anchor);
}

Sector projectedExtrudeFace(const Sector& source, const Surface& face,
    const TVector& normal, const TVector& anchor)
{
    Sector result;
    Surface polygon;
    for ( const auto index : face.vertices )
    {
        const auto point = projectExtrudePoint(precision::position(source.vertices[static_cast<std::size_t>(index)]), normal, anchor);
        polygon.vertices.push_back(result.newVertex(point.x, point.y, point.z));
    }
    polygon.normal = surfaceCalcNormalNewell(result, polygon);
    result.surfaces.push_back(std::move(polygon));
    return result;
}

TVector extrudeProjectionError(const Sector& source, const Surface& sourceFace,
    const Sector& destination, const Surface& destinationFace,
    const TVector& normal, const TVector& anchor) noexcept
{
    TVector magnitude{};
    const auto include = [&](const Sector& sector, const Surface& face)
    {
        for ( const auto index : face.vertices )
        {
            const auto point = precision::position(sector.vertices[static_cast<std::size_t>(index)]);
            magnitude.x = std::max(magnitude.x, std::abs(point.x));
            magnitude.y = std::max(magnitude.y, std::abs(point.y));
            magnitude.z = std::max(magnitude.z, std::abs(point.z));
        }
    };
    include(source, sourceFace);
    include(destination, destinationFace);
    magnitude.x += std::abs(anchor.x);
    magnitude.y += std::abs(anchor.y);
    magnitude.z += std::abs(anchor.z);
    const double distanceMagnitude = std::abs(normal.x) * magnitude.x
        + std::abs(normal.y) * magnitude.y + std::abs(normal.z) * magnitude.z;
    // Propagate the initial construction/projection operands, not the small
    // rounded result near zero. This is a coordinate error, never a weld radius.
    return {precision::kRoundoff * (magnitude.x + std::abs(normal.x) * distanceMagnitude),
        precision::kRoundoff * (magnitude.y + std::abs(normal.y) * distanceMagnitude),
        precision::kRoundoff * (magnitude.z + std::abs(normal.z) * distanceMagnitude)};
}

bool extrudeProjectedPointsMatch(const TVector& first, const TVector& second,
    const TVector& error) noexcept
{
    return precision::finite(first) && precision::finite(second)
        && std::abs(first.x - second.x) <= error.x + precision::kRoundoff * (std::abs(first.x) + std::abs(second.x))
        && std::abs(first.y - second.y) <= error.y + precision::kRoundoff * (std::abs(first.y) + std::abs(second.y))
        && std::abs(first.z - second.z) <= error.z + precision::kRoundoff * (std::abs(first.z) + std::abs(second.z));
}

bool extrudeProjectedPointOnSegment(const TVector& point, const TVector& first,
    const TVector& second, const TVector& error, double& amount) noexcept
{
    const TVector edge = second - first;
    const double length = std::max({std::abs(edge.x), std::abs(edge.y), std::abs(edge.z)});
    if ( !(length > 0.0) || !std::isfinite(length) )
    {
        return false;
    }
    const bool useX = std::abs(edge.x) == length;
    const bool useY = !useX && std::abs(edge.y) == length;
    const double delta = useX ? point.x - first.x : useY ? point.y - first.y : point.z - first.z;
    const double component = useX ? edge.x : useY ? edge.y : edge.z;
    const double allowance = (useX ? error.x : useY ? error.y : error.z) / length + precision::kRoundoff;
    const double parameter = delta / component;
    if ( !std::isfinite(parameter) || !std::isfinite(allowance)
        || parameter < -allowance || parameter > 1.0 + allowance )
    {
        return false;
    }
    const double clamped = std::clamp(parameter, 0.0, 1.0);
    const TVector projected{std::lerp(first.x, second.x, clamped),
        std::lerp(first.y, second.y, clamped), std::lerp(first.z, second.z, clamped)};
    if ( !extrudeProjectedPointsMatch(point, projected, error) )
    {
        return false;
    }
    amount = clamped;
    return true;
}

bool reconcileExtrudeProjection(const Sector& source, Sector& destination,
    const TVector& error, const std::vector<detail::OriginalCornerProjection>& certificates)
{
    const auto& corners = source.surfaces.front().vertices;
    for ( auto& vertex : destination.vertices )
    {
        const TVector point = precision::position(vertex);
        // A different original corner may have disappeared from this clipped
        // fragment. Its plausible certificate still makes reconstruction
        // ambiguous, so select against the immutable complete source contour.
        const auto originalMatch = detail::matchOriginalCornerProjections(point, certificates, error);
        if ( originalMatch.kind == detail::OriginalCornerMatchKind::Ambiguous )
        {
            return false;
        }
        std::optional<std::size_t> cornerMatch;
        for ( std::size_t corner = 0; corner < corners.size(); ++corner )
        {
            const auto sourcePoint = precision::position(source.vertices[corners[corner]]);
            const bool conditionedMatch = originalMatch.kind == detail::OriginalCornerMatchKind::Unique
                && precision::samePoint(sourcePoint, certificates[originalMatch.index].projected);
            if ( extrudeProjectedPointsMatch(point, sourcePoint, error) || conditionedMatch )
            {
                if ( cornerMatch )
                {
                    return false;
                }
                cornerMatch = corner;
            }
        }
        std::optional<TVector> match;
        for ( std::size_t edge = 0; edge < corners.size(); ++edge )
        {
            const auto next = (edge + 1) % corners.size();
            const auto first = precision::position(source.vertices[corners[edge]]);
            const auto second = precision::position(source.vertices[corners[next]]);
            double amount{};
            if ( !extrudeProjectedPointOnSegment(point, first, second, error, amount) )
            {
                continue;
            }
            if ( cornerMatch && (edge == *cornerMatch || next == *cornerMatch) )
            {
                continue;
            }
            // Distinct plausible edges/corners would erase a real small feature.
            if ( cornerMatch || match )
            {
                return false;
            }
            match = TVector{std::lerp(first.x, second.x, amount),
                std::lerp(first.y, second.y, amount), std::lerp(first.z, second.z, amount)};
        }
        if ( cornerMatch )
        {
            match = precision::position(source.vertices[corners[*cornerMatch]]);
        }
        if ( !match )
        {
            return false;
        }
        vertex.x = match->x;
        vertex.y = match->y;
        vertex.z = match->z;
    }
    return precision::surfaceBoundariesMatchChecked(source, source.surfaces.front(),
        destination, destination.surfaces.front());
}

bool canonicalizeExtrudeCutJunctions(Sector& sector, const Sector& original,
    const Surface& originalFace, const Sector& rectangleOwner, const Surface& rectangle,
    const TVector& normal, const TVector& anchor, const TVector& error,
    const std::int32_t sectorIndex, const CleaveProfileContext& profiles)
{
    for ( std::size_t rectangleCorner = 0; rectangleCorner < rectangle.vertices.size(); ++rectangleCorner )
    {
        const auto rectangleIndex = rectangle.vertices[rectangleCorner];
        const TVector junction = projectExtrudePoint(precision::position(rectangleOwner.vertices[rectangleIndex]), normal, anchor);
        std::vector<std::int32_t> candidates;
        std::optional<std::int32_t> exact;
        for ( std::size_t index = original.vertices.size(); index < sector.vertices.size(); ++index )
        {
            const auto point = projectExtrudePoint(precision::position(sector.vertices[index]), normal, anchor);
            if ( !extrudeProjectedPointsMatch(point, junction, error) )
            {
                continue;
            }
            candidates.push_back(static_cast<std::int32_t>(index));
            if ( point.x == junction.x && point.y == junction.y && point.z == junction.z )
            {
                exact = static_cast<std::int32_t>(index);
            }
        }
        if ( candidates.empty() || (candidates.size() == 1 && exact) )
        {
            continue;
        }
        // Only this transaction's intersections can coalesce. An existing
        // authored corner or two distinct source edges would be an ambiguous
        // real feature, rather than one rectangle-plane construction junction.
        std::size_t matchingEdges = 0;
        std::optional<std::size_t> sourceEdge;
        double sourceAmount{};
        for ( std::size_t corner = 0; corner < originalFace.vertices.size(); ++corner )
        {
            const auto first = projectExtrudePoint(precision::position(original.vertices[originalFace.vertices[corner]]), normal, anchor);
            const auto second = projectExtrudePoint(precision::position(original.vertices[
                originalFace.vertices[(corner + 1) % originalFace.vertices.size()]]), normal, anchor);
            if ( extrudeProjectedPointsMatch(first, junction, error) )
            {
                return false;
            }
            double amount{};
            if ( extrudeProjectedPointOnSegment(junction, first, second, error, amount) )
            {
                ++matchingEdges;
                sourceEdge = corner;
                sourceAmount = amount;
            }
        }
        if ( matchingEdges > 1 )
        {
            return false;
        }
        std::optional<std::pair<TVector, TVector>> reconstructedEdge;
        if ( sourceEdge )
        {
            const auto next = (*sourceEdge + 1) % originalFace.vertices.size();
            reconstructedEdge = std::pair{
                precision::position(original.vertices[originalFace.vertices[*sourceEdge]]),
                precision::position(original.vertices[originalFace.vertices[next]])};
        }
        bool reconstructed = false;
        if ( !exact )
        {
            double height{};
            if ( reconstructedEdge )
            {
                height = std::lerp(normal.dot(reconstructedEdge->first - anchor),
                    normal.dot(reconstructedEdge->second - anchor), sourceAmount);
            }
            else
            {
                // An interior rectangle corner is the unique intersection of
                // its two immutable cutting planes on a strictly planar source.
                // Only new intersections move; inherited warped faces require
                // the original-edge certificate above to recover their height.
                Surface planarFace = originalFace;
                planarFace.normal = normal;
                if ( !surfaceIsPlanar(original, planarFace) )
                {
                    continue;
                }
                height = normal.dot(precision::position(original.vertices[originalFace.vertices.front()]) - anchor);
                const auto previousCorner = (rectangleCorner + rectangle.vertices.size() - 1) % rectangle.vertices.size();
                const auto nextCorner = (rectangleCorner + 1) % rectangle.vertices.size();
                const TVector previous = projectExtrudePoint(precision::position(rectangleOwner.vertices[
                    rectangle.vertices[previousCorner]]), normal, anchor);
                const TVector next = projectExtrudePoint(precision::position(rectangleOwner.vertices[
                    rectangle.vertices[nextCorner]]), normal, anchor);
                TVector firstPlane = (previous - junction).cross(normal);
                TVector secondPlane = (next - junction).cross(normal);
                const double firstLength = std::hypot(firstPlane.x, firstPlane.y, firstPlane.z);
                const double secondLength = std::hypot(secondPlane.x, secondPlane.y, secondPlane.z);
                if ( !(firstLength > 0.0) || !(secondLength > 0.0)
                    || !std::isfinite(firstLength) || !std::isfinite(secondLength) )
                {
                    return false;
                }
                firstPlane = {firstPlane.x / firstLength, firstPlane.y / firstLength, firstPlane.z / firstLength};
                secondPlane = {secondPlane.x / secondLength, secondPlane.y / secondLength, secondPlane.z / secondLength};
                if ( std::abs(firstPlane.cross(secondPlane).dot(normal)) < 0.5 )
                {
                    return false;
                }
                for ( const auto index : candidates )
                {
                    const TVector point = precision::position(sector.vertices[index]);
                    const auto onPlane = [&](const TVector& plane, const double offset)
                    {
                        const double allowance = std::abs(plane.x) * error.x + std::abs(plane.y) * error.y
                            + std::abs(plane.z) * error.z + precision::planeError(plane, point, junction);
                        return std::abs(plane.dot(point - junction) - offset) <= allowance;
                    };
                    if ( !onPlane(firstPlane, 0.0) || !onPlane(secondPlane, 0.0) || !onPlane(normal, height) )
                    {
                        return false;
                    }
                }
            }
            const TVector canonical = junction + normal * height;
            if ( !precision::finite(canonical) )
            {
                return false;
            }
            exact = candidates.front();
            auto& vertex = sector.vertices[static_cast<std::size_t>(*exact)];
            vertex.x = canonical.x;
            vertex.y = canonical.y;
            vertex.z = canonical.z;
            reconstructed = true;
        }
        for ( auto& face : sector.surfaces )
        {
            bool changed = false;
            for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
            {
                auto& index = face.vertices[corner];
                if ( std::find(candidates.begin(), candidates.end(), index) != candidates.end() )
                {
                    changed = changed || index != *exact || reconstructed || reconstructedEdge.has_value();
                    index = *exact;
                    if ( reconstructedEdge )
                    {
                        const auto faceIndex = static_cast<std::size_t>(&face - sector.surfaces.data());
                        const auto id = profiles.sectors.at(sectorIndex).faces.at(faceIndex);
                        const auto& parent = profiles.originals.at(id).geometry;
                        const auto& parentFace = parent.surfaces.front();
                        bool found = false;
                        for ( std::size_t edge = 0; edge < parentFace.vertices.size(); ++edge )
                        {
                            const auto next = (edge + 1) % parentFace.vertices.size();
                            const auto a = precision::position(parent.vertices[parentFace.vertices[edge]]);
                            const auto b = precision::position(parent.vertices[parentFace.vertices[next]]);
                            if ( (precision::samePoint(a, reconstructedEdge->first) && precision::samePoint(b, reconstructedEdge->second))
                                || (precision::samePoint(a, reconstructedEdge->second) && precision::samePoint(b, reconstructedEdge->first)) )
                            {
                                face.texVertices[corner] = texVertexForInsertedEdgePoint(parent, parentFace,
                                    edge, next, sector.vertices[static_cast<std::size_t>(*exact)]);
                                found = true;
                                break;
                            }
                        }
                        if ( !found )
                        {
                            return false;
                        }
                    }
                }
            }
            if ( !changed )
            {
                continue;
            }
            for ( std::size_t corner = 0; corner < face.vertices.size(); )
            {
                const auto next = (corner + 1) % face.vertices.size();
                if ( face.vertices[corner] != face.vertices[next] )
                {
                    ++corner;
                    continue;
                }
                const auto& a = face.texVertices[corner];
                const auto& b = face.texVertices[next];
                if ( a.u != b.u || a.v != b.v || a.color != b.color || face.vertices.size() <= 3 )
                {
                    return false;
                }
                face.vertices.erase(face.vertices.begin() + static_cast<std::ptrdiff_t>(next));
                face.texVertices.erase(face.texVertices.begin() + static_cast<std::ptrdiff_t>(next));
                corner = 0;
            }
            face.normal = surfaceCalcNormalNewell(sector, face);
        }
    }
    return true;
}

bool clipExtrudeBoundary(Level& level, const SurfaceRef target,
    const Sector& boundaryOwner, const Surface& boundary, const TVector& normal,
    CleaveProfileContext& profiles)
{
    std::vector<TVector> points;
    for ( const auto index : boundary.vertices )
    {
        points.push_back(precision::position(boundaryOwner.vertices[static_cast<std::size_t>(index)]));
    }
    for ( std::size_t edge = 0; edge < points.size(); ++edge )
    {
        const TVector planeNormal = (points[(edge + 1) % points.size()] - points[edge]).cross(normal);
        const Sector& sector = level.sectors[static_cast<std::size_t>(target.sector)];
        const Surface& face = sector.surfaces[static_cast<std::size_t>(target.surface)];
        const auto& anchor = points[edge];
        if ( !surfaceCanSplitOnPlane(sector, face, planeNormal, anchor.x, anchor.y, anchor.z) )
        {
            continue;
        }
        if ( !levelCleaveSurfaceImpl(level, target, planeNormal, anchor.x, anchor.y, anchor.z, &profiles) )
        {
            return false;
        }
    }
    return true;
}

bool clipExtrudeArrangementBoundary(Level& level, const SurfaceRef target,
    const SurfaceRef boundaryRef, const Surface& boundary, const TVector& normal,
    CleaveProfileContext& profiles, detail::ExtrudeArrangementContext& arrangement)
{
    using LineId = detail::ExtrudeArrangementRegistry::LineId;
    std::vector<std::pair<LineId,int>> cuts;
    cuts.reserve(boundary.vertices.size());
    for ( std::size_t edge = 0; edge < boundary.vertices.size(); ++edge )
    {
        const auto a = boundary.vertices[edge];
        const auto b = boundary.vertices[(edge+1)%boundary.vertices.size()];
        const auto id = arrangement.edgeLine(boundaryRef,a,b);
        if ( id == detail::ExtrudeArrangementRegistry::kInvalidLine )
        {
            return false;
        }
        const auto& line = arrangement.line(id);
        const auto firstNode = arrangement.node(boundaryRef.sector,a);
        const auto secondNode = arrangement.node(boundaryRef.sector,b);
        const auto certifiedOrder = arrangement.boundaryOrientation(firstNode,secondNode,id,normal);
        double orientation = 0.0;
        if ( certifiedOrder )
        {
            // The exact sign includes the passed normal's orientation relative
            // to the registry frame and remains meaningful when two generated
            // nodes have identical rounded projected coordinates. If exact
            // provenance is unavailable, boundaryOrientation accepts only a
            // nonzero propagated interval sign.
            orientation = static_cast<double>(*certifiedOrder);
        }
        else
        {
            if ( !arrangement.isOriginalNode(firstNode) || !arrangement.isOriginalNode(secondNode) )
            {
                return false;
            }
            const auto direction = arrangement.projected(boundaryRef.sector,b)
                - arrangement.projected(boundaryRef.sector,a);
            orientation = direction.cross(normal).dot(line.plane);
        }
        if ( !std::isfinite(orientation) || orientation == 0.0 )
        {
            return false;
        }
        cuts.emplace_back(id,orientation < 0.0 ? -1 : 1);
    }
    for ( const auto& [id,orientation] : cuts )
    {
        arrangement.setCut(id,orientation);
        const auto& face = level.sectors[target.sector].surfaces[target.surface];
        bool positive = false, negative = false;
        for ( const auto index : face.vertices )
        {
            const auto side = arrangement.classify(target.sector,index);
            if ( side == 2 )
            {
                return false;
            }
            positive = positive || side > 0;
            negative = negative || side < 0;
        }
        if ( !positive || !negative )
        {
            continue;
        }
        const auto& line = arrangement.line(id);
        if ( !levelCleaveSurfaceImpl(level,target,line.plane*static_cast<double>(orientation),
                line.first.x,line.first.y,line.first.z,&profiles,&arrangement) )
        {
            return false;
        }
    }
    return true;
}

Sector projectedExtrudeArrangementFace(const Sector& owner, const SurfaceRef ref,
    const detail::ExtrudeArrangementContext& arrangement, const TVector& normal)
{
    Sector result;
    Surface face = owner.surfaces[ref.surface];
    face.vertices.clear();
    for ( const auto index : owner.surfaces[ref.surface].vertices )
    {
        const auto point = arrangement.projected(ref.sector,index);
        face.vertices.push_back(result.newVertex(point.x,point.y,point.z));
    }
    face.normal = normal;
    result.surfaces.push_back(std::move(face));
    return result;
}

bool liftExtrudeBoundary(Level& level, const SurfaceRef original, const SurfaceRef generated,
    const TVector& normal, const TVector& anchor, const TVector& projectionError,
    const std::vector<detail::OriginalCornerProjection>& certificates, const bool projectedOnly = false)
{
    const Sector& source = level.sectors[static_cast<std::size_t>(original.sector)];
    const Surface& sourceFace = source.surfaces[static_cast<std::size_t>(original.surface)];
    Sector& destination = level.sectors[static_cast<std::size_t>(generated.sector)];
    const Surface& destinationFace = destination.surfaces[static_cast<std::size_t>(generated.surface)];
    const Sector sourceProjection = projectedExtrudeFace(source, sourceFace, normal, anchor);
    Sector destinationProjection = projectedExtrudeFace(destination, destinationFace, normal, anchor);
    if ( !reconcileExtrudeProjection(sourceProjection, destinationProjection, projectionError, certificates) )
    {
        return false;
    }
    const auto targetPosition = [&](const TVector& point)
    {
        return projectedOnly ? projectExtrudePoint(point, normal, anchor) : point;
    };

    std::vector<std::pair<std::int32_t, TVector>> positions;
    for ( std::size_t corner = 0; corner < destinationFace.vertices.size(); ++corner )
    {
        const auto projected = precision::position(destinationProjection.vertices[corner]);
        bool matched = false;
        for ( std::size_t sourceCorner = 0; sourceCorner < sourceFace.vertices.size(); ++sourceCorner )
        {
            const auto point = precision::position(sourceProjection.vertices[sourceCorner]);
            if ( precision::samePoint(projected, point) )
            {
                positions.emplace_back(destinationFace.vertices[corner],
                    targetPosition(precision::position(source.vertices[sourceFace.vertices[sourceCorner]])));
                matched = true;
                break;
            }
        }
        for ( std::size_t edge = 0; !matched && edge < sourceFace.vertices.size(); ++edge )
        {
            const auto next = (edge + 1) % sourceFace.vertices.size();
            double amount{};
            if ( !precision::onSegment(projected, precision::position(sourceProjection.vertices[edge]),
                    precision::position(sourceProjection.vertices[next]), amount) )
            {
                continue;
            }
            const auto a = precision::position(source.vertices[sourceFace.vertices[edge]]);
            const auto b = precision::position(source.vertices[sourceFace.vertices[next]]);
            amount = std::clamp(amount, 0.0, 1.0);
            positions.emplace_back(destinationFace.vertices[corner], targetPosition(TVector{
                std::lerp(a.x, b.x, amount), std::lerp(a.y, b.y, amount), std::lerp(a.z, b.z, amount)}));
            matched = true;
        }
        if ( !matched )
        {
            return false;
        }
    }
    // Only newly generated sector vertices move. Their complete shared vertex
    // pool moves together, so adjacent generated side faces retain their seam.
    for ( const auto& [index, point] : positions )
    {
        auto& vertex = destination.vertices[static_cast<std::size_t>(index)];
        vertex.x = point.x;
        vertex.y = point.y;
        vertex.z = point.z;
    }

    // A rounded source corner can be collinear in projection but not in 3D.
    // Add that authored corner to the generated boundary rather than discard
    // it or move the existing source corner onto the generated plane.
    for ( const auto sourceIndex : sourceFace.vertices )
    {
        Vertex point = source.vertices[static_cast<std::size_t>(sourceIndex)];
        const auto target = targetPosition(precision::position(point));
        point.x = target.x;
        point.y = target.y;
        point.z = target.z;
        const TVector projected = projectExtrudePoint(precision::position(point), normal, anchor);
        const auto corners = destination.surfaces[static_cast<std::size_t>(generated.surface)].vertices;
        bool present = false;
        for ( const auto index : corners )
        {
            present = present || precision::samePoint(precision::position(destination.vertices[index]), precision::position(point));
        }
        if ( present )
        {
            continue;
        }
        bool inserted = false;
        for ( std::size_t edge = 0; edge < corners.size(); ++edge )
        {
            const Vertex a = destination.vertices[corners[edge]];
            const Vertex b = destination.vertices[corners[(edge + 1) % corners.size()]];
            double amount{};
            if ( precision::onSegment(projected, projectExtrudePoint(precision::position(a), normal, anchor),
                    projectExtrudePoint(precision::position(b), normal, anchor), amount)
                && amount > 0.0 && amount < 1.0 )
            {
                inserted = insertCoordinateEdgePointIntoSector(level, generated.sector, a, b, point);
                break;
            }
        }
        if ( !inserted )
        {
            return false;
        }
    }
    recalcAndRenumberSectorSurfaces(destination);
    if ( projectedOnly )
    {
        const auto projectedDestination = projectedExtrudeFace(destination,
            destination.surfaces[static_cast<std::size_t>(generated.surface)], normal, anchor);
        return precision::surfaceBoundariesMatchChecked(sourceProjection, sourceProjection.surfaces.front(),
            projectedDestination, projectedDestination.surfaces.front());
    }
    return precision::surfaceBoundariesMatchChecked(source, sourceFace, destination,
        destination.surfaces[static_cast<std::size_t>(generated.surface)]);
}

struct ExtrudeCapUvProjection
{
    TVector origin;
    TVector uGradient;
    TVector vGradient;
    double u{};
    double v{};
    ColorF color;
    std::array<TVector, 4> originalPoints;
    std::array<TexVertex, 4> originalTuples;
};

std::optional<ExtrudeCapUvProjection> captureExtrudeCapUvProjection(
    const Sector& sector, const Surface& face, const TVector& normal, const TVector& anchor)
{
    if ( face.vertices.size() != 4 || !surfaceHasValidTextureTopology(sector, face)
        || !surfaceHasFiniteUvValues(face) )
    {
        return std::nullopt;
    }
    std::array<TVector, 4> points;
    for ( std::size_t corner = 0; corner < points.size(); ++corner )
    {
        points[corner] = projectExtrudePoint(precision::position(sector.vertices[face.vertices[corner]]), normal, anchor);
        const auto& color = face.texVertices[corner].color;
        if ( !precision::finite(points[corner]) || !std::isfinite(color.r) || !std::isfinite(color.g)
            || !std::isfinite(color.b) || !std::isfinite(color.a) || color != face.texVertices[0].color )
        {
            return std::nullopt;
        }
    }
    TVector a = points[1] - points[0];
    TVector b = points[3] - points[0];
    const double lengthA = std::hypot(a.x, a.y, a.z);
    const double lengthB = std::hypot(b.x, b.y, b.z);
    if ( !(lengthA > 0.0) || !(lengthB > 0.0) || !std::isfinite(lengthA) || !std::isfinite(lengthB) )
    {
        return std::nullopt;
    }
    a = {a.x / lengthA, a.y / lengthA, a.z / lengthA};
    b = {b.x / lengthB, b.y / lengthB, b.z / lengthB};
    const TVector area = a.cross(b);
    const double determinant = area.dot(area);
    if ( !(determinant > 0.0) || !std::isfinite(determinant) )
    {
        return std::nullopt;
    }
    const auto gradient = [&](const float start, const float first, const float second)
    {
        TVector value = b.cross(area) * ((static_cast<double>(first) - start) / lengthA)
            + area.cross(a) * ((static_cast<double>(second) - start) / lengthB);
        return TVector{value.x / determinant, value.y / determinant, value.z / determinant};
    };
    ExtrudeCapUvProjection result{points[0],
        gradient(face.texVertices[0].u, face.texVertices[1].u, face.texVertices[3].u),
        gradient(face.texVertices[0].v, face.texVertices[1].v, face.texVertices[3].v),
        face.texVertices[0].u, face.texVertices[0].v, face.texVertices[0].color, points,
        {face.texVertices[0], face.texVertices[1], face.texVertices[2], face.texVertices[3]}};
    if ( !precision::finite(result.uGradient) || !precision::finite(result.vGradient) )
    {
        return std::nullopt;
    }
    const auto rounding = [](const float value)
    {
        const double above = std::nextafter(value, std::numeric_limits<float>::infinity());
        const double below = std::nextafter(value, -std::numeric_limits<float>::infinity());
        const double upward = std::isfinite(above) ? above - value : static_cast<double>(value) - below;
        const double downward = std::isfinite(below) ? static_cast<double>(value) - below : above - value;
        return std::max(upward, downward) * 0.5;
    };
    // The fourth rectangle corner must belong to the same affine generated
    // map. Its certificate accounts only for rounding the four stored float
    // samples and evaluating the local double projection, not an arbitrary
    // UV discrepancy or an authored seam.
    const auto affineFourth = [&](const TVector& axis, const bool useU)
    {
        double floatError = 0.0;
        for ( const auto& tuple : face.texVertices )
        {
            floatError += rounding(useU ? tuple.u : tuple.v);
        }
        const double value = (useU ? result.u : result.v) + axis.dot(points[2] - result.origin);
        const double expected = useU ? face.texVertices[2].u : face.texVertices[2].v;
        const double error = floatError + precision::planeError(axis, points[2], result.origin);
        return std::isfinite(value) && std::isfinite(error) && std::abs(value - expected) <= error;
    };
    if ( !affineFourth(result.uGradient, true) || !affineFourth(result.vGradient, false) )
    {
        return std::nullopt;
    }
    return result;
}

bool applyExtrudeCapUvProjection(Sector& sector, const SurfaceRef generated,
    const std::int32_t initialFaceCount, const ExtrudeCapUvProjection& projection,
    const TVector& normal, const TVector& anchor)
{
    for ( std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex )
    {
        if ( faceIndex != static_cast<std::size_t>(generated.surface)
            && faceIndex < static_cast<std::size_t>(initialFaceCount) )
        {
            continue;
        }
        auto& face = sector.surfaces[faceIndex];
        if ( !surfaceHasValidTextureTopology(sector, face) )
        {
            return false;
        }
        for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
        {
            const TVector point = projectExtrudePoint(precision::position(sector.vertices[face.vertices[corner]]), normal, anchor);
            std::optional<std::size_t> exactOriginal;
            for ( std::size_t index = 0; index < projection.originalPoints.size(); ++index )
            {
                const auto& original = projection.originalPoints[index];
                if ( point.x == original.x && point.y == original.y && point.z == original.z )
                {
                    exactOriginal = index;
                    break;
                }
            }
            if ( exactOriginal )
            {
                face.texVertices[corner] = projection.originalTuples[*exactOriginal];
                continue;
            }
            const TVector delta = point - projection.origin;
            const double u = projection.u + projection.uGradient.dot(delta);
            const double v = projection.v + projection.vGradient.dot(delta);
            if ( !std::isfinite(u) || !std::isfinite(v) || std::abs(u) > std::numeric_limits<float>::max()
                || std::abs(v) > std::numeric_limits<float>::max() )
            {
                return false;
            }
            face.texVertices[corner] = TexVertex{static_cast<float>(u), static_cast<float>(v), projection.color};
        }
    }
    return true;
}

bool normalizeExactCollapsedExtrudeFaceImpl(const Sector& sector, Surface& face) noexcept
{
    if ( !surfaceHasValidTextureTopology(sector, face) )
    {
        return false;
    }
    for ( const auto index : face.vertices )
    {
        if ( !precision::finite(precision::position(sector.vertices[static_cast<std::size_t>(index)])) )
        {
            return false;
        }
    }
    const auto samePosition = [&](const std::size_t first, const std::size_t second)
    {
        const Vertex& a = sector.vertices[static_cast<std::size_t>(face.vertices[first])];
        const Vertex& b = sector.vertices[static_cast<std::size_t>(face.vertices[second])];
        return a.x == b.x && a.y == b.y && a.z == b.z;
    };
    const auto sameTuple = [&](const std::size_t first, const std::size_t second)
    {
        const auto& a = face.texVertices[first];
        const auto& b = face.texVertices[second];
        return a.u == b.u && a.v == b.v && a.color == b.color;
    };
    const auto eraseCorner = [&](const std::size_t corner)
    {
        face.vertices.erase(face.vertices.begin() + static_cast<std::ptrdiff_t>(corner));
        face.texVertices.erase(face.texVertices.begin() + static_cast<std::ptrdiff_t>(corner));
    };
    // Matching an inherited source corner can collapse an independently
    // clipped generated cell. Cancel only exact zero-length edges and exact
    // out-and-back paths; no proximity or shallow-turn simplification is
    // allowed, and coincident retained texture corners must agree. The helper
    // intentionally leaves the face in its current state on a later conflict;
    // the enclosing edit transaction owns rollback.
    bool changed = true;
    while ( changed && face.vertices.size() >= 2 )
    {
        changed = false;
        for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
        {
            const auto next = (corner + 1) % face.vertices.size();
            if ( samePosition(corner, next) )
            {
                if ( !sameTuple(corner, next) )
                {
                    return false;
                }
                eraseCorner(next);
                changed = true;
                break;
            }
            if ( face.vertices.size() < 3 )
            {
                continue;
            }
            const auto last = (corner + 2) % face.vertices.size();
            if ( samePosition(corner, last) )
            {
                if ( !sameTuple(corner, last) )
                {
                    return false;
                }
                eraseCorner(std::max(next, last));
                eraseCorner(std::min(next, last));
                changed = true;
                break;
            }
        }
    }
    return true;
}

bool normalizeExactCollapsedExtrudeSourceFragments(Level& level,
    const std::int32_t sourceSectorIndex, const std::int32_t initialFaceCount,
    const std::size_t initialVertexCount, CleaveProfileContext& profiles)
{
    if ( sourceSectorIndex < 0 || initialFaceCount < 0 )
    {
        return false;
    }

    const auto sourceSectorIndexValue = static_cast<std::size_t>(sourceSectorIndex);
    if ( sourceSectorIndexValue >= level.sectors.size() )
    {
        return false;
    }
    Sector& sector = level.sectors[sourceSectorIndexValue];
    if ( initialVertexCount > sector.vertices.size() )
    {
        return false;
    }
    auto profileIt = profiles.sectors.find(sourceSectorIndex);
    if ( profileIt == profiles.sectors.end()
        || profileIt->second.faces.size() != sector.surfaces.size()
        || static_cast<std::size_t>(initialFaceCount) > sector.surfaces.size()
        || (!profileIt->second.validateFaces.empty()
            && profileIt->second.validateFaces.size() != sector.surfaces.size()) )
    {
        return false;
    }

    if ( sector.vertices.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) )
    {
        return false;
    }

    // Keep authored vertex identities intact. An appended point can inherit
    // one authored identity only when its stored coordinates are exactly the
    // same. Multiple authored matches are ambiguous and must abort the
    // transaction rather than selecting an arbitrary index.
    std::vector<std::int32_t> representatives(sector.vertices.size(), -1);
    for ( std::size_t index = 0; index < initialVertexCount; ++index )
    {
        if ( !precision::finite(precision::position(sector.vertices[index])) )
        {
            return false;
        }
        representatives[index] = static_cast<std::int32_t>(index);
    }
    for ( std::size_t index = initialVertexCount; index < sector.vertices.size(); ++index )
    {
        const auto& point = sector.vertices[index];
        if ( !precision::finite(precision::position(point)) )
        {
            return false;
        }
        std::optional<std::int32_t> authoredMatch;
        for ( std::size_t authored = 0; authored < initialVertexCount; ++authored )
        {
            const auto& original = sector.vertices[authored];
            if ( point.x != original.x || point.y != original.y || point.z != original.z )
            {
                continue;
            }
            if ( authoredMatch )
            {
                return false;
            }
            authoredMatch = static_cast<std::int32_t>(authored);
        }
        if ( authoredMatch )
        {
            representatives[index] = *authoredMatch;
            continue;
        }
        representatives[index] = static_cast<std::int32_t>(index);
        for ( std::size_t earlier = initialVertexCount; earlier < index; ++earlier )
        {
            const auto& prior = sector.vertices[earlier];
            if ( point.x == prior.x && point.y == prior.y && point.z == prior.z )
            {
                representatives[index] = representatives[earlier];
                break;
            }
        }
    }

    const std::size_t faceCount = sector.surfaces.size();
    std::vector<bool> normalizeFaces(faceCount, false);
    std::vector<bool> distinctAppendedReferences(faceCount, false);
    for ( std::size_t faceIndex = 0; faceIndex < faceCount; ++faceIndex )
    {
        auto& face = sector.surfaces[faceIndex];
        if ( !surfaceHasValidTextureTopology(sector, face) )
        {
            return false;
        }
        for ( auto& index : face.vertices )
        {
            const auto originalIndex = index;
            if ( originalIndex < 0 || static_cast<std::size_t>(originalIndex) >= representatives.size()
                || representatives[static_cast<std::size_t>(originalIndex)] < 0 )
            {
                return false;
            }
            const bool appended = static_cast<std::size_t>(originalIndex) >= initialVertexCount;
            normalizeFaces[faceIndex] = normalizeFaces[faceIndex] || appended;
            if ( appended && representatives[static_cast<std::size_t>(originalIndex)] == originalIndex )
            {
                distinctAppendedReferences[faceIndex] = true;
            }
            index = representatives[static_cast<std::size_t>(originalIndex)];
        }
        normalizeFaces[faceIndex] = normalizeFaces[faceIndex]
            || faceIndex >= static_cast<std::size_t>(initialFaceCount);
    }

    for ( std::size_t faceIndex = sector.surfaces.size();
          faceIndex-- > static_cast<std::size_t>(initialFaceCount); )
    {
        auto& face = sector.surfaces[faceIndex];
        if ( !normalizeExactCollapsedExtrudeFaceImpl(sector, face) )
        {
            return false;
        }
        if ( face.vertices.size() >= 3 )
        {
            face.normal = surfaceCalcNormalNewell(sector, face);
            if ( !precision::finite(face.normal) )
            {
                return false;
            }
            continue;
        }
        // Appended source fragments may be removed only while they are still
        // unadjoined. A paired portal must remain a paired portal, so leave
        // that case to the enclosing transaction's fail-closed path. Check
        // incoming references as well; clearing a one-way reference would
        // otherwise hide malformed topology during the cleanup.
        const SurfaceRef removed{sourceSectorIndex, static_cast<std::int32_t>(faceIndex)};
        const bool hasIncomingAdjoin = std::any_of(
            level.sectors.begin(), level.sectors.end(),
            [&removed](const Sector& otherSector)
            {
                return std::any_of(
                    otherSector.surfaces.begin(), otherSector.surfaces.end(),
                    [&removed](const Surface& otherFace)
                    {
                        return otherFace.adjoin && *otherFace.adjoin == removed;
                    });
            });
        if ( face.adjoin || hasIncomingAdjoin )
        {
            return false;
        }
        sector.surfaces.erase(sector.surfaces.begin() + static_cast<std::ptrdiff_t>(faceIndex));
        profileIt->second.faces.erase(profileIt->second.faces.begin() + static_cast<std::ptrdiff_t>(faceIndex));
        if ( !profileIt->second.validateFaces.empty() )
        {
            profileIt->second.validateFaces.erase(
                profileIt->second.validateFaces.begin() + static_cast<std::ptrdiff_t>(faceIndex));
        }
        shiftAdjoinsAfterSurfaceDelete(level, removed);
        if ( profiles.cogReferences )
        {
            profiles.cogReferences->eraseSurface(removed);
        }
    }

    // An authored surface slot can become a descendant while clipping adds
    // an intersection vertex to it. Apply the same exact aliases to that slot
    // before validating its shared edge users, but preserve its cached normal
    // when the alias changed only an index and removed no corner.
    for ( std::size_t faceIndex = 0;
          faceIndex < static_cast<std::size_t>(initialFaceCount) && faceIndex < sector.surfaces.size(); ++faceIndex )
    {
        if ( !normalizeFaces[faceIndex] )
        {
            continue;
        }
        auto& face = sector.surfaces[faceIndex];
        const auto originalCornerCount = face.vertices.size();
        if ( !normalizeExactCollapsedExtrudeFaceImpl(sector, face)
            || face.vertices.size() < 3 )
        {
            return false;
        }
        if ( distinctAppendedReferences[faceIndex]
            || face.vertices.size() != originalCornerCount )
        {
            face.normal = surfaceCalcNormalNewell(sector, face);
            if ( !precision::finite(face.normal) )
            {
                return false;
            }
        }
    }

    // Deletions occur only in the appended range, so leave authored surface
    // numbers untouched. Renumber the descendants whose indices did shift.
    for ( std::size_t faceIndex = static_cast<std::size_t>(initialFaceCount);
          faceIndex < sector.surfaces.size(); ++faceIndex )
    {
        sector.surfaces[faceIndex].num = static_cast<std::int32_t>(faceIndex);
    }
    for ( std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex )
    {
        const auto& face = sector.surfaces[faceIndex];
        if ( !face.adjoin )
        {
            continue;
        }
        if ( !isValidSurfaceRef(level, *face.adjoin) )
        {
            return false;
        }
        const auto& partnerSector = level.sectors[static_cast<std::size_t>(face.adjoin->sector)];
        const auto& partner = partnerSector.surfaces[static_cast<std::size_t>(face.adjoin->surface)];
        if ( !partner.adjoin || *partner.adjoin != SurfaceRef{sourceSectorIndex,
                static_cast<std::int32_t>(faceIndex)}
            || !surfaceBoundariesMatchWithResolvedNormals(sector, face, partnerSector, partner) )
        {
            return false;
        }
    }
    return true;
}

bool normalizeCollapsedExtrudeCaps(Level& level, const SurfaceRef generated,
    const std::int32_t initialFaceCount, CleaveProfileContext& profiles)
{
    Sector& sector = level.sectors[static_cast<std::size_t>(generated.sector)];
    auto& faceProfiles = profiles.sectors.at(generated.sector).faces;
    if ( faceProfiles.size() != sector.surfaces.size() )
    {
        return false;
    }
    // Lifting resolves several generated corners from immutable source
    // identities. A rounded construction can consequently leave two vertex
    // indices with the same exact coordinates. Keep the vertex pool and every
    // corner tuple intact, but use one representative index for those exact
    // aliases throughout this generated sector before removing a zero edge.
    // No near-point comparison is allowed here: distinct representable
    // geometry remains distinct and is handled by the regular validation.
    std::vector<std::int32_t> representatives(sector.vertices.size(), -1);
    for ( std::size_t index = 0; index < sector.vertices.size(); ++index )
    {
        const auto& point = sector.vertices[index];
        if ( !precision::finite(precision::position(point)) )
        {
            return false;
        }
        representatives[index] = static_cast<std::int32_t>(index);
        for ( std::size_t earlier = 0; earlier < index; ++earlier )
        {
            const auto& prior = sector.vertices[earlier];
            if ( point.x == prior.x && point.y == prior.y && point.z == prior.z )
            {
                representatives[index] = representatives[earlier];
                break;
            }
        }
    }
    for ( auto& face : sector.surfaces )
    {
        if ( !surfaceHasValidTextureTopology(sector, face) )
        {
            return false;
        }
        for ( auto& index : face.vertices )
        {
            index = representatives[static_cast<std::size_t>(index)];
        }
    }
    for ( std::size_t faceIndex = sector.surfaces.size(); faceIndex-- > 0; )
    {
        const bool initialGeneratedSide = faceIndex >= 2
            && faceIndex < static_cast<std::size_t>(initialFaceCount);
        if ( faceIndex != static_cast<std::size_t>(generated.surface)
            && !initialGeneratedSide && faceIndex < static_cast<std::size_t>(initialFaceCount) )
        {
            continue;
        }
        Surface& face = sector.surfaces[faceIndex];
        if ( !surfaceHasValidTextureTopology(sector, face) )
        {
            return false;
        }
        if ( !detail::normalizeExactCollapsedExtrudeFace(sector, face) )
        {
            return false;
        }
        if ( face.vertices.size() >= 3 )
        {
            face.normal = surfaceCalcNormalNewell(sector, face);
            continue;
        }
        if ( faceIndex < static_cast<std::size_t>(initialFaceCount) || face.adjoin )
        {
            // The original generated cap keeps its identity, and a partnered
            // cell cannot disappear without losing its corresponding portal.
            return false;
        }
        const SurfaceRef removed{generated.sector, static_cast<std::int32_t>(faceIndex)};
        sector.surfaces.erase(sector.surfaces.begin() + static_cast<std::ptrdiff_t>(faceIndex));
        faceProfiles.erase(faceProfiles.begin() + static_cast<std::ptrdiff_t>(faceIndex));
        shiftAdjoinsAfterSurfaceDelete(level, removed);
        if ( profiles.cogReferences )
        {
            profiles.cogReferences->eraseSurface(removed);
        }
    }
    renumberSectorSurfaces(sector);
    // A generated face may already carry a portal when this routine is used
    // by the public connect path. Its reciprocal reference and geometric
    // boundary must survive any exact duplicate removal as one transaction.
    for ( std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex )
    {
        const auto& face = sector.surfaces[faceIndex];
        if ( !face.adjoin )
        {
            continue;
        }
        if ( !isValidSurfaceRef(level, *face.adjoin) )
        {
            return false;
        }
        const auto& partnerSector = level.sectors[static_cast<std::size_t>(face.adjoin->sector)];
        const auto& partner = partnerSector.surfaces[static_cast<std::size_t>(face.adjoin->surface)];
        if ( partner.adjoin != std::optional<SurfaceRef>{{generated.sector, static_cast<std::int32_t>(faceIndex)}}
            || !surfaceBoundariesMatchWithResolvedNormals(sector, face, partnerSector, partner) )
        {
            return false;
        }
    }
    return true;
}

bool partitionExtrudeFootprint(Level& level, const SurfaceRef ref, const TVector& normal,
    CleaveProfileContext& profiles)
{
    Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    const Surface original = sector.surfaces[static_cast<std::size_t>(ref.surface)];
    std::vector<std::size_t> remaining;
    for ( std::size_t corner = 0; corner < original.vertices.size(); ++corner )
    {
        remaining.push_back(corner);
    }
    std::vector<std::array<std::size_t, 3>> triangles;
    const auto point = [&](const std::size_t corner)
    {
        return precision::position(sector.vertices[original.vertices[corner]]);
    };
    const auto turn = [&](const TVector& a, const TVector& b, const TVector& c)
    {
        return (b - a).cross(c - b).dot(normal);
    };
    while ( remaining.size() > 3 )
    {
        bool clipped = false;
        for ( std::size_t index = 0; index < remaining.size(); ++index )
        {
            const std::array<std::size_t, 3> ear{{remaining[(index + remaining.size() - 1) % remaining.size()],
                remaining[index], remaining[(index + 1) % remaining.size()]}};
            const TVector a = point(ear[0]), b = point(ear[1]), c = point(ear[2]);
            if ( turn(a, b, c) <= precision::cornerTurnError(a, b, c, normal) )
            {
                continue;
            }
            bool containsCorner = false;
            for ( const auto corner : remaining )
            {
                if ( corner == ear[0] || corner == ear[1] || corner == ear[2] )
                {
                    continue;
                }
                const TVector p = point(corner);
                if ( turn(a, b, p) >= -precision::cornerTurnError(a, b, p, normal)
                    && turn(b, c, p) >= -precision::cornerTurnError(b, c, p, normal)
                    && turn(c, a, p) >= -precision::cornerTurnError(c, a, p, normal) )
                {
                    containsCorner = true;
                    break;
                }
            }
            if ( containsCorner )
            {
                continue;
            }
            triangles.push_back(ear);
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(index));
            clipped = true;
            break;
        }
        if ( !clipped )
        {
            return false;
        }
    }
    if ( remaining.size() != 3 )
    {
        return false;
    }
    triangles.push_back({remaining[0], remaining[1], remaining[2]});
    if ( triangles.size() - 1 > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) - sector.surfaces.size() )
    {
        return false;
    }
    bool first = true;
    for ( const auto& triangle : triangles )
    {
        Surface part = original;
        part.vertices.clear();
        part.texVertices.clear();
        for ( const auto corner : triangle )
        {
            part.vertices.push_back(original.vertices[corner]);
            part.texVertices.push_back(original.texVertices[corner]);
        }
        part.normal = surfaceCalcNormalNewell(sector, part);
        if ( !surfaceIsConvex(sector, part) || !surfaceIsPlanar(sector, part) )
        {
            return false;
        }
        if ( first )
        {
            sector.surfaces[static_cast<std::size_t>(ref.surface)] = std::move(part);
            first = false;
        }
        else
        {
            part.num = static_cast<std::int32_t>(sector.surfaces.size());
            profiles.splitFace(ref, {ref.sector, part.num});
            sector.surfaces.push_back(std::move(part));
        }
    }
    return true;
}

struct ExtrudeProjectedArea
{
    long double value{};
    long double error{};
};

bool extrudeFragmentLimits(const Level& level, const std::int32_t source, const std::int32_t generated) noexcept
{
    const std::size_t maximumCorners = level.kind == ProjectType::IJIM ? 64 : 24;
    for ( const auto owner : {source, generated} )
    {
        const auto& sector = level.sectors[static_cast<std::size_t>(owner)];
        if ( sector.surfaces.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
            || sector.vertices.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) )
        {
            return false;
        }
        for ( const auto& face : sector.surfaces )
        {
            if ( face.vertices.size() > maximumCorners )
            {
                return false;
            }
        }
    }
    return true;
}

ExtrudeProjectedArea extrudeProjectedArea(const std::vector<TVector>& points,
    const TVector& normal, const TVector& coordinateError) noexcept
{
    ExtrudeProjectedArea result;
    if ( points.size() < 3 )
    {
        return result;
    }
    const auto origin = points.front();
    const std::array<long double, 3> n{{normal.x, normal.y, normal.z}};
    const std::array<long double, 3> e{{coordinateError.x, coordinateError.y, coordinateError.z}};
    for ( std::size_t corner = 1; corner + 1 < points.size(); ++corner )
    {
        const auto av = points[corner] - origin, bv = points[corner + 1] - origin;
        const std::array<long double, 3> a{{av.x, av.y, av.z}}, b{{bv.x, bv.y, bv.z}};
        for ( std::size_t component = 0; component < 3; ++component )
        {
            const auto j = (component + 1) % 3, k = (component + 2) % 3;
            result.value += n[component] * (a[j] * b[k] - a[k] * b[j]);
            result.error += std::abs(n[component]) * (precision::kRoundoff *
                (std::abs(a[j] * b[k]) + std::abs(a[k] * b[j]))
                + 2 * e[j] * (std::abs(a[k]) + std::abs(b[k]))
                + 2 * e[k] * (std::abs(a[j]) + std::abs(b[j])) + 8 * e[j] * e[k]);
        }
    }
    result.value = std::abs(result.value) / 2;
    result.error /= 2;
    return result;
}

std::vector<TVector> extrudeProjectedLoop(const Sector& sector, const Surface& face,
    const TVector& normal, const TVector& anchor)
{
    std::vector<TVector> points;
    for ( const auto index : face.vertices )
    {
        points.push_back(projectExtrudePoint(precision::position(sector.vertices[index]), normal, anchor));
    }
    return points;
}

ExtrudeProjectedArea extrudeIntersectionArea(std::vector<TVector> subject,
    const std::vector<TVector>& rectangle, const TVector& normal, const TVector& coordinateError)
{
    // The rectangle is opposite-wound to the source. Signed clipping retains
    // the source polygon's winding; zero-area bridges between disconnected
    // components cancel in the boundary area sum, including reflex inputs.
    for ( std::size_t edge = 0; edge < rectangle.size() && !subject.empty(); ++edge )
    {
        const auto anchor = rectangle[edge];
        const auto plane = (rectangle[(edge + 1) % rectangle.size()] - anchor).cross(normal);
        std::vector<TVector> output;
        for ( std::size_t corner = 0; corner < subject.size(); ++corner )
        {
            const auto a = subject[corner], b = subject[(corner + 1) % subject.size()];
            const double da = plane.dot(a - anchor), db = plane.dot(b - anchor);
            const bool insideA = da >= 0.0, insideB = db >= 0.0;
            if ( insideA )
            {
                output.push_back(a);
            }
            if ( insideA != insideB )
            {
                const double amount = da / (da - db);
                if ( !std::isfinite(amount) )
                {
                    return {0, std::numeric_limits<long double>::infinity()};
                }
                output.push_back({std::lerp(a.x, b.x, amount), std::lerp(a.y, b.y, amount), std::lerp(a.z, b.z, amount)});
            }
        }
        subject = std::move(output);
    }
    return extrudeProjectedArea(subject, normal, coordinateError);
}

bool extrudeProjectedBoundsOverlap(const Sector& firstOwner, const Surface& first,
    const Sector& secondOwner, const Surface& second, const TVector& normal,
    const TVector& anchor, const TVector& error) noexcept
{
    const auto bounds = [&](const Sector& owner, const Surface& face,
                            std::array<double, 3>& minimum, std::array<double, 3>& maximum)
    {
        minimum.fill(std::numeric_limits<double>::infinity());
        maximum.fill(-std::numeric_limits<double>::infinity());
        for ( const auto index : face.vertices )
        {
            if ( index < 0 || static_cast<std::size_t>(index) >= owner.vertices.size() )
            {
                return false;
            }
            const auto point = projectExtrudePoint(precision::position(owner.vertices[index]), normal, anchor);
            if ( !precision::finite(point) )
            {
                return false;
            }
            const std::array<double, 3> components{{point.x, point.y, point.z}};
            for ( std::size_t axis = 0; axis < 3; ++axis )
            {
                minimum[axis] = std::min(minimum[axis], components[axis]);
                maximum[axis] = std::max(maximum[axis], components[axis]);
            }
        }
        return !face.vertices.empty();
    };
    std::array<double, 3> firstMinimum, firstMaximum, secondMinimum, secondMaximum;
    if ( !bounds(firstOwner, first, firstMinimum, firstMaximum)
        || !bounds(secondOwner, second, secondMinimum, secondMaximum) )
    {
        return false;
    }
    const std::array<double, 3> projectionError{{error.x, error.y, error.z}};
    for ( std::size_t axis = 0; axis < 3; ++axis )
    {
        const auto separated = [&](const double maximum, const double minimum)
        {
            const double allowance = 2 * projectionError[axis]
                + precision::kRoundoff * (std::abs(maximum) + std::abs(minimum));
            return minimum - maximum > allowance;
        };
        if ( separated(firstMaximum[axis], secondMinimum[axis])
            || separated(secondMaximum[axis], firstMinimum[axis]) )
        {
            return false;
        }
    }
    return true;
}

bool liftExtrudeArrangementPairs(Level& level,
    const std::vector<std::pair<SurfaceRef,SurfaceRef>>& pairs,
    detail::ExtrudeArrangementContext& arrangement)
{
    // First retain every original source corner in its paired generated loop.
    // Incidence and interval membership use immutable nodes, never the current
    // lifted mesh. Shared generated edge users receive the same exact index.
    for ( const auto& [sourceRef,generatedRef] : pairs )
    {
        const auto& source = level.sectors[sourceRef.sector];
        auto& generated = level.sectors[generatedRef.sector];
        for ( const auto sourceIndex : source.surfaces[sourceRef.surface].vertices )
        {
            const auto id = arrangement.node(sourceRef.sector,sourceIndex);
            const auto corners = generated.surfaces[generatedRef.surface].vertices;
            if ( std::any_of(corners.begin(),corners.end(),[&](const auto index)
                {
                    return arrangement.node(generatedRef.sector,index) == id;
                }) )
            {
                continue;
            }
            bool inserted = false;
            for ( std::size_t edge = 0; edge < corners.size(); ++edge )
            {
                const auto a = corners[edge], b = corners[(edge+1)%corners.size()];
                if ( !arrangement.interiorNodeOnEdge(generatedRef,a,b,id) )
                {
                    continue;
                }
                const auto point = arrangement.sourcePosition(sourceRef,id);
                if ( !point )
                {
                    return false;
                }
                const auto index = arrangement.materializeNode(generated,generatedRef.sector,id,*point);
                const Vertex first = generated.vertices[a], second = generated.vertices[b];
                const Vertex added = generated.vertices[index];
                for ( auto& face : generated.surfaces )
                {
                    if ( !surfaceContainsIndexedEdge(face,a,b) )
                    {
                        continue;
                    }
                    if ( std::find(face.vertices.begin(),face.vertices.end(),index) != face.vertices.end() )
                    {
                        return false;
                    }
                    if ( !insertCutVertexIntoMatchingEdges(
                        level, generated, face, a, b, first, second, added, index) )
                    {
                        return false;
                    }
                }
                inserted = true;
                break;
            }
            if ( !inserted )
            {
                return false;
            }
        }
    }
    // Resolve every shared generated node once from the immutable original
    // source triangle/edge height. No later pair can change an earlier pair's
    // position through a fresh interpolation of rounded descendant endpoints.
    for ( const auto& [sourceRef,generatedRef] : pairs )
    {
        auto& generated = level.sectors[generatedRef.sector];
        for ( const auto index : generated.surfaces[generatedRef.surface].vertices )
        {
            const auto point = arrangement.sourcePosition(sourceRef,arrangement.node(generatedRef.sector,index));
            if ( !point )
            {
                return false;
            }
            auto& vertex = generated.vertices[index];
            vertex.x = point->x;
            vertex.y = point->y;
            vertex.z = point->z;
        }
    }
    std::vector<std::int32_t> refreshedOwners;
    for ( const auto& [sourceRef,generatedRef] : pairs )
    {
        (void)sourceRef;
        if ( std::find(refreshedOwners.begin(),refreshedOwners.end(),generatedRef.sector) == refreshedOwners.end() )
        {
            recalcAndRenumberSectorSurfaces(level.sectors[generatedRef.sector]);
            refreshedOwners.push_back(generatedRef.sector);
        }
    }
    for ( const auto& [sourceRef,generatedRef] : pairs )
    {
        auto& source = level.sectors[sourceRef.sector];
        auto& generated = level.sectors[generatedRef.sector];
        auto& face = generated.surfaces[generatedRef.surface];
        face.normal = surfaceCalcNormalNewell(generated,face);
        if ( !precision::surfaceBoundariesMatchChecked(source,source.surfaces[sourceRef.surface],generated,face) )
        {
            return false;
        }
    }
    for ( const auto& [sourceRef,generatedRef] : pairs )
    {
        if ( !levelSysAdjoinSurfaces(level,sourceRef,generatedRef,true) )
        {
            return false;
        }
    }
    return true;
}

bool connectPartitionedExtrudeFootprint(Level& candidate, const SurfaceRef original, const SurfaceRef generated,
    const std::int32_t sourceCount, const std::int32_t destinationCount, const TVector& normal,
    const TVector& anchor, const TVector& projectionError, CleaveProfileContext& profiles)
{
    if ( !extrudeFragmentLimits(candidate, original.sector, generated.sector) )
    {
        return false;
    }
    const auto sourceLoop = extrudeProjectedLoop(candidate.sectors[original.sector],
        candidate.sectors[original.sector].surfaces[original.surface], normal, anchor);
    const auto rectangle = extrudeProjectedLoop(candidate.sectors[generated.sector],
        candidate.sectors[generated.sector].surfaces[generated.surface], normal, anchor);
    const auto expected = extrudeIntersectionArea(sourceLoop, rectangle, normal, projectionError);
    if ( !std::isfinite(expected.value) || !std::isfinite(expected.error) || expected.value <= expected.error
        || !partitionExtrudeFootprint(candidate, original, normal, profiles) )
    {
        return false;
    }

    detail::ExtrudeArrangementRegistry registry(normal,anchor);
    detail::ExtrudeArrangementContext arrangement(registry,original.sector,normal,anchor);
    if ( !registry.valid() )
    {
        return false;
    }
    for ( const auto ref : collectSurfaceCandidates(candidate,original.sector,original.surface,sourceCount) )
    {
        if ( !arrangement.addCell(candidate,ref) )
        {
            return false;
        }
    }
    if ( !arrangement.addCell(candidate,generated) )
    {
        return false;
    }
    if ( !registry.initializeOriginalIncidences() )
    {
        return false;
    }

    // Keep the generated pool in its original projected plane until the whole
    // common partition is known. Lifting a paired cell earlier would turn its
    // reconstructed corner positions into the next cell's clipping operands.
    // Pending pair identities are sidecars, not partially published adjoins.
    std::vector<std::int32_t> affected;
    affected.reserve(profiles.sectors.size());
    for ( const auto& [index,profile] : profiles.sectors )
    {
        (void)profile;
        affected.push_back(index);
    }
    std::vector<std::pair<SurfaceRef, SurfaceRef>> pending;
    const auto paired = [&](const SurfaceRef ref, const bool source)
    {
        return std::any_of(pending.begin(), pending.end(), [&](const auto& pair)
        {
            const auto& item = source ? pair.first : pair.second;
            return item.sector == ref.sector && item.surface == ref.surface;
        });
    };
    // Restart after each accepted subdivision so appended cells participate.
    // No original corner moves and no descendant supplies a new error budget.
    // A triangulated n-corner polygon has n boundary and n-3 diagonal lines.
    // Adding the rectangle lines bounds their planar arrangement's cells.
    const std::size_t lineCount = 2 * sourceLoop.size() - 3 + rectangle.size();
    const std::size_t maximumCells = 1 + lineCount * (lineCount + 1) / 2;
    for ( std::size_t pass = 0; pass <= maximumCells; ++pass )
    {
        bool progress = false;
        const auto sources = collectSurfaceCandidates(candidate, original.sector, original.surface, sourceCount);
        const auto destinations = collectSurfaceCandidates(candidate, generated.sector, generated.surface, destinationCount);
        for ( const auto a : sources )
        {
            if ( candidate.sectors[a.sector].surfaces[a.surface].adjoin || paired(a, true) )
            {
                continue;
            }
            for ( const auto b : destinations )
            {
                if ( candidate.sectors[b.sector].surfaces[b.surface].adjoin || paired(b, false) )
                {
                    continue;
                }
                // Reject only provably disjoint projected bounds before
                // allocating trial geometry. Touching or uncertain intervals
                // retain the complete clipping and correspondence checks.
                if ( !extrudeProjectedBoundsOverlap(candidate.sectors[a.sector], candidate.sectors[a.sector].surfaces[a.surface],
                        candidate.sectors[b.sector], candidate.sectors[b.sector].surfaces[b.surface], normal, anchor, projectionError) )
                {
                    continue;
                }
                CleaveSectorRollback rollback(candidate,affected);
                const auto* firstBoundary = rollback.originalSector(a.sector);
                const auto* secondBoundary = rollback.originalSector(b.sector);
                if ( !firstBoundary || !secondBoundary )
                {
                    return false;
                }
                Level& trial = candidate;
                CleaveProfileContext trialProfiles = profiles;
                auto trialArrangement = arrangement;
                const auto firstCount = static_cast<std::int32_t>(trial.sectors[a.sector].surfaces.size());
                const auto secondCount = static_cast<std::int32_t>(trial.sectors[b.sector].surfaces.size());
                if ( !clipExtrudeArrangementBoundary(trial,b,a,
                        firstBoundary->surfaces[a.surface],normal,trialProfiles,trialArrangement)
                    || !clipExtrudeArrangementBoundary(trial,a,b,
                        secondBoundary->surfaces[b.surface],-normal,trialProfiles,trialArrangement) )
                {
                    continue;
                }
                const auto firstParts = collectSurfaceCandidates(trial, a.sector, a.surface, firstCount);
                const auto secondParts = collectSurfaceCandidates(trial, b.sector, b.surface, secondCount);
                for ( const auto first : firstParts )
                {
                    for ( const auto second : secondParts )
                    {
                        const auto& firstOwner = trial.sectors[first.sector];
                        const auto& secondOwner = trial.sectors[second.sector];
                        const auto sourceProjection = projectedExtrudeArrangementFace(firstOwner,first,trialArrangement,normal);
                        const auto destinationProjection = projectedExtrudeArrangementFace(secondOwner,second,trialArrangement,-normal);
                        if ( !precision::surfaceBoundariesMatchChecked(sourceProjection,sourceProjection.surfaces[0],
                                destinationProjection,destinationProjection.surfaces[0]) )
                        {
                            continue;
                        }
                        // Both cells now refer to the same immutable line-pair
                        // nodes. No descendant endpoint is moved or promoted
                        // into an operand for a later clipping plane.
                        if ( !extrudeFragmentLimits(trial, original.sector, generated.sector) )
                        {
                            return false;
                        }
                        pending.emplace_back(first, second);
                        static_assert(std::is_nothrow_move_assignable_v<CleaveProfileContext>);
                        static_assert(std::is_nothrow_move_assignable_v<detail::ExtrudeArrangementContext>);
                        profiles = std::move(trialProfiles);
                        arrangement = std::move(trialArrangement);
                        rollback.commit();
                        progress = true;
                        break;
                    }
                    if ( progress )
                    {
                        break;
                    }
                }
                if ( progress )
                {
                    break;
                }
            }
            if ( progress )
            {
                break;
            }
        }
        if ( !progress )
        {
            ExtrudeProjectedArea joined;
            for ( const auto& [ref, partner] : pending )
            {
                const auto& face = candidate.sectors[ref.sector].surfaces[ref.surface];
                const auto area = extrudeProjectedArea(extrudeProjectedLoop(candidate.sectors[ref.sector], face, normal, anchor),
                    normal, projectionError);
                joined.value += area.value;
                joined.error += area.error;
            }
            if ( !std::isfinite(joined.value) || !std::isfinite(joined.error)
                || std::abs(joined.value - expected.value) > joined.error + expected.error )
            {
                return false;
            }
            return liftExtrudeArrangementPairs(candidate,pending,arrangement);
        }
    }
    return false;
}
}

bool detail::normalizeExactCollapsedExtrudeFace(const Sector& sector, Surface& face) noexcept
{
    return normalizeExactCollapsedExtrudeFaceImpl(sector, face);
}

bool precision::extrudeSourceContextIsValid(const Level& level, const SurfaceRef source)
{
    if ( !isValidUvCoordinateSpace(level.uvCoordinateSpace) || !isValidSurfaceRef(level, source) )
    {
        return false;
    }
    const auto affected = collectCleaveAffectedSectors(level, source.sector);
    CleaveProfileContext profiles;
    return cleaveAffectedSectorsHaveFiniteUvValues(level, affected)
        && profiles.capture(level, affected, false, false)
        && validateCleaveAffectedSectors(level, affected, &profiles).error == SectorCleaveBatchError::None;
}

namespace
{

// An expanded extrusion starts with a rectangular generated cap.  When the
// source footprint is strictly contained by that rectangle, retain the
// source boundary as one portal and triangulate only the annulus.  The
// partitioned fallback remains responsible for cases where the rectangle is
// not a certified containing footprint.
bool tryContainedExtrudeConnection(
    Level& level,
    const SurfaceRef original,
    const SurfaceRef generated,
    const TVector& normal,
    const TVector& anchor,
    const std::int32_t destinationCount,
    const ExtrudeCapUvProjection& generatedUv,
    const std::vector<std::int32_t>& affected,
    const CleaveProfileContext& profiles,
    const precision::ExtrudeConnectionValidator validator,
    void* const validatorContext)
{
    // The generated cap convention used by ExtrudeResultValidation treats
    // face zero as the source-plane cap and the remaining initial faces as
    // the outer cap and prism sides.  A public caller with another face
    // layout must use the existing connection paths instead.
    if ( generated.surface != 0 || !isValidSurfaceRef(level, original)
        || !isValidSurfaceRef(level, generated) )
    {
        return false;
    }

    const Sector& inputSource = level.sectors[static_cast<std::size_t>(original.sector)];
    const Sector& inputGenerated = level.sectors[static_cast<std::size_t>(generated.sector)];
    const Surface& inputSourceFace = inputSource.surfaces[static_cast<std::size_t>(original.surface)];
    const Surface& inputRectangle = inputGenerated.surfaces[static_cast<std::size_t>(generated.surface)];
    if ( inputSourceFace.vertices.size() < 3 || inputRectangle.vertices.size() != 4
        || !surfaceHasValidTextureTopology(inputSource, inputSourceFace)
        || !surfaceHasValidTextureTopology(inputGenerated, inputRectangle) )
    {
        return false;
    }

    // The contained-ring kernel needs a stable 2D representation of the
    // immutable world coordinates. Drop the component whose magnitude is
    // largest in the already unit-scaled source normal. This preserves exact
    // collinearity in the two stored coordinates; an oblique orthogonal
    // projection would manufacture a tiny bend through cancellation.
    if ( !precision::finite(normal) )
    {
        return false;
    }
    std::size_t droppedAxis = 0;
    if ( std::abs(normal.y) > std::abs(normal.x) )
    {
        droppedAxis = 1;
    }
    const double droppedMagnitude = droppedAxis == 0 ? std::abs(normal.x)
        : (droppedAxis == 1 ? std::abs(normal.y) : std::abs(normal.z));
    if ( std::abs(normal.z) > droppedMagnitude )
    {
        droppedAxis = 2;
    }
    const double selectedMagnitude = droppedAxis == 0 ? std::abs(normal.x)
        : (droppedAxis == 1 ? std::abs(normal.y) : std::abs(normal.z));
    if ( selectedMagnitude == 0.0 )
    {
        return false;
    }
    const auto projectTo2D = [droppedAxis](const TVector& point) -> std::optional<TPoint2>
    {
        if ( !precision::finite(point) )
        {
            return std::nullopt;
        }
        TPoint2 result{};
        if ( droppedAxis == 0 )
        {
            result = {point.y, point.z};
        }
        else if ( droppedAxis == 1 )
        {
            result = {point.x, point.z};
        }
        else
        {
            result = {point.x, point.y};
        }
        if ( !std::isfinite(result.x) || !std::isfinite(result.y) )
        {
            return std::nullopt;
        }
        return result;
    };

    std::vector<detail::ContainedRingPoint> sourceContour;
    sourceContour.reserve(inputSourceFace.vertices.size());
    for ( const auto index : inputSourceFace.vertices )
    {
        if ( index < 0 || static_cast<std::size_t>(index) >= inputSource.vertices.size() )
        {
            return false;
        }
        const auto point = projectTo2D(
            precision::position(inputSource.vertices[static_cast<std::size_t>(index)]));
        if ( !point )
        {
            return false;
        }
        sourceContour.push_back({*point, static_cast<std::size_t>(index)});
    }

    std::vector<detail::ContainedRingPoint> rectangleContour;
    rectangleContour.reserve(inputRectangle.vertices.size());
    for ( const auto index : inputRectangle.vertices )
    {
        if ( index < 0 || static_cast<std::size_t>(index) >= inputGenerated.vertices.size() )
        {
            return false;
        }
        const auto point = projectTo2D(
            precision::position(inputGenerated.vertices[static_cast<std::size_t>(index)]));
        if ( !point )
        {
            return false;
        }
        rectangleContour.push_back({*point, static_cast<std::size_t>(index)});
    }

    detail::ContainedRingResult ring;
    if ( !detail::buildContainedExtrudeRing(sourceContour, rectangleContour, ring) )
    {
        return false;
    }

    Level candidate = level;
    CleaveProfileContext trialProfiles = profiles;
    std::vector<std::int32_t> trialAffected = affected;
    Sector& sourceSector = candidate.sectors[static_cast<std::size_t>(original.sector)];
    Sector& generatedSector = candidate.sectors[static_cast<std::size_t>(generated.sector)];
    const Surface sourceFace = sourceSector.surfaces[static_cast<std::size_t>(original.surface)];
    const Surface rectangleFace = generatedSector.surfaces[static_cast<std::size_t>(generated.surface)];

    if ( sourceFace.vertices.size() != sourceContour.size()
        || rectangleFace.vertices.size() != rectangleContour.size()
        || sourceFace.vertices.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
        || generatedSector.vertices.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
            - sourceFace.vertices.size()
        || generatedSector.surfaces.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
        || ring.triangles.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
            - generatedSector.surfaces.size() )
    {
        return false;
    }

    // Materialize one exact source XYZ/mark copy per source contour identity.
    // The source sector itself remains untouched until the portal is
    // connected in the isolated candidate.
    std::vector<std::int32_t> sourceVertices;
    sourceVertices.reserve(sourceFace.vertices.size());
    for ( const auto index : sourceFace.vertices )
    {
        const Vertex& point = sourceSector.vertices[static_cast<std::size_t>(index)];
        generatedSector.vertices.push_back(point);
        sourceVertices.push_back(static_cast<std::int32_t>(generatedSector.vertices.size() - 1));
    }

    Surface portal = rectangleFace;
    portal.vertices.clear();
    portal.texVertices.clear();
    portal.vertices.reserve(sourceVertices.size());
    for ( auto it = sourceVertices.rbegin(); it != sourceVertices.rend(); ++it )
    {
        portal.vertices.push_back(*it);
    }
    portal.texVertices.assign(portal.vertices.size(), TexVertex{});
    portal.adjoin.reset();
    portal.nAdj = -1;
    generatedSector.surfaces[static_cast<std::size_t>(generated.surface)] = std::move(portal);

    const auto materialize = [&](const detail::ContainedRingVertex vertex) -> std::optional<std::int32_t>
    {
        const auto& contour = vertex.contour == detail::ContainedRingContour::source
            ? sourceContour : rectangleContour;
        const auto found = std::find_if(contour.begin(), contour.end(),
            [&](const detail::ContainedRingPoint& point)
            {
                return point.index == vertex.index;
            });
        if ( found == contour.end() )
        {
            return std::nullopt;
        }
        const auto corner = static_cast<std::size_t>(found - contour.begin());
        if ( vertex.contour == detail::ContainedRingContour::source )
        {
            return sourceVertices[corner];
        }
        return rectangleFace.vertices[corner];
    };

    for ( const auto& triangle : ring.triangles )
    {
        Surface part = rectangleFace;
        part.vertices.clear();
        part.texVertices.clear();
        part.vertices.reserve(triangle.vertices.size());
        for ( const auto vertex : triangle.vertices )
        {
            const auto index = materialize(vertex);
            if ( !index )
            {
                return false;
            }
            part.vertices.push_back(*index);
        }
        part.texVertices.assign(part.vertices.size(), TexVertex{});
        part.adjoin.reset();
        part.nAdj = -1;
        generatedSector.surfaces.push_back(std::move(part));
    }

    recalcAndRenumberSectorSurfaces(generatedSector);
    Surface sourceBoundary = sourceFace;
    // A zero authored normal is legal input. Boundary matching needs a
    // geometric direction for this isolated proof, while the source surface
    // in the candidate keeps its authored cache unchanged.
    sourceBoundary.normal = normal;
    if ( !applyExtrudeCapUvProjection(generatedSector, generated, destinationCount,
            generatedUv, normal, anchor)
        || !precision::closedBoundary(generatedSector)
        || !precision::surfaceBoundariesMatchChecked(
            sourceSector, sourceBoundary,
            generatedSector, generatedSector.surfaces[static_cast<std::size_t>(generated.surface)])
        || !levelSysAdjoinSurfaces(candidate, original, generated, true) )
    {
        return false;
    }

    trialAffected.erase(
        std::remove(trialAffected.begin(), trialAffected.end(), generated.sector), trialAffected.end());
    if ( !extrudeFragmentLimits(candidate, original.sector, generated.sector)
        || validateCleaveAffectedSectors(candidate, trialAffected, &trialProfiles).error
            != SectorCleaveBatchError::None )
    {
        return false;
    }
    if ( validator && !validator(candidate, original, generated, validatorContext) )
    {
        return false;
    }

    const Sector& sourceBefore = level.sectors[static_cast<std::size_t>(original.sector)];
    for ( std::size_t index = 0; index < sourceBefore.vertices.size(); ++index )
    {
        candidate.sectors[static_cast<std::size_t>(original.sector)].vertices[index].mark =
            sourceBefore.vertices[index].mark;
    }
    if ( trialProfiles.cogReferences )
    {
        trialProfiles.cogReferences->publish(candidate);
    }
    level = std::move(candidate);
    return true;
}

} // namespace

bool precision::connectExtrudeSurfacesChecked(Level& level, const SurfaceRef original, const SurfaceRef generated,
    const Level* const referenceSource, const ExtrudeConnectionValidator validator,
    void* const validatorContext)
{
    if ( !isValidUvCoordinateSpace(level.uvCoordinateSpace) || !isValidSurfaceRef(level, original)
        || !isValidSurfaceRef(level, generated) || original.sector == generated.sector )
    {
        return false;
    }
    const auto& source = level.sectors[static_cast<std::size_t>(original.sector)];
    const auto& sourceFace = source.surfaces[static_cast<std::size_t>(original.surface)];
    const auto& destination = level.sectors[static_cast<std::size_t>(generated.sector)];
    const auto& destinationFace = destination.surfaces[static_cast<std::size_t>(generated.surface)];
    if ( sourceFace.adjoin || destinationFace.adjoin )
    {
        return false;
    }
    auto affected = collectCleaveAffectedSectors(level, original.sector);
    appendUniqueSector(affected, generated.sector);
    CleaveProfileContext profiles;
    if ( !profiles.capture(level, affected, false, false) )
    {
        return false;
    }
    // The generated Sector already exists in the geometric candidate. Resolve
    // authored pointers against the pre-append Level so newly created indices
    // cannot turn invalid globals or malformed pair text into live targets.
    profiles.cogReferences.emplace(referenceSource ? *referenceSource : level);
    const auto& sourceProfile = profiles.originals[profiles.sectors.at(original.sector).faces.at(original.surface)];
    const TVector normal = sourceProfile.normal;
    const TVector anchor = sourceProfile.anchor;
    if ( normal.dot(destinationFace.normal) >= 0.0 )
    {
        return false;
    }
    for ( const auto index : destinationFace.vertices )
    {
        const auto point = precision::position(destination.vertices[static_cast<std::size_t>(index)]);
        const double distance = normal.dot(point - anchor);
        const double error = profiles.errorAt(sourceProfile, point);
        if ( !std::isfinite(distance) || !std::isfinite(error) || error < 0.0
            || distance < sourceProfile.minimum - error || distance > sourceProfile.maximum + error )
        {
            return false;
        }
    }
    const auto sourceCount = static_cast<std::int32_t>(source.surfaces.size());
    const auto sourceVertexCount = source.vertices.size();
    const auto destinationCount = static_cast<std::int32_t>(destination.surfaces.size());
    const auto generatedUv = captureExtrudeCapUvProjection(destination, destinationFace, normal, anchor);
    if ( !generatedUv )
    {
        return false;
    }
    const TVector projectionError = extrudeProjectionError(source, sourceFace, destination, destinationFace, normal, anchor);
    if ( !precision::finite(projectionError) )
    {
        return false;
    }
    std::vector<detail::OriginalCornerProjection> certificates;
    for ( std::size_t corner = 0; corner < sourceFace.vertices.size(); ++corner )
    {
        const auto previous = (corner + sourceFace.vertices.size() - 1) % sourceFace.vertices.size();
        const auto next = (corner + 1) % sourceFace.vertices.size();
        certificates.push_back(detail::originalCornerProjection(
            precision::position(source.vertices[sourceFace.vertices[previous]]),
            precision::position(source.vertices[sourceFace.vertices[corner]]),
            precision::position(source.vertices[sourceFace.vertices[next]]), normal, anchor));
    }
    // Preserve the minimal topology of the successful convex path. Its
    // independently clipped boundaries can disagree on inherited projected
    // bends, so a failed attempt is discarded before constructing a common
    // immutable-line partition. Neither its geometry nor its profile/COG
    // remapping state is allowed to seed the second attempt.
    const auto tryConnection = [&](const bool partitioned)
    {
        Level candidate = level;
        auto trialProfiles = partitioned ? std::move(profiles) : profiles;
        auto trialAffected = partitioned ? std::move(affected) : affected;
        if ( partitioned )
        {
            if ( !connectPartitionedExtrudeFootprint(candidate, original, generated, sourceCount, destinationCount,
                    normal, anchor, projectionError, trialProfiles)
                || !applyExtrudeCapUvProjection(candidate.sectors[generated.sector], generated, destinationCount,
                    *generatedUv, normal, anchor)
                || !normalizeCollapsedExtrudeCaps(candidate, generated, destinationCount, trialProfiles)
                || !normalizeExactCollapsedExtrudeSourceFragments(candidate, original.sector, sourceCount,
                    sourceVertexCount, trialProfiles) )
            {
                return false;
            }
            trialAffected.erase(std::remove(trialAffected.begin(), trialAffected.end(), generated.sector), trialAffected.end());
            if ( !extrudeFragmentLimits(candidate, original.sector, generated.sector)
                || validateCleaveAffectedSectors(candidate, trialAffected, &trialProfiles).error != SectorCleaveBatchError::None )
            {
                return false;
            }
            if ( validator && !validator(candidate, original, generated, validatorContext) )
            {
                return false;
            }
            for ( std::size_t index = 0; index < source.vertices.size(); ++index )
            {
                candidate.sectors[original.sector].vertices[index].mark = source.vertices[index].mark;
            }
            trialProfiles.cogReferences->publish(candidate);
            level = std::move(candidate);
            return true;
        }
        // Both clipping boundaries belong to the immutable input. In particular,
        // the generated rectangle must not become the already-trimmed first result.
        if ( !clipExtrudeBoundary(candidate, generated,
                source, sourceFace, normal, trialProfiles)
            || !clipExtrudeBoundary(candidate, original,
                destination, destinationFace, -normal, trialProfiles) )
        {
            return false;
        }
        if ( !canonicalizeExtrudeCutJunctions(candidate.sectors[original.sector], source, sourceFace,
                destination, destinationFace, normal, anchor, projectionError, original.sector, trialProfiles) )
        {
            return false;
        }
        const auto sourceCandidates = collectSurfaceCandidates(candidate, original.sector, original.surface, sourceCount);
        const auto destinationCandidates = collectSurfaceCandidates(candidate, generated.sector, generated.surface, destinationCount);
        for ( const auto a : sourceCandidates )
        {
            for ( const auto b : destinationCandidates )
            {
                const auto& first = candidate.sectors[a.sector];
                const auto& second = candidate.sectors[b.sector];
                const Sector firstProjection = projectedExtrudeFace(first, first.surfaces[a.surface], normal, anchor);
                Sector secondProjection = projectedExtrudeFace(second, second.surfaces[b.surface], normal, anchor);
                if ( !reconcileExtrudeProjection(firstProjection, secondProjection, projectionError, certificates) )
                {
                    continue;
                }
                if ( !liftExtrudeBoundary(candidate, a, b, normal, anchor, projectionError, certificates)
                    || !levelSysAdjoinSurfaces(candidate, a, b, true)
                    || !applyExtrudeCapUvProjection(candidate.sectors[generated.sector], generated, destinationCount,
                        *generatedUv, normal, anchor)
                    || !normalizeCollapsedExtrudeCaps(candidate, generated, destinationCount, trialProfiles)
                    || !normalizeExactCollapsedExtrudeSourceFragments(candidate, original.sector, sourceCount,
                        sourceVertexCount, trialProfiles) )
                {
                    return false;
                }
                trialAffected.erase(std::remove(trialAffected.begin(), trialAffected.end(), generated.sector), trialAffected.end());
                if ( !extrudeFragmentLimits(candidate, original.sector, generated.sector)
                    || validateCleaveAffectedSectors(candidate, trialAffected, &trialProfiles).error != SectorCleaveBatchError::None )
                {
                    return false;
                }
                if ( validator && !validator(candidate, original, generated, validatorContext) )
                {
                    return false;
                }
                for ( std::size_t index = 0; index < source.vertices.size(); ++index )
                {
                    candidate.sectors[original.sector].vertices[index].mark = source.vertices[index].mark;
                }
                trialProfiles.cogReferences->publish(candidate);
                level = std::move(candidate);
                return true;
            }
        }
        return false;
    };
    const Sector sourceProjection = projectedExtrudeFace(source, sourceFace, normal, anchor);
    if ( surfaceIsConvex(sourceProjection, sourceProjection.surfaces.front()) && tryConnection(false) )
    {
        return true;
    }
    if ( tryContainedExtrudeConnection(level, original, generated, normal, anchor,
            destinationCount, *generatedUv, affected, profiles, validator, validatorContext) )
    {
        return true;
    }
    return tryConnection(true);
}

bool levelConnectSurfaces(
    Level& level,
    const SurfaceRef surf1,
    const SurfaceRef surf2) noexcept
{
    try
    {
        return precision::connectSurfacesChecked(level, surf1, surf2);
    }
    catch (...)
    {
        return false;
    }
}

static bool levelCleaveSectorImpl(
    Level& level,
    const std::int32_t sectorIndex,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz,
    const bool preserveOriginalIdentity,
    CleaveProfileContext* const context)
{
    // Step 1: validate the target sector and cleave all original surfaces against the plane.
    if (!isValidSectorIndex(level, sectorIndex))
    {
        return false;
    }

    const Sector& originalSector =
        level.sectors[static_cast<std::size_t>(sectorIndex)];
    const TVector originalCenter = sectorCalcCenter(originalSector);
    const double originalCenterDistance = planeNormal.dot(
        originalCenter.x - px,
        originalCenter.y - py,
        originalCenter.z - pz);

    std::vector<std::pair<TVector, TVector>> sourceEdges;
    if ( context )
    {
        for ( const Surface& face : originalSector.surfaces )
        {
            for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
            {
                sourceEdges.emplace_back(
                    precision::position(originalSector.vertices[face.vertices[corner]]),
                    precision::position(originalSector.vertices[face.vertices[(corner + 1) % face.vertices.size()]]));
            }
        }
    }

    if (!splitSectorSurfacesByPlane(level, sectorIndex, planeNormal, px, py, pz, context))
    {
        return false;
    }

    Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];

    // Step 2: collect and order the boundary loop used to create the two cap surfaces.
    const std::vector<std::int32_t> capLoop = orderPlaneBoundaryLoop(collectPlaneBoundaryEdges(sector));
    if (capLoop.empty())
    {
        return false;
    }
    // A rounded nominally convex boundary can have a tiny non-convex exact
    // cross-section. Certify that every cap corner is an actual source-edge
    // intersection and each cap edge belongs to both split side contours.
    // This construction witness is not a new measured tolerance: source
    // envelopes remain immutable and the requested cap plane remains exact.
    const bool certifiedBoundary = context
        && certifyCutBoundary(sector, capLoop, sourceEdges, planeNormal, {px, py, pz});
    if ( context && !certifiedBoundary )
    {
        return false;
    }

    // Step 3: snapshot the split sector, create the new sector, and copy sector-level parameters.
    Sector sectorParams{};
    assignSectorParams(sector, sectorParams);
    const std::vector<Vertex> oldVertices = sector.vertices;
    const std::vector<Surface> oldSurfaces = sector.surfaces;

    const std::int32_t newSectorIndex = level.newSector().num;

    Sector& sec0 = level.sectors[static_cast<std::size_t>(sectorIndex)];
    Sector& sec1 = level.sectors[static_cast<std::size_t>(newSectorIndex)];

    assignSectorParams(sectorParams, sec1);

    // Step 4: divide vertices and surfaces between the original and new sector.
    std::vector<std::int32_t> map0(oldVertices.size(), -1);
    std::vector<std::int32_t> map1(oldVertices.size(), -1);
    splitSectorVertexPools(oldVertices, sec0, sec1, map0, map1);

    const std::vector<bool> moveToSec1 = classifySplitSectorSurfaces(oldVertices, oldSurfaces, planeNormal);
    std::vector<std::optional<SurfaceRef>> surfaceRemap(oldSurfaces.size());
    if (!assignSplitSectorSurfaces(
            sec0,
            sec1,
            oldSurfaces,
            oldVertices,
            moveToSec1,
            map0,
            map1,
            surfaceRemap,
            sectorIndex,
            newSectorIndex))
    {
        return false;
    }

    if ( context )
    {
        context->splitSector(sectorIndex, newSectorIndex, surfaceRemap);
    }

    // Step 5: redirect adjoins, add the cap surfaces, and relayer things formerly in the split sector.
    rewriteAdjoinsAfterSectorSplit(level, sectorIndex, surfaceRemap);
    if (!buildAndAdjoinSplitCaps(level, sec0, sec1, sectorIndex, newSectorIndex, planeNormal, capLoop, oldVertices, map0, map1))
    {
        return false;
    }

    if ( context )
    {
        const TVector unitNormal = resolvedSurfaceNormal(sec0, sec0.surfaces.back());
        // Requested planes, not the caps' measured residuals, own these new
        // constraints. Only orientation comes from the constructed winding.
        TVector requested = planeNormal;
        const double scale = std::max({std::abs(requested.x), std::abs(requested.y), std::abs(requested.z)});
        requested = {requested.x / scale, requested.y / scale, requested.z / scale};
        requested.normalize();
        if ( requested.dot(unitNormal) < 0.0 )
        {
            requested = -requested;
        }
        context->addCap(sec0, sectorIndex, requested, {px, py, pz}, certifiedBoundary);
        context->addCap(sec1, newSectorIndex, -requested, {px, py, pz}, certifiedBoundary);
    }
    relayerThingsAfterSectorSplit(level, sectorIndex);

    // The low-level split classifies the positive plane side into the
    // existing slot.  For editor identity, however, the body containing the
    // pre-cut sector centre is the retained/original sector.  This keeps the
    // old num/id, sector COG links, and the main room body stable when a small
    // fraction is cleaved away; the cut-off body remains the appended sector
    // selected by the viewport.
    if (preserveOriginalIdentity && originalCenterDistance < -precision::planeError(
            planeNormal, originalCenter, {px, py, pz}))
    {
        swapSplitSectorBodiesPreservingIdentity(
            level, sectorIndex, newSectorIndex);
        if ( context )
        {
            std::swap(context->sectors.at(sectorIndex), context->sectors.at(newSectorIndex));
            if ( context->cogReferences )
            {
                context->cogReferences->swapSurfaceOwners(sectorIndex, newSectorIndex);
            }
        }
    }
    return true;
}

bool levelCleaveSector(
    Level& level,
    const std::int32_t sectorIndex,
    const TVector& planeNormal,
    const double px,
    const double py,
    const double pz) noexcept
{
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || !isValidSectorIndex(level, sectorIndex))
    {
        return false;
    }

    try
    {
        std::vector<std::int32_t> affected = collectCleaveAffectedSectors(level, sectorIndex);
        if ( !cleaveAffectedSectorsHaveFiniteUvValues(level, affected)
            || validateCleaveAffectedSectors(level, affected, nullptr, true).error != SectorCleaveBatchError::None )
        {
            return false;
        }
        CleaveProfileContext profiles;
        if ( !profiles.capture(level, affected, true) )
        {
            return false;
        }
        Level candidate = level;
        if (!levelCleaveSectorImpl(
                candidate,
                sectorIndex,
                planeNormal,
                px,
                py,
                pz,
                true,
                &profiles))
        {
            return false;
        }

        appendUniqueSector(affected, static_cast<std::int32_t>(candidate.sectors.size() - 1));
        if (validateCleaveAffectedSectors(candidate, affected, &profiles).error != SectorCleaveBatchError::None)
        {
            return false;
        }

        profiles.cogReferences->publish(candidate);
        level = std::move(candidate);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static SectorCleaveBatchResult levelCleaveSectorByPlanesImpl(
    Level& level,
    const std::int32_t sectorIndex,
    const std::span<const SectorCleavePlane> planes,
    const SectorCleaveBatchOptions& options)
{
    SectorCleaveBatchResult result{};

    auto fail = [&](const SectorCleaveBatchError error,
                    const std::int32_t failedPlane,
                    const std::int32_t failedSector,
                    const std::int32_t failedSurface = -1)
    {
        result.error = error;
        result.failedPlane = failedPlane;
        result.failedSector = failedSector;
        result.failedSurface = failedSurface;
        result.outputSectors.clear();
        return result;
    };

    // Step 1: reject malformed requests before copying or mutating the Level.
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace))
    {
        return fail(
            SectorCleaveBatchError::InvalidInputGeometry,
            -1,
            sectorIndex);
    }
    if (!isValidSectorIndex(level, sectorIndex))
    {
        return fail(SectorCleaveBatchError::InvalidSector, -1, sectorIndex);
    }
    if (planes.empty())
    {
        return fail(SectorCleaveBatchError::EmptyPlaneSet, -1, sectorIndex);
    }

    const std::vector<std::int32_t> initiallyAffected =
        collectCleaveAffectedSectors(level, sectorIndex);
    if (!cleaveAffectedSectorsHaveFiniteUvValues(level, initiallyAffected))
    {
        return fail(
            SectorCleaveBatchError::InvalidInputGeometry,
            -1,
            sectorIndex);
    }

    // The selected solid and any directly adjoined solids can both be touched
    // by a mirrored portal split, so all of them must start valid.
    const SectorCleaveGeometryValidation inputValidation =
        validateCleaveAffectedSectors(level, initiallyAffected, nullptr, true);
    if (inputValidation.error != SectorCleaveBatchError::None)
    {
        return fail(
            SectorCleaveBatchError::InvalidInputGeometry,
            -1,
            inputValidation.sector,
            inputValidation.surface);
    }

    if (options.closedShape && !isFiniteVector(options.interiorPoint))
    {
        return fail(SectorCleaveBatchError::InvalidPlane, -1, sectorIndex);
    }
    if ( options.bounded && !options.closedShape )
    {
        if ( options.strokes.size() != planes.size() )
        {
            return fail(SectorCleaveBatchError::InvalidPlane, -1, sectorIndex);
        }
        for ( std::size_t strokeIndex = 0; strokeIndex < options.strokes.size(); ++strokeIndex )
        {
            const SectorCleaveStroke& stroke = options.strokes[strokeIndex];
            if ( !isFiniteVector(stroke.first) || !isFiniteVector(stroke.second) )
            {
                return fail(
                    SectorCleaveBatchError::InvalidPlane,
                    static_cast<std::int32_t>(strokeIndex),
                    sectorIndex);
            }
            const long double strokeLength = std::hypot(
                static_cast<long double>(stroke.second.x) - stroke.first.x,
                static_cast<long double>(stroke.second.y) - stroke.first.y,
                static_cast<long double>(stroke.second.z) - stroke.first.z);
            if ( !std::isfinite(strokeLength) || strokeLength <= 0.0L )
            {
                return fail(
                    SectorCleaveBatchError::InvalidPlane,
                    static_cast<std::int32_t>(strokeIndex),
                    sectorIndex);
            }
        }
    }

    CleaveProfileContext profiles;
    if ( !profiles.capture(level, initiallyAffected, true) )
    {
        return fail(SectorCleaveBatchError::InvalidInputGeometry, -1, sectorIndex);
    }
    Level candidate = level;
    std::vector<std::int32_t> descendants{ sectorIndex };
    std::vector<std::int32_t> activeDescendants{ sectorIndex };
    std::unordered_map<std::int32_t, std::vector<CleaveStrokeHalfSpace>> descendantHalfSpaces{};
    if ( options.bounded && !options.closedShape )
    {
        descendantHalfSpaces.emplace(sectorIndex, std::vector<CleaveStrokeHalfSpace>{});
    }
    bool splitAnyPlane = false;

    // Step 2: apply each staged plane to only the current descendants it
    // crosses. The fixed stage list prevents a plane from being applied twice
    // to pieces it creates during that same stage.
    for (std::size_t planeIndex = 0; planeIndex < planes.size(); ++planeIndex)
    {
        const std::int32_t planeNumber = static_cast<std::int32_t>(planeIndex);
        const SectorCleavePlane& sourcePlane = planes[planeIndex];
        if (!isFiniteVector(sourcePlane.normal) || !isFiniteVector(sourcePlane.point))
        {
            return fail(SectorCleaveBatchError::InvalidPlane, planeNumber, sectorIndex);
        }

        TVector planeNormal = sourcePlane.normal;
        const double normalScale = std::max({std::abs(planeNormal.x),
            std::abs(planeNormal.y), std::abs(planeNormal.z)});
        if ( normalScale == 0.0 )
        {
            return fail(SectorCleaveBatchError::InvalidPlane, planeNumber, sectorIndex);
        }
        planeNormal = {planeNormal.x / normalScale, planeNormal.y / normalScale,
            planeNormal.z / normalScale};
        if ( planeNormal.normalize() == 0.0 || !isFiniteVector(planeNormal) )
        {
            return fail(SectorCleaveBatchError::InvalidPlane, planeNumber, sectorIndex);
        }

        const double interiorDistance = options.closedShape
            ? planeNormal.dot(
                options.interiorPoint.x - sourcePlane.point.x,
                options.interiorPoint.y - sourcePlane.point.y,
                options.interiorPoint.z - sourcePlane.point.z)
            : 0.0;
        if (options.closedShape && std::abs(interiorDistance) <= precision::planeError(
                planeNormal, options.interiorPoint, sourcePlane.point))
        {
            return fail(SectorCleaveBatchError::InvalidPlane, planeNumber, sectorIndex);
        }

        const std::vector<std::int32_t> stageDescendants = options.closedShape
            ? activeDescendants
            : descendants;
        std::vector<std::int32_t> nextActiveDescendants{};

        for (const std::int32_t descendant : stageDescendants)
        {
            if ( !isValidSectorIndex(candidate, descendant) )
            {
                return fail(SectorCleaveBatchError::CleaveFailed, planeNumber, descendant);
            }

            if ( options.bounded && !options.closedShape )
            {
                const auto lineage = descendantHalfSpaces.find(descendant);
                if ( lineage == descendantHalfSpaces.end()
                    || !cleaveStrokeReachesHalfSpaces(
                        options.strokes[planeIndex], lineage->second) )
                {
                    continue;
                }
            }

            const Sector& descendantSector = candidate.sectors[static_cast<std::size_t>(descendant)];
            if (!planeSplitsSector(descendantSector, planeNormal, sourcePlane.point))
            {
                if (options.closedShape)
                {
                    bool hasInteriorSideVertex = false;
                    bool hasExteriorSideVertex = false;
                    for (const Vertex& vertex : descendantSector.vertices)
                    {
                        const double distance = planeNormal.dot(
                            vertex.x - sourcePlane.point.x,
                            vertex.y - sourcePlane.point.y,
                            vertex.z - sourcePlane.point.z);
                        if (std::abs(distance) <= precision::planeError(planeNormal,
                                precision::position(vertex), sourcePlane.point))
                        {
                            continue;
                        }
                        const bool interiorSide = (distance > 0.0) == (interiorDistance > 0.0);
                        hasInteriorSideVertex = hasInteriorSideVertex || interiorSide;
                        hasExteriorSideVertex = hasExteriorSideVertex || !interiorSide;
                    }
                    if (hasInteriorSideVertex && !hasExteriorSideVertex)
                    {
                        nextActiveDescendants.push_back(descendant);
                    }
                }
                continue;
            }

            splitAnyPlane = true;
            std::vector<std::int32_t> affected = collectCleaveAffectedSectors(candidate, descendant);
            const std::size_t sectorCountBefore = candidate.sectors.size();
            if (!levelCleaveSectorImpl(
                    candidate,
                    descendant,
                    planeNormal,
                    sourcePlane.point.x,
                    sourcePlane.point.y,
                    sourcePlane.point.z,
                    true,
                    &profiles)
                || candidate.sectors.size() != sectorCountBefore + 1)
            {
                return fail(SectorCleaveBatchError::CleaveFailed, planeNumber, descendant);
            }

            const std::int32_t newSector = static_cast<std::int32_t>(sectorCountBefore);
            descendants.push_back(newSector);
            if ( options.bounded && !options.closedShape )
            {
                const bool descendantPositive = sectorIsOnPositivePlaneSide(
                    candidate.sectors[static_cast<std::size_t>(descendant)],
                    planeNormal,
                    sourcePlane.point.x,
                    sourcePlane.point.y,
                    sourcePlane.point.z);
                const bool appendedPositive = sectorIsOnPositivePlaneSide(
                    candidate.sectors[static_cast<std::size_t>(newSector)],
                    planeNormal,
                    sourcePlane.point.x,
                    sourcePlane.point.y,
                    sourcePlane.point.z);
                if ( descendantPositive == appendedPositive )
                {
                    return fail(
                        SectorCleaveBatchError::InvalidResultTopology,
                        planeNumber,
                        descendant);
                }
                const auto lineage = descendantHalfSpaces.find(descendant);
                if ( lineage == descendantHalfSpaces.end() )
                {
                    return fail(
                        SectorCleaveBatchError::InvalidResultTopology,
                        planeNumber,
                        descendant);
                }
                std::vector<CleaveStrokeHalfSpace> retainedLineage = lineage->second;
                retainedLineage.push_back(CleaveStrokeHalfSpace{
                    planeNormal,
                    sourcePlane.point,
                    descendantPositive,
                });
                std::vector<CleaveStrokeHalfSpace> appendedLineage = retainedLineage;
                appendedLineage.back().positiveSide = appendedPositive;
                lineage->second = std::move(retainedLineage);
                descendantHalfSpaces.emplace(newSector, std::move(appendedLineage));
            }
            if (options.closedShape)
            {
                // The identity-preserving split may exchange the two bodies.
                // Continue with whichever geometric half is on the same side
                // of this plane as the closed chain's interior sample.
                const bool descendantPositive = sectorIsOnPositivePlaneSide(
                    candidate.sectors[static_cast<std::size_t>(descendant)],
                    planeNormal,
                    sourcePlane.point.x,
                    sourcePlane.point.y,
                    sourcePlane.point.z);
                nextActiveDescendants.push_back(
                    descendantPositive == (interiorDistance > 0.0)
                        ? descendant
                        : newSector);
            }
            appendUniqueSector(affected, descendant);
            appendUniqueSector(affected, newSector);

            const std::vector<std::int32_t> descendantAdjoins = collectCleaveAffectedSectors(candidate, descendant);
            for (const std::int32_t affectedSector : descendantAdjoins)
            {
                appendUniqueSector(affected, affectedSector);
            }
            const std::vector<std::int32_t> newSectorAdjoins = collectCleaveAffectedSectors(candidate, newSector);
            for (const std::int32_t affectedSector : newSectorAdjoins)
            {
                appendUniqueSector(affected, affectedSector);
            }

            const SectorCleaveGeometryValidation validation = validateCleaveAffectedSectors(candidate, affected, &profiles);
            if (validation.error != SectorCleaveBatchError::None)
            {
                return fail(validation.error, planeNumber, validation.sector, validation.surface);
            }
        }

        if (options.closedShape)
        {
            activeDescendants = std::move(nextActiveDescendants);
            if (activeDescendants.empty())
            {
                return fail(SectorCleaveBatchError::PlaneDoesNotSplit, planeNumber, sectorIndex);
            }
        }
    }

    if (!splitAnyPlane)
    {
        return fail(SectorCleaveBatchError::PlaneDoesNotSplit, 0, sectorIndex);
    }

    // Step 3: validate every final descendant once more, then commit the copy
    // as a single atomic mutation.
    const SectorCleaveGeometryValidation finalValidation = validateCleaveAffectedSectors(candidate, descendants, &profiles);
    if (finalValidation.error != SectorCleaveBatchError::None)
    {
        return fail(
            finalValidation.error,
            static_cast<std::int32_t>(planes.size() - 1),
            finalValidation.sector,
            finalValidation.surface);
    }

    // Complete every output before publishing geometry. Both final moves are
    // statically no-throw, including the caller's returned result transfer.
    static_assert(std::is_nothrow_move_assignable_v<Level>);
    static_assert(std::is_nothrow_move_constructible_v<SectorCleaveBatchResult>);
    result.outputSectors = std::move(descendants);
    profiles.cogReferences->publish(candidate);
    level = std::move(candidate);
    return result;
}

SectorCleaveBatchResult levelCleaveSectorByPlanes(Level& level, const std::int32_t sector,
    const std::span<const SectorCleavePlane> planes, const SectorCleaveBatchOptions& options) noexcept
{
    try
    {
        return levelCleaveSectorByPlanesImpl(level, sector, planes, options);
    }
    catch (...)
    {
        SectorCleaveBatchResult result;
        result.error = SectorCleaveBatchError::CleaveFailed;
        result.failedSector = sector;
        return result;
    }
}

static bool levelConnectSectorsImpl(Level& level, const std::int32_t sector1, const std::int32_t sector2)
{
    if ( !isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || !isValidSectorIndex(level, sector1) || !isValidSectorIndex(level, sector2)
        || sector1 == sector2 )
    {
        return false;
    }

    auto affected = collectCleaveAffectedSectors(level, sector1);
    for ( const auto index : collectCleaveAffectedSectors(level, sector2) )
    {
        appendUniqueSector(affected, index);
    }
    CleaveProfileContext profiles;
    if ( !cleaveAffectedSectorsHaveFiniteUvValues(level, affected)
        || !profiles.capture(level, affected, false)
        || validateCleaveAffectedSectors(level, affected, &profiles).error != SectorCleaveBatchError::None )
    {
        return false;
    }
    Level candidate = level;
    if ( !connectSectorSurfacesToTargetSectors(candidate, { sector1 }, { sector2 }, nullptr, &profiles)
        || !cleaveAffectedSectorsHaveFiniteUvValues(candidate, affected)
        || validateCleaveAffectedSectors(candidate, affected, &profiles).error != SectorCleaveBatchError::None )
    {
        return false;
    }

    profiles.cogReferences->publish(candidate);
    level = std::move(candidate);
    return true;
}

bool levelConnectSectors(Level& level, const std::int32_t sector1, const std::int32_t sector2) noexcept
{
    try
    {
        return levelConnectSectorsImpl(level, sector1, sector2);
    }
    catch (...)
    {
        return false;
    }
}

bool levelConnectSectorsKnifeCutTargetHolesOnly(
    Level& level,
    const std::int32_t knifeSector,
    const std::int32_t targetSector,
    const Sector& originalTarget,
    CleaveProfileContext& profiles)
{
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace))
    {
        return false;
    }

    Level candidate = level;
    std::vector<std::int32_t> affected = collectCleaveAffectedSectors(level, knifeSector);
    for ( const auto index : collectCleaveAffectedSectors(level, targetSector) )
    {
        appendUniqueSector(affected, index);
    }

    // Step 1: clip only the knife family by the target's solid boundary. Adjoined target
    // surfaces are already open portals and must not slice the knife in no-overlap mode.
    const std::vector<Plane> targetPlanes =
        collectSectorPlanes(candidate, targetSector, SectorPlaneFilter::SolidSurfacesOnly);
    const auto trackedKnifeFragments =
        clipTrackedSectorFamilyByPlanes(candidate, { TrackedSectorFragment{ knifeSector, true } }, targetPlanes, profiles);

    if ( !trackedKnifeFragments )
    {
        return false;
    }

    if (!isValidSectorIndex(candidate, knifeSector) || !isValidSectorIndex(candidate, targetSector))
    {
        return false;
    }

    // Step 2: delete the pieces of the knife that truly entered the target volume.
    std::vector<std::int32_t> embeddedKnifeSectors{};
    embeddedKnifeSectors.reserve(trackedKnifeFragments->size());
    std::vector<std::int32_t> knifeCandidateSectors{};
    knifeCandidateSectors.reserve(trackedKnifeFragments->size());
    for (const TrackedSectorFragment& fragment : *trackedKnifeFragments)
    {
        const std::int32_t knifeCandidate = fragment.sector;
        if (!isValidSectorIndex(candidate, knifeCandidate))
        {
            continue;
        }

        knifeCandidateSectors.push_back(knifeCandidate);

        if (!fragment.insideAllPlanes
            && !sectorInteriorOverlaps(candidate.sectors[static_cast<std::size_t>(knifeCandidate)], originalTarget))
        {
            continue;
        }

        embeddedKnifeSectors.push_back(knifeCandidate);
    }

    std::sort(embeddedKnifeSectors.begin(), embeddedKnifeSectors.end());
    embeddedKnifeSectors.erase(std::unique(embeddedKnifeSectors.begin(), embeddedKnifeSectors.end()), embeddedKnifeSectors.end());

    // Step 3: before deleting the embedded cutter volume, let its faces split the target surfaces.
    // Those temporary adjoins are intentionally cleared when the embedded sectors are removed, but
    // the target surface cuts remain and give the outside knife pieces matching portal candidates.
    const std::vector<std::int32_t> originalTargetCandidateSectors{ targetSector };
    (void)connectSectorSurfacesToTargetSectors(candidate, embeddedKnifeSectors, originalTargetCandidateSectors, nullptr, &profiles);

    for ( std::size_t index = level.sectors.size(); index < candidate.sectors.size(); ++index )
    {
        appendUniqueSector(affected, static_cast<std::int32_t>(index));
    }
    if (!deleteSectorsDescending(candidate, embeddedKnifeSectors))
    {
        return false;
    }
    profiles.eraseSectors(embeddedKnifeSectors);

    knifeCandidateSectors = remapSectorsAfterDeletes(knifeCandidateSectors, embeddedKnifeSectors);
    affected = remapSectorsAfterDeletes(affected, embeddedKnifeSectors);
    const std::int32_t remappedTargetSector = remapSectorAfterDeletes(targetSector, embeddedKnifeSectors);
    if (remappedTargetSector < 0)
    {
        return false;
    }
    const std::vector<std::int32_t> targetCandidateSectors{ remappedTargetSector };

    // Step 4: make the actual hole/adjoin from the outside knife pieces to the now-split target.
    bool anyConnected = sectorsHaveAdjoinBetween(candidate, knifeCandidateSectors, targetCandidateSectors);
    anyConnected = connectSectorSurfacesToTargetSectors(candidate, knifeCandidateSectors, targetCandidateSectors, nullptr, &profiles) || anyConnected;

    if (!anyConnected
        || validateCleaveAffectedSectors(candidate, affected, &profiles).error != SectorCleaveBatchError::None)
    {
        return false;
    }

    profiles.cogReferences->publish(candidate);
    level = std::move(candidate);
    return true;
}

static bool levelConnectSectorsKnifeImpl(
    Level& level,
    const std::int32_t knifeSector,
    const std::int32_t targetSector,
    const SectorConnectKnifeMode mode)
{
    // Step 1: validate broad overlap before trying the knife-sector workflow on a candidate copy.
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || !isValidSectorIndex(level, knifeSector)
        || !isValidSectorIndex(level, targetSector) || knifeSector == targetSector)
    {
        return false;
    }

    std::vector<std::int32_t> affected = collectCleaveAffectedSectors(level, knifeSector);
    for ( const auto index : collectCleaveAffectedSectors(level, targetSector) )
    {
        appendUniqueSector(affected, index);
    }
    CleaveProfileContext profiles;
    if ( !profiles.capture(level, affected, true)
        || validateCleaveAffectedSectors(level, affected, &profiles).error != SectorCleaveBatchError::None )
    {
        return false;
    }

    if (!sectorsMayIntersectForConnect(level.sectors[static_cast<std::size_t>(knifeSector)], level.sectors[static_cast<std::size_t>(targetSector)]))
    {
        return false;
    }

    const Sector originalTarget = level.sectors[static_cast<std::size_t>(targetSector)];

    if (mode == SectorConnectKnifeMode::CutTargetHolesOnly)
    {
        return levelConnectSectorsKnifeCutTargetHolesOnly(level, knifeSector, targetSector, originalTarget, profiles);
    }

    Level candidate = level;
    const std::int32_t startSector = static_cast<std::int32_t>(candidate.sectors.size());

    // Step 2: cleave the target by knife planes, then cleave the knife by target planes.
    bool anyCleave = false;
    const std::vector<Plane> knifePlanes = collectSectorPlanes(candidate, knifeSector);
    for (const Plane& p : knifePlanes)
    {
        const auto sides = countSectorSidesForPlane(candidate.sectors[static_cast<std::size_t>(targetSector)], p);
        if ( sides.positive == 0 || sides.negative == 0 )
        {
            continue;
        }
        if ( !levelCleaveSectorImpl(candidate, targetSector, p.normal, p.x, p.y, p.z, false, &profiles) )
        {
            return false;
        }
        anyCleave = true;
    }
    const std::int32_t targetDerivedEnd = static_cast<std::int32_t>(candidate.sectors.size());

    if (!isValidSectorIndex(candidate, knifeSector) || !isValidSectorIndex(candidate, targetSector))
    {
        return false;
    }

    const std::vector<Plane> targetPlanes = collectSectorPlanes(candidate, targetSector);
    for (const Plane& p : targetPlanes)
    {
        const auto sides = countSectorSidesForPlane(candidate.sectors[static_cast<std::size_t>(knifeSector)], p);
        if ( sides.positive == 0 || sides.negative == 0 )
        {
            continue;
        }
        if ( !levelCleaveSectorImpl(candidate, knifeSector, p.normal, p.x, p.y, p.z, false, &profiles) )
        {
            return false;
        }
        anyCleave = true;
    }

    if (!anyCleave || !isValidSectorIndex(candidate, knifeSector))
    {
        return false;
    }

    std::vector<std::int32_t> embeddedKnifeSectors{};
    // Step 3: collect knife pieces that still have real interior overlap with the target volume.
    if (isValidSectorIndex(candidate, knifeSector)
        && sectorInteriorOverlaps(candidate.sectors[static_cast<std::size_t>(knifeSector)], originalTarget))
    {
        embeddedKnifeSectors.push_back(knifeSector);
    }
    for (std::int32_t s = targetDerivedEnd; s < static_cast<std::int32_t>(candidate.sectors.size()); ++s)
    {
        if (!isValidSectorIndex(candidate, s))
        {
            continue;
        }

        if (!sectorInteriorOverlaps(candidate.sectors[static_cast<std::size_t>(s)], originalTarget))
        {
            continue;
        }

        embeddedKnifeSectors.push_back(s);
    }

    std::sort(embeddedKnifeSectors.begin(), embeddedKnifeSectors.end());
    embeddedKnifeSectors.erase(std::unique(embeddedKnifeSectors.begin(), embeddedKnifeSectors.end()), embeddedKnifeSectors.end());

    std::vector<std::int32_t> knifeCandidateSectors{ knifeSector };
    for (std::int32_t s = targetDerivedEnd; s < static_cast<std::int32_t>(candidate.sectors.size()); ++s)
    {
        knifeCandidateSectors.push_back(s);
    }
    std::sort(knifeCandidateSectors.begin(), knifeCandidateSectors.end());
    knifeCandidateSectors.erase(std::unique(knifeCandidateSectors.begin(), knifeCandidateSectors.end()), knifeCandidateSectors.end());

    bool anyConnected = false;
    // Step 4: legacy mode adjoins embedded knife pieces against the target-derived sector range.
    for (const std::int32_t knifeCandidate : embeddedKnifeSectors)
    {
        for (std::int32_t sf = 0;
             isValidSectorIndex(candidate, knifeCandidate)
             && sf < static_cast<std::int32_t>(candidate.sectors[static_cast<std::size_t>(knifeCandidate)].surfaces.size());
             ++sf)
        {
            std::vector<std::int32_t> targetCandidates;
            for ( std::int32_t index = startSector; index < static_cast<std::int32_t>(candidate.sectors.size()); ++index )
            {
                targetCandidates.push_back(index);
            }
            // Keep mode intentionally permits exact matches to an already
            // adjoined derived face, retaining the original scan/relink policy.
            anyConnected = levelMakeAdjoinFromSectorCandidates(candidate, SurfaceRef{knifeCandidate, sf},
                std::move(targetCandidates), true, &profiles, true) || anyConnected;
        }
    }

    if (!anyConnected)
    {
        return false;
    }

    // Step 5: legacy mode removes the original target piece after the derived pieces are valid.
    for ( std::int32_t index = startSector; index < static_cast<std::int32_t>(candidate.sectors.size()); ++index )
    {
        appendUniqueSector(affected, index);
    }
    if (!deleteSectorGeometry(candidate, targetSector))
    {
        return false;
    }
    profiles.eraseSectors({targetSector});

    affected = remapSectorsAfterDeletes(affected, {targetSector});
    if (validateCleaveAffectedSectors(candidate, affected, &profiles).error != SectorCleaveBatchError::None)
    {
        return false;
    }

    profiles.cogReferences->publish(candidate);
    level = std::move(candidate);
    return true;
}

bool levelConnectSectorsKnife(Level& level, const std::int32_t knifeSector,
    const std::int32_t targetSector, const SectorConnectKnifeMode mode) noexcept
{
    return levelConnectSectorsKnife(
        level, knifeSector, targetSector, mode, nullptr);
}

bool levelConnectSectorsKnife(Level& level, const std::int32_t knifeSector,
    const std::int32_t targetSector, const SectorConnectKnifeMode mode,
    bool* const idExhausted) noexcept
{
    if ( idExhausted )
    {
        *idExhausted = false;
    }
    try
    {
        return levelConnectSectorsKnifeImpl(level, knifeSector, targetSector, mode);
    }
    catch ( const IdExhaustionError& )
    {
        if ( idExhausted )
        {
            *idExhausted = true;
        }
        return false;
    }
    catch (...)
    {
        return false;
    }
}

bool levelConnectSectorSurfaceCandidates(
    Level& level,
    const std::vector<std::int32_t>& sourceSectors,
    const std::vector<std::int32_t>& targetSectors) noexcept
{
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || sourceSectors.empty() || targetSectors.empty())
    {
        return false;
    }

    // Each connection is atomic, while this batch intentionally retains earlier
    // successful connections if a later candidate cannot be completed.
    bool committed = false;
    std::optional<detail::GeometryCogReferences> references;
    try
    {
        references.emplace(level);
        const bool connected = connectSectorSurfacesToTargetSectors(level, sourceSectors, targetSectors, &committed);
        if ( committed )
        {
            references->publish(level);
        }
        return connected;
    }
    catch (...)
    {
        if ( committed && references )
        {
            references->publish(level);
        }
        return committed;
    }
}

static bool deleteSectorGeometry(Level& level, const std::int32_t sectorIndex) noexcept
{
    static_assert(std::is_nothrow_move_assignable_v<Sector>);
    // Step 1: validate and clear direct references before erasing the sector from the vector.
    if (!isValidSectorIndex(level, sectorIndex))
    {
        return false;
    }

    clearAdjoinsTargetingSector(level, sectorIndex);
    clearThingSectorRefs(level, sectorIndex);

    // Step 2: remove and renumber sectors.
    level.sectors.erase(level.sectors.begin() + static_cast<std::ptrdiff_t>(sectorIndex));

    for (std::size_t i = 0; i < level.sectors.size(); ++i)
    {
        level.sectors[i].num = static_cast<std::int32_t>(i);
    }

    // Step 3: shift references that pointed past the deleted sector.
    shiftAdjoinsAfterSectorDelete(level, sectorIndex);
    shiftThingSectorRefsAfterDelete(level, sectorIndex);

    return true;
}

bool levelDeleteSector(Level& level, const std::int32_t sectorIndex) noexcept
{
    if ( !isValidSectorIndex(level, sectorIndex) )
    {
        return false;
    }
    try
    {
        detail::GeometryCogReferences references(level);
        if ( !deleteSectorGeometry(level, sectorIndex) )
        {
            return false;
        }
        references.eraseSector(sectorIndex);
        references.publish(level);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static bool deleteSurfaceGeometry(Level& level, const SurfaceRef surfaceRef) noexcept
{
    static_assert(std::is_nothrow_move_assignable_v<Surface>);
    // Step 1: validate and clear direct references before erasing the surface from its sector.
    if (!isValidSurfaceRef(level, surfaceRef))
    {
        return false;
    }

    clearAdjoinsTargetingSurface(level, surfaceRef);

    // Step 2: remove and renumber surfaces in the owner sector.
    Sector& owner = level.sectors[static_cast<std::size_t>(surfaceRef.sector)];
    owner.surfaces.erase(owner.surfaces.begin() + static_cast<std::ptrdiff_t>(surfaceRef.surface));
    renumberSectorSurfaces(owner);

    // Step 3: shift references that pointed past the deleted surface in the same sector.
    shiftAdjoinsAfterSurfaceDelete(level, surfaceRef);

    return true;
}

bool levelDeleteSurface(Level& level, const SurfaceRef surfaceRef) noexcept
{
    if ( !isValidSurfaceRef(level, surfaceRef) )
    {
        return false;
    }
    try
    {
        detail::GeometryCogReferences references(level);
        if ( !deleteSurfaceGeometry(level, surfaceRef) )
        {
            return false;
        }
        references.eraseSurface(surfaceRef);
        references.publish(level);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

} // namespace sed::level
