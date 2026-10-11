#include "main_window/common.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "ui/checks/level_check_utils.hpp"
#include "ui/components/preferred_width_dock_widget.hpp"

namespace sed
{

namespace
{

bool selectionHasEntityItems(const editor::Selection& selection) noexcept
{
    return !selection.sectors.empty()
        || !selection.surfaces.empty()
        || !selection.surfaceVertices.empty()
        || !selection.things.empty()
        || !selection.frames.empty()
        || !selection.lights.empty();
}

QString outlinerDockStyleSheet()
{
    return QStringLiteral(R"(
QDockWidget#outlinerDock
{
    background-color: #2b2b2b;
    color: #eeeeee;
}

QDockWidget#outlinerDock::title
{
    background-color: #252525;
    border-bottom: 1px solid #414141;
    padding: 6px 8px;
    text-align: left;
}

QDockWidget#outlinerDock::close-button,
QDockWidget#outlinerDock::float-button
{
    background-color: transparent;
    border: 0;
    padding: 2px;
}

QDockWidget#outlinerDock::close-button:hover,
QDockWidget#outlinerDock::float-button:hover
{
    background-color: #3a3a3a;
}

QWidget#outlinerDockRoot
{
    background-color: #2b2b2b;
}

QTabBar#outlinerTabs::tab
{
    background-color: transparent;
    border: 0;
    border-bottom: 2px solid transparent;
    color: #bfc3c8;
    padding: 7px 10px 6px 10px;
}

QTabBar#outlinerTabs::tab:hover
{
    background-color: #303030;
    color: #ffffff;
}

QTabBar#outlinerTabs::tab:selected
{
    background-color: #303030;
    border-bottom-color: #8498ad;
    color: #ffffff;
}

QLabel#outlinerCountLabel
{
    color: #c7cbd0;
    padding: 1px 2px;
}

QTreeWidget#outlinerTree,
QTreeWidget#propertiesTree,
QTreeWidget#thingParametersTree,
QListWidget#cogLinksList
{
    background-color: #202020;
    border: 1px solid #414141;
    color: #eeeeee;
    outline: 0;
}

QTreeWidget#outlinerTree::item,
QListWidget#cogLinksList::item
{
    border-bottom: 1px solid #303030;
    padding: 3px 5px;
}

QTreeWidget#propertiesTree::item,
QTreeWidget#thingParametersTree::item
{
    border-bottom: 1px solid #303030;
    padding: 1px 5px;
}

QTreeWidget#outlinerTree::item:hover,
QTreeWidget#propertiesTree::item:hover,
QTreeWidget#thingParametersTree::item:hover,
QListWidget#cogLinksList::item:hover
{
    background-color: #34373a;
}

QTreeWidget#outlinerTree::item:selected,
QTreeWidget#propertiesTree::item:selected,
QTreeWidget#thingParametersTree::item:selected,
QListWidget#cogLinksList::item:selected
{
    background-color: #4a5159;
    color: #ffffff;
}

QTreeWidget#propertiesTree QHeaderView::section,
QTreeWidget#thingParametersTree QHeaderView::section
{
    background-color: #303030;
    border: 0;
    border-right: 1px solid #484848;
    border-bottom: 1px solid #484848;
    color: #eeeeee;
    padding: 5px 7px;
}

QSplitter#outlinerSplitter::handle
{
    background-color: #202020;
    margin: 2px 0;
}

QSplitter#thingParameterSectionsSplitter::handle
{
    background-color: transparent;
    border-top: 1px solid #3b3e42;
    margin: 1px 10px 2px 10px;
}

QWidget#thingParametersSection
{
    background-color: transparent;
    border: 0;
}

QWidget#thingParametersSection > QToolButton#pref_section_header
{
    background-color: transparent;
    border: 0;
    border-radius: 0;
    color: #d5d8dc;
    padding: 2px 4px;
    text-align: left;
}

QWidget#thingParametersSection > QToolButton#pref_section_header:hover
{
    background-color: #303236;
}

QWidget#thingParametersSection > QFrame#pref_section_content
{
    background-color: transparent;
    border: 0;
}
)");
}


} // namespace

void MainWindow::setupDockWidgets()
{
    setDockNestingEnabled(true);
    setDockOptions(
        dockOptions()
        | QMainWindow::AllowNestedDocks
        | QMainWindow::AllowTabbedDocks
        | QMainWindow::GroupedDragging
        | QMainWindow::AnimatedDocks);

    auto* dock = new ui::PreferredWidthDockWidget("Outliner", this);
    dock->setPreferredWidth(kDefaultOutlinerDockWidth);
    outlinerDock_ = dock;
    dock->setObjectName("outlinerDock");
    dock->setStyleSheet(outlinerDockStyleSheet());
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    // Keep all four entity tabs readable while preserving enough room for property editors.
    dock->setMinimumWidth(kMinimumOutlinerDockWidth);
    auto* dockRoot = new QWidget(dock);
    dockRoot->setObjectName("outlinerDockRoot");
    auto* dockLayout = new QVBoxLayout(dockRoot);
    dockLayout->setContentsMargins(8, 7, 8, 8);
    dockLayout->setSpacing(7);

    auto* outlinerHeader = new QWidget(dockRoot);
    outlinerHeader->setObjectName("outlinerHeader");
    auto* outlinerHeaderLayout = new QHBoxLayout(outlinerHeader);
    outlinerHeaderLayout->setContentsMargins(0, 0, 0, 0);
    outlinerHeaderLayout->setSpacing(6);

    outlinerTabs_ = new QTabBar(outlinerHeader);
    outlinerTabs_->setObjectName("outlinerTabs");
    outlinerTabs_->setExpanding(true);
    outlinerTabs_->setUsesScrollButtons(false);
    outlinerTabs_->setElideMode(Qt::ElideRight);
    outlinerTabs_->setDocumentMode(true);
    outlinerTabs_->addTab("Sectors");
    outlinerTabs_->addTab("Things");
    outlinerTabs_->addTab("COGs");
    outlinerTabs_->addTab("Lights");
    outlinerTabs_->setTabToolTip(0, QStringLiteral("Level geometry"));
    outlinerTabs_->setTabToolTip(1, QStringLiteral("Game objects"));
    outlinerTabs_->setTabToolTip(2, QStringLiteral("COG script instances"));
    outlinerTabs_->setTabToolTip(3, QStringLiteral("Baked lights"));
    outlinerTabs_->setIconSize(QSize(18, 18));
    outlinerTabs_->setTabIcon(0, ui::makeViewportIcon(ui::ViewportIcon::outlinerSector));
    outlinerTabs_->setTabIcon(1, ui::makeViewportIcon(ui::ViewportIcon::outlinerThing));
    outlinerTabs_->setTabIcon(2, ui::makeViewportIcon(ui::ViewportIcon::outlinerCogs));
    outlinerTabs_->setTabIcon(3, ui::makeViewportIcon(ui::ViewportIcon::outlinerLightBulb));
    outlinerTabs_->setCurrentIndex(0);
    refreshOutlinerTabCaptions();
    outlinerHeaderLayout->addWidget(outlinerTabs_, 1);

    const QIcon outlinerVisibleLayersIcon =
        ui::makeViewportIcon(ui::ViewportIcon::visibilityEye);
    const QIcon outlinerHiddenLayersIcon =
        ui::makeCrossedOutViewportIcon(outlinerVisibleLayersIcon);
    outlinerShowHiddenLayerEntities_ = settingsStore()
        .value("outliner/show_hidden_layer_entities", false)
        .toBool();
    outlinerShowHiddenLayersButton_ = new QToolButton(outlinerHeader);
    outlinerShowHiddenLayersButton_->setObjectName(
        QStringLiteral("outlinerShowHiddenLayerEntities"));
    outlinerShowHiddenLayersButton_->setAutoRaise(true);
    outlinerShowHiddenLayersButton_->setCheckable(true);
    outlinerShowHiddenLayersButton_->setChecked(
        outlinerShowHiddenLayerEntities_);
    outlinerShowHiddenLayersButton_->setIcon(
        outlinerShowHiddenLayerEntities_
            ? outlinerVisibleLayersIcon
            : outlinerHiddenLayersIcon);
    outlinerShowHiddenLayersButton_->setIconSize(QSize(17, 17));
    outlinerShowHiddenLayersButton_->setToolTip(
        QStringLiteral("Show entities on hidden layers in the Outliner (read-only)"));
    outlinerHeaderLayout->addWidget(outlinerShowHiddenLayersButton_);
    connect(outlinerShowHiddenLayersButton_, &QToolButton::toggled, this,
        [this, outlinerVisibleLayersIcon, outlinerHiddenLayersIcon](const bool checked)
        {
            outlinerShowHiddenLayerEntities_ = checked;
            if (outlinerShowHiddenLayersButton_)
            {
                outlinerShowHiddenLayersButton_->setIcon(
                    checked ? outlinerVisibleLayersIcon : outlinerHiddenLayersIcon);
            }
            settingsStore().setValue(
                "outliner/show_hidden_layer_entities", checked);
            rebuildOutliner();
        });
    dockLayout->addWidget(outlinerHeader);

    cogOutlinerSearchRow_ = new QWidget(dockRoot);
    auto* cogOutlinerSearchLayout = new QHBoxLayout(cogOutlinerSearchRow_);
    cogOutlinerSearchLayout->setContentsMargins(0, 0, 0, 0);
    cogOutlinerSearchLayout->setSpacing(6);
    auto* cogOutlinerSearchLabel = new QLabel("Filter", cogOutlinerSearchRow_);
    cogOutlinerSearchLayout->addWidget(cogOutlinerSearchLabel);
    cogOutlinerSearchEdit_ = new QLineEdit(cogOutlinerSearchRow_);
    cogOutlinerSearchEdit_->setPlaceholderText("COG script name");
    cogOutlinerSearchEdit_->setClearButtonEnabled(true);
    cogOutlinerSearchLayout->addWidget(cogOutlinerSearchEdit_, 1);
    cogOutlinerSearchRow_->setVisible(outlinerTabs_ && outlinerTabs_->currentIndex() == 2);
    dockLayout->addWidget(cogOutlinerSearchRow_);

    outlinerCountLabel_ = new QLabel(dockRoot);
    outlinerCountLabel_->setObjectName("outlinerCountLabel");
    outlinerCountLabel_->setText(" ");
    dockLayout->addWidget(outlinerCountLabel_);

    auto* splitter = new QSplitter(Qt::Vertical, dockRoot);
    splitter->setObjectName("outlinerSplitter");
    splitter->setChildrenCollapsible(false);
    dockLayout->addWidget(splitter, 1);

    auto* outlinerContainer = new QWidget(splitter);
    auto* outlinerContainerLayout = new QGridLayout(outlinerContainer);
    outlinerContainerLayout->setContentsMargins(0, 0, 0, 0);
    outlinerContainerLayout->setSpacing(0);

    outliner_ = new QTreeWidget(outlinerContainer);
    outliner_->setObjectName("outlinerTree");
    outliner_->setHeaderHidden(true);
    outliner_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    outliner_->setRootIsDecorated(true);
    outliner_->setIndentation(14);
    outliner_->setUniformRowHeights(true);
    outliner_->setItemsExpandable(true);
    outliner_->setExpandsOnDoubleClick(false);
    outliner_->setContextMenuPolicy(Qt::CustomContextMenu);
    {
        QFont f = outliner_->font();
        f.setFamily("Segoe UI");
        f.setPointSizeF(f.pointSizeF() + 1.0);
        outliner_->setFont(f);
    }
    outlinerContainerLayout->addWidget(outliner_, 0, 0);
    outlinerContainerLayout->setRowStretch(0, 1);

    // Cogs outliner: legacy-style list actions at the bottom.
    auto* cogOutlinerActions = new QWidget(outlinerContainer);
    auto* cogOutlinerActionsLayout = new QHBoxLayout(cogOutlinerActions);
    cogOutlinerActionsLayout->setContentsMargins(0, 6, 0, 0);
    cogOutlinerActionsLayout->setSpacing(6);

    auto* newCogScriptButton = new QPushButton("New", cogOutlinerActions);
    newCogScriptButton->setObjectName(QStringLiteral("newCogScriptButton"));
    cogOutlinerActionsLayout->addWidget(newCogScriptButton);

    auto* addCogButton = new QPushButton("Add", cogOutlinerActions);
    addCogButton->setObjectName(QStringLiteral("addCogButton"));
    addCogButton->setToolTip("Add COG");
    cogOutlinerActionsLayout->addWidget(addCogButton);

    auto* duplicateCogButton = new QPushButton("Duplicate", cogOutlinerActions);
    duplicateCogButton->setObjectName(QStringLiteral("duplicateCogButton"));
    duplicateCogButton->setToolTip("Duplicate selected COG");
    cogOutlinerActionsLayout->addWidget(duplicateCogButton);

    auto* deleteCogButton = new QPushButton("Delete", cogOutlinerActions);
    deleteCogButton->setObjectName(QStringLiteral("deleteCogButton"));
    deleteCogButton->setToolTip("Delete selected COG");
    cogOutlinerActionsLayout->addWidget(deleteCogButton);
    cogOutlinerActionsLayout->addStretch(1);

    outlinerContainerLayout->addWidget(cogOutlinerActions, 1, 0);
    cogOutlinerActions->setVisible(outlinerTabs_ && outlinerTabs_->currentIndex() == 2);

    auto syncCogOutlinerControls = [this, cogOutlinerActions]()
    {
        const bool isCogTab = outlinerTabs_ && outlinerTabs_->currentIndex() == 2;
        if (cogOutlinerActions)
        {
            cogOutlinerActions->setVisible(isCogTab);
        }
        if (cogOutlinerSearchRow_)
        {
            cogOutlinerSearchRow_->setVisible(isCogTab);
        }
        if (outlinerShowHiddenLayersButton_)
        {
            outlinerShowHiddenLayersButton_->setVisible(!isCogTab);
        }
    };

    connect(addCogButton, &QPushButton::clicked, this, [this]()
        {
            if (addCogAction_)
            {
                addCogAction_->trigger();
            }
        });
    connect(newCogScriptButton, &QPushButton::clicked, this, [this]()
        {
            createNewCogScript();
        });
    newCogScriptButton->setToolTip(
        "Open an unsaved COG script draft, add a placed COG to the level, and edit the script in the COG Script Editor.");
    connect(duplicateCogButton, &QPushButton::clicked, this, [this]()
        {
            if (duplicateCogAction_)
            {
                duplicateCogAction_->trigger();
            }
        });
    connect(deleteCogButton, &QPushButton::clicked, this, [this]()
        {
            if (deleteAction_)
            {
                deleteAction_->trigger();
            }
        });
    connect(outlinerTabs_, &QTabBar::currentChanged, this, [this, syncCogOutlinerControls](const int idx)
        {
            syncCogOutlinerControls();
            if (idx != 2)
            {
                return;
            }

            rebuildOutliner();
        });
    if (cogOutlinerSearchEdit_)
    {
        connect(cogOutlinerSearchEdit_, &QLineEdit::textChanged, this, [this](const QString&)
            {
                if (outlinerTabs_ && outlinerTabs_->currentIndex() == 2)
                {
                    rebuildOutliner();
                }
            });
    }

    auto* cogOutlinerFindShortcut = new QShortcut(QKeySequence::Find, outlinerContainer);
    cogOutlinerFindShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(cogOutlinerFindShortcut, &QShortcut::activated, this, [this]()
        {
            if (!outlinerTabs_ || outlinerTabs_->currentIndex() != 2 || !cogOutlinerSearchEdit_)
            {
                return;
            }

            cogOutlinerSearchEdit_->setFocus(Qt::ShortcutFocusReason);
            cogOutlinerSearchEdit_->selectAll();
        });

    splitter->addWidget(outlinerContainer);

    connect(outliner_, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos)
        {
            if (!outliner_ || !outlinerTabs_)
            {
                return;
            }

            ui::shortcuts::KeySequenceDisplayMenu menu(outliner_);

            const int tab = outlinerTabs_->currentIndex();

            // Sector list: expand/collapse helpers.
            if (tab == 0)
            {
                auto* expandAll = menu.addAction("Expand all");
                connect(expandAll, &QAction::triggered, outliner_, [this]()
                    {
                        if (outliner_)
                        {
                            outliner_->expandAll();
                        }
                    });

                auto* collapseAll = menu.addAction("Collapse all");
                connect(collapseAll, &QAction::triggered, outliner_, [this]()
                    {
                        if (outliner_)
                        {
                            outliner_->collapseAll();
                        }
                    });
            }
            // Cogs: add/delete.
            else if (tab == 2)
            {
                auto* newScript = menu.addAction("New");
                newScript->setToolTip(
                    "Create a new COG script file, add a new placed COG to the level, and open the script in the COG Script Editor.");
                connect(newScript, &QAction::triggered, outliner_, [this]()
                    {
                        createNewCogScript();
                    });

                menu.addSeparator();

                auto* findScripts = menu.addAction("Find scripts");
                connect(findScripts, &QAction::triggered, outliner_, [this]()
                    {
                        if (!cogOutlinerSearchEdit_)
                        {
                            return;
                        }

                        cogOutlinerSearchEdit_->setFocus(Qt::ShortcutFocusReason);
                        cogOutlinerSearchEdit_->selectAll();
                    });

                menu.addSeparator();
                if (addCogAction_)
                {
                    menu.addAction(addCogAction_);
                }
                if (duplicateCogAction_)
                {
                    menu.addAction(duplicateCogAction_);
                }
                if (deleteAction_)
                {
                    if (!menu.actions().empty())
                    {
                        menu.addSeparator();
                    }
                    menu.addAction(deleteAction_);
                }
            }

            if (!menu.actions().empty())
            {
                menu.exec(outliner_->viewport()->mapToGlobal(pos));
            }
        });

    connect(outliner_, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* item)
        {
            if (!item || !outlinerTabs_ || outlinerTabs_->currentIndex() != 0)
            {
                return;
            }

            const OutlinerKind kind = static_cast<OutlinerKind>(item->data(0, kOutlinerKindRole).toInt());
            if (kind == OutlinerKind::sector)
            {
                populateOutlinerSectorChildren(item);
                syncOutlinerSelectionOnly();
            }
            else if (kind == OutlinerKind::surface)
            {
                populateOutlinerSurfaceVertexChildren(item);
                if (item->childCount() > 0)
                {
                    item->child(0)->setExpanded(true);
                }
                syncOutlinerSelectionOnly();
            }
        });

    auto* propertiesContainer = new QWidget(splitter);
    auto* propertiesLayout = new QVBoxLayout(propertiesContainer);
    propertiesLayout->setContentsMargins(0, 0, 0, 0);
    propertiesLayout->setSpacing(6);

    auto* thingParameterSectionsSplitter = new QSplitter(Qt::Vertical, propertiesContainer);
    thingParameterSectionsSplitter->setObjectName("thingParameterSectionsSplitter");
    thingParameterSectionsSplitter->setHandleWidth(4);
    thingParameterSectionsSplitter->setChildrenCollapsible(false);
    propertiesLayout->addWidget(thingParameterSectionsSplitter, 1);

    properties_ = new QTreeWidget(thingParameterSectionsSplitter);
    properties_->setObjectName("propertiesTree");
    properties_->setColumnCount(2);
    properties_->setHeaderLabels({ "Field", "Value" });
    properties_->setSelectionBehavior(QAbstractItemView::SelectRows);
    properties_->setUniformRowHeights(false);
    properties_->setRootIsDecorated(false);
    properties_->setIndentation(0);
    properties_->setExpandsOnDoubleClick(false);
    properties_->setColumnWidth(0, 150);
    if (auto* header = properties_->header())
    {
        header->setStretchLastSection(true);
        header->setSectionResizeMode(0, QHeaderView::Interactive);
        header->setSectionResizeMode(1, QHeaderView::Stretch);
    }
    {
        QFont f = properties_->font();
        f.setFamily("Segoe UI");
        f.setPointSizeF(f.pointSizeF() + 0.5);
        properties_->setFont(f);
    }
    thingParameterSectionsSplitter->addWidget(properties_);

    auto* thingParametersSection = new ui::CollapsibleSection(
        "Thing parameters", thingParameterSectionsSplitter);
    thingParametersSection->setObjectName("thingParametersSection");
    thingParametersSection->setHeaderBold(true);
    thingParametersSection->setContentResizable(true);
    thingParametersPanel_ = thingParametersSection;

    thingParameterSearch_ = new QLineEdit(thingParametersSection);
    thingParameterSearch_->setObjectName(QStringLiteral("prop.thing.parameters.search"));
    thingParameterSearch_->setPlaceholderText(QStringLiteral("Search parameters"));
    thingParameterSearch_->setClearButtonEnabled(true);
    {
        QPixmap searchPixmap(16, 16);
        searchPixmap.fill(Qt::transparent);
        QPainter painter(&searchPixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(QColor(QStringLiteral("#aeb4bc")));
        pen.setWidthF(1.7);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QRectF(2.0, 2.0, 8.5, 8.5));
        painter.drawLine(QPointF(9.0, 9.0), QPointF(14.0, 14.0));
        thingParameterSearch_->addAction(QIcon(searchPixmap), QLineEdit::LeadingPosition);
    }

    thingParameters_ = new QTreeWidget(thingParametersSection);
    thingParameters_->setObjectName("thingParametersTree");
    thingParameters_->setColumnCount(2);
    thingParameters_->setHeaderLabels({ "Parameter", "Value" });
    thingParameters_->setSelectionBehavior(QAbstractItemView::SelectRows);
    thingParameters_->setUniformRowHeights(false);
    thingParameters_->setRootIsDecorated(false);
    thingParameters_->setIndentation(0);
    thingParameters_->setExpandsOnDoubleClick(false);
    thingParameters_->setColumnWidth(0, 150);
    thingParameters_->setMinimumHeight(96);
    if (auto* header = thingParameters_->header())
    {
        header->setStretchLastSection(true);
        header->setSectionResizeMode(0, QHeaderView::Interactive);
        header->setSectionResizeMode(1, QHeaderView::Stretch);
    }
    thingParameters_->setFont(properties_->font());

    thingParametersSection->contentLayout()->setContentsMargins(8, 4, 8, 8);
    thingParametersSection->contentLayout()->setSpacing(5);
    thingParametersSection->contentLayout()->addWidget(thingParameterSearch_);
    thingParametersSection->contentLayout()->addWidget(thingParameters_, 1);
    thingParametersPanel_->setVisible(false);
    thingParameterSectionsSplitter->addWidget(thingParametersPanel_);
    thingParameterSectionsSplitter->setStretchFactor(0, 3);
    thingParameterSectionsSplitter->setStretchFactor(1, 2);

    connect(thingParameterSearch_, &QLineEdit::textChanged, this, [this](const QString& text)
        {
            if (!thingParameters_)
            {
                return;
            }
            const QString needle = text.trimmed();
            for (int row = 0; row < thingParameters_->topLevelItemCount(); ++row)
            {
                QTreeWidgetItem* item = thingParameters_->topLevelItem(row);
                if (!item || !item->data(0, kThingParameterSearchRole).isValid())
                {
                    continue;
                }
                item->setHidden(
                    !needle.isEmpty()
                    && !item->data(0, kThingParameterSearchRole)
                            .toString().contains(needle, Qt::CaseInsensitive));
            }
        });

    auto* cogLinksSection = new ui::CollapsibleSection("COG links", propertiesContainer);
    cogLinksSection->setObjectName("cogLinksSection");
    cogLinksSection->setHeaderBold(true);
    cogLinksPanel_ = cogLinksSection;

    cogLinksList_ = new QListWidget(cogLinksSection);
    cogLinksList_->setObjectName("cogLinksList");
    cogLinksList_->setSelectionMode(QAbstractItemView::SingleSelection);
    cogLinksList_->setAlternatingRowColors(false);
    cogLinksList_->setUniformItemSizes(true);
    cogLinksList_->setMinimumHeight(72);
    cogLinksList_->setMaximumHeight(140);
    cogLinksList_->setToolTip("Double-click a link to focus the linked placed COG variable.");
    cogLinksSection->contentLayout()->setContentsMargins(8, 4, 8, 8);
    cogLinksSection->contentLayout()->setSpacing(4);
    cogLinksSection->contentLayout()->addWidget(cogLinksList_);
    cogLinksPanel_->setVisible(false);
    propertiesLayout->addWidget(cogLinksPanel_, 0);

    splitter->addWidget(propertiesContainer);
    const auto handlePropertyItemDoubleClicked = [this](QTreeWidgetItem* item, int)
        {
            if (!item || !glView_)
            {
                return;
            }

            const auto action = static_cast<PropertyAction>(item->data(0, kPropertyActionRole).toInt());

            if (action == PropertyAction::selectMirrorSurface)
            {
                const int sectorIndex = item->data(0, kPropertySectorRole).toInt();
                const int surfaceIndex = item->data(0, kPropertySurfaceRole).toInt();

                const Level& level = document_.level();
                if (sectorIndex < 0 || surfaceIndex < 0
                    || static_cast<std::size_t>(sectorIndex) >= level.sectors.size()
                    || static_cast<std::size_t>(surfaceIndex) >= level.sectors[static_cast<std::size_t>(sectorIndex)].surfaces.size())
                {
                    return;
                }

                if (editMode_ != EditMode::surface)
                {
                    setEditMode(EditMode::surface);
                }

                document_.selection().clear();
                document_.selection().surfaces.insert(SurfaceRef{ sectorIndex, surfaceIndex });

                glView_->setSelectionState(sectorIndex, surfaceIndex, -1, -1, document_.selection());

                rebuildOutliner();
                updateEditActions();
                rebuildProperties();
                return;
            }

            if (action == PropertyAction::selectFrameValue)
            {
                const int thingIndex = item->data(0, kPropertyThingRole).toInt();
                const int valueIndex = item->data(0, kPropertyValueIndexRole).toInt();

                const Level& level = document_.level();
                if (thingIndex < 0 || valueIndex < 0
                    || static_cast<std::size_t>(thingIndex) >= level.things.size()
                    || static_cast<std::size_t>(valueIndex) >= level.things[static_cast<std::size_t>(thingIndex)].values.size()
                    || level.things[static_cast<std::size_t>(thingIndex)].values[static_cast<std::size_t>(valueIndex)].type != ValueType::frame
                    || !sed::text::iequalsAscii(level.things[static_cast<std::size_t>(thingIndex)].values[static_cast<std::size_t>(valueIndex)].name, "frame"))
                {
                    return;
                }

                if (editMode_ != EditMode::frame)
                {
                    setEditMode(EditMode::frame);
                }

                document_.selection().clear();
                document_.selection().things.insert(thingIndex);
                document_.selection().frames.insert(sed::level::FrameRef{ thingIndex, valueIndex });

                if (outlinerTabs_)
                {
                    outlinerTabs_->setCurrentIndex(1);
                }

                glView_->setSelectionState(-1, -1, thingIndex, -1, document_.selection());
                glView_->setSelectedFrameValueIndex(valueIndex);

                rebuildOutliner();
                updateEditActions();
                rebuildProperties();
                return;
            }

            if (action == PropertyAction::selectThingSector)
            {
                const int thingIndex = item->data(0, kPropertyThingRole).toInt();
                const Level& level = document_.level();
                if (thingIndex < 0
                    || static_cast<std::size_t>(thingIndex) >= level.things.size())
                {
                    return;
                }

                const int sectorIndex = level.things[static_cast<std::size_t>(thingIndex)].sector;
                if (sectorIndex < 0
                    || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
                {
                    return;
                }

                (void)navigateToEntity(
                    OutlinerKind::sector, sectorIndex, -1, -1, true, false);
                return;
            }

            if (action == PropertyAction::selectCogEntityValue)
            {
                const auto kind = static_cast<OutlinerKind>(item->data(0, kPropertyOutlinerKindRole).toInt());
                const int index = item->data(0, kPropertyEntityIndexRole).toInt();
                const int sectorIndex = item->data(0, kPropertySectorRole).toInt();
                const int surfaceIndex = item->data(0, kPropertySurfaceRole).toInt();

                (void)navigateToEntity(kind, index, sectorIndex, surfaceIndex, true, true);
                return;
            }

            if (action == PropertyAction::selectLinkedCogValue)
            {
                const int cogIndex = item->data(0, kPropertyCogRole).toInt();
                const int valueIndex = item->data(0, kPropertyValueIndexRole).toInt();

                const Level& level = document_.level();
                if (cogIndex < 0 || valueIndex < 0
                    || static_cast<std::size_t>(cogIndex) >= level.cogs.size()
                    || static_cast<std::size_t>(valueIndex) >= level.cogs[static_cast<std::size_t>(cogIndex)].values.size())
                {
                    return;
                }

                editor::Selection entitySelection = document_.selection();
                entitySelection.cogs.clear();
                if (selectionHasEntityItems(entitySelection))
                {
                    cogLinkSelectionBackup_ = entitySelection;
                    cogLinkSelectionRestorePending_ = true;
                }
                else
                {
                    cogLinkSelectionBackup_.clear();
                    cogLinkSelectionRestorePending_ = false;
                }

                document_.selection().clear();
                document_.selection().cogs.insert(cogIndex);

                if (outlinerTabs_)
                {
                    outlinerTabs_->setCurrentIndex(2);
                }

                cogValueFocusPending_ = true;
                cogValueFocusCogIndex_ = cogIndex;
                cogValueFocusValueIndex_ = valueIndex;

                glView_->setSelectionState(-1, -1, -1, -1, document_.selection());
                rebuildOutliner();
                updateEditActions();
                rebuildProperties();
                return;
            }

            if (action == PropertyAction::selectCogReferenceValue)
            {
                const int targetCogIndex = item->data(0, kPropertyEntityIndexRole).toInt();
                const int sourceCogIndex = item->data(0, kPropertyCogRole).toInt();
                const int valueIndex = item->data(0, kPropertyValueIndexRole).toInt();

                const Level& level = document_.level();
                if (targetCogIndex >= 0 && static_cast<std::size_t>(targetCogIndex) < level.cogs.size())
                {
                    document_.selection().clear();
                    document_.selection().cogs.insert(targetCogIndex);

                    if (outlinerTabs_)
                    {
                        outlinerTabs_->setCurrentIndex(2);
                    }

                    glView_->setSelectionState(-1, -1, -1, -1, document_.selection());
                    rebuildOutliner();
                    updateEditActions();
                    rebuildProperties();
                    return;
                }

                if (sourceCogIndex < 0 || valueIndex < 0
                    || static_cast<std::size_t>(sourceCogIndex) >= level.cogs.size()
                    || static_cast<std::size_t>(valueIndex) >= level.cogs[static_cast<std::size_t>(sourceCogIndex)].values.size())
                {
                    return;
                }

                const CogValue& value = level.cogs[static_cast<std::size_t>(sourceCogIndex)].values[static_cast<std::size_t>(valueIndex)];
                if (value.cogType != CogType::cog || value.intValue >= 0)
                {
                    return;
                }

                const auto newIndex = pickAndCreatePlacedCog(false);
                if (!newIndex)
                {
                    return;
                }

                std::vector<std::int32_t> targets{};
                targets.reserve(document_.selection().cogs.size());
                for (const std::int32_t idx : document_.selection().cogs)
                {
                    targets.push_back(idx);
                }
                if (targets.empty())
                {
                    targets.push_back(sourceCogIndex);
                }

                const bool applied = document_.applyUndoable(
                    "COG: Reference changed",
                    editor::DocumentDirty::Cogs,
                    [targets, valueIndex, newIndex](editor::DocumentState& state) -> bool
                    {
                        bool any = false;
                        for (const std::int32_t idx : targets)
                        {
                            if (idx < 0 || static_cast<std::size_t>(idx) >= state.level.cogs.size())
                            {
                                continue;
                            }

                            Cog& cog = state.level.cogs[static_cast<std::size_t>(idx)];
                            if (static_cast<std::size_t>(valueIndex) >= cog.values.size())
                            {
                                continue;
                            }

                            CogValue& cogValue = cog.values[static_cast<std::size_t>(valueIndex)];
                            if (cogValue.cogType != CogType::cog || cogValue.intValue == *newIndex)
                            {
                                continue;
                            }

                            cogValue.intValue = *newIndex;
                            any = true;
                        }
                        return any;
                    });

                if (!applied)
                {
                    return;
                }

                cogValueFocusPending_ = true;
                cogValueFocusCogIndex_ = sourceCogIndex;
                cogValueFocusValueIndex_ = valueIndex;
                syncCogScriptWatches();
                rebuildOutliner();
                updateEditActions();
                rebuildProperties();
                return;
            }

            if (action == PropertyAction::openCogResourceValue)
            {
                const int cogIndex = item->data(0, kPropertyCogRole).toInt();
                const int valueIndex = item->data(0, kPropertyValueIndexRole).toInt();
                const int resourceKindValue = item->data(0, kPropertyResourceKindRole).toInt();

                const Level& level = document_.level();
                if (cogIndex < 0 || valueIndex < 0
                    || static_cast<std::size_t>(cogIndex) >= level.cogs.size()
                    || static_cast<std::size_t>(valueIndex) >= level.cogs[static_cast<std::size_t>(cogIndex)].values.size())
                {
                    return;
                }

                QTreeWidget* owner = item->treeWidget();
                QWidget* widget = owner ? owner->itemWidget(item, 1) : nullptr;
                if (!widget)
                {
                    return;
                }

                QLineEdit* edit = qobject_cast<QLineEdit*>(widget);
                if (!edit)
                {
                    edit = widget->findChild<QLineEdit*>();
                }
                if (!edit)
                {
                    return;
                }

                ui::ResourcePickerOptions options{};
                options.kind = static_cast<ui::ResourcePickerKind>(resourceKindValue);
                options.initialName = edit->text().trimmed().toStdString();
                options.preferredDiskRoot = preferredExtractionRoot();
                if (!currentPath_.empty())
                {
                    options.projectDirectory = currentPath_.parent_path();
                }
                options.title = "Pick Resource";
                options.previewCmp = document_.level().getMasterCMP();
                options.previewProjectType = document_.level().kind;
                options.templateOverrides = &document_.level().templates;

                const auto picked = ui::pickResource(
                    this, resourceLocatorSnapshot_, options);
                if (!picked)
                {
                    return;
                }

                edit->setText(QString::fromStdString(*picked));
                QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
                return;
            }

            if (action == PropertyAction::openCogScript)
            {
                const int cogIndex = item->data(0, kPropertyCogRole).toInt();

                const Level& level = document_.level();
                if (cogIndex < 0 || static_cast<std::size_t>(cogIndex) >= level.cogs.size())
                {
                    return;
                }

                const Cog& cog = level.cogs[static_cast<std::size_t>(cogIndex)];
                openCogScriptEditor(cog.name);
                return;
            }
        };
    connect(properties_, &QTreeWidget::itemDoubleClicked,
        this, handlePropertyItemDoubleClicked);
    connect(thingParameters_, &QTreeWidget::itemDoubleClicked,
        this, handlePropertyItemDoubleClicked);
    connect(cogLinksList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item)
        {
            if (!item || !glView_
                || !item->flags().testFlag(Qt::ItemIsEnabled)
                || !item->flags().testFlag(Qt::ItemIsSelectable))
            {
                return;
            }

            const QVariant cogData = item->data(kPropertyCogRole);
            const QVariant valueData = item->data(kPropertyValueIndexRole);
            bool cogIndexValid = false;
            bool valueIndexValid = false;
            const int cogIndex = cogData.toInt(&cogIndexValid);
            const int valueIndex = valueData.toInt(&valueIndexValid);

            const Level& level = document_.level();
            if (!cogData.isValid() || !valueData.isValid()
                || !cogIndexValid || !valueIndexValid
                || cogIndex < 0 || valueIndex < 0
                || static_cast<std::size_t>(cogIndex) >= level.cogs.size()
                || static_cast<std::size_t>(valueIndex) >= level.cogs[static_cast<std::size_t>(cogIndex)].values.size())
            {
                return;
            }

            editor::Selection entitySelection = document_.selection();
            entitySelection.cogs.clear();
            if (selectionHasEntityItems(entitySelection))
            {
                cogLinkSelectionBackup_ = entitySelection;
                cogLinkSelectionRestorePending_ = true;
            }
            else
            {
                cogLinkSelectionBackup_.clear();
                cogLinkSelectionRestorePending_ = false;
            }

            document_.selection().clear();
            document_.selection().cogs.insert(cogIndex);

            if (outlinerTabs_)
            {
                outlinerTabs_->setCurrentIndex(2);
            }

            cogValueFocusPending_ = true;
            cogValueFocusCogIndex_ = cogIndex;
            cogValueFocusValueIndex_ = valueIndex;

            glView_->setSelectionState(-1, -1, -1, -1, document_.selection());
            rebuildOutliner();
            updateEditActions();
            rebuildProperties();
        });

    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 2);
    // Give the item editor (properties) more default space; users can still resize via the splitter.
    splitter->setSizes({ 320, 760 });

    dock->setWidget(dockRoot);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    resizeDocks({ dock }, { kDefaultOutlinerDockWidth }, Qt::Horizontal);
    // Do not "learn" the startup dock width here. During initial layout it can still
    // be the full left-panel span, which then gets treated as the standalone width and
    // keeps reopening the outliner too wide after load/undock/show-hide cycles.
    standaloneOutlinerPreferredWidth_ = kDefaultOutlinerDockWidth;
    // Create and wire the detachable COG editor dock next to the outliner.
    setupCogScriptEditorDock(dock);

    auto* checkDock = new QDockWidget("Checks", this);
    checkDock->setObjectName("checksDock");
    auto* checkRoot = new QWidget(checkDock);
    auto* checkLayout = new QVBoxLayout(checkRoot);
    checkLayout->setContentsMargins(6, 6, 6, 6);
    checkLayout->setSpacing(6);

    auto* checkHeader = new QWidget(checkRoot);
    auto* checkHeaderLayout = new QHBoxLayout(checkHeader);
    checkHeaderLayout->setContentsMargins(0, 0, 0, 0);
    checkHeaderLayout->setSpacing(6);

    auto* runChecksButton = new QPushButton("Run checks", checkHeader);
    runChecksButton->setToolTip("Run level consistency/resource checks");
    connect(runChecksButton, &QPushButton::clicked, this, [this]()
        {
            rebuildChecks();
        });

    checkHeaderLayout->addWidget(runChecksButton);

    checkHeaderLayout->addStretch(1);
    checkLayout->addWidget(checkHeader);

    // Checks: split into two tabs (consistency + resources).
    checksTabs_ = new QTabWidget(checkRoot);
    checksTabs_->setDocumentMode(true);
    checksTabs_->setTabPosition(QTabWidget::North);

    auto makeChecksTree = [&](QWidget* parent) -> QTreeWidget*
    {
        auto* tree = new QTreeWidget(parent);
        tree->setColumnCount(2);
        tree->setHeaderLabels({ "Severity", "Issue" });
        tree->setRootIsDecorated(false);
        tree->setUniformRowHeights(true);
        tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
        tree->setSelectionBehavior(QAbstractItemView::SelectRows);
        tree->setStyleSheet(
            "QTreeWidget::item { border-bottom: 1px solid rgba(214,218,223,42); }"
            "QTreeWidget::item:selected { background-color: rgba(150,156,164,104); color: #ffffff; }"
            "QTreeWidget::item:selected:active { background-color: rgba(150,156,164,104); color: #ffffff; }"
            "QTreeWidget::item:selected:!active { background-color: rgba(150,156,164,72); color: #ffffff; }");
        tree->setColumnWidth(0, 90);
        return tree;
    };

    consistencyChecks_ = makeChecksTree(checksTabs_);
    resourceChecks_ = makeChecksTree(checksTabs_);
    const int consistencyTabIndex = checksTabs_->addTab(consistencyChecks_, "Consistency");
    const int resourceTabIndex = checksTabs_->addTab(resourceChecks_, "Resources");

    auto makeCheckSettingsButton = [&](const QString& objectName,
                                       const QString& accessibleName,
                                       const ui::checks::CheckRuleGroup group) -> QToolButton*
    {
        auto* button = new QToolButton(checksTabs_->tabBar());
        button->setObjectName(objectName);
        button->setAccessibleName(accessibleName);
        button->setToolTip(accessibleName);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIcon(ui::makeViewportIcon(ui::ViewportIcon::settingsCog));
        button->setIconSize(QSize(16, 16));
        button->setFixedSize(22, 22);
        connect(button, &QToolButton::clicked, this, [this, group]()
            {
                showCheckerConfigurationDialog(group);
            });
        return button;
    };

    consistencyCheckSettingsButton_ = makeCheckSettingsButton(
        QStringLiteral("consistencyCheckSettingsButton"),
        QStringLiteral("Configure consistency checks"),
        ui::checks::CheckRuleGroup::consistency);
    resourceCheckSettingsButton_ = makeCheckSettingsButton(
        QStringLiteral("resourceCheckSettingsButton"),
        QStringLiteral("Configure resource checks"),
        ui::checks::CheckRuleGroup::resources);
    checksTabs_->tabBar()->setTabButton(
        consistencyTabIndex,
        QTabBar::RightSide,
        consistencyCheckSettingsButton_);
    checksTabs_->tabBar()->setTabButton(
        resourceTabIndex,
        QTabBar::RightSide,
        resourceCheckSettingsButton_);
    checkLayout->addWidget(checksTabs_, 1);

    checkDock->setWidget(checkRoot);

    auto* msgDock = new QDockWidget("Messages", this);
    msgDock->setObjectName("messagesDock");

    auto* messagesTitleBar = new QWidget(msgDock);
    messagesTitleBar->setObjectName("messagesTitleBar");
    auto* messagesTitleLayout = new QHBoxLayout(messagesTitleBar);
    messagesTitleLayout->setContentsMargins(7, 2, 3, 2);
    messagesTitleLayout->setSpacing(2);
    auto* messagesTitle = new QLabel("Messages", messagesTitleBar);
    messagesTitle->setObjectName("messagesTitleLabel");
    messagesTitleLayout->addWidget(messagesTitle);

    auto* messageSettingsButton = new QToolButton(messagesTitleBar);
    messageSettingsButton->setObjectName("messagesSeveritySettingsButton");
    messageSettingsButton->setAutoRaise(true);
    messageSettingsButton->setIcon(ui::makeViewportIcon(ui::ViewportIcon::mapSettings));
    messageSettingsButton->setIconSize(QSize(16, 16));
    messageSettingsButton->setToolTip("Filter message severities");
    messageSettingsButton->setAccessibleName("Message severity settings");
    messagesTitleLayout->addWidget(messageSettingsButton);
    messagesTitleLayout->addStretch(1);

    auto* messageFloatButton = new QToolButton(messagesTitleBar);
    messageFloatButton->setObjectName("messagesFloatButton");
    messageFloatButton->setAutoRaise(true);
    messageFloatButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
    messageFloatButton->setToolTip("Float or dock Messages");
    messagesTitleLayout->addWidget(messageFloatButton);

    auto* messageCloseButton = new QToolButton(messagesTitleBar);
    messageCloseButton->setObjectName("messagesCloseButton");
    messageCloseButton->setAutoRaise(true);
    messageCloseButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    messageCloseButton->setToolTip("Close Messages");
    messagesTitleLayout->addWidget(messageCloseButton);

    connect(messageSettingsButton, &QToolButton::clicked, this,
        [this, messageSettingsButton]()
        {
            showMessageSeverityPopup(messageSettingsButton);
        });
    connect(messageFloatButton, &QToolButton::clicked, msgDock,
        [msgDock]()
        {
            msgDock->setFloating(!msgDock->isFloating());
        });
    connect(messageCloseButton, &QToolButton::clicked, msgDock, &QDockWidget::close);
    msgDock->setTitleBarWidget(messagesTitleBar);

    messages_ = new QTreeWidget(msgDock);
    messages_->setObjectName("messagesTree");
    messages_->setColumnCount(2);
    messages_->setHeaderLabels({ "Severity", "Message" });
    messages_->setRootIsDecorated(false);
    messages_->setUniformRowHeights(true);
    messages_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    messages_->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* copyMessagesAction = new QAction("Copy", messages_);
    copyMessagesAction->setShortcut(QKeySequence::Copy);
    copyMessagesAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(copyMessagesAction, &QAction::triggered, this, [this]()
        {
            copySelectedMessagesToClipboard();
        });
    messages_->addAction(copyMessagesAction);

    connect(messages_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int)
        {
            showMessageDetails(item);
        });
    msgDock->setWidget(messages_);

    auto* historyDock = new QDockWidget("History", this);
    historyDock->setObjectName("historyDock");
    history_ = new QTreeWidget(historyDock);
    history_->setObjectName("historyTree");
    history_->setColumnCount(1);
    history_->setHeaderLabels({ "Undo/Redo Timeline" });
    history_->setRootIsDecorated(false);
    history_->setUniformRowHeights(true);
    history_->setSelectionMode(QAbstractItemView::SingleSelection);
    history_->setSelectionBehavior(QAbstractItemView::SelectRows);
    history_->setContextMenuPolicy(Qt::CustomContextMenu);
    history_->setStyleSheet(
        "QTreeWidget::item { border-bottom: 1px solid rgba(214,218,223,42); }"
        "QTreeWidget::item:selected { background-color: rgba(150,156,164,104); color: #ffffff; }"
        "QTreeWidget::item:selected:active { background-color: rgba(150,156,164,104); color: #ffffff; }"
        "QTreeWidget::item:selected:!active { background-color: rgba(150,156,164,72); color: #ffffff; }");
    history_->setToolTip("Click an entry to jump to that undo/redo state.");
    historyDock->setWidget(history_);

    connect(history_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int)
        {
            if (!item)
            {
                return;
            }

            if (!documentEditingAllowed())
            {
                // QTreeWidget changes its current row before emitting
                // itemClicked.  Preview is view-only, so restore the visual
                // cursor to the actual document checkpoint as well as
                // rejecting the timeline mutation itself.
                rebuildHistoryView(true);
                return;
            }

            const std::size_t targetDepth = static_cast<std::size_t>(item->data(0, Qt::UserRole).toULongLong());
            jumpToUndoDepth(targetDepth);
        });
    connect(history_, &QTreeWidget::itemDoubleClicked, this,
        [this](QTreeWidgetItem* item, int)
        {
            // Copy the depth before invoking navigation.  The navigation
            // refreshes properties/outliner state and may rebuild the History
            // tree in future implementations; never dereference `item`
            // after that call.
            if (!item)
            {
                return;
            }
            const std::size_t targetDepth = static_cast<std::size_t>(
                item->data(0, Qt::UserRole).toULongLong());
            (void)goToHistoryDepth(targetDepth, true);
        });
    connect(history_, &QTreeWidget::customContextMenuRequested, this,
        [this](const QPoint& position)
        {
            if (!history_)
            {
                return;
            }
            QTreeWidgetItem* item = history_->itemAt(position);
            if (!item)
            {
                return;
            }
            const std::size_t targetDepth = static_cast<std::size_t>(
                item->data(0, Qt::UserRole).toULongLong());
            history_->setCurrentItem(item);

            ui::shortcuts::KeySequenceDisplayMenu menu(history_);
            populateHistoryContextMenu(&menu, targetDepth);
            menu.exec(history_->viewport()->mapToGlobal(position));
        });
    connect(historyDock, &QDockWidget::visibilityChanged, this,
        [this](const bool visible)
        {
            if (visible)
            {
                rebuildHistoryView();
            }
        });

    addDockWidget(Qt::BottomDockWidgetArea, checkDock);
    addDockWidget(Qt::BottomDockWidgetArea, msgDock);
    addDockWidget(Qt::BottomDockWidgetArea, historyDock);
    splitDockWidget(checkDock, msgDock, Qt::Horizontal);
    splitDockWidget(msgDock, historyDock, Qt::Horizontal);
    historyDock->hide(); // Keep history window opt-in via View -> Windows.
    // Default bottom layout: Checks on the left, Messages on the right, with
    // equal width. Once the user moves/resizes them, QMainWindow state owns
    // the exact topology and splitter ratios across sessions.
    resizeDocks({ checkDock, msgDock }, { 1, 1 }, Qt::Horizontal);
    // Make the bottom dock stack a bit taller by default (messages/checks are easier to read).
    resizeDocks({ checkDock, msgDock }, { 280, 280 }, Qt::Vertical);

    rebuildHistoryView();

    rebuildChecks();

    auto navigateCheckIssue = [this](QTreeWidgetItem* item)
    {
        if (!item || !glView_)
        {
            return;
        }

        const auto kind = static_cast<OutlinerKind>(item->data(0, kOutlinerKindRole).toInt());
        if (kind == OutlinerKind::none)
        {
            return;
        }
        const int index = item->data(0, kOutlinerIndexRole).toInt();
        const int sectorIndex = item->data(0, kOutlinerSectorRole).toInt();
        const QVariant surfaceData = item->data(0, kOutlinerSurfaceRole);
        const int surfaceIndex = (kind == OutlinerKind::surface && !surfaceData.isValid()) ? index : surfaceData.toInt();
        (void)navigateToEntity(kind, index, sectorIndex, surfaceIndex, true);
    };

    connect(consistencyChecks_, &QTreeWidget::itemDoubleClicked, this, [navigateCheckIssue](QTreeWidgetItem* item, int)
        {
            navigateCheckIssue(item);
        });
    connect(resourceChecks_, &QTreeWidget::itemDoubleClicked, this, [navigateCheckIssue](QTreeWidgetItem* item, int)
        {
            navigateCheckIssue(item);
        });

    connect(outliner_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int)
        {
            if (!item || !glView_)
            {
                return;
            }

            QSettings settings = settingsStore();
            if (!settings.value("viewport/outliner_double_click_frame", true).toBool())
            {
                return;
            }

            const bool animate = settings.value("viewport/outliner_double_click_frame_animate", true).toBool();

            const Level& level = document_.level();

            const auto kind = static_cast<OutlinerKind>(item->data(0, kOutlinerKindRole).toInt());
            const int index = item->data(0, kOutlinerIndexRole).toInt();

            if (kind == OutlinerKind::cog)
            {
                if (index >= 0 && static_cast<std::size_t>(index) < level.cogs.size())
                {
                    openCogScriptEditor(level.cogs[static_cast<std::size_t>(index)].name);
                }
                return;
            }

            QVector3D minv(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
            QVector3D maxv(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
            bool haveAny = false;

            auto include = [&](const double x, const double y, const double z)
            {
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                {
                    return;
                }

                const QVector3D p(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                if (!haveAny)
                {
                    minv = p;
                    maxv = p;
                    haveAny = true;
                    return;
                }

                minv.setX(std::min(minv.x(), p.x()));
                minv.setY(std::min(minv.y(), p.y()));
                minv.setZ(std::min(minv.z(), p.z()));

                maxv.setX(std::max(maxv.x(), p.x()));
                maxv.setY(std::max(maxv.y(), p.y()));
                maxv.setZ(std::max(maxv.z(), p.z()));
            };

            switch (kind)
            {
                case OutlinerKind::sector:
                {
                    if (index < 0 || static_cast<std::size_t>(index) >= level.sectors.size())
                    {
                        return;
                    }

                    const Sector& sector = level.sectors[static_cast<std::size_t>(index)];
                    for (const Vertex& v : sector.vertices)
                    {
                        include(v.x, v.y, v.z);
                    }
                    break;
                }
                case OutlinerKind::surface:
                {
                    const int sectorIndex = item->data(0, kOutlinerSectorRole).toInt();
                    if (sectorIndex < 0
                        || index < 0
                        || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
                    {
                        return;
                    }

                    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
                    if (static_cast<std::size_t>(index) >= sector.surfaces.size())
                    {
                        return;
                    }

                    const Surface& surface = sector.surfaces[static_cast<std::size_t>(index)];
                    for (const std::int32_t vi : surface.vertices)
                    {
                        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                        {
                            continue;
                        }
                        const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
                        include(v.x, v.y, v.z);
                    }
                    break;
                }
                case OutlinerKind::thing:
                {
                    if (index < 0 || static_cast<std::size_t>(index) >= level.things.size())
                    {
                        return;
                    }

                    const Thing& thing = level.things[static_cast<std::size_t>(index)];
                    if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
                    {
                        return;
                    }

                    const std::string_view tplName = !thing.templateName.empty() ? std::string_view(thing.templateName)
                                                                                 : std::string_view(thing.name);

                    ThingBox box = thing.bbox;
                    for (const ThingTemplate& tpl : level.templates)
                    {
                        if (tpl.name == tplName)
                        {
                            box = tpl.bbox;
                            break;
                        }
                    }
                    if (templates_.size() > 0)
                    {
                        if (const ThingTemplate* tpl = templates_.find(tplName))
                        {
                            box = tpl->bbox;
                        }
                    }

                    const bool zeroBox = IsZero(box.x1) && IsZero(box.y1) && IsZero(box.z1)
                                      && IsZero(box.x2) && IsZero(box.y2) && IsZero(box.z2);

                    TVector localMin{};
                    TVector localMax{};
                    if (!zeroBox)
                    {
                        localMin = TVector{ box.x1, box.y1, box.z1 };
                        localMax = TVector{ box.x2, box.y2, box.z2 };
                    }
                    else
                    {
                        const double h = static_cast<double>(glView_->thingCubeHalfSize());
                        localMin = TVector{ -h, -h, -h };
                        localMax = TVector{ h, h, h };
                    }

                    TVector pyr{};
                    pyr.pitch = thing.pch;
                    pyr.yaw = thing.yaw;
                    pyr.roll = thing.rol;
                    const TMatrix43 xf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, pyr);

                    const TVector mn = localMin;
                    const TVector mx = localMax;
                    const TVector localCorners[8] = {
                        TVector{ mn.x, mn.y, mn.z },
                        TVector{ mx.x, mn.y, mn.z },
                        TVector{ mx.x, mx.y, mn.z },
                        TVector{ mn.x, mx.y, mn.z },
                        TVector{ mn.x, mn.y, mx.z },
                        TVector{ mx.x, mn.y, mx.z },
                        TVector{ mx.x, mx.y, mx.z },
                        TVector{ mn.x, mx.y, mx.z },
                    };

                    for (const TVector& c : localCorners)
                    {
                        const TVector p = xf.TransformPoint(c);
                        include(p.x, p.y, p.z);
                    }

                    break;
                }
                case OutlinerKind::light:
                {
                    if (index < 0 || static_cast<std::size_t>(index) >= level.lights.size())
                    {
                        return;
                    }

                    const SedLight& light = level.lights[static_cast<std::size_t>(index)];
                    if (!std::isfinite(light.position.x) || !std::isfinite(light.position.y) || !std::isfinite(light.position.z))
                    {
                        return;
                    }

                    const float range = static_cast<float>(light.range);
                    const float r = std::clamp(std::isfinite(range) ? range : 0.0f, 0.01f, 1.0e6f);

                    include(light.position.x - r, light.position.y - r, light.position.z - r);
                    include(light.position.x + r, light.position.y + r, light.position.z + r);
                    break;
                }
                default:
                {
                    return;
                }
            }

            if (!haveAny)
            {
                return;
            }

            frameBoundsWithNavigationPrefs(minv, maxv, animate);
        });

    auto syncActiveSurfaceVertexFromCurrentOutlinerItem = [this](QTreeWidgetItem* current) -> void
    {
        if (!glView_)
        {
            return;
        }

        if (!current || !current->isSelected())
        {
            glView_->clearActiveSurfaceVertexHint();
            return;
        }

        const auto kind = static_cast<OutlinerKind>(current->data(0, kOutlinerKindRole).toInt());
        if (kind != OutlinerKind::surfaceVertex)
        {
            glView_->clearActiveSurfaceVertexHint();
            return;
        }

        const int sectorIndex = current->data(0, kOutlinerSectorRole).toInt();
        const int surfaceIndex = current->data(0, kOutlinerSurfaceRole).toInt();
        const int localVertex = current->data(0, kOutlinerIndexRole).toInt();
        glView_->setActiveSurfaceVertexHint(sectorIndex, surfaceIndex, localVertex);
    };

    connect(outliner_, &QTreeWidget::currentItemChanged, this, [this, syncActiveSurfaceVertexFromCurrentOutlinerItem](QTreeWidgetItem* current, QTreeWidgetItem*)
        {
            if (!outliner_ || !glView_)
            {
                return;
            }

            // Keep the active (red) vertex aligned with the currently focused outliner row.
            syncActiveSurfaceVertexFromCurrentOutlinerItem(current);
        });

    connect(outliner_, &QTreeWidget::itemSelectionChanged, this, [this]()
        {
            if (!outliner_ || !glView_)
            {
                return;
            }

            document_.selection().clear();

            const QList<QTreeWidgetItem*> selectedItems = outliner_->selectedItems();
            if (selectedItems.isEmpty())
            {
                glView_->clearActiveSurfaceVertexHint();
                glView_->setSelectionState(-1, -1, -1, -1, document_.selection());
                updateEditActions();
                rebuildProperties();
                return;
            }

            for (QTreeWidgetItem* item : selectedItems)
            {
                if (!item)
                {
                    continue;
                }

                const auto kind = static_cast<OutlinerKind>(item->data(0, kOutlinerKindRole).toInt());
                const int index = item->data(0, kOutlinerIndexRole).toInt();

                if (kind == OutlinerKind::sector)
                {
                    document_.selection().sectors.insert(index);
                }
                else if (kind == OutlinerKind::surface)
                {
                    const int sectorIndex = item->data(0, kOutlinerSectorRole).toInt();
                    document_.selection().surfaces.insert(SurfaceRef{ sectorIndex, index });
                }
                else if (kind == OutlinerKind::surfaceVertex)
                {
                    const int sectorIndex = item->data(0, kOutlinerSectorRole).toInt();
                    const int surfaceIndex = item->data(0, kOutlinerSurfaceRole).toInt();
                    const SurfaceRef surfRef{ sectorIndex, surfaceIndex };
                    document_.selection().surfaceVertices.insert(
                        editor::SurfaceVertexRef{
                            surfRef,
                            static_cast<std::int32_t>(index) });
                    // Selecting a vertex implies selecting its parent surface (needed for surface item editor context).
                    document_.selection().surfaces.insert(surfRef);
                }
                else if (kind == OutlinerKind::thing)
                {
                    document_.selection().things.insert(index);
                }
                else if (kind == OutlinerKind::light)
                {
                    document_.selection().lights.insert(index);
                }
                else if (kind == OutlinerKind::cog)
                {
                    document_.selection().cogs.insert(index);
                }
            }

            QTreeWidgetItem* primaryItem = outliner_->currentItem();
            if (!primaryItem || !primaryItem->isSelected())
            {
                primaryItem = selectedItems.front();
            }

            const auto primaryKind = static_cast<OutlinerKind>(primaryItem->data(0, kOutlinerKindRole).toInt());
            const int primaryIndex = primaryItem->data(0, kOutlinerIndexRole).toInt();

            // Selecting a non-cog outliner item supersedes pending cog-editor navigation.
            if (primaryKind != OutlinerKind::cog)
            {
                cogEntityNavigatePending_ = false;
                cogEntityNavigatePendingKind_ = OutlinerKind::none;
                cogEntityNavigatePendingIndex_ = -1;
                cogEntityNavigatePendingSector_ = -1;
                cogEntityNavigatePendingSurface_ = -1;
            }

            // Mixed-type outliner multi-selection is confusing for both the viewport and the item editor.
            // Constrain selection to the primary kind (and required context like parent surfaces for vertices).
            {
                editor::Selection& sel = document_.selection();
                switch (primaryKind)
                {
                    case OutlinerKind::sector:
                    {
                        sel.surfaces.clear();
                        sel.surfaceVertices.clear();
                        sel.things.clear();
                        sel.lights.clear();
                        sel.cogs.clear();
                        break;
                    }
                    case OutlinerKind::surface:
                    {
                        sel.sectors.clear();
                        sel.surfaceVertices.clear();
                        sel.things.clear();
                        sel.lights.clear();
                        sel.cogs.clear();
                        break;
                    }
                    case OutlinerKind::surfaceVertex:
                    {
                        sel.sectors.clear();
                        sel.things.clear();
                        sel.lights.clear();
                        sel.cogs.clear();
                        break;
                    }
                    case OutlinerKind::thing:
                    {
                        sel.sectors.clear();
                        sel.surfaces.clear();
                        sel.surfaceVertices.clear();
                        sel.lights.clear();
                        sel.cogs.clear();
                        break;
                    }
                    case OutlinerKind::light:
                    {
                        sel.sectors.clear();
                        sel.surfaces.clear();
                        sel.surfaceVertices.clear();
                        sel.things.clear();
                        sel.cogs.clear();
                        break;
                    }
                    case OutlinerKind::cog:
                    {
                        sel.sectors.clear();
                        sel.surfaces.clear();
                        sel.surfaceVertices.clear();
                        sel.things.clear();
                        sel.lights.clear();
                        break;
                    }
                    default:
                        break;
                }
            }

            if (primaryKind == OutlinerKind::sector)
            {
                glView_->clearActiveSurfaceVertexHint();
                glView_->setSelectionState(primaryIndex, -1, -1, -1, document_.selection());
            }
            else if (primaryKind == OutlinerKind::surface)
            {
                glView_->clearActiveSurfaceVertexHint();
                const int sectorIndex = primaryItem->data(0, kOutlinerSectorRole).toInt();
                glView_->setSelectionState(sectorIndex, primaryIndex, -1, -1, document_.selection());

                haveLastSurfaceClick_ = false;
                lastSurfaceClickSurface_ = {};
                lastSurfaceClickPos_ = {};

                const Level& level = document_.level();
                if (sectorIndex >= 0 && primaryIndex >= 0
                    && static_cast<std::size_t>(sectorIndex) < level.sectors.size()
                    && static_cast<std::size_t>(primaryIndex) < level.sectors[static_cast<std::size_t>(sectorIndex)].surfaces.size())
                {
                    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
                    const Surface& surface = sector.surfaces[static_cast<std::size_t>(primaryIndex)];

                    TVector center{};
                    std::size_t centerCount = 0;
                    for (const std::int32_t vidx : surface.vertices)
                    {
                        if (vidx < 0 || static_cast<std::size_t>(vidx) >= sector.vertices.size())
                        {
                            continue;
                        }

                        const Vertex& v = sector.vertices[static_cast<std::size_t>(vidx)];
                        center.x += v.x;
                        center.y += v.y;
                        center.z += v.z;
                        ++centerCount;
                    }

                    if (centerCount > 0)
                    {
                        const double invCount = 1.0 / static_cast<double>(centerCount);
                        center.x *= invCount;
                        center.y *= invCount;
                        center.z *= invCount;
                    }

                    haveLastSurfaceClick_ = true;
                    lastSurfaceClickSurface_ = SurfaceRef{ sectorIndex, primaryIndex };
                    lastSurfaceClickPos_ = center;
                }
            }
            else if (primaryKind == OutlinerKind::surfaceVertex)
            {
                const int sectorIndex = primaryItem->data(0, kOutlinerSectorRole).toInt();
                const int surfaceIndex = primaryItem->data(0, kOutlinerSurfaceRole).toInt();
                glView_->setActiveSurfaceVertexHint(sectorIndex, surfaceIndex, primaryIndex);
                glView_->setSelectionState(sectorIndex, surfaceIndex, -1, -1, document_.selection());

                // Drive the surface sub-editors (edge/vertex) which use the last clicked surface position as a hint.
                haveLastSurfaceClick_ = false;
                lastSurfaceClickSurface_ = {};
                lastSurfaceClickPos_ = {};

                const Level& level = document_.level();
                if (sectorIndex >= 0 && surfaceIndex >= 0
                    && static_cast<std::size_t>(sectorIndex) < level.sectors.size()
                    && static_cast<std::size_t>(surfaceIndex) < level.sectors[static_cast<std::size_t>(sectorIndex)].surfaces.size())
                {
                    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
                    const Surface& surface = sector.surfaces[static_cast<std::size_t>(surfaceIndex)];
                    if (primaryIndex >= 0 && static_cast<std::size_t>(primaryIndex) < surface.vertices.size())
                    {
                        const std::int32_t vertexIndex = surface.vertices[static_cast<std::size_t>(primaryIndex)];
                        if (vertexIndex >= 0 && static_cast<std::size_t>(vertexIndex) < sector.vertices.size())
                        {
                            const Vertex& v = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                            haveLastSurfaceClick_ = true;
                            lastSurfaceClickSurface_ = SurfaceRef{ sectorIndex, surfaceIndex };
                            lastSurfaceClickPos_ = TVector{ v.x, v.y, v.z };
                        }
                    }
                }

                // Selecting a vertex in the outliner should behave like vertex-mode picking in the viewport and
                // should drive the item editor into vertex sub-mode.
                if (documentEditingAllowed())
                {
                    setSurfaceSelectionMode(SurfaceSelectionMode::vertex);
                }
            }
            else if (primaryKind == OutlinerKind::thing)
            {
                glView_->clearActiveSurfaceVertexHint();
                glView_->setSelectionState(-1, -1, primaryIndex, -1, document_.selection());
            }
            else if (primaryKind == OutlinerKind::light)
            {
                glView_->clearActiveSurfaceVertexHint();
                glView_->setSelectionState(-1, -1, -1, primaryIndex, document_.selection());
            }
            else
            {
                glView_->clearActiveSurfaceVertexHint();
                glView_->setSelectionState(-1, -1, -1, -1, document_.selection());
            }
            updateEditActions();
            rebuildProperties();
        });

    connect(outlinerTabs_, &QTabBar::currentChanged, this, [this](const int tab)
        {
            if (tab != 2 && cogLinkSelectionRestorePending_)
            {
                const bool selectionOnlyHasCogs =
                    document_.selection().sectors.empty()
                    && document_.selection().surfaces.empty()
                    && document_.selection().surfaceVertices.empty()
                    && document_.selection().things.empty()
                    && document_.selection().frames.empty()
                    && document_.selection().lights.empty()
                    && !document_.selection().cogs.empty();

                if (selectionOnlyHasCogs && !cogLinkSelectionBackup_.empty())
                {
                    document_.selection() = cogLinkSelectionBackup_;
                    cogLinkSelectionBackup_.clear();
                    cogLinkSelectionRestorePending_ = false;
                }
                else if (!selectionOnlyHasCogs)
                {
                    cogLinkSelectionBackup_.clear();
                    cogLinkSelectionRestorePending_ = false;
                }
            }

            if (!outliner_ || !glView_)
            {
                rebuildOutliner();
                updateEditActions();
                rebuildProperties();
                return;
            }

            editor::Selection restored{};

            const bool tabMatchesPending =
                cogEntityNavigatePending_
                && ((tab == 0 && (cogEntityNavigatePendingKind_ == OutlinerKind::sector || cogEntityNavigatePendingKind_ == OutlinerKind::surface))
                    || (tab == 1 && cogEntityNavigatePendingKind_ == OutlinerKind::thing)
                    || (tab == 3 && cogEntityNavigatePendingKind_ == OutlinerKind::light));

            const Level& level = document_.level();
            if (tabMatchesPending)
            {
                switch (cogEntityNavigatePendingKind_)
                {
                    case OutlinerKind::sector:
                    {
                        const std::int32_t idx = cogEntityNavigatePendingIndex_;
                        if (idx < 0 || static_cast<std::size_t>(idx) >= level.sectors.size())
                        {
                            break;
                        }
                        restored.sectors.insert(idx);
                        break;
                    }
                    case OutlinerKind::surface:
                    {
                        const std::int32_t s = cogEntityNavigatePendingSector_;
                        const std::int32_t sf = cogEntityNavigatePendingSurface_;
                        if (s < 0 || sf < 0
                            || static_cast<std::size_t>(s) >= level.sectors.size()
                            || static_cast<std::size_t>(sf) >= level.sectors[static_cast<std::size_t>(s)].surfaces.size())
                        {
                            break;
                        }
                        restored.surfaces.insert(SurfaceRef{ s, sf });
                        break;
                    }
                    case OutlinerKind::thing:
                    {
                        const std::int32_t idx = cogEntityNavigatePendingIndex_;
                        if (idx < 0 || static_cast<std::size_t>(idx) >= level.things.size())
                        {
                            break;
                        }
                        restored.things.insert(idx);
                        break;
                    }
                    case OutlinerKind::light:
                    {
                        const std::int32_t idx = cogEntityNavigatePendingIndex_;
                        if (idx < 0 || static_cast<std::size_t>(idx) >= level.lights.size())
                        {
                            break;
                        }
                        restored.lights.insert(idx);
                        break;
                    }
                    default:
                        break;
                }

                cogEntityNavigatePending_ = false;
                cogEntityNavigatePendingKind_ = OutlinerKind::none;
                cogEntityNavigatePendingIndex_ = -1;
                cogEntityNavigatePendingSector_ = -1;
                cogEntityNavigatePendingSurface_ = -1;
            }
            else
            {
                const editor::Selection& sel = document_.selection();
                if (tab == 0)
                {
                    restored.sectors = sel.sectors;
                    restored.surfaces = sel.surfaces;
                    restored.surfaceVertices = sel.surfaceVertices;
                }
                else if (tab == 1)
                {
                    restored.things = sel.things;
                }
                else if (tab == 2)
                {
                    restored.cogs = sel.cogs;
                }
                else
                {
                    restored.lights = sel.lights;
                }
            }

            document_.selection() = restored;

            if (!restored.surfaceVertices.empty())
            {
                editor::SurfaceVertexRef best = *restored.surfaceVertices.begin();
                for (const editor::SurfaceVertexRef& ref : restored.surfaceVertices)
                {
                    if (ref.surface.sector < best.surface.sector
                        || (ref.surface.sector == best.surface.sector && ref.surface.surface < best.surface.surface)
                        || (ref.surface == best.surface && ref.localVertex < best.localVertex))
                    {
                        best = ref;
                    }
                }
                glView_->setActiveSurfaceVertexHint(best.surface.sector, best.surface.surface, best.localVertex);
            }
            else
            {
                glView_->clearActiveSurfaceVertexHint();
            }

            auto minIndex = [](const auto& set) -> std::int32_t
            {
                if (set.empty())
                {
                    return -1;
                }
                return *std::min_element(set.begin(), set.end());
            };

            if (!restored.surfaces.empty())
            {
                SurfaceRef best = *restored.surfaces.begin();
                for (const SurfaceRef& ref : restored.surfaces)
                {
                    if (ref.sector < best.sector || (ref.sector == best.sector && ref.surface < best.surface))
                    {
                        best = ref;
                    }
                }
                glView_->setSelectionState(best.sector, best.surface, -1, -1, restored);
            }
            else if (!restored.sectors.empty())
            {
                glView_->setSelectionState(minIndex(restored.sectors), -1, -1, -1, restored);
            }
            else if (!restored.things.empty())
            {
                glView_->setSelectionState(-1, -1, minIndex(restored.things), -1, restored);
            }
            else if (!restored.lights.empty())
            {
                glView_->setSelectionState(-1, -1, -1, minIndex(restored.lights), restored);
            }
            else
            {
                glView_->setSelectionState(-1, -1, -1, -1, restored);
            }
            rebuildOutliner();
            updateEditActions();
            rebuildProperties();
        });
}

bool MainWindow::navigateToEntity(
    const OutlinerKind kind,
    const std::int32_t index,
    const std::int32_t sectorIndex,
    const std::int32_t surfaceIndex,
    const bool animate,
    const bool preserveCogContext)
{
    if (!glView_)
    {
        return false;
    }

    // Any explicit navigation supersedes previously pending cog-context navigation.
    cogEntityNavigatePending_ = false;
    cogEntityNavigatePendingKind_ = OutlinerKind::none;
    cogEntityNavigatePendingIndex_ = -1;
    cogEntityNavigatePendingSector_ = -1;
    cogEntityNavigatePendingSurface_ = -1;

    // Ensure the viewport is in a compatible edit mode before selecting/framing.
    if (!preserveCogContext && documentEditingAllowed())
    {
        switch (kind)
        {
            case OutlinerKind::sector:
                if (editMode_ != EditMode::sector)
                {
                    setEditMode(EditMode::sector);
                }
                break;
            case OutlinerKind::surface:
                if (editMode_ != EditMode::surface)
                {
                    setEditMode(EditMode::surface);
                }
                break;
            case OutlinerKind::thing:
                if (editMode_ != EditMode::thing)
                {
                    setEditMode(EditMode::thing);
                }
                break;
            case OutlinerKind::light:
                if (editMode_ != EditMode::light)
                {
                    setEditMode(EditMode::light);
                }
                break;
            default:
                break;
        }
    }

    const Level& level = document_.level();

    QVector3D minv(
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max());
    QVector3D maxv(
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max());
    bool haveAny = false;
    OutlinerKind resolvedKind = OutlinerKind::none;
    std::int32_t resolvedIndex = -1;
    std::int32_t resolvedSector = -1;
    std::int32_t resolvedSurfaceIndex = -1;

    auto include = [&](const double x, const double y, const double z)
    {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
        {
            return;
        }

        const QVector3D p(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        if (!haveAny)
        {
            minv = p;
            maxv = p;
            haveAny = true;
            return;
        }

        minv.setX(std::min(minv.x(), p.x()));
        minv.setY(std::min(minv.y(), p.y()));
        minv.setZ(std::min(minv.z(), p.z()));

        maxv.setX(std::max(maxv.x(), p.x()));
        maxv.setY(std::max(maxv.y(), p.y()));
        maxv.setZ(std::max(maxv.z(), p.z()));
    };

    if (!preserveCogContext)
    {
        document_.selection().clear();
    }

    switch (kind)
    {
        case OutlinerKind::sector:
        {
            if (index < 0 || static_cast<std::size_t>(index) >= level.sectors.size())
            {
                return false;
            }

            if (!preserveCogContext)
            {
                document_.selection().sectors.insert(index);
            }

            const Sector& sector = level.sectors[static_cast<std::size_t>(index)];
            for (const Vertex& v : sector.vertices)
            {
                include(v.x, v.y, v.z);
            }

            if (outlinerTabs_ && !preserveCogContext)
            {
                outlinerTabs_->setCurrentIndex(0);
            }
            if (preserveCogContext)
            {
                glView_->setSelection(index, -1, -1, -1);
            }
            resolvedKind = OutlinerKind::sector;
            resolvedIndex = index;
            break;
        }
        case OutlinerKind::surface:
        {
            const std::int32_t selectedSurface = (surfaceIndex >= 0) ? surfaceIndex : index;
            if (sectorIndex < 0 || selectedSurface < 0
                || static_cast<std::size_t>(sectorIndex) >= level.sectors.size()
                || static_cast<std::size_t>(selectedSurface) >= level.sectors[static_cast<std::size_t>(sectorIndex)].surfaces.size())
            {
                return false;
            }

            const SurfaceRef ref{ sectorIndex, selectedSurface };
            if (!preserveCogContext)
            {
                document_.selection().surfaces.insert(ref);
            }

            const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
            const Surface& surface = sector.surfaces[static_cast<std::size_t>(selectedSurface)];
            for (const std::int32_t vi : surface.vertices)
            {
                if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                {
                    continue;
                }
                const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
                include(v.x, v.y, v.z);
            }

            if (outlinerTabs_ && !preserveCogContext)
            {
                outlinerTabs_->setCurrentIndex(0);
            }
            if (preserveCogContext)
            {
                glView_->setSelection(sectorIndex, selectedSurface, -1, -1);
            }
            resolvedKind = OutlinerKind::surface;
            resolvedIndex = selectedSurface;
            resolvedSector = sectorIndex;
            resolvedSurfaceIndex = selectedSurface;
            break;
        }
        case OutlinerKind::thing:
        {
            if (index < 0 || static_cast<std::size_t>(index) >= level.things.size())
            {
                return false;
            }

            if (!preserveCogContext)
            {
                document_.selection().things.insert(index);
            }
            const Thing& thing = level.things[static_cast<std::size_t>(index)];
            if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
            {
                return false;
            }

            const std::string_view tplName = !thing.templateName.empty() ? std::string_view(thing.templateName)
                                                                          : std::string_view(thing.name);

            ThingBox box = thing.bbox;
            for (const ThingTemplate& tpl : level.templates)
            {
                if (tpl.name == tplName)
                {
                    box = tpl.bbox;
                    break;
                }
            }
            if (templates_.size() > 0)
            {
                if (const ThingTemplate* tpl = templates_.find(tplName))
                {
                    box = tpl->bbox;
                }
            }

            const bool zeroBox = IsZero(box.x1) && IsZero(box.y1) && IsZero(box.z1)
                              && IsZero(box.x2) && IsZero(box.y2) && IsZero(box.z2);

            TVector localMin{};
            TVector localMax{};
            if (!zeroBox)
            {
                localMin = TVector{ box.x1, box.y1, box.z1 };
                localMax = TVector{ box.x2, box.y2, box.z2 };
            }
            else
            {
                const double h = static_cast<double>(glView_->thingCubeHalfSize());
                localMin = TVector{ -h, -h, -h };
                localMax = TVector{ h, h, h };
            }

            TVector pyr{};
            pyr.pitch = thing.pch;
            pyr.yaw = thing.yaw;
            pyr.roll = thing.rol;
            const TMatrix43 xf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, pyr);

            const TVector mn = localMin;
            const TVector mx = localMax;
            const TVector localCorners[8] = {
                TVector{ mn.x, mn.y, mn.z },
                TVector{ mx.x, mn.y, mn.z },
                TVector{ mx.x, mx.y, mn.z },
                TVector{ mn.x, mx.y, mn.z },
                TVector{ mn.x, mn.y, mx.z },
                TVector{ mx.x, mn.y, mx.z },
                TVector{ mx.x, mx.y, mx.z },
                TVector{ mn.x, mx.y, mx.z },
            };

            for (const TVector& c : localCorners)
            {
                const TVector p = xf.TransformPoint(c);
                include(p.x, p.y, p.z);
            }

            if (outlinerTabs_ && !preserveCogContext)
            {
                outlinerTabs_->setCurrentIndex(1);
            }
            if (preserveCogContext)
            {
                glView_->setSelection(-1, -1, index, -1);
            }
            resolvedKind = OutlinerKind::thing;
            resolvedIndex = index;
            break;
        }
        case OutlinerKind::light:
        {
            if (index < 0 || static_cast<std::size_t>(index) >= level.lights.size())
            {
                return false;
            }

            if (!preserveCogContext)
            {
                document_.selection().lights.insert(index);
            }
            const SedLight& light = level.lights[static_cast<std::size_t>(index)];
            const double r = std::max(0.01, light.range);
            include(light.position.x - r, light.position.y - r, light.position.z - r);
            include(light.position.x + r, light.position.y + r, light.position.z + r);

            if (outlinerTabs_ && !preserveCogContext)
            {
                outlinerTabs_->setCurrentIndex(3);
            }
            if (preserveCogContext)
            {
                glView_->setSelection(-1, -1, -1, index);
            }
            resolvedKind = OutlinerKind::light;
            resolvedIndex = index;
            break;
        }
        default:
            return false;
    }

    if (!preserveCogContext)
    {
        std::int32_t selSector = -1;
        std::int32_t selSurface = -1;
        std::int32_t selThing = -1;
        std::int32_t selLight = -1;

        switch (resolvedKind)
        {
            case OutlinerKind::sector:
                selSector = resolvedIndex;
                break;
            case OutlinerKind::surface:
                selSector = resolvedSector;
                selSurface = resolvedSurfaceIndex;
                break;
            case OutlinerKind::thing:
                selThing = resolvedIndex;
                break;
            case OutlinerKind::light:
                selLight = resolvedIndex;
                break;
            default:
                break;
        }

        glView_->setSelectionState(selSector, selSurface, selThing, selLight, document_.selection());
        rebuildOutliner();
        updateEditActions();
        rebuildProperties();
    }

    if (haveAny)
    {
        frameBoundsWithNavigationPrefs(minv, maxv, animate);
    }

    if (preserveCogContext && resolvedKind != OutlinerKind::none)
    {
        cogEntityNavigatePending_ = true;
        cogEntityNavigatePendingKind_ = resolvedKind;
        cogEntityNavigatePendingIndex_ = resolvedIndex;
        cogEntityNavigatePendingSector_ = resolvedSector;
        cogEntityNavigatePendingSurface_ = resolvedSurfaceIndex;
    }

    return true;
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
bool MainWindow::outlinerDoubleClickEntityForTest(
    const OutlinerKind kind,
    const std::int32_t index,
    const std::int32_t sectorIndex,
    const std::int32_t surfaceIndex)
{
    if (!outliner_)
    {
        return false;
    }

    rebuildOutliner();
    QTreeWidgetItem* matched = nullptr;
    const auto visit = [&](auto&& self, QTreeWidgetItem* const item) -> void
    {
        if (!item || matched)
        {
            return;
        }
        const auto itemKind = static_cast<OutlinerKind>(
            item->data(0, kOutlinerKindRole).toInt());
        const std::int32_t itemIndex = item->data(0, kOutlinerIndexRole).toInt();
        const std::int32_t itemSector = item->data(0, kOutlinerSectorRole).toInt();
        const QVariant itemSurfaceData = item->data(0, kOutlinerSurfaceRole);
        const std::int32_t itemSurface = itemSurfaceData.isValid()
            ? itemSurfaceData.toInt()
            : itemIndex;
        const bool surfaceMatches = kind != OutlinerKind::surface
            || (itemSector == sectorIndex
                && (itemSurface >= 0 ? itemSurface : itemIndex)
                    == (surfaceIndex >= 0 ? surfaceIndex : index));
        if (itemKind == kind && itemIndex == index && surfaceMatches)
        {
            matched = item;
            return;
        }
        for (int child = 0; child < item->childCount(); ++child)
        {
            self(self, item->child(child));
            if (matched)
            {
                return;
            }
        }
    };

    for (int row = 0; row < outliner_->topLevelItemCount() && !matched; ++row)
    {
        visit(visit, outliner_->topLevelItem(row));
    }
    if (!matched)
    {
        return false;
    }

    outliner_->setCurrentItem(matched);
    Q_EMIT outliner_->itemDoubleClicked(matched, 0);
    return true;
}
#endif

} // namespace sed
