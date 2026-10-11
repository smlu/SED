#include "sed/level/thing.hpp"

#include "sed/assets/tpl.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace sed::level
{
namespace
{

using sed::text::iequalsAscii;
using sed::text::parseDouble;
using sed::text::parseHex;
using sed::text::parseInt;
using sed::text::splitWords;
using sed::text::trimView;

const ThingTemplate* findTemplate(const Level& level, const std::string_view name) noexcept
{
    for (const ThingTemplate& tpl : level.templates)
    {
        if (iequalsAscii(tpl.name, name))
        {
            return &tpl;
        }
    }
    return nullptr;
}

const TplValue* findTemplateField(const Level& level, const std::string_view tplName, const std::string_view fieldName) noexcept
{
    std::string_view current = tplName;

    for (int depth = 0; depth < 1000; ++depth)
    {
        const ThingTemplate* tpl = findTemplate(level, current);
        if (!tpl)
        {
            return nullptr;
        }

        // Thing arguments are parsed from left to right by the engines, so a
        // later scalar occurrence in the same template replaces an earlier
        // one.  Repeatable parameters must be enumerated by their callers;
        // this single-value lookup returns the last authored occurrence.
        for (auto it = tpl->values.rbegin(); it != tpl->values.rend(); ++it)
        {
            if (iequalsAscii(it->name, fieldName))
            {
                return &*it;
            }
        }

        if (tpl->parent.empty() || iequalsAscii(tpl->parent, "none"))
        {
            return nullptr;
        }

        current = tpl->parent;
    }

    return nullptr;
}

std::optional<double> parseScalar(const Value& value) noexcept
{
    if (value.type == ValueType::floating)
    {
        return value.floatValue;
    }

    if (value.type == ValueType::integer || value.type == ValueType::ptr || value.type == ValueType::flag)
    {
        return static_cast<double>(value.intValue);
    }

    const std::string_view text = trimView(value.stringValue);
    if (text.empty())
    {
        return std::nullopt;
    }

    return parseDouble(text);
}

ColorF parseColor(const std::string_view text) noexcept
{
    std::string normalized;
    normalized.reserve(text.size());

    for (const char ch : text)
    {
        if (ch == '/' || ch == ',' || ch == '(' || ch == ')' || ch == ':')
        {
            normalized.push_back(' ');
        }
        else
        {
            normalized.push_back(ch);
        }
    }

    const auto parts = splitWords(normalized);

    ColorF out{ ColorBlack };
    if (parts.size() >= 3)
    {
        out.r = static_cast<float>(parseDouble(parts[0]).value_or(0.0));
        out.g = static_cast<float>(parseDouble(parts[1]).value_or(0.0));
        out.b = static_cast<float>(parseDouble(parts[2]).value_or(0.0));
        out.a = (parts.size() >= 4) ? static_cast<float>(parseDouble(parts[3]).value_or(1.0)) : 1.0f;
        out.normalize();
    }

    return out;
}

} // namespace

const TplValue* thingFindValue(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const std::string_view name) noexcept
{
    // Placed-Thing arguments follow the same left-to-right parser rule as
    // templates: the last scalar occurrence is the effective local value.
    for (auto it = thing.values.rbegin(); it != thing.values.rend(); ++it)
    {
        if (iequalsAscii(it->name, name))
        {
            return &*it;
        }
    }

    const std::string_view tplName = !thing.templateName.empty() ? thing.templateName : thing.name;

    if (const TplValue* v = findTemplateField(level, tplName, name))
    {
        return v;
    }

    return (templates != nullptr) ? templates->findField(tplName, name) : nullptr;
}

const TplValue* thingFindValue(const Level& level, const Thing& thing, const std::string_view name) noexcept
{
    return thingFindValue(level, nullptr, thing, name);
}

std::int32_t thingFlags(const Level& level, const assets::Templates* templates, const Thing& thing) noexcept
{
    const TplValue* v = thingFindValue(level, templates, thing, "thingflags");
    if (!v)
    {
        return 0;
    }

    if (v->type == ValueType::integer || v->type == ValueType::flag || v->type == ValueType::ptr)
    {
        return v->intValue;
    }

    const std::string_view text = trimView(v->stringValue);
    if (text.empty())
    {
        return 0;
    }

    if (const auto parsed = parseInt<std::int32_t>(text))
    {
        return *parsed;
    }

    if (const auto parsedHex = parseHex(text))
    {
        return static_cast<std::int32_t>(*parsedHex);
    }

    return 0;
}

std::int32_t thingFlags(const Level& level, const Thing& thing) noexcept
{
    return thingFlags(level, nullptr, thing);
}

ColorF thingLight(const Level& level, const assets::Templates* templates, const Thing& thing) noexcept
{
    const TplValue* v = thingFindValue(level, templates, thing, "light");
    if (!v)
    {
        return ColorBlack;
    }

    if (level.kind == ProjectType::IJIM)
    {
        return parseColor(v->stringValue);
    }

    const std::optional<double> intensity = parseScalar(*v);
    if (!intensity)
    {
        return ColorBlack;
    }

    return intensityToColor(static_cast<float>(*intensity));
}

ColorF thingLight(const Level& level, const Thing& thing) noexcept
{
    return thingLight(level, nullptr, thing);
}

std::size_t thingCountFrames(const Thing& thing) noexcept
{
    return static_cast<std::size_t>(std::count_if(thing.values.begin(), thing.values.end(), [](const TplValue& v)
        {
            return v.type == ValueType::frame && sed::text::iequalsAscii(v.name, "frame");
        }));
}

void thingSyncNumFrames(Thing& thing) noexcept
{
    const std::int32_t frameCount = static_cast<std::int32_t>(thingCountFrames(thing));

    auto isNumFrames = [](const TplValue& v) -> bool
        {
            return sed::text::iequalsAscii(v.name, "numframes");
        };

    // When there are no frames, keep the thing clean: remove `numframes` entirely.
    // (The editor maintains it automatically; users shouldn't have to manage it.)
    if (frameCount <= 0)
    {
        thing.values.erase(std::remove_if(thing.values.begin(), thing.values.end(), isNumFrames), thing.values.end());
        return;
    }

    TplValue numFrames{};
    const auto authoredNumFrames =
        std::find_if(thing.values.begin(), thing.values.end(), isNumFrames);
    if (authoredNumFrames != thing.values.end())
    {
        numFrames = *authoredNumFrames;
    }
    numFrames.name = "numframes";
    numFrames.type = ValueType::integer;
    numFrames.adjType = tplAdjTypeForName(numFrames.name);
    numFrames.intValue = frameCount;
    numFrames.floatValue = 0.0;
    numFrames.stringValue.clear();

    // The runtime allocates the path/AI frame buffer when it parses
    // `numframes`; every following `frame` appends into that buffer. Normalize
    // both missing and misplaced legacy/editor values so save order remains
    // parseable, while also collapsing duplicates.
    thing.values.erase(
        std::remove_if(thing.values.begin(), thing.values.end(), isNumFrames),
        thing.values.end());
    const auto firstFrame = std::find_if(
        thing.values.begin(), thing.values.end(), [](const TplValue& value)
        {
            return value.type == ValueType::frame
                && sed::text::iequalsAscii(value.name, "frame");
        });
    thing.values.insert(firstFrame, std::move(numFrames));
}

void thingSyncNumFrames(ThingTemplate& thingTemplate) noexcept
{
    Thing valueOwner{};
    valueOwner.values = std::move(thingTemplate.values);
    thingSyncNumFrames(valueOwner);
    thingTemplate.values = std::move(valueOwner.values);
}

} // namespace sed::level
