#pragma once

#include "render/mesh/render_lighting_utils.hpp"

namespace sed
{
namespace
{

template <typename T>
concept FloatType = std::same_as<T, float> || std::same_as<T, double>;

constexpr bool kFlipV = false;

constexpr double kDegToRad = std::numbers::pi_v<double> / 180.0;

using render::ThingSectorLighting;
using render::calcThingLightMul;
using render::calcThingSectorLighting;
using render::calcThingSectorLightingMul;
using render::ijimFogTableEnd;
using render::ijimFogTableStart;
using render::kThingIconMatPrefix;
using render::levelSurfaceRenderFaceFlags;
using render::makeBatchKey;
using render::modelFaceRenderFaceFlags;
using render::polyCalcLitColor;
using render::sectorRenderColormap;
using render::shouldRenderIjimFog;

constexpr FloatType auto deg2Rad(const FloatType auto deg) noexcept
{
    return deg * static_cast<decltype(deg)>(kDegToRad);
}

float calcInvDistanceScale(
    const float distance,
    const float minDistance,
    const float maxDistance,
    const float minScale,
    const float maxScale) noexcept
{
    if (!std::isfinite(distance) || !std::isfinite(minDistance) || !std::isfinite(maxDistance) || !std::isfinite(minScale) || !std::isfinite(maxScale))
    {
        return 1.0f;
    }

    if (distance <= minDistance)
    {
        return minScale;
    }
    if (distance >= maxDistance)
    {
        return maxScale;
    }

    const float invMin = 1.0f / std::max(maxDistance, 1e-6f);
    const float invMax = 1.0f / std::max(minDistance, 1e-6f);
    const float invDist = 1.0f / std::max(distance, 1e-6f);

    const float denom = invMin - invMax;
    float t = (std::abs(denom) > sed::math::kTinyEpsilonF) ? ((invDist - invMax) / denom) : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);

    return minScale + (maxScale - minScale) * t;
}

#include "render/detail/axis_widget.hpp"

#include "render/detail/view_helpers.hpp"

std::optional<QVector3D> intersectWidgetRayWithPlane(
    const QPoint& widgetPos,
    const QMatrix4x4& invMvp,
    const QVector3D& eye,
    const bool isOrtho,
    const int widgetW,
    const int widgetH,
    const QVector3D& planePoint,
    const QVector3D& planeNormalUnit) noexcept
{
    if (widgetW <= 0 || widgetH <= 0)
    {
        return std::nullopt;
    }

    if (!std::isfinite(planeNormalUnit.x()) || !std::isfinite(planeNormalUnit.y()) || !std::isfinite(planeNormalUnit.z()))
    {
        return std::nullopt;
    }

    const float w = static_cast<float>(std::max(1, widgetW));
    const float h = static_cast<float>(std::max(1, widgetH));

    const float ndcX = (2.0f * static_cast<float>(widgetPos.x()) / w) - 1.0f;
    const float ndcY = 1.0f - (2.0f * static_cast<float>(widgetPos.y()) / h);

    const QVector3D nearWorld = unprojectNdc(invMvp, ndcX, ndcY, -1.0f);
    const QVector3D farWorld = unprojectNdc(invMvp, ndcX, ndcY, 1.0f);

    const QVector3D rayOrigin = isOrtho ? nearWorld : eye;
    QVector3D rayDir = farWorld - rayOrigin;
    if (rayDir.lengthSquared() < sed::math::kTinyEpsilonF)
    {
        return std::nullopt;
    }
    rayDir.normalize();

    const float denom = QVector3D::dotProduct(rayDir, planeNormalUnit);
    if (!std::isfinite(denom) || std::abs(denom) < 1e-6f)
    {
        return std::nullopt;
    }

    const float t = QVector3D::dotProduct(planePoint - rayOrigin, planeNormalUnit) / denom;
    if (!std::isfinite(t))
    {
        return std::nullopt;
    }

    const QVector3D hit = rayOrigin + rayDir * t;
    return hit;
}

} // namespace
} // namespace sed
