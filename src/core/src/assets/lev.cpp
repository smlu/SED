#include "sed/assets/lev.hpp"

#include "sed/assets/material_dimensions.hpp"
#include "sed/level/geometry.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::assets
{

namespace
{

constexpr std::int32_t DfMaxSurfVerts = 24;

void normalizeLoadedLevUvsIfPossible(
    Level& level,
    const LevLoadOptions& options)
{
    if (options.resources == nullptr)
    {
        return;
    }

    const level::LevelUvConversionResult converted =
        normalizeLegacyLevelUvCoordinates(level, *options.resources);
    if (!converted)
    {
        const std::string location = converted.failedSurface
            ? " at sector "
                + std::to_string(converted.failedSurface->sector)
                + ", surface "
                + std::to_string(converted.failedSurface->surface)
            : std::string{};
        diagnosticEmit(
            options.onDiagnostic,
            DiagnosticSeverity::warning,
            "LEV texture coordinates remain in legacy pixel space because "
            "the referenced MAT dimensions could not be resolved"
                + location + ".");
    }
}

void setDefaultHeader(Level& level) noexcept
{
    level.header = LevelHeader{};
    level.header.version = 1;
    level.header.gravity = 4.0;

    level.header.ceilingSky.height = 15.0;
    level.header.horizonSky.distance = 100.0;
    level.header.horizonSky.pixelsPerRev = 768.0;

    level.header.mipmapDistances = { 1.0, 2.0, 3.0, 4.0 };
    level.header.lodDistances = { 0.3, 0.6, 0.9, 1.2 };
    level.header.perspectiveDistance = 2.0;
    level.header.gouraudDistance = 2.0;
    level.header.fog.enabled = false;
    level.header.fog.color = ColorWhite;
    level.header.fog.fogStart = 0.0;
    level.header.fog.fogEnd = 250.0;
}

struct LevLine
{
    std::string text{};
    std::size_t lineNumber{};
};

class LevReader
{
public:
    explicit LevReader(std::istream& in) : in_(in)
    {
    }

    std::optional<LevLine> nextNonEmptyLine()
    {
        std::string raw{};
        while (std::getline(in_, raw))
        {
            ++lineNumber_;

            sed::text::stripLineComments(raw);
            const std::string trimmed = sed::text::trim(raw);
            if (trimmed.empty())
            {
                continue;
            }

            LevLine line{};
            line.text = trimmed;
            line.lineNumber = lineNumber_;
            return line;
        }

        return std::nullopt;
    }

private:
    std::istream& in_;
    std::size_t lineNumber_{};
};

std::runtime_error makeParseError(const LevLine& line, const std::string_view message)
{
    std::ostringstream oss{};
    oss << "LEV parse error at line " << line.lineNumber << ": " << message;
    return std::runtime_error(oss.str());
}

std::vector<std::string_view> splitWordsReplaceColon(std::string& text)
{
    std::replace(text.begin(), text.end(), ':', ' ');
    return sed::text::splitWords(text);
}

bool isWord(const std::string_view token, const std::string_view expected) noexcept
{
    return sed::text::iequalsAscii(token, expected);
}

std::int32_t parseI32Token(const LevLine& line, const std::string_view token)
{
    if (const auto parsed = sed::text::parseInt<std::int32_t>(token))
    {
        return *parsed;
    }

    if (const auto parsedHex = sed::text::parseHex(token))
    {
        return static_cast<std::int32_t>(*parsedHex);
    }

    throw makeParseError(line, "Invalid integer token: " + std::string(token));
}

double parseDoubleToken(const LevLine& line, const std::string_view token)
{
    if (const auto parsed = sed::text::parseDouble(token))
    {
        return *parsed;
    }
    throw makeParseError(line, "Invalid float token: " + std::string(token));
}

std::uint32_t parseU32Token(const LevLine& line, const std::string_view token)
{
    if (const auto parsedHex = sed::text::parseHex(token))
    {
        return *parsedHex;
    }

    if (const auto parsed = sed::text::parseInt<std::uint32_t>(token))
    {
        return *parsed;
    }

    throw makeParseError(line, "Invalid unsigned integer token: " + std::string(token));
}

struct DfWall
{
    std::int32_t v1{};
    std::int32_t v2{};
    std::int32_t light{};
    std::int32_t iMid{ -1 };
    std::int32_t iBot{ -1 };
    std::int32_t iTop{ -1 };
    std::int32_t adjoin{ -1 };
    std::int32_t mirror{ -1 };
    std::int32_t flags1{};
    std::int32_t flags2{};
    std::int32_t mark{};
};

struct DfVertex
{
    double x{};
    double z{};
    std::int32_t mark{};
};

struct DfSector
{
    std::int32_t ambient{};
    double floorY{};
    double ceilingY{};
    std::int32_t floorTx{};
    std::int32_t ceilingTx{};
    double secY{};
    std::uint32_t flags{};
    std::int32_t layer{};
};

struct LevImportState
{
    LevLoadOptions options{};
    Level level{};
    std::vector<std::string> textures{};
    std::vector<DfSector> dfSectors{};

    explicit LevImportState(const LevLoadOptions& opts) : options(opts)
    {
        if (options.dfToJkFactor == 0.0)
        {
            throw std::runtime_error("LEV dfToJkFactor must be non-zero");
        }
    }

    void dfToJk(const double dfx, const double dfy, const double dfz, double& jkx, double& jky, double& jkz) const noexcept
    {
        const double f = options.dfToJkFactor;
        jkx = dfx / f;
        jky = dfz / f;
        jkz = -dfy / f;
    }

    double dfYToJkZ(const double dfy) const noexcept
    {
        return -dfy / options.dfToJkFactor;
    }

    std::string getTexture(const std::int32_t index) const
    {
        if (index == -1)
        {
            return {};
        }

        if (options.textureMode == LevTextureMode::Default)
        {
            return "dflt.mat";
        }

        if (index < 0 || static_cast<std::size_t>(index) >= textures.size())
        {
            return "dflt.mat";
        }

        std::filesystem::path p(textures[static_cast<std::size_t>(index)]);
        if (p.has_extension())
        {
            p.replace_extension(".mat");
        }
        else
        {
            p += ".mat";
        }
        return sed::text::lowerAscii(p.string());
    }
};

Surface makeDefaultSurface(const ProjectType projectType)
{
    Surface surface{};
    surface.adjoinFlags = SAF_Visible | SAF_Move;
    if (projectType != ProjectType::IJIM)
    {
        surface.adjoinFlags |= SAF_AllowSoundPass;
    }
    surface.material = "dflt.mat";
    surface.surfaceFlags = SF_Collision;
    surface.faceFlags = 0;
    surface.geo = static_cast<std::int32_t>(TGeoMode::Texture);
    surface.light = static_cast<std::int32_t>(TLightMode::Gouraud);
    surface.tex = 1;
    surface.uScale = 1.0f;
    surface.vScale = 1.0f;
    return surface;
}

double calcAngle(const std::vector<DfVertex>& vertices, const std::int32_t nv1, const std::int32_t nv2, const std::int32_t nv3) noexcept
{
    const DfVertex& v1 = vertices[static_cast<std::size_t>(nv1)];
    const DfVertex& v2 = vertices[static_cast<std::size_t>(nv2)];
    const DfVertex& v3 = vertices[static_cast<std::size_t>(nv3)];
    return sed::level::getAngle(v2.x, v2.z, v1.x, v1.z, v3.x, v3.z);
}

bool is0to180ex(const std::vector<DfVertex>& vertices, const std::int32_t nv1, const std::int32_t nv2, const std::int32_t nv3) noexcept
{
    const DfVertex& v1 = vertices[static_cast<std::size_t>(nv1)];
    const DfVertex& v2 = vertices[static_cast<std::size_t>(nv2)];
    const DfVertex& v3 = vertices[static_cast<std::size_t>(nv3)];

    const double dx1 = v1.x - v2.x;
    const double dy1 = v1.z - v2.z;
    const double dx2 = v3.x - v2.x;
    const double dy2 = v3.z - v2.z;
    return (dx1 * dy2 - dx2 * dy1) > 0.0;
}

bool is0to180inc(const std::vector<DfVertex>& vertices, const std::int32_t nv1, const std::int32_t nv2, const std::int32_t nv3) noexcept
{
    const DfVertex& v1 = vertices[static_cast<std::size_t>(nv1)];
    const DfVertex& v2 = vertices[static_cast<std::size_t>(nv2)];
    const DfVertex& v3 = vertices[static_cast<std::size_t>(nv3)];

    const double dx1 = v1.x - v2.x;
    const double dy1 = v1.z - v2.z;
    const double dx2 = v3.x - v2.x;
    const double dy2 = v3.z - v2.z;
    return (dx1 * dy2 - dx2 * dy1) >= 0.0;
}

void createUnderWaterSector(
    LevImportState& st,
    const std::int32_t dfSectorId,
    const DfSector& dfSector,
    const std::vector<DfVertex>& vertices,
    const std::vector<DfWall>& walls,
    const std::vector<std::int32_t>& poly,
    const double ceilingZ,
    const double floorZ)
{
    Sector& jsec = st.level.newSector();
    jsec.colormap = "dflt.cmp";
    jsec.mark = dfSectorId;
    jsec.layer = dfSector.layer;
    jsec.flags = 2;

    const double ppu = sed::level::levelPixelPerUnit(st.level);

    Surface fsurf = makeDefaultSurface(st.level.kind);
    fsurf.mark = -1;

    std::vector<std::int32_t> floorVertexIndices{};
    floorVertexIndices.reserve(poly.size());

    for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(poly.size()) - 1; i >= 0; --i)
    {
        const DfWall& w = walls[static_cast<std::size_t>(poly[static_cast<std::size_t>(i)])];
        const DfVertex& v = vertices[static_cast<std::size_t>(w.v1)];

        double x{};
        double y{};
        double z{};
        st.dfToJk(v.x, dfSector.floorY, v.z, x, y, z);
        z = floorZ;

        const std::int32_t vi = jsec.newVertex(x, y, z);
        floorVertexIndices.push_back(vi);
        fsurf.vertices.push_back(vi);
    }

    fsurf.surfaceFlags |= SF_Floor;
    fsurf.texVertices = std::vector<TexVertex>(fsurf.vertices.size());
    fsurf.normal = sed::level::surfaceCalcNormalNewell(jsec, fsurf);
    sed::level::surfaceArrangeTexture(ppu, jsec, fsurf, 0, TVector{ 1.0, 0.0, 0.0 }, TVector{ 0.0, 1.0, 0.0 });

    Surface csurf = makeDefaultSurface(st.level.kind);
    csurf.mark = dfSectorId;

    std::vector<std::int32_t> ceilVertexIndices{};
    ceilVertexIndices.reserve(poly.size());

    for (std::size_t i = 0; i < poly.size(); ++i)
    {
        const DfWall& w = walls[static_cast<std::size_t>(poly[i])];
        const DfVertex& v = vertices[static_cast<std::size_t>(w.v1)];

        double x{};
        double y{};
        double z{};
        st.dfToJk(v.x, dfSector.ceilingY, v.z, x, y, z);
        z = ceilingZ;

        const std::int32_t vi = jsec.newVertex(x, y, z);
        ceilVertexIndices.push_back(vi);
        csurf.vertices.push_back(vi);
    }

    csurf.texVertices = std::vector<TexVertex>(csurf.vertices.size());
    csurf.normal = sed::level::surfaceCalcNormalNewell(jsec, csurf);
    sed::level::surfaceArrangeTexture(ppu, jsec, csurf, 0, TVector{ 1.0, 0.0, 0.0 }, TVector{ 0.0, 1.0, 0.0 });

    (void)jsec.addSurface(std::move(fsurf));
    (void)jsec.addSurface(std::move(csurf));

    const std::int32_t nv = static_cast<std::int32_t>(poly.size());

    auto prev = [&](const std::int32_t index) noexcept
    {
        return sed::level::prevIndex(index, nv);
    };
    auto next = [&](const std::int32_t index) noexcept
    {
        return sed::level::nextIndex(index, nv);
    };

    for (std::int32_t i = 0; i < nv; ++i)
    {
        Surface jsurf = makeDefaultSurface(st.level.kind);
        jsurf.mark = -1;

        const std::int32_t j = (nv - i - 1);

        jsurf.vertices.push_back(floorVertexIndices[static_cast<std::size_t>(j)]);
        jsurf.vertices.push_back(floorVertexIndices[static_cast<std::size_t>(prev(j))]);

        jsurf.vertices.push_back(ceilVertexIndices[static_cast<std::size_t>(next(i))]);
        jsurf.vertices.push_back(ceilVertexIndices[static_cast<std::size_t>(i)]);

        jsurf.texVertices = std::vector<TexVertex>(jsurf.vertices.size());
        sed::level::surfaceRecalcNewAll(ppu, jsec, jsurf, 0);

        (void)jsec.addSurface(std::move(jsurf));
    }
}

void polyToJkSector(
    LevImportState& st,
    const std::int32_t dfSectorId,
    const DfSector& dfSector,
    const std::vector<DfVertex>& vertices,
    const std::vector<DfWall>& walls,
    const std::vector<std::int32_t>& poly)
{
    Sector& jsec = st.level.newSector();
    jsec.colormap = "dflt.cmp";
    jsec.mark = dfSectorId;
    jsec.layer = dfSector.layer;

    const double ppu = sed::level::levelPixelPerUnit(st.level);

    Surface fsurf = makeDefaultSurface(st.level.kind);
    fsurf.mark = -1;

    std::vector<std::int32_t> floorVertexIndices{};
    floorVertexIndices.reserve(poly.size());

    for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(poly.size()) - 1; i >= 0; --i)
    {
        const DfWall& w = walls[static_cast<std::size_t>(poly[static_cast<std::size_t>(i)])];
        const DfVertex& v = vertices[static_cast<std::size_t>(w.v1)];

        double x{};
        double y{};
        double z{};
        st.dfToJk(v.x, dfSector.floorY, v.z, x, y, z);
        const std::int32_t vi = jsec.newVertex(x, y, z);
        floorVertexIndices.push_back(vi);
        fsurf.vertices.push_back(vi);
    }

    fsurf.surfaceFlags |= SF_Floor;
    fsurf.material = st.getTexture(dfSector.floorTx);
    if ((dfSector.flags & 0x80u) != 0u)
    {
        fsurf.surfaceFlags |= SF_SkyHorizon;
    }

    fsurf.texVertices = std::vector<TexVertex>(fsurf.vertices.size());
    fsurf.normal = sed::level::surfaceCalcNormalNewell(jsec, fsurf);
    sed::level::surfaceArrangeTexture(ppu, jsec, fsurf, 0, TVector{ 1.0, 0.0, 0.0 }, TVector{ 0.0, 1.0, 0.0 });

    Surface csurf = makeDefaultSurface(st.level.kind);
    csurf.mark = -1;

    std::vector<std::int32_t> ceilVertexIndices{};
    ceilVertexIndices.reserve(poly.size());

    for (std::size_t i = 0; i < poly.size(); ++i)
    {
        const DfWall& w = walls[static_cast<std::size_t>(poly[i])];
        const DfVertex& v = vertices[static_cast<std::size_t>(w.v1)];

        double x{};
        double y{};
        double z{};
        st.dfToJk(v.x, dfSector.ceilingY, v.z, x, y, z);
        const std::int32_t vi = jsec.newVertex(x, y, z);
        ceilVertexIndices.push_back(vi);
        csurf.vertices.push_back(vi);
    }

    csurf.material = st.getTexture(dfSector.ceilingTx);
    if ((dfSector.flags & 0x1u) != 0u)
    {
        csurf.surfaceFlags |= SF_SkyHorizon;
    }

    csurf.texVertices = std::vector<TexVertex>(csurf.vertices.size());
    csurf.normal = sed::level::surfaceCalcNormalNewell(jsec, csurf);
    sed::level::surfaceArrangeTexture(ppu, jsec, csurf, 0, TVector{ 1.0, 0.0, 0.0 }, TVector{ 0.0, 1.0, 0.0 });

    (void)jsec.addSurface(std::move(fsurf));
    (void)jsec.addSurface(std::move(csurf));

    const std::int32_t nv = static_cast<std::int32_t>(poly.size());

    auto prev = [&](const std::int32_t index) noexcept
    {
        return sed::level::prevIndex(index, nv);
    };
    auto next = [&](const std::int32_t index) noexcept
    {
        return sed::level::nextIndex(index, nv);
    };

    for (std::int32_t i = 0; i < nv; ++i)
    {
        Surface jsurf = makeDefaultSurface(st.level.kind);

        const DfWall& w = walls[static_cast<std::size_t>(poly[static_cast<std::size_t>(i)])];
        jsurf.mark = w.adjoin;

        const std::uint32_t bot = static_cast<std::uint32_t>(static_cast<std::uint16_t>(w.iBot));
        const std::uint32_t top = static_cast<std::uint32_t>(static_cast<std::uint16_t>(w.iTop));
        jsurf.nMat = static_cast<std::int32_t>(bot | (top << 16));

        jsurf.material = st.getTexture(w.iMid);

        const std::int32_t j = (nv - i - 1);

        jsurf.vertices.push_back(floorVertexIndices[static_cast<std::size_t>(j)]);
        jsurf.vertices.push_back(floorVertexIndices[static_cast<std::size_t>(prev(j))]);

        jsurf.vertices.push_back(ceilVertexIndices[static_cast<std::size_t>(next(i))]);
        jsurf.vertices.push_back(ceilVertexIndices[static_cast<std::size_t>(i)]);

        jsurf.texVertices = std::vector<TexVertex>(jsurf.vertices.size());
        sed::level::surfaceRecalcNewAll(ppu, jsec, jsurf, 0);

        (void)jsec.addSurface(std::move(jsurf));
    }

    for (std::size_t i = 0; i < jsec.surfaces.size(); ++i)
    {
        jsec.surfaces[i].num = static_cast<std::int32_t>(i);
    }

    if (dfSector.secY > 0.0)
    {
        const double ceilingZ = st.dfYToJkZ(dfSector.floorY);
        const double floorZ = st.dfYToJkZ(dfSector.floorY + dfSector.secY);
        createUnderWaterSector(st, dfSectorId, dfSector, vertices, walls, poly, ceilingZ, floorZ);
    }
}

void importCycle(
    LevImportState& st,
    const std::int32_t dfSectorId,
    const DfSector& dfSector,
    const std::vector<DfVertex>& vertices,
    std::vector<DfWall>& walls,
    std::vector<std::int32_t> cycle)
{
    auto arePointsInTri = [&](const std::int32_t nv1, const std::int32_t nv2, const std::int32_t nv3) noexcept
    {
        for (const std::int32_t wi : cycle)
        {
            const std::int32_t iv = walls[static_cast<std::size_t>(wi)].v1;
            if (iv == nv1 || iv == nv2 || iv == nv3)
            {
                continue;
            }

            if (is0to180inc(vertices, nv1, nv2, iv) && is0to180inc(vertices, nv2, nv3, iv) && is0to180inc(vertices, nv3, nv1, iv))
            {
                return true;
            }
        }

        return false;
    };

    bool nonConvex = false;
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(cycle.size()); ++i)
    {
        const std::int32_t prev = sed::level::prevIndex(i, static_cast<std::int32_t>(cycle.size()));
        const DfWall& w = walls[static_cast<std::size_t>(cycle[static_cast<std::size_t>(i)])];
        const DfWall& pw = walls[static_cast<std::size_t>(cycle[static_cast<std::size_t>(prev)])];
        if (!is0to180inc(vertices, pw.v1, w.v1, w.v2))
        {
            nonConvex = true;
            break;
        }
    }

    if (!nonConvex && static_cast<std::int32_t>(cycle.size()) <= DfMaxSurfVerts)
    {
        polyToJkSector(st, dfSectorId, dfSector, vertices, walls, cycle);
        return;
    }

    std::vector<std::vector<std::int32_t>> polys{};

    auto addTriangle = [&](const std::int32_t wl1, const std::int32_t wl2, const std::optional<std::int32_t> wl3)
    {
        std::vector<std::int32_t> tri{};
        tri.push_back(wl1);
        tri.push_back(wl2);
        if (wl3)
        {
            tri.push_back(*wl3);
        }
        else
        {
            const DfWall& a = walls[static_cast<std::size_t>(wl1)];
            const DfWall& b = walls[static_cast<std::size_t>(wl2)];

            DfWall nw{};
            nw.v1 = b.v2;
            nw.v2 = a.v1;
            nw.adjoin = dfSectorId;
            const std::int32_t idx = static_cast<std::int32_t>(walls.size());
            walls.push_back(nw);
            tri.push_back(idx);
        }
        polys.push_back(std::move(tri));
    };

    auto subtractTri = [&](const std::int32_t swall)
    {
        const std::int32_t count = static_cast<std::int32_t>(cycle.size());
        const std::int32_t ewall = sed::level::nextIndex(swall, count);

        const DfWall& a = walls[static_cast<std::size_t>(cycle[static_cast<std::size_t>(swall)])];
        const DfWall& b = walls[static_cast<std::size_t>(cycle[static_cast<std::size_t>(ewall)])];

        DfWall nw{};
        nw.v1 = a.v1;
        nw.v2 = b.v2;
        nw.adjoin = dfSectorId;
        const std::int32_t idx = static_cast<std::int32_t>(walls.size());
        walls.push_back(nw);

        cycle.erase(cycle.begin() + swall);
        if (swall >= static_cast<std::int32_t>(cycle.size()))
        {
            if (!cycle.empty())
            {
                cycle.erase(cycle.begin());
            }
            cycle.push_back(idx);
        }
        else
        {
            cycle.erase(cycle.begin() + swall);
            cycle.insert(cycle.begin() + swall, idx);
        }
    };

    while (true)
    {
        if (cycle.size() == 3)
        {
            addTriangle(cycle[0], cycle[1], cycle[2]);
            break;
        }

        bool found = false;
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(cycle.size()); ++i)
        {
            const std::int32_t next = sed::level::nextIndex(i, static_cast<std::int32_t>(cycle.size()));
            const std::int32_t cwl = cycle[static_cast<std::size_t>(i)];
            const std::int32_t nwl = cycle[static_cast<std::size_t>(next)];

            const DfWall& a = walls[static_cast<std::size_t>(cwl)];
            const DfWall& b = walls[static_cast<std::size_t>(nwl)];

            if (!is0to180ex(vertices, a.v1, a.v2, b.v2))
            {
                continue;
            }
            if (arePointsInTri(a.v1, a.v2, b.v2))
            {
                continue;
            }

            addTriangle(cwl, nwl, std::nullopt);
            subtractTri(i);
            found = true;
            break;
        }

        if (cycle.size() == 3)
        {
            addTriangle(cycle[0], cycle[1], cycle[2]);
            break;
        }

        if (!found)
        {
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(cycle.size()); ++i)
            {
                const std::int32_t prev = sed::level::prevIndex(i, static_cast<std::int32_t>(cycle.size()));
                const std::int32_t cwl = cycle[static_cast<std::size_t>(prev)];
                const std::int32_t nwl = cycle[static_cast<std::size_t>(i)];

                const DfWall& a = walls[static_cast<std::size_t>(cwl)];
                const DfWall& b = walls[static_cast<std::size_t>(nwl)];

                if (!is0to180ex(vertices, a.v1, a.v2, b.v2))
                {
                    continue;
                }
                if (arePointsInTri(a.v1, a.v2, b.v2))
                {
                    continue;
                }

                addTriangle(cwl, nwl, std::nullopt);
                subtractTri(prev);
                found = true;
                break;
            }
        }

        if (!found)
        {
            break;
        }
    }

    auto findBackWall = [&](const std::int32_t wallIndex, std::int32_t& outPoly, std::int32_t& outWall) noexcept
    {
        const DfWall& wl = walls[static_cast<std::size_t>(wallIndex)];
        for (std::size_t p = 0; p < polys.size(); ++p)
        {
            const auto& pl = polys[p];
            for (std::size_t w = 0; w < pl.size(); ++w)
            {
                const DfWall& cw = walls[static_cast<std::size_t>(pl[w])];
                if (cw.v1 == wl.v2 && cw.v2 == wl.v1)
                {
                    outPoly = static_cast<std::int32_t>(p);
                    outWall = static_cast<std::int32_t>(w);
                    return true;
                }
            }
        }
        return false;
    };

    auto wouldBeConvex = [&](const std::int32_t np1, const std::int32_t nw1, const std::int32_t np2, const std::int32_t nw2) noexcept
    {
        const auto& p1 = polys[static_cast<std::size_t>(np1)];
        const auto& p2 = polys[static_cast<std::size_t>(np2)];

        const std::int32_t p1Prev = sed::level::prevIndex(nw1, static_cast<std::int32_t>(p1.size()));
        const std::int32_t p2Next = sed::level::nextIndex(nw2, static_cast<std::int32_t>(p2.size()));

        const DfWall& wl1a = walls[static_cast<std::size_t>(p1[static_cast<std::size_t>(p1Prev)])];
        const DfWall& wl2a = walls[static_cast<std::size_t>(p2[static_cast<std::size_t>(p2Next)])];
        if (!is0to180inc(vertices, wl1a.v1, wl1a.v2, wl2a.v2))
        {
            return false;
        }

        const std::int32_t p2Prev = sed::level::prevIndex(nw2, static_cast<std::int32_t>(p2.size()));
        const std::int32_t p1Next = sed::level::nextIndex(nw1, static_cast<std::int32_t>(p1.size()));

        const DfWall& wl1b = walls[static_cast<std::size_t>(p2[static_cast<std::size_t>(p2Prev)])];
        const DfWall& wl2b = walls[static_cast<std::size_t>(p1[static_cast<std::size_t>(p1Next)])];
        if (!is0to180inc(vertices, wl1b.v1, wl1b.v2, wl2b.v2))
        {
            return false;
        }

        return true;
    };

    auto doMergePolys = [&](const std::int32_t np1, const std::int32_t nw1, const std::int32_t np2, const std::int32_t nw2)
    {
        std::vector<std::int32_t>& p1 = polys[static_cast<std::size_t>(np1)];
        const std::vector<std::int32_t> p2 = polys[static_cast<std::size_t>(np2)];

        p1.erase(p1.begin() + nw1);

        std::int32_t n = sed::level::prevIndex(nw2, static_cast<std::int32_t>(p2.size()));
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(p2.size()) - 1; ++i)
        {
            p1.insert(p1.begin() + nw1, p2[static_cast<std::size_t>(n)]);
            n = sed::level::prevIndex(n, static_cast<std::int32_t>(p2.size()));
        }

        polys.erase(polys.begin() + np2);
    };

    while (true)
    {
        bool merged = false;
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(polys.size()); ++i)
        {
            const auto& pl = polys[static_cast<std::size_t>(i)];
            if (static_cast<std::int32_t>(pl.size()) >= DfMaxSurfVerts)
            {
                continue;
            }

            for (std::int32_t j = 0; j < static_cast<std::int32_t>(pl.size()); ++j)
            {
                std::int32_t p = -1;
                std::int32_t w = -1;
                if (!findBackWall(pl[static_cast<std::size_t>(j)], p, w))
                {
                    continue;
                }

                if (!wouldBeConvex(i, j, p, w))
                {
                    continue;
                }

                if (static_cast<std::int32_t>(polys[static_cast<std::size_t>(p)].size()) >= DfMaxSurfVerts)
                {
                    continue;
                }

                doMergePolys(i, j, p, w);
                merged = true;
                break;
            }

            if (merged)
            {
                break;
            }
        }

        if (!merged)
        {
            break;
        }
    }

    for (const auto& poly : polys)
    {
        polyToJkSector(st, dfSectorId, dfSector, vertices, walls, poly);
    }
}

void importSector(
    LevImportState& st,
    const std::int32_t dfSectorId,
    const DfSector& dfSector,
    const std::vector<DfVertex>& vertices,
    std::vector<DfWall>& walls)
{
    if (walls.empty())
    {
        return;
    }

    for (DfWall& w : walls)
    {
        w.mark = 0;
    }

    std::vector<std::vector<std::int32_t>> cycles(21);
    std::int32_t ncycles = 0;

    auto takeWall = [&](std::vector<std::int32_t>& wls, const std::int32_t index)
    {
        wls.push_back(index);
        walls[static_cast<std::size_t>(index)].mark = 1;
    };

    std::int32_t fvx = 0;
    auto takeFirstWall = [&](const std::int32_t index)
    {
        cycles[static_cast<std::size_t>(ncycles)].clear();
        fvx = walls[static_cast<std::size_t>(index)].v1;
        takeWall(cycles[static_cast<std::size_t>(ncycles)], index);
    };

    std::int32_t nwl = 0;
    takeFirstWall(0);

    while (true)
    {
        const std::int32_t nvx = walls[static_cast<std::size_t>(nwl)].v2;

        if (nvx == fvx)
        {
            ++ncycles;
            nwl = -1;
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(walls.size()); ++i)
            {
                if (walls[static_cast<std::size_t>(i)].mark == 0)
                {
                    nwl = i;
                    break;
                }
            }

            if (nwl == -1 || ncycles >= static_cast<std::int32_t>(cycles.size()))
            {
                break;
            }

            takeFirstWall(nwl);
            continue;
        }

        nwl = -1;
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(walls.size()); ++i)
        {
            const DfWall& w = walls[static_cast<std::size_t>(i)];
            if (w.mark == 1)
            {
                continue;
            }
            if (w.v1 == nvx)
            {
                nwl = i;
                break;
            }
        }

        if (nwl == -1)
        {
            break;
        }

        takeWall(cycles[static_cast<std::size_t>(ncycles)], nwl);
    }

    if (ncycles == 0)
    {
        return;
    }

    std::int32_t outer = -1;
    for (std::int32_t i = 0; i < ncycles; ++i)
    {
        const auto& wls = cycles[static_cast<std::size_t>(i)];

        double asum = 0.0;
        for (std::int32_t j = 0; j < static_cast<std::int32_t>(wls.size()); ++j)
        {
            const std::int32_t prev = sed::level::prevIndex(j, static_cast<std::int32_t>(wls.size()));
            const DfWall& w = walls[static_cast<std::size_t>(wls[static_cast<std::size_t>(j)])];
            const DfWall& pw = walls[static_cast<std::size_t>(wls[static_cast<std::size_t>(prev)])];
            asum += calcAngle(vertices, pw.v1, w.v1, w.v2);
        }

        const double expectedInteriorAngleSum =
            static_cast<double>(wls.size() - 2) * std::numbers::pi_v<double>;
        if (std::abs(asum - expectedInteriorAngleSum) < 0.1)
        {
            outer = i;
            break;
        }
    }

    if (outer == -1)
    {
        return;
    }

    if (outer != 0)
    {
        std::swap(cycles[0], cycles[static_cast<std::size_t>(outer)]);
    }

    auto doesLineIntersect = [&](const std::int32_t nv1, const std::int32_t nv2) noexcept
    {
        const DfVertex& vx1 = vertices[static_cast<std::size_t>(nv1)];
        const DfVertex& vx2 = vertices[static_cast<std::size_t>(nv2)];

        for (std::int32_t i = 0; i < ncycles; ++i)
        {
            const auto& cyc = cycles[static_cast<std::size_t>(i)];
            for (const std::int32_t wi : cyc)
            {
                const DfWall& w = walls[static_cast<std::size_t>(wi)];
                if (w.v1 == nv1 || w.v1 == nv2 || w.v2 == nv1 || w.v2 == nv2)
                {
                    continue;
                }

                const DfVertex& vx3 = vertices[static_cast<std::size_t>(w.v1)];
                const DfVertex& vx4 = vertices[static_cast<std::size_t>(w.v2)];

                if (sed::level::doIntersect(vx1.x, vx1.z, vx2.x, vx2.z, vx3.x, vx3.z, vx4.x, vx4.z))
                {
                    return true;
                }
            }
        }

        return false;
    };

    auto findNonIntersectingWall = [&](const std::int32_t cycleIndex, std::int32_t& outHoleWall) noexcept -> std::int32_t
    {
        const auto& main = cycles[0];
        const auto& hole = cycles[static_cast<std::size_t>(cycleIndex)];

        for (std::int32_t iwl0 = 0; iwl0 < static_cast<std::int32_t>(hole.size()); ++iwl0)
        {
            const std::int32_t v1 = walls[static_cast<std::size_t>(hole[static_cast<std::size_t>(iwl0)])].v1;
            const DfVertex& vx = vertices[static_cast<std::size_t>(v1)];

            std::int32_t n = 0;
            double dist = 99999.0;
            for (std::int32_t i = 0; i < static_cast<std::int32_t>(main.size()); ++i)
            {
                const std::int32_t v = walls[static_cast<std::size_t>(main[static_cast<std::size_t>(i)])].v1;
                const DfVertex& vx1 = vertices[static_cast<std::size_t>(v)];
                const double cdist = (vx1.x - vx.x) * (vx1.x - vx.x) + (vx1.z - vx.z) * (vx1.z - vx.z);
                if (cdist < dist)
                {
                    dist = cdist;
                    n = i;
                }
            }

            std::int32_t i = n;
            for (std::int32_t iter = 0; iter < static_cast<std::int32_t>(main.size()); ++iter)
            {
                const std::int32_t v2 = walls[static_cast<std::size_t>(main[static_cast<std::size_t>(i)])].v1;
                if (doesLineIntersect(v1, v2))
                {
                    i = sed::level::nextIndex(i, static_cast<std::int32_t>(main.size()));
                    continue;
                }

                outHoleWall = iwl0;
                return i;
            }
        }

        return -1;
    };

    auto mergeCycleAt = [&](const std::int32_t cycleIndex, const std::int32_t holeWallIndex, std::int32_t mainWallIndex)
    {
        auto& main = cycles[0];
        auto& hole = cycles[static_cast<std::size_t>(cycleIndex)];

        const std::int32_t vx0 = walls[static_cast<std::size_t>(main[static_cast<std::size_t>(mainWallIndex)])].v1;
        const std::int32_t vxn = walls[static_cast<std::size_t>(hole[static_cast<std::size_t>(holeWallIndex)])].v1;

        DfWall w0{};
        w0.v1 = vx0;
        w0.v2 = vxn;
        w0.adjoin = dfSectorId;
        const std::int32_t w0Index = static_cast<std::int32_t>(walls.size());
        walls.push_back(w0);
        main.insert(main.begin() + mainWallIndex, w0Index);

        DfWall w1{};
        w1.v1 = vxn;
        w1.v2 = vx0;
        w1.adjoin = dfSectorId;
        const std::int32_t w1Index = static_cast<std::int32_t>(walls.size());
        walls.push_back(w1);
        main.insert(main.begin() + mainWallIndex + 1, w1Index);
        ++mainWallIndex;

        std::int32_t n = sed::level::prevIndex(holeWallIndex, static_cast<std::int32_t>(hole.size()));
        for (std::int32_t w = 0; w < static_cast<std::int32_t>(hole.size()); ++w)
        {
            main.insert(main.begin() + mainWallIndex, hole[static_cast<std::size_t>(n)]);
            n = sed::level::prevIndex(n, static_cast<std::int32_t>(hole.size()));
        }
    };

    for (std::int32_t i = ncycles - 1; i >= 1; --i)
    {
        std::int32_t holeWall = -1;
        const std::int32_t mainWall = findNonIntersectingWall(i, holeWall);
        if (mainWall == -1)
        {
            continue;
        }

        mergeCycleAt(i, holeWall, mainWall);
        cycles[static_cast<std::size_t>(i)].clear();
        --ncycles;
    }

    importCycle(st, dfSectorId, dfSector, vertices, walls, std::move(cycles[0]));
}

void applySectorLights(LevImportState& st) noexcept
{
    for (Sector& sector : st.level.sectors)
    {
        const std::int32_t secnum = sector.mark;
        if (secnum < 0 || static_cast<std::size_t>(secnum) >= st.dfSectors.size())
        {
            continue;
        }

        const DfSector& dfSec = st.dfSectors[static_cast<std::size_t>(secnum)];
        const float i = static_cast<float>(dfSec.ambient) / 31.0f;

        sector.ambient = ColorF{ i, i, i, 0.0f };

        for (Surface& surf : sector.surfaces)
        {
            for (TexVertex& tv : surf.texVertices)
            {
                tv.color = ColorF{ i, i, i, i };
            }
        }
    }
}

void arrangeAdjoins(LevImportState& st)
{
    if (st.dfSectors.empty())
    {
        return;
    }

    const double ppu = sed::level::levelPixelPerUnit(st.level);

    for (Sector& sector : st.level.sectors)
    {
        const std::int32_t secnum = sector.mark;
        if (secnum < 0 || static_cast<std::size_t>(secnum) >= st.dfSectors.size())
        {
            continue;
        }

        const DfSector& dfSec = st.dfSectors[static_cast<std::size_t>(secnum)];
        const double csecFz = st.dfYToJkZ(dfSec.floorY);
        const double csecCz = st.dfYToJkZ(dfSec.ceilingY);

        for (std::ptrdiff_t j = static_cast<std::ptrdiff_t>(sector.surfaces.size()) - 1; j >= 0; --j)
        {
            const Surface original = sector.surfaces[static_cast<std::size_t>(j)];

            if (original.mark < 0 || original.mark == secnum)
            {
                continue;
            }

            const std::int32_t adjsc = original.mark;
            if (adjsc < 0 || static_cast<std::size_t>(adjsc) >= st.dfSectors.size())
            {
                continue;
            }

            const DfSector& adjDf = st.dfSectors[static_cast<std::size_t>(adjsc)];
            const double sec2Fz = st.dfYToJkZ(adjDf.floorY);
            const double sec2Cz = st.dfYToJkZ(adjDf.ceilingY);

            if (((sec2Fz <= csecFz) && (sec2Cz >= csecCz)) || (sec2Fz >= csecCz) || (sec2Cz <= csecFz))
            {
                continue;
            }

            if (original.vertices.size() < 4)
            {
                continue;
            }

            const std::int32_t v1Index = original.vertices[0];
            const std::int32_t v2Index = original.vertices[1];
            const std::int32_t v3Index = original.vertices[2];
            const std::int32_t v4Index = original.vertices[3];

            if (v1Index < 0 || v2Index < 0 || v3Index < 0 || v4Index < 0
                || static_cast<std::size_t>(v1Index) >= sector.vertices.size()
                || static_cast<std::size_t>(v2Index) >= sector.vertices.size()
                || static_cast<std::size_t>(v3Index) >= sector.vertices.size()
                || static_cast<std::size_t>(v4Index) >= sector.vertices.size())
            {
                continue;
            }

            const Vertex v1 = sector.vertices[static_cast<std::size_t>(v1Index)];
            const Vertex v2 = sector.vertices[static_cast<std::size_t>(v2Index)];

            const std::uint32_t packed = static_cast<std::uint32_t>(original.nMat);
            const std::int32_t botTex = static_cast<std::int32_t>(packed & 0xFFFFu);
            const std::int32_t topTex = static_cast<std::int32_t>((packed >> 16) & 0xFFFFu);

            const std::string midMaterial = original.material;
            const std::string botMaterial = st.getTexture(botTex);
            const std::string topMaterial = st.getTexture(topTex);

            const auto buildSurface = [&](const std::int32_t mark, const std::string& material, const std::array<std::int32_t, 4>& vxs)
            {
                Surface s = makeDefaultSurface(st.level.kind);
                s.mark = mark;
                s.material = material;
                s.vertices.assign(vxs.begin(), vxs.end());
                s.texVertices = std::vector<TexVertex>(s.vertices.size());
                sed::level::surfaceRecalcNewAll(ppu, sector, s, 0);
                sector.surfaces.push_back(std::move(s));
            };

            if ((sec2Fz > csecFz) && (sec2Cz >= csecCz))
            {
                const std::int32_t nv3 = sector.newVertex(v2.x, v2.y, sec2Fz);
                const std::int32_t nv4 = sector.newVertex(v1.x, v1.y, sec2Fz);

                buildSurface(-1, botMaterial, { v1Index, v2Index, nv3, nv4 });
                buildSurface(adjsc, midMaterial, { nv4, nv3, v3Index, v4Index });

                sector.surfaces.erase(sector.surfaces.begin() + j);
                continue;
            }

            if ((sec2Fz <= csecFz) && (sec2Cz < csecCz))
            {
                const std::int32_t nv3 = sector.newVertex(v2.x, v2.y, sec2Cz);
                const std::int32_t nv4 = sector.newVertex(v1.x, v1.y, sec2Cz);

                buildSurface(adjsc, midMaterial, { v1Index, v2Index, nv3, nv4 });
                buildSurface(-1, topMaterial, { nv4, nv3, v3Index, v4Index });

                sector.surfaces.erase(sector.surfaces.begin() + j);
                continue;
            }

            if ((sec2Fz > csecFz) && (sec2Cz < csecCz))
            {
                const std::int32_t nv3Bot = sector.newVertex(v2.x, v2.y, sec2Fz);
                const std::int32_t nv4Bot = sector.newVertex(v1.x, v1.y, sec2Fz);
                const std::int32_t nv3Top = sector.newVertex(v2.x, v2.y, sec2Cz);
                const std::int32_t nv4Top = sector.newVertex(v1.x, v1.y, sec2Cz);

                buildSurface(-1, botMaterial, { v1Index, v2Index, nv3Bot, nv4Bot });
                buildSurface(adjsc, midMaterial, { nv4Bot, nv3Bot, nv3Top, nv4Top });
                buildSurface(-1, topMaterial, { nv4Top, nv3Top, v3Index, v4Index });

                sector.surfaces.erase(sector.surfaces.begin() + j);
            }
        }
    }

    for (std::size_t si = 0; si < st.level.sectors.size(); ++si)
    {
        Sector& sec = st.level.sectors[si];

        for (std::size_t sfi = sec.surfaces.size(); sfi-- > 0;)
        {
            Surface& surf = sec.surfaces[sfi];
            if (surf.mark < 0)
            {
                continue;
            }
            if (surf.adjoin)
            {
                continue;
            }

            for (std::size_t oi = 0; oi < st.level.sectors.size(); ++oi)
            {
                Sector& otherSec = st.level.sectors[oi];
                if (otherSec.mark != surf.mark)
                {
                    continue;
                }

                for (std::size_t osf = 0; osf < otherSec.surfaces.size(); ++osf)
                {
                    if (oi == si && osf == sfi)
                    {
                        continue;
                    }

                    const Surface& otherSurf = otherSec.surfaces[osf];
                    if (sed::level::surfaceDoOverlap(sec, surf, otherSec, otherSurf))
                    {
                        (void)sed::level::levelSysAdjoinSurfaces(
                            st.level,
                            SurfaceRef{ static_cast<std::int32_t>(si), static_cast<std::int32_t>(sfi) },
                            SurfaceRef{ static_cast<std::int32_t>(oi), static_cast<std::int32_t>(osf) },
                            true);
                        break;
                    }
                }

                if (surf.adjoin)
                {
                    break;
                }
            }
        }
    }
}

void parseSector(LevReader& reader, LevImportState& st, const std::int32_t dfSectorId)
{
    if (dfSectorId < 0)
    {
        throw std::runtime_error("LEV sector id must be non-negative");
    }

    if (static_cast<std::size_t>(dfSectorId) >= st.dfSectors.size())
    {
        st.dfSectors.resize(static_cast<std::size_t>(dfSectorId) + 1);
    }

    DfSector sector{};
    sector.floorTx = 0;
    sector.ceilingTx = 0;
    sector.layer = 0;

    std::vector<DfVertex> vertices{};
    std::vector<DfWall> walls{};

    std::optional<LevLine> line{};
    while (true)
    {
        line = reader.nextNonEmptyLine();
        if (!line)
        {
            throw std::runtime_error("Unexpected EOF while reading SECTOR");
        }

        std::string normalized = line->text;
        const std::vector<std::string_view> tokens = splitWordsReplaceColon(normalized);
        if (tokens.empty())
        {
            continue;
        }

        if (isWord(tokens[0], "VERTICES") && tokens.size() >= 2)
        {
            const std::int32_t vcount = parseI32Token(*line, tokens[1]);
            vertices.reserve(static_cast<std::size_t>(std::max(0, vcount)));

            for (std::int32_t i = 0; i < vcount; ++i)
            {
                const std::optional<LevLine> vl = reader.nextNonEmptyLine();
                if (!vl)
                {
                    throw makeParseError(*line, "Unexpected EOF while reading VERTICES");
                }

                std::string vnorm = vl->text;
                const std::vector<std::string_view> vt = splitWordsReplaceColon(vnorm);

                double x{};
                double z{};
                for (std::size_t t = 0; t + 1 < vt.size(); ++t)
                {
                    if (isWord(vt[t], "X"))
                    {
                        x = parseDoubleToken(*vl, vt[t + 1]);
                    }
                    else if (isWord(vt[t], "Z"))
                    {
                        z = parseDoubleToken(*vl, vt[t + 1]);
                    }
                }

                vertices.push_back(DfVertex{ x, z, 0 });
            }

            break;
        }

        if (isWord(tokens[0], "AMBIENT") && tokens.size() >= 2)
        {
            sector.ambient = parseI32Token(*line, tokens[1]);
            continue;
        }

        if (tokens.size() >= 3 && isWord(tokens[0], "FLOOR") && isWord(tokens[1], "ALTITUDE"))
        {
            sector.floorY = parseDoubleToken(*line, tokens[2]);
            continue;
        }

        if (tokens.size() >= 3 && isWord(tokens[0], "CEILING") && isWord(tokens[1], "ALTITUDE"))
        {
            sector.ceilingY = parseDoubleToken(*line, tokens[2]);
            continue;
        }

        if (tokens.size() >= 3 && isWord(tokens[0], "SECOND") && isWord(tokens[1], "ALTITUDE"))
        {
            sector.secY = parseDoubleToken(*line, tokens[2]);
            continue;
        }

        if (tokens.size() >= 3 && isWord(tokens[0], "FLOOR") && isWord(tokens[1], "TEXTURE"))
        {
            sector.floorTx = parseI32Token(*line, tokens[2]);
            continue;
        }

        if (tokens.size() >= 3 && isWord(tokens[0], "CEILING") && isWord(tokens[1], "TEXTURE"))
        {
            sector.ceilingTx = parseI32Token(*line, tokens[2]);
            continue;
        }

        if (tokens.size() >= 2 && isWord(tokens[0], "FLAGS"))
        {
            sector.flags = parseU32Token(*line, tokens[1]);
            continue;
        }

        if (tokens.size() >= 2 && isWord(tokens[0], "LAYER"))
        {
            sector.layer = parseI32Token(*line, tokens[1]);
            continue;
        }
    }

    while (true)
    {
        line = reader.nextNonEmptyLine();
        if (!line)
        {
            throw std::runtime_error("Unexpected EOF while reading SECTOR");
        }

        std::string normalized = line->text;
        const std::vector<std::string_view> tokens = splitWordsReplaceColon(normalized);
        if (tokens.empty())
        {
            continue;
        }

        if (isWord(tokens[0], "WALLS") && tokens.size() >= 2)
        {
            const std::int32_t wcount = parseI32Token(*line, tokens[1]);
            walls.reserve(static_cast<std::size_t>(std::max(0, wcount)));

            std::int32_t read = 0;
            while (read < wcount)
            {
                const std::optional<LevLine> wl = reader.nextNonEmptyLine();
                if (!wl)
                {
                    throw makeParseError(*line, "Unexpected EOF while reading WALLS");
                }

                std::string wnorm = wl->text;
                const std::vector<std::string_view> wt = splitWordsReplaceColon(wnorm);
                if (wt.empty())
                {
                    continue;
                }

                DfWall wall{};

                for (std::size_t t = 0; t + 1 < wt.size(); ++t)
                {
                    const std::string_view k = wt[t];
                    const std::string_view v = wt[t + 1];

                    if (isWord(k, "LEFT"))
                    {
                        wall.v1 = parseI32Token(*wl, v);
                    }
                    else if (isWord(k, "RIGHT"))
                    {
                        wall.v2 = parseI32Token(*wl, v);
                    }
                    else if (isWord(k, "MID"))
                    {
                        wall.iMid = parseI32Token(*wl, v);
                    }
                    else if (isWord(k, "TOP"))
                    {
                        wall.iTop = parseI32Token(*wl, v);
                    }
                    else if (isWord(k, "BOT"))
                    {
                        wall.iBot = parseI32Token(*wl, v);
                    }
                    else if (isWord(k, "ADJOIN"))
                    {
                        wall.adjoin = parseI32Token(*wl, v);
                    }
                    else if (isWord(k, "MIRROR"))
                    {
                        wall.mirror = parseI32Token(*wl, v);
                    }
                    else if (isWord(k, "FLAGS"))
                    {
                        wall.flags1 = parseI32Token(*wl, v);
                        if (t + 3 < wt.size())
                        {
                            wall.flags2 = parseI32Token(*wl, wt[t + 3]);
                        }
                    }
                    else if (isWord(k, "LIGHT"))
                    {
                        wall.light = parseI32Token(*wl, v);
                    }
                }

                walls.push_back(wall);
                ++read;
            }

            break;
        }
    }

    st.dfSectors[static_cast<std::size_t>(dfSectorId)] = sector;
    importSector(st, dfSectorId, sector, vertices, walls);
}

void parseLev(std::istream& in, LevImportState& st)
{
    LevReader reader(in);

    while (true)
    {
        const std::optional<LevLine> line = reader.nextNonEmptyLine();
        if (!line)
        {
            break;
        }

        std::string normalized = line->text;
        const std::vector<std::string_view> tokens = splitWordsReplaceColon(normalized);
        if (tokens.empty())
        {
            continue;
        }

        if (isWord(tokens[0], "TEXTURES") && tokens.size() >= 2)
        {
            const std::int32_t count = parseI32Token(*line, tokens[1]);
            st.textures.clear();
            st.textures.reserve(static_cast<std::size_t>(std::max(0, count)));

            for (std::int32_t i = 0; i < count; ++i)
            {
                const std::optional<LevLine> tl = reader.nextNonEmptyLine();
                if (!tl)
                {
                    throw makeParseError(*line, "Unexpected EOF while reading TEXTURES");
                }

                std::string tnorm = tl->text;
                const std::vector<std::string_view> tt = splitWordsReplaceColon(tnorm);
                if (tt.size() < 2 || !isWord(tt[0], "TEXTURE"))
                {
                    throw makeParseError(*tl, "Expected TEXTURE line");
                }

                st.textures.push_back(std::string(tt[1]));
            }
            continue;
        }

        if (isWord(tokens[0], "NUMSECTORS") && tokens.size() >= 2)
        {
            const std::int32_t expected = parseI32Token(*line, tokens[1]);

            std::int32_t seen = 0;
            while (seen < expected)
            {
                const std::optional<LevLine> sl = reader.nextNonEmptyLine();
                if (!sl)
                {
                    break;
                }

                std::string snorm = sl->text;
                const std::vector<std::string_view> stokens = splitWordsReplaceColon(snorm);
                if (stokens.size() < 2 || !isWord(stokens[0], "SECTOR"))
                {
                    continue;
                }

                const std::int32_t dfSectorId = parseI32Token(*sl, stokens[1]);
                parseSector(reader, st, dfSectorId);
                ++seen;
            }
        }
    }
}

void parseObjects(std::istream& in, LevImportState& st, const Df2JkConvTable* conversionTable)
{
    LevReader reader(in);

    Thing* currentThing = nullptr;

    while (true)
    {
        const std::optional<LevLine> line = reader.nextNonEmptyLine();
        if (!line)
        {
            break;
        }

        std::string normalized = line->text;
        const std::vector<std::string_view> tokens = splitWordsReplaceColon(normalized);
        if (tokens.empty())
        {
            continue;
        }

        if (isWord(tokens[0], "CLASS"))
        {
            std::string_view className{};
            double ax{};
            double ay{};
            double az{};
            double pch{};
            double yaw{};
            double rol{};

            for (std::size_t i = 0; i + 1 < tokens.size(); ++i)
            {
                const std::string_view k = tokens[i];
                const std::string_view v = tokens[i + 1];

                if (isWord(k, "CLASS"))
                {
                    className = v;
                }
                else if (isWord(k, "X"))
                {
                    ax = parseDoubleToken(*line, v);
                }
                else if (isWord(k, "Y"))
                {
                    ay = parseDoubleToken(*line, v);
                }
                else if (isWord(k, "Z"))
                {
                    az = parseDoubleToken(*line, v);
                }
                else if (isWord(k, "PCH"))
                {
                    pch = parseDoubleToken(*line, v);
                }
                else if (isWord(k, "YAW"))
                {
                    yaw = parseDoubleToken(*line, v);
                }
                else if (isWord(k, "ROL"))
                {
                    rol = parseDoubleToken(*line, v);
                }
            }

            Thing& thing = st.level.newThing();
            currentThing = &thing;

            thing.name = "ghost";
            if (sed::text::iequalsAscii(className, "SPIRIT") || sed::text::iequalsAscii(className, "SAFE"))
            {
                thing.name = "walkplayer";
            }

            st.dfToJk(ax, ay, az, thing.x, thing.y, thing.z);
            thing.pch = pch;
            thing.yaw = yaw;
            thing.rol = rol;
            continue;
        }

        if (isWord(tokens[0], "SEQ"))
        {
            if (currentThing == nullptr)
            {
                continue;
            }

            while (true)
            {
                const std::optional<LevLine> seqLine = reader.nextNonEmptyLine();
                if (!seqLine)
                {
                    break;
                }

                std::string seqNorm = seqLine->text;
                const std::vector<std::string_view> seqTokens = splitWordsReplaceColon(seqNorm);
                if (seqTokens.empty())
                {
                    break;
                }

                if (isWord(seqTokens[0], "SEQEND"))
                {
                    break;
                }

                if (!isWord(seqTokens[0], "LOGIC") && !isWord(seqTokens[0], "TYPE"))
                {
                    continue;
                }

                std::string_view logic = (seqTokens.size() >= 2) ? seqTokens[1] : std::string_view{};
                if (sed::text::iequalsAscii(logic, "ITEM") && seqTokens.size() >= 3)
                {
                    logic = seqTokens[2];
                }

                if (conversionTable != nullptr)
                {
                    if (const auto mapped = lookupDf2Jk(*conversionTable, logic))
                    {
                        currentThing->name = std::string(*mapped);
                    }
                }
            }
        }
    }

    for (Thing& thing : st.level.things)
    {
        thing.sector = sed::level::levelFindSectorForThing(st.level, thing);
    }
}

} // namespace

Level loadFromLEV(std::istream& in, const LevLoadOptions& options)
{
    LevImportState st(options);
    st.level.clear();
    st.level.kind = ProjectType::JKDF2;
    st.level.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
    setDefaultHeader(st.level);
    st.level.ppunit = 320.0 * options.dfToJkFactor / 40.0;

    parseLev(in, st);
    arrangeAdjoins(st);
    applySectorLights(st);
    st.level.addMissingLayers();

    for (std::size_t s = 0; s < st.level.sectors.size(); ++s)
    {
        Sector& sector = st.level.sectors[s];
        sector.num = static_cast<std::int32_t>(s);
        for (std::size_t sf = 0; sf < sector.surfaces.size(); ++sf)
        {
            sector.surfaces[sf].num = static_cast<std::int32_t>(sf);
        }
    }

    for (std::size_t i = 0; i < st.level.things.size(); ++i)
    {
        st.level.things[i].num = static_cast<std::int32_t>(i);
    }

    normalizeLoadedLevUvsIfPossible(st.level, options);

    return std::move(st.level);
}

Level loadFromLEV(const std::filesystem::path& levPath, const LevLoadOptions& options)
{
    std::ifstream levIn(levPath);
    if (!levIn)
    {
        throw std::runtime_error("Failed to open LEV file");
    }

    LevImportState st(options);
    st.level.clear();
    st.level.kind = ProjectType::JKDF2;
    st.level.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
    setDefaultHeader(st.level);
    st.level.ppunit = 320.0 * options.dfToJkFactor / 40.0;

    parseLev(levIn, st);
    arrangeAdjoins(st);
    applySectorLights(st);

    if (options.loadObjects)
    {
        std::filesystem::path oPath = levPath;
        oPath.replace_extension(".O");

        std::ifstream oIn(oPath);
        if (!oIn)
        {
            oPath.replace_extension(".o");
            oIn.open(oPath);
        }

        if (oIn)
        {
            std::optional<Df2JkConvTable> loaded{};
            const Df2JkConvTable* table = options.conversionTable;

            if (table == nullptr)
            {
                if (!options.conversionTablePath.empty())
                {
                    loaded = loadDf2JkConversionTable(options.conversionTablePath);
                    table = &(*loaded);
                }
                else if (std::filesystem::exists("data/df2jk.lst"))
                {
                    loaded = loadDf2JkConversionTable("data/df2jk.lst");
                    table = &(*loaded);
                }
            }

            parseObjects(oIn, st, table);
        }
    }

    st.level.addMissingLayers();

    for (std::size_t s = 0; s < st.level.sectors.size(); ++s)
    {
        Sector& sector = st.level.sectors[s];
        sector.num = static_cast<std::int32_t>(s);
        for (std::size_t sf = 0; sf < sector.surfaces.size(); ++sf)
        {
            sector.surfaces[sf].num = static_cast<std::int32_t>(sf);
        }
    }

    for (std::size_t i = 0; i < st.level.things.size(); ++i)
    {
        st.level.things[i].num = static_cast<std::int32_t>(i);
    }

    normalizeLoadedLevUvsIfPossible(st.level, options);

    return std::move(st.level);
}

} // namespace sed::assets
