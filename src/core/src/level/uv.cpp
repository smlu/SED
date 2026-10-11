#include "sed/level/uv.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "sed/text.hpp"

namespace sed::level
{
namespace
{

bool isValidDimensions(const UvTextureDimensions dimensions) noexcept
{
    return std::isfinite(dimensions.width)
        && std::isfinite(dimensions.height)
        && dimensions.width > 0.0
        && dimensions.height > 0.0;
}

bool isMaterialLessSurface(const Surface& surface) noexcept
{
    const std::string_view material = text::trimView(surface.material);
    return material.empty() || text::iequalsAscii(material, "none");
}

std::optional<float> convertUvScale(
    const float scale,
    const UvCoordinateSpace source,
    const UvTextureDimensions dimensions,
    const bool uAxis) noexcept
{
    if (!std::isfinite(scale))
    {
        return std::nullopt;
    }

    const double extent = uAxis ? dimensions.width : dimensions.height;
    const double converted = source == UvCoordinateSpace::TexturePixels
        ? static_cast<double>(scale) / extent
        : static_cast<double>(scale) * extent;
    if (!std::isfinite(converted)
        || converted < -static_cast<double>(std::numeric_limits<float>::max())
        || converted > static_cast<double>(std::numeric_limits<float>::max()))
    {
        return std::nullopt;
    }
    return static_cast<float>(converted);
}

struct PendingSurfaceUvConversion
{
    Surface* surface{};
    std::vector<TexVertex> coordinates{};
    float uScale{};
    float vScale{};
};

LevelUvConversionResult stageSectorUvConversion(
    Sector& sector,
    const std::int32_t sectorIndex,
    const UvCoordinateSpace source,
    const UvCoordinateSpace destination,
    const UvTextureDimensionsResolver& resolveDimensions,
    std::vector<PendingSurfaceUvConversion>& pending)
{
    for (std::size_t surfaceIndex = 0;
         surfaceIndex < sector.surfaces.size();
         ++surfaceIndex)
    {
        Surface& surface = sector.surfaces[surfaceIndex];
        const SurfaceRef surfaceRef{
            sectorIndex,
            static_cast<std::int32_t>(surfaceIndex) };
        if (surface.texVertices.size() != surface.vertices.size())
        {
            return {
                surfaceRef,
                "Texture-coordinate count does not match the polygon corner count." };
        }
        if (source == destination || isMaterialLessSurface(surface))
        {
            if (!std::isfinite(surface.uScale)
                || !std::isfinite(surface.vScale))
            {
                return {
                    surfaceRef,
                    "A UV scale cannot be represented in the destination space." };
            }
            const bool coordinatesAreFinite = std::all_of(
                surface.texVertices.begin(),
                surface.texVertices.end(),
                [](const TexVertex& coordinate)
                {
                    return std::isfinite(coordinate.u)
                        && std::isfinite(coordinate.v);
                });
            if (!coordinatesAreFinite)
            {
                return {
                    surfaceRef,
                    "A texture coordinate cannot be represented in the destination space." };
            }
            continue;
        }

        const std::optional<UvTextureDimensions> dimensions =
            resolveDimensions
                ? resolveDimensions(surface)
                : std::nullopt;
        if (!dimensions || !isValidDimensions(*dimensions))
        {
            return {
                surfaceRef,
                "Texture dimensions are missing or invalid." };
        }

        PendingSurfaceUvConversion converted{};
        converted.surface = &surface;
        const std::optional<float> convertedUScale = convertUvScale(
            surface.uScale,
            source,
            *dimensions,
            true);
        const std::optional<float> convertedVScale = convertUvScale(
            surface.vScale,
            source,
            *dimensions,
            false);
        if (!convertedUScale || !convertedVScale)
        {
            return {
                surfaceRef,
                "A UV scale cannot be represented in the destination space." };
        }
        converted.uScale = *convertedUScale;
        converted.vScale = *convertedVScale;
        converted.coordinates.reserve(surface.texVertices.size());
        for (const TexVertex& coordinate : surface.texVertices)
        {
            const std::optional<TexVertex> value = convertUvCoordinate(
                coordinate,
                source,
                destination,
                *dimensions);
            if (!value)
            {
                return {
                    surfaceRef,
                    "A texture coordinate cannot be represented in the destination space." };
            }
            converted.coordinates.push_back(*value);
        }
        pending.push_back(std::move(converted));
    }
    return {};
}

void publishUvConversions(
    std::vector<PendingSurfaceUvConversion>& pending) noexcept
{
    for (PendingSurfaceUvConversion& converted : pending)
    {
        converted.surface->texVertices = std::move(converted.coordinates);
        converted.surface->uScale = converted.uScale;
        converted.surface->vScale = converted.vScale;
    }
}

} // namespace

std::optional<TexVertex> convertUvCoordinate(
    const TexVertex& coordinate,
    const UvCoordinateSpace source,
    const UvCoordinateSpace destination,
    const UvTextureDimensions dimensions) noexcept
{
    if (!isValidUvCoordinateSpace(source)
        || !isValidUvCoordinateSpace(destination)
        || !std::isfinite(coordinate.u)
        || !std::isfinite(coordinate.v))
    {
        return std::nullopt;
    }
    if (source == destination)
    {
        return coordinate;
    }
    if (!isValidDimensions(dimensions))
    {
        return std::nullopt;
    }

    const double u = source == UvCoordinateSpace::TexturePixels
        ? static_cast<double>(coordinate.u) / dimensions.width
        : static_cast<double>(coordinate.u) * dimensions.width;
    const double v = source == UvCoordinateSpace::TexturePixels
        ? static_cast<double>(coordinate.v) / dimensions.height
        : static_cast<double>(coordinate.v) * dimensions.height;
    if (!std::isfinite(u) || !std::isfinite(v)
        || u < -static_cast<double>(std::numeric_limits<float>::max())
        || u > static_cast<double>(std::numeric_limits<float>::max())
        || v < -static_cast<double>(std::numeric_limits<float>::max())
        || v > static_cast<double>(std::numeric_limits<float>::max()))
    {
        return std::nullopt;
    }

    TexVertex converted = coordinate;
    converted.u = static_cast<float>(u);
    converted.v = static_cast<float>(v);
    return converted;
}

LevelUvConversionResult convertLevelUvCoordinateSpace(
    Level& level,
    const UvCoordinateSpace destination,
    const UvTextureDimensionsResolver& resolveDimensions)
{
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || !isValidUvCoordinateSpace(destination))
    {
        return { {}, "Level has an unknown UV coordinate space." };
    }
    std::vector<PendingSurfaceUvConversion> pending{};
    for (std::size_t sectorIndex = 0;
         sectorIndex < level.sectors.size();
         ++sectorIndex)
    {
        Sector& sector = level.sectors[sectorIndex];
        const LevelUvConversionResult staged = stageSectorUvConversion(
            sector,
            static_cast<std::int32_t>(sectorIndex),
            level.uvCoordinateSpace,
            destination,
            resolveDimensions,
            pending);
        if (!staged)
        {
            return staged;
        }
    }

    publishUvConversions(pending);
    level.uvCoordinateSpace = destination;
    return {};
}

LevelUvConversionResult convertSectorUvCoordinateSpace(
    Sector& sector,
    const std::int32_t sectorIndex,
    const UvCoordinateSpace source,
    const UvCoordinateSpace destination,
    const UvTextureDimensionsResolver& resolveDimensions)
{
    if (!isValidUvCoordinateSpace(source)
        || !isValidUvCoordinateSpace(destination))
    {
        return { {}, "Sector has an unknown UV coordinate space." };
    }
    std::vector<PendingSurfaceUvConversion> pending{};
    const LevelUvConversionResult staged = stageSectorUvConversion(
        sector,
        sectorIndex,
        source,
        destination,
        resolveDimensions,
        pending);
    if (!staged)
    {
        return staged;
    }
    publishUvConversions(pending);
    return {};
}

} // namespace sed::level
