#include "ui/resources/project_resource_settings.hpp"

#include <algorithm>
#include <array>
#include <system_error>
#include <utility>

#include <QDir>

#include "sed/assets/resource_locator.hpp"
#include "sed/text.hpp"

namespace sed::ui::resources
{
namespace
{

constexpr std::array<std::string_view, 2> kIjimArchiveNames{
    "CD1.GOB",
    "CD2.GOB",
};

constexpr std::array<std::string_view, 5> kJkArchiveNames{
    "Res1hi.gob",
    "Res2.gob",
    "Jk1.gob",
    "Jk1mp.gob",
    "Jk1ctf.gob",
};

constexpr std::string_view kJkFallbackFirstArchive = "Res1low.gob";

constexpr std::array<std::string_view, 6> kMotsArchiveNames{
    "JKMsndLO.goo",
    "Jkmres.goo",
    "Jkm.goo",
    "Jkm_mp.goo",
    "Jkm_kfy.goo",
    "JKM_SABER.GOO",
};

struct ProjectPathSignature
{
    ProjectType kind{};
    std::string_view filenameLower{};
};

constexpr std::array<ProjectPathSignature, 9>
    kProjectPathSignatures{
        ProjectPathSignature{ ProjectType::JKDF2, "res1hi.gob" },
        ProjectPathSignature{ ProjectType::JKDF2, "res1low.gob" },
        ProjectPathSignature{ ProjectType::JKDF2, "res2.gob" },
        ProjectPathSignature{ ProjectType::JKDF2, "jk1.gob" },
        ProjectPathSignature{ ProjectType::MOTS, "jkmres.goo" },
        ProjectPathSignature{ ProjectType::MOTS, "jkmsndlo.goo" },
        ProjectPathSignature{ ProjectType::MOTS, "jkm.goo" },
        ProjectPathSignature{ ProjectType::IJIM, "cd1.gob" },
        ProjectPathSignature{ ProjectType::IJIM, "cd2.gob" },
    };

std::optional<std::size_t> projectTypeIndex(const ProjectType kind) noexcept
{
    switch (kind)
    {
    case ProjectType::JKDF2: return 0;
    case ProjectType::MOTS: return 1;
    case ProjectType::IJIM: return 2;
    default: return std::nullopt;
    }
}

ProjectType projectTypeAtIndex(const std::size_t index) noexcept
{
    constexpr std::array<ProjectType, 3> kKinds{
        ProjectType::JKDF2,
        ProjectType::MOTS,
        ProjectType::IJIM,
    };
    return index < kKinds.size() ? kKinds[index] : ProjectType::JKDF2;
}

void addUniquePath(std::vector<std::filesystem::path>& paths, const std::filesystem::path& path)
{
    if (path.empty())
    {
        return;
    }
    if (std::find(paths.begin(), paths.end(), path) != paths.end())
    {
        return;
    }
    paths.push_back(path);
}

bool addUniqueExistingEquivalent(std::vector<std::filesystem::path>& paths, const std::filesystem::path& path)
{
    if (path.empty())
    {
        return false;
    }

    for (const std::filesystem::path& existing : paths)
    {
        std::error_code ec;
        if (!existing.empty() && std::filesystem::equivalent(existing, path, ec) && !ec)
        {
            return false;
        }
    }

    paths.push_back(path);
    return true;
}

template<std::size_t N>
std::optional<std::filesystem::path> findFileInAnyDir(
    const std::array<std::filesystem::path, N>& dirs,
    const std::string_view name)
{
    for (const std::filesystem::path& dir : dirs)
    {
        if (const auto p = findFileIfExists(dir / std::filesystem::path(name)))
        {
            return p;
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<QString> settingsProjectName(const ProjectType kind)
{
    switch (kind)
    {
    case ProjectType::JKDF2:
        return QStringLiteral("jkdf2");
    case ProjectType::MOTS:
        return QStringLiteral("mots");
    case ProjectType::IJIM:
        return QStringLiteral("ijim");
    default:
        return std::nullopt;
    }
}

std::optional<ProjectType> parseProjectTypeSettingValue(const QString& value) noexcept
{
    const QString v = value.trimmed().toLower();
    if (v == QStringLiteral("jkdf2"))
    {
        return ProjectType::JKDF2;
    }
    if (v == QStringLiteral("mots"))
    {
        return ProjectType::MOTS;
    }
    if (v == QStringLiteral("ijim"))
    {
        return ProjectType::IJIM;
    }
    return std::nullopt;
}

bool isAllowedArchiveExt(const std::string_view extLower, const std::optional<ProjectType> kindFilter) noexcept
{
    if (!kindFilter)
    {
        return extLower == ".gob" || extLower == ".goo";
    }

    switch (*kindFilter)
    {
    case ProjectType::JKDF2: [[fallthrough]];
    case ProjectType::MOTS:
        return extLower == ".goo" || extLower == ".gob";
    case ProjectType::IJIM:
        return extLower == ".gob";
    default:
        return false;
    }
}

bool isValidProjectType(const ProjectType kind) noexcept
{
    switch (kind)
    {
    case ProjectType::JKDF2:
    case ProjectType::MOTS:
    case ProjectType::IJIM:
        return true;
    default:
        return false;
    }
}

ProjectInstallPathValidation validateProjectInstallPath(
    const ProjectType expectedKind,
    const std::filesystem::path& installDir) noexcept
{
    const std::optional<std::size_t> expectedIndex =
        projectTypeIndex(expectedKind);
    if (!expectedIndex || installDir.empty())
    {
        return {};
    }

    try
    {
        std::array<std::optional<std::filesystem::path>, 3> evidence{};
        std::vector<std::filesystem::path> candidateDirs{
            installDir,
            installDir / "Resource",
            installDir / "Episode",
        };
        candidateDirs.erase(
            std::unique(candidateDirs.begin(), candidateDirs.end()),
            candidateDirs.end());

        for (const std::filesystem::path& directory : candidateDirs)
        {
            std::error_code dirEc;
            if (!std::filesystem::is_directory(directory, dirEc) || dirEc)
            {
                continue;
            }

            std::filesystem::directory_iterator it(
                directory,
                std::filesystem::directory_options::skip_permission_denied,
                dirEc);
            const std::filesystem::directory_iterator end{};
            while (!dirEc && it != end)
            {
                std::error_code regularEc;
                if (it->is_regular_file(regularEc) && !regularEc)
                {
                    const std::string filenameLower = sed::text::lowerAscii(
                        it->path().filename().string());
                    for (const ProjectPathSignature& signature :
                         kProjectPathSignatures)
                    {
                        const std::optional<std::size_t> signatureIndex =
                            projectTypeIndex(signature.kind);
                        if (signatureIndex
                            && !evidence[*signatureIndex]
                            && filenameLower == signature.filenameLower)
                        {
                            evidence[*signatureIndex] = it->path();
                        }
                    }
                }
                it.increment(dirEc);
            }
        }

        if (evidence[*expectedIndex])
        {
            return ProjectInstallPathValidation{
                .match = ProjectInstallPathMatch::matching,
                .detectedKind = expectedKind,
                .signaturePath = *evidence[*expectedIndex],
            };
        }

        std::optional<std::size_t> detectedIndex{};
        for (std::size_t index = 0; index < evidence.size(); ++index)
        {
            if (!evidence[index])
            {
                continue;
            }
            if (detectedIndex)
            {
                // A shared/custom root containing signatures for multiple
                // games is ambiguous rather than an obvious configuration
                // mistake.
                return {};
            }
            detectedIndex = index;
        }

        if (!detectedIndex)
        {
            return {};
        }

        return ProjectInstallPathValidation{
            .match = ProjectInstallPathMatch::mismatched,
            .detectedKind = projectTypeAtIndex(*detectedIndex),
            .signaturePath = *evidence[*detectedIndex],
        };
    }
    catch (...)
    {
        // A path that cannot be fully inspected remains usable. This catch
        // also covers allocation while copying signature paths into the
        // noexcept result. Normal resource discovery reports concrete read
        // failures later.
        return {};
    }
}

std::optional<std::filesystem::path> findFileIfExists(const std::filesystem::path& path) noexcept
{
    std::error_code ec;
    if (std::filesystem::exists(path, ec) && !ec)
    {
        return path;
    }
    return std::nullopt;
}

ResourceSettings findDefaultProjectResources(const ProjectType kind, const std::filesystem::path& installDir)
{
    ResourceSettings out{};

    const std::filesystem::path resourceDir = installDir / "Resource";
    const std::filesystem::path episodeDir = installDir / "Episode";

    const std::optional<std::filesystem::path> resourceDirOnDisk = findFileIfExists(resourceDir);
    const std::optional<std::filesystem::path> episodeDirOnDisk = findFileIfExists(episodeDir);

    if (kind == ProjectType::IJIM)
    {
        addUniquePath(out.roots, resourceDirOnDisk ? *resourceDirOnDisk : installDir);
    }
    else
    {
        addUniquePath(out.roots, installDir);
        if (resourceDirOnDisk)
        {
            addUniquePath(out.roots, *resourceDirOnDisk);
        }
        if (episodeDirOnDisk)
        {
            addUniquePath(out.roots, *episodeDirOnDisk);
        }
    }

    if (kind == ProjectType::IJIM)
    {
        const std::filesystem::path resourceRoot = resourceDirOnDisk ? *resourceDirOnDisk : installDir;
        const std::array<std::filesystem::path, 2> archiveDirs{ resourceRoot, installDir };

        // IJIM archives may be in Resource/ or beside it, depending on install layout.
        for (const std::string_view name : kIjimArchiveNames)
        {
            if (const auto archive = findFileInAnyDir(archiveDirs, name))
            {
                addUniquePath(out.gobs, *archive);
            }
        }

        return out;
    }

    if (kind == ProjectType::JKDF2)
    {
        const std::array<std::filesystem::path, 3> archiveDirs{
            episodeDir,
            resourceDir,
            installDir,
        };

        // Prefer the high-resolution base archive, then fall back to the low-resolution copy.
        if (const auto res1 = findFileInAnyDir(archiveDirs, kJkArchiveNames.front()))
        {
            addUniquePath(out.gobs, *res1);
        }
        else if (const auto res1 = findFileInAnyDir(archiveDirs, kJkFallbackFirstArchive))
        {
            addUniquePath(out.gobs, *res1);
        }

        for (std::size_t i = 1; i < kJkArchiveNames.size(); ++i)
        {
            if (const auto archive = findFileInAnyDir(archiveDirs, kJkArchiveNames[i]))
            {
                addUniquePath(out.gobs, *archive);
            }
        }

        return out;
    }

    if (kind == ProjectType::MOTS)
    {
        const std::array<std::filesystem::path, 3> archiveDirs{
            episodeDir,
            resourceDir,
            installDir,
        };

        for (const std::string_view name : kMotsArchiveNames)
        {
            if (const auto archive = findFileInAnyDir(archiveDirs, name))
            {
                addUniquePath(out.gobs, *archive);
            }
        }

        return out;
    }

    return out;
}

ResourceSettingsMergeResult mergeDetectedResourceSettings(
    ResourceSettings existing,
    const ResourceSettings& detected)
{
    bool changed = false;

    // Merge roots before archives so ResourceLocator search order remains stable.
    for (const std::filesystem::path& root : detected.roots)
    {
        changed = addUniqueExistingEquivalent(existing.roots, root) || changed;
    }

    for (const std::filesystem::path& gob : detected.gobs)
    {
        changed = addUniqueExistingEquivalent(existing.gobs, gob) || changed;
    }

    return ResourceSettingsMergeResult{ std::move(existing), changed };
}

bool pathIsUnderInstallDir(const std::filesystem::path& path, const std::filesystem::path& installDir)
{
    std::error_code ecPath;
    std::error_code ecBase;
    const std::filesystem::path absPath = std::filesystem::absolute(path, ecPath).lexically_normal();
    const std::filesystem::path absBase = std::filesystem::absolute(installDir, ecBase).lexically_normal();

    const QString pathLower = QString::fromStdWString(absPath.wstring()).toLower();
    const QString baseLower = QString::fromStdWString(absBase.wstring()).toLower();
    QString baseLowerWithSeparator = baseLower;
    if (!baseLowerWithSeparator.endsWith('/') && !baseLowerWithSeparator.endsWith('\\'))
    {
        baseLowerWithSeparator += QDir::separator();
    }

    return pathLower == baseLower || pathLower.startsWith(baseLowerWithSeparator);
}

bool pathsEquivalentForSettings(const std::filesystem::path& a, const std::filesystem::path& b)
{
    if (a.empty() || b.empty())
    {
        return false;
    }

    std::error_code ecA;
    std::error_code ecB;
    const bool aExists = std::filesystem::exists(a, ecA) && !ecA;
    const bool bExists = std::filesystem::exists(b, ecB) && !ecB;

    if (aExists && bExists)
    {
        std::error_code eqEc;
        if (std::filesystem::equivalent(a, b, eqEc) && !eqEc)
        {
            return true;
        }
    }

    const QString aLower = QString::fromStdWString(a.lexically_normal().wstring()).toLower();
    const QString bLower = QString::fromStdWString(b.lexically_normal().wstring()).toLower();
    return aLower == bLower;
}

void putIjimResourceRootFirst(std::vector<std::filesystem::path>& roots, const std::filesystem::path& installDir)
{
    const std::filesystem::path resourceDir = installDir / "Resource";
    const std::filesystem::path desiredRoot = findFileIfExists(resourceDir).value_or(installDir);

    auto removeEquivalent = [&](const std::filesystem::path& target)
    {
        roots.erase(
            std::remove_if(
                roots.begin(),
                roots.end(),
                [&](const std::filesystem::path& path)
                {
                    return pathsEquivalentForSettings(path, target);
                }),
            roots.end());
    };

    // Remove both candidate roots first so inserting at the front never duplicates them.
    removeEquivalent(desiredRoot);
    if (desiredRoot != installDir)
    {
        removeEquivalent(installDir);
    }
    roots.insert(roots.begin(), desiredRoot);
}

std::vector<std::filesystem::path> collectGobsInDir(
    const std::filesystem::path& dir,
    const std::optional<ProjectType> kindFilter)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec) || ec)
    {
        return {};
    }

    std::vector<std::filesystem::path> gobs{};
    for (const auto& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec))
    {
        if (ec)
        {
            break;
        }

        std::error_code isRegEc;
        if (!entry.is_regular_file(isRegEc) || isRegEc)
        {
            continue;
        }

        const std::string ext = sed::text::lowerAscii(entry.path().extension().string());

        if (!isAllowedArchiveExt(ext, kindFilter))
        {
            continue;
        }

        gobs.push_back(entry.path());
    }

    std::sort(gobs.begin(), gobs.end());
    return gobs;
}

void configureResourceLocator(
    assets::ResourceLocator& locator,
    const std::filesystem::path& levelPath,
    const std::optional<ProjectType> archiveKindFilter)
{
    (void)archiveKindFilter;
    const std::filesystem::path levelDir = levelPath.parent_path();
    if (levelDir.empty())
    {
        return;
    }

    const std::vector<std::filesystem::path> roots = locator.roots();
    if (std::find(roots.begin(), roots.end(), levelDir) != roots.end())
    {
        return;
    }

    locator.addRoot(levelDir);
}

} // namespace sed::ui::resources
