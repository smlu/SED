#pragma once

#include "thing/thing_parameter_schema.hpp"
#include "sed/text.hpp"
#include "sed/level.hpp"
#include "sed/values.hpp"

#include <QString>
#include <QStringList>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <vector>

namespace sed::ui
{

[[nodiscard]] inline QString templateParameterTypeLabel(const TplValue& value);

struct TemplateFilterCandidate
{
    QString name{};
    QString displayText{};
    QString toolTip{};
    bool categoryMatch{ true };
};

struct TemplateFilterResult
{
    std::vector<bool> visibleRows{};
    int preferredRow{ -1 };
};

struct EffectiveTemplateParameter
{
    TplValue value{};
    std::optional<std::size_t> localIndex{};
    std::optional<TplValue> inheritedValue{};
    std::string inheritedFrom{};
    bool repeatable{};
};

/**
 * Builds Template Editor Source order for the active category. Category/JLL
 * order comes first; non-members remain in their TPL order so filtering can
 * hide them without losing stable source identities.
 */
[[nodiscard]] inline std::vector<std::size_t> templateCategorySourceDisplayOrder(
    const std::vector<ThingTemplate>& templates,
    const QStringList& categoryNames)
{
    std::vector<std::size_t> order{};
    order.reserve(templates.size());
    if (categoryNames.isEmpty())
    {
        for (std::size_t index = 0; index < templates.size(); ++index)
        {
            order.push_back(index);
        }
        return order;
    }

    std::unordered_map<std::string, std::size_t> sourceIndexByName{};
    sourceIndexByName.reserve(templates.size());
    for (std::size_t index = 0; index < templates.size(); ++index)
    {
        sourceIndexByName.try_emplace(
            ::sed::text::upperAscii(::sed::text::trimView(templates[index].name)),
            index);
    }

    std::vector<bool> emitted(templates.size(), false);
    for (const QString& categoryName : categoryNames)
    {
        const std::string key = ::sed::text::upperAscii(
            categoryName.trimmed().toStdString());
        const auto found = sourceIndexByName.find(key);
        if (found != sourceIndexByName.end() && !emitted[found->second])
        {
            emitted[found->second] = true;
            order.push_back(found->second);
        }
    }
    for (std::size_t index = 0; index < templates.size(); ++index)
    {
        if (!emitted[index])
        {
            order.push_back(index);
        }
    }
    return order;
}

namespace detail
{

inline const ThingTemplate* findTemplate(
    const std::vector<ThingTemplate>& templates,
    const std::string_view name) noexcept
{
    const auto it = std::find_if(
        templates.begin(), templates.end(), [name](const ThingTemplate& tpl)
        {
            return ::sed::text::iequalsAscii(tpl.name, name);
        });
    return it == templates.end() ? nullptr : &*it;
}

inline bool containsParameterName(
    const std::vector<std::string>& names,
    const std::string_view name) noexcept
{
    return std::any_of(names.begin(), names.end(), [name](const std::string& current)
        {
            return ::sed::text::iequalsAscii(current, name);
        });
}

} // namespace detail

/** Build the Template Editor's effective parameter surface.
 *
 * Local rows retain their authored order and positional identity. Missing
 * scalar rows are appended from the nearest parent which defines them. A
 * local scalar records the parent value it overrides so Restore can remove
 * all child occurrences and reveal that value. Repeatable parameters are
 * append-only in the engines, so inherited occurrences remain visible in
 * base-to-derived order but are deliberately not presented as replaceable
 * child overrides.
 */
[[nodiscard]] inline std::vector<EffectiveTemplateParameter>
effectiveTemplateParameters(
    const std::vector<ThingTemplate>& templates,
    const std::size_t templateIndex,
    const ProjectType projectType)
{
    if (templateIndex >= templates.size())
    {
        return {};
    }

    const ThingTemplate& selected = templates[templateIndex];
    std::vector<const ThingTemplate*> derivedToBase{};
    std::string parentName = selected.parent;
    for (int depth = 0;
         depth < 1000 && !parentName.empty()
             && !::sed::text::iequalsAscii(parentName, "none");
         ++depth)
    {
        const ThingTemplate* parent = detail::findTemplate(templates, parentName);
        if (!parent
            || std::find(derivedToBase.begin(), derivedToBase.end(), parent)
                != derivedToBase.end()
            || parent == &selected)
        {
            break;
        }
        derivedToBase.push_back(parent);
        parentName = parent->parent;
    }

    const auto inheritedScalar = [&](const std::string_view name)
        -> std::pair<const TplValue*, const ThingTemplate*>
    {
        for (const ThingTemplate* parent : derivedToBase)
        {
            for (auto value = parent->values.rbegin(); value != parent->values.rend(); ++value)
            {
                if (::sed::text::iequalsAscii(value->name, name))
                {
                    return { &*value, parent };
                }
            }
        }
        return {};
    };

    std::vector<EffectiveTemplateParameter> rows{};
    rows.reserve(selected.values.size() + 16u);
    std::vector<std::string> localNames{};
    localNames.reserve(selected.values.size());
    for (std::size_t index = 0; index < selected.values.size(); ++index)
    {
        const TplValue& value = selected.values[index];
        const bool repeatable = thing::parameter_schema::isRepeatableParameter(
            projectType, value.name);
        EffectiveTemplateParameter row{};
        row.value = value;
        row.localIndex = index;
        row.repeatable = repeatable;
        if (!repeatable)
        {
            const auto [inherited, owner] = inheritedScalar(value.name);
            if (inherited && owner)
            {
                row.inheritedValue = *inherited;
                row.inheritedFrom = owner->name;
            }
        }
        rows.push_back(std::move(row));
        if (!detail::containsParameterName(localNames, value.name))
        {
            localNames.push_back(value.name);
        }
    }

    std::vector<std::string> appendedScalarNames = localNames;
    for (const ThingTemplate* parent : derivedToBase)
    {
        for (std::size_t index = 0; index < parent->values.size(); ++index)
        {
            const TplValue& value = parent->values[index];
            const bool shadowedInSameTemplate = std::any_of(
                parent->values.begin()
                    + static_cast<std::vector<TplValue>::difference_type>(index + 1u),
                parent->values.end(), [&value](const TplValue& later)
                {
                    return ::sed::text::iequalsAscii(later.name, value.name);
                });
            if (shadowedInSameTemplate
                || thing::parameter_schema::isRepeatableParameter(projectType, value.name)
                || detail::containsParameterName(appendedScalarNames, value.name))
            {
                continue;
            }
            rows.push_back(EffectiveTemplateParameter{
                value, std::nullopt, value, parent->name, false });
            appendedScalarNames.push_back(value.name);
        }
    }

    for (auto parent = derivedToBase.rbegin(); parent != derivedToBase.rend(); ++parent)
    {
        for (const TplValue& value : (*parent)->values)
        {
            if (!thing::parameter_schema::isRepeatableParameter(projectType, value.name))
            {
                continue;
            }
            rows.push_back(EffectiveTemplateParameter{
                value, std::nullopt, value, (*parent)->name, true });
        }
    }
    return rows;
}

[[nodiscard]] inline QString templateParameterDisplayTypeLabel(
    const ProjectType projectType,
    const TplValue& value)
{
    if (::sed::text::iequalsAscii(value.name, "light"))
    {
        return projectType == ProjectType::IJIM
            ? QStringLiteral("vec4 tuple (RGB / range)")
            : QStringLiteral("scalar intensity");
    }
    if (::sed::text::iequalsAscii(value.name, "lightintensity"))
    {
        return projectType == ProjectType::IJIM
            ? QStringLiteral("vec3 tuple (RGB)")
            : QStringLiteral("scalar intensity");
    }
    return templateParameterTypeLabel(value);
}

/** Decide visibility and the stable current row for the Template Editor.
 *
 * Exact template-name matches take precedence over incidental matches in a
 * parent/type caption or description tooltip. If filtering cleared Qt's
 * current index, the first visible result becomes current again.
 */
[[nodiscard]] inline TemplateFilterResult filterTemplateCandidates(
    const std::vector<TemplateFilterCandidate>& candidates,
    const QString& searchText,
    const int currentRow)
{
    TemplateFilterResult result{};
    result.visibleRows.resize(candidates.size(), false);

    const QString needle = searchText.trimmed();
    int firstVisibleRow = -1;
    int exactNameRow = -1;
    for (std::size_t row = 0; row < candidates.size(); ++row)
    {
        const TemplateFilterCandidate& candidate = candidates[row];
        const bool exactName = !needle.isEmpty()
            && candidate.name.trimmed().compare(needle, Qt::CaseInsensitive) == 0;
        const bool matchesSearch = needle.isEmpty()
            || candidate.displayText.contains(needle, Qt::CaseInsensitive)
            || candidate.toolTip.contains(needle, Qt::CaseInsensitive);
        const bool visible = candidate.categoryMatch && matchesSearch;
        result.visibleRows[row] = visible;
        if (visible && firstVisibleRow < 0)
        {
            firstVisibleRow = static_cast<int>(row);
        }
        if (visible && exactName && exactNameRow < 0)
        {
            exactNameRow = static_cast<int>(row);
        }
    }

    const bool currentVisible = currentRow >= 0
        && static_cast<std::size_t>(currentRow) < result.visibleRows.size()
        && result.visibleRows[static_cast<std::size_t>(currentRow)];
    result.preferredRow = exactNameRow >= 0
        ? exactNameRow
        : (currentVisible ? currentRow : firstVisibleRow);
    return result;
}

[[nodiscard]] inline QString templateParameterTypeLabel(
    const ValueType valueType,
    const AdjType adjType)
{
    QStringList parts{};
    const std::string_view valueTypeName = ::sed::toString(valueType);
    parts.push_back(QString::fromUtf8(
        valueTypeName.data(), static_cast<int>(valueTypeName.size())));
    if (adjType != AdjType::unk)
    {
        const std::string_view adjTypeName = ::sed::toString(adjType);
        parts.push_back(QString::fromUtf8(
            adjTypeName.data(), static_cast<int>(adjTypeName.size())));
    }
    return parts.join(QStringLiteral(" / "));
}

/** Display the semantics carried by the parsed parameter, not a guess based
 * only on its name. This matters for custom/engine flag fields stored as hex.
 */
[[nodiscard]] inline QString templateParameterTypeLabel(const TplValue& value)
{
    return templateParameterTypeLabel(value.type, value.adjType);
}

/** Parse an edited parameter while preserving its existing typed semantics
 * when the parameter name did not change. New/renamed parameters continue to
 * use the canonical name-based parser mapping.
 */
[[nodiscard]] inline std::optional<TplValue> parseTemplateParameterValue(
    const QString& parameterName,
    const QString& parameterValue,
    const TplValue* existingValue = nullptr)
{
    const std::string name = parameterName.trimmed().toStdString();
    if (name.empty())
    {
        return std::nullopt;
    }

    TplValue value{};
    value.name = name;
    if (existingValue && ::sed::text::iequalsAscii(existingValue->name, name))
    {
        value.type = existingValue->type;
        value.adjType = existingValue->adjType;
    }
    else
    {
        value.type = ::sed::tplValueTypeForName(name);
        value.adjType = ::sed::tplAdjTypeForName(name);
    }
    if (!value.parse(parameterValue.trimmed().toStdString()))
    {
        return std::nullopt;
    }
    return value;
}

} // namespace sed::ui
