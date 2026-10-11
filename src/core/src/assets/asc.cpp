#include "sed/assets/asc.hpp"

#include "sed/assets/material_dimensions.hpp"
#include "sed/level/geometry.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace sed::assets
{

namespace
{

void setDefaultHeader(Level& level) noexcept
{
    level.header = LevelHeader{};
    level.header.version = 1;
    level.header.gravity = 4.0;

    level.header.ceilingSky.height = 15.0;
    level.header.horizonSky.distance = 100.0;
    level.header.horizonSky.pixelsPerRev = 768.0;

    level.header.mipmapDistances = {1.0, 2.0, 3.0, 4.0};
    level.header.lodDistances = {0.3, 0.6, 0.9, 1.2};
    level.header.perspectiveDistance = 2.0;
    level.header.gouraudDistance = 2.0;
    level.header.fog.enabled = false;
    level.header.fog.color = ColorWhite;
    level.header.fog.fogStart = 0.0;
    level.header.fog.fogEnd = 250.0;
}

struct AscLine
{
    std::string text{};
    std::size_t lineNumber{};
};

class AscReader
{
public:
    explicit AscReader(std::istream& in) : in_(in)
    {
    }

    std::optional<AscLine> nextNonEmptyLine()
    {
        std::string raw{};
        while (std::getline(in_, raw))
        {
            ++lineNumber_;

            sed::text::stripHashComment(raw);
            const std::string trimmed = sed::text::trim(raw);
            if (trimmed.empty())
            {
                continue;
            }

            AscLine out{};
            out.text = sed::text::lowerAscii(trimmed);
            out.lineNumber = lineNumber_;
            return out;
        }

        return std::nullopt;
    }

private:
    std::istream& in_;
    std::size_t lineNumber_{};
};

std::runtime_error makeParseError(const AscLine& line, const std::string_view message)
{
    std::ostringstream oss{};
    oss << "ASC parse error at line " << line.lineNumber << ": " << message;
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

std::int32_t parseI32Token(const AscLine& line, const std::string_view token)
{
    if (const auto parsed = sed::text::parseInt<std::int32_t>(token))
    {
        return *parsed;
    }
    throw makeParseError(line, "Invalid integer token: " + std::string(token));
}

double parseDoubleToken(const AscLine& line, const std::string_view token)
{
    if (const auto parsed = sed::text::parseDouble(token))
    {
        return *parsed;
    }
    throw makeParseError(line, "Invalid float token: " + std::string(token));
}

void parseVertexLine(const AscLine& line, std::int32_t& outIndex, double& outX, double& outY, double& outZ)
{
    std::string normalized = line.text;
    const std::vector<std::string_view> words = splitWordsReplaceColon(normalized);

    if (words.size() < 8 || !isWord(words[0], "vertex"))
    {
        throw makeParseError(line, "Expected vertex record");
    }

    outIndex = parseI32Token(line, words[1]);

    if (!isWord(words[2], "x") || !isWord(words[4], "y") || !isWord(words[6], "z"))
    {
        throw makeParseError(line, "Expected vertex coordinate keys x y z");
    }

    outX = parseDoubleToken(line, words[3]);
    outY = parseDoubleToken(line, words[5]);
    outZ = parseDoubleToken(line, words[7]);
}

void parseFaceLine(const AscLine& line, std::int32_t& outIndex, std::int32_t& outA, std::int32_t& outB, std::int32_t& outC)
{
    std::string normalized = line.text;
    const std::vector<std::string_view> words = splitWordsReplaceColon(normalized);

    if (words.size() < 8 || !isWord(words[0], "face"))
    {
        throw makeParseError(line, "Expected face record");
    }

    outIndex = parseI32Token(line, words[1]);

    if (!isWord(words[2], "a") || !isWord(words[4], "b") || !isWord(words[6], "c"))
    {
        throw makeParseError(line, "Expected face index keys a b c");
    }

    outA = parseI32Token(line, words[3]);
    outB = parseI32Token(line, words[5]);
    outC = parseI32Token(line, words[7]);
}

void parseTriMesh(Level& level, AscReader& reader, const AscLine& header)
{
    const std::vector<std::string_view> words = sed::text::splitWords(header.text);
    if (words.size() < 5 || !isWord(words[0], "tri-mesh,"))
    {
        throw makeParseError(header, "Expected tri-mesh header");
    }

    if (!isWord(words[1], "vertices:") || !isWord(words[3], "faces:"))
    {
        throw makeParseError(header, "Expected tri-mesh counts: vertices: N faces: M");
    }

    const std::int32_t vertexCount = parseI32Token(header, words[2]);
    const std::int32_t faceCount = parseI32Token(header, words[4]);

    const std::optional<AscLine> vertexList = reader.nextNonEmptyLine();
    if (!vertexList)
    {
        throw makeParseError(header, "Unexpected EOF after tri-mesh header");
    }

    const std::vector<std::string_view> vertexListWords = sed::text::splitWords(vertexList->text);
    if (vertexListWords.empty() || !isWord(vertexListWords[0], "vertex"))
    {
        throw makeParseError(*vertexList, "Expected vertex list header");
    }

    Sector& sector = level.newSector();
    sector.colormap = "dflt.cmp";

    sector.vertices.clear();
    sector.surfaces.clear();

    for (std::int32_t i = 0; i < vertexCount; ++i)
    {
        const std::optional<AscLine> vertexLine = reader.nextNonEmptyLine();
        if (!vertexLine)
        {
            throw makeParseError(*vertexList, "Unexpected EOF in vertex list");
        }

        std::int32_t index{};
        double x{};
        double y{};
        double z{};
        parseVertexLine(*vertexLine, index, x, y, z);
        (void)index;

        sector.newVertex(x, y, z);
    }

    const std::optional<AscLine> faceList = reader.nextNonEmptyLine();
    if (!faceList)
    {
        throw makeParseError(header, "Unexpected EOF after vertices");
    }

    const std::vector<std::string_view> faceListWords = sed::text::splitWords(faceList->text);
    if (faceListWords.empty() || !isWord(faceListWords[0], "face"))
    {
        throw makeParseError(*faceList, "Expected face list header");
    }

    const double ppu = sed::level::levelPixelPerUnit(level);

    std::int32_t facesRead = 0;
    while (facesRead < faceCount)
    {
        const std::optional<AscLine> faceLine = reader.nextNonEmptyLine();
        if (!faceLine)
        {
            break;
        }

        const std::vector<std::string_view> tokens = sed::text::splitWords(faceLine->text);
        if (tokens.empty() || !isWord(tokens[0], "face"))
        {
            continue;
        }

        std::int32_t index{};
        std::int32_t a{};
        std::int32_t b{};
        std::int32_t c{};
        parseFaceLine(*faceLine, index, a, b, c);
        (void)index;

        if (a < 0 || b < 0 || c < 0 || static_cast<std::size_t>(a) >= sector.vertices.size() || static_cast<std::size_t>(b) >= sector.vertices.size()
            || static_cast<std::size_t>(c) >= sector.vertices.size())
        {
            throw makeParseError(*faceLine, "Face references out of range vertex indices");
        }

        Surface surface{};
        surface.adjoinFlags = SAF_Visible | SAF_Move | SAF_AllowSoundPass;
        surface.material = "dflt.mat";
        surface.surfaceFlags = SF_Collision;
        surface.geo = static_cast<std::int32_t>(TGeoMode::Texture);
        surface.light = static_cast<std::int32_t>(TLightMode::Gouraud);
        surface.tex = 1;

        surface.vertices = {a, b, c};
        surface.texVertices = std::vector<TexVertex>(surface.vertices.size());

        sed::level::surfaceRecalcNewAll(ppu, sector, surface, 0);

        surface.num = static_cast<std::int32_t>(sector.surfaces.size());
        sector.surfaces.push_back(std::move(surface));

        ++facesRead;
    }

    for (std::size_t i = 0; i < sector.surfaces.size(); ++i)
    {
        sector.surfaces[i].num = static_cast<std::int32_t>(i);
    }
}

} // namespace

Level loadFromASC(std::istream& in, const AscLoadOptions& options)
{
    Level level{};
    level.clear();
    level.kind = ProjectType::JKDF2;
    level.uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
    setDefaultHeader(level);

    AscReader reader(in);

    while (true)
    {
        const std::optional<AscLine> line = reader.nextNonEmptyLine();
        if (!line)
        {
            break;
        }

        const std::vector<std::string_view> words = sed::text::splitWords(line->text);
        if (words.empty())
        {
            continue;
        }

        if (isWord(words[0], "tri-mesh,"))
        {
            parseTriMesh(level, reader, *line);
        }
    }

    (void)level.newThing();

    for (std::size_t i = 0; i < level.sectors.size(); ++i)
    {
        level.sectors[i].num = static_cast<std::int32_t>(i);
    }

    if (options.resources != nullptr)
    {
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
                "ASC texture coordinates remain in legacy pixel space because "
                "the referenced MAT dimensions could not be resolved"
                    + location + ".");
        }
    }

    return level;
}

Level loadFromASC(
    const std::filesystem::path& path,
    const AscLoadOptions& options)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Failed to open ASC file: " + path.string());
    }
    return loadFromASC(file, options);
}

} // namespace sed::assets
