#include "plugins/jed_plugin_manager.hpp"

#include "app_settings.hpp"
#include "sed/plugins/jed_plugin_abi.hpp"
#include "sed/plugins/jed_plugin_bridge.hpp"
#include "sed/plugins/plugin_binary.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QMenu>
#include <QProcess>
#include <QProgressDialog>
#include <QSettings>
#include <QTimer>
#include <QEventLoop>
#include <QUrl>

#include <exception>
#include <utility>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace sed::app_plugins
{
namespace
{

constexpr int kMaximumFolderDepth = 8;
constexpr qint64 kMaximumDescriptionBytes = 64 * 1024;
constexpr qint64 kArchitectureProbeBytes = 4096;

bool isLibrarySuffix(const QString& suffix)
{
#ifdef Q_OS_WIN
    return suffix == QStringLiteral("dll");
#else
    return suffix == QStringLiteral("so");
#endif
}

void applyDescription(PluginMenuEntry& entry, const QString& descriptionPath)
{
    QFile file(descriptionPath);
    if ( file.size() > kMaximumDescriptionBytes || !file.open(QIODevice::ReadOnly | QIODevice::Text) )
    {
        return;
    }
    const QStringList lines = QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'));
    for ( const QString& rawLine : lines )
    {
        const qsizetype equals = rawLine.indexOf(QLatin1Char('='));
        if ( equals <= 0 )
        {
            continue;
        }
        const QString key = rawLine.left(equals).trimmed().toLower();
        const QString value = rawLine.mid(equals + 1).trimmed();
        if ( key == QStringLiteral("name") && !value.isEmpty() )
        {
            entry.title = value;
        }
        else if ( key == QStringLiteral("key") )
        {
            entry.shortcut = QKeySequence(value, QKeySequence::PortableText);
        }
    }
}

PluginMenuFolder scanFolder(const QDir& directory, const int depth)
{
    PluginMenuFolder folder{};
    folder.title = directory.dirName();
    if ( depth < kMaximumFolderDepth )
    {
        const QFileInfoList subfolders =
            directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name | QDir::IgnoreCase);
        for ( const QFileInfo& info : subfolders )
        {
            PluginMenuFolder child = scanFolder(QDir(info.absoluteFilePath()), depth + 1);
            if ( !child.empty() )
            {
                folder.folders.push_back(std::move(child));
            }
        }
    }
    const QFileInfoList files = directory.entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase);
    for ( const QFileInfo& info : files )
    {
        const QString suffix = info.suffix().toLower();
        if ( !isLibrarySuffix(suffix) )
        {
            continue;
        }
        PluginMenuEntry entry{};
        entry.path = info.absoluteFilePath();
        entry.title = info.fileName();
        applyDescription(entry, entry.path + QStringLiteral(".dsc"));
        folder.entries.push_back(std::move(entry));
    }
    return folder;
}

plugins::PluginBinaryArchitecture hostArchitecture()
{
#if defined(Q_PROCESSOR_ARM_64)
    return plugins::PluginBinaryArchitecture::arm64;
#elif defined(Q_PROCESSOR_X86_64)
    return plugins::PluginBinaryArchitecture::x64;
#elif defined(Q_PROCESSOR_X86_32)
    return plugins::PluginBinaryArchitecture::x86;
#else
    return plugins::PluginBinaryArchitecture::unknown;
#endif
}

plugins::PluginBinaryArchitecture fileArchitecture(const QString& path)
{
    QFile file(path);
    if ( !file.open(QIODevice::ReadOnly) )
    {
        return plugins::PluginBinaryArchitecture::unknown;
    }
    const QByteArray head = file.read(kArchitectureProbeBytes);
    return plugins::detectPeArchitecture(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(head.constData()), static_cast<std::size_t>(head.size())));
}

} // namespace

PluginMenuFolder scanPluginFolder(const QString& directory)
{
    const QDir dir(directory);
    if ( directory.isEmpty() || !dir.exists() )
    {
        return {};
    }
    return scanFolder(dir, 0);
}

JedPluginManager::JedPluginManager(PluginEditorServices services, QObject* parent)
    : QObject(parent)
    , services_(std::move(services))
    , host_(*this)
{
    setObjectName(QStringLiteral("jedPluginManager"));
    bridgeDirectory_ = QCoreApplication::applicationDirPath();
}

JedPluginManager::~JedPluginManager()
{
    // Never unload plug-in modules: their windows and threads may still run.
    for ( std::unique_ptr<QLibrary>& library : libraries_ )
    {
        (void)library.release();
    }
}

void JedPluginManager::setPluginDirectory(const QString& directory)
{
    pluginDirectory_ = directory;
}

void JedPluginManager::setBridgeDirectory(const QString& directory)
{
    bridgeDirectory_ = directory;
}

void JedPluginManager::populateMenu(QMenu* menu)
{
    if ( menu == nullptr )
    {
        return;
    }
    menu->clear();
    const PluginMenuFolder root = scanPluginFolder(pluginDirectory_);
    if ( root.empty() )
    {
        QAction* none = menu->addAction(tr("(No plug-ins installed)"));
        none->setEnabled(false);
    }
    else
    {
        addFolder(menu, root);
    }
    menu->addSeparator();
    QAction* rescan = menu->addAction(tr("&Rescan Plug-ins"));
    rescan->setObjectName(QStringLiteral("pluginsRescanAction"));
    connect(rescan, &QAction::triggered, this, [this, menu]()
        {
            // Rebuilding deletes the triggering action; defer until its signal returns.
            QTimer::singleShot(0, this, [this, menu]() { populateMenu(menu); });
        });
    QAction* isolate = menu->addAction(tr("Run Plug-ins in a &Separate Process"));
    isolate->setObjectName(QStringLiteral("pluginsRunOutOfProcessAction"));
    isolate->setCheckable(true);
    isolate->setChecked(runOutOfProcess_);
    isolate->setStatusTip(tr("Run each plug-in in the plug-in bridge, so a crashing plug-in cannot close SED. "
                             "Different-architecture plug-ins always use a matching bridge."));
    connect(isolate, &QAction::toggled, this, [this](const bool enabled)
        {
            runOutOfProcess_ = enabled;
            QSettings settings = settingsStore();
            settings.setValue(QStringLiteral("plugins/run_out_of_process"), enabled);
        });
    QAction* openFolder = menu->addAction(tr("Open Plug-ins &Folder"));
    openFolder->setObjectName(QStringLiteral("pluginsOpenFolderAction"));
    connect(openFolder, &QAction::triggered, this, [this]()
        {
            if ( pluginDirectory_.isEmpty() || !QDir().mkpath(pluginDirectory_) )
            {
                report(plugins::JedMessageType::Warning, tr("Couldn't create the plug-ins folder %1").arg(pluginDirectory_));
                return;
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(pluginDirectory_));
        });
}

void JedPluginManager::addFolder(QMenu* menu, const PluginMenuFolder& folder)
{
    for ( const PluginMenuFolder& child : folder.folders )
    {
        addFolder(menu->addMenu(child.title), child);
    }
    for ( const PluginMenuEntry& entry : folder.entries )
    {
        QAction* action = menu->addAction(entry.title);
        action->setData(entry.path);
        action->setStatusTip(QDir::toNativeSeparators(entry.path));
        if ( !entry.shortcut.isEmpty() )
        {
            action->setShortcut(entry.shortcut);
        }
        connect(action, &QAction::triggered, this, [this, entry]() { run(entry); });
    }
}

bool JedPluginManager::run(const PluginMenuEntry& entry)
{
    const bool ok = runLibrary(entry);
    commitPendingEdit();
    return ok;
}

bool JedPluginManager::runLibrary(const PluginMenuEntry& entry)
{
    const QString name = QFileInfo(entry.path).fileName();
    const plugins::PluginBinaryArchitecture pluginArch = fileArchitecture(entry.path);
    const plugins::PluginBinaryArchitecture hostArch = hostArchitecture();
    const bool mismatch = pluginArch != plugins::PluginBinaryArchitecture::unknown
        && hostArch != plugins::PluginBinaryArchitecture::unknown && pluginArch != hostArch;
    if ( mismatch || runOutOfProcess_ )
    {
        const plugins::PluginBinaryArchitecture bridgeArch = mismatch ? pluginArch : hostArch;
#ifdef Q_OS_WIN
        constexpr bool windows = true;
#else
        constexpr bool windows = false;
#endif
        const QString bridgeName = QString::fromStdString(plugins::pluginBridgeExecutableName(bridgeArch, windows));
        const QString bridgePath = bridgeName.isEmpty() ? QString() : QDir(bridgeDirectory_).filePath(bridgeName);
        if ( !bridgePath.isEmpty() && QFileInfo(bridgePath).isFile() )
        {
            return runBridged(entry, bridgePath);
        }
        if ( mismatch )
        {
            report(plugins::JedMessageType::Error,
                tr("Couldn't load plugin %1: it is a %2 DLL, but SED is %3. Install the %2 plug-in bridge (%4) "
                   "beside SED, or a %3 build of this plug-in.")
                    .arg(name, QString::fromLatin1(plugins::pluginArchitectureName(pluginArch)),
                        QString::fromLatin1(plugins::pluginArchitectureName(hostArch)), bridgeName));
        }
        else
        {
            report(plugins::JedMessageType::Error,
                tr("Couldn't run plugin %1 in a separate process: the plug-in bridge %2 is not installed beside SED.")
                    .arg(name, bridgeName.isEmpty() ? tr("for this architecture") : bridgeName));
        }
        return false;
    }

    const QString canonical = QFileInfo(entry.path).canonicalFilePath();
    QLibrary* library = nullptr;
    for ( const std::unique_ptr<QLibrary>& loaded : libraries_ )
    {
        if ( QFileInfo(loaded->fileName()).canonicalFilePath() == canonical )
        {
            library = loaded.get();
            break;
        }
    }
    if ( library == nullptr )
    {
        auto candidate = std::make_unique<QLibrary>(entry.path);
        if ( !candidate->load() )
        {
            report(plugins::JedMessageType::Error, tr("Couldn't load plugin %1: %2").arg(name, candidate->errorString()));
            return false;
        }
        const bool jedPlugin = candidate->resolve(plugins::jed::kJedPluginLoadStdCallSymbol) != nullptr
            || candidate->resolve(plugins::jed::kJedPluginLoadSymbol) != nullptr;
        if ( !jedPlugin )
        {
            const bool sedPlugin = candidate->resolve(plugins::jed::kSedPluginLoadStdCallSymbol) != nullptr
                || candidate->resolve(plugins::jed::kSedPluginLoadSymbol) != nullptr;
            candidate->unload();
            report(plugins::JedMessageType::Error,
                sedPlugin ? tr("The DLL %1 is a SED (ISED) plug-in. SED supports JED (IJED) plug-ins only.").arg(name)
                          : tr("The DLL %1 is not a SED or JED plug-in: No load function found!").arg(name));
            return false;
        }
        if (!plugins::jed::kSupportsPlainEntry
            && !candidate->resolve(plugins::jed::kJedPluginLoadStdCallSymbol))
        {
            candidate->unload();
            report(plugins::JedMessageType::Error,
                tr("%1 exports only the Delphi register entry JEDPluginLoad. This x86 host requires JEDPluginLoadStdCall.").arg(name));
            return false;
        }
        library = candidate.get();
        libraries_.push_back(std::move(candidate));
    }

    bool result = false;
    try
    {
        if ( auto stdCall = reinterpret_cast<plugins::jed::JedPluginLoadStdCall>(
                 library->resolve(plugins::jed::kJedPluginLoadStdCallSymbol)) )
        {
            result = stdCall(host_.jed());
        }
        else if ( auto plain = reinterpret_cast<plugins::jed::JedPluginLoad>(
                      library->resolve(plugins::jed::kJedPluginLoadSymbol)) )
        {
            result = plain(host_.jed());
        }
    }
    catch ( const std::exception& error )
    {
        report(plugins::JedMessageType::Error, tr("Exception raised by %1: %2").arg(name, QString::fromLocal8Bit(error.what())));
        return false;
    }
    catch ( ... )
    {
        report(plugins::JedMessageType::Error, tr("Exception raised by %1: unknown exception").arg(name));
        return false;
    }
    if ( !result )
    {
        report(plugins::JedMessageType::Warning, tr("Plug-in %1 returned error while executing!").arg(name));
    }
    return result;
}

bool JedPluginManager::runBridged(const PluginMenuEntry& entry, const QString& bridgePath)
{
    namespace bridge = plugins::bridge;
    const QString name = QFileInfo(entry.path).fileName();
    commitPendingEdit();

    bridge::RunRequest request{};
    request.pluginPath = QFileInfo(entry.path).absoluteFilePath().toStdString();
    request.level = services_.level();
    request.selection = services_.selection();
    request.cursor = services_.cursor();
    request.mapMode = services_.mapMode();
    for ( std::int32_t what = plugins::jed::js_ProjectDir; what <= plugins::jed::js_mp3gob; ++what )
    {
        request.jedStrings.emplace_back(what, jedString(what));
    }
    for ( std::int32_t which = plugins::jed::jw_Main; which <= plugins::jed::jw_ToolWindow; ++which )
    {
        request.windowHandles.emplace_back(which, static_cast<std::int64_t>(windowHandle(which)));
    }
    for ( std::size_t layer = 0; layer < request.level.layers.size(); ++layer )
    {
        if ( !isLayerVisible(static_cast<std::int32_t>(layer)) )
        {
            request.hiddenLayers.push_back(static_cast<std::int32_t>(layer));
        }
    }

    auto expectedRevision = services_.documentRevision();
    bool editRejected = false;
    bridge::HostServices hostServices{};
    hostServices.message = [this](const plugins::JedMessageType type, const std::string& text)
    {
        message(type, text);
    };
    hostServices.commit = [&](const std::string& label, Level&& level, editor::Selection&& selection)
    {
        if (editRejected || services_.documentRevision() != expectedRevision
            || !services_.commit(label, editor::DocumentState{std::move(level), std::move(selection)}))
        {
            editRejected = true;
            return;
        }
        expectedRevision = services_.documentRevision();
    };
    hostServices.selectionChanged = [&](editor::Selection&& selection)
    {
        if (editRejected)
        {
            return;
        }
        services_.liveSelection() = std::move(selection);
        services_.selectionChanged();
    };
    hostServices.setCursor = [&](const plugins::JedCursor& cursor)
    {
        if (editRejected)
        {
            return;
        }
        services_.setCursor(cursor);
    };
    hostServices.setMapMode = [&](const std::int32_t mode)
    {
        if (editRejected)
        {
            return;
        }
        services_.setMapMode(mode);
    };
    hostServices.readGameFile = [this](const std::string& file)
    {
        return readFile(file, true);
    };
    hostServices.cogSymbols = [this](const std::string& script)
    {
        return cogScriptSymbols(script);
    };
    bridge::HostSession session(std::move(hostServices));

    QProcess process{};
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setProgram(bridgePath);
    process.setArguments({ QStringLiteral("--serve") });
    process.setWorkingDirectory(QFileInfo(entry.path).absolutePath());
    process.start();
    if ( !process.waitForStarted(10000) )
    {
        report(plugins::JedMessageType::Error,
            tr("Couldn't start the plug-in bridge for %1: %2").arg(name, process.errorString()));
        return false;
    }
#ifdef Q_OS_WIN
    // Let the plug-in's dialogs come to the front.
    AllowSetForegroundWindow(static_cast<DWORD>(process.processId()));
#endif

    QProgressDialog progress(tr("Running %1 in the plug-in bridge…").arg(name), tr("Cancel"), 0, 0,
        QApplication::activeWindow());
    progress.setObjectName(QStringLiteral("pluginBridgeProgress"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    QEventLoop loop{};
    bool cancelled = false;
    const auto pump = [&]()
    {
        const QByteArray data = process.readAllStandardOutput();
        if ( data.isEmpty() )
        {
            return;
        }
        if (editRejected || services_.documentRevision() != expectedRevision)
        {
            editRejected = true;
            loop.quit();
            return;
        }
        const std::vector<std::byte> replies = session.receive(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(data.constData()), static_cast<std::size_t>(data.size())));
        if ( !replies.empty() )
        {
            process.write(reinterpret_cast<const char*>(replies.data()), static_cast<qint64>(replies.size()));
        }
        if ( editRejected || session.finished() || session.failed() )
        {
            loop.quit();
        }
    };
    connect(&process, &QProcess::readyReadStandardOutput, &loop, pump);
    connect(&process, &QProcess::finished, &loop, [&]()
        {
            pump();
            loop.quit();
        });
    connect(&progress, &QProgressDialog::canceled, &loop, [&]()
        {
            cancelled = true;
            loop.quit();
        });

    try
    {
        const std::vector<std::byte> run = session.start(request);
        process.write(reinterpret_cast<const char*>(run.data()), static_cast<qint64>(run.size()));
    }
    catch ( const std::exception& error )
    {
        process.kill();
        process.waitForFinished(5000);
        report(plugins::JedMessageType::Error,
            tr("Couldn't send the level to the plug-in bridge for %1: %2").arg(name, QString::fromLocal8Bit(error.what())));
        return false;
    }
    progress.show();
    if ( process.state() != QProcess::NotRunning && !session.finished() && !session.failed() )
    {
        loop.exec();
    }
    // QProgressDialog::close() emits canceled(); only a cancel during the run counts.
    progress.disconnect(&loop);
    progress.close();

    if ( session.finished() )
    {
        process.closeWriteChannel();
        if ( !process.waitForFinished(5000) )
        {
            process.kill();
            process.waitForFinished(5000);
        }
    }
    else
    {
        process.kill();
        process.waitForFinished(5000);
    }

    if (editRejected)
    {
        report(plugins::JedMessageType::Warning, tr("A plug-in edit was discarded because the document changed or could no longer be edited."));
        return false;
    }
    if ( cancelled && !session.finished() )
    {
        report(plugins::JedMessageType::Warning,
            tr("Plug-in %1 was cancelled. Edits it committed before cancelling remain in the undo history.").arg(name));
        return false;
    }
    if ( session.failed() )
    {
        report(plugins::JedMessageType::Error,
            tr("The plug-in bridge for %1 failed: %2").arg(name, QString::fromStdString(session.error())));
        return false;
    }
    if ( !session.finished() )
    {
        report(plugins::JedMessageType::Error,
            tr("Plug-in %1 exited unexpectedly (bridge exit code %2). Edits it committed before it stopped remain "
               "in the undo history.")
                .arg(name)
                .arg(process.exitCode()));
        return false;
    }
    if ( !session.pluginLoaded() )
    {
        report(plugins::JedMessageType::Error, QString::fromLocal8Bit(session.error().c_str()));
        return false;
    }
    if ( !session.error().empty() )
    {
        report(plugins::JedMessageType::Error, tr("%1: %2").arg(name, QString::fromLocal8Bit(session.error().c_str())));
    }
    if ( !session.pluginResult() )
    {
        report(plugins::JedMessageType::Warning, tr("Plug-in %1 returned error while executing!").arg(name));
    }
    return session.pluginResult();
}

void JedPluginManager::ensurePending()
{
    if ( pending_ )
    {
        return;
    }
    pending_ = editor::DocumentState{ services_.level(), services_.selection() };
    pendingRevision_ = services_.documentRevision();
    pendingLabel_ = nextLabel_.empty() ? std::string("Plugin Edit") : nextLabel_;
    nextLabel_.clear();
}

void JedPluginManager::scheduleCommit()
{
    if ( commitScheduled_ )
    {
        return;
    }
    commitScheduled_ = true;
    QTimer::singleShot(0, this, [this]()
        {
            commitScheduled_ = false;
            commitPendingEdit();
        });
}

void JedPluginManager::flushSelection()
{
    if ( selectionDirty_ )
    {
        selectionDirty_ = false;
        services_.selectionChanged();
    }
}

void JedPluginManager::commitPendingEdit()
{
    if ( pending_ )
    {
        editor::DocumentState next = std::move(*pending_);
        pending_.reset();
        if ( services_.documentRevision() != pendingRevision_ )
        {
            pendingCursor_.reset();
            report(plugins::JedMessageType::Warning,
                tr("A plug-in edit was discarded because the level changed before it finished."));
        }
        else
        {
            services_.commit(pendingLabel_, std::move(next));
            selectionDirty_ = false;
        }
    }
    if ( pendingCursor_ )
    {
        const plugins::JedCursor cursor = *pendingCursor_;
        pendingCursor_.reset();
        services_.setCursor(cursor);
    }
    flushSelection();
}

const Level& JedPluginManager::level() const
{
    return pending_ ? pending_->level : services_.level();
}

Level& JedPluginManager::mutableLevel()
{
    ensurePending();
    scheduleCommit();
    return pending_->level;
}

const editor::Selection& JedPluginManager::selection() const
{
    return pending_ ? pending_->selection : services_.selection();
}

editor::Selection& JedPluginManager::mutableSelection()
{
    scheduleCommit();
    if ( pending_ )
    {
        return pending_->selection;
    }
    selectionDirty_ = true;
    return services_.liveSelection();
}

void JedPluginManager::startUndo(const std::string& label)
{
    commitPendingEdit();
    nextLabel_ = label.empty() ? std::string{} : "Plugin: " + label;
}

void JedPluginManager::updateMap()
{
    commitPendingEdit();
}

void JedPluginManager::message(const plugins::JedMessageType type, const std::string& text)
{
    report(type, QString::fromLocal8Bit(text.data(), static_cast<qsizetype>(text.size())));
}

void JedPluginManager::report(const plugins::JedMessageType type, const QString& text)
{
    if ( services_.message )
    {
        services_.message(type, text);
    }
}

std::int32_t JedPluginManager::mapMode() const
{
    return services_.mapMode();
}

void JedPluginManager::setMapMode(const std::int32_t mode)
{
    commitPendingEdit();
    services_.setMapMode(mode);
}

plugins::JedCursor JedPluginManager::cursor() const
{
    return pendingCursor_ ? *pendingCursor_ : services_.cursor();
}

void JedPluginManager::setCursor(const plugins::JedCursor& cursor)
{
    if ( pending_ )
    {
        // Defer until the edit commits: the cursor may name items that only exist in the pending level.
        pendingCursor_ = cursor;
        return;
    }
    services_.setCursor(cursor);
}

std::optional<std::vector<std::uint8_t>> JedPluginManager::readFile(const std::string& name, const bool gameFile)
{
    if ( gameFile )
    {
        std::optional<std::vector<std::byte>> bytes = services_.readGameFile(name);
        if ( !bytes )
        {
            return std::nullopt;
        }
        std::vector<std::uint8_t> out(bytes->size());
        for ( std::size_t i = 0; i < bytes->size(); ++i )
        {
            out[i] = std::to_integer<std::uint8_t>((*bytes)[i]);
        }
        return out;
    }
    QFile file(QString::fromLocal8Bit(name.data(), static_cast<qsizetype>(name.size())));
    if ( !file.open(QIODevice::ReadOnly) || file.size() < 0
        || static_cast<std::uint64_t>(file.size()) > kMaximumPluginFileBytes )
    {
        return std::nullopt;
    }
    const QByteArray data = file.readAll();
    if ( data.size() != file.size() )
    {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(data.begin(), data.end());
}

std::optional<std::vector<CogValue>> JedPluginManager::cogScriptSymbols(const std::string& scriptName)
{
    return services_.cogScriptSymbols(scriptName);
}

std::string JedPluginManager::jedString(const std::int32_t what) const
{
    return services_.jedString(what).toLocal8Bit().toStdString();
}

std::intptr_t JedPluginManager::windowHandle(const std::int32_t which) const
{
    return services_.windowHandle ? services_.windowHandle(which) : 0;
}

bool JedPluginManager::isLayerVisible(const std::int32_t layer) const
{
    return services_.isLayerVisible ? services_.isLayerVisible(layer) : true;
}

} // namespace sed::app_plugins
