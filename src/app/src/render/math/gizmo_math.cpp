#include "render/math/gizmo_math.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "sed/math_utils.hpp"
#include "sed/level/geometry.hpp"

namespace sed::render
{
namespace
{

std::optional<TVector> surfaceVertexPoint(
    const Sector& sector,
    const Surface& surface,
    const std::size_t surfaceVertexIndex) noexcept
{
    if (surfaceVertexIndex >= surface.vertices.size())
    {
        return std::nullopt;
    }

    const std::int32_t vertexIndex = surface.vertices[surfaceVertexIndex];
    if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
    {
        return std::nullopt;
    }

    const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) return std::nullopt;
    return TVector{vertex.x, vertex.y, vertex.z};
}

} // namespace

void normalizeBasis(QVector3D& x, QVector3D& y, QVector3D& z) noexcept
{
    if (x.lengthSquared() > sed::math::kTinyEpsilonF) { x.normalize(); }
    if (y.lengthSquared() > sed::math::kTinyEpsilonF) { y.normalize(); }
    if (z.lengthSquared() > sed::math::kTinyEpsilonF) { z.normalize(); }
}

bool basisIsUsable(const QVector3D& x, const QVector3D& y, const QVector3D& z) noexcept
{
    return x.lengthSquared() > sed::math::kTinyEpsilonF
        && y.lengthSquared() > sed::math::kTinyEpsilonF
        && z.lengthSquared() > sed::math::kTinyEpsilonF;
}

void addPointToBounds(
    QVector3D& min,
    QVector3D& max,
    bool& haveBounds,
    const float x,
    const float y,
    const float z) noexcept
{
    if (!haveBounds)
    {
        min = QVector3D(x, y, z);
        max = QVector3D(x, y, z);
        haveBounds = true;
        return;
    }

    min.setX(std::min(min.x(), x));
    min.setY(std::min(min.y(), y));
    min.setZ(std::min(min.z(), z));
    max.setX(std::max(max.x(), x));
    max.setY(std::max(max.y(), y));
    max.setZ(std::max(max.z(), z));
}

bool computeSurfaceLocalBasis(
    const Level& level,
    const SurfaceRef ref,
    QVector3D& outX,
    QVector3D& outY,
    QVector3D& outZ) noexcept
{
    if (ref.sector < 0 || ref.surface < 0)
    {
        return false;
    }

    const std::size_t sectorIndex = static_cast<std::size_t>(ref.sector);
    if (sectorIndex >= level.sectors.size())
    {
        return false;
    }

    const Sector& sector = level.sectors[sectorIndex];
    const std::size_t surfaceIndex = static_cast<std::size_t>(ref.surface);
    if (surfaceIndex >= sector.surfaces.size())
    {
        return false;
    }

    const Surface& surface = sector.surfaces[surfaceIndex];
    if (surface.vertices.size() < 3)
    {
        return false;
    }

    const auto p0 = surfaceVertexPoint(sector, surface, 0);
    if (!p0) return false;
    const auto normalize = [](TVector& value)
    {
        const double length = std::hypot(value.x, value.y, value.z);
        if (!std::isfinite(length) || length == 0.0) return false;
        value = TVector{value.x / length, value.y / length, value.z / length};
        return true;
    };
    TVector normal = level::surfaceCalcNormalNewell(sector, surface);
    if (!normalize(normal)) return false;
    TVector tangent{};
    bool haveTangent = false;
    for (std::size_t index = 1; index < surface.vertices.size(); ++index)
    {
        const auto point = surfaceVertexPoint(sector, surface, index);
        if (!point) return false;
        if (!haveTangent)
        {
            tangent = *point - *p0;
            tangent = tangent - normal * tangent.Dot(normal);
            haveTangent = normalize(tangent);
        }
    }
    if (!haveTangent) return false;
    TVector bitangent = normal.cross(tangent);
    if (!normalize(bitangent)) return false;
    outX = QVector3D(static_cast<float>(tangent.x), static_cast<float>(tangent.y), static_cast<float>(tangent.z));
    outY = QVector3D(static_cast<float>(bitangent.x), static_cast<float>(bitangent.y), static_cast<float>(bitangent.z));
    outZ = QVector3D(static_cast<float>(normal.x), static_cast<float>(normal.y), static_cast<float>(normal.z));
    return true;
}

} // namespace sed::render
