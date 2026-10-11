#include "ui/episode/episode_editor.hpp"

#include "sed/text.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <QAbstractItemView>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QVariant>

namespace sed::ui::episode
{
namespace
{

constexpr int kSequenceColumnCount = 9;
constexpr int kStartupGoalCount = 50;
constexpr int kSequenceTargetIdentityRole = Qt::UserRole + 1;
constexpr int kCogStringUnusedNumberRole = Qt::UserRole + 2;
constexpr int kSequenceRowIdentityRole = Qt::UserRole + 3;

QString fromUtf8(const std::string_view value)
{
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

std::string toUtf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::string gameBytesUnchecked(const QString& value)
{
    const QByteArray bytes = value.toLatin1();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString goalKey(const int number)
{
    return QStringLiteral("GOAL_%1").arg(number, 5, 10, QLatin1Char('0'));
}

bool parseGoalNumber(const std::string_view key, int& number)
{
    if (key.size() <= 5 || !sed::text::istartsWithAscii(key, "GOAL_"))
    {
        return false;
    }
    const char* first = key.data() + 5;
    const char* last = key.data() + key.size();
    const auto result = std::from_chars(first, last, number);
    return result.ec == std::errc{} && result.ptr == last;
}

std::optional<int> goalNumberFromDisplayKey(const QString& key)
{
    const std::optional<std::string> bytes = gameBytesForDisplayText(key.trimmed());
    int number = 0;
    if (!bytes || !parseGoalNumber(*bytes, number))
    {
        return std::nullopt;
    }
    return number;
}

class EpisodeEditorDialog final : public QDialog
{
public:
    EpisodeEditorDialog(
        assets::EpisodeFile episode,
        assets::CogStringsFile strings,
        std::string currentLevelFileName,
        const ProjectType projectType,
        QWidget* parent)
        : QDialog(parent)
        , initialStrings_(std::move(strings))
        , currentLevelFileName_(std::move(currentLevelFileName))
        , projectType_(projectType)
    {
        setObjectName(QStringLiteral("episodeCampaignEditorDialog"));
        setWindowTitle(QStringLiteral("Episode / Campaign Editor"));
        setModal(true);
        resize(1080, 690);
        setMinimumSize(860, 560);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(12, 12, 12, 12);
        root->setSpacing(9);

        auto* heading = new QLabel(
            QStringLiteral("Edit the JK/MotS episode sequence, localized COG strings, goals, and optional startup COG."),
            this);
        heading->setWordWrap(true);
        root->addWidget(heading);

        tabs_ = new QTabWidget(this);
        tabs_->setObjectName(QStringLiteral("episodeCampaignTabs"));
        root->addWidget(tabs_, 1);
        buildSequenceTab(episode);
        buildStringsTab(initialStrings_);
        buildStartupTab();

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Save | QDialogButtonBox::Cancel,
            this);
        buttons->setObjectName(QStringLiteral("episodeCampaignButtons"));
        if (QPushButton* save = buttons->button(QDialogButtonBox::Save))
        {
            save->setText(QStringLiteral("Save Campaign"));
        }
        connect(buttons, &QDialogButtonBox::accepted, this, [this]()
            {
                collectAndAccept();
            });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        root->addWidget(buttons);

        rebuildStartupTargets();
    }

    const std::optional<EpisodeEditorResult>& result() const noexcept
    {
        return result_;
    }

private:
    void buildSequenceTab(const assets::EpisodeFile& episode)
    {
        auto* page = new QWidget(tabs_);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(8, 8, 8, 8);
        pageLayout->setSpacing(8);

        auto* headerForm = new QFormLayout();
        episodeName_ = new QLineEdit(displayTextForGameBytes(episode.name), page);
        episodeName_->setObjectName(QStringLiteral("episodeNameEdit"));
        headerForm->addRow(QStringLiteral("Episode name"), episodeName_);

        episodeType_ = new QComboBox(page);
        episodeType_->setObjectName(QStringLiteral("episodeTypeCombo"));
        episodeType_->addItem(QStringLiteral("1 — Single Player"), 1);
        episodeType_->addItem(QStringLiteral("2 — Deathmatch"), 2);
        episodeType_->addItem(QStringLiteral("8 — Special / CTF"), 8);
        episodeType_->addItem(QStringLiteral("16 — Saber Deathmatch"), 16);
        int typeIndex = episodeType_->findData(episode.type);
        if (typeIndex < 0)
        {
            episodeType_->addItem(QString::number(episode.type), episode.type);
            typeIndex = episodeType_->count() - 1;
        }
        episodeType_->setCurrentIndex(typeIndex);
        headerForm->addRow(QStringLiteral("Episode type"), episodeType_);
        pageLayout->addLayout(headerForm);

        sequenceTable_ = new QTableWidget(0, kSequenceColumnCount, page);
        sequenceTable_->setObjectName(QStringLiteral("episodeSequenceTable"));
        sequenceTable_->setHorizontalHeaderLabels({
            QStringLiteral("Line"),
            QStringLiteral("CD"),
            QStringLiteral("Level"),
            QStringLiteral("Type"),
            QStringLiteral("File"),
            QStringLiteral("Light power"),
            QStringLiteral("Dark power"),
            QStringLiteral("Goto A"),
            QStringLiteral("Goto B"),
        });
        sequenceTable_->verticalHeader()->setVisible(false);
        sequenceTable_->setAlternatingRowColors(true);
        sequenceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        sequenceTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        sequenceTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        sequenceTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
        for (const assets::EpisodeSequence& sequence : episode.sequences)
        {
            appendSequence(sequence);
        }
        if (sequenceTable_->rowCount() == 0)
        {
            assets::EpisodeSequence sequence{};
            sequence.fileName = currentLevelFileName_.empty() ? "level01.jkl" : currentLevelFileName_;
            appendSequence(sequence);
        }
        sequenceTable_->selectRow(0);
        pageLayout->addWidget(sequenceTable_, 1);

        auto* row = new QHBoxLayout();
        auto* add = new QPushButton(QStringLiteral("Add Sequence"), page);
        add->setObjectName(QStringLiteral("episodeAddSequenceButton"));
        connect(add, &QPushButton::clicked, this, [this]()
            {
                assets::EpisodeSequence sequence{};
                const int sourceRow = sequenceTable_->currentRow();
                if (sourceRow >= 0)
                {
                    bool okLine = false;
                    bool okLevel = false;
                    const qlonglong sourceLine = sequenceTable_->item(sourceRow, 0)->text().toLongLong(&okLine);
                    const qlonglong sourceLevel = sequenceTable_->item(sourceRow, 2)->text().toLongLong(&okLevel);
                    if (okLine && sourceLine <= std::numeric_limits<int>::max() - 10
                        && sourceLine >= std::numeric_limits<int>::min())
                    {
                        sequence.lineNumber = static_cast<int>(sourceLine + 10);
                    }
                    else
                    {
                        sequence.lineNumber = (sourceRow + 2) * 10;
                    }
                    if (okLevel && sourceLevel <= std::numeric_limits<int>::max() - 1
                        && sourceLevel >= std::numeric_limits<int>::min())
                    {
                        sequence.levelNumber = static_cast<int>(sourceLevel + 1);
                    }
                    else
                    {
                        sequence.levelNumber = sourceRow + 2;
                    }
                }
                sequence.fileName = currentLevelFileName_.empty() ? "level01.jkl" : currentLevelFileName_;
                appendSequence(sequence);
                sequenceTable_->selectRow(sequenceTable_->rowCount() - 1);
                rebuildStartupTargets();
            });
        row->addWidget(add);

        auto* remove = new QPushButton(QStringLiteral("Remove"), page);
        remove->setObjectName(QStringLiteral("episodeRemoveSequenceButton"));
        connect(remove, &QPushButton::clicked, this, [this]()
            {
                const int selected = sequenceTable_->currentRow();
                if (selected < 0 || sequenceTable_->rowCount() <= 1)
                {
                    return;
                }
                sequenceTable_->removeRow(selected);
                sequenceTable_->selectRow(std::min(selected, sequenceTable_->rowCount() - 1));
                rebuildStartupTargets();
            });
        row->addWidget(remove);

        auto* up = new QPushButton(QStringLiteral("Move Up"), page);
        auto* down = new QPushButton(QStringLiteral("Move Down"), page);
        up->setObjectName(QStringLiteral("episodeMoveSequenceUpButton"));
        down->setObjectName(QStringLiteral("episodeMoveSequenceDownButton"));
        connect(up, &QPushButton::clicked, this, [this]() { moveSequence(-1); });
        connect(down, &QPushButton::clicked, this, [this]() { moveSequence(1); });
        row->addWidget(up);
        row->addWidget(down);

        auto* current = new QPushButton(QStringLiteral("Use Current Level"), page);
        current->setObjectName(QStringLiteral("episodeUseCurrentLevelButton"));
        current->setEnabled(!currentLevelFileName_.empty());
        connect(current, &QPushButton::clicked, this, [this]()
            {
                const int selected = sequenceTable_->currentRow();
                if (selected < 0 || currentLevelFileName_.empty())
                {
                    return;
                }
                sequenceTable_->item(selected, 3)->setText(QStringLiteral("LEVEL"));
                sequenceTable_->item(selected, 4)->setText(displayTextForGameBytes(currentLevelFileName_));
                rebuildStartupTargets();
            });
        row->addWidget(current);
        row->addStretch(1);
        pageLayout->addLayout(row);

        connect(sequenceTable_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item)
            {
                if (item
                    && (item->column() == 0
                        || item->column() == 2
                        || item->column() == 3
                        || item->column() == 4))
                {
                    rebuildStartupTargets();
                }
            });

        tabs_->addTab(page, QStringLiteral("Sequence"));
    }

    void buildStringsTab(const assets::CogStringsFile& strings)
    {
        auto* page = new QWidget(tabs_);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(8, 8, 8, 8);
        pageLayout->setSpacing(8);

        stringsSearch_ = new QLineEdit(page);
        stringsSearch_->setObjectName(QStringLiteral("episodeStringsSearch"));
        stringsSearch_->setPlaceholderText(QStringLiteral("Search keys or text..."));
        stringsSearch_->setClearButtonEnabled(true);
        pageLayout->addWidget(stringsSearch_);

        stringsTable_ = new QTableWidget(0, 2, page);
        stringsTable_->setObjectName(QStringLiteral("episodeStringsTable"));
        stringsTable_->setHorizontalHeaderLabels({ QStringLiteral("Key"), QStringLiteral("Text") });
        stringsTable_->verticalHeader()->setVisible(false);
        stringsTable_->setAlternatingRowColors(true);
        stringsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        stringsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        stringsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        stringsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        for (const assets::CogStringEntry& entry : strings.entries)
        {
            appendString(entry.key, entry.value, entry.unusedNumber);
        }
        pageLayout->addWidget(stringsTable_, 1);

        connect(stringsTable_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item)
            {
                if (!item)
                {
                    return;
                }
                if (item->column() == 0)
                {
                    const QString previousKey = item->data(Qt::UserRole).toString();
                    markDirectGoalStringEdit(previousKey);
                    markDirectGoalStringEdit(item->text());
                    const QSignalBlocker blocker(stringsTable_);
                    item->setData(Qt::UserRole, item->text());
                }
                else if (item->column() == 1)
                {
                    markDirectGoalStringEdit(stringsTable_->item(item->row(), 0)->text());
                }
            });

        connect(stringsSearch_, &QLineEdit::textChanged, this, [this](const QString& text)
            {
                const QString needle = text.trimmed();
                for (int row = 0; row < stringsTable_->rowCount(); ++row)
                {
                    const QString key = stringsTable_->item(row, 0)->text();
                    const QString value = stringsTable_->item(row, 1)->text();
                    stringsTable_->setRowHidden(
                        row,
                        !needle.isEmpty()
                            && !key.contains(needle, Qt::CaseInsensitive)
                            && !value.contains(needle, Qt::CaseInsensitive));
                }
            });

        auto* row = new QHBoxLayout();
        auto* add = new QPushButton(QStringLiteral("Add String"), page);
        add->setObjectName(QStringLiteral("episodeAddStringButton"));
        connect(add, &QPushButton::clicked, this, [this]()
            {
                std::unordered_set<std::string> used{};
                for (int row = 0; row < stringsTable_->rowCount(); ++row)
                {
                    used.insert(sed::text::upperAscii(gameBytesUnchecked(stringsTable_->item(row, 0)->text().trimmed())));
                }
                QString candidate = QStringLiteral("NEW_KEY");
                int suffix = 2;
                while (used.contains(gameBytesUnchecked(candidate)))
                {
                    candidate = QStringLiteral("NEW_KEY_%1").arg(suffix++);
                }
                appendString(gameBytesUnchecked(candidate), {}, 0);
                stringsTable_->selectRow(stringsTable_->rowCount() - 1);
                stringsTable_->editItem(stringsTable_->item(stringsTable_->rowCount() - 1, 0));
            });
        row->addWidget(add);

        auto* remove = new QPushButton(QStringLiteral("Remove"), page);
        remove->setObjectName(QStringLiteral("episodeRemoveStringButton"));
        connect(remove, &QPushButton::clicked, this, [this]()
            {
                const int selected = stringsTable_->currentRow();
                if (selected >= 0)
                {
                    markDirectGoalStringEdit(stringsTable_->item(selected, 0)->text());
                    stringsTable_->removeRow(selected);
                }
            });
        row->addWidget(remove);
        row->addStretch(1);
        pageLayout->addLayout(row);

        tabs_->addTab(page, QStringLiteral("COG Strings"));
    }

    void buildStartupTab()
    {
        auto* page = new QWidget(tabs_);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(8, 8, 8, 8);
        pageLayout->setSpacing(8);

        const bool startupSupported = startupCogGenerationSupported(projectType_);
        if (!startupSupported)
        {
            auto* notice = new QLabel(
                QStringLiteral(
                    "MotS campaign sequences and COG strings are fully editable here. Startup COG generation is "
                    "disabled because MotS inventory/goal bins differ from JKDF2; the legacy generator was not "
                    "MotS-correct."),
                page);
            notice->setObjectName(QStringLiteral("episodeStartupMotsNotice"));
            notice->setWordWrap(true);
            pageLayout->addWidget(notice);
        }

        generateStartup_ = new QCheckBox(
            QStringLiteral("Generate a startup/goal COG when the campaign is saved"),
            page);
        generateStartup_->setObjectName(QStringLiteral("episodeGenerateStartupCogCheck"));
        generateStartup_->setToolTip(
            QStringLiteral("The generated COG is also placed into the open level when the selected sequence is the current JKL."));
        generateStartup_->setEnabled(startupSupported);
        pageLayout->addWidget(generateStartup_);

        auto* form = new QFormLayout();
        startupTarget_ = new QComboBox(page);
        startupTarget_->setObjectName(QStringLiteral("episodeStartupTargetCombo"));
        form->addRow(QStringLiteral("LEVEL sequence"), startupTarget_);

        forceRank_ = new QComboBox(page);
        forceRank_->setObjectName(QStringLiteral("episodeStartupForceRankCombo"));
        for (int rank = 0; rank <= 8; ++rank)
        {
            forceRank_->addItem(QString::number(rank), rank);
        }
        form->addRow(QStringLiteral("Force rank"), forceRank_);
        pageLayout->addLayout(form);

        goalsGroup_ = new QGroupBox(QStringLiteral("Goals"), page);
        auto* goalsLayout = new QVBoxLayout(goalsGroup_);
        goalsTable_ = new QTableWidget(0, 1, goalsGroup_);
        goalsTable_->setObjectName(QStringLiteral("episodeStartupGoalsTable"));
        goalsTable_->setHorizontalHeaderLabels({ QStringLiteral("Goal text") });
        goalsTable_->verticalHeader()->setVisible(true);
        goalsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        goalsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        goalsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        goalsLayout->addWidget(goalsTable_, 1);
        auto* goalsButtons = new QHBoxLayout();
        auto* addGoal = new QPushButton(QStringLiteral("Add Goal"), goalsGroup_);
        auto* removeGoal = new QPushButton(QStringLiteral("Remove"), goalsGroup_);
        addGoal->setObjectName(QStringLiteral("episodeAddGoalButton"));
        removeGoal->setObjectName(QStringLiteral("episodeRemoveGoalButton"));
        connect(addGoal, &QPushButton::clicked, this, [this]()
            {
                for (int row = 0; row < goalsTable_->rowCount(); ++row)
                {
                    QTableWidgetItem* item = goalsTable_->item(row, 0);
                    if (item && item->text().isEmpty())
                    {
                        item->setText(QStringLiteral("New goal"));
                        goalsTable_->selectRow(row);
                        goalsTable_->editItem(item);
                        break;
                    }
                }
            });
        connect(removeGoal, &QPushButton::clicked, this, [this]()
            {
                const int row = goalsTable_->currentRow();
                if (row >= 0)
                {
                    if (QTableWidgetItem* item = goalsTable_->item(row, 0))
                    {
                        item->setText({});
                    }
                }
            });
        goalsButtons->addWidget(addGoal);
        goalsButtons->addWidget(removeGoal);
        goalsButtons->addStretch(1);
        goalsLayout->addLayout(goalsButtons);
        pageLayout->addWidget(goalsGroup_, 1);

        auto* equipment = new QGroupBox(QStringLiteral("Starting equipment"), page);
        auto* equipmentLayout = new QGridLayout(equipment);
        bryar_ = addEquipmentCheck(equipmentLayout, QStringLiteral("Bryar pistol"), 0, 0);
        rifle_ = addEquipmentCheck(equipmentLayout, QStringLiteral("ST rifle"), 0, 1);
        thermal_ = addEquipmentCheck(equipmentLayout, QStringLiteral("Thermal detonators"), 0, 2);
        crossbow_ = addEquipmentCheck(equipmentLayout, QStringLiteral("Crossbow"), 1, 0);
        repeater_ = addEquipmentCheck(equipmentLayout, QStringLiteral("Repeater"), 1, 1);
        railgun_ = addEquipmentCheck(equipmentLayout, QStringLiteral("Rail detonator"), 1, 2);
        sequencer_ = addEquipmentCheck(equipmentLayout, QStringLiteral("Sequencer charges"), 2, 0);
        concussion_ = addEquipmentCheck(equipmentLayout, QStringLiteral("Concussion rifle"), 2, 1);
        lightsaber_ = addEquipmentCheck(equipmentLayout, QStringLiteral("Lightsaber"), 2, 2);
        pageLayout->addWidget(equipment);

        if (!startupSupported)
        {
            startupTarget_->setEnabled(false);
            forceRank_->setEnabled(false);
            goalsGroup_->setEnabled(false);
            equipment->setEnabled(false);
        }

        connect(startupTarget_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](const int)
            {
                loadGoalsForCurrentTarget();
            });
        connect(goalsTable_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item)
            {
                if (loadedGoalBase_ >= 0 && item)
                {
                    dirtyGoalBases_.insert(loadedGoalBase_);
                    goalEditSerials_.insert_or_assign(
                        loadedGoalBase_ + item->row(),
                        ++editSerial_);
                }
            });

        tabs_->addTab(page, QStringLiteral("Startup COG"));
    }

    QCheckBox* addEquipmentCheck(QGridLayout* layout, const QString& label, const int row, const int column)
    {
        auto* check = new QCheckBox(label, layout->parentWidget());
        layout->addWidget(check, row, column);
        return check;
    }

    void appendSequence(const assets::EpisodeSequence& sequence)
    {
        const QSignalBlocker blocker(sequenceTable_);
        const int row = sequenceTable_->rowCount();
        const qulonglong rowIdentity = nextSequenceRowIdentity_++;
        sequenceTable_->insertRow(row);
        const std::array<QString, kSequenceColumnCount> values{
            QString::number(sequence.lineNumber),
            QString::number(sequence.cdNumber),
            QString::number(sequence.levelNumber),
            displayTextForGameBytes(sequence.type),
            displayTextForGameBytes(sequence.fileName),
            QString::number(sequence.lightPower),
            QString::number(sequence.darkPower),
            QString::number(sequence.gotoA),
            QString::number(sequence.gotoB),
        };
        for (int column = 0; column < kSequenceColumnCount; ++column)
        {
            auto* item = new QTableWidgetItem(values[static_cast<std::size_t>(column)]);
            item->setData(kSequenceRowIdentityRole, rowIdentity);
            sequenceTable_->setItem(row, column, item);
        }
    }

    void appendString(
        const std::string_view key,
        const std::string_view value,
        const int unusedNumber)
    {
        const int row = stringsTable_->rowCount();
        stringsTable_->insertRow(row);
        auto* keyItem = new QTableWidgetItem(displayTextForGameBytes(key));
        keyItem->setData(Qt::UserRole, keyItem->text());
        keyItem->setData(kCogStringUnusedNumberRole, unusedNumber);
        stringsTable_->setItem(row, 0, keyItem);
        stringsTable_->setItem(row, 1, new QTableWidgetItem(displayTextForGameBytes(value)));
    }

    void markDirectGoalStringEdit(const QString& key)
    {
        const std::optional<int> number = goalNumberFromDisplayKey(key);
        if (number)
        {
            directGoalStringEditSerials_.insert_or_assign(*number, ++editSerial_);
        }
    }

    void moveSequence(const int direction)
    {
        const int source = sequenceTable_->currentRow();
        const int destination = source + direction;
        if (source < 0 || destination < 0 || destination >= sequenceTable_->rowCount())
        {
            return;
        }

        const QSignalBlocker blocker(sequenceTable_);
        for (int column = 0; column < kSequenceColumnCount; ++column)
        {
            QTableWidgetItem* sourceItem = sequenceTable_->takeItem(source, column);
            QTableWidgetItem* destinationItem = sequenceTable_->takeItem(destination, column);
            sequenceTable_->setItem(source, column, destinationItem);
            sequenceTable_->setItem(destination, column, sourceItem);
        }
        sequenceTable_->selectRow(destination);
        rebuildStartupTargets();
    }

    std::optional<int> targetRow() const
    {
        if (!startupTarget_ || startupTarget_->currentIndex() < 0)
        {
            return std::nullopt;
        }
        bool ok = false;
        const int row = startupTarget_->currentData().toInt(&ok);
        if (!ok || row < 0 || row >= sequenceTable_->rowCount())
        {
            return std::nullopt;
        }
        return row;
    }

    int targetGoalBase() const
    {
        const std::optional<int> row = targetRow();
        if (!row)
        {
            return -1;
        }
        bool ok = false;
        const int level = sequenceTable_->item(*row, 2)->text().toInt(&ok);
        if (!ok)
        {
            return -1;
        }
        return goalBaseForLevelNumber(level).value_or(-1);
    }

    void rebuildStartupTargets()
    {
        if (!startupTarget_ || rebuildingStartupTargets_)
        {
            return;
        }
        storeGoalsForLoadedTarget();
        rebuildingStartupTargets_ = true;
        const int previousRow = targetRow().value_or(-1);
        const QVariant previousRowIdentity = startupTarget_->currentData(
            kSequenceTargetIdentityRole);
        startupTarget_->clear();
        for (int row = 0; row < sequenceTable_->rowCount(); ++row)
        {
            const QString type = sequenceTable_->item(row, 3)->text().trimmed();
            if (type.compare(QStringLiteral("LEVEL"), Qt::CaseInsensitive) != 0)
            {
                continue;
            }
            const QString file = sequenceTable_->item(row, 4)->text().trimmed();
            const QString level = sequenceTable_->item(row, 2)->text().trimmed();
            startupTarget_->addItem(QStringLiteral("%1 — %2").arg(level, file), row);
            startupTarget_->setItemData(
                startupTarget_->count() - 1,
                sequenceTable_->item(row, 0)->data(kSequenceRowIdentityRole),
                kSequenceTargetIdentityRole);
        }
        int restore = -1;
        if (previousRowIdentity.isValid())
        {
            restore = startupTarget_->findData(
                previousRowIdentity,
                kSequenceTargetIdentityRole);
        }
        if (restore < 0)
        {
            restore = startupTarget_->findData(previousRow);
        }
        if (restore < 0 && !currentLevelFileName_.empty())
        {
            for (int index = 0; index < startupTarget_->count(); ++index)
            {
                const int row = startupTarget_->itemData(index).toInt();
                if (sequenceTable_->item(row, 4)->text().compare(
                        displayTextForGameBytes(currentLevelFileName_),
                        Qt::CaseInsensitive) == 0)
                {
                    restore = index;
                    break;
                }
            }
        }
        startupTarget_->setCurrentIndex(restore >= 0 ? restore : (startupTarget_->count() > 0 ? 0 : -1));
        generateStartup_->setEnabled(
            startupCogGenerationSupported(projectType_)
            && startupTarget_->count() > 0);
        if (!generateStartup_->isEnabled())
        {
            generateStartup_->setChecked(false);
        }
        rebuildingStartupTargets_ = false;
        loadGoalsForCurrentTarget();
    }

    assets::CogStringsFile collectStringsTable(bool* ok = nullptr)
    {
        assets::CogStringsFile strings{};
        std::unordered_set<std::string> keys{};
        bool valid = true;
        for (int row = 0; row < stringsTable_->rowCount(); ++row)
        {
            const QString rawKey = stringsTable_->item(row, 0)->text();
            const QString trimmedKey = rawKey.trimmed();
            const std::optional<std::string> encodedKey = gameBytesForDisplayText(trimmedKey);
            const std::optional<std::string> encodedValue = gameBytesForDisplayText(
                stringsTable_->item(row, 1)->text());
            const std::string key = encodedKey
                ? sed::text::upperAscii(*encodedKey)
                : std::string{};
            if (!encodedKey
                || !encodedValue
                || rawKey != trimmedKey
                || key.empty()
                || key.find('"') != std::string::npos
                || key.find('\r') != std::string::npos
                || key.find('\n') != std::string::npos
                || encodedValue->find('"') != std::string::npos
                || encodedValue->find('\r') != std::string::npos
                || encodedValue->find('\n') != std::string::npos
                || !keys.insert(key).second)
            {
                valid = false;
                stringsTable_->selectRow(row);
                break;
            }
            strings.set(
                key,
                *encodedValue,
                stringsTable_->item(row, 0)->data(kCogStringUnusedNumberRole).toInt());
        }
        if (ok)
        {
            *ok = valid;
        }
        return strings;
    }

    void loadGoalsForCurrentTarget()
    {
        if (!goalsTable_ || rebuildingStartupTargets_)
        {
            return;
        }
        storeGoalsForLoadedTarget();
        const int base = targetGoalBase();
        const QSignalBlocker blocker(goalsTable_);
        goalsTable_->setRowCount(0);
        loadedGoalBase_ = base;
        goalsGroup_->setEnabled(
            startupCogGenerationSupported(projectType_)
            && base >= 0);
        if (base < 0)
        {
            return;
        }

        const auto draft = goalDrafts_.find(base);
        if (draft != goalDrafts_.end())
        {
            for (int offset = 0; offset < kStartupGoalCount; ++offset)
            {
                const int row = goalsTable_->rowCount();
                goalsTable_->insertRow(row);
                goalsTable_->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(offset)));
                const QString text = static_cast<std::size_t>(offset) < draft->second.size()
                    ? draft->second[static_cast<std::size_t>(offset)]
                    : QString{};
                goalsTable_->setItem(row, 0, new QTableWidgetItem(text));
            }
            return;
        }

        bool ok = false;
        const assets::CogStringsFile current = collectStringsTable(&ok);
        const assets::CogStringsFile& source = ok ? current : initialStrings_;
        std::vector<QString> goals(static_cast<std::size_t>(kStartupGoalCount));
        for (int offset = 0; offset < kStartupGoalCount; ++offset)
        {
            const std::string key = gameBytesUnchecked(goalKey(base + offset));
            const std::string* value = source.find(key);
            const int row = goalsTable_->rowCount();
            goalsTable_->insertRow(row);
            goalsTable_->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(offset)));
            const QString text = value ? displayTextForGameBytes(*value) : QString{};
            goalsTable_->setItem(row, 0, new QTableWidgetItem(text));
            goals[static_cast<std::size_t>(offset)] = text;
        }
        goalDrafts_.insert_or_assign(base, std::move(goals));
    }

    void storeGoalsForLoadedTarget()
    {
        if (!goalsTable_ || loadedGoalBase_ < 0)
        {
            return;
        }

        std::vector<QString> goals{};
        goals.reserve(static_cast<std::size_t>(goalsTable_->rowCount()));
        for (int row = 0; row < goalsTable_->rowCount(); ++row)
        {
            const QTableWidgetItem* item = goalsTable_->item(row, 0);
            goals.push_back(item ? item->text() : QString{});
        }
        goalDrafts_.insert_or_assign(loadedGoalBase_, std::move(goals));
    }

    std::optional<assets::EpisodeSequence> sequenceAt(const int row, QString* error) const
    {
        assets::EpisodeSequence sequence{};
        const std::array<int*, 7> numericTargets{
            &sequence.lineNumber,
            &sequence.cdNumber,
            &sequence.levelNumber,
            &sequence.lightPower,
            &sequence.darkPower,
            &sequence.gotoA,
            &sequence.gotoB,
        };
        const std::array<int, 7> numericColumns{ 0, 1, 2, 5, 6, 7, 8 };
        for (std::size_t index = 0; index < numericTargets.size(); ++index)
        {
            bool ok = false;
            *numericTargets[index] = sequenceTable_->item(row, numericColumns[index])->text().trimmed().toInt(&ok);
            if (!ok)
            {
                if (error)
                {
                    *error = QStringLiteral("Sequence row %1 contains an invalid integer.").arg(row + 1);
                }
                return std::nullopt;
            }
        }
        const std::optional<std::string> type = gameBytesForDisplayText(
            sequenceTable_->item(row, 3)->text().trimmed());
        const std::optional<std::string> fileName = gameBytesForDisplayText(
            sequenceTable_->item(row, 4)->text().trimmed());
        sequence.type = type.value_or(std::string{});
        sequence.fileName = fileName.value_or(std::string{});
        if (!type || !fileName || sequence.type.empty() || sequence.fileName.empty())
        {
            if (error)
            {
                *error = QStringLiteral("Sequence row %1 requires both Type and File.").arg(row + 1);
            }
            return std::nullopt;
        }
        if (sequence.fileName.size() > 31)
        {
            if (error)
            {
                *error = QStringLiteral("Sequence row %1 File exceeds the JK/MotS 31-byte limit.").arg(row + 1);
            }
            return std::nullopt;
        }
        return sequence;
    }

    void collectAndAccept()
    {
        storeGoalsForLoadedTarget();
        EpisodeEditorResult value{};
        const QString episodeName = episodeName_->text();
        const std::optional<std::string> encodedEpisodeName = gameBytesForDisplayText(episodeName);
        value.episode.name = encodedEpisodeName.value_or(std::string{});
        value.episode.type = episodeType_->currentData().toInt();
        if (!encodedEpisodeName || value.episode.name.empty())
        {
            QMessageBox::warning(
                this,
                windowTitle(),
                QStringLiteral("Episode name cannot be empty and must use JK/MotS one-byte characters."));
            tabs_->setCurrentIndex(0);
            episodeName_->setFocus();
            return;
        }

        for (int row = 0; row < sequenceTable_->rowCount(); ++row)
        {
            QString error{};
            const std::optional<assets::EpisodeSequence> sequence = sequenceAt(row, &error);
            if (!sequence)
            {
                QMessageBox::warning(this, windowTitle(), error);
                tabs_->setCurrentIndex(0);
                sequenceTable_->selectRow(row);
                return;
            }
            value.episode.sequences.push_back(*sequence);
        }

        bool stringsOk = false;
        value.strings = collectStringsTable(&stringsOk);
        if (!stringsOk)
        {
            QMessageBox::warning(
                this,
                windowTitle(),
                QStringLiteral(
                    "COG string keys must be non-empty and unique. Keys/text must fit the JK/MotS one-byte encoding, "
                    "stay on one line, and cannot contain double quotes."));
            tabs_->setCurrentIndex(1);
            return;
        }

        // COG Strings and Goals are two views of the same GOAL_n values. Merge
        // each slot by its actual last edit so neither tab silently overwrites
        // a later edit made in the other tab.
        for (const int goalBase : dirtyGoalBases_)
        {
            const auto draft = goalDrafts_.find(goalBase);
            if (draft == goalDrafts_.end())
            {
                continue;
            }
            for (int offset = 0; offset < kStartupGoalCount; ++offset)
            {
                const int goalNumber = goalBase + offset;
                const auto goalEdit = goalEditSerials_.find(goalNumber);
                if (goalEdit == goalEditSerials_.end())
                {
                    continue;
                }
                const auto directEdit = directGoalStringEditSerials_.find(goalNumber);
                if (directEdit != directGoalStringEditSerials_.end()
                    && directEdit->second > goalEdit->second)
                {
                    continue;
                }

                const QString text = static_cast<std::size_t>(offset) < draft->second.size()
                    ? draft->second[static_cast<std::size_t>(offset)]
                    : QString{};
                const std::optional<std::string> encoded = gameBytesForDisplayText(text);
                if (!encoded
                    || encoded->find('"') != std::string::npos
                    || encoded->find('\r') != std::string::npos
                    || encoded->find('\n') != std::string::npos)
                {
                    QMessageBox::warning(
                        this,
                        windowTitle(),
                        QStringLiteral(
                            "Goal text must fit the JK/MotS one-byte encoding, stay on one line, and cannot contain double quotes."));
                    tabs_->setCurrentIndex(2);
                    return;
                }
                const std::string key = gameBytesUnchecked(goalKey(goalNumber));
                if (encoded->empty())
                {
                    value.strings.erase(key);
                }
                else
                {
                    value.strings.set(key, *encoded);
                }
            }
        }

        if (generateStartup_->isEnabled() && generateStartup_->isChecked())
        {
            const std::optional<int> row = targetRow();
            if (!row || static_cast<std::size_t>(*row) >= value.episode.sequences.size())
            {
                QMessageBox::warning(this, windowTitle(), QStringLiteral("Choose a LEVEL sequence for the startup COG."));
                tabs_->setCurrentIndex(2);
                return;
            }

            StartupCogOptions startup{};
            startup.sequenceIndex = static_cast<std::size_t>(*row);
            startup.forceRank = forceRank_->currentData().toInt();
            const std::optional<int> goalBase = goalBaseForLevelNumber(
                value.episode.sequences[startup.sequenceIndex].levelNumber);
            if (!goalBase)
            {
                QMessageBox::warning(
                    this,
                    windowTitle(),
                    QStringLiteral("The selected LEVEL number cannot be represented as a GOAL_n inventory base."));
                tabs_->setCurrentIndex(2);
                return;
            }
            startup.goals.resize(static_cast<std::size_t>(kStartupGoalCount));
            for (int offset = 0; offset < kStartupGoalCount; ++offset)
            {
                const std::string key = gameBytesUnchecked(goalKey(*goalBase + offset));
                if (const std::string* goal = value.strings.find(key))
                {
                    startup.goals[static_cast<std::size_t>(offset)] = *goal;
                }
            }
            startup.bryar = bryar_->isChecked();
            startup.rifle = rifle_->isChecked();
            startup.thermalDetonators = thermal_->isChecked();
            startup.crossbow = crossbow_->isChecked();
            startup.repeater = repeater_->isChecked();
            startup.railgun = railgun_->isChecked();
            startup.sequencer = sequencer_->isChecked();
            startup.concussion = concussion_->isChecked();
            startup.lightsaber = lightsaber_->isChecked();
            value.startupCog = std::move(startup);
        }

        try
        {
            (void)assets::saveToEpisodeJkText(value.episode);
            (void)assets::saveToCogStringsUniText(value.strings);
        }
        catch (const std::exception& error)
        {
            QMessageBox::warning(this, windowTitle(), fromUtf8(error.what()));
            return;
        }

        result_ = std::move(value);
        accept();
    }

    QTabWidget* tabs_{};
    QLineEdit* episodeName_{};
    QComboBox* episodeType_{};
    QTableWidget* sequenceTable_{};
    QLineEdit* stringsSearch_{};
    QTableWidget* stringsTable_{};
    QCheckBox* generateStartup_{};
    QComboBox* startupTarget_{};
    QComboBox* forceRank_{};
    QGroupBox* goalsGroup_{};
    QTableWidget* goalsTable_{};
    QCheckBox* bryar_{};
    QCheckBox* rifle_{};
    QCheckBox* thermal_{};
    QCheckBox* crossbow_{};
    QCheckBox* repeater_{};
    QCheckBox* railgun_{};
    QCheckBox* sequencer_{};
    QCheckBox* concussion_{};
    QCheckBox* lightsaber_{};
    assets::CogStringsFile initialStrings_{};
    std::string currentLevelFileName_{};
    ProjectType projectType_{ProjectType::JKDF2};
    bool rebuildingStartupTargets_{};
    qulonglong nextSequenceRowIdentity_{1};
    int loadedGoalBase_{-1};
    std::uint64_t editSerial_{};
    std::unordered_map<int, std::vector<QString>> goalDrafts_{};
    std::unordered_set<int> dirtyGoalBases_{};
    std::unordered_map<int, std::uint64_t> goalEditSerials_{};
    std::unordered_map<int, std::uint64_t> directGoalStringEditSerials_{};
    std::optional<EpisodeEditorResult> result_{};
};

} // namespace

QString displayTextForGameBytes(const std::string_view value)
{
    return QString::fromLatin1(value.data(), static_cast<int>(value.size()));
}

std::optional<std::string> gameBytesForDisplayText(const QString& value)
{
    const QByteArray bytes = value.toLatin1();
    if (QString::fromLatin1(bytes) != value)
    {
        return std::nullopt;
    }
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::optional<EpisodeEditorResult> editEpisodeCampaign(
    QWidget* parent,
    assets::EpisodeFile episode,
    assets::CogStringsFile strings,
    std::string currentLevelFileName,
    const ProjectType projectType)
{
    EpisodeEditorDialog dialog(
        std::move(episode),
        std::move(strings),
        std::move(currentLevelFileName),
        projectType,
        parent);
    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }
    return dialog.result();
}

bool startupCogGenerationSupported(const ProjectType projectType) noexcept
{
    return projectType == ProjectType::JKDF2;
}

std::optional<int> goalBaseForLevelNumber(const int levelNumber) noexcept
{
    constexpr int kGoalRangeSize = 1000;
    if (levelNumber <= 0
        || levelNumber > (std::numeric_limits<int>::max() - (kGoalRangeSize - 1)) / kGoalRangeSize)
    {
        return std::nullopt;
    }
    return levelNumber * kGoalRangeSize;
}

std::filesystem::path projectDirectoryForLevel(const std::filesystem::path& levelPath)
{
    std::filesystem::path directory = levelPath;
    std::error_code ec{};
    if (!std::filesystem::is_directory(directory, ec))
    {
        directory = levelPath.parent_path();
    }
    if (directory.empty())
    {
        return directory;
    }

    for (std::filesystem::path candidate = directory;
         !candidate.empty();
         candidate = candidate.parent_path())
    {
        ec.clear();
        if (std::filesystem::exists(candidate / "episode.jk", ec)
            || std::filesystem::exists(candidate / "misc" / "cogstrings.uni", ec)
            || std::filesystem::exists(candidate / "cogstrings.uni", ec)
            || std::filesystem::exists(candidate / "ui" / "cogstrings.uni", ec))
        {
            return candidate;
        }
        if (candidate == candidate.parent_path())
        {
            break;
        }
    }

    if (sed::text::iequalsAscii(directory.filename().string(), "jkl"))
    {
        return directory.parent_path();
    }
    return directory;
}

std::filesystem::path existingCogStringsPath(const std::filesystem::path& projectDirectory)
{
    std::error_code ec{};
    const std::filesystem::path root = projectDirectory / "cogstrings.uni";
    if (std::filesystem::exists(root, ec))
    {
        return root;
    }
    const std::filesystem::path misc = canonicalCogStringsPath(projectDirectory);
    ec.clear();
    if (std::filesystem::exists(misc, ec))
    {
        return misc;
    }
    const std::filesystem::path ui = projectDirectory / "ui" / "cogstrings.uni";
    ec.clear();
    if (std::filesystem::exists(ui, ec))
    {
        return ui;
    }
    return misc;
}

std::filesystem::path canonicalCogStringsPath(const std::filesystem::path& projectDirectory)
{
    return projectDirectory / "misc" / "cogstrings.uni";
}

std::string startupCogFileName(const assets::EpisodeSequence& sequence)
{
    std::filesystem::path path(sequence.fileName);
    const std::string stem = path.stem().string().empty() ? path.filename().string() : path.stem().string();
    return stem + "_start.cog";
}

void replaceGoalStrings(
    assets::CogStringsFile& strings,
    const int goalBase,
    const std::vector<std::string>& goals)
{
    if (goalBase < 0
        || goalBase > std::numeric_limits<int>::max() - (kStartupGoalCount - 1)
        || goals.size() > static_cast<std::size_t>(kStartupGoalCount))
    {
        throw std::invalid_argument("Invalid GOAL_n range");
    }
    strings.entries.erase(
        std::remove_if(strings.entries.begin(), strings.entries.end(), [goalBase](const assets::CogStringEntry& entry)
            {
                int number = 0;
                const std::int64_t rangeStart = goalBase;
                if (!parseGoalNumber(entry.key, number))
                {
                    return false;
                }
                const std::int64_t keyNumber = number;
                return keyNumber >= rangeStart
                    && keyNumber < rangeStart + kStartupGoalCount;
            }),
        strings.entries.end());

    for (std::size_t index = 0; index < goals.size(); ++index)
    {
        if (!goals[index].empty())
        {
            strings.set(
                gameBytesUnchecked(goalKey(goalBase + static_cast<int>(index))),
                goals[index]);
        }
    }
}

std::string generateStartupCogText(
    const assets::EpisodeSequence& sequence,
    const StartupCogOptions& options)
{
    const std::optional<int> checkedGoalBase = goalBaseForLevelNumber(sequence.levelNumber);
    if (!checkedGoalBase)
    {
        throw std::out_of_range("LEVEL number cannot be represented as a GOAL_n inventory base");
    }
    const int goalBase = *checkedGoalBase;
    if (options.goals.size() > static_cast<std::size_t>(kStartupGoalCount))
    {
        throw std::invalid_argument("JKDF2 supports at most 50 startup goals");
    }
    std::ostringstream out{};
    out << "# Level master COG\n";
    out << "# Generated by SED C++\n\n";
    out << "symbols\n";
    out << "message startup\n";
    out << "message timer\n";
    out << "int player local\n";
    out << "end\n\n";
    out << "code\n\n";
    out << "startup:\n";
    out << "  SetMasterCOG(GetSelfCOG());\n";
    out << "  player = GetLocalPlayerThing();\n";
    out << "  SetInv(player, 99, " << goalBase << ");\n";
    for (std::size_t index = 0; index < options.goals.size(); ++index)
    {
        if (!options.goals[index].empty())
        {
            out << "  SetGoalFlags(player, " << index << ", 1);\n";
        }
    }
    out << "  SetInv(player, 1, 1); // fists\n";
    if (options.bryar) out << "  SetInv(player, 2, 1); // Bryar pistol\n";
    if (options.rifle) out << "  SetInv(player, 3, 1); // ST rifle\n";
    if (options.thermalDetonators) out << "  SetInv(player, 4, 1); // thermal detonators\n";
    if (options.crossbow) out << "  SetInv(player, 5, 1); // crossbow\n";
    if (options.repeater) out << "  SetInv(player, 6, 1); // repeater\n";
    if (options.railgun) out << "  SetInv(player, 7, 1); // rail detonator\n";
    if (options.sequencer) out << "  SetInv(player, 8, 10); // sequencer charges\n";
    if (options.concussion) out << "  SetInv(player, 9, 1); // concussion rifle\n";
    if (options.lightsaber) out << "  SetInv(player, 10, 1); // lightsaber\n";
    if (options.bryar || options.rifle) out << "  SetInv(player, 11, 100); // energy\n";
    if (options.crossbow || options.repeater || options.concussion) out << "  SetInv(player, 12, 100); // power\n";
    if (options.railgun) out << "  SetInv(player, 15, 10); // rail charges\n";
    out << "  SetFireWait(player, -1);\n";
    out << "  SetMountWait(player, 0);\n";
    out << "  SetCurInvWeapon(player, 0);\n";
    out << "  SelectWeapon(player, AutoSelectWeapon(player, 1));\n";
    out << "  SetTimer(15);\n";
    // Legacy SED intentionally falls through once so Force state is applied
    // immediately, then the timer repeats it after startup settles.
    out << "\n";
    out << "timer:\n";
    out << "  SetInv(player, 20, " << std::clamp(options.forceRank, 0, 8) << ");\n";
    out << "  SetInv(player, 14, 4 * 50);\n";
    out << "  jkSyncForcePowers();\n";
    out << "  Return;\n\n";
    out << "end\n";
    return out.str();
}

bool writeTextFileAtomically(
    const std::filesystem::path& path,
    const std::string& text,
    std::string* error)
{
    const QString qPath = QString::fromStdWString(path.wstring());
    const QFileInfo info(qPath);
    QDir directory{};
    if (!directory.mkpath(info.absolutePath()))
    {
        if (error)
        {
            *error = "Failed to create directory: " + toUtf8(info.absolutePath());
        }
        return false;
    }

    QSaveFile file(qPath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
    {
        if (error)
        {
            *error = toUtf8(file.errorString());
        }
        return false;
    }
    if (file.write(text.data(), static_cast<qint64>(text.size())) != static_cast<qint64>(text.size()))
    {
        if (error)
        {
            *error = toUtf8(file.errorString());
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit())
    {
        if (error)
        {
            *error = toUtf8(file.errorString());
        }
        return false;
    }
    return true;
}

} // namespace sed::ui::episode
