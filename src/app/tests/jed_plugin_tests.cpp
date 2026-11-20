#include "test_common.hpp"

#include "main_window/main_window.hpp"
#include "app_settings.hpp"
#include "plugins/jed_plugin_manager.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QTemporaryDir>

#include <cstdint>
#include <utility>

namespace sed::app_tests
{
namespace
{

using app_plugins::JedPluginManager;
using app_plugins::PluginMenuEntry;
using app_plugins::PluginMenuFolder;

QStringList* capturedMessages = nullptr;
QtMessageHandler previousHandler = nullptr;

void captureMessage(const QtMsgType type, const QMessageLogContext& context, const QString& text)
{
    if ( capturedMessages )
    {
        capturedMessages->push_back(text);
    }
    if ( previousHandler )
    {
        previousHandler(type, context, text);
    }
}

class MessageCapture
{
public:
    MessageCapture()
    {
        capturedMessages = &messages;
        previousHandler = qInstallMessageHandler(captureMessage);
    }
    ~MessageCapture()
    {
        qInstallMessageHandler(previousHandler);
        capturedMessages = nullptr;
        previousHandler = nullptr;
    }
    MessageCapture(const MessageCapture&) = delete;
    MessageCapture& operator=(const MessageCapture&) = delete;

    bool contains(const QString& text) const
    {
        for ( const QString& message : messages )
        {
            if ( message.contains(text) )
            {
                return true;
            }
        }
        return false;
    }

    QStringList messages{};
};

QString librarySuffix()
{
#if defined(Q_OS_WIN)
    return QStringLiteral(".dll");
#else
    return QStringLiteral(".so");
#endif
}

void writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    if ( file.open(QIODevice::WriteOnly | QIODevice::Truncate) )
    {
        file.write(bytes);
    }
}

PluginMenuEntry libraryEntry(const char* path)
{
    PluginMenuEntry entry{};
    entry.path = QString::fromLocal8Bit(path);
    entry.title = QFileInfo(entry.path).fileName();

    return entry;
}

JedPluginManager* managerOf(MainWindow& window)
{
    return window.findChild<JedPluginManager*>(QStringLiteral("jedPluginManager"));
}

editor::DocumentState stateWithLights(const std::size_t count)
{
    editor::DocumentState state{};
    state.level.lights.resize(count);
    return state;
}

void testScanFollowsSedFolderLayout()
{
    QTemporaryDir root{};
    check(root.isValid(), "plugins: temporary folder is available");
    const QDir dir(root.path());
    const QString suffix = librarySuffix();
    writeFile(dir.filePath(QStringLiteral("b") + suffix), "x");
    writeFile(dir.filePath(QStringLiteral("A tool") + suffix), "x");
    writeFile(dir.filePath(QStringLiteral("A tool") + suffix + QStringLiteral(".dsc")),
        "name=Bake Lights\r\nkey=Ctrl+Shift+L\r\nignored line\r\n");
    writeFile(dir.filePath(QStringLiteral("readme.txt")), "x");
    writeFile(dir.filePath(QStringLiteral("companion.exe")), "x");
    writeFile(dir.filePath(QStringLiteral("companion.bat")), "x");
    check(dir.mkpath(QStringLiteral("Tools/Empty")), "plugins: nested folders are created");
    writeFile(dir.filePath(QStringLiteral("Tools/inner") + suffix), "x");

    const PluginMenuFolder scanned = app_plugins::scanPluginFolder(root.path());
    check(scanned.folders.size() == 1u && scanned.folders[0].title == QStringLiteral("Tools")
            && scanned.folders[0].entries.size() == 1u && scanned.folders[0].folders.empty(),
        "plugins: subfolders become submenus and empty folders are omitted");
    check(scanned.entries.size() == 2u, "plugins: only plug-in libraries are listed");
    if ( scanned.entries.size() == 2u )
    {
        check(scanned.entries[0].title == QStringLiteral("Bake Lights")
                && scanned.entries[0].shortcut == QKeySequence(QStringLiteral("Ctrl+Shift+L")),
            "plugins: a .dsc sidecar sets the menu caption and shortcut");
        check(scanned.entries[1].title == QStringLiteral("b") + suffix,
            "plugins: entries without a .dsc use the file name and sort case-insensitively");
    }
    check(app_plugins::scanPluginFolder(dir.filePath(QStringLiteral("missing"))).empty(),
        "plugins: a missing folder scans as empty");
}

void testMenuListsPluginsAndRuns()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(stateWithLights(0));
    JedPluginManager* const manager = managerOf(window);
    QMenu* const menu = window.menuBar()->findChild<QMenu*>(QStringLiteral("plugins"));
    check(manager != nullptr && menu != nullptr, "plugins: the main window owns a Plugins menu and manager");
    if ( !manager || !menu )
    {
        return;
    }

    QTemporaryDir empty{};
    manager->setPluginDirectory(empty.path());
    manager->populateMenu(menu);
    const QList<QAction*> emptyActions = menu->actions();
    check(!emptyActions.isEmpty() && emptyActions.front()->text() == QStringLiteral("(No plug-ins installed)")
            && !emptyActions.front()->isEnabled(),
        "plugins: an empty folder shows a disabled placeholder");
    check(menu->findChild<QAction*>(QStringLiteral("pluginsRescanAction")) != nullptr
            && menu->findChild<QAction*>(QStringLiteral("pluginsOpenFolderAction")) != nullptr,
        "plugins: the menu offers Rescan and Open Folder");

    QTemporaryDir folder{};
    const QString installed = QDir(folder.path()).filePath(QStringLiteral("Edit Plugin") + librarySuffix());
    check(QFile::copy(QString::fromLocal8Bit(SED_TEST_JED_PLUGIN_EDIT_PATH), installed),
        "plugins: the edit test plug-in is installed");
    manager->setPluginDirectory(folder.path());
    manager->populateMenu(menu);
    QAction* run = nullptr;
    for ( QAction* action : menu->actions() )
    {
        if ( action->text() == QFileInfo(installed).fileName() )
        {
            run = action;
        }
    }
    check(run != nullptr && run->isEnabled(), "plugins: an installed plug-in appears in the menu");
    if ( run )
    {
        run->trigger();
    }
    check(window.documentStateForTest().level.lights.size() == 2u && window.undoDepthForTest() == 1u,
        "plugins: triggering the menu entry runs the plug-in as one undo step");
}

void testEditIsOneUndoableStep()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(stateWithLights(1));
    JedPluginManager* const manager = managerOf(window);
    if ( !manager )
    {
        check(false, "plugins: manager exists");
        return;
    }
    MessageCapture capture{};
    const std::size_t undoBefore = window.undoDepthForTest();
    check(manager->run(libraryEntry(SED_TEST_JED_PLUGIN_EDIT_PATH)), "plugins: the edit plug-in reports success");
    check(!manager->hasPendingEditForTest(), "plugins: the edit is committed when the plug-in returns");
    check(window.documentStateForTest().level.lights.size() == 3u, "plugins: plug-in edits reach the document");
    check(window.undoDepthForTest() == undoBefore + 1u, "plugins: all plug-in edits form one undo step");
    check(capture.contains(QStringLiteral("test plug-in added two lights")),
        "plugins: PanMessage text reaches the message log");
    check(window.undoForTest() && window.documentStateForTest().level.lights.size() == 1u,
        "plugins: one undo restores the level from before the plug-in ran");

    check(manager->run(libraryEntry(SED_TEST_JED_PLUGIN_EDIT_PATH)),
        "plugins: a loaded plug-in can be run again");
    check(window.documentStateForTest().level.lights.size() == 3u, "plugins: rerunning reuses the loaded module");
}

void testSelectionOnlyRunAddsNoUndoStep()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(stateWithLights(2));
    JedPluginManager* const manager = managerOf(window);
    if ( !manager )
    {
        check(false, "plugins: manager exists");
        return;
    }
    check(manager->run(libraryEntry(SED_TEST_JED_PLUGIN_SELECT_PATH)), "plugins: the select plug-in reports success");
    check(window.documentStateForTest().selection.lights.contains(0), "plugins: plug-in selection reaches the editor");
    check(window.undoDepthForTest() == 0u, "plugins: selection-only runs do not add an undo step");
}

void testFailuresAreReported()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(stateWithLights(1));
    JedPluginManager* const manager = managerOf(window);
    if ( !manager )
    {
        check(false, "plugins: manager exists");
        return;
    }
    {
        MessageCapture capture{};
        check(!manager->run(libraryEntry(SED_TEST_JED_PLUGIN_FAIL_PATH)), "plugins: a false return is a failed run");
        check(capture.contains(QStringLiteral("returned error while executing")),
            "plugins: a false return reports SED's warning");
    }
    {
        MessageCapture capture{};
        check(!manager->run(libraryEntry(SED_TEST_JED_PLUGIN_NO_ENTRY_PATH)), "plugins: a DLL without an entry fails");
        check(capture.contains(QStringLiteral("No load function found")),
            "plugins: a missing entry point reports SED's error");
    }
    {
        MessageCapture capture{};
        check(!manager->run(libraryEntry(SED_TEST_JED_PLUGIN_SED_ONLY_PATH)), "plugins: an ISED-only plug-in fails");
        check(capture.contains(QStringLiteral("SED (ISED) plug-in")),
            "plugins: ISED-only plug-ins are reported as unsupported");
    }
    {
        QTemporaryDir folder{};
        QByteArray image(0x100, '\0');
        image[0] = 'M';
        image[1] = 'Z';
        image[0x3C] = static_cast<char>(0x80);
        image[0x80] = 'P';
        image[0x81] = 'E';
        const bool host64 = sizeof(void*) == 8u;
        image[0x84] = static_cast<char>(host64 ? 0x4C : 0x64);
        image[0x85] = static_cast<char>(host64 ? 0x01 : 0x86);
        const QString path = QDir(folder.path()).filePath(QStringLiteral("Light Baker") + librarySuffix());
        writeFile(path, image);
        PluginMenuEntry entry{};
        entry.path = path;

        MessageCapture capture{};
        check(!manager->run(entry), "plugins: a DLL for the other architecture is refused");
        check(capture.contains(host64 ? QStringLiteral("it is a 32-bit x86 DLL, but SED is 64-bit")
                                      : QStringLiteral("it is a 64-bit x64 DLL, but SED is 32-bit")),
            "plugins: an architecture mismatch names both architectures");
        check(capture.contains(host64 ? QStringLiteral("sed-jed-plugin-bridge-x86") : QStringLiteral("sed-jed-plugin-bridge-x64")),
            "plugins: an architecture mismatch without a bridge names the bridge to install");
    }
    check(window.documentStateForTest().level.lights.size() == 1u && window.undoDepthForTest() == 0u,
        "plugins: failed runs leave the document unchanged");
}

void testBridgedRunsMatchInProcess()
{
#if defined(SED_TEST_JED_PLUGIN_BRIDGE_DIR)
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(stateWithLights(1));
    JedPluginManager* const manager = managerOf(window);
    if ( !manager )
    {
        check(false, "bridge: manager exists");
        return;
    }
    manager->setBridgeDirectory(QString::fromLocal8Bit(SED_TEST_JED_PLUGIN_BRIDGE_DIR));
    manager->setRunOutOfProcess(true);
    {
        MessageCapture capture{};
        check(manager->run(libraryEntry(SED_TEST_JED_PLUGIN_EDIT_PATH)), "bridge: the edit plug-in reports success");
        check(window.documentStateForTest().level.lights.size() == 3u && window.undoDepthForTest() == 1u,
            "bridge: a bridged edit reaches the document as one undo step");
        check(capture.contains(QStringLiteral("test plug-in added two lights")),
            "bridge: PanMessage text from the bridge reaches the message log");
        check(window.undoForTest() && window.documentStateForTest().level.lights.size() == 1u,
            "bridge: one undo restores the level from before the bridged run");
    }
    check(manager->run(libraryEntry(SED_TEST_JED_PLUGIN_SELECT_PATH))
            && window.documentStateForTest().selection.lights.contains(0) && window.undoDepthForTest() == 0u,
        "bridge: a bridged selection-only run selects without an undo step");
    {
        MessageCapture capture{};
        check(!manager->run(libraryEntry(SED_TEST_JED_PLUGIN_FAIL_PATH))
                && capture.contains(QStringLiteral("returned error while executing")),
            "bridge: a false return reports SED's warning");
    }
    {
        MessageCapture capture{};
        check(!manager->run(libraryEntry(SED_TEST_JED_PLUGIN_NO_ENTRY_PATH))
                && capture.contains(QStringLiteral("No load function found")),
            "bridge: a missing entry point reports SED's error");
    }
    {
        MessageCapture capture{};
        check(!manager->run(libraryEntry(SED_TEST_JED_PLUGIN_CRASH_PATH)),
            "bridge: a plug-in that kills its process is a failed run");
        check(capture.contains(QStringLiteral("exited unexpectedly")),
            "bridge: SED survives a crashing plug-in and reports it");
        check(window.documentStateForTest().level.lights.size() == 2u && window.undoDepthForTest() == 1u,
            "bridge: edits committed before the crash stay as their own undo step");
    }
#else
    check(true, "bridge: not built");
#endif
}

void testMissingBridgeIsReported()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(stateWithLights(1));
    JedPluginManager* const manager = managerOf(window);
    if ( !manager )
    {
        check(false, "bridge: manager exists");
        return;
    }
    QTemporaryDir empty{};
    manager->setBridgeDirectory(empty.path());
    manager->setRunOutOfProcess(true);
    MessageCapture capture{};
    check(!manager->run(libraryEntry(SED_TEST_JED_PLUGIN_EDIT_PATH)), "bridge: a missing bridge fails the run");
    check(capture.contains(QStringLiteral("sed-jed-plugin-bridge-")), "bridge: a missing bridge is named");
    check(window.documentStateForTest().level.lights.size() == 1u && window.undoDepthForTest() == 0u,
        "bridge: a missing bridge leaves the document unchanged");
}

void testRunOutOfProcessSettingPersists()
{
    {
        QSettings settings = settingsStore();
        settings.remove(QStringLiteral("plugins/run_out_of_process"));
    }
    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        JedPluginManager* const manager = managerOf(window);
        QAction* const toggle = window.findChild<QAction*>(QStringLiteral("pluginsRunOutOfProcessAction"));
        check(manager && toggle && toggle->isCheckable() && !toggle->isChecked() && !manager->runOutOfProcess(),
            "bridge: Run Plug-ins in a Separate Process starts off");
        if ( !manager || !toggle )
        {
            return;
        }
        toggle->trigger();
        check(manager->runOutOfProcess(), "bridge: the toggle switches the manager to the bridge");
        QSettings settings = settingsStore();
        check(settings.value(QStringLiteral("plugins/run_out_of_process")).toBool(), "bridge: the toggle is saved");
    }
    {
        MainWindow reopened{};
        reopened.setAttribute(Qt::WA_DontShowOnScreen);
        JedPluginManager* const manager = managerOf(reopened);
        QAction* const toggle = reopened.findChild<QAction*>(QStringLiteral("pluginsRunOutOfProcessAction"));
        check(manager && manager->runOutOfProcess() && toggle && toggle->isChecked(),
            "bridge: a new window restores the saved choice");
    }
    QSettings settings = settingsStore();
    settings.remove(QStringLiteral("plugins/run_out_of_process"));
}

} // namespace

void runJedPluginTests()
{
    testScanFollowsSedFolderLayout();
    testMenuListsPluginsAndRuns();
    testEditIsOneUndoableStep();
    testSelectionOnlyRunAddsNoUndoStep();
    testFailuresAreReported();
    testBridgedRunsMatchInProcess();
    testMissingBridgeIsReported();
    testRunOutOfProcessSettingPersists();
}

} // namespace sed::app_tests
