#include "sed/level/build.hpp"

#include "sed/level/cleave.hpp"
#include "sed/level/geometry.hpp"
#include "exact_orientation.hpp"
#include "geometry_precision.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <istream>
#include <limits>
#include <numbers>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sed::level
{
namespace
{

constexpr double kTemplatePixelsPerUnit = 320.0;
constexpr double kUnitCenter = 0.5;
constexpr double kUnitRadius = 0.5;
constexpr std::int32_t kDefaultPrismSides = 16;
constexpr std::int32_t kDefaultUvSphereSegments = 8;
constexpr std::int32_t kMinPrismSides = 3;
constexpr std::int32_t kMinUvSphereSegments = 4;
constexpr std::int32_t kMaxUvSphereSegments = 96;
constexpr std::int32_t kMinUvSphereRings = 3;
constexpr std::int32_t kMaxUvSphereRings = 48;
constexpr double kGoldenRatio = 1.6180339887498948482;

bool generatedSolidIsValid(const Sector& sector)
{
    if (!precision::closedBoundary(sector) || !sectorIsConvex(sector)) return false;
    for (const Surface& face : sector.surfaces)
    {
        if (!surfaceIsPlanar(sector, face) || !surfaceIsConvex(sector, face)
            || face.texVertices.size() != face.vertices.size()) return false;
        for (const TexVertex& corner : face.texVertices)
            if (!std::isfinite(corner.u) || !std::isfinite(corner.v)) return false;
    }
    return true;
}

bool extrusionLoopsAreSeparated(const SurfaceExtrudePreview& preview) noexcept
{
    if (preview.innerLoop.size() < 3 || preview.innerLoop.size() != preview.outerLoop.size()) return false;
    for (std::size_t corner = 0; corner < preview.innerLoop.size(); ++corner)
    {
        const TVector& inner = preview.innerLoop[corner];
        const TVector& outer = preview.outerLoop[preview.outerLoop.size()-1-corner];
        if (!precision::finite(inner) || !precision::finite(outer) || precision::samePoint(inner, outer)) return false;
    }
    return true;
}

std::optional<UvTextureDimensions> defaultGeneratedSectorTextureDimensions(
    const Surface&) noexcept
{
    return DefaultGeneratedSectorTextureDimensions;
}

bool admitGeneratedPixelUvs(
    Sector& sector,
    const std::int32_t sectorIndex,
    const UvCoordinateSpace destination,
    const UvTextureDimensionsResolver& resolveDimensions)
{
    if (!isValidUvCoordinateSpace(destination))
    {
        return false;
    }
    if (destination == UvCoordinateSpace::TexturePixels)
    {
        return true;
    }

    return static_cast<bool>(convertSectorUvCoordinateSpace(
        sector,
        sectorIndex,
        UvCoordinateSpace::TexturePixels,
        destination,
        resolveDimensions));
}

struct ShapeBounds
{
    TVector min{};
    TVector max{};
};

struct SurfacePlaneBounds
{
    TVector origin{};
    TVector xAxis{};
    TVector yAxis{};
    double minX{};
    double minY{};
    double maxX{};
    double maxY{};
};

/** Applies the editor's default surface values before shape-specific data is filled in. */
void initNewSurface(const ProjectType projectType, Surface& surface)
{
    surface.adjoinFlags = SAF_Visible | SAF_Move;
    if (projectType != ProjectType::IJIM)
    {
        surface.adjoinFlags |= SAF_AllowSoundPass;
    }

    surface.material     = "dflt.mat";
    surface.surfaceFlags = SF_Collision;
    surface.faceFlags    = 0;
    surface.geo          = static_cast<std::int32_t>(TGeoMode::Texture);
    surface.light        = static_cast<std::int32_t>(TLightMode::Gouraud);
    surface.tex          = (projectType == ProjectType::IJIM) ? 3 : 1;

    surface.extraLight = ColorBlack;
    surface.uScale     = 1.0f;
    surface.vScale     = 1.0f;

    surface.adjoin.reset();
    surface.nAdj = -1;
    surface.nMat = -1;
}

/** Copies render/material properties while deliberately dropping adjoin links. */
void assignSurfaceParams(const Surface& src, Surface& dst)
{
    dst.surfaceFlags = src.surfaceFlags;
    dst.faceFlags    = src.faceFlags;
    dst.geo          = src.geo;
    dst.light        = src.light;
    dst.tex          = src.tex;
    dst.extraLight   = src.extraLight;
    dst.adjoinFlags  = src.adjoinFlags;
    dst.material     = src.material;
    dst.uScale       = src.uScale;
    dst.vScale       = src.vScale;

    dst.adjoin.reset();
    dst.nAdj = -1;
    dst.nMat = src.nMat;
}

/** Copies sector-level metadata while leaving vertex/surface arrays to the caller. */
void assignSectorParams(const Sector& src, Sector& dst)
{
    dst.flags       = src.flags;
    dst.ambient     = src.ambient;
    dst.extraLight  = src.extraLight;
    dst.pointLight  = src.pointLight;
    dst.colormap    = src.colormap;
    dst.tint        = src.tint;
    dst.sound       = src.sound;
    dst.soundVolume = src.soundVolume;
    dst.thrust      = src.thrust;
    dst.collideBox  = src.collideBox;
    dst.layer       = src.layer;
    dst.mark        = src.mark;
}

/**
 * Generated primitive shapes are palette-agnostic templates. Once inserted
 * into a JK/MOTS level, the concrete sector needs a real CMP so palette-indexed
 * MATs render with the same colors as the starter sector.
 */
void applyLevelDefaultsToPrimitiveShape(const Level& level, Sector& shape)
{
    if (level.kind != ProjectType::IJIM && shape.colormap.empty())
    {
        shape.colormap = level.getMasterCMP();
    }
}

/** Returns whether a surface reference can be safely dereferenced. */
bool isValidSurfaceRef(const Level& level, const SurfaceRef ref) noexcept
{
    if (ref.sector < 0 || ref.surface < 0)
    {
        return false;
    }

    const std::size_t sectorIndex = static_cast<std::size_t>(ref.sector);
    if (sectorIndex >= level.sectors.size())
    {
        return false;
    }

    const Sector& sector = level.sectors[sectorIndex];
    const std::size_t surfaceIndex = static_cast<std::size_t>(ref.surface);
    return surfaceIndex < sector.surfaces.size();
}

bool isValidVertexRef(const Sector& sector, const std::int32_t vertexIndex) noexcept
{
    return vertexIndex >= 0 && static_cast<std::size_t>(vertexIndex) < sector.vertices.size();
}

TVector vertexPosition(const Vertex& vertex) noexcept
{
    return TVector{ vertex.x, vertex.y, vertex.z };
}

bool isFiniteVector(const TVector& value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

TVector orderedMinCorner(const TVector& a, const TVector& b) noexcept
{
    return TVector{ std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) };
}

TVector orderedMaxCorner(const TVector& a, const TVector& b) noexcept
{
    return TVector{ std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) };
}

TVector extentVector(const TVector& min, const TVector& max) noexcept
{
    return TVector{ max.x - min.x, max.y - min.y, max.z - min.z };
}

double maxExtent(const TVector& extent) noexcept
{
    return std::max({ extent.x, extent.y, extent.z });
}

bool hasPositiveVolume(const TVector& extent) noexcept
{
    return extent.x > std::numeric_limits<double>::epsilon()
        && extent.y > std::numeric_limits<double>::epsilon()
        && extent.z > std::numeric_limits<double>::epsilon();
}

bool areDistinctAxes(const std::array<int, 3>& axes) noexcept
{
    std::array<bool, 3> used{};
    for (const int axis : axes)
    {
        if (axis < 0 || axis > 2 || used[static_cast<std::size_t>(axis)])
        {
            return false;
        }
        used[static_cast<std::size_t>(axis)] = true;
    }
    return true;
}

double axisValue(const TVector& v, const int axis) noexcept
{
    switch (axis)
    {
        case 0: return v.x;
        case 1: return v.y;
        default: return v.z;
    }
}

void setAxis(Vertex& vertex, const int axis, const double value) noexcept
{
    switch (axis)
    {
        case 0:
            vertex.x = value;
            break;
        case 1:
            vertex.y = value;
            break;
        default:
            vertex.z = value;
            break;
    }
}

double normalizeAxisValue(const double value, const double srcMin, const double srcExtent) noexcept
{
    if (std::abs(srcExtent) <= std::numeric_limits<double>::epsilon())
    {
        return kUnitCenter;
    }
    return (value - srcMin) / srcExtent;
}

Surface makeSurfaceFromParams(const Surface& params, std::vector<std::int32_t> vertices)
{
    Surface surface{};
    assignSurfaceParams(params, surface);
    surface.vertices = std::move(vertices);
    surface.texVertices = std::vector<TexVertex>(surface.vertices.size());
    return surface;
}

void recalcSurfaces(const Level& level, Sector& sector, const std::size_t firstSurface)
{
    const double ppu = levelPixelPerUnit(level);
    for (std::size_t i = firstSurface; i < sector.surfaces.size(); ++i)
    {
        surfaceRecalcNewAll(ppu, sector, sector.surfaces[i], 0);
    }
}

void renumberSurfaces(Sector& sector) noexcept
{
    for (std::size_t i = 0; i < sector.surfaces.size(); ++i)
    {
        sector.surfaces[i].num = static_cast<std::int32_t>(i);
    }
}

void addSideSurfacesBetweenLoops(
    Sector& sector,
    const Surface& surfaceParams,
    const std::vector<std::int32_t>& frontLoop,
    const std::vector<std::int32_t>& backLoop)
{
    const std::int32_t vertexCount = static_cast<std::int32_t>(frontLoop.size());
    if (vertexCount <= 0 || frontLoop.size() != backLoop.size())
    {
        return;
    }

    // Step through the loops using the legacy winding order so side normals and
    // vertex indices remain identical to the old cube/extrude construction.
    for (std::int32_t i = 0; i < vertexCount; ++i)
    {
        std::vector<std::int32_t> vertices{};
        vertices.reserve(4);
        vertices.push_back(frontLoop[static_cast<std::size_t>(i)]);
        vertices.push_back(frontLoop[static_cast<std::size_t>((i == 0) ? vertexCount - 1 : i - 1)]);
        vertices.push_back(backLoop[static_cast<std::size_t>((i == 0) ? 0 : vertexCount - i)]);
        vertices.push_back(backLoop[static_cast<std::size_t>(vertexCount - i - 1)]);

        sector.addSurface(makeSurfaceFromParams(surfaceParams, std::move(vertices)));
    }
}

std::optional<std::vector<TVector>> collectSurfaceVertexPositions(const Sector& sector, const Surface& surface)
{
    std::vector<TVector> positions{};
    positions.reserve(surface.vertices.size());

    for (const std::int32_t vertexIndex : surface.vertices)
    {
        if (!isValidVertexRef(sector, vertexIndex))
        {
            return std::nullopt;
        }

        positions.push_back(vertexPosition(sector.vertices[static_cast<std::size_t>(vertexIndex)]));
    }

    return positions;
}

std::optional<TVector> surfacePlaneXAxis(const std::vector<TVector>& vertices) noexcept
{
    if (vertices.size() < 2)
    {
        return std::nullopt;
    }

    const TVector& origin = vertices.front();
    for (std::size_t i = 1; i < vertices.size(); ++i)
    {
        const TVector axis = vertices[i] - origin;
        const double length = std::hypot(axis.x, axis.y, axis.z);
        if ( length > 0.0 && std::isfinite(length) )
        {
            return TVector{axis.x / length, axis.y / length, axis.z / length};
        }
    }

    return std::nullopt;
}

std::optional<SurfacePlaneBounds> surfacePlaneBounds(
    const std::vector<TVector>& vertices,
    const TVector& normal) noexcept
{
    std::optional<TVector> xAxis = surfacePlaneXAxis(vertices);
    if (!xAxis)
    {
        return std::nullopt;
    }

    // The frame is a plane basis, even when imported edge endpoints carry
    // inherited height rounding. Keep expansion and contextual portal lifting
    // in the same immutable normal frame.
    *xAxis = *xAxis - normal.scaled(xAxis->dot(normal));
    const double axisLength = std::hypot(xAxis->x, xAxis->y, xAxis->z);
    if ( !(axisLength > 0.0) || !std::isfinite(axisLength) )
    {
        return std::nullopt;
    }
    *xAxis = {xAxis->x / axisLength, xAxis->y / axisLength, xAxis->z / axisLength};
    TVector yAxis = normal.cross(*xAxis);
    if (yAxis.normalize() <= std::numeric_limits<double>::epsilon())
    {
        return std::nullopt;
    }

    SurfacePlaneBounds bounds{};
    bounds.origin = vertices.front();
    bounds.xAxis = *xAxis;
    bounds.yAxis = yAxis;
    bounds.minX = std::numeric_limits<double>::infinity();
    bounds.minY = std::numeric_limits<double>::infinity();
    bounds.maxX = -std::numeric_limits<double>::infinity();
    bounds.maxY = -std::numeric_limits<double>::infinity();

    for (const TVector& vertex : vertices)
    {
        const TVector delta = vertex - bounds.origin;
        const double x = delta.dot(bounds.xAxis);
        const double y = delta.dot(bounds.yAxis);

        bounds.minX = std::min(bounds.minX, x);
        bounds.minY = std::min(bounds.minY, y);
        bounds.maxX = std::max(bounds.maxX, x);
        bounds.maxY = std::max(bounds.maxY, y);
    }

    if (!std::isfinite(bounds.minX) || !std::isfinite(bounds.minY) || !std::isfinite(bounds.maxX) || !std::isfinite(bounds.maxY))
    {
        return std::nullopt;
    }

    return bounds;
}

TVector surfacePlanePoint(const SurfacePlaneBounds& bounds, const double x, const double y) noexcept
{
    return bounds.origin + bounds.xAxis.scaled(x) + bounds.yAxis.scaled(y);
}

bool buildExpandedExtrudeRectangle(
    const std::vector<TVector>& sourceVertices,
    const TVector& normal,
    const double length,
    const double expand,
    SurfaceExtrudePreview& preview)
{
    const std::optional<SurfacePlaneBounds> bounds = surfacePlaneBounds(sourceVertices, normal);
    if (!bounds)
    {
        return false;
    }

    const double minX = bounds->minX - expand;
    const double minY = bounds->minY - expand;
    const double maxX = bounds->maxX + expand;
    const double maxY = bounds->maxY + expand;
    if (!(maxX > minX) || !(maxY > minY))
    {
        return false;
    }

    // Step 1: build the inner rectangle in the same winding as JED's
    // ExtrudeAndExpandSurface, giving it the opposite normal of the source.
    std::array<TVector, 4> inner{
        surfacePlanePoint(*bounds, minX, minY),
        surfacePlanePoint(*bounds, minX, maxY),
        surfacePlanePoint(*bounds, maxX, maxY),
        surfacePlanePoint(*bounds, maxX, minY),
    };

    preview.innerLoop.assign(inner.begin(), inner.end());
    preview.outerLoop.clear();
    preview.outerLoop.reserve(inner.size());

    // Step 2: reverse the rectangle for the outer face so it faces away from
    // the source, and offset that face by the requested extrusion length.
    const TVector extrusion = normal.scaled(-length);
    for (auto it = inner.rbegin(); it != inner.rend(); ++it)
    {
        preview.outerLoop.push_back(*it + extrusion);
    }

    return true;
}

std::string stripCommentAndTrim(std::string line)
{
    if (const std::size_t comment = line.find('#'); comment != std::string::npos)
    {
        line.resize(comment);
    }

    const auto first = std::find_if_not(line.begin(), line.end(), [](const unsigned char ch)
        {
            return std::isspace(ch) != 0;
        });
    const auto last = std::find_if_not(line.rbegin(), line.rend(), [](const unsigned char ch)
        {
            return std::isspace(ch) != 0;
        }).base();

    if (first >= last)
    {
        return {};
    }

    return std::string(first, last);
}

std::vector<std::string> splitWords(const std::string& line)
{
    std::istringstream iss(line);
    std::vector<std::string> words{};
    std::string word{};
    while (iss >> word)
    {
        words.push_back(word);
    }
    return words;
}

std::optional<double> parseDouble(const std::string_view text) noexcept
{
    try
    {
        std::size_t used = 0;
        const double value = std::stod(std::string(text), &used);
        return (used > 0) ? std::optional<double>(value) : std::nullopt;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

bool parseTemplateUvFloat(
    const std::string_view text,
    const float fallback,
    float& destination) noexcept
{
    const std::optional<double> value = parseDouble(text);
    if (!value)
    {
        destination = fallback;
        return true;
    }

    constexpr double maxFloat =
        static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(*value) || *value < -maxFloat || *value > maxFloat)
    {
        return false;
    }
    destination = static_cast<float>(*value);
    return true;
}

std::optional<std::int32_t> parseInt(const std::string_view text) noexcept
{
    try
    {
        std::size_t used = 0;
        const long value = std::stol(std::string(text), &used, 10);
        if (used == 0)
        {
            return std::nullopt;
        }
        return static_cast<std::int32_t>(value);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<std::uint32_t> parseHex(const std::string_view text) noexcept
{
    try
    {
        std::size_t used = 0;
        const unsigned long value = std::stoul(std::string(text), &used, 16);
        if (used == 0)
        {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(value);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

bool readMeaningfulLine(std::istream& input, std::string& out)
{
    std::string line{};
    while (std::getline(input, line))
    {
        out = stripCommentAndTrim(line);
        if (!out.empty())
        {
            return true;
        }
    }
    return false;
}

ColorF prefabIntensityColor(const std::string_view text, const float fallback = 0.0f) noexcept
{
    return intensityToColor(static_cast<float>(parseDouble(text).value_or(fallback)));
}

ColorF prefabSurfaceExtraLight(const std::string_view text, const float fallback = 0.0f) noexcept
{
    return signedIntensityToColor(static_cast<float>(parseDouble(text).value_or(fallback)));
}

std::string templateShapeNameFromHeader(const std::string& line, const std::size_t shapeCount)
{
    std::string name{};
    const std::size_t secPos = line.find("SEC");
    if (secPos != std::string::npos)
    {
        name = stripCommentAndTrim(line.substr(secPos + 3));
    }
    if (name.empty())
    {
        name = "Shape " + std::to_string(shapeCount + 1);
    }
    return name;
}

std::optional<UvCoordinateSpace> parseTemplateUvCoordinateSpace(
    const std::vector<std::string>& tokens) noexcept
{
    if (tokens.size() <= 10 || tokens[10] != "UV")
    {
        return UvCoordinateSpace::TexturePixels;
    }
    if (tokens.size() < 12)
    {
        return std::nullopt;
    }
    if (tokens[11] == "normalized")
    {
        return UvCoordinateSpace::Normalized;
    }
    if (tokens[11] == "pixels")
    {
        return UvCoordinateSpace::TexturePixels;
    }

    return std::nullopt;
}

bool parseTemplateSectorSettings(
    const std::vector<std::string>& tokens,
    Sector& sector,
    UvCoordinateSpace& uvCoordinateSpace)
{
    const std::optional<UvCoordinateSpace> parsedUvCoordinateSpace =
        parseTemplateUvCoordinateSpace(tokens);
    if (!parsedUvCoordinateSpace)
    {
        return false;
    }
    uvCoordinateSpace = *parsedUvCoordinateSpace;

    if (tokens.size() < 10)
    {
        return true;
    }

    sector.colormap = (tokens[0] == "-") ? std::string{} : tokens[0];
    sector.sound = (tokens[1] == "-") ? std::string{} : tokens[1];
    sector.soundVolume = parseDouble(tokens[2]).value_or(0.0);
    sector.flags = static_cast<std::int32_t>(parseHex(tokens[3]).value_or(0));
    sector.ambient = prefabIntensityColor(tokens[4], 1.0f);
    sector.extraLight = prefabIntensityColor(tokens[5], 0.0f);
    sector.tint.r = static_cast<float>(parseDouble(tokens[6]).value_or(0.0));
    sector.tint.g = static_cast<float>(parseDouble(tokens[7]).value_or(0.0));
    sector.tint.b = static_cast<float>(parseDouble(tokens[8]).value_or(0.0));
    sector.tint.a = 1.0f;
    sector.layer = parseInt(tokens[9]).value_or(0);
    return true;
}

void readTemplateVertices(std::istream& input, const std::int32_t vertexCount, Sector& sector)
{
    std::string line{};
    for (std::int32_t i = 0; i < vertexCount; ++i)
    {
        if (!readMeaningfulLine(input, line))
        {
            break;
        }

        const std::vector<std::string> tokens = splitWords(line);
        if (tokens.size() < 3)
        {
            continue;
        }

        sector.newVertex(
            parseDouble(tokens[0]).value_or(0.0),
            parseDouble(tokens[1]).value_or(0.0),
            parseDouble(tokens[2]).value_or(0.0));
    }
}

struct TemplateSurfaceHeader
{
    Surface surface{};
    std::int32_t vertexCount = 0;
};

std::optional<TemplateSurfaceHeader> parseTemplateSurfaceHeader(
    const std::vector<std::string>& tokens,
    const std::int32_t surfaceIndex)
{
    if (tokens.size() < 11)
    {
        return std::nullopt;
    }

    TemplateSurfaceHeader parsed{};
    Surface& surface = parsed.surface;
    surface.num = surfaceIndex;
    surface.material = (tokens[0] == "-") ? std::string{} : tokens[0];
    surface.surfaceFlags = static_cast<std::int32_t>(parseHex(tokens[1]).value_or(0));
    surface.faceFlags = parseHex(tokens[2]).value_or(0);
    surface.geo = parseInt(tokens[3]).value_or(0);
    surface.light = parseInt(tokens[4]).value_or(0);
    surface.tex = parseInt(tokens[5]).value_or(0);
    surface.extraLight = prefabSurfaceExtraLight(tokens[6], 0.0f);
    parsed.vertexCount = parseInt(tokens[7]).value_or(0);
    const std::int32_t adjoinSector = parseInt(tokens[8]).value_or(-1);
    const std::int32_t adjoinSurface = parseInt(tokens[9]).value_or(-1);
    surface.adjoinFlags = static_cast<std::int32_t>(parseHex(tokens[10]).value_or(0));
    surface.uScale = 1.0f;
    surface.vScale = 1.0f;
    if ((tokens.size() >= 12
            && !parseTemplateUvFloat(tokens[11], 1.0f, surface.uScale))
        || (tokens.size() >= 13
            && !parseTemplateUvFloat(tokens[12], 1.0f, surface.vScale)))
    {
        return std::nullopt;
    }
    surface.nAdj = -1;
    surface.nMat = -1;
    if (adjoinSector >= 0 && adjoinSurface >= 0)
    {
        surface.adjoin = SurfaceRef{ adjoinSector, adjoinSurface };
    }
    else
    {
        surface.adjoin.reset();
    }

    return parsed;
}

bool readTemplateSurfaceVertices(
    std::istream& input,
    Surface& surface,
    const std::int32_t surfaceVertexCount)
{
    std::string line{};
    if (!readMeaningfulLine(input, line))
    {
        return false;
    }

    const std::vector<std::string> vertexTokens = splitWords(line);
    surface.vertices.reserve(static_cast<std::size_t>(surfaceVertexCount));
    surface.texVertices.reserve(static_cast<std::size_t>(surfaceVertexCount));

    // Step 1: read vertex index, U, V, and alpha tuples.
    for (std::int32_t v = 0; v < surfaceVertexCount; ++v)
    {
        const std::size_t base = static_cast<std::size_t>(v) * 4u;
        if (base + 3 >= vertexTokens.size())
        {
            break;
        }

        TexVertex tv{};
        surface.vertices.push_back(parseInt(vertexTokens[base + 0]).value_or(-1));
        if (!parseTemplateUvFloat(vertexTokens[base + 1], 0.0f, tv.u)
            || !parseTemplateUvFloat(vertexTokens[base + 2], 0.0f, tv.v))
        {
            return false;
        }
        tv.color.a = static_cast<float>(parseDouble(vertexTokens[base + 3]).value_or(1.0));
        tv.color.r = tv.color.a;
        tv.color.g = tv.color.a;
        tv.color.b = tv.color.a;
        surface.texVertices.push_back(tv);
    }

    // Step 2: if present, read explicit RGB triples after the alpha tuples.
    const std::size_t rgbStart = static_cast<std::size_t>(surfaceVertexCount) * 4u;
    for (std::int32_t v = 0; v < surfaceVertexCount; ++v)
    {
        const std::size_t base = rgbStart + static_cast<std::size_t>(v) * 3u;
        if (base + 2 >= vertexTokens.size() || static_cast<std::size_t>(v) >= surface.texVertices.size())
        {
            break;
        }

        TexVertex& tv = surface.texVertices[static_cast<std::size_t>(v)];
        tv.color.r = static_cast<float>(parseDouble(vertexTokens[base + 0]).value_or(tv.color.r));
        tv.color.g = static_cast<float>(parseDouble(vertexTokens[base + 1]).value_or(tv.color.g));
        tv.color.b = static_cast<float>(parseDouble(vertexTokens[base + 2]).value_or(tv.color.b));
    }
    return true;
}

bool readTemplateSurfaces(
    std::istream& input,
    const std::int32_t surfaceCount,
    Sector& sector)
{
    std::string line{};
    for (std::int32_t sf = 0; sf < surfaceCount; ++sf)
    {
        if (!readMeaningfulLine(input, line))
        {
            return false;
        }

        std::optional<TemplateSurfaceHeader> parsed = parseTemplateSurfaceHeader(splitWords(line), sf);
        if (!parsed)
        {
            return false;
        }

        Surface surface = std::move(parsed->surface);
        if (!readTemplateSurfaceVertices(
                input, surface, parsed->vertexCount))
        {
            return false;
        }
        if (surface.vertices.size()
                != static_cast<std::size_t>(parsed->vertexCount)
            || surface.texVertices.size() != surface.vertices.size())
        {
            return false;
        }
        surface.normal = surfaceCalcNormalNewell(sector, surface);
        sector.addSurface(std::move(surface));
    }
    return true;
}

void consumeTemplateShapeEnd(std::istream& input)
{
    std::string line{};
    while (readMeaningfulLine(input, line))
    {
        const std::vector<std::string> tokens = splitWords(line);
        if (!tokens.empty() && tokens.front() == "END")
        {
            break;
        }
    }
}

bool finalizeLoadedTemplateSector(Sector& sector) noexcept
{
    if (sector.vertices.empty() || sector.surfaces.empty())
    {
        return false;
    }

    for (std::size_t i = 0; i < sector.surfaces.size(); ++i)
    {
        Surface& surface = sector.surfaces[i];
        surface.num = static_cast<std::int32_t>(i);
        surface.normal = surfaceCalcNormalNewell(sector, surface);
    }
    return true;
}

void renumberAndRecalcTemplateSector(Sector& sector)
{
    const TVector center = sectorCalcCenter(sector);

    for (std::size_t i = 0; i < sector.surfaces.size(); ++i)
    {
        Surface& surface = sector.surfaces[i];
        surface.num = static_cast<std::int32_t>(i);
        if (surface.texVertices.size() != surface.vertices.size())
        {
            surface.texVertices = std::vector<TexVertex>(surface.vertices.size());
        }

        surface.normal = surfaceCalcNormalNewell(sector, surface);

        TVector surfaceCenter{};
        if (surfaceCalcCenter(sector, surface, surfaceCenter))
        {
            const TVector inward = center - surfaceCenter;
            if (surface.normal.Dot(inward) < 0.0)
            {
                std::reverse(surface.vertices.begin(), surface.vertices.end());
                std::reverse(surface.texVertices.begin(), surface.texVertices.end());
            }
        }

        surfaceRecalcNewAll(kTemplatePixelsPerUnit, sector, surface, 0);
    }
}

void addTemplateSurface(
    const ProjectType projectType,
    Sector& sector,
    std::vector<std::int32_t> vertices)
{
    Surface surface{};
    initNewSurface(projectType, surface);
    surface.vertices = std::move(vertices);
    surface.texVertices = std::vector<TexVertex>(surface.vertices.size());
    sector.addSurface(std::move(surface));
}

Sector makeBoxShape(const ProjectType projectType)
{
    Sector sector{};
    const std::int32_t v0 = sector.newVertex(0.0, 0.0, 0.0);
    const std::int32_t v1 = sector.newVertex(1.0, 0.0, 0.0);
    const std::int32_t v2 = sector.newVertex(1.0, 1.0, 0.0);
    const std::int32_t v3 = sector.newVertex(0.0, 1.0, 0.0);
    const std::int32_t v4 = sector.newVertex(0.0, 0.0, 1.0);
    const std::int32_t v5 = sector.newVertex(1.0, 0.0, 1.0);
    const std::int32_t v6 = sector.newVertex(1.0, 1.0, 1.0);
    const std::int32_t v7 = sector.newVertex(0.0, 1.0, 1.0);

    addTemplateSurface(projectType, sector, { v0, v1, v2, v3 });
    addTemplateSurface(projectType, sector, { v7, v6, v5, v4 });

    // Preserve Pascal CreateCubeSec's side order and starting corner. Cyclically
    // rotating these otherwise equivalent loops changes the default UV anchor,
    // making newly drawn walls appear rotated relative to classic SED cubes.
    addTemplateSurface(projectType, sector, { v0, v3, v7, v4 });
    addTemplateSurface(projectType, sector, { v1, v0, v4, v5 });
    addTemplateSurface(projectType, sector, { v2, v1, v5, v6 });
    addTemplateSurface(projectType, sector, { v3, v2, v6, v7 });

    renumberAndRecalcTemplateSector(sector);
    return sector;
}

Sector makePrismShape(
    const ProjectType projectType,
    const std::int32_t sides,
    const double topScale,
    const bool pointTop)
{
    const std::int32_t clampedSides = std::max<std::int32_t>(kMinPrismSides, sides);
    Sector sector{};
    sector.vertices.reserve(static_cast<std::size_t>(clampedSides) * 2u + (pointTop ? 1u : 0u));

    std::vector<std::int32_t> bottom{};
    std::vector<std::int32_t> top{};
    bottom.reserve(static_cast<std::size_t>(clampedSides));
    top.reserve(static_cast<std::size_t>(clampedSides));

    for (std::int32_t i = 0; i < clampedSides; ++i)
    {
        const double angle = (2.0 * std::numbers::pi_v<double> * static_cast<double>(i)) / static_cast<double>(clampedSides);
        const double x = kUnitCenter + kUnitRadius * std::cos(angle);
        const double y = kUnitCenter + kUnitRadius * std::sin(angle);
        bottom.push_back(sector.newVertex(x, y, 0.0));
    }

    std::int32_t apex = -1;
    if (pointTop)
    {
        apex = sector.newVertex(kUnitCenter, kUnitCenter, 1.0);
    }
    else
    {
        for (std::int32_t i = 0; i < clampedSides; ++i)
        {
            const double angle = (2.0 * std::numbers::pi_v<double> * static_cast<double>(i)) / static_cast<double>(clampedSides);
            const double x = kUnitCenter + (kUnitRadius * topScale) * std::cos(angle);
            const double y = kUnitCenter + (kUnitRadius * topScale) * std::sin(angle);
            top.push_back(sector.newVertex(x, y, 1.0));
        }
    }

    addTemplateSurface(projectType, sector, bottom);

    if (!pointTop)
    {
        std::vector<std::int32_t> topSurface = top;
        std::reverse(topSurface.begin(), topSurface.end());
        addTemplateSurface(projectType, sector, std::move(topSurface));
    }

    for (std::int32_t i = 0; i < clampedSides; ++i)
    {
        const std::int32_t next = (i + 1) % clampedSides;
        if (pointTop)
        {
            addTemplateSurface(projectType, sector, { bottom[static_cast<std::size_t>(i)], bottom[static_cast<std::size_t>(next)], apex });
        }
        else
        {
            addTemplateSurface(
                projectType,
                sector,
                {
                    bottom[static_cast<std::size_t>(i)],
                    bottom[static_cast<std::size_t>(next)],
                    top[static_cast<std::size_t>(next)],
                    top[static_cast<std::size_t>(i)]
                });
        }
    }

    renumberAndRecalcTemplateSector(sector);
    return sector;
}

Sector makeUvSphereShape(const ProjectType projectType, const std::int32_t requestedSegments)
{
    Sector sector{};
    const std::int32_t segments = std::clamp<std::int32_t>(requestedSegments, kMinUvSphereSegments, kMaxUvSphereSegments);
    const std::int32_t rings = std::clamp<std::int32_t>(segments / 2, kMinUvSphereRings, kMaxUvSphereRings);

    std::vector<std::vector<std::int32_t>> ringVertices{};
    ringVertices.resize(static_cast<std::size_t>(rings + 1));

    for (std::int32_t r = 0; r <= rings; ++r)
    {
        const double phi = std::numbers::pi_v<double> * static_cast<double>(r) / static_cast<double>(rings);
        const double z = kUnitCenter + kUnitRadius * std::cos(phi);
        const double radius = kUnitRadius * std::sin(phi);

        if (r == 0 || r == rings)
        {
            ringVertices[static_cast<std::size_t>(r)].push_back(sector.newVertex(kUnitCenter, kUnitCenter, z));
            continue;
        }

        for (std::int32_t s = 0; s < segments; ++s)
        {
            const double theta = 2.0 * std::numbers::pi_v<double> * static_cast<double>(s) / static_cast<double>(segments);
            ringVertices[static_cast<std::size_t>(r)].push_back(sector.newVertex(
                kUnitCenter + radius * std::cos(theta),
                kUnitCenter + radius * std::sin(theta),
                z));
        }
    }

    const std::int32_t top = ringVertices.front().front();
    const std::int32_t bottom = ringVertices.back().front();

    for (std::int32_t s = 0; s < segments; ++s)
    {
        const std::int32_t next = (s + 1) % segments;
        addTemplateSurface(
            projectType,
            sector,
            { top, ringVertices[1][static_cast<std::size_t>(s)], ringVertices[1][static_cast<std::size_t>(next)] });
    }

    for (std::int32_t r = 1; r < rings - 1; ++r)
    {
        for (std::int32_t s = 0; s < segments; ++s)
        {
            const std::int32_t next = (s + 1) % segments;
            addTemplateSurface(
                projectType,
                sector,
                {
                    ringVertices[static_cast<std::size_t>(r)][static_cast<std::size_t>(s)],
                    ringVertices[static_cast<std::size_t>(r)][static_cast<std::size_t>(next)],
                    ringVertices[static_cast<std::size_t>(r + 1)][static_cast<std::size_t>(next)],
                    ringVertices[static_cast<std::size_t>(r + 1)][static_cast<std::size_t>(s)]
                });
        }
    }

    for (std::int32_t s = 0; s < segments; ++s)
    {
        const std::int32_t next = (s + 1) % segments;
        addTemplateSurface(
            projectType,
            sector,
            { ringVertices[static_cast<std::size_t>(rings - 1)][static_cast<std::size_t>(s)], bottom, ringVertices[static_cast<std::size_t>(rings - 1)][static_cast<std::size_t>(next)] });
    }

    renumberAndRecalcTemplateSector(sector);
    return sector;
}

Sector makeIcoSphereShape(const ProjectType projectType)
{
    Sector sector{};
    const std::array<TVector, 12> raw{ {
        TVector{ -1.0,  kGoldenRatio,  0.0 },
        TVector{  1.0,  kGoldenRatio,  0.0 },
        TVector{ -1.0, -kGoldenRatio,  0.0 },
        TVector{  1.0, -kGoldenRatio,  0.0 },
        TVector{  0.0, -1.0,  kGoldenRatio },
        TVector{  0.0,  1.0,  kGoldenRatio },
        TVector{  0.0, -1.0, -kGoldenRatio },
        TVector{  0.0,  1.0, -kGoldenRatio },
        TVector{  kGoldenRatio,  0.0, -1.0 },
        TVector{  kGoldenRatio,  0.0,  1.0 },
        TVector{ -kGoldenRatio,  0.0, -1.0 },
        TVector{ -kGoldenRatio,  0.0,  1.0 },
    } };

    for (const TVector& v : raw)
    {
        const TVector n = v.Normalized();
        sector.newVertex(kUnitCenter + n.x * kUnitRadius, kUnitCenter + n.y * kUnitRadius, kUnitCenter + n.z * kUnitRadius);
    }

    const std::array<std::array<std::int32_t, 3>, 20> faces{ {
        {{0, 11, 5}}, {{0, 5, 1}}, {{0, 1, 7}}, {{0, 7, 10}}, {{0, 10, 11}},
        {{1, 5, 9}}, {{5, 11, 4}}, {{11, 10, 2}}, {{10, 7, 6}}, {{7, 1, 8}},
        {{3, 9, 4}}, {{3, 4, 2}}, {{3, 2, 6}}, {{3, 6, 8}}, {{3, 8, 9}},
        {{4, 9, 5}}, {{2, 4, 11}}, {{6, 2, 10}}, {{8, 6, 7}}, {{9, 8, 1}},
    } };

    for (const auto& face : faces)
    {
        addTemplateSurface(projectType, sector, { face[0], face[1], face[2] });
    }

    renumberAndRecalcTemplateSector(sector);
    return sector;
}

std::optional<ShapeBounds> sectorShapeBounds(const Sector& sector) noexcept
{
    if (sector.vertices.empty())
    {
        return std::nullopt;
    }

    ShapeBounds bounds{};
    bounds.min = TVector{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    };
    bounds.max = TVector{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };

    for (const Vertex& vertex : sector.vertices)
    {
        bounds.min.x = std::min(bounds.min.x, vertex.x);
        bounds.min.y = std::min(bounds.min.y, vertex.y);
        bounds.min.z = std::min(bounds.min.z, vertex.z);
        bounds.max.x = std::max(bounds.max.x, vertex.x);
        bounds.max.y = std::max(bounds.max.y, vertex.y);
        bounds.max.z = std::max(bounds.max.z, vertex.z);
    }

    if (!isFiniteVector(bounds.min) || !isFiniteVector(bounds.max))
    {
        return std::nullopt;
    }
    return bounds;
}

void prepareShapeDestination(const Sector& shape, Sector& dst)
{
    assignSectorParams(shape, dst);
    dst.vertices.clear();
    dst.surfaces.clear();
}

bool copyShapeSurfaces(const Sector& shape, Sector& dst)
{
    TVector sectorCenter{};
    for (const Vertex& vertex : dst.vertices)
    {
        sectorCenter.x += vertex.x;
        sectorCenter.y += vertex.y;
        sectorCenter.z += vertex.z;
    }
    if (!dst.vertices.empty())
    {
        const double inverseCount = 1.0 / static_cast<double>(dst.vertices.size());
        sectorCenter = sectorCenter * inverseCount;
    }

    dst.surfaces.reserve(shape.surfaces.size());
    for (const Surface& surface : shape.surfaces)
    {
        Surface copy = surface;
        copy.num = static_cast<std::int32_t>(dst.surfaces.size());
        copy.mark = 0;
        copy.adjoin.reset();
        copy.nAdj = -1;
        if (copy.texVertices.size() != copy.vertices.size())
        {
            return false;
        }
        copy.normal = surfaceCalcNormalNewell(dst, copy);

        // Axis remapping can contain a reflection (for example local Y/Z to
        // the editor's Y construction plane).  Reusing the source winding
        // through an odd permutation turns an inward-facing room into an
        // outward-facing solid.  Judge the mapped polygon in destination
        // space and restore the sector convention: normals point toward the
        // volume interior.  UV tuples must follow their vertices when the
        // loop is reversed.
        TVector surfaceCenter{};
        if (surfaceCalcCenter(dst, copy, surfaceCenter)
            && copy.normal.Dot(sectorCenter - surfaceCenter) < 0.0)
        {
            std::reverse(copy.vertices.begin(), copy.vertices.end());
            std::reverse(copy.texVertices.begin(), copy.texVertices.end());
            copy.normal = surfaceCalcNormalNewell(dst, copy);
        }
        dst.surfaces.push_back(std::move(copy));
    }
    return true;
}

} // namespace

namespace
{

void buildCubeSectorPixelUvs(
    Level& level,
    Sector& sector,
    const double x,
    const double y,
    const double z,
    const TVector& planeNormal,
    const TVector& edge) noexcept
{
    Surface asurf{};
    initNewSurface(level.kind, asurf);

    // Step 1: build the starting face from the requested origin, plane, and edge.
    const TVector enormal = planeNormal.Cross(edge).Normalized();
    const double a = edge.Length();

    const std::int32_t v0 = precision::addVertex(sector, {x, y, z});
    const std::int32_t v1 = precision::addVertex(sector, {x + edge.x, y + edge.y, z + edge.z});
    const std::int32_t v2 = precision::addVertex(sector, {x + edge.x + a * enormal.x, y + edge.y + a * enormal.y, z + edge.z + a * enormal.z});
    const std::int32_t v3 = precision::addVertex(sector, {x + a * enormal.x, y + a * enormal.y, z + a * enormal.z});

    asurf.vertices    = { v0, v1, v2, v3 };
    asurf.texVertices = std::vector<TexVertex>(asurf.vertices.size());

    const std::int32_t asurfIndex = sector.addSurface(std::move(asurf));
    surfaceRecalcNewAll(levelPixelPerUnit(level), sector, sector.surfaces[static_cast<std::size_t>(asurfIndex)], 0);

    // Step 2: snapshot the starting face after UV/normal recalculation.
    const Surface& asurfRef = sector.surfaces[static_cast<std::size_t>(asurfIndex)];
    const std::vector<std::int32_t> asurfVertices = asurfRef.vertices;
    const TVector asurfNormal = asurfRef.normal;

    Surface surfaceParams{};
    assignSurfaceParams(asurfRef, surfaceParams);

    Surface osurf{};
    assignSurfaceParams(surfaceParams, osurf);
    osurf.vertices.reserve(asurfVertices.size());

    // Step 3: create the opposite face by offsetting the starting loop along
    // its normal and reversing the loop so the face points outward.
    for (auto it = asurfVertices.rbegin(); it != asurfVertices.rend(); ++it)
    {
        const std::int32_t vi = *it;
        if (!isValidVertexRef(sector, vi))
        {
            continue;
        }

        const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
        const std::int32_t newVx = precision::addVertex(sector, {
            v.x + asurfNormal.x * a,
            v.y + asurfNormal.y * a,
            v.z + asurfNormal.z * a});
        osurf.vertices.push_back(newVx);
    }

    osurf.texVertices = std::vector<TexVertex>(osurf.vertices.size());
    const std::int32_t osurfIndex = sector.addSurface(std::move(osurf));

    const std::vector<std::int32_t> osurfVertices = sector.surfaces[static_cast<std::size_t>(osurfIndex)].vertices;

    // Step 4: add the side faces that connect the starting and opposite loops.
    addSideSurfacesBetweenLoops(sector, surfaceParams, asurfVertices, osurfVertices);

    // Step 5: refresh generated normals/UVs and maintain stable surface numbers.
    recalcSurfaces(level, sector, 1);
    renumberSurfaces(sector);
}

} // namespace

bool levelCreateCubeSec(
    Level& level,
    Sector& sector,
    const double x,
    const double y,
    const double z,
    const TVector& planeNormal,
    const TVector& edge,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept
{
    if (!isValidUvCoordinateSpace(level.uvCoordinateSpace))
    {
        return false;
    }

    Sector candidate = sector;
    buildCubeSectorPixelUvs(
        level, candidate, x, y, z, planeNormal, edge);
    if (!generatedSolidIsValid(candidate)) return false;
    if (!admitGeneratedPixelUvs(
            candidate,
            candidate.num,
            level.uvCoordinateSpace,
            resolveDimensions))
    {
        return false;
    }

    sector = std::move(candidate);
    return true;
}

void levelCreateCubeSec(
    Level& level,
    Sector& sector,
    const double x,
    const double y,
    const double z,
    const TVector& planeNormal,
    const TVector& edge) noexcept
{
    (void)levelCreateCubeSec(
        level,
        sector,
        x,
        y,
        z,
        planeNormal,
        edge,
        defaultGeneratedSectorTextureDimensions);
}

Sector& levelCreateCube(
    Level& level,
    const double x,
    const double y,
    const double z,
    const TVector& planeNormal,
    const TVector& edge)
{
    const std::optional<std::int32_t> sectorIndex = levelCreateCube(
        level,
        x,
        y,
        z,
        planeNormal,
        edge,
        defaultGeneratedSectorTextureDimensions);
    if (!sectorIndex)
    {
        throw std::runtime_error(
            "Cannot create a Sector with the Level's current UV coordinate space.");
    }
    return level.sectors[static_cast<std::size_t>(*sectorIndex)];
}

std::optional<std::int32_t> levelCreateCube(
    Level& level,
    const double x,
    const double y,
    const double z,
    const TVector& planeNormal,
    const TVector& edge,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept
{
    if ( !level.canAllocateIds(1)
        || !isValidUvCoordinateSpace(level.uvCoordinateSpace) )
    {
        return std::nullopt;
    }

    Level candidate = level;
    Sector& sector = candidate.newSector();
    if (!levelCreateCubeSec(
            candidate,
            sector,
            x,
            y,
            z,
            planeNormal,
            edge,
            resolveDimensions))
    {
        return std::nullopt;
    }

    const std::int32_t sectorIndex = sector.num;
    level = std::move(candidate);
    return sectorIndex;
}

int defaultPrimitiveSides(const SectorPrimitive primitive) noexcept
{
    switch (primitive)
    {
        case SectorPrimitive::Cone:
        case SectorPrimitive::Cylinder:
            return kDefaultPrismSides;
        case SectorPrimitive::UvSphere:
            return kDefaultUvSphereSegments;
        default:
            return 0;
    }
}

Sector makeSectorPrimitiveShape(const ProjectType projectType, const SectorPrimitive primitive)
{
    return makeSectorPrimitiveShape(projectType, primitive, defaultPrimitiveSides(primitive));
}

Sector makeSectorPrimitiveShape(const ProjectType projectType, const SectorPrimitive primitive, const int sides)
{
    switch (primitive)
    {
        case SectorPrimitive::Cube:
        {
            return makeBoxShape(projectType);
        }
        case SectorPrimitive::Cone:
        {
            return makePrismShape(projectType, sides, 0.0, true);
        }
        case SectorPrimitive::Cylinder:
        {
            return makePrismShape(projectType, sides, 1.0, false);
        }
        case SectorPrimitive::UvSphere:
        {
            return makeUvSphereShape(projectType, sides);
        }
        case SectorPrimitive::IcoSphere:
        {
            return makeIcoSphereShape(projectType);
        }
    }

    return makePrismShape(projectType, 4, 1.0, false);
}

std::vector<SectorShape> loadSectorShapesTpl(std::istream& input)
{
    std::vector<SectorShape> shapes{};
    std::string line{};

    while (readMeaningfulLine(input, line))
    {
        // Step 1: find the next shape header.
        const std::vector<std::string> header = splitWords(line);
        if (header.empty() || header[0] != "SEC")
        {
            continue;
        }

        std::string name = templateShapeNameFromHeader(line, shapes.size());

        // Step 2: parse sector metadata.
        Sector sector{};
        UvCoordinateSpace uvCoordinateSpace = UvCoordinateSpace::TexturePixels;
        if (!readMeaningfulLine(input, line))
        {
            break;
        }

        const bool sectorSettingsValid = parseTemplateSectorSettings(
            splitWords(line), sector, uvCoordinateSpace);

        // Step 3: read the declared vertex block.
        if (!readMeaningfulLine(input, line))
        {
            break;
        }

        const std::vector<std::string> vxsHeader = splitWords(line);
        const std::int32_t vertexCount =
            (vxsHeader.size() >= 2 && vxsHeader[0] == "VXS") ? parseInt(vxsHeader[1]).value_or(0) : 0;
        readTemplateVertices(input, vertexCount, sector);

        // Step 4: read the declared surface block.
        if (!readMeaningfulLine(input, line))
        {
            break;
        }

        const std::vector<std::string> surfHeader = splitWords(line);
        const std::int32_t surfaceCount =
            (surfHeader.size() >= 2 && surfHeader[0] == "SURFS") ? parseInt(surfHeader[1]).value_or(0) : 0;
        const bool surfacesValid =
            readTemplateSurfaces(input, surfaceCount, sector);

        // Step 5: consume to END and store only usable shapes.
        consumeTemplateShapeEnd(input);

        if (sectorSettingsValid
            && surfacesValid
            && finalizeLoadedTemplateSector(sector))
        {
            shapes.push_back(SectorShape{
                std::move(name),
                std::move(sector),
                uvCoordinateSpace });
        }
    }

    return shapes;
}

namespace
{

std::int32_t createSectorFromShapeGeometry(
    Level& level,
    const Sector& shape,
    const TVector& center,
    const double size) noexcept
{
    if (shape.vertices.empty() || shape.surfaces.empty()
        || !isFiniteVector(center)
        || !std::isfinite(size)
        || size <= 0.0)
    {
        return -1;
    }

    // Step 1: measure the source shape and reject degenerate templates.
    const std::optional<ShapeBounds> bounds = sectorShapeBounds(shape);
    if (!bounds)
    {
        return -1;
    }

    const TVector extent = extentVector(bounds->min, bounds->max);
    const double largestExtent = maxExtent(extent);
    if (!std::isfinite(largestExtent) || largestExtent <= std::numeric_limits<double>::epsilon())
    {
        return -1;
    }

    // Step 2: calculate a uniform scale around the template's local center.
    const TVector localCenter{
        (bounds->min.x + bounds->max.x) * kUnitCenter,
        (bounds->min.y + bounds->max.y) * kUnitCenter,
        (bounds->min.z + bounds->max.z) * kUnitCenter
    };
    const double scale = size / largestExtent;
    if (!isFiniteVector(localCenter) || !std::isfinite(scale))
    {
        return -1;
    }

    // Step 3: create the destination sector and map vertices into world space.
    const std::int32_t dstIndex = static_cast<std::int32_t>(level.sectors.size());
    Sector& dst = level.newSector();
    prepareShapeDestination(shape, dst);

    dst.vertices.reserve(shape.vertices.size());
    for (const Vertex& vertex : shape.vertices)
    {
        const TVector mapped{
            center.x + (vertex.x - localCenter.x) * scale,
            center.y + (vertex.y - localCenter.y) * scale,
            center.z + (vertex.z - localCenter.z) * scale
        };
        if (!isFiniteVector(mapped))
        {
            return -1;
        }
        dst.vertices.push_back(Vertex{
            mapped.x,
            mapped.y,
            mapped.z,
            0
        });
    }

    // Step 4: copy surfaces after vertices exist so normals can be recalculated.
    if (!copyShapeSurfaces(shape, dst))
    {
        return -1;
    }

    return dstIndex;
}

std::int32_t createSectorFromShapeBoxAxesGeometry(
    Level& level,
    const Sector& shape,
    const TVector& minCorner,
    const TVector& maxCorner,
    const int localXAxis,
    const int localYAxis,
    const int localZAxis) noexcept
{
    if (shape.vertices.empty() || shape.surfaces.empty())
    {
        return -1;
    }

    const std::array<int, 3> axes{ localXAxis, localYAxis, localZAxis };
    if (!areDistinctAxes(axes) || !isFiniteVector(minCorner) || !isFiniteVector(maxCorner))
    {
        return -1;
    }

    // Step 1: normalize the destination box and reject flat boxes.
    const TVector dstMin = orderedMinCorner(minCorner, maxCorner);
    const TVector dstMax = orderedMaxCorner(minCorner, maxCorner);
    const TVector dstExtent = extentVector(dstMin, dstMax);
    if (!isFiniteVector(dstExtent) || !hasPositiveVolume(dstExtent))
    {
        return -1;
    }

    // Step 2: measure the source template and reject degenerate shapes.
    const std::optional<ShapeBounds> bounds = sectorShapeBounds(shape);
    if (!bounds)
    {
        return -1;
    }

    const TVector srcExtent = extentVector(bounds->min, bounds->max);
    const double largestExtent = maxExtent(srcExtent);
    if (!std::isfinite(largestExtent) || largestExtent <= std::numeric_limits<double>::epsilon())
    {
        return -1;
    }

    // Step 3: create the destination sector and map each local source axis to
    // the caller-selected world axis.
    const std::int32_t dstIndex = static_cast<std::int32_t>(level.sectors.size());
    Sector& dst = level.newSector();
    prepareShapeDestination(shape, dst);

    dst.vertices.reserve(shape.vertices.size());
    for (const Vertex& vertex : shape.vertices)
    {
        const double nx = normalizeAxisValue(vertex.x, bounds->min.x, srcExtent.x);
        const double ny = normalizeAxisValue(vertex.y, bounds->min.y, srcExtent.y);
        const double nz = normalizeAxisValue(vertex.z, bounds->min.z, srcExtent.z);
        if (!std::isfinite(nx) || !std::isfinite(ny)
            || !std::isfinite(nz))
        {
            return -1;
        }

        Vertex mapped{};
        setAxis(mapped, localXAxis, axisValue(dstMin, localXAxis) + nx * axisValue(dstExtent, localXAxis));
        setAxis(mapped, localYAxis, axisValue(dstMin, localYAxis) + ny * axisValue(dstExtent, localYAxis));
        setAxis(mapped, localZAxis, axisValue(dstMin, localZAxis) + nz * axisValue(dstExtent, localZAxis));
        if (!std::isfinite(mapped.x) || !std::isfinite(mapped.y)
            || !std::isfinite(mapped.z))
        {
            return -1;
        }
        mapped.mark = 0;
        dst.vertices.push_back(mapped);
    }

    // Step 4: copy surfaces after vertices exist so normals use mapped geometry.
    if (!copyShapeSurfaces(shape, dst))
    {
        return -1;
    }

    return dstIndex;
}

bool admitPlacedShapeUvs(
    Level& level,
    const std::int32_t sectorIndex,
    const UvCoordinateSpace sourceUvCoordinateSpace,
    const UvTextureDimensionsResolver& resolveDimensions)
{
    if (!isValidUvCoordinateSpace(sourceUvCoordinateSpace)
        || !isValidUvCoordinateSpace(level.uvCoordinateSpace)
        || sectorIndex < 0
        || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        return false;
    }

    return static_cast<bool>(convertSectorUvCoordinateSpace(
        level.sectors[static_cast<std::size_t>(sectorIndex)],
        sectorIndex,
        sourceUvCoordinateSpace,
        level.uvCoordinateSpace,
        resolveDimensions));
}

} // namespace

std::int32_t levelCreateSectorFromShape(
    Level& level,
    const Sector& shape,
    const UvCoordinateSpace sourceUvCoordinateSpace,
    const TVector& center,
    const double size,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept
{
    if ( !level.canAllocateIds(1)
        || !isValidUvCoordinateSpace(sourceUvCoordinateSpace)
        || !isValidUvCoordinateSpace(level.uvCoordinateSpace) )
    {
        return -1;
    }

    Level candidate = level;
    const std::int32_t sectorIndex = createSectorFromShapeGeometry(
        candidate, shape, center, size);
    if (!admitPlacedShapeUvs(
            candidate,
            sectorIndex,
            sourceUvCoordinateSpace,
            resolveDimensions))
    {
        return -1;
    }

    level = std::move(candidate);
    return sectorIndex;
}

std::int32_t levelCreateSectorFromShape(
    Level& level,
    const Sector& shape,
    const TVector& center,
    const double size) noexcept
{
    return levelCreateSectorFromShape(
        level,
        shape,
        level.uvCoordinateSpace,
        center,
        size,
        {});
}

std::int32_t levelCreateSectorFromShapeBox(
    Level& level,
    const Sector& shape,
    const UvCoordinateSpace sourceUvCoordinateSpace,
    const TVector& minCorner,
    const TVector& maxCorner,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept
{
    return levelCreateSectorFromShapeBoxAxes(
        level,
        shape,
        sourceUvCoordinateSpace,
        minCorner,
        maxCorner,
        0,
        1,
        2,
        resolveDimensions);
}

std::int32_t levelCreateSectorFromShapeBox(
    Level& level,
    const Sector& shape,
    const TVector& minCorner,
    const TVector& maxCorner) noexcept
{
    return levelCreateSectorFromShapeBox(
        level,
        shape,
        level.uvCoordinateSpace,
        minCorner,
        maxCorner,
        {});
}

std::int32_t levelCreateSectorFromShapeBoxAxes(
    Level& level,
    const Sector& shape,
    const UvCoordinateSpace sourceUvCoordinateSpace,
    const TVector& minCorner,
    const TVector& maxCorner,
    const int localXAxis,
    const int localYAxis,
    const int localZAxis,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept
{
    if ( !level.canAllocateIds(1)
        || !isValidUvCoordinateSpace(sourceUvCoordinateSpace)
        || !isValidUvCoordinateSpace(level.uvCoordinateSpace) )
    {
        return -1;
    }

    Level candidate = level;
    const std::int32_t sectorIndex =
        createSectorFromShapeBoxAxesGeometry(
            candidate,
            shape,
            minCorner,
            maxCorner,
            localXAxis,
            localYAxis,
            localZAxis);
    if (!admitPlacedShapeUvs(
            candidate,
            sectorIndex,
            sourceUvCoordinateSpace,
            resolveDimensions))
    {
        return -1;
    }

    level = std::move(candidate);
    return sectorIndex;
}

std::int32_t levelCreateSectorFromShapeBoxAxes(
    Level& level,
    const Sector& shape,
    const TVector& minCorner,
    const TVector& maxCorner,
    const int localXAxis,
    const int localYAxis,
    const int localZAxis) noexcept
{
    return levelCreateSectorFromShapeBoxAxes(
        level,
        shape,
        level.uvCoordinateSpace,
        minCorner,
        maxCorner,
        localXAxis,
        localYAxis,
        localZAxis,
        {});
}

std::int32_t levelCreateSectorPrimitiveBox(
    Level& level,
    const SectorPrimitive primitive,
    const TVector& minCorner,
    const TVector& maxCorner) noexcept
{
    return levelCreateSectorPrimitiveBox(
        level,
        primitive,
        minCorner,
        maxCorner,
        defaultPrimitiveSides(primitive),
        defaultGeneratedSectorTextureDimensions);
}

std::int32_t levelCreateSectorPrimitiveBox(
    Level& level,
    const SectorPrimitive primitive,
    const TVector& minCorner,
    const TVector& maxCorner,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept
{
    return levelCreateSectorPrimitiveBox(
        level,
        primitive,
        minCorner,
        maxCorner,
        defaultPrimitiveSides(primitive),
        resolveDimensions);
}

std::int32_t levelCreateSectorPrimitiveBox(
    Level& level,
    const SectorPrimitive primitive,
    const TVector& minCorner,
    const TVector& maxCorner,
    const int sides) noexcept
{
    return levelCreateSectorPrimitiveBox(
        level,
        primitive,
        minCorner,
        maxCorner,
        sides,
        defaultGeneratedSectorTextureDimensions);
}

std::int32_t levelCreateSectorPrimitiveBox(
    Level& level,
    const SectorPrimitive primitive,
    const TVector& minCorner,
    const TVector& maxCorner,
    const int sides,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept
{
    return levelCreateSectorPrimitiveBoxAxes(
        level,
        primitive,
        minCorner,
        maxCorner,
        0,
        1,
        2,
        sides,
        resolveDimensions);
}

std::int32_t levelCreateSectorPrimitiveBoxAxes(
    Level& level,
    const SectorPrimitive primitive,
    const TVector& minCorner,
    const TVector& maxCorner,
    const int localXAxis,
    const int localYAxis,
    const int localZAxis,
    const int sides) noexcept
{
    return levelCreateSectorPrimitiveBoxAxes(
        level,
        primitive,
        minCorner,
        maxCorner,
        localXAxis,
        localYAxis,
        localZAxis,
        sides,
        defaultGeneratedSectorTextureDimensions);
}

std::int32_t levelCreateSectorPrimitiveBoxAxes(
    Level& level,
    const SectorPrimitive primitive,
    const TVector& minCorner,
    const TVector& maxCorner,
    const int localXAxis,
    const int localYAxis,
    const int localZAxis,
    const int sides,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept
{
    if ( !level.canAllocateIds(1)
        || !isValidUvCoordinateSpace(level.uvCoordinateSpace) )
    {
        return -1;
    }

    Level candidate = level;
    Sector shape = makeSectorPrimitiveShape(candidate.kind, primitive, sides);
    applyLevelDefaultsToPrimitiveShape(candidate, shape);

    const std::int32_t sectorIndex =
        createSectorFromShapeBoxAxesGeometry(
            candidate,
            shape,
            minCorner,
            maxCorner,
            localXAxis,
            localYAxis,
            localZAxis);
    if (sectorIndex < 0
        || static_cast<std::size_t>(sectorIndex) >= candidate.sectors.size())
    {
        return -1;
    }

    Sector& sector = candidate.sectors[static_cast<std::size_t>(sectorIndex)];
    const double ppu = levelPixelPerUnit(candidate);
    for (Surface& surface : sector.surfaces)
    {
        surfaceRecalcNewAll(ppu, sector, surface, 0);
    }

    if (!admitGeneratedPixelUvs(
            sector,
            sectorIndex,
            candidate.uvCoordinateSpace,
            resolveDimensions))
    {
        return -1;
    }

    level = std::move(candidate);
    return sectorIndex;
}

std::int32_t levelCreateSectorPrimitive(
    Level& level,
    const SectorPrimitive primitive,
    const TVector& center,
    const double size) noexcept
{
    return levelCreateSectorPrimitive(
        level,
        primitive,
        center,
        size,
        defaultPrimitiveSides(primitive),
        defaultGeneratedSectorTextureDimensions);
}

std::int32_t levelCreateSectorPrimitive(
    Level& level,
    const SectorPrimitive primitive,
    const TVector& center,
    const double size,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept
{
    return levelCreateSectorPrimitive(
        level,
        primitive,
        center,
        size,
        defaultPrimitiveSides(primitive),
        resolveDimensions);
}

std::int32_t levelCreateSectorPrimitive(
    Level& level,
    const SectorPrimitive primitive,
    const TVector& center,
    const double size,
    const int sides) noexcept
{
    return levelCreateSectorPrimitive(
        level,
        primitive,
        center,
        size,
        sides,
        defaultGeneratedSectorTextureDimensions);
}

std::int32_t levelCreateSectorPrimitive(
    Level& level,
    const SectorPrimitive primitive,
    const TVector& center,
    const double size,
    const int sides,
    const UvTextureDimensionsResolver& resolveDimensions) noexcept
{
    if (!std::isfinite(size) || size <= 0.0)
    {
        return -1;
    }

    const double half = size * kUnitRadius;
    return levelCreateSectorPrimitiveBox(
        level,
        primitive,
        TVector{ center.x - half, center.y - half, center.z - half },
        TVector{ center.x + half, center.y + half, center.z + half },
        sides,
        resolveDimensions);
}

namespace
{
void setExtrudeReport(SurfaceExtrudeReport* report, const SurfaceExtrudeError error,
    const SurfaceRef source) noexcept
{
    if ( report )
    {
        *report = {error, source};
    }
}

struct ExtrudeSource
{
    std::vector<TVector> points;
    TVector normal;
    SurfacePlaneBounds bounds;
};

std::optional<ExtrudeSource> resolveExtrudeSource(const Level& level, const SurfaceRef ref,
    SurfaceExtrudeReport* report)
{
    if ( !isValidSurfaceRef(level, ref) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidSurface, ref);
        return std::nullopt;
    }
    const auto& sector = level.sectors[ref.sector];
    const auto& face = sector.surfaces[ref.surface];
    if ( face.adjoin )
    {
        setExtrudeReport(report, SurfaceExtrudeError::AlreadyAdjoined, ref);
        return std::nullopt;
    }
    if ( !isValidUvCoordinateSpace(level.uvCoordinateSpace) || face.vertices.size() < 3
        || face.vertices.size() != face.texVertices.size()
        || !std::isfinite(face.uScale) || !std::isfinite(face.vScale) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidSourceTopology, ref);
        return std::nullopt;
    }
    for ( std::size_t index = 0; index < face.vertices.size(); ++index )
    {
        const auto vertex = face.vertices[index];
        if ( !isValidVertexRef(sector, vertex) || !precision::finite(vertexPosition(sector.vertices[vertex]))
            || !std::isfinite(face.texVertices[index].u) || !std::isfinite(face.texVertices[index].v)
            || std::find(face.vertices.begin(), face.vertices.begin() + index, vertex) != face.vertices.begin() + index )
        {
            setExtrudeReport(report, SurfaceExtrudeError::InvalidSourceTopology, ref);
            return std::nullopt;
        }
    }
    TVector normal = face.normal;
    if ( normal.isZero() )
    {
        normal = surfaceCalcNormalNewell(sector, face);
    }
    const double normalScale = std::max({std::abs(normal.x), std::abs(normal.y), std::abs(normal.z)});
    if ( !precision::finite(normal) || !(normalScale > 0.0) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidSourceTopology, ref);
        return std::nullopt;
    }
    // Cached magnitude does not define the geometry. Scale before measuring
    // length so finite huge and subnormal same-facing caches retain direction.
    normal = {normal.x / normalScale, normal.y / normalScale, normal.z / normalScale};
    const double normalLength = std::hypot(normal.x, normal.y, normal.z);
    normal = {normal.x / normalLength, normal.y / normalLength, normal.z / normalLength};
    Surface validated = face;
    validated.normal = normal;
    if ( !precision::sourceSurfaceIsPlanar(sector, validated) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::NonPlanarSource, ref);
        return std::nullopt;
    }
    // Input-only admission bounds inherited corner rounding. Publication must
    // separately prove that generated geometry preserves the exact footprint.
    if ( !precision::sourceSurfaceIsConvex(sector, validated) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::NonConvexSource, ref);
        return std::nullopt;
    }
    // The cache has already passed finite/orientation admission. Build from
    // geometric winding so cached-normal quantization does not masquerade as
    // coordinate warp or choose a different expansion frame.
    normal = surfaceCalcNormalNewell(sector, validated);
    auto points = collectSurfaceVertexPositions(sector, face);
    const auto bounds = surfacePlaneBounds(*points, normal);
    if ( !bounds )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidSourceTopology, ref);
        return std::nullopt;
    }
    return ExtrudeSource{std::move(*points), normal, *bounds};
}

bool plainExtrudePreservesFootprint(const Sector& sector, const ExtrudeSource& source,
    const SurfaceExtrudePreview& preview) noexcept
{
    const auto count = source.points.size();
    if ( sector.vertices.size() != 2 * count || sector.surfaces.size() != count + 2
        || preview.outerLoop.size() != count || sector.surfaces[0].vertices.size() != count
        || sector.surfaces[1].vertices.size() != count )
    {
        return false;
    }
    const auto samePoint = [](const TVector& a, const TVector& b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    };
    for ( std::size_t index = 0; index < count; ++index )
    {
        if ( sector.surfaces[0].vertices[index] != static_cast<std::int32_t>(index)
            || sector.surfaces[1].vertices[index] != static_cast<std::int32_t>(count + index)
            || !samePoint(vertexPosition(sector.vertices[count - index - 1]), source.points[index])
            || !samePoint(vertexPosition(sector.vertices[count + index]), preview.outerLoop[index]) )
        {
            return false;
        }
    }
    return true;
}

// Only generated side faces can reach this partition. Their exact boundary
// and corner attributes are retained; ear diagonals add no geometric points.
bool partitionExtrudeSides(Sector& sector, const std::size_t originalFaceCount,
    std::vector<std::size_t>& appendedSides)
{
    for ( std::size_t faceIndex = 2; faceIndex < originalFaceCount; ++faceIndex )
    {
        const Surface original = sector.surfaces[faceIndex];
        if ( original.adjoin || original.vertices.size() != original.texVertices.size()
            || !surfaceIsPlanar(sector, original) )
        {
            return false;
        }
        if ( surfaceIsConvex(sector, original) )
        {
            continue;
        }
        std::vector<std::size_t> remaining;
        remaining.reserve(original.vertices.size());
        for ( std::size_t corner = 0; corner < original.vertices.size(); ++corner )
        {
            remaining.push_back(corner);
        }
        std::vector<std::array<std::size_t, 3>> triangles;
        const auto point = [&](const std::size_t corner)
        {
            return vertexPosition(sector.vertices[original.vertices[corner]]);
        };
        if ( !precision::finite(original.normal) )
        {
            return false;
        }
        std::size_t dominantAxis = 0;
        double dominantMagnitude = std::abs(original.normal.x);
        if ( std::abs(original.normal.y) > dominantMagnitude )
        {
            dominantAxis = 1;
            dominantMagnitude = std::abs(original.normal.y);
        }
        if ( std::abs(original.normal.z) > dominantMagnitude )
        {
            dominantAxis = 2;
            dominantMagnitude = std::abs(original.normal.z);
        }
        if ( !(dominantMagnitude > 0.0) )
        {
            return false;
        }
        const double dominantNormal = dominantAxis == 0 ? original.normal.x
            : (dominantAxis == 1 ? original.normal.y : original.normal.z);
        const auto projectedTurnSign = [&](const TVector& a, const TVector& b, const TVector& c)
            -> std::optional<int>
        {
            std::optional<int> orientation;
            if ( dominantAxis == 0 )
            {
                orientation = detail::exactOrientationSign(a.y, a.z, b.y, b.z, c.y, c.z);
            }
            else if ( dominantAxis == 1 )
            {
                orientation = detail::exactOrientationSign(a.x, a.z, b.x, b.z, c.x, c.z);
            }
            else
            {
                orientation = detail::exactOrientationSign(a.x, a.y, b.x, b.y, c.x, c.y);
            }
            if ( !orientation || *orientation == 0 )
            {
                return orientation;
            }
            // Dropping the Y coordinate reverses the projected cross-product
            // axis; the other two dropped-axis projections preserve it.
            const int projectionSign = dominantAxis == 1 ? -1 : 1;
            const int normalSign = dominantNormal > 0.0 ? 1 : -1;
            return *orientation * projectionSign * normalSign;
        };
        while ( remaining.size() > 3 )
        {
            bool clipped = false;
            for ( std::size_t index = 0; index < remaining.size(); ++index )
            {
                const std::array<std::size_t, 3> ear{{
                    remaining[(index + remaining.size() - 1) % remaining.size()],
                    remaining[index], remaining[(index + 1) % remaining.size()]}};
                const TVector a = point(ear[0]), b = point(ear[1]), c = point(ear[2]);
                const auto earTurn = projectedTurnSign(a, b, c);
                if ( !earTurn || *earTurn <= 0 )
                {
                    continue;
                }
                const auto blocksEar = [&](const TVector& first, const TVector& second,
                    const TVector& test)
                {
                    const auto side = projectedTurnSign(first, second, test);
                    // Collinear points lie on the ear boundary and must keep
                    // blocking it. An unrepresentable/nonfinite sign also
                    // fails closed instead of admitting an unproven ear.
                    return !side || *side >= 0;
                };
                bool containsCorner = false;
                for ( const auto corner : remaining )
                {
                    if ( corner == ear[0] || corner == ear[1] || corner == ear[2] )
                    {
                        continue;
                    }
                    const TVector p = point(corner);
                    if ( blocksEar(a, b, p) && blocksEar(b, c, p) && blocksEar(c, a, p) )
                    {
                        containsCorner = true;
                        break;
                    }
                }
                if ( containsCorner )
                {
                    continue;
                }
                triangles.push_back(ear);
                remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(index));
                clipped = true;
                break;
            }
            if ( !clipped )
            {
                return false;
            }
        }
        triangles.push_back({remaining[0], remaining[1], remaining[2]});
        bool first = true;
        for ( const auto& triangle : triangles )
        {
            Surface part = original;
            part.vertices.clear();
            part.texVertices.clear();
            for ( const auto corner : triangle )
            {
                part.vertices.push_back(original.vertices[corner]);
                part.texVertices.push_back(original.texVertices[corner]);
            }
            part.normal = surfaceCalcNormalNewell(sector, part);
            if ( !surfaceIsPlanar(sector, part) || !surfaceIsConvex(sector, part) )
            {
                return false;
            }
            if ( first )
            {
                sector.surfaces[faceIndex] = std::move(part);
                first = false;
            }
            else
            {
                part.num = static_cast<std::int32_t>(sector.surfaces.size());
                appendedSides.push_back(sector.surfaces.size());
                sector.surfaces.push_back(std::move(part));
            }
        }
    }
    return true;
}

bool inheritedCapHasConvexFootprint(const Sector& sector, const Surface& face,
    const ExtrudeSource& source, const SurfaceExtrudePreview& preview, const TVector& normal) noexcept
{
    const auto point = [&](const std::size_t index)
    {
        return vertexPosition(sector.vertices[face.vertices[index % face.vertices.size()]]);
    };
    const auto followsConstructionLine = [&](const TVector& a, const TVector& b, const TVector& c,
        const TVector& start, const TVector& end)
    {
        TVector along = end - start;
        along = along - source.normal.scaled(along.dot(source.normal));
        const double length = std::hypot(along.x, along.y, along.z);
        if ( !(length > 0.0) || !std::isfinite(length) )
        {
            return false;
        }
        along = {along.x / length, along.y / length, along.z / length};
        const TVector across = source.normal.cross(along);
        // Intersections are evaluated from the original clipping operands.
        // Their cancellation error cannot be inferred from the final tiny
        // coordinate alone (for example a computed z very close to zero).
        double constructionError = precision::planeError(across, start, end);
        for ( const auto& operand : source.points )
        {
            constructionError = std::max(constructionError, precision::planeError(across, operand, start));
        }
        for ( const auto& operand : preview.innerLoop )
        {
            constructionError = std::max(constructionError, precision::planeError(across, operand, start));
        }
        for ( const auto& value : {a,b,c} )
        {
            const double distance = across.dot(value - start);
            const double error = constructionError + precision::planeError(across, value, start);
            if ( !std::isfinite(distance) || !std::isfinite(error) || error < 0.0 || std::abs(distance) > error )
            {
                return false;
            }
        }
        double middle = along.dot(b - a);
        double finish = along.dot(c - a);
        if ( finish < 0.0 )
        {
            middle = -middle;
            finish = -finish;
        }
        const double error = precision::planeError(along, a, b) + precision::planeError(along, a, c);
        return std::isfinite(middle) && std::isfinite(finish) && std::isfinite(error)
            && middle >= -error && middle <= finish + error;
    };
    for ( std::size_t corner = 0; corner < face.vertices.size(); ++corner )
    {
        const TVector a = point(corner), b = point(corner + 1), c = point(corner + 2);
        const double turn = (b - a).cross(c - b).dot(normal);
        if ( !std::isfinite(turn) )
        {
            return false;
        }
        if ( turn >= -precision::cornerTurnError(a, b, c, normal) )
        {
            continue;
        }
        bool inheritedLine = false;
        for ( std::size_t edge = 0; !inheritedLine && edge < source.points.size(); ++edge )
        {
            inheritedLine = followsConstructionLine(a, b, c, source.points[edge],
                source.points[(edge + 1) % source.points.size()]);
        }
        for ( std::size_t edge = 0; !inheritedLine && edge < preview.innerLoop.size(); ++edge )
        {
            inheritedLine = followsConstructionLine(a, b, c, preview.innerLoop[edge],
                preview.innerLoop[(edge + 1) % preview.innerLoop.size()]);
        }
        if ( !inheritedLine )
        {
            return false;
        }
    }
    return true;
}

bool extrudeResultIsValid(const Sector& sector, const ExtrudeSource& source,
    const SurfaceExtrudePreview& preview, const bool expanded, const bool plainPrism,
    const std::size_t originalFaceCount, const std::vector<std::size_t>& appendedSides = {})
{
    if ( !precision::closedBoundary(sector) || sector.surfaces.size() < originalFaceCount )
    {
        return false;
    }
    const bool inheritedFootprint = plainPrism && plainExtrudePreservesFootprint(sector, source, preview);
    if ( plainPrism && !inheritedFootprint )
    {
        return false;
    }
    TVector planeNormal = source.bounds.xAxis.cross(source.bounds.yAxis);
    const double planeLength = std::hypot(planeNormal.x, planeNormal.y, planeNormal.z);
    if ( !(planeLength > 0.0) || !std::isfinite(planeLength) )
    {
        return false;
    }
    planeNormal = {planeNormal.x / planeLength, planeNormal.y / planeLength, planeNormal.z / planeLength};
    const TVector anchor = source.points.front();
    double minimum = 0.0;
    double maximum = 0.0;
    for ( const auto& point : source.points )
    {
        const double height = planeNormal.dot(point - anchor);
        minimum = std::min(minimum, height);
        maximum = std::max(maximum, height);
    }
    const TVector offset = preview.outerLoop.front() - (expanded ? preview.innerLoop.back() : source.points.front());
    const double outerHeight = planeNormal.dot(offset);
    for ( std::size_t faceIndex = 0; faceIndex < sector.surfaces.size(); ++faceIndex )
    {
        const auto& face = sector.surfaces[faceIndex];
        if ( face.vertices.size() < 3 || face.vertices.size() != face.texVertices.size()
            || !precision::finite(face.normal) || !std::isfinite(face.uScale) || !std::isfinite(face.vScale) )
        {
            return false;
        }
        for ( const auto& corner : face.texVertices )
        {
            if ( !std::isfinite(corner.u) || !std::isfinite(corner.v) )
            {
                return false;
            }
        }
        const bool inheritedInner = faceIndex == 0 || (faceIndex >= originalFaceCount
            && std::find(appendedSides.begin(), appendedSides.end(), faceIndex) == appendedSides.end());
        const bool inheritedOuter = !expanded && faceIndex == 1;
        if ( inheritedInner || inheritedOuter )
        {
            const double transport = inheritedOuter ? outerHeight : 0.0;
            Surface projected = face;
            projected.normal = inheritedOuter ? planeNormal : -planeNormal;
            const bool exactSourceCap = precision::exactReversedSourceCapIdentity(
                sector,
                faceIndex,
                face,
                std::span<const TVector>(source.points.data(), source.points.size()));
            if ( ((!inheritedFootprint && !exactSourceCap)
                    && !inheritedCapHasConvexFootprint(sector, face, source, preview, projected.normal))
                || face.normal.dot(projected.normal) <= 0.0 )
            {
                return false;
            }
            for ( const auto index : face.vertices )
            {
                const TVector point = vertexPosition(sector.vertices[index]);
                const double height = planeNormal.dot(point - anchor) - transport;
                const double error = precision::planeError(planeNormal, point, anchor)
                    + precision::kRoundoff * std::abs(transport);
                if ( !std::isfinite(height) || height < minimum - error || height > maximum + error )
                {
                    return false;
                }
            }
            // Each inherited cap transports the immutable original support
            // interval. New vertices cannot broaden it on the exterior side.
            for ( const auto& vertex : sector.vertices )
            {
                const TVector point = vertexPosition(vertex);
                const double height = planeNormal.dot(point - anchor) - transport;
                const double error = precision::planeError(planeNormal, point, anchor)
                    + precision::kRoundoff * std::abs(transport);
                if ( (inheritedInner && height > maximum + error)
                    || (inheritedOuter && height < minimum - error) )
                {
                    return false;
                }
            }
        }
        else
        {
            if ( !surfaceIsPlanar(sector, face) || !surfaceIsConvex(sector, face) )
            {
                return false;
            }
            if ( inheritedFootprint )
            {
                // Side i is constructed from original edge (N-1-i, N-i).
                // Freeze its support deficit from the input polygon, never
                // from the output face or a diagnostic output tolerance.
                const auto count = source.points.size();
                const auto corner = count - 1 - (faceIndex - 2);
                const TVector anchor = source.points[corner];
                TVector inward = source.normal.cross(source.points[(corner + 1) % count] - anchor);
                const double inwardLength = std::hypot(inward.x, inward.y, inward.z);
                if ( !(inwardLength > 0.0) || !std::isfinite(inwardLength) )
                {
                    return false;
                }
                inward = {inward.x / inwardLength, inward.y / inwardLength, inward.z / inwardLength};
                if ( face.normal.dot(inward) <= 0.0 )
                {
                    return false;
                }
                double minimumSupport = 0.0;
                double sourceError = 0.0;
                for ( const auto& point : source.points )
                {
                    const double support = inward.dot(point - anchor);
                    const double error = precision::planeError(inward, point, anchor);
                    if ( !std::isfinite(support) || !std::isfinite(error) || error < 0.0 )
                    {
                        return false;
                    }
                    minimumSupport = std::min(minimumSupport, support);
                    sourceError = std::max(sourceError, error);
                }
                for ( const auto& vertex : sector.vertices )
                {
                    const TVector point = vertexPosition(vertex);
                    const double support = inward.dot(point - anchor);
                    const double error = precision::planeError(inward, point, anchor);
                    if ( !std::isfinite(support) || !std::isfinite(error) || error < 0.0
                        || support < minimumSupport - sourceError - error )
                    {
                        return false;
                    }
                }
                continue;
            }
            const auto context = precision::surfacePlaneError(sector, face);
            if ( !context.valid )
            {
                return false;
            }
            const TVector faceAnchor = vertexPosition(sector.vertices[face.vertices.front()]);
            for ( const auto& vertex : sector.vertices )
            {
                const TVector point = vertexPosition(vertex);
                const double distance = face.normal.dot(point - faceAnchor);
                const double error = context.at(face.normal, point, faceAnchor);
                if ( !std::isfinite(distance) || error < 0.0 || distance < -error )
                {
                    return false;
                }
            }
        }
    }
    return true;
}
} // namespace

static std::optional<SurfaceExtrudePreview> buildSurfaceExtrudeWire(
    const Level& level, const SurfaceRef surfaceRef, const double length, const double expand,
    SurfaceExtrudeReport* const report) noexcept
try
{
    setExtrudeReport(report, SurfaceExtrudeError::None, surfaceRef);
    if ( !std::isfinite(length) || length <= 0.0 )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidLength, surfaceRef);
        return std::nullopt;
    }
    if ( !std::isfinite(expand) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidExpand, surfaceRef);
        return std::nullopt;
    }
    const auto source = resolveExtrudeSource(level, surfaceRef, report);
    if ( !source )
    {
        return std::nullopt;
    }
    SurfaceExtrudePreview preview;
    if ( expand != 0.0 )
    {
        const double width = source->bounds.maxX - source->bounds.minX + 2.0 * expand;
        const double height = source->bounds.maxY - source->bounds.minY + 2.0 * expand;
        if ( !(width > 0.0) || !(height > 0.0) )
        {
            setExtrudeReport(report, SurfaceExtrudeError::CollapsedExpand, surfaceRef);
            return std::nullopt;
        }
        if ( !buildExpandedExtrudeRectangle(source->points, source->normal, length, expand, preview) )
        {
            setExtrudeReport(report, SurfaceExtrudeError::InvalidResult, surfaceRef);
            return std::nullopt;
        }
    }
    else
    {
        preview.innerLoop.assign(source->points.rbegin(), source->points.rend());
        preview.outerLoop.reserve(source->points.size());
        const TVector translation = source->normal.scaled(-length);
        for ( const auto& point : source->points )
        {
            preview.outerLoop.push_back(point + translation);
        }
    }
    if ( !extrusionLoopsAreSeparated(preview) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidResult, surfaceRef);
        return std::nullopt;
    }
    return preview;
}
catch ( const std::bad_alloc& )
{
    setExtrudeReport(report, SurfaceExtrudeError::AllocationFailure, surfaceRef);
    return std::nullopt;
}

std::optional<SurfaceExtrudeExpandBounds> levelSurfaceExtrudeExpandBounds(
    const Level& level, const SurfaceRef surfaceRef, SurfaceExtrudeReport* const report) noexcept
try
{
    setExtrudeReport(report, SurfaceExtrudeError::None, surfaceRef);
    const auto source = resolveExtrudeSource(level, surfaceRef, report);
    if ( !source )
    {
        return std::nullopt;
    }
    const double minSpan = std::min(source->bounds.maxX - source->bounds.minX,
        source->bounds.maxY - source->bounds.minY);
    if ( !std::isfinite(minSpan) || !(minSpan > 0.0) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::CollapsedExpand, surfaceRef);
        return std::nullopt;
    }
    return SurfaceExtrudeExpandBounds{-0.5 * minSpan, minSpan};
}
catch ( const std::bad_alloc& )
{
    setExtrudeReport(report, SurfaceExtrudeError::AllocationFailure, surfaceRef);
    return std::nullopt;
}

namespace
{

bool applyExpandedCapUvProjection(const Sector&, const Surface&, Sector&, std::int32_t);

std::optional<std::int32_t> populateSurfaceExtrudeSector(
    const Level& level,
    const Sector& srcSector,
    const Surface& srcSurface,
    const SurfaceExtrudePreview& preview,
    Sector& newSector,
    const bool hideInnerSurface,
    const bool preserveSourceCapUv)
{
    Surface surfaceParams{};
    assignSurfaceParams(srcSurface, surfaceParams);
    surfaceParams.normal = srcSurface.normal;

    assignSectorParams(srcSector, newSector);
    if (level.kind != ProjectType::IJIM && newSector.colormap.empty())
    {
        newSector.colormap = level.getMasterCMP();
    }

    // Step 1: create the inner face with reversed winding. The live preview
    // hides it because the eventual adjoin face occupies the source plane.
    Surface asurf{};
    initNewSurface(level.kind, asurf);
    assignSurfaceParams(surfaceParams, asurf);
    if (hideInnerSurface)
    {
        asurf.geo = static_cast<std::int32_t>(TGeoMode::NotDrawn);
    }

    asurf.vertices.reserve(preview.innerLoop.size());
    for (const TVector& v : preview.innerLoop)
    {
        asurf.vertices.push_back(newSector.newVertex(v.x, v.y, v.z));
    }
    asurf.texVertices = std::vector<TexVertex>(asurf.vertices.size());

    const std::int32_t asurfIndex = newSector.addSurface(std::move(asurf));

    // Step 2: create the outer face by offsetting along the extrusion
    // direction, matching the commit path exactly.
    Surface osurf{};
    initNewSurface(level.kind, osurf);
    assignSurfaceParams(surfaceParams, osurf);

    osurf.vertices.reserve(preview.outerLoop.size());
    for (const TVector& v : preview.outerLoop)
    {
        osurf.vertices.push_back(newSector.newVertex(v.x, v.y, v.z));
    }
    osurf.texVertices = std::vector<TexVertex>(osurf.vertices.size());

    const std::int32_t osurfIndex = newSector.addSurface(std::move(osurf));

    if (asurfIndex < 0 || osurfIndex < 0)
    {
        return std::nullopt;
    }

    const std::vector<std::int32_t> asurfVertices = newSector.surfaces[static_cast<std::size_t>(asurfIndex)].vertices;
    const std::vector<std::int32_t> osurfVertices = newSector.surfaces[static_cast<std::size_t>(osurfIndex)].vertices;
    if (asurfVertices.size() != osurfVertices.size())
    {
        return std::nullopt;
    }

    // Step 3: connect the loops with side faces and recalculate all generated
    // texture/normal data in the same way a committed sector does.
    addSideSurfacesBetweenLoops(newSector, surfaceParams, asurfVertices, osurfVertices);
    recalcSurfaces(level, newSector, 0);
    if (!preserveSourceCapUv
        && (!applyExpandedCapUvProjection(srcSector, srcSurface, newSector, asurfIndex)
            || !applyExpandedCapUvProjection(srcSector, srcSurface, newSector, osurfIndex)))
    {
        return std::nullopt;
    }

    // A plain extrusion has the same source corner set on both caps. Rebuild
    // the normals and generated side mapping above, then restore the authored
    // cap tuples in their corresponding winding order. The geometric UV
    // recalculation is still needed for the side walls, but it must not replace
    // source UV/RGBA data on either translated cap.
    const bool sourceCapTuplesValid = preview.innerLoop.size() == srcSurface.vertices.size()
        && preview.outerLoop.size() == srcSurface.vertices.size()
        && srcSurface.texVertices.size() == srcSurface.vertices.size()
        && asurfVertices.size() == srcSurface.texVertices.size();
    if (preserveSourceCapUv && !sourceCapTuplesValid)
    {
        return std::nullopt;
    }
    if (preserveSourceCapUv)
    {
        Surface& inner = newSector.surfaces[static_cast<std::size_t>(asurfIndex)];
        Surface& outer = newSector.surfaces[static_cast<std::size_t>(osurfIndex)];
        inner.texVertices.assign(
            srcSurface.texVertices.rbegin(), srcSurface.texVertices.rend());
        outer.texVertices = srcSurface.texVertices;
    }
    renumberSurfaces(newSector);

    // Publication validates this body with immutable source-cap provenance;
    // only freshly generated sides and a rectangular expanded outer cap must
    // satisfy strict planarity independently of inherited source rounding.
    return asurfIndex;
}

struct ExtrudeCandidate
{
    Level level;
    SurfaceExtrudePreview wire;
    SurfaceRef source;
    std::size_t originalSourceFaceCount{};
};

bool surfaceBoundariesMatchWithResolvedNormals(
    const Sector& firstSector,
    const Surface& first,
    const Sector& secondSector,
    const Surface& second)
{
    // An exactly unset authored cache is valid input. Resolve a temporary
    // normal for the boundary predicate while retaining the source Surface
    // unchanged. Tiny nonzero caches remain subject to the strict diagnostic
    // normal checks, including their orientation and finite magnitude.
    std::optional<Surface> resolvedFirst;
    std::optional<Surface> resolvedSecond;
    if ( first.normal.x == 0.0 && first.normal.y == 0.0 && first.normal.z == 0.0 )
    {
        resolvedFirst = first;
        resolvedFirst->normal = surfaceCalcNormalNewell(firstSector, first);
    }
    if ( second.normal.x == 0.0 && second.normal.y == 0.0 && second.normal.z == 0.0 )
    {
        resolvedSecond = second;
        resolvedSecond->normal = surfaceCalcNormalNewell(secondSector, second);
    }
    return precision::surfaceBoundariesMatchChecked(
        firstSector,
        resolvedFirst ? *resolvedFirst : first,
        secondSector,
        resolvedSecond ? *resolvedSecond : second);
}

bool sourcePortalsAreValid(const Level& level, const SurfaceRef source)
{
    const auto& sector = level.sectors[source.sector];
    for ( std::size_t index = 0; index < sector.surfaces.size(); ++index )
    {
        const auto& face = sector.surfaces[index];
        if ( !face.adjoin )
        {
            continue;
        }
        if ( !isValidSurfaceRef(level, *face.adjoin) )
        {
            return false;
        }
        const auto& otherSector = level.sectors[face.adjoin->sector];
        const auto& other = otherSector.surfaces[face.adjoin->surface];
        if ( other.adjoin != std::optional<SurfaceRef>{{source.sector, static_cast<std::int32_t>(index)}}
            || !surfaceBoundariesMatchWithResolvedNormals(sector, face, otherSector, other) )
        {
            return false;
        }
    }
    return true;
}

bool applyExpandedCapUvProjection(const Sector& sourceSector, const Surface& sourceSurface,
    Sector& generated, const std::int32_t capIndex)
{
    if (sourceSurface.vertices.size() < 3
        || sourceSurface.vertices.size() != sourceSurface.texVertices.size()
        || capIndex < 0
        || static_cast<std::size_t>(capIndex) >= generated.surfaces.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < sourceSurface.vertices.size(); ++index)
    {
        const std::int32_t vertex = sourceSurface.vertices[index];
        if (vertex < 0
            || static_cast<std::size_t>(vertex) >= sourceSector.vertices.size()
            || !std::isfinite(sourceSurface.texVertices[index].u)
            || !std::isfinite(sourceSurface.texVertices[index].v))
        {
            return false;
        }
    }
    const TVector origin = vertexPosition(
        sourceSector.vertices[static_cast<std::size_t>(sourceSurface.vertices[0])]);
    Surface& cap = generated.surfaces[static_cast<std::size_t>(capIndex)];
    if (cap.vertices.size() != cap.texVertices.size())
    {
        return false;
    }

    std::vector<TexVertex> exact = cap.texVertices;
    std::vector<bool> matched(cap.vertices.size(), false);
    for (std::size_t index = 0; index < cap.vertices.size(); ++index)
    {
        const std::int32_t vertexIndex = cap.vertices[index];
        if (vertexIndex < 0
            || static_cast<std::size_t>(vertexIndex) >= generated.vertices.size())
        {
            return false;
        }
        const TVector point = vertexPosition(
            generated.vertices[static_cast<std::size_t>(vertexIndex)]);
        for (std::size_t sourceIndex = 0; sourceIndex < sourceSurface.vertices.size(); ++sourceIndex)
        {
            const TVector sourcePoint = vertexPosition(
                sourceSector.vertices[static_cast<std::size_t>(sourceSurface.vertices[sourceIndex])]);
            if (point.x == sourcePoint.x && point.y == sourcePoint.y && point.z == sourcePoint.z)
            {
                exact[index] = sourceSurface.texVertices[sourceIndex];
                matched[index] = true;
                break;
            }
        }
    }

    std::size_t chosenFirst = 0;
    std::size_t chosenSecond = 0;
    TVector a{};
    TVector b{};
    TVector area{};
    double determinant = 0.0;
    for (std::size_t first = 1; first + 1 < sourceSurface.vertices.size(); ++first)
    {
        for (std::size_t second = first + 1; second < sourceSurface.vertices.size(); ++second)
        {
            const TVector candidateA = vertexPosition(sourceSector.vertices[static_cast<std::size_t>(sourceSurface.vertices[first])]) - origin;
            const TVector candidateB = vertexPosition(sourceSector.vertices[static_cast<std::size_t>(sourceSurface.vertices[second])]) - origin;
            const TVector candidateArea = candidateA.cross(candidateB);
            const double candidateDeterminant = candidateArea.dot(candidateArea);
            if (std::isfinite(candidateDeterminant)
                && candidateDeterminant > determinant)
            {
                chosenFirst = first;
                chosenSecond = second;
                a = candidateA;
                b = candidateB;
                area = candidateArea;
                determinant = candidateDeterminant;
            }
        }
    }
    cap.texVertices = exact;
    if (!(determinant > 0.0))
    {
        return true;
    }
    const TexVertex& originUv = sourceSurface.texVertices[0];
    const double duA = static_cast<double>(sourceSurface.texVertices[chosenFirst].u)
        - static_cast<double>(originUv.u);
    const double duB = static_cast<double>(sourceSurface.texVertices[chosenSecond].u)
        - static_cast<double>(originUv.u);
    const double dvA = static_cast<double>(sourceSurface.texVertices[chosenFirst].v)
        - static_cast<double>(originUv.v);
    const double dvB = static_cast<double>(sourceSurface.texVertices[chosenSecond].v)
        - static_cast<double>(originUv.v);
    const TVector uGradient = (b.cross(area).scaled(duA) + area.cross(a).scaled(duB)).scaled(1.0 / determinant);
    const TVector vGradient = (b.cross(area).scaled(dvA) + area.cross(a).scaled(dvB)).scaled(1.0 / determinant);
    if (!std::isfinite(uGradient.x) || !std::isfinite(uGradient.y)
        || !std::isfinite(uGradient.z) || !std::isfinite(vGradient.x)
        || !std::isfinite(vGradient.y) || !std::isfinite(vGradient.z))
    {
        return true;
    }
    std::vector<TexVertex> candidate = exact;
    for (std::size_t index = 0; index < cap.vertices.size(); ++index)
    {
        if (matched[index])
        {
            continue;
        }
        const std::int32_t vertexIndex = cap.vertices[index];
        const TVector point = vertexPosition(
            generated.vertices[static_cast<std::size_t>(vertexIndex)]);
        const TVector offset = point - origin;
        const double u = static_cast<double>(originUv.u) + uGradient.dot(offset);
        const double v = static_cast<double>(originUv.v) + vGradient.dot(offset);
        if (!std::isfinite(u) || !std::isfinite(v)
            || std::abs(u) > static_cast<double>(std::numeric_limits<float>::max())
            || std::abs(v) > static_cast<double>(std::numeric_limits<float>::max()))
        {
            return true;
        }
        candidate[index].u = static_cast<float>(u);
        candidate[index].v = static_cast<float>(v);
    }
    cap.texVertices = std::move(candidate);
    return true;
}

struct ExtrudeConnectionValidation
{
    const ExtrudeSource& source;
    const Sector& inputSector;
    const Surface& inputSurface;
    const SurfaceExtrudePreview& preview;
    bool expanded{};
    bool plainPrism{};
    bool sourceIsSolid{};
    std::size_t originalGeneratedFaceCount{};
    std::size_t expectedSectorCount{};
    std::vector<std::size_t> appendedSides;
    bool finalValidationFailed{};
};

bool validateExtrudeConnectionCandidate(Level& candidate, const SurfaceRef original,
    const SurfaceRef generated, void* const opaque)
{
    auto* const validation = static_cast<ExtrudeConnectionValidation*>(opaque);
    if ( !validation )
    {
        return false;
    }
    validation->appendedSides.clear();
    // A false result is meaningful to connectExtrudeSurfacesChecked: it
    // discards this trial and starts the partitioned construction from the
    // immutable input. Keep the reason until a later trial proves success.
    validation->finalValidationFailed = true;
    if ( candidate.sectors.size() != validation->expectedSectorCount
        || !isValidSurfaceRef(candidate, original) || !isValidSurfaceRef(candidate, generated) )
    {
        return false;
    }
    auto& generatedSector = candidate.sectors[static_cast<std::size_t>(generated.sector)];
    if ( validation->expanded
        && !partitionExtrudeSides(generatedSector, validation->originalGeneratedFaceCount,
            validation->appendedSides) )
    {
        return false;
    }
    if (validation->expanded)
    {
        if (!applyExpandedCapUvProjection(validation->inputSector, validation->inputSurface, generatedSector, 0))
        {
            return false;
        }
        for (std::size_t faceIndex = validation->originalGeneratedFaceCount;
             faceIndex < generatedSector.surfaces.size(); ++faceIndex)
        {
            if (std::find(validation->appendedSides.begin(), validation->appendedSides.end(), faceIndex)
                == validation->appendedSides.end()
                && !applyExpandedCapUvProjection(validation->inputSector, validation->inputSurface,
                    generatedSector, static_cast<std::int32_t>(faceIndex)))
            {
                return false;
            }
        }
    }
    if ( !extrudeResultIsValid(generatedSector, validation->source, validation->preview,
            validation->expanded, validation->plainPrism, validation->originalGeneratedFaceCount,
            validation->appendedSides)
        || (validation->sourceIsSolid
            && !precision::closedBoundary(candidate.sectors[static_cast<std::size_t>(original.sector)]))
        || !sourcePortalsAreValid(candidate, original)
        || !sourcePortalsAreValid(candidate, generated) )
    {
        return false;
    }
    const auto count = validation->preview.innerLoop.size();
    if ( generatedSector.vertices.size() < count + validation->preview.outerLoop.size() )
    {
        return false;
    }
    validation->finalValidationFailed = false;
    return true;
}

std::optional<ExtrudeCandidate> buildExtrudeCandidate(const Level& level, const SurfaceRef requested,
    const double length, const double expand, const bool completeLevel, SurfaceExtrudeReport* report)
{
    auto wire = buildSurfaceExtrudeWire(level, requested, length, expand, report);
    if ( !wire )
    {
        return std::nullopt;
    }
    const auto source = resolveExtrudeSource(level, requested, report);
    if ( !source )
    {
        return std::nullopt;
    }
    const auto& originalSector = level.sectors[requested.sector];
    const bool sourceIsSolid = originalSector.surfaces.size() >= 4;
    if ( sourceIsSolid && !precision::closedBoundary(originalSector) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidSourceTopology, requested);
        return std::nullopt;
    }
    if ( !sourcePortalsAreValid(level, requested)
        || (expand != 0.0 && !precision::extrudeSourceContextIsValid(level, requested)) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidAdjoin, requested);
        return std::nullopt;
    }
    if ( level.curId == std::numeric_limits<std::int32_t>::max()
        || level.sectors.size() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidResult, requested);
        return std::nullopt;
    }
    ExtrudeCandidate result;
    result.source = requested;
    result.originalSourceFaceCount = originalSector.surfaces.size();
    auto& candidate = result.level;
    if ( completeLevel )
    {
        candidate = level;
    }
    else
    {
        // Preview owns only affected geometry. Existing neighbor links were
        // checked against the immutable complete Level above; their geometry
        // does not participate in the unadjoined source face's construction.
        candidate.kind = level.kind;
        candidate.uvCoordinateSpace = level.uvCoordinateSpace;
        candidate.ppunit = level.ppunit;
        candidate.masterCMP = level.getMasterCMP();
        candidate.curId = level.curId;
        candidate.sectors.push_back(originalSector);
        candidate.sectors[0].num = 0;
        for ( auto& face : candidate.sectors[0].surfaces )
        {
            face.adjoin.reset();
            face.nAdj = -1;
        }
        result.source.sector = 0;
    }
    const Sector sourceSnapshot = candidate.sectors[result.source.sector];
    // A zero stored normal is valid input: resolveExtrudeSource derives the
    // geometric normal for admission and construction. Keep the authored
    // source snapshot intact, but use that resolved normal for the temporary
    // source face so plain adjoin validation does not reject a valid zero-cache
    // surface through diagnosticSurfaceNormal.
    Surface sourceFace = sourceSnapshot.surfaces[result.source.surface];
    sourceFace.normal = source->normal;
    Sector& generated = candidate.newSector();
    const bool expanded = expand != 0.0;
    const auto inner = populateSurfaceExtrudeSector(
        candidate, sourceSnapshot, sourceFace, *wire, generated, false, !expanded);
    const std::size_t originalGeneratedFaceCount = wire->innerLoop.size() + 2;
    if ( !inner || !extrudeResultIsValid(generated, *source, *wire, expanded, expand == 0.0, originalGeneratedFaceCount) )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidResult, requested);
        return std::nullopt;
    }
    const SurfaceRef generatedRef{generated.num, *inner};
    bool connected;
    ExtrudeConnectionValidation validation{*source,
        level.sectors[static_cast<std::size_t>(requested.sector)],
        level.sectors[static_cast<std::size_t>(requested.sector)].surfaces[
            static_cast<std::size_t>(requested.surface)],
        *wire, expanded, expand == 0.0, sourceIsSolid,
        originalGeneratedFaceCount, completeLevel ? level.sectors.size() + 1 : 2};
    if ( expanded )
    {
        connected = precision::connectExtrudeSurfacesChecked(candidate, result.source, generatedRef,
            completeLevel ? &level : nullptr, validateExtrudeConnectionCandidate, &validation);
    }
    else
    {
        connected = precision::surfaceBoundariesMatchChecked(candidate.sectors[result.source.sector],
            sourceFace, generated, generated.surfaces[*inner])
            && levelSysAdjoinSurfaces(candidate, result.source, generatedRef, true);
        if ( connected )
        {
            connected = validateExtrudeConnectionCandidate(candidate, result.source, generatedRef, &validation);
        }
    }
    if ( !connected )
    {
        setExtrudeReport(report,
            validation.finalValidationFailed ? SurfaceExtrudeError::InvalidResult
                : SurfaceExtrudeError::InvalidAdjoin,
            requested);
        return std::nullopt;
    }
    // A successful retry clears a diagnostic from a discarded fast path. The
    // transaction now owns only the candidate whose final validator returned
    // true.
    setExtrudeReport(report, SurfaceExtrudeError::None, requested);
    // Connect appends cap fragments and vertices without reordering the initial
    // pool. A lifted inherited boundary therefore updates the actual wire too.
    const auto& pool = candidate.sectors.back().vertices;
    const auto count = wire->innerLoop.size();
    if ( pool.size() < count + wire->outerLoop.size() )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidResult, requested);
        return std::nullopt;
    }
    for ( std::size_t index = 0; index < count; ++index )
    {
        wire->innerLoop[index] = vertexPosition(pool[index]);
        wire->outerLoop[index] = vertexPosition(pool[count + index]);
    }
    result.wire = std::move(*wire);
    return result;
}
} // namespace

std::optional<SurfaceExtrudePreview> levelBuildSurfaceExtrudePreview(
    const Level& level, const SurfaceRef surfaceRef, const double length, const double expand,
    SurfaceExtrudeReport* const report) noexcept
try
{
    setExtrudeReport(report, SurfaceExtrudeError::None, surfaceRef);
    // Every wire preview has the same admission and exact generated endpoints
    // as the solid preview and commit. Staging remains limited to the affected
    // source and generated sectors, even for a large imported level.
    auto candidate = buildExtrudeCandidate(level, surfaceRef, length, expand, false, report);
    return candidate ? std::optional<SurfaceExtrudePreview>{std::move(candidate->wire)} : std::nullopt;
}
catch ( const std::bad_alloc& )
{
    setExtrudeReport(report, SurfaceExtrudeError::AllocationFailure, surfaceRef);
    return std::nullopt;
}

std::optional<Sector> levelBuildSurfaceExtrudeSectorPreview(
    const Level& level, const SurfaceRef surfaceRef, const double length, const double expand,
    SurfaceExtrudeReport* const report) noexcept
try
{
    setExtrudeReport(report, SurfaceExtrudeError::None, surfaceRef);
    auto candidate = buildExtrudeCandidate(level, surfaceRef, length, expand, false, report);
    if ( !candidate )
    {
        return std::nullopt;
    }
    auto& generated = candidate->level.sectors.back();
    for ( auto& face : generated.surfaces )
    {
        if ( face.adjoin )
        {
            face.geo = static_cast<std::int32_t>(TGeoMode::NotDrawn);
            face.adjoin->sector = surfaceRef.sector;
        }
    }
    return std::move(generated);
}
catch ( const std::bad_alloc& )
{
    setExtrudeReport(report, SurfaceExtrudeError::AllocationFailure, surfaceRef);
    return std::nullopt;
}

std::optional<Sector> levelBuildSurfaceExtrudeSourceCutPreview(
    const Level& level, const SurfaceRef surfaceRef, const double expand,
    SurfaceExtrudeReport* const report) noexcept
try
{
    setExtrudeReport(report, SurfaceExtrudeError::None, surfaceRef);
    if ( !std::isfinite(expand) || expand >= 0.0 )
    {
        setExtrudeReport(report, SurfaceExtrudeError::InvalidExpand, surfaceRef);
        return std::nullopt;
    }
    // Retained source fragments are independent of extrusion depth. Build
    // the same bounded candidate for every source footprint, then expose only
    // its visible selected-face descendants (possibly several portal holes).
    auto candidate = buildExtrudeCandidate(level, surfaceRef, 1.0, expand, false, report);
    if ( !candidate )
    {
        return std::nullopt;
    }
    Sector result = std::move(candidate->level.sectors[candidate->source.sector]);
    std::vector<Surface> rims;
    for ( std::size_t index = 0; index < result.surfaces.size(); ++index )
    {
        if ( (index == static_cast<std::size_t>(surfaceRef.surface) || index >= candidate->originalSourceFaceCount)
            && !result.surfaces[index].adjoin )
        {
            rims.push_back(std::move(result.surfaces[index]));
        }
    }
    if ( rims.empty() )
    {
        setExtrudeReport(report, SurfaceExtrudeError::CollapsedExpand, surfaceRef);
        return std::nullopt;
    }
    result.surfaces = std::move(rims);
    renumberSurfaces(result);
    return result;
}

catch ( const std::bad_alloc& )
{
    setExtrudeReport(report, SurfaceExtrudeError::AllocationFailure, surfaceRef);
    return std::nullopt;
}

bool levelExtrudeSurface(Level& level, const SurfaceRef surfaceRef, const double by, const double expand,
    SurfaceExtrudeReport* const report) noexcept
try
{
    setExtrudeReport(report, SurfaceExtrudeError::None, surfaceRef);
    auto candidate = buildExtrudeCandidate(level, surfaceRef, by, expand, true, report);
    if ( !candidate )
    {
        return false;
    }
    static_assert(std::is_nothrow_move_assignable_v<Level>);
    level = std::move(candidate->level);
    return true;
}
catch ( const std::bad_alloc& )
{
    setExtrudeReport(report, SurfaceExtrudeError::AllocationFailure, surfaceRef);
    return false;
}

} // namespace sed::level
