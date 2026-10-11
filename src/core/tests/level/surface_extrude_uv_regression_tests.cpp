#include "test_common.hpp"

#include "sed/level/build.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

namespace sed::tests
{
namespace
{

bool sameTextureVertex(const TexVertex& first, const TexVertex& second) noexcept
{
    return first.u == second.u
        && first.v == second.v
        && first.color == second.color;
}

bool samePoint(const Vertex& first, const TVector& second) noexcept
{
    return first.x == second.x
        && first.y == second.y
        && first.z == second.z;
}

Level makeTranslatedUvFixture()
{
    Level level{};
    level.kind = ProjectType::JKDF2;
    level.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
    level.layers = { "Surface extrude UV regression" };

    Sector& sector = level.newSector();
    sector.layer = 0;
    const TVector origin{ 12.5, -4.25, 7.75 };
    const std::array<TVector, 4> points{{
        origin,
        origin + TVector{ 2.0, 0.0, 0.0 },
        origin + TVector{ 2.0, 1.5, 0.0 },
        origin + TVector{ 0.0, 1.5, 0.0 },
    }};
    for ( const TVector& point : points )
    {
        sector.newVertex(point.x, point.y, point.z);
    }

    Surface& source = sector.surfaces.emplace_back();
    source.num = 17;
    source.mark = 19;
    source.d3dId = 23;
    source.surfaceFlags = SF_Collision;
    source.faceFlags = FF_SF_FLIP;
    source.geo = static_cast<std::int32_t>(TGeoMode::Texture);
    source.light = static_cast<std::int32_t>(TLightMode::Gouraud);
    source.tex = 29;
    source.extraLight = { 0.11f, 0.22f, 0.33f, 0.44f };
    source.adjoinFlags = SAF_Visible | SAF_Move | SAF_AllowSoundPass;
    source.material = "translated-uv-source.mat";
    source.nAdj = 31;
    source.nMat = 37;
    source.uScale = -0.375f;
    source.vScale = 0.625f;
    source.vertices = { 0, 1, 2, 3 };
    source.texVertices = {
        { -17.25f, 23.5f, { 0.1f, 0.2f, 0.3f, 0.4f } },
        { 41.75f, -9.125f, { 0.2f, 0.3f, 0.4f, 0.5f } },
        { 3.625f, 87.25f, { 0.3f, 0.4f, 0.5f, 0.6f } },
        { -66.5f, 4.75f, { 0.4f, 0.5f, 0.6f, 0.7f } },
    };
    source.normal = { 0.0, 0.0, 1.0 };
    return level;
}

void applyAffineUvFixture(Level& level)
{
    Surface& source = level.sectors[0].surfaces[0];
    source.texVertices = {
        { 29.1875f, -17.9375f, source.texVertices[0].color },
        { 32.1875f, -17.4375f, source.texVertices[1].color },
        { 31.0625f, -14.0625f, source.texVertices[2].color },
        { 28.0625f, -14.5625f, source.texVertices[3].color },
    };
}

TexVertex affineUvAt(const TVector& point, const TexVertex& sourceColor)
{
    return TexVertex{
        static_cast<float>(7.25 + 1.5 * point.x - 0.75 * point.y),
        static_cast<float>(-11.5 + 0.25 * point.x + 2.25 * point.y),
        sourceColor.color};
}

bool capCornersUseSourceUv(
    const Sector& generated,
    const Surface& cap,
    const std::array<TVector, 4>& expectedPositions,
    const std::array<TexVertex, 4>& expectedUv)
{
    if ( cap.vertices.size() != expectedPositions.size()
        || cap.texVertices.size() != expectedUv.size() )
    {
        return false;
    }
    for ( std::size_t corner = 0; corner < expectedPositions.size(); ++corner )
    {
        const std::int32_t vertexIndex = cap.vertices[corner];
        if ( vertexIndex < 0
            || static_cast<std::size_t>(vertexIndex) >= generated.vertices.size()
            || !samePoint(generated.vertices[static_cast<std::size_t>(vertexIndex)], expectedPositions[corner])
            || !sameTextureVertex(cap.texVertices[corner], expectedUv[corner]) )
        {
            return false;
        }
    }
    return true;
}

bool generatedSurfaceParamsMatch(const Surface& actual, const Surface& source) noexcept
{
    return actual.material == source.material
        && (actual.surfaceFlags & ~SF_Floor)
            == (source.surfaceFlags & ~SF_Floor)
        && (actual.surfaceFlags & SF_Collision) != 0
        && actual.faceFlags == source.faceFlags
        && actual.geo == source.geo
        && actual.light == source.light
        && actual.tex == source.tex
        && actual.extraLight == source.extraLight
        && actual.adjoinFlags == source.adjoinFlags
        && actual.nMat == source.nMat
        && actual.uScale == source.uScale
        && actual.vScale == source.vScale;
}

} // namespace

void runSurfaceExtrudeUvRegressionTests()
{
    constexpr SurfaceRef sourceRef{ 0, 0 };
    constexpr double length = 0.75;
    const Level original = makeTranslatedUvFixture();
    Level level = original;
    const Surface source = original.sectors[0].surfaces[0];
    const std::array<TVector, 4> sourcePositions{{
        { 12.5, -4.25, 7.75 },
        { 14.5, -4.25, 7.75 },
        { 14.5, -2.75, 7.75 },
        { 12.5, -2.75, 7.75 },
    }};
    const std::array<TexVertex, 4> sourceUv = {
        source.texVertices[0],
        source.texVertices[1],
        source.texVertices[2],
        source.texVertices[3],
    };
    const std::array<TVector, 4> expectedInner{{
        sourcePositions[3], sourcePositions[2], sourcePositions[1], sourcePositions[0] }};
    const std::array<TVector, 4> expectedOuter{{
        sourcePositions[0] + TVector{ 0.0, 0.0, -length },
        sourcePositions[1] + TVector{ 0.0, 0.0, -length },
        sourcePositions[2] + TVector{ 0.0, 0.0, -length },
        sourcePositions[3] + TVector{ 0.0, 0.0, -length },
    }};
    const std::array<TexVertex, 4> expectedInnerUv{{
        sourceUv[3], sourceUv[2], sourceUv[1], sourceUv[0] }};

    level::SurfaceExtrudeReport report{};
    const bool accepted = level::levelExtrudeSurface(
        level, sourceRef, length, 0.0, &report);
    check(accepted && report.error == level::SurfaceExtrudeError::None
            && report.surface == sourceRef
            && level.sectors.size() == 2
            && level.sectors[1].surfaces.size() == 6,
        "surface extrude UV RED fixture: translated plain extrusion publishes a complete generated sector");
    check(level.uvCoordinateSpace == original.uvCoordinateSpace,
        "surface extrude UV RED fixture: Level UV coordinate-space tag remains unchanged");
    check(level.sectors[0].surfaces[0].texVertices.size() == source.texVertices.size()
            && std::equal(
                level.sectors[0].surfaces[0].texVertices.begin(),
                level.sectors[0].surfaces[0].texVertices.end(),
                source.texVertices.begin(),
                [](const TexVertex& first, const TexVertex& second)
                {
                    return sameTextureVertex(first, second);
                })
            && level.sectors[0].surfaces[0].uScale == source.uScale
            && level.sectors[0].surfaces[0].vScale == source.vScale,
        "surface extrude UV RED fixture: authored source UV corners and signed scales remain exact");

    if ( !accepted || level.sectors.size() != 2
        || level.sectors[1].surfaces.size() != 6 )
    {
        return;
    }

    const Sector& generated = level.sectors[1];
    check(capCornersUseSourceUv(
              generated, generated.surfaces[0], expectedInner,
              expectedInnerUv),
        "surface extrude UV RED fixture: translated inner cap keeps every source corner UV/RGBA with reversed winding");
    check(capCornersUseSourceUv(
              generated, generated.surfaces[1], expectedOuter,
              sourceUv),
        "surface extrude UV RED fixture: translated outer cap keeps every source corner UV/RGBA and exact position");
    bool generatedParams = true;
    for ( std::size_t faceIndex = 1; faceIndex < generated.surfaces.size(); ++faceIndex )
    {
        generatedParams = generatedParams
            && generatedSurfaceParamsMatch(generated.surfaces[faceIndex], source);
    }
    check(generatedParams,
        "surface extrude UV RED fixture: generated caps and side walls retain source mapping metadata and scales");

    for ( const ProjectType kind : { ProjectType::IJIM, ProjectType::JKDF2 } )
    {
        Level expanded = makeTranslatedUvFixture();
        expanded.kind = kind;
        expanded.uvCoordinateSpace = kind == ProjectType::IJIM
            ? UvCoordinateSpace::Normalized
            : UvCoordinateSpace::TexturePixels;
        const UvCoordinateSpace expectedSpace = expanded.uvCoordinateSpace;
        applyAffineUvFixture(expanded);
        const Surface expandedSource = expanded.sectors[0].surfaces[0];
        bool fixedAffineInputs = expandedSource.texVertices.size() == 4;
        for ( std::size_t corner = 0; fixedAffineInputs && corner < 4; ++corner )
        {
            const Vertex& point = expanded.sectors[0].vertices[
                static_cast<std::size_t>(expandedSource.vertices[corner])];
            const TexVertex expected = affineUvAt(
                TVector{point.x, point.y, point.z}, expandedSource.texVertices[corner]);
            fixedAffineInputs = expandedSource.texVertices[corner].u == expected.u
                && expandedSource.texVertices[corner].v == expected.v;
        }
        check(fixedAffineInputs,
            "surface extrude UV RED fixture: fixed source vectors match the independent affine UV oracle");
        level::SurfaceExtrudeReport expandedReport{};
        const bool expandedAccepted = level::levelExtrudeSurface(
            expanded, sourceRef, length, 0.25, &expandedReport);
        check(expandedAccepted && expandedReport.error == level::SurfaceExtrudeError::None
                && expanded.sectors.size() == 2,
            "surface extrude UV RED fixture: affine expanded extrusion succeeds in normalized and pixel UV spaces");
        check(expanded.uvCoordinateSpace == expectedSpace,
            "surface extrude UV RED fixture: expanded extrusion preserves the explicit UV coordinate-space tag");
        if ( !expandedAccepted || expanded.sectors.size() != 2 )
        {
            continue;
        }

        bool sourceExact = expanded.sectors[0].surfaces[0].texVertices.size()
            == expandedSource.texVertices.size();
        for ( std::size_t corner = 0; sourceExact && corner < expandedSource.texVertices.size(); ++corner )
        {
            sourceExact = sameTextureVertex(
                expanded.sectors[0].surfaces[0].texVertices[corner],
                expandedSource.texVertices[corner]);
        }
        check(sourceExact,
            "surface extrude UV RED fixture: expanded source preserves affine UV and RGBA tuples exactly");

        const Sector& expandedSector = expanded.sectors[1];
        bool outerAffine = expandedSector.surfaces.size() > 1
            && expandedSector.surfaces[1].vertices.size() == 4
            && expandedSector.surfaces[1].texVertices.size() == 4;
        if ( outerAffine )
        {
            const Surface& outer = expandedSector.surfaces[1];
            for ( std::size_t corner = 0; corner < 4; ++corner )
            {
                const TVector point = TVector{
                    expandedSector.vertices[static_cast<std::size_t>(outer.vertices[corner])].x,
                    expandedSector.vertices[static_cast<std::size_t>(outer.vertices[corner])].y,
                    expandedSector.vertices[static_cast<std::size_t>(outer.vertices[corner])].z};
                const TexVertex expected = affineUvAt(
                    point, outer.texVertices[corner]);
                outerAffine = outerAffine
                    && outer.texVertices[corner].u == expected.u
                    && outer.texVertices[corner].v == expected.v;
            }
        }
        check(outerAffine,
            "surface extrude UV RED fixture: expanded outer cap retains affine source UV orientation and position");

        bool sourceCornersRetained = true;
        for ( std::size_t corner = 0; corner < sourcePositions.size(); ++corner )
        {
            bool found = false;
            for ( const Surface& candidate : expandedSector.surfaces )
            {
                for ( std::size_t pointIndex = 0;
                      pointIndex < candidate.vertices.size()
                      && pointIndex < candidate.texVertices.size(); ++pointIndex )
                {
                    const Vertex& point = expandedSector.vertices[
                        static_cast<std::size_t>(candidate.vertices[pointIndex])];
                    if ( point.x == sourcePositions[corner].x
                        && point.y == sourcePositions[corner].y
                        && point.z == sourcePositions[corner].z
                        && sameTextureVertex(
                            candidate.texVertices[pointIndex],
                            expandedSource.texVertices[corner]) )
                    {
                        found = true;
                    }
                }
            }
            sourceCornersRetained = sourceCornersRetained && found;
        }
        check(sourceCornersRetained,
            "surface extrude UV RED fixture: expanded inner cap retains each corresponding source UV/RGBA corner");
    }

    {
        Level collinear = makeTranslatedUvFixture();
        Sector& collinearSector = collinear.sectors[0];
        Surface& collinearSurface = collinearSector.surfaces[0];
        collinearSector.vertices.push_back(Vertex{ 13.5, -4.25, 7.75, 4 });
        collinearSurface.vertices = { 0, 4, 1, 2, 3 };
        collinearSurface.texVertices.insert(
            collinearSurface.texVertices.begin() + 1,
            TexVertex{ 12.25f, 7.1875f, collinearSurface.texVertices[0].color });
        level::SurfaceExtrudeReport collinearReport{};
        check(level::levelExtrudeSurface(collinear, sourceRef, length, 0.25, &collinearReport)
                && collinearReport.error == level::SurfaceExtrudeError::None,
            "surface extrude UV regression accepts a valid source whose first three corners are collinear");
    }

    {
        Level nonAffine = makeTranslatedUvFixture();
        applyAffineUvFixture(nonAffine);
        nonAffine.sectors[0].surfaces[0].texVertices[2].u += 3.75f;
        const Surface before = nonAffine.sectors[0].surfaces[0];
        level::SurfaceExtrudeReport nonAffineReport{};
        const bool accepted = level::levelExtrudeSurface(
            nonAffine, sourceRef, length, 0.25, &nonAffineReport);
        bool generatedCorner = accepted && nonAffine.sectors.size() == 2;
        if (generatedCorner)
        {
            const Sector& generated = nonAffine.sectors[1];
            const TVector expectedPoint = TVector{
                nonAffine.sectors[0].vertices[2].x,
                nonAffine.sectors[0].vertices[2].y,
                nonAffine.sectors[0].vertices[2].z};
            generatedCorner = std::any_of(
                generated.surfaces.begin(), generated.surfaces.end(),
                [&](const Surface& candidate)
                {
                    for (std::size_t index = 0;
                         index < candidate.vertices.size()
                         && index < candidate.texVertices.size(); ++index)
                    {
                        const Vertex& point = generated.vertices[
                            static_cast<std::size_t>(candidate.vertices[index])];
                        if (point.x == expectedPoint.x && point.y == expectedPoint.y
                            && point.z == expectedPoint.z
                            && sameTextureVertex(candidate.texVertices[index], before.texVertices[2]))
                        {
                            return true;
                        }
                    }
                    return false;
                });
        }
        check(generatedCorner && sameTextureVertex(
                nonAffine.sectors[0].surfaces[0].texVertices[2], before.texVertices[2]),
            "surface extrude UV regression preserves exact non-affine authored corners");
    }

    {
        Level overflow = makeTranslatedUvFixture();
        applyAffineUvFixture(overflow);
        overflow.sectors[0].surfaces[0].texVertices[0].u = 0.0f;
        overflow.sectors[0].surfaces[0].texVertices[1].u = std::numeric_limits<float>::max();
        overflow.sectors[0].surfaces[0].texVertices[2].u = 0.0f;
        overflow.sectors[0].surfaces[0].texVertices[3].u = 0.0f;
        const double overflowOracle = static_cast<double>(std::numeric_limits<float>::max())
            * (2.25 / 2.0 + 0.25 / 1.5);
        check(!std::isfinite(overflowOracle)
                || overflowOracle > static_cast<double>(std::numeric_limits<float>::max()),
            "surface extrude UV regression overflow oracle exceeds float range at an expanded corner");
        level::SurfaceExtrudeReport overflowReport{};
        const bool overflowAccepted = level::levelExtrudeSurface(
            overflow, sourceRef, length, 0.25, &overflowReport);
        bool overflowFinite = overflowAccepted && overflow.sectors.size() == 2;
        if (overflowFinite)
        {
            for (const TexVertex& vertex : overflow.sectors[1].surfaces[1].texVertices)
            {
                overflowFinite = overflowFinite && std::isfinite(vertex.u)
                    && std::isfinite(vertex.v);
            }
        }
        bool overflowGeometric = overflowFinite;
        if (overflowGeometric)
        {
            Sector geometric = overflow.sectors[1];
            level::surfaceRecalcNewAll(
                level::levelPixelPerUnit(overflow), geometric, geometric.surfaces[1], 0);
            const auto& actual = overflow.sectors[1].surfaces[1].texVertices;
            const auto& expected = geometric.surfaces[1].texVertices;
            overflowGeometric = actual.size() == expected.size();
            for (std::size_t index = 0; overflowGeometric && index < actual.size(); ++index)
            {
                overflowGeometric = actual[index].u == expected[index].u
                    && actual[index].v == expected[index].v;
            }
        }
        check(overflowGeometric,
            "surface extrude UV regression falls back when expanded projection is unrepresentable");
    }
}

} // namespace sed::tests
