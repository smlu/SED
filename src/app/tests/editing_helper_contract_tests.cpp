#include "test_common.hpp"
#include "render/grid/viewport_grid_utils.hpp"
#include "render/snap/surface_transform_snap_utils.hpp"
#include "ui/uv/follow_active_quads_utils.hpp"
#include "ui/uv/smart_uv_project_utils.hpp"
#include "ui/uv/uv_surface_geometry_utils.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <string_view>

namespace sed::app_tests
{
namespace
{
bool exactVector(const TVector& a, const TVector& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
bool exactIslands(const std::vector<ui::uv::UvIslandState>& a,
    const std::vector<ui::uv::UvIslandState>& b)
{
    if ( a.size() != b.size() ) return false;
    for ( std::size_t i = 0; i < a.size(); ++i )
    {
        const auto& x = a[i];
        const auto& y = b[i];
        if ( x.ref != y.ref || x.label != y.label || x.color != y.color
            || x.material != y.material || x.textureSize != y.textureSize
            || x.storedCoordinateSpace != y.storedCoordinateSpace || x.uv != y.uv
            || x.unwrapUv != y.unwrapUv || x.displayOffset != y.displayOffset
            || x.displayMirrorAxisX != y.displayMirrorAxisX || x.displayMirrorX != y.displayMirrorX
            || x.selected != y.selected ) return false;
    }
    return true;
}
bool exactSmartReport(const ui::uv::SmartUvProjectReport& a, const ui::uv::SmartUvProjectReport& b)
{
    return a.projectionGroupCount == b.projectionGroupCount && a.packedIslandCount == b.packedIslandCount
        && a.ignoredFaceCount == b.ignoredFaceCount && a.projectionNormals == b.projectionNormals
        && a.faceProjectionGroups == b.faceProjectionGroups;
}
bool exactStickState(const render::GridPlaneStickState& state, const bool holding, const bool released,
    const double direction, const double contact, const double offset)
{
    return state.holding == holding && state.released == released && state.direction == direction
        && state.contactTranslation == contact && state.releaseOffset == offset;
}
std::pair<Level,std::vector<ui::uv::UvIslandState>> quadStrip()
{
    Level level;
    Sector sector;
    sector.vertices = {{0,0,0,0},{1,0,0,0},{1,1,0,0},{0,1,0,0},{2,0,0,0},{2,1,0,0}};
    Surface left;
    left.vertices = {0,1,2,3}; left.normal = {0,0,1};
    Surface right = left; right.vertices = {1,4,5,2};
    sector.surfaces = {left,right}; level.sectors.push_back(sector);
    std::vector<ui::uv::UvIslandState> islands(2);
    islands[0].ref = {0,0}; islands[1].ref = {0,1};
    islands[0].uv = {{0,0},{1,0},{1,1},{0,1}};
    islands[1].uv = {{4,4},{5,4},{5,5},{4,5}};
    for ( auto& island : islands )
    {
        island.unwrapUv = island.uv;
        island.label = QStringLiteral("preserved-label");
        island.material = QStringLiteral("preserved.mat");
        island.color = QColor(11,22,33,44);
    }
    return {level,islands};
}
}

void runEditingHelperContractTests()
{
    {
        const auto frame = render::makeTransformGridFrame({}, {1,0,0}, {0,1,0}, 0.5);
        TVector out{11,22,33};
        const auto returned = render::snapTransformTranslationToGrid(TVector{},TVector{0.26,-0.26,0.125},frame,&out);
        check(returned == QVector3D(0.5f,-0.5f,0.125f) && exactVector(out,{0.5,-0.5,0.125}),
            "grid snap RETURN and document OUT match fixed three-component correction vector");
        auto invalid = frame; invalid.valid = false;
        out = {11,22,33};
        const TVector raw{0.125,-0.25,0.375};
        const auto fallback = render::snapTransformTranslationToGrid(TVector{},raw,invalid,&out);
        check(fallback == QVector3D(0.125f,-0.25f,0.375f) && exactVector(out,raw),
            "grid snap invalid frame explicitly replaces OUT sentinel with unchanged raw displacement");
        check(render::snapTransformTranslationToGrid(TVector{},raw,invalid,nullptr) == fallback,
            "grid snap nullable document OUT does not change fallback RETURN");
    }
    {
        const auto frame = render::makeTransformGridFrame({}, {1,0,0}, {0,1,0}, 1.0);
        const std::array<TVector,1> points{TVector{0,0,-2}};
        render::GridPlaneStickState state{};
        check(exactVector(render::applyGridPlaneStickThrough(TVector{3,4,1.75},points,frame,0.25,0.5,state),{3,4,2})
                && exactStickState(state,true,false,1,2,0),
            "GridStick inclusive capture boundary RETURN and all INOUT state fields match fixed vector");
        check(exactVector(render::applyGridPlaneStickThrough(TVector{3,4,2.25},points,frame,0.25,0.5,state),{3,4,2})
                && exactStickState(state,true,false,1,2,0),
            "GridStick hold RETURN preserves tangent components and complete clutch state");
        check(exactVector(render::applyGridPlaneStickThrough(TVector{3,4,2.5},points,frame,0.25,0.5,state),{3,4,2})
                && exactStickState(state,false,true,1,2,-0.5),
            "GridStick inclusive release boundary publishes exact RETURN and complete release state");
        check(exactVector(render::applyGridPlaneStickThrough(TVector{3,4,3},points,frame,0.25,0.5,state),{3,4,2.5})
                && exactStickState(state,false,true,1,2,-0.5),
            "GridStick released state remains latched with fixed normal-only offset");
        const TVector raw{3,4,1.75};
        for ( const double invalidAxis : {std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()} )
        {
            state = {};
            const auto result = render::applyGridPlaneStickThrough(raw,points,frame,0.25,0.5,state,{invalidAxis,0,0});
            check(exactVector(result,raw) && exactStickState(state,false,false,0,0,0),
                "GridStick rejects nonfinite constraint input without treating it as free movement or acquiring state");
        }
        state = {true,false,1,2,0};
        auto invalidFrame = frame; invalidFrame.valid = false;
        check(exactVector(render::applyGridPlaneStickThrough(raw,points,invalidFrame,0.25,0.5,state),raw)
                && exactStickState(state,false,false,0,0,0),
            "GridStick invalid frame returns exact raw vector and resets every INOUT clutch field");
    }
    {
        render::SurfaceSnapVertex moving{{0,0,0},0,0,TVector{0,0,0}};
        const std::array<render::SurfaceSnapVertex,1> target{{{{1,0,0},1,0,TVector{1,0,0}}}};
        TVector out{11,22,33};
        const auto exact = render::snapVertexMoveTranslation(moving,TVector{},target,1.0f,{},&out);
        check(exact && *exact == QVector3D(1,0,0) && exactVector(out,{1,0,0}),
            "Vertex snap inclusive capture RETURN and document OUT match fixed vector");
        for ( const float capture : {0.0f,-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()} )
        {
            out = {11,22,33};
            check(!render::snapVertexMoveTranslation(moving,TVector{},target,capture,{},&out)
                    && exactVector(out,{11,22,33}),
                "Vertex snap invalid capture rejects RETURN and leaves every OUT sentinel component untouched");
        }
        auto outside = target;
        outside[0].documentPoint = TVector{std::nextafter(1.0,2.0),0,0};
        out = {11,22,33};
        check(!render::snapVertexMoveTranslation(moving,TVector{},outside,1.0f,{},&out)
                && exactVector(out,{11,22,33}),
            "Vertex snap nextafter outside capture rejects without overwriting document OUT");
        const std::array<render::SurfaceSnapVertex,2> overflowFirst{{
            {{},1,0,TVector{4e38,0,0}}, {{},2,0,TVector{3e38,0,0}}}};
        out = {11,22,33};
        const auto representable = render::snapVertexMoveTranslation(moving,TVector{4e38,0,0},overflowFirst,
            std::numeric_limits<float>::max(),{},&out);
        check(representable && std::isfinite(representable->x()) && out.x == 3e38 && out.y == 0 && out.z == 0,
            "Vertex snap unrepresentable candidate cannot poison ranking before a representable result publishes its OUT");
        const render::SurfaceSnapEdge edge{{0,0,0},{0,1,0},0,0,1,TVector{0,0,0},TVector{0,1,0}};
        const std::array<render::SurfaceSnapEdge,1> remote{{
            {{0,0,0},{0,1,0},1,0,1,TVector{4e38,0,0},TVector{4e38,1,0}}}};
        check(!render::snapEdgeMoveTransform(edge,TVector{4e38,0,0},remote,1.0f),
            "Edge snap cannot return success with a nonfinite presentation translation from finite document inputs");
    }
    {
        const auto [level,initial] = quadStrip();
        ui::uv::SmartUvProjectReport sentinel{71,72,73,{{0.125f,0.25f,0.5f}},{74,75}};
        auto report = sentinel;
        auto islands = initial;
        ui::uv::SmartUvProjectOptions options;
        options.rotationMethod = ui::uv::SmartUvRotationMethod::axisAligned;
        options.correctAspect = false;
        check(ui::uv::smartUvProject(level,islands,options,{1,1},&report)
                && report.projectionGroupCount == 1 && report.packedIslandCount == 1 && report.ignoredFaceCount == 0
                && report.projectionNormals == std::vector<QVector3D>{{0,0,1}}
                && report.faceProjectionGroups == std::vector<int>{0,0}
                && islands[0].unwrapUv == std::vector<QPointF>{{0,0},{0.5,0},{0.5,0.5},{0,0.5}}
                && islands[1].unwrapUv == std::vector<QPointF>{{0.5,0},{1,0},{1,0.5},{0.5,0.5}},
            "SmartUV success replaces complete report sentinel and publishes fixed normalized two-quad UV vectors");
        for ( const bool lateFailure : {false,true} )
        {
            islands = initial; report = sentinel;
            Level malformed = level;
            auto badOptions = options;
            if ( lateFailure ) malformed.sectors[0].surfaces[1].vertices.back() = 99;
            else badOptions.marginMethod = static_cast<ui::uv::SmartUvMarginMethod>(255);
            check(!ui::uv::smartUvProject(malformed,islands,badOptions,{1,1},&report)
                    && exactIslands(islands,initial) && exactSmartReport(report,sentinel),
                "SmartUV early and late failure preserve every island INOUT field and every report OUT sentinel field");
        }
        ui::uv::FollowActiveQuadsReport followReport{ui::uv::FollowActiveQuadsError::invalidGeometry,71,72,73,74};
        islands = initial;
        check(ui::uv::followActiveQuads(level,islands,{true,true},SurfaceRef{0,0},ui::uv::FollowActiveQuadsMode::even,&followReport)
                && followReport.error == ui::uv::FollowActiveQuadsError::none && followReport.visitedFaceCount == 2
                && followReport.updatedFaceCount == 1 && followReport.ignoredNonQuadCount == 0 && followReport.ambiguousEdgeCount == 0
                && exactIslands({islands[0]},{initial[0]})
                && islands[1].uv == std::vector<QPointF>{{1,0},{2,0},{2,1},{1,1}},
            "Follow success replaces complete report sentinel, preserves exact active island and publishes fixed neighbor UVs");
        islands = initial;
        followReport = {ui::uv::FollowActiveQuadsError::none,71,72,73,74};
        check(!ui::uv::followActiveQuads(level,islands,{true,true},SurfaceRef{0,0},static_cast<ui::uv::FollowActiveQuadsMode>(255),&followReport)
                && exactIslands(islands,initial) && followReport.error == ui::uv::FollowActiveQuadsError::invalidGeometry
                && followReport.visitedFaceCount == 0 && followReport.updatedFaceCount == 0
                && followReport.ignoredNonQuadCount == 0 && followReport.ambiguousEdgeCount == 0,
            "Follow invalid mode rejects atomically and replaces all report fields with the failure diagnostic");
    }
    {
        using Error = ui::uv::FollowActiveQuadsError;
        const std::array<std::pair<Error,std::string_view>,8> messages{{
            {Error::none,""},
            {Error::noActiveFace,"Choose an active selected surface first."},
            {Error::activeFaceNotSelected,"The active surface must be part of the UV selection."},
            {Error::activeFaceNotQuad,"Follow Active Quads needs at least one selected four-vertex surface. Surfaces with other vertex counts are ignored."},
            {Error::invalidActiveUv,"The active surface has incomplete or invalid UV coordinates."},
            {Error::noConnectedQuads,"Follow Active Quads needs at least two selected four-vertex surfaces that share a complete edge. Surfaces with other vertex counts are ignored."},
            {Error::invalidGeometry,"The selected quad geometry or UV coordinates are invalid."},
            {static_cast<Error>(255),"Follow Active Quads failed."}}};
        for ( const auto& [error,expected] : messages )
            check(std::string_view(ui::uv::followActiveQuadsErrorMessage(error)) == expected,
                "Follow diagnostic STRING OUTPUT matches complete fixed vectors including empty success and unknown enum");
        QVector3D normal(3,4,0);
        check(ui::uv::normalizeUvVector(normal) && normal == QVector3D(0.6f,0.8f,0),
            "normalizeUvVector true RETURN publishes fixed unit-vector INOUT components");
        const float nan = std::bit_cast<float>(std::uint32_t{0x7fc01234});
        normal = QVector3D(nan,-2,3);
        check(!ui::uv::normalizeUvVector(normal) && std::bit_cast<std::uint32_t>(normal.x()) == 0x7fc01234
                && normal.y() == -2 && normal.z() == 3,
            "normalizeUvVector false RETURN preserves NaN payload and remaining INOUT components");
    }
}
}
