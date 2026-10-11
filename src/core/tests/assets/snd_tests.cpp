#include "test_common.hpp"

#include "sed/assets/snd.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace sed::tests
{
namespace
{

bool loadSoundClassThrows(const std::string& text)
{
    try
    {
        std::istringstream input(text);
        (void)sed::assets::loadFromSoundClass(input);
        return false;
    }
    catch (const std::runtime_error&)
    {
        return true;
    }
}

bool saveSoundClassThrows(const sed::assets::SoundClassFile& file)
{
    try
    {
        (void)sed::assets::saveToSoundClassText(file);
        return false;
    }
    catch (const std::runtime_error&)
    {
        return true;
    }
}

struct CommaDecimal final : std::numpunct<char>
{
    char do_decimal_point() const override
    {
        return ',';
    }
};

class ScopedGlobalLocale final
{
public:
    explicit ScopedGlobalLocale(const std::locale& locale)
        : original_(std::locale::global(locale))
    {
    }

    ~ScopedGlobalLocale()
    {
        std::locale::global(original_);
    }

private:
    std::locale original_;
};

} // namespace

void runSndTests()
{
    const char* const snd = R"SND(
# comment
0 foo.wav
1 BAR.WAV ; comment
badline
2
)SND";

    std::istringstream iss(snd);
    const sed::assets::SndFile file = sed::assets::loadFromSND(iss);

    check(file.wavs.size() == 2, "SND: wavs.size");
    if (file.wavs.size() == 2)
    {
        check(file.wavs[0] == "foo.wav", "SND: wavs[0]");
        check(file.wavs[1] == "BAR.WAV", "SND: wavs[1]");
    }

    checkThrowsWithMessage(
        []()
        {
            (void)sed::assets::loadFromSND(
                std::filesystem::path("build/test-temp/core/missing.snd"));
        },
        "Failed to open SND: build/test-temp/core/missing.snd",
        "SND: missing path reports the failed asset kind");

    {
        const auto classicModes = sed::assets::soundClassModeNames(
            sed::assets::SoundClassProfile::classic);
        const auto ijimModes = sed::assets::soundClassModeNames(
            sed::assets::SoundClassProfile::ijim);
        check(std::find(classicModes.begin(), classicModes.end(), "moving") != classicModes.end()
                && std::find(classicModes.begin(), classicModes.end(), "lwalksnow") == classicModes.end()
                && std::find(ijimModes.begin(), ijimModes.end(), "lwalksnow") != ijimModes.end()
                && classicModes.size() == 95U
                && ijimModes.size() == 140U,
            "sound class: mode suggestions and table sizes follow the selected engine profile");
    }

    {
        std::istringstream input(
            "moving first.wav 0x0000000B\n"
            "futureMode opaque.wav 0x00000002\n"
            "moving next.wav\n");
        const sed::assets::SoundClassFile soundClass =
            sed::assets::loadFromSoundClass(input, sed::assets::SoundClassProfile::classic);
        check(soundClass.entries.size() == 3
                && soundClass.entries[0].playFlags == 0x0BU
                && soundClass.entries[1].mode == "futureMode"
                && soundClass.entries[1].playFlags == 0x02U
                && soundClass.entries[2].playFlags == 0x40U,
            "sound class: classic omitted flags use the per-row engine default");
        std::istringstream ijimInput(
            "moving first.wav 0x0000000B\n"
            "futureMode opaque.wav 0x00000002\n"
            "moving next.wav\n");
        const sed::assets::SoundClassFile ijimClass = sed::assets::loadFromSoundClass(
            ijimInput, sed::assets::SoundClassProfile::ijim);
        check(ijimClass.entries.size() == 3
                && ijimClass.entries[0].playFlags == 0x0BU
                && ijimClass.entries[1].playFlags == 0x02U
                && ijimClass.entries[2].playFlags == 0x0BU,
            "sound class: IJIM unknown rows preserve their flags without changing recognized-mode inheritance");
    }

    {
        const std::string authoredSnowMode =
            "lwalksnow snow.wav 0x00000012\nmoving next.wav\n";
        std::istringstream classicInput(authoredSnowMode);
        std::istringstream ijimInput(authoredSnowMode);
        const sed::assets::SoundClassFile classic = sed::assets::loadFromSoundClass(
            classicInput, sed::assets::SoundClassProfile::classic);
        const sed::assets::SoundClassFile ijim = sed::assets::loadFromSoundClass(
            ijimInput, sed::assets::SoundClassProfile::ijim);
        check(classic.entries.size() == 2 && ijim.entries.size() == 2
                && classic.entries[1].playFlags == 0x40U
                && ijim.entries[1].playFlags == 0x12U,
            "sound class: only names recognized by the selected engine profile update inherited flags");
    }

    {
        constexpr std::string_view alternatives =
            "moving first.wav 0x0000000B\n"
            "moving second.wav\n";
        std::istringstream classicInput{ std::string(alternatives) };
        std::istringstream ijimInput{ std::string(alternatives) };
        const sed::assets::SoundClassFile classic = sed::assets::loadFromSoundClass(
            classicInput, sed::assets::SoundClassProfile::classic);
        const sed::assets::SoundClassFile ijim = sed::assets::loadFromSoundClass(
            ijimInput, sed::assets::SoundClassProfile::ijim);
        check(classic.entries.size() == 2
                && ijim.entries.size() == 2
                && classic.entries[1].playFlags == 0x40U
                && ijim.entries[1].playFlags == 0x0BU,
            "sound class: classic omitted flags reset per row while IJIM inherits recognized flags");
    }

    {
        std::istringstream input(R"SND(
# mode / wav flags min-radius max-radius max-volume
moving step.wav
moving step_alt.wav 0x0000000B 0.25 8.5 0.75 ; authored levels
futureMode none
moving step_last.wav
shortline
)SND");
        const sed::assets::SoundClassFile soundClass =
            sed::assets::loadFromSoundClass(input);

        check(soundClass.entries.size() == 4,
            "sound class: structured loader keeps valid entries and ignores short rows");
        if (soundClass.entries.size() == 4)
        {
            const sed::assets::SoundClassEntry& first = soundClass.entries[0];
            const sed::assets::SoundClassEntry& second = soundClass.entries[1];
            const sed::assets::SoundClassEntry& unknown = soundClass.entries[2];
            const sed::assets::SoundClassEntry& last = soundClass.entries[3];

            check(first.mode == "moving" && first.wav == "step.wav"
                    && first.playFlags == 0x40U
                    && first.minRadius == 0.5f
                    && first.maxRadius == 2.5f
                    && first.maxVolume == 1.0f,
                "sound class: first row receives engine flags and radius/volume defaults");
            check(second.mode == "moving" && second.wav == "step_alt.wav"
                    && second.playFlags == 0x0BU
                    && second.minRadius == 0.25f
                    && second.maxRadius == 8.5f
                    && second.maxVolume == 0.75f,
                "sound class: parses explicit hexadecimal flags and all three playback values");
            check(unknown.mode == "futureMode" && unknown.wav == "none"
                    && unknown.playFlags == 0x40U
                    && unknown.minRadius == 0.5f
                    && unknown.maxRadius == 2.5f
                    && unknown.maxVolume == 1.0f,
                "sound class: classic retains unknown mode names while resetting flags and playback defaults");
            check(last.mode == "moving" && last.wav == "step_last.wav"
                    && last.playFlags == 0x40U
                    && last.minRadius == 0.5f
                    && last.maxRadius == 2.5f
                    && last.maxVolume == 1.0f,
                "sound class: classic repeated alternatives remain ordered with per-row omitted-field defaults");
        }
    }

    {
        sed::assets::SoundClassFile soundClass{};
        soundClass.entries = {
            { "moving", "step.wav", 0x00000040U, 0.5f, 2.5f, 1.0f },
            { "moving", "step_alt.wav", 0x0000000BU, 0.25f, 8.5f, 0.75f },
            { "futureMode", "none", 0x80000000U, 0.5f, 2.5f, 1.25f },
        };
        constexpr std::string_view expected =
            "moving step.wav 0x00000040 0.5 2.5 1\n"
            "moving step_alt.wav 0x0000000B 0.25 8.5 0.75\n"
            "futureMode none 0x80000000 0.5 2.5 1.25\n";
        check(sed::assets::saveToSoundClassText(soundClass) == expected,
            "sound class: writer emits every field in a deterministic fixed vector");

        std::ostringstream output;
        output << "prefix\n";
        sed::assets::saveToSoundClassText(output, soundClass);
        check(output.str() == std::string("prefix\n") + std::string(expected),
            "sound class: stream writer appends the complete deterministic vector");

        std::istringstream roundTrip{ std::string(expected) };
        const sed::assets::SoundClassFile loaded =
            sed::assets::loadFromSoundClass(roundTrip);
        check(loaded.entries.size() == soundClass.entries.size(),
            "sound class: canonical text reloads the same number of event rows");
        bool allFieldsMatch = loaded.entries.size() == soundClass.entries.size();
        if (allFieldsMatch)
        {
            for (std::size_t index = 0; index < soundClass.entries.size(); ++index)
            {
                const auto& actual = loaded.entries[index];
                const auto& original = soundClass.entries[index];
                allFieldsMatch = allFieldsMatch
                    && actual.mode == original.mode
                    && actual.wav == original.wav
                    && actual.playFlags == original.playFlags
                    && actual.minRadius == original.minRadius
                    && actual.maxRadius == original.maxRadius
                    && actual.maxVolume == original.maxVolume;
            }
        }
        check(allFieldsMatch,
            "sound class: writer/reader preserves event order, names, flags, and every numeric field");
    }

    {
        sed::assets::SoundClassFile soundClass{};
        soundClass.entries.push_back({
            "localeMode", "locale.wav", 0x40U, 0.125f, 1.25f, 0.75f });
        const ScopedGlobalLocale locale(
            std::locale(std::locale::classic(), new CommaDecimal));
        (void)locale;
        const std::string localized =
            sed::assets::saveToSoundClassText(soundClass);
        check(localized == "localeMode locale.wav 0x00000040 0.125 1.25 0.75\n",
            "sound class: decimal output ignores the process numeric locale");
    }

    {
        struct WavBoundary
        {
            sed::assets::SoundClassProfile profile{};
            std::size_t maximumBytes{};
        };
        const std::array<WavBoundary, 2> boundaries{
            WavBoundary{ sed::assets::SoundClassProfile::classic, 31U },
            WavBoundary{ sed::assets::SoundClassProfile::ijim, 121U } };
        for ( const WavBoundary& boundary : boundaries )
        {
            sed::assets::SoundClassFile file{};
            const std::string maximumWav = std::string(boundary.maximumBytes - 4U, 'w') + ".wav";
            file.entries = {
                { "moving", maximumWav, 0x80004001U, 0.25f, 8.5f, 0.75f },
                { "moving", "none", 0x00000040U, 0.5f, 2.5f, 1.0f } };
            const std::string expected = "moving " + maximumWav
                + " 0x80004001 0.25 8.5 0.75\nmoving none 0x00000040 0.5 2.5 1\n";
            check(sed::assets::saveToSoundClassText(file, boundary.profile) == expected,
                "sound class engine boundary: maximum WAV identity retains alternative order and every flag and float");

            file.entries[0].wav.insert(file.entries[0].wav.begin(), 'w');
            const std::string rawText = "moving " + file.entries[0].wav
                + " 0x80004001 0.25 8.5 0.75\nmoving none 0x00000040 0.5 2.5 1\n";
            std::istringstream rawInput(rawText);
            const sed::assets::SoundClassFile imported = sed::assets::loadFromSoundClass(
                rawInput, boundary.profile);
            check(imported.entries.size() == 2U && imported.entries[0].wav == file.entries[0].wav
                    && imported.entries[0].playFlags == 0x80004001U
                    && imported.entries[1].wav == "none",
                "sound class engine boundary: bounded overlong engine WAV names remain exact when imported");
            bool rejected = false;
            try
            {
                (void)sed::assets::saveToSoundClassText(imported, boundary.profile);
            }
            catch ( const std::runtime_error& )
            {
                rejected = true;
            }
            check(rejected,
                "sound class engine boundary: external writer rejects WAV identity one byte beyond the selected engine buffer");
            std::ostringstream unchanged;
            unchanged << "sentinel";
            try
            {
                sed::assets::saveToSoundClassText(unchanged, imported, boundary.profile);
            }
            catch ( const std::runtime_error& )
            {
            }
            check(unchanged.str() == "sentinel",
                "sound class engine boundary: rejected WAV identity cannot partially append output");
        }
        checkThrowsWithMessage(
            []()
            {
                sed::assets::SoundClassFile file{};
                file.entries.push_back({ "moving", "sound.wav" });
                (void)sed::assets::saveToSoundClassText(file, static_cast<sed::assets::SoundClassProfile>(255U));
            },
            "Unknown sound-class profile.",
            "sound class engine boundary: writer rejects unknown profiles before emitting bytes");
        std::ostringstream unchanged;
        unchanged << "sentinel";
        checkThrowsWithMessage(
            [&unchanged]()
            {
                sed::assets::saveToSoundClassText(
                    unchanged, {}, static_cast<sed::assets::SoundClassProfile>(255U));
            },
            "Unknown sound-class profile.",
            "sound class engine boundary: stream writer rejects an unknown profile even for an empty file");
        check(unchanged.str() == "sentinel",
            "sound class engine boundary: an unknown profile leaves stream output unchanged");
    }

    {
        const std::array<std::string, 15> unsafeTokens{
            "", "two words", "mode#comment", "mode;comment", "mode//comment",
            "comma,mode", "equals=mode", "quoted\"mode", "line\nbreak", "tab\tbreak",
            "carriage\rbreak", "vertical\vbreak", "formfeed\fbreak",
            std::string("mode\x01", 5), std::string("mode\0tail", 9),
        };
        bool allUnsafeModesRejected = true;
        bool allUnsafeWavsRejected = true;
        for (const std::string& token : unsafeTokens)
        {
            sed::assets::SoundClassFile badMode{};
            badMode.entries.push_back({ token, "sound.wav" });
            allUnsafeModesRejected = allUnsafeModesRejected
                && saveSoundClassThrows(badMode);

            sed::assets::SoundClassFile badWav{};
            badWav.entries.push_back({ "moving", token });
            allUnsafeWavsRejected = allUnsafeWavsRejected
                && saveSoundClassThrows(badWav);
        }
        check(allUnsafeModesRejected && allUnsafeWavsRejected,
            "sound class: rejects whitespace, comment/separator, quote, control, and NUL token injection");

        sed::assets::SoundClassFile valid{};
        valid.entries.push_back({ "moving", "sound.wav" });
        const std::array<std::string, 6> malformedRows{
            "moving sound.wav invalid",
            "moving sound.wav 0x100000000",
            "moving sound.wav 0x40 invalid",
            "moving sound.wav 0x40 1e999",
            "moving sound.wav 0x40 nan",
            "moving sound.wav 0x40 0.5 2.5 inf",
        };
        bool allMalformedRejected = true;
        for (const std::string& row : malformedRows)
        {
            allMalformedRejected = allMalformedRejected
                && loadSoundClassThrows(row);
        }
        check(allMalformedRejected,
            "sound class: rejects malformed, overflowing, and non-finite authored numeric fields");

        const std::array<std::string, 3> invalidRanges{
            "moving sound.wav 0x40 -0.1 2.5 1",
            "moving sound.wav 0x40 3 2 1",
            "moving sound.wav 0x40 0.5 2.5 -0.1",
        };
        bool allInvalidRangesRejected = true;
        for (const std::string& row : invalidRanges)
        {
            allInvalidRangesRejected = allInvalidRangesRejected
                && loadSoundClassThrows(row);
        }
        check(allInvalidRangesRejected,
            "sound class: rejects negative radii, reversed radius bounds, and negative maximum volume");

        const std::string overlongRow = std::string(4097, 'm') + " sound.wav\n";
        const std::string overlongToken = std::string(256, 'm') + " sound.wav\n";
        std::string tooManyRows;
        tooManyRows.reserve(4097U * 4U);
        for (std::size_t row = 0; row < 4097U; ++row)
        {
            tooManyRows += "m w\n";
        }
        bool allResourceBoundsRejected = loadSoundClassThrows(overlongRow)
            && loadSoundClassThrows(overlongToken)
            && loadSoundClassThrows(tooManyRows);
        sed::assets::SoundClassFile overlongWriterToken{};
        overlongWriterToken.entries.push_back({ std::string(256, 'm'), "sound.wav" });
        allResourceBoundsRejected = allResourceBoundsRejected
            && saveSoundClassThrows(overlongWriterToken);
        check(allResourceBoundsRejected,
            "sound class: bounds row bytes, mode/WAV token size, and total entry count");

        sed::assets::SoundClassFile invalidWriter = valid;
        invalidWriter.entries[0].minRadius = 2.0f;
        invalidWriter.entries[0].maxRadius = 1.0f;
        bool rejectedInvalidRange = saveSoundClassThrows(invalidWriter);
        invalidWriter.entries[0].minRadius = 0.5f;
        invalidWriter.entries[0].maxRadius = 2.5f;
        invalidWriter.entries[0].maxVolume = std::numeric_limits<float>::quiet_NaN();
        rejectedInvalidRange = rejectedInvalidRange
            && saveSoundClassThrows(invalidWriter);
        check(rejectedInvalidRange,
            "sound class: writer applies the same range guards as the structured reader");

        std::ostringstream unchanged;
        unchanged << "sentinel";
        try
        {
            sed::assets::saveToSoundClassText(unchanged, invalidWriter);
        }
        catch (const std::runtime_error&)
        {
        }
        check(unchanged.str() == "sentinel",
            "sound class: validation failure does not partially append output");

        std::ostream failedOutput(nullptr);
        bool streamWriteRejected = false;
        try
        {
            sed::assets::saveToSoundClassText(failedOutput, valid);
        }
        catch (const std::runtime_error&)
        {
            streamWriteRejected = true;
        }
        check(streamWriteRejected && failedOutput.fail(),
            "sound class: stream device failure is reported and retained in stream state");
    }

    {
        class FailedReadBuffer final : public std::streambuf
        {
        protected:
            int_type underflow() override
            {
                throw std::runtime_error("synthetic input device failure");
            }
        } buffer;

        std::istream failedInput(&buffer);
        bool rejected = false;
        try
        {
            (void)sed::assets::loadFromSoundClass(failedInput);
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        check(rejected && failedInput.bad(),
            "sound class: source stream device failure is reported");
    }
}

} // namespace sed::tests
