#include "ui/audio/sector_sound_utils.hpp"

#include "sed/level.hpp"
#include "sed/level/geometry.hpp"

#include <algorithm>

namespace sed::ui::audio
{

bool viewportSoundMuted(
    const bool editorMuted,
    const bool previewMuted,
    const bool previewPipeline) noexcept
{
    return previewPipeline ? previewMuted : editorMuted;
}

std::optional<SectorSoundRequest> sectorSoundAtPosition(
    const Level& level,
    const TVector& position,
    const bool muted) noexcept
{
    if (muted)
    {
        return std::nullopt;
    }

    const std::int32_t sectorIndex = level::levelFindSectorForXYZ(
        level,
        position.x,
        position.y,
        position.z);
    return sectorSoundForSector(level, sectorIndex, false);
}

std::optional<SectorSoundRequest> sectorSoundForSector(
    const Level& level,
    const std::int32_t sectorIndex,
    const bool muted) noexcept
{
    if (muted)
    {
        return std::nullopt;
    }

    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        return std::nullopt;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    if (sector.sound.empty())
    {
        return std::nullopt;
    }

    return SectorSoundRequest{
        .sectorIndex = sectorIndex,
        .soundName   = sector.sound,
        .volume      = std::clamp(sector.soundVolume, 0.0, 1.0),
    };
}

} // namespace sed::ui::audio
