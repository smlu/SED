#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

namespace sed::render::picking
{

/** Marquee origin used by the Select tool and temporary Alt-transform mode. */
enum class MarqueeSelectionSource : std::uint8_t
{
    Select,
    TransformTool,
};

/**
 * Surface/Sector edge picking follows the wireframe that the user can see.
 * A wireframe base render always exposes edges; in a filled render the
 * explicit level-wire overlay owns them.  Filled rendering without the
 * overlay must not make invisible/adjoin edges clickable.
 */
[[nodiscard]] constexpr bool surfaceWirePickingEnabled(
    const bool showSolid,
    const bool showVertices,
    const bool showWireframeOverlay) noexcept
{
    return (!showSolid && !showVertices) || showWireframeOverlay;
}

/**
 * A projected wire hit is useful only when it belongs to the narrow cursor
 * edge group.  The absolute radius keeps picking usable on high-DPI displays,
 * while the small slack from the closest edge preserves both owners of one
 * shared polygon edge without admitting a nearby, unrelated edge.
 */
[[nodiscard]] inline bool surfaceWireCandidateNearCursor(
    const float distanceSquared,
    const float closestDistanceSquared,
    const float radiusPx = 6.0f,
    const float closestSlackPx = 1.5f) noexcept
{
    if (!std::isfinite(distanceSquared)
        || !std::isfinite(closestDistanceSquared))
    {
        return false;
    }
    const float radius = std::max(radiusPx, 0.0f);
    if (distanceSquared > radius * radius)
    {
        return false;
    }
    const float distance = std::sqrt(std::max(distanceSquared, 0.0f));
    const float closest = std::sqrt(std::max(closestDistanceSquared, 0.0f));
    return distance <= closest + std::max(closestSlackPx, 0.0f);
}

/**
 * Ordinary face/edge/vertex picking cannot see through opaque scene depth.
 * X-Ray traversal or the explicit through-selection chord deliberately
 * bypasses that constraint. Keeping the exact-subelement decision shared
 * prevents point, marquee, GPU, and CPU fallback paths from drifting apart.
 */
[[nodiscard]] inline float surfaceSelectionDepthTolerance(
    const float sceneDepth,
    const float requestedTolerance = 0.0f) noexcept
{
    if (!std::isfinite(sceneDepth))
    {
        return 0.0f;
    }
    // Candidate depth is calculated on the CPU while scene depth has made a
    // raster/depth-buffer round trip. Permit a small float-safe band rather
    // than a fixed normalized-depth offset, which grows into a large world
    // distance in far perspective views and can expose near-behind geometry.
    constexpr std::size_t rasterUlps = 4u;
    float rasterUpperBound = sceneDepth;
    for (std::size_t step = 0; step < rasterUlps; ++step)
    {
        rasterUpperBound = std::nextafter(
            rasterUpperBound,
            std::numeric_limits<float>::infinity());
    }
    const float floatSafeTolerance = rasterUpperBound - sceneDepth;
    const float safeRequestedTolerance =
        std::isfinite(requestedTolerance)
        ? std::max(requestedTolerance, 0.0f)
        : 0.0f;
    return std::max(safeRequestedTolerance, floatSafeTolerance);
}

[[nodiscard]] inline bool surfaceSelectionCandidatePassesSceneDepth(
    const float candidateDepth,
    const float sceneDepth,
    const bool throughSelection,
    const bool xrayTraversal,
    const float tolerance = 0.0f) noexcept
{
    return throughSelection
        || xrayTraversal
        || (std::isfinite(candidateDepth)
            && std::isfinite(sceneDepth)
            && candidateDepth
                <= sceneDepth
                    + surfaceSelectionDepthTolerance(
                        sceneDepth, tolerance));
}

[[nodiscard]] inline bool surfaceWireCandidatePassesSceneDepth(
    const float candidateDepth,
    const float sceneDepth,
    const bool throughSelection,
    const bool xrayTraversal,
    const float tolerance = 0.0f) noexcept
{
    return surfaceSelectionCandidatePassesSceneDepth(
        candidateDepth,
        sceneDepth,
        throughSelection,
        xrayTraversal,
        tolerance);
}

/**
 * Sector/object-volume scene depth is deliberately two-sided. Ordinary
 * Face/Edge/Vertex and light-gizmo picking instead request a front-facing
 * Level pass: a single-sided backface is not rendered, selectable, or allowed
 * to hide an otherwise visible eligible candidate. Through/X-Ray traversal
 * uses the two-sided form.
 *
 * Thing meshes retain their normal shaded-pass culling policy; sprites and
 * explicitly double-sided Thing meshes never cull.
 */
[[nodiscard]] constexpr bool sceneDepthBatchCullBackFaces(
    const bool levelGeometry,
    const bool sprite,
    const bool doubleSided,
    const bool cullSingleSidedLevelBackFaces = false) noexcept
{
    if (sprite || doubleSided)
    {
        return false;
    }
    return !levelGeometry || cullSingleSidedLevelBackFaces;
}

/**
 * Ordinary solid-Level Surface candidates match front-facing render
 * eligibility. Through/X-Ray candidates are deliberately two-sided so a
 * designer can explicitly select backfaces and continue behind them.
 */
[[nodiscard]] constexpr bool solidLevelPointCandidatesCullBackFaces(
    const bool traverseBoundary) noexcept
{
    return !traverseBoundary;
}

/**
 * The selectable family determines whether a solid Level boundary represents
 * an enclosing volume or visible rendered geometry. Sector picking targets
 * the complete volume and therefore remains two-sided. Surface, Thing, and
 * Preview Auto picking use only visible single-sided faces during an ordinary
 * point pick, so an outside-sector backface cannot become an invisible
 * occluder. Explicit through/X-Ray traversal restores the two-sided pass.
 */
enum class SolidLevelOcclusionPickFamily : std::uint8_t
{
    Sector,
    Surface,
    Thing,
    PreviewAuto,
};

[[nodiscard]] constexpr bool solidLevelOccludersCullBackFaces(
    const SolidLevelOcclusionPickFamily family,
    const bool traverseBoundary) noexcept
{
    return !traverseBoundary
        && family != SolidLevelOcclusionPickFamily::Sector;
}

/** CPU counterpart of `solidLevelOccludersCullBackFaces`. */
[[nodiscard]] constexpr bool solidLevelOccluderPassesFacing(
    const SolidLevelOcclusionPickFamily family,
    const bool doubleSided,
    const bool backFacing,
    const bool traverseBoundary) noexcept
{
    return !solidLevelOccludersCullBackFaces(family, traverseBoundary)
        || doubleSided
        || !backFacing;
}

/**
 * CPU ray hits do not make a normalized depth-buffer round trip. Keep every
 * hit on the nearest plane within a small scale-aware double precision band,
 * while rejecting the first meaningfully deeper wall. Through/X-Ray
 * traversal deliberately bypasses this boundary.
 */
[[nodiscard]] inline double surfaceRayDistanceBoundaryTolerance(
    const double nearestDistance,
    const double requestedTolerance = 0.0) noexcept
{
    if (!std::isfinite(nearestDistance))
    {
        return 0.0;
    }
    constexpr double scaleUlps = 256.0;
    const double scale = std::max(std::abs(nearestDistance), 1.0);
    const double numericTolerance =
        scale * std::numeric_limits<double>::epsilon() * scaleUlps;
    const double safeRequestedTolerance =
        std::isfinite(requestedTolerance)
        ? std::max(requestedTolerance, 0.0)
        : 0.0;
    return std::max(numericTolerance, safeRequestedTolerance);
}

[[nodiscard]] inline bool surfaceRayCandidatePassesNearestBoundary(
    const double candidateDistance,
    const double nearestDistance,
    const bool traverseBoundary,
    const double tolerance = 0.0) noexcept
{
    return traverseBoundary
        || (std::isfinite(candidateDistance)
            && std::isfinite(nearestDistance)
            && candidateDistance <= nearestDistance
                + surfaceRayDistanceBoundaryTolerance(
                    nearestDistance, tolerance));
}

enum class SurfacePointElement : std::uint8_t
{
    Face,
    Edge,
    Vertex,
};

/** Edge/Vertex point picking uses one logical-pixel radius in every path. */
inline constexpr float kSurfaceSubElementPickRadiusPx = 16.0f;

/**
 * Maximum world-space `t` for a normalized CPU pick ray, measured before
 * normalization from the chosen origin to the unprojected far endpoint. A
 * projection's far-plane value is camera-axis distance: it can cover only
 * part of an orthographic near/far segment and is shorter than an off-axis
 * perspective ray.
 */
[[nodiscard]] inline double pickRaySegmentMaxDistance(
    const double deltaX,
    const double deltaY,
    const double deltaZ) noexcept
{
    if (!std::isfinite(deltaX)
        || !std::isfinite(deltaY)
        || !std::isfinite(deltaZ))
    {
        return 0.0;
    }
    return std::hypot(std::hypot(deltaX, deltaY), deltaZ);
}

/**
 * Converts a logical horizontal pixel radius to world units at a perspective
 * ray distance. `tanHalfFovX` is horizontal, so the matching viewport extent
 * is the logical width (DPR cancels between both quantities).
 */
[[nodiscard]] inline double perspectivePickWorldTolerance(
    const double rayDistance,
    const double tanHalfFovX,
    const int logicalViewportWidth,
    const double logicalPixelRadius,
    const double minimumTolerance = 0.001) noexcept
{
    if (!std::isfinite(rayDistance) || rayDistance <= 0.0
        || !std::isfinite(tanHalfFovX) || tanHalfFovX <= 0.0
        || logicalViewportWidth <= 0
        || !std::isfinite(logicalPixelRadius)
        || logicalPixelRadius <= 0.0)
    {
        return std::max(minimumTolerance, 0.0);
    }
    const double worldPerLogicalPixel =
        (2.0 * rayDistance * tanHalfFovX)
        / static_cast<double>(logicalViewportWidth);
    return std::max(
        worldPerLogicalPixel * logicalPixelRadius,
        std::max(minimumTolerance, 0.0));
}

/** Ordinary sub-element picking ignores a back-facing single-sided owner. */
[[nodiscard]] constexpr bool surfacePointOwnerPassesFacing(
    const bool ownerDoubleSided,
    const bool ownerBackFacing,
    const bool traverseBoundary) noexcept
{
    return traverseBoundary || ownerDoubleSided || !ownerBackFacing;
}

/**
 * Use the exact pointer pixel when it contains geometry. Neighbor samples are
 * only a shared-edge/background fallback, so the depth of a sloped visible
 * face is not compared with a legitimately nearer adjacent pixel.
 */
[[nodiscard]] inline float resolveSurfacePointSceneDepth(
    const std::span<const float> crossSamples,
    const float backgroundDepth = 1.0f) noexcept
{
    if (crossSamples.empty())
    {
        return backgroundDepth;
    }
    const float exactDepth = crossSamples.front();
    if (std::isfinite(exactDepth) && exactDepth < backgroundDepth)
    {
        return exactDepth;
    }

    float result = backgroundDepth;
    for (const float sampledDepth : crossSamples.subspan(1))
    {
        if (std::isfinite(sampledDepth))
        {
            result = std::min(result, sampledDepth);
        }
    }
    return result;
}

struct SurfacePointDepthCandidate
{
    std::int32_t index{-1};
    float depth{1.0f};
};

[[nodiscard]] inline std::vector<std::int32_t>
collectSurfacePointDepthCandidates(
    const std::span<const SurfacePointDepthCandidate> candidates,
    const float sceneDepth,
    const bool throughSelection,
    const bool xrayTraversal,
    const float tolerance = 0.0f)
{
    std::vector<std::int32_t> result{};
    result.reserve(candidates.size());
    for (const SurfacePointDepthCandidate& candidate : candidates)
    {
        if (candidate.index >= 0
            && surfaceSelectionCandidatePassesSceneDepth(
                candidate.depth,
                sceneDepth,
                throughSelection,
                xrayTraversal,
                tolerance))
        {
            result.push_back(candidate.index);
        }
    }
    return result;
}

/**
 * X-Ray has identical depth semantics for Select and Alt-transform marquees.
 * Keeping that decision in one pure policy prevents the two input paths from
 * drifting back to different occlusion behavior.
 */
[[nodiscard]] constexpr bool marqueeIgnoresSceneDepth(
    const bool xrayEnabled,
    const MarqueeSelectionSource /*source*/) noexcept
{
    return xrayEnabled;
}

/** One already-projected marquee candidate used by pure policy tests/callers. */
struct MarqueeDepthCandidate
{
    std::int32_t index{-1};
    float depth{1.0f};
    bool layerVisible{true};
};

/**
 * Normalized screen-space rectangle used by the X-Ray marquee broad phase.
 * This deliberately stays independent of Qt so the containment/refinement
 * policy can be tested without a viewport or an OpenGL context.
 */
struct MarqueeRect
{
    double left{};
    double top{};
    double right{};
    double bottom{};
};

enum class MarqueeBoundsRelation : std::uint8_t
{
    Disjoint,
    PartialOverlap,
    Contained,
};

/**
 * Classifies a projected candidate bounds rectangle against the marquee.
 * Contained means the candidate bounds are fully inside the marquee. A
 * partial overlap is deliberately conservative and needs triangle refinement
 * when cached triangle geometry exists; otherwise the bounds remain the only
 * available pick representation and are accepted by the caller.
 */
[[nodiscard]] constexpr MarqueeBoundsRelation classifyMarqueeBounds(
    const MarqueeRect& marquee,
    const MarqueeRect& projectedBounds) noexcept
{
    if (projectedBounds.right < marquee.left
        || projectedBounds.left > marquee.right
        || projectedBounds.bottom < marquee.top
        || projectedBounds.top > marquee.bottom)
    {
        return MarqueeBoundsRelation::Disjoint;
    }

    if (marquee.left <= projectedBounds.left
        && marquee.top <= projectedBounds.top
        && marquee.right >= projectedBounds.right
        && marquee.bottom >= projectedBounds.bottom)
    {
        return MarqueeBoundsRelation::Contained;
    }

    return MarqueeBoundsRelation::PartialOverlap;
}

/** Return whether a partial projected-bound hit needs exact triangle tests. */
[[nodiscard]] constexpr bool marqueeBoundsNeedTriangleRefinement(
    const MarqueeBoundsRelation relation,
    const bool hasTriangleGeometry) noexcept
{
    return relation == MarqueeBoundsRelation::PartialOverlap && hasTriangleGeometry;
}

/**
 * Returns whether a projected candidate survives scene-depth filtering.
 * Hidden-layer filtering remains separate and explicit in the caller; this
 * helper only owns the X-Ray/ordinary depth decision.
 */
[[nodiscard]] inline bool marqueeCandidatePassesSceneDepth(
    const float candidateDepth,
    const float sceneDepth,
    const bool xrayEnabled,
    const float tolerance = 0.0f) noexcept
{
    if (marqueeIgnoresSceneDepth(xrayEnabled, MarqueeSelectionSource::Select))
    {
        return true;
    }

    return surfaceSelectionCandidatePassesSceneDepth(
        candidateDepth,
        sceneDepth,
        false,
        false,
        tolerance);
}

/**
 * Small pure candidate filter used by regression tests and non-GL marquee
 * callers. Candidates are assumed to have already intersected the drag box;
 * hidden-layer entries are intentionally rejected here.
 */
[[nodiscard]] inline std::vector<std::int32_t> collectMarqueeDepthCandidates(
    const std::vector<MarqueeDepthCandidate>& candidates,
    const float sceneDepth,
    const bool xrayEnabled,
    const float tolerance = 0.0f)
{
    std::vector<std::int32_t> result{};
    result.reserve(candidates.size());
    for (const MarqueeDepthCandidate& candidate : candidates)
    {
        if (candidate.index < 0 || !candidate.layerVisible)
        {
            continue;
        }
        if (marqueeCandidatePassesSceneDepth(
                candidate.depth,
                sceneDepth,
                xrayEnabled,
                tolerance))
        {
            result.push_back(candidate.index);
        }
    }
    return result;
}

template<typename Candidate, typename Selection>
[[nodiscard]] constexpr bool candidateBelongsToSelection(
    const Candidate& candidate,
    const Candidate& activeCandidate,
    const Selection& completeSelection) noexcept
{
    return candidate == activeCandidate || completeSelection.contains(candidate);
}

struct ViewportPickCycleContext
{
    std::int32_t widgetX{};
    std::int32_t widgetY{};
    std::int32_t pickMode{};
    std::int32_t surfaceSubMode{};
    std::int32_t transformTool{};
    std::int32_t selectionModifiers{};
    std::uint64_t viewSerial{};
    std::chrono::steady_clock::time_point timestamp{};
    bool valid{};
};

/** Entity families exposed by Game Preview's mode-independent Auto picker. */
enum class PreviewAutoPickKind : std::uint8_t
{
    Thing,
    BakedLight,
    Surface,
    Sector,
};

/**
 * One already-filtered Preview pick candidate. `primaryIndex` is the Thing,
 * light, or sector index; Surface uses it for the sector and `secondaryIndex`
 * for the local surface index. Depth is normalized only for ordering, so the
 * same policy works for both the GPU and CPU fallback pickers.
 */
struct PreviewAutoPickCandidate
{
    PreviewAutoPickKind kind{PreviewAutoPickKind::Thing};
    std::int32_t primaryIndex{-1};
    std::int32_t secondaryIndex{-1};
    double depth{};

    [[nodiscard]] friend constexpr bool operator==(
        const PreviewAutoPickCandidate& lhs,
        const PreviewAutoPickCandidate& rhs) noexcept
    {
        return lhs.kind == rhs.kind
            && lhs.primaryIndex == rhs.primaryIndex
            && lhs.secondaryIndex == rhs.secondaryIndex;
    }
};

/**
 * Deterministically sorts and de-duplicates Preview Auto candidates. Without
 * X-Ray, callers pass the first visible level depth and this policy rejects
 * entities behind that geometry boundary. Surface and Sector remain separate
 * candidates at the same boundary so repeated clicks can inspect both without
 * changing the Editor's active edit mode.
 */
[[nodiscard]] inline std::vector<PreviewAutoPickCandidate>
orderedPreviewAutoPickCandidates(
    std::vector<PreviewAutoPickCandidate> candidates,
    const double firstGeometryDepth,
    const bool traverseBehindGeometry,
    const double depthTolerance = 1.0e-5)
{
    std::erase_if(
        candidates,
        [firstGeometryDepth, traverseBehindGeometry, depthTolerance](
            const PreviewAutoPickCandidate& candidate)
        {
            if (candidate.primaryIndex < 0 || !std::isfinite(candidate.depth))
            {
                return true;
            }
            if (candidate.kind == PreviewAutoPickKind::Surface
                && candidate.secondaryIndex < 0)
            {
                return true;
            }
            return !traverseBehindGeometry
                && std::isfinite(firstGeometryDepth)
                && candidate.depth > firstGeometryDepth
                    + std::max(depthTolerance, 0.0);
        });

    const auto familyOrder = [](const PreviewAutoPickKind kind) noexcept
    {
        switch (kind)
        {
            case PreviewAutoPickKind::Thing: return 0;
            case PreviewAutoPickKind::BakedLight: return 1;
            case PreviewAutoPickKind::Surface: return 2;
            case PreviewAutoPickKind::Sector: return 3;
        }
        return 4;
    };
    std::ranges::sort(
        candidates,
        [&](const PreviewAutoPickCandidate& lhs,
            const PreviewAutoPickCandidate& rhs)
        {
            if (lhs.depth != rhs.depth)
            {
                return lhs.depth < rhs.depth;
            }
            const int lhsFamily = familyOrder(lhs.kind);
            const int rhsFamily = familyOrder(rhs.kind);
            if (lhsFamily != rhsFamily)
            {
                return lhsFamily < rhsFamily;
            }
            if (lhs.primaryIndex != rhs.primaryIndex)
            {
                return lhs.primaryIndex < rhs.primaryIndex;
            }
            return lhs.secondaryIndex < rhs.secondaryIndex;
        });
    std::vector<PreviewAutoPickCandidate> unique{};
    unique.reserve(candidates.size());
    for (const PreviewAutoPickCandidate& candidate : candidates)
    {
        if (std::find(unique.begin(), unique.end(), candidate) == unique.end())
        {
            unique.push_back(candidate);
        }
    }
    return unique;
}

[[nodiscard]] inline bool shouldContinueViewportPickCycle(
    const ViewportPickCycleContext& previous,
    const ViewportPickCycleContext& current,
    const int clickTolerancePx = 8,
    const std::chrono::milliseconds timeout = std::chrono::milliseconds{1500}) noexcept
{
    if (!previous.valid || !current.valid)
    {
        return false;
    }
    if (previous.pickMode != current.pickMode
        || previous.surfaceSubMode != current.surfaceSubMode
        || previous.transformTool != current.transformTool
        || previous.selectionModifiers != current.selectionModifiers
        || previous.viewSerial != current.viewSerial)
    {
        return false;
    }
    const std::int64_t dx = static_cast<std::int64_t>(current.widgetX) - previous.widgetX;
    const std::int64_t dy = static_cast<std::int64_t>(current.widgetY) - previous.widgetY;
    const std::int64_t manhattanDistance = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    if (manhattanDistance > clickTolerancePx)
    {
        return false;
    }
    if (current.timestamp < previous.timestamp)
    {
        return false;
    }
    return (current.timestamp - previous.timestamp) <= timeout;
}

[[nodiscard]] constexpr bool shouldTraverseOccludedCandidates(
    const bool xrayEnabled,
    const bool repeatedClick) noexcept
{
    return xrayEnabled && repeatedClick;
}

/**
 * Returns whether the current click may advance an established point-pick
 * cycle. Ordinary clicks and the configured Add chord keep their existing
 * front-boundary cycling. Surface Ignore Filters is an explicit through-pick
 * operation, so repeated clicks with that exact chord must also advance between
 * back-facing/coplanar owners (notably the two sides of an adjoin portal).
 * Other filter/remove/path chords remain non-cycling.
 */
[[nodiscard]] constexpr bool pointSelectionClickCanCycle(
    const bool repeatedClick,
    const bool previewAutoSelection,
    const bool ordinaryCycleEligible,
    const bool surfaceMode,
    const bool exactSurfaceIgnoreFiltersChord) noexcept
{
    return repeatedClick
        && (previewAutoSelection
            || ordinaryCycleEligible
            || (surfaceMode && exactSurfaceIgnoreFiltersChord));
}

/** Surface candidate peeling required before resolving a point selection. */
[[nodiscard]] constexpr bool surfacePointPickNeedsFullPeel(
    const bool allowThroughSurfacePick,
    const bool edgeOrVertexPick,
    const bool wireXraySurfacePick,
    const bool cyclePick,
    const bool xrayCycleTraversal) noexcept
{
    return xrayCycleTraversal
        || (allowThroughSurfacePick
            && (edgeOrVertexPick || wireXraySurfacePick || cyclePick));
}

/** Returns whether an eligible X-ray click may step past an already-selected
 * front candidate without first reselecting it.
 *
 * The caller admits plain clicks and its exact Add chord. Remove/path/filter
 * operations deliberately retain the front candidate so the exact clicked
 * member reaches the selection dispatcher.
 */
[[nodiscard]] constexpr bool shouldSkipSelectedFrontCandidate(
    const bool xrayEnabled,
    const bool cycleEligibleClick,
    const bool frontCandidateSelected) noexcept
{
    return xrayEnabled && cycleEligibleClick && frontCandidateSelected;
}

template<typename Depth>
[[nodiscard]] constexpr bool candidatePassesOcclusion(
    const Depth candidateDepth,
    const Depth frontOccluderDepth,
    const bool ordinaryThroughSelection,
    const bool xrayEnabled,
    const bool repeatedClick,
    const Depth tolerance = Depth{}) noexcept
{
    return ordinaryThroughSelection
        || shouldTraverseOccludedCandidates(xrayEnabled, repeatedClick)
        || candidateDepth <= frontOccluderDepth + tolerance;
}

[[nodiscard]] constexpr std::size_t nextFrontToBackCandidateIndex(
    const std::size_t candidateCount,
    const std::optional<std::size_t> currentIndex,
    const bool repeatedClick,
    const bool skipSelectedFront = false) noexcept
{
    if (candidateCount == 0u)
    {
        return 0u;
    }
    if (repeatedClick && currentIndex && *currentIndex < candidateCount)
    {
        return (*currentIndex + 1u) % candidateCount;
    }
    return skipSelectedFront && candidateCount > 1u ? 1u : 0u;
}

} // namespace sed::render::picking
