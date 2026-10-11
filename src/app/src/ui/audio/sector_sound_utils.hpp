#pragma once

#include "sed/geometry.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace sed
{
struct Level;
}

namespace sed::ui::audio
{

/** Ambient sound selected for the sector containing the viewport camera. */
struct SectorSoundRequest
{
    std::int32_t sectorIndex{-1};
    std::string soundName{};
    double volume{};
};

/** Selects the independent mute preference for the active viewport pipeline. */
bool viewportSoundMuted(bool editorMuted, bool previewMuted, bool previewPipeline) noexcept;

/** Returns the ambient sound authored on an already-resolved camera sector. */
std::optional<SectorSoundRequest> sectorSoundForSector(
    const Level& level,
    std::int32_t sectorIndex,
    bool muted) noexcept;

/**
 * Finds the camera sector and returns its ambient sound, if playback is
 * enabled and the sector has a sound assigned. Returned volume is clamped to
 * the range accepted by the platform sound player.
 */
std::optional<SectorSoundRequest> sectorSoundAtPosition(
    const Level& level,
    const TVector& position,
    bool muted) noexcept;

} // namespace sed::ui::audio
