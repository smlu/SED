#include "sed/assets/jed.hpp"

#include "sed/assets/material_dimensions.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/level/uv.hpp"

#include "sed/level/geometry.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sed::assets
{

namespace
{

using sed::text::iequalsAscii;
using sed::text::parseDouble;
using sed::text::parseHex;
using sed::text::parseInt;
using sed::text::splitWords;
using sed::text::trim;
using sed::text::trimView;
using sed::text::upperAscii;

void stripComments(std::string& line)
{
    std::size_t cut = std::string::npos;

    const std::size_t hash = line.find('#');
    if (hash != std::string::npos)
    {
        cut = hash;
    }

    const std::size_t semi = line.find(';');
    if (semi != std::string::npos)
    {
        cut = (cut == std::string::npos) ? semi : std::min(cut, semi);
    }

    const std::size_t slash = line.find("//");
    if (slash != std::string::npos)
    {
        cut = (cut == std::string::npos) ? slash : std::min(cut, slash);
    }

    if (cut != std::string::npos)
    {
        line.erase(cut);
    }
}

std::optional<double> parseVersionNumber(const std::string_view token)
{
    bool seenDot = false;
    std::size_t end = 0;

    for (; end < token.size(); ++end)
    {
        const unsigned char ch = static_cast<unsigned char>(token[end]);
        if (std::isdigit(ch))
        {
            continue;
        }
        if (ch == '.' && !seenDot)
        {
            seenDot = true;
            continue;
        }
        break;
    }

    if (end == 0)
    {
        return std::nullopt;
    }

    return parseDouble(token.substr(0, end));
}

float parseJedUvFloatOr(
    const std::string_view token,
    const float fallback)
{
    const std::optional<double> value = parseDouble(token);
    if (!value)
    {
        return fallback;
    }

    constexpr double maxFloat =
        static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(*value) || *value < -maxFloat || *value > maxFloat)
    {
        throw std::runtime_error(
            "Cannot load JED UV value: it is invalid or outside float range.");
    }
    return static_cast<float>(*value);
}

std::string orDash(const std::string& s)
{
    return s.empty() ? std::string{"-"} : s;
}

class ScopedStreamState
{
public:
    explicit ScopedStreamState(std::ostream& out) : out_(out), flags_(out.flags()), precision_(out.precision())
    {
    }

    ~ScopedStreamState()
    {
        out_.flags(flags_);
        out_.precision(precision_);
    }

    ScopedStreamState(const ScopedStreamState&) = delete;
    ScopedStreamState& operator=(const ScopedStreamState&) = delete;

private:
    std::ostream& out_;
    std::ios::fmtflags flags_{};
    std::streamsize precision_{};
};

void writeHex(std::ostream& out, const std::uint32_t value)
{
    ScopedStreamState scoped(out);
    out << "0x" << std::hex << std::nouppercase << value;
}

void skipEmpty(const std::vector<std::string>& lines, std::size_t& idx)
{
    while (idx < lines.size() && lines[idx].empty())
    {
        ++idx;
    }
}

Level loadLegacyJedLines(const std::vector<std::string>& lines)
{
    Level level{};
    level.clear();
    level.kind = ProjectType::JKDF2;

    std::size_t idx = 0;
    while (idx < lines.size())
    {
        const auto tokens = splitWords(lines[idx++]);
        if (tokens.empty())
        {
            continue;
        }

        const std::string key = upperAscii(tokens[0]);
        if (key == "VERSION" && tokens.size() >= 2)
        {
            level.header.version = parseInt<std::int32_t>(tokens[1]).value_or(level.header.version);
            continue;
        }

        if (key != "SECTORS" || tokens.size() < 2)
        {
            continue;
        }

        const std::int32_t sectorCount = parseInt<std::int32_t>(tokens[1]).value_or(0);
        for (std::int32_t sectorIndex = 0; sectorIndex < sectorCount && idx < lines.size(); ++sectorIndex)
        {
            skipEmpty(lines, idx);
            if (idx >= lines.size())
            {
                break;
            }

            const auto sectorTokens = splitWords(lines[idx++]);
            if (sectorTokens.empty() || !iequalsAscii(sectorTokens[0], "SECTOR"))
            {
                continue;
            }

            Sector& sector = level.newSector();
            if (sectorTokens.size() >= 2)
            {
                sector.num = parseInt<std::int32_t>(sectorTokens[1]).value_or(sector.num);
            }
            if (sectorTokens.size() >= 3)
            {
                sector.flags = static_cast<std::int32_t>(parseHex(sectorTokens[2]).value_or(0));
            }

            skipEmpty(lines, idx);
            if (idx >= lines.size())
            {
                break;
            }

            const auto vertexHeader = splitWords(lines[idx++]);
            const std::int32_t vertexCount =
                (vertexHeader.size() >= 2 && iequalsAscii(vertexHeader[0], "VERTICES"))
                    ? parseInt<std::int32_t>(vertexHeader[1]).value_or(0)
                    : 0;

            for (std::int32_t v = 0; v < vertexCount && idx < lines.size(); ++v)
            {
                skipEmpty(lines, idx);
                if (idx >= lines.size())
                {
                    break;
                }

                const auto vertexTokens = splitWords(lines[idx++]);
                if (vertexTokens.size() < 3)
                {
                    continue;
                }

                sector.addVertex(
                    parseDouble(vertexTokens[0]).value_or(0.0),
                    parseDouble(vertexTokens[1]).value_or(0.0),
                    parseDouble(vertexTokens[2]).value_or(0.0));
            }

            skipEmpty(lines, idx);
            if (idx >= lines.size())
            {
                break;
            }

            const auto surfaceHeader = splitWords(lines[idx++]);
            const std::int32_t surfaceCount =
                (surfaceHeader.size() >= 2 && iequalsAscii(surfaceHeader[0], "SURFACES"))
                    ? parseInt<std::int32_t>(surfaceHeader[1]).value_or(0)
                    : 0;

            for (std::int32_t surfaceIndex = 0; surfaceIndex < surfaceCount && idx < lines.size(); ++surfaceIndex)
            {
                skipEmpty(lines, idx);
                if (idx >= lines.size())
                {
                    break;
                }

                const auto surfaceTokens = splitWords(lines[idx++]);
                if (surfaceTokens.size() < 14 || !iequalsAscii(surfaceTokens[0], "SURFACE"))
                {
                    continue;
                }

                Surface surface{};
                surface.num = surfaceIndex;
                surface.surfaceFlags = static_cast<std::int32_t>(parseHex(surfaceTokens[1]).value_or(0));
                surface.faceFlags = parseHex(surfaceTokens[2]).value_or(0);
                const std::int32_t surfaceVertexCount = parseInt<std::int32_t>(surfaceTokens[3]).value_or(0);
                surface.geo = parseInt<std::int32_t>(surfaceTokens[4]).value_or(0);
                surface.light = parseInt<std::int32_t>(surfaceTokens[5]).value_or(0);
                surface.tex = parseInt<std::int32_t>(surfaceTokens[6]).value_or(0);
                surface.nMat = -1;
                surface.material = std::string(surfaceTokens[13]);

                std::vector<std::string_view> vertexTokens{};
                if (idx < lines.size())
                {
                    vertexTokens = splitWords(lines[idx++]);
                }

                std::vector<std::string_view> texTokens{};
                if (idx < lines.size())
                {
                    texTokens = splitWords(lines[idx++]);
                }

                surface.vertices.reserve(static_cast<std::size_t>(surfaceVertexCount));
                surface.texVertices.reserve(static_cast<std::size_t>(surfaceVertexCount));

                for (std::int32_t v = 0; v < surfaceVertexCount; ++v)
                {
                    const std::size_t vi = static_cast<std::size_t>(v);
                    if (vi < vertexTokens.size())
                    {
                        surface.vertices.push_back(parseInt<std::int32_t>(vertexTokens[vi]).value_or(-1));
                    }

                    TexVertex tv{};
                    const std::size_t texBase = vi * 2u;
                    if (texBase + 1u < texTokens.size())
                    {
                        tv.u = parseJedUvFloatOr(texTokens[texBase], 0.0f);
                        tv.v = parseJedUvFloatOr(texTokens[texBase + 1u], 0.0f);
                    }
                    tv.color = ColorWhite;
                    surface.texVertices.push_back(tv);
                }

                surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);
                const std::int32_t localIndex = sector.addSurface(std::move(surface));
                if (localIndex >= 0 && static_cast<std::size_t>(localIndex) < sector.surfaces.size())
                {
                    sector.surfaces[static_cast<std::size_t>(localIndex)].num = localIndex;
                }
            }
        }
    }

    return level;
}

} // namespace

static void normalizeLoadedJedUvsIfPossible(
    Level& level,
    const JedLoadOptions& options)
{
    if (options.resources == nullptr
        || level.uvCoordinateSpace != UvCoordinateSpace::TexturePixels)
    {
        return;
    }

    std::unordered_map<std::string,
        std::optional<level::UvTextureDimensions>> dimensions{};
    const level::LevelUvConversionResult converted =
        level::convertLevelUvCoordinateSpace(
            level,
            UvCoordinateSpace::Normalized,
            [&](const Surface& surface)
                -> std::optional<level::UvTextureDimensions>
            {
                const std::string key = upperAscii(
                    trimView(surface.material));
                if (const auto found = dimensions.find(key);
                    found != dimensions.end())
                {
                    return found->second;
                }
                std::optional<level::UvTextureDimensions> resolved =
                    resolveMaterialTextureDimensions(
                        *options.resources, surface.material);
                dimensions.emplace(key, resolved);
                return resolved;
            });
    if (!converted)
    {
        const std::string location = converted.failedSurface
            ? " at sector "
                + std::to_string(converted.failedSurface->sector)
                + ", surface "
                + std::to_string(converted.failedSurface->surface)
            : std::string{};
        diagnosticEmit(options.onDiagnostic, DiagnosticSeverity::warning,
            "JED texture coordinates remain in legacy pixel space because "
            "the referenced MAT dimensions could not be resolved"
                + location + ".");
    }
}

Level loadFromJED(std::istream& in, const JedLoadOptions& options)
{
    Level level{};
    level.clear();
    level.kind = ProjectType::JKDF2;
    level.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;

    std::unordered_map<std::string, std::vector<std::string>> sections;
    std::vector<std::string> legacyLines;

    std::string line;
    std::string firstLine;
    while (std::getline(in, line))
    {
        stripComments(line);
        firstLine = trim(line);
        if (!firstLine.empty())
        {
            break;
        }
    }

    if (firstLine.empty())
    {
        return level;
    }

    {
        const auto tokens = splitWords(firstLine);
        if (tokens.size() >= 2 && (iequalsAscii(tokens[0], "JED") || iequalsAscii(tokens[0], "SED")))
        {
            (void)parseVersionNumber(tokens[1]).value_or(0.0);
        }
    }

    while (std::getline(in, line))
    {
        stripComments(line);
        std::string trimmed = trim(line);
        if (trimmed.empty())
        {
            continue;
        }

        const auto tokens = splitWords(trimmed);
        if (tokens.size() >= 2 && iequalsAscii(tokens[0], "SEC:"))
        {
            const std::string secName = upperAscii(tokens[1]);
            std::vector<std::string> secLines;

            while (std::getline(in, line))
            {
                stripComments(line);
                std::string t = trim(line);
                if (iequalsAscii(t, "END"))
                {
                    break;
                }

                secLines.push_back(std::move(t));
            }

            sections.emplace(secName, std::move(secLines));
        }
        else
        {
            legacyLines.push_back(std::move(trimmed));
        }
    }

    if (sections.empty() && !legacyLines.empty())
    {
        level = loadLegacyJedLines(legacyLines);
        level.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
        normalizeLoadedJedUvsIfPossible(level, options);
        return level;
    }

    if (sections.find("MOTS") != sections.end())
    {
        level.kind = ProjectType::MOTS;
    }

    if (const auto it = sections.find("HEADER"); it != sections.end())
    {
        for (const std::string& raw : it->second)
        {
            const auto tokens = splitWords(raw);
            if (tokens.empty())
            {
                continue;
            }

            const std::string key = upperAscii(tokens[0]);
            if (key == "HDR" && tokens.size() >= 8)
            {
                level.header.version = parseInt<std::int32_t>(tokens[1]).value_or(0);
                level.header.gravity = parseDouble(tokens[2]).value_or(0.0);
                level.header.ceilingSky.height = parseDouble(tokens[3]).value_or(0.0);
                level.header.perspectiveDistance = parseDouble(tokens[4]).value_or(0.0);
                level.header.gouraudDistance = parseDouble(tokens[5]).value_or(0.0);

                const std::string_view cmp = tokens[6];
                level.masterCMP = (cmp == "-") ? std::string{} : std::string(cmp);
                level.ppunit = parseDouble(tokens[7]).value_or(level.ppunit);

                if (level.header.version == 2 || level.header.version == 3)
                {
                    level.kind = ProjectType::IJIM;
                }
            }
            else if (key == "HOR" && tokens.size() >= 5)
            {
                level.header.horizonSky.distance = parseDouble(tokens[1]).value_or(0.0);
                level.header.horizonSky.pixelsPerRev = parseDouble(tokens[2]).value_or(0.0);
                level.header.horizonSky.offset.x = parseDouble(tokens[3]).value_or(0.0);
                level.header.horizonSky.offset.y = parseDouble(tokens[4]).value_or(0.0);
            }
            else if (key == "CEIL" && tokens.size() >= 3)
            {
                level.header.ceilingSky.offset.x = parseDouble(tokens[1]).value_or(0.0);
                level.header.ceilingSky.offset.y = parseDouble(tokens[2]).value_or(0.0);
            }
            else if (key == "MIP" && tokens.size() >= 5)
            {
                for (int i = 0; i < 4; ++i)
                {
                    level.header.mipmapDistances[static_cast<std::size_t>(i)] = parseDouble(tokens[1 + i]).value_or(0.0);
                }
            }
            else if (key == "LOD" && tokens.size() >= 5)
            {
                for (int i = 0; i < 4; ++i)
                {
                    level.header.lodDistances[static_cast<std::size_t>(i)] = parseDouble(tokens[1 + i]).value_or(0.0);
                }
            }
        }
    }

    if (const auto it = sections.find("EXPORTBUFFERS"); it != sections.end())
    {
        auto setExtra = [&level](const std::string& key, const std::uint32_t value)
        {
            LevelExportBufferExtras& extras = level.exportBufferExtras;
            if (key == "SOUNDS") extras.sounds = value;
            else if (key == "MATERIALS") extras.materials = value;
            else if (key == "AICLASSES") extras.aiClasses = value;
            else if (key == "MODELS") extras.models = value;
            else if (key == "SPRITES") extras.sprites = value;
            else if (key == "KEYFRAMES") extras.keyframes = value;
            else if (key == "PUPPETS") extras.puppets = value;
            else if (key == "SOUNDCLASSES") extras.soundClasses = value;
            else if (key == "COGSCRIPTS") extras.cogScripts = value;
            else if (key == "COGS") extras.cogs = value;
            else if (key == "TEMPLATES") extras.templates = value;
            else if (key == "THINGS") extras.things = value;
        };

        for (const std::string& raw : it->second)
        {
            const auto tokens = splitWords(raw);
            if (tokens.size() < 2)
            {
                continue;
            }
            if (const auto value = parseInt<std::uint32_t>(tokens[1]))
            {
                setExtra(upperAscii(tokens[0]), *value);
            }
        }
    }

    if (const auto it = sections.find("GEO"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        skipEmpty(lines, idx);
        const std::int32_t secCount = (idx < lines.size())
            ? ([&]()
                {
                    const auto hdr = splitWords(lines[idx++]);
                    if (hdr.size() >= 2 && iequalsAscii(hdr[0], "SECS"))
                    {
                        return parseInt<std::int32_t>(hdr[1]).value_or(0);
                    }
                    return 0;
                })()
            : 0;

        for (std::int32_t s = 0; s < secCount && idx < lines.size(); ++s)
        {
            skipEmpty(lines, idx);
            if (idx >= lines.size())
            {
                break;
            }

            Sector& sector = level.newSector();

            {
                const auto tokens = splitWords(lines[idx++]);
                if (tokens.size() >= 10)
                {
                    sector.colormap = (tokens[0] == "-") ? std::string{} : std::string(tokens[0]);
                    sector.sound = (tokens[1] == "-") ? std::string{} : std::string(tokens[1]);
                    sector.soundVolume = parseDouble(tokens[2]).value_or(0.0);
                    sector.flags = static_cast<std::int32_t>(parseHex(tokens[3]).value_or(0));

                    const float amb = static_cast<float>(parseDouble(tokens[4]).value_or(0.0));
                    const float ex = static_cast<float>(parseDouble(tokens[5]).value_or(0.0));
                    sector.ambient = sed::intensityToColor(amb);
                    sector.extraLight = sed::intensityToColor(ex);

                    sector.tint.r = static_cast<float>(parseDouble(tokens[6]).value_or(0.0));
                    sector.tint.g = static_cast<float>(parseDouble(tokens[7]).value_or(0.0));
                    sector.tint.b = static_cast<float>(parseDouble(tokens[8]).value_or(0.0));
                    sector.layer = parseInt<std::int32_t>(tokens[9]).value_or(0);
                }
            }

            skipEmpty(lines, idx);
            if (idx >= lines.size())
            {
                break;
            }

            const auto vHdr = splitWords(lines[idx++]);
            const std::int32_t vCount =
                (vHdr.size() >= 2 && iequalsAscii(vHdr[0], "VXS")) ? parseInt<std::int32_t>(vHdr[1]).value_or(0) : 0;

            for (std::int32_t v = 0; v < vCount && idx < lines.size(); ++v)
            {
                skipEmpty(lines, idx);
                if (idx >= lines.size())
                {
                    break;
                }

                const auto tokens = splitWords(lines[idx++]);
                if (tokens.size() < 3)
                {
                    continue;
                }

                sector.newVertex(parseDouble(tokens[0]).value_or(0.0),
                                 parseDouble(tokens[1]).value_or(0.0),
                                 parseDouble(tokens[2]).value_or(0.0));
            }

            skipEmpty(lines, idx);
            if (idx >= lines.size())
            {
                break;
            }

            const auto sHdr = splitWords(lines[idx++]);
            const std::int32_t surfCount =
                (sHdr.size() >= 2 && iequalsAscii(sHdr[0], "SURFS")) ? parseInt<std::int32_t>(sHdr[1]).value_or(0) : 0;

            for (std::int32_t sf = 0; sf < surfCount && idx < lines.size(); ++sf)
            {
                skipEmpty(lines, idx);
                if (idx >= lines.size())
                {
                    break;
                }

                const auto tokens = splitWords(lines[idx++]);
                if (tokens.size() < 13)
                {
                    continue;
                }

                Surface surf{};
                surf.num = sf;
                surf.material = (tokens[0] == "-") ? std::string{} : std::string(tokens[0]);
                surf.surfaceFlags = static_cast<std::int32_t>(parseHex(tokens[1]).value_or(0));
                surf.faceFlags = parseHex(tokens[2]).value_or(0);
                surf.geo = parseInt<std::int32_t>(tokens[3]).value_or(0);
                surf.light = parseInt<std::int32_t>(tokens[4]).value_or(0);
                surf.tex = parseInt<std::int32_t>(tokens[5]).value_or(0);

                const float ex = static_cast<float>(parseDouble(tokens[6]).value_or(0.0));
                surf.extraLight = sed::signedIntensityToColor(ex);

                const std::int32_t nsv = parseInt<std::int32_t>(tokens[7]).value_or(0);
                const std::int32_t asec = parseInt<std::int32_t>(tokens[8]).value_or(-1);
                const std::int32_t asf2 = parseInt<std::int32_t>(tokens[9]).value_or(-1);

                surf.adjoinFlags = static_cast<std::int32_t>(parseHex(tokens[10]).value_or(0));
                surf.uScale = parseJedUvFloatOr(tokens[11], 1.0f);
                surf.vScale = parseJedUvFloatOr(tokens[12], 1.0f);

                if (asec >= 0 && asf2 >= 0)
                {
                    surf.adjoin = SurfaceRef{asec, asf2};
                }
                else
                {
                    surf.adjoin.reset();
                }

                if (idx < lines.size())
                {
                    const auto vtokens = splitWords(lines[idx++]);

                    surf.vertices.reserve(static_cast<std::size_t>(nsv));
                    surf.texVertices.reserve(static_cast<std::size_t>(nsv));

                    for (std::int32_t v = 0; v < nsv; ++v)
                    {
                        const std::size_t base = static_cast<std::size_t>(v) * 4u;
                        if (base + 3 >= vtokens.size())
                        {
                            break;
                        }

                        const std::int32_t vi = parseInt<std::int32_t>(vtokens[base + 0]).value_or(-1);
                        TexVertex tv{};
                        tv.u = parseJedUvFloatOr(vtokens[base + 1], 0.0f);
                        tv.v = parseJedUvFloatOr(vtokens[base + 2], 0.0f);
                        tv.color.a = static_cast<float>(parseDouble(vtokens[base + 3]).value_or(1.0));
                        tv.color.r = tv.color.a;
                        tv.color.g = tv.color.a;
                        tv.color.b = tv.color.a;

                        surf.vertices.push_back(vi);
                        surf.texVertices.push_back(tv);
                    }

                    const std::size_t rgbStart = static_cast<std::size_t>(nsv) * 4u;
                    for (std::int32_t v = 0; v < nsv; ++v)
                    {
                        const std::size_t base = rgbStart + static_cast<std::size_t>(v) * 3u;
                        if (base + 2 >= vtokens.size() || static_cast<std::size_t>(v) >= surf.texVertices.size())
                        {
                            break;
                        }

                        surf.texVertices[static_cast<std::size_t>(v)].color.r =
                            static_cast<float>(parseDouble(vtokens[base + 0]).value_or(0.0));
                        surf.texVertices[static_cast<std::size_t>(v)].color.g =
                            static_cast<float>(parseDouble(vtokens[base + 1]).value_or(0.0));
                        surf.texVertices[static_cast<std::size_t>(v)].color.b =
                            static_cast<float>(parseDouble(vtokens[base + 2]).value_or(0.0));
                    }
                }

                const std::int32_t localIndex = sector.addSurface(std::move(surf));
                if (localIndex >= 0 && static_cast<std::size_t>(localIndex) < sector.surfaces.size())
                {
                    sector.surfaces[static_cast<std::size_t>(localIndex)].num = localIndex;
                }
            }
        }

        for (Sector& sector : level.sectors)
        {
            for (Surface& surface : sector.surfaces)
            {
                if (!surface.adjoin)
                {
                    continue;
                }

                const std::int32_t asec = surface.adjoin->sector;
                const std::int32_t asf = surface.adjoin->surface;
                if (asec < 0 || asf < 0)
                {
                    surface.adjoin.reset();
                    continue;
                }
                if (static_cast<std::size_t>(asec) >= level.sectors.size())
                {
                    surface.adjoin.reset();
                    continue;
                }
                if (static_cast<std::size_t>(asf) >= level.sectors[static_cast<std::size_t>(asec)].surfaces.size())
                {
                    surface.adjoin.reset();
                }
            }
        }
    }

    if (const auto it = sections.find("THINGS"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        skipEmpty(lines, idx);
        const std::int32_t count = (idx < lines.size())
            ? ([&]()
                {
                    const auto hdr = splitWords(lines[idx++]);
                    if (hdr.size() >= 2 && iequalsAscii(hdr[0], "THS"))
                    {
                        return parseInt<std::int32_t>(hdr[1]).value_or(0);
                    }
                    return 0;
                })()
            : 0;

        for (std::int32_t i = 0; i < count && idx < lines.size(); ++i)
        {
            skipEmpty(lines, idx);
            if (idx >= lines.size())
            {
                break;
            }

            const auto tokens = splitWords(lines[idx++]);
            if (tokens.size() < 9)
            {
                continue;
            }

            Thing& thing = level.newThing();
            thing.name = std::string(tokens[0]);
            thing.x = parseDouble(tokens[1]).value_or(0.0);
            thing.y = parseDouble(tokens[2]).value_or(0.0);
            thing.z = parseDouble(tokens[3]).value_or(0.0);
            thing.pch = parseDouble(tokens[4]).value_or(0.0);
            thing.yaw = parseDouble(tokens[5]).value_or(0.0);
            thing.rol = parseDouble(tokens[6]).value_or(0.0);
            thing.sector = parseInt<std::int32_t>(tokens[7]).value_or(-1);
            thing.layer = parseInt<std::int32_t>(tokens[8]).value_or(0);

            if (thing.sector < 0 || static_cast<std::size_t>(thing.sector) >= level.sectors.size())
            {
                thing.sector = -1;
            }

            std::string valuesLine;
            if (idx < lines.size())
            {
                valuesLine = lines[idx++];
            }

            const auto kvs = splitWords(valuesLine);
            for (const std::string_view kv : kvs)
            {
                const std::size_t eq = kv.find('=');
                if (eq == std::string_view::npos)
                {
                    continue;
                }

                thing.values.push_back(deserializeTplValue(
                    kv.substr(0, eq), kv.substr(eq + 1)));
            }
        }
    }

    if (const auto it = sections.find("LIGHTS"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        skipEmpty(lines, idx);
        const std::int32_t count = (idx < lines.size())
            ? ([&]()
                {
                    const auto hdr = splitWords(lines[idx++]);
                    if (hdr.size() >= 2 && iequalsAscii(hdr[0], "LTS"))
                    {
                        return parseInt<std::int32_t>(hdr[1]).value_or(0);
                    }
                    return 0;
                })()
            : 0;

        for (std::int32_t i = 0; i < count && idx < lines.size(); ++i)
        {
            skipEmpty(lines, idx);
            if (idx >= lines.size())
            {
                break;
            }

            const auto tokens = splitWords(lines[idx++]);
            if (tokens.size() < 10)
            {
                continue;
            }

            SedLight& light = level.newLight();
            light.position.x = parseDouble(tokens[0]).value_or(0.0);
            light.position.y = parseDouble(tokens[1]).value_or(0.0);
            light.position.z = parseDouble(tokens[2]).value_or(0.0);
            light.layer = parseInt<std::int32_t>(tokens[3]).value_or(0);
            light.intensity = parseDouble(tokens[4]).value_or(10.0);
            light.range = parseDouble(tokens[5]).value_or(2.0);
            light.flags = static_cast<std::int32_t>(parseHex(tokens[6]).value_or(0));

            light.color.r = static_cast<float>(parseDouble(tokens[7]).value_or(1.0));
            light.color.g = static_cast<float>(parseDouble(tokens[8]).value_or(1.0));
            light.color.b = static_cast<float>(parseDouble(tokens[9]).value_or(1.0));
            light.color.a = 1.0f;

            if (level.kind == ProjectType::JKDF2)
            {
                light.color = ColorWhite;
            }
        }
    }

    if (const auto it = sections.find("COGS"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        skipEmpty(lines, idx);
        const std::int32_t count = (idx < lines.size())
            ? ([&]()
                {
                    const auto hdr = splitWords(lines[idx++]);
                    if (hdr.size() >= 2 && iequalsAscii(hdr[0], "COGS"))
                    {
                        return parseInt<std::int32_t>(hdr[1]).value_or(0);
                    }
                    return 0;
                })()
            : 0;

        auto cogTypeFromName = [&](const std::string_view name) -> CogType
        {
            if (iequalsAscii(name, "ai"))
            {
                return CogType::ai;
            }
            if (iequalsAscii(name, "cog"))
            {
                return CogType::cog;
            }
            if (iequalsAscii(name, "key"))
            {
                return CogType::key;
            }
            if (iequalsAscii(name, "mat"))
            {
                return CogType::mat;
            }
            if (iequalsAscii(name, "msg"))
            {
                return CogType::msg;
            }
            if (iequalsAscii(name, "3do"))
            {
                return CogType::model3do;
            }
            if (iequalsAscii(name, "sec"))
            {
                return CogType::sec;
            }
            // Legacy Pascal JED files serialize COG audio fields as `sound`;
            // newer C++ saves use the canonical `wav` spelling.
            if (iequalsAscii(name, "sound") || iequalsAscii(name, "wav"))
            {
                return CogType::wav;
            }
            if (iequalsAscii(name, "srf") || iequalsAscii(name, "surface"))
            {
                return CogType::surface;
            }
            if (iequalsAscii(name, "tpl") || iequalsAscii(name, "template"))
            {
                return CogType::template_;
            }
            if (iequalsAscii(name, "thg") || iequalsAscii(name, "thing"))
            {
                return CogType::thing;
            }
            if (iequalsAscii(name, "int"))
            {
                return CogType::int_;
            }
            if (iequalsAscii(name, "float"))
            {
                return CogType::float_;
            }
            if (iequalsAscii(name, "vect"))
            {
                return CogType::vect;
            }

            return CogType::unk;
        };

        for (std::int32_t i = 0; i < count && idx < lines.size(); ++i)
        {
            skipEmpty(lines, idx);
            if (idx >= lines.size())
            {
                break;
            }

            const auto tokens = splitWords(lines[idx++]);
            if (tokens.empty())
            {
                continue;
            }

            Cog& cog = level.newCog();
            cog.name = std::string(tokens[0]);

            for (std::size_t t = 1; t < tokens.size(); ++t)
            {
                const std::string_view spec = tokens[t];
                const std::size_t eq = spec.find('=');
                if (eq == std::string_view::npos)
                {
                    continue;
                }

                std::string_view left = spec.substr(0, eq);
                const std::string_view right = spec.substr(eq + 1);

                std::string_view valueName = left;
                std::string_view typeName;
                if (const std::size_t colon = left.find(':'); colon != std::string_view::npos)
                {
                    valueName = left.substr(0, colon);
                    typeName = left.substr(colon + 1);
                }

                CogValue v{};
                v.name = std::string(valueName);
                v.cogType = cogTypeFromName(typeName);
                std::string rightText(right);

                switch (v.cogType)
                {
                case CogType::int_:
                    v.type = ValueType::integer;
                    break;
                case CogType::cog:
                    v.type = ValueType::integer;
                    break;
                case CogType::float_:
                    v.type = ValueType::floating;
                    break;
                case CogType::vect:
                    v.type = ValueType::vect;
                    break;
                case CogType::sec:
                case CogType::surface:
                case CogType::thing:
                    v.type = ValueType::ptr;
                    break;
                default:
                    v.type = ValueType::str;
                    break;
                }

                if (v.cogType == CogType::surface
                    && t + 1 < tokens.size()
                    && right.find_first_of(" \t\r\n") == std::string_view::npos
                    && tokens[t + 1].find('=') == std::string_view::npos)
                {
                    rightText.push_back(' ');
                    rightText.append(tokens[t + 1]);
                    ++t;
                }

                if (!v.parseJed(level, rightText) && !v.parse(rightText))
                {
                    v.type = ValueType::str;
                    v.stringValue = std::move(rightText);
                }

                cog.values.push_back(std::move(v));
            }
        }
    }

    if (const auto it = sections.find("LAYERS"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        skipEmpty(lines, idx);
        if (idx >= lines.size())
        {
            normalizeLoadedJedUvsIfPossible(level, options);
            return level;
        }

        const auto hdr = splitWords(lines[idx++]);
        const std::int32_t count =
            (hdr.size() >= 2 && iequalsAscii(hdr[0], "LAYERS")) ? parseInt<std::int32_t>(hdr[1]).value_or(0) : 0;

        for (std::int32_t i = 0; i < count && idx < lines.size(); ++i)
        {
            skipEmpty(lines, idx);
            if (idx >= lines.size())
            {
                break;
            }

            level.layers.push_back(lines[idx++]);
        }
    }

    if (const auto it = sections.find("LVIS"); it != sections.end())
    {
        for (const std::string& l : it->second)
        {
            if (!l.empty())
            {
                level.lvisstring = l;
                break;
            }
        }
    }

    if (const auto it = sections.find("HIERARCHYDEF"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        skipEmpty(lines, idx);
        std::int32_t count = 0;
        if (idx < lines.size())
        {
            const auto hdr = splitWords(lines[idx++]);
            if (hdr.size() >= 2 && iequalsAscii(hdr[0], "NODES"))
            {
                count = parseInt<std::int32_t>(hdr[1]).value_or(0);
            }
        }

        level.h3doNodes.reserve(static_cast<std::size_t>(std::max<std::int32_t>(0, count)));
        for (std::int32_t i = 0; i < count && idx < lines.size(); ++i)
        {
            skipEmpty(lines, idx);
            if (idx >= lines.size())
            {
                break;
            }

            const auto tokens = splitWords(lines[idx++]);
            if (tokens.size() < 18)
            {
                continue;
            }

            HNode& node = level.new3doNode();
            node.type = parseHex(tokens[2]).value_or(1);
            node.mesh = parseInt<std::int32_t>(tokens[3]).value_or(-1);
            node.parent = parseInt<std::int32_t>(tokens[4]).value_or(-1);

            node.position.x = parseDouble(tokens[8]).value_or(0.0);
            node.position.y = parseDouble(tokens[9]).value_or(0.0);
            node.position.z = parseDouble(tokens[10]).value_or(0.0);

            node.rotation.pitch = parseDouble(tokens[11]).value_or(0.0);
            node.rotation.yaw = parseDouble(tokens[12]).value_or(0.0);
            node.rotation.roll = parseDouble(tokens[13]).value_or(0.0);

            node.pivot.x = parseDouble(tokens[14]).value_or(0.0);
            node.pivot.y = parseDouble(tokens[15]).value_or(0.0);
            node.pivot.z = parseDouble(tokens[16]).value_or(0.0);

            node.name = std::string(tokens[17]);
        }
    }

    normalizeLoadedJedUvsIfPossible(level, options);

    return level;
}

Level loadFromJED(
    const std::filesystem::path& path,
    const JedLoadOptions& options)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("Failed to open JED: " + path.string());
    }

    return loadFromJED(in, options);
}

static void saveToJedImpl(
    const Level& sourceLevel,
    std::ostream& out,
    const JedSaveOptions& options)
{
    if (sourceLevel.hasAmbiguousPendingJklCogSurfaceReferences())
    {
        std::string scriptName = "<unnamed>";
        for (const Cog& cog : sourceLevel.cogs)
        {
            bool pending = false;
            for (std::size_t index = 0; index < cog.values.size(); ++index)
            {
                const CogValue& value = cog.values[index];
                if (value.type == ValueType::str && value.cogType == CogType::unk
                    && value.name.size() > 5
                    && value.name.compare(0, 5, "param") == 0)
                {
                    pending = true;
                    break;
                }
            }
            if (pending)
            {
                scriptName = cog.name.empty() ? "<unnamed>" : cog.name;
                break;
            }
        }
        throw std::runtime_error(
            "Cannot export JED: imported COG surface references require "
            "resource-backed COG script '" + scriptName
            + "' before serialization.");
    }

    if (sourceLevel.uvCoordinateSpace != UvCoordinateSpace::TexturePixels
        && sourceLevel.uvCoordinateSpace != UvCoordinateSpace::Normalized)
    {
        throw std::runtime_error(
            "Cannot export JED: the Level UV coordinate space is invalid.");
    }

    for (std::size_t sectorIndex = 0;
         sectorIndex < sourceLevel.sectors.size();
         ++sectorIndex)
    {
        const Sector& sector = sourceLevel.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0;
             surfaceIndex < sector.surfaces.size();
             ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            if (surface.texVertices.size() != surface.vertices.size())
            {
                throw std::runtime_error(
                    "Cannot export JED: texture-vertex data is incomplete at sector "
                    + std::to_string(sectorIndex) + ", surface "
                    + std::to_string(surfaceIndex) + ".");
            }
        }
    }

    Level convertedLevel = sourceLevel;
    std::unordered_map<std::string,
        std::optional<level::UvTextureDimensions>> dimensions{};
    const level::LevelUvConversionResult converted =
        level::convertLevelUvCoordinateSpace(
            convertedLevel,
            UvCoordinateSpace::TexturePixels,
            [&](const Surface& surface)
                -> std::optional<level::UvTextureDimensions>
            {
                if (options.resources == nullptr)
                {
                    return std::nullopt;
                }
                const std::string key = upperAscii(
                    trimView(surface.material));
                if (const auto found = dimensions.find(key);
                    found != dimensions.end())
                {
                    return found->second;
                }
                std::optional<level::UvTextureDimensions> resolved =
                    resolveMaterialTextureDimensions(
                        *options.resources, surface.material);
                dimensions.emplace(key, resolved);
                return resolved;
            });
    if (!converted)
    {
        const std::string location = converted.failedSurface
            ? " at sector "
                + std::to_string(converted.failedSurface->sector)
                + ", surface "
                + std::to_string(converted.failedSurface->surface)
            : std::string{};
        if (converted.error.find("dimensions") != std::string::npos)
        {
            throw std::runtime_error(
                "Cannot export JED texture coordinates because the referenced "
                "MAT dimensions could not be resolved" + location + ".");
        }
        throw std::runtime_error(
            "Cannot export JED: a UV value is invalid" + location + ".");
    }

    const Level& level = convertedLevel;
    out.imbue(std::locale::classic());
    out << std::fixed;

    out << "SED 0.3.0\n";

    if (level.kind == ProjectType::MOTS)
    {
        out << '\n';
        out << "SEC: MOTS\n";
        out << "END\n";
    }

    out << '\n';
    out << "SEC: HEADER\n";

    {
        ScopedStreamState scoped(out);
        out << std::setprecision(6);
        out << "HDR " << level.header.version << ' ' << level.header.gravity << ' ' << level.header.ceilingSky.height << ' ' << level.header.perspectiveDistance
            << ' ' << level.header.gouraudDistance << ' ' << orDash(level.masterCMP) << ' ' << level.ppunit << '\n';

        out << "HOR " << level.header.horizonSky.distance << ' ' << level.header.horizonSky.pixelsPerRev << ' ' << level.header.horizonSky.offset.x << ' '
            << level.header.horizonSky.offset.y << '\n';

        out << "CEIL " << level.header.ceilingSky.offset.x << ' ' << level.header.ceilingSky.offset.y << '\n';

        out << "MIP " << level.header.mipmapDistances[0] << ' ' << level.header.mipmapDistances[1] << ' ' << level.header.mipmapDistances[2] << ' '
            << level.header.mipmapDistances[3] << '\n';

        out << "LOD " << level.header.lodDistances[0] << ' ' << level.header.lodDistances[1] << ' ' << level.header.lodDistances[2] << ' '
            << level.header.lodDistances[3] << '\n';
    }

    out << "END\n";

    if (level.exportBufferExtras != LevelExportBufferExtras{})
    {
        const LevelExportBufferExtras& extras = level.exportBufferExtras;
        out << '\n';
        out << "SEC: EXPORTBUFFERS\n";
        out << "SOUNDS " << extras.sounds << '\n';
        out << "MATERIALS " << extras.materials << '\n';
        out << "AICLASSES " << extras.aiClasses << '\n';
        out << "MODELS " << extras.models << '\n';
        out << "SPRITES " << extras.sprites << '\n';
        out << "KEYFRAMES " << extras.keyframes << '\n';
        out << "PUPPETS " << extras.puppets << '\n';
        out << "SOUNDCLASSES " << extras.soundClasses << '\n';
        out << "COGSCRIPTS " << extras.cogScripts << '\n';
        out << "COGS " << extras.cogs << '\n';
        out << "TEMPLATES " << extras.templates << '\n';
        out << "THINGS " << extras.things << '\n';
        out << "END\n";
    }

    out << '\n';
    out << "SEC: GEO\n";
    out << "SECS " << level.sectors.size() << '\n';

    for (const Sector& sector : level.sectors)
    {
        {
            ScopedStreamState scoped(out);
            out << std::setprecision(6);

            out << orDash(sector.colormap) << ' ' << orDash(sector.sound) << ' ' << sector.soundVolume << ' ';
            writeHex(out, static_cast<std::uint32_t>(sector.flags));
            out << ' ' << sector.ambient.intensity() << ' ' << sector.extraLight.intensity() << ' ' << sector.tint.r << ' ' << sector.tint.g << ' '
                << sector.tint.b << ' ' << sector.layer << '\n';
        }

        out << "VXS " << sector.vertices.size() << '\n';
        {
            ScopedStreamState scoped(out);
            out << std::setprecision(6);

            for (const Vertex& v : sector.vertices)
            {
                out << v.x << ' ' << v.y << ' ' << v.z << '\n';
            }
        }

        out << "SURFS " << sector.surfaces.size() << '\n';
        for (const Surface& surf : sector.surfaces)
        {
            const std::int32_t asec = surf.adjoin ? surf.adjoin->sector : -1;
            const std::int32_t asf = surf.adjoin ? surf.adjoin->surface : -1;

            {
                ScopedStreamState scoped(out);
                out << std::setprecision(6);

                out << orDash(surf.material) << ' ';
                writeHex(out, static_cast<std::uint32_t>(surf.surfaceFlags));
                out << ' ';
                writeHex(out, surf.faceFlags);
                out << ' ' << surf.geo << ' ' << surf.light << ' ' << surf.tex << ' '
                    << clampSignedUnit(surf.extraLight.intensity()) << ' ' << surf.vertices.size() << ' '
                    << asec << ' ' << asf << ' ';
                writeHex(out, static_cast<std::uint32_t>(surf.adjoinFlags));
                out << ' ' << surf.uScale << ' ' << surf.vScale << '\n';
            }

            ScopedStreamState scoped(out);

            for (std::size_t i = 0; i < surf.vertices.size(); ++i)
            {
                const std::int32_t vi = surf.vertices[i];
                const TexVertex tv = surf.texVertices[i];

                out << ' ' << vi;
                out << std::setprecision(2) << ' ' << tv.u << ' ' << tv.v;
                out << std::setprecision(4) << ' ' << tv.color.a;
            }

            out << std::setprecision(4);
            for (std::size_t i = 0; i < surf.vertices.size(); ++i)
            {
                const TexVertex tv = surf.texVertices[i];
                out << ' ' << tv.color.r << ' ' << tv.color.g << ' ' << tv.color.b;
            }

            out << '\n';
        }
    }

    out << "END\n";

    out << '\n';
    out << "SEC: THINGS\n";
    out << "THS " << level.things.size() << '\n';

    for (const Thing& thing : level.things)
    {
        {
            ScopedStreamState scoped(out);
            out << std::setprecision(6);

            out << thing.name << ' ' << thing.x << ' ' << thing.y << ' ' << thing.z << ' ' << thing.pch << ' ' << thing.yaw << ' ' << thing.rol << ' '
                << thing.sector << ' ' << thing.layer << '\n';
        }

        for (const TplValue& v : thing.values)
        {
            out << ' ' << v.name << '=' << serializeTplValue(v);
        }
        out << '\n';
    }

    out << "END\n";

    out << '\n';
    out << "SEC: LIGHTS\n";
    out << "LTS " << level.lights.size() << '\n';

    for (const SedLight& light : level.lights)
    {
        ScopedStreamState scoped(out);
        out << std::setprecision(6);

        out << light.position.x << ' ' << light.position.y << ' ' << light.position.z << ' ' << light.layer << ' ' << light.intensity << ' ' << light.range << ' ';
        writeHex(out, static_cast<std::uint32_t>(light.flags));
        out << std::setprecision(4) << ' ' << light.color.r << ' ' << light.color.g << ' ' << light.color.b << '\n';
    }

    out << "END\n";

    out << '\n';
    out << "SEC: COGS\n";
    out << "COGS " << level.cogs.size() << '\n';

    for (const Cog& cog : level.cogs)
    {
        out << cog.name;
        for (const CogValue& v : cog.values)
        {
            out << ' ' << v.name << ':' << sed::toString(v.cogType) << '=' << v.asJedString(level);
        }
        out << '\n';
    }

    out << "END\n";

    out << '\n';
    out << "SEC: LAYERS\n";
    out << "LAYERS " << level.layers.size() << '\n';
    for (const std::string& layer : level.layers)
    {
        out << layer << '\n';
    }
    out << "END\n";

    out << '\n';
    out << "SEC: LVIS\n";
    out << level.lvisstring << '\n';
    out << "END\n";

    out << '\n';
    out << "SEC: HIERARCHYDEF\n";
    out << "NODES " << level.h3doNodes.size() << '\n';

    for (std::size_t i = 0; i < level.h3doNodes.size(); ++i)
    {
        const HNode& node = level.h3doNodes[i];

        std::int32_t firstChild = -1;
        std::int32_t nextSibling = -1;
        std::int32_t childCount = 0;

        for (std::size_t n = 0; n < level.h3doNodes.size(); ++n)
        {
            if (level.h3doNodes[n].parent == static_cast<std::int32_t>(i))
            {
                ++childCount;
                if (firstChild == -1)
                {
                    firstChild = static_cast<std::int32_t>(n);
                }
            }
        }

        if (node.parent != -1)
        {
            for (std::size_t n = i + 1; n < level.h3doNodes.size(); ++n)
            {
                if (level.h3doNodes[n].parent == node.parent)
                {
                    nextSibling = static_cast<std::int32_t>(n);
                    break;
                }
            }
        }

        ScopedStreamState scoped(out);
        out << std::setprecision(6);

        out << i << ':';
        out << " 0x0 0x" << std::hex << std::nouppercase << node.type << std::dec;
        out << ' ' << node.mesh;
        out << ' ' << node.parent;
        out << ' ' << firstChild;
        out << ' ' << nextSibling;
        out << ' ' << childCount;

        out << ' ' << node.position.x;
        out << ' ' << node.position.y;
        out << ' ' << node.position.z;

        out << ' ' << node.rotation.pitch;
        out << ' ' << node.rotation.yaw;
        out << ' ' << node.rotation.roll;

        out << ' ' << node.pivot.x;
        out << ' ' << node.pivot.y;
        out << ' ' << node.pivot.z;

        out << ' ' << node.name << '\n';
    }

    out << "END\n";
}

static std::string serializeJedText(
    const Level& level,
    const JedSaveOptions& options)
{
    std::ostringstream staged{};
    saveToJedImpl(level, staged, options);
    return staged.str();
}

void saveToJED(
    const Level& level,
    std::ostream& out,
    const JedSaveOptions& options)
{
    const std::string text = serializeJedText(level, options);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out)
    {
        throw std::runtime_error("Failed to write JED output.");
    }
}

void saveToJED(
    const Level& level,
    const std::filesystem::path& path,
    const JedSaveOptions& options)
{
    const std::string text = serializeJedText(level, options);

    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        throw std::runtime_error("Failed to open for write: " + path.string());
    }

    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out)
    {
        throw std::runtime_error("Failed to write JED file: " + path.string());
    }
}

} // namespace sed::assets
