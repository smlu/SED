
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "sed/assets/resource_locator.hpp"
#include "sed/values.hpp"

namespace sed::assets
{

enum class CogSymbolLoadStatus
{
    loaded,
    missing,
    malformed,
};

struct CogSymbolLoadResult
{
    CogSymbolLoadStatus status{ CogSymbolLoadStatus::missing };
    std::vector<CogValue> symbols{};
    std::string diagnostic{};

    bool loaded() const noexcept
    {
        return status == CogSymbolLoadStatus::loaded;
    }
};

/** Parses the symbols section and reports whether it was loaded, unavailable, or malformed. */
CogSymbolLoadResult loadCogSymbolsNoLocalsResult(const ResourceLocator& resources, std::string_view scriptName);

/** Parses symbols with optional locals/messages and reports an empty successful schema distinctly. */
CogSymbolLoadResult loadCogSymbolsWithDefaultsResult(
    const ResourceLocator& resources,
    std::string_view scriptName,
    bool includeLocals);

/** Parses captured .cog text without rereading the resource. */
CogSymbolLoadResult loadCogSymbolsFromTextResult(std::string_view text, bool includeLocals);

// Parses the `symbols` section of a .cog script and returns non-local, non-message symbols.
// This matches the original editor behavior for level assignments.
std::vector<CogValue> loadCogSymbolsNoLocals(const ResourceLocator& resources, std::string_view scriptName);

// Parses the `symbols` section of a .cog script and returns symbols, optionally including locals/messages.
// Default assignments written as `name=value` are parsed into the returned CogValue,
// except for vector declarations: the engine's symbol parser ignores their initializer
// text and always starts them at `(0/0/0)`. Runtime level-instance vector values use the
// separate CogValue parsing path.
// Unset non-local COG/sector/surface/thing refs use the placed-COG default of `-1`.
// Vectors use `(0/0/0)`; resource and message strings remain empty until authored.
std::vector<CogValue> loadCogSymbolsWithDefaults(const ResourceLocator& resources, std::string_view scriptName, bool includeLocals);

} // namespace sed::assets
