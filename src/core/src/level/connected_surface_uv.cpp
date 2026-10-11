#include "sed/level/transform.hpp"

#include "sed/level/uv.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sed::level
{
namespace
{

constexpr std::uint64_t vertexKey(const VertexRef ref) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32u) |
           static_cast<std::uint32_t>(ref.vertex);
}

constexpr std::uint64_t surfaceKey(const SurfaceRef ref) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32u) |
           static_cast<std::uint32_t>(ref.surface);
}

bool finite(const TVector& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool finite(const TexVertex& value) noexcept
{
    return std::isfinite(value.u) && std::isfinite(value.v) && std::isfinite(value.color.r) &&
           std::isfinite(value.color.g) && std::isfinite(value.color.b) && std::isfinite(value.color.a);
}

bool validVertex(const Level& level, const VertexRef ref) noexcept
{
    return ref.sector >= 0 && ref.vertex >= 0 && static_cast<std::size_t>(ref.sector) < level.sectors.size() &&
           static_cast<std::size_t>(ref.vertex) < level.sectors[static_cast<std::size_t>(ref.sector)].vertices.size();
}

bool validSurface(const Level& level, const SurfaceRef ref) noexcept
{
    return ref.sector >= 0 && ref.surface >= 0 && static_cast<std::size_t>(ref.sector) < level.sectors.size() &&
           static_cast<std::size_t>(ref.surface) < level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size();
}

TVector newellNormal(const std::vector<TVector>& positions) noexcept
{
    TVector normal{};
    if ( positions.size() < 3u )
    {
        return normal;
    }

    for ( std::size_t index = 0; index < positions.size(); ++index )
    {
        const TVector& current = positions[index];
        const TVector& next = positions[(index + 1u) % positions.size()];
        normal.x += (current.y - next.y) * (current.z + next.z);
        normal.y += (current.z - next.z) * (current.x + next.x);
        normal.z += (current.x - next.x) * (current.y + next.y);
    }
    if ( !finite(normal) || normal.normalize() <= 1.0e-12 )
    {
        return TVector{};
    }
    return normal;
}

bool deriveGradient(const std::vector<TVector>& positions, const std::vector<TexVertex>& texVertices,
                    const std::size_t anchor, const bool useU, TVector& gradient) noexcept
{
    if ( positions.size() != texVertices.size() || positions.size() < 3u || anchor >= positions.size() )
    {
        return false;
    }

    double bestDeterminant{};
    std::size_t bestFirst{};
    std::size_t bestSecond{};
    for ( std::size_t first = 0; first < positions.size(); ++first )
    {
        if ( first == anchor )
        {
            continue;
        }
        const TVector firstEdge = positions[first] - positions[anchor];
        for ( std::size_t second = first + 1u; second < positions.size(); ++second )
        {
            if ( second == anchor )
            {
                continue;
            }
            const TVector secondEdge = positions[second] - positions[anchor];
            const TVector normal = firstEdge.cross(secondEdge);
            const double determinant = normal.dot(normal);
            if ( std::isfinite(determinant) && determinant > bestDeterminant )
            {
                bestDeterminant = determinant;
                bestFirst = first;
                bestSecond = second;
            }
        }
    }

    if ( !(bestDeterminant > 0.0) )
    {
        return false;
    }

    const TVector firstEdge = positions[bestFirst] - positions[anchor];
    const TVector secondEdge = positions[bestSecond] - positions[anchor];
    const TVector normal = firstEdge.cross(secondEdge);
    const double anchorValue =
        useU ? static_cast<double>(texVertices[anchor].u) : static_cast<double>(texVertices[anchor].v);
    const double firstDelta =
        (useU ? static_cast<double>(texVertices[bestFirst].u) : static_cast<double>(texVertices[bestFirst].v)) -
        anchorValue;
    const double secondDelta =
        (useU ? static_cast<double>(texVertices[bestSecond].u) : static_cast<double>(texVertices[bestSecond].v)) -
        anchorValue;
    // Cross-product duals avoid subtracting nearly equal squared edge lengths.
    // Face-normal validation handles degeneracy without an additional absolute basis-area cutoff.
    gradient = secondEdge.cross(normal) * firstDelta + normal.cross(firstEdge) * secondDelta;
    gradient.x /= bestDeterminant;
    gradient.y /= bestDeterminant;
    gradient.z /= bestDeterminant;
    return finite(gradient);
}

TVector rotateBetweenNormals(const TVector& value, const TVector& sourceNormal, const TVector& targetNormal) noexcept
{
    const double cosine = std::clamp(sourceNormal.dot(targetNormal), -1.0, 1.0);
    TVector cross = sourceNormal.cross(targetNormal);
    const double sine = cross.length();
    if ( sine <= 1.0e-12 )
    {
        if ( cosine >= 0.0 )
        {
            return value;
        }

        TVector axis =
            std::abs(sourceNormal.x) <= std::abs(sourceNormal.y) && std::abs(sourceNormal.x) <= std::abs(sourceNormal.z)
                ? sourceNormal.cross(TVector::right)
            : std::abs(sourceNormal.y) <= std::abs(sourceNormal.z) ? sourceNormal.cross(TVector::forward)
                                                                   : sourceNormal.cross(TVector::up);
        if ( axis.normalize() <= 1.0e-12 )
        {
            return value;
        }
        return axis * (2.0 * axis.dot(value)) - value;
    }

    cross.scale(1.0 / sine);
    return value * cosine + cross.cross(value) * sine + cross * (cross.dot(value) * (1.0 - cosine));
}

bool representableFloat(const double value) noexcept
{
    constexpr double maximum = static_cast<double>(std::numeric_limits<float>::max());
    return std::isfinite(value) && value >= -maximum && value <= maximum;
}

bool validTransform(const ConnectedSurfaceUvTransform& transform) noexcept
{
    if ( !finite(transform.translation) || !finite(transform.rotationAxis) || !finite(transform.pivot) ||
         !std::isfinite(transform.angleDegrees) || !std::isfinite(transform.scale.cx) ||
         !std::isfinite(transform.scale.cy) || !std::isfinite(transform.scale.cz) ||
         !std::isfinite(transform.scale.sfactor) || !finite(transform.scale.vec) )
    {
        return false;
    }

    switch ( transform.kind )
    {
    case ConnectedSurfaceUvTransformKind::identity:
    case ConnectedSurfaceUvTransformKind::translate:
        return true;
    case ConnectedSurfaceUvTransformKind::scale:
        switch ( transform.scale.how )
        {
        case ScaleHow::xyz:
        case ScaleHow::x:
        case ScaleHow::y:
        case ScaleHow::z:
            return true;
        case ScaleHow::vec:
            return transform.scale.vec.lengthSqr() >
                   std::numeric_limits<double>::epsilon();
        default:
            return false;
        }
    case ConnectedSurfaceUvTransformKind::rotate:
    case ConnectedSurfaceUvTransformKind::rotateThenTranslate:
        return transform.rotationAxis.length() > 1.0e-12;
    default:
        return false;
    }
}

} // namespace

TVector transformConnectedSurfaceUvPosition(const TVector& source,
                                            const ConnectedSurfaceUvTransform& transform) noexcept
{
    TVector result = source;
    switch ( transform.kind )
    {
    case ConnectedSurfaceUvTransformKind::identity:
        break;
    case ConnectedSurfaceUvTransformKind::translate:
        result = result + transform.translation;
        break;
    case ConnectedSurfaceUvTransformKind::rotate:
    case ConnectedSurfaceUvTransformKind::rotateThenTranslate:
    {
        RotateData rotation{};
        TVector normalizedAxis = transform.rotationAxis;
        normalizedAxis.Normalize();
        prepareRotateData(rotation, normalizedAxis, transform.pivot.x, transform.pivot.y, transform.pivot.z,
                          transform.angleDegrees);
        rotateXYZ(result.x, result.y, result.z, rotation);
        if ( transform.kind == ConnectedSurfaceUvTransformKind::rotateThenTranslate )
        {
            result = result + transform.translation;
        }
        break;
    }
    case ConnectedSurfaceUvTransformKind::scale:
    {
        const ScaleData& scale = transform.scale;
        switch ( scale.how )
        {
        case ScaleHow::xyz:
            result.x = scale.cx + scale.sfactor * (result.x - scale.cx);
            result.y = scale.cy + scale.sfactor * (result.y - scale.cy);
            result.z = scale.cz + scale.sfactor * (result.z - scale.cz);
            break;
        case ScaleHow::x:
            result.x = scale.cx + scale.sfactor * (result.x - scale.cx);
            break;
        case ScaleHow::y:
            result.y = scale.cy + scale.sfactor * (result.y - scale.cy);
            break;
        case ScaleHow::z:
            result.z = scale.cz + scale.sfactor * (result.z - scale.cz);
            break;
        case ScaleHow::vec:
        {
            const TVector origin{scale.cx, scale.cy, scale.cz};
            TVector axis = scale.vec;
            axis.Normalize();
            const double distanceAlongAxis = axis.dot(result - origin);
            result = result + axis * ((scale.sfactor - 1.0) * distanceAlongAxis);
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
    return result;
}

ConnectedSurfaceUvPlan captureConnectedSurfaceUvPlan(const Level& level, const std::span<const VertexRef> movedVertices,
                                                     const bool includeFullyMovedSurfaces)
{
    ConnectedSurfaceUvPlan plan{};
    plan.coordinateSpace = level.uvCoordinateSpace;
    if ( !isValidUvCoordinateSpace(plan.coordinateSpace) )
    {
        return plan;
    }

    std::unordered_set<std::uint64_t> movedKeys{};
    movedKeys.reserve(movedVertices.size());
    for ( const VertexRef ref : movedVertices )
    {
        if ( !validVertex(level, ref) || !movedKeys.insert(vertexKey(ref)).second )
        {
            return plan;
        }
    }

    for ( std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex )
    {
        const Sector& sector = level.sectors[sectorIndex];
        for ( std::size_t vertexIndex = 0; vertexIndex < sector.vertices.size(); ++vertexIndex )
        {
            const VertexRef ref{static_cast<std::int32_t>(sectorIndex), static_cast<std::int32_t>(vertexIndex)};
            if ( movedKeys.contains(vertexKey(ref)) )
            {
                plan.movedVertices.push_back(ref);
                const Vertex& vertex = sector.vertices[vertexIndex];
                const TVector position{vertex.x, vertex.y, vertex.z};
                if ( !finite(position) )
                {
                    return ConnectedSurfaceUvPlan{};
                }
                plan.sourceMovedPositions.push_back(position);
            }
        }

        for ( std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            std::size_t movedCount{};
            bool touchesMovedVertex{};
            for ( const std::int32_t vertexIndex : surface.vertices )
            {
                const VertexRef ref{static_cast<std::int32_t>(sectorIndex), vertexIndex};
                if ( validVertex(level, ref) && movedKeys.contains(vertexKey(ref)) )
                {
                    touchesMovedVertex = true;
                    ++movedCount;
                }
            }
            if ( !touchesMovedVertex )
            {
                continue;
            }
            if ( surface.vertices.size() < 3u || surface.vertices.size() != surface.texVertices.size() )
            {
                return ConnectedSurfaceUvPlan{};
            }

            bool malformed{};
            std::vector<VertexRef> vertices{};
            std::vector<TVector> positions{};
            std::vector<bool> movedCorners{};
            std::unordered_set<std::uint64_t> surfaceVertexKeys{};
            vertices.reserve(surface.vertices.size());
            positions.reserve(surface.vertices.size());
            movedCorners.reserve(surface.vertices.size());
            surfaceVertexKeys.reserve(surface.vertices.size());
            for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
            {
                const VertexRef ref{static_cast<std::int32_t>(sectorIndex), surface.vertices[corner]};
                if ( !validVertex(level, ref) || !surfaceVertexKeys.insert(vertexKey(ref)).second ||
                     !finite(surface.texVertices[corner]) )
                {
                    malformed = true;
                    break;
                }
                const Vertex& vertex = sector.vertices[static_cast<std::size_t>(ref.vertex)];
                const TVector position{vertex.x, vertex.y, vertex.z};
                if ( !finite(position) )
                {
                    malformed = true;
                    break;
                }
                vertices.push_back(ref);
                positions.push_back(position);
                const bool moved = movedKeys.contains(vertexKey(ref));
                movedCorners.push_back(moved);
            }
            if ( malformed )
            {
                return ConnectedSurfaceUvPlan{};
            }
            if ( !includeFullyMovedSurfaces && movedCount == surface.vertices.size() )
            {
                continue;
            }

            std::size_t anchor{};
            if ( movedCount != surface.vertices.size() )
            {
                const auto unmoved = std::ranges::find_if(vertices,
                                                          [&movedKeys](const VertexRef ref)
                                                          {
                                                              return !movedKeys.contains(vertexKey(ref));
                                                          });
                if ( unmoved != vertices.end() )
                {
                    anchor = static_cast<std::size_t>(std::distance(vertices.begin(), unmoved));
                }
            }

            ConnectedSurfaceUvPlanSurface entry{};
            entry.surface = SurfaceRef{static_cast<std::int32_t>(sectorIndex), static_cast<std::int32_t>(surfaceIndex)};
            entry.vertices = std::move(vertices);
            entry.sourcePositions = std::move(positions);
            entry.sourceTexVertices = surface.texVertices;
            entry.movedCorners = std::move(movedCorners);
            entry.anchorCorner = anchor;
            entry.sourceNormal = newellNormal(entry.sourcePositions);
            if ( entry.sourceNormal.isZero() ||
                 !deriveGradient(entry.sourcePositions, entry.sourceTexVertices, anchor, true, entry.uGradient) ||
                 !deriveGradient(entry.sourcePositions, entry.sourceTexVertices, anchor, false, entry.vGradient) )
            {
                return ConnectedSurfaceUvPlan{};
            }
            plan.surfaces.push_back(std::move(entry));
        }
    }
    plan.valid = true;
    return plan;
}

ConnectedSurfaceUvEvaluation
evaluateConnectedSurfaceUvPlan(const ConnectedSurfaceUvPlan& plan,
                               const std::span<const ConnectedSurfaceUvVertexPosition> transformedVertices)
{
    ConnectedSurfaceUvEvaluation result{};
    result.coordinateSpace = plan.coordinateSpace;
    if ( !isValidUvCoordinateSpace(plan.coordinateSpace) || !plan.valid ||
         transformedVertices.size() != plan.movedVertices.size() ||
         plan.sourceMovedPositions.size() != plan.movedVertices.size() )
    {
        return result;
    }

    std::unordered_map<std::uint64_t, TVector> transformed{};
    transformed.reserve(transformedVertices.size());
    std::unordered_set<std::uint64_t> expected{};
    expected.reserve(plan.movedVertices.size());
    for ( const VertexRef ref : plan.movedVertices )
    {
        if ( ref.sector < 0 || ref.vertex < 0 || !expected.insert(vertexKey(ref)).second )
        {
            return result;
        }
    }
    for ( const ConnectedSurfaceUvVertexPosition& entry : transformedVertices )
    {
        const std::uint64_t key = vertexKey(entry.vertex);
        if ( entry.vertex.sector < 0 || entry.vertex.vertex < 0 || !expected.contains(key) || !finite(entry.position) ||
             !transformed.emplace(key, entry.position).second )
        {
            return result;
        }
    }
    for ( const VertexRef ref : plan.movedVertices )
    {
        if ( !transformed.contains(vertexKey(ref)) )
        {
            return result;
        }
    }

    result.surfaces.reserve(plan.surfaces.size());
    for ( const ConnectedSurfaceUvPlanSurface& entry : plan.surfaces )
    {
        if ( entry.vertices.size() < 3u || entry.vertices.size() != entry.sourcePositions.size() ||
             entry.vertices.size() != entry.sourceTexVertices.size() ||
             entry.vertices.size() != entry.movedCorners.size() || entry.anchorCorner >= entry.vertices.size() ||
             !finite(entry.sourceNormal) || !finite(entry.uGradient) || !finite(entry.vGradient) )
        {
            result.surfaces.clear();
            return result;
        }

        std::vector<TVector> positions = entry.sourcePositions;
        for ( std::size_t corner = 0; corner < entry.vertices.size(); ++corner )
        {
            const auto moved = transformed.find(vertexKey(entry.vertices[corner]));
            if ( moved != transformed.end() )
            {
                positions[corner] = moved->second;
            }
        }

        if ( std::ranges::equal(positions, entry.sourcePositions,
                                [](const TVector& lhs, const TVector& rhs)
                                {
                                    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
                                }) )
        {
            ConnectedSurfaceUvSurface surfaceResult{};
            surfaceResult.surface = entry.surface;
            surfaceResult.texVertices = entry.sourceTexVertices;
            surfaceResult.vertices.reserve(entry.vertices.size());
            for ( const VertexRef ref : entry.vertices )
            {
                if ( ref.sector != entry.surface.sector )
                {
                    result.surfaces.clear();
                    return result;
                }
                surfaceResult.vertices.push_back(ref.vertex);
            }
            result.surfaces.push_back(std::move(surfaceResult));
            continue;
        }

        const TVector targetNormal = newellNormal(positions);
        if ( targetNormal.isZero() )
        {
            result.surfaces.clear();
            return result;
        }
        const TVector uGradient = rotateBetweenNormals(entry.uGradient, entry.sourceNormal, targetNormal);
        const TVector vGradient = rotateBetweenNormals(entry.vGradient, entry.sourceNormal, targetNormal);
        if ( !finite(uGradient) || !finite(vGradient) )
        {
            result.surfaces.clear();
            return result;
        }

        ConnectedSurfaceUvSurface surfaceResult{};
        surfaceResult.surface = entry.surface;
        surfaceResult.vertices.reserve(entry.vertices.size());
        for ( const VertexRef ref : entry.vertices )
        {
            if ( ref.sector != entry.surface.sector )
            {
                result.surfaces.clear();
                return result;
            }
            surfaceResult.vertices.push_back(ref.vertex);
        }
        surfaceResult.texVertices = entry.sourceTexVertices;
        const TVector& sourceAnchorPosition = entry.sourcePositions[entry.anchorCorner];
        const TVector& targetAnchorPosition = positions[entry.anchorCorner];
        for ( std::size_t corner = 0; corner < positions.size(); ++corner )
        {
            if ( !entry.movedCorners[corner] )
            {
                continue;
            }
            const TVector sourceOffset = entry.sourcePositions[corner] - sourceAnchorPosition;
            const TVector targetOffset = positions[corner] - targetAnchorPosition;
            const double u = static_cast<double>(entry.sourceTexVertices[corner].u) + uGradient.dot(targetOffset) -
                             entry.uGradient.dot(sourceOffset);
            const double v = static_cast<double>(entry.sourceTexVertices[corner].v) + vGradient.dot(targetOffset) -
                             entry.vGradient.dot(sourceOffset);
            if ( !representableFloat(u) || !representableFloat(v) )
            {
                result.surfaces.clear();
                return result;
            }
            surfaceResult.texVertices[corner].u = static_cast<float>(u);
            surfaceResult.texVertices[corner].v = static_cast<float>(v);
        }
        result.surfaces.push_back(std::move(surfaceResult));
    }

    result.valid = true;
    return result;
}

ConnectedSurfaceUvEvaluation evaluateConnectedSurfaceUvPlan(const ConnectedSurfaceUvPlan& plan,
                                                            const Level& transformedLevel)
{
    ConnectedSurfaceUvEvaluation result{};
    result.coordinateSpace = plan.coordinateSpace;
    if ( !isValidUvCoordinateSpace(plan.coordinateSpace) ||
         !isValidUvCoordinateSpace(transformedLevel.uvCoordinateSpace) || !plan.valid ||
         transformedLevel.uvCoordinateSpace != plan.coordinateSpace )
    {
        return result;
    }

    std::vector<ConnectedSurfaceUvVertexPosition> positions{};
    positions.reserve(plan.movedVertices.size());
    for ( const VertexRef ref : plan.movedVertices )
    {
        if ( !validVertex(transformedLevel, ref) )
        {
            return result;
        }
        const Vertex& vertex = transformedLevel.sectors[static_cast<std::size_t>(ref.sector)]
                                   .vertices[static_cast<std::size_t>(ref.vertex)];
        positions.push_back(ConnectedSurfaceUvVertexPosition{ref, TVector{vertex.x, vertex.y, vertex.z}});
    }
    return evaluateConnectedSurfaceUvPlan(plan, positions);
}

ConnectedSurfaceUvEvaluation evaluateConnectedSurfaceUvPlan(const ConnectedSurfaceUvPlan& plan,
                                                            const ConnectedSurfaceUvTransform& transform)
{
    ConnectedSurfaceUvEvaluation result{};
    result.coordinateSpace = plan.coordinateSpace;
    if ( !isValidUvCoordinateSpace(plan.coordinateSpace) || !plan.valid ||
         plan.sourceMovedPositions.size() != plan.movedVertices.size() || !validTransform(transform) )
    {
        return result;
    }

    std::vector<ConnectedSurfaceUvVertexPosition> positions{};
    positions.reserve(plan.movedVertices.size());
    for ( std::size_t index = 0; index < plan.movedVertices.size(); ++index )
    {
        const TVector position = transformConnectedSurfaceUvPosition(plan.sourceMovedPositions[index], transform);
        if ( !finite(position) )
        {
            return result;
        }
        positions.push_back(ConnectedSurfaceUvVertexPosition{plan.movedVertices[index], position});
    }
    return evaluateConnectedSurfaceUvPlan(plan, positions);
}

bool applyConnectedSurfaceUvEvaluation(Level& level, const ConnectedSurfaceUvEvaluation& evaluation) noexcept
{
    if ( !isValidUvCoordinateSpace(level.uvCoordinateSpace) ||
         !isValidUvCoordinateSpace(evaluation.coordinateSpace) || !evaluation.valid ||
         level.uvCoordinateSpace != evaluation.coordinateSpace )
    {
        return false;
    }

    std::unordered_set<std::uint64_t> visited{};
    visited.reserve(evaluation.surfaces.size());
    for ( const ConnectedSurfaceUvSurface& update : evaluation.surfaces )
    {
        if ( !validSurface(level, update.surface) || !visited.insert(surfaceKey(update.surface)).second )
        {
            return false;
        }
        const Surface& target = level.sectors[static_cast<std::size_t>(update.surface.sector)]
                                    .surfaces[static_cast<std::size_t>(update.surface.surface)];
        if ( target.vertices != update.vertices || target.texVertices.size() != update.texVertices.size() ||
             !std::ranges::all_of(update.texVertices,
                                  [](const TexVertex& value)
                                  {
                                      return finite(value);
                                  }) )
        {
            return false;
        }
    }

    for ( const ConnectedSurfaceUvSurface& update : evaluation.surfaces )
    {
        level.sectors[static_cast<std::size_t>(update.surface.sector)]
            .surfaces[static_cast<std::size_t>(update.surface.surface)]
            .texVertices = update.texVertices;
    }
    return true;
}

} // namespace sed::level
