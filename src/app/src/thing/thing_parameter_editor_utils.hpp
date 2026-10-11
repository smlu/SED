#pragma once

#include "thing/thing_parameter_schema.hpp"
#include "ui/components/dialog_window_policy.hpp"
#include "ui/query/level_query_utils.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <QComboBox>
#include <QCompleter>
#include <QInputDialog>
#include <QLineEdit>
#include <QStringList>

namespace sed::thing::parameter_editor
{

enum class ValueEditorKind
{
    none,
    enumChoice,
    flags,
};

struct ValueEditorInfo
{
    ValueEditorKind kind{ ValueEditorKind::none };
    QString buttonText{};
    QString toolTip{};
};

/** Bits which a schema-backed flag picker is allowed to author.
 *
 * Composite aliases do not represent an independent checkbox, while
 * runtime-managed bits are deliberately presented read-only.  Excluding both
 * here gives mixed-Thing edits one authoritative protection mask.
 */
inline std::uint32_t editableFlagEditMask(
    const std::span<const parameter_schema::FlagDefinition> definitions) noexcept
{
    std::uint32_t mask = 0u;
    for (const parameter_schema::FlagDefinition& definition : definitions)
    {
        if (definition.kind == parameter_schema::FlagKind::singleBit
            && !definition.runtimeManaged)
        {
            mask |= definition.mask;
        }
    }
    return mask;
}

/** Apply one picker edit to a target Thing without copying protected bits.
 *
 * The editor's before/after values describe user intent.  Only changed,
 * independently editable schema bits are transferred to the target; its own
 * unknown, composite-only, and runtime-managed bits remain intact.  A null
 * result means that the edit has no authorable delta or that the target
 * already has the merged value, so callers should not materialize an override
 * or create an undo record.
 */
inline std::optional<std::uint32_t> applyProtectedFlagEditDelta(
    const std::uint32_t targetValue,
    const std::uint32_t editorBefore,
    const std::uint32_t editorAfter,
    const std::span<const parameter_schema::FlagDefinition> definitions) noexcept
{
    const std::uint32_t changedBits = (editorBefore ^ editorAfter)
        & editableFlagEditMask(definitions);
    if (changedBits == 0u)
    {
        return std::nullopt;
    }

    const std::uint32_t merged = (targetValue & ~changedBits)
        | (editorAfter & changedBits);
    return merged == targetValue
        ? std::nullopt
        : std::optional<std::uint32_t>{ merged };
}

/**
 * Project-aware engine parameter names plus caller-supplied custom names.
 * The returned list is suitable for completion only: editors deliberately
 * remain editable so future, mod-specific, and otherwise unknown names work.
 */
inline QStringList completionNameOptions(
    const ProjectType project,
    QStringList additionalNames = {})
{
    for (const std::string_view name : parameter_schema::completionNames(project))
    {
        additionalNames.push_back(QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())));
    }

    additionalNames.removeAll(QString{});
    additionalNames.removeDuplicates();
    std::sort(additionalNames.begin(), additionalNames.end(), [](const QString& lhs, const QString& rhs)
        {
            return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
        });
    return additionalNames;
}

inline QCompleter* installNameCompleter(
    QLineEdit* edit,
    const QStringList& names,
    const QString& objectName = {})
{
    if (!edit)
    {
        return nullptr;
    }

    auto* completer = new QCompleter(names, edit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setFilterMode(Qt::MatchContains);
    completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    if (!objectName.isEmpty())
    {
        completer->setObjectName(objectName);
    }
    edit->setCompleter(completer);
    return completer;
}

inline QCompleter* installNameCompleter(
    QComboBox* box,
    const QStringList& names,
    const QString& objectName = {})
{
    if (!box)
    {
        return nullptr;
    }
    box->setEditable(true);
    QCompleter* completer = installNameCompleter(box->lineEdit(), names, objectName);
    box->setCompleter(completer);
    return completer;
}

inline ValueEditorInfo valueEditorInfo(
    const ProjectType project,
    const std::string_view parameterName,
    const std::string_view effectiveThingType = {})
{
    if (!parameter_schema::enumChoices(project, parameterName).empty())
    {
        return {
            ValueEditorKind::enumChoice,
            QStringLiteral("Choose..."),
            QStringLiteral("Choose a known engine value; manual and unknown values remain allowed."),
        };
    }

    const std::optional<parameter_schema::FlagFamily> family =
        parameter_schema::flagFamily(parameterName, effectiveThingType);
    if (!family || parameter_schema::flagDefinitions(project, *family).empty())
    {
        return {};
    }

    const QString familyName = QString::fromLatin1(
        parameter_schema::flagFamilyName(*family).data(),
        static_cast<qsizetype>(parameter_schema::flagFamilyName(*family).size()));
    return {
        ValueEditorKind::flags,
        QStringLiteral("Flags..."),
        QStringLiteral("Edit %1 flags; unrecognized bits are preserved.").arg(familyName),
    };
}

/** Build the rows shown by the flag picker.
 *
 * Engine enum identifiers remain useful schema provenance, but are deliberately
 * not exposed in descriptions: the editor should explain behavior in level-
 * authoring terms rather than repeat implementation symbols such as SITH_PF_*.
 */
inline std::vector<ui::query::LevelQueryFlagDef> flagEditorDefinitions(
    const std::span<const parameter_schema::FlagDefinition> schemaDefinitions)
{
    std::vector<ui::query::LevelQueryFlagDef> definitions{};
    definitions.reserve(schemaDefinitions.size());
    for (const parameter_schema::FlagDefinition& definition : schemaDefinitions)
    {
        // Composite names such as IJIM IMP5 alias multiple independent bits.
        // They remain documented in the schema/tests but are not rendered as
        // misleading checkbox rows; users edit their constituent bits.
        if (definition.kind == parameter_schema::FlagKind::compositePreset)
        {
            continue;
        }

        QString description = QString::fromLatin1(
            definition.description.data(), static_cast<qsizetype>(definition.description.size()));
        if (definition.runtimeManaged)
        {
            description += QStringLiteral(" Runtime-managed state; shown read-only.");
        }

        QString name = QString::fromLatin1(
            definition.name.data(), static_cast<qsizetype>(definition.name.size()));
        if (name.startsWith(QStringLiteral("Unknown"), Qt::CaseInsensitive))
        {
            name = QStringLiteral("Unknown");
        }
        else if (name.startsWith(QStringLiteral("Unused"), Qt::CaseInsensitive))
        {
            name = QStringLiteral("Unused");
        }

        definitions.push_back(ui::query::LevelQueryFlagDef{
            definition.mask,
            std::move(name),
            std::move(description),
            !definition.runtimeManaged,
        });
    }
    return definitions;
}

inline std::optional<QString> openValueEditor(
    QWidget* parent,
    const ProjectType project,
    const std::string_view parameterName,
    const std::string_view effectiveThingType,
    const QString& currentValue,
    QWidget* anchor = nullptr)
{
    const std::vector<parameter_schema::EnumChoice> choices =
        parameter_schema::enumChoices(project, parameterName);
    if (!choices.empty())
    {
        QStringList labels{};
        labels.reserve(static_cast<qsizetype>(choices.size()));
        int currentIndex = 0;
        for (std::size_t index = 0; index < choices.size(); ++index)
        {
            const parameter_schema::EnumChoice& choice = choices[index];
            const QString value = QString::fromLatin1(
                choice.value.data(), static_cast<qsizetype>(choice.value.size()));
            const QString name = QString::fromLatin1(
                choice.name.data(), static_cast<qsizetype>(choice.name.size()));
            labels.push_back(QStringLiteral("%1 (%2)").arg(name, value));
            if (value.compare(currentValue.trimmed(), Qt::CaseInsensitive) == 0)
            {
                currentIndex = static_cast<int>(index);
            }
        }

        const QString parameter = QString::fromLatin1(
            parameterName.data(), static_cast<qsizetype>(parameterName.size()));
        QInputDialog dialog(parent);
        dialog.setWindowTitle(QStringLiteral("Choose %1").arg(parameter));
        dialog.setLabelText(QStringLiteral("Value"));
        dialog.setComboBoxItems(labels);
        dialog.setComboBoxEditable(false);
        dialog.setTextValue(labels.value(currentIndex));
        ::sed::ui::applyDialogWindowPolicy(dialog);
        dialog.adjustSize();
        ::sed::ui::positionDialogNearWidget(dialog, anchor);
        if (dialog.exec() != QDialog::Accepted)
        {
            return std::nullopt;
        }

        const QString picked = dialog.textValue();
        const int pickedIndex = labels.indexOf(picked);
        if (pickedIndex < 0 || static_cast<std::size_t>(pickedIndex) >= choices.size())
        {
            return std::nullopt;
        }
        const std::string_view value = choices[static_cast<std::size_t>(pickedIndex)].value;
        return QString::fromLatin1(value.data(), static_cast<qsizetype>(value.size()));
    }

    const std::optional<parameter_schema::FlagFamily> family =
        parameter_schema::flagFamily(parameterName, effectiveThingType);
    if (!family)
    {
        return std::nullopt;
    }

    const std::vector<parameter_schema::FlagDefinition> schemaDefinitions =
        parameter_schema::flagDefinitions(project, *family);
    if (schemaDefinitions.empty())
    {
        return std::nullopt;
    }

    std::vector<ui::query::LevelQueryFlagDef> definitions =
        flagEditorDefinitions(schemaDefinitions);

    const std::string_view familyLabel = parameter_schema::flagFamilyName(*family);
    return ui::query::openLevelQueryFlagEditor(
        parent,
        QStringLiteral("Edit %1 flags").arg(QString::fromLatin1(
            familyLabel.data(), static_cast<qsizetype>(familyLabel.size()))),
        currentValue,
        std::move(definitions),
        anchor);
}

} // namespace sed::thing::parameter_editor
