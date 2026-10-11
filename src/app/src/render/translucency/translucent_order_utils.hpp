#pragma once

#include <cmath>
#include <limits>

namespace sed::render::translucency
{

enum class PrimitiveKind
{
    level,
    thing,
    sprite,
};

enum class ProjectionKind
{
    perspective,
    orthographic,
};

/**
 * Returns the projection-appropriate scalar used for back-to-front ordering.
 * Perspective rays diverge from the eye, so radial squared distance remains a
 * useful proxy. Orthographic rays are parallel, so only signed distance along
 * the normalized camera-forward axis is meaningful; lateral displacement must
 * not change the order.
 */
inline float projectionOrderDepth(
    const float relativeX,
    const float relativeY,
    const float relativeZ,
    const float cameraForwardX,
    const float cameraForwardY,
    const float cameraForwardZ,
    const ProjectionKind projection) noexcept
{
    const float depth = projection == ProjectionKind::orthographic
        ? (relativeX * cameraForwardX)
            + (relativeY * cameraForwardY)
            + (relativeZ * cameraForwardZ)
        : (relativeX * relativeX)
            + (relativeY * relativeY)
            + (relativeZ * relativeZ);
    return std::isfinite(depth)
        ? depth
        : -std::numeric_limits<float>::infinity();
}

/**
 * Stable key used to merge every scene translucency source into one
 * back-to-front order. The depth is either squared eye distance for perspective
 * projection or signed camera-forward depth for orthographic projection.
 */
struct OrderKey
{
    float depth{};
    PrimitiveKind kind{ PrimitiveKind::level };
    int stableKey{};
    int firstVertex{};
};

inline float finiteDepthOrBack(const float depth) noexcept
{
    return std::isfinite(depth)
        ? depth
        : -std::numeric_limits<float>::infinity();
}

/** Strict weak ordering for std::sort: far-to-near with deterministic ties. */
inline bool backToFrontLess(const OrderKey& a, const OrderKey& b) noexcept
{
    const float aDepth = finiteDepthOrBack(a.depth);
    const float bDepth = finiteDepthOrBack(b.depth);
    if (aDepth != bDepth)
    {
        return aDepth > bDepth;
    }

    const bool aSprite = a.kind == PrimitiveKind::sprite;
    const bool bSprite = b.kind == PrimitiveKind::sprite;
    if (aSprite != bSprite)
    {
        // At the same depth, sprite effects behave most predictably when
        // blended after ordinary level/Thing translucency.
        return !aSprite;
    }
    if (a.kind != b.kind)
    {
        return static_cast<int>(a.kind) < static_cast<int>(b.kind);
    }
    if (a.stableKey != b.stableKey)
    {
        return a.stableKey < b.stableKey;
    }
    return a.firstVertex < b.firstVertex;
}

/**
 * Mirrors the alpha-depth prepass policy.  Near-opaque depth may only be
 * written when the final material-modulated alpha is near opaque; using the
 * texture alpha alone lets a translucent Thing occlude content behind it.
 */
inline bool passesAlphaDepthPrepass(
    const float textureAlpha,
    const float materialAlpha,
    const float alphaReference = 0.95f) noexcept
{
    return std::isfinite(textureAlpha)
        && std::isfinite(materialAlpha)
        && std::isfinite(alphaReference)
        && (textureAlpha * materialAlpha) >= alphaReference;
}

/** A face that explicitly disables z-write must never enter a depth prepass. */
inline bool allowsAlphaDepthPrepassForFace(const bool zWriteDisabled) noexcept
{
    return !zWriteDisabled;
}

/**
 * Returns whether one filled level surface belongs in the sorted translucent
 * queue. A solid/textured adjoin can still be translucent: geometry mode and
 * portal visibility do not replace the Face translucency/no-z-write flags.
 */
inline bool shouldSeparateLevelSurfaceForTranslucency(
    const bool adjoin,
    const bool editorPreview,
    const bool projectedSky,
    const bool translucent,
    const bool zWriteDisabled) noexcept
{
    return !projectedSky
        && (adjoin || editorPreview)
        && (translucent || zWriteDisabled);
}

/**
 * Mirrors the classic alpha-adjoin queue admission. Explicit translucent or
 * no-z-write faces always use the sorted pass; texture alpha additionally
 * moves an adjoin there while an ordinary cutout wall stays in the solid pass.
 */
inline bool levelSurfaceUsesTranslucentPass(
    const bool adjoin,
    const bool translucent,
    const bool zWriteDisabled,
    const bool textureHasAlpha) noexcept
{
    return translucent
        || zWriteDisabled
        || (adjoin && textureHasAlpha);
}

/**
 * Batch-level admission for the pragmatic near-opaque depth prepass. Level
 * non-adjoin walls and ordinary Thing meshes may participate, but adjoins,
 * projected sky, sprites, textures without alpha, and authored no-z-write
 * faces may not.
 */
inline bool allowsAlphaDepthPrepassForBatch(
    const PrimitiveKind kind,
    const bool translucent,
    const bool hasTextureAlpha,
    const bool zWriteDisabled,
    const bool projectedSky,
    const bool adjoin) noexcept
{
    return (kind == PrimitiveKind::level || kind == PrimitiveKind::thing)
        && translucent
        && hasTextureAlpha
        && allowsAlphaDepthPrepassForFace(zWriteDisabled)
        && !(kind == PrimitiveKind::level && (projectedSky || adjoin));
}

} // namespace sed::render::translucency
