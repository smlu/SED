#include "test_common.hpp"

#include "sed/assets/indy_wv.hpp"
#include "sed/assets/wav.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace sed::tests
{
namespace
{

void appendText(std::vector<std::byte>& bytes, const std::string_view text)
{
    for (const char value : text)
    {
        bytes.push_back(static_cast<std::byte>(value));
    }
}

void appendU16Be(std::vector<std::byte>& bytes, const std::uint16_t value)
{
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
}

void appendU32Le(std::vector<std::byte>& bytes, const std::uint32_t value)
{
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
}

std::uint16_t readU16Le(const std::span<const std::byte> bytes, const std::size_t offset)
{
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset]))
        | static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(bytes[offset + 1U]) << 8U);
}

std::uint32_t readU32Le(const std::span<const std::byte> bytes, const std::size_t offset)
{
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset]))
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1U])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2U])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3U])) << 24U);
}

bool matchesText(
    const std::span<const std::byte> bytes,
    const std::size_t offset,
    const std::string_view expected)
{
    if (offset + expected.size() > bytes.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        if (std::to_integer<unsigned char>(bytes[offset + i])
            != static_cast<unsigned char>(expected[i]))
        {
            return false;
        }
    }
    return true;
}

std::vector<std::byte> makeIndyWvFixture(const std::uint32_t channelCount = 1U)
{
    std::vector<std::byte> bytes;

    // INDIWV container header. Container values are little endian.
    appendText(bytes, "INDYWV");
    appendU32Le(bytes, 11'025U);
    appendU32Le(bytes, 16U);
    appendU32Le(bytes, channelCount);
    appendU32Le(bytes, 8U);
    appendU32Le(bytes, 0U);

    // Legacy stereo compressor metadata followed by one short WVSM block.
    appendU32Le(bytes, 8U);
    bytes.push_back(std::byte{0xFF});
    appendU16Be(bytes, 0x1111U);
    bytes.push_back(std::byte{0x64});
    appendU16Be(bytes, 0x2222U);
    appendText(bytes, "WVSM");
    appendU16Be(bytes, 7U);
    bytes.push_back(std::byte{0x00});

    // Samples: 1, -2, 127, and escaped big-endian 0x1234.
    bytes.push_back(std::byte{0x01});
    bytes.push_back(std::byte{0xFE});
    bytes.push_back(std::byte{0x7F});
    bytes.push_back(std::byte{0x80});
    appendU16Be(bytes, 0x1234U);
    return bytes;
}

} // namespace

void runIndyWvTests()
{
    // Shipped IJIM INDIWV resources declare mono output even though their
    // compressor metadata contains two alternating expander lanes.
    const std::vector<std::byte> source = makeIndyWvFixture();
    check(assets::isIndyWvSound(source), "INDYWV: signature is recognized");

    const auto wav = assets::convertIndyWvToWav(source);
    check(wav.has_value(), "INDYWV: supported stereo WVSM stream decodes");
    if (wav)
    {
        const std::span<const std::byte> bytes = *wav;
        check(bytes.size() == 52U, "INDYWV: decoded WAV has canonical header and data size");
        check(matchesText(bytes, 0U, "RIFF"), "INDYWV: decoded WAV has RIFF tag");
        check(readU32Le(bytes, 4U) == 44U, "INDYWV: decoded WAV RIFF size is correct");
        check(matchesText(bytes, 8U, "WAVE"), "INDYWV: decoded WAV has WAVE tag");
        check(matchesText(bytes, 12U, "fmt "), "INDYWV: decoded WAV has fmt chunk");
        check(readU16Le(bytes, 20U) == 1U, "INDYWV: decoded WAV is PCM");
        check(readU16Le(bytes, 22U) == 1U, "INDYWV: decoded WAV preserves mono output");
        check(readU32Le(bytes, 24U) == 11'025U, "INDYWV: decoded WAV preserves sample rate");
        check(readU32Le(bytes, 28U) == 22'050U, "INDYWV: decoded mono WAV computes byte rate");
        check(readU16Le(bytes, 32U) == 2U, "INDYWV: decoded mono WAV computes block alignment");
        check(readU16Le(bytes, 34U) == 16U, "INDYWV: decoded WAV preserves sample bits");
        check(matchesText(bytes, 36U, "data"), "INDYWV: decoded WAV has data chunk");

        constexpr std::array<std::byte, 8> expectedSamples{
            std::byte{0x01}, std::byte{0x00},
            std::byte{0xFE}, std::byte{0xFF},
            std::byte{0x7F}, std::byte{0x00},
            std::byte{0x34}, std::byte{0x12},
        };
        check(
            std::equal(expectedSamples.begin(), expectedSamples.end(), bytes.begin() + 44),
            "INDYWV: literal and escaped samples decode exactly");

        const auto pcm = assets::decodePcmWave(bytes);
        check(pcm && pcm->sampleRate == 11'025U && pcm->channelCount == 1U,
              "INDYWV: shipped-style mono output passes the direct PCM playback parser");
        check(pcm && pcm->sampleFormat == assets::PcmSampleFormat::signed16,
              "INDYWV: direct playback receives signed PCM16 samples");
    }

    const auto stereoWav = assets::convertIndyWvToWav(makeIndyWvFixture(2U));
    check(stereoWav && readU16Le(*stereoWav, 22U) == 2U,
          "INDYWV: stereo output remains supported and preserves its channel count");

    check(!assets::convertIndyWvToWav(makeIndyWvFixture(0U)).has_value(),
          "INDYWV: zero output channels are rejected");
    check(!assets::convertIndyWvToWav(makeIndyWvFixture(3U)).has_value(),
          "INDYWV: unsupported output channel counts are rejected");

    std::vector<std::byte> ordinaryWav;
    appendText(ordinaryWav, "RIFF");
    appendU32Le(ordinaryWav, 0U);
    appendText(ordinaryWav, "WAVE");
    check(!assets::isIndyWvSound(ordinaryWav), "INDYWV: ordinary WAV is not misidentified");
    check(!assets::convertIndyWvToWav(ordinaryWav).has_value(),
          "INDYWV: ordinary WAV is not passed through as compressed input");

    std::vector<std::byte> unsupported = source;
    unsupported[31] = std::byte{0x10};
    check(!assets::convertIndyWvToWav(unsupported).has_value(),
          "INDYWV: unsupported compressor metadata is rejected");

    std::vector<std::byte> truncated = source;
    truncated.pop_back();
    check(!assets::convertIndyWvToWav(truncated).has_value(),
          "INDYWV: truncated escaped sample is rejected");

    std::vector<std::byte> oversized = source;
    std::fill(oversized.begin() + 26, oversized.begin() + 30, std::byte{0xFF});
    check(!assets::convertIndyWvToWav(oversized).has_value(),
          "INDYWV: unreasonable inflated size is rejected before allocation");

    check(!assets::isIndyWvSound(std::span<const std::byte>{}),
          "INDYWV: empty input is not identified");
    check(!assets::convertIndyWvToWav(std::span<const std::byte>{}).has_value(),
          "INDYWV: empty input is rejected");
}

} // namespace sed::tests
