#include "sed/assets/snd.hpp"

#include "sed/text.hpp"

#include <array>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace sed::assets
{

namespace
{

using sed::text::splitWords;
using sed::text::stripLineComments;
using sed::text::trim;
using sed::text::trimView;

constexpr std::size_t kMaxSoundClassEntries = 4096U;
constexpr std::size_t kMaxSoundClassLineBytes = 4096U;
constexpr std::size_t kMaxSoundClassTokenBytes = 255U;

// OpenJKDF2's sithSoundClass_aKeys, excluding the reserved index-zero sentinel.
constexpr std::string_view kClassicSoundClassModes[] = {
    "create", "activate", "startmove", "stopmove", "moving",
    "lwalkhard", "rwalkhard", "lrunhard", "rrunhard",
    "lwalkmetal", "rwalkmetal", "lrunmetal", "rrunmetal",
    "lwalkwater", "rwalkwater", "lrunwater", "rrunwater",
    "lwalkpuddle", "rwalkpuddle", "lrunpuddle", "rrunpuddle",
    "lwalkearth", "rwalkearth", "lrunearth", "rrunearth",
    "enterwater", "enterwaterslow", "exitwater", "exitwaterslow",
    "lswimsurface", "rswimsurface", "treadsurface",
    "lswimunder", "rswimunder", "treadunder",
    "jump", "jumpmetal", "jumpwater", "jumpearth",
    "landhard", "landmetal", "landwater", "landpuddle", "landearth", "landhurt",
    "hithard", "hitmetal", "hitearth", "deflected",
    "scrapehard", "scrapemetal", "scrapeearth", "hitdamaged",
    "falling", "corpsehit", "hurtimpact", "hurtenergy", "hurtfire", "hurtmagic", "hurtspecial",
    "drowning", "choking", "death1", "death2", "deathunder", "drowned", "splattered",
    "pant", "breath", "gasp", "fire1", "fire2", "fire3", "fire4",
    "curious", "alert", "idle", "gloat", "fear", "boast", "happy", "victory", "help",
    "flee", "search", "calm", "surprise",
    "reserved1", "reserved2", "reserved3", "reserved4",
    "reserved5", "reserved6", "reserved7", "reserved8",
};

// OpenJones3D's sithSoundClass_aSoundModeNames, excluding its reserved sentinel.
constexpr std::string_view kIjimSoundClassModes[] = {
    "create", "activate", "startmove", "stopmove", "moving",
    "lwalkhard", "rwalkhard", "lrunhard", "rrunhard",
    "lwalkmetal", "rwalkmetal", "lrunmetal", "rrunmetal",
    "lwalkwater", "rwalkwater", "lrunwater", "rrunwater",
    "lwalkpuddle", "rwalkpuddle", "lrunpuddle", "rrunpuddle",
    "lwalkearth", "rwalkearth", "lrunearth", "rrunearth",
    "lwalksnow", "rwalksnow", "lrunsnow", "rrunsnow",
    "lwalkwood", "rwalkwood", "lrunwood", "rrunwood",
    "lwalkhardecho", "rwalkhardecho", "lrunhardecho", "rrunhardecho",
    "lwalkwoodecho", "rwalkwoodecho", "lrunwoodecho", "rrunwoodecho",
    "lwalkearthecho", "rwalkearthecho", "lrunearthecho", "rrunearthecho",
    "lwalkaet", "rwalkaet", "lrunaet", "rrunaet",
    "enterwater", "enterwaterslow", "exitwater", "exitwaterslow",
    "lswimsurface", "rswimsurface", "treadsurface",
    "lswimunder", "rswimunder", "treadunder",
    "jump", "jumpmetal", "jumpwater", "jumpearth", "jumpsnow", "jumpwood",
    "jumphardecho", "jumpwoodecho", "jumpearthecho", "jumpaet",
    "landhard", "landmetal", "landwater", "landpuddle", "landearth", "landsnow", "landwood",
    "landhardecho", "landwoodecho", "landearthecho", "landaet", "landhurt",
    "hithard", "hitmetal", "hitearth", "deflected",
    "scrapehard", "scrapemetal", "scrapeearth", "hitdamaged",
    "falling", "corpsehit", "hurtimpact", "hurtenergy", "hurtfire", "hurtfists", "hurtmachete",
    "drowning", "death1", "death2", "deathunder", "drowned", "splattered",
    "pant", "breath", "gasp", "fire1", "fire2", "fire3", "fire4",
    "curious", "alert", "idle", "gloat", "fear", "boast", "slide1", "victory", "help",
    "flee", "search", "calm", "surprise",
    "reserved1", "reserved2", "reserved3", "reserved4",
    "reserved5", "reserved6", "reserved7", "reserved8",
    "stand2walk", "walk2stand", "climbhandleft", "climbhandright",
    "climbhoriz", "climbonto", "climbdownonto", "lswimaet", "rswimaet", "treadaet",
};

bool isSafeToken(const std::string_view token) noexcept
{
    if (token.empty() || token.size() > kMaxSoundClassTokenBytes)
    {
        return false;
    }

    for (std::size_t index = 0; index < token.size(); ++index)
    {
        const unsigned char character = static_cast<unsigned char>(token[index]);
        if (character <= 0x20U || character == 0x7FU
            || character == '#' || character == ';' || character == ','
            || character == '=' || character == '"'
            || (character == '/' && index + 1U < token.size() && token[index + 1U] == '/'))
        {
            return false;
        }
    }
    return true;
}

void validateSoundClassEntry(const SoundClassEntry& entry)
{
    if (!isSafeToken(entry.mode))
    {
        throw std::runtime_error("Sound-class mode must be a safe token of at most 255 bytes.");
    }
    if (!isSafeToken(entry.wav))
    {
        throw std::runtime_error("Sound-class WAV must be a safe token of at most 255 bytes.");
    }
    if (!std::isfinite(entry.minRadius) || !std::isfinite(entry.maxRadius)
        || !std::isfinite(entry.maxVolume))
    {
        throw std::runtime_error("Sound-class radii and volume must be finite.");
    }
    if (entry.minRadius < 0.0f || entry.maxRadius < 0.0f
        || entry.minRadius > entry.maxRadius || entry.maxVolume < 0.0f)
    {
        throw std::runtime_error(
            "Sound-class radii must be nonnegative and ordered; maximum volume must be nonnegative.");
    }
}

bool parseFiniteFloat(const std::string_view token, float& out) noexcept
{
    if (token.empty())
    {
        return false;
    }

    std::string_view number = token;
    bool positive = false;
    if (number.front() == '+')
    {
        number.remove_prefix(1U);
        positive = true;
        if (number.empty() || number.front() == '+' || number.front() == '-')
        {
            return false;
        }
    }

    float value{};
    const auto parsed = std::from_chars(
        number.data(), number.data() + number.size(), value, std::chars_format::general);
    if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size()
        || !std::isfinite(value))
    {
        return false;
    }
    if (positive && value < 0.0f)
    {
        return false;
    }
    out = value;
    return true;
}

std::string formatFloat(const float value)
{
    std::array<char, 64> buffer{};
    const auto formatted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value,
        std::chars_format::general, std::numeric_limits<float>::max_digits10);
    if (formatted.ec != std::errc{})
    {
        throw std::runtime_error("Failed to format sound-class number.");
    }
    return std::string(buffer.data(), formatted.ptr);
}

std::string formatFlags(const std::uint32_t flags)
{
    constexpr char kHexDigits[] = "0123456789ABCDEF";
    std::string text("0x00000000");
    for (std::size_t index = 0; index < 8U; ++index)
    {
        const unsigned shift = static_cast<unsigned>((7U - index) * 4U);
        const std::uint32_t digit = (flags >> shift) & 0x0FU;
        text[2U + index] = kHexDigits[digit];
    }
    return text;
}

void appendField(std::string& text, const std::string_view field)
{
    if (!text.empty() && text.back() != '\n')
    {
        text.push_back(' ');
    }
    text.append(field);
}

std::string serializeSoundClass(const SoundClassFile& file, const SoundClassProfile profile)
{
    if ( profile != SoundClassProfile::classic && profile != SoundClassProfile::ijim )
    {
        throw std::runtime_error("Unknown sound-class profile.");
    }
    if ( file.entries.size() > kMaxSoundClassEntries )
    {
        throw std::runtime_error("Sound class exceeds the 4096-row limit.");
    }

    std::string text{};
    for ( const SoundClassEntry& entry : file.entries )
    {
        validateSoundClassEntry(entry);
        if ( entry.wav.size() > (profile == SoundClassProfile::classic ? 31U : 121U) )
        {
            throw std::runtime_error(profile == SoundClassProfile::classic
                ? "Classic sound-class WAV names are limited to 31 bytes."
                : "IJIM sound-class WAV names are limited to 121 bytes.");
        }
        appendField(text, entry.mode);
        appendField(text, entry.wav);
        appendField(text, formatFlags(entry.playFlags));
        appendField(text, formatFloat(entry.minRadius));
        appendField(text, formatFloat(entry.maxRadius));
        appendField(text, formatFloat(entry.maxVolume));
        text.push_back('\n');
    }
    return text;
}

bool readBoundedLine(std::istream& in, std::string& line)
{
    line.clear();
    while (true)
    {
        const std::istream::int_type value = in.get();
        if (std::istream::traits_type::eq_int_type(value, std::istream::traits_type::eof()))
        {
            if (in.bad() || (in.fail() && !in.eof()))
            {
                throw std::runtime_error("Failed to read sound class.");
            }
            return !line.empty();
        }

        const char character = std::istream::traits_type::to_char_type(value);
        if (character == '\n')
        {
            return true;
        }
        if (line.size() == kMaxSoundClassLineBytes)
        {
            throw std::runtime_error("Sound-class line exceeds the 4096-byte limit.");
        }
        line.push_back(character);
    }
}

[[noreturn]] void badSoundClassRow(const std::size_t lineNumber, const std::string_view reason)
{
    throw std::runtime_error(
        "Bad sound-class data on line " + std::to_string(lineNumber) + ": " + std::string(reason) + ".");
}

SoundClassFile loadSoundClass(std::istream& in, const SoundClassProfile profile)
{
    if (profile != SoundClassProfile::classic && profile != SoundClassProfile::ijim)
    {
        throw std::runtime_error("Unknown sound-class profile.");
    }

    SoundClassFile file{};
    std::string line{};
    line.reserve(kMaxSoundClassLineBytes);
    std::size_t lineNumber = 0U;
    std::uint32_t inheritedFlags = 0x40U;

    while (readBoundedLine(in, line))
    {
        ++lineNumber;
        stripLineComments(line);
        const std::string_view trimmed = trimView(line);
        if (trimmed.empty())
        {
            continue;
        }

        const std::vector<std::string_view> words = splitWords(trimmed);
        if (words.size() < 2U)
        {
            continue;
        }
        for (const std::string_view word : words)
        {
            if (word.size() > kMaxSoundClassTokenBytes)
            {
                badSoundClassRow(lineNumber, "a token exceeds the 255-byte limit");
            }
        }
        if (!isSafeToken(words[0]) || !isSafeToken(words[1]))
        {
            badSoundClassRow(lineNumber, "mode and WAV names must be safe tokens");
        }
        if (file.entries.size() == kMaxSoundClassEntries)
        {
            badSoundClassRow(lineNumber, "sound class exceeds the 4096-row limit");
        }

        SoundClassEntry entry{};
        entry.mode = std::string(words[0]);
        entry.wav = std::string(words[1]);
        // Classic SED sound classes do not inherit an omitted flags field:
        // every row starts from the engine's 0x40 default. IJIM carries the
        // last explicit flags value from a recognized mode into later rows.
        entry.playFlags = profile == SoundClassProfile::ijim ? inheritedFlags : 0x40U;
        const std::span<const std::string_view> knownModes = soundClassModeNames(profile);
        const bool recognizedMode = std::find(knownModes.begin(), knownModes.end(), words[0])
            != knownModes.end();
        if (words.size() > 2U)
        {
            const std::optional<std::uint32_t> flags = sed::text::parseHex(words[2]);
            if (!flags)
            {
                badSoundClassRow(lineNumber, "invalid hexadecimal playback flags");
            }
            entry.playFlags = *flags;
            if ( profile == SoundClassProfile::ijim && recognizedMode )
            {
                inheritedFlags = *flags;
            }
        }

        std::array<float*, 3> values{ &entry.minRadius, &entry.maxRadius, &entry.maxVolume };
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            const std::size_t wordIndex = index + 3U;
            if (words.size() > wordIndex && !parseFiniteFloat(words[wordIndex], *values[index]))
            {
                badSoundClassRow(lineNumber, "invalid or non-finite playback value");
            }
        }
        try
        {
            validateSoundClassEntry(entry);
        }
        catch (const std::runtime_error&)
        {
            badSoundClassRow(lineNumber, "playback values are outside the supported ranges");
        }
        file.entries.push_back(std::move(entry));
    }

    if (in.bad() || (in.fail() && !in.eof()))
    {
        throw std::runtime_error("Failed to read sound class.");
    }
    return file;
}

} // namespace

SndFile loadFromSND(std::istream& in)
{
    SndFile file{};

    std::string line;
    while (std::getline(in, line))
    {
        stripLineComments(line);
        const std::string trimmed = trim(line);
        if (trimmed.empty())
        {
            continue;
        }

        const auto words = splitWords(trimmed);
        if (words.size() < 2)
        {
            continue;
        }

        file.wavs.emplace_back(words[1]);
    }

    if (in.bad() || (in.fail() && !in.eof()))
    {
        throw std::runtime_error("Failed to read SND.");
    }

    return file;
}

SndFile loadFromSND(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("Failed to open SND: " + path.string());
    }

    return loadFromSND(in);
}

std::span<const std::string_view> soundClassModeNames(const SoundClassProfile profile) noexcept
{
    switch (profile)
    {
        case SoundClassProfile::classic:
            return kClassicSoundClassModes;
        case SoundClassProfile::ijim:
            return kIjimSoundClassModes;
        default:
            return {};
    }
}

SoundClassFile loadFromSoundClass(std::istream& in, const SoundClassProfile profile)
{
    return loadSoundClass(in, profile);
}

SoundClassFile loadFromSoundClass(
    const std::filesystem::path& path, const SoundClassProfile profile)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Failed to open sound class: " + path.string());
    }
    return loadSoundClass(in, profile);
}

std::string saveToSoundClassText(const SoundClassFile& file, const SoundClassProfile profile)
{
    return serializeSoundClass(file, profile);
}

void saveToSoundClassText(
    std::ostream& out, const SoundClassFile& file, const SoundClassProfile profile)
{
    const std::string text = serializeSoundClass(file, profile);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if ( !out )
    {
        throw std::runtime_error("Failed to write sound class.");
    }
}

} // namespace sed::assets
