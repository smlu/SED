#pragma once

#include <filesystem>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sed::assets
{

/** One canonical `episode.jk` sequence row. */
struct EpisodeSequence
{
    int lineNumber{ 10 };
    int cdNumber{};
    int levelNumber{ 1 };
    std::string type{ "LEVEL" };
    std::string fileName{};
    int lightPower{};
    int darkPower{};
    int gotoA{ -1 };
    int gotoB{ -1 };
};

struct EpisodeFile
{
    std::string name{ "New Episode" };
    int type{ 1 };
    std::vector<EpisodeSequence> sequences{};
};

/** Returns a user-facing error for an invalid sequence graph. */
std::optional<std::string> episodeGraphValidationError(const EpisodeFile& episode);

struct CogStringEntry
{
    std::string key{};
    std::string value{};
    int unusedNumber{};
};

/** Editable `cogstrings.uni` contents with case-insensitive keys. */
struct CogStringsFile
{
    std::vector<CogStringEntry> entries{};

    const std::string* find(std::string_view key) const noexcept;
    void set(std::string key, std::string value);
    void set(std::string key, std::string value, int unusedNumber);
    bool erase(std::string_view key) noexcept;
};

/** Case-insensitive keys whose values differ between two string tables. */
std::vector<std::string> conflictingCogStringKeys(
    const CogStringsFile& first,
    const CogStringsFile& second);

/** Union of two tables, with `preferred` values winning key conflicts. */
CogStringsFile mergeCogStrings(
    const CogStringsFile& preferred,
    const CogStringsFile& fallback);

EpisodeFile loadFromEpisodeJk(std::istream& in);
EpisodeFile loadFromEpisodeJk(const std::filesystem::path& path);
std::string saveToEpisodeJkText(const EpisodeFile& episode);
void saveToEpisodeJk(const EpisodeFile& episode, const std::filesystem::path& path);

CogStringsFile loadFromCogStringsUni(std::istream& in);
CogStringsFile loadFromCogStringsUni(const std::filesystem::path& path);
std::string saveToCogStringsUniText(const CogStringsFile& strings);
void saveToCogStringsUni(const CogStringsFile& strings, const std::filesystem::path& path);

} // namespace sed::assets
