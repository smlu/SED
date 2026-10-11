#include "main_window/common.hpp"

#include "ui/components/viewport_icons.hpp"
#include "ui/layers/layer_color_utils.hpp"

namespace sed
{

namespace
{

constexpr std::size_t kOutlinerBulkSurfaceSelectionThreshold = 96;

std::uint64_t encodeOutlinerSurfaceKey(const std::int32_t sectorIndex, const std::int32_t surfaceIndex) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(sectorIndex)) << 32u)
        ^ static_cast<std::uint64_t>(static_cast<std::uint32_t>(surfaceIndex));
}

bool outlinerTreatSurfaceSelectionAsBulk(const editor::Selection& selection) noexcept
{
    return (selection.surfaces.size() + selection.surfaceVertices.size()) > kOutlinerBulkSurfaceSelectionThreshold;
}

class ScopedWidgetUpdatesDisabled
{
public:
    explicit ScopedWidgetUpdatesDisabled(QWidget* widget) : widget_(widget), wasEnabled_(widget ? widget->updatesEnabled() : false)
    {
        if (widget_)
        {
            widget_->setUpdatesEnabled(false);
        }
    }

    ~ScopedWidgetUpdatesDisabled()
    {
        if (widget_)
        {
            widget_->setUpdatesEnabled(wasEnabled_);
        }
    }

private:
    QWidget* widget_{};
    bool wasEnabled_{};
};

void applyOutlinerLayerTint(
    QTreeWidgetItem* item,
    const bool colorByLayer,
    const std::vector<QColor>& layerColors,
    const std::int32_t layerIndex)
{
    if (!colorByLayer || !item || layerIndex < 0)
    {
        return;
    }

    const std::size_t idx = static_cast<std::size_t>(layerIndex);
    if (idx >= layerColors.size())
    {
        return;
    }

    QColor tint = layerColors[idx];
    if (!tint.isValid())
    {
        return;
    }

    tint = tint.lighter(145);
    tint.setAlpha(255);
    item->setForeground(0, QBrush(tint));
}

bool outlinerIncludesLayer(
    const bool showHiddenLayerEntities,
    const std::vector<bool>& layerVisible,
    const std::int32_t layer) noexcept
{
    return showHiddenLayerEntities
        || ui::layers::isLayerVisible(layerVisible, layer);
}

void applyOutlinerLayerStateBadge(
    QTreeWidgetItem* item,
    const std::vector<bool>& layerVisible,
    const std::vector<bool>& layerLocked,
    const std::int32_t layer)
{
    if (!item)
    {
        return;
    }

    const bool visible = ui::layers::isLayerVisible(layerVisible, layer);
    const bool locked = ui::layers::isLayerLocked(layerLocked, layer);
    static const QIcon lockedIcon =
        ui::makeViewportIcon(ui::ViewportIcon::layerLocked);
    static const QIcon hiddenIcon = ui::makeCrossedOutViewportIcon(
        ui::makeViewportIcon(ui::ViewportIcon::visibilityEye));
    if (locked)
    {
        item->setIcon(0, lockedIcon);
        item->setToolTip(
            0,
            visible
                ? QStringLiteral("Layer is locked; this entity is read-only.")
                : QStringLiteral("Layer is hidden and locked; this entity is read-only."));
    }
    else if (!visible)
    {
        item->setIcon(0, hiddenIcon);
        item->setToolTip(
            0,
            QStringLiteral("Layer is hidden; this entity is shown read-only."));
    }
    else
    {
        // Existing Outliner rows can survive an entity-layer edit. Remove a
        // badge left by the old layer when the destination is editable.
        item->setIcon(0, QIcon{});
        item->setToolTip(0, QString{});
    }
}

} // namespace

void MainWindow::refreshOutlinerTabCaptions()
{
    if (!outlinerTabs_)
    {
        return;
    }

    const Level& level = document_.level();

    auto setTabCaption = [this](const int tabIndex, const std::size_t count, const char* singular, const char* plural)
    {
        if (tabIndex < 0 || tabIndex >= outlinerTabs_->count())
        {
            return;
        }

        const QString noun = (count == 1) ? QString::fromLatin1(singular) : QString::fromLatin1(plural);
        outlinerTabs_->setTabText(tabIndex, QString("%1 %2").arg(count).arg(noun));
    };

    setTabCaption(0, level.sectors.size(), "Sector", "Sectors");
    setTabCaption(1, level.things.size(), "Thing", "Things");
    setTabCaption(2, level.cogs.size(), "COG", "COGs");
    setTabCaption(3, level.lights.size(), "Light", "Lights");
}

void MainWindow::populateOutlinerSurfaceVertexChildren(QTreeWidgetItem* surfaceItem)
{
    if (!surfaceItem
        || surfaceItem->data(0, kOutlinerKindRole).toInt() != static_cast<int>(OutlinerKind::surface)
        || surfaceItem->data(0, kOutlinerVertexChildrenPopulatedRole).toBool())
    {
        return;
    }

    const Level& level = document_.level();
    const std::int32_t sectorIndex = surfaceItem->data(0, kOutlinerSectorRole).toInt();
    const std::int32_t surfaceIndex = surfaceItem->data(0, kOutlinerIndexRole).toInt();
    if (sectorIndex < 0 || surfaceIndex < 0
        || static_cast<std::size_t>(sectorIndex) >= level.sectors.size()
        || static_cast<std::size_t>(surfaceIndex) >= level.sectors[static_cast<std::size_t>(sectorIndex)].surfaces.size())
    {
        surfaceItem->setData(0, kOutlinerVertexChildrenPopulatedRole, true);
        return;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    const Surface& surface = sector.surfaces[static_cast<std::size_t>(surfaceIndex)];

    auto* vertsGroup = new QTreeWidgetItem(surfaceItem);
    vertsGroup->setText(0, "Vertices");
    vertsGroup->setData(0, kOutlinerKindRole, static_cast<int>(OutlinerKind::none));
    vertsGroup->setData(0, kOutlinerIndexRole, -1);
    vertsGroup->setFlags(Qt::ItemIsEnabled);
    applyOutlinerLayerTint(vertsGroup, outlinerColorByLayer_, layerColors_, sector.layer);

    const SurfaceRef sref{ sectorIndex, surfaceIndex };
    for (std::size_t li = 0; li < surface.vertices.size(); ++li)
    {
        const std::int32_t vi = surface.vertices[li];

        auto* vItem = new QTreeWidgetItem(vertsGroup);
        vItem->setText(0, QString("v%1: #%2").arg(static_cast<int>(li)).arg(vi));
        vItem->setData(0, kOutlinerKindRole, static_cast<int>(OutlinerKind::surfaceVertex));
        vItem->setData(0, kOutlinerIndexRole, static_cast<int>(li));
        vItem->setData(0, kOutlinerSectorRole, sectorIndex);
        vItem->setData(0, kOutlinerSurfaceRole, surfaceIndex);
        applyOutlinerLayerTint(vItem, outlinerColorByLayer_, layerColors_, sector.layer);

        const editor::SurfaceVertexRef vref{ sref, static_cast<std::int32_t>(li) };
        if (document_.selection().surfaceVertices.contains(vref))
        {
            vItem->setSelected(true);
        }
    }

    surfaceItem->setData(0, kOutlinerVertexChildrenPopulatedRole, true);
}

void MainWindow::populateOutlinerSectorChildren(QTreeWidgetItem* sectorItem)
{
    if (!sectorItem
        || sectorItem->data(0, kOutlinerKindRole).toInt() != static_cast<int>(OutlinerKind::sector)
        || sectorItem->data(0, kOutlinerChildrenPopulatedRole).toBool())
    {
        return;
    }

    const Level& level = document_.level();
    const std::int32_t sectorIndex = sectorItem->data(0, kOutlinerIndexRole).toInt();
    if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
    {
        sectorItem->setData(0, kOutlinerChildrenPopulatedRole, true);
        return;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
    const editor::Selection& selection = document_.selection();
    std::unordered_set<std::uint64_t> expandedSurfaceItems{};
    if (outliner_)
    {
        const QVariantList list = outliner_->property("sed.expandedSurfaceItems").toList();
        expandedSurfaceItems.reserve(static_cast<std::size_t>(list.size()));
        for (const QVariant& kv : list)
        {
            expandedSurfaceItems.insert(static_cast<std::uint64_t>(kv.toULongLong()));
        }
    }

    for (std::size_t si = 0; si < sector.surfaces.size(); ++si)
    {
        const Surface& s = sector.surfaces[si];
        const QString mat = s.material.empty() ? QString("<none>") : QString::fromStdString(sed::text::trim(s.material));
        auto* surfItem = new QTreeWidgetItem(sectorItem);
        surfItem->setText(0, QString("Surface %1: %2").arg(si).arg(mat));
        surfItem->setData(0, kOutlinerKindRole, static_cast<int>(OutlinerKind::surface));
        surfItem->setData(0, kOutlinerIndexRole, static_cast<int>(si));
        surfItem->setData(0, kOutlinerSectorRole, sectorIndex);
        surfItem->setData(0, kOutlinerVertexChildrenPopulatedRole, false);
        applyOutlinerLayerTint(surfItem, outlinerColorByLayer_, layerColors_, sector.layer);
        applyOutlinerLayerStateBadge(
            surfItem, layerVisible_, layerLocked_, sector.layer);

        if (!s.vertices.empty())
        {
            surfItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }

        const SurfaceRef sref{ sectorIndex, static_cast<std::int32_t>(si) };
        const bool surfaceSelected = selection.surfaces.contains(sref);
        const bool vertexSelected = std::any_of(
            selection.surfaceVertices.begin(),
            selection.surfaceVertices.end(),
            [&](const editor::SurfaceVertexRef& vref)
            {
                return vref.surface == sref;
            });

        if (surfaceSelected || vertexSelected)
        {
            surfItem->setSelected(true);
        }

        const bool expandSurface = vertexSelected || expandedSurfaceItems.contains(encodeOutlinerSurfaceKey(sref.sector, sref.surface));
        if (expandSurface && !s.vertices.empty())
        {
            populateOutlinerSurfaceVertexChildren(surfItem);
            surfItem->setExpanded(true);
            if (surfItem->childCount() > 0)
            {
                surfItem->child(0)->setExpanded(true);
            }
        }
    }

    sectorItem->setData(0, kOutlinerChildrenPopulatedRole, true);
}

void MainWindow::syncOutlinerSelectionOnly()
{
    if (!outliner_)
    {
        return;
    }

    const editor::Selection& selection = document_.selection();
    const int tab = outlinerTabs_ ? outlinerTabs_->currentIndex() : 0;
    const QSignalBlocker blocker(outliner_);
    const ScopedWidgetUpdatesDisabled updatesGuard(outliner_);

    outliner_->clearSelection();

    auto minIndex = [](const auto& set) -> std::int32_t
    {
        if (set.empty())
        {
            return -1;
        }
        return *std::min_element(set.begin(), set.end());
    };

    QTreeWidgetItem* current = nullptr;

    if (tab == 0)
    {
        const bool bulkSurfaceSelection = outlinerTreatSurfaceSelectionAsBulk(selection);

        auto minSurfaceRef = [](const auto& set) -> SurfaceRef
        {
            SurfaceRef best{ -1, -1 };
            for (const SurfaceRef& ref : set)
            {
                if (best.sector < 0 || ref.sector < best.sector || (ref.sector == best.sector && ref.surface < best.surface))
                {
                    best = ref;
                }
            }
            return best;
        };

        auto minSurfaceVertexRef = [](const auto& set) -> editor::SurfaceVertexRef
        {
            editor::SurfaceVertexRef best{};
            best.surface = SurfaceRef{ -1, -1 };
            best.localVertex = -1;

            for (const editor::SurfaceVertexRef& ref : set)
            {
                if (best.surface.sector < 0
                    || ref.surface.sector < best.surface.sector
                    || (ref.surface.sector == best.surface.sector && ref.surface.surface < best.surface.surface)
                    || (ref.surface == best.surface && ref.localVertex < best.localVertex))
                {
                    best = ref;
                }
            }
            return best;
        };

        std::unordered_set<std::int32_t> sectorsToExpand{};
        sectorsToExpand.reserve(selection.surfaces.size() + selection.surfaceVertices.size());
        for (const SurfaceRef& ref : selection.surfaces)
        {
            sectorsToExpand.insert(ref.sector);
        }
        for (const editor::SurfaceVertexRef& ref : selection.surfaceVertices)
        {
            sectorsToExpand.insert(ref.surface.sector);
        }

        std::unordered_set<std::uint64_t> selectedSurfaceKeys{};
        selectedSurfaceKeys.reserve(selection.surfaces.size());
        for (const SurfaceRef& ref : selection.surfaces)
        {
            selectedSurfaceKeys.insert(encodeOutlinerSurfaceKey(ref.sector, ref.surface));
        }

        std::unordered_set<std::uint64_t> vertexSurfaceKeys{};
        vertexSurfaceKeys.reserve(selection.surfaceVertices.size());
        for (const editor::SurfaceVertexRef& ref : selection.surfaceVertices)
        {
            vertexSurfaceKeys.insert(encodeOutlinerSurfaceKey(ref.surface.sector, ref.surface.surface));
        }

        const SurfaceRef primarySurface = activeSelectedSurfaceRef().value_or(
            minSurfaceRef(selection.surfaces));
        const std::int32_t primarySector = (primarySurface.sector >= 0) ? primarySurface.sector : minIndex(selection.sectors);
        editor::SurfaceVertexRef primaryVertex = minSurfaceVertexRef(selection.surfaceVertices);
        if (glView_)
        {
            if (const auto activeVertex = glView_->activeSurfaceVertex();
                activeVertex
                && selection.surfaceVertices.contains(*activeVertex))
            {
                primaryVertex = *activeVertex;
            }
        }
        const bool needSurfaceWalk = !selection.surfaces.empty() || !selection.surfaceVertices.empty();
        const bool needVertexWalk = !selection.surfaceVertices.empty();

        for (int i = 0; i < outliner_->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* item = outliner_->topLevelItem(i);
            if (!item || static_cast<OutlinerKind>(item->data(0, kOutlinerKindRole).toInt()) != OutlinerKind::sector)
            {
                continue;
            }

            const std::int32_t sectorIndex = static_cast<std::int32_t>(item->data(0, kOutlinerIndexRole).toInt());
            const bool sectorSelected = selection.sectors.contains(sectorIndex);
            item->setSelected(sectorSelected);

            if (bulkSurfaceSelection && current == nullptr && sectorIndex == primarySector)
            {
                current = item;
            }

            if (sectorIndex == primarySector && primarySurface.sector < 0 && primaryVertex.surface.sector < 0)
            {
                current = item;
            }

            const bool sectorChildrenPopulated = item->data(0, kOutlinerChildrenPopulatedRole).toBool();
            const bool sectorVisibleTreeState = item->isExpanded() || sectorChildrenPopulated;
            const bool allowAutoPopulateForSelection =
                !bulkSurfaceSelection || sectorIndex == primarySector || sectorVisibleTreeState;

            if (sectorsToExpand.contains(sectorIndex) && allowAutoPopulateForSelection)
            {
                populateOutlinerSectorChildren(item);
                if (!bulkSurfaceSelection || sectorIndex == primarySector)
                {
                    item->setExpanded(true);
                }
            }

            if (!needSurfaceWalk || (bulkSurfaceSelection && !item->data(0, kOutlinerChildrenPopulatedRole).toBool()))
            {
                continue;
            }

            for (int ci = 0; ci < item->childCount(); ++ci)
            {
                QTreeWidgetItem* surfItem = item->child(ci);
                if (!surfItem || static_cast<OutlinerKind>(surfItem->data(0, kOutlinerKindRole).toInt()) != OutlinerKind::surface)
                {
                    continue;
                }

                const std::int32_t surfaceIndex = static_cast<std::int32_t>(surfItem->data(0, kOutlinerIndexRole).toInt());
                const SurfaceRef sref{ sectorIndex, surfaceIndex };
                const std::uint64_t surfaceKey = encodeOutlinerSurfaceKey(sectorIndex, surfaceIndex);
                const bool surfaceSelected = selectedSurfaceKeys.contains(surfaceKey);
                const bool vertexSelected = vertexSurfaceKeys.contains(surfaceKey);

                surfItem->setSelected(surfaceSelected || vertexSelected);
                if (vertexSelected)
                {
                    populateOutlinerSurfaceVertexChildren(surfItem);
                    item->setExpanded(true);
                    surfItem->setExpanded(true);
                }

                if (primarySurface == sref && primaryVertex.surface.sector < 0)
                {
                    current = surfItem;
                }

                if (!needVertexWalk)
                {
                    continue;
                }

                QTreeWidgetItem* vertsGroup = nullptr;
                for (int vi = 0; vi < surfItem->childCount(); ++vi)
                {
                    QTreeWidgetItem* maybeGroup = surfItem->child(vi);
                    if (!maybeGroup || maybeGroup->data(0, kOutlinerKindRole).toInt() != static_cast<int>(OutlinerKind::none))
                    {
                        continue;
                    }

                    vertsGroup = maybeGroup;
                    break;
                }

                if (vertsGroup && vertexSelected)
                {
                    vertsGroup->setExpanded(true);
                }

                if (!vertsGroup)
                {
                    continue;
                }

                for (int vi = 0; vi < vertsGroup->childCount(); ++vi)
                {
                    QTreeWidgetItem* vertexItem = vertsGroup->child(vi);
                    if (!vertexItem || static_cast<OutlinerKind>(vertexItem->data(0, kOutlinerKindRole).toInt()) != OutlinerKind::surfaceVertex)
                    {
                        continue;
                    }

                    const std::int32_t localVertex = static_cast<std::int32_t>(vertexItem->data(0, kOutlinerIndexRole).toInt());
                    const editor::SurfaceVertexRef vref{ sref, localVertex };
                    const bool vertexIsSelected = selection.surfaceVertices.contains(vref);
                    vertexItem->setSelected(vertexIsSelected);

                    if (primaryVertex.surface == sref && primaryVertex.localVertex == localVertex)
                    {
                        current = vertexItem;
                    }
                }
            }
        }
    }
    else
    {
        std::unordered_set<std::int32_t> selectedIndices{};
        std::int32_t primaryIndex = -1;
        OutlinerKind expectedKind = OutlinerKind::none;

        if (tab == 1)
        {
            expectedKind = OutlinerKind::thing;
            selectedIndices = selection.things;
            primaryIndex = minIndex(selection.things);
        }
        else if (tab == 2)
        {
            expectedKind = OutlinerKind::cog;
            selectedIndices = selection.cogs;
            primaryIndex = minIndex(selection.cogs);
        }
        else if (tab == 3)
        {
            expectedKind = OutlinerKind::light;
            selectedIndices = selection.lights;
            primaryIndex = minIndex(selection.lights);
        }

        for (int i = 0; i < outliner_->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* item = outliner_->topLevelItem(i);
            if (!item || static_cast<OutlinerKind>(item->data(0, kOutlinerKindRole).toInt()) != expectedKind)
            {
                continue;
            }

            const std::int32_t index = static_cast<std::int32_t>(item->data(0, kOutlinerIndexRole).toInt());
            const bool selected = selectedIndices.contains(index);
            item->setSelected(selected);

            if (index == primaryIndex)
            {
                current = item;
            }
        }
    }

    if (current)
    {
        outliner_->selectionModel()->setCurrentIndex(
            outliner_->indexFromItem(current), QItemSelectionModel::NoUpdate);
    }
    else
    {
        outliner_->selectionModel()->clearCurrentIndex();
    }

    if (glView_)
    {
        QTreeWidgetItem* focused = outliner_->currentItem();
        if (focused && focused->isSelected()
            && static_cast<OutlinerKind>(focused->data(0, kOutlinerKindRole).toInt()) == OutlinerKind::surfaceVertex)
        {
            const int sectorIndex = focused->data(0, kOutlinerSectorRole).toInt();
            const int surfaceIndex = focused->data(0, kOutlinerSurfaceRole).toInt();
            const int localVertex = focused->data(0, kOutlinerIndexRole).toInt();
            glView_->setActiveSurfaceVertexHint(sectorIndex, surfaceIndex, localVertex);
        }
        else
        {
            const auto activeVertex = glView_->activeSurfaceVertex();
            if (!activeVertex
                || !selection.surfaceVertices.contains(*activeVertex))
            {
                glView_->clearActiveSurfaceVertexHint();
            }
        }
    }
}

void MainWindow::appendThingsToOutliner(const std::size_t firstThingIndex)
{
    if (!outliner_ || !outlinerTabs_ || outlinerTabs_->currentIndex() != 1)
    {
        refreshOutlinerTabCaptions();
        return;
    }

    const Level& level = document_.level();
    const std::size_t safeFirst = std::min(firstThingIndex, level.things.size());
    const std::size_t includedBefore = static_cast<std::size_t>(std::count_if(
        level.things.begin(),
        level.things.begin() + static_cast<std::ptrdiff_t>(safeFirst),
        [this](const Thing& thing)
        {
            return outlinerIncludesLayer(
                outlinerShowHiddenLayerEntities_, layerVisible_, thing.layer);
        }));
    if (firstThingIndex > level.things.size()
        || static_cast<std::size_t>(outliner_->topLevelItemCount())
            != includedBefore)
    {
        rebuildOutliner();
        return;
    }

    const QSignalBlocker blocker(outliner_);
    const ScopedWidgetUpdatesDisabled updatesGuard(outliner_);
    outliner_->clearSelection();

    QTreeWidgetItem* current = nullptr;
    for (std::size_t i = firstThingIndex; i < level.things.size(); ++i)
    {
        const Thing& thing = level.things[i];
        if (!outlinerIncludesLayer(
                outlinerShowHiddenLayerEntities_, layerVisible_, thing.layer))
        {
            continue;
        }
        auto* item = new QTreeWidgetItem(outliner_);
        item->setText(
            0,
            QString("%1: %2")
                .arg(i)
                .arg(QString::fromStdString(thing.name)));
        item->setData(0, kOutlinerKindRole, static_cast<int>(OutlinerKind::thing));
        item->setData(0, kOutlinerIndexRole, static_cast<int>(i));
        applyOutlinerLayerTint(
            item,
            outlinerColorByLayer_,
            layerColors_,
            thing.layer);
        applyOutlinerLayerStateBadge(
            item, layerVisible_, layerLocked_, thing.layer);

        const std::int32_t index = static_cast<std::int32_t>(i);
        if (document_.selection().things.contains(index))
        {
            item->setSelected(true);
            if (!current)
            {
                current = item;
            }
        }
    }

    if (current)
    {
        outliner_->selectionModel()->setCurrentIndex(
            outliner_->indexFromItem(current), QItemSelectionModel::NoUpdate);
    }
    if (outlinerCountLabel_)
    {
        const int displayed = outliner_->topLevelItemCount();
        outlinerCountLabel_->setText(
            displayed == static_cast<int>(level.things.size())
                ? QString("Things: %1").arg(level.things.size())
                : QString("Things: %1 of %2")
                      .arg(displayed)
                      .arg(level.things.size()));
    }
    refreshOutlinerTabCaptions();
}

void MainWindow::appendLightsToOutliner(const std::size_t firstLightIndex)
{
    if (!outliner_ || !outlinerTabs_ || outlinerTabs_->currentIndex() != 3)
    {
        refreshOutlinerTabCaptions();
        return;
    }

    const Level& level = document_.level();
    const std::size_t safeFirst = std::min(firstLightIndex, level.lights.size());
    const std::size_t includedBefore = static_cast<std::size_t>(std::count_if(
        level.lights.begin(),
        level.lights.begin() + static_cast<std::ptrdiff_t>(safeFirst),
        [this](const SedLight& light)
        {
            return outlinerIncludesLayer(
                outlinerShowHiddenLayerEntities_, layerVisible_, light.layer);
        }));
    if (firstLightIndex > level.lights.size()
        || static_cast<std::size_t>(outliner_->topLevelItemCount())
            != includedBefore)
    {
        rebuildOutliner();
        return;
    }

    const QSignalBlocker blocker(outliner_);
    const ScopedWidgetUpdatesDisabled updatesGuard(outliner_);
    outliner_->clearSelection();

    QTreeWidgetItem* current = nullptr;
    for (std::size_t i = firstLightIndex; i < level.lights.size(); ++i)
    {
        const SedLight& light = level.lights[i];
        if (!outlinerIncludesLayer(
                outlinerShowHiddenLayerEntities_, layerVisible_, light.layer))
        {
            continue;
        }

        auto* item = new QTreeWidgetItem(outliner_);
        item->setText(
            0,
            QString("%1: Light (id %2)").arg(i).arg(light.id));
        item->setData(0, kOutlinerKindRole, static_cast<int>(OutlinerKind::light));
        item->setData(0, kOutlinerIndexRole, static_cast<int>(i));
        applyOutlinerLayerTint(
            item,
            outlinerColorByLayer_,
            layerColors_,
            light.layer);
        applyOutlinerLayerStateBadge(
            item, layerVisible_, layerLocked_, light.layer);

        const std::int32_t index = static_cast<std::int32_t>(i);
        if (document_.selection().lights.contains(index))
        {
            item->setSelected(true);
            if (!current)
            {
                current = item;
            }
        }
    }

    if (current)
    {
        outliner_->selectionModel()->setCurrentIndex(
            outliner_->indexFromItem(current), QItemSelectionModel::NoUpdate);
    }
    if (outlinerCountLabel_)
    {
        const int displayed = outliner_->topLevelItemCount();
        outlinerCountLabel_->setText(
            displayed == static_cast<int>(level.lights.size())
                ? QString("Lights: %1").arg(level.lights.size())
                : QString("Lights: %1 of %2")
                      .arg(displayed)
                      .arg(level.lights.size()));
    }
    refreshOutlinerTabCaptions();
}

void MainWindow::rebuildOutliner()
{
    if (!outliner_)
    {
        return;
    }

    const Level& level = document_.level();
    refreshOutlinerTabCaptions();

    const QSignalBlocker blocker(outliner_);
    const ScopedWidgetUpdatesDisabled updatesGuard(outliner_);

    std::unordered_set<std::int32_t> expandedSectorItems{};
    if (outlinerTabs_ && outlinerTabs_->currentIndex() == 0)
    {
        const int topCount = outliner_->topLevelItemCount();
        expandedSectorItems.reserve(static_cast<std::size_t>(topCount));

        std::unordered_set<std::uint64_t> expandedSurfaceItems{};
        expandedSurfaceItems.reserve(static_cast<std::size_t>(topCount) * 4);

        for (int i = 0; i < topCount; ++i)
        {
            QTreeWidgetItem* item = outliner_->topLevelItem(i);
            if (!item || !item->isExpanded())
            {
                continue;
            }

            const auto kind = static_cast<OutlinerKind>(item->data(0, kOutlinerKindRole).toInt());
            if (kind != OutlinerKind::sector)
            {
                continue;
            }

            expandedSectorItems.insert(item->data(0, kOutlinerIndexRole).toInt());

            // Remember expanded surfaces (so vertex sub-trees don't collapse on rebuild).
            for (int ci = 0; ci < item->childCount(); ++ci)
            {
                QTreeWidgetItem* child = item->child(ci);
                if (!child || !child->isExpanded())
                {
                    continue;
                }

                const auto ckind = static_cast<OutlinerKind>(child->data(0, kOutlinerKindRole).toInt());
                if (ckind != OutlinerKind::surface)
                {
                    continue;
                }

                const int sectorIndex = child->data(0, kOutlinerSectorRole).toInt();
                const int surfaceIndex = child->data(0, kOutlinerIndexRole).toInt();
                const std::uint64_t key =
                    (static_cast<std::uint64_t>(static_cast<std::uint32_t>(sectorIndex)) << 32u)
                    ^ static_cast<std::uint64_t>(static_cast<std::uint32_t>(surfaceIndex));
                expandedSurfaceItems.insert(key);
            }
        }

        // Stash expanded surfaces in a dynamic property so we can use it after clearing.
        QVariantList expandedKeys{};
        for (const std::uint64_t k : expandedSurfaceItems)
        {
            expandedKeys.append(QVariant::fromValue(static_cast<qulonglong>(k)));
        }
        outliner_->setProperty("sed.expandedSurfaceItems", expandedKeys);
    }
    outliner_->clear();

    const int tab = outlinerTabs_ ? outlinerTabs_->currentIndex() : 0;
    if (outlinerCountLabel_)
    {
        outlinerCountLabel_->setText(" ");
    }

    auto* current = static_cast<QTreeWidgetItem*>(nullptr);
    auto selectCurrent = [&](QTreeWidgetItem* item)
    {
        if (!item)
        {
            return;
        }
        item->setSelected(true);
        if (!current)
        {
            current = item;
        }
    };

    auto applyLayerTint = [this](QTreeWidgetItem* item, const std::int32_t layerIndex) -> void
    {
        if (!outlinerColorByLayer_ || !item || layerIndex < 0)
        {
            return;
        }

        const std::size_t idx = static_cast<std::size_t>(layerIndex);
        if (idx >= layerColors_.size())
        {
            return;
        }

        QColor tint = layerColors_[idx];
        if (!tint.isValid())
        {
            return;
        }

        tint = tint.lighter(145);
        tint.setAlpha(255);
        item->setForeground(0, QBrush(tint));
    };

    auto addItem = [&](QTreeWidgetItem* parent,
                       const QString& text,
                       const OutlinerKind kind,
                       const int index,
                       const int sectorIndex = -1,
                       const int layerIndex = -1) -> QTreeWidgetItem*
    {
        auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(outliner_);
        item->setText(0, text);
        item->setData(0, kOutlinerKindRole, static_cast<int>(kind));
        item->setData(0, kOutlinerIndexRole, index);
        if (sectorIndex >= 0)
        {
            item->setData(0, kOutlinerSectorRole, sectorIndex);
        }
        applyLayerTint(item, layerIndex);
        applyOutlinerLayerStateBadge(
            item, layerVisible_, layerLocked_, layerIndex);
        return item;
    };

    auto minIndex = [](const auto& set) -> std::int32_t
    {
        if (set.empty())
        {
            return -1;
        }
        return *std::min_element(set.begin(), set.end());
    };

    const editor::Selection& selection = document_.selection();
    const bool bulkSurfaceSelection = outlinerTreatSurfaceSelectionAsBulk(selection);

    if (tab == 0)
    {
        int displayedSectorCount = 0;

        std::unordered_set<std::int32_t> sectorsToExpand = expandedSectorItems;
        sectorsToExpand.reserve(expandedSectorItems.size() + selection.surfaces.size() + selection.surfaceVertices.size() + 1);

        auto minSurfaceRef = [](const auto& set) -> SurfaceRef
        {
            SurfaceRef best{ -1, -1 };
            for (const SurfaceRef& ref : set)
            {
                if (best.sector < 0 || ref.sector < best.sector || (ref.sector == best.sector && ref.surface < best.surface))
                {
                    best = ref;
                }
            }
            return best;
        };

        auto minSurfaceVertexRef = [](const auto& set) -> editor::SurfaceVertexRef
        {
            editor::SurfaceVertexRef best{};
            best.surface = SurfaceRef{ -1, -1 };
            best.localVertex = -1;

            for (const editor::SurfaceVertexRef& ref : set)
            {
                if (best.surface.sector < 0
                    || ref.surface.sector < best.surface.sector
                    || (ref.surface.sector == best.surface.sector && ref.surface.surface < best.surface.surface)
                    || (ref.surface == best.surface && ref.localVertex < best.localVertex))
                {
                    best = ref;
                }
            }
            return best;
        };

        if (bulkSurfaceSelection)
        {
            const SurfaceRef primarySurface = activeSelectedSurfaceRef().value_or(
                minSurfaceRef(selection.surfaces));
            editor::SurfaceVertexRef primaryVertex = minSurfaceVertexRef(selection.surfaceVertices);
            if (glView_)
            {
                if (const auto activeVertex = glView_->activeSurfaceVertex();
                    activeVertex
                    && selection.surfaceVertices.contains(*activeVertex))
                {
                    primaryVertex = *activeVertex;
                }
            }
            if (primaryVertex.surface.sector >= 0)
            {
                sectorsToExpand.insert(primaryVertex.surface.sector);
            }
            else if (primarySurface.sector >= 0)
            {
                sectorsToExpand.insert(primarySurface.sector);
            }
        }
        else
        {
            for (const SurfaceRef& ref : selection.surfaces)
            {
                sectorsToExpand.insert(ref.sector);
            }
            for (const editor::SurfaceVertexRef& ref : selection.surfaceVertices)
            {
                sectorsToExpand.insert(ref.surface.sector);
            }
        }

        for (std::size_t i = 0; i < level.sectors.size(); ++i)
        {
            const Sector& sector = level.sectors[i];
            if (!outlinerIncludesLayer(
                    outlinerShowHiddenLayerEntities_, layerVisible_, sector.layer))
            {
                continue;
            }
            ++displayedSectorCount;
            auto* item = addItem(
                nullptr,
                QString("%1: Sector (id %2, surfaces %3)").arg(i).arg(sector.id).arg(sector.surfaces.size()),
                OutlinerKind::sector,
                static_cast<int>(i),
                -1,
                sector.layer);
            item->setData(0, kOutlinerChildrenPopulatedRole, false);
            if (!sector.surfaces.empty())
            {
                item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
            }

            const auto idx = static_cast<std::int32_t>(i);
            const bool expand = sectorsToExpand.contains(idx);
            if (expand)
            {
                populateOutlinerSectorChildren(item);
                item->setExpanded(true);
            }
        }

        if (outlinerCountLabel_)
        {
            outlinerCountLabel_->setText(
                displayedSectorCount == static_cast<int>(level.sectors.size())
                    ? QString("Sectors: %1").arg(level.sectors.size())
                    : QString("Sectors: %1 of %2")
                          .arg(displayedSectorCount)
                          .arg(level.sectors.size()));
        }

        syncOutlinerSelectionOnly();
        return;
    }
    else if (tab == 1)
    {
        int displayedThingCount = 0;

        const std::int32_t primaryThing = minIndex(selection.things);

        for (std::size_t i = 0; i < level.things.size(); ++i)
        {
            const Thing& thing = level.things[i];
            if (!outlinerIncludesLayer(
                    outlinerShowHiddenLayerEntities_, layerVisible_, thing.layer))
            {
                continue;
            }
            ++displayedThingCount;
            auto* item = addItem(
                nullptr,
                QString("%1: %2").arg(i).arg(QString::fromStdString(thing.name)),
                OutlinerKind::thing,
                static_cast<int>(i),
                -1,
                thing.layer);
            const auto idx = static_cast<std::int32_t>(i);
            if (selection.things.contains(idx))
            {
                selectCurrent(item);
            }
            if (idx == primaryThing)
            {
                current = item;
            }
        }
        if (outlinerCountLabel_)
        {
            outlinerCountLabel_->setText(
                displayedThingCount == static_cast<int>(level.things.size())
                    ? QString("Things: %1").arg(level.things.size())
                    : QString("Things: %1 of %2")
                          .arg(displayedThingCount)
                          .arg(level.things.size()));
        }
    }
    else if (tab == 2)
    {
        const QString cogFilter = cogOutlinerSearchEdit_ ? cogOutlinerSearchEdit_->text().trimmed() : QString{};
        const bool hasCogFilter = !cogFilter.isEmpty();
        int visibleCogCount = 0;

        if (outlinerCountLabel_)
        {
            outlinerCountLabel_->setText(
                hasCogFilter
                    ? QString("COGs: %1 of %2").arg(0).arg(level.cogs.size())
                    : QString("COGs: %1").arg(level.cogs.size()));
        }

        const std::int32_t primaryCog = minIndex(selection.cogs);

        for (std::size_t i = 0; i < level.cogs.size(); ++i)
        {
            const Cog& cog = level.cogs[i];
            const QString cogName = QString::fromStdString(cog.name);
            if (hasCogFilter && !cogName.contains(cogFilter, Qt::CaseInsensitive))
            {
                continue;
            }

            ++visibleCogCount;
            auto* item = addItem(
                nullptr,
                QString("%1: %2 (id %3, values %4)")
                    .arg(i)
                    .arg(cogName)
                    .arg(cog.id)
                    .arg(static_cast<int>(cog.values.size())),
                OutlinerKind::cog,
                static_cast<int>(i));
            const auto idx = static_cast<std::int32_t>(i);
            if (selection.cogs.contains(idx))
            {
                selectCurrent(item);
            }
            if (idx == primaryCog)
            {
                current = item;
            }
        }

        if (outlinerCountLabel_)
        {
            outlinerCountLabel_->setText(
                hasCogFilter
                    ? QString("COGs: %1 of %2").arg(visibleCogCount).arg(level.cogs.size())
                    : QString("COGs: %1").arg(level.cogs.size()));
        }
    }
    else
    {
        int displayedLightCount = 0;

        const std::int32_t primaryLight = minIndex(selection.lights);

        for (std::size_t i = 0; i < level.lights.size(); ++i)
        {
            const SedLight& light = level.lights[i];
            if (!outlinerIncludesLayer(
                    outlinerShowHiddenLayerEntities_, layerVisible_, light.layer))
            {
                continue;
            }
            ++displayedLightCount;
            auto* item = addItem(
                nullptr,
                QString("%1: Light (id %2)").arg(i).arg(light.id),
                OutlinerKind::light,
                static_cast<int>(i),
                -1,
                light.layer);
            const auto idx = static_cast<std::int32_t>(i);
            if (selection.lights.contains(idx))
            {
                selectCurrent(item);
            }
            if (idx == primaryLight)
            {
                current = item;
            }
        }
        if (outlinerCountLabel_)
        {
            outlinerCountLabel_->setText(
                displayedLightCount == static_cast<int>(level.lights.size())
                    ? QString("Lights: %1").arg(level.lights.size())
                    : QString("Lights: %1 of %2")
                          .arg(displayedLightCount)
                          .arg(level.lights.size()));
        }
    }

    if (current)
    {
        outliner_->selectionModel()->setCurrentIndex(
            outliner_->indexFromItem(current), QItemSelectionModel::NoUpdate);
    }

    if (glView_)
    {
        QTreeWidgetItem* focused = outliner_->currentItem();
        if (focused && focused->isSelected()
            && static_cast<OutlinerKind>(focused->data(0, kOutlinerKindRole).toInt()) == OutlinerKind::surfaceVertex)
        {
            const int sectorIndex = focused->data(0, kOutlinerSectorRole).toInt();
            const int surfaceIndex = focused->data(0, kOutlinerSurfaceRole).toInt();
            const int localVertex = focused->data(0, kOutlinerIndexRole).toInt();
            glView_->setActiveSurfaceVertexHint(sectorIndex, surfaceIndex, localVertex);
        }
        else
        {
            glView_->clearActiveSurfaceVertexHint();
        }
    }
}

} // namespace sed
