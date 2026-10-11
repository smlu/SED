#include "sed/assets/cog_script.hpp"

#include "sed/text.hpp"

#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace sed::assets
{
namespace
{

using sed::text::iequalsAscii;

struct CogSymbolType
{
    CogType cogType{ CogType::unk };
    ValueType valueType{ ValueType::unknown };
};

struct CogToken
{
    std::string text{};
    bool quoted{ false };
    bool startsWithQuote{ false };
};

struct ParsedAttribute
{
    std::string name{};
    std::optional<std::string> value{};
    std::size_t nextIndex{};
};

struct ParsedSymbol
{
    CogValue value{};
    std::optional<std::string> explicitValue{};
    bool invalidAttribute{};
};

bool isCogEntityRefType(const CogType type) noexcept
{
    return type == CogType::cog
        || type == CogType::sec
        || type == CogType::surface
        || type == CogType::thing;
}

bool isCogSymbolAttributeName(const std::string_view name) noexcept
{
    return iequalsAscii(name, "local")
        || iequalsAscii(name, "nolink")
        || iequalsAscii(name, "desc")
        || iequalsAscii(name, "mask")
        || iequalsAscii(name, "linkid");
}

std::string_view attributeName(const std::string_view token) noexcept
{
    const std::size_t eq = token.find('=');
    return eq == std::string_view::npos ? token : token.substr(0, eq);
}

bool isCogSymbolAttributeToken(const std::vector<CogToken>& tokens, const std::size_t index) noexcept
{
    if (index >= tokens.size() || tokens[index].startsWithQuote)
    {
        return false;
    }

    return isCogSymbolAttributeName(attributeName(tokens[index].text));
}

std::optional<std::uint32_t> parseCogMaskValue(const std::string_view text) noexcept
{
    const std::string_view trimmed = sed::text::trimView(text);
    if (trimmed.empty())
    {
        return std::nullopt;
    }
    if (trimmed == "-1")
    {
        return 0xFFFFu;
    }
    // The engine reads masks with scanf's `%x`; bare decimal-looking tokens
    // therefore remain hexadecimal (`10` means `0x10`).
    return sed::text::parseHex(trimmed);
}

std::optional<std::int32_t> parseCogIntegerPrefix(const std::string_view text) noexcept
{
    std::string_view token = sed::text::trimView(text);
    bool negative = false;
    if (!token.empty() && (token.front() == '+' || token.front() == '-'))
    {
        negative = token.front() == '-';
        token.remove_prefix(1);
    }

    std::size_t digitCount = 0;
    while (digitCount < token.size()
        && token[digitCount] >= '0' && token[digitCount] <= '9')
    {
        ++digitCount;
    }

    // Match atoi's zero result for a token without a numeric prefix while
    // retaining a bounded conversion for actual numeric input.
    if (digitCount == 0)
    {
        return std::int32_t{ 0 };
    }

    std::uint64_t magnitude = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + digitCount, magnitude, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + digitCount)
    {
        return std::nullopt;
    }

    constexpr std::uint64_t kPositiveLimit = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
    constexpr std::uint64_t kNegativeLimit = kPositiveLimit + 1u;
    const std::uint64_t limit = negative ? kNegativeLimit : kPositiveLimit;
    if (magnitude > limit)
    {
        return std::nullopt;
    }
    if (negative)
    {
        if (magnitude == kNegativeLimit)
        {
            return std::numeric_limits<std::int32_t>::min();
        }
        return -static_cast<std::int32_t>(magnitude);
    }
    return static_cast<std::int32_t>(magnitude);
}

std::optional<std::string> normalizeCogVector(const std::string_view text)
{
    std::string normalized(sed::text::trimView(text));
    if (normalized.empty())
    {
        return std::nullopt;
    }

    const bool hasOpeningParenthesis = normalized.front() == '(';
    const bool hasClosingParenthesis = normalized.back() == ')';
    if (hasOpeningParenthesis != hasClosingParenthesis)
    {
        return std::nullopt;
    }
    if (hasOpeningParenthesis)
    {
        normalized.erase(normalized.begin());
        normalized.pop_back();
    }

    for (char& ch : normalized)
    {
        if (ch == '/' || ch == ':' || ch == ',' || ch == ';')
        {
            ch = ' ';
        }
    }
    const std::vector<std::string_view> parts = sed::text::splitWords(normalized);
    if (parts.size() != 3)
    {
        return std::nullopt;
    }
    for (const std::string_view part : parts)
    {
        const std::optional<double> parsed = sed::text::parseDouble(part);
        if (!parsed || !std::isfinite(*parsed))
        {
            return std::nullopt;
        }
    }

    std::string canonical{"("};
    canonical += parts[0];
    canonical.push_back('/');
    canonical += parts[1];
    canonical.push_back('/');
    canonical += parts[2];
    canonical.push_back(')');
    return canonical;
}

bool isValidCogSurfacePair(const std::string_view text)
{
    const std::vector<std::string_view> parts = sed::text::splitWords(text);
    if (parts.size() != 2)
    {
        return false;
    }
    return sed::text::parseInt<std::int32_t>(parts[0]).has_value()
        && sed::text::parseInt<std::int32_t>(parts[1]).has_value();
}

CogSymbolType cogSymbolTypeFromName(const std::string_view name) noexcept
{
    if (iequalsAscii(name, "flex") || iequalsAscii(name, "float"))
    {
        return CogSymbolType{ CogType::float_, ValueType::floating };
    }
    if (iequalsAscii(name, "vector") || iequalsAscii(name, "vect"))
    {
        return CogSymbolType{ CogType::vect, ValueType::vect };
    }
    if (iequalsAscii(name, "int") || iequalsAscii(name, "integer"))
    {
        return CogSymbolType{ CogType::int_, ValueType::integer };
    }
    if (iequalsAscii(name, "sector") || iequalsAscii(name, "sec"))
    {
        return CogSymbolType{ CogType::sec, ValueType::ptr };
    }
    if (iequalsAscii(name, "surface") || iequalsAscii(name, "srf"))
    {
        return CogSymbolType{ CogType::surface, ValueType::ptr };
    }
    if (iequalsAscii(name, "thing") || iequalsAscii(name, "thg"))
    {
        return CogSymbolType{ CogType::thing, ValueType::ptr };
    }
    if (iequalsAscii(name, "cog"))
    {
        return CogSymbolType{ CogType::cog, ValueType::integer };
    }
    if (iequalsAscii(name, "ai"))
    {
        return CogSymbolType{ CogType::ai, ValueType::str };
    }
    if (iequalsAscii(name, "keyframe") || iequalsAscii(name, "key"))
    {
        return CogSymbolType{ CogType::key, ValueType::str };
    }
    if (iequalsAscii(name, "material") || iequalsAscii(name, "mat"))
    {
        return CogSymbolType{ CogType::mat, ValueType::str };
    }
    if (iequalsAscii(name, "message") || iequalsAscii(name, "msg"))
    {
        return CogSymbolType{ CogType::msg, ValueType::str };
    }
    if (iequalsAscii(name, "model") || iequalsAscii(name, "3do") || iequalsAscii(name, "model3do"))
    {
        return CogSymbolType{ CogType::model3do, ValueType::str };
    }
    if (iequalsAscii(name, "sound") || iequalsAscii(name, "wav"))
    {
        return CogSymbolType{ CogType::wav, ValueType::str };
    }
    if (iequalsAscii(name, "template") || iequalsAscii(name, "tpl"))
    {
        return CogSymbolType{ CogType::template_, ValueType::str };
    }

    return CogSymbolType{ CogType::unk, ValueType::str };
}

std::optional<std::string> tryReadCogText(const ResourceLocator& resources, const std::string_view scriptName)
{
    if (const auto text = resources.tryReadText(scriptName))
    {
        return text;
    }

    if (!scriptName.ends_with(".cog"))
    {
        std::string withExt(scriptName);
        withExt += ".cog";
        return resources.tryReadText(withExt);
    }

    return std::nullopt;
}

bool quoteIsEscaped(const std::string_view line, const std::size_t quoteIndex) noexcept
{
    std::size_t backslashes = 0;
    for (std::size_t index = quoteIndex; index > 0;)
    {
        --index;
        if (line[index] != '\\')
        {
            break;
        }
        ++backslashes;
    }
    return (backslashes % 2u) != 0u;
}

bool isQuoteOpeningBoundary(const std::string_view line, const std::size_t quoteIndex) noexcept
{
    if (quoteIndex == 0)
    {
        return true;
    }

    const char previous = line[quoteIndex - 1];
    return std::isspace(static_cast<unsigned char>(previous)) != 0
        || previous == '='
        || previous == ',';
}

void stripLineComment(std::string& line) noexcept
{
    bool inSingleQuote = false;
    bool inDoubleQuote = false;

    for (std::size_t index = 0; index < line.size(); ++index)
    {
        const char ch = line[index];
        if (!inDoubleQuote && ch == '\'' && !quoteIsEscaped(line, index)
            && (inSingleQuote || isQuoteOpeningBoundary(line, index)))
        {
            inSingleQuote = !inSingleQuote;
            continue;
        }
        if (!inSingleQuote && ch == '"' && !quoteIsEscaped(line, index)
            && (inDoubleQuote || isQuoteOpeningBoundary(line, index)))
        {
            inDoubleQuote = !inDoubleQuote;
            continue;
        }
        if (inSingleQuote || inDoubleQuote)
        {
            continue;
        }

        if (ch == '#' || (ch == '/' && index + 1 < line.size() && line[index + 1] == '/'))
        {
            line.erase(index);
            return;
        }
    }
}

std::vector<CogToken> tokenizeSymbolLine(const std::string_view line, bool& balanced)
{
    std::vector<CogToken> out{};
    balanced = true;

    std::size_t index = 0;
    while (index < line.size())
    {
        while (index < line.size()
            && (std::isspace(static_cast<unsigned char>(line[index])) != 0 || line[index] == ','))
        {
            ++index;
        }
        if (index >= line.size())
        {
            break;
        }

        std::string token{};
        bool quoted = false;
        const bool startsWithQuote = line[index] == '\'' || line[index] == '"';
        bool tokenStarted = false;
        bool inSingleQuote = false;
        bool inDoubleQuote = false;
        while (index < line.size())
        {
            const char ch = line[index];
            if (!inSingleQuote && !inDoubleQuote
                && (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == ','))
            {
                break;
            }

            if (!inDoubleQuote && ch == '\'' && !quoteIsEscaped(line, index)
                && (inSingleQuote || isQuoteOpeningBoundary(line, index)))
            {
                inSingleQuote = !inSingleQuote;
                quoted = true;
                tokenStarted = true;
                ++index;
                continue;
            }
            if (!inSingleQuote && ch == '"' && !quoteIsEscaped(line, index)
                && (inDoubleQuote || isQuoteOpeningBoundary(line, index)))
            {
                inDoubleQuote = !inDoubleQuote;
                quoted = true;
                tokenStarted = true;
                ++index;
                continue;
            }

            token.push_back(ch);
            tokenStarted = true;
            ++index;
        }

        if (inSingleQuote || inDoubleQuote)
        {
            balanced = false;
            return {};
        }

        if (tokenStarted)
        {
            if (!quoted)
            {
                while (!token.empty() && token.back() == ';')
                {
                    token.pop_back();
                }
            }
            if (token.empty() && !quoted)
            {
                continue;
            }
            out.push_back(CogToken{ std::move(token), quoted, startsWithQuote });
        }
    }

    return out;
}

std::optional<ParsedAttribute> parseAttribute(
    const std::vector<CogToken>& tokens,
    const std::size_t index)
{
    if (index >= tokens.size())
    {
        return std::nullopt;
    }

    const std::string_view token = tokens[index].text;
    if (tokens[index].startsWithQuote)
    {
        return std::nullopt;
    }
    const std::size_t eq = token.find('=');
    const std::string_view name = eq == std::string_view::npos ? token : token.substr(0, eq);
    if (!isCogSymbolAttributeName(name))
    {
        return std::nullopt;
    }

    ParsedAttribute out{ std::string(name), std::nullopt, index + 1 };
    if (eq != std::string_view::npos)
    {
        const std::string_view value = token.substr(eq + 1);
        if (!value.empty())
        {
            out.value = std::string(value);
        }
        else if (out.nextIndex < tokens.size()
            && tokens[out.nextIndex].text != "="
            && (!isCogSymbolAttributeToken(tokens, out.nextIndex) || tokens[out.nextIndex].quoted))
        {
            out.value = tokens[out.nextIndex].text;
            ++out.nextIndex;
        }
        return out;
    }

    if (out.nextIndex < tokens.size() && tokens[out.nextIndex].text == "=")
    {
        ++out.nextIndex;
        if (out.nextIndex < tokens.size())
        {
            out.value = tokens[out.nextIndex].text;
            ++out.nextIndex;
        }
    }
    return out;
}

bool isValueBoundary(const std::vector<CogToken>& tokens, const std::size_t index) noexcept
{
    if (index >= tokens.size() || tokens[index].startsWithQuote)
    {
        return false;
    }

    const std::string_view token = tokens[index].text;
    return token == "="
        || token.find('=') != std::string_view::npos
        || isCogSymbolAttributeToken(tokens, index);
}

std::optional<ParsedSymbol> parseSymbolLine(const std::vector<CogToken>& tokens)
{
    if (tokens.size() < 2)
    {
        return std::nullopt;
    }

    const CogSymbolType symbolType = cogSymbolTypeFromName(tokens[0].text);
    CogValue value{};
    value.cogType = symbolType.cogType;
    value.type = symbolType.valueType;

    std::string_view name = tokens[1].text;
    std::optional<std::string> explicitValue{};
    std::size_t cursor = 2;

    if (const std::size_t eq = name.find('='); eq != std::string_view::npos)
    {
        if (eq == 0)
        {
            return std::nullopt;
        }
        explicitValue = std::string(name.substr(eq + 1));
        name = name.substr(0, eq);
    }
    else if (cursor < tokens.size() && tokens[cursor].text == "=")
    {
        explicitValue = std::string{};
        ++cursor;
    }
    else if (cursor < tokens.size() && tokens[cursor].text.starts_with('='))
    {
        explicitValue = tokens[cursor].text.substr(1);
        ++cursor;
    }

    value.name = std::string(name);
    if (value.name.empty())
    {
        return std::nullopt;
    }

    if (explicitValue)
    {
        if (explicitValue->empty()
            && cursor < tokens.size()
            && !isValueBoundary(tokens, cursor))
        {
            explicitValue = tokens[cursor].text;
            ++cursor;
        }

        if (value.type == ValueType::vect)
        {
            while (cursor < tokens.size()
                && !isValueBoundary(tokens, cursor)
                && !normalizeCogVector(*explicitValue))
            {
                if (!explicitValue->empty())
                {
                    explicitValue->push_back(' ');
                }
                explicitValue->append(tokens[cursor].text);
                ++cursor;
                if (normalizeCogVector(*explicitValue))
                {
                    break;
                }
            }
        }
        else if (value.cogType == CogType::surface
            && cursor < tokens.size()
            && !isValueBoundary(tokens, cursor))
        {
            // Surface references use the legacy `sector surface` pair. Keep
            // exactly one numeric continuation token as part of that value;
            // unrelated trailing metadata must remain independent of numeric
            // parsing.
            if (sed::text::parseInt<std::int32_t>(tokens[cursor].text))
            {
                explicitValue->push_back(' ');
                explicitValue->append(tokens[cursor].text);
                ++cursor;
            }
        }
    }

    while (cursor < tokens.size())
    {
        const std::optional<ParsedAttribute> attribute = parseAttribute(tokens, cursor);
        if (!attribute)
        {
            // The original configuration reader ignores unknown keys and
            // misspelled/bare metadata (for example `nolink1` or an old
            // descriptor word). Do the same without allowing those tokens to
            // become part of a typed default value.
            ++cursor;
            continue;
        }

        cursor = attribute->nextIndex;
        if (iequalsAscii(attribute->name, "local"))
        {
            value.local = true;
        }
        else if (iequalsAscii(attribute->name, "desc"))
        {
            value.description = attribute->value.value_or(std::string{});
        }
        else if (iequalsAscii(attribute->name, "mask"))
        {
            if (!attribute->value)
            {
                continue;
            }
            if (const auto parsed = parseCogMaskValue(*attribute->value))
            {
                value.mask = static_cast<std::int32_t>(*parsed);
            }
            else
            {
                return ParsedSymbol{ std::move(value), std::move(explicitValue), true };
            }
        }
    }

    return ParsedSymbol{ std::move(value), std::move(explicitValue) };
}

bool parseExplicitCogValue(CogValue& value, const std::string_view text)
{
    if (text.empty())
    {
        return true;
    }

    if (value.cogType == CogType::surface
        && text.find_first_of(" \t\r\n") != std::string_view::npos)
    {
        if (!isValidCogSurfacePair(text))
        {
            return false;
        }
        value.intValue = -1;
        value.stringValue = std::string(text);
        return true;
    }

    if (value.type == ValueType::vect)
    {
        const std::optional<std::string> canonical = normalizeCogVector(text);
        if (!canonical)
        {
            return false;
        }
        value.stringValue = *canonical;
        return true;
    }

    if (value.cogType == CogType::int_)
    {
        const std::optional<std::int32_t> parsedInteger = parseCogIntegerPrefix(text);
        if (!parsedInteger)
        {
            return false;
        }
        value.intValue = *parsedInteger;
        return true;
    }

    CogValue parsed = value;
    if (!parsed.parseJed(text))
    {
        return false;
    }
    if (parsed.type == ValueType::floating && !std::isfinite(parsed.floatValue))
    {
        return false;
    }
    value = std::move(parsed);
    return true;
}

void applySymbolDefault(CogValue& value)
{
    if (value.type == ValueType::vect && value.stringValue.empty())
    {
        value.stringValue = "(0/0/0)";
    }

    const bool isEntityIndex = value.type == ValueType::ptr
        || (value.cogType == CogType::cog && value.type == ValueType::integer);
    if (!value.local && isCogEntityRefType(value.cogType) && isEntityIndex)
    {
        value.intValue = -1;
    }
}

CogSymbolLoadResult makeFailure(
    const CogSymbolLoadStatus status,
    const std::string_view diagnostic)
{
    CogSymbolLoadResult result{};
    result.status = status;
    result.diagnostic = std::string(diagnostic);
    return result;
}

CogSymbolLoadResult parseCogSymbolsText(
    const std::string_view text,
    const bool includeLocals)
{
    CogSymbolLoadResult result{};
    result.status = CogSymbolLoadStatus::loaded;

    std::istringstream input{ std::string(text) };
    bool inSymbols = false;
    bool foundEnd = false;
    bool hadInvalidValue = false;
    bool hadInvalidDeclaration = false;
    std::string line{};
    while (std::getline(input, line))
    {
        stripLineComment(line);
        const std::string_view trimmed = sed::text::trimView(line);
        if (trimmed.empty())
        {
            continue;
        }

        bool balanced = true;
        const std::vector<CogToken> tokens = tokenizeSymbolLine(trimmed, balanced);
        if (!balanced)
        {
            return makeFailure(CogSymbolLoadStatus::malformed, "COG script contains an unterminated quoted value.");
        }
        if (tokens.empty())
        {
            continue;
        }

        if (!inSymbols)
        {
            if (iequalsAscii(tokens[0].text, "symbols"))
            {
                inSymbols = true;
            }
            continue;
        }

        if (iequalsAscii(tokens[0].text, "end"))
        {
            foundEnd = true;
            break;
        }

        // A few shipped scripts repeat the section marker or leave an orphan
        // `local` line in the symbols block. The engine's line-oriented
        // configuration reader ignores those standalone metadata markers;
        // accepting only these exact forms keeps ordinal/unknown declarations
        // malformed and avoids shifting public arguments.
        if (tokens.size() == 1
            && (iequalsAscii(tokens[0].text, "symbols")
                || iequalsAscii(tokens[0].text, "local")))
        {
            continue;
        }

        const std::optional<ParsedSymbol> parsed = parseSymbolLine(tokens);
        if (!parsed)
        {
            return makeFailure(CogSymbolLoadStatus::malformed, "COG symbols section contains an invalid declaration.");
        }

        CogValue value = parsed->value;
        applySymbolDefault(value);

        if (parsed->invalidAttribute)
        {
            hadInvalidValue = true;
        }

        if (value.cogType == CogType::unk)
        {
            hadInvalidDeclaration = true;
        }

        // The Sith symbol parser initializes declaration vectors to zero and
        // ignores any declaration initializer. Runtime level-instance vector
        // values are parsed separately by CogValue and retain their authored
        // values.
        if (parsed->explicitValue && value.type != ValueType::vect)
        {
            const std::string& explicitValue = *parsed->explicitValue;
            const bool parsedValue = parseExplicitCogValue(value, explicitValue);
            if (!parsedValue)
            {
                hadInvalidValue = true;
                if (value.cogType == CogType::surface
                    && explicitValue.find_first_of(" \t\r\n") != std::string::npos)
                {
                    value.intValue = -1;
                    value.stringValue = explicitValue;
                }
            }

            const bool isEntityIndex = value.type == ValueType::ptr
                || (value.cogType == CogType::cog && value.type == ValueType::integer);
            if (!value.local
                && isCogEntityRefType(value.cogType)
                && isEntityIndex
                && (explicitValue.empty() || !parsedValue))
            {
                value.intValue = -1;
            }
        }

        if (!includeLocals && (value.local || value.cogType == CogType::msg))
        {
            continue;
        }

        result.symbols.push_back(std::move(value));
    }

    if (!inSymbols || !foundEnd)
    {
        return makeFailure(CogSymbolLoadStatus::malformed, "COG script has no complete symbols section.");
    }

    if (hadInvalidValue)
    {
        result.status = CogSymbolLoadStatus::malformed;
        result.diagnostic = "COG symbols section contains an invalid value.";
    }

    if (hadInvalidDeclaration)
    {
        result.status = CogSymbolLoadStatus::malformed;
        result.diagnostic = "COG symbols section contains an unsupported declaration.";
    }

    return result;
}

CogSymbolLoadResult loadCogSymbols(
    const ResourceLocator& resources,
    const std::string_view scriptName,
    const bool includeLocals)
{
    std::optional<std::string> text{};
    try
    {
        text = tryReadCogText(resources, scriptName);
    }
    catch (const std::exception& exception)
    {
        return makeFailure(CogSymbolLoadStatus::missing, exception.what());
    }
    catch (...)
    {
        return makeFailure(CogSymbolLoadStatus::missing, "COG script could not be read.");
    }

    if (!text)
    {
        return makeFailure(CogSymbolLoadStatus::missing, "COG script resource was not found.");
    }

    return parseCogSymbolsText(*text, includeLocals);
}

} // namespace

CogSymbolLoadResult loadCogSymbolsNoLocalsResult(
    const ResourceLocator& resources,
    const std::string_view scriptName)
{
    return loadCogSymbols(resources, scriptName, false);
}

CogSymbolLoadResult loadCogSymbolsWithDefaultsResult(
    const ResourceLocator& resources,
    const std::string_view scriptName,
    const bool includeLocals)
{
    return loadCogSymbols(resources, scriptName, includeLocals);
}

CogSymbolLoadResult loadCogSymbolsFromTextResult(
    const std::string_view text,
    const bool includeLocals)
{
    return parseCogSymbolsText(text, includeLocals);
}

std::vector<CogValue> loadCogSymbolsNoLocals(const ResourceLocator& resources, const std::string_view scriptName)
{
    return loadCogSymbolsNoLocalsResult(resources, scriptName).symbols;
}

std::vector<CogValue> loadCogSymbolsWithDefaults(
    const ResourceLocator& resources,
    const std::string_view scriptName,
    const bool includeLocals)
{
    return loadCogSymbolsWithDefaultsResult(resources, scriptName, includeLocals).symbols;
}

} // namespace sed::assets
