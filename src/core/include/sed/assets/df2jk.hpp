#pragma once

#include <filesystem>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sed::assets
{

using Df2JkConvTable = std::unordered_map<std::string, std::string>;

Df2JkConvTable loadDf2JkConversionTable(std::istream& in);
Df2JkConvTable loadDf2JkConversionTable(const std::filesystem::path& path);

std::optional<std::string_view> lookupDf2Jk(const Df2JkConvTable& table, const std::string_view dfLogic) noexcept;

} // namespace sed::assets
