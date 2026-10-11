#include "test_common.hpp"

#include "main_window/main_window.hpp"
#include "sed/level.hpp"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWidget>

#include <cstdint>
#include <memory>
#include <utility>

namespace sed::app_tests
{
namespace
{

QToolButton* bakedLightFlagPicker(MainWindow& window)
{
    QTreeWidget* const properties = window.findChild<QTreeWidget*>(
        QStringLiteral("propertiesTree"));
    if (!properties)
    {
        return nullptr;
    }

    for (QTreeWidgetItemIterator it(properties); *it; ++it)
    {
        QTreeWidgetItem* const item = *it;
        if (item->text(0).trimmed() != QStringLiteral("Flags"))
        {
            continue;
        }
        QWidget* const row = properties->itemWidget(item, 1);
        if (!row)
        {
            return nullptr;
        }
        for (QToolButton* const button : row->findChildren<QToolButton*>())
        {
            if (button->text() == QStringLiteral("..."))
            {
                return button;
            }
        }
    }
    return nullptr;
}

bool bakedLightArchFlagPickerMatchesProject(
    const ProjectType project,
    const bool expectArchFlag)
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState state{};
    state.level.kind = project;
    state.level.layers.emplace_back("Layer0");
    SedLight& light = state.level.lights.emplace_back();
    light.flags = static_cast<std::int32_t>(0x40000001u);
    state.selection.lights.insert(0);
    window.setDocumentStateForTest(std::move(state));

    QToolButton* const picker = bakedLightFlagPicker(window);
    if (!picker)
    {
        return false;
    }

    bool foundArchFlag = false;
    bool clickedArchFlag = false;
    QTimer::singleShot(0, &window, [&]()
        {
            auto* const dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget());
            QTreeWidget* const bits = dialog
                ? dialog->findChild<QTreeWidget*>(QStringLiteral("flag_bits_list"))
                : nullptr;
            if (bits)
            {
                for (int row = 0; row < bits->topLevelItemCount(); ++row)
                {
                    QTreeWidgetItem* const item = bits->topLevelItem(row);
                    if (item
                        && item->data(0, Qt::UserRole).toULongLong() == 0x20u)
                    {
                        foundArchFlag = true;
                        if (expectArchFlag)
                        {
                            item->setCheckState(0, Qt::Checked);
                            clickedArchFlag = true;
                        }
                        break;
                    }
                }
            }
            if (dialog)
            {
                QDialogButtonBox* const buttons =
                    dialog->findChild<QDialogButtonBox*>();
                QPushButton* const actionButton = buttons
                    ? buttons->button(expectArchFlag
                        ? QDialogButtonBox::Ok
                        : QDialogButtonBox::Cancel)
                    : nullptr;
                if (actionButton)
                {
                    actionButton->click();
                }
                else
                {
                    dialog->reject();
                }
            }
        });
    picker->click();

    const std::int32_t flagsAfter = window.documentStateForTest().level.lights.front().flags;
    const std::uint32_t expectedFlags = expectArchFlag
        ? 0x40000021u
        : 0x40000001u;
    return foundArchFlag == expectArchFlag
        && clickedArchFlag == expectArchFlag
        && static_cast<std::uint32_t>(flagsAfter) == expectedFlags
        && window.undoDepthForTest() == (expectArchFlag ? 1u : 0u);
}

} // namespace

void runMotsArchBakeFlagTests()
{
    check(bakedLightArchFlagPickerMatchesProject(ProjectType::MOTS, true),
        "baked_light_flags exposes MotS Affect Arch Things and preserves unrelated bits on commit");
    check(bakedLightArchFlagPickerMatchesProject(ProjectType::JKDF2, false)
            && bakedLightArchFlagPickerMatchesProject(ProjectType::IJIM, false),
        "baked_light_flags hides the MotS-only Affect Arch Things flag in JKDF2 and IJIM");
}

} // namespace sed::app_tests
