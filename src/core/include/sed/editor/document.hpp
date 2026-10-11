#pragma once

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "sed/assets/sed.hpp"
#include "sed/editor/selection.hpp"
#include "sed/editor/undo.hpp"
#include "sed/level.hpp"

namespace sed::editor
{

class DocumentUndoHistory;

enum class DocumentDirty : std::uint32_t
{
    None = 0,
    LevelGeometry = 1u << 0,
    Things = 1u << 1,
    Lights = 1u << 2,
    Cogs = 1u << 3,
    Structure = 1u << 4,
    All = 0xFFFF'FFFFu,
};

constexpr DocumentDirty operator|(const DocumentDirty a, const DocumentDirty b) noexcept
{
    return static_cast<DocumentDirty>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

constexpr DocumentDirty operator&(const DocumentDirty a, const DocumentDirty b) noexcept
{
    return static_cast<DocumentDirty>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

constexpr DocumentDirty& operator|=(DocumentDirty& a, const DocumentDirty b) noexcept
{
    a = (a | b);
    return a;
}

constexpr bool hasAny(const DocumentDirty a, const DocumentDirty b) noexcept
{
    return (static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b)) != 0u;
}

struct DocumentState
{
    sed::Level level{};
    sed::editor::Selection selection{};
};

// One live GUI timeline spans both sides of the current cursor. Persistence
// deliberately has a separate, larger admission limit so the codec can inspect
// and round-trip files produced by tools with deeper histories without letting
// the interactive editor retain an unbounded undo stack.
inline constexpr std::size_t DocumentMaximumHistoryEntries = 1024;

// Portable full-state DTO used only at persistence boundaries and after
// decoding. The live undo stack remains compact (Thing/light subsets and
// baked-light deltas). Writers normally use visitRecoveryHistory() so states
// are expanded one at a time into an ABI-independent semantic-section pool
// rather than retaining or storing one complete Level per entry.
struct DocumentRecoveryHistoryEntry
{
    std::string label{};
    std::uint32_t dirtyFlags{};
    DocumentState state{};
    // Exact post-operation target used only by History navigation.  This is
    // persisted independently from the historical state's own selection
    // because structural commands can create a new Sector/Surface and the
    // user can change the live selection before saving.
    std::optional<sed::editor::Selection> navigationSelection{};
};

struct DocumentRecoveryHistory
{
    // Both vectors are nearest operation first: undoEntries.front() is the
    // state reached by the next Undo, redoEntries.front() by the next Redo.
    std::vector<DocumentRecoveryHistoryEntry> undoEntries{};
    std::vector<DocumentRecoveryHistoryEntry> redoEntries{};

    // Native readers rebuild the private compact undo representation while
    // decoding one historical Level at a time.  Keeping this opaque here
    // avoids retaining a second full Level for every Thing/light-only entry;
    // the portable vectors remain available to tests and external importers.
    std::shared_ptr<DocumentUndoHistory> compact{};

    [[nodiscard]] std::size_t undoEntryCount() const noexcept;
    [[nodiscard]] std::size_t redoEntryCount() const noexcept;
};

/**
 * One concrete transition in the live undo/redo timeline.
 *
 * The GUI uses this view for history navigation.  Selections are read from
 * the private compact snapshot owned by DocumentUndoHistory; structural
 * apply-records may additionally retain their exact post-operation selection
 * after a command replaces its sources with new topology.  Subset snapshots
 * publish only their affected indices.  No full Level is copied and no
 * descriptive label parsing is involved.  The target depth is the state depth
 * after this operation (the same depth used by the History dock).
 */
struct DocumentHistoryTransitionState
{
    std::string label{};
    std::uint32_t dirtyFlags{};
    sed::editor::Selection beforeSelection{};
    sed::editor::Selection afterSelection{};

    // Compact subset snapshots retain the concrete records touched by the
    // operation.  Expose only their indices (never the stored Thing/Sector
    // values) so UI navigation can find a changed entity even when selection
    // metadata is empty or stale.  Full-domain snapshots leave these empty
    // and the GUI falls back to before/after selection metadata.
    std::vector<std::int32_t> affectedThingIndices{};
    std::vector<std::int32_t> affectedSectorIndices{};
};

/** ID-only identity for one concrete position in the Undo/Redo timeline.
 *
 * Unlike DocumentHistoryTransitionState, this query does not copy selection
 * metadata or inspect a snapshot payload. Persistence uses it to bind small
 * editor sidecars to an exact record and to both adjacent timeline prefixes.
 */
struct DocumentHistoryTransitionIdentity
{
    std::uint64_t recordId{};
    std::uint64_t beforeUndoSignature{};
    std::uint64_t afterUndoSignature{};
};

/** Counts the concrete snapshot representations retained by a live undo stack.
 *
 * This is intentionally a small diagnostic DTO.  It lets persistence and
 * regression tests prove that a transform uses a bounded subset snapshot
 * instead of inferring memory behaviour from process RSS.
 */
struct DocumentUndoSnapshotStats
{
    std::size_t full{};
    std::size_t things{};
    std::size_t lights{};
    std::size_t cogs{};
    std::size_t thingsLights{};
    std::size_t thingSubsets{};
    std::size_t thingTails{};
    std::size_t sectorSubsets{};
    std::size_t surfaceSubsets{};
};

enum class DocumentRecoveryHistoryStack : std::uint8_t
{
    Undo,
    Redo,
};

/** Required native-Level sections changed by one concrete undo transition.
 *
 * Persistence derives this mask from the private Snapshot variant retained by
 * the record, never from its descriptive dirty flags.  The latter are part of
 * the public undo contract and can conservatively name a broader/narrower UI
 * domain than the bytes actually swapped by a compact record.
 */
enum class DocumentHistoryTransitionDomain : std::uint16_t
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

constexpr DocumentHistoryTransitionDomain operator|(
    const DocumentHistoryTransitionDomain lhs,
    const DocumentHistoryTransitionDomain rhs) noexcept
{
    return static_cast<DocumentHistoryTransitionDomain>(
        static_cast<std::uint16_t>(lhs) | static_cast<std::uint16_t>(rhs));
}

constexpr DocumentHistoryTransitionDomain& operator|=(
    DocumentHistoryTransitionDomain& lhs,
    const DocumentHistoryTransitionDomain rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool hasAny(
    const DocumentHistoryTransitionDomain lhs,
    const DocumentHistoryTransitionDomain rhs) noexcept
{
    return (static_cast<std::uint16_t>(lhs)
        & static_cast<std::uint16_t>(rhs)) != 0u;
}

class DocumentUndoHistory
{
public:
    DocumentUndoHistory() = default;
    explicit DocumentUndoHistory(DocumentState initial) : current_(std::move(initial))
    {
    }

    const DocumentState& current() const noexcept
    {
        return current_;
    }

    DocumentState& current() noexcept
    {
        return current_;
    }

    void clear(DocumentState initial = {})
    {
        current_ = std::move(initial);
        undo_.clear();
        redo_.clear();
        nextRecordId_ = 1;
    }

    bool canUndo() const noexcept
    {
        return !undo_.empty();
    }

    bool canRedo() const noexcept
    {
        return !redo_.empty();
    }

    std::size_t undoDepth() const noexcept
    {
        return undo_.size();
    }

    std::size_t redoDepth() const noexcept
    {
        return redo_.size();
    }

    std::uint64_t undoSignature() const noexcept
    {
        std::uint64_t h = 1469598103934665603ull; // FNV-1a 64-bit offset basis
        const auto mixByte = [&h](const std::uint8_t b) noexcept
        {
            h ^= static_cast<std::uint64_t>(b);
            h *= 1099511628211ull; // FNV prime
        };

        const std::uint64_t count = static_cast<std::uint64_t>(undo_.size());
        for (int i = 0; i < 8; ++i)
        {
            mixByte(static_cast<std::uint8_t>((count >> (i * 8)) & 0xFFu));
        }

        for (const Record& rec : undo_)
        {
            const std::uint64_t id = rec.id;
            for (int i = 0; i < 8; ++i)
            {
                mixByte(static_cast<std::uint8_t>((id >> (i * 8)) & 0xFFu));
            }
        }

        return h;
    }

    /**
     * Returns a stable identity for the redo side of the live timeline.
     *
     * Save-state tracking intentionally uses only undoSignature(): reaching the
     * saved undo position must still make the document clean. Recovery
     * publication has a stricter requirement and combines both signatures so
     * an Undo back to an already-published Level cannot silently lose the new
     * redo branch from the next crash checkpoint.
     */
    std::uint64_t redoSignature() const noexcept
    {
        std::uint64_t h = 1469598103934665603ull; // FNV-1a 64-bit offset basis
        const auto mixByte = [&h](const std::uint8_t b) noexcept
        {
            h ^= static_cast<std::uint64_t>(b);
            h *= 1099511628211ull; // FNV prime
        };

        const std::uint64_t count = static_cast<std::uint64_t>(redo_.size());
        for (int i = 0; i < 8; ++i)
        {
            mixByte(static_cast<std::uint8_t>((count >> (i * 8)) & 0xFFu));
        }

        for (const Record& rec : redo_)
        {
            const std::uint64_t id = rec.id;
            for (int i = 0; i < 8; ++i)
            {
                mixByte(static_cast<std::uint8_t>((id >> (i * 8)) & 0xFFu));
            }
        }

        return h;
    }

    [[nodiscard]] std::optional<std::uint64_t> nextUndoRecordId() const noexcept
    {
        return undo_.empty()
            ? std::nullopt
            : std::optional<std::uint64_t>{undo_.back().id};
    }

    [[nodiscard]] std::optional<std::uint64_t> nextRedoRecordId() const noexcept
    {
        return redo_.empty()
            ? std::nullopt
            : std::optional<std::uint64_t>{redo_.back().id};
    }

    [[nodiscard]] std::optional<std::uint64_t> historyRecordIdAtDepth(
        const std::size_t targetDepth) const noexcept
    {
        const std::size_t currentDepth = undo_.size();
        const std::size_t totalDepth = currentDepth + redo_.size();
        if (targetDepth == 0 || targetDepth > totalDepth)
        {
            return std::nullopt;
        }
        const std::size_t timelineIndex = targetDepth - 1u;
        if (timelineIndex < currentDepth)
        {
            return undo_[timelineIndex].id;
        }
        return std::next(redo_.rbegin(), static_cast<std::ptrdiff_t>(
            timelineIndex - currentDepth))->id;
    }

    [[nodiscard]] std::optional<DocumentHistoryTransitionIdentity>
    historyTransitionIdentityAtDepth(const std::size_t targetDepth) const noexcept
    {
        const std::size_t currentDepth = undo_.size();
        const std::size_t totalDepth = currentDepth + redo_.size();
        if (targetDepth == 0 || targetDepth > totalDepth)
        {
            return std::nullopt;
        }

        const auto recordAtTimelineIndex = [this, currentDepth](
            const std::size_t index) -> const Record&
        {
            return index < currentDepth
                ? undo_[index]
                : *std::next(redo_.rbegin(),
                    static_cast<std::ptrdiff_t>(index - currentDepth));
        };
        const auto signatureForDepth = [&recordAtTimelineIndex](
            const std::size_t depth) noexcept
        {
            std::uint64_t hash = 1469598103934665603ull;
            const auto mixByte = [&hash](const std::uint8_t byte) noexcept
            {
                hash ^= static_cast<std::uint64_t>(byte);
                hash *= 1099511628211ull;
            };
            const std::uint64_t count = static_cast<std::uint64_t>(depth);
            for (int byte = 0; byte < 8; ++byte)
            {
                mixByte(static_cast<std::uint8_t>(
                    (count >> (byte * 8)) & 0xFFu));
            }
            for (std::size_t index = 0; index < depth; ++index)
            {
                const std::uint64_t id = recordAtTimelineIndex(index).id;
                for (int byte = 0; byte < 8; ++byte)
                {
                    mixByte(static_cast<std::uint8_t>(
                        (id >> (byte * 8)) & 0xFFu));
                }
            }
            return hash;
        };

        return DocumentHistoryTransitionIdentity{
            recordAtTimelineIndex(targetDepth - 1).id,
            signatureForDepth(targetDepth - 1),
            signatureForDepth(targetDepth),
        };
    }

    [[nodiscard]] DocumentUndoSnapshotStats snapshotStats() const noexcept
    {
        DocumentUndoSnapshotStats stats{};
        const auto count = [&stats](const Snapshot& snapshot)
        {
            std::visit([&stats](const auto& value)
                {
                    using S = std::decay_t<decltype(value)>;
                    if constexpr ( std::is_same_v<S, FullSnapshot> )
                    {
                        ++stats.full;
                    }
                    else if constexpr ( std::is_same_v<S, ThingsSnapshot> )
                    {
                        ++stats.things;
                    }
                    else if constexpr ( std::is_same_v<S, LightsSnapshot> )
                    {
                        ++stats.lights;
                    }
                    else if constexpr ( std::is_same_v<S, CogsSnapshot> )
                    {
                        ++stats.cogs;
                    }
                    else if constexpr (
                        std::is_same_v<S, ThingsLightsSnapshot> )
                    {
                        ++stats.thingsLights;
                    }
                    else if constexpr (
                        std::is_same_v<S, ThingSubsetSnapshot> )
                    {
                        ++stats.thingSubsets;
                    }
                    else if constexpr (
                        std::is_same_v<S, ThingTailSnapshot> )
                    {
                        ++stats.thingTails;
                    }
                    else if constexpr (
                        std::is_same_v<S, SectorSubsetSnapshot> )
                    {
                        ++stats.sectorSubsets;
                    }
                    else if constexpr (
                        std::is_same_v<S, SurfaceSubsetSnapshot> )
                    {
                        ++stats.surfaceSubsets;
                    }
                },
                snapshot);
        };
        for (const Record& record : undo_)
        {
            if ( record.snapshot )
            {
                count(*record.snapshot);
            }
        }
        for (const Record& record : redo_)
        {
            if ( record.snapshot )
            {
                count(*record.snapshot);
            }
        }
        return stats;
    }

    std::string_view nextUndoLabel() const noexcept
    {
        return undo_.empty() ? std::string_view{} : std::string_view(undo_.back().label);
    }

    std::string_view nextRedoLabel() const noexcept
    {
        return redo_.empty() ? std::string_view{} : std::string_view(redo_.back().label);
    }

    std::vector<std::string> undoLabels() const
    {
        std::vector<std::string> labels{};
        labels.reserve(undo_.size());
        for (const Record& record : undo_)
        {
            labels.push_back(record.label);
        }
        return labels;
    }

    std::vector<std::string> redoLabels() const
    {
        std::vector<std::string> labels{};
        labels.reserve(redo_.size());
        // Next redo operation is at the back of the stack.
        for (auto it = redo_.rbegin(); it != redo_.rend(); ++it)
        {
            labels.push_back(it->label);
        }
        return labels;
    }

    /** Return the exact transition which produces `targetDepth`.
     *
     * Depth zero is the initial state and therefore has no transition.
     * The query reads only compact snapshot selection metadata, so the live
     * editor cursor and its Level remain untouched.  It is intentionally a
     * one-entry query rather than a materialized timeline cache: the History
     * dock is opt-in and this keeps ordinary editing cheap.
     */
    std::optional<DocumentHistoryTransitionState> transitionAtDepth(
        const std::size_t targetDepth) const
    {
        const std::size_t currentDepth = undo_.size();
        const std::size_t totalDepth = currentDepth + redo_.size();
        if (targetDepth == 0 || targetDepth > totalDepth)
        {
            return std::nullopt;
        }

        DocumentHistoryTransitionState result{};
        if (targetDepth <= currentDepth)
        {
            const Record& record = undo_[targetDepth - 1];
            if (!record.snapshot)
            {
                return std::nullopt;
            }
            result.label = record.label;
            result.dirtyFlags = record.dirtyFlags;
            appendSnapshotAffectedIndices(
                *record.snapshot,
                result.affectedThingIndices,
                result.affectedSectorIndices);
            result.beforeSelection = snapshotSelection(*record.snapshot);
            if (record.navigationSelection)
            {
                result.afterSelection = *record.navigationSelection;
            }
            else if (targetDepth == currentDepth)
            {
                result.afterSelection = current_.selection;
            }
            else
            {
                if (!undo_[targetDepth].snapshot)
                {
                    return std::nullopt;
                }
                result.afterSelection = snapshotSelection(*undo_[targetDepth].snapshot);
            }
        }
        else
        {
            // Redo records retain the post-operation compact snapshot.  The
            // vector is reverse chronological, with its back as the next
            // operation, so only the adjacent selection is needed.
            const std::size_t redoOffset = targetDepth - currentDepth - 1;
            const std::size_t recordIndex = redo_.size() - 1 - redoOffset;
            const Record& record = redo_[recordIndex];
            if (!record.snapshot)
            {
                return std::nullopt;
            }
            result.label = record.label;
            result.dirtyFlags = record.dirtyFlags;
            appendSnapshotAffectedIndices(
                *record.snapshot,
                result.affectedThingIndices,
                result.affectedSectorIndices);
            result.beforeSelection = redoOffset == 0
                ? current_.selection
                : editor::Selection{};
            if (redoOffset != 0)
            {
                if (!redo_[recordIndex + 1].snapshot)
                {
                    return std::nullopt;
                }
                result.beforeSelection = snapshotSelection(*redo_[recordIndex + 1].snapshot);
            }
            result.afterSelection = record.navigationSelection
                ? *record.navigationSelection
                : snapshotSelection(*record.snapshot);
        }
        return result;
    }

private:
    void prepareRecoveryHistory(
        DocumentState current,
        DocumentRecoveryHistory recovery)
    {
        if (recovery.compact)
        {
            current_ = std::move(current);
            if (recovery.compact.use_count() == 1)
            {
                undo_ = std::move(recovery.compact->undo_);
                redo_ = std::move(recovery.compact->redo_);
            }
            else
            {
                // Records share immutable snapshot payloads and remain COW on
                // the first undo/redo, so copying a reused decoded DTO is
                // bounded by labels/record metadata rather than Level data.
                undo_ = recovery.compact->undo_;
                redo_ = recovery.compact->redo_;
            }
            nextRecordId_ = recovery.compact->nextRecordId_;
            trimToMaximumEntryCount();
            return;
        }

        DocumentState recoveredCurrent = std::move(current);
        undo_.clear();
        redo_.clear();
        nextRecordId_ = 1;

        const std::size_t retainedUndoCount = std::min(
            recovery.undoEntries.size(), DocumentMaximumHistoryEntries);
        const std::size_t retainedRedoCount = std::min(
            recovery.redoEntries.size(),
            DocumentMaximumHistoryEntries - retainedUndoCount);

        undo_.reserve(retainedUndoCount);
        for (auto it = recovery.undoEntries.begin()
                + static_cast<std::ptrdiff_t>(retainedUndoCount);
             it != recovery.undoEntries.begin();)
        {
            --it;
            current_ = std::move(it->state);
            undo_.push_back(Record{
                nextRecordId_++,
                std::move(it->label),
                it->dirtyFlags,
                makeSharedSnapshot(it->dirtyFlags),
                makeNavigationSelection(std::move(it->navigationSelection)) });
        }

        redo_.reserve(retainedRedoCount);
        for (auto it = recovery.redoEntries.begin()
                + static_cast<std::ptrdiff_t>(retainedRedoCount);
             it != recovery.redoEntries.begin();)
        {
            --it;
            current_ = std::move(it->state);
            redo_.push_back(Record{
                nextRecordId_++,
                std::move(it->label),
                it->dirtyFlags,
                makeSharedSnapshot(it->dirtyFlags),
                makeNavigationSelection(std::move(it->navigationSelection)) });
        }
        current_ = std::move(recoveredCurrent);
    }


    template <typename SnapshotT>
    static void appendSnapshotAffectedIndices(
        const SnapshotT& snapshot,
        std::vector<std::int32_t>& thingIndices,
        std::vector<std::int32_t>& sectorIndices)
    {
        std::visit([&](const auto& value)
            {
                using SnapshotValue = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<SnapshotValue, ThingSubsetSnapshot>)
                {
                    thingIndices.reserve(thingIndices.size() + value.things.size());
                    for (const auto& [index, thing] : value.things)
                    {
                        (void)thing;
                        if (index <= static_cast<std::size_t>(
                                std::numeric_limits<std::int32_t>::max()))
                        {
                            thingIndices.push_back(static_cast<std::int32_t>(index));
                        }
                    }
                }
                else if constexpr (std::is_same_v<SnapshotValue, ThingTailSnapshot>)
                {
                    // A tail snapshot is an append-only range.  Publish the
                    // range as bounded indices without copying its Things.
                    const std::size_t maxIndex = std::numeric_limits<std::int32_t>::max();
                    const std::size_t tailCount = value.affectedCount != 0
                        ? value.affectedCount
                        : value.tail.size();
                    const std::size_t count = std::min(
                        tailCount, maxIndex >= value.baseSize
                            ? maxIndex - value.baseSize + 1
                            : std::size_t{});
                    thingIndices.reserve(thingIndices.size() + count);
                    for (std::size_t offset = 0; offset < count; ++offset)
                    {
                        thingIndices.push_back(static_cast<std::int32_t>(
                            value.baseSize + offset));
                    }
                }
                else if constexpr (std::is_same_v<SnapshotValue, SectorSubsetSnapshot>
                    || std::is_same_v<SnapshotValue, SurfaceSubsetSnapshot>)
                {
                    sectorIndices.reserve(sectorIndices.size() + value.sectors.size());
                    for (const auto& [index, sector] : value.sectors)
                    {
                        (void)sector;
                        if (index <= static_cast<std::size_t>(
                                std::numeric_limits<std::int32_t>::max()))
                        {
                            sectorIndices.push_back(static_cast<std::int32_t>(index));
                        }
                    }
                }
            }, snapshot);
    }

    template <typename SnapshotT>
    static editor::Selection snapshotSelection(const SnapshotT& snapshot)
    {
        return std::visit([](const auto& value)
            {
                using SnapshotValue = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<SnapshotValue, FullSnapshot>)
                {
                    return value.state.selection;
                }
                else
                {
                    return value.selection;
                }
            }, snapshot);
    }

public:
    DocumentRecoveryHistory recoveryHistory() const
    {
        DocumentRecoveryHistory result{};
        result.undoEntries.reserve(undoDepth());
        result.redoEntries.reserve(redoDepth());
        (void)visitRecoveryHistoryTransitions(
            [&result](
                const DocumentRecoveryHistoryStack stack,
                const std::string_view label,
                const std::uint32_t dirtyFlags,
                const DocumentHistoryTransitionDomain,
                const assets::SedHistoryTransitionToken& transition,
                const DocumentState& state)
            {
                DocumentRecoveryHistoryEntry entry{};
                entry.label.assign(label);
                entry.dirtyFlags = dirtyFlags;
                entry.state = state;
                if (const editor::Selection* const navigation =
                        transition.navigationSelection())
                {
                    entry.navigationSelection = *navigation;
                }
                auto& entries = stack == DocumentRecoveryHistoryStack::Undo
                    ? result.undoEntries
                    : result.redoEntries;
                entries.push_back(std::move(entry));
                return true;
            });
        return result;
    }

    /** Shares this timeline's immutable compact snapshots for background work.
     *
     * The returned history owns a copy of the cursor/current state and shared
     * snapshot records. Snapshot mutation remains copy-on-write, so a worker
     * can convert the detached copy without expanding bounded subsets or
     * observing later edits to this history.
     */
    DocumentRecoveryHistory compactRecoveryHistory() const
    {
        DocumentRecoveryHistory result{};
        result.compact = std::make_shared<DocumentUndoHistory>(*this);
        return result;
    }

    /**
     * Visits recovery states nearest operation first without retaining an
     * expanded full-state vector.
     *
     * The live stack deliberately keeps private compact snapshot variants.
     * Persistence must not encode those ABI-sensitive variants, so a worker
     * walks a temporary history copy and receives portable DocumentState
     * values one at a time. The first temporary walk is destroyed before the
     * redo walk begins, bounding peak memory to one live history copy plus the
     * state currently being encoded.
     */
    template <typename Visitor>
    bool visitRecoveryHistory(Visitor&& visitor) const
    {
        return visitRecoveryHistoryTransitions(
            [&visitor](
                const DocumentRecoveryHistoryStack stack,
                const std::string_view label,
                const std::uint32_t dirtyFlags,
                const DocumentHistoryTransitionDomain,
                DocumentState& state)
            {
                return visitor(stack, label, dirtyFlags, state);
            });
    }

    /**
     * Visits recovery states with the exact native sections changed by each
     * concrete compact/full Snapshot transition.
     *
     * The domain is sampled before undo()/redo() swaps the record.  This keeps
     * incremental persistence correct for FullSnapshot records whose dirty
     * flags happen to mention only one logical edit domain.
     */
    template <typename Visitor>
    bool visitRecoveryHistoryTransitions(Visitor&& visitor) const
    {
        {
            DocumentUndoHistory walk = *this;
            while (walk.canUndo())
            {
                const DocumentHistoryTransitionDomain domains =
                    transitionDomains(walk.undo_.back());
                std::uint32_t dirtyFlags{};
                if (!walk.undo(&dirtyFlags))
                {
                    return false;
                }
                const assets::SedHistoryTransitionToken transition =
                    transitionToken(
                        walk.redo_.back(), walk.current_.level, domains);
                // undo() moved the record to redo_, where its label is now
                // stable for the duration of this callback.
                if constexpr (std::is_invocable_v<Visitor,
                    DocumentRecoveryHistoryStack, std::string_view,
                    std::uint32_t, DocumentHistoryTransitionDomain,
                    const assets::SedHistoryTransitionToken&, DocumentState&>)
                {
                    if (!visitor(DocumentRecoveryHistoryStack::Undo,
                            walk.nextRedoLabel(), dirtyFlags, domains,
                            transition,
                            walk.current())) return false;
                }
                else if (!visitor(
                            DocumentRecoveryHistoryStack::Undo,
                            walk.nextRedoLabel(), dirtyFlags, domains,
                            walk.current()))
                {
                    return false;
                }
            }
        }

        {
            DocumentUndoHistory walk = *this;
            while (walk.canRedo())
            {
                const DocumentHistoryTransitionDomain domains =
                    transitionDomains(walk.redo_.back());
                std::uint32_t dirtyFlags{};
                if (!walk.redo(&dirtyFlags))
                {
                    return false;
                }
                const assets::SedHistoryTransitionToken transition =
                    transitionToken(
                        walk.undo_.back(), walk.current_.level, domains);
                // redo() moved the record to undo_, where its label is now
                // stable for the duration of this callback.
                if constexpr (std::is_invocable_v<Visitor,
                    DocumentRecoveryHistoryStack, std::string_view,
                    std::uint32_t, DocumentHistoryTransitionDomain,
                    const assets::SedHistoryTransitionToken&, DocumentState&>)
                {
                    if (!visitor(DocumentRecoveryHistoryStack::Redo,
                            walk.nextUndoLabel(), dirtyFlags, domains,
                            transition,
                            walk.current())) return false;
                }
                else if (!visitor(
                            DocumentRecoveryHistoryStack::Redo,
                            walk.nextUndoLabel(), dirtyFlags, domains,
                            walk.current()))
                {
                    return false;
                }
            }
        }
        return true;
    }

    // Prepare imported records privately; failed allocation must retain the
    // caller's current document, both stacks, and future record identities.
    void restoreRecoveryHistory(
        DocumentState current,
        DocumentRecoveryHistory recovery)
    {
        DocumentUndoHistory staged;
        staged.prepareRecoveryHistory(std::move(current), std::move(recovery));
        swap(staged);
    }

    void swap(DocumentUndoHistory& other) noexcept
    {
        static_assert(std::is_nothrow_swappable_v<sed::Level>);
        std::swap(current_.level, other.current_.level);
        swapSelection(current_.selection, other.current_.selection);
        undo_.swap(other.undo_);
        redo_.swap(other.redo_);
        std::swap(nextRecordId_, other.nextRecordId_);
    }

    /**
     * Adds one decoded nearest-first recovery state without retaining its
     * expanded Level. Optional changed-domain indices select the same compact
     * swap records used by live transforms; an omitted index list falls back
     * to the conservative dirty-domain/full snapshot. Call
     * finishCompactRecoveryImport() after both stacks.
     */
    void appendCompactRecoveryState(
        const DocumentRecoveryHistoryStack stack,
        std::string label,
        const std::uint32_t dirtyFlags,
        const sed::Level& level,
        const sed::editor::Selection& selection,
        const std::optional<std::vector<std::int32_t>>& geometrySectorIndices = std::nullopt,
        const std::optional<std::vector<std::int32_t>>& thingIndices = std::nullopt,
        const std::optional<std::pair<std::size_t, std::size_t>>& thingTail = std::nullopt,
        std::optional<sed::editor::Selection> navigationSelection = std::nullopt,
        const bool surfaceSubset = false)
    {
        auto& records = stack == DocumentRecoveryHistoryStack::Undo
            ? undo_ : redo_;
        // Concrete transition metadata is authoritative. Append-only Thing
        // tools legitimately retain Structure | Things as descriptive dirty
        // flags while their snapshot is still a bounded Thing tail.
        Snapshot snapshot = thingTail.has_value()
            ? makeThingTailRecoverySnapshot(
                stack,
                level,
                selection,
                thingTail->first,
                thingTail->second)
            : (thingIndices.has_value()
            ? makeThingSubsetSnapshot(level, selection, *thingIndices)
            : (geometrySectorIndices.has_value()
                ? (surfaceSubset
                    ? makeSurfaceSubsetSnapshot(
                        level, selection, *geometrySectorIndices)
                    : makeSectorSubsetSnapshot(
                        level, selection, *geometrySectorIndices))
                : makeSnapshotForLevel(level, selection, dirtyFlags)));
        records.push_back(Record{
            nextRecordId_,
            std::move(label),
            dirtyFlags,
            std::make_shared<Snapshot>(std::move(snapshot)),
            makeNavigationSelection(std::move(navigationSelection)) });
        ++nextRecordId_;
    }

    void finishCompactRecoveryImport()
    {
        // The wire stream is nearest operation first; the live vectors keep
        // the nearest record at the back for O(1) undo/redo.
        std::reverse(undo_.begin(), undo_.end());
        std::reverse(redo_.begin(), redo_.end());
        trimToMaximumEntryCount();
        current_ = {};
    }

    void pushUndo(std::string label)
    {
        pushUndo(std::move(label), 0);
    }

    void pushUndo(std::string label, const std::uint32_t dirtyFlags)
    {
        undo_.push_back(Record{ nextRecordId_, std::move(label), dirtyFlags, makeSharedSnapshot(dirtyFlags) });
        ++nextRecordId_;
        redo_.clear();
        trimToMaximumEntryCount();
    }

    /**
     * Capture only the named Things for a transform/value edit.
     *
     * Ordinary Things snapshots intentionally cover the complete vector, but
     * that makes a one-Thing drag finish proportional to the whole level.
     * Topology is unchanged by this record, so swapping the selected values is
     * sufficient for exact undo/redo while keeping the gesture boundary O(k).
     */
    void pushThingSubsetUndo(
        std::string label,
        const std::uint32_t dirtyFlags,
        const std::vector<std::int32_t>& thingIndices)
    {
        undo_.push_back(Record{
            nextRecordId_,
            std::move(label),
            dirtyFlags,
            std::make_shared<Snapshot>(makeThingSubsetSnapshot(thingIndices)) });
        ++nextRecordId_;
        redo_.clear();
        trimToMaximumEntryCount();
    }

    /** Capture an append-only Things tail without copying the existing prefix. */
    void pushThingTailUndo(std::string label, const std::uint32_t dirtyFlags)
    {
        undo_.push_back(Record{
            nextRecordId_,
            std::move(label),
            dirtyFlags,
            std::make_shared<Snapshot>(makeThingTailSnapshot()) });
        ++nextRecordId_;
        redo_.clear();
        trimToMaximumEntryCount();
    }

    /** Capture only the named Sectors for a topology-preserving transform. */
    void pushSectorSubsetUndo(
        std::string label,
        const std::uint32_t dirtyFlags,
        const std::vector<std::int32_t>& sectorIndices)
    {
        undo_.push_back(Record{
            nextRecordId_,
            std::move(label),
            dirtyFlags,
            std::make_shared<Snapshot>(makeSectorSubsetSnapshot(
                current_.level, current_.selection, sectorIndices)) });
        ++nextRecordId_;
        redo_.clear();
        trimToMaximumEntryCount();
    }

    /** Capture all Sectors touched by a Surface transform. */
    void pushSurfaceSubsetUndo(
        std::string label,
        const std::uint32_t dirtyFlags,
        const std::vector<std::int32_t>& touchedSectorIndices)
    {
        undo_.push_back(Record{
            nextRecordId_,
            std::move(label),
            dirtyFlags,
            std::make_shared<Snapshot>(makeSurfaceSubsetSnapshot(
                current_.level, current_.selection, touchedSectorIndices)) });
        ++nextRecordId_;
        redo_.clear();
        trimToMaximumEntryCount();
    }

    /**
     * Marks a derived mutation as part of the current user-visible edit.
     *
     * Automatic baked-light publication changes persisted level data after
     * the originating Light command has already pushed its undo record. The
     * record snapshot already contains the pre-edit light and baked output;
     * rotating its identity makes save-state tracking observe the later
     * derived publication without adding a second Undo menu entry.
     */
    bool touchLatestUndoRecord() noexcept
    {
        if (undo_.empty())
        {
            return false;
        }
        if (undo_.back().snapshot)
        {
            if (ThingTailSnapshot* tail = std::get_if<ThingTailSnapshot>(
                    undo_.back().snapshot.get()))
            {
                tail->affectedCount = current_.level.things.size() >= tail->baseSize
                    ? current_.level.things.size() - tail->baseSize
                    : 0;
            }
        }
        undo_.back().id = nextRecordId_++;
        redo_.clear();
        return true;
    }

    template <typename Fn>
    bool apply(std::string label, Fn&& mutate)
    {
        return apply(std::move(label), 0, std::forward<Fn>(mutate));
    }

    template <typename Fn>
    bool apply(std::string label, const std::uint32_t dirtyFlags, Fn&& mutate)
    {
        return applyWithSnapshot(std::move(label), dirtyFlags, std::forward<Fn>(mutate),
            [this, dirtyFlags] { return makeSnapshot(dirtyFlags); });
    }

    /** Transaction restricted to existing selected Things, selection, and curId.
     * The callback must not resize/reorder Things or edit an unlisted Thing.
     * False or an exception restores the captured subset without publishing history.
     */
    template <typename Fn>
    bool applyThingSubset(std::string label, const std::vector<std::int32_t>& thingIndices, Fn&& mutate)
    {
        return applyWithSnapshot(std::move(label), static_cast<std::uint32_t>(DocumentDirty::Things),
            std::forward<Fn>(mutate), [this, &thingIndices] { return makeThingSubsetSnapshot(thingIndices); });
    }

private:
    template <typename Fn, typename SnapshotFactory>
    bool applyWithSnapshot(std::string label, const std::uint32_t dirtyFlags,
        Fn&& mutate, SnapshotFactory&& snapshotFactory)
    {
        // Finish every fallible history allocation before exposing the live
        // state to the mutation. The shared snapshot remains our rollback
        // owner until the new Record is committed.
        std::shared_ptr<Snapshot> before =
            std::make_shared<Snapshot>(snapshotFactory());
        undo_.reserve(undo_.size() + 1);

        try
        {
            if (!mutate(current_))
            {
                restoreSnapshot(std::move(*before));
                return false;
            }

            std::shared_ptr<const editor::Selection> navigationSelection{};
            if ((dirtyFlags & static_cast<std::uint32_t>(DocumentDirty::Structure)) != 0u)
            {
                navigationSelection = std::make_shared<const editor::Selection>(current_.selection);
            }

            Record record{ nextRecordId_, std::move(label), dirtyFlags,
                before, std::move(navigationSelection) };
            undo_.push_back(std::move(record));
        }
        catch (...)
        {
            // Snapshot restoration is move-only and allocation-free for the
            // default containers. No partially-mutated state or history row
            // may escape a failed command.
            restoreSnapshot(std::move(*before));
            throw;
        }

        ++nextRecordId_;
        redo_.clear();
        trimToMaximumEntryCount();
        return true;
    }

public:

    bool undo(
        std::uint32_t* outDirtyFlags = nullptr,
        std::vector<std::int32_t>* outThingSubsetIndices = nullptr)
    {
        return transitionHistory(undo_, redo_, outDirtyFlags, outThingSubsetIndices);
    }

    bool redo(
        std::uint32_t* outDirtyFlags = nullptr,
        std::vector<std::int32_t>* outThingSubsetIndices = nullptr)
    {
        return transitionHistory(redo_, undo_, outDirtyFlags, outThingSubsetIndices);
    }

private:
    void trimToMaximumEntryCount()
    {
        static_assert(std::is_nothrow_move_constructible_v<Record>);
        static_assert(std::is_nothrow_move_assignable_v<Record>);
        while (undo_.size() + redo_.size() > DocumentMaximumHistoryEntries)
        {
            // The front is the farthest state on both live stacks. Preserve
            // the operations closest to the cursor, favoring the larger side
            // when an imported timeline must be defensively trimmed.
            auto& records = undo_.size() >= redo_.size() ? undo_ : redo_;
            records.erase(records.begin());
        }
    }

    struct BakedSurfaceSnapshot
    {
        std::vector<sed::ColorF> texVertexColors{};
    };

    struct BakedSectorSnapshot
    {
        sed::ColorF ambient{};
        std::vector<BakedSurfaceSnapshot> surfaces{};
    };

    struct BakedLightingSnapshot
    {
        std::vector<BakedSectorSnapshot> sectors{};
    };

    struct FullSnapshot
    {
        DocumentState state{};
    };

    struct ThingsSnapshot
    {
        std::vector<sed::Thing> things{};
        std::vector<sed::MotsArchLightObject> motsArchLighting{};
        std::vector<sed::MotsArchLightingBakeBaseline>
            motsArchLightingBakeBaselines{};
        std::int32_t curId{};
        sed::editor::Selection selection{};
    };

    struct LightsSnapshot
    {
        std::vector<sed::SedLight> lights{};
        BakedLightingSnapshot bakedLighting{};
        std::int32_t curId{};
        sed::editor::Selection selection{};
    };

    struct CogsSnapshot
    {
        std::vector<sed::Cog> cogs{};
        bool importedJklCogRowsPresent{};
        std::vector<sed::SurfaceRef> importedJklSurfaceRefs{};
        std::int32_t curId{};
        sed::editor::Selection selection{};
    };

    struct ThingsLightsSnapshot
    {
        std::vector<sed::Thing> things{};
        std::vector<sed::MotsArchLightObject> motsArchLighting{};
        std::vector<sed::MotsArchLightingBakeBaseline>
            motsArchLightingBakeBaselines{};
        std::vector<sed::SedLight> lights{};
        BakedLightingSnapshot bakedLighting{};
        std::int32_t curId{};
        sed::editor::Selection selection{};
    };

    struct ThingSubsetSnapshot
    {
        std::vector<std::pair<std::size_t, sed::Thing>> things{};
        std::int32_t curId{};
        sed::editor::Selection selection{};
    };

    struct ThingTailSnapshot
    {
        std::size_t baseSize{};
        // The pre-edit snapshot has an empty tail.  The append command fills
        // this bounded count when it is finalized so history navigation can
        // expose affected indices without retaining another Level.
        std::size_t affectedCount{};
        std::vector<sed::Thing> tail{};
        std::int32_t curId{};
        sed::editor::Selection selection{};
    };

    struct SectorSubsetSnapshot
    {
        std::vector<std::pair<std::size_t, sed::Sector>> sectors{};
        std::int32_t curId{};
        sed::editor::Selection selection{};
    };

    struct SurfaceSubsetSnapshot
    {
        // Surface transforms can move shared vertices and adjoined faces, so
        // the complete owning Sector is the smallest exact unit to swap.
        std::vector<std::pair<std::size_t, sed::Sector>> sectors{};
        std::int32_t curId{};
        sed::editor::Selection selection{};
    };

    using Snapshot = std::variant<
        FullSnapshot,
        ThingsSnapshot,
        LightsSnapshot,
        CogsSnapshot,
        ThingsLightsSnapshot,
        ThingSubsetSnapshot,
        ThingTailSnapshot,
        SectorSubsetSnapshot,
        SurfaceSubsetSnapshot>;

    struct Record
    {
        std::uint64_t id{};
        std::string label{};
        std::uint32_t dirtyFlags{};
        std::shared_ptr<Snapshot> snapshot{};

        // Structural commands commonly replace their source selection with
        // exact newly-created outputs (Sector/Surface cleave is the canonical
        // case).  Selection changes made later by the user must not erase
        // that navigation target from the last History row.  Keep only this
        // compact selection hint; the authoritative undo payload remains the
        // existing Snapshot and no second Level is retained. Immutable shared
        // ownership also keeps Record moves allocation-free on MSVC, where an
        // unordered container move can allocate a sentinel during cap trimming.
        std::shared_ptr<const editor::Selection> navigationSelection{};
    };

    static std::shared_ptr<const editor::Selection> makeNavigationSelection(
        std::optional<editor::Selection> selection)
    {
        return selection ? std::make_shared<const editor::Selection>(std::move(*selection))
            : std::shared_ptr<const editor::Selection>{};
    }

    static DocumentHistoryTransitionDomain transitionDomains(
        const Record& record) noexcept
    {
        if (!record.snapshot)
        {
            return DocumentHistoryTransitionDomain::All;
        }
        return std::visit([](const auto& value) noexcept
            {
                using S = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<S, FullSnapshot>)
                {
                    return DocumentHistoryTransitionDomain::All;
                }
                else if constexpr (std::is_same_v<S, ThingsSnapshot>
                    || std::is_same_v<S, ThingSubsetSnapshot>
                    || std::is_same_v<S, ThingTailSnapshot>)
                {
                    return DocumentHistoryTransitionDomain::Head
                        | DocumentHistoryTransitionDomain::Things;
                }
                else if constexpr (std::is_same_v<S, CogsSnapshot>)
                {
                    return DocumentHistoryTransitionDomain::Head
                        | DocumentHistoryTransitionDomain::Cogs;
                }
                else if constexpr (std::is_same_v<S, LightsSnapshot>)
                {
                    return DocumentHistoryTransitionDomain::Head
                        | DocumentHistoryTransitionDomain::Sectors
                        | DocumentHistoryTransitionDomain::Lights;
                }
                else if constexpr (std::is_same_v<S, ThingsLightsSnapshot>)
                {
                    return DocumentHistoryTransitionDomain::Head
                        | DocumentHistoryTransitionDomain::Sectors
                        | DocumentHistoryTransitionDomain::Things
                        | DocumentHistoryTransitionDomain::Lights;
                }
                else
                {
                    static_assert(std::is_same_v<S, SectorSubsetSnapshot>
                        || std::is_same_v<S, SurfaceSubsetSnapshot>);
                    return DocumentHistoryTransitionDomain::Head
                        | DocumentHistoryTransitionDomain::Sectors;
                }
            }, *record.snapshot);
    }

    static assets::SedRequiredSection requiredSectionsForTransition(
        const DocumentHistoryTransitionDomain domains) noexcept
    {
        assets::SedRequiredSection result = assets::SedRequiredSection::None;
        const auto add = [&](const DocumentHistoryTransitionDomain source,
                             const assets::SedRequiredSection destination)
        {
            if (hasAny(domains, source)) result |= destination;
        };
        add(DocumentHistoryTransitionDomain::Head,
            assets::SedRequiredSection::Head);
        add(DocumentHistoryTransitionDomain::Resources,
            assets::SedRequiredSection::Resources);
        add(DocumentHistoryTransitionDomain::Sectors,
            assets::SedRequiredSection::Sectors);
        add(DocumentHistoryTransitionDomain::Things,
            assets::SedRequiredSection::Things);
        add(DocumentHistoryTransitionDomain::Templates,
            assets::SedRequiredSection::Templates);
        add(DocumentHistoryTransitionDomain::Cogs,
            assets::SedRequiredSection::Cogs);
        add(DocumentHistoryTransitionDomain::Lights,
            assets::SedRequiredSection::Lights);
        add(DocumentHistoryTransitionDomain::Hierarchy,
            assets::SedRequiredSection::Hierarchy);
        return result;
    }

    static assets::SedHistoryTransitionToken transitionToken(
        const Record& record,
        const Level& level,
        const DocumentHistoryTransitionDomain domains)
    {
        bool sectorSubset{};
        std::vector<std::int32_t> sectorIndices{};
        bool surfaceSubset{};
        bool thingSubset{};
        std::vector<std::int32_t> thingIndices{};
        bool thingTail{};
        std::size_t thingTailBaseSize{};
        std::size_t thingTailAffectedCount{};
        if (record.snapshot)
        {
            std::visit([&](const auto& value)
                {
                    using S = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<S, SectorSubsetSnapshot>
                        || std::is_same_v<S, SurfaceSubsetSnapshot>)
                    {
                        sectorSubset = true;
                        surfaceSubset =
                            std::is_same_v<S, SurfaceSubsetSnapshot>;
                        sectorIndices.reserve(value.sectors.size());
                        for (const auto& [index, sector] : value.sectors)
                        {
                            (void)sector;
                            if (index <= static_cast<std::size_t>(
                                    std::numeric_limits<std::int32_t>::max()))
                            {
                                sectorIndices.push_back(
                                    static_cast<std::int32_t>(index));
                            }
                        }
                    }
                    else if constexpr (std::is_same_v<S, ThingSubsetSnapshot>)
                    {
                        thingSubset = true;
                        thingIndices.reserve(value.things.size());
                        for (const auto& [index, thing] : value.things)
                        {
                            (void)thing;
                            if (index <= static_cast<std::size_t>(
                                    std::numeric_limits<std::int32_t>::max()))
                            {
                                thingIndices.push_back(
                                    static_cast<std::int32_t>(index));
                            }
                        }
                    }
                    else if constexpr (std::is_same_v<S, ThingTailSnapshot>)
                    {
                        thingTail = true;
                        thingTailBaseSize = value.baseSize;
                        const std::size_t levelTailSize =
                            level.things.size() >= value.baseSize
                                ? level.things.size() - value.baseSize
                                : 0;
                        thingTailAffectedCount = std::max(
                            { value.affectedCount,
                              value.tail.size(),
                              levelTailSize });
                    }
                }, *record.snapshot);
        }
        return assets::SedHistoryTransitionToken(
            level, requiredSectionsForTransition(domains), sectorSubset,
            std::move(sectorIndices), surfaceSubset,
            thingSubset, std::move(thingIndices),
            thingTail, thingTailBaseSize, thingTailAffectedCount,
            record.navigationSelection ? &*record.navigationSelection : nullptr);
    }

    bool transitionHistory(
        std::vector<Record>& source,
        std::vector<Record>& destination,
        std::uint32_t* outDirtyFlags,
        std::vector<std::int32_t>* outThingSubsetIndices)
    {
        if ( source.empty() )
        {
            if ( outThingSubsetIndices ) outThingSubsetIndices->clear();
            return false;
        }

        // Prepare all fallible work without consuming a record or changing its
        // shared payload. Unique compact snapshots retain the O(k) swap path;
        // a retained recovery copy alone requires a private payload clone.
        const Record& pending = source.back();
        std::shared_ptr<Snapshot> privateSnapshot{};
        if ( !pending.snapshot )
        {
            privateSnapshot = std::make_shared<Snapshot>(FullSnapshot{});
        }
        else if ( pending.snapshot.use_count() != 1 )
        {
            privateSnapshot = std::make_shared<Snapshot>(*pending.snapshot);
        }
        Snapshot& snapshot = privateSnapshot ? *privateSnapshot : *pending.snapshot;
        const bool compact = std::holds_alternative<ThingSubsetSnapshot>(snapshot)
            || std::holds_alternative<ThingTailSnapshot>(snapshot)
            || std::holds_alternative<SectorSubsetSnapshot>(snapshot)
            || std::holds_alternative<SurfaceSubsetSnapshot>(snapshot);
        if ( compact && !canSwapCompactSnapshot(snapshot) ) return false;

        std::vector<std::int32_t> indices{};
        copyThingSubsetIndices(snapshot, outThingSubsetIndices ? &indices : nullptr);
        destination.reserve(destination.size() + 1);
        std::shared_ptr<Snapshot> inverse = compact
            ? std::shared_ptr<Snapshot>{} : makeSharedSnapshot(pending.dirtyFlags);

        // Install a fully prepared destination row while source stays intact.
        // Navigation selections have immutable shared ownership, so relocation
        // and history-cap trimming never move-construct unordered containers.
        destination.push_back(Record{pending.id, pending.label, pending.dirtyFlags,
            compact ? (privateSnapshot ? privateSnapshot : pending.snapshot) : inverse,
            pending.navigationSelection});
        const std::uint32_t dirtyFlags = pending.dirtyFlags;
        try
        {
            // A Thing tail prepares both growing buffers before its first move.
            // Other compact variants only swap after complete index preflight.
            if ( compact && !swapCompactSnapshot(snapshot) )
            {
                destination.pop_back();
                return false;
            }
            if ( !compact ) restoreSnapshot(std::move(snapshot));
        }
        catch ( ... )
        {
            destination.pop_back();
            throw;
        }
        source.pop_back();
        if ( outThingSubsetIndices ) outThingSubsetIndices->swap(indices);
        if ( outDirtyFlags ) *outDirtyFlags = dirtyFlags;
        return true;
    }

    static void swapSelection(editor::Selection& left, editor::Selection& right) noexcept
    {
        // Container member swaps exchange the existing sentinel/bucket owners;
        // std::swap(Selection) instead invokes potentially allocating moves.
        static_assert(noexcept(left.sectors.swap(right.sectors)));
        static_assert(noexcept(left.surfaces.swap(right.surfaces)));
        static_assert(noexcept(left.surfaceVertices.swap(right.surfaceVertices)));
        static_assert(noexcept(left.things.swap(right.things)));
        static_assert(noexcept(left.frames.swap(right.frames)));
        static_assert(noexcept(left.lights.swap(right.lights)));
        static_assert(noexcept(left.cogs.swap(right.cogs)));
        left.sectors.swap(right.sectors);
        left.surfaces.swap(right.surfaces);
        left.surfaceVertices.swap(right.surfaceVertices);
        left.things.swap(right.things);
        left.frames.swap(right.frames);
        left.lights.swap(right.lights);
        left.cogs.swap(right.cogs);
    }

    bool canSwapCompactSnapshot(const Snapshot& snapshot) const noexcept
    {
        return std::visit([this](const auto& value) noexcept
            {
                using S = std::decay_t<decltype(value)>;
                if constexpr ( std::is_same_v<S, ThingSubsetSnapshot> )
                {
                    return std::all_of(value.things.begin(), value.things.end(),
                        [this](const auto& item) { return item.first < current_.level.things.size(); });
                }
                else if constexpr ( std::is_same_v<S, ThingTailSnapshot> )
                {
                    return value.baseSize <= current_.level.things.size()
                        && value.tail.size() <= current_.level.things.max_size() - value.baseSize;
                }
                else if constexpr ( std::is_same_v<S, SectorSubsetSnapshot>
                    || std::is_same_v<S, SurfaceSubsetSnapshot> )
                {
                    return std::all_of(value.sectors.begin(), value.sectors.end(),
                        [this](const auto& item) { return item.first < current_.level.sectors.size(); });
                }
                else
                {
                    return false;
                }
            }, snapshot);
    }

    static constexpr std::uint32_t allowedPartialDirtyFlags() noexcept
    {
        return static_cast<std::uint32_t>(DocumentDirty::Things)
            | static_cast<std::uint32_t>(DocumentDirty::Lights)
            | static_cast<std::uint32_t>(DocumentDirty::Cogs);
    }

    [[nodiscard]] static BakedLightingSnapshot makeBakedLightingSnapshot(
        const sed::Level& level)
    {
        BakedLightingSnapshot snapshot{};
        snapshot.sectors.reserve(level.sectors.size());
        for (const sed::Sector& sector : level.sectors)
        {
            BakedSectorSnapshot sectorSnapshot{};
            sectorSnapshot.ambient = sector.ambient;
            sectorSnapshot.surfaces.reserve(sector.surfaces.size());
            for (const sed::Surface& surface : sector.surfaces)
            {
                BakedSurfaceSnapshot surfaceSnapshot{};
                surfaceSnapshot.texVertexColors.reserve(surface.texVertices.size());
                for (const sed::TexVertex& texVertex : surface.texVertices)
                {
                    surfaceSnapshot.texVertexColors.push_back(texVertex.color);
                }
                sectorSnapshot.surfaces.push_back(std::move(surfaceSnapshot));
            }
            snapshot.sectors.push_back(std::move(sectorSnapshot));
        }
        return snapshot;
    }

    void restoreBakedLighting(BakedLightingSnapshot&& snapshot)
    {
        // A Lights record cannot change topology. Later structural commands
        // must be undone before reaching it, so exact shape equality is the
        // expected history invariant. Retain a defensive bounds check so a
        // malformed/directly-mutated history cannot write out of range.
        if (snapshot.sectors.size() != current_.level.sectors.size())
        {
            return;
        }
        for (std::size_t sectorIndex = 0; sectorIndex < snapshot.sectors.size(); ++sectorIndex)
        {
            BakedSectorSnapshot& sourceSector = snapshot.sectors[sectorIndex];
            sed::Sector& destinationSector = current_.level.sectors[sectorIndex];
            if (sourceSector.surfaces.size() != destinationSector.surfaces.size())
            {
                return;
            }
            for (std::size_t surfaceIndex = 0; surfaceIndex < sourceSector.surfaces.size(); ++surfaceIndex)
            {
                const BakedSurfaceSnapshot& sourceSurface = sourceSector.surfaces[surfaceIndex];
                sed::Surface& destinationSurface = destinationSector.surfaces[surfaceIndex];
                if (sourceSurface.texVertexColors.size() != destinationSurface.texVertices.size())
                {
                    return;
                }
            }
        }

        for (std::size_t sectorIndex = 0; sectorIndex < snapshot.sectors.size(); ++sectorIndex)
        {
            BakedSectorSnapshot& sourceSector = snapshot.sectors[sectorIndex];
            sed::Sector& destinationSector = current_.level.sectors[sectorIndex];
            destinationSector.ambient = sourceSector.ambient;
            for (std::size_t surfaceIndex = 0; surfaceIndex < sourceSector.surfaces.size(); ++surfaceIndex)
            {
                BakedSurfaceSnapshot& sourceSurface = sourceSector.surfaces[surfaceIndex];
                sed::Surface& destinationSurface = destinationSector.surfaces[surfaceIndex];
                for (std::size_t vertexIndex = 0;
                     vertexIndex < sourceSurface.texVertexColors.size();
                     ++vertexIndex)
                {
                    destinationSurface.texVertices[vertexIndex].color =
                        sourceSurface.texVertexColors[vertexIndex];
                }
            }
        }
    }

    static Snapshot makeSnapshotForLevel(
        const sed::Level& level,
        const sed::editor::Selection& selection,
        const std::uint32_t dirtyFlags)
    {
        // Treat any unknown/combined edits as full snapshots for correctness.
        // Structure changes are also full snapshots even when a caller
        // combines the flag with Things/Lights/Cogs; the compact partial
        // variants cannot restore topology, templates, geometry, or arbitrary
        // mixed domains.
        if (dirtyFlags == 0
            || (dirtyFlags & static_cast<std::uint32_t>(DocumentDirty::Structure)) != 0
            || (dirtyFlags & ~allowedPartialDirtyFlags()) != 0)
        {
            return FullSnapshot{ DocumentState{ level, selection } };
        }

        const bool things = (dirtyFlags & static_cast<std::uint32_t>(DocumentDirty::Things)) != 0;
        const bool lights = (dirtyFlags & static_cast<std::uint32_t>(DocumentDirty::Lights)) != 0;
        const bool cogs = (dirtyFlags & static_cast<std::uint32_t>(DocumentDirty::Cogs)) != 0;

        // Mixed COG/domain edits are uncommon and do not have a dedicated
        // combined compact representation. Preserve exactness with a full
        // snapshot; pure COG value edits remain compact below.
        if (cogs && (things || lights))
        {
            return FullSnapshot{ DocumentState{ level, selection } };
        }

        if (things && lights)
        {
            ThingsLightsSnapshot snap{};
            snap.things = level.things;
            snap.motsArchLighting = level.motsArchLighting;
            snap.motsArchLightingBakeBaselines =
                level.motsArchLightingBakeBaselines;
            snap.lights = level.lights;
            snap.bakedLighting = makeBakedLightingSnapshot(level);
            snap.curId = level.curId;
            snap.selection = selection;
            return snap;
        }

        if (things)
        {
            ThingsSnapshot snap{};
            snap.things = level.things;
            snap.motsArchLighting = level.motsArchLighting;
            snap.motsArchLightingBakeBaselines =
                level.motsArchLightingBakeBaselines;
            snap.curId = level.curId;
            snap.selection = selection;
            return snap;
        }

        if (lights)
        {
            if (level.kind == sed::ProjectType::MOTS)
            {
                // MotS baked lights can also change Thing ARCHLIGHTING. Keep
                // those source, output, and provenance domains in one snapshot.
                ThingsLightsSnapshot snap{};
                snap.things = level.things;
                snap.motsArchLighting = level.motsArchLighting;
                snap.motsArchLightingBakeBaselines =
                    level.motsArchLightingBakeBaselines;
                snap.lights = level.lights;
                snap.bakedLighting = makeBakedLightingSnapshot(level);
                snap.curId = level.curId;
                snap.selection = selection;
                return snap;
            }
            LightsSnapshot snap{};
            snap.lights = level.lights;
            snap.bakedLighting = makeBakedLightingSnapshot(level);
            snap.curId = level.curId;
            snap.selection = selection;
            return snap;
        }

        if (cogs)
        {
            CogsSnapshot snap{};
            snap.cogs = level.cogs;
            snap.importedJklCogRowsPresent =
                level.importedJklCogRowsPresent;
            snap.importedJklSurfaceRefs = level.importedJklSurfaceRefs;
            snap.curId = level.curId;
            snap.selection = selection;
            return snap;
        }

        return FullSnapshot{ DocumentState{ level, selection } };
    }

    Snapshot makeSnapshot(const std::uint32_t dirtyFlags) const
    {
        return makeSnapshotForLevel(
            current_.level, current_.selection, dirtyFlags);
    }

    std::shared_ptr<Snapshot> makeSharedSnapshot(
        const std::uint32_t dirtyFlags) const
    {
        return std::make_shared<Snapshot>(makeSnapshot(dirtyFlags));
    }

    Snapshot makeThingSubsetSnapshot(
        const std::vector<std::int32_t>& thingIndices) const
    {
        return makeThingSubsetSnapshot(
            current_.level, current_.selection, thingIndices);
    }

    static Snapshot makeThingSubsetSnapshot(
        const sed::Level& level,
        const sed::editor::Selection& selection,
        const std::vector<std::int32_t>& thingIndices)
    {
        // Compact row snapshots cannot restore the independent MotS ARCH bake
        // provenance table. Escalate only while such provenance exists.
        if (!level.motsArchLightingBakeBaselines.empty())
        {
            return FullSnapshot{ DocumentState{ level, selection } };
        }
        ThingSubsetSnapshot snapshot{};
        snapshot.curId = level.curId;
        snapshot.selection = selection;

        std::vector<std::int32_t> ordered = thingIndices;
        std::sort(ordered.begin(), ordered.end());
        ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());
        snapshot.things.reserve(ordered.size());
        for (const std::int32_t index : ordered)
        {
            if (index < 0
                || static_cast<std::size_t>(index) >= level.things.size())
            {
                continue;
            }
            snapshot.things.emplace_back(
                static_cast<std::size_t>(index),
                level.things[static_cast<std::size_t>(index)]);
        }
        return snapshot;
    }

    Snapshot makeThingTailSnapshot() const
    {
        if (!current_.level.motsArchLightingBakeBaselines.empty())
        {
            return FullSnapshot{ DocumentState{ current_.level, current_.selection } };
        }
        ThingTailSnapshot snapshot{};
        snapshot.baseSize = current_.level.things.size();
        snapshot.curId = current_.level.curId;
        snapshot.selection = current_.selection;
        return snapshot;
    }

    static Snapshot makeThingTailRecoverySnapshot(
        const DocumentRecoveryHistoryStack stack,
        const sed::Level& level,
        const sed::editor::Selection& selection,
        const std::size_t baseSize,
        const std::size_t affectedCount)
    {
        if (!level.motsArchLightingBakeBaselines.empty())
        {
            return FullSnapshot{ DocumentState{ level, selection } };
        }
        ThingTailSnapshot snapshot{};
        snapshot.baseSize = baseSize;
        snapshot.affectedCount = affectedCount;
        snapshot.curId = level.curId;
        snapshot.selection = selection;
        if (stack == DocumentRecoveryHistoryStack::Redo
            && baseSize <= level.things.size())
        {
            snapshot.tail.assign(
                level.things.begin() + static_cast<std::ptrdiff_t>(baseSize),
                level.things.end());
        }
        return snapshot;
    }

    template <typename SnapshotType>
    static Snapshot makeSectorSubsetSnapshotFor(
        const sed::Level& level,
        const sed::editor::Selection& selection,
        const std::vector<std::int32_t>& sectorIndices)
    {
        SnapshotType snapshot{};
        snapshot.curId = level.curId;
        snapshot.selection = selection;

        std::vector<std::int32_t> ordered = sectorIndices;
        std::sort(ordered.begin(), ordered.end());
        ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());
        snapshot.sectors.reserve(ordered.size());
        for (const std::int32_t index : ordered)
        {
            if (index < 0
                || static_cast<std::size_t>(index) >= level.sectors.size())
            {
                continue;
            }
            snapshot.sectors.emplace_back(
                static_cast<std::size_t>(index),
                level.sectors[static_cast<std::size_t>(index)]);
        }
        return Snapshot{ std::move(snapshot) };
    }

    static Snapshot makeSectorSubsetSnapshot(
        const sed::Level& level,
        const sed::editor::Selection& selection,
        const std::vector<std::int32_t>& sectorIndices)
    {
        return makeSectorSubsetSnapshotFor<SectorSubsetSnapshot>(
            level, selection, sectorIndices);
    }

    static Snapshot makeSurfaceSubsetSnapshot(
        const sed::Level& level,
        const sed::editor::Selection& selection,
        const std::vector<std::int32_t>& sectorIndices)
    {
        return makeSectorSubsetSnapshotFor<SurfaceSubsetSnapshot>(
            level, selection, sectorIndices);
    }

    static void copyThingSubsetIndices(
        const Snapshot& snapshot,
        std::vector<std::int32_t>* outIndices)
    {
        if (!outIndices)
        {
            return;
        }
        const ThingSubsetSnapshot* subset =
            std::get_if<ThingSubsetSnapshot>(&snapshot);
        if (!subset)
        {
            return;
        }
        outIndices->reserve(subset->things.size());
        for (const auto& [index, thing] : subset->things)
        {
            (void)thing;
            if (index <= static_cast<std::size_t>(
                    std::numeric_limits<std::int32_t>::max()))
            {
                outIndices->push_back(static_cast<std::int32_t>(index));
            }
        }
    }

    bool swapCompactSnapshot(Snapshot& snapshot)
    {
        if (ThingSubsetSnapshot* subset = std::get_if<ThingSubsetSnapshot>(&snapshot))
        {
            for ( const auto& [index, stored] : subset->things )
            {
                (void)stored;
                if ( index >= current_.level.things.size() ) return false;
            }
            static_assert(std::is_nothrow_swappable_v<sed::Thing>);
            for ( auto& [index, stored] : subset->things )
            {
                std::swap(stored, current_.level.things[index]);
            }
            std::swap(subset->curId, current_.level.curId);
            swapSelection(subset->selection, current_.selection);
            return true;
        }

        if (ThingTailSnapshot* tail = std::get_if<ThingTailSnapshot>(&snapshot))
        {
            if (tail->baseSize > current_.level.things.size())
            {
                return false;
            }

            if ( tail->tail.size() > current_.level.things.max_size() - tail->baseSize )
            {
                return false;
            }
            static_assert(std::is_nothrow_move_constructible_v<sed::Thing>);
            static_assert(std::is_nothrow_move_assignable_v<sed::Thing>);
            std::vector<sed::Thing> currentTail{};
            currentTail.reserve(current_.level.things.size() - tail->baseSize);
            current_.level.things.reserve(tail->baseSize + tail->tail.size());
            std::move(
                current_.level.things.begin()
                    + static_cast<std::ptrdiff_t>(tail->baseSize),
                current_.level.things.end(),
                std::back_inserter(currentTail));
            current_.level.things.resize(tail->baseSize);
            current_.level.things.insert(
                current_.level.things.end(),
                std::make_move_iterator(tail->tail.begin()),
                std::make_move_iterator(tail->tail.end()));
            tail->tail = std::move(currentTail);
            std::swap(tail->curId, current_.level.curId);
            swapSelection(tail->selection, current_.selection);
            return true;
        }

        if (SectorSubsetSnapshot* subset = std::get_if<SectorSubsetSnapshot>(&snapshot))
        {
            return swapSectorSubsetSnapshot(*subset);
        }

        if (SurfaceSubsetSnapshot* subset = std::get_if<SurfaceSubsetSnapshot>(&snapshot))
        {
            return swapSurfaceSubsetSnapshot(*subset);
        }

        return false;
    }

    bool swapSectorSubsetSnapshot(SectorSubsetSnapshot& snapshot)
    {
        static_assert(std::is_nothrow_swappable_v<sed::Sector>);
        for (const auto& [index, stored] : snapshot.sectors)
        {
            (void)stored;
            if (index >= current_.level.sectors.size())
            {
                return false;
            }
        }
        for (auto& [index, stored] : snapshot.sectors)
        {
            std::swap(stored, current_.level.sectors[index]);
        }
        std::swap(snapshot.curId, current_.level.curId);
        swapSelection(snapshot.selection, current_.selection);
        return true;
    }

    bool swapSurfaceSubsetSnapshot(SurfaceSubsetSnapshot& snapshot)
    {
        static_assert(std::is_nothrow_swappable_v<sed::Sector>);
        for (const auto& [index, stored] : snapshot.sectors)
        {
            (void)stored;
            if (index >= current_.level.sectors.size())
            {
                return false;
            }
        }
        for (auto& [index, stored] : snapshot.sectors)
        {
            std::swap(stored, current_.level.sectors[index]);
        }
        std::swap(snapshot.curId, current_.level.curId);
        swapSelection(snapshot.selection, current_.selection);
        return true;
    }

    void restoreSnapshot(Snapshot&& snapshot)
    {
        std::visit([this](auto&& snap)
            {
                using S = std::decay_t<decltype(snap)>;
                if constexpr (std::is_same_v<S, FullSnapshot>)
                {
                    static_assert(std::is_nothrow_move_assignable_v<sed::Level>);
                    current_.level = std::move(snap.state.level);
                    swapSelection(current_.selection, snap.state.selection);
                }
                else if constexpr (std::is_same_v<S, ThingsSnapshot>)
                {
                    current_.level.things = std::move(snap.things);
                    current_.level.motsArchLighting =
                        std::move(snap.motsArchLighting);
                    current_.level.motsArchLightingBakeBaselines =
                        std::move(snap.motsArchLightingBakeBaselines);
                    current_.level.curId = snap.curId;
                    swapSelection(current_.selection, snap.selection);
                }
                else if constexpr (std::is_same_v<S, ThingSubsetSnapshot>)
                {
                    static_assert(std::is_nothrow_move_assignable_v<sed::Thing>);
                    // applyThingSubset callbacks preserve the Thing vector's
                    // topology, so every captured index remains addressable.
                    for (auto& [index, thing] : snap.things)
                    {
                        current_.level.things[index] = std::move(thing);
                    }
                    current_.level.curId = snap.curId;
                    swapSelection(current_.selection, snap.selection);
                }
                else if constexpr (std::is_same_v<S, LightsSnapshot>)
                {
                    current_.level.lights = std::move(snap.lights);
                    restoreBakedLighting(std::move(snap.bakedLighting));
                    current_.level.curId = snap.curId;
                    swapSelection(current_.selection, snap.selection);
                }
                else if constexpr (std::is_same_v<S, CogsSnapshot>)
                {
                    current_.level.cogs = std::move(snap.cogs);
                    current_.level.importedJklCogRowsPresent =
                        snap.importedJklCogRowsPresent;
                    current_.level.importedJklSurfaceRefs =
                        std::move(snap.importedJklSurfaceRefs);
                    current_.level.curId = snap.curId;
                    swapSelection(current_.selection, snap.selection);
                }
                else if constexpr (std::is_same_v<S, ThingsLightsSnapshot>)
                {
                    current_.level.things = std::move(snap.things);
                    current_.level.motsArchLighting =
                        std::move(snap.motsArchLighting);
                    current_.level.motsArchLightingBakeBaselines =
                        std::move(snap.motsArchLightingBakeBaselines);
                    current_.level.lights = std::move(snap.lights);
                    restoreBakedLighting(std::move(snap.bakedLighting));
                    current_.level.curId = snap.curId;
                    swapSelection(current_.selection, snap.selection);
                }
                else if constexpr (std::is_same_v<S, SectorSubsetSnapshot>)
                {
                    for (auto& [index, sector] : snap.sectors)
                    {
                        if (index < current_.level.sectors.size())
                        {
                            current_.level.sectors[index] = std::move(sector);
                        }
                    }
                    current_.level.curId = snap.curId;
                    swapSelection(current_.selection, snap.selection);
                }
                else if constexpr (std::is_same_v<S, SurfaceSubsetSnapshot>)
                {
                    for (auto& [index, sector] : snap.sectors)
                    {
                        if (index < current_.level.sectors.size())
                        {
                            current_.level.sectors[index] = std::move(sector);
                        }
                    }
                    current_.level.curId = snap.curId;
                    swapSelection(current_.selection, snap.selection);
                }
                else
                {
                    // Compact Thing/geometry snapshots are bidirectional swap
                    // records handled directly by undo()/redo().
                }
            },
            std::move(snapshot));
    }

    DocumentState current_{};
    std::vector<Record> undo_{};
    std::vector<Record> redo_{};
    std::uint64_t nextRecordId_{1};
};

class Document
{
public:
    Document() = default;
    explicit Document(DocumentState initial) : history_(std::move(initial))
    {
    }

    const DocumentState& current() const noexcept
    {
        return history_.current();
    }

    DocumentState& current() noexcept
    {
        return history_.current();
    }

    void clear(DocumentState initial = {})
    {
        history_.clear(std::move(initial));
    }

    // Publish an already prepared document without allocating or relocating
    // unordered Selection storage. Level member addresses remain stable.
    void swap(Document& other) noexcept
    {
        history_.swap(other.history_);
    }

    bool canUndo() const noexcept
    {
        return history_.canUndo();
    }

    bool canRedo() const noexcept
    {
        return history_.canRedo();
    }

    std::size_t undoDepth() const noexcept
    {
        return history_.undoDepth();
    }

    std::size_t redoDepth() const noexcept
    {
        return history_.redoDepth();
    }

    std::uint64_t undoSignature() const noexcept
    {
        return history_.undoSignature();
    }

    std::uint64_t redoSignature() const noexcept
    {
        return history_.redoSignature();
    }

    [[nodiscard]] DocumentUndoSnapshotStats undoSnapshotStats() const noexcept
    {
        return history_.snapshotStats();
    }

    std::string_view nextUndoLabel() const noexcept
    {
        return history_.nextUndoLabel();
    }

    std::string_view nextRedoLabel() const noexcept
    {
        return history_.nextRedoLabel();
    }

    std::vector<std::string> undoLabels() const
    {
        return history_.undoLabels();
    }

    std::vector<std::string> redoLabels() const
    {
        return history_.redoLabels();
    }

    [[nodiscard]] std::optional<DocumentHistoryTransitionState>
    transitionAtDepth(const std::size_t targetDepth) const
    {
        return history_.transitionAtDepth(targetDepth);
    }

    [[nodiscard]] std::optional<std::uint64_t> nextUndoRecordId() const noexcept
    {
        return history_.nextUndoRecordId();
    }

    [[nodiscard]] std::optional<std::uint64_t> nextRedoRecordId() const noexcept
    {
        return history_.nextRedoRecordId();
    }

    [[nodiscard]] std::optional<std::uint64_t> historyRecordIdAtDepth(
        const std::size_t targetDepth) const noexcept
    {
        return history_.historyRecordIdAtDepth(targetDepth);
    }

    [[nodiscard]] std::optional<DocumentHistoryTransitionIdentity>
    historyTransitionIdentityAtDepth(const std::size_t targetDepth) const noexcept
    {
        return history_.historyTransitionIdentityAtDepth(targetDepth);
    }

    DocumentRecoveryHistory recoveryHistory() const
    {
        return history_.recoveryHistory();
    }

    DocumentRecoveryHistory compactRecoveryHistory() const
    {
        return history_.compactRecoveryHistory();
    }

    template <typename Visitor>
    bool visitRecoveryHistory(Visitor&& visitor) const
    {
        return history_.visitRecoveryHistory(std::forward<Visitor>(visitor));
    }

    template <typename Visitor>
    bool visitRecoveryHistoryTransitions(Visitor&& visitor) const
    {
        return history_.visitRecoveryHistoryTransitions(
            std::forward<Visitor>(visitor));
    }

    void restoreRecoveryHistory(
        DocumentState current,
        DocumentRecoveryHistory recovery)
    {
        history_.restoreRecoveryHistory(std::move(current), std::move(recovery));
    }

    void pushUndo(std::string label)
    {
        history_.pushUndo(std::move(label), static_cast<std::uint32_t>(DocumentDirty::None));
    }

    void pushUndo(std::string label, const DocumentDirty dirty)
    {
        history_.pushUndo(std::move(label), static_cast<std::uint32_t>(dirty));
    }

    void pushThingSubsetUndo(
        std::string label,
        const DocumentDirty dirty,
        const std::vector<std::int32_t>& thingIndices)
    {
        history_.pushThingSubsetUndo(
            std::move(label),
            static_cast<std::uint32_t>(dirty),
            thingIndices);
    }

    void pushThingTailUndo(std::string label, const DocumentDirty dirty)
    {
        history_.pushThingTailUndo(
            std::move(label),
            static_cast<std::uint32_t>(dirty));
    }

    void pushSectorSubsetUndo(
        std::string label,
        const DocumentDirty dirty,
        const std::vector<std::int32_t>& sectorIndices)
    {
        history_.pushSectorSubsetUndo(
            std::move(label),
            static_cast<std::uint32_t>(dirty),
            sectorIndices);
    }

    void pushSurfaceSubsetUndo(
        std::string label,
        const DocumentDirty dirty,
        const std::vector<std::int32_t>& touchedSectorIndices)
    {
        history_.pushSurfaceSubsetUndo(
            std::move(label),
            static_cast<std::uint32_t>(dirty),
            touchedSectorIndices);
    }

    bool touchLatestUndoRecord() noexcept
    {
        return history_.touchLatestUndoRecord();
    }

    template <typename Fn>
    bool applyUndoable(std::string label, Fn&& mutate)
    {
        return history_.apply(std::move(label), static_cast<std::uint32_t>(DocumentDirty::None), std::forward<Fn>(mutate));
    }

    template <typename Fn>
    bool applyUndoable(std::string label, const DocumentDirty dirty, Fn&& mutate)
    {
        return history_.apply(std::move(label), static_cast<std::uint32_t>(dirty), std::forward<Fn>(mutate));
    }

    template <typename Fn>
    bool applyThingSubsetUndoable(std::string label,
        const std::vector<std::int32_t>& thingIndices, Fn&& mutate)
    {
        return history_.applyThingSubset(std::move(label), thingIndices, std::forward<Fn>(mutate));
    }

    bool undo(
        DocumentDirty* outDirty = nullptr,
        std::vector<std::int32_t>* outThingSubsetIndices = nullptr)
    {
        std::uint32_t dirty{};
        if (!history_.undo(
                outDirty ? &dirty : nullptr,
                outThingSubsetIndices))
        {
            return false;
        }

        if (outDirty)
        {
            *outDirty = static_cast<DocumentDirty>(dirty);
        }

        return true;
    }

    bool redo(
        DocumentDirty* outDirty = nullptr,
        std::vector<std::int32_t>* outThingSubsetIndices = nullptr)
    {
        std::uint32_t dirty{};
        if (!history_.redo(
                outDirty ? &dirty : nullptr,
                outThingSubsetIndices))
        {
            return false;
        }

        if (outDirty)
        {
            *outDirty = static_cast<DocumentDirty>(dirty);
        }

        return true;
    }

    sed::Level& level() noexcept
    {
        return history_.current().level;
    }

    const sed::Level& level() const noexcept
    {
        return history_.current().level;
    }

    sed::editor::Selection& selection() noexcept
    {
        return history_.current().selection;
    }

    const sed::editor::Selection& selection() const noexcept
    {
        return history_.current().selection;
    }

private:
    sed::editor::DocumentUndoHistory history_{};
};

} // namespace sed::editor

inline std::size_t sed::editor::DocumentRecoveryHistory::undoEntryCount() const noexcept
{
    return compact ? compact->undoDepth() : undoEntries.size();
}

inline std::size_t sed::editor::DocumentRecoveryHistory::redoEntryCount() const noexcept
{
    return compact ? compact->redoDepth() : redoEntries.size();
}
