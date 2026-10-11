#pragma once

#include "sed/geometry.hpp"
#include "sed/level.hpp"
#include "render/picking/viewport_ray_utils.hpp"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace sed::render
{

inline TVector cleaveDocumentPoint(const QVector3D& point) noexcept
{
    return {point.x(), point.y(), point.z()};
}

inline double cleaveDot(const TVector& first, const TVector& second) noexcept
{
    return first.dot(second);
}

inline TVector cleaveCross(const TVector& first, const TVector& second) noexcept
{
    return first.cross(second);
}

inline QVector3D cleaveRenderPoint(const TVector& point) noexcept
{
    return {static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z)};
}

inline bool cleaveFinitePoint(const TVector& point) noexcept
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

inline bool cleaveNormalize(TVector& vector) noexcept
{
    const double magnitude = std::max({std::abs(vector.x), std::abs(vector.y), std::abs(vector.z)});
    if (!cleaveFinitePoint(vector) || !(magnitude > 0.0))
    {
        return false;
    }
    const TVector scaled{vector.x / magnitude, vector.y / magnitude, vector.z / magnitude};
    const double length = std::sqrt(scaled.dot(scaled));
    vector = scaled * (1.0 / length);
    return true;
}

// Once a screen-space snap has selected a construction handle, its exact
// authored coordinates are shared. Proximity alone must never connect two
// independently authored handles.
inline bool cleaveSameHandle(const TVector& first, const TVector& second) noexcept
{
    return cleaveFinitePoint(first) && cleaveFinitePoint(second)
        && first.x == second.x && first.y == second.y && first.z == second.z;
}

// Independently calculated cap edges may differ by arithmetic roundoff.
// This geometric comparison never defines shared authored endpoint handles.
inline bool cleaveLineMatchesPolygonEdge(const TVector& first, const TVector& second,
    const std::span<const TVector> polygon) noexcept
{
    const auto samePoint = [](const TVector& a, const TVector& b)
    {
        constexpr double roundoff = 64.0 * std::numeric_limits<double>::epsilon();
        return cleaveFinitePoint(a) && cleaveFinitePoint(b)
            && std::abs(a.x - b.x) <= roundoff * std::max(std::abs(a.x), std::abs(b.x))
            && std::abs(a.y - b.y) <= roundoff * std::max(std::abs(a.y), std::abs(b.y))
            && std::abs(a.z - b.z) <= roundoff * std::max(std::abs(a.z), std::abs(b.z));
    };
    for (std::size_t edge = 0; edge < polygon.size(); ++edge)
    {
        const TVector& a = polygon[edge];
        const TVector& b = polygon[(edge + 1) % polygon.size()];
        if ((samePoint(first, a) && samePoint(second, b))
            || (samePoint(first, b) && samePoint(second, a))) return true;
    }
    return false;
}

inline std::optional<TVector> cleaveIntersectRayPlane(const ViewportRay& ray,
    const TVector& point, TVector normal) noexcept
{
    if (!cleaveFinitePoint(ray.origin) || !cleaveFinitePoint(ray.direction)
        || !cleaveFinitePoint(point) || !cleaveNormalize(normal))
    {
        return std::nullopt;
    }
    const double denominator = ray.direction.dot(normal);
    // Preserve the existing dimensionless edge-on acquisition cutoff.
    if (!std::isfinite(denominator) || std::abs(denominator) < 1.0e-6)
    {
        return std::nullopt;
    }
    const double distance = (point - ray.origin).dot(normal) / denominator;
    const TVector hit = ray.origin + ray.direction * distance;
    return cleaveFinitePoint(hit) ? std::optional<TVector>{hit} : std::nullopt;
}

inline std::optional<TVector> cleaveSurfaceCenter(const Sector& sector, const Surface& face)
{
    if (face.vertices.empty())
    {
        return std::nullopt;
    }
    TVector anchor{};
    TVector sum{};
    double scale = 0.0;
    for (std::size_t corner = 0; corner < face.vertices.size(); ++corner)
    {
        const auto index = face.vertices[corner];
        if (index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size())
        {
            return std::nullopt;
        }
        const auto& vertex = sector.vertices[static_cast<std::size_t>(index)];
        const TVector point{vertex.x, vertex.y, vertex.z};
        if (!cleaveFinitePoint(point))
        {
            return std::nullopt;
        }
        if (corner == 0)
        {
            anchor = point;
        }
        sum = sum + (point - anchor);
        const TVector delta = point - anchor;
        scale = std::max({scale, std::abs(delta.x), std::abs(delta.y), std::abs(delta.z)});
    }
    // Preserve the polygon area-centroid contract used by face pivots and
    // normal markers. Normalize local coordinates so a tiny face does not
    // fall back to the vertex mean merely because its world area is small.
    if (scale > 0.0 && std::isfinite(scale) && face.vertices.size() >= 3)
    {
        const auto localPoint = [&](const std::size_t corner)
        {
            const Vertex& vertex = sector.vertices[static_cast<std::size_t>(face.vertices[corner])];
            return (TVector{vertex.x, vertex.y, vertex.z} - anchor) * (1.0 / scale);
        };
        TVector normal{};
        for (std::size_t corner = 1; corner + 1 < face.vertices.size(); ++corner)
            normal = normal + localPoint(corner).cross(localPoint(corner + 1));
        if (cleaveNormalize(normal))
        {
            TVector weighted{};
            double area = 0.0;
            for (std::size_t corner = 1; corner + 1 < face.vertices.size(); ++corner)
            {
                const TVector first = localPoint(corner);
                const TVector second = localPoint(corner + 1);
                const double triangleArea = first.cross(second).dot(normal);
                weighted = weighted + (first + second) * triangleArea;
                area += triangleArea;
            }
            if (std::isfinite(area) && area != 0.0)
            {
                const TVector center = anchor + weighted * (scale / (3.0 * area));
                if (cleaveFinitePoint(center)) return center;
            }
        }
    }
    const TVector fallback = anchor + sum * (1.0 / static_cast<double>(face.vertices.size()));
    return cleaveFinitePoint(fallback) ? std::optional<TVector>{fallback} : std::nullopt;
}

// This is an arithmetic allowance in distance units, matching the core cut
// predicate. It is not a proximity radius and does not grow with unrelated
// coordinate components.
inline double cleavePlaneError(const TVector& normal, const TVector& first,
    const TVector& second) noexcept
{
    constexpr double roundoff = 64.0 * std::numeric_limits<double>::epsilon();
    return roundoff * (std::abs(normal.x) * (std::abs(first.x) + std::abs(second.x))
        + std::abs(normal.y) * (std::abs(first.y) + std::abs(second.y))
        + std::abs(normal.z) * (std::abs(first.z) + std::abs(second.z)));
}

inline std::vector<TVector> cleavePlaneSectionPolygon(const Sector& sector,
    TVector normal, const TVector& planePoint, TVector axisU)
{
    std::vector<TVector> intersections{};
    if (!cleaveFinitePoint(planePoint) || !cleaveNormalize(normal))
    {
        return intersections;
    }
    axisU = axisU - normal * axisU.dot(normal);
    if (!cleaveNormalize(axisU))
    {
        axisU = (std::abs(normal.z) < 0.9 ? TVector{0, 0, 1} : TVector{0, 1, 0}).cross(normal);
        if (!cleaveNormalize(axisU))
        {
            return intersections;
        }
    }
    TVector axisV = normal.cross(axisU);
    if (!cleaveNormalize(axisV))
    {
        return intersections;
    }

    const auto addUnique = [&](const TVector& point)
    {
        if (cleaveFinitePoint(point) && std::none_of(intersections.begin(), intersections.end(),
                [&](const TVector& existing) { return cleaveSameHandle(existing, point); }))
        {
            intersections.push_back(point);
        }
    };
    std::set<std::pair<std::int32_t, std::int32_t>> visitedEdges{};
    for (const Surface& face : sector.surfaces)
    {
        for (std::size_t edge = 0; edge < face.vertices.size(); ++edge)
        {
            auto firstIndex = face.vertices[edge];
            auto secondIndex = face.vertices[(edge + 1) % face.vertices.size()];
            if (firstIndex < 0 || secondIndex < 0
                || static_cast<std::size_t>(firstIndex) >= sector.vertices.size()
                || static_cast<std::size_t>(secondIndex) >= sector.vertices.size())
            {
                continue;
            }
            // Every shared edge uses the same directed intersection formula.
            if (secondIndex < firstIndex)
            {
                std::swap(firstIndex, secondIndex);
            }
            if (!visitedEdges.emplace(firstIndex, secondIndex).second)
            {
                continue;
            }
            const Vertex& firstVertex = sector.vertices[static_cast<std::size_t>(firstIndex)];
            const Vertex& secondVertex = sector.vertices[static_cast<std::size_t>(secondIndex)];
            const TVector first{firstVertex.x, firstVertex.y, firstVertex.z};
            const TVector second{secondVertex.x, secondVertex.y, secondVertex.z};
            const double firstDistance = normal.dot(first - planePoint);
            const double secondDistance = normal.dot(second - planePoint);
            const double firstError = cleavePlaneError(normal, first, planePoint);
            const double secondError = cleavePlaneError(normal, second, planePoint);
            if (std::abs(firstDistance) <= firstError)
            {
                addUnique(first);
            }
            if (std::abs(secondDistance) <= secondError)
            {
                addUnique(second);
            }
            if ((firstDistance < -firstError && secondDistance > secondError)
                || (firstDistance > firstError && secondDistance < -secondError))
            {
                const TVector delta = second - first;
                const double t = normal.dot(planePoint - first) / normal.dot(delta);
                addUnique(first + delta * t);
            }
        }
    }
    if (intersections.size() < 3)
    {
        return {};
    }
    const TVector anchor = intersections.front();
    TVector relativeCenter{};
    for (const TVector& point : intersections)
    {
        relativeCenter = relativeCenter + (point - anchor);
    }
    const TVector center = anchor + relativeCenter * (1.0 / static_cast<double>(intersections.size()));
    std::sort(intersections.begin(), intersections.end(), [&](const TVector& first, const TVector& second)
    {
        const TVector firstDelta = first - center;
        const TVector secondDelta = second - center;
        return std::atan2(firstDelta.dot(axisV), firstDelta.dot(axisU))
            < std::atan2(secondDelta.dot(axisV), secondDelta.dot(axisU));
    });
    return intersections;
}

} // namespace sed::render
