#pragma once

#include <filesystem>
#include <istream>
#include <string>
#include <vector>

namespace sed::assets
{

struct ModelsDatFile
{
    std::vector<std::string> snds;
    std::vector<std::string> a3dos;
};

ModelsDatFile loadFromModelsDat(std::istream& in);
ModelsDatFile loadFromModelsDat(const std::filesystem::path& path);

} // namespace sed::assets
