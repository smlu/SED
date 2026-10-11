#include "ui/components/loose_document_watcher.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QHash>
#include <QMessageBox>
#include <QList>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <QWidget>
#include <QtConcurrent/QtConcurrentRun>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#else
#include <sys/stat.h>
#endif

#include <algorithm>
#include <atomic>
#include <deque>
#include <memory>
#include <optional>
#include <utility>

namespace sed::ui
{
namespace
{

enum class DiskState
{
    readable,
    missing,
    unreadable,
    oversized,
    cancelled,
};

struct DiskRevision
{
    DiskState state{DiskState::missing};
    QByteArray fingerprint{};
    qint64 bytes{};

    bool operator==(const DiskRevision& other) const
    {
        if ( state != other.state )
        {
            return false;
        }
        return state == DiskState::readable ? fingerprint == other.fingerprint : bytes == other.bytes;
    }
};

DiskRevision acceptedRevision(const QByteArray& fingerprint)
{
    return {DiskState::readable, fingerprint, 0};
}

QString watchPath(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
}

QString pathKey(const QString& path)
{
#ifdef Q_OS_WIN
    return path.toCaseFolded();
#else
    return path;
#endif
}

struct FileMetadata
{
    bool exists{};
    qint64 bytes{};
    qint64 modified{};
    qint64 created{};
    quint64 device{};
    quint64 fileId{};

    bool operator==(const FileMetadata&) const = default;
};

// Metadata probes and file fingerprints run only on the private bounded pool.
// Native file identity catches atomic replacement with preserved size/times.
FileMetadata fileMetadata(const QString& path)
{
    const QFileInfo info(path);
    FileMetadata metadata{info.exists(), info.size(), info.lastModified().toMSecsSinceEpoch(),
        info.birthTime().toMSecsSinceEpoch()};
#ifdef Q_OS_WIN
    const HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if ( handle != INVALID_HANDLE_VALUE )
    {
        BY_HANDLE_FILE_INFORMATION native{};
        if ( GetFileInformationByHandle(handle, &native) )
        {
            metadata.device = native.dwVolumeSerialNumber;
            metadata.fileId = (static_cast<quint64>(native.nFileIndexHigh) << 32) | native.nFileIndexLow;
        }
        CloseHandle(handle);
    }
#else
    struct stat native{};
    const QByteArray encoded = QFile::encodeName(path);
    if ( ::stat(encoded.constData(), &native) == 0 )
    {
        metadata.device = static_cast<quint64>(native.st_dev);
        metadata.fileId = static_cast<quint64>(native.st_ino);
    }
#endif
    return metadata;
}

struct ProbeResult
{
    DiskRevision revision{};
    FileMetadata metadata{};
    QString survivingParent{};
    bool stable{true};
};

ProbeResult probeFile(const QString& path, const qint64 maximumBytes, const bool forceFingerprint,
    const std::optional<FileMetadata>& previousMetadata, const std::optional<DiskRevision>& previousRevision,
    const std::shared_ptr<std::atomic_bool>& cancelled)
{
    ProbeResult result{};
    if ( cancelled->load() )
    {
        result.revision.state = DiskState::cancelled;
        return result;
    }
    result.metadata = fileMetadata(path);
    QString parent = QFileInfo(path).absolutePath();
    while ( !QFileInfo(parent).isDir() )
    {
        const QString ancestor = QFileInfo(parent).absolutePath();
        if ( ancestor == parent || ancestor.isEmpty() || cancelled->load() )
        {
            parent.clear();
            break;
        }
        parent = ancestor;
    }
    result.survivingParent = parent;
    if ( !forceFingerprint && previousMetadata == result.metadata && previousRevision )
    {
        result.revision = *previousRevision;
        return result;
    }
    QFile file(path);
    if ( !file.open(QIODevice::ReadOnly) )
    {
        result.revision.state = result.metadata.exists ? DiskState::unreadable : DiskState::missing;
        return result;
    }
    if ( file.size() > maximumBytes )
    {
        result.revision = {DiskState::oversized, {}, file.size()};
        return result;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 readBytes{};
    while ( !file.atEnd() )
    {
        if ( cancelled->load() )
        {
            result.revision.state = DiskState::cancelled;
            return result;
        }
        const QByteArray chunk = file.read(64 * 1024);
        if ( file.error() != QFileDevice::NoError || chunk.isEmpty() )
        {
            result.revision.state = DiskState::unreadable;
            return result;
        }
        readBytes += chunk.size();
        if ( readBytes > maximumBytes )
        {
            result.revision = {DiskState::oversized, {}, readBytes};
            return result;
        }
        hash.addData(chunk);
    }
    result.revision = {DiskState::readable, hash.result(), readBytes};
    result.stable = result.metadata == fileMetadata(path);
    return result;
}

class LooseDocumentWatchService;

class WatchedDocument final : public QObject
{
public:
    WatchedDocument(QWidget* owner, LooseDocumentWatchOptions options, LooseDocumentWatchService* service);
    ~WatchedDocument() override;
    bool current() const;

    QPointer<QWidget> owner{};
    QWidget* ownerKey{};
    LooseDocumentWatchOptions options{};
    QPointer<LooseDocumentWatchService> service{};
    QString key{};
    DiskRevision accepted{};
    std::optional<DiskRevision> acknowledged{};
    std::optional<DiskRevision> observed{};
    std::uint64_t incarnation{};
    std::uint64_t acceptanceEpoch{};
    bool active{true};
    bool queued{};
    bool forceOffer{};
};

class LooseDocumentWatchService final : public QObject
{
public:
    explicit LooseDocumentWatchService(QObject* parent)
        : QObject(parent)
        , filesystem_(this)
    {
        filesystem_.setObjectName(QStringLiteral("sed.loose_document_watcher"));
        pool_.setMaxThreadCount(2);
        debounce_.setSingleShot(true);
        QObject::connect(&debounce_, &QTimer::timeout, this, [this]() { startReads(); });
        idle_.setSingleShot(true);
        QObject::connect(&idle_, &QTimer::timeout, this, [this]() { showNextOffer(); });
        QObject::connect(&filesystem_, &QFileSystemWatcher::fileChanged, this, [this](const QString& path)
            {
                const QString key = pathKey(watchPath(path));
                if ( auto source = sources_.find(key); source != sources_.end() )
                {
                    // Re-arm after the probe whether Qt retained or removed
                    // its old handle during an atomic replacement.
                    filesystem_.removePath(source->path);
                    source->fileWatched = false;
                }
                queueRead(key, true);
            });
        QObject::connect(&filesystem_, &QFileSystemWatcher::directoryChanged, this, [this](const QString& path)
            {
                const QString directory = pathKey(watchPath(path));
                auto watched = directories_.find(directory);
                if ( watched == directories_.end() )
                {
                    return;
                }
                // A deleted directory loses its native watch. Only its own
                // subscribers need an off-thread metadata/re-arm probe.
                filesystem_.removePath(watched->path);
                watched->watching = false;
                const auto affected = watched->sources.values();
                for ( const QString& key : affected )
                {
                    queueRead(key, false);
                }
            });
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() { shutdown(); });
    }

    ~LooseDocumentWatchService() override
    {
        shutdown();
        // Reads check cancellation between bounded chunks. No completion can
        // publish through a half-destroyed service while the pool joins.
        for ( QObject* const child : findChildren<QObject*>() )
        {
            QObject::disconnect(child, nullptr, this, nullptr);
        }
        pool_.waitForDone();
    }

    WatchedDocument* document(const QWidget* owner) const
    {
        return documents_.value(const_cast<QWidget*>(owner)).data();
    }

    void attach(QWidget* owner, LooseDocumentWatchOptions options)
    {
        if ( shuttingDown_ || !owner || options.path.isEmpty() || !options.reload
            || options.maximumFileBytes <= 0 || options.maximumFileBytes > 64LL * 1024 * 1024 )
        {
            return;
        }
        removeDocument(document(owner));
        auto* const watched = new WatchedDocument(owner, std::move(options), this);
        watched->incarnation = ++epoch_;
        watched->acceptanceEpoch = ++epoch_;
        watched->key = pathKey(watchPath(watched->options.path));
        documents_.insert(owner, watched);
        auto entry = sources_.find(watched->key);
        if ( entry == sources_.end() )
        {
            Source source{};
            source.path = watchPath(watched->options.path);
            source.incarnation = ++epoch_;
            source.epoch = ++epoch_;
            entry = sources_.insert(watched->key, std::move(source));
        }
        entry->documents.insert(watched);
        queueRead(watched->key, true);
    }

    void removeDocument(WatchedDocument* watched, const bool scheduleDeletion = true)
    {
        if ( !watched || !watched->active )
        {
            return;
        }
        watched->active = false;
        watched->queued = false;
        const QString key = watched->key;
        if ( documents_.value(watched->ownerKey) == watched )
        {
            documents_.remove(watched->ownerKey);
        }
        if ( activeDocument_ == watched && activePrompt_ )
        {
            activePrompt_->reject();
        }
        auto source = sources_.find(key);
        if ( source != sources_.end() )
        {
            source->documents.remove(watched);
            if ( source->documents.isEmpty() )
            {
                removeSourceWatch(key, source.value());
                sources_.erase(source);
                pendingReads_.remove(key);
            }
        }
        watched->options.reload = {};
        watched->options.stillCurrent = {};
        watched->options.authoredRevision = {};
        if ( scheduleDeletion )
        {
            watched->deleteLater();
        }
        if ( !shuttingDown_ )
        {
            scheduleOffer();
        }
    }

    void accept(QWidget* owner, const QString& path, const QByteArray& fingerprint)
    {
        WatchedDocument* const watched = document(owner);
        if ( !watched || fingerprint.size() != 32 )
        {
            return;
        }
        if ( watched->options.path != path )
        {
            auto options = watched->options;
            options.path = path;
            options.acceptedFingerprint = fingerprint;
            attach(owner, std::move(options));
            return;
        }
        watched->options.acceptedFingerprint = fingerprint;
        watched->accepted = acceptedRevision(fingerprint);
        watched->acknowledged = watched->accepted;
        watched->observed = watched->accepted;
        watched->acceptanceEpoch = ++epoch_;
        watched->queued = false;
        watched->forceOffer = false;
        if ( activeDocument_ == watched && activePrompt_ )
        {
            activePrompt_->reject();
        }
        // A prior worker may have opened the old file before atomic Save.
        // Its result cannot overwrite this exact accepted revision.
        queueRead(watched->key, true);
    }

    void forceOffer(QWidget* owner)
    {
        if ( WatchedDocument* const watched = document(owner) )
        {
            watched->forceOffer = true;
            queueRead(watched->key, true);
        }
    }

private:
    struct Source
    {
        QString path{};
        QString watchedParentKey{};
        QSet<WatchedDocument*> documents{};
        std::optional<FileMetadata> metadata{};
        std::optional<DiskRevision> revision{};
        std::uint64_t incarnation{};
        std::uint64_t epoch{};
        bool reading{};
        bool forceFingerprint{};
        bool fileWatched{};
    };

    struct DirectoryWatch
    {
        QString path{};
        QSet<QString> sources{};
        bool watching{};
    };

    struct Offer
    {
        QPointer<WatchedDocument> document{};
        std::uint64_t acceptanceEpoch{};
    };

    void shutdown()
    {
        if ( shuttingDown_ )
        {
            return;
        }
        shuttingDown_ = true;
        cancelled_->store(true);
        debounce_.stop();
        idle_.stop();
        pendingReads_.clear();
        offers_.clear();
        if ( activePrompt_ )
        {
            activePrompt_->reject();
        }
    }

    void queueRead(const QString& key, const bool forceFingerprint)
    {
        auto source = sources_.find(key);
        if ( shuttingDown_ || source == sources_.end() )
        {
            return;
        }
        source->epoch = ++epoch_;
        source->forceFingerprint = source->forceFingerprint || forceFingerprint;
        pendingReads_.insert(key);
        debounce_.start(100);
    }

    void startReads()
    {
        if ( shuttingDown_ )
        {
            return;
        }
        for ( auto pending = pendingReads_.begin(); pending != pendingReads_.end(); )
        {
            if ( activeReads_ >= 2 )
            {
                break;
            }
            const QString key = *pending;
            auto source = sources_.find(key);
            if ( source == sources_.end() )
            {
                pending = pendingReads_.erase(pending);
                continue;
            }
            if ( source->reading )
            {
                ++pending;
                continue;
            }
            pending = pendingReads_.erase(pending);
            source->reading = true;
            const QString path = source->path;
            const std::uint64_t epoch = source->epoch;
            const std::uint64_t incarnation = source->incarnation;
            const bool forceFingerprint = std::exchange(source->forceFingerprint, false);
            const auto previousMetadata = source->metadata;
            const auto previousRevision = source->revision;
            qint64 maximumBytes{};
            for ( const WatchedDocument* const watched : source->documents )
            {
                maximumBytes = std::max(maximumBytes, watched->options.maximumFileBytes);
            }
            auto* const completion = new QFutureWatcher<ProbeResult>(this);
            ++activeReads_;
            QObject::connect(completion, &QFutureWatcher<ProbeResult>::finished, this,
                [this, completion, key, epoch, incarnation]()
                {
                    const ProbeResult result = completion->result();
                    completion->deleteLater();
                    --activeReads_;
                    auto current = sources_.find(key);
                    // Re-registering the same path can produce a different
                    // source while this old job is still running. Its result
                    // must not clear the new source's in-flight job flag.
                    if ( current != sources_.end() && current->incarnation == incarnation )
                    {
                        current->reading = false;
                        if ( !shuttingDown_ && current->epoch == epoch
                            && result.revision.state != DiskState::cancelled )
                        {
                            const bool newlyArmed = updateSourceWatch(key, current.value(), result);
                            if ( result.stable )
                            {
                                current->metadata = result.metadata;
                                current->revision = result.revision;
                                QList<QPointer<WatchedDocument>> subscribers{};
                                for ( WatchedDocument* const watched : current->documents )
                                {
                                    subscribers.push_back(watched);
                                }
                                for ( const QPointer<WatchedDocument>& watched : subscribers )
                                {
                                    publish(watched.data(), result.revision);
                                }
                            }
                            if ( !result.stable || newlyArmed )
                            {
                                // Close the bounded race between probing and
                                // installing a previously missing watch.
                                queueRead(key, true);
                            }
                        }
                    }
                    startReads();
                    scheduleOffer();
                });
            completion->setFuture(QtConcurrent::run(&pool_,
                [path, maximumBytes, forceFingerprint, previousMetadata, previousRevision, cancelled = cancelled_]()
                {
                    return probeFile(path, maximumBytes, forceFingerprint, previousMetadata, previousRevision, cancelled);
                }));
        }
    }

    void removeDirectoryReference(const QString& key, const QString& sourceKey)
    {
        auto directory = directories_.find(key);
        if ( directory == directories_.end() )
        {
            return;
        }
        directory->sources.remove(sourceKey);
        if ( directory->sources.isEmpty() )
        {
            if ( directory->watching )
            {
                filesystem_.removePath(directory->path);
            }
            directories_.erase(directory);
        }
    }

    void removeSourceWatch(const QString& key, const Source& source)
    {
        if ( source.fileWatched )
        {
            filesystem_.removePath(source.path);
        }
        removeDirectoryReference(source.watchedParentKey, key);
    }

    bool updateSourceWatch(const QString& key, Source& source, const ProbeResult& result)
    {
        bool armed{};
        if ( source.fileWatched && source.metadata
            && (source.metadata->device != result.metadata.device || source.metadata->fileId != result.metadata.fileId) )
        {
            filesystem_.removePath(source.path);
            source.fileWatched = false;
        }
        if ( !result.metadata.exists && source.fileWatched )
        {
            filesystem_.removePath(source.path);
            source.fileWatched = false;
        }
        else if ( result.metadata.exists && !source.fileWatched )
        {
            source.fileWatched = filesystem_.addPath(source.path);
            armed = source.fileWatched;
        }
        const QString directoryKey = pathKey(result.survivingParent);
        if ( source.watchedParentKey != directoryKey )
        {
            removeDirectoryReference(source.watchedParentKey, key);
            source.watchedParentKey = directoryKey;
        }
        if ( !directoryKey.isEmpty() )
        {
            auto directory = directories_.find(directoryKey);
            if ( directory == directories_.end() )
            {
                DirectoryWatch entry{};
                entry.path = result.survivingParent;
                directory = directories_.insert(directoryKey, std::move(entry));
            }
            directory->sources.insert(key);
            if ( !directory->watching )
            {
                directory->watching = filesystem_.addPath(directory->path);
                armed = armed || directory->watching;
            }
        }
        return armed;
    }

    void publish(WatchedDocument* watched, DiskRevision revision)
    {
        const QPointer<WatchedDocument> guarded(watched);
        if ( !guarded || !guarded->current() || !guarded )
        {
            return;
        }
        if ( revision.state == DiskState::readable && revision.bytes > watched->options.maximumFileBytes )
        {
            revision.state = DiskState::oversized;
            revision.fingerprint.clear();
        }
        watched->observed = revision;
        if ( !watched->acknowledged && !watched->forceOffer )
        {
            // Recovery may not have a persisted exact disk-byte fingerprint.
            // Establish a transient watch baseline without replacing its draft.
            watched->acknowledged = revision;
            return;
        }
        if ( !watched->forceOffer && watched->acknowledged == watched->observed )
        {
            return;
        }
        if ( !watched->queued )
        {
            watched->queued = true;
            offers_.push_back({watched, watched->acceptanceEpoch});
        }
    }

    void scheduleOffer()
    {
        if ( !shuttingDown_ && activePromptSerial_ == 0 && !offers_.empty() )
        {
            idle_.start(0);
        }
    }

    bool interactionBusy() const
    {
        return QWidget::mouseGrabber() || QApplication::mouseButtons() != Qt::NoButton
            || QApplication::activeModalWidget() || QApplication::activePopupWidget();
    }

    void showNextOffer()
    {
        if ( shuttingDown_ || activePromptSerial_ != 0 )
        {
            return;
        }
        if ( interactionBusy() )
        {
            idle_.start(33);
            return;
        }
        while ( !offers_.empty() )
        {
            const Offer offer = offers_.front();
            const QPointer<WatchedDocument> watched = offer.document;
            offers_.pop_front();
            if ( !watched || watched->acceptanceEpoch != offer.acceptanceEpoch )
            {
                continue;
            }
            watched->queued = false;
            if ( !watched->current() || !watched || !watched->observed
                || (!watched->forceOffer && watched->acknowledged == watched->observed) )
            {
                continue;
            }
            const DiskRevision revision = *watched->observed;
            watched->acknowledged = revision;
            watched->forceOffer = false;
            const std::uint64_t incarnation = watched->incarnation;
            const std::uint64_t acceptanceEpoch = watched->acceptanceEpoch;
            const auto revisionQuery = watched->options.authoredRevision;
            const std::uint64_t authoredRevision = revisionQuery ? revisionQuery() : 0;
            if ( !watched || !watched->active )
            {
                continue;
            }
            const bool removed = revision.state == DiskState::missing;
            const QString leaf = QFileInfo(watched->options.path).fileName();
            const QString text = (removed ? QStringLiteral("\"%1\" is no longer available on disk.")
                : QStringLiteral("\"%1\" changed on disk.")).arg(leaf) + QStringLiteral(
                    "\n\nReload replaces the current buffer and discards its unsaved changes. Keep preserves the current buffer.");
            auto* const message = new QMessageBox(QMessageBox::Question,
                removed ? QStringLiteral("File removed from disk") : QStringLiteral("File changed on disk"),
                text, QMessageBox::NoButton, watched->owner.data());
            message->setObjectName(QStringLiteral("sed.external_document_change_prompt"));
            message->setWindowModality(Qt::ApplicationModal);
            QAbstractButton* const reload = message->addButton(QStringLiteral("Reload"), QMessageBox::AcceptRole);
            QAbstractButton* const keep = message->addButton(QStringLiteral("Keep"), QMessageBox::RejectRole);
            message->setDefaultButton(qobject_cast<QPushButton*>(keep));
            message->setEscapeButton(keep);
            activePrompt_ = message;
            activeDocument_ = watched;
            const std::uint64_t promptSerial = activePromptSerial_ = ++epoch_;
            const QPointer<QMessageBox> guardedMessage(message);
            QObject::connect(message, &QDialog::finished, this,
                [this, watched, guardedMessage, reload, incarnation, acceptanceEpoch, authoredRevision, promptSerial]()
                {
                    if ( activePromptSerial_ != promptSerial )
                    {
                        return;
                    }
                    const bool wantsReload = guardedMessage && guardedMessage->clickedButton() == reload;
                    clearPrompt(promptSerial);
                    if ( guardedMessage )
                    {
                        guardedMessage->deleteLater();
                    }
                    if ( wantsReload && watched && watched->current() && watched
                        && watched->incarnation == incarnation && watched->acceptanceEpoch == acceptanceEpoch )
                    {
                        const auto query = watched->options.authoredRevision;
                        const auto reloadDocument = watched->options.reload;
                        const bool unchanged = !query || query() == authoredRevision;
                        if ( watched && watched->active && unchanged )
                        {
                            const LooseDocumentReloadResult result = reloadDocument();
                            if ( watched && watched->current() && watched )
                            {
                                if ( result.loaded )
                                {
                                    accept(watched->owner.data(), watched->options.path, result.acceptedFingerprint);
                                }
                                else if ( !result.error.isEmpty() )
                                {
                                    showReloadError(watched, result.error);
                                }
                            }
                        }
                    }
                    scheduleOffer();
                });
            observePromptDestruction(message, promptSerial);
            message->show();
            return;
        }
    }

    void showReloadError(const QPointer<WatchedDocument>& watched, const QString& detail)
    {
        if ( !watched || !watched->current() || !watched )
        {
            return;
        }
        const QString leaf = QFileInfo(watched->options.path).fileName();
        auto* const message = new QMessageBox(QMessageBox::Critical, QStringLiteral("Could not reload file"),
            QStringLiteral("Could not reload \"%1\".\n\n%2").arg(leaf, detail), QMessageBox::Ok, watched->owner.data());
        message->setObjectName(QStringLiteral("sed.external_document_reload_error"));
        message->setWindowModality(Qt::ApplicationModal);
        activePrompt_ = message;
        activeDocument_ = watched;
        const std::uint64_t promptSerial = activePromptSerial_ = ++epoch_;
        const QPointer<QMessageBox> guardedMessage(message);
        QObject::connect(message, &QDialog::finished, this, [this, guardedMessage, promptSerial]()
            {
                clearPrompt(promptSerial);
                scheduleOffer();
                if ( guardedMessage )
                {
                    guardedMessage->deleteLater();
                }
            });
        observePromptDestruction(message, promptSerial);
        message->show();
    }

    void clearPrompt(const std::uint64_t serial)
    {
        if ( activePromptSerial_ == serial )
        {
            activePromptSerial_ = 0;
            activePrompt_.clear();
            activeDocument_.clear();
        }
    }

    void observePromptDestruction(QMessageBox* message, const std::uint64_t serial)
    {
        // QObject has already cleared QPointers by destroyed(). A stable token
        // retires the exact dialog even when its owner deletes it abruptly.
        QObject::connect(message, &QObject::destroyed, this, [this, serial]()
            {
                clearPrompt(serial);
                scheduleOffer();
            });
    }

    QFileSystemWatcher filesystem_;
    QThreadPool pool_{};
    QTimer debounce_{};
    QTimer idle_{};
    QHash<QWidget*, QPointer<WatchedDocument>> documents_{};
    QHash<QString, Source> sources_{};
    QHash<QString, DirectoryWatch> directories_{};
    QSet<QString> pendingReads_{};
    std::deque<Offer> offers_{};
    QPointer<QMessageBox> activePrompt_{};
    QPointer<WatchedDocument> activeDocument_{};
    std::shared_ptr<std::atomic_bool> cancelled_{std::make_shared<std::atomic_bool>(false)};
    std::uint64_t epoch_{};
    std::uint64_t activePromptSerial_{};
    int activeReads_{};
    bool shuttingDown_{};
};

WatchedDocument::WatchedDocument(QWidget* ownerWidget, LooseDocumentWatchOptions configuration,
    LooseDocumentWatchService* manager)
    : QObject(ownerWidget)
    , owner(ownerWidget)
    , ownerKey(ownerWidget)
    , options(std::move(configuration))
    , service(manager)
    , accepted(acceptedRevision(options.acceptedFingerprint))
{
    if ( options.acceptedFingerprint.size() == 32 )
    {
        acknowledged = accepted;
    }
}

WatchedDocument::~WatchedDocument()
{
    if ( service )
    {
        service->removeDocument(this, false);
    }
}

bool WatchedDocument::current() const
{
    if ( !active || !owner )
    {
        return false;
    }
    const auto guard = options.stillCurrent;
    return !guard || guard();
}

LooseDocumentWatchService* sharedService(const bool create)
{
    static QPointer<LooseDocumentWatchService> service{};
    if ( !service && create && qApp )
    {
        service = new LooseDocumentWatchService(qApp);
    }
    return service.data();
}

} // namespace

void watchLooseDocument(QWidget* owner, LooseDocumentWatchOptions options)
{
    if ( auto* const service = sharedService(true) )
    {
        service->attach(owner, std::move(options));
    }
}

bool looseDocumentWatchMatches(const QWidget* owner, const QString& path, const QByteArray& fingerprint)
{
    auto* const service = sharedService(false);
    const WatchedDocument* const watched = service ? service->document(owner) : nullptr;
    return watched && watched->active && watched->options.path == path
        && watched->options.acceptedFingerprint == fingerprint;
}

void acceptLooseDocumentSource(QWidget* owner, const QString& path, const QByteArray& fingerprint)
{
    if ( auto* const service = sharedService(false) )
    {
        service->accept(owner, path, fingerprint);
    }
}

void requestLooseDocumentReload(QWidget* owner)
{
    if ( auto* const service = sharedService(false) )
    {
        service->forceOffer(owner);
    }
}

void stopLooseDocumentWatch(QWidget* owner)
{
    if ( auto* const service = sharedService(false) )
    {
        service->removeDocument(service->document(owner));
    }
}

} // namespace sed::ui
