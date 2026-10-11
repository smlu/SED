#include "sed/assets/wav.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <utility>

namespace sed::assets
{
namespace
{

constexpr std::uint16_t kPcmFormat = 1U;
constexpr std::uint16_t kFloatFormat = 3U;
constexpr std::uint16_t kExtensibleFormat = 0xFFFEU;
constexpr std::size_t kBaseFormatSize = 16U;
constexpr std::size_t kExtensibleFormatSize = 40U;
constexpr std::size_t kMaxPcmBytes = 512U * 1024U * 1024U;
constexpr std::array<std::byte, 14> kWaveSubFormatGuidSuffix{
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x10}, std::byte{0x00}, std::byte{0x80}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xAA}, std::byte{0x00}, std::byte{0x38},
    std::byte{0x9B}, std::byte{0x71},
};

bool hasRange(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    const std::size_t size) noexcept
{
    return offset <= bytes.size() && size <= bytes.size() - offset;
}

bool hasTag(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    const std::string_view tag) noexcept
{
    if (!hasRange(bytes, offset, tag.size()))
    {
        return false;
    }
    for (std::size_t i = 0; i < tag.size(); ++i)
    {
        if (std::to_integer<std::uint8_t>(bytes[offset + i])
            != static_cast<std::uint8_t>(tag[i]))
        {
            return false;
        }
    }
    return true;
}

std::uint16_t readU16Le(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset]))
        | static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(bytes[offset + 1U]) << 8U);
}

std::uint32_t readU32Le(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset]))
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3U])) << 24U);
}

struct WaveFormat final
{
    std::uint16_t encoding{};
    std::uint16_t channelCount{};
    std::uint32_t sampleRate{};
    std::uint16_t blockAlign{};
    std::uint16_t bitsPerSample{};
};

std::optional<WaveFormat> readFormat(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    const std::size_t size) noexcept
{
    if (size < kBaseFormatSize || !hasRange(bytes, offset, size))
    {
        return std::nullopt;
    }

    WaveFormat format{
        .encoding      = readU16Le(bytes, offset),
        .channelCount  = readU16Le(bytes, offset + 2U),
        .sampleRate    = readU32Le(bytes, offset + 4U),
        .blockAlign    = readU16Le(bytes, offset + 12U),
        .bitsPerSample = readU16Le(bytes, offset + 14U),
    };

    // WAVE_FORMAT_EXTENSIBLE stores the actual PCM/float encoding in its GUID.
    if (format.encoding == kExtensibleFormat)
    {
        if ( size < kExtensibleFormatSize )
        {
            return std::nullopt;
        }

        const std::size_t extensionSize = readU16Le(bytes, offset + 16U);
        if ( extensionSize < 22U
            || extensionSize > size - 18U
            || !std::equal(
                kWaveSubFormatGuidSuffix.begin(),
                kWaveSubFormatGuidSuffix.end(),
                bytes.begin() + static_cast<std::ptrdiff_t>(offset + 26U)) )
        {
            return std::nullopt;
        }
        format.encoding = readU16Le(bytes, offset + 24U);
    }

    if (format.channelCount == 0U
        || format.channelCount > 32U
        || format.sampleRate == 0U
        || format.sampleRate > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
    {
        return std::nullopt;
    }

    const std::uint32_t bytesPerSample = (format.bitsPerSample + 7U) / 8U;
    const std::uint32_t expectedBlockAlign =
        static_cast<std::uint32_t>(format.channelCount) * bytesPerSample;
    if (bytesPerSample == 0U || expectedBlockAlign != format.blockAlign)
    {
        return std::nullopt;
    }

    const bool supportedPcm = format.encoding == kPcmFormat
        && (format.bitsPerSample == 8U
            || format.bitsPerSample == 16U
            || format.bitsPerSample == 24U
            || format.bitsPerSample == 32U);
    const bool supportedFloat = format.encoding == kFloatFormat
        && format.bitsPerSample == 32U;
    return supportedPcm || supportedFloat
        ? std::optional<WaveFormat>{format}
        : std::nullopt;
}

PcmSampleFormat sampleFormatFor(const WaveFormat& format) noexcept
{
    if (format.encoding == kFloatFormat)
    {
        return PcmSampleFormat::float32;
    }
    switch (format.bitsPerSample)
    {
        case 8U:  return PcmSampleFormat::unsigned8;
        case 16U: return PcmSampleFormat::signed16;
        default:  return PcmSampleFormat::signed32;
    }
}

std::optional<std::vector<std::byte>> readSamples(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    const std::size_t size,
    const WaveFormat& format)
{
    if (size == 0U
        || size > kMaxPcmBytes
        || !hasRange(bytes, offset, size)
        || (size % format.blockAlign) != 0U)
    {
        return std::nullopt;
    }

    if (format.bitsPerSample != 24U)
    {
        return std::vector<std::byte>(bytes.begin() + offset, bytes.begin() + offset + size);
    }

    // Qt has no packed 24-bit sample format. Preserve the normalized amplitude by
    // shifting every little-endian PCM24 sample into the full signed PCM32 range.
    const std::size_t sampleCount = size / 3U;
    if (sampleCount > kMaxPcmBytes / 4U)
    {
        return std::nullopt;
    }

    std::vector<std::byte> samples;
    samples.reserve(sampleCount * 4U);
    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        const std::size_t source = offset + i * 3U;
        samples.push_back(std::byte{});
        samples.push_back(bytes[source]);
        samples.push_back(bytes[source + 1U]);
        samples.push_back(bytes[source + 2U]);
    }
    return samples;
}

} // namespace

std::optional<PcmWave> decodePcmWave(const std::span<const std::byte> bytes)
{
    // Step 1: bound all chunk reads by both the byte stream and declared RIFF size.
    if (!hasTag(bytes, 0U, "RIFF")
        || !hasRange(bytes, 4U, 8U)
        || !hasTag(bytes, 8U, "WAVE"))
    {
        return std::nullopt;
    }
    const std::uint64_t riffEnd64 = static_cast<std::uint64_t>(readU32Le(bytes, 4U)) + 8U;
    if (riffEnd64 < 12U || riffEnd64 > bytes.size())
    {
        return std::nullopt;
    }
    const std::size_t riffEnd = static_cast<std::size_t>(riffEnd64);

    // Step 2: locate format and sample chunks without assuming their order.
    std::optional<WaveFormat> format;
    std::optional<std::pair<std::size_t, std::size_t>> dataRange;
    std::size_t chunkOffset = 12U;
    while (chunkOffset < riffEnd)
    {
        if (riffEnd - chunkOffset < 8U)
        {
            return std::nullopt;
        }

        const std::size_t payloadOffset = chunkOffset + 8U;
        const std::size_t payloadSize = readU32Le(bytes, chunkOffset + 4U);
        if (payloadSize > riffEnd - payloadOffset)
        {
            return std::nullopt;
        }

        if (!format && hasTag(bytes, chunkOffset, "fmt "))
        {
            format = readFormat(bytes, payloadOffset, payloadSize);
            if (!format)
            {
                return std::nullopt;
            }
        }
        else if (!dataRange && hasTag(bytes, chunkOffset, "data"))
        {
            dataRange = std::pair{payloadOffset, payloadSize};
        }

        const std::size_t paddedSize = payloadSize + (payloadSize & 1U);
        if (paddedSize > riffEnd - payloadOffset)
        {
            return std::nullopt;
        }
        chunkOffset = payloadOffset + paddedSize;
    }

    if (!format || !dataRange)
    {
        return std::nullopt;
    }

    // Step 3: extract a Qt-supported PCM payload, widening PCM24 when needed.
    auto samples = readSamples(bytes, dataRange->first, dataRange->second, *format);
    if (!samples)
    {
        return std::nullopt;
    }
    return PcmWave{
        .sampleRate   = format->sampleRate,
        .channelCount = format->channelCount,
        .sampleFormat = sampleFormatFor(*format),
        .samples      = std::move(*samples),
    };
}

} // namespace sed::assets
