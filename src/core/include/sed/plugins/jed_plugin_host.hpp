#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sed/editor/selection.hpp"
#include "sed/level.hpp"
#include "sed/plugins/jed_plugin_abi.hpp"

namespace sed::plugins
{

enum class JedMessageType : std::int32_t
{
    Info = 0,
    Warning = 1,
    Error = 2,
};

/** The editor's "current" items (JED Cur_SC, Cur_SF, ...); -1 means none. */
struct JedCursor
{
    std::int32_t sector{ -1 };
    std::int32_t surface{ -1 };
    std::int32_t edge{ -1 };
    std::int32_t vertex{ -1 };
    std::int32_t thing{ -1 };
    std::int32_t frame{ -1 };
    std::int32_t light{ -1 };
};

/**
 * Editor services the IJED host needs. Implemented by the application.
 *
 * `level()`/`selection()` return the state a plug-in currently observes.
 * `mutableLevel()`/`mutableSelection()` open (or continue) one pending plug-in
 * edit; the application commits that edit as a single undo step when the
 * plug-in calls StartUndo again, UpdateMap, or returns control to the editor.
 * References returned by these functions are only valid until the next call.
 */
class JedHostContext
{
public:
    virtual const Level& level() const = 0;
    virtual Level& mutableLevel() = 0;
    virtual const editor::Selection& selection() const = 0;
    virtual editor::Selection& mutableSelection() = 0;

    virtual void startUndo(const std::string& label) = 0;
    virtual void updateMap() = 0;
    virtual void message(JedMessageType type, const std::string& text) = 0;

    virtual std::int32_t mapMode() const = 0;
    virtual void setMapMode(std::int32_t mode) = 0;
    virtual JedCursor cursor() const = 0;
    virtual void setCursor(const JedCursor& cursor) = 0;

    /** Reads a whole file; `gameFile` resolves through project and game resources. */
    virtual std::optional<std::vector<std::uint8_t>> readFile(const std::string& name, bool gameFile) = 0;
    /** COG script symbols (with defaults) for IJEDLevel::AddCOG. */
    virtual std::optional<std::vector<CogValue>> cogScriptSymbols(const std::string& scriptName) = 0;

    virtual std::string jedString(std::int32_t what) const = 0;
    virtual std::intptr_t windowHandle(std::int32_t which) const = 0;
    virtual bool isLayerVisible(std::int32_t layer) const = 0;

protected:
    ~JedHostContext() = default;
};

/**
 * Owns the IJED and IJEDLevel objects handed to legacy JED plug-ins.
 *
 * Every entry point validates its indices and never lets a C++ exception cross
 * the plug-in boundary. Methods SED does not provide return E_NOTIMPL, -1,
 * false, or a null pointer and report a warning once per method. Returned
 * strings stay valid for the next 63 string-returning calls, as in SED's
 * 32-entry cache, and carry extra NUL padding so wide-character readers stop.
 * The host must outlive every loaded plug-in.
 */
class JedPluginHost
{
public:
    explicit JedPluginHost(JedHostContext& context);
    ~JedPluginHost();

    JedPluginHost(const JedPluginHost&) = delete;
    JedPluginHost& operator=(const JedPluginHost&) = delete;

    jed::IJED* jed() noexcept;
    jed::IJEDLevel* level() noexcept;

    /** Host version reported by IJED::GetVersion (JED 0.951 compatible). */
    static constexpr double kReportedVersion = 0.951;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sed::plugins
