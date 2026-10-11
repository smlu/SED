#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <QColor>

#include "sed/editor/selection.hpp"

namespace sed::ui::layers
{

struct LayerVisualState
{
    std::vector<bool> visible{};
    std::vector<bool> locked{};
    std::vector<QColor> colors{};
};

/** Returns opaque white, the editor fallback for a layer without an explicit color. */
QColor defaultLayerColor(std::size_t index);

/** Returns at least one visual layer even when the level has no persisted layer
 * list. */
std::size_t normalizedLayerCount(std::size_t levelLayerCount) noexcept;

/** Builds layer visibility flags from the level's persisted lvis string,
 * defaulting missing layers visible. */
std::vector<bool> initialLayerVisibility(std::size_t levelLayerCount, std::string_view persistedVisibility);

/** Converts layer visibility flags back to the persisted lvis string format. */
std::string buildLayerVisibilityString(std::size_t levelLayerCount, const std::vector<bool> &layerVisible);

/** Normalizes visibility, lock, and color arrays to the visual layer count used
 * by the editor. */
void normalizeLayerVisualState(std::size_t levelLayerCount, std::vector<bool> &layerVisible, std::vector<bool> &layerLocked, std::vector<QColor> &layerColors);

/** Reindexes parallel editor-only layer state with the same old-index to
 * new-index map used for Level::remapLayers(). Deleted entries are omitted;
 * newly introduced destinations receive visible/unlocked/default-color
 * values. */
LayerVisualState remapLayerVisualState(const std::vector<std::int32_t> &oldToNew,
                                       std::size_t newLayerCount,
                                       const std::vector<bool> &layerVisible,
                                       const std::vector<bool> &layerLocked,
                                       const std::vector<QColor> &layerColors);

/** Returns the layer indices that should be hidden in the viewport. */
std::unordered_set<std::int32_t> hiddenLayersFromVisibility(const std::vector<bool> &layerVisible);

/** Returns whether a layer index is locked, treating invalid indices as
 * unlocked. */
bool isLayerLocked(const std::vector<bool> &layerLocked, std::int32_t layer) noexcept;

/** Returns whether a layer index is visible, treating invalid or missing
 * indices as visible. */
bool isLayerVisible(const std::vector<bool> &layerVisible, std::int32_t layer) noexcept;

/** Returns whether an entity on a layer can be edited. Invalid/missing layer
 * state retains the editor's visible/unlocked fallback. */
bool isLayerEditable(const std::vector<bool> &layerVisible,
                     const std::vector<bool> &layerLocked,
                     std::int32_t layer) noexcept;

/** Resolves the layer for a newly placed entity. An explicit valid layer wins;
 * automatic placement prefers the visible target-sector layer, then the last
 * visible layer. If every layer is hidden, the valid target layer is retained
 * before falling back to the final normalized layer. */
std::int32_t resolvePlacementLayer(std::size_t levelLayerCount,
                                   std::int32_t explicitLayer,
                                   std::int32_t targetSectorLayer,
                                   const std::vector<bool> &layerVisible) noexcept;

/** Returns whether any selected editable entity belongs to a locked layer. */
bool selectionTouchesLockedLayer(const sed::Level &level, const sed::editor::Selection &selection, const std::vector<bool> &layerLocked);

/** Returns whether any selected editable entity belongs to a hidden layer. */
bool selectionTouchesHiddenLayer(const sed::Level &level, const sed::editor::Selection &selection, const std::vector<bool> &layerVisible);

/** Returns whether any selected editable entity belongs to a hidden or locked
 * layer. This is the central mutation-availability policy for selections. */
bool selectionTouchesNonEditableLayer(const sed::Level &level,
                                      const sed::editor::Selection &selection,
                                      const std::vector<bool> &layerVisible,
                                      const std::vector<bool> &layerLocked);

} // namespace sed::ui::layers
