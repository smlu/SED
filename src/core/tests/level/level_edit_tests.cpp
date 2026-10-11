#include "test_common.hpp"

#include "sed/level/build.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/edit.hpp"
#include "sed/level/geometry.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sed::tests
{

void runLevelEditTests()
{
    static_assert(!noexcept(sed::level::levelDeleteSectorVertex(
        std::declval<sed::Level&>(), 0, 0)));
    static_assert(!noexcept(sed::level::levelForceDeleteSectorVertex(
        std::declval<sed::Level&>(), 0, 0)));

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        (void)sector.newVertex(0.0, 0.0, 0.0);
        (void)sector.newVertex(1.0, 0.0, 0.0);
        (void)sector.newVertex(2.0, 0.0, 0.0);

        const bool ok = sed::level::levelDeleteSectorVertex(level, sector.num, 1, false);
        check(ok, "Level.deleteSectorVertex.unused: ok");
        check(sector.vertices.size() == 2, "Level.deleteSectorVertex.unused: vertex count");

        if (sector.vertices.size() == 2)
        {
            check(close(sector.vertices[0].x, 0.0), "Level.deleteSectorVertex.unused: v0.x");
            check(close(sector.vertices[1].x, 2.0), "Level.deleteSectorVertex.unused: v1.x");
        }
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t v4 = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(2.0, 1.0, 0.0);
        const std::int32_t v3 = sector.newVertex(0.0, 1.0, 0.0);

        sed::Surface surf{};
        surf.vertices    = { v0, v1, v4, v2, v3 };
        surf.texVertices = std::vector<sed::TexVertex>(surf.vertices.size());
        (void)sector.addSurface(std::move(surf));

        const bool ok = sed::level::levelDeleteSectorVertex(level, sector.num, v1, false);
        check(ok, "Level.deleteSectorVertex.singleSurface: ok");

        check(sector.vertices.size() == 4, "Level.deleteSectorVertex.singleSurface: vertex count");
        check(sector.surfaces.size() == 1, "Level.deleteSectorVertex.singleSurface: surface count");

        if (!sector.surfaces.empty())
        {
            const sed::Surface& out = sector.surfaces[0];
            check(out.vertices.size() == 4, "Level.deleteSectorVertex.singleSurface: out vertex count");
            check(out.texVertices.size() == 4, "Level.deleteSectorVertex.singleSurface: out tex vertex count");

            if (out.vertices.size() == 4)
            {
                const sed::Vertex& a = sector.vertices[static_cast<std::size_t>(out.vertices[0])];
                const sed::Vertex& b = sector.vertices[static_cast<std::size_t>(out.vertices[1])];
                const sed::Vertex& c = sector.vertices[static_cast<std::size_t>(out.vertices[2])];
                const sed::Vertex& d = sector.vertices[static_cast<std::size_t>(out.vertices[3])];

                check(close(a.x, 0.0) && close(a.y, 0.0), "Level.deleteSectorVertex.singleSurface: a");
                check(close(b.x, 2.0) && close(b.y, 0.0), "Level.deleteSectorVertex.singleSurface: b");
                check(close(c.x, 2.0) && close(c.y, 1.0), "Level.deleteSectorVertex.singleSurface: c");
                check(close(d.x, 0.0) && close(d.y, 1.0), "Level.deleteSectorVertex.singleSurface: d");
            }
        }

        check(sector.vertices.size() == 4, "Level.deleteSectorVertex.singleSurface: vertex count (after)");
        check(close(sector.vertices[1].x, 2.0), "Level.deleteSectorVertex.singleSurface: v4 remap");
    }

    {
        // Reproduce a marquee containing one removable edge subdivision and
        // one structural corner on otherwise valid closed geometry. Safe
        // Delete may remove the former and skip the latter, but it must never
        // remove a polygon or publish broken topology.
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& sector = sed::level::levelCreateCube(
            level,
            0.0,
            0.0,
            0.0,
            sed::TVector::up,
            sed::TVector::right);
        const std::int32_t first = sector.surfaces[0].vertices[0];
        const std::int32_t second = sector.surfaces[0].vertices[1];
        std::size_t mateSurface = sector.surfaces.size();
        std::size_t mateEdge = 0;
        for (std::size_t surfaceIndex = 1;
             surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            const sed::Surface& surface = sector.surfaces[surfaceIndex];
            for (std::size_t edge = 0; edge < surface.vertices.size(); ++edge)
            {
                if (surface.vertices[edge] == second
                    && surface.vertices[(edge + 1) % surface.vertices.size()]
                        == first)
                {
                    mateSurface = surfaceIndex;
                    mateEdge = edge;
                    break;
                }
            }
            if (mateSurface < sector.surfaces.size())
            {
                break;
            }
        }
        check(mateSurface < sector.surfaces.size(),
            "Level.deleteSectorVertex.marquee: closed cube edge has reciprocal owner");
        if (mateSurface < sector.surfaces.size())
        {
            for (std::size_t surfaceIndex = 0;
                 surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
            {
                sed::Surface& surface = sector.surfaces[surfaceIndex];
                surface.uScale = static_cast<float>(
                    -0.015625 * static_cast<double>(surfaceIndex + 1u));
                surface.vScale = static_cast<float>(
                    0.0234375 * static_cast<double>(surfaceIndex + 1u));
                surface.texVertices.resize(surface.vertices.size());
                for (std::size_t local = 0;
                     local < surface.texVertices.size(); ++local)
                {
                    surface.texVertices[local].u = static_cast<float>(
                        surfaceIndex * 100u + local);
                    surface.texVertices[local].v = -surface.texVertices[local].u;
                    surface.texVertices[local].color = sed::ColorF{
                        static_cast<float>(0.05 * (surfaceIndex + 1u)),
                        static_cast<float>(0.07 * (local + 1u)),
                        static_cast<float>(0.03 * (surfaceIndex + local + 1u)),
                        static_cast<float>(0.4 + 0.05 * local)
                    };
                }
            }
            const std::vector<sed::TexVertex> firstUvsBefore =
                sector.surfaces[0].texVertices;
            const std::vector<sed::TexVertex> mateUvsBefore =
                sector.surfaces[mateSurface].texVertices;
            const float firstUScaleBefore = sector.surfaces[0].uScale;
            const float firstVScaleBefore = sector.surfaces[0].vScale;
            const float mateUScaleBefore =
                sector.surfaces[mateSurface].uScale;
            const float mateVScaleBefore =
                sector.surfaces[mateSurface].vScale;

            const sed::Vertex& a =
                sector.vertices[static_cast<std::size_t>(first)];
            const sed::Vertex& b =
                sector.vertices[static_cast<std::size_t>(second)];
            const std::int32_t redundant = sector.newVertex(
                (a.x + b.x) * 0.5,
                (a.y + b.y) * 0.5,
                (a.z + b.z) * 0.5);

            auto insertAfter = [redundant](
                sed::Surface& surface,
                const std::size_t edge)
            {
                const std::size_t insert = edge + 1;
                surface.vertices.insert(
                    surface.vertices.begin()
                        + static_cast<std::ptrdiff_t>(insert),
                    redundant);
                if (surface.texVertices.size() != surface.vertices.size() - 1u)
                {
                    surface.texVertices.resize(surface.vertices.size() - 1u);
                }
                surface.texVertices.insert(
                    surface.texVertices.begin()
                        + static_cast<std::ptrdiff_t>(insert),
                    sed::TexVertex{});
            };
            insertAfter(sector.surfaces[0], 0);
            insertAfter(sector.surfaces[mateSurface], mateEdge);

            const std::size_t beforeSurfaceCount = sector.surfaces.size();
            const bool structuralRejected = !sed::level::levelDeleteSectorVertex(
                level, sector.num, first);
            const bool redundantDeleted = sed::level::levelDeleteSectorVertex(
                level, sector.num, redundant);
            const sed::Sector& result = level.sectors[0];
            auto hasDirectedEdge = [](const sed::Surface& surface,
                                      const std::int32_t a,
                                      const std::int32_t b) -> bool
            {
                for (std::size_t edge = 0; edge < surface.vertices.size(); ++edge)
                {
                    if (surface.vertices[edge] == a
                        && surface.vertices[(edge + 1) % surface.vertices.size()] == b)
                    {
                        return true;
                    }
                }
                return false;
            };
            std::unordered_map<std::uint64_t, std::size_t> edgeOwners{};
            for (const sed::Surface& surface : result.surfaces)
            {
                for (std::size_t edge = 0; edge < surface.vertices.size(); ++edge)
                {
                    const std::uint32_t aIndex = static_cast<std::uint32_t>(
                        std::min(surface.vertices[edge],
                            surface.vertices[(edge + 1) % surface.vertices.size()]));
                    const std::uint32_t bIndex = static_cast<std::uint32_t>(
                        std::max(surface.vertices[edge],
                            surface.vertices[(edge + 1) % surface.vertices.size()]));
                    ++edgeOwners[(static_cast<std::uint64_t>(aIndex) << 32u)
                        | bIndex];
                }
            }
            const bool closedEdgeIncidence = std::all_of(
                edgeOwners.begin(), edgeOwners.end(),
                [](const auto& edge) { return edge.second == 2u; });
            auto uvsPreserved = [](const std::vector<sed::TexVertex>& actual,
                                   const std::vector<sed::TexVertex>& expected)
            {
                if (actual.size() != expected.size())
                {
                    return false;
                }
                for (std::size_t index = 0; index < actual.size(); ++index)
                {
                    if (actual[index].u != expected[index].u
                        || actual[index].v != expected[index].v
                        || actual[index].color != expected[index].color)
                    {
                        return false;
                    }
                }
                return true;
            };
            const bool firstUvsPreserved = uvsPreserved(
                result.surfaces[0].texVertices, firstUvsBefore);
            const bool mateUvsPreserved = uvsPreserved(
                result.surfaces[mateSurface].texVertices, mateUvsBefore);
            check(structuralRejected && redundantDeleted
                    && result.surfaces.size() == beforeSurfaceCount
                    && result.vertices.size() == 8
                    && sed::level::sectorIsConvex(result)
                    && hasDirectedEdge(result.surfaces[0], first, second)
                    && hasDirectedEdge(result.surfaces[mateSurface], second, first)
                    && closedEdgeIncidence
                    && level.uvCoordinateSpace
                        == sed::UvCoordinateSpace::Normalized
                    && firstUvsPreserved
                    && mateUvsPreserved
                    && result.surfaces[0].uScale == firstUScaleBefore
                    && result.surfaces[0].vScale == firstVScaleBefore
                    && result.surfaces[mateSurface].uScale
                        == mateUScaleBefore
                    && result.surfaces[mateSurface].vScale
                        == mateVScaleBefore
                    && std::all_of(
                        result.surfaces.begin(),
                        result.surfaces.end(),
                        [](const sed::Surface& surface)
                        {
                            return surface.vertices.size() >= 3
                                && surface.vertices.size()
                                    == surface.texVertices.size();
                        }),
                "Level.deleteSectorVertex.marquee: mixed safe selection preserves every face and valid closed geometry");
        }
    }

    {
        // Imported/recovered polygons can carry equal winding on the two
        // owners of an otherwise identical split edge. Safe removal is based
        // on the restored endpoint pair, not that incidental winding.
        sed::Level level{};
        sed::Sector& sector = level.newSector();
        const std::int32_t a = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t middle = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t b = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t upper = sector.newVertex(1.0, 1.0, 0.0);
        const std::int32_t lower = sector.newVertex(1.0, -1.0, 0.0);
        sed::Surface firstOwner{};
        firstOwner.vertices = { a, middle, b, upper };
        firstOwner.texVertices.resize(4);
        (void)sector.addSurface(std::move(firstOwner));
        sed::Surface sameWindingOwner{};
        sameWindingOwner.vertices = { a, middle, b, lower };
        sameWindingOwner.texVertices.resize(4);
        (void)sector.addSurface(std::move(sameWindingOwner));

        check(sed::level::levelDeleteSectorVertex(level, sector.num, middle)
                && sector.vertices.size() == 4
                && sector.surfaces[0].vertices.size() == 3
                && sector.surfaces[1].vertices.size() == 3,
            "Level.deleteSectorVertex.sharedEdge: equal-winding owners restore the same unsplit endpoint pair atomically");
    }

    {
        // Reproduce the editor command chain rather than hand-authoring its
        // output: Edge Cleave inserts one sector vertex into both polygon
        // owners, and safe Delete must collapse that subdivision again.
        sed::Level level{};
        sed::Sector& sector = sed::level::levelCreateCube(
            level,
            0.0,
            0.0,
            0.0,
            sed::TVector::up,
            sed::TVector::right);
        const std::int32_t edgeFirst = sector.surfaces[0].vertices[0];
        const std::int32_t edgeSecond = sector.surfaces[0].vertices[1];
        const sed::Vertex first =
            sector.vertices[static_cast<std::size_t>(edgeFirst)];
        const sed::Vertex second =
            sector.vertices[static_cast<std::size_t>(edgeSecond)];
        sed::TVector planeNormal{
            second.x - first.x,
            second.y - first.y,
            second.z - first.z,
        };
        planeNormal.Normalize();
        const sed::TVector midpoint{
            (first.x + second.x) * 0.5,
            (first.y + second.y) * 0.5,
            (first.z + second.z) * 0.5,
        };
        const std::size_t originalVertexCount = sector.vertices.size();
        const bool split = sed::level::levelCleaveEdge(
            level,
            sed::SurfaceRef{ sector.num, 0 },
            0,
            planeNormal,
            midpoint.x,
            midpoint.y,
            midpoint.z);
        const std::int32_t splitVertex = split
            ? static_cast<std::int32_t>(sector.vertices.size() - 1u)
            : -1;
        std::size_t ownersBeforeDelete = 0;
        for (const sed::Surface& surface : sector.surfaces)
        {
            ownersBeforeDelete += static_cast<std::size_t>(
                std::ranges::count(surface.vertices, splitVertex));
        }

        const bool deleted = split
            && sed::level::levelDeleteSectorVertex(
                level, sector.num, splitVertex);
        std::size_t ownersAfterDelete = 0;
        for (const sed::Surface& surface : sector.surfaces)
        {
            ownersAfterDelete += static_cast<std::size_t>(
                std::ranges::count(surface.vertices, splitVertex));
        }
        check(split && ownersBeforeDelete == 2u && deleted
                && sector.vertices.size() == originalVertexCount
                && ownersAfterDelete == 0u
                && sed::level::sectorIsConvex(sector),
            "Level.deleteSectorVertex.edgeCleaveRoundTrip: removes the shared redundant vertex produced by the editor command");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();
        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(1.0, 1.0, 0.0);
        const std::int32_t v3 = sector.newVertex(0.0, 1.0, 0.0);
        sed::Surface surface{};
        surface.vertices = { v0, v1, v2, v3 };
        surface.texVertices.resize(4);
        (void)sector.addSurface(std::move(surface));

        check(!sed::level::levelDeleteSectorVertex(level, sector.num, v1),
            "Level.deleteSectorVertex.corner: safe delete rejects structural corner");
        check(sed::level::levelForceDeleteSectorVertex(level, sector.num, v1),
            "Level.forceDeleteSectorVertex.corner: confirmed path removes structural corner");
        check(sector.vertices.size() == 3 && sector.surfaces[0].vertices.size() == 3,
            "Level.forceDeleteSectorVertex.corner: references compacted");
    }

    {
        struct CardinalityCase
        {
            const char* name{};
            std::size_t textureCoordinateCount{};
        };
        const std::array cardinalityCases{
            CardinalityCase{ "missing corner coordinate", 3u },
            CardinalityCase{ "excess corner coordinate", 5u },
        };

        for (const CardinalityCase& testCase : cardinalityCases)
        {
            sed::Level malformedLevel{};
            malformedLevel.uvCoordinateSpace =
                sed::UvCoordinateSpace::Normalized;
            sed::Sector& sector = malformedLevel.newSector();
            const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
            const std::int32_t v1 = sector.newVertex(1.0, 0.0, 0.0);
            const std::int32_t v2 = sector.newVertex(1.0, 1.0, 0.0);
            const std::int32_t v3 = sector.newVertex(0.0, 1.0, 0.0);
            sed::Surface surface{};
            surface.vertices = { v0, v1, v2, v3 };
            surface.texVertices.resize(
                testCase.textureCoordinateCount,
                sed::TexVertex{ 7.0f, -9.0f, sed::ColorRed });
            surface.uScale = -0.125f;
            surface.vScale = 0.375f;
            (void)sector.addSurface(std::move(surface));

            const auto remainsExact = [](const sed::Level& actual,
                                          const sed::Level& expected)
            {
                if (actual.uvCoordinateSpace != expected.uvCoordinateSpace
                    || actual.sectors.size() != expected.sectors.size()
                    || actual.sectors[0].vertices.size()
                        != expected.sectors[0].vertices.size()
                    || actual.sectors[0].surfaces.size()
                        != expected.sectors[0].surfaces.size())
                {
                    return false;
                }
                const sed::Surface& actualSurface =
                    actual.sectors[0].surfaces[0];
                const sed::Surface& expectedSurface =
                    expected.sectors[0].surfaces[0];
                if (actualSurface.vertices != expectedSurface.vertices
                    || actualSurface.texVertices.size()
                        != expectedSurface.texVertices.size()
                    || actualSurface.uScale != expectedSurface.uScale
                    || actualSurface.vScale != expectedSurface.vScale)
                {
                    return false;
                }
                for (std::size_t corner = 0;
                     corner < expectedSurface.texVertices.size();
                     ++corner)
                {
                    if (actualSurface.texVertices[corner].u
                            != expectedSurface.texVertices[corner].u
                        || actualSurface.texVertices[corner].v
                            != expectedSurface.texVertices[corner].v
                        || actualSurface.texVertices[corner].color
                            != expectedSurface.texVertices[corner].color)
                    {
                        return false;
                    }
                }
                return true;
            };

            const sed::Level baseline = malformedLevel;
            sed::Level safeCandidate = malformedLevel;
            sed::Level forceCandidate = malformedLevel;
            const bool safeRejected =
                !sed::level::levelDeleteSectorVertex(
                    safeCandidate, 0, v1);
            const bool forceRejected =
                !sed::level::levelForceDeleteSectorVertex(
                    forceCandidate, 0, v1);
            const std::string safeMessage =
                "Level.deleteSectorVertex rejects "
                + std::string(testCase.name)
                + " without inventing UVs";
            const std::string forceMessage =
                "Level.forceDeleteSectorVertex rejects "
                + std::string(testCase.name)
                + " without inventing UVs";
            check(safeRejected && remainsExact(safeCandidate, baseline),
                safeMessage.c_str());
            check(forceRejected && remainsExact(forceCandidate, baseline),
                forceMessage.c_str());
        }
    }

    {
        sed::Level level{};
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        sed::Sector& sector = level.newSector();
        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(1.0, 1.0, 0.0);
        const std::int32_t v3 = sector.newVertex(0.0, 1.0, 0.0);
        sed::Surface surface{};
        surface.vertices = { v0, v1, v2, v3 };
        surface.texVertices = {
            { -0.25f, 0.5f, sed::ColorRed },
            { 1.25f, 0.75f, sed::ColorGreen },
            { 1.5f, 2.25f, sed::ColorBlue },
            { -0.5f, 2.0f, sed::ColorWhite },
        };
        surface.uScale = -0.125f;
        surface.vScale = 0.375f;
        (void)sector.addSurface(std::move(surface));

        const bool deleted = sed::level::levelForceDeleteSectorVertex(
            level, 0, v1);
        const sed::Surface& result = level.sectors[0].surfaces[0];
        check(deleted
                && level.uvCoordinateSpace
                    == sed::UvCoordinateSpace::Normalized
                && result.vertices
                    == std::vector<std::int32_t>{ 0, 1, 2 }
                && result.texVertices.size() == 3u
                && result.texVertices[0].u == -0.25f
                && result.texVertices[0].v == 0.5f
                && result.texVertices[0].color == sed::ColorRed
                && result.texVertices[1].u == 1.5f
                && result.texVertices[1].v == 2.25f
                && result.texVertices[1].color == sed::ColorBlue
                && result.texVertices[2].u == -0.5f
                && result.texVertices[2].v == 2.0f
                && result.texVertices[2].color == sed::ColorWhite
                && result.uScale == -0.125f
                && result.vScale == 0.375f,
            "Level.forceDeleteSectorVertex preserves the exact remaining normalized UV/color tuples and signed scales");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();
        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(2.0, 0.0, 0.0);
        const std::int32_t v3 = sector.newVertex(0.0, 1.0, 0.0);
        sed::Surface malformed{};
        malformed.vertices = { v0, v1, v2, v3, v1 };
        malformed.texVertices.resize(malformed.vertices.size());
        (void)sector.addSurface(std::move(malformed));

        const std::vector<std::int32_t> before = sector.surfaces[0].vertices;
        check(!sed::level::levelDeleteSectorVertex(level, sector.num, v1)
                && sector.surfaces[0].vertices == before
                && sector.vertices.size() == 4,
            "Level.deleteSectorVertex.duplicateReference: safe delete rejects every malformed repeated occurrence atomically");
    }

    {
        sed::Level level{};
        sed::Sector& sector = level.newSector();

        const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
        const std::int32_t v1 = sector.newVertex(1.0, 0.0, 0.0);
        const std::int32_t v2 = sector.newVertex(0.0, 1.0, 0.0);
        const std::int32_t v3 = sector.newVertex(-1.0, 0.0, 0.0);

        sed::Surface s0{};
        s0.vertices    = { v0, v1, v2 };
        s0.texVertices = std::vector<sed::TexVertex>(s0.vertices.size());
        (void)sector.addSurface(std::move(s0));

        sed::Surface s1{};
        s1.vertices    = { v0, v2, v3 };
        s1.texVertices = std::vector<sed::TexVertex>(s1.vertices.size());
        (void)sector.addSurface(std::move(s1));

        sed::Surface s2{};
        s2.vertices    = { v0, v3, v1 };
        s2.texVertices = std::vector<sed::TexVertex>(s2.vertices.size());
        (void)sector.addSurface(std::move(s2));

        const std::size_t beforeVertices = sector.vertices.size();
        const std::size_t beforeSurfaces = sector.surfaces.size();

        const bool ok = sed::level::levelDeleteSectorVertex(level, sector.num, v0, true);
        check(!ok, "Level.deleteSectorVertex.tooManySurfaces: fails");

        check(sector.vertices.size() == beforeVertices, "Level.deleteSectorVertex.tooManySurfaces: vertex count");
        check(sector.surfaces.size() == beforeSurfaces, "Level.deleteSectorVertex.tooManySurfaces: surface count");
    }

    {
        sed::Level level{};
        (void)level.newCog();
        (void)level.newCog();

        sed::CogValue v{};
        v.name     = "ref";
        v.type     = sed::ValueType::integer;
        v.intValue = 0;
        v.cogType  = sed::CogType::cog;
        level.cogs[1].values.push_back(v);

        const bool ok = sed::level::levelDeleteCog(level, 0);
        check(ok, "Level.deleteCog: ok");
        check(level.cogs.size() == 1, "Level.deleteCog: count");
        check(level.cogs[0].num == 0, "Level.deleteCog: renumber");
        check(level.cogs[0].values.size() == 1, "Level.deleteCog: values");

        if (!level.cogs[0].values.empty())
        {
            const sed::CogValue& v = level.cogs[0].values[0];
            check(v.intValue == -1, "Level.deleteCog: value remap");
        }
    }

    {
        sed::Level level{};
        (void)level.newThing();
        (void)level.newThing();
        (void)level.newThing();

        const bool ok = sed::level::levelDeleteThing(level, 1);
        check(ok, "Level.deleteThing: ok");
        check(level.things.size() == 2, "Level.deleteThing: count");
        check(level.things[0].num == 0, "Level.deleteThing: renumber 0");
        check(level.things[1].num == 1, "Level.deleteThing: renumber 1");
    }

    {
        // Thing indices are the identity carried by typed COG Thing
        // references.  Removing one Thing must clear the deleted target and
        // shift only the surviving targets that followed it.
        for (const std::int32_t deleted : { 0, 1, 2, 3 })
        {
            sed::Level level{};
            for (int i = 0; i < 4; ++i)
            {
                (void)level.newThing();
            }

            sed::Cog& cog = level.newCog();
            for (const std::int32_t target : { 0, 1, 2, 3, -1 })
            {
                sed::CogValue value{};
                value.name = "thing_target";
                value.type = sed::ValueType::ptr;
                value.cogType = sed::CogType::thing;
                value.intValue = target;
                cog.values.push_back(std::move(value));
            }
            const std::int32_t invalidBefore = 99;
            sed::CogValue invalid{};
            invalid.name = "out_of_range_thing";
            invalid.type = sed::ValueType::ptr;
            invalid.cogType = sed::CogType::thing;
            invalid.intValue = invalidBefore;
            cog.values.push_back(std::move(invalid));

            check(!sed::level::levelDeleteThing(level, -1)
                    && !sed::level::levelDeleteThing(level, 4),
                "Level.deleteThing.invalid: rejects both index boundaries");
            check(sed::level::levelDeleteThing(level, deleted),
                "Level.deleteThing.cogReferences: deletion succeeds for every position");

            const std::array<std::int32_t, 5> expected{
                deleted == 0 ? -1 : 0,
                deleted == 1 ? -1 : (deleted < 1 ? 0 : 1),
                deleted == 2 ? -1 : (deleted < 2 ? 1 : 2),
                deleted == 3 ? -1 : (deleted < 3 ? 2 : 3),
                -1};
            bool mapped = level.cogs.size() == 1 && level.cogs[0].values.size() == 6;
            if (mapped)
            {
                for (std::size_t index = 0; index < expected.size(); ++index)
                {
                    mapped = mapped
                        && level.cogs[0].values[index].intValue == expected[index];
                }
                mapped = mapped
                    && level.cogs[0].values[5].intValue == invalidBefore;
            }
            check(mapped,
                "Level.deleteThing.cogReferences: clears deleted targets and shifts surviving targets");
        }
    }

    {
        sed::Level level{};
        const std::int32_t firstId = level.newLight().id;
        const std::int32_t middleId = level.newLight().id;
        const std::int32_t lastId = level.newLight().id;

        check(!sed::level::levelDeleteLight(level, -1)
                && !sed::level::levelDeleteLight(level, 3)
                && level.lights.size() == 3u,
            "Level.deleteLight.invalid: rejects both index boundaries atomically");

        check(sed::level::levelDeleteLight(level, 1),
            "Level.deleteLight.middle: succeeds");
        check(level.lights.size() == 2u
                && level.lights[0].id == firstId
                && level.lights[1].id == lastId
                && std::ranges::none_of(
                    level.lights,
                    [middleId](const sed::SedLight& light)
                    {
                        return light.id == middleId;
                    }),
            "Level.deleteLight.middle: removes only the requested light and preserves order");

        check(sed::level::levelDeleteLight(level, 0)
                && sed::level::levelDeleteLight(level, 0)
                && level.lights.empty()
                && !sed::level::levelDeleteLight(level, 0),
            "Level.deleteLight.ends: deletes first and final entries then rejects empty input");
    }
}

} // namespace sed::tests
