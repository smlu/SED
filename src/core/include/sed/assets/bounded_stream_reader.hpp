#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <istream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sed::assets::detail
{

/** Captures a complete stream when its byte length does not exceed maxBytes.
 *
 * sizeHint is only a reserve hint; it never limits how many source bytes are
 * read. The reader consumes at most maxBytes + 1 bytes, with the extra byte
 * used only to distinguish an exact-budget stream from an oversized one.
 */
template <typename Buffer>
std::optional<Buffer> readBinaryStreamWithinBudget(
    std::istream& input,
    const std::size_t maxBytes,
    const std::size_t sizeHint,
    bool& exceededBudget,
    const std::string_view readFailureMessage)
{
    exceededBudget = false;
    Buffer bytes{};
    constexpr std::size_t kReadChunkBytes = 64u * 1024u;
    constexpr std::size_t kInitialReserveBytes = kReadChunkBytes;
    bytes.reserve(std::min({ sizeHint, maxBytes, kInitialReserveBytes }));

    std::array<char, kReadChunkBytes> chunk{};
    while ( true )
    {
        const std::size_t remainingBudget = maxBytes - bytes.size();
        if ( remainingBudget == 0 )
        {
            char probe{};
            input.read(&probe, 1);
            const std::streamsize count = input.gcount();
            if ( input.bad() )
            {
                throw std::runtime_error(std::string(readFailureMessage));
            }
            if ( count > 0 )
            {
                exceededBudget = true;
                return std::nullopt;
            }
            if ( input.eof() )
            {
                return bytes;
            }
            throw std::runtime_error(std::string(readFailureMessage));
        }

        const std::size_t requestBytes = std::min(chunk.size(), remainingBudget);
        input.read(chunk.data(), static_cast<std::streamsize>(requestBytes));
        const std::streamsize count = input.gcount();
        if ( input.bad() )
        {
            throw std::runtime_error(std::string(readFailureMessage));
        }
        if ( count < 0 || static_cast<std::size_t>(count) > requestBytes )
        {
            throw std::runtime_error(std::string(readFailureMessage));
        }
        if ( count > 0 )
        {
            const std::size_t oldSize = bytes.size();
            const std::size_t bytesRead = static_cast<std::size_t>(count);
            bytes.resize(oldSize + bytesRead);
            std::memcpy(bytes.data() + oldSize, chunk.data(), bytesRead);
        }
        if ( input.eof() )
        {
            return bytes;
        }
        if ( input.fail() || count == 0 )
        {
            throw std::runtime_error(std::string(readFailureMessage));
        }
    }
}

} // namespace sed::assets::detail
