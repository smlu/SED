#include "ui/uv/follow_active_quads_utils.hpp"

#include "ui/uv/uv_surface_geometry_utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <deque>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>


namespace sed::ui::uv
{
namespace
{

constexpr double kWorldPointToleranceSquared = 1.0e-8;
// The cell's 3D diagonal is exactly the accepted point tolerance, so sharing
// one quantized key cannot merge points that the geometry matcher rejects.
constexpr double kWorldPointQuantization = 5.773502691896258e-5;
constexpr qreal kUvEpsilon = 1.0e-12;

struct PointKey
{
    double x{};
    double y{};
    double z{};

    auto operator<=>(const PointKey &) const = default;
};

struct EdgeKey
{
    PointKey a{};
    PointKey b{};

    bool operator==(const EdgeKey &) const = default;
};

struct EdgeKeyHash
{
    std::size_t operator()(const EdgeKey &edge) const noexcept
    {
        std::size_t value = std::hash<double>{}(edge.a.x);
        const auto add = [&value](const double component) {
            value ^= std::hash<double>{}(component) + 0x9e3779b9u + (value << 6u) + (value >> 2u);
        };
        add(edge.a.y);
        add(edge.a.z);
        add(edge.b.x);
        add(edge.b.y);
        add(edge.b.z);
        return value;
    }
};

PointKey pointKey(const TVector &point)
{
    const auto quantize = [](const double value)
    {
        const double cell = value / kWorldPointQuantization;
        // At this range representable coordinates already exceed the matching tolerance.
        return std::isfinite(cell) ? std::round(cell) * kWorldPointQuantization : value;
    };
    return PointKey{
        quantize(point.x),
        quantize(point.y),
        quantize(point.z),
    };
}

EdgeKey edgeKey(const TVector &a, const TVector &b)
{
    PointKey first = pointKey(a);
    PointKey second = pointKey(b);
    if (second < first)
    {
        std::swap(first, second);
    }
    return EdgeKey{first, second};
}

bool sameWorldPoint(const TVector &lhs, const TVector &rhs) noexcept
{
    return (lhs - rhs).LengthSqr() <= kWorldPointToleranceSquared;
}

struct QuadFace
{
    std::size_t islandIndex{};
    SurfaceRef ref{};
    std::array<TVector, 4> points{};
    std::array<EdgeKey, 4> edgeKeys{};
};

struct SharedEdge
{
    std::size_t neighbor{};
    std::size_t sourceEdge{};
    std::size_t neighborEdge{};
    bool neighborSameDirection{};
};

std::optional<QuadFace> collectQuad(const Level &level, const UvIslandState &island, const std::size_t islandIndex)
{
    QuadFace face{};
    face.islandIndex = islandIndex;
    face.ref = island.ref;
    for (std::size_t i = 0; i < face.points.size(); ++i)
    {
        const std::optional<TVector> point = uvSurfaceVertexPosition(level, island.ref, i);
        if (!point)
        {
            return std::nullopt;
        }
        face.points[i] = *point;
    }
    for (std::size_t i = 0; i < face.edgeKeys.size(); ++i)
    {
        face.edgeKeys[i] = edgeKey(face.points[i], face.points[(i + 1u) % face.points.size()]);
    }
    return face;
}

bool edgesMatch(const QuadFace &a, const std::size_t aEdge, const QuadFace &b, const std::size_t bEdge,
                bool &sameDirection)
{
    const TVector &a0 = a.points[aEdge];
    const TVector &a1 = a.points[(aEdge + 1u) % 4u];
    const TVector &b0 = b.points[bEdge];
    const TVector &b1 = b.points[(bEdge + 1u) % 4u];
    if (sameWorldPoint(a0, b0) && sameWorldPoint(a1, b1))
    {
        sameDirection = true;
        return true;
    }
    if (sameWorldPoint(a0, b1) && sameWorldPoint(a1, b0))
    {
        sameDirection = false;
        return true;
    }
    return false;
}

double edgeLength(const QuadFace &face, const std::size_t edge)
{
    return static_cast<double>((face.points[(edge + 1u) % 4u] - face.points[edge]).Length());
}

struct EdgeLoopAverages
{
    std::vector<std::array<double, 4>> values{};
};

EdgeLoopAverages calculateEdgeLoopAverages(const std::vector<QuadFace> &faces)
{
    const std::size_t edgeCount = faces.size() * 4u;
    std::vector<std::size_t> parent(edgeCount);
    std::iota(parent.begin(), parent.end(), std::size_t{0});
    auto findRoot = [&parent](std::size_t edge) {
        std::size_t root = edge;
        while (parent[root] != root)
        {
            root = parent[root];
        }
        while (parent[edge] != edge)
        {
            const std::size_t next = parent[edge];
            parent[edge] = root;
            edge = next;
        }
        return root;
    };
    auto unite = [&parent, &findRoot](const std::size_t lhs, const std::size_t rhs) {
        const std::size_t lhsRoot = findRoot(lhs);
        const std::size_t rhsRoot = findRoot(rhs);
        if (lhsRoot != rhsRoot)
        {
            parent[rhsRoot] = lhsRoot;
        }
    };

    // Opposite edges are one quad edge loop. A canonical world-edge index
    // joins coincident surface-edge instances without an all-pairs face scan.
    std::unordered_map<EdgeKey, std::size_t, EdgeKeyHash> firstOccurrence{};
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
    {
        unite(faceIndex * 4u, faceIndex * 4u + 2u);
        unite(faceIndex * 4u + 1u, faceIndex * 4u + 3u);
        for (std::size_t edge = 0; edge < 4u; ++edge)
        {
            const std::size_t node = faceIndex * 4u + edge;
            const auto [entry, inserted] = firstOccurrence.try_emplace(faces[faceIndex].edgeKeys[edge], node);
            if (!inserted)
            {
                unite(node, entry->second);
            }
        }
    }

    std::vector<double> sums(edgeCount, 0.0);
    std::vector<std::size_t> counts(edgeCount, 0u);
    std::unordered_set<EdgeKey, EdgeKeyHash> countedEdges{};
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
    {
        for (std::size_t edge = 0; edge < 4u; ++edge)
        {
            const std::size_t root = findRoot(faceIndex * 4u + edge);
            if (!countedEdges.insert(faces[faceIndex].edgeKeys[edge]).second)
            {
                continue;
            }
            sums[root] += edgeLength(faces[faceIndex], edge);
            ++counts[root];
        }
    }

    EdgeLoopAverages result{};
    result.values.resize(faces.size());
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
    {
        for (std::size_t edge = 0; edge < 4u; ++edge)
        {
            const std::size_t root = findRoot(faceIndex * 4u + edge);
            result.values[faceIndex][edge] =
                counts[root] > 0u ? sums[root] / static_cast<double>(counts[root]) : edgeLength(faces[faceIndex], edge);
        }
    }
    return result;
}

std::vector<QuadFace> collectAllLevelQuads(const Level &level)
{
    std::vector<QuadFace> faces{};
    for (std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex)
    {
        const Sector &sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            if (sector.surfaces[surfaceIndex].vertices.size() != 4u)
            {
                continue;
            }
            UvIslandState island{};
            island.ref = SurfaceRef{static_cast<std::int32_t>(sectorIndex), static_cast<std::int32_t>(surfaceIndex)};
            if (const std::optional<QuadFace> face = collectQuad(level, island, faces.size()))
            {
                faces.push_back(*face);
            }
        }
    }
    return faces;
}

bool validEditableQuad(const UvIslandState &island)
{
    if (island.uv.size() != 4u || island.unwrapUv.size() != 4u)
    {
        return false;
    }
    return std::all_of(island.uv.begin(), island.uv.end(),
                       [](const QPointF &point) { return std::isfinite(point.x()) && std::isfinite(point.y()); });
}

double safeRatio(const double numerator, const double denominator)
{
    if (!std::isfinite(numerator) || !std::isfinite(denominator) || numerator <= static_cast<double>(kUvEpsilon) ||
        denominator <= static_cast<double>(kUvEpsilon))
    {
        return 1.0;
    }
    const double ratio = numerator / denominator;
    return std::isfinite(ratio) && ratio > 0.0 ? ratio : 1.0;
}

bool applyAcrossEdge(const QuadFace &sourceFace, const QuadFace &destinationFace, const SharedEdge &transition,
                     const double factor, std::vector<UvIslandState> &next)
{
    const std::size_t s0 = transition.sourceEdge;
    const std::size_t s1 = (s0 + 1u) % 4u;
    const std::size_t sOuter0 = (s0 + 3u) % 4u;
    const std::size_t sOuter1 = (s0 + 2u) % 4u;

    const std::size_t dEdge = transition.neighborEdge;
    const std::size_t d0 = transition.neighborSameDirection ? dEdge : (dEdge + 1u) % 4u;
    const std::size_t d1 = transition.neighborSameDirection ? (dEdge + 1u) % 4u : dEdge;
    const std::size_t dOuter0 = (d0 + 3u) % 4u == d1 ? (d0 + 1u) % 4u : (d0 + 3u) % 4u;
    const std::size_t dOuter1 = (d1 + 3u) % 4u == d0 ? (d1 + 1u) % 4u : (d1 + 3u) % 4u;

    UvIslandState &source = next[sourceFace.islandIndex];
    UvIslandState &destination = next[destinationFace.islandIndex];
    destination.uv[d0] = source.uv[s0];
    destination.uv[d1] = source.uv[s1];
    destination.uv[dOuter0] = source.uv[s0] + ((source.uv[s0] - source.uv[sOuter0]) * factor);
    destination.uv[dOuter1] = source.uv[s1] + ((source.uv[s1] - source.uv[sOuter1]) * factor);
    destination.unwrapUv = destination.uv;
    return std::all_of(destination.uv.begin(), destination.uv.end(),
                       [](const QPointF &point) { return std::isfinite(point.x()) && std::isfinite(point.y()); });
}

} // namespace

const char *followActiveQuadsErrorMessage(const FollowActiveQuadsError error) noexcept
{
    switch (error)
    {
    case FollowActiveQuadsError::none:
        return "";
    case FollowActiveQuadsError::noActiveFace:
        return "Choose an active selected surface first.";
    case FollowActiveQuadsError::activeFaceNotSelected:
        return "The active surface must be part of the UV selection.";
    case FollowActiveQuadsError::activeFaceNotQuad:
        return "Follow Active Quads needs at least one selected four-vertex surface. Surfaces with other vertex counts are ignored.";
    case FollowActiveQuadsError::invalidActiveUv:
        return "The active surface has incomplete or invalid UV coordinates.";
    case FollowActiveQuadsError::noConnectedQuads:
        return "Follow Active Quads needs at least two selected four-vertex surfaces that share a complete edge. Surfaces with other vertex counts are ignored.";
    case FollowActiveQuadsError::invalidGeometry:
        return "The selected quad geometry or UV coordinates are invalid.";
    }
    return "Follow Active Quads failed.";
}

bool followActiveQuads(const Level &level, std::vector<UvIslandState> &islands, const std::vector<bool> &selectedFaces,
                       const std::optional<SurfaceRef> activeFace, const FollowActiveQuadsMode mode,
                       FollowActiveQuadsReport *report)
{
    FollowActiveQuadsReport nextReport{};
    auto fail = [&](const FollowActiveQuadsError error) {
        nextReport.error = error;
        if (report)
        {
            *report = nextReport;
        }
        return false;
    };
    if (mode != FollowActiveQuadsMode::even && mode != FollowActiveQuadsMode::length
        && mode != FollowActiveQuadsMode::lengthAverage)
    {
        return fail(FollowActiveQuadsError::invalidGeometry);
    }
    const auto surfaceVertexCount = [&level](const SurfaceRef ref) -> std::size_t {
        if (ref.sector < 0 || ref.surface < 0 || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
        {
            return 0u;
        }
        const Sector &sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
        {
            return 0u;
        }
        return sector.surfaces[static_cast<std::size_t>(ref.surface)].vertices.size();
    };

    std::optional<std::size_t> requestedActiveIsland{};
    if (activeFace)
    {
        for (std::size_t i = 0; i < islands.size(); ++i)
        {
            if (islands[i].ref.sector == activeFace->sector
                && islands[i].ref.surface == activeFace->surface)
            {
                requestedActiveIsland = i;
                break;
            }
        }
        if (!requestedActiveIsland
            || *requestedActiveIsland >= selectedFaces.size()
            || !selectedFaces[*requestedActiveIsland])
        {
            return fail(requestedActiveIsland
                    ? FollowActiveQuadsError::activeFaceNotSelected
                    : FollowActiveQuadsError::noActiveFace);
        }
    }

    std::optional<std::size_t> activeIsland{};
    if (requestedActiveIsland
        && surfaceVertexCount(islands[*requestedActiveIsland].ref) == 4u)
    {
        activeIsland = requestedActiveIsland;
    }
    else if (!requestedActiveIsland)
    {
        // A caller without an explicit active face gets deterministic legacy
        // behavior. A selected non-quad active face is handled after adjacency
        // is known, so an isolated first quad cannot hide a valid later strip.
        for (std::size_t i = 0; i < islands.size(); ++i)
        {
            if (i < selectedFaces.size() && selectedFaces[i]
                && surfaceVertexCount(islands[i].ref) == 4u)
            {
                activeIsland = i;
                break;
            }
        }
    }
    if (activeIsland && !validEditableQuad(islands[*activeIsland]))
    {
        return fail(FollowActiveQuadsError::invalidActiveUv);
    }

    std::vector<QuadFace> faces{};
    std::optional<std::size_t> activeFaceIndex{};
    for (std::size_t i = 0; i < islands.size(); ++i)
    {
        if (i >= selectedFaces.size() || !selectedFaces[i])
        {
            continue;
        }
        if (surfaceVertexCount(islands[i].ref) != 4u)
        {
            ++nextReport.ignoredNonQuadCount;
            continue;
        }
        if (!validEditableQuad(islands[i]))
        {
            return fail(FollowActiveQuadsError::invalidGeometry);
        }
        const std::optional<QuadFace> face = collectQuad(level, islands[i], i);
        if (!face)
        {
            return fail(FollowActiveQuadsError::invalidGeometry);
        }
        if (activeIsland && i == *activeIsland)
        {
            activeFaceIndex = faces.size();
        }
        faces.push_back(*face);
    }
    if (faces.empty())
    {
        return fail(activeFace
                ? FollowActiveQuadsError::activeFaceNotQuad
                : FollowActiveQuadsError::noActiveFace);
    }

    std::vector<std::vector<SharedEdge>> adjacency(faces.size());
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
    {
        for (std::size_t sourceEdge = 0; sourceEdge < 4u; ++sourceEdge)
        {
            std::vector<SharedEdge> matches{};
            for (std::size_t otherFace = 0; otherFace < faces.size(); ++otherFace)
            {
                if (otherFace == faceIndex)
                {
                    continue;
                }
                for (std::size_t otherEdge = 0; otherEdge < 4u; ++otherEdge)
                {
                    bool sameDirection = false;
                    if (edgesMatch(
                            faces[faceIndex], sourceEdge,
                            faces[otherFace], otherEdge,
                            sameDirection))
                    {
                        matches.push_back(SharedEdge{
                            otherFace, sourceEdge, otherEdge, sameDirection});
                    }
                }
            }
            if (matches.size() == 1u)
            {
                adjacency[faceIndex].push_back(matches.front());
            }
            else if (matches.size() > 1u)
            {
                ++nextReport.ambiguousEdgeCount;
            }
        }
        std::stable_sort(adjacency[faceIndex].begin(), adjacency[faceIndex].end(),
                         [&faces](const SharedEdge &lhs, const SharedEdge &rhs) {
                             if (lhs.sourceEdge != rhs.sourceEdge)
                             {
                                 return lhs.sourceEdge < rhs.sourceEdge;
                             }
                             return faces[lhs.neighbor].islandIndex < faces[rhs.neighbor].islandIndex;
                         });
    }

    if (!activeFaceIndex)
    {
        // A selected n-gon may remain the UV panel's display anchor. Follow
        // Active itself is quad-only, but the anchor should not make a later
        // valid selected strip fail. Prefer the first quad that can actually
        // propagate to another selected quad.
        const auto connected = std::find_if(
            adjacency.begin(), adjacency.end(),
            [](const std::vector<SharedEdge> &edges) { return !edges.empty(); });
        if (connected == adjacency.end())
        {
            return fail(FollowActiveQuadsError::noConnectedQuads);
        }
        activeFaceIndex = static_cast<std::size_t>(
            std::distance(adjacency.begin(), connected));
        activeIsland = faces[*activeFaceIndex].islandIndex;
    }

    const std::vector<QuadFace> allLevelQuads = collectAllLevelQuads(level);
    const EdgeLoopAverages allAverages = calculateEdgeLoopAverages(allLevelQuads);
    std::unordered_map<std::uint64_t, std::size_t> allQuadByRef{};
    auto refKey = [](const SurfaceRef ref) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32u) |
               static_cast<std::uint32_t>(ref.surface);
    };
    for (std::size_t allIndex = 0; allIndex < allLevelQuads.size(); ++allIndex)
    {
        allQuadByRef.emplace(refKey(allLevelQuads[allIndex].ref), allIndex);
    }
    EdgeLoopAverages averages{};
    averages.values.resize(faces.size());
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
    {
        const auto allFace = allQuadByRef.find(refKey(faces[faceIndex].ref));
        if (allFace == allQuadByRef.end())
        {
            return fail(FollowActiveQuadsError::invalidGeometry);
        }
        averages.values[faceIndex] = allAverages.values[allFace->second];
    }
    std::vector<UvIslandState> next = islands;
    for (const QuadFace &face : faces)
    {
        UvIslandState &working = next[face.islandIndex];
        const UvIslandState &original = islands[face.islandIndex];
        for (std::size_t pointIndex = 0; pointIndex < working.uv.size(); ++pointIndex)
        {
            working.uv[pointIndex] = islandStoredFromDisplayUv(original, original.uv[pointIndex]);
        }
        working.unwrapUv = working.uv;
    }
    std::vector<bool> visited(faces.size(), false);
    std::deque<std::size_t> queue{};
    visited[*activeFaceIndex] = true;
    queue.push_back(*activeFaceIndex);
    nextReport.visitedFaceCount = 1u;

    while (!queue.empty())
    {
        const std::size_t sourceIndex = queue.front();
        queue.pop_front();
        for (const SharedEdge &transition : adjacency[sourceIndex])
        {
            if (visited[transition.neighbor])
            {
                continue;
            }

            const QuadFace &sourceFace = faces[sourceIndex];
            const QuadFace &destinationFace = faces[transition.neighbor];
            const std::size_t s0 = transition.sourceEdge;
            const std::size_t s1 = (s0 + 1u) % 4u;
            const std::size_t sOuter0 = (s0 + 3u) % 4u;
            const std::size_t sOuter1 = (s0 + 2u) % 4u;
            const std::size_t dEdge = transition.neighborEdge;
            const std::size_t d0 = transition.neighborSameDirection ? dEdge : (dEdge + 1u) % 4u;
            const std::size_t d1 = transition.neighborSameDirection ? (dEdge + 1u) % 4u : dEdge;
            const std::size_t dOuter0 = (d0 + 3u) % 4u == d1 ? (d0 + 1u) % 4u : (d0 + 3u) % 4u;
            const std::size_t dOuter1 = (d1 + 3u) % 4u == d0 ? (d1 + 1u) % 4u : (d1 + 3u) % 4u;

            double factor = 1.0;
            if (mode == FollowActiveQuadsMode::length)
            {
                const double sourceDepth =
                    static_cast<double>((sourceFace.points[s0] - sourceFace.points[sOuter0]).Length() +
                                        (sourceFace.points[s1] - sourceFace.points[sOuter1]).Length());
                const double destinationDepth =
                    static_cast<double>((destinationFace.points[d0] - destinationFace.points[dOuter0]).Length() +
                                        (destinationFace.points[d1] - destinationFace.points[dOuter1]).Length());
                factor = safeRatio(destinationDepth, sourceDepth);
            }
            const std::size_t destinationSideEdge = ((d1 + 1u) % 4u == dOuter1) ? d1 : dOuter1;
            if (mode == FollowActiveQuadsMode::lengthAverage)
            {
                // These are Blender's averaged l_a[1] and l_b[2] edge loops:
                // one source side and its corresponding destination side.
                factor = safeRatio(averages.values[transition.neighbor][destinationSideEdge],
                                   averages.values[sourceIndex][s1]);
            }

            if (!applyAcrossEdge(sourceFace, destinationFace, transition, factor, next))
            {
                return fail(FollowActiveQuadsError::invalidGeometry);
            }

            visited[transition.neighbor] = true;
            queue.push_back(transition.neighbor);
            ++nextReport.visitedFaceCount;
            ++nextReport.updatedFaceCount;
        }
    }

    if (nextReport.updatedFaceCount == 0u)
    {
        return fail(FollowActiveQuadsError::noConnectedQuads);
    }

    // Propagation is calculated in shared stored UV space. Convert only the
    // reached destination faces back through their existing display basis;
    // the active face and disconnected selection remain byte-for-byte intact.
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex)
    {
        const std::size_t islandIndex = faces[faceIndex].islandIndex;
        if (faceIndex == *activeFaceIndex || !visited[faceIndex])
        {
            next[islandIndex] = islands[islandIndex];
            continue;
        }
        UvIslandState &destination = next[islandIndex];
        const UvIslandState &original = islands[islandIndex];
        for (QPointF &point : destination.uv)
        {
            point = islandDisplayFromStoredUv(original, point);
        }
        destination.unwrapUv = destination.uv;
    }
    nextReport.error = FollowActiveQuadsError::none;
    islands = std::move(next);
    if (report)
    {
        *report = nextReport;
    }
    return true;
}

} // namespace sed::ui::uv
