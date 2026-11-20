#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace sed::plugins
{

enum class PluginBinaryArchitecture
{
    unknown,
    x86,
    x64,
    arm64,
};

/**
 * Reads the machine type of a Windows PE image (DLL or EXE) from its leading
 * bytes. Returns `unknown` for non-PE data, truncated headers, and machines
 * SED does not ship for. Only the first few kilobytes are needed.
 */
PluginBinaryArchitecture detectPeArchitecture(std::span<const std::byte> image) noexcept;

/** Designer-facing name, e.g. "32-bit x86". */
const char* pluginArchitectureName(PluginBinaryArchitecture architecture) noexcept;

/**
 * File name of the out-of-process plug-in bridge built for `architecture`,
 * e.g. "sed-jed-plugin-bridge-x86.exe". Empty for `unknown`.
 */
std::string pluginBridgeExecutableName(PluginBinaryArchitecture architecture, bool windows);

} // namespace sed::plugins
