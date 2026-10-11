#pragma once

#include <filesystem>
#include <istream>
#include <string>

namespace sed::assets
{

std::string matFromPAR(std::istream& in);
std::string matFromPAR(const std::filesystem::path& path);

} // namespace sed::assets
