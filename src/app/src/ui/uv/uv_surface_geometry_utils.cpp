#include "ui/uv/uv_surface_geometry_utils.hpp"

#include "sed/level/geometry.hpp"
#include "sed/math_utils.hpp"

#include <cmath>

namespace sed::ui::uv
{
namespace
{

bool sameUvWorldPoint(const TVector& lhs, const TVector& rhs)
{
    return (lhs - rhs).LengthSqr() <= 1.0e-8;
}

const Sector* sectorForRef(const Level& level, const SurfaceRef ref)
{
    if (ref.sector < 0 || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
    {
        return nullptr;
    }

    return &level.sectors[static_cast<std::size_t>(ref.sector)];
}

const Surface* surfaceForRef(const Level& level, const SurfaceRef ref)
{
    const Sector* sector = sectorForRef(level, ref);
    if (!sector || ref.surface < 0 || static_cast<std::size_t>(ref.surface) >= sector->surfaces.size())
    {
        return nullptr;
    }

    return &sector->surfaces[static_cast<std::size_t>(ref.surface)];
}

} // namespace

bool normalizeUvVector(QVector3D& value)
{
    if (!std::isfinite(value.x()) || !std::isfinite(value.y()) || !std::isfinite(value.z()))
    {
        return false;
    }

    const double length = std::hypot(static_cast<double>(value.x()),
        static_cast<double>(value.y()), static_cast<double>(value.z()));
    if (length <= std::sqrt(static_cast<double>(sed::math::kTinyEpsilonF)))
    {
        return false;
    }

    value = QVector3D(static_cast<float>(value.x() / length),
        static_cast<float>(value.y() / length), static_cast<float>(value.z() / length));
    return true;
}

std::optional<QVector3D> uvSurfaceVertexWorld(const Level& level, const SurfaceRef ref, const std::size_t localIndex)
{
    const auto vertex = uvSurfaceVertexPosition(level, ref, localIndex);
    if (!vertex)
    {
        return std::nullopt;
    }
    const QVector3D point(static_cast<float>(vertex->x), static_cast<float>(vertex->y), static_cast<float>(vertex->z));
    return std::isfinite(point.x()) && std::isfinite(point.y()) && std::isfinite(point.z())
        ? std::optional<QVector3D>{point} : std::nullopt;
}

std::optional<TVector> uvSurfaceVertexPosition(const Level& level, const SurfaceRef ref, const std::size_t localIndex)
{
    const Sector* sector = sectorForRef(level, ref);
    const Surface* surface = surfaceForRef(level, ref);
    if (!sector || !surface || localIndex >= surface->vertices.size())
    {
        return std::nullopt;
    }

    const std::int32_t vi = surface->vertices[localIndex];
    if (vi < 0 || static_cast<std::size_t>(vi) >= sector->vertices.size())
    {
        return std::nullopt;
    }

    const Vertex& vertex = sector->vertices[static_cast<std::size_t>(vi)];
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z))
    {
        return std::nullopt;
    }
    return TVector{vertex.x, vertex.y, vertex.z};
}

std::vector<std::pair<std::size_t, std::size_t>> uvSharedVertexPairs(const Level& level, const SurfaceRef a, const SurfaceRef b)
{
    std::vector<std::pair<std::size_t, std::size_t>> pairs{};
    const Surface* surfA = surfaceForRef(level, a);
    const Surface* surfB = surfaceForRef(level, b);
    if (!surfA || !surfB)
    {
        return pairs;
    }

    for (std::size_t i = 0; i < surfA->vertices.size(); ++i)
    {
        const std::optional<TVector> pa = uvSurfaceVertexPosition(level, a, i);
        if (!pa)
        {
            continue;
        }

        for (std::size_t j = 0; j < surfB->vertices.size(); ++j)
        {
            const std::optional<TVector> pb = uvSurfaceVertexPosition(level, b, j);
            if (pb && sameUvWorldPoint(*pa, *pb))
            {
                pairs.emplace_back(i, j);
            }
        }
    }

    return pairs;
}

QVector3D uvSurfaceNormal(const Level& level, const SurfaceRef ref)
{
    const Sector* sector = sectorForRef(level, ref);
    const Surface* surface = surfaceForRef(level, ref);
    if (!sector || !surface)
    {
        return QVector3D(0.0f, 0.0f, 1.0f);
    }

    TVector normal = surface->normal;
    if (normal.Length() == 0.0)
    {
        normal = ::sed::level::surfaceCalcNormalNewell(*sector, *surface);
    }

    QVector3D out(static_cast<float>(normal.x), static_cast<float>(normal.y), static_cast<float>(normal.z));
    if (!normalizeUvVector(out))
    {
        out = QVector3D(0.0f, 0.0f, 1.0f);
    }
    return out;
}

bool uvSurfacesShareEdge(const Level& level, const SurfaceRef a, const SurfaceRef b)
{
    const Surface* surfA = surfaceForRef(level, a);
    const Surface* surfB = surfaceForRef(level, b);
    if (!surfA || !surfB || surfA->vertices.size() < 2 || surfB->vertices.size() < 2)
    {
        return false;
    }

    for (std::size_t i = 0; i < surfA->vertices.size(); ++i)
    {
        const std::optional<TVector> a0 = uvSurfaceVertexPosition(level, a, i);
        const std::optional<TVector> a1 = uvSurfaceVertexPosition(level, a, (i + 1u) % surfA->vertices.size());
        if (!a0 || !a1)
        {
            continue;
        }

        for (std::size_t j = 0; j < surfB->vertices.size(); ++j)
        {
            const std::optional<TVector> b0 = uvSurfaceVertexPosition(level, b, j);
            const std::optional<TVector> b1 = uvSurfaceVertexPosition(level, b, (j + 1u) % surfB->vertices.size());
            if (!b0 || !b1)
            {
                continue;
            }

            if ((sameUvWorldPoint(*a0, *b1) && sameUvWorldPoint(*a1, *b0))
                || (sameUvWorldPoint(*a0, *b0) && sameUvWorldPoint(*a1, *b1)))
            {
                return true;
            }
        }
    }

    return false;
}

std::string uvSurfaceColormapName(const Level& level, const SurfaceRef ref)
{
    if (const Sector* sector = sectorForRef(level, ref))
    {
        if (!sector->colormap.empty() && sector->colormap != "-")
        {
            return sector->colormap;
        }
    }

    return level.getMasterCMP();
}

} // namespace sed::ui::uv
