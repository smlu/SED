#include "sed/assets/spr.hpp"

#include "sed/text.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sed::assets
{
namespace
{

using sed::text::splitWords;
using sed::text::stripLineComments;
using sed::text::trim;

constexpr std::size_t kMaxResourceTokenBytes = 255U;

bool isSafeResourceToken(const std::string_view token) noexcept
{
    if (token.empty() || token.size() > kMaxResourceTokenBytes)
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

std::string formatInteger(const int value)
{
    std::array<char, 32> buffer{};
    const auto formatted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (formatted.ec != std::errc{})
    {
        throw std::runtime_error("Failed to format SPR integer.");
    }
    return std::string(buffer.data(), formatted.ptr);
}

std::string formatFloat(const float value)
{
    std::array<char, 64> buffer{};
    const auto formatted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value,
        std::chars_format::general, std::numeric_limits<float>::max_digits10);
    if (formatted.ec != std::errc{})
    {
        throw std::runtime_error("Failed to format SPR number.");
    }
    return std::string(buffer.data(), formatted.ptr);
}

void appendField(std::string& text, const std::string_view field)
{
    if (!text.empty())
    {
        text.push_back(' ');
    }
    text.append(field);
}

void validateForSave(const SprFile& spr, const SprProfile profile)
{
    if (!isSafeResourceToken(spr.material))
    {
        throw std::runtime_error("SPR material must be a safe resource token.");
    }

    if (!std::isfinite(spr.width) || !std::isfinite(spr.height))
    {
        throw std::runtime_error("SPR dimensions must be finite.");
    }
    for (const float channel : spr.extraLightRGBA)
    {
        if (!std::isfinite(channel))
        {
            throw std::runtime_error("SPR extra light must be finite.");
        }
    }
    for (const float offset : spr.offsetXYZ)
    {
        if (!std::isfinite(offset))
        {
            throw std::runtime_error("SPR offsets must be finite.");
        }
    }

    switch ( profile )
    {
        case SprProfile::classic:
            if ( spr.material.size() > 31U )
            {
                throw std::runtime_error("Classic SPR material names are limited to 31 bytes.");
            }
            if (spr.extraLightRGBA[0] != spr.extraLightRGBA[1]
                || spr.extraLightRGBA[0] != spr.extraLightRGBA[2]
                || spr.extraLightRGBA[3] != 1.0f)
            {
                throw std::runtime_error(
                    "Classic SPR cannot preserve colored or non-opaque extra light.");
            }
            break;
        case SprProfile::ijim:
            if ( spr.material.size() > 63U )
            {
                throw std::runtime_error("IJIM SPR material names are limited to 63 bytes.");
            }
            break;
        default:
            throw std::runtime_error("Unknown SPR profile.");
    }
}

std::string serializeSPR(const SprFile& spr, const SprProfile profile)
{
    validateForSave(spr, profile);

    std::string text{};
    text.reserve(192U);
    appendField(text, spr.material);
    appendField(text, formatInteger(spr.type));
    appendField(text, formatFloat(spr.width));
    appendField(text, formatFloat(spr.height));
    appendField(text, formatInteger(spr.geometryMode));
    appendField(text, formatInteger(spr.lightMode));
    appendField(text, formatInteger(spr.textureMode));
    if (profile == SprProfile::classic)
    {
        appendField(text, formatFloat(spr.extraLightRGBA[0]));
    }
    else
    {
        for (const float channel : spr.extraLightRGBA)
        {
            appendField(text, formatFloat(channel));
        }
    }
    for (const float offset : spr.offsetXYZ)
    {
        appendField(text, formatFloat(offset));
    }
    text.push_back('\n');
    return text;
}

bool parseFloat(std::string_view text, float& out) noexcept
{
    // SPR numbers use a dot regardless of the process C numeric locale.
    // from_chars excludes a leading '+', which the legacy parser accepted.
    if (text.empty()) return false;
    if (text.front() == '+')
    {
        text.remove_prefix(1);
        if (text.empty() || text.front() == '+' || text.front() == '-') return false;
    }
    const bool negative = text.front() == '-';
    const std::size_t prefix = negative ? 1u : 0u;
    const bool hexadecimal = text.size() >= prefix + 2u
        && text[prefix] == '0' && (text[prefix + 1] == 'x' || text[prefix + 1] == 'X');
    if (hexadecimal)
    {
        // stof also accepted C hexadecimal floats. from_chars' hex mode
        // requires omitting 0x; retain the authored sign independently.
        text.remove_prefix(prefix + 2u);
        if (text.empty() || text.front() == '+' || text.front() == '-') return false;
    }
    float value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value,
        hexadecimal ? std::chars_format::hex : std::chars_format::general);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(value))
        return false;
    if (hexadecimal && negative) value = -value;
    out = value;
    return true;
}

bool parseInt(std::string_view text, int& out) noexcept
{
    if (text.empty()) return false;
    if (text.front() == '+')
    {
        text.remove_prefix(1);
        if (text.empty() || text.front() == '+' || text.front() == '-') return false;
    }
    int value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return false;
    out = value;
    return true;
}

} // namespace

std::string matFromSPR(std::istream& in)
{
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
        if (!words.empty())
        {
            return std::string(words[0]);
        }
    }

    if (in.bad() || (in.fail() && !in.eof()))
    {
        throw std::runtime_error("Failed to read SPR.");
    }

    return {};
}

SprFile loadFromSPR(std::istream& in)
{
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
        if (words.empty())
        {
            continue;
        }

        // IJIM: exactly 14 args. JKDF2/MOTS: at least 11 args.
        if (!isSafeResourceToken(words[0]))
        {
            throw std::runtime_error("SPR material must be a safe resource token.");
        }

        SprFile spr{};
        spr.material = std::string(words[0]);

        if (words.size() == 14)
        {
            // IJIM (RGBA extra light + xyz offset)
            const bool valid = parseInt(words[1], spr.type)
                && parseFloat(words[2], spr.width)
                && parseFloat(words[3], spr.height)
                && parseInt(words[4], spr.geometryMode)
                && parseInt(words[5], spr.lightMode)
                && parseInt(words[6], spr.textureMode)
                && parseFloat(words[7], spr.extraLightRGBA[0])
                && parseFloat(words[8], spr.extraLightRGBA[1])
                && parseFloat(words[9], spr.extraLightRGBA[2])
                && parseFloat(words[10], spr.extraLightRGBA[3])
                && parseFloat(words[11], spr.offsetXYZ[0])
                && parseFloat(words[12], spr.offsetXYZ[1])
                && parseFloat(words[13], spr.offsetXYZ[2]);
            if ( !valid )
            {
                throw std::runtime_error("Bad numeric data in IJIM SPR.");
            }

            return spr;
        }

        if (words.size() >= 11)
        {
            // JKDF2/MOTS (single extra-light intensity + xyz offset)
            float extraIntensity = 0.0f;
            const bool valid = parseInt(words[1], spr.type)
                && parseFloat(words[2], spr.width)
                && parseFloat(words[3], spr.height)
                && parseInt(words[4], spr.geometryMode)
                && parseInt(words[5], spr.lightMode)
                && parseInt(words[6], spr.textureMode)
                && parseFloat(words[7], extraIntensity)
                && parseFloat(words[8], spr.offsetXYZ[0])
                && parseFloat(words[9], spr.offsetXYZ[1])
                && parseFloat(words[10], spr.offsetXYZ[2]);
            if ( !valid )
            {
                throw std::runtime_error("Bad numeric data in JKDF2/MotS SPR.");
            }

            spr.extraLightRGBA[0] = extraIntensity;
            spr.extraLightRGBA[1] = extraIntensity;
            spr.extraLightRGBA[2] = extraIntensity;
            spr.extraLightRGBA[3] = 1.0f;

            return spr;
        }

        throw std::runtime_error("Bad data in SPR (expected 14 args for IJIM or >=11 for JKDF2/MOTS).");
    }

    if (in.bad() || (in.fail() && !in.eof()))
    {
        throw std::runtime_error("Failed to read SPR.");
    }

    throw std::runtime_error("Empty SPR.");
}

std::string matFromSPR(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("Failed to open SPR: " + path.string());
    }

    return matFromSPR(in);
}

SprFile loadFromSPR(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("Failed to open SPR: " + path.string());
    }

    return loadFromSPR(in);
}

std::string saveToSPRText(const SprFile& spr, const SprProfile profile)
{
    return serializeSPR(spr, profile);
}

void saveToSPRText(std::ostream& out, const SprFile& spr, const SprProfile profile)
{
    const std::string text = serializeSPR(spr, profile);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out)
    {
        throw std::runtime_error("Failed to write SPR.");
    }
}

} // namespace sed::assets
