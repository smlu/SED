#pragma once

#include <cstdint>
#include <filesystem>
#include <istream>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sed::assets
{

struct SndFile
{
    std::vector<std::string> wavs;
};

/** One ordered playback alternative from a sound-class `.snd` file. */
struct SoundClassEntry
{
    std::string mode{};
    std::string wav{};
    std::uint32_t playFlags{ 0x40U };
    float minRadius{ 0.5f };
    float maxRadius{ 2.5f };
    float maxVolume{ 1.0f };
};

/** Structured sound-class rows, preserving their source order and unknown names. */
struct SoundClassFile
{
    std::vector<SoundClassEntry> entries{};
};

/** Selects recognized engine modes and the external WAV-name buffer limit. */
enum class SoundClassProfile
{
    classic,
    ijim,
};

SndFile loadFromSND(std::istream& in);
SndFile loadFromSND(const std::filesystem::path& path);

/** Engine mode names, in the order used by the selected runtime. */
std::span<const std::string_view> soundClassModeNames(SoundClassProfile profile) noexcept;

/** Loads ordered authoring rows without changing the legacy SND dependency parser. */
SoundClassFile loadFromSoundClass(
    std::istream& in, SoundClassProfile profile = SoundClassProfile::classic);
SoundClassFile loadFromSoundClass(
    const std::filesystem::path& path, SoundClassProfile profile = SoundClassProfile::classic);

/** Serializes every row; WAV names are limited to 31 classic or 121 IJIM bytes. */
std::string saveToSoundClassText(
    const SoundClassFile& file, SoundClassProfile profile = SoundClassProfile::classic);
void saveToSoundClassText(
    std::ostream& out, const SoundClassFile& file,
    SoundClassProfile profile = SoundClassProfile::classic);

} // namespace sed::assets
