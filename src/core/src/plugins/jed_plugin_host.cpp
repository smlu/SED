#include "sed/plugins/jed_plugin_host.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <exception>
#include <set>
#include <string_view>
#include <utility>

#include "sed/geometry.hpp"
#include "sed/level/build.hpp"
#include "sed/level/cleave.hpp"
#include "sed/level/edit.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/merge.hpp"
#include "sed/values.hpp"

namespace sed::plugins
{

using namespace jed;

namespace
{

constexpr std::size_t kStringRingSize = 64;
// NULs appended after every returned string so plug-ins that read the
// record strings as UTF-16 (as Light Baker's header declares them) stop too.
constexpr std::size_t kStringPadding = 4;

std::string text(const char* value)
{
    return value != nullptr ? std::string(value) : std::string();
}

TVector toVector(const JedVector& v) noexcept
{
    return TVector(v.x, v.y, v.z);
}

JedVector fromVector(const TVector& v) noexcept
{
    return JedVector{ v.x, v.y, v.z };
}

JedBox fromBox(const TBox& b) noexcept
{
    return JedBox{ b.x1, b.y1, b.z1, b.x2, b.y2, b.z2 };
}

TBox toBox(const JedBox& b) noexcept
{
    TBox box{};
    box.Set(b.x1, b.y1, b.z1, b.x2, b.y2, b.z2);
    return box;
}

ValueType valueTypeForCogType(const CogType type) noexcept
{
    switch ( type )
    {
    case CogType::cog:
    case CogType::int_:
        return ValueType::integer;
    case CogType::float_:
        return ValueType::floating;
    case CogType::vect:
        return ValueType::vect;
    case CogType::sec:
    case CogType::surface:
    case CogType::thing:
        return ValueType::ptr;
    default:
        return ValueType::str;
    }
}

bool sameIntensity(const double a, const double b) noexcept
{
    return std::fabs(a - b) <= 1e-6;
}

struct OpenBinaryFile
{
    std::string name;
    std::vector<std::uint8_t> bytes;
    std::size_t position{};
};

struct TextFileState
{
    std::string name;
    std::vector<std::string> lines;
    std::size_t next{};
};

std::vector<std::string> splitLines(const std::vector<std::uint8_t>& bytes)
{
    std::vector<std::string> lines;
    std::string current;
    for ( const std::uint8_t byte : bytes )
    {
        if ( byte == '\n' )
        {
            if ( !current.empty() && current.back() == '\r' )
            {
                current.pop_back();
            }
            lines.push_back(std::move(current));
            current.clear();
        }
        else
        {
            current.push_back(static_cast<char>(byte));
        }
    }
    if ( !current.empty() )
    {
        if ( current.back() == '\r' )
        {
            current.pop_back();
        }
        lines.push_back(std::move(current));
    }
    return lines;
}

template <typename T>
int allocateHandle(std::vector<std::optional<T>>& handles, T value)
{
    for ( std::size_t i = 0; i < handles.size(); ++i )
    {
        if ( !handles[i] )
        {
            handles[i] = std::move(value);
            return static_cast<int>(i);
        }
    }
    handles.push_back(std::move(value));
    return static_cast<int>(handles.size() - 1);
}

template <typename T>
T* findHandle(std::vector<std::optional<T>>& handles, const int handle) noexcept
{
    if ( handle < 0 || static_cast<std::size_t>(handle) >= handles.size() || !handles[static_cast<std::size_t>(handle)] )
    {
        return nullptr;
    }
    return &*handles[static_cast<std::size_t>(handle)];
}

std::size_t lastSeparator(const std::string& path)
{
    const std::size_t pos = path.find_last_of("\\/>:");
    return pos;
}

} // namespace

struct JedPluginHost::Impl
{
    class LevelApi;
    class JedApi;

    explicit Impl(JedHostContext& hostContext);

    JedHostContext& context;
    std::array<std::string, kStringRingSize> strings{};
    std::size_t nextString{};
    std::set<std::string> reportedUnsupported{};
    std::vector<std::optional<OpenBinaryFile>> files{};
    std::vector<std::optional<TextFileState>> textFiles{};
    std::unique_ptr<LevelApi> levelApi;
    std::unique_ptr<JedApi> jedApi;

    char* cache(std::string value)
    {
        std::string& slot = strings[nextString];
        nextString = (nextString + 1) % strings.size();
        slot = std::move(value);
        slot.append(kStringPadding - 1, '\0');
        return slot.data();
    }

    void warn(const std::string& message)
    {
        context.message(JedMessageType::Warning, message);
    }

    void unsupported(const char* method)
    {
        if ( reportedUnsupported.insert(method).second )
        {
            warn(std::string("Plug-in called IJED::") + method + ", which SED does not support.");
        }
    }

    bool invalid(const char* method, const std::string& what)
    {
        warn(std::string("Plug-in called ") + method + " with " + what + ".");
        return false;
    }

    // Runs one entry point. No exception may cross the plug-in ABI.
    template <typename R, typename F>
    R guard(const char* method, R fallback, F&& body) noexcept
    {
        try
        {
            return body();
        }
        catch ( const std::exception& error )
        {
            try
            {
                context.message(JedMessageType::Error, std::string("Plug-in call ") + method + " failed: " + error.what());
            }
            catch ( ... )
            {
            }
        }
        catch ( ... )
        {
            try
            {
                context.message(JedMessageType::Error, std::string("Plug-in call ") + method + " failed.");
            }
            catch ( ... )
            {
            }
        }
        return fallback;
    }

    const Level& level() const
    {
        return context.level();
    }

    bool checkSector(const char* method, const int sc)
    {
        if ( sc >= 0 && static_cast<std::size_t>(sc) < level().sectors.size() )
        {
            return true;
        }
        return invalid(method, "invalid sector " + std::to_string(sc));
    }

    bool checkSurface(const char* method, const int sc, const int sf)
    {
        if ( !checkSector(method, sc) )
        {
            return false;
        }
        if ( sf >= 0 && static_cast<std::size_t>(sf) < level().sectors[static_cast<std::size_t>(sc)].surfaces.size() )
        {
            return true;
        }
        return invalid(method, "invalid surface " + std::to_string(sc) + "/" + std::to_string(sf));
    }

    bool checkSectorVertex(const char* method, const int sc, const int vx)
    {
        if ( !checkSector(method, sc) )
        {
            return false;
        }
        if ( vx >= 0 && static_cast<std::size_t>(vx) < level().sectors[static_cast<std::size_t>(sc)].vertices.size() )
        {
            return true;
        }
        return invalid(method, "invalid vertex " + std::to_string(sc) + "/" + std::to_string(vx));
    }

    bool checkSurfaceVertex(const char* method, const int sc, const int sf, const int vx)
    {
        if ( !checkSurface(method, sc, sf) )
        {
            return false;
        }
        const Surface& surface = level().sectors[static_cast<std::size_t>(sc)].surfaces[static_cast<std::size_t>(sf)];
        if ( vx >= 0 && static_cast<std::size_t>(vx) < surface.vertices.size() && surface.texVertices.size() == surface.vertices.size() )
        {
            return true;
        }
        return invalid(method, "invalid surface vertex " + std::to_string(vx));
    }

    bool checkThing(const char* method, const int th)
    {
        if ( th >= 0 && static_cast<std::size_t>(th) < level().things.size() )
        {
            return true;
        }
        return invalid(method, "invalid thing " + std::to_string(th));
    }

    bool checkThingValue(const char* method, const int th, const int n)
    {
        if ( !checkThing(method, th) )
        {
            return false;
        }
        if ( n >= 0 && static_cast<std::size_t>(n) < level().things[static_cast<std::size_t>(th)].values.size() )
        {
            return true;
        }
        return invalid(method, "invalid thing value " + std::to_string(n));
    }

    bool checkLight(const char* method, const int lt)
    {
        if ( lt >= 0 && static_cast<std::size_t>(lt) < level().lights.size() )
        {
            return true;
        }
        return invalid(method, "invalid light " + std::to_string(lt));
    }

    bool checkCog(const char* method, const int cg)
    {
        if ( cg >= 0 && static_cast<std::size_t>(cg) < level().cogs.size() )
        {
            return true;
        }
        return invalid(method, "invalid COG " + std::to_string(cg));
    }

    bool checkCogValue(const char* method, const int cg, const int n)
    {
        if ( !checkCog(method, cg) )
        {
            return false;
        }
        if ( n >= 0 && static_cast<std::size_t>(n) < level().cogs[static_cast<std::size_t>(cg)].values.size() )
        {
            return true;
        }
        return invalid(method, "invalid COG value " + std::to_string(n));
    }

    template <typename P>
    bool checkPointer(const char* method, const P* pointer)
    {
        return pointer != nullptr || invalid(method, "a null pointer");
    }

    Sector& mutableSector(const int sc)
    {
        return context.mutableLevel().sectors[static_cast<std::size_t>(sc)];
    }

    Surface& mutableSurface(const int sc, const int sf)
    {
        return mutableSector(sc).surfaces[static_cast<std::size_t>(sf)];
    }

    // Selection lists exposed to plug-ins in a deterministic (sorted) order.
    std::vector<std::int32_t> sortedSectors() const
    {
        const auto& set = context.selection().sectors;
        std::vector<std::int32_t> out(set.begin(), set.end());
        std::sort(out.begin(), out.end());
        return out;
    }

    std::vector<std::int32_t> sortedThings() const
    {
        const auto& set = context.selection().things;
        std::vector<std::int32_t> out(set.begin(), set.end());
        std::sort(out.begin(), out.end());
        return out;
    }

    std::vector<std::int32_t> sortedLights() const
    {
        const auto& set = context.selection().lights;
        std::vector<std::int32_t> out(set.begin(), set.end());
        std::sort(out.begin(), out.end());
        return out;
    }

    std::vector<std::pair<std::int32_t, std::int32_t>> sortedSurfaces() const
    {
        std::vector<std::pair<std::int32_t, std::int32_t>> out;
        for ( const SurfaceRef& ref : context.selection().surfaces )
        {
            out.emplace_back(ref.sector, ref.surface);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    std::vector<std::pair<std::int32_t, std::int32_t>> sortedFrames() const
    {
        std::vector<std::pair<std::int32_t, std::int32_t>> out;
        for ( const level::FrameRef& ref : context.selection().frames )
        {
            out.emplace_back(ref.thing, ref.value);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    // JED selects sector vertices; SED selects surface vertex corners. A
    // sector vertex is selected when any corner that uses it is selected.
    std::vector<std::pair<std::int32_t, std::int32_t>> sortedVertices() const
    {
        const Level& lev = level();
        std::set<std::pair<std::int32_t, std::int32_t>> unique;
        for ( const editor::SurfaceVertexRef& ref : context.selection().surfaceVertices )
        {
            const auto sc = static_cast<std::size_t>(ref.surface.sector);
            const auto sf = static_cast<std::size_t>(ref.surface.surface);
            const auto lv = static_cast<std::size_t>(ref.localVertex);
            if ( ref.surface.sector < 0 || sc >= lev.sectors.size() || ref.surface.surface < 0
                || sf >= lev.sectors[sc].surfaces.size() || ref.localVertex < 0
                || lv >= lev.sectors[sc].surfaces[sf].vertices.size() )
            {
                continue;
            }
            unique.emplace(ref.surface.sector, lev.sectors[sc].surfaces[sf].vertices[lv]);
        }
        return { unique.begin(), unique.end() };
    }

    template <typename T>
    static int indexOf(const std::vector<T>& list, const T& value)
    {
        const auto it = std::find(list.begin(), list.end(), value);
        return it == list.end() ? -1 : static_cast<int>(it - list.begin());
    }
};

class JedPluginHost::Impl::LevelApi final : public IJEDLevel
{
public:
    explicit LevelApi(Impl& impl) : h_(impl)
    {
    }

    JedResult SED_JED_CALL QueryInterface(const void*, void** object) override
    {
        if ( object != nullptr )
        {
            *object = nullptr;
        }
        return kClassNotAvailable;
    }

    JedLong SED_JED_CALL AddRef() override
    {
        return 1;
    }

    JedLong SED_JED_CALL Release() override
    {
        return 0;
    }

    JedResult SED_JED_CALL GetLevelHeader(JedLevelHeader* lh, int flags) override
    {
        return h_.guard("GetLevelHeader", kFail, [&]
        {
            if ( !h_.checkPointer("GetLevelHeader", lh) )
            {
                return kInvalidArgument;
            }
            const Level& level = h_.level();
            const LevelHeader& header = level.header;
            if ( flags & lh_version )
            {
                lh->version = header.version;
            }
            if ( flags & lh_gravity )
            {
                lh->gravity = header.gravity;
            }
            if ( flags & lh_skyZ )
            {
                lh->ceilingSkyZ = header.ceilingSky.height;
            }
            if ( flags & lh_cSkyOffs )
            {
                lh->ceilingSkyOffs[0] = header.ceilingSky.offset.x;
                lh->ceilingSkyOffs[1] = header.ceilingSky.offset.y;
            }
            if ( flags & lh_HorDist )
            {
                lh->horDistance = header.horizonSky.distance;
            }
            if ( flags & lh_HorPPR )
            {
                lh->horPixelsPerRev = header.horizonSky.pixelsPerRev;
            }
            if ( flags & lh_HSkyOffs )
            {
                lh->horSkyOffs[0] = header.horizonSky.offset.x;
                lh->horSkyOffs[1] = header.horizonSky.offset.y;
            }
            if ( flags & lh_MipMapDist )
            {
                std::copy(header.mipmapDistances.begin(), header.mipmapDistances.end(), lh->mipmapDist);
            }
            if ( flags & lh_LODDist )
            {
                std::copy(header.lodDistances.begin(), header.lodDistances.end(), lh->lodDist);
            }
            if ( flags & lh_PerspDist )
            {
                lh->perspDist = header.perspectiveDistance;
            }
            if ( flags & lh_GouraudDist )
            {
                lh->gouraudDist = header.gouraudDistance;
            }
            if ( flags & lh_ppu )
            {
                lh->pixelPerUnit = level.ppunit;
            }
            if ( flags & lh_MasterCMP )
            {
                lh->masterCmp = h_.cache(level.masterCMP);
            }
            return kOk;
        });
    }

    JedResult SED_JED_CALL SetLevelHeader(JedLevelHeader* lh, int flags) override
    {
        return h_.guard("SetLevelHeader", kFail, [&]
        {
            if ( !h_.checkPointer("SetLevelHeader", lh) )
            {
                return kInvalidArgument;
            }
            Level& level = h_.context.mutableLevel();
            LevelHeader& header = level.header;
            if ( flags & lh_version )
            {
                header.version = lh->version;
            }
            if ( flags & lh_gravity )
            {
                header.gravity = lh->gravity;
            }
            if ( flags & lh_skyZ )
            {
                header.ceilingSky.height = lh->ceilingSkyZ;
            }
            if ( flags & lh_cSkyOffs )
            {
                header.ceilingSky.offset.x = lh->ceilingSkyOffs[0];
                header.ceilingSky.offset.y = lh->ceilingSkyOffs[1];
            }
            if ( flags & lh_HorDist )
            {
                header.horizonSky.distance = lh->horDistance;
            }
            if ( flags & lh_HorPPR )
            {
                header.horizonSky.pixelsPerRev = lh->horPixelsPerRev;
            }
            if ( flags & lh_HSkyOffs )
            {
                header.horizonSky.offset.x = lh->horSkyOffs[0];
                header.horizonSky.offset.y = lh->horSkyOffs[1];
            }
            if ( flags & lh_MipMapDist )
            {
                std::copy(lh->mipmapDist, lh->mipmapDist + 4, header.mipmapDistances.begin());
            }
            if ( flags & lh_LODDist )
            {
                std::copy(lh->lodDist, lh->lodDist + 4, header.lodDistances.begin());
            }
            if ( flags & lh_PerspDist )
            {
                header.perspectiveDistance = lh->perspDist;
            }
            if ( flags & lh_GouraudDist )
            {
                header.gouraudDistance = lh->gouraudDist;
            }
            if ( flags & lh_ppu )
            {
                level.ppunit = lh->pixelPerUnit;
            }
            if ( flags & lh_MasterCMP )
            {
                level.masterCMP = text(lh->masterCmp);
            }
            return kOk;
        });
    }

    int SED_JED_CALL NSectors() override
    {
        return h_.guard("NSectors", 0, [&] { return static_cast<int>(h_.level().sectors.size()); });
    }

    int SED_JED_CALL NThings() override
    {
        return h_.guard("NThings", 0, [&] { return static_cast<int>(h_.level().things.size()); });
    }

    int SED_JED_CALL NLights() override
    {
        return h_.guard("NLights", 0, [&] { return static_cast<int>(h_.level().lights.size()); });
    }

    int SED_JED_CALL NCOgs() override
    {
        return h_.guard("NCOgs", 0, [&] { return static_cast<int>(h_.level().cogs.size()); });
    }

    int SED_JED_CALL AddSector() override
    {
        return h_.guard("AddSector", -1, [&]
        {
            Level& level = h_.context.mutableLevel();
            level.newSector();
            return static_cast<int>(level.sectors.size() - 1);
        });
    }

    JedResult SED_JED_CALL DeleteSector(int n) override
    {
        return h_.guard("DeleteSector", kFail, [&]
        {
            if ( !h_.checkSector("DeleteSector", n) )
            {
                return kInvalidArgument;
            }
            return level::levelDeleteSector(h_.context.mutableLevel(), n) ? kOk : kFail;
        });
    }

    JedResult SED_JED_CALL GetSector(int sec, JedSectorRec* rec, int flags) override
    {
        return h_.guard("GetSector", kFail, [&]
        {
            if ( !h_.checkSector("GetSector", sec) || !h_.checkPointer("GetSector", rec) )
            {
                return kInvalidArgument;
            }
            const Sector& sector = h_.level().sectors[static_cast<std::size_t>(sec)];
            if ( flags & s_flags )
            {
                rec->flags = sector.flags;
            }
            if ( flags & s_ambient )
            {
                rec->ambient = sector.ambient.intensity();
            }
            if ( flags & s_extra )
            {
                rec->extra = sector.extraLight.intensity();
            }
            if ( flags & s_cmp )
            {
                rec->colormap = h_.cache(sector.colormap);
            }
            if ( flags & s_tint )
            {
                rec->tintR = sector.tint.r;
                rec->tintG = sector.tint.g;
                rec->tintB = sector.tint.b;
            }
            if ( flags & s_sound )
            {
                rec->sound = h_.cache(sector.sound);
            }
            if ( flags & s_sndvol )
            {
                rec->soundVolume = sector.soundVolume;
            }
            if ( flags & s_layer )
            {
                rec->layer = sector.layer;
            }
            return kOk;
        });
    }

    JedResult SED_JED_CALL SetSector(int sec, JedSectorRec* rec, int flags) override
    {
        return h_.guard("SetSector", kFail, [&]
        {
            if ( !h_.checkSector("SetSector", sec) || !h_.checkPointer("SetSector", rec) )
            {
                return kInvalidArgument;
            }
            Sector& sector = h_.mutableSector(sec);
            if ( flags & s_flags )
            {
                sector.flags = rec->flags;
            }
            // Ambient and extra light are colors in SED; an unchanged
            // intensity keeps the authored color instead of graying it.
            if ( (flags & s_ambient) && !sameIntensity(sector.ambient.intensity(), rec->ambient) )
            {
                sector.ambient = intensityToColor(static_cast<float>(rec->ambient));
            }
            if ( (flags & s_extra) && !sameIntensity(sector.extraLight.intensity(), rec->extra) )
            {
                sector.extraLight = intensityToColor(static_cast<float>(rec->extra));
            }
            if ( flags & s_cmp )
            {
                sector.colormap = text(rec->colormap);
            }
            if ( flags & s_tint )
            {
                sector.tint.r = rec->tintR;
                sector.tint.g = rec->tintG;
                sector.tint.b = rec->tintB;
            }
            if ( flags & s_sound )
            {
                sector.sound = text(rec->sound);
            }
            if ( flags & s_sndvol )
            {
                sector.soundVolume = rec->soundVolume;
            }
            if ( flags & s_layer )
            {
                sector.layer = rec->layer;
            }
            return kOk;
        });
    }

    int SED_JED_CALL SectorNVertices(int sec) override
    {
        return h_.guard("SectorNVertices", -1, [&]
        {
            return h_.checkSector("SectorNVertices", sec)
                ? static_cast<int>(h_.level().sectors[static_cast<std::size_t>(sec)].vertices.size())
                : -1;
        });
    }

    int SED_JED_CALL SectorNSurfaces(int sec) override
    {
        return h_.guard("SectorNSurfaces", -1, [&]
        {
            return h_.checkSector("SectorNSurfaces", sec)
                ? static_cast<int>(h_.level().sectors[static_cast<std::size_t>(sec)].surfaces.size())
                : -1;
        });
    }

    JedResult SED_JED_CALL SectorGetVertex(int sec, int vx, double* x, double* y, double* z) override
    {
        return h_.guard("SectorGetVertex", kFail, [&]
        {
            if ( !h_.checkSectorVertex("SectorGetVertex", sec, vx) || !h_.checkPointer("SectorGetVertex", x)
                || !h_.checkPointer("SectorGetVertex", y) || !h_.checkPointer("SectorGetVertex", z) )
            {
                return kInvalidArgument;
            }
            const Vertex& v = h_.level().sectors[static_cast<std::size_t>(sec)].vertices[static_cast<std::size_t>(vx)];
            *x = v.x;
            *y = v.y;
            *z = v.z;
            return kOk;
        });
    }

    JedResult SED_JED_CALL SectorSetVertex(int sec, int vx, double x, double y, double z) override
    {
        return h_.guard("SectorSetVertex", kFail, [&]
        {
            if ( !h_.checkSectorVertex("SectorSetVertex", sec, vx) )
            {
                return kInvalidArgument;
            }
            Vertex& v = h_.mutableSector(sec).vertices[static_cast<std::size_t>(vx)];
            v.x = x;
            v.y = y;
            v.z = z;
            return kOk;
        });
    }

    int SED_JED_CALL SectorAddVertex(int sec, double x, double y, double z) override
    {
        return h_.guard("SectorAddVertex", -1, [&]
        {
            return h_.checkSector("SectorAddVertex", sec) ? h_.mutableSector(sec).newVertex(x, y, z) : -1;
        });
    }

    int SED_JED_CALL SectorFindVertex(int sec, double x, double y, double z) override
    {
        return h_.guard("SectorFindVertex", -1, [&]
        {
            return h_.checkSector("SectorFindVertex", sec)
                ? h_.level().sectors[static_cast<std::size_t>(sec)].findVertex(x, y, z)
                : -1;
        });
    }

    int SED_JED_CALL SectorDeleteVertex(int sec, int n) override
    {
        return h_.guard("SectorDeleteVertex", -1, [&]
        {
            if ( !h_.checkSectorVertex("SectorDeleteVertex", sec, n) )
            {
                return -1;
            }
            return level::levelDeleteSectorVertex(h_.context.mutableLevel(), sec, n, true) ? n : -1;
        });
    }

    int SED_JED_CALL SectorAddSurface(int sec) override
    {
        return h_.guard("SectorAddSurface", -1, [&]
        {
            if ( !h_.checkSector("SectorAddSurface", sec) )
            {
                return -1;
            }
            Sector& sector = h_.mutableSector(sec);
            Surface surface{};
            surface.num = static_cast<std::int32_t>(sector.surfaces.size());
            return sector.addSurface(std::move(surface));
        });
    }

    JedResult SED_JED_CALL SectorDeleteSurface(int sc, int sf) override
    {
        return h_.guard("SectorDeleteSurface", kFail, [&]
        {
            if ( !h_.checkSurface("SectorDeleteSurface", sc, sf) )
            {
                return kInvalidArgument;
            }
            return level::levelDeleteSurface(h_.context.mutableLevel(), SurfaceRef{ sc, sf }) ? kOk : kFail;
        });
    }

    JedResult SED_JED_CALL SectorUpdate(int sec) override
    {
        return h_.guard("SectorUpdate", kFail, [&]
        {
            if ( !h_.checkSector("SectorUpdate", sec) )
            {
                return kInvalidArgument;
            }
            Sector& sector = h_.mutableSector(sec);
            for ( std::size_t i = 0; i < sector.surfaces.size(); ++i )
            {
                sector.surfaces[i].num = static_cast<std::int32_t>(i);
            }
            return kOk;
        });
    }

    JedResult SED_JED_CALL GetSurface(int sc, int sf, JedSurfaceRec* rec, int flags) override
    {
        return h_.guard("GetSurface", kFail, [&]
        {
            if ( !h_.checkSurface("GetSurface", sc, sf) || !h_.checkPointer("GetSurface", rec) )
            {
                return kInvalidArgument;
            }
            const Surface& surface = h_.level().sectors[static_cast<std::size_t>(sc)].surfaces[static_cast<std::size_t>(sf)];
            if ( flags & sf_adjoin )
            {
                rec->adjoinSc = surface.adjoin ? surface.adjoin->sector : -1;
                rec->adjoinSf = surface.adjoin ? surface.adjoin->surface : -1;
            }
            if ( flags & sf_adjoinflags )
            {
                rec->adjoinFlags = surface.adjoinFlags;
            }
            if ( flags & sf_SurfFlags )
            {
                rec->surfFlags = surface.surfaceFlags;
            }
            if ( flags & sf_FaceFlags )
            {
                rec->faceFlags = static_cast<JedLong>(surface.faceFlags);
            }
            if ( flags & sf_Material )
            {
                rec->material = h_.cache(surface.material);
            }
            if ( flags & sf_geo )
            {
                rec->geo = surface.geo;
            }
            if ( flags & sf_light )
            {
                rec->light = surface.light;
            }
            if ( flags & sf_tex )
            {
                rec->tex = surface.tex;
            }
            if ( flags & sf_ExtraLight )
            {
                rec->extraLight = surface.extraLight.intensity();
            }
            if ( flags & sf_txscale )
            {
                rec->uScale = surface.uScale;
                rec->vScale = surface.vScale;
            }
            return kOk;
        });
    }

    JedResult SED_JED_CALL SetSurface(int sc, int sf, JedSurfaceRec* rec, int flags) override
    {
        return h_.guard("SetSurface", kFail, [&]
        {
            if ( !h_.checkSurface("SetSurface", sc, sf) || !h_.checkPointer("SetSurface", rec) )
            {
                return kInvalidArgument;
            }
            if ( (flags & sf_adjoin) && rec->adjoinSc >= 0 && !h_.checkSurface("SetSurface", rec->adjoinSc, rec->adjoinSf) )
            {
                return kInvalidArgument;
            }
            Surface& surface = h_.mutableSurface(sc, sf);
            if ( flags & sf_adjoin )
            {
                if ( rec->adjoinSc < 0 )
                {
                    surface.adjoin.reset();
                }
                else
                {
                    surface.adjoin = SurfaceRef{ rec->adjoinSc, rec->adjoinSf };
                }
            }
            if ( flags & sf_adjoinflags )
            {
                surface.adjoinFlags = rec->adjoinFlags;
            }
            if ( flags & sf_SurfFlags )
            {
                surface.surfaceFlags = rec->surfFlags;
            }
            if ( flags & sf_FaceFlags )
            {
                surface.faceFlags = static_cast<std::uint32_t>(rec->faceFlags);
            }
            if ( flags & sf_Material )
            {
                surface.material = text(rec->material);
            }
            if ( flags & sf_geo )
            {
                surface.geo = rec->geo;
            }
            if ( flags & sf_light )
            {
                surface.light = rec->light;
            }
            if ( flags & sf_tex )
            {
                surface.tex = rec->tex;
            }
            if ( (flags & sf_ExtraLight) && !sameIntensity(surface.extraLight.intensity(), rec->extraLight) )
            {
                surface.extraLight = signedIntensityToColor(static_cast<float>(rec->extraLight));
            }
            if ( flags & sf_txscale )
            {
                surface.uScale = rec->uScale;
                surface.vScale = rec->vScale;
            }
            return kOk;
        });
    }

    JedResult SED_JED_CALL GetSurfaceNormal(int sc, int sf, JedVector* n) override
    {
        return h_.guard("GetSurfaceNormal", kFail, [&]
        {
            if ( !h_.checkSurface("GetSurfaceNormal", sc, sf) || !h_.checkPointer("GetSurfaceNormal", n) )
            {
                return kInvalidArgument;
            }
            *n = fromVector(h_.level().sectors[static_cast<std::size_t>(sc)].surfaces[static_cast<std::size_t>(sf)].normal);
            return kOk;
        });
    }

    JedResult SED_JED_CALL SurfaceUpdate(int sc, int sf, int how) override
    {
        return h_.guard("SurfaceUpdate", kFail, [&]
        {
            if ( !h_.checkSurface("SurfaceUpdate", sc, sf) )
            {
                return kInvalidArgument;
            }
            Level& level = h_.context.mutableLevel();
            Sector& sector = level.sectors[static_cast<std::size_t>(sc)];
            Surface& surface = sector.surfaces[static_cast<std::size_t>(sf)];
            if ( how & su_texture )
            {
                level::surfaceRecalcAll(level::levelPixelPerUnit(level), sector, surface);
            }
            else
            {
                surface.normal = level::surfaceCalcNormalNewell(sector, surface);
            }
            if ( how & su_floorflag )
            {
                level::surfaceCheckIfFloor(surface);
            }
            return kOk;
        });
    }

    int SED_JED_CALL SurfaceNVertices(int sc, int sf) override
    {
        return h_.guard("SurfaceNVertices", -1, [&]
        {
            return h_.checkSurface("SurfaceNVertices", sc, sf)
                ? static_cast<int>(h_.level().sectors[static_cast<std::size_t>(sc)].surfaces[static_cast<std::size_t>(sf)].vertices.size())
                : -1;
        });
    }

    int SED_JED_CALL SurfaceGetVertexNum(int sc, int sf, int vx) override
    {
        return h_.guard("SurfaceGetVertexNum", -1, [&]
        {
            return h_.checkSurfaceVertex("SurfaceGetVertexNum", sc, sf, vx)
                ? h_.level().sectors[static_cast<std::size_t>(sc)].surfaces[static_cast<std::size_t>(sf)].vertices[static_cast<std::size_t>(vx)]
                : -1;
        });
    }

    JedResult SED_JED_CALL SurfaceSetVertexNum(int sc, int sf, int vx, int secvx) override
    {
        return h_.guard("SurfaceSetVertexNum", kFail, [&]
        {
            if ( !h_.checkSurfaceVertex("SurfaceSetVertexNum", sc, sf, vx) || !h_.checkSectorVertex("SurfaceSetVertexNum", sc, secvx) )
            {
                return kInvalidArgument;
            }
            h_.mutableSurface(sc, sf).vertices[static_cast<std::size_t>(vx)] = secvx;
            return kOk;
        });
    }

    int SED_JED_CALL SurfaceAddVertex(int sc, int sf, int secvx) override
    {
        return SurfaceInsertVertexAt("SurfaceAddVertex", sc, sf, -1, secvx);
    }

    int SED_JED_CALL SurfaceInsertVertex(int sc, int sf, int at, int secvx) override
    {
        return SurfaceInsertVertexAt("SurfaceInsertVertex", sc, sf, at, secvx);
    }

    JedResult SED_JED_CALL SurfaceDeleteVertex(int sc, int sf, int n) override
    {
        return h_.guard("SurfaceDeleteVertex", kFail, [&]
        {
            if ( !h_.checkSurfaceVertex("SurfaceDeleteVertex", sc, sf, n) )
            {
                return kInvalidArgument;
            }
            Surface& surface = h_.mutableSurface(sc, sf);
            surface.vertices.erase(surface.vertices.begin() + n);
            surface.texVertices.erase(surface.texVertices.begin() + n);
            return kOk;
        });
    }

    JedResult SED_JED_CALL SurfaceGetVertexUV(int sc, int sf, int vx, float* u, float* v) override
    {
        return h_.guard("SurfaceGetVertexUV", kFail, [&]
        {
            if ( !h_.checkSurfaceVertex("SurfaceGetVertexUV", sc, sf, vx) || !h_.checkPointer("SurfaceGetVertexUV", u)
                || !h_.checkPointer("SurfaceGetVertexUV", v) )
            {
                return kInvalidArgument;
            }
            const TexVertex& tv = h_.level().sectors[static_cast<std::size_t>(sc)].surfaces[static_cast<std::size_t>(sf)].texVertices[static_cast<std::size_t>(vx)];
            *u = tv.u;
            *v = tv.v;
            return kOk;
        });
    }

    JedResult SED_JED_CALL SurfaceSetVertexUV(int sc, int sf, int vx, float u, float v) override
    {
        return h_.guard("SurfaceSetVertexUV", kFail, [&]
        {
            if ( !h_.checkSurfaceVertex("SurfaceSetVertexUV", sc, sf, vx) )
            {
                return kInvalidArgument;
            }
            TexVertex& tv = h_.mutableSurface(sc, sf).texVertices[static_cast<std::size_t>(vx)];
            tv.u = u;
            tv.v = v;
            return kOk;
        });
    }

    JedResult SED_JED_CALL SurfaceGetVertexLight(int sc, int sf, int vx, float* white, float* r, float* g, float* b) override
    {
        return h_.guard("SurfaceGetVertexLight", kFail, [&]
        {
            if ( !h_.checkSurfaceVertex("SurfaceGetVertexLight", sc, sf, vx) || !h_.checkPointer("SurfaceGetVertexLight", white)
                || !h_.checkPointer("SurfaceGetVertexLight", r) || !h_.checkPointer("SurfaceGetVertexLight", g)
                || !h_.checkPointer("SurfaceGetVertexLight", b) )
            {
                return kInvalidArgument;
            }
            const ColorF& color = h_.level().sectors[static_cast<std::size_t>(sc)].surfaces[static_cast<std::size_t>(sf)].texVertices[static_cast<std::size_t>(vx)].color;
            *white = color.a;
            *r = color.r;
            *g = color.g;
            *b = color.b;
            return kOk;
        });
    }

    JedResult SED_JED_CALL SurfaceSetVertexLight(int sc, int sf, int vx, float white, float r, float g, float b) override
    {
        return h_.guard("SurfaceSetVertexLight", kFail, [&]
        {
            if ( !h_.checkSurfaceVertex("SurfaceSetVertexLight", sc, sf, vx) )
            {
                return kInvalidArgument;
            }
            h_.mutableSurface(sc, sf).texVertices[static_cast<std::size_t>(vx)].color = ColorF(r, g, b, white);
            return kOk;
        });
    }

    int SED_JED_CALL AddThing() override
    {
        return h_.guard("AddThing", -1, [&]
        {
            Level& level = h_.context.mutableLevel();
            level.newThing();
            return static_cast<int>(level.things.size() - 1);
        });
    }

    JedResult SED_JED_CALL DeleteThing(int th) override
    {
        return h_.guard("DeleteThing", kFail, [&]
        {
            if ( !h_.checkThing("DeleteThing", th) )
            {
                return kInvalidArgument;
            }
            return level::levelDeleteThing(h_.context.mutableLevel(), th) ? kOk : kFail;
        });
    }

    JedResult SED_JED_CALL GetThing(int th, JedThingRec* rec, int flags) override
    {
        return h_.guard("GetThing", kFail, [&]
        {
            if ( !h_.checkThing("GetThing", th) || !h_.checkPointer("GetThing", rec) )
            {
                return kInvalidArgument;
            }
            const Thing& thing = h_.level().things[static_cast<std::size_t>(th)];
            // JED's thing name is the template it instantiates.
            if ( flags & th_name )
            {
                rec->name = h_.cache(!thing.templateName.empty() ? thing.templateName : thing.name);
            }
            if ( flags & th_sector )
            {
                rec->sector = thing.sector;
            }
            if ( flags & th_position )
            {
                rec->x = thing.x;
                rec->y = thing.y;
                rec->z = thing.z;
            }
            if ( flags & th_orientation )
            {
                rec->pch = thing.pch;
                rec->yaw = thing.yaw;
                rec->rol = thing.rol;
            }
            if ( flags & th_layer )
            {
                rec->layer = thing.layer;
            }
            return kOk;
        });
    }

    JedResult SED_JED_CALL SetThing(int th, JedThingRec* rec, int flags) override
    {
        return h_.guard("SetThing", kFail, [&]
        {
            if ( !h_.checkThing("SetThing", th) || !h_.checkPointer("SetThing", rec) )
            {
                return kInvalidArgument;
            }
            if ( (flags & th_sector) && rec->sector >= 0 && !h_.checkSector("SetThing", rec->sector) )
            {
                return kInvalidArgument;
            }
            Thing& thing = h_.context.mutableLevel().things[static_cast<std::size_t>(th)];
            if ( flags & th_name )
            {
                const std::string name = text(rec->name);
                const std::string previous = !thing.templateName.empty() ? thing.templateName : thing.name;
                if ( name != previous )
                {
                    // Keep a distinct instance name; otherwise it follows the template.
                    if ( thing.name.empty() || thing.name == previous )
                    {
                        thing.name = name;
                    }
                    thing.templateName = name;
                }
            }
            if ( flags & th_sector )
            {
                thing.sector = rec->sector < 0 ? -1 : rec->sector;
            }
            if ( flags & th_position )
            {
                thing.x = rec->x;
                thing.y = rec->y;
                thing.z = rec->z;
            }
            if ( flags & th_orientation )
            {
                thing.pch = rec->pch;
                thing.yaw = rec->yaw;
                thing.rol = rec->rol;
            }
            if ( flags & th_layer )
            {
                thing.layer = rec->layer;
            }
            return kOk;
        });
    }

    JedResult SED_JED_CALL ThingUpdate(int th) override
    {
        return h_.guard("ThingUpdate", kFail, [&]
        {
            return h_.checkThing("ThingUpdate", th) ? kOk : kInvalidArgument;
        });
    }

    int SED_JED_CALL AddLight() override
    {
        return h_.guard("AddLight", -1, [&]
        {
            Level& level = h_.context.mutableLevel();
            level.newLight();
            return static_cast<int>(level.lights.size() - 1);
        });
    }

    JedResult SED_JED_CALL DeleteLight(int lt) override
    {
        return h_.guard("DeleteLight", kFail, [&]
        {
            if ( !h_.checkLight("DeleteLight", lt) )
            {
                return kInvalidArgument;
            }
            return level::levelDeleteLight(h_.context.mutableLevel(), lt) ? kOk : kFail;
        });
    }

    JedResult SED_JED_CALL GetLight(int lt, JedLightRec* rec, int flags) override
    {
        return h_.guard("GetLight", kFail, [&]
        {
            if ( !h_.checkLight("GetLight", lt) || !h_.checkPointer("GetLight", rec) )
            {
                return kInvalidArgument;
            }
            const SedLight& light = h_.level().lights[static_cast<std::size_t>(lt)];
            if ( flags & lt_position )
            {
                rec->x = light.position.x;
                rec->y = light.position.y;
                rec->z = light.position.z;
            }
            if ( flags & lt_intensity )
            {
                rec->intensity = light.intensity;
            }
            if ( flags & lt_range )
            {
                rec->range = light.range;
            }
            if ( flags & lt_rgb )
            {
                rec->r = light.color.r;
                rec->g = light.color.g;
                rec->b = light.color.b;
            }
            if ( flags & lt_rgbintensity )
            {
                rec->rgbIntensity = static_cast<float>(light.intensity);
            }
            if ( flags & lt_rgbrange )
            {
                rec->rgbRange = light.range;
            }
            if ( flags & lt_flags )
            {
                rec->flags = light.flags;
            }
            if ( flags & lt_layer )
            {
                rec->layer = light.layer;
            }
            return kOk;
        });
    }

    JedResult SED_JED_CALL SetLight(int lt, JedLightRec* rec, int flags) override
    {
        return h_.guard("SetLight", kFail, [&]
        {
            if ( !h_.checkLight("SetLight", lt) || !h_.checkPointer("SetLight", rec) )
            {
                return kInvalidArgument;
            }
            const auto index = static_cast<std::size_t>(lt);
            const SedLight& current = h_.level().lights[index];
            // A get-modify-set of lt_all leaves the previous inherited value
            // in the RGB slot. An explicit value matching the new primary is
            // also representable. Independent RGB fields are absent from SED.
            const bool changedRgbIntensity = (flags & lt_rgbintensity)
                && rec->rgbIntensity != static_cast<float>(current.intensity)
                && (!(flags & lt_intensity) || rec->rgbIntensity != static_cast<float>(rec->intensity));
            const bool changedRgbRange = (flags & lt_rgbrange) && rec->rgbRange != current.range
                && (!(flags & lt_range) || rec->rgbRange != rec->range);
            if (changedRgbIntensity || changedRgbRange)
            {
                h_.unsupported("SetLight independent RGB intensity/range");
                return kNotImplemented;
            }
            SedLight& light = h_.context.mutableLevel().lights[index];
            if ( flags & lt_position )
            {
                light.position = TVector(rec->x, rec->y, rec->z);
            }
            if ( flags & lt_intensity )
            {
                light.intensity = rec->intensity;
            }
            if ( flags & lt_range )
            {
                light.range = rec->range;
            }
            if ( flags & lt_rgb )
            {
                light.color.r = rec->r;
                light.color.g = rec->g;
                light.color.b = rec->b;
            }
            if ( flags & lt_flags )
            {
                light.flags = rec->flags;
            }
            if ( flags & lt_layer )
            {
                light.layer = rec->layer;
            }
            return kOk;
        });
    }

    int SED_JED_CALL NLayers() override
    {
        return h_.guard("NLayers", 0, [&] { return static_cast<int>(h_.level().layers.size()); });
    }

    char* SED_JED_CALL GetLayerName(int n) override
    {
        return h_.guard<char*>("GetLayerName", nullptr, [&] { return h_.cache(h_.level().getLayerName(n)); });
    }

    int SED_JED_CALL AddLayer(char* name) override
    {
        return h_.guard("AddLayer", -1, [&]
        {
            const std::string layer = text(name);
            const auto& layers = h_.level().layers;
            if ( const auto it = std::find(layers.begin(), layers.end(), layer); it != layers.end() )
            {
                return static_cast<int>(it - layers.begin());
            }
            return h_.context.mutableLevel().addLayer(layer);
        });
    }

    int SED_JED_CALL NTHingValues(int th) override
    {
        return h_.guard("NTHingValues", -1, [&]
        {
            return h_.checkThing("NTHingValues", th)
                ? static_cast<int>(h_.level().things[static_cast<std::size_t>(th)].values.size())
                : -1;
        });
    }

    char* SED_JED_CALL GetThingValueName(int th, int n) override
    {
        return h_.guard<char*>("GetThingValueName", nullptr, [&]() -> char*
        {
            return h_.checkThingValue("GetThingValueName", th, n)
                ? h_.cache(h_.level().things[static_cast<std::size_t>(th)].values[static_cast<std::size_t>(n)].name)
                : nullptr;
        });
    }

    char* SED_JED_CALL GetThingValueData(int th, int n) override
    {
        return h_.guard<char*>("GetThingValueData", nullptr, [&]() -> char*
        {
            return h_.checkThingValue("GetThingValueData", th, n)
                ? h_.cache(h_.level().things[static_cast<std::size_t>(th)].values[static_cast<std::size_t>(n)].asString())
                : nullptr;
        });
    }

    JedResult SED_JED_CALL SetThingValueData(int th, int n, char* val) override
    {
        return h_.guard("SetThingValueData", kFail, [&]
        {
            if ( !h_.checkThingValue("SetThingValueData", th, n) )
            {
                return kInvalidArgument;
            }
            TplValue& value = h_.context.mutableLevel().things[static_cast<std::size_t>(th)].values[static_cast<std::size_t>(n)];
            value = deserializeTplValue(value.name, text(val));
            return kOk;
        });
    }

    JedResult SED_JED_CALL GetThingFrame(int th, int n, double* x, double* y, double* z, double* pch, double* yaw, double* rol) override
    {
        return h_.guard("GetThingFrame", kFail, [&]
        {
            if ( !h_.checkThingValue("GetThingFrame", th, n) || !x || !y || !z || !pch || !yaw || !rol )
            {
                return kInvalidArgument;
            }
            const TplValue& value = h_.level().things[static_cast<std::size_t>(th)].values[static_cast<std::size_t>(n)];
            return value.tryGetFrame(*x, *y, *z, *pch, *yaw, *rol) ? kOk : kFail;
        });
    }

    JedResult SED_JED_CALL SetThingFrame(int th, int n, double x, double y, double z, double pch, double yaw, double rol) override
    {
        return h_.guard("SetThingFrame", kFail, [&]
        {
            if ( !h_.checkThingValue("SetThingFrame", th, n) )
            {
                return kInvalidArgument;
            }
            h_.context.mutableLevel().things[static_cast<std::size_t>(th)].values[static_cast<std::size_t>(n)].setFrame(x, y, z, pch, yaw, rol);
            return kOk;
        });
    }

    int SED_JED_CALL AddThingValue(int th, char* name, char* val) override
    {
        return h_.guard("AddThingValue", -1, [&]
        {
            if ( !h_.checkThing("AddThingValue", th) )
            {
                return -1;
            }
            auto& values = h_.context.mutableLevel().things[static_cast<std::size_t>(th)].values;
            values.push_back(deserializeTplValue(text(name), text(val)));
            return static_cast<int>(values.size() - 1);
        });
    }

    JedResult SED_JED_CALL InsertThingValue(int th, int n, char* name, char* val) override
    {
        return h_.guard("InsertThingValue", kFail, [&]
        {
            if ( !h_.checkThing("InsertThingValue", th) )
            {
                return kInvalidArgument;
            }
            if ( n < 0 || static_cast<std::size_t>(n) > h_.level().things[static_cast<std::size_t>(th)].values.size() )
            {
                h_.invalid("InsertThingValue", "invalid thing value " + std::to_string(n));
                return kInvalidArgument;
            }
            auto& values = h_.context.mutableLevel().things[static_cast<std::size_t>(th)].values;
            values.insert(values.begin() + n, deserializeTplValue(text(name), text(val)));
            return kOk;
        });
    }

    JedResult SED_JED_CALL DeleteThingValue(int th, int n) override
    {
        return h_.guard("DeleteThingValue", kFail, [&]
        {
            if ( !h_.checkThingValue("DeleteThingValue", th, n) )
            {
                return kInvalidArgument;
            }
            auto& values = h_.context.mutableLevel().things[static_cast<std::size_t>(th)].values;
            values.erase(values.begin() + n);
            return kOk;
        });
    }

    int SED_JED_CALL AddCOG(char* name) override
    {
        return h_.guard("AddCOG", -1, [&]
        {
            const std::string script = text(name);
            if ( script.empty() )
            {
                h_.invalid("AddCOG", "an empty COG script name");
                return -1;
            }
            std::optional<std::vector<CogValue>> symbols = h_.context.cogScriptSymbols(script);
            if ( !symbols )
            {
                h_.warn("Plug-in added COG " + script + ", but its script could not be read; the COG has no values.");
            }
            Level& level = h_.context.mutableLevel();
            Cog& cog = level.newCog();
            cog.name = script;
            if ( symbols )
            {
                cog.values = std::move(*symbols);
            }
            return static_cast<int>(level.cogs.size() - 1);
        });
    }

    JedResult SED_JED_CALL DeleteCOG(int n) override
    {
        return h_.guard("DeleteCOG", kFail, [&]
        {
            if ( !h_.checkCog("DeleteCOG", n) )
            {
                return kInvalidArgument;
            }
            return level::levelDeleteCog(h_.context.mutableLevel(), n) ? kOk : kFail;
        });
    }

    JedResult SED_JED_CALL UpdateCOG(int cg) override
    {
        return h_.guard("UpdateCOG", kFail, [&]
        {
            return h_.checkCog("UpdateCOG", cg) ? kOk : kInvalidArgument;
        });
    }

    char* SED_JED_CALL GetCOGFileName(int cg) override
    {
        return h_.guard<char*>("GetCOGFileName", nullptr, [&]() -> char*
        {
            return h_.checkCog("GetCOGFileName", cg) ? h_.cache(h_.level().cogs[static_cast<std::size_t>(cg)].name) : nullptr;
        });
    }

    int SED_JED_CALL NCOGValues(int cg) override
    {
        return h_.guard("NCOGValues", -1, [&]
        {
            return h_.checkCog("NCOGValues", cg) ? static_cast<int>(h_.level().cogs[static_cast<std::size_t>(cg)].values.size()) : -1;
        });
    }

    char* SED_JED_CALL GetCOGValueName(int cg, int n) override
    {
        return h_.guard<char*>("GetCOGValueName", nullptr, [&]() -> char*
        {
            return h_.checkCogValue("GetCOGValueName", cg, n)
                ? h_.cache(h_.level().cogs[static_cast<std::size_t>(cg)].values[static_cast<std::size_t>(n)].name)
                : nullptr;
        });
    }

    int SED_JED_CALL GetCOGValueType(int cg, int n) override
    {
        return h_.guard("GetCOGValueType", -1, [&]
        {
            return h_.checkCogValue("GetCOGValueType", cg, n)
                ? static_cast<int>(h_.level().cogs[static_cast<std::size_t>(cg)].values[static_cast<std::size_t>(n)].cogType)
                : -1;
        });
    }

    char* SED_JED_CALL GetCOGValue(int cg, int n) override
    {
        return h_.guard<char*>("GetCOGValue", nullptr, [&]() -> char*
        {
            if ( !h_.checkCogValue("GetCOGValue", cg, n) )
            {
                return nullptr;
            }
            const Level& level = h_.level();
            return h_.cache(level.cogs[static_cast<std::size_t>(cg)].values[static_cast<std::size_t>(n)].asJedString(level));
        });
    }

    bool SED_JED_CALL SetCOGValue(int cg, int n, char* val) override
    {
        return h_.guard("SetCOGValue", false, [&]
        {
            if ( !h_.checkCogValue("SetCOGValue", cg, n) )
            {
                return false;
            }
            CogValue candidate = h_.level().cogs[static_cast<std::size_t>(cg)].values[static_cast<std::size_t>(n)];
            if ( !candidate.parseJed(h_.level(), text(val)) )
            {
                return false;
            }
            Level& level = h_.context.mutableLevel();
            level.cogs[static_cast<std::size_t>(cg)].values[static_cast<std::size_t>(n)] = std::move(candidate);
            return true;
        });
    }

    int SED_JED_CALL AddCOGValue(int cg, char* name, char* val, int vtype) override
    {
        return h_.guard("AddCOGValue", -1, [&]
        {
            if ( !h_.checkCog("AddCOGValue", cg) )
            {
                return -1;
            }
            const std::optional<CogValue> value = makeCogValue("AddCOGValue", name, val, vtype);
            if ( !value )
            {
                return -1;
            }
            auto& values = h_.context.mutableLevel().cogs[static_cast<std::size_t>(cg)].values;
            values.push_back(*value);
            return static_cast<int>(values.size() - 1);
        });
    }

    JedResult SED_JED_CALL InsertCOGValue(int cg, int n, char* name, char* val, int vtype) override
    {
        return h_.guard("InsertCOGValue", kFail, [&]
        {
            if ( !h_.checkCog("InsertCOGValue", cg) )
            {
                return kInvalidArgument;
            }
            if ( n < 0 || static_cast<std::size_t>(n) > h_.level().cogs[static_cast<std::size_t>(cg)].values.size() )
            {
                h_.invalid("InsertCOGValue", "invalid COG value " + std::to_string(n));
                return kInvalidArgument;
            }
            const std::optional<CogValue> value = makeCogValue("InsertCOGValue", name, val, vtype);
            if ( !value )
            {
                return kInvalidArgument;
            }
            auto& values = h_.context.mutableLevel().cogs[static_cast<std::size_t>(cg)].values;
            values.insert(values.begin() + n, *value);
            return kOk;
        });
    }

    JedResult SED_JED_CALL DeleteCOGValue(int cg, int n) override
    {
        return h_.guard("DeleteCOGValue", kFail, [&]
        {
            if ( !h_.checkCogValue("DeleteCOGValue", cg, n) )
            {
                return kInvalidArgument;
            }
            auto& values = h_.context.mutableLevel().cogs[static_cast<std::size_t>(cg)].values;
            values.erase(values.begin() + n);
            return kOk;
        });
    }

private:
    int SurfaceInsertVertexAt(const char* method, int sc, int sf, int at, int secvx)
    {
        return h_.guard(method, -1, [&]
        {
            if ( !h_.checkSurface(method, sc, sf) || !h_.checkSectorVertex(method, sc, secvx) )
            {
                return -1;
            }
            const Surface& current = h_.level().sectors[static_cast<std::size_t>(sc)].surfaces[static_cast<std::size_t>(sf)];
            const int count = static_cast<int>(current.vertices.size());
            if ( current.texVertices.size() != current.vertices.size() || at > count || at < -1 )
            {
                h_.invalid(method, "invalid surface vertex position " + std::to_string(at));
                return -1;
            }
            const int index = at < 0 ? count : at;
            Surface& surface = h_.mutableSurface(sc, sf);
            surface.vertices.insert(surface.vertices.begin() + index, secvx);
            surface.texVertices.insert(surface.texVertices.begin() + index, TexVertex{});
            return index;
        });
    }

    std::optional<CogValue> makeCogValue(const char* method, char* name, char* val, int vtype)
    {
        if ( vtype < static_cast<int>(CogType::unk) || vtype > static_cast<int>(CogType::vect) )
        {
            h_.invalid(method, "invalid COG value type " + std::to_string(vtype));
            return std::nullopt;
        }
        CogValue value{};
        value.name = text(name);
        value.cogType = static_cast<CogType>(vtype);
        value.type = valueTypeForCogType(value.cogType);
        if ( !value.parseJed(h_.level(), text(val)) )
        {
            h_.invalid(method, "a value that does not parse as its COG type");
            return std::nullopt;
        }
        return value;
    }

    Impl& h_;
};

class JedPluginHost::Impl::JedApi final : public IJED
{
public:
    explicit JedApi(Impl& impl) : h_(impl)
    {
    }

    JedResult SED_JED_CALL QueryInterface(const void*, void** object) override
    {
        if ( object != nullptr )
        {
            *object = nullptr;
        }
        return kClassNotAvailable;
    }

    JedLong SED_JED_CALL AddRef() override
    {
        return 1;
    }

    JedLong SED_JED_CALL Release() override
    {
        return 0;
    }

    double SED_JED_CALL GetVersion() override
    {
        return kReportedVersion;
    }

    IJEDLevel* SED_JED_CALL GetLevel() override
    {
        return h_.levelApi.get();
    }

    int SED_JED_CALL GetMapMode() override
    {
        return h_.guard("GetMapMode", MM_SC, [&] { return static_cast<int>(h_.context.mapMode()); });
    }

    JedResult SED_JED_CALL SetMapMode(int mode) override
    {
        return h_.guard("SetMapMode", kFail, [&]
        {
            if ( mode < MM_SC || mode > MM_FR )
            {
                h_.invalid("SetMapMode", "invalid map mode " + std::to_string(mode));
                return kInvalidArgument;
            }
            h_.context.setMapMode(mode);
            return kOk;
        });
    }

    int SED_JED_CALL GetCurSC() override
    {
        return h_.guard("GetCurSC", -1, [&] { return static_cast<int>(h_.context.cursor().sector); });
    }

    JedResult SED_JED_CALL SetCurSC(int sc) override
    {
        return setCursor("SetCurSC", [&](JedCursor& c)
        {
            if ( !h_.checkSector("SetCurSC", sc) )
            {
                return false;
            }
            c.sector = sc;
            return true;
        });
    }

    int SED_JED_CALL GetCurTH() override
    {
        return h_.guard("GetCurTH", -1, [&] { return static_cast<int>(h_.context.cursor().thing); });
    }

    JedResult SED_JED_CALL SetCurTH(int th) override
    {
        return setCursor("SetCurTH", [&](JedCursor& c)
        {
            if ( !h_.checkThing("SetCurTH", th) )
            {
                return false;
            }
            c.thing = th;
            return true;
        });
    }

    int SED_JED_CALL GetCurLT() override
    {
        return h_.guard("GetCurLT", -1, [&] { return static_cast<int>(h_.context.cursor().light); });
    }

    JedResult SED_JED_CALL SetCurLT(int lt) override
    {
        return setCursor("SetCurLT", [&](JedCursor& c)
        {
            if ( !h_.checkLight("SetCurLT", lt) )
            {
                return false;
            }
            c.light = lt;
            return true;
        });
    }

    JedResult SED_JED_CALL GetCurVX(int* sc, int* vx) override
    {
        return h_.guard("GetCurVX", kFail, [&]
        {
            if ( !sc || !vx )
            {
                return kInvalidArgument;
            }
            const JedCursor c = h_.context.cursor();
            *sc = c.sector;
            *vx = c.vertex;
            return kOk;
        });
    }

    JedResult SED_JED_CALL SetCurVX(int sc, int vx) override
    {
        return setCursor("SetCurVX", [&](JedCursor& c)
        {
            if ( !h_.checkSectorVertex("SetCurVX", sc, vx) )
            {
                return false;
            }
            c.sector = sc;
            c.vertex = vx;
            return true;
        });
    }

    JedResult SED_JED_CALL GetCurSF(int* sc, int* sf) override
    {
        return h_.guard("GetCurSF", kFail, [&]
        {
            if ( !sc || !sf )
            {
                return kInvalidArgument;
            }
            const JedCursor c = h_.context.cursor();
            *sc = c.sector;
            *sf = c.surface;
            return kOk;
        });
    }

    JedResult SED_JED_CALL SetCurSF(int sc, int sf) override
    {
        return setCursor("SetCurSF", [&](JedCursor& c)
        {
            if ( !h_.checkSurface("SetCurSF", sc, sf) )
            {
                return false;
            }
            c.sector = sc;
            c.surface = sf;
            return true;
        });
    }

    JedResult SED_JED_CALL GetCurED(int* sc, int* sf, int* ed) override
    {
        return h_.guard("GetCurED", kFail, [&]
        {
            if ( !sc || !sf || !ed )
            {
                return kInvalidArgument;
            }
            const JedCursor c = h_.context.cursor();
            *sc = c.sector;
            *sf = c.surface;
            *ed = c.edge;
            return kOk;
        });
    }

    JedResult SED_JED_CALL SetCurED(int sc, int sf, int ed) override
    {
        return setCursor("SetCurED", [&](JedCursor& c)
        {
            if ( !h_.checkSurfaceVertex("SetCurED", sc, sf, ed) )
            {
                return false;
            }
            c.sector = sc;
            c.surface = sf;
            c.edge = ed;
            return true;
        });
    }

    JedResult SED_JED_CALL GetCurFR(int* th, int* fr) override
    {
        return h_.guard("GetCurFR", kFail, [&]
        {
            if ( !th || !fr )
            {
                return kInvalidArgument;
            }
            const JedCursor c = h_.context.cursor();
            *th = c.thing;
            *fr = c.frame;
            return kOk;
        });
    }

    JedResult SED_JED_CALL SetCurFR(int th, int fr) override
    {
        return setCursor("SetCurFR", [&](JedCursor& c)
        {
            if ( !h_.checkThingValue("SetCurFR", th, fr) )
            {
                return false;
            }
            c.thing = th;
            c.frame = fr;
            return true;
        });
    }

    JedResult SED_JED_CALL NewLevel(bool) override
    {
        return notImplemented("NewLevel");
    }

    JedResult SED_JED_CALL LoadLevel(char*) override
    {
        return notImplemented("LoadLevel");
    }

    JedResult SED_JED_CALL FindBBox(int sec, JedBox* box) override
    {
        return h_.guard("FindBBox", kFail, [&]
        {
            if ( !h_.checkSector("FindBBox", sec) || !h_.checkPointer("FindBBox", box) )
            {
                return kInvalidArgument;
            }
            *box = fromBox(level::sectorCalcBBox(sectorAt(sec)));
            return kOk;
        });
    }

    JedResult SED_JED_CALL FindBoundingSphere(int sec, double* cX, double* cY, double* cZ, double* radius) override
    {
        return h_.guard("FindBoundingSphere", kFail, [&]
        {
            if ( !h_.checkSector("FindBoundingSphere", sec) || !cX || !cY || !cZ || !radius )
            {
                return kInvalidArgument;
            }
            const level::BoundingSphere sphere = level::sectorCalcBoundingSphere(sectorAt(sec));
            *cX = sphere.center.x;
            *cY = sphere.center.y;
            *cZ = sphere.center.z;
            *radius = sphere.radius;
            return kOk;
        });
    }

    bool SED_JED_CALL FindCollideBox(int sec, JedBox* bbox, double cx, double cy, double cz, JedBox* cbox) override
    {
        return h_.guard("FindCollideBox", false, [&]
        {
            if ( !h_.checkSector("FindCollideBox", sec) || !bbox || !cbox )
            {
                return false;
            }
            TBox collide{};
            const bool found = level::sectorFindCollideBox(sectorAt(sec), toBox(*bbox), cx, cy, cz, collide);
            *cbox = fromBox(collide);
            return found;
        });
    }

    JedResult SED_JED_CALL FindSurfaceCenter(int sc, int sf, double* cx, double* cy, double* cz) override
    {
        return h_.guard("FindSurfaceCenter", kFail, [&]
        {
            if ( !h_.checkSurface("FindSurfaceCenter", sc, sf) || !cx || !cy || !cz )
            {
                return kInvalidArgument;
            }
            TVector center{};
            if ( !level::surfaceCalcCenter(sectorAt(sc), surfaceAt(sc, sf), center) )
            {
                return kFail;
            }
            *cx = center.x;
            *cy = center.y;
            *cz = center.z;
            return kOk;
        });
    }

    JedResult SED_JED_CALL RotateVector(JedVector* vec, double pch, double yaw, double rol) override
    {
        return h_.guard("RotateVector", kFail, [&]
        {
            if ( !vec )
            {
                return kInvalidArgument;
            }
            TVector v = toVector(*vec);
            rotateVector(v, pch, yaw, rol);
            *vec = fromVector(v);
            return kOk;
        });
    }

    JedResult SED_JED_CALL RotatePoint(double ax1, double ay1, double az1, double ax2, double ay2, double az2, double angle, double* x, double* y, double* z) override
    {
        return h_.guard("RotatePoint", kFail, [&]
        {
            if ( !x || !y || !z )
            {
                return kInvalidArgument;
            }
            rotatePoint(ax1, ay1, az1, ax2, ay2, az2, angle, *x, *y, *z);
            return kOk;
        });
    }

    JedResult SED_JED_CALL GetJKPYR(JedVector* x, JedVector* y, JedVector* z, double* pch, double* yaw, double* rol) override
    {
        return h_.guard("GetJKPYR", kFail, [&]
        {
            if ( !x || !y || !z || !pch || !yaw || !rol )
            {
                return kInvalidArgument;
            }
            getJKPYR(toVector(*x), toVector(*y), toVector(*z), *pch, *yaw, *rol);
            return kOk;
        });
    }

    bool SED_JED_CALL IsSurfaceConvex(int sc, int sf) override
    {
        return h_.guard("IsSurfaceConvex", false, [&]
        {
            return h_.checkSurface("IsSurfaceConvex", sc, sf) && level::surfaceIsConvex(sectorAt(sc), surfaceAt(sc, sf));
        });
    }

    bool SED_JED_CALL IsSurfacePlanar(int sc, int sf) override
    {
        return h_.guard("IsSurfacePlanar", false, [&]
        {
            return h_.checkSurface("IsSurfacePlanar", sc, sf) && level::surfaceIsPlanar(sectorAt(sc), surfaceAt(sc, sf));
        });
    }

    bool SED_JED_CALL IsSectorConvex(int sec) override
    {
        return h_.guard("IsSectorConvex", false, [&]
        {
            return h_.checkSector("IsSectorConvex", sec) && level::sectorIsConvex(sectorAt(sec));
        });
    }

    bool SED_JED_CALL IsInSector(int sec, double x, double y, double z) override
    {
        return h_.guard("IsInSector", false, [&]
        {
            return h_.checkSector("IsInSector", sec) && level::sectorContainsPoint(sectorAt(sec), x, y, z);
        });
    }

    bool SED_JED_CALL DoSectorsOverlap(int sec1, int sec2) override
    {
        return h_.guard("DoSectorsOverlap", false, [&]
        {
            return h_.checkSector("DoSectorsOverlap", sec1) && h_.checkSector("DoSectorsOverlap", sec2)
                && level::sectorDoOverlap(sectorAt(sec1), sectorAt(sec2));
        });
    }

    bool SED_JED_CALL IsPointOnSurface(int sc, int sf, double x, double y, double z) override
    {
        return h_.guard("IsPointOnSurface", false, [&]
        {
            return h_.checkSurface("IsPointOnSurface", sc, sf) && level::surfaceIsPointOn(sectorAt(sc), surfaceAt(sc, sf), x, y, z);
        });
    }

    int SED_JED_CALL FindSectorForThing(int th) override
    {
        return h_.guard("FindSectorForThing", -1, [&]
        {
            return h_.checkThing("FindSectorForThing", th)
                ? level::levelFindSectorForThing(h_.level(), h_.level().things[static_cast<std::size_t>(th)])
                : -1;
        });
    }

    int SED_JED_CALL FindSectorForXYZ(double x, double y, double z) override
    {
        return h_.guard("FindSectorForXYZ", -1, [&] { return level::levelFindSectorForXYZ(h_.level(), x, y, z); });
    }

    int SED_JED_CALL ExtrudeSurface(int sc, int sf, double by) override
    {
        return h_.guard("ExtrudeSurface", -1, [&]
        {
            if ( !h_.checkSurface("ExtrudeSurface", sc, sf) )
            {
                return -1;
            }
            Level& level = h_.context.mutableLevel();
            return level::levelExtrudeSurface(level, SurfaceRef{ sc, sf }, by) ? static_cast<int>(level.sectors.size() - 1) : -1;
        });
    }

    int SED_JED_CALL CleaveSurface(int sc, int sf, JedVector* cnormal, double cx, double cy, double cz) override
    {
        return h_.guard("CleaveSurface", -1, [&]
        {
            if ( !h_.checkSurface("CleaveSurface", sc, sf) || !h_.checkPointer("CleaveSurface", cnormal) )
            {
                return -1;
            }
            Level& level = h_.context.mutableLevel();
            if ( !level::levelCleaveSurface(level, SurfaceRef{ sc, sf }, toVector(*cnormal), cx, cy, cz) )
            {
                return -1;
            }
            return static_cast<int>(level.sectors[static_cast<std::size_t>(sc)].surfaces.size() - 1);
        });
    }

    int SED_JED_CALL CleaveSector(int sec, JedVector* cnormal, double cx, double cy, double cz) override
    {
        return h_.guard("CleaveSector", -1, [&]
        {
            if ( !h_.checkSector("CleaveSector", sec) || !h_.checkPointer("CleaveSector", cnormal) )
            {
                return -1;
            }
            Level& level = h_.context.mutableLevel();
            if ( !level::levelCleaveSector(level, sec, toVector(*cnormal), cx, cy, cz) )
            {
                return -1;
            }
            return static_cast<int>(level.sectors.size() - 1);
        });
    }

    bool SED_JED_CALL CleaveEdge(int sc, int sf, int ed, JedVector* cnormal, double cx, double cy, double cz) override
    {
        return h_.guard("CleaveEdge", false, [&]
        {
            if ( !h_.checkSurfaceVertex("CleaveEdge", sc, sf, ed) || !h_.checkPointer("CleaveEdge", cnormal) )
            {
                return false;
            }
            return level::levelCleaveEdge(h_.context.mutableLevel(), SurfaceRef{ sc, sf }, ed, toVector(*cnormal), cx, cy, cz);
        });
    }

    bool SED_JED_CALL JoinSurfaces(int sc1, int sf1, int sc2, int sf2) override
    {
        return h_.guard("JoinSurfaces", false, [&]
        {
            return h_.checkSurface("JoinSurfaces", sc1, sf1) && h_.checkSurface("JoinSurfaces", sc2, sf2)
                && level::levelConnectSurfaces(h_.context.mutableLevel(), SurfaceRef{ sc1, sf1 }, SurfaceRef{ sc2, sf2 });
        });
    }

    bool SED_JED_CALL PlanarizeSurface(int sc, int sf) override
    {
        return h_.guard("PlanarizeSurface", false, [&]
        {
            return h_.checkSurface("PlanarizeSurface", sc, sf)
                && level::levelFlattenSurface(h_.context.mutableLevel(), SurfaceRef{ sc, sf });
        });
    }

    int SED_JED_CALL MergeSurfaces(int sc, int sf1, int sf2) override
    {
        return h_.guard("MergeSurfaces", -1, [&]
        {
            if ( !h_.checkSurface("MergeSurfaces", sc, sf1) || !h_.checkSurface("MergeSurfaces", sc, sf2) )
            {
                return -1;
            }
            const std::optional<SurfaceRef> merged = level::levelMergeSurfaces(h_.context.mutableLevel(), SurfaceRef{ sc, sf1 }, SurfaceRef{ sc, sf2 });
            return merged ? static_cast<int>(merged->surface) : -1;
        });
    }

    int SED_JED_CALL MergeSectors(int sec1, int sec2) override
    {
        return h_.guard("MergeSectors", -1, [&]
        {
            if ( !h_.checkSector("MergeSectors", sec1) || !h_.checkSector("MergeSectors", sec2) )
            {
                return -1;
            }
            const std::optional<std::int32_t> merged = level::levelMergeSectors(h_.context.mutableLevel(), sec1, sec2);
            return merged ? static_cast<int>(*merged) : -1;
        });
    }

    JedResult SED_JED_CALL CalculateDefaultUVNormals(int sc, int sf, int orgvx, JedVector* un, JedVector* vn) override
    {
        return h_.guard("CalculateDefaultUVNormals", kFail, [&]
        {
            if ( !h_.checkSurfaceVertex("CalculateDefaultUVNormals", sc, sf, orgvx) || !un || !vn )
            {
                return kInvalidArgument;
            }
            TVector u{};
            TVector v{};
            level::surfaceCalcDefaultUVNormals(sectorAt(sc), surfaceAt(sc, sf), orgvx, u, v);
            *un = fromVector(u);
            *vn = fromVector(v);
            return kOk;
        });
    }

    JedResult SED_JED_CALL CalcUVNormals(int sc, int sf, JedVector* un, JedVector* vn) override
    {
        return h_.guard("CalcUVNormals", kFail, [&]
        {
            if ( !h_.checkSurface("CalcUVNormals", sc, sf) || !un || !vn )
            {
                return kInvalidArgument;
            }
            TVector u{};
            TVector v{};
            if ( !level::surfaceCalcUVNormals(sectorAt(sc), surfaceAt(sc, sf), u, v) )
            {
                return kFail;
            }
            *un = fromVector(u);
            *vn = fromVector(v);
            return kOk;
        });
    }

    JedResult SED_JED_CALL ArrangeTexture(int sc, int sf, int orgvx, JedVector* un, JedVector* vn) override
    {
        return h_.guard("ArrangeTexture", kFail, [&]
        {
            if ( !h_.checkSurfaceVertex("ArrangeTexture", sc, sf, orgvx) || !un || !vn )
            {
                return kInvalidArgument;
            }
            Level& level = h_.context.mutableLevel();
            Sector& sector = level.sectors[static_cast<std::size_t>(sc)];
            level::surfaceArrangeTexture(level::levelPixelPerUnit(level), sector, sector.surfaces[static_cast<std::size_t>(sf)],
                orgvx, toVector(*un), toVector(*vn));
            return kOk;
        });
    }

    JedResult SED_JED_CALL ArrangeTextureBy(int sc, int sf, JedVector* un, JedVector* vn, double refx, double refy, double refz, double refu, double refv) override
    {
        return h_.guard("ArrangeTextureBy", kFail, [&]
        {
            if ( !h_.checkSurface("ArrangeTextureBy", sc, sf) || !un || !vn )
            {
                return kInvalidArgument;
            }
            Level& level = h_.context.mutableLevel();
            Sector& sector = level.sectors[static_cast<std::size_t>(sc)];
            level::surfaceArrangeTextureBy(level::levelPixelPerUnit(level), sector, sector.surfaces[static_cast<std::size_t>(sf)],
                toVector(*un), toVector(*vn), refx, refy, refz, static_cast<float>(refu), static_cast<float>(refv));
            return kOk;
        });
    }

    bool SED_JED_CALL IsTextureFlipped(int sc, int sf) override
    {
        return h_.guard("IsTextureFlipped", false, [&]
        {
            return h_.checkSurface("IsTextureFlipped", sc, sf) && level::surfaceIsTextureFlipped(sectorAt(sc), surfaceAt(sc, sf));
        });
    }

    JedResult SED_JED_CALL RemoveSurfaceReferences(int, int) override
    {
        return notImplemented("RemoveSurfaceReferences");
    }

    JedResult SED_JED_CALL RemoveSectorReferences(int, bool) override
    {
        return notImplemented("RemoveSectorReferences");
    }

    bool SED_JED_CALL StitchSurfaces(int sc1, int sf1, int sc2, int sf2) override
    {
        return h_.guard("StitchSurfaces", false, [&]
        {
            return h_.checkSurface("StitchSurfaces", sc1, sf1) && h_.checkSurface("StitchSurfaces", sc2, sf2)
                && level::levelStitchSurfaces(h_.context.mutableLevel(), SurfaceRef{ sc1, sf1 }, SurfaceRef{ sc2, sf2 });
        });
    }

    bool SED_JED_CALL FindCommonEdges(int, int, int, int, int*, int*, int*, int*) override
    {
        h_.unsupported("FindCommonEdges");
        return false;
    }

    bool SED_JED_CALL DoSurfacesOverlap(int sc1, int sf1, int sc2, int sf2) override
    {
        return h_.guard("DoSurfacesOverlap", false, [&]
        {
            return h_.checkSurface("DoSurfacesOverlap", sc1, sf1) && h_.checkSurface("DoSurfacesOverlap", sc2, sf2)
                && level::surfaceDoOverlap(sectorAt(sc1), surfaceAt(sc1, sf1), sectorAt(sc2), surfaceAt(sc2, sf2));
        });
    }

    bool SED_JED_CALL MakeAdjoin(int sc, int sf) override
    {
        return h_.guard("MakeAdjoin", false, [&]
        {
            return h_.checkSurface("MakeAdjoin", sc, sf) && level::levelMakeAdjoin(h_.context.mutableLevel(), SurfaceRef{ sc, sf });
        });
    }

    bool SED_JED_CALL MakeAdjoinFromSectorUp(int sc, int sf, int firstsc) override
    {
        return h_.guard("MakeAdjoinFromSectorUp", false, [&]
        {
            return h_.checkSurface("MakeAdjoinFromSectorUp", sc, sf)
                && level::levelMakeAdjoinFromSector(h_.context.mutableLevel(), SurfaceRef{ sc, sf }, firstsc);
        });
    }

    bool SED_JED_CALL UnAdjoin(int sc, int sf) override
    {
        return h_.guard("UnAdjoin", false, [&]
        {
            return h_.checkSurface("UnAdjoin", sc, sf) && level::levelUnadjoinSurface(h_.context.mutableLevel(), SurfaceRef{ sc, sf });
        });
    }

    int SED_JED_CALL CreateCubicSector(double x, double y, double z, JedVector* pnormal, JedVector* edge) override
    {
        return h_.guard("CreateCubicSector", -1, [&]
        {
            if ( !pnormal || !edge )
            {
                return -1;
            }
            Level& level = h_.context.mutableLevel();
            level::levelCreateCube(level, x, y, z, toVector(*pnormal), toVector(*edge));
            return static_cast<int>(level.sectors.size() - 1);
        });
    }

    JedResult SED_JED_CALL StartUndo(char* name) override
    {
        return h_.guard("StartUndo", kFail, [&]
        {
            h_.context.startUndo(text(name));
            return kOk;
        });
    }

    // SED records the whole plug-in edit as one transaction, so per-item
    // undo snapshots are accepted and need no separate bookkeeping.
    JedResult SED_JED_CALL SaveUndoForThing(int, int) override
    {
        return kOk;
    }

    JedResult SED_JED_CALL SaveUndoForLight(int, int) override
    {
        return kOk;
    }

    JedResult SED_JED_CALL SaveUndoForSector(int, int, int) override
    {
        return kOk;
    }

    JedResult SED_JED_CALL ClearUndoBuffer() override
    {
        return notImplemented("ClearUndoBuffer");
    }

    JedResult SED_JED_CALL ApplyUndo() override
    {
        return notImplemented("ApplyUndo");
    }

    int SED_JED_CALL GetApplicationHandle() override
    {
        return h_.guard("GetApplicationHandle", 0, [&] { return static_cast<int>(h_.context.windowHandle(jw_Main)); });
    }

    bool SED_JED_CALL JoinSectors(int sec1, int sec2) override
    {
        return h_.guard("JoinSectors", false, [&]
        {
            return h_.checkSector("JoinSectors", sec1) && h_.checkSector("JoinSectors", sec2)
                && level::levelConnectSectors(h_.context.mutableLevel(), sec1, sec2);
        });
    }

    int SED_JED_CALL GetNMultiselected(int what) override
    {
        return h_.guard("GetNMultiselected", 0, [&]
        {
            const editor::Selection& s = h_.context.selection();
            switch ( what )
            {
            case MM_SC:
                return static_cast<int>(s.sectors.size());
            case MM_SF:
                return static_cast<int>(s.surfaces.size());
            case MM_VX:
                return static_cast<int>(h_.sortedVertices().size());
            case MM_TH:
                return static_cast<int>(s.things.size());
            case MM_LT:
                return static_cast<int>(s.lights.size());
            case MM_FR:
                return static_cast<int>(s.frames.size());
            default:
                return 0; // SED has no edge multiselection.
            }
        });
    }

    JedResult SED_JED_CALL ClearMultiselection(int what) override
    {
        return h_.guard("ClearMultiselection", kFail, [&]
        {
            editor::Selection& s = h_.context.mutableSelection();
            switch ( what )
            {
            case MM_SC:
                s.sectors.clear();
                break;
            case MM_SF:
                s.surfaces.clear();
                break;
            case MM_VX:
                s.surfaceVertices.clear();
                break;
            case MM_TH:
                s.things.clear();
                break;
            case MM_LT:
                s.lights.clear();
                break;
            case MM_FR:
                s.frames.clear();
                break;
            default:
                break;
            }
            return kOk;
        });
    }

    JedResult SED_JED_CALL RemoveFromMultiselection(int what, int n) override
    {
        return h_.guard("RemoveFromMultiselection", kFail, [&]
        {
            const auto bad = [&]
            {
                h_.invalid("RemoveFromMultiselection", "invalid selection index " + std::to_string(n));
                return kInvalidArgument;
            };
            switch ( what )
            {
            case MM_SC:
            {
                const auto list = h_.sortedSectors();
                if ( n < 0 || static_cast<std::size_t>(n) >= list.size() )
                {
                    return bad();
                }
                h_.context.mutableSelection().sectors.erase(list[static_cast<std::size_t>(n)]);
                return kOk;
            }
            case MM_SF:
            {
                const auto list = h_.sortedSurfaces();
                if ( n < 0 || static_cast<std::size_t>(n) >= list.size() )
                {
                    return bad();
                }
                const auto& [sc, sf] = list[static_cast<std::size_t>(n)];
                h_.context.mutableSelection().surfaces.erase(SurfaceRef{ sc, sf });
                return kOk;
            }
            case MM_VX:
            {
                const auto list = h_.sortedVertices();
                if ( n < 0 || static_cast<std::size_t>(n) >= list.size() )
                {
                    return bad();
                }
                const auto [sc, vx] = list[static_cast<std::size_t>(n)];
                const Level& level = h_.level();
                auto& corners = h_.context.mutableSelection().surfaceVertices;
                std::erase_if(corners, [&](const editor::SurfaceVertexRef& ref)
                {
                    if ( ref.surface.sector != sc )
                    {
                        return false;
                    }
                    const auto& surfaces = level.sectors[static_cast<std::size_t>(sc)].surfaces;
                    if ( ref.surface.surface < 0 || static_cast<std::size_t>(ref.surface.surface) >= surfaces.size() )
                    {
                        return false;
                    }
                    const auto& vertices = surfaces[static_cast<std::size_t>(ref.surface.surface)].vertices;
                    return ref.localVertex >= 0 && static_cast<std::size_t>(ref.localVertex) < vertices.size()
                        && vertices[static_cast<std::size_t>(ref.localVertex)] == vx;
                });
                return kOk;
            }
            case MM_TH:
            {
                const auto list = h_.sortedThings();
                if ( n < 0 || static_cast<std::size_t>(n) >= list.size() )
                {
                    return bad();
                }
                h_.context.mutableSelection().things.erase(list[static_cast<std::size_t>(n)]);
                return kOk;
            }
            case MM_LT:
            {
                const auto list = h_.sortedLights();
                if ( n < 0 || static_cast<std::size_t>(n) >= list.size() )
                {
                    return bad();
                }
                h_.context.mutableSelection().lights.erase(list[static_cast<std::size_t>(n)]);
                return kOk;
            }
            case MM_FR:
            {
                const auto list = h_.sortedFrames();
                if ( n < 0 || static_cast<std::size_t>(n) >= list.size() )
                {
                    return bad();
                }
                const auto& [th, fr] = list[static_cast<std::size_t>(n)];
                h_.context.mutableSelection().frames.erase(level::FrameRef{ th, fr });
                return kOk;
            }
            default:
                return bad();
            }
        });
    }

    int SED_JED_CALL GetSelectedSC(int n) override
    {
        return h_.guard("GetSelectedSC", -1, [&] { return itemAt("GetSelectedSC", h_.sortedSectors(), n); });
    }

    int SED_JED_CALL GetSelectedTH(int n) override
    {
        return h_.guard("GetSelectedTH", -1, [&] { return itemAt("GetSelectedTH", h_.sortedThings(), n); });
    }

    int SED_JED_CALL GetSelectedLT(int n) override
    {
        return h_.guard("GetSelectedLT", -1, [&] { return itemAt("GetSelectedLT", h_.sortedLights(), n); });
    }

    JedResult SED_JED_CALL GetSelectedSF(int n, int* sc, int* sf) override
    {
        return h_.guard("GetSelectedSF", kFail, [&] { return pairAt("GetSelectedSF", h_.sortedSurfaces(), n, sc, sf); });
    }

    JedResult SED_JED_CALL GetSelectedED(int, int* sc, int* sf, int* ed) override
    {
        if ( sc && sf && ed )
        {
            *sc = *sf = *ed = -1;
        }
        h_.unsupported("GetSelectedED");
        return kNotImplemented;
    }

    JedResult SED_JED_CALL GetSelectedVX(int n, int* sc, int* vx) override
    {
        return h_.guard("GetSelectedVX", kFail, [&] { return pairAt("GetSelectedVX", h_.sortedVertices(), n, sc, vx); });
    }

    JedResult SED_JED_CALL GetSelectedFR(int n, int* th, int* fr) override
    {
        return h_.guard("GetSelectedFR", kFail, [&] { return pairAt("GetSelectedFR", h_.sortedFrames(), n, th, fr); });
    }

    int SED_JED_CALL SelectSC(int sc) override
    {
        return h_.guard("SelectSC", -1, [&]
        {
            if ( !h_.checkSector("SelectSC", sc) )
            {
                return -1;
            }
            h_.context.mutableSelection().sectors.insert(sc);
            return Impl::indexOf(h_.sortedSectors(), sc);
        });
    }

    int SED_JED_CALL SelectSF(int sc, int sf) override
    {
        return h_.guard("SelectSF", -1, [&]
        {
            if ( !h_.checkSurface("SelectSF", sc, sf) )
            {
                return -1;
            }
            h_.context.mutableSelection().surfaces.insert(SurfaceRef{ sc, sf });
            return Impl::indexOf(h_.sortedSurfaces(), std::pair<std::int32_t, std::int32_t>{ sc, sf });
        });
    }

    int SED_JED_CALL SelectED(int, int, int) override
    {
        h_.unsupported("SelectED");
        return -1;
    }

    int SED_JED_CALL SelectVX(int sc, int vx) override
    {
        return h_.guard("SelectVX", -1, [&]
        {
            if ( !h_.checkSectorVertex("SelectVX", sc, vx) )
            {
                return -1;
            }
            const auto& surfaces = h_.level().sectors[static_cast<std::size_t>(sc)].surfaces;
            for ( std::size_t sf = 0; sf < surfaces.size(); ++sf )
            {
                const auto& vertices = surfaces[sf].vertices;
                const auto it = std::find(vertices.begin(), vertices.end(), vx);
                if ( it != vertices.end() )
                {
                    h_.context.mutableSelection().surfaceVertices.insert(editor::SurfaceVertexRef{
                        SurfaceRef{ sc, static_cast<std::int32_t>(sf) }, static_cast<std::int32_t>(it - vertices.begin()) });
                    return Impl::indexOf(h_.sortedVertices(), std::pair<std::int32_t, std::int32_t>{ sc, vx });
                }
            }
            h_.invalid("SelectVX", "a vertex that no surface uses");
            return -1;
        });
    }

    int SED_JED_CALL SelectTH(int th) override
    {
        return h_.guard("SelectTH", -1, [&]
        {
            if ( !h_.checkThing("SelectTH", th) )
            {
                return -1;
            }
            h_.context.mutableSelection().things.insert(th);
            return Impl::indexOf(h_.sortedThings(), th);
        });
    }

    int SED_JED_CALL SelectFR(int th, int fr) override
    {
        return h_.guard("SelectFR", -1, [&]
        {
            if ( !h_.checkThingValue("SelectFR", th, fr) )
            {
                return -1;
            }
            h_.context.mutableSelection().frames.insert(level::FrameRef{ th, fr });
            return Impl::indexOf(h_.sortedFrames(), std::pair<std::int32_t, std::int32_t>{ th, fr });
        });
    }

    int SED_JED_CALL SelectLT(int lt) override
    {
        return h_.guard("SelectLT", -1, [&]
        {
            if ( !h_.checkLight("SelectLT", lt) )
            {
                return -1;
            }
            h_.context.mutableSelection().lights.insert(lt);
            return Impl::indexOf(h_.sortedLights(), lt);
        });
    }

    int SED_JED_CALL FindSelectedSC(int sc) override
    {
        return h_.guard("FindSelectedSC", -1, [&] { return Impl::indexOf(h_.sortedSectors(), sc); });
    }

    int SED_JED_CALL FindSelectedSF(int sc, int sf) override
    {
        return h_.guard("FindSelectedSF", -1, [&]
        {
            return Impl::indexOf(h_.sortedSurfaces(), std::pair<std::int32_t, std::int32_t>{ sc, sf });
        });
    }

    int SED_JED_CALL FindSelectedED(int, int, int) override
    {
        return -1;
    }

    int SED_JED_CALL FindSelectedVX(int sc, int vx) override
    {
        return h_.guard("FindSelectedVX", -1, [&]
        {
            return Impl::indexOf(h_.sortedVertices(), std::pair<std::int32_t, std::int32_t>{ sc, vx });
        });
    }

    int SED_JED_CALL FindSelectedTH(int th) override
    {
        return h_.guard("FindSelectedTH", -1, [&] { return Impl::indexOf(h_.sortedThings(), th); });
    }

    int SED_JED_CALL FindSelectedFR(int th, int fr) override
    {
        return h_.guard("FindSelectedFR", -1, [&]
        {
            return Impl::indexOf(h_.sortedFrames(), std::pair<std::int32_t, std::int32_t>{ th, fr });
        });
    }

    int SED_JED_CALL FindSelectedLT(int lt) override
    {
        return h_.guard("FindSelectedLT", -1, [&] { return Impl::indexOf(h_.sortedLights(), lt); });
    }

    JedResult SED_JED_CALL SaveJED(char*) override
    {
        return notImplemented("SaveJED");
    }

    JedResult SED_JED_CALL SaveJKL(char*) override
    {
        return notImplemented("SaveJKL");
    }

    JedResult SED_JED_CALL UpdateMap() override
    {
        return h_.guard("UpdateMap", kFail, [&]
        {
            h_.context.updateMap();
            return kOk;
        });
    }

    JedResult SED_JED_CALL SetPickerCMP(char*) override
    {
        return notImplemented("SetPickerCMP");
    }

    char* SED_JED_CALL PickResource(int, char* cur) override
    {
        h_.unsupported("PickResource");
        return h_.guard<char*>("PickResource", nullptr, [&] { return h_.cache(text(cur)); });
    }

    JedLong SED_JED_CALL EditFlags(int, JedLong flags) override
    {
        h_.unsupported("EditFlags");
        return flags;
    }

    JedResult SED_JED_CALL ReloadTemplates() override
    {
        return notImplemented("ReloadTemplates");
    }

    JedResult SED_JED_CALL PanMessage(int mtype, char* msg) override
    {
        return h_.guard("PanMessage", kFail, [&]
        {
            JedMessageType type = JedMessageType::Info;
            if ( mtype == msg_warning )
            {
                type = JedMessageType::Warning;
            }
            else if ( mtype == msg_error )
            {
                type = JedMessageType::Error;
            }
            h_.context.message(type, text(msg));
            return kOk;
        });
    }

    JedResult SED_JED_CALL SendKey(int, int) override
    {
        return notImplemented("SendKey");
    }

    bool SED_JED_CALL ExecuteMenu(char*) override
    {
        h_.unsupported("ExecuteMenu");
        return false;
    }

    void* SED_JED_CALL GetJEDSetting(char*) override
    {
        h_.unsupported("GetJEDSetting");
        return nullptr;
    }

    bool SED_JED_CALL IsLayerVisible(int n) override
    {
        return h_.guard("IsLayerVisible", true, [&] { return h_.context.isLayerVisible(n); });
    }

    JedResult SED_JED_CALL CheckConsistencyErrors() override
    {
        return notImplemented("CheckConsistencyErrors");
    }

    JedResult SED_JED_CALL CheckResources() override
    {
        return notImplemented("CheckResources");
    }

    int SED_JED_CALL NConsistencyErrors() override
    {
        return 0;
    }

    char* SED_JED_CALL GetConsErrorString(int) override
    {
        return h_.guard<char*>("GetConsErrorString", nullptr, [&] { return h_.cache(std::string()); });
    }

    int SED_JED_CALL GetConsErrorType(int) override
    {
        return ce_none;
    }

    int SED_JED_CALL GetConsErrorSector(int) override
    {
        return -1;
    }

    int SED_JED_CALL GetConsErrorSurface(int) override
    {
        return -1;
    }

    int SED_JED_CALL GetConsErrorThing(int) override
    {
        return -1;
    }

    int SED_JED_CALL GetConsErrorCog(int) override
    {
        return -1;
    }

    bool SED_JED_CALL IsPreviewActive() override
    {
        return false;
    }

    char* SED_JED_CALL GetJEDString(int what) override
    {
        return h_.guard<char*>("GetJEDString", nullptr, [&] { return h_.cache(h_.context.jedString(what)); });
    }

    bool SED_JED_CALL GetIsMOTS() override
    {
        return h_.guard("GetIsMOTS", false, [&] { return h_.level().kind == ProjectType::MOTS; });
    }

    JedResult SED_JED_CALL SetIsMOTS(bool) override
    {
        return notImplemented("SetIsMOTS");
    }

    int SED_JED_CALL GetJedWindow(int whichone) override
    {
        // Windows window handles are 32-bit significant even in 64-bit processes.
        return h_.guard("GetJedWindow", 0, [&] { return static_cast<int>(h_.context.windowHandle(whichone)); });
    }

    char* SED_JED_CALL FileExtractExt(char* path) override
    {
        return h_.guard<char*>("FileExtractExt", nullptr, [&]
        {
            const std::string p = text(path);
            const std::size_t dot = p.find_last_of('.');
            const std::size_t sep = lastSeparator(p);
            return h_.cache(dot == std::string::npos || (sep != std::string::npos && dot < sep) ? std::string() : p.substr(dot));
        });
    }

    char* SED_JED_CALL FileExtractPath(char* path) override
    {
        return h_.guard<char*>("FileExtractPath", nullptr, [&]
        {
            const std::string p = text(path);
            const std::size_t sep = lastSeparator(p);
            return h_.cache(sep == std::string::npos ? std::string() : p.substr(0, sep + 1));
        });
    }

    char* SED_JED_CALL FileExtractName(char* path) override
    {
        return h_.guard<char*>("FileExtractName", nullptr, [&]
        {
            const std::string p = text(path);
            const std::size_t sep = lastSeparator(p);
            return h_.cache(sep == std::string::npos ? p : p.substr(sep + 1));
        });
    }

    char* SED_JED_CALL FindProjectDirFile(char*) override
    {
        h_.unsupported("FindProjectDirFile");
        return h_.guard<char*>("FindProjectDirFile", nullptr, [&] { return h_.cache(std::string()); });
    }

    bool SED_JED_CALL IsFileContainer(char* path) override
    {
        return h_.guard("IsFileContainer", false, [&]
        {
            std::string ext = text(path);
            const std::size_t dot = ext.find_last_of('.');
            ext = dot == std::string::npos ? std::string() : ext.substr(dot);
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return ext == ".gob" || ext == ".goo";
        });
    }

    bool SED_JED_CALL IsFileInContainer(char* path) override
    {
        return h_.guard("IsFileInContainer", false, [&] { return text(path).find('>') != std::string::npos; });
    }

    char* SED_JED_CALL FileOpenDialog(char*, char*) override
    {
        h_.unsupported("FileOpenDialog");
        return nullptr;
    }

    int SED_JED_CALL OpenFile(char* name) override
    {
        return openBinary("OpenFile", name, false);
    }

    int SED_JED_CALL OpenGameFile(char* name) override
    {
        return openBinary("OpenGameFile", name, true);
    }

    int SED_JED_CALL ReadFile(int handle, void* buf, JedLong size) override
    {
        return h_.guard("ReadFile", -1, [&]
        {
            OpenBinaryFile* file = binary("ReadFile", handle);
            if ( !file || size < 0 || (size > 0 && !buf) )
            {
                return -1;
            }
            const std::size_t available = file->bytes.size() - std::min(file->position, file->bytes.size());
            const std::size_t count = std::min(available, static_cast<std::size_t>(size));
            if ( count > 0 )
            {
                std::memcpy(buf, file->bytes.data() + file->position, count);
            }
            file->position += count;
            return static_cast<int>(count);
        });
    }

    JedResult SED_JED_CALL SetFilePos(int handle, JedLong pos) override
    {
        return h_.guard("SetFilePos", kFail, [&]
        {
            OpenBinaryFile* file = binary("SetFilePos", handle);
            if ( !file || pos < 0 || static_cast<std::size_t>(pos) > file->bytes.size() )
            {
                return kInvalidArgument;
            }
            file->position = static_cast<std::size_t>(pos);
            return kOk;
        });
    }

    JedLong SED_JED_CALL GetFilePos(int handle) override
    {
        return h_.guard("GetFilePos", static_cast<JedLong>(-1), [&]
        {
            const OpenBinaryFile* file = binary("GetFilePos", handle);
            return file ? static_cast<JedLong>(file->position) : static_cast<JedLong>(-1);
        });
    }

    JedLong SED_JED_CALL GetFileSize(int handle) override
    {
        return h_.guard("GetFileSize", static_cast<JedLong>(-1), [&]
        {
            const OpenBinaryFile* file = binary("GetFileSize", handle);
            return file ? static_cast<JedLong>(file->bytes.size()) : static_cast<JedLong>(-1);
        });
    }

    char* SED_JED_CALL GetFileName(int handle) override
    {
        return h_.guard<char*>("GetFileName", nullptr, [&]() -> char*
        {
            const OpenBinaryFile* file = binary("GetFileName", handle);
            return file ? h_.cache(file->name) : nullptr;
        });
    }

    JedResult SED_JED_CALL CloseFile(int handle) override
    {
        return h_.guard("CloseFile", kFail, [&]
        {
            if ( !binary("CloseFile", handle) )
            {
                return kInvalidArgument;
            }
            h_.files[static_cast<std::size_t>(handle)].reset();
            return kOk;
        });
    }

    int SED_JED_CALL OpenTextFile(char* name) override
    {
        return openText("OpenTextFile", name, false);
    }

    int SED_JED_CALL OpenGameTextFile(char* name) override
    {
        return openText("OpenGameTextFile", name, true);
    }

    char* SED_JED_CALL GetTextFileName(int handle) override
    {
        return h_.guard<char*>("GetTextFileName", nullptr, [&]() -> char*
        {
            const TextFileState* file = textFile("GetTextFileName", handle);
            return file ? h_.cache(file->name) : nullptr;
        });
    }

    char* SED_JED_CALL ReadTextFileString(int handle) override
    {
        return h_.guard<char*>("ReadTextFileString", nullptr, [&]() -> char*
        {
            TextFileState* file = textFile("ReadTextFileString", handle);
            if ( !file )
            {
                return nullptr;
            }
            if ( file->next >= file->lines.size() )
            {
                return h_.cache(std::string());
            }
            return h_.cache(file->lines[file->next++]);
        });
    }

    bool SED_JED_CALL TextFileEof(int handle) override
    {
        return h_.guard("TextFileEof", true, [&]
        {
            const TextFileState* file = textFile("TextFileEof", handle);
            return !file || file->next >= file->lines.size();
        });
    }

    int SED_JED_CALL TextFileCurrentLine(int handle) override
    {
        return h_.guard("TextFileCurrentLine", -1, [&]
        {
            const TextFileState* file = textFile("TextFileCurrentLine", handle);
            return file ? static_cast<int>(file->next) : -1;
        });
    }

    JedResult SED_JED_CALL CloseTextFile(int handle) override
    {
        return h_.guard("CloseTextFile", kFail, [&]
        {
            if ( !textFile("CloseTextFile", handle) )
            {
                return kInvalidArgument;
            }
            h_.textFiles[static_cast<std::size_t>(handle)].reset();
            return kOk;
        });
    }

    int SED_JED_CALL OpenGOB(char*) override
    {
        h_.unsupported("OpenGOB");
        return -1;
    }

    int SED_JED_CALL GOBNFiles(int) override
    {
        return 0;
    }

    char* SED_JED_CALL GOBNFileName(int, int) override
    {
        return nullptr;
    }

    char* SED_JED_CALL GOBNFullFileName(int, int) override
    {
        return nullptr;
    }

    JedLong SED_JED_CALL GOBGetFileSize(int, int) override
    {
        return -1;
    }

    JedLong SED_JED_CALL GOBGetFileOffset(int, int) override
    {
        return -1;
    }

    JedResult SED_JED_CALL CloseGOB(int) override
    {
        return kInvalidArgument;
    }

    IJEDWFRenderer* SED_JED_CALL CreateWFRenderer(int, int) override
    {
        h_.unsupported("CreateWFRenderer");
        return nullptr;
    }

private:
    const Sector& sectorAt(const int sc) const
    {
        return h_.level().sectors[static_cast<std::size_t>(sc)];
    }

    const Surface& surfaceAt(const int sc, const int sf) const
    {
        return sectorAt(sc).surfaces[static_cast<std::size_t>(sf)];
    }

    JedResult notImplemented(const char* method)
    {
        return h_.guard(method, kNotImplemented, [&]
        {
            h_.unsupported(method);
            return kNotImplemented;
        });
    }

    template <typename F>
    JedResult setCursor(const char* method, F&& update)
    {
        return h_.guard(method, kFail, [&]
        {
            JedCursor cursor = h_.context.cursor();
            if ( !update(cursor) )
            {
                return kInvalidArgument;
            }
            h_.context.setCursor(cursor);
            return kOk;
        });
    }

    int itemAt(const char* method, const std::vector<std::int32_t>& list, const int n)
    {
        if ( n < 0 || static_cast<std::size_t>(n) >= list.size() )
        {
            h_.invalid(method, "invalid selection index " + std::to_string(n));
            return -1;
        }
        return list[static_cast<std::size_t>(n)];
    }

    JedResult pairAt(const char* method, const std::vector<std::pair<std::int32_t, std::int32_t>>& list, const int n, int* first, int* second)
    {
        if ( !first || !second )
        {
            return kInvalidArgument;
        }
        if ( n < 0 || static_cast<std::size_t>(n) >= list.size() )
        {
            *first = -1;
            *second = -1;
            h_.invalid(method, "invalid selection index " + std::to_string(n));
            return kInvalidArgument;
        }
        *first = list[static_cast<std::size_t>(n)].first;
        *second = list[static_cast<std::size_t>(n)].second;
        return kOk;
    }

    int openBinary(const char* method, char* name, const bool gameFile)
    {
        return h_.guard(method, -1, [&]
        {
            const std::string fileName = text(name);
            std::optional<std::vector<std::uint8_t>> bytes = h_.context.readFile(fileName, gameFile);
            if ( !bytes )
            {
                return -1;
            }
            return allocateHandle(h_.files, OpenBinaryFile{ fileName, std::move(*bytes), 0 });
        });
    }

    int openText(const char* method, char* name, const bool gameFile)
    {
        return h_.guard(method, -1, [&]
        {
            const std::string fileName = text(name);
            const std::optional<std::vector<std::uint8_t>> bytes = h_.context.readFile(fileName, gameFile);
            if ( !bytes )
            {
                return -1;
            }
            return allocateHandle(h_.textFiles, TextFileState{ fileName, splitLines(*bytes), 0 });
        });
    }

    OpenBinaryFile* binary(const char* method, const int handle)
    {
        OpenBinaryFile* file = findHandle(h_.files, handle);
        if ( !file )
        {
            h_.invalid(method, "invalid file handle " + std::to_string(handle));
        }
        return file;
    }

    TextFileState* textFile(const char* method, const int handle)
    {
        TextFileState* file = findHandle(h_.textFiles, handle);
        if ( !file )
        {
            h_.invalid(method, "invalid text file handle " + std::to_string(handle));
        }
        return file;
    }

    Impl& h_;
};

JedPluginHost::Impl::Impl(JedHostContext& hostContext)
    : context(hostContext)
    , levelApi(std::make_unique<LevelApi>(*this))
    , jedApi(std::make_unique<JedApi>(*this))
{
}

JedPluginHost::JedPluginHost(JedHostContext& context) : impl_(std::make_unique<Impl>(context))
{
}

JedPluginHost::~JedPluginHost() = default;

IJED* JedPluginHost::jed() noexcept
{
    return impl_->jedApi.get();
}

IJEDLevel* JedPluginHost::level() noexcept
{
    return impl_->levelApi.get();
}

} // namespace sed::plugins
