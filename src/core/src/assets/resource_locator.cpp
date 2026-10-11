#include "sed/assets/resource_locator.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <exception>
#include <stdexcept>
#include <utility>

#include "sed/assets/bounded_stream_reader.hpp"
#include "sed/text.hpp"

namespace sed::assets
{

namespace
{

std::uint64_t nextPreviewCacheIdentity() noexcept
{
    static std::atomic<std::uint64_t> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}

std::vector<std::byte> readFileBinary(const std::filesystem::path& path)
{
    std::error_code ec;
    const std::uintmax_t fileSize = std::filesystem::file_size(path, ec);
    if (ec)
    {
        throw std::runtime_error("Failed to query file size: " + path.string());
    }

    if (fileSize > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
    {
        throw std::runtime_error("File too large: " + path.string());
    }

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    std::vector<std::byte> data;
    data.resize(static_cast<std::size_t>(fileSize));

    if (!data.empty())
    {
        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!in)
        {
            throw std::runtime_error("Failed to read file: " + path.string());
        }
    }

    return data;
}

std::optional<std::vector<std::byte>> tryReadFileBinaryWithinBudget(
    const std::filesystem::path& path,
    const std::size_t maxBytes,
    bool& exceededBudget)
{
    std::error_code ec;
    const std::uintmax_t fileSize = std::filesystem::file_size(path, ec);
    if ( ec )
    {
        throw std::runtime_error("Failed to query file size: " + path.string());
    }

    std::ifstream in(path, std::ios::binary);
    if ( !in )
    {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    const std::string failureMessage = "Failed to read file: " + path.string();
    const std::size_t sizeHint = fileSize > static_cast<std::uintmax_t>(maxBytes)
        ? maxBytes
        : static_cast<std::size_t>(fileSize);
    return detail::readBinaryStreamWithinBudget<std::vector<std::byte>>(
        in,
        maxBytes,
        sizeHint,
        exceededBudget,
        failureMessage);
}

} // namespace

struct ResourceLocator::Configuration
{
    std::vector<std::filesystem::path> roots;
    std::vector<GobArchive> gobs;
};

struct ResourceLocator::PrimaryResourceOverride
{
    std::vector<std::string> aliases;
    PrimaryResourceReader reader;
    std::uint64_t revision{};
    std::once_flag materialized;
    std::optional<std::vector<std::byte>> bytes;
    std::exception_ptr error;
};

ResourceLocator::ResourceLocator(ResourceLocator&& source) noexcept
{
    configuration_.swap(source.configuration_);
    primaryOverride_.swap(source.primaryOverride_);
    previewCacheIdentity_ = std::exchange(source.previewCacheIdentity_, 0);
}

ResourceLocator& ResourceLocator::operator=(ResourceLocator replacement) noexcept
{
    configuration_.swap(replacement.configuration_);
    primaryOverride_.swap(replacement.primaryOverride_);
    std::swap(previewCacheIdentity_, replacement.previewCacheIdentity_);
    return *this;
}

std::shared_ptr<ResourceLocator::Configuration> ResourceLocator::writableConfiguration() const
{
    if (!configuration_) return std::make_shared<Configuration>();
    if (configuration_.use_count() == 1) return configuration_;
    return std::make_shared<Configuration>(*configuration_);
}

std::uint64_t ResourceLocator::previewCacheIdentity() const noexcept
{
    return previewCacheIdentity_;
}

void ResourceLocator::setPrimaryResourceOverride(const std::string_view name, PrimaryResourceReader reader)
{
    if (normalizeRelative(name).empty() || !reader
        || std::filesystem::path(normalizeRelative(name)).is_absolute())
        throw std::invalid_argument("Primary resource override requires a relative name and reader.");
    auto replacement = std::make_shared<PrimaryResourceOverride>();
    replacement->aliases = candidates(name);
    for (auto& alias : replacement->aliases) alias = sed::text::upperAscii(alias);
    replacement->reader = std::move(reader);
    replacement->revision = nextPreviewCacheIdentity();
    primaryOverride_.swap(replacement);
    previewCacheIdentity_ = primaryOverride_->revision;
}

void ResourceLocator::clearPrimaryResourceOverride() noexcept
{
    if (!primaryOverride_) return;
    primaryOverride_.reset();
    previewCacheIdentity_ = nextPreviewCacheIdentity();
}

bool ResourceLocator::matchesPrimaryResourceOverride(const std::string_view name) const
{
    if (!primaryOverride_ || std::filesystem::path(normalizeRelative(name)).is_absolute()) return false;
    for (const auto& candidate : candidates(name))
    {
        const auto normalized = sed::text::upperAscii(candidate);
        if (std::find(primaryOverride_->aliases.begin(), primaryOverride_->aliases.end(), normalized)
            != primaryOverride_->aliases.end()) return true;
    }
    return false;
}

std::optional<std::uint64_t> ResourceLocator::primaryResourceOverrideRevision(const std::string_view name) const
{
    return matchesPrimaryResourceOverride(name)
        ? std::optional<std::uint64_t>(primaryOverride_->revision) : std::nullopt;
}

void ResourceLocator::clearRoots()
{
    if (roots().empty()) return;
    auto replacement = writableConfiguration();
    replacement->roots.clear();
    configuration_.swap(replacement);
    previewCacheIdentity_ = nextPreviewCacheIdentity();
}

void ResourceLocator::addRoot(const std::filesystem::path& root)
{
    if (root.empty()) return;
    auto replacement = writableConfiguration();
    replacement->roots.push_back(root);
    configuration_.swap(replacement);
    previewCacheIdentity_ = nextPreviewCacheIdentity();
}

const std::vector<std::filesystem::path>& ResourceLocator::roots() const noexcept
{
    static const std::vector<std::filesystem::path> empty;
    return configuration_ ? configuration_->roots : empty;
}

void ResourceLocator::clearGobs()
{
    if (gobs().empty()) return;
    auto replacement = writableConfiguration();
    replacement->gobs.clear();
    configuration_.swap(replacement);
    previewCacheIdentity_ = nextPreviewCacheIdentity();
}

void ResourceLocator::mountGob(const std::filesystem::path& path)
{
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec) && !ec;
    if (exists)
    {
        for (const GobArchive& existing : gobs())
        {
            std::error_code eqEc;
            if (std::filesystem::equivalent(existing.path(), path, eqEc) && !eqEc) return;
        }
    }
    GobArchive archive = GobArchive::open(path);
    auto replacement = writableConfiguration();
    replacement->gobs.push_back(std::move(archive));
    configuration_.swap(replacement);
    previewCacheIdentity_ = nextPreviewCacheIdentity();
}

void ResourceLocator::reloadGobs()
{
    if (gobs().empty()) return;
    std::vector<GobArchive> reloaded;
    reloaded.reserve(gobs().size());
    for (const GobArchive& archive : gobs()) reloaded.push_back(GobArchive::open(archive.path()));
    auto replacement = writableConfiguration();
    replacement->gobs.swap(reloaded);
    configuration_.swap(replacement);
    previewCacheIdentity_ = nextPreviewCacheIdentity();
}

const std::vector<GobArchive>& ResourceLocator::gobs() const noexcept
{
    static const std::vector<GobArchive> empty;
    return configuration_ ? configuration_->gobs : empty;
}

std::vector<std::string> ResourceLocator::candidates(const std::string_view name) const
{
    std::vector<std::string> out;

    const std::string rel = normalizeRelative(name);
    if (rel.empty())
    {
        return out;
    }

    out.push_back(rel);

    const std::string ext = extensionLower(rel);
    const std::string base = basename(rel);

    auto addUnique = [&](const std::string_view v)
    {
        const std::string s = normalizeRelative(v);
        if (s.empty())
        {
            return;
        }
        if (std::find(out.begin(), out.end(), s) != out.end())
        {
            return;
        }
        out.push_back(s);
    };

    if (ext == ".wav")
    {
        addUnique("sound/" + base);
        addUnique("voice/" + base);
        addUnique("voiceuu/" + base);
    }
    else if (ext == ".mat")
    {
        addUnique("mat/" + base);
        addUnique("3do/mat/" + base);
    }
    else if (ext == ".dat")
    {
        addUnique("misc/" + base);
    }
    else if (ext == ".3do")
    {
        addUnique("3do/" + base);
        addUnique("hi3do/" + base);
    }
    else if (ext == ".key")
    {
        addUnique("3do/key/" + base);
    }
    else if (ext == ".ai" || ext == ".ai0" || ext == ".ai2")
    {
        addUnique("misc/ai/" + base);
    }
    else if (ext == ".cmp")
    {
        addUnique("misc/cmp/" + base);
    }
    else if (ext == ".par")
    {
        addUnique("misc/par/" + base);
    }
    else if (ext == ".per")
    {
        addUnique("misc/per/" + base);
    }
    else if (ext == ".pup")
    {
        addUnique("misc/pup/" + base);
    }
    else if (ext == ".snd")
    {
        addUnique("misc/snd/" + base);
    }
    else if (ext == ".spr")
    {
        addUnique("misc/spr/" + base);
    }
    else if (ext == ".cog")
    {
        addUnique("cog/" + base);
    }
    else if (ext == ".tpl")
    {
        addUnique("misc/" + base);
    }
    else if (ext == ".uni")
    {
        addUnique("misc/" + base);
        addUnique("ui/" + base);
    }

    return out;
}

std::optional<std::filesystem::path> ResourceLocator::findOnDisk(const std::string_view name) const
{
    const std::string rel = normalizeRelative(name);
    if (rel.empty())
    {
        return std::nullopt;
    }

    const std::filesystem::path namePath(rel);
    if (namePath.is_absolute())
    {
        std::error_code ec;
        if (std::filesystem::is_regular_file(namePath, ec) && !ec)
        {
            return namePath;
        }
        return std::nullopt;
    }

    for (const std::string& candidate : candidates(rel))
    {
        for (auto it = roots().rbegin(); it != roots().rend(); ++it)
        {
            std::filesystem::path p = *it / std::filesystem::path(candidate);
            std::error_code ec;
            if (std::filesystem::is_regular_file(p, ec) && !ec)
            {
                return p;
            }
        }
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> ResourceLocator::findSelectedOnDisk(
    const std::string_view name) const
{
    const std::string rel = normalizeRelative(name);
    if ( rel.empty() )
    {
        return std::nullopt;
    }

    const std::filesystem::path namePath(rel);
    if ( namePath.is_absolute() )
    {
        std::error_code ec;
        if ( std::filesystem::is_regular_file(namePath, ec) && !ec )
        {
            return namePath;
        }
        return std::nullopt;
    }

    if ( matchesPrimaryResourceOverride(rel) )
    {
        return std::nullopt;
    }

    for ( const std::string& candidate : candidates(rel) )
    {
        for ( auto it = roots().rbegin(); it != roots().rend(); ++it )
        {
            const std::filesystem::path path = *it / std::filesystem::path(candidate);
            std::error_code ec;
            if ( std::filesystem::is_regular_file(path, ec) && !ec )
            {
                return path;
            }
        }

        for ( auto it = gobs().rbegin(); it != gobs().rend(); ++it )
        {
            if ( it->contains(candidate) )
            {
                return std::nullopt;
            }
        }
    }

    return std::nullopt;
}

std::optional<GobEntry> ResourceLocator::findInGobs(const std::string_view name) const
{
    const std::string rel = normalizeRelative(name);
    if (rel.empty())
    {
        return std::nullopt;
    }

    for (const std::string& candidate : candidates(rel))
    {
        for (auto it = gobs().rbegin(); it != gobs().rend(); ++it)
        {
            if (const auto entry = it->find(candidate))
            {
                return entry;
            }
        }
    }

    return std::nullopt;
}

std::optional<std::vector<std::byte>> ResourceLocator::tryReadBinary(const std::string_view name) const
{
    if (matchesPrimaryResourceOverride(name))
    {
        const auto owner = primaryOverride_;
        std::call_once(owner->materialized, [&]()
        {
            try { owner->bytes = owner->reader(); }
            catch (...) { owner->error = std::current_exception(); }
            owner->reader = {};
        });
        if (owner->error) std::rethrow_exception(owner->error);
        return owner->bytes;
    }

    const std::string rel = normalizeRelative(name);
    if (rel.empty())
    {
        return std::nullopt;
    }

    const std::filesystem::path namePath(rel);
    if (namePath.is_absolute())
    {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(namePath, ec) || ec)
        {
            return std::nullopt;
        }
        return readFileBinary(namePath);
    }

    for (const std::string& candidate : candidates(rel))
    {
        for (auto it = roots().rbegin(); it != roots().rend(); ++it)
        {
            std::filesystem::path p = *it / std::filesystem::path(candidate);
            std::error_code ec;
            if (std::filesystem::is_regular_file(p, ec) && !ec)
            {
                return readFileBinary(p);
            }
        }

        for (auto it = gobs().rbegin(); it != gobs().rend(); ++it)
        {
            if (it->contains(candidate))
            {
                return it->read(candidate);
            }
        }
    }

    return std::nullopt;
}

std::vector<std::byte> ResourceLocator::readBinary(const std::string_view name) const
{
    const auto data = tryReadBinary(name);
    if (!data)
    {
        throw std::runtime_error("Resource not found: " + std::string(name));
    }
    return *data;
}

std::optional<std::vector<std::byte>> ResourceLocator::tryReadBinary(
    const std::string_view name,
    const std::size_t maxBytes) const
{
    bool exceededBudget = false;
    return tryReadBinary(name, maxBytes, exceededBudget);
}

std::optional<std::vector<std::byte>> ResourceLocator::tryReadBinary(
    const std::string_view name,
    const std::size_t maxBytes,
    bool& exceededBudget) const
{
    return tryReadBinaryWithBudget(name, maxBytes, exceededBudget, nullptr);
}

std::optional<std::vector<std::byte>> ResourceLocator::tryReadBinary(
    const std::string_view name,
    const std::size_t maxBytes,
    std::filesystem::path* diskSource) const
{
    bool exceededBudget = false;
    return tryReadBinaryWithBudget(name, maxBytes, exceededBudget, diskSource);
}

std::vector<std::byte> ResourceLocator::readBinary(
    const std::string_view name,
    const std::size_t maxBytes) const
{
    bool exceededBudget = false;
    const auto data = tryReadBinaryWithBudget(name, maxBytes, exceededBudget, nullptr);
    if ( !data )
    {
        if ( exceededBudget )
        {
            throw std::runtime_error("Resource exceeds byte budget: " + std::string(name));
        }
        throw std::runtime_error("Resource not found: " + std::string(name));
    }
    return *data;
}

std::optional<std::vector<std::byte>> ResourceLocator::tryReadBinaryWithBudget(
    const std::string_view name,
    const std::size_t maxBytes,
    bool& exceededBudget,
    std::filesystem::path* diskSource) const
{
    if ( diskSource )
    {
        diskSource->clear();
    }
    exceededBudget = false;
    try
    {
        if ( matchesPrimaryResourceOverride(name) )
        {
            const auto owner = primaryOverride_;
            std::call_once(owner->materialized, [&]()
            {
                try
                {
                    owner->bytes = owner->reader();
                }
                catch (...)
                {
                    owner->error = std::current_exception();
                }
                owner->reader = {};
            });
            if ( owner->error )
            {
                std::rethrow_exception(owner->error);
            }
            if ( owner->bytes && owner->bytes->size() > maxBytes )
            {
                exceededBudget = true;
                return std::nullopt;
            }
            return owner->bytes;
        }

        const std::string rel = normalizeRelative(name);
        if ( rel.empty() )
        {
            return std::nullopt;
        }

        const std::filesystem::path namePath(rel);
        if ( namePath.is_absolute() )
        {
            std::error_code ec;
            if ( !std::filesystem::is_regular_file(namePath, ec) || ec )
            {
                return std::nullopt;
            }
            auto data = tryReadFileBinaryWithinBudget(namePath, maxBytes, exceededBudget);
            if ( data && diskSource )
            {
                *diskSource = namePath;
            }
            return data;
        }

        for ( const std::string& candidate : candidates(rel) )
        {
            for ( auto it = roots().rbegin(); it != roots().rend(); ++it )
            {
                std::filesystem::path p = *it / std::filesystem::path(candidate);
                std::error_code ec;
                if ( std::filesystem::is_regular_file(p, ec) && !ec )
                {
                    auto data = tryReadFileBinaryWithinBudget(p, maxBytes, exceededBudget);
                    if ( data && diskSource )
                    {
                        *diskSource = p;
                    }
                    return data;
                }
            }

            for ( auto it = gobs().rbegin(); it != gobs().rend(); ++it )
            {
                if ( const auto entry = it->find(candidate) )
                {
                    if ( static_cast<std::uintmax_t>(entry->size)
                        > static_cast<std::uintmax_t>(maxBytes) )
                    {
                        exceededBudget = true;
                        return std::nullopt;
                    }
                    return it->read(candidate);
                }
            }
        }

        return std::nullopt;
    }
    catch (...)
    {
        if ( diskSource )
        {
            diskSource->clear();
        }
        throw;
    }
}

std::optional<std::vector<std::byte>> ResourceLocator::tryReadBinaryWithExt(
    const std::string_view name,
    const std::string_view ext) const
{
    const std::string_view trimmed = sed::text::trimView(name);
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    if (const auto data = tryReadBinary(trimmed))
    {
        return data;
    }

    if (sed::text::iendsWithAscii(trimmed, ext))
    {
        return std::nullopt;
    }

    std::string withExt(trimmed);
    withExt += ext;
    return tryReadBinary(withExt);
}

std::optional<std::string> ResourceLocator::tryReadText(const std::string_view name) const
{
    const auto data = tryReadBinary(name);
    if (!data)
    {
        return std::nullopt;
    }

    std::string text;
    text.resize(data->size());
    if (!text.empty())
    {
        std::memcpy(text.data(), data->data(), data->size());
    }

    return text;
}

std::string ResourceLocator::readText(const std::string_view name) const
{
    const auto text = tryReadText(name);
    if (!text)
    {
        throw std::runtime_error("Resource not found: " + std::string(name));
    }
    return *text;
}

std::optional<std::string> ResourceLocator::tryReadTextWithExt(const std::string_view name, const std::string_view ext) const
{
    const std::string_view trimmed = sed::text::trimView(name);
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    if (const auto text = tryReadText(trimmed))
    {
        return text;
    }

    if (sed::text::iendsWithAscii(trimmed, ext))
    {
        return std::nullopt;
    }

    std::string withExt(trimmed);
    withExt += ext;
    return tryReadText(withExt);
}

std::string ResourceLocator::normalizeRelative(const std::string_view path)
{
    const std::string_view trimmed = sed::text::trimView(path);
    if (trimmed.empty())
    {
        return {};
    }

    std::string out;
    out.reserve(trimmed.size());
    for (const char ch : trimmed)
    {
        out.push_back((ch == '\\') ? '/' : ch);
    }
    return out;
}

std::string ResourceLocator::extensionLower(const std::string_view name)
{
    const std::string normalized = normalizeRelative(name);

    const std::size_t slash = normalized.find_last_of('/');
    const std::size_t dot = normalized.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
    {
        return {};
    }

    std::string ext(normalized.substr(dot));
    for (char& ch : ext)
    {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return ext;
}

std::string ResourceLocator::basename(const std::string_view name)
{
    const std::string rel = normalizeRelative(name);
    if (rel.empty())
    {
        return {};
    }

    const std::size_t slash = rel.find_last_of('/');
    if (slash == std::string::npos)
    {
        return rel;
    }
    return rel.substr(slash + 1);
}

} // namespace sed::assets
