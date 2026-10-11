#include "app_logging.hpp"
#include "main_window/common.hpp"
#include "ui/shortcuts/key_sequence_display.hpp"
#include "ui/components/path_display_utils.hpp"
#include "ui/project/project_startup_utils.hpp"
#include "ui/project/sed_editor_file_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"
#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/resources/project_resource_settings.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "main_window/menus/engine_export_dialog_utils.hpp"
#include "main_window/document/native_save_edit_utils.hpp"

#include "sed/assets/sed.hpp"
#include "sed/assets/material_dimensions.hpp"
#include "sed/editor/uv_promotion.hpp"

#include <QSaveFile>
#include <QCryptographicHash>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QGridLayout>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <streambuf>

namespace sed
{

#include "main_window/detail/helpers.hpp"

namespace
{

void showExplicitlyLoggedCriticalMessage(
    QWidget* parent,
    const QString& title,
    const QString& text)
{
    QMessageBox message(
        QMessageBox::Critical, title, text, QMessageBox::Ok, parent);
    message.setProperty(kMessageBoxLoggingSkipProperty, true);
    (void)message.exec();
}

class QSaveFileStreamBuffer final : public std::streambuf
{
public:
    explicit QSaveFileStreamBuffer(QSaveFile& file)
        : file_(file)
    {
        setp(buffer_.data(), buffer_.data() + buffer_.size());
    }

protected:
    int sync() override
    {
        return flushBuffer() ? 0 : -1;
    }

    int_type overflow(const int_type value) override
    {
        if (!flushBuffer()) return traits_type::eof();
        if (!traits_type::eq_int_type(value, traits_type::eof()))
        {
            *pptr() = traits_type::to_char_type(value);
            pbump(1);
        }
        return traits_type::not_eof(value);
    }

    std::streamsize xsputn(const char* data, std::streamsize count) override
    {
        std::streamsize total{};
        while (count > 0)
        {
            // assets::saveToSED()/saveToJKL() commonly submit one complete
            // multi-megabyte image.  When our staging buffer is empty, pass
            // that span straight to QSaveFile instead of copying it through
            // 64 KiB chunks first.  Retain the buffered path for small writes
            // and correctly continue after a short device write.
            if (pptr() == pbase()
                && count >= static_cast<std::streamsize>(buffer_.size()))
            {
                const qint64 written = file_.write(
                    data + total, static_cast<qint64>(count));
                if (written <= 0) break;
                total += static_cast<std::streamsize>(written);
                count -= static_cast<std::streamsize>(written);
                continue;
            }
            if (pptr() == epptr() && !flushBuffer()) break;
            const std::streamsize available = epptr() - pptr();
            const std::streamsize copied = std::min(available, count);
            std::memcpy(pptr(), data + total, static_cast<std::size_t>(copied));
            pbump(static_cast<int>(copied));
            total += copied;
            count -= copied;
        }
        return total;
    }

private:
    bool flushBuffer()
    {
        const qint64 size = pptr() - pbase();
        if (size == 0) return true;
        qint64 offset{};
        while (offset < size)
        {
            const qint64 written = file_.write(pbase() + offset, size - offset);
            if (written <= 0) return false;
            offset += written;
        }
        pbump(-static_cast<int>(size));
        return true;
    }

    QSaveFile& file_;
    std::array<char, 64 * 1024> buffer_{};
};

template <typename Serializer>
void saveLevelFileAtomically(
    const std::filesystem::path& path,
    Serializer&& serializer)
{
    QSaveFile file(QString::fromStdWString(path.wstring()));
    // Never fall back to truncating the destination in place: the recovery
    // checkpoint must remain authoritative if this atomic save cannot start.
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly))
    {
        throw std::runtime_error(
            QStringLiteral("Could not open atomic save file: %1")
                .arg(file.errorString()).toStdString());
    }

    QSaveFileStreamBuffer buffer(file);
    std::ostream stream(&buffer);
    serializer(stream);
    stream.flush();
    if (!stream.good())
    {
        file.cancelWriting();
        throw std::runtime_error("Could not completely write the staged level file.");
    }
    if (!file.commit())
    {
        throw std::runtime_error(
            QStringLiteral("Could not atomically replace the level file: %1")
                .arg(file.errorString()).toStdString());
    }
}

ui::project::ProjectInstallStatus currentProjectInstallStatus(const ProjectType kind)
{
    return ui::project::ProjectInstallStatus(
        kind,
        readProjectInstallDirSetting(kind),
        tryGetProjectInstallDir(kind));
}

bool canCreateProjectForKind(const ProjectType kind)
{
    return currentProjectInstallStatus(kind).canCreateProject();
}

enum class FileWriteKind
{
    nativeDocument,
    engineExport,
};

std::filesystem::path suggestedOutputPath(
    std::filesystem::path preferred,
    const QString& fallbackDirectory,
    const std::string_view extension)
{
    if (preferred.empty())
    {
        const QString directory = fallbackDirectory.trimmed();
        preferred = std::filesystem::path(
            QDir(directory).filePath(QStringLiteral("Untitled")).toStdWString());
    }
    preferred.replace_extension(extension);
    return preferred;
}

/** Adds engine-export choices to a standalone modal options dialog. */
struct EngineExportDialogOptions
{
    QCheckBox* lights{};
    QCheckBox* layers{};
    QCheckBox* compact{};
    QCheckBox* clampExtraLight{};
    QCheckBox* omitDefaultMotsArchLighting{};
    QCheckBox* weldNearbyVertices{};
    ui::ScrubFloatEdit* vertexWeldDistance{};
};

EngineExportDialogOptions addEngineExportOptions(
    QDialog& dialog,
    const bool includeLights,
    const bool includeLayers,
    const bool compact,
    const bool clampExtraLight,
    const bool omitDefaultMotsArchLighting,
    const bool weldNearbyVertices,
    const double vertexWeldDistance,
    const ProjectType project)
{
    dialog.setObjectName(QStringLiteral("engineExportOptionsDialog"));
    dialog.setWindowTitle(QStringLiteral("Export options"));
    dialog.setWindowFlags(
        Qt::Dialog
        | Qt::CustomizeWindowHint
        | Qt::WindowTitleHint
        | Qt::WindowCloseButtonHint);
    dialog.setWindowModality(Qt::WindowModal);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // Keep the options legible without forcing all six controls into one
    // unreadable row. The explicit label is part of the dialog contract and
    // remains visible even when a style does not draw a group-box title.
    auto* panel = new QGroupBox(&dialog);
    panel->setObjectName(QStringLiteral("engineExportOptionsPanel"));
    panel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    panel->setMaximumWidth(920);

    auto* panelLayout = new QGridLayout(panel);
    panelLayout->setContentsMargins(8, 4, 8, 4);
    panelLayout->setHorizontalSpacing(12);
    panelLayout->setVerticalSpacing(4);

    auto* label = new QLabel(QStringLiteral("Export options:"), panel);
    label->setObjectName(QStringLiteral("engineExportOptionsLabel"));
    panelLayout->addWidget(label, 0, 0, 1, 3);

    auto* lights = new QCheckBox(
        ui::export_dialog::optionLabel(ui::export_dialog::Option::Lights), panel);
    lights->setObjectName(QStringLiteral("engineExportLights"));
    lights->setToolTip(
        ui::export_dialog::optionDescription(ui::export_dialog::Option::Lights));
    lights->setChecked(includeLights);
    panelLayout->addWidget(lights, 1, 0);

    auto* layers = new QCheckBox(
        ui::export_dialog::optionLabel(ui::export_dialog::Option::Layers), panel);
    layers->setObjectName(QStringLiteral("engineExportLayers"));
    layers->setToolTip(
        ui::export_dialog::optionDescription(ui::export_dialog::Option::Layers));
    layers->setChecked(includeLayers);
    panelLayout->addWidget(layers, 2, 0);

    auto* compactBox = new QCheckBox(
        ui::export_dialog::optionLabel(ui::export_dialog::Option::Compact), panel);
    compactBox->setObjectName(QStringLiteral("engineExportCompact"));
    compactBox->setToolTip(
        ui::export_dialog::optionDescription(ui::export_dialog::Option::Compact));
    compactBox->setChecked(compact);
    panelLayout->addWidget(compactBox, 3, 0);

    auto* clampBox = new QCheckBox(
        ui::export_dialog::optionLabel(
            ui::export_dialog::Option::ClampExtraLight), panel);
    clampBox->setObjectName(QStringLiteral("engineExportClampExtraLight"));
    clampBox->setToolTip(
        ui::export_dialog::optionDescription(
            ui::export_dialog::Option::ClampExtraLight));
    clampBox->setChecked(clampExtraLight);
    panelLayout->addWidget(clampBox, 4, 0);

    auto* omitDefaultArchBox = static_cast<QCheckBox*>(nullptr);
    if (project == ProjectType::MOTS)
    {
        omitDefaultArchBox = new QCheckBox(
            ui::export_dialog::optionLabel(
                ui::export_dialog::Option::OmitDefaultMotsArchLighting), panel);
        omitDefaultArchBox->setObjectName(
            QStringLiteral("engineExportOmitDefaultMotsArchLighting"));
        omitDefaultArchBox->setToolTip(
            ui::export_dialog::optionDescription(
                ui::export_dialog::Option::OmitDefaultMotsArchLighting));
        omitDefaultArchBox->setChecked(omitDefaultMotsArchLighting);
        panelLayout->addWidget(omitDefaultArchBox, 5, 0, 1, 3);

        auto* warning = new QLabel(
            QStringLiteral(
                "Warning: the game will use the 3DO's vertex lighting for omitted records, which may differ from this editor preview."),
            panel);
        warning->setObjectName(
            QStringLiteral("engineExportOmitDefaultMotsArchLightingWarning"));
        warning->setWordWrap(true);
        panelLayout->addWidget(warning, 6, 0, 1, 3);
    }

    const int optionWidth = std::max({
        lights->sizeHint().width(),
        layers->sizeHint().width(),
        compactBox->sizeHint().width(),
        clampBox->sizeHint().width(),
        omitDefaultArchBox ? omitDefaultArchBox->sizeHint().width() : 0,
    });
    lights->setMinimumWidth(optionWidth);
    layers->setMinimumWidth(optionWidth);
    compactBox->setMinimumWidth(optionWidth);
    clampBox->setMinimumWidth(optionWidth);
    if (omitDefaultArchBox)
    {
        omitDefaultArchBox->setMinimumWidth(optionWidth);
    }

    const int weldRow = project == ProjectType::MOTS ? 7 : 5;
    auto* weldBox = new QCheckBox(
        ui::export_dialog::optionLabel(
            ui::export_dialog::Option::WeldNearbyVertices), panel);
    weldBox->setObjectName(QStringLiteral("engineExportWeldNearbyVertices"));
    weldBox->setToolTip(
            ui::export_dialog::optionDescription(
                ui::export_dialog::Option::WeldNearbyVertices));
    weldBox->setChecked(weldNearbyVertices);
    panelLayout->addWidget(weldBox, weldRow, 0);

    auto* weldDistanceLabel = new QLabel(
        ui::export_dialog::optionLabel(
            ui::export_dialog::Option::VertexWeldDistance) + QStringLiteral(":"),
        panel);
    weldDistanceLabel->setObjectName(
        QStringLiteral("engineExportVertexWeldDistanceLabel"));
    weldDistanceLabel->setToolTip(
        ui::export_dialog::optionDescription(
            ui::export_dialog::Option::VertexWeldDistance));
    panelLayout->addWidget(weldDistanceLabel, weldRow, 1);

    auto* weldDistance = new ui::ScrubFloatEdit(panel);
    weldDistance->setObjectName(QStringLiteral("engineExportVertexWeldDistance"));
    weldDistance->setToolTip(
        ui::export_dialog::optionDescription(
            ui::export_dialog::Option::VertexWeldDistance));
    weldDistance->setDecimals(8);
    weldDistance->setSingleStep(
        ui::export_dialog::kDefaultVertexWeldDistance);
    weldDistance->setRange(
        ui::export_dialog::kMinimumVertexWeldDistance,
        std::numeric_limits<double>::max());
    weldDistance->setValue(vertexWeldDistance);
    weldDistance->setMinimumWidth(112);
    weldDistance->setMaximumWidth(132);
    weldDistance->setEnabled(weldNearbyVertices);
    panelLayout->addWidget(weldDistance, weldRow, 2);

    QObject::connect(weldBox, &QCheckBox::toggled,
        weldDistance, &QWidget::setEnabled);
    panelLayout->setColumnStretch(3, 1);

    layout->addWidget(panel);

    auto* buttonBox = new QDialogButtonBox(&dialog);
    buttonBox->setObjectName(QStringLiteral("engineExportOptionsButtonBox"));
    auto* exportButton = buttonBox->addButton(
        QStringLiteral("Export"), QDialogButtonBox::AcceptRole);
    exportButton->setObjectName(QStringLiteral("engineExportOptionsExport"));
    exportButton->setToolTip(QStringLiteral("Export the level"));
    auto* cancelButton = buttonBox->addButton(
        QStringLiteral("Cancel"), QDialogButtonBox::RejectRole);
    cancelButton->setObjectName(QStringLiteral("engineExportOptionsCancel"));
    exportButton->setDefault(true);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted,
        &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected,
        &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    dialog.adjustSize();
    dialog.setFixedSize(dialog.sizeHint());

    return EngineExportDialogOptions{
        lights, layers, compactBox, clampBox, omitDefaultArchBox,
        weldBox, weldDistance };
}


} // namespace

void MainWindow::setupFileMenu(QMenu* fileMenu)
{
    if (!fileMenu)
    {
        return;
    }

    // Step 1: prepare the save/open/new-project helpers used by File menu actions.
    auto configureProgressDialog = [](QProgressDialog& progressDialog, const QString& title, const QString& label)
        {
            progressDialog.setWindowTitle(title);
            progressDialog.setLabelText(label);
            // This display has no input controls. Keep the editor's focus
            // while the immutable Save snapshot is written so progress alone
            // cannot complete a held Control-wheel gesture.
            progressDialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint
                | Qt::WindowTitleHint | Qt::WindowDoesNotAcceptFocus);
            progressDialog.setAttribute(Qt::WA_ShowWithoutActivating);
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
        };

    auto saveToPath = [this, configureProgressDialog](
        std::filesystem::path levelPath,
        const FileWriteKind writeKind)
        {
            struct PendingDiagnostic
            {
                ::sed::DiagnosticSeverity severity{};
                std::string text{};
            };

            if (writeKind == FileWriteKind::nativeDocument)
            {
                levelPath.replace_extension(".sed");
            }
            else if (levelPath.extension().empty())
            {
                levelPath.replace_extension(
                    ui::project::nativeRuntimeLevelSuffix(
                        document_.level().kind).toStdString());
            }
            std::string ext = sed::text::lowerAscii(levelPath.extension().string());

            bool keepExistingLegacyUvFormat = false;
            if (writeKind == FileWriteKind::nativeDocument
                && document_.level().uvCoordinateSpace
                    == UvCoordinateSpace::Normalized
                && std::filesystem::is_regular_file(levelPath))
            {
                std::optional<std::uint32_t> existingDestinationVersion{};
                bool existingDestinationUsesLegacyUvFormat =
                    nativeSourceUsesLegacyUvFormat_
                    && currentPath_ == levelPath;
                try
                {
                    existingDestinationVersion =
                        assets::readSedBinaryVersion(levelPath);
                    existingDestinationUsesLegacyUvFormat =
                        assets::sedBinaryVersionUsesTexturePixels(
                            *existingDestinationVersion);
                }
                catch (const std::exception&)
                {
                    // An invalid or unreadable destination will be handled
                    // by the ordinary atomic Save path. A legacy source
                    // already associated with this workspace is tracked
                    // separately so recoverable header damage cannot hide
                    // the upgrade warning.
                }
                if (existingDestinationUsesLegacyUvFormat)
                {
                    const bool hasArchLighting =
                        !document_.level().motsArchLighting.empty();
                    const QString existingFormat =
                        existingDestinationVersion
                            && *existingDestinationVersion
                                == assets::SedArchPixelBinaryVersion
                        ? QStringLiteral("SED V4")
                        : QStringLiteral("SED V1");
                    const QString normalizedFormat = hasArchLighting
                        ? QStringLiteral("SED V3") : QStringLiteral("SED V2");
                    const QString pixelFormat = hasArchLighting
                        ? QStringLiteral("SED V4") : QStringLiteral("SED V1");
                    QMessageBox prompt(this);
                    prompt.setObjectName(QStringLiteral(
                        "legacy_sed_v1_overwrite_prompt"));
                    prompt.setIcon(QMessageBox::Warning);
                    prompt.setWindowTitle(QStringLiteral(
                        "Save legacy SED file"));
                    prompt.setText(QStringLiteral(
                        "The existing file uses %1 pixel-space UVs.")
                        .arg(existingFormat));
                    prompt.setInformativeText(QStringLiteral(
                        "Upgrade and Save replaces it with normalized %1. "
                        "Keep the legacy format and Save writes %2 while "
                        "keeping the open document normalized. Cancel leaves "
                        "the existing file unchanged.")
                        .arg(normalizedFormat, pixelFormat));
                    QPushButton* upgrade = prompt.addButton(
                        QStringLiteral("Upgrade and Save"),
                        QMessageBox::AcceptRole);
                    upgrade->setObjectName(QStringLiteral(
                        "legacy_sed_upgrade_and_save"));
                    QPushButton* keep = prompt.addButton(
                        QStringLiteral("Keep %1 and Save").arg(pixelFormat),
                        QMessageBox::ActionRole);
                    keep->setObjectName(QStringLiteral(
                        "legacy_sed_keep_v1_and_save"));
                    QPushButton* cancel = prompt.addButton(
                        QMessageBox::Cancel);
                    cancel->setObjectName(QStringLiteral(
                        "legacy_sed_cancel_save"));
                    prompt.setDefaultButton(upgrade);
                    prompt.setEscapeButton(cancel);
                    (void)prompt.exec();
                    if (prompt.clickedButton() == keep)
                    {
                        keepExistingLegacyUvFormat = true;
                    }
                    else if (prompt.clickedButton() != upgrade)
                    {
                        return;
                    }
                }
            }

            std::optional<editor::Document> sedDocumentSnapshot{};
            std::optional<::sed::Level> exportCandidate{};
            if (writeKind == FileWriteKind::nativeDocument)
            {
                // A native Save needs the complete Document for HIST. Use its
                // current Level as the authoritative save candidate as well;
                // copying document_.level() separately and then assigning that
                // copy back into the Document snapshot doubled the largest
                // GUI-thread allocation for no semantic benefit.
                sedDocumentSnapshot.emplace(document_);
            }
            else
            {
                exportCandidate.emplace(document_.level());
            }
            ::sed::Level& candidate = writeKind == FileWriteKind::nativeDocument
                ? sedDocumentSnapshot->current().level
                : *exportCandidate;
            candidate.lvisstring = buildLayerVisibilityString();
            for (Thing& thing : candidate.things)
            {
                ::sed::level::thingSyncNumFrames(thing);
            }

            if (keepExistingLegacyUvFormat && sedDocumentSnapshot)
            {
                editor::DocumentRecoveryHistory convertedHistory =
                    sedDocumentSnapshot->compactRecoveryHistory();
                editor::DocumentState convertedCurrent =
                    sedDocumentSnapshot->current();
                auto dimensionsCache = std::make_shared<std::unordered_map<
                    std::string,
                    std::optional<level::UvTextureDimensions>>>();
                const level::UvTextureDimensionsResolver dimensions =
                    [this, dimensionsCache](const Surface& surface)
                        -> std::optional<level::UvTextureDimensions>
                    {
                        const std::string key = text::lowerAscii(
                            text::trimView(surface.material));
                        if (const auto found = dimensionsCache->find(key);
                            found != dimensionsCache->end())
                        {
                            return found->second;
                        }
                        std::optional<level::UvTextureDimensions> value =
                            assets::resolveMaterialTextureDimensions(
                                resources_, surface.material);
                        dimensionsCache->emplace(key, value);
                        return value;
                    };
                const editor::DocumentUvConversionResult conversion =
                    editor::convertDocumentUvCoordinateSpace(
                        convertedCurrent.level,
                        convertedHistory,
                        UvCoordinateSpace::TexturePixels,
                        dimensions);
                if ( !conversion )
                {
                    const QString pixelFormat =
                        !convertedCurrent.level.motsArchLighting.empty()
                        ? QStringLiteral("SED V4")
                        : QStringLiteral("SED V1");
                    QString location{};
                    if ( conversion.failedHistoryStack )
                    {
                        location += *conversion.failedHistoryStack
                                == editor::DocumentRecoveryHistoryStack::Undo
                            ? QStringLiteral(" in Undo history entry %1")
                                  .arg(conversion.failedHistoryIndex + 1)
                            : QStringLiteral(" in Redo history entry %1")
                                  .arg(conversion.failedHistoryIndex + 1);
                    }
                    if ( conversion.failedSurface )
                    {
                        location += QStringLiteral(
                            " at Sector %1, Surface %2")
                            .arg(conversion.failedSurface->sector)
                            .arg(conversion.failedSurface->surface);
                    }
                    const QString error = QStringLiteral(
                        "Cannot preserve %1 because the normalized "
                        "document and its complete undo/redo history could "
                        "not be converted safely%2: %3")
                        .arg(pixelFormat,
                            location,
                            QString::fromStdString(conversion.error));
                    logMessage(MessageSeverity::warning, error);
                    QMessageBox warning(
                        QMessageBox::Warning,
                        QStringLiteral("Cannot keep %1").arg(pixelFormat),
                        error,
                        QMessageBox::Ok,
                        this);
                    warning.setProperty(
                        kMessageBoxLoggingSkipProperty, true);
                    (void)warning.exec();
                    return;
                }
                sedDocumentSnapshot->restoreRecoveryHistory(
                    std::move(convertedCurrent),
                    std::move(convertedHistory));
            }

            // Everything consumed by the worker belongs to this exact
            // document generation. Capture it before the progress dialog can
            // pump a timer/plugin event that opens another document.
            const std::uint64_t sourceDocumentEpoch = recoveryDocumentEpoch_;
            const bool needsExportResources =
                writeKind == FileWriteKind::engineExport;
            // Reuse the immutable resource generation already shared by
            // thumbnail workers. Copying a large mounted-GOB index on every
            // Save/Export would itself cause a noticeable UI pause.
            const std::shared_ptr<const assets::ResourceLocator>
                resourceSnapshot = needsExportResources
                    ? (resourceLocatorSnapshot_
                        ? resourceLocatorSnapshot_
                        : std::make_shared<const assets::ResourceLocator>(resources_))
                    : std::shared_ptr<const assets::ResourceLocator>{};
            const std::shared_ptr<const assets::Templates> templateSnapshot =
                needsExportResources && templates_.size() > 0
                    ? std::make_shared<const assets::Templates>(templates_)
                    : std::shared_ptr<const assets::Templates>{};
            const bool exportIncludeLights = engineExportIncludeLights_;
            const bool exportIncludeLayers = engineExportIncludeLayers_;
            const bool exportCompact = engineExportCompact_;
            const bool exportClampExtraLight = engineExportClampExtraLight_;
            const bool exportOmitDefaultMotsArchLighting =
                engineExportOmitDefaultMotsArchLighting_;
            const bool exportWeldNearbyVertices = engineExportWeldNearbyVertices_;
            const double exportVertexWeldDistance = engineExportVertexWeldDistance_;

            QByteArray sedEditorState{};
            std::int64_t sedDocumentTimestamp{};
            QByteArray nativeSaveAuthoredLevelHash{};
            QByteArray nativeSaveAuthoredCleaveHash{};
            QByteArray nativeSaveAuthoredCogHash{};
            QByteArray nativeSaveAuthoredResourceHash{};
            if (writeKind == FileWriteKind::nativeDocument)
            {
                // Capture the authored transaction before the worker starts.
                // The GUI pumps non-user events while encoding, so a
                // background-authored mutation must not be blessed as saved
                // when this older snapshot reaches disk.
                nativeSaveAuthoredLevelHash = recoveryCurrentLevelStateHash();
                nativeSaveAuthoredCleaveHash = currentCleaveAuthoredStateHash();
                const QByteArray authoredCogState = captureAuthoredCogRecoveryState();
                nativeSaveAuthoredCogHash = QCryptographicHash::hash(
                    authoredCogState, QCryptographicHash::Sha256);
                nativeSaveAuthoredResourceHash = QCryptographicHash::hash(
                    captureAuthoredResourceEditorRecoveryState(),
                    QCryptographicHash::Sha256);
                sedEditorState = captureRecoveryEditorState();
                QString editorStateValidationError{};
                if (!validateRecoveryEditorState(
                        sedEditorState, &editorStateValidationError))
                {
                    const QString error = QStringLiteral(
                        "Cannot save the Sith Editor file because its EDIT payload "
                        "failed exact validation: %1")
                        .arg(recoveryEditorStateCaptureError_.isEmpty()
                            ? editorStateValidationError
                            : recoveryEditorStateCaptureError_);
                    logMessage(MessageSeverity::error, error);
                    showExplicitlyLoggedCriticalMessage(
                        this, QStringLiteral("Save failed"), error);
                    return;
                }
                const std::optional<QByteArray> candidateEditorState =
                    detail::remapRecoveryEditorFrameRefsForCandidateLevel(
                        sedEditorState, document_.level(), candidate,
                        &editorStateValidationError);
                if (!candidateEditorState)
                {
                    const QString error = QStringLiteral(
                        "Cannot save the Sith Editor file because its EDIT frame references "
                        "could not be paired with the normalized Level: %1")
                        .arg(editorStateValidationError);
                    logMessage(MessageSeverity::error, error);
                    showExplicitlyLoggedCriticalMessage(
                        this, QStringLiteral("Save failed"), error);
                    return;
                }
                sedEditorState = *candidateEditorState;
                QCborParserError candidateParseError{};
                const QCborValue candidateValue = QCborValue::fromCbor(
                    sedEditorState, &candidateParseError);
                QString candidateSelectionError{};
                const QCborValue candidateSelectionValue =
                    candidateParseError.error == QCborError::NoError
                        && candidateValue.isMap()
                    ? candidateValue.toMap().value(QStringLiteral("selection"))
                    : QCborValue{};
                const std::optional<editor::Selection> candidateSelection =
                    candidateSelectionValue.isByteArray()
                    ? ui::recovery::deserializeSelection(
                          candidateSelectionValue.toByteArray(),
                          &candidateSelectionError)
                    : std::nullopt;
                if (!candidateSelection)
                {
                    const QString error = QStringLiteral(
                        "Cannot save the Sith Editor file because its normalized EDIT selection is invalid: %1")
                        .arg(candidateSelectionError);
                    logMessage(MessageSeverity::error, error);
                    showExplicitlyLoggedCriticalMessage(
                        this, QStringLiteral("Save failed"), error);
                    return;
                }
                sedDocumentSnapshot->current().selection = *candidateSelection;
                // HIST references are delta-coded against the exact Level
                // bytes written above.  Save-time normalization (layer
                // visibility and frame counts) therefore happens directly in
                // the snapshot used by both the Level writer and history
                // serializer.

                const std::int64_t now = std::max<std::int64_t>(
                    1, QDateTime::currentMSecsSinceEpoch());
                // QSaveFile atomically publishes a newly created file image.
                // The one native timestamp therefore records this Save, rather
                // than retaining a second logical-document modification clock.
                sedDocumentTimestamp = now;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                if (nativeSaveAfterCaptureForTest_)
                {
                    nativeSaveAfterCaptureForTest_();
                }
#endif
            }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if (writeKind == FileWriteKind::engineExport
                && engineExportAfterCaptureForTest_)
            {
                engineExportAfterCaptureForTest_();
            }
#endif

            if (writeKind == FileWriteKind::engineExport
                && !ui::project::isEngineLevelExportPath(
                    levelPath, candidate.kind))
            {
                logMessage(MessageSeverity::error,
                    QStringLiteral("Cannot export to this file type for the current project: %1")
                        .arg(QString::fromStdWString(levelPath.extension().wstring())));
                return;
            }

            if (ext == ".jed" && !ui::project::supportsJedLevelExport(candidate.kind))
            {
                logMessage(MessageSeverity::error, "Cannot save IJIM levels as .jed; use .ndy instead.");
                return;
            }

            if (ext != ".jed" && ext != ".sed")
            {
                if (!ui::resources::isValidProjectType(candidate.kind))
                {
                    logMessage(MessageSeverity::error, "Invalid project type; cannot export.");
                    return;
                }

                if (ext == ".ndy" && candidate.kind != ProjectType::IJIM)
                {
                    logMessage(MessageSeverity::error, "Cannot save as .ndy: current project is not IJIM.");
                    return;
                }
                if (ext == ".jkl" && candidate.kind == ProjectType::IJIM)
                {
                    logMessage(MessageSeverity::error, "Cannot save IJIM levels as .jkl; use .ndy instead.");
                    return;
                }

                if (candidate.kind == ProjectType::IJIM && candidate.header.version != 2 && candidate.header.version != 3)
                {
                    candidate.header.version = 3;
                }
            }

            if (ext != ".jed" && ext != ".sed"
                && resourceRoots_.empty() && resourceGobs_.empty())
            {
                logMessage(
                    MessageSeverity::warning,
                    "No per-project resources configured for export; open Edit -> Preferences to set this project's Resource/install directory.");
            }

            const QString levelLabel =
                QString::fromStdWString(levelPath.filename().wstring()).isEmpty()
                ? QString::fromStdWString(levelPath.wstring())
                : QString::fromStdWString(levelPath.filename().wstring());

            const QString operation = writeKind == FileWriteKind::nativeDocument
                ? QStringLiteral("Saving")
                : QStringLiteral("Exporting");
            QProgressDialog progressDialog(this);
            configureProgressDialog(progressDialog, operation + QStringLiteral(" Level"), QString("Preparing %1...").arg(levelLabel));
            progressDialog.setValue(0);
            progressDialog.show();

            auto updateSaveProgress = [&](const QString& label, const int value)
            {
                progressDialog.setLabelText(label);
                progressDialog.setValue(std::clamp(value, 0, 100));
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            };

            updateSaveProgress(QString("Preparing %1...").arg(levelLabel), 8);

            updateSaveProgress(QString("%1 %2...").arg(operation, levelLabel), 18);
            auto saveFuture = std::async(
                std::launch::async,
                [exportCandidate = std::move(exportCandidate), levelPath, ext,
                 resourceSnapshot, templateSnapshot,
                 exportIncludeLights, exportIncludeLayers, exportCompact,
                 exportClampExtraLight, exportOmitDefaultMotsArchLighting,
                 exportWeldNearbyVertices, exportVertexWeldDistance,
                 sedEditorState = std::move(sedEditorState),
                 sedDocumentSnapshot = std::move(sedDocumentSnapshot),
                 sedDocumentTimestamp]() mutable -> std::vector<PendingDiagnostic>
                {
                    std::vector<PendingDiagnostic> diagnostics{};
                    if (ext == ".jed")
                    {
                        ::sed::Level& candidate = *exportCandidate;
                        saveLevelFileAtomically(levelPath,
                            [&candidate, &resourceSnapshot](std::ostream& out)
                            {
                                ::sed::assets::JedSaveOptions options{};
                                options.resources = resourceSnapshot.get();
                                ::sed::assets::saveToJED(
                                    candidate, out, options);
                            });
                        return diagnostics;
                    }
                    if (ext == ".sed")
                    {
                        if (!sedDocumentSnapshot)
                        {
                            throw std::runtime_error("Sith Editor document snapshot is unavailable.");
                        }
                        ::sed::Level& candidate = sedDocumentSnapshot->current().level;
                        std::vector<assets::SedOptionalSection> optionalSections{};
                        optionalSections.reserve(2);
                        // EDIT is authoritative workspace state and remains a
                        // hard requirement for a native manual save. HIST is
                        // optional: an exceptionally large or otherwise
                        // unserializable undo timeline must not prevent the
                        // current Level and EDIT state from being saved.
                        try
                        {
                            QByteArray boundEditorState{};
                            QString attachmentWarning{};
                            const QByteArray history =
                                ui::recovery::serializeEditorDocumentHistoryForSaveWithWorkspaceBindings(
                                    *sedDocumentSnapshot,
                                    sedEditorState,
                                    &boundEditorState,
                                    &attachmentWarning);
                            if (history.isEmpty()
                                || static_cast<std::uint64_t>(history.size())
                                    > assets::SedMaximumHistorySectionBytes)
                            {
                                throw std::runtime_error(
                                    "Serialized undo/redo history exceeds the native HIST limit.");
                            }
                            sedEditorState = std::move(boundEditorState);
                            optionalSections.push_back(
                                ui::project::makeSedOptionalSection(
                                    ui::project::SedHistorySection, history));
                            if (!attachmentWarning.isEmpty())
                            {
                                PendingDiagnostic warning{};
                                warning.severity = ::sed::DiagnosticSeverity::warning;
                                warning.text = attachmentWarning.toStdString();
                                diagnostics.push_back(std::move(warning));
                            }
                        }
                        catch (const std::exception& error)
                        {
                            QString attachmentWarning{};
                            sedEditorState =
                                ui::recovery::omitEditorWorkspaceHistoryAttachments(
                                    sedEditorState, &attachmentWarning);
                            PendingDiagnostic warning{};
                            warning.severity = ::sed::DiagnosticSeverity::warning;
                            warning.text =
                                "Saved without undo/redo history (HIST); the current Level "
                                "and current editor workspace (EDIT) were preserved. "
                                "Cleave/Layer history attachments were omitted. Reason: ";
                            warning.text += error.what();
                            diagnostics.push_back(std::move(warning));
                        }
                        optionalSections.insert(optionalSections.begin(),
                            ui::project::makeSedOptionalSection(
                                ui::project::SedEditorStateSection,
                                sedEditorState));
                        saveLevelFileAtomically(levelPath, [&candidate, &optionalSections,
                                                            sedDocumentTimestamp](std::ostream& out)
                            {
                                ::sed::assets::saveToSED(candidate, optionalSections,
                                    sedDocumentTimestamp, out);
                            });
                        return diagnostics;
                    }

                    ::sed::Level& candidate = *exportCandidate;
                    ::sed::assets::JklSaveOptions saveOpts{};
                    saveOpts.resources = resourceSnapshot.get();
                    saveOpts.templatesDb = templateSnapshot.get();
                    saveOpts.includeLights = exportIncludeLights;
                    saveOpts.includeLayers = exportIncludeLayers;
                    saveOpts.compact = exportCompact;
                    saveOpts.clampExtraLight = exportClampExtraLight;
                    saveOpts.omitDefaultMotsArchLighting =
                        exportOmitDefaultMotsArchLighting;
                    saveOpts.weldNearbyVertices = exportWeldNearbyVertices;
                    saveOpts.vertexWeldDistance = exportVertexWeldDistance;
                    saveOpts.onDiagnostic = [&diagnostics](const ::sed::DiagnosticSeverity severity, const std::string_view message)
                    {
                        PendingDiagnostic entry{};
                        entry.severity = severity;
                        entry.text.assign(message.data(), message.size());
                        diagnostics.push_back(std::move(entry));
                    };

                    saveLevelFileAtomically(levelPath, [&candidate, &saveOpts](std::ostream& out)
                        {
                            ::sed::assets::saveToJKL(candidate, out, saveOpts);
                        });
                    return diagnostics;
                });

            int progressPulse = 30;
            while (saveFuture.wait_for(std::chrono::milliseconds(25)) != std::future_status::ready)
            {
                progressPulse = std::min(progressPulse + 1, 82);
                progressDialog.setValue(progressPulse);
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 25);
            }

            std::vector<PendingDiagnostic> diagnostics = saveFuture.get();
            // The progress loop intentionally processes non-user events. A
            // timer/plugin may therefore have replaced the active document
            // while this immutable snapshot was being written. The file is
            // valid, but it must never rebind or mark that newer workspace as
            // saved/exported. A completed engine export is still recorded as
            // an independently openable recent below.
            if (recoveryDocumentEpoch_ != sourceDocumentEpoch)
            {
                const QString displayPath = ui::nativeDisplayPath(levelPath);
                if (writeKind == FileWriteKind::engineExport)
                {
                    // The captured export is complete and remains a useful
                    // open target even though it must not become the newer
                    // workspace's repeat-export or native Save destination.
                    rememberRecentLevelFile(
                        QString::fromStdWString(levelPath.wstring()));
                }
                logMessage(
                    MessageSeverity::warning,
                    QStringLiteral(
                        "%1 %2 from the captured document, but the active workspace changed during the operation and was left untouched.")
                        .arg(operation, displayPath));
                statusBar()->showMessage(QStringLiteral(
                    "%1 completed; active workspace unchanged.").arg(operation));
                // The document epoch was already found stale. Do not pump
                // another event while unwinding this obsolete save.
                progressDialog.setLabelText(
                    QString("%1 %2").arg(operation, levelLabel));
                progressDialog.setValue(100);
                progressDialog.close();
                return;
            }

            // Check the generation before another progress event pump or any
            // diagnostic publication. A completed export from an obsolete
            // snapshot is a valid file, but its messages and bookkeeping must
            // not be attached to the replacement workspace.
            // The epoch check above is the transaction boundary. From here to
            // bookkeeping, avoid processEvents(): a timer/plugin could replace
            // the document after validation and make us bless the wrong one.
            progressDialog.setLabelText(
                QString("Finalizing %1...").arg(levelLabel));
            progressDialog.setValue(92);

            for (const PendingDiagnostic& diagnostic : diagnostics)
            {
                const QString text = QString::fromUtf8(diagnostic.text.data(), static_cast<int>(diagnostic.text.size()));
                switch (diagnostic.severity)
                {
                    case ::sed::DiagnosticSeverity::verbose:
                        logMessage(MessageSeverity::verbose, text);
                        break;
                    case ::sed::DiagnosticSeverity::trace:
                        logMessage(MessageSeverity::trace, text);
                        break;
                    case ::sed::DiagnosticSeverity::debug:
                        logMessage(MessageSeverity::debug, text);
                        break;
                    case ::sed::DiagnosticSeverity::info:
                        logMessage(MessageSeverity::info, text);
                        break;
                    case ::sed::DiagnosticSeverity::warning:
                        logMessage(MessageSeverity::warning, text);
                        break;
                    case ::sed::DiagnosticSeverity::error:
                        logMessage(MessageSeverity::error, text);
                        break;
                    case ::sed::DiagnosticSeverity::fatal:
                        logMessage(MessageSeverity::fatal, text);
                        break;
                }
            }

            const QString displayPath = ui::nativeDisplayPath(levelPath);
            if (writeKind == FileWriteKind::nativeDocument)
            {
                nativeDocumentTimestamp_ = sedDocumentTimestamp;
                nativeSourceUsesLegacyUvFormat_ =
                    keepExistingLegacyUvFormat;
                const QByteArray currentAuthoredLevelHash =
                    recoveryCurrentLevelStateHash();
                const QByteArray currentAuthoredCogState =
                    captureAuthoredCogRecoveryState();
                const QByteArray currentAuthoredCogHash = QCryptographicHash::hash(
                    currentAuthoredCogState, QCryptographicHash::Sha256);
                const QByteArray currentAuthoredResourceHash = QCryptographicHash::hash(
                    captureAuthoredResourceEditorRecoveryState(),
                    QCryptographicHash::Sha256);
                const bool authoredSnapshotStillCurrent =
                    !nativeSaveAuthoredLevelHash.isEmpty()
                    && !nativeSaveAuthoredCleaveHash.isEmpty()
                    && currentAuthoredLevelHash == nativeSaveAuthoredLevelHash
                    && currentCleaveAuthoredStateHash()
                        == nativeSaveAuthoredCleaveHash
                    && currentAuthoredCogHash == nativeSaveAuthoredCogHash
                    && currentAuthoredResourceHash == nativeSaveAuthoredResourceHash;
                if (authoredSnapshotStillCurrent)
                {
                    completeRecoveryAfterNormalSave(levelPath);
                }
                else
                {
                    // The destination contains the captured snapshot, while
                    // the in-memory document contains newer authored data.
                    // Keep that transaction dirty and publish a newer
                    // recovery instead of silently blessing the old file.
                    if (currentPath_ != levelPath)
                    {
                        currentPath_ = levelPath;
                        switchRecoveryDocument(levelPath, false, false);
                    }
                    else
                    {
                        currentPath_ = levelPath;
                        // Keep the newer recovery's external-edit baseline
                        // bound to the file that just reached disk.  The
                        // in-memory authored mutation remains dirty, but it
                        // must not inherit the pre-save source timestamp.
                        recoverySourceModifiedAtMs_ = ui::recovery::sourceModifiedAtMs(
                            currentPath_);
                    }
                    for (const QPointer<QWidget>& page : resourceEditorPages())
                    {
                        if (page)
                        {
                            ui::resources::resourceAuthoringPaneRebindProjectDirectory(
                                page.data(), levelPath.parent_path());
                        }
                    }
                    recoveryRequiresSave_ = true;
                    refreshWindowTitle();
                    recoverySnapshotRequired_ = true;
                    scheduleRecoverySnapshot(true);
                }
                rememberRecentLevelFile(QString::fromStdWString(levelPath.wstring()));
                rememberFileDialogSelection(
                    FileDialogHistoryGroup::openSave,
                    QString::fromStdWString(levelPath.wstring()));
                statusBar()->showMessage(QString("Saved %1").arg(displayPath));
                if (authoredSnapshotStillCurrent)
                {
                    markCurrentDocumentAsSaved();
                    logMessage(MessageSeverity::info, QString("Saved: %1").arg(displayPath));
                }
                else
                {
                    logMessage(
                        MessageSeverity::warning,
                        QString("Saved %1; newer authored changes remain unsaved and were retained in recovery.")
                            .arg(displayPath));
                }
            }
            else
            {
                lastEngineExportPath_ = levelPath;
                noteRecoveryStateChanged(false);
                // Engine exports are useful open targets, but they remain
                // separate from the authoritative native .sed document path.
                // Recording the export as a recent file must not rebind
                // currentPath_ or change subsequent Ctrl+S behavior.
                rememberRecentLevelFile(
                    QString::fromStdWString(levelPath.wstring()));
                rememberFileDialogSelection(
                    FileDialogHistoryGroup::export_,
                    QString::fromStdWString(levelPath.wstring()));
                statusBar()->showMessage(QString("Exported %1").arg(displayPath));
                logMessage(MessageSeverity::info, QString("Exported: %1").arg(displayPath));
            }
            updateEditActions();
            progressDialog.setLabelText(
                QString("%1 %2").arg(
                    writeKind == FileWriteKind::nativeDocument
                        ? QStringLiteral("Saved")
                        : QStringLiteral("Exported"),
                    levelLabel));
            progressDialog.setValue(100);
            progressDialog.close();
        };

    auto* openRecentMenu = new ui::shortcuts::KeySequenceDisplayMenu("Open &Recent", this);
    openRecentMenu->setObjectName("file.open_recent_menu");

    auto* autosaveHistoryAction = new QAction("Autosave &History...", this);
    autosaveHistoryAction->setObjectName("file.autosave_history");
    autosaveHistoryAction->setToolTip(
        "Open or delete one of the ten most recent periodic autosaves for an editing session.");
    connect(autosaveHistoryAction, &QAction::triggered,
        this, &MainWindow::showAutosaveHistory);

    auto* clearRecentAction = new QAction("Clear Recent", this);
    clearRecentAction->setObjectName("file.clear_recent");
    connect(clearRecentAction, &QAction::triggered, this, [this]()
        {
            saveRecentLevels({});
            savePinnedRecentLevels({});
        });

    auto rebuildOpenRecentMenu = [this, openRecentMenu, autosaveHistoryAction,
                                  clearRecentAction]()
    {
        openRecentMenu->clear();

        QStringList recent = loadRecentLevels();
        recent.removeAll(QString{});

        QStringList validRecent{};
        validRecent.reserve(recent.size());
        for (const QString& file : recent)
        {
            const QFileInfo info(file);
            if (info.exists() && info.isFile())
            {
                validRecent.push_back(file);
            }
        }
        if (validRecent != recent)
        {
            saveRecentLevels(validRecent);
            const QStringList pinned = ui::project::sanitizedPinnedRecentLevels(
                validRecent, loadPinnedRecentLevels());
            savePinnedRecentLevels(pinned);
        }

        if (validRecent.isEmpty())
        {
            auto* empty = openRecentMenu->addAction("(No recent levels)");
            empty->setEnabled(false);
        }
        else
        {
            for (const QString& file : validRecent)
            {
                const QFileInfo info(file);
                const QString label = QString("%1").arg(QDir::toNativeSeparators(file));

                auto* a = openRecentMenu->addAction(label);
                a->setData(info.absoluteFilePath());
                a->setEnabled(true);
            }
        }

        openRecentMenu->addSeparator();
        openRecentMenu->addAction(autosaveHistoryAction);
        openRecentMenu->addSeparator();
        clearRecentAction->setEnabled(!validRecent.isEmpty());
        openRecentMenu->addAction(clearRecentAction);
    };

    auto openLevelFromFilename = [this](const QString& filename)
    {
        this->openLevelFromFilename(filename);
    };

    auto updateNewProjectActionState = [](QAction* action, const ProjectType kind)
    {
        if (!action)
        {
            return;
        }

        const bool enabled = canCreateProjectForKind(kind);
        action->setEnabled(enabled);
        if (enabled)
        {
            action->setToolTip(QString{});
        }
        else
        {
            action->setToolTip("Configure this project's install/resource path in Preferences first.");
        }
    };

    auto createNewProject = [this](const ProjectType kind)
    {
        this->createNewProject(kind);
    };

    // Step 2: create actions, connect them, and add them to the File menu.
    auto* openAction = new QAction("&Open...", this);
    openAction->setObjectName("file.open");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this, openLevelFromFilename]()
        {
            const QString startDir = fileDialogStartLocation(
                FileDialogHistoryGroup::openSave);
            const QString filename =
                QFileDialog::getOpenFileName(
                    this,
                    "Open level",
                    startDir,
                    ui::project::levelOpenDialogFilter());
            if (filename.isEmpty())
            {
                return;
            }

            openLevelFromFilename(filename);
        });

    auto* newProjectMenu = new ui::shortcuts::KeySequenceDisplayMenu("&New Project", this);
    newProjectMenu->setObjectName("file.new_project_menu");

    auto addNewProjectAction = [this, newProjectMenu, updateNewProjectActionState, createNewProject](const ProjectType kind) -> QAction*
    {
        const QString text = QString("%1...").arg(ui::project::projectTypeLabel(kind));
        auto* action = new QAction(text, this);
        updateNewProjectActionState(action, kind);
        connect(action, &QAction::triggered, this, [createNewProject, kind]()
            {
                createNewProject(kind);
            });
        newProjectMenu->addAction(action);
        return action;
    };

    QAction* newJkdf2Action = addNewProjectAction(ProjectType::JKDF2);
    QAction* newMotsAction = addNewProjectAction(ProjectType::MOTS);
    QAction* newIjimAction = addNewProjectAction(ProjectType::IJIM);
    connect(newProjectMenu, &QMenu::aboutToShow, this, [updateNewProjectActionState, newJkdf2Action, newMotsAction, newIjimAction]()
        {
            if (newJkdf2Action)
            {
                updateNewProjectActionState(newJkdf2Action, ProjectType::JKDF2);
            }
            if (newMotsAction)
            {
                updateNewProjectActionState(newMotsAction, ProjectType::MOTS);
            }
            if (newIjimAction)
            {
                updateNewProjectActionState(newIjimAction, ProjectType::IJIM);
            }
        });

    auto* openRecentAction = openRecentMenu->menuAction();
    openRecentAction->setObjectName("file.open_recent");
    openRecentAction->setShortcut(QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_O));
    connect(openRecentAction, &QAction::triggered, this, [openRecentMenu]()
        {
            openRecentMenu->popup(QCursor::pos());
        });

    rebuildOpenRecentMenu();
    connect(openRecentMenu, &QMenu::aboutToShow, this, [rebuildOpenRecentMenu]()
        {
            rebuildOpenRecentMenu();
        });
    connect(openRecentMenu, &QMenu::triggered, this, [this, openLevelFromFilename](QAction* action)
        {
            if (!action)
            {
                return;
            }

            const QString file = action->data().toString();
            if (file.isEmpty())
            {
                return;
            }

            openLevelFromFilename(file);
        });

    auto* saveAction = new QAction("&Save", this);
    saveAction->setObjectName("file.save");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this, saveToPath]()
        {
            QWidget* focusWidget = QApplication::focusWidget();
            const bool cogEditorFocusContext = [this, focusWidget]() -> bool
            {
                if (!focusWidget)
                {
                    return false;
                }

                for (QWidget* current = focusWidget; current; current = current->parentWidget())
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
            }();
            if (!forceNativeDocumentSave_ && resourceEditorOwnsFocus())
            {
                (void)triggerActiveResourceEditorSave(false);
                return;
            }
            if (!forceNativeDocumentSave_ && cogEditorFocusContext)
            {
                (void)triggerActiveCogScriptEditorSave();
                return;
            }

            std::filesystem::path target = currentPath_;
            if (target.empty() || !ui::project::isNativeLevelSavePath(target, document_.level().kind))
            {
                const std::filesystem::path defaultPath = suggestedOutputPath(
                    target, loadLastOpenLevelDir(), ".sed");

                const QString filename =
                    QFileDialog::getSaveFileName(
                        this,
                        "Save level",
                        fileDialogStartLocation(
                            FileDialogHistoryGroup::openSave,
                            QString::fromStdWString(defaultPath.wstring())),
                        ui::project::levelSaveDialogFilter(document_.level().kind));
                if (filename.isEmpty())
                {
                    return;
                }

                target = std::filesystem::path(filename.toStdWString());
            }

            try
            {
                saveToPath(target, FileWriteKind::nativeDocument);
            }
            catch (const std::exception& e)
            {
                const QString error = QString::fromUtf8(e.what());
                logMessage(MessageSeverity::error,
                    QStringLiteral("Save failed: %1").arg(error));
                showExplicitlyLoggedCriticalMessage(
                    this, QStringLiteral("Save failed"), error);
            }
        });

    auto* saveAsAction = new QAction("Save &As...", this);
    saveAsAction->setObjectName("file.save_as");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, [this, saveToPath]()
        {
            if (!forceNativeDocumentSave_ && resourceEditorOwnsFocus())
            {
                (void)triggerActiveResourceEditorSave(true);
                return;
            }
            const std::filesystem::path defaultPath = suggestedOutputPath(
                currentPath_, loadLastOpenLevelDir(), ".sed");
            const QString filename =
                QFileDialog::getSaveFileName(
                    this,
                    "Save level as",
                    fileDialogStartLocation(
                        FileDialogHistoryGroup::openSave,
                        QString::fromStdWString(defaultPath.wstring())),
                    ui::project::levelSaveDialogFilter(document_.level().kind));
            if (filename.isEmpty())
            {
                return;
            }

            try
            {
                saveToPath(
                    std::filesystem::path(filename.toStdWString()),
                    FileWriteKind::nativeDocument);
            }
            catch (const std::exception& e)
            {
                const QString error = QString::fromUtf8(e.what());
                logMessage(MessageSeverity::error,
                    QStringLiteral("Save failed: %1").arg(error));
                showExplicitlyLoggedCriticalMessage(
                    this, QStringLiteral("Save failed"), error);
            }
        });

    auto chooseEngineExportPath = [this](const std::filesystem::path& preferred)
        -> std::filesystem::path
    {
        const QString suffix = ui::project::nativeRuntimeLevelSuffix(
            document_.level().kind);
        const std::filesystem::path suggestedPath = suggestedOutputPath(
            preferred.empty() ? currentPath_ : preferred,
            loadLastOpenLevelDir(),
            suffix.toStdString());
        const std::filesystem::path defaultPath(
            fileDialogStartLocation(
                FileDialogHistoryGroup::export_,
                QString::fromStdWString(suggestedPath.wstring())).toStdWString());

        // Keep path selection in Qt's native chooser. Runtime export options
        // are presented by a separate modal dialog after a path is accepted.
        QFileDialog chooser(
            this,
            QStringLiteral("Export level"),
            QString::fromStdWString(defaultPath.parent_path().wstring()),
            ui::project::levelExportDialogFilter(document_.level().kind));
        chooser.setAcceptMode(QFileDialog::AcceptSave);
        chooser.setFileMode(QFileDialog::AnyFile);
        chooser.setDefaultSuffix(suffix.mid(1));
        chooser.selectFile(QString::fromStdWString(defaultPath.filename().wstring()));
        if (chooser.exec() != QDialog::Accepted)
        {
            return {};
        }

        const QStringList selectedFiles = chooser.selectedFiles();
        if (selectedFiles.isEmpty() || selectedFiles.front().isEmpty())
        {
            return {};
        }
        const QString filename = selectedFiles.front();
        const QString selectedFilter = chooser.selectedNameFilter();

        std::filesystem::path selected(filename.toStdWString());
        if (selected.extension().empty())
        {
            const bool choseJed = selectedFilter.contains(
                QStringLiteral("*.jed"), Qt::CaseInsensitive)
                && !selectedFilter.contains(QStringLiteral("*.jkl"), Qt::CaseInsensitive);
            selected.replace_extension(choseJed ? ".jed" : suffix.toStdString());
        }

        const QString selectedFilename =
            QString::fromStdWString(selected.wstring());
        if (ui::export_dialog::isRuntimeFilename(
                selectedFilename, document_.level().kind))
        {
            QDialog optionsDialog(this);
            const EngineExportDialogOptions options = addEngineExportOptions(
                optionsDialog,
                engineExportIncludeLights_,
                engineExportIncludeLayers_,
                engineExportCompact_,
                engineExportClampExtraLight_,
                engineExportOmitDefaultMotsArchLighting_,
                engineExportWeldNearbyVertices_,
                engineExportVertexWeldDistance_,
                document_.level().kind);
            if (optionsDialog.exec() != QDialog::Accepted)
            {
                return {};
            }

            engineExportIncludeLights_ = options.lights->isChecked();
            engineExportIncludeLayers_ = options.layers->isChecked();
            engineExportCompact_ = options.compact->isChecked();
            engineExportClampExtraLight_ = options.clampExtraLight->isChecked();
            if (options.omitDefaultMotsArchLighting)
            {
                engineExportOmitDefaultMotsArchLighting_ =
                    options.omitDefaultMotsArchLighting->isChecked();
            }
            engineExportWeldNearbyVertices_ = options.weldNearbyVertices->isChecked();
            engineExportVertexWeldDistance_ =
                ui::export_dialog::normalizedVertexWeldDistance(
                    options.vertexWeldDistance->value());
        }
        rememberFileDialogSelection(
            FileDialogHistoryGroup::export_,
            QString::fromStdWString(selected.wstring()));
        return selected;
    };

    auto exportToPath = [this, saveToPath](const std::filesystem::path& target)
    {
        if (target.empty())
        {
            return;
        }
        try
        {
            saveToPath(target, FileWriteKind::engineExport);
        }
        catch (const std::exception& e)
        {
            const QString error = QString::fromUtf8(e.what());
            logMessage(MessageSeverity::error,
                QStringLiteral("Export failed: %1").arg(error));
            showExplicitlyLoggedCriticalMessage(
                this, QStringLiteral("Export failed"), error);
        }
    };

    auto* exportAction = new QAction("&Export", this);
    exportAction->setObjectName("file.export");
    exportAction->setShortcut(QKeySequence(Qt::ControlModifier | Qt::Key_E));
    connect(exportAction, &QAction::triggered, this,
        [this, chooseEngineExportPath, exportToPath]()
        {
            std::error_code ec{};
            std::filesystem::path target = lastEngineExportPath_;
            const bool reusable = !target.empty()
                && ui::project::isEngineLevelExportPath(
                    target, document_.level().kind)
                && std::filesystem::is_regular_file(target, ec)
                && !ec;
            if (!reusable)
            {
                target = chooseEngineExportPath(target);
            }
            exportToPath(target);
        });

    auto* exportAsAction = new QAction("Export &As...", this);
    exportAsAction->setObjectName("file.export_as");
    exportAsAction->setShortcut(QKeySequence(
        Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_E));
    connect(exportAsAction, &QAction::triggered, this,
        [this, chooseEngineExportPath, exportToPath]()
        {
            exportToPath(chooseEngineExportPath(lastEngineExportPath_));
        });

    auto* mountGobAction = new QAction("&Mount Archive...", this);
    connect(mountGobAction, &QAction::triggered, this, [this]()
        {
            const QString filename =
                QFileDialog::getOpenFileName(
                    this,
                    "Mount archive",
                    fileDialogStartLocation(FileDialogHistoryGroup::import_),
                    "Archives (*.gob *.goo);;All files (*)");
            if (filename.isEmpty())
            {
                return;
            }
            rememberFileDialogSelection(
                FileDialogHistoryGroup::import_, filename);

            try
            {
                const std::filesystem::path gobPath = std::filesystem::path(filename.toStdWString());
                if (std::find(resourceGobs_.begin(), resourceGobs_.end(), gobPath) == resourceGobs_.end())
                {
                    resourceGobs_.push_back(gobPath);
                }

                rebuildResourceLocator(currentPath_);
                glView_->setResourceLocator(&resources_);
                syncCogScriptWatches();
                saveResourceSettings();
                const QString displayPath = ui::nativeDisplayPath(filename);
                statusBar()->showMessage(QString("Mounted %1").arg(displayPath));
                logMessage(MessageSeverity::info, QString("Mounted archive: %1").arg(displayPath));
            }
            catch (const std::exception& e)
            {
                const QString error = QString::fromUtf8(e.what());
                logMessage(MessageSeverity::error,
                    QStringLiteral("Mount archive failed: %1").arg(error));
                showExplicitlyLoggedCriticalMessage(
                    this, QStringLiteral("Mount failed"), error);
            }
        });

    auto* addResourceRootAction = new QAction("Add Resource &Root...", this);
    connect(addResourceRootAction, &QAction::triggered, this, [this]()
        {
            const QString dir = QFileDialog::getExistingDirectory(
                this,
                "Add resource root",
                fileDialogStartLocation(FileDialogHistoryGroup::import_));
            if (dir.isEmpty())
            {
                return;
            }
            rememberFileDialogSelection(
                FileDialogHistoryGroup::import_, dir, true);

            const std::filesystem::path root = std::filesystem::path(dir.toStdWString());
            if (std::find(resourceRoots_.begin(), resourceRoots_.end(), root) == resourceRoots_.end())
            {
                resourceRoots_.push_back(root);
            }

            rebuildResourceLocator(currentPath_);
            glView_->setResourceLocator(&resources_);
            syncCogScriptWatches();
            saveResourceSettings();
            logMessage(MessageSeverity::info, QString("Added resource root: %1").arg(ui::nativeDisplayPath(dir)));
        });

    auto* exitAction = new QAction("E&xit", this);
    exitAction->setObjectName("file.exit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    fileMenu->addMenu(newProjectMenu);
    fileMenu->addAction(openAction);
    fileMenu->addMenu(openRecentMenu);
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);
    fileMenu->addAction(exportAction);
    fileMenu->addAction(exportAsAction);
    fileMenu->addAction(mountGobAction);
    fileMenu->addAction(addResourceRootAction);
    fileMenu->addAction(exitAction);
}

} // namespace sed
