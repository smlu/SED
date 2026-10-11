#pragma once

#include <cstdint>
#include <vector>

namespace sed::level::detail
{

// Deleted indices are sorted, unique indices in the original sector array.
// Return -1 for a deleted sector; otherwise preserve its surviving identity.
inline std::int32_t remapSectorAfterDeletes(const std::int32_t sectorIndex,
    const std::vector<std::int32_t>& deletedAscending) noexcept
{
    std::int32_t removedBefore = 0;
    for ( const std::int32_t deleted : deletedAscending )
    {
        if ( sectorIndex == deleted )
        {
            return -1;
        }
        if ( deleted < sectorIndex )
        {
            ++removedBefore;
        }
    }
    return sectorIndex - removedBefore;
}

} // namespace sed::level::detail
