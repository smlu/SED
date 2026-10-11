#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace sed::ui::audio
{

/** Decoded properties shown by the resource picker's WAV preview. */
struct WavPreviewMetadata
{
    std::uint32_t sampleRate{};
    std::uint16_t channelCount{};
    std::uint64_t frameCount{};
    std::chrono::microseconds duration{};
};

/**
 * Decodes an ordinary RIFF/WAVE or IJIM INDIWV resource through the same
 * conversion and PCM parser used for playback, then derives its duration.
 * Those core decoders bound decoded PCM to 512 MiB; malformed, unsupported,
 * or internally inconsistent streams are rejected.
 */
[[nodiscard]] std::optional<WavPreviewMetadata> readWavPreviewMetadata(
    std::span<const std::byte> bytes);

/** Returns "Mono", "Stereo", or a localized-neutral "N channels" label. */
[[nodiscard]] std::string describeWavChannels(std::uint16_t channelCount);

} // namespace sed::ui::audio
