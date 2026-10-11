#include "main_window/common.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <QCryptographicHash>
#include <QFile>

namespace sed
{

std::optional<std::filesystem::path> MainWindow::preferredExtractionRoot() const
{
    auto pickRoot = [](const std::vector<std::filesystem::path>& roots) -> std::optional<std::filesystem::path>
    {
        if (roots.empty())
        {
            return std::nullopt;
        }

        for (const std::filesystem::path& root : roots)
        {
            if (sed::text::iequalsAscii(root.filename().string(), "resource"))
            {
                return root;
            }
        }

        return roots.front();
    };

    if (const auto root = pickRoot(resourceRoots_))
    {
        return root;
    }

    return pickRoot(resources_.roots());
}

std::optional<std::int32_t> MainWindow::createPlacedCogFromValues(
    const std::string& scriptName,
    const std::vector<CogValue>& values,
    const bool selectNewCog)
{
    if (scriptName.empty())
    {
        return std::nullopt;
    }
    if ( !document_.level().canAllocateIds(1) )
    {
        constexpr const char* message = "Cannot add COG: entity ID space exhausted";
        if ( glView_ )
        {
            glView_->showToast(
                message,
                3000,
                GlView::ToastPlacement::BottomCenter,
                GlView::ToastGroup::Error);
        }
        if ( statusBar() )
        {
            statusBar()->showMessage(message, 4000);
        }
        return std::nullopt;
    }

    std::optional<std::int32_t> newIndex{};
    const bool applied = document_.applyUndoable(
        "COG: Added",
        editor::DocumentDirty::Cogs | editor::DocumentDirty::Structure,
        [scriptName, values, selectNewCog, &newIndex](editor::DocumentState& state) -> bool
        {
            Cog& cog = state.level.newCog();
            cog.name = scriptName;
            cog.values = values;
            newIndex = cog.num;

            if (selectNewCog)
            {
                state.selection.clear();
                state.selection.cogs.insert(cog.num);
            }

            return true;
        });

    if (!applied)
    {
        return std::nullopt;
    }

    return newIndex;
}

std::optional<std::int32_t> MainWindow::createPlacedCogFromScript(
    const std::string& scriptName,
    const bool selectNewCog)
{
    if (scriptName.empty())
    {
        return std::nullopt;
    }

    const sed::assets::CogSymbolLoadResult loaded =
        sed::assets::loadCogSymbolsWithDefaultsResult(
            resources_, scriptName, false);
    if (!loaded.loaded())
    {
        return std::nullopt;
    }

    return createPlacedCogFromValues(scriptName, loaded.symbols, selectNewCog);
}

std::optional<std::int32_t> MainWindow::pickAndCreatePlacedCog(const bool selectNewCog)
{
    const auto picked = ui::pickCogScript(this, resources_, preferredExtractionRoot());
    if (!picked || picked->scriptName.empty())
    {
        return std::nullopt;
    }

    return createPlacedCogFromScript(picked->scriptName, selectNewCog);
}

namespace
{

std::string normalizedCogScriptName(const std::string_view name)
{
    const std::string_view trimmed = sed::text::trimView(name);
    std::string normalized{};
    normalized.reserve(trimmed.size());
    for (const char ch : trimmed)
    {
        normalized.push_back((ch == '\\') ? '/' : ch);
    }
    return normalized;
}

std::string cogScriptKey(const std::string_view name)
{
    const std::string normalized = normalizedCogScriptName(name);
    if (normalized.empty())
    {
        return {};
    }

    std::string key = sed::text::upperAscii(normalized);
    if (!sed::text::iendsWithAscii(normalized, ".cog"))
    {
        key += ".COG";
    }

    return key;
}

std::wstring normalizedCogScriptWatchPath(
    const std::filesystem::path& path)
{
    return ui::recovery::normalizedSourcePath(path).toStdWString();
}

std::wstring normalizedCogScriptWatchPath(const QString& path)
{
    return normalizedCogScriptWatchPath(
        std::filesystem::path(path.toStdWString()));
}

bool cogScriptWatcherContainsPath(
    const QStringList& watchedPaths,
    const std::filesystem::path& path)
{
    const std::wstring identity = normalizedCogScriptWatchPath(path);
    for (const QString& watchedPath : watchedPaths)
    {
        if (normalizedCogScriptWatchPath(watchedPath) == identity)
        {
            return true;
        }
    }
    return false;
}

std::optional<std::filesystem::path> tryFindCogScriptOnDisk(
    const sed::assets::ResourceLocator& resources,
    const std::string_view scriptName)
{
    const std::string normalized = normalizedCogScriptName(scriptName);
    if (const auto path = resources.findOnDisk(normalized))
    {
        return path;
    }

    if (!sed::text::iendsWithAscii(normalized, ".cog"))
    {
        std::string withExt(normalized);
        withExt += ".cog";
        return resources.findOnDisk(withExt);
    }

    return std::nullopt;
}

std::vector<std::string> cogScriptCandidateNames(
    const sed::assets::ResourceLocator& resources,
    const std::string_view scriptName)
{
    const std::string normalized = normalizedCogScriptName(scriptName);
    std::vector<std::string> candidates = resources.candidates(normalized);
    if (!sed::text::iendsWithAscii(normalized, ".cog"))
    {
        std::string withExt = normalized;
        withExt += ".cog";
        for (const std::string& candidate : resources.candidates(withExt))
        {
            if (std::find(candidates.begin(), candidates.end(), candidate)
                == candidates.end())
            {
                candidates.push_back(candidate);
            }
        }
    }
    return candidates;
}

std::optional<std::filesystem::path> expectedCogScriptPath(
    const sed::assets::ResourceLocator& resources,
    const std::string_view scriptName)
{
    const std::vector<std::string> candidates = cogScriptCandidateNames(
        resources, scriptName);
    if (candidates.empty())
    {
        return std::nullopt;
    }

    const std::filesystem::path firstCandidate(candidates.front());
    if (firstCandidate.is_absolute())
    {
        return firstCandidate;
    }

    for (auto it = resources.roots().rbegin(); it != resources.roots().rend(); ++it)
    {
        return *it / firstCandidate;
    }

    return std::nullopt;
}

std::optional<std::filesystem::path> nearestExistingDirectory(
    std::filesystem::path directory)
{
    while (!directory.empty())
    {
        std::error_code ec{};
        if (std::filesystem::is_directory(directory, ec) && !ec)
        {
            return directory;
        }

        const std::filesystem::path parent = directory.parent_path();
        if (parent == directory)
        {
            break;
        }
        directory = parent;
    }
    return std::nullopt;
}

bool cogValueEqual(const CogValue& lhs, const CogValue& rhs) noexcept
{
    return lhs.name == rhs.name
        && lhs.type == rhs.type
        && lhs.intValue == rhs.intValue
        && lhs.floatValue == rhs.floatValue
        && lhs.stringValue == rhs.stringValue
        && lhs.cogType == rhs.cogType
        && lhs.local == rhs.local
        && lhs.description == rhs.description
        && lhs.mask == rhs.mask;
}

bool cogValuesEqual(
    const std::vector<CogValue>& lhs,
    const std::vector<CogValue>& rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        if (!cogValueEqual(lhs[index], rhs[index]))
        {
            return false;
        }
    }
    return true;
}

bool isRawJklCogValue(const CogValue& value, const std::size_t parameterIndex)
{
    return value.type == ValueType::str
        && value.cogType == CogType::unk
        && value.name == "param" + std::to_string(parameterIndex);
}

void mapImportedJklSurfaceValue(
    Level& level,
    CogValue& value,
    const std::string_view raw)
{
    if (value.type != ValueType::ptr || value.cogType != CogType::surface)
    {
        return;
    }

    const std::string_view trimmed = sed::text::trimView(raw);
    std::optional<std::int32_t> wireIndex =
        sed::text::parseInt<std::int32_t>(trimmed);
    if (!wireIndex)
    {
        if (const auto parsedHex = sed::text::parseHex(trimmed);
            parsedHex
            && *parsedHex <= static_cast<std::uint32_t>(
                std::numeric_limits<std::int32_t>::max()))
        {
            wireIndex = static_cast<std::int32_t>(*parsedHex);
        }
    }
    if (!wireIndex || *wireIndex < 0)
    {
        return;
    }

    if (level.importedJklSurfaceRefs.empty())
    {
        return;
    }
    if (static_cast<std::size_t>(*wireIndex)
        >= level.importedJklSurfaceRefs.size())
    {
        value.intValue = -1;
        value.stringValue.clear();
        return;
    }

    const SurfaceRef imported = level.importedJklSurfaceRefs[
        static_cast<std::size_t>(*wireIndex)];
    const std::int32_t editorIndex = level.getGlobalSurfaceNumber(
        imported.sector, imported.surface);
    if (editorIndex < 0)
    {
        value.intValue = -1;
        value.stringValue.clear();
        return;
    }

    value.intValue = editorIndex;
    value.stringValue.clear();
}

std::vector<CogValue> mergeCogScriptSchema(
    Level& level,
    const Cog& cog,
    const std::vector<CogValue>& symbols)
{
    std::unordered_map<std::string, std::vector<const CogValue*>> oldByName{};
    oldByName.reserve(cog.values.size());
    for (const CogValue& old : cog.values)
    {
        oldByName[sed::text::upperAscii(old.name)].push_back(&old);
    }
    std::unordered_map<std::string, std::size_t> oldNameUseCount{};

    std::vector<CogValue> updated = symbols;
    for (std::size_t index = 0; index < updated.size(); ++index)
    {
        CogValue& next = updated[index];
        const std::string nameKey = sed::text::upperAscii(next.name);
        const auto oldIt = oldByName.find(nameKey);
        const std::size_t oldNameIndex = oldNameUseCount[nameKey]++;
        const CogValue* oldValue = oldIt != oldByName.end()
            && oldNameIndex < oldIt->second.size()
            ? oldIt->second[oldNameIndex]
            : nullptr;
        if (oldValue != nullptr
            && oldValue->type == next.type
            && oldValue->cogType == next.cogType)
        {
            // Keep schema metadata from the script while preserving the
            // placed, authored payload for a matching name and type.
            next.intValue = oldValue->intValue;
            next.floatValue = oldValue->floatValue;
            next.stringValue = oldValue->stringValue;
            continue;
        }

        if (index >= cog.values.size()
            || !isRawJklCogValue(cog.values[index], index))
        {
            continue;
        }

        const std::string raw = cog.values[index].stringValue;
        CogValue resolved = next;
        if (!resolved.parseJed(level, raw) && !resolved.parse(raw))
        {
            resolved.type = ValueType::str;
            resolved.stringValue = raw;
        }
        mapImportedJklSurfaceValue(level, resolved, raw);
        next = std::move(resolved);
    }

    return updated;
}

} // namespace

void MainWindow::syncCogScriptWatches()
{
    if (!cogScriptWatcher_)
    {
        cogScriptWatcher_ = new QFileSystemWatcher(this);
        connect(cogScriptWatcher_, &QFileSystemWatcher::fileChanged, this, [this](const QString& path)
            {
                handleCogScriptFileChanged(path);
            });
        connect(cogScriptWatcher_, &QFileSystemWatcher::directoryChanged, this, [this](const QString& path)
            {
                handleCogScriptDirectoryChanged(path);
            });
    }

    // This fingerprint describes the document/resource lifetime. It must stay
    // stable when an unrelated COG is added or removed in the same document;
    // resolved path checks below handle ordinary watch-map changes.
    std::string scopeIdentity = std::to_string(recoveryDocumentEpoch_);
    scopeIdentity.push_back('\n');
    scopeIdentity += currentPath_.generic_string();
    scopeIdentity.push_back('\n');
    scopeIdentity += std::to_string(resources_.previewCacheIdentity());
    for (const auto& root : resources_.roots())
    {
        scopeIdentity.push_back('\n');
        scopeIdentity += root.generic_string();
    }
    for (const auto& gob : resources_.gobs())
    {
        scopeIdentity.push_back('\n');
        scopeIdentity += gob.path().generic_string();
    }
    const bool watchScopeChanged = scopeIdentity != cogScriptWatchIdentity_;
    if (watchScopeChanged)
    {
        cogScriptWatchIdentity_ = std::move(scopeIdentity);
        ++cogScriptWatchGeneration_;
        // A path from the previous resource/document lifetime is never a
        // valid fallback for a missing script in the new lifetime.
        cogScriptDiskPathByKey_.clear();
        cogScriptDiskPathByIdentity_.clear();
        cogScriptDiagnosticsByPath_.clear();
        cogScriptAppliedContentByPath_.clear();
    }

    const Level& level = document_.level();

    std::unordered_map<std::string, std::filesystem::path> desiredByKey{};
    std::unordered_map<std::wstring, std::filesystem::path> desiredPathByIdentity{};
    std::unordered_set<std::wstring> desiredDirs{};
    std::unordered_map<std::wstring, std::filesystem::path> desiredDirPaths{};

    const auto addDesiredDirectory = [&desiredDirs, &desiredDirPaths](
        const std::filesystem::path& candidate)
    {
        if (const auto directory = nearestExistingDirectory(candidate))
        {
            const std::wstring identity = normalizedCogScriptWatchPath(*directory);
            desiredDirs.emplace(identity);
            desiredDirPaths.emplace(identity, *directory);
        }
    };

    const auto addCandidateDirectories = [this, &addDesiredDirectory](
        const std::string_view scriptName)
    {
        const std::vector<std::string> candidates = cogScriptCandidateNames(
            resources_, scriptName);
        for (const std::string& candidate : candidates)
        {
            const std::filesystem::path candidatePath(candidate);
            if (candidatePath.is_absolute())
            {
                addDesiredDirectory(candidatePath.parent_path());
                continue;
            }

            for (const std::filesystem::path& root : resources_.roots())
            {
                addDesiredDirectory((root / candidatePath).parent_path());
            }
        }
    };

    for (const Cog& cog : level.cogs)
    {
        const std::string_view name = sed::text::trimView(cog.name);
        if (name.empty())
        {
            continue;
        }

        const std::string key = cogScriptKey(name);
        if (key.empty() || desiredByKey.contains(key))
        {
            continue;
        }

        std::optional<std::filesystem::path> path =
            tryFindCogScriptOnDisk(resources_, name);
        if (!path)
        {
            const auto previous = watchScopeChanged
                ? cogScriptDiskPathByKey_.end()
                : cogScriptDiskPathByKey_.find(key);
            if (!watchScopeChanged && previous != cogScriptDiskPathByKey_.end())
            {
                path = previous->second;
            }
            else
            {
                path = expectedCogScriptPath(resources_, name);
            }
        }
        if (!path)
        {
            continue;
        }

        // The key suppresses duplicate separator aliases.  The resolved disk
        // identity below remains separate so distinct logical names that the
        // resolver maps to one file refresh together without flattening paths.
        desiredByKey.emplace(key, *path);
        const std::wstring pathIdentity = normalizedCogScriptWatchPath(*path);
        desiredPathByIdentity.emplace(pathIdentity, *path);
        addDesiredDirectory(path->parent_path());
        addCandidateDirectories(name);
    }

    // Remove stale watches.
    if (cogScriptWatcher_)
    {
        const QStringList watchedFiles = cogScriptWatcher_->files();
        for (const QString& qPath : watchedFiles)
        {
            const std::wstring wPath = normalizedCogScriptWatchPath(qPath);
            if (!desiredPathByIdentity.contains(wPath))
            {
                cogScriptWatcher_->removePath(qPath);
            }
        }

        const QStringList watchedDirs = cogScriptWatcher_->directories();
        for (const QString& qDir : watchedDirs)
        {
            const std::wstring wDir = normalizedCogScriptWatchPath(qDir);
            if (!desiredDirs.contains(wDir))
            {
                cogScriptWatcher_->removePath(qDir);
            }
        }

        // Add missing watches.
        for (const auto& [identity, path] : desiredPathByIdentity)
        {
            (void)identity;
            const QString qPath = QString::fromStdWString(path.wstring());
            std::error_code ec{};
            const bool exists = std::filesystem::is_regular_file(path, ec) && !ec;
            if (exists
                && !cogScriptWatcherContainsPath(
                    cogScriptWatcher_->files(), path))
            {
                cogScriptWatcher_->addPath(qPath);
            }
        }

        for (const auto& [wDir, directory] : desiredDirPaths)
        {
            if (!cogScriptWatcherContainsPath(
                    cogScriptWatcher_->directories(),
                    directory))
            {
                (void)wDir;
                cogScriptWatcher_->addPath(
                    QString::fromStdWString(directory.wstring()));
            }
        }
    }

    // A path fingerprint is valid only while the same logical COG key is
    // resolved to that path.  Clear the selected path when priority changes
    // (for example low -> high -> low) so a fallback schema is hydrated again;
    // unchanged selections retain their fingerprint for duplicate-notification
    // and undo protection.
    for (const auto& [key, path] : desiredByKey)
    {
        const auto previous = cogScriptDiskPathByKey_.find(key);
        if (previous == cogScriptDiskPathByKey_.end()
            || normalizedCogScriptWatchPath(previous->second)
                != normalizedCogScriptWatchPath(path))
        {
            cogScriptAppliedContentByPath_.erase(
                normalizedCogScriptWatchPath(path));
        }
    }

    cogScriptDiskPathByKey_ = std::move(desiredByKey);
    cogScriptDiskPathByIdentity_ = std::move(desiredPathByIdentity);
}

void MainWindow::handleCogScriptFileChanged(const QString& path)
{
    if (path.isEmpty())
    {
        return;
    }

    const std::wstring wPath = normalizedCogScriptWatchPath(path);
    if (!cogScriptDiskPathByIdentity_.contains(wPath))
    {
        return;
    }

    const std::uint64_t watchGeneration = cogScriptWatchGeneration_;
    const std::uint64_t documentEpoch = recoveryDocumentEpoch_;

    // Delay to accommodate editors that save via "atomic rename" (remove+replace).
    QTimer::singleShot(250, this, [this, path, watchGeneration, documentEpoch]()
        {
            if (watchGeneration != cogScriptWatchGeneration_
                || documentEpoch != recoveryDocumentEpoch_)
            {
                return;
            }

            const std::wstring currentPath = normalizedCogScriptWatchPath(path);
            if (!cogScriptDiskPathByIdentity_.contains(currentPath))
            {
                return;
            }
            applyCogScriptFileChangedNow(path, watchGeneration, documentEpoch);
        });
}

void MainWindow::refreshCogScriptFileNow(const QString& path)
{
    if (path.isEmpty())
    {
        return;
    }
    const std::wstring wPath = normalizedCogScriptWatchPath(path);
    if (!cogScriptDiskPathByIdentity_.contains(wPath))
    {
        return;
    }
    applyCogScriptFileChangedNow(
        path, cogScriptWatchGeneration_, recoveryDocumentEpoch_);
}

void MainWindow::applyCogScriptFileChangedNow(
    const QString& path,
    const std::uint64_t watchGeneration,
    const std::uint64_t documentEpoch)
{
    if (watchGeneration != cogScriptWatchGeneration_
        || documentEpoch != recoveryDocumentEpoch_)
    {
        return;
    }

    const std::wstring wPath = normalizedCogScriptWatchPath(path);
    if (!cogScriptDiskPathByIdentity_.contains(wPath))
    {
        return;
    }

    const std::filesystem::path fsPath(path.toStdWString());
    std::error_code ec{};
    const bool exists = std::filesystem::is_regular_file(fsPath, ec) && !ec;
    if (!exists)
    {
        // Keep the in-memory values and the directory watch. The consistency
        // checker will report the missing resource; the editor never silently
        // rewrites a designer's deleted source file.
        scheduleRebuildChecks(editor::DocumentDirty::Cogs);
        return;
    }

    if (cogScriptWatcher_)
    {
        if (!cogScriptWatcherContainsPath(
                cogScriptWatcher_->files(), fsPath))
        {
            cogScriptWatcher_->addPath(
                QString::fromStdWString(fsPath.wstring()));
        }
    }

    const auto reportSchemaDiagnostic = [this, &wPath, &path](
        const std::string_view diagnostic)
    {
        if (diagnostic.empty())
        {
            return;
        }
        const auto previous = cogScriptDiagnosticsByPath_.find(wPath);
        if (previous != cogScriptDiagnosticsByPath_.end()
            && previous->second == diagnostic)
        {
            return;
        }
        cogScriptDiagnosticsByPath_[wPath] = std::string(diagnostic);
        logMessage(
            MessageSeverity::error,
            QStringLiteral("COG script '%1': %2")
                .arg(
                    path,
                    QString::fromStdString(std::string(diagnostic))));
    };

    // QFileSystemWatcher can report the same replacement through both the
    // file and parent-directory channels. Fingerprint the current bytes so a
    // queued duplicate cannot reapply a schema after the designer has undone
    // the synchronous in-app save refresh. This fingerprint is scoped to the
    // current document/resource identity and is never a recovery source.
    QFile diskFile(path);
    if (!diskFile.open(QIODevice::ReadOnly))
    {
        reportSchemaDiagnostic(
            QStringLiteral("could not read the file after it changed")
                .toStdString());
        scheduleRebuildChecks(editor::DocumentDirty::Cogs);
        return;
    }
    const QByteArray diskBytes = diskFile.readAll();
    if (diskFile.error() != QFileDevice::NoError)
    {
        reportSchemaDiagnostic(
            QStringLiteral("could not read the file after it changed: %1")
                .arg(diskFile.errorString())
                .toStdString());
        scheduleRebuildChecks(editor::DocumentDirty::Cogs);
        return;
    }
    const std::string contentFingerprint =
        QCryptographicHash::hash(diskBytes, QCryptographicHash::Sha256)
            .toHex()
            .toStdString();
    const auto appliedContent = cogScriptAppliedContentByPath_.find(wPath);
    if (appliedContent != cogScriptAppliedContentByPath_.end()
        && appliedContent->second == contentFingerprint)
    {
        scheduleRebuildChecks(editor::DocumentDirty::Cogs);
        return;
    }

    const std::string_view capturedText(
        diskBytes.constData(),
        static_cast<std::size_t>(diskBytes.size()));
    const sed::assets::CogSymbolLoadResult loaded =
        sed::assets::loadCogSymbolsFromTextResult(capturedText, false);
    if (!loaded.loaded())
    {
        // A malformed or unreadable schema cannot safely replace placed
        // values. Keep the previous schema and let checks expose the failure.
        reportSchemaDiagnostic(loaded.diagnostic);
        scheduleRebuildChecks(editor::DocumentDirty::Cogs);
        return;
    }
    const std::vector<CogValue> symbols = loaded.symbols;
    const sed::assets::ResourceLocator* resources = &resources_;
    const bool applied = document_.applyUndoable(
        "COG: Script schema updated",
        editor::DocumentDirty::Cogs | editor::DocumentDirty::Structure,
        [resources, wPath, symbols](editor::DocumentState& state) -> bool
        {
            bool changed = false;
            for (Cog& cog : state.level.cogs)
            {
                const auto cogPath = tryFindCogScriptOnDisk(
                    *resources,
                    sed::text::trimView(cog.name));
                if (!cogPath
                    || normalizedCogScriptWatchPath(*cogPath) != wPath)
                {
                    continue;
                }

                std::vector<CogValue> updated = mergeCogScriptSchema(
                    state.level, cog, symbols);
                if (!cogValuesEqual(cog.values, updated))
                {
                    cog.values = std::move(updated);
                    changed = true;
                }
            }
            return changed;
        });

    // Record the bytes only after the document transaction (including a
    // successful no-op) has completed. If the transaction throws, a later
    // notification must still be allowed to retry the captured revision.
    cogScriptDiagnosticsByPath_.erase(wPath);
    cogScriptAppliedContentByPath_[wPath] = contentFingerprint;

    if (applied)
    {
        rebuildOutliner();
        rebuildProperties();
        updateEditActions();
        scheduleRebuildChecks(
            editor::DocumentDirty::Cogs | editor::DocumentDirty::Structure);
    }
    else
    {
        scheduleRebuildChecks(editor::DocumentDirty::Cogs);
    }
}

void MainWindow::handleCogScriptDirectoryChanged(const QString& path)
{
    if (path.isEmpty())
    {
        return;
    }

    // Directory notifications are used as a backstop for deletes/atomic-replace saves.
    // Delay slightly so the filesystem has settled.
    const std::uint64_t watchGeneration = cogScriptWatchGeneration_;
    const std::uint64_t documentEpoch = recoveryDocumentEpoch_;
    QTimer::singleShot(250, this, [this, path, watchGeneration, documentEpoch]()
        {
            (void)path;
            if (!cogScriptWatcher_
                || watchGeneration != cogScriptWatchGeneration_
                || documentEpoch != recoveryDocumentEpoch_)
            {
                return;
            }

            // A directory watch is also used for missing scripts.  When the
            // nearest existing ancestor was watched (for example, the
            // resource root while `cog/` did not exist), a notification for
            // the newly-created directory must first move the watch onto that
            // directory before a later file creation can be observed.
            syncCogScriptWatches();
            if (watchGeneration != cogScriptWatchGeneration_
                || documentEpoch != recoveryDocumentEpoch_)
            {
                return;
            }

            // Applying one schema rebuilds properties and synchronizes the
            // watcher maps. Snapshot the paths before invoking it so the
            // range iteration cannot be invalidated by that re-entrant work.
            std::vector<std::pair<std::wstring, std::filesystem::path>> scripts{};
            scripts.reserve(cogScriptDiskPathByIdentity_.size());
            for (const auto& entry : cogScriptDiskPathByIdentity_)
            {
                scripts.push_back(entry);
            }

            for (const auto& [identity, fsPath] : scripts)
            {
                (void)identity;
                std::error_code ec;
                const bool exists = std::filesystem::is_regular_file(fsPath, ec) && !ec;
                const QString qFile = QString::fromStdWString(fsPath.wstring());
                if (exists
                    && !cogScriptWatcherContainsPath(
                        cogScriptWatcher_->files(), fsPath))
                {
                    cogScriptWatcher_->addPath(qFile);
                }
                if (exists)
                {
                    applyCogScriptFileChangedNow(
                        qFile, watchGeneration, documentEpoch);
                }
                else
                {
                    scheduleRebuildChecks(editor::DocumentDirty::Cogs);
                }
            }
        });
}

} // namespace sed
