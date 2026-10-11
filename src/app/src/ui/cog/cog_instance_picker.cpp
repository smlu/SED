#include "ui/cog/cog_instance_picker.hpp"

#include "sed/level.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace sed::ui
{
namespace
{

struct PlacedCogPickerEntry
{
    std::int32_t index{-1};
    std::int32_t id{-1};
    std::string scriptName{};
    QString label{};
};

} // namespace

std::optional<std::int32_t> pickPlacedCogInstance(
    QWidget* parent,
    const Level& level,
    const std::int32_t currentIndex)
{
    std::vector<PlacedCogPickerEntry> entries{};
    entries.reserve(level.cogs.size() + 1);
    entries.push_back(PlacedCogPickerEntry{
        -1,
        -1,
        {},
        QStringLiteral("-1: <unset>"),
    });

    const std::size_t maximumIndex = static_cast<std::size_t>(
        std::numeric_limits<std::int32_t>::max());
    for (std::size_t index = 0; index < level.cogs.size(); ++index)
    {
        if (index > maximumIndex)
        {
            break;
        }

        const Cog& cog = level.cogs[index];
        QString scriptName = QString::fromStdString(cog.name).trimmed();
        if (scriptName.isEmpty())
        {
            scriptName = QStringLiteral("<unnamed script>");
        }
        entries.push_back(PlacedCogPickerEntry{
            static_cast<std::int32_t>(index),
            cog.id,
            cog.name,
            QStringLiteral("%1: %2 (id %3)")
                .arg(static_cast<qulonglong>(index))
                .arg(scriptName)
                .arg(cog.id),
        });
    }

    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("cogInstancePickerDialog"));
    dialog.setWindowTitle(QStringLiteral("Pick Placed COG"));
    dialog.setWindowFlags(
        Qt::Dialog
        | Qt::CustomizeWindowHint
        | Qt::WindowTitleHint
        | Qt::WindowCloseButtonHint);
    dialog.setModal(true);
    dialog.resize(480, 420);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* searchLabel = new QLabel(
        QStringLiteral("Search by COG number or script name"), &dialog);
    layout->addWidget(searchLabel);

    auto* searchEdit = new QLineEdit(&dialog);
    searchEdit->setObjectName(QStringLiteral("cogInstancePickerSearch"));
    searchEdit->setPlaceholderText(QStringLiteral("COG number or script name"));
    searchEdit->setClearButtonEnabled(true);
    layout->addWidget(searchEdit);

    auto* list = new QListWidget(&dialog);
    list->setObjectName(QStringLiteral("cogInstancePickerList"));
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setSelectionBehavior(QAbstractItemView::SelectRows);
    list->setUniformItemSizes(true);
    layout->addWidget(list, 1);

    QListWidgetItem* initialItem = nullptr;
    for (const PlacedCogPickerEntry& entry : entries)
    {
        auto* item = new QListWidgetItem(entry.label, list);
        item->setData(Qt::UserRole, entry.index);
        if (entry.index == currentIndex)
        {
            initialItem = item;
        }
    }
    if (!initialItem)
    {
        initialItem = list->item(0);
    }
    if (initialItem)
    {
        list->setCurrentItem(initialItem);
    }

    QObject::connect(
        searchEdit,
        &QLineEdit::textChanged,
        list,
        [list](const QString& rawFilter)
        {
            const QString filter = rawFilter.trimmed();
            for (int row = 0; row < list->count(); ++row)
            {
                QListWidgetItem* item = list->item(row);
                item->setHidden(
                    !filter.isEmpty()
                    && !item->text().contains(filter, Qt::CaseInsensitive));
            }

            if (list->currentItem() && list->currentItem()->isHidden())
            {
                list->setCurrentItem(nullptr);
            }
            if (!list->currentItem())
            {
                for (int row = 0; row < list->count(); ++row)
                {
                    QListWidgetItem* item = list->item(row);
                    if (!item->isHidden())
                    {
                        list->setCurrentItem(item);
                        break;
                    }
                }
            }
        });

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->setObjectName(QStringLiteral("cogInstancePickerButtons"));
    layout->addWidget(buttons);

    QObject::connect(
        buttons,
        &QDialogButtonBox::accepted,
        &dialog,
        &QDialog::accept);
    QObject::connect(
        buttons,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject);
    QObject::connect(
        list,
        &QListWidget::itemDoubleClicked,
        &dialog,
        [&dialog](QListWidgetItem*)
        {
            dialog.accept();
        });

    searchEdit->setFocus(Qt::OtherFocusReason);
    if (dialog.exec() != QDialog::Accepted || !list->currentItem())
    {
        return std::nullopt;
    }

    bool ok = false;
    const std::int32_t selectedIndex = list->currentItem()
        ->data(Qt::UserRole).toInt(&ok);
    if (!ok || selectedIndex < -1)
    {
        return std::nullopt;
    }
    if (selectedIndex < 0)
    {
        return -1;
    }

    const auto selectedEntry = std::find_if(
        entries.begin(),
        entries.end(),
        [selectedIndex](const PlacedCogPickerEntry& entry)
        {
            return entry.index == selectedIndex;
        });
    if (selectedEntry == entries.end())
    {
        return std::nullopt;
    }

    std::optional<std::int32_t> currentIndexAfterModal{};
    for (std::size_t index = 0; index < level.cogs.size(); ++index)
    {
        const Cog& cog = level.cogs[index];
        if (cog.id != selectedEntry->id
            || cog.name != selectedEntry->scriptName)
        {
            continue;
        }
        if (currentIndexAfterModal)
        {
            // COG identity must remain unambiguous while a modal picker is
            // open. A collision could otherwise bind the selection to a
            // different instance after reordering.
            return std::nullopt;
        }
        if (index > maximumIndex)
        {
            return std::nullopt;
        }
        currentIndexAfterModal = static_cast<std::int32_t>(index);
    }
    if (!currentIndexAfterModal)
    {
        return std::nullopt;
    }
    return *currentIndexAfterModal;
}

} // namespace sed::ui
