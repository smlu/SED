#pragma once

#include "sed/assets/tpl.hpp"
#include "sed/level/thing.hpp"
#include "sed/text.hpp"
#include "sed/values.hpp"

#include <QString>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sed::thing::parameters
{

struct EffectiveParameter
{
    TplValue value{};
    bool localOverride{};
    std::optional<TplValue> templateValue{};
};

/** One copied authored value and the template which contributed it. */
struct InheritedParameterOccurrence
{
    TplValue value{};
    std::string templateName{};
    std::size_t templateOrder{};
    std::size_t valueIndex{};
};

struct InheritedPathFrameState
{
    std::size_t capacity{};
    std::vector<InheritedParameterOccurrence> activeFrames{};
    std::vector<InheritedParameterOccurrence> ignoredFrames{};
};

inline const TplValue* localValue(
    const Thing& thing,
    const std::string_view name) noexcept
{
    const auto it = std::find_if(
        thing.values.rbegin(), thing.values.rend(), [name](const TplValue& value)
        {
            return text::iequalsAscii(value.name, name);
        });
    return it == thing.values.rend() ? nullptr : &*it;
}

/** Locate one authored occurrence without collapsing repeated parameters. */
inline std::optional<std::size_t> localValueIndex(
    const Thing& thing,
    const std::string_view name,
    const std::size_t occurrence) noexcept
{
    std::size_t current = 0;
    for (std::size_t index = 0; index < thing.values.size(); ++index)
    {
        if (!text::iequalsAscii(thing.values[index].name, name))
        {
            continue;
        }
        if (current++ == occurrence)
        {
            return index;
        }
    }
    return std::nullopt;
}

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

namespace detail
{

inline const ThingTemplate* levelTemplate(
    const Level& level,
    const std::string_view name) noexcept
{
    const auto it = std::find_if(
        level.templates.begin(), level.templates.end(), [name](const ThingTemplate& tpl)
        {
            return text::iequalsAscii(tpl.name, name);
        });
    return it == level.templates.end() ? nullptr : &*it;
}

inline void appendUniqueName(
    std::vector<std::string>& names,
    const std::string_view name)
{
    if (name.empty()
        || std::any_of(names.begin(), names.end(), [name](const std::string& existing)
            {
                return text::iequalsAscii(existing, name);
            }))
    {
        return;
    }
    names.emplace_back(name);
}

template <typename FindTemplate>
inline void appendTemplateChainNames(
    std::vector<std::string>& names,
    const std::string_view rootName,
    FindTemplate&& findTemplate)
{
    std::string current(rootName);
    for (int depth = 0; depth < 1000 && !current.empty(); ++depth)
    {
        const ThingTemplate* tpl = findTemplate(current);
        if (!tpl)
        {
            return;
        }
        for (const TplValue& value : tpl->values)
        {
            appendUniqueName(names, value.name);
        }
        if (tpl->parent.empty() || text::iequalsAscii(tpl->parent, "none"))
        {
            return;
        }
        current = tpl->parent;
    }
}

template <typename FindTemplate>
inline void appendTemplateChainOccurrences(
    std::vector<InheritedParameterOccurrence>& occurrences,
    const std::string_view rootName,
    const std::string_view parameterName,
    FindTemplate&& findTemplate)
{
    std::vector<const ThingTemplate*> derivedToBase{};
    std::string current(rootName);
    for (int depth = 0; depth < 1000 && !current.empty(); ++depth)
    {
        const ThingTemplate* tpl = findTemplate(current);
        if (!tpl
            || std::find(derivedToBase.begin(), derivedToBase.end(), tpl)
                != derivedToBase.end())
        {
            break;
        }
        derivedToBase.push_back(tpl);
        if (tpl->parent.empty() || text::iequalsAscii(tpl->parent, "none"))
        {
            break;
        }
        current = tpl->parent;
    }

    std::size_t templateOrder = 0;
    for (auto templateIt = derivedToBase.rbegin();
         templateIt != derivedToBase.rend();
         ++templateIt, ++templateOrder)
    {
        const ThingTemplate& tpl = **templateIt;
        for (std::size_t valueIndex = 0;
             valueIndex < tpl.values.size();
             ++valueIndex)
        {
            const TplValue& value = tpl.values[valueIndex];
            if (text::iequalsAscii(value.name, parameterName))
            {
                occurrences.push_back(InheritedParameterOccurrence{
                    value,
                    tpl.name,
                    templateOrder,
                    valueIndex,
                });
            }
        }
    }
}

} // namespace detail

/** Collect every inherited occurrence in runtime base-to-derived order.
 *
 * Repeated values and even identical repeated values remain distinct. If the
 * embedded Level template chain contributes at least one matching value, that
 * complete matching chain wins. The project template database is consulted
 * only when the embedded chain contributes none, mirroring `thingFindValue`'s
 * field-level embedded precedence and fallback.
 */
inline std::vector<InheritedParameterOccurrence>
inheritedRepeatableParameterOccurrences(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing,
    const std::string_view parameterName)
{
    const std::string_view rootName = !thing.templateName.empty()
        ? std::string_view(thing.templateName)
        : std::string_view(thing.name);

    std::vector<InheritedParameterOccurrence> occurrences{};
    detail::appendTemplateChainOccurrences(
        occurrences,
        rootName,
        parameterName,
        [&level](const std::string_view name)
        {
            return detail::levelTemplate(level, name);
        });
    if (!occurrences.empty() || !templates)
    {
        return occurrences;
    }

    detail::appendTemplateChainOccurrences(
        occurrences,
        rootName,
        parameterName,
        [templates](const std::string_view name)
        {
            return templates->find(name);
        });
    return occurrences;
}

/** Resolve the inherited Path buffer exactly enough for placed-Thing editing.
 *
 * The first positive `numframes` encountered in base-to-derived token order
 * allocates the buffer; later `numframes` values cannot replace it. A `frame`
 * before allocation or beyond capacity is ignored by the runtime. This helper
 * deliberately models Path frames only—AI waypoint frames are instance-local
 * and must not be presented as inherited template data.
 */
inline InheritedPathFrameState inheritedPathFrameState(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing)
{
    struct OrderedToken
    {
        InheritedParameterOccurrence occurrence{};
        bool frame{};
    };

    std::vector<OrderedToken> tokens{};
    for (InheritedParameterOccurrence occurrence :
         inheritedRepeatableParameterOccurrences(
             level, templates, thing, "numframes"))
    {
        tokens.push_back(OrderedToken{ std::move(occurrence), false });
    }
    for (InheritedParameterOccurrence occurrence :
         inheritedRepeatableParameterOccurrences(
             level, templates, thing, "frame"))
    {
        tokens.push_back(OrderedToken{ std::move(occurrence), true });
    }
    std::stable_sort(tokens.begin(), tokens.end(), [](const OrderedToken& lhs, const OrderedToken& rhs)
        {
            return std::tie(
                       lhs.occurrence.templateOrder,
                       lhs.occurrence.valueIndex)
                < std::tie(
                       rhs.occurrence.templateOrder,
                       rhs.occurrence.valueIndex);
        });

    InheritedPathFrameState state{};
    for (OrderedToken& token : tokens)
    {
        if (!token.frame)
        {
            if (state.capacity != 0u)
            {
                continue;
            }
            const TplValue& value = token.occurrence.value;
            const std::int32_t parsed = value.type == ValueType::integer
                || value.type == ValueType::ptr
                || value.type == ValueType::flag
                ? value.intValue
                : text::parseInt<std::int32_t>(value.asString()).value_or(0);
            if (parsed > 0)
            {
                state.capacity = static_cast<std::size_t>(parsed);
            }
            continue;
        }

        if (state.capacity != 0u
            && state.activeFrames.size() < state.capacity)
        {
            state.activeFrames.push_back(std::move(token.occurrence));
        }
        else
        {
            state.ignoredFrames.push_back(std::move(token.occurrence));
        }
    }
    return state;
}

/** Whether a debris token names a template which currently resolves. */
inline bool templateReferenceResolves(
    const Level& level,
    const assets::Templates* templates,
    const std::string_view rawName) noexcept
{
    const std::string_view name = text::trimView(rawName);
    if (name.empty() || text::iequalsAscii(name, "none"))
    {
        return false;
    }
    return detail::levelTemplate(level, name) != nullptr
        || (templates && templates->find(name) != nullptr);
}

/** Collect the editable effective parameter surface of a placed Thing.
 *
 * Instance order remains first so existing level-authored rows do not jump.
 * Missing rows are then appended in direct-template-to-base-template order.
 * Candidate names from both embedded Level templates and the project template
 * database are resolved through `thingFindValue`, retaining the editor's
 * established embedded-template precedence.
 */
inline std::vector<EffectiveParameter> effectiveParameters(
    const Level& level,
    const assets::Templates* templates,
    const Thing& thing)
{
    std::vector<std::string> names{};
    names.reserve(thing.values.size() + 16);
    for (const TplValue& value : thing.values)
    {
        detail::appendUniqueName(names, value.name);
    }

    const std::string_view rootName = !thing.templateName.empty()
        ? std::string_view(thing.templateName)
        : std::string_view(thing.name);
    detail::appendTemplateChainNames(
        names,
        rootName,
        [&level](const std::string_view name)
        {
            return detail::levelTemplate(level, name);
        });
    if (templates)
    {
        detail::appendTemplateChainNames(
            names,
            rootName,
            [templates](const std::string_view name)
            {
                return templates->find(name);
            });
    }

    std::vector<EffectiveParameter> out{};
    out.reserve(names.size());
    for (const std::string& name : names)
    {
        const TplValue* local = localValue(thing, name);
        const TplValue* inherited = inheritedValue(level, templates, thing, name);
        const TplValue* effective = local ? local : inherited;
        if (!effective)
        {
            continue;
        }

        EffectiveParameter parameter{};
        parameter.value = *effective;
        parameter.localOverride = local != nullptr;
        if (inherited)
        {
            parameter.templateValue = *inherited;
        }
        out.push_back(std::move(parameter));
    }
    return out;
}

/** Atomically materialize or edit an instance override.
 *
 * An inherited value carries its parsed type/adjacency semantics into the new
 * local value. Unknown custom parameters therefore remain lossless, while a
 * failed typed parse leaves the Thing untouched.
 */
enum class OverrideWriteResult
{
    invalid,
    unchanged,
    changed,
};

inline OverrideWriteResult upsertOverrideDetailed(
    Thing& thing,
    const std::string_view name,
    const std::string_view textValue,
    const TplValue* semanticSource = nullptr)
{
    auto it = std::find_if(
        thing.values.rbegin(), thing.values.rend(), [name](const TplValue& value)
        {
            return text::iequalsAscii(value.name, name);
        });

    TplValue next{};
    if (it != thing.values.rend())
    {
        next = *it;
    }
    else if (semanticSource)
    {
        next = *semanticSource;
    }
    else
    {
        next.name = std::string(name);
        next.type = tplValueTypeForName(name);
        next.adjType = tplAdjTypeForName(name);
    }
    // The semantic source contributes parsing/type metadata only. A newly
    // materialized override must use the caller's requested key; retaining a
    // mismatched source name can otherwise create an adjacent parameter row.
    // Existing local values retain their authored spelling.
    if (it == thing.values.rend())
    {
        next.name = std::string(name);
    }
    if (!next.parse(textValue))
    {
        return OverrideWriteResult::invalid;
    }

    if (it == thing.values.rend())
    {
        thing.values.push_back(std::move(next));
        return OverrideWriteResult::changed;
    }

    const bool changed = it->name != next.name
        || it->type != next.type
        || it->adjType != next.adjType
        || it->asString() != next.asString();
    if (changed)
    {
        *it = std::move(next);
        return OverrideWriteResult::changed;
    }
    return OverrideWriteResult::unchanged;
}

inline bool upsertOverride(
    Thing& thing,
    const std::string_view name,
    const std::string_view textValue,
    const TplValue* semanticSource = nullptr)
{
    return upsertOverrideDetailed(
               thing, name, textValue, semanticSource)
        == OverrideWriteResult::changed;
}

/** Parse and append one occurrence without mutating the Thing on failure. */
inline OverrideWriteResult appendOverrideDetailed(
    Thing& thing,
    const std::string_view name,
    const std::string_view textValue,
    const TplValue* semanticSource = nullptr)
{
    TplValue next{};
    if (semanticSource)
    {
        next = *semanticSource;
    }
    else
    {
        next.type = tplValueTypeForName(name);
        next.adjType = tplAdjTypeForName(name);
    }
    next.name = std::string(name);
    if (!next.parse(textValue))
    {
        return OverrideWriteResult::invalid;
    }
    thing.values.push_back(std::move(next));
    return OverrideWriteResult::changed;
}

/** Append one local occurrence, preserving repeatable parameter order. */
inline bool appendOverride(
    Thing& thing,
    const std::string_view name,
    const std::string_view textValue,
    const TplValue* semanticSource = nullptr)
{
    return appendOverrideDetailed(
               thing, name, textValue, semanticSource)
        == OverrideWriteResult::changed;
}

/** Edit exactly one local occurrence, rejecting a stale index/name pair. */
inline bool updateOverrideAt(
    Thing& thing,
    const std::size_t valueIndex,
    const std::string_view expectedName,
    const std::string_view textValue)
{
    if (valueIndex >= thing.values.size()
        || !text::iequalsAscii(thing.values[valueIndex].name, expectedName))
    {
        return false;
    }

    TplValue next = thing.values[valueIndex];
    if (!next.parse(textValue))
    {
        return false;
    }
    if (next.asString() == thing.values[valueIndex].asString())
    {
        return false;
    }
    thing.values[valueIndex] = std::move(next);
    return true;
}

/** Remove exactly one local occurrence, rejecting a stale index/name pair. */
inline bool removeOverrideAt(
    Thing& thing,
    const std::size_t valueIndex,
    const std::string_view expectedName)
{
    if (valueIndex >= thing.values.size()
        || !text::iequalsAscii(thing.values[valueIndex].name, expectedName))
    {
        return false;
    }
    thing.values.erase(
        thing.values.begin()
        + static_cast<std::vector<TplValue>::difference_type>(valueIndex));
    return true;
}

inline bool removeOverride(
    Thing& thing,
    const std::string_view name)
{
    const std::size_t before = thing.values.size();
    thing.values.erase(
        std::remove_if(thing.values.begin(), thing.values.end(), [name](const TplValue& value)
            {
                return text::iequalsAscii(value.name, name);
            }),
        thing.values.end());
    return thing.values.size() != before;
}

/** Store a parsed Template Editor value and return its resulting row.
 *
 * A selected row is always edited positionally. An Add operation appends a
 * repeatable parameter, but retains the historical replace-by-name behavior
 * for scalar and unknown parameters.
 */
inline std::size_t applyTemplateParameterEdit(
    ThingTemplate& thingTemplate,
    TplValue value,
    const std::optional<std::size_t> editedIndex,
    const bool repeatable)
{
    if (editedIndex && *editedIndex < thingTemplate.values.size())
    {
        thingTemplate.values[*editedIndex] = std::move(value);
        return *editedIndex;
    }

    if (!repeatable)
    {
        const auto it = std::find_if(
            thingTemplate.values.rbegin(),
            thingTemplate.values.rend(),
            [&value](const TplValue& current)
            {
                return text::iequalsAscii(current.name, value.name);
            });
        if (it != thingTemplate.values.rend())
        {
            const std::size_t index = static_cast<std::size_t>(
                std::distance(thingTemplate.values.begin(), it.base() - 1));
            *it = std::move(value);
            return index;
        }
    }

    thingTemplate.values.push_back(std::move(value));
    return thingTemplate.values.size() - 1;
}

inline bool matchesFilter(
    const EffectiveParameter& parameter,
    const QString& searchText)
{
    const QString needle = searchText.trimmed();
    return needle.isEmpty()
        || QString::fromStdString(parameter.value.name).contains(needle, Qt::CaseInsensitive)
        || QString::fromStdString(parameter.value.asString()).contains(needle, Qt::CaseInsensitive);
}

} // namespace sed::thing::parameters
