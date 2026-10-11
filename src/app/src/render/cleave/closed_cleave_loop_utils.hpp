#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

#include <QPointF>
#include "render/cleave/cleave_geometry_utils.hpp"

namespace sed::render
{

struct ClosedCleaveLoopPlane
{
    TVector normal{};
    TVector point{};
};

enum class ClosedCleaveLoopError : std::uint8_t
{
    None,
    TooFewVertices,
    PlaneCountMismatch,
    NonFiniteValue,
    DuplicateVertex,
    DegenerateArea,
    SelfIntersection,
    NonConvex,
    InvalidPlane,
    InteriorOnPlane,
    InconsistentInteriorHalfSpace,
};

struct ClosedCleaveLoopValidation
{
    ClosedCleaveLoopError error{ ClosedCleaveLoopError::None };
    std::int32_t firstIndex{ -1 };
    std::int32_t secondIndex{ -1 };

    [[nodiscard]] bool valid() const noexcept
    {
        return error == ClosedCleaveLoopError::None;
    }
};

namespace detail
{

[[nodiscard]] inline bool finitePoint(const QPointF& point) noexcept
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

[[nodiscard]] inline bool finiteVector(const TVector& vector) noexcept
{
    return std::isfinite(vector.x)
        && std::isfinite(vector.y)
        && std::isfinite(vector.z);
}

[[nodiscard]] inline double cross2d(
    const QPointF& first,
    const QPointF& second,
    const QPointF& third) noexcept
{
    const QPointF firstToSecond = second - first;
    const QPointF firstToThird = third - first;
    return firstToSecond.x() * firstToThird.y()
        - firstToSecond.y() * firstToThird.x();
}

[[nodiscard]] inline bool pointOnSegment(
    const QPointF& point,
    const QPointF& first,
    const QPointF& second,
    const double epsilon) noexcept
{
    return std::abs(cross2d(first, second, point)) <= epsilon
        && point.x() >= std::min(first.x(), second.x()) - epsilon
        && point.x() <= std::max(first.x(), second.x()) + epsilon
        && point.y() >= std::min(first.y(), second.y()) - epsilon
        && point.y() <= std::max(first.y(), second.y()) + epsilon;
}

[[nodiscard]] inline int orientationSign(
    const QPointF& first,
    const QPointF& second,
    const QPointF& third,
    const double epsilon) noexcept
{
    const double cross = cross2d(first, second, third);
    if (cross > epsilon)
    {
        return 1;
    }
    if (cross < -epsilon)
    {
        return -1;
    }
    return 0;
}

[[nodiscard]] inline bool segmentsIntersectOrTouch(
    const QPointF& firstA,
    const QPointF& secondA,
    const QPointF& firstB,
    const QPointF& secondB,
    const double epsilon) noexcept
{
    const int orientationA = orientationSign(firstA, secondA, firstB, epsilon);
    const int orientationB = orientationSign(firstA, secondA, secondB, epsilon);
    const int orientationC = orientationSign(firstB, secondB, firstA, epsilon);
    const int orientationD = orientationSign(firstB, secondB, secondA, epsilon);

    if (orientationA != 0 && orientationB != 0
        && orientationC != 0 && orientationD != 0)
    {
        return orientationA != orientationB && orientationC != orientationD;
    }

    return (orientationA == 0 && pointOnSegment(firstB, firstA, secondA, epsilon))
        || (orientationB == 0 && pointOnSegment(secondB, firstA, secondA, epsilon))
        || (orientationC == 0 && pointOnSegment(firstA, firstB, secondB, epsilon))
        || (orientationD == 0 && pointOnSegment(secondA, firstB, secondB, epsilon));
}

} // namespace detail

/**
 * Validates the screen-space topology and world-space half-spaces used by a
 * staged closed cleave loop.
 *
 * A valid loop is a finite, simple, strictly convex polygon with one unique
 * vertex per edge. `interiorPoint` must lie strictly on the same side of every
 * stored plane. Requiring a common side catches stale or reversed plane normals
 * before the batch cleave can interpret a visibly closed loop as a different
 * half-space intersection.
 */
[[nodiscard]] inline ClosedCleaveLoopValidation validateClosedCleaveLoop(
    const std::span<const QPointF> polygon,
    const std::span<const ClosedCleaveLoopPlane> planes,
    const TVector& interiorPoint,
    const double screenEpsilon = 1.0e-4,
    const double planeEpsilon = 0.0) noexcept
{
    auto fail = [](const ClosedCleaveLoopError error,
                   const std::size_t first = 0,
                   const std::size_t second = 0) noexcept
    {
        return ClosedCleaveLoopValidation{
            error,
            static_cast<std::int32_t>(first),
            static_cast<std::int32_t>(second),
        };
    };

    if (polygon.size() < 3)
    {
        return fail(ClosedCleaveLoopError::TooFewVertices);
    }
    if (planes.size() != polygon.size())
    {
        return fail(ClosedCleaveLoopError::PlaneCountMismatch);
    }
    if (!std::isfinite(screenEpsilon) || screenEpsilon <= 0.0
        || !std::isfinite(planeEpsilon) || planeEpsilon < 0.0
        || !detail::finiteVector(interiorPoint))
    {
        return fail(ClosedCleaveLoopError::NonFiniteValue);
    }

    const double pointEpsilonSquared = screenEpsilon * screenEpsilon;
    for (std::size_t index = 0; index < polygon.size(); ++index)
    {
        if (!detail::finitePoint(polygon[index]))
        {
            return fail(ClosedCleaveLoopError::NonFiniteValue, index);
        }
        for (std::size_t other = index + 1; other < polygon.size(); ++other)
        {
            const QPointF delta = polygon[index] - polygon[other];
            const double distanceSquared = delta.x() * delta.x() + delta.y() * delta.y();
            if (distanceSquared <= pointEpsilonSquared)
            {
                return fail(ClosedCleaveLoopError::DuplicateVertex, index, other);
            }
        }
    }

    double twiceArea = 0.0;
    for (std::size_t index = 0; index < polygon.size(); ++index)
    {
        const QPointF& current = polygon[index];
        const QPointF& next = polygon[(index + 1) % polygon.size()];
        twiceArea += current.x() * next.y() - current.y() * next.x();
    }
    if (!std::isfinite(twiceArea) || std::abs(twiceArea) <= screenEpsilon)
    {
        return fail(ClosedCleaveLoopError::DegenerateArea);
    }

    for (std::size_t firstEdge = 0; firstEdge < polygon.size(); ++firstEdge)
    {
        const std::size_t firstNext = (firstEdge + 1) % polygon.size();
        for (std::size_t secondEdge = firstEdge + 1; secondEdge < polygon.size(); ++secondEdge)
        {
            const std::size_t secondNext = (secondEdge + 1) % polygon.size();
            const bool adjacent = firstNext == secondEdge || secondNext == firstEdge;
            if (adjacent)
            {
                continue;
            }
            if (detail::segmentsIntersectOrTouch(
                    polygon[firstEdge],
                    polygon[firstNext],
                    polygon[secondEdge],
                    polygon[secondNext],
                    screenEpsilon))
            {
                return fail(ClosedCleaveLoopError::SelfIntersection, firstEdge, secondEdge);
            }
        }
    }

    const int winding = twiceArea > 0.0 ? 1 : -1;
    for (std::size_t index = 0; index < polygon.size(); ++index)
    {
        const double turn = detail::cross2d(
            polygon[index],
            polygon[(index + 1) % polygon.size()],
            polygon[(index + 2) % polygon.size()]);
        if (!std::isfinite(turn) || std::abs(turn) <= screenEpsilon
            || (turn > 0.0 ? 1 : -1) != winding)
        {
            return fail(ClosedCleaveLoopError::NonConvex, index);
        }
    }

    int interiorSide = 0;
    for (std::size_t index = 0; index < planes.size(); ++index)
    {
        const ClosedCleaveLoopPlane& plane = planes[index];
        if (!detail::finiteVector(plane.normal) || !detail::finiteVector(plane.point))
        {
            return fail(ClosedCleaveLoopError::NonFiniteValue, index);
        }
        TVector normal = plane.normal;
        if (!cleaveNormalize(normal))
        {
            return fail(ClosedCleaveLoopError::InvalidPlane, index);
        }
        const double normalizedDistance = normal.dot(interiorPoint - plane.point);
        if (!std::isfinite(normalizedDistance))
        {
            return fail(ClosedCleaveLoopError::NonFiniteValue, index);
        }
        const double distanceError = std::max(planeEpsilon,
            cleavePlaneError(normal, interiorPoint, plane.point));
        if (std::abs(normalizedDistance) <= distanceError)
        {
            return fail(ClosedCleaveLoopError::InteriorOnPlane, index);
        }
        const int side = normalizedDistance > 0.0f ? 1 : -1;
        if (interiorSide == 0)
        {
            interiorSide = side;
        }
        else if (side != interiorSide)
        {
            return fail(ClosedCleaveLoopError::InconsistentInteriorHalfSpace, index);
        }
    }

    return {};
}

} // namespace sed::render
