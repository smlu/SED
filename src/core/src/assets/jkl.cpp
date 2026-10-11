#include "sed/assets/jkl.hpp"

#include "atomic_file_write.hpp"
#include "sed/assets/3do.hpp"
#include "sed/assets/items_dat.hpp"
#include "sed/assets/mat.hpp"
#include "sed/assets/material_dimensions.hpp"
#include "sed/assets/par.hpp"
#include "sed/assets/pup.hpp"
#include "sed/assets/cog_script.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/static_resources.hpp"
#include "sed/assets/snd.hpp"
#include "sed/assets/spr.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/format_number.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/arch_lighting.hpp"
#include "sed/level/thing.hpp"
#include "sed/level/uv.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sed::assets
{

namespace
{

using sed::text::iequalsAscii;
using sed::text::istartsWithAscii;
using sed::text::parseDouble;
using sed::text::parseHex;
using sed::text::parseInt;
using sed::text::splitWords;
using sed::text::stripHashComment;
using sed::text::trim;
using sed::text::trimView;
using sed::text::trimLeft;
using sed::text::trimRight;
using sed::text::upperAscii;
using sed::level::MotsArchLightingTopology;

float parseJklFloat(
    const std::string_view token,
    const std::string_view diagnostic)
{
    if ( token.empty() )
    {
        throw std::runtime_error(std::string(diagnostic));
    }

    const char* first = token.data();
    const char* last = first + token.size();
    const bool hasLeadingPlus = *first == '+';
    if ( hasLeadingPlus )
    {
        ++first;
    }
    if ( first == last
        || (hasLeadingPlus && (*first == '+' || *first == '-')) )
    {
        throw std::runtime_error(std::string(diagnostic));
    }

    float value{};
    const auto parsed = std::from_chars(
        first, last, value, std::chars_format::general);
    if ( parsed.ec != std::errc{} || parsed.ptr != last || !std::isfinite(value) )
    {
        throw std::runtime_error(std::string(diagnostic));
    }
    return value;
}

constexpr std::string_view kJklExtraLightFloatDiagnostic =
    "Cannot load JKL/NDY extra light: a color value is invalid or outside float range.";
constexpr std::string_view kJklSectorLightFloatDiagnostic =
    "Cannot load JKL/NDY sector light: a color value is invalid or outside float range.";
constexpr std::string_view kJklSurfaceColorFloatDiagnostic =
    "Cannot load JKL/NDY surface: a color value is invalid or outside float range.";
constexpr std::string_view kJklFogColorFloatDiagnostic =
    "Cannot load JKL/NDY fog: a color value is invalid or outside float range.";
constexpr std::string_view kJklEditorLightColorFloatDiagnostic =
    "Cannot load JKL/NDY editor light: a color value is invalid or outside float range.";
constexpr std::string_view kJklSectorCountDiagnostic =
    "Cannot load JKL/NDY sector: declared sector count is invalid or truncated.";
constexpr std::string_view kJklSectorMissingSurfacesDiagnostic =
    "Cannot load JKL/NDY sector: SURFACES record is missing.";
constexpr std::string_view kJklWorldVertexIndexDiagnostic =
    "Cannot load JKL/NDY surface: world vertex index is invalid.";
constexpr std::string_view kJklSectorVertexCountDiagnostic =
    "Cannot load JKL/NDY sector: VERTICES count is invalid or truncated.";

constexpr std::string_view kLecLogoJK = R"(................................
................@...@...@...@...
.............@...@..@..@...@....
................@.@.@.@.@.@.....
@@@@@@@@......@...........@.....
@@@@@@@@....@@......@@@....@....
@@.....@.....@......@@@.....@@..
@@.@@@@@......@.....@@@......@@.
@@@@@@@@.......@....@@.....@@...
@@@@@@@@.........@@@@@@@@@@.....
@@@@@@@@..........@@@@@@........
@@.....@..........@@@@@.........
@@.@@@@@.........@@@@@@.........
@@.....@.........@@@@@@.........
@@@@@@@@.........@@@@@@.........
@@@@@@@@.........@@@@@@@........
@@@...@@.........@@@@@@@........
@@.@@@.@.........@.....@........
@@..@..@........@.......@.......
@@@@@@@@........@.......@.......
@@@@@@@@.......@........@.......
@@..@@@@.......@........@.......
@@@@..@@......@.........@.......
@@@@.@.@......@.........@.......
@@....@@........................
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@.@@@@@@@@@@@@@@@@@@
@@.@@..@@@@@..@@@@@@@@@@.@@@@@@@
@@.@.@.@@@@.@.@@@.@..@@...@@@..@
@@..@@@@@@....@@@..@@@@@.@@@@.@@
@@@@@@@@...@@.@@@.@@@@@..@@...@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@.copyright.(c).1997.lucasarts.@
@@@@@@..entertainment.co..@@@@@@)";

constexpr std::string_view kLecLogoIJIM = R"(................................
................@...@...@...@...
.............@...@..@..@...@....
................@.@.@.@.@.@.....
@@@@@@@@......@...........@.....
@@@@@@@@....@@......@@@....@....
@@.....@.....@......@@@.....@@..
@@.@@@@@......@.....@@@......@@.
@@@@@@@@.......@....@@.....@@...
@@@@@@@@.........@@@@@@@@@@.....
@@@@@@@@..........@@@@@@........
@@.....@..........@@@@@.........
@@.@@@@@.........@@@@@@.........
@@.....@.........@@@@@@.........
@@@@@@@@.........@@@@@@.........
@@@@@@@@.........@@@@@@@........
@@@...@@.........@@@@@@@........
@@.@@@.@.........@.....@........
@@..@..@........@.......@.......
@@@@@@@@........@.......@.......
@@@@@@@@.......@........@.......
@@..@@@@.......@........@.......
@@@@..@@......@.........@.......
@@@@.@.@......@.........@.......
@@....@@........................
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@@.@@@@@@@@@@@@@@@@@@
@@.@@..@@@@@..@@@@@@@@@@.@@@@@@@
@@.@.@.@@@@.@.@@@.@..@@...@@@..@
@@..@@@@@@....@@@..@@@@@.@@@@.@@
@@@@@@@@...@@.@@@.@@@@@..@@...@@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@.(c).lucasarts.entertainment..@
@.........company.llc..........@
@....(c).lucasfilm.ltd.&.tm....@
@.....all.rights.reserved......@
@...used.under.authorization...@
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@)";

float parseJklUvFloatOr(
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
            "Cannot load JKL/NDY texture vertex: a UV value is invalid or outside float range.");
    }
    return static_cast<float>(*value);
}

float parseJklExtraLightFloat(const std::string_view token)
{
    return parseJklFloat(token, kJklExtraLightFloatDiagnostic);
}

float parseJklSectorLightFloat(const std::string_view token)
{
    return parseJklFloat(token, kJklSectorLightFloatDiagnostic);
}

float parseMotsArchLightFloat(const std::string_view token)
{
    return parseJklFloat(token,
        "Cannot load JKL/NDY ARCHLIGHTING: a vertex channel is invalid or outside float range.");
}

double classicExtraLightIntensity(const ColorF& color)
{
    const double value = (static_cast<double>(color.r)
        + static_cast<double>(color.g) + static_cast<double>(color.b)) / 3.0;
    if (!std::isfinite(value))
    {
        throw std::runtime_error("Cannot export JKL/NDY extra light: intensity is not finite.");
    }
    return value;
}

void validateExtraLightColor(const ColorF& color)
{
    if (!std::isfinite(color.r) || !std::isfinite(color.g)
        || !std::isfinite(color.b) || !std::isfinite(color.a))
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY extra light: a color value is not finite.");
    }
}

void validateSectorLightColor(const ColorF& color)
{
    if (!std::isfinite(color.r) || !std::isfinite(color.g)
        || !std::isfinite(color.b) || !std::isfinite(color.a))
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY sector light: a color value is not finite.");
    }
}

double classicSectorLightIntensity(const ColorF& color)
{
    validateSectorLightColor(color);
    // Do the average in double. ColorF::intensity() adds float channels first,
    // which can overflow for valid finite HDR sector values.
    return (static_cast<double>(color.r)
        + static_cast<double>(color.g)
        + static_cast<double>(color.b)) / 3.0;
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

class ScopedStreamLocale
{
public:
    explicit ScopedStreamLocale(std::ostream& out)
        : out_(out), locale_(out.getloc())
    {
    }

    ~ScopedStreamLocale()
    {
        out_.imbue(locale_);
    }

    ScopedStreamLocale(const ScopedStreamLocale&) = delete;
    ScopedStreamLocale& operator=(const ScopedStreamLocale&) = delete;

private:
    std::ostream& out_;
    std::locale locale_;
};

bool compactTextWhitespace(const char c)
{
    return c == ' ' || c == '\t' || c == '\f' || c == '\v';
}

std::string compactDecimal(std::string_view value)
{
    // The normal writer has already rounded values according to the engine
    // field's precision.  Parse that rounded spelling and print the shortest
    // finite round-trippable spelling of the same value.  This is more than
    // trimming zeroes: values such as 0.000001 become 1e-6 when that is
    // shorter, while signed zero remains "-0".  Do not run this over an
    // arbitrary string: the caller has already proven that the whole token
    // is a numeric field and that it is delimited from resource/user text.
    char* end = nullptr;
    const std::string owned(value);
    const double parsed = std::strtod(owned.c_str(), &end);
    if (end != owned.c_str() + owned.size() || !std::isfinite(parsed))
    {
        return std::string(value);
    }
    if (parsed == 0.0 && std::signbit(parsed))
    {
        // A few standard-library implementations historically normalized
        // negative zero in to_chars(general).  Preserve the authored sign
        // explicitly because it is observable in a lossless Level reload.
        return "-0";
    }

    char buffer[128]{};
    const auto converted = std::to_chars(
        std::begin(buffer), std::end(buffer), parsed, std::chars_format::general);
    if (converted.ec != std::errc{})
    {
        return std::string(value);
    }

    std::string compact(buffer, converted.ptr);
    const std::size_t exponentPos = compact.find_first_of("eE");
    if (exponentPos != std::string::npos)
    {
        std::string mantissa = compact.substr(0, exponentPos);
        while (mantissa.size() > 1 && mantissa.back() == '0'
            && mantissa.find('.') != std::string::npos)
        {
            mantissa.pop_back();
        }
        if (!mantissa.empty() && mantissa.back() == '.')
        {
            mantissa.pop_back();
        }
        std::string exponent = compact.substr(exponentPos + 1);
        char exponentSign = '\0';
        if (!exponent.empty() && (exponent.front() == '+' || exponent.front() == '-'))
        {
            exponentSign = exponent.front();
            exponent.erase(exponent.begin());
        }
        const std::size_t firstSignificant = exponent.find_first_not_of('0');
        if (firstSignificant == std::string::npos)
        {
            exponent = "0";
        }
        else
        {
            exponent.erase(0, firstSignificant);
        }
        if (exponentSign == '+')
        {
            // `e8` is the same finite value as `e+08` and saves two bytes.
            exponentSign = '\0';
        }
        compact = std::move(mantissa) + 'e'
            + (exponentSign == '-' ? std::string("-") : std::string{})
            + exponent;
    }
    else if (const std::size_t dotPos = compact.find('.'); dotPos != std::string::npos)
    {
        while (compact.size() > dotPos + 1 && compact.back() == '0')
        {
            compact.pop_back();
        }
        if (!compact.empty() && compact.back() == '.')
        {
            compact.pop_back();
        }
    }

    // A leading zero before a fractional value carries no information in the
    // legacy parser.  Keeping the sign gives `-.015` its intended meaning.
    const std::size_t compactExponentPos = compact.find_first_of("eE");
    const std::size_t compactMantissaEnd = compactExponentPos == std::string::npos
        ? compact.size()
        : compactExponentPos;
    if (compactMantissaEnd >= 2 && compact[0] == '0' && compact[1] == '.')
    {
        compact.erase(0, 1);
    }
    else if (compactMantissaEnd >= 3
        && compact[0] == '-'
        && compact[1] == '0'
        && compact[2] == '.')
    {
        compact.erase(1, 1);
    }
    return compact;
}

struct PendingJklNoMaterialUv
{
    SurfaceRef reference{};
    Surface surface{};
};

struct JklNoMaterialUvPreparationResult
{
    std::optional<SurfaceRef> failedSurface{};
    std::optional<std::int32_t> invalidMaterialIndex{};
    std::string failureReason{};

    explicit operator bool() const noexcept
    {
        return !failedSurface.has_value();
    }
};

std::optional<std::int64_t> tryJklWeldCellCoordinate(
    const double coordinate,
    const double cellSize) noexcept
{
    const long double quotient = std::floor(
        static_cast<long double>(coordinate)
        / static_cast<long double>(cellSize));
    // MSVC uses the same precision for long double as double.  Use the exact
    // 2^63 exclusive upper bound instead of converting INT64_MAX, which rounds
    // to 2^63 in that mode and would make the narrowing cast undefined.
    const long double lower = -std::ldexp(1.0L, 63);
    const long double upper = std::ldexp(1.0L, 63);
    if (!std::isfinite(quotient) || quotient < lower || quotient >= upper)
    {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(quotient);
}

JklNoMaterialUvPreparationResult prepareJklNoMaterialUvDefaults(
    const Level& level,
    const double pixelsPerUnit,
    std::vector<PendingJklNoMaterialUv>& pending)
{
    for (std::size_t sectorIndex = 0;
         sectorIndex < level.sectors.size();
         ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0;
             surfaceIndex < sector.surfaces.size();
             ++surfaceIndex)
        {
            const Surface& source = sector.surfaces[surfaceIndex];
            if (!trimView(source.material).empty()
                && !iequalsAscii(trimView(source.material), "none"))
            {
                continue;
            }
            if (source.nMat != -1 && trimView(source.material).empty())
            {
                std::string reason{};
                if (source.nMat < 0)
                {
                    reason = "negative indices are reserved for material-less surfaces";
                }
                else if (source.nMat >= StaticResIdxMask)
                {
                    const std::int32_t staticIndex = source.nMat & ~StaticResIdxMask;
                    reason = "static material index " + std::to_string(staticIndex)
                        + " is not defined for this project";
                }
                else
                {
                    reason = "world material index is outside the "
                        + std::to_string(level.materials.size())
                        + "-entry material table";
                }
                return JklNoMaterialUvPreparationResult{
                    SurfaceRef{static_cast<std::int32_t>(sectorIndex),
                        static_cast<std::int32_t>(surfaceIndex)},
                    source.nMat,
                    std::move(reason)};
            }
            if (source.vertices.size() != source.texVertices.size()
                || !std::isfinite(source.uScale)
                || !std::isfinite(source.vScale))
            {
                return JklNoMaterialUvPreparationResult{
                    SurfaceRef{static_cast<std::int32_t>(sectorIndex),
                        static_cast<std::int32_t>(surfaceIndex)},
                    std::nullopt,
                    "material-less surface has mismatched vertex data or non-finite texture scale"};
            }
            if (std::any_of(
                    source.texVertices.cbegin(),
                    source.texVertices.cend(),
                    [](const TexVertex& vertex)
                    {
                        return !std::isfinite(vertex.u)
                            || !std::isfinite(vertex.v);
                    }))
            {
                return JklNoMaterialUvPreparationResult{
                    SurfaceRef{static_cast<std::int32_t>(sectorIndex),
                        static_cast<std::int32_t>(surfaceIndex)},
                    std::nullopt,
                    "material-less surface has a non-finite texture coordinate"};
            }
            if (std::none_of(
                    source.texVertices.cbegin(),
                    source.texVertices.cend(),
                    [](const TexVertex& vertex)
                    {
                        return vertex.u != 0.0f || vertex.v != 0.0f;
                    }))
            {
                continue;
            }

            Surface mapped = source;
            mapped.normal = level::surfaceCalcNormalNewell(sector, mapped);
            TVector un{};
            TVector vn{};
            for (std::int32_t originVertex = 0;
                 originVertex < static_cast<std::int32_t>(mapped.vertices.size());
                 ++originVertex)
            {
                level::surfaceCalcDefaultUVNormals(
                    sector, mapped, originVertex, un, vn);
                if (un.lengthSqr() > 0.0 && vn.lengthSqr() > 0.0)
                {
                    break;
                }
            }
            if (!std::isfinite(un.x) || !std::isfinite(un.y)
                || !std::isfinite(un.z) || !std::isfinite(vn.x)
                || !std::isfinite(vn.y) || !std::isfinite(vn.z)
                || un.lengthSqr() <= 0.0 || vn.lengthSqr() <= 0.0)
            {
                mapped.uScale = 1.0f;
                mapped.vScale = 1.0f;
                mapped.faceFlags &= ~FF_SF_FLIP;
                for (TexVertex& vertex : mapped.texVertices)
                {
                    vertex.u = 0.0f;
                    vertex.v = 0.0f;
                }
                pending.push_back(PendingJklNoMaterialUv{
                    SurfaceRef{static_cast<std::int32_t>(sectorIndex),
                        static_cast<std::int32_t>(surfaceIndex)},
                    std::move(mapped)});
                continue;
            }

            const TVector origin = [&]()
            {
                const std::int32_t index = mapped.vertices.front();
                return index >= 0 && static_cast<std::size_t>(index) < sector.vertices.size()
                    ? TVector{sector.vertices[static_cast<std::size_t>(index)].x,
                        sector.vertices[static_cast<std::size_t>(index)].y,
                        sector.vertices[static_cast<std::size_t>(index)].z}
                    : TVector::zero;
            }();
            double minU = 0.0;
            double maxU = 0.0;
            double minV = 0.0;
            double maxV = 0.0;
            bool valid = !mapped.vertices.empty();
            for (std::size_t corner = 0; valid && corner < mapped.vertices.size(); ++corner)
            {
                const std::int32_t index = mapped.vertices[corner];
                if (index < 0 || static_cast<std::size_t>(index) >= sector.vertices.size())
                {
                    valid = false;
                    break;
                }
                const Vertex& vertex = sector.vertices[static_cast<std::size_t>(index)];
                const TVector position{vertex.x, vertex.y, vertex.z};
                const TVector offset = position - origin;
                const double u = un.dot(offset);
                const double v = vn.dot(offset);
                if (!std::isfinite(u) || !std::isfinite(v))
                {
                    valid = false;
                    break;
                }
                if (corner == 0)
                {
                    minU = maxU = u;
                    minV = maxV = v;
                }
                else
                {
                    minU = std::min(minU, u);
                    maxU = std::max(maxU, u);
                    minV = std::min(minV, v);
                    maxV = std::max(maxV, v);
                }
            }
            const double extent = std::max(maxU - minU, maxV - minV);
            if (!valid || !std::isfinite(extent) || extent <= 0.0
                || !std::isfinite(pixelsPerUnit) || pixelsPerUnit <= 0.0)
            {
                mapped.uScale = 1.0f;
                mapped.vScale = 1.0f;
                mapped.faceFlags &= ~FF_SF_FLIP;
                for (TexVertex& vertex : mapped.texVertices)
                {
                    vertex.u = 0.0f;
                    vertex.v = 0.0f;
                }
            }
            else
            {
                const double normalizedScale = 1.0 / extent;
                const double scale = normalizedScale / pixelsPerUnit;
                if (!std::isfinite(scale)
                    || scale < -static_cast<double>(std::numeric_limits<float>::max())
                    || scale > static_cast<double>(std::numeric_limits<float>::max()))
                {
                    mapped.uScale = 1.0f;
                    mapped.vScale = 1.0f;
                    mapped.faceFlags &= ~FF_SF_FLIP;
                    for (TexVertex& vertex : mapped.texVertices)
                    {
                        vertex.u = 0.0f;
                        vertex.v = 0.0f;
                    }
                }
                else
                {
                    mapped.uScale = static_cast<float>(scale);
                    mapped.vScale = static_cast<float>(scale);
                    for (std::size_t corner = 0; corner < mapped.vertices.size(); ++corner)
                    {
                        const Vertex& vertex = sector.vertices[
                            static_cast<std::size_t>(mapped.vertices[corner])];
                        const TVector offset = TVector{vertex.x, vertex.y, vertex.z} - origin;
                        mapped.texVertices[corner].u = static_cast<float>((un.dot(offset) - minU) / extent);
                        mapped.texVertices[corner].v = static_cast<float>((vn.dot(offset) - minV) / extent);
                    }
                    mapped.faceFlags = level::surfaceIsTextureFlipped(sector, mapped)
                        ? mapped.faceFlags | FF_SF_FLIP
                        : mapped.faceFlags & ~FF_SF_FLIP;
                }
            }
            pending.push_back(PendingJklNoMaterialUv{
                SurfaceRef{static_cast<std::int32_t>(sectorIndex),
                    static_cast<std::int32_t>(surfaceIndex)},
                std::move(mapped)});
        }
    }
    return {};
}

std::string compactNumberFragments(
    std::string line,
    const std::array<bool, 8>& protectedFields = {},
    const int protectFromField = -1)
{
    std::string result;
    result.reserve(line.size());
    bool quoted = false;
    bool escaped = false;
    int parenthesisDepth = 0;
    int fieldIndex = 0;

    const auto numericDelimiter = [](const char c, const bool inParentheses)
    {
        if (c == '\0' || c == '\t' || c == ')')
        {
            return true;
        }
        // Slash/comma are numeric separators only inside the parenthesized
        // vector/color syntax.  Treating every slash as a token boundary
        // would rewrite resource names such as `folder/0.015foo`.
        return inParentheses && (c == '/' || c == ',');
    };

    for (std::size_t i = 0; i < line.size();)
    {
        const char c = line[i];
        if (quoted)
        {
            result.push_back(c);
            if (escaped)
            {
                escaped = false;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '"')
            {
                quoted = false;
            }
            ++i;
            continue;
        }

        if (c == '"')
        {
            quoted = true;
            result.push_back(c);
            ++i;
            continue;
        }

        // Do not treat '=' as a numeric boundary.  It begins a serialized
        // Thing/template parameter whose unknown type may legitimately be a
        // user string such as `custom=001.5`; rewriting that would not be
        // lossless.  The caller protects complete `name=value` fields,
        // including tuples, while ordinary level fields use tab-separated
        // tokens.
        const bool boundary = i == 0
            || line[i - 1] == '('
            || line[i - 1] == '\t'
            || (parenthesisDepth > 0
                && (line[i - 1] == ',' || line[i - 1] == '/'));
        const bool signedNumber = (c == '+' || c == '-')
            && i + 1 < line.size()
            && (std::isdigit(static_cast<unsigned char>(line[i + 1]))
                || line[i + 1] == '.');
        const bool unsignedNumber = std::isdigit(static_cast<unsigned char>(c))
            || (c == '.' && i + 1 < line.size()
                && std::isdigit(static_cast<unsigned char>(line[i + 1])));

        const bool protectedByArray = fieldIndex >= 0
            && fieldIndex < static_cast<int>(protectedFields.size())
            && protectedFields[static_cast<std::size_t>(fieldIndex)];
        const bool protectedByRange = protectFromField >= 0 && fieldIndex >= protectFromField;
        if (boundary && (signedNumber || unsignedNumber)
            && !protectedByArray
            && !protectedByRange)
        {
            const char* begin = line.data() + i;
            char* end = nullptr;
            (void)std::strtod(begin, &end);
            if (end != nullptr && end > begin)
            {
                const std::size_t consumed = static_cast<std::size_t>(end - begin);
                const std::string_view number(begin, consumed);
                const bool endsAtDelimiter = end == line.data() + line.size()
                    || numericDelimiter(*end, parenthesisDepth > 0);
                if (endsAtDelimiter
                    && number.find_first_of(".eE") != std::string_view::npos)
                {
                    result += compactDecimal(number);
                    i += consumed;
                    continue;
                }
            }
        }

        result.push_back(c);
        if (c == '(')
        {
            ++parenthesisDepth;
        }
        else if (c == ')' && parenthesisDepth > 0)
        {
            --parenthesisDepth;
        }
        else if (c == '\t' && parenthesisDepth == 0)
        {
            ++fieldIndex;
        }
        ++i;
    }
    return result;
}

std::string compactJklText(const std::string_view source)
{
    std::string result;
    result.reserve(source.size());
    bool inCopyright = false;
    std::string currentSection;
    bool geoColormapRecords = false;
    // Layer names and the optional LVIS string are opaque user text.  The
    // parser consumes those rows as whole lines, so converting their spaces
    // to field separators would silently change the authored value.  Keep a
    // tiny amount of section framing state to compact only the two numeric
    // membership rows belonging to each layer.
    std::size_t layersExpected = 0;
    std::size_t layersDataLines = 0;
    bool layersHeaderSeen = false;
    std::size_t lineStart = 0;

    while (lineStart <= source.size())
    {
        const std::size_t lineEnd = source.find('\n', lineStart);
        const std::size_t length = lineEnd == std::string_view::npos
            ? source.size() - lineStart
            : lineEnd - lineStart;
        std::string line(source.substr(lineStart, length));
        const std::string trimmed = std::string(trimView(line));
        const std::string upper = upperAscii(trimmed);
        const bool startsSection = upper.rfind("SECTION:", 0) == 0;
        const bool isWriterFieldComment = trimmed == "#Num\tScript\tSymbol values"
            || trimmed == "#Name:\tBased On:\tParams:";
        bool preserveRawLayerLine = false;
        if (!startsSection && iequalsAscii(currentSection, "LAYERS")
            && !iequalsAscii(trimmed, "END"))
        {
            if (!layersHeaderSeen)
            {
                const std::vector<std::string_view> words = splitWords(trimmed);
                if (words.size() >= 3
                    && iequalsAscii(words[0], "EDITOR")
                    && iequalsAscii(words[1], "LAYERS"))
                {
                    layersExpected = parseInt<std::size_t>(words[2]).value_or(0);
                    layersDataLines = 0;
                    layersHeaderSeen = true;
                }
            }
            else
            {
                const std::size_t layerRows = layersExpected * 3;
                // Every layer is serialized as: opaque name, sector indices,
                // thing indices.  Once those rows are consumed, any next
                // non-END row is the opaque LVIS string.
                preserveRawLayerLine = layersDataLines >= layerRows
                    || (layersDataLines % 3) == 0;
                ++layersDataLines;
            }
        }
        bool entersCopyright = false;
        if (startsSection)
        {
            const std::string sectionName = std::string(trimView(
                std::string_view(trimmed).substr(std::string_view("SECTION:").size())));
            entersCopyright = iequalsAscii(sectionName, "COPYRIGHT");
        }

        // The legacy writer's COPYRIGHT block is terminated by the next
        // SECTION header rather than an END line.  Stop preserving verbatim
        // at either boundary so compacting applies to all following fields.
        const bool leavesCopyright = inCopyright
            && (iequalsAscii(trimmed, "END")
                || (startsSection && !entersCopyright));
        // The writer itself emits LF, but keep a CR byte inside the legacy
        // copyright payload if a caller ever feeds this presentation pass
        // CRLF text. That block is intentionally byte-for-byte preserved;
        // only non-copyright lines normalize host line endings.
        if ((!inCopyright || leavesCopyright)
            && !line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (inCopyright && !leavesCopyright)
        {
            result += line;
            result.push_back('\n');
        }
        else if (preserveRawLayerLine)
        {
            // Preserve even an empty layer name: blank rows are meaningful in
            // this line-oriented section and dropping one would shift every
            // following layer's membership records.
            result += line;
            result.push_back('\n');
        }
        else if (!trimmed.empty()
            // These two exact lines are writer-only field-name comments.  Do
            // not drop arbitrary '#' records: a user layer/visibility string
            // or other project text can legally begin with that character.
            && !isWriterFieldComment)
        {
            std::string compactLine;
            compactLine.reserve(line.size());
            bool quoted = false;
            bool escaped = false;
            for (const char c : line)
            {
                if (quoted)
                {
                    compactLine.push_back(c);
                    if (escaped)
                    {
                        escaped = false;
                    }
                    else if (c == '\\')
                    {
                        escaped = true;
                    }
                    else if (c == '"')
                    {
                        quoted = false;
                    }
                }
                else if (c == '"')
                {
                    quoted = true;
                    compactLine.push_back(c);
                }
                else if (compactTextWhitespace(c))
                {
                    if (!compactLine.empty() && compactLine.back() != '\t')
                    {
                        compactLine.push_back('\t');
                    }
                }
                else
                {
                    compactLine.push_back(c);
                }
            }
            while (!compactLine.empty() && compactLine.back() == '\t')
            {
                compactLine.pop_back();
            }
            if (!compactLine.empty() && compactLine.front() == '\t')
            {
                compactLine.erase(compactLine.begin());
            }
            std::array<bool, 8> protectedFields{};
            int protectFromField = -1;
            const std::string compactUpper = upperAscii(compactLine);
            const bool indexedRecord = compactLine.find(':') != std::string::npos;
            // Every `name=value` field can be an unknown/user-defined Thing
            // parameter.  Preserve the complete field, including tuple-like
            // text such as `custom=(001.5000/2)`, instead of guessing whether
            // its spelling is numeric.  Runtime/geometry fields are not
            // serialized with '=' and remain eligible for compaction.
            {
                std::size_t fieldStart = 0;
                int field = 0;
                while (fieldStart <= compactLine.size())
                {
                    const std::size_t fieldEnd = compactLine.find('\t', fieldStart);
                    const std::string_view fieldText = std::string_view(compactLine).substr(
                        fieldStart,
                        fieldEnd == std::string::npos ? std::string::npos : fieldEnd - fieldStart);
                    if (fieldText.find('=') != std::string_view::npos)
                    {
                        if (field < static_cast<int>(protectedFields.size()))
                        {
                            protectedFields[static_cast<std::size_t>(field)] = true;
                        }
                        if (protectFromField < 0 || field < protectFromField)
                        {
                            // Parameter fields are emitted as a contiguous
                            // suffix in Thing/template rows.  Protecting from
                            // the first '=' handles arbitrarily many unknown
                            // parameters without a fixed field-count limit.
                            protectFromField = field;
                        }
                    }
                    if (fieldEnd == std::string::npos)
                    {
                        break;
                    }
                    fieldStart = fieldEnd + 1;
                    ++field;
                }
            }
            const auto protectIndexedResourceName = [&protectedFields, indexedRecord]()
            {
                if (indexedRecord)
                {
                    // `index: resource-name ...`: the first value after the
                    // colon is a user/resource spelling, not necessarily a
                    // number.  Protect it even when the name is `0.015`.
                    protectedFields[1] = true;
                }
            };

            if (iequalsAscii(currentSection, "SOUNDS"))
            {
                // Sound entries are one raw resource spelling per line.
                protectedFields[0] = true;
            }
            else if (iequalsAscii(currentSection, "MATERIALS")
                || iequalsAscii(currentSection, "AICLASS")
                || iequalsAscii(currentSection, "MODELS")
                || iequalsAscii(currentSection, "SPRITES")
                || iequalsAscii(currentSection, "KEYFRAMES")
                || iequalsAscii(currentSection, "ANIMCLASS")
                || iequalsAscii(currentSection, "SOUNDCLASS")
                || iequalsAscii(currentSection, "COGSCRIPTS")
                || iequalsAscii(currentSection, "COGS"))
            {
                protectIndexedResourceName();
                if (iequalsAscii(currentSection, "COGS"))
                {
                    // Every value after the script name can carry typed or
                    // unknown user text.  Protect all of those fields rather
                    // than guessing from their current CogType metadata.
                    protectFromField = 1;
                }
            }
            else if (iequalsAscii(currentSection, "GEORESOURCE") && geoColormapRecords)
            {
                protectIndexedResourceName();
            }
            else if (iequalsAscii(currentSection, "TEMPLATES"))
            {
                // Template and parent names occupy the first two fields;
                // all parameter name=value fields were protected above.
                protectedFields[0] = true;
                protectedFields[1] = true;
            }
            else if (iequalsAscii(currentSection, "THINGS") && indexedRecord)
            {
                // Thing rows are index: template-name instance-name x y z...
                protectedFields[1] = true;
                protectedFields[2] = true;
            }
            else if (iequalsAscii(currentSection, "SECTORS")
                && compactUpper.rfind("SOUND\t", 0) == 0)
            {
                // SOUND <resource> <volume> has a raw resource spelling in
                // its second field, which may itself look numeric.
                protectedFields[1] = true;
            }
            else if (iequalsAscii(currentSection, "LAYERS"))
            {
                // The first row of each layer is its user-provided name.
                // Protecting field zero also costs nothing for membership
                // rows, whose first token is an integer count.
                protectedFields[0] = true;
            }

            result += compactNumberFragments(
                std::move(compactLine), protectedFields, protectFromField);
            result.push_back('\n');
        }

        if (entersCopyright)
        {
            inCopyright = true;
        }
        else if (leavesCopyright)
        {
            inCopyright = false;
        }

        if (startsSection)
        {
            currentSection = upperAscii(trimView(
                std::string_view(trimmed).substr(std::string_view("SECTION:").size())));
            geoColormapRecords = iequalsAscii(currentSection, "GEORESOURCE");
            layersExpected = 0;
            layersDataLines = 0;
            layersHeaderSeen = false;
        }
        else if (iequalsAscii(trimmed, "END"))
        {
            currentSection.clear();
            geoColormapRecords = false;
        }
        else if (iequalsAscii(currentSection, "GEORESOURCE"))
        {
            // GEORESOURCE starts with the optional colormap name list; all
            // subsequent WORLD records are numeric geometry/index data.
            const std::string lineUpper = upperAscii(trimmed);
            if (lineUpper.rfind("WORLD VERTICES", 0) == 0
                || lineUpper.rfind("WORLD TEXTURE VERTICES", 0) == 0
                || lineUpper.rfind("WORLD ADJOINS", 0) == 0
                || lineUpper.rfind("WORLD SURFACES", 0) == 0)
            {
                geoColormapRecords = false;
            }
        }

        if (lineEnd == std::string_view::npos)
        {
            break;
        }
        lineStart = lineEnd + 1;
    }
    return result;
}

class CompactOutputScope
{
public:
    CompactOutputScope(std::ostream& output, const bool enabled)
        : output_(output), enabled_(enabled)
    {
        if (enabled_)
        {
            originalBuffer_ = output_.rdbuf(buffer_.rdbuf());
        }
    }

    ~CompactOutputScope()
    {
        if (enabled_)
        {
            output_.rdbuf(originalBuffer_);
        }
    }

    void finish()
    {
        if (!enabled_)
        {
            return;
        }

        output_.rdbuf(originalBuffer_);
        enabled_ = false;
        const std::string compact = compactJklText(buffer_.str());
        output_.write(compact.data(), static_cast<std::streamsize>(compact.size()));
    }

private:
    std::ostream& output_;
    std::ostringstream buffer_{};
    std::streambuf* originalBuffer_{};
    bool enabled_{};
};

void writeHex(std::ostream& out, const std::uint32_t value)
{
    ScopedStreamState scoped(out);
    out << "0x" << std::hex << std::nouppercase << value;
}

void setDefaultHeader(Level& level, const ProjectType projectType)
{
    level.header = LevelHeader{};
    level.header.version = (projectType == ProjectType::IJIM) ? 3 : 1;
    level.header.gravity = (projectType == ProjectType::IJIM) ? 2.0 : 4.0;

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

struct AdjoinTemp
{
    std::int32_t mirror{};
    std::int32_t surf{-1};
    std::uint32_t flags{};
};

struct TexVertexTemp
{
    float u{};
    float v{};
};

struct VertexTemp
{
    double x{};
    double y{};
    double z{};
};

struct SurfaceTemp
{
    std::int32_t nMat{};
    std::int32_t surfFlags{};
    std::uint32_t faceFlags{};
    std::int32_t geo{};
    std::int32_t light{};
    std::int32_t tex{};
    std::int32_t nAdj{};
    ColorF extraLight{ColorBlack};
    std::vector<std::int32_t> vxs;
    std::vector<std::int32_t> tvxs;
    std::vector<ColorF> colors;
};

std::optional<std::int32_t> parseLineIndexToken(std::string_view token)
{
    token = trimView(token);
    if (!token.empty() && token.back() == ':')
    {
        token.remove_suffix(1);
    }
    return parseInt<std::int32_t>(token);
}

constexpr std::size_t kMaxMotsArchLightingRecords = 1'000'000;

std::optional<std::int32_t> parseArchLabelIndex(const std::string_view token)
{
    return parseLineIndexToken(token);
}

std::optional<std::int32_t> parseArchCount(
    const std::vector<std::string_view>& tokens)
{
    if (tokens.empty())
    {
        return std::nullopt;
    }

    const std::optional<std::int64_t> parsed =
        parseInt<std::int64_t>(tokens.back());
    if (!parsed || *parsed < 0
        || static_cast<std::uint64_t>(*parsed)
            > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())
        || static_cast<std::uint64_t>(*parsed)
            > static_cast<std::uint64_t>(kMaxMotsArchLightingRecords))
    {
        return std::nullopt;
    }
    return static_cast<std::int32_t>(*parsed);
}

struct MotsArchLightingExportSnapshot
{
    std::vector<MotsArchLightObject> objects{};
    std::vector<std::int32_t> thingIndices{};
};

std::string thingExportLabel(const Thing& thing, const std::size_t index)
{
    const std::string_view name = trimView(thing.name);
    return "Thing " + std::to_string(index) + " ('"
        + (name.empty() ? std::string("<unnamed>") : std::string(name)) + "')";
}

MotsArchLightingTopology resolveMotsArchLightingTopology(
    const Level& level,
    const Thing& thing,
    const std::size_t thingIndex,
    const JklSaveOptions& options,
    const bool forDefaultSynthesis = true)
{
    if (options.resources == nullptr)
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY ARCHLIGHTING: "
            + thingExportLabel(thing, thingIndex)
            + (forDefaultSynthesis
                ? " has no ARCH record; resources are required to size its 3DO default."
                : " requires resources to validate its ARCH record topology."));
    }

    const TplValue* modelValue = sed::level::thingFindValue(
        level, options.templatesDb, thing, "model3d");
    if (!modelValue)
    {
        modelValue = sed::level::thingFindValue(
            level, options.templatesDb, thing, "model3do");
    }
    const std::string modelName = modelValue
        ? std::string(trimView(modelValue->asString())) : std::string{};
    if (modelName.empty() || iequalsAscii(modelName, "none"))
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY ARCHLIGHTING: "
            + thingExportLabel(thing, thingIndex)
            + (forDefaultSynthesis
                ? " has no usable model3d/model3do value for its default record."
                : " has no usable model3d/model3do value; its ARCH topology cannot be checked."));
    }

    const std::optional<std::string> modelText =
        options.resources->tryReadTextWithExt(modelName, ".3do");
    if (!modelText)
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY ARCHLIGHTING: "
            + thingExportLabel(thing, thingIndex)
            + " references missing 3DO '" + modelName
            + (forDefaultSynthesis
                ? "'; its default record cannot be sized."
                : "'; its ARCH topology cannot be checked."));
    }

    std::size_t geosetCount = 1;
    {
        std::istringstream lines(*modelText);
        std::string line{};
        while (std::getline(lines, line))
        {
            const std::vector<std::string_view> tokens = splitWords(line);
            if (tokens.size() >= 2 && iequalsAscii(tokens[0], "GEOSETS"))
            {
                const std::optional<std::int32_t> parsed =
                    parseInt<std::int32_t>(tokens[1]);
                if (!parsed || *parsed <= 0 || *parsed > 1024)
                {
                    throw std::runtime_error(
                        "Cannot export JKL/NDY ARCHLIGHTING: 3DO '" + modelName
                        + "' has an invalid GEOSETS count.");
                }
                geosetCount = static_cast<std::size_t>(*parsed);
                break;
            }
        }
    }

    MotsArchLightingTopology topology{};
    topology.geosets.reserve(geosetCount);
    for (std::size_t lod = 0; lod < geosetCount; ++lod)
    {
        std::istringstream modelInput(*modelText);
        Model3doLoadOptions loadOptions{};
        loadOptions.lod = static_cast<std::int32_t>(lod);
        const Model3do model = loadFrom3DO(modelInput, loadOptions);
        // A particular LOD may omit geometry carried by another geoset.
        // Keep an empty prefix here; the all-LOD maximum is validated after
        // every source-mesh mapping has been collected.
        std::vector<std::size_t> counts{};
        for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
        {
            const std::int32_t sourceIndex =
                meshIndex < model.meshSourceIndices.size()
                    ? model.meshSourceIndices[meshIndex]
                    : static_cast<std::int32_t>(meshIndex);
            if (sourceIndex < 0 || sourceIndex > 1000000)
            {
                throw std::runtime_error(
                    "Cannot export JKL/NDY ARCHLIGHTING: 3DO '" + modelName
                    + "' has an invalid source mesh index.");
            }
            const std::size_t source = static_cast<std::size_t>(sourceIndex);
            if (counts.size() <= source)
            {
                counts.resize(source + 1u, 0u);
            }
            counts[source] = std::max(counts[source], model.meshes[meshIndex].vertices.size());
        }
        topology.geosets.push_back(std::move(counts));
    }
    return topology;
}

MotsArchLightObject makeDefaultMotsArchLightingObject(
    const MotsArchLightingTopology& topology)
{
    std::size_t meshCount = 0;
    for (const std::vector<std::size_t>& geoset : topology.geosets)
    {
        meshCount = std::max(meshCount, geoset.size());
    }
    if (meshCount == 0 || meshCount > kMaxMotsArchLightingRecords)
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY ARCHLIGHTING: 3DO topology is empty or too large.");
    }

    MotsArchLightObject object{};
    object.meshes.reserve(meshCount);
    for (std::size_t meshIndex = 0; meshIndex < meshCount; ++meshIndex)
    {
        std::size_t vertexCount = 0;
        for (const std::vector<std::size_t>& geoset : topology.geosets)
        {
            if (meshIndex < geoset.size())
            {
                vertexCount = std::max(vertexCount, geoset[meshIndex]);
            }
        }
        if (vertexCount > kMaxMotsArchLightingRecords)
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY ARCHLIGHTING: 3DO vertex topology is too large.");
        }
        MotsArchLightMesh mesh{};
        mesh.wireIndex = static_cast<std::int32_t>(meshIndex);
        mesh.vertices.assign(vertexCount, MotsArchLightVertex{});
        object.meshes.push_back(std::move(mesh));
    }
    return object;
}

bool isDefaultMotsArchiveObject(const MotsArchLightObject& object) noexcept
{
    for (const MotsArchLightMesh& mesh : object.meshes)
    {
        for (const MotsArchLightVertex& vertex : mesh.vertices)
        {
            if (vertex.mono != 0.0f || vertex.red != 0.0f
                || vertex.green != 0.0f || vertex.blue != 0.0f)
            {
                return false;
            }
        }
    }
    return true;
}

MotsArchLightingExportSnapshot makeMotsArchLightingExportSnapshot(
    const Level& level,
    const JklSaveOptions& options)
{
    MotsArchLightingExportSnapshot snapshot{};
    snapshot.thingIndices.assign(level.things.size(), -1);
    if (level.kind != ProjectType::MOTS)
    {
        return snapshot;
    }

    std::vector<bool> activeReferences(level.motsArchLighting.size(), false);
    for (std::size_t thingIndex = 0; thingIndex < level.things.size(); ++thingIndex)
    {
        const Thing& thing = level.things[thingIndex];
        if (!sed::level::isMotsArchThing(
                level, thing, options.templatesDb))
        {
            continue;
        }
        if (thing.archLightingIndex < -1
            || (thing.archLightingIndex >= 0
                && static_cast<std::size_t>(thing.archLightingIndex)
                    >= level.motsArchLighting.size()))
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY ARCHLIGHTING: Thing architectural-light index is invalid.");
        }
        if (thing.archLightingIndex >= 0)
        {
            activeReferences[static_cast<std::size_t>(thing.archLightingIndex)] = true;
        }
    }

    std::vector<std::int32_t> remap(level.motsArchLighting.size(), -1);
    for (std::size_t sourceIndex = 0;
         sourceIndex < level.motsArchLighting.size(); ++sourceIndex)
    {
        if (!activeReferences[sourceIndex])
        {
            continue;
        }
        const MotsArchLightObject& object = level.motsArchLighting[sourceIndex];
        if (options.omitDefaultMotsArchLighting
            && isDefaultMotsArchiveObject(object))
        {
            continue;
        }
        remap[sourceIndex] = static_cast<std::int32_t>(snapshot.objects.size());
        snapshot.objects.push_back(object);
    }

    std::unordered_map<std::string, std::int32_t> synthesizedByModel{};
    for (std::size_t thingIndex = 0; thingIndex < level.things.size(); ++thingIndex)
    {
        const Thing& thing = level.things[thingIndex];
        if (!sed::level::isMotsArchThing(
                level, thing, options.templatesDb))
        {
            continue;
        }
        if (thing.archLightingIndex >= 0)
        {
            snapshot.thingIndices[thingIndex] = remap[
                static_cast<std::size_t>(thing.archLightingIndex)];
            continue;
        }
        if (options.omitDefaultMotsArchLighting)
        {
            continue;
        }

        const TplValue* modelValue = sed::level::thingFindValue(
            level, options.templatesDb, thing, "model3d");
        if (!modelValue)
        {
            modelValue = sed::level::thingFindValue(
                level, options.templatesDb, thing, "model3do");
        }
        const std::string modelName = modelValue
            ? std::string(trimView(modelValue->asString())) : std::string{};
        const std::string modelKey = upperAscii(modelName);
        const auto existing = synthesizedByModel.find(modelKey);
        if (existing != synthesizedByModel.end())
        {
            snapshot.thingIndices[thingIndex] = existing->second;
            continue;
        }

        const MotsArchLightingTopology topology = resolveMotsArchLightingTopology(
            level, thing, thingIndex, options);
        MotsArchLightObject object = makeDefaultMotsArchLightingObject(topology);
        if (snapshot.objects.size() >= kMaxMotsArchLightingRecords
            || snapshot.objects.size()
                >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY ARCHLIGHTING: object count is too large.");
        }
        std::unordered_set<std::int32_t> usedWireIndices{};
        usedWireIndices.reserve(snapshot.objects.size());
        for (const MotsArchLightObject& present : snapshot.objects)
        {
            usedWireIndices.insert(present.wireIndex);
        }
        std::int64_t wireIndex = 0;
        while (wireIndex <= std::numeric_limits<std::int32_t>::max()
            && usedWireIndices.contains(static_cast<std::int32_t>(wireIndex)))
        {
            ++wireIndex;
        }
        if (wireIndex > std::numeric_limits<std::int32_t>::max())
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY ARCHLIGHTING: object wire index is exhausted.");
        }
        object.wireIndex = static_cast<std::int32_t>(wireIndex);
        const std::int32_t newIndex = static_cast<std::int32_t>(snapshot.objects.size());
        snapshot.objects.push_back(std::move(object));
        synthesizedByModel.emplace(modelKey, newIndex);
        snapshot.thingIndices[thingIndex] = newIndex;
    }
    return snapshot;
}

void validateMotsArchLighting(
    const Level& level,
    const bool isMots,
    const MotsArchLightingExportSnapshot& snapshot)
{
    if (!isMots)
    {
        if (!level.motsArchLighting.empty()
            || std::any_of(
                level.things.begin(), level.things.end(),
                [](const Thing& thing)
                {
                    return thing.archLightingIndex != -1;
                }))
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY: ARCHLIGHTING data requires a MotS level.");
        }
        return;
    }

    if (snapshot.objects.size() > kMaxMotsArchLightingRecords
        || snapshot.objects.size()
            > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY ARCHLIGHTING: object count is too large.");
    }

    std::unordered_set<std::int32_t> objectIndices{};
    objectIndices.reserve(snapshot.objects.size());
    std::size_t encodedMeshRecords = 0;
    std::size_t encodedVertexRecords = 0;
    for (const MotsArchLightObject& object : snapshot.objects)
    {
        if (object.wireIndex < 0 || !objectIndices.insert(object.wireIndex).second)
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY ARCHLIGHTING: object wire index is invalid or duplicated.");
        }
        if (object.meshes.size() > kMaxMotsArchLightingRecords
            || object.meshes.size()
                > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY ARCHLIGHTING: mesh count is too large.");
        }
        if (encodedMeshRecords > kMaxMotsArchLightingRecords - object.meshes.size())
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY ARCHLIGHTING: total mesh record count is too large.");
        }
        encodedMeshRecords += object.meshes.size();

        std::unordered_set<std::int32_t> meshIndices{};
        meshIndices.reserve(object.meshes.size());
        for (const MotsArchLightMesh& mesh : object.meshes)
        {
            if (mesh.wireIndex < 0 || !meshIndices.insert(mesh.wireIndex).second)
            {
                throw std::runtime_error(
                    "Cannot export JKL/NDY ARCHLIGHTING: mesh wire index is invalid or duplicated.");
            }
            if (mesh.vertices.size() > kMaxMotsArchLightingRecords
                || mesh.vertices.size()
                    > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            {
                throw std::runtime_error(
                    "Cannot export JKL/NDY ARCHLIGHTING: vertex count is too large.");
            }
            if (encodedVertexRecords
                > kMaxMotsArchLightingRecords - mesh.vertices.size())
            {
                throw std::runtime_error(
                    "Cannot export JKL/NDY ARCHLIGHTING: total vertex record count is too large.");
            }
            encodedVertexRecords += mesh.vertices.size();
            for (const MotsArchLightVertex& vertex : mesh.vertices)
            {
                if (!std::isfinite(vertex.mono) || !std::isfinite(vertex.red)
                    || !std::isfinite(vertex.green) || !std::isfinite(vertex.blue))
                {
                    throw std::runtime_error(
                        "Cannot export JKL/NDY ARCHLIGHTING: a vertex channel is not finite.");
                }
            }
        }
    }

    if (snapshot.thingIndices.size() != level.things.size())
    {
        throw std::logic_error(
            "Cannot export JKL/NDY ARCHLIGHTING: snapshot Thing count is invalid.");
    }
    for (const std::int32_t archiveIndex : snapshot.thingIndices)
    {
        if (archiveIndex < -1
            || (archiveIndex >= 0
                && static_cast<std::size_t>(archiveIndex) >= snapshot.objects.size()))
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY ARCHLIGHTING: Thing architectural-light index is invalid.");
        }
    }
}

void validateActiveMotsArchLightingTopologies(
    const Level& level,
    const JklSaveOptions& options,
    const MotsArchLightingExportSnapshot& snapshot)
{
    // Resource-less codec callers retain the historical ability to export an
    // authored table without loading every 3DO. The editor supplies an
    // immutable resource/template snapshot; when present, validate every
    // active object against all render geosets before producing JKL text.
    if (level.kind != ProjectType::MOTS || options.resources == nullptr)
    {
        return;
    }

    struct ModelTopologyCacheEntry
    {
        MotsArchLightingTopology topology{};
        std::unordered_set<std::size_t> validatedObjectIndices{};
    };
    std::unordered_map<std::string, ModelTopologyCacheEntry> topologyByModel{};

    for (std::size_t thingIndex = 0; thingIndex < level.things.size(); ++thingIndex)
    {
        const Thing& thing = level.things[thingIndex];
        if (!sed::level::isMotsArchThing(
                level, thing, options.templatesDb)
            || thing.archLightingIndex < 0
            || thingIndex >= snapshot.thingIndices.size())
        {
            continue;
        }
        const std::int32_t archiveIndex = snapshot.thingIndices[thingIndex];
        if (archiveIndex < 0)
        {
            continue;
        }
        const std::size_t objectIndex = static_cast<std::size_t>(archiveIndex);
        if (objectIndex >= snapshot.objects.size())
        {
            throw std::logic_error(
                "Cannot export JKL/NDY ARCHLIGHTING: snapshot Thing index is invalid.");
        }

        const TplValue* modelValue = sed::level::thingFindValue(
            level, options.templatesDb, thing, "model3d");
        if (!modelValue)
        {
            modelValue = sed::level::thingFindValue(
                level, options.templatesDb, thing, "model3do");
        }
        const std::string modelName = modelValue
            ? std::string(trimView(modelValue->asString())) : std::string{};
        const std::string modelKey = upperAscii(modelName);
        auto topology = topologyByModel.find(modelKey);
        if (topology == topologyByModel.end())
        {
            ModelTopologyCacheEntry entry{};
            entry.topology = resolveMotsArchLightingTopology(
                level, thing, thingIndex, options, false);
            topology = topologyByModel.emplace(modelKey, std::move(entry)).first;
        }
        if (topology->second.validatedObjectIndices.contains(objectIndex))
        {
            continue;
        }
        if (!sed::level::matchesMotsArchLightingTopology(
                snapshot.objects[objectIndex], topology->second.topology))
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY ARCHLIGHTING: "
                + thingExportLabel(thing, thingIndex)
                + " ARCH object " + std::to_string(archiveIndex)
                + " does not match 3DO '" + modelName + "' topology.");
        }
        topology->second.validatedObjectIndices.insert(objectIndex);
    }
}

bool looksLikeSurfaceNormalLine(const std::string& line)
{
    const auto tokens = splitWords(line);
    return tokens.size() == 4
        && parseLineIndexToken(tokens[0]).has_value()
        && parseDouble(tokens[1]).has_value()
        && parseDouble(tokens[2]).has_value()
        && parseDouble(tokens[3]).has_value();
}

std::size_t countExplicitCogArguments(const std::vector<TplValue>& values)
{
    return static_cast<std::size_t>(std::count_if(
        values.begin(), values.end(), [](const TplValue& value)
        {
            // sithThing_ParseArg allocates a COG instance for every explicit
            // cog= token. Duplicate script names are separate instances, while
            // empty/none references do not load anything.
            const std::string_view script = trimView(value.stringValue);
            return iequalsAscii(value.name, "cog")
                && !script.empty()
                && !iequalsAscii(script, "none");
        }));
}

bool isRawJklCogValue(const CogValue& value, const std::size_t parameterIndex)
{
    return value.type == ValueType::str
        && value.cogType == CogType::unk
        && value.name == "param" + std::to_string(parameterIndex);
}

std::string pendingJklImportCogName(const Level& level)
{
    for (const Cog& cog : level.cogs)
    {
        for (std::size_t index = 0; index < cog.values.size(); ++index)
        {
            const CogValue& value = cog.values[index];
            if (isRawJklCogValue(value, index))
            {
                return cog.name.empty() ? std::string("<unnamed>") : cog.name;
            }
        }
        if (level.importedJklCogRowsPresent && cog.values.empty())
        {
            return cog.name.empty() ? std::string("<unnamed>") : cog.name;
        }
    }
    return "<unnamed>";
}

void finalizeJklImportCogValues(
    Level& level,
    const ResourceLocator& resources)
{
    std::unordered_map<std::string, CogSymbolLoadResult> symbolCache{};
    struct PendingCogValues
    {
        std::size_t cogIndex{};
        std::vector<CogValue> values{};
    };
    std::vector<PendingCogValues> updates{};
    std::vector<std::vector<bool>> hydrated(level.cogs.size());
    std::string unresolvedCogName{"<unnamed>"};
    bool hasUnresolvedCogDefaults = false;
    for (std::size_t cogIndex = 0; cogIndex < level.cogs.size(); ++cogIndex)
    {
        hydrated[cogIndex].assign(level.cogs[cogIndex].values.size(), false);
    }

    const auto mapJklSurfaceArgument = [&level](
        CogValue& value,
        const std::string_view rawToken)
    {
        if (value.type != ValueType::ptr || value.cogType != CogType::surface)
        {
            return;
        }

        const std::optional<std::int32_t> wireIndex =
            parseInt<std::int32_t>(trimView(rawToken));
        std::int32_t parsedWireIndex = wireIndex.value_or(value.intValue);
        if (parsedWireIndex < 0)
        {
            return;
        }

        if (level.importedJklSurfaceRefs.empty())
        {
            return;
        }
        if (static_cast<std::size_t>(parsedWireIndex)
            >= level.importedJklSurfaceRefs.size())
        {
            value.intValue = -1;
            value.stringValue.clear();
            return;
        }

        const SurfaceRef imported = level.importedJklSurfaceRefs[
            static_cast<std::size_t>(parsedWireIndex)];
        const std::int32_t editorIndex = level.getGlobalSurfaceNumber(
            imported.sector, imported.surface);
        if (editorIndex < 0)
        {
            value.intValue = -1;
            value.stringValue.clear();
            return;
        }

        value.intValue = editorIndex;
        value.stringValue.clear();
    };

    for (std::size_t cogIndex = 0; cogIndex < level.cogs.size(); ++cogIndex)
    {
        const Cog& cog = level.cogs[cogIndex];
        bool hasRawValues = false;
        for (std::size_t index = 0; index < cog.values.size(); ++index)
        {
            if (isRawJklCogValue(cog.values[index], index))
            {
                hasRawValues = true;
                break;
            }
        }
        if (!hasRawValues && !level.importedJklCogRowsPresent)
        {
            continue;
        }

        const std::string key = upperAscii(cog.name);
        auto symbols = symbolCache.find(key);
        if (symbols == symbolCache.end())
        {
            symbols = symbolCache.emplace(
                key, loadCogSymbolsNoLocalsResult(resources, cog.name)).first;
        }
        if (!symbols->second.loaded())
        {
            // Keep the generic values and their exact wire spellings when
            // the script is unavailable or malformed.  A successfully loaded
            // empty public schema is also left generic, but its status remains
            // distinguishable for callers and diagnostics.
            if (unresolvedCogName == "<unnamed>")
            {
                unresolvedCogName = cog.name.empty()
                    ? std::string("<unnamed>")
                    : cog.name;
            }
            hasUnresolvedCogDefaults = true;
            continue;
        }

        std::vector<CogValue> candidateValues = cog.values;
        bool candidateChanged = false;
        for (std::size_t index = 0;
             index < symbols->second.symbols.size();
             ++index)
        {
            if (index >= candidateValues.size())
            {
                // The parser intentionally stores only tokens present on the
                // wire.  A missing trailing token therefore receives the
                // already-normalized script declaration/default.
                CogValue defaultValue = symbols->second.symbols[index];
                if (defaultValue.intValue >= 0)
                {
                    mapJklSurfaceArgument(
                        defaultValue, std::to_string(defaultValue.intValue));
                }
                candidateValues.push_back(std::move(defaultValue));
                candidateChanged = true;
                continue;
            }

            const CogValue& rawValue = candidateValues[index];
            if (!isRawJklCogValue(rawValue, index))
            {
                continue;
            }

            const std::string rawToken = rawValue.stringValue;
            CogValue resolved = symbols->second.symbols[index];
            if (!resolved.parseJed(level, rawToken)
                && !resolved.parse(rawToken))
            {
                // Match the ordinary resource-backed load fallback while
                // retaining the schema identity for later authoring/export.
                resolved.type = ValueType::str;
                resolved.stringValue = rawToken;
            }
            mapJklSurfaceArgument(resolved, rawToken);
            hydrated[cogIndex][index] = true;
            candidateValues[index] = std::move(resolved);
            candidateChanged = true;
        }
        if (candidateChanged)
        {
            updates.push_back(PendingCogValues{
                cogIndex, std::move(candidateValues) });
        }
    }

    // Keep the map only while a resource-independent load still has raw COG
    // arguments.  Once every schema/value has been hydrated, the regular
    // typed editor indices are authoritative and the import-only metadata can
    // safely disappear before later geometry edits or native saves.
    bool hasUnresolvedRawValues = false;
    for (std::size_t cogIndex = 0; cogIndex < level.cogs.size(); ++cogIndex)
    {
        const Cog& cog = level.cogs[cogIndex];
        for (std::size_t index = 0; index < cog.values.size(); ++index)
        {
            if (isRawJklCogValue(cog.values[index], index)
                && !hydrated[cogIndex][index])
            {
                if (unresolvedCogName == "<unnamed>")
                {
                    unresolvedCogName = cog.name.empty()
                        ? std::string("<unnamed>")
                        : cog.name;
                }
                hasUnresolvedRawValues = true;
                break;
            }
        }
        if (hasUnresolvedRawValues)
        {
            break;
        }
    }

    if ((hasUnresolvedRawValues || hasUnresolvedCogDefaults)
        && level.hasAmbiguousPendingJklCogSurfaceReferences())
    {
        throw std::runtime_error(
            "Cannot finalize JKL/NDY: imported COG surface references require "
            "resource-backed COG script '" + unresolvedCogName
            + "' before publishing the level.");
    }

    for (PendingCogValues& update : updates)
    {
        level.cogs[update.cogIndex].values = std::move(update.values);
    }
    if (!hasUnresolvedRawValues && !hasUnresolvedCogDefaults)
    {
        level.importedJklSurfaceRefs.clear();
        level.importedJklCogRowsPresent = false;
    }
}

} // namespace

void finalizeJklImportUv(
    Level& level,
    const ResourceLocator& resources,
    DiagnosticSink onDiagnostic)
{
    if (level.uvCoordinateSpace != UvCoordinateSpace::TexturePixels)
    {
        return;
    }

    std::vector<PendingJklNoMaterialUv> pendingNoMaterial{};
    const JklNoMaterialUvPreparationResult preparedNoMaterial =
        prepareJklNoMaterialUvDefaults(
            level,
            level::levelPixelPerUnit(level),
            pendingNoMaterial);
    if (!preparedNoMaterial)
    {
        const SurfaceRef failedSurface = *preparedNoMaterial.failedSurface;
        const std::string indexDescription = preparedNoMaterial.invalidMaterialIndex
            ? "raw material index "
                + std::to_string(*preparedNoMaterial.invalidMaterialIndex)
                + " is invalid"
            : "material-less surface data is invalid";
        diagnosticEmit(onDiagnostic, DiagnosticSeverity::warning,
            "JKL texture coordinates remain in legacy pixel space because "
                + indexDescription + " at sector "
                + std::to_string(failedSurface.sector)
                + ", surface "
                + std::to_string(failedSurface.surface)
                + ": " + preparedNoMaterial.failureReason + ".");
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
                        resources, surface.material);
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
        std::string materialReference{};
        if (converted.failedSurface
            && converted.failedSurface->sector >= 0
            && static_cast<std::size_t>(converted.failedSurface->sector) < level.sectors.size())
        {
            const Sector& failedSector = level.sectors[
                static_cast<std::size_t>(converted.failedSurface->sector)];
            if (converted.failedSurface->surface >= 0
                && static_cast<std::size_t>(converted.failedSurface->surface) < failedSector.surfaces.size())
            {
                const Surface& failedSurface = failedSector.surfaces[
                    static_cast<std::size_t>(converted.failedSurface->surface)];
                if (!trimView(failedSurface.material).empty())
                {
                    materialReference = " for material '" + failedSurface.material + "'";
                }
            }
        }
        diagnosticEmit(onDiagnostic,
            DiagnosticSeverity::warning,
            "JKL texture coordinates remain in legacy pixel space because "
            "the referenced MAT dimensions could not be resolved"
                + materialReference
                + location + ".");
        return;
    }

    for (PendingJklNoMaterialUv& pendingSurface : pendingNoMaterial)
    {
        Surface& destination = level.sectors[
            static_cast<std::size_t>(pendingSurface.reference.sector)]
            .surfaces[static_cast<std::size_t>(pendingSurface.reference.surface)];
        destination = std::move(pendingSurface.surface);
    }
}

void finalizeJklImportResources(
    Level& level,
    const ResourceLocator& resources,
    DiagnosticSink onDiagnostic)
{
    // COG schemas are resolved only after the complete Level has been decoded:
    // JKL places COG rows before THINGS, while parseJed needs the final entity
    // numbering to preserve pointer identity.  UV conversion remains a
    // separate failure-atomic operation below.
    finalizeJklImportCogValues(level, resources);
    if (level.hasAmbiguousPendingJklCogSurfaceReferences())
    {
        throw std::runtime_error(
            "Cannot finalize JKL/NDY: imported COG surface references require "
            "resource-backed COG script '" + pendingJklImportCogName(level)
            + "' before publishing the level.");
    }
    finalizeJklImportUv(level, resources, std::move(onDiagnostic));
}

Level loadFromJKL(std::istream& in, const JklLoadOptions& options)
{
    std::unordered_map<std::string, std::vector<std::string>> sections;
    std::string currentSection;
    std::string line;
    constexpr std::string_view kInputReadFailure =
        "Cannot load JKL/NDY: input stream failed while reading level text.";
    const auto readNextLine = [&]()
    {
        try
        {
            return static_cast<bool>(std::getline(in, line));
        }
        catch (...)
        {
            if ( in.bad() )
            {
                throw std::runtime_error(std::string(kInputReadFailure));
            }
            throw;
        }
    };

    while ( readNextLine() )
    {
        stripHashComment(line);
        std::string trimmed = trim(line);
        if (trimmed.empty())
        {
            continue;
        }

        if (iequalsAscii(trimmed, "END"))
        {
            currentSection.clear();
            continue;
        }

        if (istartsWithAscii(trimmed, "SECTION:"))
        {
            const auto words = splitWords(trimmed);
            if (words.size() >= 2)
            {
                currentSection = upperAscii(words[1]);
                // Keep an empty section observable.  MotS VERSION 1 files
                // use an empty ARCHLIGHTING section as their dialect marker,
                // so relying on a first payload line loses that information.
                sections.try_emplace(currentSection);
            }
            else
            {
                currentSection.clear();
            }
            continue;
        }

        if (!currentSection.empty())
        {
            sections[currentSection].push_back(std::move(trimmed));
        }
    }
    if ( in.bad() )
    {
        throw std::runtime_error(std::string(kInputReadFailure));
    }

    ProjectType projectType = options.projectType;
    if (const auto it = sections.find("HEADER"); it != sections.end())
    {
        for (const std::string& raw : it->second)
        {
            const auto tokens = splitWords(raw);
            if (tokens.size() >= 2 && iequalsAscii(tokens[0], "VERSION"))
            {
                const auto version = parseInt<std::int32_t>(tokens[1]);
                if (!version)
                {
                    break;
                }

                if (*version == 2 || *version == 3)
                {
                    projectType = ProjectType::IJIM;
                }
                // JKDF2 and MotS both use VERSION 1.  Preserve the caller's
                // explicit MotS context; the text header cannot distinguish
                // those two formats on its own.
                else if (*version == 1
                    && options.projectType != ProjectType::MOTS)
                {
                    projectType = ProjectType::JKDF2;
                }
                break;
            }
        }
    }

    // VERSION 1 is shared by JKDF2 and MotS.  MotS is the only engine
    // dialect that carries ARCHLIGHTING, including an explicitly empty
    // section, so use that marker when the caller did not force IJIM.
    if (sections.contains("ARCHLIGHTING") && projectType != ProjectType::IJIM)
    {
        projectType = ProjectType::MOTS;
    }

    Level level{};
    level.kind = projectType;
    level.clear();
    level.uvCoordinateSpace = projectType == ProjectType::IJIM
        ? UvCoordinateSpace::Normalized
        : UvCoordinateSpace::TexturePixels;
    setDefaultHeader(level, projectType);

    std::vector<std::string> materials;
    std::vector<std::string> colormaps;
    std::vector<VertexTemp> gVertices;
    std::vector<TexVertexTemp> gTexVertices;
    std::vector<AdjoinTemp> gAdjoins;
    std::vector<SurfaceTemp> gSurfaces;

    if (const auto it = sections.find("HEADER"); it != sections.end())
    {
        for (const std::string& raw : it->second)
        {
            const auto tokens = splitWords(raw);
            if (tokens.empty())
            {
                continue;
            }

            const std::string k1 = upperAscii(tokens[0]);
            const std::string k2 = (tokens.size() >= 2) ? upperAscii(tokens[1]) : std::string{};

            if (k1 == "VERSION" && tokens.size() >= 2)
            {
                const auto version = parseInt<std::int32_t>(tokens[1]);
                if (!version)
                {
                    continue;
                }
                level.header.version = *version;
                if (*version == 2 || *version == 3)
                {
                    level.kind = ProjectType::IJIM;
                }
                else if (*version != 1)
                {
                    throw std::runtime_error("Unknown level version.");
                }
            }
            else if (k1 == "WORLD" && k2 == "GRAVITY" && tokens.size() >= 3)
            {
                if (const auto g = parseDouble(tokens[2]))
                {
                    level.header.gravity = *g;
                }
            }
            else if (k1 == "MIPMAP" && k2 == "DISTANCES" && tokens.size() >= 6)
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (const auto d = parseDouble(tokens[2 + i]))
                    {
                        level.header.mipmapDistances[static_cast<std::size_t>(i)] = *d;
                    }
                }
            }
            else if (k1 == "LOD" && k2 == "DISTANCES" && tokens.size() >= 6)
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (const auto d = parseDouble(tokens[2 + i]))
                    {
                        level.header.lodDistances[static_cast<std::size_t>(i)] = *d;
                    }
                }
            }
            else if (k1 == "PERSPECTIVE" && k2 == "DISTANCE" && tokens.size() >= 3)
            {
                if (const auto d = parseDouble(tokens[2]))
                {
                    level.header.perspectiveDistance = *d;
                }
            }
            else if (k1 == "GOURAUD" && k2 == "DISTANCE" && tokens.size() >= 3)
            {
                if (const auto d = parseDouble(tokens[2]))
                {
                    level.header.gouraudDistance = *d;
                }
            }
            else if (k1 == "FOG" && tokens.size() >= 8)
            {
                level.header.fog.enabled = iequalsAscii(tokens[1], "TRUE") || tokens[1] == "1";
                level.header.fog.color.r =
                    parseJklFloat(tokens[2], kJklFogColorFloatDiagnostic);
                level.header.fog.color.g =
                    parseJklFloat(tokens[3], kJklFogColorFloatDiagnostic);
                level.header.fog.color.b =
                    parseJklFloat(tokens[4], kJklFogColorFloatDiagnostic);
                level.header.fog.color.a =
                    parseJklFloat(tokens[5], kJklFogColorFloatDiagnostic);
                if (const auto s = parseDouble(tokens[6]))
                {
                    level.header.fog.fogStart = *s;
                }
                if (const auto e = parseDouble(tokens[7]))
                {
                    level.header.fog.fogEnd = *e;
                }
            }
            else if (k1 == "CEILING" && k2 == "SKY" && tokens.size() >= 4)
            {
                const std::string k3 = upperAscii(tokens[2]);
                if (k3 == "OFFSET" && tokens.size() >= 5)
                {
                    if (const auto x = parseDouble(tokens[3]))
                    {
                        level.header.ceilingSky.offset.x = *x;
                    }
                    if (const auto y = parseDouble(tokens[4]))
                    {
                        level.header.ceilingSky.offset.y = *y;
                    }
                }
                else if (k3 == "Z")
                {
                    if (const auto z = parseDouble(tokens[3]))
                    {
                        level.header.ceilingSky.height = *z;
                    }
                }
            }
            else if (k1 == "HORIZON" && tokens.size() >= 3)
            {
                if (k2 == "DISTANCE")
                {
                    if (const auto d = parseDouble(tokens[2]))
                    {
                        level.header.horizonSky.distance = *d;
                    }
                }
                else if (k2 == "PIXELS")
                {
                    // Typical syntax: HORIZON PIXELS PER REV <value>
                    if (tokens.size() >= 5 && iequalsAscii(tokens[2], "PER") && iequalsAscii(tokens[3], "REV"))
                    {
                        if (const auto ppr = parseDouble(tokens[4]))
                        {
                            level.header.horizonSky.pixelsPerRev = *ppr;
                        }
                    }
                    // Loose support for levels with an extra token:
                    // HORIZON PIXELS <token> PER REV <value>
                    else if (tokens.size() >= 6 && iequalsAscii(tokens[3], "PER") && iequalsAscii(tokens[4], "REV"))
                    {
                        if (const auto ppr = parseDouble(tokens[5]))
                        {
                            level.header.horizonSky.pixelsPerRev = *ppr;
                        }
                    }
                }
                else if (k2 == "SKY" && tokens.size() >= 5 && iequalsAscii(tokens[2], "OFFSET"))
                {
                    if (const auto x = parseDouble(tokens[3]))
                    {
                        level.header.horizonSky.offset.x = *x;
                    }
                    if (const auto y = parseDouble(tokens[4]))
                    {
                        level.header.horizonSky.offset.y = *y;
                    }
                }
            }
        }
    }

    if (const auto it = sections.find("MATERIALS"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        std::optional<std::int32_t> count;
        if (!lines.empty())
        {
            const auto headerTokens = splitWords(lines[0]);
            if (headerTokens.size() >= 3)
            {
                count = parseInt<std::int32_t>(headerTokens[2]);
            }
            idx = 1;
        }

        std::int32_t remaining = count.value_or(std::numeric_limits<std::int32_t>::max());
        while (idx < lines.size() && remaining-- > 0)
        {
            const std::string& raw = lines[idx++];
            if (iequalsAscii(raw, "END"))
            {
                break;
            }

            std::string name = raw;
            if (const std::size_t lastColon = name.find_last_of(':'); lastColon != std::string::npos)
            {
                name = name.substr(lastColon + 1);
            }
            name = trim(name);

            std::string lower = name;
            for (char& ch : lower)
            {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            if (const std::size_t matPos = lower.find(".mat"); matPos != std::string::npos)
            {
                name = trim(std::string_view{name}.substr(0, matPos + 4));
            }

            materials.push_back(std::move(name));
        }
    }

    if (const auto it = sections.find("GEORESOURCE"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        auto readCountLine = [&]() -> std::int32_t
        {
            if (idx >= lines.size())
            {
                throw std::runtime_error("Unexpected end of GEORESOURCE.");
            }
            const auto tokens = splitWords(lines[idx++]);
            if (tokens.empty())
            {
                throw std::runtime_error(
                    "Cannot load JKL/NDY GEORESOURCE count: count is invalid.");
            }
            const std::optional<std::int32_t> parsedCount =
                parseInt<std::int32_t>(tokens.back());
            if (!parsedCount || *parsedCount < 0)
            {
                throw std::runtime_error(
                    "Cannot load JKL/NDY GEORESOURCE count: count is invalid.");
            }
            if (static_cast<std::size_t>(*parsedCount) > lines.size() - idx)
            {
                throw std::runtime_error(
                    "Cannot load JKL/NDY GEORESOURCE count: records are truncated.");
            }
            return *parsedCount;
        };

        if (level.kind != ProjectType::IJIM)
        {
            const std::int32_t n = readCountLine(); // WORLD COLORMAPS n
            for (std::int32_t i = 0; i < n && idx < lines.size(); ++i)
            {
                const auto tokens = splitWords(lines[idx++]);
                if (tokens.size() >= 2)
                {
                    colormaps.push_back(std::string(tokens[1]));
                }
            }
        }

        {
            const std::int32_t n = readCountLine(); // WORLD VERTICES n
            gVertices.reserve(static_cast<std::size_t>(n));
            for (std::int32_t i = 0; i < n && idx < lines.size(); ++i)
            {
                const auto tokens = splitWords(lines[idx++]);
                if (tokens.size() < 4)
                {
                    continue;
                }
                VertexTemp v{};
                v.x = parseDouble(tokens[1]).value_or(0.0);
                v.y = parseDouble(tokens[2]).value_or(0.0);
                v.z = parseDouble(tokens[3]).value_or(0.0);
                gVertices.push_back(v);
            }
        }

        {
            const std::int32_t n = readCountLine(); // WORLD TEXTURE VERTICES n
            gTexVertices.reserve(static_cast<std::size_t>(n));
            for (std::int32_t i = 0; i < n && idx < lines.size(); ++i)
            {
                const auto tokens = splitWords(lines[idx++]);
                if (tokens.size() < 3)
                {
                    continue;
                }
                TexVertexTemp v{};
                v.u = parseJklUvFloatOr(tokens[1], 0.0f);
                v.v = parseJklUvFloatOr(tokens[2], 0.0f);
                gTexVertices.push_back(v);
            }
        }

        {
            const std::int32_t n = readCountLine(); // WORLD ADJOINS n
            gAdjoins.reserve(static_cast<std::size_t>(n));
            for (std::int32_t i = 0; i < n && idx < lines.size(); ++i)
            {
                const auto tokens = splitWords(lines[idx++]);
                if (tokens.size() < 3)
                {
                    continue;
                }
                AdjoinTemp a{};
                a.flags = parseHex(tokens[1]).value_or(0);
                a.mirror = parseInt<std::int32_t>(tokens[2]).value_or(0);
                gAdjoins.push_back(a);
            }
        }

        {
            const std::int32_t n = readCountLine(); // WORLD SURFACES n
            gSurfaces.reserve(static_cast<std::size_t>(n));
            for (std::int32_t parsedSurfaceCount = 0; parsedSurfaceCount < n;)
            {
                if (idx >= lines.size())
                {
                    throw std::runtime_error(
                        "Cannot load JKL/NDY surface: surface records are truncated.");
                }
                const auto tokens = splitWords(lines[idx++]);
                const std::size_t fixedFieldCount = level.header.version == 1 ? 10u : 13u;
                if (tokens.size() < fixedFieldCount)
                {
                    throw std::runtime_error(
                        "Cannot load JKL/NDY surface: fixed fields are truncated for the declared version.");
                }

                SurfaceTemp s{};
                s.nMat = parseInt<std::int32_t>(tokens[1]).value_or(0);
                s.surfFlags = static_cast<std::int32_t>(parseHex(tokens[2]).value_or(0));
                s.faceFlags = parseHex(tokens[3]).value_or(0);
                s.geo = parseInt<std::int32_t>(tokens[4]).value_or(0);
                s.light = parseInt<std::int32_t>(tokens[5]).value_or(0);
                s.tex = parseInt<std::int32_t>(tokens[6]).value_or(0);
                s.nAdj = parseInt<std::int32_t>(tokens[7]).value_or(-1);

                if (s.nAdj >= 0 && static_cast<std::size_t>(s.nAdj) < gAdjoins.size())
                {
                    gAdjoins[static_cast<std::size_t>(s.nAdj)].surf = parsedSurfaceCount;
                }

                std::size_t t = 8;
                if (level.header.version == 1)
                {
                    const float intensity = parseJklExtraLightFloat(tokens[t++]);
                    s.extraLight = ColorF{ intensity, intensity, intensity, 1.0f };
                }
                else
                {
                    s.extraLight.r = parseJklExtraLightFloat(tokens[t++]);
                    s.extraLight.g = parseJklExtraLightFloat(tokens[t++]);
                    s.extraLight.b = parseJklExtraLightFloat(tokens[t++]);
                    const float alpha = parseJklExtraLightFloat(tokens[t++]);
                    s.extraLight.a = clamp01(alpha);
                }

                const std::optional<std::int32_t> parsedVertexCount =
                    parseInt<std::int32_t>(tokens[t++]);
                if (!parsedVertexCount || *parsedVertexCount < 0)
                {
                    throw std::runtime_error(
                        "Cannot load JKL/NDY surface: vertex count is invalid.");
                }
                const std::int32_t nvert = *parsedVertexCount;

                if (static_cast<std::size_t>(nvert) > tokens.size() - t)
                {
                    throw std::runtime_error(
                        "Cannot load JKL/NDY surface: vertex index pairs are truncated.");
                }

                s.vxs.reserve(static_cast<std::size_t>(nvert));
                s.tvxs.reserve(static_cast<std::size_t>(nvert));

                for (std::int32_t j = 0; j < nvert; ++j)
                {
                    if (t >= tokens.size())
                    {
                        throw std::runtime_error(
                            "Cannot load JKL/NDY surface: vertex index pairs are truncated.");
                    }

                    std::string_view pair = tokens[t++];
                    std::string_view left = pair;
                    std::string_view right;
                    if (const std::size_t comma = pair.find(','); comma != std::string_view::npos)
                    {
                        left = pair.substr(0, comma);
                        right = pair.substr(comma + 1);
                        if (right.empty() && t < tokens.size())
                        {
                            right = tokens[t++];
                        }
                    }
                    else if (t < tokens.size())
                    {
                        right = tokens[t++];
                    }

                    if (left.empty() || right.empty())
                    {
                        throw std::runtime_error(
                            "Cannot load JKL/NDY surface: vertex index pairs are truncated.");
                    }

                    const std::optional<std::int32_t> vertexIndex =
                        parseInt<std::int32_t>(trimLeft(left));
                    const std::optional<std::int32_t> textureVertexIndex =
                        parseInt<std::int32_t>(trimLeft(right));
                    if (!vertexIndex || !textureVertexIndex)
                    {
                        throw std::runtime_error(
                            "Cannot load JKL/NDY surface: vertex index pairs are truncated.");
                    }

                    s.vxs.push_back(*vertexIndex);
                    s.tvxs.push_back(*textureVertexIndex);
                }

                std::vector<float> il;
                while (t < tokens.size())
                {
                    il.push_back(parseJklFloat(
                        tokens[t++], kJklSurfaceColorFloatDiagnostic));
                }

                s.colors.resize(s.vxs.size(), ColorWhite);
                if (il.size() > s.vxs.size())
                {
                    // MOTS (a+r+g+b) or IJIM (r+g+b).
                    std::size_t viofs = 0;
                    if (level.header.version == 1)
                    {
                        level.kind = ProjectType::MOTS;
                    }

                    for (std::size_t v = 0; v < s.vxs.size(); ++v)
                    {
                        ColorF c{};
                        c.a = s.extraLight.a;

                        if (level.header.version == 1 && viofs < il.size())
                        {
                            c.a = il[viofs++];
                        }

                        if (viofs + 2 < il.size())
                        {
                            c.r = il[viofs++];
                            c.g = il[viofs++];
                            c.b = il[viofs++];
                        }
                        s.colors[v] = c;
                    }
                }
                else
                {
                    // JKDF2: intensity only.
                    for (std::size_t v = 0; v < s.vxs.size(); ++v)
                    {
                        const float a = (v < il.size()) ? il[v] : 0.0f;
                        s.colors[v] = ColorF{a, a, a, a};
                    }
                }

                gSurfaces.push_back(std::move(s));
                ++parsedSurfaceCount;

                // Legacy editors commonly write each surface normal immediately
                // after its surface record. The normal is redundant because SED
                // recalculates it from vertices, but it must be consumed here so
                // the next surface line is not skipped as malformed data.
                if (idx < lines.size() && looksLikeSurfaceNormalLine(lines[idx]))
                {
                    ++idx;
                }
            }

            // Some files group all normals after the surface records instead.
            for (std::int32_t i = 0; i < n && idx < lines.size() && looksLikeSurfaceNormalLine(lines[idx]); ++i)
            {
                ++idx;
            }
        }
    }

    auto getMat = [&](const std::int32_t idx) -> std::string
    {
        if (idx < 0)
        {
            return {};
        }

        if (idx >= StaticResIdxMask)
        {
            const std::int32_t si = idx & ~StaticResIdxMask;
            const std::string_view name = staticMaterialName(level.kind, si);
            if (!name.empty())
            {
                return std::string(name);
            }
            return {};
        }

        if (static_cast<std::size_t>(idx) >= materials.size())
        {
            return {};
        }

        return materials[static_cast<std::size_t>(idx)];
    };

    auto getCmp = [&](const std::int32_t idx) -> std::string
    {
        if (idx < 0 || static_cast<std::size_t>(idx) >= colormaps.size())
        {
            return {};
        }
        return colormaps[static_cast<std::size_t>(idx)];
    };

    std::vector<SurfaceRef> geoSurfaceToRef;
    geoSurfaceToRef.resize(gSurfaces.size());

    if (const auto it = sections.find("SECTORS"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        if (lines.empty())
        {
            throw std::runtime_error(std::string(kJklSectorCountDiagnostic));
        }
        const auto sectorCountTokens = splitWords(lines[idx++]);
        if (sectorCountTokens.size() < 3
            || !iequalsAscii(sectorCountTokens[0], "WORLD")
            || !iequalsAscii(sectorCountTokens[1], "SECTORS"))
        {
            throw std::runtime_error(std::string(kJklSectorCountDiagnostic));
        }
        const std::optional<std::int32_t> parsedSectorCount =
            parseInt<std::int32_t>(sectorCountTokens[2]);
        if (!parsedSectorCount || *parsedSectorCount < 0)
        {
            throw std::runtime_error(std::string(kJklSectorCountDiagnostic));
        }
        const std::int32_t numSecs = *parsedSectorCount;

        for (std::int32_t secIdx = 0; secIdx < numSecs; ++secIdx)
        {
            if (idx >= lines.size())
            {
                throw std::runtime_error(std::string(kJklSectorCountDiagnostic));
            }
            const auto sectorHeader = splitWords(lines[idx++]);
            if (sectorHeader.size() < 2
                || !iequalsAscii(sectorHeader[0], "SECTOR")
                || !parseInt<std::int32_t>(sectorHeader[1]))
            {
                throw std::runtime_error(std::string(kJklSectorCountDiagnostic));
            }

            Sector& sector = level.newSector();
            if (level.kind == ProjectType::IJIM)
            {
                // The engine zero-initializes IJIM tint when no TINT record is
                // present. Keep that absent state distinct from the editor's
                // ColorWhite default for newly-created sectors.
                sector.tint.setToZero();
            }
            std::vector<std::int32_t> scvxList;
            bool hasSurfacesRecord = false;

            while (idx < lines.size())
            {
                const std::string& raw = lines[idx++];
                const auto tokens = splitWords(raw);
                if (tokens.empty())
                {
                    continue;
                }
                const std::string k1 = upperAscii(tokens[0]);

                if (k1 == "SECTOR")
                {
                    throw std::runtime_error(
                        std::string(kJklSectorMissingSurfacesDiagnostic));
                }

                if (k1 == "FLAGS" && tokens.size() >= 2)
                {
                    if (const auto f = parseHex(tokens.back()))
                    {
                        sector.flags = static_cast<std::int32_t>(*f);
                    }
                }
                else if (k1 == "AMBIENT")
                {
                    if (level.header.version == 1)
                    {
                        const float intensity = parseJklSectorLightFloat(tokens.back());
                        sector.ambient = ColorF{ intensity, intensity, intensity, 0.0f };
                    }
                    else if (tokens.size() >= 5)
                    {
                        sector.ambient.r =
                            parseJklFloat(tokens[2], kJklSectorLightFloatDiagnostic);
                        sector.ambient.g =
                            parseJklFloat(tokens[3], kJklSectorLightFloatDiagnostic);
                        sector.ambient.b =
                            parseJklFloat(tokens[4], kJklSectorLightFloatDiagnostic);
                        sector.ambient.a = 0.0f;
                    }
                }
                else if (k1 == "EXTRA")
                {
                    if (level.header.version == 1)
                    {
                        const float intensity = parseJklSectorLightFloat(tokens.back());
                        sector.extraLight = ColorF{ intensity, intensity, intensity, 0.0f };
                    }
                    else if (tokens.size() >= 5)
                    {
                        sector.extraLight.r =
                            parseJklFloat(tokens[2], kJklSectorLightFloatDiagnostic);
                        sector.extraLight.g =
                            parseJklFloat(tokens[3], kJklSectorLightFloatDiagnostic);
                        sector.extraLight.b =
                            parseJklFloat(tokens[4], kJklSectorLightFloatDiagnostic);
                        sector.extraLight.a = 0.0f;
                    }
                }
                else if (k1 == "AVERAGE" && tokens.size() >= 5)
                {
                    const std::string k2 = upperAscii(tokens[1]);
                    const std::string k3 = upperAscii(tokens[2]);
                    if (k2 == "LIGHT" && k3 == "INTENSITY" && tokens.size() >= 6)
                    {
                        sector.pointLight.color.r =
                            parseJklFloat(tokens[3], kJklSectorLightFloatDiagnostic);
                        sector.pointLight.color.g =
                            parseJklFloat(tokens[4], kJklSectorLightFloatDiagnostic);
                        sector.pointLight.color.b =
                            parseJklFloat(tokens[5], kJklSectorLightFloatDiagnostic);
                    }
                    else if (k2 == "LIGHT" && k3 == "POSITION" && tokens.size() >= 6)
                    {
                        sector.pointLight.position.x = parseDouble(tokens[3]).value_or(0.0);
                        sector.pointLight.position.y = parseDouble(tokens[4]).value_or(0.0);
                        sector.pointLight.position.z = parseDouble(tokens[5]).value_or(0.0);
                    }
                    else if (k2 == "LIGHT" && k3 == "FALLOFF" && tokens.size() >= 5)
                    {
                        sector.pointLight.minRange = parseDouble(tokens[3]).value_or(0.0);
                        sector.pointLight.maxRange =
                            (tokens.size() >= 5) ? parseDouble(tokens[4]).value_or(0.0) : 0.0;
                    }
                }
                else if (k1 == "SOUND" && tokens.size() >= 3)
                {
                    sector.sound = std::string(tokens[1]);
                    sector.soundVolume = parseDouble(tokens[2]).value_or(0.0);
                }
                else if (k1 == "COLORMAP" && tokens.size() >= 2)
                {
                    sector.colormap = getCmp(parseInt<std::int32_t>(tokens[1]).value_or(-1));
                }
                else if (k1 == "COLLIDEBOX" && tokens.size() >= 7)
                {
                    sector.collideBox.x1 = parseDouble(tokens[1]).value_or(0.0);
                    sector.collideBox.y1 = parseDouble(tokens[2]).value_or(0.0);
                    sector.collideBox.z1 = parseDouble(tokens[3]).value_or(0.0);
                    sector.collideBox.x2 = parseDouble(tokens[4]).value_or(0.0);
                    sector.collideBox.y2 = parseDouble(tokens[5]).value_or(0.0);
                    sector.collideBox.z2 = parseDouble(tokens[6]).value_or(0.0);
                }
                else if (k1 == "TINT" && tokens.size() >= 4)
                {
                    sector.tint.r =
                        parseJklFloat(tokens[1], kJklSectorLightFloatDiagnostic);
                    sector.tint.g =
                        parseJklFloat(tokens[2], kJklSectorLightFloatDiagnostic);
                    sector.tint.b =
                        parseJklFloat(tokens[3], kJklSectorLightFloatDiagnostic);
                    // IJIM's engine wire record has RGB only. Keep the
                    // editor's authored-color alpha convention for an
                    // explicit record while an absent record remains the
                    // all-zero engine default established above.
                    if (level.kind == ProjectType::IJIM)
                    {
                        sector.tint.a = 1.0f;
                    }
                }
                else if (k1 == "THRUST" && tokens.size() >= 4)
                {
                    sector.thrust.x = parseDouble(tokens[1]).value_or(0.0);
                    sector.thrust.y = parseDouble(tokens[2]).value_or(0.0);
                    sector.thrust.z = parseDouble(tokens[3]).value_or(0.0);
                }
                else if (k1 == "VERTICES")
                {
                    // Vertex list is redundant; rebuild from surfaces.
                    if (tokens.size() < 2)
                    {
                        throw std::runtime_error(
                            std::string(kJklSectorVertexCountDiagnostic));
                    }
                    const std::optional<std::int32_t> parsedCount =
                        parseInt<std::int32_t>(tokens[1]);
                    if (!parsedCount || *parsedCount < 0)
                    {
                        throw std::runtime_error(
                            std::string(kJklSectorVertexCountDiagnostic));
                    }
                    std::size_t availableRows = 0;
                    while (idx + availableRows < lines.size())
                    {
                        const auto rowTokens = splitWords(lines[idx + availableRows]);
                        if (!rowTokens.empty()
                            && (iequalsAscii(rowTokens[0], "SURFACES")
                                || iequalsAscii(rowTokens[0], "SECTOR")))
                        {
                            break;
                        }
                        ++availableRows;
                    }
                    if (static_cast<std::size_t>(*parsedCount) > availableRows)
                    {
                        throw std::runtime_error(
                            std::string(kJklSectorVertexCountDiagnostic));
                    }
                    for (std::int32_t i = 0; i < *parsedCount; ++i)
                    {
                        (void)lines[idx++];
                    }
                }
                else if (k1 == "SURFACES")
                {
                    hasSurfacesRecord = true;
                    std::optional<std::int32_t> parsedBase;
                    std::optional<std::int32_t> parsedCount;
                    if ( tokens.size() >= 2 )
                    {
                        parsedBase = parseInt<std::int32_t>(tokens[1]);
                    }
                    if ( tokens.size() >= 3 )
                    {
                        parsedCount = parseInt<std::int32_t>(tokens[2]);
                    }
                    if ( !parsedBase || !parsedCount
                        || *parsedBase < 0 || *parsedCount < 0 )
                    {
                        throw std::runtime_error(
                            "Cannot load JKL/NDY sector: SURFACES span is invalid.");
                    }

                    const std::size_t surfaceBase =
                        static_cast<std::size_t>(*parsedBase);
                    const std::size_t surfaceCount =
                        static_cast<std::size_t>(*parsedCount);
                    if ( surfaceBase > gSurfaces.size()
                        || surfaceCount > gSurfaces.size() - surfaceBase )
                    {
                        throw std::runtime_error(
                            "Cannot load JKL/NDY sector: SURFACES span is invalid.");
                    }

                    for ( std::size_t j = 0; j < surfaceCount; ++j )
                    {
                        const std::size_t geoIndex = surfaceBase + j;
                        const SurfaceTemp& ts = gSurfaces[geoIndex];

                        Surface surf{};
                        surf.nMat = ts.nMat;
                        surf.material = getMat(ts.nMat);
                        surf.surfaceFlags = ts.surfFlags;
                        surf.faceFlags = ts.faceFlags & ~FF_DoubleSided;
                        surf.geo = ts.geo;
                        surf.light = ts.light;
                        surf.tex = ts.tex;
                        surf.nAdj = ts.nAdj;
                        surf.extraLight = ts.extraLight;

                        surf.vertices.reserve(ts.vxs.size());
                        surf.texVertices.reserve(ts.vxs.size());

                        for (std::size_t v = 0; v < ts.vxs.size(); ++v)
                        {
                            const std::int32_t gvx = ts.vxs[v];
                            if (gvx < 0
                                || static_cast<std::size_t>(gvx) >= gVertices.size())
                            {
                                throw std::runtime_error(
                                    std::string(kJklWorldVertexIndexDiagnostic));
                            }
                            auto itLocal = std::find(scvxList.begin(), scvxList.end(), gvx);
                            std::int32_t lvx = (itLocal == scvxList.end())
                                ? -1
                                : static_cast<std::int32_t>(std::distance(scvxList.begin(), itLocal));

                            if (lvx == -1)
                            {
                                sed::Vertex vv{};
                                vv.x = gVertices[static_cast<std::size_t>(gvx)].x;
                                vv.y = gVertices[static_cast<std::size_t>(gvx)].y;
                                vv.z = gVertices[static_cast<std::size_t>(gvx)].z;
                                sector.vertices.push_back(vv);
                                scvxList.push_back(gvx);
                                lvx = static_cast<std::int32_t>(sector.vertices.size() - 1);
                            }

                            surf.vertices.push_back(lvx);

                            TexVertex tv{};
                            const std::int32_t gtvx = (v < ts.tvxs.size()) ? ts.tvxs[v] : 0;
                            if (gtvx >= 0 && static_cast<std::size_t>(gtvx) < gTexVertices.size())
                            {
                                tv.u = gTexVertices[static_cast<std::size_t>(gtvx)].u;
                                tv.v = gTexVertices[static_cast<std::size_t>(gtvx)].v;
                            }

                            tv.color = (v < ts.colors.size()) ? ts.colors[v] : ColorWhite;
                            surf.texVertices.push_back(tv);
                        }

                        surf.normal = sed::level::surfaceCalcNormalNewell(sector, surf);

                        const std::int32_t localIndex = sector.addSurface(std::move(surf));
                        if (localIndex >= 0 && static_cast<std::size_t>(localIndex) < sector.surfaces.size())
                        {
                            sector.surfaces[static_cast<std::size_t>(localIndex)].num = localIndex;
                        }
                        geoSurfaceToRef[static_cast<std::size_t>(geoIndex)] = SurfaceRef{sector.num, localIndex};
                    }

                    break;
                }
            }

            if (!hasSurfacesRecord)
            {
                throw std::runtime_error(
                    std::string(kJklSectorMissingSurfacesDiagnostic));
            }
        }

        // The declared count frames the geometry records in this section.
        // Once those sectors are consumed, another SECTOR record is an
        // inconsistent count rather than ignorable trailing geometry.
        for (; idx < lines.size(); ++idx)
        {
            const auto trailingTokens = splitWords(lines[idx]);
            if (!trailingTokens.empty()
                && iequalsAscii(trailingTokens[0], "SECTOR"))
            {
                throw std::runtime_error(
                    std::string(kJklSectorCountDiagnostic));
            }
        }
    }

    // Preserve the explicit GEORESOURCE-to-editor surface identity for the
    // COG pass.  JKL sector spans are not required to be monotonic, so a wire
    // surface number cannot safely be treated as the editor's flattened
    // global surface number.
    level.importedJklSurfaceRefs = geoSurfaceToRef;

    // Resolve surface adjoins.
    for (Sector& sector : level.sectors)
    {
        for (Surface& surface : sector.surfaces)
        {
            if (surface.nAdj < 0 || static_cast<std::size_t>(surface.nAdj) >= gAdjoins.size())
            {
                surface.adjoin.reset();
                continue;
            }
            const AdjoinTemp& a = gAdjoins[static_cast<std::size_t>(surface.nAdj)];
            if (a.mirror < 0 || static_cast<std::size_t>(a.mirror) >= gAdjoins.size())
            {
                continue;
            }
            const AdjoinTemp& mirror = gAdjoins[static_cast<std::size_t>(a.mirror)];
            if (mirror.surf < 0 || static_cast<std::size_t>(mirror.surf) >= geoSurfaceToRef.size())
            {
                continue;
            }
            surface.adjoin = geoSurfaceToRef[static_cast<std::size_t>(mirror.surf)];
            surface.adjoinFlags = static_cast<std::int32_t>(a.flags);
        }
    }

    if (const auto it = sections.find("COGS"); it != sections.end())
    {
        level.importedJklCogRowsPresent = true;
        const auto& lines = it->second;
        std::size_t idx = 0;

        std::int32_t count = 0;
        if (!lines.empty())
        {
            const auto hdr = splitWords(lines[idx++]);
            if (hdr.size() >= 3)
            {
                count = parseInt<std::int32_t>(hdr[2]).value_or(0);
            }
        }

        for (std::int32_t i = 0; i < count && idx < lines.size(); ++i)
        {
            const auto tokens = splitWords(lines[idx++]);
            if (tokens.size() < 2)
            {
                continue;
            }

            Cog& cog = level.newCog();
            cog.name = std::string(tokens[1]);

            std::size_t paramIndex = 0;
            for (std::size_t t = 2; t < tokens.size(); ++t, ++paramIndex)
            {
                CogValue v{};
                v.name = "param" + std::to_string(paramIndex);
                v.cogType = CogType::unk;
                v.type = ValueType::str;
                // Keep the wire token untouched until all entity collections
                // (especially THINGS) have been decoded and the authoritative
                // game's COG schema is available.
                v.stringValue = std::string(tokens[t]);

                cog.values.push_back(std::move(v));
            }
        }
    }

    if (const auto it = sections.find("TEMPLATES"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        std::int32_t count = 0;
        if (!lines.empty())
        {
            const auto hdr = splitWords(lines[idx++]);
            if (hdr.size() >= 3)
            {
                count = parseInt<std::int32_t>(hdr[2]).value_or(0);
            }
        }

        auto hasTemplate = [&](const std::string_view name) -> bool
        {
            for (const ThingTemplate& tpl : level.templates)
            {
                if (iequalsAscii(tpl.name, name))
                {
                    return true;
                }
            }
            return false;
        };

        for (std::int32_t i = 0; i < count && idx < lines.size(); ++i)
        {
            const auto tokens = splitWords(lines[idx++]);
            if (tokens.size() < 2)
            {
                continue;
            }

            const std::string_view name = tokens[0];
            if (name.empty() || hasTemplate(name))
            {
                continue;
            }

            ThingTemplate tpl{};
            tpl.name = std::string(name);
            tpl.parent = std::string(tokens[1]);

            for (std::size_t t = 2; t < tokens.size(); ++t)
            {
                const std::string_view kv = tokens[t];
                const std::size_t eq = kv.find('=');
                if (eq == std::string_view::npos)
                {
                    continue;
                }

                const std::string_view k = kv.substr(0, eq);
                const std::string_view v = kv.substr(eq + 1);

                tpl.values.push_back(
                    level.kind == ProjectType::IJIM
                        ? deserializeIjimNdyTplValue(k, v)
                        : deserializeTplValue(k, v));
            }

            level.templates.push_back(std::move(tpl));
        }
    }

    if (const auto it = sections.find("THINGS"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        std::int32_t count = 0;
        if (!lines.empty())
        {
            const auto hdr = splitWords(lines[idx++]);
            if (hdr.size() >= 3)
            {
                count = parseInt<std::int32_t>(hdr[2]).value_or(0);
            }
        }

        for (std::int32_t i = 0; i < count && idx < lines.size(); ++i)
        {
            const auto tokens = splitWords(lines[idx++]);
            if (tokens.size() < 10)
            {
                continue;
            }

            Thing& thing = level.newThing();
            thing.templateName = std::string(tokens[1]);
            thing.name = std::string(tokens[2]);
            thing.x = parseDouble(tokens[3]).value_or(0.0);
            thing.y = parseDouble(tokens[4]).value_or(0.0);
            thing.z = parseDouble(tokens[5]).value_or(0.0);
            thing.pch = parseDouble(tokens[6]).value_or(0.0);
            thing.yaw = parseDouble(tokens[7]).value_or(0.0);
            thing.rol = parseDouble(tokens[8]).value_or(0.0);
            thing.sector = parseInt<std::int32_t>(tokens[9]).value_or(-1);
            if (thing.sector < 0 || static_cast<std::size_t>(thing.sector) >= level.sectors.size())
            {
                thing.sector = -1;
            }

            std::size_t valueStart = 10;
            if (level.kind == ProjectType::MOTS
                && valueStart < tokens.size()
                && tokens[valueStart].find('=') == std::string_view::npos)
            {
                const std::optional<std::int32_t> archiveIndex =
                    parseArchLabelIndex(tokens[valueStart]);
                if (!archiveIndex)
                {
                    throw std::runtime_error(
                        "Cannot load JKL/NDY THINGS: MotS architectural-light index is invalid.");
                }
                thing.archLightingIndex = *archiveIndex;
                ++valueStart;
            }

            for (std::size_t t = valueStart; t < tokens.size(); ++t)
            {
                const std::string_view kv = tokens[t];
                const std::size_t eq = kv.find('=');
                if (eq == std::string_view::npos)
                {
                    continue;
                }
                const std::string_view k = kv.substr(0, eq);
                const std::string_view vs = kv.substr(eq + 1);
                thing.values.push_back(
                    level.kind == ProjectType::IJIM
                        ? deserializeIjimNdyTplValue(k, vs)
                        : deserializeTplValue(k, vs));
            }
        }
    }

    if (const auto it = sections.find("ARCHLIGHTING"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;
        level.motsArchLighting.clear();

        if (level.kind == ProjectType::IJIM)
        {
            throw std::runtime_error(
                "Cannot load JKL/NDY ARCHLIGHTING: the section is only valid for MotS.");
        }

        // Older MotS files can carry an empty marker section.  It still
        // identifies the dialect, but has no records to decode.
        if (!lines.empty())
        {
            const auto header = splitWords(lines[idx++]);
            if (header.size() != 3
                || !iequalsAscii(header[0], "num")
                || !iequalsAscii(header[1], "archobjects"))
            {
                throw std::runtime_error(
                    "Cannot load JKL/NDY ARCHLIGHTING: object count is invalid.");
            }
            const std::optional<std::int32_t> objectCount = parseArchCount(header);
            if (!objectCount)
            {
                throw std::runtime_error(
                    "Cannot load JKL/NDY ARCHLIGHTING: object count is invalid.");
            }

            level.motsArchLighting.reserve(std::min<std::size_t>(
                static_cast<std::size_t>(*objectCount), lines.size()));
            std::unordered_set<std::int32_t> objectIndices{};
            objectIndices.reserve(std::min<std::size_t>(
                static_cast<std::size_t>(*objectCount), lines.size()));
            std::size_t decodedMeshRecords = 0;
            std::size_t decodedVertexRecords = 0;

            auto requireLine = [&]() -> const std::vector<std::string_view>
            {
                if (idx >= lines.size())
                {
                    throw std::runtime_error(
                        "Cannot load JKL/NDY ARCHLIGHTING: archobject record is truncated.");
                }
                return splitWords(lines[idx++]);
            };

            for (std::int32_t objectNumber = 0;
                 objectNumber < *objectCount;
                 ++objectNumber)
            {
                const auto objectHeader = requireLine();
                std::size_t objectIndexToken = 1;
                const bool compactObjectLabel = objectHeader.size() >= 2
                    && (iequalsAscii(objectHeader[0], "archobject")
                        || iequalsAscii(objectHeader[0], "object"));
                const bool spacedObjectLabel = objectHeader.size() >= 3
                    && iequalsAscii(objectHeader[0], "arch")
                    && iequalsAscii(objectHeader[1], "object");
                if (!compactObjectLabel && !spacedObjectLabel)
                {
                    throw std::runtime_error(
                        "Cannot load JKL/NDY ARCHLIGHTING: archobject record is truncated.");
                }
                if (spacedObjectLabel)
                {
                    objectIndexToken = 2;
                }
                const std::optional<std::int32_t> objectWireIndex =
                    parseArchLabelIndex(objectHeader[objectIndexToken]);
                if (!objectWireIndex || *objectWireIndex < 0
                    || !objectIndices.insert(*objectWireIndex).second)
                {
                    throw std::runtime_error(
                        "Cannot load JKL/NDY ARCHLIGHTING: object wire index is invalid or duplicated.");
                }

                const auto meshCountLine = requireLine();
                const bool compactMeshCount = meshCountLine.size() >= 2
                    && (iequalsAscii(meshCountLine[0], "nummeshes:")
                        || iequalsAscii(meshCountLine[0], "nummeshes")
                        || iequalsAscii(meshCountLine[0], "num_meshes:"));
                const bool spacedMeshCount = meshCountLine.size() >= 3
                    && iequalsAscii(meshCountLine[0], "num")
                    && iequalsAscii(meshCountLine[1], "meshes");
                if (!compactMeshCount && !spacedMeshCount)
                {
                    throw std::runtime_error(
                        "Cannot load JKL/NDY ARCHLIGHTING: archobject record is truncated.");
                }
                const std::optional<std::int32_t> meshCount =
                    parseArchCount(meshCountLine);
                if (!meshCount)
                {
                    throw std::runtime_error(
                        "Cannot load JKL/NDY ARCHLIGHTING: mesh count is invalid.");
                }

                MotsArchLightObject object{};
                object.wireIndex = *objectWireIndex;
                object.meshes.reserve(std::min<std::size_t>(
                    static_cast<std::size_t>(*meshCount), lines.size() - idx));
                std::unordered_set<std::int32_t> meshIndices{};
                meshIndices.reserve(std::min<std::size_t>(
                    static_cast<std::size_t>(*meshCount), lines.size() - idx));

                for (std::int32_t meshNumber = 0;
                     meshNumber < *meshCount;
                     ++meshNumber)
                {
                    if (++decodedMeshRecords > kMaxMotsArchLightingRecords)
                    {
                        throw std::runtime_error(
                            "Cannot load JKL/NDY ARCHLIGHTING: mesh record count is too large.");
                    }
                    const auto meshHeader = requireLine();
                    if (meshHeader.size() < 2
                        || !iequalsAscii(meshHeader[0], "mesh"))
                    {
                        throw std::runtime_error(
                            "Cannot load JKL/NDY ARCHLIGHTING: archobject record is truncated.");
                    }
                    const std::optional<std::int32_t> meshWireIndex =
                        parseArchLabelIndex(meshHeader[1]);
                    if (!meshWireIndex || *meshWireIndex < 0
                        || !meshIndices.insert(*meshWireIndex).second)
                    {
                        throw std::runtime_error(
                            "Cannot load JKL/NDY ARCHLIGHTING: mesh wire index is invalid or duplicated.");
                    }

                    const auto vertexCountLine = requireLine();
                    const bool compactVertexCount = vertexCountLine.size() >= 2
                        && (iequalsAscii(vertexCountLine[0], "numvertices:")
                            || iequalsAscii(vertexCountLine[0], "numvertices")
                            || iequalsAscii(vertexCountLine[0], "num_vertices:"));
                    const bool spacedVertexCount = vertexCountLine.size() >= 3
                        && iequalsAscii(vertexCountLine[0], "num")
                        && iequalsAscii(vertexCountLine[1], "vertices");
                    if (!compactVertexCount && !spacedVertexCount)
                    {
                        throw std::runtime_error(
                            "Cannot load JKL/NDY ARCHLIGHTING: mesh record is truncated.");
                    }
                    const std::optional<std::int32_t> vertexCount =
                        parseArchCount(vertexCountLine);
                    if (!vertexCount)
                    {
                        throw std::runtime_error(
                            "Cannot load JKL/NDY ARCHLIGHTING: vertex count is invalid.");
                    }

                    MotsArchLightMesh mesh{};
                    mesh.wireIndex = *meshWireIndex;
                    mesh.vertices.reserve(std::min<std::size_t>(
                        static_cast<std::size_t>(*vertexCount), lines.size() - idx));
                    std::unordered_set<std::int32_t> vertexIndices{};
                    vertexIndices.reserve(std::min<std::size_t>(
                        static_cast<std::size_t>(*vertexCount), lines.size() - idx));

                    for (std::int32_t vertexNumber = 0;
                         vertexNumber < *vertexCount;
                         ++vertexNumber)
                    {
                        if (++decodedVertexRecords > kMaxMotsArchLightingRecords)
                        {
                            throw std::runtime_error(
                                "Cannot load JKL/NDY ARCHLIGHTING: vertex record count is too large.");
                        }
                        const auto vertexTokens = requireLine();
                        if (vertexTokens.size() < 5)
                        {
                            throw std::runtime_error(
                                "Cannot load JKL/NDY ARCHLIGHTING: vertex record is truncated.");
                        }
                        const std::optional<std::int32_t> vertexWireIndex =
                            parseArchLabelIndex(vertexTokens[0]);
                        if (!vertexWireIndex || *vertexWireIndex < 0
                            || !vertexIndices.insert(*vertexWireIndex).second)
                        {
                            throw std::runtime_error(
                                "Cannot load JKL/NDY ARCHLIGHTING: vertex wire index is invalid or duplicated.");
                        }

                        mesh.vertices.push_back(MotsArchLightVertex{
                            parseMotsArchLightFloat(vertexTokens[1]),
                            parseMotsArchLightFloat(vertexTokens[2]),
                            parseMotsArchLightFloat(vertexTokens[3]),
                            parseMotsArchLightFloat(vertexTokens[4]),
                        });
                    }
                    object.meshes.push_back(std::move(mesh));
                }
                level.motsArchLighting.push_back(std::move(object));
            }

            if (idx != lines.size())
            {
                throw std::runtime_error(
                    "Cannot load JKL/NDY ARCHLIGHTING: unexpected record after declared objects.");
            }
        }
    }

    if (level.kind == ProjectType::MOTS)
    {
        for (const Thing& thing : level.things)
        {
            if (thing.archLightingIndex < -1
                || (thing.archLightingIndex >= 0
                    && static_cast<std::size_t>(thing.archLightingIndex)
                        >= level.motsArchLighting.size()))
            {
                throw std::runtime_error(
                    "Cannot load JKL/NDY ARCHLIGHTING: Thing architectural-light index is invalid.");
            }
        }
    }

    if (const auto it = sections.find("LIGHTS"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        std::int32_t count = 0;
        if (!lines.empty())
        {
            const auto hdr = splitWords(lines[idx++]);
            if (hdr.size() >= 3)
            {
                count = parseInt<std::int32_t>(hdr[2]).value_or(0);
            }
        }

        for (std::int32_t i = 0; i < count && idx < lines.size(); ++i)
        {
            const auto tokens = splitWords(lines[idx++]);
            if (tokens.empty())
            {
                continue;
            }
            if (iequalsAscii(tokens[0], "END"))
            {
                break;
            }

            SedLight& light = level.newLight();

            if (tokens.size() >= 11)
            {
                light.flags = static_cast<std::int32_t>(parseHex(tokens[1]).value_or(0));
                light.layer = parseInt<std::int32_t>(tokens[2]).value_or(0);
                light.position.x = parseDouble(tokens[3]).value_or(0.0);
                light.position.y = parseDouble(tokens[4]).value_or(0.0);
                light.position.z = parseDouble(tokens[5]).value_or(0.0);
                light.range = parseDouble(tokens[6]).value_or(2.0);
                light.intensity = parseDouble(tokens[7]).value_or(10.0);
                light.color.r =
                    parseJklFloat(tokens[8], kJklEditorLightColorFloatDiagnostic);
                light.color.g =
                    parseJklFloat(tokens[9], kJklEditorLightColorFloatDiagnostic);
                light.color.b =
                    parseJklFloat(tokens[10], kJklEditorLightColorFloatDiagnostic);
            }
            else if (tokens.size() >= 7)
            {
                light.layer = parseInt<std::int32_t>(tokens[1]).value_or(0);
                light.position.x = parseDouble(tokens[2]).value_or(0.0);
                light.position.y = parseDouble(tokens[3]).value_or(0.0);
                light.position.z = parseDouble(tokens[4]).value_or(0.0);
                light.intensity = parseDouble(tokens[5]).value_or(10.0);
                light.range = parseDouble(tokens[6]).value_or(2.0);
                light.color = ColorWhite;
            }
        }
    }

    if (const auto it = sections.find("LAYERS"); it != sections.end())
    {
        const auto& lines = it->second;
        std::size_t idx = 0;

        std::int32_t count = 0;
        if (!lines.empty())
        {
            const auto hdr = splitWords(lines[idx++]);
            if (hdr.size() >= 3)
            {
                count = parseInt<std::int32_t>(hdr[2]).value_or(0);
            }
        }

        auto assignIndices = [&](auto& collection, const std::int32_t layerIndex)
        {
            if (idx >= lines.size())
            {
                return;
            }

            std::string raw = lines[idx++];
            auto tokens = splitWords(raw);
            if (tokens.empty())
            {
                return;
            }

            std::string_view head = tokens[0];
            if (const std::size_t colon = head.find(':'); colon != std::string_view::npos)
            {
                head = head.substr(0, colon);
            }
            const std::int32_t n = parseInt<std::int32_t>(head).value_or(0);

            std::int32_t got = 0;
            std::size_t t = 1;
            while (got < n && idx <= lines.size())
            {
                if (t >= tokens.size())
                {
                    if (idx >= lines.size())
                    {
                        break;
                    }
                    raw = lines[idx++];
                    tokens = splitWords(raw);
                    t = 0;
                    continue;
                }

                const auto v = parseInt<std::int32_t>(tokens[t++]);
                if (!v)
                {
                    continue;
                }

                const std::int32_t pos = *v;
                if (pos >= 0 && static_cast<std::size_t>(pos) < collection.size())
                {
                    collection[static_cast<std::size_t>(pos)].layer = layerIndex;
                }
                ++got;
            }
        };

        for (std::int32_t i = 0; i < count && idx < lines.size(); ++i)
        {
            level.layers.push_back(lines[idx++]);
            assignIndices(level.sectors, i);
            assignIndices(level.things, i);
        }

        if (idx < lines.size())
        {
            level.lvisstring = lines[idx++];
        }
    }

    // Post-process geometry (matches Delphi LoadFromJKL behavior).
    const double ppu = sed::level::levelPixelPerUnit(level);
    for (std::size_t sectorIndex = 0;
         sectorIndex < level.sectors.size();
         ++sectorIndex)
    {
        Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0;
             surfaceIndex < sector.surfaces.size();
             ++surfaceIndex)
        {
            Surface& surface = sector.surfaces[surfaceIndex];
            surface.normal = sed::level::surfaceCalcNormalNewell(sector, surface);

            if (sed::level::surfaceIsTextureFlipped(sector, surface))
            {
                surface.faceFlags |= FF_SF_FLIP;
            }
            else
            {
                surface.faceFlags &= ~FF_SF_FLIP;
            }

            // UV-scale recovery is best effort for compatibility with legacy
            // files. Some shipped and editor-authored levels contain dormant
            // zero-area surfaces with repeated corners, so no geometric UV
            // basis exists. The original loader kept the Surface defaults and
            // continued; valid, finite UV values are still checked at their
            // parse and coordinate-space conversion boundaries.
            (void)sed::level::surfaceFindUVScales(ppu, sector, surface);
        }
    }

    level.materials = materials;
    level.colormaps = colormaps;

    // Preserve the exact spare capacity declared by an NDY/JKL: each editable
    // extra is the world capacity minus the required records present in that
    // section. Exact-record geometry headers are deliberately excluded;
    // inflating those would make the runtime parse nonexistent records.
    auto declaredCapacity = [&sections](const std::string_view sectionName) -> std::optional<std::size_t>
    {
        const auto it = sections.find(std::string(sectionName));
        if (it == sections.end() || it->second.empty())
        {
            return std::nullopt;
        }

        const auto tokens = splitWords(it->second.front());
        if (tokens.empty())
        {
            return std::nullopt;
        }
        const auto parsed = parseInt<std::int64_t>(tokens.back());
        if (!parsed || *parsed < 0)
        {
            return std::nullopt;
        }
        return static_cast<std::size_t>(*parsed);
    };

    auto recordCount = [&sections](const std::string_view sectionName) -> std::size_t
    {
        const auto it = sections.find(std::string(sectionName));
        return (it == sections.end() || it->second.empty()) ? 0 : it->second.size() - 1;
    };

    auto inferredExtra = [&declaredCapacity](
                             const std::string_view sectionName,
                             const std::size_t requiredCount) -> std::uint32_t
    {
        const std::optional<std::size_t> declared = declaredCapacity(sectionName);
        if (!declared || *declared <= requiredCount)
        {
            return 0;
        }
        const std::size_t difference = *declared - requiredCount;
        return static_cast<std::uint32_t>(std::min<std::size_t>(
            difference, std::numeric_limits<std::uint32_t>::max()));
    };

    LevelExportBufferExtras& extras = level.exportBufferExtras;
    extras.sounds = inferredExtra("SOUNDS", recordCount("SOUNDS"));
    extras.materials = inferredExtra("MATERIALS", recordCount("MATERIALS"));
    extras.aiClasses = inferredExtra("AICLASS", recordCount("AICLASS"));
    extras.models = inferredExtra("MODELS", recordCount("MODELS"));
    extras.sprites = inferredExtra("SPRITES", recordCount("SPRITES"));
    extras.keyframes = inferredExtra("KEYFRAMES", recordCount("KEYFRAMES"));
    extras.puppets = inferredExtra("ANIMCLASS", recordCount("ANIMCLASS"));
    extras.soundClasses = inferredExtra("SOUNDCLASS", recordCount("SOUNDCLASS"));
    extras.cogScripts = inferredExtra("COGSCRIPTS", recordCount("COGSCRIPTS"));

    std::size_t requiredCogCapacity = recordCount("COGS");
    for (const ThingTemplate& tpl : level.templates)
    {
        requiredCogCapacity += countExplicitCogArguments(tpl.values);
    }
    for (const Thing& thing : level.things)
    {
        requiredCogCapacity += countExplicitCogArguments(thing.values);
    }
    extras.cogs = inferredExtra("COGS", requiredCogCapacity);
    extras.templates = inferredExtra("TEMPLATES", recordCount("TEMPLATES"));
    extras.things = inferredExtra("THINGS", recordCount("THINGS"));

    if (options.resources != nullptr)
    {
        finalizeJklImportResources(level, *options.resources, options.onDiagnostic);
    }

    return level;
}

Level loadFromJKL(const std::filesystem::path& path, const JklLoadOptions& options)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open JKL file.");
    }
    return loadFromJKL(file, options);
}

void saveToJKL(const Level& level, std::ostream& out)
{
    saveToJKL(level, out, {});
}

static std::string pendingJklCogName(const Level& level)
{
    for (const Cog& cog : level.cogs)
    {
        for (std::size_t index = 0; index < cog.values.size(); ++index)
        {
            if (isRawJklCogValue(cog.values[index], index))
            {
                return cog.name.empty() ? std::string("<unnamed>") : cog.name;
            }
        }
        if (level.importedJklCogRowsPresent && cog.values.empty())
        {
            return cog.name.empty() ? std::string("<unnamed>") : cog.name;
        }
    }
    return "<unnamed>";
}

bool isJklPlacedCogValue(const CogValue& value) noexcept
{
    // Local implementation symbols and message declarations are deliberately
    // omitted by loadCogSymbols(..., false), which is the placed-COG schema.
    // Keep native levels containing those declarations from shifting every
    // following engine argument on JKL/NDY export.
    return !value.local && value.cogType != CogType::msg;
}

bool isJklCogResourceType(const CogType type) noexcept
{
    switch (type)
    {
    case CogType::ai:
    case CogType::key:
    case CogType::mat:
    case CogType::msg:
    case CogType::model3do:
    case CogType::wav:
    case CogType::template_:
        return true;
    default:
        return false;
    }
}

bool containsWhitespace(const std::string_view text) noexcept
{
    return std::any_of(text.begin(), text.end(), [](const char ch)
    {
        return std::isspace(static_cast<unsigned char>(ch)) != 0;
    });
}

bool containsJklCommentDelimiter(const std::string_view text) noexcept
{
    return text.find('#') != std::string_view::npos;
}

std::optional<std::string> canonicalJklCogVector(const std::string_view raw)
{
    std::string text = trim(raw);
    if (text.empty())
    {
        return std::string("(0/0/0)");
    }

    const bool hasOpeningParenthesis = text.front() == '(';
    const bool hasClosingParenthesis = text.back() == ')';
    if (hasOpeningParenthesis != hasClosingParenthesis)
    {
        return std::nullopt;
    }
    if (hasOpeningParenthesis)
    {
        if (text.size() < 2)
        {
            return std::nullopt;
        }
        text.erase(text.begin());
        text.pop_back();
    }

    for (char& ch : text)
    {
        if (ch == '/' || ch == ':' || ch == ',' || ch == ';')
        {
            ch = ' ';
        }
    }

    const std::vector<std::string_view> components = splitWords(text);
    if (components.size() != 3)
    {
        return std::nullopt;
    }

    std::array<double, 3> values{};
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        const std::optional<double> parsed = parseDouble(components[index]);
        if (!parsed || !std::isfinite(*parsed))
        {
            return std::nullopt;
        }
        values[index] = *parsed;
    }

    std::string canonical{"("};
    canonical += sed::format::authoredDouble(values[0]);
    canonical.push_back('/');
    canonical += sed::format::authoredDouble(values[1]);
    canonical.push_back('/');
    canonical += sed::format::authoredDouble(values[2]);
    canonical.push_back(')');
    return canonical;
}

std::string serializeJklCogValue(
    const Cog& cog,
    const CogValue& value,
    const std::size_t valueIndex,
    const Level& level)
{
    if (value.type == ValueType::ptr && value.cogType == CogType::surface
        && containsWhitespace(trimView(value.stringValue)))
    {
        CogValue parsed = value;
        parsed.intValue = -1;
        if (!parsed.parseJed(level, trimView(value.stringValue)))
        {
            // Geometry/reference maintenance treats authored pair text as a
            // preferred spelling. A stale pair must not erase a still-valid
            // typed global target; retain that target when it resolves.
            if (level.tryGetSurfaceRefByGlobalSurfaceNumber(value.intValue))
            {
                return std::to_string(value.intValue);
            }
            throw std::runtime_error(
                "Cannot export JKL/NDY: COG '" + cog.name + "' value "
                + std::to_string(valueIndex) + " has an invalid surface pair '"
                + value.stringValue + "'.");
        }
        return std::to_string(parsed.intValue);
    }

    if (value.type == ValueType::vect)
    {
        if (const std::optional<std::string> canonical =
                canonicalJklCogVector(value.stringValue))
        {
            return *canonical;
        }

        throw std::runtime_error(
            "Cannot export JKL/NDY: COG '" + cog.name + "' value "
            + std::to_string(valueIndex)
            + " vector is not a finite three-component value.");
    }

    if (value.type == ValueType::str)
    {
        const std::string_view text = trimView(value.stringValue);
        if (containsJklCommentDelimiter(text))
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY: COG '" + cog.name + "' value "
                + std::to_string(valueIndex)
                + " contains '#' and cannot be represented as one engine token.");
        }
        if (isJklCogResourceType(value.cogType))
        {
            if (text.empty())
            {
                return "-1";
            }
            if (containsWhitespace(text))
            {
                throw std::runtime_error(
                    "Cannot export JKL/NDY: COG '" + cog.name + "' value "
                    + std::to_string(valueIndex) + " resource '"
                    + std::string(toString(value.cogType))
                    + "' contains whitespace and cannot be represented as one engine token.");
            }
            return std::string(text);
        }

        if (text.empty())
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY: COG '" + cog.name + "' value "
                + std::to_string(valueIndex)
                + " is empty and cannot be represented as one engine token.");
        }
        if (containsWhitespace(text))
        {
            throw std::runtime_error(
                "Cannot export JKL/NDY: COG '" + cog.name + "' value "
                + std::to_string(valueIndex)
                + " contains whitespace and cannot be represented as one engine token.");
        }
        return std::string(text);
    }

    if (value.type == ValueType::floating && !std::isfinite(value.floatValue))
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY: COG '" + cog.name + "' value "
            + std::to_string(valueIndex) + " floating value is not finite.");
    }

    return value.asString();
}

void validateJklCogValues(const Level& level)
{
    for (const Cog& cog : level.cogs)
    {
        for (std::size_t valueIndex = 0; valueIndex < cog.values.size(); ++valueIndex)
        {
            if (!isJklPlacedCogValue(cog.values[valueIndex]))
            {
                continue;
            }
            (void)serializeJklCogValue(
                cog, cog.values[valueIndex], valueIndex, level);
        }
    }
}

static void saveToJklImpl(
    const Level& level,
    std::ostream& out,
    const JklSaveOptions& options,
    JklExportBufferReport* const bufferReport,
    const bool reportOnly)
{
    if (level.hasAmbiguousPendingJklCogSurfaceReferences())
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY: imported COG surface references require "
            "resource-backed COG script '" + pendingJklCogName(level)
            + "' before serialization.");
    }
    validateJklCogValues(level);

    const bool isIjim = level.kind == ProjectType::IJIM;
    const bool headerVersionMatchesProject = isIjim
        ? level.header.version == 2 || level.header.version == 3
        : level.header.version == 1;
    if (!headerVersionMatchesProject)
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY: the header version contradicts project type.");
    }
    if (level.uvCoordinateSpace != UvCoordinateSpace::TexturePixels
        && level.uvCoordinateSpace != UvCoordinateSpace::Normalized)
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY: the Level UV coordinate space is invalid.");
    }
    if (options.weldNearbyVertices
        && (!std::isfinite(options.vertexWeldDistance)
            || options.vertexWeldDistance <= 0.0))
    {
        throw std::runtime_error(
            "Cannot export JKL/NDY: vertex weld distance must be finite and positive.");
    }
    for (std::size_t sectorIndex = 0;
         sectorIndex < level.sectors.size();
         ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        // Validate all sector light channels before any serialized bytes are
        // produced. This keeps failure atomic even when a later field is
        // nonfinite and prevents a partially emitted level.
        validateSectorLightColor(sector.ambient);
        validateSectorLightColor(sector.extraLight);
        for (std::size_t vertexIndex = 0;
             vertexIndex < sector.vertices.size();
             ++vertexIndex)
        {
            const Vertex& vertex = sector.vertices[vertexIndex];
            if (!std::isfinite(vertex.x)
                || !std::isfinite(vertex.y)
                || !std::isfinite(vertex.z))
            {
                throw std::runtime_error(
                    "Cannot export JKL/NDY: world coordinates must be finite at "
                    "sector "
                    + std::to_string(sectorIndex)
                    + ", vertex "
                    + std::to_string(vertexIndex)
                    + ".");
            }
            if (options.weldNearbyVertices
                && (!tryJklWeldCellCoordinate(vertex.x, options.vertexWeldDistance)
                    || !tryJklWeldCellCoordinate(vertex.y, options.vertexWeldDistance)
                    || !tryJklWeldCellCoordinate(vertex.z, options.vertexWeldDistance)))
            {
                throw std::runtime_error(
                    "Cannot export JKL/NDY: nearby vertex welding spatial cell "
                    "range is unsupported at sector "
                    + std::to_string(sectorIndex)
                    + ", vertex "
                    + std::to_string(vertexIndex)
                    + ".");
            }
        }
    }
    for (std::size_t sectorIndex = 0;
         sectorIndex < level.sectors.size();
         ++sectorIndex)
    {
        const Sector& sector = level.sectors[sectorIndex];
        for (std::size_t surfaceIndex = 0;
             surfaceIndex < sector.surfaces.size();
             ++surfaceIndex)
        {
            const Surface& surface = sector.surfaces[surfaceIndex];
            if (surface.texVertices.size() != surface.vertices.size())
            {
                throw std::runtime_error(
                    "Cannot export JKL/NDY: texture-vertex data is incomplete at sector "
                    + std::to_string(sectorIndex) + ", surface "
                    + std::to_string(surfaceIndex) + ".");
            }
        }
    }

    Level validatedLevel = level;
    const level::LevelUvConversionResult validated =
        level::convertLevelUvCoordinateSpace(
            validatedLevel,
            level.uvCoordinateSpace,
            [](const Surface&)
                -> std::optional<level::UvTextureDimensions>
            {
                return std::nullopt;
            });
    if (!validated)
    {
        const std::string location = validated.failedSurface
            ? " at sector "
                + std::to_string(validated.failedSurface->sector)
                + ", surface "
                + std::to_string(validated.failedSurface->surface)
            : std::string{};
        throw std::runtime_error(
            "Cannot export JKL/NDY: a UV value is invalid or outside the supported numeric range"
            + location + ".");
    }

    ScopedStreamLocale scopedLocale(out);
    out.imbue(std::locale::classic());
    out << std::fixed;
    CompactOutputScope compactOutput(out, options.compact && !reportOnly);

    auto capacityWithExtra = [](const std::size_t automaticCapacity, const std::uint32_t extra) -> std::size_t
    {
        constexpr std::size_t kMaxTextWorldCapacity =
            static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
        if (automaticCapacity >= kMaxTextWorldCapacity)
        {
            return kMaxTextWorldCapacity;
        }
        return automaticCapacity
            + std::min<std::size_t>(extra, kMaxTextWorldCapacity - automaticCapacity);
    };

    struct WorldVertex
    {
        double x{};
        double y{};
        double z{};
    };

    struct ExactVertexKey
    {
        double x{};
        double y{};
        double z{};

        constexpr bool operator==(const ExactVertexKey&) const noexcept = default;
    };

    struct ExactVertexKeyHash
    {
        std::size_t operator()(const ExactVertexKey& key) const noexcept
        {
            std::size_t hash = std::hash<double>{}(key.x);
            hash ^= std::hash<double>{}(key.y)
                + static_cast<std::size_t>(0x9e3779b9u)
                + (hash << 6u)
                + (hash >> 2u);
            hash ^= std::hash<double>{}(key.z)
                + static_cast<std::size_t>(0x9e3779b9u)
                + (hash << 6u)
                + (hash >> 2u);
            return hash;
        }
    };

    struct WeldCellKey
    {
        std::int64_t x{};
        std::int64_t y{};
        std::int64_t z{};

        constexpr bool operator==(const WeldCellKey&) const noexcept = default;
    };

    struct WeldCellKeyHash
    {
        std::size_t operator()(const WeldCellKey& key) const noexcept
        {
            std::size_t hash = std::hash<std::int64_t>{}(key.x);
            hash ^= std::hash<std::int64_t>{}(key.y)
                + static_cast<std::size_t>(0x9e3779b9u)
                + (hash << 6u)
                + (hash >> 2u);
            hash ^= std::hash<std::int64_t>{}(key.z)
                + static_cast<std::size_t>(0x9e3779b9u)
                + (hash << 6u)
                + (hash >> 2u);
            return hash;
        }
    };

    struct WorldTexVertex
    {
        float u{};
        float v{};
    };

    struct WorldSurface
    {
        const Surface* src{};
        TVector normal{};
        bool weldChanged{};

        std::int32_t nMat{-1};
        std::int32_t surfFlags{};
        std::uint32_t faceFlags{};
        std::int32_t geo{};
        std::int32_t light{};
        std::int32_t tex{};
        std::int32_t nAdj{-1};
        ColorF extraLight{ColorBlack};

        std::vector<std::int32_t> vxs{};
        std::vector<std::int32_t> tvxs{};
        std::vector<ColorF> colors{};
    };

    struct WorldAdjoin
    {
        std::uint32_t flags{};
        std::int32_t mirror{-1};
        double dist{};
    };

    const bool isMots = (level.kind == ProjectType::MOTS);
    const MotsArchLightingExportSnapshot archLighting =
        makeMotsArchLightingExportSnapshot(level, options);
    validateMotsArchLighting(level, isMots, archLighting);
    validateActiveMotsArchLightingTopologies(level, options, archLighting);
    const UvCoordinateSpace wireUvCoordinateSpace = isIjim
        ? UvCoordinateSpace::Normalized
        : UvCoordinateSpace::TexturePixels;
    if (!isIjim)
    {
        out << "SECTION: JK\n\n";
    }

    out << "SECTION: COPYRIGHT\n";
    out << (isIjim ? kLecLogoIJIM : kLecLogoJK) << "\n\n";

    struct UsedStrings
    {
        std::vector<std::string> items{};
        std::unordered_set<std::string> keys{};

        void add(const std::string_view value)
        {
            const std::string trimmed = std::string(trimView(value));
            if (trimmed.empty()
                || iequalsAscii(trimmed, "none")
                || iequalsAscii(trimmed, "-1"))
            {
                return;
            }

            const std::string key = upperAscii(trimmed);
            if (!keys.emplace(key).second)
            {
                return;
            }

            items.push_back(trimmed);
        }

        void sort()
        {
            std::sort(items.begin(), items.end(), [](const std::string& a, const std::string& b)
                {
                    return upperAscii(a) < upperAscii(b);
                });
        }
    };

    UsedStrings usedWavs{};
    UsedStrings usedMats{};
    UsedStrings usedCmps{};

    UsedStrings usedAIs{};
    UsedStrings used3DOs{};
    UsedStrings usedSprs{};
    UsedStrings usedPars{};
    UsedStrings usedKeys{};
    UsedStrings usedPups{};
    UsedStrings usedSnds{};
    UsedStrings usedCogs{};

    bool buffersMayBeUnderestimated = false;

    struct CogWorkItem
    {
        std::string name{};
        bool includeInScriptsList{true};
    };

    std::vector<CogWorkItem> cogQueue{};
    std::vector<std::string> tplQueue{};
    std::unordered_set<std::string> parsedCogKeys{};
    std::unordered_set<std::string> parsedTplKeys{};
    std::vector<const ThingTemplate*> usedTemplatesToWrite{};
    usedTemplatesToWrite.reserve(level.templates.size() + level.things.size() + 64);

    auto enqueueCog = [&](const std::string_view name, const bool includeInScriptsList)
    {
        const std::string trimmed = std::string(trimView(name));
        if (trimmed.empty() || iequalsAscii(trimmed, "none"))
        {
            return;
        }

        cogQueue.push_back(CogWorkItem{ trimmed, includeInScriptsList });
    };

    auto enqueueTpl = [&](const std::string_view name)
    {
        const std::string trimmed = std::string(trimView(name));
        if (trimmed.empty() || iequalsAscii(trimmed, "none"))
        {
            return;
        }

        tplQueue.push_back(trimmed);
    };

    std::unordered_map<std::string, std::size_t> templateNameToIndex{};
    templateNameToIndex.reserve(level.templates.size());
    for (std::size_t i = 0; i < level.templates.size(); ++i)
    {
        const std::string key = upperAscii(trimView(level.templates[i].name));
        if (!key.empty() && !templateNameToIndex.contains(key))
        {
            templateNameToIndex.emplace(key, i);
        }
    }

    if (options.templatesDb != nullptr)
    {
        for (const std::string_view tplName : staticTemplates(level.kind))
        {
            enqueueTpl(tplName);
        }
    }

    if (!isIjim)
    {
        usedCmps.add(level.masterCMP);
    }

    for (const Sector& sector : level.sectors)
    {
        usedWavs.add(sector.sound);
        if (!isIjim)
        {
            usedCmps.add(sector.colormap);
        }

        for (const Surface& surf : sector.surfaces)
        {
            usedMats.add(surf.material);
        }
    }

    for (const Thing& thing : level.things)
    {
        enqueueTpl(!thing.templateName.empty() ? std::string_view(thing.templateName) : std::string_view(thing.name));

        for (const TplValue& v : thing.values)
        {
            if (v.stringValue.empty())
            {
                continue;
            }

            switch (v.adjType)
            {
            case AdjType::ai:
                usedAIs.add(v.stringValue);
                break;
            case AdjType::cog:
                enqueueCog(v.stringValue, true);
                break;
            case AdjType::mat:
                usedMats.add(v.stringValue);
                break;
            case AdjType::model3do:
                used3DOs.add(v.stringValue);
                break;
            case AdjType::par:
                usedPars.add(v.stringValue);
                break;
            case AdjType::pup:
                usedPups.add(v.stringValue);
                break;
            case AdjType::snd:
                usedSnds.add(v.stringValue);
                break;
            case AdjType::spr:
                usedSprs.add(v.stringValue);
                break;
            case AdjType::tpl:
                enqueueTpl(v.stringValue);
                break;
            default:
                break;
            }
        }
    }

    for (const Cog& cog : level.cogs)
    {
        enqueueCog(cog.name, true);

        for (const CogValue& v : cog.values)
        {
            if (v.stringValue.empty())
            {
                continue;
            }

            switch (v.cogType)
            {
            case CogType::ai:
                usedAIs.add(v.stringValue);
                break;
            case CogType::cog:
                enqueueCog(v.stringValue, true);
                break;
            case CogType::key:
                usedKeys.add(v.stringValue);
                break;
            case CogType::mat:
                usedMats.add(v.stringValue);
                break;
            case CogType::model3do:
                used3DOs.add(v.stringValue);
                break;
            case CogType::template_:
                enqueueTpl(v.stringValue);
                break;
            case CogType::wav:
                usedWavs.add(v.stringValue);
                break;
            default:
                break;
            }
        }
    }

    if (options.resources != nullptr)
    {
        if (const auto itemsDatText = options.resources->tryReadText("items.dat"))
        {
            if (const auto itemsDiskPath = options.resources->findOnDisk("items.dat"))
            {
                const std::string msg = "JKL save: using items.dat from disk: " + itemsDiskPath->string();
                sed::diagnosticEmit(options.onDiagnostic, sed::DiagnosticSeverity::debug, msg);
            }
            else
            {
                const auto candidates = options.resources->candidates("items.dat");
                for (const std::string& candidate : candidates)
                {
                    bool foundInArchive = false;
                    for (auto it = options.resources->gobs().rbegin(); it != options.resources->gobs().rend(); ++it)
                    {
                        if (it->contains(candidate))
                        {
                            const std::string msg =
                                "JKL save: using items.dat from archive: " + it->path().string() + " (" + candidate + ")";
                            sed::diagnosticEmit(options.onDiagnostic, sed::DiagnosticSeverity::debug, msg);
                            foundInArchive = true;
                            break;
                        }
                    }
                    if (foundInArchive)
                    {
                        break;
                    }
                }
            }

            std::istringstream iss(*itemsDatText);
            const ItemsDatFile itemsDat = loadFromItemsDat(iss);
            for (const std::string& cogName : itemsDat.cogs)
            {
                enqueueCog(cogName, false);
            }
        }
    }

    auto processTemplate = [&](const std::string_view tplName)
    {
        const std::string trimmed = std::string(trimView(tplName));
        const std::string key = upperAscii(trimmed);
        if (key.empty())
        {
            return;
        }

        if (!parsedTplKeys.emplace(key).second)
        {
            return;
        }

        const ThingTemplate* tpl = nullptr;
        if (const auto it = templateNameToIndex.find(key); it != templateNameToIndex.end())
        {
            tpl = &level.templates[it->second];
        }
        else if (options.templatesDb != nullptr)
        {
            tpl = options.templatesDb->find(tplName);
        }

        if (tpl == nullptr)
        {
            const std::string ext = sed::text::lowerAscii(std::filesystem::path(trimmed).extension().string());
            if (!ext.empty())
            {
                if (ext == ".3do")
                {
                    used3DOs.add(trimmed);
                    return;
                }
                if (ext == ".mat")
                {
                    usedMats.add(trimmed);
                    return;
                }
                if (ext == ".cog")
                {
                    enqueueCog(trimmed, true);
                    return;
                }
                if (ext == ".wav")
                {
                    usedWavs.add(trimmed);
                    return;
                }
                if (ext == ".key")
                {
                    usedKeys.add(trimmed);
                    return;
                }
                if (ext == ".par")
                {
                    usedPars.add(trimmed);
                    return;
                }
                if (ext == ".pup")
                {
                    usedPups.add(trimmed);
                    return;
                }
                if (ext == ".snd")
                {
                    usedSnds.add(trimmed);
                    return;
                }
                if (ext == ".spr")
                {
                    usedSprs.add(trimmed);
                    return;
                }
                if (ext == ".ai" || ext == ".ai0" || ext == ".ai2")
                {
                    usedAIs.add(trimmed);
                    return;
                }
            }

            buffersMayBeUnderestimated = true;
            const std::string msg = "JKL save: missing template '" + trimmed + "'; export buffers may be underestimated";
            sed::diagnosticEmit(options.onDiagnostic, sed::DiagnosticSeverity::warning, msg);
            return;
        }

        usedTemplatesToWrite.push_back(tpl);

        if (!tpl->parent.empty() && !iequalsAscii(tpl->parent, "none"))
        {
            enqueueTpl(tpl->parent);
        }

        for (const TplValue& v : tpl->values)
        {
            if (v.stringValue.empty())
            {
                continue;
            }

            switch (v.adjType)
            {
            case AdjType::ai:
                usedAIs.add(v.stringValue);
                break;
            case AdjType::cog:
                enqueueCog(v.stringValue, true);
                break;
            case AdjType::mat:
                usedMats.add(v.stringValue);
                break;
            case AdjType::model3do:
                used3DOs.add(v.stringValue);
                break;
            case AdjType::par:
                usedPars.add(v.stringValue);
                break;
            case AdjType::pup:
                usedPups.add(v.stringValue);
                break;
            case AdjType::snd:
                usedSnds.add(v.stringValue);
                break;
            case AdjType::spr:
                usedSprs.add(v.stringValue);
                break;
            case AdjType::tpl:
                enqueueTpl(v.stringValue);
                break;
            default:
                break;
            }
        }
    };

    auto processCog = [&](const CogWorkItem& item)
    {
        const std::string key = upperAscii(trimView(item.name));
        if (key.empty())
        {
            return;
        }

        if (item.includeInScriptsList)
        {
            usedCogs.add(item.name);
        }

        if (!parsedCogKeys.emplace(key).second)
        {
            return;
        }

        if (options.resources == nullptr)
        {
            return;
        }

        if (!options.resources->tryReadTextWithExt(item.name, ".cog"))
        {
            buffersMayBeUnderestimated = true;
            const std::string msg = "JKL save: missing resource '" + item.name
                + "'; export buffers may be underestimated";
            sed::diagnosticEmit(
                options.onDiagnostic, sed::DiagnosticSeverity::warning, msg);
            return;
        }

        const std::vector<CogValue> symbols = loadCogSymbolsWithDefaults(*options.resources, item.name, true);
        for (const CogValue& v : symbols)
        {
            if (v.stringValue.empty())
            {
                continue;
            }

            switch (v.cogType)
            {
            case CogType::ai:
                usedAIs.add(v.stringValue);
                break;
            case CogType::key:
                usedKeys.add(v.stringValue);
                break;
            case CogType::mat:
                usedMats.add(v.stringValue);
                break;
            case CogType::model3do:
                used3DOs.add(v.stringValue);
                break;
            case CogType::template_:
                enqueueTpl(v.stringValue);
                break;
            case CogType::wav:
                usedWavs.add(v.stringValue);
                break;
            default:
                break;
            }
        }
    };

    while (!tplQueue.empty() || !cogQueue.empty())
    {
        if (!tplQueue.empty())
        {
            std::string tplName = std::move(tplQueue.back());
            tplQueue.pop_back();
            processTemplate(tplName);
            continue;
        }

        CogWorkItem item = std::move(cogQueue.back());
        cogQueue.pop_back();
        processCog(item);
    }

    if (options.resources != nullptr)
    {
        std::unordered_set<std::string> warnedMissingResources{};

        auto tryReadTextWithExt = [&](const std::string& name, const std::string_view ext) -> std::optional<std::string>
        {
            const std::string trimmed = std::string(trimView(name));
            if (trimmed.empty() || iequalsAscii(trimmed, "none"))
            {
                return std::nullopt;
            }

            const bool hasExt = sed::text::iendsWithAscii(trimmed, ext);
            std::string withExt = trimmed;
            if (!hasExt)
            {
                withExt += std::string(ext);
            }

            if (const auto text = options.resources->tryReadTextWithExt(trimmed, ext))
            {
                return text;
            }

            const std::string& missingName = hasExt ? trimmed : withExt;

            const std::string warnKey = upperAscii(missingName);
            if (warnedMissingResources.emplace(warnKey).second)
            {
                const std::string msg = "JKL save: missing resource '" + missingName + "'; export buffers may be underestimated";
                sed::diagnosticEmit(options.onDiagnostic, sed::DiagnosticSeverity::warning, msg);
            }
            buffersMayBeUnderestimated = true;

            return std::nullopt;
        };

        for (const std::string& pupName : usedPups.items)
        {
            const auto text = tryReadTextWithExt(pupName, ".pup");
            if (!text)
            {
                continue;
            }

            try
            {
                std::istringstream iss(*text);
                const PupFile pup = loadFromPUP(iss);
                for (const std::string& key : pup.keys)
                {
                    usedKeys.add(key);
                }
            }
            catch (const std::exception& e)
            {
                const std::string msg = "JKL save: failed to parse PUP '" + pupName + "': " + e.what();
                sed::diagnosticEmit(options.onDiagnostic, sed::DiagnosticSeverity::warning, msg);
                buffersMayBeUnderestimated = true;
            }
        }

        for (const std::string& sndName : usedSnds.items)
        {
            const auto text = tryReadTextWithExt(sndName, ".snd");
            if (!text)
            {
                continue;
            }

            try
            {
                std::istringstream iss(*text);
                const SndFile snd = loadFromSND(iss);
                for (const std::string& wav : snd.wavs)
                {
                    usedWavs.add(wav);
                }
            }
            catch (const std::exception& e)
            {
                const std::string msg = "JKL save: failed to parse SND '" + sndName + "': " + e.what();
                sed::diagnosticEmit(options.onDiagnostic, sed::DiagnosticSeverity::warning, msg);
                buffersMayBeUnderestimated = true;
            }
        }

        for (const std::string& sprName : usedSprs.items)
        {
            const auto text = tryReadTextWithExt(sprName, ".spr");
            if (!text)
            {
                continue;
            }

            try
            {
                std::istringstream iss(*text);
                const std::string mat = matFromSPR(iss);
                usedMats.add(mat);
            }
            catch (const std::exception& e)
            {
                const std::string msg = "JKL save: failed to parse SPR '" + sprName + "': " + e.what();
                sed::diagnosticEmit(options.onDiagnostic, sed::DiagnosticSeverity::warning, msg);
                buffersMayBeUnderestimated = true;
            }
        }

        for (const std::string& parName : usedPars.items)
        {
            const auto text = tryReadTextWithExt(parName, ".par");
            if (!text)
            {
                continue;
            }

            try
            {
                std::istringstream iss(*text);
                const std::string mat = matFromPAR(iss);
                usedMats.add(mat);
            }
            catch (const std::exception& e)
            {
                const std::string msg = "JKL save: failed to parse PAR '" + parName + "': " + e.what();
                sed::diagnosticEmit(options.onDiagnostic, sed::DiagnosticSeverity::warning, msg);
                buffersMayBeUnderestimated = true;
            }
        }

        for (const std::string& modelName : used3DOs.items)
        {
            const auto text = tryReadTextWithExt(modelName, ".3do");
            if (!text)
            {
                continue;
            }

            try
            {
                std::istringstream iss(*text);
                const Model3do model = loadFrom3DO(iss);
                for (const std::string& mat : model.materials)
                {
                    usedMats.add(mat);
                }
            }
            catch (const std::exception& e)
            {
                const std::string msg = "JKL save: failed to parse 3DO '" + modelName + "': " + e.what();
                sed::diagnosticEmit(options.onDiagnostic, sed::DiagnosticSeverity::warning, msg);
                buffersMayBeUnderestimated = true;
            }
        }
    }

    usedWavs.sort();
    usedMats.sort();
    usedCmps.sort();
    usedAIs.sort();
    used3DOs.sort();
    usedSprs.sort();
    usedKeys.sort();
    usedPups.sort();
    usedSnds.sort();
    usedCogs.sort();

    auto filterStaticList = [&](UsedStrings& list, const std::span<const std::string_view> statics)
    {
        if (statics.empty() || list.items.empty())
        {
            return;
        }

        list.items.erase(
            std::remove_if(list.items.begin(), list.items.end(), [&](const std::string& item)
                {
                    const std::string_view trimmed = trimView(item);
                    for (const std::string_view entry : statics)
                    {
                        if (iequalsAscii(entry, trimmed))
                        {
                            return true;
                        }
                    }
                    return false;
                }),
            list.items.end());
    };

    filterStaticList(usedWavs, staticSounds(level.kind));
    filterStaticList(used3DOs, staticModels(level.kind));
    filterStaticList(usedSprs, staticSprites(level.kind));
    filterStaticList(usedKeys, staticKeys(level.kind));
    filterStaticList(usedCogs, staticCogScripts(level.kind));
    filterStaticList(usedMats, staticMaterials(level.kind));

    const std::size_t requiredMaterialCount = usedMats.items.size();

    std::size_t requiredCogInstances = level.cogs.size();
    for (const ThingTemplate* tpl : usedTemplatesToWrite)
    {
        if (tpl != nullptr)
        {
            requiredCogInstances += countExplicitCogArguments(tpl->values);
        }
    }
    for (const Thing& thing : level.things)
    {
        requiredCogInstances += countExplicitCogArguments(thing.values);
    }
    if (bufferReport != nullptr)
    {
        auto usage = [&](const std::size_t required,
                         const std::uint32_t additional) -> JklExportBufferUsage
        {
            return JklExportBufferUsage{
                .collected = static_cast<std::uint64_t>(required),
                .automaticReserve = 0,
                .additional = additional,
                .total = static_cast<std::uint64_t>(
                    capacityWithExtra(required, additional)),
            };
        };

        bufferReport->sounds = usage(usedWavs.items.size(), level.exportBufferExtras.sounds);
        bufferReport->materials = usage(requiredMaterialCount, level.exportBufferExtras.materials);
        bufferReport->aiClasses = usage(usedAIs.items.size(), level.exportBufferExtras.aiClasses);
        bufferReport->models = usage(used3DOs.items.size(), level.exportBufferExtras.models);
        bufferReport->sprites = usage(usedSprs.items.size(), level.exportBufferExtras.sprites);
        bufferReport->keyframes = usage(usedKeys.items.size(), level.exportBufferExtras.keyframes);
        bufferReport->puppets = usage(usedPups.items.size(), level.exportBufferExtras.puppets);
        bufferReport->soundClasses = usage(usedSnds.items.size(), level.exportBufferExtras.soundClasses);
        bufferReport->cogScripts = usage(usedCogs.items.size(), level.exportBufferExtras.cogScripts);
        bufferReport->cogs = usage(requiredCogInstances, level.exportBufferExtras.cogs);
        bufferReport->templates = usage(usedTemplatesToWrite.size(), level.exportBufferExtras.templates);
        bufferReport->things = usage(level.things.size(), level.exportBufferExtras.things);
        bufferReport->dependencyScanComplete = !buffersMayBeUnderestimated;
    }

    if (reportOnly)
    {
        return;
    }

    if ( isIjim
        && level.exportBufferExtras.materials != 0u
        && level.exportBufferExtras.materials != 64u )
    {
        throw std::runtime_error(
            "Cannot export IJIM NDY: Materials Extra must be 0 or 64. "
            "Open Level Settings > Extra Buffers and edit Materials before exporting.");
    }

    std::vector<std::string> materials = std::move(usedMats.items);

    std::unordered_map<std::string, std::int32_t> materialToIndex;
    materialToIndex.reserve(materials.size());
    for (std::size_t i = 0; i < materials.size(); ++i)
    {
        materialToIndex.emplace(upperAscii(materials[i]), static_cast<std::int32_t>(i));
    }

    std::vector<std::string> colormaps{};
    std::unordered_map<std::string, std::int32_t> colormapToIndex;
    if (!isIjim)
    {
        colormaps = std::move(usedCmps.items);
        colormapToIndex.reserve(colormaps.size());
        for (std::size_t i = 0; i < colormaps.size(); ++i)
        {
            colormapToIndex.emplace(upperAscii(colormaps[i]), static_cast<std::int32_t>(i));
        }
    }

    std::size_t totalVertices = 0;
    std::size_t totalSurfaceVertices = 0;
    std::size_t totalSurfaces = 0;
    for (const Sector& sector : level.sectors)
    {
        totalVertices += sector.vertices.size();
        totalSurfaces += sector.surfaces.size();
        for (const Surface& surf : sector.surfaces)
        {
            totalSurfaceVertices += surf.vertices.size();
        }
    }

    std::vector<WorldVertex> worldVertices{};
    worldVertices.reserve(totalVertices);

    std::unordered_map<ExactVertexKey,
        std::int32_t,
        ExactVertexKeyHash> exactVertexToWorld{};
    exactVertexToWorld.reserve(totalVertices);

    std::unordered_map<WeldCellKey,
        std::vector<std::int32_t>,
        WeldCellKeyHash> weldCellToWorld{};
    if (options.weldNearbyVertices)
    {
        weldCellToWorld.reserve(totalVertices);
    }

    std::vector<WorldTexVertex> worldTexVertices{};
    worldTexVertices.reserve(totalSurfaceVertices);
    std::vector<std::int32_t> worldTexVertexIndex{};
    worldTexVertexIndex.reserve(totalSurfaceVertices);

    std::vector<WorldSurface> worldSurfaces{};
    worldSurfaces.reserve(totalSurfaces);

    std::unordered_map<std::string,
        std::optional<level::UvTextureDimensions>> matSizeCache{};
    matSizeCache.reserve(materials.size());

    auto materialSize = [&](const std::string_view material)
        -> std::optional<level::UvTextureDimensions>
    {
        const std::string_view trimmed = trimView(material);
        if (options.resources == nullptr
            || trimmed.empty()
            || iequalsAscii(trimmed, "none"))
        {
            return std::nullopt;
        }

        const std::string key = upperAscii(trimmed);
        if (const auto it = matSizeCache.find(key); it != matSizeCache.end())
        {
            return it->second;
        }

        std::optional<level::UvTextureDimensions> size =
            resolveMaterialTextureDimensions(*options.resources, trimmed);
        matSizeCache.emplace(key, size);
        return size;
    };

    const auto makeExactVertexKey = [](const WorldVertex& vertex) noexcept
    {
        return ExactVertexKey{ vertex.x, vertex.y, vertex.z };
    };

    const auto makeWeldCellKey = [&](const WorldVertex& vertex) noexcept
    {
        const std::optional<std::int64_t> x = tryJklWeldCellCoordinate(
            vertex.x, options.vertexWeldDistance);
        const std::optional<std::int64_t> y = tryJklWeldCellCoordinate(
            vertex.y, options.vertexWeldDistance);
        const std::optional<std::int64_t> z = tryJklWeldCellCoordinate(
            vertex.z, options.vertexWeldDistance);
        return WeldCellKey{
            *x,
            *y,
            *z,
        };
    };

    const auto offsetWeldCellCoordinate = [](
        const std::int64_t coordinate,
        const int offset) noexcept
        -> std::optional<std::int64_t>
    {
        if (offset < 0
            && coordinate == std::numeric_limits<std::int64_t>::min())
        {
            return std::nullopt;
        }
        if (offset > 0
            && coordinate == std::numeric_limits<std::int64_t>::max())
        {
            return std::nullopt;
        }
        return coordinate + offset;
    };

    auto addVX = [&](const WorldVertex& vertex) -> std::int32_t
    {
        const ExactVertexKey exactKey = makeExactVertexKey(vertex);
        if (const auto exact = exactVertexToWorld.find(exactKey);
            exact != exactVertexToWorld.end())
        {
            return exact->second;
        }

        if (options.weldNearbyVertices)
        {
            const WeldCellKey cell = makeWeldCellKey(vertex);
            std::int32_t bestIndex = -1;
            for (int xOffset = -1; xOffset <= 1; ++xOffset)
            {
                const std::optional<std::int64_t> x = offsetWeldCellCoordinate(cell.x, xOffset);
                if (!x)
                {
                    continue;
                }
                for (int yOffset = -1; yOffset <= 1; ++yOffset)
                {
                    const std::optional<std::int64_t> y = offsetWeldCellCoordinate(cell.y, yOffset);
                    if (!y)
                    {
                        continue;
                    }
                    for (int zOffset = -1; zOffset <= 1; ++zOffset)
                    {
                        const std::optional<std::int64_t> z = offsetWeldCellCoordinate(cell.z, zOffset);
                        if (!z)
                        {
                            continue;
                        }

                        const auto bucket = weldCellToWorld.find(WeldCellKey{ *x, *y, *z });
                        if (bucket == weldCellToWorld.end())
                        {
                            continue;
                        }
                        for (const std::int32_t candidateIndex : bucket->second)
                        {
                            const WorldVertex& candidate = worldVertices[
                                static_cast<std::size_t>(candidateIndex)];
                            const double distance = std::hypot(
                                vertex.x - candidate.x,
                                vertex.y - candidate.y,
                                vertex.z - candidate.z);
                            if (distance <= options.vertexWeldDistance
                                && (bestIndex < 0 || candidateIndex < bestIndex))
                            {
                                bestIndex = candidateIndex;
                            }
                        }
                    }
                }
            }

            if (bestIndex >= 0)
            {
                exactVertexToWorld.emplace(exactKey, bestIndex);
                return bestIndex;
            }
        }

        const std::int32_t newIndex = static_cast<std::int32_t>(worldVertices.size());
        worldVertices.push_back(vertex);
        exactVertexToWorld.emplace(exactKey, newIndex);
        if (options.weldNearbyVertices)
        {
            weldCellToWorld[makeWeldCellKey(vertex)].push_back(newIndex);
        }
        return newIndex;
    };

    auto compareTXVX = [](const WorldTexVertex& v1, const WorldTexVertex& v2) noexcept -> std::int32_t
    {
        if (v1.u < v2.u)
        {
            return -1;
        }
        if (v1.u > v2.u)
        {
            return 1;
        }
        if (v1.v < v2.v)
        {
            return -1;
        }
        if (v1.v > v2.v)
        {
            return 1;
        }
        return 0;
    };

    auto findTXVX = [&](const WorldTexVertex& v, std::size_t& index) noexcept -> bool
    {
        bool found = false;
        std::ptrdiff_t l = 0;
        std::ptrdiff_t h = static_cast<std::ptrdiff_t>(worldTexVertexIndex.size()) - 1;

        while (l <= h)
        {
            const std::ptrdiff_t i = (l + h) >> 1;
            const std::int32_t wi = worldTexVertexIndex[static_cast<std::size_t>(i)];
            const std::int32_t c = compareTXVX(worldTexVertices[static_cast<std::size_t>(wi)], v);
            if (c < 0)
            {
                l = i + 1;
            }
            else
            {
                h = i - 1;
                if (c == 0)
                {
                    found = true;
                    l = i;
                }
            }
        }

        index = static_cast<std::size_t>(l);
        return found;
    };

    auto addTXVX = [&](const WorldTexVertex& v) -> std::int32_t
    {
        std::size_t pos = 0;
        if (findTXVX(v, pos))
        {
            return worldTexVertexIndex[pos];
        }

        const std::int32_t newIndex = static_cast<std::int32_t>(worldTexVertices.size());
        worldTexVertices.push_back(v);
        worldTexVertexIndex.insert(worldTexVertexIndex.begin() + static_cast<std::ptrdiff_t>(pos), newIndex);
        return newIndex;
    };

    std::vector<std::vector<std::int32_t>> sectorVertexToWorld{};
    sectorVertexToWorld.reserve(level.sectors.size());

    std::vector<std::int32_t> sectorSurfaceBase;
    std::vector<std::int32_t> sectorSurfaceCount;
    sectorSurfaceBase.reserve(level.sectors.size());
    sectorSurfaceCount.reserve(level.sectors.size());

    std::vector<std::vector<std::int32_t>> surfaceToWorldIndex;
    surfaceToWorldIndex.reserve(level.sectors.size());

    for (std::size_t secIndex = 0; secIndex < level.sectors.size(); ++secIndex)
    {
        const Sector& sector = level.sectors[secIndex];

        sectorVertexToWorld.emplace_back(sector.vertices.size(), -1);
        std::vector<std::int32_t>& vertexToWorld = sectorVertexToWorld.back();
        for (std::size_t v = 0; v < sector.vertices.size(); ++v)
        {
            const Vertex& vx = sector.vertices[v];
            vertexToWorld[v] = addVX(WorldVertex{ vx.x, vx.y, vx.z });
        }

        const std::int32_t surfBase = static_cast<std::int32_t>(worldSurfaces.size());
        sectorSurfaceBase.push_back(surfBase);
        sectorSurfaceCount.push_back(static_cast<std::int32_t>(sector.surfaces.size()));

        surfaceToWorldIndex.push_back(std::vector<std::int32_t>(sector.surfaces.size(), -1));

        for (std::size_t surfIndex = 0; surfIndex < sector.surfaces.size(); ++surfIndex)
        {
            const Surface& surf = sector.surfaces[surfIndex];

            WorldSurface ws{};
            ws.src    = &surf;
            ws.normal = surf.normal;

            {
                const std::string_view mat = trimView(surf.material);
                if (!mat.empty() && !iequalsAscii(mat, "none"))
                {
                    const std::int32_t staticMatIndex = findStaticMaterialIndex(level.kind, mat);
                    if (staticMatIndex >= 0)
                    {
                        ws.nMat = staticMatIndex | StaticResIdxMask;
                    }
                    else if (const auto itMat = materialToIndex.find(upperAscii(mat)); itMat != materialToIndex.end())
                    {
                        ws.nMat = itMat->second;
                    }
                }
            }

            ws.surfFlags = surf.surfaceFlags;
            ws.faceFlags = surf.faceFlags;
            ws.geo = surf.geo;
            ws.light = surf.light;
            ws.tex = surf.tex;
            ws.extraLight = surf.extraLight;

            const std::size_t nvert = surf.vertices.size();
            ws.vxs.reserve(nvert);
            ws.tvxs.reserve(nvert);
            ws.colors.reserve(nvert);

            for (std::size_t v = 0; v < nvert; ++v)
            {
                const std::int32_t lvx = surf.vertices[v];
                std::int32_t gvx = -1;
                if (lvx >= 0 && static_cast<std::size_t>(lvx) < vertexToWorld.size())
                {
                    gvx = vertexToWorld[static_cast<std::size_t>(lvx)];
                    if (options.weldNearbyVertices && gvx >= 0)
                    {
                        const Vertex& sourceVertex = sector.vertices[
                            static_cast<std::size_t>(lvx)];
                        const WorldVertex& exportedVertex = worldVertices[
                            static_cast<std::size_t>(gvx)];
                        ws.weldChanged = ws.weldChanged
                            || sourceVertex.x != exportedVertex.x
                            || sourceVertex.y != exportedVertex.y
                            || sourceVertex.z != exportedVertex.z;
                    }
                }
                ws.vxs.push_back(gvx);

                const TexVertex tv = surf.texVertices[v];
                TexVertex wireCoordinate = tv;
                if (level.uvCoordinateSpace != wireUvCoordinateSpace)
                {
                    const std::string_view material = trimView(surf.material);
                    if (material.empty() || iequalsAscii(material, "none"))
                    {
                        if (!std::isfinite(tv.u) || !std::isfinite(tv.v))
                        {
                            throw std::runtime_error(
                                "Cannot export JKL/NDY texture coordinates for "
                                "a material-less surface: a UV value is invalid "
                                "or outside the supported numeric range.");
                        }
                    }
                    else
                    {
                        const std::optional<level::UvTextureDimensions> dimensions =
                            materialSize(surf.material);
                        if (!dimensions)
                        {
                            throw std::runtime_error(
                                "Cannot export JKL/NDY texture coordinates for material '"
                                + surf.material
                                + "': its MAT dimensions are unavailable.");
                        }
                        const std::optional<TexVertex> converted =
                            level::convertUvCoordinate(
                                tv,
                                level.uvCoordinateSpace,
                                wireUvCoordinateSpace,
                                *dimensions);
                        if (!converted)
                        {
                            throw std::runtime_error(
                                "Cannot export JKL/NDY texture coordinates for material '"
                                + surf.material
                                + "': a UV value is invalid or outside the supported numeric range.");
                        }
                        wireCoordinate = *converted;
                    }
                }
                const std::int32_t gtvx = addTXVX(WorldTexVertex{
                    wireCoordinate.u, wireCoordinate.v });
                ws.tvxs.push_back(gtvx);
                ws.colors.push_back(tv.color);
            }

            if (options.weldNearbyVertices && ws.weldChanged)
            {
                for (std::size_t first = 0; first < ws.vxs.size(); ++first)
                {
                    const std::int32_t firstLocal = surf.vertices[first];
                    if (firstLocal < 0
                        || static_cast<std::size_t>(firstLocal) >= sector.vertices.size())
                    {
                        continue;
                    }
                    for (std::size_t second = first + 1; second < ws.vxs.size(); ++second)
                    {
                        const std::int32_t secondLocal = surf.vertices[second];
                        if (secondLocal < 0
                            || static_cast<std::size_t>(secondLocal) >= sector.vertices.size()
                            || ws.vxs[first] != ws.vxs[second])
                        {
                            continue;
                        }

                        const Vertex& firstVertex = sector.vertices[
                            static_cast<std::size_t>(firstLocal)];
                        const Vertex& secondVertex = sector.vertices[
                            static_cast<std::size_t>(secondLocal)];
                        if (firstVertex.x != secondVertex.x
                            || firstVertex.y != secondVertex.y
                            || firstVertex.z != secondVertex.z)
                        {
                            throw std::runtime_error(
                                "Cannot export JKL/NDY: nearby vertex welding collapses "
                                "surface at sector "
                                + std::to_string(secIndex)
                                + ", surface "
                                + std::to_string(surfIndex)
                                + ".");
                        }
                    }
                }

            }

            const std::int32_t worldIndex = static_cast<std::int32_t>(worldSurfaces.size());
            surfaceToWorldIndex[secIndex][surfIndex] = worldIndex;
            worldSurfaces.push_back(std::move(ws));
        }
    }

    // Nearby welding changes only the export geometry. Keep a private Sector
    // copy with the final representative coordinates so every derived
    // calculation (and every topology safety check) sees the same geometry
    // that will be written. The authored Level and its Surface records remain
    // untouched.
    std::vector<Sector> mappedSectors{};
    std::vector<bool> sectorWeldChanged{};
    if (options.weldNearbyVertices)
    {
        mappedSectors.reserve(level.sectors.size());
        sectorWeldChanged.reserve(level.sectors.size());
        for (std::size_t secIndex = 0; secIndex < level.sectors.size(); ++secIndex)
        {
            const Sector& sourceSector = level.sectors[secIndex];
            Sector mappedSector = sourceSector;
            const std::vector<std::int32_t>& vertexToWorld =
                sectorVertexToWorld[secIndex];
            bool changed = false;
            for (std::size_t vertexIndex = 0;
                 vertexIndex < vertexToWorld.size();
                 ++vertexIndex)
            {
                const std::int32_t worldIndex = vertexToWorld[vertexIndex];
                if (worldIndex < 0
                    || static_cast<std::size_t>(worldIndex) >= worldVertices.size())
                {
                    continue;
                }
                const WorldVertex& exported = worldVertices[
                    static_cast<std::size_t>(worldIndex)];
                Vertex& destination = mappedSector.vertices[vertexIndex];
                changed = changed
                    || destination.x != exported.x
                    || destination.y != exported.y
                    || destination.z != exported.z;
                destination.x = exported.x;
                destination.y = exported.y;
                destination.z = exported.z;
            }

            for (std::size_t surfaceIndex = 0;
                 surfaceIndex < mappedSector.surfaces.size();
                 ++surfaceIndex)
            {
                const std::int32_t worldIndex = surfaceToWorldIndex[secIndex][
                    surfaceIndex];
                if (worldIndex < 0
                    || static_cast<std::size_t>(worldIndex) >= worldSurfaces.size())
                {
                    continue;
                }
                WorldSurface& worldSurface = worldSurfaces[
                    static_cast<std::size_t>(worldIndex)];
                // WorldSurface::normal is the source normal for unchanged
                // faces. Recalculate changed faces through the shared stable
                // Newell implementation after coordinates have been mapped.
                mappedSector.surfaces[surfaceIndex].normal = worldSurface.normal;
                if (worldSurface.weldChanged)
                {
                    const TVector mappedNormal = sed::level::surfaceCalcNormalNewell(
                        mappedSector,
                        mappedSector.surfaces[surfaceIndex]);
                    const double normalLengthSqr = mappedNormal.lengthSqr();
                    if (!std::isfinite(mappedNormal.x)
                        || !std::isfinite(mappedNormal.y)
                        || !std::isfinite(mappedNormal.z)
                        || !std::isfinite(normalLengthSqr)
                        || normalLengthSqr <= 0.0)
                    {
                        throw std::runtime_error(
                            "Cannot export JKL/NDY: nearby vertex welding produces a "
                            "degenerate surface at sector "
                            + std::to_string(secIndex)
                            + ", surface "
                            + std::to_string(surfaceIndex)
                            + ".");
                    }
                    worldSurface.normal = mappedNormal;
                    mappedSector.surfaces[surfaceIndex].normal = mappedNormal;
                }
            }

            mappedSectors.push_back(std::move(mappedSector));
            sectorWeldChanged.push_back(changed);
        }

        for (std::size_t secIndex = 0; secIndex < level.sectors.size(); ++secIndex)
        {
            if (!sectorWeldChanged[secIndex])
            {
                continue;
            }

            const Sector& sourceSector = level.sectors[secIndex];
            const Sector& mappedSector = mappedSectors[secIndex];
            for (std::size_t surfaceIndex = 0;
                 surfaceIndex < sourceSector.surfaces.size();
                 ++surfaceIndex)
            {
                const std::int32_t worldIndex = surfaceToWorldIndex[secIndex][
                    surfaceIndex];
                if (worldIndex < 0
                    || static_cast<std::size_t>(worldIndex) >= worldSurfaces.size()
                    || !worldSurfaces[static_cast<std::size_t>(worldIndex)].weldChanged)
                {
                    continue;
                }

                const Surface& sourceSurface = sourceSector.surfaces[surfaceIndex];
                const Surface& mappedSurface = mappedSector.surfaces[surfaceIndex];
                // Preserve legacy source geometry that was already outside a
                // strict predicate; reject only a newly invalid mapped face.
                if (sed::level::surfaceIsPlanar(sourceSector, sourceSurface)
                    && !sed::level::surfaceIsPlanar(mappedSector, mappedSurface))
                {
                    throw std::runtime_error(
                        "Cannot export JKL/NDY: nearby vertex welding makes "
                        "surface non-planar at sector "
                        + std::to_string(secIndex)
                        + ", surface "
                        + std::to_string(surfaceIndex)
                        + ".");
                }
                if (sed::level::surfaceIsConvex(sourceSector, sourceSurface)
                    && !sed::level::surfaceIsConvex(mappedSector, mappedSurface))
                {
                    throw std::runtime_error(
                        "Cannot export JKL/NDY: nearby vertex welding makes "
                        "surface concave at sector "
                        + std::to_string(secIndex)
                        + ", surface "
                        + std::to_string(surfaceIndex)
                        + ".");
                }
            }

            // Apply the same baseline comparison at sector scope so old
            // non-convex source sectors remain exportable when welding does
            // not make their validity worse.
            if (sed::level::sectorIsConvex(sourceSector)
                && !sed::level::sectorIsConvex(mappedSector))
            {
                throw std::runtime_error(
                    "Cannot export JKL/NDY: nearby vertex welding makes "
                    "sector non-convex at sector "
                    + std::to_string(secIndex)
                    + ".");
            }
        }
    }

    std::vector<WorldAdjoin> worldAdjoins;
    std::vector<std::int32_t> surfaceToAdjoin(worldSurfaces.size(), -1);
    std::unordered_map<std::uint64_t, std::pair<std::int32_t, std::int32_t>> adjoinPairs;

    auto makePairKey = [](const std::int32_t a, const std::int32_t b) -> std::uint64_t
    {
        const std::uint32_t lo = static_cast<std::uint32_t>(std::min(a, b));
        const std::uint32_t hi = static_cast<std::uint32_t>(std::max(a, b));
        return (static_cast<std::uint64_t>(hi) << 32) | static_cast<std::uint64_t>(lo);
    };

    for (std::size_t secIndex = 0; secIndex < level.sectors.size(); ++secIndex)
    {
        const Sector& sourceSector = level.sectors[secIndex];
        const Sector& geometrySector = options.weldNearbyVertices
            ? mappedSectors[secIndex]
            : sourceSector;
        const TVector sectorCenter = sed::level::sectorCalcCenter(geometrySector);
        for (std::size_t surfIndex = 0; surfIndex < sourceSector.surfaces.size(); ++surfIndex)
        {
            const Surface& sourceSurface = sourceSector.surfaces[surfIndex];
            if (!sourceSurface.adjoin)
            {
                continue;
            }

            const SurfaceRef dst = *sourceSurface.adjoin;
            if (dst.sector < 0 || dst.surface < 0)
            {
                continue;
            }
            if (static_cast<std::size_t>(dst.sector) >= surfaceToWorldIndex.size())
            {
                continue;
            }
            if (static_cast<std::size_t>(dst.surface) >= surfaceToWorldIndex[static_cast<std::size_t>(dst.sector)].size())
            {
                continue;
            }

            const std::int32_t a = surfaceToWorldIndex[secIndex][surfIndex];
            const std::int32_t b = surfaceToWorldIndex[static_cast<std::size_t>(dst.sector)][static_cast<std::size_t>(dst.surface)];
            if (a < 0 || b < 0 || a == b)
            {
                continue;
            }

            const std::uint64_t key = makePairKey(a, b);
            auto it = adjoinPairs.find(key);
            if (it == adjoinPairs.end())
            {
                const std::int32_t idxLo = static_cast<std::int32_t>(worldAdjoins.size());
                const std::int32_t idxHi = idxLo + 1;
                worldAdjoins.push_back(WorldAdjoin{ 0u, idxHi });
                worldAdjoins.push_back(WorldAdjoin{ 0u, idxLo });
                it = adjoinPairs.emplace(key, std::pair<std::int32_t, std::int32_t>{ idxLo, idxHi }).first;
            }

            const std::int32_t loSurf = std::min(a, b);
            const std::int32_t hiSurf = std::max(a, b);
            const auto [idxLo, idxHi] = it->second;

            if (surfaceToAdjoin[static_cast<std::size_t>(loSurf)] != -1 && surfaceToAdjoin[static_cast<std::size_t>(loSurf)] != idxLo)
            {
                throw std::runtime_error("JKL save: surface has multiple adjoins");
            }

            if (surfaceToAdjoin[static_cast<std::size_t>(hiSurf)] != -1 && surfaceToAdjoin[static_cast<std::size_t>(hiSurf)] != idxHi)
            {
                throw std::runtime_error("JKL save: surface has multiple adjoins");
            }

            surfaceToAdjoin[static_cast<std::size_t>(loSurf)] = idxLo;
            surfaceToAdjoin[static_cast<std::size_t>(hiSurf)] = idxHi;

            worldAdjoins[static_cast<std::size_t>(idxLo)].flags =
                static_cast<std::uint32_t>(worldSurfaces[static_cast<std::size_t>(loSurf)].src->adjoinFlags);

            worldAdjoins[static_cast<std::size_t>(idxHi)].flags =
                static_cast<std::uint32_t>(worldSurfaces[static_cast<std::size_t>(hiSurf)].src->adjoinFlags);

            const std::int32_t currentWorldSurf = a;
            const std::int32_t currentAdjIndex  = (currentWorldSurf == loSurf) ? idxLo : idxHi;
            const Surface& geometrySurface = geometrySector.surfaces[surfIndex];
            worldAdjoins[static_cast<std::size_t>(currentAdjIndex)].dist =
                sed::level::surfaceDistToPlaneAbs(
                    geometrySector,
                    geometrySurface,
                    sectorCenter.x,
                    sectorCenter.y,
                    sectorCenter.z);
        }
    }

    for (std::size_t i = 0; i < worldSurfaces.size(); ++i)
    {
        worldSurfaces[i].nAdj = surfaceToAdjoin[i];
    }

    out << "SECTION: HEADER\n";
    {
        ScopedStreamState scoped(out);
        out << std::setprecision(8);

        out << "VERSION " << level.header.version << '\n';
        out << "WORLD GRAVITY " << level.header.gravity << '\n';
        out << "CEILING SKY Z " << level.header.ceilingSky.height << '\n';
        out << "HORIZON DISTANCE " << level.header.horizonSky.distance << '\n';
        out << "HORIZON PIXELS PER REV " << level.header.horizonSky.pixelsPerRev << '\n';
        out << "HORIZON SKY OFFSET " << level.header.horizonSky.offset.x << ' ' << level.header.horizonSky.offset.y << '\n';
        out << "CEILING SKY OFFSET " << level.header.ceilingSky.offset.x << ' ' << level.header.ceilingSky.offset.y << '\n';

        if (!isIjim)
        {
            out << "MIPMAP DISTANCES " << level.header.mipmapDistances[0] << ' ' << level.header.mipmapDistances[1] << ' ' << level.header.mipmapDistances[2]
                << ' ' << level.header.mipmapDistances[3] << '\n';
        }

        out << "LOD DISTANCES " << level.header.lodDistances[0] << ' ' << level.header.lodDistances[1] << ' ' << level.header.lodDistances[2] << ' '
            << level.header.lodDistances[3] << '\n';

        if (isIjim)
        {
            out << "FOG " << (level.header.fog.enabled ? 1 : 0) << ' ' << level.header.fog.color.r << ' ' << level.header.fog.color.g << ' '
                << level.header.fog.color.b << ' ' << level.header.fog.color.a << ' ' << level.header.fog.fogStart << ' ' << level.header.fog.fogEnd << '\n';
        }
        else
        {
            out << "PERSPECTIVE DISTANCE " << level.header.perspectiveDistance << '\n';
            out << "GOURAUD DISTANCE " << level.header.gouraudDistance << '\n';
        }
    }
    out << "END\n\n";

    out << "SECTION: SOUNDS\n";
    out << "WORLD SOUNDS "
        << capacityWithExtra(usedWavs.items.size(), level.exportBufferExtras.sounds) << '\n';
    for (const std::string& wav : usedWavs.items)
    {
        out << wav << '\n';
    }
    out << "END\n\n";

    out << "SECTION: MATERIALS\n";
    const std::size_t matBufSize = capacityWithExtra(
        requiredMaterialCount, level.exportBufferExtras.materials);
    out << "WORLD MATERIALS " << matBufSize << '\n';
    for (std::size_t i = 0; i < materials.size(); ++i)
    {
        out << i << ": " << materials[i];
        if (!isIjim)
        {
            out << ' ' << 1.0 << ' ' << 1.0;
        }
        out << '\n';
    }
    out << "END\n\n";

    out << "SECTION: GEORESOURCE\n";
    // The readable dialect keeps the geometry sub-records visually separate
    // while compact mode removes empty lines in its presentation pass.  Keep
    // this switch independent from the stream wrapper so report-only buffer
    // inspection never changes the serialized text it is measuring.
    bool wroteGeometrySubsection = false;
    const auto beginGeometrySubsection = [&]()
    {
        if (wroteGeometrySubsection && !options.compact && !reportOnly)
        {
            out << '\n';
        }
        wroteGeometrySubsection = true;
    };
    if (!isIjim)
    {
        beginGeometrySubsection();
        out << "WORLD COLORMAPS " << colormaps.size() << '\n';
        for (std::size_t i = 0; i < colormaps.size(); ++i)
        {
            out << i << ": " << colormaps[i] << '\n';
        }
    }

    {
        ScopedStreamState scoped(out);
        out << std::setprecision(8);

        beginGeometrySubsection();
        out << "WORLD VERTICES " << worldVertices.size() << '\n';
        for (std::size_t i = 0; i < worldVertices.size(); ++i)
        {
            const WorldVertex& v = worldVertices[i];
            out << i << ": " << v.x << ' ' << v.y << ' ' << v.z << '\n';
        }

        beginGeometrySubsection();
        out << "WORLD TEXTURE VERTICES " << worldTexVertices.size() << '\n';
        for (std::size_t i = 0; i < worldTexVertices.size(); ++i)
        {
            const WorldTexVertex& v = worldTexVertices[i];
            out << i << ": " << v.u << ' ' << v.v << '\n';
        }

        beginGeometrySubsection();
        out << "WORLD ADJOINS " << worldAdjoins.size() << '\n';
        for (std::size_t i = 0; i < worldAdjoins.size(); ++i)
        {
            const WorldAdjoin& a = worldAdjoins[i];
            out << i << ": ";
            writeHex(out, a.flags);
            out << ' ' << a.mirror << ' ';
            {
                ScopedStreamState scopedDist(out);
                out << std::setprecision(2) << a.dist;
            }
            out << '\n';
        }

        beginGeometrySubsection();
        out << "WORLD SURFACES " << worldSurfaces.size() << '\n';
        for (std::size_t i = 0; i < worldSurfaces.size(); ++i)
        {
            const WorldSurface& s = worldSurfaces[i];

            // Write Surface flags
            out << i << ": " << s.nMat << ' ';
            writeHex(out, static_cast<std::uint32_t>(s.surfFlags));

            // Write Face flags
            out << ' ';
            writeHex(out, s.faceFlags);

            // Write Geo, Light, Tex mode and nAdjoins
            out << ' ' << s.geo << ' ' << s.light << ' ' << (isIjim ? 3 : s.tex )<< ' ' << s.nAdj << ' ';

            // Write Extra light
            validateExtraLightColor(s.extraLight);
            if (level.header.version == 1)
            {
                double intensity = classicExtraLightIntensity(s.extraLight);
                if (options.clampExtraLight)
                {
                    intensity = std::clamp(intensity, -1.0, 1.0);
                }
                out << std::setprecision(6) << intensity << std::setprecision(8);
            }
            else
            {
                const float red = options.clampExtraLight
                    ? clampSignedUnit(s.extraLight.r)
                    : s.extraLight.r;
                const float green = options.clampExtraLight
                    ? clampSignedUnit(s.extraLight.g)
                    : s.extraLight.g;
                const float blue = options.clampExtraLight
                    ? clampSignedUnit(s.extraLight.b)
                    : s.extraLight.b;
                out << red << ' '
                    << green << ' '
                    << blue << ' '
                    << clamp01(s.extraLight.a);
            }

            // Write vertex and texture vertex indices
            out << ' ' << s.vxs.size();
            for (std::size_t v = 0; v < s.vxs.size(); ++v)
            {
                out << ' ' << s.vxs[v] << ',' << s.tvxs[v];
            }

            if (level.header.version == 1 && isMots)
            {
                // For Mots, the color field is used for vertex lighting, so write alpha as intensity.
                out << std::setprecision(4);
                for (const ColorF& c : s.colors)
                {
                    out << ' ' << c.a << ' ' << c.r << ' ' << c.g << ' ' << c.b;
                }
                out << std::setprecision(8);
            }
            else if (level.header.version != 1)
            {
                // For Ijim, the color field is used for vertex lighting, so write RGB instead of alpha.
                for (const ColorF& c : s.colors)
                {
                    out << ' ' << c.r << ' ' << c.g << ' ' << c.b;
                }
            }
            else
            {
                // For JKDF, the color field is used for vertex lighting, so write intensity.
                out << std::setprecision(4);
                for (const ColorF& c : s.colors)
                {
                    out << ' ' << c.a;
                }
                out << std::setprecision(8);
            }

            out << '\n';
        }

        for (std::size_t i = 0; i < worldSurfaces.size(); ++i)
        {
            const TVector n = worldSurfaces[i].normal;
            out << i << ": " << n.x << ' ' << n.y << ' ' << n.z << '\n';
        }
    }

    out << "END\n\n";

    out << "Section: SECTORS\n";
    out << "WORLD SECTORS " << level.sectors.size() << '\n';

    for (std::size_t secIndex = 0; secIndex < level.sectors.size(); ++secIndex)
    {
        const Sector& sector = level.sectors[secIndex];
        const Sector& geometrySector = options.weldNearbyVertices
            ? mappedSectors[secIndex]
            : sector;

        out << "SECTOR " << secIndex << '\n';

        const TBox bbox = sed::level::sectorCalcBBox(geometrySector);
        const TVector secCenter = sed::level::sectorCalcCenter(geometrySector);
        TBox collideBox{};
        const bool hasExplicitCollideBox =
            (sector.flags & SECF_CollideBox) != 0
            && !sector.collideBox.IsZero();
        const bool hasCalculatedCollideBox =
            sed::level::sectorFindCollideBox(
                geometrySector,
                bbox,
                secCenter.x,
                secCenter.y,
                secCenter.z,
                collideBox);
        if (hasExplicitCollideBox)
        {
            collideBox = sector.collideBox;
        }
        const bool hasCollideBox = hasExplicitCollideBox || hasCalculatedCollideBox;

        std::int32_t secFlags = sector.flags;
        secFlags &= ~SECF_3DO;
        if (hasCollideBox)
        {
            secFlags |= SECF_CollideBox;
        }
        else
        {
            secFlags &= ~SECF_CollideBox;
        }

        out << "FLAGS ";
        writeHex(out, static_cast<std::uint32_t>(secFlags));
        out << '\n';

            {
                ScopedStreamState scoped(out);
                out << std::setprecision(8);

            validateSectorLightColor(sector.ambient);
            validateSectorLightColor(sector.extraLight);
            if (!isIjim)
            {
                out << "AMBIENT LIGHT " << classicSectorLightIntensity(sector.ambient) << '\n';
                out << "EXTRA LIGHT " << classicSectorLightIntensity(sector.extraLight) << '\n';
                const auto itCmp = colormapToIndex.find(upperAscii(sector.colormap));
                const std::int32_t cmpIndex = (itCmp != colormapToIndex.end()) ? itCmp->second : -1;
                out << "COLORMAP " << cmpIndex << '\n';
                out << "TINT " << sector.tint.r << ' ' << sector.tint.g << ' ' << sector.tint.b << '\n';
            }
            else
            {
                out << "AMBIENT LIGHT " << sector.ambient.r << ' ' << sector.ambient.g << ' ' << sector.ambient.b << '\n';
                out << "EXTRA LIGHT " << sector.extraLight.r << ' ' << sector.extraLight.g << ' ' << sector.extraLight.b << '\n';

                // IJIM has no meaningful tint alpha on the wire. Test the RGB
                // channels exactly so authored values below float epsilon are
                // not silently dropped by ColorF::isZero().
                if (sector.tint.r != 0.0f
                    || sector.tint.g != 0.0f
                    || sector.tint.b != 0.0f)
                {
                    out << "TINT " << sector.tint.r << ' ' << sector.tint.g << ' ' << sector.tint.b << '\n';
                }
                if (pointLightHasAuthoredParameters(sector.pointLight))
                {
                    // NDY stores the three AVERAGE LIGHT records independently.
                    // Preserve only authored components so a valid light at the
                    // origin does not grow a synthetic POSITION record.
                    if (!sector.pointLight.color.isRGBZero())
                    {
                        out << "AVERAGE LIGHT INTENSITY " << sector.pointLight.color.r << ' ' << sector.pointLight.color.g << ' ' << sector.pointLight.color.b << '\n';
                    }
                    if (!sector.pointLight.position.IsZero())
                    {
                        out << "AVERAGE LIGHT POSITION " << sector.pointLight.position.x << ' ' << sector.pointLight.position.y << ' ' << sector.pointLight.position.z
                            << '\n';
                    }
                    if (!sed::IsZero(sector.pointLight.minRange) || !sed::IsZero(sector.pointLight.maxRange))
                    {
                        out << "AVERAGE LIGHT FALLOFF " << sector.pointLight.minRange << ' ' << sector.pointLight.maxRange << '\n';
                    }
                }
            }

            out << "BOUNDBOX " << bbox.x1 << ' ' << bbox.y1 << ' ' << bbox.z1 << ' ' << bbox.x2 << ' ' << bbox.y2 << ' ' << bbox.z2 << '\n';

            if (hasCollideBox)
            {
                out << "COLLIDEBOX " << collideBox.x1 << ' ' << collideBox.y1 << ' ' << collideBox.z1 << ' ' << collideBox.x2
                    << ' ' << collideBox.y2 << ' ' << collideBox.z2 << '\n';
            }

            if (!sector.sound.empty())
            {
                out << "SOUND " << sector.sound << ' ' << sector.soundVolume << '\n';
            }

            if (isIjim && !sector.thrust.IsZero())
            {
                out << "THRUST " << sector.thrust.x << ' ' << sector.thrust.y << ' ' << sector.thrust.z << '\n';
            }

            const sed::level::BoundingSphere bounds =
                sed::level::sectorCalcBoundingSphere(geometrySector);
            out << "CENTER " << bounds.center.x << ' ' << bounds.center.y << ' ' << bounds.center.z << '\n';
            out << "RADIUS " << bounds.radius << '\n';
        }

        const std::vector<std::int32_t>& vertexToWorld = sectorVertexToWorld[secIndex];
        out << "VERTICES " << vertexToWorld.size() << '\n';
        for (std::size_t v = 0; v < vertexToWorld.size(); ++v)
        {
            out << v << ": " << vertexToWorld[v] << '\n';
        }

        out << "SURFACES " << sectorSurfaceBase[secIndex] << ' ' << sectorSurfaceCount[secIndex] << '\n';
    }

    out << "END\n\n";

    auto writeListSection = [&](const std::string_view name,
                                const std::string_view countName,
                                const UsedStrings& list,
                                const std::uint32_t extra)
    {
        out << name << '\n';
        out << countName << ' '
            << capacityWithExtra(list.items.size(), extra) << '\n';
        for (std::size_t i = 0; i < list.items.size(); ++i)
        {
            out << i << ": " << list.items[i] << '\n';
        }
        out << "END\n\n";
    };

    writeListSection("Section: AICLASS", "World AIClasses", usedAIs,
        level.exportBufferExtras.aiClasses);
    writeListSection("Section: MODELS", "World models", used3DOs,
        level.exportBufferExtras.models);
    writeListSection("Section: SPRITES", "World sprites", usedSprs,
        level.exportBufferExtras.sprites);
    writeListSection("Section: KEYFRAMES", "World keyframes", usedKeys,
        level.exportBufferExtras.keyframes);
    writeListSection("Section: ANIMCLASS", "World puppets", usedPups,
        level.exportBufferExtras.puppets);
    writeListSection("Section: Soundclass", "World soundclasses", usedSnds,
        level.exportBufferExtras.soundClasses);
    writeListSection("Section: cogscripts", "World scripts", usedCogs,
        level.exportBufferExtras.cogScripts);

    out << "Section: cogs\n";
    out << "World cogs\t"
        << capacityWithExtra(requiredCogInstances, level.exportBufferExtras.cogs) << '\n';
    // This is a parser-only column label; no runtime data is introduced by
    // hash-comment lines in the JKL/NDY writer.  Compact mode omits it.
    out << "#Num\tScript\tSymbol values\n";
    for (std::size_t i = 0; i < level.cogs.size(); ++i)
    {
        const Cog& cog = level.cogs[i];
        out << i << ":\t" << cog.name;
        for (std::size_t valueIndex = 0; valueIndex < cog.values.size(); ++valueIndex)
        {
            if (!isJklPlacedCogValue(cog.values[valueIndex]))
            {
                continue;
            }
            out << '\t' << serializeJklCogValue(
                cog, cog.values[valueIndex], valueIndex, level);
        }
        out << '\n';
    }
    out << "END\n\n";

    std::vector<const ThingTemplate*> templatesToWrite = usedTemplatesToWrite;
    std::sort(templatesToWrite.begin(), templatesToWrite.end(), [](const ThingTemplate* a, const ThingTemplate* b)
        {
            if (a == nullptr || b == nullptr)
            {
                return a != nullptr;
            }
            return upperAscii(trimView(a->name)) < upperAscii(trimView(b->name));
        });

    out << "Section: TEMPLATES\n";
    out << "World templates "
        << capacityWithExtra(usedTemplatesToWrite.size(), level.exportBufferExtras.templates) << '\n';
    // This is likewise documentation for the legacy editor, not a field.
    out << "#Name:\tBased On:\tParams:\n";

    auto padRight = [](const std::string_view s, const std::size_t width) -> std::string
    {
        std::string out(s);
        if (out.size() < width)
        {
            out.append(width - out.size(), ' ');
        }
        return out;
    };

    std::unordered_map<std::string, std::size_t> templateToIndex{};
    templateToIndex.reserve(templatesToWrite.size());
    for (std::size_t i = 0; i < templatesToWrite.size(); ++i)
    {
        const ThingTemplate* tplPtr = templatesToWrite[i];
        if (tplPtr == nullptr)
        {
            continue;
        }

        const std::string key = upperAscii(trimView(tplPtr->name));
        if (!key.empty() && !templateToIndex.contains(key))
        {
            templateToIndex.emplace(key, i);
        }
    }

    std::vector<std::vector<std::size_t>> deps(templatesToWrite.size());

    auto addDep = [&](const std::size_t tplIndex, const std::string_view depName)
    {
        const std::string key = upperAscii(trimView(depName));
        const auto it = templateToIndex.find(key);
        if (it == templateToIndex.end())
        {
            return;
        }
        deps[tplIndex].push_back(it->second);
    };

    for (std::size_t i = 0; i < templatesToWrite.size(); ++i)
    {
        const ThingTemplate* tplPtr = templatesToWrite[i];
        if (tplPtr == nullptr)
        {
            continue;
        }

        const ThingTemplate& tpl = *tplPtr;

        if (!tpl.parent.empty() && !iequalsAscii(tpl.parent, "none"))
        {
            addDep(i, tpl.parent);
        }

        for (const TplValue& v : tpl.values)
        {
            if (v.adjType != AdjType::tpl || v.stringValue.empty() || iequalsAscii(v.stringValue, "none"))
            {
                continue;
            }
            addDep(i, v.stringValue);
        }

        std::sort(deps[i].begin(), deps[i].end());
        deps[i].erase(std::unique(deps[i].begin(), deps[i].end()), deps[i].end());
    }

    std::vector<std::size_t> order{};
    order.reserve(templatesToWrite.size());

    std::vector<std::uint8_t> state(templatesToWrite.size(), 0); // 0=unvisited, 1=visiting, 2=done

    auto visit = [&](const auto& self, const std::size_t idx) -> void
    {
        if (idx >= state.size())
        {
            return;
        }
        if (state[idx] == 2)
        {
            return;
        }
        if (state[idx] == 1)
        {
            return;
        }

        state[idx] = 1;
        for (const std::size_t dep : deps[idx])
        {
            self(self, dep);
        }
        state[idx] = 2;
        order.push_back(idx);
    };

    for (std::size_t i = 0; i < templatesToWrite.size(); ++i)
    {
        visit(visit, i);
    }

    for (const std::size_t idx : order)
    {
        const ThingTemplate* tplPtr = templatesToWrite[idx];
        if (tplPtr == nullptr)
        {
            continue;
        }

        const ThingTemplate& tpl = *tplPtr;
        const std::string parent = tpl.parent.empty() ? "none" : tpl.parent;
        out << padRight(tpl.name, 17) << ' ' << padRight(parent, 17);
        for (const TplValue& v : tpl.values)
        {
            out << ' ' << v.name << '='
                << (isIjim ? serializeIjimNdyTplValue(v) : serializeTplValue(v));
        }
        out << '\n';
    }
    out << "END\n\n";

    out << "Section: Things\n";
    out << "World things "
        << capacityWithExtra(level.things.size(), level.exportBufferExtras.things) << '\n';
    {
        ScopedStreamState scoped(out);
        out << std::setprecision(8);

        for (std::size_t i = 0; i < level.things.size(); ++i)
        {
            const Thing& thing = level.things[i];

            const std::string_view tplName = !thing.templateName.empty() ? std::string_view(thing.templateName) : std::string_view(thing.name);
            const std::string_view instName = !thing.name.empty() ? std::string_view(thing.name) : tplName;

            out << i << ": " << tplName << ' ' << instName << ' ' << thing.x << ' ' << thing.y << ' ' << thing.z << ' ' << thing.pch << ' ' << thing.yaw
                << ' ' << thing.rol << ' ' << thing.sector;

            if (isMots)
            {
                out << ' ' << archLighting.thingIndices[i];
            }

            for (const TplValue& v : thing.values)
            {
                out << ' ' << v.name << '='
                    << (isIjim ? serializeIjimNdyTplValue(v) : serializeTplValue(v));
            }

            out << '\n';
        }
    }
    out << "END\n\n";

    if (isMots)
    {
        out << "Section: ARCHLIGHTING\n";
        out << "Num ArchObjects " << archLighting.objects.size() << '\n';
        {
            ScopedStreamState scoped(out);
            out << std::setprecision(8);
            for (const MotsArchLightObject& object : archLighting.objects)
            {
                out << "archobject " << object.wireIndex << '\n';
                out << "nummeshes: " << object.meshes.size() << '\n';
                for (const MotsArchLightMesh& mesh : object.meshes)
                {
                    out << "mesh " << mesh.wireIndex << '\n';
                    out << "numvertices: " << mesh.vertices.size() << '\n';
                    for (std::size_t vertexIndex = 0;
                         vertexIndex < mesh.vertices.size();
                         ++vertexIndex)
                    {
                        const MotsArchLightVertex& vertex = mesh.vertices[vertexIndex];
                        out << vertexIndex << ": "
                            << vertex.mono << ' ' << vertex.red << ' '
                            << vertex.green << ' ' << vertex.blue << '\n';
                    }
                }
            }
        }
        out << "END\n\n";
    }

    if (options.includeLights)
    {
        out << "Section: Lights\n";
        out << "EDITOR LIGHTS " << level.lights.size() << '\n';

        {
            ScopedStreamState scoped(out);
            out << std::setprecision(8);

            for (std::size_t i = 0; i < level.lights.size(); ++i)
            {
                const SedLight& light = level.lights[i];

                out << i << ": ";
                writeHex(out, static_cast<std::uint32_t>(light.flags));
                out << ' ' << light.layer << ' ' << light.position.x << ' ' << light.position.y << ' ' << light.position.z << ' ' << light.range << ' '
                    << light.intensity << ' ' << light.color.r << ' ' << light.color.g << ' ' << light.color.b << '\n';
            }
        }

        out << "END\n\n";
    }

    if (options.includeLayers)
    {
        out << "Section: Layers\n";
        out << "EDITOR LAYERS " << level.layers.size() << '\n';

        for (std::size_t layerIndex = 0; layerIndex < level.layers.size(); ++layerIndex)
        {
            out << level.layers[layerIndex] << '\n';

            std::vector<std::int32_t> secIndices;
            for (std::size_t i = 0; i < level.sectors.size(); ++i)
            {
                if (level.sectors[i].layer == static_cast<std::int32_t>(layerIndex))
                {
                    secIndices.push_back(static_cast<std::int32_t>(i));
                }
            }

            out << secIndices.size() << ':';
            for (const std::int32_t idx : secIndices)
            {
                out << ' ' << idx;
            }
            out << '\n';

            std::vector<std::int32_t> thingIndices;
            for (std::size_t i = 0; i < level.things.size(); ++i)
            {
                if (level.things[i].layer == static_cast<std::int32_t>(layerIndex))
                {
                    thingIndices.push_back(static_cast<std::int32_t>(i));
                }
            }

            out << thingIndices.size() << ':';
            for (const std::int32_t idx : thingIndices)
            {
                out << ' ' << idx;
            }
            out << '\n';
        }

        if (!level.lvisstring.empty())
        {
            out << level.lvisstring << '\n';
        }
        out << "END\n";
    }

    compactOutput.finish();
}

JklExportBufferReport inspectJklExportBuffers(
    const Level& level,
    const JklSaveOptions& options)
{
    JklExportBufferReport report{};
    std::ostringstream sink{};
    saveToJklImpl(level, sink, options, &report, true);
    return report;
}

static std::string serializeJklText(
    const Level& level,
    const JklSaveOptions& options)
{
    std::ostringstream staged{};
    saveToJklImpl(level, staged, options, nullptr, false);
    return staged.str();
}

void saveToJKL(const Level& level, std::ostream& out, const JklSaveOptions& options)
{
    const std::string text = serializeJklText(level, options);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out)
    {
        throw std::runtime_error("Failed to write JKL/NDY output.");
    }
}

void saveToJKL(const Level& level, const std::filesystem::path& path)
{
    saveToJKL(level, path, {});
}

void saveToJKL(const Level& level, const std::filesystem::path& path, const JklSaveOptions& options)
{
    const std::string text = serializeJklText(level, options);
    detail::StagedAssetFile staged(path);
    staged.writeAndClose(std::as_bytes(std::span(text.data(), text.size())));
    staged.publish(path);
}

} // namespace sed::assets
