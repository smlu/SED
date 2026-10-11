#include "frame_ghost_topology.hpp"

#include "sed/assets/3do.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace sed::render::frame_ghost
{
namespace
{

[[nodiscard]] std::uint64_t undirectedEdgeKey(
    const std::uint32_t first,
    const std::uint32_t second) noexcept
{
    const std::uint32_t low = first < second ? first : second;
    const std::uint32_t high = first < second ? second : first;
    return (static_cast<std::uint64_t>(low) << 32u)
        | static_cast<std::uint64_t>(high);
}

} // namespace

ModelTopology buildModelTopology(
    const assets::Model3do& model,
    const std::size_t requestedMaximumEdgeCount)
{
    ModelTopology result{};
    const std::size_t maximumEdgeCount = std::min(
        requestedMaximumEdgeCount,
        kMaximumGhostEdgeCount);

    for ( std::size_t meshIndex = 0u; meshIndex < model.meshes.size(); ++meshIndex )
    {
        const assets::Model3doMesh& mesh = model.meshes[meshIndex];
        const std::size_t remainingEdgeBudget =
            maximumEdgeCount - std::min(
                result.edges.size(), maximumEdgeCount);
        std::size_t edgeCount = 0u;
        for ( const assets::Model3doFace& face : mesh.faces )
        {
            if ( face.vertices.size() >= 2u )
            {
                if ( face.vertices.size() > remainingEdgeBudget
                    - std::min(edgeCount, remainingEdgeBudget) )
                {
                    edgeCount = remainingEdgeBudget;
                }
                else
                {
                    edgeCount += face.vertices.size();
                }
            }
            if ( edgeCount >= remainingEdgeBudget )
            {
                break;
            }
        }

        std::unordered_set<std::uint64_t> seen{};
        seen.reserve(std::min(edgeCount, remainingEdgeBudget));
        for ( const assets::Model3doFace& face : mesh.faces )
        {
            const std::size_t vertexCount = face.vertices.size();
            if ( vertexCount < 2u )
            {
                continue;
            }

            for ( std::size_t edgeIndex = 0u; edgeIndex < vertexCount; ++edgeIndex )
            {
                ++result.sourceFaceEdgeVisits;
                if ( result.sourceFaceEdgeVisits > kMaximumGhostSourceEdgeVisits )
                {
                    std::vector<EdgeRef>{}.swap(result.edges);
                    result.withinBudget = false;
                    return result;
                }
                const std::int32_t firstIndex = face.vertices[edgeIndex];
                const std::int32_t secondIndex =
                    face.vertices[(edgeIndex + 1u) % vertexCount];
                if ( firstIndex < 0 || secondIndex < 0
                    || static_cast<std::size_t>(firstIndex) >= mesh.vertices.size()
                    || static_cast<std::size_t>(secondIndex) >= mesh.vertices.size() )
                {
                    continue;
                }

                const auto first = static_cast<std::uint32_t>(firstIndex);
                const auto second = static_cast<std::uint32_t>(secondIndex);
                if ( !seen.insert(undirectedEdgeKey(first, second)).second )
                {
                    continue;
                }

                if ( result.edges.size() >= maximumEdgeCount )
                {
                    std::vector<EdgeRef>{}.swap(result.edges);
                    result.withinBudget = false;
                    return result;
                }

                result.edges.push_back(EdgeRef{
                    meshIndex,
                    first < second ? first : second,
                    first < second ? second : first,
                });
            }
        }
    }

    return result;
}

std::vector<LocalVertex> buildLocalLineVertices(
    const assets::Model3do& model,
    const ModelTopology& topology)
{
    std::vector<LocalVertex> result{};
    if ( !topology.withinBudget
        || topology.edges.size() > kMaximumGhostEdgeCount )
    {
        return result;
    }
    result.reserve(topology.edges.size() * 2u);
    for ( const EdgeRef& edge : topology.edges )
    {
        if ( edge.meshIndex >= model.meshes.size() )
        {
            continue;
        }
        const assets::Model3doMesh& mesh = model.meshes[edge.meshIndex];
        if ( edge.firstVertex >= mesh.vertices.size()
            || edge.secondVertex >= mesh.vertices.size() )
        {
            continue;
        }

        const assets::Model3doVertex& first = mesh.vertices[edge.firstVertex];
        const assets::Model3doVertex& second = mesh.vertices[edge.secondVertex];
        if ( !std::isfinite(first.x) || !std::isfinite(first.y) || !std::isfinite(first.z)
            || !std::isfinite(second.x) || !std::isfinite(second.y) || !std::isfinite(second.z) )
        {
            continue;
        }

        const LocalVertex firstPosition{
            static_cast<float>(first.x),
            static_cast<float>(first.y),
            static_cast<float>(first.z),
        };
        const LocalVertex secondPosition{
            static_cast<float>(second.x),
            static_cast<float>(second.y),
            static_cast<float>(second.z),
        };
        const auto finitePosition = [](const LocalVertex& position)
        {
            return std::isfinite(position[0])
                && std::isfinite(position[1])
                && std::isfinite(position[2]);
        };
        if ( !finitePosition(firstPosition) || !finitePosition(secondPosition) )
        {
            continue;
        }
        result.push_back(firstPosition);
        result.push_back(secondPosition);
    }
    return result;
}

} // namespace sed::render::frame_ghost
