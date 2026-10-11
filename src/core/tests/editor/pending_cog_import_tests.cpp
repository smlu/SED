#include "test_common.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"

namespace sed::tests
{

void runPendingCogImportTests()
{
    editor::DocumentState initial{};
    initial.level.importedJklCogRowsPresent = true;
    initial.level.importedJklSurfaceRefs = {
        SurfaceRef{ 0, 0 },
        SurfaceRef{ 1, 0 },
    };
    CogValue rawValue{};
    rawValue.name = "param0";
    rawValue.type = ValueType::str;
    rawValue.cogType = CogType::unk;
    rawValue.stringValue = "17";
    initial.level.cogs.push_back(
        Cog{ 71, 0, "defaults.cog", { rawValue } });

    editor::Document document(std::move(initial));
    document.pushUndo(
        "Finalize imported COG defaults", editor::DocumentDirty::Cogs);
    const editor::DocumentUndoSnapshotStats firstSnapshotStats =
        document.undoSnapshotStats();
    check(firstSnapshotStats.cogs == 1u && firstSnapshotStats.full == 0u,
        "COG-only import edit uses a compact CogsSnapshot");
    CogValue typedValue{};
    typedValue.name = "count";
    typedValue.type = ValueType::integer;
    typedValue.cogType = CogType::int_;
    typedValue.intValue = 17;
    document.current().level.cogs[0].values[0] = typedValue;
    document.current().level.importedJklCogRowsPresent = false;
    document.current().level.importedJklSurfaceRefs.clear();

    editor::DocumentDirty dirty{};
    const bool undone = document.undo(&dirty);
    const Level& undoLevel = document.current().level;
    const bool undoRestoresPendingImport = undone
        && dirty == editor::DocumentDirty::Cogs
        && undoLevel.importedJklCogRowsPresent
        && undoLevel.importedJklSurfaceRefs
            == std::vector<SurfaceRef>{ SurfaceRef{ 0, 0 }, SurfaceRef{ 1, 0 } }
        && undoLevel.cogs.size() == 1u
        && undoLevel.cogs[0].values.size() == 1u
        && undoLevel.cogs[0].values[0].name == "param0"
        && undoLevel.cogs[0].values[0].type == ValueType::str
        && undoLevel.cogs[0].values[0].cogType == CogType::unk
        && undoLevel.cogs[0].values[0].stringValue == "17";
    check(undoRestoresPendingImport,
        "COG-only Undo restores the JKL hydration marker, exact ordered surface map, and raw parameter row");
    const editor::DocumentUndoSnapshotStats afterUndoSnapshotStats =
        document.undoSnapshotStats();
    check(afterUndoSnapshotStats.cogs == 1u
            && afterUndoSnapshotStats.full == 0u,
        "COG-only Undo retains the one compact CogsSnapshot on the redo stack");

    const bool redone = document.redo(&dirty);
    const Level& redoLevel = document.current().level;
    check(redone
            && dirty == editor::DocumentDirty::Cogs
            && !redoLevel.importedJklCogRowsPresent
            && redoLevel.importedJklSurfaceRefs.empty()
            && redoLevel.cogs.size() == 1u
            && redoLevel.cogs[0].values.size() == 1u
            && redoLevel.cogs[0].values[0].name == "count"
            && redoLevel.cogs[0].values[0].type == ValueType::integer
            && redoLevel.cogs[0].values[0].intValue == 17,
        "COG-only Redo restores the hydrated typed row and clears only its import metadata");
    const editor::DocumentUndoSnapshotStats finalSnapshotStats =
        document.undoSnapshotStats();
    check(finalSnapshotStats.cogs == 1u && finalSnapshotStats.full == 0u,
        "COG-only Redo retains the one compact CogsSnapshot on the undo stack");

    Level importBaseline{};
    importBaseline.importedJklCogRowsPresent = false;
    const std::vector<std::byte> nestedBaseline = assets::saveToSedBytes(
        importBaseline, {}, assets::SedSerializationProfile::HistoryNested);
    assets::SedNestedHistoryDecoder decoder(importBaseline);
    decoder.advance(nestedBaseline,
        assets::SedJklCogImportState{ true, {} });
    const std::vector<std::byte> acceptedBytes(
        decoder.currentBytes().begin(), decoder.currentBytes().end());
    const assets::SedNestedHistoryDecodeStats acceptedStats = decoder.lastStats();

    // The COG import sidecar is installed into the candidate before the
    // nested footer is checked.  A corrupt footer must roll that late failure
    // back together with the required Level and checked-byte cache.
    std::vector<std::byte> lateFailureBytes = nestedBaseline;
    lateFailureBytes.back() ^= std::byte{ 0x01 };
    bool rejectedLateFailure = false;
    try
    {
        decoder.advance(std::move(lateFailureBytes),
            assets::SedJklCogImportState{ false, {} });
    }
    catch (const std::exception&)
    {
        rejectedLateFailure = true;
    }
    const Level& retainedPrevious = decoder.previousLevel();
    const bool importSidecarRollbackExact = rejectedLateFailure
        && decoder.currentLevel().importedJklCogRowsPresent
        && decoder.currentLevel().importedJklSurfaceRefs.empty()
        && !retainedPrevious.importedJklCogRowsPresent
        && retainedPrevious.importedJklSurfaceRefs.empty()
        && decoder.lastStats().decodedRequiredSections
            == acceptedStats.decodedRequiredSections
        && decoder.lastStats().reusedRequiredSections
            == acceptedStats.reusedRequiredSections
        && decoder.lastStats().transactionallyReplacedRequiredSections
            == acceptedStats.transactionallyReplacedRequiredSections
        && decoder.lastStats().resourceSectionChanged
            == acceptedStats.resourceSectionChanged
        && decoder.lastStats().usedTransactionalDomainReuse
            == acceptedStats.usedTransactionalDomainReuse
        && std::equal(acceptedBytes.begin(), acceptedBytes.end(),
            decoder.currentBytes().begin(), decoder.currentBytes().end());
    check(importSidecarRollbackExact,
        "late HistoryNested footer failure rolls back a changed JKL COG import sidecar and retains accepted Level bytes");
}

} // namespace sed::tests
