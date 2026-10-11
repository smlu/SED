#pragma once

#include <cstdint>

#include "sed/level.hpp"

namespace sed::level
{

bool levelDeleteSectorVertex(Level& level, std::int32_t sector, std::int32_t vertex, bool silent = false);

/**
 * Removes a sector vertex without topology safeguards.  Every reference is
 * removed and remaining indices are compacted, but affected polygons may have
 * fewer than three vertices or otherwise become invalid.  Editor callers must
 * obtain explicit user confirmation before using this recovery-oriented path.
 */
bool levelForceDeleteSectorVertex(Level& level, std::int32_t sector, std::int32_t vertex);

bool levelDeleteThing(Level& level, std::int32_t thing) noexcept;
bool levelDeleteLight(Level& level, std::int32_t light) noexcept;
bool levelDeleteCog(Level& level, std::int32_t cog) noexcept;

} // namespace sed::level
