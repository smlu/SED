#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <QString>
#include <QStringList>

#include "sed/assets/tpl.hpp"
#include "sed/assets/jlh.hpp"
#include "sed/level.hpp"

namespace sed::ui
{

/**
 * Returns the base template file used by a project type.
 */
std::optional<QString> templateFileNameForProject(ProjectType kind);

/**
 * Finds a repository/app data directory by walking upward from a starting path.
 */
std::optional<std::filesystem::path> findDataRootUpward(const std::filesystem::path& start);

/**
 * Finds a repository/app data directory by walking upward from the current path.
 */
std::optional<std::filesystem::path> findDataRoot();

/**
 * Appends non-empty trimmed template names from level-local templates.
 */
void appendTemplateNames(QStringList& names, const std::vector<ThingTemplate>& templates);

/**
 * Appends non-empty trimmed template names from a loaded template database.
 */
void appendTemplateNames(QStringList& names, const assets::Templates& templates);

/**
 * Deduplicates and sorts template names, then ensures the "none" sentinel exists.
 */
QStringList normalizedTemplatePickerNames(QStringList names);

/**
 * Builds the list shown by simple template pickers from local and optional base templates.
 */
QStringList templatePickerNames(
    const std::vector<ThingTemplate>& localTemplates,
    const assets::Templates* baseTemplates = nullptr);

/**
 * Appends a category and nested subcategories in authored depth-first order.
 * Duplicate resource names retain their first authored position.
 */
void appendTemplateCategoryEntriesInSourceOrder(
    QStringList& names,
    const assets::JlhNode& node);

} // namespace sed::ui
