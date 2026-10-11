#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "sed/assets/resource_locator.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/level.hpp"

namespace sed::thing
{

/**
 * Returns the stored string for a template value, preserving the legacy
 * Value::asString fallback used throughout thing/template lookup code.
 */
std::string tplValueString(const TplValue* value);

/**
 * Normalizes a 3DO model resource name for lookup and cache keys.
 *
 * Empty and "none" names return an empty string. Non-empty names are trimmed
 * and receive a ".3do" extension when one is not already present.
 */
std::string normalizedModel3doName(std::string_view modelName);

/**
 * Normalizes an SPR resource name for lookup and cache keys.
 *
 * Empty and "none" names return an empty string. Non-empty names are trimmed
 * and receive a ".spr" extension when one is not already present.
 */
std::string normalizedSpriteName(std::string_view spriteName);

/**
 * Returns the most useful template key for a placed thing.
 *
 * The editor stores both templateName and name. Most template-resource lookups
 * should prefer templateName, but older data can leave that empty.
 */
std::string thingTemplateKey(const Thing& thing);

/** Finds the direct template bbox used by Thing rendering and placement. */
std::optional<ThingBox> templateThingBox(
    const Level& level,
    const assets::Templates* templates,
    std::string_view templateName) noexcept;

/**
 * Applies a template picker result without erasing authored local parameters.
 * When the selected template exists, its bbox replaces the stale per-instance
 * placement copy so missing-resource bounds and framing match the new visual.
 * The optional designer preference may also replace the Thing's name; the
 * default preserves authored names.
 */
bool applyThingTemplateSelection(
    Thing& thing,
    std::string templateName,
    const std::optional<ThingBox>& templateBox,
    bool overrideThingName = false);

/**
 * Finds and normalizes the model3d/model3do resource name for a thing template.
 */
std::string templateModel3doName(
    const Level& level,
    const assets::Templates* templates,
    std::string_view thingName);

/**
 * Finds and normalizes the model3d/model3do resource name for a placed thing.
 */
std::string thingModel3doName(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing);

/**
 * Returns true when a template resolves to a usable model3d/model3do resource.
 */
bool templateHasModel3dParam(
    const Level& level,
    const assets::Templates* templates,
    std::string_view thingName);

/**
 * Returns true when a placed thing resolves to a usable model3d/model3do resource.
 */
bool thingHasModel3dParam(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing);

/**
 * Finds and normalizes the sprite resource name for a thing template.
 */
std::string templateSpriteName(
    const Level& level,
    const assets::Templates* templates,
    std::string_view thingName);

/**
 * True when either the model or thing key advertises rail/track behavior.
 *
 * Rail/track models need connector extraction instead of generic box snapping.
 */
bool isRailOrTrackThingKey(std::string_view modelName, std::string_view thingName);

/**
 * Loads a 3DO model resource and returns its local vertex bounds.
 */
std::optional<ThingBox> loadModel3doBox(
    const assets::ResourceLocator& resources,
    std::string_view modelName);

/**
 * Loads an SPR resource and returns the editor's local sprite preview bounds.
 */
std::optional<ThingBox> loadSpriteBox(
    const assets::ResourceLocator& resources,
    std::string_view spriteName);

} // namespace sed::thing
