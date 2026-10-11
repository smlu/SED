#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace sed::render
{

/**
 * Depth state used by viewport wireframe overlays.
 *
 * Thick screen-space line quads can interpolate a different depth beside a
 * sloped surface edge. Level edges therefore follow their supporting
 * surface's projected depth plane on the inward half of the expanded quad,
 * bounded by the supplied reference side. The scene and overlay deliberately
 * use unbiased depth: opposing polygon offsets or an NDC pull can exceed the
 * real separation between sectors at grazing angles and expose edges through
 * a foreground wall. LEQUAL is sufficient for the bounded coplanar case.
 *
 * Geometry-shader line quads must also be double-sided. Their triangle-strip
 * winding changes when a line's endpoint order changes in screen space, so
 * inheriting GL_CULL_FACE makes otherwise identical edges disappear as the
 * camera moves.
 */
struct WireframeOverlayDepthPolicy
{
    bool depthTestScene{ true };
    bool matchSupportingSurfaceDepth{ true };
    bool cullScreenSpaceQuads{ false };
    bool polygonOffset{ false };
    float polygonOffsetFactor{ 0.0f };
    float polygonOffsetUnits{ 0.0f };
    float ndcBias{ 0.0f };
    float projectedPlaneMinSinSquared{ 1.0e-12f };
    float homogeneousClipMinimumW{ 1.0e-6f };
};

constexpr WireframeOverlayDepthPolicy wireframeOverlayDepthPolicy() noexcept
{
    return {};
}

/**
 * Selected edges are the final colored line overlay.  A very small NDC lift
 * keeps them deterministically above their own textured fill and the ordinary
 * wireframe without disabling scene-depth rejection.  The lift is deliberately
 * far below the old broad polygon offsets, so genuinely foreground geometry
 * remains authoritative.
 */
constexpr WireframeOverlayDepthPolicy selectionHighlightOverlayDepthPolicy() noexcept
{
    WireframeOverlayDepthPolicy policy = wireframeOverlayDepthPolicy();
    policy.ndcBias = 2.0e-6f;
    return policy;
}

/**
 * State contract for the selected level-geometry stencil mask.
 *
 * The mask is only an auxiliary halo clip.  The selected polygon edge list
 * remains the complete wireframe, including edges inside the selected fill,
 * and the mask is depth-tested so its visible pass cannot reveal hidden
 * geometry. The matching occluded pass uses GL_GREATER and a reduced alpha,
 * mirroring Thing outlines, so selected geometry remains discoverable through
 * foreground surfaces without changing ordinary wireframe occlusion. The
 * pass also snapshots/restores the context's stencil clear value because
 * `glClearStencil` is persistent GL state, not a one-shot clear argument.
 */
struct SelectionGeometryStencilPolicy
{
    bool depthTestScene{ true };
    bool preserveInteriorWireframe{ true };
    bool preserveStencilClearValue{ true };
    bool drawOccludedWireframe{ true };
    float occludedWireframeAlpha{ 0.55f };
    unsigned int stencilBit{ 0x01u };
};

constexpr SelectionGeometryStencilPolicy selectionGeometryStencilPolicy() noexcept
{
    return {};
}

/** Mirrors the line shaders' `clip.z -= bias * clip.w` operation. */
inline float biasedWireframeNdcDepth(
    const float ndcDepth,
    const float bias = wireframeOverlayDepthPolicy().ndcBias) noexcept
{
    if (!std::isfinite(ndcDepth) || !std::isfinite(bias))
    {
        return ndcDepth;
    }
    return std::clamp(ndcDepth - std::max(0.0f, bias), -1.0f, 1.0f);
}

enum class SelectionGeometryDepthPass : std::uint8_t
{
    Visible,
    Occluded,
};

/**
 * CPU policy model for the two selected-geometry line passes. The renderer
 * applies the equivalent GL_LEQUAL/GL_GREATER functions to every full-wire
 * range; keeping this model pure makes the pass ordering regression-testable
 * without requiring an OpenGL context.
 */
inline bool selectionGeometryWireframePassesSceneDepth(
    const float wireNdcDepth,
    const float sceneNdcDepth,
    const SelectionGeometryDepthPass pass,
    const WireframeOverlayDepthPolicy visiblePolicy = selectionHighlightOverlayDepthPolicy()) noexcept
{
    if (!visiblePolicy.depthTestScene)
    {
        return true;
    }
    if (!std::isfinite(wireNdcDepth) || !std::isfinite(sceneNdcDepth))
    {
        return false;
    }

    if (pass == SelectionGeometryDepthPass::Occluded)
    {
        return wireNdcDepth > sceneNdcDepth;
    }

    return biasedWireframeNdcDepth(wireNdcDepth, visiblePolicy.ndcBias) <= sceneNdcDepth;
}

/** One point after perspective division, in OpenGL NDC coordinates. */
struct WireframeNdcPoint
{
    float x{};
    float y{};
    float z{};
};

/** One homogeneous clip-space point before perspective division. */
struct WireframeClipPoint
{
    float x{};
    float y{};
    float z{};
    float w{};
};

struct WireframeClippedSegment
{
    bool visible{};
    bool clippedA{};
    bool clippedB{};
    WireframeClipPoint a{};
    WireframeClipPoint b{};
};

inline bool finiteWireframeClipPoint(const WireframeClipPoint point) noexcept
{
    return std::isfinite(point.x)
        && std::isfinite(point.y)
        && std::isfinite(point.z)
        && std::isfinite(point.w);
}

inline WireframeClipPoint interpolateWireframeClipPoint(
    const WireframeClipPoint a,
    const WireframeClipPoint b,
    const float t) noexcept
{
    return WireframeClipPoint{
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t,
    };
}

/**
 * Clip one line segment to all six OpenGL homogeneous frustum planes plus the
 * positive-w guard used by the thick-line geometry shader.
 *
 * Geometry shaders run before fixed-function clipping, so a screen-space line
 * expander must clip before dividing by `w`. The interval is calculated first
 * and only published after every plane accepts it, keeping rejection atomic.
 */
inline WireframeClippedSegment clipWireframeSegmentToHomogeneousFrustum(
    const WireframeClipPoint a,
    const WireframeClipPoint b,
    const float minimumW = wireframeOverlayDepthPolicy().homogeneousClipMinimumW) noexcept
{
    WireframeClippedSegment out{};
    out.a = a;
    out.b = b;
    if (!finiteWireframeClipPoint(a) || !finiteWireframeClipPoint(b))
    {
        return out;
    }

    const float safeMinimumW = std::max(0.0f, minimumW);
    const std::array<float, 7> distancesA{
        a.x + a.w, -a.x + a.w,
        a.y + a.w, -a.y + a.w,
        a.z + a.w, -a.z + a.w,
        a.w - safeMinimumW,
    };
    const std::array<float, 7> distancesB{
        b.x + b.w, -b.x + b.w,
        b.y + b.w, -b.y + b.w,
        b.z + b.w, -b.z + b.w,
        b.w - safeMinimumW,
    };

    float firstT = 0.0f;
    float secondT = 1.0f;
    for (std::size_t plane = 0; plane < distancesA.size(); ++plane)
    {
        const float distanceA = distancesA[plane];
        const float distanceB = distancesB[plane];
        if (distanceA < 0.0f && distanceB < 0.0f)
        {
            return out;
        }
        if ((distanceA < 0.0f) == (distanceB < 0.0f))
        {
            continue;
        }
        const float denominator = distanceA - distanceB;
        if (!std::isfinite(denominator) || std::abs(denominator) <= 1.0e-20f)
        {
            return out;
        }
        const float t = std::clamp(distanceA / denominator, 0.0f, 1.0f);
        if (distanceA < 0.0f)
        {
            firstT = std::max(firstT, t);
        }
        else
        {
            secondT = std::min(secondT, t);
        }
        if (firstT > secondT)
        {
            return out;
        }
    }

    out.a = interpolateWireframeClipPoint(a, b, firstT);
    out.b = interpolateWireframeClipPoint(a, b, secondT);
    out.clippedA = firstT > 0.0f;
    out.clippedB = secondT < 1.0f;
    out.visible = out.a.w >= safeMinimumW && out.b.w >= safeMinimumW;
    return out;
}

/** Screen-space depth gradient for the surface supporting one wire edge. */
struct WireframeNdcDepthGradient
{
    bool valid{};
    float dzDx{};
    float dzDy{};
    float edgeX{};
    float edgeY{};
    float referenceSideCross{};
};

/**
 * Lateral boundaries for a thick wire quad's depth-matched bands, measured
 * from the projected edge along its screen-space normal. The center boundary
 * keeps support-plane correction off the outward half. A second interior
 * boundary is needed when the supporting reference falls inside the quad.
 */
struct WireframeLateralDepthBandPlan
{
    std::array<float, 4> offsetsPixels{};
    std::size_t boundaryCount{};
    bool supportMatched{};
};

/**
 * Plan the cross-section strips required to linearly rasterize the bounded
 * supporting-plane depth function. The half-width offset is the shader's
 * positive-normal offset at the quad edge. The sign of its screen-space cross
 * product identifies which side contains the supporting polygon.
 *
 * The returned offsets are ordered from the negative quad side to the
 * positive side. A valid support always inserts the centerline; a reference
 * closer than the half-width also inserts the exact clamp boundary. Invalid
 * support falls back to the original single, unbiased quad.
 */
inline WireframeLateralDepthBandPlan wireframeLateralDepthBandPlan(
    const float halfWidthPixels,
    const float halfWidthOffsetNdcX,
    const float halfWidthOffsetNdcY,
    const WireframeNdcDepthGradient gradient) noexcept
{
    WireframeLateralDepthBandPlan plan{};
    if (!std::isfinite(halfWidthPixels) || !(halfWidthPixels > 0.0f))
    {
        return plan;
    }

    plan.offsetsPixels[0] = -halfWidthPixels;
    plan.offsetsPixels[1] = halfWidthPixels;
    plan.boundaryCount = 2;
    if (!gradient.valid
        || !std::isfinite(halfWidthOffsetNdcX)
        || !std::isfinite(halfWidthOffsetNdcY)
        || !std::isfinite(gradient.edgeX)
        || !std::isfinite(gradient.edgeY)
        || !std::isfinite(gradient.referenceSideCross)
        || std::abs(gradient.referenceSideCross) <= 1.0e-20f)
    {
        return plan;
    }

    const float sideCross =
        gradient.edgeX * halfWidthOffsetNdcY
        - gradient.edgeY * halfWidthOffsetNdcX;
    const float referenceRatio = sideCross / gradient.referenceSideCross;
    if (!std::isfinite(referenceRatio) || std::abs(referenceRatio) <= 1.0e-20f)
    {
        return plan;
    }

    plan.supportMatched = true;
    plan.offsetsPixels[1] = 0.0f;
    plan.offsetsPixels[2] = halfWidthPixels;
    plan.boundaryCount = 3;

    const float supportBoundaryPixels =
        halfWidthPixels / std::max(1.0f, std::abs(referenceRatio));
    if (!(supportBoundaryPixels > 0.0f)
        || !(supportBoundaryPixels < halfWidthPixels))
    {
        return plan;
    }

    if (referenceRatio > 0.0f)
    {
        plan.offsetsPixels[2] = supportBoundaryPixels;
        plan.offsetsPixels[3] = halfWidthPixels;
    }
    else
    {
        plan.offsetsPixels[1] = -supportBoundaryPixels;
        plan.offsetsPixels[2] = 0.0f;
        plan.offsetsPixels[3] = halfWidthPixels;
    }
    plan.boundaryCount = 4;
    return plan;
}

/**
 * Reconstruct the supporting polygon's NDC depth plane from an edge and one
 * non-collinear point on that polygon.
 *
 * A thick line is a screen-space quad. Reusing the centre edge depth on both
 * sides of that quad is incorrect on a sloped face: fragments expanded into
 * the face can wind up behind the filled polygon. Matching the polygon's
 * projected depth gradient keeps inward covered wire fragments coplanar. The
 * caller bounds its use to avoid extrapolating beyond known supporting
 * geometry.
 */
inline WireframeNdcDepthGradient wireframeSupportingSurfaceDepthGradient(
    const WireframeNdcPoint edgeA,
    const WireframeNdcPoint edgeB,
    const WireframeNdcPoint surfacePoint,
    const float minSinSquared = wireframeOverlayDepthPolicy().projectedPlaneMinSinSquared) noexcept
{
    const float edgeX = edgeB.x - edgeA.x;
    const float edgeY = edgeB.y - edgeA.y;
    const float refX = surfacePoint.x - edgeA.x;
    const float refY = surfacePoint.y - edgeA.y;
    const float edgeZ = edgeB.z - edgeA.z;
    const float refZ = surfacePoint.z - edgeA.z;

    const float det = edgeX * refY - edgeY * refX;
    const float edgeLengthSquared = edgeX * edgeX + edgeY * edgeY;
    const float refLengthSquared = refX * refX + refY * refY;
    const float determinantScale = edgeLengthSquared * refLengthSquared;
    const float safeMinSinSquared = std::max(0.0f, minSinSquared);

    if (!std::isfinite(det)
        || !std::isfinite(determinantScale)
        || determinantScale <= 1.0e-20f
        || (det * det) <= determinantScale * safeMinSinSquared)
    {
        return {};
    }

    const float invDet = 1.0f / det;
    const float dzDx = (edgeZ * refY - refZ * edgeY) * invDet;
    const float dzDy = (edgeX * refZ - refX * edgeZ) * invDet;
    if (!std::isfinite(dzDx) || !std::isfinite(dzDy))
    {
        return {};
    }

    return WireframeNdcDepthGradient{ true, dzDx, dzDy, edgeX, edgeY, det };
}

/**
 * CPU mirror of the geometry shader's clip-space validation and perspective
 * divide for a supporting-plane reference. The reference is used only to
 * reconstruct the plane; it is not itself rasterized, so keeping it outside
 * the near clip plane is valid when it still has finite positive W.
 */
inline WireframeNdcDepthGradient wireframeSupportingSurfaceDepthGradientFromClip(
    const WireframeClipPoint edgeA,
    const WireframeClipPoint edgeB,
    const WireframeClipPoint surfacePoint,
    const float minimumW = wireframeOverlayDepthPolicy().homogeneousClipMinimumW,
    const float minSinSquared = wireframeOverlayDepthPolicy().projectedPlaneMinSinSquared) noexcept
{
    if (!finiteWireframeClipPoint(edgeA)
        || !finiteWireframeClipPoint(edgeB)
        || !finiteWireframeClipPoint(surfacePoint)
        || edgeA.w <= minimumW
        || edgeB.w <= minimumW
        || surfacePoint.w <= minimumW)
    {
        return {};
    }

    const WireframeNdcPoint ndcA{ edgeA.x / edgeA.w, edgeA.y / edgeA.w, edgeA.z / edgeA.w };
    const WireframeNdcPoint ndcB{ edgeB.x / edgeB.w, edgeB.y / edgeB.w, edgeB.z / edgeB.w };
    const WireframeNdcPoint ndcSurface{
        surfacePoint.x / surfacePoint.w,
        surfacePoint.y / surfacePoint.w,
        surfacePoint.z / surfacePoint.w };
    return wireframeSupportingSurfaceDepthGradient(ndcA, ndcB, ndcSurface, minSinSquared);
}

/**
 * Supporting-depth correction for one expanded line-quad vertex.
 *
 * Only the side pointing into the supporting polygon is matched. If the quad
 * is wider than the projected distance to the reference point, the offset is
 * shortened to that reference-side boundary instead of extrapolating the
 * plane indefinitely. The cross-product ratio is invariant under viewport
 * aspect scaling and bounds grazing/crease cases without introducing a
 * toward-camera bias on the outside of the edge.
 */
inline float boundedWireframeSupportingDepthCorrection(
    const float offsetNdcX,
    const float offsetNdcY,
    const WireframeNdcDepthGradient gradient) noexcept
{
    if (!gradient.valid
        || !std::isfinite(offsetNdcX) || !std::isfinite(offsetNdcY)
        || !std::isfinite(gradient.referenceSideCross)
        || std::abs(gradient.referenceSideCross) <= 1.0e-20f)
    {
        return 0.0f;
    }

    const float offsetSideCross =
        gradient.edgeX * offsetNdcY - gradient.edgeY * offsetNdcX;
    const float referenceRatio = offsetSideCross / gradient.referenceSideCross;
    if (!std::isfinite(referenceRatio) || !(referenceRatio > 0.0f))
    {
        return 0.0f;
    }

    const float scale = referenceRatio > 1.0f ? (1.0f / referenceRatio) : 1.0f;
    const float correction = scale
        * (gradient.dzDx * offsetNdcX + gradient.dzDy * offsetNdcY);
    return std::isfinite(correction) ? correction : 0.0f;
}

/** Mirrors the thick-line geometry shader's surface-depth match and lift. */
inline float surfaceMatchedWireframeNdcDepth(
    const float edgeNdcDepth,
    const float offsetNdcX,
    const float offsetNdcY,
    const WireframeNdcDepthGradient gradient,
    const WireframeOverlayDepthPolicy policy = wireframeOverlayDepthPolicy()) noexcept
{
    float matchedDepth = edgeNdcDepth;
    if (policy.matchSupportingSurfaceDepth && gradient.valid
        && std::isfinite(offsetNdcX) && std::isfinite(offsetNdcY))
    {
        matchedDepth += boundedWireframeSupportingDepthCorrection(
            offsetNdcX,
            offsetNdcY,
            gradient);
    }
    return biasedWireframeNdcDepth(matchedDepth, policy.ndcBias);
}

/** Focused policy helper: LEQUAL still rejects geometry farther back than the tiny lift. */
inline bool wireframeOverlayPassesSceneDepth(
    const float wireNdcDepth,
    const float sceneNdcDepth,
    const WireframeOverlayDepthPolicy policy = wireframeOverlayDepthPolicy()) noexcept
{
    if (!policy.depthTestScene)
    {
        return true;
    }
    if (!std::isfinite(wireNdcDepth) || !std::isfinite(sceneNdcDepth))
    {
        return false;
    }
    return biasedWireframeNdcDepth(wireNdcDepth, policy.ndcBias) <= sceneNdcDepth;
}

/** Focused depth-test model for a thick-line fragment expanded off its edge. */
inline bool wireframeOverlayPassesSceneDepthAtOffset(
    const float edgeNdcDepth,
    const float offsetNdcX,
    const float offsetNdcY,
    const WireframeNdcDepthGradient gradient,
    const float sceneNdcDepth,
    const WireframeOverlayDepthPolicy policy = wireframeOverlayDepthPolicy()) noexcept
{
    if (!policy.depthTestScene)
    {
        return true;
    }
    if (!std::isfinite(edgeNdcDepth) || !std::isfinite(sceneNdcDepth))
    {
        return false;
    }
    return surfaceMatchedWireframeNdcDepth(
               edgeNdcDepth,
               offsetNdcX,
               offsetNdcY,
               gradient,
               policy)
        <= sceneNdcDepth;
}

} // namespace sed::render
