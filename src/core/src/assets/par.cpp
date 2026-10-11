#include "sed/assets/par.hpp"

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

std::string matFromPAR(std::istream& in)
{
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
        if (words.size() >= 2 && words[0] == "MATERIAL")
        {
            return std::string(words[1]);
        }
    }

    return {};
}

std::string matFromPAR(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("Failed to open PAR: " + path.string());
    }

    return matFromPAR(in);
}

} // namespace sed::assets
