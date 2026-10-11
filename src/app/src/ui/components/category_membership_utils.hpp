#pragma once

#include "sed/assets/jlh.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include <QString>
#include <QStringList>
#include <Qt>

namespace sed::ui::detail
{

inline bool categoryMembershipContains(const assets::JllList& list, const QString& value)
{
    const QString normalized = value.trimmed();
    if (normalized.isEmpty())
    {
        return false;
    }

    for (const std::string& existing : list.entries)
    {
        if (QString::fromStdString(existing).compare(normalized, Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }
    return false;
}

inline void removeCategoryMembershipEntry(assets::JllList& list, const QString& value)
{
    const QString normalized = value.trimmed();
    if (normalized.isEmpty())
    {
        return;
    }

    list.entries.erase(
        std::remove_if(
            list.entries.begin(),
            list.entries.end(),
            [&](const std::string& existing)
            {
                return QString::fromStdString(existing).compare(normalized, Qt::CaseInsensitive) == 0;
            }),
        list.entries.end());
}

inline const assets::JllList* assignableCategoryMembershipList(const assets::JlhNode& node) noexcept
{
    if (node.list)
    {
        return node.list.get();
    }

    for (const assets::JlhNode& child : node.children)
    {
        if (child.list
            && QString::fromStdString(child.label).compare(QStringLiteral("General"), Qt::CaseInsensitive) == 0)
        {
            return child.list.get();
        }
    }
    return nullptr;
}

inline bool isInternalGeneralCategoryMembershipNode(const assets::JlhNode& node) noexcept
{
    return node.list
        && node.children.empty()
        && QString::fromStdString(node.label).compare(QStringLiteral("General"), Qt::CaseInsensitive) == 0;
}

inline bool categoryMembershipNodesShareStorage(
    const assets::JlhNode& first,
    const assets::JlhNode& second) noexcept
{
    const assets::JllList* firstList = assignableCategoryMembershipList(first);
    return firstList != nullptr && firstList == assignableCategoryMembershipList(second);
}

template <typename ListNameFactory>
bool mutateCategoryMembership(
    assets::JlhNode& node,
    const QStringList& names,
    const bool remove,
    ListNameFactory&& makeListName)
{
    std::shared_ptr<assets::JllList> list{};
    if (node.list)
    {
        list = std::const_pointer_cast<assets::JllList>(node.list);
    }
    else
    {
        for (assets::JlhNode& child : node.children)
        {
            if (child.list
                && QString::fromStdString(child.label).compare(QStringLiteral("General"), Qt::CaseInsensitive) == 0)
            {
                list = std::const_pointer_cast<assets::JllList>(child.list);
                break;
            }
        }

        if (!list && !remove)
        {
            auto newList = std::make_shared<assets::JllList>();
            newList->name = std::invoke(std::forward<ListNameFactory>(makeListName));
            node.children.push_back(assets::JlhNode{ "General", newList, {} });
            list = std::move(newList);
        }
    }
    if (!list)
    {
        return false;
    }

    bool changed = false;
    for (const QString& rawName : names)
    {
        const QString name = rawName.trimmed();
        if (name.isEmpty())
        {
            continue;
        }

        if (remove)
        {
            const std::size_t before = list->entries.size();
            removeCategoryMembershipEntry(*list, name);
            changed = changed || list->entries.size() != before;
        }
        else if (!categoryMembershipContains(*list, name))
        {
            list->entries.push_back(name.toStdString());
            changed = true;
        }
    }
    return changed;
}

template <typename ListNameFactory>
bool moveCategoryMembership(
    assets::JlhNode& source,
    assets::JlhNode& target,
    const QStringList& names,
    ListNameFactory&& makeTargetListName)
{
    // A JLH tree may expose the same JLL list through more than one category
    // node. Adding to one alias and then removing from the other would remove
    // the membership from both, so such a move must remain a no-op.
    if (categoryMembershipNodesShareStorage(source, target))
    {
        return false;
    }

    bool changed = mutateCategoryMembership(
        target,
        names,
        false,
        std::forward<ListNameFactory>(makeTargetListName));
    changed = mutateCategoryMembership(
        source,
        names,
        true,
        []()
        {
            return std::string{};
        }) || changed;
    return changed;
}

inline assets::JlhTree cloneCategoryMembershipTree(const assets::JlhTree& source)
{
    std::unordered_map<const assets::JllList*, assets::JllListPtr> clonedLists{};
    std::function<assets::JlhNode(const assets::JlhNode&)> cloneNode =
        [&](const assets::JlhNode& sourceNode) -> assets::JlhNode
    {
        assets::JlhNode clone{};
        clone.label = sourceNode.label;
        if (sourceNode.list)
        {
            const assets::JllList* identity = sourceNode.list.get();
            const auto existing = clonedLists.find(identity);
            if (existing != clonedLists.end())
            {
                clone.list = existing->second;
            }
            else
            {
                auto clonedList = std::make_shared<assets::JllList>(*sourceNode.list);
                clone.list = clonedList;
                clonedLists.emplace(identity, std::move(clonedList));
            }
        }

        clone.children.reserve(sourceNode.children.size());
        for (const assets::JlhNode& child : sourceNode.children)
        {
            clone.children.push_back(cloneNode(child));
        }
        return clone;
    };

    assets::JlhTree clone{};
    clone.roots.reserve(source.roots.size());
    for (const assets::JlhNode& root : source.roots)
    {
        clone.roots.push_back(cloneNode(root));
    }
    return clone;
}

template <typename MutateFn, typename SaveFn>
bool commitCategoryMembershipMutation(
    assets::JlhTree& tree,
    MutateFn&& mutate,
    SaveFn&& save)
{
    assets::JlhTree working = cloneCategoryMembershipTree(tree);
    assets::JlhTree before = std::move(tree);
    tree = std::move(working);
    try
    {
        if (!std::invoke(std::forward<MutateFn>(mutate)))
        {
            tree = std::move(before);
            return false;
        }
        if (std::invoke(std::forward<SaveFn>(save)))
        {
            return true;
        }
    }
    catch (...)
    {
        tree = std::move(before);
        throw;
    }

    tree = std::move(before);
    return false;
}

inline Qt::DropAction resolveCategoryMembershipDropAction(
    const bool targetIsRoot,
    const QString& targetPath,
    const QString& sourcePath,
    const QStringList& names,
    const Qt::KeyboardModifiers modifiers)
{
    const bool hasName = std::any_of(names.cbegin(), names.cend(), [](const QString& name)
        {
            return !name.trimmed().isEmpty();
        });
    const QString normalizedTarget = targetPath.trimmed();
    if (targetIsRoot || normalizedTarget.isEmpty() || !hasName)
    {
        return Qt::IgnoreAction;
    }

    if ((modifiers & Qt::ControlModifier) == 0)
    {
        return Qt::CopyAction;
    }

    const QString normalizedSource = sourcePath.trimmed();
    if (normalizedSource.isEmpty() || normalizedSource == normalizedTarget)
    {
        return Qt::IgnoreAction;
    }
    return Qt::MoveAction;
}

} // namespace sed::ui::detail
