#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <vector>

#include "sed/assets/sed.hpp"

namespace sed::assets::detail
{

std::vector<std::byte> compressSedLzssForTesting(
    std::span<const std::byte> input);
std::vector<std::byte> decompressSedLzssForTesting(
    std::span<const std::byte> input,
    std::size_t expandedSize);

} // namespace sed::assets::detail

extern "C" int LLVMFuzzerTestOneInput(
    const std::uint8_t* data,
    const std::size_t size)
{
    if ((data == nullptr && size != 0)
        || size > sed::assets::SedMaximumFileBytes)
    {
        return 0;
    }

    const auto bytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data), size);

    // Exercise the changed bounded matcher directly. Random bytes almost
    // never pass native framing/CRC gates, so decoder-only fuzzing otherwise
    // leaves the compressor's ring, offset, and match arithmetic untouched.
    const std::vector<std::byte> firstCompressed =
        sed::assets::detail::compressSedLzssForTesting(bytes);
    const std::vector<std::byte> secondCompressed =
        sed::assets::detail::compressSedLzssForTesting(bytes);
    if (firstCompressed != secondCompressed)
    {
        std::abort();
    }
    if (!firstCompressed.empty())
    {
        const std::vector<std::byte> expanded =
            sed::assets::detail::decompressSedLzssForTesting(
                firstCompressed, size);
        if (expanded.size() != bytes.size()
            || !std::equal(expanded.begin(), expanded.end(), bytes.begin()))
        {
            std::abort();
        }
    }

    // Feed arbitrary token bytes to the bounded decompressor as well. Limit the
    // direct token span so a maximum-size container corpus entry cannot turn
    // one iteration into redundant decoder work; the size word still explores
    // empty, exact, undersized, and oversized token outcomes.
    constexpr std::size_t kMaximumDirectTokenBytes = 64u * 1024u;
    const auto directTokens = bytes.first(std::min(bytes.size(),
        kMaximumDirectTokenBytes));
    std::uint32_t expandedSize{};
    for (std::size_t i = 0; i < std::min<std::size_t>(4, bytes.size()); ++i)
    {
        expandedSize |= static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>(bytes[i])) << (i * 8u);
    }
    expandedSize %= 1024u * 1024u + 1u;
    try
    {
        const std::vector<std::byte> arbitraryExpanded =
            sed::assets::detail::decompressSedLzssForTesting(
                directTokens, expandedSize);
        if (arbitraryExpanded.size() != expandedSize)
        {
            std::abort();
        }
    }
    catch (const std::runtime_error&)
    {
        // Malformed streams are expected. Allocation/length exceptions remain
        // visible to the fuzzing runtime.
    }

    try
    {
        (void)sed::assets::loadSedFileBytes(bytes);
    }
    catch (const std::runtime_error&)
    {
        // Format, integrity, and bounded-work rejection is expected.  Do not
        // catch allocation failures, length errors, or other unexpected
        // exception families: those are fuzzer findings inside the admitted
        // input limits and must terminate the iteration.
    }

    try
    {
        (void)sed::assets::recoverFromSedBytes(bytes);
    }
    catch (const std::runtime_error&)
    {
    }

    // This writer-side framing helper is public and deliberately skips the
    // semantic decode. Keep its footer/count/cursor arithmetic under the same
    // hostile-byte corpus as the full readers.
    try
    {
        (void)sed::assets::splitGeneratedSedFileSections(bytes);
    }
    catch (const std::runtime_error&)
    {
    }
    return 0;
}
