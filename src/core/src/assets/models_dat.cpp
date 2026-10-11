#include "sed/assets/models_dat.hpp"

#include "sed/text.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace sed::assets
{
namespace
{

using sed::text::splitWords;
using sed::text::stripLineComments;
using sed::text::trim;

} // namespace

ModelsDatFile loadFromModelsDat(std::istream& in)
{
    ModelsDatFile file{};

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
        if (words.size() >= 2)
        {
            file.a3dos.emplace_back(words[1]);
        }
        if (words.size() >= 3)
        {
            file.snds.emplace_back(words[2]);
        }
    }

    if (in.bad() || (in.fail() && !in.eof()))
    {
        throw std::runtime_error("Failed to read MODELS.DAT.");
    }

    return file;
}

ModelsDatFile loadFromModelsDat(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("Failed to open MODELS.DAT: " + path.string());
    }

    return loadFromModelsDat(in);
}

} // namespace sed::assets
