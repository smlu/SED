#include "test_common.hpp"

#include "sed/plugins/jed_plugin_host.hpp"
#include "sed/plugins/plugin_binary.hpp"

#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace sed::tests
{
namespace
{

using namespace sed::plugins;
using namespace sed::plugins::jed;

class FakeContext final : public JedHostContext
{
public:
    Level levelState{};
    editor::Selection selectionState{};
    JedCursor cursorState{};
    std::int32_t mode{ MM_SC };
    std::vector<std::pair<JedMessageType, std::string>> messages{};
    std::vector<std::string> undoLabels{};
    std::map<std::string, std::vector<std::uint8_t>> files{};
    std::optional<std::vector<CogValue>> cogSymbols{};
    int mutableLevelCalls{};
    int updateCalls{};
    bool throwOnLevel{ false };

    const Level& level() const override
    {
        if ( throwOnLevel )
        {
            throw std::runtime_error("level unavailable");
        }
        return levelState;
    }

    Level& mutableLevel() override
    {
        ++mutableLevelCalls;
        return levelState;
    }

    const editor::Selection& selection() const override
    {
        return selectionState;
    }

    editor::Selection& mutableSelection() override
    {
        return selectionState;
    }

    void startUndo(const std::string& label) override
    {
        undoLabels.push_back(label);
    }

    void updateMap() override
    {
        ++updateCalls;
    }

    void message(const JedMessageType type, const std::string& text) override
    {
        messages.emplace_back(type, text);
    }

    std::int32_t mapMode() const override
    {
        return mode;
    }

    void setMapMode(const std::int32_t value) override
    {
        mode = value;
    }

    JedCursor cursor() const override
    {
        return cursorState;
    }

    void setCursor(const JedCursor& value) override
    {
        cursorState = value;
    }

    std::optional<std::vector<std::uint8_t>> readFile(const std::string& name, bool) override
    {
        const auto it = files.find(name);
        if ( it == files.end() )
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::optional<std::vector<CogValue>> cogScriptSymbols(const std::string&) override
    {
        return cogSymbols;
    }

    std::string jedString(const std::int32_t what) const override
    {
        return what == js_LevelFile ? std::string("C:\\levels\\test.jkl") : std::string();
    }

    std::intptr_t windowHandle(const std::int32_t which) const override
    {
        return which == jw_Main ? 0x1234 : 0;
    }

    bool isLayerVisible(const std::int32_t layer) const override
    {
        return layer != 3;
    }

    std::size_t warningCount() const
    {
        std::size_t count = 0;
        for ( const auto& message : messages )
        {
            count += message.first == JedMessageType::Warning ? 1 : 0;
        }
        return count;
    }
};

std::vector<std::uint8_t> bytes(const std::string& text)
{
    return { text.begin(), text.end() };
}

char* mutableText(std::string& text)
{
    return text.data();
}

void testIdentityAndLevelAccess()
{
    FakeContext context;
    JedPluginHost host(context);
    IJED* jed = host.jed();

    check(jed->GetVersion() == JedPluginHost::kReportedVersion, "JED host reports JED 0.951 compatibility");
    check(jed->GetLevel() == host.level(), "IJED::GetLevel returns the host level interface");
    void* object = reinterpret_cast<void*>(0x1);
    check(jed->QueryInterface(nullptr, &object) == kClassNotAvailable && object == nullptr,
        "QueryInterface reports no extra interfaces and clears the out pointer");
    check(jed->AddRef() >= 1 && host.level()->Release() == 0, "IJED objects are host-owned and not ref-count freed");
    check(context.mutableLevelCalls == 0, "identity queries do not open a level edit");
}

void testHeaderRoundTrip()
{
    FakeContext context;
    context.levelState.masterCMP = "dflt.cmp";
    JedPluginHost host(context);
    IJEDLevel* level = host.level();

    JedLevelHeader header{};
    check(level->GetLevelHeader(&header, lh_all) == kOk, "GetLevelHeader succeeds");
    check(std::string(header.masterCmp) == "dflt.cmp", "GetLevelHeader returns the master colormap");
    check(context.mutableLevelCalls == 0, "GetLevelHeader does not open a level edit");

    std::string cmp = "night.cmp";
    header.gravity = 2.5;
    header.lodDist[3] = 9.0;
    header.masterCmp = mutableText(cmp);
    check(level->SetLevelHeader(&header, lh_gravity | lh_LODDist | lh_MasterCMP) == kOk, "SetLevelHeader succeeds");
    check(close(context.levelState.header.gravity, 2.5), "SetLevelHeader writes gravity");
    check(close(context.levelState.header.lodDistances[3], 9.0), "SetLevelHeader writes LOD distances");
    check(context.levelState.masterCMP == "night.cmp", "SetLevelHeader writes the master colormap");
    check(level->GetLevelHeader(nullptr, lh_all) == kInvalidArgument && context.warningCount() == 1,
        "a null header pointer is rejected with a warning");
}

void testSectorAndSurfaceRecords()
{
    FakeContext context;
    context.levelState.sectors.push_back(makeLitTestCubeSector(0.0, ColorF(1.0f, 1.0f, 1.0f)));
    context.levelState.sectors[0].colormap = "dflt.cmp";
    context.levelState.sectors[0].ambient = ColorF(0.25f, 0.5f, 0.75f, 1.0f);
    JedPluginHost host(context);
    IJEDLevel* level = host.level();

    check(level->NSectors() == 1 && level->SectorNVertices(0) == 8 && level->SectorNSurfaces(0) == 6,
        "sector counts are exposed through IJEDLevel");

    JedSectorRec sector{};
    check(level->GetSector(0, &sector, s_all) == kOk, "GetSector succeeds");
    check(std::string(sector.colormap) == "dflt.cmp", "GetSector returns the colormap");
    check(context.mutableLevelCalls == 0, "GetSector does not open a level edit");

    const ColorF authoredAmbient = context.levelState.sectors[0].ambient;
    std::string sound = "wind.wav";
    sector.flags = 0x10;
    sector.sound = mutableText(sound);
    check(level->SetSector(0, &sector, s_all) == kOk, "SetSector succeeds");
    check(context.levelState.sectors[0].flags == 0x10 && context.levelState.sectors[0].sound == "wind.wav",
        "SetSector writes flags and sound");
    check(context.levelState.sectors[0].ambient == authoredAmbient,
        "an unchanged ambient intensity keeps the authored ambient color");

    sector.ambient = 0.0;
    check(level->SetSector(0, &sector, s_ambient) == kOk && context.levelState.sectors[0].ambient.intensity() == 0.0f,
        "a changed ambient intensity is written");

    JedSurfaceRec surface{};
    check(level->GetSurface(0, 2, &surface, sf_all) == kOk, "GetSurface succeeds");
    std::string material = "rock.mat";
    surface.material = mutableText(material);
    surface.surfFlags = 0x4;
    check(level->SetSurface(0, 2, &surface, sf_Material | sf_SurfFlags) == kOk, "SetSurface succeeds");
    check(context.levelState.sectors[0].surfaces[2].material == "rock.mat"
            && context.levelState.sectors[0].surfaces[2].surfaceFlags == 0x4,
        "SetSurface writes material and surface flags");

    JedVector normal{};
    check(level->GetSurfaceNormal(0, 2, &normal) == kOk, "GetSurfaceNormal succeeds");
    const TVector expected = context.levelState.sectors[0].surfaces[2].normal;
    check(close(normal.x, expected.x) && close(normal.y, expected.y) && close(normal.z, expected.z),
        "GetSurfaceNormal returns the model normal");

    float white = 0.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    check(level->SurfaceSetVertexLight(0, 2, 1, 0.5f, 0.25f, 0.5f, 0.75f) == kOk, "SurfaceSetVertexLight succeeds");
    check(level->SurfaceGetVertexLight(0, 2, 1, &white, &r, &g, &b) == kOk && close(r, 0.25) && close(b, 0.75),
        "vertex light round-trips");

    context.levelState.sectors[0].surfaces[3].texVertices.clear();
    check(level->SurfaceSetVertexLight(0, 3, 0, 1.0f, 1.0f, 1.0f, 1.0f) == kInvalidArgument,
        "a surface without per-vertex texture data is rejected instead of indexed out of range");
}

void testInvalidIndicesAreRejected()
{
    FakeContext context;
    JedPluginHost host(context);
    IJEDLevel* level = host.level();

    JedSectorRec sector{};
    check(level->GetSector(4, &sector, s_all) == kInvalidArgument, "GetSector rejects an invalid sector");
    check(level->DeleteThing(0) == kInvalidArgument, "DeleteThing rejects an invalid thing");
    check(level->SectorNVertices(-1) == -1, "SectorNVertices rejects a negative sector");
    check(level->GetCOGValue(0, 0) == nullptr, "GetCOGValue rejects an invalid COG");
    check(context.warningCount() == 4, "each rejected call reports one warning");
    check(context.mutableLevelCalls == 0, "rejected calls never open a level edit");
}

void testThingsAndLights()
{
    FakeContext context;
    JedPluginHost host(context);
    IJEDLevel* level = host.level();

    const int th = level->AddThing();
    check(th == 0 && level->NThings() == 1, "AddThing appends a thing");
    std::string name = "health";
    JedThingRec thing{};
    thing.name = mutableText(name);
    thing.sector = -1;
    thing.x = 1.0;
    thing.yaw = 90.0;
    check(level->SetThing(th, &thing, th_all) == kOk, "SetThing succeeds");
    JedThingRec read{};
    check(level->GetThing(th, &read, th_all) == kOk && std::string(read.name) == "health" && close(read.yaw, 90.0),
        "SetThing and GetThing round-trip the template and orientation");
    thing.sector = 7;
    check(level->SetThing(th, &thing, th_sector) == kInvalidArgument, "SetThing rejects an invalid sector");

    const int lt = level->AddLight();
    JedLightRec light{};
    check(level->GetLight(lt, &light, lt_all) == kOk, "GetLight succeeds");
    light.intensity = 3.0;
    check(level->SetLight(lt, &light, lt_all) == kOk, "SetLight succeeds");
    const SedLight& stored = context.levelState.lights[static_cast<std::size_t>(lt)];
    check(close(stored.intensity, 3.0), "SetLight writes the intensity");
    check(level->GetLight(lt, &light, lt_all) == kOk && close(light.rgbIntensity, 3.0)
            && close(light.rgbRange, stored.range), "GetLight reports the inherited RGB fields");
    light.intensity = 4.0;
    light.rgbIntensity = 5.0f;
    check(level->SetLight(lt, &light, lt_intensity | lt_rgbintensity) == kNotImplemented
            && close(stored.intensity, 3.0), "an independent RGB intensity rejects the whole write");
    light.rgbRange = stored.range + 1.0;
    check(level->SetLight(lt, &light, lt_rgbrange) == kNotImplemented
            && stored.range != light.rgbRange, "an independent RGB range is refused");
}

void testCogs()
{
    FakeContext context;
    CogValue symbol{};
    symbol.name = "speed";
    symbol.cogType = CogType::int_;
    symbol.type = ValueType::integer;
    symbol.intValue = 5;
    context.cogSymbols = std::vector<CogValue>{ symbol };
    JedPluginHost host(context);
    IJEDLevel* level = host.level();

    std::string script = "door.cog";
    const int cg = level->AddCOG(mutableText(script));
    check(cg == 0 && level->NCOGValues(cg) == 1, "AddCOG adds the script's symbols");
    check(std::string(level->GetCOGFileName(cg)) == "door.cog", "GetCOGFileName returns the script");
    check(level->GetCOGValueType(cg, 0) == ct_int, "GetCOGValueType uses JED COG type numbers");
    check(std::string(level->GetCOGValue(cg, 0)) == "5", "GetCOGValue returns the JED text");

    std::string value = "7";
    check(level->SetCOGValue(cg, 0, mutableText(value)), "SetCOGValue accepts a valid integer");
    check(context.levelState.cogs[0].values[0].intValue == 7, "SetCOGValue writes the value");

    std::string extraName = "sound";
    std::string extraValue = "door.wav";
    check(level->AddCOGValue(cg, mutableText(extraName), mutableText(extraValue), ct_wav) == 1,
        "AddCOGValue appends a value");
    check(level->AddCOGValue(cg, mutableText(extraName), mutableText(extraValue), 99) == -1,
        "AddCOGValue rejects an unknown COG type");

    context.cogSymbols.reset();
    std::string missing = "missing.cog";
    const std::size_t warningsBefore = context.warningCount();
    check(level->AddCOG(mutableText(missing)) == 1 && level->NCOGValues(1) == 0,
        "AddCOG with an unreadable script still adds an empty COG");
    check(context.warningCount() == warningsBefore + 1, "AddCOG warns when the script cannot be read");
    check(level->DeleteCOG(0) == kOk && level->NCOgs() == 1, "DeleteCOG removes the COG");
}

void testSelectionOrder()
{
    FakeContext context;
    for ( int i = 0; i < 4; ++i )
    {
        context.levelState.sectors.push_back(makeTestCubeSector(i * 4.0));
    }
    context.selectionState.sectors = { 3, 1 };
    JedPluginHost host(context);
    IJED* jed = host.jed();

    check(jed->GetNMultiselected(MM_SC) == 2, "GetNMultiselected counts the sector selection");
    check(jed->GetSelectedSC(0) == 1 && jed->GetSelectedSC(1) == 3, "selected sectors are listed in sorted order");
    check(jed->GetSelectedSC(2) == -1, "GetSelectedSC rejects an index past the end");
    check(jed->SelectSC(2) == 1 && jed->FindSelectedSC(3) == 2, "SelectSC returns the sorted position");
    check(jed->RemoveFromMultiselection(MM_SC, 0) == kOk && jed->FindSelectedSC(1) == -1,
        "RemoveFromMultiselection removes by sorted position");

    check(jed->SelectSF(0, 4) == 0, "SelectSF selects a surface");
    int sc = -1;
    int sf = -1;
    check(jed->GetSelectedSF(0, &sc, &sf) == kOk && sc == 0 && sf == 4, "GetSelectedSF returns the surface");

    const int vertex = context.levelState.sectors[2].surfaces[0].vertices[1];
    check(jed->SelectVX(2, vertex) == 0 && jed->GetNMultiselected(MM_VX) == 1, "SelectVX selects a sector vertex");
    int vx = -1;
    check(jed->GetSelectedVX(0, &sc, &vx) == kOk && sc == 2 && vx == vertex, "GetSelectedVX returns the sector vertex");

    check(jed->ClearMultiselection(MM_SC) == kOk && jed->GetNMultiselected(MM_SC) == 0,
        "ClearMultiselection clears one selection kind");
    check(jed->GetNMultiselected(MM_SF) == 1, "ClearMultiselection leaves other selection kinds");
    check(context.mutableLevelCalls == 0, "selection edits do not open a level edit");
}

void testCursorAndModes()
{
    FakeContext context;
    context.levelState.sectors.push_back(makeTestCubeSector(0.0));
    JedPluginHost host(context);
    IJED* jed = host.jed();

    check(jed->SetMapMode(MM_SF) == kOk && jed->GetMapMode() == MM_SF, "map mode round-trips");
    check(jed->SetCurSF(0, 3) == kOk, "SetCurSF accepts a valid surface");
    int sc = -1;
    int sf = -1;
    check(jed->GetCurSF(&sc, &sf) == kOk && sc == 0 && sf == 3, "GetCurSF returns the current surface");
    check(jed->SetCurSC(5) == kInvalidArgument && jed->GetCurSC() == 0, "SetCurSC rejects an invalid sector");
}

void testMessagesUndoAndUnsupported()
{
    FakeContext context;
    JedPluginHost host(context);
    IJED* jed = host.jed();

    std::string text = "baking";
    check(jed->PanMessage(msg_error, mutableText(text)) == kOk, "PanMessage succeeds");
    check(context.messages.back().first == JedMessageType::Error && context.messages.back().second == "baking",
        "PanMessage forwards the type and text");

    std::string label = "Light bake";
    check(jed->StartUndo(mutableText(label)) == kOk && context.undoLabels == std::vector<std::string>{ "Light bake" },
        "StartUndo names the pending edit");
    check(jed->SaveUndoForSector(0, 0, 0) == kOk, "per-item undo snapshots are accepted");
    check(jed->UpdateMap() == kOk && context.updateCalls == 1, "UpdateMap refreshes the editor");

    const std::size_t warningsBefore = context.warningCount();
    std::string path = "out.jkl";
    check(jed->SaveJKL(mutableText(path)) == kNotImplemented, "SaveJKL reports E_NOTIMPL");
    check(jed->SaveJKL(mutableText(path)) == kNotImplemented, "SaveJKL reports E_NOTIMPL again");
    check(context.warningCount() == warningsBefore + 1, "an unsupported method warns only once");
    check(jed->CreateWFRenderer(0, 0) == nullptr, "CreateWFRenderer returns null");

    check(std::string(jed->GetJEDString(js_LevelFile)) == "C:\\levels\\test.jkl", "GetJEDString uses the host");
    check(jed->GetJedWindow(jw_Main) == 0x1234, "GetJedWindow returns the host window handle");
    check(jed->IsLayerVisible(0) && !jed->IsLayerVisible(3), "IsLayerVisible uses the host");
    check(!jed->GetIsMOTS(), "GetIsMOTS reports the project kind");
}

void testPathHelpersAndStringLifetime()
{
    FakeContext context;
    JedPluginHost host(context);
    IJED* jed = host.jed();

    std::string path = "C:\\jk\\res.gob>mat\\rock.mat";
    check(std::string(jed->FileExtractExt(mutableText(path))) == ".mat", "FileExtractExt includes the dot");
    check(std::string(jed->FileExtractName(mutableText(path))) == "rock.mat", "FileExtractName strips the path");
    check(std::string(jed->FileExtractPath(mutableText(path))) == "C:\\jk\\res.gob>mat\\",
        "FileExtractPath keeps the trailing separator");
    check(jed->IsFileInContainer(mutableText(path)), "IsFileInContainer detects a container path");
    std::string gob = "C:\\jk\\RES.GOB";
    check(jed->IsFileContainer(mutableText(gob)), "IsFileContainer detects GOB files case-insensitively");
    std::string dotted = "C:\\a.dir\\noext";
    check(std::string(jed->FileExtractExt(mutableText(dotted))).empty(), "a dot in a directory is not an extension");

    const char* first = jed->FileExtractName(mutableText(path));
    for ( int i = 0; i < 40; ++i )
    {
        (void)jed->FileExtractExt(mutableText(path));
    }
    check(std::strcmp(first, "rock.mat") == 0, "returned strings survive later string-returning calls");
    check(first[std::strlen(first) + 1] == '\0' && first[std::strlen(first) + 2] == '\0',
        "returned strings carry wide-character NUL padding");
}

void testFiles()
{
    FakeContext context;
    context.files["data.bin"] = bytes("ABCDEF");
    context.files["list.txt"] = bytes("one\r\ntwo\nthree");
    JedPluginHost host(context);
    IJED* jed = host.jed();

    std::string missing = "missing.bin";
    check(jed->OpenFile(mutableText(missing)) == -1, "OpenFile returns -1 for a missing file");

    std::string name = "data.bin";
    const int handle = jed->OpenFile(mutableText(name));
    check(handle >= 0 && jed->GetFileSize(handle) == 6, "OpenFile returns a handle with the file size");
    char buffer[8]{};
    check(jed->ReadFile(handle, buffer, 4) == 4 && std::memcmp(buffer, "ABCD", 4) == 0, "ReadFile reads bytes");
    check(jed->GetFilePos(handle) == 4, "ReadFile advances the position");
    check(jed->ReadFile(handle, buffer, 8) == 2, "ReadFile stops at the end of the file");
    check(jed->SetFilePos(handle, 1) == kOk && jed->ReadFile(handle, buffer, 1) == 1 && buffer[0] == 'B',
        "SetFilePos seeks");
    check(jed->SetFilePos(handle, 7) == kInvalidArgument, "SetFilePos rejects a position past the end");
    check(std::string(jed->GetFileName(handle)) == "data.bin", "GetFileName returns the opened name");
    check(jed->CloseFile(handle) == kOk && jed->CloseFile(handle) == kInvalidArgument, "CloseFile closes once");
    check(jed->OpenGameFile(mutableText(name)) == handle, "closed file handles are reused");

    std::string listName = "list.txt";
    const int text = jed->OpenTextFile(mutableText(listName));
    check(text >= 0 && !jed->TextFileEof(text), "OpenTextFile opens a text file");
    check(std::string(jed->ReadTextFileString(text)) == "one", "ReadTextFileString strips CRLF");
    check(std::string(jed->ReadTextFileString(text)) == "two", "ReadTextFileString reads the next line");
    check(jed->TextFileCurrentLine(text) == 2, "TextFileCurrentLine counts lines read");
    check(std::string(jed->ReadTextFileString(text)) == "three" && jed->TextFileEof(text),
        "the last line without a newline is read before EOF");
    check(std::string(jed->ReadTextFileString(text)).empty(), "reading past EOF returns an empty string");
    check(jed->CloseTextFile(text) == kOk && jed->TextFileEof(text), "CloseTextFile closes the handle");
}

void testGeometryCommands()
{
    FakeContext context;
    JedPluginHost host(context);
    IJED* jed = host.jed();

    JedVector normal{ 0.0, 0.0, 1.0 };
    JedVector edge{ 1.0, 0.0, 0.0 };
    check(jed->CreateCubicSector(0.0, 0.0, 0.0, &normal, &edge) == 0, "CreateCubicSector returns the new sector");
    check(host.level()->SectorNSurfaces(0) == 6, "CreateCubicSector builds six surfaces");
    check(jed->IsSectorConvex(0), "the created cube is convex");

    JedBox box{};
    check(jed->FindBBox(0, &box) == kOk && box.x2 > box.x1 && box.z2 > box.z1, "FindBBox returns the sector bounds");
    const double cx = (box.x1 + box.x2) / 2.0;
    const double cy = (box.y1 + box.y2) / 2.0;
    const double cz = (box.z1 + box.z2) / 2.0;
    check(jed->IsInSector(0, cx, cy, cz) && jed->FindSectorForXYZ(cx, cy, cz) == 0, "point queries find the cube");

    JedVector cleaveNormal{ 1.0, 0.0, 0.0 };
    check(jed->CleaveSector(0, &cleaveNormal, cx, cy, cz) >= 1 && host.level()->NSectors() == 2,
        "CleaveSector splits the cube");
}

void testExceptionsDoNotCrossTheBoundary()
{
    FakeContext context;
    JedPluginHost host(context);
    context.throwOnLevel = true;

    check(host.level()->NSectors() == 0, "a failing host call returns the fallback");
    check(!context.messages.empty() && context.messages.back().first == JedMessageType::Error,
        "a failing host call is reported as an error");
}

std::vector<std::byte> peImage(const std::uint32_t peOffset, const std::uint16_t machine, const std::size_t size)
{
    std::vector<std::byte> image(size);
    const auto put = [&](const std::size_t at, const std::uint32_t value, const std::size_t width)
    {
        for ( std::size_t i = 0; i < width && at + i < image.size(); ++i )
        {
            image[at + i] = static_cast<std::byte>((value >> (8 * i)) & 0xFFu);
        }
    };
    put(0, 0x5A4D, 2);
    put(0x3C, peOffset, 4);
    put(peOffset, 0x00004550, 4);
    put(static_cast<std::size_t>(peOffset) + 4, machine, 2);
    return image;
}

void testPluginBinaryArchitecture()
{
    check(detectPeArchitecture(peImage(0x80, 0x014C, 0x100)) == PluginBinaryArchitecture::x86, "PE i386 is detected");
    check(detectPeArchitecture(peImage(0x80, 0x8664, 0x100)) == PluginBinaryArchitecture::x64, "PE AMD64 is detected");
    check(detectPeArchitecture(peImage(0x80, 0xAA64, 0x100)) == PluginBinaryArchitecture::arm64, "PE ARM64 is detected");
    check(detectPeArchitecture(peImage(0x80, 0x01C4, 0x100)) == PluginBinaryArchitecture::unknown,
        "other PE machines are unknown");
    check(detectPeArchitecture(peImage(0x80, 0x8664, 0x85)) == PluginBinaryArchitecture::unknown,
        "a truncated PE header is unknown");
    check(detectPeArchitecture(peImage(0xFFFFFFFCu, 0x8664, 0x100)) == PluginBinaryArchitecture::unknown,
        "an out-of-range PE offset is rejected without overflow");
    std::vector<std::byte> elf = peImage(0x80, 0x8664, 0x100);
    elf[0] = std::byte{ 0x7F };
    check(detectPeArchitecture(elf) == PluginBinaryArchitecture::unknown, "non-PE files are unknown");
    check(detectPeArchitecture({}) == PluginBinaryArchitecture::unknown, "an empty file is unknown");
    check(std::string(pluginArchitectureName(PluginBinaryArchitecture::x86)) == "32-bit x86",
        "architectures have designer-facing names");
}

} // namespace

void runJedPluginHostTests()
{
    testIdentityAndLevelAccess();
    testHeaderRoundTrip();
    testSectorAndSurfaceRecords();
    testInvalidIndicesAreRejected();
    testThingsAndLights();
    testCogs();
    testSelectionOrder();
    testCursorAndModes();
    testMessagesUndoAndUnsupported();
    testPathHelpersAndStringLifetime();
    testFiles();
    testGeometryCommands();
    testExceptionsDoNotCrossTheBoundary();
    testPluginBinaryArchitecture();
}

} // namespace sed::tests
