#pragma once

#include <QLayout>
#include <QLayoutItem>
#include <QMargins>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QWidget>

#include <algorithm>
#include <vector>

namespace sed::ui
{

/**
 * Wraps child layout items across multiple rows using each item's size hint.
 *
 * Qt does not provide a built-in flow layout for compact tool rows. This helper
 * keeps dense button groups readable when a dock or dialog is resized.
 */
class FlowLayout final : public QLayout
{
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = 0, int hSpacing = 4, int vSpacing = 4)
        : QLayout(parent)
        , hSpace_(hSpacing)
        , vSpace_(vSpacing)
    {
        setContentsMargins(margin, margin, margin, margin);
    }

    ~FlowLayout() override
    {
        QLayoutItem* item = nullptr;
        while ((item = takeAt(0)) != nullptr)
        {
            delete item;
        }
    }

    void addItem(QLayoutItem* item) override
    {
        items_.push_back(item);
    }

    int count() const override
    {
        return static_cast<int>(items_.size());
    }

    QLayoutItem* itemAt(const int index) const override
    {
        if (index < 0 || static_cast<std::size_t>(index) >= items_.size())
        {
            return nullptr;
        }
        return items_[static_cast<std::size_t>(index)];
    }

    QLayoutItem* takeAt(const int index) override
    {
        if (index < 0 || static_cast<std::size_t>(index) >= items_.size())
        {
            return nullptr;
        }

        QLayoutItem* item = items_[static_cast<std::size_t>(index)];
        items_.erase(items_.begin() + index);
        return item;
    }

    Qt::Orientations expandingDirections() const override
    {
        return {};
    }

    bool hasHeightForWidth() const override
    {
        return true;
    }

    int heightForWidth(const int width) const override
    {
        return doLayout(QRect(0, 0, width, 0), true);
    }

    QSize minimumSize() const override
    {
        QSize size{};
        for (QLayoutItem* item : items_)
        {
            size = size.expandedTo(item->minimumSize());
        }

        const QMargins margins = contentsMargins();
        size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
        return size;
    }

    QSize sizeHint() const override
    {
        return minimumSize();
    }

    void setGeometry(const QRect& rect) override
    {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

private:
    /**
     * Places items left-to-right and wraps to a new row when the next item
     * would exceed the effective layout width.
     */
    int doLayout(const QRect& rect, const bool testOnly) const
    {
        const QMargins margins = contentsMargins();
        const QRect effectiveRect = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
        int x = effectiveRect.x();
        int y = effectiveRect.y();
        int lineHeight = 0;

        for (QLayoutItem* item : items_)
        {
            const QSize itemSize = item->sizeHint();
            const int nextX = x + itemSize.width() + hSpace_;
            if (lineHeight > 0 && nextX - hSpace_ > effectiveRect.right() + 1)
            {
                x = effectiveRect.x();
                y += lineHeight + vSpace_;
                lineHeight = 0;
            }

            if (!testOnly)
            {
                item->setGeometry(QRect(QPoint(x, y), itemSize));
            }

            x += itemSize.width() + hSpace_;
            lineHeight = std::max(lineHeight, itemSize.height());
        }

        return (y + lineHeight + margins.bottom()) - rect.y();
    }

    std::vector<QLayoutItem*> items_{};
    int hSpace_{4};
    int vSpace_{4};
};

} // namespace sed::ui
