#pragma once

#include "sed/geometry.hpp"

#include <cmath>
#include <numbers>

#include <QVector3D>

namespace sed::render
{

inline TVector authoringVector(const QVector3D& value) noexcept
{
    return TVector{value.x(), value.y(), value.z()};
}

inline QVector3D displayVector(const TVector& value) noexcept
{
    return QVector3D(static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z));
}

/** Qt/OpenGL rotation convention evaluated on a local double displacement. */
inline TVector rotateAuthoringVector(const TVector& vector, TVector axis, const double angleDegrees) noexcept
{
    const double length = std::hypot(axis.x, axis.y, axis.z);
    if (!std::isfinite(length) || length == 0.0 || !std::isfinite(angleDegrees) || angleDegrees == 0.0)
    {
        return vector;
    }
    axis.Scale(1.0 / length);
    const double angle = angleDegrees * std::numbers::pi / 180.0;
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    const TVector cross{axis.y * vector.z - axis.z * vector.y,
        axis.z * vector.x - axis.x * vector.z, axis.x * vector.y - axis.y * vector.x};
    return vector * cosine + cross * sine + axis * (axis.Dot(vector) * (1.0 - cosine));
}

} // namespace sed::render
