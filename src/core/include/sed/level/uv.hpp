#pragma once

#include <functional>
#include <optional>
#include <string>

#include "sed/level.hpp"

namespace sed::level
{

struct UvTextureDimensions
{
    double width{};
    double height{};
};

using UvTextureDimensionsResolver = std::function<
    std::optional<UvTextureDimensions>(const Surface&)>;

/** Returns whether a coordinate-space tag has a defined storage contract. */
constexpr bool isValidUvCoordinateSpace(
    const UvCoordinateSpace space) noexcept
{
    return space == UvCoordinateSpace::TexturePixels
        || space == UvCoordinateSpace::Normalized;
}

struct LevelUvConversionResult
{
    std::optional<SurfaceRef> failedSurface{};
    std::string error{};

    explicit operator bool() const noexcept
    {
        return error.empty();
    }
};

/** Converts one Sector's coordinates and UV scales without changing a Level
 * coordinate-space tag. This is used at construction/import boundaries before
 * newly generated geometry is published into its owning Level. A same-space
 * request validates finite coordinates, scales, and corner counts without
 * resolving dimensions. A cross-space request resolves dimensions only for
 * material-bearing surfaces; surfaces with no material retain their authored
 * UV values.
 */
LevelUvConversionResult convertSectorUvCoordinateSpace(
    Sector& sector,
    std::int32_t sectorIndex,
    UvCoordinateSpace source,
    UvCoordinateSpace destination,
    const UvTextureDimensionsResolver& resolveDimensions);

/** Converts one UV while preserving its baked-light color exactly.
 *
 * Invalid spaces, dimensions, non-finite input, or a non-finite float result
 * return no value. Signed and multi-tile coordinates are intentionally valid.
 */
std::optional<TexVertex> convertUvCoordinate(
    const TexVertex& coordinate,
    UvCoordinateSpace source,
    UvCoordinateSpace destination,
    UvTextureDimensions dimensions) noexcept;

/** Converts every authored Level UV as one failure-atomic transaction.
 *
 * For cross-space conversion, the resolver is called for each material-bearing
 * Surface whose texture dimensions are needed. A surface with no material is
 * validated but keeps its authored coordinates and scales; an empty corner
 * array does not by itself make a surface material-less. A same-space request
 * validates finite coordinates, scales, and corner counts without resolving
 * dimensions. Coordinates and uScale/vScale are converted or validated
 * together. A missing/invalid texture size or unrepresentable value leaves
 * both the data and the Level coordinate-space tag unchanged.
 */
LevelUvConversionResult convertLevelUvCoordinateSpace(
    Level& level,
    UvCoordinateSpace destination,
    const UvTextureDimensionsResolver& resolveDimensions);

} // namespace sed::level
