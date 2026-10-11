#include "sed/assets/df2jk.hpp"

#include "sed/text.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace sed::assets
{

Df2JkConvTable loadDf2JkConversionTable(std::istream& in)
{
    Df2JkConvTable out{};

    std::string line{};
    while (std::getline(in, line))
    {
        sed::text::stripHashComment(line);
        const std::string_view trimmed = sed::text::trimView(line);
        if (trimmed.empty())
        {
            continue;
        }

        const std::vector<std::string_view> words = sed::text::splitWords(trimmed);
        if (words.size() < 2)
        {
            continue;
        }

        out[sed::text::upperAscii(words[0])] = std::string(words[1]);
    }

    return out;
}

Df2JkConvTable loadDf2JkConversionTable(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Failed to open df2jk conversion table: " + path.string());
    }
    return loadDf2JkConversionTable(file);
}

std::optional<std::string_view> lookupDf2Jk(const Df2JkConvTable& table, const std::string_view dfLogic) noexcept
{
    const std::string key = sed::text::upperAscii(dfLogic);

    const auto it = table.find(key);
    if (it == table.end())
    {
        return std::nullopt;
    }

    return it->second;
}

} // namespace sed::assets
