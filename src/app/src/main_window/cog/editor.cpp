#include "main_window/common.hpp"
#include "app_logging.hpp"
#include "sed/assets/sed.hpp"
#include "ui/cog/cog_script_assignment_utils.hpp"
#include "ui/cog/cog_document_history.hpp"
#include "ui/cog/cog_script_diagnostics_utils.hpp"
#include "ui/cog/cog_script_editor_widget.hpp"
#include "ui/cog/cog_script_format_utils.hpp"
#include "ui/cog/cog_script_insert_utils.hpp"
#include "ui/cog/cog_script_language_utils.hpp"
#include "ui/cog/cog_script_navigation_utils.hpp"
#include "ui/cog/cog_script_outline_utils.hpp"
#include "ui/cog/cog_script_symbol_utils.hpp"
#include "ui/cog/cog_script_tab_utils.hpp"
#include "ui/shortcuts/shortcut_context_utils.hpp"
#include "ui/components/dialog_window_policy.hpp"
#include "ui/cog/cog_script_text_utils.hpp"
#include "ui/components/preferred_width_dock_widget.hpp"
#include "ui/components/document_tab_drag_filter.hpp"
#include "ui/components/dock_widget_retirement.hpp"
#include "ui/components/loose_document_watcher.hpp"
#include "ui/components/resource_unsaved_changes_prompt.hpp"
#include <QUuid>
#include "ui/recovery/document_recovery_utils.hpp"
#include "ui/resources/resource_picker.hpp"
#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "ui/resources/archive_source_tooltip.hpp"

#include <array>
#include <exception>

#include <QLayout>
#include <QApplication>
#include <QComboBox>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QCryptographicHash>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QProcess>
#include <QPushButton>
#include <QPointer>
#include <QScrollBar>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QTimer>
#include <QSaveFile>
#include <QScreen>
#include <QSet>
#include <QUrl>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QEasingCurve>
#include <QtConcurrent/QtConcurrentRun>

// MainWindow COG editor support: script loading, editor widget internals,
// tab presentation, and dock wiring live here so the dock layout module can
// stay focused on arranging high-level panels.

namespace sed
{

namespace
{

std::optional<std::filesystem::path> cogAuthoringRoot(
    const std::filesystem::path& project,
    const std::vector<std::filesystem::path>& roots)
{
    if ( !project.empty() )
    {
        return project.parent_path();
    }
    std::optional<std::filesystem::path> fallback;
    for ( const std::filesystem::path& root : roots )
    {
        const QString leaf = QString::fromStdWString(root.filename().wstring());
        std::error_code error{};
        if ( leaf.compare(QStringLiteral("Episode"), Qt::CaseInsensitive) == 0 ||
             !std::filesystem::is_directory(root, error) || error )
        {
            continue;
        }
        if ( leaf.compare(QStringLiteral("Resource"), Qt::CaseInsensitive) == 0 )
        {
            return root;
        }
        if ( !fallback )
        {
            fallback = root;
        }
    }
    return fallback;
}

constexpr qint64 kMaximumCogScriptSourceBytes = 16 * 1024 * 1024;

enum class CogScriptSourceReadFailure
{
    none,
    open,
    overLimit,
    read,
};

struct CogScriptSourceReadResult
{
    QByteArray bytes{};
    CogScriptSourceReadFailure failure{ CogScriptSourceReadFailure::none };
    QString systemError{};

    bool succeeded() const noexcept
    {
        return failure == CogScriptSourceReadFailure::none;
    }
};

CogScriptSourceReadResult readCogScriptSourceBounded(const std::filesystem::path& path)
{
    QFile source(QString::fromStdWString(path.wstring()));
    if ( !source.open(QIODevice::ReadOnly) )
    {
        return { {}, CogScriptSourceReadFailure::open, source.errorString() };
    }
    if ( source.size() > kMaximumCogScriptSourceBytes )
    {
        return { {}, CogScriptSourceReadFailure::overLimit, {} };
    }

    QByteArray bytes = source.read(kMaximumCogScriptSourceBytes + 1);
    if ( source.error() != QFileDevice::NoError )
    {
        return { {}, CogScriptSourceReadFailure::read, source.errorString() };
    }
    if ( bytes.size() > kMaximumCogScriptSourceBytes || !source.atEnd() )
    {
        // The size can change between the initial metadata check and the
        // bounded read. Treat any remaining source bytes as over budget.
        return { {}, CogScriptSourceReadFailure::overLimit, {} };
    }
    return { std::move(bytes), CogScriptSourceReadFailure::none, {} };
}

QString cogScriptSourceReadError(
    const CogScriptSourceReadResult& result,
    const QString& path)
{
    switch ( result.failure )
    {
        case CogScriptSourceReadFailure::none:
            return {};
        case CogScriptSourceReadFailure::open:
            return QStringLiteral("Couldn't read script:\n%1\n\n%2")
                .arg(path, result.systemError);
        case CogScriptSourceReadFailure::overLimit:
            return QStringLiteral("The script exceeds the 16 MiB source limit.");
        case CogScriptSourceReadFailure::read:
            return QStringLiteral("Couldn't read the complete script:\n%1\n\n%2")
                .arg(path, result.systemError);
    }
    return QStringLiteral("Couldn't read the script.");
}

class CogHostCloseFilter final : public QObject
{
public:
    CogHostCloseFilter(QObject* owner, std::function<bool()> close)
        : QObject(owner)
        , close_(std::move(close))
    {
    }
protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if ( event->type() != QEvent::Close )
        {
            return false;
        }
        const auto close = close_;
        const bool accepted = close();
        event->setAccepted(accepted);
        return true;
    }
private:
    std::function<bool()> close_{};
};

void ensureFloatingCogMaximizeButton(QDockWidget* const rawDock)
{
    QPointer<QDockWidget> dock(rawDock);
    if ( !dock || !dock->isFloating() )
    {
        return;
    }

    constexpr Qt::WindowFlags kNativeFloatingWindowFlags =
        Qt::Window
        | Qt::CustomizeWindowHint
        | Qt::WindowTitleHint
        | Qt::WindowSystemMenuHint
        | Qt::WindowCloseButtonHint
        | Qt::WindowMaximizeButtonHint;
    const Qt::WindowFlags currentFlags = dock->windowFlags();
    if ( (currentFlags & Qt::WindowType_Mask) == Qt::Window
        && (currentFlags & kNativeFloatingWindowFlags) == kNativeFloatingWindowFlags )
    {
        return;
    }

    const bool wasVisible = dock->isVisible();
    const bool wasMaximized = dock->isMaximized();
    const bool wasActive = dock->isActiveWindow();
    const bool wasShowWithoutActivating = dock->testAttribute(Qt::WA_ShowWithoutActivating);
    const QRect previousFrameGeometry = dock->frameGeometry();
    const QSize previousClientSize = dock->size();
    QPointer<QWidget> previousFocus(QApplication::focusWidget());
    const bool focusWasInside = previousFocus
        && (previousFocus.data() == dock.data() || dock->isAncestorOf(previousFocus.data()));
    Qt::WindowFlags repairedFlags = currentFlags;
    repairedFlags &= ~Qt::WindowType_Mask;
    repairedFlags |= kNativeFloatingWindowFlags;

    // Qt reconstructs QDockWidget's top-level flags as Qt::Tool when it floats,
    // which suppresses the native maximize control on Windows. Repair the
    // complete caption only after the native drag transition has ended.
    if ( wasVisible )
    {
        dock->setAttribute(Qt::WA_ShowWithoutActivating, true);
        if ( !dock )
        {
            return;
        }
    }
    dock->setWindowFlags(repairedFlags);
    if ( !dock )
    {
        return;
    }
    if ( wasVisible )
    {
        if ( wasMaximized )
        {
            dock->showMaximized();
            if ( !dock )
            {
                return;
            }
        }
        else
        {
            dock->resize(previousClientSize);
            if ( !dock )
            {
                return;
            }
            dock->show();
            if ( !dock )
            {
                return;
            }
            const QRect currentFrameGeometry = dock->frameGeometry();
            dock->move(dock->pos()
                + previousFrameGeometry.topLeft() - currentFrameGeometry.topLeft());
            if ( !dock )
            {
                return;
            }
        }
    }
    if ( wasVisible )
    {
        dock->setAttribute(Qt::WA_ShowWithoutActivating, wasShowWithoutActivating);
        if ( !dock )
        {
            return;
        }
    }
    if ( !dock )
    {
        return;
    }
    if ( wasActive )
    {
        dock->raise();
        if ( !dock )
        {
            return;
        }
        dock->activateWindow();
        if ( !dock )
        {
            return;
        }
    }
    if ( focusWasInside && previousFocus && previousFocus->isVisible() )
    {
        previousFocus->setFocus(Qt::OtherFocusReason);
    }
}

class CogDockTitleIcon final : public QLabel
{
public:
    explicit CogDockTitleIcon(QDockWidget* const dock)
        : QLabel(dock)
        , dock_(dock)
    {
        setObjectName(QStringLiteral("cogEditorDockTitleIcon"));
        setFixedSize(16, 16);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        setStyleSheet(QStringLiteral("background: transparent; border: none; padding: 0px;"));
        setPixmap(dock->windowIcon().pixmap(size()));
        dock->installEventFilter(this);
        if ( QWidget* const body = dock->widget() )
        {
            body->installEventFilter(this);
        }
        connect(dock, &QWidget::windowIconChanged, this, [this](const QIcon& icon)
            {
                setPixmap(icon.pixmap(size()));
            });
        connect(dock, &QDockWidget::topLevelChanged, this, [this](bool)
            {
                updatePlacement();
                queueFloatingMaximizeButtonRepair();
            });
        updatePlacement();
    }

protected:
    bool eventFilter(QObject* const watched, QEvent* const event) override
    {
        switch ( event->type() )
        {
        case QEvent::Hide:
            hide();
            break;
        case QEvent::Show:
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::LayoutRequest:
        case QEvent::StyleChange:
            updatePlacement();
            if ( watched == dock_.data() && event->type() == QEvent::Show )
            {
                queueFloatingMaximizeButtonRepair();
            }
            break;
        case QEvent::UngrabMouse:
        case QEvent::MouseButtonRelease:
        case QEvent::NonClientAreaMouseButtonRelease:
            if ( watched == dock_.data() )
            {
                queueFloatingMaximizeButtonRepair();
            }
            break;
        case QEvent::NonClientAreaMouseMove:
            if ( watched == dock_.data()
                && !static_cast<QMouseEvent*>(event)->buttons().testFlag(Qt::LeftButton) )
            {
                queueFloatingMaximizeButtonRepair();
            }
            break;
        default:
            break;
        }
        return QLabel::eventFilter(watched, event);
    }

private:
    void queueFloatingMaximizeButtonRepair()
    {
        if ( !dock_ || !dock_->isFloating() )
        {
            return;
        }

        const QPointer<QDockWidget> dock = dock_;
        QTimer::singleShot(0, this, [dock]()
            {
                if ( !dock || !dock->isFloating()
                    || QApplication::mouseButtons().testFlag(Qt::LeftButton)
                    || QWidget::mouseGrabber() == dock.data() )
                {
                    return;
                }
                ensureFloatingCogMaximizeButton(dock.data());
            });
    }

    void updatePlacement()
    {
        QWidget* const body = dock_ ? dock_->widget() : nullptr;
        const int titleHeight = body ? body->geometry().top() : 0;
        if ( !dock_ || !body || dock_->isFloating() || !dock_->isVisible() || !body->isVisible()
            || dock_->titleBarWidget() || titleHeight < height() )
        {
            hide();
            return;
        }
        move(8, (titleHeight - height()) / 2);
        show();
        raise();
    }

    QPointer<QDockWidget> dock_{};
};

void positionCogPopupAtAnchor(
    QDialog& dialog,
    const QPoint globalAnchor,
    const QWidget* const owner)
{
    QPoint position = globalAnchor + QPoint(8, 8);
    const QScreen* screen = QGuiApplication::screenAt(globalAnchor);
    if ( !screen && owner )
    {
        screen = owner->screen();
    }
    if ( screen )
    {
        const QRect available = screen->availableGeometry().adjusted(6, 6, -6, -6);
        const int maximumX = std::max(
            available.left(), available.right() - dialog.width() + 1);
        const int maximumY = std::max(
            available.top(), available.bottom() - dialog.height() + 1);
        position.setX(std::clamp(position.x(), available.left(), maximumX));
        position.setY(std::clamp(position.y(), available.top(), maximumY));
    }
    dialog.move(position);
}

struct CogScriptResourceReadResult
{
    std::optional<std::vector<std::byte>> bytes{};
    bool exceededBudget{};
};

CogScriptResourceReadResult tryReadCogScriptBytesForEditor(
    const sed::assets::ResourceLocator& resources,
    const std::string_view scriptName)
{
    CogScriptResourceReadResult result{};
    result.bytes = resources.tryReadBinary(
        scriptName,
        static_cast<std::size_t>(kMaximumCogScriptSourceBytes),
        result.exceededBudget);
    return result;
}

QString normalizeCogScriptFileName(const QString& name)
{
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
    {
        return {};
    }

    QString base = QFileInfo(trimmed).fileName();
    if (base.isEmpty() || base == QStringLiteral(".")
        || base == QStringLiteral(".."))
    {
        return {};
    }

    if (!base.endsWith(".cog", Qt::CaseInsensitive))
    {
        base += ".cog";
    }
    return base;
}

bool isUnsafeCogDraftName(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
    {
        return true;
    }

    const QFileInfo info(trimmed);
    if (info.isAbsolute()
        || trimmed.contains(QLatin1Char('/'))
        || trimmed.contains(QLatin1Char('\\')))
    {
        return true;
    }

    // QFileInfo treats a Windows drive prefix as relative on POSIX. Keep the
    // draft destination safe and portable across host platforms.
    return trimmed.size() >= 2
        && trimmed.at(1) == QLatin1Char(':')
        && trimmed.at(0).isLetter();
}

std::wstring normalizedCogEditorPathKey(
    const std::filesystem::path& path)
{
    return ui::recovery::normalizedSourcePath(path).toStdWString();
}

struct CogDiskProbeResult
{
    bool conflict{};
    bool error{};
};

std::optional<bool> filesystemPathExistsCaseInsensitive(const std::filesystem::path& candidate)
{
    try
    {
        std::error_code ec{};
        if (std::filesystem::exists(candidate, ec) && !ec)
        {
            return true;
        }

        // Windows already performs this comparison in exists().  The explicit
        // directory probe keeps the same normalized-name behavior for portable
        // project folders on case-sensitive hosts, while running only in the
        // background validator (never on the key-entry path).
        const std::filesystem::path parent = candidate.parent_path();
        const std::wstring target = candidate.filename().wstring();
        if (parent.empty() || target.empty())
        {
            return false;
        }

        // A new/unsaved project commonly has no `cog` directory yet.  That is
        // a clean non-conflict, not an I/O failure.  Distinguish that case
        // before constructing the iterator; permission and other filesystem
        // errors remain conservative probe failures.
        std::error_code parentEc{};
        const std::filesystem::file_status parentStatus =
            std::filesystem::status(parent, parentEc);
        if (parentStatus.type() == std::filesystem::file_type::not_found)
        {
            return false;
        }
        if (parentEc || !std::filesystem::is_directory(parentStatus))
        {
            return std::nullopt;
        }

        ec.clear();
        std::filesystem::directory_iterator it(parent, ec);
        const std::filesystem::directory_iterator end{};
        while (!ec && it != end)
        {
            const std::wstring actual = it->path().filename().wstring();
            if (QString::fromStdWString(actual).compare(
                    QString::fromStdWString(target), Qt::CaseInsensitive) == 0)
            {
                return true;
            }
            it.increment(ec);
        }
        if (ec)
        {
            return std::nullopt;
        }
        return false;
    }
    catch (...)
    {
        // The caller's worker wraps the operation and reports I/O failure
        // separately.  Keep this helper itself non-throwing for all path
        // conversion and directory-iteration implementations.
        return std::nullopt;
    }
}

using ui::cog::buildCogScriptCallableWords;
using ui::cog::buildCogScriptCompletionWords;
using ui::cog::buildCogScriptDiagnostics;
using ui::cog::CogMaskThingTypeOption;
using ui::cog::CogScriptCodeEditor;
using ui::cog::CogScriptDiagnostic;
using ui::cog::CogScriptInsertContext;
using ui::cog::CogScriptFlagOption;
using ui::cog::CogScriptSectionsInfo;
using ui::cog::CogScriptSymbolLineAttributeMatch;
using ui::cog::CogScriptSymbolLineContext;
using ui::cog::CogScriptSyntaxHighlighter;
using ui::cog::cogScriptAttributeHelpText;
using ui::cog::cogScriptAttributeWords;
using ui::cog::cogScriptGlobalWords;
using ui::cog::cogScriptHoverHelpText;
using ui::cog::cogScriptInsertTypeWords;
using ui::cog::cogScriptKeywordWords;
using ui::cog::cogScriptMaskThingTypeOptions;
using ui::cog::cogScriptMessageWords;
using ui::cog::cogScriptProjectMessageWords;
using ui::cog::cogScriptSectionWords;
using ui::cog::cogScriptSymbolLineAttributeAtColumn;
using ui::cog::cogScriptSymbolLineAttributeCount;
using ui::cog::cogScriptSymbolLineAttributeMatch;
using ui::cog::cogScriptSymbolLineAttributeValue;
using ui::cog::cogScriptSymbolLineAttributeValueHit;
using ui::cog::cogScriptSymbolLineContextAtLine;
using ui::cog::cogScriptSymbolLineHasAttribute;
using ui::cog::cogScriptSupportedMaskBits;
using ui::cog::CogScriptTabHoverFilter;
using ui::cog::cogScriptTabCanClose;
using ui::cog::cogScriptTabRevealPath;
using ui::cog::cogScriptTypeSupportsTriggerFilterAttributes;
using ui::cog::cogScriptTypeWords;
using ui::cog::clampCogScriptEditorZoomPercent;
using ui::cog::cogScriptEditorFontAtZoom;
using ui::cog::dedupeCogScriptSymbolLineAttribute;
using ui::cog::findCogScriptCodeLabelLine;
using ui::cog::findCogScriptDefinitionLine;
using ui::cog::findCogScriptIdentifierOccurrences;
using ui::cog::findCogScriptSymbolLine;
using ui::cog::findCogScriptTextMatches;
using ui::cog::normalizeCogScriptTabOrder;
using ui::cog::cogScriptInsertContextAtLine;
using ui::cog::preferredCogScriptCodeInsertLine;
using ui::cog::preferredCogScriptMessageSymbolInsertLine;
using ui::cog::preferredCogScriptSectionInsertLine;
using ui::cog::preferredCogScriptSymbolInsertLine;
using ui::cog::removeCogScriptSymbolLineAttribute;
using ui::cog::parseCogScriptEditorZoomPercent;
using ui::cog::resolveCogScriptEditorFont;
using ui::cog::refreshCogScriptDockTabPresentation;
using ui::cog::scanCogScriptSections;
using ui::cog::updateCogScriptDockTabPresentation;
using ui::cog::upsertCogScriptSymbolLineAttribute;
using ui::cog::buildCogScriptOutline;
using ui::cog::CogScriptOutlineEntry;
using ui::cog::CogScriptOutlineGroup;
using ui::cog::formatCogScriptText;

constexpr int kCogEditorZoomControlWidth = 60;
constexpr int kCogEditorAccessoryControlHeight = 21;
constexpr int kCogEditorAccessoryHeight = 24;
constexpr int kCogEditorFindControlHeight = 17;
constexpr int kCogEditorOutlineMinimumWidth = 260;
constexpr int kCogEditorDerivedStateDebounceMs = 90;

enum class CogFindGlyph
{
    previous,
    next,
    close,
};

QIcon makeCogFindIcon(const CogFindGlyph glyph)
{
    constexpr int kSize = 18;
    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(245, 247, 250));
    pen.setWidthF(2.15);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);

    if (glyph == CogFindGlyph::close)
    {
        painter.drawLine(QPointF(2.0, 2.0), QPointF(16.0, 16.0));
        painter.drawLine(QPointF(16.0, 2.0), QPointF(2.0, 16.0));
    }
    else if (glyph == CogFindGlyph::previous)
    {
        painter.drawPolyline(QPolygonF{
            QPointF(13.0, 2.0), QPointF(6.0, 9.0), QPointF(13.0, 16.0),
        });
    }
    else
    {
        painter.drawPolyline(QPolygonF{
            QPointF(5.0, 2.0), QPointF(12.0, 9.0), QPointF(5.0, 16.0),
        });
    }
    return QIcon(pixmap);
}

QMessageBox::StandardButton showCogEditorMessageBox(
    QWidget* parent,
    const QMessageBox::Icon icon,
    const QString& title,
    const QString& text,
    const QMessageBox::StandardButtons buttons = QMessageBox::Ok,
    const QMessageBox::StandardButton defaultButton = QMessageBox::NoButton,
    const QString& loggingContext = {},
    const bool skipAutomaticLogging = false,
    const bool destructiveAffirmativeAction = false)
{
    QMessageBox box;
    box.setWindowModality(Qt::ApplicationModal);
    if ( parent )
    {
        QObject::connect(parent, &QObject::destroyed, &box, &QDialog::reject);
    }
    box.setIcon(icon);
    box.setWindowTitle(title);
    box.setText(text);
    box.setStandardButtons(buttons);
    if (defaultButton != QMessageBox::NoButton)
    {
        box.setDefaultButton(defaultButton);
    }
    if (!loggingContext.trimmed().isEmpty())
    {
        box.setProperty(kMessageBoxLoggingContextProperty, loggingContext);
    }
    if (skipAutomaticLogging)
    {
        box.setProperty(kMessageBoxLoggingSkipProperty, true);
    }
    if (destructiveAffirmativeAction)
    {
        box.setProperty(kMessageBoxLoggingDestructiveProperty, true);
    }
    box.setWindowFlag(Qt::WindowMinMaxButtonsHint, false);
    box.setMinimumSize(560, 220);
    return static_cast<QMessageBox::StandardButton>(box.exec());
}

enum class CogExternalSaveConflictDecision
{
    reload,
    overwrite,
    cancel,
};

CogExternalSaveConflictDecision showCogExternalSaveConflictPrompt(
    QWidget* parent,
    const QString& loggingContext)
{
    QMessageBox box(
        QMessageBox::Warning,
        QStringLiteral("COG Script Changed on Disk"),
        QStringLiteral(
            "This COG script changed or disappeared since it was opened. "
            "Choose whether to reload it or overwrite the disk version."),
        QMessageBox::NoButton,
        nullptr);
    box.setObjectName(QStringLiteral("sed.cog_external_save_conflict_prompt"));
    box.setWindowModality(Qt::ApplicationModal);
    if ( parent )
    {
        QObject::connect(parent, &QObject::destroyed, &box, &QDialog::reject);
    }
    QAbstractButton* const overwrite = box.addButton(
        QStringLiteral("Overwrite Disk"), QMessageBox::AcceptRole);
    QAbstractButton* const reload = box.addButton(
        QStringLiteral("Reload from Disk"), QMessageBox::DestructiveRole);
    QAbstractButton* const cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(qobject_cast<QPushButton*>(cancel));
    box.setEscapeButton(cancel);
    if (!loggingContext.trimmed().isEmpty())
    {
        box.setProperty(kMessageBoxLoggingContextProperty, loggingContext);
    }
    box.setProperty(kMessageBoxLoggingDestructiveProperty, true);
    box.setWindowFlag(Qt::WindowMinMaxButtonsHint, false);
    box.setMinimumSize(560, 220);
    box.exec();
    if (box.clickedButton() == reload)
    {
        return CogExternalSaveConflictDecision::reload;
    }
    if (box.clickedButton() == overwrite)
    {
        return CogExternalSaveConflictDecision::overwrite;
    }
    return CogExternalSaveConflictDecision::cancel;
}

enum class CogEditorFailureSeverity
{
    warning,
    error,
};

void showCogEditorResourceFailure(
    QWidget* parent,
    const QString& title,
    const QString& displayText,
    const QString& logText,
    const CogEditorFailureSeverity severity)
{
    if (severity == CogEditorFailureSeverity::error)
    {
        qCCritical(logResources).noquote() << logText;
    }
    else
    {
        qCWarning(logResources).noquote() << logText;
    }

    (void)showCogEditorMessageBox(
        parent,
        QMessageBox::Information,
        title,
        displayText,
        QMessageBox::Ok,
        QMessageBox::NoButton,
        {},
        true);
}

void showCogEditorApplicationFailure(
    QWidget* parent,
    const QString& title,
    const QString& displayText,
    const QString& logText,
    const CogEditorFailureSeverity severity)
{
    if (severity == CogEditorFailureSeverity::error)
    {
        qCCritical(logApplication).noquote() << logText;
    }
    else
    {
        qCWarning(logApplication).noquote() << logText;
    }

    (void)showCogEditorMessageBox(
        parent,
        QMessageBox::Information,
        title,
        displayText,
        QMessageBox::Ok,
        QMessageBox::NoButton,
        {},
        true);
}

QString cogBufferLoggingContext(
    const QString& scriptName,
    const std::filesystem::path& path)
{
    return QStringLiteral("COG buffer '%1' at '%2'")
        .arg(scriptName, QString::fromStdWString(path.wstring()));
}

std::optional<QString> pickCogInlineAssignmentResource(
    QWidget* parent,
    const ui::ResourcePickerKind kind,
    const QString& currentValue,
    std::shared_ptr<const assets::ResourceLocator> resourceSnapshot,
    const assets::Templates& templateDatabase,
    const Level& currentLevel,
    std::optional<std::filesystem::path> preferredDiskRoot,
    const std::filesystem::path& currentProjectPath)
{
    // The picker runs a nested event loop. Keep template sources in value
    // snapshots so the preview cannot retain pointers into mutable project
    // state while another event changes the active level.
    std::optional<assets::Templates> templateDatabaseSnapshot;
    std::vector<ThingTemplate> templateOverrides;
    if (kind == ui::ResourcePickerKind::template_)
    {
        templateDatabaseSnapshot.emplace(templateDatabase);
        templateOverrides = currentLevel.templates;
    }
    ui::ResourcePickerOptions options{};
    options.kind = kind;
    options.initialName = currentValue.trimmed().toStdString();
    options.preferredDiskRoot = std::move(preferredDiskRoot);
    if (!currentProjectPath.empty())
    {
        options.projectDirectory = currentProjectPath.parent_path();
    }
    options.title = ui::resourcePickerTitle(kind);
    options.previewCmp = currentLevel.getMasterCMP();
    options.previewProjectType = currentLevel.kind;
    options.templateDatabaseFallback = templateDatabaseSnapshot
            && templateDatabaseSnapshot->size() > 0
        ? &*templateDatabaseSnapshot
        : nullptr;
    options.templateOverrides = templateDatabaseSnapshot
        ? &templateOverrides
        : nullptr;

    const std::optional<std::string> picked = ui::pickResource(
        parent, std::move(resourceSnapshot), options);
    if (!picked)
    {
        return std::nullopt;
    }
    return QString::fromStdString(*picked);
}

class CogScriptEditorDialog final : public QWidget
{
public:
    using SaveCallback = std::function<void(const std::filesystem::path&)>;
    using AuthoredBufferResolvedCallback = std::function<void(const std::filesystem::path&)>;
    using AuthoredBufferChangedCallback = std::function<void()>;
    using TitleChangedCallback = std::function<void(const QString&)>;
    using StateChangedCallback = std::function<void()>;
    using DraftSavePathResolver = std::function<std::optional<std::filesystem::path>()>;
    using ScriptPathChangedCallback = std::function<void(
        CogScriptEditorDialog*,
        const std::filesystem::path&,
        const std::filesystem::path&)>;
    using ScriptSectionsInfo = CogScriptSectionsInfo;

    ~CogScriptEditorDialog() override
    {
        ui::stopLooseDocumentWatch(this);
        // The child may receive native resize events during QObject teardown.
        // Drop its callback before this parent begins destruction so the
        // captured dialog can never be called after its lifetime ends.
        if (editor_)
        {
            editor_->setResizeLayoutCompletedCallback({});
        }
    }

    CogScriptEditorDialog(
        const QString& scriptName,
        const std::filesystem::path& path,
        const QString& initialText,
        const ProjectType projectType,
        SaveCallback onSave,
        AuthoredBufferResolvedCallback onAuthoredBufferResolved,
        StateChangedCallback onStateChanged,
        AuthoredBufferChangedCallback onAuthoredBufferChanged,
        QWidget* parent = nullptr)
        : QWidget(parent)
        , scriptName_(normalizeCogScriptFileName(scriptName))
        , path_(path)
        , projectType_(projectType)
        , lastSavedText_(initialText)
        , onSave_(std::move(onSave))
        , onAuthoredBufferResolved_(std::move(onAuthoredBufferResolved))
        , onStateChanged_(std::move(onStateChanged))
        , onAuthoredBufferChanged_(std::move(onAuthoredBufferChanged))
    {
        QSettings settings = settingsStore();
        const CogScriptTheme theme = resolveCogScriptTheme(settings);
        currentTheme_ = theme;
        baseEditorFont_ = resolveCogScriptEditorFont(settings);
        zoomUiFont_ = QApplication::font();
        zoomPercent_ = clampCogScriptEditorZoomPercent(
            settings.value("cog_editor/zoom_percent", 100).toInt());
        formatUseTabs_ = settings.value("cog_editor/format_use_tabs", false).toBool();
        formatIndentWidth_ = std::clamp(settings.value("cog_editor/format_indent_width", 4).toInt(), 1, 8);
        formatBraceStyle_ = ui::cog::braceStyleFromId(settings.value("cog_editor/format_brace_style", QStringLiteral("next_line")).toString());
        formatIndentSections_ = settings.value("cog_editor/format_indent_sections", true).toBool();
        formatIndentCodeBodies_ = settings.value("cog_editor/format_indent_code_bodies", true).toBool();
        formatKeywordSpacing_ = settings.value("cog_editor/format_keyword_spacing", true).toBool();
        formatInsideParenthesesSpacing_ = settings.value("cog_editor/format_inside_parentheses_spacing", false).toBool();
        formatFunctionParenthesesSpacing_ = settings.value(
            "cog_editor/format_function_parentheses_spacing", false).toBool();
        formatForSemicolonSpacing_ = settings.value("cog_editor/format_for_semicolon_spacing", true).toBool();
        formatAlignFinalReturnLabel_ = settings.value("cog_editor/format_align_final_return_label", false).toBool();
        formatTrimExcessBlankLines_ = settings.value("cog_editor/format_trim_excess_blank_lines", true).toBool();
        formatIndentComments_ = settings.value("cog_editor/format_indent_comments", false).toBool();
        formatOnSave_ = settings.value("cog_editor/format_on_save", false).toBool();
        const bool showWhitespaceDefault = settings.value("cog_editor/show_whitespace", false).toBool();

        updateWindowTitle();
        resize(980, 720);
        setWindowModality(Qt::NonModal);
        setWindowFlag(Qt::Widget, true);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        auto* topRow = new QWidget(this);
        topRow->setObjectName(QStringLiteral("cogEditorToolbar"));
        auto* topRowLayout = new QHBoxLayout(topRow);
        topRowLayout->setContentsMargins(0, 0, 0, 0);
        topRowLayout->setSpacing(6);

        MainWindow* historyOwner = nullptr;
        for ( QWidget* ancestor = parent; ancestor; ancestor = ancestor->parentWidget() )
        {
            if ( auto* const window = dynamic_cast<MainWindow*>(ancestor) )
            {
                historyOwner = window;
                break;
            }
        }
        undoToolButton_ = new QToolButton(topRow);
        undoToolButton_->setObjectName(QStringLiteral("cogEditorUndoButton"));
        ui::configureDocumentHistoryButton(undoToolButton_, historyOwner
            ? historyOwner->findChild<QAction*>(QStringLiteral("edit.undo")) : nullptr, false);
        undoToolButton_->setEnabled(false);
        topRowLayout->addWidget(undoToolButton_);
        redoToolButton_ = new QToolButton(topRow);
        redoToolButton_->setObjectName(QStringLiteral("cogEditorRedoButton"));
        ui::configureDocumentHistoryButton(redoToolButton_, historyOwner
            ? historyOwner->findChild<QAction*>(QStringLiteral("edit.redo")) : nullptr, true);
        redoToolButton_->setEnabled(false);
        topRowLayout->addWidget(redoToolButton_);
        topRowLayout->addSpacing(8);

        whitespaceToolButton_ = new QToolButton(topRow);
        whitespaceToolButton_->setObjectName("cogEditorWhitespaceButton");
        whitespaceToolButton_->setAutoRaise(true);
        whitespaceToolButton_->setCheckable(true);
        whitespaceToolButton_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionWhitespace));
        whitespaceToolButton_->setIconSize(QSize(20, 20));
        whitespaceToolButton_->setFixedSize(30, 30);
        whitespaceToolButton_->setToolTip("Show whitespace");
        topRowLayout->addWidget(whitespaceToolButton_);

        formatToolButton_ = new QToolButton(topRow);
        formatToolButton_->setObjectName("cogEditorFormatButton");
        formatToolButton_->setAutoRaise(true);
        formatToolButton_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionFormat));
        formatToolButton_->setIconSize(QSize(20, 20));
        formatToolButton_->setFixedSize(36, 30);
        formatToolButton_->setPopupMode(QToolButton::MenuButtonPopup);
        formatToolButton_->setAccessibleName(QStringLiteral("Format COG script"));
        formatToolButton_->setToolTip(
            "Format with the current options. Use the arrow to change formatting options.");
        auto* formatMenu = new ui::shortcuts::KeySequenceDisplayMenu(formatToolButton_);
        formatMenu->setObjectName(QStringLiteral("cogEditorFormatMenu"));
        connect(formatMenu, &QMenu::aboutToShow, this, [this, formatMenu]()
            {
                populateFormatMenu(formatMenu);
            });
        formatToolButton_->setMenu(formatMenu);
        topRowLayout->addWidget(formatToolButton_);

        insertToolButton_ = new QToolButton(topRow);
        insertToolButton_->setObjectName(QStringLiteral("cogEditorInsertButton"));
        insertToolButton_->setAutoRaise(true);
        insertToolButton_->setIcon(ui::makeViewportIcon(ui::ViewportIcon::actionInsert));
        insertToolButton_->setIconSize(QSize(20, 20));
        insertToolButton_->setFixedSize(34, 30);
        insertToolButton_->setPopupMode(QToolButton::InstantPopup);
        insertToolButton_->setAccessibleName(QStringLiteral("Insert COG construct"));
        insertToolButton_->setToolTip(
            QStringLiteral("Insert a COG construct or edit this script's flags."));
        auto* insertMenu = new ui::shortcuts::KeySequenceDisplayMenu(insertToolButton_);
        insertMenu->setObjectName(QStringLiteral("cogEditorInsertMenu"));

        connect(insertMenu, &QMenu::aboutToShow, this,
            [this]()
            {
                captureInsertMenuContext();
            });

        auto* snippetsMenu = ui::shortcuts::addKeySequenceDisplayMenu(insertMenu, "Snippet");
        connect(snippetsMenu, &QMenu::aboutToShow, this, [this, snippetsMenu]()
            {
                snippetsMenu->clear();
                populateSnippetInsertMenu(snippetsMenu, insertMenuLineHint_);
            });

        auto* sectionsMenu = ui::shortcuts::addKeySequenceDisplayMenu(insertMenu, "Section");
        connect(sectionsMenu, &QMenu::aboutToShow, this, [this, sectionsMenu]()
            {
                sectionsMenu->clear();
                populateSectionInsertMenu(sectionsMenu, insertMenuLineHint_);
            });

        auto* symbolsMenu = ui::shortcuts::addKeySequenceDisplayMenu(insertMenu, "Symbol");
        connect(symbolsMenu, &QMenu::aboutToShow, this, [this, symbolsMenu]()
            {
                symbolsMenu->clear();
                populateSymbolInsertMenu(symbolsMenu, insertMenuLineHint_);
            });

        auto* attributesMenu = ui::shortcuts::addKeySequenceDisplayMenu(insertMenu, "Symbol attribute");
        connect(insertMenu, &QMenu::aboutToShow, this, [this, attributesMenu]()
            {
                attributesMenu->clear();
                const bool hasAttributeContext = populateAttributeInsertMenu(
                    attributesMenu,
                    insertMenuLineHint_,
                    insertMenuColumnHint_,
                    defaultCogPopupAnchor());
                attributesMenu->setEnabled(hasAttributeContext);
                attributesMenu->menuAction()->setVisible(hasAttributeContext);
            });

        auto* messagesMenu = ui::shortcuts::addKeySequenceDisplayMenu(insertMenu, "Message");
        connect(messagesMenu, &QMenu::aboutToShow, this, [this, messagesMenu]()
            {
                messagesMenu->clear();
                populateHandlerInsertMenu(messagesMenu, insertMenuLineHint_);
            });

        QAction* codeSectionAction = insertMenu->addAction("Code section...");
        codeSectionAction->setToolTip("Insert a custom code label and matching symbol entry.");
        codeSectionAction->setStatusTip("Insert a custom code label and matching symbol entry.");
        connect(codeSectionAction, &QAction::triggered, this, [this]()
            {
                insertGenericLabel(insertMenuLineHint_);
            });

        insertMenu->addSeparator();
        QAction* flagsAction = insertMenu->addAction(QStringLiteral("COG flags..."));
        flagsAction->setObjectName(QStringLiteral("cogEditorFlagsAction"));
        flagsAction->setToolTip(QStringLiteral(
            "Edit the engine-defined flags= bit field for this COG script."));
        connect(flagsAction, &QAction::triggered, this, [this]()
            {
                editCogScriptFlags(
                    insertMenuLineHint_,
                    insertToolButton_
                        ? std::optional<QPoint>(insertToolButton_->mapToGlobal(
                              insertToolButton_->rect().center()))
                        : std::nullopt);
            });

        insertToolButton_->setMenu(insertMenu);
        topRowLayout->addWidget(insertToolButton_);
        topRowLayout->addStretch(1);
        layout->addWidget(topRow);

        findRow_ = new QWidget(this);
        findRow_->setObjectName(QStringLiteral("cogEditorFindRow"));
        auto* findLayout = new QHBoxLayout(findRow_);
        findLayout->setContentsMargins(0, 0, 0, 0);
        findLayout->setSpacing(4);
        auto* findLabel = new QLabel("Find", findRow_);
        findLabel->setStyleSheet(
            "color: rgba(235, 236, 240, 200); font-size: 11px; font-weight: 600;");
        findEdit_ = new QLineEdit(findRow_);
        findEdit_->setObjectName(QStringLiteral("cogEditorFindEdit"));
        findEdit_->setPlaceholderText("Search script");
        // Use a fixed child button instead of the native clear action. The
        // Windows style repositions its internal QLineEditIconButton against
        // the unstyled height and leaves it several pixels above center.
        findEdit_->setClearButtonEnabled(false);
        findEdit_->setFixedWidth(146);
        findEdit_->setStyleSheet(QStringLiteral(
            "QLineEdit { color: #f3f5f8; background: rgba(31, 36, 46, 238);"
            " border: 1px solid rgba(125, 150, 195, 125); border-radius: 3px;"
            " padding: 0 15px 0 4px; }"));
        // Apply the hard height after the stylesheet; otherwise the Windows
        // style can restore its 21px native minimum.
        findEdit_->setFixedHeight(kCogEditorFindControlHeight);
        findClearButton_ = new QToolButton(findEdit_);
        findClearButton_->setObjectName(
            QStringLiteral("cogEditorFindClearButton"));
        findClearButton_->setAutoRaise(true);
        findClearButton_->setFocusPolicy(Qt::NoFocus);
        findClearButton_->setAccessibleName(QStringLiteral("Clear search"));
        findClearButton_->setToolTip(QStringLiteral("Clear search"));
        findClearButton_->setFixedSize(13, 13);
        findClearButton_->setIcon(makeCogFindIcon(CogFindGlyph::close));
        findClearButton_->setIconSize(QSize(10, 10));
        findClearButton_->setStyleSheet(QStringLiteral(
            "QToolButton { margin: 0; padding: 0; border: 0; }"));
        findClearButton_->hide();
        connect(findClearButton_, &QToolButton::clicked,
            findEdit_, &QLineEdit::clear);
        connect(findEdit_, &QLineEdit::textChanged,
            findClearButton_, [this](const QString& text)
            {
                if (findClearButton_)
                {
                    findClearButton_->setVisible(!text.isEmpty());
                    positionFindClearButton();
                }
            });
        positionFindClearButton();
        findEdit_->installEventFilter(this);
        findCaseSensitiveCheck_ = new QCheckBox("Match case", findRow_);
        findCaseSensitiveCheck_->setObjectName(
            QStringLiteral("cogEditorFindMatchCase"));
        findCaseSensitiveCheck_->setToolTip(
            QStringLiteral("Match uppercase and lowercase letters exactly."));
        findCaseSensitiveCheck_->setFixedHeight(kCogEditorAccessoryControlHeight);
        findWholeWordCheck_ = new QCheckBox("Whole word", findRow_);
        findWholeWordCheck_->setObjectName(
            QStringLiteral("cogEditorFindWholeWord"));
        findWholeWordCheck_->setToolTip(
            QStringLiteral("Match only complete words."));
        findWholeWordCheck_->setFixedHeight(kCogEditorAccessoryControlHeight);
        findPreviousButton_ = new QToolButton(findRow_);
        findPreviousButton_->setObjectName(QStringLiteral("cogEditorFindPreviousButton"));
        findPreviousButton_->setAutoRaise(true);
        findPreviousButton_->setIcon(makeCogFindIcon(CogFindGlyph::previous));
        findPreviousButton_->setIconSize(QSize(16, 16));
        findPreviousButton_->setAccessibleName(QStringLiteral("Find previous"));
        findPreviousButton_->setToolTip(QStringLiteral("Find previous match"));
        findPreviousButton_->setFixedSize(20, kCogEditorFindControlHeight);
        findNextButton_ = new QToolButton(findRow_);
        findNextButton_->setObjectName(QStringLiteral("cogEditorFindNextButton"));
        findNextButton_->setAutoRaise(true);
        findNextButton_->setIcon(makeCogFindIcon(CogFindGlyph::next));
        findNextButton_->setIconSize(QSize(16, 16));
        findNextButton_->setAccessibleName(QStringLiteral("Find next"));
        findNextButton_->setToolTip(QStringLiteral("Find next match"));
        findNextButton_->setFixedSize(20, kCogEditorFindControlHeight);
        findCloseButton_ = new QToolButton(findRow_);
        findCloseButton_->setObjectName(QStringLiteral("cogEditorFindCloseButton"));
        findCloseButton_->setAutoRaise(true);
        findCloseButton_->setIcon(makeCogFindIcon(CogFindGlyph::close));
        findCloseButton_->setIconSize(QSize(16, 16));
        findCloseButton_->setAccessibleName(QStringLiteral("Close Find"));
        findCloseButton_->setToolTip("Hide search");
        findCloseButton_->setFixedSize(20, kCogEditorFindControlHeight);
        findStatusLabel_ = new QLabel(findRow_);
        findStatusLabel_->setObjectName(QStringLiteral("cogEditorFindStatus"));
        findStatusLabel_->setAccessibleName(QStringLiteral("Find result count"));
        findStatusLabel_->setFixedSize(76, kCogEditorFindControlHeight);
        findStatusLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        findStatusLabel_->setStyleSheet(
            "color: rgba(235, 236, 240, 150); font-size: 11px;");
        findLayout->addWidget(findLabel);
        findLayout->addWidget(findEdit_, 0, Qt::AlignVCenter);
        findLayout->addWidget(findCaseSensitiveCheck_, 0);
        findLayout->addWidget(findWholeWordCheck_, 0);
        findLayout->addWidget(findPreviousButton_, 0, Qt::AlignVCenter);
        findLayout->addWidget(findNextButton_, 0, Qt::AlignVCenter);
        findLayout->addWidget(findStatusLabel_, 0, Qt::AlignVCenter);
        findLayout->addStretch(1);
        // Keep the hide action at the far edge of the source editor, away
        // from the match navigation cluster. The expanding Find row receives
        // all accessory-row surplus width, so this remains true at every dock
        // width rather than only at the default geometry.
        findLayout->addWidget(findCloseButton_, 0, Qt::AlignVCenter);
        findRow_->setFixedHeight(kCogEditorAccessoryControlHeight);
        findRow_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        const int findRowMinimumWidth = findLayout->minimumSize().width();
        findRow_->hide();

        editorRow_ = new QSplitter(Qt::Horizontal, this);
        editorRow_->setChildrenCollapsible(false);
        editorRow_->setHandleWidth(4);

        editor_ = new CogScriptCodeEditor(this);
        editor_->setResizeLayoutCompletedCallback([this]()
            {
                // Unlike an event filter, this runs after QPlainTextEdit has
                // finalized its viewport and native scrollbar rectangles.
                repositionEditorAccessoryRow();
            });
        editor_->setFont(cogScriptEditorFontAtZoom(baseEditorFont_, zoomPercent_));
        editor_->setIndentationStyle(formatUseTabs_, formatIndentWidth_);
        editor_->setMessageWords(cogScriptProjectMessageWords(projectType_));
        editor_->setPlainText(initialText);
        editor_->document()->setModified(false);
        editor_->applyTheme(theme);
        editorRow_->addWidget(editor_);

        auto* outlinePanel = new QWidget(editorRow_);
        outlinePanel->setObjectName(QStringLiteral("cogEditorOutlinePanel"));
        // Keep both outline lists readable when the horizontal splitter is
        // dragged.  QSplitter::setChildrenCollapsible(false) prevents a true
        // zero-width collapse, but without a practical child minimum Qt can
        // still reduce this pane to little more than its handle.
        outlinePanel->setMinimumWidth(kCogEditorOutlineMinimumWidth);
        auto* outlineLayout = new QVBoxLayout(outlinePanel);
        outlineLayout->setContentsMargins(0, 0, 0, 0);
        outlineLayout->setSpacing(4);
        auto* outlineLabel = new QLabel("Outline", outlinePanel);
        outlineLabel->setStyleSheet("color: rgba(235, 236, 240, 200); font-weight: 700;");
        outlineLayout->addWidget(outlineLabel);

        outlineSplitter_ = new QSplitter(Qt::Vertical, outlinePanel);
        outlineSplitter_->setObjectName(QStringLiteral("cogEditorOutlineSplitter"));
        outlineSplitter_->setChildrenCollapsible(false);
        outlineSplitter_->setHandleWidth(4);
        const QString outlineStyle = QStringLiteral(
            "QListWidget { background: rgba(13, 18, 28, 210); border: 1px solid rgba(120, 150, 210, 45);"
            " border-radius: 8px; padding: 4px; }"
            "QListWidget::item { padding: 4px 6px; color: rgba(235,236,240,205); border-bottom: 1px solid rgba(214,218,223,42); }"
            "QListWidget::item:selected { background-color: rgba(150,156,164,104); color: #ffffff; border-radius: 4px; }");
        const auto addOutlineGroup = [this, &outlineStyle](
                                         const QString& title,
                                         const QString& objectName,
                                         QListWidget*& list)
        {
            auto* panel = new QWidget(outlineSplitter_);
            panel->setMinimumHeight(72);
            auto* panelLayout = new QVBoxLayout(panel);
            panelLayout->setContentsMargins(0, 0, 0, 0);
            panelLayout->setSpacing(3);
            auto* label = new QLabel(title, panel);
            label->setStyleSheet(
                QStringLiteral("color: rgba(235, 236, 240, 185); font-weight: 600;"));
            list = new QListWidget(panel);
            list->setObjectName(objectName);
            list->setSelectionMode(QAbstractItemView::SingleSelection);
            list->setUniformItemSizes(true);
            list->setStyleSheet(outlineStyle);
            panelLayout->addWidget(label);
            panelLayout->addWidget(list, 1);
            outlineSplitter_->addWidget(panel);
        };
        addOutlineGroup(
            QStringLiteral("Symbols"),
            QStringLiteral("cogEditorSymbolsOutlineList"),
            symbolOutlineList_);
        addOutlineGroup(
            QStringLiteral("Code"),
            QStringLiteral("cogEditorCodeOutlineList"),
            codeOutlineList_);
        // QSplitter's global childrenCollapsible property controls the
        // effective behavior, but isCollapsible(index) reports each child's
        // explicit flag. Keep both layers unambiguous so neither outline can
        // be collapsed to zero by a drag or by restored splitter sizes.
        outlineSplitter_->setCollapsible(0, false);
        outlineSplitter_->setCollapsible(1, false);
        outlineSplitter_->setStretchFactor(0, 1);
        outlineSplitter_->setStretchFactor(1, 1);
        outlineSplitter_->setSizes(QList<int>{ 240, 240 });
        outlineLayout->addWidget(outlineSplitter_, 1);
        editorRow_->addWidget(outlinePanel);
        editorRow_->setStretchFactor(0, 1);
        editorRow_->setStretchFactor(1, 0);
        editorRow_->setSizes(QList<int>{ 760, 220 });

        editorColumn_ = new QSplitter(Qt::Vertical, this);
        editorColumn_->setObjectName(QStringLiteral("cogEditorVerticalSplitter"));
        editorColumn_->setChildrenCollapsible(false);
        editorColumn_->setHandleWidth(4);
        editorColumn_->addWidget(editorRow_);

        diagnosticsPanel_ = new QWidget(editorColumn_);
        diagnosticsPanel_->setObjectName(QStringLiteral("cogEditorDiagnosticsPanel"));
        auto* diagnosticsLayout = new QVBoxLayout(diagnosticsPanel_);
        diagnosticsLayout->setContentsMargins(0, 0, 0, 0);
        diagnosticsLayout->setSpacing(4);
        diagnosticsHeader_ = new QWidget(diagnosticsPanel_);
        diagnosticsHeader_->setObjectName(QStringLiteral("cogEditorDiagnosticsHeader"));
        auto* diagnosticsOuterHeaderLayout = new QHBoxLayout(diagnosticsHeader_);
        diagnosticsOuterHeaderLayout->setContentsMargins(0, 0, 0, 0);
        diagnosticsOuterHeaderLayout->setSpacing(0);
        diagnosticsToggle_ = new QToolButton(diagnosticsHeader_);
        diagnosticsToggle_->setObjectName(QStringLiteral("cogEditorDiagnosticsToggle"));
        diagnosticsToggle_->setCheckable(true);
        diagnosticsToggle_->setChecked(true);
        diagnosticsToggle_->setAutoRaise(false);
        diagnosticsToggle_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        diagnosticsToggle_->setAccessibleName(QStringLiteral("Diagnostics"));
        diagnosticsToggle_->setToolTip(QStringLiteral("Show or hide script diagnostics."));
        diagnosticsToggle_->setStyleSheet(QStringLiteral(
            "QToolButton { text-align: left; border: 1px solid rgba(120, 150, 210, 45);"
            " border-radius: 5px; background: rgba(29, 36, 49, 180); padding: 2px 5px; }"
            "QToolButton:hover { background: rgba(48, 60, 80, 190);"
            " border-color: rgba(140, 170, 225, 78); }"));
        diagnosticsOuterHeaderLayout->addWidget(diagnosticsToggle_);
        auto* diagnosticsHeaderLayout = new QHBoxLayout(diagnosticsToggle_);
        diagnosticsHeaderLayout->setContentsMargins(7, 2, 7, 2);
        diagnosticsHeaderLayout->setSpacing(4);
        diagnosticsArrow_ = new QLabel(QStringLiteral("\u25be"), diagnosticsToggle_);
        diagnosticsArrow_->setObjectName(QStringLiteral("cogEditorDiagnosticsArrow"));
        diagnosticsArrow_->setAttribute(Qt::WA_TransparentForMouseEvents);
        diagnosticsArrow_->setAlignment(Qt::AlignCenter);
        diagnosticsArrow_->setFixedWidth(14);
        diagnosticsHeaderLayout->addWidget(diagnosticsArrow_);
        auto* diagnosticsTitle = new QLabel(QStringLiteral("Diagnostics"), diagnosticsToggle_);
        diagnosticsTitle->setObjectName(QStringLiteral("cogEditorDiagnosticsTitle"));
        diagnosticsTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
        diagnosticsTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
        diagnosticsHeaderLayout->addWidget(diagnosticsTitle);

        auto makeDiagnosticCount = [this, diagnosticsHeaderLayout](
                                       const QString& objectName,
                                       const QStyle::StandardPixmap icon,
                                       const QString& tooltip,
                                       const bool error) -> QLabel*
        {
            auto* iconLabel = new QLabel(diagnosticsToggle_);
            iconLabel->setPixmap(style()->standardIcon(icon).pixmap(14, 14));
            iconLabel->setToolTip(tooltip);
            iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            diagnosticsHeaderLayout->addWidget(iconLabel);
            auto* countLabel = new QLabel(QStringLiteral("0"), diagnosticsToggle_);
            countLabel->setObjectName(objectName);
            countLabel->setToolTip(tooltip);
            countLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            countLabel->setAlignment(Qt::AlignCenter);
            countLabel->setMinimumWidth(24);
            countLabel->setFixedHeight(18);
            countLabel->setStyleSheet(error
                ? QStringLiteral(
                    "QLabel { color: #ffb1b1; background: rgba(220, 70, 70, 62);"
                    " border: 1px solid rgba(255, 120, 120, 92); border-radius: 8px; padding: 0 5px; }")
                : QStringLiteral(
                    "QLabel { color: #f4cf87; background: rgba(210, 154, 50, 56);"
                    " border: 1px solid rgba(245, 195, 110, 86); border-radius: 8px; padding: 0 5px; }"));
            diagnosticsHeaderLayout->addWidget(countLabel);
            return countLabel;
        };
        diagnosticsErrorCount_ = makeDiagnosticCount(
            QStringLiteral("cogEditorDiagnosticErrorCount"),
            QStyle::SP_MessageBoxCritical,
            QStringLiteral("Errors"),
            true);
        diagnosticsWarningCount_ = makeDiagnosticCount(
            QStringLiteral("cogEditorDiagnosticWarningCount"),
            QStyle::SP_MessageBoxWarning,
            QStringLiteral("Warnings"),
            false);
        diagnosticsHeaderLayout->addStretch(1);
        diagnosticsLayout->addWidget(diagnosticsHeader_);

        issuesList_ = new QListWidget(diagnosticsPanel_);
        issuesList_->setObjectName(QStringLiteral("cogEditorDiagnosticsList"));
        issuesList_->setSelectionMode(QAbstractItemView::SingleSelection);
        issuesList_->setUniformItemSizes(true);
        issuesList_->setContextMenuPolicy(Qt::CustomContextMenu);
        issuesList_->setStyleSheet(
            "QListWidget { background: rgba(13, 18, 28, 210); border: 1px solid rgba(120, 150, 210, 45);"
            " border-radius: 8px; padding: 4px; }"
            "QListWidget::item { padding: 4px 6px; color: rgba(235,236,240,205); border-bottom: 1px solid rgba(214,218,223,42); }"
            "QListWidget::item:selected { background-color: rgba(150,156,164,104); color: #ffffff; border-radius: 4px; }");
        diagnosticsLayout->addWidget(issuesList_, 1);
        editorColumn_->addWidget(diagnosticsPanel_);
        editorColumn_->setStretchFactor(0, 1);
        editorColumn_->setStretchFactor(1, 0);
        editorColumn_->setSizes(QList<int>{ 540, 150 });
        layout->addWidget(editorColumn_, 1);

        // IDE-style Find and presentation zoom share one stable editor-chrome
        // line. The source editor reserves this strip internally, so revealing
        // Find cannot push source text down or cover either scrollbar.
        editor_->setBottomAccessoryHeight(kCogEditorAccessoryHeight);
        editorAccessoryRow_ = new QWidget(editor_);
        editorAccessoryRow_->setObjectName(QStringLiteral("cogEditorAccessoryRow"));
        editorAccessoryRow_->setLayoutDirection(Qt::LeftToRight);
        editorAccessoryRow_->setFixedHeight(kCogEditorAccessoryHeight);
        editorAccessoryRow_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        editorAccessoryRow_->setStyleSheet(QStringLiteral(
            "QWidget#cogEditorAccessoryRow {"
            " background: transparent; border: none; }"
            "QWidget#cogEditorAccessoryRow[sedFindVisible=\"true\"] {"
            " background: rgba(19, 23, 31, 246);"
            " border-bottom: 1px solid rgba(125, 150, 195, 95); }"));
        auto* accessoryLayout = new QHBoxLayout(editorAccessoryRow_);
        // Pin source-editor chrome in visual left-to-right order even when
        // the native/application layout direction changes during tests or
        // localization. Widget layoutDirection alone does not force a
        // QBoxLayout's explicit direction on every platform style.
        accessoryLayout->setDirection(QBoxLayout::LeftToRight);
        accessoryLayout->setContentsMargins(0, 1, 0, 1);
        accessoryLayout->setSpacing(6);

        zoomCombo_ = new QComboBox(editorAccessoryRow_);
        zoomCombo_->setObjectName(QStringLiteral("cogEditorZoomCombo"));
        zoomCombo_->setAccessibleName(QStringLiteral("Code editor zoom"));
        zoomCombo_->setToolTip(QStringLiteral(
            "Change this COG tab's display zoom. Type any value from 20% to 400%."));
        zoomCombo_->setEditable(true);
        applyZoomControlUiFont();
        zoomCombo_->setInsertPolicy(QComboBox::NoInsert);
        zoomCombo_->setFocusPolicy(Qt::StrongFocus);
        zoomCombo_->setProperty("sed.allowFocusedWheel", true);
        zoomCombo_->addItems(QStringList{
            QStringLiteral("20%"),
            QStringLiteral("50%"),
            QStringLiteral("70%"),
            QStringLiteral("100%"),
            QStringLiteral("150%"),
            QStringLiteral("200%"),
            QStringLiteral("400%"),
        });
        zoomCombo_->setMinimumContentsLength(4);
        zoomCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        zoomCombo_->setFixedSize(
            kCogEditorZoomControlWidth,
            kCogEditorAccessoryControlHeight);
        zoomCombo_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        zoomCombo_->setStyleSheet(QStringLiteral(
            "QComboBox { color: #eeeeee; background: rgba(31, 36, 46, 238);"
            " border: 1px solid rgba(125, 150, 195, 105); border-radius: 4px;"
            " padding: 0; }"
            "QComboBox::drop-down { width: 14px; border: none; }"
            "QComboBox QLineEdit { color: #eeeeee; background: transparent;"
            " border: none; padding: 0; margin: 0; }") );
        setZoomComboText(zoomPercent_);
        zoomCombo_->installEventFilter(this);
        if (zoomCombo_->lineEdit())
        {
            zoomCombo_->lineEdit()->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            zoomCombo_->lineEdit()->setMinimumWidth(0);
            zoomCombo_->lineEdit()->setTextMargins(0, 0, 0, 0);
            zoomCombo_->lineEdit()->installEventFilter(this);
        }
        connect(zoomCombo_, qOverload<int>(&QComboBox::activated), this,
            [this](const int index)
            {
                if (!zoomCombo_ || index < 0)
                {
                    return;
                }
                if (const auto percent = parseCogScriptEditorZoomPercent(
                        zoomCombo_->itemText(index)))
                {
                    applyEditorZoomPercent(*percent, true);
                }
            });
        if (zoomCombo_->lineEdit())
        {
            connect(zoomCombo_->lineEdit(), &QLineEdit::editingFinished, this,
                [this]()
                {
                    if (!zoomCombo_)
                    {
                        return;
                    }
                    if (const auto percent = parseCogScriptEditorZoomPercent(
                            zoomCombo_->currentText()))
                    {
                        applyEditorZoomPercent(*percent, true);
                    }
                    else
                    {
                        setZoomComboText(zoomPercent_);
                    }
                });
        }
        // The fixed zoom control remains left-aligned when Find is hidden.
        // When Find is visible its row consumes the remaining width, allowing
        // the row's own stretch to pin the hide button to the far right.
        accessoryLayout->addWidget(
            zoomCombo_, 0, Qt::AlignLeft | Qt::AlignVCenter);
        findRow_->setParent(editorAccessoryRow_);
        findRow_->installEventFilter(this);
        accessoryLayout->addWidget(findRow_, 1);

        // This manually positioned accessory is not part of the text editor's
        // layout, so its minimum size would otherwise never propagate through
        // the splitter to the COG dock. Reserve enough source-editor width for
        // every visible Find control, including the vertical scrollbar inset.
        // Qt can then stop a dock drag before any controls overlap or clip.
        const QMargins chromeMargins = accessoryLayout->contentsMargins();
        const int chromeMinimumWidth = chromeMargins.left()
            + zoomCombo_->width()
            + accessoryLayout->spacing()
            + findRowMinimumWidth
            + chromeMargins.right();
        const int chromeHorizontalInsets = 8 + (editor_->frameWidth() * 2)
            + style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, editor_);
        editor_->setMinimumWidth(chromeMinimumWidth + chromeHorizontalInsets);
        updateFindAccessoryChrome();
        editorAccessoryRow_->show();
        editorAccessoryRow_->raise();
        repositionEditorAccessoryRow();
        editor_->horizontalScrollBar()->installEventFilter(this);
        editor_->verticalScrollBar()->installEventFilter(this);
        QTimer::singleShot(0, this, [this]() { repositionEditorAccessoryRow(); });

        connect(editorRow_, &QSplitter::splitterMoved, this, [this](int, int)
            {
                if (onStateChanged_) onStateChanged_();
            });
        connect(outlineSplitter_, &QSplitter::splitterMoved, this, [this](int, int)
            {
                if (onStateChanged_) onStateChanged_();
            });
        connect(editorColumn_, &QSplitter::splitterMoved, this, [this](int, int)
            {
                if (suppressDiagnosticsSplitterState_)
                {
                    return;
                }
                if (diagnosticsToggle_ && diagnosticsToggle_->isChecked()
                    && editorColumn_ && editorColumn_->sizes().size() == 2)
                {
                    diagnosticsExpandedHeight_ = std::max(
                        80,
                        editorColumn_->sizes().at(1));
                }
                if (onStateChanged_) onStateChanged_();
            });
        connect(diagnosticsToggle_, &QToolButton::toggled, this, [this](const bool expanded)
            {
                setDiagnosticsExpanded(expanded, true, true);
            });
        connect(issuesList_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
            {
                QListWidgetItem* item = issuesList_ ? issuesList_->itemAt(pos) : nullptr;
                if (!item)
                {
                    return;
                }
                const QString renderedMessage = item->data(Qt::UserRole + 3).toString();
                ui::shortcuts::KeySequenceDisplayMenu menu(issuesList_);
                QAction* copyAction = menu.addAction(QStringLiteral("Copy Message"));
                connect(copyAction, &QAction::triggered, issuesList_, [renderedMessage]()
                    {
                        if (QClipboard* clipboard = QGuiApplication::clipboard())
                        {
                            clipboard->setText(renderedMessage);
                        }
                    });
                menu.exec(issuesList_->viewport()->mapToGlobal(pos));
            });
        setDiagnosticsExpanded(true, false);

        highlighter_ = new CogScriptSyntaxHighlighter(editor_->document(), cogScriptProjectMessageWords(projectType_), theme);
        Q_UNUSED(highlighter_);

        connect(undoToolButton_, &QToolButton::clicked, editor_, &QPlainTextEdit::undo);
        connect(redoToolButton_, &QToolButton::clicked, editor_, &QPlainTextEdit::redo);
        connect(editor_->document(), &QTextDocument::undoAvailable,
            undoToolButton_, &QToolButton::setEnabled);
        connect(editor_->document(), &QTextDocument::redoAvailable,
            redoToolButton_, &QToolButton::setEnabled);
        connect(whitespaceToolButton_, &QToolButton::toggled, this, [this](const bool checked)
            {
                if (editor_)
                {
                    editor_->setWhitespaceVisible(checked);
                }
            });
        connect(formatToolButton_, &QToolButton::clicked, this, [this]()
            {
                applyFormatting();
            });

        // All COG keyboard commands are MainWindow QActions. This keeps the
        // configurable keymap authoritative and lets one strict focus gate
        // prevent an open editor from stealing viewport shortcuts.

        connect(findEdit_, &QLineEdit::textChanged, this, [this]()
            {
                if (!suppressIncrementalFind_ && findRow_ && !findRow_->isHidden())
                {
                    findIncrementally();
                }
                else
                {
                    updateFindNavigationAvailability();
                    updateFindStatus(QString());
                }
            });
        connect(findPreviousButton_, &QToolButton::clicked, this, [this]()
            {
                findInEditor(false);
            });
        connect(findNextButton_, &QToolButton::clicked, this, [this]()
            {
                findInEditor(true);
            });
        connect(findCloseButton_, &QToolButton::clicked, this, [this]()
            {
                if (findRow_)
                {
                    findRow_->hide();
                    updateFindAccessoryChrome();
                }
                clearFindMatchSelections();
                updateFindStatus(QString());
                if (editor_)
                {
                    editor_->setFocus(Qt::OtherFocusReason);
                }
                if (onStateChanged_) onStateChanged_();
            });
        derivedStateTimer_ = new QTimer(this);
        documentHistory_ = new ui::cog::CogDocumentHistory(editor_, [this]()
        {
            documentDerivedStateDirty_ = true;
            identifierReferencesDirty_ = true;
            rebuildDerivedStateNow();
            if ( highlighter_ )
            {
                highlighter_->refreshSemanticNamesNow();
            }
            const auto changed = onStateChanged_;
            if ( changed )
            {
                changed();
            }
        });
        derivedStateTimer_->setObjectName(QStringLiteral(
            "cogEditorDerivedStateTimer"));
        derivedStateTimer_->setSingleShot(true);
        derivedStateTimer_->setInterval(kCogEditorDerivedStateDebounceMs);
        connect(derivedStateTimer_, &QTimer::timeout, this, [this]()
            {
                rebuildDerivedStateNow();
            });
        connect(editor_->document(), &QTextDocument::contentsChange, this,
            [this](const int, const int removed, const int inserted)
            {
                // Formatting/highlighting notifications carry no source bytes.
                // Real text edits, including native Undo/Redo, stale a queued offer.
                if ( removed != 0 || inserted != 0 )
                {
                    ++looseWatchAuthoredRevision_;
                }
            });
        connect(editor_, &QPlainTextEdit::textChanged, this, [this]()
            {
                if (highlighter_ && editor_)
                {
                    highlighter_->noteDocumentEdit(
                        editor_->textCursor().blockNumber(),
                        reloadingExternal_);
                }
                // Outline and diagnostics both scan the complete document.
                // Batch rapid typing and whole-document formatting into one
                // derived-state pass, while source editing/highlighting stays
                // immediately responsive.
                clearIdentifierReferenceSelections();
                updateFindNavigationAvailability();
                documentDerivedStateDirty_ = true;
                identifierReferencesDirty_ = true;
                scheduleDerivedStateRebuild();
                // Text edits are authored COG content and need the short
                // recovery debounce.  Cursor/scroll/find/layout changes use
                // onStateChanged_ below and remain editor-only.
                if (!reloadingExternal_ && onAuthoredBufferChanged_)
                {
                    onAuthoredBufferChanged_();
                }
                if (onStateChanged_) onStateChanged_();
            });
        connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, [this]()
            {
                if (onStateChanged_) onStateChanged_();
            });
        connect(editor_, &QPlainTextEdit::selectionChanged, this, [this]()
            {
                // Clear the old identifier immediately so a selection change
                // cannot leave a stale reference tint on screen. The full
                // source snapshot and occurrence scan are coalesced with the
                // other derived editor state after rapid cursor movement.
                clearIdentifierReferenceSelections();
                identifierReferencesDirty_ = true;
                scheduleDerivedStateRebuild();
                if (findRow_ && !findRow_->isHidden()
                    && findEdit_ && !findEdit_->text().isEmpty())
                {
                    // Clicking an already-highlighted result makes it the
                    // current X-of-N entry without changing the query.
                    updateFindNavigationAvailability();
                }
            });
        connect(editor_->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int)
            {
                if (onStateChanged_) onStateChanged_();
            });
        connect(editor_->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int)
            {
                if (onStateChanged_) onStateChanged_();
            });
        connect(whitespaceToolButton_, &QToolButton::toggled, this, [this](bool)
            {
                if (onStateChanged_) onStateChanged_();
            });
        connect(findEdit_, &QLineEdit::textChanged, this, [this](const QString&)
            {
                if (onStateChanged_) onStateChanged_();
            });
        connect(findWholeWordCheck_, &QCheckBox::toggled, this, [this](bool)
            {
                if (!suppressIncrementalFind_ && findRow_ && !findRow_->isHidden())
                {
                    findIncrementally();
                }
                else
                {
                    updateFindNavigationAvailability();
                }
                if (onStateChanged_) onStateChanged_();
            });
        connect(findCaseSensitiveCheck_, &QCheckBox::toggled, this, [this](bool)
            {
                if (!suppressIncrementalFind_ && findRow_ && !findRow_->isHidden())
                {
                    findIncrementally();
                }
                else
                {
                    updateFindNavigationAvailability();
                }
                if (onStateChanged_) onStateChanged_();
            });
        updateFindNavigationAvailability();
        connect(editor_->document(), &QTextDocument::modificationChanged, this, [this](const bool)
            {
                updateWindowTitle();
                if (onStateChanged_) onStateChanged_();
            });
        const auto connectOutlineList = [this](QListWidget* list)
        {
            connect(list, &QListWidget::itemActivated, this,
                [this](QListWidgetItem* item) { jumpToOutlineItem(item); });
            connect(list, &QListWidget::itemClicked, this,
                [this](QListWidgetItem* item) { jumpToOutlineItem(item); });
        };
        connectOutlineList(symbolOutlineList_);
        connectOutlineList(codeOutlineList_);
        connect(issuesList_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item)
            {
                jumpToDiagnosticItem(item);
            });
        connect(issuesList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item)
            {
                jumpToDiagnosticItem(item);
            });

        editor_->installEventFilter(this);
        editor_->viewport()->installEventFilter(this);
        editor_->viewport()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(editor_->viewport(), &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
            {
                showEditorContextMenu(
                    pos,
                    editor_ ? editor_->viewport()->mapToGlobal(pos) : QPoint{});
            });
        whitespaceToolButton_->setChecked(showWhitespaceDefault);
        documentDerivedStateDirty_ = true;
        identifierReferencesDirty_ = true;
        scheduleDerivedStateRebuild();
        // CogScriptSyntaxHighlighter::applyTheme() synchronously formats the
        // initial document. A second complete rehighlight here was redundant
        // and expensive for symbol-heavy cutscene scripts.
    }

    void bindLooseSourceWatch(
        const QByteArray& acceptedFingerprint,
        std::function<bool()> stillCurrent)
    {
        looseWatchStillCurrent_ = std::move(stillCurrent);
        if ( !acceptedFingerprint.isEmpty() )
        {
            acceptedSourceHash_ = acceptedFingerprint.size() == 32
                ? acceptedFingerprint : QByteArray{};
        }
        ensureWatchPaths(acceptedSourceHash_);
    }

    bool triggerSave()
    {
        return saveScript();
    }

    void markAsUnsavedDraft(
        DraftSavePathResolver savePathResolver,
        ScriptPathChangedCallback pathChanged)
    {
        ui::stopLooseDocumentWatch(this);
        draft_ = true;
        acceptedSourceHash_.clear();
        draftSavePathResolver_ = std::move(savePathResolver);
        scriptPathChanged_ = std::move(pathChanged);
        if (editor_ && editor_->document())
        {
            editor_->document()->setModified(true);
            documentHistory_->reset(true);
        }
        updateWindowTitle();
    }

    void triggerFormat()
    {
        applyFormatting();
    }

    void showFindRow(const bool focusEdit)
    {
        if (!findRow_ || !findEdit_)
        {
            return;
        }

        if (editor_)
        {
            const QTextCursor sourceCursor = editor_->textCursor();
            findAnchorPosition_ = sourceCursor.hasSelection()
                ? sourceCursor.selectionStart()
                : sourceCursor.position();
            const QString selected = sourceCursor.selectedText();
            if (!selected.trimmed().isEmpty())
            {
                const QSignalBlocker blocker(findEdit_);
                findEdit_->setText(selected);
            }
        }
        findRow_->show();
        updateFindAccessoryChrome();
        updateFindNavigationAvailability();
        if (onStateChanged_) onStateChanged_();
        if (focusEdit)
        {
            findEdit_->setFocus(Qt::ShortcutFocusReason);
            findEdit_->selectAll();
        }
    }

    void triggerFindFocus()
    {
        showFindRow(true);
    }

    void triggerFindNext()
    {
        findInEditor(true);
    }

    void triggerFindPrevious()
    {
        findInEditor(false);
    }

    void triggerToggleWhitespace()
    {
        if (whitespaceToolButton_)
        {
            whitespaceToolButton_->toggle();
        }
    }

    void triggerJumpToDefinition()
    {
        jumpToDefinitionFromCursor();
    }

    void triggerCompletion()
    {
        if (editor_)
        {
            editor_->triggerCompletion();
        }
    }

    bool contextInsertAvailable() const
    {
        QWidget* const focus = QApplication::focusWidget();
        if ( !editor_ || editor_->isReadOnly()
            || (focus != editor_ && focus != editor_->viewport()) )
        {
            return false;
        }
        const CogScriptInsertContext context = shortcutInsertContext();
        return context == CogScriptInsertContext::symbols
            || context == CogScriptInsertContext::code;
    }

    void triggerContextInsert()
    {
        if ( !contextInsertAvailable() )
        {
            return;
        }
        if ( shortcutInsertContext() == CogScriptInsertContext::symbols )
        {
            triggerAddSymbol();
            return;
        }
        showKeyboardInsertMenu(KeyboardInsertMenu::codeBoundary);
    }

    void triggerAddSymbol()
    {
        showKeyboardInsertMenu(KeyboardInsertMenu::symbol);
    }

    void triggerAddMessage()
    {
        showKeyboardInsertMenu(KeyboardInsertMenu::message);
    }

    void triggerAddCodeLabel()
    {
        if ( editor_ && !editor_->isReadOnly() )
        {
            insertGenericLabel(currentEditorCursorLine());
        }
    }

    void triggerEditFlags()
    {
        if ( editor_ && !editor_->isReadOnly() )
        {
            editCogScriptFlags(currentEditorCursorLine());
        }
    }

    bool triggerReloadFromDisk()
    {
        return reloadFromDisk(false, true);
    }

    void offerReloadIfDiskIsNewerThanCheckpoint(
        const qint64 checkpointTimestampMs)
    {
        if (draft_ || path_.empty() || checkpointTimestampMs <= 0)
        {
            return;
        }

        const QString sourcePath = QString::fromStdWString(path_.wstring());
        const QFileInfo sourceInfo(sourcePath);
        if (!sourceInfo.isFile()
            || sourceInfo.lastModified().toMSecsSinceEpoch()
                <= checkpointTimestampMs)
        {
            return;
        }

        QTimer::singleShot(0, this,
            [this, checkpointTimestampMs]()
            {
                const QFileInfo currentInfo(
                    QString::fromStdWString(path_.wstring()));
                if (draft_ || !currentInfo.isFile()
                    || currentInfo.lastModified().toMSecsSinceEpoch()
                        <= checkpointTimestampMs)
                {
                    return;
                }
                ui::requestLooseDocumentReload(this);
            });
    }

    void reloadAppearanceFromSettings()
    {
        if (!editor_)
        {
            return;
        }

        QSettings settings = settingsStore();
        const CogScriptTheme theme = resolveCogScriptTheme(settings);
        const QFont configuredFont = resolveCogScriptEditorFont(settings);
        const bool themeChanged = theme != currentTheme_;
        const bool fontChanged = configuredFont != baseEditorFont_;
        const bool showWhitespace = settings.value(
            "cog_editor/show_whitespace", false).toBool();
        baseEditorFont_ = configuredFont;
        if (fontChanged)
        {
            // Font preview/cancel restores only the presentation font. The
            // current zoom remains authoritative and an unchanged theme does
            // not trigger a full-document syntax pass.
            applyEditorZoomPercent(zoomPercent_, false);
        }
        if (themeChanged)
        {
            currentTheme_ = theme;
            editor_->applyTheme(theme);
            if (highlighter_)
            {
                highlighter_->applyTheme(theme);
            }
            clearIdentifierReferenceSelections();
            identifierReferencesDirty_ = true;
            scheduleDerivedStateRebuild();
            updateFindNavigationAvailability();
        }
        if (whitespaceToolButton_)
        {
            const QSignalBlocker blocker(whitespaceToolButton_);
            whitespaceToolButton_->setChecked(showWhitespace);
        }
        editor_->setWhitespaceVisible(showWhitespace);
        formatUseTabs_ = settings.value("cog_editor/format_use_tabs", formatUseTabs_).toBool();
        formatIndentWidth_ = std::clamp(
            settings.value("cog_editor/format_indent_width", formatIndentWidth_).toInt(), 1, 8);
        formatBraceStyle_ = ui::cog::braceStyleFromId(
            settings.value("cog_editor/format_brace_style", ui::cog::braceStyleId(formatBraceStyle_)).toString());
        formatIndentSections_ = settings.value("cog_editor/format_indent_sections", formatIndentSections_).toBool();
        formatIndentCodeBodies_ = settings.value("cog_editor/format_indent_code_bodies", formatIndentCodeBodies_).toBool();
        formatKeywordSpacing_ = settings.value("cog_editor/format_keyword_spacing", formatKeywordSpacing_).toBool();
        formatInsideParenthesesSpacing_ = settings.value(
            "cog_editor/format_inside_parentheses_spacing", formatInsideParenthesesSpacing_).toBool();
        formatFunctionParenthesesSpacing_ = settings.value(
            "cog_editor/format_function_parentheses_spacing",
            formatFunctionParenthesesSpacing_).toBool();
        formatForSemicolonSpacing_ = settings.value(
            "cog_editor/format_for_semicolon_spacing", formatForSemicolonSpacing_).toBool();
        formatAlignFinalReturnLabel_ = settings.value(
            "cog_editor/format_align_final_return_label", formatAlignFinalReturnLabel_).toBool();
        formatTrimExcessBlankLines_ = settings.value(
            "cog_editor/format_trim_excess_blank_lines", formatTrimExcessBlankLines_).toBool();
        formatIndentComments_ = settings.value(
            "cog_editor/format_indent_comments", formatIndentComments_).toBool();
        formatOnSave_ = settings.value("cog_editor/format_on_save", formatOnSave_).toBool();
        editor_->setIndentationStyle(formatUseTabs_, formatIndentWidth_);
    }

    void previewBaseEditorFont(const QFont& font)
    {
        baseEditorFont_ = font;
        applyEditorZoomPercent(zoomPercent_, false);
    }

    void setTitleChangedCallback(TitleChangedCallback callback)
    {
        titleChanged_ = std::move(callback);
        updateWindowTitle();
    }

    void setInlineAssignmentResourcePickerCallback(
        CogScriptCodeEditor::InlineResourcePickerCallback resourcePicker)
    {
        if (editor_)
        {
            editor_->setInlineAssignmentCallbacks(
                std::move(resourcePicker),
                [this]()
                {
                    editCogScriptFlags(
                        std::nullopt,
                        editor_ ? std::optional<QPoint>(
                                      editor_->inlineAssignmentActivationAnchor())
                                : std::nullopt);
                },
                [this](
                    const ui::cog::CogScriptInlineAssignmentSpan& span,
                    const int blockNumber,
                    const int sourceRevision)
                {
                    editCogScriptMask(
                        span,
                        blockNumber,
                        sourceRevision,
                        editor_ ? editor_->inlineAssignmentActivationAnchor()
                                : defaultCogPopupAnchor());
                });
        }
    }

    bool confirmPendingClose(bool* discarded = nullptr)
    {
        QPointer<CogScriptEditorDialog> self(this);
        const bool accepted = confirmClose();
        if (discarded)
        {
            *discarded = self && accepted && self->lastCloseDiscarded_;
        }
        return self && accepted;
    }

    bool hasModifiedContents() const
    {
        return editor_ && editor_->document() && editor_->document()->isModified();
    }

    bool canUndoRedo(const bool redo) const
    {
        return editor_ && (redo ? editor_->document()->isRedoAvailable()
            : editor_->document()->isUndoAvailable());
    }

    bool triggerUndoRedo(const bool redo)
    {
        if ( !editor_ )
        {
            return false;
        }
        if ( redo )
        {
            editor_->redo();
        }
        else
        {
            editor_->undo();
        }
        return true;
    }

    const QString& scriptName() const noexcept { return scriptName_; }
    const std::filesystem::path& scriptPath() const noexcept { return path_; }

    QCborMap recoveryState(const bool includeHistory = true) const
    {
        QCborMap state{};
        state.insert(QStringLiteral("name"), scriptName_);
        state.insert(QStringLiteral("path"), QString::fromStdWString(path_.wstring()));
        state.insert(QStringLiteral("text"), editor_ ? editor_->toPlainText() : QString{});
        state.insert(QStringLiteral("modified"), hasModifiedContents());
        state.insert(QStringLiteral("draft"), draft_);
        state.insert(QStringLiteral("acceptedSourceHash"), acceptedSourceHash_);
        // A reveal action is safe only for an authored loose file.  Bundled
        // resources and recovery/draft buffers are deliberately not exposed
        // as filesystem locations even when their working copy is extracted.
        state.insert(QStringLiteral("loose"), property("cogLooseFile").toBool());
        const QString archiveTooltip = property("cogArchiveTooltip").toString();
        if (!archiveTooltip.isEmpty())
        {
            state.insert(QStringLiteral("archiveTooltip"), archiveTooltip);
        }
        state.insert(QStringLiteral("pinned"), property("cogPinned").toBool());
        if (editor_)
        {
            const QTextCursor cursor = editor_->textCursor();
            state.insert(QStringLiteral("cursor"), cursor.position());
            state.insert(QStringLiteral("anchor"), cursor.anchor());
            state.insert(QStringLiteral("vscroll"), editor_->verticalScrollBar()->value());
            state.insert(QStringLiteral("hscroll"), editor_->horizontalScrollBar()->value());
        }
        state.insert(QStringLiteral("whitespace"),
            whitespaceToolButton_ && whitespaceToolButton_->isChecked());
        state.insert(QStringLiteral("zoom"), zoomPercent_);
        // QWidget::isVisible() is false for inactive tabs even when the Find
        // row itself is explicitly shown. Persist the row's own state.
        state.insert(QStringLiteral("findVisible"), findRow_ && !findRow_->isHidden());
        state.insert(QStringLiteral("findText"), findEdit_ ? findEdit_->text() : QString{});
        state.insert(QStringLiteral("findWhole"),
            findWholeWordCheck_ && findWholeWordCheck_->isChecked());
        state.insert(QStringLiteral("findCase"),
            findCaseSensitiveCheck_ && findCaseSensitiveCheck_->isChecked());
        QCborArray splitterSizes{};
        if (editorRow_)
        {
            for (const int size : editorRow_->sizes()) splitterSizes.append(size);
        }
        state.insert(QStringLiteral("splitter"), splitterSizes);
        QCborArray outlineSplitterSizes{};
        if (outlineSplitter_)
        {
            for (const int size : outlineSplitter_->sizes())
            {
                outlineSplitterSizes.append(size);
            }
        }
        state.insert(QStringLiteral("outlineSplitter"), outlineSplitterSizes);
        state.insert(QStringLiteral("diagnosticsExpanded"),
            diagnosticsCommittedExpanded_);
        state.insert(QStringLiteral("diagnosticsExpandedHeight"),
            std::max(80, diagnosticsExpandedHeight_));
        QCborArray diagnosticsSplitterSizes{};
        if (editorColumn_)
        {
            const QList<int> sizes = editorColumn_->sizes();
            if (diagnosticsAnimation_ && sizes.size() == 2)
            {
                const int total = sizes.at(0) + sizes.at(1);
                const int headerHeight = diagnosticsHeader_
                    ? std::max(24, diagnosticsHeader_->sizeHint().height())
                    : 24;
                const int committedHeight = diagnosticsCommittedExpanded_
                    ? std::clamp(diagnosticsExpandedHeight_, 80, std::max(80, total / 2))
                    : headerHeight;
                diagnosticsSplitterSizes.append(std::max(1, total - committedHeight));
                diagnosticsSplitterSizes.append(committedHeight);
            }
            else
            {
                for (const int size : sizes) diagnosticsSplitterSizes.append(size);
            }
        }
        state.insert(QStringLiteral("diagnosticsSplitter"), diagnosticsSplitterSizes);
        if ( includeHistory )
        {
            state.insert(QStringLiteral("history"), documentHistory_->capture());
        }
        return state;
    }

    static bool recoveryStateIsValid(const QCborMap& state)
    {
        const QCborValue name = state.value(QStringLiteral("name"));
        const QCborValue path = state.value(QStringLiteral("path"));
        const QCborValue text = state.value(QStringLiteral("text"));
        const QCborValue cursor = state.value(QStringLiteral("cursor"));
        const QCborValue anchor = state.value(QStringLiteral("anchor"));
        const QCborValue vertical = state.value(QStringLiteral("vscroll"));
        const QCborValue horizontal = state.value(QStringLiteral("hscroll"));
        const QCborValue zoom = state.value(QStringLiteral("zoom"));
        const QCborValue splitter = state.value(QStringLiteral("splitter"));
        const QCborValue outlineSplitter = state.value(
            QStringLiteral("outlineSplitter"));
        const QCborValue loose = state.value(QStringLiteral("loose"));
        const QCborValue archiveTooltip = state.value(QStringLiteral("archiveTooltip"));
        const QCborValue acceptedSourceHash = state.value(QStringLiteral("acceptedSourceHash"));
        const QCborValue pinned = state.value(QStringLiteral("pinned"));
        const QCborValue findCase = state.value(QStringLiteral("findCase"));
        const QCborValue diagnosticsExpanded = state.value(QStringLiteral("diagnosticsExpanded"));
        const QCborValue diagnosticsExpandedHeight = state.value(QStringLiteral("diagnosticsExpandedHeight"));
        const QCborValue diagnosticsSplitter = state.value(QStringLiteral("diagnosticsSplitter"));
        const auto safeNonNegativeInt = [](const QCborValue& value)
        {
            return value.isInteger() && value.toInteger() >= 0
                && value.toInteger() <= std::numeric_limits<int>::max();
        };
        if (!name.isString() || name.toString().trimmed().isEmpty()
            || name.toString().size() > 4'096
            || !path.isString() || path.toString().size() > 32'768
            || !text.isString() || text.toString().size() > 64 * 1024 * 1024
            || !state.value(QStringLiteral("modified")).isBool()
            || (!state.value(QStringLiteral("draft")).isUndefined()
                && !state.value(QStringLiteral("draft")).isBool())
            || (!acceptedSourceHash.isUndefined()
                && (!acceptedSourceHash.isByteArray()
                    || (acceptedSourceHash.toByteArray().size() != 0
                        && acceptedSourceHash.toByteArray().size() != 32)))
            || !safeNonNegativeInt(cursor) || cursor.toInteger() > text.toString().size()
            || !safeNonNegativeInt(anchor) || anchor.toInteger() > text.toString().size()
            || !safeNonNegativeInt(vertical) || !safeNonNegativeInt(horizontal)
            || !state.value(QStringLiteral("whitespace")).isBool()
            || (!zoom.isUndefined()
                && (!zoom.isInteger() || zoom.toInteger() < 20
                    || zoom.toInteger() > 400))
            || !state.value(QStringLiteral("findVisible")).isBool()
            || (!state.value(QStringLiteral("findText")).isUndefined()
                && (!state.value(QStringLiteral("findText")).isString()
                    || state.value(QStringLiteral("findText")).toString().size() > 1'000'000))
            || !state.value(QStringLiteral("findWhole")).isBool()
            || (!findCase.isUndefined() && !findCase.isBool())
            || (!loose.isUndefined() && !loose.isBool())
            || (!archiveTooltip.isUndefined()
                && (!archiveTooltip.isString()
                    || archiveTooltip.toString().size() > 32768
                    || archiveTooltip.toString().contains(QChar::Null)
                    || (!archiveTooltip.toString().isEmpty()
                        && (!loose.isBool() || loose.toBool()))))
            || (!pinned.isUndefined() && !pinned.isBool())
            || (!diagnosticsExpanded.isUndefined() && !diagnosticsExpanded.isBool())
            || (!diagnosticsExpandedHeight.isUndefined()
                && (!safeNonNegativeInt(diagnosticsExpandedHeight)
                    || diagnosticsExpandedHeight.toInteger() > 1'000'000))
            || (!splitter.isUndefined() && !splitter.isArray())
            || (!outlineSplitter.isUndefined() && !outlineSplitter.isArray())
            || (!diagnosticsSplitter.isUndefined() && !diagnosticsSplitter.isArray()))
        {
            return false;
        }
        if (splitter.isArray())
        {
            const QCborArray sizes = splitter.toArray();
            if (sizes.size() > 32)
            {
                return false;
            }
            for (const QCborValue& size : sizes)
            {
                if (!size.isInteger() || size.toInteger() < 0
                    || size.toInteger() > 1'000'000)
                {
                    return false;
                }
            }
        }
        if (outlineSplitter.isArray())
        {
            const QCborArray sizes = outlineSplitter.toArray();
            if (sizes.size() > 32)
            {
                return false;
            }
            for (const QCborValue& size : sizes)
            {
                if (!size.isInteger() || size.toInteger() < 0
                    || size.toInteger() > 1'000'000)
                {
                    return false;
                }
            }
        }
        if (diagnosticsSplitter.isArray())
        {
            const QCborArray sizes = diagnosticsSplitter.toArray();
            if (sizes.size() > 32)
            {
                return false;
            }
            for (const QCborValue& size : sizes)
            {
                if (!size.isInteger() || size.toInteger() < 0
                    || size.toInteger() > 1'000'000)
                {
                    return false;
                }
            }
        }
        const QCborValue history = state.value(QStringLiteral("history"));
        return history.isUndefined() || (history.isMap() &&
            ui::cog::CogDocumentHistory::validate(history.toMap(), text.toString(),
                state.value(QStringLiteral("modified")).toBool()));
    }

    bool restoreRecoveryState(const QCborMap& state)
    {
        const QCborValue cursorValue = state.value(QStringLiteral("cursor"));
        const QCborValue anchorValue = state.value(QStringLiteral("anchor"));
        const QCborValue verticalValue = state.value(QStringLiteral("vscroll"));
        const QCborValue horizontalValue = state.value(QStringLiteral("hscroll"));
        if (!editor_ || !recoveryStateIsValid(state))
        {
            return false;
        }
        acceptedSourceHash_ = draft_
            ? QByteArray{}
            : state.value(QStringLiteral("acceptedSourceHash")).toByteArray();
        {
            const QSignalBlocker blocker(editor_->document());
            documentHistory_->restore(state.value(QStringLiteral("history")).toMap(),
                state.value(QStringLiteral("text")).toString(), state.value(QStringLiteral("modified")).toBool());
        }
        QTextCursor cursor(editor_->document());
        const int maximum = std::max(0, editor_->document()->characterCount() - 1);
        const int anchor = std::clamp(
            static_cast<int>(anchorValue.toInteger()), 0, maximum);
        const int position = std::clamp(
            static_cast<int>(cursorValue.toInteger()), 0, maximum);
        cursor.setPosition(anchor);
        cursor.setPosition(position, QTextCursor::KeepAnchor);
        editor_->setTextCursor(cursor);
        editor_->verticalScrollBar()->setValue(
            static_cast<int>(verticalValue.toInteger()));
        editor_->horizontalScrollBar()->setValue(
            static_cast<int>(horizontalValue.toInteger()));
        if (whitespaceToolButton_)
            whitespaceToolButton_->setChecked(state.value(QStringLiteral("whitespace")).toBool());
        const QCborValue zoomValue = state.value(QStringLiteral("zoom"));
        applyEditorZoomPercent(
            zoomValue.isInteger()
                ? static_cast<int>(zoomValue.toInteger())
                : 100,
            false);
        suppressIncrementalFind_ = true;
        if (findEdit_) findEdit_->setText(state.value(QStringLiteral("findText")).toString());
        if (findWholeWordCheck_)
            findWholeWordCheck_->setChecked(state.value(QStringLiteral("findWhole")).toBool());
        if (findCaseSensitiveCheck_)
            findCaseSensitiveCheck_->setChecked(
                state.value(QStringLiteral("findCase")).toBool(false));
        if (findRow_)
        {
            findRow_->setVisible(state.value(QStringLiteral("findVisible")).toBool());
            updateFindAccessoryChrome();
        }
        suppressIncrementalFind_ = false;
        findAnchorPosition_ = editor_ ? editor_->textCursor().selectionStart() : 0;
        updateFindNavigationAvailability();
        setProperty("cogPinned", state.value(QStringLiteral("pinned")).toBool(false));
        setProperty("cogArchiveTooltip", state.value(QStringLiteral("archiveTooltip")).toString());
        if (state.value(QStringLiteral("loose")).isBool())
        {
            setProperty("cogLooseFile", state.value(QStringLiteral("loose")).toBool());
        }
        documentDerivedStateDirty_ = true;
        identifierReferencesDirty_ = true;
        rebuildDerivedStateNow();
        // Document signals are deliberately blocked while recovery applies a
        // complete buffer. Rehighlight synchronously so a just-restored SED
        // or recovery tab never displays stale semantic colors while waiting
        // for an editor textChanged notification that was intentionally muted.
        if (highlighter_)
        {
            highlighter_->refreshSemanticNamesNow();
        }
        // Rebuilding the outline may show/hide its pane and causes QSplitter
        // to normalize sizes. Apply the recovered allocation afterwards so
        // it remains authoritative instead of being immediately overwritten.
        const QCborArray splitterSizes = state.value(QStringLiteral("splitter")).toArray();
        if (editorRow_ && splitterSizes.size() == editorRow_->count())
        {
            QList<int> sizes{};
            bool valid = true;
            for (const QCborValue& size : splitterSizes)
            {
                if (!size.isInteger() || size.toInteger() < 0
                    || size.toInteger() > 1'000'000)
                {
                    valid = false;
                    break;
                }
                sizes.push_back(static_cast<int>(size.toInteger()));
            }
            if (valid) editorRow_->setSizes(sizes);
        }
        const QCborArray outlineSplitterSizes = state.value(
            QStringLiteral("outlineSplitter")).toArray();
        if (outlineSplitter_
            && outlineSplitterSizes.size() == outlineSplitter_->count())
        {
            QList<int> sizes{};
            for (const QCborValue& size : outlineSplitterSizes)
            {
                sizes.push_back(static_cast<int>(size.toInteger()));
            }
            outlineSplitter_->setSizes(sizes);
        }
        const bool diagnosticsExpanded =
            state.value(QStringLiteral("diagnosticsExpanded")).toBool(true);
        const QCborValue expandedHeightValue =
            state.value(QStringLiteral("diagnosticsExpandedHeight"));
        const int recoveredExpandedHeight = expandedHeightValue.isInteger()
            ? std::clamp(static_cast<int>(expandedHeightValue.toInteger()), 80, 1'000'000)
            : diagnosticsExpandedHeight_;
        setDiagnosticsExpanded(diagnosticsExpanded, false);
        if (diagnosticsExpanded)
        {
            const QCborArray sizes = state.value(QStringLiteral("diagnosticsSplitter")).toArray();
            if (editorColumn_ && sizes.size() == editorColumn_->count())
            {
                QList<int> restoredSizes{};
                for (const QCborValue& size : sizes)
                {
                    restoredSizes.push_back(static_cast<int>(size.toInteger()));
                }
                editorColumn_->setSizes(restoredSizes);
                const QList<int> applied = editorColumn_->sizes();
                diagnosticsExpandedHeight_ = applied.size() == 2
                    ? std::max(80, applied.at(1))
                    : recoveredExpandedHeight;
            }
            else
            {
                diagnosticsExpandedHeight_ = recoveredExpandedHeight;
                setDiagnosticsExpanded(true, false);
            }
        }
        else
        {
            diagnosticsExpandedHeight_ = recoveredExpandedHeight;
        }
        const QPointer<CogScriptEditorDialog> self(this);
        updateWindowTitle();
        if ( self )
        {
            documentHistory_->publishAvailability();
        }
        return true;
    }

protected:
    void closeEvent(QCloseEvent* event) override
    {
        if (!confirmClose())
        {
            event->ignore();
            return;
        }
        QWidget::closeEvent(event);
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (findRow_ && watched == findRow_ && event
            && (event->type() == QEvent::Show || event->type() == QEvent::Hide))
        {
            updateFindAccessoryChrome();
        }
        if (findEdit_ && watched == findEdit_ && event
            && (event->type() == QEvent::Resize
                || event->type() == QEvent::Show
                || event->type() == QEvent::Polish
                || event->type() == QEvent::LayoutRequest))
        {
            positionFindClearButton();
            QTimer::singleShot(0, this, [this]()
                {
                    positionFindClearButton();
                });
        }
        if (editor_ && watched == editor_ && event
            && (event->type() == QEvent::Show
                || event->type() == QEvent::LayoutRequest))
        {
            repositionEditorAccessoryRow();
            // Show/layout requests can also change native scrollbars without
            // resizing the editor. Keep a queued fallback for those paths;
            // Resize itself is handled synchronously by the editor's
            // post-layout callback.
            QTimer::singleShot(0, this, [this]()
                {
                    repositionEditorAccessoryRow();
                });
        }
        if (editor_ && event
            && (watched == editor_->horizontalScrollBar()
                || watched == editor_->verticalScrollBar())
            && (event->type() == QEvent::Resize
                || event->type() == QEvent::Show
                || event->type() == QEvent::Hide
                || event->type() == QEvent::LayoutRequest))
        {
            repositionEditorAccessoryRow();
            QTimer::singleShot(0, this, [this]()
                {
                    repositionEditorAccessoryRow();
                });
        }

        if (zoomCombo_ && event && event->type() == QEvent::Wheel
            && (watched == zoomCombo_ || watched == zoomCombo_->lineEdit()))
        {
            auto* wheel = static_cast<QWheelEvent*>(event);
            QWidget* focus = QApplication::focusWidget();
            const bool ownsFocus = focus
                && (focus == zoomCombo_ || zoomCombo_->isAncestorOf(focus));
            if (!ownsFocus)
            {
                // Hovering the zoom control must never steal scroll input or
                // silently alter a tab. The application policy forwards the
                // event to an enclosing scroll area when one exists.
                wheel->accept();
                return true;
            }

            const int delta = wheel->angleDelta().y() != 0
                ? wheel->angleDelta().y()
                : wheel->pixelDelta().y();
            if (delta != 0)
            {
                static constexpr std::array<int, 7> kZoomSteps{
                    20, 50, 70, 100, 150, 200, 400,
                };
                int next = zoomPercent_;
                if (delta > 0)
                {
                    for (const int candidate : kZoomSteps)
                    {
                        if (candidate > zoomPercent_)
                        {
                            next = candidate;
                            break;
                        }
                    }
                }
                else
                {
                    for (auto it = kZoomSteps.rbegin(); it != kZoomSteps.rend(); ++it)
                    {
                        if (*it < zoomPercent_)
                        {
                            next = *it;
                            break;
                        }
                    }
                }
                applyEditorZoomPercent(next, true);
            }
            wheel->accept();
            return true;
        }

        if (findEdit_ && watched == findEdit_ && event
            && event->type() == QEvent::KeyPress)
        {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return
                || keyEvent->key() == Qt::Key_Enter)
            {
                // The Find row is intentionally embedded in the source
                // editor's reserved bottom chrome. Consume Return here after
                // searching so the key cannot bubble to QPlainTextEdit and
                // replace the found token with a newline.
                findInEditor(true);
                keyEvent->accept();
                return true;
            }
        }

        if (editor_ && watched == editor_->viewport() && event)
        {
            if (event->type() == QEvent::Wheel)
            {
                auto* wheel = static_cast<QWheelEvent*>(event);
                if (wheel->modifiers().testFlag(Qt::ControlModifier))
                {
                    const int delta = wheel->angleDelta().y() != 0
                        ? wheel->angleDelta().y()
                        : wheel->pixelDelta().y();
                    if (delta != 0)
                    {
                        applyEditorZoomPercent(
                            zoomPercent_ + (delta > 0 ? 10 : -10),
                            true);
                    }
                    wheel->accept();
                    return true;
                }
            }
            if (event->type() == QEvent::ContextMenu)
            {
                auto* contextEvent = static_cast<QContextMenuEvent*>(event);
                showEditorContextMenu(
                    contextEvent->pos(), contextEvent->globalPos());
                return true;
            }
            if (event->type() == QEvent::MouseButtonPress)
            {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton && mouseEvent->modifiers().testFlag(Qt::ControlModifier))
                {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                    const QPoint editorPosition = mouseEvent->position().toPoint();
#else
                    const QPoint editorPosition = mouseEvent->pos();
#endif
                    jumpToDefinitionAtPosition(editorPosition);
                    return true;
                }
            }
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    enum class KeyboardInsertMenu
    {
        symbol,
        message,
        codeBoundary,
    };

    CogScriptInsertContext shortcutInsertContext() const
    {
        if ( !editor_ || !editor_->document() )
        {
            return CogScriptInsertContext::outside;
        }
        const int revision = editor_->document()->revision();
        if ( shortcutInsertContextsRevision_ != revision )
        {
            shortcutInsertContexts_ = ui::cog::buildCogScriptShortcutInsertContexts(
                editor_->toPlainText());
            shortcutInsertContextsRevision_ = revision;
        }
        const int line = editor_->textCursor().blockNumber();
        return line >= 0 && static_cast<std::size_t>(line) < shortcutInsertContexts_.size()
            ? shortcutInsertContexts_[static_cast<std::size_t>(line)]
            : CogScriptInsertContext::outside;
    }

    void showKeyboardInsertMenu(const KeyboardInsertMenu kind)
    {
        if ( !editor_ || editor_->isReadOnly() )
        {
            return;
        }
        const std::optional<int> lineHint = currentEditorCursorLine();
        const QTextCursor sourceCursor = editor_->textCursor();
        const int revision = editor_->document()->revision();
        QPointer<CogScriptEditorDialog> self(this);
        QPointer<CogScriptCodeEditor> source(editor_);
        // A parentless stack menu remains valid if the owning editor is closed
        // during its nested event loop. QObject disconnects its edit callbacks.
        ui::shortcuts::KeySequenceDisplayMenu menu{};
        menu.setObjectName(QStringLiteral("cogEditorContextInsertMenu"));
        if ( kind == KeyboardInsertMenu::symbol )
        {
            populateSymbolInsertMenu(&menu, lineHint);
        }
        else if ( kind == KeyboardInsertMenu::message )
        {
            populateHandlerInsertMenu(&menu, lineHint);
        }
        else
        {
            QMenu* const messages = ui::shortcuts::addKeySequenceDisplayMenu(
                &menu, QStringLiteral("Message"));
            populateHandlerInsertMenu(messages, lineHint, true);
            QAction* const label = menu.addAction(QStringLiteral("Code section..."));
            label->setToolTip(QStringLiteral("Insert a custom code label and matching symbol entry."));
            connect(label, &QAction::triggered, this, [this, lineHint, revision]()
                {
                    if ( editor_ && !editor_->isReadOnly()
                        && editor_->document()->revision() == revision )
                    {
                        insertGenericLabel(lineHint, true);
                    }
                });
        }
        const QRect caret = source->cursorRect(sourceCursor);
        menu.exec(source->viewport()->mapToGlobal(caret.bottomLeft()));
        if ( self && source && self->editor_ == source.data() )
        {
            // Accepted edits keep their insertion cursor; cancellation can
            // return source focus without changing the original selection.
            if ( source->document()->revision() == revision )
            {
                source->setTextCursor(sourceCursor);
            }
            source->setFocus(Qt::OtherFocusReason);
        }
    }

    void positionFindClearButton()
    {
        if (!findEdit_ || !findClearButton_)
        {
            return;
        }
        const QRect field = findEdit_->contentsRect();
        findClearButton_->move(
            field.right() - findClearButton_->width() - 1,
            field.top() + (field.height() - findClearButton_->height()) / 2);
        findClearButton_->raise();
    }

    void persistFormatOptions()
    {
        QSettings settings = settingsStore();
        settings.setValue("cog_editor/format_use_tabs", formatUseTabs_);
        settings.setValue("cog_editor/format_indent_width", formatIndentWidth_);
        settings.setValue("cog_editor/format_brace_style", ui::cog::braceStyleId(formatBraceStyle_));
        settings.setValue("cog_editor/format_indent_sections", formatIndentSections_);
        settings.setValue("cog_editor/format_indent_code_bodies", formatIndentCodeBodies_);
        settings.setValue("cog_editor/format_keyword_spacing", formatKeywordSpacing_);
        settings.setValue("cog_editor/format_inside_parentheses_spacing", formatInsideParenthesesSpacing_);
        settings.setValue(
            "cog_editor/format_function_parentheses_spacing",
            formatFunctionParenthesesSpacing_);
        settings.setValue("cog_editor/format_for_semicolon_spacing", formatForSemicolonSpacing_);
        settings.setValue("cog_editor/format_align_final_return_label", formatAlignFinalReturnLabel_);
        settings.setValue("cog_editor/format_trim_excess_blank_lines", formatTrimExcessBlankLines_);
        settings.setValue("cog_editor/format_indent_comments", formatIndentComments_);
        settings.setValue("cog_editor/format_on_save", formatOnSave_);
    }

    void populateFormatMenu(QMenu* menu)
    {
        if (!menu)
        {
            return;
        }

        menu->clear();
        QAction* formatNow = menu->addAction(
            ui::makeViewportIcon(ui::ViewportIcon::actionFormat),
            QStringLiteral("Format with current options"));
        formatNow->setObjectName(QStringLiteral("cogEditorFormatNowAction"));
        formatNow->setToolTip(QStringLiteral("Format this script immediately."));
        connect(formatNow, &QAction::triggered, this, [this]()
            {
                applyFormatting();
            });
        menu->addSeparator();

        const auto addToggle = [this, menu](
                                   const QString& text,
                                   const QString& objectName,
                                   const QString& tooltip,
                                   bool* value)
        {
            QAction* action = menu->addAction(text);
            action->setObjectName(objectName);
            action->setCheckable(true);
            action->setChecked(*value);
            action->setToolTip(tooltip);
            action->setStatusTip(tooltip);
            connect(action, &QAction::toggled, this, [this, value](const bool checked)
                {
                    *value = checked;
                    if (editor_)
                    {
                        editor_->setIndentationStyle(formatUseTabs_, formatIndentWidth_);
                    }
                    persistFormatOptions();
                });
            return action;
        };

        addToggle(
            QStringLiteral("Use tabs"),
            QStringLiteral("cogEditorFormatUseTabsAction"),
            QStringLiteral("Indent with tabs instead of spaces."),
            &formatUseTabs_);

        QMenu* indentWidthMenu = ui::shortcuts::addKeySequenceDisplayMenu(
            menu, QStringLiteral("Indent width"));
        indentWidthMenu->setObjectName(QStringLiteral("cogEditorFormatIndentWidthMenu"));
        auto* indentWidthGroup = new QActionGroup(indentWidthMenu);
        indentWidthGroup->setExclusive(true);
        for (const int width : std::array<int, 8>{ 1, 2, 3, 4, 5, 6, 7, 8 })
        {
            QAction* widthAction = indentWidthMenu->addAction(
                QStringLiteral("%1 %2").arg(width).arg(
                    width == 1 ? QStringLiteral("space") : QStringLiteral("spaces")));
            widthAction->setCheckable(true);
            widthAction->setChecked(formatIndentWidth_ == width);
            widthAction->setData(width);
            indentWidthGroup->addAction(widthAction);
        }
        connect(indentWidthGroup, &QActionGroup::triggered, this, [this](QAction* action)
            {
                if (!action)
                {
                    return;
                }
                formatIndentWidth_ = std::clamp(action->data().toInt(), 1, 8);
                if (editor_)
                {
                    editor_->setIndentationStyle(formatUseTabs_, formatIndentWidth_);
                }
                persistFormatOptions();
            });

        QMenu* braceStyleMenu = ui::shortcuts::addKeySequenceDisplayMenu(
            menu, QStringLiteral("Brace style"));
        braceStyleMenu->setObjectName(QStringLiteral("cogEditorFormatBraceStyleMenu"));
        auto* braceStyleGroup = new QActionGroup(braceStyleMenu);
        braceStyleGroup->setExclusive(true);
        const auto addBraceStyle = [this, braceStyleMenu, braceStyleGroup](
                                       const QString& text,
                                       const ui::cog::BraceStyle style)
        {
            QAction* action = braceStyleMenu->addAction(text);
            action->setCheckable(true);
            action->setChecked(formatBraceStyle_ == style);
            action->setData(ui::cog::braceStyleId(style));
            braceStyleGroup->addAction(action);
        };
        addBraceStyle(QStringLiteral("Same line"), ui::cog::BraceStyle::sameLine);
        addBraceStyle(QStringLiteral("Next line"), ui::cog::BraceStyle::nextLine);
        connect(braceStyleGroup, &QActionGroup::triggered, this, [this](QAction* action)
            {
                if (!action)
                {
                    return;
                }
                formatBraceStyle_ = ui::cog::braceStyleFromId(action->data().toString());
                persistFormatOptions();
            });

        menu->addSeparator();
        addToggle(
            QStringLiteral("Indent section bodies"),
            QStringLiteral("cogEditorFormatIndentSectionsAction"),
            QStringLiteral("Indent declarations and code inside section boundaries."),
            &formatIndentSections_);
        addToggle(
            QStringLiteral("Indent handler/label bodies"),
            QStringLiteral("cogEditorFormatIndentCodeBodiesAction"),
            QStringLiteral("Indent code beneath message handlers and custom labels."),
            &formatIndentCodeBodies_);
        addToggle(
            QStringLiteral("Space after control keywords"),
            QStringLiteral("cogEditorFormatKeywordSpacingAction"),
            QStringLiteral("Format if/while/for/switch as `if (condition)`."),
            &formatKeywordSpacing_);
        addToggle(
            QStringLiteral("Space inside condition/group parentheses"),
            QStringLiteral("cogEditorFormatGroupSpacingAction"),
            QStringLiteral("Format control conditions and grouped expressions as `( value )`."),
            &formatInsideParenthesesSpacing_);
        addToggle(
            QStringLiteral("Space inside function calls"),
            QStringLiteral("cogEditorFormatFunctionSpacingAction"),
            QStringLiteral("Format `Function(value)` as `Function( value )`."),
            &formatFunctionParenthesesSpacing_);
        addToggle(
            QStringLiteral("Space after for semicolons"),
            QStringLiteral("cogEditorFormatForSpacingAction"),
            QStringLiteral("Format for clauses as `for (a; b; c)`."),
            &formatForSemicolonSpacing_);
        addToggle(
            QStringLiteral("Align final return with handler label"),
            QStringLiteral("cogEditorFormatAlignReturnAction"),
            QStringLiteral("Align a handler's final return statement with its label."),
            &formatAlignFinalReturnLabel_);
        addToggle(
            QStringLiteral("Trim boundary and repeated blank lines"),
            QStringLiteral("cogEditorFormatTrimBlankLinesAction"),
            QStringLiteral("Remove boundary blanks and reduce repeated interior blanks to one."),
            &formatTrimExcessBlankLines_);
        addToggle(
            QStringLiteral("Indent standalone comments"),
            QStringLiteral("cogEditorFormatIndentCommentsAction"),
            QStringLiteral("Align comment-only lines with their surrounding scope."),
            &formatIndentComments_);
        menu->addSeparator();
        addToggle(
            QStringLiteral("Format automatically before saving"),
            QStringLiteral("cogEditorFormatOnSaveAction"),
            QStringLiteral("Apply these formatting rules before writing a COG file."),
            &formatOnSave_);
    }

    void updateFindAccessoryChrome()
    {
        if (!editorAccessoryRow_)
        {
            return;
        }

        const bool findVisible = findRow_ && !findRow_->isHidden();
        if (editorAccessoryRow_->property("sedFindVisible").toBool()
            == findVisible)
        {
            return;
        }

        editorAccessoryRow_->setProperty("sedFindVisible", findVisible);
        // Qt style sheets cache dynamic-property selector matches. Re-polish
        // immediately so the blue Find background/border disappears on the
        // same event as the controls instead of one layout pass later.
        if (QStyle* accessoryStyle = editorAccessoryRow_->style())
        {
            accessoryStyle->unpolish(editorAccessoryRow_);
            accessoryStyle->polish(editorAccessoryRow_);
        }
        editorAccessoryRow_->update();
    }

    void repositionEditorAccessoryRow()
    {
        if (!editor_ || !editorAccessoryRow_)
        {
            return;
        }

        // The code editor reserves a compact bottom viewport margin for this
        // row. Place the complete row inside that strip so neither source text
        // nor the horizontal scrollbar/Diagnostics header can be covered.
        const QRect viewportRect = editor_->viewport()->geometry();
        const int x = editor_->frameWidth() + 4;
        const int reservedTop = viewportRect.bottom() + 1;
        int bottomLimit = editor_->height() - editor_->frameWidth();
        QScrollBar* horizontalScrollBar = editor_->horizontalScrollBar();
        const QRect horizontalScrollRect(
            horizontalScrollBar->mapTo(editor_, QPoint{}),
            horizontalScrollBar->size());
        if (horizontalScrollBar->isVisible()
            && horizontalScrollRect.top() > reservedTop)
        {
            bottomLimit = horizontalScrollRect.top();
        }
        const int availableHeight = std::max(0, bottomLimit - reservedTop);
        const int y = reservedTop
            + std::max(0, (availableHeight - kCogEditorAccessoryHeight) / 2);

        int rightLimit = editor_->width() - editor_->frameWidth();
        QScrollBar* verticalScrollBar = editor_->verticalScrollBar();
        const QRect verticalScrollRect(
            verticalScrollBar->mapTo(editor_, QPoint{}),
            verticalScrollBar->size());
        if (verticalScrollBar->isVisible()
            && verticalScrollRect.left() > x)
        {
            rightLimit = std::min(
                rightLimit,
                verticalScrollRect.left());
        }
        editorAccessoryRow_->setGeometry(
            x,
            y,
            std::max(0, rightLimit - x - 4),
            kCogEditorAccessoryHeight);
        editorAccessoryRow_->raise();
    }

    void setZoomComboText(const int percent)
    {
        if (!zoomCombo_)
        {
            return;
        }
        const QString text = QStringLiteral("%1%").arg(
            clampCogScriptEditorZoomPercent(percent));
        const QSignalBlocker blocker(zoomCombo_);
        const int index = zoomCombo_->findText(text, Qt::MatchExactly);
        if (index >= 0)
        {
            zoomCombo_->setCurrentIndex(index);
        }
        else
        {
            zoomCombo_->setEditText(text);
        }
    }

    void applyZoomControlUiFont()
    {
        if (!zoomCombo_)
        {
            return;
        }

        // The combo lives in source-editor chrome for compact placement,
        // but it is application chrome rather than source text. Give the
        // combo, its editable field, and its popup an explicit UI font so a
        // 200-400% source zoom or Preferences font preview cannot enlarge and
        // clip the percentage inside the fixed-size control.
        zoomCombo_->setFont(zoomUiFont_);
        if (zoomCombo_->lineEdit())
        {
            zoomCombo_->lineEdit()->setFont(zoomUiFont_);
        }
        if (zoomCombo_->view())
        {
            zoomCombo_->view()->setFont(zoomUiFont_);
        }
    }

    void applyEditorZoomPercent(const int percent, const bool persist)
    {
        const int bounded = clampCogScriptEditorZoomPercent(percent);
        const bool changed = zoomPercent_ != bounded;
        zoomPercent_ = bounded;
        setZoomComboText(zoomPercent_);
        if (editor_)
        {
            editor_->setFont(cogScriptEditorFontAtZoom(
                baseEditorFont_, zoomPercent_));
            applyZoomControlUiFont();
            editor_->setIndentationStyle(formatUseTabs_, formatIndentWidth_);
            editor_->setBottomAccessoryHeight(kCogEditorAccessoryHeight);
            repositionEditorAccessoryRow();
        }
        if (persist)
        {
            QSettings settings = settingsStore();
            settings.setValue("cog_editor/zoom_percent", zoomPercent_);
        }
        if (changed && persist && onStateChanged_)
        {
            onStateChanged_();
        }
    }

    void setDiagnosticsExpanded(
        const bool expanded,
        const bool notify,
        const bool animate = false)
    {
        if (!diagnosticsPanel_ || !diagnosticsHeader_ || !issuesList_)
        {
            return;
        }

        if (diagnosticsToggle_ && diagnosticsToggle_->isChecked() != expanded)
        {
            const QSignalBlocker blocker(diagnosticsToggle_);
            diagnosticsToggle_->setChecked(expanded);
        }
        if (diagnosticsArrow_)
        {
            diagnosticsArrow_->setText(expanded ? QStringLiteral("\u25be") : QStringLiteral("\u25b8"));
        }
        // The checked disclosure state is authoritative as soon as the user
        // requests it. Animation is presentation only: recovery/autosave must
        // capture the requested endpoint even if it snapshots mid-transition,
        // and must never serialize an intermediate splitter height.
        const bool committedStateChanged = diagnosticsCommittedExpanded_ != expanded;
        diagnosticsCommittedExpanded_ = expanded;

        const int headerHeight = std::max(24, diagnosticsHeader_->sizeHint().height());
        const QList<int> previousSizes = editorColumn_ ? editorColumn_->sizes() : QList<int>{};
        const int total = previousSizes.size() == 2
            ? previousSizes.at(0) + previousSizes.at(1)
            : 0;
        if (!expanded && !diagnosticsAnimation_
            && previousSizes.size() == 2 && previousSizes.at(1) > headerHeight)
        {
            diagnosticsExpandedHeight_ = previousSizes.at(1);
        }

        const int targetHeight = expanded
            ? std::clamp(diagnosticsExpandedHeight_, 80, std::max(80, total / 2))
            : headerHeight;
        if (diagnosticsAnimation_)
        {
            diagnosticsAnimation_->stop();
            diagnosticsAnimation_->deleteLater();
            diagnosticsAnimation_ = nullptr;
        }

        auto applyHeight = [this, total](const int height)
        {
            if (!diagnosticsPanel_ || !editorColumn_)
            {
                return;
            }
            const int bounded = std::max(1, height);
            diagnosticsPanel_->setMinimumHeight(bounded);
            diagnosticsPanel_->setMaximumHeight(bounded);
            if (total > bounded)
            {
                const bool wasSuppressed = suppressDiagnosticsSplitterState_;
                suppressDiagnosticsSplitterState_ = true;
                editorColumn_->setSizes(QList<int>{ std::max(1, total - bounded), bounded });
                suppressDiagnosticsSplitterState_ = wasSuppressed;
                // QSplitter normally delivers the source resize synchronously,
                // which invokes the post-layout callback. Keep the animation
                // invariant explicit as well: every applied splitter height
                // immediately recomputes the bottom chrome from current
                // viewport/scrollbar geometry.
                repositionEditorAccessoryRow();
            }
        };

        if (expanded)
        {
            issuesList_->show();
        }

        const int currentHeight = previousSizes.size() == 2
            ? std::max(headerHeight, previousSizes.at(1))
            : headerHeight;
        if (animate && total > headerHeight && currentHeight != targetHeight)
        {
            auto* animation = new QVariantAnimation(this);
            diagnosticsAnimation_ = animation;
            animation->setStartValue(currentHeight);
            animation->setEndValue(targetHeight);
            animation->setDuration(170);
            animation->setEasingCurve(QEasingCurve::InOutCubic);
            connect(animation, &QVariantAnimation::valueChanged, this,
                [this, animation, applyHeight](const QVariant& value)
                {
                    if (diagnosticsAnimation_ == animation)
                    {
                        applyHeight(value.toInt());
                    }
                });
            connect(animation, &QVariantAnimation::finished, this,
                [this, animation, expanded, headerHeight]()
                {
                    if (diagnosticsAnimation_ != animation)
                    {
                        return;
                    }
                    diagnosticsAnimation_ = nullptr;
                    diagnosticsPanel_->setMinimumHeight(headerHeight);
                    diagnosticsPanel_->setMaximumHeight(expanded ? QWIDGETSIZE_MAX : headerHeight);
                    issuesList_->setVisible(expanded);
                    animation->deleteLater();
                });
            if (notify && committedStateChanged && onStateChanged_)
            {
                onStateChanged_();
            }
            animation->start();
            return;
        }

        applyHeight(targetHeight);
        diagnosticsPanel_->setMinimumHeight(headerHeight);
        diagnosticsPanel_->setMaximumHeight(expanded ? QWIDGETSIZE_MAX : headerHeight);
        issuesList_->setVisible(expanded);
        if (notify && committedStateChanged && onStateChanged_)
        {
            onStateChanged_();
        }
    }

    bool applyFormatting()
    {
        if (!editor_)
        {
            return false;
        }

        const QString before = editor_->toPlainText();
        const ui::cog::CogScriptFormatOptions options{
            formatUseTabs_,
            formatIndentWidth_,
            formatBraceStyle_,
            formatIndentSections_,
            formatIndentCodeBodies_,
            formatKeywordSpacing_,
            formatInsideParenthesesSpacing_,
            formatFunctionParenthesesSpacing_,
            formatForSemicolonSpacing_,
            formatAlignFinalReturnLabel_,
            formatTrimExcessBlankLines_,
            formatIndentComments_,
        };
        const QString after = formatCogScriptText(before, options);

        persistFormatOptions();

        if (before == after)
        {
            return false;
        }

        QTextCursor cursor = editor_->textCursor();
        const int position = cursor.position();
        const int anchor = cursor.anchor();
        QTextCursor docCursor(editor_->document());
        docCursor.beginEditBlock();
        docCursor.select(QTextCursor::Document);
        docCursor.insertText(after);
        docCursor.endEditBlock();

        const int maximum = static_cast<int>(after.size());
        QTextCursor restoredCursor(editor_->document());
        restoredCursor.setPosition(std::clamp(anchor, 0, maximum));
        restoredCursor.setPosition(
            std::clamp(position, 0, maximum), QTextCursor::KeepAnchor);
        editor_->setTextCursor(restoredCursor);
        editor_->document()->setModified(true);
        // The document replacement emits textChanged once at the edit-block
        // boundary; that path schedules one batched derived-state refresh.
        // Repeating complete-script scans here made Format block on large
        // cutscene COGs.
        updateWindowTitle();
        return true;
    }

    void updateWindowTitle()
    {
        const QString dirty = (editor_ && editor_->document() && editor_->document()->isModified())
            ? QStringLiteral("*")
            : QString{};
        const QString title = QStringLiteral("%1%2").arg(scriptName_, dirty);
        setWindowTitle(title);
        if (titleChanged_)
        {
            titleChanged_(title);
        }
    }

    void ensureWatchPaths(const QByteArray& acceptedFingerprint)
    {
        if ( draft_ || path_.empty() || !looseWatchStillCurrent_ )
        {
            ui::stopLooseDocumentWatch(this);
            return;
        }
        const QString qPath = QString::fromStdWString(path_.wstring());
        if ( ui::looseDocumentWatchMatches(this, qPath, acceptedFingerprint) )
        {
            return;
        }
        const QPointer<CogScriptEditorDialog> self(this);
        const auto stillCurrent = looseWatchStillCurrent_;
        ui::LooseDocumentWatchOptions options{};
        options.path = qPath;
        options.acceptedFingerprint = acceptedFingerprint;
        options.stillCurrent = [self, stillCurrent]()
        {
            return self && !self->draft_ && stillCurrent();
        };
        options.authoredRevision = [self]() -> std::uint64_t
        {
            return self ? self->looseWatchAuthoredRevision_ : 0;
        };
        options.reload = [self]() -> ui::LooseDocumentReloadResult
        {
            return self ? self->reloadWatchedSource() : ui::LooseDocumentReloadResult{};
        };
        ui::watchLooseDocument(this, std::move(options));
    }

    ui::LooseDocumentReloadResult reloadWatchedSource()
    {
        if ( !editor_ || draft_ || !looseWatchStillCurrent_ || !looseWatchStillCurrent_() )
        {
            return {};
        }
        const QString qPath = QString::fromStdWString(path_.wstring());
        const CogScriptSourceReadResult source = readCogScriptSourceBounded(path_);
        if ( !source.succeeded() )
        {
            return { false, {}, cogScriptSourceReadError(source, qPath) };
        }
        const ui::LooseDocumentReloadResult result{
            true, QCryptographicHash::hash(source.bytes, QCryptographicHash::Sha256), {}};
        return applyReloadedText(QString::fromUtf8(source.bytes), result.acceptedFingerprint)
            ? result : ui::LooseDocumentReloadResult{};
    }

    bool applyReloadedText(const QString& diskText, const QByteArray& acceptedFingerprint)
    {
        const QPointer<CogScriptEditorDialog> self(this);
        const QPointer<CogScriptCodeEditor> editor(editor_);
        const AuthoredBufferResolvedCallback resolved = onAuthoredBufferResolved_;
        const std::filesystem::path resolvedPath = path_;
        reloadingExternal_ = true;
        editor->setPlainText(diskText);
        if ( !self || !editor )
        {
            return true;
        }
        editor->document()->setModified(false);
        if ( !self || !editor )
        {
            return true;
        }
        documentHistory_->reset();
        reloadingExternal_ = false;
        lastSavedText_ = diskText;
        acceptedSourceHash_ = acceptedFingerprint.size() == 32
            ? acceptedFingerprint : QByteArray{};
        // setPlainText schedules one derived-state pass; do not rescan here.
        ui::acceptLooseDocumentSource(this, QString::fromStdWString(path_.wstring()), acceptedFingerprint);
        updateWindowTitle();
        if ( self && resolved )
        {
            resolved(resolvedPath);
        }
        return true;
    }

    bool reloadFromDisk(
        const bool onlyIfDiskChanged,
        const bool showReadError,
        const bool confirmRecoveredNewerVersion = false)
    {
        if (!editor_ || selfSaving_)
        {
            return false;
        }

        const QString qPath = QString::fromStdWString(path_.wstring());
        CogScriptSourceReadResult source = readCogScriptSourceBounded(path_);
        if ( !source.succeeded() )
        {
            if (showReadError)
            {
                const QString detail = cogScriptSourceReadError(source, qPath);
                showCogEditorResourceFailure(
                    this,
                    "Reload COG script",
                    detail,
                    QStringLiteral("COG script reload failed for '%1' and buffer '%2': %3")
                        .arg(qPath, scriptName_, detail),
                    CogEditorFailureSeverity::warning);
            }
            return false;
        }

        QByteArray diskBytes = std::move(source.bytes);
        QString diskText = QString::fromUtf8(diskBytes);
        if (onlyIfDiskChanged && diskText == lastSavedText_)
        {
            return false;
        }

        if (editor_->document()->isModified() || confirmRecoveredNewerVersion)
        {
            if (externalReloadPromptActive_)
            {
                return false;
            }
            QPointer<CogScriptEditorDialog> self(this);
            QPointer<CogScriptCodeEditor> editor(editor_);
            const int documentRevision = editor->document()->revision();
            const QString authoredText = editor->toPlainText();
            const QString authoredName = scriptName_;
            const std::filesystem::path authoredPath = path_;
            const bool authoredDraft = draft_;
            externalReloadPromptActive_ = true;

            const auto answer = showCogEditorMessageBox(
                this,
                QMessageBox::Question,
                "Reload COG script",
                confirmRecoveredNewerVersion
                    ? "A newer version of this COG script is available on disk. Reload it and discard the buffer stored in the SED file?"
                    : "Reload from disk and discard local edits?",
                QMessageBox::Discard | QMessageBox::Cancel,
                QMessageBox::Cancel,
                cogBufferLoggingContext(scriptName_, path_));
            if ( !self )
            {
                return false;
            }
            self->externalReloadPromptActive_ = false;
            if ( answer != QMessageBox::Discard || !editor
                || self->editor_ != editor.data() || self->scriptName_ != authoredName
                || self->path_ != authoredPath || self->draft_ != authoredDraft
                || editor->document()->revision() != documentRevision
                || editor->toPlainText() != authoredText )
            {
                return false;
            }

            // The prompt runs a nested event loop. A writer can replace the
            // file after the initial read while the question is visible;
            // never install that stale pre-prompt snapshot after Discard.
            CogScriptSourceReadResult latest = readCogScriptSourceBounded(path_);
            if ( !latest.succeeded() )
            {
                if (showReadError)
                {
                    const QString detail = cogScriptSourceReadError(latest, qPath);
                    showCogEditorResourceFailure(
                        this,
                        "Reload COG script",
                        detail,
                        QStringLiteral("COG script reload failed after the confirmation prompt for '%1': %2")
                            .arg(qPath, detail),
                        CogEditorFailureSeverity::warning);
                }
                return false;
            }
            diskBytes = std::move(latest.bytes);
            diskText = QString::fromUtf8(diskBytes);
        }

        return applyReloadedText(
            diskText, QCryptographicHash::hash(diskBytes, QCryptographicHash::Sha256));
    }

    void jumpToLine(const int line)
    {
        if (!editor_ || line < 0)
        {
            return;
        }

        QTextBlock block = editor_->document()->findBlockByNumber(line);
        if (!block.isValid())
        {
            return;
        }

        QTextCursor cursor(block);
        editor_->setTextCursor(cursor);
        editor_->centerCursor();
        editor_->setFocus(Qt::OtherFocusReason);
    }

    std::optional<QString> identifierFromCursor(const QTextCursor& sourceCursor) const
    {
        if (!editor_)
        {
            return std::nullopt;
        }

        const QTextBlock block = sourceCursor.block();
        const ui::cog::CogScriptEditorSection section =
            CogScriptSyntaxHighlighter::sectionForBlock(block);
        if ( section == ui::cog::CogScriptEditorSection::symbols
            && !ui::cog::cogScriptCursorIsSymbolDeclarationName(
                block.text(), section, sourceCursor.positionInBlock()) )
        {
            return std::nullopt;
        }

        QTextCursor cursor(sourceCursor);
        cursor.select(QTextCursor::WordUnderCursor);
        const QString ident = cursor.selectedText().trimmed();
        if (ident.isEmpty())
        {
            return std::nullopt;
        }
        return ident;
    }

    void jumpToDefinitionFromCursor()
    {
        if (!editor_)
        {
            return;
        }

        const std::optional<QString> ident = identifierFromCursor(editor_->textCursor());
        if (!ident)
        {
            return;
        }
        const QString text = editor_->toPlainText();
        if (const std::optional<int> line = findCogScriptCodeLabelLine(text, *ident))
        {
            jumpToLine(*line);
            return;
        }
        if (const std::optional<int> line = findCogScriptDefinitionLine(text, *ident))
        {
            jumpToLine(*line);
        }
    }

    void jumpToDefinitionAtPosition(const QPoint& pos)
    {
        if (!editor_)
        {
            return;
        }

        QTextCursor cursor = editor_->cursorForPosition(pos);
        editor_->setTextCursor(cursor);
        const std::optional<QString> ident = identifierFromCursor(cursor);
        if (!ident)
        {
            return;
        }
        const QString text = editor_->toPlainText();
        if (const std::optional<int> line = findCogScriptCodeLabelLine(text, *ident))
        {
            jumpToLine(*line);
            return;
        }
        if (const std::optional<int> line = findCogScriptDefinitionLine(text, *ident))
        {
            jumpToLine(*line);
        }
    }

    ScriptSectionsInfo scanSections() const
    {
        return editor_ ? scanCogScriptSections(editor_->toPlainText()) : ScriptSectionsInfo{};
    }

    QTextCursor cursorAtLineStart(const int line) const
    {
        QTextCursor cursor(editor_ ? editor_->document() : nullptr);
        if (!editor_ || !editor_->document())
        {
            return cursor;
        }

        if (line < 0)
        {
            cursor.movePosition(QTextCursor::End);
            return cursor;
        }

        QTextBlock block = editor_->document()->findBlockByNumber(line);
        if (!block.isValid())
        {
            cursor.movePosition(QTextCursor::End);
            return cursor;
        }

        return QTextCursor(block);
    }

    int viewportLineForPosition(const QPoint& pos) const
    {
        if (!editor_)
        {
            return -1;
        }
        return editor_->cursorForPosition(pos).block().blockNumber();
    }

    std::optional<int> currentEditorCursorLine() const
    {
        if (!editor_)
        {
            return std::nullopt;
        }
        return editor_->textCursor().blockNumber();
    }

    std::optional<int> currentEditorCursorColumn() const
    {
        if (!editor_)
        {
            return std::nullopt;
        }

        const QTextCursor cursor = editor_->textCursor();
        return cursor.position() - cursor.block().position();
    }

    void captureInsertMenuContext()
    {
        // QToolButton/QMenu focus transitions must not replace the source
        // location that decides which context-sensitive insert actions exist.
        insertMenuLineHint_ = currentEditorCursorLine();
        insertMenuColumnHint_ = currentEditorCursorColumn();
    }

    using EditorInsertContext = CogScriptInsertContext;

    EditorInsertContext editorInsertContextAtLine(const std::optional<int> lineHint) const
    {
        if (!editor_)
        {
            return EditorInsertContext::outside;
        }

        const int targetLine = lineHint.value_or(editor_->textCursor().blockNumber());
        return cogScriptInsertContextAtLine(editor_->toPlainText(), targetLine);
    }

    int preferredSymbolInsertLine(const std::optional<int> lineHint) const
    {
        return editor_ ? preferredCogScriptSymbolInsertLine(editor_->toPlainText(), lineHint) : -1;
    }

    int preferredMessageSymbolInsertLine(const std::optional<int> lineHint) const
    {
        return editor_ ? preferredCogScriptMessageSymbolInsertLine(editor_->toPlainText(), lineHint) : -1;
    }

    int preferredCodeInsertLine(const std::optional<int> lineHint) const
    {
        return editor_ ? preferredCogScriptCodeInsertLine(editor_->toPlainText(), lineHint) : -1;
    }

    void insertTextAtLine(const int line, const QString& text)
    {
        if (!editor_ || text.isEmpty())
        {
            return;
        }

        QTextCursor cursor = cursorAtLineStart(line);
        cursor.beginEditBlock();
        if ( !cursor.atBlockStart() )
        {
            cursor.insertText(QStringLiteral("\n"));
        }
        cursor.insertText(text);
        cursor.endEditBlock();
        editor_->setTextCursor(cursor);
        editor_->setFocus(Qt::OtherFocusReason);
    }

    static QString leadingWhitespace(const QString& text)
    {
        int length = 0;
        while (length < text.size() && text.at(length).isSpace())
        {
            ++length;
        }
        return text.left(length);
    }

    QString defaultSectionBodyIndent() const
    {
        return ui::cog::indentString(
            formatIndentSections_ ? 1 : 0,
            formatUseTabs_,
            formatIndentWidth_);
    }

    QString inferredSymbolIndent(const int targetLine) const
    {
        if (!editor_)
        {
            return defaultSectionBodyIndent();
        }

        const QString text = editor_->toPlainText();
        const QStringList lines = text.split(QLatin1Char('\n'));
        const ScriptSectionsInfo sections = scanCogScriptSections(text);
        int bestDistance = std::numeric_limits<int>::max();
        QString bestIndent{};
        for (int line = sections.symbolsLine + 1;
             line >= 0 && line < sections.symbolsEndLine && line < lines.size();
             ++line)
        {
            if (!cogScriptSymbolLineContextAtLine(text, line))
            {
                continue;
            }
            const int distance = std::abs(line - targetLine);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndent = leadingWhitespace(lines.at(line));
            }
        }
        return bestDistance == std::numeric_limits<int>::max()
            ? defaultSectionBodyIndent()
            : bestIndent;
    }

    QString inferredCodeLabelIndent(const int targetLine) const
    {
        if (!editor_)
        {
            return defaultSectionBodyIndent();
        }

        static const QRegularExpression kLabelRx(
            QStringLiteral("^\\s*[A-Za-z_][A-Za-z0-9_]*\\s*:"),
            QRegularExpression::UseUnicodePropertiesOption);
        const QStringList lines = editor_->toPlainText().split(QLatin1Char('\n'));
        const ScriptSectionsInfo sections = scanSections();
        int bestDistance = std::numeric_limits<int>::max();
        QString bestIndent{};
        for (int line = sections.codeLine + 1;
             line >= 0 && line < sections.codeEndLine && line < lines.size();
             ++line)
        {
            if (!kLabelRx.match(ui::cog::stripComment(lines.at(line))).hasMatch())
            {
                continue;
            }
            const int distance = std::abs(line - targetLine);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestIndent = leadingWhitespace(lines.at(line));
            }
        }
        return bestDistance == std::numeric_limits<int>::max()
            ? defaultSectionBodyIndent()
            : bestIndent;
    }

    QString inferredHandlerBodyIndent(
        const int targetLine,
        const QString& labelIndent) const
    {
        if (editor_)
        {
            static const QRegularExpression kLabelRx(
                QStringLiteral("^\\s*[A-Za-z_][A-Za-z0-9_]*\\s*:"),
                QRegularExpression::UseUnicodePropertiesOption);
            const QStringList lines = editor_->toPlainText().split(QLatin1Char('\n'));
            const ScriptSectionsInfo sections = scanSections();
            int bestDistance = std::numeric_limits<int>::max();
            QString bestIndent{};
            for (int labelLine = sections.codeLine + 1;
                 labelLine >= 0 && labelLine < sections.codeEndLine && labelLine < lines.size();
                 ++labelLine)
            {
                if (!kLabelRx.match(ui::cog::stripComment(lines.at(labelLine))).hasMatch())
                {
                    continue;
                }
                for (int bodyLine = labelLine + 1;
                     bodyLine < sections.codeEndLine && bodyLine < lines.size();
                     ++bodyLine)
                {
                    const QString body = ui::cog::stripComment(lines.at(bodyLine)).trimmed();
                    if (body.isEmpty())
                    {
                        continue;
                    }
                    if (kLabelRx.match(body).hasMatch()
                        || body.compare(QStringLiteral("end"), Qt::CaseInsensitive) == 0)
                    {
                        break;
                    }
                    const int distance = std::abs(labelLine - targetLine);
                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        bestIndent = leadingWhitespace(lines.at(bodyLine));
                    }
                    break;
                }
            }
            if (bestDistance != std::numeric_limits<int>::max())
            {
                return bestIndent;
            }
        }

        if (!formatIndentCodeBodies_)
        {
            return labelIndent;
        }
        return labelIndent + ui::cog::indentString(1, formatUseTabs_, formatIndentWidth_);
    }

    void populateSectionInsertMenu(QMenu* menu, const std::optional<int> lineHint)
    {
        if (!menu)
        {
            return;
        }

        const ScriptSectionsInfo sections = scanSections();
        const struct { const char* name; bool present; } defs[] = {
            { "flags", sections.flagsLine >= 0 },
            { "symbols", sections.symbolsLine >= 0 },
            { "code", sections.codeLine >= 0 },
        };
        for (const auto& def : defs)
        {
            QAction* action = menu->addAction(QString::fromLatin1(def.name));
            action->setEnabled(!def.present);
            connect(action, &QAction::triggered, this, [this, sectionName = QString::fromLatin1(def.name), lineHint]()
                {
                    insertSection(sectionName, lineHint);
                });
        }
    }

    void populateSnippetInsertMenu(QMenu* menu, const std::optional<int> lineHint)
    {
        if (!menu)
        {
            return;
        }

        QAction* skeletonAction = menu->addAction("Full script skeleton");
        if (editor_)
        {
            skeletonAction->setEnabled(editor_->toPlainText().trimmed().isEmpty());
        }
        connect(skeletonAction, &QAction::triggered, this, [this]()
            {
                insertSnippet(ui::cog::skeletonSnippet());
            });

        menu->addSeparator();

        QAction* startupHandlerAction = menu->addAction("Startup message");
        startupHandlerAction->setEnabled(
            !findCogScriptCodeLabelLine(editor_ ? editor_->toPlainText() : QString{}, QStringLiteral("startup")).has_value());
        connect(startupHandlerAction, &QAction::triggered, this, [this, lineHint]()
            {
                insertHandler(QStringLiteral("startup"), true, lineHint);
            });
    }

    void populateSymbolInsertMenu(QMenu* menu, const std::optional<int> lineHint)
    {
        if (!menu)
        {
            return;
        }

        const QStringList resourceTypeNames{
            QStringLiteral("ai"),
            QStringLiteral("keyframe"),
            QStringLiteral("material"),
            QStringLiteral("model"),
            QStringLiteral("sound"),
            QStringLiteral("template"),
        };
        QStringList primitiveTypes{};
        QStringList resourceTypes{};
        for (const QString& typeName : cogScriptInsertTypeWords())
        {
            const bool isResourceType = std::any_of(
                resourceTypeNames.cbegin(),
                resourceTypeNames.cend(),
                [&typeName](const QString& resourceTypeName)
                {
                    return typeName.compare(
                        resourceTypeName, Qt::CaseInsensitive) == 0;
                });
            (isResourceType ? resourceTypes : primitiveTypes).push_back(typeName);
        }
        primitiveTypes.sort(Qt::CaseInsensitive);
        resourceTypes.sort(Qt::CaseInsensitive);

        const int sourceRevision = editor_ ? editor_->document()->revision() : -1;
        auto addTypeActions = [this, menu, lineHint, sourceRevision](const QStringList& typeNames)
        {
            for (const QString& typeName : typeNames)
            {
                QAction* action = menu->addAction(typeName);
                connect(action, &QAction::triggered, this,
                    [this, typeName, lineHint, sourceRevision]()
                    {
                        if ( editor_ && !editor_->isReadOnly()
                            && editor_->document()->revision() == sourceRevision )
                        {
                            insertSymbolDeclaration(typeName, lineHint);
                        }
                    });
            }
        };

        addTypeActions(primitiveTypes);
        if (!primitiveTypes.isEmpty() && !resourceTypes.isEmpty())
        {
            menu->addSeparator();
        }
        addTypeActions(resourceTypes);
    }

    void populateHandlerInsertMenu(QMenu* menu, const std::optional<int> lineHint,
                                   const bool codeBoundaryLine = false)
    {
        if (!menu)
        {
            return;
        }

        const int sourceRevision = editor_ ? editor_->document()->revision() : -1;
        QStringList messages = cogScriptProjectMessageWords(projectType_);
        messages.sort(Qt::CaseInsensitive);
        for (const QString& messageName : messages)
        {
            QAction* action = menu->addAction(messageName);
            action->setEnabled(!findCogScriptCodeLabelLine(editor_ ? editor_->toPlainText() : QString{}, messageName).has_value());
            connect(action, &QAction::triggered, this,
                [this, messageName, lineHint, sourceRevision, codeBoundaryLine]()
                {
                    if ( editor_ && !editor_->isReadOnly()
                        && editor_->document()->revision() == sourceRevision )
                    {
                        insertHandler(messageName, true, lineHint, codeBoundaryLine);
                    }
                });
        }
    }

    using SymbolLineContext = CogScriptSymbolLineContext;
    using SymbolLineAttributeMatch = CogScriptSymbolLineAttributeMatch;

    std::optional<SymbolLineContext> symbolLineContextAtLine(const std::optional<int> lineHint) const
    {
        if (!editor_)
        {
            return std::nullopt;
        }

        const int targetLine = lineHint.value_or(editor_->textCursor().blockNumber());
        return cogScriptSymbolLineContextAtLine(editor_->toPlainText(), targetLine);
    }

    std::optional<SymbolLineAttributeMatch> symbolLineAttributeMatch(
        const SymbolLineContext& context,
        const QString& attributeName,
        const std::optional<int> column = std::nullopt) const
    {
        if (!editor_ || context.line < 0)
        {
            return std::nullopt;
        }
        return cogScriptSymbolLineAttributeMatch(editor_->toPlainText(), context, attributeName, column);
    }

    std::optional<QString> symbolLineAttributeValue(
        const SymbolLineContext& context,
        const QString& attributeName,
        const std::optional<int> column = std::nullopt) const
    {
        return editor_
            ? cogScriptSymbolLineAttributeValue(editor_->toPlainText(), context, attributeName, column)
            : std::nullopt;
    }

    bool symbolLineHasAttribute(const SymbolLineContext& context, const QString& attributeName) const
    {
        return editor_ && cogScriptSymbolLineHasAttribute(editor_->toPlainText(), context, attributeName);
    }

    int symbolLineAttributeCount(const SymbolLineContext& context, const QString& attributeName) const
    {
        return editor_ ? cogScriptSymbolLineAttributeCount(editor_->toPlainText(), context, attributeName) : 0;
    }

    std::optional<SymbolLineAttributeMatch> symbolLineAttributeAtColumn(const SymbolLineContext& context, const int column) const
    {
        return editor_
            ? cogScriptSymbolLineAttributeAtColumn(editor_->toPlainText(), context, column)
            : std::nullopt;
    }

    static bool symbolLineAttributeValueHit(const SymbolLineAttributeMatch& match, const int column)
    {
        return cogScriptSymbolLineAttributeValueHit(match, column);
    }

    void applySymbolLineTextEdit(const std::optional<QString>& updatedText)
    {
        if (!editor_ || !updatedText)
        {
            return;
        }

        const QString oldText = editor_->toPlainText();
        const ui::cog::CogScriptTextEditSelection mappedSelection =
            ui::cog::mapCogScriptTextEditSelection(
                oldText,
                *updatedText,
                editor_->textCursor().anchor(),
                editor_->textCursor().position());
        QTextCursor cursor = editor_->textCursor();
        cursor.beginEditBlock();
        cursor.select(QTextCursor::Document);
        cursor.insertText(*updatedText);
        cursor.setPosition(mappedSelection.anchor);
        cursor.setPosition(mappedSelection.position, QTextCursor::KeepAnchor);
        cursor.endEditBlock();
        editor_->setTextCursor(cursor);
        editor_->setFocus(Qt::OtherFocusReason);
        // textChanged schedules the complete derived-state refresh. Avoid a
        // second synchronous diagnostics scan for context-menu symbol edits.
    }

    bool symbolLineContextStillMatches(const SymbolLineContext& expected) const
    {
        const std::optional<SymbolLineContext> current =
            symbolLineContextAtLine(expected.line);
        return current
            && current->typeName.compare(expected.typeName, Qt::CaseInsensitive) == 0
            && current->identifier.compare(expected.identifier, Qt::CaseInsensitive) == 0;
    }

    void removeSymbolLineAttribute(
        const SymbolLineContext& context,
        const QString& attributeName,
        const std::optional<int> column = std::nullopt)
    {
        if (!symbolLineContextStillMatches(context))
        {
            return;
        }
        applySymbolLineTextEdit(
            editor_ ? removeCogScriptSymbolLineAttribute(editor_->toPlainText(), context, attributeName, column) : std::nullopt);
    }

    void dedupeSymbolLineAttribute(
        const SymbolLineContext& context,
        const QString& attributeName,
        const std::optional<int> keepColumn = std::nullopt)
    {
        if (!symbolLineContextStillMatches(context))
        {
            return;
        }
        applySymbolLineTextEdit(
            editor_ ? dedupeCogScriptSymbolLineAttribute(editor_->toPlainText(), context, attributeName, keepColumn) : std::nullopt);
    }

    void upsertSymbolLineAttribute(
        const SymbolLineContext& context,
        const QString& attributeName,
        const std::optional<QString>& attributeValue,
        const std::optional<int> column = std::nullopt)
    {
        if (!symbolLineContextStillMatches(context))
        {
            return;
        }
        applySymbolLineTextEdit(
            editor_ ? upsertCogScriptSymbolLineAttribute(editor_->toPlainText(), context, attributeName, attributeValue, column) : std::nullopt);
    }

    QPoint defaultCogPopupAnchor() const
    {
        if ( insertToolButton_ )
        {
            return insertToolButton_->mapToGlobal(
                insertToolButton_->rect().center());
        }
        if ( editor_ )
        {
            return editor_->viewport()->mapToGlobal(
                editor_->viewport()->rect().center());
        }
        return mapToGlobal(rect().center());
    }

    void editSymbolLineMask(
        const SymbolLineContext& context,
        const int targetColumn,
        const int sourceRevision,
        const bool allowAdd = false,
        const std::optional<QPoint> popupAnchor = std::nullopt)
    {
        if ( !editor_ || editor_->isReadOnly()
            || editor_->document()->revision() != sourceRevision
            || !symbolLineContextStillMatches(context) )
        {
            return;
        }

        const QPointer<CogScriptEditorDialog> ownerGuard(this);
        const QPointer<CogScriptCodeEditor> editorGuard(editor_);
        const QPointer<QTextDocument> documentGuard(editor_->document());
        const auto initialMatch = symbolLineAttributeAtColumn(context, targetColumn);
        if ( !initialMatch || initialMatch->name.compare(
                QStringLiteral("mask"), Qt::CaseInsensitive) != 0 )
        {
            if ( !allowAdd || symbolLineHasAttribute(
                    context, QStringLiteral("mask")) )
            {
                return;
            }
            const std::optional<QString> next = promptMaskAttributeValue(
                std::nullopt,
                popupAnchor.value_or(defaultCogPopupAnchor()));
            if ( !ownerGuard || !editorGuard || !documentGuard || !next
                || documentGuard != editorGuard->document()
                || documentGuard->revision() != sourceRevision
                || editorGuard->isReadOnly()
                || !symbolLineContextStillMatches(context) )
            {
                return;
            }
            upsertSymbolLineAttribute(
                context, QStringLiteral("mask"), *next);
            return;
        }

        const QTextBlock initialBlock = documentGuard->findBlockByNumber(context.line);
        if ( !initialBlock.isValid() )
        {
            return;
        }
        const QString initialRawToken = initialMatch->valueStart >= 0
                && initialMatch->valueLength > 0
            ? initialBlock.text().mid(
                initialMatch->valueStart, initialMatch->valueLength)
            : initialMatch->value;
        const std::optional<QString> next = promptMaskAttributeValue(
            initialRawToken,
            popupAnchor.value_or(defaultCogPopupAnchor()));
        if ( !ownerGuard || !editorGuard || !documentGuard || !next
            || documentGuard != editorGuard->document()
            || documentGuard->revision() != sourceRevision
            || editorGuard->isReadOnly()
            || !symbolLineContextStillMatches(context) )
        {
            return;
        }

        const auto currentMatch = symbolLineAttributeAtColumn(context, targetColumn);
        const QTextBlock currentBlock = documentGuard->findBlockByNumber(context.line);
        if ( !currentMatch || !currentBlock.isValid()
            || currentMatch->name.compare(
                QStringLiteral("mask"), Qt::CaseInsensitive) != 0 )
        {
            return;
        }
        const QString currentRawToken = currentMatch->valueStart >= 0
                && currentMatch->valueLength > 0
            ? currentBlock.text().mid(
                currentMatch->valueStart, currentMatch->valueLength)
            : currentMatch->value;
        if ( currentRawToken != initialRawToken || *next == initialRawToken )
        {
            return;
        }

        if ( currentMatch->valueStart < 0 || currentMatch->valueLength <= 0 )
        {
            upsertSymbolLineAttribute(
                context, QStringLiteral("mask"), *next, targetColumn);
            return;
        }

        QString replacement = *next;
        if ( currentRawToken.startsWith(QLatin1Char('\''))
            || currentRawToken.startsWith(QLatin1Char('"')) )
        {
            const QChar quote = currentRawToken.front();
            replacement = quote + replacement + quote;
        }
        QString updatedText = editorGuard->toPlainText();
        const int absoluteStart = currentBlock.position() + currentMatch->valueStart;
        updatedText.replace(absoluteStart, currentMatch->valueLength, replacement);
        applySymbolLineTextEdit(updatedText);
    }

    void editCogScriptMask(
        const ui::cog::CogScriptInlineAssignmentSpan& span,
        const int blockNumber,
        const int sourceRevision,
        const QPoint popupAnchor)
    {
        if ( !editor_ || editor_->isReadOnly()
            || span.kind != ui::cog::CogScriptInlineAssignmentKind::mask
            || span.section != ui::cog::CogScriptEditorSection::symbols
            || span.valueLength <= 0
            || editor_->document()->revision() != sourceRevision )
        {
            return;
        }

        const QTextBlock block = editor_->document()->findBlockByNumber(blockNumber);
        const std::optional<SymbolLineContext> context =
            symbolLineContextAtLine(blockNumber);
        if ( !block.isValid() || !context
            || context->typeName.compare(span.typeName, Qt::CaseInsensitive) != 0
            || context->identifier.compare(span.symbolName, Qt::CaseInsensitive) != 0 )
        {
            return;
        }

        const auto match = symbolLineAttributeAtColumn(*context, span.valueStart);
        if ( !match || match->name.compare(
                QStringLiteral("mask"), Qt::CaseInsensitive) != 0 )
        {
            return;
        }
        editSymbolLineMask(
            *context, span.valueStart, sourceRevision, false, popupAnchor);
    }

    std::optional<QString> promptMaskAttributeValue(
        const std::optional<QString>& initialValue,
        const QPoint globalAnchor)
    {
        if ( !editor_ || editor_->isReadOnly() )
        {
            return std::nullopt;
        }

        const QTextCursor sourceCursor = editor_->textCursor();
        const int sourceRevision = editor_->document()->revision();
        const QPointer<CogScriptEditorDialog> ownerGuard(this);
        const QPointer<CogScriptCodeEditor> editorGuard(editor_);
        const QPointer<QTextDocument> documentGuard(editor_->document());
        QPointer<QDialog> dialog(new QDialog(this, Qt::Popup | Qt::FramelessWindowHint));
        const auto dialogCleanup = qScopeGuard([&dialog]()
        {
            if ( dialog )
            {
                dialog->deleteLater();
            }
        });
        dialog->setObjectName(QStringLiteral("cogEditorMaskDialog"));
        dialog->setWindowTitle(QStringLiteral("Select mask"));
        dialog->setModal(true);

        auto* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(8);

        auto* info = new QLabel(
            QStringLiteral("Choose which source types can trigger messages for this symbol."),
            dialog);
        info->setWordWrap(true);
        layout->addWidget(info);

        auto* systemHint = new QLabel(
            QStringLiteral("Hint: <b>System (0x1)</b> covers engine/system-triggered messages such as startup."),
            dialog);
        systemHint->setWordWrap(true);
        systemHint->setTextFormat(Qt::RichText);
        systemHint->setStyleSheet(QStringLiteral("color: rgba(220, 224, 230, 170);"));
        layout->addWidget(systemHint);

        auto* grid = new QGridLayout();
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(22);
        grid->setVerticalSpacing(6);
        grid->setColumnMinimumWidth(0, 156);
        grid->setColumnMinimumWidth(1, 156);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        layout->addLayout(grid);

        std::uint32_t initialMask = 0x401u;
        bool initialValueValid = false;
        if (initialValue)
        {
            QString token = initialValue->trimmed();
            if ( token.size() >= 2
                && (token.front() == QLatin1Char('\'')
                    || token.front() == QLatin1Char('"'))
                && token.back() == token.front() )
            {
                token = token.mid(1, token.size() - 2).trimmed();
            }
            if ( token == QStringLiteral("-1") )
            {
                initialMask = std::numeric_limits<std::uint32_t>::max();
                initialValueValid = true;
            }
            else if (const auto parsedMask = ui::cog::parseMaskValue(token))
            {
                initialMask = *parsedMask;
                initialValueValid = true;
            }
        }
        std::uint32_t representedBits = 0x1u;

        auto* systemCheck = new QCheckBox(QStringLiteral("System (0x1)"), dialog);
        systemCheck->setChecked((initialMask & 0x1u) != 0u);
        const QString systemHelp = QStringLiteral("Enable messages fired by the engine/system itself, such as startup.");
        systemCheck->setToolTip(systemHelp);
        systemCheck->setStatusTip(systemHelp);
        systemCheck->setWhatsThis(systemHelp);
        grid->addWidget(systemCheck, 0, 0, 1, 2, Qt::AlignLeft | Qt::AlignVCenter);

        QList<QCheckBox*> checks{};
        const auto thingTypes = cogScriptMaskThingTypeOptions(projectType_);
        for (int i = 0; i < thingTypes.size(); ++i)
        {
            const CogMaskThingTypeOption& def = thingTypes[i];
            const std::uint32_t bit = (1u << static_cast<std::uint32_t>(def.typeNumber));
            representedBits |= bit;
            auto* check = new QCheckBox(
                QStringLiteral("%1 (0x%2)")
                    .arg(def.label)
                    .arg(bit, 0, 16),
                dialog);
            check->setChecked((initialMask & bit) != 0u);
            grid->addWidget(check, 1 + (i / 2), i % 2, Qt::AlignLeft | Qt::AlignVCenter);
            checks.push_back(check);
        }

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            dialog);
        buttons->setCenterButtons(true);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

        QLayout* const dialogLayout = dialog->layout();
        dialogLayout->setSizeConstraint(QLayout::SetFixedSize);
        dialog->ensurePolished();
        dialogLayout->activate();
        dialog->adjustSize();
        const QSize fixedSize = dialog->sizeHint().expandedTo(
            QSize(404, dialog->sizeHint().height()));
        dialogLayout->setSizeConstraint(QLayout::SetNoConstraint);
        dialog->setFixedSize(fixedSize);
        positionCogPopupAtAnchor(*dialog, globalAnchor, this);

        const int dialogResult = dialog->exec();
        if ( !ownerGuard || !editorGuard || !documentGuard || !dialog )
        {
            return std::nullopt;
        }
        if ( editor_ != editorGuard.data()
            || editorGuard->document() != documentGuard
            || documentGuard->revision() != sourceRevision
            || editorGuard->isReadOnly() )
        {
            return std::nullopt;
        }
        if ( dialogResult != QDialog::Accepted )
        {
            editorGuard->setTextCursor(sourceCursor);
            editorGuard->setFocus(Qt::OtherFocusReason);
            return std::nullopt;
        }

        std::uint32_t mask = initialMask & ~representedBits;
        if (systemCheck->isChecked())
        {
            mask |= 0x1u;
        }
        for (int i = 0; i < thingTypes.size() && i < checks.size(); ++i)
        {
            if (checks[i] && checks[i]->isChecked())
            {
                mask |= (1u << static_cast<std::uint32_t>(thingTypes[i].typeNumber));
            }
        }

        editorGuard->setTextCursor(sourceCursor);
        editorGuard->setFocus(Qt::OtherFocusReason);
        if ( initialValue && initialValueValid && mask == initialMask )
        {
            return initialValue;
        }
        return ui::cog::formatMaskHex(mask);
    }

    std::optional<QString> promptLinkIdAttributeValue(const std::optional<QString>& initialValue)
    {
        const QTextCursor sourceCursor = editor_ ? editor_->textCursor() : QTextCursor{};
        int initialNumericValue = 0;
        if (initialValue)
        {
            if (const auto parsed = sed::text::parseInt<int>(initialValue->toStdString()))
            {
                initialNumericValue = *parsed;
            }
        }

        bool ok = false;
        const int linkId = QInputDialog::getInt(
            this,
            QStringLiteral("Link ID"),
            QStringLiteral("Sender link id"),
            initialNumericValue,
            std::numeric_limits<int>::min(),
            std::numeric_limits<int>::max(),
            1,
            &ok);
        if (editor_)
        {
            editor_->setTextCursor(sourceCursor);
            editor_->setFocus(Qt::OtherFocusReason);
        }
        if (!ok)
        {
            return std::nullopt;
        }

        return QString::number(linkId);
    }

    std::optional<QString> promptDescAttributeValue(const std::optional<QString>& initialValue)
    {
        const QTextCursor sourceCursor = editor_ ? editor_->textCursor() : QTextCursor{};
        bool ok = false;
        QString desc = QInputDialog::getText(
            this,
            QStringLiteral("Description"),
            QStringLiteral("One-word description"),
            QLineEdit::Normal,
            initialValue.value_or(QString{}),
            &ok).trimmed();
        if (editor_)
        {
            editor_->setTextCursor(sourceCursor);
            editor_->setFocus(Qt::OtherFocusReason);
        }
        if (!ok || desc.isEmpty())
        {
            return std::nullopt;
        }

        desc.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
        return desc;
    }

    std::optional<int> preferredAttributeColumnHint(
        const SymbolLineContext& context,
        const QString& attributeName,
        const std::optional<int> columnHint) const
    {
        if (columnHint && symbolLineAttributeMatch(context, attributeName, columnHint))
        {
            return columnHint;
        }

        if (symbolLineAttributeMatch(context, attributeName))
        {
            return std::nullopt;
        }

        return columnHint;
    }

    bool populateAttributeInsertMenu(
        QMenu* menu,
        const std::optional<int> lineHint,
        const std::optional<int> columnHint = std::nullopt,
        const std::optional<QPoint> popupAnchor = std::nullopt)
    {
        if (!menu)
        {
            return false;
        }

        const std::optional<SymbolLineContext> context = symbolLineContextAtLine(lineHint);
        if (!context)
        {
            return false;
        }

        auto addAction = [&](const QString& label, const QString& attributeName, const bool enabled, auto onTrigger)
        {
            QAction* action = menu->addAction(label);
            const QString help = cogScriptAttributeHelpText(attributeName);
            action->setToolTip(help);
            action->setStatusTip(help);
            action->setWhatsThis(help);
            action->setEnabled(enabled);
            if (enabled)
            {
                connect(action, &QAction::triggered, this, onTrigger);
            }
            return action;
        };

        const bool supportsTriggerFilterAttributes = cogScriptTypeSupportsTriggerFilterAttributes(context->typeName);
        const bool hasLocalAttribute = symbolLineHasAttribute(*context, QStringLiteral("local"));
        const bool hasDescAttribute = symbolLineHasAttribute(*context, QStringLiteral("desc"));
        const bool hasMaskAttribute = symbolLineHasAttribute(*context, QStringLiteral("mask"));
        const bool hasLinkIdAttribute = symbolLineHasAttribute(*context, QStringLiteral("linkid"));
        const bool hasNoLinkAttribute = symbolLineHasAttribute(*context, QStringLiteral("nolink"));

        addAction(
            hasLocalAttribute ? QStringLiteral("Remove local") : QStringLiteral("Add local"),
            QStringLiteral("local"),
            true,
            [this, context, columnHint, hasLocalAttribute]()
            {
                const std::optional<int> targetColumnHint =
                    preferredAttributeColumnHint(*context, QStringLiteral("local"), columnHint);
                if (hasLocalAttribute)
                {
                    removeSymbolLineAttribute(*context, QStringLiteral("local"), targetColumnHint);
                }
                else
                {
                    upsertSymbolLineAttribute(*context, QStringLiteral("local"), std::nullopt, targetColumnHint);
                }
            });

        addAction(
            hasDescAttribute ? QStringLiteral("Edit desc...") : QStringLiteral("Add desc..."),
            QStringLiteral("desc"),
            true,
            [this, context, columnHint]()
            {
                const std::optional<int> targetColumnHint =
                    preferredAttributeColumnHint(*context, QStringLiteral("desc"), columnHint);
                const int sourceRevision = editor_ ? editor_->document()->revision() : -1;
                const std::optional<QString> next = promptDescAttributeValue(
                    symbolLineAttributeValue(*context, QStringLiteral("desc"), targetColumnHint));
                if (!next || !editor_ || editor_->document()->revision() != sourceRevision)
                {
                    return;
                }
                upsertSymbolLineAttribute(*context, QStringLiteral("desc"), *next, targetColumnHint);
            });
        if (hasDescAttribute)
        {
            addAction(
                QStringLiteral("Remove desc"),
                QStringLiteral("desc"),
                true,
                [this, context, columnHint]()
                {
                    const std::optional<int> targetColumnHint =
                        preferredAttributeColumnHint(*context, QStringLiteral("desc"), columnHint);
                    removeSymbolLineAttribute(*context, QStringLiteral("desc"), targetColumnHint);
                });
        }

        if (hasMaskAttribute || supportsTriggerFilterAttributes)
        {
            addAction(
                hasMaskAttribute ? QStringLiteral("Edit mask...") : QStringLiteral("Add mask..."),
                QStringLiteral("mask"),
                true,
                [this, context, columnHint, hasMaskAttribute, popupAnchor]()
                {
                    std::optional<int> targetColumnHint =
                        preferredAttributeColumnHint(*context, QStringLiteral("mask"), columnHint);
                    if ( !targetColumnHint && hasMaskAttribute )
                    {
                        if ( const auto firstMask = symbolLineAttributeMatch(
                                *context, QStringLiteral("mask")) )
                        {
                            targetColumnHint = firstMask->tokenStart;
                        }
                    }
                    const int sourceRevision = editor_
                        ? editor_->document()->revision() : -1;
                    if ( !targetColumnHint || sourceRevision < 0 )
                    {
                        return;
                    }
                    editSymbolLineMask(
                        *context, *targetColumnHint, sourceRevision,
                        !hasMaskAttribute,
                        popupAnchor.value_or(defaultCogPopupAnchor()));
                });
            if (hasMaskAttribute)
            {
                addAction(
                    QStringLiteral("Remove mask"),
                    QStringLiteral("mask"),
                    true,
                    [this, context, columnHint]()
                    {
                        const std::optional<int> targetColumnHint =
                            preferredAttributeColumnHint(*context, QStringLiteral("mask"), columnHint);
                        removeSymbolLineAttribute(*context, QStringLiteral("mask"), targetColumnHint);
                    });
            }
        }

        if (hasLinkIdAttribute || supportsTriggerFilterAttributes)
        {
            addAction(
                hasLinkIdAttribute ? QStringLiteral("Edit linkid...") : QStringLiteral("Add linkid..."),
                QStringLiteral("linkid"),
                hasLinkIdAttribute || !hasNoLinkAttribute,
                [this, context, columnHint]()
                {
                    const std::optional<int> targetColumnHint =
                        preferredAttributeColumnHint(*context, QStringLiteral("linkid"), columnHint);
                    const int sourceRevision = editor_ ? editor_->document()->revision() : -1;
                    const std::optional<QString> next = promptLinkIdAttributeValue(
                        symbolLineAttributeValue(*context, QStringLiteral("linkid"), targetColumnHint));
                    if (!next || !editor_ || editor_->document()->revision() != sourceRevision)
                    {
                        return;
                    }
                    upsertSymbolLineAttribute(*context, QStringLiteral("linkid"), *next, targetColumnHint);
                });
            if (hasLinkIdAttribute)
            {
                addAction(
                    QStringLiteral("Remove linkid"),
                    QStringLiteral("linkid"),
                    true,
                    [this, context, columnHint]()
                    {
                        const std::optional<int> targetColumnHint =
                            preferredAttributeColumnHint(*context, QStringLiteral("linkid"), columnHint);
                        removeSymbolLineAttribute(*context, QStringLiteral("linkid"), targetColumnHint);
                    });
            }
        }

        if (hasNoLinkAttribute || supportsTriggerFilterAttributes)
        {
            addAction(
                hasNoLinkAttribute ? QStringLiteral("Remove nolink") : QStringLiteral("Add nolink"),
                QStringLiteral("nolink"),
                hasNoLinkAttribute || !hasLinkIdAttribute,
                [this, context, columnHint, hasNoLinkAttribute]()
                {
                    const std::optional<int> targetColumnHint =
                        preferredAttributeColumnHint(*context, QStringLiteral("nolink"), columnHint);
                    if (hasNoLinkAttribute)
                    {
                        removeSymbolLineAttribute(*context, QStringLiteral("nolink"), targetColumnHint);
                    }
                    else
                    {
                        upsertSymbolLineAttribute(*context, QStringLiteral("nolink"), std::nullopt, targetColumnHint);
                    }
                });
        }

        return true;
    }

    bool appendContextAwareInsertMenus(
        QMenu* menu,
        const std::optional<int> lineHint,
        const std::optional<int> columnHint = std::nullopt,
        const std::optional<QPoint> popupAnchor = std::nullopt)
    {
        if (!menu)
        {
            return false;
        }

        bool addedActions = false;
        auto addSubmenu = [&](const QString& title, auto populate)
        {
            QMenu* subMenu = ui::shortcuts::addKeySequenceDisplayMenu(menu, title);
            populate(subMenu);
            const bool hasActions = !subMenu->actions().isEmpty();
            subMenu->setEnabled(hasActions);
            subMenu->menuAction()->setVisible(hasActions);
            addedActions = addedActions || hasActions;
        };
        auto addActionItem = [&](const QString& title, auto setup)
        {
            QAction* action = menu->addAction(title);
            setup(action);
            addedActions = true;
        };

        switch (editorInsertContextAtLine(lineHint))
        {
        case EditorInsertContext::symbols:
            addSubmenu(QStringLiteral("Symbol"), [this, lineHint](QMenu* subMenu)
                {
                    populateSymbolInsertMenu(subMenu, lineHint);
                });
            addSubmenu(QStringLiteral("Symbol attribute"), [this, lineHint, columnHint, popupAnchor](QMenu* subMenu)
                {
                    const bool hasAttributeContext = populateAttributeInsertMenu(
                        subMenu, lineHint, columnHint, popupAnchor);
                    subMenu->setEnabled(hasAttributeContext);
                });
            addSubmenu(QStringLiteral("Message"), [this, lineHint](QMenu* subMenu)
                {
                    populateHandlerInsertMenu(subMenu, lineHint);
                });
            addActionItem(QStringLiteral("Code section..."), [this, lineHint](QAction* action)
                {
                    action->setToolTip(QStringLiteral("Insert a custom code label and matching symbol entry."));
                    action->setStatusTip(QStringLiteral("Insert a custom code label and matching symbol entry."));
                    connect(action, &QAction::triggered, this, [this, lineHint]()
                        {
                            insertGenericLabel(lineHint);
                        });
                });
            addSubmenu(QStringLiteral("Section"), [this, lineHint](QMenu* subMenu)
                {
                    populateSectionInsertMenu(subMenu, lineHint);
                });
            addSubmenu(QStringLiteral("Snippet"), [this, lineHint](QMenu* subMenu)
                {
                    populateSnippetInsertMenu(subMenu, lineHint);
                });
            break;
        case EditorInsertContext::code:
            addSubmenu(QStringLiteral("Message"), [this, lineHint](QMenu* subMenu)
                {
                    populateHandlerInsertMenu(subMenu, lineHint);
                });
            addActionItem(QStringLiteral("Code section..."), [this, lineHint](QAction* action)
                {
                    action->setToolTip(QStringLiteral("Insert a custom code label and matching symbol entry."));
                    action->setStatusTip(QStringLiteral("Insert a custom code label and matching symbol entry."));
                    connect(action, &QAction::triggered, this, [this, lineHint]()
                        {
                            insertGenericLabel(lineHint);
                        });
                });
            addSubmenu(QStringLiteral("Snippet"), [this, lineHint](QMenu* subMenu)
                {
                    populateSnippetInsertMenu(subMenu, lineHint);
                });
            addSubmenu(QStringLiteral("Section"), [this, lineHint](QMenu* subMenu)
                {
                    populateSectionInsertMenu(subMenu, lineHint);
                });
            break;
        case EditorInsertContext::flags:
        case EditorInsertContext::outside:
            addSubmenu(QStringLiteral("Section"), [this, lineHint](QMenu* subMenu)
                {
                    populateSectionInsertMenu(subMenu, lineHint);
                });
            addActionItem(QStringLiteral("Code section..."), [this, lineHint](QAction* action)
                {
                    action->setToolTip(QStringLiteral("Insert a custom code label and matching symbol entry."));
                    action->setStatusTip(QStringLiteral("Insert a custom code label and matching symbol entry."));
                    connect(action, &QAction::triggered, this, [this, lineHint]()
                        {
                            insertGenericLabel(lineHint);
                        });
                });
            addSubmenu(QStringLiteral("Snippet"), [this, lineHint](QMenu* subMenu)
                {
                    populateSnippetInsertMenu(subMenu, lineHint);
                });
            break;
        }

        return addedActions;
    }

    bool populateContextAwareInsertMenu(
        QMenu* menu,
        const std::optional<int> lineHint,
        const std::optional<int> columnHint = std::nullopt,
        const std::optional<QPoint> popupAnchor = std::nullopt)
    {
        return appendContextAwareInsertMenus(menu, lineHint, columnHint, popupAnchor);
    }

    void showEditorContextMenu(const QPoint& pos, const QPoint globalAnchor)
    {
        if (!editor_)
        {
            return;
        }

        const int lineHint = viewportLineForPosition(pos);
        QTextCursor cursor = editor_->cursorForPosition(pos);
        editor_->setTextCursor(cursor);
        const int columnHint = cursor.position() - cursor.block().position();
        const std::optional<SymbolLineContext> symbolContext = symbolLineContextAtLine(lineHint);
        const std::optional<SymbolLineAttributeMatch> clickedAttribute =
            symbolContext ? symbolLineAttributeAtColumn(*symbolContext, columnHint) : std::nullopt;
        const bool clickedAttributeValue = clickedAttribute
            && symbolLineAttributeValueHit(*clickedAttribute, columnHint);

        std::unique_ptr<QMenu> nativeMenu(editor_->createStandardContextMenu());
        auto menu = std::make_unique<ui::shortcuts::KeySequenceDisplayMenu>();
        for (QAction* const action : nativeMenu->actions())
        {
            action->setParent(menu.get());
            menu->addAction(action);
        }
        nativeMenu->clear();
        menu->addSeparator();

        QAction* saveAction = menu->addAction("Save");
        connect(saveAction, &QAction::triggered, this, [this]()
            {
                saveScript();
            });

        QAction* reloadAction = menu->addAction("Reload from disk");
        connect(reloadAction, &QAction::triggered, this, [this]()
            {
                reloadFromDisk(false, true);
            });

        QAction* formatAction = menu->addAction("Format");
        connect(formatAction, &QAction::triggered, this, [this]()
            {
                if (formatToolButton_)
                {
                    formatToolButton_->click();
                }
            });

        QAction* definitionAction = menu->addAction("Go to definition");
        connect(definitionAction, &QAction::triggered, this, [this]()
            {
                jumpToDefinitionFromCursor();
            });

        menu->addSeparator();

        QAction* findAction = menu->addAction("Find");
        connect(findAction, &QAction::triggered, this, [this]()
            {
                triggerFindFocus();
            });

        if (editorInsertContextAtLine(lineHint) == EditorInsertContext::flags)
        {
            QAction* flagsAction = menu->addAction(QStringLiteral("Edit COG flags..."));
            flagsAction->setObjectName(QStringLiteral("cogEditorContextFlagsAction"));
            flagsAction->setToolTip(QStringLiteral(
                "Edit the engine-defined flags= bit field for this COG script."));
            connect(flagsAction, &QAction::triggered, this, [this, lineHint, globalAnchor]()
                {
                    editCogScriptFlags(lineHint, globalAnchor);
                });
        }

        if (symbolContext && clickedAttribute)
        {
            QAction* removeAttributeAction = menu->addAction(
                QStringLiteral("Remove %1").arg(clickedAttribute->name));
            connect(removeAttributeAction, &QAction::triggered, this, [this, symbolContext, columnHint, attributeName = clickedAttribute->name]()
                {
                    removeSymbolLineAttribute(*symbolContext, attributeName, columnHint);
                });

            if (symbolLineAttributeCount(*symbolContext, clickedAttribute->name) > 1)
            {
                QAction* dedupeAttributeAction = menu->addAction(
                    QStringLiteral("Keep only this %1").arg(clickedAttribute->name));
                connect(dedupeAttributeAction, &QAction::triggered, this, [this, symbolContext, columnHint, attributeName = clickedAttribute->name]()
                    {
                        dedupeSymbolLineAttribute(*symbolContext, attributeName, columnHint);
                    });
            }
        }

        if (symbolContext
            && clickedAttribute
            && clickedAttribute->name == QStringLiteral("desc"))
        {
            QAction* editDescAction = menu->addAction("Edit desc...");
            connect(editDescAction, &QAction::triggered, this, [this, symbolContext, columnHint]()
                {
                    const int sourceRevision = editor_ ? editor_->document()->revision() : -1;
                    const std::optional<QString> next = promptDescAttributeValue(
                        symbolLineAttributeValue(*symbolContext, QStringLiteral("desc"), columnHint));
                    if (!next || !editor_ || editor_->document()->revision() != sourceRevision)
                    {
                        return;
                    }
                    upsertSymbolLineAttribute(*symbolContext, QStringLiteral("desc"), *next, columnHint);
                });
        }

        if (symbolContext
            && clickedAttribute
            && clickedAttribute->name == QStringLiteral("mask")
            && clickedAttributeValue)
        {
            QAction* editMaskAction = menu->addAction("Edit mask...");
            connect(editMaskAction, &QAction::triggered, this, [this, symbolContext, columnHint, globalAnchor]()
                {
                    const int sourceRevision = editor_
                        ? editor_->document()->revision() : -1;
                    const auto targetColumn = preferredAttributeColumnHint(
                        *symbolContext, QStringLiteral("mask"), columnHint);
                    if ( sourceRevision < 0 || !targetColumn )
                    {
                        return;
                    }
                    editSymbolLineMask(
                        *symbolContext, *targetColumn, sourceRevision, false, globalAnchor);
                });
        }

        if (symbolContext
            && clickedAttribute
            && clickedAttribute->name == QStringLiteral("linkid"))
        {
            QAction* editLinkIdAction = menu->addAction("Edit linkid...");
            connect(editLinkIdAction, &QAction::triggered, this, [this, symbolContext, columnHint]()
                {
                    const int sourceRevision = editor_ ? editor_->document()->revision() : -1;
                    const std::optional<QString> next = promptLinkIdAttributeValue(
                        symbolLineAttributeValue(*symbolContext, QStringLiteral("linkid"), columnHint));
                    if (!next || !editor_ || editor_->document()->revision() != sourceRevision)
                    {
                        return;
                    }

                    upsertSymbolLineAttribute(*symbolContext, QStringLiteral("linkid"), *next, columnHint);
                });
        }

        bool addedDirectInsertMenus = false;
        if (appendContextAwareInsertMenus(
                menu.get(), lineHint, columnHint, globalAnchor))
        {
            addedDirectInsertMenus = true;
        }

        if (addedDirectInsertMenus)
        {
            menu->addSeparator();
        }

        menu->exec(editor_->viewport()->mapToGlobal(pos));
    }

    void insertSection(const QString& sectionName, const std::optional<int> lineHint = std::nullopt)
    {
        if (!editor_)
        {
            return;
        }

        const int insertLine = preferredCogScriptSectionInsertLine(editor_->toPlainText(), sectionName, lineHint);
        if (insertLine < 0)
        {
            return;
        }

        if (sectionName.compare(QStringLiteral("flags"), Qt::CaseInsensitive) == 0)
        {
            insertTextAtLine(insertLine, ui::cog::flagsLineSnippet() + QLatin1Char('\n'));
            return;
        }

        if (sectionName.compare(QStringLiteral("symbols"), Qt::CaseInsensitive) == 0)
        {
            insertTextAtLine(insertLine, ui::cog::sectionSnippet(QStringLiteral("symbols")) + QLatin1Char('\n'));
            return;
        }

        if (sectionName.compare(QStringLiteral("code"), Qt::CaseInsensitive) == 0)
        {
            insertTextAtLine(insertLine, ui::cog::sectionSnippet(QStringLiteral("code")));
        }
    }

    void editCogScriptFlags(
        const std::optional<int> preferredLine = std::nullopt,
        const std::optional<QPoint> popupAnchor = std::nullopt)
    {
        if (!editor_)
        {
            return;
        }

        const QTextCursor sourceCursor = editor_->textCursor();
        const int sourceRevision = editor_->document()->revision();
        const QPointer<CogScriptEditorDialog> ownerGuard(this);
        const QPointer<CogScriptCodeEditor> editorGuard(editor_);
        const QPointer<QTextDocument> documentGuard(editor_->document());

        const ScriptSectionsInfo initialSections = scanSections();
        QString initialLine{};
        ui::cog::CogScriptFlagsValueParseResult parsedInitial{};
        if (initialSections.flagsLine >= 0)
        {
            const QTextBlock block = editor_->document()->findBlockByNumber(
                initialSections.flagsLine);
            initialLine = block.isValid() ? block.text() : QString{};
            const QString code = ui::cog::stripComment(initialLine);
            const int equals = code.indexOf(QLatin1Char('='));
            if (equals >= 0)
            {
                parsedInitial = ui::cog::parseCogScriptFlagsValue(
                    QStringView(code).mid(equals + 1));
            }
        }
        const bool initialValueValid =
            parsedInitial.state == ui::cog::CogScriptFlagsValueParseState::ok;
        const std::uint32_t initialValue = initialValueValid
            ? parsedInitial.value
            : 0u;
        const std::uint32_t supportedBits = ui::cog::cogScriptSupportedFlagBits(projectType_);
        const QList<CogScriptFlagOption> options = ui::cog::cogScriptFlagOptions(projectType_);

        QPointer<QDialog> dialog(new QDialog(
            this, Qt::Popup | Qt::FramelessWindowHint));
        const auto dialogCleanup = qScopeGuard([&dialog]()
        {
            if ( dialog )
            {
                dialog->deleteLater();
            }
        });
        dialog->setObjectName(QStringLiteral("cogEditorFlagsDialog"));
        dialog->setWindowTitle(QStringLiteral("Edit COG Flags"));
        dialog->setModal(true);
        dialog->setMinimumWidth(std::max(
            430,
            ui::minimumDialogWidthForCaption(*dialog)));

        auto* layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(7);

        auto* intro = new QLabel(
            QStringLiteral("Choose the engine flags stored on this COG script."),
            dialog);
        intro->setWordWrap(true);
        layout->addWidget(intro);

        if (initialSections.flagsLine >= 0 && !initialValueValid)
        {
            auto* warning = new QLabel(
                QStringLiteral("The current flags value is invalid. Applying will replace it with a valid value."),
                dialog);
            warning->setObjectName(QStringLiteral("cogEditorFlagsInvalidValueWarning"));
            warning->setWordWrap(true);
            warning->setStyleSheet(QStringLiteral("color: #efb56f;"));
            layout->addWidget(warning);
        }

        struct FlagCheck
        {
            CogScriptFlagOption option{};
            QCheckBox* check{};
        };
        QList<FlagCheck> flagChecks{};
        for (const CogScriptFlagOption& option : options)
        {
            QString label = QStringLiteral("%1 (0x%2)")
                .arg(option.label)
                .arg(option.bit, 0, 16);
            if (option.runtimeManaged)
            {
                label += QStringLiteral(" - runtime managed");
            }
            auto* check = new QCheckBox(label, dialog);
            check->setObjectName(QStringLiteral("cogEditorFlagBit_%1")
                .arg(option.bit, 0, 16));
            check->setChecked((initialValue & option.bit) != 0u);
            check->setEnabled(!option.runtimeManaged);
            check->setToolTip(option.description);
            check->setStatusTip(option.description);
            check->setWhatsThis(option.description);
            layout->addWidget(check);
            flagChecks.push_back(FlagCheck{ option, check });
        }

        const std::uint32_t unknownBits = initialValue & ~supportedBits;
        if (unknownBits != 0u)
        {
            auto* unknown = new QLabel(
                QStringLiteral("Unknown bits 0x%1 are preserved unchanged.")
                    .arg(unknownBits, 0, 16),
                dialog);
            unknown->setObjectName(QStringLiteral("cogEditorFlagsUnknownBits"));
            unknown->setWordWrap(true);
            unknown->setStyleSheet(QStringLiteral("color: rgba(220, 224, 230, 175);"));
            layout->addWidget(unknown);
        }

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

        layout->setSizeConstraint(QLayout::SetFixedSize);
        dialog->adjustSize();
        dialog->setFixedWidth(std::max(dialog->width(), dialog->minimumWidth()));
        positionCogPopupAtAnchor(
            *dialog,
            popupAnchor.value_or(defaultCogPopupAnchor()),
            this);
        const int dialogResult = dialog->exec();
        if ( !ownerGuard || !editorGuard || !documentGuard || !dialog )
        {
            return;
        }
        if ( editor_ != editorGuard.data()
            || editorGuard->document() != documentGuard
            || documentGuard->revision() != sourceRevision
            || editorGuard->isReadOnly() )
        {
            // A replaced source keeps its own cursor and scroll state after the modal closes.
            return;
        }
        if ( dialogResult != QDialog::Accepted )
        {
            editorGuard->setTextCursor(sourceCursor);
            editorGuard->setFocus(Qt::OtherFocusReason);
            return;
        }

        // Preserve bits not represented by the project schema and all
        // runtime-managed bits. Only authorable checkboxes change the value.
        std::uint32_t nextValue = initialValue & ~supportedBits;
        for (const FlagCheck& entry : flagChecks)
        {
            if (entry.option.runtimeManaged)
            {
                nextValue |= initialValue & entry.option.bit;
            }
            else if (entry.check && entry.check->isChecked())
            {
                nextValue |= entry.option.bit;
            }
        }

        if ( initialSections.flagsLine >= 0 && initialValueValid
            && nextValue == initialValue )
        {
            editorGuard->setTextCursor(sourceCursor);
            editorGuard->setFocus(Qt::OtherFocusReason);
            return;
        }

        ScriptSectionsInfo sections = scanSections();
        if (sections.flagsLine < 0)
        {
            const int insertLine = preferredCogScriptSectionInsertLine(
                editor_->toPlainText(),
                QStringLiteral("flags"),
                preferredLine);
            if (insertLine < 0)
            {
                editor_->setTextCursor(sourceCursor);
                editor_->setFocus(Qt::OtherFocusReason);
                return;
            }
            QTextCursor cursor = cursorAtLineStart(insertLine);
            cursor.beginEditBlock();
            cursor.insertText(
                QStringLiteral("flags=")
                    + ui::cog::formatCogScriptFlagsValue(nextValue)
                    + QLatin1Char('\n'));
            cursor.endEditBlock();
            editor_->setTextCursor(cursor);
            editor_->setFocus(Qt::OtherFocusReason);
            return;
        }

        QTextBlock block = editor_->document()->findBlockByNumber(sections.flagsLine);
        if (!block.isValid())
        {
            editor_->setTextCursor(sourceCursor);
            editor_->setFocus(Qt::OtherFocusReason);
            return;
        }
        const QString rawLine = block.text();
        const int commentStart = ui::cog::findCommentStart(rawLine);
        int suffixStart = commentStart;
        while (suffixStart > 0 && rawLine.at(suffixStart - 1).isSpace())
        {
            --suffixStart;
        }
        const QString commentSuffix = commentStart >= 0
            ? rawLine.mid(suffixStart)
            : QString{};
        QString replacement = leadingWhitespace(rawLine)
            + QStringLiteral("flags=")
            + ui::cog::formatCogScriptFlagsValue(nextValue);
        if (!commentSuffix.isEmpty())
        {
            replacement += commentSuffix;
        }

        QTextCursor cursor(block);
        cursor.beginEditBlock();
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
        cursor.insertText(replacement);
        cursor.endEditBlock();
        editor_->setTextCursor(cursor);
        editor_->setFocus(Qt::OtherFocusReason);
    }

    void insertSymbolDeclaration(const QString& typeName, const std::optional<int> preferredLine = std::nullopt)
    {
        if (!editor_)
        {
            return;
        }

        ScriptSectionsInfo sections = scanSections();
        if (sections.symbolsLine < 0)
        {
            insertSection(QStringLiteral("symbols"));
            sections = scanSections();
        }
        const int insertLine = preferredSymbolInsertLine(preferredLine);
        insertTextAtLine(
            insertLine,
            inferredSymbolIndent(insertLine) + ui::cog::symbolSnippet(typeName));
    }

    void ensureMessageSymbol(const QString& messageName, const std::optional<int> preferredLine = std::nullopt)
    {
        if (!editor_)
        {
            return;
        }

        static const QRegularExpression kMessageDeclRx(
            QStringLiteral("^\\s*message\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
        const QString text = editor_->toPlainText();
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (const QString& lineRaw : lines)
        {
            const QString line = ui::cog::stripComment(lineRaw).trimmed();
            const auto match = kMessageDeclRx.match(line);
            if (match.hasMatch() && match.captured(1).compare(messageName, Qt::CaseInsensitive) == 0)
            {
                return;
            }
        }

        ScriptSectionsInfo sections = scanSections();
        if (sections.symbolsLine < 0)
        {
            insertSection(QStringLiteral("symbols"));
            sections = scanSections();
        }
        const int insertLine = preferredMessageSymbolInsertLine(preferredLine);
        insertTextAtLine(
            insertLine,
            inferredSymbolIndent(insertLine) + QStringLiteral("message %1\n").arg(messageName));
    }

    void insertHandler(
        const QString& label,
        const bool ensureMessageDeclaration,
        const std::optional<int> preferredLine = std::nullopt,
        const bool codeBoundaryLine = false)
    {
        if (!editor_)
        {
            return;
        }

        const QString trimmedLabel = label.trimmed();
        if (trimmedLabel.isEmpty())
        {
            return;
        }

        if (const std::optional<int> line = findCogScriptCodeLabelLine(editor_->toPlainText(), trimmedLabel))
        {
            jumpToLine(*line);
            return;
        }

        QTextCursor insertionAnchor = cursorAtLineStart(
            preferredLine.value_or(editor_->textCursor().blockNumber()));
        QTextCursor editGroup(editor_->document());
        if (ensureMessageDeclaration)
        {
            editGroup.beginEditBlock();
        }

        if (ensureMessageDeclaration)
        {
            ensureMessageSymbol(trimmedLabel, preferredLine);
        }

        ScriptSectionsInfo sections = scanSections();
        if (sections.codeLine < 0)
        {
            insertSection(QStringLiteral("code"));
            sections = scanSections();
        }
        const std::optional<int> livePreferredLine = insertionAnchor.block().isValid()
            ? std::optional<int>{ insertionAnchor.blockNumber() }
            : preferredLine;
        const int insertLine = codeBoundaryLine && livePreferredLine
            ? std::clamp(*livePreferredLine, sections.codeLine + 1, sections.codeEndLine)
            : preferredCodeInsertLine(livePreferredLine);
        const QString labelIndent = inferredCodeLabelIndent(insertLine);
        const QString bodyIndent = inferredHandlerBodyIndent(insertLine, labelIndent);
        insertTextAtLine(
            insertLine,
            QStringLiteral("%1%2:\n%3return;\n\n")
                .arg(labelIndent, trimmedLabel, bodyIndent));
        if (ensureMessageDeclaration)
        {
            editGroup.endEditBlock();
        }
    }

    std::optional<QString> promptCodeSectionLabel()
    {
        if (!editor_)
        {
            return std::nullopt;
        }

        const QTextCursor sourceCursor = editor_->textCursor();

        QDialog dialog(this);
        dialog.setObjectName(QStringLiteral("cogEditorCodeSectionDialog"));
        dialog.setWindowTitle(QStringLiteral("Insert COG Code Section"));
        dialog.setModal(true);
        ui::applyFixedDialogWindowPolicy(dialog);
        dialog.setMinimumWidth(std::max(
            380,
            ui::minimumDialogWidthForCaption(dialog)));

        auto* layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(7);

        auto* prompt = new QLabel(QStringLiteral("Code section name"), &dialog);
        layout->addWidget(prompt);

        auto* nameEdit = new QLineEdit(&dialog);
        nameEdit->setObjectName(QStringLiteral("cogEditorCodeSectionNameEdit"));
        nameEdit->setPlaceholderText(QStringLiteral("example_label"));
        nameEdit->setAccessibleName(QStringLiteral("COG code section name"));
        // QLayout::SetFixedSize recomputes the native dialog from child size
        // hints during exec(). Keep the content hint itself wide enough that
        // this pass cannot shrink the caption below its title text.
        nameEdit->setMinimumWidth(396);
        layout->addWidget(nameEdit);

        auto* errorLabel = new QLabel(&dialog);
        errorLabel->setObjectName(QStringLiteral("cogEditorCodeSectionValidation"));
        errorLabel->setStyleSheet(QStringLiteral("color: #ef8d8d;"));
        errorLabel->setMinimumHeight(errorLabel->fontMetrics().height());
        layout->addWidget(errorLabel);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            &dialog);
        layout->addWidget(buttons);
        QPushButton* okButton = buttons->button(QDialogButtonBox::Ok);
        if (okButton)
        {
            okButton->setObjectName(QStringLiteral("cogEditorCodeSectionOkButton"));
        }

        const auto validate = [this, nameEdit, errorLabel, okButton]() -> bool
        {
            static const QRegularExpression kIdentifierRx(
                QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"),
                QRegularExpression::UseUnicodePropertiesOption);
            const QString candidate = nameEdit->text().trimmed();
            QString error{};
            if (candidate.isEmpty())
            {
                error = QStringLiteral("Enter a code section name.");
            }
            else if (!kIdentifierRx.match(candidate).hasMatch())
            {
                error = QStringLiteral("Use letters, digits, and underscores; do not start with a digit.");
            }
            else if (editor_
                && findCogScriptCodeLabelLine(editor_->toPlainText(), candidate).has_value())
            {
                error = QStringLiteral("That code section already exists.");
            }
            errorLabel->setText(error);
            if (okButton)
            {
                okButton->setEnabled(error.isEmpty());
            }
            return error.isEmpty();
        };

        connect(nameEdit, &QLineEdit::textChanged, &dialog,
            [validate](const QString&) { (void)validate(); });
        if (okButton)
        {
            connect(okButton, &QPushButton::clicked, &dialog,
                [&dialog, validate]()
                {
                    if (validate())
                    {
                        dialog.accept();
                    }
                });
        }
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        (void)validate();
        layout->setSizeConstraint(QLayout::SetFixedSize);
        // Re-evaluate caption metrics after child/style polish. Windows can
        // report a wider native title-label rectangle at this point than it
        // did before the dialog contents existed.
        dialog.ensurePolished();
        ui::applyFixedDialogWindowPolicy(dialog);
        dialog.setMinimumWidth(std::max({
            420,
            dialog.minimumWidth(),
            ui::minimumDialogWidthForCaption(dialog),
        }));
        dialog.adjustSize();
        dialog.setFixedWidth(std::max(dialog.width(), dialog.minimumWidth()));
        nameEdit->setFocus(Qt::OtherFocusReason);

        const bool accepted = dialog.exec() == QDialog::Accepted;
        const bool finalValid = accepted && validate();
        editor_->setTextCursor(sourceCursor);
        editor_->setFocus(Qt::OtherFocusReason);
        if (!finalValid)
        {
            return std::nullopt;
        }
        return nameEdit->text().trimmed();
    }

    void insertGenericLabel(const std::optional<int> preferredLine = std::nullopt,
                            const bool codeBoundaryLine = false)
    {
        if (!editor_)
        {
            return;
        }

        QTextCursor insertionAnchor = cursorAtLineStart(
            preferredLine.value_or(editor_->textCursor().blockNumber()));
        const int sourceRevision = editor_->document()->revision();
        const std::optional<QString> requestedLabel = promptCodeSectionLabel();
        if (!requestedLabel)
        {
            return;
        }
        if (editor_->document()->revision() != sourceRevision)
        {
            return;
        }
        const QString label = requestedLabel->trimmed();

        if (const std::optional<int> line = findCogScriptCodeLabelLine(editor_->toPlainText(), label))
        {
            jumpToLine(*line);
            return;
        }

        QTextCursor editGroup(editor_->document());
        editGroup.beginEditBlock();

        if (!findCogScriptSymbolLine(editor_->toPlainText(), label))
        {
            ScriptSectionsInfo sections = scanSections();
            if (sections.symbolsLine < 0)
            {
                insertSection(QStringLiteral("symbols"));
                sections = scanSections();
            }
            const int insertLine = preferredSymbolInsertLine(
                insertionAnchor.block().isValid()
                    ? std::optional<int>{ insertionAnchor.blockNumber() }
                    : preferredLine);
            insertTextAtLine(
                insertLine,
                inferredSymbolIndent(insertLine)
                    + QStringLiteral("flex %1=0 local\n").arg(label));
        }

        insertHandler(
            label,
            false,
            insertionAnchor.block().isValid()
                ? std::optional<int>{ insertionAnchor.blockNumber() }
                : preferredLine,
            codeBoundaryLine);
        editGroup.endEditBlock();
    }

    void insertSnippet(const QString& text)
    {
        if (!editor_ || text.isEmpty())
        {
            return;
        }

        QTextCursor cursor = editor_->textCursor();
        if (!cursor.atBlockStart() && !cursor.block().text().isEmpty())
        {
            cursor.insertText(QStringLiteral("\n"));
        }
        cursor.insertText(text);
        editor_->setTextCursor(cursor);
        editor_->setFocus(Qt::OtherFocusReason);
    }

    void updateFindStatus(const QString& text, const bool error = false)
    {
        if (!findStatusLabel_)
        {
            return;
        }

        findStatusLabel_->setText(text);
        findStatusLabel_->setStyleSheet(
            error
                ? "color: rgba(245, 120, 120, 230); font-size: 11px;"
                : "color: rgba(235, 236, 240, 175); font-size: 11px;");
    }

    QTextDocument::FindFlags findFlags(const bool backward = false) const
    {
        QTextDocument::FindFlags flags{};
        if (backward)
        {
            flags |= QTextDocument::FindBackward;
        }
        if (findCaseSensitiveCheck_ && findCaseSensitiveCheck_->isChecked())
        {
            flags |= QTextDocument::FindCaseSensitively;
        }
        if (findWholeWordCheck_ && findWholeWordCheck_->isChecked())
        {
            flags |= QTextDocument::FindWholeWords;
        }
        return flags;
    }

    std::vector<ui::cog::CogScriptTextMatch> currentFindMatches() const
    {
        if (!editor_ || !findEdit_ || findEdit_->text().isEmpty())
        {
            return {};
        }
        return findCogScriptTextMatches(
            *editor_->document(),
            findEdit_->text(),
            findCaseSensitiveCheck_ && findCaseSensitiveCheck_->isChecked(),
            findWholeWordCheck_ && findWholeWordCheck_->isChecked());
    }

    void setFindNavigationEnabled(const bool enabled)
    {
        if (findPreviousButton_)
        {
            findPreviousButton_->setEnabled(enabled);
        }
        if (findNextButton_)
        {
            findNextButton_->setEnabled(enabled);
        }
    }

    void clearFindMatchSelections()
    {
        activeFindMatchPosition_ = -1;
        if (!findMatchSelections_.isEmpty())
        {
            findMatchSelections_.clear();
            applyEditorSourceSelections();
        }
    }

    int findMatchIndex(
        const std::vector<ui::cog::CogScriptTextMatch>& matches,
        const std::optional<int> preferredPosition = std::nullopt) const
    {
        if (!editor_ || matches.empty())
        {
            return -1;
        }

        if (preferredPosition)
        {
            for (std::size_t i = 0; i < matches.size(); ++i)
            {
                if (matches[i].position == *preferredPosition)
                {
                    return static_cast<int>(i);
                }
            }
        }

        const QTextCursor current = editor_->textCursor();
        if (current.hasSelection())
        {
            for (std::size_t i = 0; i < matches.size(); ++i)
            {
                if (matches[i].position == current.selectionStart()
                    && matches[i].position + matches[i].length
                        == current.selectionEnd())
                {
                    return static_cast<int>(i);
                }
            }
        }

        if (activeFindMatchPosition_ >= 0)
        {
            for (std::size_t i = 0; i < matches.size(); ++i)
            {
                if (matches[i].position == activeFindMatchPosition_)
                {
                    return static_cast<int>(i);
                }
            }
        }

        const auto next = std::ranges::find_if(
            matches,
            [this](const ui::cog::CogScriptTextMatch& match)
            {
                return match.position >= findAnchorPosition_;
            });
        return next == matches.end()
            ? 0
            : static_cast<int>(std::distance(matches.begin(), next));
    }

    void presentFindMatches(
        const std::vector<ui::cog::CogScriptTextMatch>& matches,
        const std::optional<int> preferredPosition = std::nullopt)
    {
        findMatchSelections_.clear();
        if (!editor_ || !findRow_ || findRow_->isHidden()
            || !findEdit_ || findEdit_->text().isEmpty())
        {
            activeFindMatchPosition_ = -1;
            updateFindStatus(QString());
            applyEditorSourceSelections();
            return;
        }
        if (matches.empty())
        {
            activeFindMatchPosition_ = -1;
            updateFindStatus(QStringLiteral("No results"), true);
            applyEditorSourceSelections();
            return;
        }

        const int currentIndex = std::clamp(
            findMatchIndex(matches, preferredPosition),
            0,
            static_cast<int>(matches.size()) - 1);
        activeFindMatchPosition_ = matches[static_cast<std::size_t>(currentIndex)].position;
        findMatchSelections_.reserve(static_cast<int>(matches.size()));
        for (std::size_t i = 0; i < matches.size(); ++i)
        {
            const ui::cog::CogScriptTextMatch& match = matches[i];
            QTextCursor cursor(editor_->document());
            cursor.setPosition(match.position);
            cursor.setPosition(match.position + match.length, QTextCursor::KeepAnchor);
            QTextEdit::ExtraSelection selection{};
            selection.cursor = cursor;
            selection.format.setBackground(
                static_cast<int>(i) == currentIndex
                    ? currentTheme_.findCurrentMatchBackground
                    : currentTheme_.findMatchBackground);
            findMatchSelections_.push_back(selection);
        }
        updateFindStatus(QStringLiteral("%1 of %2")
            .arg(currentIndex + 1)
            .arg(static_cast<int>(matches.size())));
        applyEditorSourceSelections();
    }

    QTextCursor findFromPosition(
        const QString& needle,
        const int position,
        const bool forward) const
    {
        if (!editor_ || needle.isEmpty())
        {
            return {};
        }

        QTextCursor start(editor_->document());
        start.setPosition(std::clamp(position, 0, editor_->document()->characterCount() - 1));
        QTextCursor match = editor_->document()->find(needle, start, findFlags(!forward));
        if (!match.isNull())
        {
            return match;
        }

        start.movePosition(forward ? QTextCursor::Start : QTextCursor::End);
        match = editor_->document()->find(needle, start, findFlags(!forward));
        return match;
    }

    void updateFindNavigationAvailability()
    {
        const std::vector<ui::cog::CogScriptTextMatch> matches =
            currentFindMatches();
        setFindNavigationEnabled(!matches.empty());
        presentFindMatches(matches);
    }

    bool findIncrementally()
    {
        if (!editor_ || !findEdit_)
        {
            return false;
        }

        const QString needle = findEdit_->text();
        if (needle.isEmpty())
        {
            setFindNavigationEnabled(false);
            clearFindMatchSelections();
            updateFindStatus(QString());
            return false;
        }

        const std::vector<ui::cog::CogScriptTextMatch> matches =
            currentFindMatches();
        setFindNavigationEnabled(!matches.empty());

        const QTextCursor match = findFromPosition(
            needle,
            findAnchorPosition_,
            true);
        if (match.isNull())
        {
            presentFindMatches(matches);
            return false;
        }

        editor_->setTextCursor(match);
        presentFindMatches(matches, match.selectionStart());
        return true;
    }

    bool findInEditor(const bool forward)
    {
        if (!editor_ || !findEdit_)
        {
            return false;
        }

        const QString needle = findEdit_->text();
        if (needle.isEmpty())
        {
            setFindNavigationEnabled(false);
            clearFindMatchSelections();
            updateFindStatus(QString());
            return false;
        }

        const std::vector<ui::cog::CogScriptTextMatch> matches =
            currentFindMatches();
        setFindNavigationEnabled(!matches.empty());

        const QTextCursor current = editor_->textCursor();
        const int startPosition = current.hasSelection()
            ? (forward ? current.selectionEnd() : current.selectionStart())
            : current.position();
        const QTextCursor match = findFromPosition(
            needle,
            startPosition,
            forward);
        if (!match.isNull())
        {
            editor_->setTextCursor(match);
            presentFindMatches(matches, match.selectionStart());
            return true;
        }

        // Search on temporary QTextCursor values so a complete miss never
        // destroys the user's source selection/cursor or marks the buffer.
        presentFindMatches(matches);
        return false;
    }

    void applyEditorSourceSelections()
    {
        if (!editor_)
        {
            return;
        }
        QList<QTextEdit::ExtraSelection> selections =
            identifierReferenceSelections_;
        selections.reserve(
            identifierReferenceSelections_.size()
            + findMatchSelections_.size()
            + diagnosticSelections_.size());
        for (const QTextEdit::ExtraSelection& match : findMatchSelections_)
        {
            // Find results sit above ordinary identifier references, while
            // Diagnostics remain authoritative for issue decoration.
            selections.push_back(match);
        }
        for (const QTextEdit::ExtraSelection& diagnostic : diagnosticSelections_)
        {
            // Diagnostics are appended last so their wave underline and
            // severity tint remain authoritative where the ranges overlap.
            selections.push_back(diagnostic);
        }
        editor_->setDiagnosticSelections(selections);
    }

    void scheduleDerivedStateRebuild()
    {
        if (derivedStateTimer_)
        {
            derivedStateTimer_->start();
        }
    }

    void rebuildDerivedStateNow()
    {
        if (!editor_)
        {
            return;
        }
        if (derivedStateTimer_)
        {
            derivedStateTimer_->stop();
        }

        if (!documentDerivedStateDirty_ && !identifierReferencesDirty_)
        {
            return;
        }

        // QString is implicitly shared, and taking one document snapshot
        // avoids separate toPlainText() traversals for large scripts.
        const QString source = editor_->toPlainText();
        if (documentDerivedStateDirty_)
        {
            rebuildOutline(source);
            rebuildDiagnostics(source);
            documentDerivedStateDirty_ = false;
        }
        if (identifierReferencesDirty_)
        {
            rebuildIdentifierReferenceSelections(source);
            identifierReferencesDirty_ = false;
        }
    }

    void clearIdentifierReferenceSelections()
    {
        if (!identifierReferenceSelections_.isEmpty())
        {
            identifierReferenceSelections_.clear();
            applyEditorSourceSelections();
        }
    }

    void rebuildIdentifierReferenceSelections(const QString& source)
    {
        identifierReferenceSelections_.clear();
        if (!editor_)
        {
            return;
        }

        const QTextCursor selectedCursor = editor_->textCursor();
        if (!selectedCursor.hasSelection())
        {
            applyEditorSourceSelections();
            return;
        }
        const QString identifier = selectedCursor.selectedText();
        const std::vector<ui::cog::CogScriptIdentifierOccurrence> occurrences =
            findCogScriptIdentifierOccurrences(source, identifier);
        if (occurrences.empty())
        {
            applyEditorSourceSelections();
            return;
        }

        identifierReferenceSelections_.reserve(
            static_cast<int>(occurrences.size()));
        for (const ui::cog::CogScriptIdentifierOccurrence& occurrence : occurrences)
        {
            QTextCursor cursor(editor_->document());
            cursor.setPosition(occurrence.position);
            cursor.setPosition(
                occurrence.position + occurrence.length,
                QTextCursor::KeepAnchor);
            QTextEdit::ExtraSelection selection{};
            selection.cursor = cursor;
            selection.format.setBackground(
                currentTheme_.identifierReferenceBackground);
            identifierReferenceSelections_.push_back(selection);
        }
        applyEditorSourceSelections();
    }

    void rebuildOutline(const QString& source)
    {
        if (!editor_ || !symbolOutlineList_ || !codeOutlineList_)
        {
            return;
        }

        const QTextCursor cursor = editor_->textCursor();
        const int currentLine = cursor.block().blockNumber();
        const std::vector<CogScriptOutlineEntry> entries =
            buildCogScriptOutline(source);

        const QSignalBlocker symbolBlocker(symbolOutlineList_);
        const QSignalBlocker codeBlocker(codeOutlineList_);
        symbolOutlineList_->clear();
        codeOutlineList_->clear();

        QListWidget* bestList = nullptr;
        int bestRow = -1;
        int bestLine = -1;
        for (const CogScriptOutlineEntry& entry : entries)
        {
            // The two panel labels already communicate their section, so the
            // corresponding source-header rows would be redundant. Keep the
            // top-level flags row visible in Symbols.
            if (entry.section
                && (entry.label == QStringLiteral("Symbols")
                    || entry.label == QStringLiteral("Code")))
            {
                continue;
            }

            QListWidget* list = entry.group == CogScriptOutlineGroup::code
                ? codeOutlineList_
                : symbolOutlineList_;
            auto* item = new QListWidgetItem(entry.label.trimmed(), list);
            item->setData(Qt::UserRole, entry.line);
            if (entry.section)
            {
                item->setForeground(QColor(145, 196, 255));
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
            }

            if (entry.line <= currentLine && entry.line >= bestLine)
            {
                bestLine = entry.line;
                bestList = list;
                bestRow = list->row(item);
            }
        }

        symbolOutlineList_->clearSelection();
        codeOutlineList_->clearSelection();
        if (bestList && bestRow >= 0 && bestRow < bestList->count())
        {
            bestList->setCurrentRow(bestRow);
        }
    }

    void rebuildDiagnostics(const QString& source)
    {
        if (!editor_ || !issuesList_)
        {
            return;
        }

        const std::vector<CogScriptDiagnostic> projectDiagnostics = buildCogScriptDiagnostics(
            source,
            cogScriptProjectMessageWords(projectType_),
            projectType_);
        const auto& finalDiagnostics = projectDiagnostics;
        QSignalBlocker blocker(issuesList_);
        issuesList_->clear();

        diagnosticSelections_.clear();
        for (const CogScriptDiagnostic& diagnostic : finalDiagnostics)
        {
            if (diagnostic.line < 0)
            {
                continue;
            }

            QTextBlock block = editor_->document()->findBlockByNumber(diagnostic.line);
            if (!block.isValid())
            {
                continue;
            }

            if ((diagnostic.column < 0 || block.text().isEmpty())
                && block.text().trimmed().isEmpty())
            {
                const QTextBlock previousBlock = block.previous();
                if (previousBlock.isValid() && !previousBlock.text().trimmed().isEmpty())
                {
                    block = previousBlock;
                }
            }

            const QString blockText = block.text();
            int column = diagnostic.column;
            int length = diagnostic.length;
            if (column < 0 || column >= blockText.size())
            {
                column = 0;
                length = std::max(1, block.length() - 1);
            }
            else if (length <= 0)
            {
                length = 1;
            }

            QTextCursor cursor(block);
            cursor.setPosition(block.position() + column);
            cursor.setPosition(
                std::min(block.position() + static_cast<int>(blockText.size()), block.position() + column + length),
                QTextCursor::KeepAnchor);

            QTextEdit::ExtraSelection sel{};
            sel.cursor = cursor;
            sel.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
            sel.format.setUnderlineColor(diagnostic.error ? QColor(255, 86, 86) : QColor(245, 195, 110));
            sel.format.setBackground(diagnostic.error ? QColor(255, 60, 60, 28) : QColor(245, 190, 95, 20));
            diagnosticSelections_.push_back(sel);
        }
        applyEditorSourceSelections();
        editor_->setDiagnosticEntries(finalDiagnostics);

        int errorCount = 0;
        int warningCount = 0;
        for (const CogScriptDiagnostic& diagnostic : finalDiagnostics)
        {
            QString text = diagnostic.message;
            if (diagnostic.line >= 0)
            {
                text = QStringLiteral("L%1:C%2: %3")
                    .arg(diagnostic.line + 1)
                    .arg(std::max(0, diagnostic.column) + 1)
                    .arg(text);
            }
            auto* item = new QListWidgetItem(text, issuesList_);
            item->setData(Qt::UserRole, diagnostic.line);
            item->setData(Qt::UserRole + 1, std::max(0, diagnostic.column));
            item->setData(Qt::UserRole + 2, diagnostic.length);
            item->setData(Qt::UserRole + 3, text);
            item->setIcon(style()->standardIcon(
                diagnostic.error ? QStyle::SP_MessageBoxCritical : QStyle::SP_MessageBoxWarning));
            item->setForeground(diagnostic.error ? QColor(255, 142, 142) : QColor(245, 195, 110));
            if (diagnostic.error)
            {
                ++errorCount;
            }
            else
            {
                ++warningCount;
            }
        }
        if (diagnosticsErrorCount_)
        {
            diagnosticsErrorCount_->setText(QString::number(errorCount));
        }
        if (diagnosticsWarningCount_)
        {
            diagnosticsWarningCount_->setText(QString::number(warningCount));
        }
    }

    void jumpToOutlineItem(QListWidgetItem* item)
    {
        if (!editor_ || !item)
        {
            return;
        }

        bool ok = false;
        const int line = item->data(Qt::UserRole).toInt(&ok);
        if (!ok || line < 0)
        {
            return;
        }

        jumpToLine(line);
    }

    void jumpToDiagnosticItem(QListWidgetItem* item)
    {
        if (!editor_ || !item)
        {
            return;
        }

        bool ok = false;
        const int line = item->data(Qt::UserRole).toInt(&ok);
        if (!ok || line < 0)
        {
            return;
        }

        const int column = std::max(0, item->data(Qt::UserRole + 1).toInt());
        const int length = std::max(1, item->data(Qt::UserRole + 2).toInt());
        const QTextBlock block = editor_->document()->findBlockByNumber(line);
        if (!block.isValid())
        {
            return;
        }
        const int safeColumn = std::clamp(column, 0, std::max(0, block.length() - 1));
        const int safeLength = std::clamp(
            length,
            1,
            std::max(1, block.length() - 1 - safeColumn));
        QTextCursor cursor(block);
        cursor.setPosition(block.position() + safeColumn);
        cursor.setPosition(block.position() + safeColumn + safeLength, QTextCursor::KeepAnchor);
        editor_->setTextCursor(cursor);
        editor_->centerCursor();
        editor_->setFocus(Qt::OtherFocusReason);
    }

    bool saveScript()
    {
        if (!editor_)
        {
            return false;
        }

        std::filesystem::path savePath = path_;
        if (draft_)
        {
            const std::optional<std::filesystem::path> resolved =
                draftSavePathResolver_ ? draftSavePathResolver_() : std::nullopt;
            if (!resolved || resolved->empty())
            {
                (void)showCogEditorMessageBox(
                    this,
                    QMessageBox::Information,
                    "Save COG script",
                    "Save the project first so the script can be written to its COG folder.");
                return false;
            }
            savePath = *resolved;

            const QString saveIdentity = ui::recovery::normalizedSourcePath(savePath);
            const auto otherBufferAlreadyOpen = [this, saveIdentity]()
            {
                QTabWidget* owningTabs = nullptr;
                MainWindow* owningWindow = nullptr;
                for ( QWidget* ancestor = parentWidget(); ancestor;
                      ancestor = ancestor->parentWidget() )
                {
                    if ( !owningTabs )
                    {
                        owningTabs = qobject_cast<QTabWidget*>(ancestor);
                    }
                    if ( auto* window = dynamic_cast<MainWindow*>(ancestor) )
                    {
                        owningWindow = window;
                        break;
                    }
                }
                const QList<QTabWidget*> hosts = owningWindow
                    ? owningWindow->findChildren<QTabWidget*>()
                    : QList<QTabWidget*>{ owningTabs };
                if ( !saveIdentity.isEmpty() )
                {
                    for ( QTabWidget* tabs : hosts )
                    {
                        if ( !tabs || (owningWindow
                            && !tabs->property("sed.cogScriptEditorHost").toBool()) )
                        {
                            continue;
                        }
                        for ( int index = 0; index < tabs->count(); ++index )
                        {
                            auto* other = dynamic_cast<CogScriptEditorDialog*>(
                                tabs->widget(index));
                            if ( other && other != this
                                && ui::recovery::normalizedSourcePath(
                                    other->scriptPath()) == saveIdentity )
                            {
                                return true;
                            }
                        }
                    }
                }
                return false;
            };
            if ( otherBufferAlreadyOpen() )
            {
                (void)showCogEditorMessageBox(
                    this,
                    QMessageBox::Information,
                    "Save COG script",
                    "This script path is already open in another COG tab. Close or resolve that buffer before saving this draft.");
                return false;
            }

            std::error_code existsError{};
            const bool savePathExists = std::filesystem::exists(
                savePath, existsError);
            if (existsError)
            {
                const QString qPath = QString::fromStdWString(
                    savePath.wstring());
                showCogEditorResourceFailure(
                    this,
                    "Save COG script",
                    QString("Couldn't inspect the script path:\n%1")
                        .arg(qPath),
                    QStringLiteral(
                        "COG script save failed: couldn't inspect '%1' for buffer '%2': %3")
                        .arg(
                            qPath,
                            scriptName_,
                            QString::fromStdString(existsError.message())),
                    CogEditorFailureSeverity::error);
                return false;
            }
            if (savePathExists)
            {
                QPointer<CogScriptEditorDialog> self(this);
                QPointer<CogScriptCodeEditor> editor(editor_);
                const int documentRevision = editor->document()->revision();
                const QString authoredText = editor->toPlainText();
                const QString authoredName = scriptName_;
                const std::filesystem::path authoredPath = path_;
                const auto overwrite = showCogEditorMessageBox(
                    this,
                    QMessageBox::Question,
                    "Save COG script",
                    QString("A script already exists at:\n%1\n\nReplace it?")
                        .arg(QString::fromStdWString(savePath.wstring())),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No,
                    cogBufferLoggingContext(scriptName_, savePath),
                    false,
                    true);
                if ( overwrite != QMessageBox::Yes || !self || !editor
                    || self->editor_ != editor.data() || self->scriptName_ != authoredName
                    || self->path_ != authoredPath || !self->draft_
                    || editor->document()->revision() != documentRevision
                    || editor->toPlainText() != authoredText )
                {
                    return false;
                }
                const std::optional<std::filesystem::path> currentTarget =
                    self->draftSavePathResolver_ ? self->draftSavePathResolver_() : std::nullopt;
                if ( !self || !currentTarget
                    || ui::recovery::normalizedSourcePath(*currentTarget) != saveIdentity )
                {
                    return false;
                }
                if ( otherBufferAlreadyOpen() )
                {
                    (void)showCogEditorMessageBox(
                        this,
                        QMessageBox::Information,
                        "Save COG script",
                        "This script path is already open in another COG tab. Close or resolve that buffer before saving this draft.");
                    return false;
                }
            }
        }

        if ( !draft_ && !savePath.empty() )
        {
            const QByteArray baselineHash = acceptedSourceHash_;
            const CogScriptSourceReadResult currentSource = readCogScriptSourceBounded(savePath);
            const QByteArray currentHash = currentSource.succeeded()
                ? QCryptographicHash::hash(currentSource.bytes, QCryptographicHash::Sha256)
                : QByteArray{};
            if ( baselineHash.isEmpty() || currentHash != baselineHash )
            {
                QPointer<CogScriptEditorDialog> self(this);
                QPointer<CogScriptCodeEditor> editor(editor_);
                if ( !editor )
                {
                    return false;
                }
                QPointer<QTextDocument> document(editor->document());
                if ( !document )
                {
                    return false;
                }
                const int documentRevision = document->revision();
                const QString authoredText = editor->toPlainText();
                const QTextCursor authoredCursor = editor->textCursor();
                const int authoredCursorPosition = authoredCursor.position();
                const int authoredCursorAnchor = authoredCursor.anchor();
                const bool authoredModified = document->isModified();
                const QCborMap authoredHistory = documentHistory_->capture();
                const QString authoredName = scriptName_;
                const std::filesystem::path authoredPath = path_;
                const std::filesystem::path authoredSavePath = savePath;
                const auto stillCurrent = looseWatchStillCurrent_;
                const auto sourceContextIsCurrent = [&stillCurrent]()
                {
                    return !stillCurrent || stillCurrent();
                };
                if ( !sourceContextIsCurrent() )
                {
                    return false;
                }

                const CogExternalSaveConflictDecision decision = showCogExternalSaveConflictPrompt(
                    this,
                    cogBufferLoggingContext(scriptName_, savePath));
                const bool documentIsUnchanged = self && editor && document
                    && self->editor_ == editor.data()
                    && editor->document() == document.data()
                    && self->scriptName_ == authoredName
                    && self->path_ == authoredPath
                    && !self->draft_
                    && savePath == authoredSavePath
                    && self->acceptedSourceHash_ == baselineHash
                    && document->revision() == documentRevision
                    && editor->toPlainText() == authoredText
                    && document->isModified() == authoredModified
                    && editor->textCursor().position() == authoredCursorPosition
                    && editor->textCursor().anchor() == authoredCursorAnchor
                    && self->documentHistory_->capture() == authoredHistory
                    && sourceContextIsCurrent()
                    && (!self->looseWatchStillCurrent_ || self->looseWatchStillCurrent_());
                if ( !documentIsUnchanged )
                {
                    return false;
                }
                if ( decision == CogExternalSaveConflictDecision::reload )
                {
                    const CogScriptSourceReadResult latestSource = readCogScriptSourceBounded(savePath);
                    if ( !latestSource.succeeded() )
                    {
                        const QString qPath = QString::fromStdWString(savePath.wstring());
                        const QString detail = cogScriptSourceReadError(latestSource, qPath);
                        showCogEditorResourceFailure(
                            this,
                            "Reload COG script",
                            detail,
                            QStringLiteral("COG script reload failed after a Save conflict for '%1': %2")
                                .arg(qPath, detail),
                            CogEditorFailureSeverity::warning);
                        return false;
                    }
                    const QByteArray latestHash = QCryptographicHash::hash(
                        latestSource.bytes, QCryptographicHash::Sha256);
                    (void)applyReloadedText(QString::fromUtf8(latestSource.bytes), latestHash);
                    return false;
                }
                if ( decision != CogExternalSaveConflictDecision::overwrite )
                {
                    return false;
                }
            }
        }

        if (formatOnSave_)
        {
            // Formatting is a normal document edit, so it remains part of
            // the draft/recovery state and is included in the same atomic
            // save transaction.  No formatting is attempted for a disabled
            // option or while an external reload is being applied.
            applyFormatting();
        }
        const QByteArray bytes = editor_->toPlainText().toUtf8();
        std::error_code mkEc;
        std::filesystem::create_directories(savePath.parent_path(), mkEc);
        if (mkEc)
        {
            const QString qDirectory = QString::fromStdWString(
                savePath.parent_path().wstring());
            showCogEditorResourceFailure(
                this,
                "Save COG script",
                QString("Couldn't create the script directory:\n%1")
                    .arg(qDirectory),
                QStringLiteral(
                    "COG script save failed: couldn't create directory '%1' for buffer '%2' at '%3': %4")
                    .arg(
                        qDirectory,
                        scriptName_,
                        QString::fromStdWString(savePath.wstring()),
                        QString::fromStdString(mkEc.message())),
                CogEditorFailureSeverity::error);
            return false;
        }

        selfSaving_ = true;
        QSaveFile out(QString::fromStdWString(savePath.wstring()));
        out.setDirectWriteFallback(false);
        if (!out.open(QIODevice::WriteOnly))
        {
            selfSaving_ = false;
            const QString qPath = QString::fromStdWString(savePath.wstring());
            showCogEditorResourceFailure(
                this,
                "Save COG script",
                QString("Couldn't write script:\n%1").arg(qPath),
                QStringLiteral(
                    "COG script save failed: couldn't open '%1' for buffer '%2': %3")
                    .arg(qPath, scriptName_, out.errorString()),
                CogEditorFailureSeverity::error);
            return false;
        }

        if (out.write(bytes) != bytes.size() || !out.commit())
        {
            const QString saveError = out.errorString();
            out.cancelWriting();
            selfSaving_ = false;
            const QString qPath = QString::fromStdWString(savePath.wstring());
            showCogEditorResourceFailure(
                this,
                "Save COG script",
                QString("Couldn't atomically replace script:\n%1\n\n%2")
                    .arg(qPath, saveError),
                QStringLiteral(
                    "COG script atomic save failed for buffer '%1' at '%2': %3")
                    .arg(scriptName_, qPath, saveError),
                CogEditorFailureSeverity::error);
            return false;
        }
        // Retire queued observations of our own atomic write before any UI
        // callback can enter an event loop. A first draft Save registers below.
        acceptedSourceHash_ = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
        ui::acceptLooseDocumentSource(this, QString::fromStdWString(savePath.wstring()),
            acceptedSourceHash_);
        if (draft_)
        {
            const std::filesystem::path oldPath = path_;
            path_ = savePath;
            draft_ = false;
            if (scriptPathChanged_)
            {
                scriptPathChanged_(this, oldPath, path_);
            }
        }
        setProperty("cogFullPath", QString::fromStdWString(path_.wstring()));
        setProperty("cogLooseFile", true);
        setProperty("cogArchiveTooltip", QString{});
        lastSavedText_ = editor_->toPlainText();
        documentHistory_->saved();
        editor_->document()->setModified(false);
        updateWindowTitle();
        // Accept the committed bytes before any queued filesystem delivery.
        selfSaving_ = false;
        ensureWatchPaths(acceptedSourceHash_);

        const SaveCallback onSave = onSave_;
        const std::filesystem::path savedPath = path_;
        if ( onSave )
        {
            onSave(savedPath);
        }
        return true;
    }

    bool confirmClose()
    {
        lastCloseDiscarded_ = false;
        if (!editor_ || !editor_->document()->isModified())
        {
            return true;
        }

        QPointer<CogScriptEditorDialog> self(this);
        QPointer<CogScriptCodeEditor> editor(editor_);
        const int documentRevision = editor->document()->revision();
        const QString authoredText = editor->toPlainText();
        const QString scriptName = scriptName_;
        const std::filesystem::path scriptPath = path_;
        const auto answer = ui::components::showSharedUnsavedChangesPrompt(
            this,
            QStringLiteral("Unsaved COG script"),
            scriptName,
            cogBufferLoggingContext(scriptName, scriptPath));
        if (!self)
        {
            return false;
        }
        if (!editor || self->editor_ != editor.data() || self->scriptName_ != scriptName || self->path_ != scriptPath
            || editor->document()->revision() != documentRevision || editor->toPlainText() != authoredText)
        {
            return false;
        }
        if (answer == QMessageBox::Cancel)
        {
            return false;
        }
        if (answer == QMessageBox::Save)
        {
            const bool saved = self->saveScript();
            return self && saved;
        }
        if (answer != QMessageBox::Discard)
        {
            return false;
        }
        const AuthoredBufferResolvedCallback onAuthoredBufferResolved =
            self->onAuthoredBufferResolved_;
        self->lastCloseDiscarded_ = true;
        if (onAuthoredBufferResolved)
        {
            onAuthoredBufferResolved(scriptPath);
        }
        return self != nullptr;
    }

    QString scriptName_{};
    std::filesystem::path path_{};
    ProjectType projectType_{ProjectType::JKDF2};
    QString lastSavedText_{};
    QByteArray acceptedSourceHash_{};
    SaveCallback onSave_{};
    AuthoredBufferResolvedCallback onAuthoredBufferResolved_{};
    StateChangedCallback onStateChanged_{};
    mutable int shortcutInsertContextsRevision_{-1};
    mutable std::vector<CogScriptInsertContext> shortcutInsertContexts_{};
    AuthoredBufferChangedCallback onAuthoredBufferChanged_{};
    TitleChangedCallback titleChanged_{};
    CogScriptCodeEditor* editor_{};
    ui::cog::CogDocumentHistory* documentHistory_{};
    QSplitter* editorRow_{};
    QSplitter* outlineSplitter_{};
    QSplitter* editorColumn_{};
    QToolButton* undoToolButton_{};
    QToolButton* redoToolButton_{};
    QToolButton* whitespaceToolButton_{};
    QToolButton* formatToolButton_{};
    QToolButton* insertToolButton_{};
    QWidget* editorAccessoryRow_{};
    QComboBox* zoomCombo_{};
    CogScriptSyntaxHighlighter* highlighter_{};
    QWidget* findRow_{};
    QLineEdit* findEdit_{};
    QToolButton* findClearButton_{};
    QCheckBox* findCaseSensitiveCheck_{};
    QCheckBox* findWholeWordCheck_{};
    QToolButton* findPreviousButton_{};
    QToolButton* findNextButton_{};
    QToolButton* findCloseButton_{};
    QLabel* findStatusLabel_{};
    QListWidget* symbolOutlineList_{};
    QListWidget* codeOutlineList_{};
    QListWidget* issuesList_{};
    QWidget* diagnosticsPanel_{};
    QWidget* diagnosticsHeader_{};
    QToolButton* diagnosticsToggle_{};
    QLabel* diagnosticsArrow_{};
    QLabel* diagnosticsErrorCount_{};
    QLabel* diagnosticsWarningCount_{};
    QList<QTextEdit::ExtraSelection> diagnosticSelections_{};
    QList<QTextEdit::ExtraSelection> identifierReferenceSelections_{};
    QList<QTextEdit::ExtraSelection> findMatchSelections_{};
    QTimer* derivedStateTimer_{};
    QVariantAnimation* diagnosticsAnimation_{};
    bool suppressDiagnosticsSplitterState_{};
    bool diagnosticsCommittedExpanded_{true};
    QFont baseEditorFont_{};
    QFont zoomUiFont_{};
    CogScriptTheme currentTheme_{};
    int zoomPercent_{100};
    int findAnchorPosition_{};
    int activeFindMatchPosition_{-1};
    bool suppressIncrementalFind_{};
    std::optional<int> insertMenuLineHint_{};
    std::optional<int> insertMenuColumnHint_{};
    int formatIndentWidth_{4};
    bool formatUseTabs_{false};
    ui::cog::BraceStyle formatBraceStyle_{ui::cog::BraceStyle::nextLine};
    bool formatIndentSections_{true};
    bool formatIndentCodeBodies_{true};
    bool formatKeywordSpacing_{true};
    bool formatInsideParenthesesSpacing_{false};
    bool formatFunctionParenthesesSpacing_{false};
    bool formatForSemicolonSpacing_{true};
    bool formatAlignFinalReturnLabel_{false};
    bool formatTrimExcessBlankLines_{true};
    bool formatIndentComments_{false};
    bool formatOnSave_{false};
    bool documentDerivedStateDirty_{true};
    bool identifierReferencesDirty_{true};
    int diagnosticsExpandedHeight_{150};
    bool selfSaving_{};
    bool reloadingExternal_{};
    bool externalReloadPromptActive_{};
    std::function<bool()> looseWatchStillCurrent_{};
    std::uint64_t looseWatchAuthoredRevision_{};
    bool lastCloseDiscarded_{};
    bool draft_{};
    DraftSavePathResolver draftSavePathResolver_{};
    ScriptPathChangedCallback scriptPathChanged_{};
};

} // namespace

namespace
{

constexpr ui::recovery::RecoveryCborLimits kCogEditorRecoveryCborLimits{
    assets::SedMaximumSemanticSectionBytes,
    2'000'000,
    64,
    3ull * 64ull * 1024ull * 1024ull,
    1024ull * 1024ull,
    assets::SedMaximumDecodedObjectBytes,
};

std::optional<QCborMap> decodeCogEditorRecoveryState(
    const QByteArray& payload,
    QString* error)
{
    const std::optional<QCborValue> value = ui::recovery::deserializeCanonicalCbor(
        payload, kCogEditorRecoveryCborLimits, error);
    if (!value || !value->isMap())
    {
        if (error && error->isEmpty())
        {
            *error = QStringLiteral("COG Script Editor recovery state is not a CBOR map");
        }
        return std::nullopt;
    }
    const QCborMap root = value->toMap();
    const QCborValue tabsValue = root.value(QStringLiteral("tabs"));
    const QCborValue activeValue = root.value(QStringLiteral("active"));
    const QCborArray tabs = tabsValue.isArray() ? tabsValue.toArray() : QCborArray{};
    if (!root.value(QStringLiteral("version")).isInteger()
        || root.value(QStringLiteral("version")).toInteger() != 1
        || !tabsValue.isArray() || tabs.size() > 1024
        || !activeValue.isInteger() || activeValue.toInteger() < -1
        || activeValue.toInteger() >= std::max<qint64>(qint64{1}, tabs.size())
        || (tabs.isEmpty() && activeValue.toInteger() != -1)
        || !root.value(QStringLiteral("dockVisible")).isBool())
    {
        if (error) *error = QStringLiteral("COG Script Editor recovery state uses an unsupported format");
        return std::nullopt;
    }
    return root;
}

QCborArray cogRecoveryHosts(const QCborMap& root)
{
    if ( root.contains(QStringLiteral("hosts")) )
    {
        return root.value(QStringLiteral("hosts")).toArray();
    }
    QCborMap primary{};
    primary.insert(QStringLiteral("id"), QStringLiteral("primary"));
    primary.insert(QStringLiteral("primary"), true);
    primary.insert(QStringLiteral("visible"), root.value(QStringLiteral("dockVisible")));
    primary.insert(QStringLiteral("floating"), false);
    primary.insert(QStringLiteral("geometry"), QByteArray{});
    QCborArray indices{};
    const qsizetype count = root.value(QStringLiteral("tabs")).toArray().size();
    for ( qsizetype index = 0; index < count; ++index )
    {
        indices.append(index);
    }
    primary.insert(QStringLiteral("tabs"), indices);
    const qint64 active = root.value(QStringLiteral("active")).toInteger();
    primary.insert(QStringLiteral("active"), count == 0 ? -1 : std::max<qint64>(0, active));
    return QCborArray{primary};
}

bool validateCogRecoveryHosts(const QCborMap& root, QString* const error)
{
    if ( !root.contains(QStringLiteral("hosts")) )
    {
        return true;
    }
    const auto reject = [error]()
    {
        if ( error )
        {
            *error = QStringLiteral("COG Script Editor host membership or presentation is invalid");
        }
        return false;
    };
    const QCborValue value = root.value(QStringLiteral("hosts"));
    if ( !value.isArray() || value.toArray().isEmpty() || value.toArray().size() > 64 )
    {
        return reject();
    }
    const QCborArray hosts = value.toArray();
    const qsizetype count = root.value(QStringLiteral("tabs")).toArray().size();
    const qint64 globalActive = root.value(QStringLiteral("active")).toInteger();
    QSet<QString> ids{};
    std::vector<bool> occupied(static_cast<std::size_t>(count), false);
    bool activeMatched = globalActive == -1;
    for ( qsizetype index = 0; index < hosts.size(); ++index )
    {
        if ( !hosts.at(index).isMap() )
        {
            return reject();
        }
        const QCborMap host = hosts.at(index).toMap();
        const QCborValue idValue = host.value(QStringLiteral("id"));
        const QString id = idValue.toString();
        if ( !idValue.isString() || id.isEmpty() || id.size() > 64 || ids.contains(id)
            || !host.value(QStringLiteral("primary")).isBool()
            || host.value(QStringLiteral("primary")).toBool() != (index == 0)
            || (index == 0 ? id != QStringLiteral("primary") : id == QStringLiteral("primary"))
            || !host.value(QStringLiteral("visible")).isBool()
            || !host.value(QStringLiteral("floating")).isBool()
            || !host.value(QStringLiteral("geometry")).isByteArray()
            || host.value(QStringLiteral("geometry")).toByteArray().size() > 1024 * 1024
            || !host.value(QStringLiteral("tabs")).isArray()
            || !host.value(QStringLiteral("active")).isInteger() )
        {
            return reject();
        }
        for ( const QChar character : id )
        {
            const ushort code = character.unicode();
            if ( !((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')
                || (code >= '0' && code <= '9') || code == '_' || code == '-') )
            {
                return reject();
            }
        }
        ids.insert(id);
        const QCborArray tabs = host.value(QStringLiteral("tabs")).toArray();
        const qint64 active = host.value(QStringLiteral("active")).toInteger();
        if ( (tabs.isEmpty() && (index != 0 || active != -1))
            || (!tabs.isEmpty() && (active < 0 || active >= tabs.size())) )
        {
            return reject();
        }
        for ( qsizetype local = 0; local < tabs.size(); ++local )
        {
            const QCborValue tab = tabs.at(local);
            if ( !tab.isInteger() || tab.toInteger() < 0 || tab.toInteger() >= count )
            {
                return reject();
            }
            const std::size_t flat = static_cast<std::size_t>(tab.toInteger());
            if ( occupied[flat] )
            {
                return reject();
            }
            occupied[flat] = true;
            if ( tab.toInteger() == globalActive )
            {
                activeMatched = local == active;
            }
        }
    }
    return activeMatched && std::all_of(occupied.begin(), occupied.end(), [](const bool member)
            {
                return member;
            })
        ? true : reject();
}

bool validateCogEditorRecoveryTabs(const QCborMap& root, QString* error)
{
    if ( !validateCogRecoveryHosts(root, error) )
    {
        return false;
    }
    const QCborArray tabs = root.value(QStringLiteral("tabs")).toArray();
    QSet<QString> capturedIdentities{};
    for (qsizetype index = 0; index < tabs.size(); ++index)
    {
        const QCborValue tabValue = tabs.at(index);
        if (!tabValue.isMap())
        {
            if (error)
            {
                *error = QStringLiteral("COG Script Editor recovery tab %1 is not a map")
                    .arg(index + 1);
            }
            return false;
        }

        const QCborMap state = tabValue.toMap();
        if (!CogScriptEditorDialog::recoveryStateIsValid(state))
        {
            if (error)
            {
                *error = QStringLiteral(
                    "COG Script Editor recovery tab %1 has invalid identity or content data")
                    .arg(index + 1);
            }
            return false;
        }

        const QString scriptName = normalizeCogScriptFileName(
            state.value(QStringLiteral("name")).toString().trimmed());
        if (scriptName.isEmpty())
        {
            if (error)
            {
                *error = QStringLiteral("COG Script Editor recovery tab %1 has no safe script name")
                    .arg(index + 1);
            }
            return false;
        }

        QString capturedPathKey{};
        try
        {
            capturedPathKey = ui::recovery::normalizedSourcePath(
                std::filesystem::path(
                    state.value(QStringLiteral("path")).toString().toStdWString()));
        }
        catch (...)
        {
            if (error)
            {
                *error = QStringLiteral("COG Script Editor recovery tab %1 has an invalid path")
                    .arg(index + 1);
            }
            return false;
        }

        // A path is the stable identity when present. Pathless legacy drafts
        // fall back to their normalized name; two such buffers cannot be
        // restored without one silently replacing the other.
        const QString identity = capturedPathKey.isEmpty()
            ? QStringLiteral("name:%1").arg(scriptName.toCaseFolded())
            : QStringLiteral("path:%1").arg(capturedPathKey);
        if (capturedIdentities.contains(identity))
        {
            if (error)
            {
                *error = QStringLiteral(
                    "COG Script Editor recovery tab %1 duplicates an earlier buffer identity")
                    .arg(index + 1);
            }
            return false;
        }
        capturedIdentities.insert(identity);
    }
    return true;
}

} // namespace


std::vector<QPointer<QWidget>> MainWindow::cogScriptEditorPages() const
{
    std::vector<QPointer<QWidget>> pages{};
    for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        if ( !host.tabs )
        {
            continue;
        }
        for ( int index = 0; index < host.tabs->count(); ++index )
        {
            pages.emplace_back(host.tabs->widget(index));
        }
    }
    return pages;
}

std::vector<QPointer<QDockWidget>> MainWindow::cogScriptEditorDocks() const
{
    std::vector<QPointer<QDockWidget>> docks{};
    for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        if ( host.dock && host.tabs )
        {
            docks.emplace_back(host.dock);
        }
    }
    return docks;
}

QTabWidget* MainWindow::cogScriptEditorTabsForPage(const QWidget* const page) const noexcept
{
    for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        if ( host.tabs && host.tabs->indexOf(const_cast<QWidget*>(page)) >= 0 )
        {
            return host.tabs.data();
        }
    }
    return nullptr;
}

QDockWidget* MainWindow::cogScriptEditorDockForTabs(const QTabWidget* const tabs) const noexcept
{
    for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        if ( host.tabs == tabs && host.dock )
        {
            return host.dock.data();
        }
    }
    return nullptr;
}

QTabWidget* MainWindow::cogScriptEditorDropTargetAtPosition(
    QTabWidget* const source,
    QWidget* const page,
    const QPoint& globalPosition,
    bool* const rejectForeignTabBar) const noexcept
{
    if ( rejectForeignTabBar )
    {
        *rejectForeignTabBar = false;
    }
    const std::uint64_t context = resourceEditorDocumentContextGeneration_;
    const auto hostFor = [this, context](const QTabWidget* const tabs) -> const CogScriptEditorTabHost*
    {
        for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
        {
            if ( host.tabs == tabs && host.dock && host.sessionGeneration == context )
            {
                return &host;
            }
        }
        return nullptr;
    };
    const auto hostForDock = [this, context](const QDockWidget* const dock) -> const CogScriptEditorTabHost*
    {
        for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
        {
            if ( host.dock == dock && host.tabs && host.sessionGeneration == context )
            {
                return &host;
            }
        }
        return nullptr;
    };
    const CogScriptEditorTabHost* const sourceHost = hostFor(source);
    if ( !sourceHost || !page || !documentEditingAllowed()
        || cogScriptEditorTabsForPage(page) != source
        || !sourceHost->dock->isVisible() )
    {
        return nullptr;
    }

    if ( QWidget* const underPointer = QApplication::widgetAt(globalPosition) )
    {
        QDockWidget* actualDockOwner = nullptr;
        MainWindow* pointerOwner = nullptr;
        QTabBar* actualTabBar = nullptr;
        for ( QWidget* current = underPointer; current; current = current->parentWidget() )
        {
            if ( !pointerOwner )
            {
                pointerOwner = dynamic_cast<MainWindow*>(current);
            }
            if ( !actualDockOwner )
            {
                actualDockOwner = qobject_cast<QDockWidget*>(current);
            }
            if ( !actualTabBar )
            {
                actualTabBar = qobject_cast<QTabBar*>(current);
            }
        }
        if ( pointerOwner && pointerOwner != this )
        {
            if ( rejectForeignTabBar )
            {
                *rejectForeignTabBar = true;
            }
            return nullptr;
        }
        if ( actualTabBar )
        {
            for ( QWidget* parent = actualTabBar->parentWidget(); parent; parent = parent->parentWidget() )
            {
                if ( auto* const tabs = qobject_cast<QTabWidget*>(parent) )
                {
                    const CogScriptEditorTabHost* const targetHost = hostFor(tabs);
                    if ( !targetHost || !targetHost->dock->isVisible()
                        || targetHost->dock->visibleRegion().isEmpty() )
                    {
                        const bool isDocumentDock = actualDockOwner
                            && (actualDockOwner->property("sed.cogScriptEditorDock").toBool()
                                || actualDockOwner->property("sed.cogScriptEditorHostId").isValid()
                                || actualDockOwner->property("resource_authoring.kind").isValid());
                        const bool isDocumentTabs = tabs->property("sed.cogScriptEditorHost").toBool()
                            || tabs->objectName().startsWith(QStringLiteral("resourceEditorTabs"))
                            || tabs->objectName() == QStringLiteral("soundClassResourceEditorTabs")
                            || tabs->objectName() == QStringLiteral("uniResourceEditorTabs");
                        if ( isDocumentDock || isDocumentTabs )
                        {
                            if ( rejectForeignTabBar )
                            {
                                *rejectForeignTabBar = true;
                            }
                        }
                        return nullptr;
                    }
                    return tabs;
                }
            }
        }
        if ( actualDockOwner )
        {
            const bool isCogDock = actualDockOwner->property("sed.cogScriptEditorDock").toBool()
                || actualDockOwner->property("sed.cogScriptEditorHostId").isValid();
            const bool isResourceDock = actualDockOwner->property("resource_authoring.kind").isValid();
            const CogScriptEditorTabHost* const actualHost = hostForDock(actualDockOwner);
            if ( isCogDock )
            {
                if ( actualHost && actualHost->dock->isVisible()
                    && !actualHost->dock->visibleRegion().isEmpty() )
                {
                    return actualHost->tabs.data();
                }
                if ( rejectForeignTabBar )
                {
                    *rejectForeignTabBar = true;
                }
                return nullptr;
            }
            if ( actualHost && actualHost->dock->isVisible()
                && !actualHost->dock->visibleRegion().isEmpty() )
            {
                return actualHost->tabs.data();
            }
            if ( isResourceDock )
            {
                if ( rejectForeignTabBar )
                {
                    *rejectForeignTabBar = true;
                }
                return nullptr;
            }
            // An unrelated dock in this window has no Join target; allow normal detachment.
            return nullptr;
        }
        // An unrelated widget owns this point; never select an obscured COG dock by geometry.
        return nullptr;
    }

    for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        if ( !host.dock || !host.tabs || host.sessionGeneration != context
            || !host.dock->isVisible() || host.dock->visibleRegion().isEmpty() )
        {
            continue;
        }
        const QRect bounds(host.dock->mapToGlobal(QPoint{}), host.dock->size());
        if ( bounds.contains(globalPosition) )
        {
            return host.tabs.data();
        }
    }
    return nullptr;
}

QTabWidget* MainWindow::activeCogScriptEditorTabHost() const noexcept
{
    QWidget* const focus = QApplication::focusWidget();
    if ( focus )
    {
        for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
        {
            if ( host.dock && host.tabs && (focus == host.dock || host.dock->isAncestorOf(focus)) )
            {
                return host.tabs.data();
            }
        }
        if ( focus != this && !isAncestorOf(focus) )
        {
            return nullptr;
        }
    }
    QWidget* const active = QApplication::activeWindow();
    for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        if ( host.dock && host.tabs && active == host.dock.data() )
        {
            return host.tabs.data();
        }
    }
    if ( lastActiveCogScriptEditorHost_ && cogScriptEditorDockForTabs(lastActiveCogScriptEditorHost_.data()) )
    {
        return lastActiveCogScriptEditorHost_.data();
    }
    return cogScriptEditorTabs_;
}

void MainWindow::registerCogScriptEditorTabHost(QDockWidget* const dock, QTabWidget* const tabs,
    const QString& id, const bool primary)
{
    if ( !dock || !tabs )
    {
        return;
    }
    std::erase_if(cogScriptEditorTabHosts_, [tabs](const CogScriptEditorTabHost& host)
        {
            return !host.dock || !host.tabs || host.tabs == tabs;
        });
    const CogScriptEditorTabHost record{dock, tabs, id, resourceEditorDocumentContextGeneration_, primary};
    if ( primary )
    {
        cogScriptEditorTabHosts_.insert(cogScriptEditorTabHosts_.begin(), record);
    }
    else
    {
        cogScriptEditorTabHosts_.push_back(record);
    }
    dock->setProperty("sed.cogScriptEditorDock", true);
    dock->setProperty("sed.cogScriptEditorHostId", id);
    tabs->setProperty("sed.cogScriptEditorHost", true);
    tabs->setProperty("sed.cogScriptEditorHostId", id);
    dock->installEventFilter(this);
    if ( !primary )
    {
        const QPointer<MainWindow> self(this);
        const QPointer<QTabWidget> guardedTabs(tabs);
        dock->installEventFilter(new CogHostCloseFilter(dock, [self, guardedTabs]()
            {
                if ( !self || !guardedTabs || !self->closeCogScriptEditorHost(guardedTabs.data()) )
                {
                    return false;
                }
                if ( self )
                {
                    self->cleanupEmptyCogScriptEditorHosts();
                }
                return true;
            }));
    }

    observeCogScriptEditorRecoveryHosts();
}

QDockWidget* MainWindow::createCogScriptEditorDockHost(const QString& id, const bool floating)
{
    if ( !workspaceWindow_ || cogScriptEditorTabHosts_.size() >= 64 )
    {
        return nullptr;
    }
    const QPointer<MainWindow> self(this);
    const QPointer<ui::PreferredWidthDockWidget> dock = new ui::PreferredWidthDockWidget(QStringLiteral("COG Scripts"), workspaceWindow_);
    // Detached/recovery COG hosts reuse this factory; seed the hidden host's
    // maximize hint, then repair it after Qt's floating transition.
    dock->setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    dock->setObjectName(QStringLiteral("cogScriptEditorDock_%1").arg(id));
    dock->setWindowIcon(ui::makeViewportIcon(ui::ViewportIcon::outlinerCogs));
    dock->setStyleSheet(QStringLiteral("QDockWidget::title { padding-left: 28px; }"));
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    dock->setMinimumWidth(kMinimumCogScriptDockWidth);
    dock->setPreferredWidth(cogScriptDockPreferredWidth_);
    auto* const body = new QWidget(dock.data());
    body->setEnabled(documentEditingAllowed());
    auto* const layout = new QVBoxLayout(body);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    auto* const tabs = new QTabWidget(body);
    tabs->setObjectName(QStringLiteral("cogScriptEditorTabs_%1").arg(id));
    tabs->setDocumentMode(true);
    tabs->setMovable(true);
    tabs->setTabsClosable(true);
    tabs->setProperty("cogHoverIndex", -1);
    layout->addWidget(tabs, 1);
    dock->setWidget(body);
    new CogDockTitleIcon(dock.data());
    workspaceWindow_->addDockWidget(Qt::LeftDockWidgetArea, dock.data());
    if ( !self || !dock )
    {
        return nullptr;
    }
    registerCogScriptEditorTabHost(dock.data(), tabs, id, false);
    if ( !self || !dock )
    {
        return nullptr;
    }
    configureCogScriptEditorTabHost(dock.data(), tabs);
    if ( !self || !dock )
    {
        return nullptr;
    }
    dock->setFloating(floating);
    if ( !self || !dock )
    {
        return nullptr;
    }
    if ( floating )
    {
        ensureFloatingCogMaximizeButton(dock.data());
    }
    if ( !self || !dock )
    {
        return nullptr;
    }
    dock->hide();
    return self && dock ? dock.data() : nullptr;
}

void MainWindow::configureCogScriptEditorTabHost(QDockWidget* const dock, QTabWidget* const tabs)
{
    observeResourceEditorWindowDrops(dock);
    const QPointer<MainWindow> self(this);
    const QPointer<QDockWidget> guardedDock(dock);
    const QPointer<QTabWidget> guardedTabs(tabs);
    auto* const layout = qobject_cast<QVBoxLayout*>(dock->widget()->layout());
    auto* const tools = new QWidget(dock->widget());
    tools->setObjectName(QStringLiteral("cogEditorGroupToolbar"));
    auto* const row = new QHBoxLayout(tools);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);
    const auto add = [tools, row](const char* const name, const QString& title, const ui::ViewportIcon icon)
    {
        auto* const button = new QToolButton(tools);
        button->setObjectName(QString::fromLatin1(name));
        button->setToolTip(title);
        button->setAccessibleName(title);
        button->setIcon(ui::makeViewportIcon(icon));
        button->setIconSize(QSize(20, 20));
        button->setFixedHeight(30);
        button->setAutoRaise(true);
        row->addWidget(button);
        return button;
    };
    auto* const create = add("cogEditorNewButton", QStringLiteral("New COG script"), ui::ViewportIcon::resourceNew);
    auto* const open = add("cogEditorOpenButton", QStringLiteral("Open COG script"), ui::ViewportIcon::resourceOpen);
    auto* const save = add("cogEditorSaveButton", QStringLiteral("Save COG script"), ui::ViewportIcon::actionSave);
    row->addStretch(1);
    layout->insertWidget(0, tools);
    auto* const menu = new ui::shortcuts::KeySequenceDisplayMenu(save);
    auto* const saveAll = menu->addAction(QStringLiteral("Save All"));
    saveAll->setObjectName(QStringLiteral("cog_editor.save_all"));
    save->setMenu(menu);
    save->setPopupMode(QToolButton::MenuButtonPopup);
    save->setEnabled(false);
    saveAll->setEnabled(false);
    auto* const returnDock = new QToolButton(dock->widget());
    returnDock->setObjectName(QStringLiteral("cogEditorDockReturnButton"));
    returnDock->setToolTip(QStringLiteral("Dock COG Scripts"));
    returnDock->setAccessibleName(returnDock->toolTip());
    returnDock->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
    returnDock->setFocusPolicy(Qt::NoFocus);
    returnDock->setAutoRaise(true);
    layout->insertWidget(0, returnDock, 0, Qt::AlignRight);
    returnDock->setVisible(dock->isFloating());
    connect(dock, &QDockWidget::topLevelChanged, returnDock, [returnDock](const bool floating)
        {
            returnDock->setVisible(floating);
        });
    connect(returnDock, &QToolButton::clicked, dock, [guardedDock]()
        {
            if ( guardedDock )
            {
                guardedDock->setFloating(false);
            }
            if ( guardedDock )
            {
                guardedDock->show();
            }
            if ( guardedDock )
            {
                guardedDock->raise();
            }
        });
    connect(create, &QToolButton::clicked, this, [self, guardedTabs]()
        {
            if ( !self || !guardedTabs )
            {
                return;
            }
            const QPointer<QTabWidget> previous = self->preferredCogScriptEditorHost_;
            self->preferredCogScriptEditorHost_ = guardedTabs;
            self->createNewCogScript();
            if ( self )
            {
                self->preferredCogScriptEditorHost_ = previous;
            }
        });
    connect(open, &QToolButton::clicked, this, [self, guardedTabs]()
        {
            if ( !self || !guardedTabs )
            {
                return;
            }
            const std::uint64_t generation = self->resourceEditorDocumentContextGeneration_;
            const auto selected = pickCogInlineAssignmentResource(self.data(), ui::ResourcePickerKind::cog, {},
                self->resourceLocatorSnapshot_, self->templates_, self->document_.level(),
                self->preferredExtractionRoot(), self->currentPath_);
            if ( self && guardedTabs && selected && self->resourceEditorDocumentContextGeneration_ == generation )
            {
                const QPointer<QTabWidget> previous = self->preferredCogScriptEditorHost_;
                self->preferredCogScriptEditorHost_ = guardedTabs;
                self->openCogScriptEditor(selected->toStdString());
                if ( self )
                {
                    self->preferredCogScriptEditorHost_ = previous;
                }
            }
        });
    connect(save, &QToolButton::clicked, this, [self, guardedTabs]()
        {
            auto* const editor = self && guardedTabs
                ? dynamic_cast<CogScriptEditorDialog*>(guardedTabs->currentWidget()) : nullptr;
            if ( editor && self->documentEditingAllowed() && editor->hasModifiedContents() )
            {
                (void)editor->triggerSave();
            }
        });
    connect(saveAll, &QAction::triggered, this, [self]()
        {
            if ( self )
            {
                (void)self->saveAllCogScriptEditors();
            }
        });
    connect(tabs, &QTabWidget::currentChanged, this, [self, guardedTabs](const int index)
        {
            if ( self && guardedTabs )
            {
                if ( index >= 0 )
                {
                    self->lastActiveCogScriptEditorHost_ = guardedTabs;
                }
                self->refreshCogScriptEditorToolbars();
            }
        });
    connect(tabs, &QTabWidget::tabCloseRequested, this, [self, guardedTabs](const int index)
        {
            QPointer<QWidget> page(guardedTabs ? guardedTabs->widget(index) : nullptr);
            if ( self && page && cogScriptTabCanClose(page.data()) )
            {
                page->close();
            }
        });
    QTabBar* const bar = tabs->tabBar();
    bar->setMouseTracking(true);
    bar->setIconSize(QSize(16, 16));
    bar->installEventFilter(new CogScriptTabHoverFilter(tabs));
    qApp->installEventFilter(new ui::DocumentTabDragFilter(
        [self](QTabWidget* source, QWidget* page, const QPoint& position)
        {
            if ( self )
            {
                self->transferCogScriptEditorTab(source, page, position);
            }
        },
        bar,
        [self](QTabWidget* const source, QWidget* const page, const QPoint& position)
        {
            return self ? QPointer<QTabWidget>(self->cogScriptEditorDropTargetAtPosition(
                source, page, position)) : QPointer<QTabWidget>{};
        },
        [self]()
        {
            return self ? self->resourceEditorDocumentContextGeneration_ : std::uint64_t{};
        },
        QStringLiteral("Join COG file tabs")));
    bar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(bar, &QTabBar::customContextMenuRequested, this, [self, guardedTabs, guardedDock, bar](const QPoint& point)
        {
            const QPointer<QTabBar> guardedBar(bar);
            QPointer<QWidget> page(guardedTabs ? guardedTabs->widget(bar->tabAt(point)) : nullptr);
            if ( !self || !guardedDock || !guardedBar || !page )
            {
                return;
            }
            ui::shortcuts::KeySequenceDisplayMenu popup{};
            connect(self.data(), &QObject::destroyed, &popup, [&popup]() { popup.close(); });
            connect(guardedDock.data(), &QObject::destroyed, &popup, [&popup]() { popup.close(); });
            self->populateCogScriptTabContextMenu(&popup, page.data());
            popup.exec(guardedBar->mapToGlobal(point));
        });
    connect(bar, &QTabBar::tabMoved, this, [self, guardedTabs](const int, const int)
        {
            if ( self && guardedTabs )
            {
                normalizeCogScriptTabOrder(guardedTabs.data());
                refreshCogScriptDockTabPresentation(guardedTabs.data());
            }
        });
}

void MainWindow::transferCogScriptEditorTab(QTabWidget* const source, QWidget* const page,
    const QPoint& globalPosition)
{
    const QPointer<MainWindow> self(this);
    const QPointer<QTabWidget> guardedSource(source);
    const QPointer<QWidget> guardedPage(page);
    const std::uint64_t generation = resourceEditorDocumentContextGeneration_;
    if ( !documentEditingAllowed() || !source || !page || source->indexOf(page) < 0
        || !cogScriptEditorDockForTabs(source) )
    {
        return;
    }
    const auto sourceHost = std::find_if(cogScriptEditorTabHosts_.begin(), cogScriptEditorTabHosts_.end(),
        [source](const CogScriptEditorTabHost& host)
            {
                return host.tabs == source;
            });
    if ( sourceHost == cogScriptEditorTabHosts_.end() || sourceHost->sessionGeneration != generation )
    {
        return;
    }
    const bool sourcePrimary = sourceHost->primary;
    const QPointer<QDockWidget> sourceDock(sourceHost->dock);
    if ( !sourceDock )
    {
        return;
    }
    bool rejectForeignTabBar = false;
    QPointer<QTabWidget> target(cogScriptEditorDropTargetAtPosition(
        source, page, globalPosition, &rejectForeignTabBar));
    if ( rejectForeignTabBar )
    {
        return;
    }
    if ( target == source )
    {
        return;
    }
    if ( !target )
    {
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
        const QPointer<QDockWidget> dock(createCogScriptEditorDockHost(id, true));
        if ( !self || !guardedSource || !guardedPage || self->resourceEditorDocumentContextGeneration_ != generation )
        {
            return;
        }
        if ( !dock )
        {
            self->statusBar()->showMessage(QStringLiteral("The workspace already has the maximum of 64 COG Script Editor windows."), 5000);
            return;
        }
        for ( const CogScriptEditorTabHost& host : self->cogScriptEditorTabHosts_ )
        {
            if ( host.dock == dock )
            {
                target = host.tabs;
                break;
            }
        }
        QScreen* screen = QGuiApplication::screenAt(globalPosition);
        if ( !screen )
        {
            screen = QGuiApplication::primaryScreen();
        }
        QPoint origin = globalPosition;
        if ( screen )
        {
            const QRect available = screen->availableGeometry();
            const QSize preferred(std::min(std::max(kMinimumCogScriptDockWidth, 800), available.width()),
                std::min(600, available.height()));
            dock->resize(preferred);
            origin.setX(std::clamp(origin.x(), available.left(),
                std::max(available.left(), available.right() - preferred.width() + 1)));
            origin.setY(std::clamp(origin.y(), available.top(),
                std::max(available.top(), available.bottom() - preferred.height() + 1)));
        }
        if ( dock )
        {
            dock->move(origin);
        }
    }
    if ( !self || !guardedSource || !target || !guardedPage
        || self->resourceEditorDocumentContextGeneration_ != generation
        || self->cogScriptEditorTabsForPage(guardedPage.data()) != guardedSource.data() )
    {
        return;
    }
    const int index = guardedSource->indexOf(guardedPage.data());
    const QString title = guardedSource->tabText(index);
    const QIcon icon = guardedSource->tabIcon(index);
    // Membership changes form one pointer transaction. Publish selection only
    // after the page belongs to its destination, so observers never see an orphan.
    {
        const QSignalBlocker sourceBlocker(guardedSource.data());
        const QSignalBlocker targetBlocker(target.data());
        guardedSource->removeTab(index);
        target->addTab(guardedPage.data(), icon, title);
    }
    if ( !self || !target || !guardedPage || self->resourceEditorDocumentContextGeneration_ != generation )
    {
        return;
    }
    updateCogScriptDockTabPresentation(target.data(), guardedPage.data());
    if ( !self || !target || !guardedPage || self->resourceEditorDocumentContextGeneration_ != generation )
    {
        return;
    }
    normalizeCogScriptTabOrder(target.data());
    if ( !self || !target || !guardedPage || self->resourceEditorDocumentContextGeneration_ != generation )
    {
        return;
    }
    target->setCurrentWidget(guardedPage.data());
    if ( !self || !target || !guardedPage || self->resourceEditorDocumentContextGeneration_ != generation )
    {
        return;
    }
    self->lastActiveCogScriptEditorHost_ = target;
    const QPointer<QDockWidget> targetDock(self->cogScriptEditorDockForTabs(target.data()));
    if ( targetDock )
    {
        targetDock->show();
    }
    if ( self && targetDock )
    {
        targetDock->raise();
    }
    if ( self && targetDock )
    {
        targetDock->activateWindow();
    }
    if ( !self || self->resourceEditorDocumentContextGeneration_ != generation )
    {
        return;
    }
    if ( sourcePrimary && guardedSource && guardedSource->count() == 0 && sourceDock
        && self->cogScriptEditorDockForTabs(guardedSource.data()) == sourceDock.data()
        && self->resourceEditorDocumentContextGeneration_ == generation )
    {
        sourceDock->hide();
        if ( !self || !sourceDock || self->resourceEditorDocumentContextGeneration_ != generation
            || self->cogScriptEditorDockForTabs(guardedSource.data()) != sourceDock.data() )
        {
            return;
        }
    }
    self->cleanupEmptyCogScriptEditorHosts();
    if ( self )
    {
        self->refreshCogScriptEditorToolbars();
    }
    if ( self )
    {
        self->noteRecoveryStateChanged();
    }
}

bool MainWindow::closeCogScriptEditorHost(QTabWidget* const tabs)
{
    if ( !tabs || !cogScriptEditorDockForTabs(tabs) )
    {
        return false;
    }
    const QPointer<MainWindow> self(this);
    const QPointer<QTabWidget> guardedTabs(tabs);
    const std::uint64_t generation = resourceEditorDocumentContextGeneration_;
    std::vector<QPointer<QWidget>> pages{};
    std::vector<QByteArray> resolved{};
    for ( int index = 0; index < tabs->count(); ++index )
    {
        pages.emplace_back(tabs->widget(index));
    }
    for ( const QPointer<QWidget>& page : pages )
    {
        const QPointer<CogScriptEditorDialog> editor(dynamic_cast<CogScriptEditorDialog*>(page.data()));
        if ( !editor || !editor->confirmPendingClose() || !self || !editor || !guardedTabs
            || self->resourceEditorDocumentContextGeneration_ != generation
            || self->cogScriptEditorTabsForPage(page.data()) != guardedTabs )
        {
            return false;
        }
        resolved.push_back(ui::recovery::serializeCanonicalCbor(QCborValue(editor->recoveryState(false))));
    }
    if ( !self || !guardedTabs || guardedTabs->count() != static_cast<int>(pages.size()) )
    {
        return false;
    }
    for ( std::size_t index = 0; index < pages.size(); ++index )
    {
        auto* const editor = dynamic_cast<CogScriptEditorDialog*>(pages[index].data());
        if ( !editor || guardedTabs->widget(static_cast<int>(index)) != editor
            || ui::recovery::serializeCanonicalCbor(QCborValue(editor->recoveryState(false))) != resolved[index] )
        {
            return false;
        }
    }
    for ( const QPointer<QWidget>& page : pages )
    {
        const int index = guardedTabs->indexOf(page.data());
        guardedTabs->removeTab(index);
        if ( !self || !guardedTabs )
        {
            return true;
        }
        delete page.data();
        if ( !self || !guardedTabs )
        {
            return true;
        }
    }
    return true;
}

void MainWindow::cleanupEmptyCogScriptEditorHosts()
{
    std::vector<QPointer<QDockWidget>> empty{};
    std::erase_if(cogScriptEditorTabHosts_, [&empty](const CogScriptEditorTabHost& host)
        {
            if ( !host.dock || !host.tabs )
            {
                return true;
            }
            if ( !host.primary && host.tabs->count() == 0 )
            {
                empty.emplace_back(host.dock);
                return true;
            }
            return false;
        });
    if ( lastActiveCogScriptEditorHost_ && !cogScriptEditorDockForTabs(lastActiveCogScriptEditorHost_.data()) )
    {
        lastActiveCogScriptEditorHost_ = cogScriptEditorTabs_;
    }
    if ( preferredCogScriptEditorHost_ && !cogScriptEditorDockForTabs(preferredCogScriptEditorHost_.data()) )
    {
        preferredCogScriptEditorHost_.clear();
    }
    const QPointer<MainWindow> self(this);
    const QPointer<QMainWindow> workspace(workspaceWindow_);
    for ( const QPointer<QDockWidget>& dock : empty )
    {
        if ( !self || !workspace )
        {
            return;
        }
        if ( !dock )
        {
            continue;
        }
        // Remove the live widget first. Qt cancels this dock's geometry
        // animation and clears its pending plug before deferred deletion.
        if ( !ui::removeDockWidgetAndCompleteAnimations(workspace.data(), dock.data(),
            [self, workspace]()
            {
                return self && workspace;
            }) )
        {
            return;
        }
        if ( !self || !workspace )
        {
            return;
        }
        if ( dock )
        {
            dock->hide();
        }
        if ( !self || !workspace )
        {
            return;
        }
        if ( dock )
        {
            dock->deleteLater();
        }
        if ( !self )
        {
            return;
        }
    }
}

bool MainWindow::prepareCogEditorRecoveryHosts(const QByteArray& payload)
{
    QString error{};
    const std::optional<QCborMap> root = decodeCogEditorRecoveryState(payload, &error);
    if ( !root || !validateCogEditorRecoveryTabs(*root, &error) || !cogScriptEditorTabs_ )
    {
        return false;
    }
    const QCborArray hosts = cogRecoveryHosts(*root);
    const QPointer<MainWindow> self(this);
    const std::uint64_t generation = resourceEditorDocumentContextGeneration_;
    for ( const QCborValue& value : hosts )
    {
        const QCborMap state = value.toMap();
        const QString id = state.value(QStringLiteral("id")).toString();
        const auto existing = std::find_if(cogScriptEditorTabHosts_.begin(), cogScriptEditorTabHosts_.end(),
            [&id](const CogScriptEditorTabHost& host)
            {
                return host.stableId == id && host.dock && host.tabs;
            });
        if ( existing == cogScriptEditorTabHosts_.end() )
        {
            if ( !createCogScriptEditorDockHost(id, state.value(QStringLiteral("floating")).toBool()) )
            {
                return false;
            }
        }
        if ( !self || self->resourceEditorDocumentContextGeneration_ != generation )
        {
            return false;
        }
    }
    return true;
}

bool MainWindow::cogScriptEditorOwnsFocus() const
{
    QWidget* const focus = QApplication::focusWidget();
    if ( focus )
    {
        for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
        {
            if ( host.dock && (focus == host.dock || host.dock->isAncestorOf(focus)) )
            {
                return true;
            }
        }
        return false;
    }
    QWidget* const active = QApplication::activeWindow();
    return std::any_of(cogScriptEditorTabHosts_.begin(), cogScriptEditorTabHosts_.end(),
        [active](const CogScriptEditorTabHost& host)
        {
            return host.dock && host.dock.data() == active;
        });
}

bool MainWindow::activeCogScriptEditorCanUndoRedo(const bool redo) const
{
    if ( !cogScriptEditorOwnsFocus() )
    {
        return false;
    }
    QWidget* const focus = QApplication::focusWidget();
    if ( QLineEdit* const edit = qobject_cast<QLineEdit*>(focus) )
    {
        return redo ? edit->isRedoAvailable() : edit->isUndoAvailable();
    }
    for ( QWidget* current = focus; current; current = current->parentWidget() )
    {
        if ( const auto* const editor = dynamic_cast<CogScriptEditorDialog*>(current) )
        {
            return editor->canUndoRedo(redo);
        }
    }
    const auto* const editor = activeCogScriptEditorTabHost()
        ? dynamic_cast<CogScriptEditorDialog*>(activeCogScriptEditorTabHost()->currentWidget()) : nullptr;
    return editor && editor->canUndoRedo(redo);
}

bool MainWindow::triggerActiveCogScriptEditorUndoRedo(const bool redo)
{
    if ( !documentEditingAllowed() )
    {
        return false;
    }
    QWidget* const focus = QApplication::focusWidget();
    if ( QLineEdit* const edit = qobject_cast<QLineEdit*>(focus) )
    {
        if ( redo )
        {
            edit->redo();
        }
        else
        {
            edit->undo();
        }
        return true;
    }
    for ( QWidget* current = focus; current; current = current->parentWidget() )
    {
        if ( auto* const editor = dynamic_cast<CogScriptEditorDialog*>(current) )
        {
            return editor->triggerUndoRedo(redo);
        }
    }
    auto* const editor = activeCogScriptEditorTabHost()
        ? dynamic_cast<CogScriptEditorDialog*>(activeCogScriptEditorTabHost()->currentWidget()) : nullptr;
    return editor && editor->triggerUndoRedo(redo);
}

bool MainWindow::triggerActiveCogScriptEditorSave()
{
    if (!documentEditingAllowed())
    {
        return false;
    }
    if (QWidget* focus = QApplication::focusWidget())
    {
        for (QWidget* current = focus; current; current = current->parentWidget())
        {
            if (auto* editor = dynamic_cast<CogScriptEditorDialog*>(current))
            {
                return editor->triggerSave();
            }
        }
    }

    if (activeCogScriptEditorTabHost())
    {
        if (auto* editor = dynamic_cast<CogScriptEditorDialog*>(activeCogScriptEditorTabHost()->currentWidget()))
        {
            return editor->triggerSave();
        }
    }

    if (QWidget* active = QApplication::activeWindow())
    {
        for (QWidget* current = active; current; current = current->parentWidget())
        {
            if (auto* editor = dynamic_cast<CogScriptEditorDialog*>(current))
            {
                return editor->triggerSave();
            }
        }
    }

    for (const auto& [_, ptr] : openCogScriptEditors_)
    {
        if (auto* editor = dynamic_cast<CogScriptEditorDialog*>(ptr.data()))
        {
            if (editor->isActiveWindow())
            {
                return editor->triggerSave();
            }
        }
    }

    return false;
}

void MainWindow::refreshCogScriptEditorToolbars()
{
    if ( recoveryShuttingDown_ )
    {
        return;
    }
    bool anyModified = false;
    for ( const QPointer<QWidget>& page : cogScriptEditorPages() )
    {
        auto* const editor = dynamic_cast<CogScriptEditorDialog*>(page.data());
        anyModified = anyModified || (editor && editor->hasModifiedContents());
        QPlainTextEdit* const source = editor ? editor->findChild<QPlainTextEdit*>() : nullptr;
        if ( source && !source->property("cogEditorHistoryUiConnected").toBool() )
        {
            source->setProperty("cogEditorHistoryUiConnected", true);
            connect(source->document(), &QTextDocument::modificationChanged, this, [this](const bool)
                {
                    refreshCogScriptEditorToolbars();
                });
            connect(source->document(), &QTextDocument::undoAvailable, this, [this](const bool)
                {
                    syncCogScriptEditorActionFocus();
                });
            connect(source->document(), &QTextDocument::redoAvailable, this, [this](const bool)
                {
                    syncCogScriptEditorActionFocus();
                });
            connect(source, &QPlainTextEdit::cursorPositionChanged,
                this, &MainWindow::syncCogScriptEditorActionFocus);
            connect(source, &QPlainTextEdit::textChanged,
                this, &MainWindow::syncCogScriptEditorActionFocus);
            if ( QLineEdit* const find = editor->findChild<QLineEdit*>(QStringLiteral("cogEditorFindEdit")) )
            {
                connect(find, &QLineEdit::textChanged, this, [this](const QString&)
                    {
                        syncCogScriptEditorActionFocus();
                    });
            }
        }
    }
    for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        if ( !host.dock || !host.tabs )
        {
            continue;
        }
        if ( QToolButton* const save = host.dock->findChild<QToolButton*>(QStringLiteral("cogEditorSaveButton")) )
        {
            save->setEnabled(documentEditingAllowed() && anyModified);
        }
        if ( QAction* const saveAll = host.dock->findChild<QAction*>(QStringLiteral("cog_editor.save_all")) )
        {
            saveAll->setEnabled(documentEditingAllowed() && anyModified);
        }
    }
}

bool MainWindow::saveAllCogScriptEditors()
{
    if ( !documentEditingAllowed() )
    {
        return false;
    }
    const QPointer<MainWindow> self(this);
    const QPointer<QTabWidget> selectedHost(activeCogScriptEditorTabHost());
    const QPointer<QWidget> selected(selectedHost ? selectedHost->currentWidget() : nullptr);
    const std::uint64_t generation = resourceEditorDocumentContextGeneration_;
    std::vector<std::pair<QPointer<QWidget>, QPointer<QTabWidget>>> pages{};
    for ( const QPointer<QWidget>& page : cogScriptEditorPages() )
    {
        pages.emplace_back(page, cogScriptEditorTabsForPage(page.data()));
    }
    bool success = true;
    for ( const auto& [page, tabs] : pages )
    {
        if ( !self || !tabs || self->resourceEditorDocumentContextGeneration_ != generation
            || !page || self->cogScriptEditorTabsForPage(page.data()) != tabs.data() )
        {
            return false;
        }
        auto* const editor = dynamic_cast<CogScriptEditorDialog*>(page.data());
        if ( editor && editor->hasModifiedContents() && !editor->triggerSave() )
        {
            success = false;
            break;
        }
        if ( !self || !page || !tabs || self->resourceEditorDocumentContextGeneration_ != generation
            || self->cogScriptEditorTabsForPage(page.data()) != tabs.data() )
        {
            return false;
        }
    }
    if ( self && selectedHost && self->resourceEditorDocumentContextGeneration_ == generation )
    {
        if ( selected && selectedHost->indexOf(selected.data()) >= 0 )
        {
            selectedHost->setCurrentWidget(selected.data());
            self->lastActiveCogScriptEditorHost_ = selectedHost;
        }
        if ( self )
        {
            self->refreshCogScriptEditorToolbars();
        }
    }
    return success;
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
bool MainWindow::saveCogScriptEditorForTest(const int index)
{
    if (!cogScriptEditorTabs_ || index < 0
        || index >= cogScriptEditorTabs_->count())
    {
        return false;
    }
    cogScriptEditorTabs_->setCurrentIndex(index);
    auto* editor = dynamic_cast<CogScriptEditorDialog*>(
        cogScriptEditorTabs_->widget(index));
    return editor && editor->triggerSave();
}

QString MainWindow::normalizedCogScriptNameForTest(const QString& typedName) const
{
    return normalizeCogScriptFileName(typedName);
}

QString MainWindow::cogScriptNameValidationErrorForTest(
    const QString& typedName) const
{
    return newCogScriptNameValidationError(typedName);
}

QStringList MainWindow::cogScriptTabContextActionTextsForTest(const int index)
{
    if (!cogScriptEditorTabs_ || index < 0
        || index >= cogScriptEditorTabs_->count())
    {
        return {};
    }
    ui::shortcuts::KeySequenceDisplayMenu menu{};
    populateCogScriptTabContextMenu(&menu, cogScriptEditorTabs_->widget(index));
    QStringList result{};
    for (const QAction* action : menu.actions())
    {
        if (action && !action->isSeparator())
        {
            result.push_back(action->text());
        }
    }
    return result;
}

bool MainWindow::cogScriptTabContextActionEnabledForTest(
    const int index,
    const QString& actionText)
{
    if (!cogScriptEditorTabs_ || index < 0
        || index >= cogScriptEditorTabs_->count())
    {
        return false;
    }
    ui::shortcuts::KeySequenceDisplayMenu menu{};
    populateCogScriptTabContextMenu(&menu, cogScriptEditorTabs_->widget(index));
    const QList<QAction*> actions = menu.actions();
    const auto actionIt = std::find_if(
        actions.begin(), actions.end(),
        [&actionText](const QAction* action)
        {
            return action && action->text() == actionText;
        });
    return actionIt != actions.end() && (*actionIt)->isEnabled();
}

bool MainWindow::triggerCogScriptTabContextActionForTest(
    const int index,
    const QString& actionText)
{
    if (!cogScriptEditorTabs_ || index < 0
        || index >= cogScriptEditorTabs_->count())
    {
        return false;
    }
    ui::shortcuts::KeySequenceDisplayMenu menu{};
    populateCogScriptTabContextMenu(&menu, cogScriptEditorTabs_->widget(index));
    const QList<QAction*> actions = menu.actions();
    const auto actionIt = std::find_if(
        actions.begin(), actions.end(),
        [&actionText](const QAction* action)
        {
            return action && action->text() == actionText;
        });
    if (actionIt == actions.end() || !(*actionIt)->isEnabled())
    {
        return false;
    }
    (*actionIt)->trigger();
    return true;
}
#endif

std::vector<QPointer<QWidget>> MainWindow::cogScriptTabsForClose(
    QWidget* contextPage,
    const CogScriptTabCloseScope scope) const
{
    const QPointer<QTabWidget> tabs(cogScriptEditorTabsForPage(contextPage));
    if (!tabs || !contextPage
        || tabs->indexOf(contextPage) < 0)
    {
        return {};
    }

    const int contextIndex = tabs->indexOf(contextPage);
    std::vector<QPointer<QWidget>> targets{};
    targets.reserve(static_cast<std::size_t>(tabs->count()));
    for (int index = 0; index < tabs->count(); ++index)
    {
        QWidget* page = tabs->widget(index);
        if (!page || !cogScriptTabCanClose(page))
        {
            continue;
        }

        bool include = false;
        switch (scope)
        {
            case CogScriptTabCloseScope::others:
                include = page != contextPage;
                break;
            case CogScriptTabCloseScope::toRight:
                include = index > contextIndex;
                break;
            case CogScriptTabCloseScope::saved:
            {
                const auto* editor = dynamic_cast<CogScriptEditorDialog*>(page);
                include = editor && !editor->hasModifiedContents();
                break;
            }
            case CogScriptTabCloseScope::all:
                include = true;
                break;
        }
        if (include)
        {
            targets.emplace_back(page);
        }
    }
    return targets;
}

bool MainWindow::closeCogScriptTabs(
    QWidget* contextPage,
    const CogScriptTabCloseScope scope)
{
    const QPointer<QTabWidget> tabs(cogScriptEditorTabsForPage(contextPage));
    if (!tabs || !contextPage
        || tabs->indexOf(contextPage) < 0)
    {
        return false;
    }

    const QPointer<MainWindow> self(this);
    const std::uint64_t generation = resourceEditorDocumentContextGeneration_;
    const QPointer<QWidget> guardedContext(contextPage);
    const std::vector<QPointer<QWidget>> targets =
        cogScriptTabsForClose(contextPage, scope);

    if (targets.empty())
    {
        return false;
    }

    const bool contextIncluded = std::any_of(
        targets.begin(), targets.end(),
        [contextPage](const QPointer<QWidget>& page)
        {
            return page.data() == contextPage;
        });

    // Keep the tab which owned the menu as the stable visual anchor. Modified
    // targets are then visited in current left-to-right order; Cancel stops
    // immediately and never reaches a later buffer.
    tabs->setCurrentWidget(contextPage);
    for (const QPointer<QWidget>& guardedPage : targets)
    {
        if ( !self || !tabs || self->resourceEditorDocumentContextGeneration_ != generation )
        {
            return false;
        }
        if (!guardedPage
            || tabs->indexOf(guardedPage) < 0
            || !cogScriptTabCanClose(guardedPage))
        {
            continue;
        }
        tabs->setCurrentWidget(guardedPage);
        if ( !self || !tabs || !guardedPage || self->resourceEditorDocumentContextGeneration_ != generation )
        {
            return false;
        }
        const bool closed = guardedPage->close();
        if ( !self || !tabs || self->resourceEditorDocumentContextGeneration_ != generation )
        {
            return false;
        }
        if (!closed)
        {
            if (!contextIncluded && guardedContext
                && tabs->indexOf(guardedContext) >= 0)
            {
                tabs->setCurrentWidget(guardedContext);
            }
            return false;
        }
    }

    if ( self && tabs && !contextIncluded && guardedContext
        && tabs->indexOf(guardedContext) >= 0)
    {
        tabs->setCurrentWidget(guardedContext);
    }
    if ( !self || !tabs || self->resourceEditorDocumentContextGeneration_ != generation )
    {
        return false;
    }
    QTimer::singleShot(0, this, [this, tabs]()
        {
            if (tabs && tabs->count() == 0
                && cogScriptEditorDockForTabs(tabs.data()))
            {
                cogScriptEditorDockForTabs(tabs.data())->hide();
            }
        });
    return true;
}

void MainWindow::populateCogScriptTabContextMenu(
    QMenu* menu,
    QWidget* contextPage)
{
    const QPointer<QTabWidget> tabs(cogScriptEditorTabsForPage(contextPage));
    if (!menu || !tabs || !contextPage
        || tabs->indexOf(contextPage) < 0)
    {
        return;
    }

    const QPointer<QWidget> guardedPage(contextPage);
    const bool pinned = contextPage->property("cogPinned").toBool();
    QAction* pinAction = menu->addAction(pinned ? "Unpin tab" : "Pin tab");
    connect(pinAction, &QAction::triggered, menu, [this, guardedPage, tabs]()
        {
            if (!tabs || !guardedPage)
            {
                return;
            }
            guardedPage->setProperty(
                "cogPinned", !guardedPage->property("cogPinned").toBool());
            normalizeCogScriptTabOrder(tabs);
            refreshCogScriptDockTabPresentation(tabs);
            tabs->setCurrentWidget(guardedPage);
            noteRecoveryStateChanged();
        });

    if (const std::optional<QString> authoredPath =
            cogScriptTabRevealPath(contextPage))
    {
        menu->addSeparator();
        QAction* revealAction = menu->addAction("Reveal in Explorer");
        revealAction->setToolTip(
            "Open the folder containing this loose COG file.");
        connect(revealAction, &QAction::triggered, menu,
            [authoredPath = *authoredPath]()
            {
                const QFileInfo info(authoredPath);
#ifdef Q_OS_WIN
                QProcess::startDetached(
                    QStringLiteral("explorer.exe"),
                    { QStringLiteral("/select,")
                        + QDir::toNativeSeparators(info.absoluteFilePath()) });
#else
                QDesktopServices::openUrl(
                    QUrl::fromLocalFile(info.absolutePath()));
#endif
            });
    }

    menu->addSeparator();
    const auto addCloseAction = [this, menu, guardedPage](
                                    const QString& text,
                                    const CogScriptTabCloseScope scope)
    {
        QAction* action = menu->addAction(text);
        action->setEnabled(
            guardedPage && !cogScriptTabsForClose(guardedPage, scope).empty());
        connect(action, &QAction::triggered, menu,
            [this, guardedPage, scope]()
            {
                if (guardedPage)
                {
                    (void)closeCogScriptTabs(guardedPage, scope);
                }
            });
    };

    addCloseAction(QStringLiteral("Close Others"),
        CogScriptTabCloseScope::others);
    addCloseAction(QStringLiteral("Close to the Right"),
        CogScriptTabCloseScope::toRight);
    addCloseAction(QStringLiteral("Close Saved"),
        CogScriptTabCloseScope::saved);
    addCloseAction(QStringLiteral("Close All"),
        CogScriptTabCloseScope::all);
}

void MainWindow::updateCogScriptDockWidthConstraints()
{
    if (!outlinerDock_)
    {
        return;
    }

    const int oldMinWidth = outlinerDock_->minimumWidth();
    const int oldMaxWidth = outlinerDock_->maximumWidth();
    const int oldCogMinWidth = cogScriptEditorDock_ ? cogScriptEditorDock_->minimumWidth() : 0;
    const int oldCogMaxWidth = cogScriptEditorDock_ ? cogScriptEditorDock_->maximumWidth() : 0;
    constexpr int newMaxWidth = QWIDGETSIZE_MAX;

    if (oldMinWidth != kMinimumOutlinerDockWidth)
    {
        outlinerDock_->setMinimumWidth(kMinimumOutlinerDockWidth);
    }
    if (oldMaxWidth != newMaxWidth)
    {
        outlinerDock_->setMaximumWidth(newMaxWidth);
    }
    if (cogScriptEditorDock_ && oldCogMinWidth != kMinimumCogScriptDockWidth)
    {
        cogScriptEditorDock_->setMinimumWidth(kMinimumCogScriptDockWidth);
    }
    if (cogScriptEditorDock_ && oldCogMaxWidth != newMaxWidth)
    {
        cogScriptEditorDock_->setMaximumWidth(newMaxWidth);
    }

    if ((oldMinWidth != kMinimumOutlinerDockWidth
            || oldMaxWidth != newMaxWidth
            || (cogScriptEditorDock_ && oldCogMinWidth != kMinimumCogScriptDockWidth)
            || (cogScriptEditorDock_ && oldCogMaxWidth != newMaxWidth))
        && pendingEditorViewRefitAfterResize_ && editorViewRefitTimer_)
    {
        editorViewRefitTimer_->start();
    }
}

void MainWindow::restoreCogScriptDockWidth()
{
    updateCogScriptDockWidthConstraints();
    if (!workspaceWindow_ || !cogScriptEditorDock_ || cogScriptEditorDock_->isFloating())
    {
        return;
    }

    const int cogWidth =
        std::max(kMinimumCogScriptDockWidth, cogScriptDockPreferredWidth_);
    if (auto* dock = dynamic_cast<ui::PreferredWidthDockWidget*>(cogScriptEditorDock_))
    {
        dock->setPreferredWidth(cogWidth);
    }

    // Resize only the nested workspace dock. The outer outliner belongs to a
    // different layout and cannot be pulled narrower by this operation.
    adjustingCogDockWidths_ = true;
    workspaceWindow_->resizeDocks({ cogScriptEditorDock_ }, { cogWidth }, Qt::Horizontal);
    adjustingCogDockWidths_ = false;
}

void MainWindow::restoreStandaloneOutlinerDockWidth()
{
    if (!outlinerDock_ || outlinerDock_->isFloating())
    {
        updateCogScriptDockWidthConstraints();
        if (pendingEditorViewRefitAfterResize_ && editorViewRefitTimer_ && !editorViewRefitTimer_->isActive())
        {
            editorViewRefitTimer_->start();
        }
        return;
    }

    const int standaloneWidth =
        std::max(kMinimumOutlinerDockWidth, standaloneOutlinerPreferredWidth_);
    if (auto* dock = dynamic_cast<ui::PreferredWidthDockWidget*>(outlinerDock_))
    {
        dock->setPreferredWidth(standaloneWidth);
    }
    // A newly shown QMainWindow may briefly give its sole outer dock all
    // available width before the central workspace has a useful size hint.
    // Pin the preferred width for one layout pass, then immediately release
    // both constraints so subsequent splitter drags remain unrestricted.
    suppressStandaloneOutlinerWidthCapture_ = true;
    adjustingCogDockWidths_ = true;
    outlinerDock_->setMinimumWidth(standaloneWidth);
    outlinerDock_->setMaximumWidth(standaloneWidth);
    resizeDocks(
        { outlinerDock_ },
        { standaloneWidth },
        Qt::Horizontal);
    // Let Qt apply the dock-area ratio through its normal queued layout pass.
    // Forcing QLayout::activate() here can re-enter a dock layout that is still
    // being rebuilt by restoreState()/topLevelChanged and dereference stale
    // layout items.  Keep the temporary pin until the queued release below;
    // that is sufficient to preserve the requested first-show width.
    adjustingCogDockWidths_ = false;
    QTimer::singleShot(0, this, [this]()
        {
            updateCogScriptDockWidthConstraints();
            suppressStandaloneOutlinerWidthCapture_ = false;
            if (pendingEditorViewRefitAfterResize_
                && editorViewRefitTimer_
                && !editorViewRefitTimer_->isActive())
            {
                // Width restoration may precede the first visible GL layout.
                // Keep the delayed top-down refit queued once dimensions settle.
                editorViewRefitTimer_->start();
            }
        });
}

int MainWindow::defaultCogScriptDockWidth() const
{
    // Restore persisted width from previous session if available.
    {
        QSettings settings = settingsStore();
        const int saved = settings.value("cog_editor/dock_width", 0).toInt();
        if (saved >= kMinimumCogScriptDockWidth)
        {
            return saved;
        }
    }

    int viewportWidth = 0;
    if (glView_)
    {
        viewportWidth = glView_->width();
    }
    if (viewportWidth <= 0 && viewportUvSplitter_)
    {
        viewportWidth = viewportUvSplitter_->width();
    }
    if (viewportWidth <= 0 && centralWidget())
    {
        viewportWidth = centralWidget()->width();
    }
    if (viewportWidth <= 0)
    {
        viewportWidth = width();
    }

    // Target roughly half the viewport width for a comfortable split.
    const int halfViewport = std::max(kMinimumCogScriptDockWidth, viewportWidth / 2);
    return halfViewport;
}

std::optional<std::filesystem::path> MainWindow::ensureCogScriptEditablePath(
    const std::string_view scriptName,
    bool& sourceWasLoose)
{
    sourceWasLoose = false;
    const std::string_view trimmed = sed::text::trimView(scriptName);
    if (trimmed.empty())
    {
        return std::nullopt;
    }

    if (const auto path = resources_.findSelectedOnDisk(trimmed))
    {
        sourceWasLoose = true;
        return path;
    }

    const QString scriptIdentity = QString::fromStdString(std::string(trimmed));
    const auto showResourceReadFailure = [&](const QString& detail)
    {
        showCogEditorResourceFailure(
            this,
            "Edit script",
            QString("Couldn't read script:\n%1\n\n%2")
                .arg(scriptIdentity, detail),
            QStringLiteral("COG script open failed for resource '%1': %2")
                .arg(scriptIdentity, detail),
            CogEditorFailureSeverity::warning);
    };

    CogScriptResourceReadResult sourceBytes{};
    try
    {
        sourceBytes = tryReadCogScriptBytesForEditor(resources_, trimmed);
        if ( !sourceBytes.bytes && !sourceBytes.exceededBudget
            && !sed::text::iendsWithAscii(trimmed, ".cog") )
        {
            std::string withExt(trimmed);
            withExt += ".cog";
            if ( const auto path = resources_.findSelectedOnDisk(withExt) )
            {
                sourceWasLoose = true;
                return path;
            }
            sourceBytes = tryReadCogScriptBytesForEditor(resources_, withExt);
        }
    }
    catch ( const std::exception& exception )
    {
        showResourceReadFailure(QString::fromUtf8(exception.what()));
        return std::nullopt;
    }
    catch ( ... )
    {
        showResourceReadFailure(QStringLiteral("Unknown resource read error."));
        return std::nullopt;
    }

    if ( !sourceBytes.bytes )
    {
        if ( sourceBytes.exceededBudget )
        {
            showCogEditorResourceFailure(
                this,
                "Edit script",
                "The script exceeds the 16 MiB source limit.",
                QStringLiteral(
                    "COG script extraction refused: resource '%1' exceeds the 16 MiB source limit.")
                    .arg(scriptIdentity),
                CogEditorFailureSeverity::warning);
        }
        else
        {
            showCogEditorResourceFailure(
                this,
                "Edit script",
                QString("Couldn't find script in resources:\n%1")
                    .arg(scriptIdentity),
                QStringLiteral(
                    "COG script open failed: resource '%1' was not found in mounted resources.")
                    .arg(scriptIdentity),
                CogEditorFailureSeverity::warning);
        }
        return std::nullopt;
    }

    const auto root = cogAuthoringRoot(currentPath_, resourceRoots_.empty() ? resources_.roots() : resourceRoots_);
    if (!root)
    {
        const QString scriptIdentity = QString::fromStdString(
            std::string(trimmed));
        showCogEditorResourceFailure(
            this,
            "Edit script",
            "No resource root is configured; can't extract the script.",
            QStringLiteral(
                "COG script extraction failed for resource '%1': no writable resource root is configured.")
                .arg(scriptIdentity),
            CogEditorFailureSeverity::warning);
        return std::nullopt;
    }

    const QString normalizedName = normalizeCogScriptFileName(
        QString::fromStdString(std::string(trimmed)));
    if (normalizedName.isEmpty())
    {
        const QString scriptIdentity = QString::fromStdString(
            std::string(trimmed));
        showCogEditorResourceFailure(
            this,
            "Edit script",
            QString("Couldn't determine a safe extraction name:\n%1")
                .arg(scriptIdentity),
            QStringLiteral(
                "COG script extraction failed: resource '%1' has no safe filename.")
                .arg(scriptIdentity),
            CogEditorFailureSeverity::warning);
        return std::nullopt;
    }
    const std::filesystem::path fileName{ normalizedName.toStdWString() };

    const std::filesystem::path outDir = *root / "cog";
    std::error_code mkEc;
    std::filesystem::create_directories(outDir, mkEc);
    if (mkEc)
    {
        const QString qDirectory = QString::fromStdWString(outDir.wstring());
        showCogEditorResourceFailure(
            this,
            "Edit script",
            QString("Couldn't create the script directory:\n%1")
                .arg(qDirectory),
            QStringLiteral(
                "COG script extraction failed: couldn't create directory '%1' for resource '%2': %3")
                .arg(
                    qDirectory,
                    QString::fromStdString(std::string(trimmed)),
                    QString::fromStdString(mkEc.message())),
            CogEditorFailureSeverity::warning);
        return std::nullopt;
    }

    const std::filesystem::path outPath = outDir / fileName;
    std::error_code existsEc;
    const bool extractedFileExists = std::filesystem::exists(
        outPath, existsEc);
    if (existsEc)
    {
        const QString qPath = QString::fromStdWString(outPath.wstring());
        showCogEditorResourceFailure(
            this,
            "Edit script",
            QString("Couldn't inspect the extraction path:\n%1").arg(qPath),
            QStringLiteral(
                "COG script extraction failed: couldn't inspect '%1' for resource '%2': %3")
                .arg(
                    qPath,
                    QString::fromStdString(std::string(trimmed)),
                    QString::fromStdString(existsEc.message())),
            CogEditorFailureSeverity::warning);
        return std::nullopt;
    }
    if (!extractedFileExists)
    {
        QSaveFile out(QString::fromStdWString(outPath.wstring()));
        out.setDirectWriteFallback(false);
        if (!out.open(QIODevice::WriteOnly))
        {
            const QString qPath = QString::fromStdWString(outPath.wstring());
            showCogEditorResourceFailure(
                this,
                "Edit script",
                QString("Couldn't extract script:\n%1").arg(qPath),
                QStringLiteral(
                    "COG script extraction failed: couldn't open '%1' for resource '%2'.")
                    .arg(qPath, QString::fromStdString(std::string(trimmed))),
                CogEditorFailureSeverity::warning);
            return std::nullopt;
        }
        const qint64 byteCount = static_cast<qint64>(sourceBytes.bytes->size());
        const char* const byteData = reinterpret_cast<const char*>(sourceBytes.bytes->data());
        if ((byteCount != 0 && out.write(byteData, byteCount) != byteCount) || !out.commit())
        {
            const QString saveError = out.errorString();
            out.cancelWriting();
            const QString qPath = QString::fromStdWString(outPath.wstring());
            showCogEditorResourceFailure(
                this,
                "Edit script",
                QString("Couldn't extract script:\n%1").arg(qPath),
                QStringLiteral(
                    "COG script extraction failed while atomically writing '%1' for resource '%2': %3")
                    .arg(
                        qPath,
                        QString::fromStdString(std::string(trimmed)),
                        saveError),
                CogEditorFailureSeverity::warning);
            return std::nullopt;
        }
    }

    syncCogScriptWatches();
    return outPath;
}

void MainWindow::openCogScriptEditor(const std::string_view scriptName)
{
    if (!documentEditingAllowed())
    {
        return;
    }
    const std::string_view trimmed = sed::text::trimView(scriptName);
    if (trimmed.empty())
    {
        return;
    }

    // Resolve archive metadata before extraction changes the candidate set by
    // adding a writable working copy. The path helper reports the source kind
    // selected by the same precedence walk, without materializing resource bytes.
    QString archiveTooltip = ui::resources::resolveArchiveSourceTooltipWithFallback(
        &resources_, trimmed, ".cog");
    const auto activateOpenCogScriptEditor = [&](const std::filesystem::path& editorPath)
    {
        const std::wstring pathKey = normalizedCogEditorPathKey(editorPath);
        const auto it = openCogScriptEditors_.find(pathKey);
        if ( it == openCogScriptEditors_.end() )
        {
            return false;
        }
        if ( !it->second )
        {
            openCogScriptEditors_.erase(it);
            return false;
        }

        // Re-activating an existing tab must not reread or decode a large
        // script which already has an authoritative live edit buffer.
        it->second->setProperty(
            "cogFullPath", QString::fromStdWString(editorPath.wstring()));
        const QPointer<QWidget> page(it->second);
        const QPointer<QTabWidget> tabs(cogScriptEditorTabsForPage(page.data()));
        const QPointer<QDockWidget> dock(cogScriptEditorDockForTabs(tabs.data()));
        const QPointer<MainWindow> self(this);
        if ( tabs && page )
        {
            updateCogScriptDockTabPresentation(tabs.data(), page.data());
            tabs->setCurrentWidget(page.data());
        }
        if ( dock )
        {
            dock->show();
        }
        if ( dock )
        {
            dock->raise();
        }
        if ( self && tabs )
        {
            self->lastActiveCogScriptEditorHost_ = tabs;
        }
        if ( page )
        {
            page->show();
        }
        if ( page )
        {
            page->setFocus(Qt::OtherFocusReason);
        }
        return true;
    };

    if ( !archiveTooltip.isEmpty() )
    {
        const auto root = cogAuthoringRoot(
            currentPath_, resourceRoots_.empty() ? resources_.roots() : resourceRoots_);
        if ( root )
        {
            const QString normalizedName = normalizeCogScriptFileName(
                QString::fromStdString(std::string(trimmed)));
            if ( !normalizedName.isEmpty() )
            {
                const std::filesystem::path workingCopy =
                    *root / "cog" / std::filesystem::path(normalizedName.toStdWString());
                if ( activateOpenCogScriptEditor(workingCopy) )
                {
                    return;
                }
            }
        }
    }

    bool sourceWasLoose = false;
    const auto path = ensureCogScriptEditablePath(trimmed, sourceWasLoose);
    if (!path)
    {
        return;
    }
    if (sourceWasLoose)
    {
        archiveTooltip.clear();
    }

    if ( activateOpenCogScriptEditor(*path) )
    {
        return;
    }

    const std::wstring pathKey = normalizedCogEditorPathKey(*path);
    const QString qPath = QString::fromStdWString(path->wstring());
    CogScriptSourceReadResult source = readCogScriptSourceBounded(*path);
    if ( !source.succeeded() )
    {
        const QString detail = cogScriptSourceReadError(source, qPath);
        showCogEditorResourceFailure(
            this,
            "Edit script",
            detail,
            QStringLiteral("COG script open failed for requested script '%1': %2")
                .arg(QString::fromStdString(std::string(trimmed)), detail),
            CogEditorFailureSeverity::warning);
        return;
    }
    const QString text = QString::fromUtf8(source.bytes);

    const QPointer<QTabWidget> destination(preferredCogScriptEditorHost_ &&
        cogScriptEditorDockForTabs(preferredCogScriptEditorHost_.data())
        ? preferredCogScriptEditorHost_.data() : cogScriptEditorTabs_);
    const QPointer<QDockWidget> destinationDock(cogScriptEditorDockForTabs(destination.data()));
    if ( !destination || !destinationDock || cogScriptEditorPages().size() >= 1024 )
    {
        return;
    }

    auto* dlg = new CogScriptEditorDialog(
        QString::fromStdString(std::string(trimmed)),
        *path,
        text,
        document_.level().kind,
        [this](const std::filesystem::path& path)
        {
            const QPointer<MainWindow> self(this);
            const std::uint64_t expectedContext = resourceEditorDocumentContextGeneration_;
            syncCogScriptWatches();
            refreshCogScriptFileNow(QString::fromStdWString(path.wstring()));
            markRecoveryCogBufferAsSaved(path);
            noteRecoveryStateChanged();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if ( self && self->cogSaveCallbackForTest_ )
            {
                const auto callback = self->cogSaveCallbackForTest_;
                callback();
            }
#endif
            if ( self && !self->recoveryShuttingDown_
                && self->resourceEditorDocumentContextGeneration_ == expectedContext )
            {
                self->statusBar()->showMessage(QStringLiteral("Saved %1").arg(
                    QDir::toNativeSeparators(QString::fromStdWString(path.wstring()))), 5000);
            }
        },
        [this](const std::filesystem::path& path)
        {
            markRecoveryCogBufferAsSaved(path);
            noteRecoveryStateChanged();
        },
        [this]() { noteRecoveryStateChanged(); },
        [this]() { noteRecoveryStateChanged(true); },
        destination.data());
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->setProperty("cogFullPath", QString::fromStdWString(path->wstring()));
    dlg->setProperty("cogLooseFile", sourceWasLoose);
    dlg->setProperty("cogArchiveTooltip", archiveTooltip);
    dlg->setTitleChangedCallback([this, dlg](const QString& title)
        {
            updateCogScriptDockTabPresentation(cogScriptEditorTabsForPage(dlg), dlg, title);
        });
    dlg->setInlineAssignmentResourcePickerCallback(
        [this](const ui::ResourcePickerKind kind, const QString& currentValue)
        {
            return pickCogInlineAssignmentResource(
                this,
                kind,
                currentValue,
                resourceLocatorSnapshot_,
                templates_,
                document_.level(),
                preferredExtractionRoot(),
                currentPath_);
        });
    const int tabIndex = destination->addTab(dlg, dlg->windowTitle());
    dlg->setProperty("cogPinned", false);
    updateCogScriptDockTabPresentation(destination.data(), dlg, dlg->windowTitle());
    destination->setCurrentIndex(tabIndex);
    if (!cogScriptDockWidthLearned_)
    {
        cogScriptDockPreferredWidth_ = defaultCogScriptDockWidth();
    }
    lastActiveCogScriptEditorHost_ = destination;
    destinationDock->show();
    if ( destinationDock )
    {
        destinationDock->raise();
    }
    if ( destination == cogScriptEditorTabs_ )
    {
        restoreCogScriptDockWidth();
    }
    openCogScriptEditors_[pathKey] = dlg;
    connect(dlg, &QObject::destroyed, this, [this, pathKey]()
        {
            openCogScriptEditors_.erase(pathKey);
            noteRecoveryStateChanged();
            QTimer::singleShot(0, this, [this]()
                {
                    if ( !recoveryShuttingDown_ )
                    {
                        cleanupEmptyCogScriptEditorHosts();
                    }
                });
        });
    const QPointer<MainWindow> watchOwner(this);
    const std::uint64_t watchContext = resourceEditorDocumentContextGeneration_;
    dlg->bindLooseSourceWatch(QCryptographicHash::hash(source.bytes, QCryptographicHash::Sha256),
        [watchOwner, watchContext]()
        {
            return watchOwner && !watchOwner->recoveryShuttingDown_
                && watchOwner->resourceEditorDocumentContextGeneration_ == watchContext;
        });
    dlg->show();
    dlg->setFocus(Qt::OtherFocusReason);
}

bool MainWindow::confirmCloseCogScriptEditors(bool* discardedAny)
{
    if ( discardedAny )
    {
        *discardedAny = false;
    }
    const QPointer<MainWindow> self(this);
    const std::uint64_t generation = resourceEditorDocumentContextGeneration_;
    const auto authoredState = [](const CogScriptEditorDialog* editor)
    {
        if ( !editor )
        {
            return QByteArray{};
        }
        const QCborMap state = editor->recoveryState(false);
        return ui::recovery::serializeCanonicalCbor(QCborValue(QCborArray{
            state.value(QStringLiteral("name")), state.value(QStringLiteral("path")),
            state.value(QStringLiteral("text")), state.value(QStringLiteral("modified"))}));
    };
    const std::vector<QPointer<QWidget>> pages = cogScriptEditorPages();
    std::vector<QPointer<QTabWidget>> hosts{};
    std::vector<QByteArray> resolved{};
    for ( const QPointer<QWidget>& page : pages )
    {
        hosts.emplace_back(cogScriptEditorTabsForPage(page.data()));
    }
    for ( const QPointer<QWidget>& page : pages )
    {
        const QPointer<CogScriptEditorDialog> editor(dynamic_cast<CogScriptEditorDialog*>(page.data()));
        if ( !editor )
        {
            return false;
        }
        bool discarded = false;
        if ( editor->hasModifiedContents() && !editor->confirmPendingClose(&discarded) )
        {
            return false;
        }
        if ( !self || !editor || self->resourceEditorDocumentContextGeneration_ != generation )
        {
            return false;
        }
        if ( discardedAny )
        {
            *discardedAny = *discardedAny || discarded;
        }
        resolved.push_back(authoredState(editor.data()));
    }
    if ( self->cogScriptEditorPages() != pages )
    {
        return false;
    }
    for ( std::size_t index = 0; index < pages.size(); ++index )
    {
        const auto* const editor = dynamic_cast<CogScriptEditorDialog*>(pages[index].data());
        if ( !editor || self->cogScriptEditorTabsForPage(editor) != hosts[index].data()
            || authoredState(editor) != resolved[index] )
        {
            return false;
        }
    }
    return true;
}

void MainWindow::closeCogScriptEditorsAfterReplacement()
{
    const QPointer<MainWindow> self(this);
    for ( const QPointer<QWidget>& page : cogScriptEditorPages() )
    {
        if ( QTabWidget* const tabs = cogScriptEditorTabsForPage(page.data()) )
        {
            tabs->removeTab(tabs->indexOf(page.data()));
        }
        if ( !self )
        {
            return;
        }
        delete page.data();
        if ( !self )
        {
            return;
        }
    }
    openCogScriptEditors_.clear();
    preferredCogScriptEditorHost_.clear();
    lastActiveCogScriptEditorHost_ = cogScriptEditorTabs_;
    cleanupEmptyCogScriptEditorHosts();
    if ( !self )
    {
        return;
    }
    for ( CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        host.sessionGeneration = resourceEditorDocumentContextGeneration_;
    }
    observeCogScriptEditorRecoveryHosts();
}

bool MainWindow::hasUnsavedCogScriptChanges() const
{
    for ( const QPointer<QWidget>& page : cogScriptEditorPages() )
    {
        const auto* const editor = dynamic_cast<CogScriptEditorDialog*>(page.data());
        if ( editor && editor->hasModifiedContents() )
        {
            return true;
        }
    }
    return false;
}

QByteArray MainWindow::captureCogEditorRecoveryState() const
{
    QCborMap root{};
    root.insert(QStringLiteral("version"), 1);
    QCborArray tabs{};
    QCborArray hosts{};
    int active = -1;
    QTabWidget* activeHost = activeCogScriptEditorTabHost();
    // Keyboard routing rejects another window's focus. A workspace snapshot
    // still retains this owner's last selected file when that focus changes.
    if ( !activeHost )
    {
        activeHost = lastActiveCogScriptEditorHost_
            && cogScriptEditorDockForTabs(lastActiveCogScriptEditorHost_.data())
            ? lastActiveCogScriptEditorHost_.data() : cogScriptEditorTabs_;
    }
    const bool preview = !documentEditingAllowed() && previewEditingUiVisibility_.valid
        && previewEditingUiVisibility_.documentEpoch == recoveryDocumentEpoch_;
    bool primaryVisible = false;
    for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        if ( !host.dock || !host.tabs || (!host.primary && host.tabs->count() == 0) )
        {
            continue;
        }
        QCborMap state{};
        state.insert(QStringLiteral("id"), host.stableId);
        state.insert(QStringLiteral("primary"), host.primary);
        bool visible = !host.dock->isHidden();
        if ( preview )
        {
            if ( host.primary )
            {
                visible = previewEditingUiVisibility_.cogScriptEditor;
            }
            else
            {
                for ( const auto& [dock, wasVisible] : previewEditingUiVisibility_.detachedCogScriptEditorVisibility )
                {
                    if ( dock == host.dock )
                    {
                        visible = wasVisible;
                        break;
                    }
                }
            }
        }
        if ( host.primary )
        {
            primaryVisible = visible;
        }
        state.insert(QStringLiteral("visible"), visible);
        state.insert(QStringLiteral("floating"), host.dock->isFloating());
        state.insert(QStringLiteral("geometry"), host.dock->saveGeometry());
        state.insert(QStringLiteral("active"), host.tabs->currentIndex());
        QCborArray members{};
        for ( int index = 0; index < host.tabs->count(); ++index )
        {
            const auto* const editor = dynamic_cast<CogScriptEditorDialog*>(host.tabs->widget(index));
            if ( editor )
            {
                const int flat = static_cast<int>(tabs.size());
                members.append(flat);
                tabs.append(editor->recoveryState());
                if ( host.tabs == activeHost && index == host.tabs->currentIndex() )
                {
                    active = flat;
                }
            }
        }
        state.insert(QStringLiteral("tabs"), members);
        hosts.append(state);
    }
    root.insert(QStringLiteral("active"), active);
    root.insert(QStringLiteral("dockVisible"), primaryVisible);
    root.insert(QStringLiteral("tabs"), tabs);
    root.insert(QStringLiteral("hosts"), hosts);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray MainWindow::captureAuthoredCogRecoveryState() const
{
    struct AuthoredBuffer
    {
        QString key{};
        QString name{};
        QString text{};
    };
    std::vector<AuthoredBuffer> buffers{};
    for ( const QPointer<QWidget>& page : cogScriptEditorPages() )
    {
        const auto* const editor = dynamic_cast<CogScriptEditorDialog*>(page.data());
        if ( !editor || !editor->hasModifiedContents() )
        {
            continue;
        }
        const QCborMap state = editor->recoveryState(false);
        const QString name = state.value(QStringLiteral("name")).toString();
        const QString normalizedPath = ui::recovery::normalizedSourcePath(
            std::filesystem::path(state.value(QStringLiteral("path")).toString().toStdWString()));
        buffers.push_back(AuthoredBuffer{normalizedPath.isEmpty() ? name.toCaseFolded() : normalizedPath,
            name, state.value(QStringLiteral("text")).toString()});
    }
    std::sort(buffers.begin(), buffers.end(), [](const AuthoredBuffer& lhs, const AuthoredBuffer& rhs)
        {
            if ( lhs.key != rhs.key )
            {
                return lhs.key < rhs.key;
            }
            if ( lhs.name != rhs.name )
            {
                return lhs.name < rhs.name;
            }
            return lhs.text < rhs.text;
        });
    QCborArray authored{};
    for ( const AuthoredBuffer& buffer : buffers )
    {
        authored.append(QCborArray{buffer.key, buffer.name, buffer.text});
    }
    return ui::recovery::serializeCanonicalCbor(QCborValue(authored));
}

void MainWindow::markRecoveryCogBufferAsSaved(
    const std::filesystem::path& path)
{
    if (recoverySavedCogState_.isEmpty())
    {
        // No trustworthy baseline exists. Keep eligibility conservative until
        // the next successful document Save/Open/New establishes one.
        recoverySavedCogStateHash_.clear();
        return;
    }

    QCborParserError parseError{};
    const QCborValue decoded = QCborValue::fromCbor(
        recoverySavedCogState_, &parseError);
    if (parseError.error != QCborError::NoError || !decoded.isArray())
    {
        recoverySavedCogState_.clear();
        recoverySavedCogStateHash_.clear();
        return;
    }

    const QString savedKey = ui::recovery::normalizedSourcePath(path);
    QCborArray retained{};
    for (const QCborValue& value : decoded.toArray())
    {
        const QCborArray buffer = value.toArray();
        if (buffer.isEmpty() || buffer.at(0).toString() != savedKey)
        {
            retained.append(value);
        }
    }
    recoverySavedCogState_ = ui::recovery::serializeCanonicalCbor(
        QCborValue(retained));
    recoverySavedCogStateHash_ = QCryptographicHash::hash(
        recoverySavedCogState_, QCryptographicHash::Sha256);
}

bool MainWindow::validateCogEditorRecoveryState(
    const QByteArray& payload,
    QString* warning) const
{
    QString error{};
    const std::optional<QCborMap> decoded = decodeCogEditorRecoveryState(
        payload, &error);
    const bool valid = decoded
        && validateCogEditorRecoveryTabs(*decoded, &error);
    if (!valid && warning) *warning = error;
    return valid;
}

bool MainWindow::restoreCogEditorRecoveryState(
    const QByteArray& payload,
    QStringList* warnings)
{
    QString decodeError{};
    const std::optional<QCborMap> decoded = decodeCogEditorRecoveryState(
        payload, &decodeError);
    if (!decoded)
    {
        if (warnings) warnings->push_back(QStringLiteral(
            "COG Script Editor recovery state was malformed and was skipped: %1.")
                .arg(decodeError));
        return false;
    }
    const QCborMap root = *decoded;
    const QCborArray tabs = root.value(QStringLiteral("tabs")).toArray();
    QString tabsError{};
    if (!validateCogEditorRecoveryTabs(root, &tabsError))
    {
        if (warnings)
        {
            warnings->push_back(QStringLiteral(
                "COG Script Editor recovery state was rejected before any tabs were changed: %1.")
                    .arg(tabsError));
        }
        return false;
    }

    if ( !prepareCogEditorRecoveryHosts(payload) )
    {
        return false;
    }
    const QCborArray hosts = cogRecoveryHosts(root);
    std::vector<QPointer<QWidget>> restored(static_cast<std::size_t>(tabs.size()));
    int restoredCount = 0;
    for (qsizetype tabIndex = 0; tabIndex < tabs.size(); ++tabIndex)
    {
        QCborMap state = tabs.at(tabIndex).toMap();
        const QString scriptName = normalizeCogScriptFileName(
            state.value(QStringLiteral("name")).toString().trimmed());
        const QString capturedPathText = state.value(QStringLiteral("path")).toString();
        const bool recoveredDraft = state.value(QStringLiteral("draft")).toBool(false);
        const std::filesystem::path capturedPath(capturedPathText.toStdWString());
        const QString capturedPathKey = ui::recovery::normalizedSourcePath(
            capturedPath);

        // A serialized path is data, not write authority. Rebind only to the
        // exact loose resource currently mounted for this script, or to the
        // exact project COG destination. Everything else gets a recovery-owned
        // virtual identity and must resolve through the safe draft Save path.
        std::optional<std::filesystem::path> trustedWritablePath{};
        if (!capturedPath.empty())
        {
            if (const auto mountedLoosePath = resources_.findOnDisk(scriptName.toStdString()))
            {
                if (ui::recovery::normalizedSourcePath(*mountedLoosePath)
                    == capturedPathKey)
                {
                    trustedWritablePath = *mountedLoosePath;
                }
            }
            if (!trustedWritablePath && !currentPath_.empty())
            {
                const std::filesystem::path projectCogPath =
                    currentPath_.parent_path() / "cog" / scriptName.toStdWString();
                if (ui::recovery::normalizedSourcePath(projectCogPath)
                    == capturedPathKey)
                {
                    trustedWritablePath = projectCogPath;
                }
            }
        }

        const bool recoverAsDraft = recoveredDraft || !trustedWritablePath;
        const std::filesystem::path recoveredPath = trustedWritablePath
            ? *trustedWritablePath
            : recoveryDirectory() / "recovered-cog"
                / recoverySessionId_.toStdString()
                / (std::to_string(tabIndex) + "-" + scriptName.toStdString());
        const QString recoveredPathKey = ui::recovery::normalizedSourcePath(
            recoveredPath);
        if (recoverAsDraft)
        {
            // A draft must remain visibly unsaved even if a hostile/legacy
            // payload claims its arbitrary-path buffer was clean.
            state.insert(QStringLiteral("modified"), true);
            state.insert(QStringLiteral("draft"), true);
            state.insert(QStringLiteral("loose"), false);
            if ( state.value(QStringLiteral("history")).isMap() )
            {
                QCborMap history = state.value(QStringLiteral("history")).toMap();
                history.insert(QStringLiteral("baselineDirty"), true);
                state.insert(QStringLiteral("history"), history);
            }
        }
        CogScriptEditorDialog* target = nullptr;
        for ( const QPointer<QWidget>& page : cogScriptEditorPages() )
        {
            auto* const candidate = dynamic_cast<CogScriptEditorDialog*>(page.data());
            if ( candidate && !recoveredPathKey.isEmpty()
                && ui::recovery::normalizedSourcePath(candidate->scriptPath()) == recoveredPathKey )
            {
                target = candidate;
                break;
            }
        }
        if (!target && cogScriptEditorTabs_)
        {
            target = new CogScriptEditorDialog(
                scriptName,
                recoveredPath,
                state.value(QStringLiteral("text")).toString(),
                document_.level().kind,
                [this](const std::filesystem::path& savedPath)
                {
                    const QPointer<MainWindow> self(this);
                    const std::uint64_t expectedContext = resourceEditorDocumentContextGeneration_;
                    syncCogScriptWatches();
                    refreshCogScriptFileNow(
                        QString::fromStdWString(savedPath.wstring()));
                    markRecoveryCogBufferAsSaved(savedPath);
                    noteRecoveryStateChanged();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                    if ( self && self->cogSaveCallbackForTest_ )
                    {
                        const auto callback = self->cogSaveCallbackForTest_;
                        callback();
                    }
#endif
                    if ( self && !self->recoveryShuttingDown_
                        && self->resourceEditorDocumentContextGeneration_ == expectedContext )
                    {
                        self->statusBar()->showMessage(QStringLiteral("Saved %1").arg(
                            QDir::toNativeSeparators(QString::fromStdWString(savedPath.wstring()))), 5000);
                    }
                },
                [this](const std::filesystem::path& resolvedPath)
                {
                    markRecoveryCogBufferAsSaved(resolvedPath);
                    noteRecoveryStateChanged();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                    if ( cogDiscardCallbackForTest_ )
                    {
                        const auto callback = cogDiscardCallbackForTest_;
                        callback();
                    }
#endif
                },
                [this]() { noteRecoveryStateChanged(); },
                [this]() { noteRecoveryStateChanged(true); },
                cogScriptEditorTabs_);
            if (recoverAsDraft)
            {
                auto resolveSavePath = [this, scriptName]() -> std::optional<std::filesystem::path>
                {
                    const auto root = cogAuthoringRoot(currentPath_, resourceRoots_.empty() ? resources_.roots() : resourceRoots_);
                    return root ? std::optional<std::filesystem::path>(*root / "cog" / scriptName.toStdWString()) : std::nullopt;
                };
                auto pathChanged = [this](
                    CogScriptEditorDialog* editor,
                    const std::filesystem::path& oldPath,
                    const std::filesystem::path& newPath)
                {
                    for (auto it = openCogScriptEditors_.begin(); it != openCogScriptEditors_.end();)
                    {
                        if (!it->second || it->second.data() == editor)
                        {
                            it = openCogScriptEditors_.erase(it);
                        }
                        else
                        {
                            ++it;
                        }
                    }
                    openCogScriptEditors_[normalizedCogEditorPathKey(newPath)] = editor;
                    editor->setProperty(
                        "cogFullPath", QString::fromStdWString(newPath.wstring()));
                    editor->setProperty("cogLooseFile", true);
                    syncCogScriptWatches();
                    refreshCogScriptFileNow(QString::fromStdWString(newPath.wstring()));
                    markRecoveryCogBufferAsSaved(oldPath);
                    markRecoveryCogBufferAsSaved(newPath);
                    noteRecoveryStateChanged();
                };
                target->markAsUnsavedDraft(
                    std::move(resolveSavePath), std::move(pathChanged));
            }
            target->setAttribute(Qt::WA_DeleteOnClose, true);
            target->setProperty("cogFullPath", QString::fromStdWString(recoveredPath.wstring()));
            target->setProperty(
                "cogLooseFile", trustedWritablePath.has_value() && !recoverAsDraft);
            target->setTitleChangedCallback([this, target](const QString& title)
                {
                    updateCogScriptDockTabPresentation(cogScriptEditorTabsForPage(target), target, title);
                });
            target->setInlineAssignmentResourcePickerCallback(
                [this](const ui::ResourcePickerKind kind, const QString& currentValue)
                {
                    return pickCogInlineAssignmentResource(
                        this,
                        kind,
                        currentValue,
                        resourceLocatorSnapshot_,
                        templates_,
                        document_.level(),
                        preferredExtractionRoot(),
                        currentPath_);
                });
            cogScriptEditorTabs_->addTab(target, target->windowTitle());
            const std::wstring pathKey = normalizedCogEditorPathKey(recoveredPath);
            openCogScriptEditors_[pathKey] = target;
            connect(target, &QObject::destroyed, this, [this]()
                {
                    for (auto it = openCogScriptEditors_.begin(); it != openCogScriptEditors_.end();)
                    {
                        if (!it->second)
                        {
                            it = openCogScriptEditors_.erase(it);
                        }
                        else
                        {
                            ++it;
                        }
                    }
                    noteRecoveryStateChanged();
                });
        }
        if (!target || !target->restoreRecoveryState(state))
        {
            if (warnings) warnings->push_back(QStringLiteral(
                "Could not reopen recovered COG buffer %1.").arg(scriptName));
            continue;
        }
        const QPointer<MainWindow> watchOwner(this);
        const std::uint64_t watchContext = resourceEditorDocumentContextGeneration_;
        target->bindLooseSourceWatch({}, [watchOwner, watchContext]()
            {
                return watchOwner && !watchOwner->recoveryShuttingDown_
                    && watchOwner->resourceEditorDocumentContextGeneration_ == watchContext;
            });
        if (trustedWritablePath)
        {
            target->offerReloadIfDiskIsNewerThanCheckpoint(
                nativeDocumentTimestamp_);
        }
        updateCogScriptDockTabPresentation(cogScriptEditorTabsForPage(target), target);
        restored[static_cast<std::size_t>(tabIndex)] = target;
        ++restoredCount;
    }

    const QPointer<MainWindow> self(this);
    const std::uint64_t generation = resourceEditorDocumentContextGeneration_;
    for ( const QCborValue& hostValue : hosts )
    {
        const QCborMap hostState = hostValue.toMap();
        const QString id = hostState.value(QStringLiteral("id")).toString();
        const auto it = std::find_if(cogScriptEditorTabHosts_.begin(), cogScriptEditorTabHosts_.end(),
            [&id](const CogScriptEditorTabHost& host)
            {
                return host.stableId == id;
            });
        if ( it == cogScriptEditorTabHosts_.end() || !it->tabs || !it->dock )
        {
            return false;
        }
        const QPointer<QTabWidget> destination(it->tabs);
        const QPointer<QDockWidget> dock(it->dock);
        const QCborArray members = hostState.value(QStringLiteral("tabs")).toArray();
        for ( const QCborValue& member : members )
        {
            const QPointer<QWidget> page(restored[static_cast<std::size_t>(member.toInteger())]);
            if ( !page )
            {
                return false;
            }
            if ( QTabWidget* const source = cogScriptEditorTabsForPage(page.data()) )
            {
                const QSignalBlocker blocker(source);
                source->removeTab(source->indexOf(page.data()));
            }
            if ( !self || !destination || !page || self->resourceEditorDocumentContextGeneration_ != generation )
            {
                return false;
            }
            const QSignalBlocker blocker(destination.data());
            destination->addTab(page.data(), page->windowTitle());
            updateCogScriptDockTabPresentation(destination.data(), page.data());
        }
        destination->setCurrentIndex(static_cast<int>(hostState.value(QStringLiteral("active")).toInteger()));
        if ( !self || !dock || !destination || self->resourceEditorDocumentContextGeneration_ != generation )
        {
            return false;
        }
        dock->setFloating(hostState.value(QStringLiteral("floating")).toBool());
        if ( !self || !dock )
        {
            return false;
        }
        if ( dock->isFloating() )
        {
            if ( QApplication::mouseButtons().testFlag(Qt::LeftButton)
                || QWidget::mouseGrabber() == dock.data() )
            {
                return false;
            }
            ensureFloatingCogMaximizeButton(dock.data());
            if ( !self || !dock )
            {
                return false;
            }
        }
        const QByteArray geometry = hostState.value(QStringLiteral("geometry")).toByteArray();
        // QWidget's geometry codec restores a top-level window and constrains
        // its frame to a screen. The nested QMainWindow state already owns a
        // docked host's local rectangle, including when that host is hidden.
        if ( dock->isFloating() && !geometry.isEmpty() )
        {
            dock->restoreGeometry(geometry);
        }
        if ( !self || !dock )
        {
            return false;
        }
        if ( dock->isFloating() )
        {
            QScreen* screen = QGuiApplication::screenAt(dock->frameGeometry().center());
            if ( !screen )
            {
                screen = QGuiApplication::primaryScreen();
            }
            if ( screen )
            {
                const QRect available = screen->availableGeometry();
                const QRect frame = dock->frameGeometry();
                const QPoint origin(std::clamp(frame.left(), available.left(),
                    std::max(available.left(), available.right() - frame.width() + 1)),
                    std::clamp(frame.top(), available.top(),
                        std::max(available.top(), available.bottom() - frame.height() + 1)));
                dock->move(dock->pos() + origin - frame.topLeft());
            }
        }
        if ( dock )
        {
            dock->setVisible(hostState.value(QStringLiteral("visible")).toBool());
        }
        if ( !self || self->resourceEditorDocumentContextGeneration_ != generation )
        {
            return false;
        }
    }
    const qint64 active = root.value(QStringLiteral("active")).toInteger();
    if ( active >= 0 && active < static_cast<qint64>(restored.size()) )
    {
        lastActiveCogScriptEditorHost_ = cogScriptEditorTabsForPage(restored[static_cast<std::size_t>(active)].data());
    }
    cleanupEmptyCogScriptEditorHosts();
    if ( self )
    {
        refreshCogScriptEditorToolbars();
    }
    return restoredCount == tabs.size();
}

QString MainWindow::newCogScriptNameValidationError(const QString& typedName) const
{
    const QString normalized = normalizeCogScriptFileName(typedName);
    if (normalized.isEmpty())
    {
        return QStringLiteral("Enter a script name.");
    }
    if (isUnsafeCogDraftName(typedName))
    {
        return QStringLiteral(
            "Enter a COG script filename without directory components.");
    }

    // The editor identity is the normalized basename, not the spelling used
    // in a tab.  Keep this check case-insensitive so `Foo`, `foo.cog`, and
    // `foo.COG` cannot produce duplicate tabs or draft paths.
    if (cogScriptEditorTabs_)
    {
        for (int i = 0; i < cogScriptEditorTabs_->count(); ++i)
        {
            const auto* existing = dynamic_cast<const CogScriptEditorDialog*>(
                cogScriptEditorTabs_->widget(i));
            if (existing && existing->scriptName().compare(normalized, Qt::CaseInsensitive) == 0)
            {
                return QStringLiteral("A COG script with this name is already open.");
            }
        }
    }
    // Detached/recovery-reopened editors are indexed independently of the
    // tab widget.  Check that registry as well so a same-name open buffer
    // cannot be hidden by a dock/layout state.
    for (const auto& [_, ptr] : openCogScriptEditors_)
    {
        const auto* existing = dynamic_cast<const CogScriptEditorDialog*>(ptr.data());
        if (existing && existing->scriptName().compare(normalized, Qt::CaseInsensitive) == 0)
        {
            return QStringLiteral("A COG script with this name is already open.");
        }
    }

    // Mounted GOB entries are already indexed and can be checked immediately.
    // Direct roots are deliberately not probed here: ResourceLocator's disk
    // lookup performs filesystem I/O and is handled by the debounced worker in
    // createNewCogScript().
    const std::string resourceName = normalized.toStdString();
    if (resources_.findInGobs(resourceName).has_value())
    {
        return QStringLiteral("A COG script with this name already exists in the project resources.");
    }

    return {};
}

void MainWindow::createNewCogScript()
{
    if (!documentEditingAllowed())
    {
        return;
    }
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("newCogScriptDialog"));
    dialog.setWindowTitle(QStringLiteral("New COG Script"));
    auto* layout = new QFormLayout(&dialog);
    auto* nameEdit = new QLineEdit(&dialog);
    nameEdit->setObjectName(QStringLiteral("newCogScriptNameEdit"));
    nameEdit->setPlaceholderText(QStringLiteral("script_name.cog"));
    layout->addRow(QStringLiteral("Script name"), nameEdit);

    auto* validationLabel = new QLabel(&dialog);
    validationLabel->setObjectName(QStringLiteral("newCogScriptValidationLabel"));
    validationLabel->setWordWrap(true);
    validationLabel->setStyleSheet(QStringLiteral("color: #ff6b6b;"));
    validationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addRow(QString(), validationLabel);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    QPushButton* okButton = buttons->button(QDialogButtonBox::Ok);

    auto setValidation = [validationLabel, okButton](const QString& error, const bool pending)
    {
        validationLabel->setText(error);
        validationLabel->setVisible(pending || !error.isEmpty());
        if (okButton)
        {
            okButton->setEnabled(!pending && error.isEmpty());
        }
    };

    // Disk/root probing is intentionally debounced and asynchronous.  GOB
    // indexes and open tabs are cheap in-memory checks, but ResourceLocator's
    // direct-root lookup can touch a slow/network filesystem.  Each probe
    // carries a generation and normalized name so stale results can never
    // overwrite feedback for text the user has continued typing.
    QTimer validationDebounce(&dialog);
    validationDebounce.setSingleShot(true);
    validationDebounce.setInterval(75);
    std::uint64_t validationGeneration = 0;
    bool filesystemProbePending = false;
    bool filesystemConflict = false;
    QString validatedFilesystemName{};

    auto startDiskProbe = [&dialog, &validationGeneration, &filesystemProbePending,
                           &filesystemConflict, &validatedFilesystemName,
                           setValidation](const std::uint64_t generation,
                                          const QString& normalized,
                                          std::vector<std::filesystem::path> paths)
    {
        auto* watcher = new QFutureWatcher<CogDiskProbeResult>(&dialog);
        QObject::connect(watcher, &QFutureWatcher<CogDiskProbeResult>::finished, &dialog,
            [watcher, generation, normalized, &validationGeneration,
             &filesystemProbePending, &filesystemConflict,
             &validatedFilesystemName, setValidation]()
            {
                CogDiskProbeResult probe{};
                try
                {
                    probe = watcher->result();
                }
                catch (...)
                {
                    probe.error = true;
                }
                watcher->deleteLater();
                if (generation != validationGeneration)
                {
                    return;
                }

                filesystemProbePending = false;
                filesystemConflict = probe.conflict || probe.error;
                validatedFilesystemName = normalized;
                setValidation(
                    probe.error
                        ? QStringLiteral("The project files could not be checked. Continue typing or retry.")
                        : (probe.conflict
                            ? QStringLiteral("A COG script with this name already exists in the project files.")
                            : QString{}),
                    false);
            });
        watcher->setFuture(QtConcurrent::run(
            [paths = std::move(paths)]() -> CogDiskProbeResult
            {
                CogDiskProbeResult result{};
                for (const std::filesystem::path& path : paths)
                {
                    const std::optional<bool> exists = filesystemPathExistsCaseInsensitive(path);
                    if (!exists)
                    {
                        result.error = true;
                        return result;
                    }
                    if (*exists)
                    {
                        result.conflict = true;
                        return result;
                    }
                }
                return result;
            }));
    };

    auto refreshValidation = [this, &dialog, nameEdit, setValidation,
                              &validationDebounce, &validationGeneration,
                              &filesystemProbePending, &filesystemConflict,
                              &validatedFilesystemName, startDiskProbe]()
    {
        ++validationGeneration;
        const std::uint64_t generation = validationGeneration;
        const QString normalized = normalizeCogScriptFileName(nameEdit->text());
        const QString immediateError = newCogScriptNameValidationError(nameEdit->text());
        validationDebounce.stop();
        filesystemProbePending = false;
        filesystemConflict = false;
        validatedFilesystemName.clear();

        if (!immediateError.isEmpty())
        {
            setValidation(immediateError, false);
            return;
        }

        std::vector<std::filesystem::path> paths{};
        const std::string resourceName = normalized.toStdString();
        for (const std::filesystem::path& root : this->resources_.roots())
        {
            for (const std::string& candidate : this->resources_.candidates(resourceName))
            {
                paths.push_back(root / std::filesystem::path(candidate));
            }
        }
        if (!this->currentPath_.empty())
        {
            paths.push_back(this->currentPath_.parent_path() / "cog" / normalized.toStdWString());
        }
        std::sort(paths.begin(), paths.end());
        paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

        if (paths.empty())
        {
            validatedFilesystemName = normalized;
            setValidation(QString{}, false);
            return;
        }

        filesystemProbePending = true;
        setValidation(QStringLiteral("Checking project files..."), true);
        QObject::disconnect(&validationDebounce, nullptr, &dialog, nullptr);
        QObject::connect(&validationDebounce, &QTimer::timeout, &dialog,
            [generation, normalized, paths = std::move(paths), startDiskProbe]() mutable
            {
                startDiskProbe(generation, normalized, std::move(paths));
            });
        // Connect before starting the one-shot timer.  Starting first can
        // race a zero/very-short timer on a busy event loop and leave the
        // current name permanently in the pending state.
        validationDebounce.start();
    };
    QObject::connect(nameEdit, &QLineEdit::textChanged, &dialog,
        [refreshValidation](const QString&) { refreshValidation(); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog,
        [this, &dialog, nameEdit, refreshValidation, &filesystemProbePending,
         &filesystemConflict, &validatedFilesystemName, &validationGeneration,
         setValidation]()
        {
            const QString normalized = normalizeCogScriptFileName(nameEdit->text());
            const QString immediateError = newCogScriptNameValidationError(nameEdit->text());
            if (!immediateError.isEmpty())
            {
                setValidation(immediateError, false);
                return;
            }
            if (filesystemProbePending
                || validatedFilesystemName != normalized
                || filesystemConflict)
            {
                // A second asynchronous probe is required before accepting;
                // never synchronously touch a project filesystem from the
                // button callback.
                refreshValidation();
                return;
            }
            dialog.accept();
        });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.setMinimumWidth(420);
    ui::applyFixedDialogWindowPolicy(dialog);
    refreshValidation();
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }
    createNewCogScriptNamed(nameEdit->text());
}

void MainWindow::createNewCogScriptNamed(const QString& typedName)
{
    if (!documentEditingAllowed())
    {
        return;
    }
    const QString normalized = normalizeCogScriptFileName(typedName);
    const QString validationError = newCogScriptNameValidationError(typedName);
    if (!validationError.isEmpty())
    {
        if (normalized.isEmpty())
        {
            return;
        }

        // This path is also used by recovery/tests and can be reached without
        // the live dialog.  Preserve the previous non-destructive behavior,
        // but make the reason consistent with the interactive validator.
        (void)showCogEditorMessageBox(
            this,
            QMessageBox::Information,
            QStringLiteral("New COG Script"),
            validationError);
        return;
    }

    // The dialog's asynchronous probe keeps typing responsive, but the
    // filesystem may change after that probe completes.  Keep this final
    // synchronous guard immediately before creating the draft so a newly
    // arriving file can never be shadowed by a same-name COG.
    bool diskConflict = false;
    bool diskProbeError = false;
    const std::string resourceName = normalized.toStdString();
    for (const std::filesystem::path& root : resources_.roots())
    {
        for (const std::string& candidate : resources_.candidates(resourceName))
        {
            const std::optional<bool> exists = filesystemPathExistsCaseInsensitive(
                root / std::filesystem::path(candidate));
            // A final race check is deliberately conservative: if the
            // directory cannot be inspected, do not create a draft which
            // could shadow an existing script.
            if (!exists)
            {
                diskProbeError = true;
                break;
            }
            if (*exists)
            {
                diskConflict = true;
                break;
            }
        }
        if (diskConflict || diskProbeError)
        {
            break;
        }
    }
    if (!diskConflict && !diskProbeError && !currentPath_.empty())
    {
        const std::optional<bool> exists = filesystemPathExistsCaseInsensitive(
            currentPath_.parent_path() / "cog" / normalized.toStdWString());
        diskProbeError = !exists;
        diskConflict = exists && *exists;
    }
    if (diskProbeError)
    {
        const QString scriptIdentity = normalized;
        showCogEditorResourceFailure(
            this,
            QStringLiteral("New COG Script"),
            QStringLiteral(
                "The project files could not be checked. The script was not created; please retry."),
            QStringLiteral(
                "COG script creation failed: project files couldn't be checked for buffer '%1'.")
                .arg(scriptIdentity),
            CogEditorFailureSeverity::warning);
        return;
    }
    if (diskConflict)
    {
        (void)showCogEditorMessageBox(
            this,
            QMessageBox::Information,
            QStringLiteral("New COG Script"),
            QStringLiteral(
                "A COG script with this name already exists in the project files."));
        return;
    }

    const QPointer<QTabWidget> destination(preferredCogScriptEditorHost_ &&
        cogScriptEditorDockForTabs(preferredCogScriptEditorHost_.data())
        ? preferredCogScriptEditorHost_.data() : cogScriptEditorTabs_);
    const QPointer<QDockWidget> destinationDock(cogScriptEditorDockForTabs(destination.data()));
    if ( !destination || !destinationDock || cogScriptEditorPages().size() >= 1024 )
    {
        return;
    }

    // This path is an identity only: creation does not create it or any parent
    // directories. The first explicit Save resolves to <project>/cog/name.
    const std::filesystem::path virtualPath =
        recoveryDirectory() / "draft-cog" / recoverySessionId_.toStdString()
        / normalized.toStdWString();
    auto resolveSavePath = [this, normalized]() -> std::optional<std::filesystem::path>
    {
        const auto root = cogAuthoringRoot(currentPath_, resourceRoots_.empty() ? resources_.roots() : resourceRoots_);
        return root ? std::optional<std::filesystem::path>(*root / "cog" / normalized.toStdWString()) : std::nullopt;
    };
    auto pathChanged = [this](
        CogScriptEditorDialog* editor,
        const std::filesystem::path& oldPath,
        const std::filesystem::path& newPath)
    {
        for (auto it = openCogScriptEditors_.begin(); it != openCogScriptEditors_.end();)
        {
            if (!it->second || it->second.data() == editor)
            {
                it = openCogScriptEditors_.erase(it);
            }
            else
            {
                ++it;
            }
        }
        openCogScriptEditors_[normalizedCogEditorPathKey(newPath)] = editor;
        editor->setProperty("cogFullPath", QString::fromStdWString(newPath.wstring()));
        editor->setProperty("cogLooseFile", true);
        syncCogScriptWatches();
        refreshCogScriptFileNow(QString::fromStdWString(newPath.wstring()));
        markRecoveryCogBufferAsSaved(oldPath);
        markRecoveryCogBufferAsSaved(newPath);
        noteRecoveryStateChanged();
    };

    auto* editor = new CogScriptEditorDialog(
        normalized,
        virtualPath,
        ui::cog::skeletonSnippet(),
        document_.level().kind,
        [this](const std::filesystem::path& savedPath)
        {
            const QPointer<MainWindow> self(this);
            const std::uint64_t expectedContext = resourceEditorDocumentContextGeneration_;
            syncCogScriptWatches();
            refreshCogScriptFileNow(QString::fromStdWString(savedPath.wstring()));
            markRecoveryCogBufferAsSaved(savedPath);
            noteRecoveryStateChanged();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if ( self && self->cogSaveCallbackForTest_ )
            {
                const auto callback = self->cogSaveCallbackForTest_;
                callback();
            }
#endif
            if ( self && !self->recoveryShuttingDown_
                && self->resourceEditorDocumentContextGeneration_ == expectedContext )
            {
                self->statusBar()->showMessage(QStringLiteral("Saved %1").arg(
                    QDir::toNativeSeparators(QString::fromStdWString(savedPath.wstring()))), 5000);
            }
        },
        [this](const std::filesystem::path& resolvedPath)
        {
            markRecoveryCogBufferAsSaved(resolvedPath);
            noteRecoveryStateChanged();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if ( cogDiscardCallbackForTest_ )
            {
                const auto callback = cogDiscardCallbackForTest_;
                callback();
            }
#endif
        },
        [this]() { noteRecoveryStateChanged(); },
        [this]() { noteRecoveryStateChanged(true); },
        destination.data());
    editor->markAsUnsavedDraft(std::move(resolveSavePath), std::move(pathChanged));
    const QPointer<MainWindow> watchOwner(this);
    const std::uint64_t watchContext = resourceEditorDocumentContextGeneration_;
    editor->bindLooseSourceWatch({}, [watchOwner, watchContext]()
        {
            return watchOwner && !watchOwner->recoveryShuttingDown_
                && watchOwner->resourceEditorDocumentContextGeneration_ == watchContext;
        });
    editor->setAttribute(Qt::WA_DeleteOnClose, true);
    editor->setProperty("cogFullPath", QString::fromStdWString(virtualPath.wstring()));
    editor->setTitleChangedCallback([this, editor](const QString& title)
        {
            updateCogScriptDockTabPresentation(cogScriptEditorTabsForPage(editor), editor, title);
        });
    editor->setInlineAssignmentResourcePickerCallback(
        [this](const ui::ResourcePickerKind kind, const QString& currentValue)
        {
            return pickCogInlineAssignmentResource(
                this,
                kind,
                currentValue,
                resourceLocatorSnapshot_,
                templates_,
                document_.level(),
                preferredExtractionRoot(),
                currentPath_);
        });
    const int tabIndex = destination->addTab(editor, editor->windowTitle());
    editor->setProperty("cogPinned", false);
    updateCogScriptDockTabPresentation(destination.data(), editor, editor->windowTitle());
    destination->setCurrentIndex(tabIndex);
    if (!cogScriptDockWidthLearned_)
    {
        cogScriptDockPreferredWidth_ = defaultCogScriptDockWidth();
    }
    lastActiveCogScriptEditorHost_ = destination;
    destinationDock->show();
    if ( destinationDock )
    {
        destinationDock->raise();
    }
    if ( destination == cogScriptEditorTabs_ )
    {
        restoreCogScriptDockWidth();
    }
    openCogScriptEditors_[normalizedCogEditorPathKey(virtualPath)] = editor;
    connect(editor, &QObject::destroyed, this, [this]()
        {
            for (auto it = openCogScriptEditors_.begin(); it != openCogScriptEditors_.end();)
            {
                if (!it->second)
                {
                    it = openCogScriptEditors_.erase(it);
                }
                else
                {
                    ++it;
                }
            }
            noteRecoveryStateChanged();
            QTimer::singleShot(0, this, [this]()
                {
                    if ( !recoveryShuttingDown_ )
                    {
                        cleanupEmptyCogScriptEditorHosts();
                    }
                });
        });
    editor->show();
    editor->setFocus(Qt::OtherFocusReason);
    noteRecoveryStateChanged(true);

    try
    {
        const sed::assets::CogSymbolLoadResult draftSchema =
            sed::assets::loadCogSymbolsFromTextResult(
                ui::cog::skeletonSnippet().toStdString(), false);
        if (!draftSchema.loaded()
            || !createPlacedCogFromValues(
                normalized.toStdString(), draftSchema.symbols, true))
        {
            showCogEditorApplicationFailure(
                this,
                "New COG Script",
                "The script draft was opened, but the placed COG entry could not be added to the level.",
                QStringLiteral(
                    "COG script draft '%1' opened, but its placed COG entry couldn't be added to the level.")
                    .arg(normalized),
                CogEditorFailureSeverity::warning);
        }
    }
    catch (const std::exception& ex)
    {
        const QString exceptionText = QString::fromUtf8(ex.what());
        showCogEditorApplicationFailure(
            this,
            "New COG Script",
            QString("The script draft was opened, but adding the placed COG failed:\n%1")
                .arg(exceptionText),
            QStringLiteral(
                "COG script draft '%1' opened, but adding its placed COG entry failed: %2")
                .arg(normalized, exceptionText),
            CogEditorFailureSeverity::error);
    }
    catch (...)
    {
        showCogEditorApplicationFailure(
            this,
            "New COG Script",
            "The script draft was opened, but adding the placed COG failed.",
            QStringLiteral(
                "COG script draft '%1' opened, but adding its placed COG entry failed with an unknown exception.")
                .arg(normalized),
            CogEditorFailureSeverity::error);
    }
}

void MainWindow::refreshCogScriptEditorAppearance()
{
    for (auto it = openCogScriptEditors_.begin(); it != openCogScriptEditors_.end();)
    {
        if (auto* editor = dynamic_cast<CogScriptEditorDialog*>(it->second.data()))
        {
            editor->reloadAppearanceFromSettings();
            ++it;
            continue;
        }

        it = openCogScriptEditors_.erase(it);
    }
}

void MainWindow::previewCogScriptEditorFont(const QFont& font)
{
    for (auto it = openCogScriptEditors_.begin(); it != openCogScriptEditors_.end();)
    {
        if (auto* editor = dynamic_cast<CogScriptEditorDialog*>(it->second.data()))
        {
            editor->previewBaseEditorFont(font);
            ++it;
            continue;
        }

        it = openCogScriptEditors_.erase(it);
    }
}


void MainWindow::setupCogScriptEditorActions()
{
    // Step 1: locate the COG editor which actually owns keyboard focus.
    // Shared keyboard commands use this strict form so an open-but-unfocused
    // tab can never steal a viewport/application accelerator.
    auto findFocusedCogScriptEditor = [this]() -> CogScriptEditorDialog*
    {
        if (QWidget* focus = QApplication::focusWidget())
        {
            for (QWidget* current = focus; current; current = current->parentWidget())
            {
                if (auto* editor = dynamic_cast<CogScriptEditorDialog*>(current))
                {
                    if (cogScriptEditorTabsForPage(editor))
                    {
                        return editor;
                    }
                    return nullptr;
                }
            }
        }
        if (cogEditorFocusActive_ && activeCogScriptEditorTabHost())
        {
            return dynamic_cast<CogScriptEditorDialog*>(
                activeCogScriptEditorTabHost()->currentWidget());
        }
        return nullptr;
    };

    auto* cogEditorReloadAction = new QAction("Reload", this);
    cogEditorReloadAction->setObjectName("cog_editor.reload");
    cogEditorReloadAction->setShortcut(QKeySequence::Refresh);
    cogEditorReloadAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(cogEditorReloadAction, &QAction::triggered, this, [findFocusedCogScriptEditor]()
        {
            if (auto* editor = findFocusedCogScriptEditor())
            {
                (void)editor->triggerReloadFromDisk();
            }
        });

    // Edit > Find owns the application's one Ctrl+F shortcut. Route that
    // existing action by actual focus rather than registering a competing
    // COG ApplicationShortcut (which Qt correctly treats as ambiguous).
    if (findAction_)
    {
        disconnect(findAction_, &QAction::triggered,
            this, &MainWindow::showLevelQueryDialog);
        connect(findAction_, &QAction::triggered, this,
            [this, findFocusedCogScriptEditor]()
            {
                if (auto* editor = findFocusedCogScriptEditor())
                {
                    editor->triggerFindFocus();
                    return;
                }
                showLevelQueryDialog();
            });
    }
    if (findNextAction_)
    {
        disconnect(findNextAction_, &QAction::triggered,
            this, &MainWindow::findNextLevelQuery);
        connect(findNextAction_, &QAction::triggered, this,
            [this, findFocusedCogScriptEditor]()
            {
                if (auto* editor = findFocusedCogScriptEditor())
                {
                    editor->triggerFindNext();
                    return;
                }
                findNextLevelQuery();
            });
    }

    auto* cogEditorCompletionAction = new QAction("Completion", this);
    cogEditorCompletionAction->setObjectName("cog_editor.complete");
    cogEditorCompletionAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Space")));
    cogEditorCompletionAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(cogEditorCompletionAction, &QAction::triggered, this, [findFocusedCogScriptEditor]()
        {
            if (auto* editor = findFocusedCogScriptEditor())
            {
                editor->triggerCompletion();
            }
        });

    auto* cogEditorFindPrevAction = new QAction("Find Previous", this);
    cogEditorFindPrevAction->setObjectName("cog_editor.find_previous");
    cogEditorFindPrevAction->setShortcut(QKeySequence::FindPrevious);
    cogEditorFindPrevAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(cogEditorFindPrevAction, &QAction::triggered, this, [this, findFocusedCogScriptEditor]()
        {
            if (auto* editor = findFocusedCogScriptEditor())
            {
                editor->triggerFindPrevious();
            }
        });

    auto* cogEditorFormatAction = new QAction("Format", this);
    cogEditorFormatAction->setObjectName("cog_editor.format");
    cogEditorFormatAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+F")));
    cogEditorFormatAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(cogEditorFormatAction, &QAction::triggered, this, [findFocusedCogScriptEditor]()
        {
            if (auto* editor = findFocusedCogScriptEditor())
            {
                editor->triggerFormat();
            }
        });

    auto* cogEditorWhitespaceAction = new QAction("Toggle Whitespace", this);
    cogEditorWhitespaceAction->setObjectName("cog_editor.toggle_whitespace");
    cogEditorWhitespaceAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+W")));
    cogEditorWhitespaceAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(cogEditorWhitespaceAction, &QAction::triggered, this, [findFocusedCogScriptEditor]()
        {
            if (auto* editor = findFocusedCogScriptEditor())
            {
                editor->triggerToggleWhitespace();
            }
        });

    auto* cogEditorDefinitionAction = new QAction("Go To Definition", this);
    cogEditorDefinitionAction->setObjectName("cog_editor.goto_definition");
    cogEditorDefinitionAction->setShortcut(QKeySequence(Qt::Key_F12));
    cogEditorDefinitionAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(cogEditorDefinitionAction, &QAction::triggered, this, [findFocusedCogScriptEditor]()
        {
            if (auto* editor = findFocusedCogScriptEditor())
            {
                editor->triggerJumpToDefinition();
            }
        });

    const auto makeEditorCommand = [this, findFocusedCogScriptEditor](
                                      const QString& label,
                                      const char* id,
                                      const QString& description,
                                      void (CogScriptEditorDialog::*command)())
    {
        auto* action = new QAction(label, this);
        action->setObjectName(QString::fromLatin1(id));
        action->setProperty("sed_keymap_expose", true);
        action->setToolTip(description);
        action->setStatusTip(description);
        action->setShortcutContext(Qt::ApplicationShortcut);
        connect(action, &QAction::triggered, this, [findFocusedCogScriptEditor, command]()
            {
                if ( auto* editor = findFocusedCogScriptEditor() )
                {
                    (editor->*command)();
                }
            });
        return action;
    };
    QAction* const cogEditorInsertAction = makeEditorCommand(
        QStringLiteral("Insert"), "cog_editor.insert_context",
        QStringLiteral("At the focused script caret, add a symbol in Symbols or a message/code label outside an existing handler."),
        &CogScriptEditorDialog::triggerContextInsert);
    cogEditorInsertAction->setShortcut(QKeySequence(Qt::Key_Insert));
    QAction* const cogEditorAddSymbolAction = makeEditorCommand(
        QStringLiteral("Add Symbol"), "cog_editor.add_symbol",
        QStringLiteral("Choose a primitive, entity or resource type and insert its declaration in this script's Symbols section."),
        &CogScriptEditorDialog::triggerAddSymbol);
    QAction* const cogEditorAddMessageAction = makeEditorCommand(
        QStringLiteral("Add Message"), "cog_editor.add_message",
        QStringLiteral("Choose a project message and insert its matching declaration and code handler in one local Undo step."),
        &CogScriptEditorDialog::triggerAddMessage);
    QAction* const cogEditorAddCodeLabelAction = makeEditorCommand(
        QStringLiteral("Add Code Label..."), "cog_editor.add_code_label",
        QStringLiteral("Insert a named code label and its matching symbol declaration in one local Undo step."),
        &CogScriptEditorDialog::triggerAddCodeLabel);
    QAction* const cogEditorFlagsAction = makeEditorCommand(
        QStringLiteral("COG Flags..."), "cog_editor.edit_flags",
        QStringLiteral("Edit this COG script's engine-defined flags while preserving unknown and runtime-managed bits."),
        &CogScriptEditorDialog::triggerEditFlags);

    // These actions are the keymap-owned keyboard surface. Register each one
    // with MainWindow, then enable the complete group only while focus belongs
    // to a COG page (including its Find and outline child widgets).
    cogEditorFocusActions_ = {
        cogEditorReloadAction,
        cogEditorCompletionAction,
        cogEditorFindPrevAction,
        cogEditorFormatAction,
        cogEditorWhitespaceAction,
        cogEditorDefinitionAction,
        cogEditorInsertAction,
        cogEditorAddSymbolAction,
        cogEditorAddMessageAction,
        cogEditorAddCodeLabelAction,
        cogEditorFlagsAction,
    };
    for (QAction* action : cogEditorFocusActions_)
    {
        addAction(action);
        action->setEnabled(false);
    }

    // Capture the current viewport eligibility before the first focus event;
    // subsequent updateEditActions() calls replace these base values.
    levelFindBaseEnabled_ = findAction_ && findAction_->isEnabled();
    levelFindNextBaseEnabled_ = findNextAction_ && findNextAction_->isEnabled();
    cogEditorActionsBaseEnabled_ = documentEditingAllowed();
    connect(qApp, &QApplication::focusChanged, this,
        [this](QWidget*, QWidget* now)
        {
            updateCogScriptEditorFocusFromWidget(now);
        });
    QTimer::singleShot(0, this, [this]()
        {
            updateCogScriptEditorFocusFromWidget(
                QApplication::focusWidget());
        });

}

void MainWindow::updateCogScriptEditorFocusFromWidget(QWidget* focusWidget)
{
    if ( recoveryShuttingDown_ )
    {
        return;
    }
    cogEditorFocusActive_ = false;
    for ( const CogScriptEditorTabHost& host : cogScriptEditorTabHosts_ )
    {
        if ( focusWidget && host.dock && host.tabs
            && (focusWidget == host.dock || host.dock->isAncestorOf(focusWidget)) )
        {
            cogEditorFocusActive_ = true;
            lastActiveCogScriptEditorHost_ = host.tabs;
            break;
        }
    }
    syncCogScriptEditorActionFocus();
}

void MainWindow::syncCogScriptEditorActionFocus()
{
    if ( recoveryShuttingDown_ )
    {
        return;
    }
    const bool resourceFocus = resourceEditorOwnsFocus();
    const bool cogFocus = cogScriptEditorOwnsFocus();
    QWidget* const focusWidget = QApplication::focusWidget();
    QWidget* const nativeTextEditor =
        ui::shortcuts::focusedNativeTextEditor(focusWidget);
    const bool nativeTextFocus = nativeTextEditor != nullptr;
    const bool textEntryFocus =
        ui::shortcuts::focusAcceptsTextInput(focusWidget);
    const bool stagedCleaveHistoryFocus = !resourceFocus
        && !cogFocus
        && !textEntryFocus
        && glView_
        && glView_->stagedCleaveHistoryOwnsUndo();
    const bool documentCleaveApplyRedo = cleaveApplyDocumentRedoHasPriority();
    const auto canUndoRedo = [this, resourceFocus, cogFocus,
                              focusWidget, nativeTextEditor, textEntryFocus,
                              stagedCleaveHistoryFocus,
                              documentCleaveApplyRedo](const bool redo)
    {
        if ( resourceFocus )
        {
            if ( nativeTextEditor && nativeTextEditor != focusWidget
                && qobject_cast<QLineEdit*>(nativeTextEditor) )
            {
                return ui::shortcuts::nativeTextEditorCanUndoRedo(
                    nativeTextEditor, redo);
            }
            if ( QLineEdit* const edit = qobject_cast<QLineEdit*>(focusWidget) )
            {
                return redo ? edit->isRedoAvailable() : edit->isUndoAvailable();
            }
            if ( QPlainTextEdit* const edit = qobject_cast<QPlainTextEdit*>(focusWidget) )
            {
                return redo ? edit->document()->isRedoAvailable() : edit->document()->isUndoAvailable();
            }
            QTabWidget* const tabs = activeResourceEditorTabHost();
            const bool resourceCanUndoRedo =
                ui::resources::resourceAuthoringPaneCanUndoRedo(
                    tabs ? tabs->currentWidget() : nullptr,
                    redo);
            return resourceCanUndoRedo
                || (nativeTextEditor
                    && ui::shortcuts::nativeTextEditorCanUndoRedo(
                        nativeTextEditor,
                        redo));
        }
        if ( cogFocus )
        {
            if ( nativeTextEditor && nativeTextEditor != focusWidget
                && qobject_cast<QLineEdit*>(nativeTextEditor) )
            {
                return ui::shortcuts::nativeTextEditorCanUndoRedo(
                    nativeTextEditor, redo);
            }
            return activeCogScriptEditorCanUndoRedo(redo);
        }
        if ( nativeTextEditor )
        {
            return ui::shortcuts::nativeTextEditorCanUndoRedo(
                nativeTextEditor, redo);
        }
        if ( textEntryFocus )
        {
            return false;
        }
        if ( stagedCleaveHistoryFocus && !(redo && documentCleaveApplyRedo) )
        {
            return glView_ && (redo
                ? glView_->canRedoStagedCleaveHistory()
                : glView_->canUndoStagedCleaveHistory());
        }
        return redo ? document_.canRedo() : document_.canUndo();
    };
    if ( undoAction_ )
    {
        undoAction_->setEnabled(documentEditingAllowed() && canUndoRedo(false));
        if ( resourceFocus || cogFocus || nativeTextFocus || textEntryFocus
            || stagedCleaveHistoryFocus )
        {
            undoAction_->setText(QStringLiteral("&Undo"));
        }
        else
        {
            const std::string_view label = document_.nextUndoLabel();
            undoAction_->setText(label.empty() ? QStringLiteral("&Undo")
                : QStringLiteral("&Undo %1").arg(QString::fromStdString(std::string(label))));
        }
    }
    if ( redoAction_ )
    {
        redoAction_->setEnabled(documentEditingAllowed() && canUndoRedo(true));
        if ( resourceFocus || cogFocus || nativeTextFocus || textEntryFocus
            || (stagedCleaveHistoryFocus && !documentCleaveApplyRedo) )
        {
            redoAction_->setText(QStringLiteral("&Redo"));
        }
        else
        {
            const std::string_view label = document_.nextRedoLabel();
            redoAction_->setText(label.empty() ? QStringLiteral("&Redo")
                : QStringLiteral("&Redo %1").arg(QString::fromStdString(std::string(label))));
        }
    }
    const bool cogActionsEnabled =
        cogEditorFocusActive_ && cogEditorActionsBaseEnabled_;
    CogScriptEditorDialog* focusedSource = nullptr;
    for ( QWidget* current = focusWidget; current; current = current->parentWidget() )
    {
        if ( auto* editor = dynamic_cast<CogScriptEditorDialog*>(current) )
        {
            if ( cogScriptEditorTabsForPage(editor) )
            {
                focusedSource = editor;
            }
            break;
        }
    }
    for (QAction* action : cogEditorFocusActions_)
    {
        if (action)
        {
            action->setEnabled(cogActionsEnabled
                && (action->objectName() != QStringLiteral("cog_editor.insert_context")
                    || (focusedSource && focusedSource->contextInsertAvailable())));
        }
    }
    if (findAction_)
    {
        findAction_->setEnabled(
            cogEditorFocusActive_
                ? cogEditorActionsBaseEnabled_
                : levelFindBaseEnabled_);
    }
    if (findNextAction_)
    {
        findNextAction_->setEnabled(
            cogEditorFocusActive_
                ? cogEditorActionsBaseEnabled_
                : levelFindNextBaseEnabled_);
    }
}

void MainWindow::setupCogScriptEditorDock(QDockWidget* dock)
{
    if (!workspaceWindow_ || !dock)
    {
        return;
    }

    // Step 1: create the detachable COG editor inside the nested workspace.
    // The outer window still places the outliner immediately to its left, but
    // the two docks no longer share a splitter or consume each other's width.
    auto* preferredDock = new ui::PreferredWidthDockWidget("COG Scripts", workspaceWindow_);
    // Seed the maximize hint while this primary host is hidden. The title-icon
    // transition filter reapplies it after Qt rebuilds the floating flags.
    preferredDock->setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    cogScriptEditorDock_ = preferredDock;
    cogScriptEditorDock_->setObjectName("cogScriptEditorDock");
    cogScriptEditorDock_->setWindowIcon(ui::makeViewportIcon(ui::ViewportIcon::outlinerCogs));
    cogScriptEditorDock_->setStyleSheet(QStringLiteral(
        "QDockWidget#cogScriptEditorDock::title { padding-left: 28px; }"));
    cogScriptEditorDock_->setAllowedAreas(Qt::AllDockWidgetAreas);
    cogScriptEditorDock_->setFeatures(
        QDockWidget::DockWidgetMovable
        | QDockWidget::DockWidgetFloatable
        | QDockWidget::DockWidgetClosable);
    cogScriptEditorDock_->setMinimumWidth(kMinimumCogScriptDockWidth);
    cogScriptDockPreferredWidth_ = defaultCogScriptDockWidth();
    preferredDock->setPreferredWidth(cogScriptDockPreferredWidth_);
    auto* cogScriptDockRoot = new QWidget(cogScriptEditorDock_);
    auto* cogScriptDockLayout = new QVBoxLayout(cogScriptDockRoot);
    cogScriptDockLayout->setContentsMargins(4, 4, 4, 4);
    cogScriptDockLayout->setSpacing(4);
    cogScriptEditorTabs_ = new QTabWidget(cogScriptDockRoot);
    cogScriptEditorTabs_->setObjectName("cogScriptEditorTabs");
    cogScriptEditorTabs_->setDocumentMode(true);
    cogScriptEditorTabs_->setTabsClosable(true);
    cogScriptEditorTabs_->setMovable(true);
    cogScriptEditorTabs_->setProperty("cogHoverIndex", -1);
    cogScriptDockLayout->addWidget(cogScriptEditorTabs_, 1);
    cogScriptEditorDock_->setWidget(cogScriptDockRoot);
    new CogDockTitleIcon(cogScriptEditorDock_);
    workspaceWindow_->addDockWidget(Qt::LeftDockWidgetArea, cogScriptEditorDock_);
    registerCogScriptEditorTabHost(cogScriptEditorDock_, cogScriptEditorTabs_, QStringLiteral("primary"), true);
    configureCogScriptEditorTabHost(cogScriptEditorDock_, cogScriptEditorTabs_);
    adjustingCogDockWidths_ = true;
    workspaceWindow_->resizeDocks(
        { cogScriptEditorDock_ },
        { cogScriptDockPreferredWidth_ },
        Qt::Horizontal);
    adjustingCogDockWidths_ = false;
    cogScriptEditorDock_->hide();
    restoreStandaloneOutlinerDockWidth();
    dock->installEventFilter(this);
    cogScriptEditorDock_->installEventFilter(this);

    // Step 2: remember and restore each dock independently. Splitter drags in
    // either QMainWindow update only that window's preferred dock width.
    connect(cogScriptEditorDock_, &QDockWidget::visibilityChanged, this, [this](const bool visible)
        {
            if (!cogScriptEditorDock_)
            {
                return;
            }

            if (visible)
            {
                if (!cogScriptDockWidthLearned_)
                {
                    cogScriptDockPreferredWidth_ = defaultCogScriptDockWidth();
                }
                restoreCogScriptDockWidth();
            }
            else if (cogScriptEditorDock_->width() >= kMinimumCogScriptDockWidth)
            {
                cogScriptDockPreferredWidth_ = cogScriptEditorDock_->width();
                cogScriptDockWidthLearned_ = true;
            }
        });
    connect(cogScriptEditorDock_, &QDockWidget::topLevelChanged, this, [this](const bool floating)
        {
            if (floating)
            {
                if (cogScriptEditorDock_
                    && cogScriptEditorDock_->width() >= kMinimumCogScriptDockWidth)
                {
                    cogScriptDockPreferredWidth_ = cogScriptEditorDock_->width();
                    cogScriptDockWidthLearned_ = true;
                }
            }
            else
            {
                QTimer::singleShot(0, this, [this]() { restoreCogScriptDockWidth(); });
            }
        });
    connect(cogScriptEditorDock_, &QDockWidget::dockLocationChanged, this, [this](Qt::DockWidgetArea)
        {
            QTimer::singleShot(0, this, [this]() { restoreCogScriptDockWidth(); });
        });
    connect(dock, &QDockWidget::topLevelChanged, this, [this](const bool)
        {
            QTimer::singleShot(0, this, [this]() { restoreStandaloneOutlinerDockWidth(); });
        });
    connect(dock, &QDockWidget::dockLocationChanged, this, [this](Qt::DockWidgetArea)
        {
            QTimer::singleShot(0, this, [this]() { restoreStandaloneOutlinerDockWidth(); });
        });

}

} // namespace sed
