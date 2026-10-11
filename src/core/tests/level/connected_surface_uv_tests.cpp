#include "test_common.hpp"

#include "sed/editor/document.hpp"
#include "sed/level/transform.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <vector>

namespace sed::tests
{
namespace
{

Surface makeSurface(const std::initializer_list<std::int32_t> vertices,
                    const std::initializer_list<std::array<float, 2>> uvs)
{
    Surface surface{};
    surface.vertices.assign(vertices);
    for ( const std::array<float, 2>& uv : uvs )
    {
        surface.texVertices.push_back(TexVertex{uv[0], uv[1], ColorF{0.25f, 0.5f, 0.75f, 0.625f}});
    }
    return surface;
}

Level makeConnectedUvFixture(const UvCoordinateSpace coordinateSpace)
{
    const float units = coordinateSpace == UvCoordinateSpace::Normalized ? 1.0f : 256.0f;

    Level level{};
    level.kind = ProjectType::JKDF2;
    level.uvCoordinateSpace = coordinateSpace;
    level.sectors.resize(2);

    Sector& source = level.sectors[0];
    source.vertices = {
        Vertex{0.0, 0.0, 0.0}, Vertex{1.0, 0.0, 0.0}, Vertex{1.0, 1.0, 0.0},
        Vertex{0.0, 1.0, 0.0}, Vertex{1.0, 0.0, 1.0}, Vertex{0.0, 0.0, 1.0},
    };
    source.surfaces = {
        makeSurface({0, 1, 2, 3}, {{0.0f, 0.0f}, {units, 0.0f}, {units, units}, {0.0f, units}}),
        makeSurface({0, 1, 4, 5}, {{0.0f, 0.0f}, {units, 0.0f}, {units, units}, {0.0f, units}}),
    };

    Sector& mate = level.sectors[1];
    mate.vertices = {
        Vertex{0.0, 0.0, 0.0}, Vertex{0.0, 1.0, 0.0},  Vertex{1.0, 1.0, 0.0},
        Vertex{1.0, 0.0, 0.0}, Vertex{0.0, 0.0, -1.0}, Vertex{1.0, 0.0, -1.0},
    };
    mate.surfaces = {
        makeSurface({0, 1, 2, 3}, {{units, 0.0f}, {units, units}, {0.0f, units}, {0.0f, 0.0f}}),
        makeSurface({0, 3, 5, 4}, {{0.0f, 0.0f}, {units, 0.0f}, {units, units}, {0.0f, units}}),
    };

    source.surfaces[0].adjoin = SurfaceRef{1, 0};
    mate.surfaces[0].adjoin = SurfaceRef{0, 0};
    return level;
}

bool sameFloatBits(const float lhs, const float rhs) noexcept
{
    return std::bit_cast<std::uint32_t>(lhs) == std::bit_cast<std::uint32_t>(rhs);
}

bool sameUvBits(const Surface& lhs, const Surface& rhs) noexcept
{
    if ( lhs.texVertices.size() != rhs.texVertices.size() )
    {
        return false;
    }

    for ( std::size_t corner = 0; corner < lhs.texVertices.size(); ++corner )
    {
        const TexVertex& left = lhs.texVertices[corner];
        const TexVertex& right = rhs.texVertices[corner];
        if ( !sameFloatBits(left.u, right.u) || !sameFloatBits(left.v, right.v) ||
             !sameFloatBits(left.color.r, right.color.r) || !sameFloatBits(left.color.g, right.color.g) ||
             !sameFloatBits(left.color.b, right.color.b) || !sameFloatBits(left.color.a, right.color.a) )
        {
            return false;
        }
    }
    return true;
}

std::vector<level::ConnectedSurfaceUvVertexPosition> transformedPositions(const Level& level,
                                                                          const level::ConnectedSurfaceUvPlan& plan)
{
    std::vector<level::ConnectedSurfaceUvVertexPosition> positions{};
    positions.reserve(plan.movedVertices.size());
    for ( const VertexRef ref : plan.movedVertices )
    {
        const Vertex& vertex =
            level.sectors[static_cast<std::size_t>(ref.sector)].vertices[static_cast<std::size_t>(ref.vertex)];
        positions.push_back(level::ConnectedSurfaceUvVertexPosition{ref, TVector{vertex.x, vertex.y, vertex.z}});
    }
    return positions;
}

bool closeUv(const TexVertex& lhs, const TexVertex& rhs) noexcept
{
    return close(lhs.u, rhs.u, 1.0e-5) && close(lhs.v, rhs.v, 1.0e-5);
}

bool sameLevelUvBits(const Level& lhs, const Level& rhs) noexcept
{
    if ( lhs.sectors.size() != rhs.sectors.size() )
    {
        return false;
    }
    for ( std::size_t sector = 0; sector < lhs.sectors.size(); ++sector )
    {
        if ( lhs.sectors[sector].surfaces.size() != rhs.sectors[sector].surfaces.size() )
        {
            return false;
        }
        for ( std::size_t surface = 0; surface < lhs.sectors[sector].surfaces.size(); ++surface )
        {
            if ( !sameUvBits(lhs.sectors[sector].surfaces[surface], rhs.sectors[sector].surfaces[surface]) )
            {
                return false;
            }
        }
    }
    return true;
}

bool sameEvaluation(const level::ConnectedSurfaceUvEvaluation& lhs,
                    const level::ConnectedSurfaceUvEvaluation& rhs) noexcept
{
    if ( lhs.valid != rhs.valid || lhs.coordinateSpace != rhs.coordinateSpace ||
         lhs.surfaces.size() != rhs.surfaces.size() )
    {
        return false;
    }
    for ( std::size_t index = 0; index < lhs.surfaces.size(); ++index )
    {
        if ( lhs.surfaces[index].surface != rhs.surfaces[index].surface )
        {
            return false;
        }
        if ( lhs.surfaces[index].vertices != rhs.surfaces[index].vertices )
        {
            return false;
        }
        Surface left{};
        left.texVertices = lhs.surfaces[index].texVertices;
        Surface right{};
        right.texVertices = rhs.surfaces[index].texVertices;
        if ( !sameUvBits(left, right) )
        {
            return false;
        }
    }
    return true;
}

const level::ConnectedSurfaceUvSurface* findEvaluationSurface(const level::ConnectedSurfaceUvEvaluation& evaluation,
                                                              const SurfaceRef ref) noexcept
{
    const auto found = std::ranges::find_if(evaluation.surfaces,
                                            [ref](const level::ConnectedSurfaceUvSurface& surface)
                                            {
                                                return surface.surface == ref;
                                            });
    return found == evaluation.surfaces.end() ? nullptr : &*found;
}

bool evaluationMatchesLevelUvBits(const level::ConnectedSurfaceUvEvaluation& evaluation, const Level& level) noexcept
{
    for ( const level::ConnectedSurfaceUvSurface& update : evaluation.surfaces )
    {
        if ( update.surface.sector < 0 || update.surface.surface < 0 ||
             static_cast<std::size_t>(update.surface.sector) >= level.sectors.size() )
        {
            return false;
        }
        const Sector& sector = level.sectors[static_cast<std::size_t>(update.surface.sector)];
        if ( static_cast<std::size_t>(update.surface.surface) >= sector.surfaces.size() )
        {
            return false;
        }
        Surface comparable{};
        comparable.texVertices = update.texVertices;
        if ( !sameUvBits(comparable, sector.surfaces[static_cast<std::size_t>(update.surface.surface)]) )
        {
            return false;
        }
    }
    return true;
}

bool sameColors(const Level& lhs, const Level& rhs) noexcept
{
    if ( lhs.sectors.size() != rhs.sectors.size() )
    {
        return false;
    }
    for ( std::size_t sector = 0; sector < lhs.sectors.size(); ++sector )
    {
        if ( lhs.sectors[sector].surfaces.size() != rhs.sectors[sector].surfaces.size() )
        {
            return false;
        }
        for ( std::size_t surface = 0; surface < lhs.sectors[sector].surfaces.size(); ++surface )
        {
            const auto& left = lhs.sectors[sector].surfaces[surface].texVertices;
            const auto& right = rhs.sectors[sector].surfaces[surface].texVertices;
            if ( left.size() != right.size() )
            {
                return false;
            }
            for ( std::size_t corner = 0; corner < left.size(); ++corner )
            {
                if ( !sameFloatBits(left[corner].color.r, right[corner].color.r) ||
                     !sameFloatBits(left[corner].color.g, right[corner].color.g) ||
                     !sameFloatBits(left[corner].color.b, right[corner].color.b) ||
                     !sameFloatBits(left[corner].color.a, right[corner].color.a) )
                {
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace

void runConnectedSurfaceUvTests()
{
    for ( const UvCoordinateSpace coordinateSpace : {UvCoordinateSpace::Normalized, UvCoordinateSpace::TexturePixels} )
    {
        const float units = coordinateSpace == UvCoordinateSpace::Normalized ? 1.0f : 256.0f;
        const double side = 5.0e-6;
        Level source{};
        source.uvCoordinateSpace = coordinateSpace;
        source.sectors.resize(1);
        source.sectors[0].vertices = {
            Vertex{0.0, 0.0, 0.0}, Vertex{side, 0.0, 0.0},
            Vertex{side, side, 0.0}, Vertex{0.0, side, 0.0},
        };
        source.sectors[0].surfaces = {
            makeSurface({0, 1, 2, 3}, {{-2.0f * units, 3.0f * units}, {2.0f * units, 3.0f * units},
                                       {2.0f * units, 5.0f * units}, {-2.0f * units, 5.0f * units}}),
        };
        const std::array moved{VertexRef{0, 1}, VertexRef{0, 2}};
        const level::ConnectedSurfaceUvPlan plan = level::captureConnectedSurfaceUvPlan(source, moved, false);
        check(plan.valid, "connected UV capture accepts a small nondegenerate face in either UV space");

        std::vector<level::ConnectedSurfaceUvVertexPosition> positions = transformedPositions(source, plan);
        for ( level::ConnectedSurfaceUvVertexPosition& position : positions )
        {
            position.position.x *= 2.0;
        }
        const level::ConnectedSurfaceUvEvaluation evaluation = level::evaluateConnectedSurfaceUvPlan(plan, positions);
        const level::ConnectedSurfaceUvSurface* surface = findEvaluationSurface(evaluation, SurfaceRef{0, 0});
        check(evaluation.valid && surface && surface->texVertices.size() == 4u &&
                  close(surface->texVertices[1].u / units, 6.0, 1.0e-5) &&
                  close(surface->texVertices[2].u / units, 6.0, 1.0e-5),
              "small-face connected UV deformation preserves analytic texture density without unit conversion");

        bool allCornersCorrect = surface && surface->texVertices.size() == 4u;
        if ( allCornersCorrect )
        {
            for ( std::size_t corner = 0; corner < 4u; ++corner )
            {
                const TexVertex& before = source.sectors[0].surfaces[0].texVertices[corner];
                const TexVertex& after = surface->texVertices[corner];
                const float expectedU = (corner == 1u || corner == 2u) ? 6.0f * units : before.u;
                allCornersCorrect = allCornersCorrect && close(after.u / units, expectedU / units, 1.0e-5) &&
                                    sameFloatBits(after.v, before.v) &&
                                    sameFloatBits(after.color.r, before.color.r) &&
                                    sameFloatBits(after.color.g, before.color.g) &&
                                    sameFloatBits(after.color.b, before.color.b) &&
                                    sameFloatBits(after.color.a, before.color.a);
            }
        }
        check(allCornersCorrect, "small-face correction preserves anchored UVs, V density, and every corner color");

        editor::DocumentState state{};
        state.level = source;
        editor::Document document{std::move(state)};
        const bool committed = document.applyUndoable(
            "Small face edge: Moved", editor::DocumentDirty::LevelGeometry,
            [&](editor::DocumentState& current)
            {
                level::levelTranslateVertices(current.level, moved, side, 0.0, 0.0);
                const level::ConnectedSurfaceUvEvaluation commit = level::evaluateConnectedSurfaceUvPlan(plan, current.level);
                return sameEvaluation(evaluation, commit) && level::applyConnectedSurfaceUvEvaluation(current.level, commit);
            });
        check(committed && document.level().uvCoordinateSpace == coordinateSpace &&
                  document.level().sectors[0].vertices[1].x == 2.0 * side &&
                  close(document.level().sectors[0].surfaces[0].texVertices[1].u / units, 6.0, 1.0e-5),
              "small-face document commit matches preview, preserves the space tag, and moves geometry");
        const Level after = document.level();
        const bool undone = document.undo();
        check(undone && sameLevelUvBits(document.level(), source) &&
                  document.level().sectors[0].vertices[1].x == side && !document.canUndo(),
              "small-face correction and geometry undo together exactly as one step");
        const bool redone = document.redo();
        check(redone && sameLevelUvBits(document.level(), after) &&
                  document.level().sectors[0].vertices[1].x == 2.0 * side,
              "small-face geometry and UV correction redo together");
    }

    for ( const UvCoordinateSpace coordinateSpace : {UvCoordinateSpace::Normalized, UvCoordinateSpace::TexturePixels} )
    {
        const float units = coordinateSpace == UvCoordinateSpace::Normalized ? 1.0f : 256.0f;
        Level source{};
        source.uvCoordinateSpace = coordinateSpace;
        source.sectors.resize(1);
        source.sectors[0].vertices = {
            Vertex{0.0, 0.0, 0.0}, Vertex{1.0, 1.0, 0.0}, Vertex{2.0, 2.0 + 1.0e-8, 0.0},
        };
        source.sectors[0].surfaces = {
            makeSurface({0, 1, 2}, {{0.0f, 0.0f}, {units, 0.0f}, {2.0f * units, units}}),
        };
        const std::array moved{VertexRef{0, 0}, VertexRef{0, 1}, VertexRef{0, 2}};
        const level::ConnectedSurfaceUvPlan plan = level::captureConnectedSurfaceUvPlan(source, moved, true);
        level::ConnectedSurfaceUvTransform scale{};
        scale.kind = level::ConnectedSurfaceUvTransformKind::scale;
        scale.scale.how = ScaleHow::xyz;
        scale.scale.sfactor = 2.0;
        const level::ConnectedSurfaceUvEvaluation evaluation = level::evaluateConnectedSurfaceUvPlan(plan, scale);
        const level::ConnectedSurfaceUvSurface* surface = findEvaluationSurface(evaluation, SurfaceRef{0, 0});
        bool correct = plan.valid && evaluation.valid && surface && surface->texVertices.size() == 3u;
        if ( correct )
        {
            for ( std::size_t corner = 0; corner < 3u; ++corner )
            {
                const TexVertex& before = source.sectors[0].surfaces[0].texVertices[corner];
                correct = correct && close(surface->texVertices[corner].u / units, 2.0 * before.u / units, 1.0e-5) &&
                          close(surface->texVertices[corner].v / units, 2.0 * before.v / units, 1.0e-5);
            }
        }
        check(correct, "oblique skinny-face UV basis avoids squared-length cancellation and preserves scale density");
    }

    for ( const UvCoordinateSpace coordinateSpace : {UvCoordinateSpace::Normalized, UvCoordinateSpace::TexturePixels} )
    {
        Level source = makeConnectedUvFixture(coordinateSpace);
        const float units = coordinateSpace == UvCoordinateSpace::Normalized ? 1.0f : 256.0f;
        Surface& distorted = source.sectors[0].surfaces[1];
        distorted.texVertices[0].u = -3.25f * units;
        distorted.texVertices[0].v = 7.75f * units;
        distorted.texVertices[1].u = 12.5f * units;
        distorted.texVertices[1].v = -4.125f * units;
        distorted.texVertices[2].u = -21.75f * units;
        distorted.texVertices[2].v = 15.625f * units;
        const std::array moved{
            VertexRef{0, 0},
            VertexRef{0, 1},
            VertexRef{0, 4},
        };
        const level::ConnectedSurfaceUvPlan plan = level::captureConnectedSurfaceUvPlan(source, moved, false);
        const level::ConnectedSurfaceUvEvaluation noOp =
            level::evaluateConnectedSurfaceUvPlan(plan, transformedPositions(source, plan));
        const level::ConnectedSurfaceUvEvaluation descriptorNoOp =
            level::evaluateConnectedSurfaceUvPlan(plan, level::ConnectedSurfaceUvTransform{});
        const level::ConnectedSurfaceUvSurface* noOpSurface = findEvaluationSurface(noOp, SurfaceRef{0, 1});
        check(noOp.valid && descriptorNoOp.valid && noOpSurface && evaluationMatchesLevelUvBits(noOp, source) &&
                  evaluationMatchesLevelUvBits(descriptorNoOp, source),
              "connected UV no-op evaluation preserves every authored UV and color bit-exactly in normalized and pixel "
              "space");

        std::vector<level::ConnectedSurfaceUvVertexPosition> positions = transformedPositions(source, plan);
        for ( level::ConnectedSurfaceUvVertexPosition& position : positions )
        {
            position.position.z += 0.375;
        }
        const level::ConnectedSurfaceUvEvaluation movedEvaluation =
            level::evaluateConnectedSurfaceUvPlan(plan, positions);
        const level::ConnectedSurfaceUvSurface* movedSurface = findEvaluationSurface(movedEvaluation, SurfaceRef{0, 1});
        const auto planSurface = std::ranges::find_if(plan.surfaces,
                                                      [](const level::ConnectedSurfaceUvPlanSurface& surface)
                                                      {
                                                          return surface.surface == SurfaceRef{0, 1};
                                                      });
        bool residualPreserved = movedSurface && planSurface != plan.surfaces.end();
        if ( residualPreserved )
        {
            for ( std::size_t corner = 0; corner < planSurface->vertices.size(); ++corner )
            {
                if ( !planSurface->movedCorners[corner] )
                {
                    continue;
                }
                const TVector sourceOffset =
                    planSurface->sourcePositions[corner] - planSurface->sourcePositions[planSurface->anchorCorner];
                const auto target = std::ranges::find_if(
                    positions,
                    [ref = planSurface->vertices[corner]](const level::ConnectedSurfaceUvVertexPosition& value)
                    {
                        return value.vertex.sector == ref.sector && value.vertex.vertex == ref.vertex;
                    });
                if ( target == positions.end() )
                {
                    residualPreserved = false;
                    break;
                }
                const TVector targetOffset = target->position - planSurface->sourcePositions[planSurface->anchorCorner];
                const double expectedU = static_cast<double>(planSurface->sourceTexVertices[corner].u) +
                                         planSurface->uGradient.dot(targetOffset) -
                                         planSurface->uGradient.dot(sourceOffset);
                const double expectedV = static_cast<double>(planSurface->sourceTexVertices[corner].v) +
                                         planSurface->vGradient.dot(targetOffset) -
                                         planSurface->vGradient.dot(sourceOffset);
                residualPreserved = residualPreserved &&
                                    close(movedSurface->texVertices[corner].u, expectedU, 1.0e-5) &&
                                    close(movedSurface->texVertices[corner].v, expectedV, 1.0e-5);
            }
        }
        check(movedEvaluation.valid && residualPreserved, "connected UV deformation applies only the geometric UV "
                                                          "delta and retains signed multi-tile per-corner residuals");
    }

    for ( const UvCoordinateSpace coordinateSpace : {UvCoordinateSpace::Normalized, UvCoordinateSpace::TexturePixels} )
    {
        const Level before = makeConnectedUvFixture(coordinateSpace);
        Level committed = before;
        const std::array selected{SurfaceRef{0, 0}};
        const std::array moved{
            VertexRef{0, 0}, VertexRef{0, 1}, VertexRef{0, 2}, VertexRef{0, 3},
            VertexRef{1, 0}, VertexRef{1, 1}, VertexRef{1, 2}, VertexRef{1, 3},
        };
        const level::ConnectedSurfaceUvPlan plan = level::captureConnectedSurfaceUvPlan(before, moved, false);

        level::levelTranslateSurfaces(committed, selected, 0.0, 0.0, 2.0);
        const level::ConnectedSurfaceUvEvaluation preview =
            level::evaluateConnectedSurfaceUvPlan(plan, transformedPositions(committed, plan));

        check(preview.valid && preview.surfaces.size() == 2u,
              "connected UV Move evaluates both partial portal-side neighbors in normalized and pixel space");
        check(level::applyConnectedSurfaceUvEvaluation(committed, preview),
              "connected UV Move applies a complete valid evaluation atomically");
        check(committed.uvCoordinateSpace == coordinateSpace,
              "connected UV Move preserves the Level coordinate-space tag without MAT dimensions");
        check(sameUvBits(committed.sectors[0].surfaces[0], before.sectors[0].surfaces[0]) &&
                  sameUvBits(committed.sectors[1].surfaces[0], before.sectors[1].surfaces[0]),
              "connected UV Move keeps complete selected and reciprocal faces bit-exact");
        check(!sameUvBits(committed.sectors[0].surfaces[1], before.sectors[0].surfaces[1]) &&
                  !sameUvBits(committed.sectors[1].surfaces[1], before.sectors[1].surfaces[1]),
              "connected UV Move reprojects every adjacent partially deformed face");
        check(sameColors(committed, before), "connected UV Move preserves every per-corner lighting color bit-exactly");
        check(
            closeUv(committed.sectors[0].surfaces[1].texVertices[2], before.sectors[0].surfaces[1].texVertices[2]) &&
                closeUv(committed.sectors[0].surfaces[1].texVertices[3], before.sectors[0].surfaces[1].texVertices[3]),
            "connected UV Move preserves the unmoved edge as the reprojection anchor");
    }

    for ( const UvCoordinateSpace coordinateSpace : {UvCoordinateSpace::Normalized, UvCoordinateSpace::TexturePixels} )
    {
        const Level before = makeConnectedUvFixture(coordinateSpace);
        Level rotated = before;
        const std::array selected{SurfaceRef{0, 0}};
        const std::array moved{
            VertexRef{0, 0}, VertexRef{0, 1}, VertexRef{0, 2}, VertexRef{0, 3},
            VertexRef{1, 0}, VertexRef{1, 1}, VertexRef{1, 2}, VertexRef{1, 3},
        };
        const level::ConnectedSurfaceUvPlan plan = level::captureConnectedSurfaceUvPlan(before, moved, false);
        level::levelRotateSurfaces(rotated, selected, TVector::right, 0.0, 0.0, 0.0, 35.0);
        const level::ConnectedSurfaceUvEvaluation evaluation = level::evaluateConnectedSurfaceUvPlan(plan, rotated);
        check(evaluation.valid && level::applyConnectedSurfaceUvEvaluation(rotated, evaluation) &&
                  sameUvBits(rotated.sectors[0].surfaces[0], before.sectors[0].surfaces[0]) &&
                  sameUvBits(rotated.sectors[1].surfaces[0], before.sectors[1].surfaces[0]) &&
                  sameColors(rotated, before),
              "connected UV Rotate keeps rigid portal faces exact and transports neighbor texture bases in both UV "
              "spaces");
    }

    for ( const UvCoordinateSpace coordinateSpace : {UvCoordinateSpace::Normalized, UvCoordinateSpace::TexturePixels} )
    {
        const Level before = makeConnectedUvFixture(coordinateSpace);
        const double units = coordinateSpace == UvCoordinateSpace::Normalized ? 1.0 : 256.0;
        Level scaled = before;
        const std::array selected{SurfaceRef{0, 0}};
        const std::array moved{
            VertexRef{0, 0}, VertexRef{0, 1}, VertexRef{0, 2}, VertexRef{0, 3},
            VertexRef{1, 0}, VertexRef{1, 1}, VertexRef{1, 2}, VertexRef{1, 3},
        };
        const level::ConnectedSurfaceUvPlan plan = level::captureConnectedSurfaceUvPlan(before, moved, true);
        const ScaleData scale{
            .cx = 0.5,
            .cy = 0.5,
            .cz = 0.0,
            .sfactor = 2.0,
            .vec = TVector{},
            .how = ScaleHow::x,
        };

        level::levelScaleSurfaces(scaled, selected, scale, false);
        const level::ConnectedSurfaceUvEvaluation evaluation = level::evaluateConnectedSurfaceUvPlan(plan, scaled);
        check(evaluation.valid && evaluation.surfaces.size() == 4u,
              "connected UV Scale includes selected, reciprocal and adjacent surfaces");
        check(level::applyConnectedSurfaceUvEvaluation(scaled, evaluation),
              "connected UV Scale applies all affected surfaces");
        check(close(scaled.sectors[0].surfaces[0].texVertices[1].u - scaled.sectors[0].surfaces[0].texVertices[0].u,
                    units * 2.0, 1.0e-5) &&
                  close(scaled.sectors[0].surfaces[0].texVertices[2].v - scaled.sectors[0].surfaces[0].texVertices[1].v,
                        units, 1.0e-5),
              "connected UV axis Scale preserves texture density on the scaled selected face in normalized and pixel "
              "space");
        check(sameFloatBits(scaled.sectors[0].surfaces[0].uScale, before.sectors[0].surfaces[0].uScale) &&
                  sameFloatBits(scaled.sectors[0].surfaces[0].vScale, before.sectors[0].surfaces[0].vScale),
              "connected UV Scale remains distinct from legacy scaleTextures metadata scaling");
    }

    {
        const Level before = makeConnectedUvFixture(UvCoordinateSpace::Normalized);
        const std::array selected{SurfaceRef{0, 0}};
        const std::array moved{
            VertexRef{0, 0},
            VertexRef{0, 1},
            VertexRef{0, 2},
            VertexRef{0, 3},
        };
        const level::ConnectedSurfaceUvPlan plan = level::captureConnectedSurfaceUvPlan(before, moved, false);

        const std::vector<level::ConnectedSurfaceUvVertexPosition> positions = transformedPositions(before, plan);
        const level::ConnectedSurfaceUvEvaluation first = level::evaluateConnectedSurfaceUvPlan(plan, positions);
        const level::ConnectedSurfaceUvEvaluation second = level::evaluateConnectedSurfaceUvPlan(plan, positions);
        check(first.valid && second.valid && sameEvaluation(first, second),
              "connected UV preview and commit evaluations are deterministic from one immutable baseline plan");

        const level::ConnectedSurfaceUvTransform exactTransform{
            .kind = level::ConnectedSurfaceUvTransformKind::rotateThenTranslate,
            .translation = TVector{0.12500011920928955, -0.375, 1.75},
            .rotationAxis = TVector{0.25, -0.5, 0.75},
            .pivot = TVector{1048576.125, -2097152.375, 524288.625},
            .angleDegrees = 17.125,
        };
        Level large = before;
        for ( Sector& sector : large.sectors )
        {
            for ( Vertex& vertex : sector.vertices )
            {
                vertex.x += 1048576.125;
                vertex.y -= 2097152.375;
                vertex.z += 524288.625;
            }
        }
        const level::ConnectedSurfaceUvPlan largePlan = level::captureConnectedSurfaceUvPlan(large, moved, false);
        Level committedLarge = large;
        TVector normalizedAxis = exactTransform.rotationAxis;
        normalizedAxis.Normalize();
        level::levelRotateSurfaces(committedLarge, selected, normalizedAxis, exactTransform.pivot.x,
                                   exactTransform.pivot.y, exactTransform.pivot.z, exactTransform.angleDegrees);
        level::levelTranslateSurfaces(committedLarge, selected, exactTransform.translation.x,
                                      exactTransform.translation.y, exactTransform.translation.z);
        const level::ConnectedSurfaceUvEvaluation descriptorEvaluation =
            level::evaluateConnectedSurfaceUvPlan(largePlan, exactTransform);
        const level::ConnectedSurfaceUvEvaluation committedEvaluation =
            level::evaluateConnectedSurfaceUvPlan(largePlan, committedLarge);
        check(descriptorEvaluation.valid && committedEvaluation.valid &&
                  sameEvaluation(descriptorEvaluation, committedEvaluation),
              "connected UV preview descriptor exactly matches committed double-precision rotate-then-translate "
              "geometry at large fractional coordinates");

        level::ConnectedSurfaceUvTransform invalidRotation = exactTransform;
        invalidRotation.rotationAxis = TVector{};
        check(!level::evaluateConnectedSurfaceUvPlan(largePlan, invalidRotation).valid,
              "connected UV preview rejects a degenerate rotation axis instead of publishing an identity correction");

        const level::ConnectedSurfaceUvPlan largeScalePlan = level::captureConnectedSurfaceUvPlan(large, moved, true);
        const ScaleData exactScale{
            .cx = 1048576.375,
            .cy = -2097152.125,
            .cz = 524288.875,
            .sfactor = 1.3750001192092896,
            .vec = TVector{},
            .how = ScaleHow::y,
        };
        const level::ConnectedSurfaceUvTransform scaleTransform{
            .kind = level::ConnectedSurfaceUvTransformKind::scale,
            .scale = exactScale,
        };
        Level committedScale = large;
        level::levelScaleSurfaces(committedScale, selected, exactScale, false);
        const level::ConnectedSurfaceUvEvaluation scalePreview =
            level::evaluateConnectedSurfaceUvPlan(largeScalePlan, scaleTransform);
        const level::ConnectedSurfaceUvEvaluation scaleCommit =
            level::evaluateConnectedSurfaceUvPlan(largeScalePlan, committedScale);
        check(scalePreview.valid && scaleCommit.valid && sameEvaluation(scalePreview, scaleCommit),
              "connected UV preview descriptor exactly matches committed double-precision axis Scale at large "
              "fractional coordinates");

        ScaleData vectorScale = exactScale;
        vectorScale.sfactor = 1.625;
        vectorScale.vec = TVector{0.25, -0.5, 0.75}.Normalized();
        vectorScale.how = ScaleHow::vec;
        const level::ConnectedSurfaceUvTransform vectorScaleTransform{
            .kind = level::ConnectedSurfaceUvTransformKind::scale,
            .scale = vectorScale,
        };
        Level committedVectorScale = large;
        level::levelScaleSurfaces(committedVectorScale, selected, vectorScale, false);
        const level::ConnectedSurfaceUvEvaluation vectorScalePreview =
            level::evaluateConnectedSurfaceUvPlan(largeScalePlan, vectorScaleTransform);
        const level::ConnectedSurfaceUvEvaluation vectorScaleCommit =
            level::evaluateConnectedSurfaceUvPlan(largeScalePlan, committedVectorScale);
        check(vectorScalePreview.valid && vectorScaleCommit.valid &&
                  sameEvaluation(vectorScalePreview, vectorScaleCommit),
              "connected UV preview descriptor exactly matches committed vector-axis Scale geometry");

        ScaleData nonUnitVectorScale = vectorScale;
        nonUnitVectorScale.vec = vectorScale.vec * 7.0;
        const level::ConnectedSurfaceUvTransform nonUnitVectorScaleTransform{
            .kind = level::ConnectedSurfaceUvTransformKind::scale,
            .scale = nonUnitVectorScale,
        };
        const level::ConnectedSurfaceUvEvaluation nonUnitVectorScalePreview =
            level::evaluateConnectedSurfaceUvPlan(
                largeScalePlan, nonUnitVectorScaleTransform);
        check(nonUnitVectorScalePreview.valid &&
                  sameEvaluation(nonUnitVectorScalePreview, vectorScalePreview),
              "connected UV vector-axis Scale is independent of finite axis magnitude");

        level::ConnectedSurfaceUvTransform invalidScaleTransform = vectorScaleTransform;
        invalidScaleTransform.scale.how = static_cast<ScaleHow>(std::numeric_limits<std::int32_t>::max());
        check(!level::evaluateConnectedSurfaceUvPlan(largeScalePlan, invalidScaleTransform).valid,
              "connected UV preview rejects an unknown Scale mode instead of publishing an identity correction");

        level::ConnectedSurfaceUvTransform degenerateScaleTransform =
            vectorScaleTransform;
        degenerateScaleTransform.scale.vec = TVector::zero;
        check(!level::evaluateConnectedSurfaceUvPlan(
                  largeScalePlan, degenerateScaleTransform).valid,
              "connected UV preview rejects a zero vector-axis Scale instead of changing only texture density");

        Level wrongSpace = before;
        wrongSpace.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
        const Level beforeWrongSpace = wrongSpace;
        check(!level::applyConnectedSurfaceUvEvaluation(wrongSpace, first) &&
                  sameLevelUvBits(wrongSpace, beforeWrongSpace),
              "connected UV publication rejects a changed coordinate space without partial mutation");

        level::ConnectedSurfaceUvEvaluation malformed = first;
        malformed.surfaces.push_back(level::ConnectedSurfaceUvSurface{SurfaceRef{99, 99}, {}, {}});
        Level atomic = before;
        const Level beforeAtomic = atomic;
        check(!level::applyConnectedSurfaceUvEvaluation(atomic, malformed) && sameLevelUvBits(atomic, beforeAtomic),
              "connected UV publication validates every output before modifying any surface");

        level::ConnectedSurfaceUvEvaluation staleTopology = first;
        Level remapped = before;
        std::rotate(remapped.sectors[0].surfaces[1].vertices.begin(),
                    remapped.sectors[0].surfaces[1].vertices.begin() + 1,
                    remapped.sectors[0].surfaces[1].vertices.end());
        const Level beforeRemapped = remapped;
        check(!level::applyConnectedSurfaceUvEvaluation(remapped, staleTopology) &&
                  sameLevelUvBits(remapped, beforeRemapped),
              "connected UV publication rejects stale same-sized face topology atomically");

        std::vector<level::ConnectedSurfaceUvVertexPosition> duplicatePositions = positions;
        if ( duplicatePositions.size() >= 2u )
        {
            duplicatePositions[1].vertex = duplicatePositions[0].vertex;
        }
        check(!level::evaluateConnectedSurfaceUvPlan(plan, duplicatePositions).valid,
              "connected UV evaluation rejects duplicate transformed vertex addresses");
    }

    {
        Level invalid = makeConnectedUvFixture(UvCoordinateSpace::Normalized);
        invalid.sectors[0].surfaces[1].texVertices[0].u = std::numeric_limits<float>::quiet_NaN();
        const std::array moved{VertexRef{0, 0}};
        const level::ConnectedSurfaceUvPlan plan = level::captureConnectedSurfaceUvPlan(invalid, moved, false);
        check(!plan.valid && plan.surfaces.empty(),
              "connected UV capture rejects an affected malformed source mapping atomically");
    }

    {
        const Level level = makeConnectedUvFixture(UvCoordinateSpace::Normalized);
        const std::array duplicate{VertexRef{0, 0}, VertexRef{0, 0}};
        const std::array stale{VertexRef{99, 0}};
        check(!level::captureConnectedSurfaceUvPlan(level, duplicate, false).valid &&
                  !level::captureConnectedSurfaceUvPlan(level, stale, false).valid,
              "connected UV capture rejects duplicate and stale moved-vertex addresses");
    }

    {
        const Level level = makeConnectedUvFixture(UvCoordinateSpace::Normalized);
        const level::ConnectedSurfaceUvPlan plan = level::captureConnectedSurfaceUvPlan(level, {}, false);
        Level unchanged = level;
        const level::ConnectedSurfaceUvEvaluation evaluation = level::evaluateConnectedSurfaceUvPlan(plan, unchanged);
        check(plan.valid && plan.movedVertices.empty() && plan.surfaces.empty() && evaluation.valid &&
                  evaluation.surfaces.empty() && level::applyConnectedSurfaceUvEvaluation(unchanged, evaluation) &&
                  sameLevelUvBits(unchanged, level),
              "empty connected UV plans are deterministic valid no-ops");
    }

    {
        Level invalidSpace = makeConnectedUvFixture(UvCoordinateSpace::Normalized);
        invalidSpace.uvCoordinateSpace = static_cast<UvCoordinateSpace>(255);
        const std::array moved{VertexRef{0, 0}};
        const level::ConnectedSurfaceUvPlan rejectedCapture =
            level::captureConnectedSurfaceUvPlan(invalidSpace, moved, false);

        Level valid = makeConnectedUvFixture(UvCoordinateSpace::Normalized);
        level::ConnectedSurfaceUvPlan forgedPlan =
            level::captureConnectedSurfaceUvPlan(valid, moved, false);
        forgedPlan.coordinateSpace = static_cast<UvCoordinateSpace>(255);
        const level::ConnectedSurfaceUvEvaluation rejectedPositions =
            level::evaluateConnectedSurfaceUvPlan(forgedPlan, transformedPositions(valid, forgedPlan));
        const level::ConnectedSurfaceUvEvaluation rejectedLevel =
            level::evaluateConnectedSurfaceUvPlan(forgedPlan, valid);
        const level::ConnectedSurfaceUvEvaluation rejectedTransform =
            level::evaluateConnectedSurfaceUvPlan(forgedPlan, level::ConnectedSurfaceUvTransform{});

        level::ConnectedSurfaceUvEvaluation forgedEvaluation{};
        forgedEvaluation.valid = true;
        forgedEvaluation.coordinateSpace = static_cast<UvCoordinateSpace>(255);
        invalidSpace = makeConnectedUvFixture(UvCoordinateSpace::Normalized);
        invalidSpace.uvCoordinateSpace = forgedEvaluation.coordinateSpace;
        const Level beforeApply = invalidSpace;

        check(!rejectedCapture.valid && !rejectedPositions.valid && !rejectedLevel.valid &&
                  !rejectedTransform.valid &&
                  !level::applyConnectedSurfaceUvEvaluation(invalidSpace, forgedEvaluation) &&
                  sameLevelUvBits(invalidSpace, beforeApply),
              "connected UV capture, every evaluation overload, and publication reject an unknown coordinate-space tag without mutation");
    }

    {
        const Level before = makeConnectedUvFixture(UvCoordinateSpace::Normalized);
        const std::array moved{VertexRef{0, 0}};
        const level::ConnectedSurfaceUvPlan plan =
            level::captureConnectedSurfaceUvPlan(before, moved, false);
        level::ConnectedSurfaceUvEvaluation duplicate =
            level::evaluateConnectedSurfaceUvPlan(plan, transformedPositions(before, plan));
        if ( !duplicate.surfaces.empty() )
        {
            duplicate.surfaces.push_back(duplicate.surfaces.front());
        }

        Level target = before;
        check(!level::applyConnectedSurfaceUvEvaluation(target, duplicate) && sameLevelUvBits(target, before),
              "connected UV publication rejects duplicate surface replacements before changing any target");
    }

    {
        const Level before = makeConnectedUvFixture(UvCoordinateSpace::TexturePixels);
        editor::DocumentState state{};
        state.level = before;
        editor::Document document{std::move(state)};
        const std::array selected{SurfaceRef{0, 0}};
        const std::array moved{
            VertexRef{0, 0}, VertexRef{0, 1}, VertexRef{0, 2}, VertexRef{0, 3},
            VertexRef{1, 0}, VertexRef{1, 1}, VertexRef{1, 2}, VertexRef{1, 3},
        };
        const bool applied =
            document.applyUndoable("Surface: Moved", editor::DocumentDirty::LevelGeometry,
                                   [&](editor::DocumentState& current)
                                   {
                                       const level::ConnectedSurfaceUvPlan plan =
                                           level::captureConnectedSurfaceUvPlan(current.level, moved, false);
                                       level::levelTranslateSurfaces(current.level, selected, 0.0, 0.0, 0.75);
                                       const level::ConnectedSurfaceUvEvaluation evaluation =
                                           level::evaluateConnectedSurfaceUvPlan(plan, current.level);
                                       return level::applyConnectedSurfaceUvEvaluation(current.level, evaluation);
                                   });
        const Level committed = document.level();
        const bool undone = document.undo();
        const bool undoExact = sameLevelUvBits(document.level(), before);
        const bool redone = document.redo();
        check(
            applied && document.canUndo() && undone && undoExact && redone &&
                sameLevelUvBits(document.level(), committed),
            "connected UV geometry and neighbor correction commit as one atomic Undo/Redo transaction in pixel space");
    }
}

} // namespace sed::tests
