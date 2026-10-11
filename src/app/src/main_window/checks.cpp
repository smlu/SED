#include "main_window/common.hpp"

#include "ui/checks/check_rebuild_utils.hpp"
#include "ui/checks/level_check_utils.hpp"
#include "ui/project/project_startup_utils.hpp"

namespace sed
{

void MainWindow::scheduleRebuildChecks(const editor::DocumentDirty dirty)
{
    if (!checksTabs_ || !consistencyChecks_ || !resourceChecks_ || !checksRebuildTimer_)
    {
        return;
    }

    if (!ui::checks::checksDirtyIsRelevant(dirty))
    {
        return;
    }

    // Debounce: coalesce frequent edits (e.g. live scrubbing) into a single checks rebuild.
    checksRebuildTimer_->start(200);
}

void MainWindow::refreshThingChecks(
    const std::vector<std::int32_t>& thingIndices,
    const bool refreshResources,
    const bool containmentAlreadyResolved)
{
    if (!checksTabs_ || !consistencyChecks_ || !resourceChecks_ || thingIndices.empty())
    {
        return;
    }

    auto removeOldRows = [this, &thingIndices](
        QTreeWidget* tree,
        const QString& placeholder,
        std::unordered_map<int, std::vector<QTreeWidgetItem*>>& rowsByThing)
    {
        // A targeted Thing transform must not scan every checker row. Large
        // projects can contain thousands of unrelated diagnostics, and the
        // former O(total rows) walk visibly stalled the UI/audio at mouse-up.
        // Full rebuilds populate this keyed index; targeted refresh cost is
        // now proportional only to rows owned by the changed Things.
        if (tree->topLevelItemCount() == 1)
        {
            QTreeWidgetItem* item = tree->topLevelItem(0);
            if (item && item->text(1) == placeholder)
            {
                delete tree->takeTopLevelItem(0);
            }
        }

        for (const std::int32_t thingIndex : thingIndices)
        {
            const auto it = rowsByThing.find(thingIndex);
            if (it == rowsByThing.end())
            {
                continue;
            }
            thingCheckRowsVisited_ += static_cast<std::uint64_t>(it->second.size());
            for (QTreeWidgetItem* item : it->second)
            {
                delete item;
            }
            rowsByThing.erase(it);
        }
    };

    auto addIssue = [](
        QTreeWidget* tree,
        std::unordered_map<int, std::vector<QTreeWidgetItem*>>& rowsByThing,
        const ui::checks::CheckIssue& issue)
    {
        auto* item = new QTreeWidgetItem(tree);
        switch (issue.severity)
        {
            case ui::checks::CheckSeverity::info:
                item->setText(0, QStringLiteral("Info"));
                item->setForeground(0, QBrush(QColor(180, 200, 255)));
                break;
            case ui::checks::CheckSeverity::warning:
                item->setText(0, QStringLiteral("Warning"));
                item->setForeground(0, QBrush(QColor(255, 210, 120)));
                break;
            case ui::checks::CheckSeverity::error:
                item->setText(0, QStringLiteral("Error"));
                item->setForeground(0, QBrush(QColor(255, 120, 120)));
                break;
        }
        item->setText(1, issue.message);
        if (issue.navKind)
        {
            item->setData(0, kOutlinerKindRole, static_cast<int>(*issue.navKind));
        }
        if (issue.navIndex)
        {
            item->setData(0, kOutlinerIndexRole, *issue.navIndex);
        }
        if (issue.navSector)
        {
            item->setData(0, kOutlinerSectorRole, *issue.navSector);
        }
        if (issue.navSurface)
        {
            item->setData(0, kOutlinerSurfaceRole, *issue.navSurface);
        }
        if (issue.navKind && *issue.navKind == OutlinerKind::thing && issue.navIndex)
        {
            rowsByThing[*issue.navIndex].push_back(item);
        }
    };

    auto actualIssueCount = [](const QTreeWidget* tree)
    {
        return tree->topLevelItemCount();
    };

    auto finishTree = [this, &actualIssueCount](
        QTreeWidget* tree,
        const QString& label,
        const QString& placeholder)
    {
        const int count = actualIssueCount(tree);
        if (count == 0)
        {
            auto* item = new QTreeWidgetItem(tree);
            item->setText(0, QStringLiteral("Info"));
            item->setForeground(0, QBrush(QColor(180, 200, 255)));
            item->setText(1, placeholder);
        }
        const int tabIndex = checksTabs_->indexOf(tree);
        if (tabIndex >= 0)
        {
            checksTabs_->setTabText(
                tabIndex,
                QStringLiteral("%1 (%2)").arg(label).arg(count));
        }
        // Severity labels have fixed text and the column was sized by the full
        // rebuild. Re-layouting the complete tree after every Thing transform
        // is unnecessary and can dominate release latency in large projects.
    };

    QSettings settings = settingsStore();
    const Level& level = document_.level();
    const ui::checks::CheckRuleSelection selection =
        ui::checks::loadCheckRuleSelection(settings, level.kind);

    const QString consistencyPlaceholder = QStringLiteral("No consistency issues found.");
    removeOldRows(
        consistencyChecks_,
        consistencyPlaceholder,
        consistencyThingCheckRows_);
    for (const ui::checks::CheckIssue& issue
        : ui::checks::collectThingConsistencyCheckIssues(
            level,
            selection,
            thingIndices,
            containmentAlreadyResolved))
    {
        addIssue(consistencyChecks_, consistencyThingCheckRows_, issue);
    }
    finishTree(
        consistencyChecks_,
        QStringLiteral("Consistency"),
        consistencyPlaceholder);

    if (refreshResources)
    {
        const QString resourcePlaceholder = QStringLiteral("No resource issues found.");
        removeOldRows(
            resourceChecks_,
            resourcePlaceholder,
            resourceThingCheckRows_);
        for (const ui::checks::CheckIssue& issue
            : ui::checks::collectThingResourceCheckIssues(
                level,
                resources_,
                templates_,
                selection,
                thingIndices))
        {
            addIssue(resourceChecks_, resourceThingCheckRows_, issue);
        }
        finishTree(
            resourceChecks_,
            QStringLiteral("Resources"),
            resourcePlaceholder);
    }
}

void MainWindow::showCheckerConfigurationDialog(const ui::checks::CheckRuleGroup group)
{
    const ProjectType project = document_.level().kind;
    const std::vector<ui::checks::CheckRuleDefinition> definitions =
        ui::checks::checkRuleDefinitions(group, project);

    QSettings settings = settingsStore();
    ui::checks::CheckRuleSelection selection =
        ui::checks::loadCheckRuleSelection(settings, project);

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("checkerConfigurationDialog"));
    dialog.setProperty("checkerRuleGroup", static_cast<int>(group));
    dialog.setProperty("checkerProjectType", static_cast<int>(project));
    dialog.setWindowTitle(group == ui::checks::CheckRuleGroup::consistency
            ? QStringLiteral("Consistency Checker Settings")
            : QStringLiteral("Resource Checker Settings"));
    dialog.resize(760, 500);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* projectLabel = new QLabel(
        QStringLiteral("Rules for %1").arg(ui::project::projectTypeLabel(project)),
        &dialog);
    projectLabel->setObjectName(QStringLiteral("checkerProjectLabel"));
    layout->addWidget(projectLabel);

    QComboBox* geometryPrecision = nullptr;
    if ( group == ui::checks::CheckRuleGroup::consistency )
    {
        auto* precisionRow = new QHBoxLayout();
        auto* precisionLabel = new QLabel(QStringLiteral("Geometry precision"), &dialog);
        precisionLabel->setObjectName(QStringLiteral("checkerGeometryPrecisionLabel"));
        geometryPrecision = new QComboBox(&dialog);
        geometryPrecision->setObjectName(QStringLiteral("checkerGeometryPrecision"));
        geometryPrecision->setAccessibleName(QStringLiteral("Geometry precision"));
        geometryPrecision->addItem(QStringLiteral("Game-compatible"),
            static_cast<int>(ui::checks::GeometryCheckPrecision::GameCompatible));
        geometryPrecision->addItem(QStringLiteral("Precise"),
            static_cast<int>(ui::checks::GeometryCheckPrecision::Precise));
        geometryPrecision->setCurrentIndex(selection.geometryPrecision
            == ui::checks::GeometryCheckPrecision::Precise ? 1 : 0);
        precisionLabel->setBuddy(geometryPrecision);
        precisionRow->addWidget(precisionLabel);
        precisionRow->addWidget(geometryPrecision);
        precisionRow->addStretch(1);
        layout->addLayout(precisionRow);

        auto* precisionWarning = new QLabel(QStringLiteral(
            "Precise checks can report coordinate-rounding differences in original game levels."), &dialog);
        precisionWarning->setObjectName(QStringLiteral("checkerGeometryPrecisionWarning"));
        precisionWarning->setWordWrap(true);
        layout->addWidget(precisionWarning);
        const auto refreshPrecisionWarning = [geometryPrecision, precisionWarning]()
        {
            precisionWarning->setVisible(geometryPrecision->currentData().toInt()
                == static_cast<int>(ui::checks::GeometryCheckPrecision::Precise));
        };
        connect(geometryPrecision, &QComboBox::currentIndexChanged, &dialog,
            [refreshPrecisionWarning](int)
            {
                refreshPrecisionWarning();
            });
        refreshPrecisionWarning();
    }

    auto* search = new QLineEdit(&dialog);
    search->setObjectName(QStringLiteral("checkerRuleSearch"));
    search->setPlaceholderText(QStringLiteral("Search checks..."));
    search->setAccessibleName(QStringLiteral("Search checker rules"));
    search->setClearButtonEnabled(true);
    layout->addWidget(search);

    auto* table = new QTableWidget(static_cast<int>(definitions.size()), 3, &dialog);
    table->setObjectName(QStringLiteral("checkerRuleTable"));
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);

    auto* enableHeader = new QTableWidgetItem(QStringLiteral("Active"));
    enableHeader->setToolTip(QStringLiteral("Click to activate or deactivate every listed rule"));
    enableHeader->setTextAlignment(Qt::AlignCenter);
    table->setHorizontalHeaderItem(0, enableHeader);
    table->setHorizontalHeaderItem(1, new QTableWidgetItem(QStringLiteral("Name")));
    table->setHorizontalHeaderItem(2, new QTableWidgetItem(QStringLiteral("Description")));

    auto fromView = [](const std::string_view text)
    {
        return QString::fromUtf8(text.data(), static_cast<int>(text.size()));
    };

    auto ruleForRow = [](const QTableWidget* ruleTable, const int row)
        -> std::optional<ui::checks::CheckRule>
    {
        const QTableWidgetItem* item = ruleTable ? ruleTable->item(row, 0) : nullptr;
        bool ok = false;
        const int value = item ? item->data(Qt::UserRole).toInt(&ok) : -1;
        if (!ok || value < 0
            || value >= static_cast<int>(ui::checks::CheckRule::count))
        {
            return std::nullopt;
        }
        return static_cast<ui::checks::CheckRule>(value);
    };

    for (std::size_t index = 0; index < definitions.size(); ++index)
    {
        const ui::checks::CheckRuleDefinition& definition = definitions[index];
        const int row = static_cast<int>(index);
        auto* enabledItem = new QTableWidgetItem{};
        enabledItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        enabledItem->setCheckState(
            selection.isEnabled(definition.rule) ? Qt::Checked : Qt::Unchecked);
        enabledItem->setData(Qt::UserRole, static_cast<int>(definition.rule));
        enabledItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, 0, enabledItem);

        auto* nameItem = new QTableWidgetItem(fromView(definition.name));
        nameItem->setData(Qt::UserRole, static_cast<int>(definition.rule));
        table->setItem(row, 1, nameItem);
        table->setItem(row, 2, new QTableWidgetItem(fromView(definition.description)));
    }

    // Present a stable alphabetical list. Explicit row identity stays in
    // Qt::UserRole so programmatic reordering never changes which rule is saved.
    table->sortItems(1, Qt::AscendingOrder);

    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table->setColumnWidth(0, 78);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(table, 1);

    connect(search, &QLineEdit::textChanged, &dialog,
        [table](const QString& text)
        {
            const QString needle = text.trimmed();
            for (int row = 0; row < table->rowCount(); ++row)
            {
                const QString name = table->item(row, 1)
                    ? table->item(row, 1)->text()
                    : QString{};
                const QString description = table->item(row, 2)
                    ? table->item(row, 2)->text()
                    : QString{};
                table->setRowHidden(
                    row,
                    !needle.isEmpty()
                        && !name.contains(needle, Qt::CaseInsensitive)
                        && !description.contains(needle, Qt::CaseInsensitive));
            }
        });

    auto* enableAll = new QCheckBox(table->horizontalHeader());
    enableAll->setObjectName(QStringLiteral("checkerEnableAllHeader"));
    enableAll->setAccessibleName(QStringLiteral("Activate or deactivate all checker rules"));
    enableAll->setToolTip(QStringLiteral("Activate or deactivate every listed rule"));
    enableAll->setTristate(true);
    enableAll->show();

    auto positionEnableAll = [table, enableAll]()
    {
        QHeaderView* header = table->horizontalHeader();
        const int indicatorWidth = std::max(18, enableAll->sizeHint().width());
        enableAll->setGeometry(
            header->sectionViewportPosition(0) + 5,
            std::max(0, (header->height() - enableAll->sizeHint().height()) / 2),
            indicatorWidth,
            enableAll->sizeHint().height());
        enableAll->raise();
    };
    connect(table->horizontalHeader(), &QHeaderView::sectionResized, &dialog,
        [positionEnableAll](int, int, int)
        {
            positionEnableAll();
        });
    connect(table->horizontalHeader(), &QHeaderView::geometriesChanged, &dialog, positionEnableAll);
    QTimer::singleShot(0, &dialog, positionEnableAll);

    auto refreshEnableHeader = [table, enableAll]()
    {
        int enabledCount = 0;
        for (int row = 0; row < table->rowCount(); ++row)
        {
            if (const QTableWidgetItem* item = table->item(row, 0);
                item && item->checkState() == Qt::Checked)
            {
                ++enabledCount;
            }
        }

        if (enabledCount == 0)
        {
            const QSignalBlocker blocker(enableAll);
            enableAll->setCheckState(Qt::Unchecked);
        }
        else if (enabledCount == table->rowCount())
        {
            const QSignalBlocker blocker(enableAll);
            enableAll->setCheckState(Qt::Checked);
        }
        else
        {
            const QSignalBlocker blocker(enableAll);
            enableAll->setCheckState(Qt::PartiallyChecked);
        }
    };
    refreshEnableHeader();

    connect(table, &QTableWidget::itemChanged, &dialog,
        [refreshEnableHeader](QTableWidgetItem* item)
        {
            if (item && item->column() == 0)
            {
                refreshEnableHeader();
            }
        });
    connect(enableAll, &QCheckBox::clicked, &dialog,
        [table, enableAll, refreshEnableHeader]()
        {
            const Qt::CheckState next = enableAll->checkState() == Qt::Unchecked
                ? Qt::Unchecked
                : Qt::Checked;
            const QSignalBlocker blocker(table);
            for (int row = 0; row < table->rowCount(); ++row)
            {
                if (QTableWidgetItem* item = table->item(row, 0))
                {
                    item->setCheckState(next);
                }
            }
            refreshEnableHeader();
        });

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults,
        &dialog);
    QPushButton* restoreDefaults = buttons->button(QDialogButtonBox::RestoreDefaults);
    if (restoreDefaults)
    {
        restoreDefaults->setObjectName(QStringLiteral("checkerRestoreDefaultsButton"));
        restoreDefaults->setText(QStringLiteral("Restore Defaults"));
        connect(restoreDefaults, &QPushButton::clicked, &dialog,
            [table, project, refreshEnableHeader, ruleForRow, geometryPrecision]()
            {
                const ui::checks::CheckRuleSelection defaults =
                    ui::checks::defaultCheckRuleSelection(project);
                if ( geometryPrecision )
                {
                    geometryPrecision->setCurrentIndex(0);
                }
                const QSignalBlocker blocker(table);
                for (int row = 0; row < table->rowCount(); ++row)
                {
                    const std::optional<ui::checks::CheckRule> rule =
                        ruleForRow(table, row);
                    if (rule)
                    {
                        if (QTableWidgetItem* item = table->item(row, 0))
                        {
                            item->setCheckState(defaults.isEnabled(*rule)
                                    ? Qt::Checked
                                    : Qt::Unchecked);
                        }
                    }
                }
                refreshEnableHeader();
            });
    }

    connect(buttons, &QDialogButtonBox::accepted, &dialog,
        [this, &dialog, table, project, group, selection, ruleForRow, geometryPrecision]() mutable
        {
            for (int row = 0; row < table->rowCount(); ++row)
            {
                const std::optional<ui::checks::CheckRule> rule =
                    ruleForRow(table, row);
                const QTableWidgetItem* item = table->item(row, 0);
                if (rule)
                {
                    selection.setEnabled(
                        *rule,
                        item && item->checkState() == Qt::Checked);
                }
            }

            if ( geometryPrecision )
            {
                selection.geometryPrecision = geometryPrecision->currentData().toInt()
                    == static_cast<int>(ui::checks::GeometryCheckPrecision::Precise)
                    ? ui::checks::GeometryCheckPrecision::Precise
                    : ui::checks::GeometryCheckPrecision::GameCompatible;
            }

            QSettings store = settingsStore();
            ui::checks::saveCheckRuleSelection(store, project, group, selection);
            store.sync();
            rebuildChecks();
            dialog.accept();
        });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::rebuildChecks()
{
    if (!checksTabs_ || !consistencyChecks_ || !resourceChecks_)
    {
        return;
    }

    consistencyThingCheckRows_.clear();
    resourceThingCheckRows_.clear();
    consistencyChecks_->clear();
    resourceChecks_->clear();

    auto setSeverityCell = [](QTreeWidgetItem* item, const MessageSeverity severity)
    {
        if (!item)
        {
            return;
        }

        switch (severity)
        {
            case MessageSeverity::verbose:
            {
                item->setText(0, "Verbose");
                item->setForeground(0, QBrush(QColor(100, 100, 100)));
                break;
            }
            case MessageSeverity::trace:
            {
                item->setText(0, "Trace");
                item->setForeground(0, QBrush(QColor(120, 120, 120)));
                break;
            }
            case MessageSeverity::debug:
            {
                item->setText(0, "Debug");
                item->setForeground(0, QBrush(QColor(150, 150, 150)));
                break;
            }
            case MessageSeverity::info:
            {
                item->setText(0, "Info");
                item->setForeground(0, QBrush(QColor(180, 200, 255)));
                break;
            }
            case MessageSeverity::warning:
            {
                item->setText(0, "Warning");
                item->setForeground(0, QBrush(QColor(255, 210, 120)));
                break;
            }
            case MessageSeverity::error:
            {
                item->setText(0, "Error");
                item->setForeground(0, QBrush(QColor(255, 120, 120)));
                break;
            }
            case MessageSeverity::fatal:
            {
                item->setText(0, "Fatal");
                item->setForeground(0, QBrush(QColor(255, 80, 80)));
                break;
            }
        }
    };

    auto addIssue = [&](QTreeWidget* tree,
                        const MessageSeverity severity,
                        const QString& message,
                        const std::optional<OutlinerKind> navKind = std::nullopt,
                        const std::optional<int> navIndex = std::nullopt,
                        const std::optional<int> navSector = std::nullopt,
                        const std::optional<int> navSurface = std::nullopt) -> QTreeWidgetItem*
    {
        if (!tree)
        {
            return nullptr;
        }

        auto* item = new QTreeWidgetItem(tree);
        setSeverityCell(item, severity);
        item->setText(1, message);

        // Navigation metadata (reuses outliner roles for consistency).
        if (navKind)
        {
            item->setData(0, kOutlinerKindRole, static_cast<int>(*navKind));
        }
        if (navIndex)
        {
            item->setData(0, kOutlinerIndexRole, *navIndex);
        }
        if (navSector)
        {
            item->setData(0, kOutlinerSectorRole, *navSector);
        }
        if (navSurface)
        {
            item->setData(0, kOutlinerSurfaceRole, *navSurface);
        }
        else if (navKind && *navKind == OutlinerKind::surface && navIndex)
        {
            item->setData(0, kOutlinerSurfaceRole, *navIndex);
        }
        if (navKind && *navKind == OutlinerKind::thing && navIndex)
        {
            auto& rows = tree == consistencyChecks_
                ? consistencyThingCheckRows_
                : resourceThingCheckRows_;
            rows[*navIndex].push_back(item);
        }
        return item;
    };

    auto toMessageSeverity = [](const ui::checks::CheckSeverity severity) -> MessageSeverity
    {
        switch (severity)
        {
            case ui::checks::CheckSeverity::info:
                return MessageSeverity::info;
            case ui::checks::CheckSeverity::warning:
                return MessageSeverity::warning;
            case ui::checks::CheckSeverity::error:
                return MessageSeverity::error;
        }

        return MessageSeverity::info;
    };

    const Level& level = document_.level();

    // Step 1: collect structural consistency issues through testable helpers, then
    // adapt each result into the tree row format used by the checks dock.
    QSettings settings = settingsStore();
    const ui::checks::CheckRuleSelection checkSelection =
        ui::checks::loadCheckRuleSelection(settings, level.kind);
    const auto consistencyIssues = ui::checks::collectConsistencyCheckIssues(
        level,
        checkSelection);
    int consistencyCount = static_cast<int>(consistencyIssues.size());
    for (const ui::checks::CheckIssue& issue : consistencyIssues)
    {
        addIssue(
            consistencyChecks_,
            toMessageSeverity(issue.severity),
            issue.message,
            issue.navKind,
            issue.navIndex,
            issue.navSector,
            issue.navSurface);
    }

    if (consistencyCount == 0)
    {
        addIssue(consistencyChecks_, MessageSeverity::info, "No consistency issues found.");
    }

    // Step 2: collect resource availability issues with the current mounted locator state.
    const auto resourceIssues = ui::checks::collectResourceCheckIssues(
        level,
        resources_,
        templates_,
        checkSelection);
    int resourceCount = static_cast<int>(resourceIssues.size());
    for (const ui::checks::CheckIssue& issue : resourceIssues)
    {
        addIssue(
            resourceChecks_,
            toMessageSeverity(issue.severity),
            issue.message,
            issue.navKind,
            issue.navIndex,
            issue.navSector,
            issue.navSurface);
    }

    if (resourceCount == 0)
    {
        addIssue(resourceChecks_, MessageSeverity::info, "No resource issues found.");
    }

    // Step 3: refresh tab counters and first-column widths after both trees are populated.
    auto updateTabLabel = [&](QTreeWidget* tree, const QString& base, const int count)
    {
        if (!checksTabs_ || !tree)
        {
            return;
        }

        const int tabIndex = checksTabs_->indexOf(tree);
        if (tabIndex < 0)
        {
            return;
        }

        checksTabs_->setTabText(tabIndex, QString("%1 (%2)").arg(base).arg(count));
    };

    updateTabLabel(consistencyChecks_, "Consistency", consistencyCount);
    updateTabLabel(resourceChecks_, "Resources", resourceCount);

    consistencyChecks_->resizeColumnToContents(0);
    resourceChecks_->resizeColumnToContents(0);
}

} // namespace sed
