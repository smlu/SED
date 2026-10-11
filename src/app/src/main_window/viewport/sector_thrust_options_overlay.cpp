#include "main_window/common.hpp"

namespace sed
{

void MainWindow::setupSectorThrustOptionsOverlay()
{
    if (!glView_ || !viewportSectorThrustAction_)
    {
        return;
    }

    sectorThrustOptionsOverlay_ = new QWidget(glView_);
    sectorThrustOptionsOverlay_->setAttribute(Qt::WA_StyledBackground, true);
    sectorThrustOptionsOverlay_->setObjectName("sectorThrustOptionsOverlay");

    auto* layout = new QHBoxLayout(sectorThrustOptionsOverlay_);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    auto* title = new QLabel(QStringLiteral("Sector Thrust"), sectorThrustOptionsOverlay_);
    title->setObjectName("sectorThrustOptionsLabel");
    layout->addWidget(title);

    auto* separator = new QFrame(sectorThrustOptionsOverlay_);
    separator->setFrameShape(QFrame::VLine);
    separator->setObjectName("sectorThrustOptionsSeparator");
    layout->addWidget(separator);

    viewportSectorThrustShowAllButton_ = new QToolButton(sectorThrustOptionsOverlay_);
    viewportSectorThrustShowAllButton_->setObjectName("sectorThrustShowAllButton");
    viewportSectorThrustShowAllButton_->setText(QStringLiteral("Show all"));
    viewportSectorThrustShowAllButton_->setCheckable(true);
    viewportSectorThrustShowAllButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    viewportSectorThrustShowAllButton_->setAutoRaise(true);
    viewportSectorThrustShowAllButton_->setCursor(Qt::PointingHandCursor);
    viewportSectorThrustShowAllButton_->setAccessibleName(QStringLiteral("Show all sector thrust gizmos"));
    const bool showAll = settingsStore().value("sector_thrust/show_all_gizmos", true).toBool();
    viewportSectorThrustShowAllButton_->setChecked(showAll);
    layout->addWidget(viewportSectorThrustShowAllButton_);

    const auto updateDescription = [](QToolButton* button, const bool enabled)
    {
        if (!button)
        {
            return;
        }
        const QString tooltip = enabled
            ? QStringLiteral("<b>Show all thrust</b><br>Show handles for every visible, unlocked Sector, including sectors without thrust (zero-vector handles).")
            : QStringLiteral("<b>Show all thrust</b><br>Only sectors with thrust are shown on visible, unlocked layers. Turn this on to include sectors without thrust (zero-vector handles).");
        const QString accessibleDescription = enabled
            ? QStringLiteral("Showing thrust gizmos for all sectors, including sectors without thrust.")
            : QStringLiteral("Only sectors with thrust are shown. Enable Show all to include sectors without thrust.");
        button->setToolTip(tooltip);
        button->setAccessibleDescription(accessibleDescription);
    };
    updateDescription(viewportSectorThrustShowAllButton_, showAll);
    glView_->setShowAllSectorThrustGizmos(showAll);

    connect(viewportSectorThrustShowAllButton_, &QToolButton::toggled, this,
        [this, updateDescription](const bool checked)
        {
            settingsStore().setValue("sector_thrust/show_all_gizmos", checked);
            updateDescription(viewportSectorThrustShowAllButton_, checked);
            if (glView_)
            {
                glView_->setShowAllSectorThrustGizmos(checked);
            }
        });
    connect(viewportSectorThrustAction_, &QAction::toggled, this,
        [this](const bool)
        {
            syncSectorThrustOptionsOverlay();
        });

    sectorThrustOptionsOverlay_->setStyleSheet(
        "QWidget#sectorThrustOptionsOverlay {"
        "  background: rgba(18, 18, 22, 235);"
        "  border: 1px solid rgba(255, 255, 255, 40);"
        "  border-radius: 8px;"
        "}"
        "QLabel#sectorThrustOptionsLabel {"
        "  color: rgba(230, 232, 238, 220);"
        "  padding-left: 2px;"
        "}"
        "QFrame#sectorThrustOptionsSeparator {"
        "  color: rgba(255, 255, 255, 32);"
        "}"
        "QToolButton#sectorThrustShowAllButton {"
        "  color: rgba(236, 238, 242, 245);"
        "  background: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 5px;"
        "  padding: 3px 8px;"
        "}"
        "QToolButton#sectorThrustShowAllButton:hover {"
        "  background: rgba(255, 255, 255, 24);"
        "}"
        "QToolButton#sectorThrustShowAllButton:checked {"
        "  background: rgba(90, 145, 220, 105);"
        "  border-color: rgba(150, 195, 255, 90);"
        "}");

    sectorThrustOptionsOverlay_->hide();
    syncSectorThrustOptionsOverlay();
}

void MainWindow::positionSectorThrustOptionsOverlay()
{
    if (!glView_ || !sectorThrustOptionsOverlay_)
    {
        return;
    }

    sectorThrustOptionsOverlay_->adjustSize();
    const int margin = 10;
    int x = std::max(margin, (glView_->width() - sectorThrustOptionsOverlay_->width()) / 2);
    int y = margin;

    if (transformMoveOptionsOverlay_ && transformMoveOptionsOverlay_->isVisible())
    {
        y = transformMoveOptionsOverlay_->geometry().bottom() + 1 + margin;
    }
    if (viewportOverlay_ && viewportOverlay_->isVisible())
    {
        const QRect optionRect(x, y, sectorThrustOptionsOverlay_->width(), sectorThrustOptionsOverlay_->height());
        const QRect avoidRect = viewportOverlay_->geometry().adjusted(-margin, -margin, margin, margin);
        if (optionRect.intersects(avoidRect))
        {
            x = std::max(margin, avoidRect.left() - sectorThrustOptionsOverlay_->width() - margin);
        }
    }

    sectorThrustOptionsOverlay_->move(x, y);
    reflowTopCenterOverlays();
}

void MainWindow::syncSectorThrustOptionsOverlay()
{
    if (!sectorThrustOptionsOverlay_)
    {
        return;
    }

    const bool active = glView_
        && glView_->renderPipeline() == GlView::RenderPipeline::Editor
        && glView_->sectorThrustToolEnabled()
        && viewportSectorThrustAction_
        && viewportSectorThrustAction_->isChecked()
        && editMode_ == EditMode::sector
        && document_.level().kind == ProjectType::IJIM;
    sectorThrustOptionsOverlay_->setVisible(active);
    if (!active)
    {
        return;
    }

    sectorThrustOptionsOverlay_->adjustSize();
    positionSectorThrustOptionsOverlay();
    sectorThrustOptionsOverlay_->raise();
}

} // namespace sed
