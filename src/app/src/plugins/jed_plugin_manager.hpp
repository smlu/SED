#pragma once

#include "sed/editor/document.hpp"
#include "sed/plugins/jed_plugin_host.hpp"
#include "sed/values.hpp"

#include <QKeySequence>
#include <QObject>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class QLibrary;
class QMenu;

namespace sed::app_plugins
{

struct PluginDocumentRevision
{
    std::uint64_t epoch{};
    std::uint64_t undoSignature{};
    bool operator==(const PluginDocumentRevision&) const = default;
};

/** One runnable entry in the Plugins menu (SED/JED `plugins` folder layout). */
struct PluginMenuEntry
{
    QString title{};
    QString path{};
    QKeySequence shortcut{};
};

struct PluginMenuFolder
{
    QString title{};
    std::vector<PluginMenuFolder> folders{};
    std::vector<PluginMenuEntry> entries{};

    bool empty() const noexcept
    {
        return folders.empty() && entries.empty();
    }
};

/**
 * Lists native plug-in libraries and nested folders, sorted without regard to
 * case. A <library>.dsc sidecar may set the menu caption (name=) and key=.
 */
PluginMenuFolder scanPluginFolder(const QString& directory);

/** Editor services the plug-in bridge needs from the main window. */
struct PluginEditorServices
{
    std::function<const Level&()> level;
    std::function<const editor::Selection&()> selection;
    std::function<editor::Selection&()> liveSelection;
    std::function<void()> selectionChanged;
    std::function<PluginDocumentRevision()> documentRevision;
    std::function<bool(const std::string& label, editor::DocumentState&& next)> commit;
    std::function<std::int32_t()> mapMode;
    std::function<void(std::int32_t)> setMapMode;
    std::function<plugins::JedCursor()> cursor;
    std::function<void(const plugins::JedCursor&)> setCursor;
    std::function<void(plugins::JedMessageType, const QString&)> message;
    std::function<std::optional<std::vector<std::byte>>(const std::string&)> readGameFile;
    std::function<std::optional<std::vector<CogValue>>(const std::string&)> cogScriptSymbols;
    std::function<QString(std::int32_t)> jedString;
    std::function<std::intptr_t(std::int32_t)> windowHandle;
    std::function<bool(std::int32_t)> isLayerVisible;
};

/**
 * Loads legacy JED plug-ins and bridges their IJED calls to the document.
 *
 * Plug-in reads see the live document until the first write, which copies
 * the document into one pending working state. StartUndo, UpdateMap, the
 * plug-in's load function returning, or the next event-loop turn commits that
 * state as one undoable step. A pending edit is discarded with a warning if
 * the document changed underneath it. Loaded libraries are never unloaded
 * because modeless plug-in windows may keep calling back into the host.
 */
class JedPluginManager final : public QObject, private plugins::JedHostContext
{
    Q_OBJECT

public:
    static constexpr std::size_t kMaximumPluginFileBytes = 256u * 1024u * 1024u;

    JedPluginManager(PluginEditorServices services, QObject* parent);
    ~JedPluginManager() override;

    void setPluginDirectory(const QString& directory);
    const QString& pluginDirectory() const noexcept
    {
        return pluginDirectory_;
    }

    /** Rebuilds `menu` from the plug-in folder plus Rescan / Open Folder actions. */
    /** Folder searched for sed-jed-plugin-bridge-<arch>; defaults to SED's own folder. */
    void setBridgeDirectory(const QString& directory);
    const QString& bridgeDirectory() const noexcept
    {
        return bridgeDirectory_;
    }

    /** Runs every plug-in through the bridge, so a crashing plug-in cannot close SED. */
    void setRunOutOfProcess(bool enabled) noexcept
    {
        runOutOfProcess_ = enabled;
    }
    bool runOutOfProcess() const noexcept
    {
        return runOutOfProcess_;
    }

    void populateMenu(QMenu* menu);

    /** Runs a plug-in entry. Returns false if it could not be started or reported failure. */
    bool run(const PluginMenuEntry& entry);

    /** Commits any pending plug-in edit now. */
    void commitPendingEdit();

    bool hasPendingEditForTest() const noexcept
    {
        return pending_.has_value();
    }

private:
    const Level& level() const override;
    Level& mutableLevel() override;
    const editor::Selection& selection() const override;
    editor::Selection& mutableSelection() override;
    void startUndo(const std::string& label) override;
    void updateMap() override;
    void message(plugins::JedMessageType type, const std::string& text) override;
    std::int32_t mapMode() const override;
    void setMapMode(std::int32_t mode) override;
    plugins::JedCursor cursor() const override;
    void setCursor(const plugins::JedCursor& cursor) override;
    std::optional<std::vector<std::uint8_t>> readFile(const std::string& name, bool gameFile) override;
    std::optional<std::vector<CogValue>> cogScriptSymbols(const std::string& scriptName) override;
    std::string jedString(std::int32_t what) const override;
    std::intptr_t windowHandle(std::int32_t which) const override;
    bool isLayerVisible(std::int32_t layer) const override;

    bool runLibrary(const PluginMenuEntry& entry);
    bool runBridged(const PluginMenuEntry& entry, const QString& bridgePath);
    void addFolder(QMenu* menu, const PluginMenuFolder& folder);
    void ensurePending();
    void scheduleCommit();
    void flushSelection();
    void report(plugins::JedMessageType type, const QString& text);

    PluginEditorServices services_;
    plugins::JedPluginHost host_;
    QString pluginDirectory_{};
    QString bridgeDirectory_{};
    bool runOutOfProcess_{ false };
    std::vector<std::unique_ptr<QLibrary>> libraries_{};
    std::optional<editor::DocumentState> pending_{};
    PluginDocumentRevision pendingRevision_{};
    std::string pendingLabel_{};
    std::string nextLabel_{};
    std::optional<plugins::JedCursor> pendingCursor_{};
    bool selectionDirty_{ false };
    bool commitScheduled_{ false };
};

} // namespace sed::app_plugins
