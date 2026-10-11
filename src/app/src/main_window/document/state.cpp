#include "main_window/common.hpp"

#include "app_logging.hpp"
#include "ui/resources/resource_authoring_editor.hpp"

#include <QCryptographicHash>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>

namespace sed
{

namespace
{

QByteArray resourceNativeBaselineHash(const QCborMap& state)
{
    QCborMap projection{};
    for (const QString& key : {
             QStringLiteral("kind"),
             QStringLiteral("profile"),
             QStringLiteral("name"),
             QStringLiteral("sprite"),
             QStringLiteral("spritePendingNumericEdits"),
             QStringLiteral("rows"),
             QStringLiteral("capacity"),
             QStringLiteral("sourceKind"),
             QStringLiteral("path"),
             QStringLiteral("projectResource"),
             QStringLiteral("draftIdentity") })
    {
        if (state.contains(key))
        {
            projection.insert(key, state.value(key));
        }
    }
    return QCryptographicHash::hash(
        QCborValue(projection).toCbor(), QCryptographicHash::Sha256);
}

QCborArray vectorToCbor(const TVector& vector)
{
    return QCborArray{ vector.x, vector.y, vector.z };
}

QByteArray cleaveAuthoredProjectionHash(const GlView& view, bool* hasDraft)
{
    if ( hasDraft )
    {
        *hasDraft = false;
    }
    QCborArray families{};
    // Storage ownership and the currently displayed family do not change the
    // authored projection. Borrow each settled draft without copying timelines.
    for ( const GlView::CleaveWorkspaceFamily family : {
        GlView::CleaveWorkspaceFamily::Sector,
        GlView::CleaveWorkspaceFamily::Surface } )
    {
        const GlView::RecoveryState::CleaveDraftView draft = view.stagedCleaveDraftView(family);
        if ( hasDraft )
        {
            *hasDraft = *hasDraft || !draft.lines.empty();
        }
        QCborMap projection{};
        if (draft.lines.empty())
        {
            projection.insert(QStringLiteral("hasDraft"), false);
        }
        else
        {
            QCborArray lines{};
            for (const GlView::RecoveryState::CleaveLine& line : draft.lines)
            {
                lines.append(QCborArray{
                    vectorToCbor(line.first), vectorToCbor(line.second),
                    vectorToCbor(line.planeNormal),
                    vectorToCbor(line.constructionPlaneNormal),
                    vectorToCbor(line.angleReferenceAxis),
                    line.linkedToPrevious, line.linkedToNext,
                });
            }
            QCborArray order{};
            for (const std::int32_t index : draft.applyOrder)
            {
                order.append(index);
            }
            QCborArray targetSectors{};
            for (const std::int32_t sector : draft.context->targetSectors)
            {
                targetSectors.append(sector);
            }
            QCborArray targetSurfaces{};
            for (const SurfaceRef& surface : draft.context->targetSurfaces)
            {
                targetSurfaces.append(QCborArray{ surface.sector, surface.surface });
            }
            QCborValue origin{};
            if (draft.chainOrigin)
            {
                origin = QCborArray{
                    draft.chainOrigin->lineIndex, draft.chainOrigin->endpoint };
            }
            projection.insert(QStringLiteral("hasDraft"), true);
            projection.insert(QStringLiteral("lines"), lines);
            projection.insert(QStringLiteral("context"), QCborArray{
                draft.context->valid, draft.context->surfaceMode,
                draft.context->sourceSurface.sector,
                draft.context->sourceSurface.surface,
                targetSectors, targetSurfaces,
            });
            projection.insert(QStringLiteral("origin"), origin);
            projection.insert(QStringLiteral("applyOrder"), order);
            projection.insert(QStringLiteral("bounded"), view.cleaveWorkspaceBounded(family));
        }
        families.append(std::move(projection));
    }
    return QCryptographicHash::hash(
        QCborValue(families).toCbor(), QCryptographicHash::Sha256);
}

QString statusProjectTypeShortLabel(const ProjectType kind)
{
    switch (kind)
    {
    case ProjectType::JKDF2:
        return QStringLiteral("JKDF2");
    case ProjectType::MOTS:
        return QStringLiteral("MOTS");
    case ProjectType::IJIM:
        return QStringLiteral("IJIM");
    default:
        return QStringLiteral("SED");
    }
}

QString statusProjectTypeFullLabel(const ProjectType kind)
{
    switch (kind)
    {
    case ProjectType::JKDF2:
        return QStringLiteral("Jedi Knight: Dark Forces II");
    case ProjectType::MOTS:
        return QStringLiteral("Mysteries of the Sith");
    case ProjectType::IJIM:
        return QStringLiteral("Indiana Jones and the Infernal Machine");
    default:
        return QStringLiteral("Unknown project type");
    }
}

QString statusProjectTypeStyle()
{
    return QStringLiteral(
        "QLabel#status_project_type {"
        " color: #dde4ec;"
        " background-color: #30343a;"
        " border: 1px solid #4a515a;"
        " border-radius: 3px;"
        " padding: 2px 8px;"
        " font-weight: 650;"
        "}");
}

} // namespace

bool MainWindow::hasUnsavedChanges() const noexcept
{
    refreshCleaveAuthoredDirtyCache();
    // Keep the title/update hot path O(1). Same-undo in-place Level changes
    // are checked at replacement/recovery boundaries; resource panes advance
    // a small authored generation from their edit callback. Neither path
    // serializes a Level or walks raw resource rows during a title refresh.
    return recoveryRequiresSave_
        || document_.undoSignature() != savedUndoSignature_
        || resourceEditorAuthoredGeneration_ != recoverySavedResourceEditorGeneration_
        || recoveryCleaveAuthoredDirty_;
}

void MainWindow::refreshCleaveAuthoredDirtyCache() const noexcept
{
    if (!glView_)
    {
        recoveryCurrentCleaveAuthoredHash_.clear();
        recoveryCleaveAuthoredRevision_ = 0;
        recoveryCleaveAuthoredCacheValid_ = true;
        recoveryCleaveAuthoredDirty_ = recoverySavedCleaveAuthoredHashKnown_
            && !recoverySavedCleaveAuthoredHash_.isEmpty();
        return;
    }

    const std::uint64_t revision = glView_->stagedCleaveAuthoredRevision();
    if (recoveryCleaveAuthoredCacheValid_
        && recoveryCleaveAuthoredRevision_ == revision)
    {
        return;
    }

    try
    {
        bool hasDraft = false;
        recoveryCurrentCleaveAuthoredHash_ = cleaveAuthoredProjectionHash(
            *glView_, &hasDraft);
        recoveryCleaveAuthoredRevision_ = revision;
        recoveryCleaveAuthoredCacheValid_ = true;
        recoveryCleaveAuthoredDirty_ = recoverySavedCleaveAuthoredHashKnown_
            ? recoveryCurrentCleaveAuthoredHash_ != recoverySavedCleaveAuthoredHash_
            : hasDraft;
    }
    catch (...)
    {
        recoveryCurrentCleaveAuthoredHash_.clear();
        recoveryCleaveAuthoredRevision_ = revision;
        recoveryCleaveAuthoredCacheValid_ = true;
        recoveryCleaveAuthoredDirty_ = true;
    }
}

QByteArray MainWindow::currentCleaveAuthoredStateHash() const noexcept
{
    refreshCleaveAuthoredDirtyCache();
    return recoveryCurrentCleaveAuthoredHash_;
}

bool MainWindow::hasUnsavedAuthoredLevelChanges() const
{
    refreshCleaveAuthoredDirtyCache();
    if (recoveryCleaveAuthoredDirty_)
    {
        return true;
    }
    if (recoveryRequiresSave_ || document_.undoSignature() != savedUndoSignature_)
    {
        return true;
    }
    if (!recoverySavedLevelHashKnown_)
    {
        return recoveryAutomaticBaselineEnabled_;
    }
    try
    {
        const QByteArray currentLevelHash = recoveryCurrentLevelStateHash();
        return currentLevelHash.isEmpty()
            || currentLevelHash != recoverySavedLevelStateHash_;
    }
    catch (...)
    {
        return true;
    }
}

void MainWindow::refreshWindowTitle()
{
    if (currentPath_.empty())
    {
        setWindowTitle(QStringLiteral("Untitled*"));
        updateProjectTypeStatusLabel();
        return;
    }

    const QString levelPath = QDir::toNativeSeparators(QString::fromStdWString(currentPath_.wstring()));
    const QString dirtySuffix = hasUnsavedChanges() ? QStringLiteral("*") : QString{};
    setWindowTitle(levelPath + dirtySuffix);
    updateProjectTypeStatusLabel();
}

void MainWindow::updateProjectTypeStatusLabel()
{
    if (!projectTypeStatusLabel_)
    {
        return;
    }

    const ProjectType kind = document_.level().kind;
    projectTypeStatusLabel_->setText(statusProjectTypeShortLabel(kind));
    projectTypeStatusLabel_->setToolTip(statusProjectTypeFullLabel(kind));
    projectTypeStatusLabel_->setStyleSheet(statusProjectTypeStyle());
    updateLegacyUvCompatibilityStatusLabel();
}

void MainWindow::markCurrentDocumentAsSaved()
{
    savedUndoSignature_ = document_.undoSignature();
    recoveryRequiresSave_ = false;
    recoveryAuthoredMutationPending_ = false;
    recoveryAuthoredLevelDirty_ = false;
    refreshWindowTitle();
}

void MainWindow::markRecoveryWorkspaceAsSaved()
{
    try
    {
        recoverySavedLevelStateHash_ = recoveryCurrentLevelStateHash();
        recoverySavedLevelHashKnown_ = !recoverySavedLevelStateHash_.isEmpty();
        if (glView_)
        {
            recoverySavedCleaveAuthoredHash_ = cleaveAuthoredProjectionHash(
                *glView_, nullptr);
            recoverySavedCleaveAuthoredHashKnown_ =
                !recoverySavedCleaveAuthoredHash_.isEmpty();
            recoveryCurrentCleaveAuthoredHash_ = recoverySavedCleaveAuthoredHash_;
            recoveryCleaveAuthoredRevision_ = glView_->stagedCleaveAuthoredRevision();
            recoveryCleaveAuthoredCacheValid_ = true;
            recoveryCleaveAuthoredDirty_ = false;
        }
        else
        {
            recoverySavedCleaveAuthoredHash_.clear();
            recoverySavedCleaveAuthoredHashKnown_ = false;
            recoveryCurrentCleaveAuthoredHash_.clear();
            recoveryCleaveAuthoredRevision_ = 0;
            recoveryCleaveAuthoredCacheValid_ = true;
            recoveryCleaveAuthoredDirty_ = false;
        }
        markRecoveryCogWorkspaceAsSaved();
        for (const QPointer<QWidget>& page : resourceEditorPages())
        {
            if (page)
            {
                page->setProperty(
                    "resource_authoring.nativeBaselineHash",
                    resourceNativeBaselineHash(
                        ui::resources::captureResourceAuthoringPaneState(page.data())));
            }
        }
        recoverySavedResourceEditorState_ = captureAuthoredResourceEditorRecoveryState();
        recoverySavedResourceEditorStateHash_ = QCryptographicHash::hash(
            recoverySavedResourceEditorState_, QCryptographicHash::Sha256);
        recoverySavedResourceEditorGeneration_ = resourceEditorAuthoredGeneration_;
    }
    catch (...)
    {
        // A missing baseline is conservative for persistence: the next
        // explicit editor mutation can still be checkpointed, while a Save
        // or document-open failure must never escape through title refresh.
        recoverySavedLevelStateHash_.clear();
        recoverySavedLevelHashKnown_ = false;
        recoverySavedCleaveAuthoredHash_.clear();
        recoverySavedCleaveAuthoredHashKnown_ = false;
        recoveryCurrentCleaveAuthoredHash_.clear();
        recoveryCleaveAuthoredCacheValid_ = false;
        recoveryCleaveAuthoredDirty_ = true;
        recoverySavedCogState_.clear();
        recoverySavedCogStateHash_.clear();
        recoverySavedResourceEditorState_.clear();
        recoverySavedResourceEditorStateHash_.clear();
        recoverySavedResourceEditorGeneration_ = resourceEditorAuthoredGeneration_ == 0
            ? 1 : resourceEditorAuthoredGeneration_ - 1;
    }
}

void MainWindow::markRecoveryCogWorkspaceAsSaved()
{
    recoverySavedCogState_ = captureAuthoredCogRecoveryState();
    recoverySavedCogStateHash_ = QCryptographicHash::hash(
        recoverySavedCogState_, QCryptographicHash::Sha256);
}

void MainWindow::captureReplacementAuthorizationFingerprint()
{
    replacementAuthorizationLevelHash_ = recoveryCurrentLevelStateHash();
    replacementAuthorizationCleaveHash_ = currentCleaveAuthoredStateHash();
    replacementAuthorizationCogHash_ = QCryptographicHash::hash(
        captureAuthoredCogRecoveryState(), QCryptographicHash::Sha256);
    replacementAuthorizationResourceHash_ = QCryptographicHash::hash(
        captureAuthoredResourceEditorRecoveryState(), QCryptographicHash::Sha256);
    replacementAuthorizationFingerprintValid_ =
        !replacementAuthorizationLevelHash_.isEmpty()
        && !replacementAuthorizationCleaveHash_.isEmpty()
        && !replacementAuthorizationCogHash_.isEmpty()
        && !replacementAuthorizationResourceHash_.isEmpty();
}

bool MainWindow::replacementAuthorizationStillCurrent() const
{
    if (!replacementAuthorizationFingerprintValid_)
    {
        return true;
    }
    const QByteArray currentLevelHash = recoveryCurrentLevelStateHash();
    const QByteArray currentCleaveHash = currentCleaveAuthoredStateHash();
    const QByteArray currentCogHash = QCryptographicHash::hash(
        captureAuthoredCogRecoveryState(), QCryptographicHash::Sha256);
    const QByteArray currentResourceHash = QCryptographicHash::hash(
        captureAuthoredResourceEditorRecoveryState(), QCryptographicHash::Sha256);
    return !currentLevelHash.isEmpty()
        && !currentCleaveHash.isEmpty()
        && !currentCogHash.isEmpty()
        && !currentResourceHash.isEmpty()
        && currentLevelHash == replacementAuthorizationLevelHash_
        && currentCleaveHash == replacementAuthorizationCleaveHash_
        && currentCogHash == replacementAuthorizationCogHash_
        && currentResourceHash == replacementAuthorizationResourceHash_;
}

void MainWindow::clearReplacementAuthorizationFingerprint() noexcept
{
    replacementAuthorizationLevelHash_.clear();
    replacementAuthorizationCleaveHash_.clear();
    replacementAuthorizationCogHash_.clear();
    replacementAuthorizationResourceHash_.clear();
    replacementAuthorizationFingerprintValid_ = false;
}

bool MainWindow::promptSaveIfUnsaved(
    const QString &actionDescription,
    const std::filesystem::path& preservedRecoveryPath,
    QString* deferredDiscardIdentity,
    QString* deferredDiscardSessionId)
{
    if (!hasUnsavedAuthoredLevelChanges())
    {
        return true;
    }

    const QString message = QString("The current level has unsaved changes.\nDo you want to save "
                                    "before %1?")
                                .arg(actionDescription.trimmed().isEmpty() ? QString("continuing") : actionDescription);

    QMessageBox prompt(
        QMessageBox::Warning,
        QStringLiteral("Unsaved Changes"),
        message,
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        this);
    prompt.setDefaultButton(QMessageBox::Save);
    prompt.setEscapeButton(QMessageBox::Cancel);
    const QString levelIdentity = currentPath_.empty()
        ? QStringLiteral("Untitled")
        : QDir::toNativeSeparators(
            QString::fromStdWString(currentPath_.wstring()));
    const QString requestedAction = actionDescription.trimmed().isEmpty()
        ? QStringLiteral("continuing")
        : actionDescription.trimmed();
    prompt.setProperty(kMessageBoxLoggingContextProperty,
        QStringLiteral("Level: %1; Requested operation: %2")
            .arg(levelIdentity, requestedAction));
    const QMessageBox::StandardButton choice =
        static_cast<QMessageBox::StandardButton>(prompt.exec());

    if (choice == QMessageBox::Cancel)
    {
        return false;
    }

    if (choice == QMessageBox::Discard)
    {
        // This is an intentional abandonment of the current document, not a
        // crash. Retire only the live automatic offer. Periodic `.sedautosave`
        // entries are durable user history and remain available after a
        // Discard; only Autosave History Delete or retention pruning removes
        // them. Open/restore callers defer live retirement until replacement
        // commits, so a failed replacement cannot destroy the current offer.
        if (deferredDiscardIdentity)
        {
            *deferredDiscardIdentity = recoveryIdentity_;
            if (deferredDiscardSessionId)
            {
                *deferredDiscardSessionId = recoverySessionId_;
            }
        }
        else
        {
            discardRecoveryFamily(
                recoveryIdentity_, recoverySessionId_, preservedRecoveryPath);
        }
        return true;
    }

    QAction *saveAction = findChild<QAction *>("file.save");
    if (!saveAction)
    {
        return false;
    }

    const std::filesystem::path previousProtectedPath =
        recoveryProtectedPathDuringSave_;
    recoveryProtectedPathDuringSave_ = preservedRecoveryPath;
    const auto protectedPathGuard = qScopeGuard(
        [this, previousProtectedPath]()
        {
            recoveryProtectedPathDuringSave_ = previousProtectedPath;
        });
    // This save is part of the replacement authorization transaction.  It
    // must serialize the complete native document (Level + EDIT + HIST),
    // even when the focused widget is a COG editor whose ordinary Ctrl+S
    // shortcut intentionally saves only that script.  Keep the normal
    // shortcut routing untouched by scoping this override to the QAction
    // invocation below.
    const bool previousForceNativeSave = forceNativeDocumentSave_;
    forceNativeDocumentSave_ = true;
    const auto forceNativeSaveGuard = qScopeGuard(
        [this, previousForceNativeSave]()
        {
            forceNativeDocumentSave_ = previousForceNativeSave;
        });
    saveAction->trigger();
    return !hasUnsavedAuthoredLevelChanges();
}

bool MainWindow::authorizeDocumentReplacement(
    const QString& actionDescription,
    const std::filesystem::path& preservedRecoveryPath,
    QString* deferredDiscardIdentity,
    QString* deferredDiscardSessionId)
{
    clearReplacementAuthorizationFingerprint();
    if (deferredDiscardIdentity) deferredDiscardIdentity->clear();
    if (deferredDiscardSessionId) deferredDiscardSessionId->clear();
    if (!promptSaveIfUnsaved(
            actionDescription,
            preservedRecoveryPath,
            deferredDiscardIdentity,
            deferredDiscardSessionId))
    {
        return false;
    }

    const bool levelDiscardIntent = deferredDiscardIdentity
        && !deferredDiscardIdentity->isEmpty();
    // Capture the state immediately after the user's Level Discard choice.
    // COG close confirmation below may pump the event loop; a mutation after
    // this point is newer work and must cancel replacement.
    QByteArray discardLevelHash{};
    QByteArray discardCleaveHash{};
    if (levelDiscardIntent)
    {
        discardLevelHash = recoveryCurrentLevelStateHash();
        discardCleaveHash = currentCleaveAuthoredStateHash();
    }

    bool discardedCogChanges = false;
    if (!confirmCloseCogScriptEditors(&discardedCogChanges))
    {
        // Any Level Discard recorded above is still only a deferred intent.
        // Cancelling a later COG prompt therefore leaves the current document
        // and its complete Level+EDIT checkpoint family untouched.
        if (deferredDiscardIdentity) deferredDiscardIdentity->clear();
        if (deferredDiscardSessionId) deferredDiscardSessionId->clear();
        return false;
    }
    const QByteArray authorizedCogState = captureAuthoredCogRecoveryState();
    if (discardedCogChanges && deferredDiscardIdentity
        && deferredDiscardIdentity->isEmpty())
    {
        *deferredDiscardIdentity = recoveryIdentity_;
        if (deferredDiscardSessionId)
        {
            *deferredDiscardSessionId = recoverySessionId_;
        }
    }
    bool discardedResourceChanges = false;
    if (!confirmCloseResourceEditors(&discardedResourceChanges))
    {
        if (deferredDiscardIdentity) deferredDiscardIdentity->clear();
        if (deferredDiscardSessionId) deferredDiscardSessionId->clear();
        return false;
    }
    if (captureAuthoredCogRecoveryState() != authorizedCogState)
    {
        if (deferredDiscardIdentity) deferredDiscardIdentity->clear();
        if (deferredDiscardSessionId) deferredDiscardSessionId->clear();
        return false;
    }
    if (discardedResourceChanges && deferredDiscardIdentity
        && deferredDiscardIdentity->isEmpty())
    {
        *deferredDiscardIdentity = recoveryIdentity_;
        if (deferredDiscardSessionId)
        {
            *deferredDiscardSessionId = recoverySessionId_;
        }
    }
    // Both the native Save and COG close confirmation pump the Qt event loop.
    // A plugin/timer may author a new mutation after the user authorized the
    // prompt but before replacement commits.  An explicit Level Discard is
    // allowed to leave the old Level dirty; compare the post-discard
    // transaction instead, and refuse only if a newer mutation arrived while
    // the COG confirmation was running.
    if (levelDiscardIntent)
    {
        const QByteArray currentLevelHash = recoveryCurrentLevelStateHash();
        const QByteArray currentCleaveHash = currentCleaveAuthoredStateHash();
        if (discardLevelHash.isEmpty()
            || currentLevelHash.isEmpty()
            || discardCleaveHash.isEmpty()
            || currentLevelHash != discardLevelHash
            || currentCleaveHash != discardCleaveHash)
        {
            return false;
        }
        // Restore the deferred intent after validation; replacement callers
        // retire it only after their new document commits.
        if (deferredDiscardIdentity)
        {
            *deferredDiscardIdentity = recoveryIdentity_;
            if (deferredDiscardSessionId)
            {
                *deferredDiscardSessionId = recoverySessionId_;
            }
        }
        captureReplacementAuthorizationFingerprint();
        return replacementAuthorizationFingerprintValid_;
    }
    // confirmCloseCogScriptEditors has already obtained an explicit
    // Save/Discard decision for each modified COG tab.  A discarded editor
    // intentionally remains text-modified until the replacement commits; do
    // not mistake that authorized state for a new mutation.  If no tab was
    // resolved, a modified buffer means replacement was not authorized.
    if (hasUnsavedAuthoredLevelChanges()
        || (!discardedCogChanges && hasUnsavedCogScriptChanges())
        || (!discardedResourceChanges && hasUnsavedResourceEditorChanges()))
    {
        return false;
    }
    captureReplacementAuthorizationFingerprint();
    return replacementAuthorizationFingerprintValid_;
}

} // namespace sed
