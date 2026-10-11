#pragma once

#include "render/overlays/thing_shape_overlay_utils.hpp"

#include "sed/level/thing.hpp"
#include "sed/text.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace sed::render::thing_overlay
{

struct AuthoredCollisionBoundsGuide
{
    double halfWidth{};
    double halfHeight{};
};

namespace detail
{

inline std::optional<double> collisionGuideScalar(const TplValue* value) noexcept
{
    if (!value)
    {
        return std::nullopt;
    }

    double parsed{};
    switch (value->type)
    {
    case ValueType::integer:
    case ValueType::flag:
    case ValueType::ptr:
        parsed = static_cast<double>(value->intValue);
        break;
    case ValueType::floating:
        parsed = value->floatValue;
        break;
    case ValueType::str:
    case ValueType::unknown:
        if (const std::optional<double> textValue = text::parseDouble(value->stringValue))
        {
            parsed = *textValue;
        }
        else
        {
            return std::nullopt;
        }
        break;
    case ValueType::vect:
    case ValueType::frame:
        return std::nullopt;
    }

    // OpenJones3D treats zero as an unset sentinel for both dimensions and
    // replaces it from movesize/model radius after loading a model. Keep this
    // guide authored-only, so zero does not draw a misleading zero-size bound.
    return std::isfinite(parsed)
            && parsed > 0.0
            && parsed <= static_cast<double>(std::numeric_limits<float>::max())
        ? std::optional<double>{ parsed }
        : std::nullopt;
}

} // namespace detail

/**
 * Resolve a complete IJIM `collwidth`/`collheight` parameter guide. Width is
 * the X/Y half-extent and height is the Z half-extent, centered on the Thing
 * and aligned to world axes. Both authored dimensions must be positive; if
 * either is absent, invalid, or zero, the guide is omitted rather than drawing
 * a partial or guessed shape. This is deliberately independent of `collide`:
 * the engine's corresponding AABB collision check is inactive, so this only
 * visualizes authored parameters.
 */
inline std::optional<AuthoredCollisionBoundsGuide> resolveIjimAuthoredCollisionBoundsGuide(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing) noexcept
{
    if (level.kind != ProjectType::IJIM)
    {
        return std::nullopt;
    }

    const std::optional<double> halfWidth = detail::collisionGuideScalar(
        level::thingFindValue(level, templates, thing, "collwidth"));
    const std::optional<double> halfHeight = detail::collisionGuideScalar(
        level::thingFindValue(level, templates, thing, "collheight"));
    if (!halfWidth || !halfHeight)
    {
        return std::nullopt;
    }

    return AuthoredCollisionBoundsGuide{ *halfWidth, *halfHeight };
}

inline std::vector<TVector> makeAuthoredCollisionBoundsGuideWirePoints(
    const AuthoredCollisionBoundsGuide& guide,
    const TMatrix43& transform)
{
    if (!std::isfinite(guide.halfWidth)
        || !std::isfinite(guide.halfHeight)
        || !(guide.halfWidth > 0.0)
        || !(guide.halfHeight > 0.0))
    {
        return {};
    }

    const TVector center = transform.TransformPoint(TVector{});
    if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z))
    {
        return {};
    }

    // Engine collide bounds use world-axis-aligned extents around Thing pos;
    // only the center follows the Thing transform, never its rotation.
    const TMatrix43 centerTranslation = TMatrix43::CreateTranslate(center);
    const std::array<TVector, 24> boxLines = makeBoundsWirePoints(
        Bounds3d{
            TVector{ -guide.halfWidth, -guide.halfWidth, -guide.halfHeight },
            TVector{ guide.halfWidth, guide.halfWidth, guide.halfHeight },
        },
        centerTranslation);
    return std::vector<TVector>(boxLines.begin(), boxLines.end());
}

} // namespace sed::render::thing_overlay
