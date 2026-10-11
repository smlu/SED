#include "template_resource_utils.hpp"

#include <algorithm>
#include <array>
#include <system_error>

#include <QCoreApplication>
#include <Qt>

namespace sed::ui
{

std::optional<QString> templateFileNameForProject(const ProjectType kind)
{
    switch (kind)
    {
        case ProjectType::JKDF2: return QStringLiteral("master.tpl");
        case ProjectType::MOTS:  return QStringLiteral("mots.tpl");
        case ProjectType::IJIM:  return QStringLiteral("ijim.tpl");
        default:                 return std::nullopt;
    }
}

std::optional<std::filesystem::path> findDataRootUpward(const std::filesystem::path& start)
{
    if (start.empty())
    {
        return std::nullopt;
    }

    std::error_code ec{};
    for (std::filesystem::path cur = start; !cur.empty(); cur = cur.parent_path())
    {
        // Step 1: check the conventional "data" child under this ancestor.
        const std::filesystem::path dataDir = cur / "data";
        if (std::filesystem::exists(dataDir, ec) && std::filesystem::is_directory(dataDir, ec))
        {
            return dataDir;
        }

        // Step 2: stop explicitly at the root to avoid looping on parent_path().
        if (cur == cur.root_path())
        {
            break;
        }
    }
    return std::nullopt;
}

std::optional<std::filesystem::path> findDataRoot()
{
    struct Candidate
    {
        std::filesystem::path path{};
        int projectTemplateCount{};
        int supportingFileCount{};
    };

    const auto scoreDataRoot = [](const std::filesystem::path& dataRoot)
        -> std::optional<Candidate>
    {
        std::error_code ec{};
        if (!std::filesystem::is_directory(dataRoot, ec) || ec)
        {
            return std::nullopt;
        }

        Candidate candidate{};
        candidate.path = dataRoot;
        for (const char* fileName : { "master.tpl", "mots.tpl", "ijim.tpl" })
        {
            ec.clear();
            candidate.projectTemplateCount +=
                std::filesystem::is_regular_file(dataRoot / fileName, ec)
                    && !ec
                ? 1
                : 0;
        }
        constexpr std::array<const char*, 7> supportingFiles{
            "shapes.tpl",
            "tpls.jlh",
            "tpls.jll",
            "mtpls.jlh",
            "mtpls.jll",
            "ijimtpls.jlh",
            "ijimtpls.jll",
        };
        for (const char* fileName : supportingFiles)
        {
            ec.clear();
            candidate.supportingFileCount +=
                std::filesystem::is_regular_file(dataRoot / fileName, ec)
                    && !ec
                ? 1
                : 0;
        }
        return candidate;
    };

    const auto betterCandidate = [](const Candidate& candidate,
                                      const Candidate& current) noexcept
    {
        return candidate.projectTemplateCount > current.projectTemplateCount
            || (candidate.projectTemplateCount
                    == current.projectTemplateCount
                && candidate.supportingFileCount
                    > current.supportingFileCount);
    };

    std::optional<Candidate> best{};
    const auto inspectUpward = [&](std::filesystem::path start)
    {
        for (std::filesystem::path current = std::move(start);
             !current.empty(); current = current.parent_path())
        {
            if (const std::optional<Candidate> candidate =
                    scoreDataRoot(current / "data");
                candidate && (!best || betterCandidate(*candidate, *best)))
            {
                best = candidate;
            }
            if (current == current.root_path())
            {
                break;
            }
        }
    };

    const QString applicationDirectory =
        QCoreApplication::applicationDirPath();
    if (!applicationDirectory.isEmpty())
    {
        inspectUpward(std::filesystem::path(
            applicationDirectory.toStdWString()));
    }

    std::error_code ec{};
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (!ec)
    {
        inspectUpward(cwd);
    }
    return best ? std::optional<std::filesystem::path>{ best->path }
                : std::nullopt;
}

void appendTemplateNames(QStringList& names, const std::vector<ThingTemplate>& templates)
{
    for (const ThingTemplate& tpl : templates)
    {
        const QString name = QString::fromStdString(tpl.name).trimmed();
        if (!name.isEmpty())
        {
            names.push_back(name);
        }
    }
}

void appendTemplateNames(QStringList& names, const assets::Templates& templates)
{
    for (std::size_t i = 0; i < templates.size(); ++i)
    {
        if (const ThingTemplate* tpl = templates.get(i))
        {
            const QString name = QString::fromStdString(tpl->name).trimmed();
            if (!name.isEmpty())
            {
                names.push_back(name);
            }
        }
    }
}

QStringList normalizedTemplatePickerNames(QStringList names)
{
    // Step 1: preserve legacy duplicate handling: exact duplicate strings are removed.
    names.removeDuplicates();

    // Step 2: sort case-insensitively so the picker is stable across local/base sources.
    std::sort(names.begin(), names.end(), [](const QString& a, const QString& b)
        {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });

    // Step 3: keep the "none" sentinel available even when no template database exists.
    if (!names.contains(QStringLiteral("none"), Qt::CaseInsensitive))
    {
        names.push_front(QStringLiteral("none"));
    }
    return names;
}

QStringList templatePickerNames(
    const std::vector<ThingTemplate>& localTemplates,
    const assets::Templates* baseTemplates)
{
    QStringList names{};
    appendTemplateNames(names, localTemplates);
    if (baseTemplates)
    {
        appendTemplateNames(names, *baseTemplates);
    }
    return normalizedTemplatePickerNames(names);
}

void appendTemplateCategoryEntriesInSourceOrder(
    QStringList& names,
    const assets::JlhNode& node)
{
    auto appendUnique = [&names](const QString& value)
    {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !names.contains(trimmed, Qt::CaseInsensitive))
        {
            names.push_back(trimmed);
        }
    };

    if (node.list)
    {
        for (const std::string& entry : node.list->entries)
        {
            appendUnique(QString::fromStdString(entry));
        }
    }
    for (const assets::JlhNode& child : node.children)
    {
        appendTemplateCategoryEntriesInSourceOrder(names, child);
    }
}

} // namespace sed::ui
