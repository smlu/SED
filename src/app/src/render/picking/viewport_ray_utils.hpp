#pragma once

#include "sed/geometry.hpp"

#include <cmath>
#include <optional>

#include <QMatrix4x4>
#include <QPointF>
#include <QVector3D>
#include <QVector4D>

namespace sed::render
{

struct ViewportRay
{
    TVector origin{};
    TVector direction{};
};

/**
 * Returns the ray segment length from the same origin used by
 * viewportRayForWidget() to the projection's actual far endpoint. The
 * camera's scalar far plane is measured along its forward axis and therefore
 * does not describe off-axis perspective rays or the full orthographic
 * near-to-far interval.
 */
inline std::optional<double> viewportRayMaxDistanceForWidget(
    const QMatrix4x4& projection,
    const QMatrix4x4& view,
    const QPointF& widgetPoint,
    const int width,
    const int height,
    const bool orthographic)
{
    if (width <= 0 || height <= 0
        || !std::isfinite(widgetPoint.x()) || !std::isfinite(widgetPoint.y()))
    {
        return std::nullopt;
    }

    const double ndcX = 2.0 * widgetPoint.x() / width - 1.0;
    const double ndcY = 1.0 - 2.0 * widgetPoint.y() / height;

    // QMatrix4x4 stores float coefficients. Inverting a perspective matrix
    // whose far plane is many orders of magnitude beyond its near plane is
    // ill-conditioned: the homogeneous far-point w is tiny, so the inverse
    // matrix can lose a meaningful fraction of the endpoint. Recover the
    // finite far depth directly from the projection's clip equation instead.
    // For the standard OpenGL perspective form, clipZ = m22*z + m23,
    // clipW = -z; at the far plane clipZ == clipW. Solving in double from the
    // stored coefficients follows the actual projection boundary without
    // dividing by the poorly-conditioned inverse homogeneous w.
    const double clipWFromZ = projection(3, 2);
    const double clipWConstant = projection(3, 3);
    if (!orthographic
        && std::abs(clipWFromZ + 1.0) <= 1.0e-6
        && std::abs(clipWConstant) <= 1.0e-7
        && std::abs(static_cast<double>(projection(3, 0))) <= 1.0e-7
        && std::abs(static_cast<double>(projection(3, 1))) <= 1.0e-7)
    {
        const double projection00 = projection(0, 0);
        const double projection01 = projection(0, 1);
        const double projection10 = projection(1, 0);
        const double projection11 = projection(1, 1);
        const double determinant = projection00 * projection11 - projection01 * projection10;
        const double farDenominator = static_cast<double>(projection(2, 2)) + 1.0;
        const double farNumerator = static_cast<double>(projection(2, 3));
        if (std::isfinite(determinant) && std::abs(determinant) > 0.0
            && std::isfinite(farDenominator) && farDenominator != 0.0
            && std::isfinite(farNumerator))
        {
            const double farDepth = std::abs(farNumerator / farDenominator);
            const double shiftedX = ndcX + projection(0, 2);
            const double shiftedY = ndcY + projection(1, 2);
            const double slopeX =
                (shiftedX * projection11 - projection01 * shiftedY) / determinant;
            const double slopeY =
                (projection00 * shiftedY - shiftedX * projection10) / determinant;
            const double distance = farDepth * std::hypot(1.0, slopeX, slopeY);
            if (std::isfinite(distance) && distance > 0.0)
            {
                return distance;
            }
            return std::nullopt;
        }
    }

    bool projectionInvertible = false;
    const QMatrix4x4 inverseProjection = projection.inverted(&projectionInvertible);
    QMatrix4x4 rotation = view;
    rotation.setColumn(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
    bool viewInvertible = false;
    const QMatrix4x4 inverseRotation = rotation.inverted(&viewInvertible);
    if (!projectionInvertible || !viewInvertible)
    {
        return std::nullopt;
    }

    const auto cameraPoint = [&](const double ndcZ) -> std::optional<TVector>
    {
        const double x = inverseProjection(0, 0) * ndcX + inverseProjection(0, 1) * ndcY
            + inverseProjection(0, 2) * ndcZ + inverseProjection(0, 3);
        const double y = inverseProjection(1, 0) * ndcX + inverseProjection(1, 1) * ndcY
            + inverseProjection(1, 2) * ndcZ + inverseProjection(1, 3);
        const double z = inverseProjection(2, 0) * ndcX + inverseProjection(2, 1) * ndcY
            + inverseProjection(2, 2) * ndcZ + inverseProjection(2, 3);
        const double w = inverseProjection(3, 0) * ndcX + inverseProjection(3, 1) * ndcY
            + inverseProjection(3, 2) * ndcZ + inverseProjection(3, 3);
        if (!std::isfinite(w) || w == 0.0)
        {
            return std::nullopt;
        }
        const TVector point{x / w, y / w, z / w};
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z)
            ? std::optional<TVector>{point} : std::nullopt;
    };

    const auto nearPoint = cameraPoint(-1.0);
    const auto farPoint = cameraPoint(1.0);
    if (!nearPoint || !farPoint)
    {
        return std::nullopt;
    }

    const TVector cameraDelta = orthographic
        ? *farPoint - *nearPoint
        : *farPoint;
    const TVector worldDelta{
        inverseRotation(0, 0) * cameraDelta.x + inverseRotation(0, 1) * cameraDelta.y
            + inverseRotation(0, 2) * cameraDelta.z,
        inverseRotation(1, 0) * cameraDelta.x + inverseRotation(1, 1) * cameraDelta.y
            + inverseRotation(1, 2) * cameraDelta.z,
        inverseRotation(2, 0) * cameraDelta.x + inverseRotation(2, 1) * cameraDelta.y
            + inverseRotation(2, 2) * cameraDelta.z};
    const double distance = std::hypot(worldDelta.x, worldDelta.y, worldDelta.z);
    return std::isfinite(distance) && distance > 0.0
        ? std::optional<double>{distance} : std::nullopt;
}

/**
 * Reconstructs a ray around the camera origin. Unprojection never forms an
 * absolute float world point, so small pointer changes survive scene translation.
 */
inline std::optional<ViewportRay> viewportRayForWidget(
    const QMatrix4x4& projection,
    const QMatrix4x4& view,
    const QVector3D& eye,
    const QPointF& widgetPoint,
    const int width,
    const int height,
    const bool orthographic)
{
    if ( width <= 0 || height <= 0 || !std::isfinite(widgetPoint.x()) || !std::isfinite(widgetPoint.y())
         || !std::isfinite(eye.x()) || !std::isfinite(eye.y()) || !std::isfinite(eye.z()) )
    {
        return std::nullopt;
    }
    bool projectionInvertible = false;
    const QMatrix4x4 inverseProjection = projection.inverted(&projectionInvertible);
    QMatrix4x4 rotation = view;
    rotation.setColumn(3, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
    bool viewInvertible = false;
    const QMatrix4x4 inverseRotation = rotation.inverted(&viewInvertible);
    if ( !projectionInvertible || !viewInvertible )
    {
        return std::nullopt;
    }

    const double ndcX = 2.0 * widgetPoint.x() / width - 1.0;
    const double ndcY = 1.0 - 2.0 * widgetPoint.y() / height;
    const auto cameraPoint = [&](const double ndcZ) -> std::optional<TVector>
    {
        const double x = inverseProjection(0, 0) * ndcX + inverseProjection(0, 1) * ndcY
            + inverseProjection(0, 2) * ndcZ + inverseProjection(0, 3);
        const double y = inverseProjection(1, 0) * ndcX + inverseProjection(1, 1) * ndcY
            + inverseProjection(1, 2) * ndcZ + inverseProjection(1, 3);
        const double z = inverseProjection(2, 0) * ndcX + inverseProjection(2, 1) * ndcY
            + inverseProjection(2, 2) * ndcZ + inverseProjection(2, 3);
        const double w = inverseProjection(3, 0) * ndcX + inverseProjection(3, 1) * ndcY
            + inverseProjection(3, 2) * ndcZ + inverseProjection(3, 3);
        if ( !std::isfinite(w) || w == 0.0 )
        {
            return std::nullopt;
        }
        const TVector point{x / w, y / w, z / w};
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z)
            ? std::optional<TVector>{point} : std::nullopt;
    };
    const auto nearPoint = cameraPoint(-1.0);
    // The middle plane is enough to determine a direction and remains finite
    // when an extreme far clip rounds the far homogeneous coordinate to zero.
    const auto middlePoint = cameraPoint(0.0);
    if ( !nearPoint || !middlePoint )
    {
        return std::nullopt;
    }
    const auto rotate = [&](const TVector& point)
    {
        return TVector{
            inverseRotation(0, 0) * point.x + inverseRotation(0, 1) * point.y + inverseRotation(0, 2) * point.z,
            inverseRotation(1, 0) * point.x + inverseRotation(1, 1) * point.y + inverseRotation(1, 2) * point.z,
            inverseRotation(2, 0) * point.x + inverseRotation(2, 1) * point.y + inverseRotation(2, 2) * point.z};
    };
    ViewportRay ray{};
    ray.origin = TVector{eye.x(), eye.y(), eye.z()};
    ray.direction = rotate(orthographic ? *middlePoint - *nearPoint : *middlePoint);
    if ( orthographic )
    {
        ray.origin = ray.origin + rotate(*nearPoint);
    }
    const double length = std::hypot(ray.direction.x, ray.direction.y, ray.direction.z);
    if ( !std::isfinite(length) || length == 0.0 || !std::isfinite(ray.origin.x)
         || !std::isfinite(ray.origin.y) || !std::isfinite(ray.origin.z) )
    {
        return std::nullopt;
    }
    ray.direction.Scale(1.0 / length);
    return ray;
}

} // namespace sed::render
