#include "sed/values.hpp"

#include "sed/parse_frame.hpp"
#include "sed/level.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace sed
{

namespace
{

enum class TplTupleLayout
{
    slash,
    frame,
};

std::optional<std::int32_t> parseTplFlag(const std::string_view textValue) noexcept
{
    std::string_view token = text::trimView(textValue);
    bool negative = false;
    if (!token.empty() && (token.front() == '+' || token.front() == '-'))
    {
        negative = token.front() == '-';
        token.remove_prefix(1);
    }

    const std::optional<std::uint32_t> magnitude = text::parseHex(token);
    if (!magnitude)
    {
        return std::nullopt;
    }

    // The original engines read Thing/template flags with scanf's `%x`, where
    // bare digits are hexadecimal and an optional sign is applied modulo 2^32.
    const std::uint32_t bits = negative ? std::uint32_t{ 0 } - *magnitude : *magnitude;
    return static_cast<std::int32_t>(bits);
}

bool isSignedDecimalInteger(const std::string_view token) noexcept
{
    if (token.empty())
    {
        return false;
    }

    std::string_view parsedToken = token;
    if (parsedToken.front() == '+')
    {
        parsedToken.remove_prefix(1);
        if (parsedToken.empty())
        {
            return false;
        }
    }
    return text::parseInt<std::int32_t>(parsedToken, 10).has_value();
}

bool isThreeComponentTplTuple(const std::string_view name) noexcept
{
    return text::iequalsAscii(name, "angvel")
        || text::iequalsAscii(name, "eyeoffset")
        || text::iequalsAscii(name, "fireoffset")
        || text::iequalsAscii(name, "flashrgb")
        || text::iequalsAscii(name, "lightoffset")
        || text::iequalsAscii(name, "orient")
        || text::iequalsAscii(name, "spriteend")
        || text::iequalsAscii(name, "spritestart")
        || text::iequalsAscii(name, "vel");
}

std::optional<TplTupleLayout> tplTupleLayout(
    const TplValue& value,
    const std::size_t componentCount) noexcept
{
    if (value.type == ValueType::frame || text::iequalsAscii(value.name, "frame"))
    {
        return (componentCount == 3 || componentCount == 6)
            ? std::optional<TplTupleLayout>{ TplTupleLayout::frame }
            : std::nullopt;
    }

    if (value.type == ValueType::vect)
    {
        return componentCount >= 2
            ? std::optional<TplTupleLayout>{ TplTupleLayout::slash }
            : std::nullopt;
    }

    if (isThreeComponentTplTuple(value.name))
    {
        return componentCount == 3
            ? std::optional<TplTupleLayout>{ TplTupleLayout::slash }
            : std::nullopt;
    }
    if (text::iequalsAscii(value.name, "light"))
    {
        // JK/MotS use a scalar. IJIM uses RGB with an optional fourth range.
        return (componentCount == 3 || componentCount == 4)
            ? std::optional<TplTupleLayout>{ TplTupleLayout::slash }
            : std::nullopt;
    }
    if (text::iequalsAscii(value.name, "lightintensity"))
    {
        // JK/MotS use a scalar. IJIM uses RGB with an optional fourth range.
        return (componentCount == 3 || componentCount == 4)
            ? std::optional<TplTupleLayout>{ TplTupleLayout::slash }
            : std::nullopt;
    }
    if (text::iequalsAscii(value.name, "voicecolor"))
    {
        return componentCount == 16
            ? std::optional<TplTupleLayout>{ TplTupleLayout::slash }
            : std::nullopt;
    }
    return std::nullopt;
}

std::string canonicalTplTupleText(const TplValue& value, const std::string_view textValue)
{
    const std::string_view trimmed = text::trimView(textValue);
    if (trimmed.empty())
    {
        return {};
    }

    const bool opens = trimmed.front() == '(';
    const bool closes = trimmed.back() == ')';
    if (opens != closes)
    {
        return std::string(trimmed);
    }

    std::string_view body = trimmed;
    if (opens)
    {
        body.remove_prefix(1);
        body.remove_suffix(1);
    }

    // A scalar is intentionally not a tuple. This keeps classic JK/MotS
    // `light` and `lightintensity` values byte-compatible.
    const bool hasTupleSeparator = body.find_first_of("/:,; \t\r\n") != std::string_view::npos;
    if (!hasTupleSeparator)
    {
        return std::string(trimmed);
    }

    std::string compact{};
    compact.reserve(body.size());
    for (const char ch : body)
    {
        if (std::isspace(static_cast<unsigned char>(ch)) == 0)
        {
            compact.push_back(ch);
        }
    }
    const auto isExplicitSeparator = [](const char ch) noexcept
    {
        return ch == '/' || ch == ':' || ch == ',' || ch == ';';
    };
    for (std::size_t i = 0; i < compact.size(); ++i)
    {
        if (!isExplicitSeparator(compact[i]))
        {
            continue;
        }
        if (i == 0 || i + 1 == compact.size()
            || isExplicitSeparator(compact[i - 1])
            || isExplicitSeparator(compact[i + 1]))
        {
            return std::string(trimmed);
        }
    }

    std::string normalized{};
    normalized.reserve(body.size());
    for (const char ch : body)
    {
        normalized.push_back(
            ch == '/' || ch == ':' || ch == ',' || ch == ';'
                || std::isspace(static_cast<unsigned char>(ch)) != 0
            ? ' '
            : ch);
    }

    const std::vector<std::string_view> parts = text::splitWords(normalized);
    const std::optional<TplTupleLayout> layout = tplTupleLayout(value, parts.size());
    if (!layout)
    {
        return std::string(trimmed);
    }

    const bool integerComponents = text::iequalsAscii(value.name, "flashrgb");
    for (const std::string_view part : parts)
    {
        if (integerComponents)
        {
            if (!isSignedDecimalInteger(part))
            {
                return std::string(trimmed);
            }
            continue;
        }

        const std::optional<double> parsed = text::parseDouble(part);
        if (!parsed || !std::isfinite(*parsed))
        {
            return std::string(trimmed);
        }
    }

    std::string out{"("};
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        if (i > 0)
        {
            out.push_back(*layout == TplTupleLayout::frame && i == 3 ? ':' : '/');
        }
        out.append(parts[i]);
    }
    out.push_back(')');
    return out;
}

std::vector<std::string_view> canonicalSlashTupleParts(const std::string_view textValue)
{
    if (textValue.size() < 3 || textValue.front() != '(' || textValue.back() != ')')
    {
        return {};
    }

    std::string_view body = textValue.substr(1, textValue.size() - 2);
    std::vector<std::string_view> parts{};
    while (!body.empty())
    {
        const std::size_t separator = body.find('/');
        const std::string_view part = body.substr(0, separator);
        if (part.empty())
        {
            return {};
        }
        const std::optional<double> parsed = text::parseDouble(part);
        if (!parsed || !std::isfinite(*parsed))
        {
            return {};
        }
        parts.push_back(part);
        if (separator == std::string_view::npos)
        {
            body = {};
        }
        else
        {
            body.remove_prefix(separator + 1);
            if (body.empty())
            {
                return {};
            }
        }
    }
    return parts;
}

std::string formatCanonicalSlashTuple(const std::vector<std::string_view>& parts, const std::size_t count)
{
    std::string out{"("};
    for (std::size_t i = 0; i < count && i < parts.size(); ++i)
    {
        if (i > 0)
        {
            out.push_back('/');
        }
        out.append(parts[i]);
    }
    out.push_back(')');
    return out;
}

} // namespace

std::string Value::asString() const
{
    switch (type)
    {
    case ValueType::str:
    case ValueType::unknown:
    case ValueType::vect:
    case ValueType::frame:
        return stringValue;
    case ValueType::integer:
    case ValueType::ptr:
        return std::to_string(intValue);
    case ValueType::flag:
    {
        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        oss << "0x" << std::hex << std::nouppercase << static_cast<std::uint32_t>(intValue);
        return oss.str();
    }
    case ValueType::floating:
    {
        return sed::format::authoredDouble(floatValue);
    }
    default:
        return {};
    }
}

bool Value::parse(const std::string_view text)
{
    switch (type)
    {
    case ValueType::str:
    case ValueType::unknown:
    case ValueType::vect:
        stringValue = std::string(text);
        return true;
    case ValueType::frame:
    {
        const std::string_view trimmed = sed::text::trimView(text);
        if (trimmed.empty())
        {
            stringValue.clear();
            return true;
        }

        const std::optional<sed::FrameValue> parsed = sed::parse::frameValue(trimmed);
        if (!parsed)
        {
            return false;
        }

        const bool hasWhitespace = std::any_of(trimmed.begin(), trimmed.end(), [](const char ch)
            {
                return std::isspace(static_cast<unsigned char>(ch)) != 0;
            });
        const bool hasParens = trimmed.size() >= 2 && trimmed.front() == '(' && trimmed.back() == ')';

        // Preserve canonical JKL tokens as-is to avoid changing formatting on load.
        // If the user enters a space-separated frame (or omits parentheses), rewrite to a safe single-token format.
        stringValue = (hasWhitespace || !hasParens) ? sed::format::frameValue(*parsed) : std::string(trimmed);
        return true;
    }
    case ValueType::integer:
    case ValueType::ptr:
    {
        if (const auto parsed = sed::text::parseInt<std::int32_t>(text))
        {
            intValue = *parsed;
            return true;
        }

        if (const auto parsedHex = sed::text::parseHex(text))
        {
            intValue = static_cast<std::int32_t>(*parsedHex);
            return true;
        }

        return false;
    }
    case ValueType::flag:
    {
        if (const std::optional<std::int32_t> parsed = parseTplFlag(text))
        {
            intValue = *parsed;
            return true;
        }
        return false;
    }
    case ValueType::floating:
    {
        const auto parsed = sed::text::parseDouble(text);
        if (!parsed)
        {
            return false;
        }
        floatValue = *parsed;
        return true;
    }
    default:
        return false;
    }
}

std::string CogValue::asJedString() const
{
    return asString();
}

std::string CogValue::asJedString(const Level& level) const
{
    if (cogType == CogType::sec)
    {
        if (type != ValueType::ptr)
        {
            return asString();
        }

        if (intValue < 0)
        {
            return "-1";
        }

        const std::size_t sectorIndex = static_cast<std::size_t>(intValue);
        if (sectorIndex >= level.sectors.size())
        {
            return "-1";
        }

        return std::to_string(level.sectors[sectorIndex].num);
    }

    if (cogType == CogType::thing)
    {
        if (type != ValueType::ptr)
        {
            return asString();
        }

        if (intValue < 0)
        {
            return "-1";
        }

        const std::size_t thingIndex = static_cast<std::size_t>(intValue);
        if (thingIndex >= level.things.size())
        {
            return "-1";
        }

        return std::to_string(level.things[thingIndex].num);
    }

    if (cogType != CogType::surface)
    {
        return asString();
    }

    if (type != ValueType::ptr)
    {
        return asString();
    }

    auto tryFormatSurfaceFromText = [&]() -> std::optional<std::string>
    {
        const std::string_view raw = sed::text::trimView(stringValue);
        if (raw.empty())
        {
            return std::nullopt;
        }

        CogValue parsed = *this;
        parsed.intValue = -1;
        if (!parsed.parseJed(level, raw))
        {
            return std::nullopt;
        }

        if (parsed.intValue < 0)
        {
            return std::string("-1");
        }

        const std::optional<SurfaceRef> parsedRef = level.tryGetSurfaceRefByGlobalSurfaceNumber(parsed.intValue);
        if (!parsedRef)
        {
            return std::string("-1");
        }

        const std::size_t parsedSectorIndex = static_cast<std::size_t>(parsedRef->sector);
        const std::size_t parsedSurfaceIndex = static_cast<std::size_t>(parsedRef->surface);
        if (parsedSectorIndex >= level.sectors.size())
        {
            return std::string("-1");
        }

        const Sector& parsedSector = level.sectors[parsedSectorIndex];
        if (parsedSurfaceIndex >= parsedSector.surfaces.size())
        {
            return std::string("-1");
        }

        const Surface& parsedSurface = parsedSector.surfaces[parsedSurfaceIndex];
        return std::to_string(parsedSector.num) + " " + std::to_string(parsedSurface.num);
    };

    if (stringValue.find_first_of(" \t\r\n") != std::string::npos)
    {
        if (const std::optional<std::string> formatted = tryFormatSurfaceFromText())
        {
            return *formatted;
        }
    }

    if (intValue < 0)
    {
        if (const std::optional<std::string> formatted = tryFormatSurfaceFromText())
        {
            return *formatted;
        }
        return "-1";
    }

    const std::optional<SurfaceRef> ref = level.tryGetSurfaceRefByGlobalSurfaceNumber(intValue);
    if (!ref)
    {
        if (const std::optional<std::string> formatted = tryFormatSurfaceFromText())
        {
            return *formatted;
        }
        return "-1";
    }

    const std::size_t sectorIndex = static_cast<std::size_t>(ref->sector);
    const std::size_t surfaceIndex = static_cast<std::size_t>(ref->surface);
    if (sectorIndex >= level.sectors.size())
    {
        return "-1";
    }

    const Sector& sector = level.sectors[sectorIndex];
    if (surfaceIndex >= sector.surfaces.size())
    {
        return "-1";
    }

    const Surface& surface = sector.surfaces[surfaceIndex];
    return std::to_string(sector.num) + " " + std::to_string(surface.num);
}

bool CogValue::parseJed(const std::string_view text)
{
    return parse(text);
}

bool CogValue::parseJed(const Level& level, const std::string_view text)
{
    switch (cogType)
    {
    case CogType::sec:
    {
        const auto parsed = sed::text::parseInt<std::int32_t>(text);
        if (!parsed)
        {
            return false;
        }

        const std::int32_t sectorNumber = *parsed;
        if (sectorNumber < 0)
        {
            intValue = -1;
            return true;
        }

        if (const std::optional<std::int32_t> sectorIndex = level.tryGetSectorIndexByNum(sectorNumber))
        {
            intValue = *sectorIndex;
            return true;
        }

        if (static_cast<std::size_t>(sectorNumber) >= level.sectors.size())
        {
            return false;
        }

        intValue = sectorNumber;
        return true;
    }
    case CogType::thing:
    {
        const auto parsed = sed::text::parseInt<std::int32_t>(text);
        if (!parsed)
        {
            return false;
        }

        const std::int32_t thingNumber = *parsed;
        if (thingNumber < 0)
        {
            intValue = -1;
            return true;
        }

        if (const std::optional<std::int32_t> thingIndex = level.tryGetThingIndexByNum(thingNumber))
        {
            intValue = *thingIndex;
            return true;
        }

        if (static_cast<std::size_t>(thingNumber) >= level.things.size())
        {
            return false;
        }

        intValue = thingNumber;
        return true;
    }
    case CogType::surface:
    {
        const auto words = sed::text::splitWords(text);
        if (words.empty())
        {
            return false;
        }

        if (words.size() == 1)
        {
            const auto parsedGlobal = sed::text::parseInt<std::int32_t>(words[0]);
            if (!parsedGlobal)
            {
                return false;
            }
            if (*parsedGlobal < 0)
            {
                intValue = -1;
                return true;
            }

            // JED surface references are written as "sector surface". A single
            // non-negative token is ambiguous with a missing surface index, so
            // reject it and let callers that truly want raw/global values fall
            // back to Value::parse.
            return false;
        }

        const auto parsedSector = sed::text::parseInt<std::int32_t>(words[0]);
        const auto parsedSurface = sed::text::parseInt<std::int32_t>(words[1]);
        if (!parsedSector || !parsedSurface)
        {
            return false;
        }

        if (*parsedSector < 0)
        {
            intValue = -1;
            return true;
        }

        if (const std::optional<SurfaceRef> ref = level.tryGetSurfaceRefBySectorAndSurfaceNum(*parsedSector, *parsedSurface))
        {
            intValue = level.getGlobalSurfaceNumber(ref->sector, ref->surface);
            return intValue >= 0;
        }

        const std::int32_t sectorIndex = *parsedSector;
        const std::int32_t surfaceIndex = *parsedSurface;
        if (sectorIndex < 0 || surfaceIndex < 0)
        {
            return false;
        }
        if (static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            return false;
        }
        if (static_cast<std::size_t>(surfaceIndex) >= level.sectors[static_cast<std::size_t>(sectorIndex)].surfaces.size())
        {
            return false;
        }

        const std::int32_t globalSurface = level.getGlobalSurfaceNumber(sectorIndex, surfaceIndex);
        if (globalSurface < 0)
        {
            return false;
        }

        intValue = globalSurface;
        return true;
    }
    default:
        break;
    }

    return parse(text);
}

bool TplValue::tryGetFrame(double& x, double& y, double& z, double& pch, double& yaw, double& rol) const
{
    const std::optional<sed::FrameValue> parsed = sed::parse::frameValue(stringValue);
    if (!parsed)
    {
        return false;
    }

    x = parsed->x;
    y = parsed->y;
    z = parsed->z;
    pch = parsed->pch;
    yaw = parsed->yaw;
    rol = parsed->rol;
    return true;
}

void TplValue::getFrame(double& x, double& y, double& z, double& pch, double& yaw, double& rol) const
{
    double nextX{}, nextY{}, nextZ{}, nextPitch{}, nextYaw{}, nextRoll{};
    (void)tryGetFrame(nextX, nextY, nextZ, nextPitch, nextYaw, nextRoll);
    x = nextX;
    y = nextY;
    z = nextZ;
    pch = nextPitch;
    yaw = nextYaw;
    rol = nextRoll;
}

void TplValue::setFrame(const double x, const double y, const double z, const double pch, const double yaw, const double rol)
{
    stringValue = sed::format::frameValue(x, y, z, pch, yaw, rol);
    type = ValueType::frame;
}

std::string formatTplTuple(const std::initializer_list<double> components)
{
    std::string result("(");
    bool first = true;
    for (const double component : components)
    {
        if (!first) result += '/';
        result += sed::format::authoredDouble(component);
        first = false;
    }
    result += ')';
    return result;
}

std::string formatTplTuple(
    const std::initializer_list<double> components,
    const int significantDigits)
{
    std::ostringstream stream{};
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::clamp(significantDigits, 1, 17));
    stream << '(';
    bool first = true;
    for (const double component : components)
    {
        if (!first)
        {
            stream << '/';
        }
        stream << component;
        first = false;
    }
    stream << ')';
    return stream.str();
}

TplValue deserializeTplValue(const std::string_view name, const std::string_view textValue)
{
    TplValue value{};
    value.name = std::string(name);
    value.type = tplValueTypeForName(name);
    value.adjType = tplAdjTypeForName(name);

    const std::string canonical = canonicalTplTupleText(value, textValue);
    if (!value.parse(canonical))
    {
        value.type = ValueType::str;
        value.stringValue = canonical;
    }
    return value;
}

std::string serializeTplValue(const TplValue& value)
{
    return canonicalTplTupleText(value, value.asString());
}

TplValue deserializeIjimNdyTplValue(
    const std::string_view name,
    const std::string_view textValue)
{
    TplValue value = deserializeTplValue(name, textValue);
    if (!text::iequalsAscii(name, "lightintensity"))
    {
        return value;
    }

    const std::string canonical = value.asString();
    const std::vector<std::string_view> parts = canonicalSlashTupleParts(canonical);
    if (parts.size() == 3)
    {
        // TODO(OpenJones3D text format): Preserve an authored Headlight range
        // once sithActor_ParseArg accepts a fourth lightintensity component.
        // Retail-compatible text supplies RGB only; SED materializes its
        // editor-side default range explicitly.
        value.stringValue = formatCanonicalSlashTuple(parts, parts.size());
        value.stringValue.insert(value.stringValue.size() - 1, "/1");
    }
    return value;
}

std::string serializeIjimNdyTplValue(const TplValue& value)
{
    const std::string canonical = serializeTplValue(value);
    if (!text::iequalsAscii(value.name, "lightintensity"))
    {
        return canonical;
    }

    const std::vector<std::string_view> parts = canonicalSlashTupleParts(canonical);
    if (parts.size() != 4)
    {
        return canonical;
    }

    // TODO(OpenJones3D text format): Emit the fourth Headlight range only
    // after sithActor_ParseArg is extended to scan it.  It currently scans
    // exactly RGB, so retaining the editor-only range would be misleading.
    return formatCanonicalSlashTuple(parts, 3);
}

std::string_view toString(const ValueType type) noexcept
{
    switch (type)
    {
    case ValueType::str:
        return "str";
    case ValueType::integer:
        return "int";
    case ValueType::floating:
        return "float";
    case ValueType::flag:
        return "flag";
    case ValueType::ptr:
        return "ptr";
    case ValueType::vect:
        return "vect";
    case ValueType::frame:
        return "frame";
    default:
        return "unknown";
    }
}

std::string_view toString(const CogType type) noexcept
{
    switch (type)
    {
    case CogType::ai:
        return "ai";
    case CogType::cog:
        return "cog";
    case CogType::key:
        return "key";
    case CogType::mat:
        return "mat";
    case CogType::msg:
        return "msg";
    case CogType::model3do:
        return "3do";
    case CogType::sec:
        return "sec";
    case CogType::wav:
        return "wav";
    case CogType::surface:
        return "srf";
    case CogType::template_:
        return "tpl";
    case CogType::thing:
        return "thg";
    case CogType::int_:
        return "int";
    case CogType::float_:
        return "float";
    case CogType::vect:
        return "vect";
    default:
        return "unk";
    }
}

std::string_view toString(const AdjType type) noexcept
{
    switch (type)
    {
    case AdjType::mat:
        return "mat";
    case AdjType::cog:
        return "cog";
    case AdjType::snd:
        return "snd";
    case AdjType::pup:
        return "pup";
    case AdjType::spr:
        return "spr";
    case AdjType::par:
        return "par";
    case AdjType::model3do:
        return "3do";
    case AdjType::ai:
        return "ai";
    case AdjType::tpl:
        return "tpl";
    case AdjType::frame:
        return "frame";
    default:
        return "unk";
    }
}

ValueType tplValueTypeForName(const std::string_view name) noexcept
{
    if (sed::text::iequalsAscii(name, "frame"))
    {
        return ValueType::frame;
    }

    if (sed::text::iequalsAscii(name, "thingflags")
        || sed::text::iequalsAscii(name, "typeflags")
        || sed::text::iequalsAscii(name, "physflags")
        || sed::text::iequalsAscii(name, "damageclass"))
    {
        return ValueType::flag;
    }

    return ValueType::str;
}

AdjType tplAdjTypeForName(const std::string_view name) noexcept
{
    if (sed::text::iequalsAscii(name, "material"))
    {
        return AdjType::mat;
    }
    if (sed::text::iequalsAscii(name, "cog"))
    {
        return AdjType::cog;
    }
    if (sed::text::iequalsAscii(name, "soundclass"))
    {
        return AdjType::snd;
    }
    if (sed::text::iequalsAscii(name, "puppet"))
    {
        return AdjType::pup;
    }
    if (sed::text::iequalsAscii(name, "sprite"))
    {
        return AdjType::spr;
    }
    if (sed::text::iequalsAscii(name, "particle"))
    {
        return AdjType::par;
    }
    if (sed::text::iequalsAscii(name, "model3d") || sed::text::iequalsAscii(name, "model3do"))
    {
        return AdjType::model3do;
    }
    if (sed::text::iequalsAscii(name, "aiclass"))
    {
        return AdjType::ai;
    }

    if (sed::text::iequalsAscii(name, "creatething")
        || sed::text::iequalsAscii(name, "explode")
        || sed::text::iequalsAscii(name, "fleshhit")
        || sed::text::iequalsAscii(name, "weapon")
        || sed::text::iequalsAscii(name, "weapon2")
        || sed::text::iequalsAscii(name, "debris")
        || sed::text::iequalsAscii(name, "trailthing"))
    {
        return AdjType::tpl;
    }

    if (sed::text::iequalsAscii(name, "frame"))
    {
        return AdjType::frame;
    }

    return AdjType::unk;
}

} // namespace sed
