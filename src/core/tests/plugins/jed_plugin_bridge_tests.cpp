#include "test_common.hpp"

#include "sed/plugins/jed_plugin_bridge.hpp"
#include "sed/plugins/plugin_binary.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace sed::tests
{
namespace
{

using namespace sed::plugins;
using namespace sed::plugins::bridge;
using namespace sed::plugins::jed;

char* mutableText(std::string& text)
{
    return text.data();
}

/** Host-side recorder; every service call appends one event in order. */
struct Recorder
{
    std::vector<std::string> events{};
    std::vector<std::pair<std::string, Level>> commits{};
    std::vector<editor::Selection> committedSelections{};
    std::vector<editor::Selection> selections{};
    std::vector<JedCursor> cursors{};
    std::vector<std::pair<JedMessageType, std::string>> messages{};

    HostServices services()
    {
        HostServices s{};
        s.message = [this](const JedMessageType type, const std::string& text)
        {
            events.push_back("message");
            messages.emplace_back(type, text);
        };
        s.commit = [this](const std::string& label, Level&& level, editor::Selection&& selection)
        {
            events.push_back("commit:" + label);
            commits.emplace_back(label, std::move(level));
            committedSelections.push_back(std::move(selection));
        };
        s.selectionChanged = [this](editor::Selection&& selection)
        {
            events.push_back("selection");
            selections.push_back(std::move(selection));
        };
        s.setCursor = [this](const JedCursor& cursor)
        {
            events.push_back("cursor");
            cursors.push_back(cursor);
        };
        s.setMapMode = [this](const std::int32_t mode)
        {
            events.push_back("mapmode:" + std::to_string(mode));
        };
        s.readGameFile = [this](const std::string& name) -> std::optional<std::vector<std::uint8_t>>
        {
            events.push_back("read:" + name);
            if ( name == "data.bin" )
            {
                return std::vector<std::uint8_t>{ 'A', 'B', 'C' };
            }
            return std::nullopt;
        };
        s.cogSymbols = [this](const std::string& name) -> std::optional<std::vector<CogValue>>
        {
            events.push_back("cog:" + name);
            CogValue symbol{};
            symbol.name = "speed";
            symbol.cogType = CogType::int_;
            symbol.type = ValueType::integer;
            symbol.intValue = 5;
            symbol.description = "door speed";
            return std::vector<CogValue>{ symbol };
        };
        return s;
    }
};

/**
 * Bridge-side channel that pumps the host session synchronously: every write is
 * fed to the session (optionally in small chunks) and its replies become the
 * bridge's next reads.
 */
class LoopbackChannel final : public BlockingChannel
{
public:
    LoopbackChannel(HostSession& session, std::vector<std::byte> initial, const std::size_t chunk)
        : session_(session), inbound_(std::move(initial)), chunk_(chunk)
    {
    }

    bool write(const std::span<const std::byte> bytes) override
    {
        for ( std::size_t offset = 0; offset < bytes.size(); offset += chunk_ )
        {
            const std::vector<std::byte> replies =
                session_.receive(bytes.subspan(offset, std::min(chunk_, bytes.size() - offset)));
            inbound_.insert(inbound_.end(), replies.begin(), replies.end());
        }
        return true;
    }

    bool readExact(const std::span<std::byte> bytes) override
    {
        if ( inbound_.size() - readPos_ < bytes.size() )
        {
            return false;
        }
        std::memcpy(bytes.data(), inbound_.data() + readPos_, bytes.size());
        readPos_ += bytes.size();
        return true;
    }

private:
    HostSession& session_;
    std::vector<std::byte> inbound_;
    std::size_t readPos_{};
    std::size_t chunk_{};
};

RunRequest cubeRequest()
{
    RunRequest request{};
    request.pluginPath = "C:/plugins/Test.dll";
    request.level.sectors.push_back(makeLitTestCubeSector(0.0, ColorF(1.0f, 1.0f, 1.0f)));
    request.level.sectors[0].colormap = "dflt.cmp";
    request.selection.sectors.insert(0);
    request.cursor.sector = 0;
    request.mapMode = MM_SC;
    request.jedStrings = { { js_ProjectDir, "C:/project" }, { js_GameDir, "C:/game" } };
    request.windowHandles = { { jw_Main, 0x1234 } };
    request.hiddenLayers = { 2 };
    return request;
}

struct RunOutcome
{
    int exitCode{ -1 };
    bool invoked{ false };
    std::string invokedPath{};
};

RunOutcome runLoopback(HostSession& session, const RunRequest& request, const std::size_t chunk,
    const std::function<bool(IJED*)>& plugin)
{
    RunOutcome outcome{};
    LoopbackChannel channel(session, session.start(request), chunk);
    outcome.exitCode = runBridge(channel, [&](const std::string& path, IJED* jed)
        {
            outcome.invoked = true;
            outcome.invokedPath = path;
            PluginInvocation invocation{};
            invocation.loaded = true;
            invocation.result = plugin(jed);
            return invocation;
        });
    return outcome;
}

void testSnapshotQueriesAndCommitOrder(const std::size_t chunk)
{
    Recorder recorder{};
    HostSession session(recorder.services());
    const RunRequest request = cubeRequest();
    std::vector<std::string> observed{};
    const RunOutcome outcome = runLoopback(session, request, chunk, [&](IJED* jed)
        {
            IJEDLevel* level = jed->GetLevel();
            check(level->NSectors() == 1 && level->SectorNSurfaces(0) == 6,
                "bridge: the plug-in sees the transported level");
            check(std::string(jed->GetJEDString(js_ProjectDir)) == "C:/project"
                    && std::string(jed->GetJEDString(js_GameDir)) == "C:/game"
                    && std::string(jed->GetJEDString(js_LevelFile)).empty(),
                "bridge: host strings are answered locally, unknown ones are empty");
            check(jed->GetJedWindow(jw_Main) == 0x1234 && jed->GetJedWindow(jw_3DPreview) == 0,
                "bridge: window handles are forwarded so plug-in dialogs can be owned by SED");
            check(!jed->IsLayerVisible(2) && jed->IsLayerVisible(0), "bridge: hidden layers are forwarded");
            check(jed->GetCurSC() == 0 && jed->GetMapMode() == MM_SC && jed->GetNMultiselected(MM_SC) == 1,
                "bridge: cursor, map mode, and selection are forwarded");

            std::string file = "data.bin";
            const int handle = jed->OpenGameFile(mutableText(file));
            char buffer[4]{};
            check(handle >= 0 && jed->ReadFile(handle, buffer, 4) == 3 && std::memcmp(buffer, "ABC", 3) == 0,
                "bridge: game files are read through the editor");
            std::string missing = "missing.bin";
            check(jed->OpenGameFile(mutableText(missing)) == -1, "bridge: a missing game file stays missing");

            std::string script = "door.cog";
            check(level->AddCOG(mutableText(script)) == 0 && level->NCOGValues(0) == 1,
                "bridge: COG symbols are resolved through the editor");

            std::string label = "Make Cube";
            jed->StartUndo(mutableText(label));
            JedVector normal{ 0.0, 0.0, 1.0 };
            JedVector edge{ 1.0, 0.0, 0.0 };
            check(jed->CreateCubicSector(4.0, 0.0, 0.0, &normal, &edge) == 1, "bridge: geometry commands run locally");
            jed->SelectSC(1);
            jed->SetCurSC(1);
            std::string hello = "hello from 32-bit";
            jed->PanMessage(0, mutableText(hello));
            observed.push_back("plugin-returned");
            return true;
        });

    check(outcome.exitCode == 0 && outcome.invoked && outcome.invokedPath == request.pluginPath,
        "bridge: the requested plug-in path is invoked and the session exits cleanly");
    check(session.finished() && !session.failed() && session.pluginLoaded() && session.pluginResult(),
        "bridge: the host sees a successful Done");

    const std::vector<std::string> expected{
        "read:data.bin",
        "read:missing.bin",
        "cog:door.cog",
        "commit:Plugin Edit",
        "message",
        "commit:Plugin: Make Cube",
        "cursor",
    };
    check(recorder.events == expected,
        "bridge: StartUndo commits the earlier edit; return commits the rest, then applies the deferred cursor");
    if ( recorder.commits.size() == 2 )
    {
        const Level& first = recorder.commits[0].second;
        check(first.sectors.size() == 1 && first.cogs.size() == 1 && first.cogs[0].values.size() == 1
                && first.cogs[0].values[0].name == "speed" && first.cogs[0].values[0].intValue == 5,
            "bridge: the first commit carries the COG added before StartUndo");
        check(first.sectors[0].colormap == "dflt.cmp", "bridge: untouched sector data survives the round trip");
        const Level& second = recorder.commits[1].second;
        check(second.sectors.size() == 2, "bridge: the second commit carries the new sector");
        check(recorder.committedSelections[1].sectors.count(1) == 1,
            "bridge: the commit carries the plug-in's selection");
    }
    check(recorder.cursors.size() == 1 && recorder.cursors[0].sector == 1, "bridge: the cursor names the new sector");
    check(recorder.messages.size() == 1 && recorder.messages[0].second == "hello from 32-bit"
            && recorder.messages[0].first == JedMessageType::Info,
        "bridge: plug-in messages reach the editor");
}

void testReadOnlyRunCommitsNothing()
{
    Recorder recorder{};
    HostSession session(recorder.services());
    const RunOutcome outcome = runLoopback(session, cubeRequest(), 4096, [](IJED* jed)
        {
            return jed->GetLevel()->NSectors() == 1;
        });
    check(outcome.exitCode == 0 && session.finished() && session.pluginResult() && recorder.events.empty(),
        "bridge: a read-only plug-in produces no commits or selection changes");
}

void testSelectionOnlyAndMapModeFlush()
{
    Recorder recorder{};
    HostSession session(recorder.services());
    runLoopback(session, cubeRequest(), 4096, [](IJED* jed)
        {
            jed->ClearMultiselection(MM_SC);
            JedVector normal{ 0.0, 0.0, 1.0 };
            JedVector edge{ 1.0, 0.0, 0.0 };
            jed->CreateCubicSector(4.0, 0.0, 0.0, &normal, &edge);
            jed->SetMapMode(MM_TH);
            jed->SetCurSC(0);
            return false;
        });
    const std::vector<std::string> expected{ "commit:Plugin Edit", "mapmode:3", "cursor" };
    check(recorder.events == expected, "bridge: SetMapMode commits the pending edit before switching modes");
    check(session.finished() && session.pluginLoaded() && !session.pluginResult(),
        "bridge: a plug-in that returns false is reported as loaded but failed");

    Recorder selectionOnly{};
    HostSession selectionSession(selectionOnly.services());
    runLoopback(selectionSession, cubeRequest(), 4096, [](IJED* jed)
        {
            jed->ClearMultiselection(MM_SC);
            return true;
        });
    check(selectionOnly.events == std::vector<std::string>{ "selection" } && selectionOnly.selections.size() == 1
            && selectionOnly.selections[0].sectors.empty(),
        "bridge: a selection-only change is published without an undo step");
}

void testLoadFailureIsReported()
{
    Recorder recorder{};
    HostSession session(recorder.services());
    LoopbackChannel channel(session, session.start(cubeRequest()), 4096);
    const int exitCode = runBridge(channel, [](const std::string&, IJED*)
        {
            PluginInvocation invocation{};
            invocation.error = "The DLL Test.dll is not a SED or JED plug-in: No load function found!";
            return invocation;
        });
    check(exitCode == 0 && session.finished() && !session.failed() && !session.pluginLoaded()
            && session.error().find("No load function found") != std::string::npos,
        "bridge: a load failure reaches the editor as an error, not a protocol failure");
}

std::vector<std::byte> header(const std::uint32_t magic, const std::uint32_t kind, const std::uint32_t length)
{
    std::vector<std::byte> out(kFrameHeaderBytes);
    const std::uint32_t words[3]{ magic, kind, length };
    for ( std::size_t w = 0; w < 3; ++w )
    {
        for ( std::size_t b = 0; b < 4; ++b )
        {
            out[w * 4 + b] = static_cast<std::byte>((words[w] >> (8 * b)) & 0xFFu);
        }
    }
    return out;
}

void testMalformedHostInputFailsAtomically()
{
    const auto failsWith = [](const std::vector<std::byte>& input, const char* message)
    {
        Recorder recorder{};
        HostSession session(recorder.services());
        session.start(cubeRequest());
        const std::vector<std::byte> replies = session.receive(input);
        check(session.failed() && !session.finished() && replies.empty() && recorder.events.empty()
                && !session.error().empty(),
            message);
        const std::vector<std::byte> done = encodeFrame(MessageKind::Done, std::vector<std::byte>(3));
        session.receive(done);
        check(!session.finished(), "bridge: input after a protocol failure is ignored");
    };
    failsWith(header(0x12345678u, 2, 0), "bridge: a bad frame magic fails the session");
    failsWith(header(kFrameMagic, 99, 0), "bridge: an unknown message kind fails the session");
    failsWith(header(kFrameMagic, static_cast<std::uint32_t>(MessageKind::Commit), 0xFFFFFFF0u),
        "bridge: an oversize frame fails before allocation");
    failsWith(header(kFrameMagic, static_cast<std::uint32_t>(MessageKind::Run), 0),
        "bridge: a bridge-to-host frame of the wrong direction fails the session");

    std::vector<std::byte> truncatedCommit = encodeFrame(MessageKind::Commit, std::vector<std::byte>(5, std::byte{ 0x7F }));
    failsWith(truncatedCommit, "bridge: a malformed Commit payload publishes nothing");

    Recorder partial{};
    HostSession partialSession(partial.services());
    partialSession.start(cubeRequest());
    const std::vector<std::byte> half = header(kFrameMagic, static_cast<std::uint32_t>(MessageKind::Done), 6);
    partialSession.receive(half);
    check(!partialSession.failed() && !partialSession.finished(), "bridge: a partial frame waits for more bytes");
}

class FixedChannel final : public BlockingChannel
{
public:
    explicit FixedChannel(std::vector<std::byte> input)
        : input_(std::move(input))
    {
    }

    bool write(const std::span<const std::byte> bytes) override
    {
        written.insert(written.end(), bytes.begin(), bytes.end());
        return true;
    }

    bool readExact(const std::span<std::byte> bytes) override
    {
        if ( input_.size() - pos_ < bytes.size() )
        {
            return false;
        }
        std::memcpy(bytes.data(), input_.data() + pos_, bytes.size());
        pos_ += bytes.size();
        return true;
    }

    std::vector<std::byte> written{};

private:
    std::vector<std::byte> input_;
    std::size_t pos_{};
};

void testBridgeRejectsBadRequests()
{
    bool invoked = false;
    const PluginInvoker invoker = [&](const std::string&, IJED*)
    {
        invoked = true;
        return PluginInvocation{ true, true, {} };
    };

    FixedChannel empty({});
    check(runBridge(empty, invoker) != 0 && !invoked, "bridge: a closed channel exits without invoking");

    FixedChannel garbage(header(0xDEADBEEFu, 1, 0));
    check(runBridge(garbage, invoker) != 0 && !invoked, "bridge: a bad magic exits without invoking");

    FixedChannel wrongVersion(encodeFrame(MessageKind::Run, std::vector<std::byte>{
        std::byte{ 0x09 }, std::byte{ 0 }, std::byte{ 0 }, std::byte{ 0 } }));
    check(runBridge(wrongVersion, invoker) != 0 && !invoked, "bridge: an unsupported protocol version exits");

    Recorder recorder{};
    HostSession session(recorder.services());
    std::vector<std::byte> run = session.start(cubeRequest());
    run.resize(run.size() - 7);
    FixedChannel truncated(run);
    check(runBridge(truncated, invoker) != 0 && !invoked, "bridge: a truncated Run request exits");
}

void testBridgeExecutableNames()
{
    check(pluginBridgeExecutableName(PluginBinaryArchitecture::x86, true) == "sed-jed-plugin-bridge-x86.exe"
            && pluginBridgeExecutableName(PluginBinaryArchitecture::x64, true) == "sed-jed-plugin-bridge-x64.exe"
            && pluginBridgeExecutableName(PluginBinaryArchitecture::arm64, false) == "sed-jed-plugin-bridge-arm64",
        "bridge: each architecture has its own bridge executable name");
    check(pluginBridgeExecutableName(PluginBinaryArchitecture::unknown, true).empty(),
        "bridge: an unknown architecture has no bridge");
}

} // namespace

void runJedPluginBridgeTests()
{
    testBridgeExecutableNames();
    testSnapshotQueriesAndCommitOrder(4096);
    testSnapshotQueriesAndCommitOrder(1);
    testReadOnlyRunCommitsNothing();
    testSelectionOnlyAndMapModeFlush();
    testLoadFailureIsReported();
    testMalformedHostInputFailsAtomically();
    testBridgeRejectsBadRequests();
}

} // namespace sed::tests
