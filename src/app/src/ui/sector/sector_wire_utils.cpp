#include "ui/sector/sector_wire_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_set>

namespace sed::ui::sector
{

std::vector<TVector> documentWireLinesFromSector(const sed::Sector& sector)
{
    std::vector<TVector> lines{};
    std::unordered_set<std::uint64_t> edges{};

    auto vertexToDocument = [&](const std::int32_t index) -> std::optional<TVector>
    {
        if (index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size())
        {
            return std::nullopt;
        }

        const sed::Vertex& vertex = sector.vertices[static_cast<std::size_t>(index)];
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z))
        {
            return std::nullopt;
        }

        return TVector{vertex.x, vertex.y, vertex.z};
    };

    for (const sed::Surface& surface : sector.surfaces)
    {
        if (surface.vertices.size() < 2)
        {
            continue;
        }

        for (std::size_t i = 0; i < surface.vertices.size(); ++i)
        {
            const std::int32_t aIndex = surface.vertices[i];
            const std::int32_t bIndex = surface.vertices[(i + 1u) % surface.vertices.size()];
            if (aIndex < 0 || bIndex < 0)
            {
                continue;
            }

            const std::uint32_t lo = static_cast<std::uint32_t>(std::min(aIndex, bIndex));
            const std::uint32_t hi = static_cast<std::uint32_t>(std::max(aIndex, bIndex));
            const std::uint64_t key = (static_cast<std::uint64_t>(lo) << 32u) | static_cast<std::uint64_t>(hi);
            if (!edges.insert(key).second)
            {
                continue;
            }

            const std::optional<TVector> a = vertexToDocument(aIndex);
            const std::optional<TVector> b = vertexToDocument(bIndex);
            if (!a || !b)
            {
                continue;
            }

            lines.push_back(*a);
            lines.push_back(*b);
        }
    }

    return lines;
}

std::vector<QVector3D> wireLinesFromSector(const sed::Sector& sector)
{
    const auto documentLines = documentWireLinesFromSector(sector);
    std::vector<QVector3D> lines;
    lines.reserve(documentLines.size());
    for (const TVector& point : documentLines)
    {
        lines.emplace_back(static_cast<float>(point.x), static_cast<float>(point.y), static_cast<float>(point.z));
    }
    return lines;
}

} // namespace sed::ui::sector
