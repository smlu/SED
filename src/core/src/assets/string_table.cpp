#include "sed/assets/string_table.hpp"

#include "sed/text.hpp"

#include <charconv>
#include <cctype>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace sed::assets
{
namespace
{

constexpr std::size_t kMaxStringTableEntries = 4096U;
constexpr std::size_t kMaxStringTableLineBytes = 4096U;
constexpr std::size_t kMaxStringTableFieldBytes = 255U;
constexpr std::size_t kClassicRecordBytes = 253U;
constexpr std::size_t kIJimRecordBytes = 273U;

bool isKnownProfile(const UniProfile profile) noexcept
{
    return profile == UniProfile::classic || profile == UniProfile::ijim;
}

[[noreturn]] void parseError(
    const std::string_view sourceName,
    const std::size_t lineNumber,
    const std::string_view detail)
{
    throw std::runtime_error(
        std::string(sourceName) + " parse error at line " + std::to_string(lineNumber)
        + (detail.empty() ? std::string{} : ": " + std::string(detail)));
}

[[noreturn]] void writerError(const std::string_view detail)
{
    throw std::runtime_error("UNI writer: " + std::string(detail));
}

void validateInputLine(
    const std::string_view line,
    const std::size_t lineNumber,
    const std::string_view sourceName)
{
    if ( line.find('\0') != std::string_view::npos || line.find('\x1a') != std::string_view::npos )
    {
        parseError(sourceName, lineNumber, "embedded NUL/DOS EOF is not supported");
    }
}

bool readBoundedLine(
    std::istream& input,
    std::string& line,
    std::size_t& lineNumber,
    const std::string_view sourceName)
{
    line.clear();
    while ( true )
    {
        const std::istream::int_type next = input.get();
        if ( std::istream::traits_type::eq_int_type(next, std::istream::traits_type::eof()) )
        {
            if ( input.bad() || (input.fail() && !input.eof()) )
            {
                parseError(sourceName, lineNumber, "failed while reading input");
            }
            if ( !line.empty() )
            {
                ++lineNumber;
                validateInputLine(line, lineNumber, sourceName);
                return true;
            }
            return false;
        }

        const char character = std::istream::traits_type::to_char_type(next);
        if ( character == '\n' )
        {
            ++lineNumber;
            if ( !line.empty() && line.back() == '\r' )
            {
                line.pop_back();
            }
            validateInputLine(line, lineNumber, sourceName);
            return true;
        }
        if ( line.size() == kMaxStringTableLineBytes )
        {
            parseError(sourceName, lineNumber + 1U, "line exceeds the 4096-byte editor limit");
        }
        line.push_back(character);
    }
}

std::string stripComments(std::string line, const UniProfile profile)
{
    if ( profile == UniProfile::ijim )
    {
        const std::string_view trimmed = sed::text::trimView(line);
        if ( !trimmed.empty() && trimmed.front() == '#' )
        {
            return {};
        }
        return std::string(trimmed);
    }

    bool quoted = false;
    for ( std::size_t index = 0; index < line.size(); ++index )
    {
        if ( line[index] == '"' )
        {
            quoted = !quoted;
        }
        else if ( line[index] == '#' && !quoted )
        {
            line.resize(index);
            break;
        }
    }
    const std::string_view trimmed = sed::text::trimView(line);
    return std::string(trimmed.begin(), trimmed.end());
}

void skipWhitespace(const std::string_view line, std::size_t& cursor) noexcept
{
    while ( cursor < line.size()
        && std::isspace(static_cast<unsigned char>(line[cursor])) != 0 )
    {
        ++cursor;
    }
}

std::optional<std::string_view> parseFirstQuotedField(
    const std::string_view line,
    std::size_t& cursor)
{
    skipWhitespace(line, cursor);
    if ( cursor >= line.size() || line[cursor] != '"' )
    {
        return std::nullopt;
    }

    const std::size_t first = cursor + 1U;
    const std::size_t closing = line.find('"', first);
    if ( closing == std::string_view::npos )
    {
        return std::nullopt;
    }
    cursor = closing + 1U;
    return line.substr(first, closing - first);
}

std::optional<std::int32_t> parseNumericField(
    const std::string_view line,
    std::size_t& cursor) noexcept
{
    skipWhitespace(line, cursor);
    const std::size_t first = cursor;
    while ( cursor < line.size()
        && std::isspace(static_cast<unsigned char>(line[cursor])) == 0 )
    {
        ++cursor;
    }
    if ( first == cursor )
    {
        return std::nullopt;
    }

    std::int32_t value{};
    const char* begin = line.data() + first;
    const char* const end = line.data() + cursor;
    // std::from_chars intentionally rejects a leading plus, while the game's
    // strtol-based parser accepts it. Keep the numeric field bounded and
    // overflow checked while matching that input contract.
    if ( *begin == '+' )
    {
        ++begin;
        if ( begin == end || *begin < '0' || *begin > '9' )
        {
            return std::nullopt;
        }
        const auto parsed = std::from_chars(begin, end, value, 10);
        if ( parsed.ec != std::errc{} || parsed.ptr != end )
        {
            return std::nullopt;
        }
        return value;
    }
    const auto parsed = std::from_chars(begin, end, value, 10);
    if ( parsed.ec != std::errc{} || parsed.ptr != end )
    {
        return std::nullopt;
    }
    return value;
}

std::optional<std::string_view> parseValueField(
    const std::string_view line,
    std::size_t& cursor,
    const UniProfile profile)
{
    skipWhitespace(line, cursor);
    if ( cursor >= line.size() || line[cursor] != '"' )
    {
        return std::nullopt;
    }

    const std::size_t first = cursor + 1U;
    std::size_t closing = std::string_view::npos;
    if ( profile == UniProfile::classic )
    {
        closing = line.find('"', first);
    }
    else
    {
        closing = line.rfind('"');
        if ( closing < first )
        {
            closing = std::string_view::npos;
        }
    }
    if ( closing == std::string_view::npos )
    {
        return std::nullopt;
    }

    cursor = closing + 1U;
    skipWhitespace(line, cursor);
    if ( cursor != line.size() )
    {
        return std::nullopt;
    }
    return line.substr(first, closing - first);
}

bool hasForbiddenStringByte(const std::string_view value) noexcept
{
    return value.find('\0') != std::string_view::npos
        || value.find('\x1a') != std::string_view::npos
        || value.find('\r') != std::string_view::npos
        || value.find('\n') != std::string_view::npos;
}

void validateEntry(
    const StringTableEntry& entry,
    const UniProfile profile,
    const bool forWriter)
{
    const auto fail = [forWriter](const std::string_view detail)
    {
        if ( forWriter )
        {
            writerError(detail);
        }
        throw std::runtime_error(std::string(detail));
    };

    const std::string_view trimmedKey = sed::text::trimView(entry.key);
    if ( entry.key.empty()
        || ( profile == UniProfile::classic && trimmedKey.size() != entry.key.size() ) )
    {
        fail("string-table keys must be non-empty and have no surrounding whitespace");
    }
    if ( entry.key.size() > kMaxStringTableFieldBytes
        || entry.value.size() > kMaxStringTableFieldBytes )
    {
        fail("string-table keys and values are limited to 255 game bytes");
    }
    if ( entry.key.find('"') != std::string::npos
        || hasForbiddenStringByte(entry.key)
        || hasForbiddenStringByte(entry.value) )
    {
        fail("string-table records cannot contain a quote in the key or an embedded line break/NUL");
    }
    if ( profile == UniProfile::classic && entry.value.find('"') != std::string::npos )
    {
        fail("classic string-table text cannot contain a double quote");
    }
}

std::string duplicateIdentity(const std::string_view key)
{
    return std::string(key);
}

std::size_t maxRecordBytes(const UniProfile profile) noexcept
{
    return profile == UniProfile::classic ? kClassicRecordBytes : kIJimRecordBytes;
}

std::size_t recordLength(const StringTableEntry& entry)
{
    return entry.key.size() + std::to_string(entry.numericValue).size()
        + entry.value.size() + 6U;
}

} // namespace

StringTableFile loadFromStringTableUni(
    std::istream& input,
    const UniProfile profile,
    const std::string_view sourceName)
{
    if ( !isKnownProfile(profile) )
    {
        throw std::runtime_error("Unknown UNI profile.");
    }

    const std::string source = sourceName.empty() ? "<string-table>" : std::string(sourceName);
    StringTableFile result{};
    std::string line{};
    line.reserve(kMaxStringTableLineBytes);
    std::size_t lineNumber = 0U;
    bool sawHeader = false;
    bool sawEnd = false;
    std::unordered_set<std::string> keys{};

    while ( readBoundedLine(input, line, lineNumber, source) )
    {
        const std::size_t physicalLineBytes = line.size();
        line = stripComments(std::move(line), profile);
        if ( line.empty() )
        {
            continue;
        }

        if ( !sawHeader )
        {
            const std::vector<std::string_view> words = sed::text::splitWords(line);
            std::uint32_t capacity{};
            if ( words.size() != 2U
                || !sed::text::iequalsAscii(words[0], "MSGS") )
            {
                parseError(source, lineNumber, "expected MSGS capacity header");
            }
            const auto parsed = std::from_chars(
                words[1].data(), words[1].data() + words[1].size(), capacity, 10);
            if ( parsed.ec != std::errc{}
                || parsed.ptr != words[1].data() + words[1].size() )
            {
                parseError(source, lineNumber, "invalid MSGS capacity");
            }
            if ( capacity > kMaxStringTableEntries )
            {
                parseError(source, lineNumber, "MSGS capacity exceeds the 4096-row editor limit");
            }
            result.capacity = capacity;
            sawHeader = true;
            continue;
        }

        if ( sed::text::iequalsAscii(line, "END") )
        {
            sawEnd = true;
            break;
        }
        if ( physicalLineBytes > maxRecordBytes(profile) )
        {
            parseError(source, lineNumber, "message record exceeds the engine line limit");
        }
        if ( result.entries.size() == kMaxStringTableEntries )
        {
            parseError(source, lineNumber, "table exceeds the 4096-row editor limit");
        }

        std::size_t cursor = 0U;
        const std::optional<std::string_view> key = parseFirstQuotedField(line, cursor);
        if ( !key )
        {
            parseError(source, lineNumber, "message key must be quoted");
        }
        const std::optional<std::int32_t> number = parseNumericField(line, cursor);
        if ( !number )
        {
            parseError(source, lineNumber, "invalid signed decimal numeric field");
        }
        const std::optional<std::string_view> value = parseValueField(line, cursor, profile);
        if ( !value )
        {
            parseError(source, lineNumber, "message text must be the final quoted field");
        }

        StringTableEntry entry{
            std::string(*key),
            *number,
            std::string(*value) };
        try
        {
            validateEntry(entry, profile, false);
        }
        catch ( const std::runtime_error& exception )
        {
            parseError(source, lineNumber, exception.what());
        }

        const std::string identity = duplicateIdentity(entry.key);
        if ( !keys.insert(identity).second )
        {
            parseError(source, lineNumber, "duplicate message key " + entry.key);
        }
        if ( result.entries.size() >= result.capacity )
        {
            parseError(source, lineNumber, "message records exceed declared MSGS capacity");
        }
        result.entries.push_back(std::move(entry));
    }

    if ( !sawHeader )
    {
        parseError(source, lineNumber, "missing MSGS capacity header");
    }
    if ( !sawEnd )
    {
        parseError(source, lineNumber, "missing END after message list");
    }
    while ( readBoundedLine(input, line, lineNumber, source) )
    {
        line = stripComments(std::move(line), profile);
        if ( !line.empty() )
        {
            parseError(source, lineNumber, "unexpected content after END");
        }
    }
    return result;
}

StringTableFile loadFromStringTableUni(
    const std::filesystem::path& path,
    const UniProfile profile)
{
    std::ifstream input(path, std::ios::binary);
    if ( !input )
    {
        throw std::runtime_error("Failed to open string table: " + path.string());
    }
    return loadFromStringTableUni(input, profile, path.string());
}

std::string saveToStringTableUniText(
    const StringTableFile& strings,
    const UniProfile profile)
{
    if ( !isKnownProfile(profile) )
    {
        throw std::runtime_error("Unknown UNI profile.");
    }
    if ( strings.entries.size() > kMaxStringTableEntries )
    {
        writerError("table exceeds the 4096-row editor limit");
    }
    if ( strings.capacity > kMaxStringTableEntries )
    {
        writerError("MSGS capacity exceeds the 4096-row editor limit");
    }
    std::unordered_set<std::string> keys{};
    keys.reserve(strings.entries.size());
    for ( const StringTableEntry& entry : strings.entries )
    {
        validateEntry(entry, profile, true);
        if ( recordLength(entry) > maxRecordBytes(profile) )
        {
            writerError("message record exceeds the engine line limit");
        }
        if ( !keys.insert(duplicateIdentity(entry.key)).second )
        {
            writerError("duplicate message key " + entry.key);
        }
    }

    std::string text{};
    text.reserve(strings.entries.size() * 64U + 32U);
    text += "MSGS ";
    // Both runtimes expect the declared number of records before END. Keep
    // imported capacity in memory, but emit the exact live external count.
    text += std::to_string(strings.entries.size());
    text += "\n\n";
    for ( const StringTableEntry& entry : strings.entries )
    {
        text.push_back('"');
        text += entry.key;
        text += "\" ";
        text += std::to_string(entry.numericValue);
        text += " \"";
        text += entry.value;
        text += "\"\n";
    }
    text += "\nEND\n";
    return text;
}

} // namespace sed::assets
