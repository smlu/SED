#pragma once

#include <cstdint>

#include "sed/editor/document.hpp"

namespace sed::ui::history_detail
{

inline constexpr std::uint32_t kKnownHistoryDirtyFlags =
    static_cast<std::uint32_t>(editor::DocumentDirty::LevelGeometry)
    | static_cast<std::uint32_t>(editor::DocumentDirty::Things)
    | static_cast<std::uint32_t>(editor::DocumentDirty::Lights)
    | static_cast<std::uint32_t>(editor::DocumentDirty::Cogs)
    | static_cast<std::uint32_t>(editor::DocumentDirty::Structure);

inline constexpr std::uint32_t kSurfaceTopologyHistoryDirtyFlags =
    static_cast<std::uint32_t>(editor::DocumentDirty::Structure)
    | static_cast<std::uint32_t>(editor::DocumentDirty::LevelGeometry);

inline constexpr bool hasUnsupportedHistoryDirtyBits(
    const std::uint32_t dirtyFlags) noexcept
{
    return dirtyFlags != static_cast<std::uint32_t>(editor::DocumentDirty::All)
        && (dirtyFlags & ~kKnownHistoryDirtyFlags) != 0u;
}

inline constexpr bool isUnknownHistoryDirtyFlags(
    const std::uint32_t dirtyFlags) noexcept
{
    return dirtyFlags == static_cast<std::uint32_t>(editor::DocumentDirty::None)
        || hasUnsupportedHistoryDirtyBits(dirtyFlags);
}

inline constexpr bool historyDirtyMayChangeSurfaceTopology(
    const std::uint32_t dirtyFlags) noexcept
{
    return isUnknownHistoryDirtyFlags(dirtyFlags)
        || (dirtyFlags & kSurfaceTopologyHistoryDirtyFlags) != 0u;
}

inline constexpr bool isKnownTopologyHistoryTransition(
    const std::uint32_t dirtyFlags) noexcept
{
    return !hasUnsupportedHistoryDirtyBits(dirtyFlags)
        && (dirtyFlags & kSurfaceTopologyHistoryDirtyFlags)
            == kSurfaceTopologyHistoryDirtyFlags;
}

inline constexpr std::uint32_t normalizeUnsupportedHistoryDirtyFlags(
    const std::uint32_t dirtyFlags) noexcept
{
    return hasUnsupportedHistoryDirtyBits(dirtyFlags)
        ? static_cast<std::uint32_t>(editor::DocumentDirty::None)
        : dirtyFlags;
}

} // namespace sed::ui::history_detail
