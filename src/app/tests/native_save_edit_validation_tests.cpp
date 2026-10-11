#include "test_common.hpp"

#include "app_logging.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "ui/project/sed_editor_file_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

#include <QAction>
#include <QApplication>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>

namespace sed::app_tests
{
namespace
{

editor::DocumentState saveFixture(const ProjectType project = ProjectType::MOTS)
{
    editor::DocumentState state{};
    state.level.kind = project;
    state.level.header.version = project == ProjectType::IJIM ? 3 : 1;
    state.level.layers = { "Base" };
    return state;
}

QByteArray readBytes(const std::filesystem::path& path)
{
    QFile file(QString::fromStdWString(path.wstring()));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

QByteArray editorPayload(const assets::SedFile& file)
{
    const auto section = std::find_if(file.optionalSections.begin(), file.optionalSections.end(),
        [](const assets::SedOptionalSection& value)
        {
            return value.type == ui::project::SedEditorStateSection;
        });
    return section == file.optionalSections.end() ? QByteArray{} : QByteArray(
        reinterpret_cast<const char*>(section->payload.data()),
        static_cast<qsizetype>(section->payload.size()));
}

QCborArray editorModes(const QByteArray& payload)
{
    return QCborValue::fromCbor(payload).toMap().value(QStringLiteral("edit")).toArray();
}

struct ModalResult
{
    int chooserCount{};
    int errorCount{};
    QString title{};
    QString text{};
    QMessageBox::Icon icon{QMessageBox::NoIcon};
    bool skipsDuplicateLogging{};
};

ModalResult triggerSave(
    QAction* action,
    const std::filesystem::path& chosenPath = {},
    const bool cancelChooser = false)
{
    ModalResult result{};
    QTimer driver{};
    driver.setInterval(5);
    QObject::connect(&driver, &QTimer::timeout, [&]()
    {
        if ( auto* chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()) )
        {
            ++result.chooserCount;
            if ( cancelChooser || result.chooserCount > 1 )
            {
                chooser->reject();
            }
            else
            {
                chooser->setOption(QFileDialog::DontConfirmOverwrite, true);
                chooser->selectFile(QString::fromStdWString(chosenPath.wstring()));
                QMetaObject::invokeMethod(chooser, "accept", Qt::DirectConnection);
            }
        }
        else if ( auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            ++result.errorCount;
            result.title = message->windowTitle();
            result.text = message->text();
            result.icon = message->icon();
            result.skipsDuplicateLogging = message->property(kMessageBoxLoggingSkipProperty).toBool();
            message->accept();
        }
    });
    driver.start();
    if ( action )
    {
        action->trigger();
    }
    driver.stop();
    return result;
}

int editErrorCount(const MainWindow& window)
{
    const auto* messages = window.findChild<QTreeWidget*>(QStringLiteral("messagesTree"));
    int count{};
    if ( messages )
    {
        for ( int index = 0; index < messages->topLevelItemCount(); ++index )
        {
            const QTreeWidgetItem* item = messages->topLevelItem(index);
            if ( item && item->text(0) == QStringLiteral("Error")
                && item->text(1).contains(QStringLiteral("EDIT payload failed exact validation")) )
            {
                ++count;
            }
        }
    }
    return count;
}

} // namespace

void runNativeSaveEditValidationTests()
{
    QTemporaryDir directory{};
    check(directory.isValid(), "native Save EDIT tests use isolated files and recovery storage");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());

    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setRecoveryDirectoryForTest(root / "mode-recovery");
        window.setDocumentStateForTest(saveFixture());
        window.activateArchitecturalLightingEditModeForTest();
        const QByteArray payload = window.captureRecoveryEditorStateForTest();
        const QCborArray modes = editorModes(payload);
        check(modes.size() == 3 && modes.at(2).toInteger() == 3
                && window.architecturalLightingEditModeActiveForTest(),
            "native Save captures the supported architectural Light mode as EDIT value three");
        QString warning{};
        check(window.validateRecoveryEditorStateForTest(payload, &warning) && warning.isEmpty(),
            "native Save exact EDIT validation accepts its own architectural Light mode capture");

        const QByteArray beforeValidation = window.captureRecoveryEditorStateForTest();
        const QCborMap base = QCborValue::fromCbor(payload).toMap();
        for ( int editMode = 0; editMode <= window.frameEditModeValueForTest(); ++editMode )
        {
            for ( int surfaceMode = 0; surfaceMode <= 2; ++surfaceMode )
            {
                for ( int lightMode = 0; lightMode <= 3; ++lightMode )
                {
                    QCborMap candidate = base;
                    candidate.insert(QStringLiteral("edit"), QCborArray{ editMode, surfaceMode, lightMode });
                    warning.clear();
                    check(window.validateRecoveryEditorStateForTest(
                            ui::recovery::serializeCanonicalCbor(QCborValue(candidate)), &warning)
                            && warning.isEmpty(),
                        "exact EDIT validation accepts every supported edit, Surface, and Light mode value");
                }
            }
        }
        check(window.captureRecoveryEditorStateForTest() == beforeValidation,
            "exact EDIT validation does not apply any candidate modes or workspace fields");

        const std::array<int, 3> upper{ window.frameEditModeValueForTest(), 2, 3 };
        for ( qsizetype modeIndex = 0; modeIndex < 3; ++modeIndex )
        {
            const std::array<QCborValue, 6> invalid{
                QCborValue(-1), QCborValue(upper[static_cast<std::size_t>(modeIndex)] + 1),
                QCborValue(std::numeric_limits<qint64>::max()), QCborValue(true),
                QCborValue(0.5), QCborValue(QStringLiteral("0")) };
            for ( const QCborValue& value : invalid )
            {
                QCborMap candidate = base;
                QCborArray changedModes = modes;
                changedModes[modeIndex] = value;
                candidate.insert(QStringLiteral("edit"), changedModes);
                const QByteArray invalidPayload = ui::recovery::serializeCanonicalCbor(QCborValue(candidate));
                warning.clear();
                check(!window.validateRecoveryEditorStateForTest(invalidPayload, &warning)
                        && warning == QStringLiteral(
                            "The optional editor modes are invalid; the level was still recovered."),
                    "exact EDIT validation rejects negative, excessive, and noninteger mode values");
                check(!window.restoreRecoveryEditorStateForTest(invalidPayload)
                        && window.captureRecoveryEditorStateForTest() == beforeValidation,
                    "invalid EDIT modes reject the entire restore without changing the workspace");
            }
        }
    }

    for ( const ProjectType project : { ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM } )
    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setRecoveryDirectoryForTest(root / "project-mode-recovery");
        window.setDocumentStateForTest(saveFixture(project));
        window.activateBakedLightEditModeForTest();
        const QCborMap base = QCborValue::fromCbor(window.captureRecoveryEditorStateForTest()).toMap();
        for ( int lightMode = 0; lightMode <= 3; ++lightMode )
        {
            QCborMap candidate = base;
            QCborArray modes = base.value(QStringLiteral("edit")).toArray();
            modes[2] = lightMode;
            candidate.insert(QStringLiteral("edit"), modes);
            const QByteArray payload = ui::recovery::serializeCanonicalCbor(QCborValue(candidate));
            const int expectedMode = (lightMode == 2 && project != ProjectType::IJIM)
                    || (lightMode == 3 && project != ProjectType::MOTS)
                ? 0 : lightMode;
            check(window.validateRecoveryEditorStateForTest(payload)
                    && window.restoreRecoveryEditorStateForTest(payload)
                    && editorModes(window.captureRecoveryEditorStateForTest()).at(2).toInteger()
                        == expectedMode,
                "shared EDIT restore retains supported project Light modes and falls back to Baked for inapplicable modes");
        }
        window.activateArchitecturalLightingEditModeForTest();
        check(editorModes(window.captureRecoveryEditorStateForTest()).at(2).toInteger()
                == (project == ProjectType::MOTS ? 3 : 0),
            "live architectural Light mode remains applicable only to MotS");
    }

    {
        const std::filesystem::path recoveryRoot = root / "architectural-autosave";
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setRecoveryDirectoryForTest(recoveryRoot);
        window.setDocumentStateForTest(saveFixture());
        window.beginRecoveryDocumentForTest(root / "architectural-import.jkl", false);
        window.activateArchitecturalLightingEditModeForTest();
        window.pushUndoForTitleForTest();
        window.mutableDocumentStateForTest().level.header.gravity = 3.125;
        window.triggerRecoverySnapshotForTest();
        window.waitForRecoverySnapshotForTest();
        const std::filesystem::path livePath = window.currentRecoveryPathForTest();
        const auto live = ui::recovery::readRecoveryRecord(livePath);
        check(live && live->decodedLevel && live->decodedLevel->kind == ProjectType::MOTS
                && live->decodedLevel->header.gravity == 3.125
                && live->metadata.requiresUserRecovery
                && editorModes(live->editorStatePayload).at(2).toInteger() == 3,
            "live recovery publishes an authored MotS document with architectural EDIT mode");

        window.pushUndoForTitleForTest();
        window.mutableDocumentStateForTest().level.header.gravity = 6.25;
        window.triggerPeriodicRecoverySnapshotForTest();
        window.waitForRecoverySnapshotForTest();
        const auto autosaves = ui::recovery::scanAutosaveHistory(recoveryRoot);
        check(autosaves.size() == 1,
            "periodic architectural recovery archives the preceding authored checkpoint once");
        if ( autosaves.size() == 1 )
        {
            const std::filesystem::path autosavePath = autosaves.front().recoveryPath;
            const QByteArray autosaveBytes = readBytes(autosavePath);
            const auto autosave = ui::recovery::readRecoveryRecord(autosavePath);
            const auto latest = ui::recovery::readRecoveryRecord(livePath);
            check(autosave && autosave->decodedLevel && latest && latest->decodedLevel
                    && autosave->decodedLevel->header.gravity == 3.125
                    && latest->decodedLevel->header.gravity == 6.25
                    && editorModes(autosave->editorStatePayload).at(2).toInteger() == 3
                    && editorModes(latest->editorStatePayload).at(2).toInteger() == 3,
                "autosave and its newer live recovery share exact architectural EDIT admission while retaining separate Level states");

            MainWindow restored{};
            restored.setAttribute(Qt::WA_DontShowOnScreen);
            restored.setRecoveryDirectoryForTest(root / "restored-architectural-recovery");
            restored.setDocumentStateForTest(saveFixture(ProjectType::JKDF2));
            check(restored.applyRecoveryFileForTest(autosavePath)
                    && restored.documentStateForTest().level.kind == ProjectType::MOTS
                    && restored.documentStateForTest().level.header.gravity == 3.125
                    && restored.architecturalLightingEditModeActiveForTest()
                    && restored.hasUnsavedChangesForTest() && readBytes(autosavePath) == autosaveBytes,
                "restoring an architectural autosave applies MotS Level plus EDIT and preserves the historical file for explicit Save");
            restored.waitForRecoverySnapshotForTest();
            restored.discardCurrentRecoveryForTest();
        }
        window.discardCurrentRecoveryForTest();
    }

    {
        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setRecoveryDirectoryForTest(root / "save-recovery");
        window.setDocumentStateForTest(saveFixture());
        window.activateArchitecturalLightingEditModeForTest();
        window.pushUndoForTitleForTest();
        window.mutableDocumentStateForTest().level.header.gravity = 3.125;
        const std::filesystem::path savePath = root / "architectural.sed";
        window.setCurrentPathForTitleForTest(savePath);
        QAction* save = window.findChild<QAction*>(QStringLiteral("file.save"));
        const ModalResult saved = triggerSave(save);
        check(save && saved.errorCount == 0 && !readBytes(savePath).isEmpty()
                && !window.hasUnsavedChangesForTest(),
            "native Save writes architectural Light mode and marks the unchanged authored document saved");
        if ( !readBytes(savePath).isEmpty() )
        {
            const assets::SedFile file = assets::loadSedFile(savePath);
            check(file.level.kind == ProjectType::MOTS && file.level.header.gravity == 3.125
                    && editorModes(editorPayload(file)).at(2).toInteger() == 3,
                "native SED retains authored Level data and architectural EDIT mode together");
            window.activateBakedLightEditModeForTest();
            check(window.restoreRecoveryEditorStateForTest(editorPayload(file))
                    && window.architecturalLightingEditModeActiveForTest(),
                "native SED architectural EDIT restores through the shared exact workspace parser");
        }
        const std::filesystem::path saveAsPath = root / "architectural-save-as.sed";
        QAction* saveAs = window.findChild<QAction*>(QStringLiteral("file.save_as"));
        const ModalResult savedAs = triggerSave(saveAs, saveAsPath);
        check(saveAs && savedAs.chooserCount == 1 && savedAs.errorCount == 0
                && !readBytes(saveAsPath).isEmpty() && window.currentPathForTest() == saveAsPath,
            "native Save As accepts architectural EDIT and publishes the chosen native document path");

        const QByteArray savedBytes = readBytes(saveAsPath);
        const std::int64_t savedTimestamp = window.nativeDocumentTimestampForTest();
        window.mutableDocumentStateForTest().level.layers.push_back("Unrepresented layer");
        window.pushUndoForTitleForTest();
        const std::size_t undoDepth = window.undoDepthForTest();
        const QString expectedError = QStringLiteral(
            "Cannot save the Sith Editor file because its EDIT payload failed exact validation: "
            "The recovered layer arrays do not match the current level; the level was still recovered.");
        const int errorsBefore = editErrorCount(window);
        const ModalResult failed = triggerSave(save);
        check(failed.errorCount == 1 && failed.icon == QMessageBox::Critical
                && failed.title == QStringLiteral("Save failed")
                && failed.text == expectedError
                && failed.skipsDuplicateLogging && editErrorCount(window) == errorsBefore + 1,
            "rejected native Save shows one critical error dialog and one explicit Messages error");
        check(window.hasUnsavedChangesForTest() && window.currentPathForTest() == saveAsPath
                && window.nativeDocumentTimestampForTest() == savedTimestamp
                && window.undoDepthForTest() == undoDepth && readBytes(saveAsPath) == savedBytes,
            "rejected native Save preserves existing file, native path, timestamp, history, and dirty state");
        const std::filesystem::path rejectedPath = root / "rejected-save-as.sed";
        const ModalResult failedAs = triggerSave(saveAs, rejectedPath);
        check(failedAs.chooserCount == 1 && failedAs.errorCount == 1
                && failedAs.icon == QMessageBox::Critical
                && failedAs.title == QStringLiteral("Save failed")
                && failedAs.text == expectedError && failedAs.skipsDuplicateLogging
                && editErrorCount(window) == errorsBefore + 2
                && window.currentPathForTest() == saveAsPath && window.hasUnsavedChangesForTest()
                && !std::filesystem::exists(rejectedPath) && readBytes(saveAsPath) == savedBytes,
            "rejected native Save As shows one error and leaves both old and proposed destinations untouched");
        const int errorsBeforeCancel = editErrorCount(window);
        const ModalResult cancelled = triggerSave(saveAs, rejectedPath, true);
        check(cancelled.chooserCount == 1 && cancelled.errorCount == 0
                && editErrorCount(window) == errorsBeforeCancel
                && window.currentPathForTest() == saveAsPath && window.hasUnsavedChangesForTest()
                && !std::filesystem::exists(rejectedPath),
            "cancelling native Save As remains quiet and preserves the dirty document and paths");
    }
}

} // namespace sed::app_tests
