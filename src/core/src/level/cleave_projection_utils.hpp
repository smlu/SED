#pragma once

#include "geometry_precision.hpp"

#include <vector>

namespace sed::level::detail
{

struct OriginalCornerProjection
{
    TVector original;
    TVector projected;
    TVector normal;
    TVector firstPlane;
    TVector secondPlane;
    double sine{};
    double minimumEdge{};
    bool valid{};

    bool matches(const TVector& point, const TVector& coordinateError) const noexcept
    {
        if ( !valid || !precision::finite(point) || !precision::finite(coordinateError)
            || coordinateError.x < 0 || coordinateError.y < 0 || coordinateError.z < 0 )
        {
            return false;
        }
        const auto allowance = [&](const TVector& plane, const TVector& anchor)
        {
            return std::abs(plane.x) * coordinateError.x + std::abs(plane.y) * coordinateError.y
                + std::abs(plane.z) * coordinateError.z + precision::planeError(plane, point, anchor);
        };
        const double firstError = allowance(firstPlane, original);
        const double secondError = allowance(secondPlane, original);
        // Invert the two incident line equations only within the original
        // corner's local neighborhood. Near-parallel lines can amplify the
        // construction error; they cannot justify an unbounded snap distance.
        const double displacement = (firstError + secondError) / sine;
        const auto delta = point - projected;
        return std::isfinite(displacement) && displacement < minimumEdge * 0.5
            && std::hypot(delta.x, delta.y, delta.z) <= displacement
            && std::abs(firstPlane.dot(point - original)) <= firstError
            && std::abs(secondPlane.dot(point - original)) <= secondError
            && std::abs(normal.dot(delta)) <= allowance(normal, projected);
    }
};

inline OriginalCornerProjection originalCornerProjection(const TVector& previous,
    const TVector& corner, const TVector& next, const TVector& normal, const TVector& anchor) noexcept
{
    OriginalCornerProjection result;
    if ( !precision::finite(previous) || !precision::finite(corner) || !precision::finite(next)
        || !precision::finite(normal) || !precision::finite(anchor) )
    {
        return result;
    }
    const double normalLength = std::hypot(normal.x, normal.y, normal.z);
    if ( !std::isfinite(normalLength) || std::abs(normalLength - 1.0) > precision::kRoundoff )
    {
        return result;
    }
    const TVector first = (corner - previous).cross(normal);
    const TVector second = (next - corner).cross(normal);
    const double firstLength = std::hypot(first.x, first.y, first.z);
    const double secondLength = std::hypot(second.x, second.y, second.z);
    if ( !(firstLength > 0.0) || !(secondLength > 0.0)
        || !std::isfinite(firstLength) || !std::isfinite(secondLength) )
    {
        return result;
    }
    result.original = corner;
    result.projected = corner - normal * normal.dot(corner - anchor);
    result.normal = normal;
    result.firstPlane = {first.x / firstLength, first.y / firstLength, first.z / firstLength};
    result.secondPlane = {second.x / secondLength, second.y / secondLength, second.z / secondLength};
    result.sine = std::abs(result.firstPlane.cross(result.secondPlane).dot(normal));
    result.minimumEdge = std::min(firstLength, secondLength);
    result.valid = precision::finite(result.projected) && result.sine > 0.0 && std::isfinite(result.sine);
    return result;
}

enum class OriginalCornerMatchKind
{
    None,
    Unique,
    Ambiguous
};

struct OriginalCornerMatch
{
    OriginalCornerMatchKind kind{OriginalCornerMatchKind::None};
    std::size_t index{};
};

inline OriginalCornerMatch matchOriginalCornerProjections(const TVector& point,
    const std::vector<OriginalCornerProjection>& certificates, const TVector& error) noexcept
{
    OriginalCornerMatch result;
    for ( std::size_t index = 0; index < certificates.size(); ++index )
    {
        if ( !certificates[index].matches(point, error) )
        {
            continue;
        }
        if ( result.kind == OriginalCornerMatchKind::Unique )
        {
            const auto& first = certificates[result.index].original;
            const auto& second = certificates[index].original;
            if ( first.x != second.x || first.y != second.y || first.z != second.z )
            {
                return {OriginalCornerMatchKind::Ambiguous, 0};
            }
            continue;
        }
        result = {OriginalCornerMatchKind::Unique, index};
    }
    return result;
}

} // namespace sed::level::detail
