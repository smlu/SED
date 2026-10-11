#pragma once

#include "sed/level/thing.hpp"

namespace sed::render
{

/** Flags that suppress a Thing in the original runtime renderer. */
[[nodiscard]] inline constexpr bool thingIsRuntimeHidden(const std::int32_t flags) noexcept
{
    return (flags & (TF_INVISIBLE | TF_DISABLED)) != 0;
}

/**
 * Resolves editor Thing visibility from the effective instance/template flags.
 *
 * `level::thingFlags` deliberately performs the same instance-first, inherited
 * template lookup used by the rest of the renderer.  Keeping that lookup here
 * prevents viewport drawing and CPU picking from disagreeing for template-only
 * `thingflags` values.
 */
[[nodiscard]] inline bool thingVisibleInViewport(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const bool showHiddenThings) noexcept
{
    return showHiddenThings
        || !thingIsRuntimeHidden(level::thingFlags(level, templates, thing));
}

} // namespace sed::render
