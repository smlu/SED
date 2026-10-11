#pragma once

#include <filesystem>
#include <istream>

#include "sed/assets/df2jk.hpp"
#include "sed/diagnostic.hpp"
#include "sed/level.hpp"

namespace sed::assets
{

class ResourceLocator;

enum class LevTextureMode : std::int32_t
{
    Default,
    Keep,
};

struct LevLoadOptions
{
    // DF -> JK conversion factor. Delphi calls this `scfactor` / `jkdffactor`.
    // DF coordinates are divided by this value.
    double dfToJkFactor{ 35.0 };

    LevTextureMode textureMode{ LevTextureMode::Keep };

    // Only applies to the path-based overload; when enabled, we attempt to load a same-name `.O` file.
    bool loadObjects{ true };

    // Optional DF logic -> JK template conversion table used when loading `.O` objects (df2jk.lst).
    // If `conversionTable` is null, `conversionTablePath` can be provided instead.
    const Df2JkConvTable* conversionTable{};
    std::filesystem::path conversionTablePath{};

    const ResourceLocator* resources{};
    DiagnosticSink onDiagnostic{};
};

// Imports a Dark Forces `.LEV` (and optional `.O`) into a JK level (see src/DF_IMPORT.INC).
Level loadFromLEV(std::istream& in, const LevLoadOptions& options = {});
Level loadFromLEV(const std::filesystem::path& levPath, const LevLoadOptions& options = {});

} // namespace sed::assets
