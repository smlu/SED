#include "sed/assets/gob.hpp"

#include <array>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace sed::assets
{

namespace
{

std::uint32_t readU32LE(std::istream& in)
{
    std::array<unsigned char, 4> bytes{};
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!in)
    {
        throw std::runtime_error("Unexpected EOF while reading uint32");
    }
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8)
        | (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

void validateEntryBounds(
    const GobEntry& entry,
    const std::uint64_t archiveSize,
    const std::filesystem::path& path)
{
    const std::uint64_t offset = entry.offset;
    const std::uint64_t size = entry.size;
    if (offset > archiveSize || size > archiveSize - offset)
    {
        throw std::runtime_error(
            "GOB entry extends past end of file: " + entry.name + " in " + path.string());
    }
}

} // namespace

GobArchive GobArchive::open(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open GOB: " + path.string());
    }

    file.seekg(0, std::ios::end);
    const std::streampos endPosition = file.tellg();
    if (endPosition == std::streampos(-1))
    {
        throw std::runtime_error("Failed to query GOB size: " + path.string());
    }
    const std::uint64_t archiveSize = static_cast<std::uint64_t>(
        static_cast<std::streamoff>(endPosition));
    file.seekg(0, std::ios::beg);
    if (!file)
    {
        throw std::runtime_error("Failed to seek GOB header: " + path.string());
    }

    std::array<char, 4> magic{};
    file.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!file)
    {
        throw std::runtime_error("Failed to read GOB header: " + path.string());
    }

    GobArchive archive{};
    archive.path_ = path;

    if (magic[0] == 'G' && magic[1] == 'O' && magic[2] == 'B' && magic[3] == '\n')
    {
        const std::uint32_t indexOffset = readU32LE(file);
        constexpr std::uint64_t kEntrySize = 21u;
        if ( indexOffset > archiveSize || archiveSize - indexOffset < 4u )
        {
            throw std::runtime_error("Invalid GOB v1 index offset: " + path.string());
        }
        file.seekg(static_cast<std::streamoff>(indexOffset), std::ios::beg);
        if (!file)
        {
            throw std::runtime_error("Invalid GOB v1 index offset: " + path.string());
        }

        const std::uint32_t numEntries = readU32LE(file);
        if ( numEntries > (archiveSize - indexOffset - 4u) / kEntrySize )
        {
            throw std::runtime_error(
                "Unexpected EOF: GOB v1 index extends past end of file: "
                    + path.string());
        }
        archive.entries_.reserve(numEntries);

        for (std::uint32_t i = 0; i < numEntries; ++i)
        {
            GobEntry entry{};
            entry.offset = readU32LE(file);
            entry.size = readU32LE(file);

            std::array<char, 13> nameBytes{};
            file.read(nameBytes.data(), static_cast<std::streamsize>(nameBytes.size()));
            if (!file)
            {
                throw std::runtime_error("Unexpected EOF while reading GOB v1 entry name: " + path.string());
            }

            entry.name.assign(nameBytes.data(), nameBytes.size());
            const std::size_t nul = entry.name.find('\0');
            if (nul != std::string::npos)
            {
                entry.name.resize(nul);
            }

            validateEntryBounds(entry, archiveSize, path);

            const std::string normalized = normalizeEntryName(entry.name);
            if (!normalized.empty() && !archive.index_.contains(normalized))
            {
                archive.index_.emplace(normalized, archive.entries_.size());
            }
            archive.entries_.push_back(std::move(entry));
        }
    }
    else if (magic[0] == 'G' && magic[1] == 'O' && magic[2] == 'B' && magic[3] == ' ')
    {
        (void)readU32LE(file); // long1
        (void)readU32LE(file); // long2
        const std::uint32_t numEntries = readU32LE(file);
        constexpr std::uint64_t kHeaderSize = 16u;
        constexpr std::uint64_t kEntrySize = 136u;
        if ( archiveSize < kHeaderSize
            || numEntries > (archiveSize - kHeaderSize) / kEntrySize )
        {
            throw std::runtime_error(
                "Unexpected EOF: GOB v2 index extends past end of file: "
                    + path.string());
        }
        archive.entries_.reserve(numEntries);

        for (std::uint32_t i = 0; i < numEntries; ++i)
        {
            GobEntry entry{};
            entry.offset = readU32LE(file);
            entry.size = readU32LE(file);

            std::array<char, 128> nameBytes{};
            file.read(nameBytes.data(), static_cast<std::streamsize>(nameBytes.size()));
            if (!file)
            {
                throw std::runtime_error("Unexpected EOF while reading GOB v2 entry name: " + path.string());
            }

            entry.name.assign(nameBytes.data(), nameBytes.size());
            const std::size_t nul = entry.name.find('\0');
            if (nul != std::string::npos)
            {
                entry.name.resize(nul);
            }

            validateEntryBounds(entry, archiveSize, path);

            const std::string normalized = normalizeEntryName(entry.name);
            if (!normalized.empty() && !archive.index_.contains(normalized))
            {
                archive.index_.emplace(normalized, archive.entries_.size());
            }
            archive.entries_.push_back(std::move(entry));
        }
    }
    else
    {
        throw std::runtime_error("Not a GOB file: " + path.string());
    }

    return archive;
}

const std::filesystem::path& GobArchive::path() const noexcept
{
    return path_;
}

const std::vector<GobEntry>& GobArchive::entries() const noexcept
{
    return entries_;
}

std::optional<GobEntry> GobArchive::find(const std::string_view name) const
{
    const std::string key = normalizeEntryName(name);
    if (key.empty())
    {
        return std::nullopt;
    }

    const auto it = index_.find(key);
    if (it == index_.end())
    {
        return std::nullopt;
    }

    return entries_[it->second];
}

bool GobArchive::contains(const std::string_view name) const
{
    return find(name).has_value();
}

std::vector<std::byte> GobArchive::read(const std::string_view name) const
{
    const auto entryOpt = find(name);
    if (!entryOpt)
    {
        throw std::runtime_error("GOB entry not found: " + std::string(name));
    }

    const GobEntry& entry = *entryOpt;
    std::ifstream file(path_, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open GOB: " + path_.string());
    }

    file.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    if (!file)
    {
        throw std::runtime_error("Invalid GOB entry offset for: " + entry.name);
    }

    std::vector<std::byte> data;
    data.resize(entry.size);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file)
    {
        throw std::runtime_error("Unexpected EOF while reading GOB entry: " + entry.name);
    }
    return data;
}

std::string GobArchive::normalizeEntryName(const std::string_view name)
{
    std::string out;
    out.reserve(name.size());

    for (const char ch : name)
    {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (ch == '\\' || ch == '/')
        {
            out.push_back('/');
            continue;
        }
        out.push_back(static_cast<char>(std::tolower(uch)));
    }

    while (!out.empty() && out.front() == '/')
    {
        out.erase(out.begin());
    }

    return out;
}

} // namespace sed::assets
