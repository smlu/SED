#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sed::assets
{

struct GobEntry
{
    std::string name;
    std::uint32_t offset{};
    std::uint32_t size{};
};

class GobArchive
{
public:
    static GobArchive open(const std::filesystem::path& path);

    const std::filesystem::path& path() const noexcept;

    const std::vector<GobEntry>& entries() const noexcept;
    std::optional<GobEntry> find(const std::string_view name) const;
    bool contains(const std::string_view name) const;

    std::vector<std::byte> read(const std::string_view name) const;

private:
    static std::string normalizeEntryName(const std::string_view name);

    std::filesystem::path path_;
    std::vector<GobEntry> entries_;
    std::unordered_map<std::string, std::size_t> index_;
};

} // namespace sed::assets
