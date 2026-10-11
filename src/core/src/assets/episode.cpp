#include "sed/assets/episode.hpp"

#include "sed/text.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace sed::assets
{
namespace
{

constexpr std::size_t kEpisodeNameLineMaxBytes = 62;
constexpr std::size_t kEpisodeSequenceLineMaxBytes = 126;
constexpr std::size_t kEpisodeFileNameMaxBytes = 31;
constexpr std::size_t kCogStringLineMaxBytes = 253;

std::string trimCopy(const std::string_view value)
{
    const std::string_view trimmed = sed::text::trimView(value);
    return std::string(trimmed.begin(), trimmed.end());
}

std::string stripCommentOutsideQuotes(std::string line)
{
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        const char c = line[i];
        if (c == '"')
        {
            quoted = !quoted;
            continue;
        }
        if (c == '#' && !quoted)
        {
            line.resize(i);
            break;
        }
    }
    return trimCopy(line);
}

bool readMeaningfulLine(std::istream& in, std::string& line, std::size_t& lineNumber)
{
    while (std::getline(in, line))
    {
        ++lineNumber;
        if (line.find('\0') != std::string::npos
            || line.find('\x1a') != std::string::npos)
        {
            throw std::runtime_error(
                "campaign parse error at line " + std::to_string(lineNumber)
                + ": embedded NUL/DOS EOF is not supported by JK/MotS");
        }
        line = stripCommentOutsideQuotes(std::move(line));
        if (!line.empty())
        {
            return true;
        }
    }
    return false;
}

[[noreturn]] void parseError(const std::string_view format, const std::size_t lineNumber, const std::string_view detail)
{
    throw std::runtime_error(
        std::string(format) + " parse error at line " + std::to_string(lineNumber)
        + (detail.empty() ? std::string{} : ": " + std::string(detail)));
}

std::optional<std::string> parseQuoted(std::string_view line, std::size_t& cursor)
{
    while (cursor < line.size() && std::isspace(static_cast<unsigned char>(line[cursor])) != 0)
    {
        ++cursor;
    }
    if (cursor >= line.size() || line[cursor] != '"')
    {
        return std::nullopt;
    }

    ++cursor;
    std::string value{};
    for (; cursor < line.size(); ++cursor)
    {
        const char c = line[cursor];
        if (c == '"')
        {
            ++cursor;
            return value;
        }
        value.push_back(c);
    }
    return std::nullopt;
}

void requireSingleLineQuotedText(const std::string_view value, const std::string_view field)
{
    if (value.find('"') != std::string_view::npos)
    {
        throw std::runtime_error(std::string(field) + " cannot contain a double quote");
    }
    if (value.find('\0') != std::string_view::npos
        || value.find('\x1a') != std::string_view::npos)
    {
        throw std::runtime_error(std::string(field) + " cannot contain an embedded NUL/DOS EOF");
    }
    if (value.find('\r') != std::string_view::npos
        || value.find('\n') != std::string_view::npos)
    {
        throw std::runtime_error(std::string(field) + " must fit on one line");
    }
}

void requireToken(const std::string_view value, const std::string_view field)
{
    if (value.empty()
        || value.find('#') != std::string_view::npos
        || value.find('\0') != std::string_view::npos
        || value.find('\x1a') != std::string_view::npos
        || std::any_of(value.begin(), value.end(), [](const char c)
            {
                return std::isspace(static_cast<unsigned char>(c)) != 0;
            }))
    {
        throw std::runtime_error(std::string(field) + " must be one non-empty token");
    }
}

bool hasNonWhitespaceRemainder(const std::string_view line, std::size_t cursor) noexcept
{
    while (cursor < line.size()
        && std::isspace(static_cast<unsigned char>(line[cursor])) != 0)
    {
        ++cursor;
    }
    return cursor < line.size();
}

} // namespace

std::optional<std::string> episodeGraphValidationError(const EpisodeFile& episode)
{
    if (episode.sequences.empty())
    {
        return std::string("Episode must contain at least one sequence");
    }

    std::unordered_set<int> lineNumbers{};
    lineNumbers.reserve(episode.sequences.size());
    for (const EpisodeSequence& sequence : episode.sequences)
    {
        if (sequence.lineNumber < 0)
        {
            return "Episode line identifiers must be non-negative; -1 is reserved for terminal branches";
        }
        if (sequence.fileName.size() > kEpisodeFileNameMaxBytes)
        {
            return "Episode line " + std::to_string(sequence.lineNumber)
                + " file name exceeds the runtime 31-byte limit";
        }
        if (sequence.type.size() > 63)
        {
            return "Episode line " + std::to_string(sequence.lineNumber)
                + " type exceeds the runtime 63-byte field limit";
        }
        if (!sed::text::iequalsAscii(sequence.type, "LEVEL")
            && !sed::text::iequalsAscii(sequence.type, "CUT")
            && !sed::text::iequalsAscii(sequence.type, "DECIDE"))
        {
            return "Episode line " + std::to_string(sequence.lineNumber)
                + " has unknown type " + sequence.type
                + "; expected LEVEL, CUT, or DECIDE";
        }
        if (!lineNumbers.insert(sequence.lineNumber).second)
        {
            return "Duplicate episode line number " + std::to_string(sequence.lineNumber);
        }
    }

    for (const EpisodeSequence& sequence : episode.sequences)
    {
        const auto danglingBranchError = [&lineNumbers, &sequence](
                                             const std::string_view label,
                                             const int target) -> std::optional<std::string>
        {
            if (target != -1 && !lineNumbers.contains(target))
            {
                return "Episode line " + std::to_string(sequence.lineNumber)
                    + " has dangling " + std::string(label)
                    + " target " + std::to_string(target);
            }
            return std::nullopt;
        };
        if (const std::optional<std::string> error =
                danglingBranchError("gotoA", sequence.gotoA))
        {
            return error;
        }
        if (const std::optional<std::string> error =
                danglingBranchError("gotoB", sequence.gotoB))
        {
            return error;
        }
    }
    return std::nullopt;
}

const std::string* CogStringsFile::find(const std::string_view key) const noexcept
{
    const auto found = std::find_if(entries.begin(), entries.end(), [key](const CogStringEntry& entry)
        {
            return sed::text::iequalsAscii(entry.key, key);
        });
    return found == entries.end() ? nullptr : &found->value;
}

void CogStringsFile::set(std::string key, std::string value)
{
    key = sed::text::upperAscii(sed::text::trimView(key));
    if (key.empty())
    {
        throw std::invalid_argument("COG string key cannot be empty");
    }

    const auto found = std::find_if(entries.begin(), entries.end(), [&key](const CogStringEntry& entry)
        {
            return sed::text::iequalsAscii(entry.key, key);
        });
    if (found != entries.end())
    {
        found->key = std::move(key);
        found->value = std::move(value);
        return;
    }
    entries.push_back(CogStringEntry{ std::move(key), std::move(value), 0 });
}

void CogStringsFile::set(std::string key, std::string value, const int unusedNumber)
{
    key = sed::text::upperAscii(sed::text::trimView(key));
    if (key.empty())
    {
        throw std::invalid_argument("COG string key cannot be empty");
    }

    const auto found = std::find_if(entries.begin(), entries.end(), [&key](const CogStringEntry& entry)
        {
            return sed::text::iequalsAscii(entry.key, key);
        });
    if (found != entries.end())
    {
        found->key = std::move(key);
        found->value = std::move(value);
        found->unusedNumber = unusedNumber;
        return;
    }
    entries.push_back(CogStringEntry{ std::move(key), std::move(value), unusedNumber });
}

bool CogStringsFile::erase(const std::string_view key) noexcept
{
    const auto found = std::find_if(entries.begin(), entries.end(), [key](const CogStringEntry& entry)
        {
            return sed::text::iequalsAscii(entry.key, key);
        });
    if (found == entries.end())
    {
        return false;
    }
    entries.erase(found);
    return true;
}

std::vector<std::string> conflictingCogStringKeys(
    const CogStringsFile& first,
    const CogStringsFile& second)
{
    std::vector<std::string> conflicts{};
    for (const CogStringEntry& entry : first.entries)
    {
        const auto other = std::find_if(second.entries.begin(), second.entries.end(), [&entry](const CogStringEntry& candidate)
            {
                return sed::text::iequalsAscii(candidate.key, entry.key);
            });
        if (other != second.entries.end()
            && (other->value != entry.value
                || other->unusedNumber != entry.unusedNumber))
        {
            conflicts.push_back(sed::text::upperAscii(sed::text::trimView(entry.key)));
        }
    }
    std::sort(conflicts.begin(), conflicts.end());
    conflicts.erase(std::unique(conflicts.begin(), conflicts.end()), conflicts.end());
    return conflicts;
}

CogStringsFile mergeCogStrings(
    const CogStringsFile& preferred,
    const CogStringsFile& fallback)
{
    CogStringsFile merged = preferred;
    for (const CogStringEntry& entry : fallback.entries)
    {
        if (!merged.find(entry.key))
        {
            merged.set(entry.key, entry.value, entry.unusedNumber);
        }
    }
    return merged;
}

EpisodeFile loadFromEpisodeJk(std::istream& in)
{
    EpisodeFile episode{};
    std::string line{};
    std::size_t lineNumber = 0;

    if (!readMeaningfulLine(in, line, lineNumber))
    {
        parseError("episode.jk", lineNumber, "missing episode name");
    }
    if (line.size() > kEpisodeNameLineMaxBytes)
    {
        parseError("episode.jk", lineNumber, "episode name exceeds the runtime line limit");
    }
    std::size_t cursor = 0;
    const std::optional<std::string> name = parseQuoted(line, cursor);
    if (!name || hasNonWhitespaceRemainder(line, cursor))
    {
        parseError("episode.jk", lineNumber, "episode name must be one quoted field");
    }
    episode.name = *name;

    if (!readMeaningfulLine(in, line, lineNumber))
    {
        parseError("episode.jk", lineNumber, "missing TYPE record");
    }
    {
        std::istringstream record(line);
        std::string keyword{};
        std::string trailing{};
        if (!(record >> keyword >> episode.type)
            || !sed::text::iequalsAscii(keyword, "TYPE")
            || (record >> trailing))
        {
            parseError("episode.jk", lineNumber, "invalid TYPE record");
        }
    }

    int sequenceCount = 0;
    if (!readMeaningfulLine(in, line, lineNumber))
    {
        parseError("episode.jk", lineNumber, "missing SEQ record");
    }
    {
        std::istringstream record(line);
        std::string keyword{};
        std::string trailing{};
        if (!(record >> keyword >> sequenceCount)
            || !sed::text::iequalsAscii(keyword, "SEQ")
            || sequenceCount < 0
            || (record >> trailing))
        {
            parseError("episode.jk", lineNumber, "invalid SEQ record");
        }
    }

    episode.sequences.reserve(static_cast<std::size_t>(sequenceCount));
    for (int index = 0; index < sequenceCount; ++index)
    {
        if (!readMeaningfulLine(in, line, lineNumber))
        {
            parseError("episode.jk", lineNumber, "sequence count exceeds available records");
        }
        if (sed::text::iequalsAscii(line, "END"))
        {
            parseError("episode.jk", lineNumber, "unexpected END in sequence list");
        }
        if (line.size() > kEpisodeSequenceLineMaxBytes)
        {
            parseError("episode.jk", lineNumber, "sequence record exceeds the runtime line limit");
        }

        EpisodeSequence sequence{};
        std::istringstream record(line);
        char colon = '\0';
        std::string trailing{};
        if (!(record >> sequence.lineNumber)
            || !record.get(colon)
            || colon != ':'
            || !(record
                >> sequence.cdNumber
                >> sequence.levelNumber
                >> sequence.type
                >> sequence.fileName
                >> sequence.lightPower
                >> sequence.darkPower
                >> sequence.gotoA
                >> sequence.gotoB)
            || (record >> trailing))
        {
            parseError("episode.jk", lineNumber, "invalid sequence record");
        }
        episode.sequences.push_back(std::move(sequence));
    }

    if (!readMeaningfulLine(in, line, lineNumber)
        || !sed::text::iequalsAscii(line, "END"))
    {
        parseError("episode.jk", lineNumber, "missing END after sequence list");
    }
    if (readMeaningfulLine(in, line, lineNumber))
    {
        parseError("episode.jk", lineNumber, "unexpected content after END");
    }
    if (const std::optional<std::string> graphError = episodeGraphValidationError(episode))
    {
        parseError("episode.jk", lineNumber, *graphError);
    }
    return episode;
}

EpisodeFile loadFromEpisodeJk(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Failed to open episode.jk: " + path.string());
    }
    return loadFromEpisodeJk(in);
}

std::string saveToEpisodeJkText(const EpisodeFile& episode)
{
    const std::string& name = episode.name;
    if (name.empty())
    {
        throw std::runtime_error("Episode name cannot be empty");
    }
    requireSingleLineQuotedText(name, "Episode name");
    if (name.size() + 2 > kEpisodeNameLineMaxBytes)
    {
        throw std::runtime_error("Episode name exceeds the runtime 60-byte limit");
    }
    if (const std::optional<std::string> graphError = episodeGraphValidationError(episode))
    {
        throw std::runtime_error(*graphError);
    }

    std::ostringstream out{};
    out << '"' << name << "\"\n\n";
    out << "TYPE " << episode.type << "\n\n";
    out << "SEQ " << episode.sequences.size() << "\n\n";
    out << "# <line> <cd>  <level>  <type>   <file>         <lightpow>  <darkpow>   <gotoA>  <gotoB>\n\n";
    for (const EpisodeSequence& sequence : episode.sequences)
    {
        requireToken(sequence.type, "Sequence type");
        requireToken(sequence.fileName, "Sequence file name");
        std::ostringstream record{};
        record << sequence.lineNumber << ":\t"
            << sequence.cdNumber << '\t'
            << sequence.levelNumber << '\t'
            << sequence.type << '\t'
            << sequence.fileName << '\t'
            << sequence.lightPower << '\t'
            << sequence.darkPower << '\t'
            << sequence.gotoA << '\t'
            << sequence.gotoB;
        const std::string line = record.str();
        if (line.size() > kEpisodeSequenceLineMaxBytes)
        {
            throw std::runtime_error(
                "Episode line " + std::to_string(sequence.lineNumber)
                + " exceeds the runtime 126-byte record limit");
        }
        out << line << '\n';
    }
    out << "\nend\n";
    return out.str();
}

void saveToEpisodeJk(const EpisodeFile& episode, const std::filesystem::path& path)
{
    const std::string text = saveToEpisodeJkText(episode);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        throw std::runtime_error("Failed to create episode.jk: " + path.string());
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out)
    {
        throw std::runtime_error("Failed to write episode.jk: " + path.string());
    }
}

CogStringsFile loadFromCogStringsUni(std::istream& in)
{
    CogStringsFile result{};
    std::string line{};
    std::size_t lineNumber = 0;
    int declaredCount = -1;

    if (!readMeaningfulLine(in, line, lineNumber))
    {
        parseError("cogstrings.uni", lineNumber, "missing MSGS header");
    }
    {
        std::istringstream header(line);
        std::string keyword{};
        std::string trailing{};
        if (!(header >> keyword >> declaredCount)
            || !sed::text::iequalsAscii(keyword, "MSGS")
            || declaredCount < 0
            || (header >> trailing))
        {
            parseError("cogstrings.uni", lineNumber, "invalid MSGS header");
        }
    }

    int parsedCount = 0;
    bool sawEnd = false;
    while (readMeaningfulLine(in, line, lineNumber))
    {
        if (sed::text::iequalsAscii(line, "END"))
        {
            sawEnd = true;
            break;
        }
        if (line.size() > kCogStringLineMaxBytes)
        {
            parseError("cogstrings.uni", lineNumber, "message record exceeds the runtime line limit");
        }

        std::size_t keyCursor = 0;
        const std::optional<std::string> key = parseQuoted(line, keyCursor);
        if (!key)
        {
            parseError("cogstrings.uni", lineNumber, "message key must be quoted");
        }
        if (key->empty())
        {
            parseError("cogstrings.uni", lineNumber, "message key cannot be empty");
        }
        if (trimCopy(*key) != *key)
        {
            parseError("cogstrings.uni", lineNumber, "message key cannot have leading or trailing whitespace");
        }

        while (keyCursor < line.size() && std::isspace(static_cast<unsigned char>(line[keyCursor])) != 0)
        {
            ++keyCursor;
        }
        int unusedNumber = 0;
        const char* unusedFirst = line.data() + keyCursor;
        const char* unusedLast = line.data() + line.size();
        const auto unusedResult = std::from_chars(unusedFirst, unusedLast, unusedNumber);
        if (unusedResult.ec != std::errc{} || unusedResult.ptr == unusedFirst)
        {
            parseError("cogstrings.uni", lineNumber, "invalid unused numeric field");
        }
        keyCursor = static_cast<std::size_t>(unusedResult.ptr - line.data());

        const std::optional<std::string> value = parseQuoted(line, keyCursor);
        if (!value || hasNonWhitespaceRemainder(line, keyCursor))
        {
            parseError("cogstrings.uni", lineNumber, "message text must be the final quoted field");
        }
        if (result.find(*key))
        {
            parseError("cogstrings.uni", lineNumber, "duplicate message key " + *key);
        }
        result.set(*key, *value, unusedNumber);
        ++parsedCount;
    }

    if (!sawEnd)
    {
        parseError("cogstrings.uni", lineNumber, "missing END after message list");
    }
    // Retail MotS ships `MSGS 1000` tables with fewer live records, so an
    // overdeclared count is a valid capacity. An underdeclared count is not:
    // the game stops after MSGS rows and then expects END.
    if (parsedCount > declaredCount)
    {
        parseError("cogstrings.uni", lineNumber,
            "message records exceed the declared MSGS capacity");
    }
    if (readMeaningfulLine(in, line, lineNumber))
    {
        parseError("cogstrings.uni", lineNumber, "unexpected content after END");
    }
    return result;
}

CogStringsFile loadFromCogStringsUni(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Failed to open cogstrings.uni: " + path.string());
    }
    return loadFromCogStringsUni(in);
}

std::string saveToCogStringsUniText(const CogStringsFile& strings)
{
    std::vector<CogStringEntry> entries{};
    entries.reserve(strings.entries.size());
    std::unordered_set<std::string> keys{};
    keys.reserve(strings.entries.size());
    for (const CogStringEntry& entry : strings.entries)
    {
        if (entry.key.empty() || trimCopy(entry.key) != entry.key)
        {
            throw std::runtime_error(
                "COG string key must be non-empty without leading or trailing whitespace");
        }
        const std::string key = sed::text::upperAscii(entry.key);
        requireSingleLineQuotedText(key, "COG string key");
        const std::string& value = entry.value;
        requireSingleLineQuotedText(value, "COG string text");

        if (!keys.insert(key).second)
        {
            throw std::runtime_error("Duplicate COG string key " + key);
        }
        const std::size_t serializedLength = key.size() + value.size()
            + std::to_string(entry.unusedNumber).size() + 6;
        if (serializedLength > kCogStringLineMaxBytes)
        {
            throw std::runtime_error(
                "COG string " + key + " exceeds the runtime 253-byte record limit");
        }
        entries.push_back(CogStringEntry{ key, value, entry.unusedNumber });
    }

    std::sort(entries.begin(), entries.end(), [](const CogStringEntry& lhs, const CogStringEntry& rhs)
        {
            return sed::text::upperAscii(lhs.key) < sed::text::upperAscii(rhs.key);
        });

    std::ostringstream out{};
    out << "MSGS " << entries.size() << "\n\n";
    out << "#  \"<key>\"     <unused number>   \"<string>\"\n\n";
    for (const CogStringEntry& entry : entries)
    {
        out << '"' << entry.key << "\" " << entry.unusedNumber << " \"" << entry.value << "\"\n";
    }
    out << "\nEND\n";
    return out.str();
}

void saveToCogStringsUni(const CogStringsFile& strings, const std::filesystem::path& path)
{
    const std::string text = saveToCogStringsUniText(strings);
    std::error_code ec{};
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    if (ec)
    {
        throw std::runtime_error("Failed to create cogstrings.uni directory: " + ec.message());
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        throw std::runtime_error("Failed to create cogstrings.uni: " + path.string());
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out)
    {
        throw std::runtime_error("Failed to write cogstrings.uni: " + path.string());
    }
}

} // namespace sed::assets
