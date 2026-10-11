#include "sed/assets/material_dimensions.hpp"

#include "sed/assets/mat.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/text.hpp"

#include <cstdint>
#include <exception>
#include <unordered_map>

namespace sed::assets
{

std::optional<level::UvTextureDimensions> resolveMaterialTextureDimensions(
    const ResourceLocator& resources,
    const std::string_view material) noexcept
{
    const std::string_view trimmed = text::trimView(material);
    if (trimmed.empty() || text::iequalsAscii(trimmed, "none"))
    {
        return std::nullopt;
    }

    try
    {
        std::optional<std::vector<std::byte>> bytes =
            resources.tryReadBinaryWithExt(trimmed, ".mat");
        if (!bytes)
        {
            return std::nullopt;
        }

        const MatFile mat = MatFile::parse(std::move(*bytes));
        if (mat.celCount() == 0)
        {
            return std::nullopt;
        }

        const MatFile::Cel& cel = mat.cel(0);
        if (cel.width <= 0 || cel.height <= 0)
        {
            return std::nullopt;
        }

        return level::UvTextureDimensions{
            static_cast<double>(cel.width),
            static_cast<double>(cel.height) };
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

level::LevelUvConversionResult normalizeLegacyLevelUvCoordinates(
    Level& level,
    const ResourceLocator& resources) noexcept
{
    std::unordered_map<std::string,
        std::optional<level::UvTextureDimensions>> cache{};
    return level::convertLevelUvCoordinateSpace(
        level,
        UvCoordinateSpace::Normalized,
        [&](const Surface& surface)
            -> std::optional<level::UvTextureDimensions>
        {
            const std::string key = text::lowerAscii(
                text::trimView(surface.material));
            if (const auto found = cache.find(key); found != cache.end())
            {
                return found->second;
            }

            std::optional<level::UvTextureDimensions> dimensions =
                resolveMaterialTextureDimensions(resources, surface.material);
            cache.emplace(key, dimensions);
            return dimensions;
        });
}

} // namespace sed::assets
