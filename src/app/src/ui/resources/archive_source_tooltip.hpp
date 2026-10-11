#pragma once

#include "sed/assets/resource_locator.hpp"
#include "sed/text.hpp"

#include <QString>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace sed::ui::resources
{

struct ArchiveSourceResolution
{
    // A winning non-archive source is still a resolution and must stop
    // extension fallback for callers which mirror tryReadTextWithExt.
    bool found{};
    QString tooltip{};
};

/**
 * Resolve source metadata following tryReadBinary's precedence. The returned
 * tooltip contains the mounted archive's basename and the entry spelling
 * stored in its index; it never reads archive payload bytes or exposes the
 * archive's filesystem path.
 */
inline ArchiveSourceResolution resolveArchiveSourceTooltip(
    const assets::ResourceLocator* const resources,
    const std::string_view lookup)
{
    if (!resources || lookup.empty())
    {
        return {};
    }

    if (resources->primaryResourceOverrideRevision(lookup))
    {
        return {true, {}};
    }

    std::string normalized(sed::text::trimView(lookup));
    for (char& character : normalized)
    {
        if (character == '\\')
        {
            character = '/';
        }
    }
    const std::filesystem::path lookupPath(normalized);
    if (lookupPath.is_absolute())
    {
        std::error_code error{};
        return {std::filesystem::is_regular_file(lookupPath, error) && !error, {}};
    }

    for (const std::string& candidate : resources->candidates(lookup))
    {
        for (auto root = resources->roots().rbegin(); root != resources->roots().rend(); ++root)
        {
            const std::filesystem::path path = *root / std::filesystem::path(candidate);
            std::error_code error{};
            if (std::filesystem::is_regular_file(path, error) && !error)
            {
                return {true, {}};
            }
        }

        for (auto archive = resources->gobs().rbegin(); archive != resources->gobs().rend(); ++archive)
        {
            if (const auto entry = archive->find(candidate))
            {
                const QString archiveName = QString::fromStdWString(archive->path().filename().wstring());
                const QString entryName = QString::fromLatin1(
                    entry->name.data(), static_cast<qsizetype>(entry->name.size()));
                const QString tooltip = archiveName + QStringLiteral(": ") + entryName;
                return {true, tooltip.size() <= 32768 ? tooltip : QString{}};
            }
        }
    }

    return {};
}

inline QString resolveArchiveSourceTooltipWithFallback(
    const assets::ResourceLocator* const resources,
    const std::string_view lookup,
    const std::string_view fallbackSuffix)
{
    const ArchiveSourceResolution primary = resolveArchiveSourceTooltip(resources, lookup);
    if (primary.found || fallbackSuffix.empty()
        || (lookup.size() >= fallbackSuffix.size()
            && sed::text::iendsWithAscii(lookup, fallbackSuffix)))
    {
        return primary.tooltip;
    }

    std::string fallback(lookup);
    fallback.append(fallbackSuffix);
    return resolveArchiveSourceTooltip(resources, fallback).tooltip;
}

inline bool validArchiveSourceTooltip(const QString& tooltip) noexcept
{
    return tooltip.size() <= 32768 && !tooltip.contains(QChar::Null);
}

} // namespace sed::ui::resources
