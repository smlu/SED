#pragma once

#include "render/gizmos/light_category_gizmo_utils.hpp"

#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <unordered_set>
#include <vector>

namespace sed::render
{

struct SelectedLightRange
{
    std::int32_t lightIndex{ -1 };
    bool primary{};
};

struct LightGizmoRangeRing
{
    LightGizmoEntry entry{};
    float radius{};
    bool primary{};
    bool dashed{};
    bool scalable{ true };
};

struct LightGizmoRangeScalability
{
    bool outer{ true };
    bool inner{ true };
};

/** Range geometry is a selection affordance, not emitted light. Keep it white
 * so the authored RGB remains reserved for the source icon itself. */
[[nodiscard]] inline QVector4D selectedLightRangeColor(const bool primary) noexcept
{
    return QVector4D(1.0f, 1.0f, 1.0f, primary ? 0.82f : 0.58f);
}

/** Maps IJIM's independently authored minimum/maximum fields to the visible
 * outer and inner rings. A zero field is an engine-derived sentinel; its
 * corresponding editor ring is omitted so changing RGB cannot make an
 * apparently authored range move. Runtime illumination still performs the
 * original rdCamera derivation.
 */
[[nodiscard]] inline LightGizmoRangeScalability sectorLightRangeScalability(
    const LightGizmoEntry& entry,
    const PointLight& authored) noexcept
{
    const bool minExplicit = authored.minRange != 0.0;
    const bool maxExplicit = authored.maxRange != 0.0;
    if (std::abs(entry.minRadius - entry.maxRadius) <= 1.0e-5f)
    {
        const bool scalable = minExplicit || maxExplicit;
        return LightGizmoRangeScalability{ scalable, scalable };
    }
    if (entry.minRadius < entry.maxRadius)
    {
        return LightGizmoRangeScalability{ maxExplicit, minExplicit };
    }
    return LightGizmoRangeScalability{ minExplicit, maxExplicit };
}

/** Builds the outer/inner range rings for one selected runtime emitter.
 *
 * The outer ring of the active emitter is solid. Independently authored inner
 * falloff rings and all secondary emitters are dashed. Thing-owned emitters
 * always expose one outer presentation ring because their single authored
 * range/intensity control cannot edit the runtime radii independently. For
 * Things, owner selection alone is not enough because a single owner may
 * expose both regular and head emitters.
 */
[[nodiscard]] inline std::vector<LightGizmoRangeRing> lightGizmoRangeRings(
    const LightGizmoEntry& entry,
    const bool ownerPrimary,
    const LightGizmoKind selectedKind,
    const bool outerScalable = true,
    const bool innerScalable = true)
{
    std::vector<LightGizmoRangeRing> result{};
    const float outer = std::max(entry.minRadius, entry.maxRadius);
    const float inner = std::min(entry.minRadius, entry.maxRadius);
    if (std::isfinite(outer) && outer > 0.0f)
    {
        const bool emitterPrimary = ownerPrimary
            && (entry.category != LightGizmoCategory::Thing
                || entry.kind == selectedKind);
        result.push_back(LightGizmoRangeRing{
            entry,
            outer,
            emitterPrimary,
            !emitterPrimary,
            outerScalable,
        });
    }
    if (entry.category != LightGizmoCategory::Thing
        && std::isfinite(inner) && inner > 0.0f
        && std::abs(inner - outer) > 1.0e-5f)
    {
        result.push_back(LightGizmoRangeRing{ entry, inner, false, true, innerScalable });
    }
    return result;
}

/**
 * Returns the visible range-gizmo order for a Light selection.
 *
 * The active/primary light is always first and solid. Remaining valid selected
 * lights are sorted for stable dashed rendering. As before, no active light
 * means no range visualization.
 */
[[nodiscard]] inline std::vector<SelectedLightRange> selectedLightRangeDrawOrder(
    const std::int32_t primaryLight,
    const std::unordered_set<std::int32_t>& selectedLights,
    const std::size_t lightCount)
{
    if (primaryLight < 0 || static_cast<std::size_t>(primaryLight) >= lightCount)
    {
        return {};
    }

    std::vector<SelectedLightRange> result{
        SelectedLightRange{ primaryLight, true },
    };

    std::vector<std::int32_t> secondary{};
    secondary.reserve(selectedLights.size());
    for (const std::int32_t lightIndex : selectedLights)
    {
        if (lightIndex < 0
            || lightIndex == primaryLight
            || static_cast<std::size_t>(lightIndex) >= lightCount)
        {
            continue;
        }
        secondary.push_back(lightIndex);
    }
    std::ranges::sort(secondary);

    result.reserve(1u + secondary.size());
    for (const std::int32_t lightIndex : secondary)
    {
        result.push_back(SelectedLightRange{ lightIndex, false });
    }
    return result;
}

/** Appends a camera-facing solid or alternating-segment dashed range ring. */
inline void appendLightRangeRingLineVertices(
    std::vector<float>& vertices,
    const QVector3D& center,
    const QVector3D& right,
    const QVector3D& up,
    const float radius,
    const int segmentCount,
    const bool dashed)
{
    if (!std::isfinite(radius) || radius <= 0.0f || segmentCount < 3)
    {
        return;
    }

    auto appendLine = [&vertices](const QVector3D& a, const QVector3D& b)
    {
        vertices.push_back(a.x());
        vertices.push_back(a.y());
        vertices.push_back(a.z());
        vertices.push_back(b.x());
        vertices.push_back(b.y());
        vertices.push_back(b.z());
    };

    for (int segment = 0; segment < segmentCount; ++segment)
    {
        if (dashed && (segment & 1) != 0)
        {
            continue;
        }

        const float a0 = (static_cast<float>(segment) / static_cast<float>(segmentCount))
            * 2.0f * std::numbers::pi_v<float>;
        const float a1 = (static_cast<float>(segment + 1) / static_cast<float>(segmentCount))
            * 2.0f * std::numbers::pi_v<float>;
        const QVector3D p0 = center
            + right * (std::cos(a0) * radius)
            + up * (std::sin(a0) * radius);
        const QVector3D p1 = center
            + right * (std::cos(a1) * radius)
            + up * (std::sin(a1) * radius);
        appendLine(p0, p1);
    }
}

} // namespace sed::render
