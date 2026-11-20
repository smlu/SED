#include "sed/plugins/plugin_binary.hpp"

#include <cstdint>

namespace sed::plugins
{
namespace
{

constexpr std::size_t kPeOffsetField = 0x3C;
constexpr std::uint32_t kPeSignature = 0x00004550; // "PE\0\0"
constexpr std::uint16_t kMachineI386 = 0x014C;
constexpr std::uint16_t kMachineAmd64 = 0x8664;
constexpr std::uint16_t kMachineArm64 = 0xAA64;

std::uint32_t readLittleEndian(std::span<const std::byte> bytes, const std::size_t at, const std::size_t width) noexcept
{
    std::uint32_t value = 0;
    for ( std::size_t i = 0; i < width; ++i )
    {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[at + i])) << (8 * i);
    }
    return value;
}

} // namespace

PluginBinaryArchitecture detectPeArchitecture(const std::span<const std::byte> image) noexcept
{
    if ( image.size() < kPeOffsetField + 4 || readLittleEndian(image, 0, 2) != 0x5A4D )
    {
        return PluginBinaryArchitecture::unknown;
    }
    const std::size_t peOffset = readLittleEndian(image, kPeOffsetField, 4);
    // Signature (4) and machine (2); compare without forming peOffset + 6.
    if ( peOffset > image.size() || image.size() - peOffset < 6
        || readLittleEndian(image, peOffset, 4) != kPeSignature )
    {
        return PluginBinaryArchitecture::unknown;
    }
    switch ( readLittleEndian(image, peOffset + 4, 2) )
    {
    case kMachineI386:
        return PluginBinaryArchitecture::x86;
    case kMachineAmd64:
        return PluginBinaryArchitecture::x64;
    case kMachineArm64:
        return PluginBinaryArchitecture::arm64;
    default:
        return PluginBinaryArchitecture::unknown;
    }
}

const char* pluginArchitectureName(const PluginBinaryArchitecture architecture) noexcept
{
    switch ( architecture )
    {
    case PluginBinaryArchitecture::x86:
        return "32-bit x86";
    case PluginBinaryArchitecture::x64:
        return "64-bit x64";
    case PluginBinaryArchitecture::arm64:
        return "64-bit ARM64";
    case PluginBinaryArchitecture::unknown:
        break;
    }
    return "unknown";
}

std::string pluginBridgeExecutableName(const PluginBinaryArchitecture architecture, const bool windows)
{
    const char* token = nullptr;
    switch ( architecture )
    {
    case PluginBinaryArchitecture::x86:
        token = "x86";
        break;
    case PluginBinaryArchitecture::x64:
        token = "x64";
        break;
    case PluginBinaryArchitecture::arm64:
        token = "arm64";
        break;
    case PluginBinaryArchitecture::unknown:
        return {};
    }
    return std::string("sed-jed-plugin-bridge-") + token + (windows ? ".exe" : "");
}

} // namespace sed::plugins
