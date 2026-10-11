#pragma once

#include "sed/level.hpp"

namespace sed::ui::viewport
{

/**
 * Returns whether the viewport fog-rendering option is meaningful for a project.
 *
 * IJIM is the only supported project type with level fog data and the legacy
 * DirectX table-fog behavior mirrored by the OpenGL renderer.
 */
inline bool viewportFogRenderOptionAvailable(const ProjectType project) noexcept
{
    return project == ProjectType::IJIM;
}

} // namespace sed::ui::viewport
