#pragma once

#include "sed/assets/resource_locator.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace sed::ui::detail
{

/**
 * Stable cache-key material for immutable ResourceLocator snapshots.
 * Callers include the exact selected disk/GOB revision in first/second, so an
 * object's address must not isolate equivalent dialog-owned snapshots.
 */
inline std::string previewResourceCacheIdentity(
    const assets::ResourceLocator* resources,
    const std::string_view first,
    const std::string_view second = {})
{
    (void)resources;
    return ::sed::text::upperAscii(first)
        + "|"
        + ::sed::text::upperAscii(second);
}

inline std::string previewResourceLookupName(
    const std::string_view name,
    const std::string_view ext = {})
{
    std::string lookup = ::sed::text::trim(name);
    if (lookup.empty())
    {
        return {};
    }

    if (!ext.empty() && std::filesystem::path(lookup).extension().empty())
    {
        lookup += std::string(ext);
    }
    return lookup;
}

inline std::string previewFileRevisionToken(
    const std::string_view source,
    const std::filesystem::path& path)
{
    std::error_code timeEc;
    std::error_code sizeEc;
    const auto stamp = std::filesystem::last_write_time(path, timeEc);
    const std::uintmax_t size = std::filesystem::file_size(path, sizeEc);
    return std::string(source)
        + "|"
        + path.lexically_normal().generic_string()
        + "|"
        + std::to_string(sizeEc ? 0u : size)
        + "|"
        + std::to_string(timeEc ? 0ll : stamp.time_since_epoch().count());
}

/**
 * Identifies the exact disk file or GOB entry ResourceLocator would read.
 * Including the selected path and archive metadata also invalidates caches
 * when roots/GOB mounts change while the locator object itself is retained.
 */
inline std::optional<std::string> locatePreviewResourceRevisionToken(
    const assets::ResourceLocator* resources,
    const std::string_view lookup)
{
    if (const auto revision = resources->primaryResourceOverrideRevision(lookup))
        return "PRIMARY|" + std::to_string(*revision);
    const std::filesystem::path lookupPath(lookup);
    if (lookupPath.is_absolute())
    {
        std::error_code existsEc;
        if (std::filesystem::is_regular_file(lookupPath, existsEc) && !existsEc)
        {
            return previewFileRevisionToken("DISK", lookupPath);
        }
        return std::nullopt;
    }

    for (const std::string& candidate : resources->candidates(lookup))
    {
        for (auto root = resources->roots().rbegin(); root != resources->roots().rend(); ++root)
        {
            const std::filesystem::path path = *root / std::filesystem::path(candidate);
            std::error_code existsEc;
            if (std::filesystem::is_regular_file(path, existsEc) && !existsEc)
            {
                return previewFileRevisionToken("DISK", path);
            }
        }

        for (auto archive = resources->gobs().rbegin(); archive != resources->gobs().rend(); ++archive)
        {
            if (const auto entry = archive->find(candidate))
            {
                return previewFileRevisionToken("GOB", archive->path())
                    + "|mount=" + std::to_string(resources->previewCacheIdentity())
                    + "|"
                    + ::sed::text::upperAscii(entry->name)
                    + "|"
                    + std::to_string(entry->offset)
                    + "|"
                    + std::to_string(entry->size);
            }
        }
    }

    return std::nullopt;
}

inline std::string previewResourceRevisionToken(
    const assets::ResourceLocator* resources,
    const std::string_view name,
    const std::string_view ext = {})
{
    if (!resources)
    {
        return "NORES";
    }

    const std::string lookup = ::sed::text::trim(name);
    if (lookup.empty())
    {
        return "EMPTY";
    }

    // Match tryReadBinaryWithExt exactly: an explicitly named resource wins,
    // and the extension-appended lookup is only the fallback.
    if (const auto token = locatePreviewResourceRevisionToken(resources, lookup))
    {
        return *token;
    }
    if (ext.empty() || ::sed::text::iendsWithAscii(lookup, ext))
    {
        return "MISSING";
    }
    if (const auto token = locatePreviewResourceRevisionToken(
            resources,
            lookup + std::string(ext)))
    {
        return *token;
    }
    return "MISSING";
}

struct PreviewResourceDiskWatchPlan
{
    std::vector<std::filesystem::path> files{};
    std::vector<std::filesystem::path> directories{};
};

inline std::filesystem::path normalizedPreviewWatchPath(
    const std::filesystem::path& path)
{
    std::error_code ec;
    const std::filesystem::path absolute = std::filesystem::absolute(path, ec);
    return (ec ? path : absolute).lexically_normal();
}

inline void appendPreviewWatchPath(
    std::vector<std::filesystem::path>& paths,
    std::unordered_set<std::wstring>& keys,
    const std::filesystem::path& rawPath)
{
    if (rawPath.empty())
    {
        return;
    }

    const std::filesystem::path path = normalizedPreviewWatchPath(rawPath);
    if (keys.insert(path.wstring()).second)
    {
        paths.push_back(path);
    }
}

/**
 * Builds the disk watch set for the exact resource ResourceLocator would read.
 * Candidate parent directories are also included so replacing/deleting the
 * selected file, or creating a higher-priority loose-file override, wakes the
 * consumer and lets it resolve precedence again.
 */
inline PreviewResourceDiskWatchPlan previewResourceDiskWatchPlan(
    const assets::ResourceLocator* resources,
    const std::string_view name,
    const std::string_view ext = {})
{
    PreviewResourceDiskWatchPlan plan{};
    if (!resources)
    {
        return plan;
    }

    const std::string lookup = ::sed::text::trim(name);
    if (lookup.empty())
    {
        return plan;
    }

    std::unordered_set<std::wstring> fileKeys{};
    std::unordered_set<std::wstring> directoryKeys{};

    const auto addExistingDirectoryOrAncestor = [&](std::filesystem::path directory)
    {
        while (!directory.empty())
        {
            std::error_code ec;
            if (std::filesystem::is_directory(directory, ec) && !ec)
            {
                appendPreviewWatchPath(plan.directories, directoryKeys, directory);
                return;
            }

            const std::filesystem::path parent = directory.parent_path();
            if (parent == directory)
            {
                return;
            }
            directory = parent;
        }
    };

    const auto addLookupCandidates = [&](const std::string_view candidateLookup)
    {
        const std::filesystem::path lookupPath(candidateLookup);
        if (lookupPath.is_absolute())
        {
            addExistingDirectoryOrAncestor(lookupPath.parent_path());
            std::error_code ec;
            if (std::filesystem::is_regular_file(lookupPath, ec) && !ec)
            {
                appendPreviewWatchPath(plan.files, fileKeys, lookupPath);
            }
            return;
        }

        for (const std::string& candidate : resources->candidates(candidateLookup))
        {
            for (const std::filesystem::path& root : resources->roots())
            {
                addExistingDirectoryOrAncestor(
                    root / std::filesystem::path(candidate).parent_path());
            }
        }
    };

    // tryReadBinaryWithExt first attempts the explicit name and only then the
    // extension-appended fallback. Watch both candidate sets so a new explicit
    // override is noticed even while the fallback currently wins.
    addLookupCandidates(lookup);
    if (!ext.empty() && !::sed::text::iendsWithAscii(lookup, ext))
    {
        addLookupCandidates(lookup + std::string(ext));
    }

    const auto addSelectedFile = [&](const std::string_view selectedLookup) -> bool
    {
        const std::filesystem::path lookupPath(selectedLookup);
        if (lookupPath.is_absolute())
        {
            std::error_code ec;
            if (std::filesystem::is_regular_file(lookupPath, ec) && !ec)
            {
                appendPreviewWatchPath(plan.files, fileKeys, lookupPath);
                return true;
            }
            return false;
        }

        for (const std::string& candidate : resources->candidates(selectedLookup))
        {
            for (auto root = resources->roots().rbegin(); root != resources->roots().rend(); ++root)
            {
                const std::filesystem::path path = *root / std::filesystem::path(candidate);
                std::error_code ec;
                if (std::filesystem::is_regular_file(path, ec) && !ec)
                {
                    appendPreviewWatchPath(plan.files, fileKeys, path);
                    return true;
                }
            }

            // A GOB entry at this precedence means no loose file currently
            // wins. Its candidate directories remain watched for overrides;
            // archive remounting itself is owned by project resource setup.
            for (auto archive = resources->gobs().rbegin(); archive != resources->gobs().rend(); ++archive)
            {
                if (archive->find(candidate))
                {
                    return true;
                }
            }
        }
        return false;
    };

    if (!addSelectedFile(lookup)
        && !ext.empty()
        && !::sed::text::iendsWithAscii(lookup, ext))
    {
        (void)addSelectedFile(lookup + std::string(ext));
    }

    return plan;
}

inline void appendPreviewRevisionPart(
    std::string& token,
    const std::string_view label,
    const std::string_view value)
{
    token += "|";
    token += label;
    token += "=";
    token += std::to_string(value.size());
    token += ":";
    token += value;
}

/** Complete dependency revision for a rendered 3DO preview. */
inline std::string previewModelDependencyRevisionToken(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view modelName,
    const std::vector<std::string>& materials)
{
    std::string token("MODEL_PREVIEW_V2");
    appendPreviewRevisionPart(
        token,
        "3DO",
        previewResourceRevisionToken(resources, modelName, ".3do"));
    appendPreviewRevisionPart(
        token,
        "CMP",
        ::sed::text::upperAscii(cmpName)
            + "|"
            + previewResourceRevisionToken(resources, cmpName, ".cmp"));

    std::vector<std::string> names{};
    names.reserve(materials.size());
    for (const std::string& material : materials)
    {
        std::string name = ::sed::text::trim(material);
        if (!name.empty() && name != "-")
        {
            names.push_back(std::move(name));
        }
    }
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b)
        {
            return ::sed::text::upperAscii(a) < ::sed::text::upperAscii(b);
        });
    names.erase(
        std::unique(names.begin(), names.end(), [](const std::string& a, const std::string& b)
            {
                return ::sed::text::iequalsAscii(a, b);
            }),
        names.end());

    for (const std::string& material : names)
    {
        appendPreviewRevisionPart(
            token,
            "MAT",
            ::sed::text::upperAscii(material)
                + "|"
                + previewResourceRevisionToken(resources, material, ".mat"));
    }
    return token;
}

} // namespace sed::ui::detail
