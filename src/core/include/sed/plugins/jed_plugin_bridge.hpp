#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "sed/editor/selection.hpp"
#include "sed/level.hpp"
#include "sed/plugins/jed_plugin_host.hpp"
#include "sed/values.hpp"

/**
 * Out-of-process JED plug-in bridge.
 *
 * A 64-bit editor cannot load a 32-bit DLL. The bridge is a small helper
 * process built for the plug-in's architecture: it receives a snapshot of the
 * level, selection, cursor, and host strings, runs the plug-in against its own
 * JedPluginHost, and sends every pending edit back as one Commit message. The
 * editor applies each Commit as one undo step, exactly as the in-process host
 * does at StartUndo, UpdateMap, SetMapMode, and plug-in return.
 *
 * Frames are `magic, kind, payload length` (little-endian u32 each) followed by
 * the payload. Levels travel as native SED bytes. Both sides treat incoming
 * data as untrusted: every count, length, and frame size is bounded before
 * allocation, and a malformed frame ends the session without publishing data.
 */
namespace sed::plugins::bridge
{

inline constexpr std::uint32_t kFrameMagic = 0x42444552u; // "REDB"
inline constexpr std::uint32_t kProtocolVersion = 1u;
inline constexpr std::size_t kFrameHeaderBytes = 12u;
/** Largest payload either side accepts: a maximum SED level plus selection and strings. */
inline constexpr std::uint64_t kMaximumPayloadBytes = 320ull * 1024ull * 1024ull;

enum class MessageKind : std::uint32_t
{
    Run = 1,
    Done = 2,
    Message = 3,
    Commit = 4,
    SelectionChanged = 5,
    SetCursor = 6,
    SetMapMode = 7,
    ReadGameFile = 8,
    ReadGameFileReply = 9,
    CogSymbols = 10,
    CogSymbolsReply = 11,
};

/** Everything the bridge needs to answer IJED queries without a round trip. */
struct RunRequest
{
    std::string pluginPath{};
    Level level{};
    editor::Selection selection{};
    JedCursor cursor{};
    std::int32_t mapMode{};
    std::vector<std::pair<std::int32_t, std::string>> jedStrings{};
    std::vector<std::pair<std::int32_t, std::int64_t>> windowHandles{};
    std::vector<std::int32_t> hiddenLayers{};
};

/** Result of loading and calling a plug-in inside the bridge process. */
struct PluginInvocation
{
    bool loaded{ false };
    bool result{ false };
    std::string error{};
};

using PluginInvoker = std::function<PluginInvocation(const std::string& pluginPath, jed::IJED* jed)>;

/** Blocking byte stream used by the bridge process (stdin/stdout in practice). */
class BlockingChannel
{
public:
    virtual bool write(std::span<const std::byte> bytes) = 0;
    virtual bool readExact(std::span<std::byte> bytes) = 0;

protected:
    ~BlockingChannel() = default;
};

/**
 * Bridge-process entry point: reads one Run request, invokes the plug-in, and
 * reports Done. Returns 0 when the session completed (even if the plug-in
 * failed) and non-zero when the protocol or channel failed.
 */
int runBridge(BlockingChannel& channel, const PluginInvoker& invoke);

/** Editor services used while serving one bridged plug-in run. */
struct HostServices
{
    std::function<void(JedMessageType, const std::string&)> message;
    std::function<void(const std::string& label, Level&& level, editor::Selection&& selection)> commit;
    std::function<void(editor::Selection&& selection)> selectionChanged;
    std::function<void(const JedCursor&)> setCursor;
    std::function<void(std::int32_t)> setMapMode;
    std::function<std::optional<std::vector<std::uint8_t>>(const std::string&)> readGameFile;
    std::function<std::optional<std::vector<CogValue>>(const std::string&)> cogSymbols;
};

/**
 * Editor side of one bridged run. Feed it bridge output in any chunking; it
 * applies messages through HostServices and returns the reply bytes to send.
 */
class HostSession
{
public:
    explicit HostSession(HostServices services);
    ~HostSession();

    HostSession(const HostSession&) = delete;
    HostSession& operator=(const HostSession&) = delete;

    /** Encodes the Run request; send these bytes first. */
    std::vector<std::byte> start(const RunRequest& request);

    /** Consumes bridge output and returns replies. Ignores input after finish or failure. */
    std::vector<std::byte> receive(std::span<const std::byte> bytes);

    bool finished() const noexcept;
    bool failed() const noexcept;
    /** Valid when finished(): the bridge loaded and called the plug-in. */
    bool pluginLoaded() const noexcept;
    /** Valid when finished(): the plug-in's own return value. */
    bool pluginResult() const noexcept;
    /** Protocol failure or the bridge's load error. */
    const std::string& error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/** Frame encoder shared by both sides and by tests. */
std::vector<std::byte> encodeFrame(MessageKind kind, std::span<const std::byte> payload);

} // namespace sed::plugins::bridge
