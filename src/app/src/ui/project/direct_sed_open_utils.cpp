#include "ui/project/direct_sed_open_utils.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "ui/project/project_startup_utils.hpp"
#include "ui/project/sed_editor_file_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

namespace sed::ui::project
{
namespace
{

bool containsSection(
    const std::vector<std::string>& sections,
    const std::string_view section)
{
    return std::find(sections.begin(), sections.end(), section) != sections.end();
}

void appendMissingSection(
    std::vector<std::string>& sections,
    const std::string_view section)
{
    if (!containsSection(sections, section))
    {
        sections.emplace_back(section);
    }
}

QString sectionList(const std::vector<std::string>& sections)
{
    QStringList values{};
    values.reserve(static_cast<qsizetype>(sections.size()));
    for (const std::string& section : sections)
    {
        values.push_back(QString::fromStdString(section));
    }
    return values.join(QStringLiteral(", "));
}

} // namespace

DirectSedOpenResult loadDirectSedDocument(const std::filesystem::path& path)
{
    DirectSedOpenResult result{};
    std::vector<assets::SedOptionalSection> optionalSections{};

    try
    {
        assets::SedFile file = assets::loadSedFile(path);
        result.level = std::move(file.level);
        result.documentTimestamp = file.timestamp;
        optionalSections = std::move(file.optionalSections);
        result.hadCorruption = file.hadCorruption;
        result.repairedChunkCount = file.repairedChunks;
        result.repairedSections = std::move(file.repairedSections);
        result.unreadableRanges = std::move(file.unreadableRanges);
    }
    catch (const std::runtime_error& strictFailure)
    {
        // The corruption-aware decoder is itself the classification boundary:
        // it defaults/omits only damage proven by checked framing, CRC, parity,
        // decompression, or strong-hash evidence. Checksum-valid required
        // schema/programming failures throw again and remain hard errors.
        result.strictFailure = QString::fromUtf8(strictFailure.what());
        assets::SedRecoveryLoadResult recovered = assets::recoverSedFile(path);
        result.usedRecoveryFallback = true;
        result.level = std::move(recovered.level);
        result.documentTimestamp = recovered.timestamp;
        optionalSections = std::move(recovered.optionalSections);
        result.hadCorruption = recovered.hadCorruption || result.usedRecoveryFallback;
        result.repairedChunkCount = recovered.repairedChunks;
        result.repairedSections = std::move(recovered.repairedSections);
        result.missingRequiredSections = std::move(recovered.missingRequiredSections);
        result.missingOptionalSections = std::move(recovered.missingOptionalSections);
        result.unidentifiedMissingSectionCount = recovered.unidentifiedMissingSections;
        result.unreadableRanges = std::move(recovered.unreadableRanges);
        result.projectTypeKnown = recovered.projectTypeTrusted
            && recovered.projectType.has_value();
        if (result.projectTypeKnown)
        {
            if (containsSection(result.missingRequiredSections, "head"))
            {
                // The fixed header supplies authoritative project identity,
                // but every other HEAD field was lost. Replace only that
                // irrecoverable domain with safe project defaults.
                applyNewProjectTypeDefaults(
                    result.level, *recovered.projectType);
            }
            else
            {
                result.level.kind = *recovered.projectType;
            }
        }
    }

    result.editorStateSectionPresent = std::any_of(
        optionalSections.begin(), optionalSections.end(),
        [](const assets::SedOptionalSection& section)
        {
            return section.type == SedEditorStateSection;
        });
    result.historySectionPresent = std::any_of(
        optionalSections.begin(), optionalSections.end(),
        [](const assets::SedOptionalSection& section)
        {
            return section.type == SedHistorySection;
        });
    result.editorStatePayload = findSedOptionalSection(
        optionalSections, SedEditorStateSection);
    result.historyPayload = findSedOptionalSection(
        optionalSections, SedHistorySection);

    if (!result.editorStateSectionPresent)
    {
        appendMissingSection(result.missingOptionalSections, "edit");
    }
    if (!result.historySectionPresent)
    {
        appendMissingSection(result.missingOptionalSections, "hist");
        if (!result.editorStatePayload.isEmpty())
        {
            QString attachmentWarning{};
            result.editorStatePayload =
                recovery::omitEditorWorkspaceHistoryAttachments(
                    result.editorStatePayload, &attachmentWarning);
            result.workspaceHistory = {};
            if (!attachmentWarning.isEmpty())
            {
                result.historyWarning = attachmentWarning;
            }
        }
    }
    else if (result.projectTypeKnown)
    {
        QString historyError{};
        QString workspaceWarning{};
        QByteArray validatedEditorState{};
        result.history = recovery::deserializeEditorDocumentHistoryForRestoreWithWorkspaceBindings(
            result.historyPayload,
            result.level.kind,
            result.level,
            result.editorStatePayload,
            &validatedEditorState,
            &historyError,
            &workspaceWarning,
            &result.workspaceHistory,
            &recovery::validateWorkspaceHistoryAttachmentForLevels);
        if (!validatedEditorState.isEmpty())
        {
            result.editorStatePayload = std::move(validatedEditorState);
        }
        if (!workspaceWarning.isEmpty())
        {
            result.historyWarning = workspaceWarning;
        }
        if (!result.history)
        {
            // HIST is optional workspace history, never authority for the
            // Level. A checksum-valid but semantically invalid history must
            // degrade to an empty timeline instead of aborting an otherwise
            // valid direct open.
            appendMissingSection(result.missingOptionalSections, "hist");
            result.historyPayload.clear();
            result.historyWarning = historyError.isEmpty()
                ? QStringLiteral("HIST semantic validation failed")
                : QStringLiteral("HIST semantic validation failed: %1")
                    .arg(historyError);
            if (!workspaceWarning.isEmpty())
            {
                result.historyWarning += QStringLiteral(" %1")
                    .arg(workspaceWarning);
            }
        }
    }

    // If checked descriptor copies no longer identify a lost unit, it may
    // have been one of the required Level domains. Treat that uncertainty as
    // required loss: remaining associated would let Save overwrite the only
    // forensic source with a potentially partial document.
    result.detachFromSource = !result.missingRequiredSections.empty()
        || result.unidentifiedMissingSectionCount != 0;
    result.requiresCleanSave = result.hadCorruption
        || result.usedRecoveryFallback
        || !result.missingRequiredSections.empty()
        || !result.missingOptionalSections.empty()
        || result.unidentifiedMissingSectionCount != 0
        || !result.unreadableRanges.empty();
    return result;
}

QStringList describeDirectSedRecovery(const DirectSedOpenResult& result)
{
    QStringList warnings{};
    if (!result.projectTypeKnown)
    {
        warnings.push_back(QStringLiteral(
            "Both checked fixed headers and the required HEAD section were lost. The in-memory project kind is only a decoder placeholder; an explicit project choice is required before applying or exporting this recovery."));
    }
    if (result.usedRecoveryFallback)
    {
        warnings.push_back(result.strictFailure.isEmpty()
            ? QStringLiteral(
                "Strict SED validation failed; checked corruption recovery reconstructed the surviving document.")
            : QStringLiteral(
                "Strict SED validation reported '%1'; checked corruption recovery reconstructed the surviving document.")
                .arg(result.strictFailure));
    }
    if (!result.historyWarning.isEmpty())
    {
        warnings.push_back(QStringLiteral(
            "%1. The authoritative Level was retained, undo/redo history was dropped, and a clean save is required.")
            .arg(result.historyWarning));
    }

    if (result.repairedChunkCount != 0)
    {
        const QString repaired = sectionList(result.repairedSections);
        warnings.push_back(QStringLiteral(
            "Repaired %1 invalid checked SED slot(s) in %2.")
            .arg(static_cast<qulonglong>(result.repairedChunkCount))
            .arg(repaired.isEmpty() ? QStringLiteral("file metadata") : repaired));
    }
    else if (!result.repairedSections.empty())
    {
        warnings.push_back(QStringLiteral(
            "Recovered damaged redundant checked framing for SED sections: %1.")
            .arg(sectionList(result.repairedSections)));
    }
    else if (result.hadCorruption && !result.usedRecoveryFallback
        && result.unreadableRanges.empty())
    {
        warnings.push_back(QStringLiteral(
            "Checked SED framing or redundant metadata was repaired transparently."));
    }

    if (!result.missingRequiredSections.empty())
    {
        warnings.push_back(QStringLiteral(
            "Missing required Level sections: %1. Each lost domain was defaulted as a whole; surviving references may be dangling.")
            .arg(sectionList(result.missingRequiredSections)));
    }
    if (!result.missingOptionalSections.empty())
    {
        warnings.push_back(QStringLiteral(
            "Missing optional document sections: %1. SED will use safe workspace defaults and/or an empty undo timeline.")
            .arg(sectionList(result.missingOptionalSections)));
    }
    if (result.unidentifiedMissingSectionCount != 0)
    {
        warnings.push_back(QStringLiteral(
            "%1 damaged SED section unit(s) could not be identified from surviving checked framing. An unidentified unit may conceal required Level data, so this recovery cannot remain associated with the source.")
            .arg(static_cast<qulonglong>(
                result.unidentifiedMissingSectionCount)));
    }
    if (!result.unreadableRanges.empty())
    {
        QStringList ranges{};
        ranges.reserve(static_cast<qsizetype>(result.unreadableRanges.size()));
        for (const assets::SedUnreadableRange& range : result.unreadableRanges)
        {
            ranges.push_back(QStringLiteral("offset %1, %2 byte(s)")
                .arg(static_cast<qulonglong>(range.offset))
                .arg(static_cast<qulonglong>(range.length)));
        }
        warnings.push_back(QStringLiteral(
            "Persistent storage read failures were zero-filled and treated as erasures: %1.")
            .arg(ranges.join(QStringLiteral("; "))));
    }
    if (result.requiresCleanSave)
    {
        warnings.push_back(result.detachFromSource
            ? QStringLiteral(
                "The partial Level is detached from the damaged source and must be reviewed and saved to a new file.")
            : QStringLiteral(
                "The document is marked modified so the next Save publishes a clean protected copy."));
    }
    return warnings;
}

} // namespace sed::ui::project
