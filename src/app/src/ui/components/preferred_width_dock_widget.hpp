#pragma once

#include <algorithm>

#include <QDockWidget>
#include <QSize>

namespace sed::ui
{

/**
 * Dock widget with a configurable horizontal size hint and ordinary flexible
 * minimum/maximum constraints.
 *
 * QMainWindow otherwise initializes a newly revealed split dock at the sum of
 * its minimum widths. A preferred size hint gives the dock area useful initial
 * space without preventing the user from moving either splitter afterward.
 */
class PreferredWidthDockWidget final : public QDockWidget
{
public:
    explicit PreferredWidthDockWidget(const QString& title, QWidget* parent = nullptr)
        : QDockWidget(title, parent)
    {
    }

    void setPreferredWidth(const int width)
    {
        const int clampedWidth = std::max(0, width);
        if (preferredWidth_ == clampedWidth)
        {
            return;
        }

        preferredWidth_ = clampedWidth;
        updateGeometry();
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        QSize hint = QDockWidget::sizeHint();
        hint.setWidth(std::max(hint.width(), preferredWidth_));
        return hint;
    }

private:
    int preferredWidth_{};
};

} // namespace sed::ui
