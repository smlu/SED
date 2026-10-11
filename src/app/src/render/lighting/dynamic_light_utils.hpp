#pragma once

#include "sed/assets/tpl.hpp"
#include "sed/level/thing.hpp"
#include "sed/text.hpp"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace sed::render::lighting
{

inline constexpr float kIjimFlatLightMaxRange = 0.01f;
inline constexpr std::size_t kIjimThingLightCap = 64;
inline constexpr std::size_t kClassicThingLightCap = 32;

/**
 * Runtime dynamic-light illumination is enabled in the editor.
 *
 * SED deliberately uses a bounded, view-stable collector instead of trying to
 * reproduce the games' portal/PVS traversal. Collection is an editor concern;
 * the contribution, attenuation and per-project shading equations below mirror
 * the original renderers.
 */
inline constexpr bool kRuntimeDynamicLightPreviewEnabled = true;

// Both original runtime camera paths replace rdCamera's construction defaults
// with 0.4/0.8 before collecting lights. AddLight derives the outer/minimum
// and inner/maximum radii from those installed values.
inline constexpr float kIjimAttenuationMax = 0.8f;
inline constexpr float kIjimAttenuationMin = 0.4f;
inline constexpr float kClassicAttenuationMin = 0.4f;
inline constexpr float kClassicAttenuationMax = 0.8f;
inline constexpr std::int32_t kActorHeadlightFlag = 0x4;

enum class ThingLightClass : std::uint8_t
{
    None,
    Flat,
    Dynamic,
};

enum class RuntimeThingLightSource : std::uint8_t
{
    Regular,
    Headlight,
};

/** Whether authored Thing visibility flags should gate light resolution.
 *
 * RuntimeParity mirrors the games, which skip disabled and invisible owners.
 * AuthoringPreview is used by SED's viewport because those Things remain drawn
 * and editable there (notably IJIM cutscene actors). It changes owner-state
 * visibility only; type, Headlight flag, color, sector, and range rules remain
 * engine-equivalent.
 */
enum class ThingLightOwnerPolicy : std::uint8_t
{
    RuntimeParity,
    AuthoringPreview,
};

/** Camera-frustum policy for runtime Thing-light collection.
 *
 * Gouraud light is evaluated at the original world/model vertices before
 * raster clipping. A source whose influence sphere is outside the camera
 * frustum can therefore still light an off-screen vertex whose color is
 * interpolated across a visible clipped primitive. ViewStable preserves those
 * sources, matching the engines' visible-sector collection behavior and
 * preventing camera-frustum light popping. InfluenceFrustum is safe only for
 * a pass whose contribution is evaluated entirely per fragment. If eligible
 * sources exceed the engine light cap, the editor's nearest-light membership
 * can still change as the camera translates.
 */
enum class ThingLightCollectionPolicy : std::uint8_t
{
    ViewStable,
    InfluenceFrustum,
};

/** Effective rdModel3 light mode after applying face, mesh, and camera caps. */
inline std::int32_t effectiveModelLightMode(
    const std::int32_t faceLightMode,
    const std::int32_t meshLightMode,
    const TLightMode cameraLightMode) noexcept
{
    return std::min(
        std::min(faceLightMode, meshLightMode),
        static_cast<std::int32_t>(cameraLightMode));
}

/** Shader payload selected from the effective rdModel3 light mode. */
inline float modelDynamicLightTarget(const std::int32_t effectiveLightMode) noexcept
{
    if (effectiveLightMode == static_cast<std::int32_t>(TLightMode::Gouraud))
    {
        return 2.0f;
    }
    if (effectiveLightMode == static_cast<std::int32_t>(TLightMode::Diffuse))
    {
        return 3.0f;
    }
    return 0.0f;
}

struct RuntimeThingLight
{
    ThingLightClass classification{ ThingLightClass::None };
    RuntimeThingLightSource source{ RuntimeThingLightSource::Regular };
    QVector3D position{};
    QVector3D color{};
    float minRadius{};
    float maxRadius{};
    std::int32_t sourceThing{ -1 };
};

struct RuntimeSectorPointLight
{
    QVector3D position{};
    QVector3D color{};
    float minRadius{};
    float maxRadius{};
};

/** CPU mirror of the OpenGL 3.3 std140 dynamic-light uniform block. */
struct alignas(16) DynamicLightGpuVec4
{
    float x{};
    float y{};
    float z{};
    float w{};
};

struct alignas(16) DynamicLightGpuBlock
{
    std::array<DynamicLightGpuVec4, kIjimThingLightCap> positionsAndMinRadii{};
    std::array<DynamicLightGpuVec4, kIjimThingLightCap> colorsAndMaxRadii{};
};

inline constexpr std::size_t kDynamicLightGpuBlockBytes = 2048;
inline constexpr std::uint32_t kDynamicLightUniformBlockBinding = 3;

constexpr bool dynamicLightGpuBlockHasStd140Layout() noexcept
{
    return std::is_standard_layout_v<DynamicLightGpuVec4>
        && std::is_standard_layout_v<DynamicLightGpuBlock>
        && alignof(DynamicLightGpuVec4) == 16
        && sizeof(DynamicLightGpuVec4) == 16
        && alignof(DynamicLightGpuBlock) == 16
        && offsetof(DynamicLightGpuBlock, positionsAndMinRadii) == 0
        && offsetof(DynamicLightGpuBlock, colorsAndMaxRadii) == 1024
        && sizeof(DynamicLightGpuBlock) == kDynamicLightGpuBlockBytes;
}

static_assert(dynamicLightGpuBlockHasStd140Layout());

inline DynamicLightGpuBlock packDynamicLightGpuBlock(
    const std::span<const RuntimeThingLight> lights) noexcept
{
    DynamicLightGpuBlock block{};
    const std::size_t count = std::min(lights.size(), kIjimThingLightCap);
    for (std::size_t i = 0; i < count; ++i)
    {
        const RuntimeThingLight& light = lights[i];
        block.positionsAndMinRadii[i] = DynamicLightGpuVec4{
            light.position.x(), light.position.y(), light.position.z(), light.minRadius };
        block.colorsAndMaxRadii[i] = DynamicLightGpuVec4{
            light.color.x(), light.color.y(), light.color.z(), light.maxRadius };
    }
    return block;
}

inline bool finitePositive(const float value) noexcept
{
    return std::isfinite(value) && value > 0.0f;
}

inline bool hasVisibleColor(const QVector3D& color) noexcept
{
    return std::isfinite(color.x()) && std::isfinite(color.y()) && std::isfinite(color.z())
        && (color.x() != 0.0f || color.y() != 0.0f || color.z() != 0.0f);
}

inline bool validThingSector(
    const Level& level,
    const Thing& thing,
    const std::optional<std::int32_t> sectorOverride = std::nullopt) noexcept
{
    const std::int32_t sector = sectorOverride.value_or(thing.sector);
    return sector >= 0 && static_cast<std::size_t>(sector) < level.sectors.size();
}

inline bool thingLightOwnerAllowed(
    const std::int32_t flags,
    const ThingLightOwnerPolicy policy) noexcept
{
    return policy == ThingLightOwnerPolicy::AuthoringPreview
        || (flags & (TF_DISABLED | TF_INVISIBLE)) == 0;
}

/** Resolve IJIM sector-light radii exactly as rdCamera_AddLight does.
 *
 * Sector files normally author both radii. For either radius left at exactly
 * zero, the runtime substitutes average RGB divided by the installed camera
 * attenuation. Keeping the substitutions independent also matches the engine
 * when only one authored radius is zero.
 */
inline RuntimeSectorPointLight resolveRuntimeSectorPointLight(
    const PointLight& light) noexcept
{
    RuntimeSectorPointLight out{};
    out.position = QVector3D(
        static_cast<float>(light.position.x),
        static_cast<float>(light.position.y),
        static_cast<float>(light.position.z));
    out.color = QVector3D(light.color.r, light.color.g, light.color.b);
    out.minRadius = static_cast<float>(light.minRange);
    out.maxRadius = static_cast<float>(light.maxRange);

    const float intensity = (out.color.x() + out.color.y() + out.color.z()) / 3.0f;
    if (out.minRadius == 0.0f)
    {
        out.minRadius = intensity / kIjimAttenuationMin;
    }
    if (out.maxRadius == 0.0f)
    {
        out.maxRadius = intensity / kIjimAttenuationMax;
    }
    return out;
}

/** Original IJIM sector-light participation in rdModel3 lighting.
 *
 * sithRender initializes Thing-light slots with rdLight_NewEntry, but leaves
 * its static sector-light slot zero-initialized and copies only color/radii.
 * rdLight_CalcVertexIntensities ignores bEnabled, so Gouraud sees the sector
 * light; rdLight_CalcFaceIntensity checks bEnabled, so Diffuse does not.
 */
inline constexpr bool ijimSectorPointLightContributesToModel(
    const bool diffuseFace) noexcept
{
    return !diffuseFace;
}

struct IjimAuthoredLight
{
    QVector3D color{};
    float range{};
    bool valid{};
};

/** Parse IJIM's authored `light=(r/g/b/range)` without ColorF normalization.
 *
 * OpenJones stores all four values verbatim: RGB may exceed 1, and a missing
 * fourth component leaves range at zero. The general-purpose Thing color
 * helper clamps both RGB and alpha and defaults alpha to one, so it cannot be
 * used for runtime-light parity.
 */
inline IjimAuthoredLight parseIjimAuthoredLight(const TplValue& value) noexcept
{
    std::string normalized{};
    normalized.reserve(value.stringValue.size());
    for (const char ch : value.stringValue)
    {
        normalized.push_back(
            (ch == '/' || ch == ',' || ch == '(' || ch == ')' || ch == ':') ? ' ' : ch);
    }

    const auto parts = text::splitWords(normalized);
    if (parts.size() < 3)
    {
        return {};
    }

    IjimAuthoredLight out{};
    out.color.setX(static_cast<float>(text::parseDouble(parts[0]).value_or(0.0)));
    out.color.setY(static_cast<float>(text::parseDouble(parts[1]).value_or(0.0)));
    out.color.setZ(static_cast<float>(text::parseDouble(parts[2]).value_or(0.0)));
    out.range = (parts.size() >= 4)
        ? static_cast<float>(text::parseDouble(parts[3]).value_or(0.0))
        : 0.0f;
    out.valid = std::isfinite(out.color.x())
        && std::isfinite(out.color.y())
        && std::isfinite(out.color.z())
        && std::isfinite(out.range);
    return out;
}

inline std::int32_t integralThingValue(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const std::string_view name) noexcept
{
    const TplValue* value = level::thingFindValue(level, templates, thing, name);
    if (!value)
    {
        return 0;
    }
    if (value->type == ValueType::integer || value->type == ValueType::flag || value->type == ValueType::ptr)
    {
        return value->intValue;
    }
    const std::string_view authored = text::trimView(value->stringValue);
    if (const auto parsed = text::parseInt<std::int32_t>(authored))
    {
        return *parsed;
    }
    return static_cast<std::int32_t>(text::parseHex(authored).value_or(0u));
}

inline bool isActorOrPlayer(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing) noexcept
{
    const TplValue* value = level::thingFindValue(level, templates, thing, "type");
    if (!value)
    {
        return false;
    }
    const std::string authored = value->stringValue.empty() ? value->asString() : value->stringValue;
    const std::string_view type = text::trimView(authored);
    return text::iequalsAscii(type, "actor") || text::iequalsAscii(type, "player");
}

inline float scalarThingLightValue(const TplValue& value) noexcept
{
    if (value.type == ValueType::floating)
    {
        return static_cast<float>(value.floatValue);
    }
    if (value.type == ValueType::integer || value.type == ValueType::ptr || value.type == ValueType::flag)
    {
        return static_cast<float>(value.intValue);
    }
    return static_cast<float>(
        text::parseDouble(text::trimView(value.stringValue)).value_or(0.0));
}

/**
 * Resolves the original runtime `light` field for an editor Thing.
 *
 * The parser itself marks a Thing as an emitter when this field exists, so an
 * authored light must not be rejected merely because the stored thingflags do
 * not contain that parser-side bit. RuntimeParity excludes disabled and
 * invisible Things; AuthoringPreview keeps those editor-visible owners
 * inspectable. Destroyed/runtime-removal state is not represented by SED.
 */
inline RuntimeThingLight resolveRuntimeThingLight(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const std::int32_t thingIndex,
    const std::optional<std::int32_t> sectorOverride = std::nullopt,
    const ThingLightOwnerPolicy ownerPolicy = ThingLightOwnerPolicy::RuntimeParity) noexcept
{
    RuntimeThingLight out{};
    out.source = RuntimeThingLightSource::Regular;
    out.sourceThing = thingIndex;
    out.position = QVector3D(
        static_cast<float>(thing.x),
        static_cast<float>(thing.y),
        static_cast<float>(thing.z));

    if (!validThingSector(level, thing, sectorOverride)
        || !std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
    {
        return out;
    }

    const std::int32_t flags = level::thingFlags(level, templates, thing);
    if (!thingLightOwnerAllowed(flags, ownerPolicy))
    {
        return out;
    }

    const TplValue* value = level::thingFindValue(level, templates, thing, "light");
    if (!value)
    {
        return out;
    }

    if (level.kind == ProjectType::IJIM)
    {
        const IjimAuthoredLight parsed = parseIjimAuthoredLight(*value);
        out.color = parsed.color;
        if (!parsed.valid
            || (!hasVisibleColor(out.color)
                && ownerPolicy != ThingLightOwnerPolicy::AuthoringPreview))
        {
            return out;
        }

        const float range = std::max(parsed.range, 0.0f);
        out.minRadius = range;
        out.maxRadius = range;
        out.classification = (range <= kIjimFlatLightMaxRange)
            ? ThingLightClass::Flat
            : ThingLightClass::Dynamic;
        return out;
    }

    const float intensity = scalarThingLightValue(*value);
    if (!finitePositive(intensity)
        && !(ownerPolicy == ThingLightOwnerPolicy::AuthoringPreview
            && std::isfinite(intensity)
            && intensity == 0.0f))
    {
        return out;
    }

    out.color = QVector3D(intensity, intensity, intensity);
    out.minRadius = intensity / kClassicAttenuationMin;
    out.maxRadius = intensity / kClassicAttenuationMax;
    out.classification = ThingLightClass::Dynamic;
    return out;
}

/** Apply IJIM's range-at-or-below-0.01 self light to the Thing ambient.
 *
 * sithRender installs this adjusted camera ambient only while drawing the
 * emitting Thing. It is therefore deliberately separate from the dynamic
 * scene-light collector and preserves the sector ambient alpha.
 */
inline ColorF applyIjimFlatSelfLight(
    const ColorF& ambient,
    const RuntimeThingLight& light) noexcept
{
    if (light.classification != ThingLightClass::Flat)
    {
        return ambient;
    }

    ColorF out = ambient;
    out.r = std::clamp(out.r + light.color.x(), 0.0f, 1.0f);
    out.g = std::clamp(out.g + light.color.y(), 0.0f, 1.0f);
    out.b = std::clamp(out.b + light.color.z(), 0.0f, 1.0f);
    return out;
}

/** Apply JK/MotS regular-Thing self light to the Thing camera ambient floor.
 *
 * The classic renderer still registers the regular light as a scene emitter,
 * but temporarily raises the owning Thing's camera ambient to the authored
 * scalar when that scalar is brighter than the sector floor. Headlights use a
 * separate runtime light entry and never take this owner-only path.
 */
inline ColorF applyClassicRegularSelfLight(
    const ColorF& ambient,
    const RuntimeThingLight& light) noexcept
{
    if (light.source != RuntimeThingLightSource::Regular
        || light.classification != ThingLightClass::Dynamic)
    {
        return ambient;
    }

    const float intensity = std::clamp(light.color.x(), 0.0f, 1.0f);
    ColorF out = ambient;
    out.r = std::max(out.r, intensity);
    out.g = std::max(out.g, intensity);
    out.b = std::max(out.b, intensity);
    return out;
}

/** Project-aware owner-only Thing light adjustment used by every draw path. */
inline ColorF applyRuntimeThingSelfLight(
    const ProjectType project,
    const ColorF& ambient,
    const RuntimeThingLight& light,
    const bool enabled = true) noexcept
{
    if ( !enabled || light.source != RuntimeThingLightSource::Regular )
    {
        return ambient;
    }
    return (project == ProjectType::IJIM)
        ? applyIjimFlatSelfLight(ambient, light)
        : applyClassicRegularSelfLight(ambient, light);
}

/** Resolve an actor/player head light (`lightintensity` + `lightoffset`).
 * IJIM stores raw RGB plus an optional fourth range component. OpenJones3D
 * copies that fourth value into both rdLight radii; an exact zero lets
 * rdCamera_AddLight independently derive the outer/inner radii from average
 * RGB. JK/MotS stores one scalar intensity and always derives both radii.
 */
inline RuntimeThingLight resolveRuntimeHeadLight(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const std::int32_t thingIndex,
    const std::optional<std::int32_t> sectorOverride = std::nullopt,
    const ThingLightOwnerPolicy ownerPolicy = ThingLightOwnerPolicy::RuntimeParity) noexcept
{
    RuntimeThingLight out{};
    out.source = RuntimeThingLightSource::Headlight;
    out.sourceThing = thingIndex;
    if (!validThingSector(level, thing, sectorOverride)
        || !isActorOrPlayer(level, templates, thing)
        || (integralThingValue(level, templates, thing, "typeflags") & kActorHeadlightFlag) == 0)
    {
        return out;
    }

    const std::int32_t flags = level::thingFlags(level, templates, thing);
    if (!thingLightOwnerAllowed(flags, ownerPolicy))
    {
        return out;
    }

    const TplValue* intensityValue = level::thingFindValue(level, templates, thing, "lightintensity");
    if (!intensityValue)
    {
        return out;
    }
    QVector3D lightColor{};
    float radiusIntensity{};
    if (level.kind == ProjectType::IJIM)
    {
        const IjimAuthoredLight intensity = parseIjimAuthoredLight(*intensityValue);
        if (!intensity.valid
            || (!hasVisibleColor(intensity.color)
                && ownerPolicy != ThingLightOwnerPolicy::AuthoringPreview))
        {
            return out;
        }
        lightColor = intensity.color;
        radiusIntensity = (lightColor.x() + lightColor.y() + lightColor.z()) / 3.0f;
        if (intensity.range != 0.0f)
        {
            out.minRadius = intensity.range;
            out.maxRadius = intensity.range;
        }
    }
    else
    {
        radiusIntensity = scalarThingLightValue(*intensityValue);
        lightColor = QVector3D(radiusIntensity, radiusIntensity, radiusIntensity);
    }
    if (!finitePositive(radiusIntensity)
        && !(ownerPolicy == ThingLightOwnerPolicy::AuthoringPreview
            && std::isfinite(radiusIntensity)
            && radiusIntensity == 0.0f))
    {
        return out;
    }

    QVector3D offset{};
    if (const TplValue* offsetValue = level::thingFindValue(level, templates, thing, "lightoffset"))
    {
        const IjimAuthoredLight parsedOffset = parseIjimAuthoredLight(*offsetValue);
        if (parsedOffset.valid)
        {
            offset = parsedOffset.color;
        }
    }

    TVector pyr{};
    pyr.pitch = thing.pch;
    pyr.yaw = thing.yaw;
    pyr.roll = thing.rol;
    const TMatrix43 orientation = TMatrix43::Create(TVector::zero, pyr);
    const TVector rotatedOffset = orientation.TransformVector(TVector{ offset.x(), offset.y(), offset.z() });

    out.position = QVector3D(
        static_cast<float>(thing.x + rotatedOffset.x),
        static_cast<float>(thing.y + rotatedOffset.y),
        static_cast<float>(thing.z + rotatedOffset.z));
    if (!std::isfinite(out.position.x()) || !std::isfinite(out.position.y()) || !std::isfinite(out.position.z()))
    {
        return RuntimeThingLight{};
    }
    out.color = lightColor;
    if (level.kind == ProjectType::IJIM)
    {
        if (out.minRadius == 0.0f)
        {
            out.minRadius = radiusIntensity / kIjimAttenuationMin;
        }
        if (out.maxRadius == 0.0f)
        {
            out.maxRadius = radiusIntensity / kIjimAttenuationMax;
        }
    }
    else
    {
        out.minRadius = radiusIntensity / kClassicAttenuationMin;
        out.maxRadius = radiusIntensity / kClassicAttenuationMax;
    }
    out.classification = ThingLightClass::Dynamic;
    return out;
}

inline std::array<QVector4D, 6> extractNormalizedFrustumPlanes(const QMatrix4x4& clipFromWorld) noexcept
{
    const QVector4D r0 = clipFromWorld.row(0);
    const QVector4D r1 = clipFromWorld.row(1);
    const QVector4D r2 = clipFromWorld.row(2);
    const QVector4D r3 = clipFromWorld.row(3);
    std::array<QVector4D, 6> planes{
        r3 + r0,
        r3 - r0,
        r3 + r1,
        r3 - r1,
        r3 + r2,
        r3 - r2,
    };

    for (QVector4D& plane : planes)
    {
        const float len = QVector3D(plane.x(), plane.y(), plane.z()).length();
        if (len > 1.0e-8f && std::isfinite(len))
        {
            plane /= len;
        }
    }
    return planes;
}

/** Conservative sphere test: an off-camera emitter remains eligible whenever
 * its influence sphere crosses the visible frustum. */
inline bool influenceSphereIntersectsFrustum(
    const std::array<QVector4D, 6>& planes,
    const QVector3D& center,
    const float radius) noexcept
{
    if (!finitePositive(radius)
        || !std::isfinite(center.x()) || !std::isfinite(center.y()) || !std::isfinite(center.z()))
    {
        return false;
    }

    for (const QVector4D& plane : planes)
    {
        const float distance =
            (plane.x() * center.x()) + (plane.y() * center.y()) + (plane.z() * center.z()) + plane.w();
        if (distance < -radius)
        {
            return false;
        }
    }
    return true;
}

inline std::vector<RuntimeThingLight> collectRuntimeThingLights(
    const Level& level,
    const assets::Templates* templates,
    const QMatrix4x4& clipFromWorld,
    const QVector3D& cameraPosition,
    const std::size_t cap = 0,
    const std::span<const std::optional<QMatrix4x4>> sourceTransformOverrides = {},
    const std::span<const std::optional<std::int32_t>> sourceSectorOverrides = {},
    const float influenceRadiusScale = 1.0f,
    const ThingLightOwnerPolicy ownerPolicy = ThingLightOwnerPolicy::AuthoringPreview,
    const ThingLightCollectionPolicy collectionPolicy = ThingLightCollectionPolicy::ViewStable)
{
    const std::size_t projectCap = (level.kind == ProjectType::IJIM)
        ? kIjimThingLightCap
        : kClassicThingLightCap;
    const std::size_t effectiveCap = cap != 0 ? std::min(cap, projectCap) : projectCap;
    const auto planes = extractNormalizedFrustumPlanes(clipFromWorld);

    struct Candidate
    {
        RuntimeThingLight light{};
        float cameraDistanceSquared{};
        std::size_t collectionOrder{};
    };

    std::vector<Candidate> candidates{};
    candidates.reserve(level.things.size());
    std::size_t nextCollectionOrder = 0;
    const auto addCandidate = [&](RuntimeThingLight light, const std::size_t sourceIndex)
    {
        // AuthoringPreview deliberately resolves black sources so their gizmos
        // remain editable. They must not consume an engine light-pool slot.
        if (light.classification != ThingLightClass::Dynamic
            || !hasVisibleColor(light.color))
        {
            return;
        }

        if (sourceIndex < sourceTransformOverrides.size() && sourceTransformOverrides[sourceIndex])
        {
            light.position = sourceTransformOverrides[sourceIndex]->map(light.position);
        }

        // World vertices use maxRadius and 3DO vertices use minRadius as the
        // outer test. Keep the conservative larger value for collection.
        const float safeInfluenceScale = std::isfinite(influenceRadiusScale)
            ? std::max(1.0f, influenceRadiusScale)
            : 1.0f;
        const float influenceRadius =
            std::max(light.minRadius, light.maxRadius) * safeInfluenceScale;
        if (collectionPolicy == ThingLightCollectionPolicy::InfluenceFrustum
            && !influenceSphereIntersectsFrustum(planes, light.position, influenceRadius))
        {
            return;
        }

        const QVector3D delta = light.position - cameraPosition;
        const float distanceSquared = QVector3D::dotProduct(delta, delta);
        candidates.push_back(Candidate{
            light,
            std::isfinite(distanceSquared)
                ? distanceSquared
                : std::numeric_limits<float>::max(),
            nextCollectionOrder++,
        });
    };

    for (std::size_t i = 0; i < level.things.size(); ++i)
    {
        const std::optional<std::int32_t> sectorOverride =
            i < sourceSectorOverrides.size() ? sourceSectorOverrides[i] : std::nullopt;
        addCandidate(resolveRuntimeThingLight(
                         level,
                         templates,
                         level.things[i],
                         static_cast<std::int32_t>(i),
                         sectorOverride,
                         ownerPolicy),
            i);
        addCandidate(resolveRuntimeHeadLight(
                         level,
                         templates,
                         level.things[i],
                         static_cast<std::int32_t>(i),
                         sectorOverride,
                         ownerPolicy),
            i);
    }

    // Camera distance chooses membership only when more valid sources exist
    // than the original camera can hold. It must not become evaluation order:
    // IJIM clamps world dynamic RGB after every light, so feeding the shader a
    // distance-sorted list makes otherwise unchanged surfaces change color as
    // the editor camera crosses between two emitters. The engine evaluates its
    // rdCamera append order. Preserve our deterministic Thing traversal order
    // for the selected set after applying the editor's nearest-light cap.
    if (candidates.size() > effectiveCap)
    {
        std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
        {
            if (a.cameraDistanceSquared != b.cameraDistanceSquared)
            {
                return a.cameraDistanceSquared < b.cameraDistanceSquared;
            }
            return a.collectionOrder < b.collectionOrder;
        });
        candidates.resize(effectiveCap);
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
        {
            return a.collectionOrder < b.collectionOrder;
        });
    }

    const std::size_t count = candidates.size();
    std::vector<RuntimeThingLight> out{};
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        out.push_back(candidates[i].light);
    }
    return out;
}

/** IJIM world-vertex contribution from sithRender_BuildDynamicLights. */
inline QVector3D ijimWorldVertexContribution(
    const QVector3D& lightColor,
    const float distance,
    const float maxRadius) noexcept
{
    if (!std::isfinite(distance) || distance < 0.0f || !(distance < maxRadius))
    {
        return QVector3D{};
    }
    const float attenuation = distance * kIjimAttenuationMax;
    return QVector3D(
        lightColor.x() - attenuation,
        lightColor.y() - attenuation,
        lightColor.z() - attenuation);
}

/** One ordered IJIM world-light accumulation step, including its per-emitter clamp. */
inline QVector3D accumulateIjimWorldVertexLight(
    const QVector3D& accumulated,
    const QVector3D& lightColor,
    const float distance,
    const float maxRadius) noexcept
{
    const QVector3D sum =
        accumulated + ijimWorldVertexContribution(lightColor, distance, maxRadius);
    return QVector3D(
        std::clamp(sum.x(), 0.0f, 1.0f),
        std::clamp(sum.y(), 0.0f, 1.0f),
        std::clamp(sum.z(), 0.0f, 1.0f));
}

/** Final IJIM world-vertex composition performed by rdClip then rdCache.
 *
 * Dynamic light is already the per-emitter-clamped accumulator produced by
 * sithRender_BuildDynamicLights. rdClip first combines and clamps it with the
 * authored surface intensity; rdCache adds signed surface+sector extra light
 * afterwards and clamps once more.
 */
inline QVector3D finalizeIjimWorldVertexLightColor(
    const QVector3D& authoredColor,
    const QVector3D& dynamicColor,
    const QVector3D& extraColor) noexcept
{
    const auto clamped = [](const QVector3D& color) noexcept
    {
        return QVector3D(
            std::clamp(color.x(), 0.0f, 1.0f),
            std::clamp(color.y(), 0.0f, 1.0f),
            std::clamp(color.z(), 0.0f, 1.0f));
    };
    return clamped(clamped(authoredColor + dynamicColor) + extraColor);
}

inline QVector3D clampLightColor(const QVector3D& color) noexcept
{
    return QVector3D(
        std::clamp(color.x(), 0.0f, 1.0f),
        std::clamp(color.y(), 0.0f, 1.0f),
        std::clamp(color.z(), 0.0f, 1.0f));
}

inline QVector3D maxLightColor(
    const QVector3D& color,
    const QVector3D& floor) noexcept
{
    return QVector3D(
        std::max(color.x(), floor.x()),
        std::max(color.y(), floor.y()),
        std::max(color.z(), floor.z()));
}

/** Final JK/MotS world/Gouraud composition.
 *
 * rdPrimit3 first clamps authored plus dynamic vertex intensity. rdCache then
 * adds signed face/surface extra light, clamps again, and finally applies the
 * sector-derived camera ambient as a lower floor.
 */
inline QVector3D finalizeClassicVertexLightColor(
    const QVector3D& authoredColor,
    const QVector3D& dynamicColor,
    const QVector3D& extraColor,
    const QVector3D& ambientFloor) noexcept
{
    return maxLightColor(
        clampLightColor(clampLightColor(authoredColor + dynamicColor) + extraColor),
        clampLightColor(ambientFloor));
}

/** Final JK/MotS Diffuse-face composition.
 *
 * Unlike Gouraud, the flat contribution is not clipped before rdCache adds
 * face extra light. This preserves the first >=1 multi-light overshoot, which
 * remains observable when signed negative extra light follows it.
 */
inline QVector3D finalizeClassicDiffuseLightColor(
    const QVector3D& dynamicColor,
    const QVector3D& extraColor,
    const QVector3D& ambientFloor) noexcept
{
    return maxLightColor(
        clampLightColor(dynamicColor + extraColor),
        clampLightColor(ambientFloor));
}

/** Classic JK/MotS world-vertex scalar contribution. */
inline float classicWorldVertexContribution(
    const float intensity,
    const float distance,
    const float maxRadius) noexcept
{
    if (!std::isfinite(intensity) || !std::isfinite(distance)
        || distance < 0.0f || !(distance < maxRadius))
    {
        return 0.0f;
    }
    return intensity - (distance * kClassicAttenuationMax);
}

/** IJIM rdLight_CalcVertexIntensities RGB contribution used by 3DO meshes. */
inline QVector3D ijimModelVertexContribution(
    const QVector3D& lightColor,
    const QVector3D& lightPosition,
    const float minRadius,
    const float maxRadius,
    const QVector3D& vertexPosition,
    const QVector3D& vertexNormal) noexcept
{
    QVector3D lightDirection = lightPosition - vertexPosition;
    const float distance = lightDirection.length();
    if (!std::isfinite(distance) || !(distance < minRadius) || distance <= 1.0e-8f)
    {
        return {};
    }

    lightDirection /= distance;
    const float dot = QVector3D::dotProduct(vertexNormal.normalized(), lightDirection);
    if (!(dot > 0.0f))
    {
        return {};
    }

    float strength = 1.0f;
    if (!(distance < maxRadius))
    {
        const float span = minRadius - maxRadius;
        if (std::abs(span) <= 1.0e-8f)
        {
            return {};
        }
        const float attenuation = (distance - maxRadius) / span;
        strength = 1.0f - attenuation;
    }
    return lightColor * (dot * strength);
}

/** Final 3DO vertex RGB written by the original rdCache path.
 *
 * rdLight leaves the authored vertex color and every dynamic contribution
 * unbounded. Face extra light and camera ambient are added afterwards, then
 * rdCache clamps the completed vertex to [0, 1] before hardware interpolation
 * and material modulation. Keeping this as a final, per-vertex operation is
 * important: clamping only the fragment lets an over-bright endpoint wash
 * across a face and overpower the material texture.
 */
inline QVector3D finalizeModelVertexLightColor(
    const QVector3D& accumulatedColor) noexcept
{
    return QVector3D(
        std::clamp(accumulatedColor.x(), 0.0f, 1.0f),
        std::clamp(accumulatedColor.y(), 0.0f, 1.0f),
        std::clamp(accumulatedColor.z(), 0.0f, 1.0f));
}

/** OpenJKDF2/MotS 3DO contribution: (I - distance*0.4) * N.L. */
inline QVector3D classicModelVertexContribution(
    const float intensity,
    const QVector3D& lightPosition,
    const float minRadius,
    const QVector3D& vertexPosition,
    const QVector3D& vertexNormal) noexcept
{
    QVector3D lightDirection = lightPosition - vertexPosition;
    const float distance = lightDirection.length();
    if (!std::isfinite(distance) || !(distance < minRadius) || distance <= 1.0e-8f)
    {
        return {};
    }
    lightDirection /= distance;
    const float dot = QVector3D::dotProduct(vertexNormal.normalized(), lightDirection);
    if (!(dot > 0.0f))
    {
        return {};
    }
    const float contribution = (intensity - distance * kClassicAttenuationMin) * dot;
    return QVector3D(contribution, contribution, contribution);
}

/** Returns the flat Diffuse normal used for the side actually being rendered. */
inline QVector3D orientedModelDiffuseFaceNormal(
    const QVector3D& faceNormal,
    const bool backFacing) noexcept
{
    const QVector3D normal = faceNormal.normalized();
    return backFacing ? -normal : normal;
}

/** One contribution from IJIM rdLight_CalcFaceIntensity.
 *
 * The routine attenuates by signed point-to-plane distance while N.L is
 * calculated from the face's first vertex. Every vertex receives this value.
 *
 * `attenuationState` is intentionally mutable. The original routine reuses
 * and overwrites its attenuation argument inside the enabled-light loop, so
 * every contributing light after the first inherits the preceding signed
 * plane-distance product. This preserves that camera-light-order bug.
 */
inline QVector3D ijimModelFaceContributionLegacy(
    const QVector3D& lightColor,
    const QVector3D& lightPosition,
    const float minRadius,
    const QVector3D& faceAnchor,
    const QVector3D& faceNormal,
    float& attenuationState) noexcept
{
    const QVector3D normal = faceNormal.normalized();
    QVector3D lightDirection = lightPosition - faceAnchor;
    const float distance = lightDirection.length();
    if (!std::isfinite(distance) || distance <= 1.0e-8f)
    {
        return {};
    }

    const float planeDistance = QVector3D::dotProduct(lightDirection, normal);
    if (!(planeDistance < minRadius))
    {
        return {};
    }

    lightDirection /= distance;
    const float dot = QVector3D::dotProduct(normal, lightDirection);
    if (!(dot > 0.0f))
    {
        return {};
    }

    attenuationState = planeDistance * attenuationState;
    return QVector3D(
        (lightColor.x() - attenuationState) * dot,
        (lightColor.y() - attenuationState) * dot,
        (lightColor.z() - attenuationState) * dot);
}

/** One-light convenience wrapper for IJIM Diffuse-face calculations. */
inline QVector3D ijimModelFaceContribution(
    const QVector3D& lightColor,
    const QVector3D& lightPosition,
    const float minRadius,
    const QVector3D& faceAnchor,
    const QVector3D& faceNormal) noexcept
{
    float attenuationState = kIjimAttenuationMin;
    return ijimModelFaceContributionLegacy(
        lightColor,
        lightPosition,
        minRadius,
        faceAnchor,
        faceNormal,
        attenuationState);
}

/** JK/MotS rdLight_CalcFaceIntensity scalar Diffuse-face contribution. */
inline QVector3D classicModelFaceContribution(
    const float intensity,
    const QVector3D& lightPosition,
    const float minRadius,
    const QVector3D& faceAnchor,
    const QVector3D& faceNormal) noexcept
{
    const QVector3D normal = faceNormal.normalized();
    QVector3D lightDirection = lightPosition - faceAnchor;
    const float distance = lightDirection.length();
    if (!std::isfinite(distance) || distance <= 1.0e-8f)
    {
        return {};
    }

    const float planeDistance = QVector3D::dotProduct(lightDirection, normal);
    if (!(planeDistance < minRadius))
    {
        return {};
    }

    lightDirection /= distance;
    const float dot = QVector3D::dotProduct(normal, lightDirection);
    if (!(dot > 0.0f))
    {
        return {};
    }

    const float contribution = (intensity - planeDistance * kClassicAttenuationMin) * dot;
    return QVector3D(contribution, contribution, contribution);
}

} // namespace sed::render::lighting
