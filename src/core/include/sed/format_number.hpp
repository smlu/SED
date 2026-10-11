#pragma once

#include <array>
#include <charconv>
#include <stdexcept>
#include <string>
#include <system_error>

namespace sed::format
{

// Locale-independent shortest spelling that parses back to the authored double.
// This is persistence/editing text; fixed decimal presentation belongs in the UI.
inline std::string authoredDouble(const double value)
{
    std::array<char, 64> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
        value, std::chars_format::general);
    if (result.ec != std::errc{})
        throw std::runtime_error("Failed to format an authored floating-point value");
    return std::string(buffer.data(), result.ptr);
}

} // namespace sed::format
