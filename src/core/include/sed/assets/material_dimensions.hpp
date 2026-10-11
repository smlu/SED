#pragma once

#include <optional>
#include <string_view>

#include "sed/level/uv.hpp"

namespace sed::assets
{

class ResourceLocator;

/** Resolves the first MAT cel dimensions used by authored surface UVs.
 *
 * Missing, malformed, or dimensionless resources return no value. Callers
 * must preserve the source coordinate space rather than substituting a fake
 * texture size.
 */
std::optional<level::UvTextureDimensions> resolveMaterialTextureDimensions(
    const ResourceLocator& resources,
    std::string_view material) noexcept;

/** Normalizes a legacy pixel-space Level through its referenced MAT sizes.
 *
 * The conversion is failure atomic. Missing or malformed resources leave the
 * Level and its coordinate-space tag unchanged.
 */
level::LevelUvConversionResult normalizeLegacyLevelUvCoordinates(
    Level& level,
    const ResourceLocator& resources) noexcept;

} // namespace sed::assets
