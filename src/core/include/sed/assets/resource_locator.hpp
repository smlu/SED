#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sed/assets/gob.hpp"

namespace sed::assets
{

class ResourceLocator
{
public:
    ResourceLocator() = default;
    ResourceLocator(const ResourceLocator&) = default;
    ResourceLocator(ResourceLocator&& source) noexcept;
    ResourceLocator& operator=(ResourceLocator replacement) noexcept;

    /** Cheap hint scope copied with immutable snapshots; changes only after a
     * successful root, mounted-index, or primary-override mutation. This identifies
     * configuration, not immutable loose-file contents. */
    std::uint64_t previewCacheIdentity() const noexcept;

    using PrimaryResourceReader = std::function<std::optional<std::vector<std::byte>>() >;
    /** Pin one resource and its standard candidate aliases while retaining
     * normal dependency lookup. Copies share once-only lazy materialization,
     * including a missing result or exception, until explicitly replaced. */
    void setPrimaryResourceOverride(std::string_view name, PrimaryResourceReader reader);
    void clearPrimaryResourceOverride() noexcept;
    /** Cheap immutable payload revision. Never invokes the lazy reader. */
    std::optional<std::uint64_t> primaryResourceOverrideRevision(std::string_view name) const;

    void clearRoots();
    void addRoot(const std::filesystem::path& root);
    // References belong to the current shared configuration. A mutation may
    // detach this locator; retain a copied locator when retaining references.
    const std::vector<std::filesystem::path>& roots() const noexcept;

    void clearGobs();
    void mountGob(const std::filesystem::path& path);
    /**
     * Reopens every mounted archive from its original path and publishes the
     * new indexes as one transaction.  If any replacement is incomplete or
     * malformed, the currently mounted indexes remain intact and the
     * exception is propagated to the caller.
     */
    void reloadGobs();
    const std::vector<GobArchive>& gobs() const noexcept;

    std::vector<std::string> candidates(const std::string_view name) const;

    std::optional<std::filesystem::path> findOnDisk(const std::string_view name) const;
    /** Returns the loose file only when it wins normal candidate/root/archive precedence.
     * A selected primary override or GOB entry returns no path. This metadata query
     * never materializes a lazy primary override or reads resource payload bytes. */
    std::optional<std::filesystem::path> findSelectedOnDisk(const std::string_view name) const;
    std::optional<GobEntry> findInGobs(const std::string_view name) const;

    std::optional<std::vector<std::byte>> tryReadBinary(const std::string_view name) const;
    std::vector<std::byte> readBinary(const std::string_view name) const;
    /** Reads the selected resource only when its payload fits maxBytes. An oversized winner is refused without
     * falling back to a shadowed resource. Primary override callbacks are checked after their trusted result is
     * materialized because they do not provide size metadata. */
    std::optional<std::vector<std::byte>> tryReadBinary(
        const std::string_view name,
        std::size_t maxBytes) const;
    /** Like the two-argument bounded read, while reporting whether the selected winner
     * was refused for exceeding maxBytes. A missing resource leaves exceededBudget false. */
    std::optional<std::vector<std::byte>> tryReadBinary(
        std::string_view name,
        std::size_t maxBytes,
        bool& exceededBudget) const;
    /** Optionally reports the loose file that supplied a successful bounded read. The output is cleared for every
     * non-loose result, refusal, missing resource, or exception. */
    std::optional<std::vector<std::byte>> tryReadBinary(
        const std::string_view name,
        std::size_t maxBytes,
        std::filesystem::path* diskSource) const;
    std::vector<std::byte> readBinary(const std::string_view name, std::size_t maxBytes) const;
    std::optional<std::vector<std::byte>> tryReadBinaryWithExt(const std::string_view name, const std::string_view ext) const;

    std::optional<std::string> tryReadText(const std::string_view name) const;
    std::string readText(const std::string_view name) const;
    std::optional<std::string> tryReadTextWithExt(const std::string_view name, const std::string_view ext) const;

private:
    static std::string normalizeRelative(const std::string_view path);
    static std::string extensionLower(const std::string_view name);
    static std::string basename(const std::string_view name);

    struct Configuration;
    struct PrimaryResourceOverride;
    std::shared_ptr<Configuration> writableConfiguration() const;
    bool matchesPrimaryResourceOverride(std::string_view name) const;
    std::optional<std::vector<std::byte>> tryReadBinaryWithBudget(
        std::string_view name,
        std::size_t maxBytes,
        bool& exceededBudget,
        std::filesystem::path* diskSource) const;

    std::uint64_t previewCacheIdentity_{};
    std::shared_ptr<Configuration> configuration_{};
    std::shared_ptr<PrimaryResourceOverride> primaryOverride_{};
};

} // namespace sed::assets
