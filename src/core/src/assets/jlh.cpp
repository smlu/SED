#include "sed/assets/jlh.hpp"

#include "sed/assets/resource_locator.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace sed::assets
{

namespace
{

struct WordScan
{
    std::string_view word{};
    std::size_t next{};
};

WordScan scanWord(const std::string_view text, const std::size_t start) noexcept
{
    std::size_t b = start;
    while (b < text.size() && std::isspace(static_cast<unsigned char>(text[b])))
    {
        ++b;
    }

    std::size_t e = b;
    while (e < text.size() && !std::isspace(static_cast<unsigned char>(text[e])))
    {
        ++e;
    }

    return WordScan{ text.substr(b, e - b), e };
}

JlhNode* nodeAt(JlhTree& tree, const std::vector<std::size_t>& path) noexcept
{
    std::vector<JlhNode>* nodes = &tree.roots;
    JlhNode* cur = nullptr;
    for (const std::size_t idx : path)
    {
        if (idx >= nodes->size())
        {
            return nullptr;
        }
        cur = &(*nodes)[idx];
        nodes = &cur->children;
    }
    return cur;
}

std::vector<JlhNode>& currentChildren(JlhTree& tree, const std::vector<std::size_t>& path) noexcept
{
    if (path.empty())
    {
        return tree.roots;
    }

    if (JlhNode* cur = nodeAt(tree, path))
    {
        return cur->children;
    }

    return tree.roots;
}

void collectLists(
    const std::vector<JlhNode>& nodes,
    std::vector<JllListPtr>& out,
    std::set<std::string>& seen)
{
    for (const JlhNode& node : nodes)
    {
        if (node.list)
        {
            const std::string key = sed::text::upperAscii(node.list->name);
            if (seen.insert(key).second)
            {
                out.push_back(node.list);
            }
        }
        if (!node.children.empty())
        {
            collectLists(node.children, out, seen);
        }
    }
}

void writeNodes(std::ostream& out, const std::vector<JlhNode>& nodes, const int depth = 0)
{
    const std::string indent(static_cast<std::size_t>(std::max(0, depth)) * 2u, ' ');
    for (const JlhNode& node : nodes)
    {
        if (!node.list)
        {
            out << indent << "SUB " << node.label << '\n';
            writeNodes(out, node.children, depth + 1);
            out << indent << "SUBEND\n";
            continue;
        }

        if (node.list)
        {
            out << indent << "LIST " << node.list->name;
            if (!node.label.empty())
            {
                out << ' ' << node.label;
            }
            out << '\n';
        }
    }
}

std::filesystem::file_status checkedSymlinkStatus(const std::filesystem::path& path)
{
    std::error_code ec{};
    const std::filesystem::file_status status = std::filesystem::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory)
    {
        return std::filesystem::file_status{ std::filesystem::file_type::not_found };
    }
    if (ec)
    {
        throw std::runtime_error("Failed to inspect category file '" + path.string() + "': " + ec.message());
    }
    return status;
}

bool validateCategoryTarget(const std::filesystem::path& path)
{
    const std::filesystem::file_status status = checkedSymlinkStatus(path);
    if (!std::filesystem::exists(status))
    {
        return false;
    }

    // A directory, device, or symlink is never a valid category file. Reject it
    // before staging so a typo cannot be renamed out of the way and replaced.
    if (status.type() != std::filesystem::file_type::regular)
    {
        throw std::runtime_error("Category save target is not a regular file: " + path.string());
    }
    return true;
}

std::filesystem::path createTransactionDirectory(const std::filesystem::path& jlhPath)
{
    static std::atomic<std::uint64_t> nextId{ 0 };
    const auto timestamp = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path parent = jlhPath.parent_path();

    for (std::uint64_t attempt = 0; attempt < 128; ++attempt)
    {
        std::filesystem::path name = jlhPath.filename();
        name += ".sed-pair-save-";
        name += std::to_string(timestamp);
        name += "-";
        name += std::to_string(nextId.fetch_add(1, std::memory_order_relaxed) + attempt);
        const std::filesystem::path candidate = parent / name;

        std::error_code ec{};
        if (std::filesystem::create_directory(candidate, ec))
        {
            return candidate;
        }
        if (ec && ec != std::errc::file_exists)
        {
            throw std::runtime_error(
                "Failed to create category save transaction directory '" + candidate.string() + "': "
                + ec.message());
        }
    }

    throw std::runtime_error("Failed to allocate a unique category save transaction next to: " + jlhPath.string());
}

void writeChecked(
    const std::filesystem::path& path,
    const std::function<void(std::ostream&)>& writer)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out)
    {
        throw std::runtime_error("Failed to open staged category file for writing: " + path.string());
    }

    writer(out);
    out.flush();
    if (!out)
    {
        throw std::runtime_error("Failed while writing staged category file: " + path.string());
    }

    out.close();
    if (!out)
    {
        throw std::runtime_error("Failed to close staged category file: " + path.string());
    }
}

void renameChecked(
    const std::filesystem::path& from,
    const std::filesystem::path& to,
    const std::string_view operation)
{
    std::error_code ec{};
    std::filesystem::rename(from, to, ec);
    if (ec)
    {
        throw std::runtime_error(
            std::string(operation) + " ('" + from.string() + "' -> '" + to.string() + "'): "
            + ec.message());
    }
}

void removeChecked(const std::filesystem::path& path, const std::string_view operation)
{
    std::error_code ec{};
    const bool removed = std::filesystem::remove(path, ec);
    if (ec || !removed)
    {
        throw std::runtime_error(
            std::string(operation) + " ('" + path.string() + "'): "
            + (ec ? ec.message() : std::string("path was not removed")));
    }
}

struct PairCommitState
{
    bool jlhBackedUp{};
    bool jllBackedUp{};
    bool jlhInstalled{};
    bool jllInstalled{};
};

void rollBackPair(
    const std::filesystem::path& jlhPath,
    const std::filesystem::path& jllPath,
    const std::filesystem::path& jlhBackup,
    const std::filesystem::path& jllBackup,
    const PairCommitState& state)
{
    // Undo in reverse installation order. An absent original is restored by
    // removing the newly installed file; an existing original is renamed back
    // from the transaction directory.
    if (state.jlhInstalled)
    {
        removeChecked(jlhPath, "Failed to remove partial JLH replacement during rollback");
    }
    if (state.jlhBackedUp)
    {
        renameChecked(jlhBackup, jlhPath, "Failed to restore original JLH during rollback");
    }

    if (state.jllInstalled)
    {
        removeChecked(jllPath, "Failed to remove partial JLL replacement during rollback");
    }
    if (state.jllBackedUp)
    {
        renameChecked(jllBackup, jllPath, "Failed to restore original JLL during rollback");
    }
}

} // namespace

std::unordered_map<std::string, JllListPtr> parseJLL(std::istream& in)
{
    std::unordered_map<std::string, JllListPtr> out{};

    std::shared_ptr<JllList> current{};
    std::string line{};

    while (std::getline(in, line))
    {
        sed::text::stripHashComment(line);
        const std::string_view trimmed = sed::text::trimView(line);
        if (trimmed.empty())
        {
            continue;
        }

        if (trimmed.front() == '[')
        {
            std::size_t close = trimmed.find(']');
            if (close == std::string_view::npos)
            {
                close = trimmed.size();
            }

            const std::string name = sed::text::trim(trimmed.substr(1, close - 1));
            current = std::make_shared<JllList>();
            current->name = name;

            out[sed::text::upperAscii(name)] = current;
            continue;
        }

        if (!current)
        {
            continue;
        }

        current->entries.emplace_back(sed::text::trim(trimmed));
    }

    return out;
}

JlhTree parseJLH(std::istream& in, const std::unordered_map<std::string, JllListPtr>& lists)
{
    JlhTree out{};
    std::vector<std::size_t> path{};

    std::string line{};

    while (std::getline(in, line))
    {
        sed::text::stripHashComment(line);
        const std::string_view trimmed = sed::text::trimView(line);
        if (trimmed.empty())
        {
            continue;
        }

        const WordScan cmd = scanWord(trimmed, 0);
        if (cmd.word.empty())
        {
            continue;
        }

        if (sed::text::iequalsAscii(cmd.word, "SUBEND"))
        {
            if (!path.empty())
            {
                path.pop_back();
            }
            continue;
        }

        if (sed::text::iequalsAscii(cmd.word, "SUB"))
        {
            const std::string label = sed::text::trim(trimmed.substr(cmd.next));
            if (label.empty())
            {
                continue;
            }

            std::vector<JlhNode>& siblings = currentChildren(out, path);
            siblings.push_back(JlhNode{ label, {}, {} });
            path.push_back(siblings.size() - 1);
            continue;
        }

        if (sed::text::iequalsAscii(cmd.word, "LIST"))
        {
            const WordScan key = scanWord(trimmed, cmd.next);
            if (key.word.empty())
            {
                continue;
            }

            const std::string listKeyUpper = sed::text::upperAscii(key.word);
            const auto it = lists.find(listKeyUpper);
            if (it == lists.end())
            {
                continue;
            }

            std::string label = sed::text::trim(trimmed.substr(key.next));
            if (label.empty())
            {
                label = std::string(key.word);
            }

            std::vector<JlhNode>& siblings = currentChildren(out, path);
            siblings.push_back(JlhNode{ std::move(label), it->second, {} });
            continue;
        }
    }

    return out;
}

JlhTree loadFromJLH(std::istream& jlh, std::istream& jll)
{
    auto lists = parseJLL(jll);
    return parseJLH(jlh, lists);
}

JlhTree loadFromJLH(const std::filesystem::path& jlhPath)
{
    std::ifstream jlhFile(jlhPath);
    if (!jlhFile)
    {
        throw std::runtime_error("Failed to open JLH file: " + jlhPath.string());
    }

    std::filesystem::path jllPath = jlhPath;
    jllPath.replace_extension(".jll");

    std::ifstream jllFile(jllPath);
    if (!jllFile)
    {
        throw std::runtime_error("Failed to open JLL file: " + jllPath.string());
    }

    return loadFromJLH(jlhFile, jllFile);
}

JlhTree loadFromJLH(const std::string_view jlhName, const ResourceLocator& resources)
{
    const std::string jlhText = resources.readText(jlhName);

    std::filesystem::path jllName{ std::string(jlhName) };
    jllName.replace_extension(".jll");
    const std::string jllText = resources.readText(jllName.generic_string());

    std::istringstream jlhStream(jlhText);
    std::istringstream jllStream(jllText);

    return loadFromJLH(jlhStream, jllStream);
}

void writeJLL(std::ostream& out, const JlhTree& tree)
{
    std::vector<JllListPtr> lists{};
    std::set<std::string> seen{};
    collectLists(tree.roots, lists, seen);

    for (std::size_t i = 0; i < lists.size(); ++i)
    {
        const JllListPtr& list = lists[i];
        if (!list)
        {
            continue;
        }

        out << '[' << list->name << "]\n";
        for (const std::string& entry : list->entries)
        {
            out << entry << '\n';
        }
        if (i + 1 < lists.size())
        {
            out << '\n';
        }
    }
}

void writeJLH(std::ostream& out, const JlhTree& tree)
{
    writeNodes(out, tree.roots, 0);
}

void saveToJLH(const JlhTree& tree, const std::filesystem::path& jlhPath)
{
    if (jlhPath.empty())
    {
        throw std::runtime_error("Cannot save categories to an empty JLH path");
    }

    std::filesystem::path jllPath = jlhPath;
    jllPath.replace_extension(".jll");
    if (jlhPath.lexically_normal() == jllPath.lexically_normal())
    {
        throw std::runtime_error("JLH and JLL category paths resolve to the same file: " + jlhPath.string());
    }

    const bool hadJlh = validateCategoryTarget(jlhPath);
    const bool hadJll = validateCategoryTarget(jllPath);

    const std::filesystem::path transactionDir = createTransactionDirectory(jlhPath);
    const std::filesystem::path stagedJlh = transactionDir / "new.jlh";
    const std::filesystem::path stagedJll = transactionDir / "new.jll";
    const std::filesystem::path backupJlh = transactionDir / "original.jlh";
    const std::filesystem::path backupJll = transactionDir / "original.jll";

    try
    {
        // Complete and check both writes before touching either original.
        writeChecked(stagedJlh, [&tree](std::ostream& out) { writeJLH(out, tree); });
        writeChecked(stagedJll, [&tree](std::ostream& out) { writeJLL(out, tree); });
    }
    catch (...)
    {
        std::error_code cleanupError{};
        std::filesystem::remove_all(transactionDir, cleanupError);
        throw;
    }

    PairCommitState state{};
    try
    {
        if (hadJlh)
        {
            renameChecked(jlhPath, backupJlh, "Failed to back up original JLH");
            state.jlhBackedUp = true;
        }
        if (hadJll)
        {
            renameChecked(jllPath, backupJll, "Failed to back up original JLL");
            state.jllBackedUp = true;
        }

        // Install JLL first: until the JLH replacement lands, readers cannot
        // discover newly introduced list names from the hierarchy file.
        renameChecked(stagedJll, jllPath, "Failed to install staged JLL");
        state.jllInstalled = true;
        renameChecked(stagedJlh, jlhPath, "Failed to install staged JLH");
        state.jlhInstalled = true;
    }
    catch (const std::exception& saveError)
    {
        try
        {
            rollBackPair(jlhPath, jllPath, backupJlh, backupJll, state);
        }
        catch (const std::exception& rollbackError)
        {
            // Keep the transaction directory: it contains any original that
            // could not be restored and is the manual recovery source.
            throw std::runtime_error(
                std::string(saveError.what()) + "; rollback also failed: " + rollbackError.what()
                + "; recovery files remain in " + transactionDir.string());
        }

        std::error_code cleanupError{};
        std::filesystem::remove_all(transactionDir, cleanupError);
        throw std::runtime_error(std::string(saveError.what()) + "; original category files were restored");
    }

    // The new pair is complete. Failure to remove obsolete backup artifacts
    // does not invalidate the installed files.
    std::error_code cleanupError{};
    std::filesystem::remove_all(transactionDir, cleanupError);
}

} // namespace sed::assets
