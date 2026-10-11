#include "render/fast_update/thing_transform_update.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "render/mesh/render_mesh_utils.hpp"
#include "sed/math_utils.hpp"

namespace sed::render::fast_update
{
QVector3D Affine3f::mapPoint(const QVector3D& point) const noexcept
{
    return QVector3D(
        m00 * point.x() + m01 * point.y() + m02 * point.z() + m03,
        m10 * point.x() + m11 * point.y() + m12 * point.z() + m13,
        m20 * point.x() + m21 * point.y() + m22 * point.z() + m23);
}

Affine3f affineFromMatrix(const QMatrix4x4& matrix) noexcept
{
    Affine3f out{};
    out.m00 = matrix(0, 0); out.m01 = matrix(0, 1); out.m02 = matrix(0, 2); out.m03 = matrix(0, 3);
    out.m10 = matrix(1, 0); out.m11 = matrix(1, 1); out.m12 = matrix(1, 2); out.m13 = matrix(1, 3);
    out.m20 = matrix(2, 0); out.m21 = matrix(2, 1); out.m22 = matrix(2, 2); out.m23 = matrix(2, 3);
    return out;
}

void transformDynamicLightGeometry(
    QVector3D& position,
    QVector3D& normal,
    QVector3D* diffuseFaceAnchor,
    const Affine3f& transform) noexcept
{
    position = transform.mapPoint(position);
    normal = QVector3D(
        transform.m00 * normal.x() + transform.m01 * normal.y() + transform.m02 * normal.z(),
        transform.m10 * normal.x() + transform.m11 * normal.y() + transform.m12 * normal.z(),
        transform.m20 * normal.x() + transform.m21 * normal.y() + transform.m22 * normal.z()).normalized();
    if (diffuseFaceAnchor)
    {
        *diffuseFaceAnchor = transform.mapPoint(*diffuseFaceAnchor);
    }
}

bool nearlySameTransform(const double a, const double b) noexcept
{
    // Callers validate authored transform fields before comparison. A fixed
    // epsilon must not suppress a representable document change.
    return a == b;
}

bool nearlySameRenderValue(const float a, const float b) noexcept
{
    return sed::math::equal(a, b, sed::math::kRotationEpsilonF);
}

bool nearlySameRenderValue(const QVector3D& a, const QVector3D& b) noexcept
{
    return nearlySameRenderValue(a.x(), b.x())
        && nearlySameRenderValue(a.y(), b.y())
        && nearlySameRenderValue(a.z(), b.z());
}

bool nearlySameRenderValue(const ColorF& a, const ColorF& b) noexcept
{
    return nearlySameRenderValue(a.r, b.r)
        && nearlySameRenderValue(a.g, b.g)
        && nearlySameRenderValue(a.b, b.b)
        && nearlySameRenderValue(a.a, b.a);
}

bool usesOriginalSpriteCameraFacingFrame(const int spriteType, const bool gamePreview) noexcept
{
    return gamePreview && spriteType == 0;
}

OriginalSpriteType2Frame makeOriginalSpriteType2Frame(
    const QVector3D& thingForward) noexcept
{
    const auto finiteVector = [](const QVector3D& value) noexcept
    {
        return std::isfinite(value.x())
            && std::isfinite(value.y())
            && std::isfinite(value.z());
    };
    const auto normalize = [&finiteVector](QVector3D value, const QVector3D& fallback) noexcept
    {
        if (!finiteVector(value)
            || value.lengthSquared() <= sed::math::kTinyEpsilonF)
        {
            return fallback;
        }
        value.normalize();
        return finiteVector(value) ? value : fallback;
    };

    OriginalSpriteType2Frame frame{};
    frame.forward = normalize(thingForward, QVector3D(0.0f, 1.0f, 0.0f));
    const QVector3D reference = std::abs(frame.forward.z()) < 0.99989998f
        ? QVector3D(0.0f, 0.0f, 1.0f)
        : QVector3D(1.0f, 0.0f, 0.0f);
    frame.right = normalize(
        QVector3D::crossProduct(reference, frame.forward),
        QVector3D(-1.0f, 0.0f, 0.0f));
    frame.up = normalize(
        QVector3D::crossProduct(frame.right, frame.forward),
        QVector3D(0.0f, 0.0f, -1.0f));
    return frame;
}

int pickIdKey(const float pickId) noexcept
{
    return sed::render::pickIdKey(pickId);
}

void includeBounds(
    QVector3D& outMin,
    QVector3D& outMax,
    bool& haveBounds,
    const QVector3D& min,
    const QVector3D& max) noexcept
{
    if (!haveBounds)
    {
        outMin = min;
        outMax = max;
        haveBounds = true;
        return;
    }

    outMin.setX(std::min(outMin.x(), min.x()));
    outMin.setY(std::min(outMin.y(), min.y()));
    outMin.setZ(std::min(outMin.z(), min.z()));
    outMax.setX(std::max(outMax.x(), max.x()));
    outMax.setY(std::max(outMax.y(), max.y()));
    outMax.setZ(std::max(outMax.z(), max.z()));
}

void transformBounds(QVector3D& min, QVector3D& max, const Affine3f& xf) noexcept
{
    const QVector3D oldMin = min;
    const QVector3D oldMax = max;
    const std::array<QVector3D, 8> corners{
        QVector3D(oldMin.x(), oldMin.y(), oldMin.z()),
        QVector3D(oldMax.x(), oldMin.y(), oldMin.z()),
        QVector3D(oldMin.x(), oldMax.y(), oldMin.z()),
        QVector3D(oldMax.x(), oldMax.y(), oldMin.z()),
        QVector3D(oldMin.x(), oldMin.y(), oldMax.z()),
        QVector3D(oldMax.x(), oldMin.y(), oldMax.z()),
        QVector3D(oldMin.x(), oldMax.y(), oldMax.z()),
        QVector3D(oldMax.x(), oldMax.y(), oldMax.z()),
    };

    QVector3D outMin(
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max());
    QVector3D outMax(
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max());

    for (const QVector3D& corner : corners)
    {
        const QVector3D point = xf.mapPoint(corner);
        outMin.setX(std::min(outMin.x(), point.x()));
        outMin.setY(std::min(outMin.y(), point.y()));
        outMin.setZ(std::min(outMin.z(), point.z()));
        outMax.setX(std::max(outMax.x(), point.x()));
        outMax.setY(std::max(outMax.y(), point.y()));
        outMax.setZ(std::max(outMax.z(), point.z()));
    }

    min = outMin;
    max = outMax;
}

void transformBounds(QVector3D& min, QVector3D& max, QVector3D& center, const Affine3f& xf) noexcept
{
    transformBounds(min, max, xf);
    center = (min + max) * 0.5f;
}

} // namespace sed::render::fast_update
