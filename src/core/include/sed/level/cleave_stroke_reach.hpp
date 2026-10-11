#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>

#include "sed/level/cleave.hpp"

namespace sed::level
{

/** One oriented half-space inherited from an earlier successful plane split. */
struct CleaveStrokeHalfSpace
{
    TVector normal{};
    TVector point{};
    bool positiveSide{ true };
};

/**
 * Returns whether a positive-length portion of a finite authored stroke
 * reaches the intersection of all supplied half-spaces. A segment that only
 * touches a boundary at one endpoint does not reach the opposite half-space;
 * a positive-length segment contained by a boundary reaches both sides.
 */
[[nodiscard]] inline bool cleaveStrokeReachesHalfSpaces(
    const SectorCleaveStroke& stroke,
    const std::span<const CleaveStrokeHalfSpace> halfSpaces) noexcept
{
    const auto finiteVector = [](const TVector& value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    if ( !finiteVector(stroke.first) || !finiteVector(stroke.second) )
    {
        return false;
    }

    const long double strokeDx = static_cast<long double>(stroke.second.x) - stroke.first.x;
    const long double strokeDy = static_cast<long double>(stroke.second.y) - stroke.first.y;
    const long double strokeDz = static_cast<long double>(stroke.second.z) - stroke.first.z;
    if ( !std::isfinite(strokeDx) || !std::isfinite(strokeDy) || !std::isfinite(strokeDz)
        || (strokeDx == 0.0L && strokeDy == 0.0L && strokeDz == 0.0L) )
    {
        return false;
    }

    struct CrossingFraction
    {
        long double fromFirst{};
        long double fromSecond{};
    };
    const auto fractionLess = [](const CrossingFraction& lhs, const CrossingFraction& rhs) noexcept
    {
        return lhs.fromFirst * rhs.fromSecond
            < rhs.fromFirst * lhs.fromSecond;
    };
    CrossingFraction lowerBound{ 0.0L, 1.0L };
    CrossingFraction upperBound{ 1.0L, 0.0L };
    constexpr long double roundoff =
        16.0L * std::numeric_limits<double>::epsilon();

    for ( const CleaveStrokeHalfSpace& halfSpace : halfSpaces )
    {
        if ( !finiteVector(halfSpace.normal) || !finiteVector(halfSpace.point) )
        {
            return false;
        }
        const long double normalScale = std::max({
            std::abs(static_cast<long double>(halfSpace.normal.x)),
            std::abs(static_cast<long double>(halfSpace.normal.y)),
            std::abs(static_cast<long double>(halfSpace.normal.z)),
        });
        if ( normalScale <= 0.0L || !std::isfinite(normalScale) )
        {
            return false;
        }

        long double nx = static_cast<long double>(halfSpace.normal.x) / normalScale;
        long double ny = static_cast<long double>(halfSpace.normal.y) / normalScale;
        long double nz = static_cast<long double>(halfSpace.normal.z) / normalScale;
        const long double normalLength = std::sqrt(nx * nx + ny * ny + nz * nz);
        if ( !std::isfinite(normalLength) || normalLength <= 0.0L )
        {
            return false;
        }
        nx /= normalLength;
        ny /= normalLength;
        nz /= normalLength;

        const long double sign = halfSpace.positiveSide ? 1.0L : -1.0L;
        const auto signedDistance = [&](const TVector& endpoint, long double& distance) noexcept
        {
            const long double x = static_cast<long double>(endpoint.x) - halfSpace.point.x;
            const long double y = static_cast<long double>(endpoint.y) - halfSpace.point.y;
            const long double z = static_cast<long double>(endpoint.z) - halfSpace.point.z;
            const long double xTerm = nx * x;
            const long double yTerm = ny * y;
            const long double zTerm = nz * z;
            distance = sign * (xTerm + yTerm + zTerm);
            if ( !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
                || !std::isfinite(distance) )
            {
                return false;
            }
            const long double distanceRoundoff = roundoff * std::abs(xTerm)
                + roundoff * std::abs(yTerm)
                + roundoff * std::abs(zTerm);
            if ( std::abs(distance) <= distanceRoundoff )
            {
                distance = 0.0L;
            }
            return true;
        };
        long double firstDistance{};
        long double secondDistance{};
        if ( !signedDistance(stroke.first, firstDistance)
            || !signedDistance(stroke.second, secondDistance) )
        {
            return false;
        }

        if ( firstDistance < 0.0L && secondDistance < 0.0L )
        {
            return false;
        }
        if ( firstDistance < 0.0L || secondDistance < 0.0L )
        {
            const long double distanceScale = std::max(
                std::abs(firstDistance), std::abs(secondDistance));
            if ( distanceScale <= 0.0L || !std::isfinite(distanceScale) )
            {
                return false;
            }
            const CrossingFraction crossing{
                std::abs(firstDistance) / distanceScale,
                std::abs(secondDistance) / distanceScale,
            };
            if ( firstDistance < 0.0L )
            {
                if ( fractionLess(lowerBound, crossing) )
                {
                    lowerBound = crossing;
                }
            }
            else if ( fractionLess(crossing, upperBound) )
            {
                upperBound = crossing;
            }
        }
        if ( !fractionLess(lowerBound, upperBound) )
        {
            return false;
        }
    }

    return fractionLess(lowerBound, upperBound);
}

} // namespace sed::level
