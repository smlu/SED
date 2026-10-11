#include "app_settings.hpp"
#include "main_window/main_window.hpp"
#include "test_common.hpp"
#include "ui/checks/level_check_utils.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

namespace sed::app_tests
{
void runConsistencyPrecisionDialogTests()
{
    using namespace ui::checks;
    struct RestoreSettings
    {
        QMap<QString, QVariant> values;
        RestoreSettings()
        {
            QSettings settings = settingsStore();
            for ( const QString& key : settings.allKeys() )
            {
                if ( key.startsWith(QStringLiteral("checks/")) )
                {
                    values.insert(key, settings.value(key));
                }
            }
            settings.remove(QStringLiteral("checks"));
            settings.sync();
        }
        ~RestoreSettings()
        {
            QSettings settings = settingsStore();
            settings.remove(QStringLiteral("checks"));
            for ( auto it = values.cbegin(); it != values.cend(); ++it )
            {
                settings.setValue(it.key(), it.value());
            }
            settings.sync();
        }
    } restoreSettings;
    MainWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen);
    auto* consistencyButton =
        window.findChild<QToolButton*>(QStringLiteral("consistencyCheckSettingsButton"));
    auto* resourceButton = window.findChild<QToolButton*>(QStringLiteral("resourceCheckSettingsButton"));
    check(consistencyButton && resourceButton,
          "Precision settings fixture exposes both real checker buttons");
    if ( !consistencyButton || !resourceButton )
    {
        return;
    }
    check(consistencyButton->isEnabled() && resourceButton->isEnabled(),
          "Both checker settings buttons are enabled before modal drivers are scheduled");
    if ( !consistencyButton->isEnabled() || !resourceButton->isEnabled() )
    {
        return;
    }
    for ( const ProjectType project : {ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM} )
    {
        editor::DocumentState state;
        state.level.kind = project;
        window.setDocumentStateForTest(state);
        // Cancel a change, accept Precise, reopen/cancel, restore defaults/accept.
        for ( int path = 0; path < 4; ++path )
        {
            window.rebuildChecksForTest();
            std::vector<QTreeWidget*> checkTrees;
            for ( auto* tree : window.findChildren<QTreeWidget*>() )
            {
                if ( tree->columnCount() == 2 && tree->headerItem()->text(1) == QStringLiteral("Issue") )
                {
                    checkTrees.push_back(tree);
                    auto* marker = new QTreeWidgetItem(tree);
                    marker->setText(1, QStringLiteral("precision refresh sentinel"));
                }
            }
            check(checkTrees.size() == 2, "Precision settings fixture identifies both checker output trees");
            bool inspected = false;
            // Disconnect the driver before any captured loop locals disappear,
            // even when a disabled or broken button fails to open its dialog.
            QObject responseContext;
            QTimer::singleShot(
                0, &responseContext,
                [&]
                {
                    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    check(dialog != nullptr, "Precision settings opens the actual modal dialog");
                    if ( !dialog )
                    {
                        return;
                    }
                    auto* combo = dialog->findChild<QComboBox*>(QStringLiteral("checkerGeometryPrecision"));
                    auto* label = dialog->findChild<QLabel*>(QStringLiteral("checkerGeometryPrecisionLabel"));
                    auto* warning =
                        dialog->findChild<QLabel*>(QStringLiteral("checkerGeometryPrecisionWarning"));
                    auto* table = dialog->findChild<QTableWidget*>(QStringLiteral("checkerRuleTable"));
                    auto* buttons = dialog->findChild<QDialogButtonBox*>();
                    inspected = combo && label && warning && table && buttons;
                    check(
                        inspected,
                        "Consistency settings exposes the precision selector label warning and real buttons");
                    if ( !inspected )
                    {
                        dialog->reject();
                        return;
                    }
                    check(
                        label->text() == QStringLiteral("Geometry precision") && label->buddy() == combo &&
                            combo->count() == 2 && combo->itemText(0) == QStringLiteral("Game-compatible") &&
                            combo->itemText(1) == QStringLiteral("Precise") &&
                            combo->itemData(0).toInt() ==
                                static_cast<int>(GeometryCheckPrecision::GameCompatible) &&
                            combo->itemData(1).toInt() == static_cast<int>(GeometryCheckPrecision::Precise) &&
                            warning->text() == QStringLiteral("Precise checks can report coordinate-rounding "
                                                              "differences in original game levels."),
                        "Precision selector exposes complete fixed labels enum mapping and original-level "
                        "explanation");
                    check(combo->currentIndex() == (path < 2 ? 0 : 1) && warning->isHidden() == (path < 2),
                          "Precision selector loads the per-project default or saved value and contextual "
                          "warning");
                    const auto normalDescription = [&]() -> QString
                    {
                        for ( int row = 0; row < table->rowCount(); ++row )
                        {
                            if ( table->item(row, 0)->data(Qt::UserRole).toInt() ==
                                 static_cast<int>(CheckRule::surfaceNormal) )
                            {
                                return table->item(row, 2)->text();
                            }
                        }
                        return {};
                    };
                    combo->setCurrentIndex(1);
                    check(!warning->isHidden() &&
                              normalDescription() == QStringLiteral("Report stored normals whose length "
                                                                    "differs from one by at least 0.0001."),
                          "Precise mode immediately explains original-level warnings without changing the "
                          "unit-normal rule");
                    if ( path == 3 )
                    {
                        auto* restore =
                            dialog->findChild<QPushButton*>(QStringLiteral("checkerRestoreDefaultsButton"));
                        check(restore != nullptr, "Precision settings exposes Restore Defaults");
                        if ( restore )
                        {
                            restore->click();
                        }
                        check(combo->currentIndex() == 0 && warning->isHidden() &&
                                  normalDescription() ==
                                      QStringLiteral("Report stored normals whose length differs from one by "
                                                     "at least 0.0001."),
                              "Restore Defaults resets geometry precision and retains the unchanged "
                              "unit-normal rule");
                    }
                    buttons->button(path == 1 || path == 3 ? QDialogButtonBox::Ok : QDialogButtonBox::Cancel)
                        ->click();
                });
            consistencyButton->click();
            check(inspected, "Consistency precision modal response was exercised");
            if ( path == 1 || path == 3 )
            {
                for ( auto* tree : checkTrees )
                {
                    bool retainedMarker = false;
                    for ( int row = 0; row < tree->topLevelItemCount(); ++row )
                    {
                        retainedMarker |=
                            tree->topLevelItem(row)->text(1) == QStringLiteral("precision refresh sentinel");
                    }
                    check(!retainedMarker,
                          "Accepting precision settings refreshes the actual checker output immediately");
                }
            }
            QSettings settings = settingsStore();
            const bool precise = path == 1 || path == 2;
            check(loadCheckRuleSelection(settings, project).geometryPrecision ==
                      (precise ? GeometryCheckPrecision::Precise : GeometryCheckPrecision::GameCompatible),
                  "Actual dialog Accept persists precision while Cancel preserves the previous mode");
            if ( path == 0 )
            {
                check(!settings.contains(geometryCheckPrecisionSettingsKey(project)),
                      "Canceled initial precision edit writes no preference key");
            }
            else
            {
                check(settings.value(geometryCheckPrecisionSettingsKey(project)).toInt() == (precise ? 1 : 0),
                      "Actual consistency dialog stores the fixed precision value for this project");
            }
        }
        QSettings settings = settingsStore();
        auto selected = loadCheckRuleSelection(settings, project);
        selected.geometryPrecision = GeometryCheckPrecision::Precise;
        saveCheckRuleSelection(settings, project, CheckRuleGroup::consistency, selected);
        settings.sync();
        bool inspectedResources = false;
        QObject resourceResponseContext;
        QTimer::singleShot(
            0, &resourceResponseContext,
            [&]
            {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if ( !dialog )
                {
                    return;
                }
                auto* buttons = dialog->findChild<QDialogButtonBox*>();
                inspectedResources =
                    buttons && !dialog->findChild<QComboBox*>(QStringLiteral("checkerGeometryPrecision")) &&
                    !dialog->findChild<QLabel*>(QStringLiteral("checkerGeometryPrecisionWarning"));
                check(inspectedResources, "Resource settings omits consistency-only precision controls");
                if ( buttons )
                {
                    buttons->button(QDialogButtonBox::Ok)->click();
                }
                else
                {
                    dialog->reject();
                }
            });
        resourceButton->click();
        check(inspectedResources && loadCheckRuleSelection(settings, project).geometryPrecision ==
                                        GeometryCheckPrecision::Precise,
              "Resource settings acceptance preserves the saved consistency precision");
    }
}
} // namespace sed::app_tests
