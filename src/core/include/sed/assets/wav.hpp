#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace sed::assets
{

/** Sample encodings accepted by the editor's cross-platform audio stream. */
enum class PcmSampleFormat : std::uint8_t
{
    unsigned8,
    signed16,
    signed32,
    float32,
};

/** Decoded PCM payload and the format needed to submit it to an audio device. */
struct PcmWave
{
    std::uint32_t sampleRate{};
    std::uint16_t channelCount{};
    PcmSampleFormat sampleFormat{PcmSampleFormat::signed16};
    std::vector<std::byte> samples{};
};

/**
 * Reads an uncompressed RIFF/WAVE stream. PCM24 samples are widened to PCM32;
 * malformed, truncated, or compressed streams are rejected.
 */
std::optional<PcmWave> decodePcmWave(std::span<const std::byte> bytes);

} // namespace sed::assets
