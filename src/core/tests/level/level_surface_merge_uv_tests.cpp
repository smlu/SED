#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/merge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::tests
{

namespace
{

struct LocalPoint
{
    double x{};
    double y{};
};

constexpr double kTinyLeadingEdge = 0x1p-27;
constexpr std::array<LocalPoint, 9> kMergePoints{{
    { kTinyLeadingEdge, 1.0 },
    { 0.0, 1.0 },
    { 0.0, 0.0 },
    { 1.0, 0.0 },
    { 1.0, 1.0 },
    { 2.0, 0.0 },
    { 2.0, 1.0 },
    { 3.0, 0.0 },
    { 3.0, 1.0 },
}};

constexpr std::array<std::array<std::size_t, 5>, 3> kFacePoints{{
    { 0, 1, 2, 3, 4 },
    { 3, 5, 6, 4, 4 },
    { 5, 7, 8, 6, 6 },
}};

constexpr std::array<std::size_t, 3> kFaceCornerCounts{ 5, 4, 4 };

struct UvDimensions
{
    float u{ 1.0f };
    float v{ 1.0f };
};

struct MergeFixture
{
    sed::Level level{};
    std::size_t faceCount{};
    bool mirrored{};
    bool noisyLeadingV{};
};

sed::Vertex worldPoint(const LocalPoint point)
{
    // Keep the same convex source shape while exercising an oblique plane and
    // a translated origin.  Its affine UV map still has an independent oracle
    // in the local coordinates above.
    return sed::Vertex{
        17.0 + 2.0 * point.x - point.y,
        -9.0 + point.x + 4.0 * point.y,
        3.0 + 3.0 * point.x + 2.0 * point.y,
    };
}

std::pair<double, double> analyticUv(
    const LocalPoint point,
    const std::size_t side,
    const UvDimensions dimensions)
{
    if ( side == 0 )
    {
        return {
            static_cast<double>(dimensions.u) * (1.0 + point.x),
            static_cast<double>(dimensions.v) * point.y,
        };
    }

    // The second portal owner deliberately has a separate mirrored affine
    // frame, including a negative UV orientation.
    return {
        static_cast<double>(dimensions.u) * (3.25 - 0.75 * point.x + 0.5 * point.y),
        static_cast<double>(dimensions.v) * (-1.5 + 0.25 * point.x + 1.25 * point.y),
    };
}

sed::ColorF authoredColor(const std::size_t point, const std::size_t side)
{
    const float index = static_cast<float>(point);
    const float owner = static_cast<float>(side);
    return {
        0.11f + index * 0.061f + owner * 0.07f,
        0.17f + index * 0.037f + owner * 0.03f,
        0.23f + index * 0.029f,
        0.41f + index * 0.021f,
    };
}

sed::TexVertex authoredTexVertex(
    const std::size_t point,
    const std::size_t side,
    const UvDimensions dimensions,
    const bool noisyLeadingV)
{
    const auto [u, v] = analyticUv(kMergePoints[point], side, dimensions);
    float storedV = static_cast<float>(v);
    if ( noisyLeadingV && side == 0 && point == 1 )
    {
        // One representable float step is ordinary authored rounding, but a
        // triangle with a 2^-27 leading edge must not turn it into a large
        // world-space gradient.
        storedV = std::nextafter(
            storedV,
            std::numeric_limits<float>::infinity());
    }
    return {
        static_cast<float>(u),
        storedV,
        authoredColor(point, side),
    };
}

std::pair<float, float> survivorScales(
    const std::size_t side,
    const UvDimensions dimensions)
{
    if ( side == 0 )
    {
        return { -0.03125f * dimensions.u, 0.09375f * dimensions.v };
    }
    return { 0.0625f * dimensions.u, -0.125f * dimensions.v };
}

std::pair<float, float> donorScales(
    const std::size_t side,
    const std::size_t face,
    const UvDimensions dimensions)
{
    const float faceFactor = static_cast<float>(face);
    const float ownerFactor = static_cast<float>(side + 1);
    return {
        (face % 2 == 0 ? -0.375f : 0.25f) * ownerFactor * dimensions.u,
        (face % 2 == 0 ? 0.1875f : -0.5f) * ownerFactor * dimensions.v
            + faceFactor * 0.0f,
    };
}

MergeFixture makeMergeFixture(
    const sed::UvCoordinateSpace coordinateSpace,
    const UvDimensions dimensions,
    const std::size_t faceCount,
    const bool mirrored,
    const bool noisyLeadingV)
{
    MergeFixture fixture{};
    fixture.faceCount = faceCount;
    fixture.mirrored = mirrored;
    fixture.noisyLeadingV = noisyLeadingV;
    fixture.level.uvCoordinateSpace = coordinateSpace;

    const std::size_t sectorCount = mirrored ? 2 : 1;
    std::array<std::array<std::int32_t, kMergePoints.size()>, 2> vertexIds{};
    for ( std::size_t side = 0; side < sectorCount; ++side )
    {
        sed::Sector& sector = fixture.level.newSector();
        for ( std::size_t point = 0; point < kMergePoints.size(); ++point )
        {
            const sed::Vertex position = worldPoint(kMergePoints[point]);
            // newVertex is intentional: the distinct epsilon corner must not
            // be collapsed by a convenience vertex-welding path.
            vertexIds[side][point] = sector.newVertex(
                position.x,
                position.y,
                position.z);
        }

        for ( std::size_t faceIndex = 0; faceIndex < faceCount; ++faceIndex )
        {
            std::vector<std::size_t> points(
                kFacePoints[faceIndex].begin(),
                kFacePoints[faceIndex].begin()
                    + static_cast<std::ptrdiff_t>(kFaceCornerCounts[faceIndex]));
            if ( side == 1 )
            {
                std::reverse(points.begin(), points.end());
            }

            sed::Surface surface{};
            surface.vertices.reserve(points.size());
            surface.texVertices.reserve(points.size());
            for ( const std::size_t point : points )
            {
                surface.vertices.push_back(vertexIds[side][point]);
                surface.texVertices.push_back(authoredTexVertex(
                    point,
                    side,
                    dimensions,
                    noisyLeadingV));
            }
            const auto [uScale, vScale] = faceIndex == 0
                ? survivorScales(side, dimensions)
                : donorScales(side, faceIndex, dimensions);
            surface.uScale = uScale;
            surface.vScale = vScale;
            surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
            (void)sector.addSurface(std::move(surface));
        }
    }

    if ( mirrored )
    {
        for ( std::size_t face = 0; face < faceCount; ++face )
        {
            (void)sed::level::levelSysAdjoinSurfaces(
                fixture.level,
                { 0, static_cast<std::int32_t>(face) },
                { 1, static_cast<std::int32_t>(face) });
        }
    }
    return fixture;
}

bool sameTexVertex(const sed::TexVertex& lhs, const sed::TexVertex& rhs)
{
    return lhs.u == rhs.u
        && lhs.v == rhs.v
        && lhs.color == rhs.color;
}

std::size_t pointAtPosition(const sed::Vertex& vertex)
{
    for ( std::size_t point = 0; point < kMergePoints.size(); ++point )
    {
        const sed::Vertex expected = worldPoint(kMergePoints[point]);
        if ( vertex.x == expected.x
            && vertex.y == expected.y
            && vertex.z == expected.z )
        {
            return point;
        }
    }
    return kMergePoints.size();
}

double floatUlpAt(const double magnitude)
{
    const float reference = static_cast<float>(std::max(1.0, magnitude));
    return static_cast<double>(std::nextafter(
        reference,
        std::numeric_limits<float>::infinity())) - reference;
}

double analyticUvScale(
    const std::size_t side,
    const UvDimensions dimensions,
    const bool uAxis)
{
    double maximum = 0.0;
    for ( const LocalPoint point : kMergePoints )
    {
        const auto [u, v] = analyticUv(point, side, dimensions);
        maximum = std::max(maximum, std::abs(uAxis ? u : v));
    }
    return maximum;
}

void checkMergedSurfaceUv(
    const sed::Level& level,
    const std::size_t side,
    const UvDimensions dimensions,
    const std::size_t expectedPointCount,
    const bool noisyLeadingV,
    const bool expectAdjoin,
    const std::string_view label)
{
    const auto namedCheck = [label](const bool ok, const std::string_view detail)
    {
        const std::string message = std::string(label) + "." + std::string(detail);
        check(ok, message.c_str());
    };

    if ( side >= level.sectors.size() )
    {
        namedCheck(false, "sectorExists");
        return;
    }
    const sed::Sector& sector = level.sectors[side];
    namedCheck(sector.surfaces.size() == 1, "oneMergedSurface");
    if ( sector.surfaces.size() != 1 )
    {
        return;
    }

    const sed::Surface& surface = sector.surfaces[0];
    namedCheck(surface.vertices.size() == expectedPointCount, "outerBoundaryCornerCount");
    namedCheck(surface.texVertices.size() == surface.vertices.size(), "completeTextureBoundary");
    if ( surface.texVertices.size() != surface.vertices.size() )
    {
        return;
    }

    const auto [expectedUScale, expectedVScale] = survivorScales(side, dimensions);
    namedCheck(surface.uScale == expectedUScale, "survivorUScaleExact");
    namedCheck(surface.vScale == expectedVScale, "survivorVScaleExact");
    namedCheck(
        expectAdjoin
            ? surface.adjoin == std::optional<sed::SurfaceRef>{
                sed::SurfaceRef{ static_cast<std::int32_t>(1 - side), 0 }}
            : !surface.adjoin,
        "portalState");

    std::array<bool, kMergePoints.size()> seen{};
    const double uTolerance = 4.0 * floatUlpAt(analyticUvScale(side, dimensions, true));
    const double vTolerance = 4.0 * floatUlpAt(analyticUvScale(side, dimensions, false));
    for ( std::size_t corner = 0; corner < surface.vertices.size(); ++corner )
    {
        const std::int32_t vertexIndex = surface.vertices[corner];
        const bool validVertex = vertexIndex >= 0
            && static_cast<std::size_t>(vertexIndex) < sector.vertices.size();
        namedCheck(validVertex, "outputVertexReferenceValid");
        if ( !validVertex )
        {
            continue;
        }

        const std::size_t point = pointAtPosition(
            sector.vertices[static_cast<std::size_t>(vertexIndex)]);
        namedCheck(point < kMergePoints.size(), "onlyExpectedSourcePositions");
        if ( point >= kMergePoints.size() )
        {
            continue;
        }
        seen[point] = true;

        const auto [expectedU, expectedV] = analyticUv(kMergePoints[point], side, dimensions);
        const sed::TexVertex& textureVertex = surface.texVertices[corner];
        namedCheck(
            std::abs(static_cast<double>(textureVertex.u) - expectedU) <= uTolerance,
            "analyticUWithinFourStoredFloatUlps");
        namedCheck(
            std::abs(static_cast<double>(textureVertex.v) - expectedV) <= vTolerance,
            "analyticVWithinFourStoredFloatUlps");
        namedCheck(
            textureVertex.color == authoredColor(point, side),
            "sourceAndDonorColorsExact");
    }

    for ( std::size_t point = 0; point < expectedPointCount; ++point )
    {
        namedCheck(seen[point], "completeExpectedOuterBoundary");
    }

    // The first face owns the surviving frame. Every one of its original
    // corners, including the tiny leading edge and its baked color, must stay
    // bit-exact even though a better-spread triangle supplies the projection
    // used only for newly contributed corners.
    constexpr std::array<std::size_t, 5> sourcePoints{ 0, 1, 2, 3, 4 };
    for ( const std::size_t point : sourcePoints )
    {
        const auto outputCorner = std::find_if(
            surface.vertices.begin(),
            surface.vertices.end(),
            [&sector, point](const std::int32_t vertexIndex)
            {
                return vertexIndex >= 0
                    && static_cast<std::size_t>(vertexIndex) < sector.vertices.size()
                    && pointAtPosition(sector.vertices[static_cast<std::size_t>(vertexIndex)]) == point;
            });
        namedCheck(outputCorner != surface.vertices.end(), "retainedSourceCornerPresent");
        if ( outputCorner == surface.vertices.end() )
        {
            continue;
        }
        const std::size_t corner = static_cast<std::size_t>(
            std::distance(surface.vertices.begin(), outputCorner));
        namedCheck(
            sameTexVertex(
                surface.texVertices[corner],
                authoredTexVertex(point, side, dimensions, noisyLeadingV)),
            "retainedSourceUvAndColorExact");
    }
}

std::vector<std::byte> encodeLevel(const sed::Level& level)
{
    return sed::assets::saveToSedBytes(
        level,
        {},
        sed::assets::SedSerializationProfile::ProtectedV1,
        0);
}

void addSelectedSurfaces(
    sed::editor::Selection& selection,
    const std::span<const sed::SurfaceRef> surfaces)
{
    for ( const sed::SurfaceRef surface : surfaces )
    {
        selection.surfaces.insert(surface);
    }
}

std::vector<sed::SurfaceRef> surfaceSelection(const std::size_t count, const bool reverse)
{
    std::vector<sed::SurfaceRef> result;
    result.reserve(count);
    for ( std::size_t index = 0; index < count; ++index )
    {
        const std::size_t surface = reverse ? count - index - 1 : index;
        result.push_back({ 0, static_cast<std::int32_t>(surface) });
    }
    return result;
}

void testSingleMerge(
    const sed::UvCoordinateSpace coordinateSpace,
    const UvDimensions dimensions,
    const bool noisyLeadingV)
{
    MergeFixture fixture = makeMergeFixture(
        coordinateSpace,
        dimensions,
        2,
        false,
        noisyLeadingV);
    const auto merged = sed::level::levelMergeSurfaces(
        fixture.level,
        { 0, 0 },
        { 0, 1 });
    check(merged == std::optional<sed::SurfaceRef>{ sed::SurfaceRef{ 0, 0 } },
        "tinyLeadingSurfaceMerge.single.acceptsAndKeepsLowerIndexSurvivor");
    check(fixture.level.uvCoordinateSpace == coordinateSpace,
        "tinyLeadingSurfaceMerge.single.preservesCoordinateSpaceTag");
    checkMergedSurfaceUv(
        fixture.level,
        0,
        dimensions,
        7,
        noisyLeadingV,
        false,
        "tinyLeadingSurfaceMerge.single");
}

void testBatchMergeAndHistory(
    const sed::UvCoordinateSpace coordinateSpace,
    const UvDimensions dimensions,
    const bool noisyLeadingV)
{
    MergeFixture fixture = makeMergeFixture(
        coordinateSpace,
        dimensions,
        3,
        false,
        noisyLeadingV);
    const auto before = encodeLevel(fixture.level);
    const std::vector<sed::SurfaceRef> reversedSelection = surfaceSelection(3, true);

    sed::editor::DocumentUndoHistory history{};
    history.current().level = fixture.level;
    addSelectedSurfaces(history.current().selection, reversedSelection);
    const auto originalSelection = history.current().selection.surfaces;

    const std::uint32_t structuralDirtyFlags =
        static_cast<std::uint32_t>(sed::editor::DocumentDirty::Structure)
        | static_cast<std::uint32_t>(sed::editor::DocumentDirty::LevelGeometry);
    const bool committed = history.apply(
        "tiny-leading surface merge",
        structuralDirtyFlags,
        [&reversedSelection](sed::editor::DocumentState& state)
        {
            const auto merged = sed::level::levelMergeSelectedSurfaces(
                state.level,
                reversedSelection);
            if ( !merged )
            {
                return false;
            }
            state.selection.surfaces.clear();
            addSelectedSurfaces(state.selection, merged->selection);
            return true;
        });
    check(committed, "tinyLeadingSurfaceMerge.batch.commits");
    if ( !committed )
    {
        return;
    }

    const auto after = encodeLevel(history.current().level);
    check(history.current().level.uvCoordinateSpace == coordinateSpace,
        "tinyLeadingSurfaceMerge.batch.preservesCoordinateSpaceTag");
    check(history.current().level.sectors[0].surfaces.size() == 1,
        "tinyLeadingSurfaceMerge.batch.reachesTwoMergeFixedPoint");
    check(
        history.current().selection.surfaces
            == std::unordered_set<sed::SurfaceRef, sed::editor::SurfaceRefHash,
                sed::editor::SurfaceRefEq>{ sed::SurfaceRef{ 0, 0 } },
        "tinyLeadingSurfaceMerge.batch.remapsSelectionToSurvivor");
    checkMergedSurfaceUv(
        history.current().level,
        0,
        dimensions,
        9,
        noisyLeadingV,
        false,
        "tinyLeadingSurfaceMerge.batch");

    MergeFixture ordered = makeMergeFixture(
        coordinateSpace,
        dimensions,
        3,
        false,
        noisyLeadingV);
    const std::vector<sed::SurfaceRef> forwardSelection = surfaceSelection(3, false);
    const auto orderedResult = sed::level::levelMergeSelectedSurfaces(
        ordered.level,
        forwardSelection);
    check(orderedResult && orderedResult->mergeCount == 2
            && orderedResult->selection == std::vector<sed::SurfaceRef>{ { 0, 0 } }
            && encodeLevel(ordered.level) == after,
        "tinyLeadingSurfaceMerge.batch.inputOrderDoesNotChangeResult");

    check(history.undo(), "tinyLeadingSurfaceMerge.history.undoSucceeds");
    check(encodeLevel(history.current().level) == before,
        "tinyLeadingSurfaceMerge.history.undoRestoresExactSerializedLevel");
    check(history.current().selection.surfaces == originalSelection,
        "tinyLeadingSurfaceMerge.history.undoRestoresOriginalSelection");

    check(history.redo(), "tinyLeadingSurfaceMerge.history.redoSucceeds");
    check(encodeLevel(history.current().level) == after,
        "tinyLeadingSurfaceMerge.history.redoRestoresExactSerializedMergedLevel");
    check(
        history.current().selection.surfaces
            == std::unordered_set<sed::SurfaceRef, sed::editor::SurfaceRefHash,
                sed::editor::SurfaceRefEq>{ sed::SurfaceRef{ 0, 0 } },
        "tinyLeadingSurfaceMerge.history.redoRestoresMergedSelection");
}

void testMirroredAdjoinBatch(
    const sed::UvCoordinateSpace coordinateSpace,
    const UvDimensions dimensions)
{
    MergeFixture fixture = makeMergeFixture(
        coordinateSpace,
        dimensions,
        3,
        true,
        true);
    const auto result = sed::level::levelMergeSelectedSurfaces(
        fixture.level,
        surfaceSelection(3, true));
    check(result && result->mergeCount == 2
            && result->selection == std::vector<sed::SurfaceRef>{ { 0, 0 } },
        "tinyLeadingSurfaceMerge.mirroredBatch.reachesFixedPoint");
    check(fixture.level.uvCoordinateSpace == coordinateSpace,
        "tinyLeadingSurfaceMerge.mirroredBatch.preservesCoordinateSpaceTag");
    check(fixture.level.sectors.size() == 2,
        "tinyLeadingSurfaceMerge.mirroredBatch.preservesBothPortalOwners");
    if ( fixture.level.sectors.size() != 2 )
    {
        return;
    }

    checkMergedSurfaceUv(
        fixture.level,
        0,
        dimensions,
        9,
        true,
        true,
        "tinyLeadingSurfaceMerge.mirroredBatch.survivorOwner");
    checkMergedSurfaceUv(
        fixture.level,
        1,
        dimensions,
        9,
        true,
        true,
        "tinyLeadingSurfaceMerge.mirroredBatch.mirroredOwner");
    check(fixture.level.sectors[0].surfaces.size() == 1
            && fixture.level.sectors[1].surfaces.size() == 1
            && fixture.level.sectors[0].surfaces[0].adjoin
            && fixture.level.sectors[1].surfaces[0].adjoin
            && *fixture.level.sectors[0].surfaces[0].adjoin == sed::SurfaceRef{ 1, 0 }
            && *fixture.level.sectors[1].surfaces[0].adjoin == sed::SurfaceRef{ 0, 0 },
        "tinyLeadingSurfaceMerge.mirroredBatch.keepsReciprocalAdjoins");
}

} // namespace

void runLevelSurfaceMergeUvRegressionTests()
{
    constexpr UvDimensions normalizedDimensions{ 1.0f, 1.0f };
    constexpr UvDimensions anisotropicPixelDimensions{ 512.0f, 128.0f };
    for ( const auto [coordinateSpace, dimensions] : {
            std::pair{ sed::UvCoordinateSpace::Normalized, normalizedDimensions },
            std::pair{ sed::UvCoordinateSpace::TexturePixels, anisotropicPixelDimensions } } )
    {
        testSingleMerge(coordinateSpace, dimensions, false);
        testSingleMerge(coordinateSpace, dimensions, true);
        testBatchMergeAndHistory(coordinateSpace, dimensions, false);
        testBatchMergeAndHistory(coordinateSpace, dimensions, true);
        testMirroredAdjoinBatch(coordinateSpace, dimensions);
    }
}

} // namespace sed::tests
