#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

namespace sed::ui::project
{

/**
 * Fully classified result of opening a native SED document directly.
 *
 * The loader always attempts strict native validation first. Corruption-aware
 * recovery is entered only after that strict native load throws; required
 * schema errors remain hard because the recovery codec does not classify
 * checksum-valid semantic failures as storage damage.
 */
struct DirectSedOpenResult
{
    Level level{};
    std::int64_t documentTimestamp{};
    QByteArray editorStatePayload{};
    QByteArray historyPayload{};
    std::optional<editor::DocumentRecoveryHistory> history{};
    ui::recovery::WorkspaceHistoryState workspaceHistory{};

    bool editorStateSectionPresent{};
    bool historySectionPresent{};
    bool projectTypeKnown{true};
    bool usedRecoveryFallback{};
    bool hadCorruption{};
    bool requiresCleanSave{};
    bool detachFromSource{};

    std::uint32_t repairedChunkCount{};
    std::vector<std::string> repairedSections{};
    std::vector<std::string> missingRequiredSections{};
    std::vector<std::string> missingOptionalSections{};
    std::uint32_t unidentifiedMissingSectionCount{};
    std::vector<assets::SedUnreadableRange> unreadableRanges{};
    QString strictFailure{};
    QString historyWarning{};
};

/** Strict-first direct `.sed` loading with an integrity-only recovery fallback. */
DirectSedOpenResult loadDirectSedDocument(const std::filesystem::path& path);

/** User-facing, structured descriptions of every repair or loss classification. */
QStringList describeDirectSedRecovery(const DirectSedOpenResult& result);

} // namespace sed::ui::project
