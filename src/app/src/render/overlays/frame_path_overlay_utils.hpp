#pragma once

#include "sed/geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

#include <QMatrix4x4>
#include <QPointF>
#include <QVector3D>
#include <QVector4D>

namespace sed::render::frame_path
{

inline constexpr float kMinimumClipW = 1.0e-5f;

namespace detail
{

struct ClipPoint
{
    double x{};
    double y{};
    double z{};
    double w{};
};

[[nodiscard]] inline ClipPoint worldToClip(const QMatrix4x4& matrix, const TVector& point) noexcept
{
    const auto component = [&](const int row) noexcept
    {
        return static_cast<double>(matrix(row, 0)) * point.x
            + static_cast<double>(matrix(row, 1)) * point.y
            + static_cast<double>(matrix(row, 2)) * point.z
            + static_cast<double>(matrix(row, 3));
    };
    return {component(0), component(1), component(2), component(3)};
}

[[nodiscard]] inline ClipPoint toClipPoint(const QVector4D& point) noexcept
{
    return ClipPoint{ point.x(), point.y(), point.z(), point.w() };
}

[[nodiscard]] inline ClipPoint lerp(
    const ClipPoint& first,
    const ClipPoint& second,
    const double t) noexcept
{
    return ClipPoint{
        first.x + (second.x - first.x) * t,
        first.y + (second.y - first.y) * t,
        first.z + (second.z - first.z) * t,
        first.w + (second.w - first.w) * t
    };
}

// Camera matrices originate as floats. Scale the boundary tolerance with the
// homogeneous coordinates rather than with 1.0 so that it remains useful near
// the small positive-w camera guard without accepting visibly off-screen NDC.
[[nodiscard]] inline double boundaryTolerance(
    const ClipPoint& point,
    const double minimumW) noexcept
{
    const double scale = std::max({
        std::abs(point.x), std::abs(point.y), std::abs(point.z),
        std::abs(point.w), std::abs(minimumW),
        std::numeric_limits<double>::min()
    });
    return scale * static_cast<double>(std::numeric_limits<float>::epsilon()) * 32.0;
}

[[nodiscard]] inline std::array<double, 7> halfspaceDistances(
    const ClipPoint& point,
    const double minimumW) noexcept
{
    return {
        point.x + point.w,
        -point.x + point.w,
        point.y + point.w,
        -point.y + point.w,
        point.z + point.w,
        -point.z + point.w,
        point.w - minimumW
    };
}

[[nodiscard]] inline std::optional<QPointF> projectInsideClipPoint(
    const ClipPoint& point,
    const int viewportWidth,
    const int viewportHeight,
    const double minimumW) noexcept
{
    if (viewportWidth <= 0 || viewportHeight <= 0
        || !std::isfinite(point.x) || !std::isfinite(point.y)
        || !std::isfinite(point.z) || !std::isfinite(point.w))
    {
        return std::nullopt;
    }

    const double tolerance = boundaryTolerance(point, minimumW);
    const std::array<double, 7> distances = halfspaceDistances(point, minimumW);
    if (std::any_of(distances.begin(), distances.end(), [tolerance](const double distance)
        {
            return distance < -tolerance;
        }))
    {
        return std::nullopt;
    }

    // A clipped endpoint can be a few ulps below the positive-w guard.  Use
    // the guard for division in that case, and clamp the equally tiny NDC
    // boundary overshoot before handing coordinates to QPainter.
    const double safeW = std::max(point.w, minimumW);
    if (!std::isfinite(safeW) || safeW <= 0.0)
    {
        return std::nullopt;
    }
    const double ndcX = std::clamp(point.x / safeW, -1.0, 1.0);
    const double ndcY = std::clamp(point.y / safeW, -1.0, 1.0);
    if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
    {
        return std::nullopt;
    }

    return QPointF(
        (ndcX * 0.5 + 0.5) * static_cast<double>(viewportWidth),
        (1.0 - (ndcY * 0.5 + 0.5)) * static_cast<double>(viewportHeight));
}

} // namespace detail

struct ProjectedSegment
{
    QPointF first{};
    QPointF second{};
    float firstT{};
    float secondT{1.0f};
};

[[nodiscard]] inline bool finiteClipPoint(const QVector4D& point) noexcept
{
    return std::isfinite(point.x())
        && std::isfinite(point.y())
        && std::isfinite(point.z())
        && std::isfinite(point.w());
}

[[nodiscard]] inline bool finiteClipPoint(const detail::ClipPoint& point) noexcept
{
    return std::isfinite(point.x) && std::isfinite(point.y)
        && std::isfinite(point.z) && std::isfinite(point.w);
}

[[nodiscard]] inline std::optional<QPointF> projectClipPoint(
    const QVector4D& clip,
    const int viewportWidth,
    const int viewportHeight,
    const float minimumW = kMinimumClipW) noexcept
{
    if (!finiteClipPoint(clip) || !std::isfinite(minimumW) || minimumW <= 0.0f)
    {
        return std::nullopt;
    }
    return detail::projectInsideClipPoint(
        detail::toClipPoint(clip),
        viewportWidth,
        viewportHeight,
        static_cast<double>(minimumW));
}

[[nodiscard]] inline std::optional<QPointF> projectWorldPoint(
    const QMatrix4x4& mvp,
    const QVector3D& world,
    const int viewportWidth,
    const int viewportHeight,
    const float minimumW = kMinimumClipW) noexcept
{
    return projectClipPoint(
        mvp * QVector4D(world, 1.0f),
        viewportWidth,
        viewportHeight,
        minimumW);
}

[[nodiscard]] inline std::optional<ProjectedSegment> projectClippedSegment(
    const detail::ClipPoint& clipFirst,
    const detail::ClipPoint& clipSecond,
    const int viewportWidth,
    const int viewportHeight,
    const float minimumW = kMinimumClipW) noexcept
{
    if (viewportWidth <= 0 || viewportHeight <= 0
        || !std::isfinite(minimumW) || minimumW <= 0.0f
        || !finiteClipPoint(clipFirst) || !finiteClipPoint(clipSecond))
    {
        return std::nullopt;
    }

    const detail::ClipPoint first = clipFirst;
    const detail::ClipPoint second = clipSecond;
    const double minimumWDouble = static_cast<double>(minimumW);
    const std::array<double, 7> firstDistances = detail::halfspaceDistances(first, minimumWDouble);
    const std::array<double, 7> secondDistances = detail::halfspaceDistances(second, minimumWDouble);
    const double firstTolerance = detail::boundaryTolerance(first, minimumWDouble);
    const double secondTolerance = detail::boundaryTolerance(second, minimumWDouble);

    double firstT = 0.0;
    double secondT = 1.0;
    for (std::size_t plane = 0; plane < firstDistances.size(); ++plane)
    {
        const double firstDistance = firstDistances[plane];
        const double secondDistance = secondDistances[plane];
        const bool firstOutside = firstDistance < -firstTolerance;
        const bool secondOutside = secondDistance < -secondTolerance;
        if (firstOutside && secondOutside)
        {
            return std::nullopt;
        }
        if (!firstOutside && !secondOutside)
        {
            continue;
        }

        const double denominator = firstDistance - secondDistance;
        if (!std::isfinite(denominator) || denominator == 0.0)
        {
            return std::nullopt;
        }
        const double t = std::clamp(firstDistance / denominator, 0.0, 1.0);
        if (firstOutside)
        {
            firstT = std::max(firstT, t);
        }
        else
        {
            secondT = std::min(secondT, t);
        }

        constexpr double intervalTolerance = 64.0 * std::numeric_limits<double>::epsilon();
        if (firstT > secondT + intervalTolerance)
        {
            return std::nullopt;
        }
        if (firstT > secondT)
        {
            firstT = secondT = (firstT + secondT) * 0.5;
        }
    }

    const detail::ClipPoint clippedFirst = detail::lerp(first, second, firstT);
    const detail::ClipPoint clippedSecond = detail::lerp(first, second, secondT);
    const std::optional<QPointF> projectedFirst = detail::projectInsideClipPoint(
        clippedFirst, viewportWidth, viewportHeight, minimumWDouble);
    const std::optional<QPointF> projectedSecond = detail::projectInsideClipPoint(
        clippedSecond, viewportWidth, viewportHeight, minimumWDouble);
    if (!projectedFirst || !projectedSecond)
    {
        return std::nullopt;
    }

    return ProjectedSegment{
        *projectedFirst,
        *projectedSecond,
        static_cast<float>(firstT),
        static_cast<float>(secondT)
    };
}

[[nodiscard]] inline std::optional<ProjectedSegment> projectClippedSegment(
    const QVector4D& clipFirst,
    const QVector4D& clipSecond,
    const int viewportWidth,
    const int viewportHeight,
    const float minimumW = kMinimumClipW) noexcept
{
    return projectClippedSegment(detail::toClipPoint(clipFirst), detail::toClipPoint(clipSecond),
        viewportWidth, viewportHeight, minimumW);
}

/** Preserves authored world detail until double homogeneous clipping/projection. */
[[nodiscard]] inline std::optional<ProjectedSegment> projectWorldSegment(
    const QMatrix4x4& mvp,
    const TVector& worldFirst,
    const TVector& worldSecond,
    const int viewportWidth,
    const int viewportHeight,
    const float minimumW = kMinimumClipW) noexcept
{
    return projectClippedSegment(detail::worldToClip(mvp, worldFirst), detail::worldToClip(mvp, worldSecond),
        viewportWidth, viewportHeight, minimumW);
}

[[nodiscard]] inline std::optional<QPointF> projectWorldPoint(
    const QMatrix4x4& mvp,
    const TVector& world,
    const int viewportWidth,
    const int viewportHeight,
    const float minimumW = kMinimumClipW) noexcept
{
    if ( !std::isfinite(minimumW) || minimumW <= 0.0f )
    {
        return std::nullopt;
    }
    return detail::projectInsideClipPoint(detail::worldToClip(mvp, world),
        viewportWidth, viewportHeight, minimumW);
}

[[nodiscard]] inline std::optional<ProjectedSegment> projectWorldSegment(
    const QMatrix4x4& mvp,
    const QVector3D& worldFirst,
    const QVector3D& worldSecond,
    const int viewportWidth,
    const int viewportHeight,
    const float minimumW = kMinimumClipW) noexcept
{
    return projectClippedSegment(
        mvp * QVector4D(worldFirst, 1.0f),
        mvp * QVector4D(worldSecond, 1.0f),
        viewportWidth,
        viewportHeight,
        minimumW);
}

} // namespace sed::render::frame_path
