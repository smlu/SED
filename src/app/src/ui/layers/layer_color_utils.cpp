#include "ui/layers/layer_color_utils.hpp"

#include <algorithm>

namespace sed::ui::layers
{

QColor defaultLayerColor(const std::size_t /*index*/)
{
    return QColor(255, 255, 255, 255);
}

std::size_t normalizedLayerCount(const std::size_t levelLayerCount) noexcept
{
    return std::max<std::size_t>(1, levelLayerCount);
}

std::vector<bool> initialLayerVisibility(const std::size_t levelLayerCount, const std::string_view persistedVisibility)
{
    std::vector<bool> layerVisible(normalizedLayerCount(levelLayerCount), true);

    for (std::size_t i = 0; i < levelLayerCount && i < layerVisible.size(); ++i)
    {
        layerVisible[i] = (i >= persistedVisibility.size() || persistedVisibility[i] == '1');
    }

    return layerVisible;
}

std::string buildLayerVisibilityString(const std::size_t levelLayerCount, const std::vector<bool> &layerVisible)
{
    std::string result{};
    result.reserve(levelLayerCount);

    for (std::size_t i = 0; i < levelLayerCount; ++i)
    {
        const bool visible = i < layerVisible.size() ? layerVisible[i] : true;
        result.push_back(visible ? '1' : '0');
    }

    return result;
}

void normalizeLayerVisualState(const std::size_t levelLayerCount, std::vector<bool> &layerVisible, std::vector<bool> &layerLocked,
                               std::vector<QColor> &layerColors)
{
    const std::size_t layerCount = normalizedLayerCount(levelLayerCount);

    layerVisible.resize(layerCount, true);
    layerLocked.resize(layerCount, false);

    const std::size_t previousColorCount = layerColors.size();
    if (previousColorCount < layerCount)
    {
        layerColors.reserve(layerCount);
        for (std::size_t i = previousColorCount; i < layerCount; ++i)
        {
            layerColors.push_back(defaultLayerColor(i));
        }
    }
    else if (previousColorCount > layerCount)
    {
        layerColors.resize(layerCount);
    }

    for (std::size_t i = 0; i < layerColors.size(); ++i)
    {
        if (!layerColors[i].isValid())
        {
            layerColors[i] = defaultLayerColor(i);
        }
        layerColors[i].setAlpha(255);
    }
}

LayerVisualState remapLayerVisualState(const std::vector<std::int32_t> &oldToNew,
                                       const std::size_t newLayerCount,
                                       const std::vector<bool> &layerVisible,
                                       const std::vector<bool> &layerLocked,
                                       const std::vector<QColor> &layerColors)
{
    LayerVisualState result{};
    result.visible.assign(newLayerCount, true);
    result.locked.assign(newLayerCount, false);
    result.colors.reserve(newLayerCount);
    for (std::size_t index = 0; index < newLayerCount; ++index)
    {
        result.colors.push_back(defaultLayerColor(index));
    }

    std::vector<bool> destinationAssigned(newLayerCount, false);
    for (std::size_t oldIndex = 0; oldIndex < oldToNew.size(); ++oldIndex)
    {
        const std::int32_t destination = oldToNew[oldIndex];
        if (destination < 0 || static_cast<std::size_t>(destination) >= newLayerCount)
        {
            continue;
        }

        const std::size_t newIndex = static_cast<std::size_t>(destination);
        if (destinationAssigned[newIndex])
        {
            continue;
        }
        destinationAssigned[newIndex] = true;

        result.visible[newIndex] = oldIndex < layerVisible.size() ? layerVisible[oldIndex] : true;
        result.locked[newIndex] = oldIndex < layerLocked.size() ? layerLocked[oldIndex] : false;

        QColor color = oldIndex < layerColors.size() ? layerColors[oldIndex] : defaultLayerColor(oldIndex);
        if (!color.isValid())
        {
            color = defaultLayerColor(oldIndex);
        }
        color.setAlpha(255);
        result.colors[newIndex] = color;
    }

    return result;
}

std::unordered_set<std::int32_t> hiddenLayersFromVisibility(const std::vector<bool> &layerVisible)
{
    std::unordered_set<std::int32_t> hiddenLayers{};
    hiddenLayers.reserve(layerVisible.size());
    for (std::size_t i = 0; i < layerVisible.size(); ++i)
    {
        if (!layerVisible[i])
        {
            hiddenLayers.insert(static_cast<std::int32_t>(i));
        }
    }
    return hiddenLayers;
}

bool isLayerLocked(const std::vector<bool> &layerLocked, const std::int32_t layer) noexcept
{
    if (layer < 0)
    {
        return false;
    }

    const std::size_t index = static_cast<std::size_t>(layer);
    return index < layerLocked.size() && layerLocked[index];
}

bool isLayerVisible(const std::vector<bool> &layerVisible, const std::int32_t layer) noexcept
{
    if (layer < 0)
    {
        return true;
    }

    const std::size_t index = static_cast<std::size_t>(layer);
    return index >= layerVisible.size() || layerVisible[index];
}

bool isLayerEditable(const std::vector<bool> &layerVisible,
                     const std::vector<bool> &layerLocked,
                     const std::int32_t layer) noexcept
{
    return isLayerVisible(layerVisible, layer) && !isLayerLocked(layerLocked, layer);
}

std::int32_t resolvePlacementLayer(const std::size_t levelLayerCount,
                                   const std::int32_t explicitLayer,
                                   const std::int32_t targetSectorLayer,
                                   const std::vector<bool> &layerVisible) noexcept
{
    const std::size_t layerCount = normalizedLayerCount(levelLayerCount);
    const auto validLayer = [layerCount](const std::int32_t layer) noexcept
    {
        return layer >= 0 && static_cast<std::size_t>(layer) < layerCount;
    };
    const auto visibleLayer = [&layerVisible, &validLayer](const std::int32_t layer) noexcept
    {
        if (!validLayer(layer))
        {
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(layer);
        return index >= layerVisible.size() || layerVisible[index];
    };

    if (validLayer(explicitLayer))
    {
        return explicitLayer;
    }
    if (visibleLayer(targetSectorLayer))
    {
        return targetSectorLayer;
    }
    for (std::size_t index = layerCount; index > 0; --index)
    {
        const std::int32_t layer = static_cast<std::int32_t>(index - 1);
        if (visibleLayer(layer))
        {
            return layer;
        }
    }
    if (validLayer(targetSectorLayer))
    {
        return targetSectorLayer;
    }
    return static_cast<std::int32_t>(layerCount - 1);
}

namespace
{

template <typename LayerPredicate>
bool selectionTouchesMatchingLayer(const sed::Level &level,
                                   const sed::editor::Selection &selection,
                                   LayerPredicate&& matchesLayer)
{
    const auto sectorMatches = [&](const std::int32_t sectorIndex) noexcept -> bool {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            return false;
        }

        return matchesLayer(level.sectors[static_cast<std::size_t>(sectorIndex)].layer);
    };

    const auto thingMatches = [&](const std::int32_t thingIndex) noexcept -> bool {
        if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= level.things.size())
        {
            return false;
        }

        return matchesLayer(level.things[static_cast<std::size_t>(thingIndex)].layer);
    };

    const auto lightMatches = [&](const std::int32_t lightIndex) noexcept -> bool {
        if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >= level.lights.size())
        {
            return false;
        }

        return matchesLayer(level.lights[static_cast<std::size_t>(lightIndex)].layer);
    };

    if (std::ranges::any_of(selection.sectors, sectorMatches))
    {
        return true;
    }

    if (std::ranges::any_of(
            selection.surfaces,
            [&](const sed::SurfaceRef &ref) noexcept {
                return sectorMatches(ref.sector);
            }))
    {
        return true;
    }

    if (std::ranges::any_of(
            selection.surfaceVertices,
            [&](const sed::editor::SurfaceVertexRef &ref) noexcept {
                return sectorMatches(ref.surface.sector);
            }))
    {
        return true;
    }

    if (std::ranges::any_of(selection.things, thingMatches))
    {
        return true;
    }

    if (std::ranges::any_of(
            selection.frames,
            [&](const sed::level::FrameRef &ref) noexcept {
                return thingMatches(ref.thing);
            }))
    {
        return true;
    }

    return std::ranges::any_of(selection.lights, lightMatches);
}

} // namespace

bool selectionTouchesLockedLayer(const sed::Level &level,
                                 const sed::editor::Selection &selection,
                                 const std::vector<bool> &layerLocked)
{
    return selectionTouchesMatchingLayer(level, selection,
        [&layerLocked](const std::int32_t layer) noexcept {
            return isLayerLocked(layerLocked, layer);
        });
}

bool selectionTouchesHiddenLayer(const sed::Level &level,
                                 const sed::editor::Selection &selection,
                                 const std::vector<bool> &layerVisible)
{
    return selectionTouchesMatchingLayer(level, selection,
        [&layerVisible](const std::int32_t layer) noexcept {
            return !isLayerVisible(layerVisible, layer);
        });
}

bool selectionTouchesNonEditableLayer(const sed::Level &level,
                                      const sed::editor::Selection &selection,
                                      const std::vector<bool> &layerVisible,
                                      const std::vector<bool> &layerLocked)
{
    return selectionTouchesMatchingLayer(level, selection,
        [&layerVisible, &layerLocked](const std::int32_t layer) noexcept {
            return !isLayerEditable(layerVisible, layerLocked, layer);
        });
}

} // namespace sed::ui::layers
