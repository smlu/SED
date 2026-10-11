#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <cstddef>
#include <unordered_set>
#include <vector>

#include <QVector3D>

namespace sed::render
{

/**
 * Contiguous vertex range inside a composed render buffer.
 *
 * The viewport keeps these ranges for picking, dirty VBO updates, and per-thing
 * cache updates. Storing the type outside GlView lets rebuild and fast-update
 * code share the same range utilities.
 */
struct DrawRange
{
    int first{};
    int count{};
};

/** The two color-role portions of one selected-wire draw range. */
struct SelectionWireRangeParts
{
    DrawRange multi{};
    DrawRange primary{};
};

/** Stable identity for one undirected authored Sector edge. */
struct GeometryWireEdgeKey
{
    std::int32_t sector{};
    std::int32_t first{};
    std::int32_t second{};

    friend constexpr bool operator==(
        const GeometryWireEdgeKey&,
        const GeometryWireEdgeKey&) = default;
};

/** Canonicalizes both directions of a shared polygon edge to one key. */
constexpr GeometryWireEdgeKey geometryWireEdgeKey(
    const std::int32_t sector,
    const std::int32_t first,
    const std::int32_t second) noexcept
{
    return GeometryWireEdgeKey{
        sector,
        std::min(first, second),
        std::max(first, second) };
}

/**
 * Matches authored edge geometry across different Sector-local vertex lists.
 * Adjoined surfaces duplicate their vertices, so index identity alone cannot
 * prevent a secondary edge from blending over the active primary edge.
 */
inline bool geometryWireSegmentsCoincide(
    const QVector3D& a,
    const QVector3D& b,
    const QVector3D& c,
    const QVector3D& d,
    const float tolerance = 1.0e-5f) noexcept
{
    const float toleranceSquared = tolerance * tolerance;
    const auto samePoint = [toleranceSquared](
        const QVector3D& lhs,
        const QVector3D& rhs)
    {
        return (lhs - rhs).lengthSquared() <= toleranceSquared;
    };
    return (samePoint(a, c) && samePoint(b, d))
        || (samePoint(a, d) && samePoint(b, c));
}

/**
 * Returns whether a polygon belongs to the active geometry selection.
 *
 * A selected Sector owns all of its surfaces; an active selected Surface owns
 * only that one polygon.  Secondary-selection emission uses this predicate to
 * avoid drawing the same active edge first in translucent multi blue and then
 * in primary yellow.  Besides wasting fill, that overlap produced a greenish
 * anti-aliased result in multi-selection.
 */
constexpr bool isPrimarySelectionSurface(
    const int activeSector,
    const int activeSurfaceSector,
    const int activeSurface,
    const int candidateSector,
    const int candidateSurface) noexcept
{
    if (activeSurfaceSector >= 0 && activeSurface >= 0)
    {
        return candidateSector == activeSurfaceSector
            && candidateSurface == activeSurface;
    }
    return activeSector >= 0 && candidateSector == activeSector;
}

/**
 * Splits a selected-wire range at the multi-selection/primary boundary.
 * A zero count means that portion is absent. The helper is independent of
 * rendering state so both visible and occluded overlays can retain the same
 * selection colors without duplicating boundary arithmetic.
 */
constexpr SelectionWireRangeParts splitSelectionWireRange(
    const DrawRange range,
    const int multiVertexEnd) noexcept
{
    if (range.first < 0 || range.count <= 0)
    {
        return {};
    }

    const int maxInt = std::numeric_limits<int>::max();
    const int rangeEnd = range.first > (maxInt - range.count)
        ? maxInt
        : range.first + range.count;
    const int split = std::clamp(multiVertexEnd, 0, maxInt);

    SelectionWireRangeParts parts{};
    const int multiEnd = std::min(rangeEnd, split);
    if (multiEnd > range.first)
    {
        parts.multi = DrawRange{ range.first, multiEnd - range.first };
    }
    if (rangeEnd > split)
    {
        const int primaryFirst = std::max(range.first, split);
        if (rangeEnd > primaryFirst)
        {
            parts.primary = DrawRange{ primaryFirst, rangeEnd - primaryFirst };
        }
    }
    return parts;
}

/**
 * Returns whether a separately-rendered surface-normal marker occupies a
 * valid suffix range of the composed selection-line buffer.  Normal markers
 * are appended after the multi-selection prefix and must not be folded into
 * the ordinary primary geometry range; keeping this arithmetic here makes the
 * visible and occluded render passes use the same overflow-safe rule.
 */
constexpr bool isValidSelectionNormalRange(
    const DrawRange normal,
    const int multiVertexEnd,
    const int vertexCount) noexcept
{
    if (vertexCount < 0 || normal.first < 0 || normal.count <= 0
        || normal.first > vertexCount)
    {
        return false;
    }
    const int split = std::clamp(multiVertexEnd, 0, vertexCount);
    return normal.first >= split
        && normal.count <= (vertexCount - normal.first);
}

/**
 * Returns whether a draw range is wholly contained in a vertex buffer.
 *
 * Keep the subtraction form of the upper-bound check: callers commonly read
 * ranges from a cache or a file, so `first + count` must not be allowed to
 * overflow before it is compared with the buffer size.
 */
inline bool isValidDrawRange(const DrawRange range, const std::size_t vertexCount) noexcept
{
    if (range.first < 0 || range.count <= 0)
    {
        return false;
    }

    const auto first = static_cast<std::size_t>(range.first);
    const auto count = static_cast<std::size_t>(range.count);
    return first <= vertexCount && count <= (vertexCount - first);
}

/**
 * Contiguous vertex range inside one cached triangle build.
 *
 * These ranges point back into `MeshPartCache::triBuilds` and are used by the
 * thing fast-update path to update only vertices belonging to a moved thing.
 */
struct TriBuildRange
{
    std::uint32_t buildIndex{};
    int first{};
    int count{};
};

/**
 * Optional world-space bounds for one rendered thing.
 *
 * Non-visual things may temporarily fall back to a point-sized bounds. Visual
 * things expand this from their line and triangle cache vertices.
 */
struct ThingBounds
{
    QVector3D min{};
    QVector3D max{};
    bool valid{};
};

/** Converts a float pick id written into vertex data back into its integer key. */
int pickIdKey(float pickId) noexcept;

/** Adds a point to an optional thing bounds. */
void includePoint(ThingBounds& bounds, float x, float y, float z) noexcept;

/** Adds a point to an optional thing bounds. */
void includePoint(ThingBounds& bounds, const QVector3D& point) noexcept;

/**
 * Appends a draw range or extends the previous one when it is exactly adjacent.
 *
 * Invalid empty ranges are ignored.
 */
void appendOrExtendDrawRange(std::vector<DrawRange>& ranges, int first, int count);

/**
 * Appends a triangle-build range or extends the previous one when it belongs to
 * the same build and is exactly adjacent. Invalid empty ranges are ignored.
 */
void appendOrExtendTriBuildRange(
    std::vector<TriBuildRange>& ranges,
    std::uint32_t buildIndex,
    int first,
    int count);

/**
 * Removes empty ranges, sorts by start vertex, and merges overlapping or
 * adjacent ranges for efficient VBO writes.
 */
void mergeDrawRanges(std::vector<DrawRange>& ranges);

/**
 * Collect contiguous triangle ranges whose three pick-id vertices all belong
 * to the selected set.
 *
 * The caller supplies the pick-id accessor so this remains independent of the
 * interleaved render-vertex type.  Incomplete trailing triangles and invalid
 * (non-finite, non-positive, or out-of-range) pick ids are ignored.  This is
 * deliberately a pure range extraction helper: the regular line overlay
 * still owns the complete polygon wireframe, including shared edges between
 * adjacent selected surfaces.
 */
template <typename PickIdGetter>
void appendSelectedTriangleRanges(
    std::vector<DrawRange>& ranges,
    const int vertexCount,
    const std::unordered_set<int>& selectedPickIds,
    PickIdGetter&& pickIdAt)
{
    if (vertexCount < 3 || selectedPickIds.empty())
    {
        return;
    }

    const int completeVertexCount = vertexCount - (vertexCount % 3);
    int runFirst = -1;
    int runEnd = -1;

    const auto flush = [&]() {
        if (runFirst >= 0)
        {
            ranges.push_back(DrawRange{ runFirst, runEnd - runFirst });
            runFirst = -1;
            runEnd = -1;
        }
    };

    const auto safePickId = [](const float value) noexcept -> int {
        if (!std::isfinite(value) || value <= 0.0f)
        {
            return 0;
        }

        const double rounded = std::floor(static_cast<double>(value) + 0.5);
        if (!std::isfinite(rounded)
            || rounded > static_cast<double>(std::numeric_limits<int>::max()))
        {
            return 0;
        }
        return static_cast<int>(rounded);
    };

    for (int first = 0; first < completeVertexCount; first += 3)
    {
        const int pid0 = safePickId(static_cast<float>(pickIdAt(first + 0)));
        const int pid1 = safePickId(static_cast<float>(pickIdAt(first + 1)));
        const int pid2 = safePickId(static_cast<float>(pickIdAt(first + 2)));
        const bool selectedTriangle =
            pid0 > 0
            && pid0 == pid1
            && pid0 == pid2
            && selectedPickIds.contains(pid0);

        if (!selectedTriangle)
        {
            flush();
            continue;
        }

        if (runFirst < 0)
        {
            runFirst = first;
            runEnd = first + 3;
        }
        else if (runEnd == first)
        {
            runEnd += 3;
        }
        else
        {
            flush();
            runFirst = first;
            runEnd = first + 3;
        }
    }

    flush();
}

/**
 * Visits every edge of a polygon in authored order, including edges shared by
 * adjacent polygons.  Selection wire overlays intentionally do not deduplicate
 * these edges: the shared edge is part of both selected surfaces' complete
 * wireframes and must remain visible when both are selected.
 */
template <typename VertexIndexGetter, typename EdgeVisitor>
void forEachPolygonWireEdge(
    const std::size_t vertexCount,
    VertexIndexGetter&& vertexIndexAt,
    EdgeVisitor&& visitEdge)
{
    if (vertexCount < 2)
    {
        return;
    }

    for (std::size_t edge = 0; edge < vertexCount; ++edge)
    {
        visitEdge(
            vertexIndexAt(edge),
            vertexIndexAt((edge + 1u) % vertexCount));
    }
}

} // namespace sed::render
