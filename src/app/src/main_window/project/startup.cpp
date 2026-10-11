#include "app_icon.hpp"
#include "app_logging.hpp"
#include "main_window/common.hpp"
#include "ui/components/path_display_utils.hpp"
#include "ui/components/preview_resource_revision_utils.hpp"
#include "ui/project/direct_sed_open_utils.hpp"
#include "ui/project/project_startup_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"
#include "ui/recovery/partial_level_salvage.hpp"
#include "ui/resources/project_resource_settings.hpp"

#include "sed/assets/sed.hpp"
#include "sed/assets/material_dimensions.hpp"
#include "sed/editor/uv_promotion.hpp"

#include <QFutureWatcher>
#include <QCursor>
#include <QGuiApplication>
#include <QScopeGuard>
#include <QScreen>
#include <QWindow>

#include <QtConcurrent/QtConcurrentRun>

namespace sed
{

#include "main_window/detail/helpers.hpp"

namespace
{

void showExplicitlyLoggedMessageBox(
    QWidget* parent,
    const QMessageBox::Icon icon,
    const QString& title,
    const QString& text)
{
    QMessageBox message(icon, title, text, QMessageBox::Ok, parent);
    message.setProperty(kMessageBoxLoggingSkipProperty, true);
    (void)message.exec();
}

QString describeLegacyUvPromotionFailure(
    const editor::DocumentUvConversionResult& result,
    const QString& nativeVersionLabel)
{
    QString location{};
    if (result.failedHistoryStack)
    {
        location = QStringLiteral(" in %1 history entry %2")
            .arg(*result.failedHistoryStack
                    == editor::DocumentRecoveryHistoryStack::Undo
                ? QStringLiteral("Undo") : QStringLiteral("Redo"))
            .arg(static_cast<qulonglong>(result.failedHistoryIndex + 1));
    }
    if (result.failedSurface)
    {
        location += QStringLiteral(" at Sector %1, Surface %2")
            .arg(result.failedSurface->sector)
            .arg(result.failedSurface->surface);
    }
    return QStringLiteral(
        "Could not normalize legacy SED %1 UVs%2: %3 The complete Level and "
        "its undo/redo history remain in legacy pixel space.")
        .arg(nativeVersionLabel,
            location,
            QString::fromStdString(result.error));
}

QString nativeLegacyUvVersionLabel(const Level& level)
{
    return level.motsArchLighting.empty()
        ? QStringLiteral("V1")
        : QStringLiteral("V4");
}

/**
 * Provides native window dragging for the startup dialog's client-drawn title
 * area. Caption controls remain ordinary child widgets inside that area.
 */
class StartupTitleBar final : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            QWindow* nativeWindow = window() ? window()->windowHandle() : nullptr;
            if (nativeWindow && nativeWindow->startSystemMove())
            {
                event->accept();
                return;
            }
        }

        QWidget::mousePressEvent(event);
    }
};

ui::project::ProjectInstallStatus currentProjectInstallStatus(const ProjectType kind)
{
    return ui::project::ProjectInstallStatus(
        kind,
        readProjectInstallDirSetting(kind),
        tryGetProjectInstallDir(kind));
}

QString shortProjectTypeLabel(const ProjectType kind)
{
    switch (kind)
    {
    case ProjectType::JKDF2:
        return QStringLiteral("JKDF2");
    case ProjectType::MOTS:
        return QStringLiteral("MOTS");
    case ProjectType::IJIM:
        return QStringLiteral("IJIM");
    default:
        return QStringLiteral("SED");
    }
}

std::optional<ProjectType> inferRecentLevelProjectType(const QString& path)
{
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("ndy"))
    {
        return ProjectType::IJIM;
    }
    if (suffix != QStringLiteral("jkl") && suffix != QStringLiteral("sed"))
    {
        return std::nullopt;
    }

    const std::filesystem::path levelPath(path.toStdWString());
    std::error_code ec;
    if (!std::filesystem::exists(levelPath, ec) || ec)
    {
        return std::nullopt;
    }

    try
    {
        if (suffix == QStringLiteral("sed"))
        {
            return ::sed::assets::readSedProjectType(levelPath);
        }
        ::sed::assets::JklLoadOptions options{};
        return ::sed::assets::loadFromJKL(levelPath, options).kind;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

QPixmap makeRecentFileIcon(const QString& path, const std::optional<ProjectType> projectKind)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    const bool legacyEditorFormat = suffix == QStringLiteral("jed");
    const QString label = suffix.isEmpty() ? QStringLiteral("LVL") : suffix.left(3).toUpper();

    QColor projectColor("#a98338");

    if (legacyEditorFormat)
    {
        projectColor = QColor("#8b7fb4");
    }
    else if (projectKind == ProjectType::IJIM)
    {
        projectColor = QColor("#b37a35");
    }
    else if (projectKind == ProjectType::MOTS)
    {
        projectColor = QColor("#7667c9");
    }
    else if (projectKind == ProjectType::JKDF2)
    {
        projectColor = QColor("#c3484b");
    }

    const QColor kPageFill("#27342e");
    const QColor kCornerFill("#34453d");
    const QColor kGridColor("#8da09b");

    QPixmap pixmap(46, 46);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF pageRect(7.0, 3.0, 31.0, 39.0);
    painter.setPen(QPen(projectColor, 1.8));
    painter.setBrush(kPageFill);
    painter.drawRoundedRect(pageRect, 4.0, 4.0);

    const QPolygonF foldedCorner{
        QPointF(29.0, 3.0),
        QPointF(38.0, 12.0),
        QPointF(29.0, 12.0),
    };
    painter.setBrush(kCornerFill);
    painter.drawPolygon(foldedCorner);

    if (legacyEditorFormat)
    {
        painter.setPen(QPen(kGridColor, 1.7));
        painter.drawLine(QPointF(12.0, 24.0), QPointF(27.0, 10.5));
        painter.setPen(QPen(QColor("#b6a6d8"), 1.0));
        painter.drawLine(QPointF(12.0, 13.0), QPointF(25.0, 13.0));
        painter.drawLine(QPointF(12.0, 18.0), QPointF(22.0, 18.0));
    }
    else
    {
        painter.setPen(QPen(kGridColor, 1.25));
        painter.drawLine(QPointF(12.0, 12.0), QPointF(32.0, 12.0));
        painter.drawLine(QPointF(12.0, 17.5), QPointF(32.0, 17.5));
        painter.drawLine(QPointF(12.0, 23.0), QPointF(32.0, 23.0));
        painter.drawLine(QPointF(17.0, 8.0), QPointF(17.0, 28.0));
        painter.drawLine(QPointF(22.5, 8.0), QPointF(22.5, 28.0));
        painter.drawLine(QPointF(28.0, 8.0), QPointF(28.0, 28.0));
    }

    painter.setPen(QPen(projectColor, 1.6));
    painter.drawLine(QPointF(9.5, 29.0), QPointF(36.0, 29.0));

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(10);
    painter.setFont(font);
    painter.setPen(QColor("#f6f6f6"));
    painter.drawText(QRectF(8.0, 28.0, 29.0, 12.0), Qt::AlignCenter, label);

    return pixmap;
}

QString recentFileFormatTooltip(const QString& path, const std::optional<ProjectType> projectKind)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    QString format;
    if (suffix == QStringLiteral("jed"))
    {
        format = QStringLiteral("Legacy JED editor file");
    }
    else if (suffix == QStringLiteral("ndy"))
    {
        format = QStringLiteral("NDY level file");
    }
    else if (suffix == QStringLiteral("jkl"))
    {
        format = QStringLiteral("JKL level file");
    }
    else
    {
        format = QStringLiteral("Level file");
    }

    if (projectKind)
    {
        return QStringLiteral("%1 - %2").arg(format, shortProjectTypeLabel(*projectKind));
    }
    return format;
}

QIcon makeStartupGearIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(12.0, 12.0);

    painter.setPen(QPen(QColor("#cfd6dd"), 1.6, Qt::SolidLine, Qt::RoundCap));
    for (int i = 0; i < 8; ++i)
    {
        painter.save();
        painter.rotate(i * 45.0);
        painter.drawLine(QPointF(0.0, -8.5), QPointF(0.0, -10.0));
        painter.restore();
    }

    painter.setPen(QPen(QColor("#cfd6dd"), 1.8));
    painter.setBrush(QColor("#3a4654"));
    painter.drawEllipse(QPointF(0.0, 0.0), 6.6, 6.6);
    painter.setBrush(QColor("#252525"));
    painter.drawEllipse(QPointF(0.0, 0.0), 2.4, 2.4);

    return QIcon(pixmap);
}

QIcon makeStartupPinIcon(const bool pinned)
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(12.0, 12.0);
    painter.rotate(-38.0);

    QPainterPath pin;
    pin.moveTo(-3.0, -8.0);
    pin.lineTo(3.0, -8.0);
    pin.lineTo(2.2, -3.0);
    pin.lineTo(5.6, 0.4);
    pin.lineTo(1.25, 0.4);
    pin.lineTo(1.25, 7.2);
    pin.lineTo(-1.25, 7.2);
    pin.lineTo(-1.25, 0.4);
    pin.lineTo(-5.6, 0.4);
    pin.lineTo(-2.2, -3.0);
    pin.closeSubpath();

    const QColor color = pinned ? QColor("#f6f6f6") : QColor("#a9b2bd");
    painter.setPen(QPen(color, pinned ? 1.0 : 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(pinned ? color : Qt::NoBrush);
    painter.drawPath(pin);

    return QIcon(pixmap);
}

class RecentPinVisibilityFilter final : public QObject
{
public:
    RecentPinVisibilityFilter(QWidget* row, QToolButton* button, const bool pinned)
        : QObject(row)
        , row_(row)
        , button_(button)
        , pinned_(pinned)
    {
        syncVisibility(false);
    }

    void watch(QWidget* widget)
    {
        if (!widget)
        {
            return;
        }

        widget->setMouseTracking(true);
        widget->setAttribute(Qt::WA_Hover, true);
        widget->installEventFilter(this);
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        Q_UNUSED(watched);

        switch (event->type())
        {
        case QEvent::Enter:
        case QEvent::HoverEnter:
        case QEvent::MouseMove:
            syncVisibility(true);
            break;
        case QEvent::Leave:
        case QEvent::HoverLeave:
            QTimer::singleShot(0, this, [this]()
                {
                    syncVisibility(rowContainsCursor());
                });
            break;
        default:
            break;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    void syncVisibility(const bool hovered)
    {
        if (button_)
        {
            button_->setVisible(pinned_ || hovered);
        }
    }

    bool rowContainsCursor() const
    {
        if (!row_)
        {
            return false;
        }

        const QPoint cursor = QCursor::pos();
        const QRect globalRowRect(row_->mapToGlobal(QPoint(0, 0)), row_->size());
        return globalRowRect.contains(cursor);
    }

    QPointer<QWidget> row_;
    QPointer<QToolButton> button_;
    bool pinned_{};
};

double calcThingInsertOffsetZ(
    const ::sed::Level& level,
    const assets::ResourceLocator& resources,
    const assets::Templates* templatesDb,
    const std::string_view thingName)
{
    static std::unordered_map<std::string, double> modelInsertZCache{};
    static std::unordered_map<std::string, double> spriteOffsetZCache{};

    Thing temp{};
    temp.name = std::string(thingName);
    temp.templateName = std::string(thingName);

    auto valueString = [](const TplValue* v) -> std::string
    {
        if (!v)
        {
            return {};
        }
        return v->stringValue.empty() ? v->asString() : v->stringValue;
    };

    auto valueDouble = [](const TplValue* v) -> std::optional<double>
    {
        if (!v)
        {
            return std::nullopt;
        }
        if (v->type == ValueType::floating)
        {
            return v->floatValue;
        }
        if (v->type == ValueType::integer)
        {
            return static_cast<double>(v->intValue);
        }
        const std::string s = v->stringValue.empty() ? v->asString() : v->stringValue;
        try
        {
            std::size_t end = 0;
            const double d = std::stod(s, &end);
            return (end > 0) ? std::optional<double>(d) : std::nullopt;
        }
        catch (...)
        {
            return std::nullopt;
        }
    };

    const std::string move = ::sed::text::lowerAscii(::sed::text::trimView(valueString(
        ::sed::level::thingFindValue(level, templatesDb, temp, "move"))));
    if (move == "physics")
    {
        if (const auto h = valueDouble(::sed::level::thingFindValue(level, templatesDb, temp, "height")))
        {
            if (std::isfinite(*h) && std::abs(*h) > 1e-9)
            {
                return *h;
            }
        }
    }

    const TplValue* modelVal = ::sed::level::thingFindValue(level, templatesDb, temp, "model3d");
    if (!modelVal)
    {
        modelVal = ::sed::level::thingFindValue(level, templatesDb, temp, "model3do");
    }

    const std::string modelName = ::sed::text::trim(valueString(modelVal));
    if (!modelName.empty())
    {
        std::string key = modelName;
        if (!::sed::text::iendsWithAscii(key, ".3do"))
        {
            key += ".3do";
        }

        const std::string revisionKey = key + "|"
            + ui::detail::previewResourceRevisionToken(&resources, key, ".3do");
        if (const auto it = modelInsertZCache.find(revisionKey); it != modelInsertZCache.end())
        {
            return it->second;
        }

        double z = 0.0;
        try
        {
            if (const auto text = resources.tryReadTextWithExt(key, ".3do"))
            {
                std::istringstream iss(*text);
                const assets::Model3do model = assets::loadFrom3DO(iss);
                z = model.insertOffset.z;
            }
        }
        catch (...)
        {
            z = 0.0;
        }

        if (modelInsertZCache.size() >= 256) modelInsertZCache.clear();
        modelInsertZCache.emplace(revisionKey, z);
        return z;
    }

    const std::string spriteName = ::sed::text::trim(valueString(
        ::sed::level::thingFindValue(level, templatesDb, temp, "sprite")));
    if (!spriteName.empty())
    {
        std::string key = spriteName;
        if (!::sed::text::iendsWithAscii(key, ".spr"))
        {
            key += ".spr";
        }

        const std::string revisionKey = key + "|"
            + ui::detail::previewResourceRevisionToken(&resources, key, ".spr");
        if (const auto it = spriteOffsetZCache.find(revisionKey); it != spriteOffsetZCache.end())
        {
            return it->second;
        }

        double z = 0.0;
        try
        {
            if (const auto text = resources.tryReadTextWithExt(key, ".spr"))
            {
                std::istringstream iss(*text);
                const assets::SprFile spr = assets::loadFromSPR(iss);
                z = static_cast<double>(spr.offsetXYZ[2]);
            }
        }
        catch (...)
        {
            z = 0.0;
        }

        if (spriteOffsetZCache.size() >= 256) spriteOffsetZCache.clear();
        spriteOffsetZCache.emplace(revisionKey, z);
        return z;
    }

    return 0.0;
}

std::optional<assets::LevLoadOptions> promptLevImportOptions(QWidget* parent)
{
    assets::LevLoadOptions chosen = ui::project::defaultLevImportOptions();

    QDialog dialog(parent);
    dialog.setObjectName(QStringLiteral("levImportOptionsDialog"));
    dialog.setWindowTitle(QStringLiteral("Import Dark Forces LEV"));
    dialog.setWindowFlags(
        Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    dialog.setModal(true);
    dialog.setMinimumWidth(460);

    auto* root = new QVBoxLayout(&dialog);
    auto* description = new QLabel(
        QStringLiteral(
            "Choose how Dark Forces coordinates and texture names are converted into this SED level."),
        &dialog);
    description->setWordWrap(true);
    root->addWidget(description);

    auto* form = new QFormLayout();
    auto* scale = new QDoubleSpinBox(&dialog);
    scale->setObjectName(QStringLiteral("levImportScale"));
    scale->setDecimals(6);
    scale->setRange(0.000001, 1000000000.0);
    scale->setSingleStep(1.0);
    scale->setValue(chosen.dfToJkFactor);
    scale->setKeyboardTracking(false);
    scale->setToolTip(QStringLiteral(
        "Dark Forces coordinates are divided by this value. The original SED default is 40."));
    form->addRow(QStringLiteral("Coordinate scale"), scale);

    auto* textures = new QComboBox(&dialog);
    textures->setObjectName(QStringLiteral("levImportTextureMode"));
    textures->addItem(
        QStringLiteral("Keep texture names"),
        static_cast<int>(assets::LevTextureMode::Keep));
    textures->addItem(
        QStringLiteral("Set all textures to DFLT.MAT"),
        static_cast<int>(assets::LevTextureMode::Default));
    textures->setToolTip(QStringLiteral(
        "Keep converts Dark Forces texture names to MAT names; DFLT.MAT assigns one placeholder material."));
    form->addRow(QStringLiteral("Textures"), textures);
    root->addLayout(form);

    auto* loadObjects = new QCheckBox(
        QStringLiteral("Import objects from a matching .O file"), &dialog);
    loadObjects->setObjectName(QStringLiteral("levImportObjects"));
    loadObjects->setChecked(chosen.loadObjects);
    root->addWidget(loadObjects);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->setObjectName(QStringLiteral("levImportButtons"));
    if (QPushButton* importButton = buttons->button(QDialogButtonBox::Ok))
    {
        importButton->setText(QStringLiteral("Import"));
        importButton->setDefault(true);
    }
    QObject::connect(
        buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(
        buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }

    chosen.dfToJkFactor = scale->value();
    chosen.textureMode = static_cast<assets::LevTextureMode>(
        textures->currentData().toInt());
    chosen.loadObjects = loadObjects->isChecked();
    if (!ui::project::isValidLevImportScale(chosen.dfToJkFactor))
    {
        return std::nullopt;
    }

    return chosen;
}

} // namespace

bool MainWindow::runInitialStartupFlow()
{
    const bool recovered = offerStartupRecovery();
    if (windowPlacementRestoreDeferred_)
    {
        // A valid recovery EDIT section already restores this window's docks
        // and nested workspace. Applying the previous app layout as well can
        // leave Qt's hidden QMainWindow layout with stale dock references;
        // the first show then faults while activating that layout. For a
        // recovery without accepted EDIT state, or the project chooser path,
        // retain the normal persisted placement behavior.
        if (!recovered || !startupRecoveryEditorStateRestored_)
        {
            restoreWindowPlacement();
        }
        windowPlacementRestoreDeferred_ = false;
    }
    if (recovered)
    {
        return true;
    }
    return showProjectStartDialog();
}

void MainWindow::openLevelFromFilename(
    const QString& filename,
    std::function<void()> beforeReplacement)
{
    if (filename.isEmpty())
    {
        return;
    }

    const QFileInfo requestedInfo(filename);
    if (!requestedInfo.exists() || !requestedInfo.isFile())
    {
        forgetRecentLevelFile(filename);
        const QString message = QStringLiteral("The file does not exist:\n%1")
            .arg(QDir::toNativeSeparators(requestedInfo.absoluteFilePath()));
        logMessage(MessageSeverity::error, message);
        showExplicitlyLoggedMessageBox(this, QMessageBox::Warning,
            QStringLiteral("File not found"), message);
        return;
    }

    const std::filesystem::path requestedPath(filename.toStdWString());
    if (!ui::project::isSupportedLevelOpenPath(requestedPath))
    {
        const QString message = QStringLiteral("SED cannot open this file type:\n%1")
            .arg(QDir::toNativeSeparators(requestedInfo.absoluteFilePath()));
        logMessage(MessageSeverity::error, message);
        showExplicitlyLoggedMessageBox(this, QMessageBox::Warning,
            QStringLiteral("Unsupported file"), message);
        return;
    }

    std::filesystem::path selectedRecoveryPath{};
    QString selectedRecoveryIdentity{};
    QString selectedRecoverySessionId{};
    const RecoveryOfferResult recoveryOffer = offerRecoveryForPath(
        requestedPath,
        false,
        &selectedRecoveryPath,
        &selectedRecoveryIdentity,
        &selectedRecoverySessionId);
    if (recoveryOffer == RecoveryOfferResult::cancelled)
    {
        return;
    }
    const std::string requestedExtension =
        sed::text::lowerAscii(requestedPath.extension().string());
    std::optional<assets::LevLoadOptions> levImportOptions{};
    if (recoveryOffer != RecoveryOfferResult::recovered
        && requestedExtension == ".lev")
    {
        levImportOptions = promptLevImportOptions(this);
        if (!levImportOptions)
        {
            return;
        }
    }
    const QString recoveryIdentityBeforeReplacementPrompt = recoveryIdentity_;
    const QString recoverySessionBeforeReplacementPrompt = recoverySessionId_;
    const std::filesystem::path recoveryPathBeforeReplacementPrompt =
        currentRecoveryPath_;

    // Freeze this session's live publisher across the two-phase replacement.
    // In particular, Save from the current-workspace prompt must not replace
    // a same-identity recovery path selected in the preceding dialog.
    const bool wasRecoveryPublicationPaused = recoveryPublicationPaused_;
    recoveryPublicationPaused_ = true;
    if (recoveryDebounceTimer_) recoveryDebounceTimer_->stop();
    if (recoveryRetryTimer_) recoveryRetryTimer_->stop();
    if (recoveryPublicationGate_)
    {
        recoveryPublicationGate_->generation.fetch_add(
            1, std::memory_order_acq_rel);
    }
    const auto recoveryPublicationPause = qScopeGuard(
        [this, wasRecoveryPublicationPaused]()
        {
            recoveryPublicationPaused_ = wasRecoveryPublicationPaused;
            if (!recoveryPublicationPaused_ && !recoveryShuttingDown_)
            {
                configureRecoveryTimerFromSettings();
                if (recoverySnapshotRequired_)
                {
                    QTimer::singleShot(0, this,
                        [this]() { scheduleRecoverySnapshot(); });
                }
            }
        });

    // The recovery choice is deliberately inspect-only until the current
    // workspace authorizes replacement.  Defer an explicit Discard of the
    // current workspace until the selected recovery or saved file has fully
    // committed; cancellation and decode/load failure must leave both the
    // current document and its last durable checkpoint intact.
    QString deferredCurrentDiscardIdentity{};
    QString deferredCurrentDiscardSessionId{};
    const std::filesystem::path preservedRecoveryPath =
        recoveryOffer == RecoveryOfferResult::recovered
            ? selectedRecoveryPath
            : std::filesystem::path{};
    if (!authorizeDocumentReplacement(
            QStringLiteral("opening another level"),
            preservedRecoveryPath,
            &deferredCurrentDiscardIdentity,
            &deferredCurrentDiscardSessionId))
    {
        return;
    }
    // Startup owns a modal chooser until both the matching-recovery decision
    // and the current-workspace Save/Discard/Cancel transaction have accepted
    // replacement. Reveal the editor only at this boundary: missing,
    // unsupported, cancelled, or rejected opens leave the chooser intact, and
    // the callback is never invoked after candidate parsing has begun.
    if (beforeReplacement)
    {
        beforeReplacement();
        beforeReplacement = {};
    }
    const auto replacementFingerprintGuard = qScopeGuard(
        [this]() { clearReplacementAuthorizationFingerprint(); });
    // Save/Save As from the replacement prompt can establish a different
    // clean document identity. Remember that exact session as well so a
    // successful Open retires its live automatic offer without touching its
    // bounded periodic history.
    const QString recoveryIdentityAfterReplacementPrompt = recoveryIdentity_;
    const QString recoverySessionAfterReplacementPrompt = recoverySessionId_;
    const std::filesystem::path recoveryPathAfterReplacementPrompt =
        currentRecoveryPath_;
    if (recoveryOffer == RecoveryOfferResult::recovered)
    {
        if (applyRecoveryFile(selectedRecoveryPath))
        {
            if (!deferredCurrentDiscardIdentity.isEmpty())
            {
                // The replacement is now authoritative. Preserve its
                // canonical live slot when the old and recovered documents
                // share identity.
                discardRecoveryFamily(
                    deferredCurrentDiscardIdentity,
                    deferredCurrentDiscardSessionId,
                    currentRecoveryPath_);
            }
            else
            {
                // A clean or EDIT-only old workspace had no explicit Discard
                // prompt, but its live automatic offer is still superseded by
                // the successfully installed recovery. Keep history and the
                // recovered session's canonical live slot.
                discardRecoveryLiveFamily(
                    recoveryIdentityBeforeReplacementPrompt,
                    recoverySessionBeforeReplacementPrompt,
                    currentRecoveryPath_);
                discardRecoveryLiveFamily(
                    recoveryIdentityAfterReplacementPrompt,
                    recoverySessionAfterReplacementPrompt,
                    currentRecoveryPath_);
            }

            // applyRecoveryFile() returns before the ordinary-Open success
            // retirement below. Close the same publication race here: a
            // superseded worker can have passed its last generation check
            // before the family scan deleted its slot. Gate-invalidating each
            // exact pre-recovery path after that scan prevents it from
            // resurrecting a stale automatic offer. The recovered document's
            // canonical live path is preserved and periodic history is not in
            // this exact-path set.
            std::vector<std::filesystem::path> supersededLivePaths{
                recoveryPathBeforeReplacementPrompt,
                recoveryPathAfterReplacementPrompt,
            };
            std::sort(supersededLivePaths.begin(), supersededLivePaths.end());
            supersededLivePaths.erase(std::unique(
                supersededLivePaths.begin(), supersededLivePaths.end()),
                supersededLivePaths.end());
            for (const std::filesystem::path& supersededPath
                 : supersededLivePaths)
            {
                if (supersededPath.empty()
                    || ui::recovery::recoveryPathsEqual(
                        supersededPath, currentRecoveryPath_))
                {
                    continue;
                }
                QString retirementError{};
                if (!ui::recovery::invalidateAndRemoveRecovery(
                        recoveryPublicationGate_, supersededPath,
                        &retirementError)
                    && !retirementError.isEmpty())
                {
                    logMessage(MessageSeverity::warning, retirementError);
                }
            }
        }
        return;
    }

    // Keep the current session's exact live slot until ordinary Open reaches
    // its success boundary. switchRecoveryDocument advances the generation so
    // an old worker cannot publish, but retirement itself is deferred below.
    try
    {
        const std::filesystem::path levelPath = requestedPath;
        const QString levelLabel = QFileInfo(filename).fileName().isEmpty() ? filename : QFileInfo(filename).fileName();

        const std::string ext = requestedExtension;

        ProjectType kindGuess = document_.level().kind;
        if (ext == ".ndy")
        {
            kindGuess = ProjectType::IJIM;
        }
        else if (ext == ".jkl")
        {
            kindGuess = ProjectType::JKDF2;
        }
        else if (ext == ".sed")
        {
            try
            {
                kindGuess = ::sed::assets::readSedProjectType(levelPath);
            }
            catch (const std::exception& exception)
            {
                // Resource discovery may begin with the current project as a
                // provisional guess. The full strict/recovery load below is
                // authoritative and refreshes resources from the recovered
                // Level kind, so a damaged primary header is not an Open
                // precondition.
                logMessage(MessageSeverity::warning, QStringLiteral(
                    "Could not pre-read the SED project type (%1); the full checked document load will determine it.")
                    .arg(QString::fromUtf8(exception.what())));
            }
        }

        QProgressDialog progressDialog(this);
        progressDialog.setWindowTitle("Loading Level");
        progressDialog.setLabelText(QString("Preparing %1...").arg(levelLabel));
        progressDialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        progressDialog.setCancelButton(nullptr);
        progressDialog.setRange(0, 100);
        progressDialog.setMinimumDuration(0);
        progressDialog.setAutoClose(false);
        progressDialog.setAutoReset(false);
        progressDialog.setWindowModality(Qt::WindowModal);
        progressDialog.setMinimumWidth(560);
        progressDialog.setFixedWidth(560);
        progressDialog.setStyleSheet(
            "QProgressDialog {"
            " background-color: #25282d;"
            " border: 1px solid #1c1f24;"
            " border-radius: 14px;"
            "}"
            "QProgressDialog QLabel {"
            " color: #eef2f7;"
            " font-size: 14px;"
            " min-height: 28px;"
            "}"
            "QProgressBar {"
            " background-color: #1a2028;"
            " border: 1px solid #1f2c3a;"
            " border-radius: 11px;"
            " min-height: 18px;"
            " text-align: center;"
            " padding: 1px;"
            "}"
            "QProgressBar::chunk {"
            " background-color: #2f6db1;"
            " border-radius: 9px;"
            " margin: 0px;"
            "}");
        auto* progressBar = new QProgressBar(&progressDialog);
        progressBar->setTextVisible(false);
        progressBar->setMinimumHeight(20);
        progressDialog.setBar(progressBar);
        progressDialog.setValue(0);
        progressDialog.show();

        auto updateLoadProgress = [&](const QString& label, const int value)
        {
            progressDialog.setLabelText(label);
            progressDialog.setValue(std::clamp(value, 0, 100));
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        };

        std::optional<StagedProjectResourceState> stagedResources{};

        const bool deferSedResourceSetup = ext == ".sed";
        const bool deferJklResourceResolution = ext == ".jkl";
        if (deferSedResourceSetup)
        {
            // Native SED parsing is resource-independent. Do not mutate the
            // current document's project settings, locator, or snap caches
            // until a partial recovery has been explicitly accepted.
            updateLoadProgress(QString("Validating %1 before resource setup...")
                .arg(levelLabel), 12);
        }
        else if (deferJklResourceResolution)
        {
            // JKL VERSION 1 does not identify JKDF2 versus MotS until its
            // geometry has been decoded. Defer resource staging so the first
            // pass cannot consult the extension's provisional project kind.
            updateLoadProgress(QString("Parsing %1 before resource setup...")
                .arg(levelLabel), 12);
        }
        else
        {
            updateLoadProgress(QString("Preparing resources for %1...").arg(levelLabel), 5);
            stagedResources.emplace(stageProjectResources(kindGuess, levelPath));
            updateLoadProgress(QString("Scanning resources for %1...").arg(levelLabel), 12);
        }

        struct PendingDiagnostic
        {
            ::sed::DiagnosticSeverity severity{};
            std::string text{};
        };

        struct LoadResult
        {
            ::sed::Level level{};
            std::int64_t sedDocumentTimestamp{};
            QByteArray editorStatePayload{};
            QByteArray sedHistoryPayload{};
            std::optional<editor::DocumentRecoveryHistory> history{};
            ui::recovery::WorkspaceHistoryState workspaceHistory{};
            std::vector<PendingDiagnostic> diagnostics{};
            bool sedEditorStateSectionPresent{};
            bool sedHistorySectionPresent{};
            bool sedProjectTypeKnown{true};
            bool sedProjectTypeSelectedByUser{};
            bool sedPartialRecoveryConfirmed{};
            bool sedRequiresCleanSave{};
            bool sedDetachFromSource{};
            std::uint32_t sedUnidentifiedMissingSectionCount{};
            std::optional<std::uintmax_t> sedSourceSize{};
            std::optional<std::filesystem::file_time_type> sedSourceWriteTime{};
            QStringList sedWarnings{};
            QStringList sedMissingRequiredSections{};
        };

        updateLoadProgress(QString("Parsing %1...").arg(levelLabel), 20);

        const assets::ResourceLocator* resourceLocator =
            deferJklResourceResolution
                ? nullptr
                : stagedResources
                    ? &stagedResources->locator
                    : &resources_;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        // Copy the optional test seam before launching the worker.  The
        // callback is deliberately invoked only after native SED decoding and
        // before the post-read revision check, so tests can model a concurrent
        // source disappearance without touching GUI-owned state.
        const std::function<void()> directSedAfterLoad =
            directSedAfterLoadForTest_;
#endif
        auto loadFuture = std::async(
            std::launch::async,
            [levelPath, ext, resourceLocator, levImportOptions
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
             , directSedAfterLoad
#endif
            ]() -> LoadResult
            {
                LoadResult result{};

                if (ext == ".jed")
                {
                    ::sed::assets::JedLoadOptions options{};
                    options.resources = resourceLocator;
                    options.onDiagnostic = [&result](
                        const ::sed::DiagnosticSeverity severity,
                        const std::string_view message)
                    {
                        PendingDiagnostic entry{};
                        entry.severity = severity;
                        entry.text.assign(message.data(), message.size());
                        result.diagnostics.push_back(std::move(entry));
                    };
                    result.level = ::sed::assets::loadFromJED(
                        levelPath, options);
                    return result;
                }

                if (ext == ".sed")
                {
                    std::error_code sizeBeforeError{};
                    std::error_code timeBeforeError{};
                    const std::uintmax_t sizeBefore =
                        std::filesystem::file_size(levelPath, sizeBeforeError);
                    const std::filesystem::file_time_type timeBefore =
                        std::filesystem::last_write_time(
                            levelPath, timeBeforeError);
                    ui::project::DirectSedOpenResult opened =
                        ui::project::loadDirectSedDocument(levelPath);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                    if (directSedAfterLoad)
                    {
                        directSedAfterLoad();
                    }
#endif
                    std::error_code sizeAfterError{};
                    std::error_code timeAfterError{};
                    const std::uintmax_t sizeAfter =
                        std::filesystem::file_size(levelPath, sizeAfterError);
                    const std::filesystem::file_time_type timeAfter =
                        std::filesystem::last_write_time(
                            levelPath, timeAfterError);
                    if (!sizeBeforeError && !timeBeforeError
                        && !sizeAfterError && !timeAfterError
                        && (sizeBefore != sizeAfter
                            || timeBefore != timeAfter))
                    {
                        throw std::runtime_error(
                            "The SED source changed while it was being read; no recovered document was applied.");
                    }
                    // A direct native open is only safe when the source
                    // revision was observed both before and after decoding.
                    // Previously a transient stat/read error simply left the
                    // optional revision fields empty; the candidate then
                    // reached installation without the later replacement
                    // guard being able to detect a concurrent delete,
                    // replace, or permission change. Treat inability to
                    // establish either boundary as a failed, retryable Open
                    // and keep the current workspace untouched.
                    if (sizeBeforeError || timeBeforeError
                        || sizeAfterError || timeAfterError)
                    {
                        throw std::runtime_error(
                            "The SED source revision could not be verified while it was being read; no recovered document was applied.");
                    }
                    if (!sizeAfterError)
                    {
                        result.sedSourceSize = sizeAfter;
                    }
                    if (!timeAfterError)
                    {
                        result.sedSourceWriteTime = timeAfter;
                    }
                    result.sedEditorStateSectionPresent =
                        opened.editorStateSectionPresent;
                    result.sedDocumentTimestamp = opened.documentTimestamp;
                    result.sedHistorySectionPresent =
                        opened.historySectionPresent;
                    result.sedProjectTypeKnown = opened.projectTypeKnown;
                    result.sedRequiresCleanSave = opened.requiresCleanSave;
                    result.sedDetachFromSource = opened.detachFromSource;
                    result.sedUnidentifiedMissingSectionCount =
                        opened.unidentifiedMissingSectionCount;
                    result.sedWarnings = ui::project::describeDirectSedRecovery(opened);
                    result.sedMissingRequiredSections.reserve(
                        static_cast<qsizetype>(opened.missingRequiredSections.size()));
                    for (const std::string& section : opened.missingRequiredSections)
                    {
                        result.sedMissingRequiredSections.push_back(
                            QString::fromStdString(section));
                    }
                    result.level = std::move(opened.level);
                    result.editorStatePayload = std::move(opened.editorStatePayload);
                    result.sedHistoryPayload = std::move(opened.historyPayload);
                    result.history = std::move(opened.history);
                    result.workspaceHistory = std::move(opened.workspaceHistory);
                    return result;
                }

                if (ext == ".asc")
                {
                    ::sed::assets::AscLoadOptions options{};
                    options.resources = resourceLocator;
                    options.onDiagnostic = [&result](
                        const ::sed::DiagnosticSeverity severity,
                        const std::string_view message)
                    {
                        PendingDiagnostic entry{};
                        entry.severity = severity;
                        entry.text.assign(message.data(), message.size());
                        result.diagnostics.push_back(std::move(entry));
                    };
                    result.level = ::sed::assets::loadFromASC(
                        levelPath,
                        options);
                    return result;
                }

                if (ext == ".lev")
                {
                    ::sed::assets::LevLoadOptions options =
                        levImportOptions.value_or(
                            ui::project::defaultLevImportOptions());
                    options.resources = resourceLocator;
                    options.onDiagnostic = [&result](
                        const ::sed::DiagnosticSeverity severity,
                        const std::string_view message)
                    {
                        PendingDiagnostic entry{};
                        entry.severity = severity;
                        entry.text.assign(message.data(), message.size());
                        result.diagnostics.push_back(std::move(entry));
                    };
                    result.level = ::sed::assets::loadFromLEV(
                        levelPath,
                        options);
                    return result;
                }

                ::sed::assets::JklLoadOptions opts{};
                if (ext == ".ndy")
                {
                    opts.projectType = ::sed::ProjectType::IJIM;
                }
                opts.resources = resourceLocator;
                opts.onDiagnostic = [&result](const ::sed::DiagnosticSeverity severity, const std::string_view message)
                {
                    PendingDiagnostic entry{};
                    entry.severity = severity;
                    entry.text.assign(message.data(), message.size());
                    result.diagnostics.push_back(std::move(entry));
                };

                result.level = ::sed::assets::loadFromJKL(levelPath, opts);
                return result;
            });

        int progressPulse = 32;
        while (loadFuture.wait_for(std::chrono::milliseconds(25)) != std::future_status::ready)
        {
            progressPulse = std::min(progressPulse + 1, 70);
            progressDialog.setValue(progressPulse);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 25);
        }

        LoadResult loadResult = loadFuture.get();

        if (ext == ".sed" && !loadResult.sedProjectTypeKnown)
        {
            // Recovery had to synthesize neutral framing after both fixed
            // header copies and HEAD were lost. The placeholder JKDF2 value
            // is never authoritative: require an explicit user decision
            // before history decoding, resource mutation, document teardown,
            // or native text salvage.
            progressDialog.hide();
            QMessageBox projectPrompt(this);
            projectPrompt.setObjectName(QStringLiteral(
                "direct_sed_project_type_prompt"));
            projectPrompt.setIcon(QMessageBox::Warning);
            projectPrompt.setWindowTitle(QStringLiteral(
                "Choose project and open partial recovery?"));
            projectPrompt.setText(QStringLiteral(
                "The checked project type and required Level data could not be fully recovered from %1.")
                .arg(levelLabel));
            projectPrompt.setInformativeText(QStringLiteral(
                "Both protected fixed headers and the required HEAD section were lost. "
                "Choosing the original project opens the surviving data as an Untitled, modified document. "
                "Choose only if you know it; Cancel leaves the current document and damaged source unchanged."));
            projectPrompt.setDetailedText(loadResult.sedWarnings.join(
                QStringLiteral("\n")));
            QPushButton* chooseJkdf2 = projectPrompt.addButton(
                QStringLiteral("Open as Jedi Knight (JKDF2)"),
                QMessageBox::AcceptRole);
            chooseJkdf2->setObjectName(QStringLiteral(
                "direct_sed_choose_jkdf2"));
            QPushButton* chooseMots = projectPrompt.addButton(
                QStringLiteral("Open as Mysteries of the Sith (MotS)"),
                QMessageBox::AcceptRole);
            chooseMots->setObjectName(QStringLiteral(
                "direct_sed_choose_mots"));
            QPushButton* chooseIjim = projectPrompt.addButton(
                QStringLiteral("Open as Indiana Jones (IJIM)"),
                QMessageBox::AcceptRole);
            chooseIjim->setObjectName(QStringLiteral(
                "direct_sed_choose_ijim"));
            QPushButton* cancelProject = projectPrompt.addButton(
                QMessageBox::Cancel);
            cancelProject->setObjectName(QStringLiteral(
                "direct_sed_cancel_project_type"));
            projectPrompt.setDefaultButton(cancelProject);
            projectPrompt.setEscapeButton(cancelProject);
            (void)projectPrompt.exec();

            std::optional<ProjectType> selectedProject{};
            if (projectPrompt.clickedButton() == chooseJkdf2)
            {
                selectedProject = ProjectType::JKDF2;
            }
            else if (projectPrompt.clickedButton() == chooseMots)
            {
                selectedProject = ProjectType::MOTS;
            }
            else if (projectPrompt.clickedButton() == chooseIjim)
            {
                selectedProject = ProjectType::IJIM;
            }
            if (!selectedProject)
            {
                progressDialog.close();
                statusBar()->showMessage(QStringLiteral(
                    "Partial SED project selection cancelled; the current document was not changed."));
                return;
            }

            // HEAD was lost as a whole, so replace only its project/header
            // defaults. This routine does not touch any surviving semantic
            // section; in particular it gives IJIM a valid version-3 header
            // before resource setup, history validation, or NDY salvage.
            ui::project::applyNewProjectTypeDefaults(
                loadResult.level, *selectedProject);
            loadResult.sedProjectTypeKnown = true;
            loadResult.sedProjectTypeSelectedByUser = true;
            loadResult.sedPartialRecoveryConfirmed = true;
            loadResult.sedDetachFromSource = true;
            loadResult.sedRequiresCleanSave = true;
            loadResult.sedWarnings.push_back(QStringLiteral(
                "Checked project metadata was unavailable; the user explicitly selected %1 for this detached recovery.")
                .arg(shortProjectTypeLabel(*selectedProject)));

            if (loadResult.sedHistorySectionPresent)
            {
                QString historyError{};
                QString workspaceWarning{};
                QByteArray validatedEditorState{};
                loadResult.history =
                    ui::recovery::deserializeEditorDocumentHistoryForRestoreWithWorkspaceBindings(
                        loadResult.sedHistoryPayload,
                        *selectedProject,
                        loadResult.level,
                        loadResult.editorStatePayload,
                        &validatedEditorState,
                        &historyError,
                        &workspaceWarning,
                        &loadResult.workspaceHistory,
                        &ui::recovery::validateWorkspaceHistoryAttachmentForLevels);
                if (!validatedEditorState.isEmpty())
                {
                    loadResult.editorStatePayload = std::move(validatedEditorState);
                }
                if (!workspaceWarning.isEmpty())
                {
                    loadResult.sedWarnings.push_back(workspaceWarning);
                }
                if (!loadResult.history)
                {
                    // The selected project makes HIST decodable, not
                    // authoritative. Preserve the recovered Level and fall
                    // back to an empty timeline when optional history is
                    // semantically invalid for that project.
                    loadResult.sedWarnings.push_back(QStringLiteral(
                        "HIST semantic validation failed for the selected project: %1. The Level was retained, undo/redo history was dropped, and a clean save is required.")
                        .arg(historyError.isEmpty()
                            ? QStringLiteral("unknown schema error")
                            : historyError));
                    loadResult.sedHistoryPayload.clear();
                    loadResult.sedHistorySectionPresent = false;
                    loadResult.sedRequiresCleanSave = true;
                }
            }

            progressDialog.show();
            updateLoadProgress(QString("Project type selected for %1...")
                .arg(levelLabel), 70);
        }

        if (loadResult.sedDetachFromSource
            && !loadResult.sedPartialRecoveryConfirmed)
        {
            // Required-domain loss changes the meaning of the Level, and an
            // unidentified unit may conceal the same. Ask before tearing down
            // any interaction or replacing document_ so Cancel leaves the
            // current document and path untouched.
            progressDialog.hide();
            QMessageBox partialPrompt(this);
            partialPrompt.setObjectName(QStringLiteral(
                "direct_sed_partial_recovery_prompt"));
            partialPrompt.setIcon(QMessageBox::Warning);
            partialPrompt.setWindowTitle(QStringLiteral(
                "Open partial SED recovery?"));
            const bool hasNamedRequiredLoss =
                !loadResult.sedMissingRequiredSections.isEmpty();
            const bool hasUnidentifiedLoss =
                loadResult.sedUnidentifiedMissingSectionCount != 0;
            partialPrompt.setText(hasNamedRequiredLoss
                ? QStringLiteral(
                    "Required Level data could not be reconstructed from %1.")
                    .arg(levelLabel)
                : QStringLiteral(
                    "Damaged SED section data could not be safely identified in %1.")
                    .arg(levelLabel));
            partialPrompt.setInformativeText(QStringLiteral(
                "%1Opening the surviving data will replace the current workspace with an Untitled, modified document. "
                "The damaged source will remain unchanged and Save will require a new destination.")
                .arg(hasUnidentifiedLoss
                    ? QStringLiteral(
                        "An unidentified unit may contain required Level data. ")
                    : QString{}));
            partialPrompt.setDetailedText(loadResult.sedWarnings.join(
                QStringLiteral("\n")));
            QPushButton* openRecovered = partialPrompt.addButton(
                QStringLiteral("Open Recovered Copy"), QMessageBox::AcceptRole);
            openRecovered->setObjectName(QStringLiteral(
                "direct_sed_open_recovered_copy"));
            QPushButton* cancelRecovery = partialPrompt.addButton(
                QMessageBox::Cancel);
            cancelRecovery->setObjectName(QStringLiteral(
                "direct_sed_cancel_partial_recovery"));
            partialPrompt.setDefaultButton(cancelRecovery);
            partialPrompt.setEscapeButton(cancelRecovery);
            (void)partialPrompt.exec();
            if (partialPrompt.clickedButton() != openRecovered)
            {
                progressDialog.close();
                statusBar()->showMessage(QStringLiteral(
                    "Partial SED recovery cancelled; the current document was not changed."));
                return;
            }
            progressDialog.show();
            updateLoadProgress(QString("Recovery accepted for %1...")
                .arg(levelLabel), 71);
        }

        const auto sedSourceRevisionStillCurrent = [&]()
        {
            if (!deferSedResourceSetup
                || !loadResult.sedSourceSize
                || !loadResult.sedSourceWriteTime)
            {
                return true;
            }
            std::error_code sizeError{};
            std::error_code timeError{};
            const std::uintmax_t currentSize = std::filesystem::file_size(
                levelPath, sizeError);
            const std::filesystem::file_time_type currentWriteTime =
                std::filesystem::last_write_time(levelPath, timeError);
            return !sizeError && !timeError
                && currentSize == *loadResult.sedSourceSize
                && currentWriteTime == *loadResult.sedSourceWriteTime;
        };
        const auto rejectChangedSedSource = [&]()
        {
            progressDialog.close();
            const QString warning = QStringLiteral(
                "The SED source changed after validation and before installation; the current document and resources were left unchanged.");
            logMessage(MessageSeverity::warning, warning);
            statusBar()->showMessage(warning);
        };
        if (!sedSourceRevisionStillCurrent())
        {
            rejectChangedSedSource();
            return;
        }

        if (deferSedResourceSetup)
        {
            // At this point strict/native recovery succeeded and any required
            // partial-document confirmation was accepted. Use only the
            // authoritative/recovered kind when publishing candidate
            // resource state.
            kindGuess = loadResult.level.kind;
            updateLoadProgress(QString("Preparing resources for %1...")
                .arg(levelLabel), 72);
            stagedResources.emplace(stageProjectResources(kindGuess, levelPath));
            updateLoadProgress(QString("Scanning resources for %1...")
                .arg(levelLabel), 73);
        }
        else if (deferJklResourceResolution)
        {
            // The resource-independent JKL pass is authoritative for the
            // project kind. Stage only that project's locator before the
            // worker performs COG hydration and MAT-dependent UV conversion.
            kindGuess = loadResult.level.kind;
            updateLoadProgress(QString("Preparing resources for %1...")
                .arg(levelLabel), 72);
            stagedResources.emplace(stageProjectResources(kindGuess, levelPath));
            updateLoadProgress(QString("Scanning resources for %1...")
                .arg(levelLabel), 73);
        }
        else if (stagedResources
            && loadResult.level.kind != stagedResources->kind)
        {
            // Some interchange headers refine the provisional extension-based
            // project guess. Stage the authoritative project's locator now,
            // while the old document still owns the active resource state.
            kindGuess = loadResult.level.kind;
            updateLoadProgress(QString("Preparing final resources for %1...")
                .arg(levelLabel), 73);
            stagedResources.emplace(stageProjectResources(kindGuess, levelPath));
        }

        if (deferJklResourceResolution && stagedResources)
        {
            // JKL VERSION 1 files do not identify JKDF2 versus MotS before
            // their geometry has been decoded. The first pass deliberately
            // had no locator, so an extension guess could not hydrate COG
            // schemas or normalize wire pixels with same-name resources from
            // the wrong game. Complete both resource-dependent stages only
            // after the authoritative project resource state is staged, and
            // keep the work on the worker thread.
            const assets::ResourceLocator* authoritativeResources =
                &stagedResources->locator;
            auto initialDiagnostics = std::move(loadResult.diagnostics);
            auto finalizeFuture = std::async(
                std::launch::async,
                [level = std::move(loadResult.level),
                    authoritativeResources,
                    diagnostics = std::move(initialDiagnostics)]() mutable
                    -> LoadResult
                {
                    LoadResult result{};
                    result.level = std::move(level);
                    result.diagnostics = std::move(diagnostics);
                    assets::finalizeJklImportResources(
                        result.level,
                        *authoritativeResources,
                        [&result](
                            const ::sed::DiagnosticSeverity severity,
                            const std::string_view message)
                        {
                            PendingDiagnostic entry{};
                            entry.severity = severity;
                            entry.text.assign(message.data(), message.size());
                            result.diagnostics.push_back(std::move(entry));
                        });
                    return result;
                });
            updateLoadProgress(QString("Resolving resources for %1...")
                .arg(levelLabel), 80);
            while (finalizeFuture.wait_for(std::chrono::milliseconds(25))
                != std::future_status::ready)
            {
                updateLoadProgress(QString("Resolving resources for %1...")
                    .arg(levelLabel), 80);
            }
            loadResult = finalizeFuture.get();
        }

        const bool openedNativeLegacyUvDocument = ext == ".sed"
            && loadResult.level.uvCoordinateSpace
                == UvCoordinateSpace::TexturePixels;
        std::optional<editor::DocumentUvConversionResult>
            legacyUvOpenConversion{};
        if (openedNativeLegacyUvDocument
            && loadLegacyUvPromotionPolicy()
                != LegacyUvPromotionPolicy::keepPixelSpace)
        {
            auto makeDimensionsResolver = [&stagedResources]()
                -> level::UvTextureDimensionsResolver
            {
                auto cache = std::make_shared<std::unordered_map<std::string,
                    std::optional<level::UvTextureDimensions>>>();
                const assets::ResourceLocator* const locator =
                    &stagedResources->locator;
                return [cache, locator](const Surface& surface)
                    -> std::optional<level::UvTextureDimensions>
                {
                    const std::string key = text::lowerAscii(
                        text::trimView(surface.material));
                    if (const auto found = cache->find(key);
                        found != cache->end())
                    {
                        return found->second;
                    }
                    std::optional<level::UvTextureDimensions> dimensions =
                        assets::resolveMaterialTextureDimensions(
                            *locator, surface.material);
                    cache->emplace(key, dimensions);
                    return dimensions;
                };
            };

            const LegacyUvPromotionPolicy policy =
                loadLegacyUvPromotionPolicy();
            if (policy == LegacyUvPromotionPolicy::askBeforeNormalizing)
            {
                const QString nativeVersionLabel =
                    nativeLegacyUvVersionLabel(loadResult.level);
                Level convertedLevel = loadResult.level;
                editor::DocumentRecoveryHistory convertedHistory =
                    loadResult.history.value_or(
                        editor::DocumentRecoveryHistory{});
                const editor::DocumentUvConversionResult conversion =
                    editor::convertDocumentUvCoordinateSpace(
                        convertedLevel,
                        convertedHistory,
                        UvCoordinateSpace::Normalized,
                        makeDimensionsResolver());
                legacyUvOpenConversion = conversion;
                if (conversion)
                {
                    progressDialog.hide();
                    QMessageBox prompt(this);
                    prompt.setObjectName(QStringLiteral(
                        "legacy_sed_uv_promotion_prompt"));
                    prompt.setIcon(QMessageBox::Warning);
                    prompt.setWindowTitle(QStringLiteral(
                        "Normalize legacy SED UVs?"));
                    const QString nativeVersionDisplay =
                        QStringLiteral("SED %1").arg(nativeVersionLabel);
                    prompt.setText(QStringLiteral(
                        "%1 uses the %2 UV format.")
                        .arg(levelLabel, nativeVersionDisplay));
                    prompt.setInformativeText(QStringLiteral(
                        "Normalize the complete Level and its undo/redo "
                        "history now? The visible texture mapping is "
                        "preserved. Choosing Keep Pixel Space opens the file "
                        "without conversion. Cancel leaves the current "
                        "workspace unchanged."));
                    QPushButton* normalize = prompt.addButton(
                        QStringLiteral("Normalize"), QMessageBox::AcceptRole);
                    normalize->setObjectName(QStringLiteral(
                        "legacy_sed_uv_normalize"));
                    QPushButton* keep = prompt.addButton(
                        QStringLiteral("Keep Pixel Space"),
                        QMessageBox::DestructiveRole);
                    keep->setObjectName(QStringLiteral(
                        "legacy_sed_uv_keep_pixels"));
                    QPushButton* cancel = prompt.addButton(
                        QMessageBox::Cancel);
                    cancel->setObjectName(QStringLiteral(
                        "legacy_sed_uv_cancel"));
                    prompt.setDefaultButton(normalize);
                    prompt.setEscapeButton(cancel);
                    (void)prompt.exec();
                    if (prompt.clickedButton() == normalize)
                    {
                        loadResult.level = std::move(convertedLevel);
                        if (loadResult.history)
                        {
                            loadResult.history = std::move(convertedHistory);
                        }
                    }
                    else if (prompt.clickedButton() != keep)
                    {
                        progressDialog.close();
                        statusBar()->showMessage(QStringLiteral(
                            "Legacy UV normalization cancelled; the current "
                            "document was not changed."));
                        return;
                    }
                    progressDialog.show();
                }
                else
                {
                    loadResult.sedWarnings.push_back(
                        describeLegacyUvPromotionFailure(
                            conversion, nativeVersionLabel));
                }
            }
            else
            {
                const QString nativeVersionLabel =
                    nativeLegacyUvVersionLabel(loadResult.level);
                editor::DocumentRecoveryHistory convertedHistory =
                    loadResult.history.value_or(
                        editor::DocumentRecoveryHistory{});
                const editor::DocumentUvConversionResult conversion =
                    editor::convertDocumentUvCoordinateSpace(
                        loadResult.level,
                        convertedHistory,
                        UvCoordinateSpace::Normalized,
                        makeDimensionsResolver());
                legacyUvOpenConversion = conversion;
                if (conversion)
                {
                    if (loadResult.history)
                    {
                        loadResult.history = std::move(convertedHistory);
                    }
                }
                else
                {
                    loadResult.sedWarnings.push_back(
                        describeLegacyUvPromotionFailure(
                            conversion, nativeVersionLabel));
                }
            }
        }

        // Template parsing is also part of the candidate transaction. Keep it
        // detached from templates_, placement thumbnails, and GlView until the
        // final workspace fingerprint has been accepted.
        StagedGlobalTemplateState stagedTemplates =
            stageGlobalTemplates(loadResult.level);

        // Import history before the final authorization and teardown boundary.
        // Allocation failure still leaves the complete live workspace intact.
        editor::Document stagedDocument;
        stagedDocument.restoreRecoveryHistory(
            editor::DocumentState{std::move(loadResult.level), {}},
            loadResult.history ? std::move(*loadResult.history)
                               : editor::DocumentRecoveryHistory{});

        struct ScopedViewportUpdates
        {
            GlView* view{};
            bool previous{};
            QLabel* overlay{};

            explicit ScopedViewportUpdates(GlView* glView) : view(glView), previous(glView ? glView->updatesEnabled() : true)
            {
                if (view)
                {
                    QWidget* overlayParent = view->parentWidget() ? view->parentWidget() : static_cast<QWidget*>(view);
                    overlay = new QLabel(overlayParent);
                    overlay->setObjectName("viewport_load_mask");
                    overlay->setGeometry((overlayParent == view) ? view->rect() : view->geometry());
                    overlay->setAlignment(Qt::AlignCenter);
                    overlay->setScaledContents(true);
                    overlay->setFrameShape(QFrame::NoFrame);
                    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
                    overlay->setStyleSheet(
                        "#viewport_load_mask {"
                        " background-color: #10141b;"
                        " border: 1px solid #1c2430;"
                        " border-radius: 6px;"
                        "}");

                    if (view->width() > 0 && view->height() > 0)
                    {
                        const QImage frame = view->grabFramebuffer();
                        if (!frame.isNull())
                        {
                            overlay->setPixmap(QPixmap::fromImage(frame));
                        }
                    }

                    overlay->show();
                    overlay->raise();
                    view->setUpdatesEnabled(false);
                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                }
            }

            ~ScopedViewportUpdates()
            {
                if (view)
                {
                    view->setUpdatesEnabled(previous);
                    view->update();
                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                }
                if (overlay)
                {
                    overlay->hide();
                    overlay->deleteLater();
                }
            }
        };

        ScopedViewportUpdates suspendViewport(glView_);

        updateLoadProgress(QString("Applying %1...").arg(levelLabel), 74);
        // ScopedViewportUpdates and updateLoadProgress both pump non-user Qt
        // events. This is the decisive authorization check: no event loop is
        // entered between it and teardown/publication, so timer/plugin edits
        // made at any earlier load boundary cannot be overwritten.
        if (!replacementAuthorizationStillCurrent())
        {
            const QString warning = QStringLiteral(
                "The current workspace changed while the level was loading; the new level was not applied.");
            logMessage(MessageSeverity::warning, warning);
            statusBar()->showMessage(warning);
            progressDialog.close();
            return;
        }
        // Resource staging, legacy UV conversion, and the Ask policy dialog can
        // take time and pump events. Re-check the exact decoded source at the
        // final no-event-pump installation boundary so an externally replaced
        // pixel-space file is never installed or retained as its associated source.
        if (!sedSourceRevisionStillCurrent())
        {
            rejectChangedSedSource();
            return;
        }
        if (!stagedResources)
        {
            throw std::runtime_error(
                "The candidate resource state was not prepared before level installation.");
        }

        if (thingPlacementBrushStrokeActive_)
        {
            (void)finalizeThingPlacementBrushStroke();
        }

        // Tear down every interaction that belongs to the old document before
        // replacing its storage.  Several previews (cleave, join, extrude,
        // brush and transform) retain indices/pointers into the current level;
        // clearing the document first leaves those indices pointing at the new
        // level and can keep a hidden tool active after a normal level load.
        resetSelectionContextState();
        // Save/Discard/Cancel for every old COG tab was authorized before the
        // load. Close those pages only now that parsing succeeded and document
        // replacement is committing; a failed/cancelled load leaves them live.
        closeCogScriptEditorsAfterReplacement();

        // Publish Level resources and templates as one non-event-pumping
        // generation. Their owning objects keep stable addresses for GlView;
        // only their contents change at this accepted replacement boundary.
        closeResourceEditorsAfterReplacement();
        resourceKind_ = stagedResources->kind;
        resourceRoots_ = std::move(stagedResources->roots);
        resourceGobs_ = std::move(stagedResources->gobs);
        resources_ = std::move(stagedResources->locator);
        publishResourceLocatorSnapshot(
            std::move(stagedResources->locatorSnapshot));
        const ProjectType previousProjectType = document_.level().kind;
        document_.swap(stagedDocument);
        const bool projectTypeChanged =
            previousProjectType != document_.level().kind;
        // The ambience preferred-sector hint belongs to the preceding Level
        // generation.  Reusing its numeric index in a new overlapping map
        // could select a valid-but-unrelated Sector and restart the wrong
        // loop even though the containment BVH itself was rebuilt.
        viewportSoundSector_ = -1;
        if (ext == ".sed")
        {
            nativeDocumentTimestamp_ = loadResult.sedDocumentTimestamp;
        }
        else
        {
            const std::int64_t openedAt = std::max<std::int64_t>(
                1, QDateTime::currentMSecsSinceEpoch());
            nativeDocumentTimestamp_ = openedAt;
        }
        resetLightBakeSourceTracking();
        publishGlobalTemplates(std::move(stagedTemplates), false);

        // Runtime/interchange formats carry no editor layer palette. Do not let
        // a prior document or legacy preference paint their default layer red;
        // native SED restoration below replaces this white baseline from EDIT.
        resetLayerColorsToDefaults();
        resetLayerStatesFromLevel(false);
        lastEngineExportPath_.clear();
        if (ui::project::isEngineLevelExportPath(levelPath, document_.level().kind))
        {
            lastEngineExportPath_ = levelPath;
        }
        currentPath_ = loadResult.sedDetachFromSource
            ? std::filesystem::path{}
            : levelPath;
        nativeSourceUsesLegacyUvFormat_ =
            !loadResult.sedDetachFromSource
            && openedNativeLegacyUvDocument;
        switchRecoveryDocument(
            currentPath_, loadResult.sedDetachFromSource, false);

        // Do not pump Qt events between document publication and GlView's
        // matching resource/template/Level pointers. Timers and plugins must
        // never observe a mixed old/new workspace generation.
        progressDialog.setLabelText(
            QString("Building viewport/resource cache for %1...").arg(levelLabel));
        progressDialog.setValue(90);
        // Defer GPU upload until the restored editor state has been applied;
        // the old document's meshes are no longer useful after clearTextures.
        glView_->setResourceLocator(&resources_, false);
        const assets::Templates* publishedTemplates =
            templates_.size() > 0 ? &templates_ : nullptr;
        // Publish the level and staged templates in one render-cache build.
        // The SED editor chunk is applied below; the temporary view is hidden
        // by ScopedViewportUpdates while this transaction completes.
        const bool deferSedRenderRebuild =
            ext == ".sed" && !loadResult.editorStatePayload.isEmpty();
        glView_->setLevel(
            &document_.level(),
            deferSedRenderRebuild ? GlView::SetLevelMode::keepView : GlView::SetLevelMode::resetView,
            publishedTemplates,
            !deferSedRenderRebuild);
        // Arch Thing marker choices are application preferences shared across
        // projects. Refresh them only at an accepted type transition; a .sed
        // EDIT payload restored below remains authoritative for that workspace.
        if (projectTypeChanged)
        {
            applyArchThingGizmoPreferences();
        }
        refreshPublishedGlobalTemplates();

        // Persistence and diagnostics are delayed until the complete accepted
        // generation is installed, but still occur before the first
        // post-commit event pump.
        // The vectors were moved into the active generation above. Persist
        // that accepted state, not the now-empty staging shell.
        StagedProjectResourceState acceptedResourceSettings{};
        acceptedResourceSettings.kind = resourceKind_;
        acceptedResourceSettings.roots = resourceRoots_;
        acceptedResourceSettings.gobs = resourceGobs_;
        acceptedResourceSettings.detectedInstallDir =
            stagedResources->detectedInstallDir;
        persistStagedProjectResources(acceptedResourceSettings);
        bool restoredSedEditorState = false;
        if (ext == ".sed" && loadResult.sedEditorStateSectionPresent)
        {
            // A Sith Editor file is a complete workspace document. Restore its
            // authoritative camera/tools/layout after the one mesh build.
            progressDialog.setLabelText(
                QString("Restoring editor workspace for %1...").arg(levelLabel));
            progressDialog.setValue(93);
            QString warning{};
            restoredSedEditorState = restoreRecoveryEditorState(
                loadResult.editorStatePayload, &warning, deferSedRenderRebuild);
            if (!warning.isEmpty())
            {
                logMessage(MessageSeverity::warning, warning);
            }
            if (!restoredSedEditorState)
            {
                // The optional payload is all-or-nothing. A safe default
                // workspace is usable, but the associated SED must be
                // rewritten before it can be considered clean.
                loadResult.sedRequiresCleanSave = true;
            }
            if (deferSedRenderRebuild)
            {
                if (restoredSedEditorState)
                {
                    // All render-affecting viewport fields are now final, so
                    // publish exactly one complete mesh/cache build. The
                    // operation preserves the recovered camera; suppress only
                    // UV View Face recomputation so resource/render warnings
                    // emitted by the rebuild still reach the user.
                    progressDialog.setLabelText(
                        QString("Publishing restored viewport for %1...").arg(levelLabel));
                    progressDialog.setValue(95);
                    const QScopedValueRollback suppressUvViewTransformSync(
                        uvEditorSuppressViewTransformSync_, true);
                    glView_->notifyAllMeshesChanged();
                }
                else
                {
                    // A malformed optional editor chunk must still leave a
                    // fully renderable level; fall back to the ordinary reset
                    // path with one build rather than retaining deferred data.
                    glView_->setLevel(
                        &document_.level(),
                        GlView::SetLevelMode::resetView,
                        publishedTemplates,
                        true);
                }
            }
            if (restoredSedEditorState)
            {
                pendingEditorViewRefitAfterResize_ = false;
                editorViewRefitLastViewportSize_ = {};
                if (editorViewRefitTimer_)
                {
                    editorViewRefitTimer_->stop();
                }
            }
        }
        if (restoredSedEditorState
            && !loadResult.workspaceHistory.attachments.empty())
        {
            QStringList attachmentWarnings{};
            restoreWorkspaceHistoryAttachments(
                loadResult.workspaceHistory, &attachmentWarnings);
            for (const QString& warning : attachmentWarnings)
            {
                logMessage(MessageSeverity::warning, warning);
            }
        }

        // Keep this guard younger than ScopedViewportUpdates so its request
        // is armed immediately before that guard re-enables the viewport.
        // It also preserves a usable bounds fit if a nonessential late
        // bookkeeping step throws after the new Level has already committed.
        const auto finalEditorViewRefit = qScopeGuard(
            [this, &restoredSedEditorState]()
            {
                if (!restoredSedEditorState)
                {
                    scheduleEditorViewRefitAfterLayout();
                }
            });

        if (!restoredSedEditorState)
        {
            // Runtime/interchange formats intentionally contain no workspace
            // state and retain the established clean editor reset + refit.
            // Temporary grid alignment/center overrides are editor-session
            // state too.  They must not leak from the previously opened
            // document into an NDY/JKL/JED import (or a SED whose EDIT chunk
            // was absent/invalid); a valid SED EDIT payload is restored in
            // the branch above instead.
            glView_->resetGridOverrides();
            glView_->setRenderPipeline(GlView::RenderPipeline::Editor);
            glView_->resetPreviewViewportState();
            glView_->setLightingMode(GlView::LightingMode::Gouraud);
            glView_->setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
            glView_->resetFlySpeedMultiplier();
            glView_->setShowWireframe(true);
            glView_->setShowVertices(false);
            glView_->setShowSolid(true);
            glView_->setShowTextures(true);
            glView_->setShowFog(glView_->editorDefaultShowFog());
            glView_->setShowThingCollisionShapes(
                glView_->editorDefaultShowThingCollisionShapes());
            glView_->setShowThingVisualBounds(
                glView_->editorDefaultShowThingVisualBounds());
            glView_->setShowThings(true);
            glView_->setShowThingGizmos(glView_->editorDefaultShowThingGizmos());
            glView_->setThingViewMode(GlView::ThingViewMode::Shaded);
            glView_->setFocus(Qt::OtherFocusReason);
            glView_->resetEditorTopDownView();
            glView_->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
            setPlacementMode(PlacementMode::none);
            setSurfaceSelectionMode(SurfaceSelectionMode::face);
            setEditMode(EditMode::none);
            if (uvEditorPanelHost_)
            {
                uvEditorPanelHost_->hide();
            }
            if (outlinerTabs_)
            {
                outlinerTabs_->setCurrentIndex(0);
            }

            if (viewportGamePreviewAction_)
            {
                viewportGamePreviewAction_->setChecked(false);
            }
            if (viewportGouraudAction_)
            {
                viewportGouraudAction_->setChecked(true);
            }
        }

        // Flush candidate diagnostics only after both the Level generation and
        // complete restored/default editor state are coherent. QTreeWidget
        // insertion can invoke synchronous observers even without an event
        // pump, so logging earlier exposed a half-restored workspace.
        reportStagedProjectResources(*stagedResources);
        reportStagedGlobalTemplates(stagedTemplates);
        for (const PendingDiagnostic& diagnostic : loadResult.diagnostics)
        {
            const QString text = QString::fromUtf8(
                diagnostic.text.data(), static_cast<int>(diagnostic.text.size()));
            switch (diagnostic.severity)
            {
                case ::sed::DiagnosticSeverity::verbose: logMessage(MessageSeverity::verbose, text); break;
                case ::sed::DiagnosticSeverity::trace: logMessage(MessageSeverity::trace, text); break;
                case ::sed::DiagnosticSeverity::debug: logMessage(MessageSeverity::debug, text); break;
                case ::sed::DiagnosticSeverity::info: logMessage(MessageSeverity::info, text); break;
                case ::sed::DiagnosticSeverity::warning: logMessage(MessageSeverity::warning, text); break;
                case ::sed::DiagnosticSeverity::error: logMessage(MessageSeverity::error, text); break;
                case ::sed::DiagnosticSeverity::fatal: logMessage(MessageSeverity::fatal, text); break;
            }
        }
        for (const QString& warning : loadResult.sedWarnings)
        {
            logMessage(MessageSeverity::warning, warning);
        }

        updateLoadProgress(QString("Finalizing %1...").arg(levelLabel), 96);
        rebuildOutliner();
        rebuildProperties();
        syncCogScriptWatches();
        rebuildChecks();
        if (!restoredSedEditorState)
        {
            restoreStandaloneOutlinerDockWidth();
        }
        markCurrentDocumentAsSaved();
        if (loadResult.sedRequiresCleanSave)
        {
            // Repaired framing/data and optional-section defaults are usable,
            // but never pristine. Required-domain or unidentified loss is
            // additionally detached above, so this flag produces Untitled*
            // and forces Save As instead of making the damaged source an
            // overwrite target.
            recoveryRequiresSave_ = true;
            refreshWindowTitle();
        }
        resetLegacyUvCompatibilityStatus();
        if ( document_.level().uvCoordinateSpace
                == UvCoordinateSpace::TexturePixels
            && legacyUvOpenConversion )
        {
            legacyUvPromotionReady_ =
                static_cast<bool>(*legacyUvOpenConversion);
            legacyUvBlockedMaterial_ =
                legacyUvOpenConversion->failedMaterial;
            legacyUvCompatibilityDetail_ =
                legacyUvOpenConversion->error.empty()
                    ? QStringLiteral(
                        "Every required material size is available. The "
                        "complete Level and its undo/redo history can be "
                        "normalized without changing the visible texture "
                        "mapping.")
                    : describeLegacyUvPromotionFailure(
                        *legacyUvOpenConversion,
                        nativeLegacyUvVersionLabel(document_.level()));
            updateLegacyUvCompatibilityStatusLabel();
        }
        else if ( document_.level().uvCoordinateSpace
            == UvCoordinateSpace::TexturePixels )
        {
            evaluateLegacyUvCompatibility(false, false);
        }
        updateEditActions();
        if (!restoredSedEditorState)
        {
            // A runtime-format Open has no tool state. Select is also GlView's
            // neutral enum, so explicitly keep both interaction and its icon
            // inactive after every late edit/action refresh.
            setViewportTransformActionChecks(nullptr);
            rememberedViewportTransformAction_ = nullptr;
            if (glView_)
            {
                glView_->setTransformTool(GlView::TransformTool::Select);
                glView_->setTransformInteractionEnabled(false);
            }
            syncViewportSelectionInputEnabled();
        }
        // Freeze the clean EDIT baseline only after late tool/action
        // normalization. Capturing it before updateEditActions() made an
        // untouched load look like an editor-only modification.
        markRecoveryWorkspaceAsSaved();
        updateLoadProgress(QString("Loaded %1").arg(levelLabel), 100);
        progressDialog.close();

        if (!loadResult.sedMissingRequiredSections.isEmpty()
            || loadResult.sedUnidentifiedMissingSectionCount != 0)
        {
            ui::recovery::PartialLevelSalvageContext salvageContext{};
            salvageContext.sourceRecoveryPath = levelPath;
            // The directly opened source is itself the unchanged forensic
            // evidence. Artifacts are written beside it under collision-safe
            // names; no extra copy and no source rewrite is performed.
            salvageContext.forensicRecoveryPath = levelPath;
            salvageContext.missingRequiredSections =
                loadResult.sedMissingRequiredSections;
            salvageContext.unidentifiedMissingSectionCount =
                loadResult.sedUnidentifiedMissingSectionCount;
            salvageContext.recoveryWarnings = loadResult.sedWarnings;
            salvageContext.projectTypeSource =
                loadResult.sedProjectTypeSelectedByUser
                    ? ui::recovery::PartialLevelProjectTypeSource::userSelected
                    : loadResult.sedProjectTypeKnown
                        ? ui::recovery::PartialLevelProjectTypeSource::checkedMetadata
                        : ui::recovery::PartialLevelProjectTypeSource::unknown;
            salvageContext.nativeExportResources = resourceLocatorSnapshot_;
            const QStringList acceptedMissingRequiredSections =
                ui::recovery::lostRequiredLevelSections(
                    loadResult.sedMissingRequiredSections);
            const std::uint32_t acceptedUnidentifiedMissingSections =
                loadResult.sedUnidentifiedMissingSectionCount;
            const bool acceptedUserProjectChoice =
                loadResult.sedProjectTypeSelectedByUser;
            const ProjectType acceptedProjectType = document_.level().kind;
            const std::optional<std::uintmax_t> acceptedSourceSize =
                loadResult.sedSourceSize;
            const std::optional<std::filesystem::file_time_type>
                acceptedSourceWriteTime = loadResult.sedSourceWriteTime;
            std::function<void()> salvageBeforeReread{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            salvageBeforeReread = directSedSalvageBeforeRereadForTest_;
#endif
            logMessage(MessageSeverity::warning, QStringLiteral(
                "A non-authoritative partial-Level text salvage is being generated in the background beside %1; Open will not modify the damaged source.")
                .arg(ui::nativeDisplayPath(levelPath)));

            auto* salvageWatcher =
                new QFutureWatcher<ui::recovery::PartialLevelSalvageResult>(this);
            connect(salvageWatcher,
                &QFutureWatcher<ui::recovery::PartialLevelSalvageResult>::finished,
                this,
                [this, salvageWatcher]()
                {
                    const ui::recovery::PartialLevelSalvageResult salvage =
                        salvageWatcher->result();
                    const QString warning = salvage.recoveryWarning();
                    if (!warning.isEmpty())
                    {
                        logMessage(MessageSeverity::warning, warning);
                    }
                    salvageWatcher->deleteLater();
                });
            salvageWatcher->setFuture(QtConcurrent::run(
                QThreadPool::globalInstance(),
                [levelPath,
                 salvageContext = std::move(salvageContext),
                 acceptedMissingRequiredSections,
                 acceptedUnidentifiedMissingSections,
                 acceptedUserProjectChoice,
                 acceptedProjectType,
                 acceptedSourceSize,
                 acceptedSourceWriteTime,
                 salvageBeforeReread = std::move(salvageBeforeReread)]()
                {
                    try
                    {
                        const auto sourceChangedFailure = [](
                            const QString& detail)
                        {
                            ui::recovery::PartialLevelSalvageResult failure{};
                            failure.error = QStringLiteral(
                                "Background direct-SED salvage was not written because the source changed after recovery was accepted: %1")
                                .arg(detail);
                            return failure;
                        };
                        if (salvageBeforeReread)
                        {
                            salvageBeforeReread();
                        }
                        if (acceptedSourceSize)
                        {
                            std::error_code sizeError{};
                            const std::uintmax_t currentSize =
                                std::filesystem::file_size(levelPath, sizeError);
                            if (sizeError || currentSize != *acceptedSourceSize)
                            {
                                return sourceChangedFailure(QStringLiteral(
                                    "file size no longer matches"));
                            }
                        }
                        if (acceptedSourceWriteTime)
                        {
                            std::error_code timeError{};
                            const std::filesystem::file_time_type currentWriteTime =
                                std::filesystem::last_write_time(
                                    levelPath, timeError);
                            if (timeError
                                || currentWriteTime != *acceptedSourceWriteTime)
                            {
                                return sourceChangedFailure(QStringLiteral(
                                    "last-write time no longer matches"));
                            }
                        }

                        // Re-read on the worker to avoid a potentially large
                        // GUI-thread Level copy. recoverSedFile is read-only;
                        // the original remains the forensic source. Require
                        // the same semantic loss/project classification that
                        // the user accepted before exporting under its audit
                        // warnings.
                        assets::SedRecoveryLoadResult recovered =
                            assets::recoverSedFile(levelPath);
                        QStringList rereadMissingRequiredSections{};
                        rereadMissingRequiredSections.reserve(
                            static_cast<qsizetype>(
                                recovered.missingRequiredSections.size()));
                        for (const std::string& section
                            : recovered.missingRequiredSections)
                        {
                            rereadMissingRequiredSections.push_back(
                                QString::fromStdString(section));
                        }
                        rereadMissingRequiredSections =
                            ui::recovery::lostRequiredLevelSections(
                                rereadMissingRequiredSections);
                        const bool missingClassificationMatches =
                            rereadMissingRequiredSections
                                == acceptedMissingRequiredSections
                            && recovered.unidentifiedMissingSections
                                == acceptedUnidentifiedMissingSections;
                        const bool projectClassificationMatches =
                            acceptedUserProjectChoice
                                ? !recovered.projectTypeTrusted
                                    && !recovered.projectType.has_value()
                                : recovered.projectTypeTrusted
                                    && recovered.projectType.has_value()
                                    && *recovered.projectType
                                        == acceptedProjectType;
                        if (!missingClassificationMatches
                            || !projectClassificationMatches)
                        {
                            return sourceChangedFailure(QStringLiteral(
                                "checked semantic loss or project classification no longer matches"));
                        }
                        if (acceptedMissingRequiredSections.contains(
                                QStringLiteral("head")))
                        {
                            // Reapply the same safe project/header defaults to
                            // the worker's lost-HEAD Level, whether identity
                            // came from checked framing or an explicit choice.
                            // Never export using a zeroed header or synthesized
                            // JKDF2 placeholder.
                            ui::project::applyNewProjectTypeDefaults(
                                recovered.level, acceptedProjectType);
                        }
                        return ui::recovery::writePartialLevelSalvage(
                            recovered.level, salvageContext);
                    }
                    catch (const std::exception& exception)
                    {
                        ui::recovery::PartialLevelSalvageResult failure{};
                        failure.error = QStringLiteral(
                            "Background direct-SED partial-Level salvage failed safely: %1")
                            .arg(QString::fromUtf8(exception.what()));
                        return failure;
                    }
                    catch (...)
                    {
                        ui::recovery::PartialLevelSalvageResult failure{};
                        failure.error = QStringLiteral(
                            "Background direct-SED partial-Level salvage failed safely after an unknown error.");
                        return failure;
                    }
                }));
        }

        const QString loadedStatus = loadResult.sedDetachFromSource
            ? QStringLiteral(
                "Opened partial recovery from %1 as Untitled - %2 sectors, %3 things, %4 lights")
            : QStringLiteral(
                "Loaded %1 - %2 sectors, %3 things, %4 lights");
        statusBar()->showMessage(
            loadedStatus
                .arg(filename)
                .arg(document_.level().sectors.size())
                .arg(document_.level().things.size())
                .arg(document_.level().lights.size()));

        rememberFileDialogSelection(
            FileDialogHistoryGroup::openSave, filename);

        if (!loadResult.sedDetachFromSource)
        {
            rememberRecentLevelFile(filename);
        }

        // Only a fully committed Open may retire work that the user chose to
        // discard or the stale target recovery they chose to bypass.  A
        // failed Open exits through the catch below without either mutation.
        std::vector<std::pair<QString, QString>> familiesToRetire{};
        if (!deferredCurrentDiscardIdentity.isEmpty())
        {
            familiesToRetire.emplace_back(
                deferredCurrentDiscardIdentity,
                deferredCurrentDiscardSessionId);
        }
        if (recoveryOffer == RecoveryOfferResult::openSaved
            && !selectedRecoveryIdentity.isEmpty()
            && std::find(familiesToRetire.begin(), familiesToRetire.end(),
                   std::pair{ selectedRecoveryIdentity,
                       selectedRecoverySessionId }) == familiesToRetire.end())
        {
            familiesToRetire.emplace_back(
                selectedRecoveryIdentity, selectedRecoverySessionId);
        }
        bool retiredCurrentFamily = false;
        for (const auto& [identity, sessionId] : familiesToRetire)
        {
            retiredCurrentFamily = retiredCurrentFamily
                || (identity == recoveryIdentity_
                    && sessionId == recoverySessionId_);
            discardRecoveryFamily(identity, sessionId);
        }

        // A clean document, or one changed only in EDIT (camera/UV/layout),
        // does not necessarily show the Level Save/Discard prompt. Its old
        // live slot is still an automatic recovery offer and must be retired
        // after this intentional replacement commits. `.sedautosave` history
        // remains durable regardless of whether the replacement used Save or
        // Discard; only the explicit Autosave History Delete action or writer
        // retention pruning removes those entries.
        bool retiredCurrentLiveFamily = false;
        std::vector<std::pair<QString, QString>> liveFamiliesToRetire{
            { recoveryIdentityBeforeReplacementPrompt,
                recoverySessionBeforeReplacementPrompt },
            { recoveryIdentityAfterReplacementPrompt,
                recoverySessionAfterReplacementPrompt },
        };
        std::sort(liveFamiliesToRetire.begin(), liveFamiliesToRetire.end());
        liveFamiliesToRetire.erase(std::unique(
            liveFamiliesToRetire.begin(), liveFamiliesToRetire.end()),
            liveFamiliesToRetire.end());
        for (const auto& [identity, sessionId] : liveFamiliesToRetire)
        {
            if (identity.trimmed().isEmpty() || sessionId.trimmed().isEmpty()
                || std::find(familiesToRetire.begin(), familiesToRetire.end(),
                    std::pair{ identity, sessionId }) != familiesToRetire.end())
            {
                continue;
            }
            retiredCurrentLiveFamily = retiredCurrentLiveFamily
                || (identity == recoveryIdentity_
                    && sessionId == recoverySessionId_);
            discardRecoveryLiveFamily(identity, sessionId);
        }
        if (retiredCurrentFamily || retiredCurrentLiveFamily)
        {
            // Cleanup invalidated the same session/path now owned by the
            // freshly opened document. Recreate its clean hard-kill baseline
            // under the new epoch.
            recoverySnapshotRequired_ = true;
            scheduleRecoverySnapshot(true);
        }
        std::vector<std::filesystem::path> supersededLivePaths{
            recoveryPathBeforeReplacementPrompt,
            recoveryPathAfterReplacementPrompt,
        };
        std::sort(supersededLivePaths.begin(), supersededLivePaths.end());
        supersededLivePaths.erase(std::unique(
            supersededLivePaths.begin(), supersededLivePaths.end()),
            supersededLivePaths.end());
        for (const std::filesystem::path& supersededPath : supersededLivePaths)
        {
            if (supersededPath.empty()
                || ui::recovery::recoveryPathsEqual(
                    supersededPath, currentRecoveryPath_))
            {
                continue;
            }
            QString retirementError{};
            if (!ui::recovery::invalidateAndRemoveRecovery(
                    recoveryPublicationGate_,
                    supersededPath,
                    &retirementError)
                && !retirementError.isEmpty())
            {
                logMessage(MessageSeverity::warning, retirementError);
            }
        }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        if (directOpenBeforeViewportRefitForTest_)
        {
            directOpenBeforeViewportRefitForTest_();
        }
#endif
    }
    catch (const std::exception& e)
    {
        const QString error = QString::fromUtf8(e.what());
        logMessage(MessageSeverity::error,
            QStringLiteral("Open failed: %1").arg(error));
        showExplicitlyLoggedMessageBox(this, QMessageBox::Critical,
            QStringLiteral("Open failed"), error);
    }
}

void MainWindow::createNewProject(const ProjectType kind)
{
    QString deferredDiscardIdentity{};
    QString deferredDiscardSessionId{};
    if (!authorizeDocumentReplacement(
            QStringLiteral("creating a new project"), {},
            &deferredDiscardIdentity,
            &deferredDiscardSessionId))
    {
        return;
    }
    const auto replacementFingerprintGuard = qScopeGuard(
        [this]() { clearReplacementAuthorizationFingerprint(); });
    const std::filesystem::path previousRecoveryPath = currentRecoveryPath_;
    const QString previousRecoveryIdentity = recoveryIdentity_;
    const QString previousRecoverySessionId = recoverySessionId_;

    editor::DocumentState state{};
    ui::project::applyNewProjectTypeDefaults(state.level, kind);
    level::UvTextureDimensions starterMaterialDimensions =
        ui::project::DefaultStarterMaterialDimensions;
    const StagedProjectResourceState starterResources =
        stageProjectResources(kind, {});
    if (const std::optional<level::UvTextureDimensions> resolved =
            assets::resolveMaterialTextureDimensions(
                starterResources.locator,
                "dflt.mat"))
    {
        starterMaterialDimensions = *resolved;
    }
    ui::project::seedDefaultNewProjectLevel(
        state.level,
        starterMaterialDimensions);
    {
        const QSettings settings = settingsStore();
        state.level.exportBufferExtras =
            ui::project::configuredNewProjectExportBufferExtras(settings, kind);
    }

    if (!replacementAuthorizationStillCurrent())
    {
        const QString warning = QStringLiteral(
            "The current workspace changed while the new project was being prepared; it was not replaced.");
        logMessage(MessageSeverity::warning, warning);
        statusBar()->showMessage(warning);
        return;
    }

    if (thingPlacementBrushStrokeActive_)
    {
        (void)finalizeThingPlacementBrushStroke();
    }
    const ProjectType previousProjectType = document_.level().kind;
    // Reset old-document interaction before replacing the document.  This is
    // deliberately before clear(): the selection/preview state may refer to
    // indices in the old level and must never be interpreted against the new
    // level.
    resetSelectionContextState();
    closeResourceEditorsAfterReplacement();
    closeCogScriptEditorsAfterReplacement();
    document_.clear(std::move(state));
    viewportSoundSector_ = -1;
    const std::int64_t timestamp = std::max<std::int64_t>(
        1, QDateTime::currentMSecsSinceEpoch());
    nativeDocumentTimestamp_ = timestamp;
    resetLightBakeSourceTracking();
    resetLayerColorsToDefaults();
    resetLayerStatesFromLevel(false);
    currentPath_.clear();
    nativeSourceUsesLegacyUvFormat_ = false;
    lastEngineExportPath_.clear();
    switchRecoveryDocument(currentPath_, true, false);

    loadResourceSettings(kind);
    rebuildResourceLocator({});
    loadGlobalTemplates();

    if (!document_.level().things.empty())
    {
        Thing& player = document_.level().things.front();
        const assets::Templates* templatesDb = (templates_.size() > 0) ? &templates_ : nullptr;
        player.z = calcThingInsertOffsetZ(document_.level(), resources_, templatesDb, player.templateName);
        player.sector = sed::level::levelFindSectorForXYZ(document_.level(), player.x, player.y, player.z);
    }

    glView_->setResourceLocator(&resources_);
    glView_->setLevel(&document_.level());
    // A new project starts with the canonical editor grid.  Do this
    // explicitly because grid alignment/center is intentionally persisted in
    // SED/recovery state but is not part of a newly seeded Level.
    glView_->resetGridOverrides();

    glView_->setRenderPipeline(GlView::RenderPipeline::Editor);
    glView_->resetPreviewViewportState();
    if (previousProjectType != kind)
    {
        applyArchThingGizmoPreferences();
    }
    glView_->setLightingMode(GlView::LightingMode::Gouraud);
    glView_->setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
    glView_->resetFlySpeedMultiplier();
    glView_->setShowWireframe(true);
    glView_->setShowVertices(false);
    glView_->setShowSolid(true);
    glView_->setShowTextures(true);
    glView_->setShowFog(glView_->editorDefaultShowFog());
    glView_->setShowThingCollisionShapes(
        glView_->editorDefaultShowThingCollisionShapes());
    glView_->setShowThingVisualBounds(
        glView_->editorDefaultShowThingVisualBounds());
    glView_->setShowThings(true);
    glView_->setShowThingGizmos(glView_->editorDefaultShowThingGizmos());
    glView_->setThingViewMode(GlView::ThingViewMode::Shaded);
    glView_->setFocus(Qt::OtherFocusReason);
    glView_->resetEditorTopDownView();
    glView_->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);

    setPlacementMode(PlacementMode::none);
    setSurfaceSelectionMode(SurfaceSelectionMode::face);
    setEditMode(EditMode::none);
    if (uvEditorPanelHost_)
    {
        uvEditorPanelHost_->hide();
    }
    if (outlinerTabs_)
    {
        outlinerTabs_->setCurrentIndex(0);
    }
    scheduleEditorViewRefitAfterLayout();

    if (viewportGamePreviewAction_)
    {
        viewportGamePreviewAction_->setChecked(false);
    }
    if (viewportGouraudAction_)
    {
        viewportGouraudAction_->setChecked(true);
    }

    rebuildOutliner();
    rebuildProperties();
    syncCogScriptWatches();
    rebuildChecks();
    restoreStandaloneOutlinerDockWidth();
    markCurrentDocumentAsSaved();
    // A new project has no on-disk baseline even though its initial seed has no
    // undo record. Keep it save/recovery-dirty until the first explicit Save.
    recoveryRequiresSave_ = true;
    resetLegacyUvCompatibilityStatus();
    evaluateLegacyUvCompatibility(false, false);
    refreshWindowTitle();
    updateEditActions();
    setViewportTransformActionChecks(nullptr);
    rememberedViewportTransformAction_ = nullptr;
    if (glView_)
    {
        glView_->setTransformTool(GlView::TransformTool::Select);
        glView_->setTransformInteractionEnabled(false);
    }
    syncViewportSelectionInputEnabled();
    markRecoveryWorkspaceAsSaved();

    if (!deferredDiscardIdentity.isEmpty())
    {
        discardRecoveryFamily(
            deferredDiscardIdentity, deferredDiscardSessionId);
    }
    else
    {
        // New Project is an intentional successful replacement even when the
        // prior Level was clean or changed only in EDIT. Remove every live
        // automatic offer for that exact old session, while retaining its
        // periodic Autosave History entries.
        discardRecoveryLiveFamily(
            previousRecoveryIdentity, previousRecoverySessionId);
    }

    // The direct gated removal also covers a malformed old descriptor that
    // could not participate in the bounded metadata scan and closes the
    // check/commit race for both explicit Discard and clean/EDIT-only
    // replacement. It must run even when the path is absent at this instant:
    // advancing the generation is what prevents an old worker from recreating
    // the file immediately afterward.
    QString retirementError{};
    if (!previousRecoveryPath.empty()
        && !ui::recovery::recoveryPathsEqual(
            previousRecoveryPath, currentRecoveryPath_)
        && !ui::recovery::invalidateAndRemoveRecovery(
            recoveryPublicationGate_, previousRecoveryPath, &retirementError)
        && !retirementError.isEmpty())
    {
        logMessage(MessageSeverity::warning, retirementError);
    }
    // The exact-path invalidation advances the shared writer generation after
    // the new Untitled document was installed. Requeue its required baseline
    // explicitly so cancelling the old publisher cannot also cancel the first
    // durable checkpoint for the new project.
    recoverySnapshotRequired_ = true;
    scheduleRecoverySnapshot(true);

    statusBar()->showMessage(QString("New project: %1").arg(ui::project::projectTypeLabel(kind)));
}

bool MainWindow::configureProjectInstallDir(const ProjectType kind, QWidget* parent)
{
    std::error_code cwdEc;
    const auto configured = readProjectInstallDirSetting(kind);
    const auto detected = tryGetProjectInstallDir(kind);
    const std::filesystem::path initialDir =
        configured.value_or(detected.value_or(std::filesystem::current_path(cwdEc)));

    const QString selectedDir = QFileDialog::getExistingDirectory(
        parent ? parent : this,
        QString("Configure %1").arg(ui::project::projectTypeLabel(kind)),
        QString::fromStdWString(initialDir.wstring()));
    if (selectedDir.isEmpty())
    {
        return false;
    }

    const std::filesystem::path installDir(selectedDir.toStdWString());
    const ui::resources::ProjectInstallPathValidation validation =
        ui::resources::validateProjectInstallPath(kind, installDir);
    if (!validation.canUse())
    {
        const QString message =
            ui::project::projectInstallPathMismatchMessage(
                kind, installDir, validation);
        logMessage(
            MessageSeverity::warning,
            QStringLiteral("Project path was not configured: %1")
                .arg(message));
        showExplicitlyLoggedMessageBox(
            parent ? parent : this,
            QMessageBox::Warning,
            QStringLiteral("Project Path Does Not Match"),
            message);
        return false;
    }

    writeProjectInstallDirSetting(kind, installDir);
    writeResourceSettings(kind, ui::resources::findDefaultProjectResources(kind, installDir));

    if (resourceKind_ == kind)
    {
        loadResourceSettings(kind);
        rebuildResourceLocator(currentPath_);
        loadGlobalTemplates();
        if (glView_)
        {
            glView_->setResourceLocator(&resources_);
        }
    }

    statusBar()->showMessage(QString("%1 configured: %2").arg(ui::project::projectTypeLabel(kind), QDir::toNativeSeparators(selectedDir)), 5000);
    return true;
}

bool MainWindow::showProjectStartDialog()
{
    if (!currentPath_.empty()
        || !document_.level().sectors.empty()
        || !document_.level().things.empty()
        || !document_.level().lights.empty()
        || !document_.level().cogs.empty())
    {
        return true;
    }

    QDialog dialog(nullptr);
    dialog.setObjectName("startup_dialog");
    // Opt out of the global native-dialog policy so it cannot replace the
    // startup window's deliberately minimal client-drawn chrome during Polish.
    dialog.setProperty("sed.allowMinMaxButtons", true);
    const QIcon startupBrandIcon = loadApplicationIcon();
    dialog.setWindowTitle(QStringLiteral("SED - Sith Editor"));
    dialog.setWindowIcon(startupBrandIcon);
    dialog.setModal(true);
    dialog.setWindowFlags(
        Qt::Window | Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_TranslucentBackground, true);
    const QScreen* startupScreen = QGuiApplication::screenAt(QCursor::pos());
    if (!startupScreen)
    {
        startupScreen = QGuiApplication::primaryScreen();
    }
    if (startupScreen)
    {
        const QRect startupGeometry =
            ui::project::startupDialogGeometryForAvailableScreen(
                startupScreen->availableGeometry());
        dialog.setMinimumSize(
            std::min(1040, startupGeometry.width()),
            std::min(640, startupGeometry.height()));
        dialog.setGeometry(startupGeometry);
    }
    else
    {
        dialog.resize(1180, 720);
        dialog.setMinimumSize(1040, 640);
    }
    dialog.setStyleSheet(
        "QDialog#startup_dialog {"
        " background-color: transparent;"
        " color: #f2f2f2;"
        "}"
        "QFrame#startup_canvas {"
        " background-color: #252525;"
        " border: 1px solid #454545;"
        " border-radius: 12px;"
        " color: #f2f2f2;"
        "}"
        "QWidget#startup_title_bar { background-color: transparent; }"
        "QLabel { color: #f2f2f2; }"
        "QLabel#startup_brand { font-size: 26px; font-weight: 650; }"
        "QLabel#startup_section { font-size: 18px; font-weight: 650; }"
        "QLabel#startup_hint, QLabel#startup_recent_path { color: #b8b8b8; }"
        "QLabel#startup_status_ok { color: #9edb9e; }"
        "QLabel#startup_status_warn { color: #efd27a; }"
        "QLabel#startup_status_bad { color: #ef8d8d; }"
        "QLineEdit#startup_recent_search {"
        " background-color: #202020;"
        " border: 1px solid #404040;"
        " border-radius: 6px;"
        " color: #f2f2f2;"
        " padding: 7px 10px;"
        " selection-background-color: #315a83;"
        "}"
        "QListWidget#startup_recent_list {"
        " background-color: #202020;"
        " border: 1px solid #3a3a3a;"
        " border-radius: 0px;"
        " outline: 0;"
        "}"
        "QListWidget#startup_recent_list::item {"
        " border: 0px;"
        " padding: 2px;"
        "}"
        "QListWidget#startup_recent_list::item:selected {"
        " background-color: #353f4b;"
        "}"
        "QFrame#startup_recent_row { background-color: transparent; }"
        "QToolButton#startup_pin_button {"
        " background-color: transparent;"
        " border: 0px;"
        " border-radius: 4px;"
        " padding: 3px;"
        "}"
        "QToolButton#startup_pin_button:hover { background-color: #3a414a; }"
        "QToolButton#startup_pin_button:checked { background-color: transparent; }"
        "QToolButton#startup_pin_button:checked:hover { background-color: #3a414a; }"
        "QToolButton#startup_close_button {"
        " background-color: transparent;"
        " border: 0px;"
        " border-radius: 4px;"
        " color: #f2f2f2;"
        " font-size: 22px;"
        " font-weight: 400;"
        " padding: 0px;"
        "}"
        "QToolButton#startup_close_button:hover { background-color: #c42b1c; color: #ffffff; }"
        "QToolButton#startup_close_button:pressed { background-color: #9f2117; color: #ffffff; }"
        "QFrame#startup_project_card {"
        " background-color: #2b2b2b;"
        " border: 1px solid #3a3a3a;"
        " border-radius: 6px;"
        "}"
        "QPushButton#startup_action_button {"
        " background-color: transparent;"
        " border: 0px;"
        " border-radius: 4px;"
        " color: #f2f2f2;"
        " text-align: left;"
        " padding: 8px 10px;"
        "}"
        "QPushButton#startup_action_button:hover { background-color: #333a42; }"
        "QPushButton#startup_primary_button {"
        " background-color: #33383f;"
        " border: 1px solid #597083;"
        " border-radius: 5px;"
        " color: #ffffff;"
        " font-weight: 650;"
        " text-align: left;"
        " padding: 10px 12px;"
        "}"
        "QPushButton#startup_primary_button:hover { background-color: #3d4650; border-color: #6f8fa6; }"
        "QPushButton#startup_project_button {"
        " background-color: #383838;"
        " border: 1px solid #4a4a4a;"
        " border-radius: 4px;"
        " color: #f2f2f2;"
        " padding: 6px 10px;"
        "}"
        "QPushButton#startup_project_button:hover { background-color: #454545; }"
        "QPushButton#startup_project_button:disabled { color: #808080; background-color: #303030; }"
        "QPushButton#startup_footer_button {"
        " background-color: transparent;"
        " border: 1px solid #454545;"
        " border-radius: 4px;"
        " color: #f2f2f2;"
        " padding: 7px 14px;"
        "}"
        "QPushButton#startup_footer_button:hover { background-color: #333a42; }");

    auto* dialogLayout = new QVBoxLayout(&dialog);
    dialogLayout->setContentsMargins(0, 0, 0, 0);
    dialogLayout->setSpacing(0);

    auto* startupCanvas = new QFrame(&dialog);
    startupCanvas->setObjectName("startup_canvas");
    startupCanvas->setAttribute(Qt::WA_StyledBackground, true);
    dialogLayout->addWidget(startupCanvas);

    auto* root = new QVBoxLayout(startupCanvas);
    root->setContentsMargins(10, 0, 0, 14);
    root->setSpacing(12);

    auto* titleBar = new StartupTitleBar(&dialog);
    titleBar->setObjectName("startup_title_bar");
    titleBar->setFixedHeight(52);
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(0, 0, 4, 0);
    titleLayout->setSpacing(8);

    auto* brandIcon = new QLabel(titleBar);
    brandIcon->setObjectName("startup_brand_icon");
    const QPixmap brandPixmap = startupBrandIcon.pixmap(QSize(48, 48)).scaled(
        QSize(34, 34),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    brandIcon->setPixmap(brandPixmap);
    brandIcon->setFixedSize(38, 38);
    brandIcon->setAlignment(Qt::AlignCenter);
    brandIcon->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    titleLayout->addWidget(brandIcon);

    auto* brand = new QLabel("SED - Sith Editor", titleBar);
    brand->setObjectName("startup_brand");
    brand->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    titleLayout->addWidget(brand);
    titleLayout->addStretch(1);

    auto* closeButton = new QToolButton(titleBar);
    closeButton->setObjectName("startup_close_button");
    closeButton->setText(QStringLiteral("\u00d7"));
    closeButton->setToolTip(QStringLiteral("Close"));
    closeButton->setAccessibleName(QStringLiteral("Close startup window"));
    closeButton->setAutoRaise(true);
    closeButton->setCursor(Qt::ArrowCursor);
    closeButton->setFixedSize(46, 40);
    titleLayout->addWidget(closeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    connect(closeButton, &QToolButton::clicked, &dialog, &QDialog::reject);

    root->addWidget(titleBar);

    auto* body = new QHBoxLayout();
    body->setContentsMargins(0, 0, 16, 0);
    body->setSpacing(28);
    root->addLayout(body, 1);

    auto* leftPane = new QWidget(&dialog);
    auto* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(12);
    body->addWidget(leftPane, 1);

    auto* getStarted = new QLabel("Get started", leftPane);
    getStarted->setObjectName("startup_section");
    leftLayout->addWidget(getStarted);

    auto* recentSearch = new QLineEdit(leftPane);
    recentSearch->setObjectName("startup_recent_search");
    recentSearch->setPlaceholderText("Search recent levels");
    recentSearch->setClearButtonEnabled(true);
    leftLayout->addWidget(recentSearch);

    auto* recentList = new QListWidget(leftPane);
    recentList->setObjectName("startup_recent_list");
    recentList->setSelectionMode(QAbstractItemView::SingleSelection);
    recentList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    recentList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    recentList->setFrameShape(QFrame::NoFrame);
    leftLayout->addWidget(recentList, 1);

    auto* emptyRecentLabel = new QLabel("No recent levels yet.", leftPane);
    emptyRecentLabel->setObjectName("startup_hint");
    emptyRecentLabel->setAlignment(Qt::AlignCenter);
    emptyRecentLabel->setMinimumHeight(28);
    emptyRecentLabel->hide();
    leftLayout->addWidget(emptyRecentLabel);

    auto* rightPane = new QWidget(&dialog);
    rightPane->setMinimumWidth(330);
    rightPane->setMaximumWidth(390);
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 42, 0, 0);
    rightLayout->setSpacing(12);
    body->addWidget(rightPane);

    auto showMainWindowForProject = [this]()
    {
        if (!isVisible())
        {
            show();
        }
        raise();
        activateWindow();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };

    auto* openLevelButton = new QPushButton("Open...", rightPane);
    openLevelButton->setObjectName("startup_primary_button");
    const int openIconSize = 20;
    openLevelButton->setIcon(makeFlatFolderIcon(QColor(235, 245, 255), openIconSize, openLevelButton->devicePixelRatioF()));
    openLevelButton->setIconSize(QSize(openIconSize, openIconSize));
    openLevelButton->setToolTip("Open an existing JKL, NDY, or legacy JED file");
    openLevelButton->setMinimumHeight(44);
    rightLayout->addWidget(openLevelButton);

    auto* projectSection = new QLabel("Create a new project", rightPane);
    projectSection->setObjectName("startup_section");
    rightLayout->addSpacing(18);
    rightLayout->addWidget(projectSection);

    struct ProjectRowWidgets
    {
        ProjectType kind{};
        QLabel* status{};
        QPushButton* create{};
        QPushButton* configure{};
    };

    std::vector<ProjectRowWidgets> rows{};
    rows.reserve(3);

    for (const ProjectType kind : std::array<ProjectType, 3>{ ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM })
    {
        auto* card = new QFrame(rightPane);
        card->setObjectName("startup_project_card");

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(8);

        auto* projectLabel = new QLabel(ui::project::projectTypeLabel(kind), card);
        QFont projectFont = projectLabel->font();
        projectFont.setBold(true);
        projectLabel->setFont(projectFont);
        auto* status = new QLabel(card);
        status->setWordWrap(true);
        cardLayout->addWidget(projectLabel);
        cardLayout->addWidget(status);

        auto* buttonCol = new QHBoxLayout();
        buttonCol->setSpacing(8);
        auto* create = new QPushButton("New", card);
        create->setObjectName("startup_project_button");
        create->setMinimumHeight(30);
        auto* configure = new QPushButton("Configure...", card);
        configure->setObjectName("startup_project_button");
        configure->setMinimumHeight(30);
        buttonCol->addWidget(create, 1);
        buttonCol->addWidget(configure, 1);
        cardLayout->addLayout(buttonCol);

        rightLayout->addWidget(card);
        rows.push_back(ProjectRowWidgets{ kind, status, create, configure });
    }
    rightLayout->addStretch(1);

    auto refreshRows = [&rows]()
    {
        for (ProjectRowWidgets& row : rows)
        {
            const ui::project::ProjectInstallStatus status = currentProjectInstallStatus(row.kind);

            row.status->setObjectName(status.statusObjectName());
            row.status->style()->unpolish(row.status);
            row.status->style()->polish(row.status);
            row.status->setText(status.statusText());
            row.create->setEnabled(status.canCreateProject());
        }
    };

    struct RecentRow
    {
        QListWidgetItem* item{};
        QString path{};
        QString searchable{};
    };

    std::vector<RecentRow> recentRows{};
    QHash<QString, std::optional<ProjectType>> recentProjectKindCache;

    auto cachedRecentProjectKind = [&recentProjectKindCache](const QString& path) -> std::optional<ProjectType>
    {
        auto it = recentProjectKindCache.constFind(path);
        if (it != recentProjectKindCache.constEnd())
        {
            return it.value();
        }

        const std::optional<ProjectType> projectKind = inferRecentLevelProjectType(path);
        recentProjectKindCache.insert(path, projectKind);
        return projectKind;
    };

    std::function<void()> populateRecentLevels;
    std::function<void()> updateRecentFilter;

    populateRecentLevels = [&]()
    {
        recentList->clear();
        recentRows.clear();

        const QStringList storedRecentLevels = loadRecentLevels();
        QStringList lastUsedRecentLevels{};
        for (const QString& path : storedRecentLevels)
        {
            const QFileInfo info(path.trimmed());
            if (!path.trimmed().isEmpty() && info.exists() && info.isFile())
            {
                lastUsedRecentLevels.push_back(path);
            }
        }
        if (lastUsedRecentLevels != storedRecentLevels)
        {
            saveRecentLevels(lastUsedRecentLevels);
        }
        const QStringList loadedPinnedLevels = loadPinnedRecentLevels();
        const QStringList pinnedLevels = ui::project::sanitizedPinnedRecentLevels(lastUsedRecentLevels, loadedPinnedLevels);
        if (pinnedLevels != loadedPinnedLevels)
        {
            savePinnedRecentLevels(pinnedLevels);
        }
        const QStringList recentLevels = ui::project::recentLevelsWithPinnedFirst(lastUsedRecentLevels, pinnedLevels);
        for (const QString& path : recentLevels)
        {
            const QString trimmed = path.trimmed();
            if (trimmed.isEmpty())
            {
                continue;
            }

            const QFileInfo info(trimmed);
            const QString title = info.fileName().isEmpty() ? trimmed : info.fileName();
            const QString folder = QDir::toNativeSeparators(info.absolutePath());

            auto* item = new QListWidgetItem(recentList);
            item->setData(Qt::UserRole, trimmed);
            item->setSizeHint(QSize(0, 64));

            auto* row = new QFrame(recentList);
            row->setObjectName("startup_recent_row");
            row->setAttribute(Qt::WA_Hover, true);
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(10, 6, 10, 6);
            rowLayout->setSpacing(10);

            const std::optional<ProjectType> projectKind = cachedRecentProjectKind(trimmed);

            auto* icon = new QLabel(row);
            icon->setObjectName("startup_hint");
            icon->setPixmap(makeRecentFileIcon(trimmed, projectKind));
            icon->setToolTip(recentFileFormatTooltip(trimmed, projectKind));
            icon->setFixedWidth(50);
            icon->setAlignment(Qt::AlignCenter);
            rowLayout->addWidget(icon);

            auto* textLayout = new QVBoxLayout();
            textLayout->setContentsMargins(0, 0, 0, 0);
            textLayout->setSpacing(2);
            auto* titleLabel = new QLabel(title, row);
            titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            auto* pathLabel = new QLabel(folder, row);
            pathLabel->setObjectName("startup_recent_path");
            pathLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
            pathLabel->setTextInteractionFlags(Qt::NoTextInteraction);
            textLayout->addWidget(titleLabel);
            textLayout->addWidget(pathLabel);
            rowLayout->addLayout(textLayout, 1);

            const bool pinned = ui::project::recentLevelListContainsPath(pinnedLevels, trimmed);
            auto* pinButton = new QToolButton(row);
            pinButton->setObjectName("startup_pin_button");
            pinButton->setAutoRaise(true);
            pinButton->setCheckable(true);
            pinButton->setChecked(pinned);
            pinButton->setIcon(makeStartupPinIcon(pinned));
            pinButton->setIconSize(QSize(20, 20));
            pinButton->setToolTip(pinned ? "Unpin from top" : "Pin to top");
            pinButton->setFixedSize(28, 28);
            pinButton->setAttribute(Qt::WA_Hover, true);
            rowLayout->addWidget(pinButton);
            auto* pinVisibility = new RecentPinVisibilityFilter(row, pinButton, pinned);
            pinVisibility->watch(row);
            pinVisibility->watch(icon);
            pinVisibility->watch(titleLabel);
            pinVisibility->watch(pathLabel);
            pinVisibility->watch(pinButton);
            connect(pinButton, &QToolButton::clicked, &dialog, [&, pathToPin = trimmed](const bool checked)
                {
                    const QStringList pins = ui::project::pinnedRecentLevelsAfterToggle(
                        loadRecentLevels(),
                        loadPinnedRecentLevels(),
                        pathToPin,
                        checked);
                    savePinnedRecentLevels(pins);
                    populateRecentLevels();
                    if (updateRecentFilter)
                    {
                        updateRecentFilter();
                    }
                });

            recentList->setItemWidget(item, row);
            QString searchable = title + '\n' + folder + '\n' + trimmed;
            if (projectKind)
            {
                searchable += '\n' + shortProjectTypeLabel(*projectKind);
            }
            recentRows.push_back(RecentRow{ item, trimmed, searchable });
        }

        emptyRecentLabel->setVisible(recentRows.empty());
    };

    updateRecentFilter = [&]()
    {
        const QString needle = recentSearch->text().trimmed();
        bool anyVisible = false;
        for (RecentRow& row : recentRows)
        {
            const bool visible = needle.isEmpty() || row.searchable.contains(needle, Qt::CaseInsensitive);
            if (row.item)
            {
                row.item->setHidden(!visible);
            }
            anyVisible = anyVisible || visible;
        }

        emptyRecentLabel->setText(recentRows.empty() ? "No recent levels yet." : "No recent levels match the search.");
        emptyRecentLabel->setVisible(!anyVisible);
    };

    auto openRecentLevel = [&](QListWidgetItem* item)
    {
        if (!item)
        {
            return;
        }
        const QString filename = item->data(Qt::UserRole).toString().trimmed();
        if (filename.isEmpty())
        {
            return;
        }

        const QFileInfo info(filename);
        if (!info.exists() || !info.isFile())
        {
            forgetRecentLevelFile(filename);
            populateRecentLevels();
            updateRecentFilter();
            QMessageBox::warning(
                &dialog,
                QStringLiteral("File not found"),
                QStringLiteral("The file does not exist and was removed from Recent Files:\n%1")
                    .arg(QDir::toNativeSeparators(info.absoluteFilePath())));
            return;
        }

        openLevelFromFilename(filename, [&dialog, showMainWindowForProject]()
            {
                dialog.accept();
                showMainWindowForProject();
            });
    };

    connect(recentSearch, &QLineEdit::textChanged, &dialog, [&updateRecentFilter](const QString&)
        {
            if (updateRecentFilter)
            {
                updateRecentFilter();
            }
    });
    connect(recentList, &QListWidget::itemActivated, &dialog, openRecentLevel);
    // QListWidget::itemActivated already covers both keyboard activation and
    // double-click on desktop platforms. Connecting itemDoubleClicked as well
    // ran the complete Open transaction twice for one startup-dialog gesture,
    // producing a second Parsing/Applying/Finalizing sequence and a late
    // autosave offer against the document that had just finished loading.

    for (ProjectRowWidgets& row : rows)
    {
        connect(row.create, &QPushButton::clicked, &dialog, [this, &dialog, showMainWindowForProject, kind = row.kind]()
            {
                showMainWindowForProject();
                dialog.accept();
                createNewProject(kind);
            });
        connect(row.configure, &QPushButton::clicked, &dialog, [this, &dialog, &refreshRows, kind = row.kind]()
            {
                if (configureProjectInstallDir(kind, &dialog))
                {
                    refreshRows();
                }
            });
    }

    connect(openLevelButton, &QPushButton::clicked, &dialog, [this, &dialog, showMainWindowForProject]()
        {
            const QString startDir = fileDialogStartLocation(
                FileDialogHistoryGroup::openSave);
            const QString filename =
                QFileDialog::getOpenFileName(
                    &dialog,
                    "Open level",
                    startDir,
                    ui::project::levelOpenDialogFilter());
            if (filename.isEmpty())
            {
                return;
            }

            openLevelFromFilename(filename, [&dialog, showMainWindowForProject]()
                {
                    dialog.accept();
                    showMainWindowForProject();
                });
        });

    populateRecentLevels();
    refreshRows();
    updateRecentFilter();

    auto* footer = new QHBoxLayout();
    footer->setContentsMargins(0, 0, 16, 0);
    footer->addStretch(1);
    auto* preferencesButton = new QPushButton("Preferences...", &dialog);
    preferencesButton->setObjectName("startup_footer_button");
    preferencesButton->setIcon(makeStartupGearIcon());
    preferencesButton->setIconSize(QSize(18, 18));
    preferencesButton->setMinimumWidth(132);
    footer->addWidget(preferencesButton);
    connect(preferencesButton, &QPushButton::clicked, &dialog, [this, &dialog, &refreshRows]()
        {
            showPreferencesDialog(&dialog);
            refreshRows();
        });
    root->addLayout(footer);

    // Ensure the frameless top-level still exposes the executable icon to the
    // Windows taskbar once its native handle exists.
    QTimer::singleShot(0, &dialog, [&dialog]()
        {
            applyWindowsTaskbarIcon(dialog);
        });

    return dialog.exec() == QDialog::Accepted;
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
double calcThingInsertOffsetZForTest(const Level& level,
    const assets::ResourceLocator& resources, const std::string_view name)
{
    return calcThingInsertOffsetZ(level, resources, nullptr, name);
}
#endif

} // namespace sed
