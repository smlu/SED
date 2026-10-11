#pragma once

#include <cstddef>
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "sed/level.hpp"

namespace sed::editor
{
class DocumentUndoHistory;
struct Selection;
}

namespace sed::assets
{

/** Supported binary SED project format versions.
 *
 * V1 stores Level UV coordinates in texture pixels. V2 stores normalized
 * Level UV coordinates. V3 and V4 retain the same UV units while adding the
 * optional MotS ARCHLIGHTING table to the Things section (V3 normalized,
 * V4 pixel-space). SED is the lossless editor format; JKL and NDY remain
 * project-specific runtime interchange formats and may intentionally omit or
 * reduce editor data that their respective engines cannot represent.
 */
inline constexpr std::uint32_t SedLegacyBinaryVersion = 1;
inline constexpr std::uint32_t SedBinaryVersion = 2;
inline constexpr std::uint32_t SedArchNormalizedBinaryVersion = 3;
inline constexpr std::uint32_t SedArchPixelBinaryVersion = 4;

/** Returns whether a native SED header version is understood by this build. */
[[nodiscard]] constexpr bool isSedBinaryVersionSupported(
    const std::uint32_t version) noexcept
{
    return version == SedLegacyBinaryVersion
        || version == SedBinaryVersion
        || version == SedArchNormalizedBinaryVersion
        || version == SedArchPixelBinaryVersion;
}

/** Returns the UV coordinate space encoded by a native SED version. */
[[nodiscard]] constexpr bool sedBinaryVersionUsesTexturePixels(
    const std::uint32_t version) noexcept
{
    return version == SedLegacyBinaryVersion
        || version == SedArchPixelBinaryVersion;
}

/** Returns whether a native SED version carries MotS ARCHLIGHTING data. */
[[nodiscard]] constexpr bool sedBinaryVersionHasArchLighting(
    const std::uint32_t version) noexcept
{
    return version == SedArchNormalizedBinaryVersion
        || version == SedArchPixelBinaryVersion;
}
// Native project/recovery documents are staged in memory by the current API.
// A multi-gigabyte ceiling would therefore be a denial-of-service
// allowance rather than useful format headroom.  Keep this public so the
// recovery envelope applies the same limit before it extracts DOCM.
inline constexpr std::uint64_t SedMaximumFileBytes =
    256ull * 1024ull * 1024ull;
inline constexpr std::uint64_t SedMaximumSemanticSectionBytes =
    128ull * 1024ull * 1024ull;
// Compression can make many individually valid sections expand far beyond
// the encoded file size.  Bound their combined semantic footprint before any
// section is expanded so one document cannot drive multi-gigabyte allocation.
inline constexpr std::uint64_t SedMaximumExpandedPayloadBytes =
    256ull * 1024ull * 1024ull;
// Semantic bytes can still expand when represented by vectors, strings, and
// nested Level objects. These limits cover the decoded object graph itself;
// they do not double-count the checked raw section buffers.
inline constexpr std::uint64_t SedMaximumDecodedObjectBytes =
    128ull * 1024ull * 1024ull;
inline constexpr std::uint64_t SedMaximumDecodedItems = 2'000'000ull;

/** Logical owned storage retained by one decoded Level.
 *
 * ownedBytes counts sizeof(Level), logical vector element storage, and every
 * string's characters plus its terminator. itemCount counts the Level root and
 * every logical vector element. Spare capacity, allocator bookkeeping, and raw
 * encoded/expanded section buffers are intentionally excluded. This is the
 * exact accounting used by the native reader and writer admission checks.
 */
struct SedLevelDecodedFootprint
{
    std::uint64_t ownedBytes{};
    std::uint64_t itemCount{};

    constexpr bool operator==(const SedLevelDecodedFootprint&) const noexcept = default;
};

/** Adds a retained Level footprint without wrapping either counter.
 * The destination is unchanged on overflow.
 */
[[nodiscard]] constexpr bool tryAccumulateSedLevelDecodedFootprint(
    SedLevelDecodedFootprint& destination,
    const SedLevelDecodedFootprint addition) noexcept
{
    if (addition.ownedBytes
            > std::numeric_limits<std::uint64_t>::max() - destination.ownedBytes
        || addition.itemCount
            > std::numeric_limits<std::uint64_t>::max() - destination.itemCount)
    {
        return false;
    }
    destination.ownedBytes += addition.ownedBytes;
    destination.itemCount += addition.itemCount;
    return true;
}
// HIST is deliberately lower priority than the authoritative Level and EDIT
// domains.  Bounding its expanded CBOR before decompression prevents an
// optional undo timeline from becoming a multi-gigabyte parse allocation.
// The application history codec uses this same V1 wire limit.
inline constexpr std::uint64_t SedMaximumHistorySectionBytes =
    64ull * 1024ull * 1024ull;
inline constexpr std::uint64_t SedFecSlotBytes = 4096;
inline constexpr std::uint64_t SedFecSlotPayloadBytes = SedFecSlotBytes - 4;
inline constexpr std::uint64_t SedMaximumCodedSectionBytes =
    (16ull + 4ull)
    * (((SedMaximumSemanticSectionBytes + SedFecSlotPayloadBytes - 1ull)
            / SedFecSlotPayloadBytes + 15ull) / 16ull)
    * SedFecSlotBytes + (SedFecSlotBytes - 1ull);
static_assert(SedMaximumCodedSectionBytes < SedMaximumFileBytes,
    "One maximally protected semantic section must fit the V1 file cap.");

/** Application-owned data embedded in a SED file.
 *
 * The core deliberately treats these four-character sections as opaque so
 * editor workspace data can evolve without introducing Qt into sed_core.
 */
struct SedOptionalSection
{
    std::array<char, 4> type{};
    std::vector<std::byte> payload{};
};

/** Import-only JKL COG hydration metadata carried outside the eight required
 * native Level sections. An empty state is explicit and never inherited. */
struct SedJklCogImportState
{
    bool cogRowsPresent{};
    std::vector<SurfaceRef> surfaceRefs{};
};

/** True when JKL import metadata is needed to interpret this Level's COG rows.
 * A false marker with an unused surface map is intentionally treated as empty.
 */
[[nodiscard]] bool hasRepresentedJklCogImportState(
    const Level& level) noexcept;

struct SedUnreadableRange
{
    std::uint64_t offset{};
    std::uint64_t length{};
};

struct SedFile
{
    Level level{};
    std::vector<SedOptionalSection> optionalSections{};
    // Signed UTC Unix-epoch milliseconds. Zero means unknown. A top-level
    // writer supplies the instant at which these file bytes were created;
    // nested history encodings use canonical zero.
    std::int64_t timestamp{};

    /** Integrity details for the transparent V1 repair path.
     *
     * A normal load still fails when a semantic unit is beyond its parity
     * budget. Required Level sections and the optional EDIT section (which may
     * contain the user's only unsaved COG source) repair up to four invalid
     * checked chunks per stripe; lower-priority optional sections such as HIST
     * repair two. Repairs and mirrored-descriptor recovery remain observable
     * to callers that want to surface a warning.
     */
    std::uint32_t repairedChunks{};
    bool hadCorruption{};
    std::vector<std::string> repairedSections{};
    std::vector<SedUnreadableRange> unreadableRanges{};
};

/** Serialization profiles for top-level files and nested Level streams.
 *
 * ProtectedV1 is the normal on-disk `.sed` profile. UnprotectedNested keeps
 * SHA-256 but omits compression/parity for generic canonical content hashes.
 * HistoryNested is reserved for snapshots inside an already SHA-256/FEC
 * checked HIST section.
 */
enum class SedSerializationProfile
{
    ProtectedV1,
    UnprotectedNested,
    /**
     * Canonical Level framing used only inside the independently SHA-256
     * checked `hist` section. Its section payloads use CRC32C rather than a
     * redundant per-state SHA-256 pass; callers must still validate the
     * enclosing HIST container before selecting this profile.
     */
    HistoryNested,
};

/** Canonical required-section invalidation mask for generated nested Levels. */
enum class SedRequiredSection : std::uint16_t
{
    None = 0,
    Head = 1u << 0,
    Resources = 1u << 1,
    Sectors = 1u << 2,
    Things = 1u << 3,
    Templates = 1u << 4,
    Cogs = 1u << 5,
    Lights = 1u << 6,
    Hierarchy = 1u << 7,
    All = (1u << 8) - 1u,
};

constexpr SedRequiredSection operator|(
    const SedRequiredSection lhs, const SedRequiredSection rhs) noexcept
{
    return static_cast<SedRequiredSection>(
        static_cast<std::uint16_t>(lhs) | static_cast<std::uint16_t>(rhs));
}

constexpr SedRequiredSection& operator|=(
    SedRequiredSection& lhs, const SedRequiredSection rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool hasAny(
    const SedRequiredSection lhs, const SedRequiredSection rhs) noexcept
{
    return (static_cast<std::uint16_t>(lhs)
        & static_cast<std::uint16_t>(rhs)) != 0u;
}

/** Immutable exact frames reused while generating adjacent HIST Levels.
 *
 * HistoryNested section frames contain no offset-dependent FEC alignment,
 * so unchanged checked frames can be shared safely between adjacent outputs.
 * This cache is writer-owned generated state; it is never populated from an
 * input file and is not a substitute for strict decoding.
 */
struct SedGeneratedUnprotectedDocument;

/** Opaque proof for one exact live undo/redo transition.
 *
 * Only DocumentUndoHistory can create this token, while it is visiting the
 * concrete private Snapshot variant that owns the transition. The token is
 * bound to that visit's exact Level object and cannot be default-constructed,
 * copied, moved, or retained by a value-taking callback. Persistence may read
 * its immutable domain information and pass it to the internal history writer.
 */
class SedHistoryTransitionToken final
{
public:
    SedHistoryTransitionToken() = delete;
    SedHistoryTransitionToken(const SedHistoryTransitionToken&) = delete;
    SedHistoryTransitionToken& operator=(
        const SedHistoryTransitionToken&) = delete;
    SedHistoryTransitionToken(SedHistoryTransitionToken&&) = delete;
    SedHistoryTransitionToken& operator=(SedHistoryTransitionToken&&) = delete;

    [[nodiscard]] bool matches(const Level& level) const noexcept
    {
        return level_ == &level;
    }

    [[nodiscard]] SedRequiredSection requiredSections() const noexcept
    {
        return requiredSections_;
    }

    [[nodiscard]] bool sectorSubset() const noexcept
    {
        return sectorSubset_;
    }

    [[nodiscard]] bool surfaceSubset() const noexcept
    {
        return surfaceSubset_;
    }

    [[nodiscard]] std::span<const std::int32_t> sectorIndices() const noexcept
    {
        return sectorIndices_;
    }

    [[nodiscard]] bool thingSubset() const noexcept
    {
        return thingSubset_;
    }

    [[nodiscard]] std::span<const std::int32_t> thingIndices() const noexcept
    {
        return thingIndices_;
    }

    [[nodiscard]] bool thingTail() const noexcept
    {
        return thingTail_;
    }

    [[nodiscard]] std::size_t thingTailBaseSize() const noexcept
    {
        return thingTailBaseSize_;
    }

    [[nodiscard]] std::size_t thingTailAffectedCount() const noexcept
    {
        return thingTailAffectedCount_;
    }

    /** Exact post-operation selection retained for History navigation.
     *
     * The pointer is valid only for the synchronous history-visitor callback
     * which owns this token.  It is intentionally a compact Selection hint,
     * not a second Level snapshot.
     */
    [[nodiscard]] const editor::Selection* navigationSelection() const noexcept
    {
        return navigationSelection_;
    }

private:
    friend class ::sed::editor::DocumentUndoHistory;

    SedHistoryTransitionToken(
        const Level& level,
        const SedRequiredSection requiredSections,
        const bool sectorSubset,
        std::vector<std::int32_t> sectorIndices,
        const bool surfaceSubset,
        const bool thingSubset,
        std::vector<std::int32_t> thingIndices,
        const bool thingTail,
        const std::size_t thingTailBaseSize,
        const std::size_t thingTailAffectedCount,
        const editor::Selection* const navigationSelection)
        : level_(&level),
          requiredSections_(requiredSections),
          sectorSubset_(sectorSubset),
          sectorIndices_(std::move(sectorIndices)),
          surfaceSubset_(surfaceSubset),
          thingSubset_(thingSubset),
          thingIndices_(std::move(thingIndices)),
          thingTail_(thingTail),
          thingTailBaseSize_(thingTailBaseSize),
          thingTailAffectedCount_(thingTailAffectedCount),
          navigationSelection_(navigationSelection)
    {
    }

    const Level* level_{};
    SedRequiredSection requiredSections_{ SedRequiredSection::All };
    bool sectorSubset_{};
    std::vector<std::int32_t> sectorIndices_{};
    bool surfaceSubset_{};
    bool thingSubset_{};
    std::vector<std::int32_t> thingIndices_{};
    bool thingTail_{};
    std::size_t thingTailBaseSize_{};
    std::size_t thingTailAffectedCount_{};
    const editor::Selection* navigationSelection_{};
};

class SedGeneratedUnprotectedRequiredSections;

namespace detail
{
struct HistoryNestedGenerationAccess;
SedGeneratedUnprotectedDocument generateHistoryNestedTransitionDocument(
    const Level&,
    const SedHistoryTransitionToken&,
    const SedGeneratedUnprotectedRequiredSections*,
    bool,
    SedRequiredSection);
}

class SedGeneratedUnprotectedRequiredSections
{
public:
    SedGeneratedUnprotectedRequiredSections() = default;

    /** Exact immutable required-section frames in canonical file order.
     *
     * These pointers are generated-writer provenance, not decoded input.  A
     * repeated pointer therefore proves byte identity and lets the HIST
     * container reuse its already-interned fragment references without
     * hashing or copying a multi-megabyte unchanged section again.
     */
    [[nodiscard]] const std::array<
        std::shared_ptr<const std::vector<std::byte>>, 8>&
    exactFrames() const noexcept
    {
        return frames;
    }

private:
    friend struct SedGeneratedUnprotectedDocument;
    friend struct detail::HistoryNestedGenerationAccess;

    ProjectType projectType{ ProjectType::JKDF2 };
    std::uint32_t binaryVersion{};
    bool configured{};
    std::array<std::shared_ptr<const std::vector<std::byte>>, 8> frames{};
    // Opaque writer-only provenance.  The concrete type lives in sed.cpp so
    // the public header does not expose the resource hash/index machinery.
    // It is populated only by the complete/token-checked generators below
    // and is never accepted from an input file.
    std::shared_ptr<const void> writerProvenance{};
};

struct SedGeneratedUnprotectedDocument
{
    SedGeneratedUnprotectedRequiredSections requiredSections{};
    // Canonical fixed header for this exact generated Level, available even
    // when `bytes` is omitted and only the required-section frames are built.
    // Recovery HIST uses it to update the version/CRC prefix across native
    // format-version transitions without materializing the complete file.
    std::vector<std::byte> canonicalHeader{};
    // Canonical end-of-file header mirror and footer for this exact Level.
    // HistoryNested stores this framing with the HNOD tail, so it must follow
    // the same native-version transition as `canonicalHeader`.
    std::vector<std::byte> canonicalTrailingFraming{};
    std::vector<std::byte> bytes{};
    std::size_t byteSize{};
    SedRequiredSection regeneratedSections{ SedRequiredSection::None };
    bool resourceCatalogChanged{};
};

/** Generates one exact HistoryNested required-Level document.
 *
 * This public entry point always validates and encodes every required Level
 * domain. It returns an immutable writer-owned cache only so the internal,
 * token-checked transition overload can reuse exact frames later; public
 * callers cannot supply an incomplete invalidation mask.
 */
SedGeneratedUnprotectedDocument generateUnprotectedSedDocument(
    const Level& level, bool materializeBytes = true);

namespace detail
{

/** Internal writer-side fast path for an exact DocumentUndoHistory visit.
 *
 * The public generator above always performs a complete validation/encode.
 * This overload honors an incomplete mask only after the unforgeable token is
 * verified against the exact Level object currently supplied by the visitor.
 * additionalSections can only broaden the token's mask (manual Save uses it
 * for derived Thing-frame normalization); it can never omit a Snapshot-owned
 * domain.
 */
SedGeneratedUnprotectedDocument generateHistoryNestedTransitionDocument(
    const Level& level,
    const SedHistoryTransitionToken& transition,
    const SedGeneratedUnprotectedRequiredSections* reusable,
    bool materializeBytes,
    SedRequiredSection additionalSections = SedRequiredSection::None);

/**
 * Returns the byte ranges of the individual Sector records in a validated
 * HistoryNested `sect` frame.  The offsets are relative to the complete
 * frame, including its checked descriptor and trailer.  This is a writer-
 * side seam used by HIST pooling: record-sized fragments let an unchanged
 * Sector be reused after a neighboring Sector grows or shrinks, without
 * changing the on-disk nested Level schema.  The input must be produced by
 * the HistoryNested writer; untrusted input must go through the normal
 * checked loader instead. `maximumRecords` is an optional writer-side
 * allocation bound; when the counted record vector is larger, the function
 * throws before reserving/materializing it.
 */
struct SedHistorySectionRecordSpan
{
    std::size_t begin{};
    std::size_t end{};
};

std::vector<SedHistorySectionRecordSpan>
splitHistoryNestedSectorFrameRecords(
    std::span<const std::byte> frame,
    std::size_t maximumRecords = std::numeric_limits<std::size_t>::max());

} // namespace detail

/** Encodes one complete Level as a versioned, checksummed binary SED file.
 *
 * Independent semantic sections are compressed when that reduces their size.
 * All integer fields are little-endian. Finite floating-point values retain
 * their exact IEEE-754 bit representation, including signed zero; non-finite
 * authored values are rejected before a file can be emitted.
 */
std::vector<std::byte> saveToSedBytes(const Level& level);

/** Encodes a Level plus independently checked application-owned sections. */
std::vector<std::byte> saveToSedBytes(
    const Level& level,
    std::span<const SedOptionalSection> optionalSections);

/** Encodes with an explicit top-level or nested-history protection profile. */
std::vector<std::byte> saveToSedBytes(
    const Level& level,
    std::span<const SedOptionalSection> optionalSections,
    SedSerializationProfile profile);

/** Encodes with an explicit top-level file-creation timestamp.
 *
 * ProtectedV1 stores the supplied non-negative UTC Unix-epoch millisecond
 * value. Both nested profiles always write canonical zero so history/content
 * digests do not depend on wall-clock time.
 */
std::vector<std::byte> saveToSedBytes(
    const Level& level,
    std::span<const SedOptionalSection> optionalSections,
    SedSerializationProfile profile,
    std::int64_t timestamp);

/** Decodes and validates one complete binary SED file. */
Level loadFromSedBytes(std::span<const std::byte> bytes);

/**
 * Exact semantic comparison for the complete current Level model.
 *
 * This compares every authored Level/header/resource/entity field using
 * IEEE bit identity for floating-point values (including signed zero; the
 * comparison itself also distinguishes NaN payloads even though the file
 * writer rejects non-finite authored values). It is intended for
 * codec/recovery self-checks and tests; it does not compare application-owned
 * optional editor sections.
 */
bool sedLevelsEqual(const Level& lhs, const Level& rhs) noexcept;

/** Strict native-save/readback comparison for a Level and its decoded result.
 *
 * The authored Level is allowed only the ARCH table filtering and reference
 * compaction performed by the native MotS writer. Every other authored field,
 * including bake provenance and every non-ARCH Thing field, remains exact.
 */
bool sedLevelNativeSerializationMatches(
    const Level& authored,
    const Level& decoded) noexcept;

/** Exact semantic comparison excluding only the Sector vector.
 *
 * History import uses this non-copying predicate while inferring a compact
 * geometry subset from adjacent states. It avoids constructing metadata-only
 * Level proxies (and therefore avoids copying large Things/resources) for
 * every structural checkpoint. `curId` is intentionally excluded because
 * compact snapshots retain and swap it beside their changed domain.
 */
bool sedLevelsEqualIgnoringSectors(const Level& lhs, const Level& rhs) noexcept;

/**
 * Exact comparison for history dirty-mask validation without copying either
 * Level. `ignoreThings` excludes the complete Things vector, while
 * `ignoreLightsAndBakedLighting` excludes authored lights plus the derived
 * sector ambient/texture-vertex colors captured by a Lights snapshot, and
 * `ignoreCogs` excludes the complete COG vector. The
 * Level curId is always excluded because every compact partial snapshot keeps
 * that identifier alongside its mutable domain.
 */
bool sedLevelsEqualIgnoringHistoryDomains(
    const Level& lhs,
    const Level& rhs,
    bool ignoreThings,
    bool ignoreLightsAndBakedLighting,
    bool ignoreCogs) noexcept;

/** Estimates retained decoded Level storage using native V1 admission rules.
 *
 * This does not enforce the per-Level limits, allowing callers to inspect an
 * over-limit in-memory graph. It throws std::runtime_error if either exact
 * counter cannot be represented in uint64_t. Use the checked accumulator above
 * when combining multiple retained HIST snapshots.
 */
[[nodiscard]] SedLevelDecodedFootprint estimateSedLevelDecodedFootprint(
    const Level& level);

/** Decodes the generic SHA-256 unprotected nested profile. */
Level loadNestedSedLevelBytes(std::span<const std::byte> bytes);

/** Strictly decodes a Level frame already authenticated by a HIST section. */
Level loadHistoryNestedSedLevelBytes(std::span<const std::byte> bytes);

/** Per-transition instrumentation for incremental nested-HIST decoding. */
struct SedNestedHistoryDecodeStats
{
    std::uint32_t decodedRequiredSections{};
    std::uint32_t reusedRequiredSections{};
    /** Changed semantic domains installed by vector/string swap rather than
     * copying the complete preceding Level. */
    std::uint32_t transactionallyReplacedRequiredSections{};
    bool resourceSectionChanged{};
    bool usedTransactionalDomainReuse{};
};

/** Strict stateful decoder for adjacent HistoryNested HIST Levels.
 *
 * Construction canonicalizes and validates the authoritative current Level.
 * advance() then validates the complete framing, descriptor copies, payload
 * CRC32C, canonical resource order, and final object graph of every supplied
 * state. Semantic objects are reused only when the exact checked section frame
 * is byte-identical to the previously accepted state. A changed `rsrc` frame
 * forces every category-indexed section through semantic decoding even when
 * that section's bytes happen to be unchanged.
 *
 * The decoder owns the trust chain: callers cannot provide an unrelated prior
 * Level/byte pair. Unchanged vectors are moved through a failure-atomic
 * transaction; only domains actually rematerialized by the candidate retain
 * their preceding values. previousLevel() remains available as a compatibility
 * view and materializes a full copy lazily. Compact history import instead uses
 * the transition predicates/subsets below and never creates that copy. The span
 * overload copies its candidate; the vector overload adopts the candidate only
 * after strict validation succeeds.
 */
class SedNestedHistoryDecoder
{
public:
    explicit SedNestedHistoryDecoder(const Level& authoritativeCurrent);

    void advance(std::span<const std::byte> bytes);
    void advance(std::vector<std::byte>&& bytes);
    void advance(
        std::span<const std::byte> bytes,
        SedJklCogImportState cogImportState);
    void advance(
        std::vector<std::byte>&& bytes,
        SedJklCogImportState cogImportState);

    [[nodiscard]] bool hasPrevious() const noexcept { return hasPrevious_; }
    [[nodiscard]] const Level& previousLevel() const;
    [[nodiscard]] const Level& currentLevel() const noexcept { return current_; }
    [[nodiscard]] std::span<const std::byte> currentBytes() const noexcept
    {
        return currentBytes_;
    }
    [[nodiscard]] const SedNestedHistoryDecodeStats& lastStats() const noexcept
    {
        return lastStats_;
    }
    /** Diagnostic count of explicit compatibility previousLevel() copies.
     * Normal compact HIST restore must leave this at zero. */
    [[nodiscard]] std::size_t previousLevelMaterializationCount() const noexcept
    {
        return previousLevelMaterializationCount_;
    }

    /** Exact adjacent-state comparison used by compact history validation.
     *
     * curId is intentionally ignored, matching the live compact snapshots.
     * Lights also ignores baked Sector ambient/texture-vertex colors. No
     * result is derived from the untrusted persisted dirty mask.
     */
    [[nodiscard]] bool lastTransitionEqualsIgnoringHistoryDomains(
        bool ignoreThings,
        bool ignoreLightsAndBakedLighting,
        bool ignoreCogs) const;

    /** Exact changed Thing indices when every other semantic domain is equal
     * and Thing topology is unchanged; nullopt otherwise. */
    [[nodiscard]] const std::optional<std::vector<std::int32_t>>&
        lastChangedThingIndices() const;

    /** Exact changed Sector indices when every other semantic domain is equal
     * and Sector topology is unchanged; nullopt otherwise. */
    [[nodiscard]] const std::optional<std::vector<std::int32_t>>&
        lastChangedSectorIndices() const;

private:
    Level previousDomains_{};
    Level current_{};
    std::vector<std::byte> currentBytes_{};
    SedNestedHistoryDecodeStats lastStats_{};
    SedRequiredSection previousDomainsMask_{ SedRequiredSection::None };
    mutable std::optional<Level> materializedPrevious_{};
    mutable std::optional<std::vector<std::int32_t>> changedThingIndices_{};
    mutable std::optional<std::vector<std::int32_t>> changedSectorIndices_{};
    mutable bool changedThingIndicesComputed_{};
    mutable bool changedSectorIndicesComputed_{};
    bool previousCogImportStateChanged_{};
    mutable std::size_t previousLevelMaterializationCount_{};
    bool hasPrevious_{};
};

/** Decodes both the Level and every validated optional section. */
SedFile loadSedFileBytes(std::span<const std::byte> bytes);

/** Result used only by crash recovery after outer parity reports data loss. */
struct SedRecoveryLoadResult
{
    Level level{};
    std::int64_t timestamp{};
    /** Project identity recovered from checked framing or HEAD.  This is
     * empty when both fixed headers and the HEAD semantic section are lost;
     * callers must not infer JKDF2 from the Level default in that case. */
    std::optional<ProjectType> projectType{};
    bool projectTypeTrusted{};
    std::vector<SedOptionalSection> optionalSections{};
    std::vector<std::string> missingSections{};
    std::uint32_t repairedChunks{};
    bool hadCorruption{};
    std::vector<std::string> repairedSections{};

    /** Known semantic units lost specifically because validated framing,
     * chunk-checksum, decompression, or strong-hash evidence proved damage.
     * Checksum-valid schema/programming errors remain hard failures.
     */
    std::vector<std::string> missingRequiredSections{};
    std::vector<std::string> missingOptionalSections{};
    std::uint32_t unidentifiedMissingSections{};
    bool partialRecoveryFromCorruption{};
    std::vector<SedUnreadableRange> unreadableRanges{};
};

/** Loads every independently valid section and defaults only missing sections.
 * Strict project-file opening must use loadFromSedBytes instead.
 */
SedRecoveryLoadResult recoverFromSedBytes(std::span<const std::byte> bytes);

/** Injectable random-access seam used when a normal whole-file read reports
 * an I/O error. A callback returns true only after filling the complete range.
 */
using SedRangeReader = std::function<bool(
    std::uint64_t offset, std::span<std::byte> destination)>;

struct SedResilientReadOptions
{
    std::uint32_t blockBytes{ 4096 };
    std::uint32_t maximumAttempts{ 3 };
};

/** Performs independent bounded-retry reads and strictly loads the repaired
 * document. Persistently unreadable blocks are zero-filled erasures.
 */
SedFile loadSedFileResilient(
    std::uint64_t fileSize,
    const SedRangeReader& reader,
    SedResilientReadOptions options = {});

/** Same bounded-retry reader, followed by corruption-only partial recovery. */
SedRecoveryLoadResult recoverSedFileResilient(
    std::uint64_t fileSize,
    const SedRangeReader& reader,
    SedResilientReadOptions options = {});

/** Path convenience for direct `.sed` salvage, including resilient reads. */
SedRecoveryLoadResult recoverSedFile(const std::filesystem::path& path);

struct SedEmbeddedDocument
{
    std::size_t offset{};
    std::size_t size{};
};

/** Locates one strictly valid framed SED document inside an arbitrary span. */
std::optional<SedEmbeddedDocument> findEmbeddedSedDocument(
    std::span<const std::byte> bytes,
    std::size_t searchOffset = 0,
    SedSerializationProfile profile = SedSerializationProfile::ProtectedV1);

/** Format CRC32C primitive. Passing false forces the portable table path. */
std::uint32_t calculateSedCrc32c(
    std::span<const std::byte> bytes,
    bool allowHardwareAcceleration = true) noexcept;
bool sedCrc32cHardwareAccelerationAvailable() noexcept;

/** Format SHA-256 primitive used for expanded semantic-section digests. */
std::array<std::byte, 32> calculateSedSha256(
    std::span<const std::byte> bytes) noexcept;

/** Splits a validated file at semantic boundaries for history/recovery tooling. */
std::vector<std::vector<std::byte>> splitSedFileSections(
    std::span<const std::byte> bytes,
    SedSerializationProfile profile = SedSerializationProfile::ProtectedV1);

/** Splits bytes just emitted by saveToSedBytes() without decoding every
 * semantic section a second time.
 *
 * This is an internal writer-side optimization for history pooling. Callers
 * must never pass file, network, recovery, or otherwise untrusted bytes; use
 * splitSedFileSections() for those. Framing and size arithmetic remain
 * checked so an encoder regression still fails closed.
 */
std::vector<std::vector<std::byte>> splitGeneratedSedFileSections(
    std::span<const std::byte> bytes,
    SedSerializationProfile profile = SedSerializationProfile::ProtectedV1);

/** Lightweight signature check; full validation is performed by the loader. */
bool hasSedBinarySignature(std::span<const std::byte> bytes) noexcept;

void saveToSED(const Level& level, std::ostream& out);
void saveToSED(
    const Level& level,
    std::span<const SedOptionalSection> optionalSections,
    std::ostream& out);
void saveToSED(
    const Level& level,
    std::span<const SedOptionalSection> optionalSections,
    std::int64_t timestamp,
    std::ostream& out);
/** Serializes and checks a sibling staging file before atomic replacement.
 * Failure preserves an existing destination; no direct-write fallback is used. */
void saveToSED(const Level& level, const std::filesystem::path& path);
Level loadFromSED(std::istream& in);
Level loadFromSED(const std::filesystem::path& path);
SedFile loadSedFile(std::istream& in);
SedFile loadSedFile(const std::filesystem::path& path);

/** Reads and validates only the fixed header needed for project-aware setup. */
ProjectType readSedProjectType(const std::filesystem::path& path);

/** Reads the checked native SED version, using the full strict reader when
 * the primary fixed header needs reconstruction from its checked mirror. */
std::uint32_t readSedBinaryVersion(const std::filesystem::path& path);

} // namespace sed::assets
