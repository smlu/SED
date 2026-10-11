#include "test_common.hpp"

#include "native_caption_test_utils.hpp"

#include "main_window/main_window.hpp"
#include "ui/cog/cog_script_assignment_utils.hpp"
#include "ui/cog/cog_script_diagnostics_utils.hpp"
#include "ui/cog/cog_script_editor_widget.hpp"
#include "ui/cog/cog_script_language_utils.hpp"
#include "ui/cog/cog_script_symbol_utils.hpp"
#include "ui/cog/cog_script_tab_utils.hpp"
#include "ui/components/shared_ui_styles.hpp"
#include "ui/resources/resource_picker.hpp"

#include "sed/editor/document.hpp"
#include "sed/level.hpp"

#include <QApplication>
#include <QAction>
#include <QAbstractButton>
#include <QCheckBox>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCloseEvent>
#include <QCursor>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QEnterEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QGuiApplication>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMainWindow>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QStringList>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QFile>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextOption>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QToolButton>
#include <QTreeWidgetItemIterator>
#include <QTableWidget>
#include <QVector>
#include <QWidget>

#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace sed::app_tests
{
namespace
{

class ModalInvocationGuard final
{
public:
    explicit ModalInvocationGuard(const int deadlineMs = 4000)
        : watchdog_(&context_)
    {
        watchdog_.setSingleShot(true);
        QObject::connect(&watchdog_, &QTimer::timeout, &context_, [this]()
            {
                dismissActivePopupLayers();
            });
        watchdog_.start(deadlineMs);
    }

    ~ModalInvocationGuard()
    {
        watchdog_.stop();
        dismissActivePopupLayers();
    }

    QObject* context() noexcept
    {
        return &context_;
    }

private:
    static void dismissActivePopupLayers()
    {
        if ( QDialog* const dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget()) )
        {
            dialog->reject();
        }
        if ( QWidget* const popup = QApplication::activePopupWidget() )
        {
            popup->close();
        }
    }

    QObject context_{};
    QTimer watchdog_{};
};

QTimer* startInvocationPoller(
    QObject* owner,
    const std::function<bool()>& poll)
{
    if ( !owner )
    {
        return nullptr;
    }

    auto* const timer = new QTimer(owner);
    timer->setInterval(5);
    QObject::connect(timer, &QTimer::timeout, timer, [timer, poll]()
        {
            if ( poll() )
            {
                timer->stop();
            }
        });
    timer->start();
    return timer;
}

int popupAnchorEdgeDistance(const QRect& frame, const QPoint& anchor)
{
    return std::min({
        std::abs(anchor.x() - frame.left()),
        std::abs(anchor.x() - frame.right()),
        std::abs(anchor.y() - frame.top()),
        std::abs(anchor.y() - frame.bottom()) });
}

struct CogScriptTestWindow
{
    std::unique_ptr<MainWindow> window{};
    QTabWidget* tabs{};
    QWidget* page{};
    QPlainTextEdit* editor{};
};

CogScriptTestWindow createCogScriptTestWindow(const QString& scriptName)
{
    CogScriptTestWindow result{};
    result.window = std::make_unique<MainWindow>();
    result.window->setAttribute(Qt::WA_DontShowOnScreen);
    result.window->move(-32'000, -32'000);
    result.window->createNewCogScriptForTest(scriptName);
    result.tabs = result.window->findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    result.page = result.tabs ? result.tabs->currentWidget() : nullptr;
    result.editor = result.page
        ? result.page->findChild<QPlainTextEdit*>()
        : nullptr;
    result.window->show();
    QApplication::processEvents();
    return result;
}

bool settleTabLayout(QTabWidget* tabs)
{
    if ( !tabs )
    {
        return false;
    }

    QSize previousSize{};
    QRect previousTabBarRect{};
    int stablePolls = 0;
    for ( int poll = 0; poll < 16; ++poll )
    {
        QApplication::processEvents();
        const QSize currentSize = tabs->size();
        const QRect currentTabBarRect = tabs->tabBar()->geometry();
        if ( currentSize == previousSize
            && currentTabBarRect == previousTabBarRect
            && !currentSize.isEmpty()
            && !currentTabBarRect.isEmpty() )
        {
            ++stablePolls;
            if ( stablePolls >= 2 )
            {
                return true;
            }
        }
        else
        {
            stablePolls = 0;
            previousSize = currentSize;
            previousTabBarRect = currentTabBarRect;
        }
    }
    return false;
}

CogScriptTestWindow createVisibleCogScriptTestWindow(const QString& scriptName)
{
    CogScriptTestWindow result{};
    result.window = std::make_unique<MainWindow>();
    result.window->setAnimated(false);
    if ( QMainWindow* workspace = result.window->findChild<QMainWindow*>(
            QStringLiteral("workspaceWindow")) )
    {
        workspace->setAnimated(false);
    }
    result.window->resize(920, 680);
    result.window->createNewCogScriptForTest(scriptName);
    result.tabs = result.window->findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    result.page = result.tabs ? result.tabs->currentWidget() : nullptr;
    result.editor = result.page
        ? result.page->findChild<QPlainTextEdit*>()
        : nullptr;
    result.window->show();
    (void)settleTabLayout(result.tabs);
    return result;
}

void setCogScriptEditorSource(
    QPlainTextEdit* editor,
    const QString& source)
{
    if ( !editor )
    {
        return;
    }
    editor->setPlainText(source);
    editor->document()->clearUndoRedoStacks();
    editor->document()->setModified(false);
    QApplication::processEvents();
}

void moveMouseToText(
    QPlainTextEdit* editor,
    const QString& source,
    const QString& needle,
    const int offset = 0)
{
    if ( !editor )
    {
        return;
    }
    const int position = source.indexOf(needle);
    if ( position < 0 )
    {
        return;
    }
    QTextCursor cursor(editor->document());
    cursor.setPosition(position + offset);
    const QRect cursorRect = editor->cursorRect(cursor);
    const QPoint local(cursorRect.left() + 2, cursorRect.center().y());
    const QPoint global = editor->viewport()->mapToGlobal(local);
    QMouseEvent move(
        QEvent::MouseMove,
        QPointF(local),
        QPointF(global),
        QPointF(global),
        Qt::NoButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(editor->viewport(), &move);
    QApplication::processEvents();
}

void testAllTypesMaskDiagnostic()
{
    const QString source = QStringLiteral(
        "symbols\n"
        "thing allTypes=-1 mask=-1\n"
        "thing classic=-1 mask=0xffff\n"
        "thing padded=-1 mask=0x0000FFFF\n"
        "thing engine32=-1 mask=0xFFFFFFFF\n"
        "thing upperPrefix=-1 mask=0XFFFF\n"
        "thing decimal=-1 mask=65535\n"
        "thing oneUnknownBit=-1 mask=0x1000\n"
        "end\n"
        "code\n"
        "end\n");
    const std::vector<ui::cog::CogScriptDiagnostic> diagnostics =
        ui::cog::buildCogScriptDiagnostics(source, {}, ProjectType::JKDF2);

    QStringList maskWarnings{};
    for ( const ui::cog::CogScriptDiagnostic& diagnostic : diagnostics )
    {
        if ( !diagnostic.error
            && diagnostic.message.startsWith(
                QStringLiteral("Attribute 'mask' contains unsupported bits")))
        {
            maskWarnings.push_back(QStringLiteral("%1:%2")
                .arg(diagnostic.line)
                .arg(diagnostic.message));
        }
    }

    check(maskWarnings == QStringList{
            QStringLiteral("6:Attribute 'mask' contains unsupported bits for the current project type: 0xf000."),
            QStringLiteral("7:Attribute 'mask' contains unsupported bits for the current project type: 0x1000.")},
        "cog_diagnostics treats -1 and prefixed hexadecimal all-types masks as sentinels while preserving decimal and unknown-bit warnings");
}

void testHoverDoesNotRecenterCogTabs()
{
    QTabWidget tabs{};
    tabs.resize(360, 180);
    tabs.setTabPosition(QTabWidget::North);
    tabs.tabBar()->setUsesScrollButtons(true);
    tabs.tabBar()->setMouseTracking(true);
    tabs.setProperty("cogHoverIndex", -1);

    for ( int index = 0; index < 24; ++index )
    {
        const QString title = QStringLiteral("long-cog-script-%1.cog")
            .arg(index, 2, 10, QLatin1Char('0'));
        auto* page = new QWidget(&tabs);
        page->setProperty("cogRawTitle", title);
        page->setProperty("cogPinned", false);
        tabs.addTab(page, title);
    }

    auto* hoverFilter = new ui::cog::CogScriptTabHoverFilter(&tabs);
    tabs.tabBar()->installEventFilter(hoverFilter);
    ui::cog::refreshCogScriptDockTabPresentation(&tabs);
    tabs.setCurrentIndex(0);
    tabs.show();
    QApplication::processEvents();

    QToolButton* scrollRight = nullptr;
    for ( QToolButton* button : tabs.tabBar()->findChildren<QToolButton*>() )
    {
        if ( button->arrowType() == Qt::RightArrow )
        {
            scrollRight = button;
            break;
        }
    }

    bool scrolledPastSelected = false;
    const QRect selectedAtStart = tabs.tabBar()->tabRect(0);
    if ( scrollRight && scrollRight->isEnabled() )
    {
        for ( int step = 0; step < 40 && scrollRight->isEnabled(); ++step )
        {
            scrollRight->click();
            QApplication::processEvents();
        }
        scrolledPastSelected = tabs.tabBar()->tabRect(0) != selectedAtStart
            && tabs.tabBar()->tabRect(0).right() < 0;
    }

    QPoint hoverPosition{};
    int hoverIndex = -1;
    const int tabY = tabs.tabBar()->rect().center().y();
    for ( int x = 0; x < tabs.tabBar()->width(); ++x )
    {
        const int index = tabs.tabBar()->tabAt(QPoint(x, tabY));
        if ( index > 0 )
        {
            hoverIndex = index;
            hoverPosition = QPoint(x, tabY);
            break;
        }
    }

    const QRect selectedBeforeHover = tabs.tabBar()->tabRect(0);
    const QRect hoveredBeforeHover = hoverIndex >= 0
        ? tabs.tabBar()->tabRect(hoverIndex) : QRect{};
    if ( hoverIndex >= 0 )
    {
        const QPoint global = tabs.tabBar()->mapToGlobal(hoverPosition);
        QMouseEvent move(
            QEvent::MouseMove,
            QPointF(hoverPosition),
            QPointF(global),
            QPointF(global),
            Qt::NoButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(tabs.tabBar(), &move);
        QApplication::processEvents();
    }

    check(scrollRight && scrolledPastSelected && hoverIndex > 0
            && tabs.currentIndex() == 0
            && tabs.tabBar()->tabRect(0) == selectedBeforeHover
            && tabs.tabBar()->tabRect(hoverIndex) == hoveredBeforeHover,
        "cog_script_tab hover updates preserve the scrolled tab geometry and current selection");
}

QToolButton* tabScrollButton(QTabBar& tabBar, const Qt::ArrowType direction)
{
    for ( QToolButton* button : tabBar.findChildren<QToolButton*>() )
    {
        if ( button->arrowType() == direction )
        {
            return button;
        }
    }

    return nullptr;
}

bool hasVisibleSharedStyleChevron(
    QToolButton* button,
    QTabBar& tabBar,
    const Qt::ArrowType expectedArrow,
    bool* darkRenderedBackdrop = nullptr,
    QString* diagnostics = nullptr)
{
    if ( !button || button->size().isEmpty() || !button->isVisible() )
    {
        if ( diagnostics )
        {
            *diagnostics = QStringLiteral("button missing/hidden/empty");
        }
        return false;
    }

    const Qt::ArrowType originalArrow = button->arrowType();
    if ( originalArrow != expectedArrow )
    {
        if ( diagnostics )
        {
            *diagnostics = QStringLiteral("wrong arrowType=%1 expected=%2")
                .arg(static_cast<int>(originalArrow))
                .arg(static_cast<int>(expectedArrow));
        }
        return false;
    }

    QWidget* const chromeRoot = button->window();
    if ( !chromeRoot || chromeRoot->size().isEmpty() )
    {
        if ( diagnostics )
        {
            *diagnostics = QStringLiteral("chrome root missing or empty");
        }
        return false;
    }

    const QRect buttonGeometryBefore = button->geometry();
    const QRect tabBarGeometryBefore = tabBar.geometry();
    const QSize tabBarSizeBefore = tabBar.size();
    const QRect buttonRectInChrome(
        button->mapTo(chromeRoot, QPoint(0, 0)), button->size());

    QImage withArrow(chromeRoot->size(), QImage::Format_ARGB32_Premultiplied);
    withArrow.fill(Qt::transparent);
    chromeRoot->render(&withArrow);

    QVector<int> backdropLuminances{};
    int darkestBackgroundPixel = 255;
    for ( int y = buttonRectInChrome.top(); y <= buttonRectInChrome.bottom(); ++y )
    {
        for ( int x = buttonRectInChrome.left(); x <= buttonRectInChrome.right(); ++x )
        {
            const bool perimeter = x == buttonRectInChrome.left()
                || x == buttonRectInChrome.right()
                || y == buttonRectInChrome.top()
                || y == buttonRectInChrome.bottom();
            if ( perimeter && withArrow.rect().contains(x, y) )
            {
                const int luminance = qGray(withArrow.pixelColor(x, y).rgb());
                backdropLuminances.push_back(luminance);
                darkestBackgroundPixel = std::min(darkestBackgroundPixel, luminance);
            }
        }
    }
    std::sort(backdropLuminances.begin(), backdropLuminances.end());
    const int backdropLuminance = backdropLuminances.empty()
        ? 255
        : backdropLuminances[backdropLuminances.size() / 2];
    if ( darkRenderedBackdrop )
    {
        *darkRenderedBackdrop = backdropLuminance < 100;
    }

    const QRect centerRegion(
        buttonRectInChrome.center().x() - 7,
        buttonRectInChrome.center().y() - 6,
        15,
        13);
    const QRect sampledRegion = centerRegion.intersected(withArrow.rect());
    const int requiredLuminanceLift = button->isEnabled() ? 72 : 34;
    int readableChevronPixels = 0;
    int maximumLuminanceLift = std::numeric_limits<int>::min();
    int brightestArrowPixel = 0;
    int upperArmPixels = 0;
    int lowerArmPixels = 0;
    int tipPixels = 0;
    for ( int y = sampledRegion.top(); y <= sampledRegion.bottom(); ++y )
    {
        for ( int x = sampledRegion.left(); x <= sampledRegion.right(); ++x )
        {
            const QColor foreground = withArrow.pixelColor(x, y);
            const int foregroundLuminance = qGray(foreground.rgb());
            maximumLuminanceLift = std::max(
                maximumLuminanceLift,
                foregroundLuminance - backdropLuminance);
            brightestArrowPixel = std::max(
                brightestArrowPixel, foregroundLuminance);
            if ( foregroundLuminance
                >= backdropLuminance + requiredLuminanceLift )
            {
                ++readableChevronPixels;
                const int dx = x - buttonRectInChrome.center().x();
                const int dy = y - buttonRectInChrome.center().y();
                if ( expectedArrow == Qt::LeftArrow )
                {
                    if ( dy <= -2 && dx >= 0 )
                    {
                        ++upperArmPixels;
                    }
                    if ( dy >= 2 && dx >= 0 )
                    {
                        ++lowerArmPixels;
                    }
                    if ( std::abs(dy) <= 1 && dx <= -1 )
                    {
                        ++tipPixels;
                    }
                }
                else
                {
                    if ( dy <= -2 && dx <= 0 )
                    {
                        ++upperArmPixels;
                    }
                    if ( dy >= 2 && dx <= 0 )
                    {
                        ++lowerArmPixels;
                    }
                    if ( std::abs(dy) <= 1 && dx >= 1 )
                    {
                        ++tipPixels;
                    }
                }
            }
        }
    }
    const bool hasDirectionalChevronShape = upperArmPixels > 0
        && lowerArmPixels > 0
        && tipPixels > 0;

    const bool geometryStable = buttonGeometryBefore == button->geometry()
        && tabBarGeometryBefore == tabBar.geometry()
        && tabBarSizeBefore == tabBar.size();
    if ( diagnostics )
    {
        const QColor centerWithArrow = withArrow.pixelColor(
            buttonRectInChrome.center());
        *diagnostics = QStringLiteral(
            "type=%1 enabled=%2 hover=%3 visible=%4 iconNull=%5 iconSize=%6x%7 button=%8,%9,%10,%11 medianBackdrop=%12 center=%13,%14,%15 darkestBg=%16 brightestFg=%17 maxLift=%18 readablePixels=%19 arms=%20,%21 tip=%22 shape=%23 geometryStable=%24 style=%25 buttonQss=%26 tabBarQss=%27")
            .arg(static_cast<int>(originalArrow))
            .arg(button->isEnabled())
            .arg(button->underMouse())
            .arg(button->isVisible())
            .arg(button->icon().isNull())
            .arg(button->iconSize().width())
            .arg(button->iconSize().height())
            .arg(buttonGeometryBefore.x())
            .arg(buttonGeometryBefore.y())
            .arg(buttonGeometryBefore.width())
            .arg(buttonGeometryBefore.height())
            .arg(backdropLuminance)
            .arg(centerWithArrow.red())
            .arg(centerWithArrow.green())
            .arg(centerWithArrow.blue())
            .arg(darkestBackgroundPixel)
            .arg(brightestArrowPixel)
            .arg(maximumLuminanceLift)
            .arg(readableChevronPixels)
            .arg(upperArmPixels)
            .arg(lowerArmPixels)
            .arg(tipPixels)
            .arg(hasDirectionalChevronShape)
            .arg(geometryStable)
            .arg(QString::fromLatin1(button->style()->metaObject()->className()))
            .arg(button->styleSheet().replace(QLatin1Char('\n'), QLatin1Char(' ')))
            .arg(tabBar.styleSheet().replace(QLatin1Char('\n'), QLatin1Char(' ')));
    }
    return geometryStable && readableChevronPixels >= 10 && hasDirectionalChevronShape;
}

bool sendHoverToButtonAndCheckState(QToolButton* button)
{
    if ( !button )
    {
        return false;
    }

    if ( QTabBar* const tabBar = button->parentWidget()
            ? qobject_cast<QTabBar*>(button->parentWidget())
            : nullptr )
    {
        for ( QToolButton* const sibling : tabBar->findChildren<QToolButton*>() )
        {
            if ( sibling != button )
            {
                QEvent leave(QEvent::Leave);
                QApplication::sendEvent(sibling, &leave);
                sibling->setAttribute(Qt::WA_UnderMouse, false);
            }
        }
    }

    const QPoint local = button->rect().center();
    const QPoint global = button->mapToGlobal(local);
    QEnterEvent enter{ QPointF(local), QPointF(local), QPointF(global) };
    QApplication::sendEvent(button, &enter);
    QMouseEvent move(
        QEvent::MouseMove,
        QPointF(local),
        QPointF(global),
        QPointF(global),
        Qt::NoButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(button, &move);
    QApplication::processEvents(QEventLoop::AllEvents, 10);
    return button->underMouse();
}

void testTabScrollButtonsShowSharedStyleChevrons()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("tab_arrow_chrome_probe"));
    QTabWidget* tabs = fixture.tabs;
    if ( tabs )
    {
        tabs->tabBar()->setUsesScrollButtons(true);
    }
    for ( int index = 0; index < 20; ++index )
    {
        if ( tabs )
        {
            tabs->addTab(
                new QWidget(tabs),
                QStringLiteral("long-cog-script-%1.cog")
                    .arg(index, 2, 10, QLatin1Char('0')));
        }
    }
    const bool layoutSettled = settleTabLayout(tabs);

    QToolButton* const scrollLeft = tabs
        ? tabScrollButton(*tabs->tabBar(), Qt::LeftArrow) : nullptr;
    QToolButton* const scrollRight = tabs
        ? tabScrollButton(*tabs->tabBar(), Qt::RightArrow) : nullptr;
    const bool startsAtLeft = scrollLeft && scrollRight
        && !scrollLeft->isEnabled()
        && scrollRight->isEnabled();
    const bool scrollButtonsDoNotOverlap = scrollLeft && scrollRight
        && !scrollLeft->geometry().intersects(scrollRight->geometry());
    const bool enabledArrowHovered = sendHoverToButtonAndCheckState(scrollRight);
    bool disabledLeftBackdropIsDark = false;
    bool enabledRightBackdropIsDark = false;
    QString disabledLeftDiagnostics{};
    QString enabledRightDiagnostics{};
    const bool disabledLeftChevronVisible = tabs
        && hasVisibleSharedStyleChevron(
            scrollLeft,
            *tabs->tabBar(),
            Qt::LeftArrow,
            &disabledLeftBackdropIsDark,
            &disabledLeftDiagnostics);
    const bool enabledRightChevronVisible = tabs
        && hasVisibleSharedStyleChevron(
            scrollRight,
            *tabs->tabBar(),
            Qt::RightArrow,
            &enabledRightBackdropIsDark,
            &enabledRightDiagnostics);
    const bool initialArrowsVisible = startsAtLeft
        && scrollButtonsDoNotOverlap
        && enabledArrowHovered
        && disabledLeftChevronVisible
        && enabledRightChevronVisible;

    bool reachesRightEnd = false;
    if ( scrollRight )
    {
        for ( int step = 0; step < 60 && scrollRight->isEnabled(); ++step )
        {
            scrollRight->click();
            QApplication::processEvents();
        }
        reachesRightEnd = !scrollRight->isEnabled();
    }
    const bool reversedArrowHovered = sendHoverToButtonAndCheckState(scrollLeft);
    bool enabledLeftBackdropIsDark = false;
    bool disabledRightBackdropIsDark = false;
    QString enabledLeftDiagnostics{};
    QString disabledRightDiagnostics{};
    const bool enabledLeftChevronVisible = tabs
        && hasVisibleSharedStyleChevron(
            scrollLeft,
            *tabs->tabBar(),
            Qt::LeftArrow,
            &enabledLeftBackdropIsDark,
            &enabledLeftDiagnostics);
    const bool disabledRightChevronVisible = tabs
        && hasVisibleSharedStyleChevron(
            scrollRight,
            *tabs->tabBar(),
            Qt::RightArrow,
            &disabledRightBackdropIsDark,
            &disabledRightDiagnostics);
    const bool endArrowsVisible = reachesRightEnd
        && scrollLeft->isEnabled()
        && reversedArrowHovered
        && enabledLeftChevronVisible
        && disabledRightChevronVisible;

    const bool usesApplicationChrome = fixture.window && tabs
        && fixture.window->objectName() == QStringLiteral("sed_main_window")
        && qApp->property("sed.globalComponentStylesApplied").toBool()
        && disabledLeftBackdropIsDark
        && enabledRightBackdropIsDark
        && enabledLeftBackdropIsDark
        && disabledRightBackdropIsDark;
    if ( !usesApplicationChrome || !initialArrowsVisible || !endArrowsVisible )
    {
        std::cerr
            << QStringLiteral(
                "tab-chevron diagnostic chrome=%1 layout=%2 startsAtLeft=%3 buttonsDoNotOverlap=%4 enabledHover=%5 reachesEnd=%6 reversedHover=%7 backdrop=%8,%9,%10,%11 initialVisible=%12 endVisible=%13 leftDisabled={%14} rightEnabled={%15} leftEnabled={%16} rightDisabled={%17}")
                .arg(usesApplicationChrome)
                .arg(layoutSettled)
                .arg(startsAtLeft)
                .arg(scrollButtonsDoNotOverlap)
                .arg(enabledArrowHovered)
                .arg(reachesRightEnd)
                .arg(reversedArrowHovered)
                .arg(disabledLeftBackdropIsDark)
                .arg(enabledRightBackdropIsDark)
                .arg(enabledLeftBackdropIsDark)
                .arg(disabledRightBackdropIsDark)
                .arg(initialArrowsVisible)
                .arg(endArrowsVisible)
                .arg(disabledLeftDiagnostics)
                .arg(enabledRightDiagnostics)
                .arg(enabledLeftDiagnostics)
                .arg(disabledRightDiagnostics)
                .toStdString()
            << '\n';
    }
    check(usesApplicationChrome && layoutSettled && startsAtLeft
            && scrollButtonsDoNotOverlap,
        "cog_tabs uses the actual dark MainWindow chrome and creates an enabled right and disabled left scroll arrow");
    check(initialArrowsVisible,
        "cog_tabs renders visible shared-style chevrons for both enabled and disabled scroll arrows");
    check(reachesRightEnd,
        "cog_tabs can scroll to the final tab");
    check(endArrowsVisible,
        "cog_tabs renders visible shared-style chevrons after the scroll-arrow enabled states reverse");
}

void testConciseResourcePickerTitles()
{
    static_assert(static_cast<std::uint8_t>(ui::ResourcePickerKind::uni) == 13);
    check(ui::resourcePickerTitle(ui::ResourcePickerKind::wav) == "Sound Picker"
            && ui::resourcePickerTitle(ui::ResourcePickerKind::template_)
                == "Template Picker"
            && ui::resourcePickerTitle(ui::ResourcePickerKind::material)
                == "Material Picker"
            && ui::resourcePickerTitle(ui::ResourcePickerKind::uni)
                == "String Table Picker",
        "resource_picker uses concise shared titles for sound, template, material, and string-table resources");
}

struct MaskPopupObservation
{
    bool actionFound{};
    bool dialogOpened{};
    bool interactionAccepted{};
    bool popupFrameless{};
    bool nearMouseAnchor{};
    bool insideScreen{};
    QPoint requestedAnchor{};
    QPoint anchorEventGlobalPosition{};
    QPoint cursorAtActionDispatch{};
    QPoint cursorAtPopupOpen{};
    QPoint expectedFrameTopLeft{};
    QRect popupFrame{};
    QRect availableScreen{};
};

MaskPopupObservation invokeMaskEditor(
    QPlainTextEdit* editor,
    const QString& source,
    const QString& attributeValueToken,
    const std::function<void(QDialog*)>& interact,
    const std::optional<QPoint> globalAnchorOverride = std::nullopt)
{
    MaskPopupObservation observation{};
    if ( !editor )
    {
        return observation;
    }

    const int targetPosition = source.indexOf(attributeValueToken);
    if ( targetPosition < 0 )
    {
        return observation;
    }

    QTextCursor targetCursor(editor->document());
    const int valueOffset = attributeValueToken.indexOf(QLatin1Char('=')) + 2;
    targetCursor.setPosition(targetPosition + std::max(1, valueOffset));
    const QRect cursorRect = editor->cursorRect(targetCursor);
    const QPoint localPosition(cursorRect.left() + 2, cursorRect.center().y());
    const QPoint globalPosition = globalAnchorOverride.value_or(
        editor->viewport()->mapToGlobal(localPosition));
    observation.requestedAnchor = globalPosition;
    ModalInvocationGuard invocation{};
    startInvocationPoller(invocation.context(),
        [&observation, &invocation, editor, globalPosition, &interact]()
        {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if ( !menu )
            {
                return false;
            }

            QAction* editMaskAction = nullptr;
            for ( QAction* action : menu->actions() )
            {
                if ( action && action->text() == QStringLiteral("Edit mask...") )
                {
                    editMaskAction = action;
                    break;
                }
            }
            observation.actionFound = editMaskAction != nullptr;
            if ( !editMaskAction )
            {
                menu->close();
                return true;
            }

            const std::function<void(QDialog*)> interactCopy = interact;
            startInvocationPoller(invocation.context(),
                [&observation, globalPosition, interactCopy]()
                {
                    auto* dialog = qobject_cast<QDialog*>(
                        QApplication::activeModalWidget());
                    observation.dialogOpened = dialog
                        && dialog->windowTitle() == QStringLiteral("Select mask");
                    if ( !dialog )
                    {
                        return false;
                    }

                    const Qt::WindowFlags flags = dialog->windowFlags();
                    observation.popupFrameless = flags.testFlag(Qt::Popup)
                        && flags.testFlag(Qt::FramelessWindowHint);
                    const QRect frame = dialog->frameGeometry();
                    observation.popupFrame = frame;
                    observation.cursorAtPopupOpen = QCursor::pos();
                    QScreen* screen = QGuiApplication::screenAt(globalPosition);
                    if ( !screen )
                    {
                        screen = QGuiApplication::primaryScreen();
                    }
                    observation.availableScreen = screen
                        ? screen->availableGeometry() : QRect{};
                    if ( screen )
                    {
                        const QRect safe = screen->availableGeometry().adjusted(
                            6, 6, -6, -6);
                        const int maximumX = std::max(
                            safe.left(), safe.right() - frame.width() + 1);
                        const int maximumY = std::max(
                            safe.top(), safe.bottom() - frame.height() + 1);
                        observation.expectedFrameTopLeft = QPoint(
                            std::clamp(
                                globalPosition.x() + 8,
                                safe.left(),
                                maximumX),
                            std::clamp(
                                globalPosition.y() + 8,
                                safe.top(),
                                maximumY));
                        observation.nearMouseAnchor =
                            (frame.topLeft()
                                - observation.expectedFrameTopLeft)
                                .manhattanLength() <= 3;
                    }
                    observation.insideScreen = screen
                        && screen->availableGeometry().contains(frame);
                    const QPointer<QDialog> guard(dialog);
                    interactCopy(dialog);
                    observation.interactionAccepted = guard
                        && guard->result() == QDialog::Accepted;
                    return true;
                });

            observation.cursorAtActionDispatch = QCursor::pos();
            editMaskAction->trigger();
            // QAction::trigger() does not select an item in QMenu::exec(),
            // so explicitly close the outer menu after the inner dialog exits.
            menu->close();
            return true;
        });

    QContextMenuEvent contextEvent(
        QContextMenuEvent::Mouse,
        localPosition,
        globalPosition,
        Qt::NoModifier);
    observation.anchorEventGlobalPosition = contextEvent.globalPos();
    QApplication::sendEvent(editor->viewport(), &contextEvent);
    QApplication::processEvents();
    return observation;
}

QAction* prepareSymbolAttributeMenuAction(
    QPlainTextEdit* editor,
    const QString& actionText)
{
    if ( !editor )
    {
        return nullptr;
    }
    QMenu* const insertMenu = editor->window()->findChild<QMenu*>(
        QStringLiteral("cogEditorInsertMenu"));
    if ( !insertMenu
        || !QMetaObject::invokeMethod(
            insertMenu, "aboutToShow", Qt::DirectConnection) )
    {
        return nullptr;
    }
    QMenu* attributesMenu = nullptr;
    for ( QMenu* menu : insertMenu->findChildren<QMenu*>() )
    {
        if ( menu && menu->title() == QStringLiteral("Symbol attribute") )
        {
            attributesMenu = menu;
            break;
        }
    }
    if ( !attributesMenu
        || !QMetaObject::invokeMethod(
            attributesMenu, "aboutToShow", Qt::DirectConnection) )
    {
        return nullptr;
    }
    for ( QAction* action : attributesMenu->actions() )
    {
        if ( action && action->text() == actionText )
        {
            return action;
        }
    }
    return nullptr;
}

MaskPopupObservation invokeMaskAttributeAction(
    QPlainTextEdit* editor,
    QAction* action,
    const std::function<void(QDialog*)>& interact)
{
    MaskPopupObservation observation{};
    if ( !editor || !action )
    {
        return observation;
    }
    observation.actionFound = true;
    ModalInvocationGuard invocation{};
    const std::function<void(QDialog*)> interactCopy = interact;
    startInvocationPoller(invocation.context(),
        [&observation, interactCopy]()
        {
            auto* dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget());
            observation.dialogOpened = dialog
                && dialog->windowTitle() == QStringLiteral("Select mask");
            if ( !dialog )
            {
                return false;
            }
            const Qt::WindowFlags flags = dialog->windowFlags();
            observation.popupFrameless = flags.testFlag(Qt::Popup)
                && flags.testFlag(Qt::FramelessWindowHint);
            const QPointer<QDialog> dialogGuard(dialog);
            interactCopy(dialog);
            observation.interactionAccepted = dialogGuard
                && dialogGuard->result() == QDialog::Accepted;
            return true;
        });
    action->trigger();
    return observation;
}

QCheckBox* checkBoxStartingWith(QDialog* dialog, const QString& prefix)
{
    if ( !dialog )
    {
        return nullptr;
    }

    for ( QCheckBox* checkBox : dialog->findChildren<QCheckBox*>() )
    {
        if ( checkBox->text().startsWith(prefix, Qt::CaseInsensitive) )
        {
            return checkBox;
        }
    }
    return nullptr;
}

void acceptDialog(QDialog* dialog)
{
    QDialogButtonBox* buttons = dialog
        ? dialog->findChild<QDialogButtonBox*>() : nullptr;
    if ( buttons && buttons->button(QDialogButtonBox::Ok) )
    {
        buttons->button(QDialogButtonBox::Ok)->click();
    }
    else if ( dialog )
    {
        dialog->accept();
    }
}

void cancelDialog(QDialog* dialog)
{
    if ( dialog )
    {
        dialog->reject();
    }
}

struct FlagsPopupObservation
{
    bool buttonVisibleBeforeClick{};
    bool dialogOpened{};
    bool interactionAccepted{};
    bool popupFrameless{};
    bool nearMouseAnchor{};
    bool insideScreen{};
    QPoint requestedAnchor{};
    QPoint clickEventGlobalPosition{};
    QPoint cursorAtActionDispatch{};
    QPoint cursorAtPopupOpen{};
    QRect popupFrame{};
    QRect availableScreen{};
};

FlagsPopupObservation invokeFlagsEditor(
    QPlainTextEdit* editor,
    const QString& source,
    const QString& targetToken,
    const std::function<void(QDialog*)>& interact,
    const std::optional<QPoint> globalAnchorOverride = std::nullopt)
{
    FlagsPopupObservation observation{};
    if ( !editor )
    {
        return observation;
    }

    moveMouseToText(editor, source, targetToken, targetToken.size() - 1);
    QToolButton* button = editor->findChild<QToolButton*>(
        QStringLiteral("cogInlineAssignmentPickerButton"));
    observation.buttonVisibleBeforeClick = button && button->isVisible();
    if ( !observation.buttonVisibleBeforeClick )
    {
        return observation;
    }

    const QPoint anchor = globalAnchorOverride.value_or(
        button->mapToGlobal(button->rect().center()));
    observation.requestedAnchor = anchor;
    ModalInvocationGuard invocation{};
    const std::function<void(QDialog*)> interactCopy = interact;
    startInvocationPoller(invocation.context(),
        [&observation, anchor, interactCopy]()
        {
            auto* dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget());
            observation.dialogOpened = dialog
                && dialog->windowTitle() == QStringLiteral("Edit COG Flags");
            if ( !dialog )
            {
                return false;
            }

            const Qt::WindowFlags flags = dialog->windowFlags();
            observation.popupFrameless = flags.testFlag(Qt::Popup)
                && flags.testFlag(Qt::FramelessWindowHint);
            const QRect frame = dialog->frameGeometry();
            observation.popupFrame = frame;
            observation.cursorAtPopupOpen = QCursor::pos();
            observation.nearMouseAnchor = frame.adjusted(
                -8, -8, 8, 8).contains(anchor)
                && popupAnchorEdgeDistance(frame, anchor) <= 18;
            QScreen* screen = QGuiApplication::screenAt(anchor);
            if ( !screen )
            {
                screen = QGuiApplication::primaryScreen();
            }
            observation.insideScreen = screen
                && screen->availableGeometry().contains(frame);
            observation.availableScreen = screen ? screen->availableGeometry() : QRect{};
            const QPointer<QDialog> guard(dialog);
            interactCopy(dialog);
            observation.interactionAccepted = guard
                && guard->result() == QDialog::Accepted;
            return true;
        });
    const QPoint local = button->rect().center();
    const QPoint scene = button->mapTo(button->window(), local);
    QMouseEvent press(
        QEvent::MouseButtonPress,
        QPointF(local),
        QPointF(scene),
        QPointF(anchor),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QMouseEvent release(
        QEvent::MouseButtonRelease,
        QPointF(local),
        QPointF(scene),
        QPointF(anchor),
        Qt::LeftButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(button, &press);
    observation.cursorAtActionDispatch = QCursor::pos();
    observation.clickEventGlobalPosition = release.globalPosition().toPoint();
    QApplication::sendEvent(button, &release);
    return observation;
}

FlagsPopupObservation invokeFlagsEditorFromInsertMenu(
    QPlainTextEdit* editor,
    const std::function<void(QDialog*)>& interact)
{
    FlagsPopupObservation observation{};
    if ( !editor )
    {
        return observation;
    }
    QMenu* const insertMenu = editor->window()->findChild<QMenu*>(
        QStringLiteral("cogEditorInsertMenu"));
    QAction* const flagsAction = insertMenu
        ? insertMenu->findChild<QAction*>(QStringLiteral("cogEditorFlagsAction"))
        : nullptr;
    if ( !flagsAction )
    {
        return observation;
    }

    ModalInvocationGuard invocation{};
    const std::function<void(QDialog*)> interactCopy = interact;
    startInvocationPoller(invocation.context(), [&observation, interactCopy]()
        {
            auto* dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget());
            observation.dialogOpened = dialog
                && dialog->windowTitle() == QStringLiteral("Edit COG Flags");
            if ( !dialog )
            {
                return false;
            }
            const QPointer<QDialog> dialogGuard(dialog);
            interactCopy(dialog);
            observation.interactionAccepted = dialogGuard
                && dialogGuard->result() == QDialog::Accepted;
            return true;
        });
    flagsAction->trigger();
    return observation;
}

void testFlagsPopupAnchorCancelNoOpAndUndo()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("flags_popup_probe"));
    QPlainTextEdit* editor = fixture.editor;
    const QString source = QStringLiteral(
        "flags=0x00000003\n"
        "symbols\n"
        "int trigger\n"
        "end\n"
        "code\n"
        "end\n");

    setCogScriptEditorSource(editor, source);
    const int beforeCancelRevision = editor
        ? editor->document()->revision() : -1;
    const FlagsPopupObservation cancelObservation = invokeFlagsEditor(
        editor, source, QStringLiteral("flags=0x00000003"), cancelDialog);
    const bool cancelPreservedSource = editor
        && editor->toPlainText() == source
        && editor->document()->revision() == beforeCancelRevision
        && !editor->document()->isModified();

    setCogScriptEditorSource(editor, source);
    const int beforeNoOpRevision = editor
        ? editor->document()->revision() : -1;
    const FlagsPopupObservation noOpObservation = invokeFlagsEditor(
        editor,
        source,
        QStringLiteral("flags=0x00000003"),
        acceptDialog);
    const bool noOpPreservedSpelling = editor
        && editor->toPlainText() == source
        && editor->document()->revision() == beforeNoOpRevision
        && !editor->document()->isModified()
        && !editor->document()->isUndoAvailable();

    QScreen* const primaryScreen = QGuiApplication::primaryScreen();
    const QPoint clampAnchor = primaryScreen
        ? primaryScreen->availableGeometry().bottomRight() - QPoint(2, 2)
        : QPoint{};
    const FlagsPopupObservation edgeClampObservation = invokeFlagsEditor(
        editor,
        source,
        QStringLiteral("flags=0x00000003"),
        cancelDialog,
        clampAnchor);

    setCogScriptEditorSource(editor, source);
    (void)invokeFlagsEditor(
        editor,
        source,
        QStringLiteral("flags=0x00000003"),
        [](QDialog* dialog)
        {
            if ( QCheckBox* disabled = checkBoxStartingWith(
                    dialog, QStringLiteral("Disabled")) )
            {
                disabled->setChecked(false);
            }
            acceptDialog(dialog);
        });
    const QString changedSource = QStringLiteral(
        "flags=0x00000001\n"
        "symbols\n"
        "int trigger\n"
        "end\n"
        "code\n"
        "end\n");
    const bool oneUndoRedo = editor
        && editor->toPlainText() == changedSource
        && editor->document()->isUndoAvailable()
        && (editor->undo(), editor->toPlainText() == source)
        && (editor->redo(), editor->toPlainText() == changedSource);

    check(cancelObservation.buttonVisibleBeforeClick
            && cancelObservation.dialogOpened
            && cancelObservation.popupFrameless
            && cancelObservation.nearMouseAnchor
            && cancelObservation.insideScreen
            && cancelObservation.clickEventGlobalPosition
                == cancelObservation.requestedAnchor
            && cancelPreservedSource,
        "COG flags editor opens as a screen-clamped popup anchored to the clicked inline button, and Cancel preserves the source");
    check(noOpPreservedSpelling,
        "accepting unchanged COG flags opens and accepts the popup while preserving spelling, dirty state, and undo history");
    check(noOpObservation.dialogOpened && noOpObservation.interactionAccepted,
        "COG flags no-op regression actually accepts the flags popup");
    check(edgeClampObservation.dialogOpened
            && edgeClampObservation.nearMouseAnchor
            && edgeClampObservation.clickEventGlobalPosition
                == edgeClampObservation.requestedAnchor
            && edgeClampObservation.insideScreen,
        "COG flags popup clamps to the available screen edge while remaining mouse-anchored");
    check(oneUndoRedo,
        "accepted COG flags changes remain a single undoable edit");
    if ( !cancelObservation.nearMouseAnchor || !edgeClampObservation.nearMouseAnchor )
    {
        std::cerr
            << "flags-popup-anchor requested=" << cancelObservation.requestedAnchor.x() << ','
            << cancelObservation.requestedAnchor.y() << " eventGlobal="
            << cancelObservation.clickEventGlobalPosition.x() << ','
            << cancelObservation.clickEventGlobalPosition.y() << " cursor="
            << cancelObservation.cursorAtActionDispatch.x() << ','
            << cancelObservation.cursorAtActionDispatch.y() << " openedCursor="
            << cancelObservation.cursorAtPopupOpen.x() << ','
            << cancelObservation.cursorAtPopupOpen.y() << " frame="
            << cancelObservation.popupFrame.x() << ','
            << cancelObservation.popupFrame.y() << ','
            << cancelObservation.popupFrame.width() << ','
            << cancelObservation.popupFrame.height() << " edgeFrame="
            << edgeClampObservation.popupFrame.x() << ','
            << edgeClampObservation.popupFrame.y() << ','
            << edgeClampObservation.popupFrame.width() << ','
            << edgeClampObservation.popupFrame.height() << " available="
            << cancelObservation.availableScreen.x() << ','
            << cancelObservation.availableScreen.y() << ','
            << cancelObservation.availableScreen.width() << ','
            << cancelObservation.availableScreen.height() << '\n';
    }
}

void testFlagsAcceptRepairsInvalidAndAddsMissingRecord()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("flags_default_probe"));
    const QString symbolsSource = QStringLiteral(
        "symbols\nthing actor\nend\ncode\nend\n");
    setCogScriptEditorSource(fixture.editor, symbolsSource);
    const FlagsPopupObservation missingObservation =
        invokeFlagsEditorFromInsertMenu(fixture.editor, acceptDialog);
    const QString missingExpected = QStringLiteral(
        "flags=0x00000000\nsymbols\nthing actor\nend\ncode\nend\n");
    const bool missingRecordWasAdded = fixture.editor
        && fixture.editor->toPlainText() == missingExpected
        && fixture.editor->document()->isUndoAvailable();

    const QString invalidSource = QStringLiteral(
        "flags=oops\nsymbols\nthing actor\nend\ncode\nend\n");
    setCogScriptEditorSource(fixture.editor, invalidSource);
    const FlagsPopupObservation invalidObservation = invokeFlagsEditor(
        fixture.editor,
        invalidSource,
        QStringLiteral("flags=oops"),
        acceptDialog);
    const QString invalidExpected = QStringLiteral(
        "flags=0x00000000\nsymbols\nthing actor\nend\ncode\nend\n");
    check(missingObservation.dialogOpened
            && missingObservation.interactionAccepted
            && missingRecordWasAdded,
        "accepting default COG flags creates a missing flags record as one undoable edit");
    check(invalidObservation.dialogOpened
            && invalidObservation.interactionAccepted
            && fixture.editor
            && fixture.editor->toPlainText() == invalidExpected,
        "accepting default COG flags repairs an invalid authored value instead of preserving it as a no-op");
}

void testMaskPopupPreservesTargetTokenAndEngineBits()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("mask_popup_probe"));
    QPlainTextEdit* editor = fixture.editor;
    const QString scriptTemplate = QStringLiteral(
        "symbols\n"
        "thing actor mask=%1\n"
        "end\n"
        "code\n"
        "end\n");

    const QString sentinelSource = scriptTemplate.arg(QStringLiteral("-1"));
    setCogScriptEditorSource(editor, sentinelSource);
    const int beforeCancelRevision = editor
        ? editor->document()->revision() : -1;
    MaskPopupObservation cancelObservation = invokeMaskEditor(
        editor,
        sentinelSource,
        QStringLiteral("mask=-1"),
        cancelDialog);
    const bool cancelPreservedAuthoredToken = editor
        && editor->toPlainText() == sentinelSource
        && editor->document()->revision() == beforeCancelRevision
        && !editor->document()->isModified();

    QScreen* const primaryScreen = QGuiApplication::primaryScreen();
    const QPoint clampAnchor = primaryScreen
        ? primaryScreen->availableGeometry().bottomRight() - QPoint(2, 2)
        : QPoint{};
    setCogScriptEditorSource(editor, sentinelSource);
    const MaskPopupObservation edgeClampObservation = invokeMaskEditor(
        editor,
        sentinelSource,
        QStringLiteral("mask=-1"),
        cancelDialog,
        clampAnchor);
    if ( edgeClampObservation.dialogOpened
        && !edgeClampObservation.nearMouseAnchor )
    {
        std::cerr
            << "mask-popup-edge requested="
            << edgeClampObservation.requestedAnchor.x() << ','
            << edgeClampObservation.requestedAnchor.y() << " eventGlobal="
            << edgeClampObservation.anchorEventGlobalPosition.x() << ','
            << edgeClampObservation.anchorEventGlobalPosition.y() << " cursor="
            << edgeClampObservation.cursorAtActionDispatch.x() << ','
            << edgeClampObservation.cursorAtActionDispatch.y() << " openedCursor="
            << edgeClampObservation.cursorAtPopupOpen.x() << ','
            << edgeClampObservation.cursorAtPopupOpen.y() << " frame="
            << edgeClampObservation.popupFrame.x() << ','
            << edgeClampObservation.popupFrame.y() << ','
            << edgeClampObservation.popupFrame.width() << ','
            << edgeClampObservation.popupFrame.height() << " expected="
            << edgeClampObservation.expectedFrameTopLeft.x() << ','
            << edgeClampObservation.expectedFrameTopLeft.y() << " available="
            << edgeClampObservation.availableScreen.x() << ','
            << edgeClampObservation.availableScreen.y() << ','
            << edgeClampObservation.availableScreen.width() << ','
            << edgeClampObservation.availableScreen.height() << '\n';
    }

    setCogScriptEditorSource(editor, sentinelSource);
    const int beforeNoOpRevision = editor
        ? editor->document()->revision() : -1;
    MaskPopupObservation noOpObservation = invokeMaskEditor(
        editor,
        sentinelSource,
        QStringLiteral("mask=-1"),
        acceptDialog);
    const bool noOpPreservedAuthoredToken = editor
        && editor->toPlainText() == sentinelSource
        && editor->document()->revision() == beforeNoOpRevision
        && !editor->document()->isModified()
        && !editor->document()->isUndoAvailable();

    const QString allMaskSource = scriptTemplate.arg(QStringLiteral("-1"));
    setCogScriptEditorSource(editor, allMaskSource);
    MaskPopupObservation allMaskObservation = invokeMaskEditor(
        editor,
        allMaskSource,
        QStringLiteral("mask=-1"),
        [](QDialog* dialog)
        {
            if ( QCheckBox* player = checkBoxStartingWith(
                    dialog, QStringLiteral("Player")) )
            {
                player->setChecked(false);
            }
            acceptDialog(dialog);
        });
    const QString allMaskExpected = scriptTemplate.arg(
        QStringLiteral("0xfffffbff"));
    const bool allMaskRawBitsPreserved = editor
        && editor->toPlainText() == allMaskExpected;
    const bool allMaskOneUndoAndRedo = editor
        && editor->document()->isModified()
        && editor->document()->isUndoAvailable()
        && (editor->undo(), editor->toPlainText() == allMaskSource)
        && (editor->redo(), editor->toPlainText() == allMaskExpected);

    const QString classicMaskSource = scriptTemplate.arg(
        QStringLiteral("0xffff"));
    setCogScriptEditorSource(editor, classicMaskSource);
    (void)invokeMaskEditor(
        editor,
        classicMaskSource,
        QStringLiteral("mask=0xffff"),
        [](QDialog* dialog)
        {
            if ( QCheckBox* player = checkBoxStartingWith(
                    dialog, QStringLiteral("Player")) )
            {
                player->setChecked(false);
            }
            acceptDialog(dialog);
        });
    const bool classicUnknownBitsPreserved = editor
        && editor->toPlainText() == scriptTemplate.arg(
            QStringLiteral("0xfbff"));

    const QString highBitSource = scriptTemplate.arg(
        QStringLiteral("0x80000000"));
    setCogScriptEditorSource(editor, highBitSource);
    (void)invokeMaskEditor(
        editor,
        highBitSource,
        QStringLiteral("mask=0x80000000"),
        [](QDialog* dialog)
        {
            if ( QCheckBox* system = checkBoxStartingWith(
                    dialog, QStringLiteral("System")) )
            {
                system->setChecked(true);
            }
            acceptDialog(dialog);
        });
    const bool unknownHighBitPreserved = editor
        && editor->toPlainText() == scriptTemplate.arg(
            QStringLiteral("0x80000001"));

    check(cancelObservation.dialogOpened && cancelObservation.popupFrameless
            && cancelObservation.nearMouseAnchor
            && cancelObservation.anchorEventGlobalPosition
                == cancelObservation.requestedAnchor
            && cancelObservation.insideScreen
            && cancelPreservedAuthoredToken,
        "COG mask popup opens beside the cursor with a screen-clamped frameless popup, and Cancel preserves the exact source");
    check(edgeClampObservation.dialogOpened
            && edgeClampObservation.nearMouseAnchor
            && edgeClampObservation.anchorEventGlobalPosition
                == edgeClampObservation.requestedAnchor
            && edgeClampObservation.insideScreen,
        "COG mask popup clamps to the available screen edge while remaining mouse-anchored");
    check(noOpObservation.actionFound && noOpObservation.dialogOpened
            && noOpObservation.interactionAccepted
            && noOpPreservedAuthoredToken,
        "accepting an unchanged COG mask preserves the authored -1 token without dirtying or adding an edit");
    check(allMaskObservation.dialogOpened && allMaskRawBitsPreserved
            && allMaskOneUndoAndRedo,
        "COG mask editing preserves all 32 engine sentinel bits and applies Player removal as one undoable change");
    check(classicUnknownBitsPreserved,
        "COG mask editing preserves unrepresented high bits from a 0xffff token");
    check(unknownHighBitPreserved,
        "COG mask editing preserves an unknown high bit when enabling System");
}

void testMaskPopupUsesExactDuplicateAttributeSpan()
{
    const QString source = QStringLiteral(
        "symbols\n"
        "thing actor mask=0xffff mask=0x80000000\n"
        "end\n");
    const QString line = QStringLiteral(
        "thing actor mask=0xffff mask=0x80000000");
    const auto context = ui::cog::cogScriptSymbolLineContextAtLine(source, 1);
    const int valueStart = line.lastIndexOf(QStringLiteral("0x80000000"));
    const auto target = context
        ? ui::cog::cogScriptSymbolLineAttributeAtColumn(
            source, *context, valueStart + 2)
        : std::nullopt;

    const QString quotedDescription = QStringLiteral(
        "thing actor desc=\"keep mask=0x80000000 in this text\"");
    const auto quotedDescriptionMask = ui::cog::cogScriptInlineAssignmentAtColumn(
        quotedDescription,
        ui::cog::CogScriptEditorSection::symbols,
        quotedDescription.indexOf(QStringLiteral("mask=0x")) + 6);
    const QString quotedResource = QStringLiteral(
        "thing actor resource=\"maps/mask=-1.cog\"");
    const auto quotedResourceMask = ui::cog::cogScriptInlineAssignmentAtColumn(
        quotedResource,
        ui::cog::CogScriptEditorSection::symbols,
        quotedResource.indexOf(QStringLiteral("mask=-1")) + 5);
    const QString missingResourceValue = QStringLiteral(
        "thing actor sound= mask=-1");
    const auto missingResourceMask = ui::cog::cogScriptInlineAssignmentAtColumn(
        missingResourceValue,
        ui::cog::CogScriptEditorSection::symbols,
        missingResourceValue.indexOf(QStringLiteral("mask=-1")) + 5);
    const QString malformedMask = QStringLiteral("thing actor mask=\"unfinished");
    const auto malformedMaskSpan = ui::cog::cogScriptInlineAssignmentAtColumn(
        malformedMask,
        ui::cog::CogScriptEditorSection::symbols,
        malformedMask.indexOf(QStringLiteral("unfinished")));
    const QString commentMask = QStringLiteral("thing actor mask=0 # mask=-1");
    const auto commentMaskSpan = ui::cog::cogScriptInlineAssignmentAtColumn(
        commentMask,
        ui::cog::CogScriptEditorSection::symbols,
        commentMask.indexOf(QStringLiteral("mask=-1")) + 5);
    const auto codeMaskSpan = ui::cog::cogScriptInlineAssignmentAtColumn(
        QStringLiteral("mask=-1"),
        ui::cog::CogScriptEditorSection::code,
        5);

    check(target
            && target->name == QStringLiteral("mask")
            && target->value == QStringLiteral("0x80000000")
            && target->valueStart == valueStart
            && target->valueLength == 10,
        "COG mask editing resolves the clicked duplicate attribute to its exact authored value span");
    check(!quotedDescriptionMask && !quotedResourceMask && !missingResourceMask
            && !malformedMaskSpan && !commentMaskSpan && !codeMaskSpan,
        "COG inline mask targeting ignores quoted metadata and resources, missing initializer values, malformed values, comments, and code");
}

void testMaskInsertMenuAddAndToolbarFallback()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("mask_insert_menu_probe"));
    const QString addSource = QStringLiteral(
        "symbols\nthing actor\nend\ncode\nend\n");
    setCogScriptEditorSource(fixture.editor, addSource);
    QTextCursor actorCursor(fixture.editor->document());
    actorCursor.setPosition(addSource.indexOf(QStringLiteral("actor")) + 2);
    fixture.editor->setTextCursor(actorCursor);
    QAction* const addAction = prepareSymbolAttributeMenuAction(
        fixture.editor, QStringLiteral("Add mask..."));
    const MaskPopupObservation addObservation = invokeMaskAttributeAction(
        fixture.editor, addAction, acceptDialog);
    const QString addExpected = QStringLiteral(
        "symbols\nthing actor mask=0x401\nend\ncode\nend\n");
    const bool addUndoRedo = fixture.editor
        && fixture.editor->toPlainText() == addExpected
        && fixture.editor->document()->isUndoAvailable()
        && (fixture.editor->undo(), fixture.editor->toPlainText() == addSource)
        && (fixture.editor->redo(), fixture.editor->toPlainText() == addExpected);

    const QString editSource = QStringLiteral(
        "symbols\nthing actor mask=0xffff mask=0x80000000\nend\ncode\nend\n");
    setCogScriptEditorSource(fixture.editor, editSource);
    QTextCursor declarationCursor(fixture.editor->document());
    declarationCursor.setPosition(editSource.indexOf(QStringLiteral("actor")) + 2);
    fixture.editor->setTextCursor(declarationCursor);
    QAction* const editAction = prepareSymbolAttributeMenuAction(
        fixture.editor, QStringLiteral("Edit mask..."));
    const MaskPopupObservation editObservation = invokeMaskAttributeAction(
        fixture.editor,
        editAction,
        [](QDialog* dialog)
        {
            if ( QCheckBox* player = checkBoxStartingWith(
                    dialog, QStringLiteral("Player")) )
            {
                player->setChecked(false);
            }
            acceptDialog(dialog);
        });
    const QString editExpected = QStringLiteral(
        "symbols\nthing actor mask=0xfbff mask=0x80000000\nend\ncode\nend\n");
    const bool toolbarTargetsFirstMask = fixture.editor
        && fixture.editor->toPlainText() == editExpected
        && fixture.editor->document()->isUndoAvailable()
        && (fixture.editor->undo(), fixture.editor->toPlainText() == editSource)
        && (fixture.editor->redo(), fixture.editor->toPlainText() == editExpected);

    check(addObservation.actionFound && addObservation.dialogOpened
            && addObservation.interactionAccepted && addUndoRedo,
        "Insert > Symbol attribute > Add mask creates the default attribute in one undoable edit");
    check(editObservation.actionFound && editObservation.dialogOpened
            && editObservation.interactionAccepted && toolbarTargetsFirstMask,
        "Insert > Symbol attribute > Edit mask falls back to the first mask when the cursor is on the declaration name");
}

void testMaskInvalidTokenCancelAndAccept()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("mask_invalid_token_probe"));
    const QString source = QStringLiteral(
        "symbols\nthing actor mask=potato\nend\ncode\nend\n");
    setCogScriptEditorSource(fixture.editor, source);
    const int cancelRevision = fixture.editor->document()->revision();
    const MaskPopupObservation cancelObservation = invokeMaskEditor(
        fixture.editor, source, QStringLiteral("mask=potato"), cancelDialog);
    const bool cancelKeepsInvalidToken = fixture.editor
        && fixture.editor->toPlainText() == source
        && fixture.editor->document()->revision() == cancelRevision
        && !fixture.editor->document()->isModified();

    setCogScriptEditorSource(fixture.editor, source);
    const MaskPopupObservation acceptObservation = invokeMaskEditor(
        fixture.editor,
        source,
        QStringLiteral("mask=potato"),
        acceptDialog);
    const QString expected = QStringLiteral(
        "symbols\nthing actor mask=0x401\nend\ncode\nend\n");
    const bool acceptedCanonicalDefault = fixture.editor
        && fixture.editor->toPlainText() == expected
        && fixture.editor->document()->isUndoAvailable()
        && (fixture.editor->undo(), fixture.editor->toPlainText() == source)
        && (fixture.editor->redo(), fixture.editor->toPlainText() == expected);
    check(cancelObservation.dialogOpened && cancelKeepsInvalidToken,
        "Cancel preserves an invalid authored mask token exactly");
    check(acceptObservation.dialogOpened && acceptObservation.interactionAccepted
            && acceptedCanonicalDefault,
        "accepting the fallback checkbox state replaces an invalid mask token with the canonical default in one edit");
}

struct InlinePopupObservation
{
    bool buttonVisibleBeforeClick{};
    bool dialogOpened{};
    bool interactionAccepted{};
    bool interactionRejected{};
    QString observedDialogTitle{};
};

InlinePopupObservation invokeInlinePopupFromButton(
    QPlainTextEdit* editor,
    const QString& targetToken,
    const QString& dialogTitle,
    const std::function<void(QDialog*)>& interact)
{
    InlinePopupObservation observation{};
    if ( !editor )
    {
        return observation;
    }
    const QString source = editor->toPlainText();
    moveMouseToText(editor, source, targetToken, targetToken.size() - 1);
    QToolButton* const button = editor->findChild<QToolButton*>(
        QStringLiteral("cogInlineAssignmentPickerButton"));
    observation.buttonVisibleBeforeClick = button && button->isVisible();
    if ( !observation.buttonVisibleBeforeClick )
    {
        return observation;
    }

    ModalInvocationGuard invocation{};
    const std::function<void(QDialog*)> interactCopy = interact;
    startInvocationPoller(invocation.context(),
        [&observation, dialogTitle, interactCopy]()
        {
            auto* dialog = qobject_cast<QDialog*>(
                QApplication::activeModalWidget());
            observation.observedDialogTitle = dialog
                ? dialog->windowTitle() : QString{};
            observation.dialogOpened = dialog
                && observation.observedDialogTitle == dialogTitle;
            if ( !observation.dialogOpened )
            {
                return false;
            }
            const QPointer<QDialog> dialogGuard(dialog);
            interactCopy(dialog);
            observation.interactionAccepted = dialogGuard
                && dialogGuard->result() == QDialog::Accepted;
            observation.interactionRejected = dialogGuard
                && dialogGuard->result() == QDialog::Rejected;
            return true;
        });
    button->click();
    return observation;
}

struct EditorNavigationSnapshot
{
    int cursorAnchor{};
    int cursorPosition{};
    int horizontalScroll{};
    int verticalScroll{};
    int horizontalScrollMaximum{};
    int verticalScrollMaximum{};
    QString selectedText{};
    bool positiveScroll{};
};

void replaceEditorDuringPopup(
    QPlainTextEdit* editor,
    QDialog* dialog,
    const QString& replacement,
    EditorNavigationSnapshot* navigation,
    const bool accept)
{
    if ( !editor || !dialog || !navigation )
    {
        return;
    }
    editor->setPlainText(replacement);
    for ( int attempt = 0; attempt < 20; ++attempt )
    {
        QApplication::processEvents();
        if ( editor->horizontalScrollBar()->maximum() > 0
            && editor->verticalScrollBar()->maximum() > 0 )
        {
            break;
        }
    }
    const int selectionStart = replacement.indexOf(QStringLiteral("NAVIGATION_PROBE"));
    QTextCursor cursor(editor->document());
    cursor.setPosition(selectionStart);
    cursor.setPosition(
        selectionStart + QStringLiteral("NAVIGATION_PROBE").size(),
        QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
    editor->horizontalScrollBar()->setValue(
        editor->horizontalScrollBar()->maximum());
    editor->verticalScrollBar()->setValue(
        editor->verticalScrollBar()->maximum());
    const QTextCursor selected = editor->textCursor();
    *navigation = EditorNavigationSnapshot{
        selected.anchor(),
        selected.position(),
        editor->horizontalScrollBar()->value(),
        editor->verticalScrollBar()->value(),
        editor->horizontalScrollBar()->maximum(),
        editor->verticalScrollBar()->maximum(),
        selected.selectedText(),
        editor->horizontalScrollBar()->value() > 0
            && editor->verticalScrollBar()->value() > 0,
    };
    if ( accept )
    {
        acceptDialog(dialog);
    }
    else
    {
        cancelDialog(dialog);
    }
}

QString longNavigationReplacement()
{
    QString replacement = QStringLiteral("outside\n");
    for ( int line = 0; line < 80; ++line )
    {
        replacement += QString(240, QLatin1Char('x'));
        if ( line == 79 )
        {
            replacement += QStringLiteral("NAVIGATION_PROBE");
        }
        replacement += QLatin1Char('\n');
    }
    return replacement;
}

void testInlinePopupsKeepNavigationAfterSourceReplacement()
{
    const QString replacement = longNavigationReplacement();
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("stale_popup_navigation_probe"));
    if ( fixture.editor )
    {
        fixture.editor->setFixedHeight(360);
        QApplication::processEvents();
    }
    const QString flagsSource = QStringLiteral(
        "flags=0x00000003\nsymbols\nthing actor\nend\ncode\nend\n");
    const QString maskSource = QStringLiteral(
        "symbols\nthing actor mask=0xffff\nend\ncode\nend\n");
    struct StaleCaseResult
    {
        InlinePopupObservation popup{};
        EditorNavigationSnapshot navigation{};
        bool navigationPreserved{};
        int sourceRevision{};
        int finalRevision{};
        int finalHorizontalScroll{};
        int finalVerticalScroll{};
        bool fullCasePassed{};
    };
    const auto runCase = [&fixture, &replacement](
        const QString& source,
        const QString& token,
        const QString& title,
        const bool accept)
    {
        StaleCaseResult result{};
        setCogScriptEditorSource(fixture.editor, source);
        result.sourceRevision = fixture.editor
            ? fixture.editor->document()->revision() : -1;
        result.popup = invokeInlinePopupFromButton(
            fixture.editor,
            token,
            title,
            [&fixture, &replacement, &result, accept](QDialog* dialog)
            {
                replaceEditorDuringPopup(
                    fixture.editor, dialog, replacement,
                    &result.navigation, accept);
            });
        const QTextCursor cursor = fixture.editor
            ? fixture.editor->textCursor() : QTextCursor{};
        result.finalRevision = fixture.editor
            ? fixture.editor->document()->revision() : -1;
        result.finalHorizontalScroll = fixture.editor
            ? fixture.editor->horizontalScrollBar()->value() : -1;
        result.finalVerticalScroll = fixture.editor
            ? fixture.editor->verticalScrollBar()->value() : -1;
        result.navigationPreserved = fixture.editor
            && fixture.editor->toPlainText() == replacement
            && result.navigation.positiveScroll
            && result.navigation.selectedText
                == QStringLiteral("NAVIGATION_PROBE")
            && cursor.anchor() == result.navigation.cursorAnchor
            && cursor.position() == result.navigation.cursorPosition
            && cursor.selectedText() == result.navigation.selectedText
            && fixture.editor->horizontalScrollBar()->value()
                == result.navigation.horizontalScroll
            && fixture.editor->verticalScrollBar()->value()
                == result.navigation.verticalScroll;
        const bool expectedResult = accept
            ? result.popup.interactionAccepted
            : result.popup.interactionRejected;
        result.fullCasePassed = result.popup.buttonVisibleBeforeClick
            && result.popup.dialogOpened
            && expectedResult
            && result.navigationPreserved;
        if ( !result.fullCasePassed )
        {
            std::cerr
                << QStringLiteral(
                    "stale-popup diagnostic title=%1 observedTitle=%2 buttonVisible=%3 opened=%4 acceptRequested=%5 accepted=%6 rejected=%7 sourceRevision=%8 finalRevision=%9 positiveScroll=%10 capturedH=%11/%12 capturedV=%13/%14 finalH=%15 finalV=%16 selectedExpected=%17 selectedCaptured=%18 selectedFinal=%19 cursorCaptured=%20:%21 cursorFinal=%22:%23 textStillReplacement=%24 navigationPreserved=%25")
                    .arg(title)
                    .arg(result.popup.observedDialogTitle)
                    .arg(result.popup.buttonVisibleBeforeClick)
                    .arg(result.popup.dialogOpened)
                    .arg(accept)
                    .arg(result.popup.interactionAccepted)
                    .arg(result.popup.interactionRejected)
                    .arg(result.sourceRevision)
                    .arg(result.finalRevision)
                    .arg(result.navigation.positiveScroll)
                    .arg(result.navigation.horizontalScroll)
                    .arg(result.navigation.horizontalScrollMaximum)
                    .arg(result.navigation.verticalScroll)
                    .arg(result.navigation.verticalScrollMaximum)
                    .arg(result.finalHorizontalScroll)
                    .arg(result.finalVerticalScroll)
                    .arg(QStringLiteral("NAVIGATION_PROBE"))
                    .arg(result.navigation.selectedText)
                    .arg(cursor.selectedText())
                    .arg(result.navigation.cursorAnchor)
                    .arg(result.navigation.cursorPosition)
                    .arg(cursor.anchor())
                    .arg(cursor.position())
                    .arg(fixture.editor
                        && fixture.editor->toPlainText() == replacement)
                    .arg(result.navigationPreserved)
                    .toStdString()
                << '\n';
        }
        return result;
    };

    const StaleCaseResult flagsCancel = runCase(
        flagsSource,
        QStringLiteral("flags=0x00000003"),
        QStringLiteral("Edit COG Flags"),
        false);
    const StaleCaseResult flagsAccept = runCase(
        flagsSource,
        QStringLiteral("flags=0x00000003"),
        QStringLiteral("Edit COG Flags"),
        true);
    const StaleCaseResult maskAccept = runCase(
        maskSource,
        QStringLiteral("mask=0xffff"),
        QStringLiteral("Select mask"),
        true);
    const StaleCaseResult maskCancel = runCase(
        maskSource,
        QStringLiteral("mask=0xffff"),
        QStringLiteral("Select mask"),
        false);

    const bool allNavigationFixturesHavePositiveScroll =
        flagsCancel.navigation.positiveScroll
        && flagsAccept.navigation.positiveScroll
        && maskAccept.navigation.positiveScroll
        && maskCancel.navigation.positiveScroll
        && flagsCancel.navigation.selectedText
            == QStringLiteral("NAVIGATION_PROBE")
        && flagsAccept.navigation.selectedText
            == QStringLiteral("NAVIGATION_PROBE")
        && maskAccept.navigation.selectedText
            == QStringLiteral("NAVIGATION_PROBE")
        && maskCancel.navigation.selectedText
            == QStringLiteral("NAVIGATION_PROBE");
    check(allNavigationFixturesHavePositiveScroll,
        "stale COG popup fixtures establish a selected marker with positive horizontal and vertical scroll ranges");

    check(flagsCancel.popup.buttonVisibleBeforeClick
            && flagsCancel.popup.dialogOpened
            && flagsCancel.popup.interactionRejected
            && flagsCancel.navigationPreserved,
        "Canceling a stale flags popup preserves the replacement buffer selection and scroll position");
    check(flagsAccept.popup.buttonVisibleBeforeClick
            && flagsAccept.popup.dialogOpened
            && flagsAccept.popup.interactionAccepted
            && flagsAccept.navigationPreserved,
        "Accepting a stale flags popup preserves the replacement buffer selection and scroll position");
    check(maskAccept.popup.buttonVisibleBeforeClick
            && maskAccept.popup.dialogOpened
            && maskAccept.popup.interactionAccepted
            && maskAccept.navigationPreserved,
        "Accepting a stale mask popup preserves the replacement buffer selection and scroll position");
    check(maskCancel.popup.buttonVisibleBeforeClick
            && maskCancel.popup.dialogOpened
            && maskCancel.popup.interactionRejected
            && maskCancel.navigationPreserved,
        "Canceling a stale mask popup preserves the replacement buffer selection and scroll position");
}

void testMaskEllipsisEditsExactAttributeOnce()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("mask_ellipsis_probe"));
    const QString source = QStringLiteral(
        "symbols\n"
        "thing actor mask=0xffff mask=0x80000000\n"
        "end\n"
        "code\n"
        "end\n");
    setCogScriptEditorSource(fixture.editor, source);
    moveMouseToText(
        fixture.editor,
        source,
        QStringLiteral("mask=0x80000000"),
        QStringLiteral("mask=0x80000000").size() - 1);
    QToolButton* const button = fixture.editor
        ? fixture.editor->findChild<QToolButton*>(
            QStringLiteral("cogInlineAssignmentPickerButton"))
        : nullptr;
    const bool buttonVisibleBeforeClick = button && button->isVisible();
    bool dialogOpened = false;
    bool interactionAccepted = false;
    if ( buttonVisibleBeforeClick )
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* dialog = qobject_cast<QDialog*>(
                    QApplication::activeModalWidget());
                if ( !dialog )
                {
                    return false;
                }
                dialogOpened = dialog->windowTitle()
                    == QStringLiteral("Select mask");
                if ( QCheckBox* system = checkBoxStartingWith(
                        dialog, QStringLiteral("System")) )
                {
                    system->setChecked(true);
                }
                const QPointer<QDialog> dialogGuard(dialog);
                acceptDialog(dialog);
                interactionAccepted = dialogGuard
                    && dialogGuard->result() == QDialog::Accepted;
                return true;
            });
        button->click();
    }

    const QString expected = QStringLiteral(
        "symbols\n"
        "thing actor mask=0xffff mask=0x80000001\n"
        "end\n"
        "code\n"
        "end\n");
    const bool oneUndoRedo = fixture.editor
        && fixture.editor->toPlainText() == expected
        && fixture.editor->document()->isUndoAvailable()
        && (fixture.editor->undo(), fixture.editor->toPlainText() == source)
        && (fixture.editor->redo(), fixture.editor->toPlainText() == expected);
    check(buttonVisibleBeforeClick && dialogOpened && interactionAccepted,
        "hovering an exact COG mask value exposes a clickable inline ellipsis that opens the mask editor");
    check(oneUndoRedo,
        "the COG mask ellipsis edits only the hovered duplicate attribute in one undoable operation");
}

editor::DocumentState makeCogTemplatePickerState(
    const QString& currentValue,
    const QStringList& templateNames)
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.header.version = 1;
    for ( const QString& name : templateNames )
    {
        ThingTemplate templateValue{};
        templateValue.name = name.toStdString();
        state.level.templates.push_back(std::move(templateValue));
    }

    Cog cog{};
    cog.id = 1;
    cog.num = 0;
    cog.name = "template_picker_probe.cog";
    CogValue value{};
    value.name = "targetTemplate";
    value.type = ValueType::str;
    value.cogType = CogType::template_;
    value.stringValue = currentValue.toStdString();
    cog.values.push_back(std::move(value));
    state.level.cogs.push_back(std::move(cog));
    state.selection.cogs.insert(0);
    return state;
}

struct CogTemplateControls
{
    QLineEdit* edit{};
    QToolButton* picker{};
};

CogTemplateControls findCogTemplateControls(MainWindow& window)
{
    CogTemplateControls controls{};
    QTreeWidget* const properties = window.findChild<QTreeWidget*>(
        QStringLiteral("propertiesTree"));
    if ( !properties )
    {
        return controls;
    }

    for ( QTreeWidgetItemIterator item(properties); *item; ++item )
    {
        if ( (*item)->text(0).trimmed()
            != QStringLiteral("targetTemplate : template") )
        {
            continue;
        }

        QWidget* const row = properties->itemWidget(*item, 1);
        controls.edit = row ? row->findChild<QLineEdit*>() : nullptr;
        controls.picker = row ? row->findChild<QToolButton*>() : nullptr;
        return controls;
    }

    return controls;
}

bool resourcePickerContains(QDialog* dialog, const QString& resourceName)
{
    if ( !dialog )
    {
        return false;
    }

    for ( QListWidget* list : dialog->findChildren<QListWidget*>() )
    {
        for ( int row = 0; row < list->count(); ++row )
        {
            const QListWidgetItem* item = list->item(row);
            if ( item && item->text().compare(
                    resourceName, Qt::CaseInsensitive) == 0 )
            {
                return true;
            }
        }
    }

    return false;
}

bool templatePickerIsReady(QDialog* dialog, const QString& templateName)
{
    if ( !dialog
        || dialog->windowTitle() != QStringLiteral("Template Picker") )
    {
        return false;
    }

    if ( dialog->findChild<QLineEdit*>(
            QStringLiteral("resourcePickerFilter")) )
    {
        return resourcePickerContains(dialog, templateName);
    }

    return dialog->findChild<QComboBox*>() != nullptr;
}

bool selectAndAcceptResourcePicker(
    QDialog* dialog,
    const QString& resourceName)
{
    if ( !dialog )
    {
        return false;
    }

    bool selected = false;
    for ( QListWidget* list : dialog->findChildren<QListWidget*>() )
    {
        for ( int row = 0; row < list->count(); ++row )
        {
            QListWidgetItem* item = list->item(row);
            if ( item && item->text().compare(
                    resourceName, Qt::CaseInsensitive) == 0 )
            {
                list->setCurrentItem(item);
                selected = true;
                break;
            }
        }
        if ( selected )
        {
            break;
        }
    }

    QDialogButtonBox* const buttons = dialog->findChild<QDialogButtonBox*>();
    QAbstractButton* const accept = buttons
        ? buttons->button(QDialogButtonBox::Ok) : nullptr;
    if ( !selected || !accept || !accept->isEnabled() )
    {
        return false;
    }
    accept->click();
    return true;
}

bool selectAndAcceptTemplatePicker(
    QDialog* dialog,
    const QString& templateName)
{
    if ( !dialog )
    {
        return false;
    }

    if ( dialog->findChild<QLineEdit*>(
            QStringLiteral("resourcePickerFilter")) )
    {
        return selectAndAcceptResourcePicker(dialog, templateName);
    }

    QComboBox* const combo = dialog->findChild<QComboBox*>();
    const int candidate = combo
        ? combo->findText(templateName, Qt::MatchFixedString) : -1;
    QDialogButtonBox* const buttons = dialog->findChild<QDialogButtonBox*>();
    QAbstractButton* const accept = buttons
        ? buttons->button(QDialogButtonBox::Ok) : nullptr;
    if ( candidate < 0 || !accept )
    {
        return false;
    }

    combo->setCurrentIndex(candidate);
    accept->click();
    return true;
}

void testCogTemplatePickersUseSharedRichPicker()
{
    const QStringList templates{
        QStringLiteral("original_local_template"),
        QStringLiteral("candidate_local_template") };
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("template_picker_probe"));
    if ( fixture.window )
    {
        fixture.window->setDocumentStateForTest(makeCogTemplatePickerState(
            QStringLiteral("initial_template"), templates));
    }

    CogTemplateControls controls = fixture.window
        ? findCogTemplateControls(*fixture.window) : CogTemplateControls{};
    const std::size_t undoBeforePickers = fixture.window
        ? fixture.window->undoDepthForTest() : 0;
    QString propertyPickerTitle{};
    bool propertyPickerIsRich = false;
    bool propertyPickerContainsLocalTemplate = false;
    if ( controls.picker )
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* dialog = qobject_cast<QDialog*>(
                    QApplication::activeModalWidget());
                if ( !templatePickerIsReady(
                        dialog,
                        QStringLiteral("original_local_template")) )
                {
                    return false;
                }
                propertyPickerTitle = dialog->windowTitle();
                propertyPickerIsRich = dialog->findChild<QLineEdit*>(
                    QStringLiteral("resourcePickerFilter")) != nullptr;
                propertyPickerContainsLocalTemplate = resourcePickerContains(
                    dialog, QStringLiteral("original_local_template"));
                dialog->reject();
                return true;
            });
        controls.picker->click();
    }

    if ( fixture.window )
    {
        fixture.window->setDocumentStateForTest(makeCogTemplatePickerState(
            QStringLiteral("initial_template"), templates));
    }
    controls = fixture.window
        ? findCogTemplateControls(*fixture.window) : CogTemplateControls{};
    QString doubleClickPickerTitle{};
    bool doubleClickPickerIsRich = false;
    bool doubleClickPickerContainsLocalTemplate = false;
    bool doubleClickWasConsumed = false;
    bool doubleClickWasAccepted = false;
    if ( controls.edit && fixture.window )
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* dialog = qobject_cast<QDialog*>(
                    QApplication::activeModalWidget());
                if ( !templatePickerIsReady(
                        dialog,
                        QStringLiteral("original_local_template")) )
                {
                    return false;
                }
                doubleClickPickerTitle = dialog->windowTitle();
                doubleClickPickerIsRich = dialog->findChild<QLineEdit*>(
                    QStringLiteral("resourcePickerFilter")) != nullptr;
                doubleClickPickerContainsLocalTemplate =
                    resourcePickerContains(
                        dialog, QStringLiteral("original_local_template"));
                dialog->reject();
                return true;
            });
        const QPoint local = controls.edit->rect().center();
        const QPoint global = controls.edit->mapToGlobal(local);
        QMouseEvent doubleClick(
            QEvent::MouseButtonDblClick,
            QPointF(local),
            QPointF(global),
            QPointF(global),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        doubleClick.ignore();
        doubleClickWasConsumed = fixture.window->eventFilterForTest(
            controls.edit, &doubleClick);
        doubleClickWasAccepted = doubleClick.isAccepted();
    }

    check(propertyPickerIsRich
            && propertyPickerTitle == QStringLiteral("Template Picker")
            && propertyPickerContainsLocalTemplate,
        "COG template property button opens the shared rich Template Picker with local overrides");
    check(doubleClickPickerIsRich
            && doubleClickPickerTitle == QStringLiteral("Template Picker")
            && doubleClickPickerContainsLocalTemplate
            && doubleClickWasConsumed,
        "COG template double-click opens the same rich Template Picker with local overrides");
    const bool canceledWithoutMutation = fixture.window
        && fixture.window->documentStateForTest().level.cogs.size() == 1
        && fixture.window->documentStateForTest().level.cogs[0].values[0]
                .stringValue == "initial_template"
        && fixture.window->undoDepthForTest() == undoBeforePickers;
    if ( !(propertyPickerIsRich
            && propertyPickerTitle == QStringLiteral("Template Picker")
            && propertyPickerContainsLocalTemplate
            && doubleClickPickerIsRich
            && doubleClickPickerTitle == QStringLiteral("Template Picker")
            && doubleClickPickerContainsLocalTemplate
            && doubleClickWasConsumed
            && canceledWithoutMutation) )
    {
        std::cerr
            << QStringLiteral(
                "template-parity diagnostic edit=%1 picker=%2 propertyTitle=%3 propertyRich=%4 propertyLocal=%5 doubleTitle=%6 doubleRich=%7 doubleLocal=%8 consumed=%9 eventAccepted=%10 auxiliary=%11 finalValue=%12 undoBefore=%13 undoAfter=%14")
                .arg(controls.edit != nullptr)
                .arg(controls.picker != nullptr)
                .arg(propertyPickerTitle)
                .arg(propertyPickerIsRich)
                .arg(propertyPickerContainsLocalTemplate)
                .arg(doubleClickPickerTitle)
                .arg(doubleClickPickerIsRich)
                .arg(doubleClickPickerContainsLocalTemplate)
                .arg(doubleClickWasConsumed)
                .arg(doubleClickWasAccepted)
                .arg(controls.edit
                    && controls.edit->property(
                        "sed.auxiliaryButton").isValid())
                .arg(fixture.window
                    && !fixture.window->documentStateForTest().level.cogs.empty()
                    ? QString::fromStdString(
                        fixture.window->documentStateForTest().level.cogs[0]
                            .values[0].stringValue)
                    : QStringLiteral("<missing>"))
                .arg(undoBeforePickers)
                .arg(fixture.window
                    ? fixture.window->undoDepthForTest() : 0)
                .toStdString()
            << '\n';
    }
}

void testCogTemplatePropertyPickerUsesStableSnapshotAndRejectsStaleOwner()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("template_picker_stale_property"));
    const QStringList templates{
        QStringLiteral("original_local_template"),
        QStringLiteral("candidate_local_template") };
    if ( fixture.window )
    {
        fixture.window->setDocumentStateForTest(makeCogTemplatePickerState(
            QStringLiteral("initial_template"), templates));
    }

    CogTemplateControls controls = fixture.window
        ? findCogTemplateControls(*fixture.window) : CogTemplateControls{};
    bool snapshotStillListsOriginal = false;
    bool snapshotListsReplacement = false;
    QString snapshotPickerTitle{};
    if ( controls.picker && fixture.window )
    {
        bool liveOverridesChanged = false;
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* dialog = qobject_cast<QDialog*>(
                    QApplication::activeModalWidget());
                if ( !dialog )
                {
                    return false;
                }

                if ( !dialog->findChild<QLineEdit*>(
                        QStringLiteral("resourcePickerFilter")) )
                {
                    dialog->reject();
                    return true;
                }

                if ( !liveOverridesChanged )
                {
                    liveOverridesChanged = true;
                    editor::DocumentState& state =
                        fixture.window->mutableDocumentStateForTest();
                    state.level.templates.clear();
                    ThingTemplate replacementTemplate{};
                    replacementTemplate.name = "replacement_local_template";
                    state.level.templates.push_back(
                        std::move(replacementTemplate));
                    return false;
                }

                if ( !resourcePickerContains(
                        dialog, QStringLiteral("original_local_template"))
                    && !resourcePickerContains(
                        dialog, QStringLiteral("replacement_local_template")) )
                {
                    return false;
                }

                snapshotStillListsOriginal = resourcePickerContains(
                    dialog,
                    QStringLiteral("original_local_template"));
                snapshotListsReplacement = resourcePickerContains(
                    dialog,
                    QStringLiteral("replacement_local_template"));
                snapshotPickerTitle = dialog->windowTitle();
                dialog->reject();
                return true;
            });
        controls.picker->click();
    }

    if ( fixture.window )
    {
        fixture.window->setDocumentStateForTest(makeCogTemplatePickerState(
            QStringLiteral("initial_template"), templates));
    }
    controls = fixture.window
        ? findCogTemplateControls(*fixture.window) : CogTemplateControls{};
    const std::size_t undoBefore = fixture.window
        ? fixture.window->undoDepthForTest() : 0;
    bool selectedCandidate = false;
    bool acceptedCandidate = false;
    QString acceptedPickerTitle{};
    bool acceptedPickerIsRich = false;
    if ( controls.picker && fixture.window )
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* dialog = qobject_cast<QDialog*>(
                    QApplication::activeModalWidget());
                if ( !templatePickerIsReady(
                        dialog,
                        QStringLiteral("candidate_local_template")) )
                {
                    return false;
                }
                editor::DocumentState& state =
                    fixture.window->mutableDocumentStateForTest();
                state.level.cogs[0].values[0].stringValue =
                    "replacement_current_template";
                acceptedPickerTitle = dialog->windowTitle();
                acceptedPickerIsRich = dialog->findChild<QLineEdit*>(
                    QStringLiteral("resourcePickerFilter")) != nullptr;
                const QPointer<QDialog> dialogGuard(dialog);
                selectedCandidate = selectAndAcceptTemplatePicker(
                    dialog,
                    QStringLiteral("candidate_local_template"));
                acceptedCandidate = dialogGuard
                    && dialogGuard->result() == QDialog::Accepted;
                if ( !selectedCandidate && dialogGuard )
                {
                    dialogGuard->reject();
                }
                return true;
            });
        controls.picker->click();
    }

    const bool staleOwnerUnchanged = fixture.window
        && fixture.window->documentStateForTest().level.cogs.size() == 1
        && fixture.window->documentStateForTest().level.cogs[0].values[0]
                .stringValue == "replacement_current_template"
        && fixture.window->undoDepthForTest() == undoBefore;
    check(snapshotStillListsOriginal && !snapshotListsReplacement,
        "COG template picker keeps its original local override snapshot when templates change before deferred population");
    check(selectedCandidate && acceptedCandidate && staleOwnerUnchanged,
        "COG template property picker ignores an accepted result after its value owner changes during the modal session");
    if ( !(snapshotStillListsOriginal && !snapshotListsReplacement
            && selectedCandidate && acceptedCandidate
            && staleOwnerUnchanged) )
    {
        std::cerr
            << QStringLiteral(
                "template-property-stale diagnostic snapshotTitle=%1 snapshotOriginal=%2 snapshotReplacement=%3 acceptedTitle=%4 acceptedRich=%5 selected=%6 accepted=%7 finalValue=%8 undoBefore=%9 undoAfter=%10")
                .arg(snapshotPickerTitle)
                .arg(snapshotStillListsOriginal)
                .arg(snapshotListsReplacement)
                .arg(acceptedPickerTitle)
                .arg(acceptedPickerIsRich)
                .arg(selectedCandidate)
                .arg(acceptedCandidate)
                .arg(fixture.window
                    && !fixture.window->documentStateForTest().level.cogs.empty()
                    ? QString::fromStdString(
                        fixture.window->documentStateForTest().level.cogs[0]
                            .values[0].stringValue)
                    : QStringLiteral("<missing>"))
                .arg(undoBefore)
                .arg(fixture.window
                    ? fixture.window->undoDepthForTest() : 0)
                .toStdString()
            << '\n';
    }
}

void testCogTemplateDoubleClickRejectsStaleOwner()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("template_picker_stale_doubleclick"));
    const QStringList templates{
        QStringLiteral("original_local_template"),
        QStringLiteral("candidate_local_template") };
    if ( fixture.window )
    {
        fixture.window->setDocumentStateForTest(makeCogTemplatePickerState(
            QStringLiteral("initial_template"), templates));
    }
    const CogTemplateControls controls = fixture.window
        ? findCogTemplateControls(*fixture.window) : CogTemplateControls{};
    const std::size_t undoBefore = fixture.window
        ? fixture.window->undoDepthForTest() : 0;
    bool acceptedCandidate = false;
    bool acceptedResult = false;
    bool doubleClickWasConsumed = false;
    bool doubleClickWasAccepted = false;
    QString pickerTitle{};
    bool pickerIsRich = false;
    if ( controls.edit && fixture.window )
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* dialog = qobject_cast<QDialog*>(
                    QApplication::activeModalWidget());
                if ( !templatePickerIsReady(
                        dialog,
                        QStringLiteral("candidate_local_template")) )
                {
                    return false;
                }
                fixture.window->mutableDocumentStateForTest()
                    .level.cogs[0].values[0].stringValue =
                        "replacement_current_template";
                pickerTitle = dialog->windowTitle();
                pickerIsRich = dialog->findChild<QLineEdit*>(
                    QStringLiteral("resourcePickerFilter")) != nullptr;
                const QPointer<QDialog> dialogGuard(dialog);
                acceptedCandidate = selectAndAcceptTemplatePicker(
                    dialog,
                    QStringLiteral("candidate_local_template"));
                acceptedResult = dialogGuard
                    && dialogGuard->result() == QDialog::Accepted;
                if ( !acceptedCandidate && dialogGuard )
                {
                    dialogGuard->reject();
                }
                return true;
            });
        const QPoint local = controls.edit->rect().center();
        const QPoint global = controls.edit->mapToGlobal(local);
        QMouseEvent doubleClick(
            QEvent::MouseButtonDblClick,
            QPointF(local),
            QPointF(global),
            QPointF(global),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        doubleClick.ignore();
        doubleClickWasConsumed = fixture.window->eventFilterForTest(
            controls.edit, &doubleClick);
        doubleClickWasAccepted = doubleClick.isAccepted();
    }

    const bool staleOwnerUnchanged = fixture.window
        && fixture.window->documentStateForTest().level.cogs.size() == 1
        && fixture.window->documentStateForTest().level.cogs[0].values[0]
                .stringValue == "replacement_current_template"
        && fixture.window->undoDepthForTest() == undoBefore;
    // Returning true from QObject::eventFilter consumes the double-click;
    // QEvent::isAccepted() is not part of that filter contract.
    check(acceptedCandidate && acceptedResult && staleOwnerUnchanged
            && doubleClickWasConsumed,
        "COG template double-click ignores a selected resource after the value owner changes during the modal session");
    if ( !(acceptedCandidate && acceptedResult && staleOwnerUnchanged
            && doubleClickWasConsumed) )
    {
        std::cerr
            << QStringLiteral(
                "template-doubleclick-stale diagnostic title=%1 rich=%2 selected=%3 accepted=%4 consumed=%5 eventAccepted=%6 finalValue=%7 undoBefore=%8 undoAfter=%9")
                .arg(pickerTitle)
                .arg(pickerIsRich)
                .arg(acceptedCandidate)
                .arg(acceptedResult)
                .arg(doubleClickWasConsumed)
                .arg(doubleClickWasAccepted)
                .arg(fixture.window
                    && !fixture.window->documentStateForTest().level.cogs.empty()
                    ? QString::fromStdString(
                        fixture.window->documentStateForTest().level.cogs[0]
                            .values[0].stringValue)
                    : QStringLiteral("<missing>"))
                .arg(undoBefore)
                .arg(fixture.window
                    ? fixture.window->undoDepthForTest() : 0)
                .toStdString()
            << '\n';
    }
}

void testResourceEditorSaveShortcutsStayWithFocusedPane()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("resource_editor_shortcut_probe"));
    if ( fixture.window )
    {
        fixture.window->raise();
        fixture.window->activateWindow();
        QApplication::processEvents();
    }
    QAction* const newStringTable = fixture.window
        ? fixture.window->findChild<QAction*>(
            QStringLiteral("tools.resource_editors.string_table_uni.new"))
        : nullptr;
    QAction* const legacyStringTable = fixture.window
        ? fixture.window->findChild<QAction*>(
            QStringLiteral("edit.resources.string_table_uni"))
        : nullptr;
    QAction* const openStringTable = newStringTable
        ? newStringTable : legacyStringTable;
    if ( openStringTable )
    {
        openStringTable->trigger();
    }

    QDockWidget* const dock = fixture.window
        ? fixture.window->findChild<QDockWidget*>(
            QStringLiteral("uniResourceEditorsDock"))
        : nullptr;
    QTabWidget* const tabs = dock
        ? dock->findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"))
        : nullptr;
    QAbstractButton* const explicitNew = dock
        ? dock->findChild<QAbstractButton*>(QStringLiteral("resource_authoring.new")) : nullptr;
    if ( newStringTable && explicitNew )
    {
        explicitNew->click();
    }
    QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
    QTableWidget* const table = page
        ? page->findChild<QTableWidget*>(QStringLiteral("uni.entries")) : nullptr;
    if ( fixture.window && dock && tabs && page )
    {
        dock->show();
        dock->raise();
        tabs->setCurrentWidget(page);
        (void)settleTabLayout(tabs);
        fixture.window->raise();
        fixture.window->activateWindow();
    }

    QAction* const levelSaveAction = fixture.window
        ? fixture.window->findChild<QAction*>(QStringLiteral("file.save"))
        : nullptr;
    int levelSaveTriggerCount = 0;
    if ( levelSaveAction )
    {
        QObject::connect(levelSaveAction, &QAction::triggered,
            fixture.window.get(), [&levelSaveTriggerCount]()
            {
                ++levelSaveTriggerCount;
            });
    }

    struct ShortcutResult
    {
        bool overrideConsumed{};
        bool overrideAccepted{};
        bool pressConsumed{};
        bool pressAccepted{};
        bool resourceSaveDialogCanceled{};
        bool unexpectedModalCanceled{};
    };
    const auto sendSaveShortcut = [
        &fixture](QTableWidget* focusedTable,
                  const Qt::KeyboardModifiers modifiers,
                  const bool autoRepeat) -> ShortcutResult
    {
        ShortcutResult result{};
        if ( !fixture.window || !focusedTable )
        {
            return result;
        }

        QKeyEvent overrideEvent(
            QEvent::ShortcutOverride,
            Qt::Key_S,
            modifiers,
            QStringLiteral("s"),
            autoRepeat,
            1);
        overrideEvent.ignore();
        result.overrideConsumed = fixture.window->eventFilterForTest(
            focusedTable, &overrideEvent);
        result.overrideAccepted = overrideEvent.isAccepted();

        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&result]()
            {
                QWidget* const modal = QApplication::activeModalWidget();
                auto* const chooser = qobject_cast<QFileDialog*>(modal);
                if ( chooser
                    && chooser->objectName()
                        == QStringLiteral("resource_authoring.file_dialog") )
                {
                    result.resourceSaveDialogCanceled = true;
                    chooser->reject();
                }
                else if ( QDialog* const dialog = qobject_cast<QDialog*>(modal) )
                {
                    result.unexpectedModalCanceled = true;
                    dialog->reject();
                }
                return modal != nullptr;
            });

        QKeyEvent pressEvent(
            QEvent::KeyPress,
            Qt::Key_S,
            modifiers,
            QStringLiteral("s"),
            autoRepeat,
            1);
        pressEvent.ignore();
        result.pressConsumed = fixture.window->eventFilterForTest(
            focusedTable, &pressEvent);
        result.pressAccepted = pressEvent.isAccepted();
        QApplication::processEvents();
        return result;
    };

    const auto activateResourceTableFocus = [&]()
    {
        if ( !fixture.window || !dock || !tabs || !page || !table
            || !dock->isVisible() || !page->isVisible() )
        {
            return false;
        }
        fixture.window->raise();
        fixture.window->activateWindow();
        dock->raise();
        tabs->setCurrentWidget(page);
        table->setFocus(Qt::OtherFocusReason);
        for ( int attempt = 0; attempt < 20; ++attempt )
        {
            QApplication::processEvents();
            QWidget* const focused = QApplication::focusWidget();
            if ( QApplication::activeWindow() == fixture.window.get()
                && focused && tabs->isAncestorOf(focused) )
            {
                return true;
            }
        }
        return false;
    };

    const bool ownsResourceFocusBeforeSave = activateResourceTableFocus();
    const ShortcutResult saveResult = ownsResourceFocusBeforeSave
        ? sendSaveShortcut(table, Qt::ControlModifier, false)
        : ShortcutResult{};
    const bool ownsResourceFocusBeforeSaveAs = activateResourceTableFocus();
    const ShortcutResult saveAsResult = ownsResourceFocusBeforeSaveAs
        ? sendSaveShortcut(
            table, Qt::ControlModifier | Qt::ShiftModifier, false)
        : ShortcutResult{};
    const bool ownsResourceFocusBeforeRepeat = activateResourceTableFocus();
    const ShortcutResult repeatResult = ownsResourceFocusBeforeRepeat
        ? sendSaveShortcut(table, Qt::ControlModifier, true)
        : ShortcutResult{};
    const bool saveShortcutWasClaimed = saveResult.overrideConsumed
        && saveResult.overrideAccepted && saveResult.pressConsumed
        && saveResult.pressAccepted && saveResult.resourceSaveDialogCanceled
        && !saveResult.unexpectedModalCanceled;
    const bool saveAsShortcutWasClaimed = saveAsResult.overrideConsumed
        && saveAsResult.overrideAccepted && saveAsResult.pressConsumed
        && saveAsResult.pressAccepted
        && saveAsResult.resourceSaveDialogCanceled
        && !saveAsResult.unexpectedModalCanceled;
    const bool repeatedSaveWasConsumedWithoutSaving =
        repeatResult.overrideConsumed && repeatResult.overrideAccepted
        && repeatResult.pressConsumed && repeatResult.pressAccepted
        && !repeatResult.resourceSaveDialogCanceled
        && !repeatResult.unexpectedModalCanceled;
    check(openStringTable && openStringTable->isEnabled()
            && dock && tabs && page && table
            && dock->isVisible() && !dock->geometry().isEmpty()
            && !tabs->geometry().isEmpty() && !table->geometry().isEmpty()
            && ownsResourceFocusBeforeSave && ownsResourceFocusBeforeSaveAs,
        "resource editor shortcut fixture opens a UNI table and gives its table focus ownership");
    if ( !(ownsResourceFocusBeforeSave && ownsResourceFocusBeforeSaveAs) )
    {
        QWidget* const active = QApplication::activeWindow();
        QWidget* const focused = QApplication::focusWidget();
        QWidget* const modal = QApplication::activeModalWidget();
        std::cerr
            << QStringLiteral(
                "resource-shortcut-fixture diagnostic action=%1 actionEnabled=%2 active=%3/%4 focus=%5/%6 dockVisible=%7 dockEnabled=%8 dockGeometry=%9,%10,%11,%12 tabsVisible=%13 tabsGeometry=%14,%15,%16,%17 page=%18 tableVisible=%19 tableEnabled=%20 tableGeometry=%21,%22,%23,%24 focusIsDescendant=%25 modal=%26")
                .arg(openStringTable != nullptr)
                .arg(openStringTable && openStringTable->isEnabled())
                .arg(active ? active->objectName() : QStringLiteral("<null>"))
                .arg(active ? QString::fromLatin1(active->metaObject()->className()) : QStringLiteral("<null>"))
                .arg(focused ? focused->objectName() : QStringLiteral("<null>"))
                .arg(focused ? QString::fromLatin1(focused->metaObject()->className()) : QStringLiteral("<null>"))
                .arg(dock && dock->isVisible())
                .arg(dock && dock->isEnabled())
                .arg(dock ? dock->geometry().x() : -1)
                .arg(dock ? dock->geometry().y() : -1)
                .arg(dock ? dock->geometry().width() : -1)
                .arg(dock ? dock->geometry().height() : -1)
                .arg(tabs && tabs->isVisible())
                .arg(tabs ? tabs->geometry().x() : -1)
                .arg(tabs ? tabs->geometry().y() : -1)
                .arg(tabs ? tabs->geometry().width() : -1)
                .arg(tabs ? tabs->geometry().height() : -1)
                .arg(page ? page->objectName() : QStringLiteral("<null>"))
                .arg(table && table->isVisible())
                .arg(table && table->isEnabled())
                .arg(table ? table->geometry().x() : -1)
                .arg(table ? table->geometry().y() : -1)
                .arg(table ? table->geometry().width() : -1)
                .arg(table ? table->geometry().height() : -1)
                .arg(tabs && focused && tabs->isAncestorOf(focused))
                .arg(modal ? modal->objectName() : QStringLiteral("<null>"))
                .toStdString()
            << '\n';
    }
    check(ownsResourceFocusBeforeSave && ownsResourceFocusBeforeSaveAs
            && saveShortcutWasClaimed && saveAsShortcutWasClaimed
            && ownsResourceFocusBeforeRepeat
            && repeatedSaveWasConsumedWithoutSaving
            && levelSaveTriggerCount == 0,
        "resource editor focus consumes Ctrl+S and Ctrl+Shift+S through canceled saves, blocks repeated saves, and never triggers Level Save");
}

void testCogToolbarIconsKeepThemeSizeAndActions()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("toolbar_icon_probe.cog"));
    check(fixture.window && fixture.page && fixture.editor,
        "COG toolbar icon fixture opens an actual script editor");
    if ( !fixture.window || !fixture.page || !fixture.editor )
    {
        return;
    }
    const std::array<const char*, 4> names{
        "cogEditorSaveButton", "cogEditorWhitespaceButton", "cogEditorFormatButton",
        "cogEditorInsertButton" };
    std::array<QToolButton*, 4> buttons{};
    std::array<QImage, 4> glyphs{};
    QDockWidget* const editorDock = fixture.window->findChild<QDockWidget*>(QStringLiteral("cogScriptEditorDock"));
    bool iconContract = true;
    for ( std::size_t index = 0; index < names.size(); ++index )
    {
        buttons[index] = index == 0
            ? editorDock ? editorDock->findChild<QToolButton*>(QString::fromLatin1(names[index])) : nullptr
            : fixture.page->findChild<QToolButton*>(QString::fromLatin1(names[index]));
        QToolButton* const button = buttons[index];
        iconContract = iconContract && button && !button->icon().isNull()
            && button->iconSize() == QSize(20, 20) && button->height() == 30
            && button->autoRaise() && !button->toolTip().isEmpty();
        if ( !button )
        {
            continue;
        }
        glyphs[index] = button->icon().pixmap(QSize(32, 32), QIcon::Normal, QIcon::Off).toImage();
        bool hasThemeAccent = false;
        for ( int y = 0; y < glyphs[index].height(); ++y )
        {
            for ( int x = 0; x < glyphs[index].width(); ++x )
            {
                const QColor pixel = glyphs[index].pixelColor(x, y);
                hasThemeAccent = hasThemeAccent || (pixel.alpha() >= 160
                    && std::abs(pixel.red() - 115) <= 8
                    && std::abs(pixel.green() - 205) <= 8
                    && std::abs(pixel.blue() - 255) <= 8);
            }
        }
        check(hasThemeAccent,
            "COG toolbar glyphs share the resource toolbar's cyan theme accent");
    }
    for ( std::size_t first = 0; first < glyphs.size(); ++first )
    {
        for ( std::size_t second = first + 1; second < glyphs.size(); ++second )
        {
            iconContract = iconContract && !glyphs[first].isNull()
                && !glyphs[second].isNull() && glyphs[first] != glyphs[second];
        }
    }
    check(iconContract,
        "COG toolbar keeps distinct nonempty icons at 20 pixels and existing 30-pixel button heights");
    QToolButton* const whitespace = buttons[1];
    QToolButton* const format = buttons[2];
    QToolButton* const insert = buttons[3];
    check(whitespace && whitespace->isCheckable() && format && format->menu()
            && format->popupMode() == QToolButton::MenuButtonPopup
            && format->menu()->objectName() == QStringLiteral("cogEditorFormatMenu")
            && insert && insert->menu() && insert->popupMode() == QToolButton::InstantPopup
            && insert->menu()->objectName() == QStringLiteral("cogEditorInsertMenu")
            && !fixture.page->findChild<QToolButton*>(QStringLiteral("cogEditorReloadButton"))
            && fixture.window->findChild<QAction*>(QStringLiteral("cog_editor.reload"))
            && fixture.window->findChild<QAction*>(QStringLiteral("cog_editor.format")),
        "COG icon polish removes the Reload button while retaining the Refresh action, whitespace toggle, and Format and Insert menus");
    if ( whitespace )
    {
        const QString source = fixture.editor->toPlainText();
        const bool wasChecked = whitespace->isChecked();
        whitespace->click();
        check(whitespace->isChecked() != wasChecked
                && fixture.editor->document()->defaultTextOption().flags()
                    .testFlag(QTextOption::ShowTabsAndSpaces) == whitespace->isChecked()
                && fixture.editor->toPlainText() == source,
            "COG whitespace toolbar action still changes only whitespace presentation");
        whitespace->click();
    }
}

void testCogGroupFileActionsAndDocumentHistoryTools()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(QStringLiteral("document_tools_probe.cog"));
    QDockWidget* const dock = fixture.window
        ? fixture.window->findChild<QDockWidget*>(QStringLiteral("cogScriptEditorDock")) : nullptr;
    QWidget* const groupTools = dock && dock->widget()
        ? dock->widget()->findChild<QWidget*>(QStringLiteral("cogEditorGroupToolbar"), Qt::FindDirectChildrenOnly) : nullptr;
    QWidget* const documentTools = fixture.page
        ? fixture.page->findChild<QWidget*>(QStringLiteral("cogEditorToolbar")) : nullptr;
    QToolButton* const create = groupTools ? groupTools->findChild<QToolButton*>(QStringLiteral("cogEditorNewButton")) : nullptr;
    QToolButton* const open = groupTools ? groupTools->findChild<QToolButton*>(QStringLiteral("cogEditorOpenButton")) : nullptr;
    QToolButton* const save = groupTools ? groupTools->findChild<QToolButton*>(QStringLiteral("cogEditorSaveButton")) : nullptr;
    QToolButton* const undo = documentTools ? documentTools->findChild<QToolButton*>(QStringLiteral("cogEditorUndoButton")) : nullptr;
    QToolButton* const redo = documentTools ? documentTools->findChild<QToolButton*>(QStringLiteral("cogEditorRedoButton")) : nullptr;
    QAction* const saveAll = save && save->menu()
        ? save->menu()->findChild<QAction*>(QStringLiteral("cog_editor.save_all")) : nullptr;
    check(groupTools && groupTools->findChildren<QToolButton*>(QString{}, Qt::FindDirectChildrenOnly).size() == 3
            && create && open && save && saveAll && save->popupMode() == QToolButton::MenuButtonPopup
            && create->isEnabled() && open->isEnabled() && save->isEnabled() && saveAll->isEnabled()
            && fixture.page && !fixture.page->isAncestorOf(groupTools),
        "COG group tools: one New Open Save toolbar above file tabs offers Save All for modified scripts");
    check(documentTools && documentTools->findChildren<QToolButton*>().size() == 5
            && undo && redo && !undo->icon().isNull() && !redo->icon().isNull()
            && undo->iconSize() == QSize(20, 20) && redo->iconSize() == QSize(20, 20)
            && undo->focusPolicy() == Qt::NoFocus && redo->focusPolicy() == Qt::NoFocus
            && !undo->isEnabled() && !redo->isEnabled()
            && !documentTools->findChild<QToolButton*>(QStringLiteral("cogEditorSaveButton"))
            && !documentTools->findChild<QToolButton*>(QStringLiteral("cogEditorReloadButton"))
            && documentTools->findChild<QToolButton*>(QStringLiteral("cogEditorWhitespaceButton"))
            && documentTools->findChild<QToolButton*>(QStringLiteral("cogEditorFormatButton"))
            && documentTools->findChild<QToolButton*>(QStringLiteral("cogEditorInsertButton")),
        "COG document toolbar has exactly Undo Redo Whitespace Format and Insert without Reload or a duplicate group Save");
    if ( !fixture.window || !fixture.editor || !undo || !redo || !save || !saveAll )
    {
        return;
    }
    QAction* const configuredUndo = fixture.window->findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* const configuredRedo = fixture.window->findChild<QAction*>(QStringLiteral("edit.redo"));
    check(configuredUndo && configuredRedo, "COG document tools: configured history actions exist");
    if ( !configuredUndo || !configuredRedo )
    {
        return;
    }
    configuredUndo->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+U")));
    configuredRedo->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+R")));
    check(undo->toolTip().contains(QStringLiteral("Ctrl Alt U"))
            && redo->toolTip().contains(QStringLiteral("Ctrl Alt R")),
        "COG document tools: history tooltips track configured bindings without competing shortcuts");
    const QString before = fixture.editor->toPlainText();
    fixture.editor->setFocus(Qt::OtherFocusReason);
    QTextCursor cursor = fixture.editor->textCursor();
    cursor.movePosition(QTextCursor::End);
    fixture.editor->setTextCursor(cursor);
    fixture.editor->insertPlainText(QStringLiteral("\n# document history marker\n"));
    const QString after = fixture.editor->toPlainText();
    check(undo->isEnabled() && !redo->isEnabled(), "COG document tools: source edits enable only available history directions");
    undo->click();
    check(fixture.editor->toPlainText() == before && !undo->isEnabled() && redo->isEnabled(),
        "COG document tools: icon Undo restores exact source text through the current document stack");
    redo->click();
    check(fixture.editor->toPlainText() == after && undo->isEnabled() && !redo->isEnabled(),
        "COG document tools: icon Redo restores exact source text and updates truthful enablement");
    fixture.editor->document()->setModified(false);
    check(!save->isEnabled() && !saveAll->isEnabled(),
        "COG group tools: clean scripts disable Save and Save All without disabling document history");
}

void testCogRestoredDocumentHistoryButtonsMatchNativeAvailability()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::filesystem::path path = root / "restored_document_tools.cog";
    const QString initial = QStringLiteral("symbols\nend\ncode\nend\n");
    QFile sourceFile(QString::fromStdWString(path.wstring()));
    const bool sourceWritten = temporary.isValid() && sourceFile.open(QIODevice::WriteOnly)
        && sourceFile.write(initial.toUtf8()) == initial.toUtf8().size();
    sourceFile.close();
    check(sourceWritten, "COG restored document tools: independent clean loose source exists");
    if ( !sourceWritten )
    {
        return;
    }

    MainWindow source(nullptr, true);
    source.setProjectTypeForTest(ProjectType::JKDF2);
    source.setCurrentPathForTitleForTest(root / "restored_document_tools.sed");
    source.setResourcePathsForTest({ root }, {}, root / "restored_document_tools.sed");
    source.openCogScriptEditorForTest("restored_document_tools.cog");
    QTabWidget* const sourceTabs = source.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QPlainTextEdit* const editor = sourceTabs && sourceTabs->currentWidget()
        ? sourceTabs->currentWidget()->findChild<QPlainTextEdit*>() : nullptr;
    check(editor && editor->toPlainText() == initial && !editor->document()->isModified()
            && !editor->document()->isUndoAvailable() && !editor->document()->isRedoAvailable(),
        "COG restored document tools: source begins at a clean empty native timeline");
    if ( !editor )
    {
        return;
    }
    const auto appendCommand = [editor](const QString& text)
    {
        QTextCursor cursor(editor->document());
        cursor.movePosition(QTextCursor::End);
        cursor.beginEditBlock();
        cursor.insertText(text);
        cursor.endEditBlock();
        editor->setTextCursor(cursor);
    };
    appendCommand(QStringLiteral("# first restored command\n"));
    const QString first = editor->toPlainText();
    appendCommand(QStringLiteral("# second restored command\n"));
    const QString second = editor->toPlainText();
    editor->undo();
    const QByteArray middlePayload = source.captureCogEditorRecoveryStateForTest();
    const QCborArray middleTabs = QCborValue::fromCbor(middlePayload).toMap()
        .value(QStringLiteral("tabs")).toArray();
    const QCborMap middleTab = middleTabs.size() == 1 ? middleTabs.at(0).toMap() : QCborMap{};
    const QCborMap middleHistory = middleTab.value(QStringLiteral("history")).toMap();
    check(editor->toPlainText() == first && editor->document()->isUndoAvailable()
            && editor->document()->isRedoAvailable() && editor->document()->isModified()
            && middleTabs.size() == 1
            && middleHistory.value(QStringLiteral("cursor")).toInteger(-1) == 1
            && middleHistory.value(QStringLiteral("clean")).toInteger(-1) == 0
            && middleHistory.value(QStringLiteral("commands")).toArray().size() == 2,
        "COG restored document tools: captured midcursor retains both real command directions");
    editor->undo();
    const QByteArray startPayload = source.captureCogEditorRecoveryStateForTest();
    check(editor->toPlainText() == initial && !editor->document()->isModified()
            && !editor->document()->isUndoAvailable() && editor->document()->isRedoAvailable(),
        "COG restored document tools: clean start retains only the real redo branch");

    for ( const bool atStart : { false, true } )
    {
        MainWindow restored(nullptr, true);
        restored.setProjectTypeForTest(ProjectType::JKDF2);
        restored.setCurrentPathForTitleForTest(root / "restored_document_tools.sed");
        restored.setResourcePathsForTest({ root }, {}, root / "restored_document_tools.sed");
        QTabWidget* const tabs = restored.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
        check(tabs && tabs->count() == 0,
            "COG restored document tools: restore targets a fresh group with no existing page controls");
        const bool restoredOk = restored.restoreCogEditorRecoveryStateForTest(atStart ? startPayload : middlePayload);
        QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
        QPlainTextEdit* const restoredEditor = page ? page->findChild<QPlainTextEdit*>() : nullptr;
        QToolButton* const undo = page ? page->findChild<QToolButton*>(QStringLiteral("cogEditorUndoButton")) : nullptr;
        QToolButton* const redo = page ? page->findChild<QToolButton*>(QStringLiteral("cogEditorRedoButton")) : nullptr;
        check(restoredOk && restoredEditor && undo && redo
                && restoredEditor->toPlainText() == (atStart ? initial : first)
                && restoredEditor->document()->isUndoAvailable() == !atStart
                && restoredEditor->document()->isRedoAvailable()
                && restoredEditor->document()->isModified() == !atStart
                && undo->isEnabled() == !atStart && redo->isEnabled(),
            atStart ? "COG restored document tools: fresh clean-start page disables Undo and enables Redo from native availability"
                : "COG restored document tools: fresh midcursor page enables both buttons from native availability");
        if ( !restoredEditor || !undo || !redo )
        {
            continue;
        }
        if ( !atStart )
        {
            undo->click();
            check(restoredEditor->toPlainText() == initial && !restoredEditor->document()->isModified()
                    && !undo->isEnabled() && redo->isEnabled(),
                "COG restored document tools: icon Undo reaches the exact clean source and updates both directions");
        }
        redo->click();
        check(restoredEditor->toPlainText() == first && undo->isEnabled() && redo->isEnabled(),
            "COG restored document tools: first icon Redo restores the exact midcursor and both available directions");
        redo->click();
        check(restoredEditor->toPlainText() == second && undo->isEnabled() && !redo->isEnabled(),
            "COG restored document tools: final icon Redo restores exact source and disables the exhausted direction");
    }
    check(sourceFile.open(QIODevice::ReadOnly) && sourceFile.readAll() == initial.toUtf8(),
        "COG restored document tools: reconstruction and icon replay leave the independent loose source bytes untouched");
}

void testCogRemappedDocumentHistoryKeepsLevelHistory()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(QStringLiteral("remapped_history_probe.cog"));
    QAction* const undo = fixture.window ? fixture.window->findChild<QAction*>(QStringLiteral("edit.undo")) : nullptr;
    QAction* const redo = fixture.window ? fixture.window->findChild<QAction*>(QStringLiteral("edit.redo")) : nullptr;
    QDockWidget* const dock = fixture.window ? fixture.window->findChild<QDockWidget*>(QStringLiteral("cogScriptEditorDock")) : nullptr;
    check(fixture.window && fixture.page && fixture.editor && undo && redo && dock,
        "COG remapped history: real editor, dock and configured actions exist");
    if ( !fixture.window || !fixture.page || !fixture.editor || !undo || !redo || !dock )
    {
        return;
    }
    undo->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+U")));
    redo->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+R")));
    check(fixture.window->applyStructuralHistoryEditForTest([](editor::DocumentState& state)
        {
            state.level.header.gravity += 3.0;
            return true;
        }), "COG remapped history: independent Level command exists");
    const double gravity = fixture.window->documentStateForTest().level.header.gravity;
    const std::size_t worldUndo = fixture.window->undoDepthForTest();
    const std::size_t worldRedo = fixture.window->redoDepthForTest();
    const QString before = fixture.editor->toPlainText();
    const auto dispatch = [&fixture](QWidget* receiver, const int key)
    {
        const Qt::KeyboardModifiers modifiers = Qt::ControlModifier | Qt::AltModifier;
        QKeyEvent overrideEvent(QEvent::ShortcutOverride, key, modifiers);
        overrideEvent.ignore();
        const bool overrideConsumed = fixture.window->eventFilterForTest(receiver, &overrideEvent);
        QKeyEvent pressEvent(QEvent::KeyPress, key, modifiers);
        pressEvent.ignore();
        const bool pressConsumed = fixture.window->eventFilterForTest(receiver, &pressEvent);
        return overrideConsumed && overrideEvent.isAccepted() && pressConsumed && pressEvent.isAccepted();
    };
    for ( const bool floating : { false, true } )
    {
        dock->setFloating(floating);
        dock->show();
        dock->raise();
        if ( floating )
        {
            dock->activateWindow();
        }
        else
        {
            fixture.window->activateWindow();
        }
        QTextCursor cursor = fixture.editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        fixture.editor->setTextCursor(cursor);
        fixture.editor->insertPlainText(QStringLiteral("\n# remapped document command\n"));
        const QString after = fixture.editor->toPlainText();
        fixture.page->setFocusPolicy(Qt::StrongFocus);
        native_caption_test::ScopedTestFocusRestore restoreFocus{};
        const bool actualBodyFocus = native_caption_test::focusTestWidget(
            fixture.page, fixture.page, 2000);
        check(actualBodyFocus,
            "COG remapped history: actual docked/floating document body owns focus");
        const bool undoConsumed = dispatch(fixture.page, Qt::Key_U);
        check(undoConsumed && fixture.editor->toPlainText() == before && fixture.editor->document()->isRedoAvailable(),
            "COG remapped history: configured body Undo restores exact source in docked and floating editors");
        const bool redoConsumed = dispatch(fixture.page, Qt::Key_R);
        check(redoConsumed && fixture.editor->toPlainText() == after && !fixture.editor->document()->isRedoAvailable(),
            "COG remapped history: configured body Redo restores exact source in docked and floating editors");
        check(fixture.window->documentStateForTest().level.header.gravity == gravity
                && fixture.window->undoDepthForTest() == worldUndo && fixture.window->redoDepthForTest() == worldRedo,
            "COG remapped history: configured document keys preserve the independent Level state and both history depths");
        fixture.editor->undo();
    }
    fixture.editor->setFocus(Qt::OtherFocusReason);
    QTextCursor cursor = fixture.editor->textCursor();
    cursor.movePosition(QTextCursor::End);
    fixture.editor->setTextCursor(cursor);
    fixture.editor->insertPlainText(QStringLiteral("\n# native field command\n"));
    native_caption_test::ScopedTestFocusRestore editorFocusRestore{};
    const bool actualEditorFocus = native_caption_test::focusTestWidget(
        fixture.editor, fixture.editor, 2000);
    check(actualEditorFocus,
        "COG remapped history: actual source editor owns focus for native Ctrl+Z priority");
    QKeyEvent nativeUndo(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
    const bool nativeUndoDispatched = actualEditorFocus
        && QApplication::sendEvent(fixture.editor, &nativeUndo);
    check(nativeUndoDispatched && fixture.editor->toPlainText() == before
            && fixture.window->undoDepthForTest() == worldUndo,
        "COG remapped history: normal native source Ctrl Z keeps text history priority");
}

void testCogGroupSaveAllStopsCanceledFileWithoutLosingDrafts()
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "COG Save All: isolated project directory exists");
    if ( !temporary.isValid() )
    {
        return;
    }
    MainWindow window{};
    const std::filesystem::path directory(temporary.path().toStdWString());
    window.setCurrentPathForTitleForTest(directory / "project.sed");
    window.createNewCogScriptForTest(QStringLiteral("save_all_first.cog"));
    window.createNewCogScriptForTest(QStringLiteral("save_all_second.cog"));
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QDockWidget* const dock = window.findChild<QDockWidget*>(QStringLiteral("cogScriptEditorDock"));
    QAction* const saveAll = dock ? dock->findChild<QAction*>(QStringLiteral("cog_editor.save_all")) : nullptr;
    QToolButton* const save = dock ? dock->findChild<QToolButton*>(QStringLiteral("cogEditorSaveButton")) : nullptr;
    QWidget* const firstPage = tabs && tabs->count() == 2 ? tabs->widget(0) : nullptr;
    QWidget* const secondPage = tabs && tabs->count() == 2 ? tabs->widget(1) : nullptr;
    QPlainTextEdit* const first = firstPage ? firstPage->findChild<QPlainTextEdit*>() : nullptr;
    QPlainTextEdit* const second = secondPage ? secondPage->findChild<QPlainTextEdit*>() : nullptr;
    check(saveAll && save && first && second, "COG Save All: real group action and both source drafts exist");
    if ( !saveAll || !save || !first || !second )
    {
        return;
    }
    const QByteArray firstText = first->toPlainText().toUtf8();
    const QByteArray secondText = second->toPlainText().toUtf8();
    const std::filesystem::path firstPath = directory / "cog" / "save_all_first.cog";
    const std::filesystem::path secondPath = directory / "cog" / "save_all_second.cog";
    std::error_code directoryError{};
    std::filesystem::create_directories(secondPath.parent_path(), directoryError);
    QFile sentinel(QString::fromStdWString(secondPath.wstring()));
    const QByteArray sentinelBytes("# untouched existing file\n");
    const bool sentinelWritten = !directoryError && sentinel.open(QIODevice::WriteOnly)
        && sentinel.write(sentinelBytes) == sentinelBytes.size();
    sentinel.close();
    check(sentinelWritten, "COG Save All: second target has exact pre-existing bytes for cancellation");
    if ( !sentinelWritten )
    {
        return;
    }
    window.resize(1000, 750);
    window.show();
    (void)settleTabLayout(tabs);
    dock->setFloating(true);
    dock->show();
    (void)settleTabLayout(tabs);
    check(dock->isFloating() && tabs->count() == 2,
        "COG Save All: real floating group retains both original source pages");
    tabs->setCurrentWidget(secondPage);
    const std::size_t worldUndo = window.undoDepthForTest();
    bool overwriteSeen = false;
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if ( !message )
                {
                    return false;
                }
                overwriteSeen = message->text().contains(QString::fromStdWString(secondPath.wstring()));
                if ( QAbstractButton* const cancel = message->button(QMessageBox::No) )
                {
                    cancel->click();
                }
                else
                {
                    message->reject();
                }
                return true;
            });
        saveAll->trigger();
    }
    const auto readBytes = [](const std::filesystem::path& path)
    {
        QFile file(QString::fromStdWString(path.wstring()));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    };
    check(overwriteSeen && readBytes(firstPath) == firstText && !first->document()->isModified()
            && readBytes(secondPath) == sentinelBytes && second->toPlainText().toUtf8() == secondText
            && second->document()->isModified() && tabs->currentWidget() == secondPage
            && tabs->count() == 2 && window.undoDepthForTest() == worldUndo,
        "COG Save All: a canceled later file preserves its exact bytes and draft while an earlier atomic save remains clean and selection stays exact");
    check(save->isEnabled() && saveAll->isEnabled(), "COG Save All: cancellation keeps the remaining modified group available");
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if ( !message )
                {
                    return false;
                }
                if ( QAbstractButton* const accept = message->button(QMessageBox::Yes) )
                {
                    accept->click();
                }
                else
                {
                    message->reject();
                }
                return true;
            });
        tabs->setCurrentWidget(firstPage);
        save->click();
        check(readBytes(secondPath) == sentinelBytes && second->document()->isModified(),
            "COG Save All: plain Save on a clean current file leaves the dirty neighbor and its target untouched");
        saveAll->trigger();
    }
    check(readBytes(firstPath) == firstText && readBytes(secondPath) == secondText
            && !first->document()->isModified() && !second->document()->isModified()
            && !save->isEnabled() && !saveAll->isEnabled() && tabs->currentWidget() == firstPage
            && window.undoDepthForTest() == worldUndo,
        "COG Save All: success saves only remaining modifications and disables the clean group without changing selection or Level history");
}

void testCogSuccessfulSaveNoticeAndCanceledSaveStatus()
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "COG save notice: isolated project directory exists");
    if ( !temporary.isValid() )
    {
        return;
    }
    MainWindow window{};
    const std::filesystem::path directory(temporary.path().toStdWString());
    window.setCurrentPathForTitleForTest(directory / "project.sed");
    window.createNewCogScriptForTest(QStringLiteral("saved_notice.cog"));
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QDockWidget* const dock = window.findChild<QDockWidget*>(QStringLiteral("cogScriptEditorDock"));
    QToolButton* const save = dock ? dock->findChild<QToolButton*>(QStringLiteral("cogEditorSaveButton")) : nullptr;
    QWidget* const firstPage = tabs ? tabs->currentWidget() : nullptr;
    QPlainTextEdit* const first = firstPage ? firstPage->findChild<QPlainTextEdit*>() : nullptr;
    check(tabs && save && first && first->document()->isModified(),
        "COG save notice: real New draft and enabled group Save exist");
    if ( !tabs || !save || !first )
    {
        return;
    }
    const std::filesystem::path firstPath = directory / "cog" / "saved_notice.cog";
    const QByteArray firstBytes = first->toPlainText().toUtf8();
    std::error_code canonicalError{};
    const std::filesystem::path canonicalFirstPath = std::filesystem::weakly_canonical(firstPath, canonicalError);
    check(!canonicalError && !canonicalFirstPath.empty(),
        "COG save notice: expected saved path is independently canonicalized");
    if ( canonicalError || canonicalFirstPath.empty() )
    {
        return;
    }
    const QString expectedNotice = QStringLiteral("Saved %1").arg(
        QDir::toNativeSeparators(QString::fromStdWString(canonicalFirstPath.wstring())));
    const auto readBytes = [](const std::filesystem::path& path)
    {
        QFile file(QString::fromStdWString(path.wstring()));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    };
    window.resize(1000, 750);
    window.show();
    (void)settleTabLayout(tabs);
    window.statusBar()->showMessage(QStringLiteral("Prior operation"));
    bool unexpectedModal = false;
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* const modal = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if ( !modal )
                {
                    return false;
                }
                unexpectedModal = true;
                modal->reject();
                return true;
            });
        save->click();
    }
    check(!unexpectedModal && readBytes(firstPath) == firstBytes
            && !first->document()->isModified() && !save->isEnabled()
            && QDir::toNativeSeparators(firstPage->property("cogFullPath").toString())
                == QDir::toNativeSeparators(QString::fromStdWString(canonicalFirstPath.wstring())),
        "COG save notice: successful group Save writes exact source and commits its path without a modal");
    check(window.statusBar()->currentMessage() == expectedNotice,
        "COG save notice: successful Save briefly reports the exact native saved path");

    window.createNewCogScriptForTest(QStringLiteral("canceled_notice.cog"));
    QWidget* const canceledPage = tabs->currentWidget();
    QPlainTextEdit* const canceled = canceledPage ? canceledPage->findChild<QPlainTextEdit*>() : nullptr;
    check(canceled && canceled->document()->isModified(),
        "COG save notice: canceled overwrite targets a second real modified draft");
    if ( !canceled )
    {
        return;
    }
    const std::filesystem::path canceledPath = directory / "cog" / "canceled_notice.cog";
    const QByteArray sentinelBytes("# existing source must remain unchanged\n");
    QFile sentinel(QString::fromStdWString(canceledPath.wstring()));
    const bool sentinelWritten = sentinel.open(QIODevice::WriteOnly)
        && sentinel.write(sentinelBytes) == sentinelBytes.size();
    sentinel.close();
    check(sentinelWritten, "COG save notice: overwrite target has exact independent bytes");
    if ( !sentinelWritten )
    {
        return;
    }
    const QString draftText = canceled->toPlainText();
    const QString draftPath = canceledPage->property("cogFullPath").toString();
    const QString priorStatus = QStringLiteral("Keep this prior status after cancel");
    window.statusBar()->showMessage(priorStatus);
    bool overwriteSeen = false;
    bool cancelClicked = false;
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if ( !message )
                {
                    return false;
                }
                overwriteSeen = message->text().contains(QString::fromStdWString(canceledPath.wstring()));
                if ( QAbstractButton* const cancel = message->button(QMessageBox::No) )
                {
                    cancelClicked = true;
                    cancel->click();
                }
                else
                {
                    message->reject();
                }
                return true;
            });
        save->click();
    }
    check(overwriteSeen && cancelClicked && readBytes(canceledPath) == sentinelBytes
            && canceled->toPlainText() == draftText && canceled->document()->isModified()
            && canceledPage->property("cogFullPath").toString() == draftPath
            && tabs->currentWidget() == canceledPage && save->isEnabled()
            && window.statusBar()->currentMessage() == priorStatus,
        "COG save notice: canceled overwrite keeps status, exact bytes, draft identity, dirty marker and selection");
}

void testCogSaveNoticeCanDeleteItsEditor()
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "COG save notice lifetime: isolated project directory exists");
    if ( !temporary.isValid() )
    {
        return;
    }
    QPointer<MainWindow> window(new MainWindow);
    const std::filesystem::path directory(temporary.path().toStdWString());
    window->setCurrentPathForTitleForTest(directory / "project.sed");
    window->createNewCogScriptForTest(QStringLiteral("notice_owner.cog"));
    QTabWidget* const tabs = window->findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QPointer<QWidget> page(tabs ? tabs->currentWidget() : nullptr);
    QPlainTextEdit* const editor = page ? page->findChild<QPlainTextEdit*>() : nullptr;
    QDockWidget* const dock = window->findChild<QDockWidget*>(QStringLiteral("cogScriptEditorDock"));
    QPointer<QToolButton> save(dock ? dock->findChild<QToolButton*>(QStringLiteral("cogEditorSaveButton")) : nullptr);
    check(page && editor && save, "COG save notice lifetime: real modified source and group Save exist");
    if ( !page || !editor || !save )
    {
        delete window.data();
        return;
    }
    const QByteArray expectedBytes = editor->toPlainText().toUtf8();
    const std::filesystem::path path = directory / "cog" / "notice_owner.cog";
    std::error_code canonicalError{};
    const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, canonicalError);
    check(!canonicalError && !canonicalPath.empty(),
        "COG save notice lifetime: expected destination is independently canonicalized");
    if ( canonicalError || canonicalPath.empty() )
    {
        delete window.data();
        return;
    }
    const QString expectedNotice = QStringLiteral("Saved %1").arg(
        QDir::toNativeSeparators(QString::fromStdWString(canonicalPath.wstring())));
    QObject observer{};
    QString actualNotice{};
    QObject::connect(window->statusBar(), &QStatusBar::messageChanged, &observer,
        [&page, &actualNotice, expectedNotice](const QString& message)
        {
            if ( message == expectedNotice )
            {
                actualNotice = message;
                delete page.data();
            }
        });
    save->click();
    QFile saved(QString::fromStdWString(path.wstring()));
    check(actualNotice == expectedNotice && window && !page && save && tabs->count() == 0
            && saved.open(QIODevice::ReadOnly) && saved.readAll() == expectedBytes,
        "COG save notice lifetime: synchronous status observer may delete the editor after exact atomic Save without retaining a callback or path member");
    if ( window )
    {
        delete window.data();
    }
}

void testCogOverwriteRejectsDestroyedOwner()
{
    for ( const bool destroyWindow : { false, true } )
    {
        QTemporaryDir temporary{};
        check(temporary.isValid(),
            "COG overwrite lifetime: isolated project directory exists");
        if ( !temporary.isValid() )
        {
            continue;
        }
        const std::filesystem::path root(temporary.path().toStdWString());
        const std::filesystem::path projectPath = root / "overwrite_lifetime.sed";
        QPointer<MainWindow> window(new MainWindow());
        window->setAttribute(Qt::WA_DontShowOnScreen);
        window->setAnimated(false);
        window->setCurrentPathForTitleForTest(projectPath);
        window->setResourcePathsForTest({ root }, {}, projectPath);
        window->createNewCogScriptForTest(QStringLiteral("unrelated_neighbor.cog"));
        QPointer<QTabWidget> tabs(window->findChild<QTabWidget*>(
            QStringLiteral("cogScriptEditorTabs")));
        QPointer<QWidget> neighbor(tabs ? tabs->currentWidget() : nullptr);
        QPointer<QPlainTextEdit> neighborEditor(neighbor
            ? neighbor->findChild<QPlainTextEdit*>() : nullptr);
        check(neighbor && neighborEditor,
            "COG overwrite lifetime: an independent unsaved neighbor exists");
        if ( !neighbor || !neighborEditor )
        {
            delete window.data();
            continue;
        }
        neighborEditor->insertPlainText(QStringLiteral("# retain the neighbor's native Undo branch\n"));
        const QString neighborText = neighborEditor->toPlainText();
        const int neighborRevision = neighborEditor->document()->revision();
        const int neighborUndoSteps = neighborEditor->document()->availableUndoSteps();
        const bool neighborDirty = neighborEditor->document()->isModified();
        window->createNewCogScriptForTest(QStringLiteral("overwrite_destroyed.cog"));
        QPointer<QWidget> page(tabs ? tabs->currentWidget() : nullptr);
        QPointer<QPlainTextEdit> editor(page
            ? page->findChild<QPlainTextEdit*>() : nullptr);
        QDockWidget* const dock = window->findChild<QDockWidget*>(
            QStringLiteral("cogScriptEditorDock"));
        QPointer<QToolButton> save(dock
            ? dock->findChild<QToolButton*>(QStringLiteral("cogEditorSaveButton")) : nullptr);
        check(page && editor && save && save->isEnabled()
                && page != neighbor && editor->document()->isModified(),
            "COG overwrite lifetime: the distinct dirty draft owns the enabled real group Save");
        if ( !page || !editor || !save || !save->isEnabled() || page == neighbor )
        {
            delete window.data();
            continue;
        }
        const std::filesystem::path target = root / "cog" / "overwrite_destroyed.cog";
        const QByteArray sentinel("# preserve this existing source exactly\nsymbols\nint sentinel\nend\ncode\nend\n");
        QDir().mkpath(QString::fromStdWString(target.parent_path().wstring()));
        QFile file(QString::fromStdWString(target.wstring()));
        const bool written = file.open(QIODevice::WriteOnly)
            && file.write(sentinel) == sentinel.size();
        file.close();
        check(written, "COG overwrite lifetime: existing target has exact sentinel bytes");
        if ( !written )
        {
            delete window.data();
            continue;
        }
        bool overwriteSeen = false;
        bool ownerDestroyed = false;
        const auto levelUndoDepth = window->undoDepthForTest();
        {
            ModalInvocationGuard invocation{};
            startInvocationPoller(invocation.context(), [&]()
                {
                    QPointer<QMessageBox> message(qobject_cast<QMessageBox*>(
                        QApplication::activeModalWidget()));
                    if ( !message || message->windowTitle() != QStringLiteral("Save COG script")
                        || !message->text().contains(QStringLiteral("overwrite_destroyed.cog"))
                        || !message->button(QMessageBox::Yes) || !message->button(QMessageBox::No) )
                    {
                        return false;
                    }
                    overwriteSeen = true;
                    std::cerr << "COG overwrite lifetime: deleting "
                        << (destroyWindow ? "MainWindow" : "draft page")
                        << " during actual overwrite confirmation" << std::endl;
                    if ( destroyWindow )
                    {
                        delete window.data();
                        ownerDestroyed = window.isNull();
                    }
                    else
                    {
                        delete page.data();
                        ownerDestroyed = page.isNull();
                    }
                    if ( message && message->isVisible() )
                    {
                        message->button(QMessageBox::Yes)->click();
                    }
                    return true;
                });
            save->click();
        }
        const bool read = file.open(QIODevice::ReadOnly);
        const QByteArray after = read ? file.readAll() : QByteArray{};
        check(overwriteSeen && ownerDestroyed && !page && !editor
                && read && after == sentinel,
            "COG overwrite confirmation safely rejects a destroyed draft or window and preserves the existing target bytes");
        if ( !destroyWindow )
        {
            const bool neighborUnchanged = window && tabs && tabs->currentWidget() == neighbor
                    && neighborEditor && neighborEditor->toPlainText() == neighborText
                    && neighborEditor->document()->revision() == neighborRevision
                    && neighborEditor->document()->availableUndoSteps() == neighborUndoSteps
                    && neighborEditor->document()->isModified() == neighborDirty
                    && window->undoDepthForTest() == levelUndoDepth;
            if ( !neighborUnchanged )
            {
                std::cerr << "COG overwrite neighbor state: owner=" << bool(window)
                    << " tabs=" << bool(tabs)
                    << " selected=" << bool(tabs && tabs->currentWidget() == neighbor)
                    << " editor=" << bool(neighborEditor)
                    << " textEqual=" << bool(neighborEditor && neighborEditor->toPlainText() == neighborText)
                    << " revision=" << (neighborEditor ? neighborEditor->document()->revision() : -1) << '/' << neighborRevision
                    << " undo=" << (neighborEditor ? neighborEditor->document()->availableUndoSteps() : -1) << '/' << neighborUndoSteps
                    << " dirty=" << (neighborEditor ? neighborEditor->document()->isModified() : false) << '/' << neighborDirty
                    << " levelUndo=" << (window ? window->undoDepthForTest() : 0) << '/' << levelUndoDepth << std::endl;
            }
            check(neighborUnchanged,
                "destroyed COG overwrite leaves the independent neighbor's exact text, dirty marker, native Undo branch and Level history untouched");
        }
        delete window.data();
    }
}

void testCogDockTitleIconPreservesNativeChrome()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(QStringLiteral("title_icon_probe.cog"));
    QDockWidget* const dock = fixture.window ? fixture.window->findChild<QDockWidget*>(
        QStringLiteral("cogScriptEditorDock")) : nullptr;
    QLabel* const icon = dock ? dock->findChild<QLabel*>(QStringLiteral("cogEditorDockTitleIcon")) : nullptr;
    QAction* const tools = fixture.window ? fixture.window->findChild<QAction*>(
        QStringLiteral("tools.cog_script_editor")) : nullptr;
    check(dock && dock->widget() && !dock->titleBarWidget()
            && dock->features().testFlag(QDockWidget::DockWidgetMovable)
            && dock->features().testFlag(QDockWidget::DockWidgetFloatable)
            && dock->features().testFlag(QDockWidget::DockWidgetClosable),
        "COG dock icon: native titlebar and movable, floatable, closable features remain available");
    check(icon && icon->isVisible() && icon->size() == QSize(16, 16)
            && !icon->pixmap().isNull() && icon->testAttribute(Qt::WA_TransparentForMouseEvents)
            && tools && icon->pixmap().toImage() == dock->windowIcon().pixmap(QSize(16, 16)).toImage()
            && icon->pixmap().toImage() == tools->icon().pixmap(QSize(16, 16)).toImage()
            && icon->geometry().bottom() < dock->widget()->geometry().top(),
        "COG dock icon: the docked native grip displays a 16px icon without intercepting native title gestures");
    if ( !dock )
    {
        return;
    }
    dock->setFloating(true);
    dock->show();
    (void)settleTabLayout(fixture.tabs);
    check(dock->isFloating() && !dock->windowIcon().isNull() && (!icon || !icon->isVisible())
            && !dock->titleBarWidget(),
        "COG dock icon: floating keeps its native window icon and titlebar without an extra overlay");
    dock->setFloating(false);
    dock->show();
    dock->raise();
    (void)settleTabLayout(fixture.tabs);
    check(!dock->isFloating() && icon && icon->isVisible()
            && icon->geometry().bottom() < dock->widget()->geometry().top()
            && !dock->titleBarWidget() && fixture.tabs->currentWidget() == fixture.page,
        "COG dock icon: redocking restores the icon in the native grip and retains the same document page");
    QMainWindow* const workspace = fixture.window->findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
    if ( workspace )
    {
        workspace->resizeDocks({ dock }, { dock->minimumWidth() }, Qt::Horizontal);
        (void)settleTabLayout(fixture.tabs);
    }
    check(workspace && icon && icon->isVisible() && dock->rect().contains(icon->geometry())
            && icon->geometry().bottom() < dock->widget()->geometry().top()
            && fixture.tabs->currentWidget() == fixture.page,
        "COG dock icon: native minimum-width layout retains the icon above the unchanged document body");
}

void testUnsavedCogClosePromptNamesScript()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("named_close_probe"));
    if ( fixture.window )
    {
        fixture.window->markCurrentDocumentAsSavedForTest();
    }
    if ( fixture.editor )
    {
        fixture.editor->setPlainText(QStringLiteral(
            "symbols\nint changed\nend\ncode\nend\n"));
        fixture.editor->document()->setModified(true);
    }

    QString promptText{};
    QString promptTitle{};
    bool cancelWasAvailable = false;
    bool discardWasAvailable = false;
    bool saveWasDefault = false;
    bool cancelWasEscape = false;
    bool sharedPromptContract = false;
    bool promptWasOpened = false;
    bool cancelWasClicked = false;
    bool closeAccepted = false;
    if ( fixture.window )
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* message = qobject_cast<QMessageBox*>(
                    QApplication::activeModalWidget());
                if ( !message )
                {
                    return false;
                }
                promptWasOpened = true;
                promptText = message->text();
                promptTitle = message->windowTitle();
                cancelWasAvailable = message->button(QMessageBox::Cancel) != nullptr;
                discardWasAvailable = message->button(QMessageBox::Discard) != nullptr;
                sharedPromptContract =
                    message->objectName() == QStringLiteral("resource_authoring.unsaved_prompt") &&
                    message->parent() == nullptr &&
                    message->windowModality() == Qt::ApplicationModal &&
                    message->minimumSize().width() >= 560 && message->minimumSize().height() >= 220 &&
                    message->size().width() >= 560 && message->size().height() >= 220 &&
                    message->icon() == QMessageBox::Question &&
                    !message->windowFlags().testFlag(Qt::WindowMinimizeButtonHint) &&
                    !message->windowFlags().testFlag(Qt::WindowMaximizeButtonHint);
                saveWasDefault = message->defaultButton() == message->button(QMessageBox::Save);
                cancelWasEscape = message->escapeButton() == message->button(QMessageBox::Cancel);
                if ( QAbstractButton* cancel = message->button(QMessageBox::Cancel) )
                {
                    cancel->click();
                    cancelWasClicked = true;
                }
                return true;
            });
        closeAccepted = fixture.window->close();
    }

    check(fixture.editor && !closeAccepted && fixture.window->isVisible()
            && promptWasOpened && cancelWasAvailable && cancelWasClicked
            && discardWasAvailable && saveWasDefault && cancelWasEscape && sharedPromptContract
            && promptTitle == QStringLiteral("Unsaved COG script")
            && promptText == QStringLiteral("Save changes to named_close_probe.cog before closing?")
            && fixture.window->hasUnsavedCogScriptChangesForTest(),
        "unsaved COG close prompt matches the shared application-modal contract, names the script, and Cancel keeps its buffer open");
}

void testUnsavedCogClosePromptRejectKeepsBuffer()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("reject_close_probe"));
    if ( fixture.window )
    {
        fixture.window->markCurrentDocumentAsSavedForTest();
    }
    if ( fixture.editor )
    {
        fixture.editor->setPlainText(QStringLiteral("symbols\nint changed\nend\ncode\nend\n"));
        fixture.editor->document()->setModified(true);
    }

    bool promptWasRejected = false;
    bool closeAccepted = true;
    if ( fixture.window )
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if ( !message || message->objectName() != QStringLiteral("resource_authoring.unsaved_prompt") )
                {
                    return false;
                }
                message->reject();
                promptWasRejected = true;
                return true;
            });
        closeAccepted = fixture.window->close();
    }

    check(fixture.editor && !closeAccepted && promptWasRejected && fixture.window->isVisible()
            && fixture.window->hasUnsavedCogScriptChangesForTest(),
        "rejecting the shared COG unsaved prompt keeps the authored script open and dirty");
}

void testUnsavedCogOwnerMayBeDestroyedDuringPrompt()
{
    CogScriptTestWindow fixture = createVisibleCogScriptTestWindow(
        QStringLiteral("destroy_during_prompt_probe"));
    if ( fixture.window )
    {
        fixture.window->markCurrentDocumentAsSavedForTest();
    }
    if ( fixture.editor )
    {
        fixture.editor->setPlainText(QStringLiteral("symbols\nint changed\nend\ncode\nend\n"));
        fixture.editor->document()->setModified(true);
    }

    QPointer<QWidget> owner(fixture.page);
    bool promptWasOpened = false;
    bool promptWasParentless = false;
    bool closeAccepted = true;
    if ( owner )
    {
        ModalInvocationGuard invocation{};
        startInvocationPoller(invocation.context(), [&]()
            {
                auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if ( !message || message->objectName() != QStringLiteral("resource_authoring.unsaved_prompt") )
                {
                    return false;
                }
                promptWasOpened = true;
                promptWasParentless = message->parent() == nullptr;
                owner->deleteLater();
                return true;
            });
        QCloseEvent closeEvent{};
        QApplication::sendEvent(owner.data(), &closeEvent);
        closeAccepted = closeEvent.isAccepted();
    }

    check(owner.isNull() && promptWasOpened && promptWasParentless && !closeAccepted,
        "destroying a COG editor owner during its shared modal prompt rejects safely without deleting the prompt twice");
}

void testUnsavedCogResolutionCallbackMayDestroyEditor()
{
    for ( const bool discard : { false, true } )
    {
        QTemporaryDir temporary{};
        const bool temporaryReady = temporary.isValid();
        check(temporaryReady,
            "COG close callback lifetime: isolated project directory exists");
        if ( !temporaryReady )
        {
            continue;
        }

        const std::filesystem::path root(temporary.path().toStdWString());
        const std::filesystem::path projectPath = root / "close_callback_probe.sed";
        auto window = std::make_unique<MainWindow>();
        window->setAnimated(false);
        window->setCurrentPathForTitleForTest(projectPath);
        window->setResourcePathsForTest({ root }, {}, projectPath);
        window->createNewCogScriptForTest(discard
            ? QStringLiteral("discard_callback_probe.cog")
            : QStringLiteral("save_callback_probe.cog"));

        QTabWidget* const tabs = window->findChild<QTabWidget*>(
            QStringLiteral("cogScriptEditorTabs"));
        QPointer<QWidget> page(tabs ? tabs->currentWidget() : nullptr);
        QPlainTextEdit* const editor = page
            ? page->findChild<QPlainTextEdit*>() : nullptr;
        check(page && editor,
            "COG close callback lifetime: real draft and editor exist");
        if ( !page || !editor )
        {
            continue;
        }

        window->markCurrentDocumentAsSavedForTest();
        editor->setPlainText(QStringLiteral(
            "symbols\nint close_callback_changed\nend\ncode\nend\n"));
        editor->document()->setModified(true);
        int callbackCount = 0;
        if ( discard )
        {
            window->setCogDiscardCallbackForTest([page, &callbackCount]() mutable
                {
                    ++callbackCount;
                    delete page.data();
                });
        }
        else
        {
            window->setCogSaveCallbackForTest([page, &callbackCount]() mutable
                {
                    ++callbackCount;
                    delete page.data();
                });
        }
        window->show();
        (void)settleTabLayout(tabs);

        bool promptWasOpened = false;
        bool resolutionButtonClicked = false;
        bool closeAccepted = true;
        {
            ModalInvocationGuard invocation{};
            startInvocationPoller(invocation.context(), [&]()
                {
                    auto* const message = qobject_cast<QMessageBox*>(
                        QApplication::activeModalWidget());
                    if ( !message || message->objectName()
                            != QStringLiteral("resource_authoring.unsaved_prompt"))
                    {
                        return false;
                    }
                    promptWasOpened = true;
                    QAbstractButton* const button = message->button(discard
                        ? QMessageBox::Discard : QMessageBox::Save);
                    if ( !button )
                    {
                        message->reject();
                        return true;
                    }
                    button->click();
                    resolutionButtonClicked = true;
                    return true;
                });
            closeAccepted = window->close();
        }

        check(page.isNull() && callbackCount == 1 && promptWasOpened
                && resolutionButtonClicked && !closeAccepted,
            discard
                ? "COG Discard resolver may synchronously destroy its dialog and rejects the enclosing close safely"
                : "COG Save callback may synchronously destroy its dialog and rejects the enclosing close safely");
    }
}

} // namespace

void runCogOverwriteLifetimeTests()
{
    testCogOverwriteRejectsDestroyedOwner();
}

void runCogInteractionRegressionTests()
{
    testAllTypesMaskDiagnostic();
    testHoverDoesNotRecenterCogTabs();
    testTabScrollButtonsShowSharedStyleChevrons();
    testConciseResourcePickerTitles();
    testFlagsPopupAnchorCancelNoOpAndUndo();
    testFlagsAcceptRepairsInvalidAndAddsMissingRecord();
    testMaskPopupPreservesTargetTokenAndEngineBits();
    testMaskPopupUsesExactDuplicateAttributeSpan();
    testMaskInsertMenuAddAndToolbarFallback();
    testMaskInvalidTokenCancelAndAccept();
    testInlinePopupsKeepNavigationAfterSourceReplacement();
    testMaskEllipsisEditsExactAttributeOnce();
    testCogTemplatePickersUseSharedRichPicker();
    testCogTemplatePropertyPickerUsesStableSnapshotAndRejectsStaleOwner();
    testCogTemplateDoubleClickRejectsStaleOwner();
    testResourceEditorSaveShortcutsStayWithFocusedPane();
    testCogToolbarIconsKeepThemeSizeAndActions();
    testCogGroupFileActionsAndDocumentHistoryTools();
    testCogRestoredDocumentHistoryButtonsMatchNativeAvailability();
    testCogRemappedDocumentHistoryKeepsLevelHistory();
    testCogGroupSaveAllStopsCanceledFileWithoutLosingDrafts();
    testCogSuccessfulSaveNoticeAndCanceledSaveStatus();
    testCogSaveNoticeCanDeleteItsEditor();
    testCogDockTitleIconPreservesNativeChrome();
    testUnsavedCogClosePromptNamesScript();
    testUnsavedCogClosePromptRejectKeepsBuffer();
    testUnsavedCogOwnerMayBeDestroyedDuringPrompt();
    testUnsavedCogResolutionCallbackMayDestroyEditor();
}

} // namespace sed::app_tests
