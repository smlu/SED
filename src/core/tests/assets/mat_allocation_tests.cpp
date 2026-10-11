#include "test_common.hpp"

#include "sed/assets/mat.hpp"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace
{
thread_local bool guardLargeAllocations{};
thread_local std::size_t refusedAllocationBytes{};
constexpr std::size_t kFixtureAllocationBudget = 64u * 1024u;

void* allocate(const std::size_t bytes)
{
    if ( guardLargeAllocations && bytes > kFixtureAllocationBudget )
    {
        refusedAllocationBytes = bytes;
        throw std::bad_alloc();
    }
    if ( void* const memory = std::malloc(bytes == 0u ? 1u : bytes) )
    {
        return memory;
    }
    throw std::bad_alloc();
}
} // namespace

void* operator new(std::size_t bytes) { return allocate(bytes); }
void* operator new[](std::size_t bytes) { return allocate(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace
{
using sed::tests::check;

void appendU32(std::vector<std::byte>& bytes, const std::uint32_t value)
{
    for ( unsigned shift = 0u; shift < 32u; shift += 8u )
    {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffu));
    }
}

std::vector<std::byte> header(const std::uint32_t celCount)
{
    // Synthetic classic indexed texture MAT, using its public 76-byte wire layout.
    std::vector<std::byte> bytes{ std::byte{'M'}, std::byte{'A'}, std::byte{'T'}, std::byte{' '} };
    appendU32(bytes, 0x32u);
    appendU32(bytes, 2u);
    appendU32(bytes, celCount);
    appendU32(bytes, celCount);
    appendU32(bytes, static_cast<std::uint32_t>(sed::ColorMode::Indexed));
    appendU32(bytes, 8u);
    for ( int field = 0; field < 12; ++field )
    {
        appendU32(bytes, 0u);
    }
    return bytes;
}

void appendTextureHeader(std::vector<std::byte>& bytes)
{
    appendU32(bytes, 8u);
    appendU32(bytes, 37u);
    for ( int field = 0; field < 8; ++field )
    {
        appendU32(bytes, 0u);
    }
}

void appendTextureData(std::vector<std::byte>& bytes,
    const std::uint32_t width, const std::uint32_t height,
    const std::uint32_t mipmaps)
{
    appendU32(bytes, width);
    appendU32(bytes, height);
    appendU32(bytes, 0u);
    appendU32(bytes, 0u);
    appendU32(bytes, 0u);
    appendU32(bytes, mipmaps);
}

void checkValidCountBoundaries()
{
    auto bytes = header(2u);
    appendTextureHeader(bytes);
    appendTextureHeader(bytes);
    appendTextureData(bytes, 4u, 4u, 3u);
    bytes.insert(bytes.end(), 16u, std::byte{17});
    bytes.insert(bytes.end(), 4u, std::byte{29});
    bytes.push_back(std::byte{43});
    appendTextureData(bytes, 2u, 1u, 1u);
    bytes.push_back(std::byte{61});
    bytes.push_back(std::byte{79});
    const auto mat = sed::assets::MatFile::parse(std::move(bytes));
    check(mat.celCount() == 2 && mat.isAnimated()
            && mat.cel(0).mipmaps.size() == 3u
            && mat.cel(0).mipmaps[0].width == 4
            && mat.cel(0).mipmaps[1].width == 2
            && mat.cel(0).mipmaps[2].width == 1
            && mat.cel(0).mipmaps[2].pixels == std::vector<std::uint8_t>{
                43u, 43u, 43u }
            && mat.cel(1).mipmaps.size() == 1u
            && mat.cel(1).width == 2 && mat.cel(1).height == 1
            && mat.cel(1).mipmaps[0].pixels == std::vector<std::uint8_t>{
                61u, 61u, 61u, 79u, 79u, 79u },
        "MAT count admission preserves exact animated, full-chain and nonsquare indexed texture pixels");
}

void checkRejectedBeforeAllocation(const bool mipmapCount)
{
    const auto impossibleCount = static_cast<std::uint32_t>(
        std::numeric_limits<std::int32_t>::max());
    auto bytes = header(mipmapCount ? 1u : impossibleCount);
    if ( mipmapCount )
    {
        appendTextureHeader(bytes);
        appendTextureData(bytes, 2u, 2u, impossibleCount);
        bytes.insert(bytes.end(), 4u, std::byte{17});
    }
    bool validationRejected = false;
    bool allocationRejected = false;
    refusedAllocationBytes = 0u;
    guardLargeAllocations = true;
    try
    {
        (void)sed::assets::MatFile::parse(std::move(bytes));
    }
    catch ( const std::bad_alloc& )
    {
        allocationRejected = true;
    }
    catch ( const std::runtime_error& )
    {
        validationRejected = true;
    }
    guardLargeAllocations = false;
    if ( refusedAllocationBytes != 0u )
    {
        std::fprintf(stderr, "MAT %s count requested %zu bytes before rejecting its tiny input\n",
            mipmapCount ? "mipmap" : "cel", refusedAllocationBytes);
    }
    check(validationRejected && !allocationRejected && refusedAllocationBytes == 0u,
        mipmapCount
            ? "MAT rejects a mipmap count impossible for its dimensions and remaining bytes before reserving metadata"
            : "MAT rejects a cel count impossible for its remaining bytes before reserving metadata");
}
} // namespace

int main(const int argc, char** argv)
{
    if ( argc != 2 )
    {
        return 2;
    }
    const std::string_view mode(argv[1]);
    if ( mode != "cel-count" && mode != "mipmap-count" )
    {
        return 2;
    }
    checkValidCountBoundaries();
    checkRejectedBeforeAllocation(mode == "mipmap-count");
    return sed::tests::failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
