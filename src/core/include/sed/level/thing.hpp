#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "sed/level.hpp"

namespace sed::assets
{
class Templates;
} // namespace sed::assets

namespace sed::level
{

const TplValue* thingFindValue(const Level& level, const Thing& thing, const std::string_view name) noexcept;
const TplValue* thingFindValue(const Level& level, const assets::Templates* templates, const Thing& thing, const std::string_view name) noexcept;

// Counts `frame` values on a thing (values with name "frame" and ValueType::frame).
std::size_t thingCountFrames(const Thing& thing) noexcept;

// Ensures `numframes` matches the authored frame count and precedes every
// `frame`, as required by the runtime's allocate-then-append parser.
void thingSyncNumFrames(Thing& thing) noexcept;
void thingSyncNumFrames(ThingTemplate& thingTemplate) noexcept;

std::int32_t thingFlags(const Level& level, const Thing& thing) noexcept;
std::int32_t thingFlags(const Level& level, const assets::Templates* templates, const Thing& thing) noexcept;

ColorF thingLight(const Level& level, const Thing& thing) noexcept;
ColorF thingLight(const Level& level, const assets::Templates* templates, const Thing& thing) noexcept;

} // namespace sed::level
