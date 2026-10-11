#include "test_common.hpp"

#include "native_caption_test_utils.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"
#include "ui/cog/cog_document_history.hpp"
#include "ui/project/sed_editor_file_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

#include <QAction>
#include <QAbstractButton>
#include <QApplication>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QCursor>
#include <QDir>
#include <QDockWidget>
#include <QDrag>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLayout>
#include <QLayoutItem>
#include <QMainWindow>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMargins>
#include <QMoveEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPropertyAnimation>
#include <QProxyStyle>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QWindow>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace sed::app_tests
{
namespace
{

const QByteArray kCogSource("symbols\nint value=1\nend\ncode\nend\n");
bool cogNativeEdgeFailed{};

bool nativeMaximizeButtonClickRoundTrip(QDockWidget* const dock)
{
    return native_caption_test::maximizeButtonClickRoundTrip(dock);
}

class CogNativeAnimationStyle final : public QProxyStyle
{
public:
    explicit CogNativeAnimationStyle(QObject* const parent)
    {
        // QProxyStyle constructs its own base style. Do not adopt or destroy
        // QApplication's shared style when the detached dock is retired.
        setParent(parent);
    }

    int styleHint(const StyleHint hint, const QStyleOption* const option = nullptr,
        const QWidget* const widget = nullptr, QStyleHintReturn* const returnData = nullptr) const override
    {
        return hint == SH_Widget_Animation_Duration ? 1000
            : QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

bool writeCogFile(const std::filesystem::path& path, const QByteArray& bytes)
{
    std::error_code error{};
    std::filesystem::create_directories(path.parent_path(), error);
    QFile file(QString::fromStdWString(path.wstring()));
    return !error && file.open(QIODevice::WriteOnly)
        && file.write(bytes) == bytes.size();
}

bool writeSingleEntryCogArchive(
    const std::filesystem::path& path,
    const std::string_view entryName,
    const std::string_view entryBytes)
{
    if ( entryName.size() >= 128U
        || entryBytes.size() > std::numeric_limits<std::uint32_t>::max() )
    {
        return false;
    }
    std::error_code error{};
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::binary);
    if ( error || !output )
    {
        return false;
    }
    const auto writeU32 = [&output](const std::uint32_t value)
    {
        const std::array<char, 4> bytes{
            static_cast<char>(value & 0xffU),
            static_cast<char>((value >> 8U) & 0xffU),
            static_cast<char>((value >> 16U) & 0xffU),
            static_cast<char>((value >> 24U) & 0xffU) };
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    };
    constexpr std::uint32_t dataOffset = 16U + 8U + 128U;
    output.write("GOB ", 4);
    writeU32(0U);
    writeU32(0U);
    writeU32(1U);
    writeU32(dataOffset);
    writeU32(static_cast<std::uint32_t>(entryBytes.size()));
    std::array<char, 128> name{};
    std::copy(entryName.begin(), entryName.end(), name.begin());
    output.write(name.data(), static_cast<std::streamsize>(name.size()));
    output.write(entryBytes.data(), static_cast<std::streamsize>(entryBytes.size()));
    return static_cast<bool>(output);
}

QByteArray readCogFile(const std::filesystem::path& path)
{
    QFile file(QString::fromStdWString(path.wstring()));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

QByteArray canonical(const QCborValue& value)
{
    return ui::recovery::serializeCanonicalCbor(value);
}

void bindCogProject(MainWindow& window, const std::filesystem::path& root)
{
    editor::DocumentState document{};
    document.level.kind = ProjectType::JKDF2;
    document.level.header.version = 1;
    document.level.layers = {"Base"};
    window.setDocumentStateForTest(std::move(document));
    window.setProjectTypeForTest(ProjectType::JKDF2);
    window.setCurrentPathForTitleForTest(root / "docking.sed");
    window.setResourcePathsForTest({root}, {}, root / "docking.sed");
    window.setAnimated(false);
    if ( auto* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow")) )
    {
        workspace->setAnimated(false);
    }
    const QRect available = window.screen() ? window.screen()->availableGeometry() : QRect(0, 0, 1280, 900);
    window.resize(std::min(1280, available.width()), std::min(860, available.height()));
    window.move(available.topLeft());
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();
}

QTabWidget* cogHostForPage(const MainWindow& window, const QWidget* page)
{
    for ( QTabWidget* const tabs : window.findChildren<QTabWidget*>() )
    {
        if ( tabs->indexOf(const_cast<QWidget*>(page)) >= 0 )
        {
            return tabs;
        }
    }
    return nullptr;
}

QWidget* cogPage(const MainWindow& window, const QString& name)
{
    for ( QTabWidget* const tabs : window.findChildren<QTabWidget*>() )
    {
        for ( int index = 0; index < tabs->count(); ++index )
        {
            QWidget* const page = tabs->widget(index);
            if ( page && QFileInfo(page->property("cogFullPath").toString()).fileName() == name )
            {
                return page;
            }
        }
    }
    return nullptr;
}

QDockWidget* dockForHost(QWidget* widget)
{
    for ( QWidget* current = widget; current; current = current->parentWidget() )
    {
        if ( auto* const dock = qobject_cast<QDockWidget*>(current) )
        {
            return dock;
        }
    }
    return nullptr;
}

QCborMap cogWorkspace(const MainWindow& window)
{
    return QCborValue::fromCbor(window.captureCogEditorRecoveryStateForTest()).toMap();
}

QCborMap cogPageState(const MainWindow& window, const QString& name)
{
    for ( const QCborValue& value : cogWorkspace(window).value(QStringLiteral("tabs")).toArray() )
    {
        const QCborMap page = value.toMap();
        if ( page.value(QStringLiteral("name")).toString() == name )
        {
            return page;
        }
    }
    return {};
}

// Window resizing may change pane allocations. This projection retains every
// document/identity/history field, including the exact text selection.
// Recovery recomputes diagnosticsExpandedHeight from the applied splitter
// allocation; it is presentation geometry, while diagnosticsExpanded remains.
QCborMap cogDocumentState(const MainWindow& window, const QString& name)
{
    QCborMap page = cogPageState(window, name);
    for ( const QString& key : { QStringLiteral("splitter"), QStringLiteral("outlineSplitter"),
              QStringLiteral("diagnosticsSplitter"), QStringLiteral("diagnosticsExpandedHeight"),
              QStringLiteral("vscroll"), QStringLiteral("hscroll") } )
    {
        page.remove(key);
    }
    return page;
}

void reportCogStateDifferences(const QCborMap& expected, const QCborMap& actual)
{
    for ( auto it = expected.cbegin(); it != expected.cend(); ++it )
    {
        if ( canonical(it.value()) != canonical(actual.value(it.key())) )
        {
            std::cerr << "COG restore field diagnostic: key=" << it.key().toString().toStdString()
                << " expected=" << canonical(it.value()).toHex().left(256).constData()
                << " actual=" << canonical(actual.value(it.key())).toHex().left(256).constData() << '\n';
        }
    }
}

QString cogWidgetDescription(const QWidget* const widget)
{
    return widget ? QStringLiteral("%1:%2").arg(QString::fromLatin1(widget->metaObject()->className()),
        widget->objectName()) : QStringLiteral("<none>");
}

void appendCogCommand(QPlainTextEdit* editor, const QString& text)
{
    if ( !editor )
    {
        return;
    }
    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::End);
    cursor.beginEditBlock();
    cursor.insertText(text);
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
}

bool openCogFixture(MainWindow& window, const std::filesystem::path& root)
{
    bool written = true;
    for ( const char* const name : { "first.cog", "second.cog", "third.cog" } )
    {
        written = writeCogFile(root / "cog" / name, kCogSource) && written;
    }
    bindCogProject(window, root);
    for ( const char* const name : { "first.cog", "second.cog", "third.cog" } )
    {
        window.openCogScriptEditorForTest(name);
    }
    QApplication::processEvents();
    QTabWidget* const primary = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    const bool ready = written && primary && primary->count() == 3
        && cogPage(window, QStringLiteral("first.cog"))
        && cogPage(window, QStringLiteral("second.cog"))
        && cogPage(window, QStringLiteral("third.cog"));
    check(ready, "COG docking fixture opens three independent real loose source files in the primary script group");
    return ready;
}

std::optional<QPoint> outsideTabPoint(MainWindow& window, QTabBar* bar)
{
    // A previous Dock return may still have a posted dock-layout request.
    // Settle it before selecting a point outside the final native group.
    QApplication::processEvents();
    auto* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
    QWidget* const center = workspace ? workspace->centralWidget() : nullptr;
    QDockWidget* const sourceDock = dockForHost(bar);
    if ( !bar || !center || !sourceDock )
    {
        return std::nullopt;
    }
    const QRect available = window.screen() ? window.screen()->availableGeometry() : QRect(0, 0, 1280, 900);
    std::vector<QRect> hostRects{};
    for ( QDockWidget* const dock : window.findChildren<QDockWidget*>() )
    {
        if ( dock && dock->isVisible() && dock->property("sed.cogScriptEditorDock").toBool() )
        {
            hostRects.push_back(dock->isWindow() ? dock->frameGeometry()
                : QRect(dock->mapToGlobal(QPoint{}), dock->size()));
        }
    }
    const QRect inset = available.adjusted(4, 4, -4, -4);
    std::vector<QPoint> candidates{inset.bottomRight(), inset.topRight(), inset.bottomLeft(), inset.topLeft()};
    std::stable_sort(candidates.begin(), candidates.end(), [&window](const QPoint& left, const QPoint& right)
        {
            return !window.frameGeometry().contains(left) && window.frameGeometry().contains(right);
        });
    candidates.push_back(center->mapToGlobal(center->rect().center()));
    candidates.push_back(window.mapToGlobal(QPoint(window.width() / 2, 4)));
    for ( int row = 0; row < 5; ++row )
    {
        for ( int column = 0; column < 5; ++column )
        {
            candidates.emplace_back(inset.left() + column * inset.width() / 4,
                inset.top() + row * inset.height() / 4);
        }
    }
    for ( const QPoint& candidate : candidates )
    {
        if ( !available.contains(candidate)
            || std::any_of(hostRects.begin(), hostRects.end(), [&candidate](const QRect& rect)
                {
                    return rect.contains(candidate);
                }) )
        {
            continue;
        }
        bool onTabBar = false;
        for ( QWidget* widget = QApplication::widgetAt(candidate); widget; widget = widget->parentWidget() )
        {
            onTabBar = onTabBar || qobject_cast<QTabBar*>(widget);
        }
        if ( !onTabBar )
        {
            return candidate;
        }
    }
    return std::nullopt;
}

bool dragCogTab(QTabWidget* tabs, QWidget* page, const QPoint& destination,
    const bool cancel = false, QWidget* escapeTarget = nullptr)
{
    QTabBar* const bar = tabs ? tabs->tabBar() : nullptr;
    const int index = tabs ? tabs->indexOf(page) : -1;
    if ( !bar || index < 0 )
    {
        return false;
    }
    const QPoint start = bar->tabRect(index).center();
    const QPoint startGlobal = bar->mapToGlobal(start);
    const QPoint end = bar->mapFromGlobal(destination);
    const QPoint previousCursor = QCursor::pos();
    const QString destinationWidget = cogWidgetDescription(QApplication::widgetAt(destination));
    QCursor::setPos(startGlobal);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(start), QPointF(start), QPointF(startGlobal),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(bar, &press);
    QEventLoop loop{};
    QTimer watchdog{};
    watchdog.setSingleShot(true);
    bool terminal{};
    bool timedOut{};
    QPointer<QTabBar> guardedBar(bar);
    QPointer<QWidget> guardedEscape(escapeTarget ? escapeTarget : bar);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, [&]()
        {
            timedOut = true;
            QDrag::cancel();
            loop.quit();
        });
    watchdog.start(2000);
    QTimer::singleShot(0, &loop, [&]()
        {
            QTimer::singleShot(0, &loop, [&]()
                {
                    if ( guardedBar )
                    {
                        if ( cancel && guardedEscape )
                        {
                            guardedEscape->setFocus(Qt::OtherFocusReason);
                            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                            QApplication::sendEvent(guardedEscape, &escape);
                        }
                        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(end), QPointF(end),
                            QPointF(destination), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                        QApplication::sendEvent(guardedBar, &release);
                        terminal = true;
                    }
                    loop.quit();
                });
            if ( guardedBar )
            {
                QCursor::setPos(destination);
                QMouseEvent move(QEvent::MouseMove, QPointF(end), QPointF(end), QPointF(destination),
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(guardedBar, &move);
            }
        });
    loop.exec();
    watchdog.stop();
    QCursor::setPos(previousCursor);
    QApplication::processEvents();
    if ( guardedBar )
    {
        guardedBar->setProperty("sed.cog_test.dragDispatch", QStringLiteral(
            "terminal=%1 timedOut=%2 cancelRequested=%3 start=%4,%5 destination=%6,%7 widget=%8 active=%9")
            .arg(terminal).arg(timedOut).arg(cancel).arg(startGlobal.x()).arg(startGlobal.y())
            .arg(destination.x()).arg(destination.y()).arg(destinationWidget)
            .arg(cogWidgetDescription(QApplication::activeWindow())));
    }
    return terminal && !timedOut;
}

bool detachCogPage(MainWindow& window, QWidget* page)
{
    const QPointer<QTabWidget> source(cogHostForPage(window, page));
    const auto destination = outsideTabPoint(window, source ? source->tabBar() : nullptr);
    const bool dispatched = destination && dragCogTab(source.data(), page, *destination);
    QTabWidget* const target = cogHostForPage(window, page);
    QDockWidget* const dock = dockForHost(target);
    const bool detached = dispatched && target && target != source && dock && dock->isFloating();
    if ( !detached )
    {
        QTabBar* const bar = source ? source->tabBar() : nullptr;
        std::cerr << "COG detach diagnostic: dispatched=" << dispatched
            << " source=" << cogWidgetDescription(source.data()).toStdString()
            << " target=" << cogWidgetDescription(target).toStdString()
            << " floating=" << (dock && dock->isFloating())
            << " generation=" << (dock ? dock->property("sed.cogScriptEditorRecoveryGeneration").toULongLong() : 0)
            << " dispatch=" << (bar ? bar->property("sed.cog_test.dragDispatch").toString().toStdString() : "<destroyed>")
            << " trace=" << (bar ? bar->property("resource_authoring.dragTrace").toString().toStdString() : "<destroyed>") << '\n';
    }
    return detached;
}

void focusCogPage(MainWindow& window, QWidget* page)
{
    QTabWidget* const tabs = cogHostForPage(window, page);
    QDockWidget* const dock = dockForHost(tabs);
    if ( tabs && dock )
    {
        tabs->setCurrentWidget(page);
        dock->show();
        dock->raise();
        dock->activateWindow();
        if ( auto* const editor = page->findChild<QPlainTextEdit*>() )
        {
            editor->setFocus(Qt::OtherFocusReason);
        }
        QApplication::processEvents();
    }
}

void reportCogNativeCaptionFailure(const char* const phase, QDockWidget* const source,
    QDockWidget* const target, const QPoint start, const QPoint destination)
{
    if ( !source )
    {
        std::cerr << "COG native caption: phase=" << phase << " source=<destroyed>\n";
        return;
    }
    const auto printRect = [](const QRect rectangle)
    {
        std::cerr << rectangle.x() << ',' << rectangle.y() << ',' << rectangle.width() << ',' << rectangle.height();
    };
    QWindow* const handle = source->windowHandle();
    const QMargins margins = handle ? handle->frameMargins() : QMargins{};
    std::cerr << "COG native caption: phase=" << phase << " object=" << source->objectName().toStdString()
        << " flags=" << static_cast<quint64>(source->windowFlags())
        << " handleFlags=" << (handle ? static_cast<quint64>(handle->flags()) : 0)
        << " nativeId=" << static_cast<quint64>(source->internalWinId())
        << " floating=" << source->isFloating() << " window=" << source->isWindow()
        << " visible=" << source->isVisible() << " customTitle=" << (source->titleBarWidget() != nullptr)
        << " native=" << source->testAttribute(Qt::WA_NativeWindow)
        << " created=" << source->testAttribute(Qt::WA_WState_Created)
        << " parent=" << cogWidgetDescription(source->parentWidget()).toStdString() << " frame=";
    printRect(source->frameGeometry());
    std::cerr << " client=";
    printRect(source->geometry());
    std::cerr << " windowFrame=";
    printRect(handle ? handle->frameGeometry() : QRect{});
    std::cerr << " margins=" << margins.left() << ',' << margins.top() << ',' << margins.right() << ',' << margins.bottom()
        << " minimum=" << source->minimumWidth() << ',' << source->minimumHeight()
        << " minimumHint=" << source->minimumSizeHint().width() << ',' << source->minimumSizeHint().height()
        << " start=" << start.x() << ',' << start.y()
        << " destination=" << destination.x() << ',' << destination.y() << " screen=";
    printRect(source->screen() ? source->screen()->availableGeometry() : QRect{});
    std::cerr << " target=" << cogWidgetDescription(target).toStdString()
        << " targetVisible=" << (target && target->isVisible()) << " targetRect=";
    printRect(target ? QRect(target->mapToGlobal(QPoint{}), target->size()) : QRect{});
    std::cerr << " widgetAt=" << cogWidgetDescription(QApplication::widgetAt(destination)).toStdString()
        << " mouseGrab=" << cogWidgetDescription(QWidget::mouseGrabber()).toStdString() << '\n';
}

QSize prospectiveCogDockedMinimum(QDockWidget* const dock)
{
    if ( !dock || !dock->widget() )
    {
        return {};
    }
    // Qt 6.10.2 QDockWidgetItem::minimumSize uses sizeFromContent(...,
    // false), even for a currently native floating dock. Its public floating
    // minimum excludes the docked title, so it cannot size the prospective gap.
    QWidgetItem content(dock->widget());
    const QSize contentMinimum = content.minimumSize();
    const bool vertical = dock->features().testFlag(QDockWidget::DockWidgetVerticalTitleBar);
    const auto alongTitle = [vertical](const QSize size) { return vertical ? size.height() : size.width(); };
    const auto acrossTitle = [vertical](const QSize size) { return vertical ? size.width() : size.height(); };
    int titleHeight{};
    int titleMinimum{};
    if ( QWidget* const title = dock->titleBarWidget() )
    {
        titleHeight = acrossTitle(title->sizeHint());
        titleMinimum = alongTitle(title->minimumSizeHint());
    }
    else
    {
        const auto buttonSize = [dock](const char* const name)
        {
            auto* const button = dock->findChild<QAbstractButton*>(QString::fromLatin1(name));
            return button ? button->sizeHint() : QSize(0, 0);
        };
        const QSize close = buttonSize("qt_dockwidget_closebutton");
        const QSize floating = buttonSize("qt_dockwidget_floatbutton");
        const int margin = dock->style()->pixelMetric(QStyle::PM_DockWidgetTitleMargin, nullptr, dock);
        const int frame = dock->style()->pixelMetric(QStyle::PM_DockWidgetFrameWidth, nullptr, dock);
        // Match QDockWidgetLayout::titleHeight and minimumTitleWidth using
        // the actual native buttons, font and style, without altering panels.
        titleHeight = std::max(std::max(acrossTitle(close), acrossTitle(floating)) + 2,
            dock->fontMetrics().height() + 2 * margin);
        titleMinimum = (dock->features().testFlag(QDockWidget::DockWidgetClosable) ? alongTitle(close) : 0)
            + (dock->features().testFlag(QDockWidget::DockWidgetFloatable) ? alongTitle(floating) : 0)
            + titleHeight + 2 * frame + 3 * margin;
    }
    const QSize result = vertical
        ? QSize(std::max(0, contentMinimum.width()) + titleHeight,
              std::max(contentMinimum.height(), titleMinimum))
        : QSize(std::max(contentMinimum.width(), titleMinimum),
              std::max(0, contentMinimum.height()) + titleHeight);
    return result.expandedTo(dock->minimumSize());
}

std::string cogNativeLayoutDiagnostic(const char* const phase, QMainWindow* const workspace,
    QDockWidget* const source, QDockWidget* const target, const QSize required = {})
{
    std::ostringstream output{};
    const auto size = [&output](const QSize value) { output << value.width() << ',' << value.height(); };
    const auto rectangle = [&output](const QRect value)
    {
        output << value.x() << ',' << value.y() << ',' << value.width() << ',' << value.height();
    };
    output << "COG native layout: phase=" << phase << " required=";
    size(required);
    output << " workspace=";
    rectangle(workspace ? workspace->rect() : QRect{});
    output << " layoutRect=";
    rectangle(workspace && workspace->layout() ? workspace->layout()->geometry() : QRect{});
    output << " layoutMinimum=";
    size(workspace && workspace->layout() ? workspace->layout()->minimumSize() : QSize{});
    const auto dockSizes = [&output, &size](const char* const label, QDockWidget* const dock)
    {
        output << ' ' << label << '=' << cogWidgetDescription(dock).toStdString() << " dockMinimum=";
        size(dock ? dock->minimumSize() : QSize{});
        output << " prospective=";
        size(prospectiveCogDockedMinimum(dock));
        output << " bodyMinimum=";
        QSize bodyMinimum{};
        if ( dock && dock->widget() )
        {
            QWidgetItem body(dock->widget());
            bodyMinimum = body.minimumSize();
        }
        size(bodyMinimum);
        const QMargins margins = dock ? dock->contentsMargins() : QMargins{};
        output << " margins=" << margins.left() << ',' << margins.top() << ',' << margins.right() << ',' << margins.bottom();
    };
    dockSizes("source", source);
    dockSizes("target", target);
    QWidget* const gap = workspace ? workspace->findChild<QWidget*>(QStringLiteral("qt_rubberband")) : nullptr;
    output << " gap=" << cogWidgetDescription(gap).toStdString() << " gapVisible=" << (gap && gap->isVisible()) << " gapRect=";
    rectangle(gap ? gap->geometry() : QRect{});
    output << '\n';
    if ( workspace )
    {
        const auto bars = workspace->findChildren<QTabBar*>(QString{}, Qt::FindDirectChildrenOnly);
        output << "COG native layout bars: count=" << bars.size();
        for ( int index = 0; index < std::min<qsizetype>(bars.size(), 8); ++index )
        {
            output << " bar=" << cogWidgetDescription(bars[index]).toStdString() << " minimum=";
            size(bars[index]->minimumSizeHint());
            output << " geometry=";
            rectangle(bars[index]->geometry());
        }
        output << '\n';
        const auto animations = workspace->findChildren<QPropertyAnimation*>();
        output << "COG native layout animations: count=" << animations.size() << '\n';
        for ( int index = 0; index < std::min<qsizetype>(animations.size(), 8); ++index )
        {
            QPropertyAnimation* const animation = animations[index];
            output << "COG native layout animation: parent="
                << cogWidgetDescription(qobject_cast<QWidget*>(animation->parent())).toStdString()
                << " target=" << cogWidgetDescription(qobject_cast<QWidget*>(animation->targetObject())).toStdString()
                << " property=" << animation->propertyName().toStdString()
                << " state=" << static_cast<int>(animation->state()) << " duration=" << animation->duration() << '\n';
        }
    }
    return output.str();
}

class CogNativeClientMoveObserver final : public QObject
{
public:
    CogNativeClientMoveObserver(QDockWidget* const dock, const QPoint clientPress, const QPoint destination)
        : dock_(dock), clientPress_(clientPress), destination_(destination)
    {
        if ( dock_ )
        {
            dock_->installEventFilter(this);
        }
    }

    bool exact() const { return exact_; }
    QPoint observedClient() const { return observedClient_; }
    QPoint observedHover() const { return observedClient_ + clientPress_; }

protected:
    bool eventFilter(QObject* const watched, QEvent* const event) override
    {
        if ( dock_ && watched == dock_.data() && event->type() == QEvent::Move )
        {
            observedClient_ = static_cast<QMoveEvent*>(event)->pos();
            exact_ = observedClient_ + clientPress_ == destination_;
        }
        return false;
    }

private:
    QPointer<QDockWidget> dock_{};
    QPoint clientPress_{};
    QPoint destination_{};
    QPoint observedClient_{};
    bool exact_{};
};

bool moveNativeCogCaptionTo(QDockWidget* const dock, const QPoint clientPress,
    const QPoint destination, std::string* const diagnostic)
{
    const QPointer<QDockWidget> guardedDock(dock);
    const QPointer<QWindow> nativeWindow(dock ? dock->windowHandle() : nullptr);
    if ( !guardedDock || !nativeWindow )
    {
        return false;
    }
    const QPoint previousClient = nativeWindow->geometry().topLeft();
    CogNativeClientMoveObserver observed(dock, clientPress, destination);
    // Qt 6.10.2 QWidget::move (qwidget.cpp6804) reports frame coordinates.
    // The real native path (qwidgetwindow.cpp774-797) forwards QWindow's
    // client origin, which QDockWidgetPrivate::moveEvent adds to clientPress.
    nativeWindow->setPosition(destination - clientPress);
    bool translatedNativeMove{};
    if ( guardedDock && nativeWindow && !observed.exact() )
    {
        // If the platform notification is still queued, deliver the same
        // public client-coordinate event QWidgetWindow forwards. Its position
        // comes from the actual native window, never a compensated cursor.
        // Keep the native gesture synchronous until its intended terminal:
        // pumping events here can deliver another no-button NC mouse move,
        // which qdockwidget.cpp1113-1120 treats as endDrag before this move.
        QMoveEvent move(nativeWindow->geometry().topLeft(), previousClient);
        translatedNativeMove = true;
        QApplication::sendEvent(guardedDock.data(), &move);
    }
    const bool exact = guardedDock && nativeWindow && observed.exact();
    if ( diagnostic )
    {
        std::ostringstream output{};
        output << "COG native client move: expectedHover=" << destination.x() << ',' << destination.y()
            << " clientPress=" << clientPress.x() << ',' << clientPress.y()
            << " observedClient=" << observed.observedClient().x() << ',' << observed.observedClient().y()
            << " observedHover=" << observed.observedHover().x() << ',' << observed.observedHover().y()
            << " nativeClient=" << (nativeWindow ? nativeWindow->geometry().x() : 0) << ','
            << (nativeWindow ? nativeWindow->geometry().y() : 0)
            << " translatedNativeMove=" << translatedNativeMove << " exact=" << exact << '\n';
        *diagnostic += output.str();
    }
    return exact;
}

bool allocateCogNativeDropTarget(QDockWidget* const target, QMainWindow* const workspace,
    const QSize required)
{
    if ( !target || !workspace )
    {
        return false;
    }
    const QPointer<QDockWidget> guardedTarget(target);
    const QPointer<QMainWindow> guardedWorkspace(workspace);
    const QPointer<QWidget> owner(workspace->window());
    const QRect available = owner && owner->screen() ? owner->screen()->availableGeometry() : QRect{};
    if ( !owner || !available.isValid() )
    {
        return false;
    }
    const QSize frameChrome = owner->frameGeometry().size() - owner->size();
    const QSize maximumOwner = available.size() - frameChrome;
    // Qt inserts a gap into the actual dock area, not the full workspace.
    // Retain all panels and grow the measured allocation before the gesture;
    // resizeDocks only prepares that allocation and never performs the drop.
    for ( int pass = 0; pass < 8 && guardedTarget && guardedWorkspace && owner; ++pass )
    {
        const QSize workspaceMinimum = guardedWorkspace->layout()
            ? guardedWorkspace->layout()->minimumSize() : QSize(0, 0);
        const QSize deficit(std::max({0, required.width() - guardedTarget->width(),
                workspaceMinimum.width() - guardedWorkspace->width()}),
            std::max({0, required.height() - guardedTarget->height(),
                workspaceMinimum.height() - guardedWorkspace->height()}));
        if ( deficit.width() == 0 && deficit.height() == 0 )
        {
            return true;
        }
        owner->resize(std::min(maximumOwner.width(), owner->width() + deficit.width()),
            std::min(maximumOwner.height(), owner->height() + deficit.height()));
        if ( !guardedTarget || !guardedWorkspace || !owner )
        {
            return false;
        }
        guardedWorkspace->resizeDocks({guardedTarget.data()}, {required.width()}, Qt::Horizontal);
        if ( !guardedTarget || !guardedWorkspace || !owner )
        {
            return false;
        }
        guardedWorkspace->resizeDocks({guardedTarget.data()}, {required.height()}, Qt::Vertical);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QApplication::processEvents();
    }
    const QSize workspaceMinimum = guardedWorkspace && guardedWorkspace->layout()
        ? guardedWorkspace->layout()->minimumSize() : QSize(0, 0);
    const bool feasible = guardedTarget && guardedWorkspace && owner
        && guardedTarget->width() >= required.width() && guardedTarget->height() >= required.height()
        && guardedWorkspace->width() >= workspaceMinimum.width()
        && guardedWorkspace->height() >= workspaceMinimum.height();
    if ( !feasible )
    {
        std::cerr << "COG native target allocation: required=" << required.width() << ',' << required.height()
            << " actual=" << (guardedTarget ? guardedTarget->width() : 0) << ','
            << (guardedTarget ? guardedTarget->height() : 0)
            << " workspace=" << (guardedWorkspace ? guardedWorkspace->width() : 0) << ','
            << (guardedWorkspace ? guardedWorkspace->height() : 0)
            << " workspaceMinimum=" << workspaceMinimum.width() << ',' << workspaceMinimum.height()
            << " owner=" << (owner ? owner->width() : 0) << ',' << (owner ? owner->height() : 0)
            << " maximumOwner=" << maximumOwner.width() << ',' << maximumOwner.height() << '\n';
    }
    return feasible;
}

bool dragNativeCogCaption(QDockWidget* dock, QDockWidget* target, QMainWindow* workspace,
    const bool settle = true)
{
    if ( !dock || !target || !workspace || !dock->isFloating() || dock->titleBarWidget() )
    {
        return false;
    }
    const QPointer<QDockWidget> guardedDock(dock);
    const QPointer<QDockWidget> guardedTarget(target);
    const QPointer<QMainWindow> guardedWorkspace(workspace);
    QWidget* const owner = workspace->window();
    const QRect available = owner->screen() ? owner->screen()->availableGeometry() : QRect{};
    const int separator = workspace->style()->pixelMetric(QStyle::PM_DockWidgetSeparatorExtent);
    const QSize sourceMinimum = prospectiveCogDockedMinimum(dock);
    const QSize targetMinimum = prospectiveCogDockedMinimum(target);
    const QSize verticalMinimum(std::max(sourceMinimum.width(), targetMinimum.width()),
        sourceMinimum.height() + targetMinimum.height() + separator);
    const int frameHeight = owner->frameGeometry().height() - owner->height();
    const bool vertical = owner->height() + std::max(0, verticalMinimum.height() - target->height())
        <= available.height() - frameHeight;
    const QSize required = vertical ? verticalMinimum
        : QSize(sourceMinimum.width() + targetMinimum.width() + separator,
              std::max(sourceMinimum.height(), targetMinimum.height()));
    const bool feasible = allocateCogNativeDropTarget(target, workspace, required)
        && guardedDock && guardedTarget && guardedWorkspace;
    check(feasible, "native COG edge docking retains full editor panels and provides both docks' actual minima in a feasible screen-bounded orientation");
    if ( !feasible )
    {
        return false;
    }
    const QRect frame = dock->frameGeometry();
    const QRect client = dock->geometry();
    const QRect caption(client.left(), frame.top(), client.width(), client.top() - frame.top());
    const QPoint startGlobal = caption.center();
    // Qt's non-client translation and real native move both use client
    // coordinates. Keep the stored press and the observed hover in that space.
    const QPoint start = dock->mapFromGlobal(startGlobal);
    const QRect targetRect(target->mapToGlobal(QPoint{}), target->size());
    QPoint destination = vertical ? QPoint(targetRect.center().x(), targetRect.bottom() - 6)
        : QPoint(targetRect.right() - 6, targetRect.center().y());
    if ( caption.height() <= 0 || !dock->screen()
        || !dock->screen()->availableGeometry().contains(startGlobal)
        || !dock->screen()->availableGeometry().contains(destination) )
    {
        reportCogNativeCaptionFailure("edge precondition", dock, target, startGlobal, destination);
        return false;
    }
    const QPoint previousCursor = QCursor::pos();
    std::string layoutTrace = cogNativeLayoutDiagnostic("edge before press", workspace, dock, target, required);
    QCursor::setPos(startGlobal);
    QMouseEvent press(QEvent::NonClientAreaMouseButtonPress, QPointF(start), QPointF(startGlobal),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(dock, &press);
    layoutTrace += cogNativeLayoutDiagnostic("edge after press", guardedWorkspace, guardedDock, guardedTarget, required);
    if ( guardedTarget )
    {
        const QRect currentTarget(guardedTarget->mapToGlobal(QPoint{}), guardedTarget->size());
        destination = vertical ? QPoint(currentTarget.center().x(), currentTarget.bottom() - 6)
            : QPoint(currentTarget.right() - 6, currentTarget.center().y());
    }
    QCursor::setPos(destination);
    bool exactNativeMove{};
    if ( guardedDock )
    {
        exactNativeMove = moveNativeCogCaptionTo(guardedDock, start, destination, &layoutTrace);
        layoutTrace += cogNativeLayoutDiagnostic("edge after native move", guardedWorkspace, guardedDock, guardedTarget, required);
        if ( guardedDock )
        {
            QMouseEvent release(QEvent::NonClientAreaMouseMove,
                QPointF(guardedDock->mapFromGlobal(destination)), QPointF(destination),
                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(guardedDock, &release);
        }
    }
    QElapsedTimer deadline{};
    deadline.start();
    while ( settle && guardedDock && guardedDock->isFloating() && deadline.elapsed() < 2000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    QCursor::setPos(previousCursor);
    const bool docked = exactNativeMove && guardedDock && guardedWorkspace && !guardedDock->isFloating()
        && guardedWorkspace->dockWidgetArea(guardedDock) != Qt::NoDockWidgetArea;
    if ( settle && !docked )
    {
        reportCogNativeCaptionFailure("edge after native release", guardedDock, guardedTarget, startGlobal, destination);
        std::cerr << layoutTrace;
    }
    if ( !settle && guardedDock )
    {
        guardedDock->setProperty("sed.cog_test.nativeLayoutTrace", QString::fromStdString(layoutTrace));
    }
    return settle ? docked : exactNativeMove && guardedDock && guardedTarget && guardedWorkspace;
}

void checkCogDetachTransferAndDockReturn()
{
    cogNativeEdgeFailed = false;
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    MainWindow window(nullptr, true);
    if ( !temporary.isValid() || !openCogFixture(window, root) )
    {
        return;
    }
    QTabWidget* const primary = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QDockWidget* const primaryDock = dockForHost(primary);
    QPointer<QWidget> first(cogPage(window, QStringLiteral("first.cog")));
    QPointer<QWidget> second(cogPage(window, QStringLiteral("second.cog")));
    QPointer<QPlainTextEdit> editor(first ? first->findChild<QPlainTextEdit*>() : nullptr);
    QPointer<QTextDocument> document(editor ? editor->document() : nullptr);
    if ( !first || !second || !editor || !primaryDock )
    {
        check(false, "COG detach resolves real source editor and native document pointers");
        return;
    }
    appendCogCommand(editor, QStringLiteral("# raw unclosed \"\n"));
    const QString current = editor->toPlainText();
    appendCogCommand(editor, QStringLiteral("# redo \U0001f642\n"));
    const QString future = editor->toPlainText();
    editor->undo();
    QTextCursor selected(editor->document());
    selected.setPosition(3);
    selected.setPosition(9, QTextCursor::KeepAnchor);
    editor->setTextCursor(selected);
    primary->setCurrentWidget(first);
    check(window.triggerCogScriptTabContextActionForTest(primary->indexOf(first), QStringLiteral("Pin tab")),
        "COG detach fixture pins the real script tab through its public tab action");
    const QByteArray before = canonical(cogDocumentState(window, QStringLiteral("first.cog")));
    const QPointer<QWidget> body(primaryDock->widget());
    const QPointer<QWidget> titleIcon(primaryDock->findChild<QWidget*>(QStringLiteral("cogEditorDockTitleIcon")));
    check(body && body->isVisible() && titleIcon && titleIcon->isVisible()
            && !primaryDock->titleBarWidget(),
        "the native docked COG header paints its icon above the visible document body");
    if ( body && titleIcon )
    {
        body->hide();
        QApplication::processEvents();
        check(titleIcon && titleIcon->isHidden() && !titleIcon->isVisible(),
            "hiding the COG body leaves its native header icon explicitly hidden without recreating visible children");
        body->show();
        QApplication::processEvents();
        check(titleIcon && titleIcon->isVisible() && !titleIcon->isHidden(),
            "showing the same COG body restores its docked native header icon");
        primaryDock->hide();
        QApplication::processEvents();
        check(titleIcon && titleIcon->isHidden() && !titleIcon->isVisible(),
            "hiding the COG dock also explicitly hides the native header icon");
        primaryDock->show();
        QApplication::processEvents();
        check(titleIcon && titleIcon->isVisible() && !titleIcon->isHidden()
                && canonical(cogDocumentState(window, QStringLiteral("first.cog"))) == before,
            "showing the COG dock restores its native header icon while retaining the exact draft and document history");
    }
    const QByteArray beforeWholeGroupFloat = canonical(cogDocumentState(window, QStringLiteral("first.cog")));
    QPointer<QToolButton> wholeGroupReturn(primaryDock->findChild<QToolButton*>(
        QStringLiteral("cogEditorDockReturnButton")));
    QWidget* const wholeGroupToolbar = primaryDock->findChild<QWidget*>(QStringLiteral("cogEditorGroupToolbar"));
    primaryDock->setFloating(true);
    primaryDock->show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const Qt::WindowFlags primaryFlags = primaryDock->windowFlags();
    check(primaryDock->isFloating() && primaryDock->isWindow() && !primaryDock->titleBarWidget()
            && !primaryFlags.testFlag(Qt::FramelessWindowHint)
            && primaryFlags.testFlag(Qt::WindowTitleHint)
            && primaryFlags.testFlag(Qt::WindowSystemMenuHint)
            && primaryFlags.testFlag(Qt::WindowCloseButtonHint)
            && primaryFlags.testFlag(Qt::WindowMaximizeButtonHint)
            && primaryDock->features().testFlag(QDockWidget::DockWidgetMovable)
            && primaryDock->features().testFlag(QDockWidget::DockWidgetFloatable)
            && primaryDock->features().testFlag(QDockWidget::DockWidgetClosable)
            && wholeGroupToolbar && wholeGroupToolbar->isVisible()
            && wholeGroupReturn && wholeGroupReturn->isVisible() && wholeGroupReturn->isEnabled()
            && nativeMaximizeButtonClickRoundTrip(primaryDock),
        "the whole COG editor group floats with native maximize, decorated controls, toolbar, and Dock return");
    if ( wholeGroupReturn )
    {
        wholeGroupReturn->click();
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    check(!primaryDock->isFloating()
            && canonical(cogDocumentState(window, QStringLiteral("first.cog"))) == beforeWholeGroupFloat,
        "the whole COG editor Dock return restores its native dock without changing raw source or history");
    const int undoSteps = document->availableUndoSteps();
    const int redoSteps = document->availableRedoSteps();
    const bool detached = detachCogPage(window, first);
    check(detached,
        "dragging an individual COG file tab outside its group creates a separate floating script host");
    if ( !detached )
    {
        return;
    }
    const QPointer<QTabWidget> floating(cogHostForPage(window, first));
    QPointer<QDockWidget> floatingDock(dockForHost(floating));
    QWidget* const tools = floatingDock ? floatingDock->findChild<QWidget*>(QStringLiteral("cogEditorGroupToolbar")) : nullptr;
    QToolButton* const create = tools ? tools->findChild<QToolButton*>(QStringLiteral("cogEditorNewButton")) : nullptr;
    QToolButton* const open = tools ? tools->findChild<QToolButton*>(QStringLiteral("cogEditorOpenButton")) : nullptr;
    QToolButton* const save = tools ? tools->findChild<QToolButton*>(QStringLiteral("cogEditorSaveButton")) : nullptr;
    QToolButton* const dockReturn = floatingDock
        ? floatingDock->findChild<QToolButton*>(QStringLiteral("cogEditorDockReturnButton")) : nullptr;
    const QRect frame = floatingDock ? floatingDock->frameGeometry() : QRect{};
    const QRect client = floatingDock ? floatingDock->geometry() : QRect{};
    check(floating && floating->property("sed.cogScriptEditorHost").toBool()
            && floatingDock && floatingDock->property("sed.cogScriptEditorDock").toBool()
            && !floatingDock->titleBarWidget() && !floatingDock->windowFlags().testFlag(Qt::FramelessWindowHint)
            && floatingDock->windowFlags().testFlag(Qt::WindowTitleHint)
            && floatingDock->windowFlags().testFlag(Qt::WindowSystemMenuHint)
            && floatingDock->windowFlags().testFlag(Qt::WindowCloseButtonHint)
            && floatingDock->windowFlags().testFlag(Qt::WindowMaximizeButtonHint)
            && nativeMaximizeButtonClickRoundTrip(floatingDock.data())
            && frame.top() < client.top() && !floatingDock->windowIcon().isNull()
            && tools && tools->isVisible() && create && open && save
            && create->isEnabled() && open->isEnabled() && save->isEnabled()
            && dockReturn && dockReturn->isVisible() && dockReturn->isEnabled(),
        "a detached COG file has native maximize, decorated controls, group toolbar and visible Dock return control");
    check(first && editor && document && first->findChild<QPlainTextEdit*>() == editor
            && editor->document() == document && editor->toPlainText() == current
            && document->availableUndoSteps() == undoSteps && document->availableRedoSteps() == redoSteps
            && canonical(cogDocumentState(window, QStringLiteral("first.cog"))) == before
            && first->property("cogPinned").toBool()
            && floating && floating->tabToolTip(floating->indexOf(first))
                == first->property("cogFullPath").toString() + QStringLiteral("\nPinned - unpin to close"),
        "COG file detachment retains the same live editor, exact raw text, source, pin, selection, clean cursor and Redo branch");
    editor->redo();
    check(editor->toPlainText() == future, "the transferred COG document still replays its exact supplementary-Unicode Redo command");
    editor->undo();
    editor->setTextCursor(selected);

    auto* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
    const bool nativeDocked = dragNativeCogCaption(floatingDock, primaryDock, workspace);
    cogNativeEdgeFailed = !nativeDocked;
    const auto failedEdgePhase = [&floatingDock](const char* const phase)
    {
        if ( cogNativeEdgeFailed )
        {
            std::cerr << "COG failed edge follow-up: phase=" << phase
                << " source=" << cogWidgetDescription(floatingDock.data()).toStdString()
                << " floating=" << (floatingDock && floatingDock->isFloating())
                << " grab=" << cogWidgetDescription(QWidget::mouseGrabber()).toStdString() << '\n';
        }
    };
    check(nativeDocked && cogHostForPage(window, first) == floating,
        "the actual native floating COG caption docks its file host without replacing the source editor");
    if ( floatingDock )
    {
        failedEdgePhase("before refloat");
        floatingDock->setFloating(true);
        failedEdgePhase("after refloat");
        if ( floatingDock )
        {
            floatingDock->show();
        }
        failedEdgePhase("before refloat events");
        QApplication::processEvents();
        failedEdgePhase("after refloat events");
        const QPointer<QToolButton> currentReturn(floatingDock
            ? floatingDock->findChild<QToolButton*>(QStringLiteral("cogEditorDockReturnButton")) : nullptr);
        if ( currentReturn )
        {
            failedEdgePhase("before Dock return click");
            currentReturn->click();
            failedEdgePhase("after Dock return click");
        }
        check(currentReturn && floatingDock && !floatingDock->isFloating()
                && cogHostForPage(window, first) == floating,
            "the COG Dock return button docks the same file host through its visible control");
    }

    failedEdgePhase("before second file detach");
    const bool secondDetached = detachCogPage(window, second);
    failedEdgePhase("after second file detach");
    const QPointer<QTabWidget> secondHost(cogHostForPage(window, second));
    const QPointer<QDockWidget> secondDock(dockForHost(secondHost));
    if ( secondDock )
    {
        secondDock->raise();
        secondDock->activateWindow();
        QApplication::processEvents();
    }
    const bool joined = secondDetached && floating && secondHost && secondHost != floating
        && dragCogTab(floating, first, secondHost->tabBar()->mapToGlobal(secondHost->tabBar()->tabRect(0).center()));
    failedEdgePhase("after cross-host file join");
    check(joined && secondHost && cogHostForPage(window, first) == secondHost && secondHost->count() == 2
            && canonical(cogDocumentState(window, QStringLiteral("first.cog"))) == before,
        "an individual COG file transfers into another COG window with its complete local history and source unchanged");
    primaryDock->show();
    primaryDock->raise();
    QApplication::processEvents();
    const bool returned = joined && secondHost && dragCogTab(secondHost, first,
        primary->tabBar()->mapToGlobal(primary->tabBar()->tabRect(0).center()));
    check(returned && cogHostForPage(window, first) == primary
            && canonical(cogDocumentState(window, QStringLiteral("first.cog"))) == before
            && readCogFile(root / "cog" / "first.cog") == kCogSource,
        "a detached COG file redocks into its primary tab bar without writing, extracting or losing its Redo branch");
    failedEdgePhase("before fixture owner destruction");
    if ( cogNativeEdgeFailed )
    {
        std::cerr << cogNativeLayoutDiagnostic("edge owner before destruction", workspace, floatingDock, primaryDock);
    }
}

void checkCogArchiveTabTooltipIdentifiesMountedSource()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::filesystem::path archive = root / "test.gob";
    const bool fixtureReady = temporary.isValid()
        && writeSingleEntryCogArchive(archive, "cog/Foo.COG",
            std::string_view(kCogSource.constData(), static_cast<std::size_t>(kCogSource.size())));
    MainWindow window(nullptr, true);
    if ( !fixtureReady )
    {
        check(false, "archive-backed COG tooltip fixture writes one synthetic mixed-case GOB entry");
        return;
    }
    bindCogProject(window, root);
    window.setResourcePathsForTest({ root }, { archive }, root / "docking.sed");
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();
    window.openCogScriptEditorForTest("Foo.COG");
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
    const int index = page && tabs ? tabs->indexOf(page) : -1;
    const QString expectedTooltip = QStringLiteral("test.gob: cog/Foo.COG");
    check(tabs && tabs->count() == 1 && page && index == 0
            && page->property("cogLooseFile").toBool() == false
            && page->property("cogFullPath").toString() != expectedTooltip
            && tabs->tabToolTip(index) == expectedTooltip,
        "an archive-backed COG tab tooltip identifies its exact GOB basename and case-preserved entry while retaining the extracted working-copy path separately");

    const bool detached = page && detachCogPage(window, page);
    QTabWidget* const detachedTabs = page ? cogHostForPage(window, page) : nullptr;
    const int detachedIndex = detachedTabs && page ? detachedTabs->indexOf(page) : -1;
    check(detached && detachedTabs && detachedIndex >= 0
            && detachedTabs->tabToolTip(detachedIndex) == expectedTooltip
            && page->property("cogArchiveTooltip").toString() == expectedTooltip,
        "native COG detach keeps the cached archive origin tooltip on the same live page");

    const QByteArray recovery = window.captureCogEditorRecoveryStateForTest();
    {
        MainWindow restoredWindow(nullptr, true);
        bindCogProject(restoredWindow, root);
        const bool recovered = restoredWindow.restoreCogEditorRecoveryStateForTest(recovery);
        QWidget* const recoveredPage = cogPage(restoredWindow, QStringLiteral("Foo.COG"));
        QTabWidget* const recoveredTabs = recoveredPage
            ? cogHostForPage(restoredWindow, recoveredPage) : nullptr;
        const int recoveredIndex = recoveredTabs && recoveredPage
            ? recoveredTabs->indexOf(recoveredPage) : -1;
        check(recovered && recoveredPage && recoveredTabs && recoveredIndex >= 0
                && recoveredPage->property("cogLooseFile").toBool() == false
                && recoveredTabs->tabToolTip(recoveredIndex) == expectedTooltip,
            "COG recovery retains the recorded archive tooltip and source kind after restoring with no archive mounted");
    }
    QApplication::processEvents();

    QDockWidget* const detachedDock = dockForHost(detachedTabs);
    QToolButton* const saveButton = detachedDock
        ? detachedDock->findChild<QToolButton*>(QStringLiteral("cogEditorSaveButton")) : nullptr;
    QPlainTextEdit* const editor = page ? page->findChild<QPlainTextEdit*>() : nullptr;
    const QString saveMarker = QStringLiteral("# archive tooltip saved\n");
    const QByteArray expectedSavedBytes = kCogSource + saveMarker.toUtf8();
    const int undoStepsBeforeEdit = editor ? editor->document()->availableUndoSteps() : -1;
    appendCogCommand(editor, saveMarker);
    QApplication::processEvents();
    const bool saveEnabledAfterEdit = saveButton && saveButton->isEnabled()
        && editor && editor->document()->isModified()
        && editor->document()->availableUndoSteps() > undoStepsBeforeEdit;
    if ( saveEnabledAfterEdit )
    {
        saveButton->click();
    }
    const int savedIndex = detachedTabs && page ? detachedTabs->indexOf(page) : -1;
    const QString workingPath = page ? page->property("cogFullPath").toString() : QString{};
    check(saveEnabledAfterEdit && detachedTabs && page && savedIndex >= 0
            && page->property("cogArchiveTooltip").toString().isEmpty()
            && page->property("cogLooseFile").toBool()
            && detachedTabs->tabToolTip(savedIndex) == workingPath
            && editor && editor->toPlainText().toUtf8() == expectedSavedBytes
            && readCogFile(root / "cog" / "Foo.COG") == expectedSavedBytes,
        "a successful COG Save clears the archive origin tooltip and switches to its extracted loose working path");
    const bool undoAvailableAfterSave = editor && editor->document()->isUndoAvailable();
    if ( undoAvailableAfterSave )
    {
        editor->undo();
    }
    const bool undoRetainedSource = editor && editor->toPlainText().toUtf8() == kCogSource;
    const bool redoAvailableAfterUndo = editor && editor->document()->isRedoAvailable();
    if ( redoAvailableAfterUndo )
    {
        editor->redo();
    }
    check(undoAvailableAfterSave && undoRetainedSource && redoAvailableAfterUndo
            && editor && editor->toPlainText().toUtf8() == expectedSavedBytes
            && readCogFile(root / "cog" / "Foo.COG") == expectedSavedBytes,
        "the archive-tooltip Save preserves the exact undo/redo edit branch and authored bytes");
}

void checkCogDraftSaveProtectsAnotherHostDestination()
{
    for ( const bool detachDraft : { false, true } )
    {
        QTemporaryDir temporary{};
        if ( !temporary.isValid() )
        {
            check(false, "COG destination ownership uses an isolated temporary project");
            return;
        }
        const std::filesystem::path root(temporary.path().toStdWString());
        const std::filesystem::path diskPath = root / "cog" / "late_conflict.cog";
        MainWindow window(nullptr, true);
        bindCogProject(window, root);
        window.setRecoveryDirectoryForTest(root / "recovery");
        window.createNewCogScriptForTest(QStringLiteral("late_conflict"));
        QTabWidget* const primary = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
        QPointer<QWidget> draft(primary ? primary->currentWidget() : nullptr);
        QPointer<QPlainTextEdit> draftEditor(draft ? draft->findChild<QPlainTextEdit*>() : nullptr);
        const QByteArray externalText("symbols\nend\ncode\nexternal_copy:\nreturn;\nend\n");
        const bool written = writeCogFile(diskPath, externalText);
        window.openCogScriptEditorForTest("late_conflict.cog");
        QPointer<QWidget> loose(primary ? primary->currentWidget() : nullptr);
        QPointer<QPlainTextEdit> looseEditor(loose ? loose->findChild<QPlainTextEdit*>() : nullptr);
        const bool opened = written && primary && primary->count() == 2
            && draft && loose && draft != loose && draftEditor && looseEditor;
        check(opened, "a late loose COG copy opens independently of the existing unsaved same-name draft");
        if ( !opened )
        {
            return;
        }
        appendCogCommand(draftEditor, QStringLiteral("# retained draft\n"));
        appendCogCommand(draftEditor, QStringLiteral("# draft redo\n"));
        draftEditor->undo();
        appendCogCommand(looseEditor, QStringLiteral("# retained loose edit\n"));
        appendCogCommand(looseEditor, QStringLiteral("# loose redo\n"));
        looseEditor->undo();
        const bool detached = detachCogPage(window, detachDraft ? draft.data() : loose.data());
        QTabWidget* const draftHost = cogHostForPage(window, draft.data());
        QTabWidget* const looseHost = cogHostForPage(window, loose.data());
        QDockWidget* const draftDock = dockForHost(draftHost);
        QToolButton* const save = draftDock
            ? draftDock->findChild<QToolButton*>(QStringLiteral("cogEditorSaveButton")) : nullptr;
        check(detached && draftHost && looseHost && draftHost != looseHost && save && save->isEnabled(),
            "the conflicting COG draft and loose buffer retain separate live hosts with draft Save enabled");
        if ( !detached || !draftHost || !looseHost || draftHost == looseHost || !save || !save->isEnabled() )
        {
            return;
        }
        const auto documents = [&window]()
        {
            QCborArray result{};
            for ( const QCborValue& value : cogWorkspace(window).value(QStringLiteral("tabs")).toArray() )
            {
                QCborMap page = value.toMap();
                for ( const QString& key : { QStringLiteral("splitter"), QStringLiteral("outlineSplitter"),
                          QStringLiteral("diagnosticsSplitter"), QStringLiteral("diagnosticsExpandedHeight"),
                          QStringLiteral("vscroll"), QStringLiteral("hscroll") } )
                {
                    page.remove(key);
                }
                result.append(page);
            }
            return canonical(result);
        };
        const QByteArray before = documents();
        int ownershipPrompts{};
        int overwritePrompts{};
        int unexpectedPrompts{};
        QTimer modal{};
        modal.setInterval(5);
        QObject::connect(&modal, &QTimer::timeout, [&]()
        {
            auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if ( !message )
            {
                return;
            }
            if ( message->windowTitle() == QStringLiteral("Save COG script")
                && message->text() == QStringLiteral(
                    "This script path is already open in another COG tab. Close or resolve that buffer before saving this draft.") )
            {
                ++ownershipPrompts;
                message->done(QMessageBox::Ok);
            }
            else if ( message->windowTitle() == QStringLiteral("Save COG script")
                && message->text().contains(QStringLiteral("Replace it?")) )
            {
                // Accept the obsolete overwrite path so Red proves that the
                // other live buffer's destination can actually be replaced.
                ++overwritePrompts;
                message->done(QMessageBox::Yes);
            }
            else
            {
                ++unexpectedPrompts;
                message->done(QMessageBox::Cancel);
            }
        });
        modal.start();
        save->click();
        modal.stop();
        QString warning{};
        const bool validWorkspace = window.validateRecoveryEditorStateForTest(
            window.captureRecoveryEditorStateForTest(), &warning);
        const bool preserved = ownershipPrompts == 1 && overwritePrompts == 0 && unexpectedPrompts == 0
            && readCogFile(diskPath) == externalText && documents() == before
            && draftEditor && looseEditor && draftEditor->document()->isModified()
            && looseEditor->document()->isModified() && draftEditor->document()->isRedoAvailable()
            && looseEditor->document()->isRedoAvailable()
            && cogHostForPage(window, draft.data()) == draftHost
            && cogHostForPage(window, loose.data()) == looseHost && validWorkspace && warning.isEmpty();
        if ( !preserved )
        {
            std::cerr << "COG destination ownership diagnostic: detachDraft=" << detachDraft
                << " ownershipPrompts=" << ownershipPrompts << " overwritePrompts=" << overwritePrompts
                << " unexpectedPrompts=" << unexpectedPrompts << " diskExact=" << (readCogFile(diskPath) == externalText)
                << " documentsExact=" << (documents() == before) << " validWorkspace=" << validWorkspace
                << " warning=" << warning.toStdString() << '\n';
        }
        check(preserved,
            "a COG draft Save refuses a destination owned by another host and preserves both raw histories sources and valid native workspace");
        if ( draftEditor )
        {
            draftEditor->document()->setModified(false);
        }
        if ( looseEditor )
        {
            looseEditor->document()->setModified(false);
        }
        window.discardCurrentRecoveryForTest();
    }
}

void checkCogPrimaryLastFileTransferHidesAndReopensEmptyHost()
{
    QTemporaryDir temporary{};
    if ( !temporary.isValid() )
    {
        check(false, "the COG primary-empty transfer fixture owns a valid temporary source directory");
        return;
    }
    const std::filesystem::path root(temporary.path().toStdWString());
    const bool filesReady = writeCogFile(root / "cog" / "first.cog", kCogSource)
        && writeCogFile(root / "cog" / "second.cog", kCogSource);
    MainWindow window(nullptr, true);
    editor::DocumentState documentState{};
    documentState.level.kind = ProjectType::JKDF2;
    documentState.level.header.version = 1;
    documentState.level.layers = { "Base" };
    window.setDocumentStateForTest(std::move(documentState));
    window.setProjectTypeForTest(ProjectType::JKDF2);
    window.setCurrentPathForTitleForTest(root / "docking.sed");
    window.setResourcePathsForTest({ root }, {}, root / "docking.sed");
    const QRect available = window.screen() ? window.screen()->availableGeometry() : QRect(0, 0, 1280, 900);
    window.resize(std::min(1280, available.width()), std::min(860, available.height()));
    window.move(available.topLeft());
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();
    if ( filesReady )
    {
        window.openCogScriptEditorForTest("first.cog");
        window.openCogScriptEditorForTest("second.cog");
    }
    QApplication::processEvents();

    QPointer<QTabWidget> primary(window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs")));
    QPointer<QDockWidget> primaryDock(dockForHost(primary));
    QPointer<QWidget> first(cogPage(window, QStringLiteral("first.cog")));
    QPointer<QWidget> second(cogPage(window, QStringLiteral("second.cog")));
    QPointer<QPlainTextEdit> secondEditor(second ? second->findChild<QPlainTextEdit*>() : nullptr);
    QPointer<QTextDocument> secondDocument(secondEditor ? secondEditor->document() : nullptr);
    check(filesReady && primary && primaryDock && first && second && secondEditor && secondDocument
            && primary->count() == 2,
        "the COG primary-empty fixture opens two real loose source files with the native workspace animations enabled");
    if ( !filesReady || !primary || !primaryDock || !first || !second || !secondEditor || !secondDocument
        || primary->count() != 2 )
    {
        return;
    }

    appendCogCommand(secondEditor, QStringLiteral("# primary transfer draft with raw \"\n"));
    const QString liveText = secondEditor->toPlainText();
    appendCogCommand(secondEditor, QStringLiteral("# primary transfer redo \U0001f642\n"));
    secondEditor->undo();
    QTextCursor selection(secondDocument);
    selection.setPosition(4);
    selection.setPosition(13, QTextCursor::KeepAnchor);
    secondEditor->setTextCursor(selection);
    check(window.triggerCogScriptTabContextActionForTest(primary->indexOf(second), QStringLiteral("Pin tab")),
        "the COG primary-empty fixture pins the last primary file through its actual tab action");
    primary->setCurrentWidget(second);
    const QByteArray firstBefore = canonical(cogDocumentState(window, QStringLiteral("first.cog")));
    const QByteArray secondBefore = canonical(cogDocumentState(window, QStringLiteral("second.cog")));
    const QString sourcePath = second->property("cogFullPath").toString();
    const int undoSteps = secondDocument->availableUndoSteps();
    const int redoSteps = secondDocument->availableRedoSteps();
    const bool detached = detachCogPage(window, first);
    QPointer<QTabWidget> auxiliary(detached ? cogHostForPage(window, first) : nullptr);
    QPointer<QDockWidget> auxiliaryDock(dockForHost(auxiliary));
    check(detached && primary->count() == 1 && auxiliary && auxiliary != primary
            && auxiliary->count() == 1 && auxiliaryDock && auxiliaryDock->isFloating(),
        "detaching the unrelated COG file leaves exactly one real file in the primary source host");
    if ( !detached || !auxiliary || !auxiliaryDock || !second || !primary )
    {
        return;
    }

    primaryDock->show();
    auxiliaryDock->show();
    auxiliaryDock->raise();
    auxiliaryDock->activateWindow();
    QApplication::processEvents();
    const QPoint targetPoint = auxiliary->tabBar()->mapToGlobal(
        auxiliary->tabBar()->tabRect(auxiliary->indexOf(first)).center());
    const bool transferred = dragCogTab(primary, second, targetPoint);
    const bool exactTransfer = transferred && primary && primaryDock && auxiliary && second && first
        && primary->count() == 0 && primaryDock->isHidden()
        && auxiliary->count() == 2 && auxiliary->currentWidget() == second
        && auxiliary->indexOf(second) == 0 && auxiliary->indexOf(first) == 1
        && cogHostForPage(window, second) == auxiliary && cogHostForPage(window, first) == auxiliary
        && second->findChild<QPlainTextEdit*>() == secondEditor && secondEditor->document() == secondDocument
        && secondEditor->toPlainText() == liveText && secondEditor->textCursor().position() == selection.position()
        && secondEditor->textCursor().anchor() == selection.anchor()
        && secondDocument->availableUndoSteps() == undoSteps && secondDocument->availableRedoSteps() == redoSteps
        && second->property("cogPinned").toBool() && second->property("cogFullPath").toString() == sourcePath
        && canonical(cogDocumentState(window, QStringLiteral("first.cog"))) == firstBefore
        && canonical(cogDocumentState(window, QStringLiteral("second.cog"))) == secondBefore
        && readCogFile(root / "cog" / "second.cog") == kCogSource;
    check(exactTransfer,
        "transferring the last primary COG file hides only the empty primary and preserves both live documents order source cursor pin and exact history");
    if ( !exactTransfer )
    {
        std::cerr << "COG primary-empty transfer diagnostic: transferred=" << transferred
            << " primaryCount=" << (primary ? primary->count() : -1)
            << " primaryHidden=" << (primaryDock && primaryDock->isHidden())
            << " auxiliaryCount=" << (auxiliary ? auxiliary->count() : -1)
            << " sameSecond=" << (second && second->findChild<QPlainTextEdit*>() == secondEditor
                && secondEditor && secondEditor->document() == secondDocument)
            << " history=" << (secondDocument && secondDocument->availableUndoSteps() == undoSteps
                && secondDocument->availableRedoSteps() == redoSteps) << '\n';
    }

    QAction* const toolsAction = window.findChild<QAction*>(QStringLiteral("tools.cog_script_editor"));
    if ( primaryDock && primary && toolsAction )
    {
        if ( !primaryDock->isHidden() )
        {
            // The RED implementation leaves the primary open. Exercise the
            // actual Tools toggle in both directions without masking the
            // transfer assertion above.
            toolsAction->trigger();
        }
        const bool emptyPrimaryHidden = primaryDock->isHidden() && primary->count() == 0;
        if ( emptyPrimaryHidden )
        {
            toolsAction->trigger();
        }
        QApplication::processEvents();
        check(emptyPrimaryHidden && primaryDock && primaryDock->isVisible() && primary->count() == 0,
            "Tools reopens the existing empty primary COG host without creating a replacement or document");
    }
    else
    {
        check(false, "the empty primary COG host retains its Tools toggle action after its last file transfers");
    }
}

void checkCogDockReturnRetiresNativeAnimatedEmptyHost()
{
    QTemporaryDir temporary{};
    if ( !temporary.isValid() )
    {
        check(false, "the native COG animation-retirement fixture owns a valid temporary source directory");
        return;
    }
    const std::filesystem::path root(temporary.path().toStdWString());
    QPointer<QWidget> lifetimePage{};
    QPointer<QPlainTextEdit> lifetimeEditor{};
    QPointer<QTextDocument> lifetimeDocument{};
    QPointer<QMainWindow> lifetimeWorkspace{};
    std::vector<QPointer<QPropertyAnimation>> nativeAnimations{};
    std::vector<QPointer<QPropertyAnimation>> ownerGeometryAnimations{};
    bool geometryCompletedWhileTargetsAlive{true};
    QObject completionObserver{};
    bool stoppedWhileEmptyAndAlive{};
    bool emptyHideObserved{};
    bool layoutRemovedAtEmptyHide{};
    bool stoppedAtEmptyHide{};
    bool stoppedBeforeDestroyed{};
    {
        MainWindow window(nullptr, true);
        if ( !openCogFixture(window, root) )
        {
            return;
        }
        const QPointer<QTabWidget> primary(window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs")));
        const QPointer<QDockWidget> primaryDock(dockForHost(primary));
        const QPointer<QMainWindow> workspace(window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow")));
        const QPointer<QWidget> page(cogPage(window, QStringLiteral("first.cog")));
        const QPointer<QPlainTextEdit> editor(page ? page->findChild<QPlainTextEdit*>() : nullptr);
        const QPointer<QTextDocument> document(editor ? editor->document() : nullptr);
        if ( !primary || !primaryDock || !workspace || !page || !editor || !document )
        {
            check(false, "the animation-retirement fixture resolves the actual COG source, native document and workspace");
            return;
        }
        appendCogCommand(editor, QStringLiteral("# retirement unclosed \"\n"));
        const QString current = editor->toPlainText();
        appendCogCommand(editor, QStringLiteral("# retirement redo \U0001f642\n"));
        const QString future = editor->toPlainText();
        editor->undo();
        QTextCursor selection(document);
        selection.setPosition(3);
        selection.setPosition(8, QTextCursor::KeepAnchor);
        editor->setTextCursor(selection);
        const bool detached = detachCogPage(window, page);
        const QPointer<QTabWidget> source(cogHostForPage(window, page));
        const QPointer<QDockWidget> sourceDock(dockForHost(source));
        check(detached && source && sourceDock && source != primary,
            "the animation-retirement fixture detaches one existing COG file through its actual file tab");
        if ( !detached || !source || !sourceDock )
        {
            return;
        }
        const QByteArray exact = canonical(cogDocumentState(window, QStringLiteral("first.cog")));
        const int undoSteps = document->availableUndoSteps();
        const int redoSteps = document->availableRedoSteps();
        sourceDock->setStyle(new CogNativeAnimationStyle(sourceDock.data()));
        workspace->setAnimated(true);
        // QMainWindowLayout::plug creates the target geometry animation. The
        // local duration hint keeps that real Qt animation observable until
        // retirement, rather than manufacturing an application animation.
        const bool dispatched = dragNativeCogCaption(sourceDock, primaryDock, workspace, false);
        const QList<QPropertyAnimation*> animations = sourceDock
            ? sourceDock->findChildren<QPropertyAnimation*>() : QList<QPropertyAnimation*>{};
        for ( QPropertyAnimation* const animation : animations )
        {
            if ( animation->targetObject() == sourceDock.data() && animation->propertyName() == QByteArrayLiteral("geometry")
                && animation->state() == QAbstractAnimation::Running )
            {
                nativeAnimations.emplace_back(animation);
                QObject::connect(animation, &QAbstractAnimation::stateChanged, &window,
                    [source, sourceDock, &stoppedWhileEmptyAndAlive](const QAbstractAnimation::State state,
                        const QAbstractAnimation::State)
                    {
                        if ( state == QAbstractAnimation::Stopped && source && sourceDock && source->count() == 0 )
                        {
                            stoppedWhileEmptyAndAlive = true;
                        }
                    });
            }
        }
        const bool activeNativeTarget = dispatched && !nativeAnimations.empty() && sourceDock
            && sourceDock->isFloating() && workspace->isAnimated();
        check(activeNativeTarget,
            "a real native COG caption drop leaves Qt's matching target geometry animation active before Dock return");
        if ( !activeNativeTarget )
        {
            std::cerr << "COG native animation witness: dispatched=" << dispatched
                << " matched=" << nativeAnimations.size() << " candidates=" << animations.size()
                << " sourceAlive=" << static_cast<bool>(sourceDock)
                << " floating=" << (sourceDock && sourceDock->isFloating())
                << " animated=" << (workspace && workspace->isAnimated()) << '\n';
            if ( sourceDock )
            {
                std::cerr << sourceDock->property("sed.cog_test.nativeLayoutTrace").toString().toStdString();
            }
            std::cerr << cogNativeLayoutDiagnostic("animation witness owner before destruction", workspace, sourceDock, primaryDock);
            return;
        }
        QObject::connect(sourceDock, &QDockWidget::visibilityChanged, &window,
            [source, sourceDock, workspace, &nativeAnimations, &emptyHideObserved, &layoutRemovedAtEmptyHide,
                &stoppedAtEmptyHide](const bool visible)
            {
                if ( !visible && source && sourceDock && source->count() == 0 )
                {
                    emptyHideObserved = true;
                    layoutRemovedAtEmptyHide = workspace
                        && workspace->dockWidgetArea(sourceDock) == Qt::NoDockWidgetArea;
                    stoppedAtEmptyHide = std::all_of(nativeAnimations.begin(), nativeAnimations.end(),
                        [](const QPointer<QPropertyAnimation>& animation)
                        {
                            return !animation || animation->state() == QAbstractAnimation::Stopped;
                        });
                }
            });
        QObject::connect(sourceDock, &QObject::destroyed, &window, [&nativeAnimations, &stoppedBeforeDestroyed]()
            {
                // This observes the alive animation objects, never the dock
                // whose QPointer has already cleared at QObject destruction.
                stoppedBeforeDestroyed = std::all_of(nativeAnimations.begin(), nativeAnimations.end(),
                    [](const QPointer<QPropertyAnimation>& animation)
                    {
                        return !animation || animation->state() == QAbstractAnimation::Stopped;
                    });
            });
        const QPointer<QToolButton> dockReturn(sourceDock->findChild<QToolButton*>(QStringLiteral("cogEditorDockReturnButton")));
        const bool returnVisible = dockReturn && dockReturn->isVisible();
        if ( dockReturn )
        {
            dockReturn->click();
        }
        const bool returned = returnVisible && sourceDock && source && !sourceDock->isFloating()
            && cogHostForPage(window, page) == source;
        const bool stillActive = std::any_of(nativeAnimations.begin(), nativeAnimations.end(),
            [](const QPointer<QPropertyAnimation>& animation)
            {
                return animation && animation->state() == QAbstractAnimation::Running;
            });
        check(returned && stillActive,
            "the real COG Dock-return control retains the same file host while its native geometry animation is still active");
        if ( !returned || !stillActive || !primary || !source )
        {
            std::cerr << "COG Dock-return animation witness: visible=" << returnVisible
                << " returned=" << returned << " active=" << stillActive
                << " sourceAlive=" << static_cast<bool>(sourceDock) << '\n';
            return;
        }
        const bool transferred = dragCogTab(source, page,
            primary->tabBar()->mapToGlobal(primary->tabBar()->tabRect(0).center()));
        const bool exactTransferred = transferred && page && editor && document && primary
            && cogHostForPage(window, page) == primary && primary->count() == 3
            && primary->widget(0) == cogPage(window, QStringLiteral("second.cog"))
            && primary->widget(1) == cogPage(window, QStringLiteral("third.cog"))
            && primary->widget(2) == page && primary->currentWidget() == page
            && page->findChild<QPlainTextEdit*>() == editor && editor->document() == document
            && editor->toPlainText() == current && document->availableUndoSteps() == undoSteps
            && document->availableRedoSteps() == redoSteps
            && canonical(cogDocumentState(window, QStringLiteral("first.cog"))) == exact
            && readCogFile(root / "cog" / "first.cog") == kCogSource;
        check(exactTransferred,
            "immediate file transfer after animated Dock return preserves the same COG document exact source selection dirty checkpoint and Redo branch");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents();
        const bool retiredSafely = emptyHideObserved && layoutRemovedAtEmptyHide && stoppedAtEmptyHide
            && stoppedWhileEmptyAndAlive && stoppedBeforeDestroyed && !source && !sourceDock
            && std::all_of(nativeAnimations.begin(), nativeAnimations.end(),
                [](const QPointer<QPropertyAnimation>& animation) { return !animation; });
        if ( !retiredSafely )
        {
            std::cerr << "COG animation retirement diagnostic: matched=" << nativeAnimations.size()
                << " transferred=" << transferred << " emptyHide=" << emptyHideObserved
                << " removedAtHide=" << layoutRemovedAtEmptyHide << " stoppedAtHide=" << stoppedAtEmptyHide
                << " stoppedAlive=" << stoppedWhileEmptyAndAlive << " stoppedBeforeDestroyed=" << stoppedBeforeDestroyed
                << " sourceAlive=" << static_cast<bool>(source) << " dockAlive=" << static_cast<bool>(sourceDock) << '\n';
        }
        check(retiredSafely,
            "empty COG host retirement removes the live dock from Qt layout and aborts its native target animation before deferred destruction");
        if ( exactTransferred && editor )
        {
            editor->redo();
            check(editor->toPlainText() == future,
                "the document surviving native animation retirement retains its exact supplementary-Unicode Redo command");
            editor->undo();
            editor->setTextCursor(selection);
            check(canonical(cogDocumentState(window, QStringLiteral("first.cog"))) == exact,
                "Undo after native animation retirement restores the complete original COG document checkpoint");
        }
        lifetimePage = page;
        lifetimeEditor = editor;
        lifetimeDocument = document;
        lifetimeWorkspace = workspace;
        // Observe actual outstanding Qt actors, including native tab bars
        // and the central splitter. They must complete while their targets
        // and workspace still live, rather than during QObject base teardown.
        for ( QPropertyAnimation* const animation : window.findChildren<QPropertyAnimation*>() )
        {
            const QPointer<QWidget> target(qobject_cast<QWidget*>(animation->targetObject()));
            if ( target && animation->parent() == target.data()
                && animation->propertyName() == QByteArrayLiteral("geometry") )
            {
                ownerGeometryAnimations.emplace_back(animation);
                QObject::connect(animation, &QObject::destroyed, &completionObserver,
                    [target, workspace, &geometryCompletedWhileTargetsAlive]()
                    {
                        geometryCompletedWhileTargetsAlive = geometryCompletedWhileTargetsAlive && target && workspace;
                    });
            }
        }
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
    check(!lifetimePage && !lifetimeEditor && !lifetimeDocument && !lifetimeWorkspace,
        "owner teardown after native COG animation retirement destroys its remaining document and workspace without late target callbacks");
    check(geometryCompletedWhileTargetsAlive
            && std::all_of(ownerGeometryAnimations.begin(), ownerGeometryAnimations.end(),
                [](const QPointer<QPropertyAnimation>& animation) { return !animation; }),
        "owner teardown completes every observed QWidget-owned geometry animation while its target and native workspace remain alive");
}

bool waitForCogWindowMerge(const std::function<bool()>& predicate)
{
    if ( predicate() )
    {
        return true;
    }
    QEventLoop loop{};
    QTimer poll{};
    QTimer timeout{};
    timeout.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]()
        {
            if ( predicate() )
            {
                loop.quit();
            }
        });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start(5);
    timeout.start(2000);
    loop.exec();
    return predicate();
}

bool completeCogNativeActionCallbacks(QWidget* const owner)
{
    const QPointer<QWidget> guardedOwner(owner);
    if ( !guardedOwner )
    {
        return false;
    }
    const auto nativeActors = [guardedOwner]()
    {
        std::vector<QPointer<QPropertyAnimation>> actors{};
        if ( guardedOwner )
        {
            for ( QPropertyAnimation* const animation : guardedOwner->findChildren<QPropertyAnimation*>() )
            {
                auto* const target = qobject_cast<QWidget*>(animation->targetObject());
                if ( target && animation->parent() == target
                    && animation->propertyName() == QByteArrayLiteral("geometry") )
                {
                    actors.emplace_back(animation);
                }
            }
        }
        return actors;
    };
    const qsizetype maximumPasses = guardedOwner->findChildren<QWidget*>().size() + 2;
    for ( qsizetype pass = 0; pass < maximumPasses && guardedOwner; ++pass )
    {
        const bool stopped = waitForCogWindowMerge([guardedOwner, nativeActors]()
            {
                const auto actors = nativeActors();
                return !guardedOwner || std::all_of(actors.begin(), actors.end(),
                    [](const QPointer<QPropertyAnimation>& animation)
                    {
                        return !animation || animation->state() == QAbstractAnimation::Stopped;
                    });
            });
        if ( !stopped || !guardedOwner )
        {
            return false;
        }
        const auto actors = nativeActors();
        if ( actors.empty() )
        {
            return true;
        }
        // Deletes posted before a nested event loop can remain depth-gated.
        // Back at the caller's depth, dispatch only these actual stopped Qt
        // actors. Their completion may create another generation to inspect.
        for ( const QPointer<QPropertyAnimation>& animation : actors )
        {
            if ( animation && animation->state() != QAbstractAnimation::Stopped )
            {
                break;
            }
            if ( animation )
            {
                QCoreApplication::sendPostedEvents(animation.data(), QEvent::DeferredDelete);
            }
            if ( !guardedOwner || animation )
            {
                return false;
            }
        }
    }
    return false;
}

void placeCogMergeSource(QDockWidget* const dock, MainWindow& window)
{
    if ( !dock )
    {
        return;
    }
    QScreen* const screen = window.screen();
    if ( !screen )
    {
        return;
    }
    const QRect available = screen->availableGeometry().adjusted(8, 8, -8, -8);
    const QSize minimum = dock->minimumSize().expandedTo(dock->minimumSizeHint());
    dock->setFloating(true);
    dock->resize(minimum.expandedTo(QSize(720, 720)));
    dock->move(available.right() - dock->width(), available.top() + 32);
    dock->show();
    dock->raise();
    QApplication::processEvents();
}

bool dispatchCogNativeCenterDrop(QDockWidget* const source, QDockWidget* const target)
{
    if ( !source || !target || !source->isFloating() || source->titleBarWidget() )
    {
        return false;
    }
    const QPointer<QDockWidget> guarded(source);
    const QPointer<QDockWidget> guardedTarget(target);
    auto* const workspace = qobject_cast<QMainWindow*>(target->parentWidget());
    const QSize required = prospectiveCogDockedMinimum(source).expandedTo(prospectiveCogDockedMinimum(target));
    const bool feasible = allocateCogNativeDropTarget(target, workspace, required) && guarded && guardedTarget;
    check(feasible, "the actual native COG tabification target provides the maximum minimum size of both full document panels");
    if ( !feasible )
    {
        return false;
    }
    const QRect frame = source->frameGeometry();
    const QRect client = source->geometry();
    const QRect caption(client.left(), frame.top(), client.width(), client.top() - frame.top());
    const QPoint startGlobal = caption.center();
    const QPoint start = source->mapFromGlobal(startGlobal);
    QPoint destination = target->mapToGlobal(target->rect().center());
    const QRect available = source->screen() ? source->screen()->availableGeometry() : QRect{};
    const bool usable = caption.height() > 0 && available.contains(startGlobal)
        && available.contains(destination) && !source->frameGeometry().contains(destination);
    check(usable, "the COG window merge gesture starts on a real native caption and ends at a visible target center outside the source window");
    if ( !usable )
    {
        reportCogNativeCaptionFailure("center precondition", source, target, startGlobal, destination);
        return false;
    }
    const QPoint previousCursor = QCursor::pos();
    QCursor::setPos(startGlobal);
    QMouseEvent press(QEvent::NonClientAreaMouseButtonPress, QPointF(start), QPointF(startGlobal),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(source, &press);
    if ( guardedTarget )
    {
        destination = guardedTarget->mapToGlobal(guardedTarget->rect().center());
    }
    QCursor::setPos(destination);
    bool exactNativeMove{};
    std::string nativeMoveDiagnostic{};
    if ( guarded )
    {
        exactNativeMove = moveNativeCogCaptionTo(guarded, start, destination, &nativeMoveDiagnostic);
        if ( guarded )
        {
            QMouseEvent release(QEvent::NonClientAreaMouseMove,
                QPointF(guarded->mapFromGlobal(destination)), QPointF(destination),
                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(guarded, &release);
        }
    }
    QApplication::processEvents();
    QCursor::setPos(previousCursor);
    if ( guarded && (guarded->isFloating() || !exactNativeMove) )
    {
        reportCogNativeCaptionFailure("center after native release", guarded, guardedTarget, startGlobal, destination);
        std::cerr << nativeMoveDiagnostic;
    }
    return exactNativeMove && guarded && guardedTarget;
}

void checkCogWholeWindowMergePreservesDocumentsAndMixedActiveTarget()
{
    if ( cogNativeEdgeFailed )
    {
        std::cerr << "COG failed edge follow-up: phase=entered next native window fixture\n";
    }
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    QObject nativeGeometryObserver{};
    MainWindow window(nullptr, true);
    if ( !temporary.isValid() || !openCogFixture(window, root) )
    {
        return;
    }
    QPointer<QTabWidget> primary(window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs")));
    QPointer<QDockWidget> primaryDock(dockForHost(primary));
    QPointer<QWidget> first(cogPage(window, QStringLiteral("first.cog")));
    QPointer<QWidget> second(cogPage(window, QStringLiteral("second.cog")));
    QPointer<QWidget> third(cogPage(window, QStringLiteral("third.cog")));
    QPointer<QPlainTextEdit> firstEditor(first ? first->findChild<QPlainTextEdit*>() : nullptr);
    QPointer<QPlainTextEdit> secondEditor(second ? second->findChild<QPlainTextEdit*>() : nullptr);
    QPointer<QTextDocument> firstDocument(firstEditor ? firstEditor->document() : nullptr);
    QPointer<QTextDocument> secondDocument(secondEditor ? secondEditor->document() : nullptr);
    auto* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
    if ( !primary || !primaryDock || !first || !second || !third || !firstEditor || !secondEditor || !workspace )
    {
        check(false, "the whole-window COG merge fixture resolves its actual native file editors and host");
        return;
    }
    appendCogCommand(firstEditor, QStringLiteral("# merged raw unclosed \"\n"));
    const QString firstCurrent = firstEditor->toPlainText();
    appendCogCommand(firstEditor, QStringLiteral("# retained redo \U0001f642\n"));
    const QString firstFuture = firstEditor->toPlainText();
    firstEditor->undo();
    QTextCursor selected(firstDocument);
    selected.setPosition(3);
    selected.setPosition(8, QTextCursor::KeepAnchor);
    firstEditor->setTextCursor(selected);
    appendCogCommand(secondEditor, QStringLiteral("# second raw file\n"));
    const QString secondCurrent = secondEditor->toPlainText();

    const bool detached = detachCogPage(window, first);
    QPointer<QTabWidget> source(cogHostForPage(window, first));
    QPointer<QDockWidget> sourceDock(dockForHost(source));
    if ( !detached || !source || !sourceDock )
    {
        check(false, "the whole-window merge fixture creates a real native detached COG source window");
        return;
    }
    placeCogMergeSource(sourceDock, window);
    const bool transferred = dragCogTab(primary, second,
        source->tabBar()->mapToGlobal(source->tabBar()->tabRect(0).center()));
    const bool prepared = transferred && cogHostForPage(window, second) == source && source->count() == 2
        && source->widget(0) == first && source->widget(1) == second
        && primary->count() == 1 && primary->widget(0) == third;
    check(prepared, "the COG native window merge starts with two existing source files and one independent destination file");
    if ( !prepared )
    {
        return;
    }
    source->setCurrentWidget(first);
    const std::array<QByteArray, 3> exactDocuments{canonical(cogDocumentState(window, QStringLiteral("first.cog"))),
        canonical(cogDocumentState(window, QStringLiteral("second.cog"))),
        canonical(cogDocumentState(window, QStringLiteral("third.cog")))};
    const int firstUndoSteps = firstDocument->availableUndoSteps();
    const int firstRedoSteps = firstDocument->availableRedoSteps();
    const int secondUndoSteps = secondDocument->availableUndoSteps();

    QPointer<QDockWidget> otherKind(window.findChild<QDockWidget*>(QStringLiteral("uniResourceEditorsDock")));
    QToolButton* const newUni = otherKind
        ? otherKind->findChild<QToolButton*>(QStringLiteral("resource_authoring.new")) : nullptr;
    if ( newUni )
    {
        newUni->click();
    }
    check(newUni && otherKind, "the native COG merge control opens a real different-kind resource group");
    if ( !otherKind )
    {
        return;
    }
    // A real New click and a later native group arrangement are distinct
    // event turns. Let Qt finish the New action's actual geometry callbacks
    // before programmatic fixture setup can retire one of its native bars.
    nativeGeometryObserver.setProperty("cog_test.newActionTurnDelivered", false);
    const QPointer<QObject> guardedNativeObserver(&nativeGeometryObserver);
    QTimer::singleShot(0, &nativeGeometryObserver, [guardedNativeObserver]()
        {
            if ( guardedNativeObserver )
            {
                guardedNativeObserver->setProperty("cog_test.newActionTurnDelivered", true);
            }
        });
    const bool newActionTurnDelivered = waitForCogWindowMerge([&]()
        {
            return nativeGeometryObserver.property("cog_test.newActionTurnDelivered").toBool();
        });
    const bool newActionSettled = newActionTurnDelivered && completeCogNativeActionCallbacks(&window);
    check(newActionSettled, "the New UNI action completes its native geometry callbacks before the independent mixed-group fixture arrangement");
    if ( !newActionSettled )
    {
        return;
    }
    workspace->tabifyDockWidget(primaryDock, otherKind);
    otherKind->show();
    otherKind->raise();
    QApplication::processEvents();
    const bool mixedTarget = workspace->tabifiedDockWidgets(primaryDock).contains(otherKind)
        && !otherKind->visibleRegion().isEmpty() && primaryDock->visibleRegion().isEmpty();
    check(mixedTarget, "the mixed-kind target exposes UNI while a COG peer remains hidden in the same native dock group");
    placeCogMergeSource(sourceDock, window);
    const bool mixedDispatched = mixedTarget && dispatchCogNativeCenterDrop(sourceDock, otherKind);
    const bool mixedGrouped = mixedDispatched && waitForCogWindowMerge([&]()
        {
            return sourceDock && !sourceDock->isFloating()
                && workspace->tabifiedDockWidgets(otherKind).contains(sourceDock);
        });
    check(mixedGrouped && source && source->count() == 2 && source->widget(0) == first
            && source->widget(1) == second && primary && primary->count() == 1
            && canonical(cogDocumentState(window, QStringLiteral("first.cog"))) == exactDocuments[0]
            && canonical(cogDocumentState(window, QStringLiteral("second.cog"))) == exactDocuments[1],
        "a native drop onto the visible different-kind group retains a separate COG dock and never merges into a hidden same-kind peer");
    if ( !mixedGrouped || !sourceDock )
    {
        return;
    }
    // Programmatic separation does not merge documents. The following native
    // caption gesture is the only trigger for same-kind file normalization.
    placeCogMergeSource(sourceDock, window);
    primaryDock->raise();
    QApplication::processEvents();
    const bool dispatched = dispatchCogNativeCenterDrop(sourceDock, primaryDock);
    const bool merged = dispatched && waitForCogWindowMerge([&]()
        {
            return first && second && primary && cogHostForPage(window, first) == primary
                && cogHostForPage(window, second) == primary;
        });
    if ( !merged )
    {
        std::cerr << "COG whole-window merge diagnostic: dispatched=" << dispatched
            << " sourceAlive=" << static_cast<bool>(source)
            << " sourceFloating=" << (sourceDock && sourceDock->isFloating())
            << " sourceCount=" << (source ? source->count() : -1)
            << " targetCount=" << (primary ? primary->count() : -1)
            << " targetPeers=" << workspace->tabifiedDockWidgets(primaryDock).size() << '\n';
    }
    check(merged && primary && first && second && firstEditor && secondEditor && firstDocument && secondDocument
            && primary->count() == 3 && primary->widget(0) == third
            && primary->widget(1) == first && primary->widget(2) == second && primary->currentWidget() == first
            && first->findChild<QPlainTextEdit*>() == firstEditor && firstEditor->document() == firstDocument
            && second->findChild<QPlainTextEdit*>() == secondEditor && secondEditor->document() == secondDocument
            && firstDocument->availableUndoSteps() == firstUndoSteps
            && firstDocument->availableRedoSteps() == firstRedoSteps
            && secondDocument->availableUndoSteps() == secondUndoSteps
            && canonical(cogDocumentState(window, QStringLiteral("first.cog"))) == exactDocuments[0]
            && canonical(cogDocumentState(window, QStringLiteral("second.cog"))) == exactDocuments[1]
            && canonical(cogDocumentState(window, QStringLiteral("third.cog"))) == exactDocuments[2]
            && readCogFile(root / "cog" / "first.cog") == kCogSource
            && readCogFile(root / "cog" / "second.cog") == kCogSource
            && readCogFile(root / "cog" / "third.cog") == kCogSource,
        "a native same-kind window drop joins all source files in order into the stable primary host with identical text documents selection sources histories and current file");
    if ( !merged || !primary || !primaryDock || !firstEditor || !secondEditor || !firstDocument || !secondDocument )
    {
        return;
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
    check(!source && !sourceDock && cogWorkspace(window).value(QStringLiteral("hosts")).toArray().size() == 1,
        "a completed COG window merge retires the empty detached native host without deleting any transferred editor");
    firstEditor->redo();
    check(firstEditor->toPlainText() == firstFuture, "the window-merged COG still replays its exact supplementary-Unicode Redo branch");
    firstEditor->undo();
    firstEditor->setTextCursor(selected);
    QAction* const saveAll = primaryDock->findChild<QAction*>(QStringLiteral("cog_editor.save_all"));
    if ( saveAll )
    {
        saveAll->trigger();
    }
    check(saveAll && readCogFile(root / "cog" / "first.cog") == firstCurrent.toUtf8()
            && readCogFile(root / "cog" / "second.cog") == secondCurrent.toUtf8()
            && readCogFile(root / "cog" / "third.cog") == kCogSource
            && !firstDocument->isModified() && !secondDocument->isModified()
            && primary->currentWidget() == first,
        "Save All after a native COG window merge writes the exact owned loose files and preserves the dropped current file");
}

void checkCogWrongKindForeignAndCancel()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    MainWindow window(nullptr, true);
    if ( !temporary.isValid() || !openCogFixture(window, root) )
    {
        return;
    }
    QWidget* const page = cogPage(window, QStringLiteral("first.cog"));
    QTabWidget* const source = cogHostForPage(window, page);
    QPlainTextEdit* const editor = page ? page->findChild<QPlainTextEdit*>() : nullptr;
    appendCogCommand(editor, QStringLiteral("# pending \""));
    source->setCurrentWidget(page);
    const QByteArray before = canonical(cogWorkspace(window));
    const auto outside = outsideTabPoint(window, source->tabBar());
    check(outside && dragCogTab(source, page, *outside, true, editor)
            && canonical(cogWorkspace(window)) == before && cogHostForPage(window, page) == source,
        "Escape from the real COG source field cancels file-tab dragging without changing tab order or raw history");

    auto* const resourceDock = window.findChild<QDockWidget*>(QStringLiteral("uniResourceEditorsDock"));
    auto* const resourceNew = resourceDock
        ? resourceDock->findChild<QToolButton*>(QStringLiteral("resource_authoring.new")) : nullptr;
    if ( resourceDock && resourceNew )
    {
        resourceDock->setFloating(true);
        resourceDock->show();
        resourceNew->click();
        resourceDock->raise();
        resourceDock->activateWindow();
        QApplication::processEvents();
    }
    QTabWidget* const resourceTabs = window.findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"));
    const QByteArray beforeWrongKind = canonical(cogWorkspace(window));
    const bool wrongKindDispatched = resourceTabs && resourceTabs->count() > 0
        && dragCogTab(source, page, resourceTabs->tabBar()->mapToGlobal(resourceTabs->tabBar()->tabRect(0).center()));
    check(wrongKindDispatched && cogHostForPage(window, page) == source
            && canonical(cogWorkspace(window)) == beforeWrongKind && resourceTabs->indexOf(page) < 0,
        "a COG file-tab drop on an actual UNI tab bar is rejected without changing either document kind");

    focusCogPage(window, page);
    const QByteArray selectedSource = canonical(cogWorkspace(window));
    QAction* const sourceUndo = window.findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* const sourceReload = window.findChild<QAction*>(QStringLiteral("cog_editor.reload"));
    check(cogWorkspace(window).value(QStringLiteral("active")).toInteger() >= 0
            && sourceUndo && sourceUndo->isEnabled() && sourceReload && sourceReload->isEnabled(),
        "the foreign-owner fixture starts with one real selected COG file and truthful local Undo and Reload actions");
    MainWindow foreign(nullptr, true);
    bindCogProject(foreign, root);
    foreign.openCogScriptEditorForTest("third.cog");
    focusCogPage(foreign, cogPage(foreign, QStringLiteral("third.cog")));
    QTabWidget* const foreignTabs = foreign.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    const QByteArray sourceWithForeignFocus = canonical(cogWorkspace(window));
    if ( sourceWithForeignFocus != selectedSource )
    {
        std::cerr << "COG foreign focus capture diagnostic: sourceUndo=" << (sourceUndo && sourceUndo->isEnabled())
            << " sourceReload=" << (sourceReload && sourceReload->isEnabled()) << '\n';
        reportCogStateDifferences(QCborValue::fromCbor(selectedSource).toMap(),
            QCborValue::fromCbor(sourceWithForeignFocus).toMap());
    }
    check(sourceWithForeignFocus == selectedSource && sourceUndo && !sourceUndo->isEnabled()
            && sourceReload && !sourceReload->isEnabled(),
        "full COG capture retains this owner's exact selected file and document history while its actions reject a foreign owner's focus");
    const QByteArray beforeForeign = canonical(cogWorkspace(foreign));
    const QByteArray beforeSource = canonical(cogWorkspace(window));
    const bool foreignDispatched = foreignTabs && foreignTabs->count() == 1
        && dragCogTab(source, page, foreignTabs->tabBar()->mapToGlobal(foreignTabs->tabBar()->tabRect(0).center()));
    const bool rejectedForeign = foreignDispatched && cogHostForPage(window, page) == source
            && canonical(cogWorkspace(window)) == beforeSource
            && canonical(cogWorkspace(foreign)) == beforeForeign && !cogHostForPage(foreign, page);
    if ( !rejectedForeign )
    {
        std::cerr << "COG foreign drop diagnostic: dispatched=" << foreignDispatched
            << " sourceRetained=" << (cogHostForPage(window, page) == source)
            << " sourceExact=" << (canonical(cogWorkspace(window)) == beforeSource)
            << " foreignExact=" << (canonical(cogWorkspace(foreign)) == beforeForeign)
            << " foreignOwnsPage=" << static_cast<bool>(cogHostForPage(foreign, page))
            << " dispatch=" << source->tabBar()->property("sed.cog_test.dragDispatch").toString().toStdString()
            << " trace=" << source->tabBar()->property("resource_authoring.dragTrace").toString().toStdString() << '\n';
        reportCogStateDifferences(QCborValue::fromCbor(beforeSource).toMap(), cogWorkspace(window));
        reportCogStateDifferences(QCborValue::fromCbor(beforeForeign).toMap(), cogWorkspace(foreign));
    }
    check(rejectedForeign,
        "a COG file cannot transfer to a foreign MainWindow and retain callbacks bound to its former project");
}

void checkDetachedCogActionRoutingAndSaveAll()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    MainWindow window(nullptr, true);
    if ( !temporary.isValid() || !openCogFixture(window, root) )
    {
        return;
    }
    QPointer<QWidget> first(cogPage(window, QStringLiteral("first.cog")));
    QPointer<QPlainTextEdit> firstEditor(first ? first->findChild<QPlainTextEdit*>() : nullptr);
    appendCogCommand(firstEditor, QStringLiteral("# first save\n"));
    const QString firstSaved = firstEditor ? firstEditor->toPlainText() : QString{};
    const bool detached = detachCogPage(window, first);
    check(detached, "detached COG action fixture separates one real modified file from the primary group");
    if ( !detached || !firstEditor )
    {
        return;
    }
    int saveCallbacks{};
    window.setCogSaveCallbackForTest([&]() { ++saveCallbacks; });
    focusCogPage(window, first);
    QAction* const save = window.findChild<QAction*>(QStringLiteral("file.save"));
    if ( save )
    {
        save->trigger();
    }
    const bool focusedSaved = save && saveCallbacks == 1 && readCogFile(root / "cog" / "first.cog") == firstSaved.toUtf8()
            && !firstEditor->document()->isModified() && !std::filesystem::exists(root / "docking.sed")
            && readCogFile(root / "cog" / "second.cog") == kCogSource;
    if ( !focusedSaved )
    {
        std::cerr << "Detached COG Save diagnostic: action=" << static_cast<bool>(save)
            << " callbacks=" << saveCallbacks
            << " bytesExact=" << (readCogFile(root / "cog" / "first.cog") == firstSaved.toUtf8())
            << " clean=" << !firstEditor->document()->isModified()
            << " nativeExists=" << std::filesystem::exists(root / "docking.sed")
            << " siblingExact=" << (readCogFile(root / "cog" / "second.cog") == kCogSource) << '\n';
    }
    check(focusedSaved,
        "focused Save from a detached COG editor writes only its exact source and never routes to native Level Save");
    QAction* const undo = window.findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* const redo = window.findChild<QAction*>(QStringLiteral("edit.redo"));
    if ( undo )
    {
        undo->trigger();
    }
    check(undo && firstEditor->toPlainText().toUtf8() == kCogSource
            && firstEditor->document()->isModified() && firstEditor->document()->isRedoAvailable(),
        "the global Edit Undo action targets a detached COG document and retains its saved clean marker");
    if ( redo )
    {
        redo->trigger();
    }
    check(redo && firstEditor->toPlainText() == firstSaved && !firstEditor->document()->isModified(),
        "the global Edit Redo action returns the detached script to its exact saved checkpoint");

    const std::array<QString, 3> names{ QStringLiteral("first.cog"), QStringLiteral("second.cog"), QStringLiteral("third.cog") };
    std::array<QString, 3> expected{};
    for ( std::size_t index = 0; index < names.size(); ++index )
    {
        QWidget* const page = cogPage(window, names[index]);
        QPlainTextEdit* const editor = page ? page->findChild<QPlainTextEdit*>() : nullptr;
        appendCogCommand(editor, QStringLiteral("# Save All %1\n").arg(index + 1));
        expected[index] = editor ? editor->toPlainText() : QString{};
    }
    focusCogPage(window, first);
    QTabWidget* const selectedHost = cogHostForPage(window, first);
    QDockWidget* const selectedDock = dockForHost(selectedHost);
    QAction* const saveAll = selectedDock ? selectedDock->findChild<QAction*>(QStringLiteral("cog_editor.save_all")) : nullptr;
    saveCallbacks = 0;
    if ( saveAll )
    {
        saveAll->trigger();
    }
    bool allExact = saveAll && saveCallbacks == 3 && selectedHost->currentWidget() == first;
    for ( std::size_t index = 0; index < names.size(); ++index )
    {
        QWidget* const page = cogPage(window, names[index]);
        QPlainTextEdit* const editor = page ? page->findChild<QPlainTextEdit*>() : nullptr;
        allExact = allExact && editor && !editor->document()->isModified()
            && readCogFile(root / "cog" / names[index].toStdWString()) == expected[index].toUtf8();
    }
    const bool savedAll = allExact && !window.hasUnsavedCogScriptChangesForTest()
        && !std::filesystem::exists(root / "docking.sed");
    if ( !savedAll )
    {
        std::cerr << "Detached COG Save All diagnostic: action=" << static_cast<bool>(saveAll)
            << " callbacks=" << saveCallbacks << " activeExact=" << (selectedHost->currentWidget() == first)
            << " noDirty=" << !window.hasUnsavedCogScriptChangesForTest()
            << " nativeExists=" << std::filesystem::exists(root / "docking.sed") << '\n';
        for ( std::size_t index = 0; index < names.size(); ++index )
        {
            QWidget* const page = cogPage(window, names[index]);
            QPlainTextEdit* const editor = page ? page->findChild<QPlainTextEdit*>() : nullptr;
            std::cerr << "Detached COG Save All file: name=" << names[index].toStdString()
                << " bytesExact=" << (readCogFile(root / "cog" / names[index].toStdWString()) == expected[index].toUtf8())
                << " clean=" << (editor && !editor->document()->isModified()) << '\n';
        }
    }
    check(savedAll,
        "Save All COG includes primary and detached script hosts exactly once and preserves the active file selection");
}

void checkDetachedCogCloseAndOwnerLifetime()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    QPointer<QWidget> lifetimePage{};
    QPointer<QPlainTextEdit> lifetimeEditor{};
    QPointer<QTextDocument> lifetimeDocument{};
    {
        MainWindow window(nullptr, true);
        if ( !temporary.isValid() || !openCogFixture(window, root) )
        {
            return;
        }
        QPointer<QWidget> first(cogPage(window, QStringLiteral("first.cog")));
        QPlainTextEdit* const editor = first ? first->findChild<QPlainTextEdit*>() : nullptr;
        appendCogCommand(editor, QStringLiteral("# close retains raw \""));
        const bool detached = detachCogPage(window, first);
        check(detached, "COG close fixture owns one individually detached modified script window");
        if ( !detached )
        {
            return;
        }
        QPointer<QDockWidget> dock(dockForHost(cogHostForPage(window, first)));
        const QByteArray before = canonical(cogDocumentState(window, QStringLiteral("first.cog")));
        int prompts{};
        bool discard{};
        QTimer modal{};
        QObject::connect(&modal, &QTimer::timeout, [&]()
            {
                if ( auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
                {
                    ++prompts;
                    if ( QAbstractButton* const answer = message->button(discard ? QMessageBox::Discard : QMessageBox::Cancel) )
                    {
                        answer->click();
                    }
                    else
                    {
                        message->reject();
                    }
                }
            });
        modal.start(5);
        const bool canceledClose = dock && dock->close();
        modal.stop();
        check(!canceledClose && prompts == 1 && dock && dock->isVisible() && first
                && canonical(cogDocumentState(window, QStringLiteral("first.cog"))) == before,
            "Cancel on a detached COG window close preserves its exact source draft history and visible owner");
        discard = true;
        prompts = 0;
        modal.start(5);
        const bool discardedClose = dock && dock->close();
        modal.stop();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents();
        check(discardedClose && prompts == 1 && !first
                && !cogPage(window, QStringLiteral("first.cog"))
                && readCogFile(root / "cog" / "first.cog") == kCogSource,
            "Discard closes the detached COG file once and releases its editor without replacing the loose source");
        QWidget* const second = cogPage(window, QStringLiteral("second.cog"));
        const bool remainingDetached = detachCogPage(window, second);
        check(remainingDetached, "COG shutdown fixture leaves one actual script in a detached host");
        lifetimePage = second;
        lifetimeEditor = second ? second->findChild<QPlainTextEdit*>() : nullptr;
        lifetimeDocument = lifetimeEditor ? lifetimeEditor->document() : nullptr;
    }
    check(!lifetimePage && !lifetimeEditor && !lifetimeDocument,
        "MainWindow teardown destroys all detached COG pages and native documents while their source registry is still alive");
}

QCborMap cogHostRecord(const QString& id, const bool primary, const QByteArray& geometry,
    const QCborArray& tabs, const int active)
{
    QCborMap host{};
    host.insert(QStringLiteral("id"), id);
    host.insert(QStringLiteral("primary"), primary);
    host.insert(QStringLiteral("visible"), true);
    host.insert(QStringLiteral("floating"), !primary);
    host.insert(QStringLiteral("geometry"), geometry);
    host.insert(QStringLiteral("active"), active);
    host.insert(QStringLiteral("tabs"), tabs);
    return host;
}

QCborMap cogHostFixture(MainWindow& window)
{
    QCborMap state = cogWorkspace(window);
    QDockWidget* const dock = window.findChild<QDockWidget*>(QStringLiteral("cogScriptEditorDock"));
    const QByteArray geometry = dock ? dock->saveGeometry() : QByteArray{};
    QCborArray hosts{};
    hosts.append(cogHostRecord(QStringLiteral("primary"), true, geometry, QCborArray{0, 2}, 0));
    hosts.append(cogHostRecord(QStringLiteral("detached_history"), false, geometry, QCborArray{1}, 0));
    state.insert(QStringLiteral("hosts"), hosts);
    state.insert(QStringLiteral("active"), 1);
    state.insert(QStringLiteral("dockVisible"), true);
    return state;
}

QByteArray nativeCogWorkspace(const QCborMap& fullState)
{
    Level level{};
    level.kind = ProjectType::JKDF2;
    level.header.version = 1;
    level.layers = {"Base"};
    const QByteArray encoded = canonical(fullState);
    assets::SedOptionalSection section{};
    section.type = ui::project::SedEditorStateSection;
    section.payload.assign(reinterpret_cast<const std::byte*>(encoded.constData()),
        reinterpret_cast<const std::byte*>(encoded.constData()) + encoded.size());
    const std::array<assets::SedOptionalSection, 1> sections{section};
    const assets::SedFile decoded = assets::loadSedFileBytes(assets::saveToSedBytes(level, sections));
    return decoded.optionalSections.size() == 1
        ? QByteArray(reinterpret_cast<const char*>(decoded.optionalSections[0].payload.data()),
            static_cast<qsizetype>(decoded.optionalSections[0].payload.size())) : QByteArray{};
}

void checkLegacyEditWithoutResourceEditorsKeepsCogTransfersCurrent()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    MainWindow source(nullptr, true);
    if ( !temporary.isValid() || !openCogFixture(source, root) )
    {
        return;
    }
    QWidget* const sourcePage = cogPage(source, QStringLiteral("second.cog"));
    QPlainTextEdit* const sourceEditor = sourcePage ? sourcePage->findChild<QPlainTextEdit*>() : nullptr;
    appendCogCommand(sourceEditor, QStringLiteral("# legacy raw unclosed \"\n"));
    const QString current = sourceEditor ? sourceEditor->toPlainText() : QString{};
    appendCogCommand(sourceEditor, QStringLiteral("# retained redo \U0001f642\n"));
    const QString future = sourceEditor ? sourceEditor->toPlainText() : QString{};
    if ( sourceEditor )
    {
        sourceEditor->undo();
        QTextCursor selected(sourceEditor->document());
        selected.setPosition(3);
        selected.setPosition(9, QTextCursor::KeepAnchor);
        sourceEditor->setTextCursor(selected);
    }
    const QByteArray expected = canonical(cogDocumentState(source, QStringLiteral("second.cog")));
    QCborMap full = QCborValue::fromCbor(source.captureRecoveryEditorStateForTest()).toMap();
    full.insert(QStringLiteral("cogEditors"), canonical(cogHostFixture(source)));
    // resourceEditors is optional in legacy EDIT. Omitting it must not make
    // restored COG hosts stale relative to the replacement document context.
    full.remove(QStringLiteral("resourceEditors"));
    QString warning{};
    const bool admitted = source.validateRecoveryEditorStateForTest(canonical(full), &warning);
    check(admitted && warning.isEmpty(),
        "legacy full EDIT without optional resource editors still admits exact detached COG source and history");
    const QByteArray native = nativeCogWorkspace(full);
    check(!native.isEmpty() && native == canonical(full),
        "legacy optional-resource omission survives complete native SED EDIT transport exactly");

    MainWindow restored(nullptr, true);
    bindCogProject(restored, root);
    const bool loaded = restored.restoreRecoveryEditorStateForTest(native, &warning);
    const QPointer<QWidget> page(cogPage(restored, QStringLiteral("second.cog")));
    const QPointer<QPlainTextEdit> editor(page ? page->findChild<QPlainTextEdit*>() : nullptr);
    const QPointer<QTextDocument> document(editor ? editor->document() : nullptr);
    const QPointer<QTabWidget> detached(cogHostForPage(restored, page.data()));
    const QPointer<QDockWidget> detachedDock(dockForHost(detached.data()));
    const QPointer<QTabWidget> primary(restored.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs")));
    const QPointer<QDockWidget> primaryDock(dockForHost(primary.data()));
    const bool exactRestore = loaded && warning.isEmpty() && page && editor && document
        && detached && primary && detached != primary && detachedDock && detachedDock->isFloating()
        && primaryDock && cogWorkspace(restored).value(QStringLiteral("hosts")).toArray().size() == 2
        && canonical(cogDocumentState(restored, QStringLiteral("second.cog"))) == expected
        && editor->toPlainText() == current && document->isRedoAvailable();
    if ( !exactRestore )
    {
        std::cerr << "COG optional-resource restore diagnostic: admitted=" << admitted
            << " loaded=" << loaded << " warning=" << warning.toStdString()
            << " exact=" << (canonical(cogDocumentState(restored, QStringLiteral("second.cog"))) == expected)
            << " hosts=" << cogWorkspace(restored).value(QStringLiteral("hosts")).toArray().size() << '\n';
    }
    check(exactRestore,
        "legacy native EDIT without resource editors restores both COG groups and the exact selected mid-history draft");
    if ( !exactRestore )
    {
        return;
    }
    const int undoSteps = document->availableUndoSteps();
    const int redoSteps = document->availableRedoSteps();
    const QRect available = restored.screen() ? restored.screen()->availableGeometry() : QRect(0, 0, 1280, 900);
    detachedDock->resize(std::min(640, available.width() / 2), std::min(480, available.height() / 2));
    detachedDock->move(available.bottomRight() - QPoint(detachedDock->width() - 1, detachedDock->height() - 1));
    primaryDock->show();
    primaryDock->raise();
    restored.raise();
    restored.activateWindow();
    QApplication::processEvents();
    const QPoint destination = primary->tabBar()->mapToGlobal(primary->tabBar()->tabRect(0).center());
    const bool dispatched = dragCogTab(detached.data(), page.data(), destination);
    const bool returned = dispatched && page && editor && document && primary
        && cogHostForPage(restored, page.data()) == primary
        && page->findChild<QPlainTextEdit*>() == editor && editor->document() == document
        && document->availableUndoSteps() == undoSteps && document->availableRedoSteps() == redoSteps
        && canonical(cogDocumentState(restored, QStringLiteral("second.cog"))) == expected;
    if ( !returned )
    {
        QTabBar* const bar = detached ? detached->tabBar() : nullptr;
        std::cerr << "COG optional-resource transfer diagnostic: dispatched=" << dispatched
            << " returned=" << (page && primary && cogHostForPage(restored, page.data()) == primary)
            << " generation=" << (detachedDock ? detachedDock->property("sed.cogScriptEditorRecoveryGeneration").toULongLong() : 0)
            << " target=" << cogWidgetDescription(QApplication::widgetAt(destination)).toStdString()
            << " trace=" << (bar ? bar->property("resource_authoring.dragTrace").toString().toStdString() : "<destroyed>") << '\n';
    }
    check(returned,
        "a detached COG restored from legacy EDIT without resource editors returns through a real tab drag with identical editor history and source");
    if ( returned )
    {
        editor->redo();
        check(editor->toPlainText() == future,
            "legacy EDIT return drag retains the exact supplementary-Unicode Redo command");
        editor->undo();
    }
    check(readCogFile(root / "cog" / "first.cog") == kCogSource
            && readCogFile(root / "cog" / "second.cog") == kCogSource
            && readCogFile(root / "cog" / "third.cog") == kCogSource,
        "legacy optional-resource EDIT restoration and COG return drag never rewrite source files");
}

void checkCogNativeAndRecoveryHosts()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    MainWindow source(nullptr, true);
    if ( !temporary.isValid() || !openCogFixture(source, root) )
    {
        return;
    }
    QWidget* const second = cogPage(source, QStringLiteral("second.cog"));
    QPlainTextEdit* const editor = second ? second->findChild<QPlainTextEdit*>() : nullptr;
    appendCogCommand(editor, QStringLiteral("# intermediate \"\n"));
    const QString current = editor ? editor->toPlainText() : QString{};
    appendCogCommand(editor, QStringLiteral("# future \U0001f642\n"));
    const QString future = editor ? editor->toPlainText() : QString{};
    if ( editor )
    {
        editor->undo();
    }
    const QCborMap hostState = cogHostFixture(source);
    const QByteArray expected = canonical(cogDocumentState(source, QStringLiteral("second.cog")));
    QCborMap full = QCborValue::fromCbor(source.captureRecoveryEditorStateForTest()).toMap();
    full.insert(QStringLiteral("cogEditors"), canonical(hostState));
    QString warning{};
    const QByteArray fullPayload = canonical(full);
    const bool validFull = source.validateRecoveryEditorStateForTest(fullPayload, &warning);
    if ( !validFull || !warning.isEmpty() )
    {
        std::cerr << "COG host fixture admission diagnostic: warning=" << warning.toStdString() << '\n';
        for ( const QCborValue& value : hostState.value(QStringLiteral("tabs")).toArray() )
        {
            const QCborMap tab = value.toMap();
            const QCborMap history = tab.value(QStringLiteral("history")).toMap();
            std::cerr << "COG host fixture tab diagnostic: name=" << tab.value(QStringLiteral("name")).toString().toStdString()
                << " modified=" << tab.value(QStringLiteral("modified")).toBool()
                << " cursor=" << history.value(QStringLiteral("cursor")).toInteger(-1)
                << " clean=" << history.value(QStringLiteral("clean")).toInteger(-1)
                << " commands=" << history.value(QStringLiteral("commands")).toArray().size()
                << " validHistory=" << ui::cog::CogDocumentHistory::validate(history,
                    tab.value(QStringLiteral("text")).toString(), tab.value(QStringLiteral("modified")).toBool()) << '\n';
        }
    }
    check(validFull && warning.isEmpty(),
        "a bounded COG host fixture remains a valid complete MainWindow EDIT record with exact script histories");
    const QByteArray native = nativeCogWorkspace(full);
    check(!native.isEmpty() && native == fullPayload,
        "the complete COG host and journal workspace survives checked native SED EDIT transport exactly");
    MainWindow restored(nullptr, true);
    bindCogProject(restored, root);
    const bool restoredState = restored.restoreRecoveryEditorStateForTest(native, &warning);
    restored.show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const QPointer<QWidget> restoredPage(cogPage(restored, QStringLiteral("second.cog")));
    const QPointer<QTabWidget> host(cogHostForPage(restored, restoredPage.data()));
    const QPointer<QDockWidget> dock(dockForHost(host.data()));
    const QPointer<QPlainTextEdit> restoredEditor(restoredPage ? restoredPage->findChild<QPlainTextEdit*>() : nullptr);
    const bool restoredHostIsDetached = host
        && host != restored.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    const bool restoredDockIsFloating = dock && dock->isFloating();
    const bool restoredDockHasMaximize = dock
        && dock->windowFlags().testFlag(Qt::WindowMaximizeButtonHint);
    const bool restoredNativeDockHasMaximize = nativeMaximizeButtonClickRoundTrip(dock.data());
    const bool restoredPageIsCurrent = host && host->currentWidget() == restoredPage.data();
    const bool restoredHostIdMatches = dock
        && dock->property("sed.cogScriptEditorHostId").toString() == QStringLiteral("detached_history");
    const bool restoredCanonicalStateMatches = restoredPage
        && canonical(cogDocumentState(restored, QStringLiteral("second.cog"))) == expected;
    const bool restoredTextMatches = restoredEditor && restoredEditor->toPlainText() == current;
    const bool restoredRedoAvailable = restoredEditor && restoredEditor->document()->isRedoAvailable();
    if ( !(restoredState && warning.isEmpty() && restoredEditor && host
            && restoredHostIsDetached && restoredDockIsFloating && restoredDockHasMaximize
            && restoredNativeDockHasMaximize
            && restoredPageIsCurrent && restoredHostIdMatches && restoredCanonicalStateMatches
            && restoredTextMatches && restoredRedoAvailable) )
    {
        std::cerr << "COG restored-host diagnostic: restored=" << restoredState
            << " warning=[" << warning.toStdString() << "] editor=" << static_cast<bool>(restoredEditor)
            << " host=" << static_cast<bool>(host) << " detached=" << restoredHostIsDetached
            << " floating=" << restoredDockIsFloating << " maximize=" << restoredDockHasMaximize
            << " nativeMaximize=" << restoredNativeDockHasMaximize
            << " current=" << restoredPageIsCurrent << " hostId=" << restoredHostIdMatches
            << " canonical=" << restoredCanonicalStateMatches << " text=" << restoredTextMatches
            << " redo=" << restoredRedoAvailable << '\n';
    }
    check(restoredState && warning.isEmpty() && restoredEditor && host
            && host != restored.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"))
            && dock && dock->isFloating()
            && dock->windowFlags().testFlag(Qt::WindowMaximizeButtonHint)
            && restoredNativeDockHasMaximize
            && host->currentWidget() == restoredPage
            && dock->property("sed.cogScriptEditorHostId").toString() == QStringLiteral("detached_history")
            && canonical(cogDocumentState(restored, QStringLiteral("second.cog"))) == expected
            && restoredEditor->toPlainText() == current && restoredEditor->document()->isRedoAvailable(),
        "native EDIT and autosave restore retain detached COG membership, active script and exact dirty mid-history raw source");
    if ( restoredEditor )
    {
        restoredEditor->redo();
        check(restoredEditor->toPlainText() == future,
            "a detached COG script restored from native EDIT still applies its exact saved Redo command");
        restoredEditor->undo();
    }
    const QCborMap captured = cogWorkspace(restored);
    check(captured.value(QStringLiteral("hosts")).isArray()
            && captured.value(QStringLiteral("hosts")).toArray().size() == 2
            && captured.value(QStringLiteral("tabs")).toArray().size() == 3,
        "subsequent COG recovery capture includes each detached and primary script exactly once with its host record");

    if ( restoredState && restoredEditor && dock && dock->isFloating() )
    {
        const QPointer<QDockWidget> primaryDock(restored.findChild<QDockWidget*>(QStringLiteral("cogScriptEditorDock")));
        const QPointer<GlView> view(restored.glViewForTest());
        focusCogPage(restored, restoredPage.data());
        const QByteArray beforePreview = canonical(cogDocumentState(restored, QStringLiteral("second.cog")));
        const bool initiallyEditable = view && primaryDock && dock->isVisible()
            && dock->widget() && dock->widget()->isEnabled();
        check(initiallyEditable, "the restored detached COG script is visible and editable before real Game Preview");
        if ( initiallyEditable )
        {
            view->setRenderPipeline(GlView::RenderPipeline::GamePreview);
            QApplication::processEvents();
            check(primaryDock && dock && primaryDock->isHidden() && dock->isHidden()
                    && primaryDock->widget() && !primaryDock->widget()->isEnabled()
                    && !dock->widget()->isEnabled(),
                "real Game Preview hides and disables both primary and detached COG script hosts");
            const QCborMap previewCog = cogWorkspace(restored);
            bool requestedVisible{};
            for ( const QCborValue& hostValue : previewCog.value(QStringLiteral("hosts")).toArray() )
            {
                const QCborMap record = hostValue.toMap();
                if ( record.value(QStringLiteral("id")).toString() == QStringLiteral("detached_history") )
                {
                    requestedVisible = record.value(QStringLiteral("visible")).toBool();
                }
            }
            check(requestedVisible && canonical(cogDocumentState(restored, QStringLiteral("second.cog"))) == beforePreview,
                "capturing in Game Preview retains requested detached COG visibility and exact raw document history");
            const QCborMap previewFull = QCborValue::fromCbor(restored.captureRecoveryEditorStateForTest()).toMap();
            const QByteArray previewNative = nativeCogWorkspace(previewFull);
            MainWindow previewRestored(nullptr, true);
            bindCogProject(previewRestored, root);
            QString previewWarning{};
            const bool previewLoaded = previewRestored.restoreRecoveryEditorStateForTest(previewNative, &previewWarning);
            const QPointer<QWidget> previewPage(cogPage(previewRestored, QStringLiteral("second.cog")));
            const QPointer<QDockWidget> previewDock(dockForHost(cogHostForPage(previewRestored, previewPage.data())));
            const QPointer<QDockWidget> previewPrimary(previewRestored.findChild<QDockWidget*>(QStringLiteral("cogScriptEditorDock")));
            const QPointer<GlView> previewView(previewRestored.glViewForTest());
            const bool readOnlyPreview = previewLoaded && previewWarning.isEmpty() && previewView
                    && previewView->renderPipeline() == GlView::RenderPipeline::GamePreview
                    && previewDock && previewPrimary && previewDock->isHidden() && previewPrimary->isHidden()
                    && previewDock->widget() && !previewDock->widget()->isEnabled()
                    && previewPrimary->widget() && !previewPrimary->widget()->isEnabled();
            if ( !readOnlyPreview )
            {
                std::cerr << "COG Preview diagnostic: restored=" << previewLoaded
                    << " warning=" << previewWarning.toStdString()
                    << " preview=" << (previewView && previewView->renderPipeline() == GlView::RenderPipeline::GamePreview)
                    << " primaryAlive=" << static_cast<bool>(previewPrimary)
                    << " detachedAlive=" << static_cast<bool>(previewDock)
                    << " primaryHidden=" << (previewPrimary && previewPrimary->isHidden())
                    << " detachedHidden=" << (previewDock && previewDock->isHidden())
                    << " primaryEnabled=" << (previewPrimary && previewPrimary->widget() && previewPrimary->widget()->isEnabled())
                    << " detachedEnabled=" << (previewDock && previewDock->widget() && previewDock->widget()->isEnabled()) << '\n';
            }
            check(readOnlyPreview,
                "native SED recovery opened in Preview keeps every restored COG host hidden and read-only");
            if ( previewView )
            {
                previewView->setRenderPipeline(GlView::RenderPipeline::Editor);
                QApplication::processEvents();
            }
            check(previewDock && previewDock->isVisible() && previewDock->widget() && previewDock->widget()->isEnabled()
                    && previewPrimary && previewPrimary->isVisible() && previewPrimary->widget() && previewPrimary->widget()->isEnabled()
                    && canonical(cogDocumentState(previewRestored, QStringLiteral("second.cog"))) == beforePreview
                    && cogWorkspace(previewRestored).value(QStringLiteral("tabs")).toArray().size() == 3
                    && readCogFile(root / "cog" / "second.cog") == kCogSource,
                "returning to Editor restores requested COG windows and exact source selection cursor clean marker and Redo without duplicating files");
            if ( view )
            {
                view->setRenderPipeline(GlView::RenderPipeline::Editor);
            }
        }
    }

    QCborMap legacy = hostState;
    legacy.remove(QStringLiteral("hosts"));
    MainWindow legacyWindow(nullptr, true);
    bindCogProject(legacyWindow, root);
    const bool legacyRestored = legacyWindow.restoreCogEditorRecoveryStateForTest(canonical(legacy));
    QTabWidget* const legacyTabs = legacyWindow.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    const QCborMap legacyPageState = cogDocumentState(legacyWindow, QStringLiteral("second.cog"));
    const bool legacyExact = legacyRestored && legacyTabs && legacyTabs->count() == 3
        && canonical(legacyPageState) == expected;
    if ( !legacyExact )
    {
        std::cerr << "COG legacy host fixture diagnostic: restored=" << legacyRestored
            << " tabs=" << (legacyTabs ? legacyTabs->count() : -1)
            << " exact=" << (canonical(legacyPageState) == expected) << '\n';
        reportCogStateDifferences(cogDocumentState(source, QStringLiteral("second.cog")), legacyPageState);
    }
    check(legacyExact,
        "legacy COG EDIT without host metadata restores all scripts and histories in the existing primary group");
    check(readCogFile(root / "cog" / "first.cog") == kCogSource
            && readCogFile(root / "cog" / "second.cog") == kCogSource
            && readCogFile(root / "cog" / "third.cog") == kCogSource,
        "restoring detached COG workspace presentation never saves or extracts source files");
}

void checkMalformedCogHostTopologyIsAtomic()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    MainWindow window(nullptr, true);
    if ( !temporary.isValid() || !openCogFixture(window, root) )
    {
        return;
    }
    QWidget* const page = cogPage(window, QStringLiteral("first.cog"));
    QPointer<QPlainTextEdit> editor(page ? page->findChild<QPlainTextEdit*>() : nullptr);
    appendCogCommand(editor, QStringLiteral("# kept live command\n"));
    const QCborMap valid = cogHostFixture(window);
    const QByteArray before = canonical(cogWorkspace(window));
    const int steps = editor ? editor->document()->availableUndoSteps() : -1;
    const int cursor = editor ? editor->textCursor().position() : -1;
    for ( int arm = 0; arm < 5; ++arm )
    {
        QCborMap malformed = valid;
        QCborArray hosts = malformed.value(QStringLiteral("hosts")).toArray();
        QCborMap detached = hosts.at(1).toMap();
        if ( arm == 0 )
        {
            detached.insert(QStringLiteral("tabs"), QCborArray{0});
        }
        else if ( arm == 1 )
        {
            detached.insert(QStringLiteral("id"), QStringLiteral("primary"));
        }
        else if ( arm == 2 )
        {
            detached.insert(QStringLiteral("active"), 1);
        }
        else if ( arm == 3 )
        {
            detached.insert(QStringLiteral("geometry"), QByteArray(1024 * 1024 + 1, 'x'));
        }
        else
        {
            QCborArray tabs{};
            hosts = QCborArray{};
            const QCborMap prototype = valid.value(QStringLiteral("tabs")).toArray().at(0).toMap();
            for ( int index = 0; index < 65; ++index )
            {
                QCborMap tab = prototype;
                const QString name = QStringLiteral("bounded_%1.cog").arg(index);
                tab.insert(QStringLiteral("name"), name);
                tab.insert(QStringLiteral("path"), QString::fromStdWString((root / "cog" / name.toStdWString()).wstring()));
                tabs.append(tab);
                hosts.append(cogHostRecord(index == 0 ? QStringLiteral("primary")
                        : QStringLiteral("host_%1").arg(index), index == 0, QByteArray{}, QCborArray{index}, 0));
            }
            malformed.insert(QStringLiteral("tabs"), tabs);
        }
        if ( arm < 4 )
        {
            hosts[1] = detached;
        }
        malformed.insert(QStringLiteral("hosts"), hosts);
        QCborMap full = QCborValue::fromCbor(window.captureRecoveryEditorStateForTest()).toMap();
        full.insert(QStringLiteral("cogEditors"), canonical(malformed));
        QString warning{};
        const bool admitted = window.validateRecoveryEditorStateForTest(canonical(full), &warning);
        const bool restored = window.restoreCogEditorRecoveryStateForTest(canonical(malformed));
        const bool atomic = !admitted && !warning.isEmpty() && !restored && editor
            && canonical(cogWorkspace(window)) == before && editor->document()->availableUndoSteps() == steps
            && editor->textCursor().position() == cursor && cogHostForPage(window, page)
            && readCogFile(root / "cog" / "first.cog") == kCogSource;
        if ( !atomic )
        {
            std::cerr << "COG host admission diagnostic: arm=" << arm << " admitted=" << admitted
                << " restored=" << restored << " warning=" << warning.toStdString()
                << " liveExact=" << (canonical(cogWorkspace(window)) == before) << '\n';
        }
        check(atomic,
            "malformed COG host membership identity active index geometry or host count rejects before any live script or history changes");
    }
}

} // namespace

void runCogDocumentDockingTests()
{
    checkCogDraftSaveProtectsAnotherHostDestination();
    checkCogArchiveTabTooltipIdentifiesMountedSource();
    checkCogDetachTransferAndDockReturn();
    checkCogPrimaryLastFileTransferHidesAndReopensEmptyHost();
    checkCogDockReturnRetiresNativeAnimatedEmptyHost();
    checkCogWholeWindowMergePreservesDocumentsAndMixedActiveTarget();
    checkCogWrongKindForeignAndCancel();
    checkDetachedCogActionRoutingAndSaveAll();
    checkDetachedCogCloseAndOwnerLifetime();
    checkLegacyEditWithoutResourceEditorsKeepsCogTransfersCurrent();
    checkCogNativeAndRecoveryHosts();
    checkMalformedCogHostTopologyIsAtomic();
}

} // namespace sed::app_tests
