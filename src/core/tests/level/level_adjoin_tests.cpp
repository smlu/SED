#include "test_common.hpp"
#include "sed/assets/sed.hpp"

#include "sed/level/cleave.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/merge.hpp"
#include "sed/level.hpp"

#include <algorithm>
#include <array>

namespace sed::tests
{

namespace
{

bool sameTextureState(
    const sed::Surface& actual,
    const sed::Surface& expected) noexcept
{
    if ( actual.uScale != expected.uScale
        || actual.vScale != expected.vScale
        || actual.texVertices.size() != expected.texVertices.size() )
    {
        return false;
    }

    for ( std::size_t corner = 0;
        corner < actual.texVertices.size();
        ++corner )
    {
        const sed::TexVertex& lhs = actual.texVertices[corner];
        const sed::TexVertex& rhs = expected.texVertices[corner];
        if ( lhs.u != rhs.u || lhs.v != rhs.v || lhs.color != rhs.color )
        {
            return false;
        }
    }
    return true;
}

} // namespace

void runLevelAdjoinTests()
{
    {
        sed::Level level{};
        level.kind = sed::ProjectType::JKDF2;
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        level.sectors.push_back(makeLitTestCubeSector(2.0, sed::ColorWhite));
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));

        auto splitSurface = [](sed::Sector& sector,
                                const std::size_t surfaceIndex,
                                const std::initializer_list<std::int32_t> firstVertices,
                                const std::initializer_list<std::int32_t> secondVertices)
        {
            auto makeSurface = [&sector](const std::initializer_list<std::int32_t> vertices)
            {
                sed::Surface surface{};
                surface.vertices.assign(vertices.begin(), vertices.end());
                surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
                return surface;
            };

            sector.surfaces[surfaceIndex] = makeSurface(firstVertices);
            sector.surfaces.insert(
                sector.surfaces.begin() + static_cast<std::ptrdiff_t>(surfaceIndex + 1),
                makeSurface(secondVertices));
        };

        // Split the shared x=1 boundary into two adjacent, non-overlapping portal faces.
        const std::int32_t aLowerMid = level.sectors[0].addVertex(1.0, 0.0, -1.0);
        const std::int32_t aUpperMid = level.sectors[0].addVertex(1.0, 0.0, 1.0);
        splitSurface(level.sectors[0], 0,
            {1, 5, aUpperMid, aLowerMid},
            {aLowerMid, aUpperMid, 6, 2});

        const std::int32_t bLowerMid = level.sectors[1].addVertex(1.0, 0.0, -1.0);
        const std::int32_t bUpperMid = level.sectors[1].addVertex(1.0, 0.0, 1.0);
        splitSurface(level.sectors[1], 1,
            {0, bLowerMid, bUpperMid, 4},
            {bLowerMid, 3, 7, bUpperMid});

        // A third sector shares a separate face with sector 0 and must remain adjoined.
        for ( sed::Vertex& vertex : level.sectors[2].vertices )
        {
            vertex.y += 2.0;
        }

        check(sed::level::levelAdjoinSectors(level, 0, 1),
            "levelUnadjoinSectors: fixture establishes both distinct portal adjoins");
        check(sed::level::levelAdjoinSectors(level, 0, 2),
            "levelUnadjoinSectors: fixture establishes unrelated sector portal");

        auto pointsTo = [&level](const sed::SurfaceRef source, const sed::SurfaceRef destination)
        {
            const sed::Surface& surface = level.sectors[static_cast<std::size_t>(source.sector)]
                .surfaces[static_cast<std::size_t>(source.surface)];
            return surface.adjoin
                && surface.adjoin->sector == destination.sector
                && surface.adjoin->surface == destination.surface;
        };

        const bool twoPortalsReciprocal = pointsTo({0, 0}, {1, 1})
            && pointsTo({1, 1}, {0, 0})
            && pointsTo({0, 1}, {1, 2})
            && pointsTo({1, 2}, {0, 1});
        check(twoPortalsReciprocal,
            "levelUnadjoinSectors: fixture has two reciprocal, distinct portals");

        const sed::SurfaceRef unrelatedA{0, 3};
        const sed::SurfaceRef unrelatedB{2, 3};
        const bool unrelatedWasReciprocal = pointsTo(unrelatedA, unrelatedB)
            && pointsTo(unrelatedB, unrelatedA);
        check(unrelatedWasReciprocal,
            "levelUnadjoinSectors: fixture unrelated portal is reciprocal");

        const bool unjoined = sed::level::levelUnadjoinSectors(level, 0, 1);
        check(unjoined, "levelUnadjoinSectors: multi-portal call returns true");
        check(!level.sectors[0].surfaces[0].adjoin
                && !level.sectors[0].surfaces[1].adjoin
                && !level.sectors[1].surfaces[1].adjoin
                && !level.sectors[1].surfaces[2].adjoin,
            "levelUnadjoinSectors: removes both directions of every matching portal");
        check(pointsTo(unrelatedA, unrelatedB) && pointsTo(unrelatedB, unrelatedA),
            "levelUnadjoinSectors: leaves unrelated sector portal intact");
        check(!sed::level::levelUnadjoinSectors(level, 0, 1),
            "levelUnadjoinSectors: returns false when no matching portal remains");
        check(pointsTo(unrelatedA, unrelatedB) && pointsTo(unrelatedB, unrelatedA),
            "levelUnadjoinSectors: no-op call still preserves unrelated portal");
    }

    for ( const double warp : {0.0, 0.00005, 0.01} )
    {
        for ( const double gap : {0.0, 0.0000001} )
        {
            for ( std::size_t rotation = 0; rotation < 4; ++rotation )
            {
                sed::Level level;
                level.sectors.push_back(makeLitTestCubeSector(0, sed::ColorWhite));
                level.sectors.push_back(makeLitTestCubeSector(2, sed::ColorWhite));
                level.sectors[0].vertices[6].x += warp;
                level.sectors[1].vertices[7].x += warp;
                for ( auto& vertex : level.sectors[1].vertices )
                {
                    vertex.x += gap;
                }
                auto& mirror = level.sectors[1].surfaces[1];
                std::rotate(mirror.vertices.begin(), mirror.vertices.begin() + rotation, mirror.vertices.end());
                std::rotate(mirror.texVertices.begin(), mirror.texVertices.begin() + rotation, mirror.texVertices.end());
                sed::Level expected = level;
                const bool shouldAdjoin = gap == 0.0 && warp < 0.01;
                if ( shouldAdjoin )
                {
                    expected.sectors[0].surfaces[0].adjoin = sed::SurfaceRef{1, 1};
                    expected.sectors[1].surfaces[1].adjoin = sed::SurfaceRef{0, 0};
                    expected.sectors[0].surfaces[0].nAdj = -1;
                    expected.sectors[1].surfaces[1].nAdj = -1;
                }
                check(sed::level::levelAdjoinSectors(level, 0, 1, false) == shouldAdjoin,
                    "Adjoin accepts exact rounded boundaries independently of cyclic start and rejects real warp/gaps");
                check(sed::assets::sedLevelsEqual(level, expected),
                    "Adjoin changes only the two specified reciprocal links or preserves every input field");
            }
        }
    }

    for ( const double gap : { 0.0005, 0.005, 0.019 } )
    {
        sed::Level level{};
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        level.sectors.push_back(makeLitTestCubeSector(2.0 + gap, sed::ColorWhite));
        check(!sed::level::levelAdjoinSectors(level, 0, 1, true),
            "release F2: adjoin rejects parallel faces with a real normal-axis gap");
        for ( const auto& sector : level.sectors )
        {
            for ( const auto& face : sector.surfaces )
            {
                check(!face.adjoin, "release F2: rejected separated portal publishes no links");
            }
        }
    }
    {
        sed::Level level{};
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        level.sectors.push_back(makeLitTestCubeSector(2.0, sed::ColorWhite));
        check(sed::level::levelAdjoinSectors(level, 0, 1, true),
            "release F2: exactly contacting cubes remain adjoinable");
        for ( auto& vertex : level.sectors[1].vertices )
        {
            vertex.x += 0.005;
        }
        check(!sed::level::levelMergeSectors(level, 0, 1),
            "release F2: existing reciprocal links cannot bypass coincident portal admission");
        check(level.sectors.size() == 2,
            "release F2: invalid stored portal does not publish an open merged solid");
    }
    {
        sed::Level level{};
        level.sectors.push_back(makeLitTestCubeSector(0.0, sed::ColorWhite));
        level.sectors.push_back(makeLitTestCubeSector(2.0, sed::ColorWhite));
        for ( auto& vertex : level.sectors[1].vertices )
        {
            if ( vertex.y > 0.0 )
            {
                vertex.x += 0.0001;
            }
        }
        for ( auto& face : level.sectors[1].surfaces )
        {
            face.normal = sed::level::surfaceCalcNormalNewell(level.sectors[1], face);
        }
        check(!sed::level::levelAdjoinSectors(level, 0, 1, true),
            "release F2: small plane tilt cannot be hidden by opposite-normal tolerance");
    }
    {
        sed::Surface surf{};
        surf.surfaceFlags = 0x7B;
        surf.faceFlags    = 0x1C8u;
        surf.geo          = 7;
        surf.adjoinFlags  = 0x0;

        sed::level::surfaceSetAdjoinParams(sed::ProjectType::JKDF2, surf);

        check(surf.surfaceFlags == 0x0, "surfaceSetAdjoinParams: surfaceFlags");
        check(surf.faceFlags == 0x0u, "surfaceSetAdjoinParams: faceFlags");
        check(surf.geo == 0, "surfaceSetAdjoinParams: geo");
        check((surf.adjoinFlags & sed::SAF_Visible) != 0, "surfaceSetAdjoinParams: SAF_Visible");
        check((surf.adjoinFlags & sed::SAF_Move) != 0, "surfaceSetAdjoinParams: SAF_Move");
        check((surf.adjoinFlags & sed::SAF_AllowSoundPass) != 0, "surfaceSetAdjoinParams: SAF_AllowSoundPass");
    }

    {
        sed::Surface surf{};
        sed::level::surfaceSetAdjoinParams(sed::ProjectType::IJIM, surf);
        check((surf.adjoinFlags & sed::SAF_AllowSoundPass) == 0, "surfaceSetAdjoinParams(IJIM): SAF_AllowSoundPass");
    }

    {
        sed::Surface surf{};
        surf.material = "";

        sed::level::surfaceClearAdjoinParams(surf);

        check(surf.surfaceFlags == sed::SF_Collision, "surfaceClearAdjoinParams: surfaceFlags");
        check(surf.faceFlags == 0u, "surfaceClearAdjoinParams: faceFlags");
        check(surf.geo == 4, "surfaceClearAdjoinParams: geo");
        check(surf.material == "dflt.mat", "surfaceClearAdjoinParams: default material");
    }

    {
        sed::Level level{};
        level.clear();
        level.kind = sed::ProjectType::JKDF2;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        level.sectors.resize(2);

        sed::Sector& a = level.sectors[0];
        sed::Sector& b = level.sectors[1];

        a.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 2.0, 1.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        b.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
            { 2.0, 1.0, 0.0, 0 },
            { 2.0, 0.0, 0.0, 0 },
            { 1.0, 0.0, 0.0, 0 },
        };

        sed::Surface sa{};
        sa.vertices      = { 0, 1, 2, 3, 4 };
        sa.texVertices = {
            { -2.5f, 3.75f, sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } },
            { 5.25f, -7.5f, sed::ColorF{ 0.2f, 0.3f, 0.4f, 0.5f } },
            { 11.0f, 13.25f, sed::ColorF{ 0.3f, 0.4f, 0.5f, 0.6f } },
            { -17.75f, 19.5f, sed::ColorF{ 0.4f, 0.5f, 0.6f, 0.7f } },
            { 23.0f, -29.25f, sed::ColorF{ 0.5f, 0.6f, 0.7f, 0.8f } },
        };
        sa.uScale = -0.03125f;
        sa.vScale = 0.078125f;
        sa.surfaceFlags  = 0x6F;
        sa.faceFlags     = 0xDEu;
        sa.geo           = 333;
        sa.adjoinFlags   = 0x1BC;
        sa.material      = "";
        sa.adjoin.reset();
        sa.nAdj = 123;
        const sed::Surface expectedATextureState = sa;
        a.surfaces.push_back(sa);

        sed::Surface sb{};
        sb.vertices      = { 0, 1, 2, 3, 4 };
        sb.texVertices = {
            { 31.5f, -37.75f, sed::ColorF{ 0.15f, 0.25f, 0.35f, 0.45f } },
            { -41.0f, 43.25f, sed::ColorF{ 0.25f, 0.35f, 0.45f, 0.55f } },
            { 47.5f, -53.75f, sed::ColorF{ 0.35f, 0.45f, 0.55f, 0.65f } },
            { -59.0f, 61.25f, sed::ColorF{ 0.45f, 0.55f, 0.65f, 0.75f } },
            { 67.5f, -71.75f, sed::ColorF{ 0.55f, 0.65f, 0.75f, 0.85f } },
        };
        sb.uScale = 0.046875f;
        sb.vScale = -0.109375f;
        sb.surfaceFlags  = 0x309;
        sb.faceFlags     = 0x378u;
        sb.geo           = 999;
        sb.adjoinFlags   = 0x457;
        sb.material      = "";
        sb.adjoin.reset();
        sb.nAdj = 456;
        const sed::Surface expectedBTextureState = sb;
        b.surfaces.push_back(sb);

        check(sed::level::surfaceDoOverlap(a, a.surfaces[0], b, b.surfaces[0]), "surfaceDoOverlap: rectangle+collinear");

        const bool made = sed::level::levelMakeAdjoin(level, sed::SurfaceRef{ 0, 0 });
        check(made, "makeAdjoin: returns true");

        check(level.sectors[0].surfaces[0].adjoin.has_value(), "makeAdjoin: a.adjoin");
        check(level.sectors[1].surfaces[0].adjoin.has_value(), "makeAdjoin: b.adjoin");
        if (level.sectors[0].surfaces[0].adjoin && level.sectors[1].surfaces[0].adjoin)
        {
            check(level.sectors[0].surfaces[0].adjoin->sector == 1, "makeAdjoin: a.adjoin.sector");
            check(level.sectors[0].surfaces[0].adjoin->surface == 0, "makeAdjoin: a.adjoin.surface");
            check(level.sectors[1].surfaces[0].adjoin->sector == 0, "makeAdjoin: b.adjoin.sector");
            check(level.sectors[1].surfaces[0].adjoin->surface == 0, "makeAdjoin: b.adjoin.surface");
        }

        check(level.sectors[0].surfaces[0].nAdj == -1, "makeAdjoin: a.nAdj");
        check(level.sectors[1].surfaces[0].nAdj == -1, "makeAdjoin: b.nAdj");

        check(level.sectors[0].surfaces[0].surfaceFlags == 0x0, "makeAdjoin: a.surfaceFlags reset");
        check(level.sectors[0].surfaces[0].faceFlags == 0x0u, "makeAdjoin: a.faceFlags reset");
        check(level.sectors[0].surfaces[0].geo == 0, "makeAdjoin: a.geo reset");

        check(level.sectors[1].surfaces[0].surfaceFlags == 0x0, "makeAdjoin: b.surfaceFlags reset");
        check(level.sectors[1].surfaces[0].faceFlags == 0x0u, "makeAdjoin: b.faceFlags reset");
        check(level.sectors[1].surfaces[0].geo == 0, "makeAdjoin: b.geo reset");

        check((level.sectors[0].surfaces[0].adjoinFlags & sed::SAF_AllowSoundPass) != 0, "makeAdjoin: a.adjoinFlags AllowSoundPass");
        check((level.sectors[1].surfaces[0].adjoinFlags & sed::SAF_AllowSoundPass) != 0, "makeAdjoin: b.adjoinFlags AllowSoundPass");
        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized
                && sameTextureState(
                    level.sectors[0].surfaces[0],
                    expectedATextureState)
                && sameTextureState(
                    level.sectors[1].surfaces[0],
                    expectedBTextureState),
            "makeAdjoin: signed multi-tile UVs, colors, scales, and coordinate-space tag remain exact");

        const bool un = sed::level::levelUnadjoinSurface(level, sed::SurfaceRef{ 0, 0 });
        check(un, "levelUnadjoinSurface: returns true");

        check(!level.sectors[0].surfaces[0].adjoin.has_value(), "levelUnadjoinSurface: a.adjoin cleared");
        check(!level.sectors[1].surfaces[0].adjoin.has_value(), "levelUnadjoinSurface: b.adjoin cleared");

        check(level.sectors[0].surfaces[0].surfaceFlags == sed::SF_Collision, "levelUnadjoinSurface: a.surfaceFlags");
        check(level.sectors[0].surfaces[0].faceFlags == 0u, "levelUnadjoinSurface: a.faceFlags");
        check(level.sectors[0].surfaces[0].geo == 4, "levelUnadjoinSurface: a.geo");
        check(level.sectors[0].surfaces[0].material == "dflt.mat", "levelUnadjoinSurface: a.material");
        check(level.sectors[0].surfaces[0].nAdj == -1, "levelUnadjoinSurface: a.nAdj");

        check(level.sectors[1].surfaces[0].surfaceFlags == sed::SF_Collision, "levelUnadjoinSurface: b.surfaceFlags");
        check(level.sectors[1].surfaces[0].faceFlags == 0u, "levelUnadjoinSurface: b.faceFlags");
        check(level.sectors[1].surfaces[0].geo == 4, "levelUnadjoinSurface: b.geo");
        check(level.sectors[1].surfaces[0].material == "dflt.mat", "levelUnadjoinSurface: b.material");
        check(level.sectors[1].surfaces[0].nAdj == -1, "levelUnadjoinSurface: b.nAdj");
        check(sameTextureState(
                    level.sectors[0].surfaces[0],
                    expectedATextureState)
                && sameTextureState(
                    level.sectors[1].surfaces[0],
                    expectedBTextureState),
            "levelUnadjoinSurface: authored texture state remains exact");

        const bool joined = sed::level::levelAdjoinSectors(level, 0, 1);
        check(joined, "adjoinSectors: returns true");
        check(level.sectors[0].surfaces[0].adjoin.has_value(), "adjoinSectors: a.adjoin");
        check(level.sectors[1].surfaces[0].adjoin.has_value(), "adjoinSectors: b.adjoin");

        const bool unjoined = sed::level::levelUnadjoinSectors(level, 0, 1);
        check(unjoined, "levelUnadjoinSectors: returns true");
        check(!level.sectors[0].surfaces[0].adjoin.has_value(), "levelUnadjoinSectors: a.adjoin cleared");
        check(!level.sectors[1].surfaces[0].adjoin.has_value(), "levelUnadjoinSectors: b.adjoin cleared");
        check(sameTextureState(
                    level.sectors[0].surfaces[0],
                    expectedATextureState)
                && sameTextureState(
                    level.sectors[1].surfaces[0],
                    expectedBTextureState),
            "levelAdjoinSectors round trip: authored texture state remains exact");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::JKDF2;
        level.uvCoordinateSpace = sed::UvCoordinateSpace::Normalized;
        level.sectors.resize(2);

        sed::Sector& a = level.sectors[0];
        sed::Sector& b = level.sectors[1];

        a.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 3.0, 0.0, 0.0, 0 },
            { 3.0, 2.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
        };

        b.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
            { 0.0, 2.0, 0.0, 0 },
            { 3.0, 2.0, 0.0, 0 },
            { 3.0, 1.0, 0.0, 0 },
            { 3.0, 0.0, 0.0, 0 },
        };

        sed::Surface sa{};
        sa.vertices = { 0, 1, 2, 3 };
        sa.texVertices = std::vector<sed::TexVertex>(sa.vertices.size());
        a.surfaces.push_back(sa);

        sed::Surface sb{};
        sb.vertices = { 0, 1, 2, 3, 4, 5 };
        sb.texVertices = std::vector<sed::TexVertex>(sb.vertices.size());
        b.surfaces.push_back(sb);

        check(
            sed::level::surfaceDoOverlap(a, a.surfaces[0], b, b.surfaces[0]),
            "surfaceDoOverlap: opposite winding tolerates collinear differences");
    }

    {
        sed::Level level{};
        level.kind = sed::ProjectType::JKDF2;
        level.sectors.resize(2);

        sed::Sector& larger = level.sectors[0];
        sed::Sector& smaller = level.sectors[1];

        larger.vertices = {
            { 0.0, 0.0, 0.0, 0 },
            { 4.0, 0.0, 0.0, 0 },
            { 4.0, 1.0, 0.0, 0 },
            { 0.0, 1.0, 0.0, 0 },
        };

        smaller.vertices = {
            { 1.0, 0.0, 0.0, 0 },
            { 1.0, 1.0, 0.0, 0 },
            { 3.0, 1.0, 0.0, 0 },
            { 3.0, 0.0, 0.0, 0 },
        };

        sed::Surface largeSurface{};
        largeSurface.nMat = 41;
        largeSurface.vertices = { 0, 1, 2, 3 };
        largeSurface.texVertices = {
            { -3.25f, 5.5f, sed::ColorF{ 0.1f, 0.2f, 0.3f, 0.4f } },
            { 7.75f, -11.0f, sed::ColorF{ 0.2f, 0.3f, 0.4f, 0.5f } },
            { 13.25f, 17.5f, sed::ColorF{ 0.3f, 0.4f, 0.5f, 0.6f } },
            { -19.75f, 23.0f, sed::ColorF{ 0.4f, 0.5f, 0.6f, 0.7f } },
        };
        largeSurface.uScale = -0.03125f;
        largeSurface.vScale = 0.078125f;
        larger.surfaces.push_back(largeSurface);

        sed::Surface smallSurface{};
        smallSurface.nMat = 42;
        smallSurface.vertices = { 0, 1, 2, 3 };
        smallSurface.texVertices = {
            { 29.5f, -31.75f, sed::ColorF{ 0.15f, 0.25f, 0.35f, 0.45f } },
            { -37.0f, 41.25f, sed::ColorF{ 0.25f, 0.35f, 0.45f, 0.55f } },
            { 43.5f, -47.75f, sed::ColorF{ 0.35f, 0.45f, 0.55f, 0.65f } },
            { -53.0f, 59.25f, sed::ColorF{ 0.45f, 0.55f, 0.65f, 0.75f } },
        };
        smallSurface.uScale = 0.046875f;
        smallSurface.vScale = -0.109375f;
        smaller.surfaces.push_back(smallSurface);

        const bool connected = sed::level::levelConnectSurfaces(level, sed::SurfaceRef{ 0, 0 }, sed::SurfaceRef{ 1, 0 });
        check(connected, "levelConnectSurfaces: larger face auto-cut");

        std::array<bool, 4> retainedLargeCorners{};
        bool exactTextureState = level.uvCoordinateSpace
            == sed::UvCoordinateSpace::Normalized
            && level.sectors[1].surfaces.size() == 1
            && sameTextureState(
                level.sectors[1].surfaces[0], smallSurface);
        for ( const sed::Surface& fragment : level.sectors[0].surfaces )
        {
            exactTextureState = exactTextureState
                && fragment.uScale == largeSurface.uScale
                && fragment.vScale == largeSurface.vScale
                && fragment.vertices.size() == fragment.texVertices.size();
            for ( std::size_t corner = 0;
                exactTextureState && corner < fragment.vertices.size();
                ++corner )
            {
                const std::int32_t vertexIndex = fragment.vertices[corner];
                if ( vertexIndex < 0 || vertexIndex >= 4 )
                {
                    continue;
                }
                retainedLargeCorners[static_cast<std::size_t>(vertexIndex)] =
                    true;
                const sed::TexVertex& actual = fragment.texVertices[corner];
                const sed::TexVertex& expected =
                    largeSurface.texVertices[
                        static_cast<std::size_t>(vertexIndex)];
                exactTextureState = actual.u == expected.u
                    && actual.v == expected.v
                    && actual.color == expected.color;
            }
        }
        exactTextureState = exactTextureState
            && std::all_of(
                retainedLargeCorners.begin(),
                retainedLargeCorners.end(),
                [](const bool retained)
                {
                    return retained;
                });
        check(exactTextureState,
            "levelConnectSurfaces: auto-cut preserves all authored UV/color tuples, scales, and coordinate-space tag while interpolating only new corners");

        bool foundAdjoin = false;
        for (const sed::Surface& surfaceA : level.sectors[0].surfaces)
        {
            if (!surfaceA.adjoin || surfaceA.adjoin->sector != 1)
            {
                continue;
            }

            const std::size_t otherIndex = static_cast<std::size_t>(surfaceA.adjoin->surface);
            if (otherIndex >= level.sectors[1].surfaces.size())
            {
                continue;
            }

            const sed::Surface& surfaceB = level.sectors[1].surfaces[otherIndex];
            if (!surfaceB.adjoin || surfaceB.adjoin->sector != 0)
            {
                continue;
            }

            check(
                sed::level::surfaceDoOverlap(level.sectors[0], surfaceA, level.sectors[1], surfaceB),
                "levelConnectSurfaces: cut piece overlaps smaller face");
            foundAdjoin = true;
            break;
        }

        check(foundAdjoin, "levelConnectSurfaces: found adjoined cut piece");
    }
}

} // namespace sed::tests
