#pragma once

#include "sed/editor/selection.hpp"

namespace sed::ui::selection
{

/** Returns whether the current selection is valid for Sector mode cleave. */
bool canCleaveSectorSelection(const editor::Selection& selection) noexcept;

/** Returns whether the current selection is valid for joining sectors. */
bool canJoinSectorSelection(const editor::Selection& selection) noexcept;

/** Returns whether the current selection is valid for merging sectors. */
bool canMergeSectorSelection(const editor::Selection& selection) noexcept;

/** Returns whether the current selection is valid for Surface face cleave. */
bool canCleaveSurfaceSelection(const editor::Selection& selection) noexcept;

/** Returns whether the current selection is valid for Surface edge cleave. */
bool canCleaveSurfaceEdgeSelection(const editor::Selection& selection) noexcept;

/**
 * Reconcile exact Edge-selection owners without discarding an existing Face
 * multi-selection during additive/removal Edge picks. A plain replacement
 * click still replaces the Face context with the surviving exact-edge owners.
 */
void reconcileSurfaceEdgeOwnerSelection(
    editor::Selection& selection,
    const std::unordered_set<SurfaceRef,
        editor::SurfaceRefHash,
        editor::SurfaceRefEq>& edgeOwners,
    bool replaceFaceContext);

} // namespace sed::ui::selection
