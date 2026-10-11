#pragma once

#include "ui/shortcuts/key_sequence_display.hpp"

class QAction;
class QMouseEvent;
class QResizeEvent;
class QStyleOptionMenuItem;
class QString;

namespace sed::ui
{

class ViewportCameraMenu final : public shortcuts::KeySequenceDisplayMenu
{
public:
    explicit ViewportCameraMenu(QWidget* parent = nullptr);
    ViewportCameraMenu(const QString& title, QWidget* parent = nullptr);

protected:
    void initStyleOption(QStyleOptionMenuItem* option, const QAction* action) const override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
};

} // namespace sed::ui
