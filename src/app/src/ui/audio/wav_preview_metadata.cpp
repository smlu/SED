#include "ui/audio/wav_preview_metadata.hpp"

#include "sed/assets/indy_wv.hpp"
#include "sed/assets/wav.hpp"

#include <limits>

namespace sed::ui::audio
{
namespace
{

std::optional<assets::PcmWave> decodePreviewWave(
    const std::span<const std::byte> bytes)
{
    if (!assets::isIndyWvSound(bytes))
    {
        return assets::decodePcmWave(bytes);
    }

    auto wavBytes = assets::convertIndyWvToWav(bytes);
    return wavBytes
        ? assets::decodePcmWave(*wavBytes)
        : std::nullopt;
}

std::size_t bytesPerSample(const assets::PcmSampleFormat format) noexcept
{
    switch (format)
    {
        case assets::PcmSampleFormat::unsigned8: return 1U;
        case assets::PcmSampleFormat::signed16:  return 2U;
        case assets::PcmSampleFormat::signed32:
        case assets::PcmSampleFormat::float32:   return 4U;
    }
    return 0U;
}

} // namespace

std::optional<WavPreviewMetadata> readWavPreviewMetadata(
    const std::span<const std::byte> bytes)
{
    const auto wave = decodePreviewWave(bytes);
    if (!wave)
    {
        return std::nullopt;
    }

    const std::size_t sampleSize = bytesPerSample(wave->sampleFormat);
    if (sampleSize == 0U || wave->channelCount == 0U || wave->sampleRate == 0U)
    {
        return std::nullopt;
    }

    const std::size_t frameSize = sampleSize * wave->channelCount;
    if (frameSize == 0U || (wave->samples.size() % frameSize) != 0U)
    {
        return std::nullopt;
    }

    const auto frameCount = static_cast<std::uint64_t>(wave->samples.size() / frameSize);
    constexpr std::uint64_t kMicrosecondsPerSecond = 1'000'000U;
    if (frameCount > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
            / kMicrosecondsPerSecond)
    {
        return std::nullopt;
    }

    const auto durationCount = static_cast<std::int64_t>(
        frameCount * kMicrosecondsPerSecond / wave->sampleRate);
    return WavPreviewMetadata{
        .sampleRate   = wave->sampleRate,
        .channelCount = wave->channelCount,
        .frameCount   = frameCount,
        .duration     = std::chrono::microseconds{durationCount},
    };
}

std::string describeWavChannels(const std::uint16_t channelCount)
{
    switch (channelCount)
    {
        case 1U: return "Mono";
        case 2U: return "Stereo";
        default: return std::to_string(channelCount) + " channels";
    }
}

} // namespace sed::ui::audio
