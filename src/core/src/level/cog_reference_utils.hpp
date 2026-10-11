#pragma once

#include "sed/level.hpp"

#include <optional>
#include <span>
#include <vector>

namespace sed::level::detail
{
struct CapturedCogReference
{
    std::size_t cogIndex{};
    std::size_t valueIndex{};
    CogType type{CogType::unk};
    std::int32_t sector{-1};
    SurfaceRef surface{-1, -1};
    bool remapped{};
    std::int32_t originalSectorNumber{-1};
    std::int32_t originalSurfaceNumber{-1};
    bool invalidPreferredText{};
};

inline std::optional<SurfaceRef> resolveCogSurfaceReference(const Level& level, const CogValue& value,
                                                            bool* const invalidPreferredText = nullptr)
{
    if ( invalidPreferredText )
    {
        *invalidPreferredText = false;
    }
    std::int32_t globalSurface = value.intValue;
    if ( value.stringValue.find_first_of(" \t\r\n") != std::string::npos )
    {
        CogValue parsed = value;
        parsed.intValue = -1;
        if ( parsed.parseJed(level, value.stringValue) )
        {
            globalSurface = parsed.intValue;
        }
        else if ( invalidPreferredText )
        {
            *invalidPreferredText = true;
        }
    }
    return level.tryGetSurfaceRefByGlobalSurfaceNumber(globalSurface);
}

inline std::vector<CapturedCogReference> captureCogReferences(const Level& level)
{
    std::vector<CapturedCogReference> references{};
    for ( std::size_t cogIndex = 0; cogIndex < level.cogs.size(); ++cogIndex )
    {
        const Cog& cog = level.cogs[cogIndex];
        for ( std::size_t valueIndex = 0; valueIndex < cog.values.size(); ++valueIndex )
        {
            const CogValue& value = cog.values[valueIndex];
            if ( value.type != ValueType::ptr )
            {
                continue;
            }
            if ( value.cogType == CogType::sec && value.intValue >= 0 &&
                 static_cast<std::size_t>(value.intValue) < level.sectors.size() )
            {
                references.push_back(
                    CapturedCogReference{cogIndex,
                                         valueIndex,
                                         CogType::sec,
                                         value.intValue,
                                         {},
                                         false,
                                         level.sectors[static_cast<std::size_t>(value.intValue)].num,
                                         -1});
            }
            else if ( value.cogType == CogType::surface )
            {
                bool invalidPreferredText = false;
                if ( const std::optional<SurfaceRef> surface =
                         resolveCogSurfaceReference(level, value, &invalidPreferredText) )
                {
                    references.push_back(
                        CapturedCogReference{cogIndex, valueIndex, CogType::surface, -1, *surface, false,
                                             level.sectors[static_cast<std::size_t>(surface->sector)].num,
                                             level.sectors[static_cast<std::size_t>(surface->sector)]
                                                 .surfaces[static_cast<std::size_t>(surface->surface)]
                                                 .num,
                                             invalidPreferredText});
                }
            }
        }
    }
    return references;
}

// References stay semantic while a geometry transaction changes flattened
// surface numbering. All allocations occur during capture/copy; remapping and
// final publication cannot throw or expose a half-published COG collection.
class GeometryCogReferences
{
  public:
    GeometryCogReferences() = default;
    explicit GeometryCogReferences(const Level& level) : references_(captureCogReferences(level)) {}

    void splitSector(const std::int32_t sector,
                     const std::span<const std::optional<SurfaceRef>> remap) noexcept
    {
        for ( auto& reference : references_ )
        {
            if ( reference.type != CogType::surface || reference.surface.sector != sector )
            {
                continue;
            }
            const auto old = reference.surface;
            const auto index = static_cast<std::size_t>(old.surface);
            reference.surface = index < remap.size() && remap[index] ? *remap[index] : SurfaceRef{-1, -1};
            reference.remapped = reference.remapped || reference.surface != old;
        }
    }

    void swapSurfaceOwners(const std::int32_t first, const std::int32_t second) noexcept
    {
        for ( auto& reference : references_ )
        {
            if ( reference.type != CogType::surface )
            {
                continue;
            }
            auto& owner = reference.surface.sector;
            if ( owner == first || owner == second )
            {
                owner = owner == first ? second : first;
                reference.remapped = true;
            }
        }
    }

    void eraseSector(const std::int32_t removed) noexcept
    {
        for ( auto& reference : references_ )
        {
            auto& owner = reference.type == CogType::sec ? reference.sector : reference.surface.sector;
            if ( owner >= removed )
            {
                owner = owner == removed ? -1 : owner - 1;
                reference.remapped = true;
            }
        }
    }

    void eraseSurface(const SurfaceRef removed) noexcept
    {
        for ( auto& reference : references_ )
        {
            if ( reference.type != CogType::surface || reference.surface.sector != removed.sector )
            {
                continue;
            }
            auto& face = reference.surface.surface;
            if ( face >= removed.surface )
            {
                face = face == removed.surface ? -1 : face - 1;
                reference.remapped = true;
            }
        }
    }

    void publish(Level& level) const noexcept
    {
        for ( const auto& reference : references_ )
        {
            auto& value = level.cogs[reference.cogIndex].values[reference.valueIndex];
            const auto index =
                reference.type == CogType::sec
                    ? reference.sector
                    : level.getGlobalSurfaceNumber(reference.surface.sector, reference.surface.surface);
            const bool displayChanged =
                index >= 0 &&
                (reference.type == CogType::sec
                     ? level.sectors[static_cast<std::size_t>(reference.sector)].num !=
                           reference.originalSectorNumber
                     : level.sectors[static_cast<std::size_t>(reference.surface.sector)].num !=
                               reference.originalSectorNumber ||
                           level.sectors[static_cast<std::size_t>(reference.surface.sector)]
                                   .surfaces[static_cast<std::size_t>(reference.surface.surface)]
                                   .num != reference.originalSurfaceNumber);
            if ( reference.remapped || value.intValue != index || displayChanged ||
                 reference.invalidPreferredText )
            {
                value.intValue = index;
                value.stringValue.clear();
            }
        }
    }

  private:
    std::vector<CapturedCogReference> references_;
};
} // namespace sed::level::detail
