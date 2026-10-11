#include "app_icon.hpp"
#include "main_window/common.hpp"
#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "ui/components/desktop_input_policy.hpp"
#include "ui/components/dialog_window_policy.hpp"
#include "ui/components/spin_scrub_utils.hpp"
#include "ui/cog/cog_instance_picker.hpp"
#include "ui/resources/resource_picker.hpp"
#include "ui/resources/resource_picker_kind_utils.hpp"
#include "ui/resources/template_resource_utils.hpp"
#include "ui/shortcuts/shortcut_context_utils.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"
#include "ui/viewport/thing_placement_drop_utils.hpp"
#include "ui/viewport/thing_placement_panel_resize_utils.hpp"
#include "ui/viewport/thing_placement_wheel_utils.hpp"

#include <QFontDatabase>
#include <QFileDialog>
#include <QAbstractSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QStyleOptionTitleBar>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>

namespace sed
{

namespace
{

int minimumWidthForMessageBoxCaption(const QMessageBox& box)
{
    if (box.windowTitle().isEmpty() || !box.style())
    {
        return 0;
    }

    // Native captions use the platform title font and reserve space for the
    // window icon and caption buttons. Measuring with the message-body font (or
    // merely resizing the client layout) can therefore still leave an elided
    // title on Windows.
    const QFont titleFont = QFontDatabase::systemFont(QFontDatabase::TitleFont);
    const QFontMetrics titleMetrics(titleFont);
    const int titleTextWidth = titleMetrics.horizontalAdvance(box.windowTitle()) + 12;
    const int titleBarHeight = std::max(
        box.style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, &box),
        titleMetrics.height() + 8);
    const int titleButtonWidth = std::max(
        titleBarHeight,
        box.style()->pixelMetric(QStyle::PM_TitleBarButtonSize, nullptr, &box));
    const int iconWidth = std::max(
        titleBarHeight,
        box.style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, &box));

    // Editor message boxes use an explicit title-and-close frame.
    int candidateWidth = titleTextWidth + iconWidth + titleButtonWidth + 32;

    QStyleOptionTitleBar option{};
    option.initFrom(&box);
    option.rect = QRect(0, 0, candidateWidth, titleBarHeight);
    option.text = box.windowTitle();
    option.icon = box.windowIcon();
    option.titleBarFlags = box.windowFlags();
    option.titleBarState = static_cast<int>(box.windowState());
    const QRect labelRect = box.style()->subControlRect(
        QStyle::CC_TitleBar,
        &option,
        QStyle::SC_TitleBarLabel,
        &box);
    if (labelRect.isValid() && labelRect.width() < titleTextWidth)
    {
        candidateWidth += titleTextWidth - labelRect.width();
    }
    return candidateWidth;
}

bool sameCogValuePickerSnapshot(
    const CogValue& current,
    const CogValue& snapshot)
{
    return current.name == snapshot.name
        && current.type == snapshot.type
        && current.intValue == snapshot.intValue
        && current.floatValue == snapshot.floatValue
        && current.stringValue == snapshot.stringValue
        && current.cogType == snapshot.cogType
        && current.local == snapshot.local
        && current.description == snapshot.description
        && current.mask == snapshot.mask;
}

bool sameEditorSelectionSnapshot(
    const editor::Selection& current,
    const editor::Selection& snapshot)
{
    const auto sameSet = [](const auto& left, const auto& right)
    {
        return left.size() == right.size()
            && std::all_of(right.begin(), right.end(), [&left](const auto& item)
            {
                return left.find(item) != left.end();
            });
    };
    return sameSet(current.sectors, snapshot.sectors)
        && sameSet(current.surfaces, snapshot.surfaces)
        && sameSet(current.surfaceVertices, snapshot.surfaceVertices)
        && sameSet(current.things, snapshot.things)
        && sameSet(current.frames, snapshot.frames)
        && sameSet(current.lights, snapshot.lights)
        && sameSet(current.cogs, snapshot.cogs);
}

void syncTopLevelWindowChrome(QObject* watched, QEvent* event)
{
    if (!watched || !event
        || (event->type() != QEvent::Show && event->type() != QEvent::WinIdChange))
    {
        return;
    }

    auto* window = qobject_cast<QWidget*>(watched);
    if (!window || !window->isWindow())
    {
        return;
    }

    // Standard windows, dialogs, and detached tool docks all own native captions.
    switch (window->windowType())
    {
        case Qt::Window:
        case Qt::Dialog:
        case Qt::Tool:
        case Qt::Sheet:
        case Qt::Drawer:
            break;
        default:
            return;
    }

    const QColor titleBarColor = window->objectName() == QStringLiteral("startup_dialog")
        ? ui::applicationCanvasColor()
        : ui::applicationChromeColor();
    applyWindowsTitleBarColor(*window, titleBarColor);
}

void syncApplicationDialogPolicy(QObject* watched, QEvent* event)
{
    if (!event || event->type() != QEvent::Polish)
    {
        return;
    }
    auto* dialog = qobject_cast<QDialog*>(watched);
    if (!dialog || !dialog->isWindow()
        || qobject_cast<QMessageBox*>(dialog)
        || qobject_cast<QFileDialog*>(dialog)
        || dialog->windowType() == Qt::Popup
        || dialog->property("sed.allowMinMaxButtons").toBool())
    {
        return;
    }
    if (!dialog->isVisible())
    {
        ui::applyDialogWindowPolicy(*dialog);
        return;
    }

    // A style or native child can deliver a late Polish after the modal is
    // already visible. Calling any Qt or native window-flag mutator from that
    // event can hide the dialog and end QDialog::exec(). Caption flags must be
    // finalized by the ordinary pre-show Polish above; a late Polish may only
    // perform non-destructive client-geometry work.
    dialog->setMinimumWidth(std::max(
        dialog->minimumWidth(),
        ui::minimumDialogWidthForCaption(*dialog)));
}

} // namespace

ui::viewport::PanelDockSide MainWindow::thingPlacementPanelDockSideForPosition(
    const ThingPlacementDockPosition position) const noexcept
{
    switch (position)
    {
        case ThingPlacementDockPosition::left:   return ui::viewport::PanelDockSide::left;
        case ThingPlacementDockPosition::right:  return ui::viewport::PanelDockSide::right;
        case ThingPlacementDockPosition::top:    return ui::viewport::PanelDockSide::top;
        case ThingPlacementDockPosition::bottom:
        default:                                 return ui::viewport::PanelDockSide::bottom;
    }
}

ui::viewport::PanelDockSide MainWindow::thingPlacementPanelDockSide() const noexcept
{
    return thingPlacementPanelDockSideForPosition(thingPlacementDockPosition_);
}

MainWindow::ThingPlacementDockPosition MainWindow::thingPlacementDockPositionForPanelDockSide(
    const ui::viewport::PanelDockSide dockSide) const noexcept
{
    switch (dockSide)
    {
        case ui::viewport::PanelDockSide::left:   return ThingPlacementDockPosition::left;
        case ui::viewport::PanelDockSide::right:  return ThingPlacementDockPosition::right;
        case ui::viewport::PanelDockSide::top:    return ThingPlacementDockPosition::top;
        case ui::viewport::PanelDockSide::bottom:
        default:                                  return ThingPlacementDockPosition::bottom;
    }
}

std::optional<MainWindow::ThingPlacementDockPosition> MainWindow::thingPlacementDockDropTarget(
    const QPoint& globalPos) const
{
    if (!glView_)
    {
        return std::nullopt;
    }

    // Step 1: convert from global cursor coordinates into viewport-local geometry.
    const QRect localBounds(QPoint(0, 0), glView_->size());
    const QPoint localPos = glView_->mapFromGlobal(globalPos);

    // Step 2: let the shared helper resolve nearest-edge behavior.
    const std::optional<ui::viewport::PanelDockSide> side =
        ui::viewport::panelDockSideForLocalPoint(localBounds, localPos);
    if (!side)
    {
        return std::nullopt;
    }
    return thingPlacementDockPositionForPanelDockSide(*side);
}

QRect MainWindow::thingPlacementDockPreviewRect(const ThingPlacementDockPosition position) const
{
    if (!glView_)
    {
        return {};
    }

    return ui::viewport::thingPlacementDockPreviewRect(
        QRect(QPoint(0, 0), glView_->size()),
        thingPlacementPanelDockSideForPosition(position));
}

std::optional<MainWindow::ThingPlacementDockPosition> MainWindow::thingPlacementDockPositionForDragDelta(
    const QPoint& delta) const noexcept
{
    const std::optional<ui::viewport::PanelDockSide> side =
        ui::viewport::panelDockSideForDragDelta(delta);
    if (!side)
    {
        return std::nullopt;
    }
    return thingPlacementDockPositionForPanelDockSide(*side);
}

MainWindow::UvEditorDockPosition MainWindow::uvEditorDockPositionForPanelDockSide(
    const ui::viewport::PanelDockSide dockSide) const noexcept
{
    switch (dockSide)
    {
        case ui::viewport::PanelDockSide::left:   return UvEditorDockPosition::left;
        case ui::viewport::PanelDockSide::right:  return UvEditorDockPosition::right;
        case ui::viewport::PanelDockSide::top:    return UvEditorDockPosition::top;
        case ui::viewport::PanelDockSide::bottom:
        default:                                  return UvEditorDockPosition::bottom;
    }
}

ui::viewport::PanelDockSide MainWindow::uvEditorPanelDockSideForPosition(
    const UvEditorDockPosition position) const noexcept
{
    switch (position)
    {
        case UvEditorDockPosition::left:   return ui::viewport::PanelDockSide::left;
        case UvEditorDockPosition::right:  return ui::viewport::PanelDockSide::right;
        case UvEditorDockPosition::top:    return ui::viewport::PanelDockSide::top;
        case UvEditorDockPosition::bottom:
        default:                           return ui::viewport::PanelDockSide::bottom;
    }
}

std::optional<MainWindow::UvEditorDockPosition> MainWindow::uvEditorDockDropTarget(
    const QPoint& globalPos) const
{
    if (!viewportUvSplitter_)
    {
        return std::nullopt;
    }

    // Step 1: convert from global cursor coordinates into splitter-local geometry.
    const QRect localBounds(QPoint(0, 0), viewportUvSplitter_->size());
    const QPoint localPos = viewportUvSplitter_->mapFromGlobal(globalPos);

    // Step 2: reuse the shared edge-distance dock resolver.
    const std::optional<ui::viewport::PanelDockSide> side =
        ui::viewport::panelDockSideForLocalPoint(localBounds, localPos);
    if (!side)
    {
        return std::nullopt;
    }
    return uvEditorDockPositionForPanelDockSide(*side);
}

QRect MainWindow::uvEditorDockPreviewRect(const UvEditorDockPosition position) const
{
    if (!viewportUvSplitter_)
    {
        return {};
    }

    return ui::viewport::uvEditorDockPreviewRect(
        QRect(QPoint(0, 0), viewportUvSplitter_->size()),
        uvEditorPanelDockSideForPosition(position));
}

std::optional<MainWindow::UvEditorDockPosition> MainWindow::uvEditorDockPositionForDragDelta(
    const QPoint& delta) const noexcept
{
    const std::optional<ui::viewport::PanelDockSide> side =
        ui::viewport::panelDockSideForDragDelta(delta);
    if (!side)
    {
        return std::nullopt;
    }
    return uvEditorDockPositionForPanelDockSide(*side);
}

bool MainWindow::thingPlacementPanelResizeEdgeHit(const QPoint& globalPos) const
{
    if (!thingPlacementPanel_ || !thingPlacementPanel_->isVisible())
    {
        return false;
    }

    const QRect globalPanelRect(
        thingPlacementPanel_->mapToGlobal(QPoint(0, 0)),
        thingPlacementPanel_->size());
    return ui::viewport::thingPlacementPanelResizeEdgeHit(
        globalPanelRect,
        globalPos,
        thingPlacementPanelDockSide());
}

void MainWindow::applyThingPlacementPanelResize(const QPoint& globalPos)
{
    if (!glView_)
    {
        return;
    }

    // Step 1: choose the axis and viewport extent from the current dock side.
    const ui::viewport::PanelDockSide dockSide = thingPlacementPanelDockSide();
    const bool sideDock = ui::viewport::isSideDock(dockSide);
    const int viewportExtent = sideDock ? glView_->width() : glView_->height();

    // Step 2: apply the tested size math and store it in the matching panel dimension.
    const int nextSize = ui::viewport::resizedThingPlacementPanelSize(
        dockSide,
        viewportExtent,
        thingPlacementResizeStartSize_,
        thingPlacementResizeStartGlobal_,
        globalPos);
    if (sideDock)
    {
        thingPlacementPanelSideSize_ = nextSize;
    }
    else
    {
        thingPlacementPanelBandSize_ = nextSize;
    }

    // Step 3: refresh panel geometry and list layout after the stored size changes.
    positionThingPlacementPanel();
    if (thingPlacementTemplateList_)
    {
        thingPlacementTemplateList_->doItemsLayout();
    }
}

void MainWindow::applyThingPlacementFilterResize(const QPoint& globalPos)
{
    // Step 1: compute and store the clamped filter height from the vertical drag.
    thingPlacementFilterContentHeight_ = ui::viewport::resizedThingPlacementFilterHeight(
        thingPlacementFilterResizeStartHeight_,
        thingPlacementFilterResizeStartGlobal_,
        globalPos);

    // Step 2: notify every dependent widget so the panel can settle immediately.
    if (thingPlacementCategoryList_)
    {
        thingPlacementCategoryList_->setMaximumHeight(thingPlacementFilterContentHeight_);
        thingPlacementCategoryList_->updateGeometry();
    }
    if (thingPlacementFilterContent_)
    {
        thingPlacementFilterContent_->updateGeometry();
    }
    if (thingPlacementFilterHost_)
    {
        thingPlacementFilterHost_->updateGeometry();
    }
    if (thingPlacementPanel_)
    {
        thingPlacementPanel_->updateGeometry();
    }
    if (thingPlacementTemplateList_)
    {
        thingPlacementTemplateList_->doItemsLayout();
    }
}

std::optional<QPoint> MainWindow::thingPlacementViewportPosForDropEvent(
    QObject* watched,
    const QDropEvent* dropEvent) const
{
    const QWidget* targetWidget = qobject_cast<QWidget*>(watched);
    if (!dropEvent || !targetWidget || !glView_)
    {
        return std::nullopt;
    }

    // Step 1: convert from the watched widget's drop coordinates to global space.
    const QPoint targetPos = dropEvent->position().toPoint();
    const QPoint globalPos = targetWidget->mapToGlobal(targetPos);

    // Step 2: ignore drags that are still above the template panel or its collapsed bar.
    if (ui::viewport::visibleWidgetContainsGlobalPoint(thingPlacementPanel_, globalPos)
        || ui::viewport::visibleWidgetContainsGlobalPoint(thingPlacementCollapsedBar_, globalPos))
    {
        return std::nullopt;
    }

    // Step 3: map the global point into the viewport and require it to land inside GlView.
    const QPoint viewportPos = glView_->mapFromGlobal(globalPos);
    if (!glView_->rect().contains(viewportPos))
    {
        return std::nullopt;
    }
    return viewportPos;
}

bool MainWindow::activateThingTemplatePlacementFromDrop(const QString& templateName)
{
    const QString trimmed = templateName.trimmed();
    if (trimmed.isEmpty())
    {
        return false;
    }

    // Step 1: reset per-template yaw state only when the user dragged a different template.
    const std::string nextName = trimmed.toStdString();
    if (selectedThingTemplateName_ != nextName)
    {
        thingPlacementYawOffset_ = 0.0;
    }
    selectedThingTemplateName_ = nextName;

    // Step 2: make thing placement active, or refresh its panel if it was already active.
    if (placementMode_ != PlacementMode::thing)
    {
        setPlacementMode(PlacementMode::thing);
    }
    else
    {
        syncThingPlacementPanel();
    }
    return true;
}

bool MainWindow::handleThingPlacementTemplateDropEvent(QObject* watched, QEvent* event)
{
    if (!glView_ || !event || !ui::viewport::isThingPlacementTemplateDropEvent(event->type()))
    {
        return false;
    }

    // Step 1: drag-enter only validates the payload and viewport target.
    if (event->type() == QEvent::DragEnter)
    {
        auto* dragEvent = static_cast<QDragEnterEvent*>(event);
        if (thingPlacementViewportPosForDropEvent(watched, dragEvent)
            && activateThingTemplatePlacementFromDrop(
                ui::viewport::thingTemplateNameFromMime(dragEvent->mimeData())))
        {
            dragEvent->acceptProposedAction();
            return true;
        }
        return false;
    }

    // Step 2: drag-move keeps the preview under the cursor while the payload is valid.
    if (event->type() == QEvent::DragMove)
    {
        auto* dragEvent = static_cast<QDragMoveEvent*>(event);
        const std::optional<QPoint> viewportPos = thingPlacementViewportPosForDropEvent(watched, dragEvent);
        if (viewportPos
            && activateThingTemplatePlacementFromDrop(
                ui::viewport::thingTemplateNameFromMime(dragEvent->mimeData())))
        {
            updateThingPlacementPreview(*viewportPos, dragEvent->modifiers());
            dragEvent->acceptProposedAction();
            return true;
        }
        return false;
    }

    // Step 3: drop commits the thing at the resolved placement target when one is available.
    auto* dropEvent = static_cast<QDropEvent*>(event);
    const QString templateName = ui::viewport::thingTemplateNameFromMime(dropEvent->mimeData());
    if (templateName.isEmpty() || !activateThingTemplatePlacementFromDrop(templateName))
    {
        return false;
    }

    const std::optional<QPoint> viewportPos = thingPlacementViewportPosForDropEvent(watched, dropEvent);
    if (!viewportPos)
    {
        return false;
    }

    if (const std::optional<ThingPlacementTarget> target =
            thingPlacementTargetForViewport(*viewportPos, dropEvent->modifiers()))
    {
        (void)commitThingPlacementAt(
            target->position,
            target->haveSurfaceHit || target->havePlacementTarget,
            target->surfaceNormal,
            dropEvent->modifiers(),
            target->sectorFallback,
            &*viewportPos, &target->documentPosition);
    }
    dropEvent->acceptProposedAction();
    return true;
}

std::optional<bool> MainWindow::handleThingPlacementSpinScrubEvent(QObject* watched, QEvent* event)
{
    if (!event)
    {
        return std::nullopt;
    }

    QAbstractSpinBox* spin = ui::findSpinScrubBox(watched);
    const bool mousePress = event->type() == QEvent::MouseButtonPress;
    const bool mouseMove = event->type() == QEvent::MouseMove;
    const bool mouseRelease = event->type() == QEvent::MouseButtonRelease;
    auto* mouseEvent = (mousePress || mouseMove || mouseRelease)
        ? static_cast<QMouseEvent*>(event)
        : nullptr;
    const bool matchingSpin = spin && thingPlacementSpinScrubBox_ == spin;
    const int dx = (mouseEvent && matchingSpin)
        ? mouseEvent->globalPosition().toPoint().x() - thingPlacementSpinScrubStartX_
        : 0;

    const ui::SpinScrubEventContext context{
        .objectMarked         = ui::objectHasSpinScrubMarker(watched),
        .spinAvailable        = spin != nullptr,
        .spinEnabled          = spin && spin->isEnabled(),
        .mousePress           = mousePress,
        .mouseMove            = mouseMove,
        .mouseRelease         = mouseRelease,
        .leftButton           = mouseEvent && mouseEvent->button() == Qt::LeftButton,
        .leftButtonHeld       = mouseEvent && mouseEvent->buttons().testFlag(Qt::LeftButton),
        .matchingSpin         = matchingSpin,
        .pending              = thingPlacementSpinScrubPending_,
        .active               = thingPlacementSpinScrubActive_,
        .startThresholdPassed = ui::spinScrubShouldStart(dx),
    };

    auto clearScrubState = [this]()
    {
        thingPlacementSpinScrubPending_ = false;
        thingPlacementSpinScrubActive_ = false;
        thingPlacementSpinScrubBox_.clear();
    };

    auto applyScrubValue = [this, spin, dx]()
    {
        const double nextValue = ui::spinScrubNextValue(
            thingPlacementSpinScrubStartValue_,
            dx,
            ui::spinScrubStep(spin),
            QApplication::keyboardModifiers());
        ui::setSpinScrubValue(spin, nextValue);
    };

    switch (ui::spinScrubEventRoute(context))
    {
        case ui::SpinScrubEventRoute::beginPending:
            thingPlacementSpinScrubBox_ = spin;
            thingPlacementSpinScrubPending_ = true;
            thingPlacementSpinScrubActive_ = false;
            thingPlacementSpinScrubStartX_ = mouseEvent->globalPosition().toPoint().x();
            thingPlacementSpinScrubStartValue_ = ui::spinScrubValue(spin);
            return false;
        case ui::SpinScrubEventRoute::keepPending:
            return false;
        case ui::SpinScrubEventRoute::startAndApply:
            thingPlacementSpinScrubActive_ = true;
            thingPlacementSpinScrubPending_ = false;
            QApplication::setOverrideCursor(Qt::SizeHorCursor);
            applyScrubValue();
            return true;
        case ui::SpinScrubEventRoute::apply:
            applyScrubValue();
            return true;
        case ui::SpinScrubEventRoute::cancelActive:
            QApplication::restoreOverrideCursor();
            clearScrubState();
            return false;
        case ui::SpinScrubEventRoute::cancelInactive:
            clearScrubState();
            return false;
        case ui::SpinScrubEventRoute::finishActive:
            QApplication::restoreOverrideCursor();
            clearScrubState();
            return true;
        case ui::SpinScrubEventRoute::finishInactive:
            clearScrubState();
            return false;
        case ui::SpinScrubEventRoute::none:
            return std::nullopt;
    }

    return std::nullopt;
}

void MainWindow::cancelThingPlacementHeaderDrag()
{
    thingPlacementHeaderDragActive_ = false;
    if (thingPlacementDockPreviewBand_)
    {
        thingPlacementDockPreviewBand_->hide();
    }
    QApplication::restoreOverrideCursor();
}

void MainWindow::updateThingPlacementHeaderDragPreview(const QPoint& globalPos)
{
    // A press remains a click until it crosses Qt's platform drag distance.
    const QPoint delta = globalPos - thingPlacementHeaderDragStartGlobal_;
    if (!ui::viewport::panelHeaderDragExceedsThreshold(delta, QApplication::startDragDistance()))
    {
        if (thingPlacementDockPreviewBand_)
        {
            thingPlacementDockPreviewBand_->hide();
        }
        return;
    }

    // Step 1: show the rubber-band preview when the cursor is over a dock target.
    if (const std::optional<ThingPlacementDockPosition> target = thingPlacementDockDropTarget(globalPos))
    {
        if (thingPlacementDockPreviewBand_)
        {
            thingPlacementDockPreviewBand_->setGeometry(thingPlacementDockPreviewRect(*target));
            thingPlacementDockPreviewBand_->show();
            thingPlacementDockPreviewBand_->raise();
        }
        return;
    }

    // Step 2: no target means the user is still dragging freely, so hide the preview.
    if (thingPlacementDockPreviewBand_)
    {
        thingPlacementDockPreviewBand_->hide();
    }
}

void MainWindow::finishThingPlacementHeaderDrag(const QPoint& releaseGlobal)
{
    const QPoint delta = releaseGlobal - thingPlacementHeaderDragStartGlobal_;
    cancelThingPlacementHeaderDrag();

    // Header clicks must not be interpreted as a drop onto the nearest edge.
    if (!ui::viewport::panelHeaderDragExceedsThreshold(delta, QApplication::startDragDistance()))
    {
        return;
    }

    // Step 1: prefer the explicit edge currently under the released cursor.
    if (const std::optional<ThingPlacementDockPosition> target = thingPlacementDockDropTarget(releaseGlobal))
    {
        applyThingPlacementDockPosition(*target);
        return;
    }

    // Step 2: fall back to drag direction when the release point is away from an edge.
    if (const std::optional<ThingPlacementDockPosition> target = thingPlacementDockPositionForDragDelta(delta))
    {
        applyThingPlacementDockPosition(*target);
    }
}

void MainWindow::cancelUvEditorPanelHeaderDrag()
{
    uvEditorPanelHeaderDragActive_ = false;
    uvEditorPanelDockPreviewVisible_ = false;
    if (uvEditorPanelDockPreviewBand_)
    {
        uvEditorPanelDockPreviewBand_->hide();
    }
    QApplication::restoreOverrideCursor();
}

void MainWindow::updateUvEditorPanelHeaderDragPreview(const QPoint& globalPos)
{
    const QPoint delta = globalPos - uvEditorPanelHeaderDragStartGlobal_;
    if (!ui::viewport::panelHeaderDragExceedsThreshold(delta, QApplication::startDragDistance()))
    {
        uvEditorPanelDockPreviewVisible_ = false;
        if (uvEditorPanelDockPreviewBand_)
        {
            uvEditorPanelDockPreviewBand_->hide();
        }
        return;
    }

    // Step 1: store and show the hover target so paint/update code sees the same state.
    if (const std::optional<UvEditorDockPosition> target = uvEditorDockDropTarget(globalPos))
    {
        uvEditorPanelDockPreviewPosition_ = *target;
        uvEditorPanelDockPreviewVisible_ = true;
        if (uvEditorPanelDockPreviewBand_)
        {
            QRect previewGeometry = uvEditorDockPreviewRect(*target);
            if (QWidget* previewParent =
                    uvEditorPanelDockPreviewBand_->parentWidget();
                previewParent && previewParent != viewportUvSplitter_)
            {
                previewGeometry.moveTopLeft(viewportUvSplitter_->mapTo(
                    previewParent, previewGeometry.topLeft()));
            }
            uvEditorPanelDockPreviewBand_->setGeometry(previewGeometry);
            uvEditorPanelDockPreviewBand_->show();
            uvEditorPanelDockPreviewBand_->raise();
        }
        return;
    }

    // Step 2: leaving the target area clears both the stored state and rubber band.
    uvEditorPanelDockPreviewVisible_ = false;
    if (uvEditorPanelDockPreviewBand_)
    {
        uvEditorPanelDockPreviewBand_->hide();
    }
}

void MainWindow::finishUvEditorPanelHeaderDrag(const QPoint& releaseGlobal)
{
    const QPoint delta = releaseGlobal - uvEditorPanelHeaderDragStartGlobal_;
    cancelUvEditorPanelHeaderDrag();

    if (!ui::viewport::panelHeaderDragExceedsThreshold(delta, QApplication::startDragDistance()))
    {
        return;
    }

    // Step 1: use the release edge when the cursor is over a valid dock target.
    if (const std::optional<UvEditorDockPosition> target = uvEditorDockDropTarget(releaseGlobal))
    {
        applyUvEditorDockPosition(*target);
        return;
    }

    // Step 2: otherwise resolve the intended edge from the completed drag vector.
    if (const std::optional<UvEditorDockPosition> target = uvEditorDockPositionForDragDelta(delta))
    {
        applyUvEditorDockPosition(*target);
    }
}

void MainWindow::polishMessageBoxForEventFilter(QObject* watched, QEvent* event)
{
    if (!event || (event->type() != QEvent::Show && event->type() != QEvent::Polish && event->type() != QEvent::LayoutRequest))
    {
        return;
    }

    auto* box = qobject_cast<QMessageBox*>(watched);
    if (!box)
    {
        return;
    }
    if (box->property("sed.messageBoxPolishing").toBool())
    {
        return;
    }
    box->setProperty("sed.messageBoxPolishing", true);

    // Step 1: request an explicit dialog frame. Merely clearing minimize and
    // maximize hints leaves their native caption slots/buttons present on some
    // Windows styles, just as it did for the About dialog.
    if (event->type() == QEvent::Polish)
    {
        box->setWindowFlags(
            Qt::Dialog
            | Qt::CustomizeWindowHint
            | Qt::WindowTitleHint
            | Qt::WindowCloseButtonHint
            | Qt::MSWindowsFixedSizeDialogHint);
    }

    // Step 2: keep a one-line primary message vertically aligned with its icon.
    // QMessageBox otherwise gives the label only its text height, which leaves it
    // pinned to the top of the much taller icon row on some platform styles.
    QLabel* primaryLabel = nullptr;
    const QList<QLabel*> labels = box->findChildren<QLabel*>();
    for (QLabel* label : labels)
    {
        if (label && label->text() == box->text())
        {
            primaryLabel = label;
            break;
        }
    }
    if (!primaryLabel)
    {
        primaryLabel = box->findChild<QLabel*>(QStringLiteral("qt_msgbox_label"));
    }
    if (primaryLabel)
    {
        primaryLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        const bool needsWrapping = box->text().contains(QLatin1Char('\n'))
            || box->text().size() > 80;
        primaryLabel->setWordWrap(needsWrapping);
        if (needsWrapping)
        {
            primaryLabel->setMinimumWidth(430);
            primaryLabel->setMaximumWidth(650);
            primaryLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        }
        if (box->icon() != QMessageBox::NoIcon && box->informativeText().isEmpty())
        {
            const int iconSize = box->style()->pixelMetric(QStyle::PM_MessageBoxIconSize, nullptr, box);
            primaryLabel->setMinimumHeight(std::max(primaryLabel->minimumHeight(), iconSize));
        }
    }

    if (QLabel* informative = box->findChild<QLabel*>(
            QStringLiteral("qt_msgbox_informativelabel")))
    {
        informative->setWordWrap(true);
        informative->setMinimumWidth(430);
        informative->setMaximumWidth(650);
        informative->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }

    // Step 3: force a fresh layout pass before measuring the preferred message size.
    box->ensurePolished();
    if (QLayout* layout = box->layout())
    {
        layout->invalidate();
        layout->activate();
    }

    // Step 4: reset any size fixed by an earlier LayoutRequest, then measure
    // the complete wrapped text. Fix the resulting size so the user cannot
    // resize a transient message box, while still allowing a later text/layout
    // change to recompute both width and height.
    box->setMinimumSize(0, 0);
    box->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    QSize preferredSize = box->sizeHint().expandedTo(box->minimumSizeHint());
    if (const QLayout* layout = box->layout())
    {
        preferredSize = preferredSize.expandedTo(layout->sizeHint());
    }
    const int targetWidth = std::max({
        480,
        preferredSize.width(),
        minimumWidthForMessageBoxCaption(*box),
    });
    const int targetHeight = std::max(160, preferredSize.height());
    box->setFixedSize(targetWidth, targetHeight);
    box->setProperty("sed.messageBoxPolishing", false);
}

void MainWindow::syncDockWidthsForEventFilterResize(QObject* watched, QEvent* event)
{
    if (!event
        || event->type() != QEvent::Resize
        || (watched != outlinerDock_ && watched != cogScriptEditorDock_)
        || !outlinerDock_
        || !cogScriptEditorDock_)
    {
        return;
    }

    // Step 1: ignore resize notifications generated by our own topology changes.
    if (adjustingCogDockWidths_)
    {
        return;
    }

    // Step 2: the outliner belongs to the outer window. Remember its own drag
    // without consulting or resizing the COG dock in the nested workspace.
    if (watched == outlinerDock_)
    {
        if (!suppressStandaloneOutlinerWidthCapture_)
        {
            standaloneOutlinerPreferredWidth_ =
                std::max(kMinimumOutlinerDockWidth, outlinerDock_->width());
        }
        // Record the user size, but do not mutate sizeHint from inside a
        // resize notification. That emits a nested layout request which can
        // fight the splitter drag and pull the embedded workspace/COG dock.
        // Topology restoration applies the recorded hint explicitly.
        updateCogScriptDockWidthConstraints();
        return;
    }

    // Step 3: the COG dock remembers only its nested-workspace width.
    if (watched == cogScriptEditorDock_ && cogScriptEditorDock_->width() > 0)
    {
        cogScriptDockPreferredWidth_ =
            std::max(kMinimumCogScriptDockWidth, cogScriptEditorDock_->width());
        cogScriptDockWidthLearned_ = true;
        // The nested dock's live geometry is already authoritative. Updating
        // its preferred size here propagates a LayoutRequest through the
        // embedded QMainWindow and can resize the outer Outliner. Apply this
        // recorded width only when the dock is shown/redocked/restored.
    }
}

void MainWindow::handleThingPlacementTemplateListBackgroundPress(QObject* watched, QEvent* event)
{
    if (!thingPlacementTemplateList_
        || watched != thingPlacementTemplateList_->viewport()
        || !event
        || event->type() != QEvent::MouseButtonPress)
    {
        return;
    }

    // Step 1: a left click on empty list space clears the selected placement template.
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton
        && !thingPlacementTemplateList_->itemAt(mouseEvent->position().toPoint()))
    {
        deselectThingPlacementTemplate();
    }
}

std::optional<bool> MainWindow::handleThingPlacementWheelEvent(QObject* watched, QEvent* event)
{
    if (!event || event->type() != QEvent::Wheel)
    {
        return std::nullopt;
    }

    auto* wheelEvent = static_cast<QWheelEvent*>(event);
    const bool watchedViewport = watched == glView_;
    const bool pointerInsidePanel =
        glView_ && watchedViewport && thingPlacementPointerInsidePanel();
    const Qt::KeyboardModifiers mods = wheelEvent->modifiers();
    const ui::viewport::ThingPlacementWheelContext wheelContext{
        .viewportAvailable   = glView_ != nullptr,
        .watchedViewport     = watchedViewport,
        .wheelEvent          = true,
        .pointerInsidePanel  = pointerInsidePanel,
        .thingPlacementMode  = placementMode_ == PlacementMode::thing,
        .templateSelected    = !selectedThingTemplateName_.empty(),
        .flyNavigationActive = glView_ && glView_->flyNavigationActive(),
        .yawShortcut         = ui::viewport::isThingPlacementYawWheelShortcut(mods),
    };

    switch (ui::viewport::thingPlacementWheelRoute(wheelContext))
    {
        case ui::viewport::ThingPlacementWheelRoute::consumePanelWheel:
            event->accept();
            return true;
        case ui::viewport::ThingPlacementWheelRoute::rotatePlacementYaw:
        {
            // Step 1: normalize mouse-wheel or touchpad deltas into logical yaw steps.
            const double steps = ui::viewport::thingPlacementWheelSteps(
                wheelEvent->angleDelta(),
                wheelEvent->pixelDelta());

            // Step 2: apply meaningful yaw changes and refresh the preview under the cursor.
            if (ui::viewport::hasThingPlacementWheelSteps(steps))
            {
                thingPlacementYawOffset_ =
                    ui::viewport::updatedThingPlacementYaw(thingPlacementYawOffset_, steps);

                const Qt::KeyboardModifiers previewModifiers =
                    ui::viewport::thingPlacementYawPreviewModifiers(mods);
                updateThingPlacementPreview(
                    glView_->mapFromGlobal(QCursor::pos()),
                    previewModifiers);
                glView_->showToast(
                    QString("Placement yaw: %1 deg").arg(thingPlacementYawOffset_, 0, 'f', 0),
                    900,
                    GlView::ToastPlacement::BottomCenter,
                    GlView::ToastGroup::PlacementYaw);
            }

            wheelEvent->accept();
            return true;
        }
        case ui::viewport::ThingPlacementWheelRoute::none:
            return std::nullopt;
    }

    return std::nullopt;
}

std::optional<bool> MainWindow::handleThingPlacementPanelResizeEvent(QObject* watched, QEvent* event)
{
    if (!thingPlacementPanel_ || !event)
    {
        return std::nullopt;
    }

    auto* watchedWidget = qobject_cast<QWidget*>(watched);
    const bool inThingPanel = ui::viewport::widgetInsidePanel(watchedWidget, thingPlacementPanel_);
    const bool mousePress = event->type() == QEvent::MouseButtonPress;
    const bool mouseMove = event->type() == QEvent::MouseMove;
    const bool mouseRelease = event->type() == QEvent::MouseButtonRelease;
    auto* mouseEvent = (mousePress || mouseMove || mouseRelease)
        ? static_cast<QMouseEvent*>(event)
        : nullptr;
    const QPoint globalPos = mouseEvent ? mouseEvent->globalPosition().toPoint() : QPoint{};
    const bool resizeEdgeHit = mouseEvent && thingPlacementPanelResizeEdgeHit(globalPos);
    const bool headerDragWidget = watched && watched->property("sed.thingPlacementHeaderDrag").toBool();

    const ui::viewport::ThingPlacementPanelEventContext panelEventContext{
        .panelAvailable          = thingPlacementPanel_ != nullptr,
        .eventAvailable          = event != nullptr,
        .filterResizeActive      = thingPlacementFilterResizeActive_,
        .panelResizeActive       = thingPlacementResizeActive_,
        .insidePanel             = inThingPanel,
        .mousePress              = mousePress,
        .mouseMove               = mouseMove,
        .mouseRelease            = mouseRelease,
        .leave                   = event->type() == QEvent::Leave,
        .leftButton              = mouseEvent && mouseEvent->button() == Qt::LeftButton,
        .noButtons               = mouseEvent && mouseEvent->buttons() == Qt::NoButton,
        .resizeEdgeHit           = resizeEdgeHit,
        .watchedFilterResizeGrip = watched == thingPlacementFilterResizeGrip_,
        .headerDragWidget        = headerDragWidget,
    };

    switch (ui::viewport::thingPlacementPanelEventRoute(panelEventContext))
    {
        case ui::viewport::ThingPlacementPanelEventRoute::applyFilterResize:
            applyThingPlacementFilterResize(globalPos);
            return true;
        case ui::viewport::ThingPlacementPanelEventRoute::finishFilterResize:
            applyThingPlacementFilterResize(globalPos);
            thingPlacementFilterResizeActive_ = false;
            QApplication::restoreOverrideCursor();
            saveThingPlacementOptions();
            return true;
        case ui::viewport::ThingPlacementPanelEventRoute::applyPanelResize:
            applyThingPlacementPanelResize(globalPos);
            return true;
        case ui::viewport::ThingPlacementPanelEventRoute::finishPanelResize:
            applyThingPlacementPanelResize(globalPos);
            thingPlacementResizeActive_ = false;
            QApplication::restoreOverrideCursor();
            saveThingPlacementOptions();
            return true;
        case ui::viewport::ThingPlacementPanelEventRoute::beginFilterResize:
            thingPlacementFilterResizeActive_ = true;
            thingPlacementFilterResizeStartGlobal_ = globalPos;
            thingPlacementFilterResizeStartHeight_ = thingPlacementFilterContentHeight_;
            QApplication::setOverrideCursor(Qt::SizeVerCursor);
            return true;
        case ui::viewport::ThingPlacementPanelEventRoute::beginPanelResize:
        {
            thingPlacementResizeActive_ = true;
            thingPlacementResizeStartGlobal_ = globalPos;
            const ui::viewport::PanelDockSide dockSide = thingPlacementPanelDockSide();
            const bool sideDock = ui::viewport::isSideDock(dockSide);
            thingPlacementResizeStartSize_ = sideDock ? thingPlacementPanel_->width() : thingPlacementPanel_->height();
            QApplication::setOverrideCursor(ui::viewport::panelResizeCursor(dockSide));
            return true;
        }
        case ui::viewport::ThingPlacementPanelEventRoute::showResizeCursor:
            watchedWidget->setCursor(ui::viewport::panelResizeCursor(thingPlacementPanelDockSide()));
            return std::nullopt;
        case ui::viewport::ThingPlacementPanelEventRoute::clearCursor:
            watchedWidget->unsetCursor();
            return std::nullopt;
        case ui::viewport::ThingPlacementPanelEventRoute::none:
            return std::nullopt;
    }

    return std::nullopt;
}

std::optional<bool> MainWindow::handleGlobalShortcutKeyEvent(QObject* watched, QEvent* event)
{
    if (!event
        || (event->type() != QEvent::ShortcutOverride
            && event->type() != QEvent::KeyPress
            && event->type() != QEvent::KeyRelease))
    {
        return std::nullopt;
    }

    auto* keyEvent = static_cast<QKeyEvent*>(event);
    const bool pressed = event->type() == QEvent::KeyPress;

    const int key = keyEvent ? keyEvent->key() : 0;
    const auto modifiers = keyEvent ? keyEvent->modifiers() : Qt::NoModifier;
    auto* watchedWidget = qobject_cast<QWidget*>(watched);
    const bool inViewport = ui::viewport::widgetInsidePanel(watchedWidget, glView_);
    QWidget* focusWidget = QApplication::focusWidget();
    QWidget* nativeTextEditor = ui::shortcuts::focusedNativeTextEditor(focusWidget);
    if (!nativeTextEditor)
    {
        nativeTextEditor = ui::shortcuts::focusedNativeTextEditor(watchedWidget);
    }
    QWidget* const nativeTextFocusOwner = focusWidget ? focusWidget : watchedWidget;
    const bool nativeTextCompositeContext = nativeTextEditor
        && nativeTextEditor != nativeTextFocusOwner
        && qobject_cast<QLineEdit*>(nativeTextEditor);
    const bool cogEditorFocusContext = [this, focusWidget, watchedWidget]() -> bool
    {
        const auto belongsToOpenCogEditor = [this](QWidget* widget) -> bool
        {
            for (QWidget* current = widget; current; current = current->parentWidget())
            {
                for (const auto& [_, ptr] : openCogScriptEditors_)
                {
                    if (ptr && ptr.data() == current)
                    {
                        return true;
                    }
                }
            }
            return false;
        };

        // QApplication::focusWidget() can be temporarily null while a dock is
        // being activated or moved.  The key event receiver remains an exact,
        // trustworthy COG-editor descendant in that interval, so do not let
        // Ctrl+S fall through to the level Save action.
        return cogScriptEditorOwnsFocus()
            || belongsToOpenCogEditor(focusWidget)
            || belongsToOpenCogEditor(watchedWidget);
    }();
    const bool resourceEditorFocusContext =
        [this, focusWidget, watchedWidget]() -> bool
    {
        if ( resourceEditorOwnsFocus() )
        {
            return true;
        }
        const auto belongsToResourceEditor = [this](QWidget* widget) -> bool
        {
            for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
            {
                if ( widget && host.dock && host.tabs
                    && host.sessionGeneration == resourceEditorDocumentContextGeneration_
                    && (host.dock.data() == widget || host.dock->isAncestorOf(widget)) )
                {
                    return true;
                }
            }
            return false;
        };
        return belongsToResourceEditor(focusWidget)
            || belongsToResourceEditor(watchedWidget);
    }();
    const bool viewportFocusContext = ui::viewport::widgetInsidePanel(focusWidget, glView_);
    const bool viewportPointerContext = glView_ && glView_->underMouse();
    const bool textEntryFocusContext = ui::shortcuts::focusAcceptsTextInput(
        focusWidget ? focusWidget : watchedWidget);
    const bool nativeTextFocusContext = nativeTextEditor != nullptr;
    QAction* cogEditorCommand = nullptr;
    bool cogContextInsertBinding = false;
    if ( cogEditorFocusContext && !QApplication::activeModalWidget()
        && !QApplication::activePopupWidget() )
    {
        for ( QAction* action : cogEditorFocusActions_ )
        {
            if ( nativeTextFocusContext && action
                && action->objectName() == QStringLiteral("cog_editor.insert_context")
                && ui::shortcuts::actionMatchesShortcut(action, key, modifiers, false) )
            {
                cogContextInsertBinding = true;
            }
            if ( action && action->isEnabled()
                && ui::shortcuts::actionMatchesShortcut(action, key, modifiers, false) )
            {
                cogEditorCommand = action;
                break;
            }
        }
    }
    const bool viewportKeyShortcutContext =
        ui::shortcuts::shouldDispatchViewportShortcut(inViewport, textEntryFocusContext);
    const bool uvPanelVisible =
        uvEditorPanelHost_
        && uvEditorPanelHost_->isVisible()
        && uvEditorPanelExpanded_;
    const bool uvFocusContext =
        uvPanelVisible
        && ui::viewport::widgetInsidePanel(focusWidget, uvEditorPanelHost_);
    const bool uvCanvasPointerContext = [this, uvPanelVisible]() -> bool
    {
        if (!uvPanelVisible)
        {
            return false;
        }
        const QWidget* canvas = uvEditorPanelHost_->findChild<QWidget*>(
            QStringLiteral("uvEditorCanvas"));
        return ui::viewport::visibleWidgetContainsGlobalPoint(canvas, QCursor::pos());
    }();
    const bool lightAddBindingMatches = editMode_ == EditMode::light
        && ui::shortcuts::actionMatchesShortcut(
            addLightAction_, key, modifiers, false);
    const bool sectorAddBindingMatches = editMode_ == EditMode::sector
        && ui::shortcuts::actionMatchesShortcut(
            addCubeAction_, key, modifiers, false);
    const bool thingAddBindingMatches = editMode_ == EditMode::thing
        && ui::shortcuts::actionMatchesShortcut(
            addThingAction_, key, modifiers, false);
    const bool modeAddBindingMatches = lightAddBindingMatches
        || sectorAddBindingMatches || thingAddBindingMatches;
    const bool addViewportShortcutContext = viewportKeyShortcutContext
        && !resourceEditorFocusContext
        && !cogEditorFocusContext
        && !textEntryFocusContext
        && !nativeTextFocusContext
        && !uvFocusContext
        && !uvCanvasPointerContext;
    const bool lightViewportShortcutContext = addViewportShortcutContext;
    const bool uvOwnedViewportShortcutContext = viewportKeyShortcutContext
        && !resourceEditorFocusContext
        && !cogEditorFocusContext
        && !textEntryFocusContext
        && !nativeTextFocusContext
        && (uvFocusContext || uvCanvasPointerContext);
    const bool addShortcutOverrideContext =
        addViewportShortcutContext && modeAddBindingMatches;
    const bool lightForceDeleteBindingMatches = editMode_ == EditMode::light
        && ui::shortcuts::actionMatchesShortcut(
            forceDeleteSurfaceVertexAction_, key, modifiers, false);
    const bool lightDeleteBindingMatches = editMode_ == EditMode::light
        && deleteAction_
        && ui::shortcuts::actionMatchesShortcut(deleteAction_, key, modifiers, false)
        && !lightForceDeleteBindingMatches;
    if (glView_ && glView_->flyNavigationActive()
        && (resourceEditorFocusContext || cogEditorFocusContext
            || textEntryFocusContext || nativeTextFocusContext || uvFocusContext))
    {
        // A focused editor owns its keys even if the platform missed the
        // viewport FocusOut or mouse release during a dock/focus handoff.
        glView_->cancelViewportMouseGesture();
        flyHeldKeys_.clear();
    }
    const bool bareTransformShortcut = modifiers == Qt::NoModifier
        && (key == Qt::Key_G || key == Qt::Key_R || key == Qt::Key_S);
    const bool pointerShortcutAllowed = !resourceEditorFocusContext
        && !cogEditorFocusContext && !textEntryFocusContext && !nativeTextFocusContext
        && !(glView_ && glView_->flyNavigationActive());
    const bool viewportPointerShortcutContext = pointerShortcutAllowed
        && ui::viewport::visibleWidgetContainsGlobalPoint(glView_, QCursor::pos())
        && !uvCanvasPointerContext;
    const bool uvTransformShortcutContext = documentEditingAllowed()
        && pointerShortcutAllowed && uvCanvasPointerContext && bareTransformShortcut;
    QAction* const gridAction = findChild<QAction*>(QStringLiteral("viewport.show_grid"));
    const bool cleaveOwnsInput = documentEditingAllowed() && glView_
        && glView_->isStagedCleaveInteractionActive();
    const bool cleaveLocalShortcut = cleaveOwnsInput
        && glView_->stagedCleaveTransformShortcutForKey(key, modifiers).has_value();
    const auto matchesGlobalTransform = [key, modifiers](const QAction* action)
    {
        return ui::shortcuts::actionMatchesShortcut(action, key, modifiers, false);
    };
    const bool cleaveProtectedShortcut = cleaveOwnsInput
        && (matchesGlobalTransform(thingToolSelectKeyAction_)
            || matchesGlobalTransform(thingToolMoveKeyAction_)
            || matchesGlobalTransform(thingToolRotateKeyAction_)
            || matchesGlobalTransform(thingToolScaleKeyAction_)
            || matchesGlobalTransform(gridAction));
    const bool cleaveTransformShortcutContext = pointerShortcutAllowed
        && (viewportKeyShortcutContext || viewportPointerShortcutContext)
        && !uvCanvasPointerContext
        && (cleaveLocalShortcut || cleaveProtectedShortcut);
    const bool cleaveBlockedByUvPointer = pointerShortcutAllowed
        && uvCanvasPointerContext && (cleaveLocalShortcut || cleaveProtectedShortcut);
    const bool cleaveModifierShortcutContext = cleaveOwnsInput && pointerShortcutAllowed
        && (viewportKeyShortcutContext || viewportPointerShortcutContext)
        && !uvCanvasPointerContext && ui::shortcuts::isEditorModifierKey(key);
    const bool gridVisibilityShortcutContext = pointerShortcutAllowed
        && (viewportKeyShortcutContext || viewportPointerShortcutContext)
        && !uvCanvasPointerContext && matchesGlobalTransform(gridAction);

    // Step 1: RMB fly navigation owns all keyboard input. Claim every shortcut
    // before QAction dispatch so editor commands and held-modifier previews
    // cannot leak through while the camera is being flown.
    if (event->type() == QEvent::ShortcutOverride)
    {
        if ( resourceEditorFocusContext )
        {
            if ( ui::shortcuts::isEditorSaveShortcutKey(key, modifiers) )
            {
                keyEvent->accept();
                return true;
            }
            if ( ui::shortcuts::actionMatchesShortcut(undoAction_, key, modifiers, false)
                || ui::shortcuts::actionMatchesShortcut(redoAction_, key, modifiers, false) )
            {
                keyEvent->accept();
                return true;
            }

            const std::array<QAction*, 8> protectedWorldActions{
                undoAction_,
                redoAction_,
                deleteAction_,
                forceDeleteSurfaceVertexAction_,
                viewportFrameDeleteAction_,
                translateAction_,
                rotateAction_,
                scaleAction_,
            };
            const bool matchesWorldShortcut = std::any_of(
                protectedWorldActions.begin(),
                protectedWorldActions.end(),
                [key, modifiers](const QAction* action)
                {
                    return action
                        && action->isEnabled()
                        && ui::shortcuts::actionMatchesShortcut(
                            action, key, modifiers, false);
                });
            if ( matchesWorldShortcut )
            {
                keyEvent->accept();
                return true;
            }
        }

        // Qt resolves QAction/QShortcut accelerators during ShortcutOverride,
        // before a KeyPress reaches this filter.  Claim COG Ctrl+S here so
        // the level's file.save QAction cannot win the accelerator race.  We
        // only consume the override; the actual save is performed once by
        // triggerActiveCogScriptEditorSaveForKeyState on the following
        // non-repeated KeyPress.
        if (cogEditorFocusContext
            && !keyEvent->isAutoRepeat()
            && ui::shortcuts::isEditorSaveShortcutKey(key, modifiers))
        {
            keyEvent->accept();
            return true;
        }
        if ( cogEditorFocusContext
            && (ui::shortcuts::actionMatchesShortcut(undoAction_, key, modifiers, false)
                || ui::shortcuts::actionMatchesShortcut(redoAction_, key, modifiers, false)) )
        {
            keyEvent->accept();
            return true;
        }
        if ( cogEditorCommand || cogContextInsertBinding
            || (nativeTextFocusContext && key == Qt::Key_Insert
                && modifiers == Qt::NoModifier) )
        {
            // Focused COG commands resolve their live keymap bindings before
            // Qt's global accelerator map. Bare native Insert stays local even
            // when no COG construct can be inserted at the source caret.
            keyEvent->accept();
            return true;
        }
        if (glView_ && glView_->flyNavigationActive())
        {
            keyEvent->accept();
            return true;
        }
        if ( addShortcutOverrideContext )
        {
            // Claim the active mode's Add binding before Qt resolves its menu
            // QAction, then execute that real action once from KeyPress.
            keyEvent->accept();
            return true;
        }
        if ( lightViewportShortcutContext && lightDeleteBindingMatches )
        {
            // Keep canonical Del in Light mode on the deterministic viewport
            // path, including disabled and repeat cases. Shift+Del remains
            // owned by the guarded force-delete command above.
            keyEvent->accept();
            return true;
        }
        if ( uvTransformShortcutContext || cleaveTransformShortcutContext
            || cleaveBlockedByUvPointer || cleaveModifierShortcutContext
            || gridVisibilityShortcutContext )
        {
            // Claim the accelerator before a viewport QAction can change the
            // owning tool. The corresponding KeyPress dispatches it once.
            keyEvent->accept();
            return true;
        }
        if ( nativeTextFocusContext
            && (ui::shortcuts::actionMatchesShortcut(undoAction_, key, modifiers, false)
                || ui::shortcuts::actionMatchesShortcut(redoAction_, key, modifiers, false)) )
        {
            keyEvent->accept();
            return true;
        }
        if ( !resourceEditorFocusContext
            && !cogEditorFocusContext
            && !textEntryFocusContext
            && !nativeTextFocusContext
            && glView_
            && glView_->stagedCleaveHistoryOwnsUndo()
            && (ui::shortcuts::actionMatchesShortcut(undoAction_, key, modifiers, false)
                || ui::shortcuts::actionMatchesShortcut(redoAction_, key, modifiers, false)) )
        {
            // The draft owns these bindings even when one direction is at an
            // end of its local timeline and its QAction is disabled.
            keyEvent->accept();
            return true;
        }
        // Qt can otherwise resolve Delete and Shift+Delete through different
        // platform standard-key aliases before the viewport KeyPress reaches
        // our deterministic dispatcher. Claim the guarded force-delete
        // binding here, then execute it exactly once from the KeyPress branch
        // below (the same ownership pattern used for COG Ctrl+S).
        if (viewportKeyShortcutContext
            && forceDeleteSurfaceVertexAction_
            && forceDeleteSurfaceVertexAction_->isEnabled()
            && !keyEvent->isAutoRepeat()
            && ui::shortcuts::actionMatchesShortcut(
                forceDeleteSurfaceVertexAction_, key, modifiers, false))
        {
            keyEvent->accept();
            return true;
        }
        const ui::shortcuts::FlyNavigationShortcutBindings flyBindings{
            flyFastAction_ ? flyFastAction_->shortcuts() : QList<QKeySequence>{},
            flyForwardAction_ ? flyForwardAction_->shortcuts() : QList<QKeySequence>{},
            flyBackwardAction_ ? flyBackwardAction_->shortcuts() : QList<QKeySequence>{},
            flyLeftAction_ ? flyLeftAction_->shortcuts() : QList<QKeySequence>{},
            flyRightAction_ ? flyRightAction_->shortcuts() : QList<QKeySequence>{},
            flyDownAction_ ? flyDownAction_->shortcuts() : QList<QKeySequence>{},
            flyUpAction_ ? flyUpAction_->shortcuts() : QList<QKeySequence>{},
            flySlowAction_ ? flySlowAction_->shortcuts() : QList<QKeySequence>{},
            flySpeedUpAction_ ? flySpeedUpAction_->shortcuts() : QList<QKeySequence>{},
            flySpeedDownAction_ ? flySpeedDownAction_->shortcuts() : QList<QKeySequence>{},
        };
        const ui::shortcuts::FlyNavigationDispatchContext context{
            .viewportAvailable     = glView_ != nullptr,
            .flyNavigationActive   = glView_ && glView_->flyNavigationActive(),
            .keyEventAvailable     = keyEvent != nullptr,
            .autoRepeat            = keyEvent && keyEvent->isAutoRepeat(),
            .textEntryFocusContext = textEntryFocusContext,
        };
        if (ui::shortcuts::shouldClaimFlyNavigationShortcutOverride(
                context, flyBindings, key, modifiers))
        {
            keyEvent->accept();
            return true;
        }
        return std::nullopt;
    }

    if ( resourceEditorFocusContext && event->type() == QEvent::KeyPress )
    {
        for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
        {
            if ( watchedWidget && host.dock && host.tabs
                && host.sessionGeneration == resourceEditorDocumentContextGeneration_
                && (host.dock.data() == watchedWidget || host.dock->isAncestorOf(watchedWidget)) )
            {
                setProperty("resource_authoring.lastActiveHostId", host.stableId);
                break;
            }
        }
        if ( ui::shortcuts::isEditorSaveShortcutKey(key, modifiers) )
        {
            if ( !keyEvent->isAutoRepeat() )
            {
                const bool saveAs = modifiers.testFlag(Qt::ShiftModifier);
                (void)triggerActiveResourceEditorSave(saveAs);
            }
            keyEvent->accept();
            return true;
        }
        const bool undo = ui::shortcuts::actionMatchesShortcut(undoAction_, key, modifiers, false);
        const bool redo = ui::shortcuts::actionMatchesShortcut(redoAction_, key, modifiers, false);
        if ( (undo || redo) && nativeTextFocusContext )
        {
            if ( !keyEvent->isAutoRepeat() )
            {
                if ( nativeTextCompositeContext
                    || !triggerActiveResourceEditorUndoRedo(redo) )
                {
                    (void)ui::shortcuts::triggerNativeTextEditorUndoRedo(
                        nativeTextEditor, redo);
                }
            }
            keyEvent->accept();
            return true;
        }
        if ( (undo || redo) && !keyEvent->isAutoRepeat() && !textEntryFocusContext
            && triggerActiveResourceEditorUndoRedo(redo) )
        {
            keyEvent->accept();
            return true;
        }

        const std::array<QAction*, 8> protectedWorldActions{
            undoAction_,
            redoAction_,
            deleteAction_,
            forceDeleteSurfaceVertexAction_,
            viewportFrameDeleteAction_,
            translateAction_,
            rotateAction_,
            scaleAction_,
        };
        const bool matchesWorldShortcut = std::any_of(
            protectedWorldActions.begin(),
            protectedWorldActions.end(),
            [key, modifiers](const QAction* action)
            {
                return action
                    && action->isEnabled()
                    && ui::shortcuts::actionMatchesShortcut(
                        action, key, modifiers, false);
            });
        if ( matchesWorldShortcut )
        {
            // The matching ShortcutOverride above prevents a MainWindow QAction
            // from firing, while false lets a resource cell editor handle its
            // own text undo/delete input.
            return false;
        }
    }

    if ( cogEditorFocusContext && event->type() == QEvent::KeyPress )
    {
        if ( cogEditorCommand )
        {
            if ( !keyEvent->isAutoRepeat() )
            {
                cogEditorCommand->trigger();
            }
            keyEvent->accept();
            return true;
        }
        const bool undo = ui::shortcuts::actionMatchesShortcut(undoAction_, key, modifiers, false);
        const bool redo = ui::shortcuts::actionMatchesShortcut(redoAction_, key, modifiers, false);
        if ( undo || redo )
        {
            if ( nativeTextFocusContext )
            {
                if ( !keyEvent->isAutoRepeat() )
                {
                    if ( nativeTextCompositeContext
                        || !triggerActiveCogScriptEditorUndoRedo(redo) )
                    {
                        (void)ui::shortcuts::triggerNativeTextEditorUndoRedo(
                            nativeTextEditor, redo);
                    }
                }
                keyEvent->accept();
                return true;
            }
            if ( !keyEvent->isAutoRepeat() )
            {
                (void)triggerActiveCogScriptEditorUndoRedo(redo);
            }
            keyEvent->accept();
            return true;
        }
    }

    if (glView_ && glView_->flyNavigationActive())
    {
        (void)dispatchFlyNavigationShortcut(key, modifiers, pressed);
        keyEvent->accept();
        return true;
    }

    if ( (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        && addViewportShortcutContext
        && modeAddBindingMatches )
    {
        if ( pressed && !keyEvent->isAutoRepeat() )
        {
            // dispatchEditorShortcut routes the active Add action and consumes
            // disabled/no-op Light submodes without repeating a toggle.
            (void)dispatchEditorShortcut(key, modifiers);
        }
        keyEvent->accept();
        return true;
    }

    if ( (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
        && lightViewportShortcutContext
        && lightDeleteBindingMatches )
    {
        if ( pressed && !keyEvent->isAutoRepeat() )
        {
            (void)dispatchEditorShortcut(key, modifiers);
        }
        keyEvent->accept();
        return true;
    }

    if ( nativeTextFocusContext && event->type() == QEvent::KeyPress )
    {
        const bool undo = ui::shortcuts::actionMatchesShortcut(
            undoAction_, key, modifiers, false);
        const bool redo = ui::shortcuts::actionMatchesShortcut(
            redoAction_, key, modifiers, false);
        if ( undo || redo )
        {
            if ( !keyEvent->isAutoRepeat() )
            {
                (void)ui::shortcuts::triggerNativeTextEditorUndoRedo(
                    nativeTextEditor, redo);
            }
            keyEvent->accept();
            return true;
        }
        if ( !keyEvent->isAutoRepeat() )
        {
            QTimer::singleShot(
                0,
                this,
                &MainWindow::syncCogScriptEditorActionFocus);
        }
    }

    if ( !resourceEditorFocusContext
        && !cogEditorFocusContext
        && !textEntryFocusContext
        && !nativeTextFocusContext
        && glView_
        && glView_->stagedCleaveHistoryOwnsUndo()
        && event->type() == QEvent::KeyPress )
    {
        const bool undo = ui::shortcuts::actionMatchesShortcut(
            undoAction_, key, modifiers, false);
        const bool redo = ui::shortcuts::actionMatchesShortcut(
            redoAction_, key, modifiers, false);
        if ( undo || redo )
        {
            if ( !keyEvent->isAutoRepeat() )
            {
                QAction* const action = redo ? redoAction_ : undoAction_;
                if ( action && action->isEnabled() )
                {
                    action->trigger();
                }
            }
            keyEvent->accept();
            return true;
        }
    }

    // Step 2: modifier changes update an active transform immediately, even
    // while the mouse is stationary. Mouse-move events keep supplying their
    // own modifier snapshot during the rest of the drag.
    if (glView_
        && keyEvent
        && !keyEvent->isAutoRepeat()
        && ui::shortcuts::isEditorModifierKey(key))
    {
        const Qt::KeyboardModifiers held = ui::shortcuts::modifiersWithKeyState(
            modifiers & ui::shortcuts::editorModifierMask(),
            key,
            pressed);
        glView_->refreshTransformDragForModifiers(held);
        if (viewportFocusContext || viewportPointerContext)
        {
            // Refresh Shift construction-point feedback even when the pointer
            // is stationary before the first cleave click.
            glView_->refreshCleaveSnapPreviewForModifiers(held);
        }
        updateTransformMoveModifierPreviewForKeyState(key, modifiers, pressed);
    }

    // Step 3: update modifier previews before dispatching shortcuts that may consume the key.
    updateSectorDrawModifierPreviewForKeyState(
        key,
        modifiers,
        pressed,
        viewportFocusContext,
        viewportPointerContext);
    updateThingPlacementModifierPreviewForKeyState(
        key,
        modifiers,
        pressed,
        viewportFocusContext,
        viewportPointerContext);

    // Step 4: panel/editor specific shortcuts get first chance to consume the event.
    if ( pressed && uvTransformShortcutContext )
    {
        // Pointer ownership survives focus lag and an ineligible UV command.
        // Consume the key even when the canvas has no selected island.
        if ( !keyEvent->isAutoRepeat() )
        {
            (void)dispatchUvEditorShortcut(key, modifiers);
        }
        keyEvent->accept();
        return true;
    }
    if ( pressed && cleaveBlockedByUvPointer )
    {
        // UV pointer ownership also prevents custom Cleave keys from reaching
        // the native viewport fallback when keyboard focus has not caught up.
        if ( !keyEvent->isAutoRepeat() )
        {
            (void)dispatchUvEditorShortcut(key, modifiers);
        }
        keyEvent->accept();
        return true;
    }
    if ( pressed && (cleaveTransformShortcutContext || gridVisibilityShortcutContext) )
    {
        if ( !keyEvent->isAutoRepeat() )
        {
            (void)dispatchEditorShortcut(key, modifiers);
        }
        keyEvent->accept();
        return true;
    }
    if (keyEvent
        && pressed
        && forwardUvEditorShortcutIfNeeded(
            key,
            keyEvent->modifiers(),
            keyEvent->isAutoRepeat(),
            uvPanelVisible,
            uvFocusContext,
            uvCanvasPointerContext,
            textEntryFocusContext))
    {
        return true;
    }

    if ( keyEvent
        && pressed
        && uvOwnedViewportShortcutContext
        && (modeAddBindingMatches || lightDeleteBindingMatches) )
    {
        // The UV canvas gets first refusal above. Forwarding A can move focus
        // into the UV panel, so consume a still viewport-delivered Add/Delete
        // binding while UV focus or pointer ownership remains active.
        keyEvent->accept();
        return true;
    }

    if (keyEvent
        && triggerActiveCogScriptEditorSaveForKeyState(
            key,
            modifiers,
            pressed,
            keyEvent->isAutoRepeat(),
            cogEditorFocusContext))
    {
        keyEvent->accept();
        return true;
    }

    // Step 5: non-repeat modifier keys drive the selection hint overlay.
    if (keyEvent && !keyEvent->isAutoRepeat())
    {
        if (ui::shortcuts::isEditorModifierKey(key))
        {
            updateSelectionModifierHintForKeyState(
                key,
                keyEvent->modifiers(),
                pressed,
                viewportFocusContext,
                viewportPointerContext);
        }
        else
        {
            clearSelectionModifierHintIfShown();
        }
    }

    if ( cleaveModifierShortcutContext )
    {
        keyEvent->accept();
        return true;
    }

    // Step 6: viewport shortcuts are gated by focus/text-input context to avoid stealing typing.
    const ui::shortcuts::FlyNavigationDispatchContext flyDispatchContext{
        .viewportAvailable     = glView_ != nullptr,
        .flyNavigationActive   = glView_ && glView_->flyNavigationActive(),
        .keyEventAvailable     = keyEvent != nullptr,
        .autoRepeat            = keyEvent && keyEvent->isAutoRepeat(),
        .textEntryFocusContext = textEntryFocusContext,
    };
    if (ui::shortcuts::shouldDispatchFlyNavigationShortcut(flyDispatchContext))
    {
        if (dispatchFlyNavigationShortcut(key, modifiers, pressed))
        {
            return true;
        }
    }

    const ui::shortcuts::EditorShortcutDispatchContext editorShortcutDispatchContext{
        .viewportAvailable       = glView_ != nullptr,
        .keyEventAvailable       = keyEvent != nullptr,
        .pressed                 = pressed,
        .autoRepeat              = keyEvent && keyEvent->isAutoRepeat(),
        .viewportShortcutContext = viewportKeyShortcutContext,
    };
    if (ui::shortcuts::shouldDispatchEditorShortcut(editorShortcutDispatchContext)
        && dispatchEditorShortcut(key, modifiers))
    {
        return true;
    }

    const ui::shortcuts::ViewportRollDispatchContext viewportRollDispatchContext{
        .viewportAvailable = glView_ != nullptr,
        .keyEventAvailable = keyEvent != nullptr,
        .pressed           = pressed,
        .autoRepeat        = keyEvent && keyEvent->isAutoRepeat(),
        .inViewport        = inViewport,
    };
    if (ui::shortcuts::shouldDispatchViewportRollShortcut(viewportRollDispatchContext))
    {
        if (dispatchViewportRollShortcut(key, modifiers, inViewport))
        {
            return true;
        }
    }

    return std::nullopt;
}

void MainWindow::clearSelectionModifierHintForEventFilterEvent(QEvent* event)
{
    if (event && (event->type() == QEvent::FocusOut || event->type() == QEvent::Leave || event->type() == QEvent::WindowDeactivate))
    {
        clearSelectionModifierHintIfShown();
    }
}

std::optional<bool> MainWindow::handleActiveThingPlacementHeaderDragEvent(QEvent* event)
{
    if (!thingPlacementHeaderDragActive_ || !event)
    {
        return std::nullopt;
    }

    const bool mouseMove = event->type() == QEvent::MouseMove;
    const bool mouseRelease = event->type() == QEvent::MouseButtonRelease;
    auto* mouseEvent = (mouseMove || mouseRelease)
        ? static_cast<QMouseEvent*>(event)
        : nullptr;
    const ui::viewport::PanelHeaderDragEventContext context{
        .active         = thingPlacementHeaderDragActive_,
        .eventAvailable = event != nullptr,
        .mouseMove      = mouseMove,
        .mouseRelease   = mouseRelease,
        .leftButtonHeld = mouseEvent && mouseEvent->buttons().testFlag(Qt::LeftButton),
        .leftButton     = mouseEvent && mouseEvent->button() == Qt::LeftButton,
    };

    // Step 1: route active header-drag events before considering idle header presses.
    switch (ui::viewport::panelHeaderDragEventRoute(context))
    {
        case ui::viewport::PanelHeaderDragEventRoute::cancelDrag:
            cancelThingPlacementHeaderDrag();
            return true;
        case ui::viewport::PanelHeaderDragEventRoute::updatePreview:
            updateThingPlacementHeaderDragPreview(mouseEvent->globalPosition().toPoint());
            return true;
        case ui::viewport::PanelHeaderDragEventRoute::finishDrag:
            finishThingPlacementHeaderDrag(mouseEvent->globalPosition().toPoint());
            return true;
        case ui::viewport::PanelHeaderDragEventRoute::none:
            return std::nullopt;
    }

    return std::nullopt;
}

std::optional<bool> MainWindow::handleThingPlacementHeaderWidgetEvent(QObject* watched, QEvent* event)
{
    if (!watched
        || !watched->property("sed.thingPlacementHeaderDrag").toBool()
        || !event)
    {
        return std::nullopt;
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            thingPlacementHeaderDragActive_ = true;
            thingPlacementHeaderDragStartGlobal_ = mouseEvent->globalPosition().toPoint();
            if (thingPlacementDockPreviewBand_)
            {
                thingPlacementDockPreviewBand_->hide();
            }
            QApplication::setOverrideCursor(Qt::ClosedHandCursor);
            return true;
        }
    }
    if (event->type() == QEvent::MouseButtonRelease)
    {
        return thingPlacementHeaderDragActive_;
    }

    return std::nullopt;
}

std::optional<bool> MainWindow::handleActiveUvEditorPanelHeaderDragEvent(QEvent* event)
{
    if (!uvEditorPanelHeaderDragActive_ || !event)
    {
        return std::nullopt;
    }

    const bool mouseMove = event->type() == QEvent::MouseMove;
    const bool mouseRelease = event->type() == QEvent::MouseButtonRelease;
    auto* mouseEvent = (mouseMove || mouseRelease)
        ? static_cast<QMouseEvent*>(event)
        : nullptr;
    const ui::viewport::PanelHeaderDragEventContext context{
        .active         = uvEditorPanelHeaderDragActive_,
        .eventAvailable = event != nullptr,
        .mouseMove      = mouseMove,
        .mouseRelease   = mouseRelease,
        .leftButtonHeld = mouseEvent && mouseEvent->buttons().testFlag(Qt::LeftButton),
        .leftButton     = mouseEvent && mouseEvent->button() == Qt::LeftButton,
    };

    // Step 1: update, cancel, or finish an active UV-panel drag without duplicating route rules.
    switch (ui::viewport::panelHeaderDragEventRoute(context))
    {
        case ui::viewport::PanelHeaderDragEventRoute::cancelDrag:
            cancelUvEditorPanelHeaderDrag();
            return std::nullopt;
        case ui::viewport::PanelHeaderDragEventRoute::updatePreview:
            updateUvEditorPanelHeaderDragPreview(mouseEvent->globalPosition().toPoint());
            return true;
        case ui::viewport::PanelHeaderDragEventRoute::finishDrag:
            finishUvEditorPanelHeaderDrag(mouseEvent->globalPosition().toPoint());
            return true;
        case ui::viewport::PanelHeaderDragEventRoute::none:
            return std::nullopt;
    }

    return std::nullopt;
}

std::optional<bool> MainWindow::handleUvEditorPanelHeaderWidgetEvent(QObject* watched, QEvent* event)
{
    if (watched != uvEditorPanelHeader_ || !event)
    {
        return std::nullopt;
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            uvEditorPanelHeaderDragActive_ = true;
            uvEditorPanelDockPreviewVisible_ = false;
            if (uvEditorPanelDockPreviewBand_)
            {
                uvEditorPanelDockPreviewBand_->hide();
            }
            uvEditorPanelHeaderDragStartGlobal_ = mouseEvent->globalPosition().toPoint();
            QApplication::setOverrideCursor(Qt::ClosedHandCursor);
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        return uvEditorPanelHeaderDragActive_;
    }

    return std::nullopt;
}

void MainWindow::clearThingPlacementPreviewForPanelHover(QObject* watched, QEvent* event)
{
    if (!event
        || !glView_
        || placementMode_ != PlacementMode::thing
        || (event->type() != QEvent::Enter && event->type() != QEvent::MouseMove))
    {
        return;
    }

    QWidget* watchedWidget = qobject_cast<QWidget*>(watched);
    const bool inThingPanel = ui::viewport::widgetInsidePanel(watchedWidget, thingPlacementPanel_);
    if (inThingPanel)
    {
        glView_->clearPlacementPreviewThing();
        glView_->clearPlacementPreviewLines();
    }
}

void MainWindow::updateViewportOverlaysForEventFilterResize(QObject* watched, QEvent* event)
{
    if (watched != glView_ || !event || event->type() != QEvent::Resize)
    {
        return;
    }

    // Step 1: every viewport-bound overlay follows the resized GL widget.
    positionViewportOverlay();
    positionAddSectorOptionsOverlay();
    positionTransformMoveOptionsOverlay();
    positionLightBakeOverlay();
    positionSectorJoinOptionsOverlay();
    positionSectorThrustOptionsOverlay();
    positionCleaveOptionsOverlay();
    positionThingMoveOptionsOverlay();
    positionSurfaceExtrudeOptionsOverlay();
    positionThingPlacementPanel();
    positionViewportTransformOverlay();
    positionViewportToolsOverlay();
    positionViewportEditModeOverlay();
    reflowTopCenterOverlays();

    // Step 2: delayed refit waits for Qt's resize/layout churn to settle.
    if (pendingEditorViewRefitAfterResize_ && editorViewRefitTimer_)
    {
        editorViewRefitTimer_->start();
    }
}

std::optional<QString> MainWindow::pickCogValueResourceForDoubleClick(
    const ui::ResourcePickerKind kind,
    const QString& currentValue)
{
    const assets::Templates templateDatabaseSnapshot = templates_;
    const std::vector<ThingTemplate> templateOverridesSnapshot =
        document_.level().templates;
    const std::shared_ptr<const assets::ResourceLocator> resourcesSnapshot =
        resourceLocatorSnapshot_;
    const std::filesystem::path currentPathSnapshot = currentPath_;
    const std::string previewCmp = document_.level().getMasterCMP();
    const ProjectType previewProjectType = document_.level().kind;

    // Keep all picker inputs alive and immutable for the nested modal session.
    ui::ResourcePickerOptions options{};
    options.kind = kind;
    options.initialName = currentValue.trimmed().toStdString();
    options.preferredDiskRoot = preferredExtractionRoot();
    if (!currentPathSnapshot.empty())
    {
        options.projectDirectory = currentPathSnapshot.parent_path();
    }
    options.title = ui::resourcePickerTitle(kind);
    options.previewCmp = previewCmp;
    options.previewProjectType = previewProjectType;
    options.templateDatabaseFallback = templateDatabaseSnapshot.size() > 0
        ? &templateDatabaseSnapshot : nullptr;
    options.templateOverrides = &templateOverridesSnapshot;
    const auto picked = ui::pickResource(
        this, resourcesSnapshot, options);
    if (!picked)
    {
        return std::nullopt;
    }
    return QString::fromStdString(*picked);
}

std::optional<bool> MainWindow::handleLineEditDoubleClickEvent(QObject* watched, QEvent* event)
{
    if (!event || event->type() != QEvent::MouseButtonDblClick)
    {
        return std::nullopt;
    }

    auto* edit = qobject_cast<QLineEdit*>(watched);
    if (!edit)
    {
        return std::nullopt;
    }

    // Step 1: ordinary property fields can mirror their adjacent picker button.
    const QVariant auxiliaryButtonVar = edit->property("sed.auxiliaryButton");
    if (auxiliaryButtonVar.isValid())
    {
        auto* auxiliaryButton = qobject_cast<QAbstractButton*>(
            auxiliaryButtonVar.value<QObject*>());
        if (auxiliaryButton && auxiliaryButton->isEnabled())
        {
            auxiliaryButton->click();
            return true;
        }
        return false;
    }

    // Step 2: COG script-name editors open the script editor directly.
    const QVariant scriptNameVar = edit->property("sed.cogScriptName");
    if (scriptNameVar.isValid())
    {
        const QString scriptName = scriptNameVar.toString().trimmed();
        if (!scriptName.isEmpty())
        {
            openCogScriptEditor(scriptName.toStdString());
            return true;
        }
    }

    if (!glView_)
    {
        return false;
    }

    // Step 3: frame-value editors select the referenced frame in the viewport.
    const QVariant thingVar = edit->property("sed.frameThingIndex");
    const QVariant valueVar = edit->property("sed.frameValueIndex");
    if (thingVar.isValid() && valueVar.isValid())
    {
        const int thingIndex = thingVar.toInt();
        const int valueIndex = valueVar.toInt();

        const Level& level = document_.level();
        if (thingIndex >= 0 && valueIndex >= 0
            && static_cast<std::size_t>(thingIndex) < level.things.size()
            && static_cast<std::size_t>(valueIndex) < level.things[static_cast<std::size_t>(thingIndex)].values.size())
        {
            const TplValue& v = level.things[static_cast<std::size_t>(thingIndex)].values[static_cast<std::size_t>(valueIndex)];
            if (v.type == ValueType::frame && sed::text::iequalsAscii(v.name, "frame"))
            {
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
                return true; // consume: widget may be destroyed by rebuildProperties
            }
        }
    }

    // Step 3: COG-value editors either open resource pickers or navigate to referenced entities.
    const QVariant cogVar = edit->property("sed.cogEntityCogIndex");
    const QVariant cogValueVar = edit->property("sed.cogEntityValueIndex");
    if (!cogVar.isValid() || !cogValueVar.isValid())
    {
        return std::nullopt;
    }

    const std::int32_t cogIndex = cogVar.toInt();
    const std::int32_t cogValueIndex = cogValueVar.toInt();

    const Level& level = document_.level();
    if (cogIndex < 0
        || cogValueIndex < 0
        || static_cast<std::size_t>(cogIndex) >= level.cogs.size()
        || static_cast<std::size_t>(cogValueIndex) >= level.cogs[static_cast<std::size_t>(cogIndex)].values.size())
    {
        return std::nullopt;
    }

    CogValue value = level.cogs[static_cast<std::size_t>(cogIndex)].values[static_cast<std::size_t>(cogValueIndex)];
    const CogValue authoredValue = value;
    const QString raw = edit->text().trimmed();
    const bool parseFailed = !raw.isEmpty() && !value.parseJed(level, raw.toStdString());
    if (parseFailed && !ui::cogTypeAllowsDoubleClickAfterParseFailure(value.cogType))
    {
        return true;
    }

    OutlinerKind kind = OutlinerKind::none;
    std::int32_t index = -1;
    std::int32_t sector = -1;
    std::int32_t surface = -1;

    // Step 4: resource-like COG values open the matching picker and write back the chosen value.
    if (ui::cogTypeUsesDoubleClickResourcePicker(value.cogType))
    {
        const auto pickerKind = ui::resourcePickerKindForCogType(value.cogType);
        if (pickerKind)
        {
            const QPointer<MainWindow> ownerGuard(this);
            const QPointer<QLineEdit> editGuard(edit);
            const std::uint64_t sourceDocumentEpoch = recoveryDocumentEpoch_;
            const Cog& sourceOwner = level.cogs[static_cast<std::size_t>(cogIndex)];
            const std::int32_t sourceOwnerId = sourceOwner.id;
            const std::string sourceOwnerName = sourceOwner.name;
            const editor::Selection sourceSelection = document_.selection();
            const auto picked = pickCogValueResourceForDoubleClick(*pickerKind, raw);
            if ( !ownerGuard || !editGuard || !picked )
            {
                return true;
            }

            if ( recoveryDocumentEpoch_ != sourceDocumentEpoch
                || !documentEditingAllowed()
                || editGuard->text().trimmed() != raw )
            {
                return true;
            }

            const Level& currentLevel = document_.level();
            if ( static_cast<std::size_t>(cogIndex) >= currentLevel.cogs.size() )
            {
                return true;
            }
            const Cog& currentOwner =
                currentLevel.cogs[static_cast<std::size_t>(cogIndex)];
            if ( currentOwner.id != sourceOwnerId
                || currentOwner.name != sourceOwnerName
                || static_cast<std::size_t>(cogValueIndex)
                    >= currentOwner.values.size()
                || !sameCogValuePickerSnapshot(
                    currentOwner.values[static_cast<std::size_t>(cogValueIndex)],
                    authoredValue)
                || !sameEditorSelectionSnapshot(
                    document_.selection(), sourceSelection) )
            {
                return true;
            }

            if ( picked->trimmed() != raw )
            {
                editGuard->setText(*picked);
                QMetaObject::invokeMethod(
                    editGuard.data(), "editingFinished", Qt::DirectConnection);
            }
        }
        return true;
    }

    // Step 5: entity-like COG values navigate to the referenced editor object.
    switch (value.cogType)
    {
        case CogType::cog:
        {
            if (value.intValue >= 0 && static_cast<std::size_t>(value.intValue) < level.cogs.size())
            {
                document_.selection().clear();
                document_.selection().cogs.insert(value.intValue);
                if (outlinerTabs_)
                {
                    outlinerTabs_->setCurrentIndex(2);
                }
                glView_->setSelectionState(-1, -1, -1, -1, document_.selection());
                rebuildOutliner();
                updateEditActions();
                rebuildProperties();
            }
            else
            {
                const Cog& sourceOwner =
                    level.cogs[static_cast<std::size_t>(cogIndex)];
                const std::int32_t sourceOwnerId = sourceOwner.id;
                const std::string sourceOwnerName = sourceOwner.name;
                const std::unordered_set<std::int32_t> sourceSelection =
                    document_.selection().cogs;
                const QPointer<QLineEdit> editGuard(edit);
                const std::optional<std::int32_t> selectedIndex =
                    ui::pickPlacedCogInstance(this, level, value.intValue);
                if (!selectedIndex || !editGuard)
                {
                    return true;
                }

                const Level& currentLevel = document_.level();
                if (cogIndex < 0
                    || static_cast<std::size_t>(cogIndex)
                        >= currentLevel.cogs.size())
                {
                    return true;
                }
                const Cog& currentOwner =
                    currentLevel.cogs[static_cast<std::size_t>(cogIndex)];
                if (currentOwner.id != sourceOwnerId
                    || currentOwner.name != sourceOwnerName
                    || cogValueIndex < 0
                    || static_cast<std::size_t>(cogValueIndex)
                        >= currentOwner.values.size()
                    || !sameCogValuePickerSnapshot(
                        currentOwner.values[
                            static_cast<std::size_t>(cogValueIndex)],
                        authoredValue)
                    || document_.selection().cogs != sourceSelection
                    || (*selectedIndex >= 0
                        && static_cast<std::size_t>(*selectedIndex)
                            >= currentLevel.cogs.size()))
                {
                    return true;
                }

                if (authoredValue.intValue != *selectedIndex)
                {
                    editGuard->setText(QString::number(*selectedIndex));
                    QMetaObject::invokeMethod(
                        editGuard.data(), "editingFinished",
                        Qt::DirectConnection);
                    cogValueFocusPending_ = true;
                    cogValueFocusCogIndex_ = cogIndex;
                    cogValueFocusValueIndex_ = cogValueIndex;
                }
            }
            return true;
        }
        case CogType::sec:
        {
            if (value.intValue >= 0 && static_cast<std::size_t>(value.intValue) < level.sectors.size())
            {
                kind = OutlinerKind::sector;
                index = value.intValue;
            }
            break;
        }
        case CogType::thing:
        {
            if (value.intValue >= 0 && static_cast<std::size_t>(value.intValue) < level.things.size())
            {
                kind = OutlinerKind::thing;
                index = value.intValue;
            }
            break;
        }
        case CogType::surface:
        {
            if (value.intValue >= 0)
            {
                if (const std::optional<SurfaceRef> ref = level.tryGetSurfaceRefByGlobalSurfaceNumber(value.intValue))
                {
                    kind = OutlinerKind::surface;
                    index = ref->surface;
                    sector = ref->sector;
                    surface = ref->surface;
                }
            }
            break;
        }
        default:
            break;
    }

    if (kind != OutlinerKind::none)
    {
        if (navigateToEntity(kind, index, sector, surface, true, true))
        {
            return true; // consume: handled as navigation
        }
    }

    return std::nullopt;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    bool cleaveWheelModifierReleaseOwned = false;
    if (event && glView_)
    {
        if (event->type() == QEvent::KeyRelease)
        {
            const auto* keyEvent = static_cast<QKeyEvent*>(event);
            cleaveWheelModifierReleaseOwned = glView_->handleStagedCleaveWheelModifierRelease(
                keyEvent->key(), keyEvent->isAutoRepeat());
        }
        if (event->type() == QEvent::WindowDeactivate
            || event->type() == QEvent::ApplicationDeactivate
            || (watched == glView_ && (event->type() == QEvent::FocusOut
                || event->type() == QEvent::UngrabMouse || event->type() == QEvent::Hide)))
        {
            glView_->cancelViewportMouseGesture();
            flyHeldKeys_.clear();
        }
        if ( event->type() == QEvent::MouseButtonRelease
            && glView_->viewportMouseGestureButton() != Qt::NoButton )
        {
            const QWidget* const watchedWidget = qobject_cast<QWidget*>(watched);
            if ( watchedWidget
                && !ui::viewport::widgetInsidePanel(watchedWidget, glView_)
                && static_cast<QMouseEvent*>(event)->button()
                    == glView_->viewportMouseGestureButton() )
            {
                glView_->cancelViewportMouseGesture();
                flyHeldKeys_.clear();
            }
        }
    }
    if (event && event->type() == QEvent::FocusIn)
    {
        // QApplication::focusWidget() may lag the delivered FocusIn event
        // (and is deliberately null in offscreen/headless hosts). Route from
        // the actual event owner while retaining this window's ancestry gate.
        updateCogScriptEditorFocusFromWidget(
            qobject_cast<QWidget*>(watched));
        if ( QWidget* const nativeEditor = ui::shortcuts::focusedNativeTextEditor(
                qobject_cast<QWidget*>(watched)) )
        {
            if ( QLineEdit* const lineEdit = qobject_cast<QLineEdit*>(nativeEditor) )
            {
                connect(
                    lineEdit,
                    &QLineEdit::textChanged,
                    this,
                    &MainWindow::syncCogScriptEditorActionFocus,
                    Qt::UniqueConnection);
            }
            else if ( QPlainTextEdit* const plainTextEdit =
                         qobject_cast<QPlainTextEdit*>(nativeEditor) )
            {
                connect(
                    plainTextEdit->document(),
                    &QTextDocument::undoAvailable,
                    this,
                    &MainWindow::syncCogScriptEditorActionFocus,
                    Qt::UniqueConnection);
                connect(
                    plainTextEdit->document(),
                    &QTextDocument::redoAvailable,
                    this,
                    &MainWindow::syncCogScriptEditorActionFocus,
                    Qt::UniqueConnection);
            }
            else if ( QTextEdit* const textEdit = qobject_cast<QTextEdit*>(nativeEditor) )
            {
                connect(
                    textEdit->document(),
                    &QTextDocument::undoAvailable,
                    this,
                    &MainWindow::syncCogScriptEditorActionFocus,
                    Qt::UniqueConnection);
                connect(
                    textEdit->document(),
                    &QTextDocument::redoAvailable,
                    this,
                    &MainWindow::syncCogScriptEditorActionFocus,
                    Qt::UniqueConnection);
            }
        }
        QWidget* const focusOwner = qobject_cast<QWidget*>(watched);
        for ( const ResourceEditorTabHost& host : resourceEditorTabHosts_ )
        {
            if ( focusOwner && host.dock && host.tabs && host.tabs->count() > 0
                && host.sessionGeneration == resourceEditorDocumentContextGeneration_
                && (host.dock.data() == focusOwner || host.dock->isAncestorOf(focusOwner)) )
            {
                setProperty("resource_authoring.lastActiveHostId", host.stableId);
                break;
            }
        }
    }

    // Normalize value editors as they are polished, before an initial wheel
    // event can use Qt's WheelFocus policy to focus a control merely because
    // the pointer is hovering over it.  The wheel router below remains the
    // final guard for controls created dynamically after a panel opens.
    if (event && (event->type() == QEvent::Polish
            || event->type() == QEvent::Show))
    {
        ui::normalizeWheelEditorFocusPolicy(watched);
    }

    // Step 1: apply non-consuming global housekeeping branches.
    syncApplicationDialogPolicy(watched, event);
    syncTopLevelWindowChrome(watched, event);
    polishMessageBoxForEventFilter(watched, event);
    syncDockWidthsForEventFilterResize(watched, event);
    handleThingPlacementTemplateListBackgroundPress(watched, event);
    if (event && (event->type() == QEvent::MouseButtonRelease
            || event->type() == QEvent::KeyRelease
            || event->type() == QEvent::FocusOut))
    {
        // This catches state-only edits (tool options, tabs, panel layout) that
        // do not create a level undo record. The debounce keeps the global
        // event filter inexpensive during normal interaction.
        // Most releases do not change recoverable state (notably ending RMB
        // fly/look). Compare the compact EDIT payload first so a no-op input
        // release cannot serialize/copy the full Level and undo timeline.
        noteRecoveryEditorStateChangedIfNeeded();
    }
    if (cleaveWheelModifierReleaseOwned)
    {
        event->accept();
        return true;
    }

    // Keep persistent viewport settings panels available while their own
    // controls are being edited, but dismiss them like popups as soon as the
    // user clicks elsewhere. Each launcher is excluded so its checked state
    // remains the authoritative explicit show/hide toggle.
    if (event && event->type() == QEvent::MouseButtonPress)
    {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QPoint globalPress = mouseEvent->globalPosition().toPoint();
        const auto containsGlobalPress = [globalPress](const QWidget* widget)
        {
            return widget && QRect(widget->mapToGlobal(QPoint(0, 0)), widget->size())
                .contains(globalPress);
        };
        QWidget* target = qobject_cast<QWidget*>(watched);
        const bool windowLevelDispatch = !target || target->isWindow();
        const auto dismissWhenOutside = [&](QWidget* panel, QToolButton* launcher, QWidget* content = nullptr)
        {
            if (!panel || panel->isHidden())
            {
                return;
            }
            QWidget* const activePopup = QApplication::activePopupWidget();
            bool activePopupBelongsToPanel = false;
            for ( QWidget* popupAncestor = activePopup;
                popupAncestor != nullptr;
                popupAncestor = popupAncestor->parentWidget() )
            {
                // Popup widgets are separate native windows, so the usual
                // isAncestorOf(target) check cannot reach through their
                // window boundary. Follow the parent chain explicitly, but
                // only for the active popup and only when this panel owns it.
                if ( popupAncestor == panel || panel->isAncestorOf(popupAncestor) )
                {
                    activePopupBelongsToPanel = true;
                    break;
                }
            }
            const bool insideOwningPopup = activePopupBelongsToPanel
                && containsGlobalPress(activePopup);
            const bool insidePanel = (target
                    && (target == panel || panel->isAncestorOf(target)))
                || insideOwningPopup
                || (windowLevelDispatch && containsGlobalPress(panel));
            const bool onLauncher = (target && launcher
                    && (target == launcher || launcher->isAncestorOf(target)))
                || (windowLevelDispatch && containsGlobalPress(launcher));
            if (!insidePanel && !onLauncher)
            {
                if (content)
                {
                    content->hide();
                }
                panel->hide();
                if (launcher)
                {
                    const QSignalBlocker blocker(launcher);
                    launcher->setChecked(false);
                }
            }
        };
        dismissWhenOutside(
            viewportMapSettingsScrollArea_, viewportMapSettingsButton_, viewportMapSettingsMenu_);
        dismissWhenOutside(viewportThingRenderingMenu_, viewportThingModeButton_);
    }

    // Step 2: dropdowns never change from hover/focus wheel input; other
    // value editors consume the wheel only while genuinely focused. When an
    // editor is blocked, its enclosing scroll area receives the delta when
    // one exists.
    if (ui::routeUnfocusedEditorWheel(watched, event))
    {
        return true;
    }

    // Step 3: consume thing-placement wheel and spin-scrub interactions before panel resize routing.
    if (const std::optional<bool> wheelResult = handleThingPlacementWheelEvent(watched, event))
    {
        return *wheelResult;
    }

    if (const std::optional<bool> spinScrubResult =
            handleThingPlacementSpinScrubEvent(watched, event))
    {
        return *spinScrubResult;
    }

    if (const std::optional<bool> panelResizeResult =
            handleThingPlacementPanelResizeEvent(watched, event))
    {
        return *panelResizeResult;
    }

    // Step 4: a second click on a light gizmo opens the compact editor for the
    // active family. GlView stores the resolved kind because one actor can own
    // both a regular light and a Headlight.
    if (watched == glView_ && event && event->type() == QEvent::MouseButtonDblClick
        && documentEditingAllowed()
        && editMode_ == EditMode::light
        && viewportEntitySelectionInputActive()
        && placementMode_ == PlacementMode::none)
    {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QVariant kindValue = glView_->property("sed.selectedLightGizmoKind");
        const auto kind = static_cast<render::LightGizmoKind>(kindValue.toInt());
        if (!glView_->flyNavigationActive()
            && glView_->viewportMouseGestureButton() == Qt::NoButton
            && viewportMouseActionForPress(glView_->viewportNavigationSettings(),
                mouseEvent->button(), mouseEvent->modifiers()) == ViewportMouseAction::pick)
        {
            if (lightEditSubMode_ == LightEditSubMode::thing
                && lastViewportClickThing_ >= 0
                && kindValue.isValid()
                && (kind == render::LightGizmoKind::ThingRegular
                    || kind == render::LightGizmoKind::ThingHead))
            {
                const std::int32_t thingIndex = lastViewportClickThing_;
                const std::string valueName = kind == render::LightGizmoKind::ThingHead
                    ? "lightintensity"
                    : "light";
                QTimer::singleShot(0, this, [this, thingIndex, valueName]()
                    {
                        if (documentEditingAllowed())
                        {
                            openThingLightValueDialog(thingIndex, valueName);
                        }
                    });
                return true;
            }
            if (lightEditSubMode_ == LightEditSubMode::baked
                && lastViewportClickLight_ >= 0)
            {
                const std::int32_t lightIndex = lastViewportClickLight_;
                QTimer::singleShot(0, this, [this, lightIndex]()
                    {
                        if (documentEditingAllowed())
                        {
                            openBakedLightParametersDialog(lightIndex);
                        }
                    });
                return true;
            }
            if (lightEditSubMode_ == LightEditSubMode::sector)
            {
                const Level& level = document_.level();
                const bool pickedSectorLightGizmo = lastViewportClickSector_ >= 0
                    && static_cast<std::size_t>(lastViewportClickSector_) < level.sectors.size()
                    && kindValue.isValid()
                    && kind == render::LightGizmoKind::SectorPoint
                    && pointLightHasAuthoredParameters(
                        level.sectors[static_cast<std::size_t>(lastViewportClickSector_)].pointLight);
                if (pickedSectorLightGizmo)
                {
                    const std::int32_t sectorIndex = lastViewportClickSector_;
                    QTimer::singleShot(0, this, [this, sectorIndex]()
                        {
                            if (documentEditingAllowed())
                            {
                                openSectorLightParametersDialog(sectorIndex);
                            }
                        });
                    return true;
                }

                // A Sector-light category click normally targets the icon. On
                // double-click, also resolve the level surface below the cursor
                // so a distant point-light source can be found from anywhere
                // inside its owner sector.
                std::int32_t sectorIndex = -1;
                if (lastViewportClickSector_ >= 0
                    && static_cast<std::size_t>(lastViewportClickSector_) < level.sectors.size())
                {
                    sectorIndex = lastViewportClickSector_;
                }
                else
                {
                    bool haveSurfaceHit = false;
                    SurfaceRef surfaceRef{};
                    (void)glView_->placementPointForViewport(
                        mouseEvent->position().toPoint(),
                        &haveSurfaceHit,
                        &surfaceRef,
                        nullptr,
                        true,
                        false);
                    if (haveSurfaceHit
                        && surfaceRef.sector >= 0
                        && static_cast<std::size_t>(surfaceRef.sector) < level.sectors.size())
                    {
                        sectorIndex = surfaceRef.sector;
                    }
                }

                if (sectorIndex >= 0)
                {
                    const PointLight& light =
                        level.sectors[static_cast<std::size_t>(sectorIndex)].pointLight;
                    if (!pointLightHasAuthoredParameters(light))
                    {
                        glView_->showToast(
                            QStringLiteral("Sector has no point light to focus"),
                            2800,
                            GlView::ToastPlacement::BottomCenter,
                            GlView::ToastGroup::Error);
                        return true;
                    }

                    const QVector3D center(
                        static_cast<float>(light.position.x),
                        static_cast<float>(light.position.y),
                        static_cast<float>(light.position.z));
                    if (!std::isfinite(center.x())
                        || !std::isfinite(center.y())
                        || !std::isfinite(center.z()))
                    {
                        glView_->showToast(
                            QStringLiteral("Sector point light has an invalid position"),
                            2800,
                            GlView::ToastPlacement::BottomCenter,
                            GlView::ToastGroup::Error);
                        return true;
                    }

                    document_.selection().clear();
                    document_.selection().sectors.insert(sectorIndex);
                    lastLightSectorSelection_.clear();
                    lastLightSectorSelection_.sectors.insert(sectorIndex);
                    glView_->setSelectionState(
                        sectorIndex,
                        -1,
                        -1,
                        -1,
                        document_.selection());
                    rebuildOutliner();
                    updateEditActions();
                    rebuildProperties();

                    const float halfExtent = std::max(
                        0.1f,
                        glView_->lightGizmoWorldSize() * 2.5f);
                    const QVector3D extent(halfExtent, halfExtent, halfExtent);
                    frameBoundsWithNavigationPrefs(center - extent, center + extent, true);
                    return true;
                }
            }
        }
    }

    // Step 4: double-clicks from property editors may navigate or open resource pickers.
    if (const std::optional<bool> doubleClickResult =
            handleLineEditDoubleClickEvent(watched, event))
    {
        return *doubleClickResult;
    }

    // Step 5: keyboard shortcuts are centralized so text-entry guards stay in one place.
    // A brush stroke owns one live undo transaction. Close it before the small
    // set of commands that can insert/consume another history record or mark
    // the document saved. Ordinary and unbound keys must not interrupt a drag.
    if (thingPlacementBrushStrokeActive_
        && !(glView_ && glView_->flyNavigationActive())
        && event
        && event->type() == QEvent::KeyPress)
    {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        auto matchesLiveStrokeBoundary = [keyEvent](const QAction* action) noexcept
        {
            return action
                && action->isEnabled()
                && ui::shortcuts::actionMatchesShortcut(
                    action,
                    keyEvent->key(),
                    keyEvent->modifiers(),
                    false);
        };
        const std::array<QAction*, 6> strokeBoundaryActions{
            undoAction_,
            redoAction_,
            deleteAction_,
            findChild<QAction*>(QStringLiteral("edit.paste_entities")),
            findChild<QAction*>(QStringLiteral("file.save")),
            findChild<QAction*>(QStringLiteral("file.save_as")),
        };
        if (std::any_of(
                strokeBoundaryActions.begin(),
                strokeBoundaryActions.end(),
                matchesLiveStrokeBoundary))
        {
            (void)finalizeThingPlacementBrushStroke();
        }
    }
    if (const std::optional<bool> keyResult = handleGlobalShortcutKeyEvent(watched, event))
    {
        return *keyResult;
    }
    clearSelectionModifierHintForEventFilterEvent(event);
    if (event
        && (event->type() == QEvent::WindowDeactivate
            || event->type() == QEvent::ApplicationDeactivate))
    {
        if (thingPlacementBrushStrokeActive_)
        {
            (void)finalizeThingPlacementBrushStroke();
        }
        // A modifier release can be delivered to another application. Restore
        // the saved option states so the top-bar preview cannot remain latched.
        if (glView_)
        {
            glView_->refreshTransformDragForModifiers(Qt::NoModifier);
        }
        updateTransformMoveModifierPreviewForKeyState(
            Qt::Key_Control,
            Qt::NoModifier,
            false);
    }

    // Step 6: floating panel header drags are ordered active-drag first, idle header second.
    if (const std::optional<bool> activeThingHeaderResult =
            handleActiveThingPlacementHeaderDragEvent(event))
    {
        return *activeThingHeaderResult;
    }
    if (const std::optional<bool> thingHeaderResult =
            handleThingPlacementHeaderWidgetEvent(watched, event))
    {
        return *thingHeaderResult;
    }

    if (const std::optional<bool> activeUvHeaderResult =
            handleActiveUvEditorPanelHeaderDragEvent(event))
    {
        return *activeUvHeaderResult;
    }

    if (const std::optional<bool> uvHeaderResult =
            handleUvEditorPanelHeaderWidgetEvent(watched, event))
    {
        return *uvHeaderResult;
    }

    clearThingPlacementPreviewForPanelHover(watched, event);

    if (glView_ && event
        && ui::viewport::isThingPlacementTemplateDropEvent(event->type()))
    {
        return handleThingPlacementTemplateDropEvent(watched, event);
    }

    updateViewportOverlaysForEventFilterResize(watched, event);

    const bool recoveryLayoutEvent = event
        && (event->type() == QEvent::Move
            || event->type() == QEvent::Resize
            || event->type() == QEvent::WindowStateChange);
    QDockWidget* movedDock = recoveryLayoutEvent
        ? qobject_cast<QDockWidget*>(watched) : nullptr;
    if (recoveryLayoutEvent
        && (watched == this
            || (movedDock
                && (isAncestorOf(movedDock)
                    || (workspaceWindow_ && workspaceWindow_->isAncestorOf(movedDock))))))
    {
        noteRecoveryEditorStateChangedIfNeeded();
    }

    return QMainWindow::eventFilter(watched, event);
}

} // namespace sed
