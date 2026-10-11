#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/build.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/cleave_stroke_reach.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/uv.hpp"
#include "../../src/level/cleave_index_utils.hpp"
#include "../../src/level/cleave_projection_utils.hpp"
#include "../../src/level/extrude_arrangement_context.hpp"
#include "../../src/level/geometry_precision.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sed::tests
{

namespace
{

std::int32_t findVertexIndex(const sed::Sector& sector, const double x, const double y, const double z)
{
    for (std::size_t i = 0; i < sector.vertices.size(); ++i)
    {
        const sed::Vertex& v = sector.vertices[i];
        if (close(v.x, x) && close(v.y, y) && close(v.z, z))
        {
            return static_cast<std::int32_t>(i);
        }
    }
    return -1;
}

bool containsVertex(const std::vector<std::int32_t>& vertices, const std::int32_t vi)
{
    return std::find(vertices.begin(), vertices.end(), vi) != vertices.end();
}

const sed::TexVertex* findSurfaceTexVertex(
    const sed::Surface& surface,
    const std::int32_t sectorVertex) noexcept
{
    for (std::size_t index = 0; index < surface.vertices.size(); ++index)
    {
        if (surface.vertices[index] == sectorVertex && index < surface.texVertices.size())
        {
            return &surface.texVertices[index];
        }
    }
    return nullptr;
}

bool sameTexVertex(const sed::TexVertex& lhs, const sed::TexVertex& rhs) noexcept
{
    return lhs.u == rhs.u
        && lhs.v == rhs.v
        && lhs.color == rhs.color;
}

bool closeTexVertex(const sed::TexVertex& lhs, const sed::TexVertex& rhs) noexcept
{
    return close(lhs.u, rhs.u)
        && close(lhs.v, rhs.v)
        && close(lhs.color.r, rhs.color.r)
        && close(lhs.color.g, rhs.color.g)
        && close(lhs.color.b, rhs.color.b)
        && close(lhs.color.a, rhs.color.a);
}

bool sameTextureVertexVectors(
    const std::vector<sed::TexVertex>& lhs,
    const std::vector<sed::TexVertex>& rhs) noexcept
{
    if ( lhs.size() != rhs.size() )
    {
        return false;
    }
    for ( std::size_t index = 0; index < lhs.size(); ++index )
    {
        if ( !sameTexVertex(lhs[index], rhs[index]) )
        {
            return false;
        }
    }
    return true;
}

sed::Level makeExtrudeConnectionValidatorLevel()
{
    sed::Level level;
    level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
    auto& source = level.newSector();
    source.mark = 701;
    source.vertices = {{0, 0, 0}, {2, 0, 0}, {0, 2, 0}};
    sed::Surface triangle;
    triangle.mark = 703;
    triangle.material = "source.mat";
    triangle.vertices = {0, 1, 2};
    triangle.normal = {0, 0, 1};
    triangle.texVertices = {{-2, 3, sed::ColorWhite}, {4, -5, sed::ColorWhite}, {6, 7, sed::ColorWhite}};
    source.addSurface(std::move(triangle));

    sed::Sector generated;
    generated.num = 1;
    generated.mark = 702;
    generated.vertices = {{-1, -1, 0}, {3, -1, 0}, {3, 3, 0}, {-1, 3, 0},
        {-1, -1, -1}, {3, -1, -1}, {3, 3, -1}, {-1, 3, -1}};
    const auto append = [&](std::vector<std::int32_t> corners, const char* material)
    {
        sed::Surface face;
        face.material = material;
        face.vertices = std::move(corners);
        face.texVertices.resize(face.vertices.size());
        face.normal = sed::level::surfaceCalcNormalNewell(generated, face);
        face.num = static_cast<std::int32_t>(generated.surfaces.size());
        generated.addSurface(std::move(face));
    };
    append({0, 3, 2, 1}, "generated.mat");
    append({4, 5, 6, 7}, "other.mat");
    append({0, 1, 5, 4}, "other.mat");
    append({1, 2, 6, 5}, "other.mat");
    append({2, 3, 7, 6}, "other.mat");
    append({3, 0, 4, 7}, "other.mat");
    level.sectors.push_back(std::move(generated));
    return level;
}

struct ExtrudeConnectionValidatorProbe
{
    std::int32_t sourceMark{};
    std::int32_t generatedMark{};
    std::string generatedMaterial;
    int calls{};
    bool rejectAll{};
    bool throwAllocation{};
    bool firstSawPristine{};
    bool secondSawPristine{};
    bool thirdSawPristine{};
    bool sawContainedCandidate{};
    bool containedSawPristine{};
    bool rejectContained{};
    bool throwContained{};
};

bool extrudeConnectionValidatorProbe(sed::Level& level, const sed::SurfaceRef original,
    const sed::SurfaceRef generated, void* const opaque)
{
    auto* const probe = static_cast<ExtrudeConnectionValidatorProbe*>(opaque);
    if ( !probe || original.sector < 0 || generated.sector < 0
        || static_cast<std::size_t>(original.sector) >= level.sectors.size()
        || static_cast<std::size_t>(generated.sector) >= level.sectors.size()
        || level.sectors[static_cast<std::size_t>(original.sector)].surfaces.empty()
        || generated.surface < 0
        || static_cast<std::size_t>(generated.surface)
            >= level.sectors[static_cast<std::size_t>(generated.sector)].surfaces.size() )
    {
        return false;
    }
    ++probe->calls;
    auto& source = level.sectors[static_cast<std::size_t>(original.sector)];
    auto& generatedSector = level.sectors[static_cast<std::size_t>(generated.sector)];
    const auto& generatedFace = generatedSector.surfaces[static_cast<std::size_t>(generated.surface)];
    const bool pristine = source.mark == probe->sourceMark
        && generatedSector.mark == probe->generatedMark
        && generatedFace.material == probe->generatedMaterial;
    if ( probe->calls == 1 )
    {
        probe->firstSawPristine = pristine;
    }
    else if ( probe->calls == 2 )
    {
        probe->secondSawPristine = pristine;
    }
    else if ( probe->calls == 3 )
    {
        probe->thirdSawPristine = pristine;
    }
    const bool containedCandidate = generatedSector.surfaces.size() == 13
        && generatedSector.vertices.size() == 11
        && generatedFace.vertices.size() == 3;
    if ( containedCandidate )
    {
        probe->sawContainedCandidate = true;
        probe->containedSawPristine = pristine;
        if ( probe->throwContained )
        {
            throw std::bad_alloc{};
        }
        if ( probe->rejectContained )
        {
            source.mark = 9001;
            generatedSector.mark = 9002;
            generatedSector.surfaces[static_cast<std::size_t>(generated.surface)].material = "trial-mutated.mat";
            return false;
        }
    }
    if ( probe->throwAllocation )
    {
        throw std::bad_alloc{};
    }
    if ( probe->rejectAll || probe->calls == 1 )
    {
        source.mark = 9001;
        generatedSector.mark = 9002;
        generatedSector.surfaces[static_cast<std::size_t>(generated.surface)].material = "trial-mutated.mat";
        return false;
    }
    return true;
}

struct PartitionRouteProbe
{
    int calls{};
    std::size_t firstPortalCount{};
    std::size_t lastPortalCount{};
    std::size_t rejectedSinglePortalCandidates{};
    std::size_t acceptedMultipartitionCandidates{};
};

bool rejectSinglePortalThenAcceptPartition(sed::Level& level,
    const sed::SurfaceRef original,
    const sed::SurfaceRef generated,
    void* const opaque)
{
    auto* const probe = static_cast<PartitionRouteProbe*>(opaque);
    if ( !probe || original.sector < 0 || generated.sector < 0
        || static_cast<std::size_t>(original.sector) >= level.sectors.size()
        || static_cast<std::size_t>(generated.sector) >= level.sectors.size())
    {
        return false;
    }

    ++probe->calls;
    const auto& source = level.sectors[static_cast<std::size_t>(original.sector)];
    std::size_t portalCount = 0;
    for ( const auto& face : source.surfaces )
    {
        if ( face.adjoin && face.adjoin->sector == generated.sector )
        {
            ++portalCount;
        }
    }
    if (probe->calls == 1)
    {
        probe->firstPortalCount = portalCount;
    }
    probe->lastPortalCount = portalCount;
    if (portalCount <= 1)
    {
        ++probe->rejectedSinglePortalCandidates;
        return false;
    }
    ++probe->acceptedMultipartitionCandidates;
    return true;
}

bool sameVertexVectors(
    const std::vector<sed::Vertex>& lhs,
    const std::vector<sed::Vertex>& rhs) noexcept
{
    if ( lhs.size() != rhs.size() )
    {
        return false;
    }
    for ( std::size_t index = 0; index < lhs.size(); ++index )
    {
        if ( lhs[index].x != rhs[index].x
            || lhs[index].y != rhs[index].y
            || lhs[index].z != rhs[index].z
            || lhs[index].mark != rhs[index].mark )
        {
            return false;
        }
    }
    return true;
}

bool cleaveResultsEquivalent(
    const sed::Level& lhs,
    const sed::Level& rhs) noexcept
{
    if ( lhs.uvCoordinateSpace != rhs.uvCoordinateSpace
        || lhs.sectors.size() != rhs.sectors.size() )
    {
        return false;
    }

    for ( std::size_t sectorIndex = 0;
         sectorIndex < lhs.sectors.size();
         ++sectorIndex )
    {
        const sed::Sector& lhsSector = lhs.sectors[sectorIndex];
        const sed::Sector& rhsSector = rhs.sectors[sectorIndex];
        if ( lhsSector.vertices.size() != rhsSector.vertices.size()
            || lhsSector.surfaces.size() != rhsSector.surfaces.size() )
        {
            return false;
        }

        for ( std::size_t vertexIndex = 0;
             vertexIndex < lhsSector.vertices.size();
             ++vertexIndex )
        {
            const sed::Vertex& lhsVertex = lhsSector.vertices[vertexIndex];
            const sed::Vertex& rhsVertex = rhsSector.vertices[vertexIndex];
            if ( !close(lhsVertex.x, rhsVertex.x)
                || !close(lhsVertex.y, rhsVertex.y)
                || !close(lhsVertex.z, rhsVertex.z)
                || lhsVertex.mark != rhsVertex.mark )
            {
                return false;
            }
        }

        for ( std::size_t surfaceIndex = 0;
             surfaceIndex < lhsSector.surfaces.size();
             ++surfaceIndex )
        {
            const sed::Surface& lhsSurface = lhsSector.surfaces[surfaceIndex];
            const sed::Surface& rhsSurface = rhsSector.surfaces[surfaceIndex];
            if ( lhsSurface.vertices != rhsSurface.vertices
                || lhsSurface.nMat != rhsSurface.nMat
                || lhsSurface.adjoin != rhsSurface.adjoin
                || lhsSurface.texVertices.size()
                    != rhsSurface.texVertices.size()
                || !close(lhsSurface.uScale, rhsSurface.uScale)
                || !close(lhsSurface.vScale, rhsSurface.vScale)
                || !close(lhsSurface.normal.x, rhsSurface.normal.x)
                || !close(lhsSurface.normal.y, rhsSurface.normal.y)
                || !close(lhsSurface.normal.z, rhsSurface.normal.z) )
            {
                return false;
            }

            for ( std::size_t corner = 0;
                 corner < lhsSurface.texVertices.size();
                 ++corner )
            {
                if ( !closeTexVertex(
                        lhsSurface.texVertices[corner],
                        rhsSurface.texVertices[corner]) )
                {
                    return false;
                }
            }
        }
    }

    return true;
}

void checkSurfaceKeepsOriginalTextureVertices(
    const sed::Sector& sector,
    const std::int32_t material,
    const std::vector<std::pair<std::int32_t, sed::TexVertex>>& originals,
    const char* const label)
{
    for ( const auto& [vertex, expected] : originals )
    {
        std::size_t occurrenceCount = 0;
        for ( const sed::Surface& surface : sector.surfaces )
        {
            if ( surface.nMat != material )
            {
                continue;
            }

            const sed::TexVertex* actual = findSurfaceTexVertex(surface, vertex);
            if ( actual != nullptr )
            {
                ++occurrenceCount;
                check(sameTexVertex(*actual, expected), label);
            }
        }
        check(occurrenceCount == 1, label);
    }
}

double maxCoordX(const sed::Sector& sector)
{
    double value = -1.0e9;
    for (const sed::Vertex& v : sector.vertices)
    {
        value = std::max(value, v.x);
    }
    return value;
}

std::size_t countAdjoinedSurfaces(const sed::Level& level)
{
    std::size_t count = 0;
    for (const sed::Sector& sector : level.sectors)
    {
        for (const sed::Surface& surface : sector.surfaces)
        {
            if (surface.adjoin)
            {
                ++count;
            }
        }
    }
    return count;
}

bool allAdjoinsAreReciprocal(const sed::Level& level)
{
    for ( std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex )
    {
        const sed::Sector& sector = level.sectors[sectorIndex];
        for ( std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
        {
            const sed::Surface& surface = sector.surfaces[surfaceIndex];
            if ( !surface.adjoin )
            {
                continue;
            }

            const sed::SurfaceRef partner = *surface.adjoin;
            if ( partner.sector < 0 || partner.surface < 0
                || static_cast<std::size_t>(partner.sector) >= level.sectors.size() )
            {
                return false;
            }

            const sed::Sector& partnerSector =
                level.sectors[static_cast<std::size_t>(partner.sector)];
            if ( static_cast<std::size_t>(partner.surface) >= partnerSector.surfaces.size() )
            {
                return false;
            }

            const std::optional<sed::SurfaceRef>& back =
                partnerSector.surfaces[static_cast<std::size_t>(partner.surface)].adjoin;
            if ( !back || back->sector != static_cast<std::int32_t>(sectorIndex)
                || back->surface != static_cast<std::int32_t>(surfaceIndex) )
            {
                return false;
            }
        }
    }

    return true;
}

void setSectorSurfaceMaterial(sed::Sector& sector, const std::int32_t material)
{
    for (sed::Surface& surface : sector.surfaces)
    {
        surface.nMat = material;
    }
}

bool allAdjoinBoundariesMatchByCoordinates(const sed::Level& level)
{
    if ( !allAdjoinsAreReciprocal(level) )
    {
        return false;
    }
    const auto sameCoordinate = [](double a, double b)
    {
        return std::isfinite(a) && std::isfinite(b)
            && std::abs(a - b) <= 16 * std::numeric_limits<double>::epsilon() * std::max(std::abs(a), std::abs(b));
    };
    for ( const auto& sector : level.sectors )
    {
        for ( const auto& face : sector.surfaces )
        {
            if ( !face.adjoin )
            {
                continue;
            }
            const auto& otherSector = level.sectors[face.adjoin->sector];
            const auto& other = otherSector.surfaces[face.adjoin->surface];
            const auto count = face.vertices.size();
            if ( count < 3 || count != other.vertices.size() )
            {
                return false;
            }
            for ( const auto index : face.vertices )
            {
                if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
                {
                    return false;
                }
            }
            for ( const auto index : other.vertices )
            {
                if ( index < 0 || static_cast<std::size_t>(index) >= otherSector.vertices.size() )
                {
                    return false;
                }
            }
            bool matched = false;
            for ( std::size_t start = 0; start < count && !matched; ++start )
            {
                matched = true;
                for ( std::size_t corner = 0; corner < count; ++corner )
                {
                    const auto& a = sector.vertices[face.vertices[corner]];
                    const auto& b = otherSector.vertices[other.vertices[(start + count - corner) % count]];
                    matched = matched && sameCoordinate(a.x, b.x) && sameCoordinate(a.y, b.y) && sameCoordinate(a.z, b.z);
                }
            }
            if ( !matched )
            {
                return false;
            }
        }
    }
    return true;
}

bool independentOpposedLoopsMatch(std::vector<sed::TVector> first, std::vector<sed::TVector> second)
{
    const auto sameCoordinate = [](const double a, const double b)
    {
        return std::isfinite(a) && std::isfinite(b)
            && std::abs(a - b) <= 16 * std::numeric_limits<double>::epsilon() * std::max(std::abs(a), std::abs(b));
    };
    const auto simplify = [](std::vector<sed::TVector>& points)
    {
        if ( points.size() < 3 )
        {
            return false;
        }
        for ( std::size_t first = 0; first < points.size(); ++first )
        {
            const auto& a = points[first];
            if ( !std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z) )
            {
                return false;
            }
            for ( std::size_t second = first + 1; second < points.size(); ++second )
            {
                const auto& b = points[second];
                if ( a.x == b.x && a.y == b.y && a.z == b.z )
                {
                    return false;
                }
            }
        }
        bool changed = true;
        while ( changed )
        {
            changed = false;
            for ( std::size_t corner = 0; corner < points.size(); ++corner )
            {
                const auto& a = points[(corner + points.size() - 1) % points.size()];
                const auto& b = points[corner];
                const auto& c = points[(corner + 1) % points.size()];
                const std::array<long double, 3> start{{a.x, a.y, a.z}};
                const std::array<long double, 3> middle{{b.x, b.y, b.z}};
                const std::array<long double, 3> end{{c.x, c.y, c.z}};
                std::array<long double, 3> delta;
                std::size_t dominant = 0;
                for ( std::size_t axis = 0; axis < 3; ++axis )
                {
                    delta[axis] = end[axis] - start[axis];
                    if ( std::abs(delta[axis]) > std::abs(delta[dominant]) )
                    {
                        dominant = axis;
                    }
                }
                if ( delta[dominant] == 0 )
                {
                    return false;
                }
                const long double amount = (middle[dominant] - start[dominant]) / delta[dominant];
                bool collinear = true;
                for ( std::size_t axis = 0; axis < 3; ++axis )
                {
                    const long double error = 16 * std::numeric_limits<double>::epsilon()
                        * std::max({std::abs(start[axis]), std::abs(middle[axis]), std::abs(end[axis])});
                    collinear = collinear
                        && std::abs((middle[axis] - start[axis]) - amount * delta[axis]) <= error;
                }
                if ( !collinear )
                {
                    continue;
                }
                if ( amount <= 0 || amount >= 1 || points.size() <= 3 )
                {
                    return false;
                }
                points.erase(points.begin() + static_cast<std::ptrdiff_t>(corner));
                changed = true;
                break;
            }
        }
        return true;
    };
    if ( !simplify(first) || !simplify(second) || first.size() != second.size() )
    {
        return false;
    }
    for ( std::size_t start = 0; start < second.size(); ++start )
    {
        bool same = true;
        for ( std::size_t corner = 0; corner < first.size(); ++corner )
        {
            const auto& a = first[corner];
            const auto& b = second[(start + second.size() - corner) % second.size()];
            same = same && sameCoordinate(a.x, b.x) && sameCoordinate(a.y, b.y) && sameCoordinate(a.z, b.z);
        }
        if ( same )
        {
            return true;
        }
    }
    return false;
}

bool allAdjoinBoundariesMatchWithSubdivisions(const sed::Level& level)
{
    if ( !allAdjoinsAreReciprocal(level) )
    {
        return false;
    }
    for ( const auto& sector : level.sectors )
    {
        for ( const auto& face : sector.surfaces )
        {
            if ( !face.adjoin )
            {
                continue;
            }
            const auto& otherSector = level.sectors[face.adjoin->sector];
            const auto& other = otherSector.surfaces[face.adjoin->surface];
            std::vector<sed::TVector> first, second;
            for ( const auto index : face.vertices )
            {
                if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
                {
                    return false;
                }
                const auto& point = sector.vertices[index];
                first.push_back({point.x, point.y, point.z});
            }
            for ( const auto index : other.vertices )
            {
                if ( index < 0 || static_cast<std::size_t>(index) >= otherSector.vertices.size() )
                {
                    return false;
                }
                const auto& point = otherSector.vertices[index];
                second.push_back({point.x, point.y, point.z});
            }
            if ( !independentOpposedLoopsMatch(std::move(first), std::move(second)) )
            {
                return false;
            }
        }
    }
    return true;
}

bool adjoinPolygonsHaveNoProjectedCrossings(const sed::Level& level)
{
    for ( const auto& sector : level.sectors )
    {
        for ( const auto& face : sector.surfaces )
        {
            if ( !face.adjoin )
            {
                continue;
            }
            const auto count = face.vertices.size();
            if ( count < 3 )
            {
                return false;
            }
            std::vector<std::pair<long double, long double>> points;
            for ( const auto index : face.vertices )
            {
                if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
                {
                    return false;
                }
                const auto& point = sector.vertices[static_cast<std::size_t>(index)];
                if ( std::abs(face.normal.x) >= std::abs(face.normal.y) && std::abs(face.normal.x) >= std::abs(face.normal.z) )
                {
                    points.emplace_back(point.y, point.z);
                }
                else if ( std::abs(face.normal.y) >= std::abs(face.normal.z) )
                {
                    points.emplace_back(point.x, point.z);
                }
                else
                {
                    points.emplace_back(point.x, point.y);
                }
            }
            const auto side = [](const auto& a, const auto& b, const auto& c)
            {
                return (b.first - a.first) * (c.second - a.second) - (b.second - a.second) * (c.first - a.first);
            };
            const auto opposed = [](const long double a, const long double b)
            {
                return (a < 0 && b > 0) || (a > 0 && b < 0);
            };
            for ( std::size_t first = 0; first < count; ++first )
            {
                for ( std::size_t second = first + 1; second < count; ++second )
                {
                    const auto firstNext = (first + 1) % count;
                    const auto secondNext = (second + 1) % count;
                    if ( firstNext == second || secondNext == first )
                    {
                        continue;
                    }
                    if ( opposed(side(points[first], points[firstNext], points[second]), side(points[first], points[firstNext], points[secondNext]))
                        && opposed(side(points[second], points[secondNext], points[first]), side(points[second], points[secondNext], points[firstNext])) )
                    {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool sectorUsesMaterial(const sed::Sector& sector, const std::int32_t material)
{
    return std::any_of(
        sector.surfaces.begin(),
        sector.surfaces.end(),
        [material](const sed::Surface& surface)
        {
            return surface.nMat == material;
        });
}

sed::TVector resolvedSurfaceNormal(const sed::Sector& sector, const sed::Surface& surface)
{
    sed::TVector normal = surface.normal;
    if (normal.length() == 0.0)
    {
        normal = sed::level::surfaceCalcNormalNewell(sector, surface);
    }

    if (normal.normalize() == 0.0)
    {
        return sed::TVector{};
    }

    return normal;
}

bool containsInteriorPoint(const sed::Sector& sector, const sed::TVector& point)
{
    for (const sed::Surface& surface : sector.surfaces)
    {
        if (surface.vertices.empty())
        {
            continue;
        }

        const std::int32_t v0Index = surface.vertices[0];
        if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= sector.vertices.size())
        {
            return false;
        }

        const sed::TVector normal = resolvedSurfaceNormal(sector, surface);
        if (normal.length() == 0.0)
        {
            continue;
        }

        const sed::Vertex& v0 = sector.vertices[static_cast<std::size_t>(v0Index)];
        if (normal.dot(point.x - v0.x, point.y - v0.y, point.z - v0.z) <= 0.001)
        {
            return false;
        }
    }

    return true;
}

bool sectorHasInteriorSampleInside(const sed::Sector& source, const sed::Sector& container)
{
    if (containsInteriorPoint(container, sed::level::sectorCalcCenter(source)))
    {
        return true;
    }

    for (const sed::Surface& surface : source.surfaces)
    {
        sed::TVector surfaceSum{};
        std::int32_t validVertexCount = 0;

        for (std::size_t i = 0; i < surface.vertices.size(); ++i)
        {
            const std::int32_t currentIndex = surface.vertices[i];
            if (currentIndex < 0 || static_cast<std::size_t>(currentIndex) >= source.vertices.size())
            {
                continue;
            }

            const sed::Vertex& currentVertex = source.vertices[static_cast<std::size_t>(currentIndex)];
            const sed::TVector current{ currentVertex.x, currentVertex.y, currentVertex.z };
            surfaceSum = surfaceSum + current;
            ++validVertexCount;

            if (containsInteriorPoint(container, current))
            {
                return true;
            }

            if (surface.vertices.size() < 2)
            {
                continue;
            }

            const std::int32_t nextIndex = surface.vertices[(i + 1) % surface.vertices.size()];
            if (nextIndex < 0 || static_cast<std::size_t>(nextIndex) >= source.vertices.size())
            {
                continue;
            }

            const sed::Vertex& nextVertex = source.vertices[static_cast<std::size_t>(nextIndex)];
            const sed::TVector midpoint{
                (currentVertex.x + nextVertex.x) * 0.5,
                (currentVertex.y + nextVertex.y) * 0.5,
                (currentVertex.z + nextVertex.z) * 0.5,
            };
            if (containsInteriorPoint(container, midpoint))
            {
                return true;
            }
        }

        if (validVertexCount <= 0)
        {
            continue;
        }

        const sed::TVector surfaceCenter = surfaceSum * (1.0 / static_cast<double>(validVertexCount));
        if (containsInteriorPoint(container, surfaceCenter))
        {
            return true;
        }
    }

    return false;
}

bool sectorInteriorOverlaps(const sed::Sector& sector, const sed::Sector& container)
{
    if (sector.vertices.empty() || container.vertices.empty())
    {
        return false;
    }

    if (!sed::level::doBoxesIntersect(sed::level::sectorCalcBBox(sector), sed::level::sectorCalcBBox(container)))
    {
        return false;
    }

    const auto sectorsOverlap = [](const sed::Sector& a, const sed::Sector& b)
    {
        const auto surfacesDoNotSeparate = [](const sed::Sector& source, const sed::Sector& other)
        {
            for (const sed::Surface& surface : source.surfaces)
            {
                if (surface.vertices.empty())
                {
                    continue;
                }

                const std::int32_t v0Index = surface.vertices[0];
                if (v0Index < 0 || static_cast<std::size_t>(v0Index) >= source.vertices.size())
                {
                    return false;
                }

                const sed::TVector normal = resolvedSurfaceNormal(source, surface);
                if (normal.length() == 0.0)
                {
                    continue;
                }

                const sed::Vertex& v0 = source.vertices[static_cast<std::size_t>(v0Index)];
                bool hasVertexInsidePlane = false;
                for (const sed::Vertex& vertex : other.vertices)
                {
                    if (normal.dot(vertex.x - v0.x, vertex.y - v0.y, vertex.z - v0.z) > 0.01)
                    {
                        hasVertexInsidePlane = true;
                        break;
                    }
                }

                if (!hasVertexInsidePlane)
                {
                    return false;
                }
            }

            return true;
        };

        return surfacesDoNotSeparate(a, b) && surfacesDoNotSeparate(b, a);
    };

    return sectorsOverlap(sector, container)
        || sectorHasInteriorSampleInside(sector, container)
        || sectorHasInteriorSampleInside(container, sector);
}

bool anySectorOverlapsWithMaterial(const sed::Level& level, const sed::Sector& container, const std::int32_t material)
{
    return std::any_of(
        level.sectors.begin(),
        level.sectors.end(),
        [&container, material](const sed::Sector& sector)
        {
            return sectorInteriorOverlaps(sector, container) && sectorUsesMaterial(sector, material);
        });
}

std::size_t countSectorsWithMaterial(const sed::Level& level, const std::int32_t material)
{
    return static_cast<std::size_t>(std::count_if(
        level.sectors.begin(),
        level.sectors.end(),
        [material](const sed::Sector& sector)
        {
            return sectorUsesMaterial(sector, material);
        }));
}

} // namespace

void runLevelCleaveTests()
{
    {
        constexpr std::int32_t maximumId =
            std::numeric_limits<std::int32_t>::max();
        constexpr std::int32_t sourceId = 23;
        const std::array<sed::level::SectorCleavePlane, 2> twoPlanes{{
            {{1, 0, 0}, {0, 0, 0}},
            {{0, 1, 0}, {0, 0, 0}},
        }};
        const auto makeLevel = [](const std::int32_t nextId)
        {
            sed::Level level{};
            sed::Sector source = makeTestCubeSector(0.0);
            // Cleave validation requires a finite UV entry for every surface vertex.
            for ( sed::Surface& surface : source.surfaces )
            {
                surface.texVertices.resize(surface.vertices.size());
            }
            source.id = sourceId;
            source.num = 0;
            source.mark = 710;
            for ( std::size_t index = 0; index < source.vertices.size(); ++index )
            {
                source.vertices[index].mark = static_cast<std::int32_t>(720 + index);
            }
            for ( std::size_t index = 0; index < source.surfaces.size(); ++index )
            {
                source.surfaces[index].mark = static_cast<std::int32_t>(740 + index);
            }
            level.sectors.push_back(std::move(source));
            level.curId = nextId;
            return level;
        };
        const auto sourceRuntimeMarksAreIntact = [](const sed::Level& level)
        {
            if ( level.sectors.size() != 1
                || level.sectors[0].mark != 710
                || level.sectors[0].vertices.size() != 8
                || level.sectors[0].surfaces.size() != 6 )
            {
                return false;
            }
            for ( std::size_t index = 0; index < level.sectors[0].vertices.size(); ++index )
            {
                if ( level.sectors[0].vertices[index].mark
                    != static_cast<std::int32_t>(720 + index) )
                {
                    return false;
                }
            }
            for ( std::size_t index = 0; index < level.sectors[0].surfaces.size(); ++index )
            {
                if ( level.sectors[0].surfaces[index].mark
                    != static_cast<std::int32_t>(740 + index) )
                {
                    return false;
                }
            }
            return true;
        };

        sed::Level ordinaryCounter = makeLevel(sourceId + 1);
        const sed::level::SectorCleaveBatchResult ordinaryResult =
            sed::level::levelCleaveSectorByPlanes(
                ordinaryCounter, 0, twoPlanes);
        std::array<std::int32_t, 3> ordinaryDescendantIds{
            ordinaryCounter.sectors.size() > 1 ? ordinaryCounter.sectors[1].id : -1,
            ordinaryCounter.sectors.size() > 2 ? ordinaryCounter.sectors[2].id : -1,
            ordinaryCounter.sectors.size() > 3 ? ordinaryCounter.sectors[3].id : -1,
        };
        std::sort(ordinaryDescendantIds.begin(), ordinaryDescendantIds.end());
        check(ordinaryResult.succeeded()
                && ordinaryResult.outputSectors.size() == 4
                && ordinaryCounter.sectors.size() == 4
                && ordinaryCounter.curId == sourceId + 4
                && ordinaryDescendantIds
                    == std::array<std::int32_t, 3>{
                        sourceId + 1, sourceId + 2, sourceId + 3},
            "Sector Multi-cut fixture performs exactly three allocations at an ordinary counter");

        for ( const std::int32_t nextId : {maximumId - 1, maximumId - 2} )
        {
            sed::Level level = makeLevel(nextId);
            const auto before = sed::assets::saveToSedBytes(level);
            for ( int attempt = 0; attempt < 2; ++attempt )
            {
                const sed::level::SectorCleaveBatchResult result =
                    sed::level::levelCleaveSectorByPlanes(level, 0, twoPlanes);
                check(result.error == sed::level::SectorCleaveBatchError::CleaveFailed
                        && result.outputSectors.empty()
                        && level.curId == nextId
                        && sed::assets::saveToSedBytes(level) == before
                        && sourceRuntimeMarksAreIntact(level),
                    "Sector Multi-cut rejects insufficient IDs after candidate splits and preserves exact authored/runtime source state");
            }
        }

        sed::Level exactCapacity = makeLevel(maximumId - 3);
        const sed::level::SectorCleaveBatchResult exactResult =
            sed::level::levelCleaveSectorByPlanes(
                exactCapacity, 0, twoPlanes);
        check(exactResult.succeeded()
                && exactResult.outputSectors.size() == 4
                && exactCapacity.sectors.size() == 4
                && exactCapacity.curId == maximumId
                && exactCapacity.sectors[1].id == maximumId - 3
                && exactCapacity.sectors[2].id == maximumId - 2
                && exactCapacity.sectors[3].id == maximumId - 1,
            "Sector Multi-cut succeeds with exactly three IDs for a two-plane unit-cube split");

        sed::Level lastIdLevel = makeLevel(maximumId - 1);
        const sed::level::SectorCleaveBatchResult lastIdResult =
            sed::level::levelCleaveSectorByPlanes(
                lastIdLevel, 0,
                std::span<const sed::level::SectorCleavePlane>(twoPlanes.data(), 1));
        check(lastIdResult.succeeded()
                && lastIdLevel.sectors.size() == 2
                && lastIdLevel.curId == maximumId
                && lastIdLevel.sectors[1].id == maximumId - 1,
            "Sector Multi-cut admits one split using the final representable ID");
        const auto afterLastIdSplit = sed::assets::saveToSedBytes(lastIdLevel);
        const sed::level::SectorCleaveBatchResult exhaustedResult =
            sed::level::levelCleaveSectorByPlanes(
                lastIdLevel, 0,
                std::span<const sed::level::SectorCleavePlane>(twoPlanes.data() + 1, 1));
        check(exhaustedResult.error == sed::level::SectorCleaveBatchError::CleaveFailed
                && exhaustedResult.outputSectors.empty()
                && lastIdLevel.curId == maximumId
                && sed::assets::saveToSedBytes(lastIdLevel) == afterLastIdSplit,
            "Sector Multi-cut rejects the next split after consuming the final ID without changing the committed source");
    }

    {
        const std::vector<sed::TVector> source{{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}};
        sed::Sector sector;
        sector.vertices = {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}};
        sed::Surface face;
        constexpr std::array<std::array<std::int32_t, 4>, 4> reverseCyclicFaces{{
            {{0, 3, 2, 1}}, {{1, 0, 3, 2}}, {{2, 1, 0, 3}}, {{3, 2, 1, 0}}}};
        for ( const auto& expectedFace : reverseCyclicFaces )
        {
            face.vertices.assign(expectedFace.begin(), expectedFace.end());
            check(sed::level::precision::exactReversedSourceCapIdentity(
                    sector,
                    0,
                    face,
                    std::span<const sed::TVector>(source.data(), source.size())),
                "Inherited source cap identity: every reverse cyclic start is accepted on face zero");
        }

        const auto faceBeforeFailures = face.vertices;
        const auto samePointBits = [](const sed::TVector& first, const sed::TVector& second )
        {
            return std::bit_cast<std::uint64_t>(first.x) == std::bit_cast<std::uint64_t>(second.x)
                && std::bit_cast<std::uint64_t>(first.y) == std::bit_cast<std::uint64_t>(second.y)
                && std::bit_cast<std::uint64_t>(first.z) == std::bit_cast<std::uint64_t>(second.z);
        };
        const auto expectRejected = [&](const std::vector<std::int32_t>& vertices,
            const std::vector<sed::TVector>& points,
            const std::size_t faceIndex,
            const char* message)
        {
            face.vertices = vertices;
            const auto beforeVertices = face.vertices;
            const auto beforeSector = sector.vertices;
            const auto beforePoints = points;
            check(!sed::level::precision::exactReversedSourceCapIdentity(
                    sector,
                    faceIndex,
                    face,
                    std::span<const sed::TVector>(points.data(), points.size()))
                    && face.vertices == beforeVertices
                    && sector.vertices.size() == beforeSector.size()
                    && points.size() == beforePoints.size()
                    && std::equal(sector.vertices.begin(), sector.vertices.end(), beforeSector.begin(),
                        [&samePointBits](const sed::Vertex& first, const sed::Vertex& second)
                        {
                            return samePointBits(
                                {first.x, first.y, first.z},
                                {second.x, second.y, second.z})
                                && first.mark == second.mark;
                        })
                    && std::equal(points.begin(), points.end(), beforePoints.begin(), samePointBits),
                message);
        };

        face.vertices.assign(reverseCyclicFaces.front().begin(), reverseCyclicFaces.front().end());
        expectRejected(face.vertices, source, 1,
            "Inherited source cap identity: an exact source boundary is rejected for nonzero face indices");
        expectRejected({0, 1, 2, 3}, source, 0,
            "Inherited source cap identity: same-facing order is rejected");
        expectRejected({0, 2, 1, 3}, source, 0,
            "Inherited source cap identity: wrong cyclic order is rejected");
        expectRejected({0, 1, 2}, source, 0,
            "Inherited source cap identity: a corner-count mismatch is rejected");

        auto oneUlp = sector.vertices;
        oneUlp[2].x = std::nextafter(oneUlp[2].x, std::numeric_limits<double>::infinity());
        sector.vertices = oneUlp;
        face.vertices.assign(reverseCyclicFaces.front().begin(), reverseCyclicFaces.front().end());
        expectRejected(face.vertices, source, 0,
            "Inherited source cap identity: one-ULP XYZ displacement is rejected");
        sector.vertices = {{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}};

        expectRejected({0, 1, 2, 2}, source, 0,
            "Inherited source cap identity: repeated boundary coordinates are rejected");
        auto repeatedSource = source;
        repeatedSource[3] = repeatedSource[2];
        expectRejected(face.vertices, repeatedSource, 0,
            "Inherited source cap identity: repeated immutable source coordinates are rejected");
        expectRejected({0, 1, 2, 99}, source, 0,
            "Inherited source cap identity: invalid boundary indices are rejected before dereference");
        expectRejected({-1, 1, 2, 3}, source, 0,
            "Inherited source cap identity: negative boundary indices are rejected before dereference");

        auto nonfiniteSource = source;
        nonfiniteSource[1].x = std::numeric_limits<double>::infinity();
        face.vertices.assign(reverseCyclicFaces.front().begin(), reverseCyclicFaces.front().end());
        expectRejected(face.vertices, nonfiniteSource, 0,
            "Inherited source cap identity: nonfinite source coordinates are rejected");

        auto nonfiniteFace = sector.vertices;
        nonfiniteFace[1].y = std::numeric_limits<double>::quiet_NaN();
        sector.vertices = nonfiniteFace;
        face.vertices.assign(reverseCyclicFaces.front().begin(), reverseCyclicFaces.front().end());
        expectRejected(face.vertices, source, 0,
            "Inherited source cap identity: nonfinite boundary coordinates are rejected");

        face.vertices = faceBeforeFailures;
    }
    {
        const std::vector<sed::TVector> square{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
        const std::vector<sed::TVector> subdivided{{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0.5, 0}, {1, 0, 0}};
        check(independentOpposedLoopsMatch(square, subdivided),
            "Portal oracle: independent forward-collinear reduction accepts a fixed four-versus-five corner boundary");
        auto gap = subdivided;
        gap[3].x = 1.000001;
        auto backtrack = subdivided;
        backtrack[3].y = 1.25;
        check(!independentOpposedLoopsMatch(square, gap) && !independentOpposedLoopsMatch(square, backtrack)
                && !independentOpposedLoopsMatch(backtrack, backtrack) && !independentOpposedLoopsMatch(square, square),
            "Portal oracle: real gap, backtracking path and same-facing winding remain rejected");
    }
    for ( const bool malformedMap : {false, true} )
    {
        sed::Level level;
        auto& source = level.newSector();
        source.vertices = {{0, 0, 0}, {2, 0, 0}, {0, 2, 0}};
        sed::Surface triangle;
        triangle.vertices = {0, 1, 2};
        triangle.normal = {0, 0, 1};
        triangle.texVertices = {{-2, 3, sed::ColorWhite}, {4, -5, sed::ColorWhite}, {6, 7, sed::ColorWhite}};
        source.addSurface(triangle);
        sed::Sector generated;
        generated.vertices = {{-1, -1, 0}, {3, -1, 0}, {3, 3, 0}, {-1, 3, 0},
            {-1, -1, -1}, {3, -1, -1}, {3, 3, -1}, {-1, 3, -1}};
        const auto append = [&](std::vector<std::int32_t> corners)
        {
            sed::Surface face;
            face.vertices = std::move(corners);
            face.texVertices.resize(face.vertices.size());
            face.normal = sed::level::surfaceCalcNormalNewell(generated, face);
            generated.addSurface(face);
        };
        append({0, 3, 2, 1});
        append({4, 5, 6, 7});
        append({0, 1, 5, 4});
        append({1, 2, 6, 5});
        append({2, 3, 7, 6});
        append({3, 0, 4, 7});
        // Fixed affine map U=2x+3y+1, V=-x+4y-2. These literal
        // rectangle tuples independently constrain every later cap corner.
        generated.surfaces[0].texVertices = {{-4, -5, sed::ColorWhite}, {8, 11, sed::ColorWhite},
            {16, 7, sed::ColorWhite}, {4, -9, sed::ColorWhite}};
        if ( malformedMap )
        {
            generated.surfaces[0].texVertices[2].u = 17;
        }
        level.sectors.push_back(generated);
        const auto before = level;
        const bool accepted = sed::level::precision::connectExtrudeSurfacesChecked(level, {0, 0}, {1, 0});
        check(accepted == !malformedMap,
            "Generated cap UV: valid fixed affine rectangle accepts and inconsistent fourth sample rejects");
        if ( !accepted )
        {
            check(sed::assets::sedLevelsEqual(level, before), "Generated cap UV: rejected map preserves every level field");
            continue;
        }
        check(level.sectors.size() == 2 && allAdjoinBoundariesMatchByCoordinates(level),
            "Generated cap UV: fixed mapping retains exact reciprocal boundary topology");
        if ( level.sectors.size() != 2 )
        {
            continue;
        }
        for ( std::size_t faceIndex = 0; faceIndex < level.sectors[1].surfaces.size(); ++faceIndex )
        {
            if ( faceIndex > 0 && faceIndex < 6 )
            {
                continue;
            }
            const auto& face = level.sectors[1].surfaces[faceIndex];
            check(face.vertices.size() == face.texVertices.size(), "Generated cap UV: every generated corner has a complete tuple");
            if ( face.vertices.size() != face.texVertices.size() )
            {
                continue;
            }
            for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
            {
                const auto index = face.vertices[corner];
                const bool valid = index >= 0 && static_cast<std::size_t>(index) < level.sectors[1].vertices.size();
                check(valid, "Generated cap UV: projected output indices are valid before oracle reads");
                if ( !valid )
                {
                    continue;
                }
                const auto& point = level.sectors[1].vertices[index];
                const auto& tuple = face.texVertices[corner];
                check(tuple.u == static_cast<float>(2 * point.x + 3 * point.y + 1)
                        && tuple.v == static_cast<float>(-point.x + 4 * point.y - 2) && tuple.color == sed::ColorWhite,
                    "Generated cap UV: every surviving generated corner agrees with the independent fixed affine function");
            }
        }
        check(sameTextureVertexVectors(level.sectors[0].surfaces[0].texVertices, triangle.texVertices),
            "Generated cap UV: source authored tuples are never projected or rewritten");
    }
    {
        // A final validator is part of the connection transaction. A failed
        // convex trial must discard every temporary metadata mutation before
        // the partitioned retry observes the original candidate.
        auto level = makeExtrudeConnectionValidatorLevel();
        ExtrudeConnectionValidatorProbe probe{701, 702, "generated.mat"};
        const bool accepted = sed::level::precision::connectExtrudeSurfacesChecked(
            level, {0, 0}, {1, 0}, nullptr, extrudeConnectionValidatorProbe, &probe);
        check(accepted && probe.calls == 2 && probe.firstSawPristine && probe.secondSawPristine,
            "Extrude connection validator: rejected first trial is rolled back before a successful retry");
        check(level.sectors.size() == 2 && level.sectors[0].mark == 701
                && level.sectors[1].mark == 702
                && level.sectors[1].surfaces[0].material == "generated.mat"
                && level.sectors[0].surfaces[0].adjoin.has_value(),
            "Extrude connection validator: accepted retry publishes only its own metadata and reciprocal link");
    }
    {
        auto level = makeExtrudeConnectionValidatorLevel();
        const auto before = level;
        const auto beforeBytes = sed::assets::saveToSedBytes(level);
        ExtrudeConnectionValidatorProbe probe{701, 702, "generated.mat", 0, true};
        const bool accepted = sed::level::precision::connectExtrudeSurfacesChecked(
            level, {0, 0}, {1, 0}, nullptr, extrudeConnectionValidatorProbe, &probe);
        check(!accepted && probe.calls == 3 && probe.firstSawPristine && probe.secondSawPristine
                && probe.thirdSawPristine && probe.sawContainedCandidate
                && sed::assets::saveToSedBytes(level) == beforeBytes
                && sed::assets::sedLevelsEqual(level, before),
            "Extrude connection validator: fast, contained and partitioned rejected trials leave native bytes and runtime fields unchanged");
    }
    {
        auto level = makeExtrudeConnectionValidatorLevel();
        const auto before = level;
        const auto beforeBytes = sed::assets::saveToSedBytes(level);
        ExtrudeConnectionValidatorProbe probe{701, 702, "generated.mat", 0, false, true};
        bool propagated = false;
        try
        {
            (void)sed::level::precision::connectExtrudeSurfacesChecked(
                level, {0, 0}, {1, 0}, nullptr, extrudeConnectionValidatorProbe, &probe);
        }
        catch ( const std::bad_alloc& )
        {
            propagated = true;
        }
        check(propagated && probe.calls == 1 && sed::assets::saveToSedBytes(level) == beforeBytes
                && sed::assets::sedLevelsEqual(level, before),
            "Extrude connection validator: allocation failure propagates without retry or publication");
    }
    {
        auto level = makeExtrudeConnectionValidatorLevel();
        ExtrudeConnectionValidatorProbe probe{701, 702, "generated.mat"};
        probe.rejectContained = true;
        const bool accepted = sed::level::precision::connectExtrudeSurfacesChecked(
            level, {0, 0}, {1, 0}, nullptr, extrudeConnectionValidatorProbe, &probe);
        check(accepted && probe.calls == 3 && probe.firstSawPristine
                && probe.secondSawPristine && probe.thirdSawPristine
                && probe.sawContainedCandidate && probe.containedSawPristine
                && level.sectors.size() == 2 && level.sectors[0].surfaces[0].adjoin.has_value(),
            "Extrude connection validator: a rejected contained-ring candidate falls back from pristine input");
    }
    {
        auto level = makeExtrudeConnectionValidatorLevel();
        const auto before = level;
        const auto beforeBytes = sed::assets::saveToSedBytes(level);
        ExtrudeConnectionValidatorProbe probe{701, 702, "generated.mat"};
        probe.throwContained = true;
        bool propagated = false;
        try
        {
            (void)sed::level::precision::connectExtrudeSurfacesChecked(
                level, {0, 0}, {1, 0}, nullptr, extrudeConnectionValidatorProbe, &probe);
        }
        catch ( const std::bad_alloc& )
        {
            propagated = true;
        }
        check(propagated && probe.calls == 2 && probe.sawContainedCandidate
                && probe.containedSawPristine
                && sed::assets::saveToSedBytes(level) == beforeBytes
                && sed::assets::sedLevelsEqual(level, before),
            "Extrude connection validator: contained-ring allocation failure propagates without partition retry or publication");
    }
    {
        // A pre-existing exact duplicate is rejected during source-profile
        // admission. The private normalizer runs only after clipping/lifting,
        // so this control keeps the public transaction's fail-closed behavior
        // explicit instead of claiming to exercise a later phase.
        auto level = makeExtrudeConnectionValidatorLevel();
        auto& generated = level.sectors[1];
        auto& side = generated.surfaces[2];
        const auto duplicateSource = generated.vertices[side.vertices[1]];
        const auto duplicateIndex = generated.newVertex(
            duplicateSource.x, duplicateSource.y, duplicateSource.z);
        side.vertices.insert(side.vertices.begin() + 2, duplicateIndex);
        side.texVertices.insert(side.texVertices.begin() + 2, side.texVertices[1]);
        const auto before = level;
        const auto beforeBytes = sed::assets::saveToSedBytes(level);
        const bool accepted = sed::level::precision::connectExtrudeSurfacesChecked(
            level, {0, 0}, {1, 0});
        check(!accepted && sed::assets::saveToSedBytes(level) == beforeBytes
                && sed::assets::sedLevelsEqual(level, before),
            "Extrude cap normalization: pre-existing exact duplicate is rejected before normalizer admission and rolls back");
    }
    {
        // Exercise the cancellation helper directly after the public
        // preflight control above. The helper is an internal seam for the
        // post-lift phase; its caller still owns transaction rollback when a
        // later tuple conflict returns false.
        struct NormalizerCase
        {
            sed::Sector sector;
            sed::Surface face;
        };
        const auto makeCase = []()
        {
            NormalizerCase result;
            result.sector.vertices = {{0, 0, 0, 17}, {1, 0, 0, 18}, {1, 1, 0, 19},
                {0, 1, 0, 20}, {1, 0, 0, 21}};
            const sed::TexVertex first{1.25f, -2.5f, sed::ColorF{0.1f, 0.2f, 0.3f, 0.4f}};
            const sed::TexVertex second{-3.5f, 4.75f, sed::ColorF{0.5f, 0.6f, 0.7f, 0.8f}};
            const sed::TexVertex third{5.5f, -6.25f, sed::ColorF{0.9f, 0.125f, 0.25f, 0.375f}};
            const sed::TexVertex fourth{-7.0f, 8.5f, sed::ColorF{0.625f, 0.75f, 0.875f, 1.0f}};
            result.face.vertices = {0, 1, 4, 2, 3};
            result.face.texVertices = {first, second, second, third, fourth};
            return result;
        };
        const auto sameVertexPool = [](const std::vector<sed::Vertex>& first,
            const std::vector<sed::Vertex>& second)
        {
            if ( first.size() != second.size() )
            {
                return false;
            }
            for ( std::size_t index = 0; index < first.size(); ++index )
            {
                if ( std::bit_cast<std::uint64_t>(first[index].x)
                        != std::bit_cast<std::uint64_t>(second[index].x)
                    || std::bit_cast<std::uint64_t>(first[index].y)
                        != std::bit_cast<std::uint64_t>(second[index].y)
                    || std::bit_cast<std::uint64_t>(first[index].z)
                        != std::bit_cast<std::uint64_t>(second[index].z)
                    || first[index].mark != second[index].mark )
                {
                    return false;
                }
            }
            return true;
        };
        {
            auto test = makeCase();
            const auto beforeVertices = test.sector.vertices;
            const auto firstTuples = test.face.texVertices;
            const bool normalized = sed::level::detail::normalizeExactCollapsedExtrudeFace(
                test.sector, test.face);
            const std::vector<std::int32_t> expectedVertices{0, 1, 2, 3};
            const std::vector<sed::TexVertex> expectedTuples{
                firstTuples[0], firstTuples[1], firstTuples[3], firstTuples[4]};
            check(normalized && test.face.vertices == expectedVertices
                    && sameTextureVertexVectors(test.face.texVertices, expectedTuples)
                    && sameVertexPool(test.sector.vertices, beforeVertices),
                "Extrude cap normalization helper: equal UV/RGBA exact duplicate removes only the duplicate corner and preserves every retained output");
        }
        {
            auto test = makeCase();
            const auto beforeVertices = test.sector.vertices;
            test.face.texVertices[2].u = 9.0f;
            const auto expectedFace = test.face;
            const bool normalized = sed::level::detail::normalizeExactCollapsedExtrudeFace(
                test.sector, test.face);
            check(!normalized && test.face.vertices == expectedFace.vertices
                    && sameTextureVertexVectors(test.face.texVertices, expectedFace.texVertices)
                    && sameVertexPool(test.sector.vertices, beforeVertices),
                "Extrude cap normalization helper: unequal UV on an exact duplicate fails before changing topology");
        }
        {
            auto test = makeCase();
            const auto beforeVertices = test.sector.vertices;
            test.face.texVertices[2].color.g = 0.0625f;
            const auto expectedFace = test.face;
            const bool normalized = sed::level::detail::normalizeExactCollapsedExtrudeFace(
                test.sector, test.face);
            check(!normalized && test.face.vertices == expectedFace.vertices
                    && sameTextureVertexVectors(test.face.texVertices, expectedFace.texVertices)
                    && sameVertexPool(test.sector.vertices, beforeVertices),
                "Extrude cap normalization helper: unequal RGBA on an exact duplicate fails before changing topology");
        }
        {
            NormalizerCase test;
            test.sector.vertices = {{0, 0, 0, 31}, {1, 0, 0, 32}, {0, 0, 0, 33},
                {0, 1, 0, 34}, {1, 1, 0, 35}};
            const sed::TexVertex first{1, 2, sed::ColorF{0.1f, 0.2f, 0.3f, 0.4f}};
            const sed::TexVertex second{3, 4, sed::ColorF{0.5f, 0.6f, 0.7f, 0.8f}};
            const sed::TexVertex third{5, 6, sed::ColorF{0.9f, 0.125f, 0.25f, 0.375f}};
            test.face.vertices = {0, 1, 2, 3, 4};
            test.face.texVertices = {first, second, first, second, third};
            const auto beforeVertices = test.sector.vertices;
            const bool normalized = sed::level::detail::normalizeExactCollapsedExtrudeFace(
                test.sector, test.face);
            check(normalized && test.face.vertices == std::vector<std::int32_t>{0, 3, 4}
                    && sameTextureVertexVectors(test.face.texVertices,
                        std::vector<sed::TexVertex>{first, second, third})
                    && sameVertexPool(test.sector.vertices, beforeVertices),
                "Extrude cap normalization helper: exact out-and-back removes both transient corners and preserves the surviving tuples");
        }
        {
            NormalizerCase test;
            const double oneUlp = std::nextafter(1.0, std::numeric_limits<double>::infinity());
            test.sector.vertices = {{0, 0, 0, 41}, {1, 0, 0, 42}, {oneUlp, 0, 0, 43},
                {1, 1, 0, 44}};
            const sed::TexVertex first{1, 2, sed::ColorF{0.1f, 0.2f, 0.3f, 0.4f}};
            const sed::TexVertex second{3, 4, sed::ColorF{0.5f, 0.6f, 0.7f, 0.8f}};
            const sed::TexVertex third{5, 6, sed::ColorF{0.9f, 0.125f, 0.25f, 0.375f}};
            const sed::TexVertex fourth{7, 8, sed::ColorF{0.625f, 0.75f, 0.875f, 1.0f}};
            test.face.vertices = {0, 1, 2, 3};
            test.face.texVertices = {first, second, third, fourth};
            const auto beforeVertices = test.sector.vertices;
            const bool normalized = sed::level::detail::normalizeExactCollapsedExtrudeFace(
                test.sector, test.face);
            check(oneUlp != 1.0 && normalized
                    && test.face.vertices == std::vector<std::int32_t>{0, 1, 2, 3}
                    && sameTextureVertexVectors(test.face.texVertices,
                        std::vector<sed::TexVertex>{first, second, third, fourth})
                    && sameVertexPool(test.sector.vertices, beforeVertices),
                "Extrude cap normalization helper: a genuine one-ULP coordinate and all of its index/UV/RGBA data remain distinct");
        }
        {
            NormalizerCase test;
            test.sector.vertices = {{0, 0, 0, 51}, {1, 0, 0, 52}, {1, 1, 0, 53},
                {0, 1, 0, 54}, {1, 0, 0, 55}, {1, 1, 0, 56}};
            const sed::TexVertex first{1, 2, sed::ColorF{0.1f, 0.2f, 0.3f, 0.4f}};
            const sed::TexVertex second{3, 4, sed::ColorF{0.5f, 0.6f, 0.7f, 0.8f}};
            const sed::TexVertex third{5, 6, sed::ColorF{0.9f, 0.125f, 0.25f, 0.375f}};
            const sed::TexVertex unequal{7, 8, sed::ColorF{0.625f, 0.75f, 0.875f, 1.0f}};
            const sed::TexVertex fourth{-1, -2, sed::ColorF{0.25f, 0.375f, 0.5f, 0.625f}};
            test.face.vertices = {0, 1, 4, 2, 5, 3};
            test.face.texVertices = {first, second, second, third, unequal, fourth};
            const auto beforeVertices = test.sector.vertices;
            const bool normalized = sed::level::detail::normalizeExactCollapsedExtrudeFace(
                test.sector, test.face);
            check(!normalized && test.face.vertices == std::vector<std::int32_t>{0, 1, 2, 5, 3}
                    && sameTextureVertexVectors(test.face.texVertices,
                        std::vector<sed::TexVertex>{first, second, third, unequal, fourth})
                    && sameVertexPool(test.sector.vertices, beforeVertices),
                "Extrude cap normalization helper: a later unequal duplicate reports failure after the earlier exact cancellation, for caller rollback");
        }
        {
            auto test = makeCase();
            const auto beforeVertices = test.sector.vertices;
            test.face.texVertices.resize(test.face.vertices.size() - 1);
            const auto expectedFace = test.face;
            const bool normalized = sed::level::detail::normalizeExactCollapsedExtrudeFace(
                test.sector, test.face);
            check(!normalized && test.face.vertices == expectedFace.vertices
                    && sameTextureVertexVectors(test.face.texVertices, expectedFace.texVertices)
                    && sameVertexPool(test.sector.vertices, beforeVertices),
                "Extrude cap normalization helper: mismatched index/texture topology fails without mutation");
        }
        {
            auto test = makeCase();
            test.face.vertices[1] = 99;
            const auto beforeVertices = test.sector.vertices;
            const auto expectedFace = test.face;
            const bool normalized = sed::level::detail::normalizeExactCollapsedExtrudeFace(
                test.sector, test.face);
            check(!normalized && test.face.vertices == expectedFace.vertices
                    && sameTextureVertexVectors(test.face.texVertices, expectedFace.texVertices)
                    && sameVertexPool(test.sector.vertices, beforeVertices),
                "Extrude cap normalization helper: an out-of-range vertex index fails without mutation");
        }
        {
            auto test = makeCase();
            test.sector.vertices[1].y = std::numeric_limits<double>::quiet_NaN();
            const auto beforeVertices = test.sector.vertices;
            const auto expectedFace = test.face;
            const bool normalized = sed::level::detail::normalizeExactCollapsedExtrudeFace(
                test.sector, test.face);
            check(!normalized && test.face.vertices == expectedFace.vertices
                    && sameTextureVertexVectors(test.face.texVertices, expectedFace.texVertices)
                    && sameVertexPool(test.sector.vertices, beforeVertices),
                "Extrude cap normalization helper: a nonfinite retained coordinate fails without mutation");
        }
    }
    {
        auto level = makeExtrudeConnectionValidatorLevel();
        auto& generated = level.sectors[1];
        auto& side = generated.surfaces[2];
        const auto duplicateSource = generated.vertices[side.vertices[1]];
        const auto duplicateIndex = generated.newVertex(
            duplicateSource.x, duplicateSource.y, duplicateSource.z);
        side.vertices.insert(side.vertices.begin() + 2, duplicateIndex);
        side.texVertices.insert(side.texVertices.begin() + 2, sed::TexVertex{1.25f, -2.5f,
            sed::ColorF{0.125f, 0.75f, 0.375f, 0.625f}});
        const auto before = level;
        const auto beforeBytes = sed::assets::saveToSedBytes(level);
        const bool accepted = sed::level::precision::connectExtrudeSurfacesChecked(
            level, {0, 0}, {1, 0});
        check(!accepted && sed::assets::saveToSedBytes(level) == beforeBytes
                && sed::assets::sedLevelsEqual(level, before),
            "Extrude cap normalization: exact duplicate with unequal UV/RGBA rejects and rolls back all fields");
    }
    {
        auto level = makeExtrudeConnectionValidatorLevel();
        auto& generated = level.sectors[1];
        auto& side = generated.surfaces[2];
        const auto shared = generated.vertices[1];
        const double oneUlpX = std::nextafter(shared.x, std::numeric_limits<double>::infinity());
        check(oneUlpX != shared.x,
            "Extrude cap normalization: one-ULP fixture is representably distinct before the operation");
        const auto distinctIndex = generated.newVertex(oneUlpX, shared.y, shared.z);
        side.vertices[1] = distinctIndex;
        const bool accepted = sed::level::precision::connectExtrudeSurfacesChecked(
            level, {0, 0}, {1, 0});
        bool retained = accepted && level.sectors.size() == 2
            && level.sectors[1].surfaces.size() > 2;
        if ( retained )
        {
            const auto& normalized = level.sectors[1].surfaces[2];
            retained = std::find(normalized.vertices.begin(), normalized.vertices.end(), distinctIndex)
                != normalized.vertices.end()
                && level.sectors[1].vertices[static_cast<std::size_t>(distinctIndex)].x == oneUlpX;
        }
        check(retained,
            "Extrude cap normalization: representably distinct one-ULP vertices remain distinct and are not welded");
    }
    {
        sed::Level level;
        auto& sector = level.newSector();
        sector.newVertex(-1,1,0);
        sector.newVertex(0.5,-1+0.25e-7,0);
        sector.newVertex(1,-1,0);
        sed::Surface face;
        face.vertices = {0,1,2};
        face.normal = {0,0,1};
        face.texVertices = {{1,1,sed::ColorWhite},{0,0,sed::ColorWhite},{-2e-7f,-2e-7f,sed::ColorWhite}};
        sector.addSurface(face);
        sed::level::detail::ExtrudeArrangementRegistry registry({0,0,1},{0,0,0});
        sed::level::detail::ExtrudeArrangementContext context(registry,0,{0,0,1},{0,0,0});
        const bool captured = context.addCell(level,{0,0});
        check(captured,"Source tuple fixed vector: original triangle capture succeeds");
        if ( captured )
        {
            constexpr double x = 0x1.fffff29406e00p-1;
            const auto node = registry.addOriginal({x,-x,0});
            const auto edge = context.edgeLine({0,0},0,2);
            const bool incident = registry.registerIncidence(node,edge);
            check(incident,"Source tuple fixed vector: query has certified original-edge incidence");
            if ( incident )
            {
                const auto index = context.materializeNode(sector,0,node,{x,-x,0});
                const auto tuple = context.sourceTuple({0,0},index);
                // Exact binary-rational reference: t=(1-x)/2, a=float(-2e-7),
                // a+t*(1-a)=-6517931919619/2475880078570760549798248448.
                check(tuple && std::bit_cast<std::uint32_t>(tuple->u) == 0xa73db261u
                        && std::bit_cast<std::uint32_t>(tuple->v) == 0xa73db261u
                        && tuple->color == sed::ColorWhite,
                    "Source tuple fixed vector: near-zero U/V round to exact literal IEEE bits and preserve RGBA");
            }
            check(!context.sourceTuple({0,0},-1),"Source tuple fixed vector: invalid mesh index returns no tuple");
        }
    }
    for ( const auto rawNormal : {sed::TVector{1e-5,2e-5,1},sed::TVector{0.3,0.4,1}} )
    {
        const double length = std::hypot(rawNormal.x,rawNormal.y,rawNormal.z);
        const sed::TVector normal{rawNormal.x/length,rawNormal.y/length,rawNormal.z/length};
        for ( const double origin : {0.0,10000.0} )
        {
            for ( const double amount : {1.0/3.0,0.25,0.9500249999139747} )
            {
                sed::Level level;
                auto& sector = level.newSector();
                const sed::TVector a{origin+1,origin-1,0};
                const sed::TVector b{origin+1,origin+1,0};
                sector.newVertex(a.x,a.y,a.z);
                sector.newVertex(b.x,b.y,b.z);
                sector.newVertex(origin-1,origin-1,0);
                sector.newVertex(origin+3,origin-1,0);
                for ( const auto corners : {std::array<std::int32_t,3>{{0,1,2}},
                        std::array<std::int32_t,3>{{1,0,3}}} )
                {
                    sed::Surface face;
                    face.vertices.assign(corners.begin(),corners.end());
                    face.texVertices.resize(3);
                    face.normal = {0,0,1};
                    sector.addSurface(face);
                }
                const auto originalVertices = sector.vertices;
                sed::level::detail::ExtrudeArrangementRegistry registry(normal,a);
                sed::level::detail::ExtrudeArrangementContext context(registry,0,normal,a);
                const bool captured = context.addCell(level,{0,0}) && context.addCell(level,{0,1});
                check(captured,"Source edge position: adjacent immutable ears capture with an oblique projection frame");
                if ( !captured )
                {
                    continue;
                }
                const auto projectedB = b-normal*normal.dot(b-a);
                const sed::TVector projected{std::lerp(a.x,projectedB.x,amount),
                    std::lerp(a.y,projectedB.y,amount),std::lerp(a.z,projectedB.z,amount)};
                const auto node = registry.addOriginal(projected);
                const auto edge = context.edgeLine({0,0},0,1);
                const bool incident = registry.registerIncidence(node,edge);
                check(incident,"Source edge position: the fixed query has explicit immutable shared-edge incidence");
                if ( !incident )
                {
                    continue;
                }
                const auto first = context.sourcePosition({0,0},node);
                const auto reversed = context.sourcePosition({0,1},node);
                check(first && reversed,"Source edge position: both ears resolve a finite world point");
                if ( !first || !reversed )
                {
                    continue;
                }
                check(first->x == origin+1 && first->z == 0.0
                        && std::abs(first->y-(origin-1+2*amount))
                            <= 16*std::numeric_limits<double>::epsilon()*(std::abs(origin)+2),
                    "Source edge position: immutable world interpolation preserves constant x and exact z=0");
                check(std::bit_cast<std::uint64_t>(first->x) == std::bit_cast<std::uint64_t>(reversed->x)
                        && std::bit_cast<std::uint64_t>(first->y) == std::bit_cast<std::uint64_t>(reversed->y)
                        && std::bit_cast<std::uint64_t>(first->z) == std::bit_cast<std::uint64_t>(reversed->z),
                    "Source edge position: reversed adjacent ears reuse the same exact world node");
                const auto retained = registry.point(node);
                check(std::bit_cast<std::uint64_t>(retained.x) == std::bit_cast<std::uint64_t>(projected.x)
                        && std::bit_cast<std::uint64_t>(retained.y) == std::bit_cast<std::uint64_t>(projected.y)
                        && std::bit_cast<std::uint64_t>(retained.z) == std::bit_cast<std::uint64_t>(projected.z)
                        && sameVertexVectors(sector.vertices,originalVertices),
                    "Source edge position: lifting changes neither projected node identity nor any authored source vertex");
            }
        }
    }
    for ( const auto [origin, angle, bend, piecewiseUv] : std::array<std::array<double, 4>, 7>{{
        {10, 0, 1e-7, 0}, {0, 0.3, 1e-7, 0}, {0, 0, 1e-7, 0},
        {10, 0.3, 1e-5, 0}, {0, 0.6, 1e-5, 0}, {0, 0, 1e-7, 1}, {0, 0, 1e-7, 2}}} )
    {
        const std::array<sed::TVector, 7> footprint{{
            {-1, -1, 0}, {-0.5, -1 + bend, 0}, {0, -1 - bend * 0.5, 0},
            {0.5, -1 + bend * 0.25, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}}};
        sed::Level level;
        auto& sector = level.newSector();
        for ( std::int32_t layer = 0; layer < 2; ++layer )
        {
            for ( const auto& point : footprint )
            {
                sector.newVertex(origin + std::cos(angle) * point.x - std::sin(angle) * point.y,
                    origin + std::sin(angle) * point.x + std::cos(angle) * point.y, origin + layer);
            }
        }
        const auto append = [&](std::vector<std::int32_t> corners)
        {
            sed::Surface face;
            face.vertices = std::move(corners);
            face.texVertices.resize(face.vertices.size());
            face.normal = sed::level::surfaceCalcNormalNewell(sector, face);
            sector.addSurface(face);
        };
        append({0, 1, 2, 3, 4, 5, 6});
        append({13, 12, 11, 10, 9, 8, 7});
        for ( std::int32_t edge = 0; edge < 7; ++edge )
        {
            append({(edge + 1) % 7, edge, edge + 7, (edge + 1) % 7 + 7});
        }
        const bool checkAffineSource = origin == 0.0 && angle == 0.0 && piecewiseUv == 0.0;
        if ( checkAffineSource )
        {
            for ( std::size_t corner = 0; corner < sector.surfaces[0].vertices.size(); ++corner )
            {
                const auto& point = sector.vertices[sector.surfaces[0].vertices[corner]];
                auto& tuple = sector.surfaces[0].texVertices[corner];
                tuple.u = static_cast<float>(2*point.x+1);
                tuple.v = static_cast<float>(3-point.x);
                tuple.color = {0.25f,0.5f,0.75f,1.0f};
            }
        }
        const std::array<float,7> fixedU = piecewiseUv == 2.0
            ? std::array<float,7>{{0,0,0,0,-2e-7f,0,1}}
            : std::array<float,7>{{0.125f,1048576.0f,-1234.5f,123.25f,-987.75f,0.000001f,0.375f}};
        const std::array<float,7> fixedV = piecewiseUv == 2.0 ? fixedU
            : std::array<float,7>{{16777216.0f,-0.5f,0.0000001f,42.125f,-512.00006103515625f,0.125f,0.625f}};
        const std::array<sed::ColorF,7> fixedColors{{
            {0,0.25f,0.5f,1},{0.125f,0.5f,0.75f,0.875f},{0.25f,0.75f,0,0.75f},
            {0.375f,1,0.25f,0.625f},{0.5f,0,1,0.5f},{0.625f,0.125f,0.375f,0.375f},
            {0.75f,0.375f,0.625f,0.25f}}};
        if ( piecewiseUv != 0.0 )
        {
            for ( std::size_t corner = 0; corner < 7; ++corner )
            {
                auto& tuple = sector.surfaces[0].texVertices[corner];
                tuple.u = fixedU[corner];
                tuple.v = fixedV[corner];
                tuple.color = fixedColors[corner];
            }
        }
        const auto originalFaceCount = sector.surfaces.size();
        const auto originalVertices = sector.vertices;
        sed::level::SurfaceExtrudeReport report;
        const auto wire = sed::level::levelBuildSurfaceExtrudePreview(
            level, {0, 0}, 0.2, 0.1, &report);
        check(wire && report.error == sed::level::SurfaceExtrudeError::None
                && wire->innerLoop.size() == 4 && wire->outerLoop.size() == 4,
            "Expanded Extrude: alternating rounded boundary chains provide a fixed rectangular connection candidate");
        if (!wire || report.error != sed::level::SurfaceExtrudeError::None
            || wire->innerLoop.size() != 4 || wire->outerLoop.size() != 4)
        {
            continue;
        }
        sed::Sector generated;
        generated.num = static_cast<std::int32_t>(level.sectors.size());
        const auto appendLoop = [&](const std::vector<sed::TVector>& loop)
        {
            std::vector<std::int32_t> indices;
            indices.reserve(loop.size());
            for (const auto& point : loop)
            {
                indices.push_back(generated.newVertex(point.x, point.y, point.z));
            }
            return indices;
        };
        const auto inner = appendLoop(wire->innerLoop);
        const auto outer = appendLoop(wire->outerLoop);
        const auto appendGeneratedFace = [&](std::vector<std::int32_t> corners)
        {
            sed::Surface generatedFace;
            generatedFace.num = static_cast<std::int32_t>(generated.surfaces.size());
            generatedFace.vertices = std::move(corners);
            generatedFace.texVertices.resize(generatedFace.vertices.size());
            generatedFace.normal = sed::level::surfaceCalcNormalNewell(generated, generatedFace);
            generated.addSurface(std::move(generatedFace));
        };
        appendGeneratedFace(inner);
        appendGeneratedFace(outer);
        for (std::size_t edge = 0; edge < inner.size(); ++edge)
        {
            const auto next = (edge + 1) % inner.size();
            const auto outerEdge = outer.size() - 1 - edge;
            const auto outerNext = outer.size() - 1 - next;
            appendGeneratedFace({inner[next], inner[edge], outer[outerEdge], outer[outerNext]});
        }
        level.sectors.push_back(std::move(generated));
        PartitionRouteProbe routeProbe;
        const bool accepted = sed::level::precision::connectExtrudeSurfacesChecked(
            level,
            {0, 0},
            {1, 0},
            nullptr,
            rejectSinglePortalThenAcceptPartition,
            &routeProbe);
        check(accepted && routeProbe.calls > 0
                && routeProbe.acceptedMultipartitionCandidates > 0
                && routeProbe.lastPortalCount >= 2
                && routeProbe.rejectedSinglePortalCandidates
                    + routeProbe.acceptedMultipartitionCandidates
                    == static_cast<std::size_t>(routeProbe.calls),
            "Expanded Extrude: checked validator accepts only a candidate with multipartition portal coverage");
        if (!accepted)
        {
            continue;
        }
        check(level.sectors.size() == 2 && allAdjoinBoundariesMatchWithSubdivisions(level),
            "Expanded Extrude: alternating-chain cells retain every reciprocal boundary");
        if ( level.sectors.size() != 2 )
        {
            continue;
        }
        check(level.sectors[0].vertices.size() > originalVertices.size(),
            "Expanded Extrude: multipartition route appends at least one fixed source corner");
        if ( level.sectors[0].vertices.size() >= originalVertices.size() )
        {
            for ( std::size_t index = 0; index < originalVertices.size(); ++index )
            {
                const auto& before = originalVertices[index];
                const auto& after = level.sectors[0].vertices[index];
                check(before.x == after.x && before.y == after.y && before.z == after.z && before.mark == after.mark,
                    "Expanded Extrude: cell construction preserves each original source coordinate and mark");
            }
        }
        if ( checkAffineSource )
        {
            std::size_t checkedNewCorners = 0;
            const auto& source = level.sectors[0];
            for ( std::size_t faceIndex = 0; faceIndex < source.surfaces.size(); ++faceIndex )
            {
                if ( faceIndex != 0 && faceIndex < originalFaceCount )
                {
                    continue;
                }
                const auto& face = source.surfaces[faceIndex];
                check(face.vertices.size() == face.texVertices.size(),
                    "Arrangement source UV: every ear descendant has complete tuples");
                if ( face.vertices.size() != face.texVertices.size() )
                {
                    continue;
                }
                for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                {
                    const auto index = face.vertices[corner];
                    const bool valid = index >= 0 && static_cast<std::size_t>(index) < source.vertices.size();
                    check(valid,"Arrangement source UV: indices are valid before fixed affine oracle reads");
                    if ( !valid )
                    {
                        continue;
                    }
                    const auto& point = source.vertices[index];
                    const auto& tuple = face.texVertices[corner];
                    check(tuple.u == static_cast<float>(2*point.x+1)
                            && tuple.v == static_cast<float>(3-point.x)
                            && tuple.color == sed::ColorF{0.25f,0.5f,0.75f,1.0f},
                        "Arrangement source UV: all ears share exact fixed affine U/V and RGBA at each inserted corner");
                    checkedNewCorners += static_cast<std::size_t>(index) >= originalVertices.size() ? 1 : 0;
                }
            }
            check(checkedNewCorners > 0,"Arrangement source UV: fixture exercises inserted corners across ear descendants");
        }
        if ( piecewiseUv == 1.0 )
        {
            // This fixed fan is the independent triangulation of the listed
            // seven-corner polygon. No output or production helper supplies
            // its triangle indices, source values, or interpolation arithmetic.
            const std::array<std::array<std::size_t,3>,5> ears{{
                {6,0,1},{6,1,2},{6,2,3},{6,3,4},{4,5,6}}};
            const auto weights = [&](const auto& ear, const sed::Vertex& point)
            {
                // Registry nodes on an original edge retain that incidence
                // even if their stored x/y components round independently.
                // Certify it independently from the fixed source endpoints;
                // do not invent a negative third weight from that roundoff.
                std::optional<std::array<long double,3>> edgeWeights;
                bool ambiguousEdge = false;
                for ( std::size_t edge = 0; edge < 3; ++edge )
                {
                    const auto next = (edge+1)%3;
                    const auto& edgeA = footprint[ear[edge]];
                    const auto& edgeB = footprint[ear[next]];
                    if ( point.x == edgeA.x && point.y == edgeA.y )
                    {
                        std::array<long double,3> exact{};
                        exact[edge] = 1;
                        return exact;
                    }
                    const long double dx = static_cast<long double>(edgeB.x)-edgeA.x;
                    const long double dy = static_cast<long double>(edgeB.y)-edgeA.y;
                    const long double px = static_cast<long double>(point.x)-edgeA.x;
                    const long double py = static_cast<long double>(point.y)-edgeA.y;
                    const long double cross = dx*py-dy*px;
                    const long double error = 16*std::numeric_limits<double>::epsilon()
                        * (std::abs(dx)*(std::abs(point.y)+std::abs(edgeA.y))
                            + std::abs(dy)*(std::abs(point.x)+std::abs(edgeA.x))
                            + std::abs(dx*py)+std::abs(dy*px));
                    if ( std::abs(cross) > error )
                    {
                        continue;
                    }
                    const bool useX = std::abs(dx) >= std::abs(dy);
                    const long double length = useX ? dx : dy;
                    const long double fromFirst = useX ? px : py;
                    const long double fromSecond = useX
                        ? static_cast<long double>(edgeB.x)-point.x
                        : static_cast<long double>(edgeB.y)-point.y;
                    std::array<long double,3> candidate{};
                    if ( std::abs(fromFirst) <= std::abs(fromSecond) )
                    {
                        candidate[next] = fromFirst/length;
                        candidate[edge] = 1-candidate[next];
                    }
                    else
                    {
                        candidate[edge] = fromSecond/length;
                        candidate[next] = 1-candidate[edge];
                    }
                    if ( candidate[edge] <= 0 || candidate[next] <= 0 )
                    {
                        continue;
                    }
                    ambiguousEdge = ambiguousEdge || edgeWeights.has_value();
                    edgeWeights = candidate;
                }
                if ( edgeWeights && !ambiguousEdge )
                {
                    return *edgeWeights;
                }
                const auto& a = footprint[ear[0]];
                const auto& b = footprint[ear[1]];
                const auto& c = footprint[ear[2]];
                const long double bx = static_cast<long double>(b.x)-a.x;
                const long double by = static_cast<long double>(b.y)-a.y;
                const long double cx = static_cast<long double>(c.x)-a.x;
                const long double cy = static_cast<long double>(c.y)-a.y;
                const long double px = static_cast<long double>(point.x)-a.x;
                const long double py = static_cast<long double>(point.y)-a.y;
                const long double determinant = bx*cy-by*cx;
                const long double v = (px*cy-py*cx)/determinant;
                const long double w = (bx*py-by*px)/determinant;
                return std::array<long double,3>{{1-v-w,v,w}};
            };
            const auto& source = level.sectors[0];
            std::size_t checkedNewCorners = 0;
            for ( std::size_t faceIndex = 0; faceIndex < source.surfaces.size(); ++faceIndex )
            {
                if ( faceIndex != 0 && faceIndex < originalFaceCount )
                {
                    continue;
                }
                const auto& face = source.surfaces[faceIndex];
                bool valid = face.vertices.size() >= 3 && face.vertices.size() == face.texVertices.size();
                for ( const auto index : face.vertices )
                {
                    valid = valid && index >= 0 && static_cast<std::size_t>(index) < source.vertices.size();
                }
                check(valid,"Piecewise source UV: complete valid face data precedes every oracle read");
                if ( !valid )
                {
                    continue;
                }
                auto parent = ears.size();
                for ( std::size_t earIndex = 0; earIndex < ears.size(); ++earIndex )
                {
                    bool inside = true;
                    for ( const auto index : face.vertices )
                    {
                        const auto barycentric = weights(ears[earIndex],source.vertices[index]);
                        for ( const auto weight : barycentric )
                        {
                            inside = inside && std::isfinite(weight)
                                && weight >= -64*std::numeric_limits<double>::epsilon();
                        }
                    }
                    if ( inside )
                    {
                        parent = earIndex;
                        break;
                    }
                }
                check(parent < ears.size(),"Piecewise source UV: every output fragment belongs to a fixed original ear");
                if ( parent >= ears.size() )
                {
                    continue;
                }
                const auto& ear = ears[parent];
                for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                {
                    const auto index = face.vertices[corner];
                    const auto barycentric = weights(ear,source.vertices[index]);
                    const auto interpolate = [&](const float a, const float b, const float c)
                    {
                        return static_cast<float>(barycentric[0]*static_cast<long double>(a)
                            + barycentric[1]*static_cast<long double>(b)
                            + barycentric[2]*static_cast<long double>(c));
                    };
                    const float u = interpolate(fixedU[ear[0]],fixedU[ear[1]],fixedU[ear[2]]);
                    const float v = interpolate(fixedV[ear[0]],fixedV[ear[1]],fixedV[ear[2]]);
                    const auto& a = fixedColors[ear[0]];
                    const auto& b = fixedColors[ear[1]];
                    const auto& c = fixedColors[ear[2]];
                    const sed::ColorF rgba{interpolate(a.r,b.r,c.r),interpolate(a.g,b.g,c.g),
                        interpolate(a.b,b.b,c.b),interpolate(a.a,b.a,c.a)};
                    const auto& tuple = face.texVertices[corner];
                    check(tuple.u == u && tuple.v == v && tuple.color == rgba,
                        "Piecewise source UV: all propagated ear corners agree with fixed original U/V/RGBA vectors");
                    checkedNewCorners += static_cast<std::size_t>(index) >= originalVertices.size() ? 1 : 0;
                }
            }
            check(checkedNewCorners > 0,"Piecewise source UV: fixed vector exercises generated source corners");
        }
        if ( piecewiseUv == 2.0 )
        {
            const auto& source = level.sectors[0];
            std::map<std::int32_t,sed::TexVertex> firstTuples;
            std::size_t sharedCorners = 0, cancellationCorners = 0;
            for ( std::size_t faceIndex = 0; faceIndex < source.surfaces.size(); ++faceIndex )
            {
                if ( faceIndex != 0 && faceIndex < originalFaceCount )
                {
                    continue;
                }
                const auto& face = source.surfaces[faceIndex];
                bool valid = face.vertices.size() >= 3 && face.vertices.size() == face.texVertices.size();
                for ( const auto index : face.vertices )
                {
                    valid = valid && index >= 0 && static_cast<std::size_t>(index) < source.vertices.size();
                }
                check(valid,"Cancellation source UV: complete face data precedes shared-node and fixed-edge reads");
                if ( !valid )
                {
                    continue;
                }
                for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                {
                    const auto index = face.vertices[corner];
                    const auto& tuple = face.texVertices[corner];
                    if ( static_cast<std::size_t>(index) < originalVertices.size() )
                    {
                        continue;
                    }
                    const auto [first,inserted] = firstTuples.emplace(index,tuple);
                    if ( !inserted )
                    {
                        ++sharedCorners;
                        check(tuple.u == first->second.u && tuple.v == first->second.v
                                && tuple.color == first->second.color,
                            "Cancellation source UV: internal ear boundaries never acquire a float UV or RGBA seam");
                    }
                    const auto& point = source.vertices[index];
                    const double edgeError = 32*std::numeric_limits<double>::epsilon()
                        * (std::abs(point.x)+std::abs(point.y)+2);
                    if ( point.x <= -1 || point.x >= 1 || std::abs(point.x+point.y) > edgeError )
                    {
                        continue;
                    }
                    // Fixed original edge (-1,1)->(1,-1), with endpoint U/V
                    // 1 and float(-2e-7). The fused affine reference avoids
                    // cancellation in the oracle even when long double is double.
                    constexpr double endpoint = static_cast<double>(-2e-7f);
                    const float expected = static_cast<float>(std::fma((1-point.x)*0.5,1-endpoint,endpoint));
                    if ( std::abs(expected) < 1e-10f )
                    {
                        ++cancellationCorners;
                        check(tuple.u == expected && tuple.v == expected,
                            "Cancellation source UV: each near-zero shared-edge tuple matches the immutable fixed endpoint function");
                    }
                }
            }
            check(sharedCorners > 0 && cancellationCorners > 0,
                "Cancellation source UV: the fixed fixture exercises repeated shared nodes and near-zero edge interpolation");
        }
        for ( const auto& face : level.sectors[1].surfaces )
        {
            bool valid = face.vertices.size() >= 3 && face.vertices.size() == face.texVertices.size();
            for ( const auto index : face.vertices )
            {
                valid = valid && index >= 0 && static_cast<std::size_t>(index) < level.sectors[1].vertices.size();
            }
            check(valid, "Expanded Extrude: every alternating-chain generated face has complete valid corners");
            if ( !valid )
            {
                continue;
            }
            for ( std::size_t first = 0; first < face.vertices.size(); ++first )
            {
                const auto& a = level.sectors[1].vertices[face.vertices[first]];
                for ( std::size_t second = first + 1; second < face.vertices.size(); ++second )
                {
                    const auto& b = level.sectors[1].vertices[face.vertices[second]];
                    check(a.x != b.x || a.y != b.y || a.z != b.z,
                        "Expanded Extrude: generated cell boundaries contain no repeated coordinates or collapsed spikes");
                }
            }
        }
    }
    {
        const auto certificate = sed::level::detail::originalCornerProjection(
            {-1, 0, 0}, {0, 0, 0}, {1, 1e-6, 0}, {0, 0, 1}, {0, 0, 0});
        const sed::TVector error{1e-15, 1e-15, 1e-15};
        check(certificate.valid && certificate.matches({1e-9, 5e-16, 0}, error),
            "Corner certificate: two fixed near-parallel lines admit their independently bounded intersection error");
        using sed::level::detail::OriginalCornerMatchKind;
        using sed::level::detail::matchOriginalCornerProjections;
        std::vector<sed::level::detail::OriginalCornerProjection> candidates{{}, certificate};
        const auto unique = matchOriginalCornerProjections({1e-9, 0, 0}, candidates, error);
        check(unique.kind == OriginalCornerMatchKind::Unique && unique.index == 1,
            "Corner certificate: unique result identifies the original contour index");
        const auto absent = matchOriginalCornerProjections({0, 1, 0}, candidates, error);
        const auto empty = matchOriginalCornerProjections({0, 0, 0}, {}, error);
        check(absent.kind == OriginalCornerMatchKind::None && absent.index == 0
                && empty.kind == OriginalCornerMatchKind::None && empty.index == 0,
            "Corner certificate: missing and empty candidates return fixed empty results");
        candidates.push_back(certificate);
        const auto duplicate = matchOriginalCornerProjections({1e-9, 0, 0}, candidates, error);
        check(duplicate.kind == OriginalCornerMatchKind::Unique && duplicate.index == 1,
            "Corner certificate: repeated provenance for the exact same original corner stays unique");
        candidates.push_back(sed::level::detail::originalCornerProjection(
            {2e-9 - 1, 0, 0}, {2e-9, 0, 0}, {2e-9 + 1, 1e-6, 0}, {0, 0, 1}, {0, 0, 0}));
        const auto ambiguous = matchOriginalCornerProjections({1e-9, 0, 0}, candidates, error);
        check(ambiguous.kind == OriginalCornerMatchKind::Ambiguous && ambiguous.index == 0
                && candidates[1].original.x == 0 && candidates[3].original.x == 2e-9,
            "Corner certificate: a distinct nearby original corner absent from the current fragment rejects without mutation");
        check(!certificate.matches({1e-7, 0, 0}, error)
                && !certificate.matches({1e-9, 1e-10, 0}, error)
                && !certificate.matches({1e-9, 5e-16, 1e-6}, error),
            "Corner certificate: real tangential displacement, boundary gap, and off-plane displacement reject");
        const auto illConditioned = sed::level::detail::originalCornerProjection(
            {-1, 0, 0}, {0, 0, 0}, {1, 1e-20, 0}, {0, 0, 1}, {0, 0, 0});
        check(!illConditioned.matches({1e-9, 0, 0}, error),
            "Corner certificate: uncertainty spanning an original incident edge is never accepted");
        const double nan = std::numeric_limits<double>::quiet_NaN();
        check(!certificate.matches({nan, 0, 0}, error)
                && !certificate.matches({0, 0, 0}, {-1, 0, 0})
                && !certificate.matches({0, 0, 0}, {nan, 0, 0})
                && !sed::level::detail::originalCornerProjection({0, 0, 0}, {0, 0, 0},
                    {1, 0, 0}, {0, 0, 1}, {0, 0, 0}).valid
                && !sed::level::detail::originalCornerProjection({-1, 0, 0}, {0, 0, 0},
                    {1, 1e-6, 0}, {0, 0, 0}, {0, 0, 0}).valid,
            "Corner certificate: nonfinite values, invalid error bounds and degenerate input fail closed");
    }
    for ( const double expansion : {0.1, -0.1} )
    {
        sed::Level level;
        auto& source = level.newSector();
        const std::array<sed::TVector, 5> local{{{-1, -1, 0}, {1, -1, 0},
            {1.0000001, 0.4, 0.00002}, {1, 1, 0}, {-1, 1, 0}}};
        const sed::TVector u{1 / std::sqrt(2.0), 0, 1 / std::sqrt(2.0)};
        const sed::TVector v{-1 / std::sqrt(6.0), 2 / std::sqrt(6.0), 1 / std::sqrt(6.0)};
        const sed::TVector normal = u.cross(v);
        sed::Surface face;
        for ( const auto& point : local )
        {
            const auto world = sed::TVector{3, 2, 4} + u * point.x + v * point.y + normal * point.z;
            face.vertices.push_back(source.newVertex(world.x, world.y, world.z));
            sed::TexVertex tuple{};
            tuple.u = static_cast<float>(point.x + 2);
            tuple.v = static_cast<float>(point.y + 2);
            tuple.color = sed::ColorWhite;
            face.texVertices.push_back(tuple);
        }
        face.normal = sed::level::surfaceCalcNormalNewell(source, face);
        source.surfaces.push_back(face);
        const auto originalVertices = source.vertices;
        sed::level::SurfaceExtrudeReport report;
        const bool accepted = sed::level::levelExtrudeSurface(level, {0, 0}, 0.2, expansion, &report);
        check(accepted, "Expanded Extrude: oblique nearly parallel authored boundary edges retain their exact shared corner");
        if ( !accepted )
        {
            continue;
        }
        check(level.sectors.size() == 2 && allAdjoinBoundariesMatchByCoordinates(level),
            "Expanded Extrude: conditioned boundary intersections still produce exact reciprocal portal loops");
        if ( level.sectors.size() != 2 || level.sectors[0].vertices.size() < originalVertices.size() )
        {
            check(false, "Expanded Extrude: conditioned source owner and original pool survive");
            continue;
        }
        for ( std::size_t index = 0; index < originalVertices.size(); ++index )
        {
            const auto& before = originalVertices[index];
            const auto& after = level.sectors[0].vertices[index];
            check(before.x == after.x && before.y == after.y && before.z == after.z,
                "Expanded Extrude: conditioned matching never moves an original authored corner");
        }
    }
    for ( const double expansion : {0.1, -0.1} )
    {
        sed::Level level;
        sed::Sector source;
        const std::array<sed::TVector, 6> footprint{{
            {-1, -1, 0}, {-0.2, -0.99998, 0.00003}, {0.1, -1.00001, -0.000015},
            {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}}};
        for ( const auto& point : footprint )
        {
            source.newVertex(point.x, point.y, point.z);
        }
        for ( const auto& point : footprint )
        {
            source.newVertex(point.x, point.y, 1);
        }
        const auto appendFace = [&](std::vector<std::int32_t> vertices)
        {
            sed::Surface face;
            face.num = static_cast<std::int32_t>(source.surfaces.size());
            face.vertices = std::move(vertices);
            face.normal = sed::level::surfaceCalcNormalNewell(source, face);
            for ( const auto index : face.vertices )
            {
                sed::TexVertex tuple{};
                tuple.u = static_cast<float>((source.vertices[index].x + 2) / 4);
                tuple.v = static_cast<float>((source.vertices[index].y + 2) / 4);
                tuple.color = sed::ColorWhite;
                face.texVertices.push_back(tuple);
            }
            source.surfaces.push_back(std::move(face));
        };
        appendFace({0, 1, 2, 3, 4, 5});
        appendFace({11, 10, 9, 8, 7, 6});
        for ( std::int32_t edge = 0; edge < 6; ++edge )
        {
            const auto next = (edge + 1) % 6;
            appendFace({next, edge, edge + 6, next + 6});
        }
        source.id = 17;
        source.num = 0;
        level.curId = 18;
        level.sectors.push_back(source);
        sed::CogValue surfaceReference;
        surfaceReference.type = sed::ValueType::ptr;
        surfaceReference.cogType = sed::CogType::surface;
        surfaceReference.intValue = 0;
        surfaceReference.stringValue = "0 0";
        level.newCog().values.push_back(surfaceReference);
        sed::level::SurfaceExtrudeReport report;
        const bool accepted = sed::level::levelExtrudeSurface(level, {0, 0}, 0.2, expansion, &report);
        check(accepted, "Expanded Extrude: a rounded collinear-chain footprint retains all inherited bends");
        if ( !accepted )
        {
            continue;
        }
        check(level.sectors.size() == 2 && countAdjoinedSurfaces(level) >= 2
                && allAdjoinBoundariesMatchByCoordinates(level),
            "Expanded Extrude: every footprint fragment reconnects through exact reciprocal boundaries");
        if ( level.sectors.size() != 2 )
        {
            continue;
        }
        check(level.sectors[0].vertices.size() >= source.vertices.size(),
            "Expanded Extrude: original vertex pool remains available before exact comparisons");
        if ( level.sectors[0].vertices.size() < source.vertices.size() )
        {
            continue;
        }
        check(level.cogs.size() == 1 && level.cogs[0].values.size() == 1
                && level.cogs[0].values[0].intValue == 0 && level.cogs[0].values[0].stringValue == "0 0",
            "Expanded Extrude: selected Surface COG reference keeps its exact semantic owner and fixed text");
        for ( std::size_t index = 0; index < source.vertices.size(); ++index )
        {
            const auto& before = source.vertices[index];
            const auto& after = level.sectors[0].vertices[index];
            check(before.x == after.x && before.y == after.y && before.z == after.z && before.mark == after.mark,
                "Expanded Extrude: every original coordinate and vertex mark remains exact");
        }
        for ( const auto& sector : level.sectors )
        {
            std::map<std::pair<std::int32_t, std::int32_t>, int> incidence;
            for ( const auto& face : sector.surfaces )
            {
                bool valid = face.vertices.size() >= 3 && face.vertices.size() == face.texVertices.size();
                for ( const auto index : face.vertices )
                {
                    valid = valid && index >= 0 && static_cast<std::size_t>(index) < sector.vertices.size();
                }
                check(valid, "Expanded Extrude: all partitioned face indices and authored corner tuples are complete");
                if ( !valid )
                {
                    continue;
                }
                for ( std::size_t edge = 0; edge < face.vertices.size(); ++edge )
                {
                    ++incidence[{face.vertices[edge], face.vertices[(edge + 1) % face.vertices.size()]}];
                    const auto vertexIndex = static_cast<std::size_t>(face.vertices[edge]);
                    if ( &sector == &level.sectors[0] && vertexIndex < source.vertices.size() )
                    {
                        const auto& original = source.vertices[vertexIndex];
                        const auto& tuple = face.texVertices[edge];
                        check(tuple.u == static_cast<float>((original.x + 2) / 4)
                                && tuple.v == static_cast<float>((original.y + 2) / 4) && tuple.color == sed::ColorWhite,
                            "Expanded Extrude: every retained original corner keeps exact U/V and color across partitions");
                    }
                }
            }
            for ( const auto& [edge, count] : incidence )
            {
                check(count == 1 && incidence[{edge.second, edge.first}] == 1,
                    "Expanded Extrude: independent edge incidence proves both owners remain closed");
            }
        }
    }
    {
        const std::vector<std::int32_t> deleted{1, 3, 5};
        const std::array<std::int32_t, 8> expected{{0, -1, 1, -1, 2, -1, 3, 4}};
        for ( std::int32_t original = 0; original < static_cast<std::int32_t>(expected.size()); ++original )
        {
            check(sed::level::detail::remapSectorAfterDeletes(original, deleted) == expected[original],
                "Cleave remap: every surviving/deleted original index matches the fixed compacted-array vector");
        }
        check(sed::level::detail::remapSectorAfterDeletes(-1, deleted) == -1
                && sed::level::detail::remapSectorAfterDeletes(4, {}) == 4,
            "Cleave remap: invalid sentinel and empty removal list remain exact");
    }
    for ( const double length : {1.0e-6, 1.0e-8, 1.0e-10} )
    {
        for ( const bool edgeOnly : {false, true} )
        {
            sed::Level level;
            sed::Sector sector;
            sector.newVertex(0, 0, 0);
            sector.newVertex(length, 0, 0);
            sector.newVertex(length, length, 0);
            sector.newVertex(0, length, 0);
            sed::Surface face;
            face.normal = {0, 0, 1};
            face.vertices = {0, 1, 2, 3};
            face.texVertices.resize(4);
            for ( std::size_t corner = 0; corner < 4; ++corner )
            {
                const bool right = corner == 1 || corner == 2;
                const bool top = corner >= 2;
                face.texVertices[corner].u = right ? 6.0f : 2.0f;
                face.texVertices[corner].v = top ? 12.0f : 4.0f;
                face.texVertices[corner].color = right ? sed::ColorF{1, 0, 0, 1} : sed::ColorF{0, 0, 1, 1};
            }
            sector.surfaces.push_back(face);
            level.sectors.push_back(sector);
            const bool accepted = edgeOnly
                ? sed::level::levelCleaveEdge(level, {0, 0}, 0, {1, 0, 0}, length / 2, 0, 0)
                : sed::level::levelCleaveSurface(level, {0, 0}, {1, 0, 0}, length / 2, 0, 0);
            check(accepted, "Tiny Cleave: finite positive edges below the former squared-length cutoff split");
            if ( !accepted )
            {
                continue;
            }
            std::size_t insertedCorners = 0;
            check(level.sectors.size() == 1, "Tiny Cleave: surface and edge edits retain their owner");
            if ( level.sectors.size() != 1 )
            {
                continue;
            }
            for ( const auto& output : level.sectors[0].surfaces )
            {
                bool valid = output.vertices.size() == output.texVertices.size();
                for ( const auto index : output.vertices )
                {
                    valid = valid && index >= 0 && static_cast<std::size_t>(index) < level.sectors[0].vertices.size();
                }
                check(valid, "Tiny Cleave: every output vertex and UV corner can be inspected safely");
                if ( !valid )
                {
                    continue;
                }
                for ( std::size_t corner = 0; corner < output.vertices.size(); ++corner )
                {
                    const auto& point = level.sectors[0].vertices[output.vertices[corner]];
                    if ( point.x != length / 2 )
                    {
                        continue;
                    }
                    ++insertedCorners;
                    const auto& tuple = output.texVertices[corner];
                    check(tuple.u == 4 && tuple.v == (point.y == 0 ? 4 : 12)
                            && tuple.color.r == 0.5f && tuple.color.g == 0
                            && tuple.color.b == 0.5f && tuple.color.a == 1,
                        "Tiny Cleave: midpoint has fixed analytic U/V/RGBA in both Surface and Edge paths");
                }
            }
            check(insertedCorners == (edgeOnly ? 1u : 4u),
                "Tiny Cleave: exact number of inserted midpoint tuples is preserved");
        }
    }
    // Fixed intersecting boxes: union volume 64 + 8 - 4 = 68. Only the
    // knife's uncut exterior corner is rounded; the joining plane is x=4.
    for ( const auto mode : {sed::level::SectorConnectKnifeMode::KeepTargetPieces,
            sed::level::SectorConnectKnifeMode::CutTargetHolesOnly} )
    {
        for ( const double warp : {0.0, 0.00005, 0.01} )
        {
            const auto makeBox = [](double x0, double x1, double y0, double y1, double z0, double z1)
            {
                sed::Sector sector;
                const std::array<sed::TVector, 8> points{{{x0, y0, z0}, {x1, y0, z0},
                    {x1, y1, z0}, {x0, y1, z0}, {x0, y0, z1}, {x1, y0, z1},
                    {x1, y1, z1}, {x0, y1, z1}}};
                for ( const auto& point : points )
                {
                    sector.newVertex(point.x, point.y, point.z);
                }
                const std::array<std::array<std::int32_t, 4>, 6> loops{{
                    {{1, 5, 6, 2}}, {{0, 3, 7, 4}}, {{3, 2, 6, 7}},
                    {{0, 4, 5, 1}}, {{4, 7, 6, 5}}, {{0, 1, 2, 3}}}};
                for ( const auto& loop : loops )
                {
                    sed::Surface face;
                    face.vertices.assign(loop.begin(), loop.end());
                    sector.surfaces.push_back(std::move(face));
                }
                return sector;
            };
            sed::Level level;
            level.sectors.push_back(makeBox(3, 5, 1, 3, 1, 3));
            level.sectors.push_back(makeBox(0, 4, 0, 4, 0, 4));
            level.sectors[0].vertices[6].x += warp;
            for ( std::size_t owner = 0; owner < level.sectors.size(); ++owner )
            {
                auto& sector = level.sectors[owner];
                sector.num = static_cast<std::int32_t>(owner);
                sector.id = ++level.curId;
                for ( std::size_t index = 0; index < sector.surfaces.size(); ++index )
                {
                    auto& face = sector.surfaces[index];
                    face.num = static_cast<std::int32_t>(index);
                    const auto& origin = sector.vertices[face.vertices[0]];
                    sed::TVector area{};
                    for ( std::size_t corner = 1; corner + 1 < face.vertices.size(); ++corner )
                    {
                        const auto& b = sector.vertices[face.vertices[corner]];
                        const auto& c = sector.vertices[face.vertices[corner + 1]];
                        area = area + sed::TVector{b.x - origin.x, b.y - origin.y, b.z - origin.z}.cross(
                            {c.x - origin.x, c.y - origin.y, c.z - origin.z});
                    }
                    const double length = std::hypot(area.x, area.y, area.z);
                    face.normal = {area.x / length, area.y / length, area.z / length};
                    for ( const auto vertexIndex : face.vertices )
                    {
                        const auto& point = sector.vertices[vertexIndex];
                        sed::TexVertex tuple{};
                        tuple.u = static_cast<float>(point.x + 2 * point.y);
                        tuple.v = static_cast<float>(point.z + point.y);
                        tuple.color = sed::ColorWhite;
                        face.texVertices.push_back(tuple);
                    }
                }
            }
            const auto before = level;
            const auto bytes = sed::assets::saveToSedBytes(level);
            const bool accepted = sed::level::levelConnectSectorsKnife(level, 0, 1, mode);
            check(accepted == (warp < 0.001),
                "Rounded Join: both modes accept inherited exterior rounding and reject a real warp");
            if ( !accepted )
            {
                check(sed::assets::saveToSedBytes(level) == bytes && sed::assets::sedLevelsEqual(level, before),
                    "Rounded Join: rejection preserves complete authored state and native bytes");
                continue;
            }
            check(countAdjoinedSurfaces(level) >= 2 && allAdjoinBoundariesMatchByCoordinates(level),
                "Rounded Join: every portal has an exact reciprocal boundary");
            bool retainedCorner = false;
            long double totalVolume = 0;
            for ( const auto& sector : level.sectors )
            {
                std::map<std::pair<std::int32_t, std::int32_t>, int> edges;
                long double signedVolume = 0;
                for ( const auto& point : sector.vertices )
                {
                    retainedCorner = retainedCorner || (point.x == 5 + warp && point.y == 3 && point.z == 3);
                    const bool inTarget = point.x >= 0 && point.x <= 4 && point.y >= 0 && point.y <= 4
                        && point.z >= 0 && point.z <= 4;
                    const bool inKnife = point.x >= 3 && point.x <= 5 + warp && point.y >= 1 && point.y <= 3
                        && point.z >= 1 && point.z <= 3;
                    check(inTarget || inKnife, "Rounded Join: output lies inside the independently specified source union");
                }
                for ( const auto& face : sector.surfaces )
                {
                    bool valid = face.vertices.size() >= 3 && face.vertices.size() == face.texVertices.size();
                    for ( const auto index : face.vertices )
                    {
                        valid = valid && index >= 0 && static_cast<std::size_t>(index) < sector.vertices.size();
                    }
                    check(valid, "Rounded Join: output indices and every UV tuple are complete");
                    if ( !valid )
                    {
                        continue;
                    }
                    for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                    {
                        ++edges[{face.vertices[corner], face.vertices[(corner + 1) % face.vertices.size()]}];
                        const auto& point = sector.vertices[face.vertices[corner]];
                        if ( point.x == 5 + warp && point.y == 3 && point.z == 3 )
                        {
                            check(face.texVertices[corner].u == static_cast<float>(11 + warp)
                                    && face.texVertices[corner].v == 6 && face.texVertices[corner].color == sed::ColorWhite,
                                "Rounded Join: exact uncut exterior corner retains its authored UV and color");
                        }
                    }
                    const auto& a = sector.vertices[face.vertices[0]];
                    for ( std::size_t corner = 1; corner + 1 < face.vertices.size(); ++corner )
                    {
                        const auto& b = sector.vertices[face.vertices[corner]];
                        const auto& c = sector.vertices[face.vertices[corner + 1]];
                        signedVolume += static_cast<long double>(a.x) * (static_cast<long double>(b.y) * c.z - static_cast<long double>(b.z) * c.y)
                            + static_cast<long double>(a.y) * (static_cast<long double>(b.z) * c.x - static_cast<long double>(b.x) * c.z)
                            + static_cast<long double>(a.z) * (static_cast<long double>(b.x) * c.y - static_cast<long double>(b.y) * c.x);
                    }
                }
                for ( const auto& [edge, count] : edges )
                {
                    check(count == 1 && edges[{edge.second, edge.first}] == 1,
                        "Rounded Join: independent directed edge counts prove a closed body");
                }
                check(signedVolume < 0, "Rounded Join: output preserves inward winding and positive volume");
                totalVolume -= signedVolume / 6;
            }
            check(retainedCorner, "Rounded Join: exact original uncut corner survives");
            check(std::abs(totalVolume - 68) <= warp * 4 + 1e-9,
                "Rounded Join: analytic union volume68 changes by no more than the source displacement envelope");
        }
    }
    for ( int operation = 0; operation < 2; ++operation )
    {
        sed::Level baseline;
        baseline.sectors.push_back(makeLitTestCubeSector(0, ColorWhite));
        baseline.sectors[0].vertices[6].z += 0.00002;
        baseline.sectors[0].surfaces[4].normal = {0, 0, -1};
        const auto baselineBytes = sed::assets::saveToSedBytes(baseline);
        for ( int normalCase = 0; normalCase < 3; ++normalCase )
        {
            auto level = baseline;
            if ( normalCase == 1 )
            {
                level.sectors[0].surfaces[4].normal = {0, 0, 1};
            }
            else if ( normalCase == 2 )
            {
                level.sectors[0].surfaces[4].normal.x = std::numeric_limits<double>::quiet_NaN();
            }
            const auto before = level;
            const auto oldNormal = level.sectors[0].surfaces[4].normal;
            const bool accepted = operation == 0
                ? sed::level::levelCleaveSurface(level, {0, 4}, {1, 0, 0}, 0, 0, 0)
                : sed::level::levelCleaveSector(level, 0, {1, 0, 0}, 0, 0, 0);
            if ( normalCase == 0 )
            {
                check(accepted, "Rounded same-facing cached normals admit geometric-frame Surface and Sector Cleave");
                continue;
            }
            check(!accepted && level.sectors.size() == 1,
                "Opposed and nonfinite cached normals reject rounded Cleave");
            if ( level.sectors.size() != 1 || level.sectors[0].surfaces.size() != 6 )
            {
                check(false, "Invalid cached-normal rejection preserves topology cardinality");
                continue;
            }
            const auto actualNormal = level.sectors[0].surfaces[4].normal;
            check(std::bit_cast<std::uint64_t>(actualNormal.x) == std::bit_cast<std::uint64_t>(oldNormal.x)
                    && std::bit_cast<std::uint64_t>(actualNormal.y) == std::bit_cast<std::uint64_t>(oldNormal.y)
                    && std::bit_cast<std::uint64_t>(actualNormal.z) == std::bit_cast<std::uint64_t>(oldNormal.z)
                    && sameVertexVectors(level.sectors[0].vertices, before.sectors[0].vertices),
                "Cached-normal rejection preserves exact NaN bits, coordinates and runtime vertex marks");
            // Native serialization deliberately rejects NaN. Normalize only
            // the comparison copy after checking those original bits above.
            auto comparison = level;
            comparison.sectors[0].surfaces[4].normal = {0, 0, -1};
            check(sed::assets::saveToSedBytes(comparison) == baselineBytes,
                "Cached-normal rejection preserves every other native field exactly");
        }
    }
    {
        // Surface Cleave only edits the selected face and the edge/portal
        // faces whose topology must follow its new cut.  An unrelated face
        // can therefore retain an old cached normal or an invalid portal
        // reference without preventing the scoped operation from completing.
        sed::Level level;
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        for ( std::size_t surfaceIndex = 0;
              surfaceIndex < level.sectors[0].surfaces.size();
              ++surfaceIndex )
        {
            level.sectors[0].surfaces[surfaceIndex].num =
                static_cast<std::int32_t>(surfaceIndex);
        }
        sed::Surface& unrelated = level.sectors[0].surfaces[3];
        unrelated.normal = -unrelated.normal;
        unrelated.adjoin = sed::SurfaceRef{ 0, 99 };
        unrelated.material = "legacy-unresolved-material";
        unrelated.mark = 401;
        unrelated.d3dId = 402;
        unrelated.surfaceFlags = 403;
        unrelated.faceFlags = 404;
        unrelated.geo = 405;
        unrelated.light = 406;
        unrelated.tex = 407;
        unrelated.extraLight = { 0.125f, 0.25f, 0.5f, 0.75f };
        unrelated.adjoinFlags = 408;
        unrelated.nAdj = 409;
        unrelated.nMat = 410;
        unrelated.uScale = 1.25f;
        unrelated.vScale = -0.75f;
        unrelated.texVertices[0].u = 11.0f;
        unrelated.texVertices[0].v = -12.0f;
        unrelated.texVertices[0].color = { 0.1f, 0.2f, 0.3f, 0.4f };
        const sed::Surface unrelatedBefore = unrelated;
        std::vector<sed::Vertex> unrelatedVerticesBefore;
        unrelatedVerticesBefore.reserve(unrelated.vertices.size());
        for ( const auto index : unrelated.vertices )
        {
            unrelatedVerticesBefore.push_back(
                level.sectors[0].vertices[static_cast<std::size_t>(index)]);
        }
        const auto sameSurfaceRecord = [](const sed::Surface& lhs, const sed::Surface& rhs) noexcept
        {
            return lhs.num == rhs.num
                && lhs.mark == rhs.mark
                && lhs.d3dId == rhs.d3dId
                && lhs.surfaceFlags == rhs.surfaceFlags
                && lhs.faceFlags == rhs.faceFlags
                && lhs.geo == rhs.geo
                && lhs.light == rhs.light
                && lhs.tex == rhs.tex
                && lhs.extraLight == rhs.extraLight
                && lhs.adjoinFlags == rhs.adjoinFlags
                && lhs.adjoin == rhs.adjoin
                && lhs.material == rhs.material
                && lhs.nAdj == rhs.nAdj
                && lhs.nMat == rhs.nMat
                && lhs.uScale == rhs.uScale
                && lhs.vScale == rhs.vScale
                && lhs.normal == rhs.normal
                && lhs.vertices == rhs.vertices
                && sameTextureVertexVectors(lhs.texVertices, rhs.texVertices);
        };
        const auto sameUnrelatedVertexCoordinates = [&]() noexcept
        {
            if ( unrelatedBefore.vertices.size() != unrelatedVerticesBefore.size() )
            {
                return false;
            }
            for ( std::size_t corner = 0; corner < unrelatedBefore.vertices.size(); ++corner )
            {
                const auto index = unrelatedBefore.vertices[corner];
                if ( index < 0 || static_cast<std::size_t>(index) >= level.sectors[0].vertices.size() )
                {
                    return false;
                }
                const auto& actual = level.sectors[0].vertices[static_cast<std::size_t>(index)];
                const auto& expected = unrelatedVerticesBefore[corner];
                if ( actual.x != expected.x || actual.y != expected.y
                    || actual.z != expected.z || actual.mark != expected.mark )
                {
                    return false;
                }
            }
            return true;
        };
        const bool accepted = sed::level::levelCleaveSurface(
            level,
            sed::SurfaceRef{ 0, 2 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(accepted && level.sectors[0].surfaces.size() == 7,
            "Surface Cleave accepts a valid cut when an unrelated face has invalid cached data and a portal reference");
        if ( level.sectors.size() == 1 && level.sectors[0].surfaces.size() >= 4 )
        {
            check(sameSurfaceRecord(level.sectors[0].surfaces[3], unrelatedBefore)
                    && sameUnrelatedVertexCoordinates(),
                "Surface Cleave leaves every unrelated field, portal reference and vertex coordinate untouched");
        }
    }
    for ( const std::int32_t invalidSurface : { 2, 4, 5 } )
    {
        sed::Level level;
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        level.sectors[0].surfaces[static_cast<std::size_t>(invalidSurface)].normal =
            -level.sectors[0].surfaces[static_cast<std::size_t>(invalidSurface)].normal;
        const auto beforeBytes = sed::assets::saveToSedBytes(level);
        const bool accepted = sed::level::levelCleaveSurface(
            level,
            sed::SurfaceRef{ 0, 2 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        if ( invalidSurface == 2 )
        {
            check(!accepted && sed::assets::saveToSedBytes(level) == beforeBytes,
                "Surface Cleave rejects an opposed selected-face normal atomically");
        }
        else if ( invalidSurface == 4 )
        {
            check(!accepted && sed::assets::saveToSedBytes(level) == beforeBytes,
                "Surface Cleave rejects an opposed upper edge-neighbor normal atomically");
        }
        else
        {
            check(!accepted && sed::assets::saveToSedBytes(level) == beforeBytes,
                "Surface Cleave rejects an opposed lower edge-neighbor normal atomically");
        }
    }
    for ( const std::int32_t invalidSurface : { 2, 5 } )
    {
        sed::Level level;
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        level.sectors[0].surfaces[static_cast<std::size_t>(invalidSurface)].adjoin =
            sed::SurfaceRef{ 0, 99 };
        const auto beforeBytes = sed::assets::saveToSedBytes(level);
        const bool accepted = sed::level::levelCleaveSurface(
            level,
            sed::SurfaceRef{ 0, 2 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        if ( invalidSurface == 2 )
        {
            check(!accepted && sed::assets::saveToSedBytes(level) == beforeBytes,
                "Surface Cleave rejects an invalid selected-face portal reference atomically");
        }
        else
        {
            check(!accepted && sed::assets::saveToSedBytes(level) == beforeBytes,
                "Surface Cleave rejects an invalid edge-neighbor portal reference atomically");
        }
    }
    for ( const double scale : {1e-4, 1e-6} )
    {
        sed::Level warped;
        warped.sectors.push_back(makeLitTestCubeSector(0, ColorWhite));
        warped.sectors[0].vertices[6].z += 0.02;
        for ( auto& vertex : warped.sectors[0].vertices )
        {
            vertex.x *= scale;
            vertex.y *= scale;
            vertex.z *= scale;
        }
        for ( auto& face : warped.sectors[0].surfaces )
        {
            face.normal = sed::level::surfaceCalcNormalNewell(warped.sectors[0], face);
        }
        const auto before = sed::assets::saveToSedBytes(warped);
        check(!sed::level::levelCleaveSurface(warped, {0, 4}, {1, 0, 0}, 0, 0, 0)
                && sed::assets::saveToSedBytes(warped) == before,
            "Cleave input admission rejects material relative warp even when the entire solid is below legacy absolute tolerance");
        const std::array<sed::level::SectorCleavePlane, 1> planes{{{{1, 0, 0}, {0, 0, 0}}}};
        const auto failed = sed::level::levelCleaveSectorByPlanes(warped, 0, planes);
        check(!failed.succeeded() && failed.error == sed::level::SectorCleaveBatchError::InvalidInputGeometry
                && failed.failedPlane == -1 && failed.failedSector == 0 && failed.failedSurface == 4 && failed.outputSectors.empty()
                && sed::assets::saveToSedBytes(warped) == before,
            "Tiny materially warped solid returns input failure and complete rollback");

        sed::Level reflex;
        reflex.sectors.emplace_back();
        auto& sector = reflex.sectors[0];
        for ( const auto point : std::array<sed::TVector, 5>{{{0, 0, 0}, {1, 0, 0}, {0.6, 0.4, 0}, {1, 1, 0}, {0, 1, 0}}} )
        {
            sector.newVertex(point.x * scale, point.y * scale, 0);
        }
        sed::Surface face;
        face.vertices = {0, 1, 2, 3, 4};
        face.texVertices.resize(5);
        face.normal = {0, 0, 1};
        sector.surfaces.push_back(face);
        const auto reflexBefore = sed::assets::saveToSedBytes(reflex);
        check(!sed::level::levelCleaveSurface(reflex, {0, 0}, {1, 0, 0}, scale * 0.25, 0, 0)
                && sed::assets::saveToSedBytes(reflex) == reflexBefore,
            "Cleave input admission rejects material relative reflex depth in a tiny planar polygon");

        sector.vertices[0] = {0, 0, 0};
        sector.vertices[1] = {2 * scale, 2 * scale, 0};
        sector.vertices[2] = {0, 2 * scale, 0};
        sector.vertices[3] = {scale, 0, 0};
        sector.surfaces[0].vertices = {0, 1, 2, 3};
        sector.surfaces[0].texVertices.resize(4);
        const auto crossingBefore = sed::assets::saveToSedBytes(reflex);
        check(!sed::level::levelCleaveSurface(reflex, {0, 0}, {1, 0, 0}, scale * 0.5, 0, 0)
                && sed::assets::saveToSedBytes(reflex) == crossingBefore,
            "A tiny self-crossing source with nonzero signed area cannot acquire a certified cap or Surface fragment");
    }
    {
        // A nominally rectangular prism with one inward-rounded side corner.
        // Its exact cross-section has the same 0.00002 indentation; clipping
        // must preserve that inherited shape rather than flatten its vertices.
        sed::Level level;
        level.sectors.emplace_back();
        auto& sector = level.sectors[0];
        const std::array<std::pair<double, double>, 5> section{{
            {-1, -1}, {1, -1}, {1, 1}, {-1, 1}, {-0.99998, 0}}};
        for ( const double x : {-1.0, 1.0} )
        {
            for ( const auto& [y, z] : section )
            {
                sector.newVertex(x, y, z);
            }
        }
        sed::Surface left;
        left.vertices = {0, 1, 2, 3, 4};
        left.texVertices.resize(5);
        sector.surfaces.push_back(left);
        sed::Surface right;
        right.vertices = {9, 8, 7, 6, 5};
        right.texVertices.resize(5);
        sector.surfaces.push_back(right);
        for ( std::int32_t edge = 0; edge < 5; ++edge )
        {
            const auto next = (edge + 1) % 5;
            sed::Surface face;
            face.vertices = {edge, edge + 5, next + 5, next};
            face.texVertices.resize(4);
            sector.surfaces.push_back(face);
        }
        for ( auto& face : sector.surfaces )
        {
            face.normal = sed::level::surfaceCalcNormalNewell(sector, face);
        }
        const std::array<sed::level::SectorCleavePlane, 2> planes{{
            {{1, 0, 0}, {-0.5, 0, 0}}, {{1, 0, 0}, {0.5, 0, 0}}}};
        const auto result = sed::level::levelCleaveSectorByPlanes(level, 0, planes);
        check(result.succeeded() && result.outputSectors.size() == 3,
            "Certified cap construction preserves inherited rounded reflexes through repeated cuts");
        if ( result.succeeded() )
        {
            check(countAdjoinedSurfaces(level) == 4 && allAdjoinBoundariesMatchByCoordinates(level),
                "Certified inherited cap reflexes retain both complete reciprocal pairs");
            check(adjoinPolygonsHaveNoProjectedCrossings(level),
                "Certified inherited cap reflexes have no independent projected edge crossings");
            for ( const auto& body : level.sectors )
            {
                for ( const auto& face : body.surfaces )
                {
                    if ( !face.adjoin )
                    {
                        continue;
                    }
                    unsigned mask = 0;
                    bool pointsValid = face.vertices.size() == section.size();
                    for ( const auto index : face.vertices )
                    {
                        if ( index < 0 || static_cast<std::size_t>(index) >= body.vertices.size() )
                        {
                            pointsValid = false;
                            continue;
                        }
                        const auto& point = body.vertices[static_cast<std::size_t>(index)];
                        pointsValid = pointsValid && (point.x == -0.5 || point.x == 0.5);
                        for ( std::size_t corner = 0; corner < section.size(); ++corner )
                        {
                            if ( point.y == section[corner].first && point.z == section[corner].second )
                            {
                                mask |= 1u << corner;
                            }
                        }
                    }
                    check(pointsValid && mask == 31,
                        "Every certified cap corner equals an independent original edge and requested-plane intersection");
                    check(!sed::level::surfaceIsConvex(body, face),
                        "Rounded cap compatibility preserves the genuine inherited tiny reflex instead of claiming exact convexity");
                }
            }
        }
    }
    // Fixed rounded-text input: the top corner is 0.00002 above its ideal
    // plane. The operation may inherit that discrepancy, but every new cut
    // boundary must still lie on the authored plane and close both bodies.
    for ( const double origin : {0.0, 10000.0} )
    {
        for ( const bool portal : {false, true} )
        {
            for ( int mode = 0; mode < 4; ++mode )
            {
                for ( const double warp : {0.00002, 0.02} )
                {
                    sed::Level level;
                    level.layers = {"Rounded input"};
                    level.sectors.push_back(makeLitTestCubeSector(0, ColorWhite));
                    if ( portal )
                    {
                        level.sectors.push_back(makeLitTestCubeSector(0, ColorWhite));
                        for ( auto& vertex : level.sectors[1].vertices )
                        {
                            vertex.z += 2;
                        }
                        level.sectors[1].vertices[2].z = 1 + warp;
                        level.sectors[0].surfaces[4].adjoin = sed::SurfaceRef{1, 5};
                        level.sectors[1].surfaces[5].adjoin = sed::SurfaceRef{0, 4};
                        auto& mirror = level.sectors[1].surfaces[5].vertices;
                        std::rotate(mirror.begin(), mirror.begin() + 1, mirror.end());
                    }
                    level.sectors[0].vertices[6].z = 1 + warp;
                    for ( std::size_t owner = 0; owner < level.sectors.size(); ++owner )
                    {
                        auto& sector = level.sectors[owner];
                        sector.num = static_cast<std::int32_t>(owner);
                        sector.id = 20 + sector.num;
                        for ( auto& vertex : sector.vertices )
                        {
                            vertex.x += origin;
                            vertex.y += origin;
                            vertex.z += origin;
                        }
                        for ( std::size_t face = 0; face < sector.surfaces.size(); ++face )
                        {
                            auto& surface = sector.surfaces[face];
                            surface.nMat = static_cast<std::int32_t>(owner * 6 + face);
                            surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
                            for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
                            {
                                const auto& point = sector.vertices[surface.vertices[corner]];
                                auto& uv = surface.texVertices[corner];
                                uv.u = static_cast<float>(3 * (point.x - origin) + 5 * (point.y - origin) + 7 * (point.z - origin));
                                uv.v = static_cast<float>(11 * (point.x - origin) - 2 * (point.y - origin) + (point.z - origin));
                                uv.color = {0.25f, 0.5f, 0.75f, 1.0f};
                            }
                        }
                    }
                    level.curId = 22;
                    const sed::Level original = level;
                    const auto originalBytes = sed::assets::saveToSedBytes(level);
                    const std::array<sed::level::SectorCleavePlane, 2> planes{{
                        {{1, 0, 0}, {origin, origin, origin}},
                        {{0, 1, 0}, {origin, origin, origin}}}};
                    bool accepted = false;
                    if ( mode == 0 )
                    {
                        accepted = sed::level::levelCleaveSurface(level, {0, 4}, planes[0].normal, origin, origin, origin);
                    }
                    else if ( mode == 1 )
                    {
                        accepted = sed::level::levelCleaveSector(level, 0, planes[0].normal, origin, origin, origin);
                    }
                    else
                    {
                        const auto result = sed::level::levelCleaveSectorByPlanes(level, 0,
                            std::span<const sed::level::SectorCleavePlane>(planes.data(), mode == 2 ? 1 : 2));
                        accepted = result.succeeded();
                        if ( accepted )
                        {
                            check(result.error == sed::level::SectorCleaveBatchError::None
                                    && result.failedPlane == -1 && result.failedSector == -1 && result.failedSurface == -1
                                    && result.outputSectors.size() == (mode == 2 ? 2 : 4),
                                "Rounded Cleave batch returns every descendant and clears all failure outputs");
                        }
                        else
                        {
                            check(result.outputSectors.empty(), "Rejected rounded Cleave batch returns no descendants");
                        }
                    }
                    if ( warp == 0.02 )
                    {
                        check(!accepted && sed::assets::saveToSedBytes(level) == originalBytes,
                            "Cleave rejects real input deformation atomically in Surface, Single and Multi-cut modes");
                        continue;
                    }
                    check(accepted, "Cleave accepts closed rounded convex input in Surface, Single and Multi-cut modes");
                    if ( !accepted )
                    {
                        continue;
                    }
                    const std::size_t pieces = mode == 0 ? 1 : (mode == 3 ? 4 : 2);
                    check(level.sectors.size() == pieces + (portal ? 1 : 0),
                        "Rounded Cleave creates the exact expected number of closed bodies");
                    check(allAdjoinsAreReciprocal(level) && allAdjoinBoundariesMatchByCoordinates(level),
                        "Rounded Cleave preserves complete coincident reciprocal portal boundaries");
                    check(adjoinPolygonsHaveNoProjectedCrossings(level),
                        "Rounded Cleave caps and preserved portals have no independent projected edge crossings");
                    const std::size_t links = (mode == 0 ? 0 : (mode == 3 ? 8 : 2))
                        + (portal ? (mode == 3 ? 8 : 4) : 0);
                    check(countAdjoinedSurfaces(level) == links,
                        "Rounded Cleave creates all expected portal and cap links without silently dropping any");
                    bool outputsValid = true;
                    for ( const auto& sector : level.sectors )
                    {
                        std::map<std::pair<std::int32_t, std::int32_t>, int> edges;
                        bool valid = true;
                        for ( const auto& surface : sector.surfaces )
                        {
                            valid = valid && surface.vertices.size() >= 3 && surface.vertices.size() == surface.texVertices.size();
                            for ( const auto index : surface.vertices )
                            {
                                valid = valid && index >= 0 && static_cast<std::size_t>(index) < sector.vertices.size();
                            }
                        }
                        check(valid, "Rounded Cleave outputs complete in-range corner arrays");
                        outputsValid = outputsValid && valid;
                        if ( !valid )
                        {
                            continue;
                        }
                        for ( const auto& surface : sector.surfaces )
                        {
                            bool capX = mode != 0;
                            bool capY = mode == 3;
                            for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
                            {
                                const auto a = surface.vertices[corner];
                                const auto b = surface.vertices[(corner + 1) % surface.vertices.size()];
                                ++edges[{a, b}];
                                capX = capX && sector.vertices[a].x == origin;
                                capY = capY && sector.vertices[a].y == origin;
                            }
                            if ( capX || capY )
                            {
                                const auto& n = surface.normal;
                                check((capX && std::abs(n.x) == 1 && n.y == 0 && n.z == 0)
                                        || (capY && std::abs(n.y) == 1 && n.x == 0 && n.z == 0),
                                    "Rounded Cleave new caps lie exactly on the requested axial planes");
                                for ( const auto& point : sector.vertices )
                                {
                                    check(n.x * (point.x - origin) + n.y * (point.y - origin) + n.z * (point.z - origin) >= 0,
                                        "Rounded Cleave every body obeys each new cap halfspace without inherited tolerance");
                                }
                            }
                        }
                        for ( const auto& [edge, count] : edges )
                        {
                            check(count == 1 && edges[{edge.second, edge.first}] == 1,
                                "Rounded Cleave independent directed edges certify closure");
                        }
                    }
                    if ( !outputsValid )
                    {
                        continue;
                    }
                    for ( const auto& sector : original.sectors )
                    {
                        for ( const auto& surface : sector.surfaces )
                        {
                            for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
                            {
                                const auto& point = sector.vertices[surface.vertices[corner]];
                                bool found = false;
                                for ( const auto& output : level.sectors )
                                {
                                    for ( const auto& face : output.surfaces )
                                    {
                                        if ( face.nMat != surface.nMat )
                                        {
                                            continue;
                                        }
                                        for ( std::size_t index = 0; index < face.vertices.size(); ++index )
                                        {
                                            const auto& actual = output.vertices[face.vertices[index]];
                                            if ( actual.x == point.x && actual.y == point.y && actual.z == point.z )
                                            {
                                                found = true;
                                                check(sameTexVertex(face.texVertices[index], surface.texVertices[corner]),
                                                    "Rounded Cleave preserves exact original UV and color tuples");
                                            }
                                        }
                                    }
                                }
                                check(found, "Rounded Cleave preserves every original authored corner coordinate");
                            }
                        }
                    }
                }
            }
        }
    }
    for ( const double origin : {0.0, 10000.0} )
    {
        sed::Level original;
        original.sectors.push_back(makeLitTestCubeSector(0, ColorWhite));
        original.sectors[0].vertices[6].z += 0.00002;
        for ( auto& vertex : original.sectors[0].vertices )
        {
            vertex.x += origin;
            vertex.y += origin;
            vertex.z += origin;
        }
        for ( auto& face : original.sectors[0].surfaces )
        {
            face.normal = sed::level::surfaceCalcNormalNewell(original.sectors[0], face);
        }
        const std::array<sed::level::SectorCleavePlane, 3> planes{{
            {{1, 0, 0}, {origin, origin, origin}},
            {{1, 0, 0}, {origin + 0.5, origin, origin}},
            {{1, 1, 0}, {origin, origin, origin}}}};
        auto level = original;
        const auto result = sed::level::levelCleaveSectorByPlanes(level, 0, planes);
        check(result.succeeded() && result.outputSectors.size() == 6
                && result.error == sed::level::SectorCleaveBatchError::None
                && result.failedPlane == -1 && result.failedSector == -1 && result.failedSurface == -1,
            "Rounded Cleave repeated parallel and oblique cuts produce six descendants from one immutable input");
        if ( result.succeeded() )
        {
            check(allAdjoinsAreReciprocal(level) && allAdjoinBoundariesMatchByCoordinates(level),
                "Rounded oblique Multi-cut preserves complete mirrored cap boundaries");
            check(adjoinPolygonsHaveNoProjectedCrossings(level),
                "Repeated oblique cap boundaries have no independent projected edge crossings");
            for ( const auto& sector : level.sectors )
            {
                for ( const auto& plane : planes )
                {
                    bool positive = false;
                    bool negative = false;
                    for ( const auto& vertex : sector.vertices )
                    {
                        const long double distance = static_cast<long double>(plane.normal.x) * (static_cast<long double>(vertex.x) - plane.point.x)
                            + static_cast<long double>(plane.normal.y) * (static_cast<long double>(vertex.y) - plane.point.y);
                        const double error = 128 * std::numeric_limits<double>::epsilon()
                            * (std::abs(plane.normal.x) * (std::abs(vertex.x) + std::abs(plane.point.x))
                                + std::abs(plane.normal.y) * (std::abs(vertex.y) + std::abs(plane.point.y)));
                        positive = positive || distance > error;
                        negative = negative || distance < -error;
                    }
                    check(!(positive && negative),
                        "Rounded repeated cuts never let inherited error cross any previously requested halfspace");
                }
                for ( const auto& face : sector.surfaces )
                {
                    if ( !face.adjoin )
                    {
                        continue;
                    }
                    bool requestedPlane = false;
                    for ( const auto& plane : planes )
                    {
                        bool onPlane = true;
                        for ( const auto index : face.vertices )
                        {
                            if ( index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size() )
                            {
                                onPlane = false;
                                break;
                            }
                            const auto& vertex = sector.vertices[static_cast<std::size_t>(index)];
                            const long double distance = static_cast<long double>(plane.normal.x) * (static_cast<long double>(vertex.x) - plane.point.x)
                                + static_cast<long double>(plane.normal.y) * (static_cast<long double>(vertex.y) - plane.point.y);
                            const double error = 128 * std::numeric_limits<double>::epsilon()
                                * (std::abs(plane.normal.x) * (std::abs(vertex.x) + std::abs(plane.point.x))
                                    + std::abs(plane.normal.y) * (std::abs(vertex.y) + std::abs(plane.point.y)));
                            onPlane = onPlane && std::abs(distance) <= error;
                        }
                        requestedPlane = requestedPlane || onPlane;
                    }
                    check(requestedPlane, "Every rounded oblique Multi-cut cap retains a requested plane with only arithmetic error");
                }
            }
        }

        // Touching a vertex, touching a complete edge, an existing boundary
        // plane, and a disjoint plane all leave the original Level unchanged.
        const std::array<sed::level::SectorCleavePlane, 4> noSplit{{
            {{1, 1, 1}, {origin - 1, origin - 1, origin - 1}},
            {{1, 1, 0}, {origin - 1, origin - 1, origin}},
            {{1, 0, 0}, {origin - 1, origin, origin}},
            {{1, 0, 0}, {origin - 2, origin, origin}}}};
        const auto originalBytes = sed::assets::saveToSedBytes(original);
        for ( const auto& plane : noSplit )
        {
            auto rejected = original;
            const auto failure = sed::level::levelCleaveSectorByPlanes(rejected, 0,
                std::span<const sed::level::SectorCleavePlane>(&plane, 1));
            check(!failure.succeeded() && failure.error == sed::level::SectorCleaveBatchError::PlaneDoesNotSplit
                    && failure.failedPlane == 0 && failure.failedSector == 0 && failure.failedSurface == -1
                    && failure.outputSectors.empty() && sed::assets::saveToSedBytes(rejected) == originalBytes,
                "Rounded Cleave touching and disjoint planes return complete fixed failure fields without mutation");
        }
    }
    for ( const double warp : {0.0, 0.00002} )
    {
        for ( int scenario = 0; scenario < 12; ++scenario )
        {
            const int mirrorRotation = scenario % 4;
            const int invalidMirror = scenario / 4;
            sed::Level level;
            level.layers = {"Warped portal rollback"};
            level.sectors = {makeLitTestCubeSector(0, ColorWhite), makeLitTestCubeSector(0, ColorWhite)};
            for ( auto& vertex : level.sectors[1].vertices )
            {
                vertex.z += 2;
            }
            level.sectors[0].vertices[6].z = level.sectors[1].vertices[2].z = 1 + warp;
            const double normalLength = std::sqrt(1 + warp * warp / 8);
            auto& source = level.sectors[0].surfaces[4];
            auto& mirror = level.sectors[1].surfaces[5];
            // Independent analytic normals of the two oppositely wound warped
            // loops, matching how rounded text geometry enters the document.
            source.normal = {warp / (4 * normalLength), warp / (4 * normalLength), -1 / normalLength};
            mirror.normal = -source.normal;
            std::rotate(mirror.vertices.begin(), mirror.vertices.begin() + mirrorRotation, mirror.vertices.end());
            source.adjoin = sed::SurfaceRef{1, 5};
            mirror.adjoin = sed::SurfaceRef{0, 4};
            check(allAdjoinsAreReciprocal(level) && allAdjoinBoundariesMatchByCoordinates(level),
                "Surface Cleave portal fixture starts with complete coincident reciprocal boundaries");
            if ( invalidMirror == 1 )
            {
                mirror.adjoin = sed::SurfaceRef{0, 0};
            }
            else if ( invalidMirror == 2 )
            {
                mirror.vertices.pop_back();
                mirror.texVertices.pop_back();
            }
            for ( std::size_t owner = 0; owner < level.sectors.size(); ++owner )
            {
                auto& sector = level.sectors[owner];
                sector.mark = 100 + static_cast<std::int32_t>(owner);
                for ( auto& vertex : sector.vertices )
                {
                    vertex.mark = 103;
                }
                for ( auto& face : sector.surfaces )
                {
                    face.mark = 107;
                    face.d3dId = 109;
                    face.nAdj = 113;
                    face.nMat = 127;
                }
            }
            const sed::Level before = level;
            const auto beforeBytes = sed::assets::saveToSedBytes(level);
            const bool accepted = sed::level::levelCleaveSurface(level, {0, 4}, {1, 0, 0}, 0, 0, 0);
            if ( invalidMirror != 0 )
            {
                check(!accepted, invalidMirror == 1
                    ? "Surface Cleave rejects a mismatched mirror backlink"
                    : "Surface Cleave rejects an incomplete mirrored boundary");
            }
            else if ( warp == 0 )
            {
                check(accepted, "Surface Cleave planar portal succeeds for every mirrored first-corner rotation");
            }
            if ( accepted )
            {
                check(level.sectors.size() == 2 && level.sectors[0].surfaces.size() == 7
                        && level.sectors[1].surfaces.size() == 7 && countAdjoinedSurfaces(level) == 4
                        && allAdjoinsAreReciprocal(level) && allAdjoinBoundariesMatchByCoordinates(level),
                    "Surface Cleave success preserves both complete reciprocal portal pairs on rounded input");
            }
            else
            {
                check(sed::assets::saveToSedBytes(level) == beforeBytes,
                    "Surface Cleave unreconnectable portal rejects without any authored Level mutation");
                bool runtimeUnchanged = level.sectors.size() == before.sectors.size();
                for ( std::size_t owner = 0; runtimeUnchanged && owner < level.sectors.size(); ++owner )
                {
                    const auto& a = level.sectors[owner];
                    const auto& b = before.sectors[owner];
                    runtimeUnchanged = a.id == b.id && a.num == b.num && a.mark == b.mark
                        && sameVertexVectors(a.vertices, b.vertices) && a.surfaces.size() == b.surfaces.size();
                    for ( std::size_t face = 0; runtimeUnchanged && face < a.surfaces.size(); ++face )
                    {
                        const auto& x = a.surfaces[face];
                        const auto& y = b.surfaces[face];
                        runtimeUnchanged = x.mark == y.mark && x.d3dId == y.d3dId && x.nAdj == y.nAdj && x.nMat == y.nMat
                            && std::bit_cast<std::uint64_t>(x.normal.x) == std::bit_cast<std::uint64_t>(y.normal.x)
                            && std::bit_cast<std::uint64_t>(x.normal.y) == std::bit_cast<std::uint64_t>(y.normal.y)
                            && std::bit_cast<std::uint64_t>(x.normal.z) == std::bit_cast<std::uint64_t>(y.normal.z);
                    }
                }
                check(runtimeUnchanged, "Surface Cleave rejection restores marks, renderer IDs and cached normal bits");
            }
        }
    }
    for ( const double normalScale : {1.0, 1e-200, 1e200} )
    {
        sed::Level level{};
        level.layers = {"GeometryContractLayer"};
        const auto sourceIndex = sed::level::levelCreateSectorPrimitiveBox(level,
            sed::level::SectorPrimitive::Cube, {0, 0, 0}, {1, 1, 1});
        check(sourceIndex == 0, "cleave contract: fixed unit-cube fixture created at index zero");
        if ( sourceIndex != 0 )
        {
            continue;
        }
        level.sectors[0].layer = 0;
        const std::array<sed::level::SectorCleavePlane, 1> planes{{{{normalScale, 0, 0}, {0.5, 0, 0}}}};
        const auto result = sed::level::levelCleaveSectorByPlanes(level, 0, planes);
        check(result.succeeded() && result.error == sed::level::SectorCleaveBatchError::None,
            "cleave contract: every finite nonzero normal scale represents the same splitting plane");
        check(result.failedPlane == -1 && result.failedSector == -1 && result.failedSurface == -1,
            "cleave contract: success clears every failure-detail output");
        check(result.outputSectors == std::vector<std::int32_t>{0, 1} && level.sectors.size() == 2,
            "cleave contract: success returns the retained source followed by the appended descendant");
        double volume = 0.0;
        for ( const auto& sector : level.sectors )
        {
            double low = 1.0, high = 0.0;
            bool coordinates = true;
            for ( const auto& vertex : sector.vertices )
            {
                low = std::min(low, vertex.x);
                high = std::max(high, vertex.x);
                coordinates = coordinates && (vertex.x == 0.0 || vertex.x == 0.5 || vertex.x == 1.0)
                    && (vertex.y == 0.0 || vertex.y == 1.0) && (vertex.z == 0.0 || vertex.z == 1.0);
            }
            volume += high - low;
            check(coordinates && high - low == 0.5 && sector.layer == 0,
                "cleave contract: each output has fixed half-cube coordinates, volume and layer");
        }
        check(volume == 1.0 && level.layers == std::vector<std::string>{"GeometryContractLayer"},
            "cleave contract: scale variants preserve analytic volume and exact authored layer text");
    }
    for ( const bool bounded : {false, true} )
    {
        sed::Level source{};
        source.layers = {"NoOpCleaveLayer"};
        check(sed::level::levelCreateSectorPrimitiveBox(source,
                  sed::level::SectorPrimitive::Cube, {0, 0, 0}, {1, 1, 1}) == 0,
            "Multi-cut no-op fixture creates its synthetic unit box");
        source.sectors[0].id = 71;
        source.sectors[0].layer = 0;
        auto& thing = source.newThing();
        thing.sector = 0;
        thing.x = 0.8;
        thing.y = 0.8;
        thing.z = 0.5;
        const std::array effectivePlanes{
            sed::level::SectorCleavePlane{{1, 0, 0}, {0.5, 0, 0}},
            sed::level::SectorCleavePlane{{0, 1, 0}, {0, 0.5, 0}},
        };
        const std::array effectiveStrokes{
            sed::level::SectorCleaveStroke{{0.5, 0, 0.5}, {0.5, 1, 0.5}},
            sed::level::SectorCleaveStroke{{0.25, 0.5, 0.5}, {0.75, 0.5, 0.5}},
        };
        sed::level::SectorCleaveBatchOptions options{};
        options.bounded = bounded;
        options.strokes = effectiveStrokes;
        auto expected = source;
        const auto expectedResult = sed::level::levelCleaveSectorByPlanes(
            expected, 0, effectivePlanes, options);
        check(expectedResult.succeeded() && expected.sectors.size() == 4,
            "Multi-cut no-op oracle has four exact unit-box descendants");
        if ( !expectedResult.succeeded() )
        {
            continue;
        }
        const auto expectedBytes = sed::assets::saveToSedBytes(expected);
        for ( const std::size_t insertion : {0u, 1u, 2u} )
        {
            std::vector<sed::level::SectorCleavePlane> planes(
                effectivePlanes.begin(), effectivePlanes.end());
            std::vector<sed::level::SectorCleaveStroke> strokes(
                effectiveStrokes.begin(), effectiveStrokes.end());
            planes.insert(planes.begin() + insertion, {{1, 0, 0}, {2, 0, 0}});
            strokes.insert(strokes.begin() + insertion, {{2, 0, 0.5}, {2, 1, 0.5}});
            options.strokes = strokes;
            auto actual = source;
            const auto result = sed::level::levelCleaveSectorByPlanes(actual, 0, planes, options);
            check(result.succeeded() && result.failedPlane == -1
                    && result.failedSector == -1 && result.failedSurface == -1
                    && result.outputSectors == expectedResult.outputSectors
                    && sed::assets::saveToSedBytes(actual) == expectedBytes
                    && allAdjoinsAreReciprocal(actual)
                    && allAdjoinBoundariesMatchByCoordinates(actual),
                "Multi-cut skips a non-splitting first, middle, or last plane with exact geometry, UVs, identities, Things and adjoins");
        }
        const std::array noOpPlanes{
            sed::level::SectorCleavePlane{{1, 0, 0}, {2, 0, 0}},
            sed::level::SectorCleavePlane{{0, 1, 0}, {0, 2, 0}},
        };
        const std::array noOpStrokes{
            sed::level::SectorCleaveStroke{{2, 0, 0.5}, {2, 1, 0.5}},
            sed::level::SectorCleaveStroke{{0, 2, 0.5}, {1, 2, 0.5}},
        };
        options.strokes = noOpStrokes;
        auto unchanged = source;
        const auto noOpResult = sed::level::levelCleaveSectorByPlanes(
            unchanged, 0, noOpPlanes, options);
        check(!noOpResult.succeeded()
                && noOpResult.error == sed::level::SectorCleaveBatchError::PlaneDoesNotSplit
                && noOpResult.failedPlane == 0 && noOpResult.outputSectors.empty()
                && sed::assets::saveToSedBytes(unchanged) == sed::assets::saveToSedBytes(source),
            "Multi-cut with no effective plane rejects without publishing geometry or output descendants");
    }

    {
        sed::Level source{};
        (void)sed::level::levelCreateCube(source, 0, 0, 0, sed::TVector::up, sed::TVector::right);
        const std::array planes{
            sed::level::SectorCleavePlane{{1, 0, 0}, {0.5, 0, 0}},
            sed::level::SectorCleavePlane{{1, 1, 0}, {0.75, -0.5, 0}},
            sed::level::SectorCleavePlane{{0, 1, 0}, {0, 0.5, 0}},
        };
        const std::array strokes{
            sed::level::SectorCleaveStroke{{0.5, 0, 0.5}, {0.5, 1, 0.5}},
            sed::level::SectorCleaveStroke{{0.75, -0.5, 0}, {0.75, -0.5, 1}},
            sed::level::SectorCleaveStroke{{0.25, 0.5, 0}, {0.25, 0.5, 1}},
        };
        const std::array effectivePlanes{planes[0], planes[2]};
        const std::array effectiveStrokes{strokes[0], strokes[2]};
        sed::level::SectorCleaveBatchOptions options{};
        options.bounded = true;
        options.strokes = effectiveStrokes;
        auto expected = source;
        const auto oracle = sed::level::levelCleaveSectorByPlanes(expected, 0, effectivePlanes, options);
        options.strokes = strokes;
        const auto actual = sed::level::levelCleaveSectorByPlanes(source, 0, planes, options);
        check(oracle.succeeded() && expected.sectors.size() == 3 && actual.succeeded()
                && actual.outputSectors == oracle.outputSectors
                && sed::assets::saveToSedBytes(source) == sed::assets::saveToSedBytes(expected),
            "Bounded Multi-cut skips a plane whose stroke reaches only a non-splitting child without adding false ancestry");
    }

    for ( const bool closedShape : {false, true} )
    {
        for ( const bool bounded : {false, true} )
        {
            sed::Level source{};
            (void)sed::level::levelCreateSectorPrimitiveBox(source,
                sed::level::SectorPrimitive::Cube, {-1, -1, 0}, {1, 1, 1});
            // Synthetic camera-plane Rectangle near the upper outer corner:
            // one edge's plane misses the box, while the other three cross it.
            const sed::TVector center{0.8, 1.0, 0.9};
            const sed::TVector horizontal{0.6, 0.8, 0.0};
            const sed::TVector vertical{-0.16, 0.12, std::sqrt(0.96)};
            const std::array corners{
                center - horizontal * 0.25 - vertical * 0.25,
                center + horizontal * 0.25 - vertical * 0.25,
                center + horizontal * 0.25 + vertical * 0.25,
                center - horizontal * 0.25 + vertical * 0.25,
            };
            const std::array planes{
                sed::level::SectorCleavePlane{-vertical, corners[0]},
                sed::level::SectorCleavePlane{horizontal, corners[1]},
                sed::level::SectorCleavePlane{vertical, corners[2]},
                sed::level::SectorCleavePlane{-horizontal, corners[3]},
            };
            const std::array strokes{
                sed::level::SectorCleaveStroke{corners[0], corners[1]},
                sed::level::SectorCleaveStroke{corners[1], corners[2]},
                sed::level::SectorCleaveStroke{corners[2], corners[3]},
                sed::level::SectorCleaveStroke{corners[3], corners[0]},
            };
            const std::array effectivePlanes{planes[0], planes[2], planes[3]};
            const std::array effectiveStrokes{strokes[0], strokes[2], strokes[3]};
            sed::level::SectorCleaveBatchOptions options{};
            options.closedShape = closedShape;
            options.interiorPoint = center;
            options.bounded = bounded;
            options.strokes = effectiveStrokes;
            auto expected = source;
            const auto oracle = sed::level::levelCleaveSectorByPlanes(expected, 0, effectivePlanes, options);
            check(oracle.succeeded(), "Camera-plane Rectangle oracle has a nonempty valid intersection with the selected box");
            options.strokes = strokes;
            const auto actual = sed::level::levelCleaveSectorByPlanes(source, 0, planes, options);
            check(oracle.succeeded() && actual.succeeded()
                    && actual.outputSectors == oracle.outputSectors
                    && sed::assets::saveToSedBytes(source) == sed::assets::saveToSedBytes(expected)
                    && allAdjoinsAreReciprocal(source)
                    && allAdjoinBoundariesMatchByCoordinates(source),
                "Camera-plane Rectangle retains effective cuts when an outside edge plane does not split the Sector");
        }
    }

    for ( const bool emptyClosedInterior : {false, true} )
    {
        sed::Level level{};
        (void)sed::level::levelCreateCube(level, 0, 0, 0, sed::TVector::up, sed::TVector::right);
        const auto before = sed::assets::saveToSedBytes(level);
        std::array planes{
            sed::level::SectorCleavePlane{{1, 0, 0}, {0.5, 0, 0}},
            sed::level::SectorCleavePlane{{0, 1, 0}, {0, 2, 0}},
        };
        sed::level::SectorCleaveBatchOptions options{};
        options.closedShape = emptyClosedInterior;
        options.interiorPoint = {0.75, 2.5, 0.5};
        if ( !emptyClosedInterior )
        {
            planes[1].normal.x = std::numeric_limits<double>::quiet_NaN();
        }
        const auto result = sed::level::levelCleaveSectorByPlanes(level, 0, planes, options);
        check(!result.succeeded() && result.failedPlane == 1 && result.outputSectors.empty()
                && result.error == (emptyClosedInterior
                    ? sed::level::SectorCleaveBatchError::PlaneDoesNotSplit
                    : sed::level::SectorCleaveBatchError::InvalidPlane)
                && sed::assets::saveToSedBytes(level) == before,
            "Multi-cut preserves failure atomicity for an invalid later plane or genuinely empty closed interior");
    }

    for ( const int invalidCase : {0, 1, 2, 3, 4} )
    {
        sed::Level level{};
        level.layers = {"GeometryContractLayer"};
        const auto sourceIndex = sed::level::levelCreateSectorPrimitiveBox(level,
            sed::level::SectorPrimitive::Cube, {0, 0, 0}, {1, 1, 1});
        check(sourceIndex == 0, "cleave contract: rejection fixture has its fixed source sector");
        if ( sourceIndex != 0 )
        {
            continue;
        }
        std::vector<sed::level::SectorCleavePlane> planes{{{1, 0, 0}, {0.5, 0, 0}}};
        sed::level::SectorCleaveBatchOptions options{};
        std::int32_t requestedSector = 0;
        using Error = sed::level::SectorCleaveBatchError;
        const std::array<Error, 5> expectedErrors{{Error::InvalidSector, Error::EmptyPlaneSet,
            Error::InvalidPlane, Error::PlaneDoesNotSplit, Error::InvalidPlane}};
        const std::array<std::int32_t, 5> expectedPlanes{{-1, -1, 0, 0, -1}};
        if ( invalidCase == 0 )
        {
            requestedSector = -1;
        }
        else if ( invalidCase == 1 )
        {
            planes.clear();
        }
        else if ( invalidCase == 2 )
        {
            planes[0].normal.x = std::numeric_limits<double>::quiet_NaN();
        }
        else if ( invalidCase == 3 )
        {
            planes[0].point.x = 2.0;
        }
        else
        {
            options.closedShape = true;
            options.interiorPoint.x = std::numeric_limits<double>::infinity();
        }
        const auto before = sed::assets::saveToSedBytes(level);
        const auto result = sed::level::levelCleaveSectorByPlanes(level, requestedSector, planes, options);
        check(!result.succeeded() && result.error == expectedErrors[invalidCase]
                && result.failedPlane == expectedPlanes[invalidCase] && result.failedSector == requestedSector
                && result.failedSurface == -1 && result.outputSectors.empty(),
            "cleave contract: rejected request returns every fixed failure-detail field and no descendants");
        check(sed::assets::saveToSedBytes(level) == before,
            "cleave contract: every rejected request preserves all serialized input fields");
    }
    for ( const auto mode : {sed::level::SectorConnectKnifeMode::KeepTargetPieces,
            sed::level::SectorConnectKnifeMode::CutTargetHolesOnly} )
    {
        sed::Level level{};
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        level.sectors.push_back(makeLitTestCubeSector(1.5, sed::ColorWhite));
        level.sectors[1].surfaces.pop_back();
        const auto before = sed::assets::saveToSedBytes(level, {}, sed::assets::SedSerializationProfile::ProtectedV1, 0);
        check(!sed::level::levelConnectSectorsKnife(level, 0, 1, mode),
            "release Join: knife operation rejects target with a missing boundary wall");
        check(sed::assets::saveToSedBytes(level, {}, sed::assets::SedSerializationProfile::ProtectedV1, 0) == before,
            "release Join: invalid solid rejection preserves complete native state");
    }
    for ( const double translation : {0.0, 10000.0, 1000000.0} )
    {
        sed::Level source{};
        source.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        for ( auto& vertex : source.sectors[0].vertices )
        {
            vertex.x = vertex.x * 0.01 + translation;
            vertex.y = vertex.y * 0.01 + translation;
            vertex.z = vertex.z * 0.01 + translation;
        }
        const double side = source.sectors[0].vertices[1].x - source.sectors[0].vertices[0].x;
        const long double expectedVolume = static_cast<long double>(side) * side * side;
        const std::array planes{
            sed::level::SectorCleavePlane{
                {-0.22435302743833585, -0.87516371870066367, 0.42866558591666648},
                {translation - 0.00030671589869815381, translation + 0.0037010067757163264,
                    translation + 0.0021147940514498406}},
            sed::level::SectorCleavePlane{{0.0, 0.0, 1.0}, {translation, translation, translation}}};
        sed::editor::DocumentUndoHistory history;
        history.current().level = source;
        sed::level::SectorCleaveBatchResult cutResult{};
        check(history.apply("near-corner multi-cut", [&](auto& state)
            {
                cutResult = sed::level::levelCleaveSectorByPlanes(state.level, 0, planes);
                if ( !cutResult.succeeded() )
                {
                    std::cerr << "Translated near-corner cut at " << translation
                        << ": error=" << static_cast<int>(cutResult.error)
                        << " plane=" << cutResult.failedPlane << " sector=" << cutResult.failedSector
                        << " face=" << cutResult.failedSurface << '\n';
                }
                return cutResult.succeeded();
            }), "release F3: translated near-corner Multi-cut commits one transaction");
        const auto& output = history.current().level;
        long double volume = 0.0L;
        const double coordinateError = std::max(1.0e-12, translation * 2.0e-15);
        for ( const auto& sector : output.sectors )
        {
            const auto& origin = sector.vertices[0];
            long double signedVolume = 0.0L;
            std::map<std::pair<std::int32_t, std::int32_t>, std::pair<int, int>> incidence;
            for ( const auto& face : sector.surfaces )
            {
                for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
                {
                    const auto a = face.vertices[corner];
                    const auto b = face.vertices[(corner + 1) % face.vertices.size()];
                    auto& edge = incidence[std::minmax(a, b)];
                    ++edge.first;
                    edge.second += a < b ? 1 : -1;
                }
                if ( face.adjoin )
                {
                    bool commonPlane = false;
                    for ( const auto& plane : planes )
                    {
                        bool allOnPlane = true;
                        for ( const auto index : face.vertices )
                        {
                            const auto& v = sector.vertices[index];
                            allOnPlane = allOnPlane && std::abs(plane.normal.dot(v.x - plane.point.x,
                                v.y - plane.point.y, v.z - plane.point.z)) <= coordinateError;
                        }
                        commonPlane = commonPlane || allOnPlane;
                    }
                    check(commonPlane, "release F3: Multi-cut cap belongs to one complete authored plane");
                }
                const auto& first = sector.vertices[face.vertices[0]];
                long double nx = 0.0L, ny = 0.0L, nz = 0.0L, normalSquared = 0.0L;
                for ( std::size_t corner = 1; corner + 1 < face.vertices.size(); ++corner )
                {
                    const auto& second = sector.vertices[face.vertices[corner]];
                    const auto& third = sector.vertices[face.vertices[corner + 1]];
                    const long double ax = first.x - origin.x, ay = first.y - origin.y, az = first.z - origin.z;
                    const long double bx = second.x - origin.x, by = second.y - origin.y, bz = second.z - origin.z;
                    const long double cx = third.x - origin.x, cy = third.y - origin.y, cz = third.z - origin.z;
                    signedVolume += ax * (by * cz - bz * cy) + ay * (bz * cx - bx * cz) + az * (bx * cy - by * cx);
                    const long double ux = second.x - first.x, uy = second.y - first.y, uz = second.z - first.z;
                    const long double vx = third.x - first.x, vy = third.y - first.y, vz = third.z - first.z;
                    const long double tx = uy * vz - uz * vy, ty = uz * vx - ux * vz, tz = ux * vy - uy * vx;
                    const long double squared = tx * tx + ty * ty + tz * tz;
                    if ( squared > normalSquared )
                    {
                        nx = tx;
                        ny = ty;
                        nz = tz;
                        normalSquared = squared;
                    }
                }
                check(normalSquared > 0.0L, "release F3: every split face has independent nonzero area");
                for ( const auto& vertex : sector.vertices )
                {
                    const long double distance = nx * (vertex.x - first.x)
                        + ny * (vertex.y - first.y) + nz * (vertex.z - first.z);
                    check(distance >= -coordinateError * std::sqrt(normalSquared),
                        "release F3: independent supporting-plane oracle certifies every descendant convex");
                }
            }
            for ( const auto& [edge, counts] : incidence )
            {
                (void)edge;
                check(counts.first == 2 && counts.second == 0,
                    "release F3: independent directed edge counts certify closed split output");
            }
            volume += std::abs(signedVolume / 6.0L);
        }
        check(output.sectors.size() == 4, "release F3: two crossing planes produce four descendants");
        check(std::abs(volume - expectedVolume) <= coordinateError * side * side * 8.0,
            "release F3: translated Multi-cut conserves independent analytic volume");
        const auto encode = [](const sed::Level& level)
        {
            return sed::assets::saveToSedBytes(level, {}, sed::assets::SedSerializationProfile::ProtectedV1, 0);
        };
        const auto after = encode(output);
        check(history.undo() && encode(history.current().level) == encode(source),
            "release F3: Multi-cut Undo restores exact pre-cut geometry and UVs");
        check(history.redo() && encode(history.current().level) == after,
            "release F3: Multi-cut Redo restores exact validated partition");
    }
    {
        sed::Level level{};
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        for ( auto& vertex : level.sectors[0].vertices )
        {
            vertex.x *= 0.01;
            vertex.y *= 0.01;
            vertex.z *= 0.01;
        }
        const sed::level::SectorCleavePlane plane{
            {-0.22435302743833585, -0.87516371870066367, 0.42866558591666648},
            {-0.00030671589869815381, 0.0037010067757163264, 0.0021147940514498406}};
        const auto result = sed::level::levelCleaveSectorByPlanes(level, 0, {&plane, 1});
        check(result.succeeded(), "release F3: valid near-corner oblique cut succeeds");
        long double totalVolume = 0.0L;
        for ( const auto& sector : level.sectors )
        {
            long double signedVolume = 0.0L;
            for ( const auto& face : sector.surfaces )
            {
                if ( face.adjoin )
                {
                    for ( const auto index : face.vertices )
                    {
                        const auto& v = sector.vertices[index];
                        const double distance = plane.normal.dot(v.x - plane.point.x,
                            v.y - plane.point.y, v.z - plane.point.z);
                        check(std::abs(distance) < 1.0e-12,
                            "release F3: every cap corner lies on the requested plane");
                    }
                }
                const auto& a = sector.vertices[face.vertices[0]];
                for ( std::size_t corner = 1; corner + 1 < face.vertices.size(); ++corner )
                {
                    const auto& b = sector.vertices[face.vertices[corner]];
                    const auto& c = sector.vertices[face.vertices[corner + 1]];
                    signedVolume += static_cast<long double>(a.x) * (static_cast<long double>(b.y) * c.z - static_cast<long double>(b.z) * c.y)
                        + static_cast<long double>(a.y) * (static_cast<long double>(b.z) * c.x - static_cast<long double>(b.x) * c.z)
                        + static_cast<long double>(a.z) * (static_cast<long double>(b.x) * c.y - static_cast<long double>(b.y) * c.x);
                }
            }
            totalVolume += std::abs(signedVolume / 6.0L);
        }
        check(std::abs(totalVolume - 8.0e-6L) < 1.0e-14L,
            "release F3: independent tetrahedron sum conserves analytic cube volume");
    }
    {
        sed::Level source{};
        source.uvCoordinateSpace = static_cast<sed::UvCoordinateSpace>(99);
        sed::Sector& sector = source.newSector();
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
        };
        sed::Surface surface{};
        surface.vertices = { 0, 1, 2, 3 };
        surface.texVertices = {
            { -1.0f, 2.0f, sed::ColorRed },
            { 3.0f, 2.0f, sed::ColorGreen },
            { 3.0f, 6.0f, sed::ColorBlue },
            { -1.0f, 6.0f, sed::ColorWhite },
        };
        surface.uScale = 0.125f;
        surface.vScale = 0.25f;
        surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
        sector.surfaces.push_back(std::move(surface));

        sed::Level surfaceLevel = source;
        const sed::Level surfaceBefore = surfaceLevel;
        const bool surfaceResult = sed::level::levelCleaveSurface(
            surfaceLevel,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            1.0,
            0.0,
            0.0);
        check(!surfaceResult
                && cleaveResultsEquivalent(surfaceLevel, surfaceBefore),
            "Level.cleaveSurface.invalidUvSpace rejects an unknown Level UV tag atomically");

        sed::Level edgeLevel = source;
        const sed::Level edgeBefore = edgeLevel;
        const bool edgeResult = sed::level::levelCleaveEdge(
            edgeLevel,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            1.0,
            0.0,
            0.0);
        check(!edgeResult
                && cleaveResultsEquivalent(edgeLevel, edgeBefore),
            "Level.cleaveEdge.invalidUvSpace rejects an unknown Level UV tag atomically");

        sed::Level sectorLevel{};
        (void)sed::level::levelCreateCube(
            sectorLevel,
            0.0,
            0.0,
            0.0,
            sed::TVector::up,
            sed::TVector::right);
        sectorLevel.uvCoordinateSpace =
            static_cast<sed::UvCoordinateSpace>(99);
        const sed::Level sectorBefore = sectorLevel;
        const bool sectorResult = sed::level::levelCleaveSector(
            sectorLevel,
            0,
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        const std::array batchPlanes{
            sed::level::SectorCleavePlane{
                sed::TVector::right,
                sed::TVector::Create(0.5, 0.0, 0.0) },
        };
        const sed::level::SectorCleaveBatchResult batchResult =
            sed::level::levelCleaveSectorByPlanes(
                sectorLevel,
                0,
                batchPlanes);
        check(!sectorResult
                && !batchResult.succeeded()
                && batchResult.error
                    == sed::level::SectorCleaveBatchError::InvalidInputGeometry
                && cleaveResultsEquivalent(sectorLevel, sectorBefore),
            "Level.cleaveSector.invalidUvSpace rejects single and batch Sector cleaves atomically");

        sed::Level connectSource{};
        (void)sed::level::levelCreateCube(
            connectSource,
            0.0,
            0.0,
            0.0,
            sed::TVector::up,
            sed::TVector::right);
        (void)sed::level::levelCreateCube(
            connectSource,
            0.0,
            0.0,
            1.0,
            sed::TVector::up,
            sed::TVector::right);
        sed::Level connectedProbe = connectSource;
        const bool probeConnected = sed::level::levelConnectSectors(
            connectedProbe,
            0,
            1);
        std::optional<sed::SurfaceRef> firstPortal{};
        std::optional<sed::SurfaceRef> secondPortal{};
        if (probeConnected)
        {
            for (std::size_t surfaceIndex = 0;
                 surfaceIndex < connectedProbe.sectors[0].surfaces.size();
                 ++surfaceIndex)
            {
                const sed::Surface& surface =
                    connectedProbe.sectors[0].surfaces[surfaceIndex];
                if (surface.adjoin)
                {
                    firstPortal = sed::SurfaceRef{
                        0,
                        static_cast<std::int32_t>(surfaceIndex) };
                    secondPortal = surface.adjoin;
                    break;
                }
            }
        }
        sed::Level surfaceProbe = connectSource;
        const bool surfaceProbeConnected = firstPortal && secondPortal
            && sed::level::levelConnectSurfaces(
                surfaceProbe,
                *firstPortal,
                *secondPortal);
        sed::Level candidateProbe = connectSource;
        const bool candidateProbeConnected =
            sed::level::levelConnectSectorSurfaceCandidates(
                candidateProbe,
                { 0 },
                { 1 });

        connectSource.uvCoordinateSpace =
            static_cast<sed::UvCoordinateSpace>(99);
        const sed::Level connectBefore = connectSource;
        const bool invalidSurfacesConnected = firstPortal && secondPortal
            && sed::level::levelConnectSurfaces(
                connectSource,
                *firstPortal,
                *secondPortal);
        const bool invalidSectorsConnected = sed::level::levelConnectSectors(
            connectSource,
            0,
            1);
        const bool invalidCandidatesConnected =
            sed::level::levelConnectSectorSurfaceCandidates(
                connectSource,
                { 0 },
                { 1 });
        check(probeConnected
                && surfaceProbeConnected
                && candidateProbeConnected
                && !invalidSurfacesConnected
                && !invalidSectorsConnected
                && !invalidCandidatesConnected
                && cleaveResultsEquivalent(connectSource, connectBefore),
            "Level.connect.invalidUvSpace rejects otherwise valid Surface, Sector, and candidate connections atomically");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
        };

        sed::Surface malformed{};
        malformed.vertices = { 0, 1, 2, 3 };
        malformed.texVertices = {
            { 1.0f, 2.0f, sed::ColorRed },
            { 3.0f, 4.0f, sed::ColorGreen },
            { 5.0f, 6.0f, sed::ColorBlue },
        };
        sector.surfaces.push_back(malformed);

        const std::vector<sed::Vertex> originalVertices = sector.vertices;
        const std::vector<sed::TexVertex> originalTextureVertices = malformed.texVertices;
        const bool surfaceResult = sed::level::levelCleaveSurface(
            level,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            1.0,
            0.0,
            0.0);
        check(!surfaceResult, "Level.cleaveSurface.malformedUv: rejected");
        bool surfaceTextureVerticesUnchanged =
            sector.surfaces[0].texVertices.size() == originalTextureVertices.size();
        for ( std::size_t index = 0;
              surfaceTextureVerticesUnchanged && index < originalTextureVertices.size();
              ++index )
        {
            surfaceTextureVerticesUnchanged = sameTexVertex(
                sector.surfaces[0].texVertices[index],
                originalTextureVertices[index]);
        }
        check(
            sameVertexVectors(sector.vertices, originalVertices)
                && sector.surfaces.size() == 1
                && surfaceTextureVerticesUnchanged,
            "Level.cleaveSurface.malformedUv: authored data unchanged");

        const bool edgeResult = sed::level::levelCleaveEdge(
            level,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            1.0,
            0.0,
            0.0);
        check(!edgeResult, "Level.cleaveEdge.malformedUv: rejected");
        bool edgeTextureVerticesUnchanged =
            sector.surfaces[0].texVertices.size() == originalTextureVertices.size();
        for ( std::size_t index = 0;
              edgeTextureVerticesUnchanged && index < originalTextureVertices.size();
              ++index )
        {
            edgeTextureVerticesUnchanged = sameTexVertex(
                sector.surfaces[0].texVertices[index],
                originalTextureVertices[index]);
        }
        check(
            sameVertexVectors(sector.vertices, originalVertices)
                && sector.surfaces.size() == 1
                && edgeTextureVerticesUnchanged,
            "Level.cleaveEdge.malformedUv: authored data unchanged");
    }

    {
        // Finite authored endpoints can still overflow when their difference
        // is evaluated in float precision. Both cleave paths must interpolate
        // in a wider type and publish complete finite UV topology.
        const auto makeExtremeUvLevel = []()
        {
            sed::Level level{};
            level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            sed::Sector& sector = level.newSector();
            sector.vertices = {
                { 0.0, 0.0, 0.0, 0 },
                { 2.0, 0.0, 0.0, 0 },
                { 2.0, 2.0, 0.0, 0 },
                { 0.0, 2.0, 0.0, 0 },
            };

            const float maximum = std::numeric_limits<float>::max();
            sed::Surface surface{};
            surface.vertices = { 0, 1, 2, 3 };
            surface.texVertices = {
                { maximum, maximum, sed::ColorRed },
                { -maximum, -maximum, sed::ColorGreen },
                { maximum, maximum, sed::ColorBlue },
                { -maximum, -maximum, sed::ColorWhite },
            };
            surface.uScale = 0.125f;
            surface.vScale = 0.25f;
            sector.surfaces.push_back(std::move(surface));
            return level;
        };

        const auto allTextureCoordinatesFinite = [](const sed::Level& level)
        {
            return std::all_of(
                level.sectors.front().surfaces.begin(),
                level.sectors.front().surfaces.end(),
                [](const sed::Surface& surface)
                {
                    return surface.vertices.size() == surface.texVertices.size()
                        && std::all_of(
                            surface.texVertices.begin(),
                            surface.texVertices.end(),
                            [](const sed::TexVertex& coordinate)
                            {
                                return std::isfinite(coordinate.u)
                                    && std::isfinite(coordinate.v);
                            });
                });
        };

        sed::Level surfaceLevel = makeExtremeUvLevel();
        const bool surfaceResult = sed::level::levelCleaveSurface(
            surfaceLevel,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            1.0,
            0.0,
            0.0);
        check(surfaceResult && allTextureCoordinatesFinite(surfaceLevel),
            "Level.cleaveSurface.extremeFiniteUv interpolates without float overflow");

        sed::Level edgeLevel = makeExtremeUvLevel();
        const bool edgeResult = sed::level::levelCleaveEdge(
            edgeLevel,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            1.0,
            0.0,
            0.0);
        check(edgeResult && allTextureCoordinatesFinite(edgeLevel),
            "Level.cleaveEdge.extremeFiniteUv interpolates without float overflow");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();
        sector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
            { 1.0, -1.0, 0.0, 0 },
        };

        sed::Surface source{};
        source.vertices = { 0, 1, 2, 3 };
        source.texVertices = {
            { 1.0f, 2.0f, sed::ColorRed },
            { 3.0f, 4.0f, sed::ColorGreen },
            { 5.0f, 6.0f, sed::ColorBlue },
            { 7.0f, 8.0f, sed::ColorWhite },
        };
        sector.surfaces.push_back(source);

        sed::Surface malformedRelated{};
        malformedRelated.vertices = { 0, 1, 4 };
        malformedRelated.texVertices = {
            { 11.0f, 12.0f, sed::ColorRed },
            { 13.0f, 14.0f, sed::ColorGreen },
        };
        sector.surfaces.push_back(malformedRelated);

        const std::vector<sed::Vertex> verticesBefore = sector.vertices;
        const std::vector<std::int32_t> sourceIndicesBefore = sector.surfaces[0].vertices;
        const std::vector<std::int32_t> relatedIndicesBefore = sector.surfaces[1].vertices;
        const std::vector<sed::TexVertex> sourceTextureBefore = sector.surfaces[0].texVertices;
        const std::vector<sed::TexVertex> relatedTextureBefore = sector.surfaces[1].texVertices;
        const bool ok = sed::level::levelCleaveSurface(
            level,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(!ok, "Level.cleaveSurface.malformedRelatedUv: rejected before mutation");
        check(
            sameVertexVectors(sector.vertices, verticesBefore)
                && sector.surfaces.size() == 2
                && sector.surfaces[0].vertices == sourceIndicesBefore
                && sector.surfaces[1].vertices == relatedIndicesBefore
                && sameTextureVertexVectors(sector.surfaces[0].texVertices, sourceTextureBefore)
                && sameTextureVertexVectors(sector.surfaces[1].texVertices, relatedTextureBefore),
            "Level.cleaveSurface.malformedRelatedUv: source and related topology unchanged");
    }

    {
        sed::Level level{};
        sed::Sector& sourceSector = level.newSector();
        sourceSector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
            { 1.0, -1.0, 0.0, 0 },
        };
        sed::Surface source{};
        source.vertices = { 0, 1, 2, 3 };
        source.texVertices = {
            { 1.0f, 2.0f, sed::ColorRed },
            { 3.0f, 4.0f, sed::ColorGreen },
            { 5.0f, 6.0f, sed::ColorBlue },
            { 7.0f, 8.0f, sed::ColorWhite },
        };
        source.adjoin = sed::SurfaceRef{ 1, 0 };
        source.nAdj = 0;
        sourceSector.surfaces.push_back(source);
        const std::vector<sed::Vertex> sharedVertices = sourceSector.vertices;

        sed::Sector& mirrorSector = level.newSector();
        mirrorSector.vertices = sharedVertices;
        sed::Surface mirror{};
        mirror.vertices = { 0, 3, 2, 1 };
        mirror.texVertices = {
            { 21.0f, 22.0f, sed::ColorRed },
            { 23.0f, 24.0f, sed::ColorGreen },
            { 25.0f, 26.0f, sed::ColorBlue },
            { 27.0f, 28.0f, sed::ColorWhite },
        };
        mirror.adjoin = sed::SurfaceRef{ 0, 0 };
        mirror.nAdj = 0;
        mirrorSector.surfaces.push_back(mirror);

        sed::Surface malformedMirrorRelated{};
        malformedMirrorRelated.vertices = { 0, 1, 4 };
        malformedMirrorRelated.texVertices = {
            { 31.0f, 32.0f, sed::ColorRed },
            { 33.0f, 34.0f, sed::ColorGreen },
        };
        mirrorSector.surfaces.push_back(malformedMirrorRelated);

        const std::vector<sed::Vertex> sourceVerticesBefore = level.sectors[0].vertices;
        const std::vector<sed::Vertex> mirrorVerticesBefore = level.sectors[1].vertices;
        const std::vector<std::int32_t> sourceIndicesBefore = level.sectors[0].surfaces[0].vertices;
        const std::vector<std::int32_t> mirrorIndicesBefore = level.sectors[1].surfaces[0].vertices;
        const std::vector<std::int32_t> relatedIndicesBefore = level.sectors[1].surfaces[1].vertices;
        const std::vector<sed::TexVertex> sourceTextureBefore = level.sectors[0].surfaces[0].texVertices;
        const std::vector<sed::TexVertex> mirrorTextureBefore = level.sectors[1].surfaces[0].texVertices;
        const std::vector<sed::TexVertex> relatedTextureBefore = level.sectors[1].surfaces[1].texVertices;

        const bool surfaceResult = sed::level::levelCleaveSurface(
            level,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(!surfaceResult, "Level.cleaveSurface.malformedMirrorRelatedUv: rejected before mutation");
        check(
            sameVertexVectors(level.sectors[0].vertices, sourceVerticesBefore)
                && sameVertexVectors(level.sectors[1].vertices, mirrorVerticesBefore)
                && level.sectors[0].surfaces.size() == 1
                && level.sectors[1].surfaces.size() == 2
                && level.sectors[0].surfaces[0].vertices == sourceIndicesBefore
                && level.sectors[1].surfaces[0].vertices == mirrorIndicesBefore
                && level.sectors[1].surfaces[1].vertices == relatedIndicesBefore
                && level.sectors[0].surfaces[0].adjoin == sed::SurfaceRef{ 1, 0 }
                && level.sectors[1].surfaces[0].adjoin == sed::SurfaceRef{ 0, 0 }
                && sameTextureVertexVectors(level.sectors[0].surfaces[0].texVertices, sourceTextureBefore)
                && sameTextureVertexVectors(level.sectors[1].surfaces[0].texVertices, mirrorTextureBefore)
                && sameTextureVertexVectors(level.sectors[1].surfaces[1].texVertices, relatedTextureBefore),
            "Level.cleaveSurface.malformedMirrorRelatedUv: all related topology remains exact");

        const bool edgeResult = sed::level::levelCleaveEdge(
            level,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(!edgeResult, "Level.cleaveEdge.malformedMirrorRelatedUv: rejected before mutation");
        check(
            sameVertexVectors(level.sectors[0].vertices, sourceVerticesBefore)
                && sameVertexVectors(level.sectors[1].vertices, mirrorVerticesBefore)
                && level.sectors[0].surfaces[0].vertices == sourceIndicesBefore
                && level.sectors[1].surfaces[0].vertices == mirrorIndicesBefore
                && level.sectors[1].surfaces[1].vertices == relatedIndicesBefore
                && sameTextureVertexVectors(level.sectors[0].surfaces[0].texVertices, sourceTextureBefore)
                && sameTextureVertexVectors(level.sectors[1].surfaces[0].texVertices, mirrorTextureBefore)
                && sameTextureVertexVectors(level.sectors[1].surfaces[1].texVertices, relatedTextureBefore),
            "Level.cleaveEdge.malformedMirrorRelatedUv: all related topology remains exact");
    }

    {
        sed::Level level{};
        sed::Sector& sourceSector = level.newSector();
        sourceSector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
        };
        sed::Surface source{};
        source.vertices = { 0, 1, 2, 3 };
        source.texVertices = {
            { 1.0f, 2.0f, sed::ColorRed },
            { 3.0f, 4.0f, sed::ColorGreen },
            { 5.0f, 6.0f, sed::ColorBlue },
            { 7.0f, 8.0f, sed::ColorWhite },
        };
        source.adjoin = sed::SurfaceRef{ 1, 0 };
        source.nAdj = 0;
        sourceSector.surfaces.push_back(source);
        const std::vector<sed::Vertex> sharedVertices = sourceSector.vertices;

        sed::Sector& mirrorSector = level.newSector();
        mirrorSector.vertices = sharedVertices;
        sed::Surface malformedMirror{};
        malformedMirror.vertices = { 0, 3, 2, 1 };
        malformedMirror.texVertices = {
            { 21.0f, 22.0f, sed::ColorRed },
            { 23.0f, 24.0f, sed::ColorGreen },
            { 25.0f, 26.0f, sed::ColorBlue },
        };
        malformedMirror.adjoin = sed::SurfaceRef{ 0, 0 };
        malformedMirror.nAdj = 0;
        mirrorSector.surfaces.push_back(malformedMirror);

        const std::vector<sed::Vertex> sourceVerticesBefore = level.sectors[0].vertices;
        const std::vector<sed::Vertex> mirrorVerticesBefore = level.sectors[1].vertices;
        const std::vector<std::int32_t> sourceIndicesBefore = level.sectors[0].surfaces[0].vertices;
        const std::vector<std::int32_t> mirrorIndicesBefore = level.sectors[1].surfaces[0].vertices;
        const std::vector<sed::TexVertex> sourceTextureBefore = level.sectors[0].surfaces[0].texVertices;
        const std::vector<sed::TexVertex> mirrorTextureBefore = level.sectors[1].surfaces[0].texVertices;
        const bool surfaceResult = sed::level::levelCleaveSurface(
            level,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(!surfaceResult, "Level.cleaveSurface.malformedMirrorUv: rejected before mutation");
        check(
            sameVertexVectors(level.sectors[0].vertices, sourceVerticesBefore)
                && sameVertexVectors(level.sectors[1].vertices, mirrorVerticesBefore)
                && level.sectors[0].surfaces.size() == 1
                && level.sectors[1].surfaces.size() == 1
                && level.sectors[0].surfaces[0].vertices == sourceIndicesBefore
                && level.sectors[1].surfaces[0].vertices == mirrorIndicesBefore
                && level.sectors[0].surfaces[0].adjoin == sed::SurfaceRef{ 1, 0 }
                && level.sectors[1].surfaces[0].adjoin == sed::SurfaceRef{ 0, 0 }
                && sameTextureVertexVectors(level.sectors[0].surfaces[0].texVertices, sourceTextureBefore)
                && sameTextureVertexVectors(level.sectors[1].surfaces[0].texVertices, mirrorTextureBefore),
            "Level.cleaveSurface.malformedMirrorUv: source and portal topology unchanged");

        const bool edgeResult = sed::level::levelCleaveEdge(
            level,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(!edgeResult, "Level.cleaveEdge.malformedMirrorUv: rejected before mutation");
        check(
            sameVertexVectors(level.sectors[0].vertices, sourceVerticesBefore)
                && sameVertexVectors(level.sectors[1].vertices, mirrorVerticesBefore)
                && level.sectors[0].surfaces[0].vertices == sourceIndicesBefore
                && level.sectors[1].surfaces[0].vertices == mirrorIndicesBefore
                && sameTextureVertexVectors(level.sectors[0].surfaces[0].texVertices, sourceTextureBefore)
                && sameTextureVertexVectors(level.sectors[1].surfaces[0].texVertices, mirrorTextureBefore),
            "Level.cleaveEdge.malformedMirrorUv: source and portal topology unchanged");
    }

    {
        sed::Level level{};
        sed::Sector& sourceSector = level.newSector();
        sourceSector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
        };
        sed::Surface source{};
        source.vertices = { 0, 1, 2, 3 };
        source.texVertices = {
            { 1.0f, 2.0f, sed::ColorRed },
            { 3.0f, 4.0f, sed::ColorGreen },
            { 5.0f, 6.0f, sed::ColorBlue },
            { 7.0f, 8.0f, sed::ColorWhite },
        };
        source.adjoin = sed::SurfaceRef{ 1, 0 };
        source.nAdj = 0;
        sourceSector.surfaces.push_back(source);

        sed::Sector& mirrorSector = level.newSector();
        mirrorSector.vertices = {
            { -2.0, 0.0, 0.0, 0 },
            { 0.0, 0.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
            { -2.0, 2.0, 0.0, 0 },
        };
        sed::Surface mirror{};
        mirror.vertices = { 0, 3, 2, 1 };
        mirror.texVertices = {
            { 21.0f, 22.0f, sed::ColorRed },
            { 23.0f, 24.0f, sed::ColorGreen },
            { 25.0f, 26.0f, sed::ColorBlue },
            { 27.0f, 28.0f, sed::ColorWhite },
        };
        mirror.adjoin = sed::SurfaceRef{ 0, 0 };
        mirror.nAdj = 0;
        mirrorSector.surfaces.push_back(mirror);

        const std::vector<sed::Vertex> sourceVerticesBefore = level.sectors[0].vertices;
        const std::vector<sed::Vertex> mirrorVerticesBefore = level.sectors[1].vertices;
        const std::vector<std::int32_t> sourceIndicesBefore = level.sectors[0].surfaces[0].vertices;
        const std::vector<std::int32_t> mirrorIndicesBefore = level.sectors[1].surfaces[0].vertices;
        const std::vector<sed::TexVertex> sourceTextureBefore = level.sectors[0].surfaces[0].texVertices;
        const std::vector<sed::TexVertex> mirrorTextureBefore = level.sectors[1].surfaces[0].texVertices;

        const bool ok = sed::level::levelCleaveSurface(
            level,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(!ok, "Level.cleaveSurface.unsplittableMirror: rejected before mutation");
        check(
            sameVertexVectors(level.sectors[0].vertices, sourceVerticesBefore)
                && sameVertexVectors(level.sectors[1].vertices, mirrorVerticesBefore)
                && level.sectors[0].surfaces.size() == 1
                && level.sectors[1].surfaces.size() == 1
                && level.sectors[0].surfaces[0].vertices == sourceIndicesBefore
                && level.sectors[1].surfaces[0].vertices == mirrorIndicesBefore
                && level.sectors[0].surfaces[0].adjoin == sed::SurfaceRef{ 1, 0 }
                && level.sectors[1].surfaces[0].adjoin == sed::SurfaceRef{ 0, 0 }
                && sameTextureVertexVectors(level.sectors[0].surfaces[0].texVertices, sourceTextureBefore)
                && sameTextureVertexVectors(level.sectors[1].surfaces[0].texVertices, mirrorTextureBefore),
            "Level.cleaveSurface.unsplittableMirror: source and direct mirror remain exact");
    }

    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& sector = level.newSector();

        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(2.0, 2.0, 0.0);
        const std::int32_t v3 = sector.newVertex(0.0, 2.0, 0.0);
        const std::int32_t v4 = sector.newVertex(1.0, -1.0, 0.0);

        sed::Surface quad{};
        quad.nMat = 7;
        quad.vertices = { v0, v1, v2, v3 };
        quad.texVertices = {
            { 0.10f, 0.20f, sed::ColorF{ 0.0f, 0.2f, 0.4f, 0.6f } },
            { 1.10f, 0.20f, sed::ColorF{ 1.0f, 0.4f, 0.6f, 0.8f } },
            { 1.10f, 2.20f, sed::ColorF{ 0.8f, 0.6f, 0.4f, 0.2f } },
            { 0.10f, 2.20f, sed::ColorF{ 0.2f, 0.4f, 0.6f, 1.0f } },
        };
        quad.uScale = -0.125f;
        quad.vScale = 0.375f;
        const std::vector<std::pair<std::int32_t, sed::TexVertex>> quadOriginals{
            { v0, quad.texVertices[0] },
            { v1, quad.texVertices[1] },
            { v2, quad.texVertices[2] },
            { v3, quad.texVertices[3] },
        };
        (void)sector.addSurface(quad);

        sed::Surface adjacent{};
        adjacent.nMat = 8;
        adjacent.vertices = { v0, v4, v1 };
        adjacent.texVertices = {
            { -0.30f, 0.04f, sed::ColorF{ 0.1f, 0.3f, 0.5f, 0.7f } },
            { 0.05f, -0.50f, sed::ColorF{ 0.5f, 0.4f, 0.3f, 0.2f } },
            { 0.70f, 0.14f, sed::ColorF{ 0.9f, 0.7f, 0.5f, 0.3f } },
        };
        adjacent.uScale = 0.0625f;
        adjacent.vScale = -0.1875f;
        const std::vector<std::pair<std::int32_t, sed::TexVertex>> adjacentOriginals{
            { v0, adjacent.texVertices[0] },
            { v4, adjacent.texVertices[1] },
            { v1, adjacent.texVertices[2] },
        };
        (void)sector.addSurface(adjacent);

        const bool ok = sed::level::levelCleaveSurface(
            level,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(ok, "Level.cleaveSurface.uvPreservation: succeeds");
        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized,
            "Level.cleaveSurface.uvPreservation: normalized coordinate-space tag survives");
        check(sector.surfaces.size() == 3, "Level.cleaveSurface.uvPreservation: output surface count");

        const std::int32_t bottomCut = findVertexIndex(sector, 0.5, 0.0, 0.0);
        const std::int32_t topCut = findVertexIndex(sector, 0.5, 2.0, 0.0);
        check(bottomCut >= 0 && topCut >= 0, "Level.cleaveSurface.uvPreservation: cut vertices exist");

        checkSurfaceKeepsOriginalTextureVertices(
            sector,
            quad.nMat,
            quadOriginals,
            "Level.cleaveSurface.uvPreservation: every split authored corner survives exactly once");
        checkSurfaceKeepsOriginalTextureVertices(
            sector,
            adjacent.nMat,
            adjacentOriginals,
            "Level.cleaveSurface.uvPreservation: every adjacent authored corner survives exactly once");

        const sed::TexVertex expectedBottom{
            0.35f,
            0.20f,
            sed::ColorF{ 0.25f, 0.25f, 0.45f, 0.65f }
        };
        const sed::TexVertex expectedTop{
            0.35f,
            2.20f,
            sed::ColorF{ 0.35f, 0.45f, 0.55f, 0.8f }
        };
        const sed::TexVertex expectedAdjacent{
            -0.05f,
            0.065f,
            sed::ColorF{ 0.3f, 0.4f, 0.5f, 0.6f }
        };

        for ( const sed::Surface& surface : sector.surfaces )
        {
            if ( surface.nMat == quad.nMat )
            {
                check(surface.uScale == quad.uScale
                        && surface.vScale == quad.vScale,
                    "Level.cleaveSurface.uvPreservation: both split faces retain source scale metadata");
            }
            else if ( surface.nMat == adjacent.nMat )
            {
                check(surface.uScale == adjacent.uScale
                        && surface.vScale == adjacent.vScale,
                    "Level.cleaveSurface.uvPreservation: related face retains independent scale metadata");
            }

            if ( const sed::TexVertex* bottom = findSurfaceTexVertex(surface, bottomCut) )
            {
                const sed::TexVertex& expected = surface.nMat == adjacent.nMat ? expectedAdjacent : expectedBottom;
                check(
                    close(bottom->u, expected.u) && close(bottom->v, expected.v)
                        && close(bottom->color.r, expected.color.r)
                        && close(bottom->color.g, expected.color.g)
                        && close(bottom->color.b, expected.color.b)
                        && close(bottom->color.a, expected.color.a),
                    "Level.cleaveSurface.uvPreservation: normalized bottom cut corner interpolated");
            }
            if ( const sed::TexVertex* top = findSurfaceTexVertex(surface, topCut) )
            {
                check(
                    close(top->u, expectedTop.u) && close(top->v, expectedTop.v)
                        && close(top->color.r, expectedTop.color.r)
                        && close(top->color.g, expectedTop.color.g)
                        && close(top->color.b, expectedTop.color.b)
                        && close(top->color.a, expectedTop.color.a),
                    "Level.cleaveSurface.uvPreservation: normalized top cut corner interpolated");
            }
        }
    }

    {
        // A live normalized Level must admit both cleave entry points without
        // resolving a MAT or silently treating its coordinates as pixels.
        // This is the exact unit contract used by new projects and SED V2.
        sed::Level surfaceLevel{};
        surfaceLevel.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& surfaceSector = surfaceLevel.newSector();
        surfaceSector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
        };
        sed::Surface surface{};
        surface.vertices = { 0, 1, 2, 3 };
        surface.texVertices = {
            { -0.25f, 0.125f, sed::ColorRed },
            { 1.75f, 0.375f, sed::ColorGreen },
            { 2.25f, 1.875f, sed::ColorBlue },
            { -0.75f, 1.625f, sed::ColorWhite },
        };
        surface.uScale = 0.125f;
        surface.vScale = 0.25f;
        surfaceSector.surfaces.push_back(surface);

        const bool surfaceOk = sed::level::levelCleaveSurface(
            surfaceLevel,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(surfaceOk, "Level.cleave.normalizedLive.surface: succeeds without MAT dimensions");
        check(surfaceLevel.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized,
            "Level.cleave.normalizedLive.surface: coordinate-space tag remains normalized");
        check(surfaceSector.surfaces.size() == 2,
            "Level.cleave.normalizedLive.surface: output face appended");

        sed::Level edgeLevel{};
        edgeLevel.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& edgeSector = edgeLevel.newSector();
        edgeSector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
        };
        edgeSector.surfaces.push_back(surface);

        const bool edgeOk = sed::level::levelCleaveEdge(
            edgeLevel,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(edgeOk, "Level.cleave.normalizedLive.edge: succeeds without MAT dimensions");
        check(edgeLevel.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized,
            "Level.cleave.normalizedLive.edge: coordinate-space tag remains normalized");
        check(edgeSector.vertices.size() == 5 && edgeSector.surfaces[0].vertices.size() == 5,
            "Level.cleave.normalizedLive.edge: one shared corner inserted");

        const std::int32_t insertedVertex = findVertexIndex(edgeSector, 0.5, 0.0, 0.0);
        const sed::TexVertex* insertedTextureVertex = insertedVertex >= 0
            ? findSurfaceTexVertex(edgeSector.surfaces[0], insertedVertex)
            : nullptr;
        check(insertedTextureVertex != nullptr
                && close(insertedTextureVertex->u, 0.25f)
                && close(insertedTextureVertex->v, 0.1875f),
            "Level.cleave.normalizedLive.edge: inserted UV is interpolated in normalized space");
    }

    {
        // SED V1 recovery remains explicitly pixel-space. Cleave must preserve
        // those values and tags until a separate resource-backed migration is
        // possible; no geometry edit is allowed to perform an implicit unit
        // conversion.
        const auto makePixelLevel = []()
        {
            sed::Level level{};
            level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
            sed::Sector& sector = level.newSector();
            sector.vertices = {
                { 0.0, 0.0, 0.0, 0 },
                { 2.0, 0.0, 0.0, 0 },
                { 2.0, 2.0, 0.0, 0 },
                { 0.0, 2.0, 0.0, 0 },
            };
            sed::Surface surface{};
            surface.vertices = { 0, 1, 2, 3 };
            surface.texVertices = {
                { -32.0f, 16.0f, sed::ColorRed },
                { 224.0f, 48.0f, sed::ColorGreen },
                { 288.0f, 240.0f, sed::ColorBlue },
                { -96.0f, 208.0f, sed::ColorWhite },
            };
            surface.uScale = 16.0f;
            surface.vScale = 32.0f;
            sector.surfaces.push_back(std::move(surface));
            return level;
        };

        const std::vector<std::byte> pixelBytes =
            sed::assets::saveToSedBytes(makePixelLevel());
        sed::Level surfaceLevel = sed::assets::loadFromSedBytes(pixelBytes);
        check(surfaceLevel.uvCoordinateSpace == sed::UvCoordinateSpace::TexturePixels,
            "Level.cleave.pixelCompatibility: SED V1 reopens in pixel space");
        const bool surfaceOk = sed::level::levelCleaveSurface(
            surfaceLevel,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(surfaceOk, "Level.cleave.pixelCompatibility.surface: succeeds");
        check(surfaceLevel.uvCoordinateSpace == sed::UvCoordinateSpace::TexturePixels,
            "Level.cleave.pixelCompatibility.surface: V1 tag remains pixel-space");
        check(surfaceLevel.sectors[0].surfaces.size() == 2
                && std::all_of(
                    surfaceLevel.sectors[0].surfaces.begin(),
                    surfaceLevel.sectors[0].surfaces.end(),
                    [](const sed::Surface& surface)
                    {
                        return surface.uScale == 16.0f
                            && surface.vScale == 32.0f;
                    }),
            "Level.cleave.pixelCompatibility.surface: every split face retains exact pixel-scale metadata");

        sed::Level edgeLevel = sed::assets::loadFromSedBytes(pixelBytes);
        const bool edgeOk = sed::level::levelCleaveEdge(
            edgeLevel,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(edgeOk, "Level.cleave.pixelCompatibility.edge: succeeds");
        check(edgeLevel.uvCoordinateSpace == sed::UvCoordinateSpace::TexturePixels,
            "Level.cleave.pixelCompatibility.edge: V1 tag remains pixel-space");
        check(edgeLevel.sectors[0].surfaces.size() == 1
                && edgeLevel.sectors[0].surfaces[0].uScale == 16.0f
                && edgeLevel.sectors[0].surfaces[0].vScale == 32.0f,
            "Level.cleave.pixelCompatibility.edge: pixel-scale metadata remains exact");

        const sed::Sector& edgeSector = edgeLevel.sectors[0];
        const std::int32_t insertedVertex = findVertexIndex(edgeSector, 0.5, 0.0, 0.0);
        const sed::TexVertex* insertedTextureVertex = insertedVertex >= 0
            ? findSurfaceTexVertex(edgeSector.surfaces[0], insertedVertex)
            : nullptr;
        check(insertedTextureVertex != nullptr
                && close(insertedTextureVertex->u, 32.0f)
                && close(insertedTextureVertex->v, 24.0f),
            "Level.cleave.pixelCompatibility.edge: inserted UV stays in pixel space");
    }

    {
        enum class CleavePath
        {
            surface,
            edge,
            sector,
        };
        struct CleavePathCase
        {
            const char* name{};
            CleavePath path{};
        };
        const std::array pathCases{
            CleavePathCase{ "Surface", CleavePath::surface },
            CleavePathCase{ "Edge", CleavePath::edge },
            CleavePathCase{ "Sector", CleavePath::sector },
        };

        for (const sed::UvCoordinateSpace space : {
                 sed::UvCoordinateSpace::TexturePixels,
                 sed::UvCoordinateSpace::Normalized })
        {
            for (const CleavePathCase& testCase : pathCases)
            {
                for (const bool invalidateScale : { false, true })
                {
                    sed::Level level{};
                    level.uvCoordinateSpace = space;
                    sed::Sector& sector = sed::level::levelCreateCube(
                        level,
                        0.0,
                        0.0,
                        0.0,
                        sed::TVector::up,
                        sed::TVector::right);
                    sed::Surface& malformed = sector.surfaces[0];
                    if (invalidateScale)
                    {
                        malformed.uScale =
                            std::numeric_limits<float>::infinity();
                    }
                    else
                    {
                        malformed.texVertices[0].v =
                            -std::numeric_limits<float>::infinity();
                    }

                    const sed::Level baseline = level;
                    bool result = false;
                    switch (testCase.path)
                    {
                    case CleavePath::surface:
                        result = sed::level::levelCleaveSurface(
                            level,
                            sed::SurfaceRef{ 0, 0 },
                            sed::TVector::right,
                            0.5,
                            0.0,
                            0.0);
                        break;
                    case CleavePath::edge:
                        result = sed::level::levelCleaveEdge(
                            level,
                            sed::SurfaceRef{ 0, 0 },
                            0,
                            sed::TVector::right,
                            0.5,
                            0.0,
                            0.0);
                        break;
                    case CleavePath::sector:
                        result = sed::level::levelCleaveSector(
                            level,
                            0,
                            sed::TVector::right,
                            0.5,
                            0.0,
                            0.0);
                        break;
                    }

                    const std::string message =
                        "Level.cleave rejects non-finite "
                        + std::string(invalidateScale ? "UV scale" : "UV")
                        + " atomically for "
                        + std::string(
                            space == sed::UvCoordinateSpace::TexturePixels
                                ? "pixel-space "
                                : "normalized ")
                        + testCase.name;
                    check(!result
                            && sed::assets::sedLevelsEqual(level, baseline),
                        message.c_str());
                }
            }
        }
    }

    {
        // Exercise the persisted project boundary reported by users: a new
        // normalized cube saved as SED V2 and reopened must retain complete
        // texture-corner topology for both face and edge cleave.
        sed::Level source{};
        source.kind = sed::ProjectType::IJIM;
        source.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& sourceSector = sed::level::levelCreateCube(
            source,
            0.0,
            0.0,
            0.0,
            sed::TVector::up,
            sed::TVector::right);
        for ( sed::Surface& sourceSurface : sourceSector.surfaces )
        {
            sourceSurface.material = "cleave-regression.mat";
        }

        const std::vector<std::byte> bytes = sed::assets::saveToSedBytes(source);
        const sed::Level reopened = sed::assets::loadFromSedBytes(bytes);
        check(reopened.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized,
            "Level.cleave.sedV2RoundTrip: normalized tag restored");
        check(!reopened.sectors.empty()
                && std::all_of(
                    reopened.sectors[0].surfaces.begin(),
                    reopened.sectors[0].surfaces.end(),
                    [](const sed::Surface& reopenedSurface)
                    {
                        return reopenedSurface.vertices.size()
                            == reopenedSurface.texVertices.size();
                    }),
            "Level.cleave.sedV2RoundTrip: every reopened face retains complete UV topology");

        sed::Level surfaceLevel = reopened;
        const bool surfaceOk = sed::level::levelCleaveSurface(
            surfaceLevel,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(surfaceOk, "Level.cleave.sedV2RoundTrip.surface: succeeds after reopen");

        sed::Level edgeLevel = reopened;
        const bool edgeOk = sed::level::levelCleaveEdge(
            edgeLevel,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(edgeOk, "Level.cleave.sedV2RoundTrip.edge: succeeds after reopen");
    }

    {
        // Cleave is deliberately coordinate-space agnostic: every generated
        // corner is an interpolation in its owning Surface's units. Compare
        // the complete result against a pixel compatibility document with
        // different anisotropic dimensions on the selected and related faces.
        const auto makeFixture = []()
        {
            sed::Level level{};
            level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
            sed::Sector& sector = level.newSector();
            sector.vertices = {
                { 0.0, 0.0, 0.0, 0 },
                { 2.0, 0.0, 0.0, 0 },
                { 2.0, 2.0, 0.0, 0 },
                { 0.0, 2.0, 0.0, 0 },
                { 1.0, -1.0, 0.0, 0 },
            };

            sed::Surface selected{};
            selected.nMat = 0;
            selected.vertices = { 0, 1, 2, 3 };
            selected.texVertices = {
                { -0.5f, 1.25f, sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } },
                { 2.5f, 1.75f, sed::ColorF{ 0.5f, 0.6f, 0.7f, 0.8f } },
                { 3.0f, -2.25f, sed::ColorF{ 0.9f, 0.8f, 0.7f, 0.6f } },
                { -1.0f, -1.75f, sed::ColorF{ 0.5f, 0.4f, 0.3f, 0.2f } },
            };
            selected.uScale = -0.125f;
            selected.vScale = 0.375f;
            selected.normal = sed::TVector{ 0.0, 0.0, 1.0 };
            sector.surfaces.push_back(selected);

            sed::Surface related{};
            related.nMat = 1;
            related.vertices = { 0, 4, 1 };
            related.texVertices = {
                { -2.0f, 3.0f, sed::ColorF{ 0.2f, 0.4f, 0.6f, 0.8f } },
                { 1.0f, -0.5f, sed::ColorF{ 0.3f, 0.5f, 0.7f, 0.9f } },
                { 4.0f, 2.0f, sed::ColorF{ 0.8f, 0.6f, 0.4f, 0.2f } },
            };
            related.uScale = 0.0625f;
            related.vScale = -0.1875f;
            related.normal = sed::TVector{ 0.0, 0.0, 1.0 };
            sector.surfaces.push_back(related);
            return level;
        };
        const auto dimensions = [](const sed::Surface& surface)
            -> std::optional<sed::level::UvTextureDimensions>
        {
            if ( surface.nMat == 0 )
            {
                return sed::level::UvTextureDimensions{ 128.0, 32.0 };
            }
            if ( surface.nMat == 1 )
            {
                return sed::level::UvTextureDimensions{ 64.0, 256.0 };
            }
            return std::nullopt;
        };
        const auto makePixelFixture = [&]()
        {
            sed::Level pixels = makeFixture();
            const sed::level::LevelUvConversionResult converted =
                sed::level::convertLevelUvCoordinateSpace(
                    pixels,
                    sed::UvCoordinateSpace::TexturePixels,
                    dimensions);
            check(static_cast<bool>(converted),
                "Level.cleave.crossSpace fixture converts every material through its own anisotropic dimensions");
            return pixels;
        };

        sed::Level normalizedSurface = makeFixture();
        sed::Level pixelSurface = makePixelFixture();
        const bool normalizedSurfaceOk = sed::level::levelCleaveSurface(
            normalizedSurface,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        const bool pixelSurfaceOk = sed::level::levelCleaveSurface(
            pixelSurface,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        const sed::level::LevelUvConversionResult normalizedPixelSurface =
            sed::level::convertLevelUvCoordinateSpace(
                pixelSurface,
                sed::UvCoordinateSpace::Normalized,
                dimensions);
        check(normalizedSurfaceOk
                && pixelSurfaceOk
                && static_cast<bool>(normalizedPixelSurface)
                && cleaveResultsEquivalent(normalizedSurface, pixelSurface),
            "Level.cleaveSurface.crossSpace preserves equivalent geometry, signed multi-tile UVs, scales, and colors across per-material anisotropic units");

        sed::Level normalizedEdge = makeFixture();
        sed::Level pixelEdge = makePixelFixture();
        const bool normalizedEdgeOk = sed::level::levelCleaveEdge(
            normalizedEdge,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        const bool pixelEdgeOk = sed::level::levelCleaveEdge(
            pixelEdge,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        const sed::level::LevelUvConversionResult normalizedPixelEdge =
            sed::level::convertLevelUvCoordinateSpace(
                pixelEdge,
                sed::UvCoordinateSpace::Normalized,
                dimensions);
        check(normalizedEdgeOk
                && pixelEdgeOk
                && static_cast<bool>(normalizedPixelEdge)
                && cleaveResultsEquivalent(normalizedEdge, pixelEdge),
            "Level.cleaveEdge.crossSpace preserves equivalent selected and related faces across independent anisotropic UV units");
    }

    {
        struct AuthoredCorner
        {
            std::int32_t material{};
            sed::TVector position{};
            sed::TexVertex texVertex{};
        };

        sed::Level level{};
        level.kind = sed::ProjectType::JKDF2;
        level.clear();
        const std::int32_t sourceSector = sed::level::levelCreateSectorPrimitiveBox(
            level,
            sed::level::SectorPrimitive::Cube,
            sed::TVector{ -1.0, -1.0, -1.0 },
            sed::TVector{ 1.0, 1.0, 1.0 });
        check(sourceSector == 0, "Level.cleaveSector.uvPreservation: source sector created");

        std::vector<AuthoredCorner> authoredCorners{};
        if (sourceSector == 0)
        {
            sed::Sector& sector = level.sectors[0];
            for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
            {
                sed::Surface& surface = sector.surfaces[surfaceIndex];
                surface.nMat = static_cast<std::int32_t>(100 + surfaceIndex);
                surface.uScale = static_cast<float>(
                    -0.03125 - 0.015625 * surfaceIndex);
                surface.vScale = static_cast<float>(
                    0.046875 + 0.0234375 * surfaceIndex);
                for (std::size_t cornerIndex = 0; cornerIndex < surface.texVertices.size(); ++cornerIndex)
                {
                    sed::TexVertex& texVertex = surface.texVertices[cornerIndex];
                    texVertex.u = static_cast<float>(3.125 + 41.75 * surfaceIndex + 9.5 * cornerIndex);
                    texVertex.v = static_cast<float>(-7.25 - 33.5 * surfaceIndex + 5.25 * cornerIndex);
                    texVertex.color = sed::ColorF{
                        static_cast<float>(0.02 * (surfaceIndex + 1)),
                        static_cast<float>(0.04 * (cornerIndex + 1)),
                        static_cast<float>(0.03 * (surfaceIndex + cornerIndex + 1)),
                        static_cast<float>(0.35 + 0.05 * cornerIndex)
                    };

                    const std::int32_t vertexIndex = surface.vertices[cornerIndex];
                    const sed::Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                    authoredCorners.push_back(AuthoredCorner{
                        surface.nMat,
                        sed::TVector{ vertex.x, vertex.y, vertex.z },
                        texVertex
                    });
                }
            }
        }

        const bool ok = sed::level::levelCleaveSector(
            level,
            sourceSector,
            sed::TVector::right,
            0.25,
            0.0,
            0.0);
        check(ok, "Level.cleaveSector.uvPreservation: succeeds");
        check(level.sectors.size() == 2, "Level.cleaveSector.uvPreservation: creates two sectors");

        std::size_t retainedAuthoredCorners = 0;
        for (const sed::Sector& sector : level.sectors)
        {
            for (const sed::Surface& surface : sector.surfaces)
            {
                if ( surface.nMat >= 100 && surface.nMat < 106 )
                {
                    const std::size_t sourceSurfaceIndex =
                        static_cast<std::size_t>(surface.nMat - 100);
                    check(surface.uScale == static_cast<float>(
                                -0.03125
                                - 0.015625 * sourceSurfaceIndex)
                            && surface.vScale == static_cast<float>(
                                0.046875
                                + 0.0234375 * sourceSurfaceIndex),
                        "Level.cleaveSector.uvPreservation: every retained or split authored face keeps its exact scale metadata");
                }

                for (std::size_t cornerIndex = 0; cornerIndex < surface.vertices.size(); ++cornerIndex)
                {
                    const std::int32_t vertexIndex = surface.vertices[cornerIndex];
                    if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size()
                        || cornerIndex >= surface.texVertices.size())
                    {
                        continue;
                    }

                    const sed::Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                    const auto source = std::find_if(
                        authoredCorners.begin(),
                        authoredCorners.end(),
                        [&](const AuthoredCorner& corner)
                        {
                            return corner.material == surface.nMat
                                && close(corner.position.x, vertex.x)
                                && close(corner.position.y, vertex.y)
                                && close(corner.position.z, vertex.z);
                        });
                    if (source != authoredCorners.end())
                    {
                        ++retainedAuthoredCorners;
                        check(
                            sameTexVertex(surface.texVertices[cornerIndex], source->texVertex),
                            "Level.cleaveSector.uvPreservation: every retained authored corner remains exact");
                    }
                }
            }
        }
        check(
            retainedAuthoredCorners == authoredCorners.size(),
            "Level.cleaveSector.uvPreservation: every authored corner survives exactly once");
    }

    {
        sed::Level level{};
        sed::Sector& sourceSector = level.newSector();
        sourceSector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
        };
        sed::Surface source{};
        source.vertices = { 0, 1, 2, 3 };
        source.texVertices = {
            { 10.0f, 20.0f, sed::ColorF{ 0.0f, 0.2f, 0.4f, 0.6f } },
            { 110.0f, 20.0f, sed::ColorF{ 1.0f, 0.4f, 0.6f, 0.8f } },
            { 110.0f, 220.0f, sed::ColorF{ 0.8f, 0.6f, 0.4f, 0.2f } },
            { 10.0f, 220.0f, sed::ColorF{ 0.2f, 0.4f, 0.6f, 1.0f } },
        };
        source.adjoin = sed::SurfaceRef{ 1, 0 };
        source.nAdj = 0;
        source.normal = sed::level::surfaceCalcNormalNewell(sourceSector, source);
        const std::vector<std::int32_t> sourceOriginalVertexOrder = source.vertices;
        const std::vector<sed::TexVertex> sourceOriginals = source.texVertices;
        sourceSector.surfaces.push_back(source);
        const std::vector<sed::Vertex> sharedVertices = sourceSector.vertices;

        sed::Sector& mirrorSector = level.newSector();
        mirrorSector.vertices = sharedVertices;
        sed::Surface mirror{};
        mirror.vertices = { 0, 3, 2, 1 };
        mirror.texVertices = {
            { -100.0f, -200.0f, sed::ColorF{ 0.0f, 0.0f, 0.2f, 0.4f } },
            { -100.0f, 300.0f, sed::ColorF{ 0.2f, 0.4f, 0.6f, 0.8f } },
            { 500.0f, 300.0f, sed::ColorF{ 0.6f, 0.8f, 1.0f, 0.4f } },
            { 500.0f, -200.0f, sed::ColorF{ 1.0f, 0.6f, 0.4f, 0.2f } },
        };
        mirror.adjoin = sed::SurfaceRef{ 0, 0 };
        mirror.nAdj = 0;
        mirror.normal = sed::level::surfaceCalcNormalNewell(mirrorSector, mirror);
        const std::vector<std::int32_t> mirrorOriginalVertexOrder = mirror.vertices;
        const std::vector<sed::TexVertex> mirrorOriginals = mirror.texVertices;
        mirrorSector.surfaces.push_back(mirror);

        const bool ok = sed::level::levelCleaveSurface(
            level,
            sed::SurfaceRef{ 0, 0 },
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(ok, "Level.cleaveSurface.adjoinUvPreservation: succeeds");
        check(
            level.sectors[0].surfaces.size() == 2 && level.sectors[1].surfaces.size() == 2,
            "Level.cleaveSurface.adjoinUvPreservation: both portal sides split");

        const auto checkPortalSide = [&](const sed::Sector& sector,
                                         const std::vector<std::int32_t>& originalVertexOrder,
                                         const std::vector<sed::TexVertex>& originals,
                                         const sed::TexVertex& expectedBottom,
                                         const sed::TexVertex& expectedTop,
                                         const char* const originalLabel,
                                         const char* const insertedLabel)
        {
            for ( std::size_t originalCorner = 0; originalCorner < originals.size(); ++originalCorner )
            {
                std::size_t occurrenceCount = 0;
                for ( const sed::Surface& surface : sector.surfaces )
                {
                    const sed::TexVertex* actual = findSurfaceTexVertex(
                        surface,
                        originalVertexOrder[originalCorner]);
                    if ( actual != nullptr )
                    {
                        ++occurrenceCount;
                        check(sameTexVertex(*actual, originals[originalCorner]), originalLabel);
                    }
                }
                check(occurrenceCount == 1, originalLabel);
            }

            const std::int32_t bottomCut = findVertexIndex(sector, 0.5, 0.0, 0.0);
            const std::int32_t topCut = findVertexIndex(sector, 0.5, 2.0, 0.0);
            std::size_t bottomOccurrences = 0;
            std::size_t topOccurrences = 0;
            for ( const sed::Surface& surface : sector.surfaces )
            {
                if ( const sed::TexVertex* actual = findSurfaceTexVertex(surface, bottomCut) )
                {
                    ++bottomOccurrences;
                    check(closeTexVertex(*actual, expectedBottom), insertedLabel);
                }
                if ( const sed::TexVertex* actual = findSurfaceTexVertex(surface, topCut) )
                {
                    ++topOccurrences;
                    check(closeTexVertex(*actual, expectedTop), insertedLabel);
                }
            }
            check(bottomOccurrences == 2 && topOccurrences == 2, insertedLabel);
        };

        checkPortalSide(
            level.sectors[0],
            sourceOriginalVertexOrder,
            sourceOriginals,
            sed::TexVertex{ 35.0f, 20.0f, sed::ColorF{ 0.25f, 0.25f, 0.45f, 0.65f } },
            sed::TexVertex{ 35.0f, 220.0f, sed::ColorF{ 0.35f, 0.45f, 0.55f, 0.8f } },
            "Level.cleaveSurface.adjoinUvPreservation: source corners exact",
            "Level.cleaveSurface.adjoinUvPreservation: source cut corners interpolated");
        checkPortalSide(
            level.sectors[1],
            mirrorOriginalVertexOrder,
            mirrorOriginals,
            sed::TexVertex{ 50.0f, -200.0f, sed::ColorF{ 0.25f, 0.15f, 0.25f, 0.35f } },
            sed::TexVertex{ 50.0f, 300.0f, sed::ColorF{ 0.3f, 0.5f, 0.7f, 0.7f } },
            "Level.cleaveSurface.adjoinUvPreservation: mirror corners exact",
            "Level.cleaveSurface.adjoinUvPreservation: mirror cut corners interpolated");
    }

    {
        sed::Level level{};
        sed::Sector& sourceSector = level.newSector();
        sourceSector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
            { 1.0, -1.0, 0.0, 0 },
        };

        sed::Surface source{};
        source.nMat = 17;
        source.vertices = { 0, 1, 2, 3 };
        source.texVertices = {
            { 10.0f, 20.0f, sed::ColorF{ 0.0f, 0.2f, 0.4f, 0.6f } },
            { 110.0f, 20.0f, sed::ColorF{ 1.0f, 0.4f, 0.6f, 0.8f } },
            { 110.0f, 220.0f, sed::ColorF{ 0.8f, 0.6f, 0.4f, 0.2f } },
            { 10.0f, 220.0f, sed::ColorF{ 0.2f, 0.4f, 0.6f, 1.0f } },
        };
        source.adjoin = sed::SurfaceRef{ 1, 0 };
        source.nAdj = 0;
        const std::vector<std::pair<std::int32_t, sed::TexVertex>> sourceOriginals{
            { 0, source.texVertices[0] },
            { 1, source.texVertices[1] },
            { 2, source.texVertices[2] },
            { 3, source.texVertices[3] },
        };
        sourceSector.surfaces.push_back(source);

        sed::Surface adjacent{};
        adjacent.nMat = 18;
        adjacent.vertices = { 0, 4, 1 };
        adjacent.texVertices = {
            { -30.0f, 4.0f, sed::ColorF{ 0.1f, 0.3f, 0.5f, 0.7f } },
            { 5.0f, -50.0f, sed::ColorF{ 0.5f, 0.4f, 0.3f, 0.2f } },
            { 70.0f, 14.0f, sed::ColorF{ 0.9f, 0.7f, 0.5f, 0.3f } },
        };
        const std::vector<std::pair<std::int32_t, sed::TexVertex>> adjacentOriginals{
            { 0, adjacent.texVertices[0] },
            { 4, adjacent.texVertices[1] },
            { 1, adjacent.texVertices[2] },
        };
        sourceSector.surfaces.push_back(adjacent);
        const std::vector<sed::Vertex> sharedVertices = sourceSector.vertices;

        sed::Sector& mirrorSector = level.newSector();
        mirrorSector.vertices = {
            sharedVertices[2],
            sharedVertices[0],
            sharedVertices[3],
            sharedVertices[1],
            sharedVertices[4],
        };
        sed::Surface mirror{};
        mirror.nMat = 19;
        mirror.vertices = { 1, 2, 0, 3 };
        mirror.texVertices = {
            { -100.0f, -200.0f, sed::ColorF{ 0.0f, 0.0f, 0.2f, 0.4f } },
            { -100.0f, 300.0f, sed::ColorF{ 0.2f, 0.4f, 0.6f, 0.8f } },
            { 500.0f, 300.0f, sed::ColorF{ 0.6f, 0.8f, 1.0f, 0.4f } },
            { 500.0f, -200.0f, sed::ColorF{ 1.0f, 0.6f, 0.4f, 0.2f } },
        };
        mirror.adjoin = sed::SurfaceRef{ 0, 0 };
        mirror.nAdj = 0;
        const std::vector<std::pair<std::int32_t, sed::TexVertex>> mirrorOriginals{
            { 1, mirror.texVertices[0] },
            { 2, mirror.texVertices[1] },
            { 0, mirror.texVertices[2] },
            { 3, mirror.texVertices[3] },
        };
        mirrorSector.surfaces.push_back(mirror);

        const bool ok = sed::level::levelCleaveEdge(
            level,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(ok, "Level.cleaveEdge.adjoinUvPreservation: succeeds");
        checkSurfaceKeepsOriginalTextureVertices(
            level.sectors[0],
            source.nMat,
            sourceOriginals,
            "Level.cleaveEdge.adjoinUvPreservation: source corners exact");
        checkSurfaceKeepsOriginalTextureVertices(
            level.sectors[0],
            adjacent.nMat,
            adjacentOriginals,
            "Level.cleaveEdge.adjoinUvPreservation: adjacent corners exact");
        checkSurfaceKeepsOriginalTextureVertices(
            level.sectors[1],
            mirror.nMat,
            mirrorOriginals,
            "Level.cleaveEdge.adjoinUvPreservation: mirror corners exact");

        const std::int32_t sourceCut = findVertexIndex(level.sectors[0], 0.5, 0.0, 0.0);
        const std::int32_t mirrorCut = findVertexIndex(level.sectors[1], 0.5, 0.0, 0.0);
        check(sourceCut >= 0 && mirrorCut >= 0,
            "Level.cleaveEdge.adjoinUvPreservation: both sector cut vertices exist");
        const sed::TexVertex* sourceCutTex =
            findSurfaceTexVertex(level.sectors[0].surfaces[0], sourceCut);
        const sed::TexVertex* adjacentCutTex =
            findSurfaceTexVertex(level.sectors[0].surfaces[1], sourceCut);
        const sed::TexVertex* mirrorCutTex =
            findSurfaceTexVertex(level.sectors[1].surfaces[0], mirrorCut);
        check(
            sourceCutTex != nullptr
                && adjacentCutTex != nullptr
                && mirrorCutTex != nullptr
                && closeTexVertex(
                    *sourceCutTex,
                    sed::TexVertex{ 35.0f, 20.0f, sed::ColorF{ 0.25f, 0.25f, 0.45f, 0.65f } })
                && closeTexVertex(
                    *adjacentCutTex,
                    sed::TexVertex{ -5.0f, 6.5f, sed::ColorF{ 0.3f, 0.4f, 0.5f, 0.6f } })
                && closeTexVertex(
                    *mirrorCutTex,
                    sed::TexVertex{ 50.0f, -200.0f, sed::ColorF{ 0.25f, 0.15f, 0.25f, 0.35f } }),
            "Level.cleaveEdge.adjoinUvPreservation: each surface interpolates its own corner frame");
        check(
            !level.sectors[0].surfaces[0].normal.isZero()
                && !level.sectors[0].surfaces[1].normal.isZero()
                && !level.sectors[1].surfaces[0].normal.isZero()
                && closeVec(
                    level.sectors[0].surfaces[0].normal,
                    sed::level::surfaceCalcNormalNewell(
                        level.sectors[0],
                        level.sectors[0].surfaces[0]))
                && closeVec(
                    level.sectors[0].surfaces[1].normal,
                    sed::level::surfaceCalcNormalNewell(
                        level.sectors[0],
                        level.sectors[0].surfaces[1]))
                && closeVec(
                    level.sectors[1].surfaces[0].normal,
                    sed::level::surfaceCalcNormalNewell(
                        level.sectors[1],
                        level.sectors[1].surfaces[0])),
            "Level.cleaveEdge.adjoinUvPreservation: source, related, and mirror normals refreshed");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(2.0, 2.0, 0.0);
        const std::int32_t v3 = sector.newVertex(0.0, 2.0, 0.0);

        sed::Surface surface{};
        surface.vertices = { v0, v1, v2, v3 };
        surface.texVertices = {
            { -10.0f, 5.0f, sed::ColorBlack },
            { 30.0f, 15.0f, sed::ColorWhite },
            { 80.0f, 90.0f, sed::ColorBlue },
            { 20.0f, 100.0f, sed::ColorGreen },
        };
        surface.uScale = -13.25f;
        surface.vScale = 29.5f;
        const std::vector<std::pair<std::int32_t, sed::TexVertex>> originals{
            { v0, surface.texVertices[0] },
            { v1, surface.texVertices[1] },
            { v2, surface.texVertices[2] },
            { v3, surface.texVertices[3] },
        };
        (void)sector.addSurface(surface);

        const bool ok = sed::level::levelCleaveEdge(
            level,
            sed::SurfaceRef{ 0, 0 },
            0,
            sed::TVector::right,
            0.5,
            0.0,
            0.0);
        check(ok, "Level.cleaveEdge.uvPreservation: succeeds");
        check(level.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && sector.surfaces[0].uScale == surface.uScale
                && sector.surfaces[0].vScale == surface.vScale,
            "Level.cleaveEdge.uvPreservation: coordinate-space tag and scale metadata stay exact");
        checkSurfaceKeepsOriginalTextureVertices(
            sector,
            surface.nMat,
            originals,
            "Level.cleaveEdge.uvPreservation: authored corner exact");

        const std::int32_t cut = findVertexIndex(sector, 0.5, 0.0, 0.0);
        const sed::TexVertex* cutUv = findSurfaceTexVertex(sector.surfaces[0], cut);
        check(cutUv != nullptr, "Level.cleaveEdge.uvPreservation: cut corner exists");
        if (cutUv != nullptr)
        {
            check(
                close(cutUv->u, 0.0f) && close(cutUv->v, 7.5f)
                    && close(cutUv->color.r, 0.25f)
                    && close(cutUv->color.g, 0.25f)
                    && close(cutUv->color.b, 0.25f)
                    && close(cutUv->color.a, 1.0f),
                "Level.cleaveEdge.uvPreservation: cut corner interpolated");
        }
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(2.0, 2.0, 0.0);
        const std::int32_t v3 = sector.newVertex(0.0, 2.0, 0.0);

        sed::Surface surf{};
        surf.num         = 0;
        surf.vertices    = { v0, v1, v2, v3 };
        surf.texVertices = std::vector<sed::TexVertex>(surf.vertices.size());

        (void)sector.addSurface(std::move(surf));

        const bool ok = sed::level::levelCleaveSurface(level, sed::SurfaceRef{ sector.num, 0 }, sed::TVector::right, 1.0, 0.0, 0.0);
        check(ok, "Level.cleaveSurface: ok");

        check(sector.surfaces.size() == 2, "Level.cleaveSurface: surface count");
        if (sector.surfaces.size() >= 2)
        {
            check(sector.surfaces[0].vertices.size() == 4, "Level.cleaveSurface: left vertex count");
            check(sector.surfaces[1].vertices.size() == 4, "Level.cleaveSurface: right vertex count");
        }

        const std::int32_t cut0 = findVertexIndex(sector, 1.0, 0.0, 0.0);
        const std::int32_t cut1 = findVertexIndex(sector, 1.0, 2.0, 0.0);

        check(cut0 >= 0, "Level.cleaveSurface: cut0 exists");
        check(cut1 >= 0, "Level.cleaveSurface: cut1 exists");
        check(cut0 != cut1, "Level.cleaveSurface: cut vertices distinct");

        check(sector.vertices.size() == 6, "Level.cleaveSurface: vertex count");

        if (sector.surfaces.size() >= 2 && cut0 >= 0 && cut1 >= 0)
        {
            check(containsVertex(sector.surfaces[0].vertices, cut0), "Level.cleaveSurface: left contains cut0");
            check(containsVertex(sector.surfaces[0].vertices, cut1), "Level.cleaveSurface: left contains cut1");
            check(containsVertex(sector.surfaces[1].vertices, cut0), "Level.cleaveSurface: right contains cut0");
            check(containsVertex(sector.surfaces[1].vertices, cut1), "Level.cleaveSurface: right contains cut1");
        }
    }

    {
        sed::Level level{};
        const bool ok = sed::level::levelCleaveSurface(level, sed::SurfaceRef{ 0, 0 }, sed::TVector::right, 1.0, 0.0, 0.0);
        check(!ok, "Level.cleaveSurface.invalidRef: fails");
        check(level.sectors.empty(), "Level.cleaveSurface.invalidRef: no sectors added");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(2.0, 2.0, 0.0);
        const std::int32_t v3 = sector.newVertex(0.0, 2.0, 0.0);

        sed::Surface surf{};
        surf.vertices    = { v0, v1, v2, v3 };
        surf.texVertices = std::vector<sed::TexVertex>(surf.vertices.size());
        (void)sector.addSurface(std::move(surf));

        const std::size_t oldSurfaceCount = sector.surfaces.size();
        const std::size_t oldVertexCount = sector.vertices.size();
        const bool ok = sed::level::levelCleaveSurface(level, sed::SurfaceRef{ sector.num, 0 }, sed::TVector::right, 3.0, 0.0, 0.0);
        check(!ok, "Level.cleaveSurface.noSplit: fails");
        check(sector.surfaces.size() == oldSurfaceCount, "Level.cleaveSurface.noSplit: surface count unchanged");
        check(sector.vertices.size() == oldVertexCount, "Level.cleaveSurface.noSplit: vertex count unchanged");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(2.0, 0.0, 0.0);

        sed::Surface surf{};
        surf.vertices    = { v0, v1 };
        surf.texVertices = std::vector<sed::TexVertex>(surf.vertices.size());
        (void)sector.addSurface(std::move(surf));

        const bool ok = sed::level::levelCleaveSurface(level, sed::SurfaceRef{ sector.num, 0 }, sed::TVector::right, 1.0, 0.0, 0.0);
        check(!ok, "Level.cleaveSurface.degenerate: fails");
        check(sector.surfaces.size() == 1, "Level.cleaveSurface.degenerate: surface count unchanged");
        check(sector.vertices.size() == 2, "Level.cleaveSurface.degenerate: vertex count unchanged");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(2.0, 2.0, 0.0);
        const std::int32_t v3 = sector.newVertex(0.0, 2.0, 0.0);
        const std::int32_t v4 = sector.newVertex(1.0, -1.0, 0.0);

        sed::Surface quad{};
        quad.num         = 0;
        quad.vertices    = { v0, v1, v2, v3 };
        quad.texVertices = std::vector<sed::TexVertex>(quad.vertices.size());

        sed::Surface tri{};
        tri.num         = 1;
        tri.vertices    = { v0, v1, v4 };
        tri.texVertices = std::vector<sed::TexVertex>(tri.vertices.size());

        (void)sector.addSurface(std::move(quad));
        (void)sector.addSurface(std::move(tri));

        const bool ok = sed::level::levelCleaveSurface(level, sed::SurfaceRef{ sector.num, 0 }, sed::TVector::right, 1.0, 0.0, 0.0);
        check(ok, "Level.cleaveSurface.sharedEdge: ok");

        check(sector.surfaces.size() == 3, "Level.cleaveSurface.sharedEdge: surface count");
        check(sector.vertices.size() == 7, "Level.cleaveSurface.sharedEdge: vertex count");

        const std::int32_t cut = findVertexIndex(sector, 1.0, 0.0, 0.0);
        check(cut >= 0, "Level.cleaveSurface.sharedEdge: cut exists");

        if (sector.surfaces.size() >= 2 && cut >= 0)
        {
            const sed::Surface& s = sector.surfaces[1];
            check(s.vertices.size() == 4, "Level.cleaveSurface.sharedEdge: tri vertex count");
            check(s.texVertices.size() == 4, "Level.cleaveSurface.sharedEdge: tri tex vertex count");
            check(containsVertex(s.vertices, cut), "Level.cleaveSurface.sharedEdge: tri contains cut");
        }
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(2.0, 2.0, 0.0);
        const std::int32_t v3 = sector.newVertex(0.0, 2.0, 0.0);
        const std::int32_t v4 = sector.newVertex(1.0, -1.0, 0.0);

        sed::Surface quad{};
        quad.vertices    = { v0, v1, v2, v3 };
        quad.texVertices = std::vector<sed::TexVertex>(quad.vertices.size());
        (void)sector.addSurface(std::move(quad));

        sed::Surface tri{};
        tri.vertices    = { v0, v1, v4 };
        tri.texVertices = std::vector<sed::TexVertex>(tri.vertices.size());
        (void)sector.addSurface(std::move(tri));

        const bool ok = sed::level::levelCleaveEdge(level, sed::SurfaceRef{ sector.num, 0 }, 0, sed::TVector::right, 1.0, 0.0, 0.0);
        check(ok, "Level.cleaveEdge.sharedEdge: ok");

        const std::int32_t cut = findVertexIndex(sector, 1.0, 0.0, 0.0);
        check(cut >= 0, "Level.cleaveEdge.sharedEdge: cut exists");
        check(sector.vertices.size() == 6, "Level.cleaveEdge.sharedEdge: vertex count");
        check(sector.surfaces.size() == 2, "Level.cleaveEdge.sharedEdge: surface count");
        if (sector.surfaces.size() == 2 && cut >= 0)
        {
            check(sector.surfaces[0].vertices.size() == 5, "Level.cleaveEdge.sharedEdge: quad vertex count");
            check(sector.surfaces[1].vertices.size() == 4, "Level.cleaveEdge.sharedEdge: tri vertex count");
            check(containsVertex(sector.surfaces[0].vertices, cut), "Level.cleaveEdge.sharedEdge: quad contains cut");
            check(containsVertex(sector.surfaces[1].vertices, cut), "Level.cleaveEdge.sharedEdge: tri contains cut");
        }
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(2.0, 2.0, 0.0);
        const std::int32_t v3 = sector.newVertex(0.0, 2.0, 0.0);

        sed::Surface surf{};
        surf.vertices    = { v0, v1, v2, v3 };
        surf.texVertices = std::vector<sed::TexVertex>(surf.vertices.size());
        (void)sector.addSurface(std::move(surf));

        const std::size_t oldVertexCount = sector.vertices.size();
        const bool badEdge = sed::level::levelCleaveEdge(level, sed::SurfaceRef{ sector.num, 0 }, 99, sed::TVector::right, 1.0, 0.0, 0.0);
        check(!badEdge, "Level.cleaveEdge.invalidEdge: fails");
        check(sector.vertices.size() == oldVertexCount, "Level.cleaveEdge.invalidEdge: vertex count unchanged");

        const bool endpointOnPlane = sed::level::levelCleaveEdge(level, sed::SurfaceRef{ sector.num, 0 }, 0, sed::TVector::right, 0.0, 0.0, 0.0);
        check(!endpointOnPlane, "Level.cleaveEdge.endpointOnPlane: fails");
        check(sector.vertices.size() == oldVertexCount, "Level.cleaveEdge.endpointOnPlane: vertex count unchanged");
        check(sector.surfaces.size() == 1, "Level.cleaveEdge.endpointOnPlane: surface count unchanged");
    }

    {
        sed::Level level{};
        (void)level.newSector();
        (void)level.newSector();

        sed::Sector& sec0 = level.sectors[0];
        sed::Sector& sec1 = level.sectors[1];

        const std::int32_t a0 = sec0.newVertex(0.0, 0.0, 0.0);
        const std::int32_t a1 = sec0.newVertex(2.0, 0.0, 0.0);
        const std::int32_t a2 = sec0.newVertex(2.0, 2.0, 0.0);
        const std::int32_t a3 = sec0.newVertex(0.0, 2.0, 0.0);

        sed::Surface s0{};
        s0.num         = 0;
        s0.vertices    = { a0, a1, a2, a3 };
        s0.texVertices = std::vector<sed::TexVertex>(s0.vertices.size());
        (void)sec0.addSurface(std::move(s0));

        const std::int32_t b0 = sec1.newVertex(0.0, 0.0, 0.0);
        const std::int32_t b1 = sec1.newVertex(2.0, 0.0, 0.0);
        const std::int32_t b2 = sec1.newVertex(2.0, 2.0, 0.0);
        const std::int32_t b3 = sec1.newVertex(0.0, 2.0, 0.0);

        sed::Surface s1{};
        s1.num         = 0;
        s1.vertices    = { b0, b3, b2, b1 };
        s1.texVertices = std::vector<sed::TexVertex>(s1.vertices.size());
        (void)sec1.addSurface(std::move(s1));

        const double ppu = sed::level::levelPixelPerUnit(level);
        sed::level::surfaceRecalcNewAll(ppu, sec0, sec0.surfaces[0], 0);
        sed::level::surfaceRecalcNewAll(ppu, sec1, sec1.surfaces[0], 0);

        (void)sed::level::levelSysAdjoinSurfaces(level, sed::SurfaceRef{ sec0.num, 0 }, sed::SurfaceRef{ sec1.num, 0 }, true);

        const bool ok = sed::level::levelCleaveSurface(level, sed::SurfaceRef{ sec0.num, 0 }, sed::TVector::right, 1.0, 0.0, 0.0);
        check(ok, "Level.cleaveSurface.adjoin: ok");

        check(sec0.surfaces.size() == 2, "Level.cleaveSurface.adjoin: sec0 surface count");
        check(sec1.surfaces.size() == 2, "Level.cleaveSurface.adjoin: sec1 surface count");

        for (std::size_t i = 0; i < sec0.surfaces.size(); ++i)
        {
            const sed::Surface& s = sec0.surfaces[i];
            check(s.adjoin.has_value(), "Level.cleaveSurface.adjoin: sec0 surface has adjoin");
            if (!s.adjoin)
            {
                continue;
            }

            check(s.adjoin->sector == sec1.num, "Level.cleaveSurface.adjoin: adjoin sector");
            check(s.adjoin->surface >= 0, "Level.cleaveSurface.adjoin: adjoin surface index");

            const std::size_t otherIndex = static_cast<std::size_t>(s.adjoin->surface);
            check(otherIndex < sec1.surfaces.size(), "Level.cleaveSurface.adjoin: adjoin surface in range");
            if (otherIndex >= sec1.surfaces.size())
            {
                continue;
            }

            const sed::Surface& other = sec1.surfaces[otherIndex];
            check(other.adjoin.has_value(), "Level.cleaveSurface.adjoin: other has adjoin");
            if (!other.adjoin)
            {
                continue;
            }

            check(other.adjoin->sector == sec0.num, "Level.cleaveSurface.adjoin: other.adjoin sector");
            check(other.adjoin->surface == static_cast<std::int32_t>(i), "Level.cleaveSurface.adjoin: other.adjoin surface index");

            check(sed::level::surfaceDoOverlap(sec0, s, sec1, other), "Level.cleaveSurface.adjoin: overlap");
        }
    }

    {
        sed::Level level{};
        sed::Sector& sector = sed::level::levelCreateCube(level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right);
        const std::int32_t sectorIndex = sector.num;

        sed::Thing& left = level.newThing();
        left.sector = sectorIndex;
        left.x      = 0.25;
        left.y      = 0.5;
        left.z      = 0.5;

        sed::Thing& right = level.newThing();
        right.sector = sectorIndex;
        right.x      = 0.75;
        right.y      = 0.5;
        right.z      = 0.5;

        const std::size_t oldSectorCount = level.sectors.size();
        const bool ok = sed::level::levelCleaveSector(level, sectorIndex, sed::TVector::right, 0.5, 0.0, 0.0);
        check(ok, "Level.cleaveSector: ok");
        check(level.sectors.size() == oldSectorCount + 1, "Level.cleaveSector: sector count");

        const std::int32_t newSectorIndex = static_cast<std::int32_t>(oldSectorCount);
        check(level.things[0].sector == newSectorIndex, "Level.cleaveSector: left relayer");
        check(level.things[1].sector == sectorIndex, "Level.cleaveSector: right relayer");

        const sed::Sector& sec0 = level.sectors[static_cast<std::size_t>(sectorIndex)];
        const sed::Sector& sec1 = level.sectors[static_cast<std::size_t>(newSectorIndex)];

        std::int32_t cap0 = -1;
        std::int32_t cap1 = -1;

        for (std::int32_t i = 0; i < static_cast<std::int32_t>(sec0.surfaces.size()); ++i)
        {
            const sed::Surface& s = sec0.surfaces[static_cast<std::size_t>(i)];
            if (s.adjoin && s.adjoin->sector == newSectorIndex)
            {
                cap0 = i;
                break;
            }
        }

        for (std::int32_t i = 0; i < static_cast<std::int32_t>(sec1.surfaces.size()); ++i)
        {
            const sed::Surface& s = sec1.surfaces[static_cast<std::size_t>(i)];
            if (s.adjoin && s.adjoin->sector == sectorIndex)
            {
                cap1 = i;
                break;
            }
        }

        check(cap0 >= 0, "Level.cleaveSector: cap surface in sec0");
        check(cap1 >= 0, "Level.cleaveSector: cap surface in sec1");

        if (cap0 >= 0 && cap1 >= 0)
        {
            const sed::Surface& s0 = sec0.surfaces[static_cast<std::size_t>(cap0)];
            const sed::Surface& s1 = sec1.surfaces[static_cast<std::size_t>(cap1)];

            check(s0.adjoin.has_value(), "Level.cleaveSector: sec0 cap has adjoin");
            check(s1.adjoin.has_value(), "Level.cleaveSector: sec1 cap has adjoin");

            check(sed::level::surfaceDoOverlap(sec0, s0, sec1, s1), "Level.cleaveSector: cap overlap");
            check(s0.normal.Dot(s1.normal) < -0.999, "Level.cleaveSector: cap normals opposite");

            for (const std::int32_t vi : s0.vertices)
            {
                check(vi >= 0, "Level.cleaveSector: cap vi >= 0");
                check(static_cast<std::size_t>(vi) < sec0.vertices.size(), "Level.cleaveSector: cap vi range");

                const sed::Vertex& v = sec0.vertices[static_cast<std::size_t>(vi)];
                check(close(v.x, 0.5), "Level.cleaveSector: cap sec0 vx on plane");
            }

            check(findVertexIndex(sec0, 0.5, 0.0, 0.0) >= 0, "Level.cleaveSector: sec0 cut v0");
            check(findVertexIndex(sec0, 0.5, 1.0, 0.0) >= 0, "Level.cleaveSector: sec0 cut v1");
            check(findVertexIndex(sec0, 0.5, 1.0, 1.0) >= 0, "Level.cleaveSector: sec0 cut v2");
            check(findVertexIndex(sec0, 0.5, 0.0, 1.0) >= 0, "Level.cleaveSector: sec0 cut v3");

            check(findVertexIndex(sec1, 0.5, 0.0, 0.0) >= 0, "Level.cleaveSector: sec1 cut v0");
            check(findVertexIndex(sec1, 0.5, 1.0, 0.0) >= 0, "Level.cleaveSector: sec1 cut v1");
            check(findVertexIndex(sec1, 0.5, 1.0, 1.0) >= 0, "Level.cleaveSector: sec1 cut v2");
            check(findVertexIndex(sec1, 0.5, 0.0, 1.0) >= 0, "Level.cleaveSector: sec1 cut v3");
        }
    }

    {
        sed::Level level{};
        sed::Sector& sector = sed::level::levelCreateCube(
            level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right);
        const auto malformedSurface = std::find_if(
            sector.surfaces.begin(),
            sector.surfaces.end(),
            [&sector](const sed::Surface& surface)
            {
                bool hasNegative = false;
                bool hasPositive = false;
                for (const std::int32_t vertexIndex : surface.vertices)
                {
                    const double x = sector.vertices[
                        static_cast<std::size_t>(vertexIndex)].x;
                    hasNegative = hasNegative || x < 0.5;
                    hasPositive = hasPositive || x > 0.5;
                }
                return hasNegative && hasPositive;
            });
        check(malformedSurface != sector.surfaces.end(),
            "Level.cleaveSector.lateFailureRollback: crossing source face found");
        sed::Surface& malformed = malformedSurface == sector.surfaces.end()
            ? sector.surfaces.front()
            : *malformedSurface;

        const auto positiveCorner = std::find_if(
            malformed.vertices.begin(),
            malformed.vertices.end(),
            [&sector](const std::int32_t vertexIndex)
            {
                return sector.vertices[static_cast<std::size_t>(vertexIndex)].x > 0.5;
            });
        const std::ptrdiff_t cornerOffset = std::distance(
            malformed.vertices.begin(), positiveCorner);
        std::rotate(
            malformed.vertices.begin(),
            positiveCorner,
            malformed.vertices.end());
        std::rotate(
            malformed.texVertices.begin(),
            malformed.texVertices.begin() + cornerOffset,
            malformed.texVertices.end());
        malformed.texVertices.pop_back();

        for (std::size_t vertexIndex = 0; vertexIndex < sector.vertices.size(); ++vertexIndex)
        {
            sector.vertices[vertexIndex].mark = static_cast<std::int32_t>(vertexIndex) + 11;
        }

        const std::vector<sed::Vertex> originalVertices = sector.vertices;
        const std::vector<sed::Surface> originalSurfaces = sector.surfaces;
        const std::size_t originalSectorCount = level.sectors.size();
        const bool ok = sed::level::levelCleaveSector(
            level, sector.num, sed::TVector::right, 0.5, 0.0, 0.0);

        check(!ok,
            "Level.cleaveSector.lateFailureRollback: malformed crossing face is rejected after staged surface work");
        check(level.sectors.size() == originalSectorCount
                && level.sectors[0].surfaces.size() == originalSurfaces.size(),
            "Level.cleaveSector.lateFailureRollback: no partial Sector or cap is published");
        check(sameVertexVectors(level.sectors[0].vertices, originalVertices),
            "Level.cleaveSector.lateFailureRollback: pre-cleave coordinates and marks are restored exactly");
        bool surfacesUnchanged = level.sectors[0].surfaces.size() == originalSurfaces.size();
        for (std::size_t surfaceIndex = 0;
             surfacesUnchanged && surfaceIndex < originalSurfaces.size();
             ++surfaceIndex)
        {
            const sed::Surface& actual = level.sectors[0].surfaces[surfaceIndex];
            const sed::Surface& expected = originalSurfaces[surfaceIndex];
            surfacesUnchanged = actual.vertices == expected.vertices
                && sameTextureVertexVectors(actual.texVertices, expected.texVertices)
                && actual.adjoin == expected.adjoin
                && actual.nAdj == expected.nAdj;
        }
        check(surfacesUnchanged,
            "Level.cleaveSector.lateFailureRollback: split faces and authored texture corners are not published");
    }

    {
        constexpr double textureWidth = 64.0;
        constexpr double textureHeight = 32.0;

        sed::Level normalized{};
        normalized.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& sector = sed::level::levelCreateCube(
            normalized, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right);
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            sed::Surface& surface = sector.surfaces[surfaceIndex];
            surface.nMat = 0;
            surface.uScale = 0.03125f;
            surface.vScale = 0.09375f;
            for (std::size_t corner = 0; corner < surface.texVertices.size(); ++corner)
            {
                surface.texVertices[corner].u = -1.75f
                    + static_cast<float>(surfaceIndex) * 0.625f
                    + static_cast<float>(corner) * 1.375f;
                surface.texVertices[corner].v = 2.5f
                    - static_cast<float>(surfaceIndex) * 0.75f
                    - static_cast<float>(corner) * 1.125f;
            }
        }

        sed::Level pixels = normalized;
        const bool hasSignedAndMultiTileSource = std::any_of(
            sector.surfaces.begin(),
            sector.surfaces.end(),
            [](const sed::Surface& surface)
            {
                return std::any_of(
                    surface.texVertices.begin(),
                    surface.texVertices.end(),
                    [](const sed::TexVertex& textureVertex)
                    {
                        return textureVertex.u < 0.0f
                            && textureVertex.v > 1.0f;
                    });
            });
        check(hasSignedAndMultiTileSource,
            "Level.cleaveSector.capUvSpaceEquivalence: source fixture contains signed multi-tile UVs");

        const auto dimensions = [](const sed::Surface&)
            -> std::optional<sed::level::UvTextureDimensions>
        {
            return sed::level::UvTextureDimensions{ textureWidth, textureHeight };
        };
        const sed::level::LevelUvConversionResult toPixels =
            sed::level::convertLevelUvCoordinateSpace(
                pixels, sed::UvCoordinateSpace::TexturePixels, dimensions);
        check(static_cast<bool>(toPixels),
            "Level.cleaveSector.capUvSpaceEquivalence: anisotropic pixel fixture converts");

        const bool normalizedOk = sed::level::levelCleaveSector(
            normalized, 0, sed::TVector::right, 0.5, 0.0, 0.0);
        const bool pixelOk = sed::level::levelCleaveSector(
            pixels, 0, sed::TVector::right, 0.5, 0.0, 0.0);
        check(normalizedOk && pixelOk,
            "Level.cleaveSector.capUvSpaceEquivalence: normalized and pixel fixtures cleave");

        const sed::level::LevelUvConversionResult backToNormalized =
            sed::level::convertLevelUvCoordinateSpace(
                pixels, sed::UvCoordinateSpace::Normalized, dimensions);
        check(static_cast<bool>(backToNormalized),
            "Level.cleaveSector.capUvSpaceEquivalence: pixel result converts back to normalized space");

        bool capsEquivalent = normalized.sectors.size() == 2
            && pixels.sectors.size() == normalized.sectors.size();
        for (std::int32_t sectorIndex = 0;
             capsEquivalent && sectorIndex < 2;
             ++sectorIndex)
        {
            const std::int32_t partnerSector = sectorIndex == 0 ? 1 : 0;
            const sed::Sector& normalizedSector =
                normalized.sectors[static_cast<std::size_t>(sectorIndex)];
            const sed::Sector& pixelSector =
                pixels.sectors[static_cast<std::size_t>(sectorIndex)];
            const auto findCap = [partnerSector](const sed::Sector& candidate)
                -> const sed::Surface*
            {
                const auto cap = std::find_if(
                    candidate.surfaces.begin(),
                    candidate.surfaces.end(),
                    [partnerSector](const sed::Surface& surface)
                    {
                        return surface.adjoin
                            && surface.adjoin->sector == partnerSector;
                    });
                return cap == candidate.surfaces.end() ? nullptr : &*cap;
            };

            const sed::Surface* normalizedCap = findCap(normalizedSector);
            const sed::Surface* pixelCap = findCap(pixelSector);
            capsEquivalent = normalizedCap != nullptr
                && pixelCap != nullptr
                && normalizedCap->vertices == pixelCap->vertices
                && normalizedCap->texVertices.size() == pixelCap->texVertices.size()
                && normalizedCap->nMat == pixelCap->nMat
                && normalizedCap->surfaceFlags == pixelCap->surfaceFlags
                && normalizedCap->faceFlags == pixelCap->faceFlags
                && normalizedCap->adjoin == pixelCap->adjoin
                && close(normalizedCap->uScale, pixelCap->uScale)
                && close(normalizedCap->vScale, pixelCap->vScale)
                && close(normalizedCap->normal.x, pixelCap->normal.x)
                && close(normalizedCap->normal.y, pixelCap->normal.y)
                && close(normalizedCap->normal.z, pixelCap->normal.z);

            for (std::size_t corner = 0;
                 capsEquivalent && corner < normalizedCap->vertices.size();
                 ++corner)
            {
                const sed::Vertex& normalizedCorner = normalizedSector.vertices[
                    static_cast<std::size_t>(normalizedCap->vertices[corner])];
                const sed::Vertex& pixelCorner = pixelSector.vertices[
                    static_cast<std::size_t>(pixelCap->vertices[corner])];
                capsEquivalent = close(normalizedCorner.x, pixelCorner.x)
                    && close(normalizedCorner.y, pixelCorner.y)
                    && close(normalizedCorner.z, pixelCorner.z)
                    && closeTexVertex(
                        normalizedCap->texVertices[corner],
                        pixelCap->texVertices[corner]);
            }
        }
        check(capsEquivalent,
            "Level.cleaveSector.capUvSpaceEquivalence: generated cap faces, corners, and anisotropic scales are coordinate-space equivalent with signed multi-tile source UVs");
    }

    {
        sed::Level level{};
        const bool ok = sed::level::levelCleaveSector(level, 0, sed::TVector::right, 0.5, 0.0, 0.0);
        check(!ok, "Level.cleaveSector.invalidSector: fails");
        check(level.sectors.empty(), "Level.cleaveSector.invalidSector: no sectors added");
    }

    {
        sed::Level level{};
        sed::Sector& sector = sed::level::levelCreateCube(
            level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right);
        sector.id = 77;
        const std::int32_t originalSector = sector.num;

        sed::Thing& retainedThing = level.newThing();
        retainedThing.sector = originalSector;
        retainedThing.x = 0.25;
        retainedThing.y = 0.5;
        retainedThing.z = 0.5;
        sed::Thing& cutOffThing = level.newThing();
        cutOffThing.sector = originalSector;
        cutOffThing.x = 0.9;
        cutOffThing.y = 0.5;
        cutOffThing.z = 0.5;

        sed::Cog& cog = level.newCog();
        sed::CogValue sectorLink{};
        sectorLink.type = sed::ValueType::ptr;
        sectorLink.cogType = sed::CogType::sec;
        sectorLink.intValue = originalSector;
        cog.values.push_back(sectorLink);

        const bool ok = sed::level::levelCleaveSector(
            level, originalSector, sed::TVector::right, 0.8, 0.0, 0.0);
        check(ok && level.sectors.size() == 2,
            "Level.cleaveSector.identity: asymmetric cut succeeds");
        const std::int32_t appendedSector = 1;
        check(level.sectors[0].id == 77 && level.sectors[0].num == originalSector,
            "Level.cleaveSector.identity: retained body preserves id and num");
        check(sed::level::sectorContainsPoint(level.sectors[0], 0.25, 0.5, 0.5)
                && !sed::level::sectorContainsPoint(level.sectors[0], 0.9, 0.5, 0.5)
                && sed::level::sectorContainsPoint(level.sectors[1], 0.9, 0.5, 0.5),
            "Level.cleaveSector.identity: original slot keeps main body and appended slot is cut-off");
        check(level.things[0].sector == originalSector
                && level.things[1].sector == appendedSector,
            "Level.cleaveSector.identity: Thing containment follows exchanged geometry");
        check(level.cogs[0].values[0].intValue == originalSector,
            "Level.cleaveSector.identity: authored COG sector link remains on retained identity");
    }

    {
        sed::Level level{};
        sed::Sector& sector = sed::level::levelCreateCube(level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right);
        const std::int32_t sectorIndex = sector.num;
        const std::size_t oldSectorCount = level.sectors.size();
        const std::size_t oldSurfaceCount = sector.surfaces.size();

        const bool ok = sed::level::levelCleaveSector(level, sectorIndex, sed::TVector::right, 2.0, 0.0, 0.0);
        check(!ok, "Level.cleaveSector.noSplit: fails");
        check(level.sectors.size() == oldSectorCount, "Level.cleaveSector.noSplit: sector count unchanged");
        check(level.sectors[static_cast<std::size_t>(sectorIndex)].surfaces.size() == oldSurfaceCount,
            "Level.cleaveSector.noSplit: surface count unchanged");
    }

    {
        sed::Level level{};
        const std::int32_t sectorIndex =
            sed::level::levelCreateCube(level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right).num;

        const std::array planes{
            sed::level::SectorCleavePlane{ sed::TVector::right, sed::TVector::Create(0.5, 0.0, 0.0) },
            sed::level::SectorCleavePlane{ sed::TVector::forward, sed::TVector::Create(0.0, 0.5, 0.0) },
        };

        const sed::level::SectorCleaveBatchResult result =
            sed::level::levelCleaveSectorByPlanes(level, sectorIndex, planes);
        check(result.succeeded(), "Level.cleaveSectorByPlanes.perpendicular: succeeds");
        check(result.error == sed::level::SectorCleaveBatchError::None,
            "Level.cleaveSectorByPlanes.perpendicular: no error");
        check(result.outputSectors.size() == 4,
            "Level.cleaveSectorByPlanes.perpendicular: four descendants");
        check(level.sectors.size() == 4,
            "Level.cleaveSectorByPlanes.perpendicular: commits all sectors");

        for (const std::int32_t outputSector : result.outputSectors)
        {
            check(outputSector >= 0 && static_cast<std::size_t>(outputSector) < level.sectors.size(),
                "Level.cleaveSectorByPlanes.perpendicular: output sector in range");
            if (outputSector < 0 || static_cast<std::size_t>(outputSector) >= level.sectors.size())
            {
                continue;
            }

            const sed::Sector& output = level.sectors[static_cast<std::size_t>(outputSector)];
            check(sed::level::sectorIsConvex(output),
                "Level.cleaveSectorByPlanes.perpendicular: output sector convex");
            for (const sed::Surface& surface : output.surfaces)
            {
                check(sed::level::surfaceIsPlanar(output, surface),
                    "Level.cleaveSectorByPlanes.perpendicular: output surface planar");
                check(sed::level::surfaceIsConvex(output, surface),
                    "Level.cleaveSectorByPlanes.perpendicular: output surface convex");
            }
        }
    }

    {
        using sed::level::CleaveStrokeHalfSpace;
        using sed::level::SectorCleaveStroke;
        const std::array positiveXAtOrigin{
            CleaveStrokeHalfSpace{{1, 0, 0}, {0.5, 0, 0}, true},
        };
        const std::array negativeXAtOrigin{
            CleaveStrokeHalfSpace{{1, 0, 0}, {0.5, 0, 0}, false},
        };
        const SectorCleaveStroke endpointForward{
            {0.5, 0.0, 0.0}, {0.75, 0.0, 0.0},
        };
        const SectorCleaveStroke endpointReverse{
            endpointForward.second, endpointForward.first,
        };
        check(sed::level::cleaveStrokeReachesHalfSpaces(endpointForward, positiveXAtOrigin)
                && sed::level::cleaveStrokeReachesHalfSpaces(endpointReverse, positiveXAtOrigin)
                && !sed::level::cleaveStrokeReachesHalfSpaces(endpointForward, negativeXAtOrigin)
                && !sed::level::cleaveStrokeReachesHalfSpaces(endpointReverse, negativeXAtOrigin),
            "Cleave stroke reach excludes endpoint-only sibling contact in either stroke direction");

        const std::array positiveFromOrigin{
            CleaveStrokeHalfSpace{{1, 0, 0}, {0, 0, 0}, true},
        };
        const SectorCleaveStroke shallowPositiveForward{
            {-1.0e6, 0.0, 0.0}, {1.0e-12, 0.0, 0.0},
        };
        const SectorCleaveStroke shallowPositiveReverse{
            shallowPositiveForward.second, shallowPositiveForward.first,
        };
        check(sed::level::cleaveStrokeReachesHalfSpaces(
                    shallowPositiveForward, positiveFromOrigin)
                && sed::level::cleaveStrokeReachesHalfSpaces(
                    shallowPositiveReverse, positiveFromOrigin),
            "Cleave stroke reach keeps a strictly positive crossing interval in either endpoint order");

        const std::array tinyPositiveOverlap{
            CleaveStrokeHalfSpace{{1, 0, 0}, {0, 0, 0}, true},
            CleaveStrokeHalfSpace{{1, 0, 0}, {1.0e-12, 0, 0}, false},
        };
        const std::array disjointOpposingHalfSpaces{
            CleaveStrokeHalfSpace{{1, 0, 0}, {0, 0, 0}, true},
            CleaveStrokeHalfSpace{{1, 0, 0}, {-1.0e-12, 0, 0}, false},
        };
        const std::array touchingOpposingHalfSpaces{
            CleaveStrokeHalfSpace{{1, 0, 0}, {0, 0, 0}, true},
            CleaveStrokeHalfSpace{{1, 0, 0}, {0, 0, 0}, false},
        };
        const SectorCleaveStroke nearEndCrossing{
            {-1.0e6, 0.0, 0.0}, {1.0, 0.0, 0.0},
        };
        const SectorCleaveStroke nearEndCrossingReverse{
            nearEndCrossing.second, nearEndCrossing.first,
        };
        check(sed::level::cleaveStrokeReachesHalfSpaces(
                    nearEndCrossing, tinyPositiveOverlap)
                && sed::level::cleaveStrokeReachesHalfSpaces(
                    nearEndCrossingReverse, tinyPositiveOverlap),
            "Cleave stroke reach preserves a tiny positive overlap near t=1 in either endpoint order");
        check(!sed::level::cleaveStrokeReachesHalfSpaces(
                    nearEndCrossing, disjointOpposingHalfSpaces)
                && !sed::level::cleaveStrokeReachesHalfSpaces(
                    nearEndCrossingReverse, disjointOpposingHalfSpaces),
            "Cleave stroke reach rejects disjoint opposing intervals in either endpoint order");
        check(!sed::level::cleaveStrokeReachesHalfSpaces(
                    nearEndCrossing, touchingOpposingHalfSpaces)
                && !sed::level::cleaveStrokeReachesHalfSpaces(
                    nearEndCrossingReverse, touchingOpposingHalfSpaces),
            "Cleave stroke reach rejects touching opposing intervals in either endpoint order");

        const double largeOffset = 1.0e6;
        const std::array translatedBoundary{
            CleaveStrokeHalfSpace{{1, 0, 0}, {largeOffset, 0, 0}, true},
        };
        const std::array translatedBoundaryOpposite{
            CleaveStrokeHalfSpace{{1, 0, 0}, {largeOffset, 0, 0}, false},
        };
        const SectorCleaveStroke shortTranslatedBoundary{
            {largeOffset, 0.0, 0.0}, {largeOffset, 0.0, 1.0e-8},
        };
        const SectorCleaveStroke shortTranslatedInside{
            {largeOffset + 0.25, 0.0, 0.0}, {largeOffset + 0.25, 1.0e-8, 0.0},
        };
        check(sed::level::cleaveStrokeReachesHalfSpaces(
                    shortTranslatedBoundary, translatedBoundary)
                && sed::level::cleaveStrokeReachesHalfSpaces(
                    shortTranslatedBoundary, translatedBoundaryOpposite)
                && sed::level::cleaveStrokeReachesHalfSpaces(
                    shortTranslatedInside, translatedBoundary),
            "Cleave stroke reach retains short positive-length boundary and inside strokes after translation");

        constexpr double distantPlane = 1.0e12;
        const std::array distantNegative{
            CleaveStrokeHalfSpace{{1, 0, 0}, {distantPlane, 0, 0}, false},
        };
        const std::array distantPositive{
            CleaveStrokeHalfSpace{{1, 0, 0}, {distantPlane, 0, 0}, true},
        };
        const SectorCleaveStroke distantOutside{
            {distantPlane + 0.015625, 0.0, 0.0},
            {distantPlane + 0.015625, 1.0, 0.0},
        };
        check(!sed::level::cleaveStrokeReachesHalfSpaces(distantOutside, distantNegative)
                && sed::level::cleaveStrokeReachesHalfSpaces(distantOutside, distantPositive),
            "Cleave stroke reach does not round a real translated half-space gap onto the boundary");

        const double maximumCoordinate = std::numeric_limits<double>::max();
        const double previousMaximum = std::nextafter(maximumCoordinate, 0.0);
        const std::array extremeNegative{
            CleaveStrokeHalfSpace{{1, 1, 1}, {0, 0, 0}, false},
        };
        const std::array extremePositive{
            CleaveStrokeHalfSpace{{1, 1, 1}, {0, 0, 0}, true},
        };
        const SectorCleaveStroke extremeFiniteStroke{
            {maximumCoordinate, -maximumCoordinate, maximumCoordinate},
            {maximumCoordinate, -maximumCoordinate, previousMaximum},
        };
        check(!sed::level::cleaveStrokeReachesHalfSpaces(extremeFiniteStroke, extremeNegative)
                && sed::level::cleaveStrokeReachesHalfSpaces(extremeFiniteStroke, extremePositive),
            "Cleave stroke reach keeps a finite diagonal half-space distance when the absolute dot terms exceed double range");

        const std::array narrowInterval{
            CleaveStrokeHalfSpace{{1, 0, 0}, {0.25, 0, 0}, true},
            CleaveStrokeHalfSpace{{1, 0, 0}, {0.75, 0, 0}, false},
        };
        const SectorCleaveStroke crossingInterval{
            {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},
        };
        const SectorCleaveStroke touchingOnly{
            {0.0, 0.0, 0.0}, {0.25, 0.0, 0.0},
        };
        check(sed::level::cleaveStrokeReachesHalfSpaces(crossingInterval, narrowInterval)
                && !sed::level::cleaveStrokeReachesHalfSpaces(touchingOnly, narrowInterval),
            "Cleave stroke reach intersects multiple ancestry intervals and rejects zero-length clipped contact");
    }

    {
        const auto boundedPieceCount = [](
            const auto& planes,
            const auto& strokes,
            const bool bounded)
        {
            sed::Level level{};
            const std::int32_t sectorIndex = sed::level::levelCreateCube(
                level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right).num;
            sed::level::SectorCleaveBatchOptions options{};
            options.strokes = std::span<const sed::level::SectorCleaveStroke>(strokes);
            options.bounded = bounded;
            const auto result = sed::level::levelCleaveSectorByPlanes(
                level, sectorIndex, planes, options);
            return result.succeeded()
                ? static_cast<int>(result.outputSectors.size())
                : -1;
        };

        const std::array firstPlane{
            sed::level::SectorCleavePlane{{1, 0, 0}, {0.5, 0, 0}},
            sed::level::SectorCleavePlane{{0, 1, 0}, {0, 0.5, 0}},
        };
        const std::array oneSideStrokes{
            sed::level::SectorCleaveStroke{{0.5, 0, 0.5}, {0.5, 1, 0.5}},
            sed::level::SectorCleaveStroke{{0.75, 0.5, 0}, {0.75, 0.5, 1}},
        };
        const std::array crossingStrokes{
            sed::level::SectorCleaveStroke{{0.5, 0, 0.5}, {0.5, 1, 0.5}},
            sed::level::SectorCleaveStroke{{0.25, 0.5, 0.5}, {0.75, 0.5, 0.5}},
        };
        check(boundedPieceCount(firstPlane, oneSideStrokes, true) == 3,
            "Level.cleaveSectorByPlanes.bounded: a later stroke wholly on one prior side only splits that descendant");
        check(boundedPieceCount(firstPlane, crossingStrokes, true) == 4,
            "Level.cleaveSectorByPlanes.bounded: a later stroke crossing the earlier split reaches both descendants");
        check(boundedPieceCount(firstPlane, oneSideStrokes, false) == 4,
            "Level.cleaveSectorByPlanes.unbounded: disabled bounded mode retains global all-descendant cuts");

        const std::array tangentStrokes{
            oneSideStrokes[0],
            sed::level::SectorCleaveStroke{{0.5, 0.5, 0.5}, {0.75, 0.5, 0.5}},
        };
        check(boundedPieceCount(firstPlane, tangentStrokes, true) == 3,
            "Level.cleaveSectorByPlanes.bounded: endpoint-only contact does not admit the opposite descendant");

        const std::array boundaryStrokes{
            oneSideStrokes[0],
            sed::level::SectorCleaveStroke{{0.5, 0.5, 0}, {0.5, 0.5, 1}},
        };
        check(boundedPieceCount(firstPlane, boundaryStrokes, true) == 4,
            "Level.cleaveSectorByPlanes.bounded: a positive-length stroke on the shared boundary reaches both sides");

        const std::array outsideSourceStrokes{
            oneSideStrokes[0],
            sed::level::SectorCleaveStroke{{1.25, 0.5, 0}, {1.25, 0.5, 1}},
        };
        check(boundedPieceCount(firstPlane, outsideSourceStrokes, true) == 3,
            "Level.cleaveSectorByPlanes.bounded: reached lineage is not clipped to the original source extent");

        const std::array reversedPlanes{
            sed::level::SectorCleavePlane{{-1, 0, 0}, {0.5, 0, 0}},
            sed::level::SectorCleavePlane{{0, -1, 0}, {0, 0.5, 0}},
        };
        const std::array reversedStrokes{
            oneSideStrokes[0],
            sed::level::SectorCleaveStroke{{0.25, 0.5, 0}, {0.25, 0.5, 1}},
        };
        check(boundedPieceCount(reversedPlanes, reversedStrokes, true) == 3,
            "Level.cleaveSectorByPlanes.bounded: reversed plane normals preserve geometric descendant reach");

        const std::array threePlanes{
            sed::level::SectorCleavePlane{{1, 0, 0}, {0.5, 0, 0}},
            sed::level::SectorCleavePlane{{0, 1, 0}, {0, 0.5, 0}},
            sed::level::SectorCleavePlane{{0, 0, 1}, {0, 0, 0.5}},
        };
        const std::array threeStrokes{
            oneSideStrokes[0],
            oneSideStrokes[1],
            sed::level::SectorCleaveStroke{{0.6, 0.25, 0.5}, {1, 0.25, 0.5}},
        };
        check(boundedPieceCount(threePlanes, threeStrokes, true) == 4,
            "Level.cleaveSectorByPlanes.bounded: later strokes follow only the successful split ancestry they reach");

        sed::Level rollback{};
        const std::int32_t rollbackSector = sed::level::levelCreateCube(
            rollback, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right).num;
        const auto beforeBytes = sed::assets::saveToSedBytes(rollback);
        const std::array invalidTailPlanes{
            firstPlane[0],
            sed::level::SectorCleavePlane{{0, 0, 0}, {0, 2, 0}},
        };
        const std::array invalidTailStrokes{
            oneSideStrokes[0],
            sed::level::SectorCleaveStroke{{0.75, 2, 0}, {0.75, 2, 1}},
        };
        sed::level::SectorCleaveBatchOptions boundedOptions{};
        boundedOptions.strokes = invalidTailStrokes;
        boundedOptions.bounded = true;
        const auto failed = sed::level::levelCleaveSectorByPlanes(
            rollback, rollbackSector, invalidTailPlanes, boundedOptions);
        check(!failed.succeeded()
                && failed.error == sed::level::SectorCleaveBatchError::InvalidPlane
                && failed.failedPlane == 1
                && sed::assets::saveToSedBytes(rollback) == beforeBytes,
            "Level.cleaveSectorByPlanes.bounded: an invalid tail rolls back the complete batch");

        const auto malformedStrokesRejected = [](
            const std::span<const sed::level::SectorCleaveStroke> strokes,
            const std::int32_t expectedFailedPlane)
        {
            sed::Level level{};
            const std::int32_t sectorIndex = sed::level::levelCreateCube(
                level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right).num;
            const auto before = sed::assets::saveToSedBytes(level);
            const std::array plane{
                sed::level::SectorCleavePlane{{1, 0, 0}, {0.5, 0, 0}},
            };
            sed::level::SectorCleaveBatchOptions options{};
            options.bounded = true;
            options.strokes = strokes;
            const auto result = sed::level::levelCleaveSectorByPlanes(
                level, sectorIndex, plane, options);
            return !result.succeeded()
                && result.error == sed::level::SectorCleaveBatchError::InvalidPlane
                && result.failedPlane == expectedFailedPlane
                && sed::assets::saveToSedBytes(level) == before;
        };
        const std::array zeroLengthStroke{
            sed::level::SectorCleaveStroke{{0.5, 0.0, 0.5}, {0.5, 0.0, 0.5}},
        };
        const std::array nonFiniteStroke{
            sed::level::SectorCleaveStroke{
                {0.5, 0.0, 0.5},
                {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.5},
            },
        };
        check(malformedStrokesRejected({}, -1)
                && malformedStrokesRejected(zeroLengthStroke, 0)
                && malformedStrokesRejected(nonFiniteStroke, 0),
            "Level.cleaveSectorByPlanes.bounded: missing, zero-length, and non-finite strokes reject atomically");
    }

    {
        sed::Level level{};
        const std::int32_t sectorIndex =
            sed::level::levelCreateCube(level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right).num;

        // Four independent planes form a grid-like arrangement. Without
        // explicit chain topology every plane applies to every descendant.
        const std::array planes{
            sed::level::SectorCleavePlane{ sed::TVector::right, sed::TVector::Create(0.25, 0.0, 0.0) },
            sed::level::SectorCleavePlane{ sed::TVector::right, sed::TVector::Create(0.75, 0.0, 0.0) },
            sed::level::SectorCleavePlane{ sed::TVector::forward, sed::TVector::Create(0.0, 0.25, 0.0) },
            sed::level::SectorCleavePlane{ sed::TVector::forward, sed::TVector::Create(0.0, 0.75, 0.0) },
        };

        const sed::level::SectorCleaveBatchResult result =
            sed::level::levelCleaveSectorByPlanes(level, sectorIndex, planes);
        check(result.succeeded(), "Level.cleaveSectorByPlanes.closedShape: succeeds");
        check(result.outputSectors.size() == 9,
            "Level.cleaveSectorByPlanes.closedShape: nine convex cells");
        check(level.sectors.size() == 9,
            "Level.cleaveSectorByPlanes.closedShape: commits nine sectors");
        for (const sed::Sector& output : level.sectors)
        {
            check(sed::level::sectorIsConvex(output),
                "Level.cleaveSectorByPlanes.closedShape: sector convex");
            for (const sed::Surface& surface : output.surfaces)
            {
                check(sed::level::surfaceIsPlanar(output, surface),
                    "Level.cleaveSectorByPlanes.closedShape: surface planar");
                check(sed::level::surfaceIsConvex(output, surface),
            "Level.cleaveSectorByPlanes.closedShape: surface convex");
            }
        }
    }

    {
        sed::Level level{};
        const std::int32_t sectorIndex =
            sed::level::levelCreateCube(level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right).num;

        const std::array planes{
            sed::level::SectorCleavePlane{ sed::TVector::right, sed::TVector::Create(0.25, 0.0, 0.0) },
            sed::level::SectorCleavePlane{ -sed::TVector::right, sed::TVector::Create(0.75, 0.0, 0.0) },
            sed::level::SectorCleavePlane{ sed::TVector::forward, sed::TVector::Create(0.0, 0.25, 0.0) },
            sed::level::SectorCleavePlane{ -sed::TVector::forward, sed::TVector::Create(0.0, 0.75, 0.0) },
        };
        sed::level::SectorCleaveBatchOptions options{
            true,
            sed::TVector::Create(0.5, 0.5, 0.5),
        };
        const std::array strokes{
            sed::level::SectorCleaveStroke{{0.25, 0.25, 0.5}, {0.25, 0.75, 0.5}},
            sed::level::SectorCleaveStroke{{0.75, 0.25, 0.5}, {0.75, 0.75, 0.5}},
            sed::level::SectorCleaveStroke{{0.25, 0.25, 0.5}, {0.75, 0.25, 0.5}},
            sed::level::SectorCleaveStroke{{0.25, 0.75, 0.5}, {0.75, 0.75, 0.5}},
        };
        options.strokes = strokes;
        options.bounded = true;

        const sed::level::SectorCleaveBatchResult result =
            sed::level::levelCleaveSectorByPlanes(level, sectorIndex, planes, options);
        check(result.succeeded(), "Level.cleaveSectorByPlanes.closedChain: succeeds");
        check(result.outputSectors.size() == 5,
            "Level.cleaveSectorByPlanes.closedChain: clips only the interior descendant");
        check(level.sectors.size() == 5,
            "Level.cleaveSectorByPlanes.closedChain: avoids a nine-cell global grid");
        for (const sed::Sector& output : level.sectors)
        {
            check(sed::level::sectorIsConvex(output),
                "Level.cleaveSectorByPlanes.closedChain: sector convex");
            for (const sed::Surface& surface : output.surfaces)
            {
                check(sed::level::surfaceIsPlanar(output, surface),
                    "Level.cleaveSectorByPlanes.closedChain: surface planar");
                check(sed::level::surfaceIsConvex(output, surface),
                    "Level.cleaveSectorByPlanes.closedChain: surface convex");
            }
        }
    }

    {
        sed::Level level{};
        const std::int32_t sectorIndex =
            sed::level::levelCreateCube(level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right).num;
        sed::Thing& thing = level.newThing();
        thing.sector = sectorIndex;
        thing.x = 0.2;
        thing.y = 0.2;
        thing.z = 0.2;

        const std::size_t originalSectorCount = level.sectors.size();
        const std::size_t originalVertexCount = level.sectors[0].vertices.size();
        const std::size_t originalSurfaceCount = level.sectors[0].surfaces.size();
        const std::int32_t originalThingSector = level.things[0].sector;
        const sed::Vertex originalVertex = level.sectors[0].vertices[0];

        const std::array planes{
            sed::level::SectorCleavePlane{ sed::TVector::right, sed::TVector::Create(0.5, 0.0, 0.0) },
            sed::level::SectorCleavePlane{ sed::TVector{}, sed::TVector::Create(2.0, 0.0, 0.0) },
        };

        const sed::level::SectorCleaveBatchResult result =
            sed::level::levelCleaveSectorByPlanes(level, sectorIndex, planes);
        check(!result.succeeded(), "Level.cleaveSectorByPlanes.rollback: fails");
        check(result.error == sed::level::SectorCleaveBatchError::InvalidPlane,
            "Level.cleaveSectorByPlanes.rollback: reports invalid plane");
        check(result.failedPlane == 1,
            "Level.cleaveSectorByPlanes.rollback: reports failing plane");
        check(result.outputSectors.empty(),
            "Level.cleaveSectorByPlanes.rollback: exposes no uncommitted sectors");
        check(level.sectors.size() == originalSectorCount,
            "Level.cleaveSectorByPlanes.rollback: sector count unchanged");
        check(level.sectors[0].vertices.size() == originalVertexCount,
            "Level.cleaveSectorByPlanes.rollback: vertex count unchanged");
        check(level.sectors[0].surfaces.size() == originalSurfaceCount,
            "Level.cleaveSectorByPlanes.rollback: surface count unchanged");
        check(level.things[0].sector == originalThingSector,
            "Level.cleaveSectorByPlanes.rollback: Thing sector unchanged");
        check(close(level.sectors[0].vertices[0].x, originalVertex.x)
                && close(level.sectors[0].vertices[0].y, originalVertex.y)
                && close(level.sectors[0].vertices[0].z, originalVertex.z),
            "Level.cleaveSectorByPlanes.rollback: geometry unchanged");
    }

    {
        sed::Level level{};
        const std::int32_t sectorIndex =
            sed::level::levelCreateCube(level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right).num;
        const std::array planes{
            sed::level::SectorCleavePlane{
                sed::TVector::Create(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0),
                sed::TVector{} },
        };

        const sed::level::SectorCleaveBatchResult result =
            sed::level::levelCleaveSectorByPlanes(level, sectorIndex, planes);
        check(!result.succeeded(), "Level.cleaveSectorByPlanes.invalidPlane: fails");
        check(result.error == sed::level::SectorCleaveBatchError::InvalidPlane,
            "Level.cleaveSectorByPlanes.invalidPlane: reports invalid plane");
        check(result.failedPlane == 0,
            "Level.cleaveSectorByPlanes.invalidPlane: reports plane index");
        check(level.sectors.size() == 1,
            "Level.cleaveSectorByPlanes.invalidPlane: Level unchanged");
    }

    {
        sed::Level level{};
        sed::Sector& sector =
            sed::level::levelCreateCube(level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right);
        sector.vertices[0].z += 0.1;
        const std::size_t originalSectorCount = level.sectors.size();
        const std::array planes{
            sed::level::SectorCleavePlane{ sed::TVector::right, sed::TVector::Create(0.5, 0.0, 0.0) },
        };

        const sed::level::SectorCleaveBatchResult result =
            sed::level::levelCleaveSectorByPlanes(level, sector.num, planes);
        check(!result.succeeded(), "Level.cleaveSectorByPlanes.invalidInput: fails");
        check(result.error == sed::level::SectorCleaveBatchError::InvalidInputGeometry,
            "Level.cleaveSectorByPlanes.invalidInput: reports invalid geometry");
        check(result.failedSector == sector.num,
            "Level.cleaveSectorByPlanes.invalidInput: reports sector");
        check(level.sectors.size() == originalSectorCount,
            "Level.cleaveSectorByPlanes.invalidInput: Level unchanged");
    }

    {
        sed::Level level{};
        (void)level.newSector();
        (void)level.newSector();
        (void)level.newSector();

        sed::Sector& sec0 = level.sectors[0];
        sed::Sector& sec2 = level.sectors[2];

        const std::int32_t a0 = sec0.newVertex(0.0, 0.0, 0.0);
        const std::int32_t a1 = sec0.newVertex(2.0, 0.0, 0.0);
        const std::int32_t a2 = sec0.newVertex(2.0, 2.0, 0.0);
        const std::int32_t a3 = sec0.newVertex(0.0, 2.0, 0.0);

        sed::Surface s0{};
        s0.vertices    = { a0, a1, a2, a3 };
        s0.texVertices = std::vector<sed::TexVertex>(s0.vertices.size());
        (void)sec0.addSurface(std::move(s0));

        const std::int32_t b0 = sec2.newVertex(0.0, 0.0, 0.0);
        const std::int32_t b1 = sec2.newVertex(2.0, 0.0, 0.0);
        const std::int32_t b2 = sec2.newVertex(2.0, 2.0, 0.0);
        const std::int32_t b3 = sec2.newVertex(0.0, 2.0, 0.0);

        sed::Surface s2{};
        s2.vertices    = { b0, b3, b2, b1 };
        s2.texVertices = std::vector<sed::TexVertex>(s2.vertices.size());
        (void)sec2.addSurface(std::move(s2));

        const double ppu = sed::level::levelPixelPerUnit(level);
        sed::level::surfaceRecalcNewAll(ppu, sec0, sec0.surfaces[0], 0);
        sed::level::surfaceRecalcNewAll(ppu, sec2, sec2.surfaces[0], 0);

        (void)sed::level::levelSysAdjoinSurfaces(level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 2, 0 }, true);

        check(level.sectors.size() == 3, "Level.deleteSector: setup count");

        const bool deleted = sed::level::levelDeleteSector(level, 1);
        check(deleted, "Level.deleteSector: ok");
        check(level.sectors.size() == 2, "Level.deleteSector: count");

        const sed::Sector& after0 = level.sectors[0];
        const sed::Sector& after1 = level.sectors[1];

        check(after0.surfaces.size() == 1, "Level.deleteSector: after0 surface count");
        check(after1.surfaces.size() == 1, "Level.deleteSector: after1 surface count");

        if (!after0.surfaces.empty() && !after1.surfaces.empty())
        {
            const sed::Surface& a = after0.surfaces[0];
            const sed::Surface& b = after1.surfaces[0];

            check(a.adjoin.has_value(), "Level.deleteSector: after0 adjoin");
            check(b.adjoin.has_value(), "Level.deleteSector: after1 adjoin");

            if (a.adjoin)
            {
                check(a.adjoin->sector == 1, "Level.deleteSector: after0 adjoin sector");
                check(a.adjoin->surface == 0, "Level.deleteSector: after0 adjoin surface");
            }

            if (b.adjoin)
            {
                check(b.adjoin->sector == 0, "Level.deleteSector: after1 adjoin sector");
                check(b.adjoin->surface == 0, "Level.deleteSector: after1 adjoin surface");
            }
        }
    }

    {
        sed::Level level{};
        (void)level.newSector();

        const bool deletedSector = sed::level::levelDeleteSector(level, 7);
        check(!deletedSector, "Level.deleteSector.invalid: fails");
        check(level.sectors.size() == 1, "Level.deleteSector.invalid: sector count unchanged");

        const bool deletedSurface = sed::level::levelDeleteSurface(level, sed::SurfaceRef{ 0, 3 });
        check(!deletedSurface, "Level.deleteSurface.invalid: fails");
        check(level.sectors.size() == 1, "Level.deleteSurface.invalid: sector count unchanged");
    }

    {
        sed::Level level{};
        (void)level.newSector();
        (void)level.newSector();

        sed::Sector& sec0 = level.sectors[0];
        sed::Sector& sec1 = level.sectors[1];

        const std::int32_t a0 = sec0.newVertex(0.0, 0.0, 0.0);
        const std::int32_t a1 = sec0.newVertex(2.0, 0.0, 0.0);
        const std::int32_t a2 = sec0.newVertex(2.0, 2.0, 0.0);
        const std::int32_t a3 = sec0.newVertex(0.0, 2.0, 0.0);

        const std::int32_t a4 = sec0.newVertex(0.0, 0.0, 1.0);
        const std::int32_t a5 = sec0.newVertex(2.0, 0.0, 1.0);
        const std::int32_t a6 = sec0.newVertex(2.0, 2.0, 1.0);
        const std::int32_t a7 = sec0.newVertex(0.0, 2.0, 1.0);

        sed::Surface s0{};
        s0.vertices    = { a0, a1, a2, a3 };
        s0.texVertices = std::vector<sed::TexVertex>(s0.vertices.size());

        sed::Surface s1{};
        s1.vertices    = { a4, a5, a6, a7 };
        s1.texVertices = std::vector<sed::TexVertex>(s1.vertices.size());

        (void)sec0.addSurface(std::move(s0));
        (void)sec0.addSurface(std::move(s1));

        const std::int32_t b0 = sec1.newVertex(0.0, 0.0, 1.0);
        const std::int32_t b1 = sec1.newVertex(2.0, 0.0, 1.0);
        const std::int32_t b2 = sec1.newVertex(2.0, 2.0, 1.0);
        const std::int32_t b3 = sec1.newVertex(0.0, 2.0, 1.0);

        sed::Surface t0{};
        t0.vertices    = { b0, b3, b2, b1 };
        t0.texVertices = std::vector<sed::TexVertex>(t0.vertices.size());
        (void)sec1.addSurface(std::move(t0));

        const double ppu = sed::level::levelPixelPerUnit(level);
        sed::level::surfaceRecalcNewAll(ppu, sec0, sec0.surfaces[1], 0);
        sed::level::surfaceRecalcNewAll(ppu, sec1, sec1.surfaces[0], 0);

        (void)sed::level::levelSysAdjoinSurfaces(level, sed::SurfaceRef{ 0, 1 }, sed::SurfaceRef{ 1, 0 }, true);

        const bool deleted = sed::level::levelDeleteSurface(level, sed::SurfaceRef{ 0, 0 });
        check(deleted, "Level.deleteSurface: ok");

        check(sec0.surfaces.size() == 1, "Level.deleteSurface: sec0 surface count");
        check(sec1.surfaces.size() == 1, "Level.deleteSurface: sec1 surface count");

        if (!sec0.surfaces.empty() && !sec1.surfaces.empty())
        {
            const sed::Surface& a = sec0.surfaces[0];
            const sed::Surface& b = sec1.surfaces[0];

            check(a.adjoin.has_value(), "Level.deleteSurface: a adjoin");
            check(b.adjoin.has_value(), "Level.deleteSurface: b adjoin");

            if (b.adjoin)
            {
                check(b.adjoin->sector == 0, "Level.deleteSurface: b adjoin sector");
                check(b.adjoin->surface == 0, "Level.deleteSurface: b adjoin surface");
            }
        }
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t a0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t a1 = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t a2 = sector.newVertex(2.0, 2.0, 0.0);
        const std::int32_t a3 = sector.newVertex(0.0, 2.0, 0.0);
        const std::int32_t b0 = sector.newVertex(0.0, 0.0, 1.0);
        const std::int32_t b1 = sector.newVertex(2.0, 0.0, 1.0);
        const std::int32_t b2 = sector.newVertex(2.0, 2.0, 1.0);
        const std::int32_t b3 = sector.newVertex(0.0, 2.0, 1.0);

        sed::Surface s0{};
        s0.vertices    = { a0, a1, a2, a3 };
        s0.texVertices = std::vector<sed::TexVertex>(s0.vertices.size());
        (void)sector.addSurface(std::move(s0));

        sed::Surface s1{};
        s1.vertices    = { b0, b1, b2, b3 };
        s1.texVertices = std::vector<sed::TexVertex>(s1.vertices.size());
        (void)sector.addSurface(std::move(s1));

        const bool connected = sed::level::levelConnectSurfaces(level, sed::SurfaceRef{ sector.num, 0 }, sed::SurfaceRef{ sector.num, 1 });
        check(!connected, "Level.connectSurfaces.sameSector: fails");
        check(!sector.surfaces[0].adjoin && !sector.surfaces[1].adjoin, "Level.connectSurfaces.sameSector: no adjoins");
    }

    {
        sed::Level level{};
        (void)level.newSector();
        (void)level.newSector();
        sed::Sector& firstSector = level.sectors[0];
        sed::Sector& secondSector = level.sectors[1];

        firstSector.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
        };
        secondSector.vertices = {
            { 3.0, 0.0, 0.0, 0 },
            { 3.0, 2.0, 0.0, 0 },
            { 5.0, 2.0, 0.0, 0 },
            { 5.0, 0.0, 0.0, 0 },
        };

        sed::Surface firstSurface{};
        firstSurface.vertices = { 0, 1, 2, 3 };
        firstSurface.texVertices = {
            { -2.0f, 3.0f, sed::ColorRed },
            { 5.0f, -7.0f, sed::ColorGreen },
            { 11.0f, 13.0f, sed::ColorBlue },
            { -17.0f, 19.0f, sed::ColorWhite },
        };
        firstSurface.normal = {};
        firstSector.surfaces.push_back(firstSurface);

        sed::Surface secondSurface{};
        secondSurface.vertices = { 0, 1, 2, 3 };
        secondSurface.texVertices = {
            { 23.0f, -29.0f, sed::ColorRed },
            { -31.0f, 37.0f, sed::ColorGreen },
            { 41.0f, -43.0f, sed::ColorBlue },
            { -47.0f, 53.0f, sed::ColorWhite },
        };
        secondSurface.normal = {};
        secondSector.surfaces.push_back(secondSurface);

        const sed::Level baseline = level;
        const bool connected = sed::level::levelConnectSurfaces(
            level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 1, 0 });
        check(!connected
                && sameVertexVectors(
                    level.sectors[0].vertices,
                    baseline.sectors[0].vertices)
                && sameVertexVectors(
                    level.sectors[1].vertices,
                    baseline.sectors[1].vertices)
                && level.sectors[0].surfaces.size()
                    == baseline.sectors[0].surfaces.size()
                && level.sectors[1].surfaces.size()
                    == baseline.sectors[1].surfaces.size()
                && sameTextureVertexVectors(
                    level.sectors[0].surfaces[0].texVertices,
                    baseline.sectors[0].surfaces[0].texVertices)
                && sameTextureVertexVectors(
                    level.sectors[1].surfaces[0].texVertices,
                    baseline.sectors[1].surfaces[0].texVertices)
                && level.sectors[0].surfaces[0].normal
                    == baseline.sectors[0].surfaces[0].normal
                && level.sectors[1].surfaces[0].normal
                    == baseline.sectors[1].surfaces[0].normal,
            "Level.connectSurfaces.disjoint: a rejected public connect is failure-atomic, including cached normals and authored UVs");
    }

    {
        sed::Level level{};
        (void)level.newSector();
        (void)level.newSector();

        sed::Sector& sec0 = level.sectors[0];
        sed::Sector& sec1 = level.sectors[1];

        const std::int32_t a0 = sec0.newVertex(0.0, 0.0, 0.0);
        const std::int32_t a1 = sec0.newVertex(2.0, 0.0, 0.0);
        const std::int32_t a2 = sec0.newVertex(2.0, 2.0, 0.0);
        const std::int32_t a3 = sec0.newVertex(0.0, 2.0, 0.0);

        sed::Surface s0{};
        s0.vertices    = { a0, a1, a2, a3 };
        s0.texVertices = std::vector<sed::TexVertex>(s0.vertices.size());
        (void)sec0.addSurface(std::move(s0));

        const std::int32_t b0 = sec1.newVertex(0.0, 0.0, 1.0);
        const std::int32_t b1 = sec1.newVertex(2.0, 0.0, 1.0);
        const std::int32_t b2 = sec1.newVertex(2.0, 2.0, 1.0);
        const std::int32_t b3 = sec1.newVertex(0.0, 2.0, 1.0);

        sed::Surface s1{};
        s1.vertices    = { b0, b1, b2, b3 };
        s1.texVertices = std::vector<sed::TexVertex>(s1.vertices.size());
        (void)sec1.addSurface(std::move(s1));

        const double ppu = sed::level::levelPixelPerUnit(level);
        sed::level::surfaceRecalcNewAll(ppu, sec0, sec0.surfaces[0], 0);
        sed::level::surfaceRecalcNewAll(ppu, sec1, sec1.surfaces[0], 0);

        const bool connected = sed::level::levelConnectSurfaces(level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 1, 0 });
        check(!connected, "Level.connectSurfaces.parallelNormals: fails");
        check(!sec0.surfaces[0].adjoin && !sec1.surfaces[0].adjoin, "Level.connectSurfaces.parallelNormals: no adjoins");
    }

    {
        sed::Level level{};
        level.sectors.push_back(makeTestCubeSector(0.0));
        level.sectors.push_back(makeTestCubeSector(4.0));

        for ( std::size_t sectorIndex = 0; sectorIndex < level.sectors.size(); ++sectorIndex )
        {
            sed::Sector& sector = level.sectors[sectorIndex];
            sector.num = static_cast<std::int32_t>(sectorIndex);
            for ( std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex )
            {
                sector.surfaces[surfaceIndex].num = static_cast<std::int32_t>(surfaceIndex);
            }
        }

        const std::size_t sectorCount = level.sectors.size();
        const std::size_t firstVertexCount = level.sectors[0].vertices.size();
        const std::size_t secondVertexCount = level.sectors[1].vertices.size();

        check(!sed::level::levelConnectSectors(level, -1, 1)
                && !sed::level::levelConnectSectors(level, 0, 2)
                && !sed::level::levelConnectSectors(level, 0, 0),
            "Level.connectSectors: invalid and identical indices fail");
        check(!sed::level::levelConnectSectors(level, 0, 1),
            "Level.connectSectors: disjoint sectors fail");
        check(level.sectors.size() == sectorCount
                && level.sectors[0].vertices.size() == firstVertexCount
                && level.sectors[1].vertices.size() == secondVertexCount
                && countAdjoinedSurfaces(level) == 0,
            "Level.connectSectors: rejected requests are failure-atomic");
    }

    {
        sed::Level level{};
        (void)sed::level::levelCreateCube(
            level, 0.0, 0.0, 0.0, sed::TVector::up, sed::TVector::right);
        (void)sed::level::levelCreateCube(
            level, 0.0, 0.0, 1.0, sed::TVector::up, sed::TVector::right);

        const bool connected = sed::level::levelConnectSectors(level, 0, 1);

        check(connected, "Level.connectSectors: adjacent sectors connect through their shared portal");
        check(level.sectors.size() == 2,
            "Level.connectSectors: adjacent sector identities remain present");
        check(countAdjoinedSurfaces(level) == 2,
            "Level.connectSectors: shared portal creates exactly one adjoin pair");
        check(allAdjoinsAreReciprocal(level),
            "Level.connectSectors: every generated adjoin is reciprocal and in range");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::JKDF2;

        const std::int32_t knifeIndex = sed::level::levelCreateCube(
                                           level,
                                           3.0,
                                           1.0,
                                           1.0,
                                           sed::TVector::up,
                                           sed::TVector::Create(2.0, 0.0, 0.0))
                                           .num;
        const std::int32_t targetIndex = sed::level::levelCreateCube(
                                            level,
                                            0.0,
                                            0.0,
                                            0.0,
                                            sed::TVector::up,
                                            sed::TVector::Create(4.0, 0.0, 0.0))
                                            .num;

        const bool joined = sed::level::levelConnectSectorsKnife(level, knifeIndex, targetIndex);
        check(joined, "Level.connectSectorsKnife.partialOverlap: returns true");

        bool foundKnifeAdjoin = false;
        bool foundTargetDerivedPartner = false;

        if (joined && static_cast<std::size_t>(knifeIndex) < level.sectors.size())
        {
            const sed::Sector& knifeOut = level.sectors[static_cast<std::size_t>(knifeIndex)];
            for (const sed::Surface& surfaceA : knifeOut.surfaces)
            {
                if (!surfaceA.adjoin)
                {
                    continue;
                }

                const std::size_t otherSectorIndex = static_cast<std::size_t>(surfaceA.adjoin->sector);
                const std::size_t otherSurfaceIndex = static_cast<std::size_t>(surfaceA.adjoin->surface);
                if (otherSectorIndex >= level.sectors.size())
                {
                    continue;
                }

                const sed::Sector& otherSector = level.sectors[otherSectorIndex];
                if (otherSurfaceIndex >= otherSector.surfaces.size())
                {
                    continue;
                }

                const sed::Surface& otherSurface = otherSector.surfaces[otherSurfaceIndex];
                check(
                    sed::level::surfaceDoOverlap(knifeOut, surfaceA, otherSector, otherSurface),
                    "Level.connectSectorsKnife.partialOverlap: adjoined surfaces overlap");
                foundKnifeAdjoin = true;

                if (maxCoordX(otherSector) <= 4.0001)
                {
                    foundTargetDerivedPartner = true;
                }
            }
        }

        check(foundKnifeAdjoin, "Level.connectSectorsKnife.partialOverlap: knife has adjoin");
        check(
            foundTargetDerivedPartner,
            "Level.connectSectorsKnife.partialOverlap: finds target-derived partner inside original target bounds");
    }

    {
        auto makePartialOverlapLevel = []()
        {
            constexpr std::int32_t kKnifeMaterial = 101;
            constexpr std::int32_t kTargetMaterial = 202;
            sed::Level level{};
            level.kind = sed::ProjectType::JKDF2;

            sed::Sector& knifeSector = sed::level::levelCreateCube(
                level,
                3.0,
                1.0,
                1.0,
                sed::TVector::up,
                sed::TVector::Create(2.0, 0.0, 0.0));
            setSectorSurfaceMaterial(knifeSector, kKnifeMaterial);

            sed::Sector& targetSector = sed::level::levelCreateCube(
                level,
                0.0,
                0.0,
                0.0,
                sed::TVector::up,
                sed::TVector::Create(4.0, 0.0, 0.0));
            setSectorSurfaceMaterial(targetSector, kTargetMaterial);

            return level;
        };

        sed::Level keepPiecesLevel = makePartialOverlapLevel();
        sed::Level cutOnlyLevel = makePartialOverlapLevel();
        const sed::Sector originalKnife = cutOnlyLevel.sectors[0];
        const sed::Sector originalTarget = cutOnlyLevel.sectors[1];

        const bool keepJoined = sed::level::levelConnectSectorsKnife(
            keepPiecesLevel,
            0,
            1,
            sed::level::SectorConnectKnifeMode::KeepTargetPieces);
        const bool cutJoined = sed::level::levelConnectSectorsKnife(
            cutOnlyLevel,
            0,
            1,
            sed::level::SectorConnectKnifeMode::CutTargetHolesOnly);

        check(keepJoined, "Level.connectSectorsKnife.keepTargetPiecesMode: returns true");
        check(cutJoined, "Level.connectSectorsKnife.cutTargetHolesOnlyMode: returns true");
        check(
            anySectorOverlapsWithMaterial(keepPiecesLevel, originalTarget, 101),
            "Level.connectSectorsKnife.keepTargetPiecesMode: keeps embedded knife sector");
        check(
            !anySectorOverlapsWithMaterial(cutOnlyLevel, originalTarget, 101),
            "Level.connectSectorsKnife.cutTargetHolesOnlyMode: clips overlapping knife sector");
        check(
            countSectorsWithMaterial(cutOnlyLevel, 202) == 1,
            "Level.connectSectorsKnife.cutTargetHolesOnlyMode: does not create target volume pieces");
        check(
            anySectorOverlapsWithMaterial(cutOnlyLevel, originalTarget, 202),
            "Level.connectSectorsKnife.cutTargetHolesOnlyMode: preserves non-overlap target sectors");
        check(
            countAdjoinedSurfaces(cutOnlyLevel) > 0,
            "Level.connectSectorsKnife.cutTargetHolesOnlyMode: keeps portal adjoins");
    }

    {
        constexpr std::int32_t kKnifeMaterial = 301;
        constexpr std::int32_t kTargetMaterial = 302;

        sed::Level level{};
        level.kind = sed::ProjectType::JKDF2;

        sed::Sector& knifeSector = sed::level::levelCreateCube(
            level,
            -1.5,
            1.0,
            0.0,
            sed::TVector::up,
            sed::TVector::Create(5.0, 2.0, 0.0));
        setSectorSurfaceMaterial(knifeSector, kKnifeMaterial);

        sed::Sector& targetSector = sed::level::levelCreateCube(
            level,
            0.0,
            0.0,
            0.0,
            sed::TVector::up,
            sed::TVector::Create(4.0, 0.0, 0.0));
        setSectorSurfaceMaterial(targetSector, kTargetMaterial);

        const sed::Sector originalTarget = level.sectors[1];

        const bool cutJoined = sed::level::levelConnectSectorsKnife(
            level,
            0,
            1,
            sed::level::SectorConnectKnifeMode::CutTargetHolesOnly);

        check(cutJoined, "Level.connectSectorsKnife.cutTargetHolesOnlyMode.rotatedKnife: returns true");
        check(
            !anySectorOverlapsWithMaterial(level, originalTarget, kKnifeMaterial),
            "Level.connectSectorsKnife.cutTargetHolesOnlyMode.rotatedKnife: clips rotated knife overlap");
        check(
            countAdjoinedSurfaces(level) > 0,
            "Level.connectSectorsKnife.cutTargetHolesOnlyMode.rotatedKnife: keeps portal adjoins");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::JKDF2;

        sed::Sector& targetSector = sed::level::levelCreateCube(
            level,
            0.0,
            0.0,
            0.0,
            sed::TVector::up,
            sed::TVector::Create(4.0, 0.0, 0.0));
        setSectorSurfaceMaterial(targetSector, 401);

        sed::Sector& knifeSector = sed::level::levelCreateCube(
            level,
            4.0,
            1.0,
            1.0,
            sed::TVector::up,
            sed::TVector::Create(2.0, 0.0, 0.0));
        setSectorSurfaceMaterial(knifeSector, 402);

        check(
            !sed::level::levelConnectSectorSurfaceCandidates(level, { 1 }, {}),
            "Level.connectSectorSurfaceCandidates.emptyTargets: fails");

        const bool connected = sed::level::levelConnectSectorSurfaceCandidates(level, { 1 }, { 0 });
        check(connected, "Level.connectSectorSurfaceCandidates.partialFace: connects");
        check(
            countAdjoinedSurfaces(level) >= 2,
            "Level.connectSectorSurfaceCandidates.partialFace: creates portal adjoins");
        check(
            level.sectors[0].surfaces.size() > 6,
            "Level.connectSectorSurfaceCandidates.partialFace: splits larger target face");
    }
}

} // namespace sed::tests
