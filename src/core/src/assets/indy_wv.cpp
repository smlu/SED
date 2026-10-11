#include "sed/assets/indy_wv.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace sed::assets
{
namespace
{

constexpr std::string_view kIndyWvTag = "INDYWV";
constexpr std::string_view kCompressedDataTag = "WVSM";
constexpr std::size_t kWavHeaderSize = 44U;
constexpr std::size_t kCompressedBlockSize = 4096U;
constexpr std::uint32_t kMaxInflatedPcmSize = 512U * 1024U * 1024U;
constexpr std::uint16_t kPcmFormat = 1U;

class ByteReader final
{
public:
    explicit ByteReader(const std::span<const std::byte> bytes) noexcept
        : bytes_(bytes)
    {
    }

    bool readU8(std::uint8_t& value) noexcept
    {
        if (remaining() < 1U)
        {
            return false;
        }
        value = std::to_integer<std::uint8_t>(bytes_[position_++]);
        return true;
    }

    bool readU16Be(std::uint16_t& value) noexcept
    {
        std::uint8_t high{};
        std::uint8_t low{};
        if (!readU8(high) || !readU8(low))
        {
            return false;
        }
        value = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(high) << 8U) | low);
        return true;
    }

    bool readU32Le(std::uint32_t& value) noexcept
    {
        std::array<std::uint8_t, 4> bytes{};
        for (std::uint8_t& byte : bytes)
        {
            if (!readU8(byte))
            {
                return false;
            }
        }
        value = static_cast<std::uint32_t>(bytes[0])
            | (static_cast<std::uint32_t>(bytes[1]) << 8U)
            | (static_cast<std::uint32_t>(bytes[2]) << 16U)
            | (static_cast<std::uint32_t>(bytes[3]) << 24U);
        return true;
    }

    bool readTag(const std::string_view tag) noexcept
    {
        if (remaining() < tag.size())
        {
            return false;
        }

        for (const char expected : tag)
        {
            std::uint8_t actual{};
            if (!readU8(actual) || actual != static_cast<std::uint8_t>(expected))
            {
                return false;
            }
        }
        return true;
    }

    bool skip(const std::size_t count) noexcept
    {
        if (remaining() < count)
        {
            return false;
        }
        position_ += count;
        return true;
    }

private:
    std::size_t remaining() const noexcept
    {
        return bytes_.size() - position_;
    }

    std::span<const std::byte> bytes_;
    std::size_t position_{};
};

void appendU16Le(std::vector<std::byte>& bytes, const std::uint16_t value)
{
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
}

void appendU32Le(std::vector<std::byte>& bytes, const std::uint32_t value)
{
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
}

void appendTag(std::vector<std::byte>& bytes, const std::string_view tag)
{
    for (const char value : tag)
    {
        bytes.push_back(static_cast<std::byte>(value));
    }
}

bool inflateSample(
    ByteReader& reader,
    const std::uint8_t expander,
    std::vector<std::byte>& output,
    const std::size_t offset) noexcept
{
    std::uint8_t encoded{};
    if (!reader.readU8(encoded))
    {
        return false;
    }

    std::uint16_t sample{};
    if (encoded == 0x80U)
    {
        if (!reader.readU16Be(sample))
        {
            return false;
        }
    }
    else
    {
        const auto signedByte = static_cast<std::int8_t>(encoded);
        const std::int32_t expanded = static_cast<std::int32_t>(signedByte)
            * (static_cast<std::int32_t>(1U) << expander);
        sample = static_cast<std::uint16_t>(expanded);
    }

    output[offset] = static_cast<std::byte>(sample & 0xFFU);
    output[offset + 1U] = static_cast<std::byte>((sample >> 8U) & 0xFFU);
    return true;
}

bool inflateBlock(
    ByteReader& reader,
    const std::size_t outputSize,
    std::vector<std::byte>& output,
    const std::size_t outputOffset) noexcept
{
    if (outputSize == 0U)
    {
        return true;
    }
    if ((outputSize % sizeof(std::uint16_t)) != 0U)
    {
        return false;
    }

    std::uint16_t compressedSize{};
    std::uint8_t expanders{};
    if (!reader.readU16Be(compressedSize) || !reader.readU8(expanders))
    {
        return false;
    }
    (void)compressedSize; // The legacy format stores it but decodes by output block size.

    const std::array<std::uint8_t, 2> channelExpanders{
        static_cast<std::uint8_t>(expanders >> 4U),
        static_cast<std::uint8_t>(expanders & 0x0FU),
    };

    // Samples alternate left/right expanders and are always emitted as PCM16 LE.
    const std::size_t sampleCount = outputSize / sizeof(std::uint16_t);
    for (std::size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        if (!inflateSample(
                reader,
                channelExpanders[sampleIndex % channelExpanders.size()],
                output,
                outputOffset + sampleIndex * sizeof(std::uint16_t)))
        {
            return false;
        }
    }
    return true;
}

bool startsWith(const std::span<const std::byte> bytes, const std::string_view tag) noexcept
{
    if (bytes.size() < tag.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < tag.size(); ++i)
    {
        if (std::to_integer<std::uint8_t>(bytes[i]) != static_cast<std::uint8_t>(tag[i]))
        {
            return false;
        }
    }
    return true;
}

} // namespace

bool isIndyWvSound(const std::span<const std::byte> bytes) noexcept
{
    return startsWith(bytes, kIndyWvTag);
}

std::optional<std::vector<std::byte>> convertIndyWvToWav(
    const std::span<const std::byte> bytes)
{
    ByteReader reader(bytes);

    // Step 1: read and validate the packed INDIWV container header.
    std::uint32_t sampleRate{};
    std::uint32_t sampleBitSize{};
    std::uint32_t channelCount{};
    std::uint32_t declaredDataSize{};
    std::uint32_t dataOffsetBits{};
    if (!reader.readTag(kIndyWvTag)
        || !reader.readU32Le(sampleRate)
        || !reader.readU32Le(sampleBitSize)
        || !reader.readU32Le(channelCount)
        || !reader.readU32Le(declaredDataSize)
        || !reader.readU32Le(dataOffsetBits))
    {
        return std::nullopt;
    }
    (void)declaredDataSize; // Preserved for format fidelity; inflated size is authoritative.

    const auto dataOffset = static_cast<std::int32_t>(dataOffsetBits);
    if (dataOffset < 0 || !reader.skip(static_cast<std::size_t>(dataOffset)))
    {
        return std::nullopt;
    }

    // Step 2: accept the one compressor state supported by the original editor.
    std::uint32_t inflatedSize{};
    std::uint8_t signedChannelMarker{};
    std::uint16_t stateWordLeft{};
    if (!reader.readU32Le(inflatedSize)
        || !reader.readU8(signedChannelMarker)
        || !reader.readU16Be(stateWordLeft))
    {
        return std::nullopt;
    }

    const bool stereoState = static_cast<std::int8_t>(signedChannelMarker) < 0;
    std::uint8_t stateByteRight{};
    std::uint16_t stateWordRight{};
    if (!stereoState
        || !reader.readU8(stateByteRight)
        || !reader.readU16Be(stateWordRight)
        || stateWordLeft != 0x1111U
        || stateByteRight != 0x64U
        || stateWordRight != 0x2222U
        || !reader.readTag(kCompressedDataTag))
    {
        return std::nullopt;
    }

    // The compressor always stores two alternating expander nibbles, even for
    // the mono INDIWV assets shipped with IJIM. Preserve the container's actual
    // WAV channel count instead of confusing it with that compressor state.
    if (sampleRate == 0U
        || sampleBitSize != 16U
        || (channelCount != 1U && channelCount != 2U)
        || inflatedSize > kMaxInflatedPcmSize
        || (inflatedSize % sizeof(std::uint16_t)) != 0U)
    {
        return std::nullopt;
    }

    // Step 3: inflate fixed 4096-byte blocks and the final short block.
    std::vector<std::byte> pcm(inflatedSize);
    std::size_t outputOffset{};
    while (pcm.size() - outputOffset >= kCompressedBlockSize)
    {
        if (!inflateBlock(reader, kCompressedBlockSize, pcm, outputOffset))
        {
            return std::nullopt;
        }
        outputOffset += kCompressedBlockSize;
    }
    if (!inflateBlock(reader, pcm.size() - outputOffset, pcm, outputOffset))
    {
        return std::nullopt;
    }

    const std::uint64_t blockAlign64 =
        static_cast<std::uint64_t>(channelCount) * sampleBitSize / 8U;
    const std::uint64_t byteRate64 = static_cast<std::uint64_t>(sampleRate) * blockAlign64;
    if (blockAlign64 > std::numeric_limits<std::uint16_t>::max()
        || byteRate64 > std::numeric_limits<std::uint32_t>::max()
        || (inflatedSize % blockAlign64) != 0U
        || inflatedSize > std::numeric_limits<std::uint32_t>::max() - 36U)
    {
        return std::nullopt;
    }

    // Step 4: wrap the decoded PCM in a canonical 44-byte RIFF/WAVE header.
    std::vector<std::byte> wav;
    wav.reserve(kWavHeaderSize + pcm.size());
    appendTag(wav, "RIFF");
    appendU32Le(wav, 36U + inflatedSize);
    appendTag(wav, "WAVE");
    appendTag(wav, "fmt ");
    appendU32Le(wav, 16U);
    appendU16Le(wav, kPcmFormat);
    appendU16Le(wav, static_cast<std::uint16_t>(channelCount));
    appendU32Le(wav, sampleRate);
    appendU32Le(wav, static_cast<std::uint32_t>(byteRate64));
    appendU16Le(wav, static_cast<std::uint16_t>(blockAlign64));
    appendU16Le(wav, static_cast<std::uint16_t>(sampleBitSize));
    appendTag(wav, "data");
    appendU32Le(wav, inflatedSize);
    wav.insert(wav.end(), pcm.begin(), pcm.end());
    return wav;
}

} // namespace sed::assets
