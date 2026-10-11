#pragma once

#include "render/lighting/dynamic_light_utils.hpp"
#include "render/mesh/render_mesh_utils.hpp"
#include "render/picking/viewport_pick_cycle_utils.hpp"

#include <QMatrix4x4>
#include <QPointF>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace sed::render
{

/** The editor-facing light families. These are deliberately independent of
 * the runtime renderer's light pool: Baked lights are SED entities, Thing
 * lights are authored template/instance fields, IJIM Sector lights live
 * on their owning sector, and Architectural owners are a MotS editor
 * overlay. The Architectural family deliberately has no runtime emitter. */
enum class LightGizmoCategory : std::uint8_t
{
    Baked,
    Thing,
    Sector,
    Architectural,
};

/** Identifies the authored emitter represented by a gizmo. A Thing may own
 * both a regular `light` and an actor `lightintensity`/`lightoffset` headlight,
 * so the owner index alone is not sufficient for picking or transforms. */
enum class LightGizmoKind : std::uint8_t
{
    Baked,
    ThingRegular,
    ThingHead,
    SectorPoint,
};

struct LightGizmoEntry
{
    LightGizmoCategory category{ LightGizmoCategory::Baked };
    LightGizmoKind kind{ LightGizmoKind::Baked };
    std::int32_t ownerIndex{ -1 };
    QVector3D position{};
    QVector3D color{ 1.0f, 1.0f, 1.0f };
    float minRadius{};
    float maxRadius{};
    std::int32_t layer{};
};

inline constexpr float kThingGizmoStackSpacingDefault = 0.01f;

enum class ThingGizmoStackLane : std::uint8_t
{
    ThingType,
    ThingLight,
    ArchThing,
};

/** Actual per-Thing marker visibility and independent offsets for one stack. */
struct ThingGizmoStackLayout
{
    bool thingTypeVisible{};
    bool thingLightVisible{};
    bool archThingVisible{};
    float thingTypeOffset{};
    float thingLightOffset{};
    float archThingOffset{};
    float spacing{ kThingGizmoStackSpacingDefault };
};

[[nodiscard]] inline bool thingGizmoStackLaneVisible(
    const ThingGizmoStackLayout& layout,
    const ThingGizmoStackLane lane) noexcept
{
    switch (lane)
    {
        case ThingGizmoStackLane::ThingType:
            return layout.thingTypeVisible;
        case ThingGizmoStackLane::ThingLight:
            return layout.thingLightVisible;
        case ThingGizmoStackLane::ArchThing:
            return layout.archThingVisible;
    }
    return false;
}

[[nodiscard]] inline float thingGizmoStackLaneOffset(
    const ThingGizmoStackLayout& layout,
    const ThingGizmoStackLane lane) noexcept
{
    switch (lane)
    {
        case ThingGizmoStackLane::ThingType:
            return layout.thingTypeOffset;
        case ThingGizmoStackLane::ThingLight:
            return layout.thingLightOffset;
        case ThingGizmoStackLane::ArchThing:
            return layout.archThingOffset;
    }
    return 0.0f;
}

[[nodiscard]] inline bool lightGizmoCategoryVisible(
    const LightGizmoCategory category,
    const LightGizmoCategory activeCategory,
    const bool alwaysVisible = false,
    const bool activeCategoryEnabled = true) noexcept
{
    return alwaysVisible || (activeCategoryEnabled && category == activeCategory);
}

[[nodiscard]] inline bool finiteLightGizmoPosition(const QVector3D& position) noexcept
{
    return std::isfinite(position.x())
        && std::isfinite(position.y())
        && std::isfinite(position.z());
}

/**
 * Returns the target marker center above the world-space model top. Only visible
 * preceding lanes contribute their own size, offset, and shared spacing. The
 * target lane's own offset is applied to its marker only.
 */
[[nodiscard]] inline float thingGizmoStackLaneCenterAboveTop(
    const ThingGizmoStackLane lane,
    const float thingTypeHalfSize,
    const float thingLightHalfSize,
    const float archThingHalfSize,
    const ThingGizmoStackLayout& layout) noexcept
{
    if (!std::isfinite(thingTypeHalfSize)
        || !std::isfinite(thingLightHalfSize)
        || !std::isfinite(archThingHalfSize)
        || thingTypeHalfSize <= 0.0f
        || thingLightHalfSize <= 0.0f
        || archThingHalfSize <= 0.0f
        || !std::isfinite(layout.thingTypeOffset)
        || !std::isfinite(layout.thingLightOffset)
        || !std::isfinite(layout.archThingOffset)
        || !std::isfinite(layout.spacing)
        || layout.thingTypeOffset < 0.0f
        || layout.thingLightOffset < 0.0f
        || layout.archThingOffset < 0.0f
        || layout.spacing < 0.0f)
    {
        return 0.0f;
    }

    constexpr std::array lanes{
        ThingGizmoStackLane::ThingType,
        ThingGizmoStackLane::ThingLight,
        ThingGizmoStackLane::ArchThing,
    };
    float precedingTopAboveModel{};
    for (const ThingGizmoStackLane candidate : lanes)
    {
        const float halfSize = candidate == ThingGizmoStackLane::ThingType
            ? thingTypeHalfSize
            : candidate == ThingGizmoStackLane::ThingLight
                ? thingLightHalfSize
                : archThingHalfSize;
        const float ownOffset = thingGizmoStackLaneOffset(layout, candidate);
        if (candidate == lane)
        {
            return precedingTopAboveModel + ownOffset + halfSize;
        }
        if (thingGizmoStackLaneVisible(layout, candidate))
        {
            precedingTopAboveModel += 2.0f * halfSize + ownOffset + layout.spacing;
        }
    }
    return 0.0f;
}

/**
 * Places an owner marker at the rendered Thing's world-space top. Billboard
 * orientation remains camera-facing, while the marker center stays at the same
 * world position as the camera moves. A marker's own offset is independent;
 * visible preceding marker lanes alone contribute to its stack position.
 */
[[nodiscard]] inline QVector3D thingGizmoStackDisplayPosition(
    const QVector3D& ownerPosition,
    const ThingBounds* ownerBounds,
    const ThingGizmoStackLane lane,
    const float thingTypeHalfSize,
    const float thingLightHalfSize,
    const float archThingHalfSize,
    const ThingGizmoStackLayout& layout) noexcept
{
    if (!finiteLightGizmoPosition(ownerPosition)
        || !std::isfinite(thingTypeHalfSize)
        || !std::isfinite(thingLightHalfSize)
        || !std::isfinite(archThingHalfSize)
        || thingTypeHalfSize <= 0.0f
        || thingLightHalfSize <= 0.0f
        || archThingHalfSize <= 0.0f)
    {
        return ownerPosition;
    }

    float modelTop = ownerPosition.z();
    if (ownerBounds && ownerBounds->valid
        && finiteLightGizmoPosition(ownerBounds->min)
        && finiteLightGizmoPosition(ownerBounds->max)
        && ownerBounds->min.x() <= ownerBounds->max.x()
        && ownerBounds->min.y() <= ownerBounds->max.y()
        && ownerBounds->min.z() <= ownerBounds->max.z())
    {
        modelTop = std::max(modelTop, ownerBounds->max.z());
    }

    constexpr float clearance = 0.002f;
    const float centerAboveTop = thingGizmoStackLaneCenterAboveTop(
        lane,
        thingTypeHalfSize,
        thingLightHalfSize,
        archThingHalfSize,
        layout);
    if (!std::isfinite(centerAboveTop) || centerAboveTop <= 0.0f)
    {
        return ownerPosition;
    }
    return QVector3D(ownerPosition.x(), ownerPosition.y(), modelTop + clearance + centerAboveTop);
}

[[nodiscard]] inline QVector3D lightGizmoDisplayPosition(
    const LightGizmoEntry& entry,
    const float baseHalfSize,
    const ThingBounds* ownerBounds,
    const QVector3D* ownerPosition,
    const float thingTypeHalfSize,
    const float archThingHalfSize,
    const ThingGizmoStackLayout& layout) noexcept
{
    if (entry.kind != LightGizmoKind::ThingRegular)
    {
        return entry.position;
    }
    if (!std::isfinite(baseHalfSize) || baseHalfSize <= 0.0f
        || !std::isfinite(thingTypeHalfSize) || thingTypeHalfSize <= 0.0f
        || !std::isfinite(archThingHalfSize) || archThingHalfSize <= 0.0f)
    {
        return entry.position;
    }
    const QVector3D anchor = ownerPosition && finiteLightGizmoPosition(*ownerPosition)
        ? *ownerPosition
        : entry.position;
    // Reserve the selected marker's largest visible footprint so selection
    // cannot make it overlap a neighboring stack lane.
    const float lightStackHalfSize = std::max(baseHalfSize * 1.22f * 1.20f, 0.001f);
    return thingGizmoStackDisplayPosition(
        anchor,
        ownerBounds,
        ThingGizmoStackLane::ThingLight,
        thingTypeHalfSize,
        lightStackHalfSize,
        archThingHalfSize,
        layout);
}

/** Resolve the transform-handle pivot independently from icon presentation.
 *
 * A regular Thing light originates at the Thing position (`entry.position`),
 * so its Move handle must stay there even though the icon is lifted above the
 * rendered owner for readability. A Headlight also uses `entry.position`:
 * this is already the exact runtime owner position plus rotated authored
 * `lightoffset`, and its Move handle edits that local offset.
 */
[[nodiscard]] inline QVector3D thingLightTransformPivot(
    const LightGizmoEntry& entry,
    const QVector3D& displayPosition) noexcept
{
    if ((entry.kind == LightGizmoKind::ThingRegular
            || entry.kind == LightGizmoKind::ThingHead)
        && finiteLightGizmoPosition(entry.position))
    {
        return entry.position;
    }
    return displayPosition;
}

[[nodiscard]] inline std::vector<LightGizmoEntry> bakedLightGizmoEntries(const Level& level)
{
    std::vector<LightGizmoEntry> out{};
    out.reserve(level.lights.size());
    for (std::size_t i = 0; i < level.lights.size(); ++i)
    {
        const SedLight& light = level.lights[i];
        const QVector3D position(
            static_cast<float>(light.position.x),
            static_cast<float>(light.position.y),
            static_cast<float>(light.position.z));
        if (!finiteLightGizmoPosition(position))
        {
            continue;
        }

        out.push_back(LightGizmoEntry{
            LightGizmoCategory::Baked,
            LightGizmoKind::Baked,
            static_cast<std::int32_t>(i),
            position,
            QVector3D(
                static_cast<float>(light.color.r),
                static_cast<float>(light.color.g),
                static_cast<float>(light.color.b)),
            static_cast<float>(light.range),
            static_cast<float>(light.range),
            light.layer,
        });
    }
    return out;
}

inline void appendThingLightGizmoEntries(
    std::vector<LightGizmoEntry>& out,
    const Level& level,
    const assets::Templates* templates,
    const std::size_t thingIndex)
{
    if (thingIndex >= level.things.size())
    {
        return;
    }
    const Thing& thing = level.things[thingIndex];
    const std::int32_t index = static_cast<std::int32_t>(thingIndex);
    const auto append = [&out](
        const lighting::RuntimeThingLight& light,
        const LightGizmoKind kind,
        const std::int32_t layer)
    {
        if (light.classification == lighting::ThingLightClass::None
            || light.sourceThing < 0
            || !finiteLightGizmoPosition(light.position))
        {
            return;
        }
        float minRadius = light.classification == lighting::ThingLightClass::Flat
            ? 0.0f
            : light.minRadius;
        float maxRadius = light.classification == lighting::ThingLightClass::Flat
            ? 0.0f
            : light.maxRadius;
        if (kind == LightGizmoKind::ThingRegular
            || kind == LightGizmoKind::ThingHead)
        {
            // Thing-owned sources expose one authored range/intensity control,
            // even when the classic renderer derives separate world/model
            // attenuation radii from that scalar. Keep those radii in the
            // RuntimeThingLight payload, but present the source's maximum
            // effective reach as one editor range.
            const float outerRadius = std::max(minRadius, maxRadius);
            minRadius = outerRadius;
            maxRadius = outerRadius;
        }
        out.push_back(LightGizmoEntry{
            LightGizmoCategory::Thing,
            kind,
            light.sourceThing,
            light.position,
            light.color,
            minRadius,
            maxRadius,
            layer,
        });
    };

    append(
        lighting::resolveRuntimeThingLight(
            level,
            templates,
            thing,
            index,
            std::nullopt,
            lighting::ThingLightOwnerPolicy::AuthoringPreview),
        LightGizmoKind::ThingRegular,
        thing.layer);
    append(
        lighting::resolveRuntimeHeadLight(
            level,
            templates,
            thing,
            index,
            std::nullopt,
            lighting::ThingLightOwnerPolicy::AuthoringPreview),
        LightGizmoKind::ThingHead,
        thing.layer);
}

[[nodiscard]] inline std::vector<LightGizmoEntry> thingLightGizmoEntries(
    const Level& level,
    const assets::Templates* templates)
{
    std::vector<LightGizmoEntry> out{};
    out.reserve(level.things.size());

    for (std::size_t i = 0; i < level.things.size(); ++i)
    {
        appendThingLightGizmoEntries(out, level, templates, i);
    }
    return out;
}

[[nodiscard]] inline bool sectorPointLightHasEditorGizmo(const PointLight& light) noexcept
{
    const lighting::RuntimeSectorPointLight runtime =
        lighting::resolveRuntimeSectorPointLight(light);
    return finiteLightGizmoPosition(runtime.position)
        && std::isfinite(runtime.color.x())
        && std::isfinite(runtime.color.y())
        && std::isfinite(runtime.color.z())
        && std::isfinite(runtime.minRadius)
        && std::isfinite(runtime.maxRadius)
        && pointLightHasAuthoredParameters(light);
}

[[nodiscard]] inline std::vector<LightGizmoEntry> sectorLightGizmoEntries(const Level& level)
{
    std::vector<LightGizmoEntry> out{};
    if (level.kind != ProjectType::IJIM)
    {
        return out;
    }

    out.reserve(level.sectors.size());
    for (std::size_t i = 0; i < level.sectors.size(); ++i)
    {
        const Sector& sector = level.sectors[i];
        if (!sectorPointLightHasEditorGizmo(sector.pointLight))
        {
            continue;
        }
        const lighting::RuntimeSectorPointLight runtime =
            lighting::resolveRuntimeSectorPointLight(sector.pointLight);
        out.push_back(LightGizmoEntry{
            LightGizmoCategory::Sector,
            LightGizmoKind::SectorPoint,
            static_cast<std::int32_t>(i),
            runtime.position,
            runtime.color,
            static_cast<float>(sector.pointLight.minRange),
            static_cast<float>(sector.pointLight.maxRange),
            sector.layer,
        });
    }
    return out;
}

struct LightGizmoScreenPick
{
    std::size_t entryIndex{};
    std::int32_t ownerIndex{ -1 };
    LightGizmoKind kind{ LightGizmoKind::Baked };
    float depth{ std::numeric_limits<float>::max() };
    float distanceSquared{ std::numeric_limits<float>::max() };
};

struct LightGizmoIconFootprint
{
    float minU{ -1.0f };
    float maxU{ 1.0f };
    float minV{ -1.0f };
    float maxV{ 1.0f };
};

/** Visible artwork bounds in billboard-local coordinates. U points right and
 * V points up, with the texture quad spanning -1..1. These bounds let picking
 * cover the off-center actor/source artwork of the Headlight icon without
 * turning the entire transparent billboard into a hit target. */
[[nodiscard]] inline LightGizmoIconFootprint lightGizmoIconFootprint(
    const LightGizmoKind kind) noexcept
{
    switch (kind)
    {
        case LightGizmoKind::ThingRegular:
            return LightGizmoIconFootprint{ -0.82f, 0.82f, -0.82f, 0.95f };
        case LightGizmoKind::ThingHead:
            return LightGizmoIconFootprint{ -0.94f, 0.94f, -0.90f, 0.97f };
        case LightGizmoKind::SectorPoint:
            return LightGizmoIconFootprint{ -0.96f, 0.96f, -0.96f, 0.96f };
        case LightGizmoKind::Baked:
            return LightGizmoIconFootprint{ -0.88f, 0.88f, -0.88f, 0.88f };
    }
    return {};
}

/** Returns the hit selected by a repeated click at the same gizmo location.
 *
 * A normal click always takes the first depth/distance-sorted hit. A repeated
 * click advances from the exact authored emitter (owner plus kind), which is
 * important when one Thing owns both a regular light and a headlight. If the
 * current emitter is no longer among the visible hits, cycling restarts from
 * the first candidate.
 */
[[nodiscard]] inline std::optional<std::size_t> lightGizmoPickIndex(
    const std::vector<LightGizmoScreenPick>& hits,
    const bool repeatedClick,
    const std::int32_t currentOwner,
    const LightGizmoKind currentKind) noexcept
{
    if (hits.empty())
    {
        return std::nullopt;
    }
    if (!repeatedClick)
    {
        return 0u;
    }

    const auto current = std::find_if(
        hits.begin(),
        hits.end(),
        [currentOwner, currentKind](const LightGizmoScreenPick& candidate)
        {
            return candidate.ownerIndex == currentOwner
                && candidate.kind == currentKind;
        });
    if (current == hits.end())
    {
        return 0u;
    }
    return (static_cast<std::size_t>(std::distance(hits.begin(), current)) + 1u)
        % hits.size();
}

inline constexpr float kLightGizmoDepthTolerance = 0.0015f;

[[nodiscard]] inline bool lightGizmoDepthVisible(
    const float candidateDepth,
    const float sceneDepth,
    const float tolerance = kLightGizmoDepthTolerance) noexcept
{
    return std::isfinite(candidateDepth)
        && std::isfinite(sceneDepth)
        && candidateDepth <= sceneDepth + std::max(tolerance, 0.0f);
}

[[nodiscard]] inline bool lightGizmoPickDepthVisible(
    const float candidateDepth,
    const float sceneDepth) noexcept
{
    return picking::surfaceSelectionCandidatePassesSceneDepth(
        candidateDepth,
        sceneDepth,
        false,
        false);
}

/** Picks only the visible icon footprint for a light category. Ties prefer
 * the nearest projected light, then the closest icon center and stable owner
 * index. The caller supplies scene depth when direct picking must account for
 * world/Thing occlusion. */
[[nodiscard]] inline std::vector<LightGizmoScreenPick> pickLightGizmoEntries(
    const std::vector<LightGizmoEntry>& entries,
    const QMatrix4x4& clipFromWorld,
    const int viewportWidth,
    const int viewportHeight,
    const QPointF& pointer,
    const float radiusPx = 14.0f,
    const float maxVisibleDepth = 1.0f,
    QVector3D cameraRight = {},
    QVector3D cameraUp = {},
    const float baseHalfSize = 0.0f) noexcept
{
    if (viewportWidth <= 0 || viewportHeight <= 0 || !(radiusPx > 0.0f))
    {
        return {};
    }

    const float radiusSquared = radiusPx * radiusPx;
    std::vector<LightGizmoScreenPick> hits{};
    for (std::size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex)
    {
        const LightGizmoEntry& entry = entries[entryIndex];
        const QVector4D clip = clipFromWorld * QVector4D(entry.position, 1.0f);
        if (!std::isfinite(clip.w()) || clip.w() <= 0.0f)
        {
            continue;
        }
        const float invW = 1.0f / clip.w();
        const float ndcX = clip.x() * invW;
        const float ndcY = clip.y() * invW;
        const float ndcZ = clip.z() * invW;
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY) || !std::isfinite(ndcZ)
            || ndcZ < -1.0f || ndcZ > 1.0f)
        {
            continue;
        }

        const float screenX = (ndcX * 0.5f + 0.5f) * static_cast<float>(viewportWidth);
        const float screenY = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(viewportHeight);
        const float dx = screenX - static_cast<float>(pointer.x());
        const float dy = screenY - static_cast<float>(pointer.y());
        const float distanceSquared = dx * dx + dy * dy;
        bool footprintHit = false;
        if (std::isfinite(baseHalfSize) && baseHalfSize > 0.0f
            && cameraRight.lengthSquared() > 1.0e-12f
            && cameraUp.lengthSquared() > 1.0e-12f)
        {
            cameraRight.normalize();
            cameraUp.normalize();
            float drawnHalfSize = baseHalfSize;
            if (entry.category != LightGizmoCategory::Baked)
            {
                drawnHalfSize *= entry.category == LightGizmoCategory::Sector ? 1.42f : 1.22f;
            }
            const auto projectedDelta = [&](const QVector3D& axis) -> std::optional<QPointF>
            {
                const QVector4D edgeClip = clipFromWorld * QVector4D(
                    entry.position + axis * drawnHalfSize,
                    1.0f);
                if (!std::isfinite(edgeClip.w()) || edgeClip.w() <= 0.0f)
                {
                    return std::nullopt;
                }
                const float edgeInvW = 1.0f / edgeClip.w();
                const float edgeNdcX = edgeClip.x() * edgeInvW;
                const float edgeNdcY = edgeClip.y() * edgeInvW;
                if (!std::isfinite(edgeNdcX) || !std::isfinite(edgeNdcY))
                {
                    return std::nullopt;
                }
                return QPointF(
                    (edgeNdcX * 0.5f + 0.5f) * static_cast<float>(viewportWidth) - screenX,
                    (1.0f - (edgeNdcY * 0.5f + 0.5f)) * static_cast<float>(viewportHeight) - screenY);
            };
            const std::optional<QPointF> rightDelta = projectedDelta(cameraRight);
            const std::optional<QPointF> upDelta = projectedDelta(cameraUp);
            if (rightDelta && upDelta)
            {
                const float pointerDx = static_cast<float>(pointer.x()) - screenX;
                const float pointerDy = static_cast<float>(pointer.y()) - screenY;
                const float determinant = static_cast<float>(
                    rightDelta->x() * upDelta->y() - rightDelta->y() * upDelta->x());
                if (std::isfinite(determinant) && std::abs(determinant) > 1.0e-6f)
                {
                    const float u = static_cast<float>(
                        (pointerDx * upDelta->y() - pointerDy * upDelta->x()) / determinant);
                    const float v = static_cast<float>(
                        (rightDelta->x() * pointerDy - rightDelta->y() * pointerDx) / determinant);
                    const LightGizmoIconFootprint footprint = lightGizmoIconFootprint(entry.kind);
                    footprintHit = u >= footprint.minU && u <= footprint.maxU
                        && v >= footprint.minV && v <= footprint.maxV;
                }
            }
        }
        if (distanceSquared > radiusSquared && !footprintHit)
        {
            continue;
        }

        const LightGizmoScreenPick candidate{
            entryIndex,
            entry.ownerIndex,
            entry.kind,
            ndcZ * 0.5f + 0.5f,
            distanceSquared,
        };
        if ( !lightGizmoPickDepthVisible(candidate.depth, maxVisibleDepth) )
        {
            continue;
        }
        hits.push_back(candidate);
    }
    std::stable_sort(hits.begin(), hits.end(), [](const LightGizmoScreenPick& a, const LightGizmoScreenPick& b)
    {
        if (a.depth != b.depth) return a.depth < b.depth;
        if (a.distanceSquared != b.distanceSquared) return a.distanceSquared < b.distanceSquared;
        if (a.ownerIndex != b.ownerIndex) return a.ownerIndex < b.ownerIndex;
        return static_cast<std::uint8_t>(a.kind) < static_cast<std::uint8_t>(b.kind);
    });
    return hits;
}

[[nodiscard]] inline std::optional<LightGizmoScreenPick> pickLightGizmoEntry(
    const std::vector<LightGizmoEntry>& entries,
    const QMatrix4x4& clipFromWorld,
    const int viewportWidth,
    const int viewportHeight,
    const QPointF& pointer,
    const float radiusPx = 14.0f,
    const float maxVisibleDepth = 1.0f,
    QVector3D cameraRight = {},
    QVector3D cameraUp = {},
    const float baseHalfSize = 0.0f) noexcept
{
    const std::vector<LightGizmoScreenPick> hits = pickLightGizmoEntries(
        entries,
        clipFromWorld,
        viewportWidth,
        viewportHeight,
        pointer,
        radiusPx,
        maxVisibleDepth,
        cameraRight,
        cameraUp,
        baseHalfSize);
    return hits.empty() ? std::nullopt : std::optional<LightGizmoScreenPick>(hits.front());
}

} // namespace sed::render
