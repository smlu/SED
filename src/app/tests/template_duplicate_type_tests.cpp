#include "test_common.hpp"

#include "main_window/main_window.hpp"
#include "sed/assets/tpl.hpp"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QListWidget>
#include <QListWidgetItem>

#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace sed
{
std::vector<ThingTemplate> templateEditorTemplatesForTest(QDialog* dialog);
}

namespace sed::app_tests
{
namespace
{

TplValue stringField(const std::string& name, const std::string& value)
{
    TplValue field{};
    field.name = name;
    field.type = ValueType::str;
    field.stringValue = value;
    return field;
}

const ThingTemplate* findTemplate(
    const std::vector<ThingTemplate>& templates,
    const std::string_view name)
{
    for (const ThingTemplate& tpl : templates)
    {
        if (tpl.name == name)
        {
            return &tpl;
        }
    }
    return nullptr;
}

std::vector<std::string> localTypeValues(const ThingTemplate& tpl)
{
    std::vector<std::string> values{};
    for (const TplValue& value : tpl.values)
    {
        if (value.name == "type")
        {
            values.push_back(value.asString());
        }
    }
    return values;
}

std::optional<std::string> effectiveType(
    const std::vector<ThingTemplate>& templates,
    const std::string_view templateName)
{
    assets::Templates database{};
    for (const ThingTemplate& tpl : templates)
    {
        (void)database.addOrUpdate(tpl, true);
    }
    if (const TplValue* value = database.findField(templateName, "type"))
    {
        return value->asString();
    }
    return std::nullopt;
}

std::string serializeTemplate(const ThingTemplate& tpl)
{
    assets::Templates database{};
    (void)database.addOrUpdate(tpl, true);
    std::ostringstream output{};
    assets::saveTemplates(output, database);
    return output.str();
}

QListWidgetItem* findTemplateItem(
    QListWidget* list,
    const QString& templateName)
{
    for (int row = 0; list && row < list->count(); ++row)
    {
        QListWidgetItem* item = list->item(row);
        if (item && item->data(Qt::UserRole).toString() == templateName)
        {
            return item;
        }
    }
    return nullptr;
}

void refreshTemplateList(QComboBox* sortCombo)
{
    if (!sortCombo || sortCombo->count() < 2)
    {
        return;
    }
    sortCombo->setCurrentIndex(sortCombo->currentIndex() == 0 ? 1 : 0);
    QCoreApplication::processEvents();
}

} // namespace

void runTemplateDuplicateTypeTests()
{
    constexpr char kChildName[] = "duplicate_type_child";
    constexpr char kParentName[] = "duplicate_type_base";

    ThingTemplate child{};
    child.name = kChildName;
    child.parent = kParentName;
    child.values = {
        stringField("type", "actor"),
        stringField("type", "cog"),
    };

    ThingTemplate parent{};
    parent.name = kParentName;
    parent.parent = "none";
    parent.values = { stringField("type", "free") };

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    std::unique_ptr<QDialog> editor(window.createTemplateEditorDialogForTest(
        { child, parent }, ProjectType::JKDF2, {}, assets::ResourceLocator{}));
    check(editor != nullptr,
        "Template Editor duplicate-type test creates the real dialog");
    if (!editor)
    {
        return;
    }

    QComboBox* typeBox = nullptr;
    for (QComboBox* combo : editor->findChildren<QComboBox*>())
    {
        if (combo && combo->isEditable()
            && combo->findText(QStringLiteral("weapon")) >= 0
            && combo->findText(QStringLiteral("cog")) >= 0)
        {
            typeBox = combo;
            break;
        }
    }
    auto* templateList = editor->findChild<QListWidget*>(
        QStringLiteral("templateEditorTemplateList"));
    auto* sortCombo = editor->findChild<QComboBox*>(
        QStringLiteral("templateEditorSort"));
    check(typeBox && templateList && sortCombo,
        "Template Editor exposes its editable Thing Type, template preview list, and refresh sort control");
    if (!typeBox || !templateList || !sortCombo)
    {
        return;
    }

    const std::vector<ThingTemplate> initialTemplates =
        templateEditorTemplatesForTest(editor.get());
    const ThingTemplate* initialChild = findTemplate(initialTemplates, kChildName);
    const QListWidgetItem* initialPreview = findTemplateItem(
        templateList, QString::fromLatin1(kChildName));
    check(typeBox->currentText() == QStringLiteral("cog")
            && initialPreview
            && initialPreview->text().contains(QStringLiteral("[cog]"))
            && initialPreview->toolTip().contains(QStringLiteral("Thing Type: cog"))
            && effectiveType(initialTemplates, kChildName)
                == std::optional<std::string>{ "cog" },
        "Template Editor type field and template preview use the last duplicate type token");
    check(initialChild
            && localTypeValues(*initialChild)
                == std::vector<std::string>{ "actor", "cog" }
            && initialChild
            && serializeTemplate(*initialChild)
                == "# DESC: \n# BBOX: 0 0 0 0 0 0\n"
                   "duplicate_type_child duplicate_type_base type=actor type=cog\n",
        "Duplicate-type fixture begins with the exact ordered TPL tokens");

    typeBox->setCurrentText(QStringLiteral("weapon"));
    QCoreApplication::processEvents();
    refreshTemplateList(sortCombo);

    const std::vector<ThingTemplate> editedTemplates =
        templateEditorTemplatesForTest(editor.get());
    const ThingTemplate* editedChild = findTemplate(editedTemplates, kChildName);
    const QListWidgetItem* editedPreview = findTemplateItem(
        templateList, QString::fromLatin1(kChildName));
    check(typeBox->currentText() == QStringLiteral("weapon")
            && editedPreview
            && editedPreview->text().contains(QStringLiteral("[weapon]"))
            && editedPreview->toolTip().contains(QStringLiteral("Thing Type: weapon"))
            && effectiveType(editedTemplates, kChildName)
                == std::optional<std::string>{ "weapon" },
        "Editing Thing Type updates the effective type and refreshed template preview");
    check(editedChild
            && localTypeValues(*editedChild)
                == std::vector<std::string>{ "actor", "weapon" }
            && editedChild
            && serializeTemplate(*editedChild)
                == "# DESC: \n# BBOX: 0 0 0 0 0 0\n"
                   "duplicate_type_child duplicate_type_base type=actor type=weapon\n",
        "Editing Thing Type writes the effective token to the ordered TPL serialization");

    typeBox->setCurrentText(QStringLiteral("none"));
    QCoreApplication::processEvents();
    refreshTemplateList(sortCombo);

    const std::vector<ThingTemplate> clearedTemplates =
        templateEditorTemplatesForTest(editor.get());
    const ThingTemplate* clearedChild = findTemplate(clearedTemplates, kChildName);
    const QListWidgetItem* inheritedPreview = findTemplateItem(
        templateList, QString::fromLatin1(kChildName));
    check(typeBox->currentText() == QStringLiteral("free")
            && inheritedPreview
            && inheritedPreview->text().contains(QStringLiteral("[free]"))
            && inheritedPreview->toolTip().contains(QStringLiteral("Thing Type: free"))
            && effectiveType(clearedTemplates, kChildName)
                == std::optional<std::string>{ "free" },
        "Clearing duplicate local type tokens reveals the inherited type in the editor and preview");
    check(clearedChild
            && localTypeValues(*clearedChild).empty()
            && clearedChild
            && serializeTemplate(*clearedChild)
                == "# DESC: \n# BBOX: 0 0 0 0 0 0\n"
                   "duplicate_type_child duplicate_type_base\n",
        "Clearing Thing Type removes every local duplicate override from TPL serialization");
}

} // namespace sed::app_tests
