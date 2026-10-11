#pragma once

#include <cstdint>
#include <filesystem>
#include <istream>
#include <string>
#include <string_view>
#include <vector>

namespace sed::assets
{

/** Selects the engine grammar; both profiles use exact game-byte key identity. */
enum class UniProfile : std::uint8_t
{
    classic,
    ijim,
};

/** One ordered game-byte string-table record. */
struct StringTableEntry
{
    std::string key{};
    std::int32_t numericValue{};
    std::string value{};
};

/** A UNI table retaining its declared MSGS capacity and authored row order. */
struct StringTableFile
{
    std::uint32_t capacity{};
    std::vector<StringTableEntry> entries{};
};

/** Loads a UNI table from a stream and includes sourceName in parse diagnostics. */
StringTableFile loadFromStringTableUni(
    std::istream& input,
    UniProfile profile,
    std::string_view sourceName);

/** Loads a UNI table from a path and includes that path in diagnostics. */
StringTableFile loadFromStringTableUni(
    const std::filesystem::path& path,
    UniProfile profile);

/**
 * Serializes a bounded UNI table without changing row order or key spelling.
 * Both profiles declare the exact live row count; source capacity remains in the
 * in-memory table for editing and persistence.
 */
std::string saveToStringTableUniText(
    const StringTableFile& strings,
    UniProfile profile);

} // namespace sed::assets
