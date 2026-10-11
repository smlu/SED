#pragma once

#include <filesystem>
#include <istream>
#include <string>
#include <vector>

namespace sed::assets
{

struct PupFile
{
    std::vector<std::string> keys;
};

PupFile loadFromPUP(std::istream& in);
PupFile loadFromPUP(const std::filesystem::path& path);

} // namespace sed::assets
