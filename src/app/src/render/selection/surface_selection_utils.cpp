#include "render/selection/surface_selection_utils.hpp"

#include "sed/level/geometry.hpp"
#include "sed/math_utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace sed::render
{
namespace
{

constexpr float kSurfaceFacingDotEpsilon = 1.0e-6f;

bool validVertexIndex(const Sector& sector, const std::int32_t index)
{
    return index >= 0 && static_cast<std::size_t>(index) < sector.vertices.size();
}

QVector3D vertexToVector(const Vertex& vertex)
{
    return QVector3D(
        static_cast<float>(vertex.x),
        static_cast<float>(vertex.y),
        static_cast<float>(vertex.z));
}

bool validSurfaceRef(const Level& level, const SurfaceRef ref) noexcept
{
    if (ref.sector < 0 || ref.surface < 0
        || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
    {
        return false;
    }

    return static_cast<std::size_t>(ref.surface)
        < level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size();
}

bool isFiniteVector(const QVector3D& value) noexcept
{
    return std::isfinite(value.x())
        && std::isfinite(value.y())
        && std::isfinite(value.z());
}

} // namespace

float clampedSurfaceNormalLengthScale(const float value) noexcept
{
    return std::clamp(
        std::isfinite(value) ? value : 1.0f,
        kSurfaceNormalLengthScaleMin,
        kSurfaceNormalLengthScaleMax);
}

float clampedSurfaceNormalArrowheadScale(const float value) noexcept
{
    return std::clamp(
        std::isfinite(value) ? value : 1.0f,
        kSurfaceNormalArrowheadScaleMin,
        kSurfaceNormalArrowheadScaleMax);
}

std::optional<QVector3D> surfaceFaceCenter(
    const Sector& sector,
    const Surface& surface) noexcept
{
    TVector center{};
    if (!sed::level::surfaceCalcCenter(sector, surface, center))
    {
        return std::nullopt;
    }
    const QVector3D result(
        static_cast<float>(center.x),
        static_cast<float>(center.y),
        static_cast<float>(center.z));
    return isFiniteVector(result) ? std::optional<QVector3D>(result)
                                  : std::nullopt;
}

std::optional<QVector3D> surfaceDisplayNormal(
    const Sector& sector,
    const Surface& surface) noexcept
{
    auto toVector = [](const TVector& value)
    {
        return QVector3D(static_cast<float>(value.x),
            static_cast<float>(value.y), static_cast<float>(value.z));
    };
    QVector3D normal = toVector(surface.normal);
    if (!isFiniteVector(normal)
        || normal.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        normal = toVector(sed::level::surfaceCalcNormalNewell(sector, surface));
    }
    if (!isFiniteVector(normal)
        || normal.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return std::nullopt;
    }
    normal.normalize();
    return normal;
}

std::optional<QVector3D> surfaceNormalArrowheadSide(
    QVector3D surfaceNormal,
    QVector3D directionToCamera,
    QVector3D cameraRight,
    QVector3D cameraUp) noexcept
{
    if (!isFiniteVector(surfaceNormal)
        || !isFiniteVector(directionToCamera)
        || !isFiniteVector(cameraRight)
        || !isFiniteVector(cameraUp)
        || surfaceNormal.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return std::nullopt;
    }
    surfaceNormal.normalize();

    QVector3D side{};
    if (directionToCamera.lengthSquared() > sed::math::kTinyEpsilonF)
    {
        directionToCamera.normalize();
        side = QVector3D::crossProduct(directionToCamera, surfaceNormal);
    }
    if (side.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        side = cameraRight
            - surfaceNormal
                * QVector3D::dotProduct(cameraRight, surfaceNormal);
    }
    if (side.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        side = cameraUp
            - surfaceNormal * QVector3D::dotProduct(cameraUp, surfaceNormal);
    }
    if (!isFiniteVector(side)
        || side.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return std::nullopt;
    }
    side.normalize();
    return side;
}

std::optional<QVector3D> surfaceSelectionPivot(
    const Level& level,
    const std::span<const SurfaceRef> selection,
    const std::optional<SurfaceRef>& activeSurface,
    const SurfaceSelectionPivotMode mode,
    const std::optional<SurfaceRef>& hitSurface,
    const std::optional<QVector3D>& hitPosition) noexcept
{
    const auto pivot = surfaceSelectionDocumentPivot(level, selection, activeSurface, mode, hitSurface,
        hitPosition ? std::optional<TVector>{TVector{hitPosition->x(), hitPosition->y(), hitPosition->z()}} : std::nullopt);
    if (!pivot) return std::nullopt;
    const QVector3D result(static_cast<float>(pivot->x), static_cast<float>(pivot->y), static_cast<float>(pivot->z));
    return isFiniteVector(result) ? std::optional<QVector3D>{result} : std::nullopt;
}

std::optional<TVector> surfaceSelectionDocumentPivot(
    const Level& level,
    const std::span<const SurfaceRef> selection,
    const std::optional<SurfaceRef>& activeSurface,
    const SurfaceSelectionPivotMode mode,
    const std::optional<SurfaceRef>& hitSurface,
    const std::optional<TVector>& hitPosition) noexcept
{
    const auto isFiniteVector = [](const TVector& point)
    {
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    };
    if (mode == SurfaceSelectionPivotMode::Face)
    {
        std::optional<SurfaceRef> onlyFace{};
        for (const SurfaceRef ref : selection)
        {
            if (!validSurfaceRef(level, ref))
            {
                continue;
            }
            if (onlyFace && *onlyFace != ref)
            {
                onlyFace.reset();
                break;
            }
            onlyFace = ref;
        }
        if (onlyFace)
        {
            const Sector& sector =
                level.sectors[static_cast<std::size_t>(onlyFace->sector)];
            const Surface& surface = sector.surfaces[
                static_cast<std::size_t>(onlyFace->surface)];
            TVector center{};
            return level::surfaceCalcCenter(sector, surface, center)
                ? std::optional<TVector>{center} : std::nullopt;
        }
    }

    TVector minPoint{};
    TVector maxPoint{};
    bool haveBounds = false;

    for (const SurfaceRef ref : selection)
    {
        if (!validSurfaceRef(level, ref))
        {
            continue;
        }

        const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
        for (const std::int32_t vertexIndex : surface.vertices)
        {
            if (!validVertexIndex(sector, vertexIndex))
            {
                continue;
            }

            const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
            if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z))
            {
                continue;
            }

            const TVector point = TVector{vertex.x, vertex.y, vertex.z};
            if (!haveBounds)
            {
                minPoint = point;
                maxPoint = point;
                haveBounds = true;
            }
            else
            {
                minPoint.x = std::min(minPoint.x, point.x);
                minPoint.y = std::min(minPoint.y, point.y);
                minPoint.z = std::min(minPoint.z, point.z);
                maxPoint.x = std::max(maxPoint.x, point.x);
                maxPoint.y = std::max(maxPoint.y, point.y);
                maxPoint.z = std::max(maxPoint.z, point.z);
            }
        }
    }

    if (!haveBounds)
    {
        return std::nullopt;
    }

    TVector pivot = minPoint + (maxPoint - minPoint) * 0.5;
    if ((mode != SurfaceSelectionPivotMode::Edge
            && mode != SurfaceSelectionPivotMode::Vertex)
        || !activeSurface
        || !hitSurface
        || *activeSurface != *hitSurface
        || !hitPosition
        || !std::isfinite(hitPosition->x)
        || !std::isfinite(hitPosition->y)
        || !std::isfinite(hitPosition->z)
        || !validSurfaceRef(level, *activeSurface))
    {
        return pivot;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(activeSurface->sector)];
    const Surface& surface = sector.surfaces[static_cast<std::size_t>(activeSurface->surface)];
    const TVector hit = *hitPosition;

    if (mode == SurfaceSelectionPivotMode::Vertex)
    {
        double bestDistanceSquared = std::numeric_limits<double>::infinity();
        std::optional<TVector> best{};
        for (const std::int32_t vertexIndex : surface.vertices)
        {
            if (!validVertexIndex(sector, vertexIndex))
            {
                continue;
            }
            const TVector point = TVector{sector.vertices[static_cast<std::size_t>(vertexIndex)].x, sector.vertices[static_cast<std::size_t>(vertexIndex)].y, sector.vertices[static_cast<std::size_t>(vertexIndex)].z};
            if (!isFiniteVector(point))
            {
                continue;
            }
            const double distanceSquared = point.DistanceSqr(hit);
            if (distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                best = point;
            }
        }
        if (best)
        {
            pivot = *best;
        }
        return pivot;
    }

    if (surface.vertices.size() < 2)
    {
        return pivot;
    }

    double bestDistanceSquared = std::numeric_limits<double>::infinity();
    std::optional<TVector> bestMidpoint{};
    for (std::size_t index = 0; index < surface.vertices.size(); ++index)
    {
        const std::int32_t aIndex = surface.vertices[index];
        const std::int32_t bIndex = surface.vertices[(index + 1) % surface.vertices.size()];
        if (!validVertexIndex(sector, aIndex) || !validVertexIndex(sector, bIndex))
        {
            continue;
        }

        const TVector a = TVector{sector.vertices[static_cast<std::size_t>(aIndex)].x, sector.vertices[static_cast<std::size_t>(aIndex)].y, sector.vertices[static_cast<std::size_t>(aIndex)].z};
        const TVector b = TVector{sector.vertices[static_cast<std::size_t>(bIndex)].x, sector.vertices[static_cast<std::size_t>(bIndex)].y, sector.vertices[static_cast<std::size_t>(bIndex)].z};
        if (!isFiniteVector(a) || !isFiniteVector(b))
        {
            continue;
        }

        const TVector edge = b - a;
        const double edgeLengthSquared = edge.Dot(edge);
        if (!std::isfinite(edgeLengthSquared)
            || edgeLengthSquared == 0.0)
        {
            continue;
        }

        const double t = std::clamp(
            (hit - a).Dot(edge) / edgeLengthSquared,
            0.0,
            1.0);
        const TVector closest = a + edge * t;
        const double distanceSquared = static_cast<double>(hit.DistanceSqr(closest));
        if (distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            bestMidpoint = a + (b - a) * 0.5;
        }
    }
    if (bestMidpoint)
    {
        pivot = *bestMidpoint;
    }
    return pivot;
}

bool projectWorldPointToScreen(
    const QVector3D& point,
    const QMatrix4x4& mvp,
    const int viewportWidth,
    const int viewportHeight,
    QPointF& outPos)
{
    const QVector4D clip = mvp * QVector4D(point, 1.0f);
    if (qFuzzyIsNull(clip.w()) || clip.w() <= 0.0f)
    {
        return false;
    }

    const QVector3D ndc(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());
    const float sx = (ndc.x() * 0.5f + 0.5f) * static_cast<float>(viewportWidth);
    const float sy = (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(viewportHeight);
    outPos = QPointF(sx, sy);
    return true;
}

std::vector<QPointF> projectSurfacePolygonToScreen(
    const Sector& sector,
    const Surface& surface,
    const QMatrix4x4& mvp,
    const int viewportWidth,
    const int viewportHeight)
{
    std::vector<QPointF> polygon{};
    polygon.reserve(surface.vertices.size());
    for (const std::int32_t vertexIndex : surface.vertices)
    {
        if (!validVertexIndex(sector, vertexIndex))
        {
            continue;
        }

        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z))
        {
            continue;
        }

        QPointF screenPoint{};
        if (!projectWorldPointToScreen(vertexToVector(vertex), mvp, viewportWidth, viewportHeight, screenPoint))
        {
            continue;
        }

        polygon.push_back(screenPoint);
    }
    return polygon;
}

bool surfaceFacesSelectionCamera(
    const Sector& sector,
    const Surface& surface,
    const QVector3D& eye,
    const QVector3D& target,
    const bool orthographicCamera)
{
    if ((surface.faceFlags & FF_DoubleSided) != 0u)
    {
        return true;
    }

    QVector3D normal(
        static_cast<float>(surface.normal.x),
        static_cast<float>(surface.normal.y),
        static_cast<float>(surface.normal.z));

    if (normal.lengthSquared() < sed::math::kTinyEpsilonF && surface.vertices.size() >= 3)
    {
        std::array<QVector3D, 3> points{};
        int found = 0;
        for (const std::int32_t vertexIndex : surface.vertices)
        {
            if (!validVertexIndex(sector, vertexIndex))
            {
                continue;
            }

            points[static_cast<std::size_t>(found)] =
                vertexToVector(sector.vertices[static_cast<std::size_t>(vertexIndex)]);
            ++found;
            if (found == 3)
            {
                break;
            }
        }

        if (found == 3)
        {
            normal = QVector3D::crossProduct(points[1] - points[0], points[2] - points[0]);
        }
    }

    if (normal.lengthSquared() < sed::math::kTinyEpsilonF)
    {
        return true;
    }
    normal.normalize();

    QVector3D center{};
    int centerCount = 0;
    for (const std::int32_t vertexIndex : surface.vertices)
    {
        if (!validVertexIndex(sector, vertexIndex))
        {
            continue;
        }

        center += vertexToVector(sector.vertices[static_cast<std::size_t>(vertexIndex)]);
        ++centerCount;
    }
    if (centerCount <= 0)
    {
        return false;
    }
    center /= static_cast<float>(centerCount);

    QVector3D camToSurface = orthographicCamera ? (target - eye) : (center - eye);
    if (camToSurface.lengthSquared() < sed::math::kTinyEpsilonF)
    {
        return true;
    }
    camToSurface.normalize();

    return QVector3D::dotProduct(normal, camToSurface) <= kSurfaceFacingDotEpsilon;
}

} // namespace sed::render
