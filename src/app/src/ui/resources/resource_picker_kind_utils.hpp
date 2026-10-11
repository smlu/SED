#pragma once

#include <optional>
#include <string_view>

#include "sed/values.hpp"
#include "ui/resources/resource_picker.hpp"

namespace sed::ui
{

/**
 * Maps a COG value type to the resource picker that can edit that value.
 */
std::optional<ResourcePickerKind> resourcePickerKindForCogType(CogType type) noexcept;

/**
 * True when a COG value line edit should still run its double-click action even
 * if the current editor text cannot be parsed back into the value.
 */
bool cogTypeAllowsDoubleClickAfterParseFailure(CogType type) noexcept;

/**
 * True for the COG value types handled by MainWindow's double-click resource
 * picker path.
 */
bool cogTypeUsesDoubleClickResourcePicker(CogType type) noexcept;

/**
 * True when the COG value editor line edit needs the MainWindow event-filter
 * fallback for double-click navigation or resource picking.
 */
bool cogTypeNeedsDoubleClickLineEditFallback(CogType type) noexcept;

/**
 * Maps an adjoin/resource value type to the resource picker that can edit that value.
 */
std::optional<ResourcePickerKind> resourcePickerKindForAdjType(AdjType type) noexcept;

/**
 * Returns the legacy JLH category file used by a picker for one game project.
 *
 * The original editor ships independent material, sound, and template trees
 * for JKDF2, MOTS, and IJIM. Keeping this mapping in one policy function avoids
 * accidentally presenting one game's categories in another project.
 */
std::optional<std::string_view> resourcePickerCategoryFileName(
    ResourcePickerKind kind,
    ProjectType project) noexcept;

} // namespace sed::ui
