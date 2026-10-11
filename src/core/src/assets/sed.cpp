#include "sed/assets/sed.hpp"
#include "sed/level/arch_lighting.hpp"
#include "sed/level/thing.hpp"
#include "sed/text.hpp"
#include "atomic_file_write.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#include <nmmintrin.h>
#elif (defined(__GNUC__) || defined(__clang__)) \
    && (defined(__x86_64__) || defined(__i386__))
#include <nmmintrin.h>
#endif

namespace sed::assets
{
namespace
{

using Bytes = std::vector<std::byte>;

// Canonical SED V1 magic is four bytes so the fixed header starts on a
// naturally aligned boundary after the signature.  The NUL byte is part of
// the signature, not a terminator outside the file format.
constexpr std::array<std::byte, 4> kMagic{
    std::byte{'s'}, std::byte{'e'}, std::byte{'d'}, std::byte{0} };
constexpr std::array<std::byte, 8> kFooterMagic{
    std::byte{'s'}, std::byte{'e'}, std::byte{'d'}, std::byte{'e'},
    std::byte{'n'}, std::byte{'d'}, std::byte{0x0d}, std::byte{0x0a} };
// Protected payloads are systematic GF(256) codewords. Each physical 4 KiB
// slot contains 4092 coded bytes followed by their CRC32C. Complete streams
// are contiguous, so a localized overwrite is spread across independent
// horizontal stripes rather than consuming one stripe's entire parity budget.
// `mhdr` is the independently checked mirror of the fixed file header. The
// embedded header already carries the V1 discriminator and its own CRC32C, so
// repeating the product name and version in an eight-byte marker added no
// validation. The complete mirror structure, not this short anchor alone, is
// required before recovery trusts it.
constexpr std::array<std::byte, 4> kHeaderMirrorMagic{
    std::byte{'m'}, std::byte{'h'}, std::byte{'d'}, std::byte{'r'} };
constexpr std::array<std::byte, 8> kSectionAnchorMagic{
    std::byte{'s'}, std::byte{'e'}, std::byte{'c'}, std::byte{'t'},
    std::byte{'v'}, std::byte{'1'}, std::byte{'\n'}, std::byte{0} };
constexpr std::array<std::byte, 8> kSectionMirrorMagic{
    std::byte{'s'}, std::byte{'e'}, std::byte{'d'}, std::byte{'m'},
    std::byte{'i'}, std::byte{'r'}, std::byte{'1'}, std::byte{'\n'} };
constexpr std::uint32_t kCompressed = 1u;
constexpr std::uint32_t kParityProtected = 2u;
// Only legal for the explicit HistoryNested profile. The enclosing HIST
// section already provides SHA-256 plus FEC, so repeating portable SHA-256
// for every adjacent multi-megabyte Level frame is redundant. Bytes 0..3 of
// expandedHash contain little-endian CRC32C and bytes 4..31 are canonical 0.
constexpr std::uint32_t kHistoryNestedChecksum = 4u;
constexpr std::size_t kHeaderFieldsSize = 24;
constexpr std::size_t kCanonicalHeaderSize = kMagic.size() + kHeaderFieldsSize;
constexpr std::size_t kMinimumHeaderSize = kCanonicalHeaderSize;
static_assert(kCanonicalHeaderSize == 28);
constexpr std::size_t kHeaderVersionOffset = 4;
constexpr std::size_t kHeaderProjectOffset = 8;
constexpr std::size_t kHeaderSectionCountOffset = 12;
constexpr std::size_t kHeaderTimestampOffset = 16;
constexpr std::size_t kHeaderChecksumOffset = 24;
constexpr std::size_t kSectionDescriptorSize = kSectionAnchorMagic.size()
    + 4 + 4 + 4 + 8 + 8 + 8 + 4 + 4 + 4 + 4 + 4 + 32 + 4;
constexpr std::size_t kSectionHeaderSize = kSectionDescriptorSize;
constexpr std::size_t kSectionTrailerSize = kSectionMirrorMagic.size() + 4 + kSectionDescriptorSize + 4;
constexpr std::size_t kFooterSize = kFooterMagic.size();
constexpr std::size_t kHeaderMirrorSize = kHeaderMirrorMagic.size() + 4 + kCanonicalHeaderSize + 4;
constexpr std::uint32_t kRequiredSectionCount = 8;
constexpr std::uint32_t kFecDataStreams = 16;
constexpr std::uint32_t kFecOptionalParityStreams = 2;
constexpr std::uint32_t kFecRequiredParityStreams = 4;
constexpr std::uint32_t kFecMaximumStreams = kFecDataStreams + kFecRequiredParityStreams;
constexpr std::uint32_t kFecSlotBytes = static_cast<std::uint32_t>(SedFecSlotBytes);
constexpr std::uint32_t kFecSlotPayloadBytes =
    static_cast<std::uint32_t>(SedFecSlotPayloadBytes);
// Do not clear the 256 KiB matcher for tiny semantic records.  Required Level
// records remain independently protected even when compression is skipped.
constexpr std::size_t kCompressionMinimumBytes = 4u * 1024u;
// Keep the temporary LZSS output bounded. Attempting to compress an
// incompressible near-limit record would otherwise hold a second large
// allocation before the protected encoder can reject it.
constexpr std::size_t kCompressionMaximumBytes = 64u * 1024u * 1024u;
constexpr std::uint64_t kMaxFileBytes = SedMaximumFileBytes;
constexpr std::uint64_t kMaxSectionBytes = SedMaximumSemanticSectionBytes;
constexpr std::array<char, 4> kHistorySectionType{ 'h', 'i', 's', 't' };
constexpr std::array<char, 4> kJklCogImportSectionType{ 'j', 'i', 'm', 'p' };
constexpr std::uint64_t kMaxCodedSectionBytes = SedMaximumCodedSectionBytes;
constexpr std::uint32_t kMaxCollectionItems =
    static_cast<std::uint32_t>(SedMaximumDecodedItems);
static_assert(SedMaximumDecodedItems
    <= std::numeric_limits<std::uint32_t>::max());
constexpr std::uint32_t kMaxStringBytes = 16u * 1024u * 1024u;
// Raw semantic bytes are bounded independently above, but compact encodings
// such as millions of empty strings can expand substantially when materialized
// as C++ objects. Keep one admission budget for every Level decode. Recovery
// attempts reset the concurrently-live allowance while retaining the cumulative
// work allowance, so a failed strict candidate cannot multiply parser work.
constexpr std::uint64_t kMaxSemanticDecodedOwnedBytes =
    SedMaximumDecodedObjectBytes;
constexpr std::uint64_t kMaxSemanticDecodedItems = SedMaximumDecodedItems;
constexpr std::uint64_t kMaxSemanticRecoveryWorkItems =
    kMaxSemanticDecodedItems * 4ull;
constexpr std::uint64_t kMaxSemanticRecoveryWorkBytes =
    kMaxSemanticDecodedOwnedBytes * 4ull;
// MotS ARCHLIGHTING uses the same one-million-record admission budget as the
// JKL/NDY codec. Keep this stricter than the general SED collection ceiling so
// a native document cannot save successfully and fail during engine export.
constexpr std::size_t kMaxMotsArchLightingRecords = 1'000'000u;
constexpr std::array<std::byte, 4> kArchBakeBaselineTrailerMagic{
    std::byte{ 0x61 }, std::byte{ 0x62 }, std::byte{ 0x73 }, std::byte{ 0x6c } };
constexpr std::uint32_t kArchBakeBaselineTrailerVersion = 1u;
constexpr std::uint32_t kMaximumRecoveryMarkerCandidates = 8192u;
constexpr std::uint32_t kMaximumRecoveryStrictCandidates = 8u;
constexpr std::uint64_t kMaximumRecoveryDescriptorProbes = 8192u;
constexpr std::uint64_t kRecoveryScanPassAllowance = 12u;
constexpr std::uint64_t kRecoveryDecodePassAllowance = 4u;

struct SedArchLightingSnapshot
{
    std::vector<const MotsArchLightObject*> objects{};
    std::vector<std::int32_t> thingIndices{};
};

bool isAllZeroArchObject(const MotsArchLightObject& object) noexcept
{
    for (const MotsArchLightMesh& mesh : object.meshes)
    {
        for (const MotsArchLightVertex& vertex : mesh.vertices)
        {
            if (vertex.mono != 0.0f || vertex.red != 0.0f
                || vertex.green != 0.0f || vertex.blue != 0.0f)
            {
                return false;
            }
        }
    }
    return true;
}

SedArchLightingSnapshot makeSedArchLightingSnapshot(const Level& level)
{
    SedArchLightingSnapshot snapshot{};
    snapshot.thingIndices.assign(level.things.size(), -1);
    if (level.kind != ProjectType::MOTS || level.motsArchLighting.empty())
    {
        return snapshot;
    }

    // Native SED has no external Templates argument: app template resources
    // are deliberately not serialized as an implicit dependency. If a Thing
    // has no instance or Level-local inherited `thingflags`, an external
    // template may still provide the Arch flag (0x8). Treat that archived reference
    // as potentially active so saving cannot erase a valid external-template
    // binding. An explicit local/in-Level flag value is authoritative.
    std::vector<bool> referencedByActiveOrAmbiguousThing(
        level.motsArchLighting.size(), false);
    for (const Thing& thing : level.things)
    {
        const bool flagsKnown = sed::level::thingFindValue(
            level, thing, "thingflags") != nullptr;
        const bool archFlagSet =
            (sed::level::thingFlags(level, thing) & 0x8) != 0;
        if ((archFlagSet || !flagsKnown)
            && thing.archLightingIndex >= 0
            && static_cast<std::size_t>(thing.archLightingIndex)
                < level.motsArchLighting.size())
        {
            referencedByActiveOrAmbiguousThing[
                static_cast<std::size_t>(thing.archLightingIndex)] = true;
        }
    }

    std::vector<std::int32_t> remap(level.motsArchLighting.size(), -1);
    for (std::size_t objectIndex = 0;
         objectIndex < level.motsArchLighting.size(); ++objectIndex)
    {
        const MotsArchLightObject& object =
            level.motsArchLighting[objectIndex];
        if (isAllZeroArchObject(object)
            && !referencedByActiveOrAmbiguousThing[objectIndex])
        {
            continue;
        }
        remap[objectIndex] = static_cast<std::int32_t>(snapshot.objects.size());
        snapshot.objects.push_back(&object);
    }

    for (std::size_t thingIndex = 0; thingIndex < level.things.size(); ++thingIndex)
    {
        const Thing& thing = level.things[thingIndex];
        if (thing.archLightingIndex < 0
            || static_cast<std::size_t>(thing.archLightingIndex)
                >= level.motsArchLighting.size())
        {
            continue;
        }
        const MotsArchLightObject& object = level.motsArchLighting[
            static_cast<std::size_t>(thing.archLightingIndex)];
        const bool flagsKnown = sed::level::thingFindValue(
            level, thing, "thingflags") != nullptr;
        if (isAllZeroArchObject(object)
            && flagsKnown
            && (sed::level::thingFlags(level, thing) & 0x8) == 0)
        {
            continue;
        }
        snapshot.thingIndices[thingIndex] = remap[
            static_cast<std::size_t>(thing.archLightingIndex)];
    }
    return snapshot;
}

constexpr bool isSupportedSedBinaryVersion(
    const std::uint32_t version) noexcept
{
    return isSedBinaryVersionSupported(version);
}

std::uint32_t binaryVersionForLevel(const Level& level)
{
    if ((!level.motsArchLighting.empty()
            || !level.motsArchLightingBakeBaselines.empty())
        && level.kind != ProjectType::MOTS)
    {
        throw std::runtime_error(
            "Cannot save binary SED: ARCHLIGHTING is only valid for MotS.");
    }
    const bool hasSerializedArchLighting =
        !makeSedArchLightingSnapshot(level).objects.empty()
        || !level.motsArchLightingBakeBaselines.empty();
    switch (level.uvCoordinateSpace)
    {
    case UvCoordinateSpace::TexturePixels:
        return hasSerializedArchLighting
            ? SedArchPixelBinaryVersion : SedLegacyBinaryVersion;
    case UvCoordinateSpace::Normalized:
        return hasSerializedArchLighting
            ? SedArchNormalizedBinaryVersion : SedBinaryVersion;
    }
    throw std::runtime_error(
        "Cannot save binary SED: unknown Level UV coordinate space.");
}

constexpr bool isValidSerializationProfile(
    const SedSerializationProfile profile) noexcept
{
    return profile == SedSerializationProfile::ProtectedV1
        || profile == SedSerializationProfile::UnprotectedNested
        || profile == SedSerializationProfile::HistoryNested;
}

constexpr bool isNestedSerializationProfile(
    const SedSerializationProfile profile) noexcept
{
    return profile == SedSerializationProfile::UnprotectedNested
        || profile == SedSerializationProfile::HistoryNested;
}

void requireValidSerializationProfile(const SedSerializationProfile profile)
{
    if (!isValidSerializationProfile(profile))
    {
        throw std::invalid_argument(
            "Binary SED serialization profile is invalid.");
    }
}

std::string pendingJklCogName(const Level& level)
{
    for (const Cog& cog : level.cogs)
    {
        for (const CogValue& value : cog.values)
        {
            if (value.type == ValueType::str && value.cogType == CogType::unk)
            {
                return cog.name.empty() ? std::string("<unnamed>") : cog.name;
            }
        }
    }
    return "<unnamed>";
}

[[noreturn]] void fail(std::string_view detail);

bool isRawJklCogValue(
    const CogValue& value,
    const std::size_t parameterIndex) noexcept
{
    if ( value.type != ValueType::str || value.cogType != CogType::unk
        || value.name.size() <= 5u
        || value.name.compare(0u, 5u, "param") != 0 )
    {
        return false;
    }
    if ( value.name.size() > 6u && value.name[5] == '0' )
    {
        return false;
    }
    std::size_t parsed{};
    for ( std::size_t index = 5u; index < value.name.size(); ++index )
    {
        const char character = value.name[index];
        if ( character < '0' || character > '9' )
        {
            return false;
        }
        const std::size_t digit = static_cast<std::size_t>(character - '0');
        if ( parsed > (std::numeric_limits<std::size_t>::max() - digit) / 10u )
        {
            return false;
        }
        parsed = parsed * 10u + digit;
    }
    return parsed == parameterIndex;
}

bool hasRawJklCogRows(const Level& level) noexcept
{
    for ( const Cog& cog : level.cogs )
    {
        for ( std::size_t index = 0; index < cog.values.size(); ++index )
        {
            if ( isRawJklCogValue(cog.values[index], index) ) return true;
        }
    }
    return false;
}

bool hasRepresentedJklCogImportStateInternal(const Level& level) noexcept
{
    return level.importedJklCogRowsPresent
        || (!level.importedJklSurfaceRefs.empty()
            && hasRawJklCogRows(level));
}

bool sameRepresentedJklCogImportState(
    const Level& lhs,
    const Level& rhs) noexcept
{
    const bool lhsRepresented = hasRepresentedJklCogImportStateInternal(lhs);
    const bool rhsRepresented = hasRepresentedJklCogImportStateInternal(rhs);
    if ( !lhsRepresented && !rhsRepresented ) return true;
    return lhs.importedJklCogRowsPresent == rhs.importedJklCogRowsPresent
        && lhs.importedJklSurfaceRefs == rhs.importedJklSurfaceRefs;
}

bool sameRepresentedJklCogImportState(
    const Level& level,
    const bool rowsPresent,
    const std::span<const SurfaceRef> surfaceRefs) noexcept
{
    const bool levelRepresented = hasRepresentedJklCogImportStateInternal(level);
    const bool stateRepresented = rowsPresent
        || (!surfaceRefs.empty() && hasRawJklCogRows(level));
    if ( !levelRepresented && !stateRepresented ) return true;
    return level.importedJklCogRowsPresent == rowsPresent
        && std::equal(level.importedJklSurfaceRefs.begin(),
            level.importedJklSurfaceRefs.end(),
            surfaceRefs.begin(), surfaceRefs.end());
}

bool sameRepresentedJklCogImportState(
    const Level& level,
    const SedJklCogImportState& state) noexcept
{
    return sameRepresentedJklCogImportState(
        level, state.cogRowsPresent, state.surfaceRefs);
}

void validateJklCogImportSurfaceRefs(
    const Level& level,
    const std::span<const SurfaceRef> refs)
{
    for (const SurfaceRef& ref : refs)
    {
        if ( ref.sector < 0
            || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
        {
            fail("JKL COG import Surface sector reference is out of range");
        }
        const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
        if ( ref.surface < 0
            || static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
        {
            fail("JKL COG import Surface reference is out of range");
        }
    }
}

[[noreturn]] void fail(const std::string_view detail)
{
    throw std::runtime_error(std::string("Invalid binary SED file: ") + std::string(detail));
}

std::uint32_t encodeSedIndex(
    const std::optional<std::uint32_t> index)
{
    // SED V1 reserves biased wire value zero for not-set and maps real index n
    // to n+1. This makes the common sentinel a one-byte ULEB128 value; directly
    // converting the Level model's -1 to unsigned would produce UINT32_MAX and
    // waste five bytes. The bias also leaves indices 0..126 at one byte, while
    // UINT32_MAX itself cannot be an index in this 32-bit wire type.
    if (index && *index == std::numeric_limits<std::uint32_t>::max())
    {
        throw std::runtime_error(
            "Binary SED index exceeds the biased 32-bit wire range.");
    }
    return index ? *index + 1u : 0u;
}

std::optional<std::uint32_t> decodeSedIndex(
    const std::uint32_t biasedWireValue) noexcept
{
    // The inverse mapping is total over all 2^32 wire codes: zero is not-set;
    // codes 1..UINT32_MAX name indices 0..UINT32_MAX-1.
    if (biasedWireValue == 0) return std::nullopt;
    return biasedWireValue - 1u;
}

class RecoveryBudgetExceeded final : public std::runtime_error
{
public:
    explicit RecoveryBudgetExceeded(const std::string_view detail)
        : std::runtime_error(std::string("Invalid binary SED file: ")
            + std::string(detail))
    {
    }
};

SedLevelDecodedFootprint semanticRootFootprint() noexcept
{
    return SedLevelDecodedFootprint{ sizeof(Level), 1u };
}

SedLevelDecodedFootprint semanticVectorFootprint(const std::uint64_t count,
    const std::uint64_t elementBytes)
{
    if (elementBytes != 0
        && count > std::numeric_limits<std::uint64_t>::max() / elementBytes)
    {
        fail("semantic collection storage size overflows");
    }
    return SedLevelDecodedFootprint{ count * elementBytes, count };
}

SedLevelDecodedFootprint semanticStringFootprint(const std::uint64_t bytes)
{
    if (bytes == std::numeric_limits<std::uint64_t>::max())
    {
        fail("semantic string storage size overflows");
    }
    // Account for the owned terminator as well as the characters. This is
    // deliberately conservative for small-string implementations, which may
    // keep the bytes inline in an already charged parent object.
    return SedLevelDecodedFootprint{ bytes + 1u, 0u };
}

void addSemanticFootprint(SedLevelDecodedFootprint& destination,
    const SedLevelDecodedFootprint addition)
{
    if (!tryAccumulateSedLevelDecodedFootprint(destination, addition))
    {
        fail("semantic decoded-footprint counter overflows");
    }
}

void enforceSemanticFootprintLimits(
    const SedLevelDecodedFootprint footprint)
{
    if (footprint.ownedBytes > kMaxSemanticDecodedOwnedBytes)
    {
        fail("semantic decoded-object storage exceeds the safe limit");
    }
    if (footprint.itemCount > kMaxSemanticDecodedItems)
    {
        fail("semantic decoded-item work exceeds the safe limit");
    }
}

struct SemanticDecodeBudget final
{
    void beginDocument()
    {
        footprint = {};
        consume(semanticRootFootprint(), 0u);
    }

    void consumeVector(const std::uint64_t count,
        const std::uint64_t elementBytes)
    {
        consume(semanticVectorFootprint(count, elementBytes), 0u);
    }

    void consumeString(const std::uint64_t bytes)
    {
        consume(semanticStringFootprint(bytes), bytes);
    }

private:
    void consume(const SedLevelDecodedFootprint addition,
        const std::uint64_t stringWorkBytes)
    {
        SedLevelDecodedFootprint admitted = footprint;
        addSemanticFootprint(admitted, addition);
        enforceSemanticFootprintLimits(admitted);
        if (addition.itemCount > workItemsRemaining)
        {
            fail("semantic decoded-item work exceeds the safe limit");
        }
        if (stringWorkBytes > workBytesRemaining)
        {
            fail("semantic string work exceeds the safe limit");
        }
        footprint = admitted;
        workItemsRemaining -= addition.itemCount;
        workBytesRemaining -= stringWorkBytes;
    }

    SedLevelDecodedFootprint footprint{};
    std::uint64_t workItemsRemaining{ kMaxSemanticRecoveryWorkItems };
    std::uint64_t workBytesRemaining{ kMaxSemanticRecoveryWorkBytes };
};

// All permissive discovery and resynchronization for one untrusted input
// shares this budget.  Exact marker searches keep random bytes cheap, while
// the candidate ceiling prevents a marker flood from requesting CRC32C over
// millions of fixed descriptors/trailers.  The byte ceilings also make future
// recovery branches fail closed instead of accidentally adding unlimited
// overlapping passes.
struct RecoveryWorkBudget final
{
    explicit RecoveryWorkBudget(const std::size_t inputBytes) noexcept
        : scanBytesRemaining(static_cast<std::uint64_t>(inputBytes)
              * kRecoveryScanPassAllowance)
        , decodeBytesRemaining(static_cast<std::uint64_t>(inputBytes)
                  * kRecoveryDecodePassAllowance
              + kFecSlotBytes)
        , strictValidationBytesRemaining(static_cast<std::uint64_t>(inputBytes) * 2u)
    {
    }

    void consumeScan(const std::size_t bytes)
    {
        if (bytes > scanBytesRemaining)
        {
            throw RecoveryBudgetExceeded("recovery scan work exceeds the safe limit");
        }
        scanBytesRemaining -= bytes;
    }

    void consumeMarkerCandidate()
    {
        if (markerCandidates == kMaximumRecoveryMarkerCandidates)
        {
            throw RecoveryBudgetExceeded(
                "recovery marker candidate limit exceeded");
        }
        ++markerCandidates;
    }

    void consumeDecode(const std::size_t bytes)
    {
        if (bytes > decodeBytesRemaining)
        {
            throw RecoveryBudgetExceeded("recovery decode work exceeds the safe limit");
        }
        decodeBytesRemaining -= bytes;
    }

    void consumeStrictCandidate()
    {
        if (strictCandidates == kMaximumRecoveryStrictCandidates)
        {
            throw RecoveryBudgetExceeded(
                "recovery strict-candidate limit exceeded");
        }
        ++strictCandidates;
    }

    bool consumeStrictValidationBytes(const std::size_t bytes) noexcept
    {
        if (bytes > strictValidationBytesRemaining) return false;
        strictValidationBytesRemaining -= bytes;
        return true;
    }

    void consumeDescriptorProbes(const std::uint32_t count)
    {
        if (count > kMaximumRecoveryDescriptorProbes - descriptorProbes)
        {
            throw RecoveryBudgetExceeded(
                "recovery descriptor-probe limit exceeded");
        }
        descriptorProbes += count;
    }

    std::uint64_t scanBytesRemaining{};
    std::uint64_t decodeBytesRemaining{};
    std::uint64_t strictValidationBytesRemaining{};
    std::uint64_t descriptorProbes{};
    std::uint32_t markerCandidates{};
    std::uint32_t strictCandidates{};
};

template <std::size_t Size>
std::optional<std::size_t> findExactRecoveryMarker(
    const std::span<const std::byte> bytes,
    const std::array<std::byte, Size>& marker,
    const std::size_t start, const std::size_t end,
    RecoveryWorkBudget* const budget)
{
    const std::size_t boundedEnd = std::min(end, bytes.size());
    if (start >= boundedEnd || Size > boundedEnd - start)
    {
        if (budget && start < boundedEnd) budget->consumeScan(boundedEnd - start);
        return std::nullopt;
    }
    const auto first = bytes.begin() + static_cast<std::ptrdiff_t>(start);
    const auto last = bytes.begin() + static_cast<std::ptrdiff_t>(boundedEnd);
    const auto found = std::search(first, last, marker.begin(), marker.end());
    const std::size_t scanned = found == last
        ? boundedEnd - start
        : static_cast<std::size_t>(std::distance(first, found)) + Size;
    if (budget) budget->consumeScan(scanned);
    if (found == last) return std::nullopt;
    if (budget) budget->consumeMarkerCandidate();
    return static_cast<std::size_t>(std::distance(bytes.begin(), found));
}

constexpr std::optional<std::size_t> checkedRecoveryFramingSize(
    const std::size_t sectionBytes, const bool survivingMirror) noexcept
{
    const std::uint64_t overhead = kCanonicalHeaderSize
        + (survivingMirror ? 0u : kHeaderMirrorSize + kFooterSize);
    if (sectionBytes > kMaxFileBytes - overhead) return std::nullopt;
    return sectionBytes + static_cast<std::size_t>(overhead);
}

static_assert(checkedRecoveryFramingSize(
    static_cast<std::size_t>(kMaxFileBytes - kCanonicalHeaderSize), true)
    == kMaxFileBytes);
static_assert(!checkedRecoveryFramingSize(
    static_cast<std::size_t>(kMaxFileBytes - kCanonicalHeaderSize + 1u), true));
static_assert(checkedRecoveryFramingSize(static_cast<std::size_t>(kMaxFileBytes
        - kCanonicalHeaderSize - kHeaderMirrorSize - kFooterSize), false)
    == kMaxFileBytes);
static_assert(!checkedRecoveryFramingSize(static_cast<std::size_t>(kMaxFileBytes
        - kCanonicalHeaderSize - kHeaderMirrorSize - kFooterSize + 1u), false));

constexpr bool isAdmittedSedFileSize(const std::uint64_t fileSize) noexcept
{
    return fileSize != 0 && fileSize <= kMaxFileBytes
        && fileSize <= std::numeric_limits<std::size_t>::max();
}

class IntegrityFailure : public std::runtime_error
{
public:
    explicit IntegrityFailure(const std::string_view detail)
        : std::runtime_error(std::string("Corrupted binary SED file: ")
            + std::string(detail))
    {
    }
};

// A malformed LZSS token stream is data corruption, not a generic exception
// from the decoder.  Keeping this type distinct means permissive recovery can
// drop exactly the affected semantic section without swallowing allocation or
// programming failures.
class MalformedCompression final : public IntegrityFailure
{
public:
    explicit MalformedCompression(const std::string_view detail)
        : IntegrityFailure(detail)
    {
    }
};

[[noreturn]] void compressionFail(const std::string_view detail)
{
    throw MalformedCompression(detail);
}

[[noreturn]] void integrityFail(const std::string_view detail)
{
    throw IntegrityFailure(detail);
}

std::optional<std::size_t> sedMagicSize(const std::span<const std::byte> bytes) noexcept
{
    if (bytes.size() >= kMagic.size()
        && std::equal(kMagic.begin(), kMagic.end(), bytes.begin()))
    {
        return kMagic.size();
    }
    return std::nullopt;
}

const std::array<std::uint32_t, 256>& crc32cTable() noexcept
{
    static const std::array<std::uint32_t, 256> table = []
    {
        std::array<std::uint32_t, 256> values{};
        for (std::uint32_t i = 0; i < values.size(); ++i)
        {
            std::uint32_t value = i;
            for (unsigned bit = 0; bit < 8; ++bit)
            {
                value = (value >> 1u) ^ (0x82f6'3b78u & (0u - (value & 1u)));
            }
            values[i] = value;
        }
        return values;
    }();
    return table;
}

std::uint32_t crc32cPortable(const std::span<const std::byte> bytes) noexcept
{
    const auto& table = crc32cTable();
    std::uint32_t crc = 0xffff'ffffu;
    for (const std::byte value : bytes)
    {
        crc = table[(crc ^ std::to_integer<std::uint8_t>(value)) & 0xffu]
            ^ (crc >> 8u);
    }
    return ~crc;
}

#if (defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))) \
    || ((defined(__GNUC__) || defined(__clang__)) \
        && (defined(__x86_64__) || defined(__i386__)))
#define SED_HAS_X86_CRC32C 1

bool hasHardwareCrc32cImpl() noexcept
{
#if defined(_MSC_VER)
    static const bool available = []
    {
        std::array<int, 4> registers{};
        __cpuid(registers.data(), 1);
        return (registers[2] & (1 << 20)) != 0;
    }();
    return available;
#else
    static const bool available = []
    {
        __builtin_cpu_init();
        return __builtin_cpu_supports("sse4.2") != 0;
    }();
    return available;
#endif
}

#if (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)
__attribute__((target("sse4.2")))
#endif
std::uint32_t crc32cHardware(const std::span<const std::byte> bytes) noexcept
{
    std::uint64_t crc = 0xffff'ffffu;
    std::size_t offset{};
#if defined(_M_X64) || defined(__x86_64__)
    while (bytes.size() - offset >= sizeof(std::uint64_t))
    {
        std::uint64_t value{};
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        crc = _mm_crc32_u64(crc, value);
        offset += sizeof(value);
    }
#else
    while (bytes.size() - offset >= sizeof(std::uint32_t))
    {
        std::uint32_t value{};
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        crc = _mm_crc32_u32(static_cast<std::uint32_t>(crc), value);
        offset += sizeof(value);
    }
#endif
    while (offset < bytes.size())
    {
        crc = _mm_crc32_u8(static_cast<std::uint32_t>(crc),
            std::to_integer<std::uint8_t>(bytes[offset++]));
    }
    return ~static_cast<std::uint32_t>(crc);
}
#else
#define SED_HAS_X86_CRC32C 0

bool hasHardwareCrc32cImpl() noexcept
{
    return false;
}
#endif

std::uint32_t crc32c(const std::span<const std::byte> bytes) noexcept
{
#if SED_HAS_X86_CRC32C
    if (hasHardwareCrc32cImpl()) return crc32cHardware(bytes);
#endif
    return crc32cPortable(bytes);
}

using Digest256 = std::array<std::byte, 32>;

class Sha256 final
{
public:
    Sha256() noexcept
        : state_{ 0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u }
    {
    }

    void update(const std::span<const std::byte> bytes) noexcept
    {
        std::size_t offset{};
        bitLength_ += static_cast<std::uint64_t>(bytes.size()) * 8u;
        while (offset < bytes.size())
        {
            const std::size_t count = std::min<std::size_t>(64 - buffered_, bytes.size() - offset);
            std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), count,
                buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_));
            buffered_ += count;
            offset += count;
            if (buffered_ == 64)
            {
                transform(buffer_);
                buffered_ = 0;
            }
        }
    }

    Digest256 finish() noexcept
    {
        buffer_[buffered_++] = std::byte{0x80};
        if (buffered_ > 56)
        {
            while (buffered_ < 64) buffer_[buffered_++] = std::byte{0};
            transform(buffer_);
            buffered_ = 0;
        }
        while (buffered_ < 56) buffer_[buffered_++] = std::byte{0};
        for (unsigned i = 0; i < 8; ++i)
        {
            buffer_[63 - i] = static_cast<std::byte>(bitLength_ >> (i * 8));
        }
        transform(buffer_);

        Digest256 digest{};
        for (unsigned i = 0; i < state_.size(); ++i)
        {
            for (unsigned byte = 0; byte < 4; ++byte)
            {
                digest[i * 4 + byte] = static_cast<std::byte>(
                    state_[i] >> (24 - byte * 8));
            }
        }
        return digest;
    }

private:
    static constexpr std::uint32_t rotr(const std::uint32_t value, const unsigned amount) noexcept
    {
        return (value >> amount) | (value << (32 - amount));
    }

    void transform(const std::array<std::byte, 64>& block) noexcept
    {
        constexpr std::array<std::uint32_t, 64> k{
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
            0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
            0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
            0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
            0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
            0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
            0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
            0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
            0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
            0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u };
        std::array<std::uint32_t, 64> w{};
        for (unsigned i = 0; i < 16; ++i)
        {
            w[i] = (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[i * 4])) << 24u)
                | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[i * 4 + 1])) << 16u)
                | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[i * 4 + 2])) << 8u)
                | static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[i * 4 + 3]));
        }
        for (unsigned i = 16; i < 64; ++i)
        {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (unsigned i = 0; i < 64; ++i)
        {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;
            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{};
    std::array<std::byte, 64> buffer_{};
    std::size_t buffered_{};
    std::uint64_t bitLength_{};
};

Digest256 sha256(const std::span<const std::byte> bytes) noexcept
{
    Sha256 hasher{};
    hasher.update(bytes);
    return hasher.finish();
}

Digest256 historyNestedChecksum(
    const std::span<const std::byte> bytes) noexcept
{
    Digest256 digest{};
    const std::uint32_t checksum = crc32c(bytes);
    digest[0] = static_cast<std::byte>(checksum & 0xffu);
    digest[1] = static_cast<std::byte>((checksum >> 8u) & 0xffu);
    digest[2] = static_cast<std::byte>((checksum >> 16u) & 0xffu);
    digest[3] = static_cast<std::byte>((checksum >> 24u) & 0xffu);
    return digest;
}

struct Gf256Tables final
{
    std::array<std::uint8_t, 512> exp{};
    std::array<std::uint8_t, 256> log{};
    // A direct product table keeps the hot parity loops independent of the
    // logarithm/exponent lookups. Its 64 KiB footprint is bounded and shared
    // by all sections, while avoiding billions of duplicate GF(256) index
    // calculations when a large Level is encoded or repaired.
    std::array<std::array<std::uint8_t, 256>, 256> mul{};

    Gf256Tables()
    {
        unsigned value = 1;
        for (unsigned i = 0; i < 255; ++i)
        {
            exp[i] = static_cast<std::uint8_t>(value);
            log[value] = static_cast<std::uint8_t>(i);
            value <<= 1u;
            if ((value & 0x100u) != 0) value ^= 0x11du;
        }
        for (unsigned i = 255; i < exp.size(); ++i) exp[i] = exp[i - 255];
        for (unsigned lhs = 0; lhs < 256; ++lhs)
        {
            for (unsigned rhs = 0; rhs < 256; ++rhs)
            {
                mul[lhs][rhs] = lhs == 0 || rhs == 0 ? 0u
                    : exp[static_cast<std::size_t>(log[lhs]) + log[rhs]];
            }
        }
    }
};

const Gf256Tables& gf256Tables()
{
    static const Gf256Tables tables{};
    return tables;
}

std::uint8_t gfMul(const std::uint8_t lhs, const std::uint8_t rhs) noexcept
{
    return gf256Tables().mul[lhs][rhs];
}

void xorScaledGf256(std::span<std::byte> output,
    const std::span<const std::byte> input,
    const std::uint8_t coefficient) noexcept
{
    if (coefficient == 0 || output.size() != input.size()) return;
    if (coefficient == 1)
    {
        for (std::size_t index = 0; index < output.size(); ++index)
        {
            output[index] ^= input[index];
        }
        return;
    }

    // A parity row applies one fixed coefficient to an entire 4 KiB slot.
    // Cache that multiplication row once instead of calling gfMul() (and its
    // function-local table accessor) for every byte. The lookup remains the
    // exact same GF(256) operation and therefore preserves canonical bytes.
    const auto& products = gf256Tables().mul[coefficient];
    for (std::size_t index = 0; index < output.size(); ++index)
    {
        output[index] ^= static_cast<std::byte>(
            products[std::to_integer<std::uint8_t>(input[index])]);
    }
}

std::uint8_t gfDiv(const std::uint8_t lhs, const std::uint8_t rhs) noexcept
{
    if (lhs == 0) return 0;
    if (rhs == 0) return 0;
    const auto& tables = gf256Tables();
    int exponent = static_cast<int>(tables.log[lhs]) - static_cast<int>(tables.log[rhs]);
    if (exponent < 0) exponent += 255;
    return tables.exp[static_cast<std::size_t>(exponent)];
}

std::uint8_t gfPow(const std::uint8_t base, const std::size_t exponent) noexcept
{
    if (exponent == 0) return 1;
    if (base == 0) return 0;
    const auto& tables = gf256Tables();
    return tables.exp[(static_cast<std::size_t>(tables.log[base]) * exponent) % 255u];
}

using GfSquareMatrix = std::array<std::array<std::uint8_t, kFecDataStreams>,
    kFecDataStreams>;
using GfGeneratorMatrix = std::array<std::array<std::uint8_t, kFecDataStreams>,
    kFecMaximumStreams>;

GfSquareMatrix invertGfMatrix(GfSquareMatrix matrix, const std::size_t size)
{
    if (size == 0 || size > kFecDataStreams)
    {
        throw std::logic_error("Binary SED Reed-Solomon matrix size is invalid.");
    }
    GfSquareMatrix inverse{};
    for (std::size_t row = 0; row < size; ++row)
    {
        inverse[row][row] = 1;
    }

    for (std::size_t column = 0; column < size; ++column)
    {
        std::size_t pivot = column;
        while (pivot < size && matrix[pivot][column] == 0) ++pivot;
        if (pivot == size)
        {
            throw std::logic_error("Binary SED Reed-Solomon matrix is singular.");
        }
        if (pivot != column)
        {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(inverse[pivot], inverse[column]);
        }

        const std::uint8_t pivotValue = matrix[column][column];
        if (pivotValue != 1)
        {
            const std::uint8_t reciprocal = gfDiv(1, pivotValue);
            for (std::size_t item = 0; item < size; ++item)
            {
                matrix[column][item] = gfMul(matrix[column][item], reciprocal);
                inverse[column][item] = gfMul(inverse[column][item], reciprocal);
            }
        }

        for (std::size_t row = 0; row < size; ++row)
        {
            if (row == column || matrix[row][column] == 0) continue;
            const std::uint8_t factor = matrix[row][column];
            for (std::size_t item = 0; item < size; ++item)
            {
                matrix[row][item] ^= gfMul(factor, matrix[column][item]);
                inverse[row][item] ^= gfMul(factor, inverse[column][item]);
            }
        }
    }
    return inverse;
}

const GfGeneratorMatrix& fecGeneratorMatrix(const std::size_t dataStreams)
{
    static const std::array<GfGeneratorMatrix, kFecDataStreams + 1> generators = []
    {
        std::array<GfGeneratorMatrix, kFecDataStreams + 1> result{};
        for (std::size_t dataCount = 1; dataCount <= kFecDataStreams; ++dataCount)
        {
            GfGeneratorMatrix vandermonde{};
            const std::size_t streamCount = dataCount + kFecRequiredParityStreams;
            for (std::size_t row = 0; row < streamCount; ++row)
            {
                for (std::size_t column = 0; column < dataCount; ++column)
                {
                    vandermonde[row][column] = gfPow(static_cast<std::uint8_t>(row), column);
                }
            }

            GfSquareMatrix top{};
            std::copy_n(vandermonde.begin(), dataCount, top.begin());
            const GfSquareMatrix inverseTop = invertGfMatrix(top, dataCount);
            for (std::size_t row = 0; row < streamCount; ++row)
            {
                for (std::size_t column = 0; column < dataCount; ++column)
                {
                    std::uint8_t value{};
                    for (std::size_t item = 0; item < dataCount; ++item)
                    {
                        value ^= gfMul(vandermonde[row][item], inverseTop[item][column]);
                    }
                    result[dataCount][row][column] = value;
                }
            }
            for (std::size_t row = 0; row < dataCount; ++row)
            {
                for (std::size_t column = 0; column < dataCount; ++column)
                {
                    if (result[dataCount][row][column]
                        != static_cast<std::uint8_t>(row == column))
                    {
                        throw std::logic_error(
                            "Binary SED Reed-Solomon generator is not systematic.");
                    }
                }
            }
        }
        return result;
    }();
    if (dataStreams == 0 || dataStreams > kFecDataStreams)
    {
        throw std::logic_error("Binary SED Reed-Solomon data-stream count is invalid.");
    }
    return generators[dataStreams];
}

class Writer final
{
public:
    explicit Writer(const std::size_t maximumBytes = kMaxFileBytes)
        : maximumBytes_(maximumBytes)
    {
    }

    const Bytes& bytes() const noexcept { return bytes_; }
    Bytes take() noexcept { return std::move(bytes_); }

    // Reserve only after the caller has checked the complete destination
    // extent against this writer's maximum. Grow geometrically rather than
    // reserving the exact next section: the semantic sections are appended in
    // a fixed order, and exact reservations would recopy one large SECT for
    // every subsequent small section. The lower growth floor keeps tiny
    // trailing sections from causing a long sequence of reallocations.
    void reserveAtLeast(const std::size_t requested)
    {
        if (requested > maximumBytes_)
        {
            throw std::runtime_error("Binary SED output is too large.");
        }
        if (requested <= bytes_.capacity()) return;
        constexpr std::size_t kMinimumGrowth = 64u * 1024u;
        const std::size_t currentCapacity = bytes_.capacity();
        const std::size_t geometricGrowth = std::max<std::size_t>(
            currentCapacity / 2u, kMinimumGrowth);
        const std::size_t growthCapacity = geometricGrowth
            > maximumBytes_ - currentCapacity
            ? maximumBytes_ : currentCapacity + geometricGrowth;
        bytes_.reserve(std::max(requested, growthCapacity));
    }

    void raw(const std::span<const std::byte> value)
    {
        if (value.size() > maximumBytes_
                || bytes_.size() > maximumBytes_ - value.size())
        {
            throw std::runtime_error("Binary SED output is too large.");
        }
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }

    void u8(const std::uint8_t value)
    {
        if (bytes_.size() >= maximumBytes_)
        {
            throw std::runtime_error("Binary SED output is too large.");
        }
        bytes_.push_back(static_cast<std::byte>(value));
    }
    void boolean(const bool value) { u8(value ? 1u : 0u); }

    void u32(const std::uint32_t value)
    {
        if (bytes_.size() > maximumBytes_
            || maximumBytes_ - bytes_.size() < 4)
        {
            throw std::runtime_error("Binary SED output is too large.");
        }
        const std::size_t offset = bytes_.size();
        bytes_.resize(offset + 4);
        for (unsigned shift = 0; shift < 32; shift += 8)
        {
            bytes_[offset + shift / 8] = static_cast<std::byte>(
                static_cast<std::uint8_t>(value >> shift));
        }
    }

    void i32(const std::int32_t value) { u32(std::bit_cast<std::uint32_t>(value)); }

    /** Writes an unsigned 32-bit value as canonical ULEB128. */
    void ulebU32(std::uint32_t value)
    {
        do
        {
            std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7fu);
            value >>= 7;
            if (value != 0) byte |= 0x80u;
            u8(byte);
        }
        while (value != 0);
    }

    /** Writes a sentinel-capable collection index as biased ULEB128.
     *
     * Wire zero is the sole -1 empty/not-set sentinel. A non-negative index
     * n is written as unsigned n+1, keeping the sentinel and indices 0..126
     * at one byte. Without this mapping, converting -1 directly to unsigned
     * would encode UINT32_MAX and consume five ULEB128 bytes for every empty
     * reference.
     */
    void writeSedIndex(const std::int32_t value)
    {
        // Convert the Level model's -1 sentinel to an unsigned optional, then
        // apply the standard SED index bias before writing canonical ULEB128.
        if (value < -1)
        {
            throw std::runtime_error(
                "Binary SED index is below the reserved -1 sentinel.");
        }
        const std::optional<std::uint32_t> index = value == -1
            ? std::nullopt
            : std::optional<std::uint32_t>(
                static_cast<std::uint32_t>(value));
        ulebU32(encodeSedIndex(index));
    }

    void u64(const std::uint64_t value)
    {
        if (bytes_.size() > maximumBytes_
            || maximumBytes_ - bytes_.size() < 8)
        {
            throw std::runtime_error("Binary SED output is too large.");
        }
        const std::size_t offset = bytes_.size();
        bytes_.resize(offset + 8);
        for (unsigned shift = 0; shift < 64; shift += 8)
        {
            bytes_[offset + shift / 8] = static_cast<std::byte>(
                static_cast<std::uint8_t>(value >> shift));
        }
    }

    void i64(const std::int64_t value) { u64(std::bit_cast<std::uint64_t>(value)); }

    void f32(const float value) { u32(std::bit_cast<std::uint32_t>(value)); }
    void f64(const double value) { u64(std::bit_cast<std::uint64_t>(value)); }

    void string(const std::string& value)
    {
        if (value.size() > kMaxStringBytes)
        {
            throw std::runtime_error("Binary SED string exceeds the safe size limit.");
        }
        u32(static_cast<std::uint32_t>(value.size()));
        raw(std::as_bytes(std::span(value.data(), value.size())));
    }

    template <typename T, typename Encode>
    void vector(const std::vector<T>& values, Encode encode)
    {
        if (values.size() > kMaxCollectionItems)
        {
            throw std::runtime_error("Binary SED collection exceeds the safe item limit.");
        }
        u32(static_cast<std::uint32_t>(values.size()));
        for (const T& value : values) encode(*this, value);
    }

private:
    Bytes bytes_{};
    std::size_t maximumBytes_{};
};

class Reader final
{
public:
    explicit Reader(const std::span<const std::byte> bytes,
        SemanticDecodeBudget* const semanticBudget = nullptr)
        : bytes_(bytes), semanticBudget_(semanticBudget)
    {
    }

    std::size_t remaining() const noexcept { return bytes_.size() - offset_; }
    std::size_t position() const noexcept { return offset_; }
    bool empty() const noexcept { return remaining() == 0; }
    bool startsWith(const std::span<const std::byte> prefix) const noexcept
    {
        return prefix.size() <= remaining()
            && std::equal(prefix.begin(), prefix.end(), bytes_.begin() + static_cast<std::ptrdiff_t>(offset_));
    }

    std::span<const std::byte> raw(const std::size_t size)
    {
        if (size > remaining()) fail("truncated data");
        const auto result = bytes_.subspan(offset_, size);
        offset_ += size;
        return result;
    }

    Reader bounded(const std::size_t size)
    {
        return Reader(raw(size), semanticBudget_);
    }

    std::uint8_t u8() { return std::to_integer<std::uint8_t>(raw(1)[0]); }
    bool boolean()
    {
        const std::uint8_t value = u8();
        if (value > 1) fail("invalid Boolean value");
        return value != 0;
    }

    std::uint32_t u32()
    {
        const auto value = raw(4);
        std::uint32_t result{};
        for (unsigned i = 0; i < 4; ++i)
        {
            result |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(value[i])) << (i * 8);
        }
        return result;
    }

    std::int32_t i32() { return std::bit_cast<std::int32_t>(u32()); }

    std::uint32_t ulebU32()
    {
        std::uint32_t encoded{};
        for (unsigned byteIndex = 0; byteIndex < 5; ++byteIndex)
        {
            const std::uint8_t byte = u8();
            if (byteIndex == 4 && (byte & 0xf0u) != 0)
            {
                fail("ULEB128 integer exceeds 32 bits");
            }
            encoded |= static_cast<std::uint32_t>(byte & 0x7fu)
                << (byteIndex * 7);
            if ((byte & 0x80u) == 0)
            {
                if (byteIndex != 0 && (byte & 0x7fu) == 0)
                {
                    fail("ULEB128 integer is not canonically encoded");
                }
                return encoded;
            }
        }
        fail("ULEB128 integer is overlong");
    }


    std::int32_t readSedIndex()
    {
        // Read canonical ULEB128 first, then apply the standard SED index
        // decoder. The current Level model is signed and therefore admits a
        // smaller range than the unsigned V1 wire mapping can describe.
        const std::optional<std::uint32_t> value = decodeSedIndex(ulebU32());
        if (!value) return -1;
        if (*value > static_cast<std::uint32_t>(
                std::numeric_limits<std::int32_t>::max()))
        {
            fail("index exceeds the current Level model range");
        }
        return static_cast<std::int32_t>(*value);
    }

    std::uint64_t u64()
    {
        const auto value = raw(8);
        std::uint64_t result{};
        for (unsigned i = 0; i < 8; ++i)
        {
            result |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(value[i])) << (i * 8);
        }
        return result;
    }

    std::int64_t i64() { return std::bit_cast<std::int64_t>(u64()); }

    float f32() { return std::bit_cast<float>(u32()); }
    double f64() { return std::bit_cast<double>(u64()); }

    std::string string()
    {
        const std::uint32_t size = u32();
        if (size > kMaxStringBytes) fail("string exceeds the safe size limit");
        if (semanticBudget_) semanticBudget_->consumeString(size);
        const auto value = raw(size);
        return std::string(reinterpret_cast<const char*>(value.data()), value.size());
    }

    std::string stringReference(const std::vector<std::string>* const strings,
        const bool allowMissingTable = false)
    {
        const std::int32_t reference = readSedIndex();
        if (reference == -1) return {};
        if (reference < -1) fail("resource string index is invalid");
        if (strings == nullptr)
        {
            if (!allowMissingTable) fail("resource string table is unavailable");
            // Partial corruption recovery keeps numeric/geometry domains even
            // when `rsrc` is beyond repair. The missing required section is
            // reported separately; unresolved spellings are deliberately
            // empty rather than fabricated from an untrusted index.
            return {};
        }
        if (static_cast<std::size_t>(reference) >= strings->size())
        {
            fail("resource string index is out of range");
        }
        const std::string& value = (*strings)[static_cast<std::size_t>(reference)];
        if (semanticBudget_) semanticBudget_->consumeString(value.size());
        return value;
    }

    template <typename T, typename Decode>
    std::vector<T> vector(Decode decode)
    {
        const std::uint32_t count = u32();
        if (count > kMaxCollectionItems) fail("collection exceeds the safe item limit");
        if (semanticBudget_)
        {
            semanticBudget_->consumeVector(count, sizeof(T));
        }
        std::vector<T> result{};
        result.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) result.push_back(decode(*this));
        return result;
    }

    template <typename T, typename Decode>
    std::vector<T> vectorBounded(
        const std::uint32_t maximum,
        const std::string_view limitMessage,
        Decode decode,
        std::size_t* const aggregateRemaining = nullptr)
    {
        const std::uint32_t count = u32();
        if (count > maximum) fail(limitMessage);
        if (aggregateRemaining != nullptr)
        {
            if (static_cast<std::size_t>(count) > *aggregateRemaining)
                fail(limitMessage);
            *aggregateRemaining -= static_cast<std::size_t>(count);
        }
        if (semanticBudget_)
        {
            semanticBudget_->consumeVector(count, sizeof(T));
        }
        std::vector<T> result{};
        result.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
            result.push_back(decode(*this));
        return result;
    }

private:
    std::span<const std::byte> bytes_{};
    std::size_t offset_{};
    SemanticDecodeBudget* semanticBudget_{};
};

void replaceU32(Bytes& bytes, const std::size_t offset, const std::uint32_t value)
{
    if (offset > bytes.size() || 4u > bytes.size() - offset)
    {
        throw std::logic_error("Binary SED patch offset is invalid.");
    }
    for (unsigned i = 0; i < 4; ++i)
    {
        bytes[offset + i] = static_cast<std::byte>(static_cast<std::uint8_t>(value >> (i * 8)));
    }
}

std::uint32_t readU32At(const std::span<const std::byte> bytes, const std::size_t offset)
{
    if (offset > bytes.size() || 4u > bytes.size() - offset)
    {
        fail("truncated integer");
    }
    std::uint32_t value{};
    for (unsigned i = 0; i < 4; ++i)
    {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + i])) << (i * 8);
    }
    return value;
}

bool hasValidCanonicalHeaderChecksum(
    const std::span<const std::byte> header) noexcept
{
    return header.size() == kCanonicalHeaderSize
        && std::equal(kMagic.begin(), kMagic.end(), header.begin())
        && crc32c(header.first(kHeaderChecksumOffset))
            == readU32At(header, kHeaderChecksumOffset);
}

std::uint64_t readU64At(const std::span<const std::byte> bytes, const std::size_t offset)
{
    if (offset > bytes.size() || 8u > bytes.size() - offset)
    {
        fail("truncated integer");
    }
    std::uint64_t value{};
    for (unsigned i = 0; i < 8; ++i)
    {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[offset + i])) << (i * 8);
    }
    return value;
}

std::int64_t readI64At(const std::span<const std::byte> bytes,
    const std::size_t offset)
{
    return std::bit_cast<std::int64_t>(readU64At(bytes, offset));
}

// A compact LZSS block. Literal tokens use 0..127 for 1..128 bytes; match
// tokens use 128..255 for 4..131 bytes followed by a 16-bit backward offset.
Bytes compressFast(const std::span<const std::byte> input)
{
    Bytes output{};
    if (input.empty() || input.size() > kCompressionMaximumBytes) return output;
    output.reserve(input.size());

    // Keep the matcher bounded and off the Windows thread stack.  The head
    // table names the newest position for each hash, while the ring stores one
    // predecessor for every position in the 64 KiB match window. A candidate
    // that is still within the encodable 65,535-byte distance cannot have had
    // its ring entry overwritten.
    constexpr std::size_t kHashSlots = 65'536;
    constexpr std::size_t kHashMask = kHashSlots - 1;
    constexpr std::size_t kMaximumCandidateVisits = 4;
    constexpr std::size_t kOlderCandidateComparisonBudget = 128;
    std::vector<std::int32_t> heads(kHashSlots, -1);
    std::vector<std::int32_t> previous(kHashSlots, -1);
    const auto hashAt = [&](const std::size_t pos)
    {
        const std::uint32_t a = std::to_integer<std::uint8_t>(input[pos]);
        const std::uint32_t b = std::to_integer<std::uint8_t>(input[pos + 1]);
        const std::uint32_t c = std::to_integer<std::uint8_t>(input[pos + 2]);
        return static_cast<std::uint16_t>(((a * 251u + b) * 251u + c) & 0xffffu);
    };
    const auto insertPosition = [&](const std::size_t pos)
    {
        if (pos + 2 >= input.size()) return;
        const std::uint16_t hash = hashAt(pos);
        previous[pos & kHashMask] = heads[hash];
        heads[hash] = static_cast<std::int32_t>(pos);
    };
    const auto canAppendWinningBytes = [&](const std::size_t count)
    {
        // Compression is retained only when it is strictly smaller. Once the
        // monotonic output reaches the raw size, no future match can make the
        // candidate useful. Abort before vector growth so an incompressible
        // 64 MiB section is released before FEC allocation begins.
        return output.size() < input.size()
            && count < input.size() - output.size();
    };
    const auto emitLiterals = [&](std::size_t begin, const std::size_t end)
    {
        while (begin < end)
        {
            const std::size_t count = std::min<std::size_t>(128, end - begin);
            if (!canAppendWinningBytes(1u + count)) return false;
            output.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(count - 1)));
            output.insert(output.end(), input.begin() + static_cast<std::ptrdiff_t>(begin),
                          input.begin() + static_cast<std::ptrdiff_t>(begin + count));
            begin += count;
        }
        return true;
    };

    std::size_t literalStart{};
    std::size_t pos{};
    while (pos + 3 < input.size())
    {
        const std::uint16_t hash = hashAt(pos);
        std::int32_t candidateValue = heads[hash];
        std::size_t matchLength{};
        std::size_t offset{};
        std::size_t olderComparisonBudget = kOlderCandidateComparisonBudget;
        const std::size_t maximum = std::min<std::size_t>(131, input.size() - pos);
        for (std::size_t visit = 0;
            candidateValue >= 0 && visit < kMaximumCandidateVisits; ++visit)
        {
            const std::size_t candidate = static_cast<std::size_t>(candidateValue);
            const std::size_t candidateOffset = pos - candidate;
            if (candidateOffset == 0 || candidateOffset > 65'535) break;
            const std::int32_t nextCandidate = previous[candidate & kHashMask];

            // Preserve the current matcher exactly for the newest candidate.
            // Older candidates share an explicit comparison budget, so a
            // hostile repeated-prefix input cannot turn the chain into an
            // unbounded scan.
            if (visit != 0)
            {
                if (olderComparisonBudget < 4) break;
                olderComparisonBudget -= 4;
            }
            if (input[candidate] == input[pos]
                && input[candidate + 1] == input[pos + 1]
                && input[candidate + 2] == input[pos + 2]
                && input[candidate + 3] == input[pos + 3])
            {
                if (visit != 0 && matchLength >= 4 && matchLength < maximum)
                {
                    if (olderComparisonBudget == 0) break;
                    --olderComparisonBudget;
                    if (input[candidate + matchLength] != input[pos + matchLength])
                    {
                        candidateValue = nextCandidate;
                        continue;
                    }
                }
                std::size_t candidateLength = 4;
                while (candidateLength < maximum)
                {
                    if (visit != 0)
                    {
                        if (olderComparisonBudget == 0) break;
                        --olderComparisonBudget;
                    }
                    if (input[candidate + candidateLength]
                        != input[pos + candidateLength]) break;
                    ++candidateLength;
                }
                // Chains are newest-to-oldest. Updating only for a strictly
                // longer match makes equal-length ties choose the nearest
                // (smallest-offset) candidate deterministically.
                if (candidateLength > matchLength)
                {
                    matchLength = candidateLength;
                    offset = candidateOffset;
                    if (matchLength == maximum) break;
                }
            }
            candidateValue = nextCandidate;
        }
        insertPosition(pos);

        if (matchLength < 4)
        {
            ++pos;
            continue;
        }

        if (!emitLiterals(literalStart, pos)
            || !canAppendWinningBytes(3u))
        {
            return {};
        }
        output.push_back(static_cast<std::byte>(0x80u | static_cast<std::uint8_t>(matchLength - 4)));
        output.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(offset)));
        output.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(offset >> 8)));

        const std::size_t next = pos + matchLength;
        for (std::size_t update = pos + 1; update + 2 < next; ++update)
        {
            insertPosition(update);
        }
        pos = next;
        literalStart = pos;
    }
    return emitLiterals(literalStart, input.size())
        ? output : Bytes{};
}

Bytes decompressFast(const std::span<const std::byte> stored, const std::size_t rawSize)
{
    // Production descriptors admit this bound before reaching the decoder.
    // Keep it local as well: test/fuzz seams and future call sites must not be
    // able to turn a tiny token stream plus a forged size into an unbounded
    // reserve. Once admitted, reserve the exact, already-authenticated extent
    // so a valid section is not repeatedly reallocated and copied while it is
    // expanded.
    if (rawSize > kCompressionMaximumBytes)
    {
        compressionFail("expanded compressed section exceeds the safe limit");
    }
    Bytes output{};
    output.reserve(rawSize);
    std::size_t pos{};
    while (pos < stored.size())
    {
        const std::uint8_t token = std::to_integer<std::uint8_t>(stored[pos++]);
        if ((token & 0x80u) == 0)
        {
            const std::size_t count = static_cast<std::size_t>(token) + 1;
            if (count > stored.size() - pos || count > rawSize - output.size())
            {
                compressionFail("damaged compressed literal run");
            }
            output.insert(output.end(), stored.begin() + static_cast<std::ptrdiff_t>(pos),
                          stored.begin() + static_cast<std::ptrdiff_t>(pos + count));
            pos += count;
            continue;
        }

        if (stored.size() - pos < 2) compressionFail("truncated compressed match");
        const std::size_t count = static_cast<std::size_t>(token & 0x7fu) + 4;
        const std::size_t offset = std::to_integer<std::uint8_t>(stored[pos])
            | (static_cast<std::size_t>(std::to_integer<std::uint8_t>(stored[pos + 1])) << 8);
        pos += 2;
        if (offset == 0 || offset > output.size() || count > rawSize - output.size())
        {
            compressionFail("invalid compressed match");
        }
        const std::size_t outputOffset = output.size();
        output.resize(outputOffset + count);
        if (offset >= count)
        {
            std::memcpy(
                output.data() + static_cast<std::ptrdiff_t>(outputOffset),
                output.data() + static_cast<std::ptrdiff_t>(outputOffset - offset),
                count);
        }
        else
        {
            // LZSS permits an overlapping match whose newly written bytes are
            // immediately reused. Resize once, then retain that exact forward
            // copy rule without a push_back/capacity check for every byte.
            for (std::size_t i = 0; i < count; ++i)
            {
                output[outputOffset + i] = output[outputOffset + i - offset];
            }
        }
    }
    if (output.size() != rawSize)
    {
        compressionFail("compressed section has the wrong expanded size");
    }
    return output;
}

struct EncodedFecPayload final
{
    Bytes bytes{};
    std::uint32_t dataStreams{};
    std::uint32_t parityStreams{};
    std::uint32_t stripeCount{};
};

std::uint64_t fecSlotOffset(const std::uint32_t stripeCount,
    const std::size_t stream, const std::size_t stripe) noexcept
{
    return (static_cast<std::uint64_t>(stream) * stripeCount + stripe)
        * kFecSlotBytes;
}

EncodedFecPayload encodeFecPayload(const std::span<const std::byte> stored,
    const std::uint32_t parityStreams)
{
    if (parityStreams != kFecOptionalParityStreams
        && parityStreams != kFecRequiredParityStreams)
    {
        throw std::logic_error("Binary SED parity tier is invalid.");
    }
    const std::uint64_t minimumChunks = std::max<std::uint64_t>(1,
        (static_cast<std::uint64_t>(stored.size()) + kFecSlotPayloadBytes - 1u)
            / kFecSlotPayloadBytes);
    const std::uint32_t stripeCount = static_cast<std::uint32_t>(
        (minimumChunks + kFecDataStreams - 1u) / kFecDataStreams);
    const std::uint32_t dataStreams = static_cast<std::uint32_t>(
        (minimumChunks + stripeCount - 1u) / stripeCount);
    const std::uint64_t slotCount = static_cast<std::uint64_t>(
        dataStreams + parityStreams) * stripeCount;
    if (slotCount > kMaxCodedSectionBytes / kFecSlotBytes)
    {
        throw std::runtime_error("Binary SED parity payload is too large.");
    }

    Bytes coded(static_cast<std::size_t>(slotCount * kFecSlotBytes), std::byte{0});
    for (std::size_t stream = 0; stream < dataStreams; ++stream)
    {
        for (std::size_t stripe = 0; stripe < stripeCount; ++stripe)
        {
            const std::uint64_t logicalOffset = (stream * stripeCount + stripe)
                * static_cast<std::uint64_t>(kFecSlotPayloadBytes);
            if (logicalOffset >= stored.size()) continue;
            const std::size_t available = static_cast<std::size_t>(std::min<std::uint64_t>(
                kFecSlotPayloadBytes, stored.size() - logicalOffset));
            std::copy_n(stored.begin() + static_cast<std::ptrdiff_t>(logicalOffset), available,
                coded.begin() + static_cast<std::ptrdiff_t>(
                    fecSlotOffset(stripeCount, stream, stripe)));
        }
    }

    const auto& generator = fecGeneratorMatrix(dataStreams);
    for (std::size_t parity = 0; parity < parityStreams; ++parity)
    {
        const std::size_t parityStream = dataStreams + parity;
        for (std::size_t stripe = 0; stripe < stripeCount; ++stripe)
        {
            auto output = std::span<std::byte>(coded).subspan(static_cast<std::size_t>(
                fecSlotOffset(stripeCount, parityStream, stripe)), kFecSlotPayloadBytes);
            for (std::size_t dataStream = 0; dataStream < dataStreams; ++dataStream)
            {
                const std::uint8_t coefficient = generator[parityStream][dataStream];
                if (coefficient == 0) continue;
                const auto input = std::span<const std::byte>(coded).subspan(
                    static_cast<std::size_t>(fecSlotOffset(stripeCount, dataStream, stripe)),
                    kFecSlotPayloadBytes);
                xorScaledGf256(output, input, coefficient);
            }
        }
    }

    for (std::size_t slot = 0; slot < slotCount; ++slot)
    {
        const std::size_t offset = slot * kFecSlotBytes;
        replaceU32(coded, offset + kFecSlotPayloadBytes,
            crc32c(std::span<const std::byte>(coded).subspan(offset, kFecSlotPayloadBytes)));
    }
    return EncodedFecPayload{
        std::move(coded), dataStreams, parityStreams, stripeCount };
}

struct DecodedFecPayload final
{
    Bytes stored{};
    std::uint32_t repairedChunks{};
    std::uint32_t ignoredPaddingChunks{};
    std::uint32_t inconsistentParityChunks{};
};

DecodedFecPayload decodeFecPayload(const std::span<const std::byte> coded,
    const std::uint64_t logicalStoredSize,
    const std::uint32_t dataStreams,
    const std::uint32_t parityStreams,
    const std::uint32_t chunkPayloadSize,
    const std::uint32_t stripeCount)
{
    if (dataStreams == 0 || dataStreams > kFecDataStreams
        || (parityStreams != kFecOptionalParityStreams
            && parityStreams != kFecRequiredParityStreams)
        || chunkPayloadSize != kFecSlotPayloadBytes || stripeCount == 0)
    {
        fail("parity layout values are outside the safe range");
    }
    const std::uint64_t maximumStored = static_cast<std::uint64_t>(dataStreams)
        * stripeCount * kFecSlotPayloadBytes;
    const std::uint64_t expectedCoded = static_cast<std::uint64_t>(
        dataStreams + parityStreams) * stripeCount * kFecSlotBytes;
    if (logicalStoredSize > maximumStored || expectedCoded != coded.size())
    {
        fail("parity layout is inconsistent with its section descriptor");
    }

    Bytes stored(static_cast<std::size_t>(logicalStoredSize), std::byte{0});
    std::uint32_t repairedChunks{};
    std::uint32_t ignoredPaddingChunks{};
    std::uint32_t inconsistentParityChunks{};
    const std::size_t streamCount = dataStreams + parityStreams;
    const auto& generator = fecGeneratorMatrix(dataStreams);
    for (std::size_t stripe = 0; stripe < stripeCount; ++stripe)
    {
        std::array<bool, kFecMaximumStreams> valid{};
        std::array<std::optional<Bytes>, kFecMaximumStreams> repaired{};
        std::vector<std::size_t> invalid{};
        for (std::size_t stream = 0; stream < streamCount; ++stream)
        {
            const std::size_t offset = static_cast<std::size_t>(
                fecSlotOffset(stripeCount, stream, stripe));
            const auto chunk = coded.subspan(offset, kFecSlotPayloadBytes);
            const std::uint32_t expectedCrc = readU32At(
                coded, offset + kFecSlotPayloadBytes);
            const bool crcValid = crc32c(chunk) == expectedCrc;
            bool slotValid = crcValid;
            if (stream < dataStreams)
            {
                const std::uint64_t logicalOffset = (stream * stripeCount + stripe)
                    * static_cast<std::uint64_t>(kFecSlotPayloadBytes);
                const std::size_t logicalBytes = logicalOffset >= logicalStoredSize
                    ? 0u
                    : static_cast<std::size_t>(std::min<std::uint64_t>(
                        kFecSlotPayloadBytes, logicalStoredSize - logicalOffset));
                if (logicalBytes == 0)
                {
                    // Data slots beyond the logical payload are canonical
                    // all-zero padding. They must never consume an erasure
                    // budget, even when an attacker recomputes the slot CRC;
                    // normalize the slot before it can poison reconstruction.
                    valid[stream] = true;
                    const bool canonical = std::all_of(chunk.begin(), chunk.end(),
                        [](const std::byte value) { return value == std::byte{0}; });
                    if (!canonical || !crcValid)
                    {
                        ++ignoredPaddingChunks;
                        repaired[stream].emplace(kFecSlotPayloadBytes, std::byte{0});
                    }
                    continue;
                }
                if (logicalBytes < kFecSlotPayloadBytes)
                {
                    // The tail of a short final data slot is also defined
                    // zero padding. Always inspect and normalize it. A
                    // recomputed CRC over a noncanonical tail authenticates
                    // neither the normalized bytes nor their semantic prefix.
                    // When the normalized CRC does not match, retain the
                    // noncanonical-padding diagnostic for strict loading and
                    // treat the complete slot as an erasure so permissive
                    // recovery reconstructs any simultaneously changed prefix.
                    Bytes normalized(kFecSlotPayloadBytes, std::byte{0});
                    std::copy_n(chunk.begin(), logicalBytes, normalized.begin());
                    const bool tailNonZero = std::any_of(
                        chunk.begin() + static_cast<std::ptrdiff_t>(logicalBytes),
                        chunk.end(), [](const std::byte value)
                        {
                            return value != std::byte{0};
                        });
                    const bool normalizedCrcValid = crc32c(normalized) == expectedCrc;
                    if (tailNonZero)
                    {
                        ++ignoredPaddingChunks;
                    }
                    if (normalizedCrcValid)
                    {
                        repaired[stream] = std::move(normalized);
                        slotValid = true;
                    }
                    else if (tailNonZero && crcValid)
                    {
                        slotValid = false;
                    }
                }
            }
            valid[stream] = slotValid;
            if (!valid[stream]) invalid.push_back(stream);
        }
        if (invalid.size() > parityStreams)
        {
            integrityFail("parity stripe exceeds its repair budget");
        }

        const auto chunkFor = [&](const std::size_t stream) -> std::span<const std::byte>
        {
            if (repaired[stream]) return *repaired[stream];
            return coded.subspan(static_cast<std::size_t>(
                fecSlotOffset(stripeCount, stream, stripe)), kFecSlotPayloadBytes);
        };

        const bool missingData = std::any_of(invalid.begin(), invalid.end(),
            [&](const std::size_t stream) { return stream < dataStreams; });
        if (missingData)
        {
            std::array<std::size_t, kFecDataStreams> selected{};
            std::size_t selectedCount{};
            for (std::size_t stream = 0;
                stream < streamCount && selectedCount < dataStreams; ++stream)
            {
                if (valid[stream]) selected[selectedCount++] = stream;
            }
            if (selectedCount != dataStreams)
            {
                integrityFail("parity stripe has too few surviving chunks");
            }
            GfSquareMatrix decodeMatrix{};
            for (std::size_t row = 0; row < dataStreams; ++row)
            {
                decodeMatrix[row] = generator[selected[row]];
            }
            const GfSquareMatrix inverse = invertGfMatrix(decodeMatrix, dataStreams);
            for (const std::size_t stream : invalid)
            {
                if (stream >= dataStreams) continue;
                Bytes output(kFecSlotPayloadBytes, std::byte{0});
                for (std::size_t source = 0; source < dataStreams; ++source)
                {
                    const std::uint8_t coefficient = inverse[stream][source];
                    if (coefficient == 0) continue;
                    const auto input = chunkFor(selected[source]);
                    xorScaledGf256(output, input, coefficient);
                }
                repaired[stream] = std::move(output);
                valid[stream] = true;
            }
        }

        // CRC32C tells us whether a physical parity slot changed, but a
        // damaged parity slot can carry a recomputed CRC.  Recompute each
        // parity row from the surviving/repaired data after reconstruction so
        // that this otherwise silent corruption remains observable.  We do
        // not consume the data-repair budget or discard an intact semantic
        // payload for this diagnostic: the data SHA-256 remains authoritative
        // and callers can still recover the Level.
        for (std::size_t parity = 0; parity < parityStreams; ++parity)
        {
            const std::size_t parityStream = dataStreams + parity;
            if (!valid[parityStream]) continue;
            Bytes expected(kFecSlotPayloadBytes, std::byte{0});
            for (std::size_t dataStream = 0; dataStream < dataStreams; ++dataStream)
            {
                const std::uint8_t coefficient = generator[parityStream][dataStream];
                if (coefficient == 0) continue;
                const auto input = chunkFor(dataStream);
                xorScaledGf256(expected, input, coefficient);
            }
            const auto observed = coded.subspan(static_cast<std::size_t>(
                fecSlotOffset(stripeCount, parityStream, stripe)),
                kFecSlotPayloadBytes);
            if (!std::equal(expected.begin(), expected.end(), observed.begin()))
            {
                ++inconsistentParityChunks;
            }
        }

        repairedChunks += static_cast<std::uint32_t>(invalid.size());

        for (std::size_t dataStream = 0; dataStream < dataStreams; ++dataStream)
        {
            const std::uint64_t logicalOffset = (dataStream * stripeCount + stripe)
                * static_cast<std::uint64_t>(kFecSlotPayloadBytes);
            if (logicalOffset >= logicalStoredSize) continue;
            const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(
                kFecSlotPayloadBytes, logicalStoredSize - logicalOffset));
            const auto input = chunkFor(dataStream);
            std::copy_n(input.begin(), count,
                stored.begin() + static_cast<std::ptrdiff_t>(logicalOffset));
        }
    }
    return DecodedFecPayload{
        std::move(stored), repairedChunks, ignoredPaddingChunks,
        inconsistentParityChunks };
}

void writeColor(Writer& out, const ColorF& value)
{
    out.f32(value.r); out.f32(value.g); out.f32(value.b); out.f32(value.a);
}

ColorF readColor(Reader& in)
{
    return ColorF{ in.f32(), in.f32(), in.f32(), in.f32() };
}

void writeVector(Writer& out, const TVector& value)
{
    out.f64(value.x); out.f64(value.y); out.f64(value.z);
}

TVector readVector(Reader& in)
{
    return TVector{ in.f64(), in.f64(), in.f64() };
}

void writeBox(Writer& out, const TBox& value)
{
    out.f64(value.x1); out.f64(value.y1); out.f64(value.z1);
    out.f64(value.x2); out.f64(value.y2); out.f64(value.z2);
}

TBox readBox(Reader& in)
{
    return TBox{ in.f64(), in.f64(), in.f64(), in.f64(), in.f64(), in.f64() };
}

enum class ResourceKind : std::size_t
{
    names = 0,
    colormaps = 1,
    materials = 2,
    sounds = 3,
    aiClasses = 4,
    models3do = 5,
    sprites = 6,
    keyframes = 7,
    puppets = 8,
    soundClasses = 9,
    cogScripts = 10,
    templates = 11,
    particles = 12,
    layers = 13,
    count = 14,
};

constexpr std::size_t kResourceKindCount =
    static_cast<std::size_t>(ResourceKind::count);
using ResourceCatalogs =
    std::array<std::vector<std::string>, kResourceKindCount>;

constexpr std::size_t resourceKindIndex(const ResourceKind kind) noexcept
{
    return static_cast<std::size_t>(kind);
}

ResourceKind resourceKindFor(const AdjType type) noexcept
{
    switch (type)
    {
    case AdjType::mat: return ResourceKind::materials;
    case AdjType::cog: return ResourceKind::cogScripts;
    case AdjType::snd: return ResourceKind::soundClasses;
    case AdjType::pup: return ResourceKind::puppets;
    case AdjType::spr: return ResourceKind::sprites;
    case AdjType::par: return ResourceKind::particles;
    case AdjType::model3do: return ResourceKind::models3do;
    case AdjType::ai: return ResourceKind::aiClasses;
    case AdjType::tpl: return ResourceKind::templates;
    default: return ResourceKind::names;
    }
}

ResourceKind resourceKindFor(const CogType type) noexcept
{
    switch (type)
    {
    case CogType::ai: return ResourceKind::aiClasses;
    case CogType::cog: return ResourceKind::cogScripts;
    case CogType::key: return ResourceKind::keyframes;
    case CogType::mat: return ResourceKind::materials;
    case CogType::model3do: return ResourceKind::models3do;
    case CogType::wav: return ResourceKind::sounds;
    case CogType::template_: return ResourceKind::templates;
    default: return ResourceKind::names;
    }
}

/** Canonical V1 categorized resource catalogs stored by `rsrc`.
 *
 * Every category has its own zero-based index range. The signed reference
 * `-1` is the sole empty/not-set sentinel. Non-empty entries are inserted in
 * first semantic use order within their category, so a future schema can put
 * MAT/3DO/COG payload records directly after that category without scanning a
 * mixed resource set. Generic parameter names, descriptions, and untyped
 * strings live in the separate `names` catalog.
 */
class ResourceIndex final
{
public:
    void add(const ResourceKind kind, const std::string& value)
    {
        if (value.empty()) return;
        if (value.size() > kMaxStringBytes)
        {
            throw std::runtime_error("Binary SED string exceeds the safe size limit.");
        }
        const std::size_t category = resourceKindIndex(kind);
        auto& indices = indices_[category];
        auto& strings = catalogs_[category];
        if (indices.contains(value)) return;
        if (strings.size() >= kMaxCollectionItems)
        {
            throw std::runtime_error("Binary SED resource category exceeds the safe item limit.");
        }
        const std::uint32_t index = static_cast<std::uint32_t>(strings.size());
        strings.push_back(value);
        indices.emplace(value, index);
    }

    [[nodiscard]] std::int32_t reference(const ResourceKind kind,
        const std::string& value) const
    {
        if (value.empty()) return -1;
        const auto& indices = indices_[resourceKindIndex(kind)];
        const auto found = indices.find(value);
        if (found == indices.end())
        {
            throw std::logic_error("Binary SED resource string was not indexed.");
        }
        return static_cast<std::int32_t>(found->second);
    }

    [[nodiscard]] const ResourceCatalogs& catalogs() const noexcept
    {
        return catalogs_;
    }

private:
    ResourceCatalogs catalogs_{};
    std::array<std::unordered_map<std::string, std::uint32_t>,
        kResourceKindCount> indices_{};
};

// Resource-index construction used to rebuild fourteen unordered maps for
// every adjacent HIST state, even when a Thing-only edit left every resource
// string unchanged.  Keep a compact writer-side provenance sequence so the
// next trusted transition can compare the exact resource inputs without
// allocating/copying a second ResourceIndex.  This is deliberately not a
// persisted hash: equality is checked element by element, including the
// category, so a collision cannot make stale catalog indices canonical.
struct ResourceInput final
{
    ResourceKind kind{};
    std::string value{};
};

template <typename Visitor>
void forEachResourceInput(const Level& level, Visitor&& visitor)
{
    const auto add = [&visitor](const ResourceKind kind,
                                const std::string& value)
    {
        visitor(kind, value);
    };
    add(ResourceKind::colormaps, level.masterCMP);
    for (const std::string& value : level.colormaps)
        add(ResourceKind::colormaps, value);
    for (const std::string& value : level.materials)
        add(ResourceKind::materials, value);
    for (const std::string& value : level.layers)
        add(ResourceKind::layers, value);
    for (const Sector& sector : level.sectors)
    {
        add(ResourceKind::colormaps, sector.colormap);
        add(ResourceKind::sounds, sector.sound);
        for (const Surface& surface : sector.surfaces)
            add(ResourceKind::materials, surface.material);
    }
    for (const Thing& thing : level.things)
    {
        add(ResourceKind::templates, thing.templateName);
        add(ResourceKind::names, thing.name);
        for (const TplValue& value : thing.values)
        {
            add(ResourceKind::names, value.name);
            add(resourceKindFor(value.adjType), value.stringValue);
        }
    }
    for (const ThingTemplate& value : level.templates)
    {
        add(ResourceKind::templates, value.name);
        add(ResourceKind::templates, value.parent);
        for (const TplValue& parameter : value.values)
        {
            add(ResourceKind::names, parameter.name);
            add(resourceKindFor(parameter.adjType), parameter.stringValue);
        }
        add(ResourceKind::names, value.desc);
    }
    for (const Cog& cog : level.cogs)
    {
        add(ResourceKind::cogScripts, cog.name);
        for (const CogValue& value : cog.values)
        {
            add(ResourceKind::names, value.name);
            add(resourceKindFor(value.cogType), value.stringValue);
            add(ResourceKind::names, value.description);
        }
    }
    for (const HNode& node : level.h3doNodes)
        add(ResourceKind::names, node.name);
}

std::vector<ResourceInput> makeResourceInputs(const Level& level)
{
    std::vector<ResourceInput> result{};
    forEachResourceInput(level,
        [&result](const ResourceKind kind, const std::string& value)
        {
            if (!value.empty())
            {
                result.push_back(ResourceInput{ kind, value });
            }
        });
    return result;
}

bool sameResourceInputs(const Level& level,
    const std::vector<ResourceInput>& expected) noexcept
{
    std::size_t index{};
    bool same = true;
    forEachResourceInput(level,
        [&expected, &index, &same](const ResourceKind kind,
                                   const std::string& value)
        {
            if (!same) return;
            if (value.empty()) return;
            if (index >= expected.size()
                || expected[index].kind != kind
                || expected[index].value != value)
            {
                same = false;
                return;
            }
            ++index;
        });
    return same && index == expected.size();
}

struct UnprotectedGenerationProvenance final
{
    ResourceIndex resourceIndex{};
    std::vector<ResourceInput> resourceInputs{};
};

ResourceIndex buildResourceIndex(const Level& level)
{
    ResourceIndex index{};
    forEachResourceInput(level,
        [&index](const ResourceKind kind, const std::string& value)
        {
            index.add(kind, value);
        });
    return index;
}

void writeStringReference(Writer& out, const ResourceIndex& index,
    const ResourceKind kind, const std::string& value)
{
    out.writeSedIndex(index.reference(kind, value));
}

const std::vector<std::string>* resourceCatalog(
    const ResourceCatalogs* const catalogs, const ResourceKind kind) noexcept
{
    return catalogs == nullptr ? nullptr
        : &(*catalogs)[resourceKindIndex(kind)];
}

void writeValue(Writer& out, const Value& value, const ResourceIndex& index,
    const ResourceKind valueKind, const bool integerIsIndex = false)
{
    writeStringReference(out, index, ResourceKind::names, value.name);
    out.i32(static_cast<std::int32_t>(value.type));
    if (integerIsIndex && value.type == ValueType::ptr)
        out.writeSedIndex(value.intValue);
    else
        out.i32(value.intValue);
    out.f64(value.floatValue);
    writeStringReference(out, index, valueKind, value.stringValue);
}

Value readValue(Reader& in, const ResourceCatalogs* const catalogs,
    const ResourceKind valueKind, const bool allowMissingTable,
    const bool integerIsIndex = false)
{
    Value value{};
    value.name = in.stringReference(
        resourceCatalog(catalogs, ResourceKind::names), allowMissingTable);
    value.type = static_cast<ValueType>(in.i32());
    value.intValue = integerIsIndex && value.type == ValueType::ptr
        ? in.readSedIndex() : in.i32();
    value.floatValue = in.f64();
    value.stringValue = in.stringReference(
        resourceCatalog(catalogs, valueKind), allowMissingTable);
    return value;
}

bool cogIntegerIsEntityIndex(const CogType type) noexcept
{
    return type == CogType::sec || type == CogType::surface
        || type == CogType::thing;
}

void writeTplValue(Writer& out, const TplValue& value,
    const ResourceIndex& index)
{
    out.i32(static_cast<std::int32_t>(value.adjType));
    writeValue(out, value, index, resourceKindFor(value.adjType));
}

TplValue readTplValue(Reader& in, const ResourceCatalogs* const catalogs,
    const bool allowMissingTable)
{
    const AdjType adjType = static_cast<AdjType>(in.i32());
    const Value base = readValue(in, catalogs, resourceKindFor(adjType),
        allowMissingTable);
    TplValue value{};
    static_cast<Value&>(value) = base;
    value.adjType = adjType;
    return value;
}

void writeCogValue(Writer& out, const CogValue& value,
    const ResourceIndex& index)
{
    out.i32(static_cast<std::int32_t>(value.cogType));
    writeValue(out, value, index, resourceKindFor(value.cogType),
        cogIntegerIsEntityIndex(value.cogType));
    out.boolean(value.local);
    writeStringReference(out, index, ResourceKind::names, value.description);
    out.i32(value.mask);
}

CogValue readCogValue(Reader& in, const ResourceCatalogs* const catalogs,
    const bool allowMissingTable)
{
    const CogType cogType = static_cast<CogType>(in.i32());
    const Value base = readValue(in, catalogs, resourceKindFor(cogType),
        allowMissingTable, cogIntegerIsEntityIndex(cogType));
    CogValue value{};
    static_cast<Value&>(value) = base;
    value.cogType = cogType;
    value.local = in.boolean();
    value.description = in.stringReference(
        resourceCatalog(catalogs, ResourceKind::names), allowMissingTable);
    value.mask = in.i32();
    return value;
}

Bytes encodeHead(const Level& level, const ResourceIndex& index)
{
    Writer out{ kMaxSectionBytes };
    out.i32(static_cast<std::int32_t>(level.kind));
    out.i32(level.header.version);
    out.f64(level.header.gravity);
    out.f64(level.header.horizonSky.distance);
    out.f64(level.header.horizonSky.pixelsPerRev);
    out.f64(level.header.horizonSky.offset.x);
    out.f64(level.header.horizonSky.offset.y);
    out.f64(level.header.ceilingSky.height);
    out.f64(level.header.ceilingSky.offset.x);
    out.f64(level.header.ceilingSky.offset.y);
    for (const double value : level.header.mipmapDistances) out.f64(value);
    for (const double value : level.header.lodDistances) out.f64(value);
    out.f64(level.header.perspectiveDistance);
    out.f64(level.header.gouraudDistance);
    out.boolean(level.header.fog.enabled);
    writeColor(out, level.header.fog.color);
    out.f64(level.header.fog.fogStart);
    out.f64(level.header.fog.fogEnd);

    const LevelExportBufferExtras& extra = level.exportBufferExtras;
    out.u32(extra.sounds); out.u32(extra.materials); out.u32(extra.aiClasses);
    out.u32(extra.models); out.u32(extra.sprites); out.u32(extra.keyframes);
    out.u32(extra.puppets); out.u32(extra.soundClasses); out.u32(extra.cogScripts);
    out.u32(extra.cogs); out.u32(extra.templates); out.u32(extra.things);

    writeStringReference(out, index, ResourceKind::colormaps, level.masterCMP);
    out.f64(level.ppunit);
    out.string(level.lvisstring);
    out.i32(level.curId);
    return out.take();
}

Bytes encodeJklCogImportState(const Level& level)
{
    Writer out{};
    out.u32(1u);
    out.boolean(level.importedJklCogRowsPresent);
    out.vector(level.importedJklSurfaceRefs,
        [](Writer& writer, const SurfaceRef& ref)
        {
            writer.i32(ref.sector);
            writer.i32(ref.surface);
        });
    return out.take();
}

SedJklCogImportState decodeJklCogImportState(Reader& in)
{
    if (in.u32() != 1u) fail("unsupported jimp section version");
    SedJklCogImportState state{};
    state.cogRowsPresent = in.boolean();
    state.surfaceRefs = in.vector<SurfaceRef>(
        [](Reader& reader)
        {
            return SurfaceRef{ reader.i32(), reader.i32() };
        });
    if (!in.empty()) fail("jimp section contains unexplained trailing data");
    return state;
}

void decodeHead(Level& level, Reader& in,
    const ResourceCatalogs* const catalogs,
    const bool allowMissingTable)
{
    level.kind = static_cast<ProjectType>(in.i32());
    if (level.kind != ProjectType::JKDF2 && level.kind != ProjectType::MOTS && level.kind != ProjectType::IJIM)
    {
        fail("unknown project type");
    }
    level.header.version = in.i32();
    level.header.gravity = in.f64();
    level.header.horizonSky.distance = in.f64();
    level.header.horizonSky.pixelsPerRev = in.f64();
    level.header.horizonSky.offset = { in.f64(), in.f64() };
    level.header.ceilingSky.height = in.f64();
    level.header.ceilingSky.offset = { in.f64(), in.f64() };
    for (double& value : level.header.mipmapDistances) value = in.f64();
    for (double& value : level.header.lodDistances) value = in.f64();
    level.header.perspectiveDistance = in.f64();
    level.header.gouraudDistance = in.f64();
    level.header.fog.enabled = in.boolean();
    level.header.fog.color = readColor(in);
    level.header.fog.fogStart = in.f64();
    level.header.fog.fogEnd = in.f64();

    LevelExportBufferExtras& extra = level.exportBufferExtras;
    extra.sounds = in.u32(); extra.materials = in.u32(); extra.aiClasses = in.u32();
    extra.models = in.u32(); extra.sprites = in.u32(); extra.keyframes = in.u32();
    extra.puppets = in.u32(); extra.soundClasses = in.u32(); extra.cogScripts = in.u32();
    extra.cogs = in.u32(); extra.templates = in.u32(); extra.things = in.u32();
    level.masterCMP = in.stringReference(
        resourceCatalog(catalogs, ResourceKind::colormaps), allowMissingTable);
    level.ppunit = in.f64();
    level.lvisstring = in.string();
    level.curId = in.i32();
}

Bytes encodeResources(const Level& level, const ResourceIndex& index)
{
    Writer out{ kMaxSectionBytes };
    const auto writeString = [](Writer& writer, const std::string& value) { writer.string(value); };
    for (const std::vector<std::string>& catalog : index.catalogs())
    {
        out.vector(catalog, writeString);
    }
    const auto writeInventory = [&out, &index](const ResourceKind kind,
        const std::vector<std::string>& values)
    {
        out.vector(values, [&index, kind](Writer& writer, const std::string& value)
            {
                writeStringReference(writer, index, kind, value);
            });
    };
    writeInventory(ResourceKind::colormaps, level.colormaps);
    writeInventory(ResourceKind::materials, level.materials);
    writeInventory(ResourceKind::layers, level.layers);
    return out.take();
}

ResourceCatalogs decodeResources(Level& level, Reader& in)
{
    const auto readString = [](Reader& reader) { return reader.string(); };
    ResourceCatalogs catalogs{};
    for (std::vector<std::string>& catalog : catalogs)
    {
        catalog = in.vector<std::string>(readString);
    }
    const auto readInventory = [&in, &catalogs](const ResourceKind kind)
    {
        return in.vector<std::string>([&catalogs, kind](Reader& reader)
            {
                return reader.stringReference(
                    resourceCatalog(&catalogs, kind));
            });
    };
    level.colormaps = readInventory(ResourceKind::colormaps);
    level.materials = readInventory(ResourceKind::materials);
    level.layers = readInventory(ResourceKind::layers);
    return catalogs;
}

void writeSurface(Writer& out, const Surface& surface,
    const ResourceIndex& index)
{
    out.writeSedIndex(surface.num); out.i32(surface.mark); out.i32(surface.d3dId);
    out.i32(surface.surfaceFlags); out.u32(surface.faceFlags);
    out.i32(surface.geo); out.i32(surface.light); out.i32(surface.tex);
    writeColor(out, surface.extraLight);
    out.i32(surface.adjoinFlags);
    out.boolean(surface.adjoin.has_value());
    if (surface.adjoin)
    {
        out.writeSedIndex(surface.adjoin->sector); out.writeSedIndex(surface.adjoin->surface);
    }
    writeStringReference(out, index, ResourceKind::materials, surface.material);
    out.writeSedIndex(surface.nAdj); out.writeSedIndex(surface.nMat);
    out.f32(surface.uScale); out.f32(surface.vScale);
    writeVector(out, surface.normal);
    out.vector(surface.vertices, [](Writer& writer, const std::int32_t value) { writer.writeSedIndex(value); });
    out.vector(surface.texVertices, [](Writer& writer, const TexVertex& value)
        {
            writer.f32(value.u); writer.f32(value.v); writeColor(writer, value.color);
        });
}

Surface readSurface(Reader& in, const ResourceCatalogs* const catalogs,
    const bool allowMissingTable)
{
    Surface surface{};
    surface.num = in.readSedIndex(); surface.mark = in.i32(); surface.d3dId = in.i32();
    surface.surfaceFlags = in.i32(); surface.faceFlags = in.u32();
    surface.geo = in.i32(); surface.light = in.i32(); surface.tex = in.i32();
    surface.extraLight = readColor(in);
    surface.adjoinFlags = in.i32();
    if (in.boolean()) surface.adjoin = SurfaceRef{ in.readSedIndex(), in.readSedIndex() };
    surface.material = in.stringReference(
        resourceCatalog(catalogs, ResourceKind::materials), allowMissingTable);
    surface.nAdj = in.readSedIndex(); surface.nMat = in.readSedIndex();
    surface.uScale = in.f32(); surface.vScale = in.f32();
    surface.normal = readVector(in);
    surface.vertices = in.vector<std::int32_t>([](Reader& reader) { return reader.readSedIndex(); });
    surface.texVertices = in.vector<TexVertex>([](Reader& reader)
        {
            TexVertex value{};
            value.u = reader.f32(); value.v = reader.f32(); value.color = readColor(reader);
            return value;
        });
    return surface;
}

Sector readSector(Reader& reader, const ResourceCatalogs* catalogs,
    bool allowMissingTable);

void writeSector(Writer& writer, const Sector& sector,
    const ResourceIndex& index);

void skipSector(Reader& reader, const ResourceCatalogs& catalogs);

Bytes encodeSectors(const Level& level, const ResourceIndex& index)
{
    Writer out{ kMaxSectionBytes };
    out.vector(level.sectors, [&index](Writer& writer, const Sector& sector)
        {
            writeSector(writer, sector, index);
        });
    return out.take();
}

struct SectorPayloadSlice final
{
    std::size_t begin{};
    std::size_t end{};
};

std::vector<SectorPayloadSlice> locateSectorPayloads(
    const std::span<const std::byte> payload,
    const ResourceCatalogs& catalogs)
{
    Reader reader{ payload };
    const std::uint32_t count = reader.u32();
    if (count > kMaxCollectionItems)
    {
        throw std::runtime_error("SECT sector count exceeds the safe item limit.");
    }
    std::vector<SectorPayloadSlice> result{};
    result.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const std::size_t begin = reader.position();
        skipSector(reader, catalogs);
        result.push_back(SectorPayloadSlice{ begin, reader.position() });
    }
    if (!reader.empty())
    {
        throw std::runtime_error("SECT payload has trailing bytes.");
    }
    return result;
}

Bytes encodeSectorsWithReusedPayload(
    const Level& level,
    const ResourceIndex& index,
    const std::span<const std::byte> previousPayload,
    const std::span<const std::int32_t> changedIndices)
{
    const std::vector<SectorPayloadSlice> slices = locateSectorPayloads(
        previousPayload, index.catalogs());
    if (slices.size() != level.sectors.size())
    {
        // A topology change invalidates positional patching. The caller's
        // trusted transition normally supplies a subset only for stable
        // SectorSubset snapshots; keep this fallback exact for malformed or
        // imported histories.
        return encodeSectors(level, index);
    }
    std::vector<bool> changed(slices.size(), false);
    for (const std::int32_t value : changedIndices)
    {
        if (value < 0 || static_cast<std::size_t>(value) >= changed.size())
        {
            return encodeSectors(level, index);
        }
        changed[static_cast<std::size_t>(value)] = true;
    }

    Writer out{ kMaxSectionBytes };
    out.u32(static_cast<std::uint32_t>(level.sectors.size()));
    for (std::size_t indexValue = 0; indexValue < level.sectors.size();
         ++indexValue)
    {
        if (changed[indexValue])
        {
            writeSector(out, level.sectors[indexValue], index);
        }
        else
        {
            out.raw(previousPayload.subspan(
                slices[indexValue].begin,
                slices[indexValue].end - slices[indexValue].begin));
        }
    }
    return out.take();
}


Sector readSector(Reader& reader, const ResourceCatalogs* const catalogs,
    const bool allowMissingTable)
{
    Sector sector{};
    sector.id = reader.i32(); sector.num = reader.readSedIndex(); sector.flags = reader.i32();
    sector.ambient = readColor(reader); sector.extraLight = readColor(reader);
    sector.pointLight.color = readColor(reader);
    sector.pointLight.position = readVector(reader);
    sector.pointLight.minRange = reader.f64(); sector.pointLight.maxRange = reader.f64();
    sector.colormap = reader.stringReference(
        resourceCatalog(catalogs, ResourceKind::colormaps), allowMissingTable);
    sector.tint = readColor(reader);
    sector.sound = reader.stringReference(
        resourceCatalog(catalogs, ResourceKind::sounds), allowMissingTable);
    sector.soundVolume = reader.f64();
    sector.thrust = readVector(reader); sector.collideBox = readBox(reader);
    sector.layer = reader.readSedIndex(); sector.mark = reader.i32();
    sector.vertices = reader.vector<Vertex>([](Reader& nested)
        {
            return Vertex{ nested.f64(), nested.f64(), nested.f64(), nested.i32() };
        });
    sector.surfaces = reader.vector<Surface>([catalogs, allowMissingTable](Reader& nested)
        {
            return readSurface(nested, catalogs, allowMissingTable);
        });
    return sector;
}

void skipSurface(Reader& reader, const ResourceCatalogs& catalogs)
{
    (void)reader.readSedIndex();
    (void)reader.i32(); (void)reader.i32(); (void)reader.i32();
    (void)reader.u32(); (void)reader.i32(); (void)reader.i32();
    (void)reader.i32(); readColor(reader);
    (void)reader.i32();
    if (reader.boolean())
    {
        (void)reader.readSedIndex(); (void)reader.readSedIndex();
    }
    (void)reader.stringReference(
        resourceCatalog(&catalogs, ResourceKind::materials));
    (void)reader.readSedIndex(); (void)reader.readSedIndex();
    (void)reader.f32(); (void)reader.f32(); readVector(reader);
    const std::uint32_t vertexCount = reader.u32();
    if (vertexCount > kMaxCollectionItems)
    {
        throw std::runtime_error("Surface vertex count exceeds the safe item limit.");
    }
    for (std::uint32_t index = 0; index < vertexCount; ++index)
        (void)reader.readSedIndex();
    const std::uint32_t texVertexCount = reader.u32();
    if (texVertexCount > kMaxCollectionItems)
    {
        throw std::runtime_error("Surface texture-vertex count exceeds the safe item limit.");
    }
    for (std::uint32_t index = 0; index < texVertexCount; ++index)
    {
        (void)reader.f32(); (void)reader.f32(); readColor(reader);
    }
}

void skipSector(Reader& reader, const ResourceCatalogs& catalogs)
{
    (void)reader.i32(); (void)reader.readSedIndex(); (void)reader.i32();
    readColor(reader); readColor(reader); readColor(reader); readVector(reader);
    (void)reader.f64(); (void)reader.f64();
    (void)reader.stringReference(
        resourceCatalog(&catalogs, ResourceKind::colormaps));
    readColor(reader);
    (void)reader.stringReference(
        resourceCatalog(&catalogs, ResourceKind::sounds));
    (void)reader.f64(); readVector(reader); readBox(reader);
    (void)reader.readSedIndex(); (void)reader.i32();
    const std::uint32_t vertexCount = reader.u32();
    if (vertexCount > kMaxCollectionItems)
    {
        throw std::runtime_error("Sector vertex count exceeds the safe item limit.");
    }
    for (std::uint32_t index = 0; index < vertexCount; ++index)
    {
        (void)reader.f64(); (void)reader.f64(); (void)reader.f64();
        (void)reader.i32();
    }
    const std::uint32_t surfaceCount = reader.u32();
    if (surfaceCount > kMaxCollectionItems)
    {
        throw std::runtime_error("Sector surface count exceeds the safe item limit.");
    }
    for (std::uint32_t index = 0; index < surfaceCount; ++index)
        skipSurface(reader, catalogs);
}

// History pooling needs record boundaries, but it must not need to materialize
// or look up the resource catalogs a second time just to split a writer-owned
// frame.  These two helpers consume exactly the same wire fields as the
// checked skipSector()/skipSurface() path while treating category-local
// resource references as opaque sedIndex values.  They are used only after a
// frame has been emitted by the canonical writer; untrusted bytes continue to
// use the checked decoder above.
void skipHistorySurfaceRecord(Reader& reader)
{
    (void)reader.readSedIndex();
    (void)reader.i32(); (void)reader.i32(); (void)reader.i32();
    (void)reader.u32(); (void)reader.i32(); (void)reader.i32();
    (void)reader.i32(); readColor(reader);
    (void)reader.i32();
    if (reader.boolean())
    {
        (void)reader.readSedIndex(); (void)reader.readSedIndex();
    }
    (void)reader.readSedIndex(); // material catalog-local reference
    (void)reader.readSedIndex(); (void)reader.readSedIndex();
    (void)reader.f32(); (void)reader.f32(); readVector(reader);
    const std::uint32_t vertexCount = reader.u32();
    if (vertexCount > kMaxCollectionItems)
    {
        throw std::runtime_error("History SECT surface vertex count exceeds the safe item limit.");
    }
    for (std::uint32_t index = 0; index < vertexCount; ++index)
        (void)reader.readSedIndex();
    const std::uint32_t texVertexCount = reader.u32();
    if (texVertexCount > kMaxCollectionItems)
    {
        throw std::runtime_error("History SECT texture-vertex count exceeds the safe item limit.");
    }
    for (std::uint32_t index = 0; index < texVertexCount; ++index)
    {
        (void)reader.f32(); (void)reader.f32(); readColor(reader);
    }
}

void skipHistorySectorRecord(Reader& reader)
{
    (void)reader.i32(); (void)reader.readSedIndex(); (void)reader.i32();
    readColor(reader); readColor(reader); readColor(reader); readVector(reader);
    (void)reader.f64(); (void)reader.f64();
    (void)reader.readSedIndex(); // colormap catalog-local reference
    readColor(reader);
    (void)reader.readSedIndex(); // sound catalog-local reference
    (void)reader.f64(); readVector(reader); readBox(reader);
    (void)reader.readSedIndex(); (void)reader.i32();
    const std::uint32_t vertexCount = reader.u32();
    if (vertexCount > kMaxCollectionItems)
    {
        throw std::runtime_error("History SECT vertex count exceeds the safe item limit.");
    }
    for (std::uint32_t index = 0; index < vertexCount; ++index)
    {
        (void)reader.f64(); (void)reader.f64();
        (void)reader.f64(); (void)reader.i32();
    }
    const std::uint32_t surfaceCount = reader.u32();
    if (surfaceCount > kMaxCollectionItems)
    {
        throw std::runtime_error("History SECT surface count exceeds the safe item limit.");
    }
    for (std::uint32_t index = 0; index < surfaceCount; ++index)
        skipHistorySurfaceRecord(reader);
}

void writeSector(Writer& writer, const Sector& sector,
    const ResourceIndex& index)
{
    writer.i32(sector.id); writer.writeSedIndex(sector.num); writer.i32(sector.flags);
    writeColor(writer, sector.ambient); writeColor(writer, sector.extraLight);
    writeColor(writer, sector.pointLight.color);
    writeVector(writer, sector.pointLight.position);
    writer.f64(sector.pointLight.minRange); writer.f64(sector.pointLight.maxRange);
    writeStringReference(writer, index, ResourceKind::colormaps, sector.colormap);
    writeColor(writer, sector.tint);
    writeStringReference(writer, index, ResourceKind::sounds, sector.sound);
    writer.f64(sector.soundVolume);
    writeVector(writer, sector.thrust); writeBox(writer, sector.collideBox);
    writer.writeSedIndex(sector.layer); writer.i32(sector.mark);
    writer.vector(sector.vertices, [](Writer& nested, const Vertex& vertex)
        {
            nested.f64(vertex.x); nested.f64(vertex.y); nested.f64(vertex.z); nested.i32(vertex.mark);
        });
    writer.vector(sector.surfaces, [&index](Writer& nested, const Surface& surface)
        {
            writeSurface(nested, surface, index);
        });
}

void decodeSectors(Level& level, Reader& in,
    const ResourceCatalogs* const catalogs,
    const bool allowMissingTable)
{
    level.sectors = in.vector<Sector>([catalogs, allowMissingTable](Reader& reader)
        {
            return readSector(reader, catalogs, allowMissingTable);
        });
}

Bytes encodeThings(const Level& level, const ResourceIndex& index,
    const bool /* includeArchLighting */)
{
    const SedArchLightingSnapshot archLighting =
        makeSedArchLightingSnapshot(level);
    const bool includeArchLighting = !archLighting.objects.empty()
        || !level.motsArchLightingBakeBaselines.empty();
    Writer out{ kMaxSectionBytes };
    std::size_t thingIndex = 0;
    out.vector(level.things, [&index, includeArchLighting, &archLighting,
        &thingIndex](Writer& writer, const Thing& thing)
        {
            writer.i32(thing.id); writer.writeSedIndex(thing.num); writer.writeSedIndex(thing.sector);
            if (includeArchLighting)
                writer.writeSedIndex(archLighting.thingIndices[thingIndex]);
            writeStringReference(writer, index, ResourceKind::templates, thing.templateName);
            writeStringReference(writer, index, ResourceKind::names, thing.name);
            writer.f64(thing.x); writer.f64(thing.y); writer.f64(thing.z);
            writer.f64(thing.pch); writer.f64(thing.yaw); writer.f64(thing.rol);
            writer.vector(thing.values, [&index](Writer& nested, const TplValue& value)
                {
                    writeTplValue(nested, value, index);
                });
            writer.writeSedIndex(thing.layer); writeBox(writer, thing.bbox);
            ++thingIndex;
        });
    if (includeArchLighting)
    {
        out.vector(archLighting.objects, [](Writer& writer,
            const MotsArchLightObject* const object)
            {
                writer.i32(object->wireIndex);
                writer.vector(object->meshes, [](Writer& nested,
                    const MotsArchLightMesh& mesh)
                    {
                        nested.i32(mesh.wireIndex);
                        nested.vector(mesh.vertices, [](Writer& vertexWriter,
                            const MotsArchLightVertex& vertex)
                            {
                                vertexWriter.f32(vertex.mono);
                                vertexWriter.f32(vertex.red);
                                vertexWriter.f32(vertex.green);
                                vertexWriter.f32(vertex.blue);
                            });
                    });
            });
    }
    if (!level.motsArchLightingBakeBaselines.empty())
    {
        Writer payload{ kMaxSectionBytes };
        payload.u32(static_cast<std::uint32_t>(
            level.motsArchLightingBakeBaselines.size()));
        const auto writeObject = [](Writer& writer,
                                    const MotsArchLightObject& object)
        {
            writer.i32(object.wireIndex);
            writer.vector(object.meshes, [](Writer& meshWriter,
                const MotsArchLightMesh& mesh)
                {
                    meshWriter.i32(mesh.wireIndex);
                    meshWriter.vector(mesh.vertices, [](Writer& vertexWriter,
                        const MotsArchLightVertex& vertex)
                        {
                            vertexWriter.f32(vertex.mono);
                            vertexWriter.f32(vertex.red);
                            vertexWriter.f32(vertex.green);
                            vertexWriter.f32(vertex.blue);
                        });
                });
        };
        for (const MotsArchLightingBakeBaseline& baseline :
             level.motsArchLightingBakeBaselines)
        {
            payload.i32(baseline.thingId);
            payload.string(baseline.modelName);
            payload.i32(baseline.originalArchLightingIndex);
            payload.boolean(baseline.originalArchLighting.has_value());
            if (baseline.originalArchLighting)
            {
                writeObject(payload, *baseline.originalArchLighting);
            }
        }
        if (payload.bytes().size() > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::runtime_error(
                "thng ARCH bake provenance trailer exceeds its size limit.");
        }
        out.raw(kArchBakeBaselineTrailerMagic);
        out.u32(kArchBakeBaselineTrailerVersion);
        out.u32(static_cast<std::uint32_t>(payload.bytes().size()));
        out.raw(payload.bytes());
    }
    return out.take();
}

void decodeThings(Level& level, Reader& in,
    const ResourceCatalogs* const catalogs,
    const bool allowMissingTable,
    const bool includeArchLighting)
{
    if (!includeArchLighting)
        level.motsArchLighting.clear();
    level.motsArchLightingBakeBaselines.clear();
    level.things = in.vector<Thing>([catalogs, allowMissingTable,
        includeArchLighting](Reader& reader)
        {
            Thing thing{};
            thing.id = reader.i32(); thing.num = reader.readSedIndex(); thing.sector = reader.readSedIndex();
            if (includeArchLighting)
                thing.archLightingIndex = reader.readSedIndex();
            thing.templateName = reader.stringReference(
                resourceCatalog(catalogs, ResourceKind::templates), allowMissingTable);
            thing.name = reader.stringReference(
                resourceCatalog(catalogs, ResourceKind::names), allowMissingTable);
            thing.x = reader.f64(); thing.y = reader.f64(); thing.z = reader.f64();
            thing.pch = reader.f64(); thing.yaw = reader.f64(); thing.rol = reader.f64();
            thing.values = reader.vector<TplValue>([catalogs, allowMissingTable](Reader& nested)
                {
                    return readTplValue(nested, catalogs, allowMissingTable);
                });
            thing.layer = reader.readSedIndex();
            const TBox box = readBox(reader);
            static_cast<TBox&>(thing.bbox) = box;
            return thing;
        });
    if (includeArchLighting)
    {
        std::size_t remainingMeshRecords = kMaxMotsArchLightingRecords;
        std::size_t remainingVertexRecords = kMaxMotsArchLightingRecords;
        level.motsArchLighting = in.vectorBounded<MotsArchLightObject>(
            static_cast<std::uint32_t>(kMaxMotsArchLightingRecords),
            "thng ARCHLIGHTING object count exceeds the safe item limit.",
            [&remainingMeshRecords, &remainingVertexRecords](Reader& reader)
            {
                MotsArchLightObject object{};
                object.wireIndex = reader.i32();
                object.meshes = reader.vectorBounded<MotsArchLightMesh>(
                    static_cast<std::uint32_t>(kMaxMotsArchLightingRecords),
                    "thng ARCHLIGHTING mesh count exceeds the safe item limit.",
                    [&remainingVertexRecords](Reader& meshReader)
                    {
                        MotsArchLightMesh mesh{};
                        mesh.wireIndex = meshReader.i32();
                        mesh.vertices = meshReader.vectorBounded<MotsArchLightVertex>(
                            static_cast<std::uint32_t>(kMaxMotsArchLightingRecords),
                            "thng ARCHLIGHTING vertex count exceeds the safe item limit.",
                            [](Reader& vertexReader)
                            {
                                return MotsArchLightVertex{
                                    vertexReader.f32(), vertexReader.f32(),
                                    vertexReader.f32(), vertexReader.f32() };
                            }, &remainingVertexRecords);
                        return mesh;
                    }, &remainingMeshRecords);
                return object;
            });
    }
    if (!in.empty())
    {
        if (!includeArchLighting)
        {
            fail("thng ARCH bake provenance requires an ARCH-capable SED version");
        }
        const auto magic = in.raw(kArchBakeBaselineTrailerMagic.size());
        if (!std::equal(magic.begin(), magic.end(),
                kArchBakeBaselineTrailerMagic.begin()))
        {
            fail("thng has an unknown trailing ARCH bake extension");
        }
        const std::uint32_t version = in.u32();
        if (version != kArchBakeBaselineTrailerVersion)
        {
            fail("thng ARCH bake provenance version is unsupported");
        }
        const std::uint32_t payloadBytes = in.u32();
        if (payloadBytes > kMaxSectionBytes)
        {
            fail("thng ARCH bake provenance payload exceeds the safe size limit");
        }
        Reader trailer = in.bounded(payloadBytes);
        const std::uint32_t baselineCount = trailer.u32();
        if (baselineCount > kMaxMotsArchLightingRecords)
        {
            fail("thng ARCH bake provenance count exceeds the safe item limit");
        }
        std::size_t remainingMeshRecords = kMaxMotsArchLightingRecords;
        std::size_t remainingVertexRecords = kMaxMotsArchLightingRecords;
        const auto readObject = [&](Reader& reader)
        {
            MotsArchLightObject object{};
            object.wireIndex = reader.i32();
            object.meshes = reader.vectorBounded<MotsArchLightMesh>(
                static_cast<std::uint32_t>(kMaxMotsArchLightingRecords),
                "thng ARCH bake baseline mesh count exceeds the safe item limit.",
                [&remainingVertexRecords](Reader& meshReader)
                {
                    MotsArchLightMesh mesh{};
                    mesh.wireIndex = meshReader.i32();
                    mesh.vertices = meshReader.vectorBounded<MotsArchLightVertex>(
                        static_cast<std::uint32_t>(kMaxMotsArchLightingRecords),
                        "thng ARCH bake baseline vertex count exceeds the safe item limit.",
                        [](Reader& vertexReader)
                        {
                            return MotsArchLightVertex{
                                vertexReader.f32(), vertexReader.f32(),
                                vertexReader.f32(), vertexReader.f32() };
                        }, &remainingVertexRecords);
                    return mesh;
                }, &remainingMeshRecords);
            return object;
        };
        level.motsArchLightingBakeBaselines.reserve(baselineCount);
        for (std::uint32_t index = 0; index < baselineCount; ++index)
        {
            MotsArchLightingBakeBaseline baseline{};
            baseline.thingId = trailer.i32();
            baseline.modelName = trailer.string();
            baseline.originalArchLightingIndex = trailer.i32();
            if (trailer.boolean())
            {
                baseline.originalArchLighting = readObject(trailer);
            }
            level.motsArchLightingBakeBaselines.push_back(std::move(baseline));
        }
        if (!trailer.empty())
        {
            fail("thng ARCH bake provenance payload has an invalid length");
        }
        if (!in.empty())
        {
            fail("thng contains data after the ARCH bake provenance trailer");
        }
    }
}

Bytes encodeTemplates(const Level& level, const ResourceIndex& index)
{
    Writer out{ kMaxSectionBytes };
    out.vector(level.templates, [&index](Writer& writer, const ThingTemplate& value)
        {
            writeStringReference(writer, index, ResourceKind::templates, value.name);
            writeStringReference(writer, index, ResourceKind::templates, value.parent);
            writer.vector(value.values, [&index](Writer& nested, const TplValue& parameter)
                {
                    writeTplValue(nested, parameter, index);
                });
            writeStringReference(writer, index, ResourceKind::names, value.desc);
            writeBox(writer, value.bbox);
        });
    return out.take();
}

void decodeTemplates(Level& level, Reader& in,
    const ResourceCatalogs* const catalogs,
    const bool allowMissingTable)
{
    level.templates = in.vector<ThingTemplate>([catalogs, allowMissingTable](Reader& reader)
        {
            ThingTemplate value{};
            value.name = reader.stringReference(
                resourceCatalog(catalogs, ResourceKind::templates), allowMissingTable);
            value.parent = reader.stringReference(
                resourceCatalog(catalogs, ResourceKind::templates), allowMissingTable);
            value.values = reader.vector<TplValue>([catalogs, allowMissingTable](Reader& nested)
                {
                    return readTplValue(nested, catalogs, allowMissingTable);
                });
            value.desc = reader.stringReference(
                resourceCatalog(catalogs, ResourceKind::names), allowMissingTable);
            const TBox box = readBox(reader);
            static_cast<TBox&>(value.bbox) = box;
            return value;
        });
}

Bytes encodeCogs(const Level& level, const ResourceIndex& index)
{
    Writer out{ kMaxSectionBytes };
    out.vector(level.cogs, [&index](Writer& writer, const Cog& cog)
        {
            writer.i32(cog.id); writer.writeSedIndex(cog.num);
            writeStringReference(writer, index, ResourceKind::cogScripts, cog.name);
            writer.vector(cog.values, [&index](Writer& nested, const CogValue& value)
                {
                    writeCogValue(nested, value, index);
                });
        });
    return out.take();
}

void decodeCogs(Level& level, Reader& in,
    const ResourceCatalogs* const catalogs,
    const bool allowMissingTable)
{
    level.cogs = in.vector<Cog>([catalogs, allowMissingTable](Reader& reader)
        {
            Cog cog{};
            cog.id = reader.i32(); cog.num = reader.readSedIndex();
            cog.name = reader.stringReference(
                resourceCatalog(catalogs, ResourceKind::cogScripts), allowMissingTable);
            cog.values = reader.vector<CogValue>([catalogs, allowMissingTable](Reader& nested)
                {
                    return readCogValue(nested, catalogs, allowMissingTable);
                });
            return cog;
        });
}

Bytes encodeLights(const Level& level)
{
    Writer out{ kMaxSectionBytes };
    out.vector(level.lights, [](Writer& writer, const SedLight& light)
        {
            writer.i32(light.id); writer.i32(light.flags); writer.i32(light.mark); writer.writeSedIndex(light.layer);
            writeColor(writer, light.color); writer.f64(light.intensity); writer.f64(light.range);
            writeVector(writer, light.position);
        });
    return out.take();
}

void decodeLights(Level& level, Reader& in)
{
    level.lights = in.vector<SedLight>([](Reader& reader)
        {
            SedLight light{};
            light.id = reader.i32(); light.flags = reader.i32(); light.mark = reader.i32(); light.layer = reader.readSedIndex();
            light.color = readColor(reader); light.intensity = reader.f64(); light.range = reader.f64();
            light.position = readVector(reader);
            return light;
        });
}

Bytes encodeHierarchy(const Level& level, const ResourceIndex& index)
{
    Writer out{ kMaxSectionBytes };
    out.vector(level.h3doNodes, [&index](Writer& writer, const HNode& node)
        {
            writer.i32(node.id); writer.writeSedIndex(node.num); writer.u32(node.type);
            writer.writeSedIndex(node.mesh); writer.writeSedIndex(node.parent);
            writeVector(writer, node.position); writeVector(writer, node.rotation); writeVector(writer, node.pivot);
            writeStringReference(writer, index, ResourceKind::names, node.name);
        });
    return out.take();
}

void decodeHierarchy(Level& level, Reader& in,
    const ResourceCatalogs* const catalogs,
    const bool allowMissingTable)
{
    level.h3doNodes = in.vector<HNode>([catalogs, allowMissingTable](Reader& reader)
        {
            HNode node{};
            node.id = reader.i32(); node.num = reader.readSedIndex(); node.type = reader.u32();
            node.mesh = reader.readSedIndex(); node.parent = reader.readSedIndex();
            node.position = readVector(reader); node.rotation = readVector(reader); node.pivot = readVector(reader);
            node.name = reader.stringReference(
                resourceCatalog(catalogs, ResourceKind::names), allowMissingTable);
            return node;
        });
}

bool hasDecodedSection(const std::unordered_map<std::string, bool>& found,
    const std::string_view name)
{
    const auto it = found.find(std::string(name));
    return it != found.end() && it->second;
}

void requireFinite(const double value, const std::string_view field)
{
    if (!std::isfinite(value))
    {
        fail(std::string(field) + " is not finite");
    }
}

void requireFinite(const float value, const std::string_view field)
{
    if (!std::isfinite(value))
    {
        fail(std::string(field) + " is not finite");
    }
}

void validateFinitePoint(const TPoint2& value, const std::string_view field)
{
    requireFinite(value.x, field);
    requireFinite(value.y, field);
}

void validateFiniteVector(const TVector& value, const std::string_view field)
{
    requireFinite(value.x, field);
    requireFinite(value.y, field);
    requireFinite(value.z, field);
}

void validateFiniteColor(const ColorF& value, const std::string_view field)
{
    requireFinite(value.r, field);
    requireFinite(value.g, field);
    requireFinite(value.b, field);
    requireFinite(value.a, field);
}

void validateFiniteBox(const TBox& value, const std::string_view field)
{
    requireFinite(value.x1, field);
    requireFinite(value.y1, field);
    requireFinite(value.z1, field);
    requireFinite(value.x2, field);
    requireFinite(value.y2, field);
    requireFinite(value.z2, field);
}

bool isValidValueType(const ValueType type) noexcept
{
    switch (type)
    {
    case ValueType::str:
    case ValueType::integer:
    case ValueType::floating:
    case ValueType::flag:
    case ValueType::ptr:
    case ValueType::vect:
    case ValueType::frame:
    case ValueType::unknown:
        return true;
    }
    return false;
}

bool isValidAdjType(const AdjType type) noexcept
{
    switch (type)
    {
    case AdjType::unk:
    case AdjType::mat:
    case AdjType::cog:
    case AdjType::snd:
    case AdjType::pup:
    case AdjType::spr:
    case AdjType::par:
    case AdjType::model3do:
    case AdjType::ai:
    case AdjType::tpl:
    case AdjType::frame:
        return true;
    }
    return false;
}

bool isValidCogType(const CogType type) noexcept
{
    switch (type)
    {
    case CogType::unk:
    case CogType::ai:
    case CogType::cog:
    case CogType::key:
    case CogType::mat:
    case CogType::msg:
    case CogType::model3do:
    case CogType::sec:
    case CogType::wav:
    case CogType::surface:
    case CogType::template_:
    case CogType::thing:
    case CogType::int_:
    case CogType::float_:
    case CogType::vect:
        return true;
    }
    return false;
}

void validateValue(const Value& value, const std::string_view field)
{
    if (!isValidValueType(value.type))
    {
        fail(std::string(field) + " has an unknown value type");
    }
    requireFinite(value.floatValue, field);
}

void validateTplValue(const TplValue& value, const std::string_view field)
{
    validateValue(value, field);
    if (!isValidAdjType(value.adjType))
    {
        fail(std::string(field) + " has an unknown adjustment type");
    }
}

void validateCogValue(const CogValue& value, const std::string_view field)
{
    validateValue(value, field);
    if (!isValidCogType(value.cogType))
    {
        fail(std::string(field) + " has an unknown COG type");
    }
}

void validateDecodedLevel(const Level& level,
    const std::unordered_map<std::string, bool>& found,
    const std::span<const std::int32_t> sectorSubset = {})
{
    if (level.uvCoordinateSpace != UvCoordinateSpace::TexturePixels
        && level.uvCoordinateSpace != UvCoordinateSpace::Normalized)
    {
        fail("Level has an unknown UV coordinate space");
    }
    const bool hasHead = hasDecodedSection(found, "head");
    const bool hasSectors = hasDecodedSection(found, "sect");
    const bool hasThings = hasDecodedSection(found, "thng");
    const bool hasTemplates = hasDecodedSection(found, "tmpl");
    const bool hasCogs = hasDecodedSection(found, "cogs");
    const bool hasLights = hasDecodedSection(found, "lite");
    const bool hasHierarchy = hasDecodedSection(found, "hnod");

    if (hasHead)
    {
        const LevelHeader& header = level.header;
        requireFinite(header.gravity, "head gravity");
        requireFinite(header.horizonSky.distance, "head horizon distance");
        requireFinite(header.horizonSky.pixelsPerRev, "head horizon pixels-per-revolution");
        validateFinitePoint(header.horizonSky.offset, "head horizon offset");
        requireFinite(header.ceilingSky.height, "head ceiling height");
        validateFinitePoint(header.ceilingSky.offset, "head ceiling offset");
        for (const double value : header.mipmapDistances)
            requireFinite(value, "head mipmap distance");
        for (const double value : header.lodDistances)
            requireFinite(value, "head LOD distance");
        requireFinite(header.perspectiveDistance, "head perspective distance");
        requireFinite(header.gouraudDistance, "head Gouraud distance");
        validateFiniteColor(header.fog.color, "head fog color");
        requireFinite(header.fog.fogStart, "head fog start");
        requireFinite(header.fog.fogEnd, "head fog end");
        requireFinite(level.ppunit, "head pixels-per-unit");
    }

    if (hasSectors)
    {
        const auto validateSector = [](const Sector& sector)
        {
            validateFiniteColor(sector.ambient, "sect ambient color");
            validateFiniteColor(sector.extraLight, "sect extra-light color");
            validateFiniteColor(sector.pointLight.color, "sect point-light color");
            validateFiniteVector(sector.pointLight.position, "sect point-light position");
            requireFinite(sector.pointLight.minRange, "sect point-light minimum range");
            requireFinite(sector.pointLight.maxRange, "sect point-light maximum range");
            validateFiniteColor(sector.tint, "sect tint");
            requireFinite(sector.soundVolume, "sect sound volume");
            validateFiniteVector(sector.thrust, "sect thrust");
            validateFiniteBox(sector.collideBox, "sect collision box");
            for (const Vertex& vertex : sector.vertices)
            {
                requireFinite(vertex.x, "sect vertex");
                requireFinite(vertex.y, "sect vertex");
                requireFinite(vertex.z, "sect vertex");
            }
            for (const Surface& surface : sector.surfaces)
            {
                validateFiniteColor(surface.extraLight, "sect surface extra-light color");
                requireFinite(surface.uScale, "sect surface U scale");
                requireFinite(surface.vScale, "sect surface V scale");
                validateFiniteVector(surface.normal, "sect surface normal");
                for (const TexVertex& texVertex : surface.texVertices)
                {
                    requireFinite(texVertex.u, "sect surface U coordinate");
                    requireFinite(texVertex.v, "sect surface V coordinate");
                validateFiniteColor(texVertex.color, "sect texture-vertex color");
                }
            }
        };
        if (!sectorSubset.empty())
        {
            for (const std::int32_t index : sectorSubset)
            {
                if (index < 0 || static_cast<std::size_t>(index)
                        >= level.sectors.size())
                {
                    fail("sect subset index is out of range");
                }
                validateSector(level.sectors[static_cast<std::size_t>(index)]);
            }
        }
        else
        {
            for (const Sector& sector : level.sectors)
                validateSector(sector);
        }
    }

    if (hasThings)
    {
        if ((!level.motsArchLighting.empty()
                || !level.motsArchLightingBakeBaselines.empty())
            && level.kind != ProjectType::MOTS)
        {
            fail("thng ARCHLIGHTING table is only valid for MotS");
        }
        if (level.motsArchLighting.size() > kMaxMotsArchLightingRecords)
        {
            fail("thng ARCHLIGHTING object count is too large");
        }
        std::unordered_set<std::int32_t> objectIndices{};
        objectIndices.reserve(level.motsArchLighting.size());
        std::size_t encodedMeshRecords = 0;
        std::size_t encodedVertexRecords = 0;
        for (const MotsArchLightObject& object : level.motsArchLighting)
        {
            if (object.wireIndex < 0
                || !objectIndices.insert(object.wireIndex).second)
            {
                fail("thng ARCHLIGHTING object wire index is invalid or duplicated");
            }
            if (object.meshes.size() > kMaxMotsArchLightingRecords)
            {
                fail("thng ARCHLIGHTING mesh count is too large");
            }
            if (encodedMeshRecords > kMaxMotsArchLightingRecords
                - object.meshes.size())
            {
                fail("thng ARCHLIGHTING total mesh record count is too large");
            }
            encodedMeshRecords += object.meshes.size();

            std::unordered_set<std::int32_t> meshIndices{};
            meshIndices.reserve(object.meshes.size());
            for (const MotsArchLightMesh& mesh : object.meshes)
            {
                if (mesh.wireIndex < 0
                    || !meshIndices.insert(mesh.wireIndex).second)
                {
                    fail("thng ARCHLIGHTING mesh wire index is invalid or duplicated");
                }
                if (mesh.vertices.size() > kMaxMotsArchLightingRecords)
                {
                    fail("thng ARCHLIGHTING vertex count is too large");
                }
                if (encodedVertexRecords > kMaxMotsArchLightingRecords
                    - mesh.vertices.size())
                {
                    fail("thng ARCHLIGHTING total vertex record count is too large");
                }
                encodedVertexRecords += mesh.vertices.size();
                for (const MotsArchLightVertex& vertex : mesh.vertices)
                {
                    requireFinite(vertex.mono, "thng ARCHLIGHTING monochrome channel");
                    requireFinite(vertex.red, "thng ARCHLIGHTING red channel");
                    requireFinite(vertex.green, "thng ARCHLIGHTING green channel");
                    requireFinite(vertex.blue, "thng ARCHLIGHTING blue channel");
                }
            }
        }
        if (level.motsArchLightingBakeBaselines.size()
            > kMaxMotsArchLightingRecords)
        {
            fail("thng ARCH bake provenance count is too large");
        }
        std::unordered_set<std::int32_t> baselineThingIds{};
        baselineThingIds.reserve(level.motsArchLightingBakeBaselines.size());
        std::size_t baselineMeshes = 0;
        std::size_t baselineVertices = 0;
        for (const MotsArchLightingBakeBaseline& baseline :
             level.motsArchLightingBakeBaselines)
        {
            if (!baselineThingIds.insert(baseline.thingId).second)
            {
                fail("thng ARCH bake provenance Thing ID is duplicated");
            }
            if (baseline.modelName.empty()
                || baseline.modelName.size() > kMaxStringBytes)
            {
                fail("thng ARCH bake provenance model name is invalid");
            }
            if (baseline.originalArchLightingIndex < -1
                || ((baseline.originalArchLightingIndex == -1)
                    != !baseline.originalArchLighting.has_value()))
            {
                fail("thng ARCH bake provenance original reference is inconsistent");
            }
            if (!baseline.originalArchLighting) continue;
            const MotsArchLightObject& object = *baseline.originalArchLighting;
            if (object.wireIndex < 0
                || object.meshes.size() > kMaxMotsArchLightingRecords
                || baselineMeshes > kMaxMotsArchLightingRecords
                    - object.meshes.size())
            {
                fail("thng ARCH bake provenance object is invalid or too large");
            }
            baselineMeshes += object.meshes.size();
            std::unordered_set<std::int32_t> meshWireIndices{};
            meshWireIndices.reserve(object.meshes.size());
            for (const MotsArchLightMesh& mesh : object.meshes)
            {
                if (mesh.wireIndex < 0
                    || !meshWireIndices.insert(mesh.wireIndex).second
                    || mesh.vertices.size() > kMaxMotsArchLightingRecords
                    || baselineVertices > kMaxMotsArchLightingRecords
                        - mesh.vertices.size())
                {
                    fail("thng ARCH bake provenance mesh is invalid or too large");
                }
                baselineVertices += mesh.vertices.size();
                for (const MotsArchLightVertex& vertex : mesh.vertices)
                {
                    requireFinite(vertex.mono,
                        "thng ARCH bake baseline monochrome channel");
                    requireFinite(vertex.red, "thng ARCH bake baseline red channel");
                    requireFinite(vertex.green, "thng ARCH bake baseline green channel");
                    requireFinite(vertex.blue, "thng ARCH bake baseline blue channel");
                }
            }
        }
        for (const Thing& thing : level.things)
        {
            if (thing.archLightingIndex < -1
                || (thing.archLightingIndex >= 0
                    && static_cast<std::size_t>(thing.archLightingIndex)
                        >= level.motsArchLighting.size()))
            {
                fail("thng Thing ARCHLIGHTING reference is out of range");
            }
            if (thing.archLightingIndex >= 0
                && level.kind != ProjectType::MOTS)
            {
                fail("thng Thing ARCHLIGHTING reference is only valid for MotS");
            }
            requireFinite(thing.x, "thng position");
            requireFinite(thing.y, "thng position");
            requireFinite(thing.z, "thng position");
            requireFinite(thing.pch, "thng orientation");
            requireFinite(thing.yaw, "thng orientation");
            requireFinite(thing.rol, "thng orientation");
            validateFiniteBox(thing.bbox, "thng bounding box");
            for (const TplValue& value : thing.values)
                validateTplValue(value, "thng parameter");
        }
    }

    if (hasTemplates)
    {
        for (const ThingTemplate& value : level.templates)
        {
            validateFiniteBox(value.bbox, "tmpl bounding box");
            for (const TplValue& parameter : value.values)
                validateTplValue(parameter, "tmpl parameter");
        }
    }

    if (hasCogs)
    {
        for (const Cog& cog : level.cogs)
            for (const CogValue& value : cog.values)
                validateCogValue(value, "cogs parameter");
    }

    if (hasLights)
    {
        for (const SedLight& light : level.lights)
        {
            validateFiniteColor(light.color, "lite color");
            requireFinite(light.intensity, "lite intensity");
            requireFinite(light.range, "lite range");
            validateFiniteVector(light.position, "lite position");
        }
    }

    if (hasHierarchy)
    {
        for (const HNode& node : level.h3doNodes)
        {
            validateFiniteVector(node.position, "hnod position");
            validateFiniteVector(node.rotation, "hnod rotation");
            validateFiniteVector(node.pivot, "hnod pivot");
        }
    }
}

SedLevelDecodedFootprint estimateLevelDecodedFootprintInternal(
    const Level& level, const bool enforceLimits)
{
    SedLevelDecodedFootprint footprint = semanticRootFootprint();
    const auto add = [&](const SedLevelDecodedFootprint addition)
    {
        addSemanticFootprint(footprint, addition);
        if (enforceLimits) enforceSemanticFootprintLimits(footprint);
    };
    const auto string = [&](const std::string& value)
    {
        add(semanticStringFootprint(value.size()));
    };
    const auto vector = [&]<typename T>(const std::vector<T>& values)
    {
        add(semanticVectorFootprint(values.size(), sizeof(T)));
    };
    const auto value = [&](const Value& parameter)
    {
        string(parameter.name);
        string(parameter.stringValue);
    };

    if (hasRepresentedJklCogImportStateInternal(level))
    {
        vector(level.importedJklSurfaceRefs);
    }

    string(level.masterCMP);
    string(level.lvisstring);

    vector(level.colormaps);
    for (const std::string& entry : level.colormaps) string(entry);
    vector(level.materials);
    for (const std::string& entry : level.materials) string(entry);
    vector(level.layers);
    for (const std::string& entry : level.layers) string(entry);

    vector(level.sectors);
    for (const Sector& sector : level.sectors)
    {
        string(sector.colormap);
        string(sector.sound);
        vector(sector.vertices);
        vector(sector.surfaces);
        for (const Surface& surface : sector.surfaces)
        {
            string(surface.material);
            vector(surface.vertices);
            vector(surface.texVertices);
        }
    }

    vector(level.things);
    for (const Thing& thing : level.things)
    {
        string(thing.templateName);
        string(thing.name);
        vector(thing.values);
        for (const TplValue& parameter : thing.values) value(parameter);
    }

    vector(level.motsArchLighting);
    for (const MotsArchLightObject& object : level.motsArchLighting)
    {
        vector(object.meshes);
        for (const MotsArchLightMesh& mesh : object.meshes)
            vector(mesh.vertices);
    }

    vector(level.motsArchLightingBakeBaselines);
    for (const MotsArchLightingBakeBaseline& baseline :
         level.motsArchLightingBakeBaselines)
    {
        string(baseline.modelName);
        if (baseline.originalArchLighting)
        {
            vector(baseline.originalArchLighting->meshes);
            for (const MotsArchLightMesh& mesh :
                 baseline.originalArchLighting->meshes)
            {
                vector(mesh.vertices);
            }
        }
    }

    vector(level.templates);
    for (const ThingTemplate& thingTemplate : level.templates)
    {
        string(thingTemplate.name);
        string(thingTemplate.parent);
        vector(thingTemplate.values);
        for (const TplValue& parameter : thingTemplate.values) value(parameter);
        string(thingTemplate.desc);
    }

    vector(level.cogs);
    for (const Cog& cog : level.cogs)
    {
        string(cog.name);
        vector(cog.values);
        for (const CogValue& parameter : cog.values)
        {
            value(parameter);
            string(parameter.description);
        }
    }

    vector(level.lights);
    vector(level.h3doNodes);
    for (const HNode& node : level.h3doNodes) string(node.name);
    return footprint;
}

SedLevelDecodedFootprint validateLevelBeforeResourceIndex(const Level& level)
{
    // Reject an oversized or semantically invalid object graph before the
    // resource crawler copies its strings into categorized vectors/hash maps.
    // This is both the reader/writer symmetry gate and the writer's first
    // allocation boundary.
    SedLevelDecodedFootprint footprint =
        estimateLevelDecodedFootprintInternal(level, true);
    const std::unordered_map<std::string, bool> completeSections{
        { "head", true }, { "rsrc", true }, { "sect", true },
        { "thng", true }, { "tmpl", true }, { "cogs", true },
        { "lite", true }, { "hnod", true } };
    validateDecodedLevel(level, completeSections);
    if (hasRepresentedJklCogImportStateInternal(level))
    {
        validateJklCogImportSurfaceRefs(
            level, level.importedJklSurfaceRefs);
    }
    return footprint;
}

void validateResourceIndexForSerialization(SedLevelDecodedFootprint footprint,
    const ResourceIndex& resourceIndex)
{
    // The reader retains the canonical string table while it materializes
    // every indexed reference, so the writer admits that same temporary
    // footprint after the resource index has been built.
    for (const std::vector<std::string>& catalog : resourceIndex.catalogs())
    {
        addSemanticFootprint(footprint, semanticVectorFootprint(
            catalog.size(), sizeof(std::string)));
        enforceSemanticFootprintLimits(footprint);
        for (const std::string& value : catalog)
        {
            addSemanticFootprint(footprint,
                semanticStringFootprint(value.size()));
            enforceSemanticFootprintLimits(footprint);
        }
    }
}

struct SectionDescriptor final
{
    std::array<char, 4> type{};
    std::uint32_t version{};
    std::uint32_t flags{};
    std::uint64_t rawSize{};
    std::uint64_t storedSize{};
    std::uint64_t logicalStoredSize{};
    std::uint32_t dataStreams{};
    std::uint32_t parityStreams{};
    std::uint32_t chunkPayloadSize{};
    std::uint32_t stripeCount{};
    std::uint32_t codedOffset{};
    Digest256 expandedHash{};
};

std::array<char, 4> readType(const std::span<const std::byte> bytes);

bool isValidSectionType(const std::array<char, 4>& type) noexcept
{
    return std::all_of(type.begin(), type.end(), [](const char value)
        {
            return (value >= 'a' && value <= 'z')
                || (value >= '0' && value <= '9');
        });
}

Bytes encodeSectionDescriptor(const SectionDescriptor& descriptor)
{
    Writer out{};
    out.raw(kSectionAnchorMagic);
    out.raw(std::as_bytes(std::span(descriptor.type)));
    out.u32(descriptor.version);
    out.u32(descriptor.flags);
    out.u64(descriptor.rawSize);
    out.u64(descriptor.storedSize);
    out.u64(descriptor.logicalStoredSize);
    out.u32(descriptor.dataStreams);
    out.u32(descriptor.parityStreams);
    out.u32(descriptor.chunkPayloadSize);
    out.u32(descriptor.stripeCount);
    out.u32(descriptor.codedOffset);
    out.raw(descriptor.expandedHash);
    const Bytes body = out.take();
    if (body.size() != kSectionDescriptorSize - 4)
    {
        throw std::logic_error("Binary SED descriptor size drifted.");
    }
    Writer result{};
    result.raw(body);
    result.u32(crc32c(body));
    return result.take();
}

std::optional<SectionDescriptor> parseSectionDescriptor(
    const std::span<const std::byte> bytes)
{
    if (bytes.size() != kSectionDescriptorSize
        || !std::equal(kSectionAnchorMagic.begin(), kSectionAnchorMagic.end(), bytes.begin())
        || crc32c(bytes.first(kSectionDescriptorSize - 4))
            != readU32At(bytes, kSectionDescriptorSize - 4))
    {
        return std::nullopt;
    }
    Reader in(bytes.subspan(kSectionAnchorMagic.size(), kSectionDescriptorSize
        - kSectionAnchorMagic.size() - 4));
    SectionDescriptor descriptor{};
    descriptor.type = readType(in.raw(4));
    descriptor.version = in.u32();
    descriptor.flags = in.u32();
    descriptor.rawSize = in.u64();
    descriptor.storedSize = in.u64();
    descriptor.logicalStoredSize = in.u64();
    descriptor.dataStreams = in.u32();
    descriptor.parityStreams = in.u32();
    descriptor.chunkPayloadSize = in.u32();
    descriptor.stripeCount = in.u32();
    descriptor.codedOffset = in.u32();
    const auto hash = in.raw(descriptor.expandedHash.size());
    std::copy(hash.begin(), hash.end(), descriptor.expandedHash.begin());
    const std::uint64_t maximumRawSize = descriptor.type == kHistorySectionType
        ? SedMaximumHistorySectionBytes : kMaxSectionBytes;
    if (!in.empty() || !isValidSectionType(descriptor.type)
        || !isSupportedSedBinaryVersion(descriptor.version)
        || (descriptor.flags
            & ~(kCompressed | kParityProtected | kHistoryNestedChecksum)) != 0
        || descriptor.rawSize > maximumRawSize
        || descriptor.storedSize > kMaxCodedSectionBytes)
    {
        return std::nullopt;
    }
    const bool protectedPayload = (descriptor.flags & kParityProtected) != 0;
    const bool historyNested =
        (descriptor.flags & kHistoryNestedChecksum) != 0;
    if (descriptor.logicalStoredSize > kMaxSectionBytes
        || (historyNested
            && ((descriptor.flags & (kCompressed | kParityProtected)) != 0
                || !std::all_of(
                    descriptor.expandedHash.begin() + 4,
                    descriptor.expandedHash.end(),
                    [](const std::byte value)
                    {
                        return value == std::byte{0};
                    })))
        || (protectedPayload
            && (descriptor.dataStreams == 0
                || descriptor.dataStreams > kFecDataStreams
                || (descriptor.parityStreams != kFecOptionalParityStreams
                    && descriptor.parityStreams != kFecRequiredParityStreams)
                || descriptor.chunkPayloadSize != kFecSlotPayloadBytes
                || descriptor.stripeCount == 0
                || descriptor.codedOffset >= kFecSlotBytes))
        || (!protectedPayload
            && (descriptor.logicalStoredSize != descriptor.storedSize
                || descriptor.dataStreams != 0 || descriptor.parityStreams != 0
                || descriptor.chunkPayloadSize != 0 || descriptor.stripeCount != 0
                || descriptor.codedOffset != 0)))
    {
        return std::nullopt;
    }
    const bool compressedPayload = (descriptor.flags & kCompressed) != 0;
    if ((!compressedPayload && descriptor.logicalStoredSize != descriptor.rawSize)
        || (compressedPayload
            && (descriptor.rawSize < kCompressionMinimumBytes
                || descriptor.rawSize > kCompressionMaximumBytes
                || descriptor.logicalStoredSize >= descriptor.rawSize)))
    {
        return std::nullopt;
    }
    if (protectedPayload)
    {
        const std::uint64_t slotCount = static_cast<std::uint64_t>(
            descriptor.dataStreams + descriptor.parityStreams)
            * descriptor.stripeCount;
        const std::uint64_t maximumLogical = static_cast<std::uint64_t>(
            descriptor.dataStreams) * descriptor.stripeCount
            * descriptor.chunkPayloadSize;
        const std::uint64_t minimumChunks = std::max<std::uint64_t>(1,
            (descriptor.logicalStoredSize + kFecSlotPayloadBytes - 1u)
                / kFecSlotPayloadBytes);
        const std::uint32_t expectedStripeCount = static_cast<std::uint32_t>(
            (minimumChunks + kFecDataStreams - 1u) / kFecDataStreams);
        const std::uint32_t expectedDataStreams = static_cast<std::uint32_t>(
            (minimumChunks + expectedStripeCount - 1u) / expectedStripeCount);
        if (descriptor.stripeCount != expectedStripeCount
            || descriptor.dataStreams != expectedDataStreams
            || slotCount > kMaxCodedSectionBytes / kFecSlotBytes
            || descriptor.storedSize
                != descriptor.codedOffset + slotCount * kFecSlotBytes
            || descriptor.logicalStoredSize > maximumLogical)
        {
            return std::nullopt;
        }
    }
    return descriptor;
}

// `parseSectionDescriptor` intentionally returns null for both random damage
// and schema-invalid descriptors.  Callers that are positioned at a checked
// descriptor anchor must distinguish those cases: a descriptor whose own
// CRC32C is valid but whose V1 schema/layout is unsupported is not repairable
// corruption and must remain a hard format error.
bool hasValidSectionDescriptorChecksum(
    const std::span<const std::byte> bytes) noexcept
{
    return bytes.size() == kSectionDescriptorSize
        && std::equal(kSectionAnchorMagic.begin(), kSectionAnchorMagic.end(), bytes.begin())
        && crc32c(bytes.first(kSectionDescriptorSize - 4))
            == readU32At(bytes, kSectionDescriptorSize - 4);
}

bool hasCanonicalFecAlignment(const std::size_t descriptorOffset,
    const SectionDescriptor& descriptor,
    const std::size_t absoluteBase = 0) noexcept
{
    if ((descriptor.flags & kParityProtected) == 0)
    {
        return descriptor.codedOffset == 0;
    }
    // `absoluteBase` is non-zero only for a headerless recovery stream.  Its
    // first descriptor is physically at offset zero in the stream but after
    // the complete canonical fixed header in a framed document.
    return (absoluteBase + descriptorOffset + kSectionDescriptorSize
            + descriptor.codedOffset) % kFecSlotBytes == 0;
}

Bytes encodeSectionTrailer(const std::span<const std::byte> descriptor)
{
    if (descriptor.size() != kSectionDescriptorSize)
    {
        throw std::logic_error("Binary SED trailer descriptor size is invalid.");
    }
    Writer out{};
    out.raw(kSectionMirrorMagic);
    out.u32(kSectionDescriptorSize);
    out.raw(descriptor);
    out.u32(crc32c(out.bytes()));
    return out.take();
}

std::optional<std::span<const std::byte>> readSectionTrailer(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept
{
    if (offset > bytes.size() || kSectionTrailerSize > bytes.size() - offset)
    {
        return std::nullopt;
    }
    const auto trailer = bytes.subspan(offset, kSectionTrailerSize);
    if (!std::equal(kSectionMirrorMagic.begin(), kSectionMirrorMagic.end(), trailer.begin())
        || readU32At(trailer, kSectionMirrorMagic.size()) != kSectionDescriptorSize
        || crc32c(trailer.first(kSectionTrailerSize - 4))
            != readU32At(trailer, kSectionTrailerSize - 4))
    {
        return std::nullopt;
    }
    return trailer.subspan(kSectionMirrorMagic.size() + 4, kSectionDescriptorSize);
}

std::span<const std::byte> historyNestedFramePayload(
    const std::vector<std::byte>& frame)
{
    if (frame.size() < kSectionHeaderSize + kSectionTrailerSize)
    {
        throw std::logic_error("Generated SED frame is too small.");
    }
    const auto descriptor = parseSectionDescriptor(
        std::span<const std::byte>(frame).first(kSectionHeaderSize));
    const std::size_t trailerOffset = kSectionHeaderSize
        + (descriptor ? static_cast<std::size_t>(descriptor->storedSize) : 0u);
    const auto trailer = descriptor
        ? readSectionTrailer(std::span<const std::byte>(frame), trailerOffset)
        : std::nullopt;
    const auto trailerDescriptor = trailer
        ? parseSectionDescriptor(*trailer) : std::nullopt;
    if (!descriptor || descriptor->flags != kHistoryNestedChecksum
        || descriptor->rawSize != descriptor->storedSize
        || descriptor->storedSize
            != frame.size() - kSectionHeaderSize - kSectionTrailerSize
        || !trailerDescriptor
        || !std::equal(
            frame.begin(),
            frame.begin() + static_cast<std::ptrdiff_t>(kSectionHeaderSize),
            trailer->begin(), trailer->end()))
    {
        throw std::logic_error(
            "Generated SED frame is not a canonical history-nested section.");
    }
    return std::span<const std::byte>(frame).subspan(
        kSectionHeaderSize, static_cast<std::size_t>(descriptor->rawSize));
}

void appendSection(Writer& file, const std::array<char, 4>& type,
    const std::span<const std::byte> raw,
    const std::uint32_t parityStreams, const bool compressPayload,
    const std::size_t absoluteSectionOffset,
    const bool historyNested,
    const std::uint32_t binaryVersion)
{
    if (!isSupportedSedBinaryVersion(binaryVersion))
    {
        throw std::logic_error(
            "Binary SED section version is unsupported.");
    }
    if (historyNested && (parityStreams != 0 || compressPayload))
    {
        throw std::logic_error(
            "History-nested sections cannot use compression or parity.");
    }
    const std::uint64_t maximumRawSize = type == kHistorySectionType
        ? SedMaximumHistorySectionBytes : kMaxSectionBytes;
    if (raw.size() > maximumRawSize)
    {
        throw std::runtime_error(type == kHistorySectionType
            ? "Binary SED hist section exceeds the V1 64 MiB safety limit."
            : "Binary SED section exceeds the safe semantic size limit.");
    }
    const Bytes compressed = compressPayload
            && raw.size() >= kCompressionMinimumBytes
            && raw.size() <= kCompressionMaximumBytes
        ? compressFast(raw) : Bytes{};
    const bool useCompression = !compressed.empty() && compressed.size() < raw.size();
    const std::span<const std::byte> logicalStored = useCompression
        ? std::span<const std::byte>(compressed)
        : raw;
    std::uint64_t predictedPayloadBytes = logicalStored.size();
    std::uint32_t predictedCodedOffset = 0;
    if (parityStreams != 0)
    {
        const std::uint64_t minimumChunks = std::max<std::uint64_t>(1,
            (static_cast<std::uint64_t>(logicalStored.size())
                + kFecSlotPayloadBytes - 1u) / kFecSlotPayloadBytes);
        const std::uint32_t predictedStripeCount = static_cast<std::uint32_t>(
            (minimumChunks + kFecDataStreams - 1u) / kFecDataStreams);
        const std::uint32_t predictedDataStreams = static_cast<std::uint32_t>(
            (minimumChunks + predictedStripeCount - 1u) / predictedStripeCount);
        const std::uint64_t slotCount = static_cast<std::uint64_t>(
            predictedDataStreams + parityStreams) * predictedStripeCount;
        predictedCodedOffset = static_cast<std::uint32_t>((kFecSlotBytes
            - ((absoluteSectionOffset + kSectionDescriptorSize) % kFecSlotBytes))
            % kFecSlotBytes);
        if (slotCount > kMaxCodedSectionBytes / kFecSlotBytes)
        {
            throw std::runtime_error("Binary SED protected section exceeds the safe size limit.");
        }
        predictedPayloadBytes = static_cast<std::uint64_t>(predictedCodedOffset)
            + slotCount * kFecSlotBytes;
    }
    constexpr std::uint64_t kMaximumBodyBytes =
        kMaxFileBytes - kHeaderMirrorSize - kFooterSize;
    if (file.bytes().size() > kMaximumBodyBytes
        || predictedPayloadBytes > kMaximumBodyBytes - file.bytes().size()
        || kSectionDescriptorSize + kSectionTrailerSize
            > kMaximumBodyBytes - file.bytes().size() - predictedPayloadBytes)
    {
        throw std::runtime_error("Binary SED output exceeds the safe file limit.");
    }
    const EncodedFecPayload protectedPayload = parityStreams != 0
        ? encodeFecPayload(logicalStored, parityStreams) : EncodedFecPayload{};
    const std::uint32_t codedOffset = parityStreams == 0 ? 0u
        : predictedCodedOffset;
    const std::uint64_t storedSize = parityStreams != 0
        ? static_cast<std::uint64_t>(codedOffset) + protectedPayload.bytes.size()
        : logicalStored.size();
    if (storedSize > kMaxCodedSectionBytes)
    {
        throw std::runtime_error("Binary SED protected section exceeds the safe size limit.");
    }
    const SectionDescriptor descriptor{
        type,
        binaryVersion,
        (useCompression ? kCompressed : 0u)
            | (parityStreams != 0 ? kParityProtected : 0u)
            | (historyNested ? kHistoryNestedChecksum : 0u),
        raw.size(),
        storedSize,
        logicalStored.size(),
        protectedPayload.dataStreams,
        protectedPayload.parityStreams,
        parityStreams != 0 ? kFecSlotPayloadBytes : 0u,
        protectedPayload.stripeCount,
        codedOffset,
        historyNested ? historyNestedChecksum(raw) : sha256(raw) };
    const Bytes descriptorBytes = encodeSectionDescriptor(descriptor);
    const std::uint64_t availableBodyBytes = kMaximumBodyBytes
        - file.bytes().size();
    constexpr std::uint64_t kSectionFramingBytes =
        kSectionDescriptorSize + kSectionTrailerSize;
    if (availableBodyBytes < kSectionFramingBytes
        || storedSize > availableBodyBytes - kSectionFramingBytes)
    {
        // Keep the reserve request independently checked even though the
        // predicted compressed/FEC extent was admitted before encoding.
        // This also catches an encoder/layout drift before a size_t cast.
        throw std::runtime_error("Binary SED output exceeds the safe file limit.");
    }
    const std::uint64_t sectionBytes = kSectionFramingBytes + storedSize;
    if (sectionBytes > std::numeric_limits<std::size_t>::max()
        || file.bytes().size() > std::numeric_limits<std::size_t>::max()
            - static_cast<std::size_t>(sectionBytes))
    {
        throw std::runtime_error("Binary SED output exceeds the host size limit.");
    }
    file.reserveAtLeast(file.bytes().size() + static_cast<std::size_t>(sectionBytes));
    file.raw(descriptorBytes);
    if (parityStreams != 0)
    {
        const Bytes alignment(codedOffset, std::byte{0});
        file.raw(alignment);
        file.raw(protectedPayload.bytes);
    }
    else
    {
        file.raw(logicalStored);
    }
    file.raw(encodeSectionTrailer(descriptorBytes));
}

std::array<char, 4> readType(const std::span<const std::byte> bytes)
{
    if (bytes.size() != 4) fail("invalid section type");
    std::array<char, 4> result{};
    std::memcpy(result.data(), bytes.data(), 4);
    return result;
}

std::string typeName(const std::array<char, 4>& type)
{
    return std::string(type.data(), type.size());
}

bool isRequiredSection(const std::string_view name) noexcept
{
    return name == "head" || name == "rsrc" || name == "sect" || name == "thng"
        || name == "tmpl" || name == "cogs" || name == "lite" || name == "hnod";
}

// `edit` is semantically optional: a valid Level must remain loadable when the
// editor workspace is absent.  Its payload can nevertheless contain the
// user's only unsaved COG source text, so it receives the same p=4 physical
// protection tier as the core Level sections.  Keep this decision separate
// from isRequiredSection() so semantic optionality is never conflated with
// the corruption-recovery budget.
bool isCoreProtectionSection(const std::string_view name) noexcept
{
    return isRequiredSection(name) || name == "edit" || name == "jimp";
}

void validateOptionalSections(const std::span<const SedOptionalSection> sections)
{
    if (sections.size() > 1024u - kRequiredSectionCount)
    {
        throw std::runtime_error("Too many optional binary SED sections.");
    }
    std::unordered_map<std::string, bool> found{};
    for (const SedOptionalSection& section : sections)
    {
        const std::string name = typeName(section.type);
        if (isRequiredSection(name))
        {
            throw std::runtime_error("Optional binary SED section collides with required section " + name + ".");
        }
        if (section.type == kJklCogImportSectionType)
        {
            throw std::runtime_error(
                "Optional binary SED section jimp is owned by the core codec.");
        }
        if (found[name])
        {
            throw std::runtime_error("Duplicate optional binary SED section " + name + ".");
        }
        for (const char value : section.type)
        {
            if ((value < 'a' || value > 'z') && (value < '0' || value > '9'))
            {
                throw std::runtime_error("Optional binary SED section type must contain four lowercase ASCII letters or digits.");
            }
        }
        if (section.payload.size() > kMaxSectionBytes)
        {
            throw std::runtime_error("Optional binary SED section exceeds the safe size limit.");
        }
        found[name] = true;
    }
}

// Read streams in bounded chunks.  The old istreambuf_iterator constructor
// let an untrusted stream grow a vector without an early format-size check;
// chunking keeps the growth observable and rejects anything beyond the V1
// file limit before another allocation is attempted.
Bytes readStreamBounded(std::istream& in,
    const std::optional<std::uint64_t> expectedSize = std::nullopt)
{
    constexpr std::size_t kReadChunkBytes = 64u * 1024u;
    constexpr std::size_t kKnownSizeReadChunkBytes = 1024u * 1024u;
    Bytes bytes{};
    if (expectedSize)
    {
        if (*expectedSize > kMaxFileBytes
            || *expectedSize > std::numeric_limits<std::size_t>::max())
        {
            fail("file size is outside the safe range");
        }
        // file_size() has already admitted this extent against the global
        // 256 MiB cap. Read directly into one exact allocation instead of
        // copying every chunk from a stack buffer through vector::insert().
        // A one-byte probe retains the old growth-race check. Short/device
        // reads return a partial/empty vector so the file-path callers can
        // release it before their independently checked resilient pass.
        const std::size_t size = static_cast<std::size_t>(*expectedSize);
        bytes.resize(size);
        std::size_t filled{};
        while (filled < size)
        {
            const std::size_t request = std::min(
                kKnownSizeReadChunkBytes, size - filled);
            in.read(reinterpret_cast<char*>(bytes.data() + filled),
                static_cast<std::streamsize>(request));
            const std::streamsize count = in.gcount();
            if (count <= 0)
            {
                bytes.resize(filled);
                return bytes;
            }
            filled += static_cast<std::size_t>(count);
            if (static_cast<std::size_t>(count) != request)
            {
                bytes.resize(filled);
                return bytes;
            }
        }

        char extra{};
        in.read(&extra, 1);
        if (in.gcount() != 0)
        {
            throw std::runtime_error(
                "Binary SED stream exceeded its reported size.");
        }
        if (!in.eof() && !in)
        {
            bytes.clear();
        }
        return bytes;
    }
    std::array<char, kReadChunkBytes> buffer{};
    while (true)
    {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = in.gcount();
        if (count > 0)
        {
            if (expectedSize
                && static_cast<std::uint64_t>(bytes.size())
                    + static_cast<std::uint64_t>(count) > *expectedSize)
            {
                throw std::runtime_error(
                    "Binary SED stream exceeded its reported size.");
            }
            if (bytes.size() > kMaxFileBytes - static_cast<std::size_t>(count))
            {
                fail("file exceeds the safe size limit");
            }
            const auto chunk = std::as_bytes(std::span(
                buffer.data(), static_cast<std::size_t>(count)));
            bytes.insert(bytes.end(), chunk.begin(), chunk.end());
        }
        if (in.eof()) break;
        if (!in)
        {
            if (expectedSize) break;
            throw std::runtime_error("Failed while reading binary SED file.");
        }
    }
    return bytes;
}

// The binary codec's round-trip tests must compare the semantic model rather
// than only a handful of representative fields.  Keep this helper local to
// the codec implementation and expose the small public wrapper below.  IEEE
// bit comparisons are intentional: SED preserves authored -0 and NaN payload
// bits just as it preserves every other floating-point value.
bool sameFloat(const float lhs, const float rhs) noexcept
{
    return std::bit_cast<std::uint32_t>(lhs) == std::bit_cast<std::uint32_t>(rhs);
}

bool sameDouble(const double lhs, const double rhs) noexcept
{
    return std::bit_cast<std::uint64_t>(lhs) == std::bit_cast<std::uint64_t>(rhs);
}

bool sameColor(const ColorF& lhs, const ColorF& rhs) noexcept
{
    return sameFloat(lhs.r, rhs.r) && sameFloat(lhs.g, rhs.g)
        && sameFloat(lhs.b, rhs.b) && sameFloat(lhs.a, rhs.a);
}

bool samePoint2(const TPoint2& lhs, const TPoint2& rhs) noexcept
{
    return sameDouble(lhs.x, rhs.x) && sameDouble(lhs.y, rhs.y);
}

bool sameVector(const TVector& lhs, const TVector& rhs) noexcept
{
    return sameDouble(lhs.x, rhs.x) && sameDouble(lhs.y, rhs.y)
        && sameDouble(lhs.z, rhs.z);
}

bool sameBox(const TBox& lhs, const TBox& rhs) noexcept
{
    return sameDouble(lhs.x1, rhs.x1) && sameDouble(lhs.y1, rhs.y1)
        && sameDouble(lhs.z1, rhs.z1) && sameDouble(lhs.x2, rhs.x2)
        && sameDouble(lhs.y2, rhs.y2) && sameDouble(lhs.z2, rhs.z2);
}

template <typename Container, typename Predicate>
bool sameSequence(const Container& lhs, const Container& rhs,
    Predicate predicate) noexcept
{
    return lhs.size() == rhs.size()
        && std::equal(lhs.begin(), lhs.end(), rhs.begin(), predicate);
}

bool sameArchLighting(const MotsArchLightObject& lhs,
    const MotsArchLightObject& rhs) noexcept
{
    return lhs.wireIndex == rhs.wireIndex
        && lhs.meshes.size() == rhs.meshes.size()
        && std::equal(lhs.meshes.begin(), lhs.meshes.end(), rhs.meshes.begin(),
            [](const MotsArchLightMesh& left, const MotsArchLightMesh& right)
                noexcept
            {
                return left.wireIndex == right.wireIndex
                    && left.vertices.size() == right.vertices.size()
                    && std::equal(left.vertices.begin(), left.vertices.end(),
                        right.vertices.begin(),
                        [](const MotsArchLightVertex& a,
                            const MotsArchLightVertex& b) noexcept
                        {
                            return sameFloat(a.mono, b.mono)
                                && sameFloat(a.red, b.red)
                                && sameFloat(a.green, b.green)
                                && sameFloat(a.blue, b.blue);
                        });
            });
}

bool sameArchLightingSequence(
    const std::vector<MotsArchLightObject>& lhs,
    const std::vector<MotsArchLightObject>& rhs) noexcept
{
    return sameSequence(lhs, rhs, sameArchLighting);
}

bool sameMotsArchLightingBakeBaseline(
    const MotsArchLightingBakeBaseline& lhs,
    const MotsArchLightingBakeBaseline& rhs) noexcept
{
    return lhs.thingId == rhs.thingId
        && lhs.modelName == rhs.modelName
        && lhs.originalArchLightingIndex == rhs.originalArchLightingIndex
        && lhs.originalArchLighting.has_value()
            == rhs.originalArchLighting.has_value()
        && (!lhs.originalArchLighting
            || sameArchLighting(
                *lhs.originalArchLighting, *rhs.originalArchLighting));
}

bool sameMotsArchLightingBakeBaselineSequence(
    const std::vector<MotsArchLightingBakeBaseline>& lhs,
    const std::vector<MotsArchLightingBakeBaseline>& rhs) noexcept
{
    return sameSequence(lhs, rhs, sameMotsArchLightingBakeBaseline);
}

bool sameValue(const Value& lhs, const Value& rhs) noexcept
{
    return lhs.name == rhs.name && lhs.type == rhs.type
        && lhs.intValue == rhs.intValue && sameDouble(lhs.floatValue, rhs.floatValue)
        && lhs.stringValue == rhs.stringValue;
}

bool sameTplValue(const TplValue& lhs, const TplValue& rhs) noexcept
{
    return sameValue(lhs, rhs) && lhs.adjType == rhs.adjType;
}

bool sameCogValue(const CogValue& lhs, const CogValue& rhs) noexcept
{
    return sameValue(lhs, rhs) && lhs.cogType == rhs.cogType
        && lhs.local == rhs.local && lhs.description == rhs.description
        && lhs.mask == rhs.mask;
}

bool sameTexVertex(const TexVertex& lhs, const TexVertex& rhs) noexcept
{
    return sameFloat(lhs.u, rhs.u) && sameFloat(lhs.v, rhs.v)
        && sameColor(lhs.color, rhs.color);
}

bool sameTexVertexIgnoringColor(
    const TexVertex& lhs,
    const TexVertex& rhs,
    const bool ignoreColor) noexcept
{
    return sameFloat(lhs.u, rhs.u) && sameFloat(lhs.v, rhs.v)
        && (ignoreColor || sameColor(lhs.color, rhs.color));
}

bool sameSurface(const Surface& lhs, const Surface& rhs) noexcept
{
    if (lhs.num != rhs.num || lhs.mark != rhs.mark || lhs.d3dId != rhs.d3dId
        || lhs.surfaceFlags != rhs.surfaceFlags || lhs.faceFlags != rhs.faceFlags
        || lhs.geo != rhs.geo || lhs.light != rhs.light || lhs.tex != rhs.tex
        || !sameColor(lhs.extraLight, rhs.extraLight)
        || lhs.adjoinFlags != rhs.adjoinFlags || lhs.material != rhs.material
        || lhs.nAdj != rhs.nAdj || lhs.nMat != rhs.nMat
        || !sameFloat(lhs.uScale, rhs.uScale) || !sameFloat(lhs.vScale, rhs.vScale)
        || !sameVector(lhs.normal, rhs.normal)
        || lhs.vertices != rhs.vertices)
    {
        return false;
    }
    if (lhs.adjoin.has_value() != rhs.adjoin.has_value()) return false;
    if (lhs.adjoin && *lhs.adjoin != *rhs.adjoin) return false;
    return sameSequence(lhs.texVertices, rhs.texVertices, sameTexVertex);
}

bool sameSurfaceIgnoringBakedLighting(
    const Surface& lhs,
    const Surface& rhs,
    const bool ignoreBakedLighting) noexcept
{
    if (lhs.num != rhs.num || lhs.mark != rhs.mark || lhs.d3dId != rhs.d3dId
        || lhs.surfaceFlags != rhs.surfaceFlags || lhs.faceFlags != rhs.faceFlags
        || lhs.geo != rhs.geo || lhs.light != rhs.light || lhs.tex != rhs.tex
        || !sameColor(lhs.extraLight, rhs.extraLight)
        || lhs.adjoinFlags != rhs.adjoinFlags || lhs.material != rhs.material
        || lhs.nAdj != rhs.nAdj || lhs.nMat != rhs.nMat
        || !sameFloat(lhs.uScale, rhs.uScale) || !sameFloat(lhs.vScale, rhs.vScale)
        || !sameVector(lhs.normal, rhs.normal)
        || lhs.vertices != rhs.vertices)
    {
        return false;
    }
    if (lhs.adjoin.has_value() != rhs.adjoin.has_value()) return false;
    if (lhs.adjoin && *lhs.adjoin != *rhs.adjoin) return false;
    return lhs.texVertices.size() == rhs.texVertices.size()
        && std::equal(lhs.texVertices.begin(), lhs.texVertices.end(),
            rhs.texVertices.begin(),
            [ignoreBakedLighting](const TexVertex& a, const TexVertex& b)
            {
                return sameTexVertexIgnoringColor(a, b, ignoreBakedLighting);
            });
}

bool sameSector(const Sector& lhs, const Sector& rhs) noexcept
{
    const PointLight& lhsLight = lhs.pointLight;
    const PointLight& rhsLight = rhs.pointLight;
    return lhs.id == rhs.id && lhs.num == rhs.num && lhs.flags == rhs.flags
        && sameColor(lhs.ambient, rhs.ambient)
        && sameColor(lhs.extraLight, rhs.extraLight)
        && sameColor(lhsLight.color, rhsLight.color)
        && sameVector(lhsLight.position, rhsLight.position)
        && sameDouble(lhsLight.minRange, rhsLight.minRange)
        && sameDouble(lhsLight.maxRange, rhsLight.maxRange)
        && lhs.colormap == rhs.colormap && sameColor(lhs.tint, rhs.tint)
        && lhs.sound == rhs.sound && sameDouble(lhs.soundVolume, rhs.soundVolume)
        && sameVector(lhs.thrust, rhs.thrust)
        && sameBox(lhs.collideBox, rhs.collideBox) && lhs.layer == rhs.layer
        && lhs.mark == rhs.mark
        && sameSequence(lhs.vertices, rhs.vertices,
            [](const Vertex& a, const Vertex& b) noexcept
            {
                return sameDouble(a.x, b.x) && sameDouble(a.y, b.y)
                    && sameDouble(a.z, b.z) && a.mark == b.mark;
            })
        && sameSequence(lhs.surfaces, rhs.surfaces, sameSurface);
}

bool sameSectorIgnoringBakedLighting(
    const Sector& lhs,
    const Sector& rhs,
    const bool ignoreBakedLighting) noexcept
{
    const PointLight& lhsLight = lhs.pointLight;
    const PointLight& rhsLight = rhs.pointLight;
    if (lhs.id != rhs.id || lhs.num != rhs.num || lhs.flags != rhs.flags
        || (!ignoreBakedLighting && !sameColor(lhs.ambient, rhs.ambient))
        || !sameColor(lhs.extraLight, rhs.extraLight)
        || !sameColor(lhsLight.color, rhsLight.color)
        || !sameVector(lhsLight.position, rhsLight.position)
        || !sameDouble(lhsLight.minRange, rhsLight.minRange)
        || !sameDouble(lhsLight.maxRange, rhsLight.maxRange)
        || lhs.colormap != rhs.colormap || !sameColor(lhs.tint, rhs.tint)
        || lhs.sound != rhs.sound || !sameDouble(lhs.soundVolume, rhs.soundVolume)
        || !sameVector(lhs.thrust, rhs.thrust)
        || !sameBox(lhs.collideBox, rhs.collideBox) || lhs.layer != rhs.layer
        || lhs.mark != rhs.mark)
    {
        return false;
    }
    if (lhs.vertices.size() != rhs.vertices.size()
        || !std::equal(lhs.vertices.begin(), lhs.vertices.end(), rhs.vertices.begin(),
            [](const Vertex& a, const Vertex& b) noexcept
            {
                return sameDouble(a.x, b.x) && sameDouble(a.y, b.y)
                    && sameDouble(a.z, b.z) && a.mark == b.mark;
            }))
    {
        return false;
    }
    return lhs.surfaces.size() == rhs.surfaces.size()
        && std::equal(lhs.surfaces.begin(), lhs.surfaces.end(), rhs.surfaces.begin(),
            [ignoreBakedLighting](const Surface& a, const Surface& b) noexcept
            {
                return sameSurfaceIgnoringBakedLighting(
                    a, b, ignoreBakedLighting);
            });
}

bool sameTemplate(const ThingTemplate& lhs, const ThingTemplate& rhs) noexcept
{
    return lhs.name == rhs.name && lhs.parent == rhs.parent
        && sameSequence(lhs.values, rhs.values, sameTplValue)
        && lhs.desc == rhs.desc && sameBox(lhs.bbox, rhs.bbox);
}

bool sameThing(const Thing& lhs, const Thing& rhs) noexcept
{
    return lhs.id == rhs.id && lhs.num == rhs.num && lhs.sector == rhs.sector
        && lhs.archLightingIndex == rhs.archLightingIndex
        && lhs.templateName == rhs.templateName && lhs.name == rhs.name
        && sameDouble(lhs.x, rhs.x) && sameDouble(lhs.y, rhs.y)
        && sameDouble(lhs.z, rhs.z) && sameDouble(lhs.pch, rhs.pch)
        && sameDouble(lhs.yaw, rhs.yaw) && sameDouble(lhs.rol, rhs.rol)
        && sameSequence(lhs.values, rhs.values, sameTplValue)
        && lhs.layer == rhs.layer && sameBox(lhs.bbox, rhs.bbox);
}

bool sameThingIgnoringArchLighting(const Thing& lhs, const Thing& rhs) noexcept
{
    return lhs.id == rhs.id && lhs.num == rhs.num && lhs.sector == rhs.sector
        && lhs.templateName == rhs.templateName && lhs.name == rhs.name
        && sameDouble(lhs.x, rhs.x) && sameDouble(lhs.y, rhs.y)
        && sameDouble(lhs.z, rhs.z) && sameDouble(lhs.pch, rhs.pch)
        && sameDouble(lhs.yaw, rhs.yaw) && sameDouble(lhs.rol, rhs.rol)
        && sameSequence(lhs.values, rhs.values, sameTplValue)
        && lhs.layer == rhs.layer && sameBox(lhs.bbox, rhs.bbox);
}

bool hasMotsArchBakeSource(const Level& level) noexcept
{
    return level.kind == ProjectType::MOTS
        && std::any_of(level.lights.begin(), level.lights.end(),
            [](const SedLight& light) noexcept
            {
                return (light.flags & LF_AffectArchThings) != 0;
            });
}

// MotS 0x20 bake output is stored in the Things frame but belongs to a Lights
// transition. Only ignore ARCH changes tied to validated per-Thing bake
// provenance; unrelated references and unreferenced table edits remain errors.
bool sameMotsArchBakeHistoryOutput(const Level& lhs, const Level& rhs) noexcept
{
    if ( lhs.kind != ProjectType::MOTS || rhs.kind != ProjectType::MOTS )
    {
        return false;
    }

    try
    {
        using BaselineMap = std::unordered_map<
            std::int32_t, const MotsArchLightingBakeBaseline*>;
        const auto indexBaselines = [](
            const std::vector<MotsArchLightingBakeBaseline>& source,
            BaselineMap& result)
        {
            result.reserve(source.size());
            for ( const auto& baseline : source )
            {
                if ( !result.emplace(baseline.thingId, &baseline).second
                    || baseline.modelName.empty()
                    || baseline.originalArchLightingIndex < -1
                    || ( (baseline.originalArchLightingIndex == -1)
                        != !baseline.originalArchLighting.has_value() ) )
                {
                    return false;
                }
            }
            return true;
        };
        BaselineMap leftBase{};
        BaselineMap rightBase{};
        if ( !indexBaselines(lhs.motsArchLightingBakeBaselines, leftBase)
            || !indexBaselines(rhs.motsArchLightingBakeBaselines, rightBase) )
        {
            return false;
        }

        struct IndexedThing
        {
            const Thing* value{};
            bool duplicate{};
        };
        using ThingMap = std::unordered_map<std::int32_t, IndexedThing>;
        const auto indexThings = [](const Level& level, ThingMap& result)
        {
            result.reserve(level.things.size());
            for ( const Thing& thing : level.things )
            {
                auto [entry, inserted] = result.emplace(
                    thing.id, IndexedThing{ &thing, false });
                if ( !inserted )
                {
                    entry->second.duplicate = true;
                }
            }
        };
        ThingMap leftThings{};
        ThingMap rightThings{};
        indexThings(lhs, leftThings);
        indexThings(rhs, rightThings);
        const auto uniqueThing = [](
            const ThingMap& map,
            const std::int32_t id) -> const Thing*
        {
            const auto found = map.find(id);
            if ( found == map.end() || found->second.duplicate )
            {
                return nullptr;
            }
            return found->second.value;
        };
        const auto objectAt = [](const Level& level, const std::int32_t index)
            -> const MotsArchLightObject*
        {
            if ( index < 0
                || static_cast<std::size_t>(index)
                    >= level.motsArchLighting.size() )
            {
                return nullptr;
            }
            return &level.motsArchLighting[static_cast<std::size_t>(index)];
        };
        const auto canonicalModelName = [](const std::string_view value)
        {
            std::string name(sed::text::trimView(value));
            if ( name.empty() || sed::text::iequalsAscii(name, "none") )
            {
                return std::string{};
            }
            name = sed::text::lowerAscii(name);
            if ( name.size() < 4 || name.substr(name.size() - 4) != ".3do" )
            {
                name += ".3do";
            }
            return name;
        };
        const auto baselineModelMatches = [ &canonicalModelName ](
            const Level& level,
            const Thing& owner,
            const MotsArchLightingBakeBaseline& baseline )
        {
            const TplValue* model = sed::level::thingFindValue(
                level, owner, "model3d");
            if ( !model )
            {
                model = sed::level::thingFindValue(level, owner, "model3do");
            }
            const std::string expected = canonicalModelName(baseline.modelName);
            return !expected.empty()
                && expected == baseline.modelName
                && ( !model || canonicalModelName(model->asString()) == expected );
        };
        const auto unbakedRefMatches = [ &objectAt ](
            const MotsArchLightingBakeBaseline& baseline,
            const Level& unbaked,
            const Thing& owner )
        {
            if ( owner.archLightingIndex != baseline.originalArchLightingIndex )
            {
                return false;
            }
            const auto* original = objectAt(
                unbaked, baseline.originalArchLightingIndex);
            if ( baseline.originalArchLighting )
            {
                return original && sameArchLighting(
                    *original, *baseline.originalArchLighting);
            }
            return original == nullptr;
        };

        std::unordered_set<std::int32_t> owners{};
        owners.reserve(leftBase.size() + rightBase.size());
        for ( const auto& [id, baseline] : leftBase )
        {
            const auto found = rightBase.find(id);
            const auto* otherBaseline = found == rightBase.end()
                ? nullptr : found->second;
            const Thing* before = uniqueThing(leftThings, id);
            const Thing* after = uniqueThing(rightThings, id);
            if ( !before || !after
                || !sameThingIgnoringArchLighting(*before, *after)
                || !baselineModelMatches(lhs, *before, *baseline)
                || !baselineModelMatches(rhs, *after, *baseline) )
            {
                return false;
            }
            if ( otherBaseline )
            {
                if (!sameMotsArchLightingBakeBaseline(*baseline, *otherBaseline)
                    || !hasMotsArchBakeSource(lhs)
                    || !hasMotsArchBakeSource(rhs) )
                {
                    return false;
                }
            }
            else if ( !hasMotsArchBakeSource(lhs)
                || hasMotsArchBakeSource(rhs)
                || !unbakedRefMatches(*baseline, rhs, *after) )
            {
                return false;
            }
            owners.insert(id);
        }
        for ( const auto& [id, baseline] : rightBase )
        {
            if ( leftBase.contains(id) )
            {
                continue;
            }
            const Thing* before = uniqueThing(leftThings, id);
            const Thing* after = uniqueThing(rightThings, id);
            if ( !before || !after
                || !sameThingIgnoringArchLighting(*before, *after)
                || !baselineModelMatches(rhs, *after, *baseline)
                || !baselineModelMatches(lhs, *before, *baseline)
                || !hasMotsArchBakeSource(rhs)
                || hasMotsArchBakeSource(lhs)
                || !unbakedRefMatches(*baseline, lhs, *before) )
            {
                return false;
            }
            owners.insert(id);
        }

        if ( lhs.things.size() != rhs.things.size() )
        {
            return false;
        }
        std::unordered_set<std::int32_t> ownedRefs{};
        std::unordered_set<std::int32_t> otherRefs{};
        ownedRefs.reserve(owners.size() * 3u);
        otherRefs.reserve(lhs.things.size());
        const auto trackRef = [ &owners, &ownedRefs, &otherRefs ](
            const Thing& thing )
        {
            if ( thing.archLightingIndex < 0 )
            {
                return;
            }
            if ( owners.contains(thing.id) )
            {
                ownedRefs.insert(thing.archLightingIndex);
            }
            else
            {
                otherRefs.insert(thing.archLightingIndex);
            }
        };
        for ( std::size_t i = 0; i < lhs.things.size(); ++i )
        {
            const Thing& before = lhs.things[i];
            const Thing& after = rhs.things[i];
            if ( owners.contains(before.id)
                    ? !sameThingIgnoringArchLighting(before, after)
                    : !sameThing(before, after) )
            {
                return false;
            }
            trackRef(before);
            trackRef(after);
        }
        for ( const auto* baselines : {
                  &lhs.motsArchLightingBakeBaselines,
                  &rhs.motsArchLightingBakeBaselines } )
        {
            for ( const auto& baseline : *baselines )
            {
                if ( baseline.originalArchLightingIndex >= 0 )
                {
                    ownedRefs.insert(baseline.originalArchLightingIndex);
                }
            }
        }
        const std::size_t count = std::max(
            lhs.motsArchLighting.size(), rhs.motsArchLighting.size());
        for ( std::size_t i = 0; i < count; ++i )
        {
            const bool inLeft = i < lhs.motsArchLighting.size();
            const bool inRight = i < rhs.motsArchLighting.size();
            if ( inLeft && inRight
                && sameArchLighting(lhs.motsArchLighting[i], rhs.motsArchLighting[i]) )
            {
                continue;
            }
            if ( i > static_cast<std::size_t>(
                    std::numeric_limits<std::int32_t>::max()) )
            {
                return false;
            }
            const auto index = static_cast<std::int32_t>(i);
            if ( !ownedRefs.contains(index) || otherRefs.contains(index) )
            {
                return false;
            }
        }
        return true;
    }
    catch ( ... )
    {
        return false;
    }
}

bool sameCog(const Cog& lhs, const Cog& rhs) noexcept
{
    return lhs.id == rhs.id && lhs.num == rhs.num && lhs.name == rhs.name
        && sameSequence(lhs.values, rhs.values, sameCogValue);
}

bool sameLight(const SedLight& lhs, const SedLight& rhs) noexcept
{
    return lhs.id == rhs.id && lhs.flags == rhs.flags && lhs.mark == rhs.mark
        && lhs.layer == rhs.layer && sameColor(lhs.color, rhs.color)
        && sameDouble(lhs.intensity, rhs.intensity)
        && sameDouble(lhs.range, rhs.range)
        && sameVector(lhs.position, rhs.position);
}

bool sameHNode(const HNode& lhs, const HNode& rhs) noexcept
{
    return lhs.id == rhs.id && lhs.num == rhs.num && lhs.type == rhs.type
        && lhs.mesh == rhs.mesh && lhs.parent == rhs.parent
        && sameVector(lhs.position, rhs.position)
        && sameVector(lhs.rotation, rhs.rotation)
        && sameVector(lhs.pivot, rhs.pivot) && lhs.name == rhs.name;
}

bool sameHeader(const LevelHeader& lhs, const LevelHeader& rhs) noexcept
{
    return lhs.version == rhs.version && sameDouble(lhs.gravity, rhs.gravity)
        && sameDouble(lhs.horizonSky.distance, rhs.horizonSky.distance)
        && sameDouble(lhs.horizonSky.pixelsPerRev, rhs.horizonSky.pixelsPerRev)
        && samePoint2(lhs.horizonSky.offset, rhs.horizonSky.offset)
        && sameDouble(lhs.ceilingSky.height, rhs.ceilingSky.height)
        && samePoint2(lhs.ceilingSky.offset, rhs.ceilingSky.offset)
        && sameSequence(lhs.mipmapDistances, rhs.mipmapDistances,
            [](const double a, const double b) noexcept { return sameDouble(a, b); })
        && sameSequence(lhs.lodDistances, rhs.lodDistances,
            [](const double a, const double b) noexcept { return sameDouble(a, b); })
        && sameDouble(lhs.perspectiveDistance, rhs.perspectiveDistance)
        && sameDouble(lhs.gouraudDistance, rhs.gouraudDistance)
        && lhs.fog.enabled == rhs.fog.enabled && sameColor(lhs.fog.color, rhs.fog.color)
        && sameDouble(lhs.fog.fogStart, rhs.fog.fogStart)
        && sameDouble(lhs.fog.fogEnd, rhs.fog.fogEnd);
}

} // namespace

bool hasRepresentedJklCogImportState(const Level& level) noexcept
{
    return hasRepresentedJklCogImportStateInternal(level);
}

SedLevelDecodedFootprint estimateSedLevelDecodedFootprint(const Level& level)
{
    return estimateLevelDecodedFootprintInternal(level, false);
}

bool sedLevelsEqual(const Level& lhs, const Level& rhs) noexcept
{
    return sameHeader(lhs.header, rhs.header)
        && lhs.exportBufferExtras == rhs.exportBufferExtras
        && lhs.kind == rhs.kind
        && lhs.uvCoordinateSpace == rhs.uvCoordinateSpace
        && sameSequence(lhs.sectors, rhs.sectors, sameSector)
        && sameSequence(lhs.cogs, rhs.cogs, sameCog)
        && sameSequence(lhs.templates, rhs.templates, sameTemplate)
        && sameSequence(lhs.things, rhs.things, sameThing)
        && sameArchLightingSequence(lhs.motsArchLighting, rhs.motsArchLighting)
        && sameMotsArchLightingBakeBaselineSequence(
            lhs.motsArchLightingBakeBaselines,
            rhs.motsArchLightingBakeBaselines)
        && sameSequence(lhs.lights, rhs.lights, sameLight)
        && sameSequence(lhs.h3doNodes, rhs.h3doNodes, sameHNode)
        && lhs.colormaps == rhs.colormaps && lhs.materials == rhs.materials
        && lhs.layers == rhs.layers && lhs.masterCMP == rhs.masterCMP
        && sameDouble(lhs.ppunit, rhs.ppunit) && lhs.lvisstring == rhs.lvisstring
        && lhs.curId == rhs.curId
        && sameRepresentedJklCogImportState(lhs, rhs);
}

bool sedLevelNativeSerializationMatches(
    const Level& authored,
    const Level& decoded) noexcept
{
    if (authored.kind != ProjectType::MOTS
        && (!authored.motsArchLighting.empty()
            || !authored.motsArchLightingBakeBaselines.empty()
            || std::any_of(authored.things.begin(), authored.things.end(),
                [](const Thing& thing)
                {
                    return thing.archLightingIndex != -1;
                })))
    {
        return false;
    }
    if (sedLevelsEqual(authored, decoded))
    {
        return true;
    }

    // The native writer canonicalizes only MotS ARCH bindings. Keep the full
    // non-ARCH comparison explicit here so a canonical ARCH readback never
    // relaxes recovery's validation of any other authored field.
    if (!sameHeader(authored.header, decoded.header)
        || authored.exportBufferExtras != decoded.exportBufferExtras
        || authored.kind != decoded.kind
        || authored.uvCoordinateSpace != decoded.uvCoordinateSpace
        || !sameSequence(authored.sectors, decoded.sectors, sameSector)
        || !sameSequence(authored.cogs, decoded.cogs, sameCog)
        || !sameSequence(authored.templates, decoded.templates, sameTemplate)
        || authored.things.size() != decoded.things.size()
        || !std::equal(authored.things.begin(), authored.things.end(),
            decoded.things.begin(), sameThingIgnoringArchLighting)
        || !sameMotsArchLightingBakeBaselineSequence(
            authored.motsArchLightingBakeBaselines,
            decoded.motsArchLightingBakeBaselines)
        || !sameSequence(authored.lights, decoded.lights, sameLight)
        || !sameSequence(authored.h3doNodes, decoded.h3doNodes, sameHNode)
        || authored.colormaps != decoded.colormaps
        || authored.materials != decoded.materials
        || authored.layers != decoded.layers
        || authored.masterCMP != decoded.masterCMP
        || !sameDouble(authored.ppunit, decoded.ppunit)
        || authored.lvisstring != decoded.lvisstring
        || authored.curId != decoded.curId
        || !sameRepresentedJklCogImportState(authored, decoded))
    {
        return false;
    }

    try
    {
        const SedArchLightingSnapshot canonical =
            makeSedArchLightingSnapshot(authored);
        if (canonical.objects.size() != decoded.motsArchLighting.size()
            || canonical.thingIndices.size() != decoded.things.size())
        {
            return false;
        }
        for (std::size_t index = 0; index < canonical.objects.size(); ++index)
        {
            if (!sameArchLighting(
                    *canonical.objects[index], decoded.motsArchLighting[index]))
            {
                return false;
            }
        }
        for (std::size_t index = 0; index < canonical.thingIndices.size(); ++index)
        {
            if (canonical.thingIndices[index]
                != decoded.things[index].archLightingIndex)
            {
                return false;
            }
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool sedLevelsEqualIgnoringSectors(const Level& lhs, const Level& rhs) noexcept
{
    return sameHeader(lhs.header, rhs.header)
        && lhs.exportBufferExtras == rhs.exportBufferExtras
        && lhs.kind == rhs.kind
        && lhs.uvCoordinateSpace == rhs.uvCoordinateSpace
        && sameSequence(lhs.cogs, rhs.cogs, sameCog)
        && sameSequence(lhs.templates, rhs.templates, sameTemplate)
        && sameSequence(lhs.things, rhs.things, sameThing)
        && sameArchLightingSequence(lhs.motsArchLighting, rhs.motsArchLighting)
        && sameMotsArchLightingBakeBaselineSequence(
            lhs.motsArchLightingBakeBaselines,
            rhs.motsArchLightingBakeBaselines)
        && sameSequence(lhs.lights, rhs.lights, sameLight)
        && sameSequence(lhs.h3doNodes, rhs.h3doNodes, sameHNode)
        && lhs.colormaps == rhs.colormaps && lhs.materials == rhs.materials
        && lhs.layers == rhs.layers && lhs.masterCMP == rhs.masterCMP
        && sameDouble(lhs.ppunit, rhs.ppunit) && lhs.lvisstring == rhs.lvisstring
        && sameRepresentedJklCogImportState(lhs, rhs);
}

bool sedLevelsEqualIgnoringHistoryDomains(
    const Level& lhs,
    const Level& rhs,
    const bool ignoreThings,
    const bool ignoreLightsAndBakedLighting,
    const bool ignoreCogs) noexcept
{
    const bool allowMotsArchBakeOutput = !ignoreThings
        && ignoreLightsAndBakedLighting
        && lhs.kind == ProjectType::MOTS && rhs.kind == ProjectType::MOTS
        && (hasMotsArchBakeSource(lhs) || hasMotsArchBakeSource(rhs));
    return sameHeader(lhs.header, rhs.header)
        && lhs.exportBufferExtras == rhs.exportBufferExtras
        && lhs.kind == rhs.kind
        && lhs.uvCoordinateSpace == rhs.uvCoordinateSpace
        && lhs.sectors.size() == rhs.sectors.size()
        && std::equal(lhs.sectors.begin(), lhs.sectors.end(), rhs.sectors.begin(),
            [ignoreLightsAndBakedLighting](const Sector& a, const Sector& b) noexcept
            {
                return sameSectorIgnoringBakedLighting(
                    a, b, ignoreLightsAndBakedLighting);
            })
        && (ignoreCogs || sameSequence(lhs.cogs, rhs.cogs, sameCog))
        && sameSequence(lhs.templates, rhs.templates, sameTemplate)
        && (ignoreThings
            || (allowMotsArchBakeOutput
                ? sameMotsArchBakeHistoryOutput(lhs, rhs)
                : (sameSequence(lhs.things, rhs.things, sameThing)
                && sameArchLightingSequence(
                    lhs.motsArchLighting, rhs.motsArchLighting)
                    && sameMotsArchLightingBakeBaselineSequence(
                        lhs.motsArchLightingBakeBaselines,
                        rhs.motsArchLightingBakeBaselines))))
        && (ignoreLightsAndBakedLighting
            || sameSequence(lhs.lights, rhs.lights, sameLight))
        && sameSequence(lhs.h3doNodes, rhs.h3doNodes, sameHNode)
        && lhs.colormaps == rhs.colormaps && lhs.materials == rhs.materials
        && lhs.layers == rhs.layers && lhs.masterCMP == rhs.masterCMP
        && sameDouble(lhs.ppunit, rhs.ppunit)
        && lhs.lvisstring == rhs.lvisstring
        && (ignoreCogs || sameRepresentedJklCogImportState(lhs, rhs));
}

namespace detail
{

// Link-only unit-test seam. It is intentionally absent from the public
// header: production recovery and these boundary tests must exercise the same
// checked arithmetic without allocating a maximum-sized fixture.
std::optional<std::size_t> recoveryFramingSizeForTesting(
    const std::size_t sectionBytes, const bool survivingMirror) noexcept
{
    return checkedRecoveryFramingSize(sectionBytes, survivingMirror);
}

bool sedFileSizeWithinLimitForTesting(const std::uint64_t fileSize) noexcept
{
    return isAdmittedSedFileSize(fileSize);
}

// Link-only property-test/fuzzer seams. Keeping both calls on the production
// implementation lets sanitizers exercise matcher ring/window arithmetic
// without first forging a complete checksummed SED container.
std::vector<std::byte> compressSedLzssForTesting(
    const std::span<const std::byte> input)
{
    return compressFast(input);
}

std::vector<std::byte> decompressSedLzssForTesting(
    const std::span<const std::byte> input,
    const std::size_t expandedSize)
{
    return decompressFast(input, expandedSize);
}

} // namespace detail

std::uint32_t calculateSedCrc32c(const std::span<const std::byte> bytes,
    const bool allowHardwareAcceleration) noexcept
{
#if SED_HAS_X86_CRC32C
    if (allowHardwareAcceleration && hasHardwareCrc32cImpl())
    {
        return crc32cHardware(bytes);
    }
#else
    (void)allowHardwareAcceleration;
#endif
    return crc32cPortable(bytes);
}

std::array<std::byte, 32> calculateSedSha256(
    const std::span<const std::byte> bytes) noexcept
{
    return sha256(bytes);
}

bool sedCrc32cHardwareAccelerationAvailable() noexcept
{
    return hasHardwareCrc32cImpl();
}

bool hasSedBinarySignature(const std::span<const std::byte> bytes) noexcept
{
    return sedMagicSize(bytes).has_value();
}

namespace
{

constexpr std::array<std::array<char, 4>, kRequiredSectionCount>
    kRequiredSectionTypes{{
        { 'h', 'e', 'a', 'd' },
        { 'r', 's', 'r', 'c' },
        { 's', 'e', 'c', 't' },
        { 't', 'h', 'n', 'g' },
        { 't', 'm', 'p', 'l' },
        { 'c', 'o', 'g', 's' },
        { 'l', 'i', 't', 'e' },
        { 'h', 'n', 'o', 'd' },
    }};

constexpr std::array<SedRequiredSection, kRequiredSectionCount>
    kRequiredSectionBits{{
        SedRequiredSection::Head,
        SedRequiredSection::Resources,
        SedRequiredSection::Sectors,
        SedRequiredSection::Things,
        SedRequiredSection::Templates,
        SedRequiredSection::Cogs,
        SedRequiredSection::Lights,
        SedRequiredSection::Hierarchy,
    }};

constexpr SedRequiredSection kResourceIndexedSections =
    SedRequiredSection::Head
    | SedRequiredSection::Sectors
    | SedRequiredSection::Things
    | SedRequiredSection::Templates
    | SedRequiredSection::Cogs
    | SedRequiredSection::Hierarchy;

Bytes encodeRequiredSectionRaw(const std::size_t index, const Level& level,
    const ResourceIndex& resources)
{
    switch (index)
    {
    case 0: return encodeHead(level, resources);
    case 1: return encodeResources(level, resources);
    case 2: return encodeSectors(level, resources);
    case 3: return encodeThings(level, resources,
        !level.motsArchLighting.empty());
    case 4: return encodeTemplates(level, resources);
    case 5: return encodeCogs(level, resources);
    case 6: return encodeLights(level);
    case 7: return encodeHierarchy(level, resources);
    default:
        throw std::logic_error(
            "Binary SED required-section index is invalid.");
    }
}

std::shared_ptr<const Bytes> frameGeneratedUnprotectedSection(
    const std::size_t index, Bytes raw,
    const std::uint32_t binaryVersion)
{
    Writer section{ kMaxFileBytes - kHeaderMirrorSize - kFooterSize };
    // HistoryNested has no FEC alignment and its checked descriptor does
    // not contain a physical file offset. The resulting frame is therefore
    // canonical at every required-section position and safe to share between
    // adjacent generated history documents.
    appendSection(section, kRequiredSectionTypes[index], raw, 0u, false, 0u,
        true, binaryVersion);
    return std::make_shared<const Bytes>(section.take());
}

Bytes canonicalRequiredHeader(const Level& level)
{
    Writer out{};
    out.raw(kMagic);
    out.u32(binaryVersionForLevel(level));
    out.i32(static_cast<std::int32_t>(level.kind));
    out.u32(kRequiredSectionCount);
    out.i64(0);
    out.u32(0);
    Bytes header = out.take();
    replaceU32(header, kHeaderChecksumOffset,
        crc32c(std::span<const std::byte>(header).first(kHeaderChecksumOffset)));
    return header;
}

} // namespace

struct detail::HistoryNestedGenerationAccess final
{
    static SedGeneratedUnprotectedDocument
    generate(const Level& level, SedRequiredSection requestedSections,
             const SedGeneratedUnprotectedRequiredSections* const reusable,
             const bool materializeBytes, const bool trustedTransitionRequested,
             const std::span<const std::int32_t> changedSectorIndices)
    {
        const auto requestedValue = static_cast<std::uint16_t>(requestedSections);
        const auto validValue = static_cast<std::uint16_t>(SedRequiredSection::All);
        if ((requestedValue & ~validValue) != 0u)
        {
            throw std::invalid_argument(
                "Binary SED required-section invalidation mask is invalid.");
        }
        if (level.kind != ProjectType::JKDF2 && level.kind != ProjectType::MOTS &&
            level.kind != ProjectType::IJIM)
        {
            throw std::runtime_error("Cannot save binary SED: invalid project type.");
        }

        const std::uint32_t binaryVersion = binaryVersionForLevel(level);

        const bool canReuse =
            reusable != nullptr && reusable->configured
            && reusable->projectType == level.kind
            && reusable->binaryVersion == binaryVersion &&
            std::all_of(reusable->frames.begin(), reusable->frames.end(),
                        [](const auto& frame) { return static_cast<bool>(frame); });

        // A public call cannot assert that an incomplete mask describes the
        // complete current Level. Force every section through the canonical
        // writer; only the token-bearing internal path below may honor an exact
        // Snapshot-derived subset mask.
        if (canReuse && !trustedTransitionRequested)
        {
            requestedSections = SedRequiredSection::All;
        }

        // The normal public path retains the complete ordinary writer preflight.
        // The trusted path is used only by the in-process history walker, whose
        // unforgeable token came from an exact compact Snapshot variant and is
        // bound to this Level object. It still estimates the complete object
        // footprint and validates each changed
        // domain, but can reuse the prior generated resource index when the exact
        // ordered resource-input sequence is unchanged.  No persisted dirty flag
        // or input-file metadata reaches this branch.
        // The reduced validation path is capability-confined to a cache produced
        // by this writer. A caller cannot manufacture a frame-only cache or ask
        // for incomplete-domain reuse with a merely copied byte array.
        const bool trustedTransition = trustedTransitionRequested && canReuse &&
                                       reusable != nullptr && reusable->writerProvenance;
        const SedLevelDecodedFootprint levelFootprint =
            trustedTransition ? estimateLevelDecodedFootprintInternal(level, true)
                              : validateLevelBeforeResourceIndex(level);

        std::shared_ptr<const UnprotectedGenerationProvenance> provenance{};
        const auto priorProvenance =
            trustedTransition && reusable != nullptr && reusable->writerProvenance
                ? std::static_pointer_cast<const UnprotectedGenerationProvenance>(
                      reusable->writerProvenance)
                : std::shared_ptr<const UnprotectedGenerationProvenance>{};
        const bool resourceInputsReuse =
            priorProvenance && sameResourceInputs(level, priorProvenance->resourceInputs);
        if (resourceInputsReuse)
        {
            provenance = priorProvenance;
        }
        else
        {
            auto generatedProvenance = std::make_shared<UnprotectedGenerationProvenance>();
            generatedProvenance->resourceIndex = buildResourceIndex(level);
            generatedProvenance->resourceInputs = makeResourceInputs(level);
            provenance = std::move(generatedProvenance);
        }
        const ResourceIndex& resourceIndex = provenance->resourceIndex;
        validateResourceIndexForSerialization(levelFootprint, resourceIndex);

        if (trustedTransition)
        {
            std::unordered_map<std::string, bool> changedSections{};
            for (std::size_t index = 0; index < kRequiredSectionCount; ++index)
            {
                changedSections.emplace(std::string(kRequiredSectionTypes[index].data(),
                                                    kRequiredSectionTypes[index].size()),
                                        hasAny(requestedSections, kRequiredSectionBits[index]));
            }
            // The trusted caller has already established that every omitted
            // domain is byte-identical to the prior generated Level. Validate only
            // the domain(s) it actually changed; the reused checked frames remain
            // the canonical proof for all other domains.
            validateDecodedLevel(level, changedSections, changedSectorIndices);
        }

        SedGeneratedUnprotectedDocument result{};
        if (canReuse)
        {
            result.requiredSections = *reusable;
        }
        else
        {
            requestedSections = SedRequiredSection::All;
            result.requiredSections.projectType = level.kind;
            result.requiredSections.binaryVersion = binaryVersion;
            result.requiredSections.configured = true;
        }
        result.requiredSections.writerProvenance = provenance;

        // The catalog is both the string-deduplication source of truth and the
        // namespace for category-local indices in six other sections. It is
        // intentionally regenerated exactly for every adjacent state.
        // An exact resource-input match also proves the canonical first-use
        // catalog is unchanged, so keep the previous checked frame directly and
        // avoid serializing/comparing the complete catalog for every Thing-only
        // history state.  The fallback still emits and compares it byte-for-byte.
        const std::shared_ptr<const Bytes> resourceFrame =
            resourceInputsReuse ? result.requiredSections.frames[1]
                                : frameGeneratedUnprotectedSection(
                                      1u, encodeRequiredSectionRaw(1u, level, resourceIndex),
                                      binaryVersion);
        result.regeneratedSections |= SedRequiredSection::Resources;
        if (!canReuse || *resourceFrame != *reusable->frames[1])
        {
            result.resourceCatalogChanged = canReuse;
            result.requiredSections.frames[1] = resourceFrame;
            requestedSections |= kResourceIndexedSections;
        }

        for (std::size_t index = 0; index < kRequiredSectionCount; ++index)
        {
            if (index == 1u)
                continue;
            if (!hasAny(requestedSections, kRequiredSectionBits[index]))
                continue;
            std::shared_ptr<const Bytes> generatedFrame{};
            if (index == 2u && trustedTransition && resourceInputsReuse &&
                !changedSectorIndices.empty())
            {
                const std::span<const std::byte> previousPayload =
                    historyNestedFramePayload(*reusable->frames[index]);
                generatedFrame = frameGeneratedUnprotectedSection(
                    index, encodeSectorsWithReusedPayload(level, resourceIndex, previousPayload,
                                                          changedSectorIndices),
                    binaryVersion);
            }
            else
            {
                generatedFrame = frameGeneratedUnprotectedSection(
                    index, encodeRequiredSectionRaw(index, level, resourceIndex),
                    binaryVersion);
            }
            // A compact Snapshot may conservatively name a semantic domain even
            // when the concrete edit was a no-op for this exact native frame.
            // Preserve the old generated owner after an exact byte comparison so
            // the enclosing HIST writer can also reuse its fragment references;
            // owner identity is never accepted without this byte proof.
            if (!canReuse || *generatedFrame != *reusable->frames[index])
            {
                result.requiredSections.frames[index] = generatedFrame;
            }
            result.regeneratedSections |= kRequiredSectionBits[index];
        }

        const Bytes header = canonicalRequiredHeader(level);
        result.canonicalHeader = header;
        std::uint64_t expandedPayloadBytes{};
        std::size_t bodyBytes = header.size();
        for (std::size_t index = 0; index < kRequiredSectionCount; ++index)
        {
            const std::shared_ptr<const Bytes>& frame = result.requiredSections.frames[index];
            if (!frame || frame->size() < kSectionHeaderSize + kSectionTrailerSize)
            {
                throw std::logic_error(
                    "Generated binary SED required-section cache is incomplete.");
            }
            const auto descriptor = parseSectionDescriptor(
                std::span<const std::byte>(*frame).first(kSectionHeaderSize));
            if (!descriptor || descriptor->version != binaryVersion
                || descriptor->type != kRequiredSectionTypes[index] ||
                descriptor->flags != kHistoryNestedChecksum ||
                descriptor->rawSize != descriptor->storedSize ||
                descriptor->storedSize != frame->size() - kSectionHeaderSize - kSectionTrailerSize)
            {
                throw std::logic_error("Generated binary SED required-section frame is invalid.");
            }
            if (descriptor->rawSize > SedMaximumExpandedPayloadBytes - expandedPayloadBytes)
            {
                throw std::runtime_error(
                    "Binary SED expanded payload exceeds the safe aggregate limit.");
            }
            expandedPayloadBytes += descriptor->rawSize;
            if (frame->size() > kMaxFileBytes - kHeaderMirrorSize - kFooterSize - bodyBytes)
            {
                throw std::runtime_error("Binary SED output exceeds the safe file limit.");
            }
            bodyBytes += frame->size();
        }

        result.byteSize = bodyBytes + kHeaderMirrorSize + kFooterSize;
        if (result.byteSize > kMaxFileBytes)
        {
            throw std::runtime_error("Binary SED output exceeds the safe file limit.");
        }
        Writer mirror{};
        mirror.raw(kHeaderMirrorMagic);
        mirror.u32(kCanonicalHeaderSize);
        mirror.raw(header);
        mirror.u32(crc32c(header));
        result.canonicalTrailingFraming = mirror.take();
        result.canonicalTrailingFraming.insert(
            result.canonicalTrailingFraming.end(),
            kFooterMagic.begin(), kFooterMagic.end());

        if (materializeBytes)
        {
            Writer fileWriter{kMaxFileBytes - kHeaderMirrorSize - kFooterSize};
            fileWriter.reserveAtLeast(bodyBytes);
            fileWriter.raw(header);
            for (const auto& frame : result.requiredSections.frames)
            {
                fileWriter.raw(*frame);
            }
            Bytes file = fileWriter.take();
            file.insert(file.end(), result.canonicalTrailingFraming.begin(),
                result.canonicalTrailingFraming.end());
            result.bytes = std::move(file);
        }
        return result;
    }
};

SedGeneratedUnprotectedDocument generateUnprotectedSedDocument(const Level& level,
                                                               const bool materializeBytes)
{
    return detail::HistoryNestedGenerationAccess::generate(level, SedRequiredSection::All, nullptr,
                                                           materializeBytes, false, {});
}

SedGeneratedUnprotectedDocument detail::generateHistoryNestedTransitionDocument(
    const Level& level, const SedHistoryTransitionToken& transition,
    const SedGeneratedUnprotectedRequiredSections* const reusable, const bool materializeBytes,
    const SedRequiredSection additionalSections)
{
    const auto additionalValue = static_cast<std::uint16_t>(
        additionalSections);
    const auto validValue = static_cast<std::uint16_t>(
        SedRequiredSection::All);
    if (!transition.matches(level))
    {
        throw std::invalid_argument(
            "History-nested transition token does not match its visited Level.");
    }
    if ((additionalValue & ~validValue) != 0u)
    {
        throw std::invalid_argument(
            "History-nested additional-section mask is invalid.");
    }
    if (transition.sectorSubset()
        && !hasAny(transition.requiredSections(), SedRequiredSection::Sectors))
    {
        throw std::logic_error(
            "History-nested sector-subset token omits the Sectors domain.");
    }
    return HistoryNestedGenerationAccess::generate(
        level,
        transition.requiredSections() | additionalSections,
        reusable,
        materializeBytes,
        true,
        transition.sectorSubset()
            ? transition.sectorIndices()
            : std::span<const std::int32_t>{});
}

std::vector<std::byte> saveToSedBytes(const Level& level)
{
    return saveToSedBytes(level, {});
}

std::vector<std::byte> saveToSedBytes(
    const Level& level,
    const std::span<const SedOptionalSection> optionalSections)
{
    return saveToSedBytes(level, optionalSections, SedSerializationProfile::ProtectedV1);
}

std::vector<std::byte> saveToSedBytes(
    const Level& level,
    const std::span<const SedOptionalSection> optionalSections,
    const SedSerializationProfile profile)
{
    return saveToSedBytes(level, optionalSections, profile, {});
}

std::vector<std::byte> saveToSedBytes(
    const Level& level,
    const std::span<const SedOptionalSection> optionalSections,
    const SedSerializationProfile profile,
    const std::int64_t timestamp)
{
    requireValidSerializationProfile(profile);
    if (level.hasAmbiguousPendingJklCogSurfaceReferences())
    {
        throw std::runtime_error(
            "Cannot save SED: imported COG surface references require "
            "resource-backed COG script '" + pendingJklCogName(level)
            + "' before serialization.");
    }
    if (level.kind != ProjectType::JKDF2 && level.kind != ProjectType::MOTS && level.kind != ProjectType::IJIM)
    {
        throw std::runtime_error("Cannot save binary SED: invalid project type.");
    }
    if (timestamp < 0)
    {
        throw std::runtime_error(
            "Cannot save binary SED: timestamp must be non-negative.");
    }
    // Nested history/content encodings must stay independent of ambient wall
    // clock time so fragment reuse and semantic digests remain stable.
    const std::int64_t headerTimestamp =
        profile == SedSerializationProfile::ProtectedV1
        ? timestamp : 0;
    const SedLevelDecodedFootprint levelFootprint =
        validateLevelBeforeResourceIndex(level);
    const std::uint32_t binaryVersion = binaryVersionForLevel(level);
    const ResourceIndex resourceIndex = buildResourceIndex(level);
    validateResourceIndexForSerialization(levelFootprint, resourceIndex);

    validateOptionalSections(optionalSections);
    if (profile == SedSerializationProfile::HistoryNested
        && !optionalSections.empty())
    {
        throw std::runtime_error(
            "History-nested SED accepts required Level sections only.");
    }
    const bool emitJklCogImport = profile == SedSerializationProfile::ProtectedV1
        && hasRepresentedJklCogImportStateInternal(level);
    const std::size_t sectionCount = kRequiredSectionCount
        + optionalSections.size() + static_cast<std::size_t>(emitJklCogImport);
    if (sectionCount > 1024u)
    {
        throw std::runtime_error("Too many binary SED sections.");
    }

    Writer out{};
    out.raw(kMagic);
    out.u32(binaryVersion);
    out.i32(static_cast<std::int32_t>(level.kind));
    out.u32(static_cast<std::uint32_t>(sectionCount));
    out.i64(headerTimestamp);
    out.u32(0); // checksum, filled after the fixed header is complete
    Bytes header = out.take();
    replaceU32(header, kHeaderChecksumOffset,
        crc32c(std::span<const std::byte>(header).first(kHeaderChecksumOffset)));
    Writer fileWriter{ kMaxFileBytes - kHeaderMirrorSize - kFooterSize };
    fileWriter.raw(header);

    const bool protectParity = profile == SedSerializationProfile::ProtectedV1;
    const bool compressPayload = profile == SedSerializationProfile::ProtectedV1;
    const bool historyNested =
        profile == SedSerializationProfile::HistoryNested;
    std::uint64_t expandedPayloadBytes{};
    const auto accountExpandedPayload = [&](const std::size_t rawSize)
    {
        if (rawSize > SedMaximumExpandedPayloadBytes - expandedPayloadBytes)
        {
            throw std::runtime_error(
                "Binary SED expanded payload exceeds the safe aggregate limit.");
        }
        expandedPayloadBytes += rawSize;
    };
    const auto appendEncoded = [&](const std::array<char, 4>& type, Bytes raw)
    {
        accountExpandedPayload(raw.size());
        const std::uint32_t parityStreams = !protectParity ? 0u
            : isCoreProtectionSection(typeName(type))
                ? kFecRequiredParityStreams : kFecOptionalParityStreams;
        appendSection(fileWriter, type, raw, parityStreams, compressPayload,
            fileWriter.bytes().size(), historyNested, binaryVersion);
    };
    appendEncoded({ 'h', 'e', 'a', 'd' }, encodeHead(level, resourceIndex));
    appendEncoded({ 'r', 's', 'r', 'c' }, encodeResources(level, resourceIndex));
    appendEncoded({ 's', 'e', 'c', 't' }, encodeSectors(level, resourceIndex));
    appendEncoded({ 't', 'h', 'n', 'g' }, encodeThings(level, resourceIndex,
        !level.motsArchLighting.empty()));
    appendEncoded({ 't', 'm', 'p', 'l' }, encodeTemplates(level, resourceIndex));
    appendEncoded({ 'c', 'o', 'g', 's' }, encodeCogs(level, resourceIndex));
    appendEncoded({ 'l', 'i', 't', 'e' }, encodeLights(level));
    appendEncoded({ 'h', 'n', 'o', 'd' }, encodeHierarchy(level, resourceIndex));
    if (emitJklCogImport)
    {
        appendEncoded(kJklCogImportSectionType,
            encodeJklCogImportState(level));
    }
    for (const SedOptionalSection& optional : optionalSections)
    {
        accountExpandedPayload(optional.payload.size());
        const std::uint32_t parityStreams = !protectParity ? 0u
            : isCoreProtectionSection(typeName(optional.type))
                ? kFecRequiredParityStreams : kFecOptionalParityStreams;
        appendSection(fileWriter, optional.type, optional.payload, parityStreams,
            compressPayload, fileWriter.bytes().size(), historyNested,
            binaryVersion);
    }
    Bytes file = fileWriter.take();

    // Reserve the fixed redundant framing before constructing its buffers.
    // This keeps the final mirror/footer bound part of the allocation
    // preflight rather than discovering an over-limit document after the
    // complete semantic stream has already been materialized.
    if (file.size() > kMaxFileBytes - kHeaderMirrorSize - kFooterSize)
    {
        throw std::runtime_error("Binary SED output exceeds the safe file limit.");
    }

    // Keep a checked copy of the fixed header at the end of the stream.  If
    // the primary magic/version/project/count is damaged, the loader can
    // restore canonical framing before it scans semantic sections.
    Writer mirror{};
    mirror.raw(kHeaderMirrorMagic);
    mirror.u32(kCanonicalHeaderSize);
    mirror.raw(std::span<const std::byte>(file).first(kCanonicalHeaderSize));
    mirror.u32(crc32c(std::span<const std::byte>(file).first(kCanonicalHeaderSize)));
    const Bytes mirrorBytes = mirror.take();
    file.insert(file.end(), mirrorBytes.begin(), mirrorBytes.end());
    Writer footer{};
    footer.raw(kFooterMagic);
    const Bytes footerBytes = footer.take();
    file.insert(file.end(), footerBytes.begin(), footerBytes.end());
    if (file.size() > kMaxFileBytes) throw std::runtime_error("Binary SED output exceeds the safe file limit.");
    return file;
}

using TrustedNestedRequiredFrames = std::array<
    std::span<const std::byte>, kRequiredSectionCount>;

TrustedNestedRequiredFrames trustedNestedRequiredFrames(
    const std::span<const std::byte> bytes)
{
    // This helper is reached only with bytes owned by SedNestedHistoryDecoder:
    // the initial state was generated by sed_core and every later state passed
    // the complete strict decoder below. Keep the arithmetic checked anyway so
    // an internal invariant failure cannot turn into an out-of-bounds reuse.
    if (bytes.size() < kCanonicalHeaderSize + kHeaderMirrorSize + kFooterSize
        || !sedMagicSize(bytes)
        || readU32At(bytes, kHeaderSectionCountOffset) != kRequiredSectionCount)
    {
        throw std::logic_error(
            "Incremental nested SED trust context has invalid framing.");
    }
    const std::uint32_t binaryVersion = readU32At(
        bytes, kHeaderVersionOffset);
    if (!isSupportedSedBinaryVersion(binaryVersion))
    {
        throw std::logic_error(
            "Incremental nested SED trust context has an unsupported version.");
    }
    const std::size_t bodyEnd = bytes.size() - kHeaderMirrorSize - kFooterSize;
    std::size_t cursor = kCanonicalHeaderSize;
    TrustedNestedRequiredFrames frames{};
    for (std::size_t index = 0; index < frames.size(); ++index)
    {
        if (cursor > bodyEnd || kSectionHeaderSize > bodyEnd - cursor)
        {
            throw std::logic_error(
                "Incremental nested SED trust context is truncated.");
        }
        const auto descriptor = parseSectionDescriptor(
            bytes.subspan(cursor, kSectionHeaderSize));
        if (!descriptor || descriptor->version != binaryVersion
            || descriptor->type != kRequiredSectionTypes[index]
            || descriptor->flags != kHistoryNestedChecksum
            || descriptor->rawSize != descriptor->storedSize
            || descriptor->storedSize > bodyEnd - cursor - kSectionHeaderSize
            || kSectionTrailerSize > bodyEnd - cursor - kSectionHeaderSize
                - static_cast<std::size_t>(descriptor->storedSize))
        {
            throw std::logic_error(
                "Incremental nested SED trust context has an invalid required section.");
        }
        const std::size_t end = cursor + kSectionHeaderSize
            + static_cast<std::size_t>(descriptor->storedSize)
            + kSectionTrailerSize;
        frames[index] = bytes.subspan(cursor, end - cursor);
        cursor = end;
    }
    if (cursor != bodyEnd)
    {
        throw std::logic_error(
            "Incremental nested SED trust context has unexpected body bytes.");
    }
    return frames;
}

void swapHeadDomain(Level& lhs, Level& rhs) noexcept
{
    using std::swap;
    swap(lhs.header, rhs.header);
    swap(lhs.exportBufferExtras, rhs.exportBufferExtras);
    swap(lhs.kind, rhs.kind);
    swap(lhs.masterCMP, rhs.masterCMP);
    swap(lhs.ppunit, rhs.ppunit);
    swap(lhs.lvisstring, rhs.lvisstring);
    swap(lhs.curId, rhs.curId);
}

void swapRequiredLevelDomain(
    Level& lhs, Level& rhs, const SedRequiredSection section) noexcept
{
    using std::swap;
    switch (section)
    {
    case SedRequiredSection::Head: swapHeadDomain(lhs, rhs); break;
    case SedRequiredSection::Resources:
        swap(lhs.colormaps, rhs.colormaps);
        swap(lhs.materials, rhs.materials);
        swap(lhs.layers, rhs.layers);
        break;
    case SedRequiredSection::Sectors: swap(lhs.sectors, rhs.sectors); break;
    case SedRequiredSection::Things:
        swap(lhs.things, rhs.things);
        swap(lhs.motsArchLighting, rhs.motsArchLighting);
        swap(lhs.motsArchLightingBakeBaselines,
            rhs.motsArchLightingBakeBaselines);
        break;
    case SedRequiredSection::Templates: swap(lhs.templates, rhs.templates); break;
    case SedRequiredSection::Cogs: swap(lhs.cogs, rhs.cogs); break;
    case SedRequiredSection::Lights: swap(lhs.lights, rhs.lights); break;
    case SedRequiredSection::Hierarchy: swap(lhs.h3doNodes, rhs.h3doNodes); break;
    default: break;
    }
}

/** Moves an admitted Level into the next candidate without copying its large
 * vectors. Before a semantic section is replaced, its preceding domain is
 * swapped into previousDomains_. If any later frame, graph, or catalog check
 * fails, the destructor reconstructs source_ exactly. The caller commits only
 * after every strict check has succeeded. */
class IncrementalLevelTransaction final
{
public:
    IncrementalLevelTransaction(
        Level& source, Level& candidate, Level& previousDomains)
        : source_(&source), candidate_(&candidate), previousDomains_(&previousDomains)
    {
        previousDomains = {};
        candidate = std::move(source);
    }

    IncrementalLevelTransaction(const IncrementalLevelTransaction&) = delete;
    IncrementalLevelTransaction& operator=(const IncrementalLevelTransaction&) = delete;

    ~IncrementalLevelTransaction()
    {
        if (!committed_)
        {
            if (preparedCogImportState_)
            {
                using std::swap;
                swap(candidate_->importedJklCogRowsPresent,
                    previousDomains_->importedJklCogRowsPresent);
                candidate_->importedJklSurfaceRefs.swap(
                    previousDomains_->importedJklSurfaceRefs);
            }
            *source_ = std::move(*candidate_);
            restorePreparedDomains(*source_);
        }
    }

    void prepare(const SedRequiredSection section) noexcept
    {
        if (section == SedRequiredSection::None
            || hasAny(prepared_, section))
        {
            return;
        }
        swapRequiredLevelDomain(*candidate_, *previousDomains_, section);
        prepared_ |= section;
    }

    void replaceCogImportState(SedJklCogImportState&& state) noexcept
    {
        if (candidate_->importedJklCogRowsPresent == state.cogRowsPresent
            && candidate_->importedJklSurfaceRefs == state.surfaceRefs)
        {
            return;
        }
        using std::swap;
        swap(candidate_->importedJklCogRowsPresent,
            previousDomains_->importedJklCogRowsPresent);
        candidate_->importedJklSurfaceRefs.swap(
            previousDomains_->importedJklSurfaceRefs);
        candidate_->importedJklCogRowsPresent = state.cogRowsPresent;
        candidate_->importedJklSurfaceRefs = std::move(state.surfaceRefs);
        preparedCogImportState_ = true;
    }

    [[nodiscard]] bool cogImportStateChanged() const noexcept
    {
        return preparedCogImportState_;
    }

    [[nodiscard]] SedRequiredSection prepared() const noexcept
    {
        return prepared_;
    }

    void commit() noexcept { committed_ = true; }

private:
    void restorePreparedDomains(Level& destination) noexcept
    {
        constexpr std::array<SedRequiredSection, 8> sections{
            SedRequiredSection::Head,
            SedRequiredSection::Resources,
            SedRequiredSection::Sectors,
            SedRequiredSection::Things,
            SedRequiredSection::Templates,
            SedRequiredSection::Cogs,
            SedRequiredSection::Lights,
            SedRequiredSection::Hierarchy,
        };
        for (const SedRequiredSection section : sections)
        {
            if (hasAny(prepared_, section))
            {
                swapRequiredLevelDomain(destination, *previousDomains_, section);
            }
        }
    }

    Level* source_{};
    Level* candidate_{};
    Level* previousDomains_{};
    SedRequiredSection prepared_{ SedRequiredSection::None };
    bool preparedCogImportState_{};
    bool committed_{};
};

SedRecoveryLoadResult decodeSedBytes(
    const std::span<const std::byte> bytes,
    const bool allowPartial,
    const SedSerializationProfile profile,
    const bool synthesizedHeaderFraming = false,
    RecoveryWorkBudget* const recoveryBudget = nullptr,
    SemanticDecodeBudget* const sharedSemanticBudget = nullptr,
    Level* const reusableLevel = nullptr,
    const std::span<const std::byte> reusableBytes = {},
    SedNestedHistoryDecodeStats* const incrementalStats = nullptr,
    Level* const previousDomains = nullptr,
    SedRequiredSection* const previousDomainsMask = nullptr,
    SedJklCogImportState* const nestedCogImportState = nullptr,
    bool* const previousCogImportStateChanged = nullptr)
{
    requireValidSerializationProfile(profile);
    const bool incremental = reusableLevel != nullptr;
    if (incremental
        && (allowPartial || profile != SedSerializationProfile::HistoryNested
            || reusableBytes.empty() || !previousDomains
            || !previousDomainsMask))
    {
        throw std::invalid_argument(
            "Incremental SED decoding requires a strict nested trust context.");
    }
    if (nestedCogImportState && !incremental)
    {
        throw std::invalid_argument(
            "HIST COG import metadata requires an incremental nested decoder.");
    }
    if (previousCogImportStateChanged)
    {
        *previousCogImportStateChanged = false;
    }
    // A V2/V3 or V1/V4 transition keeps the same UV units but changes the
    // Things-section schema. Decode the new state transactionally while
    // disabling frame reuse for that transition; reusing a frame with a
    // different section version would make the old payload look canonical.
    std::optional<TrustedNestedRequiredFrames> reusableFrames{};
    if (incrementalStats) *incrementalStats = {};
    if (recoveryBudget) recoveryBudget->consumeDecode(bytes.size());
    if (bytes.size() < kMinimumHeaderSize + (allowPartial ? 0 : kFooterSize)
        || bytes.size() > kMaxFileBytes)
    {
        fail("file size is outside the safe range");
    }
    SemanticDecodeBudget localSemanticBudget{};
    SemanticDecodeBudget& semanticBudget = sharedSemanticBudget
        ? *sharedSemanticBudget : localSemanticBudget;
    semanticBudget.beginDocument();
    const bool hasFooter = bytes.size() >= kFooterSize
        && std::equal(kFooterMagic.begin(), kFooterMagic.end(),
            bytes.end() - static_cast<std::ptrdiff_t>(kFooterSize));
    // Do not infer a missing footer merely from the file length.  In
    // particular, a truncated mirror/footer pair must leave the final
    // section trailer in the body instead of subtracting a phantom 8-byte
    // footer and silently dropping the tail of that section.  When the
    // footer itself is absent, a checked mirror may still be recognized at
    // EOF and used as the framing boundary.
    const std::size_t footerOffset = hasFooter ? bytes.size() - kFooterSize : bytes.size();

    // Probe the checked header mirror independently from the footer. A footer
    // bit flip or a short write must not make us mistake the final semantic
    // section trailer for body data. With no valid footer, only candidates in
    // the final fixed mirror + footer window are considered, which prevents a
    // payload occurrence of mhdr from becoming trusted framing.
    bool mirrorValid = false;
    Bytes mirrorHeader{};
    std::optional<std::size_t> mirrorOffset{};
    const auto inspectMirror = [&](const std::size_t candidateOffset) -> bool
    {
        if (candidateOffset > bytes.size()
            || kHeaderMirrorSize > bytes.size() - candidateOffset)
        {
            return false;
        }
        const auto candidateBytes = bytes.subspan(candidateOffset, kHeaderMirrorSize);
        if (!std::equal(kHeaderMirrorMagic.begin(), kHeaderMirrorMagic.end(),
                candidateBytes.begin()))
        {
            return false;
        }
        Reader mirror(candidateBytes);
        mirror.raw(kHeaderMirrorMagic.size());
        const std::uint32_t mirrorHeaderSize = mirror.u32();
        const auto candidate = mirror.raw(kCanonicalHeaderSize);
        const std::uint32_t expectedMirrorCrc = mirror.u32();
        if (mirrorHeaderSize != kCanonicalHeaderSize || !mirror.empty()
            || crc32c(candidate) != expectedMirrorCrc
            || !hasValidCanonicalHeaderChecksum(candidate))
        {
            return false;
        }
        mirrorHeader.assign(candidate.begin(), candidate.end());
        mirrorValid = true;
        mirrorOffset = candidateOffset;
        return true;
    };

    if (hasFooter && footerOffset >= kHeaderMirrorSize)
    {
        const std::size_t expectedOffset = footerOffset - kHeaderMirrorSize;
        // A valid footer does not by itself prove that the preceding 40
        // bytes are the header mirror. Keep the semantic body through the
        // footer boundary unless the fixed-width mirror validates; otherwise
        // a short/absent mirror would hide the final payload.
        (void)inspectMirror(expectedOffset);
    }
    else if (!hasFooter && bytes.size() >= kHeaderMirrorSize)
    {
        const std::size_t firstCandidate = bytes.size() - kHeaderMirrorSize;
        const std::size_t lastCandidate = bytes.size() >= kHeaderMirrorSize + kFooterSize
            ? bytes.size() - kHeaderMirrorSize - kFooterSize
            : 0;
        for (std::size_t candidate = firstCandidate;; --candidate)
        {
            if (inspectMirror(candidate)) break;
            if (candidate == lastCandidate) break;
        }
    }
    const bool reserveMirror = mirrorOffset.has_value();

    const bool primaryMagic = sedMagicSize(bytes).has_value();
    const bool primaryHeaderAvailable = primaryMagic && bytes.size() >= kCanonicalHeaderSize;
    bool primaryHeaderValid = false;
    if (primaryHeaderAvailable)
    {
        primaryHeaderValid = crc32c(bytes.first(kHeaderChecksumOffset))
            == readU32At(bytes, kHeaderChecksumOffset);
    }
    if (!primaryHeaderValid && !mirrorValid) fail(primaryMagic
        ? "header checksum mismatch" : "signature mismatch");
    if (primaryHeaderValid && mirrorValid
        && !std::equal(bytes.begin(),
            bytes.begin() + static_cast<std::ptrdiff_t>(kCanonicalHeaderSize),
            mirrorHeader.begin(), mirrorHeader.end()))
    {
        fail("fixed-header copies disagree");
    }
    const bool headerRepaired = !primaryHeaderValid;
    const std::span<const std::byte> header = headerRepaired
        ? std::span<const std::byte>(mirrorHeader)
        : bytes.first(kCanonicalHeaderSize);

    if (!mirrorValid && !allowPartial && headerRepaired)
    {
        fail("fixed-header mirror is missing or damaged");
    }
    if (!allowPartial && (!hasFooter || !mirrorValid))
    {
        fail("fixed SED footer or header mirror is missing or damaged");
    }
    const std::size_t bodyEnd = reserveMirror ? *mirrorOffset : footerOffset;
    if (bodyEnd < kCanonicalHeaderSize) fail("file body is truncated");
    const std::uint32_t version = readU32At(header, kHeaderVersionOffset);
    if (!isSupportedSedBinaryVersion(version))
    {
        fail("unsupported format version");
    }
    const UvCoordinateSpace uvCoordinateSpace =
        sedBinaryVersionUsesTexturePixels(version)
        ? UvCoordinateSpace::TexturePixels
        : UvCoordinateSpace::Normalized;
    if (incremental)
    {
        const std::uint32_t reusableVersion = readU32At(
            reusableBytes, kHeaderVersionOffset);
        if (!isSupportedSedBinaryVersion(reusableVersion))
        {
            throw std::logic_error(
                "Incremental nested SED trust context has an unsupported version.");
        }
        if (reusableLevel->uvCoordinateSpace != uvCoordinateSpace)
        {
            throw std::logic_error(
                "Incremental nested SED states use different UV coordinate spaces.");
        }
        if (reusableVersion == version)
        {
            reusableFrames = trustedNestedRequiredFrames(reusableBytes);
        }
    }
    const ProjectType headerProject = static_cast<ProjectType>(
        std::bit_cast<std::int32_t>(readU32At(header, kHeaderProjectOffset)));
    if (headerProject != ProjectType::JKDF2 && headerProject != ProjectType::MOTS && headerProject != ProjectType::IJIM)
    {
        fail("unknown header project type");
    }
    const std::uint32_t sectionCount = readU32At(header, kHeaderSectionCountOffset);
    if (sectionCount == 0 || sectionCount > 1024) fail("section count is outside the safe range");
    if (isNestedSerializationProfile(profile)
        && sectionCount != kRequiredSectionCount)
    {
        fail("nested history state has noncanonical required-section count");
    }
    const std::int64_t timestamp = readI64At(header, kHeaderTimestampOffset);
    if (timestamp < 0)
    {
        fail("timestamp is negative");
    }
    if (isNestedSerializationProfile(profile)
        && timestamp != 0)
    {
        fail("nested timestamp is not canonical zero");
    }

    SedRecoveryLoadResult result{};
    std::optional<IncrementalLevelTransaction> incrementalTransaction{};
    if (incremental)
    {
        incrementalTransaction.emplace(
            *reusableLevel, result.level, *previousDomains);
        if (incrementalStats)
        {
            incrementalStats->usedTransactionalDomainReuse = true;
        }
    }
    else
    {
        result.level.clear();
        result.level.uvCoordinateSpace = uvCoordinateSpace;
    }
    result.timestamp = timestamp;
    // Permissive recovery must report every missing or damaged redundant
    // framing copy. A marker with a bad embedded-header/trailer CRC is still
    // corruption even when the primary header and footer remain valid.
    result.hadCorruption = headerRepaired || !hasFooter || !mirrorValid;
    const auto recordMissing = [&](const std::string& name, const bool required)
    {
        result.hadCorruption = true;
        result.partialRecoveryFromCorruption = true;
        if (std::find(result.missingSections.begin(), result.missingSections.end(), name)
            == result.missingSections.end())
        {
            result.missingSections.push_back(name);
        }
        auto& tier = required
            ? result.missingRequiredSections : result.missingOptionalSections;
        if (std::find(tier.begin(), tier.end(), name) == tier.end())
        {
            tier.push_back(name);
        }
    };
    std::unordered_map<std::string, bool> found{};
    ResourceCatalogs resourceCatalogs{};
    bool resourceTableAvailable = false;
    std::optional<Bytes> deferredHead{};
    std::optional<SedJklCogImportState> decodedJklCogImportState{};
    bool deferredHeadReusable{};
    bool resourceSectionChanged{};
    std::uint64_t expandedPayloadBytes{};
    std::size_t cursor = kCanonicalHeaderSize;
    const auto nextSectionAnchor = [&](const std::size_t start)
    {
        std::size_t anchorSearch = start;
        std::size_t mirrorSearch = start;
        std::optional<std::size_t> nextAnchor = findExactRecoveryMarker(
            bytes, kSectionAnchorMagic, anchorSearch, bodyEnd, recoveryBudget);
        std::optional<std::size_t> nextMirror = findExactRecoveryMarker(
            bytes, kSectionMirrorMagic, mirrorSearch, bodyEnd, recoveryBudget);
        while (nextAnchor || nextMirror)
        {
            const bool direct = nextAnchor
                && (!nextMirror || *nextAnchor <= *nextMirror);
            const std::size_t candidate = direct ? *nextAnchor : *nextMirror;
            if (direct)
            {
                anchorSearch = candidate + 1u;
                if (kSectionDescriptorSize <= bodyEnd - candidate)
                {
                    const auto descriptor = parseSectionDescriptor(
                        bytes.subspan(candidate, kSectionDescriptorSize));
                    if (descriptor && hasCanonicalFecAlignment(candidate, *descriptor)
                        && bodyEnd >= kSectionTrailerSize
                        && descriptor->storedSize
                            <= bodyEnd - candidate - kSectionHeaderSize
                        && candidate + kSectionHeaderSize
                            + static_cast<std::size_t>(descriptor->storedSize)
                            <= bodyEnd - kSectionTrailerSize)
                    {
                        const std::size_t trailerOffset = candidate + kSectionHeaderSize
                            + static_cast<std::size_t>(descriptor->storedSize);
                        const auto trailer = readSectionTrailer(bytes, trailerOffset);
                        if (trailer && std::equal(bytes.begin()
                                + static_cast<std::ptrdiff_t>(candidate),
                            bytes.begin() + static_cast<std::ptrdiff_t>(candidate
                                + kSectionDescriptorSize), trailer->begin(), trailer->end()))
                        {
                            return candidate;
                        }
                    }
                }
                nextAnchor = findExactRecoveryMarker(bytes, kSectionAnchorMagic,
                    anchorSearch, bodyEnd, recoveryBudget);
                continue;
            }

            // A damaged fixed trailer may overlap the following section's
            // primary descriptor.  In recovery mode that following section
            // is still locatable from its independently checked mirror.  Do
            // not trust the marker alone: readSectionTrailer() verifies the
            // outer trailer CRC, parseSectionDescriptor() verifies the
            // embedded descriptor CRC and V1 schema, and the backward extent
            // plus absolute FEC phase must identify one exact section start.
            mirrorSearch = candidate + 1u;
            if (candidate <= bodyEnd
                && kSectionTrailerSize <= bodyEnd - candidate)
            {
                const auto mirrored = readSectionTrailer(bytes, candidate);
                const auto mirroredDescriptor = mirrored
                    ? parseSectionDescriptor(*mirrored) : std::nullopt;
                if (mirroredDescriptor
                    && mirroredDescriptor->storedSize <= candidate
                    && candidate - static_cast<std::size_t>(
                        mirroredDescriptor->storedSize) >= kSectionHeaderSize)
                {
                    const std::size_t mirroredStart = candidate
                        - static_cast<std::size_t>(mirroredDescriptor->storedSize)
                        - kSectionHeaderSize;
                    if (mirroredStart >= start
                        && hasCanonicalFecAlignment(mirroredStart,
                            *mirroredDescriptor))
                    {
                        return mirroredStart;
                    }
                }
            }
            nextMirror = findExactRecoveryMarker(bytes, kSectionMirrorMagic,
                mirrorSearch, bodyEnd, recoveryBudget);
        }
        return bodyEnd;
    };
    for (std::uint32_t i = 0; i < sectionCount; ++i)
    {
        const std::size_t sectionStart = cursor;
        if (cursor > bodyEnd || bodyEnd - cursor < kSectionHeaderSize)
        {
            if (allowPartial)
            {
                result.hadCorruption = true;
                result.partialRecoveryFromCorruption = true;
                result.unidentifiedMissingSections += sectionCount - i;
                break;
            }
            fail("truncated section header");
        }
        const std::size_t payloadOffset = cursor + kSectionHeaderSize;
        std::optional<SectionDescriptor> descriptor = parseSectionDescriptor(
            bytes.subspan(cursor, kSectionHeaderSize));
        std::size_t trailerOffset{};
        bool descriptorRecovered = false;
        if (!descriptor)
        {
            if (hasValidSectionDescriptorChecksum(
                    bytes.subspan(cursor, kSectionHeaderSize)))
            {
                fail("section descriptor schema is unsupported");
            }
            // The primary descriptor is fixed-width, so a checked mirror can
            // still tell us exactly where this payload ends.  Search only for
            // valid mirrored descriptors; arbitrary payload bytes cannot
            // resynchronize the parser.
            std::size_t mirrorSearch = payloadOffset;
            while (const auto foundMirror = findExactRecoveryMarker(bytes,
                kSectionMirrorMagic, mirrorSearch, bodyEnd, recoveryBudget))
            {
                const std::size_t candidate = *foundMirror;
                mirrorSearch = candidate + 1u;
                if (kSectionTrailerSize > bodyEnd - candidate) continue;
                const auto mirrored = readSectionTrailer(bytes, candidate);
                if (!mirrored) continue;
                const auto candidateDescriptor = parseSectionDescriptor(*mirrored);
                if (!candidateDescriptor
                    && hasValidSectionDescriptorChecksum(
                        std::span<const std::byte>(*mirrored)))
                {
                    fail("section descriptor mirror schema is unsupported");
                }
                if (!candidateDescriptor
                    || !hasCanonicalFecAlignment(cursor, *candidateDescriptor)
                    || bodyEnd - payloadOffset < kSectionTrailerSize
                    || candidateDescriptor->storedSize > bodyEnd - payloadOffset - kSectionTrailerSize
                    || candidate != payloadOffset + static_cast<std::size_t>(candidateDescriptor->storedSize))
                {
                    continue;
                }
                descriptor = candidateDescriptor;
                trailerOffset = candidate;
                descriptorRecovered = true;
                break;
            }
            if (!descriptor)
            {
                result.hadCorruption = true;
                if (!allowPartial) fail("section descriptor and mirror are damaged");
                result.partialRecoveryFromCorruption = true;
                ++result.unidentifiedMissingSections;
                const std::size_t next = nextSectionAnchor(payloadOffset);
                if (next >= bodyEnd) break;
                cursor = next;
                continue;
            }
        }
        const SectionDescriptor& sectionDescriptor = *descriptor;
        if (sectionDescriptor.version != version)
        {
            fail("section descriptor version does not match the file version");
        }
        const std::string name = typeName(sectionDescriptor.type);
        if (isNestedSerializationProfile(profile)
            && (i >= kRequiredSectionCount
                || sectionDescriptor.type != kRequiredSectionTypes[i]))
        {
            fail("nested history required sections are not in canonical order");
        }
        if (descriptorRecovered)
        {
            result.hadCorruption = true;
            if (std::find(result.repairedSections.begin(), result.repairedSections.end(), name)
                == result.repairedSections.end())
            {
                result.repairedSections.push_back(name);
            }
        }
        const std::uint64_t storedSize64 = sectionDescriptor.storedSize;
        if (payloadOffset > bodyEnd || storedSize64 > bodyEnd - payloadOffset)
        {
            result.hadCorruption = true;
            if (!allowPartial) fail("section size is outside the safe range");
            recordMissing(name, isRequiredSection(name));
            const std::size_t next = nextSectionAnchor(payloadOffset);
            if (next >= bodyEnd) break;
            cursor = next;
            continue;
        }
        const std::size_t payloadEnd = payloadOffset
            + static_cast<std::size_t>(storedSize64);
        const bool completeTrailerFits = bodyEnd - payloadEnd >= kSectionTrailerSize;
        const bool finalTrailerMissing = !completeTrailerFits
            && allowPartial && i + 1u == sectionCount;
        if (!completeTrailerFits && !finalTrailerMissing)
        {
            result.hadCorruption = true;
            if (!allowPartial) fail("section size is outside the safe range");
            recordMissing(name, isRequiredSection(name));
            const std::size_t next = nextSectionAnchor(payloadEnd);
            if (next >= bodyEnd) break;
            cursor = next;
            continue;
        }
        if (trailerOffset == 0)
        {
            trailerOffset = payloadEnd;
        }
        const auto trailer = finalTrailerMissing
            ? std::optional<std::span<const std::byte>>{}
            : readSectionTrailer(bytes, trailerOffset);
        const auto trailerDescriptor = trailer ? parseSectionDescriptor(*trailer) : std::nullopt;
        std::optional<std::size_t> trailerResyncCursor{};
        if (trailer && !trailerDescriptor
            && hasValidSectionDescriptorChecksum(
                std::span<const std::byte>(*trailer)))
        {
            fail("section trailer descriptor schema is unsupported");
        }
        if (finalTrailerMissing)
        {
            // The fixed-width trailer is redundant. A permissive load may
            // still consume a complete final semantic payload when its 116
            // bytes are partially written or absent; strict V1 retains the
            // canonical trailer requirement above.
            result.hadCorruption = true;
            if (std::find(result.repairedSections.begin(),
                    result.repairedSections.end(), name)
                == result.repairedSections.end())
            {
                result.repairedSections.push_back(name);
            }
        }
        else if (!trailer || !trailerDescriptor)
        {
            result.hadCorruption = true;
            if (std::find(result.repairedSections.begin(), result.repairedSections.end(), name)
                == result.repairedSections.end())
            {
                result.repairedSections.push_back(name);
            }
            if (!allowPartial)
            {
                // Strict V1 requires both descriptor copies and the complete
                // checked trailer. Permissive recovery may retain a complete
                // final payload when only this redundant framing is damaged.
                fail("section trailer is damaged");
            }
            // A non-final trailer is fixed-width framing. If its bytes are
            // erased or fail their CRC, resume at the next independently
            // checked descriptor instead of blindly skipping 116 bytes. The
            // anchor helper requires the direct absolute FEC phase and an
            // exact mirrored trailer, so the descriptor copy embedded in the
            // damaged trailer cannot become a false resynchronization point.
            if (i + 1u < sectionCount)
            {
                const std::size_t next = nextSectionAnchor(payloadEnd);
                if (next < bodyEnd)
                {
                    trailerResyncCursor = next;
                }
            }
            if (descriptorRecovered)
            {
                recordMissing(name, isRequiredSection(name));
                const std::size_t next = nextSectionAnchor(trailerOffset + 1);
                if (next >= bodyEnd) break;
                cursor = next;
                continue;
            }
        }
        else if (!descriptorRecovered
            && !std::equal(bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
                bytes.begin() + static_cast<std::ptrdiff_t>(cursor + kSectionDescriptorSize),
                trailer->begin(), trailer->end()))
        {
            // Both copies are individually valid but disagree. There is no
            // principled way to choose which metadata is authoritative.
            // Strict open rejects it; corruption recovery discards the whole
            // ambiguous unit and resumes only at another checked anchor.
            result.hadCorruption = true;
            if (!allowPartial) fail("section descriptor copies disagree");
            result.partialRecoveryFromCorruption = true;
            ++result.unidentifiedMissingSections;
            const std::size_t next = nextSectionAnchor(trailerOffset + kSectionTrailerSize);
            if (next >= bodyEnd) break;
            cursor = next;
            continue;
        }
        const auto stored = bytes.subspan(payloadOffset, static_cast<std::size_t>(storedSize64));
        cursor = trailerResyncCursor.value_or(
            finalTrailerMissing ? bodyEnd : trailerOffset + kSectionTrailerSize);
        const bool required = isRequiredSection(name);
        const bool coreProtection = isCoreProtectionSection(name);
        if (found[name]) fail("duplicate section");
        if (sectionDescriptor.rawSize
            > SedMaximumExpandedPayloadBytes - expandedPayloadBytes)
        {
            fail("aggregate expanded payload exceeds the safe limit");
        }
        expandedPayloadBytes += sectionDescriptor.rawSize;

        const bool parityProtected = (sectionDescriptor.flags & kParityProtected) != 0;
        if (parityProtected && sectionDescriptor.codedOffset != 0
            && !std::all_of(stored.begin(),
                stored.begin() + static_cast<std::ptrdiff_t>(sectionDescriptor.codedOffset),
                [](const std::byte value) { return value == std::byte{0}; }))
        {
            // The bytes before the first coded slot are alignment padding, not
            // an erasure-bearing FEC chunk.  Strict V1 requires canonical zero
            // padding; permissive recovery records the framing damage while
            // decoding the independently checked coded payload.
            result.hadCorruption = true;
            if (std::find(result.repairedSections.begin(),
                    result.repairedSections.end(), name)
                == result.repairedSections.end())
            {
                result.repairedSections.push_back(name);
            }
            if (!allowPartial) fail("parity alignment padding is noncanonical");
        }

        Bytes expanded{};
        try
        {
            const std::uint32_t flags = sectionDescriptor.flags;
            const bool parityProtectedFromFlags = (flags & kParityProtected) != 0;
            const bool historyChecksumFromFlags =
                (flags & kHistoryNestedChecksum) != 0;
            if (profile == SedSerializationProfile::ProtectedV1
                && (!parityProtectedFromFlags || historyChecksumFromFlags))
            {
                fail("top-level semantic section is not parity protected");
            }
            if (profile == SedSerializationProfile::UnprotectedNested
                && (parityProtected || (flags & kCompressed) != 0
                    || historyChecksumFromFlags))
            {
                fail("nested semantic section uses top-level protection");
            }
            if (profile == SedSerializationProfile::HistoryNested
                && (parityProtected || (flags & kCompressed) != 0
                    || !historyChecksumFromFlags))
            {
                fail("history-nested semantic section uses noncanonical protection");
            }
            Bytes logicalStored{};
            if (parityProtectedFromFlags)
            {
                const std::uint32_t expectedParity = coreProtection
                    ? kFecRequiredParityStreams : kFecOptionalParityStreams;
                if (sectionDescriptor.parityStreams != expectedParity)
                {
                    fail("section uses the wrong parity protection tier");
                }
                if ((payloadOffset + sectionDescriptor.codedOffset) % kFecSlotBytes != 0)
                {
                    fail("parity payload is not 4 KiB aligned");
                }
                DecodedFecPayload decoded = decodeFecPayload(
                    stored.subspan(sectionDescriptor.codedOffset),
                    sectionDescriptor.logicalStoredSize,
                    sectionDescriptor.dataStreams,
                    sectionDescriptor.parityStreams,
                    sectionDescriptor.chunkPayloadSize,
                    sectionDescriptor.stripeCount);
                logicalStored = std::move(decoded.stored);
                result.repairedChunks += decoded.repairedChunks;
                if (decoded.ignoredPaddingChunks != 0 && !allowPartial)
                {
                    // Unused data slots and the zero-filled tail of a short
                    // final data slot are part of the canonical physical
                    // representation.  The permissive recovery path may
                    // normalize them without consuming parity capacity, but
                    // a strict load must never silently accept a noncanonical
                    // byte stream (even when its slot CRC was recomputed).
                    fail("parity payload contains noncanonical padding");
                }
                if (decoded.repairedChunks != 0 || decoded.ignoredPaddingChunks != 0
                    || decoded.inconsistentParityChunks != 0)
                {
                    // Padding damage and CRC-valid wrong parity do not consume
                    // an erasure budget or alter semantic bytes, but they
                    // remain observable as physical corruption for callers
                    // and audit logs.
                    result.hadCorruption = true;
                    if (std::find(result.repairedSections.begin(),
                        result.repairedSections.end(), name) == result.repairedSections.end())
                    {
                        result.repairedSections.push_back(name);
                    }
                }
            }
            else
            {
                logicalStored.assign(stored.begin(), stored.end());
            }
            if ((flags & kCompressed) != 0)
            {
                // Malformed token streams are represented by the dedicated
                // MalformedCompression/IntegrityFailure hierarchy. The
                // enclosing catch handles only that integrity family; bad
                // allocation and programming exceptions propagate normally.
                expanded = decompressFast(logicalStored,
                    static_cast<std::size_t>(sectionDescriptor.rawSize));
            }
            else
            {
                expanded = std::move(logicalStored);
            }
            const Digest256 expandedChecksum = historyChecksumFromFlags
                ? historyNestedChecksum(expanded) : sha256(expanded);
            if (expanded.size() != sectionDescriptor.rawSize
                || expandedChecksum != sectionDescriptor.expandedHash)
            {
                integrityFail(historyChecksumFromFlags
                    ? "expanded history section checksum mismatch"
                    : "expanded section strong hash mismatch");
            }
        }
        catch (const IntegrityFailure&)
        {
            if (!allowPartial) throw;
            recordMissing(name, required);
            continue;
        }

        // Once expanded size and SHA-256 are valid, semantic decoding is
        // intentionally outside the corruption catch. A schema/programming
        // failure must remain hard even on the permissive recovery API.
        if (required)
        {
            const bool canonicalRequiredPosition = incremental
                && reusableFrames.has_value()
                && i < kRequiredSectionCount
                && sectionDescriptor.type == kRequiredSectionTypes[i]
                && cursor >= sectionStart
                && cursor - sectionStart == (*reusableFrames)[i].size();
            const bool exactPreviousFrame = canonicalRequiredPosition
                && std::equal(
                    bytes.begin() + static_cast<std::ptrdiff_t>(sectionStart),
                    bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
                    (*reusableFrames)[i].begin(),
                    (*reusableFrames)[i].end());
            if (incremental && name == "rsrc")
            {
                resourceSectionChanged = !exactPreviousFrame;
                if (incrementalStats)
                {
                    incrementalStats->resourceSectionChanged =
                        resourceSectionChanged;
                }
            }
            const bool resourceIndexed = name == "head" || name == "sect"
                || name == "thng" || name == "tmpl" || name == "cogs"
                || name == "hnod";
            // rsrc is intentionally decoded for every state. Apart from proving
            // the current catalog independently, this supplies the namespace
            // for every newly decoded category-indexed section. No decision is
            // based on the untrusted HIST dirty mask.
            const bool reuseSemantic = incremental && reusableFrames.has_value()
                && name != "rsrc"
                && exactPreviousFrame
                && !(resourceSectionChanged && resourceIndexed);
            Reader section(expanded, &semanticBudget);
            const ResourceCatalogs* const catalogs =
                resourceTableAvailable ? &resourceCatalogs : nullptr;
            if (name == "head")
            {
                // `head` precedes `rsrc` for robust project/framing recovery,
                // but its master-CMP spelling is a categorized resource. Hold
                // this small checked payload until the catalog has decoded so
                // the spelling appears exactly once in the semantic file.
                deferredHead = std::move(expanded);
                deferredHeadReusable = reuseSemantic;
                found[name] = true;
                continue;
            }
            else if (reuseSemantic)
            {
                if (incrementalStats)
                    ++incrementalStats->reusedRequiredSections;
            }
            else if (name == "rsrc")
            {
                // The categorized table is decoded for every state so all
                // later references are checked against the candidate itself.
                // When its exact frame is unchanged, retain the already
                // admitted inventories instead of reallocating them.
                Level decodedResources{};
                resourceCatalogs = decodeResources(decodedResources, section);
                resourceTableAvailable = true;
                if (!incremental || !exactPreviousFrame)
                {
                    if (incrementalTransaction)
                    {
                        incrementalTransaction->prepare(
                            SedRequiredSection::Resources);
                        if (incrementalStats)
                        {
                            ++incrementalStats->
                                transactionallyReplacedRequiredSections;
                        }
                    }
                    swapRequiredLevelDomain(result.level, decodedResources,
                        SedRequiredSection::Resources);
                }
            }
            else if (name == "sect")
            {
                if (incrementalTransaction)
                    incrementalTransaction->prepare(SedRequiredSection::Sectors);
                decodeSectors(result.level, section, catalogs, allowPartial);
            }
            else if (name == "thng")
            {
                if (incrementalTransaction)
                    incrementalTransaction->prepare(SedRequiredSection::Things);
                decodeThings(result.level, section, catalogs, allowPartial,
                    sedBinaryVersionHasArchLighting(version));
            }
            else if (name == "tmpl")
            {
                if (incrementalTransaction)
                    incrementalTransaction->prepare(SedRequiredSection::Templates);
                decodeTemplates(result.level, section, catalogs, allowPartial);
            }
            else if (name == "cogs")
            {
                if (incrementalTransaction)
                    incrementalTransaction->prepare(SedRequiredSection::Cogs);
                decodeCogs(result.level, section, catalogs, allowPartial);
            }
            else if (name == "lite")
            {
                if (incrementalTransaction)
                    incrementalTransaction->prepare(SedRequiredSection::Lights);
                decodeLights(result.level, section);
            }
            else if (name == "hnod")
            {
                if (incrementalTransaction)
                    incrementalTransaction->prepare(SedRequiredSection::Hierarchy);
                decodeHierarchy(result.level, section, catalogs, allowPartial);
            }
            if (!reuseSemantic && !section.empty())
                fail("required section contains unexplained trailing data");
            if (!reuseSemantic && incrementalStats)
            {
                ++incrementalStats->decodedRequiredSections;
                if (name != "rsrc")
                {
                    ++incrementalStats->
                        transactionallyReplacedRequiredSections;
                }
            }
        }
        else if (sectionDescriptor.type == kJklCogImportSectionType)
        {
            try
            {
                Reader section(expanded, &semanticBudget);
                decodedJklCogImportState = decodeJklCogImportState(section);
            }
            catch (const std::bad_alloc&)
            {
                throw;
            }
            catch (const std::runtime_error&)
            {
                // jimp is optional provenance. A permissive recovery load may
                // keep every required Level domain after this metadata is
                // beyond repair, but it must report the loss and must not
                // reconstruct provenance from the native COG rows.
                if ( !allowPartial ) throw;
                decodedJklCogImportState.reset();
                recordMissing(name, false);
                continue;
            }
        }
        else
        {
            result.optionalSections.push_back(SedOptionalSection{
                sectionDescriptor.type, std::move(expanded) });
        }
        found[name] = true;
    }
    // HEAD precedes RSRC on the wire, so its provisional exact-frame reuse
    // decision was made before the current catalog was known. A catalog
    // change invalidates every indexed domain, including HEAD, regardless of
    // whether this particular encoded resource reference stayed byte-identical.
    if (resourceSectionChanged) deferredHeadReusable = false;
    if (deferredHead && !deferredHeadReusable)
    {
        Reader head(*deferredHead, &semanticBudget);
        if (incrementalTransaction)
            incrementalTransaction->prepare(SedRequiredSection::Head);
        decodeHead(result.level, head,
            resourceTableAvailable ? &resourceCatalogs : nullptr,
            allowPartial);
        if (!head.empty()) fail("required section contains unexplained trailing data");
        if (incrementalStats)
        {
            ++incrementalStats->decodedRequiredSections;
            ++incrementalStats->transactionallyReplacedRequiredSections;
        }
    }
    else if (deferredHeadReusable && incrementalStats)
    {
        ++incrementalStats->reusedRequiredSections;
    }
    const auto allRequiredSectionsFound = [&]()
    {
        for (const char* required : {
                 "head", "rsrc", "sect", "thng", "tmpl", "cogs", "lite", "hnod" })
        {
            if (!found[required]) return false;
        }
        return true;
    };
    if ( decodedJklCogImportState )
    {
        if ( allRequiredSectionsFound() )
        {
            const SedJklCogImportState& state = *decodedJklCogImportState;
            const bool represented = state.cogRowsPresent
                || (!state.surfaceRefs.empty()
                    && hasRawJklCogRows(result.level));
            if ( represented )
            {
                bool validSurfaceRefs = true;
                try
                {
                    validateJklCogImportSurfaceRefs(result.level,
                        state.surfaceRefs);
                }
                catch ( const std::runtime_error& )
                {
                    if ( !allowPartial ) throw;
                    validSurfaceRefs = false;
                }
                if ( !validSurfaceRefs )
                {
                    result.level.importedJklCogRowsPresent = false;
                    result.level.importedJklSurfaceRefs.clear();
                    decodedJklCogImportState.reset();
                    recordMissing("jimp", false);
                }
                else
                {
                    result.level.importedJklCogRowsPresent = state.cogRowsPresent;
                    result.level.importedJklSurfaceRefs = state.surfaceRefs;
                }
            }
            else
            {
                result.level.importedJklCogRowsPresent = false;
                result.level.importedJklSurfaceRefs.clear();
            }
        }
        else if ( allowPartial )
        {
            // The import map is meaningful only against the complete decoded
            // geometry/COG domains. Do not guess when corruption removed that
            // context; expose its loss beside the already reported Level loss.
            recordMissing("jimp", false);
        }
    }
    if ( nestedCogImportState )
    {
        SedJklCogImportState& state = *nestedCogImportState;
        const bool represented = state.cogRowsPresent
            || (!state.surfaceRefs.empty()
                && hasRawJklCogRows(result.level));
        if ( represented )
        {
            validateJklCogImportSurfaceRefs(result.level, state.surfaceRefs);
        }
        else
        {
            state.cogRowsPresent = false;
            state.surfaceRefs.clear();
        }
        incrementalTransaction->replaceCogImportState(std::move(state));
        if (previousCogImportStateChanged)
        {
            *previousCogImportStateChanged =
                incrementalTransaction->cogImportStateChanged();
        }
    }
    Writer expectedMirrorWriter{};
    expectedMirrorWriter.raw(kHeaderMirrorMagic);
    expectedMirrorWriter.u32(kCanonicalHeaderSize);
    expectedMirrorWriter.raw(header);
    expectedMirrorWriter.u32(crc32c(header));
    Bytes expectedTrailingFraming = expectedMirrorWriter.take();
    if (!hasFooter)
    {
        expectedTrailingFraming.insert(expectedTrailingFraming.end(),
            kFooterMagic.begin(), kFooterMagic.end());
    }
    const std::size_t trailingByteCount = cursor <= bodyEnd
        ? bodyEnd - cursor : std::numeric_limits<std::size_t>::max();
    const bool truncatedCanonicalTrailingFraming = primaryHeaderValid
        && !mirrorValid
        && trailingByteCount != 0
        && trailingByteCount < expectedTrailingFraming.size()
        && found.size() == sectionCount
        && allRequiredSectionsFound()
        && std::equal(bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
            bytes.begin() + static_cast<std::ptrdiff_t>(bodyEnd),
            expectedTrailingFraming.begin());
    const bool exactDamagedMirrorAndFooter = primaryHeaderValid
        && !hasFooter
        && cursor <= bodyEnd
        && bodyEnd - cursor == kHeaderMirrorSize + kFooterSize
        && found.size() == sectionCount
        && allRequiredSectionsFound();
    const bool canonicalZeroMirrorAndFooter = exactDamagedMirrorAndFooter
        && std::all_of(bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
            bytes.begin() + static_cast<std::ptrdiff_t>(bodyEnd),
            [](const std::byte value) { return value == std::byte{0}; });
    // A zero-filled fixed suffix is the historical strict-load compatibility
    // case. Permissive recovery additionally treats an exact 40-byte suffix
    // as redundant framing when both mirror/footer markers were bit-damaged,
    // provided every declared semantic section already validated.
    const bool knownDamagedMirrorAndFooter = canonicalZeroMirrorAndFooter;
    const bool exactDamagedMirrorWithFooter = hasFooter && !mirrorValid
        && cursor <= bodyEnd
        && bodyEnd - cursor == kHeaderMirrorSize
        && found.size() == sectionCount
        && allRequiredSectionsFound();
    if (cursor != bodyEnd)
    {
        if (!allowPartial && !knownDamagedMirrorAndFooter)
        {
            fail("trailing bytes before footer");
        }
        const bool knownDamagedMirrorTail = exactDamagedMirrorAndFooter
            || exactDamagedMirrorWithFooter
            || truncatedCanonicalTrailingFraming;
        // A permissive caller may still recover every semantic section. A
        // known-size fixed mirror/footer remainder is framing-only damage; any
        // other unchecked suffix is unexplained and should be reported as a
        // partial Level recovery.
        result.hadCorruption = true;
        if (!knownDamagedMirrorTail)
        {
            result.partialRecoveryFromCorruption = true;
            ++result.unidentifiedMissingSections;
        }
    }

    for (const char* required : { "head", "rsrc", "sect", "thng", "tmpl", "cogs", "lite", "hnod" })
    {
        if (!found[required])
        {
            if (!allowPartial) fail(std::string("missing required section ") + required);
            recordMissing(required, true);
        }
    }
    if (allRequiredSectionsFound())
    {
        const ResourceIndex canonicalResources = buildResourceIndex(result.level);
        if (canonicalResources.catalogs() != resourceCatalogs)
        {
            fail("rsrc string table is not canonical first-use order");
        }
        if (incremental)
        {
            // Reused objects were admitted by the preceding state and newly
            // decoded objects were charged above, but their combination must
            // still obey the one-Level limits. This also repeats the complete
            // finite-value graph validation before publication.
            const SedLevelDecodedFootprint footprint =
                validateLevelBeforeResourceIndex(result.level);
            validateResourceIndexForSerialization(
                footprint, canonicalResources);
        }
    }
    if (found["head"] && result.level.kind != headerProject
        && (!allowPartial || !synthesizedHeaderFraming))
    {
        fail("project type copies disagree");
    }
    if (found["head"])
    {
        result.projectType = result.level.kind;
        result.projectTypeTrusted = true;
    }
    else if (!synthesizedHeaderFraming)
    {
        // A checksum-valid primary or mirrored fixed header is sufficient to
        // identify the project even when the HEAD semantic payload is lost.
        result.level.kind = headerProject;
        result.projectType = headerProject;
        result.projectTypeTrusted = true;
    }
    else
    {
        // The scanner synthesized a JKDF2 framing placeholder because both
        // fixed headers and HEAD were unavailable. Do not expose that
        // placeholder as an authoritative project identity.
        result.projectType.reset();
        result.projectTypeTrusted = false;
    }
    // Integrity-valid bytes are still untrusted input. Validate the complete
    // decoded object graph before it can reach renderer/editor code. In the
    // partial-recovery path the presence map deliberately validates only
    // sections that actually survived; checker-level inconsistencies remain
    // loadable so the editor can report and repair them. The incremental path
    // already performed this same complete graph validation above through
    // validateLevelBeforeResourceIndex(), before checking the reconstructed
    // catalog footprint; do not traverse a large unchanged graph twice.
    if (!incremental) validateDecodedLevel(result.level, found);
    if (incrementalTransaction)
    {
        *previousDomainsMask = incrementalTransaction->prepared();
        incrementalTransaction->commit();
    }
    return result;
}

Level loadFromSedBytes(const std::span<const std::byte> bytes)
{
    return decodeSedBytes(bytes, false, SedSerializationProfile::ProtectedV1).level;
}

Level loadNestedSedLevelBytes(const std::span<const std::byte> bytes)
{
    return decodeSedBytes(bytes, false,
        SedSerializationProfile::UnprotectedNested).level;
}

Level loadHistoryNestedSedLevelBytes(
    const std::span<const std::byte> bytes)
{
    return decodeSedBytes(bytes, false,
        SedSerializationProfile::HistoryNested).level;
}

SedNestedHistoryDecoder::SedNestedHistoryDecoder(
    const Level& authoritativeCurrent)
    : current_(authoritativeCurrent)
{
    SedGeneratedUnprotectedDocument generated =
        generateUnprotectedSedDocument(authoritativeCurrent);
    currentBytes_ = std::move(generated.bytes);
}

void SedNestedHistoryDecoder::advance(
    const std::span<const std::byte> bytes)
{
    advance(std::vector<std::byte>(bytes.begin(), bytes.end()),
        SedJklCogImportState{});
}

void SedNestedHistoryDecoder::advance(
    const std::span<const std::byte> bytes,
    SedJklCogImportState cogImportState)
{
    advance(std::vector<std::byte>(bytes.begin(), bytes.end()),
        std::move(cogImportState));
}

void SedNestedHistoryDecoder::advance(
    std::vector<std::byte>&& bytes)
{
    advance(std::move(bytes), SedJklCogImportState{});
}

void SedNestedHistoryDecoder::advance(
    std::vector<std::byte>&& bytes,
    SedJklCogImportState cogImportState)
{
    SedNestedHistoryDecodeStats decodedStats{};
    Level decodedPreviousDomains{};
    SedRequiredSection decodedPreviousDomainsMask{ SedRequiredSection::None };
    bool decodedPreviousCogImportStateChanged{};
    SedRecoveryLoadResult decoded = decodeSedBytes(
        std::span<const std::byte>(bytes),
        false,
        SedSerializationProfile::HistoryNested,
        false,
        nullptr,
        nullptr,
        &current_,
        currentBytes_,
        &decodedStats,
        &decodedPreviousDomains,
        &decodedPreviousDomainsMask,
        &cogImportState,
        &decodedPreviousCogImportStateChanged);
    if (!hasAny(decodedPreviousDomainsMask, SedRequiredSection::Head))
    {
        // The incremental previous-domain carrier starts as Level{} and only
        // receives sections whose frames changed. When HEAD is reused, its
        // project kind is unchanged from the validated current Level, so keep
        // that identity available to project-specific domain comparisons.
        decodedPreviousDomains.kind = decoded.level.kind;
    }
    current_ = std::move(decoded.level);
    previousDomains_ = std::move(decodedPreviousDomains);
    previousDomainsMask_ = decodedPreviousDomainsMask;
    previousCogImportStateChanged_ = decodedPreviousCogImportStateChanged;
    currentBytes_ = std::move(bytes);
    lastStats_ = decodedStats;
    materializedPrevious_.reset();
    changedThingIndices_.reset();
    changedSectorIndices_.reset();
    changedThingIndicesComputed_ = false;
    changedSectorIndicesComputed_ = false;
    hasPrevious_ = true;
}

const Level& SedNestedHistoryDecoder::previousLevel() const
{
    if (!hasPrevious_)
    {
        throw std::logic_error(
            "Incremental nested SED decoder has no previous state.");
    }
    if (!materializedPrevious_)
    {
        materializedPrevious_ = current_;
        ++previousLevelMaterializationCount_;
        Level& previous = *materializedPrevious_;
        const auto restore = [this, &previous](const SedRequiredSection section)
        {
            if (!hasAny(previousDomainsMask_, section)) return;
            switch (section)
            {
            case SedRequiredSection::Head:
                previous.header = previousDomains_.header;
                previous.exportBufferExtras = previousDomains_.exportBufferExtras;
                previous.kind = previousDomains_.kind;
                previous.masterCMP = previousDomains_.masterCMP;
                previous.ppunit = previousDomains_.ppunit;
                previous.lvisstring = previousDomains_.lvisstring;
                previous.curId = previousDomains_.curId;
                break;
            case SedRequiredSection::Resources:
                previous.colormaps = previousDomains_.colormaps;
                previous.materials = previousDomains_.materials;
                previous.layers = previousDomains_.layers;
                break;
            case SedRequiredSection::Sectors:
                previous.sectors = previousDomains_.sectors;
                break;
            case SedRequiredSection::Things:
                previous.things = previousDomains_.things;
                previous.motsArchLighting = previousDomains_.motsArchLighting;
                previous.motsArchLightingBakeBaselines =
                    previousDomains_.motsArchLightingBakeBaselines;
                break;
            case SedRequiredSection::Templates:
                previous.templates = previousDomains_.templates;
                break;
            case SedRequiredSection::Cogs:
                previous.cogs = previousDomains_.cogs;
                break;
            case SedRequiredSection::Lights:
                previous.lights = previousDomains_.lights;
                break;
            case SedRequiredSection::Hierarchy:
                previous.h3doNodes = previousDomains_.h3doNodes;
                break;
            default: break;
            }
        };
        restore(SedRequiredSection::Head);
        restore(SedRequiredSection::Resources);
        restore(SedRequiredSection::Sectors);
        restore(SedRequiredSection::Things);
        restore(SedRequiredSection::Templates);
        restore(SedRequiredSection::Cogs);
        restore(SedRequiredSection::Lights);
        restore(SedRequiredSection::Hierarchy);
        if (previousCogImportStateChanged_)
        {
            previous.importedJklCogRowsPresent =
                previousDomains_.importedJklCogRowsPresent;
            previous.importedJklSurfaceRefs =
                previousDomains_.importedJklSurfaceRefs;
        }
    }
    return *materializedPrevious_;
}

bool SedNestedHistoryDecoder::lastTransitionEqualsIgnoringHistoryDomains(
    const bool ignoreThings,
    const bool ignoreLightsAndBakedLighting,
    const bool ignoreCogs) const
{
    if (!hasPrevious_)
    {
        throw std::logic_error(
            "Incremental nested SED decoder has no previous transition.");
    }
    const auto changed = [this](const SedRequiredSection section)
    {
        return hasAny(previousDomainsMask_, section);
    };
    if (!ignoreCogs && previousCogImportStateChanged_
        && !sameRepresentedJklCogImportState(
            current_, previousDomains_.importedJklCogRowsPresent,
            previousDomains_.importedJklSurfaceRefs))
    {
        return false;
    }
    if (changed(SedRequiredSection::Head)
        && (!sameHeader(previousDomains_.header, current_.header)
            || previousDomains_.exportBufferExtras != current_.exportBufferExtras
            || previousDomains_.kind != current_.kind
            || previousDomains_.masterCMP != current_.masterCMP
            || !sameDouble(previousDomains_.ppunit, current_.ppunit)
            || previousDomains_.lvisstring != current_.lvisstring))
    {
        return false;
    }
    if (changed(SedRequiredSection::Resources)
        && (previousDomains_.colormaps != current_.colormaps
            || previousDomains_.materials != current_.materials
            || previousDomains_.layers != current_.layers))
    {
        return false;
    }
    if (changed(SedRequiredSection::Sectors))
    {
        const bool same = previousDomains_.sectors.size() == current_.sectors.size()
            && std::equal(previousDomains_.sectors.begin(),
                previousDomains_.sectors.end(), current_.sectors.begin(),
                [ignoreLightsAndBakedLighting](
                    const Sector& lhs, const Sector& rhs) noexcept
                {
                    return sameSectorIgnoringBakedLighting(
                        lhs, rhs, ignoreLightsAndBakedLighting);
                });
        if (!same) return false;
    }
    const bool allowMotsArchBakeOutput = !ignoreThings
        && ignoreLightsAndBakedLighting
        && current_.kind == ProjectType::MOTS
        && (hasMotsArchBakeSource(previousDomains_)
            || hasMotsArchBakeSource(current_));
    if (changed(SedRequiredSection::Things) && !ignoreThings)
    {
        if (allowMotsArchBakeOutput)
        {
            if (!sameMotsArchBakeHistoryOutput(previousDomains_, current_))
                return false;
        }
        else if (!sameSequence(previousDomains_.things, current_.things, sameThing)
            || !sameArchLightingSequence(
                previousDomains_.motsArchLighting, current_.motsArchLighting)
            || !sameMotsArchLightingBakeBaselineSequence(
                previousDomains_.motsArchLightingBakeBaselines,
                current_.motsArchLightingBakeBaselines))
        {
            return false;
        }
    }
    if (changed(SedRequiredSection::Templates)
        && !sameSequence(previousDomains_.templates, current_.templates, sameTemplate))
    {
        return false;
    }
    if (!ignoreCogs && changed(SedRequiredSection::Cogs)
        && !sameSequence(previousDomains_.cogs, current_.cogs, sameCog))
    {
        return false;
    }
    if (!ignoreLightsAndBakedLighting
        && changed(SedRequiredSection::Lights)
        && !sameSequence(previousDomains_.lights, current_.lights, sameLight))
    {
        return false;
    }
    if (changed(SedRequiredSection::Hierarchy)
        && !sameSequence(previousDomains_.h3doNodes, current_.h3doNodes, sameHNode))
    {
        return false;
    }
    return true;
}

const std::optional<std::vector<std::int32_t>>&
SedNestedHistoryDecoder::lastChangedThingIndices() const
{
    if (changedThingIndicesComputed_) return changedThingIndices_;
    if (!hasPrevious_)
    {
        throw std::logic_error(
            "Incremental nested SED decoder has no previous transition.");
    }
    changedThingIndicesComputed_ = true;
    if (!lastTransitionEqualsIgnoringHistoryDomains(true, false, false))
    {
        changedThingIndices_.reset();
        return changedThingIndices_;
    }
    const bool previousHasThings = hasAny(
        previousDomainsMask_, SedRequiredSection::Things);
    if (previousHasThings
        && (!sameArchLightingSequence(previousDomains_.motsArchLighting,
                current_.motsArchLighting)
            || !sameMotsArchLightingBakeBaselineSequence(
                previousDomains_.motsArchLightingBakeBaselines,
                current_.motsArchLightingBakeBaselines)))
    {
        // A Thing-index subset cannot represent a table-only ARCH edit; the
        // caller must refresh the complete Things renderer domain.
        changedThingIndices_.reset();
        return changedThingIndices_;
    }
    const std::vector<Thing>& previous = previousHasThings
        ? previousDomains_.things : current_.things;
    if (previous.size() != current_.things.size())
    {
        changedThingIndices_.reset();
        return changedThingIndices_;
    }
    changedThingIndices_.emplace();
    changedThingIndices_->reserve(current_.things.size());
    for (std::size_t index = 0; index < current_.things.size(); ++index)
    {
        if (!sameThing(previous[index], current_.things[index]))
        {
            if (index > static_cast<std::size_t>(
                    std::numeric_limits<std::int32_t>::max()))
            {
                changedThingIndices_.reset();
                return changedThingIndices_;
            }
            changedThingIndices_->push_back(static_cast<std::int32_t>(index));
        }
    }
    return changedThingIndices_;
}

const std::optional<std::vector<std::int32_t>>&
SedNestedHistoryDecoder::lastChangedSectorIndices() const
{
    if (changedSectorIndicesComputed_) return changedSectorIndices_;
    if (!hasPrevious_)
    {
        throw std::logic_error(
            "Incremental nested SED decoder has no previous transition.");
    }
    changedSectorIndicesComputed_ = true;
    // Geometry subsets require every non-Sector domain to remain exact. The
    // generic history predicate cannot express "ignore all sector changes"
    // because its light mode excludes only baked colors, so check the other
    // prepared domains explicitly and skip Sectors here.
    const auto changed = [this](const SedRequiredSection section)
    {
        return hasAny(previousDomainsMask_, section);
    };
    const bool otherDomainsEqual =
        (!changed(SedRequiredSection::Head)
            || (sameHeader(previousDomains_.header, current_.header)
                && previousDomains_.exportBufferExtras == current_.exportBufferExtras
                && previousDomains_.kind == current_.kind
                && previousDomains_.masterCMP == current_.masterCMP
                && sameDouble(previousDomains_.ppunit, current_.ppunit)
                && previousDomains_.lvisstring == current_.lvisstring))
        && (!changed(SedRequiredSection::Resources)
            || (previousDomains_.colormaps == current_.colormaps
                && previousDomains_.materials == current_.materials
                && previousDomains_.layers == current_.layers))
        && (!changed(SedRequiredSection::Things)
            || (sameSequence(previousDomains_.things, current_.things, sameThing)
                && sameArchLightingSequence(
                    previousDomains_.motsArchLighting,
                    current_.motsArchLighting)
                && sameMotsArchLightingBakeBaselineSequence(
                    previousDomains_.motsArchLightingBakeBaselines,
                    current_.motsArchLightingBakeBaselines)))
        && (!changed(SedRequiredSection::Templates)
            || sameSequence(previousDomains_.templates, current_.templates, sameTemplate))
        && (!changed(SedRequiredSection::Cogs)
            || sameSequence(previousDomains_.cogs, current_.cogs, sameCog))
        && (!changed(SedRequiredSection::Lights)
            || sameSequence(previousDomains_.lights, current_.lights, sameLight))
        && (!changed(SedRequiredSection::Hierarchy)
            || sameSequence(previousDomains_.h3doNodes, current_.h3doNodes, sameHNode))
        && (!previousCogImportStateChanged_
            || sameRepresentedJklCogImportState(
                current_, previousDomains_.importedJklCogRowsPresent,
                previousDomains_.importedJklSurfaceRefs));
    if (!otherDomainsEqual)
    {
        changedSectorIndices_.reset();
        return changedSectorIndices_;
    }
    const std::vector<Sector>& previous = hasAny(
        previousDomainsMask_, SedRequiredSection::Sectors)
        ? previousDomains_.sectors : current_.sectors;
    if (previous.size() != current_.sectors.size())
    {
        changedSectorIndices_.reset();
        return changedSectorIndices_;
    }
    changedSectorIndices_.emplace();
    changedSectorIndices_->reserve(current_.sectors.size());
    for (std::size_t index = 0; index < current_.sectors.size(); ++index)
    {
        if (!sameSector(previous[index], current_.sectors[index]))
        {
            if (index > static_cast<std::size_t>(
                    std::numeric_limits<std::int32_t>::max()))
            {
                changedSectorIndices_.reset();
                return changedSectorIndices_;
            }
            changedSectorIndices_->push_back(static_cast<std::int32_t>(index));
        }
    }
    return changedSectorIndices_;
}

SedFile loadSedFileBytes(const std::span<const std::byte> bytes)
{
    SedRecoveryLoadResult decoded = decodeSedBytes(
        bytes, false, SedSerializationProfile::ProtectedV1);
    return SedFile{
        std::move(decoded.level),
        std::move(decoded.optionalSections),
        decoded.timestamp,
        decoded.repairedChunks,
        decoded.hadCorruption,
        std::move(decoded.repairedSections) };
}

namespace
{

struct DecodedEmbeddedDocument final
{
    SedEmbeddedDocument document{};
    SedRecoveryLoadResult decoded{};
};

std::optional<DecodedEmbeddedDocument> findEmbeddedSedDocumentDecoded(
    const std::span<const std::byte> bytes, const std::size_t searchOffset,
    const SedSerializationProfile profile, RecoveryWorkBudget& recoveryBudget,
    SemanticDecodeBudget& semanticBudget)
{
    if (bytes.size() > kMaxFileBytes)
    {
        fail("embedded-document search exceeds the safe file limit");
    }
    if (searchOffset > bytes.size()
        || kCanonicalHeaderSize > bytes.size() - searchOffset)
    {
        return std::nullopt;
    }

    // Header candidates are found by their exact four-byte signature. The
    // shared marker, descriptor, strict-decode, and scan budgets also cover
    // every later permissive recovery pass over this same input.
    constexpr std::size_t kMaximumCheckedHeaders = 256;
    std::size_t checkedHeaders{};
    const std::size_t lastStart = bytes.size() - kCanonicalHeaderSize;
    std::size_t search = searchOffset;
    while (search <= lastStart)
    {
        const std::optional<std::size_t> found = findExactRecoveryMarker(
            bytes, kMagic, search, bytes.size(), &recoveryBudget);
        if (!found || *found > lastStart) break;
        const std::size_t start = *found;
        search = start + 1u;
        try
        {
            const auto header = bytes.subspan(start, kCanonicalHeaderSize);
            const std::uint32_t binaryVersion = readU32At(
                header, kHeaderVersionOffset);
            if (!isSupportedSedBinaryVersion(binaryVersion)
                || crc32c(header.first(kHeaderChecksumOffset))
                    != readU32At(header, kHeaderChecksumOffset))
            {
                continue;
            }
            const std::uint32_t count = readU32At(header, kHeaderSectionCountOffset);
            if (count == 0 || count > 1024) continue;
            if (++checkedHeaders > kMaximumCheckedHeaders) break;
            recoveryBudget.consumeDescriptorProbes(count);
            std::size_t cursor = start + kCanonicalHeaderSize;
            bool framingValid = true;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                if (cursor > bytes.size()
                    || kSectionHeaderSize > bytes.size() - cursor)
                {
                    framingValid = false;
                    break;
                }
                const auto descriptor = parseSectionDescriptor(
                    bytes.subspan(cursor, kSectionHeaderSize));
                if (!descriptor || descriptor->version != binaryVersion
                    || descriptor->storedSize > bytes.size() - cursor
                    || descriptor->storedSize + kSectionHeaderSize
                            + kSectionTrailerSize
                        > bytes.size() - cursor)
                {
                    framingValid = false;
                    break;
                }
                const std::size_t trailerOffset = cursor + kSectionHeaderSize
                    + static_cast<std::size_t>(descriptor->storedSize);
                const auto trailer = readSectionTrailer(bytes, trailerOffset);
                if (!trailer
                    || !std::equal(bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
                        bytes.begin() + static_cast<std::ptrdiff_t>(
                            cursor + kSectionHeaderSize),
                        trailer->begin(), trailer->end()))
                {
                    framingValid = false;
                    break;
                }
                cursor = trailerOffset + kSectionTrailerSize;
            }
            if (!framingValid
                || kHeaderMirrorSize + kFooterSize > bytes.size() - cursor)
            {
                continue;
            }

            Reader mirror(bytes.subspan(cursor, kHeaderMirrorSize));
            if (!mirror.startsWith(kHeaderMirrorMagic)) continue;
            mirror.raw(kHeaderMirrorMagic.size());
            const std::uint32_t mirrorHeaderSize = mirror.u32();
            const auto mirroredHeader = mirror.raw(kCanonicalHeaderSize);
            const std::uint32_t expectedMirrorCrc = mirror.u32();
            if (mirrorHeaderSize != kCanonicalHeaderSize || !mirror.empty()
                || crc32c(mirroredHeader) != expectedMirrorCrc
                || !hasValidCanonicalHeaderChecksum(mirroredHeader)
                || !std::equal(header.begin(), header.end(), mirroredHeader.begin())
                || !std::equal(kFooterMagic.begin(), kFooterMagic.end(),
                    bytes.begin() + static_cast<std::ptrdiff_t>(
                        cursor + kHeaderMirrorSize)))
            {
                continue;
            }
            const std::size_t size = cursor - start
                + kHeaderMirrorSize + kFooterSize;
            recoveryBudget.consumeStrictCandidate();
            if (!recoveryBudget.consumeStrictValidationBytes(size)) continue;
            SedRecoveryLoadResult decoded = decodeSedBytes(
                bytes.subspan(start, size), false, profile, false,
                &recoveryBudget, &semanticBudget);
            return DecodedEmbeddedDocument{
                SedEmbeddedDocument{ start, size }, std::move(decoded) };
        }
        catch (const RecoveryBudgetExceeded&)
        {
            throw;
        }
        catch (const std::runtime_error&)
        {
            // Candidate magic can occur in arbitrary payload bytes. Continue
            // until a fully validated framing and semantic payload are found.
        }
    }
    return std::nullopt;
}

} // namespace

SedRecoveryLoadResult recoverFromSedBytes(const std::span<const std::byte> bytes)
{
    // Reject hostile or accidentally concatenated inputs before any framing
    // scan or recovery allocation.  The semantic decoder enforces the same
    // cap, but recovery can otherwise scan an arbitrarily large prefix first.
    if (bytes.size() > kMaxFileBytes)
    {
        fail("file exceeds the maximum supported size");
    }
    RecoveryWorkBudget recoveryBudget{ bytes.size() };
    SemanticDecodeBudget semanticBudget{};

    // A complete directly framed document is always the strongest candidate.
    // If direct strict validation fails, prefer any later complete, strictly
    // validated document before attempting permissive prefix/headerless
    // salvage. This handles both a stale checksum-valid fixed header and a
    // surviving checked section fragment prepended to an intact document.
    if (hasSedBinarySignature(bytes))
    {
        try
        {
            return decodeSedBytes(bytes, false,
                SedSerializationProfile::ProtectedV1, false, &recoveryBudget,
                &semanticBudget);
        }
        catch (const RecoveryBudgetExceeded&)
        {
            throw;
        }
        catch (const std::runtime_error&)
        {
            // Continue with complete embedded-document discovery, then the
            // existing independently checked partial-recovery paths.
        }
    }
    if (std::optional<DecodedEmbeddedDocument> embedded =
            findEmbeddedSedDocumentDecoded(bytes, 0,
                SedSerializationProfile::ProtectedV1, recoveryBudget,
                semanticBudget))
    {
        // Discovery already performed the complete strict semantic decode.
        // Return that result directly instead of decompressing, hashing, and
        // rebuilding the same embedded Level a second time.
        return std::move(embedded->decoded);
    }

    if (hasSedBinarySignature(bytes))
    {
        const bool primaryHeaderChecksumValid = bytes.size() >= kCanonicalHeaderSize
            && crc32c(bytes.first(kHeaderChecksumOffset))
                == readU32At(bytes, kHeaderChecksumOffset);
        const bool hasFooter = bytes.size() >= kFooterSize
            && std::equal(kFooterMagic.begin(), kFooterMagic.end(),
                bytes.end() - static_cast<std::ptrdiff_t>(kFooterSize));
        const std::size_t footerOffset = hasFooter
            ? bytes.size() - kFooterSize : bytes.size();
        bool mirrorHeaderChecksumValid = false;
        const auto inspectMirror = [&](const std::size_t mirrorOffset) -> bool
        {
            if (mirrorOffset > bytes.size()
                || kHeaderMirrorSize > bytes.size() - mirrorOffset)
            {
                return false;
            }
            Reader mirror(bytes.subspan(mirrorOffset, kHeaderMirrorSize));
            if (!mirror.startsWith(kHeaderMirrorMagic)) return false;
            mirror.raw(kHeaderMirrorMagic.size());
            const std::uint32_t mirrorHeaderSize = mirror.u32();
            const auto candidate = mirror.raw(kCanonicalHeaderSize);
            const std::uint32_t expectedMirrorCrc = mirror.u32();
            return mirrorHeaderSize == kCanonicalHeaderSize
                && mirror.empty()
                && crc32c(candidate) == expectedMirrorCrc
                && hasValidCanonicalHeaderChecksum(candidate);
        };
        if (hasFooter && footerOffset >= kHeaderMirrorSize)
        {
            mirrorHeaderChecksumValid = inspectMirror(
                footerOffset - kHeaderMirrorSize);
        }
        else if (!hasFooter && bytes.size() >= kHeaderMirrorSize)
        {
            // The footer is redundant framing. Search the narrow final
            // mirror/footer window so a valid mirror remains usable when the
            // footer is damaged, truncated, or absent entirely.
            const std::size_t firstCandidate = bytes.size() - kHeaderMirrorSize;
            const std::size_t lastCandidate = bytes.size() >= kHeaderMirrorSize + kFooterSize
                ? bytes.size() - kHeaderMirrorSize - kFooterSize : 0u;
            for (std::size_t candidate = firstCandidate;; --candidate)
            {
                if (inspectMirror(candidate))
                {
                    mirrorHeaderChecksumValid = true;
                    break;
                }
                if (candidate == lastCandidate) break;
            }
        }
        if (primaryHeaderChecksumValid || mirrorHeaderChecksumValid)
        {
            return decodeSedBytes(bytes, true,
                SedSerializationProfile::ProtectedV1, false, &recoveryBudget,
                &semanticBudget);
        }
        // A surviving magic byte with a bad header CRC is not enough to trust
        // fixed framing. Fall through to the checked-section scanner so a
        // damaged header with a lost mirror/footer can still recover all
        // surviving Level sections.
    }

    if (!hasSedBinarySignature(bytes) && bytes.size() >= kHeaderMirrorSize)
    {
        // A damaged primary magic must not hide an otherwise valid canonical
        // mirror. Distinguish a full document from a headerless split stream
        // by requiring at least one checked trailer whose descriptor carries
        // the direct (already framed) canonical FEC phase.
        const auto validMirrorAt = [&](const std::size_t mirrorOffset) -> bool
        {
            if (mirrorOffset > bytes.size()
                || kHeaderMirrorSize > bytes.size() - mirrorOffset)
            {
                return false;
            }
            Reader mirror(bytes.subspan(mirrorOffset, kHeaderMirrorSize));
            if (!mirror.startsWith(kHeaderMirrorMagic)) return false;
            mirror.raw(kHeaderMirrorMagic.size());
            const std::uint32_t mirrorHeaderSize = mirror.u32();
            const auto candidate = mirror.raw(kCanonicalHeaderSize);
            const std::uint32_t expectedMirrorCrc = mirror.u32();
            return mirrorHeaderSize == kCanonicalHeaderSize && mirror.empty()
                && crc32c(candidate) == expectedMirrorCrc
                && hasValidCanonicalHeaderChecksum(candidate);
        };
        bool validMirror = false;
        const std::size_t firstCandidate = bytes.size() - kHeaderMirrorSize;
        const std::size_t lastCandidate = bytes.size() >= kHeaderMirrorSize + kFooterSize
            ? bytes.size() - kHeaderMirrorSize - kFooterSize : 0u;
        for (std::size_t candidate = firstCandidate;; --candidate)
        {
            if (validMirrorAt(candidate))
            {
                validMirror = true;
                break;
            }
            if (candidate == lastCandidate) break;
        }
        if (validMirror)
        {
            bool directTrailerEvidence = false;
            std::size_t mirrorSearch{};
            while (const auto foundMirror = findExactRecoveryMarker(bytes,
                kSectionMirrorMagic, mirrorSearch, bytes.size(), &recoveryBudget))
            {
                const std::size_t trailerOffset = *foundMirror;
                mirrorSearch = trailerOffset + 1u;
                if (kSectionTrailerSize > bytes.size() - trailerOffset) continue;
                const auto mirrored = readSectionTrailer(bytes, trailerOffset);
                if (!mirrored) continue;
                const auto descriptor = parseSectionDescriptor(*mirrored);
                if (!descriptor || descriptor->storedSize > trailerOffset
                    || trailerOffset - descriptor->storedSize < kSectionDescriptorSize)
                {
                    continue;
                }
                const std::size_t descriptorOffset = trailerOffset
                    - static_cast<std::size_t>(descriptor->storedSize)
                    - kSectionDescriptorSize;
                if (hasCanonicalFecAlignment(descriptorOffset, *descriptor))
                {
                    directTrailerEvidence = true;
                    break;
                }
            }
            if (directTrailerEvidence)
            {
                return decodeSedBytes(bytes, true,
                    SedSerializationProfile::ProtectedV1, false, &recoveryBudget,
                    &semanticBudget);
            }
        }
    }

    // splitSedFileSections() can produce a headerless semantic stream.  An
    // eight-byte anchor alone is not enough evidence: arbitrary outer bytes
    // may begin with that value and precede a complete embedded document.
    // Require an independently checked descriptor/trailer pair at the
    // canonical fixed-header-adjusted phase. The scan also preserves recovery when the
    // first section's descriptor and/or trailer was damaged but a later
    // protected section survived.
    const bool startsWithSectionAnchor = bytes.size() >= kSectionAnchorMagic.size()
        && std::equal(kSectionAnchorMagic.begin(), kSectionAnchorMagic.end(), bytes.begin());
    const auto firstCheckedHeaderlessSection = [&]() -> std::optional<std::size_t>
    {
        if (!startsWithSectionAnchor) return std::nullopt;
        std::size_t anchorSearch{};
        while (const auto foundAnchor = findExactRecoveryMarker(bytes,
            kSectionAnchorMagic, anchorSearch, bytes.size(), &recoveryBudget))
        {
            const std::size_t candidate = *foundAnchor;
            anchorSearch = candidate + 1u;
            if (kSectionHeaderSize + kSectionTrailerSize
                > bytes.size() - candidate)
            {
                continue;
            }
            const auto descriptor = parseSectionDescriptor(
                bytes.subspan(candidate, kSectionHeaderSize));
            if (!descriptor || (descriptor->flags & kParityProtected) == 0
                || !hasCanonicalFecAlignment(candidate, *descriptor,
                    kCanonicalHeaderSize)
                || descriptor->storedSize
                    > bytes.size() - candidate - kSectionHeaderSize
                || descriptor->storedSize + kSectionHeaderSize
                        + kSectionTrailerSize
                    > bytes.size() - candidate)
            {
                continue;
            }
            const std::size_t trailerOffset = candidate + kSectionHeaderSize
                + static_cast<std::size_t>(descriptor->storedSize);
            const auto trailer = readSectionTrailer(bytes, trailerOffset);
            if (trailer && std::equal(
                    bytes.begin() + static_cast<std::ptrdiff_t>(candidate),
                    bytes.begin() + static_cast<std::ptrdiff_t>(
                        candidate + kSectionHeaderSize),
                    trailer->begin(), trailer->end()))
            {
                return candidate;
            }
        }
        return std::nullopt;
    };
    const std::optional<std::size_t> headerlessEvidence =
        firstCheckedHeaderlessSection();
    const bool startsHeaderlessSection = headerlessEvidence.has_value();
    // A damaged primary magic byte is recovered by the checked-section
    // scanner below. Do not permissively decode an arbitrary prefix as a
    // fixed header: an outer recovery prefix followed by an intact native
    // document is handled by the validated embedded-document path above.

    std::size_t sectionStart{};
    if (!startsHeaderlessSection)
    {
        sectionStart = bytes.size();
        std::size_t anchorSearch{};
        while (const auto foundAnchor = findExactRecoveryMarker(bytes,
            kSectionAnchorMagic, anchorSearch, bytes.size(), &recoveryBudget))
        {
            const std::size_t candidate = *foundAnchor;
            anchorSearch = candidate + 1u;
            if (kSectionHeaderSize + kSectionTrailerSize
                > bytes.size() - candidate)
            {
                continue;
            }
            const auto descriptor = parseSectionDescriptor(
                bytes.subspan(candidate, kSectionHeaderSize));
            if (!descriptor || descriptor->storedSize > bytes.size() - candidate
                || descriptor->storedSize + kSectionHeaderSize + kSectionTrailerSize
                    > bytes.size() - candidate
                || (!hasCanonicalFecAlignment(candidate, *descriptor)
                    && !hasCanonicalFecAlignment(candidate, *descriptor,
                        kCanonicalHeaderSize)))
            {
                continue;
            }
            const std::size_t trailerOffset = candidate + kSectionHeaderSize
                + static_cast<std::size_t>(descriptor->storedSize);
            const auto trailer = readSectionTrailer(bytes, trailerOffset);
            if (!trailer
                || !std::equal(bytes.begin() + static_cast<std::ptrdiff_t>(candidate),
                    bytes.begin() + static_cast<std::ptrdiff_t>(candidate + kSectionHeaderSize),
                    trailer->begin(), trailer->end()))
            {
                continue;
            }
            const bool framedPhase = hasCanonicalFecAlignment(candidate, *descriptor);
            const bool headerlessPhase = hasCanonicalFecAlignment(candidate,
                *descriptor, kCanonicalHeaderSize);
            // Keep the damaged prefix when this is a headerless stream. The
            // synthesized canonical header is inserted in front of the
            // original bytes below, so discarding that prefix would shift a
            // later descriptor by its coded alignment phase.
            sectionStart = headerlessPhase && !framedPhase
                ? 0u
                : (framedPhase ? std::min<std::size_t>(kCanonicalHeaderSize, candidate)
                    : candidate);
            break;
        }
        if (sectionStart == bytes.size())
        {
            // If every primary descriptor is damaged, bootstrap framing from
            // the earliest independently checked trailer. Its mirrored
            // descriptor gives the exact preceding payload extent; the
            // canonical phase check rejects trailer-like payload bytes.
            std::size_t mirrorSearch{};
            while (const auto foundMirror = findExactRecoveryMarker(bytes,
                kSectionMirrorMagic, mirrorSearch, bytes.size(), &recoveryBudget))
            {
                const std::size_t trailerOffset = *foundMirror;
                mirrorSearch = trailerOffset + 1u;
                if (kSectionTrailerSize > bytes.size() - trailerOffset) continue;
                const auto trailer = readSectionTrailer(bytes, trailerOffset);
                if (!trailer) continue;
                const auto descriptor = parseSectionDescriptor(*trailer);
                if (!descriptor || descriptor->storedSize > trailerOffset
                    || trailerOffset - descriptor->storedSize < kSectionHeaderSize)
                {
                    continue;
                }
                const std::size_t candidate = trailerOffset
                    - static_cast<std::size_t>(descriptor->storedSize)
                    - kSectionHeaderSize;
                const bool framedPhase = hasCanonicalFecAlignment(candidate, *descriptor);
                const bool headerlessPhase = hasCanonicalFecAlignment(candidate,
                    *descriptor, kCanonicalHeaderSize);
                if (!framedPhase && !headerlessPhase) continue;
                sectionStart = headerlessPhase && !framedPhase
                    ? 0u
                    : (framedPhase ? std::min<std::size_t>(kCanonicalHeaderSize, candidate)
                        : candidate);
                break;
            }
        }
        if (sectionStart == bytes.size()) fail("no checked binary SED section anchor remains");
    }
    const auto sectionBytes = bytes.subspan(sectionStart);

    // A split recovery stream may retain the original fixed-header mirror at
    // its tail.  It is the only checked source of project/count framing once
    // the primary header has been removed, so use it verbatim.  Synthesizing
    // a JKDF2 header and then appending a valid MotS/IJIM mirror leaves two
    // individually valid but disagreeing copies, which the strict decoder
    // must (correctly) reject.  If the mirror is absent or damaged, create a
    // self-consistent JKDF2 pair; HEAD remains authoritative for the project
    // kind after semantic decoding in the permissive path.
    std::optional<Bytes> survivingMirrorHeader{};
    const bool hasTrailingMirrorAndFooter = sectionBytes.size()
        >= kFooterSize + kHeaderMirrorSize
        && std::equal(kFooterMagic.begin(), kFooterMagic.end(),
            sectionBytes.end() - static_cast<std::ptrdiff_t>(kFooterSize));
    if (hasTrailingMirrorAndFooter)
    {
        const std::size_t mirrorOffset = sectionBytes.size()
            - kFooterSize - kHeaderMirrorSize;
        Reader mirror(sectionBytes.subspan(mirrorOffset, kHeaderMirrorSize));
        if (mirror.startsWith(kHeaderMirrorMagic))
        {
            mirror.raw(kHeaderMirrorMagic.size());
            const std::uint32_t mirrorHeaderSize = mirror.u32();
            const auto candidate = mirror.raw(kCanonicalHeaderSize);
            const std::uint32_t expectedMirrorCrc = mirror.u32();
            if (mirrorHeaderSize == kCanonicalHeaderSize && mirror.empty()
                && crc32c(candidate) == expectedMirrorCrc
                && hasValidCanonicalHeaderChecksum(candidate))
            {
                survivingMirrorHeader.emplace(candidate.begin(), candidate.end());
            }
        }
    }

    // The independently protected fixed header may be the only framing lost.
    // Rebuild just enough framing to parse the remaining checked chunks; HEAD
    // remains authoritative for project type when it survived.
    std::size_t cursor{};
    std::uint32_t count{};
    std::optional<std::uint32_t> recoveredBinaryVersion{};
    const auto observeBinaryVersion = [&recoveredBinaryVersion](
        const std::uint32_t version)
    {
        if (recoveredBinaryVersion && *recoveredBinaryVersion != version)
        {
            fail("recoverable section descriptors disagree on file version");
        }
        recoveredBinaryVersion = version;
    };
    while (!survivingMirrorHeader && cursor + kSectionHeaderSize <= sectionBytes.size())
    {
        if (cursor + kHeaderMirrorMagic.size() <= sectionBytes.size()
            && std::equal(kHeaderMirrorMagic.begin(), kHeaderMirrorMagic.end(),
                sectionBytes.begin() + static_cast<std::ptrdiff_t>(cursor)))
        {
            break;
        }
        if (std::equal(kFooterMagic.begin(), kFooterMagic.end(),
                sectionBytes.begin() + static_cast<std::ptrdiff_t>(cursor)))
        {
            break;
        }
        const auto descriptor = parseSectionDescriptor(
            sectionBytes.subspan(cursor, kSectionHeaderSize));
        if (!descriptor)
        {
            if (hasValidSectionDescriptorChecksum(
                    sectionBytes.subspan(cursor, kSectionHeaderSize)))
            {
                fail("section descriptor schema is unsupported");
            }

            // A primary descriptor may be damaged while its independently
            // checked trailer survives.  Search only for a trailer whose
            // mirrored descriptor gives an exact payload boundary; this
            // preserves all later healthy sections without trusting arbitrary
            // payload bytes as framing.
            bool recoveredBoundary = false;
            std::size_t mirrorSearch = cursor + kSectionHeaderSize;
            while (const auto foundMirror = findExactRecoveryMarker(sectionBytes,
                kSectionMirrorMagic, mirrorSearch, sectionBytes.size(),
                &recoveryBudget))
            {
                const std::size_t candidate = *foundMirror;
                mirrorSearch = candidate + 1u;
                if (kSectionTrailerSize > sectionBytes.size() - candidate) continue;
                const auto trailer = readSectionTrailer(sectionBytes, candidate);
                if (!trailer) continue;
                const auto candidateDescriptor = parseSectionDescriptor(*trailer);
                if (!candidateDescriptor)
                {
                    if (hasValidSectionDescriptorChecksum(
                            std::span<const std::byte>(*trailer)))
                    {
                        fail("section descriptor mirror schema is unsupported");
                    }
                    continue;
                }
                if (!hasCanonicalFecAlignment(cursor, *candidateDescriptor,
                        kCanonicalHeaderSize)
                    || candidateDescriptor->storedSize > sectionBytes.size()
                        - (cursor + kSectionHeaderSize)
                    || candidate != cursor + kSectionHeaderSize
                        + static_cast<std::size_t>(candidateDescriptor->storedSize))
                {
                    continue;
                }
                observeBinaryVersion(candidateDescriptor->version);
                cursor = candidate + kSectionTrailerSize;
                if (count >= 1024u)
                {
                    fail("section count is outside the safe range");
                }
                ++count;
                recoveredBoundary = true;
                break;
            }
            if (!recoveredBoundary)
            {
                // If both copies of the current descriptor/trailer are gone,
                // use the next independently checked descriptor as a
                // resynchronization point. The candidate's own trailer and
                // absolute FEC alignment are validated below, so no bytes
                // are removed or shifted and 4 KiB coding offsets remain
                // meaningful for the later section.
                std::size_t anchorSearch = cursor + 1u;
                while (const auto foundAnchor = findExactRecoveryMarker(sectionBytes,
                    kSectionAnchorMagic, anchorSearch, sectionBytes.size(),
                    &recoveryBudget))
                {
                    const std::size_t candidate = *foundAnchor;
                    anchorSearch = candidate + 1u;
                    if (kSectionHeaderSize + kSectionTrailerSize
                        > sectionBytes.size() - candidate)
                    {
                        continue;
                    }
                    const auto candidateDescriptor = parseSectionDescriptor(
                        sectionBytes.subspan(candidate, kSectionHeaderSize));
                    if (!candidateDescriptor
                        || !hasCanonicalFecAlignment(candidate, *candidateDescriptor,
                            kCanonicalHeaderSize)
                        || candidateDescriptor->storedSize
                            > sectionBytes.size() - candidate - kSectionHeaderSize
                            || candidateDescriptor->storedSize
                                + kSectionHeaderSize + kSectionTrailerSize
                                > sectionBytes.size() - candidate)
                    {
                        continue;
                    }
                    const std::size_t candidateTrailer = candidate
                        + kSectionHeaderSize
                        + static_cast<std::size_t>(candidateDescriptor->storedSize);
                    const auto trailer = readSectionTrailer(sectionBytes,
                        candidateTrailer);
                    if (!trailer
                        || !std::equal(sectionBytes.begin()
                                + static_cast<std::ptrdiff_t>(candidate),
                            sectionBytes.begin() + static_cast<std::ptrdiff_t>(candidate
                                + kSectionHeaderSize),
                            trailer->begin(), trailer->end()))
                    {
                        continue;
                    }
                    observeBinaryVersion(candidateDescriptor->version);
                    cursor = candidate;
                    if (count >= 1024u)
                    {
                        fail("section count is outside the safe range");
                    }
                    ++count; // account for the lost current semantic unit
                    recoveredBoundary = true;
                    break;
                }
            }
            if (!recoveredBoundary) break;
            continue;
        }
        if (descriptor->storedSize > sectionBytes.size() - cursor
            || descriptor->storedSize + kSectionHeaderSize + kSectionTrailerSize
                > sectionBytes.size() - cursor)
        {
            break;
        }
        observeBinaryVersion(descriptor->version);
        cursor += kSectionHeaderSize + static_cast<std::size_t>(descriptor->storedSize)
            + kSectionTrailerSize;
        if (count >= 1024u)
        {
            fail("section count is outside the safe range");
        }
        ++count;
    }
    if (!survivingMirrorHeader && count == 0)
    {
        fail("no recoverable binary SED sections remain");
    }

    const std::size_t copiedSectionBytes = survivingMirrorHeader
        ? sectionBytes.size() : cursor;
    const std::optional<std::size_t> finalFramedSize =
        checkedRecoveryFramingSize(copiedSectionBytes,
            survivingMirrorHeader.has_value());
    if (!finalFramedSize)
    {
        fail("reconstructed framing exceeds the safe file limit");
    }

    // Preflight before allocating or copying any large section span. Reserve
    // the exact final size so adding the synthesized mirror/footer cannot
    // trigger a second near-file-sized allocation and copy.
    Bytes framed{};
    framed.reserve(*finalFramedSize);
    if (survivingMirrorHeader)
    {
        framed.insert(framed.end(), survivingMirrorHeader->begin(),
            survivingMirrorHeader->end());
    }
    else
    {
        Writer header{};
        header.raw(kMagic);
        header.u32(recoveredBinaryVersion.value_or(
            SedLegacyBinaryVersion));
        header.i32(static_cast<std::int32_t>(ProjectType::JKDF2));
        header.u32(count);
        header.i64(0);
        header.u32(0);
        const Bytes headerBytes = header.take();
        framed.insert(framed.end(), headerBytes.begin(), headerBytes.end());
        replaceU32(framed, kHeaderChecksumOffset,
            crc32c(std::span<const std::byte>(framed).first(kHeaderChecksumOffset)));
    }

    if (survivingMirrorHeader)
    {
        // Keep the checked mirror/footer and every section byte exactly as
        // supplied.  The reconstructed primary above is byte-identical to
        // the surviving mirror header, so strict fixed-header agreement is
        // restored without rewriting any payload.
        framed.insert(framed.end(), sectionBytes.begin(), sectionBytes.end());
    }
    else
    {
        framed.insert(framed.end(), sectionBytes.begin(),
            sectionBytes.begin() + static_cast<std::ptrdiff_t>(cursor));
        Writer mirror{};
        mirror.raw(kHeaderMirrorMagic);
        mirror.u32(kCanonicalHeaderSize);
        mirror.raw(std::span<const std::byte>(framed).first(kCanonicalHeaderSize));
        mirror.u32(crc32c(std::span<const std::byte>(framed).first(kCanonicalHeaderSize)));
        const Bytes mirrorBytes = mirror.take();
        framed.insert(framed.end(), mirrorBytes.begin(), mirrorBytes.end());
        framed.insert(framed.end(), kFooterMagic.begin(), kFooterMagic.end());
    }
    if (framed.size() != *finalFramedSize)
    {
        throw std::logic_error("Binary SED recovery framing size drifted.");
    }
    SedRecoveryLoadResult recovered = decodeSedBytes(
        framed, true, SedSerializationProfile::ProtectedV1,
        !survivingMirrorHeader.has_value(), &recoveryBudget, &semanticBudget);
    recovered.hadCorruption = true;
    return recovered;
}

namespace
{

struct ResilientBytes final
{
    Bytes bytes{};
    std::vector<SedUnreadableRange> unreadableRanges{};
};

ResilientBytes readResilientBytes(const std::uint64_t fileSize,
    const SedRangeReader& reader, const SedResilientReadOptions options)
{
    if (!reader) throw std::invalid_argument("Binary SED range reader is empty.");
    if (fileSize == 0 || fileSize > kMaxFileBytes
        || fileSize > std::numeric_limits<std::size_t>::max())
    {
        fail("resilient input size is outside the safe range");
    }
    if (options.blockBytes == 0 || options.blockBytes > 1024u * 1024u
        || options.maximumAttempts == 0 || options.maximumAttempts > 100u)
    {
        throw std::invalid_argument("Binary SED resilient-read options are out of range.");
    }

    ResilientBytes result{};
    result.bytes.resize(static_cast<std::size_t>(fileSize), std::byte{0});
    for (std::uint64_t offset = 0; offset < fileSize;)
    {
        const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(
            options.blockBytes, fileSize - offset));
        auto destination = std::span<std::byte>(result.bytes).subspan(
            static_cast<std::size_t>(offset), count);
        bool readComplete = false;
        for (std::uint32_t attempt = 0; attempt < options.maximumAttempts; ++attempt)
        {
            std::fill(destination.begin(), destination.end(), std::byte{0});
            if (reader(offset, destination))
            {
                readComplete = true;
                break;
            }
        }
        if (!readComplete)
        {
            std::fill(destination.begin(), destination.end(), std::byte{0});
            if (!result.unreadableRanges.empty()
                && result.unreadableRanges.back().offset
                        + result.unreadableRanges.back().length == offset)
            {
                result.unreadableRanges.back().length += count;
            }
            else
            {
                result.unreadableRanges.push_back(SedUnreadableRange{ offset, count });
            }
        }
        offset += count;
    }
    return result;
}

} // namespace

std::vector<detail::SedHistorySectionRecordSpan>
detail::splitHistoryNestedSectorFrameRecords(
    const std::span<const std::byte> frame,
    const std::size_t maximumRecords)
{
    if (frame.size() < kSectionHeaderSize + kSectionTrailerSize)
    {
        throw std::invalid_argument(
            "HistoryNested SECT frame is shorter than its checked framing.");
    }
    const auto descriptor = parseSectionDescriptor(frame.first(kSectionHeaderSize));
    if (!descriptor
        || descriptor->type != std::array<char, 4>{ 's', 'e', 'c', 't' }
        || descriptor->flags != kHistoryNestedChecksum
        || descriptor->rawSize != descriptor->storedSize
        || descriptor->storedSize != frame.size()
            - kSectionHeaderSize - kSectionTrailerSize)
    {
        throw std::invalid_argument(
            "HistoryNested SECT frame has invalid checked framing.");
    }

    const auto payload = frame.subspan(
        kSectionHeaderSize, static_cast<std::size_t>(descriptor->storedSize));
    Reader reader{ payload };
    const std::uint32_t count = reader.u32();
    if (count > kMaxCollectionItems)
    {
        throw std::runtime_error(
            "HistoryNested SECT sector count exceeds the safe item limit.");
    }
    // The app-side HIST writer has a bounded fragment budget.  Refuse a
    // larger record count before reserving or materializing the record-span
    // vector; callers that need the complete list can retain the default
    // unlimited bound, while bounded callers fail closed without a large
    // temporary allocation.
    if (static_cast<std::size_t>(count) > maximumRecords)
    {
        throw std::length_error(
            "HistoryNested SECT record count exceeds the requested bound.");
    }
    std::vector<detail::SedHistorySectionRecordSpan> result{};
    result.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const std::size_t begin = kSectionHeaderSize + reader.position();
        skipHistorySectorRecord(reader);
        const std::size_t end = kSectionHeaderSize + reader.position();
        if (end <= begin || end > frame.size() - kSectionTrailerSize)
        {
            throw std::runtime_error(
                "HistoryNested SECT record boundary is outside its frame.");
        }
        result.push_back(detail::SedHistorySectionRecordSpan{ begin, end });
    }
    if (!reader.empty())
    {
        throw std::runtime_error(
            "HistoryNested SECT frame has trailing payload bytes.");
    }
    return result;
}

SedFile loadSedFileResilient(const std::uint64_t fileSize,
    const SedRangeReader& reader, const SedResilientReadOptions options)
{
    ResilientBytes input = readResilientBytes(fileSize, reader, options);
    SedFile result = loadSedFileBytes(input.bytes);
    result.unreadableRanges = std::move(input.unreadableRanges);
    result.hadCorruption = result.hadCorruption || !result.unreadableRanges.empty();
    return result;
}

SedRecoveryLoadResult recoverSedFileResilient(const std::uint64_t fileSize,
    const SedRangeReader& reader, const SedResilientReadOptions options)
{
    ResilientBytes input = readResilientBytes(fileSize, reader, options);
    SedRecoveryLoadResult result = recoverFromSedBytes(input.bytes);
    result.unreadableRanges = std::move(input.unreadableRanges);
    result.hadCorruption = result.hadCorruption || !result.unreadableRanges.empty();
    return result;
}

std::optional<SedEmbeddedDocument> findEmbeddedSedDocument(
    const std::span<const std::byte> bytes, const std::size_t searchOffset,
    const SedSerializationProfile profile)
{
    requireValidSerializationProfile(profile);
    if (bytes.size() > kMaxFileBytes)
    {
        fail("embedded-document search exceeds the safe file limit");
    }
    RecoveryWorkBudget recoveryBudget{ bytes.size() };
    SemanticDecodeBudget semanticBudget{};
    const std::optional<DecodedEmbeddedDocument> found =
        findEmbeddedSedDocumentDecoded(bytes, searchOffset, profile,
            recoveryBudget, semanticBudget);
    return found ? std::optional<SedEmbeddedDocument>{ found->document }
                 : std::nullopt;
}

static std::vector<std::vector<std::byte>> splitSedFileSectionsImpl(
    const std::span<const std::byte> bytes,
    const SedSerializationProfile profile,
    const bool validateSemantics)
{
    requireValidSerializationProfile(profile);
    // This helper has a public writer-side variant which intentionally skips
    // the full semantic decode.  Keep its framing arithmetic just as hostile
    // input-safe as the validated path: in particular, do not subtract the
    // fixed footer size or reserve the header-declared section count until
    // both have been bounded.  A short span beginning with `sed\0` used to
    // underflow `footerOffset`, and a forged u32 count could request a
    // multi-gigabyte vector before the first descriptor was inspected.
    if (bytes.size() > kMaxFileBytes)
    {
        fail("file exceeds the maximum supported size while splitting");
    }
    if (validateSemantics)
    {
        // Untrusted input must validate every checksum and semantic section
        // before exposing boundaries to corruption-recovery tooling.
        (void)decodeSedBytes(bytes, false, profile);
    }
    Bytes recoveredHeader{};
    const std::optional<std::size_t> magicSize = sedMagicSize(bytes);
    if (!magicSize)
    {
        if (bytes.size() < kFooterSize + kHeaderMirrorSize
            || !std::equal(kFooterMagic.begin(), kFooterMagic.end(),
                bytes.end() - static_cast<std::ptrdiff_t>(kFooterSize)))
        {
            fail("signature mismatch while splitting");
        }
        const std::size_t mirrorOffset = bytes.size() - kFooterSize - kHeaderMirrorSize;
        Reader mirror(bytes.subspan(mirrorOffset, kHeaderMirrorSize));
        if (!mirror.startsWith(kHeaderMirrorMagic)) fail("header mirror mismatch while splitting");
        mirror.raw(kHeaderMirrorMagic.size());
        if (mirror.u32() != kCanonicalHeaderSize) fail("header mirror size mismatch while splitting");
        const auto candidate = mirror.raw(kCanonicalHeaderSize);
        const std::uint32_t expected = mirror.u32();
        if (!mirror.empty() || crc32c(candidate) != expected
            || !hasValidCanonicalHeaderChecksum(candidate))
        {
            fail("header mirror checksum mismatch while splitting");
        }
        recoveredHeader.assign(candidate.begin(), candidate.end());
    }
    constexpr std::size_t kMinimumFramedSplitBytes =
        kCanonicalHeaderSize + kHeaderMirrorSize + kFooterSize;
    if (bytes.size() < kMinimumFramedSplitBytes)
    {
        fail("file is too short while splitting sections");
    }
    // When the primary fixed header is missing but the checked trailing
    // mirror survives, `bytes` begins directly with the first section. A
    // damaged-but-present primary header, however, still occupies those 28
    // bytes. Distinguish the two forms by their checked section anchor rather
    // than assuming that every non-magic span is headerless.
    const bool sectionAtHeader = bytes.size() >= kCanonicalHeaderSize
            + kSectionAnchorMagic.size()
        && std::equal(kSectionAnchorMagic.begin(), kSectionAnchorMagic.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(kCanonicalHeaderSize));
    const bool sectionAtStart = std::equal(kSectionAnchorMagic.begin(),
        kSectionAnchorMagic.end(), bytes.begin());
    if (!recoveredHeader.empty() && !sectionAtHeader && !sectionAtStart)
    {
        fail("section anchor is missing while splitting");
    }
    const std::size_t headerSize = recoveredHeader.empty()
        ? kCanonicalHeaderSize : (sectionAtHeader ? kCanonicalHeaderSize : 0u);
    const auto header = recoveredHeader.empty()
        ? bytes.first(kCanonicalHeaderSize)
        : std::span<const std::byte>(recoveredHeader);
    const std::uint32_t binaryVersion = readU32At(
        header, kHeaderVersionOffset);
    if (!isSupportedSedBinaryVersion(binaryVersion))
    {
        fail("unsupported format version while splitting");
    }
    const std::uint32_t count = readU32At(header, kHeaderSectionCountOffset);
    if (count == 0 || count > 1024u)
    {
        fail("section count is outside the safe range while splitting");
    }
    const std::size_t footerOffset = bytes.size() - kFooterSize;
    if (footerOffset < kHeaderMirrorSize) fail("missing fixed-header mirror while splitting");
    const std::size_t bodyEnd = footerOffset - kHeaderMirrorSize;
    std::vector<std::vector<std::byte>> result{};
    result.reserve(static_cast<std::size_t>(count) + 1);
    if (recoveredHeader.empty())
    {
        result.emplace_back(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(headerSize));
    }
    else
    {
        result.push_back(recoveredHeader);
    }
    std::size_t cursor = headerSize;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        if (cursor > bodyEnd || kSectionHeaderSize > bodyEnd - cursor)
        {
            fail("truncated section while splitting");
        }
        const auto descriptor = parseSectionDescriptor(
            bytes.subspan(cursor, kSectionHeaderSize));
        if (!descriptor || descriptor->version != binaryVersion
            || descriptor->storedSize > bodyEnd - cursor
            || descriptor->storedSize
                > bodyEnd - cursor - kSectionHeaderSize
            || kSectionTrailerSize
                > bodyEnd - cursor - kSectionHeaderSize
                    - static_cast<std::size_t>(descriptor->storedSize))
        {
            fail("invalid section size while splitting");
        }
        const std::size_t end = cursor + kSectionHeaderSize
            + static_cast<std::size_t>(descriptor->storedSize) + kSectionTrailerSize;
        result.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
                            bytes.begin() + static_cast<std::ptrdiff_t>(end));
        cursor = end;
    }
    if (cursor != bodyEnd || result.empty())
    {
        fail("unexpected bytes while splitting sections");
    }
    result.back().insert(result.back().end(),
        bytes.begin() + static_cast<std::ptrdiff_t>(bodyEnd), bytes.end());
    return result;
}

std::vector<std::vector<std::byte>> splitSedFileSections(
    const std::span<const std::byte> bytes,
    const SedSerializationProfile profile)
{
    return splitSedFileSectionsImpl(bytes, profile, true);
}

std::vector<std::vector<std::byte>> splitGeneratedSedFileSections(
    const std::span<const std::byte> bytes,
    const SedSerializationProfile profile)
{
    return splitSedFileSectionsImpl(bytes, profile, false);
}

void saveToSED(const Level& level, std::ostream& out)
{
    saveToSED(level, {}, out);
}

void saveToSED(
    const Level& level,
    const std::span<const SedOptionalSection> optionalSections,
    std::ostream& out)
{
    saveToSED(level, optionalSections, {}, out);
}

void saveToSED(
    const Level& level,
    const std::span<const SedOptionalSection> optionalSections,
    const std::int64_t timestamp,
    std::ostream& out)
{
    const Bytes bytes = saveToSedBytes(level, optionalSections,
        SedSerializationProfile::ProtectedV1, timestamp);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("Failed to write binary SED file.");
}

void saveToSED(const Level& level, const std::filesystem::path& path)
{
    const Bytes bytes = saveToSedBytes(level);
    detail::StagedAssetFile staged(path);
    staged.writeAndClose(bytes);
    staged.publish(path);
}

Level loadFromSED(std::istream& in)
{
    return loadSedFile(in).level;
}

SedFile loadSedFile(std::istream& in)
{
    const Bytes chars = readStreamBounded(in);
    const auto bytes = std::span<const std::byte>(chars);
    return loadSedFileBytes(bytes);
}

Level loadFromSED(const std::filesystem::path& path)
{
    return loadSedFile(path).level;
}

SedFile loadSedFile(const std::filesystem::path& path)
{
    std::error_code sizeError{};
    const std::uint64_t fileSize = std::filesystem::file_size(path, sizeError);
    if (sizeError) throw std::runtime_error("Could not determine binary SED file size.");
    if (!isAdmittedSedFileSize(fileSize))
    {
        fail("file size is outside the safe range");
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Could not open binary SED file for reading.");
    Bytes bytes = readStreamBounded(in, fileSize);
    if (bytes.size() == fileSize)
    {
        // A content/format error after a complete read is intentionally not
        // retried through the erasure path.
        return loadSedFileBytes(bytes);
    }

    in.close();
    // readStreamBounded allocates the reported extent for the common valid
    // path. If the stream raced/truncated, release that partial buffer before
    // the resilient reader allocates its own full-size destination.
    Bytes{}.swap(bytes);
    std::ifstream ranges(path, std::ios::binary);
    if (!ranges) throw std::runtime_error("Could not reopen binary SED file for recovery.");
    const SedRangeReader reader = [&](const std::uint64_t offset,
        const std::span<std::byte> destination)
    {
        ranges.clear();
        ranges.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!ranges) return false;
        ranges.read(reinterpret_cast<char*>(destination.data()),
            static_cast<std::streamsize>(destination.size()));
        return ranges.gcount() == static_cast<std::streamsize>(destination.size());
    };
    return loadSedFileResilient(fileSize, reader);
}

SedRecoveryLoadResult recoverSedFile(const std::filesystem::path& path)
{
    std::error_code sizeError{};
    const std::uint64_t fileSize = std::filesystem::file_size(path, sizeError);
    if (sizeError) throw std::runtime_error("Could not determine binary SED file size.");
    if (!isAdmittedSedFileSize(fileSize))
    {
        fail("file size is outside the safe range");
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Could not open binary SED file for recovery.");
    Bytes bytes = readStreamBounded(in, fileSize);
    if (bytes.size() == fileSize)
    {
        return recoverFromSedBytes(bytes);
    }

    in.close();
    // Avoid retaining the initial exact-size buffer while the resilient pass
    // allocates its independently checked full-file image.
    Bytes{}.swap(bytes);
    std::ifstream ranges(path, std::ios::binary);
    if (!ranges) throw std::runtime_error("Could not reopen binary SED file for recovery.");
    const SedRangeReader reader = [&](const std::uint64_t offset,
        const std::span<std::byte> destination)
    {
        ranges.clear();
        ranges.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!ranges) return false;
        ranges.read(reinterpret_cast<char*>(destination.data()),
            static_cast<std::streamsize>(destination.size()));
        return ranges.gcount() == static_cast<std::streamsize>(destination.size());
    };
    return recoverSedFileResilient(fileSize, reader);
}

ProjectType readSedProjectType(const std::filesystem::path& path)
{
    std::error_code sizeError{};
    const std::uint64_t fileSize = std::filesystem::file_size(path, sizeError);
    if (sizeError) throw std::runtime_error("Could not determine binary SED file size.");
    if (!isAdmittedSedFileSize(fileSize))
    {
        fail("file size is outside the safe range");
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Could not open binary SED file for reading.");
    std::array<std::byte, kCanonicalHeaderSize> header{};
    in.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (in.gcount() != static_cast<std::streamsize>(header.size()))
    {
        fail("invalid fixed header");
    }
    if (!sedMagicSize(header)
        || !isSupportedSedBinaryVersion(
            readU32At(header, kHeaderVersionOffset))
        || crc32c(std::span<const std::byte>(header).first(kHeaderChecksumOffset))
            != readU32At(header, kHeaderChecksumOffset))
    {
        // A damaged primary header can still be reconstructed from the
        // checked mirror by the full V1 loader.  Keep this probe useful for
        // startup while retaining strict errors for malformed files.
        in.clear();
        in.seekg(0, std::ios::beg);
        const Bytes chars = readStreamBounded(in);
        try
        {
            return loadSedFileBytes(std::span<const std::byte>(chars)).level.kind;
        }
        catch (const std::runtime_error&)
        {
            fail("invalid fixed header");
        }
    }
    const ProjectType kind = static_cast<ProjectType>(
        std::bit_cast<std::int32_t>(readU32At(header, kHeaderProjectOffset)));
    if (kind != ProjectType::JKDF2 && kind != ProjectType::MOTS && kind != ProjectType::IJIM)
    {
        fail("unknown header project type");
    }
    return kind;
}

std::uint32_t readSedBinaryVersion(const std::filesystem::path& path)
{
    std::error_code sizeError{};
    const std::uint64_t fileSize = std::filesystem::file_size(path, sizeError);
    if (sizeError)
    {
        throw std::runtime_error("Could not determine binary SED file size.");
    }
    if (!isAdmittedSedFileSize(fileSize))
    {
        fail("file size is outside the safe range");
    }
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("Could not open binary SED file for reading.");
    }
    std::array<std::byte, kCanonicalHeaderSize> header{};
    in.read(reinterpret_cast<char*>(header.data()),
        static_cast<std::streamsize>(header.size()));
    if (in.gcount() != static_cast<std::streamsize>(header.size()))
    {
        fail("invalid fixed header");
    }

    const std::uint32_t version = readU32At(header, kHeaderVersionOffset);
    if (!sedMagicSize(header)
        || !isSupportedSedBinaryVersion(version)
        || crc32c(std::span<const std::byte>(header).first(
                kHeaderChecksumOffset))
            != readU32At(header, kHeaderChecksumOffset))
    {
        in.clear();
        in.seekg(0, std::ios::beg);
        const Bytes chars = readStreamBounded(in);
        try
        {
            const SedFile file = loadSedFileBytes(
                std::span<const std::byte>(chars));
            return binaryVersionForLevel(file.level);
        }
        catch (const std::runtime_error&)
        {
            fail("invalid fixed header");
        }
    }
    return version;
}

} // namespace sed::assets
