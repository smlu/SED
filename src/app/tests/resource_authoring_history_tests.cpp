#include "test_common.hpp"

#include "main_window/main_window.hpp"
#include "native_caption_test_utils.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/sed.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/cog/cog_document_history.hpp"
#include "ui/project/sed_editor_file_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"
#include "ui/resources/resource_authoring_editor.hpp"

#include <QApplication>
#include <QAbstractButton>
#include <QAction>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QCryptographicHash>
#include <QDialog>
#include <QDebug>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sed::app_tests
{
namespace
{

using ui::resources::ResourceAuthoringKind;

void bindHistoryProject(MainWindow& window)
{
    editor::DocumentState document{};
    document.level.kind = ProjectType::JKDF2;
    document.level.header.version = 1;
    document.level.layers = {"Base"};
    window.setDocumentStateForTest(std::move(document));
    window.setProjectTypeForTest(ProjectType::JKDF2);
}

bool writeHistoryBytes(const std::filesystem::path& path, const QByteArray& bytes)
{
    std::error_code error{};
    std::filesystem::create_directories(path.parent_path(), error);
    if ( error )
    {
        return false;
    }
    QFile file(QString::fromStdWString(path.wstring()));
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray readHistoryBytes(const std::filesystem::path& path)
{
    QFile file(QString::fromStdWString(path.wstring()));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

QCborMap resourceContent(const QWidget* pane)
{
    const QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane);
    QCborMap content{};
    for ( const QString& key : {QStringLiteral("sprite"), QStringLiteral("spritePendingNumericEdits"),
                              QStringLiteral("rows"), QStringLiteral("capacity")} )
    {
        if ( state.contains(key) )
        {
            content.insert(key, state.value(key));
        }
    }
    return content;
}

std::unique_ptr<QWidget> historyPane(
    const ResourceAuthoringKind kind,
    const ProjectType project,
    const std::filesystem::path& directory = {})
{
    ui::resources::ResourceAuthoringContext context{};
    context.projectType = project;
    context.projectDirectory = directory;
    context.startingDirectory = directory;
    context.resources = std::make_shared<assets::ResourceLocator>();
    std::unique_ptr<QWidget> pane(ui::resources::createResourceAuthoringPane(nullptr, kind, context));
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
    state.remove(QStringLiteral("history")); // This fixture defines an independent clean baseline.
    state.remove(QStringLiteral("editorPresentation")); // Its new authored baseline has no inherited caret.
    state.insert(QStringLiteral("modified"), false);
    if ( kind == ResourceAuthoringKind::sprite )
    {
        QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
        sprite[0] = QStringLiteral("old.mat");
        sprite[2] = QStringLiteral("1.000000013351432e-12");
        sprite[3] = QStringLiteral("2");
        state.insert(QStringLiteral("sprite"), sprite);
    }
    else if ( kind == ResourceAuthoringKind::soundClass )
    {
        state.insert(QStringLiteral("rows"), QCborArray{QCborValue(QCborArray{
            QStringLiteral("moving"), QStringLiteral("old.wav"), QStringLiteral("0x80000040"),
            QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("1")})});
        state.insert(QStringLiteral("soundPreviewLoops"), QCborArray{false});
    }
    else
    {
        state.insert(QStringLiteral("rows"), QCborArray{QCborValue(QCborArray{
            QStringLiteral("CaseKey"), QStringLiteral("old text"), QStringLiteral("-2147483648")})});
        state.insert(QStringLiteral("capacity"), 4);
    }
    QString warning{};
    const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning);
    if ( !restored )
    {
        qWarning().noquote() << "History fixture" << static_cast<int>(project) << static_cast<int>(kind) << warning;
    }
    check(restored,
          "document history fixture restores a precise clean source draft");
    return pane;
}

void changeHistoryField(QWidget* pane, const ResourceAuthoringKind kind, const QString& text)
{
    if ( kind == ResourceAuthoringKind::sprite )
    {
        if ( QLineEdit* const material = pane->findChild<QLineEdit*>(QStringLiteral("spr.material")) )
        {
            material->setText(text);
        }
    }
    else if ( kind == ResourceAuthoringKind::soundClass )
    {
        if ( QTreeWidget* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) )
        {
            if ( tree->topLevelItemCount() && tree->topLevelItem(0)->childCount() )
            {
                for ( int group = 0; group < tree->topLevelItemCount(); ++group )
                {
                    QTreeWidgetItem* const mode = tree->topLevelItem(group);
                    for ( int child = 0; child < mode->childCount(); ++child )
                    {
                        QTreeWidgetItem* const item = mode->child(child);
                        if ( item->text(1).endsWith(QStringLiteral(".wav")) )
                        {
                            item->setText(1, text);
                            return;
                        }
                    }
                }
            }
        }
    }
    else if ( QTableWidget* const table = pane->findChild<QTableWidget*>(QStringLiteral("uni.entries")) )
    {
        if ( table->rowCount() && table->item(0, 1) )
        {
            table->item(0, 1)->setText(text);
        }
    }
}

bool resourceUndo(QWidget* pane, const bool redo)
{
    if ( QWidget* const focused = QApplication::focusWidget() )
    {
        focused->clearFocus();
    }
    return ui::resources::resourceAuthoringPaneUndoRedo(pane, redo);
}

// Checks the core's opaque section transport. The independent aggregate
// workspace case below also validates the real MainWindow EDIT schema.
QCborMap nativeHistoryRoundTrip(const QCborMap& state, const ProjectType project)
{
    Level level{};
    level.kind = project;
    level.header.version = project == ProjectType::IJIM ? 3 : 1;
    level.layers = {"Base"};
    const QByteArray payload = QCborValue(state).toCbor();
    assets::SedOptionalSection section{};
    section.type = ui::project::SedEditorStateSection;
    section.payload.assign(reinterpret_cast<const std::byte*>(payload.constData()),
                           reinterpret_cast<const std::byte*>(payload.constData()) + payload.size());
    const std::array<assets::SedOptionalSection, 1> sections{section};
    const assets::SedFile decoded = assets::loadSedFileBytes(assets::saveToSedBytes(level, sections));
    check(decoded.optionalSections.size() == 1,
          "document history bytes survive the checked opaque native section transport");
    if ( decoded.optionalSections.size() != 1 )
    {
        return {};
    }
    const auto& bytes = decoded.optionalSections.front().payload;
    return QCborValue::fromCbor(QByteArray(reinterpret_cast<const char*>(bytes.data()),
                                         static_cast<qsizetype>(bytes.size()))).toMap();
}

void checkResourceHistoryRoundTrip(const ResourceAuthoringKind kind, const ProjectType project)
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    auto pane = historyPane(kind, project, root);
    const QCborMap initial = resourceContent(pane.get());
    const QString middle = kind == ResourceAuthoringKind::sprite ? QStringLiteral("middle.mat")
                         : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("middle.wav")
                                                                   : QStringLiteral("middle text");
    const QString final = kind == ResourceAuthoringKind::sprite ? QStringLiteral("final.mat")
                        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("final.wav")
                                                                  : QStringLiteral("final text");
    changeHistoryField(pane.get(), kind, middle);
    const QCborMap saved = resourceContent(pane.get());
    // A selected destination is explicit here so the archive-specific Save
    // policy below is tested independently from the saved clean marker.
    int chooserCount{};
    QTimer modal{};
    QObject::connect(&modal, &QTimer::timeout, [&]()
    {
        if ( auto* const dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()) )
        {
            ++chooserCount;
            dialog->setOption(QFileDialog::DontConfirmOverwrite, true);
            const QString suffix = kind == ResourceAuthoringKind::sprite ? QStringLiteral("spr")
                                 : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("snd")
                                                                           : QStringLiteral("uni");
            dialog->selectFile(QString::fromStdWString((root / ("saved." + suffix.toStdString())).wstring()));
            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        }
        else if ( auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            message->reject();
        }
    });
    modal.start(5);
    const bool externalSaved = ui::resources::resourceAuthoringPaneSave(pane.get(), true);
    modal.stop();
    check(externalSaved && chooserCount == 1 && !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "resource document history establishes a real external saved position");
    changeHistoryField(pane.get(), kind, final);
    const QCborMap changed = resourceContent(pane.get());
    check(resourceUndo(pane.get(), false) && resourceContent(pane.get()) == saved
              && !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "SPR SND and UNI document Undo returns to the exact clean saved position");
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap history = captured.value(QStringLiteral("history")).toMap();
    check(history.value(QStringLiteral("version")).toInteger(-1) == 1
              && history.value(QStringLiteral("cursor")).toInteger(-1) == 1
              && history.value(QStringLiteral("clean")).toInteger(-1) == 1
              && history.value(QStringLiteral("commands")).toArray().size() == 2,
          "native resource history stores its actual mid-timeline cursor clean marker and redo branch");
    auto restored = historyPane(kind, project, root);
    QString warning{};
    const QCborMap transported = nativeHistoryRoundTrip(captured, project);
    check(transported == captured && ui::resources::restoreResourceAuthoringPaneState(restored.get(), transported, &warning)
              && warning.isEmpty() && resourceContent(restored.get()) == saved,
          "native resource document history and exact current raw values restore together");
    check(resourceUndo(restored.get(), true) && resourceContent(restored.get()) == changed
              && ui::resources::resourceAuthoringPaneIsModified(restored.get()),
          "restored resource Redo applies the exact previously undone command and becomes dirty");
    check(resourceUndo(restored.get(), false) && resourceContent(restored.get()) == saved
              && !ui::resources::resourceAuthoringPaneIsModified(restored.get()),
          "restored resource Undo preserves the external saved clean position");
    check(resourceUndo(restored.get(), false) && resourceContent(restored.get()) == initial
              && ui::resources::resourceAuthoringPaneIsModified(restored.get()),
          "restored resource history reaches the exact imported baseline without confusing it with saved content");
    check(resourceUndo(restored.get(), true) && resourceContent(restored.get()) == saved,
          "restored resource history can return forward to its saved checkpoint");
    changeHistoryField(restored.get(), kind, kind == ResourceAuthoringKind::sprite ? QStringLiteral("branch.mat")
                                             : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("branch.wav")
                                                                                       : QStringLiteral("branch text"));
    const QCborMap branch = resourceContent(restored.get());
    (void)resourceUndo(restored.get(), true);
    const QCborMap branchHistory = ui::resources::captureResourceAuthoringPaneState(restored.get())
                                      .value(QStringLiteral("history")).toMap();
    check(resourceContent(restored.get()) == branch && branchHistory.value(QStringLiteral("commands")).toArray().size() == 2
              && branchHistory.value(QStringLiteral("cursor")).toInteger(-1) == 2,
          "a new resource edit after Undo truncates the old redo branch without losing the clean checkpoint");
    check(ui::resources::resourceAuthoringPaneSave(restored.get(), false),
          "resource history saves the later command as a real clean checkpoint");
    (void)resourceUndo(restored.get(), false);
    const QCborMap futureClean = ui::resources::captureResourceAuthoringPaneState(restored.get());
    auto future = historyPane(kind, project, root);
    check(futureClean.value(QStringLiteral("history")).toMap().value(QStringLiteral("clean")).toInteger(-1) == 2 &&
              ui::resources::restoreResourceAuthoringPaneState(future.get(), futureClean) &&
              ui::resources::resourceAuthoringPaneIsModified(future.get()) && resourceUndo(future.get(), true) &&
              resourceContent(future.get()) == branch && !ui::resources::resourceAuthoringPaneIsModified(future.get()),
          "native resource history retains a clean checkpoint in Redo until a new edit actually discards it");
}

void checkPendingSpriteHistory(const ProjectType project)
{
    auto pane = historyPane(ResourceAuthoringKind::sprite, project);
    const QCborMap initial = resourceContent(pane.get());
    auto* const width = pane->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"));
    check(width != nullptr, "pending document history finds the real Sprite width editor");
    if ( !width )
    {
        return;
    }
    width->lineEdit()->setText(QStringLiteral("1e"));
    QMetaObject::invokeMethod(width->lineEdit(), "textEdited", Qt::DirectConnection,
                              Q_ARG(QString, QStringLiteral("1e")));
    const QCborMap beforeCapture = resourceContent(pane.get());
    const QString editorBefore = width->lineEdit()->text();
    const int cursorBefore = width->lineEdit()->cursorPosition();
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(pane.get());
    check(resourceContent(pane.get()) == beforeCapture && width->lineEdit()->text() == editorBefore
              && width->lineEdit()->cursorPosition() == cursorBefore
              && captured.value(QStringLiteral("spritePendingNumericEdits")).toMap().value(QStringLiteral("width"))
                  == QCborValue(QStringLiteral("1e")),
          "native history capture preserves invalid pending Sprite text without committing or reformatting it");
    auto restored = historyPane(ResourceAuthoringKind::sprite, project);
    check(ui::resources::restoreResourceAuthoringPaneState(restored.get(), nativeHistoryRoundTrip(captured, project)),
          "native history admits an invalid pending numeric gesture alongside the exact finite Sprite model");
    check(resourceUndo(restored.get(), false) && resourceContent(restored.get()) == initial
              && !ui::resources::resourceAuthoringPaneIsModified(restored.get()),
          "document Undo after restore reverses an unfinished invalid numeric gesture and retains tiny canonical floats");
    check(resourceUndo(restored.get(), true) && resourceContent(restored.get()) == beforeCapture
              && ui::resources::resourceAuthoringPaneIsModified(restored.get()),
          "document Redo after restore returns the exact invalid pending numeric text");
}

bool saveHistoryFixture(QWidget* pane, const std::filesystem::path& path)
{
    int chooserCount{};
    QTimer modal{};
    QObject::connect(&modal, &QTimer::timeout, [&]()
    {
        if ( auto* const dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()) )
        {
            ++chooserCount;
            dialog->setOption(QFileDialog::DontConfirmOverwrite, true);
            dialog->selectFile(QString::fromStdWString(path.wstring()));
            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        }
        else if ( auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            message->reject();
        }
    });
    modal.start(5);
    const bool saved = ui::resources::resourceAuthoringPaneSave(pane, true);
    modal.stop();
    return saved && chooserCount == 1;
}

void checkSpriteNumericHistoryGestureBoundary(const ProjectType project)
{
    for ( const bool saveBoundary : {false, true} )
    {
        QTemporaryDir temporary{};
        const std::filesystem::path root(temporary.path().toStdWString());
        auto pane = historyPane(ResourceAuthoringKind::sprite, project, root);
        QCborMap baseline = ui::resources::captureResourceAuthoringPaneState(pane.get());
        baseline.remove(QStringLiteral("history")); // Independent numeric gesture baseline.
        QCborArray sprite = baseline.value(QStringLiteral("sprite")).toArray();
        sprite[2] = QStringLiteral("0.1");
        baseline.insert(QStringLiteral("sprite"), sprite);
        check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), baseline),
              "Sprite numeric history boundary restores its independent fixed baseline");
        auto* const width = pane->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"));
        if ( !width )
        {
            check(false, "Sprite numeric history boundary finds the actual width line");
            continue;
        }
        const auto typeWidth = [width](const QString& text)
        {
            width->lineEdit()->setText(text);
            QMetaObject::invokeMethod(width->lineEdit(), "textEdited", Qt::DirectConnection, Q_ARG(QString, text));
        };
        typeWidth(QStringLiteral("0.2"));
        if ( saveBoundary )
        {
            check(saveHistoryFixture(pane.get(), root / "gesture.spr")
                      && !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
                  "Sprite numeric history saves and completes the first typed gesture");
        }
        else
        {
            QMetaObject::invokeMethod(width, "valueCommitted", Qt::DirectConnection, Q_ARG(double, 0.2));
            QMetaObject::invokeMethod(width->lineEdit(), "editingFinished", Qt::DirectConnection);
        }
        const QCborMap first = resourceContent(pane.get());
        const QCborMap firstHistory = ui::resources::captureResourceAuthoringPaneState(pane.get())
                                          .value(QStringLiteral("history")).toMap();
        check(first.value(QStringLiteral("sprite")).toArray().at(2) == QCborValue(QStringLiteral("0.2"))
                  && !first.contains(QStringLiteral("spritePendingNumericEdits"))
                  && firstHistory.value(QStringLiteral("commands")).toArray().size() == 1,
              "Sprite first numeric gesture commits the exact binary32 value as one command");
        typeWidth(QStringLiteral("0.3"));
        const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(pane.get());
        const QCborMap history = captured.value(QStringLiteral("history")).toMap();
        check(ui::resources::resourceAuthoringPaneIsModified(pane.get())
                  && history.value(QStringLiteral("commands")).toArray().size() == 2
                  && history.value(QStringLiteral("cursor")).toInteger(-1) == 2
                  && history.value(QStringLiteral("clean")).toInteger(-1) == (saveBoundary ? 1 : 0)
                  && ui::resources::validateResourceAuthoringPaneState(captured),
              "typing the same Sprite field after Save or focus commit begins a new dirty history command");
        auto restored = historyPane(ResourceAuthoringKind::sprite, project, root);
        check(ui::resources::restoreResourceAuthoringPaneState(restored.get(), captured)
                  && resourceUndo(restored.get(), false) && resourceContent(restored.get()) == first
                  && ui::resources::resourceAuthoringPaneIsModified(restored.get()) == !saveBoundary,
              "restored Sprite Undo crosses only the second numeric gesture and retains the exact saved clean value");
    }
}

void checkSoundPendingHistoryDiscardsSavedRedo(const ProjectType project)
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    auto pane = historyPane(ResourceAuthoringKind::soundClass, project, root);
    QCborMap baseline = ui::resources::captureResourceAuthoringPaneState(pane.get());
    baseline.remove(QStringLiteral("history")); // Independent saved-future volume vector.
    baseline.insert(QStringLiteral("rows"), QCborArray{QCborValue(QCborArray{
        QStringLiteral("moving"), QStringLiteral("old.wav"), QStringLiteral("0x80000040"),
        QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("0.25")})});
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), baseline),
          "Sound pending branch restores the exact initial volume quarter");
    auto* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    QTreeWidgetItem* row{};
    if ( tree )
    {
        for ( int group = 0; group < tree->topLevelItemCount(); ++group )
        {
            QTreeWidgetItem* const mode = tree->topLevelItem(group);
            for ( int child = 0; child < mode->childCount(); ++child )
            {
                if ( mode->child(child)->text(1) == QStringLiteral("old.wav") )
                {
                    row = mode->child(child);
                }
            }
        }
    }
    if ( !row )
    {
        check(false, "Sound pending branch finds the actual source alternative");
        return;
    }
    row->setText(5, QStringLiteral("0.5"));
    row->setText(5, QStringLiteral("0.75"));
    check(saveHistoryFixture(pane.get(), root / "pending.snd") && resourceUndo(pane.get(), false)
              && resourceContent(pane.get()).value(QStringLiteral("rows")).toArray().at(0).toArray().at(5)
                  == QCborValue(QStringLiteral("0.5")),
          "Sound pending branch saves the second command and undoes to the precise half-volume command");
    // Use the real active compound delegate. Capture must include this raw
    // pending value without committing it or altering the live redo branch.
    pane->setAttribute(Qt::WA_DontShowOnScreen);
    pane->resize(1000, 650);
    pane->show();
    tree->setAnimated(false);
    row->parent()->setExpanded(true);
    tree->setCurrentItem(row, 5);
    tree->editItem(row, 5);
    QApplication::processEvents(QEventLoop::AllEvents);
    QWidget* const delegate = tree->indexWidget(tree->indexFromItem(row, 5));
    QLineEdit* const line = delegate ? delegate->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) : nullptr;
    if ( !line )
    {
        check(false, "Sound pending branch opens the real uncommitted volume editor");
        return;
    }
    line->setText(QStringLiteral("0.6"));
    QMetaObject::invokeMethod(line, "textEdited", Qt::DirectConnection, Q_ARG(QString, QStringLiteral("0.6")));
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap history = captured.value(QStringLiteral("history")).toMap();
    check(line->text() == QStringLiteral("0.6") && row->text(5) == QStringLiteral("0.5")
              && captured.value(QStringLiteral("modified")).toBool()
              && history.value(QStringLiteral("cursor")).toInteger(-1) == 2
              && history.value(QStringLiteral("clean")).toInteger(-2) == -1
              && history.value(QStringLiteral("cleanAuthored")).toInteger(-2) == -1
              && ui::resources::validateResourceAuthoringPaneState(captured),
          "capturing pending Sound text invalidates the discarded saved authored checkpoint without committing the live editor");
    auto restored = historyPane(ResourceAuthoringKind::soundClass, project, root);
    check(ui::resources::restoreResourceAuthoringPaneState(restored.get(), captured)
              && ui::resources::resourceAuthoringPaneIsModified(restored.get())
              && resourceUndo(restored.get(), false)
              && resourceContent(restored.get()).value(QStringLiteral("rows")).toArray().at(0).toArray().at(5)
                  == QCborValue(QStringLiteral("0.5"))
              && ui::resources::resourceAuthoringPaneIsModified(restored.get()),
          "native Sound pending branch restores dirty and Undo retains the exact earlier authored volume");
}

void checkSoundEmptyModeHistory(const ProjectType project)
{
    auto pane = historyPane(ResourceAuthoringKind::soundClass, project);
    auto* const add = pane->findChild<QPushButton*>(QStringLiteral("snd.add_mode"));
    check(add != nullptr, "native Sound history finds the real Add sound mode action");
    if ( !add )
    {
        return;
    }
    bool selected{};
    QTimer modal{};
    QObject::connect(&modal, &QTimer::timeout, [&]()
    {
        auto* const popup = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        auto* const list = popup ? popup->findChild<QListWidget*>(QStringLiteral("snd.mode.list")) : nullptr;
        if ( list )
        {
            const auto items = list->findItems(QStringLiteral("create"), Qt::MatchExactly);
            if ( !items.isEmpty() )
            {
                list->setCurrentItem(items.front());
                selected = true;
                popup->accept();
            }
            else
            {
                popup->reject();
            }
        }
    });
    modal.start(5);
    add->click();
    modal.stop();
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborArray emptyModes = captured.value(QStringLiteral("soundEmptyModes")).toArray();
    check(selected && emptyModes == QCborArray{QStringLiteral("create")}
              && !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "an empty Sound mode is an undoable native presentation command without external dirtying");
    auto restored = historyPane(ResourceAuthoringKind::soundClass, project);
    check(ui::resources::restoreResourceAuthoringPaneState(restored.get(), nativeHistoryRoundTrip(captured, project)),
          "native Sound history transports the empty-mode command alongside source rows");
    check(resourceUndo(restored.get(), false)
              && ui::resources::captureResourceAuthoringPaneState(restored.get()).value(QStringLiteral("soundEmptyModes")).toArray().isEmpty()
              && !ui::resources::resourceAuthoringPaneIsModified(restored.get()),
          "restored Sound Undo removes the empty mode without touching authored rows or clean state");
    check(resourceUndo(restored.get(), true)
              && ui::resources::captureResourceAuthoringPaneState(restored.get()).value(QStringLiteral("soundEmptyModes")).toArray() == emptyModes
              && !ui::resources::resourceAuthoringPaneIsModified(restored.get()),
          "restored Sound Redo returns the exact empty mode independently of the authored clean marker");
}

void checkHistoryValidationAtomicity()
{
    auto pane = historyPane(ResourceAuthoringKind::stringTable, ProjectType::IJIM);
    changeHistoryField(pane.get(), ResourceAuthoringKind::stringTable, QStringLiteral("next text"));
    (void)resourceUndo(pane.get(), false);
    const QCborMap before = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap validHistory = before.value(QStringLiteral("history")).toMap();
    check(ui::resources::validateResourceAuthoringPaneState(before), "malformed history vectors start from a valid real timeline");
    const auto reject = [&](QCborMap history)
    {
        QCborMap malformed = before;
        malformed.insert(QStringLiteral("history"), history);
        QString warning{};
        const bool valid = ui::resources::validateResourceAuthoringPaneState(malformed, &warning);
        const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), malformed, &warning);
        check(!valid && !restored && warning == QStringLiteral("Resource document history is invalid.")
                  && ui::resources::captureResourceAuthoringPaneState(pane.get()) == before,
              "malformed or over-budget resource history is rejected before changing any pane state with an exact warning");
    };
    QCborMap bad = validHistory;
    bad.insert(QStringLiteral("version"), QStringLiteral("1"));
    reject(bad);
    bad = validHistory;
    bad.insert(QStringLiteral("cursor"), -1);
    reject(bad);
    if ( validHistory.value(QStringLiteral("commands")).toArray().isEmpty() )
    {
        check(false, "malformed history vectors require an actual committed command");
        return;
    }
    const QCborMap command = validHistory.value(QStringLiteral("commands")).toArray().at(0).toMap();
    QCborArray excessive{};
    for ( int index = 0; index <= 1024; ++index )
    {
        QCborMap next = command;
        if ( index % 2 )
        {
            next.insert(QStringLiteral("before"), command.value(QStringLiteral("after")));
            next.insert(QStringLiteral("after"), command.value(QStringLiteral("before")));
        }
        excessive.append(next);
    }
    bad = validHistory;
    bad.insert(QStringLiteral("commands"), excessive);
    reject(bad);
    // A shape-valid row replacement whose before row is absent in the
    // independently stored current draft must never become a write primitive.
    QCborMap wrongCommand = command;
    QCborMap wrongRecord = command.value(QStringLiteral("before")).toMap();
    QCborArray wrongRaw = wrongRecord.value(QStringLiteral("raw")).toArray();
    QCborArray wrongModel = wrongRecord.value(QStringLiteral("model")).toArray();
    wrongRaw[0] = QStringLiteral("WrongKey");
    wrongModel[0] = QStringLiteral("WrongKey");
    wrongRecord.insert(QStringLiteral("raw"), wrongRaw);
    wrongRecord.insert(QStringLiteral("model"), wrongModel);
    wrongCommand.insert(QStringLiteral("before"), wrongRecord);
    bad = validHistory;
    bad.insert(QStringLiteral("commands"), QCborArray{wrongCommand});
    reject(bad);
}

void checkResourceLiveHistoryCommandLimit(const ResourceAuthoringKind kind)
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    auto pane = historyPane(kind, ProjectType::JKDF2, root);
    const auto token = [kind](const int command)
    {
        return QStringLiteral("command_%1%2").arg(command).arg(
            kind == ResourceAuthoringKind::sprite ? QStringLiteral(".mat")
            : kind == ResourceAuthoringKind::soundClass ? QStringLiteral(".wav") : QStringLiteral(" text"));
    };
    const auto field = [kind](const QCborMap& state)
    {
        if ( kind == ResourceAuthoringKind::sprite )
        {
            const QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
            return sprite.isEmpty() ? QString{} : sprite.at(0).toString();
        }
        const QCborArray rows = state.value(QStringLiteral("rows")).toArray();
        const QCborArray row = rows.isEmpty() ? QCborArray{} : rows.at(0).toArray();
        return row.size() > 1 ? row.at(1).toString() : QString{};
    };
    for ( int command = 1; command <= 1025; ++command )
    {
        changeHistoryField(pane.get(), kind, token(command));
        if ( command == 1023 )
        {
            const std::filesystem::path path = root / (kind == ResourceAuthoringKind::sprite ? "bounded.spr"
                : kind == ResourceAuthoringKind::soundClass ? "bounded.snd" : "bounded.uni");
            check(saveHistoryFixture(pane.get(), path) && !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
                  "live resource history establishes a real saved marker near its command boundary");
        }
    }
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap history = captured.value(QStringLiteral("history")).toMap();
    const QCborArray commands = history.value(QStringLiteral("commands")).toArray();
    const QCborMap first = commands.isEmpty() ? QCborMap{} : commands.at(0).toMap().value(QStringLiteral("before")).toMap();
    const QCborArray firstValues = first.value(kind == ResourceAuthoringKind::sprite
        ? QStringLiteral("sprite") : QStringLiteral("raw")).toArray();
    const int firstColumn = kind == ResourceAuthoringKind::sprite ? 0 : 1;
    const QString oldest = firstValues.size() > firstColumn ? firstValues.at(firstColumn).toString() : QString{};
    check(commands.size() == 1024 && history.value(QStringLiteral("cursor")).toInteger(-1) == 1024
              && history.value(QStringLiteral("clean")).toInteger(-1) == 1022 && oldest == token(1)
              && field(captured) == token(1025) && ui::resources::resourceAuthoringPaneIsModified(pane.get())
              && QCborValue(commands).toCbor().size() <= 16 * 1024 * 1024
              && ui::resources::validateResourceAuthoringPaneState(captured)
              && ui::resources::captureResourceAuthoringPaneState(pane.get()) == captured,
          "1025 real SPR SND and UNI commands retain exact current values clean marker and a valid bounded nonmutating timeline");
    check(resourceUndo(pane.get(), false) && field(ui::resources::captureResourceAuthoringPaneState(pane.get())) == token(1024)
              && resourceUndo(pane.get(), false) && field(ui::resources::captureResourceAuthoringPaneState(pane.get())) == token(1023)
              && !ui::resources::resourceAuthoringPaneIsModified(pane.get())
              && resourceUndo(pane.get(), true) && resourceUndo(pane.get(), true)
              && field(ui::resources::captureResourceAuthoringPaneState(pane.get())) == token(1025),
          "live bounded resource Undo and Redo preserve the exact retained external clean checkpoint");
    auto restored = historyPane(kind, ProjectType::JKDF2, root);
    check(ui::resources::restoreResourceAuthoringPaneState(restored.get(), nativeHistoryRoundTrip(captured, ProjectType::JKDF2)),
          "a legitimately evicted resource timeline survives native transport and atomic restoration");
    for ( int command = 0; command < 1024; ++command )
    {
        (void)resourceUndo(restored.get(), false);
    }
    const QCborMap boundary = ui::resources::captureResourceAuthoringPaneState(restored.get());
    check(field(boundary) == token(1) && boundary.value(QStringLiteral("history")).toMap()
              .value(QStringLiteral("cursor")).toInteger(-1) == 0
              && !ui::resources::resourceAuthoringPaneCanUndoRedo(restored.get(), false)
              && ui::resources::resourceAuthoringPaneCanUndoRedo(restored.get(), true)
              && ui::resources::resourceAuthoringPaneIsModified(restored.get())
              && resourceUndo(restored.get(), true)
              && field(ui::resources::captureResourceAuthoringPaneState(restored.get())) == token(2),
          "restored bounded resource history reaches its exact oldest retained value and keeps meaningful Redo");
}

QPlainTextEdit* cogHistoryEditor(MainWindow& window)
{
    auto* const tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    return tabs && tabs->currentWidget() ? tabs->currentWidget()->findChild<QPlainTextEdit*>() : nullptr;
}

void typeCogText(QPlainTextEdit* editor, const QString& text)
{
    for ( const QChar character : text )
    {
        QKeyEvent key(QEvent::KeyPress, 0, Qt::NoModifier, QString(character));
        QApplication::sendEvent(editor, &key);
    }
}

void checkCogLiveHistoryLimits(const bool byteLimit)
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "bounded.cog";
    const QString initial = QStringLiteral("symbols\nend\ncode\nend\n# ");
    check(writeHistoryBytes(script, initial.toUtf8()), "bounded COG history has a fixed clean source baseline");
    MainWindow window{};
    bindHistoryProject(window);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setCurrentPathForTitleForTest(root / "bounded.sed");
    window.setResourcePathsForTest({root}, {}, root / "bounded.sed");
    window.openCogScriptEditorForTest("bounded.cog");
    QPlainTextEdit* editor = cogHistoryEditor(window);
    if ( !editor )
    {
        check(false, "bounded COG history finds the actual live document");
        return;
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    const auto initialFormats = editor->document()->begin().layout()->formats();
    check(!initialFormats.isEmpty(), "bounded COG history has actual source highlighting before reconstruction");
    const int total = byteLimit ? 1 : 1025;
    // The journal charges three bytes per authored character plus overhead.
    // Exceed its live 8 MiB threshold while each recovery text stays below 3 MiB.
    constexpr int kByteLimitCharacters = (8 * 1024 * 1024) / 3 + 1024;
    const QString added = byteLimit ? QString(kByteLimitCharacters, QLatin1Char('x')) : QStringLiteral("x");
    for ( int command = 1; command <= total; ++command )
    {
        QTextCursor edit(editor->document());
        edit.movePosition(QTextCursor::End);
        edit.beginEditBlock();
        edit.insertText(added);
        edit.endEditBlock();
        if ( !byteLimit && command == 1023 )
        {
            check(window.saveCogScriptEditorForTest(0) && !editor->document()->isModified(),
                  "bounded COG command history stores a real external clean checkpoint");
        }
    }
    const QString expected = initial + (byteLimit ? added : QString(1025, QLatin1Char('x')));
    QTextCursor selected(editor->document());
    selected.setPosition(1);
    selected.setPosition(4, QTextCursor::KeepAnchor);
    editor->setTextCursor(selected);
    const int vertical = editor->verticalScrollBar()->value();
    const int horizontal = editor->horizontalScrollBar()->value();
    const int undoSteps = editor->document()->availableUndoSteps();
    const QByteArray before = window.captureCogEditorRecoveryStateForTest();
    check(editor->toPlainText() == expected && editor->textCursor().position() == 4 && editor->textCursor().anchor() == 1
              && editor->document()->isModified() && editor->document()->isUndoAvailable()
              && editor->document()->availableUndoSteps() == undoSteps
              && window.captureCogEditorRecoveryStateForTest() == before,
          "over-limit COG capture preserves exact authored text selection native stack and dirty state before queued trimming");
    QElapsedTimer deadline{};
    deadline.start();
    QCborMap history;
    while ( deadline.elapsed() < 5000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
        const QCborArray tabs = QCborValue::fromCbor(window.captureCogEditorRecoveryStateForTest()).toMap()
                                   .value(QStringLiteral("tabs")).toArray();
        const QCborMap tab = tabs.isEmpty() ? QCborMap{} : tabs.at(0).toMap();
        history = tab.value(QStringLiteral("history")).toMap();
        if ( byteLimit ? !editor->document()->isUndoAvailable()
                       : history.value(QStringLiteral("commands")).toArray().size() < 1024 )
        {
            break;
        }
    }
    const QCborArray commands = history.value(QStringLiteral("commands")).toArray();
    const int retained = static_cast<int>(commands.size());
    check(retained <= 1024 && (byteLimit ? retained == 0 : retained > 2 && retained < 1024)
              && history.value(QStringLiteral("cursor")).toInteger(-1) == retained
              && history.value(QStringLiteral("clean")).toInteger(-2) == (byteLimit ? -1 : retained - 2)
              && QCborValue(history).toCbor().size() <= 16 * 1024 * 1024
              && ui::cog::CogDocumentHistory::validate(history, expected, true)
              && editor->toPlainText() == expected && editor->textCursor().position() == 4
              && editor->textCursor().anchor() == 1 && editor->verticalScrollBar()->value() == vertical
              && editor->horizontalScrollBar()->value() == horizontal && editor->document()->isModified()
              && editor->document()->begin().layout()->formats() == initialFormats,
          "queued COG count or byte eviction preserves exact current text presentation highlighting and consistent bounded clean markers");
    if ( byteLimit )
    {
        QTextCursor next(editor->document());
        next.movePosition(QTextCursor::End);
        next.beginEditBlock();
        next.insertText(QStringLiteral("\n# retained"));
        next.endEditBlock();
        const QString final = expected + QStringLiteral("\n# retained");
        editor->undo();
        check(editor->toPlainText() == expected && editor->document()->isModified(),
              "COG byte eviction keeps the authored baseline dirty while the next retained command undoes exactly");
        editor->redo();
        check(editor->toPlainText() == final && editor->document()->isModified(),
              "COG Redo after a byte-driven native-stack rebuild restores the exact new retained edit");
    }
    else
    {
        editor->undo();
        editor->undo();
        check(editor->toPlainText() == initial + QString(1023, QLatin1Char('x')) && !editor->document()->isModified(),
              "COG count eviction retains native Undo to the exact externally saved clean checkpoint");
        editor->redo();
        editor->redo();
    }
    const QString current = editor->toPlainText();
    const QByteArray captured = window.captureCogEditorRecoveryStateForTest();
    check(window.captureCogEditorRecoveryStateForTest() == captured,
          "capture after a real bounded COG rebuild is immutable");
    MainWindow restored{};
    bindHistoryProject(restored);
    restored.setAttribute(Qt::WA_DontShowOnScreen);
    restored.setCurrentPathForTitleForTest(root / "bounded.sed");
    restored.setResourcePathsForTest({root}, {}, root / "bounded.sed");
    check(restored.restoreCogEditorRecoveryStateForTest(captured), "a legitimately bounded COG native history restores atomically");
    editor = cogHistoryEditor(restored);
    if ( !editor )
    {
        check(false, "bounded COG history reacquires the independently restored editor");
        return;
    }
    const int undoCount = byteLimit ? 1 : retained;
    for ( int command = 0; command < undoCount; ++command )
    {
        editor->undo();
    }
    const QString oldest = byteLimit ? expected : initial + QString(1025 - retained, QLatin1Char('x'));
    check(editor->toPlainText() == oldest && !editor->document()->isUndoAvailable()
              && editor->document()->isRedoAvailable() && editor->document()->isModified(),
          "restored COG count and byte boundaries preserve exact oldest retained text and native Redo");
    editor->redo();
    check(editor->toPlainText() == (byteLimit ? current : oldest + QStringLiteral("x"))
              && editor->document()->isModified()
              && readHistoryBytes(script) == (byteLimit ? initial.toUtf8()
                   : (initial + QString(1023, QLatin1Char('x'))).toUtf8()),
          "bounded COG Redo remains meaningful and native history restoration never rewrites the source");
}

void checkCogTypingSaveHistoryBoundary()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "typing_save.cog";
    const QString initial = QStringLiteral("symbols\nend\ncode\nend\n");
    check(writeHistoryBytes(script, initial.toUtf8()), "COG typing Save history has an exact loose-file baseline");
    MainWindow window{};
    bindHistoryProject(window);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setCurrentPathForTitleForTest(root / "typing.sed");
    window.setResourcePathsForTest({root}, {}, root / "typing.sed");
    window.openCogScriptEditorForTest("typing_save.cog");
    QPlainTextEdit* editor = cogHistoryEditor(window);
    if ( !editor )
    {
        check(false, "COG typing Save history finds the actual text editor");
        return;
    }
    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::End);
    editor->setTextCursor(cursor);
    typeCogText(editor, QStringLiteral("# first"));
    const QString saved = initial + QStringLiteral("# first");
    check(editor->toPlainText() == saved && window.saveCogScriptEditorForTest(0)
              && !editor->document()->isModified() && readHistoryBytes(script) == saved.toUtf8(),
          "COG real Save marks the first native typing command clean at the unchanged caret");
    // Do not create an edit block or move the caret: Save itself must separate
    // subsequent typing from the command that owns the saved clean checkpoint.
    typeCogText(editor, QStringLiteral(" second"));
    const QString final = saved + QStringLiteral(" second");
    const QByteArray captured = window.captureCogEditorRecoveryStateForTest();
    const QCborArray tabs = QCborValue::fromCbor(captured).toMap().value(QStringLiteral("tabs")).toArray();
    const QCborMap history = tabs.isEmpty() ? QCborMap{} : tabs.at(0).toMap().value(QStringLiteral("history")).toMap();
    check(editor->toPlainText() == final && editor->document()->isModified()
              && history.value(QStringLiteral("cursor")).toInteger(-1) == 2
              && history.value(QStringLiteral("clean")).toInteger(-1) == 1
              && history.value(QStringLiteral("commands")).toArray().size() == 2,
          "COG continued native typing after Save starts a second dirty command instead of mutating the saved command");
    editor->undo();
    check(editor->toPlainText() == saved && !editor->document()->isModified(),
          "COG native Undo after continued typing returns exactly to saved text without an explicit edit block");
    check(window.restoreCogEditorRecoveryStateForTest(captured),
          "COG typing Save boundary captures a valid exact native history and restores atomically");
    editor = cogHistoryEditor(window);
    if ( editor )
    {
        editor->undo();
    }
    check(editor && editor->toPlainText() == saved && !editor->document()->isModified()
              && readHistoryBytes(script) == saved.toUtf8(),
          "restored COG Undo retains the typing Save boundary and leaves the exact external source untouched");
}

void checkCogIdenticalReplacementHistory()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "identical.cog";
    const QString initial = QStringLiteral("symbols\nend\ncode\nend\n# same");
    check(writeHistoryBytes(script, initial.toUtf8()), "identical COG replacement starts from an exact clean loose source");
    MainWindow window{};
    bindHistoryProject(window);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setCurrentPathForTitleForTest(root / "identical.sed");
    window.setResourcePathsForTest({root}, {}, root / "identical.sed");
    window.openCogScriptEditorForTest("identical.cog");
    QPlainTextEdit* editor = cogHistoryEditor(window);
    if ( !editor )
    {
        check(false, "identical COG replacement finds the actual clean live document");
        return;
    }
    check(!editor->document()->isModified() && !editor->document()->isUndoAvailable(),
          "identical COG replacement has no inherited Undo command or dirty marker");
    QTextCursor replacement(editor->document());
    const int start = static_cast<int>(initial.size()) - 4;
    replacement.setPosition(start);
    replacement.setPosition(start + 4, QTextCursor::KeepAnchor);
    check(replacement.selectedText() == QStringLiteral("same"), "identical COG replacement selects an independent exact text vector");
    // QTextCursor itself groups removal and reinsertion of a selection. Do
    // not manufacture a separate edit block or alter the native dirty flag.
    replacement.insertText(QStringLiteral("same"));
    editor->setTextCursor(replacement);
    const int steps = editor->document()->availableUndoSteps();
    const bool nativeCommand = editor->toPlainText() == initial && editor->document()->isModified()
        && editor->document()->isUndoAvailable() && !editor->document()->isRedoAvailable() && steps > 0;
    check(nativeCommand, "identical nonempty COG selection replacement creates a real native dirty Undo command without changing text");
    const QByteArray captured = window.captureCogEditorRecoveryStateForTest();
    const QCborArray tabs = QCborValue::fromCbor(captured).toMap().value(QStringLiteral("tabs")).toArray();
    const QCborMap history = tabs.isEmpty() ? QCborMap{} : tabs.at(0).toMap().value(QStringLiteral("history")).toMap();
    const bool validCapture = history.value(QStringLiteral("cursor")).toInteger(-1) == 1
        && history.value(QStringLiteral("clean")).toInteger(-1) == 0
        && history.value(QStringLiteral("commands")).toArray().size() == 1
        && ui::cog::CogDocumentHistory::validate(history, initial, true)
        && window.captureCogEditorRecoveryStateForTest() == captured && editor->toPlainText() == initial
        && editor->textCursor().position() == start + 4 && editor->textCursor().anchor() == start + 4
        && editor->document()->availableUndoSteps() == steps && editor->document()->isModified();
    if ( !nativeCommand || !validCapture )
    {
        std::cerr << "COG identical replacement diagnostic: native=" << nativeCommand << " steps=" << steps
            << " modified=" << editor->document()->isModified() << " cursor="
            << history.value(QStringLiteral("cursor")).toInteger(-1) << " clean="
            << history.value(QStringLiteral("clean")).toInteger(-1) << " commands="
            << history.value(QStringLiteral("commands")).toArray().size() << " valid=" << validCapture << '\n';
    }
    check(validCapture, "identical COG replacement captures a valid dirty native command without mutating text caret or Undo stack");
    editor->undo();
    check(editor->toPlainText() == initial && !editor->document()->isModified()
              && !editor->document()->isUndoAvailable() && editor->document()->isRedoAvailable(),
          "native Undo of identical COG replacement returns to the exact clean marker while preserving source text");
    editor->redo();
    check(editor->toPlainText() == initial && editor->document()->isModified() && editor->document()->isUndoAvailable(),
          "native Redo of identical COG replacement restores its dirty command even though source text remains identical");
    MainWindow restored{};
    bindHistoryProject(restored);
    restored.setAttribute(Qt::WA_DontShowOnScreen);
    restored.setCurrentPathForTitleForTest(root / "identical.sed");
    restored.setResourcePathsForTest({root}, {}, root / "identical.sed");
    const QCborMap transported = nativeHistoryRoundTrip(QCborValue::fromCbor(captured).toMap(), ProjectType::JKDF2);
    check(restored.restoreCogEditorRecoveryStateForTest(QCborValue(transported).toCbor()),
          "identical COG replacement restores through checked native transport and the real tab validator");
    editor = cogHistoryEditor(restored);
    if ( !editor )
    {
        check(false, "identical COG replacement reacquires the restored native document");
        return;
    }
    editor->undo();
    check(editor->toPlainText() == initial && !editor->document()->isModified()
              && !editor->document()->isUndoAvailable() && editor->document()->isRedoAvailable(),
          "restored identical COG replacement retains its exact clean native Undo boundary");
    editor->redo();
    check(editor->toPlainText() == initial && editor->document()->isModified()
              && editor->document()->isUndoAvailable() && readHistoryBytes(script) == initial.toUtf8(),
          "restored identical COG replacement retains dirty Redo and never rewrites its exact external source");
}

void checkCogNonReconstructibleHistoryRejected()
{
    const std::array<QString, 3> insertions{QString{}, QStringLiteral("\u00a0"), QStringLiteral("\u2028")};
    for ( const QString& inserted : insertions )
    {
        QTemporaryDir temporary{};
        const std::filesystem::path root(temporary.path().toStdWString());
        const std::filesystem::path script = root / "cog" / "admission.cog";
        const QString initial = QStringLiteral("symbols\nend\ncode\nend\n");
        check(writeHistoryBytes(script, initial.toUtf8()), "COG admission vectors have an independent fixed source");
        MainWindow window{};
        bindHistoryProject(window);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setCurrentPathForTitleForTest(root / "admission.sed");
        window.setResourcePathsForTest({root}, {}, root / "admission.sed");
        window.openCogScriptEditorForTest("admission.cog");
        QPlainTextEdit* editor = cogHistoryEditor(window);
        if ( !editor )
        {
            check(false, "COG admission vectors find the actual live text document");
            continue;
        }
        QTextCursor cursor(editor->document());
        cursor.movePosition(QTextCursor::End);
        editor->setTextCursor(cursor);
        typeCogText(editor, QStringLiteral("# exact"));
        cursor = editor->textCursor();
        cursor.beginEditBlock();
        cursor.insertText(QStringLiteral(" second"));
        cursor.endEditBlock();
        editor->undo();
        cursor = editor->textCursor();
        cursor.setPosition(1);
        cursor.setPosition(4, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);
        const QString text = initial + QStringLiteral("# exact");
        const QByteArray before = window.captureCogEditorRecoveryStateForTest();
        const bool modified = editor->document()->isModified();
        const int position = editor->textCursor().position();
        const int anchor = editor->textCursor().anchor();
        QCborMap malformed = QCborValue::fromCbor(before).toMap();
        QCborArray tabs = malformed.value(QStringLiteral("tabs")).toArray();
        if ( tabs.isEmpty() )
        {
            check(false, "COG admission vectors capture the actual source tab");
            continue;
        }
        QCborMap tab = tabs.at(0).toMap();
        const QCborMap valid = tab.value(QStringLiteral("history")).toMap();
        check(editor->toPlainText() == text && editor->document()->isUndoAvailable()
                  && editor->document()->isRedoAvailable()
                  && ui::cog::CogDocumentHistory::validate(valid, text, modified),
              "COG malformed splice vectors start from a real valid mid-history cursor with selection and both native actions");
        QCborMap history = valid;
        QCborArray commands = history.value(QStringLiteral("commands")).toArray();
        // This is a future command; it must be checked even though the
        // currently displayed source contains neither malformed character.
        commands.append(QCborArray{QCborMap{{QStringLiteral("position"), 0},
            {QStringLiteral("removed"), QString{}}, {QStringLiteral("inserted"), inserted}}});
        history.insert(QStringLiteral("commands"), commands);
        tab.insert(QStringLiteral("history"), history);
        tabs[0] = tab;
        malformed.insert(QStringLiteral("tabs"), tabs);
        const bool validMalformed = ui::cog::CogDocumentHistory::validate(history, text, modified);
        const bool restored = window.restoreCogEditorRecoveryStateForTest(QCborValue(malformed).toCbor());
        editor = cogHistoryEditor(window);
        const bool unchanged = editor && editor->toPlainText() == text && editor->textCursor().position() == position
            && editor->textCursor().anchor() == anchor && editor->document()->isModified() == modified
            && editor->document()->isUndoAvailable() && editor->document()->isRedoAvailable()
            && window.captureCogEditorRecoveryStateForTest() == before && readHistoryBytes(script) == initial.toUtf8();
        if ( validMalformed || restored || !unchanged )
        {
            std::cerr << "COG malformed splice diagnostic: insertedUTF8=" << inserted.toUtf8().toHex().constData()
                << " validated=" << validMalformed << " restored=" << restored << " unchanged=" << unchanged << '\n';
        }
        check(!validMalformed && !restored && unchanged,
              "COG empty or noncanonical future splice is rejected before changing exact text selection clean state or native Undo and Redo");
    }
}

void checkCogHistoryRoundTrip()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "history.cog";
    const QString initial = QStringLiteral("symbols\nend\ncode\nend\n");
    check(writeHistoryBytes(script, initial.toUtf8()), "COG history has an independent authored loose baseline");
    MainWindow window{};
    bindHistoryProject(window);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setCurrentPathForTitleForTest(root / "history.sed");
    window.setResourcePathsForTest({root}, {}, root / "history.sed");
    window.openCogScriptEditorForTest("history.cog");
    QPlainTextEdit* editor = cogHistoryEditor(window);
    check(editor != nullptr, "COG history finds the real source document");
    if ( !editor )
    {
        return;
    }
    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::End);
    editor->setTextCursor(cursor);
    typeCogText(editor, QStringLiteral("# typed"));
    const QString typed = editor->toPlainText();
    editor->undo();
    check(editor->toPlainText() == initial, "COG typed characters coalesce into the existing native Qt Undo boundary");
    editor->redo();
    check(editor->toPlainText() == typed && window.saveCogScriptEditorForTest(0)
              && !editor->document()->isModified(),
          "COG real external Save marks the typed history checkpoint clean without discarding Undo");
    cursor = editor->textCursor();
    cursor.beginEditBlock();
    cursor.insertText(QStringLiteral("\n# one"));
    cursor.insertText(QStringLiteral("\n# two"));
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
    const QString grouped = editor->toPlainText();
    editor->undo();
    check(editor->toPlainText() == typed && !editor->document()->isModified(),
          "COG paired edit-block changes undo together to the saved clean position");
    const QByteArray captured = window.captureCogEditorRecoveryStateForTest();
    const QCborMap tab = QCborValue::fromCbor(captured).toMap().value(QStringLiteral("tabs")).toArray().at(0).toMap();
    const QCborMap history = tab.value(QStringLiteral("history")).toMap();
    check(history.value(QStringLiteral("version")).toInteger(-1) == 1
              && history.value(QStringLiteral("cursor")).toInteger(-1) == 1
              && history.value(QStringLiteral("clean")).toInteger(-1) == 1
              && history.value(QStringLiteral("commands")).toArray().size() == 2,
          "COG EDIT stores typed coalescing edit-block commands mid-history cursor and saved clean marker");
    const QCborMap transported = nativeHistoryRoundTrip(QCborValue::fromCbor(captured).toMap(), ProjectType::JKDF2);
    check(window.restoreCogEditorRecoveryStateForTest(QCborValue(transported).toCbor()),
          "native COG history restores through the real recovery tab validator");
    editor = cogHistoryEditor(window);
    check(editor != nullptr, "COG history reacquires the live source editor after tab restoration");
    if ( !editor )
    {
        return;
    }
    editor->redo();
    check(editor->toPlainText() == grouped && editor->document()->isModified(),
          "restored COG Redo retains the complete native edit block and dirty state");
    editor->undo();
    check(editor->toPlainText() == typed && !editor->document()->isModified(),
          "restored COG Undo returns to the exact saved clean position");
    editor->undo();
    check(editor->toPlainText() == initial && editor->document()->isModified(),
          "restored COG Undo retains the earlier typed coalescing boundary");
    editor->redo();
    cursor = editor->textCursor();
    cursor.beginEditBlock();
    cursor.insertText(QStringLiteral("\n# replacement branch"));
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
    const QString branch = editor->toPlainText();
    check(!editor->document()->isRedoAvailable(), "new COG edit after Undo truncates native Redo");
    editor->redo();
    check(editor->toPlainText() == branch, "discarded COG Redo cannot replace the new branch");
    cursor = editor->textCursor();
    cursor.beginEditBlock();
    // QTextCursor positions count UTF-16 code units; U+1F680 occupies two.
    // Preserve both surrogate units and the accented BMP character exactly.
    cursor.insertText(QStringLiteral("\n# \U0001f680 caf\u00e9"));
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
    const QString supplementary = editor->toPlainText();
    const QByteArray unicodeHistory = window.captureCogEditorRecoveryStateForTest();
    check(window.restoreCogEditorRecoveryStateForTest(unicodeHistory),
          "COG history restores supplementary Unicode without changing its UTF-16 splice offsets");
    editor = cogHistoryEditor(window);
    if ( !editor )
    {
        check(false, "Unicode COG history reacquires the restored editor");
        return;
    }
    editor->undo();
    check(editor->toPlainText() == branch,
          "restored COG Undo removes the exact supplementary-character edit block");
    editor->redo();
    check(editor->toPlainText() == supplementary,
          "restored COG Redo retains supplementary and BMP Unicode exactly");
    const QByteArray validBefore = window.captureCogEditorRecoveryStateForTest();
    QCborMap malformed = QCborValue::fromCbor(validBefore).toMap();
    QCborArray tabs = malformed.value(QStringLiteral("tabs")).toArray();
    QCborMap badTab = tabs.at(0).toMap();
    const QCborMap validHistory = badTab.value(QStringLiteral("history")).toMap();
    QCborMap malformedHistory = validHistory;
    malformedHistory.insert(QStringLiteral("version"), QStringLiteral("1"));
    badTab.insert(QStringLiteral("history"), malformedHistory);
    tabs[0] = badTab;
    malformed.insert(QStringLiteral("tabs"), tabs);
    check(!window.restoreCogEditorRecoveryStateForTest(QCborValue(malformed).toCbor())
              && window.captureCogEditorRecoveryStateForTest() == validBefore,
          "malformed COG history is rejected atomically without replacing any text cursor dirty state or timeline");
    const auto rejectCogHistory = [&](const QCborMap& badHistory)
    {
        QCborMap bad = QCborValue::fromCbor(validBefore).toMap();
        QCborArray badTabs = bad.value(QStringLiteral("tabs")).toArray();
        QCborMap first = badTabs.at(0).toMap();
        first.insert(QStringLiteral("history"), badHistory);
        badTabs[0] = first;
        bad.insert(QStringLiteral("tabs"), badTabs);
        check(!window.restoreCogEditorRecoveryStateForTest(QCborValue(bad).toCbor())
                  && window.captureCogEditorRecoveryStateForTest() == validBefore,
              "over-budget or invalid UTF-16 splice history leaves the complete COG workspace unchanged");
    };
    QCborArray tooMany = validHistory.value(QStringLiteral("commands")).toArray();
    const int originalCount = static_cast<int>(tooMany.size());
    for ( int index = originalCount; index <= 1024; ++index )
    {
        const bool inserting = (index - originalCount) % 2 == 0;
        tooMany.append(QCborArray{QCborMap{{QStringLiteral("position"), 0},
            {QStringLiteral("removed"), inserting ? QString{} : QStringLiteral("x")},
            {QStringLiteral("inserted"), inserting ? QStringLiteral("x") : QString{}}}});
    }
    QCborMap negative = validHistory;
    negative.insert(QStringLiteral("commands"), tooMany);
    rejectCogHistory(negative);
    for ( const qsizetype position : {supplementary.size() + 1,
              supplementary.indexOf(QStringLiteral("\U0001f680")) + 1} )
    {
        QCborArray commands = validHistory.value(QStringLiteral("commands")).toArray();
        commands.append(QCborArray{QCborMap{{QStringLiteral("position"), position},
            {QStringLiteral("removed"), QString{}}, {QStringLiteral("inserted"), QStringLiteral("outside")}}});
        negative = validHistory;
        negative.insert(QStringLiteral("commands"), commands);
        rejectCogHistory(negative);
    }
    check(window.saveCogScriptEditorForTest(0), "COG history saves a later real clean checkpoint");
    editor->undo();
    const QByteArray futureClean = window.captureCogEditorRecoveryStateForTest();
    check(window.restoreCogEditorRecoveryStateForTest(futureClean), "COG clean checkpoint in Redo restores atomically");
    editor = cogHistoryEditor(window);
    if ( editor )
    {
        editor->redo();
    }
    check(editor && editor->toPlainText() == supplementary && !editor->document()->isModified(),
          "restored COG Redo reaches the saved clean checkpoint in its retained future branch");
    if ( editor )
    {
        QTextCursor normalized(editor->document());
        normalized.movePosition(QTextCursor::End);
        normalized.beginEditBlock();
        normalized.insertText(QStringLiteral("\u00a0# normalized\u2028next"));
        normalized.endEditBlock();
        const QString expected = supplementary + QStringLiteral(" # normalized\nnext");
        const QByteArray normalizedState = window.captureCogEditorRecoveryStateForTest();
        check(editor->toPlainText() == expected && window.restoreCogEditorRecoveryStateForTest(normalizedState),
              "COG history uses Qt's exact fixed NBSP and line-separator plain-text projection");
        editor = cogHistoryEditor(window);
        if ( editor )
        {
            editor->undo();
            check(editor->toPlainText() == supplementary, "normalized COG splice Undo returns the exact saved baseline");
            editor->redo();
            check(editor->toPlainText() == expected, "normalized COG splice Redo preserves exact projected text");
        }
    }
}

void appendHistoryU32(QByteArray& bytes, const std::uint32_t value)
{
    for ( unsigned shift = 0U; shift < 32U; shift += 8U )
    {
        bytes.append(static_cast<char>((value >> shift) & 0xffU));
    }
}

void checkCogDefaultSave(const bool savedProject, const bool archiveSource)
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::filesystem::path resource = root / "Resource";
    const std::filesystem::path project = root / "Project";
    std::error_code error{};
    std::filesystem::create_directories(resource, error);
    std::filesystem::create_directories(project, error);
    const QByteArray leaf("default_history.cog");
    const QByteArray original("symbols\nend\ncode\nend\n");
    QByteArray gob("GOB ", 4);
    appendHistoryU32(gob, 16);
    appendHistoryU32(gob, 0);
    appendHistoryU32(gob, 1);
    appendHistoryU32(gob, 152);
    appendHistoryU32(gob, static_cast<std::uint32_t>(original.size()));
    gob.append(leaf);
    gob.append(QByteArray(128 - leaf.size(), '\0'));
    gob.append(original);
    const std::filesystem::path archive = root / "cog_history.gob";
    if ( archiveSource )
    {
        check(writeHistoryBytes(archive, gob), "COG default Save uses independent synthetic archive bytes");
    }
    MainWindow window{};
    bindHistoryProject(window);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    const std::filesystem::path native = savedProject ? project / "default.sed" : std::filesystem::path{};
    window.setCurrentPathForTitleForTest(native);
    window.setResourcePathsForTest({resource}, archiveSource ? std::vector<std::filesystem::path>{archive}
                                                           : std::vector<std::filesystem::path>{}, native);
    if ( archiveSource )
    {
        window.openCogScriptEditorForTest(leaf.toStdString());
    }
    else
    {
        window.createNewCogScriptForTest(QString::fromLatin1(leaf));
    }
    QPlainTextEdit* const editor = cogHistoryEditor(window);
    check(editor != nullptr, "COG default Save creates the real archive or Untitled authored document");
    if ( !editor )
    {
        return;
    }
    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("# changed\n"));
    const QByteArray expected = archiveSource ? QByteArray("symbols\nend\ncode\nend\n# changed\n")
        : QByteArray("symbols\n\nend\n\ncode\n\nend\n# changed\n");
    int dialogs{};
    QTimer modal{};
    QObject::connect(&modal, &QTimer::timeout, [&]()
    {
        if ( auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()) )
        {
            ++dialogs;
            dialog->reject();
        }
    });
    modal.start(5);
    const bool saved = window.saveCogScriptEditorForTest(0);
    modal.stop();
    const auto target = (savedProject ? project : resource) / "cog" / QString::fromLatin1(leaf).toStdWString();
    check(saved && dialogs == 0 && readHistoryBytes(target) == expected && !editor->document()->isModified(),
          "COG Save writes exact bytes directly to project or configured Untitled COG folder without a chooser");
    if ( archiveSource )
    {
        check(readHistoryBytes(archive) == gob, "COG default Save preserves all original synthetic archive bytes");
    }
}

void checkCogFormatAndReloadHistory()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "format_history.cog";
    const QString original = QStringLiteral("symbols\nint count\nend\ncode\nstartup:\n   print( count );\nreturn;\nend\n");
    check(writeHistoryBytes(script, original.toUtf8()), "COG Format history starts with a precise unformatted source");
    MainWindow window{};
    bindHistoryProject(window);
    window.setCurrentPathForTitleForTest(root / "format_history.sed");
    window.setResourcePathsForTest({root}, {}, root / "format_history.sed");
    window.openCogScriptEditorForTest("format_history.cog");
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();
    QPlainTextEdit* editor = cogHistoryEditor(window);
    QToolButton* format = window.findChild<QToolButton*>(QStringLiteral("cogEditorFormatButton"));
    check(editor && format, "COG native history invokes the actual Format toolbar action");
    if ( !editor || !format )
    {
        return;
    }
    format->click();
    const QString formatted = editor->toPlainText();
    check(formatted != original, "COG Format positive control changes multiple source formatting positions");
    editor->undo();
    check(editor->toPlainText() == original && !editor->document()->isModified(),
          "the actual compound COG Format operation creates one clean-baseline Undo step");
    const QByteArray captured = window.captureCogEditorRecoveryStateForTest();
    check(window.restoreCogEditorRecoveryStateForTest(captured), "COG native history restores the real Format redo command");
    editor = cogHistoryEditor(window);
    if ( !editor )
    {
        check(false, "COG Format native history reacquires its editor after restoration");
        return;
    }
    editor->redo();
    check(editor->toPlainText() == formatted && editor->document()->isModified(),
          "native COG Redo repeats the complete Format edit block exactly");
    QAction* const refresh = window.findChild<QAction*>(QStringLiteral("cog_editor.reload"));
    check(refresh && !window.findChild<QToolButton*>(QStringLiteral("cogEditorReloadButton")),
          "COG history retains the configured Refresh action without a Reload toolbar button");
    const auto triggerRefreshWithEditorFocus = [&]()
    {
        editor = cogHistoryEditor(window);
        if ( !refresh || !editor )
        {
            return false;
        }
        const bool focusAcquired = native_caption_test::focusTestWidget(editor, editor);
        const bool editorOwnsFocus = focusAcquired && QApplication::focusWidget() == editor;
        check(editorOwnsFocus && refresh->isEnabled(),
              "configured COG Refresh action is enabled while the actual script editor owns focus");
        if ( !focusAcquired || !editorOwnsFocus || !refresh->isEnabled() )
        {
            return false;
        }
        refresh->trigger();
        return true;
    };
    const QByteArray beforeFailure = window.captureCogEditorRecoveryStateForTest();
    std::error_code error{};
    std::filesystem::remove(script, error);
    QTimer modal{};
    int errors{};
    bool discard{};
    QObject::connect(&modal, &QTimer::timeout, [&]()
    {
        if ( auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            ++errors;
            if ( discard && message->windowTitle() == QStringLiteral("Reload COG script") )
            {
                message->done(QMessageBox::Discard);
            }
            else
            {
                message->reject();
            }
        }
    });
    modal.start(5);
    const bool triggeredFailedRefresh = triggerRefreshWithEditorFocus();
    modal.stop();
    check(triggeredFailedRefresh && errors == 1 && window.captureCogEditorRecoveryStateForTest() == beforeFailure,
          "failed COG Refresh leaves the exact raw buffer native history cursor redo and clean state intact");
    const QString replacement = QStringLiteral("symbols\nint replacement\nend\ncode\nend\n");
    check(writeHistoryBytes(script, replacement.toUtf8()), "COG Reload history publishes a replacement external baseline");
    discard = true;
    modal.start(5);
    const bool triggeredSuccessfulRefresh = triggerRefreshWithEditorFocus();
    modal.stop();
    editor = cogHistoryEditor(window);
    check(triggeredSuccessfulRefresh && editor && editor->toPlainText() == replacement && !editor->document()->isModified()
              && !editor->document()->isUndoAvailable() && !editor->document()->isRedoAvailable(),
          "successful COG Refresh starts a new clean external baseline and clears only the replaced document history");
}

void checkArchiveAutomaticSave(const ResourceAuthoringKind kind, const ProjectType project, const bool savedProject)
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    const QString leaf = kind == ResourceAuthoringKind::sprite ? QStringLiteral("archived.spr")
                       : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("archived.snd")
                                                                 : QStringLiteral("archived.uni");
    const QByteArray source = kind == ResourceAuthoringKind::sprite
        ? (project == ProjectType::IJIM ? QByteArray("old.mat 0 1 2 4 0 3 0 0 0 1 0 0 0\n")
                                       : QByteArray("old.mat 0 1 2 4 0 0 0 0 0 0\n"))
        : kind == ResourceAuthoringKind::soundClass ? QByteArray("moving old.wav 0x40 0.5 2.5 1\n")
                                                  : QByteArray("MSGS 1\n\"CaseKey\" -7 \"old text\"\nEND\n");
    QByteArray archive("GOB ", 4);
    appendHistoryU32(archive, 0U);
    appendHistoryU32(archive, 0U);
    appendHistoryU32(archive, 1U);
    appendHistoryU32(archive, 152U);
    appendHistoryU32(archive, static_cast<std::uint32_t>(source.size()));
    archive.append(leaf.toLatin1());
    archive.append(QByteArray(128 - leaf.size(), '\0'));
    archive.append(source);
    const std::filesystem::path archivePath = root / "synthetic.gob";
    check(writeHistoryBytes(archivePath, archive), "automatic Save uses a project-owned synthetic archive fixture");
    auto locator = std::make_shared<assets::ResourceLocator>();
    locator->mountGob(archivePath);
    ui::resources::ResourceAuthoringContext context{};
    context.resources = locator;
    context.projectType = project;
    context.projectDirectory = savedProject ? root : std::filesystem::path{};
    context.startingDirectory = savedProject ? root
        : kind == ResourceAuthoringKind::sprite ? root / "misc" / "spr"
        : kind == ResourceAuthoringKind::soundClass ? root / "misc" / "snd"
                                                  : root / "misc";
    context.automaticSaveDirectory = kind == ResourceAuthoringKind::sprite ? root / "misc" / "spr"
        : kind == ResourceAuthoringKind::soundClass ? root / "misc" / "snd" : root / "misc";
    int noticeCount{};
    QString noticeText{};
    context.briefNotice = [&](QWidget* const owner, const QString& text)
    {
        if ( owner )
        {
            ++noticeCount;
            noticeText = text;
        }
    };
    std::unique_ptr<QWidget> pane(ui::resources::createResourceAuthoringPane(nullptr, kind, context));
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
    state.remove(QStringLiteral("history")); // Imported archive bytes define a new authored baseline.
    state.remove(QStringLiteral("editorPresentation"));
    state.insert(QStringLiteral("sourceKind"), 2);
    state.insert(QStringLiteral("name"), leaf);
    state.insert(QStringLiteral("projectResource"), leaf);
    state.insert(QStringLiteral("draftIdentity"), QString{});
    state.insert(QStringLiteral("diskHash"), QCryptographicHash::hash(source, QCryptographicHash::Sha256));
    state.insert(QStringLiteral("modified"), true);
    if ( kind == ResourceAuthoringKind::sprite )
    {
        QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
        sprite[0] = QStringLiteral("changed.mat");
        sprite[2] = QStringLiteral("1");
        sprite[3] = QStringLiteral("2");
        sprite[1] = 0;
        sprite[4] = 4;
        sprite[5] = 0;
        sprite[6] = project == ProjectType::IJIM ? 3 : 0;
        sprite[7] = QStringLiteral("0");
        for ( const qsizetype index : {qsizetype{8}, qsizetype{9}, qsizetype{10}, qsizetype{12}, qsizetype{13}, qsizetype{14}} )
        {
            sprite[index] = QStringLiteral("0");
        }
        sprite[11] = QStringLiteral("1");
        state.insert(QStringLiteral("sprite"), sprite);
    }
    else if ( kind == ResourceAuthoringKind::soundClass )
    {
        state.insert(QStringLiteral("rows"), QCborArray{QCborValue(QCborArray{
            QStringLiteral("moving"), QStringLiteral("changed.wav"), QStringLiteral("0x40"),
            QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("1")})});
        state.insert(QStringLiteral("soundPreviewLoops"), QCborArray{false});
    }
    else
    {
        state.insert(QStringLiteral("rows"), QCborArray{QCborValue(QCborArray{
            QStringLiteral("CaseKey"), QStringLiteral("changed text"), QStringLiteral("-7")})});
        state.insert(QStringLiteral("capacity"), 1);
    }
    QString warning{};
    const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning);
    if ( !restored )
    {
        qWarning().noquote() << "Archive fixture" << static_cast<int>(project) << static_cast<int>(kind) << warning;
    }
    check(restored,
          "automatic Save retains the archive-backed source identity and exact modified draft");
    const std::filesystem::path target = kind == ResourceAuthoringKind::sprite ? root / "misc" / "spr" / leaf.toStdWString()
                                       : kind == ResourceAuthoringKind::soundClass ? root / "misc" / "snd" / leaf.toStdWString()
                                                                                 : root / "misc" / leaf.toStdWString();
    int chooserCount{};
    int errorCount{};
    QString errorText{};
    QTimer modal{};
    QObject::connect(&modal, &QTimer::timeout, [&]()
    {
        if ( auto* const chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()) )
        {
            ++chooserCount;
            chooser->reject();
        }
        else if ( auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            ++errorCount;
            errorText = message->windowTitle() + QStringLiteral(": ") + message->text();
            message->reject();
        }
    });
    modal.start(5);
    const bool saved = ui::resources::resourceAuthoringPaneSave(pane.get(), false);
    modal.stop();
    const QByteArray expected = kind == ResourceAuthoringKind::sprite
        ? (project == ProjectType::IJIM ? QByteArray("changed.mat 0 1 2 4 0 3 0 0 0 1 0 0 0\n")
                                       : QByteArray("changed.mat 0 1 2 4 0 0 0 0 0 0\n"))
        : kind == ResourceAuthoringKind::soundClass ? QByteArray("moving changed.wav 0x00000040 0.5 2.5 1\n")
                                                  : QByteArray("MSGS 1\n\n\"CaseKey\" -7 \"changed text\"\n\nEND\n");
    const QByteArray actual = readHistoryBytes(target);
    const QString actualPath = ui::resources::resourceAuthoringPaneLooseFilePath(pane.get());
    // Saved-project destinations are canonicalized for confinement. Untitled
    // uses the exact explicitly configured destination without that rewrite.
    std::error_code canonicalError{};
    const std::filesystem::path expectedTarget = savedProject
        ? std::filesystem::weakly_canonical(target, canonicalError) : target;
    const QString expectedPath = QString::fromStdWString(expectedTarget.wstring());
    const bool succeeded = saved && chooserCount == 0 && errorCount == 0 && actual == expected
              && !canonicalError && actualPath == expectedPath
              && !ui::resources::resourceAuthoringPaneIsModified(pane.get())
              && readHistoryBytes(archivePath) == archive && noticeCount == 1
              && noticeText == QStringLiteral("Saved %1").arg(actualPath);
    if ( !succeeded )
    {
        const QCborMap after = ui::resources::captureResourceAuthoringPaneState(pane.get());
        const auto* const status = pane->findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
        std::cerr << "Archive Save diagnostic: assertion=" << succeeded << " project=" << static_cast<int>(project)
            << " kind=" << static_cast<int>(kind) << " savedProject=" << savedProject << " saved=" << saved
            << " chooser=" << chooserCount << " errors=" << errorCount << " error=" << errorText.toStdString()
            << " noticeCount=" << noticeCount << " notice=" << noticeText.toStdString()
            << " sourceKind=" << after.value(QStringLiteral("sourceKind")).toInteger(-1)
            << " modified=" << ui::resources::resourceAuthoringPaneIsModified(pane.get())
            << " canonicalError=" << canonicalError.message() << " pathExact=" << (actualPath == expectedPath)
            << " actualPath=" << actualPath.toStdString()
            << " expectedPath=" << expectedPath.toStdString()
            << " bytesExact=" << (actual == expected) << " actualBytes=" << actual.toHex().constData()
            << " expectedBytes=" << expected.toHex().constData()
            << " archiveExact=" << (readHistoryBytes(archivePath) == archive)
            << " status=" << (status ? status->text().toStdString() : std::string{}) << '\n';
    }
    check(succeeded,
          "first archive Save writes its original name directly under the project misc category without a chooser or extraction");
    // Collision, failure and explicit Save As are common paths. Exercise
    // their new automatic-destination interaction once per resource kind.
    if ( project != ProjectType::JKDF2 || !savedProject )
    {
        return;
    }
    chooserCount = 0;
    modal.start(5);
    const QCborMap beforeSaveAs = resourceContent(pane.get());
    const bool savedAs = ui::resources::resourceAuthoringPaneSave(pane.get(), true);
    modal.stop();
    check(!savedAs && chooserCount == 1 && resourceContent(pane.get()) == beforeSaveAs,
          "explicit archive Save As retains the destination chooser and cancellation preserves the draft");
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), state),
          "archive collision fixture reopens the exact original in-memory archive draft");
    const QCborMap beforeCollision = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QByteArray diskBefore = readHistoryBytes(target);
    chooserCount = 0;
    errorCount = 0;
    modal.start(5);
    const bool collisionSaved = ui::resources::resourceAuthoringPaneSave(pane.get(), false);
    modal.stop();
    check(!collisionSaved && chooserCount == 0 && errorCount == 1
              && readHistoryBytes(target) == diskBefore
              && ui::resources::captureResourceAuthoringPaneState(pane.get()) == beforeCollision,
          "automatic archive Save still requires existing-destination collision resolution and cancellation preserves bytes identity history and raw draft");
    std::error_code error{};
    std::filesystem::remove_all(root / "misc", error);
    check(!error && writeHistoryBytes(root / "misc", QByteArray("blocking file")),
          "automatic archive Save has a deterministic unwritable category fixture");
    errorCount = 0;
    modal.start(5);
    const bool failedWrite = ui::resources::resourceAuthoringPaneSave(pane.get(), false);
    modal.stop();
    check(!failedWrite && chooserCount == 0 && errorCount == 1
              && readHistoryBytes(root / "misc") == QByteArray("blocking file")
              && readHistoryBytes(archivePath) == archive
              && ui::resources::captureResourceAuthoringPaneState(pane.get()) == beforeCollision,
          "failed automatic archive Save reports its write error and retains the complete draft history and source without extraction");
}

void checkAggregateNativeHistory()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    check(writeHistoryBytes(root / "cog" / "aggregate.cog", QByteArray("symbols\nend\ncode\nend\n")),
          "aggregate native history writes an independent COG source baseline");
    MainWindow window{};
    bindHistoryProject(window);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setCurrentPathForTitleForTest(root / "aggregate.sed");
    window.setResourcePathsForTest({root}, {}, root / "aggregate.sed");
    window.openCogScriptEditorForTest("aggregate.cog");
    QPlainTextEdit* cog = cogHistoryEditor(window);
    QAction* const spriteAction = window.findChild<QAction*>(QStringLiteral("tools.resource_editors.sprite_spr.new"));
    if ( spriteAction )
    {
        spriteAction->trigger();
    }
    auto* const dock = window.findChild<QDockWidget*>(QStringLiteral("resourceEditorsDock"));
    auto* const newButton = dock ? dock->findChild<QToolButton*>(QStringLiteral("resource_authoring.new")) : nullptr;
    if ( newButton )
    {
        newButton->click();
    }
    auto* const tabs = window.findChild<QTabWidget*>(QStringLiteral("resourceEditorTabs"));
    QWidget* const pane = tabs ? tabs->currentWidget() : nullptr;
    check(cog && pane, "aggregate history opens actual COG and Sprite document pages through their group UI");
    if ( !cog || !pane )
    {
        return;
    }
    changeHistoryField(pane, ResourceAuthoringKind::sprite, QStringLiteral("aggregate-first.mat"));
    const QCborMap firstSprite = resourceContent(pane);
    changeHistoryField(pane, ResourceAuthoringKind::sprite, QStringLiteral("aggregate-second.mat"));
    const QCborMap secondSprite = resourceContent(pane);
    (void)resourceUndo(pane, false);
    QTextCursor cursor(cog->document());
    cursor.movePosition(QTextCursor::End);
    cog->setTextCursor(cursor);
    typeCogText(cog, QStringLiteral("# aggregate"));
    const QString cogChanged = cog->toPlainText();
    cog->undo();
    const QByteArray payload = window.captureRecoveryEditorStateForTest();
    QString warning{};
    check(window.validateRecoveryEditorStateForTest(payload, &warning) && warning.isEmpty(),
          "full MainWindow EDIT validator accepts resource and COG histories together before native Save");
    const QCborMap fullState = QCborValue::fromCbor(payload).toMap();
    const QCborMap resourceWorkspace = fullState.value(QStringLiteral("resourceEditors")).toMap();
    const QCborMap resourcePage = resourceWorkspace.value(QStringLiteral("tabs")).toArray().at(0).toMap()
                                     .value(QStringLiteral("page")).toMap();
    const QCborMap cogWorkspace = QCborValue::fromCbor(fullState.value(QStringLiteral("cogEditors")).toByteArray()).toMap();
    check(resourcePage.value(QStringLiteral("history")).isMap()
              && cogWorkspace.value(QStringLiteral("tabs")).toArray().at(0).toMap().value(QStringLiteral("history")).isMap(),
          "manual native Save and recovery capture both actual document histories in the valid aggregate EDIT workspace");
    const QCborMap nativeLoaded = nativeHistoryRoundTrip(fullState, ProjectType::JKDF2);
    const QByteArray loadedPayload = QCborValue(nativeLoaded).toCbor();
    MainWindow restored{};
    bindHistoryProject(restored);
    restored.setAttribute(Qt::WA_DontShowOnScreen);
    restored.setCurrentPathForTitleForTest(root / "aggregate.sed");
    restored.setResourcePathsForTest({root}, {}, root / "aggregate.sed");
    check(restored.validateRecoveryEditorStateForTest(loadedPayload, &warning)
              && restored.restoreRecoveryEditorStateForTest(loadedPayload, &warning),
          "checked native EDIT load restores the complete resource and COG workspace through the autosave recovery path");
    auto* const restoredTabs = restored.findChild<QTabWidget*>(QStringLiteral("resourceEditorTabs"));
    QWidget* const restoredPane = restoredTabs ? restoredTabs->currentWidget() : nullptr;
    cog = cogHistoryEditor(restored);
    check(restoredPane && resourceContent(restoredPane) == firstSprite
              && resourceUndo(restoredPane, true) && resourceContent(restoredPane) == secondSprite,
          "aggregate native restore retains the Sprite current cursor and exact Redo command");
    if ( cog )
    {
        cog->redo();
    }
    check(cog && cog->toPlainText() == cogChanged,
          "aggregate native restore retains the COG Redo branch independently of resource history");
}

QCborMap resourcePathDraftState(const QWidget* pane)
{
    const QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane);
    QCborMap draft{};
    for ( const QString& key : {QStringLiteral("sourceKind"), QStringLiteral("name"), QStringLiteral("path"),
                              QStringLiteral("projectResource"), QStringLiteral("draftIdentity"),
                              QStringLiteral("diskHash"), QStringLiteral("modified"), QStringLiteral("history"),
                              QStringLiteral("sprite"), QStringLiteral("spritePendingNumericEdits"),
                              QStringLiteral("rows"), QStringLiteral("capacity")} )
    {
        if ( state.contains(key) )
        {
            draft.insert(key, state.value(key));
        }
    }
    return draft;
}

QTabWidget* resourcePathTabHost(MainWindow& window, const QString& identity, QWidget** page)
{
    for ( QTabWidget* const tabs : window.findChildren<QTabWidget*>() )
    {
        for ( int index = 0; index < tabs->count(); ++index )
        {
            QWidget* const candidate = tabs->widget(index);
            if ( ui::resources::resourceAuthoringPaneIdentity(candidate) == identity )
            {
                *page = candidate;
                return tabs;
            }
        }
    }
    *page = nullptr;
    return nullptr;
}

void checkResourceFilePathChrome(const ResourceAuthoringKind kind, const bool archiveSource)
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    const QString leaf = kind == ResourceAuthoringKind::sprite ? QStringLiteral("MixedCase.spr")
        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("MixedCase.snd") : QStringLiteral("MixedCase.uni");
    const QString logical = kind == ResourceAuthoringKind::sprite ? QStringLiteral("misc/spr/") + leaf
        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("misc/snd/") + leaf : QStringLiteral("misc/") + leaf;
    const std::filesystem::path loosePath = root / logical.toStdWString();
    const QString fullPath = QString::fromStdWString(loosePath.wstring());
    const QByteArray bytes = kind == ResourceAuthoringKind::sprite ? QByteArray("old.mat 0 1e-12 2 4 0 0 0 0 0 0\n")
        : kind == ResourceAuthoringKind::soundClass ? QByteArray("moving old.wav 0x80000040 0.5 2.5 1\n")
                                                  : QByteArray("MSGS 1\n\"CaseKey\" -2147483648 \"old text\"\nEND\n");
    std::vector<std::filesystem::path> archives{};
    QByteArray archiveBytes{};
    const std::filesystem::path archivePath = root / "tab-path.gob";
    if ( archiveSource )
    {
        archiveBytes = QByteArray("GOB ", 4);
        appendHistoryU32(archiveBytes, 0U);
        appendHistoryU32(archiveBytes, 0U);
        appendHistoryU32(archiveBytes, 1U);
        appendHistoryU32(archiveBytes, 152U);
        appendHistoryU32(archiveBytes, static_cast<std::uint32_t>(bytes.size()));
        archiveBytes.append(logical.toLatin1());
        archiveBytes.append(QByteArray(128 - logical.size(), '\0'));
        archiveBytes.append(bytes);
        check(writeHistoryBytes(archivePath, archiveBytes), "resource tab-path fixture mounts exact synthetic archive source bytes");
        archives.push_back(archivePath);
    }
    else
    {
        check(writeHistoryBytes(loosePath, bytes), "resource tab-path fixture has exact independent loose source bytes");
    }

    auto baseline = historyPane(kind, ProjectType::JKDF2, root);
    const auto* const standaloneLabel = baseline->findChild<QLabel*>(QStringLiteral("resource_authoring.document"));
    auto* const standaloneLayout = qobject_cast<QVBoxLayout*>(baseline->layout());
    QLayout* const fileControls = standaloneLayout && standaloneLayout->count()
        ? standaloneLayout->itemAt(0)->layout() : nullptr;
    bool standaloneButtons = fileControls && fileControls->count() == 3;
    for ( const QString& name : {QStringLiteral("resource_authoring.new"), QStringLiteral("resource_authoring.open"),
                               QStringLiteral("resource_authoring.save")} )
    {
        QToolButton* const button = baseline->findChild<QToolButton*>(name);
        standaloneButtons = standaloneButtons && button && !button->isHidden()
            && fileControls->indexOf(button) >= 0;
    }
    check((!standaloneLabel || standaloneLabel->isHidden()) && standaloneButtons,
          "standalone resource pages remove the filename label while retaining their three visible file controls");

    MainWindow window{};
    bindHistoryProject(window);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setCurrentPathForTitleForTest(root / "tab-path.sed");
    window.setResourcePathsForTest({root}, archives, root / "tab-path.sed");
    const QString actionName = kind == ResourceAuthoringKind::sprite ? QStringLiteral("tools.resource_editors.sprite_spr.new")
        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("tools.resource_editors.sound_class_snd.new")
                                                  : QStringLiteral("tools.resource_editors.string_table_uni.new");
    const QString dockName = kind == ResourceAuthoringKind::sprite ? QStringLiteral("resourceEditorsDock")
        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("soundClassResourceEditorsDock")
                                                  : QStringLiteral("uniResourceEditorsDock");
    const QString tabsName = kind == ResourceAuthoringKind::sprite ? QStringLiteral("resourceEditorTabs")
        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("soundClassResourceEditorTabs")
                                                  : QStringLiteral("uniResourceEditorTabs");
    QAction* const action = window.findChild<QAction*>(actionName);
    if ( action )
    {
        action->trigger();
    }
    QDockWidget* const dock = window.findChild<QDockWidget*>(dockName);
    QTabWidget* const primary = window.findChild<QTabWidget*>(tabsName);
    QToolButton* const newButton = dock ? dock->findChild<QToolButton*>(QStringLiteral("resource_authoring.new")) : nullptr;
    if ( primary && primary->count() == 0 && newButton )
    {
        newButton->click();
    }
    QWidget* const page = primary ? primary->currentWidget() : nullptr;
    check(action && page, "resource path presentation opens a real registered page in the requested group");
    if ( !page )
    {
        return;
    }
    QCborMap seed = ui::resources::captureResourceAuthoringPaneState(baseline.get());
    seed.remove(QStringLiteral("history")); // This source fixture replaces the independent authored baseline.
    seed.insert(QStringLiteral("sourceKind"), archiveSource ? 2 : 1);
    seed.insert(QStringLiteral("name"), leaf);
    seed.insert(QStringLiteral("path"), archiveSource ? QString{} : fullPath);
    seed.insert(QStringLiteral("projectResource"), archiveSource ? logical : QString{});
    seed.insert(QStringLiteral("draftIdentity"), QString{});
    seed.insert(QStringLiteral("diskHash"), QCryptographicHash::hash(bytes, QCryptographicHash::Sha256));
    QString warning{};
    const bool sourceRestored = ui::resources::restoreResourceAuthoringPaneState(page, seed, &warning);
    check(sourceRestored,
          "resource tab path binds the exact clean source identity without changing its authored baseline");
    if ( !sourceRestored )
    {
        std::cerr << "Resource tab-path seed: kind=" << static_cast<int>(kind) << " archive=" << archiveSource
            << " warning=" << warning.toStdString() << '\n';
        return;
    }
    changeHistoryField(page, kind, kind == ResourceAuthoringKind::sprite ? QStringLiteral("first.mat")
        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("first.wav") : QStringLiteral("first text"));
    changeHistoryField(page, kind, kind == ResourceAuthoringKind::sprite ? QStringLiteral("second.mat")
        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("second.wav") : QStringLiteral("second text"));
    (void)resourceUndo(page, false);
    if ( kind == ResourceAuthoringKind::sprite )
    {
        auto* const width = page->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"));
        if ( width )
        {
            width->lineEdit()->setText(QStringLiteral("1e"));
            QMetaObject::invokeMethod(width->lineEdit(), "textEdited", Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("1e")));
        }
    }
    const QString identity = ui::resources::resourceAuthoringPaneIdentity(page);
    const QCborMap draft = resourcePathDraftState(page);
    const QString hover = archiveSource ? logical : fullPath;
    const QString title = leaf + QStringLiteral(" *");
    const auto assertChrome = [&](QWidget* const candidate, QTabWidget* const host)
    {
        const auto* const label = candidate ? candidate->findChild<QLabel*>(QStringLiteral("resource_authoring.document")) : nullptr;
        auto* const layout = candidate ? qobject_cast<QVBoxLayout*>(candidate->layout()) : nullptr;
        QWidget* const tools = candidate ? candidate->findChild<QWidget*>(QStringLiteral("resource_authoring.document_tools")) : nullptr;
        QWidget* const content = candidate ? candidate->findChild<QWidget*>(QStringLiteral("resource_authoring.content")) : nullptr;
        check(candidate && (!label || label->isHidden()) && layout && tools && content
                  && layout->count() == 3 && layout->itemAt(0)->widget() == tools
                  && layout->itemAt(1)->widget() == content,
              "embedded resource pages have no visible filename label or empty header row above document controls and content");
        const int index = host ? host->indexOf(candidate) : -1;
        check(index >= 0 && host->tabText(index) == title && host->tabToolTip(index) == hover,
              "resource tabs show only the filename and expose the exact full loose path or original-case project entry on hover");
        const QCborMap actual = candidate ? resourcePathDraftState(candidate) : QCborMap{};
        // EDIT canonicalizes map keys. Compare its exact representation so
        // Qt's insertion-order-sensitive map equality does not reject the
        // same complete document timeline after native transport.
        const QByteArray expectedBytes = ui::recovery::serializeCanonicalCbor(QCborValue(draft));
        const QByteArray actualBytes = ui::recovery::serializeCanonicalCbor(QCborValue(actual));
        const bool draftExact = candidate && actualBytes == expectedBytes;
        if ( !draftExact )
        {
            std::cerr << "Resource tab-path draft mismatch: kind=" << static_cast<int>(kind)
                << " archive=" << archiveSource << " host=" << (host ? host->objectName().toStdString() : std::string{})
                << " expectedBytes=" << expectedBytes.size() << " actualBytes=" << actualBytes.size() << '\n';
        }
        check(draftExact,
              "resource path presentation preserves exact source identity pending text history cursor Redo and dirty state");
    };
    assertChrome(page, primary);

    const QCborMap full = QCborValue::fromCbor(window.captureRecoveryEditorStateForTest()).toMap();
    const QByteArray native = QCborValue(nativeHistoryRoundTrip(full, ProjectType::JKDF2)).toCbor();
    MainWindow restored{};
    bindHistoryProject(restored);
    restored.setAttribute(Qt::WA_DontShowOnScreen);
    restored.setCurrentPathForTitleForTest(root / "tab-path.sed");
    restored.setResourcePathsForTest({root}, archives, root / "tab-path.sed");
    const bool primaryRestored = restored.validateRecoveryEditorStateForTest(native, &warning)
        && restored.restoreRecoveryEditorStateForTest(native, &warning);
    QWidget* restoredPage = nullptr;
    QTabWidget* const restoredHost = primaryRestored ? resourcePathTabHost(restored, identity, &restoredPage) : nullptr;
    check(primaryRestored && restoredPage, "native EDIT restores a primary resource tab with its complete source and local history");
    if ( !primaryRestored )
    {
        std::cerr << "Resource tab-path primary restore: kind=" << static_cast<int>(kind) << " archive=" << archiveSource
            << " warning=" << warning.toStdString() << '\n';
    }
    assertChrome(restoredPage, restoredHost);

    QCborMap detachedRoot = full;
    QCborMap workspace = detachedRoot.value(QStringLiteral("resourceEditors")).toMap();
    QCborArray hosts = workspace.value(QStringLiteral("hosts")).toArray();
    QCborMap groups = workspace.value(QStringLiteral("groups")).toMap();
    const QString groupKey = kind == ResourceAuthoringKind::sprite ? QStringLiteral("sprite")
        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("soundClass") : QStringLiteral("uni");
    if ( hosts.size() != 1 || !groups.value(groupKey).isMap() )
    {
        check(false, "detached path fixture starts with exactly one typed resource host");
        return;
    }
    QCborMap host = hosts.at(0).toMap();
    host.insert(QStringLiteral("primary"), false);
    host.insert(QStringLiteral("id"), QStringLiteral("detached_path"));
    host.insert(QStringLiteral("floating"), true);
    host.insert(QStringLiteral("visible"), true);
    hosts[0] = host;
    QCborMap group = groups.value(groupKey).toMap();
    group.insert(QStringLiteral("active"), -1);
    groups.insert(groupKey, group);
    workspace.insert(QStringLiteral("hosts"), hosts);
    workspace.insert(QStringLiteral("groups"), groups);
    detachedRoot.insert(QStringLiteral("resourceEditors"), workspace);
    const QByteArray detachedNative = QCborValue(nativeHistoryRoundTrip(detachedRoot, ProjectType::JKDF2)).toCbor();
    MainWindow detached{};
    bindHistoryProject(detached);
    detached.setAttribute(Qt::WA_DontShowOnScreen);
    detached.setCurrentPathForTitleForTest(root / "tab-path.sed");
    detached.setResourcePathsForTest({root}, archives, root / "tab-path.sed");
    const bool detachedRestored = detached.validateRecoveryEditorStateForTest(detachedNative, &warning)
        && detached.restoreRecoveryEditorStateForTest(detachedNative, &warning);
    QWidget* detachedPage = nullptr;
    QTabWidget* const detachedHost = detachedRestored ? resourcePathTabHost(detached, identity, &detachedPage) : nullptr;
    QDockWidget* detachedDock = nullptr;
    for ( QWidget* ancestor = detachedHost; ancestor && !detachedDock; ancestor = ancestor->parentWidget() )
    {
        detachedDock = qobject_cast<QDockWidget*>(ancestor);
    }
    check(detachedRestored && detachedPage && detachedDock && detachedDock->isFloating(),
          "native EDIT restores resource path presentation into a genuinely detached floating host");
    if ( !detachedRestored )
    {
        std::cerr << "Resource tab-path detached restore: kind=" << static_cast<int>(kind) << " archive=" << archiveSource
            << " warning=" << warning.toStdString() << '\n';
    }
    assertChrome(detachedPage, detachedHost);
    check(archiveSource ? readHistoryBytes(archivePath) == archiveBytes && !std::filesystem::exists(loosePath)
                        : readHistoryBytes(loosePath) == bytes,
          "resource header and tab hover restoration never write or extract its source bytes");
}

void checkResourceSuccessStatusHasNoPermanentPath(const ResourceAuthoringKind kind)
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    auto pane = historyPane(kind, ProjectType::JKDF2, root);
    const std::filesystem::path path = root / (kind == ResourceAuthoringKind::sprite ? "status.spr"
        : kind == ResourceAuthoringKind::soundClass ? "status.snd" : "status.uni");
    const bool saved = saveHistoryFixture(pane.get(), path);
    QLabel* const status = pane->findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
    check(saved && status && status->text() == QStringLiteral("Saved."),
          "successful resource Save leaves a plain permanent status with no filename or path");
    const QByteArray replacement = kind == ResourceAuthoringKind::sprite ? QByteArray("replacement.mat 0 1 2 4 0 0 0 0 0 0\n")
        : kind == ResourceAuthoringKind::soundClass ? QByteArray("moving replacement.wav 0x40 0.5 2.5 1\n")
                                                  : QByteArray("MSGS 1\n\"CaseKey\" -7 \"replacement text\"\nEND\n");
    check(writeHistoryBytes(path, replacement), "resource Reload status fixture replaces exact external source bytes");
    changeHistoryField(pane.get(), kind, kind == ResourceAuthoringKind::sprite ? QStringLiteral("unsaved.mat")
        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("unsaved.wav") : QStringLiteral("unsaved text"));
    int reloadPrompts{};
    QTimer modal{};
    QObject::connect(&modal, &QTimer::timeout, [&]()
    {
        if ( auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            if ( message->objectName() == QStringLiteral("resource_authoring.external_change_prompt") )
            {
                for ( QAbstractButton* const button : message->buttons() )
                {
                    if ( button->text() == QStringLiteral("Reload from Disk") )
                    {
                        ++reloadPrompts;
                        button->click();
                        return;
                    }
                }
            }
            message->reject();
        }
        else if ( auto* const chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()) )
        {
            chooser->reject();
        }
    });
    modal.start(5);
    const bool wroteDraft = ui::resources::resourceAuthoringPaneSave(pane.get(), false);
    modal.stop();
    check(!wroteDraft && reloadPrompts == 1 && status && status->text() == QStringLiteral("Reloaded.")
              && !ui::resources::resourceAuthoringPaneIsModified(pane.get()) && readHistoryBytes(path) == replacement,
          "successful resource Reload keeps the replacement clean and reports no permanent filename or path");
    int openChoosers{};
    QTimer openModal{};
    QObject::connect(&openModal, &QTimer::timeout, [&]()
    {
        if ( auto* const chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()) )
        {
            ++openChoosers;
            chooser->selectFile(QString::fromStdWString(path.wstring()));
            QMetaObject::invokeMethod(chooser, "accept", Qt::DirectConnection);
        }
        else if ( auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            message->reject();
        }
    });
    openModal.start(5);
    const bool opened = ui::resources::resourceAuthoringPaneOpenLooseFileDirect(pane.get());
    openModal.stop();
    check(opened && openChoosers == 1 && status && status->text() == QStringLiteral("Opened.")
              && !ui::resources::resourceAuthoringPaneIsModified(pane.get()) && readHistoryBytes(path) == replacement,
          "successful resource Open retains its clean source and reports no permanent filename or path");
}

void checkValidPendingTableEditAfterUnsuccessfulSave(
    const ResourceAuthoringKind kind, const ProjectType project, const bool failedDestination)
{
    QTemporaryDir temporary{};
    const std::filesystem::path root(temporary.path().toStdWString());
    const QString leaf = kind == ResourceAuthoringKind::soundClass ? QStringLiteral("pending.snd") : QStringLiteral("pending.uni");
    const std::filesystem::path source = root / leaf.toStdWString();
    auto pane = historyPane(kind, project, root);
    QByteArray archiveBytes{};
    const std::filesystem::path archive = root / "pending.gob";
    if ( failedDestination )
    {
        const QByteArray bytes = kind == ResourceAuthoringKind::soundClass
            ? QByteArray("moving old.wav 0x80000040 0.5 2.5 1\n")
            : QByteArray("MSGS 1\n\"CaseKey\" -2147483648 \"old text\"\nEND\n");
        archiveBytes = QByteArray("GOB ", 4);
        appendHistoryU32(archiveBytes, 0U);
        appendHistoryU32(archiveBytes, 0U);
        appendHistoryU32(archiveBytes, 1U);
        appendHistoryU32(archiveBytes, 152U);
        appendHistoryU32(archiveBytes, static_cast<std::uint32_t>(bytes.size()));
        archiveBytes.append(leaf.toLatin1());
        archiveBytes.append(QByteArray(128 - leaf.size(), '\0'));
        archiveBytes.append(bytes);
        check(writeHistoryBytes(archive, archiveBytes)
                  && writeHistoryBytes(root / "misc", QByteArray("blocking resource directory")),
              "pending table Save failure has exact synthetic archive bytes and a deterministic blocked destination");
        QCborMap seed = ui::resources::captureResourceAuthoringPaneState(pane.get());
        seed.remove(QStringLiteral("history")); // The archive defines an independent clean source baseline.
        seed.insert(QStringLiteral("sourceKind"), 2);
        seed.insert(QStringLiteral("name"), leaf);
        seed.insert(QStringLiteral("path"), QString{});
        seed.insert(QStringLiteral("projectResource"), leaf);
        seed.insert(QStringLiteral("draftIdentity"), QString{});
        seed.insert(QStringLiteral("diskHash"), QCryptographicHash::hash(bytes, QCryptographicHash::Sha256));
        if ( kind == ResourceAuthoringKind::stringTable )
        {
            seed.insert(QStringLiteral("capacity"), 1);
        }
        auto locator = std::make_shared<assets::ResourceLocator>();
        locator->mountGob(archive);
        ui::resources::ResourceAuthoringContext context{};
        context.resources = locator;
        context.projectType = project;
        context.projectDirectory = root;
        context.startingDirectory = root;
        pane.reset(ui::resources::createResourceAuthoringPane(nullptr, kind, context));
        check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), seed),
              "pending table Save failure restores the exact clean archive source identity");
    }
    else
    {
        check(saveHistoryFixture(pane.get(), source),
              "pending table Save cancellation starts with a real clean loose source");
    }
    const QCborMap baseline = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QByteArray sourceBytes = failedDestination ? archiveBytes : readHistoryBytes(source);
    const QCborArray beforeRows = baseline.value(QStringLiteral("rows")).toArray();
    const int column = kind == ResourceAuthoringKind::soundClass ? 5 : 1;
    const QString raw = kind == ResourceAuthoringKind::soundClass ? QStringLiteral("0.625") : QStringLiteral("new text");
    QCborArray changedRow = beforeRows.at(0).toArray();
    changedRow[column] = raw;
    const QCborArray changedRows{QCborValue(changedRow)};
    pane->setAttribute(Qt::WA_DontShowOnScreen);
    pane->resize(1000, 650);
    pane->show();
    QLineEdit* line = nullptr;
    if ( kind == ResourceAuthoringKind::soundClass )
    {
        auto* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
        QTreeWidgetItem* row = nullptr;
        for ( int group = 0; tree && group < tree->topLevelItemCount(); ++group )
        {
            for ( int child = 0; child < tree->topLevelItem(group)->childCount(); ++child )
            {
                QTreeWidgetItem* const candidate = tree->topLevelItem(group)->child(child);
                if ( candidate->text(1) == QStringLiteral("old.wav") )
                {
                    row = candidate;
                }
            }
        }
        if ( row )
        {
            tree->setAnimated(false);
            row->parent()->setExpanded(true);
            tree->setCurrentItem(row, column);
            tree->editItem(row, column);
            QApplication::processEvents(QEventLoop::AllEvents);
            QWidget* const editor = tree->indexWidget(tree->indexFromItem(row, column));
            line = editor ? editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) : nullptr;
        }
    }
    else
    {
        auto* const table = pane->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
        if ( table && table->item(0, column) )
        {
            table->setCurrentCell(0, column);
            table->editItem(table->item(0, column));
            QApplication::processEvents(QEventLoop::AllEvents);
            line = qobject_cast<QLineEdit*>(table->indexWidget(table->model()->index(0, column)));
        }
    }
    check(line != nullptr, "unsuccessful table Save starts with a real active uncommitted delegate");
    if ( !line )
    {
        return;
    }
    line->setText(raw);
    QMetaObject::invokeMethod(line, "textEdited", Qt::DirectConnection, Q_ARG(QString, raw));
    const QCborMap pending = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap pendingHistory = pending.value(QStringLiteral("history")).toMap();
    check(pending.value(QStringLiteral("rows")).toArray() == changedRows
              && pending.value(QStringLiteral("modified")).toBool()
              && pendingHistory.value(QStringLiteral("models")).toArray().at(0).toArray().at(column)
                  == beforeRows.at(0).toArray().at(column)
              && pendingHistory.value(QStringLiteral("commands")).toArray().size() == 1
              && pendingHistory.value(QStringLiteral("cursor")).toInteger(-1) == 1,
          "valid pending table text is captured once with its original accepted model before Save");
    int chooserCount{};
    int errorCount{};
    QTimer modal{};
    QObject::connect(&modal, &QTimer::timeout, [&]()
    {
        if ( auto* const chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()) )
        {
            ++chooserCount;
            chooser->reject();
        }
        else if ( auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            ++errorCount;
            message->reject();
        }
    });
    modal.start(5);
    const bool saved = ui::resources::resourceAuthoringPaneSave(pane.get(), !failedDestination);
    modal.stop();
    const QCborMap after = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap history = after.value(QStringLiteral("history")).toMap();
    bool sourceExact = true;
    for ( const QString& key : {QStringLiteral("sourceKind"), QStringLiteral("name"), QStringLiteral("path"),
                              QStringLiteral("projectResource"), QStringLiteral("draftIdentity"), QStringLiteral("diskHash")} )
    {
        sourceExact = sourceExact && after.value(key) == baseline.value(key);
    }
    const bool retained = !saved && chooserCount == (failedDestination ? 0 : 1) && errorCount == (failedDestination ? 1 : 0)
              && sourceExact && after.value(QStringLiteral("rows")).toArray() == changedRows
              && after.value(QStringLiteral("modified")).toBool()
              && history.value(QStringLiteral("commands")).toArray().size() == 1
              && history.value(QStringLiteral("cursor")).toInteger(-1) == 1
              && history.value(QStringLiteral("clean")).toInteger(-1) == 0
              && ui::resources::validateResourceAuthoringPaneState(after)
              && readHistoryBytes(failedDestination ? archive : source) == sourceBytes;
    if ( !retained )
    {
        std::cerr << "Pending table Save diagnostic: kind=" << static_cast<int>(kind)
            << " project=" << static_cast<int>(project) << " failedDestination=" << failedDestination
            << " saved=" << saved << " chooser=" << chooserCount << " errors=" << errorCount
            << " sourceExact=" << sourceExact << " rawExact=" << (after.value(QStringLiteral("rows")).toArray() == changedRows)
            << " modified=" << after.value(QStringLiteral("modified")).toBool()
            << " commands=" << history.value(QStringLiteral("commands")).toArray().size()
            << " cursor=" << history.value(QStringLiteral("cursor")).toInteger(-1)
            << " clean=" << history.value(QStringLiteral("clean")).toInteger(-1) << '\n';
    }
    check(retained,
          "cancelled or failed Save retains exact valid table raw text source identity dirty state and one usable command");
    // Save may finish a valid delegate. Its accepted model representation can
    // change, but the field edit must remain one exact Undo/Redo command.
    check(ui::resources::resourceAuthoringPaneDocumentUndoRedo(pane.get(), false)
              && resourceContent(pane.get()).value(QStringLiteral("rows")).toArray() == beforeRows
              && !ui::resources::resourceAuthoringPaneIsModified(pane.get())
              && !ui::resources::resourceAuthoringPaneCanUndoRedo(pane.get(), false)
              && ui::resources::resourceAuthoringPaneCanUndoRedo(pane.get(), true)
              && ui::resources::resourceAuthoringPaneDocumentUndoRedo(pane.get(), true)
              && resourceContent(pane.get()).value(QStringLiteral("rows")).toArray() == changedRows
              && ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "after unsuccessful Save table document Undo reaches the original clean value and Redo restores the exact pending edit once");
    auto restored = historyPane(kind, project, root);
    check(ui::resources::restoreResourceAuthoringPaneState(restored.get(), nativeHistoryRoundTrip(after, project))
              && resourceContent(restored.get()).value(QStringLiteral("rows")).toArray() == changedRows
              && resourceUndo(restored.get(), false)
              && resourceContent(restored.get()).value(QStringLiteral("rows")).toArray() == beforeRows
              && !ui::resources::resourceAuthoringPaneIsModified(restored.get())
              && resourceUndo(restored.get(), true)
              && resourceContent(restored.get()).value(QStringLiteral("rows")).toArray() == changedRows
              && ui::resources::resourceAuthoringPaneIsModified(restored.get()),
          "unsuccessful Save preserves the same exact table edit and clean Undo through native history restoration");
}

} // namespace

void runResourceAuthoringHistoryTests()
{
    for ( const ProjectType project : {ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM} )
    {
        for ( const ResourceAuthoringKind kind : {ResourceAuthoringKind::sprite, ResourceAuthoringKind::soundClass,
                                                 ResourceAuthoringKind::stringTable} )
        {
            checkResourceHistoryRoundTrip(kind, project);
            checkArchiveAutomaticSave(kind, project, true);
            checkArchiveAutomaticSave(kind, project, false);
        }
        checkPendingSpriteHistory(project);
        checkSpriteNumericHistoryGestureBoundary(project);
        checkSoundPendingHistoryDiscardsSavedRedo(project);
        checkSoundEmptyModeHistory(project);
        for ( const ResourceAuthoringKind kind : {ResourceAuthoringKind::soundClass, ResourceAuthoringKind::stringTable} )
        {
            checkValidPendingTableEditAfterUnsuccessfulSave(kind, project, false);
            checkValidPendingTableEditAfterUnsuccessfulSave(kind, project, true);
        }
    }
    checkHistoryValidationAtomicity();
    for ( const ResourceAuthoringKind kind : {ResourceAuthoringKind::sprite, ResourceAuthoringKind::soundClass,
                                             ResourceAuthoringKind::stringTable} )
    {
        checkResourceLiveHistoryCommandLimit(kind);
    }
    checkCogLiveHistoryLimits(false);
    checkCogLiveHistoryLimits(true);
    checkCogTypingSaveHistoryBoundary();
    checkCogIdenticalReplacementHistory();
    checkCogNonReconstructibleHistoryRejected();
    checkCogHistoryRoundTrip();
    for ( const bool savedProject : {false, true} )
    {
        checkCogDefaultSave(savedProject, false);
        checkCogDefaultSave(savedProject, true);
    }
    checkCogFormatAndReloadHistory();
    checkAggregateNativeHistory();
    for ( const ResourceAuthoringKind kind : {ResourceAuthoringKind::sprite, ResourceAuthoringKind::soundClass,
                                             ResourceAuthoringKind::stringTable} )
    {
        checkResourceFilePathChrome(kind, false);
        checkResourceFilePathChrome(kind, true);
        checkResourceSuccessStatusHasNoPermanentPath(kind);
    }
}

} // namespace sed::app_tests
