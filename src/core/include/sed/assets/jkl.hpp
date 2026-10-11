#pragma once

#include <filesystem>
#include <cstdint>
#include <istream>
#include <ostream>

#include "sed/diagnostic.hpp"
#include "sed/level.hpp"

namespace sed::assets
{

class ResourceLocator;
class Templates;

struct JklLoadOptions
{
    ProjectType projectType{ProjectType::JKDF2};
    const ResourceLocator* resources{};
    DiagnosticSink onDiagnostic{};
};

struct JklSaveOptions
{
    const ResourceLocator* resources{};
    const Templates* templatesDb{};
    DiagnosticSink onDiagnostic{};

    // The editor-owned sections are useful to SED but are not required by
    // the game loader.  Keep them enabled by default for backwards
    // compatible exports; callers can omit either section when producing a
    // runtime-only JKL/NDY file.
    bool includeLights{true};
    bool includeLayers{true};

    // Emit the parser-compatible compact text dialect: LF line endings,
    // tab-separated fields without padded columns, and shortest safe decimal
    // spellings for floating point values.
    bool compact{false};

    // Clamp exported signed surface extra-light scalar/RGB values to [-1, 1].
    // Alpha keeps the existing [0, 1] wire clamp in all modes.
    bool clampExtraLight{true};

    // Omit all-zero MotS ARCHLIGHTING records and write -1 for their Things.
    // This opts into the engine's 3DO vertex-light fallback and changes the
    // rendered lighting; leave disabled to preserve the bound zero default.
    bool omitDefaultMotsArchLighting{false};

    // Share exactly coincident world coordinates by default. Nearby
    // world-vertex welding is an explicit export opt-in and uses this maximum
    // Euclidean distance when enabled.
    bool weldNearbyVertices{false};
    double vertexWeldDistance{0.00001};
};

/** One growable runtime resource pool as it will be written to JKL/NDY. */
struct JklExportBufferUsage
{
    std::uint64_t collected{};
    // Retained for source compatibility with older callers. V1 now exposes
    // every spare slot through `additional`, so this is always zero.
    std::uint64_t automaticReserve{};
    std::uint64_t additional{};
    std::uint64_t total{};

    constexpr bool operator==(const JklExportBufferUsage&) const noexcept = default;
};

/** Exact resource-capacity calculation shared with the JKL/NDY writer. */
struct JklExportBufferReport
{
    JklExportBufferUsage sounds{};
    JklExportBufferUsage materials{};
    JklExportBufferUsage aiClasses{};
    JklExportBufferUsage models{};
    JklExportBufferUsage sprites{};
    JklExportBufferUsage keyframes{};
    JklExportBufferUsage puppets{};
    JklExportBufferUsage soundClasses{};
    JklExportBufferUsage cogScripts{};
    JklExportBufferUsage cogs{};
    JklExportBufferUsage templates{};
    JklExportBufferUsage things{};
    bool dependencyScanComplete{true};

    constexpr bool operator==(const JklExportBufferReport&) const noexcept = default;
};

// Parses textual Sith level data: JKDF2/MotS JKL and IJIM NDY.
Level loadFromJKL(std::istream& in, const JklLoadOptions& options = {});
Level loadFromJKL(const std::filesystem::path& path, const JklLoadOptions& options = {});

/**
 * Completes the resource-dependent UV part of a previously decoded JKL/NDY.
 *
 * Callers that must determine the project kind from the file before choosing
 * a resource locator can first load with `JklLoadOptions::resources ==
 * nullptr`, then call this routine after staging the authoritative locator.
 * Conversion is failure-atomic and retains the legacy pixel-space Level when
 * a material dimension or material-less default cannot be resolved.
 */
void finalizeJklImportUv(
    Level& level,
    const ResourceLocator& resources,
    DiagnosticSink onDiagnostic = {});

/**
 * Completes every resource-dependent part of a previously decoded JKL/NDY.
 *
 * This includes COG schema/value hydration and, for classic pixel-space
 * input, MAT-backed UV normalization.  Callers that first decode a VERSION 1
 * JKL without resources must invoke this after staging the authoritative
 * project locator; the operation is failure-atomic for UV conversion and
 * preserves raw COG arguments when a script is unavailable.
 */
void finalizeJklImportResources(
    Level& level,
    const ResourceLocator& resources,
    DiagnosticSink onDiagnostic = {});

/**
 * Crawl the same template/resource graph as saveToJKL and report the exact
 * required-record, editable-extra, and final capacities without
 * serializing geometry.
 */
JklExportBufferReport inspectJklExportBuffers(
    const Level& level,
    const JklSaveOptions& options = {});

void saveToJKL(const Level& level, std::ostream& out);
void saveToJKL(const Level& level, const std::filesystem::path& path);
void saveToJKL(const Level& level, std::ostream& out, const JklSaveOptions& options);
void saveToJKL(const Level& level, const std::filesystem::path& path, const JklSaveOptions& options);

} // namespace sed::assets
