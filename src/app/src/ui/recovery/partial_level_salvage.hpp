#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>

#include <QString>
#include <QStringList>

#include "sed/level.hpp"

namespace sed::assets
{

class ResourceLocator;

} // namespace sed::assets

namespace sed::ui::recovery
{

namespace detail
{

enum class ForensicNewlinePolicy
{
    replaceWithSpace,
    indentContinuation,
};

/**
 * Makes untrusted text safe for plain-text forensic artifacts without first
 * materializing an expanded intermediate string. CR/LF follow the caller's
 * readability policy; other control, format, and line-separator characters
 * are emitted as visible Unicode escapes. The returned value never exceeds
 * maximumCharacters.
 */
QString escapeForensicText(
    const QString& source,
    qsizetype maximumCharacters,
    ForensicNewlinePolicy newlinePolicy,
    bool* truncated = nullptr);

} // namespace detail

enum class PartialLevelProjectTypeSource
{
    checkedMetadata,
    userSelected,
    unknown,
};

/** Context retained alongside a non-authoritative partial-Level text salvage. */
struct PartialLevelSalvageContext
{
    // The damaged input selected by the user.
    std::filesystem::path sourceRecoveryPath{};
    // The separately retained forensic input. The artifact is written beside
    // this path, or beside sourceRecoveryPath when retention failed.
    std::filesystem::path forensicRecoveryPath{};
    QStringList missingRequiredSections{};
    // Checked framing could not name these lost semantic units. Any one may
    // have contained required Level data, so this independently activates
    // salvage even when no required section name is available.
    std::uint32_t unidentifiedMissingSectionCount{};
    QStringList recoveryWarnings{};
    PartialLevelProjectTypeSource projectTypeSource{
        PartialLevelProjectTypeSource::checkedMetadata };

    // Immutable resource generation captured before background salvage starts.
    // It is consulted only for exact MAT dimensions when the recovered Level's
    // normalized UVs must be converted to classic JKL pixel coordinates.
    std::shared_ptr<const assets::ResourceLocator> nativeExportResources{};

    // Empty in production. A stable value is useful to exercise collision
    // handling without depending on timestamps or UUIDs in focused tests.
    QString artifactId{};
};

struct PartialLevelSalvageResult
{
    std::filesystem::path manifestPath{};
    std::filesystem::path nativeTextPath{};
    QString nativeExportFailure{};
    QString error{};

    bool wroteManifest() const noexcept { return !manifestPath.empty(); }
    bool wroteNativeText() const noexcept { return !nativeTextPath.empty(); }

    /** Warning suitable for the Messages panel and recovery audit log. */
    QString recoveryWarning() const;
};

/**
 * Test seam for a native text writer. Production callers leave this empty and
 * use assets::saveToJKL(), which emits JKL for JKDF2/MotS and NDY for IJIM.
 */
using PartialLevelNativeExporter =
    std::function<void(const Level&, std::ostream&)>;

struct PartialLevelSalvageTestHooks
{
    PartialLevelNativeExporter nativeExporter{};
    // Invoked after a complete same-directory staging file has been flushed,
    // immediately before the atomic no-replace publication attempt.
    std::function<void(const std::filesystem::path&)> beforePublish{};
};

/** Filters structured native SED loss reports to required Level sections. */
QStringList lostRequiredLevelSections(const QStringList& missingSections);

/**
 * Writes a collision-safe, atomically published last-resort artifact.
 *
 * A recovery with neither named required loss nor unidentified loss is a
 * no-op. Native JKL/NDY is published only for a checked or explicitly
 * user-selected project type and after bounded structural validation,
 * expected-project
 * parse-back, and a second semantic re-save check. MotS and JKDF2 both use
 * VERSION 1, so an otherwise ambiguous MotS parse also requires the writer's
 * ARCHLIGHTING marker plus checked or explicitly user-selected project
 * context. The original readable, non-compact export is preserved; validation
 * never publishes a lossy canonical rewrite. The optional compact
 * runtime-export presentation is intentionally not used for this forensic
 * artifact.
 * When normalized UVs must cross a classic JKL boundary, the context's
 * immutable resource snapshot supplies exact MAT dimensions; the recovered
 * Level itself is never retagged or modified.
 * The plain-text manifest is still written if native export fails.
 */
PartialLevelSalvageResult writePartialLevelSalvage(
    const Level& partialLevel,
    const PartialLevelSalvageContext& context,
    PartialLevelSalvageTestHooks testHooks = {});

} // namespace sed::ui::recovery
