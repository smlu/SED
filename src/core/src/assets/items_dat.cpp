#include "sed/assets/items_dat.hpp"

#include "sed/text.hpp"

#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sed::assets
{
namespace
{

using sed::text::splitWords;
using sed::text::stripLineComments;
using sed::text::trimLeft;

} // namespace

ItemsDatFile loadFromItemsDat(std::istream& in)
{
    ItemsDatFile file{};

    std::string line;
    while (std::getline(in, line))
    {
        stripLineComments(line);

        std::size_t pos = line.find("cog=");
        while (pos != std::string::npos
            && pos > 0
            && !std::isspace(static_cast<unsigned char>(line[pos - 1])))
        {
            pos = line.find("cog=", pos + 4);
        }
        if (pos == std::string::npos)
        {
            continue;
        }

        std::string_view remaining = std::string_view(line).substr(pos + 4);
        remaining = trimLeft(remaining);

        const auto words = splitWords(remaining);
        if (!words.empty())
        {
            file.cogs.emplace_back(words[0]);
        }
    }

    if (in.bad() || (in.fail() && !in.eof()))
    {
        throw std::runtime_error("Failed to read ITEMS.DAT.");
    }

    return file;
}

ItemsDatFile loadFromItemsDat(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("Failed to open ITEMS.DAT: " + path.string());
    }

    return loadFromItemsDat(in);
}

} // namespace sed::assets
