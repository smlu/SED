#pragma once

#include <filesystem>
#include <istream>
#include <string>
#include <vector>

namespace sed::assets
{

struct ItemsDatFile
{
    std::vector<std::string> cogs;
};

ItemsDatFile loadFromItemsDat(std::istream& in);
ItemsDatFile loadFromItemsDat(const std::filesystem::path& path);

} // namespace sed::assets
