#pragma once

#include <filesystem>
#include <istream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sed::assets
{

class ResourceLocator;

struct JllList
{
    std::string name{};
    std::vector<std::string> entries{};
};

using JllListPtr = std::shared_ptr<const JllList>;

struct JlhNode
{
    std::string label{};
    JllListPtr list{};
    std::vector<JlhNode> children{};
};

struct JlhTree
{
    std::vector<JlhNode> roots{};
};

std::unordered_map<std::string, JllListPtr> parseJLL(std::istream& in);
JlhTree parseJLH(std::istream& in, const std::unordered_map<std::string, JllListPtr>& lists);

JlhTree loadFromJLH(std::istream& jlh, std::istream& jll);
JlhTree loadFromJLH(const std::filesystem::path& jlhPath);
JlhTree loadFromJLH(const std::string_view jlhName, const ResourceLocator& resources);

void writeJLL(std::ostream& out, const JlhTree& tree);
void writeJLH(std::ostream& out, const JlhTree& tree);

/**
 * Saves the hierarchy and membership files as one checked pair transaction.
 * Both outputs are staged before commit; a failed replacement restores every
 * original that existed. If restoration itself fails, the exception names the
 * adjacent transaction directory that retains the recovery files.
 */
void saveToJLH(const JlhTree& tree, const std::filesystem::path& jlhPath);

} // namespace sed::assets
