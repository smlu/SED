#pragma once

#include "sed/level.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <span>
#include <utility>
#include <vector>

namespace sed::level::precision
{

// Stages the existing Surface Connect candidate and propagates allocation
// failures to compound operations instead of treating them as a mismatch.
bool connectSurfacesChecked(Level& level, SurfaceRef first, SurfaceRef second);

// Extrude-only connection: preserves original vertices and lifts only generated
// cap corners to the source boundary, with exact reciprocal portal validation.
// The caller must validate inherited cap error and every new side/outer face.
// Full commits pass the immutable pre-append Level for semantic COG capture;
// geometry-only previews have no COG collection and pass nullptr.
// A validator, when supplied, runs on each fully connected candidate before
// the transaction publishes its geometry or COG remapping. It must be
// synchronous and non-owning: the context is valid only for the call and the
// validator returns false to discard this connection attempt so the caller can
// try the partitioned construction from the same immutable input.
using ExtrudeConnectionValidator = bool (*)(Level& level, SurfaceRef original,
    SurfaceRef generated, void* context);

bool connectExtrudeSurfacesChecked(Level& level, SurfaceRef original, SurfaceRef generated,
    const Level* referenceSource = nullptr,
    ExtrudeConnectionValidator validator = nullptr, void* validatorContext = nullptr);
// Shared immutable preflight for bounded Extrude previews and full commits.
// Includes the selected owner's direct portal neighbors; allocations propagate.
bool extrudeSourceContextIsValid(const Level& level, SurfaceRef source);
bool makeAdjoinFromSectorChecked(Level& level, SurfaceRef surface, std::int32_t startSector, bool reset);

// Atomic topology operations must distinguish allocation failure from a real
// non-overlap. The public bool predicate retains its no-throw fallback contract.
bool surfaceDoOverlapChecked(const Sector& firstSector, const Surface& firstSurface,
    const Sector& secondSector, const Surface& secondSurface);

// Editing admission for inherited source rounding only. Absolute legacy bounds
// are capped by intrinsic feature size; these are never output/welding budgets.
// Transaction owners must freeze measured source residuals and certify all new
// geometry separately. Unset normals may be derived for surface-only callers.
// Topology-only projected boundary check: no inherited-error allowance.
bool surfaceHasSimpleBoundary(const Sector& sector, const Surface& surface) noexcept;
bool sourceSurfaceIsPlanar(const Sector& sector, const Surface& surface) noexcept;
bool sourceSurfaceIsConvex(const Sector& sector, const Surface& surface) noexcept;
bool sourceSectorIsConvex(const Sector& sector) noexcept;

// Exact ordered 3D boundary correspondence, including redundant edge corners.
// This does not establish planarity or solid convexity. Geometry transactions
// may use it to preserve an existing portal or pair construction-proven caps
// after independently validating inherited rounding and all new geometry.
// Allocation failure propagates to the owning transaction's rollback boundary.
bool surfaceBoundariesMatchChecked(const Sector& firstSector, const Surface& firstSurface,
    const Sector& secondSector, const Surface& secondSurface);

// Error allowances for double arithmetic, in the units of the expression
// being tested. They are not a designer-facing proximity/welding distance.
inline constexpr double kRoundoff = 64.0 * std::numeric_limits<double>::epsilon();

inline TVector position(const Vertex &vertex) noexcept
{
    return {vertex.x, vertex.y, vertex.z};
}

inline bool finite(const TVector &value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

// Checks the exact identity used for an inherited source cap. The cap must be
// face zero, contain every source corner exactly once, and run in the reverse
// cyclic order required by the generated sector. This is a topology/provenance
// proof only; it does not certify planarity, convexity, or portal orientation.
// Reject malformed input before dereferencing any face index. In particular,
// no non-finite or repeated point may be admitted by an exact comparison.
inline bool exactReversedSourceCapIdentity(const Sector& sector,
    const std::size_t faceIndex,
    const Surface& face,
    const std::span<const TVector> source) noexcept
{
    if ( faceIndex != 0 || source.size() < 3 || face.vertices.size() != source.size() )
    {
        return false;
    }

    const auto samePoint = [](const TVector& first, const TVector& second) noexcept
    {
        return first.x == second.x && first.y == second.y && first.z == second.z;
    };

    for ( std::size_t index = 0; index < source.size(); ++index )
    {
        if ( !finite(source[index]) )
        {
            return false;
        }
        for ( std::size_t other = 0; other < index; ++other )
        {
            if ( samePoint(source[index], source[other]) )
            {
                return false;
            }
        }
    }

    for ( std::size_t index = 0; index < face.vertices.size(); ++index )
    {
        const auto vertexIndex = face.vertices[index];
        if ( vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size() )
        {
            return false;
        }
        const TVector point = position(sector.vertices[static_cast<std::size_t>(vertexIndex)]);
        if ( !finite(point) )
        {
            return false;
        }
        for ( std::size_t other = 0; other < index; ++other )
        {
            const TVector otherPoint = position(
                sector.vertices[static_cast<std::size_t>(face.vertices[other])]);
            if ( samePoint(point, otherPoint) )
            {
                return false;
            }
        }
    }

    const std::size_t count = source.size();
    for ( std::size_t sourceStart = 0; sourceStart < count; ++sourceStart )
    {
        bool matches = true;
        for ( std::size_t corner = 0; corner < count; ++corner )
        {
            const std::size_t offset = corner % count;
            const std::size_t sourceIndex = sourceStart >= offset
                ? sourceStart - offset
                : count - (offset - sourceStart);
            const TVector point = position(
                sector.vertices[static_cast<std::size_t>(face.vertices[corner])]);
            if ( !samePoint(point, source[sourceIndex]) )
            {
                matches = false;
                break;
            }
        }
        if ( matches )
        {
            return true;
        }
    }
    return false;
}

struct SurfaceArea
{
    std::array<long double, 3> value{};
    std::array<long double, 3> error{};
    std::array<long double, 3> arithmeticError{};
    bool valid{};
};

inline SurfaceArea surfaceArea(const Sector &sector, const Surface &surface,
                               const bool calculateError = true) noexcept
{
    SurfaceArea result{};
    const auto count = surface.vertices.size();
    if (count < 3)
        return result;
    const auto pointAt = [&](std::size_t corner)
    { return position(sector.vertices[surface.vertices[corner]]); };
    for (const auto index : surface.vertices)
        if (index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() ||
            !finite(position(sector.vertices[index])))
            return result;
    const TVector origin = pointAt(0);
    std::array<long double, 3> compensation{};
    for (std::size_t i = 1; i + 1 < count; ++i)
    {
        // A signed fan equals Newell's area vector, including concave faces.
        // Two shortest cyclic edges avoid width² cancellation in thin faces;
        // this improves conditioning even when long double equals double.
        const std::array<TVector, 3> edges{
            {pointAt(i) - origin, pointAt(i + 1) - pointAt(i), origin - pointAt(i + 1)}};
        std::size_t longest = 0;
        double longestLength = -1.0;
        for (std::size_t edge = 0; edge < 3; ++edge)
        {
            if (!finite(edges[edge]))
                return {};
            const double length = std::hypot(edges[edge].x, edges[edge].y, edges[edge].z);
            if (!std::isfinite(length))
                return {};
            if (length > longestLength)
            {
                longestLength = length;
                longest = edge;
            }
        }
        const TVector &first = edges[(longest + 1) % 3];
        const TVector &second = edges[(longest + 2) % 3];
        const std::array<long double, 3> a{{first.x, first.y, first.z}}, b{{second.x, second.y, second.z}};
        for (std::size_t component = 0; component < 3; ++component)
        {
            const auto j = (component + 1) % 3, k = (component + 2) % 3;
            const long double product1 = a[j] * b[k], product2 = a[k] * b[j];
            const long double increment = (product1 - product2) - compensation[component];
            const long double sum = result.value[component] + increment;
            compensation[component] = (sum - result.value[component]) - increment;
            result.value[component] = sum;
            result.error[component] += kRoundoff * (std::abs(product1) + std::abs(product2));
        }
    }
    result.arithmeticError = result.error;
    if (!calculateError)
    {
        result.valid = std::all_of(result.value.begin(), result.value.end(),
                                   [](const long double value) { return std::isfinite(value); });
        return result;
    }
    // Newell A=sum(Vi × Vnext). A vertex perturbation contributes
    // dVi × (Vnext−Vprev). Propagate coordinate roundoff from every corner,
    // not just the near anchor/query; include the quadratic perturbation.
    // Both value and error have area units.
    for (std::size_t i = 0; i < count; ++i)
    {
        const TVector current = pointAt(i), previous = pointAt((i + count - 1) % count),
                      next = pointAt((i + 1) % count);
        const std::array<long double, 3> p{{current.x, current.y, current.z}},
            before{{previous.x, previous.y, previous.z}}, after{{next.x, next.y, next.z}};
        for (std::size_t component = 0; component < 3; ++component)
        {
            const auto j = (component + 1) % 3, k = (component + 2) % 3;
            result.error[component] += kRoundoff * (std::abs(p[j]) * std::abs(after[k] - before[k]) +
                                                    std::abs(p[k]) * std::abs(after[j] - before[j])) +
                                       kRoundoff * kRoundoff *
                                           (std::abs(p[j]) * (std::abs(after[k]) + std::abs(before[k])) +
                                            std::abs(p[k]) * (std::abs(after[j]) + std::abs(before[j])));
        }
    }
    result.valid = true;
    for (std::size_t i = 0; i < 3; ++i)
        result.valid = result.valid && std::isfinite(result.value[i]) && std::isfinite(result.error[i]);
    return result;
}

struct SurfacePlaneError
{
    TVector normalError{}; // Dimensionless uncertainty of the cached normal.
    TVector geometricNormal{};
    const Sector *sector{};
    const Surface *surface{};
    std::array<long double, 3> arithmeticError{};
    long double areaLowerBound{};
    long double areaUncertainty{};
    double normalLength{};
    bool valid{};

    double at(const TVector &normal, const TVector &query, const TVector &anchor) const noexcept;
};

inline SurfacePlaneError surfacePlaneError(const Sector &sector, const Surface &surface,
                                           const TVector &normal) noexcept
{
    SurfacePlaneError result{};
    if (!finite(normal))
        return result;
    const double cachedLength = std::hypot(normal.x, normal.y, normal.z);
    if (!(cachedLength > 0.0) || !std::isfinite(cachedLength))
        return result;
    const SurfaceArea area = surfaceArea(sector, surface);
    if (!area.valid)
        return result;
    const long double length = std::hypot(area.value[0], area.value[1], area.value[2]);
    const long double uncertainty = std::hypot(area.error[0], area.error[1], area.error[2]);
    // An unresolved area direction cannot justify an infinite tolerance.
    if (!std::isfinite(length) || !std::isfinite(uncertainty) || !(length > uncertainty))
        return result;
    std::array<double, 3> unit{}, error{};
    for (std::size_t i = 0; i < 3; ++i)
    {
        unit[i] = static_cast<double>(area.value[i] / length);
        // |delta(A/|A|)| <= (|deltaA|+|A/|A||*|delta|A||)/(|A|-|deltaA|).
        error[i] = static_cast<double>(
            cachedLength * ((area.error[i] + std::abs(unit[i]) * uncertainty) / (length - uncertainty) +
                            kRoundoff * std::abs(unit[i])));
        if (!std::isfinite(error[i]))
            return {};
    }
    result.normalError = {error[0], error[1], error[2]};
    result.geometricNormal = {unit[0], unit[1], unit[2]};
    // A cached normal may be scaled or sign-reversed for a plane query, but
    // cannot invent another plane. Convex solid queries check winding too.
    const double orientation = normal.dot(result.geometricNormal) < 0.0 ? -1.0 : 1.0;
    const std::array<double, 3> cached{{normal.x, normal.y, normal.z}};
    for (std::size_t i = 0; i < 3; ++i)
        if (std::abs(cached[i] - orientation * cachedLength * unit[i]) > error[i])
            return {};
    result.sector = &sector;
    result.surface = &surface;
    result.arithmeticError = area.arithmeticError;
    result.areaLowerBound = length - uncertainty;
    result.areaUncertainty = uncertainty;
    result.normalLength = cachedLength;
    result.valid = true;
    return result;
}

inline SurfacePlaneError surfacePlaneError(const Sector &sector, const Surface &surface) noexcept
{
    return surfacePlaneError(sector, surface, surface.normal);
}

inline double planeError(const TVector &normal, const TVector &a, const TVector &b) noexcept
{
    return kRoundoff * (std::abs(normal.x) * (std::abs(a.x) + std::abs(b.x)) +
                        std::abs(normal.y) * (std::abs(a.y) + std::abs(b.y)) +
                        std::abs(normal.z) * (std::abs(a.z) + std::abs(b.z)));
}

inline double SurfacePlaneError::at(const TVector &normal, const TVector &query,
                                    const TVector &anchor) const noexcept
{
    const TVector delta = query - anchor;
    if (!valid || !finite(delta) || !sector || !surface)
    {
        return -1.0;
    }
    const std::array<long double, 3> direction{{delta.x, delta.y, delta.z}};
    long double directionalAreaError = 0.0L;
    for (std::size_t component = 0; component < 3; ++component)
    {
        directionalAreaError += std::abs(direction[component]) * arithmeticError[component];
    }

    // Preserve directional correlation before taking absolute values:
    // deltaA dot D = sum(deltaVi dot ((Vnext - Vprev) cross D)).
    // Bounding each normal component first would discard cancellation for a
    // long tangent query and could incorrectly admit real outside geometry.
    // Area/normalization data is cached once per face; this directional part
    // depends on the query. Referencing the stable face avoids allocations.
    const std::size_t count = surface->vertices.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        const Vertex &current = sector->vertices[surface->vertices[i]];
        const Vertex &previous = sector->vertices[surface->vertices[(i + count - 1) % count]];
        const Vertex &next = sector->vertices[surface->vertices[(i + 1) % count]];
        const std::array<long double, 3> point{{current.x, current.y, current.z}};
        const std::array<long double, 3> before{{previous.x, previous.y, previous.z}};
        const std::array<long double, 3> after{{next.x, next.y, next.z}};
        for (std::size_t component = 0; component < 3; ++component)
        {
            const auto j = (component + 1) % 3;
            const auto k = (component + 2) % 3;
            const long double first = (after[j] - before[j]) * direction[k];
            const long double second = (after[k] - before[k]) * direction[j];
            directionalAreaError +=
                kRoundoff * std::abs(point[component]) *
                (std::abs(first - second) + kRoundoff * (std::abs(first) + std::abs(second)));
            // Second-order products of coordinate perturbations remain tiny
            // but are retained so the bound does not rely on dropping them.
            directionalAreaError += kRoundoff * kRoundoff * std::abs(direction[component]) *
                                    (std::abs(point[j]) * (std::abs(after[k]) + std::abs(before[k])) +
                                     std::abs(point[k]) * (std::abs(after[j]) + std::abs(before[j])));
        }
    }
    const std::array<long double, 3> unit{{geometricNormal.x, geometricNormal.y, geometricNormal.z}};
    const std::array<long double, 3> cached{{normal.x, normal.y, normal.z}};
    const double orientation = normal.dot(geometricNormal) < 0.0 ? -1.0 : 1.0;
    long double geometricDistance = 0.0L;
    long double cachedDifference = 0.0L;
    for (std::size_t component = 0; component < 3; ++component)
    {
        geometricDistance += unit[component] * direction[component];
        cachedDifference +=
            (cached[component] - orientation * normalLength * unit[component]) * direction[component];
    }
    const long double error = planeError(normal, query, anchor) + std::abs(cachedDifference) +
                              normalLength *
                                  (directionalAreaError + std::abs(geometricDistance) * areaUncertainty) /
                                  areaLowerBound;
    if (!std::isfinite(error) || error > std::numeric_limits<double>::max())
    {
        return -1.0;
    }
    return static_cast<double>(error);
}

inline bool samePoint(const TVector &a, const TVector &b) noexcept
{
    return finite(a) && finite(b) &&
           std::abs(a.x - b.x) <= kRoundoff * std::max(std::abs(a.x), std::abs(b.x)) &&
           std::abs(a.y - b.y) <= kRoundoff * std::max(std::abs(a.y), std::abs(b.y)) &&
           std::abs(a.z - b.z) <= kRoundoff * std::max(std::abs(a.z), std::abs(b.z));
}

inline double turnError(const TVector &a, const TVector &b, const TVector &normal) noexcept
{
    return kRoundoff * (std::abs(normal.x) * (std::abs(a.y * b.z) + std::abs(a.z * b.y)) +
                        std::abs(normal.y) * (std::abs(a.z * b.x) + std::abs(a.x * b.z)) +
                        std::abs(normal.z) * (std::abs(a.x * b.y) + std::abs(a.y * b.x)));
}

inline double cornerTurnError(const TVector &first, const TVector &center, const TVector &last,
                              const TVector &normal) noexcept
{
    const TVector a = last - center;
    const TVector b = first - center;
    // Newly intersected vertices are rounded when written back to document
    // coordinates. Propagate that component-wise positional uncertainty
    // through the signed turn's derivatives. The result has area units, and
    // does not let a large X coordinate erase independently precise Y detail.
    const TVector firstDerivative = normal.cross(a);
    const TVector lastDerivative = b.cross(normal);
    const TVector centerDerivative = (firstDerivative + lastDerivative) * -1.0;
    return turnError(a, b, normal) + planeError(firstDerivative, first, TVector{}) +
           planeError(centerDerivative, center, TVector{}) + planeError(lastDerivative, last, TVector{});
}

inline std::int32_t addVertex(Sector &sector, const double x, const double y, const double z)
{
    const TVector point{x, y, z};
    for (std::size_t index = 0; index < sector.vertices.size(); ++index)
    {
        if (samePoint(position(sector.vertices[index]), point))
        {
            return static_cast<std::int32_t>(index);
        }
    }
    return sector.newVertex(x, y, z);
}

inline std::int32_t addVertex(Sector &sector, const TVector &point)
{
    return addVertex(sector, point.x, point.y, point.z);
}

inline bool onSegment(const TVector &point, const TVector &a, const TVector &b, double &parameter) noexcept
{
    const TVector edge = b - a;
    // The dominant coordinate avoids squaring an ill-conditioned edge and
    // keeps very thin geometry independent of the other coordinate scales.
    const double magnitude = std::max({std::abs(edge.x), std::abs(edge.y), std::abs(edge.z)});
    if (!finite(point) || !finite(a) || !finite(b) || !(magnitude > 0.0))
    {
        return false;
    }
    parameter = std::abs(edge.x) == magnitude   ? (point.x - a.x) / edge.x
                : std::abs(edge.y) == magnitude ? (point.y - a.y) / edge.y
                                                : (point.z - a.z) / edge.z;
    if (parameter < -kRoundoff || parameter > 1.0 + kRoundoff)
    {
        return false;
    }
    const TVector relative = point - a;
    const auto matches =
        [parameter](const double value, const double delta, const double start, const double end) noexcept
    {
        return std::abs(value - parameter * delta) <=
               kRoundoff * (std::abs(start) + std::abs(end) + std::abs(value) + std::abs(parameter * delta));
    };
    return matches(relative.x, edge.x, a.x, b.x) && matches(relative.y, edge.y, a.y, b.y) &&
           matches(relative.z, edge.z, a.z, b.z);
}

inline bool closedBoundary(const Sector &sector)
{
    if (sector.surfaces.size() < 4 || sector.vertices.size() < 4)
    {
        return false;
    }
    std::vector<bool> used(sector.vertices.size());
    for (const auto &face : sector.surfaces)
    {
        if (face.vertices.size() < 3)
        {
            return false;
        }
        for (const auto index : face.vertices)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= used.size())
            {
                return false;
            }
            used[index] = true;
        }
    }
    for (std::size_t index = 0; index < sector.vertices.size(); ++index)
    {
        if (!finite(position(sector.vertices[index])))
        {
            return false;
        }
    }
    using Edge = std::pair<std::int32_t, std::int32_t>;
    struct IndexedIncidence
    {
        std::size_t firstFace{};
        int count{};
        int orientation{};
        bool distinctFaces{true};
    };
    std::map<Edge, IndexedIncidence> indexedEdges;
    for ( std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex )
    {
        const auto& face = sector.surfaces[faceIndex];
        for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
        {
            const auto a = face.vertices[corner];
            const auto b = face.vertices[(corner + 1) % face.vertices.size()];
            // Check the raw indexed shell before coordinate aliasing can let a
            // nearby, unrelated vertex alter its incidence. Only an exactly
            // zero-length edge invalidates an otherwise balanced indexed
            // shell; a representable-step edge must retain its raw identity
            // and leave proximity handling to the geometric fallback.
            const auto firstPoint = position(sector.vertices[a]);
            const auto lastPoint = position(sector.vertices[b]);
            if ( firstPoint.x == lastPoint.x && firstPoint.y == lastPoint.y
                && firstPoint.z == lastPoint.z )
            {
                return false;
            }
            auto& incidence = indexedEdges[std::minmax(a, b)];
            if ( incidence.count == 0 )
            {
                incidence.firstFace = faceIndex;
            }
            else if ( incidence.firstFace == faceIndex )
            {
                incidence.distinctFaces = false;
            }
            ++incidence.count;
            incidence.orientation += a < b ? 1 : -1;
        }
    }
    // Shared authored indices prove closure directly. Nearby vertices on an
    // unrelated edge must not subdivide an already complete indexed shell.
    // Coordinate aliases and T-junctions still use the geometric fallback.
    if ( !indexedEdges.empty() && std::all_of(indexedEdges.begin(), indexedEdges.end(), [](const auto& edge)
        {
            return edge.second.count == 2 && edge.second.orientation == 0 && edge.second.distinctFaces;
        }) )
    {
        return true;
    }
    std::vector<std::int32_t> canonical(sector.vertices.size(), -1);
    for (std::size_t index = 0; index < sector.vertices.size(); ++index)
    {
        if (!used[index])
        {
            continue;
        }
        canonical[index] = static_cast<std::int32_t>(index);
        for (std::size_t earlier = 0; earlier < index; ++earlier)
        {
            if (used[earlier] && samePoint(position(sector.vertices[index]), position(sector.vertices[earlier])))
            {
                canonical[index] = canonical[earlier];
                break;
            }
        }
    }
    struct Incidence
    {
        int count{};
        int orientation{};
    };
    std::map<Edge, Incidence> edges;
    std::map<Edge, std::vector<std::int32_t>> splitChains;
    for (const auto &face : sector.surfaces)
    {
        for (std::size_t corner = 0; corner < face.vertices.size(); ++corner)
        {
            const auto first = canonical[face.vertices[corner]];
            const auto last = canonical[face.vertices[(corner + 1) % face.vertices.size()]];
            if (first == last)
            {
                return false;
            }
            const Edge key = std::minmax(first, last);
            auto [chain, inserted] = splitChains.try_emplace(key);
            if (inserted)
            {
                // Evaluate a canonical undirected edge exactly once. Reversing
                // the same geometric edge must not make an independently
                // rounded onSegment calculation choose different split points.
                std::vector<std::pair<double, std::int32_t>> points{{0.0, key.first}, {1.0, key.second}};
                for (std::size_t index = 0; index < sector.vertices.size(); ++index)
                {
                    if (!used[index] || canonical[index] != static_cast<std::int32_t>(index)
                        || canonical[index] == key.first || canonical[index] == key.second)
                    {
                        continue;
                    }
                    double parameter{};
                    if (onSegment(position(sector.vertices[index]), position(sector.vertices[key.first]),
                                  position(sector.vertices[key.second]), parameter)
                        && parameter > 0.0 && parameter < 1.0)
                    {
                        points.emplace_back(parameter, canonical[index]);
                    }
                }
                std::sort(points.begin(), points.end());
                for (const auto &[parameter, index] : points)
                {
                    (void)parameter;
                    chain->second.push_back(index);
                }
            }
            const int direction = first == key.first ? 1 : -1;
            for (std::size_t index = 1; index < chain->second.size(); ++index)
            {
                const auto a = chain->second[index - 1];
                const auto b = chain->second[index];
                auto &incidence = edges[std::minmax(a, b)];
                ++incidence.count;
                incidence.orientation += direction * (a < b ? 1 : -1);
            }
        }
    }
    return !edges.empty() && std::all_of(edges.begin(), edges.end(), [](const auto &edge)
                                         { return edge.second.count == 2 && edge.second.orientation == 0; });
}

} // namespace sed::level::precision

namespace sed::level::detail
{

// Internal Extrude seam used by core contract tests. It cancels only exact
// zero-length edges and exact out-and-back triples when their retained corner
// tuples agree. A false result can follow an earlier in-place cancellation, so
// callers own rollback of the containing transaction; the helper never welds
// merely nearby coordinates.
bool normalizeExactCollapsedExtrudeFace(const Sector& sector, Surface& face) noexcept;

} // namespace sed::level::detail
