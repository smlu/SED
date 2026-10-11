#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

#include <QByteArray>
#include <QCborValue>
#include <QString>
#include <QStringList>

#include "sed/editor/document.hpp"
#include "sed/assets/sed.hpp"

namespace sed::ui::recovery
{

// The compact recovery codec is also built without MainWindow. Callers that
// own the authoritative Level-aware EDIT validator pass it through this seam
// before historical workspace attachments can be published.
using WorkspaceHistoryAttachmentLevelValidator = bool (*)(
    const QCborValue& cleaveAttachment,
    const QCborValue& layerVisualsAttachment,
    const Level* beforeLevel,
    const Level* afterLevel,
    QString* error);

// Wire-format admission limit. This is intentionally independent of the live
// GUI undo limit: backend DTO readers/writers may preserve a deeper portable
// timeline, while Document trims an installed interactive timeline to its own
// configured maximum.
inline constexpr std::size_t MaximumPersistedHistoryEntries = 100'000;

struct RecoveryCborLimits
{
    std::uint64_t maximumInputBytes{};
    std::uint64_t maximumNodes{};
    int maximumDepth{};
    std::uint64_t maximumTextBytes{};
    std::uint64_t maximumByteStringBytes{};
    // Conservative peak bytes for the decoded QCborValue graph plus the
    // canonical verification buffer. Admission also scales this allowance
    // from the encoded input so small payloads cannot amplify to this cap.
    std::uint64_t maximumMaterializedBytes{};
};

// Recovery CBOR has one deterministic wire representation. Admission first
// walks the encoded graph with bounded scratch storage, then materializes only
// an already-bounded single value and verifies its deterministic encoding.
QByteArray serializeCanonicalCbor(const QCborValue& value);
std::optional<QCborValue> deserializeCanonicalCbor(
    const QByteArray& payload,
    const RecoveryCborLimits& limits,
    QString* error = nullptr);

// Canonical authored-Level digest shared by the GUI's saved-baseline
// comparison and the recovery writer. Workspace-only layer visibility is
// excluded; represented JKL COG import provenance is conditionally bound.
QByteArray authoredLevelStateHash(const Level& level);

struct PublicationGate
{
    std::atomic<std::uint64_t> generation{0};
    std::mutex publishMutex{};
};

// A recovery snapshot can lose every native project discriminator (the
// metadata chunk, fixed SED headers, and HEAD section) while still retaining
// healthy geometry.  Keep the project kind value separate from its trust
// state so the JKDF2 enum value used for storage/default construction can
// never silently become an authoritative fallback for MotS or IJIM data.
enum class ProjectTypeTrust : std::uint8_t
{
    unknown,
    metadata,
    native,
    user,
};

struct RecoveryMetadata
{
    QString identity{};
    QString sessionId{};
    QString sourcePath{};
    QString displayName{};
    qint64 timestamp{};
    qint64 sourceModifiedAtMs{-1};
    std::uint64_t undoSignature{};
    // A live clean baseline is still useful after a hard process kill, but it
    // must not be presented as "unsaved changes" on the next launch.  Only a
    // checkpoint that contains authored Level/COG changes is eligible for an
    // automatic recovery offer. Historical autosaves are likewise rotated
    // only from offer-eligible authored checkpoints.
    bool requiresUserRecovery{};
    ProjectType projectType{ProjectType::JKDF2};
    ProjectTypeTrust projectTypeTrust{ProjectTypeTrust::unknown};
    // SHA-256 digests bind the protected metadata to the exact native Level
    // document.  The required-Level digest remains verifiable when optional
    // editor sections are lost; the complete-document digest additionally
    // detects EDIT/HIST transplantation in an otherwise valid checkpoint.
    QByteArray levelSha256{};
    QByteArray documentSha256{};

    bool projectTypeKnown() const noexcept
    {
        return projectTypeTrust != ProjectTypeTrust::unknown;
    }
};

struct RecoveryUnreadableRange
{
    std::uint64_t offset{};
    std::uint64_t size{};

    constexpr bool operator==(const RecoveryUnreadableRange&) const noexcept = default;
};

using RecoveryRangeReader = std::function<bool(
    std::uint64_t offset,
    std::span<std::byte> destination)>;

struct RecoveryContainerBytes
{
    QByteArray bytes{};
    std::vector<RecoveryUnreadableRange> unreadableRanges{};
};

struct WorkspaceHistoryAttachment
{
    editor::DocumentRecoveryHistoryStack stack{
        editor::DocumentRecoveryHistoryStack::Undo};
    std::size_t index{};
    QCborValue cleave{};
    QCborValue layerVisuals{};
};

struct WorkspaceHistoryState
{
    std::vector<WorkspaceHistoryAttachment> attachments{};
    std::optional<std::size_t> pendingRedoIndex{};
};

struct RecoveryRecord
{
    RecoveryMetadata metadata{};
    std::int64_t documentTimestamp{};
    QByteArray levelPayload{};
    // Native DOCM validation already materializes the authoritative Level.
    // Retain that checked result so the consuming recovery transaction can
    // move it directly instead of decoding the same potentially large DOCM a
    // second time. Copies of RecoveryRecord share this logically immutable
    // handoff; const load helpers copy the Level, while the rvalue document
    // loader moves it only when uniquely owned.
    std::shared_ptr<Level> decodedLevel{};
    std::optional<ProjectType> nativeProjectType{};
    bool nativeProjectTypeTrusted{};
    QByteArray editorStatePayload{};
    // Kept only when project kind was unknown at container parse time.  The
    // UI decodes this HIST payload after the user selects JKDF2, MotS, or
    // IJIM; it is never interpreted using the JKDF2 storage default.
    QByteArray historyPayload{};
    std::optional<sed::editor::DocumentRecoveryHistory> history{};
    WorkspaceHistoryState workspaceHistory{};
    QStringList warnings{};
    // Structured semantic section loss. UI warnings remain explanatory text,
    // never the source of truth for salvage policy.
    QStringList missingSections{};
    std::vector<RecoveryUnreadableRange> unreadableRanges{};
    // True when one or more independently protected binary SED sections could
    // not be reconstructed and the reader loaded the remaining healthy data.
    bool partialRecovery{};
    bool corrupted{};
    std::uint32_t repairedNativeChunkCount{};
    std::uint32_t repairedMetadataShardCount{};
    std::uint32_t lostSectionCount{};
    // Native framing was too damaged to recover the section identifier.  Such
    // a unit may contain Level data, so callers must keep the result detached
    // even when every named missing section is optional editor state.
    std::uint32_t unidentifiedMissingSectionCount{};
    std::filesystem::path recoveryPath{};
};

struct RecoveryDescriptor
{
    RecoveryMetadata metadata{};
    std::filesystem::path recoveryPath{};
};

struct WriteRequest
{
    std::filesystem::path recoveryPath{};
    RecoveryMetadata metadata{};
    sed::editor::Document document{};
    // Standard RFC 8949 CBOR assembled on the GUI thread from small editor and
    // viewport values. It remains opaque to the background container writer.
    QByteArray editorStatePayload{};
    // Manual Save/Open baseline used by the worker after it has produced the
    // canonical authored-Level digest. This keeps full Level serialization
    // out of the GUI-thread recovery scheduler.
    QByteArray savedLevelStateHash{};
    std::shared_ptr<PublicationGate> publicationGate{};
    std::uint64_t generation{};
    // Periodic authored-data checkpoints preserve the preceding eligible
    // dirty live slot as a separately browsable history entry before replacing it.
    bool archivePreviousSnapshot{};
    std::filesystem::path archiveSourcePath{};
    std::size_t maximumHistoricalSnapshots{10};
#ifdef SED_APP_RECOVERY_TEST_ACCESS
    std::function<bool()> archivePublicationAllowedForTest{};
    std::function<bool()> liveCommitAllowedForTest{};
#endif
};

struct RecoveredDocument
{
    sed::editor::DocumentState current{};
    std::int64_t documentTimestamp{};
    std::optional<sed::editor::DocumentRecoveryHistory> history{};
    QByteArray editorStatePayload{};
    WorkspaceHistoryState workspaceHistory{};
    QStringList warnings{};
    QStringList missingSections{};
    std::vector<RecoveryUnreadableRange> unreadableRanges{};
};

enum class WriteStatus : std::uint8_t
{
    published,
    superseded,
    failed,
};

struct WriteResult
{
    WriteStatus status{WriteStatus::failed};
    std::filesystem::path recoveryPath{};
    QString error{};
    QString warning{};
    // Canonical authored Level digest (workspace-only lvis excluded),
    // calculated by the background writer and reused by the GUI scheduler.
    QByteArray levelStateHash{};
};

QString newSessionId();
QString normalizedSourcePath(const std::filesystem::path& sourcePath);
QString documentIdentity(const std::filesystem::path& sourcePath, const QString& sessionId);
std::filesystem::path recoveryFilePath(
    const std::filesystem::path& recoveryDirectory,
    const QString& identity,
    const QString& sessionId,
    const QString& displayName,
    ProjectType projectType);
std::filesystem::path recoveryFilePath(
    const std::filesystem::path& recoveryDirectory,
    const std::filesystem::path& sourcePath,
    const QString& sessionId,
    const QString& displayName,
    ProjectType projectType);
qint64 sourceModifiedAtMs(const std::filesystem::path& sourcePath);

// Injectable 4 KiB block reader used after an ordinary whole-file read
// reports an I/O error. Failed blocks are retried independently, then
// zero-filled erasures are returned so later native sections remain readable.
std::optional<RecoveryContainerBytes> readRecoveryContainerResilient(
    std::uint64_t fileSize,
    const RecoveryRangeReader& reader,
    std::uint32_t blockBytes = 4096,
    std::uint32_t maximumAttempts = 3,
    QString* error = nullptr);

WriteResult writeRecoverySnapshot(WriteRequest request);
std::optional<RecoveryRecord> readRecoveryRecord(
    const std::filesystem::path& recoveryPath,
    QString* error = nullptr,
    WorkspaceHistoryAttachmentLevelValidator attachmentValidator = nullptr);
std::optional<RecoveryDescriptor> readRecoveryDescriptor(
    const std::filesystem::path& recoveryPath,
    QString* error = nullptr);
std::vector<RecoveryDescriptor> scanRecoveries(const std::filesystem::path& recoveryDirectory);
// Returns only bounded periodic `.sedautosave` history entries. Automatic
// startup/open offers deliberately use scanRecoveries() and never walk these
// historical backups.
std::vector<RecoveryDescriptor> scanAutosaveHistory(
    const std::filesystem::path& recoveryDirectory);
// Bounded maintenance scan for one session-qualified family. The filename
// digest filter is applied before metadata decoding, so unrelated/clean files
// cannot consume the family's retention or explicit-discard budget.
std::vector<RecoveryDescriptor> scanRecoveryFamily(
    const std::filesystem::path& recoveryDirectory,
    const QString& identity,
    const QString& sessionId);
bool recoveryPathsEqual(
    const std::filesystem::path& lhs,
    const std::filesystem::path& rhs);
// `.sedautosave` entries are durable user history. They must not be removed
// by live-session retirement, recovery replacement, or normal shutdown. Keep
// this suffix policy shared by UI callers and the low-level delete boundary.
bool isAutosaveHistoryPath(const std::filesystem::path& path) noexcept;
bool isLiveRecoveryPath(const std::filesystem::path& path) noexcept;

enum class RecoveryRemovalReason : std::uint8_t
{
    liveRetirement,
    explicitHistoryDelete,
    retentionPrune,
};
bool isRecoveryNewerThanSource(const RecoveryMetadata& metadata);
bool recoverySourceChanged(const RecoveryMetadata& metadata);
std::optional<Level> loadRecoveredLevel(const RecoveryRecord& record, QString* error = nullptr);
std::optional<RecoveredDocument> loadRecoveredDocument(
    const RecoveryRecord& record,
    QString* error = nullptr,
    WorkspaceHistoryAttachmentLevelValidator attachmentValidator = nullptr);
// The consuming overload moves the potentially large decoded HIST graph while
// deliberately leaving metadata/diagnostics in `record` available to the
// recovery transaction and audit UI.
std::optional<RecoveredDocument> loadRecoveredDocument(
    RecoveryRecord&& record,
    QString* error = nullptr,
    WorkspaceHistoryAttachmentLevelValidator attachmentValidator = nullptr);
QByteArray serializeSelection(const editor::Selection& selection);
std::optional<editor::Selection> deserializeSelection(
    const QByteArray& payload,
    QString* error = nullptr);

// Shared by crash recovery and the full-state Sith Editor document format.
// The Document overload streams portable states into the semantic-section
// pool one at a time and is preferred by writers: it avoids retaining a
// second vector containing every expanded historical Level. The DTO overload
// requires the exact top-level current Level explicitly; it is useful for
// tests and callers that already own decoded history without duplicating that
// Level inside the DTO.
QByteArray serializeEditorDocumentHistory(
    const editor::Document& document);
/**
 * Manual-Save variant. The caller must seed `document.current().level` with
 * the exact normalized top-level Level that will be written. Historical Thing
 * frame counts are canonicalized while walking a temporary history copy so
 * Undo/Redo cannot restore stale derived `numframes` values.
 */
QByteArray serializeEditorDocumentHistoryForSave(
    const editor::Document& document);

// Workspace-history attachments name exact positions in the compact HIST
// stream. These variants bind their adjacent HistoryNested SHA-256 values in
// the same stream walk and return a patched canonical EDIT payload. The input
// remains unchanged; on failure the output EDIT is cleared.
QByteArray serializeEditorDocumentHistoryWithWorkspaceBindings(
    const editor::Document& document,
    const QByteArray& editorStatePayload,
    QByteArray* boundEditorStatePayload,
    QString* warning = nullptr);
QByteArray serializeEditorDocumentHistoryForSaveWithWorkspaceBindings(
    const editor::Document& document,
    const QByteArray& editorStatePayload,
    QByteArray* boundEditorStatePayload,
    QString* warning = nullptr);

// Remove only optional historical workspace attachments after HIST has been
// omitted or semantically degraded. The current EDIT draft remains intact.
QByteArray omitEditorWorkspaceHistoryAttachments(
    const QByteArray& editorStatePayload,
    QString* warning = nullptr);

// Import HIST and validate optional positional attachments against the exact
// HistoryNested states reconstructed by the compact checked reader. Invalid
// optional attachments are removed from `validatedEditorStatePayload` while
// the independently valid HIST timeline is still returned. A caller that
// does not provide the application-level Level validator gets the same safe
// degradation: attachments are dropped with a warning, while current EDIT
// and HIST remain available.
std::optional<editor::DocumentRecoveryHistory>
deserializeEditorDocumentHistoryForRestoreWithWorkspaceBindings(
    const QByteArray& historyPayload,
    ProjectType projectType,
    const Level& currentLevel,
    const QByteArray& editorStatePayload,
    QByteArray* validatedEditorStatePayload,
    QString* historyError = nullptr,
    QString* workspaceWarning = nullptr,
    WorkspaceHistoryState* workspaceHistory = nullptr,
    WorkspaceHistoryAttachmentLevelValidator attachmentValidator = nullptr);

// App recovery.cpp reuses the normal EDIT Cleave grammar to validate a
// historical attachment against the pre/post Levels held by the stream
// decoder. Null pointers mean that side of the directional stream was already
// checked before or after an incremental Level advance.
bool validateWorkspaceHistoryAttachmentForLevels(
    const QCborValue& cleaveAttachment,
    const QCborValue& layerVisualsAttachment,
    const Level* beforeLevel,
    const Level* afterLevel,
    QString* error = nullptr);

namespace detail
{
struct RecoveryHistoryEncodeStats
{
    std::uint64_t reusedSectionFrames{};
    std::uint64_t fragmentedSectionFrames{};
    // Bytes that were actually hashed/interned for regenerated frames.  A
    // frame may be regenerated with a different owner while unchanged
    // fixed-offset blocks reuse the preceding references, so this is not the
    // full regenerated frame size.
    std::uint64_t fragmentedSectionBytes{};
    std::array<std::uint64_t, 8> reusedFramesBySection{};
    std::array<std::uint64_t, 8> fragmentedFramesBySection{};
    std::array<std::uint64_t, 8> fragmentedBytesBySection{};
    // Writer diagnostics for the hybrid structural-frame path. These are
    // direct prior-span bytes, not bytes routed through referenceFor().
    std::array<std::uint64_t, 8> resynchronizedPrefixBytesBySection{};
    std::array<std::uint64_t, 8> resynchronizedSuffixBytesBySection{};
    // Writer diagnostics for the semantic SECT path.  These are bytes whose
    // complete Sector-sized fragment was already present in the base/pool;
    // unlike fragmentedSectionBytes, they were not inserted again for the
    // changed frame.
    std::array<std::uint64_t, 8> recordReusedBytesBySection{};
    // Subset of recordReusedBytesBySection that reused the prior state's
    // same-index references after an in-place byte comparison.  This avoids
    // hashing or allocating unchanged records during a geometry edit.
    std::array<std::uint64_t, 8> directRecordReusedBytesBySection{};
};

// Structural test/profile hook. It executes the same manual-Save encoder and
// reports generated-frame reuse without changing the persisted HIST schema.
QByteArray serializeEditorDocumentHistoryForSaveWithStats(
    const editor::Document& document,
    RecoveryHistoryEncodeStats* stats);
// Structural test hook for the ordinary (non-normalizing) history writer.
// Keeping this separate from the manual-Save hook prevents derived Thing
// frame normalization from changing the semantic state under comparison.
QByteArray serializeEditorDocumentHistoryWithStatsForTesting(
    const editor::Document& document,
    RecoveryHistoryEncodeStats* stats);
std::vector<QByteArray> generatedHistoryFragmentsForTesting(
    const Level& level);
} // namespace detail

QByteArray serializeEditorDocumentHistory(
    const Level& currentLevel,
    const editor::DocumentRecoveryHistory& history);
std::optional<editor::DocumentRecoveryHistory> deserializeEditorDocumentHistory(
    const QByteArray& payload,
    ProjectType projectType,
    const Level& currentLevel,
    QString* error = nullptr);

// Consuming/native restore path. Unlike the inspection helper above, this
// rebuilds the opaque compact undo representation used by live SED and
// recovery documents. Production open code must use this overload so a large
// unchanged Level is not retained once for every historical transform.
std::optional<editor::DocumentRecoveryHistory>
deserializeEditorDocumentHistoryForRestore(
    const QByteArray& payload,
    ProjectType projectType,
    const Level& currentLevel,
    QString* error = nullptr);

// Invalidate every outstanding writer before deleting the published snapshot.
// The short mutex section closes the race between a worker's final generation
// check and QSaveFile's atomic commit. The default reason is live retirement;
// callers deleting durable `.sedautosave` history must opt into either the
// explicit History Delete action or post-publication retention pruning.
bool invalidateAndRemoveRecovery(
    const std::shared_ptr<PublicationGate>& publicationGate,
    const std::filesystem::path& recoveryPath,
    QString* error = nullptr,
    const RecoveryMetadata* expectedMetadata = nullptr,
    RecoveryRemovalReason reason = RecoveryRemovalReason::liveRetirement);

} // namespace sed::ui::recovery
