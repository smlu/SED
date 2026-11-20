#include "sed/plugins/jed_plugin_bridge.hpp"

#include "sed/assets/sed.hpp"

#include <algorithm>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace sed::plugins::bridge
{
namespace
{

constexpr std::uint64_t kMaximumHostFileBytes = 256ull * 1024ull * 1024ull;

bool hostAccepts(const std::uint32_t kind) noexcept
{
    switch ( static_cast<MessageKind>(kind) )
    {
        case MessageKind::Done:
        case MessageKind::Message:
        case MessageKind::Commit:
        case MessageKind::SelectionChanged:
        case MessageKind::SetCursor:
        case MessageKind::SetMapMode:
        case MessageKind::ReadGameFile:
        case MessageKind::CogSymbols:
            return true;
        default:
            return false;
    }
}

bool bridgeAccepts(const std::uint32_t kind) noexcept
{
    switch ( static_cast<MessageKind>(kind) )
    {
        case MessageKind::Run:
        case MessageKind::ReadGameFileReply:
        case MessageKind::CogSymbolsReply:
            return true;
        default:
            return false;
    }
}

std::uint32_t readU32At(const std::byte* data) noexcept
{
    std::uint32_t value = 0;
    for ( std::size_t i = 0; i < 4; ++i )
    {
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(data[i])) << (8u * i);
    }
    return value;
}

class Writer
{
public:
    void u8(const std::uint8_t value)
    {
        out.push_back(static_cast<std::byte>(value));
    }

    void u32(const std::uint32_t value)
    {
        for ( std::size_t i = 0; i < 4; ++i )
        {
            out.push_back(static_cast<std::byte>((value >> (8u * i)) & 0xFFu));
        }
    }

    void u64(const std::uint64_t value)
    {
        for ( std::size_t i = 0; i < 8; ++i )
        {
            out.push_back(static_cast<std::byte>((value >> (8u * i)) & 0xFFu));
        }
    }

    void i32(const std::int32_t value)
    {
        u32(static_cast<std::uint32_t>(value));
    }

    void i64(const std::int64_t value)
    {
        u64(static_cast<std::uint64_t>(value));
    }

    void f64(const double value)
    {
        std::uint64_t bits = 0;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        u64(bits);
    }

    void count(const std::size_t value)
    {
        if ( value > std::numeric_limits<std::uint32_t>::max() )
        {
            throw std::length_error("bridge payload count is too large");
        }
        u32(static_cast<std::uint32_t>(value));
    }

    void string(const std::string& value)
    {
        count(value.size());
        const auto* data = reinterpret_cast<const std::byte*>(value.data());
        out.insert(out.end(), data, data + value.size());
    }

    void bytes(const std::span<const std::byte> value)
    {
        count(value.size());
        out.insert(out.end(), value.begin(), value.end());
    }

    std::vector<std::byte> out{};
};

/** Bounds-checked payload reader; any overrun latches failure and returns zeros. */
class Reader
{
public:
    explicit Reader(const std::span<const std::byte> data)
        : data_(data)
    {
    }

    bool ok() const noexcept
    {
        return ok_;
    }

    bool atEnd() const noexcept
    {
        return ok_ && pos_ == data_.size();
    }

    std::uint8_t u8()
    {
        if ( !take(1) )
        {
            return 0;
        }
        return std::to_integer<std::uint8_t>(data_[pos_ - 1]);
    }

    std::uint32_t u32()
    {
        if ( !take(4) )
        {
            return 0;
        }
        return readU32At(data_.data() + pos_ - 4);
    }

    std::uint64_t u64()
    {
        const std::uint64_t low = u32();
        const std::uint64_t high = u32();
        return low | (high << 32u);
    }

    std::int32_t i32()
    {
        return static_cast<std::int32_t>(u32());
    }

    std::int64_t i64()
    {
        return static_cast<std::int64_t>(u64());
    }

    double f64()
    {
        const std::uint64_t bits = u64();
        double value = 0.0;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    void reject() noexcept { ok_ = false; }

    bool flag()
    {
        const std::uint8_t value = u8();
        if ( value > 1u )
        {
            ok_ = false;
        }
        return value == 1u;
    }

    /** Reads an element count whose elements need at least `minimumElementBytes` each. */
    std::size_t count(const std::size_t minimumElementBytes)
    {
        const std::uint32_t value = u32();
        if ( !ok_ || (minimumElementBytes > 0 && value > (data_.size() - pos_) / minimumElementBytes) )
        {
            ok_ = false;
            return 0;
        }
        return value;
    }

    std::string string()
    {
        const std::size_t length = count(1);
        if ( !take(length) )
        {
            return {};
        }
        return std::string(reinterpret_cast<const char*>(data_.data() + pos_ - length), length);
    }

    std::span<const std::byte> bytes()
    {
        const std::size_t length = count(1);
        if ( !take(length) )
        {
            return {};
        }
        return data_.subspan(pos_ - length, length);
    }

private:
    bool take(const std::size_t size)
    {
        if ( !ok_ || size > data_.size() - pos_ )
        {
            ok_ = false;
            return false;
        }
        pos_ += size;
        return true;
    }

    std::span<const std::byte> data_;
    std::size_t pos_{};
    bool ok_{ true };
};

void writeIndexSet(Writer& w, const std::unordered_set<std::int32_t>& values)
{
    std::vector<std::int32_t> sorted(values.begin(), values.end());
    std::sort(sorted.begin(), sorted.end());
    w.count(sorted.size());
    for ( const std::int32_t value : sorted )
    {
        w.i32(value);
    }
}

std::unordered_set<std::int32_t> readIndexSet(Reader& r)
{
    std::unordered_set<std::int32_t> values;
    const std::size_t n = r.count(4);
    for ( std::size_t i = 0; i < n; ++i )
    {
        values.insert(r.i32());
    }
    return values;
}

void writeSelection(Writer& w, const editor::Selection& selection)
{
    writeIndexSet(w, selection.sectors);

    std::vector<std::pair<std::int32_t, std::int32_t>> surfaces;
    for ( const SurfaceRef& ref : selection.surfaces )
    {
        surfaces.emplace_back(ref.sector, ref.surface);
    }
    std::sort(surfaces.begin(), surfaces.end());
    w.count(surfaces.size());
    for ( const auto& [sector, surface] : surfaces )
    {
        w.i32(sector);
        w.i32(surface);
    }

    std::vector<std::tuple<std::int32_t, std::int32_t, std::int32_t>> vertices;
    for ( const editor::SurfaceVertexRef& ref : selection.surfaceVertices )
    {
        vertices.emplace_back(ref.surface.sector, ref.surface.surface, ref.localVertex);
    }
    std::sort(vertices.begin(), vertices.end());
    w.count(vertices.size());
    for ( const auto& [sector, surface, vertex] : vertices )
    {
        w.i32(sector);
        w.i32(surface);
        w.i32(vertex);
    }

    writeIndexSet(w, selection.things);

    std::vector<std::pair<std::int32_t, std::int32_t>> frames;
    for ( const level::FrameRef& ref : selection.frames )
    {
        frames.emplace_back(ref.thing, ref.value);
    }
    std::sort(frames.begin(), frames.end());
    w.count(frames.size());
    for ( const auto& [thing, value] : frames )
    {
        w.i32(thing);
        w.i32(value);
    }

    writeIndexSet(w, selection.lights);
    writeIndexSet(w, selection.cogs);
}

editor::Selection readSelection(Reader& r)
{
    editor::Selection selection{};
    selection.sectors = readIndexSet(r);
    const std::size_t surfaces = r.count(8);
    for ( std::size_t i = 0; i < surfaces; ++i )
    {
        SurfaceRef ref{};
        ref.sector = r.i32();
        ref.surface = r.i32();
        selection.surfaces.insert(ref);
    }
    const std::size_t vertices = r.count(12);
    for ( std::size_t i = 0; i < vertices; ++i )
    {
        editor::SurfaceVertexRef ref{};
        ref.surface.sector = r.i32();
        ref.surface.surface = r.i32();
        ref.localVertex = r.i32();
        selection.surfaceVertices.insert(ref);
    }
    selection.things = readIndexSet(r);
    const std::size_t frames = r.count(8);
    for ( std::size_t i = 0; i < frames; ++i )
    {
        level::FrameRef ref{};
        ref.thing = r.i32();
        ref.value = r.i32();
        selection.frames.insert(ref);
    }
    selection.lights = readIndexSet(r);
    selection.cogs = readIndexSet(r);
    return selection;
}

void writeCursor(Writer& w, const JedCursor& cursor)
{
    w.i32(cursor.sector);
    w.i32(cursor.surface);
    w.i32(cursor.edge);
    w.i32(cursor.vertex);
    w.i32(cursor.thing);
    w.i32(cursor.frame);
    w.i32(cursor.light);
}

JedCursor readCursor(Reader& r)
{
    JedCursor cursor{};
    cursor.sector = r.i32();
    cursor.surface = r.i32();
    cursor.edge = r.i32();
    cursor.vertex = r.i32();
    cursor.thing = r.i32();
    cursor.frame = r.i32();
    cursor.light = r.i32();
    return cursor;
}

void writeLevel(Writer& w, const Level& level)
{
    const std::vector<std::byte> bytes = assets::saveToSedBytes(level);
    w.bytes(bytes);
}

std::optional<Level> readLevel(Reader& r)
{
    const std::span<const std::byte> bytes = r.bytes();
    if ( !r.ok() )
    {
        return std::nullopt;
    }
    try
    {
        return assets::loadFromSedBytes(bytes);
    }
    catch ( const std::exception& )
    {
        return std::nullopt;
    }
}

void writeCogValue(Writer& w, const CogValue& value)
{
    w.string(value.name);
    w.i32(static_cast<std::int32_t>(value.type));
    w.i32(value.intValue);
    w.f64(value.floatValue);
    w.string(value.stringValue);
    w.i32(static_cast<std::int32_t>(value.cogType));
    w.u8(value.local ? 1u : 0u);
    w.string(value.description);
    w.i32(value.mask);
    // Protocol v1 reserves optional link-ID/mask metadata that is absent
    // from SED's CogValue. Both applications still agree on the frame layout.
    w.u8(0); w.i32(0);
    w.u8(0); w.u32(0);
    w.u8(0);
}

CogValue readCogValue(Reader& r)
{
    CogValue value{};
    value.name = r.string();
    const std::int32_t type = r.i32();
    if ( type < 0 || type > static_cast<std::int32_t>(ValueType::unknown) )
    {
        r.reject();
        return {};
    }
    value.type = static_cast<ValueType>(type);
    value.intValue = r.i32();
    value.floatValue = r.f64();
    value.stringValue = r.string();
    const std::int32_t cogType = r.i32();
    if ( cogType < 0 || cogType > static_cast<std::int32_t>(CogType::vect) )
    {
        r.reject();
        return {};
    }
    value.cogType = static_cast<CogType>(cogType);
    value.local = r.flag();
    value.description = r.string();
    value.mask = r.i32();
    const bool hasLink = r.flag();
    r.i32();
    const bool hasMask = r.flag();
    r.u32();
    const bool noLink = r.flag();
    if (hasLink || hasMask || noLink)
    {
        r.reject();
    }
    return value;
}

/** Smallest encoded CogValue: three empty strings plus fixed fields. */
constexpr std::size_t kMinimumCogValueBytes = 3 * 4 + 4 + 4 + 8 + 4 + 1 + 4 + 1 + 4 + 1 + 4 + 1;

struct Frame
{
    MessageKind kind{};
    std::vector<std::byte> payload{};
};

std::optional<Frame> readFrame(BlockingChannel& channel)
{
    std::byte header[kFrameHeaderBytes]{};
    if ( !channel.readExact(header) )
    {
        return std::nullopt;
    }
    const std::uint32_t magic = readU32At(header);
    const std::uint32_t kind = readU32At(header + 4);
    const std::uint32_t length = readU32At(header + 8);
    if ( magic != kFrameMagic || !bridgeAccepts(kind) || length > kMaximumPayloadBytes )
    {
        return std::nullopt;
    }
    Frame frame{ static_cast<MessageKind>(kind), std::vector<std::byte>(length) };
    if ( length > 0 && !channel.readExact(frame.payload) )
    {
        return std::nullopt;
    }
    return frame;
}

std::optional<RunRequest> decodeRun(const std::span<const std::byte> payload)
{
    Reader r(payload);
    if ( r.u32() != kProtocolVersion || !r.ok() )
    {
        return std::nullopt;
    }
    RunRequest request{};
    request.pluginPath = r.string();
    std::optional<Level> level = readLevel(r);
    if ( !level )
    {
        return std::nullopt;
    }
    request.level = std::move(*level);
    request.selection = readSelection(r);
    request.cursor = readCursor(r);
    request.mapMode = r.i32();
    const std::size_t strings = r.count(8);
    for ( std::size_t i = 0; i < strings; ++i )
    {
        const std::int32_t what = r.i32();
        request.jedStrings.emplace_back(what, r.string());
    }
    const std::size_t windows = r.count(12);
    for ( std::size_t i = 0; i < windows; ++i )
    {
        const std::int32_t which = r.i32();
        request.windowHandles.emplace_back(which, r.i64());
    }
    const std::size_t hidden = r.count(4);
    for ( std::size_t i = 0; i < hidden; ++i )
    {
        request.hiddenLayers.push_back(r.i32());
    }
    if ( !r.atEnd() )
    {
        return std::nullopt;
    }
    return request;
}

/** JedHostContext inside the bridge process: a local document plus editor round trips. */
class BridgeContext final : public JedHostContext
{
public:
    BridgeContext(BlockingChannel& channel, RunRequest&& request)
        : channel_(channel),
          level_(std::move(request.level)),
          selection_(std::move(request.selection)),
          cursor_(request.cursor),
          mapMode_(request.mapMode),
          strings_(request.jedStrings.begin(), request.jedStrings.end()),
          windows_(request.windowHandles.begin(), request.windowHandles.end()),
          hiddenLayers_(request.hiddenLayers.begin(), request.hiddenLayers.end())
    {
    }

    bool broken() const noexcept
    {
        return broken_;
    }

    /** Publishes the pending edit, selection, and deferred cursor (JedPluginManager::commitPendingEdit). */
    void flush()
    {
        if ( levelDirty_ )
        {
            Writer w;
            w.string(pendingLabel_);
            writeLevel(w, level_);
            writeSelection(w, selection_);
            send(MessageKind::Commit, w.out);
            levelDirty_ = false;
            selectionDirty_ = false;
        }
        else if ( selectionDirty_ )
        {
            Writer w;
            writeSelection(w, selection_);
            send(MessageKind::SelectionChanged, w.out);
            selectionDirty_ = false;
        }
        if ( pendingCursor_ )
        {
            Writer w;
            writeCursor(w, *pendingCursor_);
            pendingCursor_.reset();
            send(MessageKind::SetCursor, w.out);
        }
    }

    void send(const MessageKind kind, const std::span<const std::byte> payload)
    {
        if ( broken_ )
        {
            return;
        }
        const std::vector<std::byte> frame = encodeFrame(kind, payload);
        if ( !channel_.write(frame) )
        {
            broken_ = true;
        }
    }

    const Level& level() const override
    {
        return level_;
    }

    Level& mutableLevel() override
    {
        if ( !levelDirty_ )
        {
            levelDirty_ = true;
            pendingLabel_ = nextLabel_.empty() ? std::string("Plugin Edit") : nextLabel_;
            nextLabel_.clear();
        }
        return level_;
    }

    const editor::Selection& selection() const override
    {
        return selection_;
    }

    editor::Selection& mutableSelection() override
    {
        selectionDirty_ = true;
        return selection_;
    }

    void startUndo(const std::string& label) override
    {
        flush();
        nextLabel_ = label.empty() ? std::string{} : "Plugin: " + label;
    }

    void updateMap() override
    {
        flush();
    }

    void message(const JedMessageType type, const std::string& text) override
    {
        Writer w;
        w.i32(static_cast<std::int32_t>(type));
        w.string(text);
        send(MessageKind::Message, w.out);
    }

    std::int32_t mapMode() const override
    {
        return mapMode_;
    }

    void setMapMode(const std::int32_t mode) override
    {
        flush();
        mapMode_ = mode;
        Writer w;
        w.i32(mode);
        send(MessageKind::SetMapMode, w.out);
    }

    JedCursor cursor() const override
    {
        return cursor_;
    }

    void setCursor(const JedCursor& cursor) override
    {
        cursor_ = cursor;
        if ( levelDirty_ )
        {
            // The cursor may name items that only exist in the pending level.
            pendingCursor_ = cursor;
            return;
        }
        Writer w;
        writeCursor(w, cursor);
        send(MessageKind::SetCursor, w.out);
    }

    std::optional<std::vector<std::uint8_t>> readFile(const std::string& name, const bool gameFile) override
    {
        if ( !gameFile )
        {
            return readLocalFile(name);
        }
        Writer w;
        w.string(name);
        const std::optional<Frame> reply = request(MessageKind::ReadGameFile, w.out, MessageKind::ReadGameFileReply);
        if ( !reply )
        {
            return std::nullopt;
        }
        Reader r(reply->payload);
        const bool present = r.flag();
        const std::span<const std::byte> data = present ? r.bytes() : std::span<const std::byte>{};
        if ( !r.atEnd() )
        {
            broken_ = true;
            return std::nullopt;
        }
        if ( !present )
        {
            return std::nullopt;
        }
        std::vector<std::uint8_t> out(data.size());
        std::memcpy(out.data(), data.data(), data.size());
        return out;
    }

    std::optional<std::vector<CogValue>> cogScriptSymbols(const std::string& scriptName) override
    {
        Writer w;
        w.string(scriptName);
        const std::optional<Frame> reply = request(MessageKind::CogSymbols, w.out, MessageKind::CogSymbolsReply);
        if ( !reply )
        {
            return std::nullopt;
        }
        Reader r(reply->payload);
        const bool present = r.flag();
        std::vector<CogValue> values;
        if ( present )
        {
            const std::size_t n = r.count(kMinimumCogValueBytes);
            for ( std::size_t i = 0; i < n && r.ok(); ++i )
            {
                values.push_back(readCogValue(r));
            }
        }
        if ( !r.atEnd() )
        {
            broken_ = true;
            return std::nullopt;
        }
        return present ? std::optional<std::vector<CogValue>>(std::move(values)) : std::nullopt;
    }

    std::string jedString(const std::int32_t what) const override
    {
        const auto it = strings_.find(what);
        return it == strings_.end() ? std::string{} : it->second;
    }

    std::intptr_t windowHandle(const std::int32_t which) const override
    {
        const auto it = windows_.find(which);
        return it == windows_.end() ? 0 : static_cast<std::intptr_t>(it->second);
    }

    bool isLayerVisible(const std::int32_t layer) const override
    {
        return hiddenLayers_.count(layer) == 0;
    }

private:
    std::optional<Frame> request(const MessageKind kind, const std::span<const std::byte> payload,
        const MessageKind replyKind)
    {
        send(kind, payload);
        if ( broken_ )
        {
            return std::nullopt;
        }
        std::optional<Frame> reply = readFrame(channel_);
        if ( !reply || reply->kind != replyKind )
        {
            broken_ = true;
            return std::nullopt;
        }
        return reply;
    }

    static std::optional<std::vector<std::uint8_t>> readLocalFile(const std::string& name)
    {
        std::error_code error;
        const std::filesystem::path path(name);
        const std::uintmax_t size = std::filesystem::file_size(path, error);
        if ( error || size > kMaximumHostFileBytes )
        {
            return std::nullopt;
        }
        std::ifstream file(path, std::ios::binary);
        std::vector<std::uint8_t> out(static_cast<std::size_t>(size));
        if ( !file || (size > 0 && !file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size))) )
        {
            return std::nullopt;
        }
        return out;
    }

    BlockingChannel& channel_;
    Level level_;
    editor::Selection selection_;
    JedCursor cursor_;
    std::int32_t mapMode_;
    std::map<std::int32_t, std::string> strings_;
    std::map<std::int32_t, std::int64_t> windows_;
    std::set<std::int32_t> hiddenLayers_;
    bool levelDirty_{ false };
    bool selectionDirty_{ false };
    bool broken_{ false };
    std::string pendingLabel_{};
    std::string nextLabel_{};
    std::optional<JedCursor> pendingCursor_{};
};

} // namespace

std::vector<std::byte> encodeFrame(const MessageKind kind, const std::span<const std::byte> payload)
{
    if ( payload.size() > kMaximumPayloadBytes )
    {
        throw std::length_error("bridge payload exceeds the protocol limit");
    }
    Writer w;
    w.u32(kFrameMagic);
    w.u32(static_cast<std::uint32_t>(kind));
    w.u32(static_cast<std::uint32_t>(payload.size()));
    w.out.insert(w.out.end(), payload.begin(), payload.end());
    return std::move(w.out);
}

int runBridge(BlockingChannel& channel, const PluginInvoker& invoke)
{
    std::optional<Frame> frame = readFrame(channel);
    if ( !frame || frame->kind != MessageKind::Run )
    {
        return 2;
    }
    std::optional<RunRequest> request = decodeRun(frame->payload);
    frame.reset();
    if ( !request )
    {
        return 2;
    }
    const std::string pluginPath = request->pluginPath;
    BridgeContext context(channel, std::move(*request));
    PluginInvocation invocation{};
    {
        JedPluginHost host(context);
        try
        {
            invocation = invoke(pluginPath, host.jed());
        }
        catch ( const std::exception& error )
        {
            invocation = PluginInvocation{ true, false, std::string("Exception raised by plug-in: ") + error.what() };
        }
        catch ( ... )
        {
            invocation = PluginInvocation{ true, false, "Exception raised by plug-in: unknown exception" };
        }
        try
        {
            context.flush();
        }
        catch ( const std::exception& error )
        {
            invocation.result = false;
            invocation.error = std::string("Couldn't send the plug-in edit to SED: ") + error.what();
        }
    }
    Writer w;
    w.u8(invocation.loaded ? 1u : 0u);
    w.u8(invocation.result ? 1u : 0u);
    w.string(invocation.error);
    context.send(MessageKind::Done, w.out);
    return context.broken() ? 3 : 0;
}

struct HostSession::Impl
{
    HostServices services;
    std::vector<std::byte> buffer{};
    std::size_t consumed{};
    bool started{ false };
    bool finished{ false };
    bool failed{ false };
    bool loaded{ false };
    bool result{ false };
    std::string error{};

    void fail(std::string message)
    {
        failed = true;
        error = std::move(message);
        buffer.clear();
        consumed = 0;
    }

    /** Decodes and applies one complete frame; returns false on a protocol error. */
    bool handle(const MessageKind kind, const std::span<const std::byte> payload, Writer& replies)
    {
        Reader r(payload);
        switch ( kind )
        {
            case MessageKind::Done:
            {
                const bool pluginLoaded = r.flag();
                const bool pluginResult = r.flag();
                std::string text = r.string();
                if ( !r.atEnd() )
                {
                    return false;
                }
                loaded = pluginLoaded;
                result = pluginResult;
                error = std::move(text);
                finished = true;
                return true;
            }
            case MessageKind::Message:
            {
                const std::int32_t type = r.i32();
                std::string text = r.string();
                if ( !r.atEnd() || type < 0 || type > static_cast<std::int32_t>(JedMessageType::Error) )
                {
                    return false;
                }
                if ( services.message )
                {
                    services.message(static_cast<JedMessageType>(type), text);
                }
                return true;
            }
            case MessageKind::Commit:
            {
                std::string label = r.string();
                std::optional<Level> level = readLevel(r);
                if ( !level )
                {
                    return false;
                }
                editor::Selection selection = readSelection(r);
                if ( !r.atEnd() )
                {
                    return false;
                }
                if ( services.commit )
                {
                    services.commit(label, std::move(*level), std::move(selection));
                }
                return true;
            }
            case MessageKind::SelectionChanged:
            {
                editor::Selection selection = readSelection(r);
                if ( !r.atEnd() )
                {
                    return false;
                }
                if ( services.selectionChanged )
                {
                    services.selectionChanged(std::move(selection));
                }
                return true;
            }
            case MessageKind::SetCursor:
            {
                const JedCursor cursor = readCursor(r);
                if ( !r.atEnd() )
                {
                    return false;
                }
                if ( services.setCursor )
                {
                    services.setCursor(cursor);
                }
                return true;
            }
            case MessageKind::SetMapMode:
            {
                const std::int32_t mode = r.i32();
                if ( !r.atEnd() )
                {
                    return false;
                }
                if ( services.setMapMode )
                {
                    services.setMapMode(mode);
                }
                return true;
            }
            case MessageKind::ReadGameFile:
            {
                const std::string name = r.string();
                if ( !r.atEnd() )
                {
                    return false;
                }
                std::optional<std::vector<std::uint8_t>> data =
                    services.readGameFile ? services.readGameFile(name) : std::nullopt;
                if ( data && data->size() > kMaximumHostFileBytes )
                {
                    data.reset();
                }
                Writer w;
                w.u8(data ? 1u : 0u);
                if ( data )
                {
                    w.bytes(std::as_bytes(std::span<const std::uint8_t>(*data)));
                }
                const std::vector<std::byte> frame = encodeFrame(MessageKind::ReadGameFileReply, w.out);
                replies.out.insert(replies.out.end(), frame.begin(), frame.end());
                return true;
            }
            case MessageKind::CogSymbols:
            {
                const std::string name = r.string();
                if ( !r.atEnd() )
                {
                    return false;
                }
                const std::optional<std::vector<CogValue>> symbols =
                    services.cogSymbols ? services.cogSymbols(name) : std::nullopt;
                Writer w;
                w.u8(symbols ? 1u : 0u);
                if ( symbols )
                {
                    w.count(symbols->size());
                    for ( const CogValue& value : *symbols )
                    {
                        writeCogValue(w, value);
                    }
                }
                const std::vector<std::byte> frame = encodeFrame(MessageKind::CogSymbolsReply, w.out);
                replies.out.insert(replies.out.end(), frame.begin(), frame.end());
                return true;
            }
            default:
                return false;
        }
    }
};

HostSession::HostSession(HostServices services)
    : impl_(std::make_unique<Impl>())
{
    impl_->services = std::move(services);
}

HostSession::~HostSession() = default;

std::vector<std::byte> HostSession::start(const RunRequest& request)
{
    Writer w;
    w.u32(kProtocolVersion);
    w.string(request.pluginPath);
    writeLevel(w, request.level);
    writeSelection(w, request.selection);
    writeCursor(w, request.cursor);
    w.i32(request.mapMode);
    w.count(request.jedStrings.size());
    for ( const auto& [what, text] : request.jedStrings )
    {
        w.i32(what);
        w.string(text);
    }
    w.count(request.windowHandles.size());
    for ( const auto& [which, handle] : request.windowHandles )
    {
        w.i32(which);
        w.i64(handle);
    }
    w.count(request.hiddenLayers.size());
    for ( const std::int32_t layer : request.hiddenLayers )
    {
        w.i32(layer);
    }
    impl_->started = true;
    return encodeFrame(MessageKind::Run, w.out);
}

std::vector<std::byte> HostSession::receive(const std::span<const std::byte> bytes)
{
    Impl& s = *impl_;
    Writer replies;
    if ( s.failed || s.finished )
    {
        return {};
    }
    s.buffer.insert(s.buffer.end(), bytes.begin(), bytes.end());
    while ( !s.failed && !s.finished && s.buffer.size() - s.consumed >= kFrameHeaderBytes )
    {
        const std::byte* head = s.buffer.data() + s.consumed;
        const std::uint32_t magic = readU32At(head);
        const std::uint32_t kind = readU32At(head + 4);
        const std::uint32_t length = readU32At(head + 8);
        if ( magic != kFrameMagic )
        {
            s.fail("The plug-in bridge sent an invalid frame.");
            return {};
        }
        if ( !hostAccepts(kind) )
        {
            s.fail("The plug-in bridge sent an unexpected message (" + std::to_string(kind) + ").");
            return {};
        }
        if ( length > kMaximumPayloadBytes )
        {
            s.fail("The plug-in bridge sent an oversized message.");
            return {};
        }
        if ( s.buffer.size() - s.consumed - kFrameHeaderBytes < length )
        {
            break;
        }
        const std::span<const std::byte> payload(head + kFrameHeaderBytes, length);
        s.consumed += kFrameHeaderBytes + length;
        if ( !s.handle(static_cast<MessageKind>(kind), payload, replies) )
        {
            s.fail("The plug-in bridge sent a malformed message.");
            return {};
        }
    }
    if ( s.consumed > 0 && (s.consumed == s.buffer.size() || s.consumed >= 64u * 1024u) )
    {
        s.buffer.erase(s.buffer.begin(), s.buffer.begin() + static_cast<std::ptrdiff_t>(s.consumed));
        s.consumed = 0;
    }
    return std::move(replies.out);
}

bool HostSession::finished() const noexcept
{
    return impl_->finished;
}

bool HostSession::failed() const noexcept
{
    return impl_->failed;
}

bool HostSession::pluginLoaded() const noexcept
{
    return impl_->loaded;
}

bool HostSession::pluginResult() const noexcept
{
    return impl_->result;
}

const std::string& HostSession::error() const noexcept
{
    return impl_->error;
}

} // namespace sed::plugins::bridge
