#include "sed/assets/pup.hpp"

#include "sed/text.hpp"

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
using sed::text::trim;

bool hasKeyExtension(const std::string_view name) noexcept
{
    return name.size() >= 4
        && sed::text::iequalsAscii(name.substr(name.size() - 4), ".KEY");
}

} // namespace

PupFile loadFromPUP(std::istream& in)
{
    PupFile file{};

    std::string line;
    while (std::getline(in, line))
    {
        stripLineComments(line);
        const std::string trimmed = trim(line);
        if (trimmed.empty())
        {
            continue;
        }

        const auto words = splitWords(trimmed);
        if (words.size() < 2)
        {
            continue;
        }

        const std::string_view key = words[1];
        if (!hasKeyExtension(key))
        {
            continue;
        }

        file.keys.emplace_back(key);
    }

    if (in.bad() || (in.fail() && !in.eof()))
    {
        throw std::runtime_error("Failed to read PUP.");
    }

    return file;
}

PupFile loadFromPUP(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("Failed to open PUP: " + path.string());
    }

    return loadFromPUP(in);
}

} // namespace sed::assets
