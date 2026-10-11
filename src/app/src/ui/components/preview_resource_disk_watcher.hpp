#pragma once

#include "ui/components/preview_resource_revision_utils.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace sed::ui::detail
{

/**
 * Debounced wake-up for loose preview resources.
 *
 * Exact file notifications remain revision-independent, so same-size writes
 * whose metadata token does not change still wake consumers. Directory
 * notifications are filtered through dependency revision tokens: an unrelated
 * autosave or editor write in a watched ancestor must not clear every render
 * cache and retry every missing resource lookup.
 */
class PreviewResourceDiskWatcher final : public QObject
{
public:
    using ChangeCallback = std::function<void()>;
    using ArchiveChangeCallback = std::function<bool()>;

    explicit PreviewResourceDiskWatcher(
        QObject* parent,
        ChangeCallback callback,
        const int debounceMs = 250)
        : QObject(parent)
        , callback_(std::move(callback))
        , configuredDebounceMs_(std::max(0, debounceMs))
        , archiveRetryBaseDelayMs_(std::clamp(debounceMs, 1, kMaxArchiveRetryDelayMs))
        , archiveRetryDelayMs_(archiveRetryBaseDelayMs_)
    {
        changeTimer_.setSingleShot(true);
        changeTimer_.setInterval(configuredDebounceMs_);
        syncTimer_.setSingleShot(true);
        syncTimer_.setInterval(0);

        QObject::connect(&watcher_, &QFileSystemWatcher::fileChanged, this,
            [this](const QString& path)
            {
                // Every exact non-archive file in the watcher is a selected
                // dependency. Trust the operating-system notification even
                // when size/timestamp metadata did not advance.
                scheduleChange(watchedArchiveFiles_.contains(path));
            });
        QObject::connect(&watcher_, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString& path)
            {
                handleDirectoryChanged(path);
            });
        QObject::connect(&changeTimer_, &QTimer::timeout, this,
            [this]()
            {
                processPendingChange();
            });
        QObject::connect(&syncTimer_, &QTimer::timeout, this,
            [this]()
            {
                syncNow(false);
            });
    }

    /**
     * Called before the ordinary cache-invalidation callback when a mounted
     * GOB/GOO file (or its parent during atomic replacement) changes.  The
     * owner can therefore remount the archive before preview caches resolve
     * their new dependency revisions.
     */
    void setArchiveChangeCallback(ArchiveChangeCallback callback)
    {
        archiveChangeCallback_ = std::move(callback);
        resetArchiveRetryState();
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    /** Test seam for isolating a nested watcher from its owner's ordinary path. */
    void setChangeCallbackForTest(ChangeCallback callback)
    {
        callback_ = std::move(callback);
    }
#endif

    void setResourceLocator(const assets::ResourceLocator* resources)
    {
        if ( resources_ != resources )
        {
            resources_ = resources;
            dependencies_.clear();
            pendingDependencies_.clear();
            cancelArchiveRetry();
        }
        else
        {
            // ResourceLocator is retained by pointer while roots and mounts
            // are rebuilt. Keep dependencies that back existing CPU preview
            // geometry, but adopt their new lookup baseline because the owner
            // already invalidates caches as part of this explicit set call.
            resources_ = resources;
            refreshDependencyRevisions();
            resetArchiveRetryState();
        }
        fullSyncRequired_ = true;
        scheduleSync();
    }

    /**
     * Controls ownership of mounted archive replacement notifications.
     *
     * Delegated previews keep watching their selected loose files and parent
     * directories, while the owning window watches and remounts GOB/GOO
     * files.  A parent directory is still retained when it is also required
     * by a loose-resource watch plan.
     */
    void setMountedArchiveWatchingEnabled(const bool enabled)
    {
        if ( watchMountedArchives_ == enabled )
        {
            return;
        }

        watchMountedArchives_ = enabled;
        fullSyncRequired_ = true;
        scheduleSync();
    }

    void clearDependencies()
    {
        dependencies_.clear();
        pendingDependencies_.clear();
        fullSyncRequired_ = true;
        ++dependencyResetSerial_;
        scheduleSync();
    }

    void watchResource(
        const std::string_view name,
        const std::string_view ext = {})
    {
        const std::string lookup = ::sed::text::trim(name);
        if (lookup.empty() || lookup == "-")
        {
            return;
        }

        Dependency dependency{
            lookup,
            std::string(ext) };
        // Hot render paths call watchResource repeatedly. Rebuilding the full
        // filesystem watch plan or even resolving the same revision on every
        // pending-thumbnail poll is needless main-thread I/O. Avoid evaluating
        // the revision token at all for an already registered dependency.
        if (dependencies_.contains(dependency))
        {
            return;
        }

        ++revisionProbeCount_;
        const auto [insertedDependency, inserted] = dependencies_.try_emplace(
            std::move(dependency),
            previewResourceRevisionToken(resources_, lookup, ext));
        if (inserted)
        {
            pendingDependencies_.push_back(insertedDependency->first);
        }
        scheduleSync();
    }

    void syncNow(const bool forceFull = true)
    {
        syncTimer_.stop();
        fullSyncRequired_ = fullSyncRequired_ || forceFull;

        if (!fullSyncRequired_)
        {
            if (resources_)
            {
                QStringList currentFiles = watcher_.files();
                QStringList currentDirectories = watcher_.directories();
                for (const Dependency& dependency : pendingDependencies_)
                {
                    ++watchPlanDependencyVisitCount_;
                    const PreviewResourceDiskWatchPlan plan =
                        previewResourceDiskWatchPlan(
                            resources_, dependency.name, dependency.ext);
                    for (const std::filesystem::path& path : plan.files)
                    {
                        const QString filePath = toQString(path);
                        if (!currentFiles.contains(filePath))
                        {
                            if (watcher_.addPath(filePath))
                            {
                                currentFiles.push_back(filePath);
                            }
                        }
                    }
                    for (const std::filesystem::path& path : plan.directories)
                    {
                        const QString directoryPath = toQString(path);
                        if (!currentDirectories.contains(directoryPath))
                        {
                            if (watcher_.addPath(directoryPath))
                            {
                                currentDirectories.push_back(directoryPath);
                            }
                        }
                    }
                }
            }
            pendingDependencies_.clear();
            return;
        }

        fullSyncRequired_ = false;
        pendingDependencies_.clear();

        QSet<QString> desiredFiles{};
        QSet<QString> desiredDirectories{};
        QSet<QString> desiredArchiveFiles{};
        QSet<QString> desiredArchiveDirectories{};
        std::unordered_map<std::wstring, std::string> desiredArchiveRevisions{};
        if (resources_)
        {
            for (const assets::GobArchive& archive : resources_->gobs())
            {
                const std::filesystem::path path =
                    normalizedPreviewWatchPath(archive.path());
                if ( watchMountedArchives_ )
                {
                    const QString filePath = toQString(path);
                    desiredFiles.insert(filePath);
                    desiredArchiveFiles.insert(filePath);
                    desiredArchiveRevisions.emplace(
                        path.wstring(),
                        previewFileRevisionToken("ARCHIVE", path));

                    const std::filesystem::path parent = path.parent_path();
                    std::error_code ec;
                    if (!parent.empty()
                        && std::filesystem::is_directory(parent, ec)
                        && !ec)
                    {
                        const QString directoryPath = toQString(parent);
                        desiredDirectories.insert(directoryPath);
                        desiredArchiveDirectories.insert(directoryPath);
                    }
                }
            }

            for (const auto& [dependency, revision] : dependencies_)
            {
                (void)revision;
                ++watchPlanDependencyVisitCount_;
                const PreviewResourceDiskWatchPlan plan = previewResourceDiskWatchPlan(
                    resources_, dependency.name, dependency.ext);
                for (const std::filesystem::path& path : plan.files)
                {
                    desiredFiles.insert(toQString(path));
                }
                for (const std::filesystem::path& path : plan.directories)
                {
                    desiredDirectories.insert(toQString(path));
                }
            }
        }

        const QStringList watchedFiles = watcher_.files();
        for (const QString& path : watchedFiles)
        {
            if (!desiredFiles.contains(path))
            {
                watcher_.removePath(path);
            }
        }
        const QStringList watchedDirectories = watcher_.directories();
        for (const QString& path : watchedDirectories)
        {
            if (!desiredDirectories.contains(path))
            {
                watcher_.removePath(path);
            }
        }

        const QStringList currentFiles = watcher_.files();
        for (const QString& path : desiredFiles)
        {
            if (!currentFiles.contains(path))
            {
                (void)watcher_.addPath(path);
            }
        }

        watchedArchiveFiles_ = std::move(desiredArchiveFiles);
        watchedArchiveDirectories_ = std::move(desiredArchiveDirectories);
        watchedArchiveRevisions_ = std::move(desiredArchiveRevisions);
        const QStringList currentDirectories = watcher_.directories();
        for (const QString& path : desiredDirectories)
        {
            if (!currentDirectories.contains(path))
            {
                (void)watcher_.addPath(path);
            }
        }
    }

    [[nodiscard]] QStringList watchedFiles() const
    {
        return watcher_.files();
    }

    [[nodiscard]] QStringList watchedDirectories() const
    {
        return watcher_.directories();
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    [[nodiscard]] bool mountedArchiveWatchingEnabledForTest() const noexcept
    {
        return watchMountedArchives_;
    }
#endif

    /** Deterministic seam for proving bulk UI refreshes retain the watch plan. */
    [[nodiscard]] std::uint64_t dependencyResetSerialForTest() const noexcept
    {
        return dependencyResetSerial_;
    }

    /** Deterministic seam for proving picker startup work is visible-first. */
    [[nodiscard]] std::size_t dependencyCountForTest() const noexcept
    {
        return dependencies_.size();
    }

    /** Counts only first-registration revision probes, not change scans. */
    [[nodiscard]] std::uint64_t revisionProbeCountForTest() const noexcept
    {
        return revisionProbeCount_;
    }

    /** Counts dependency plans resolved by full or incremental watch syncs. */
    [[nodiscard]] std::uint64_t watchPlanDependencyVisitCountForTest() const noexcept
    {
        return watchPlanDependencyVisitCount_;
    }

    /** Number of failed attempts retained for the current archive event. */
    [[nodiscard]] std::uint32_t archiveRetryAttemptCountForTest() const noexcept
    {
        return archiveRetryAttemptCount_;
    }

    /** Current retry delay, exposed so tests can verify capped backoff. */
    [[nodiscard]] int archiveRetryDelayMsForTest() const noexcept
    {
        return archiveRetryDelayMs_;
    }

    /** Delay currently installed on the coalescing timer. */
    [[nodiscard]] int changeTimerIntervalForTest() const noexcept
    {
        return changeTimer_.interval();
    }

    [[nodiscard]] bool changeTimerActiveForTest() const noexcept
    {
        return changeTimer_.isActive();
    }

    /** True while a failed archive event is retained for a retry. */
    [[nodiscard]] bool archiveRetryPendingForTest() const noexcept
    {
        return changePending_ && archiveChangePending_;
    }

    /** Deterministic seam for tests; production changes arrive via Qt. */
    void notifyPathChangedForTest()
    {
        scheduleChange();
    }

    /** Deterministic archive-replacement seam for tests. */
    void notifyArchivePathChangedForTest()
    {
        scheduleChange(true);
    }

    /** Deterministic directory-notification seam with production filtering. */
    void notifyDirectoryPathChangedForTest()
    {
        handleDirectoryChanged(QString{});
    }

    /**
     * Dispatches the currently coalesced event without depending on platform
     * event-loop timer timing. Production notifications remain timer-driven.
     */
    void flushPendingChangeForTest()
    {
        if (!changeTimer_.isActive())
        {
            return;
        }
        changeTimer_.stop();
        processPendingChange();
    }

private:
    struct Dependency
    {
        std::string name{};
        std::string ext{};

        bool operator==(const Dependency& other) const noexcept
        {
            return ::sed::text::iequalsAscii(name, other.name)
                && ::sed::text::iequalsAscii(ext, other.ext);
        }
    };

    struct DependencyHash
    {
        std::size_t operator()(const Dependency& dependency) const noexcept
        {
            const std::size_t first = std::hash<std::string>{}(
                ::sed::text::upperAscii(dependency.name));
            const std::size_t second = std::hash<std::string>{}(
                ::sed::text::upperAscii(dependency.ext));
            return first ^ (second + 0x9e3779b9u + (first << 6u) + (first >> 2u));
        }
    };

    static QString toQString(const std::filesystem::path& path)
    {
#ifdef _WIN32
        return QString::fromStdWString(path.wstring());
#else
        return QString::fromStdString(path.string());
#endif
    }

    [[nodiscard]] bool mountedArchiveRevisionChanged() const
    {
        for (const QString& watchedFile : watchedArchiveFiles_)
        {
#ifdef _WIN32
            const std::filesystem::path path(watchedFile.toStdWString());
#else
            const std::filesystem::path path(watchedFile.toStdString());
#endif
            const std::filesystem::path normalized =
                normalizedPreviewWatchPath(path);
            const auto it = watchedArchiveRevisions_.find(normalized.wstring());
            if (it == watchedArchiveRevisions_.end()
                || it->second != previewFileRevisionToken("ARCHIVE", normalized))
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool dependencyRevisionChanged() const
    {
        if (!resources_)
        {
            return false;
        }
        for (const auto& [dependency, previousRevision] : dependencies_)
        {
            if (previewResourceRevisionToken(
                    resources_, dependency.name, dependency.ext)
                != previousRevision)
            {
                return true;
            }
        }
        return false;
    }

    void refreshDependencyRevisions()
    {
        for (auto& [dependency, revision] : dependencies_)
        {
            revision = previewResourceRevisionToken(
                resources_, dependency.name, dependency.ext);
        }
    }

    void handleDirectoryChanged(const QString& path)
    {
        // Coalesce bursts before touching the filesystem. Atomic saves often
        // emit several parent notifications; one revision pass is enough.
        directoryRevisionCheckPending_ = true;
        archiveDirectoryRevisionCheckPending_ =
            archiveDirectoryRevisionCheckPending_
            || watchedArchiveDirectories_.contains(path);
        changeTimer_.start(configuredDebounceMs_);
    }

    void scheduleChange(const bool archiveChanged = false)
    {
        changePending_ = true;
        if ( archiveChanged )
        {
            resetArchiveRetryState();
        }
        archiveChangePending_ = archiveChangePending_ || archiveChanged;
        changeTimer_.start(configuredDebounceMs_);
    }

    void processPendingChange()
    {
        if (directoryRevisionCheckPending_)
        {
            const bool archiveChanged =
                archiveDirectoryRevisionCheckPending_
                && mountedArchiveRevisionChanged();
            const bool dependencyChanged = dependencyRevisionChanged();
            directoryRevisionCheckPending_ = false;
            archiveDirectoryRevisionCheckPending_ = false;
            if (archiveChanged || dependencyChanged)
            {
                if ( archiveChanged )
                {
                    resetArchiveRetryState();
                }
                changePending_ = true;
                archiveChangePending_ =
                    archiveChangePending_ || archiveChanged;
            }
        }

        // An unchanged parent-directory notification does not drop an exact
        // QFileSystemWatcher file path. Avoid a full watch-plan scan and cache
        // invalidation; real exact-file and revision changes continue below.
        if (changePending_)
        {
            dispatchPendingChange();
        }
    }

    void dispatchPendingChange()
    {
        changePending_ = false;
        const bool archiveChanged = archiveChangePending_;
        archiveChangePending_ = false;
        // QFileSystemWatcher may drop a file path after deletion or atomic
        // replacement. Resolve precedence and restore it first.
        fullSyncRequired_ = true;
        syncNow();
        if ( archiveChanged && archiveChangeCallback_ )
        {
            if ( !archiveChangeCallback_() )
            {
                // A consumer may still have a bounded background read against
                // its locator snapshot. Keep the archive event pending and
                // retry with capped exponential backoff rather than mutating
                // that locator concurrently or spinning on a bad archive.
                changePending_ = true;
                archiveChangePending_ = true;
                ++archiveRetryAttemptCount_;
                const int retryDelay = archiveRetryDelayMs_;
                archiveRetryDelayMs_ = std::min(
                    kMaxArchiveRetryDelayMs,
                    archiveRetryDelayMs_ > kMaxArchiveRetryDelayMs / 2
                        ? kMaxArchiveRetryDelayMs
                        : archiveRetryDelayMs_ * 2);
                changeTimer_.start(retryDelay);
                return;
            }
        }
        resetArchiveRetryState();
        if ( callback_ )
        {
            callback_();
        }
        refreshDependencyRevisions();
        fullSyncRequired_ = true;
        scheduleSync();
    }

    void scheduleSync()
    {
        if (!syncTimer_.isActive())
        {
            syncTimer_.start();
        }
    }

    static constexpr int kMaxArchiveRetryDelayMs = 4000;

    void resetArchiveRetryState() noexcept
    {
        archiveRetryAttemptCount_ = 0;
        archiveRetryDelayMs_ = archiveRetryBaseDelayMs_;
        changeTimer_.setInterval(configuredDebounceMs_);
    }

    void cancelArchiveRetry() noexcept
    {
        changeTimer_.stop();
        changePending_ = false;
        archiveChangePending_ = false;
        directoryRevisionCheckPending_ = false;
        archiveDirectoryRevisionCheckPending_ = false;
        resetArchiveRetryState();
    }

    const assets::ResourceLocator* resources_{};
    ChangeCallback callback_{};
    const int configuredDebounceMs_{};
    ArchiveChangeCallback archiveChangeCallback_{};
    QFileSystemWatcher watcher_{};
    QTimer changeTimer_{};
    QTimer syncTimer_{};
    QSet<QString> watchedArchiveFiles_{};
    QSet<QString> watchedArchiveDirectories_{};
    std::unordered_map<std::wstring, std::string> watchedArchiveRevisions_{};
    std::vector<Dependency> pendingDependencies_{};
    bool fullSyncRequired_{ true };
    bool changePending_{};
    bool archiveChangePending_{};
    bool directoryRevisionCheckPending_{};
    bool archiveDirectoryRevisionCheckPending_{};
    bool watchMountedArchives_{ true };
    std::uint64_t dependencyResetSerial_{};
    std::uint64_t revisionProbeCount_{};
    std::uint64_t watchPlanDependencyVisitCount_{};
    const int archiveRetryBaseDelayMs_{};
    int archiveRetryDelayMs_{};
    std::uint32_t archiveRetryAttemptCount_{};
    std::unordered_map<Dependency, std::string, DependencyHash> dependencies_{};
};

} // namespace sed::ui::detail
