#include "test_common.hpp"

#include "render/transform/geometry_transform_preview_utils.hpp"
#include "render/fast_update/thing_transform_update.hpp"

#include "sed/level.hpp"
#include "sed/level/transform.hpp"

#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

namespace sed::app_tests
{
namespace
{

Surface makeSurface(const std::initializer_list<std::int32_t> vertices)
{
    Surface surface{};
    surface.vertices.assign(vertices);
    surface.texVertices.resize(surface.vertices.size());
    return surface;
}

Level makePortalTransformFixture()
{
    Level level{};
    level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
    level.sectors.resize(2);

    Sector& first = level.sectors[0];
    first.vertices = {
        Vertex{ 0.0, 0.0, 0.0, 0 },
        Vertex{ 1.0, 0.0, 0.0, 1 },
        Vertex{ 1.0, 1.0, 0.0, 2 },
        Vertex{ 0.0, 1.0, 0.0, 3 },
        Vertex{ 1.0, 0.0, 1.0, 4 },
        Vertex{ 0.0, 0.0, 1.0, 5 },
    };
    first.surfaces = {
        makeSurface({ 0, 1, 2, 3 }),
        makeSurface({ 0, 1, 4, 5 }),
    };
    first.surfaces[0].texVertices = {
        TexVertex{ .u = -0.25f, .v = 0.5f, .color = ColorF{ 0.1f, 0.2f, 0.3f } },
        TexVertex{ .u = 1.25f, .v = 0.5f, .color = ColorF{ 0.2f, 0.3f, 0.4f } },
        TexVertex{ .u = 1.25f, .v = 1.5f, .color = ColorF{ 0.3f, 0.4f, 0.5f } },
        TexVertex{ .u = -0.25f, .v = 1.5f, .color = ColorF{ 0.4f, 0.5f, 0.6f } },
    };

    Sector& second = level.sectors[1];
    second.vertices = {
        Vertex{ 0.0, 0.0, 0.0, 0 },
        Vertex{ 0.0, 1.0, 0.0, 1 },
        Vertex{ 1.0, 1.0, 0.0, 2 },
        Vertex{ 1.0, 0.0, 0.0, 3 },
        Vertex{ 0.0, 0.0, -1.0, 4 },
        Vertex{ 1.0, 0.0, -1.0, 5 },
    };
    second.surfaces = {
        makeSurface({ 0, 1, 2, 3 }),
        makeSurface({ 0, 3, 5, 4 }),
    };
    second.surfaces[0].texVertices = {
        TexVertex{ .u = 1.25f, .v = 0.5f, .color = ColorF{ 0.6f, 0.5f, 0.4f } },
        TexVertex{ .u = 1.25f, .v = 1.5f, .color = ColorF{ 0.5f, 0.4f, 0.3f } },
        TexVertex{ .u = -0.25f, .v = 1.5f, .color = ColorF{ 0.4f, 0.3f, 0.2f } },
        TexVertex{ .u = -0.25f, .v = 0.5f, .color = ColorF{ 0.3f, 0.2f, 0.1f } },
    };

    first.surfaces[0].adjoin = SurfaceRef{ 1, 0 };
    second.surfaces[0].adjoin = SurfaceRef{ 0, 0 };
    return level;
}

bool containsSurface(
    const std::span<const SurfaceRef> surfaces,
    const SurfaceRef expected)
{
    return std::ranges::find(surfaces, expected) != surfaces.end();
}

std::size_t cornerCount(
    const std::span<const render::GeometryTransformPreviewCorner> corners,
    const SurfaceRef surface)
{
    return static_cast<std::size_t>(std::ranges::count_if(
        corners,
        [surface](const render::GeometryTransformPreviewCorner& corner)
        {
            return corner.surface == surface;
        }));
}

using VertexAddress = std::pair<std::int32_t, std::int32_t>;

std::vector<VertexAddress> changedVertices(
    const Level& before,
    const Level& after)
{
    std::vector<VertexAddress> changed{};
    for (std::size_t sectorIndex = 0;
         sectorIndex < before.sectors.size();
         ++sectorIndex)
    {
        const Sector& beforeSector = before.sectors[sectorIndex];
        const Sector& afterSector = after.sectors[sectorIndex];
        for (std::size_t vertexIndex = 0;
             vertexIndex < beforeSector.vertices.size();
             ++vertexIndex)
        {
            const Vertex& lhs = beforeSector.vertices[vertexIndex];
            const Vertex& rhs = afterSector.vertices[vertexIndex];
            if (lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z)
            {
                changed.emplace_back(
                    static_cast<std::int32_t>(sectorIndex),
                    static_cast<std::int32_t>(vertexIndex));
            }
        }
    }
    return changed;
}

std::vector<VertexAddress> plannedVertices(
    const render::GeometryTransformPreviewPlan& plan)
{
    std::vector<VertexAddress> planned{};
    planned.reserve(plan.movedVertices.size());
    for (const render::GeometryTransformPreviewVertex& vertex :
         plan.movedVertices)
    {
        planned.emplace_back(vertex.sector, vertex.vertex);
    }
    return planned;
}

using UvTuple = std::array<float, 6>;

std::vector<std::vector<UvTuple>> captureUvs(const Level& level)
{
    std::vector<std::vector<UvTuple>> result{};
    for (const Sector& sector : level.sectors)
    {
        for (const Surface& surface : sector.surfaces)
        {
            std::vector<UvTuple> surfaceUvs{};
            surfaceUvs.reserve(surface.texVertices.size());
            for (const TexVertex& uv : surface.texVertices)
            {
                surfaceUvs.push_back(UvTuple{
                    uv.u, uv.v,
                    uv.color.r, uv.color.g, uv.color.b, uv.color.a });
            }
            result.push_back(std::move(surfaceUvs));
        }
    }
    return result;
}

} // namespace

void runGeometryTransformPreviewUtilityTests()
{
    for (const float translation : {0.0f, 1000000.0f, 100000000.0f})
    {
        render::fast_update::Affine3f transform{};
        transform.m01 = -1.0f; transform.m10 = 1.0f; transform.m22 = 1.0f;
        transform.m03 = translation; transform.m13 = translation; transform.m23 = translation;
        QVector3D position(1, 2, 3);
        QVector3D normal(0.6f, 0.8f, 0.0f);
        QVector3D anchor(2, 3, 4);
        render::fast_update::transformDynamicLightGeometry(position, normal, &anchor, transform);
        check((normal - QVector3D(-0.8f, 0.6f, 0.0f)).length() < 1.0e-6f,
            "Thing fast update rotates lighting normals independently of large translation");
        check(std::abs(normal.length() - 1.0f) < 1.0e-6f,
            "Thing fast update retains a unit dynamic-light normal after distant movement");
    }
    for (const double value : {0.0, 1.0, 1000000.0})
    {
        check(render::fast_update::nearlySameTransform(value, value)
                && !render::fast_update::nearlySameTransform(value, std::nextafter(value, std::numeric_limits<double>::infinity()))
                && !render::fast_update::nearlySameTransform(value, value + 1.0e-10),
            "Thing fast update recognizes each representable authored transform change");
    }
    {
        const Level level = makePortalTransformFixture();
        const std::array selected{ SurfaceRef{ 0, 0 } };
        const render::GeometryTransformPreviewPlan plan =
            render::buildSurfaceTransformPreviewPlan(level, selected);

        check(plan.movedVertices.size() == 8u,
            "geometry transform preview records every source and portal-mate vertex moved by a Surface commit");
        check(plan.wholeSurfaces.size() == 2u
                && containsSurface(plan.wholeSurfaces, SurfaceRef{ 0, 0 })
                && containsSurface(plan.wholeSurfaces, SurfaceRef{ 1, 0 }),
            "geometry transform preview moves selected and reciprocal portal faces as complete surfaces");
        check(plan.partialCorners.size() == 4u
                && cornerCount(plan.partialCorners, SurfaceRef{ 0, 1 }) == 2u
                && cornerCount(plan.partialCorners, SurfaceRef{ 1, 1 }) == 2u,
            "geometry transform preview deforms every neighboring face that shares a moved Surface vertex");

        const level::ConnectedSurfaceUvPlan moveUvs =
            render::buildConnectedSurfaceUvPreviewPlan(level, plan, false);
        const level::ConnectedSurfaceUvPlan scaleUvs =
            render::buildConnectedSurfaceUvPreviewPlan(level, plan, true);
        const level::ConnectedSurfaceUvPlan mixedUvs =
            render::buildConnectedSurfaceUvTransformPlan(
                level,
                std::array<std::int32_t, 1>{ 0 },
                std::array<SurfaceRef, 1>{ SurfaceRef{ 1, 0 } },
                true);
        check(moveUvs.valid
                && moveUvs.movedVertices.size() == plan.movedVertices.size()
                && moveUvs.surfaces.size() == 2u,
            "geometry transform preview derives the Move UV plan from the exact shader-preview vertex set");
        check(scaleUvs.valid
                && scaleUvs.movedVertices.size() == plan.movedVertices.size()
                && scaleUvs.surfaces.size() == plan.wholeSurfaces.size() + 2u,
            "geometry transform preview includes complete faces in the Scale UV plan");
        check(mixedUvs.valid && mixedUvs.movedVertices.size() == 10u
                && mixedUvs.surfaces.size() == 4u,
            "numeric mixed sector/surface transforms capture one deduplicated reciprocal UV plan");
    }

    {
        const Level before = makePortalTransformFixture();
        Level after = before;
        const std::array selected{
            SurfaceRef{ 0, 0 }, SurfaceRef{ 0, 0 }
        };
        const render::GeometryTransformPreviewPlan plan =
            render::buildSurfaceTransformPreviewPlan(after, selected);
        const auto beforeUvs = captureUvs(before);

        level::levelTranslateSurfaces(
            after, selected, 2.0, -3.0, 5.0);

        check(changedVertices(before, after) == plannedVertices(plan),
            "geometry transform preview exactly matches Surface commit vertices, including duplicate selections and portal neighbors");
        check(captureUvs(after) == beforeUvs
                && after.uvCoordinateSpace == UvCoordinateSpace::Normalized,
            "geometry transform preview and Surface translation leave signed multi-tile normalized UVs byte-exact");
    }

    {
        const Level before = makePortalTransformFixture();
        const std::array selected{ SurfaceRef{ 0, 0 } };
        const render::GeometryTransformPreviewPlan plan =
            render::buildSurfaceTransformPreviewPlan(before, selected);
        const auto beforeUvs = captureUvs(before);

        Level rotated = before;
        level::levelRotateSurfaces(
            rotated,
            selected,
            TVector::up,
            0.5,
            0.5,
            0.0,
            37.0);
        check(changedVertices(before, rotated) == plannedVertices(plan),
            "geometry transform preview exactly matches Surface rotation vertices across a portal");
        check(captureUvs(rotated) == beforeUvs
                && rotated.uvCoordinateSpace == UvCoordinateSpace::Normalized,
            "Surface rotation preserves every signed multi-tile normalized UV and corner color");

        Level scaled = before;
        const ScaleData scale{
            .cx = 0.5,
            .cy = 0.5,
            .cz = 0.0,
            .sfactor = 1.75,
            .vec = TVector{},
            .how = ScaleHow::xyz,
        };
        level::levelScaleSurfaces(scaled, selected, scale, false);
        check(changedVertices(before, scaled) == plannedVertices(plan),
            "geometry transform preview exactly matches Surface scale vertices across a portal");
        check(captureUvs(scaled) == beforeUvs
                && scaled.uvCoordinateSpace == UvCoordinateSpace::Normalized,
            "Surface scale without texture-density scaling preserves every signed multi-tile normalized UV and corner color");
    }

    {
        const Level level = makePortalTransformFixture();
        const std::array selected{ std::int32_t{ 0 } };
        const render::GeometryTransformPreviewPlan plan =
            render::buildSectorTransformPreviewPlan(level, selected);

        check(plan.movedVertices.size() == 10u,
            "geometry transform preview records a complete Sector plus the distinct portal-mate vertices moved by commit");
        check(plan.wholeSurfaces.size() == 3u
                && containsSurface(plan.wholeSurfaces, SurfaceRef{ 0, 0 })
                && containsSurface(plan.wholeSurfaces, SurfaceRef{ 0, 1 })
                && containsSurface(plan.wholeSurfaces, SurfaceRef{ 1, 0 }),
            "geometry transform preview applies a Sector transform to all source faces and the complete portal mate");
        check(plan.partialCorners.size() == 2u
                && cornerCount(plan.partialCorners, SurfaceRef{ 1, 1 }) == 2u,
            "geometry transform preview deforms the unselected neighbor of a moved portal mate");
    }

    {
        const Level before = makePortalTransformFixture();
        Level after = before;
        const std::array selected{ std::int32_t{ 0 } };
        const render::GeometryTransformPreviewPlan plan =
            render::buildSectorTransformPreviewPlan(after, selected);
        const auto beforeUvs = captureUvs(before);
        const ScaleData scale{
            .cx = 0.25,
            .cy = -0.25,
            .cz = 0.5,
            .sfactor = 2.0,
            .vec = TVector{},
            .how = ScaleHow::xyz,
        };

        level::levelScaleSectors(after, selected, scale, false);

        check(changedVertices(before, after) == plannedVertices(plan),
            "geometry transform preview exactly matches Sector scale commit vertices across a portal");
        check(captureUvs(after) == beforeUvs
                && after.uvCoordinateSpace == UvCoordinateSpace::Normalized,
            "geometry transform preview and Sector scale preserve authored normalized UVs when texture scaling is disabled");
    }

    {
        const Level before = makePortalTransformFixture();
        const std::array selected{ std::int32_t{ 0 } };
        const render::GeometryTransformPreviewPlan plan =
            render::buildSectorTransformPreviewPlan(before, selected);
        const auto beforeUvs = captureUvs(before);

        Level translated = before;
        level::levelTranslateSectors(translated, selected, -4.0, 2.0, 3.0);
        check(changedVertices(before, translated) == plannedVertices(plan),
            "geometry transform preview exactly matches Sector translation vertices across a portal");
        check(captureUvs(translated) == beforeUvs
                && translated.uvCoordinateSpace == UvCoordinateSpace::Normalized,
            "Sector translation preserves every signed multi-tile normalized UV and corner color");

        Level rotated = before;
        level::levelRotateSectors(
            rotated,
            selected,
            TVector::right,
            0.5,
            0.5,
            0.0,
            -29.0);
        check(changedVertices(before, rotated) == plannedVertices(plan),
            "geometry transform preview exactly matches Sector rotation vertices across a portal");
        check(captureUvs(rotated) == beforeUvs
                && rotated.uvCoordinateSpace == UvCoordinateSpace::Normalized,
            "Sector rotation preserves every signed multi-tile normalized UV and corner color");
    }

    {
        Level level = makePortalTransformFixture();
        level.sectors[0].surfaces[0].adjoin = SurfaceRef{ 9, 9 };
        const std::array invalidSurfaces{
            SurfaceRef{ -1, 0 }, SurfaceRef{ 0, 99 }
        };
        const std::array invalidSectors{
            std::int32_t{ -1 }, std::int32_t{ 99 }
        };

        const render::GeometryTransformPreviewPlan surfacePlan =
            render::buildSurfaceTransformPreviewPlan(level, invalidSurfaces);
        const render::GeometryTransformPreviewPlan sectorPlan =
            render::buildSectorTransformPreviewPlan(level, invalidSectors);
        check(surfacePlan.empty() && sectorPlan.empty(),
            "geometry transform preview ignores invalid selections without publishing partial entries");
    }

    {
        constexpr int zeroFiltered = render::encodeFilteredTransformPreviewPickId(0);
        constexpr int ordinaryFiltered = render::encodeFilteredTransformPreviewPickId(73);
        constexpr int maximumFiltered = render::encodeFilteredTransformPreviewPickId(
            std::numeric_limits<int>::max());

        static_assert(zeroFiltered < 0);
        static_assert(ordinaryFiltered < 0);
        static_assert(maximumFiltered < 0);
        static_assert(render::decodeTransformPreviewPickId(zeroFiltered) == 0);
        static_assert(render::decodeTransformPreviewPickId(ordinaryFiltered) == 73);
        static_assert(render::decodeTransformPreviewPickId(maximumFiltered)
            == std::numeric_limits<int>::max());
        static_assert(render::decodeTransformPreviewPickId(73) == 73);
        check(render::transformPreviewPickIdIsFiltered(zeroFiltered)
                && render::transformPreviewPickIdIsFiltered(ordinaryFiltered)
                && !render::transformPreviewPickIdIsFiltered(73),
            "geometry transform preview encodes per-corner filtering without colliding with whole-surface IDs or overflowing at INT_MAX");
    }
}

} // namespace sed::app_tests
