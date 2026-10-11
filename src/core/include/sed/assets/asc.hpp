#pragma once

#include <filesystem>
#include <istream>

#include "sed/diagnostic.hpp"
#include "sed/level.hpp"

namespace sed::assets
{

class ResourceLocator;

struct AscLoadOptions
{
    const ResourceLocator* resources{};
    DiagnosticSink onDiagnostic{};
};

// Imports a simple ASCII tri-mesh format used by the legacy editor (see src/ASC_IMPORT.INC).
Level loadFromASC(
    std::istream& in,
    const AscLoadOptions& options = {});
Level loadFromASC(
    const std::filesystem::path& path,
    const AscLoadOptions& options = {});

} // namespace sed::assets
