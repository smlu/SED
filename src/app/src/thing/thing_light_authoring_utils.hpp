#pragma once

#include "render/lighting/dynamic_light_utils.hpp"

#include "sed/assets/tpl.hpp"
#include "sed/level.hpp"
#include "sed/text.hpp"
#include "sed/format_number.hpp"
#include <array>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace sed::thing::light_authoring
{

enum class Kind : std::uint8_t
{
    regular,
    headlight,
};

enum class AttachmentEligibility : std::uint8_t
{
    allowed,
    invalidPlacement,
    regularAlreadyPresent,
    headlightRequiresActor,
    headlightAlreadyPresent,
};

enum class RemovalResult : std::uint8_t
{
    absent,
    inheritedOnly,
    removed,
    inheritedRevealed,
};

inline const TplValue* localValue(
    const Thing& thing,
    const std::string_view name) noexcept
{
    const auto it = std::find_if(
        thing.values.begin(), thing.values.end(), [name](const TplValue& value)
        {
            return text::iequalsAscii(value.name, name);
        });
    return it == thing.values.end() ? nullptr : &*it;
}

inline TplValue* localValue(
    Thing& thing,
    const std::string_view name) noexcept
{
    const auto it = std::find_if(
        thing.values.begin(), thing.values.end(), [name](const TplValue& value)
        {
            return text::iequalsAscii(value.name, name);
        });
    return it == thing.values.end() ? nullptr : &*it;
}

inline const TplValue* effectiveValue(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const std::string_view name) noexcept
{
    return level::thingFindValue(level, templates, thing, name);
}

/** Resolve only the template side of a Thing field.
 *
 * `thingFindValue` deliberately gives instance values precedence.  Authoring
 * operations also need to know what will become effective after an instance
 * override is removed, so perform the same lookup with the local value list
 * hidden while retaining the Thing's template identity.
 */
inline const TplValue* inheritedValue(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const std::string_view name)
{
    Thing templateLookup = thing;
    templateLookup.values.clear();
    return level::thingFindValue(level, templates, templateLookup, name);
}

inline bool hasEffectiveValue(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const std::string_view name) noexcept
{
    return effectiveValue(level, templates, thing, name) != nullptr;
}

/** Whether the effective actor Headlight intensity can illuminate the scene.
 *
 * This is used when attaching over an inherited actor default: a black default
 * must be replaced by the visible editor default, while an already-enabled
 * black Headlight is detected separately as authored state.
 */
inline bool hasUsableHeadlightIntensity(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing) noexcept
{
    const TplValue* value = effectiveValue(level, templates, thing, "lightintensity");
    if (!value)
    {
        return false;
    }

    if (level.kind == ProjectType::IJIM)
    {
        const render::lighting::IjimAuthoredLight parsed =
            render::lighting::parseIjimAuthoredLight(*value);
        const float average =
            (parsed.color.x() + parsed.color.y() + parsed.color.z()) / 3.0f;
        return parsed.valid
            && render::lighting::hasVisibleColor(parsed.color)
            && render::lighting::finitePositive(average);
    }

    return render::lighting::finitePositive(
        render::lighting::scalarThingLightValue(*value));
}

inline bool hasUsableHeadlightIntensityValue(
    const ProjectType project,
    const TplValue* value) noexcept
{
    if (!value)
    {
        return false;
    }
    if (project == ProjectType::IJIM)
    {
        const render::lighting::IjimAuthoredLight parsed =
            render::lighting::parseIjimAuthoredLight(*value);
        return parsed.valid;
    }
    return std::isfinite(render::lighting::scalarThingLightValue(*value));
}

/** Whether an effective Headlight offset is a finite three-component vector. */
inline bool hasUsableHeadlightOffset(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing) noexcept
{
    const TplValue* value = effectiveValue(level, templates, thing, "lightoffset");
    return value
        && render::lighting::parseIjimAuthoredLight(*value).valid;
}

inline bool hasValidSector(const Level& level, const Thing& thing) noexcept
{
    return thing.sector >= 0 && static_cast<std::size_t>(thing.sector) < level.sectors.size();
}

inline AttachmentEligibility eligibility(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const Kind kind) noexcept
{
    if (!hasValidSector(level, thing))
    {
        return AttachmentEligibility::invalidPlacement;
    }

    if (kind == Kind::regular)
    {
        return hasEffectiveValue(level, templates, thing, "light")
            ? AttachmentEligibility::regularAlreadyPresent
            : AttachmentEligibility::allowed;
    }

    if (!render::lighting::isActorOrPlayer(level, templates, thing))
    {
        return AttachmentEligibility::headlightRequiresActor;
    }

    // Actor templates commonly provide lightintensity/lightoffset defaults
    // before a particular instance enables the engine's Headlight type flag.
    // Treat that inherited intensity as a usable default, not as a duplicate.
    const bool headlightEnabled =
        (render::lighting::integralThingValue(level, templates, thing, "typeflags")
            & render::lighting::kActorHeadlightFlag) != 0;
    if (headlightEnabled
        && hasUsableHeadlightIntensityValue(
            level.kind,
            effectiveValue(level, templates, thing, "lightintensity")))
    {
        return AttachmentEligibility::headlightAlreadyPresent;
    }
    return AttachmentEligibility::allowed;
}

inline bool canAttach(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const Kind kind) noexcept
{
    return eligibility(level, templates, thing, kind) == AttachmentEligibility::allowed;
}

inline std::string defaultValue(const ProjectType project, const Kind kind)
{
    if (kind == Kind::regular)
    {
        // IJIM stores RGB plus the authored cut-off range.  JK/MotS store a
        // scalar whose runtime attenuation derives both radii.
        return project == ProjectType::IJIM ? "(1/1/1/1)" : "1";
    }

    // OpenJones3D stores Headlight intensity in rdVector4 and consumes alpha
    // as a shared authored range. Keep the safe one-unit editor default
    // explicit. Classic projects have no independent range field and retain
    // the original scalar-intensity/derived-radius representation.
    return project == ProjectType::IJIM ? "(1/1/1/1)" : "1";
}

inline constexpr std::string_view defaultHeadlightOffset() noexcept
{
    return "(0/0/0)";
}

/** Return the safe initial text for a runtime-light Thing parameter.
 *
 * Keep this mapping in the authoring utility so the placement tool and the
 * generic Add Value row cannot silently diverge.  Unknown parameters have no
 * implicit default.
 */
inline std::optional<std::string> defaultValueForParameter(
    const ProjectType project,
    const std::string_view name)
{
    if (text::iequalsAscii(name, "light"))
    {
        return defaultValue(project, Kind::regular);
    }
    if (text::iequalsAscii(name, "lightintensity"))
    {
        return defaultValue(project, Kind::headlight);
    }
    if (text::iequalsAscii(name, "lightoffset"))
    {
        return std::string(defaultHeadlightOffset());
    }
    return std::nullopt;
}

inline TplValue& upsertStringValue(
    Thing& thing,
    const std::string_view name,
    const std::string_view value)
{
    for (TplValue& current : thing.values)
    {
        if (!text::iequalsAscii(current.name, name))
        {
            continue;
        }

        const std::string currentName = current.name;
        current = deserializeTplValue(currentName, value);
        return current;
    }

    thing.values.push_back(deserializeTplValue(name, value));
    return thing.values.back();
}

// Authored controls keep double precision; rendering's float light DTO is
// deliberately unsuitable for modifying these persistent numeric tokens.
inline std::optional<std::array<double, 4>> parseAuthoredTuple(const TplValue& value)
{
    std::string normalized = value.asString();
    for (char& ch : normalized)
        if (ch == '/' || ch == ',' || ch == '(' || ch == ')' || ch == ':') ch = ' ';
    const auto parts = text::splitWords(normalized);
    if (parts.size() < 3) return std::nullopt;
    std::array<double, 4> result{};
    for (std::size_t i = 0; i < std::min<std::size_t>(4, parts.size()); ++i)
    {
        const auto parsed = text::parseDouble(parts[i]);
        if (!parsed || !std::isfinite(*parsed)) return std::nullopt;
        result[i] = *parsed;
    }
    return result;
}

inline std::optional<TVector> parseAuthoredOffset(const TplValue& value)
{
    const auto parsed = parseAuthoredTuple(value);
    return parsed ? std::optional<TVector>(TVector{(*parsed)[0], (*parsed)[1], (*parsed)[2]})
                  : std::nullopt;
}

inline std::optional<double> parseAuthoredScalar(const TplValue& value)
{
    if (value.type == ValueType::integer || value.type == ValueType::flag || value.type == ValueType::ptr)
        return static_cast<double>(value.intValue);
    const auto parsed = value.type == ValueType::floating
        ? std::optional<double>(value.floatValue) : text::parseDouble(text::trimView(value.asString()));
    return parsed && std::isfinite(*parsed) ? parsed : std::nullopt;
}

inline std::string formatScalar(const double value)
{
    return sed::format::authoredDouble(value);
}

inline std::string formatVector(const double x, const double y, const double z)
{
    return formatTplTuple({ x, y, z });
}

inline std::string formatIjimLight(
    const double red,
    const double green,
    const double blue,
    const double range)
{
    return formatTplTuple({ red, green, blue, range });
}

/** Preserve authored IJIM RGB token precision while making range explicit.
 *
 * Attaching a new Headlight to an actor whose template supplies legacy RGB
 * uses the same one-unit default range as a wholly new Headlight. An already
 * explicit fourth component remains authoritative, including zero.
 */
inline std::string canonicalIjimHeadlightIntensity(const TplValue& value)
{
    std::string canonical = serializeTplValue(value);
    const std::size_t slashCount = static_cast<std::size_t>(
        std::count(canonical.begin(), canonical.end(), '/'));
    if (slashCount == 2u && canonical.size() >= 2u
        && canonical.front() == '(' && canonical.back() == ')')
    {
        canonical.insert(canonical.size() - 1u, "/1");
    }
    return canonical;
}

inline std::string formatFlags(const std::int32_t flags)
{
    std::ostringstream stream{};
    stream.imbue(std::locale::classic());
    stream << "0x" << std::hex << static_cast<std::uint32_t>(flags);
    return stream.str();
}

inline bool eraseLocalValue(Thing& thing, const std::string_view name)
{
    const auto before = thing.values.size();
    std::erase_if(thing.values, [name](const TplValue& value)
    {
        return text::iequalsAscii(value.name, name);
    });
    return thing.values.size() != before;
}

/** Remove an instance-authored runtime light without flattening inheritance.
 *
 * A local regular-light value is simply erased, revealing the template value
 * when one exists.  A Headlight is one logical source spread across the actor
 * flag, intensity and offset fields: local intensity/offset overrides are
 * erased together and only the Headlight bit is reconciled with the inherited
 * flags, preserving unrelated local actor flags.
 */
inline RemovalResult removeLocalOverride(
    Level& level,
    const assets::Templates* templates,
    const std::int32_t thingIndex,
    const Kind kind)
{
    if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= level.things.size())
    {
        return RemovalResult::absent;
    }

    Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
    if (kind == Kind::regular)
    {
        const bool inherited = inheritedValue(level, templates, thing, "light") != nullptr;
        if (!localValue(thing, "light"))
        {
            return inherited ? RemovalResult::inheritedOnly : RemovalResult::absent;
        }
        (void)eraseLocalValue(thing, "light");
        return inherited ? RemovalResult::inheritedRevealed : RemovalResult::removed;
    }

    const TplValue* inheritedIntensity =
        inheritedValue(level, templates, thing, "lightintensity");
    Thing inheritedLookup = thing;
    inheritedLookup.values.clear();
    const std::int32_t inheritedFlags = render::lighting::integralThingValue(
        level, templates, inheritedLookup, "typeflags");
    const bool inheritedEmitter =
        render::lighting::isActorOrPlayer(level, templates, thing)
        && (inheritedFlags & render::lighting::kActorHeadlightFlag) != 0
        && hasUsableHeadlightIntensityValue(level.kind, inheritedIntensity);

    const bool hadLocalFlags = localValue(thing, "typeflags") != nullptr;
    const std::int32_t localFlags = hadLocalFlags
        ? render::lighting::integralThingValue(level, nullptr, thing, "typeflags")
        : inheritedFlags;
    const bool localFlagOverride = hadLocalFlags
        && ((localFlags ^ inheritedFlags) & render::lighting::kActorHeadlightFlag) != 0;
    const bool haveLocalOverride = localValue(thing, "lightintensity")
        || localValue(thing, "lightoffset")
        || localFlagOverride;
    if (!haveLocalOverride)
    {
        return inheritedEmitter ? RemovalResult::inheritedOnly : RemovalResult::absent;
    }

    (void)eraseLocalValue(thing, "lightintensity");
    (void)eraseLocalValue(thing, "lightoffset");

    if (hadLocalFlags)
    {
        const std::int32_t reconciledFlags =
            (localFlags & ~render::lighting::kActorHeadlightFlag)
            | (inheritedFlags & render::lighting::kActorHeadlightFlag);
        if (reconciledFlags == inheritedFlags)
        {
            (void)eraseLocalValue(thing, "typeflags");
        }
        else
        {
            (void)upsertStringValue(thing, "typeflags", formatFlags(reconciledFlags));
        }
    }

    return inheritedEmitter ? RemovalResult::inheritedRevealed : RemovalResult::removed;
}

/** Attach a source-compatible authored light to one Thing.
 *
 * Headlights additionally set the actor Headlight type flag and materialize
 * local intensity/offset fields. Usable inherited values are preserved; IJIM
 * RGB is canonicalized to the four-component color/range form. A missing,
 * malformed, or disabled-zero value receives a safe default. This
 * makes the attachment visible in the Thing editor and ensures it survives as
 * an explicit instance override. The caller is expected to have checked layer
 * locking before mutating the document.
 */
inline bool attachDefault(
    Level& level,
    const assets::Templates* templates,
    const std::int32_t thingIndex,
    const Kind kind)
{
    if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= level.things.size())
    {
        return false;
    }

    Thing& thing = level.things[static_cast<std::size_t>(thingIndex)];
    if (!canAttach(level, templates, thing, kind))
    {
        return false;
    }

    if (kind == Kind::regular)
    {
        (void)upsertStringValue(thing, "light", defaultValue(level.kind, kind));
        return true;
    }

    // Resolve and copy inherited fields before appending to thing.values: an
    // upsert may reallocate that vector and invalidate a local-field pointer.
    const TplValue* effectiveIntensity =
        effectiveValue(level, templates, thing, "lightintensity");
    std::string localIntensity = defaultValue(level.kind, kind);
    if (hasUsableHeadlightIntensity(level, templates, thing) && effectiveIntensity)
    {
        if (level.kind == ProjectType::IJIM)
        {
            localIntensity = canonicalIjimHeadlightIntensity(*effectiveIntensity);
        }
        else
        {
            localIntensity = effectiveIntensity->asString();
        }
    }

    const TplValue* effectiveOffset =
        effectiveValue(level, templates, thing, "lightoffset");
    const std::string localOffset =
        hasUsableHeadlightOffset(level, templates, thing) && effectiveOffset
        ? effectiveOffset->asString()
        : std::string(defaultHeadlightOffset());

    const std::int32_t currentTypeFlags =
        render::lighting::integralThingValue(level, templates, thing, "typeflags");
    (void)upsertStringValue(
        thing,
        "typeflags",
        formatFlags(currentTypeFlags | render::lighting::kActorHeadlightFlag));
    (void)upsertStringValue(thing, "lightintensity", localIntensity);
    (void)upsertStringValue(thing, "lightoffset", localOffset);
    return true;
}

} // namespace sed::thing::light_authoring
