#include "test_common.hpp"
#include "../../src/level/geometry_precision.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace sed::tests
{
namespace
{
bool rawIndexedClosure(const Sector& sector)
{
    struct Usage { int count{}; int direction{}; std::set<std::size_t> faces; };
    std::map<std::pair<std::int32_t,std::int32_t>,Usage> edges;
    for ( std::size_t f = 0; f < sector.surfaces.size(); ++f )
    {
        const auto& face = sector.surfaces[f];
        if ( face.vertices.size() < 3 )
        {
            return false;
        }
        for ( std::size_t c = 0; c < face.vertices.size(); ++c )
        {
            const auto a = face.vertices[c], b = face.vertices[(c+1)%face.vertices.size()];
            if ( a == b || a < 0 || b < 0 || static_cast<std::size_t>(a) >= sector.vertices.size()
                || static_cast<std::size_t>(b) >= sector.vertices.size() )
            {
                return false;
            }
            auto& usage = edges[std::minmax(a,b)];
            ++usage.count;
            usage.direction += a < b ? 1 : -1;
            usage.faces.insert(f);
        }
    }
    return !edges.empty() && std::all_of(edges.begin(),edges.end(),[](const auto& edge)
    {
        return edge.second.count == 2 && edge.second.direction == 0 && edge.second.faces.size() == 2;
    });
}
void appendClosureFace(Sector& sector, std::initializer_list<std::int32_t> indices)
{
    Surface face;
    face.vertices.assign(indices);
    sector.surfaces.push_back(std::move(face));
}
Sector nearEdgeClosedFixture()
{
    // Fixed synthetic expanded-prism geometry: every directed indexed edge
    // already has its opposite. A nearby long edge must not acquire the
    // unrelated short edge's endpoints through geometric subdivision.
    Sector result;
    const std::array<TVector,20> points{{
        {3.5999999999999748,2.9078908166790227,1.990413612669957},
        {2.8999999999999391,2.9078912627337457,1.9904135719743892},
        {2.9000000000000385,3.0987697044080003,4.0825863854061781},
        {3.6000000000000743,3.0987692583532773,4.0825864261017459},
        {3.6000001279735869,3.2979420446406271,4.0644149869040023},
        {2.9000001279735512,3.2979424906953501,4.0644149462084345},
        {2.9000001279734517,3.1070640490210955,1.9722421327766453},
        {3.6000001279734875,3.1070636029663725,1.9722421734722131},
        {3.4999999999999951,2.9078908804011259,1.9904136068563045},
        {3.5000000000000946,3.0987693220753805,4.0825864202880933},
        {3.4999999999999494,3.0987693220753805,4.0825864202880933},
        {2.9000000000000337,3.0896839853044278,3.9829999976913619},
        {3.5,3.0896835999999999,3.9830000000000001},
        {3.0000000000000138,2.9078911990116425,1.9904135777880418},
        {3,3.0896838999999998,3.9830000000000001},
        {3,2.9169768999999999,2.0899999999999999},
        {3.5,2.9169765999999999,2.0899999999999999},
        {3.5,2.9516456999999998,2.4699999999999998},
        {3.5,2.9872272,2.8599999999999999},
        {3.5,3.0583901,3.6400000000000001}
    }};
    for ( const auto& p : points )
    {
        result.newVertex(p.x,p.y,p.z);
    }
    appendClosureFace(result, {15,14,12,19,18,17,16});
    appendClosureFace(result, {4,5,6,7});
    appendClosureFace(result, {0,3,4,7});
    appendClosureFace(result, {1,13,8,0,7,6});
    appendClosureFace(result, {2,11,1,6,5});
    appendClosureFace(result, {3,9,10,2,5,4});
    appendClosureFace(result, {0,8,9,3});
    appendClosureFace(result, {8,16,17,18,19,12,10,9});
    appendClosureFace(result, {11,2,10,12,14});
    appendClosureFace(result, {13,1,11,14,15});
    appendClosureFace(result, {8,13,15,16});
    return result;
}
Sector closureCube()
{
    Sector result;
    for ( const auto& p : std::array<TVector,8>{{{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
            {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}}} )
    {
        result.newVertex(p.x,p.y,p.z);
    }
    appendClosureFace(result,{1,5,6,2});
    appendClosureFace(result,{0,3,7,4});
    appendClosureFace(result,{3,2,6,7});
    appendClosureFace(result,{0,4,5,1});
    appendClosureFace(result,{4,7,6,5});
    appendClosureFace(result,{0,1,2,3});
    return result;
}
}
void runLevelClosureContractTests()
{
    const auto closeEdges = nearEdgeClosedFixture();
    check(rawIndexedClosure(closeEdges), "Closure fixed near-edge fixture has exactly two opposite indexed uses on distinct faces");
    check(level::precision::closedBoundary(closeEdges), "Closure preserves already paired indexed edges beside unrelated near-collinear edges");
    auto thin = closureCube();
    for ( auto& vertex : thin.vertices )
    {
        vertex.x *= 1e-10;
    }
    check(rawIndexedClosure(thin) && level::precision::closedBoundary(thin), "Closure accepts a fixed thin indexed prism");
    auto oneUlp = closureCube();
    const double next = std::nextafter(1.0, 2.0);
    for ( auto& vertex : oneUlp.vertices )
    {
        vertex.x = vertex.x < 0.0 ? 1.0 : next;
    }
    check(rawIndexedClosure(oneUlp) && level::precision::closedBoundary(oneUlp),
        "Closure accepts an indexed prism whose thickness is one representable coordinate step near one");
    auto oneUlpZeroEdge = oneUlp;
    oneUlpZeroEdge.vertices[1] = oneUlpZeroEdge.vertices[0];
    check(rawIndexedClosure(oneUlpZeroEdge) && !level::precision::closedBoundary(oneUlpZeroEdge),
        "Closure rejects an exactly zero indexed edge even when its surrounding prism is one representable step thick");
    auto split = closureCube();
    const auto midpoint = split.newVertex(1,-1,0);
    split.surfaces[0].vertices.insert(split.surfaces[0].vertices.begin()+1,midpoint);
    check(!rawIndexedClosure(split) && level::precision::closedBoundary(split), "Closure fallback joins an exact T-junction subdivision");
    for ( const bool gap : {false,true} )
    {
        auto aliased = closureCube();
        for ( auto& index : aliased.surfaces[0].vertices )
        {
            const auto point = aliased.vertices[index];
            index = aliased.newVertex(point.x+(gap ? 1e-6 : 0),point.y,point.z);
        }
        check(!rawIndexedClosure(aliased) && level::precision::closedBoundary(aliased) == !gap,
            "Closure fallback accepts exact coordinate aliases and rejects a fixed genuine gap");
    }
    auto missing = closureCube();
    missing.surfaces.pop_back();
    check(!level::precision::closedBoundary(missing), "Closure rejects a missing wall");
    auto duplicated = closureCube();
    duplicated.surfaces.push_back(duplicated.surfaces[0]);
    check(!level::precision::closedBoundary(duplicated), "Closure rejects a third face on an indexed edge");
    auto degenerate = closureCube();
    degenerate.vertices[1] = degenerate.vertices[0];
    check(rawIndexedClosure(degenerate) && !level::precision::closedBoundary(degenerate),
        "Closure raw topology cannot hide an adjacent zero-length coordinate edge");
    auto backtrack = closureCube();
    backtrack.surfaces[0].vertices.insert(backtrack.surfaces[0].vertices.begin()+1,2);
    backtrack.surfaces[0].vertices.insert(backtrack.surfaces[0].vertices.begin()+2,1);
    check(!level::precision::closedBoundary(backtrack), "Closure rejects repeated opposite edge traversal within one face");
}
}
