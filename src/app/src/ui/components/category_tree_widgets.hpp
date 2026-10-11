#pragma once

#include "ui/components/preview_thumbnail_list.hpp"

#include <algorithm>
#include <functional>

#include <QEvent>
#include <QDragEnterEvent>
#include <QDrag>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QApplication>
#include <QIcon>
#include <QItemSelection>
#include <QKeyEvent>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QStyle>
#include <QStyleOption>
#include <QScrollBar>
#include <QTimer>
#include <QTreeWidget>

namespace sed::ui::detail
{

inline constexpr char kCategoryAssignmentMimeType[] = "application/x-sed-category-assignment-items";

inline QByteArray encodeCategoryAssignmentPayload(QStringList names)
{
    for (QString& name : names)
    {
        name = name.trimmed();
    }
    names.removeAll(QString{});
    names.removeDuplicates();
    return names.join(QChar::fromLatin1('\n')).toUtf8();
}

inline QStringList decodeCategoryAssignmentPayload(const QMimeData* mime)
{
    if (!mime || !mime->hasFormat(kCategoryAssignmentMimeType))
    {
        return {};
    }

    QStringList names = QString::fromUtf8(mime->data(kCategoryAssignmentMimeType))
                            .split(QChar::fromLatin1('\n'), Qt::SkipEmptyParts);
    for (QString& name : names)
    {
        name = name.trimmed();
    }
    names.removeAll(QString{});
    names.removeDuplicates();
    return names;
}

class CategoryAssignableListWidget final : public QListWidget
{
public:
    using NameProvider = std::function<QString(const QListWidgetItem*)>;

    explicit CategoryAssignableListWidget(QWidget* parent = nullptr)
        : QListWidget(parent)
    {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setDefaultDropAction(Qt::CopyAction);
        setCategoryMultiSelectMode(false);
        if (viewport())
        {
            viewport()->installEventFilter(this);
        }
    }

    void setCategoryDragNameProvider(NameProvider provider)
    {
        nameProvider_ = std::move(provider);
    }

    bool categoryMultiSelectMode() const noexcept
    {
        return multiSelectMode_;
    }

    QStringList categoryDragNames() const
    {
        QStringList names = preservedDragNames_;
        if (names.isEmpty() && multiSelectMode_ && !lastMultiSelectionDragNames_.isEmpty())
        {
            names = lastMultiSelectionDragNames_;
        }
        if (names.isEmpty())
        {
            QList<QListWidgetItem*> items = selectedItems();
            if (items.isEmpty())
            {
                if (QListWidgetItem* current = currentItem())
                {
                    items.push_back(current);
                }
            }
            names = categoryNamesForItems(items);
        }
        return names;
    }

    void disableCategoryMultiSelectPreserveSelection()
    {
        const QPersistentModelIndex current = currentIndex();
        multiSelectMode_ = false;
        lastMultiSelectionDragNames_.clear();
        setSelectionMode(QAbstractItemView::SingleSelection);
        setSelectionRectVisible(false);
        if (selectionModel())
        {
            clearSelection();
            if (current.isValid())
            {
                selectionModel()->select(current, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                setCurrentIndex(current);
            }
        }
        if (viewport())
        {
            viewport()->setCursor(Qt::ArrowCursor);
            viewport()->update();
        }
    }

    void setCategoryMultiSelectMode(const bool enabled)
    {
        multiSelectMode_ = enabled;
        if (!enabled)
        {
            lastMultiSelectionDragNames_.clear();
        }
        setSelectionMode(enabled ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
        setSelectionRectVisible(enabled);
        if (viewport())
        {
            viewport()->setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
        }

        if (!enabled && selectionModel())
        {
            const QModelIndex current = currentIndex();
            clearSelection();
            if (current.isValid())
            {
                selectionModel()->select(current, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                setCurrentIndex(current);
            }
        }
        if (viewport())
        {
            viewport()->update();
        }
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event
            && viewMode() == QListView::IconMode
            && (event->key() == Qt::Key_PageDown
                || event->key() == Qt::Key_PageUp))
        {
            if (QScrollBar* scroll = verticalScrollBar())
            {
                const int direction = event->key() == Qt::Key_PageDown ? 1 : -1;
                const int pageStep = previewThumbnailPageStep(
                    scroll->pageStep(), gridSize().height());
                scroll->setValue(scroll->value() + direction * pageStep);
                event->accept();
                return;
            }
        }
        QListWidget::keyPressEvent(event);
    }

    QStringList mimeTypes() const override
    {
        return { QString::fromLatin1(kCategoryAssignmentMimeType) };
    }

    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override
    {
        QStringList names = categoryNamesForItems(items);

        if (names.isEmpty())
        {
            return QListWidget::mimeData(items);
        }

        return categoryMimeDataFromNames(std::move(names));
    }

    Qt::DropActions supportedDropActions() const override
    {
        return Qt::CopyAction | Qt::MoveAction;
    }

    void startDrag(Qt::DropActions) override
    {
        startCategoryDrag();
    }

    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override
    {
        QListWidget::selectionChanged(selected, deselected);
        if (!multiSelectMode_)
        {
            return;
        }

        QStringList names = categoryNamesForItems(selectedItems());
        if (!names.isEmpty())
        {
            lastMultiSelectionDragNames_ = std::move(names);
        }
        else
        {
            lastMultiSelectionDragNames_.clear();
        }
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == viewport())
        {
            switch (event ? event->type() : QEvent::None)
            {
                case QEvent::MouseButtonPress:
                {
                    const auto* mouseEvent = static_cast<QMouseEvent*>(event);
                    pressPos_ = mouseEvent ? mouseEvent->pos() : QPoint{};
                    pressStartedOnSelectedItem_ = false;
                    dragStartedThisPress_ = false;
                    movedDuringPress_ = false;
                    plainPressShouldExitMultiSelect_ = false;
                    preservedSelection_.clear();
                    preservedDragNames_.clear();
                    pressedIndex_ = QPersistentModelIndex{};
                    if (mouseEvent && mouseEvent->button() == Qt::LeftButton)
                    {
                        const QModelIndex index = indexAt(mouseEvent->pos());
                        if (QListWidgetItem* item = itemAt(mouseEvent->pos()))
                        {
                            pressStartedOnSelectedItem_ = item->isSelected();
                            if (multiSelectMode_ && mouseEvent->modifiers() == Qt::NoModifier)
                            {
                                plainPressShouldExitMultiSelect_ = true;
                            }
                            if ((multiSelectMode_ || selectedIndexes().size() > 1)
                                && pressStartedOnSelectedItem_
                                && mouseEvent->modifiers() == Qt::NoModifier)
                            {
                                pressedIndex_ = QPersistentModelIndex(index);
                                for (const QModelIndex& selected : selectedIndexes())
                                {
                                    if (selected.isValid())
                                    {
                                        preservedSelection_.push_back(QPersistentModelIndex(selected));
                                    }
                                }
                                preservedDragNames_ = categoryNamesForItems(selectedItems());
                                return true;
                            }
                        }
                        else if (multiSelectMode_ && mouseEvent->modifiers() == Qt::NoModifier)
                        {
                            plainPressShouldExitMultiSelect_ = true;
                        }
                    }
                    break;
                }
                case QEvent::MouseMove:
                {
                    const auto* mouseEvent = static_cast<QMouseEvent*>(event);
                    if (mouseEvent
                        && (mouseEvent->buttons() & Qt::LeftButton)
                        && (mouseEvent->pos() - pressPos_).manhattanLength() >= QApplication::startDragDistance())
                    {
                        movedDuringPress_ = true;
                    }
                    if (mouseEvent
                        && (mouseEvent->buttons() & Qt::LeftButton)
                        && pressStartedOnSelectedItem_
                        && (mouseEvent->pos() - pressPos_).manhattanLength() >= QApplication::startDragDistance())
                    {
                        pressStartedOnSelectedItem_ = false;
                        dragStartedThisPress_ = true;
                        restorePreservedSelection();
                        startCategoryDrag();
                        return true;
                    }
                    break;
                }
                case QEvent::MouseButtonRelease:
                {
                    bool deferDisableNormal = false;
                    const QPersistentModelIndex releasedIndex = pressedIndex_;
                    if (multiSelectMode_ && plainPressShouldExitMultiSelect_ && !dragStartedThisPress_ && !movedDuringPress_)
                    {
                        deferDisableNormal = true;
                    }
                    pressStartedOnSelectedItem_ = false;
                    dragStartedThisPress_ = false;
                    movedDuringPress_ = false;
                    plainPressShouldExitMultiSelect_ = false;
                    preservedSelection_.clear();
                    preservedDragNames_.clear();
                    pressedIndex_ = QPersistentModelIndex{};
                    if (deferDisableNormal)
                    {
                        QTimer::singleShot(0, this, [this, releasedIndex]()
                            {
                                // The press was swallowed so a selected-row drag could
                                // retain the complete multi-selection. If it remained a
                                // click, make the clicked row current before collapsing
                                // back to the normal one-item selection.
                                if (releasedIndex.isValid())
                                {
                                    setCurrentIndex(releasedIndex);
                                }
                                disableCategoryMultiSelectPreserveSelection();
                            });
                    }
                    break;
                }
                default:
                    break;
            }
        }

        return QListWidget::eventFilter(watched, event);
    }

private:
    QString categoryNameForItem(const QListWidgetItem* item) const
    {
        if (!item)
        {
            return {};
        }

        const QString name = nameProvider_ ? nameProvider_(item) : item->text();
        return name.trimmed();
    }

    QStringList categoryNamesForItems(const QList<QListWidgetItem*>& items) const
    {
        QStringList names{};
        for (const QListWidgetItem* item : items)
        {
            const QString name = categoryNameForItem(item);
            if (!name.isEmpty())
            {
                names.push_back(name);
            }
        }
        names.removeDuplicates();
        return names;
    }

    QMimeData* categoryMimeDataFromNames(QStringList names) const
    {
        for (QString& name : names)
        {
            name = name.trimmed();
        }
        names.removeAll(QString{});
        names.removeDuplicates();
        if (names.isEmpty())
        {
            return nullptr;
        }

        auto* mime = new QMimeData();
        mime->setData(kCategoryAssignmentMimeType, encodeCategoryAssignmentPayload(std::move(names)));
        return mime;
    }

    QList<QListWidgetItem*> dragPreviewItems() const
    {
        QList<QListWidgetItem*> items{};
        for (const QPersistentModelIndex& index : preservedSelection_)
        {
            if (!index.isValid())
            {
                continue;
            }
            if (QListWidgetItem* item = itemFromIndex(index))
            {
                items.push_back(item);
            }
        }

        if (items.isEmpty())
        {
            items = selectedItems();
        }
        if (items.isEmpty())
        {
            if (QListWidgetItem* current = currentItem())
            {
                items.push_back(current);
            }
        }
        return items;
    }

    QPixmap makeCategoryDragPixmap(const QList<QListWidgetItem*>& items, const int itemCount) const
    {
        const int count = std::max(1, itemCount);
        const int cardSize = 46;
        const int offsetX = 7;
        const int offsetY = 5;
        const int visibleCards = std::max(1, std::min(3, static_cast<int>(items.size())));
        const int width = cardSize + 22 + (visibleCards - 1) * offsetX;
        const int height = cardSize + 16 + (visibleCards - 1) * offsetY;

        QPixmap pixmap(width, height);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        for (int i = visibleCards - 1; i >= 0; --i)
        {
            const QRect cardRect(6 + i * offsetX, 5 + i * offsetY, cardSize, cardSize);
            painter.setPen(QPen(QColor(255, 255, 255, 90), 1));
            painter.setBrush(QColor(34, 38, 46, 230));
            painter.drawRoundedRect(cardRect, 6, 6);

            QPixmap iconPixmap{};
            if (i < items.size() && items[i] && !items[i]->icon().isNull())
            {
                iconPixmap = items[i]->icon().pixmap(QSize(38, 38));
            }

            if (!iconPixmap.isNull())
            {
                const QPoint iconPos(
                    cardRect.left() + (cardRect.width() - iconPixmap.width()) / 2,
                    cardRect.top() + (cardRect.height() - iconPixmap.height()) / 2);
                painter.drawPixmap(iconPos, iconPixmap);
            }
            else
            {
                painter.setPen(QColor(210, 216, 226, 170));
                QFont font = painter.font();
                font.setBold(true);
                font.setPointSize(15);
                painter.setFont(font);
                painter.drawText(cardRect, Qt::AlignCenter, QStringLiteral("SED"));
            }
        }

        if (count > 1)
        {
            const QString badgeText = QString::number(count);
            const QRect badgeRect(width - 31, height - 25, 26, 20);
            painter.setPen(QPen(QColor(255, 255, 255, 190), 1));
            painter.setBrush(QColor(36, 105, 210, 238));
            painter.drawRoundedRect(badgeRect, 10, 10);
            painter.setPen(Qt::white);
            QFont font = painter.font();
            font.setBold(true);
            font.setPointSize(9);
            painter.setFont(font);
            painter.drawText(badgeRect, Qt::AlignCenter, badgeText);
        }

        return pixmap;
    }

    void startCategoryDrag()
    {
        QStringList names = categoryDragNames();
        const bool exitMultiSelectAfterDrag = multiSelectMode_;
        if (names.isEmpty())
        {
            finishCategoryDragGesture(exitMultiSelectAfterDrag);
            return;
        }

        const QList<QListWidgetItem*> previewItems = dragPreviewItems();
        QMimeData* data = categoryMimeDataFromNames(std::move(names));
        if (!data)
        {
            finishCategoryDragGesture(exitMultiSelectAfterDrag);
            return;
        }

        auto* drag = new QDrag(this);
        drag->setMimeData(data);
        const QPixmap dragPixmap = makeCategoryDragPixmap(previewItems, decodeCategoryAssignmentPayload(data).size());
        if (!dragPixmap.isNull())
        {
            drag->setPixmap(dragPixmap);
            drag->setHotSpot(QPoint(14, 14));
        }
        drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
        finishCategoryDragGesture(exitMultiSelectAfterDrag);
    }

    void finishCategoryDragGesture(const bool exitMultiSelect)
    {
        pressStartedOnSelectedItem_ = false;
        dragStartedThisPress_ = false;
        movedDuringPress_ = false;
        plainPressShouldExitMultiSelect_ = false;
        preservedSelection_.clear();
        preservedDragNames_.clear();
        pressedIndex_ = QPersistentModelIndex{};

        setState(QAbstractItemView::NoState);
        if (exitMultiSelect)
        {
            disableCategoryMultiSelectPreserveSelection();
        }
        else
        {
            setSelectionRectVisible(false);
        }
        if (viewport())
        {
            viewport()->update();
        }
    }

    void restorePreservedSelection()
    {
        if (!selectionModel() || preservedSelection_.isEmpty())
        {
            return;
        }

        clearSelection();
        for (const QPersistentModelIndex& index : preservedSelection_)
        {
            if (index.isValid())
            {
                selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            }
        }
        if (pressedIndex_.isValid())
        {
            setCurrentIndex(pressedIndex_);
        }
    }

    NameProvider nameProvider_{};
    QPoint pressPos_{};
    bool pressStartedOnSelectedItem_{ false };
    bool multiSelectMode_{ false };
    bool dragStartedThisPress_{ false };
    bool movedDuringPress_{ false };
    bool plainPressShouldExitMultiSelect_{ false };
    QList<QPersistentModelIndex> preservedSelection_{};
    QStringList preservedDragNames_{};
    QStringList lastMultiSelectionDragNames_{};
    QPersistentModelIndex pressedIndex_{};
};

class CategoryTreeWidget final : public QTreeWidget
{
public:
    using DropActionResolver = std::function<Qt::DropAction(QTreeWidgetItem*, const QStringList&, Qt::KeyboardModifiers)>;
    using DropHandler = std::function<bool(QTreeWidgetItem*, const QStringList&, Qt::DropAction)>;

    explicit CategoryTreeWidget(QWidget* parent = nullptr)
        : QTreeWidget(parent)
    {
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setDragDropMode(QAbstractItemView::DropOnly);
        setDefaultDropAction(Qt::CopyAction);
        setItemsExpandable(true);
        setRootIsDecorated(true);
    }

    void setCategoryDropHandler(DropHandler handler)
    {
        dropHandler_ = std::move(handler);
    }

    void setCategoryDropActionResolver(DropActionResolver resolver)
    {
        dropActionResolver_ = std::move(resolver);
    }

protected:
    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override
    {
        if (!index.isValid())
        {
            return;
        }

        if (!index.parent().isValid())
        {
            if (!model() || !model()->hasChildren(index))
            {
                return;
            }

            QStyleOption option;
            option.initFrom(this);
            option.rect = QRect(rect.left(), rect.top(), indentation(), rect.height());
            option.state |= QStyle::State_Enabled | QStyle::State_Item | QStyle::State_Children;
            if (isExpanded(index))
            {
                option.state |= QStyle::State_Open;
            }
            if (index.row() + 1 < model()->rowCount(index.parent()))
            {
                option.state |= QStyle::State_Sibling;
            }
            style()->drawPrimitive(QStyle::PE_IndicatorBranch, &option, painter, this);
            return;
        }

        QTreeWidget::drawBranches(painter, rect, index);
    }

    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (event && event->mimeData()->hasFormat(kCategoryAssignmentMimeType))
        {
            event->setDropAction(Qt::CopyAction);
            event->accept();
            return;
        }
        QTreeWidget::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        if (event && event->mimeData()->hasFormat(kCategoryAssignmentMimeType))
        {
            QTreeWidgetItem* item = nullptr;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            item = categoryDropTargetAt(event->position().toPoint());
#else
            item = categoryDropTargetAt(event->pos());
#endif
            if (item)
            {
                const QStringList names = decodeCategoryAssignmentPayload(event->mimeData());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                const Qt::KeyboardModifiers modifiers = event->modifiers();
#else
                const Qt::KeyboardModifiers modifiers = event->keyboardModifiers();
#endif
                const Qt::DropAction action = categoryDropAction(item, names, modifiers);
                if (action != Qt::IgnoreAction)
                {
                    event->setDropAction(action);
                    event->accept();
                }
                else
                {
                    event->ignore();
                }
            }
            else
            {
                event->ignore();
            }
            return;
        }
        QTreeWidget::dragMoveEvent(event);
    }

    void dropEvent(QDropEvent* event) override
    {
        if (!event || !event->mimeData()->hasFormat(kCategoryAssignmentMimeType))
        {
            QTreeWidget::dropEvent(event);
            return;
        }

        QTreeWidgetItem* item = nullptr;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        item = categoryDropTargetAt(event->position().toPoint());
#else
        item = categoryDropTargetAt(event->pos());
#endif
        const QStringList names = decodeCategoryAssignmentPayload(event->mimeData());
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const Qt::KeyboardModifiers modifiers = event->modifiers();
#else
        const Qt::KeyboardModifiers modifiers = event->keyboardModifiers();
#endif
        const Qt::DropAction action = categoryDropAction(item, names, modifiers);
        if (dropHandler_ && item && !names.isEmpty() && action != Qt::IgnoreAction && dropHandler_(item, names, action))
        {
            event->setDropAction(action);
            event->accept();
            return;
        }

        event->ignore();
    }

private:
    Qt::DropAction categoryDropAction(
        QTreeWidgetItem* item,
        const QStringList& names,
        const Qt::KeyboardModifiers modifiers) const
    {
        if (!item || names.isEmpty())
        {
            return Qt::IgnoreAction;
        }

        if (dropActionResolver_)
        {
            return dropActionResolver_(item, names, modifiers);
        }
        return (modifiers & Qt::ControlModifier) ? Qt::MoveAction : Qt::CopyAction;
    }

    QTreeWidgetItem* categoryDropTargetAt(const QPoint& pos) const
    {
        if (QTreeWidgetItem* direct = itemAt(pos))
        {
            return direct;
        }

        if (viewport() && !viewport()->rect().contains(pos))
        {
            const QPoint viewportPos = viewport()->mapFrom(this, pos);
            if (viewport()->rect().contains(viewportPos))
            {
                if (QTreeWidgetItem* mapped = itemAt(viewportPos))
                {
                    return mapped;
                }
                return categoryVisibleRowAtY(viewportPos.y());
            }
            return nullptr;
        }

        return categoryVisibleRowAtY(pos.y());
    }

    QTreeWidgetItem* categoryVisibleRowAtY(const int y) const
    {
        std::function<QTreeWidgetItem*(QTreeWidgetItem*)> findInSubtree =
            [&](QTreeWidgetItem* item) -> QTreeWidgetItem*
        {
            if (!item || item->isHidden())
            {
                return nullptr;
            }

            const QRect rect = visualItemRect(item);
            if (rect.isValid() && y >= rect.top() && y <= rect.bottom())
            {
                return item;
            }

            if (!item->isExpanded())
            {
                return nullptr;
            }

            for (int i = 0; i < item->childCount(); ++i)
            {
                if (QTreeWidgetItem* child = findInSubtree(item->child(i)))
                {
                    return child;
                }
            }
            return nullptr;
        };

        for (int i = 0; i < topLevelItemCount(); ++i)
        {
            if (QTreeWidgetItem* item = findInSubtree(topLevelItem(i)))
            {
                return item;
            }
        }
        return nullptr;
    }

    DropHandler dropHandler_{};
    DropActionResolver dropActionResolver_{};
};

} // namespace sed::ui::detail
