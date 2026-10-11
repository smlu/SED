#pragma once

#include "sed/editor/document.hpp"

namespace sed::ui::checks
{

inline constexpr editor::DocumentDirty kCheckerRelevantDirty =
    editor::DocumentDirty::LevelGeometry
    | editor::DocumentDirty::Things
    | editor::DocumentDirty::Lights
    | editor::DocumentDirty::Cogs
    | editor::DocumentDirty::Structure;

constexpr bool checksDirtyIsRelevant(const editor::DocumentDirty dirty) noexcept
{
    return editor::hasAny(dirty, kCheckerRelevantDirty);
}

} // namespace sed::ui::checks
