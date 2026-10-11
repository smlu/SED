#include "test_common.hpp"

#include "sed/assets/wav.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace sed::tests
{
namespace
{

using Bytes = std::vector<std::byte>;

constexpr std::array<std::byte, 16> kPcmSubFormatGuid{
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x00},
    std::byte{0x80}, std::byte{0x00}, std::byte{0x00}, std::byte{0xAA},
    std::byte{0x00}, std::byte{0x38}, std::byte{0x9B}, std::byte{0x71},
};

constexpr std::array<std::byte, 16> kFloatSubFormatGuid{
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x10}, std::byte{0x00},
    std::byte{0x80}, std::byte{0x00}, std::byte{0x00}, std::byte{0xAA},
    std::byte{0x00}, std::byte{0x38}, std::byte{0x9B}, std::byte{0x71},
};

struct FormatSpec final
{
    std::uint16_t encoding{1U};
    std::uint16_t channelCount{1U};
    std::uint32_t sampleRate{22'050U};
    std::uint16_t bitsPerSample{16U};
    std::optional<std::uint16_t> blockAlign{};
    std::optional<std::uint32_t> byteRate{};
    std::optional<std::array<std::byte, 16>> subFormatGuid{};
    std::uint16_t extensionSize{22U};
};

void appendText(Bytes& bytes, const std::string_view text)
{
    for ( const char value : text )
    {
        bytes.push_back(static_cast<std::byte>(value));
    }
}

void appendU16Le(Bytes& bytes, const std::uint16_t value)
{
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
}

void appendU32Le(Bytes& bytes, const std::uint32_t value)
{
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
}

void writeU32Le(Bytes& bytes, const std::size_t offset, const std::uint32_t value)
{
    bytes[offset] = static_cast<std::byte>(value & 0xFFU);
    bytes[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    bytes[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    bytes[offset + 3U] = static_cast<std::byte>((value >> 24U) & 0xFFU);
}

Bytes makeFormat(const FormatSpec& spec)
{
    const std::uint32_t bytesPerSample = (spec.bitsPerSample + 7U) / 8U;
    const std::uint16_t defaultBlockAlign = static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(spec.channelCount) * bytesPerSample);
    const std::uint16_t blockAlign = spec.blockAlign.value_or(defaultBlockAlign);
    const std::uint32_t byteRate = spec.byteRate.value_or(spec.sampleRate * blockAlign);

    Bytes format;
    appendU16Le(format, spec.encoding);
    appendU16Le(format, spec.channelCount);
    appendU32Le(format, spec.sampleRate);
    appendU32Le(format, byteRate);
    appendU16Le(format, blockAlign);
    appendU16Le(format, spec.bitsPerSample);

    if ( spec.subFormatGuid )
    {
        appendU16Le(format, spec.extensionSize);
        appendU16Le(format, spec.bitsPerSample);
        appendU32Le(format, 0U);
        format.insert(format.end(), spec.subFormatGuid->begin(), spec.subFormatGuid->end());
    }

    return format;
}

void appendChunk(
    Bytes& body,
    const std::string_view tag,
    const std::span<const std::byte> payload,
    const bool appendPadding = true)
{
    appendText(body, tag);
    appendU32Le(body, static_cast<std::uint32_t>(payload.size()));
    body.insert(body.end(), payload.begin(), payload.end());
    if ( appendPadding && (payload.size() & 1U) != 0U )
    {
        body.push_back(std::byte{});
    }
}

Bytes makeRiff(const Bytes& chunks)
{
    Bytes bytes;
    appendText(bytes, "RIFF");
    appendU32Le(bytes, static_cast<std::uint32_t>(chunks.size() + 4U));
    appendText(bytes, "WAVE");
    bytes.insert(bytes.end(), chunks.begin(), chunks.end());
    return bytes;
}

Bytes makeWave(
    const FormatSpec& spec,
    const std::span<const std::byte> samples,
    const bool dataBeforeFormat = false,
    const bool appendDataPadding = true)
{
    const Bytes format = makeFormat(spec);
    Bytes chunks;
    if ( dataBeforeFormat )
    {
        appendChunk(chunks, "data", samples, appendDataPadding);
        appendChunk(chunks, "fmt ", format);
    }
    else
    {
        appendChunk(chunks, "fmt ", format);
        appendChunk(chunks, "data", samples, appendDataPadding);
    }
    return makeRiff(chunks);
}

bool decodes(const FormatSpec& spec, const std::span<const std::byte> samples)
{
    return assets::decodePcmWave(makeWave(spec, samples)).has_value();
}

} // namespace

void runWavTests()
{
    {
        constexpr std::array<std::byte, 4> samples{
            std::byte{0x01}, std::byte{0x00}, std::byte{0xFF}, std::byte{0x7F},
        };
        const auto wave = assets::decodePcmWave(makeWave(FormatSpec{}, samples));

        check(wave.has_value(), "WAV PCM16: canonical stream decodes");
        check(wave && wave->sampleRate == 22'050U,
            "WAV PCM16: sample rate is preserved");
        check(wave && wave->channelCount == 1U,
            "WAV PCM16: channel count is preserved");
        check(wave && wave->sampleFormat == assets::PcmSampleFormat::signed16,
            "WAV PCM16: sample format is signed16");
        check(wave && wave->samples == Bytes(samples.begin(), samples.end()),
            "WAV PCM16: sample bytes are preserved exactly");
    }

    {
        constexpr std::array<std::byte, 2> samples{std::byte{0x00}, std::byte{0xFF}};
        const auto wave = assets::decodePcmWave(makeWave(
            FormatSpec{.bitsPerSample = 8U}, samples));

        check(wave.has_value(), "WAV PCM8: canonical stream decodes");
        check(wave && wave->sampleFormat == assets::PcmSampleFormat::unsigned8,
            "WAV PCM8: sample format is unsigned8");
        check(wave && wave->samples == Bytes(samples.begin(), samples.end()),
            "WAV PCM8: sample bytes are preserved exactly");
    }

    {
        constexpr std::array<std::byte, 6> samples{
            std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
            std::byte{0xFF}, std::byte{0xFF}, std::byte{0x7F},
        };
        constexpr std::array<std::byte, 8> expected{
            std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
            std::byte{0x00}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x7F},
        };
        const auto wave = assets::decodePcmWave(makeWave(
            FormatSpec{.bitsPerSample = 24U}, samples));

        check(wave.has_value(), "WAV PCM24: canonical stream decodes");
        check(wave && wave->sampleFormat == assets::PcmSampleFormat::signed32,
            "WAV PCM24: sample format widens to signed32");
        check(wave && wave->samples == Bytes(expected.begin(), expected.end()),
            "WAV PCM24: signed full-scale samples widen exactly");
    }

    {
        constexpr std::array<std::byte, 8> samples{
            std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80},
            std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x7F},
        };
        const auto wave = assets::decodePcmWave(makeWave(
            FormatSpec{.bitsPerSample = 32U}, samples));

        check(wave.has_value(), "WAV PCM32: canonical stream decodes");
        check(wave && wave->sampleFormat == assets::PcmSampleFormat::signed32,
            "WAV PCM32: sample format is signed32");
        check(wave && wave->samples == Bytes(samples.begin(), samples.end()),
            "WAV PCM32: sample bytes are preserved exactly");
    }

    {
        constexpr std::array<std::byte, 8> samples{
            std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x3F},
            std::byte{0x00}, std::byte{0x00}, std::byte{0x80}, std::byte{0xBF},
        };
        const auto wave = assets::decodePcmWave(makeWave(
            FormatSpec{.encoding = 3U, .bitsPerSample = 32U}, samples));

        check(wave.has_value(), "WAV float32: canonical stream decodes");
        check(wave && wave->sampleFormat == assets::PcmSampleFormat::float32,
            "WAV float32: sample format is float32");
        check(wave && wave->samples == Bytes(samples.begin(), samples.end()),
            "WAV float32: sample bytes are preserved exactly");
    }

    {
        constexpr std::array<std::byte, 4> samples{
            std::byte{0x01}, std::byte{0x00}, std::byte{0xFF}, std::byte{0x7F},
        };
        const auto pcm = assets::decodePcmWave(makeWave(
            FormatSpec{
                .encoding = 0xFFFEU,
                .subFormatGuid = kPcmSubFormatGuid,
            }, samples));
        const auto floating = assets::decodePcmWave(makeWave(
            FormatSpec{
                .encoding = 0xFFFEU,
                .bitsPerSample = 32U,
                .subFormatGuid = kFloatSubFormatGuid,
            }, samples));

        check(pcm && pcm->sampleFormat == assets::PcmSampleFormat::signed16,
            "WAV extensible: canonical PCM GUID resolves to signed16");
        check(floating && floating->sampleFormat == assets::PcmSampleFormat::float32,
            "WAV extensible: canonical IEEE-float GUID resolves to float32");

        auto malformedGuid = kPcmSubFormatGuid;
        malformedGuid.back() ^= std::byte{0x01};
        check(!decodes(FormatSpec{
                .encoding = 0xFFFEU,
                .subFormatGuid = malformedGuid,
            }, samples),
            "WAV extensible: a noncanonical GUID tail is rejected");
        check(!decodes(FormatSpec{
                .encoding = 0xFFFEU,
                .subFormatGuid = kPcmSubFormatGuid,
                .extensionSize = 21U,
            }, samples),
            "WAV extensible: an undersized extension is rejected");
        check(!decodes(FormatSpec{
                .encoding = 0xFFFEU,
                .subFormatGuid = kPcmSubFormatGuid,
                .extensionSize = 23U,
            }, samples),
            "WAV extensible: an extension larger than its payload is rejected");

        Bytes shortFormat = makeFormat(FormatSpec{
            .encoding = 0xFFFEU,
            .subFormatGuid = kPcmSubFormatGuid,
        });
        shortFormat.pop_back();
        Bytes chunks;
        appendChunk(chunks, "fmt ", shortFormat);
        appendChunk(chunks, "data", samples);
        check(!assets::decodePcmWave(makeRiff(chunks)).has_value(),
            "WAV extensible: a truncated 40-byte format is rejected");
    }

    {
        constexpr std::array<std::byte, 64> samples{};
        for ( const std::uint16_t channelCount : {1U, 2U, 32U} )
        {
            const auto sampleBytes = std::span(samples).first(
                static_cast<std::size_t>(channelCount) * 2U);
            const auto wave = assets::decodePcmWave(makeWave(
                FormatSpec{.channelCount = channelCount}, sampleBytes));

            check(wave.has_value(),
                "WAV channels: supported boundary channel count decodes");
            check(wave && wave->channelCount == channelCount,
                "WAV channels: supported boundary channel count is preserved");
            check(wave && wave->samples == Bytes(sampleBytes.begin(), sampleBytes.end()),
                "WAV channels: supported boundary sample bytes are preserved");
        }

        check(!decodes(FormatSpec{.channelCount = 0U}, samples),
            "WAV channels: zero channels are rejected");
        check(!decodes(FormatSpec{.channelCount = 33U}, samples),
            "WAV channels: more than 32 channels are rejected");
    }

    {
        constexpr std::array<std::byte, 2> samples{};
        const auto minimumRate = assets::decodePcmWave(makeWave(
            FormatSpec{.sampleRate = 1U}, samples));
        const auto maximumRate = assets::decodePcmWave(makeWave(FormatSpec{
            .sampleRate = static_cast<std::uint32_t>(std::numeric_limits<int>::max()),
        }, samples));

        check(minimumRate.has_value(),
            "WAV sample rate: minimum positive rate decodes");
        check(minimumRate && minimumRate->sampleRate == 1U,
            "WAV sample rate: minimum positive rate is preserved");
        check(maximumRate.has_value(),
            "WAV sample rate: signed-int maximum decodes");
        check(maximumRate
                && maximumRate->sampleRate
                    == static_cast<std::uint32_t>(std::numeric_limits<int>::max()),
            "WAV sample rate: signed-int maximum is preserved");
        check(!decodes(FormatSpec{.sampleRate = 0U}, samples),
            "WAV sample rate: zero is rejected");
        check(!decodes(FormatSpec{
                .sampleRate = static_cast<std::uint32_t>(std::numeric_limits<int>::max()) + 1U,
            }, samples),
            "WAV sample rate: values above signed-int maximum are rejected");
    }

    {
        constexpr std::array<std::byte, 4> samples{};
        check(!decodes(FormatSpec{.blockAlign = 1U}, samples),
            "WAV block alignment: a mismatched value is rejected");
        check(!decodes(FormatSpec{.bitsPerSample = 0U, .blockAlign = 0U}, samples),
            "WAV sample width: zero bits are rejected");
        check(!decodes(FormatSpec{.bitsPerSample = 12U}, samples),
            "WAV sample width: packed PCM12 is rejected");
        check(!decodes(FormatSpec{.bitsPerSample = 64U}, samples),
            "WAV sample width: PCM64 is rejected");
        check(!decodes(FormatSpec{.encoding = 3U, .bitsPerSample = 64U}, samples),
            "WAV sample width: float64 is rejected");
        check(!decodes(FormatSpec{.encoding = 2U}, samples),
            "WAV encoding: compressed Microsoft ADPCM is rejected");
        check(!decodes(FormatSpec{.encoding = 0x1234U}, samples),
            "WAV encoding: unknown encodings are rejected");
    }

    {
        constexpr std::array<std::byte, 4> samples{
            std::byte{0x01}, std::byte{0x00}, std::byte{0x02}, std::byte{0x00},
        };
        const auto dataFirst = assets::decodePcmWave(makeWave(
            FormatSpec{}, samples, true));
        check(dataFirst.has_value(),
            "WAV chunk order: data before format decodes");
        check(dataFirst && dataFirst->samples == Bytes(samples.begin(), samples.end()),
            "WAV chunk order: data-before-format output is exact");

        Bytes chunks;
        const Bytes format = makeFormat(FormatSpec{});
        const Bytes unsupportedFormat = makeFormat(FormatSpec{.encoding = 2U});
        appendChunk(chunks, "fmt ", format);
        appendChunk(chunks, "fmt ", unsupportedFormat);
        appendChunk(chunks, "data", samples);
        check(assets::decodePcmWave(makeRiff(chunks)).has_value(),
            "WAV duplicate format: the first valid chunk wins");

        chunks.clear();
        appendChunk(chunks, "fmt ", unsupportedFormat);
        appendChunk(chunks, "fmt ", format);
        appendChunk(chunks, "data", samples);
        check(!assets::decodePcmWave(makeRiff(chunks)).has_value(),
            "WAV duplicate format: an invalid first chunk fails closed");

        constexpr std::array<std::byte, 2> otherSamples{
            std::byte{0x03}, std::byte{0x00},
        };
        chunks.clear();
        appendChunk(chunks, "fmt ", format);
        appendChunk(chunks, "data", samples);
        appendChunk(chunks, "data", otherSamples);
        const auto duplicateData = assets::decodePcmWave(makeRiff(chunks));
        check(duplicateData.has_value(),
            "WAV duplicate data: canonical chunks decode");
        check(duplicateData
                && duplicateData->samples == Bytes(samples.begin(), samples.end()),
            "WAV duplicate data: the first chunk wins exactly");

        chunks.clear();
        appendChunk(chunks, "fmt ", format);
        appendChunk(chunks, "data", std::span<const std::byte>{});
        appendChunk(chunks, "data", samples);
        check(!assets::decodePcmWave(makeRiff(chunks)).has_value(),
            "WAV duplicate data: an empty first chunk fails closed");
    }

    {
        constexpr std::array<std::byte, 2> samples{
            std::byte{0x01}, std::byte{0x00},
        };
        const Bytes format = makeFormat(FormatSpec{});
        Bytes chunks;
        constexpr std::array<std::byte, 1> metadata{std::byte{0xA5}};
        appendChunk(chunks, "JUNK", metadata);
        appendChunk(chunks, "fmt ", format);
        appendChunk(chunks, "data", samples);
        check(assets::decodePcmWave(makeRiff(chunks)).has_value(),
            "WAV unknown chunks: odd-sized metadata and padding are skipped");

        const Bytes noPadding = makeWave(
            FormatSpec{.bitsPerSample = 8U}, metadata, false, false);
        check(!assets::decodePcmWave(noPadding).has_value(),
            "WAV padding: a missing odd data pad byte is rejected");
    }

    {
        constexpr std::array<std::byte, 4> samples{};
        Bytes valid = makeWave(FormatSpec{}, samples);

        Bytes trailing = valid;
        trailing.push_back(std::byte{0xCC});
        check(assets::decodePcmWave(trailing).has_value(),
            "WAV RIFF size: bytes after the declared RIFF form are ignored");

        Bytes declaredLong = valid;
        writeU32Le(declaredLong, 4U,
            static_cast<std::uint32_t>(declaredLong.size() - 7U));
        check(!assets::decodePcmWave(declaredLong).has_value(),
            "WAV RIFF size: a declaration beyond physical EOF is rejected");

        Bytes declaredShort = valid;
        writeU32Le(declaredShort, 4U,
            static_cast<std::uint32_t>(declaredShort.size() - 9U));
        check(!assets::decodePcmWave(declaredShort).has_value(),
            "WAV RIFF size: a declaration cutting the final chunk is rejected");

        Bytes truncated = valid;
        truncated.pop_back();
        check(!assets::decodePcmWave(truncated).has_value(),
            "WAV RIFF size: a physically truncated payload is rejected");
    }

    {
        constexpr std::array<std::byte, 4> samples{};
        const Bytes valid = makeWave(FormatSpec{}, samples);

        Bytes rf64 = valid;
        rf64[0] = std::byte{'R'};
        rf64[1] = std::byte{'F'};
        rf64[2] = std::byte{'6'};
        rf64[3] = std::byte{'4'};
        check(!assets::decodePcmWave(rf64).has_value(),
            "WAV container: unsupported RF64 is explicitly rejected");

        Bytes badWave = valid;
        badWave[8] = std::byte{'N'};
        check(!assets::decodePcmWave(badWave).has_value(),
            "WAV container: a non-WAVE RIFF form is rejected");

        Bytes shortHeader(valid.begin(), valid.begin() + 11);
        check(!assets::decodePcmWave(shortHeader).has_value(),
            "WAV container: a truncated RIFF/WAVE header is rejected");

        Bytes chunks;
        appendChunk(chunks, "data", samples);
        check(!assets::decodePcmWave(makeRiff(chunks)).has_value(),
            "WAV required chunks: missing format is rejected");

        chunks.clear();
        const Bytes format = makeFormat(FormatSpec{});
        appendChunk(chunks, "fmt ", format);
        check(!assets::decodePcmWave(makeRiff(chunks)).has_value(),
            "WAV required chunks: missing data is rejected");

        Bytes shortFormat = format;
        shortFormat.pop_back();
        chunks.clear();
        appendChunk(chunks, "fmt ", shortFormat);
        appendChunk(chunks, "data", samples);
        check(!assets::decodePcmWave(makeRiff(chunks)).has_value(),
            "WAV format: a base format shorter than 16 bytes is rejected");

        chunks.push_back(std::byte{'f'});
        check(!assets::decodePcmWave(makeRiff(chunks)).has_value(),
            "WAV chunks: a truncated chunk header is rejected");
    }

    {
        const Bytes format = makeFormat(FormatSpec{});
        Bytes chunks;
        appendChunk(chunks, "fmt ", format);
        appendChunk(chunks, "data", std::span<const std::byte>{});
        check(!assets::decodePcmWave(makeRiff(chunks)).has_value(),
            "WAV data: an empty payload is rejected");

        constexpr std::array<std::byte, 3> misaligned{
            std::byte{0x01}, std::byte{0x00}, std::byte{0x02},
        };
        check(!assets::decodePcmWave(makeWave(FormatSpec{}, misaligned)).has_value(),
            "WAV data: a payload not divisible by block alignment is rejected");

        Bytes truncatedPayload;
        appendText(truncatedPayload, "fmt ");
        appendU32Le(truncatedPayload, 17U);
        truncatedPayload.insert(truncatedPayload.end(), format.begin(), format.end());
        check(!assets::decodePcmWave(makeRiff(truncatedPayload)).has_value(),
            "WAV chunks: a declared payload longer than the RIFF body is rejected");
    }

    check(!assets::decodePcmWave(std::span<const std::byte>{}).has_value(),
        "WAV input: empty input is rejected");
}

} // namespace sed::tests
