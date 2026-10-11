#pragma once

#include <filesystem>
#include <istream>
#include <ostream>

#include "sed/diagnostic.hpp"
#include "sed/level.hpp"

namespace sed::assets
{

class ResourceLocator;

struct JedLoadOptions
{
    const ResourceLocator* resources{};
    DiagnosticSink onDiagnostic{};
};

struct JedSaveOptions
{
    const ResourceLocator* resources{};
};

// Minimal JED/SED (editor) format support, based on src/LEVEL_IO.INC.
Level loadFromJED(
    std::istream& in,
    const JedLoadOptions& options = {});
Level loadFromJED(
    const std::filesystem::path& path,
    const JedLoadOptions& options = {});

void saveToJED(
    const Level& level,
    std::ostream& out,
    const JedSaveOptions& options = {});
void saveToJED(
    const Level& level,
    const std::filesystem::path& path,
    const JedSaveOptions& options = {});

} // namespace sed::assets
