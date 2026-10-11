#include "ui/cog/cog_script_tab_utils.hpp"

#include <utility>
#include <vector>

#include <QColor>
#include <QEvent>
#include <QFileInfo>
#include <QAbstractButton>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QRect>
#include <QSignalBlocker>
#include <QTabBar>
#include <QTabWidget>
#include <QVariant>
#include <QWidget>

namespace sed::ui::cog
{
namespace
{

QIcon makeCogScriptTabPinIcon(const QColor& color, const bool pinned)
{
    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::transparent);

    // Step 1: angle a familiar pushpin silhouette without adding a background tile.
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(9.0, 9.0);
    painter.rotate(-38.0);

    // Step 2: the hover state is an outline; pinned tabs use the same thin filled shape.
    QColor stroke = color;
    stroke.setAlpha(pinned ? 245 : 190);
    QPen pen(stroke, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);

    QColor fill = color;
    fill.setAlpha(pinned ? 220 : 26);

    QPainterPath pin;
    pin.moveTo(-3.5, -5.0);
    pin.lineTo(3.5, -5.0);
    pin.lineTo(2.2, -2.5);
    pin.lineTo(1.4, -1.5);
    pin.lineTo(1.0, 2.4);
    pin.lineTo(0.0, 6.0);
    pin.lineTo(-1.0, 2.4);
    pin.lineTo(-1.4, -1.5);
    pin.lineTo(-2.2, -2.5);
    pin.closeSubpath();
    painter.setBrush(fill);
    painter.drawPath(pin);
    return QIcon(pixmap);
}

QIcon makeCogScriptTabIconPlaceholder()
{
    static const QIcon placeholder = []()
    {
        QPixmap pixmap(18, 18);
        pixmap.fill(Qt::transparent);
        return QIcon(pixmap);
    }();
    return placeholder;
}

void updateCogScriptDockTabIcon(QTabWidget* tabs, const int index)
{
    if ( !tabs || !tabs->tabBar() || index < 0 || index >= tabs->count() )
    {
        return;
    }

    QWidget* page = tabs->widget(index);
    if ( !page )
    {
        return;
    }

    const bool pinned = page->property("cogPinned").toBool();
    const QVariant hoveredVar = tabs->property("cogHoverIndex");
    const int hoveredIndex = hoveredVar.isValid() ? hoveredVar.toInt() : -1;
    const bool showHoverIcon = !pinned && hoveredIndex == index;
    const QIcon desiredIcon = (pinned || showHoverIcon)
        ? makeCogScriptTabPinIcon(
            pinned ? QColor(118, 172, 255) : QColor(182, 196, 224),
            pinned)
        : makeCogScriptTabIconPlaceholder();
    if ( tabs->tabIcon(index).cacheKey() != desiredIcon.cacheKey() )
    {
        tabs->setTabIcon(index, desiredIcon);
    }
}

QRect cogScriptTabPinHitRect(QTabWidget* tabs, const int index)
{
    // Step 1: reject stale indexes or partially constructed tab widgets.
    if (!tabs || !tabs->tabBar() || index < 0 || index >= tabs->count())
    {
        return QRect{};
    }

    // Step 2: align the clickable target with the icon position at the left edge of the tab.
    const QRect tabRect = tabs->tabBar()->tabRect(index);
    if (!tabRect.isValid())
    {
        return QRect{};
    }

    const QSize iconSize = tabs->tabBar()->iconSize().expandedTo(QSize(12, 12));
    return QRect(
        tabRect.left() + 6,
        tabRect.center().y() - (iconSize.height() / 2),
        iconSize.width() + 8,
        iconSize.height() + 4);
}

void updateCogScriptTabCloseAffordance(QTabWidget* tabs, const int index, const bool pinned)
{
    if ( !tabs || !tabs->tabsClosable() || !tabs->tabBar() )
    {
        return;
    }
    QWidget* closeButton = tabs->tabBar()->tabButton(index, QTabBar::RightSide);
    if ( !closeButton )
    {
        closeButton = tabs->tabBar()->tabButton(index, QTabBar::LeftSide);
    }
    if ( !closeButton )
    {
        return;
    }

    if ( auto* button = qobject_cast<QAbstractButton*>(closeButton) )
    {
        constexpr auto kOriginalCloseIcon = "sed.cogOriginalCloseIcon";
        if ( !button->property(kOriginalCloseIcon).isValid() )
        {
            button->setProperty(kOriginalCloseIcon, button->icon());
        }
        const QIcon original = qvariant_cast<QIcon>(
            button->property(kOriginalCloseIcon));
        const QIcon desired = pinned ? makeCogScriptTabIconPlaceholder() : original;
        if ( button->icon().cacheKey() != desired.cacheKey() )
        {
            button->setIcon(desired);
        }
    }
    // Keep the native close widget in the tab layout so pin/unpin cannot
    // change width. Replacing only its icon avoids attaching an effects object
    // to Qt's internally recreated tab button while the pin is authoritative.
    closeButton->setEnabled(!pinned);
    closeButton->setAttribute(Qt::WA_TransparentForMouseEvents, pinned);
}

} // namespace

void updateCogScriptDockTabPresentation(QTabWidget* tabs, QWidget* page, const QString& rawTitle)
{
    // Step 1: resolve the page's current tab index; detached pages cannot be updated.
    if (!tabs || !page)
    {
        return;
    }

    const int index = tabs->indexOf(page);
    if (index < 0)
    {
        return;
    }

    const QString storedTitle = rawTitle.isEmpty()
        ? page->property("cogRawTitle").toString()
        : rawTitle;
    const QString effectiveTitle = storedTitle.isEmpty() ? page->windowTitle() : storedTitle;
    page->setProperty("cogRawTitle", effectiveTitle);

    // Step 2: choose the label, tooltip, and optional pin icon from page state plus hover state.
    const bool pinned = page->property("cogPinned").toBool();
    const QString fullPath = page->property("cogFullPath").toString().trimmed();
    const QString archiveTooltip = page->property("cogArchiveTooltip").toString();
    if ( tabs->tabText(index) != effectiveTitle )
    {
        tabs->setTabText(index, effectiveTitle);
    }
    QString tooltip = !archiveTooltip.isEmpty()
        ? archiveTooltip
        : fullPath.isEmpty() ? effectiveTitle : fullPath;
    if (pinned)
    {
        tooltip += QStringLiteral("\nPinned - unpin to close");
    }
    if ( tabs->tabToolTip(index) != tooltip )
    {
        tabs->setTabToolTip(index, tooltip);
    }
    updateCogScriptDockTabIcon(tabs, index);
    updateCogScriptTabCloseAffordance(tabs, index, pinned);
}

void refreshCogScriptDockTabPresentation(QTabWidget* tabs)
{
    if (!tabs)
    {
        return;
    }

    // Step: re-derive every tab's icon/text from properties instead of relying on cached UI state.
    for (int i = 0; i < tabs->count(); ++i)
    {
        updateCogScriptDockTabPresentation(tabs, tabs->widget(i));
    }
}

void normalizeCogScriptTabOrder(QTabWidget* tabs)
{
    if (!tabs || !tabs->tabBar())
    {
        return;
    }

    struct TabState
    {
        QWidget* page{};
        QString text{};
        QIcon icon{};
        QString tooltip{};
        QVariant data{};
        bool enabled{true};
    };

    QSignalBlocker blocker(tabs->tabBar());
    QSignalBlocker tabWidgetBlocker(tabs);
    std::vector<TabState> pinnedTabs{};
    std::vector<TabState> unpinnedTabs{};
    pinnedTabs.reserve(static_cast<std::size_t>(tabs->count()));
    unpinnedTabs.reserve(static_cast<std::size_t>(tabs->count()));
    QWidget* currentPage = tabs->currentWidget();

    // Step 1: snapshot each tab because QTabWidget owns the tab metadata while pages are reordered.
    for (int i = 0; i < tabs->count(); ++i)
    {
        QWidget* page = tabs->widget(i);
        if (!page)
        {
            continue;
        }

        TabState state{
            .page    = page,
            .text    = tabs->tabText(i),
            .icon    = tabs->tabIcon(i),
            .tooltip = tabs->tabToolTip(i),
            .data    = tabs->tabBar()->tabData(i),
            .enabled = tabs->isTabEnabled(i),
        };
        if (page->property("cogPinned").toBool())
        {
            pinnedTabs.push_back(std::move(state));
        }
        else
        {
            unpinnedTabs.push_back(std::move(state));
        }
    }

    // Step 2: preserve existing relative order inside pinned and unpinned groups.
    std::vector<TabState> orderedTabs{};
    orderedTabs.reserve(pinnedTabs.size() + unpinnedTabs.size());
    orderedTabs.insert(orderedTabs.end(), pinnedTabs.begin(), pinnedTabs.end());
    orderedTabs.insert(orderedTabs.end(), unpinnedTabs.begin(), unpinnedTabs.end());

    // Step 3: rebuild tab order through QTabWidget itself so page and tab indexes stay aligned.
    for (int i = tabs->count() - 1; i >= 0; --i)
    {
        tabs->removeTab(i);
    }
    for (int i = 0; i < static_cast<int>(orderedTabs.size()); ++i)
    {
        const TabState& state = orderedTabs[static_cast<std::size_t>(i)];
        tabs->insertTab(i, state.page, state.icon, state.text);
        tabs->setTabToolTip(i, state.tooltip);
        tabs->setTabEnabled(i, state.enabled);
        tabs->tabBar()->setTabData(i, state.data);
    }

    if (currentPage)
    {
        tabs->setCurrentWidget(currentPage);
    }
}

std::optional<QString> cogScriptTabRevealPath(const QWidget* page)
{
    if (!page || !page->property("cogLooseFile").toBool())
    {
        return std::nullopt;
    }
    const QString path = page->property("cogFullPath").toString().trimmed();
    if (path.isEmpty())
    {
        return std::nullopt;
    }
    const QFileInfo info(path);
    if (!info.isFile())
    {
        return std::nullopt;
    }
    return info.absoluteFilePath();
}

bool cogScriptTabCanClose(const QWidget* page)
{
    return page && !page->property("cogPinned").toBool();
}

CogScriptTabHoverFilter::CogScriptTabHoverFilter(QTabWidget* tabs)
    : QObject(tabs)
    , tabs_(tabs)
{
}

bool CogScriptTabHoverFilter::eventFilter(QObject* watched, QEvent* event)
{
    // Step 1: ignore events that are not from the tab bar we were installed on.
    if (!tabs_ || !tabs_->tabBar() || watched != tabs_->tabBar() || !event)
    {
        return QObject::eventFilter(watched, event);
    }

    const QVariant hoverProperty = tabs_->property("cogHoverIndex");
    int hoverIndex = hoverProperty.isValid() ? hoverProperty.toInt() : -1;
    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const int index = tabs_->tabBar()->tabAt(mouseEvent->pos());
            // Step 2: consume pin-icon clicks so they toggle pin state instead of selecting/closing tabs.
            if (mouseEvent->button() == Qt::LeftButton && index >= 0
                && cogScriptTabPinHitRect(tabs_, index).contains(mouseEvent->pos()))
            {
                if (event->type() == QEvent::MouseButtonRelease)
                {
                    if (QWidget* page = tabs_->widget(index))
                    {
                        page->setProperty("cogPinned", !page->property("cogPinned").toBool());
                        normalizeCogScriptTabOrder(tabs_);
                        refreshCogScriptDockTabPresentation(tabs_);
                        tabs_->setCurrentWidget(page);
                    }
                }
                return true;
            }
            return QObject::eventFilter(watched, event);
        }
        case QEvent::MouseMove:
        {
            // Step 3: remember which tab should show the hover pin icon.
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            hoverIndex = tabs_->tabBar()->tabAt(mouseEvent->pos());
            break;
        }
        case QEvent::Leave:
            hoverIndex = -1;
            break;
        default:
            return QObject::eventFilter(watched, event);
    }

    const int previousHoverIndex = hoverProperty.isValid()
        ? hoverProperty.toInt()
        : -1;
    if ( previousHoverIndex != hoverIndex )
    {
        // Step 4: change only the old and new icons. Refreshing every tab's
        // label makes QTabBar ensure the selected tab is visible again.
        tabs_->setProperty("cogHoverIndex", hoverIndex);
        updateCogScriptDockTabIcon(tabs_, previousHoverIndex);
        updateCogScriptDockTabIcon(tabs_, hoverIndex);
    }
    return QObject::eventFilter(watched, event);
}

} // namespace sed::ui::cog
