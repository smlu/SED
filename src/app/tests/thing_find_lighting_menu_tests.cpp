#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "ui/query/level_query_utils.hpp"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

#include <algorithm>

namespace sed::app_tests
{

void runThingFindLightingMenuTests()
{
    using namespace ui::query;

    const auto& thingFields = levelQueryFieldChoices(LevelQueryEntityKind::thing);
    const auto templateField = std::find_if(
        thingFields.begin(), thingFields.end(), [](const auto& field)
        {
            return field.first == LevelQueryField::thingTemplate
                && field.second == QStringLiteral("Template");
        });
    check(templateField != thingFields.end(),
        "Thing Find exposes Template as a searchable field");
    check(levelQueryPickerKind(LevelQueryField::thingTemplate)
                == ui::ResourcePickerKind::template_
            && levelQueryBrowseTitle(LevelQueryField::thingTemplate)
                == QStringLiteral("Choose Template"),
        "Thing Find routes the Template field to the Template resource picker");

    Level level{};
    Thing byTemplate{};
    byTemplate.name = "instance_named_otherwise";
    byTemplate.templateName = "wanted_template";
    Thing byInstanceName{};
    byInstanceName.name = "wanted_template";
    byInstanceName.templateName = "different_template";
    level.things = { byTemplate, byInstanceName };

    LevelQuerySpec templateQuery{};
    templateQuery.kind = LevelQueryEntityKind::thing;
    templateQuery.field = LevelQueryField::thingTemplate;
    templateQuery.matchMode = LevelQueryMatchMode::exact;
    templateQuery.value = "wanted_template";
    const std::vector<LevelQueryHit> hits = collectLevelQueryHits(
        level, nullptr, templateQuery);
    check(hits.size() == 1
            && hits.front().kind == OutlinerKind::thing
            && hits.front().index == 0,
        "Thing Find matches the template name rather than the instance name");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setProjectTypeForTest(ProjectType::MOTS);
    window.activateThingEditModeForTest();
    window.updateEditActionsForTest();
    window.show();
    window.activateWindow();
    window.raise();
    QApplication::setActiveWindow(&window);
    QApplication::processEvents();

    QAction* const findAction = window.findChild<QAction*>(
        QStringLiteral("edit.find"));
    QAction* const recalculateAction = window.findChild<QAction*>(
        QStringLiteral("edit.recalculate_lighting"));
    QAction* const architecturalModeAction = window.findChild<QAction*>(
        QStringLiteral("viewport.light_mode.architectural"));
    check(findAction != nullptr,
        "Edit menu retains the Find command");
    check(recalculateAction != nullptr,
        "Edit Light menu retains the baked-light recalculation command");
    if (recalculateAction)
    {
        check(recalculateAction->text() == QStringLiteral("Recalculate &Lighting"),
            "baked-light recalculation uses the current Recalculate Lighting label");
    }
    check(window.findChild<QAction*>(QStringLiteral("edit.arch_lighting")) == nullptr
            && window.findChild<QAction*>(
                QStringLiteral("edit.arch_lighting.edit_vertices")) == nullptr,
        "Edit Light menu omits the bulk Arch dialog and redundant Edit selected vertices action");
    check(architecturalModeAction && architecturalModeAction->isEnabled(),
        "MotS viewport Arch editing remains available after the bulk menu path is removed");

    if (!findAction)
    {
        return;
    }

    bool sawFindDialog = false;
    bool selectedThingKind = false;
    bool selectedTemplateField = false;
    bool clickedTemplateEntryButton = false;
    bool waitingForPicker = false;
    bool sawNestedPicker = false;
    bool sawTemplatePicker = false;
    bool pickerDismissed = false;
    bool closedFindDialog = false;
    bool timedOut = false;
    int timerTicks = 0;

    const auto findVisibleDialog = [&window](const QString& title) -> QDialog*
    {
        const QList<QDialog*> dialogs = window.findChildren<QDialog*>();
        for (QDialog* const candidate : dialogs)
        {
            if (candidate && candidate->isVisible()
                && candidate->windowTitle() == title)
            {
                return candidate;
            }
        }
        return nullptr;
    };

    QTimer dialogDriver{};
    dialogDriver.setInterval(5);
    QObject::connect(&dialogDriver, &QTimer::timeout, &window, [&]()
        {
            if (waitingForPicker)
            {
                return;
            }
            ++timerTicks;
            if (timerTicks > 600)
            {
                timedOut = true;
                if (auto* const active = qobject_cast<QDialog*>(
                        QApplication::activeModalWidget()))
                {
                    active->reject();
                }
                const QList<QDialog*> dialogs = window.findChildren<QDialog*>();
                for (QDialog* const candidate : dialogs)
                {
                    if (candidate && candidate->isVisible())
                    {
                        candidate->reject();
                    }
                }
                return;
            }

            QDialog* const dialog = findVisibleDialog(
                QStringLiteral("Find things"));
            if (!dialog)
            {
                return;
            }
            sawFindDialog = true;
            selectedThingKind = true;

            const QList<QLabel*> labels = dialog->findChildren<QLabel*>();
            selectedTemplateField = std::any_of(
                labels.begin(), labels.end(), [](const QLabel* const label)
                {
                    return label->text() == QStringLiteral("Template");
                });
            if (!selectedTemplateField)
            {
                return;
            }

            for (QLabel* const label : labels)
            {
                if (label->text() != QStringLiteral("Template"))
                {
                    continue;
                }
                QWidget* const row = label->parentWidget();
                const QList<QPushButton*> buttons = row
                    ? row->findChildren<QPushButton*>()
                    : QList<QPushButton*>{};
                for (QPushButton* const button : buttons)
                {
                    if (button->text() != QStringLiteral("..."))
                    {
                        continue;
                    }
                    check(button->isEnabled(),
                        "Thing Find enables the Template entry button");
                    clickedTemplateEntryButton = button->isEnabled();
                    if (!clickedTemplateEntryButton)
                    {
                        continue;
                    }

                    waitingForPicker = true;
                    QObject::connect(dialog, &QDialog::finished, &window,
                        [&, queryDialog = dialog](int)
                        {
                            closedFindDialog = pickerDismissed
                                && !queryDialog->isVisible();
                        });
                    QTimer::singleShot(100, &window,
                        [&, queryDialog = dialog]()
                        {
                            QDialog* nested = qobject_cast<QDialog*>(
                                QApplication::activeModalWidget());
                            if (!nested || nested == queryDialog)
                            {
                                const QWidgetList topLevelWidgets =
                                    QApplication::topLevelWidgets();
                                for (QWidget* const topLevel : topLevelWidgets)
                                {
                                    QDialog* const candidate =
                                        qobject_cast<QDialog*>(topLevel);
                                    if (candidate && candidate != queryDialog
                                        && candidate->isVisible())
                                    {
                                        nested = candidate;
                                        break;
                                    }
                                }
                            }

                            if (!nested || nested == queryDialog)
                            {
                                timedOut = true;
                                queryDialog->reject();
                                return;
                            }

                            sawNestedPicker = true;
                            sawTemplatePicker = nested->windowTitle()
                                == QStringLiteral("Choose Template");
                            QObject::connect(nested, &QDialog::finished, &window,
                                [&, queryDialog](int)
                                {
                                    pickerDismissed = true;
                                    queryDialog->reject();
                                });
                            nested->reject();
                        });
                    button->click();
                    return;
                }
            }
        });

    dialogDriver.start();
    findAction->trigger();
    dialogDriver.stop();

    check(sawFindDialog && selectedThingKind && selectedTemplateField,
        "Find dialog can target the Thing Template field");
    check(clickedTemplateEntryButton && sawTemplatePicker,
        "Thing Find's Template entry button opens the Template picker");
    check(closedFindDialog && !timedOut,
        "Thing Find closes its query dialog after the Template picker is dismissed");
}

} // namespace sed::app_tests
