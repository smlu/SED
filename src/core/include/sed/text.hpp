#pragma once

#include <cctype>
#include <charconv>
#include <cstdint>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace sed::text
{

inline std::string_view trimLeft(const std::string_view s) noexcept
{
    auto out = s;
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.front())))
    {
        out.remove_prefix(1);
    }
    return out;
}

inline std::string_view trimRight(const std::string_view s) noexcept
{
    auto out = s;
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back())))
    {
        out.remove_suffix(1);
    }
    return out;
}

inline std::string_view trimView(const std::string_view s) noexcept
{
    return trimLeft(trimRight(s));
}

inline std::string trim(const std::string_view s)
{
    return std::string(trimView(s));
}

inline void stripHashComment(std::string& line)
{
    const std::size_t pos = line.find('#');
    if (pos != std::string::npos)
    {
        line.erase(pos);
    }
}

inline void stripLineComments(std::string& line)
{
    std::size_t cut = std::string::npos;

    auto updateCut = [&](const std::size_t pos) noexcept
    {
        if (pos == std::string::npos)
        {
            return;
        }
        if (cut == std::string::npos || pos < cut)
        {
            cut = pos;
        }
    };

    updateCut(line.find('#'));
    updateCut(line.find(';'));
    updateCut(line.find("//"));

    if (cut != std::string::npos)
    {
        line.erase(cut);
    }
}

inline bool iequalsAscii(const std::string_view a, const std::string_view b) noexcept
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (std::toupper(static_cast<unsigned char>(a[i])) != std::toupper(static_cast<unsigned char>(b[i])))
        {
            return false;
        }
    }
    return true;
}

inline bool istartsWithAscii(const std::string_view s, const std::string_view prefix) noexcept
{
    if (s.size() < prefix.size())
    {
        return false;
    }
    return iequalsAscii(s.substr(0, prefix.size()), prefix);
}

inline bool iendsWithAscii(const std::string_view s, const std::string_view suffix) noexcept
{
    if (suffix.empty())
    {
        return true;
    }
    if (s.size() < suffix.size())
    {
        return false;
    }
    return iequalsAscii(s.substr(s.size() - suffix.size()), suffix);
}

inline std::string upperAscii(const std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (const char ch : s)
    {
        out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    return out;
}

inline std::string lowerAscii(const std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    for (const char ch : s)
    {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

inline std::vector<std::string_view> splitWords(const std::string_view text)
{
    std::vector<std::string_view> out;
    auto remaining = trimLeft(text);
    while (!remaining.empty())
    {
        std::size_t i = 0;
        while (i < remaining.size() && !std::isspace(static_cast<unsigned char>(remaining[i])))
        {
            ++i;
        }
        out.emplace_back(remaining.substr(0, i));
        remaining.remove_prefix(i);
        remaining = trimLeft(remaining);
    }
    return out;
}

template <typename IntT>
inline std::optional<IntT> parseInt(const std::string_view token, const int base = 10) noexcept
{
    IntT value{};
    const char* first = token.data();
    const char* last = token.data() + token.size();
    const auto result = std::from_chars(first, last, value, base);
    if (result.ec != std::errc{} || result.ptr != last)
    {
        return std::nullopt;
    }
    return value;
}

inline std::optional<std::uint32_t> parseHex(const std::string_view token) noexcept
{
    auto t = token;
    if (t.size() >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X'))
    {
        t.remove_prefix(2);
    }
    return parseInt<std::uint32_t>(t, 16);
}

inline std::optional<double> parseDouble(const std::string_view token)
{
    // std::from_chars(float) isn't consistently implemented across toolchains; use stringstream for now.
    std::string tmp(token);
    std::istringstream iss(tmp);
    // Numeric extraction reports allocation failure through badbit unless its
    // original exception is enabled. Malformed numbers still return nullopt
    // through ordinary failbit; resource failure must not masquerade as syntax.
    iss.exceptions(std::ios::badbit);
    iss.imbue(std::locale::classic());

    double v{};
    iss >> v;
    if (!iss || !iss.eof())
    {
        return std::nullopt;
    }
    return v;
}

} // namespace sed::text
