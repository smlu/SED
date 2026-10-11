#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sed::assets
{
struct Model3do;
}

namespace sed::render::frame_ghost
{

inline constexpr std::size_t kMaximumGhostLineBufferBytes = 16u * 1024u * 1024u;
inline constexpr std::size_t kMaximumGhostEdgeCount = 512u * 1024u;
inline constexpr std::size_t kMaximumGhostSourceEdgeVisits = 8u * 1024u * 1024u;

/** An undirected, validated edge in one source 3DO mesh. */
struct EdgeRef
{
    std::size_t meshIndex{};
    std::uint32_t firstVertex{};
    std::uint32_t secondVertex{};

    constexpr bool operator==(const EdgeRef&) const noexcept = default;
};

/** Validated line topology, retaining mesh boundaries and first-seen order. */
struct ModelTopology
{
    std::vector<EdgeRef> edges{};
    std::size_t sourceFaceEdgeVisits{};
    bool withinBudget{true};
};

/** One model-local endpoint for a persistent GL_LINES vertex buffer. */
using LocalVertex = std::array<float, 3u>;

/**
 * Build the exact per-mesh wire edge list used by Frame ghosts.
 *
 * Face vertex indices are validated against their own mesh and each
 * undirected vertex pair is emitted once per mesh. Vertices are deliberately
 * not merged across meshes, even when their coordinates happen to match.
 */
[[nodiscard]] ModelTopology buildModelTopology(
    const assets::Model3do& model,
    std::size_t maximumEdgeCount = kMaximumGhostEdgeCount);

/** Expand deduplicated refs once into interleaved vec3-ready local endpoints. */
[[nodiscard]] std::vector<LocalVertex> buildLocalLineVertices(
    const assets::Model3do& model,
    const ModelTopology& topology);

} // namespace sed::render::frame_ghost
