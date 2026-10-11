#include "test_common.hpp"

#include "main_window/main_window.hpp"
#include "sed/editor/document.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/recovery/document_recovery_utils.hpp"
#include "ui/resources/resource_authoring_editor.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QCryptographicHash>
#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSaveFile>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>

namespace sed::app_tests
{
namespace
{

using ui::resources::ResourceAuthoringKind;

constexpr const char* kChangePrompt = "sed.external_document_change_prompt";
const QByteArray kCogWatchSource("symbols\nint value=1\nend\ncode\nend\n");

QString expectedWatchText(const QString& leaf, const bool removed = false)
{
    const QString first = removed ? QStringLiteral("\"%1\" is no longer available on disk.")
                                  : QStringLiteral("\"%1\" changed on disk.");
    return first.arg(leaf) + QStringLiteral(
        "\n\nReload replaces the current buffer and discards its unsaved changes. Keep preserves the current buffer.");
}

bool waitForWatch(const std::function<bool()>& predicate, const int timeoutMs = 2500)
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
    timeout.start(timeoutMs);
    loop.exec();
    return predicate();
}

// Negative notification checks still dispatch real filesystem and UI events;
// they do not sleep or synthesize the production watcher's private signals.
void observeQuietWatchEvents(const int durationMs = 450)
{
    QEventLoop loop{};
    QTimer::singleShot(durationMs, &loop, &QEventLoop::quit);
    loop.exec();
}

bool replaceWatchBytes(const std::filesystem::path& path, const QByteArray& bytes)
{
    std::error_code error{};
    std::filesystem::create_directories(path.parent_path(), error);
    QSaveFile file(QString::fromStdWString(path.wstring()));
    file.setDirectWriteFallback(false);
    return !error && file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

QByteArray readWatchBytes(const std::filesystem::path& path)
{
    QFile file(QString::fromStdWString(path.wstring()));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

QByteArray canonicalWatchState(const QCborValue& value)
{
    return ui::recovery::serializeCanonicalCbor(value);
}

QCborMap watchedResourceDraft(QWidget* const pane)
{
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane);
    state.remove(QStringLiteral("paneGeometry"));
    state.remove(QStringLiteral("presentation"));
    return state;
}

class WatchDialogs final
{
public:
    enum class Reply
    {
        keep,
        reload,
    };

    WatchDialogs()
    {
        QObject::connect(&timer_, &QTimer::timeout, [&]()
            {
                auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if ( !message || handled_ == message )
                {
                    return;
                }
                handled_ = message;
                if ( overwriteConflict && message->objectName() == QStringLiteral("resource_authoring.external_change_prompt") )
                {
                    for ( QAbstractButton* const button : message->buttons() )
                    {
                        if ( button->text() == QStringLiteral("Overwrite Disk") )
                        {
                            ++overwrites;
                            button->click();
                            return;
                        }
                    }
                }
                int visible{};
                for ( QWidget* const widget : QApplication::topLevelWidgets() )
                {
                    visible += qobject_cast<QMessageBox*>(widget) && widget->isVisible() ? 1 : 0;
                }
                maximumVisible = std::max(maximumVisible, visible);
                const bool change = message->objectName() == QString::fromLatin1(kChangePrompt)
                    || (message->windowTitle() == QStringLiteral("Reload COG script")
                        && message->icon() == QMessageBox::Question);
                if ( !change )
                {
                    ++errors;
                    lastError = message->text();
                    lastErrorTitle = message->windowTitle();
                    message->reject();
                    return;
                }
                ++changes;
                lastText = message->text();
                lastTitle = message->windowTitle();
                lastObject = message->objectName();
                changeTexts.push_back(lastText);
                QAbstractButton* reloadButton{};
                QAbstractButton* keepButton{};
                for ( QAbstractButton* const button : message->buttons() )
                {
                    if ( button->text() == QStringLiteral("Reload") )
                    {
                        reloadButton = button;
                    }
                    if ( button->text() == QStringLiteral("Keep") )
                    {
                        keepButton = button;
                    }
                }
                exactButtons = reloadButton && keepButton && message->buttons().size() == 2;
                if ( beforeAnswer )
                {
                    const auto callback = beforeAnswer;
                    callback(message);
                }
                if ( !handled_ )
                {
                    return;
                }
                QAbstractButton* const answer = reply == Reply::reload ? reloadButton : keepButton;
                if ( answer )
                {
                    answer->click();
                }
                else
                {
                    message->reject();
                }
            });
        timer_.start(5);
    }

    Reply reply{Reply::keep};
    int changes{};
    int errors{};
    int overwrites{};
    int maximumVisible{};
    bool exactButtons{};
    bool overwriteConflict{};
    QString lastText{};
    QString lastTitle{};
    QString lastObject{};
    QString lastError{};
    QString lastErrorTitle{};
    QStringList changeTexts{};
    std::function<void(QMessageBox*)> beforeAnswer{};

private:
    QTimer timer_{};
    QPointer<QMessageBox> handled_{};
};

QByteArray resourceWatchBytes(const ResourceAuthoringKind kind, const ProjectType project, const QByteArray& value)
{
    if ( kind == ResourceAuthoringKind::sprite )
    {
        return value + (project == ProjectType::IJIM ? " 0 1 2 4 0 0 0 0 0 1 0 0 0\n"
                                                  : " 0 1 2 4 0 0 0 0 0 0\n");
    }
    if ( kind == ResourceAuthoringKind::soundClass )
    {
        return QByteArray("moving ") + value + " 0x80000040 0.5 2.5 1\n";
    }
    return QByteArray("MSGS 1\n\"WatchKey\" -2147483648 \"") + value + "\"\nEND\n";
}

QString resourceWatchValue(const ResourceAuthoringKind kind, const char* const phase)
{
    return QString::fromLatin1(phase) + (kind == ResourceAuthoringKind::sprite ? QStringLiteral(".mat")
        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral(".wav") : QStringLiteral(" value"));
}

QString resourceCapturedValue(const QCborMap& state, const ResourceAuthoringKind kind)
{
    if ( kind == ResourceAuthoringKind::sprite )
    {
        const QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
        return sprite.isEmpty() ? QString{} : sprite.at(0).toString();
    }
    const QCborArray rows = state.value(QStringLiteral("rows")).toArray();
    // Both resource rows put the editable Sound/Text in slot 1. UNI slot 2
    // retains the hidden signed legacy metadata and is checked separately.
    const QCborArray row = rows.isEmpty() ? QCborArray{} : rows.at(0).toArray();
    return row.size() < 2 ? QString{} : row.at(1).toString();
}

void editWatchedResource(QWidget* const pane, const ResourceAuthoringKind kind, const QString& value)
{
    if ( kind == ResourceAuthoringKind::sprite )
    {
        if ( QLineEdit* const material = pane->findChild<QLineEdit*>(QStringLiteral("spr.material")) )
        {
            material->setText(value);
            QMetaObject::invokeMethod(material, "editingFinished", Qt::DirectConnection);
        }
    }
    else if ( kind == ResourceAuthoringKind::soundClass )
    {
        QTreeWidget* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
        for ( int group = 0; tree && group < tree->topLevelItemCount(); ++group )
        {
            QTreeWidgetItem* const mode = tree->topLevelItem(group);
            for ( int child = 0; child < mode->childCount(); ++child )
            {
                if ( mode->child(child)->text(1).endsWith(QStringLiteral(".wav")) )
                {
                    mode->child(child)->setText(1, value);
                    return;
                }
            }
        }
    }
    else if ( QTableWidget* const table = pane->findChild<QTableWidget*>(QStringLiteral("uni.entries")) )
    {
        if ( table->rowCount() && table->item(0, 1) )
        {
            table->item(0, 1)->setText(value);
        }
    }
}

bool selectWatchFile(QWidget* const pane, const std::filesystem::path& path, const bool saveAs)
{
    int choosers{};
    QTimer timer{};
    QObject::connect(&timer, &QTimer::timeout, [&]()
        {
            if ( auto* const chooser = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()) )
            {
                ++choosers;
                chooser->setOption(QFileDialog::DontConfirmOverwrite, true);
                chooser->selectFile(QString::fromStdWString(path.wstring()));
                QMetaObject::invokeMethod(chooser, "accept", Qt::DirectConnection);
            }
        });
    timer.start(5);
    const bool result = saveAs ? ui::resources::resourceAuthoringPaneSave(pane, true)
        : ui::resources::resourceAuthoringPaneOpenLooseFileDirect(pane);
    check(choosers == 1, "loose document watch fixture uses one real explicit source chooser");
    return result;
}

std::unique_ptr<QWidget> openWatchedResource(const ResourceAuthoringKind kind,
    const ProjectType project, const std::filesystem::path& path)
{
    ui::resources::ResourceAuthoringContext context{};
    context.projectType = project;
    context.resources = std::make_shared<assets::ResourceLocator>();
    context.projectDirectory = path.parent_path();
    context.startingDirectory = path.parent_path();
    std::unique_ptr<QWidget> pane(ui::resources::createResourceAuthoringPane(nullptr, kind, context));
    pane->show();
    const QString original = resourceWatchValue(kind, "original");
    const QByteArray bytes = resourceWatchBytes(kind, project, original.toLatin1());
    const bool opened = replaceWatchBytes(path, bytes) && selectWatchFile(pane.get(), path, false);
    const QCborMap state = watchedResourceDraft(pane.get());
    const QCborArray rows = state.value(QStringLiteral("rows")).toArray();
    const QCborArray firstRow = rows.isEmpty() ? QCborArray{} : rows.at(0).toArray();
    const bool hiddenMetadata = kind != ResourceAuthoringKind::stringTable
        || (firstRow.size() == 3 && firstRow.at(2).toString() == QStringLiteral("-2147483648"));
    check(opened && !ui::resources::resourceAuthoringPaneIsModified(pane.get())
            && resourceCapturedValue(state, kind) == original && hiddenMetadata,
        "loose document watch fixture opens the exact independent clean engine-profile source including UNI hidden signed metadata");
    if ( !opened )
    {
        return {};
    }
    return pane;
}

void checkResourceExternalChange(const ResourceAuthoringKind kind, const ProjectType project)
{
    QTemporaryDir temporary{};
    if ( !temporary.isValid() )
    {
        check(false, "the resource watcher fixture creates a private source directory before any file write");
        return;
    }
    const QString leaf = kind == ResourceAuthoringKind::sprite ? QStringLiteral("WatchCase.spr")
        : kind == ResourceAuthoringKind::soundClass ? QStringLiteral("WatchCase.snd") : QStringLiteral("WatchCase.uni");
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::filesystem::path path = root / leaf.toStdWString();
    auto pane = openWatchedResource(kind, project, path);
    if ( !pane )
    {
        return;
    }
    editWatchedResource(pane.get(), kind, resourceWatchValue(kind, "local"));
    editWatchedResource(pane.get(), kind, resourceWatchValue(kind, "future"));
    const bool undone = ui::resources::resourceAuthoringPaneDocumentUndoRedo(pane.get(), false);
    if ( kind == ResourceAuthoringKind::sprite )
    {
        auto* const width = pane->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"));
        if ( width )
        {
            width->lineEdit()->setText(QStringLiteral("1e"));
            QMetaObject::invokeMethod(width->lineEdit(), "textEdited", Qt::DirectConnection,
                Q_ARG(QString, QStringLiteral("1e")));
        }
    }
    const QCborMap before = watchedResourceDraft(pane.get());
    check(undone && before.value(QStringLiteral("modified")).toBool(),
        "external-change fixture retains a real dirty local timeline before watching disk revisions");
    WatchDialogs dialogs{};
    const QByteArray external = resourceWatchBytes(kind, project, resourceWatchValue(kind, "external").toLatin1());
    check(replaceWatchBytes(path, external), "an external editor atomically replaces the watched loose resource");
    const bool prompted = waitForWatch([&]() { return dialogs.changes == 1; });
    const bool kept = prompted && dialogs.lastObject == QString::fromLatin1(kChangePrompt)
        && dialogs.exactButtons && dialogs.lastTitle == QStringLiteral("File changed on disk")
        && dialogs.lastText == expectedWatchText(leaf)
        && canonicalWatchState(watchedResourceDraft(pane.get())) == canonicalWatchState(before)
        && readWatchBytes(path) == external && dialogs.maximumVisible == 1;
    if ( !kept )
    {
        std::cerr << "Loose resource watch diagnostic: project=" << static_cast<int>(project)
            << " kind=" << static_cast<int>(kind) << " prompted=" << prompted << " changes=" << dialogs.changes
            << " object=" << dialogs.lastObject.toStdString() << " exactButtons=" << dialogs.exactButtons
            << " exactText=" << (dialogs.lastText == expectedWatchText(leaf)) << " draftExact="
            << (canonicalWatchState(watchedResourceDraft(pane.get())) == canonicalWatchState(before)) << '\n';
    }
    check(kept,
        "automatic named Reload or Keep offer detects an atomic loose resource replacement and Keep preserves exact raw identity history cursor clean Redo and dirty state");
    if ( !kept )
    {
        return;
    }
    check(replaceWatchBytes(root / "unrelated.tmp", QByteArray("unrelated directory notification\n")),
        "the external-change acknowledgement fixture produces a real unrelated directory notification");
    observeQuietWatchEvents();
    check(dialogs.changes == 1 && canonicalWatchState(watchedResourceDraft(pane.get())) == canonicalWatchState(before),
        "Keep acknowledges one observed disk revision without repeating its prompt on unrelated directory changes");

    dialogs.reply = WatchDialogs::Reply::reload;
    const QString replacement = resourceWatchValue(kind, "reloaded");
    const QByteArray replacementBytes = resourceWatchBytes(kind, project, replacement.toLatin1());
    check(replaceWatchBytes(path, replacementBytes), "a later distinct revision replaces the same watched loose resource again");
    const bool reloaded = waitForWatch([&]()
        {
            return dialogs.changes == 2 && resourceCapturedValue(watchedResourceDraft(pane.get()), kind) == replacement;
        });
    const QCborMap clean = watchedResourceDraft(pane.get());
    const QCborMap history = clean.value(QStringLiteral("history")).toMap();
    check(reloaded && dialogs.errors == 0 && dialogs.lastTitle == QStringLiteral("File changed on disk")
            && dialogs.lastText == expectedWatchText(leaf) && !clean.value(QStringLiteral("modified")).toBool()
            && history.value(QStringLiteral("cursor")).toInteger(-1) == 0
            && history.value(QStringLiteral("commands")).toArray().isEmpty()
            && !ui::resources::resourceAuthoringPaneCanUndoRedo(pane.get(), false)
            && !ui::resources::resourceAuthoringPaneCanUndoRedo(pane.get(), true)
            && !clean.contains(QStringLiteral("spritePendingNumericEdits"))
            && clean.value(QStringLiteral("diskHash")).toByteArray() == QCryptographicHash::hash(replacementBytes, QCryptographicHash::Sha256),
        "accepting automatic Reload validates the new source and establishes one clean replacement baseline with reset local history");
    editWatchedResource(pane.get(), kind, resourceWatchValue(kind, "retained"));
    const QCborMap retained = watchedResourceDraft(pane.get());
    const QByteArray malformedReload = kind == ResourceAuthoringKind::sprite ? QByteArray("broken SPR\n")
        : kind == ResourceAuthoringKind::soundClass ? QByteArray("moving wrong.wav BAD_FLAGS 0.5 2.5 1\n")
                                                   : QByteArray("MSGS 1\nbroken UNI\nEND\n");
    check(replaceWatchBytes(path, malformedReload), "the watched external replacement can be syntactically malformed");
    const bool failedReload = waitForWatch([&]() { return dialogs.changes == 3 && dialogs.errors == 1; });
    const QString parseError = kind == ResourceAuthoringKind::sprite
        ? QStringLiteral("Bad data in SPR (expected 14 args for IJIM or >=11 for JKDF2/MOTS).")
        : kind == ResourceAuthoringKind::soundClass
            ? QStringLiteral("Bad sound-class data on line 1: invalid hexadecimal playback flags.")
            : QStringLiteral("%1 parse error at line 2: message key must be quoted")
                .arg(ui::resources::resourceAuthoringPaneLooseFilePath(pane.get()));
    const QString expectedError = QStringLiteral("Could not reload \"%1\".\n\n%2").arg(leaf, parseError);
    check(failedReload && dialogs.lastErrorTitle == QStringLiteral("Could not reload file")
            && dialogs.lastError == expectedError
            && canonicalWatchState(watchedResourceDraft(pane.get())) == canonicalWatchState(retained)
            && readWatchBytes(path) == malformedReload,
        "a named automatic Reload error preserves the exact existing raw buffer identity history and dirty state");

    int finalRejectedReloadChangeCount = 3;
    int finalRejectedReloadErrorCount = 1;
    if ( kind == ResourceAuthoringKind::sprite )
    {
        const QByteArray oversizedMaterial(256, 'm');
        const QByteArray oversizedReload = oversizedMaterial + (project == ProjectType::IJIM
            ? QByteArray(" 0 1 2 4 0 0 0 0 0 1 0 0 0\n")
            : QByteArray(" 0 1 2 4 0 0 0 0 0 0\n"));
        dialogs.reply = WatchDialogs::Reply::reload;
        check(replaceWatchBytes(path, oversizedReload),
            "the external SPR replacement contains a valid numeric record with a 256-byte material token");
        const bool oversizedReloadRejected = waitForWatch([&]()
        {
            return dialogs.changes == 4 && dialogs.errors == 2;
        });
        const QString oversizedError = QStringLiteral("Could not reload \"%1\".\n\n"
            "SPR material must be a safe resource token.").arg(leaf);
        check(oversizedReloadRejected && dialogs.lastErrorTitle == QStringLiteral("Could not reload file")
                && dialogs.lastError == oversizedError
                && canonicalWatchState(watchedResourceDraft(pane.get())) == canonicalWatchState(retained)
                && readWatchBytes(path) == oversizedReload,
            "automatic Reload rejects a numerically valid 256-byte SPR token without replacing the exact dirty draft or history");
        finalRejectedReloadChangeCount = 4;
        finalRejectedReloadErrorCount = 2;
    }

    dialogs.reply = WatchDialogs::Reply::keep;
    check(QFile::remove(QString::fromStdWString(path.wstring())), "external deletion removes the watched loose source without touching its draft");
    const bool removed = waitForWatch([&]() { return dialogs.changes == finalRejectedReloadChangeCount + 1; });
    check(removed && dialogs.lastTitle == QStringLiteral("File removed from disk")
            && dialogs.lastText == expectedWatchText(leaf, true)
            && canonicalWatchState(watchedResourceDraft(pane.get())) == canonicalWatchState(retained)
            && !std::filesystem::exists(path),
        "an externally deleted loose resource produces one named offer and Keep retains its exact unsaved buffer without recreating a file");
    dialogs.reply = WatchDialogs::Reply::reload;
    check(replaceWatchBytes(path, replacementBytes), "an external editor recreates the removed source at its original path");
    check(waitForWatch([&]()
        {
            return dialogs.changes == finalRejectedReloadChangeCount + 2 && resourceCapturedValue(watchedResourceDraft(pane.get()), kind) == replacement
                && !ui::resources::resourceAuthoringPaneIsModified(pane.get());
        }), "the retained parent-directory watch detects recreation and Reload restores the new clean source");
    editWatchedResource(pane.get(), kind, resourceWatchValue(kind, "ownsave"));
    const bool saved = ui::resources::resourceAuthoringPaneSave(pane.get(), false);
    observeQuietWatchEvents();
    check(saved && dialogs.changes == finalRejectedReloadChangeCount + 2
              && dialogs.errors == finalRejectedReloadErrorCount && !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
        "an ordinary resource Save accepts its own exact bytes before queued filesystem events and produces no external-change offer");
    const std::filesystem::path renamed = root / (QStringLiteral("Renamed.") + leaf.section(QLatin1Char('.'), -1)).toStdWString();
    const bool savedAs = selectWatchFile(pane.get(), renamed, true);
    dialogs.reply = WatchDialogs::Reply::keep;
    const QCborMap renamedDraft = watchedResourceDraft(pane.get());
    check(replaceWatchBytes(path, external), "the previous source still changes after Save As moved document ownership");
    observeQuietWatchEvents();
    check(savedAs && dialogs.changes == finalRejectedReloadChangeCount + 2
              && canonicalWatchState(watchedResourceDraft(pane.get())) == canonicalWatchState(renamedDraft),
        "Save As removes the previous loose source watch without changing the current renamed document");
    check(replaceWatchBytes(renamed, external), "the newly owned Save As destination receives a genuine external replacement");
    check(waitForWatch([&]() { return dialogs.changes == finalRejectedReloadChangeCount + 3; })
            && dialogs.lastTitle == QStringLiteral("File changed on disk")
            && dialogs.lastText == expectedWatchText(QString::fromStdWString(renamed.filename().wstring()))
            && canonicalWatchState(watchedResourceDraft(pane.get())) == canonicalWatchState(renamedDraft),
        "Save As rebinds automatic external-change offers to the exact new filename and preserves its clean buffer when kept");
}

QWidget* watchedCogPage(const MainWindow& window, const QString& leaf)
{
    for ( QTabWidget* const tabs : window.findChildren<QTabWidget*>() )
    {
        for ( int index = 0; index < tabs->count(); ++index )
        {
            QWidget* const page = tabs->widget(index);
            if ( page && QFileInfo(page->property("cogFullPath").toString()).fileName() == leaf )
            {
                return page;
            }
        }
    }
    return nullptr;
}

QCborMap watchedCogDraft(const MainWindow& window, const QString& leaf)
{
    const QCborMap workspace = QCborValue::fromCbor(window.captureCogEditorRecoveryStateForTest()).toMap();
    for ( const QCborValue& value : workspace.value(QStringLiteral("tabs")).toArray() )
    {
        QCborMap page = value.toMap();
        if ( page.value(QStringLiteral("name")).toString() != leaf )
        {
            continue;
        }
        // These allocations may settle while a modal is shown. Every source,
        // text, selection, dirty and native-journal field remains compared.
        for ( const QString& key : {QStringLiteral("splitter"), QStringLiteral("outlineSplitter"),
                  QStringLiteral("diagnosticsSplitter"), QStringLiteral("diagnosticsExpandedHeight"),
                  QStringLiteral("vscroll"), QStringLiteral("hscroll")} )
        {
            page.remove(key);
        }
        return page;
    }
    return {};
}

void appendWatchCogCommand(QPlainTextEdit* const editor, const QString& text)
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

void bindWatchCogProject(MainWindow& window, const ProjectType project, const std::filesystem::path& root)
{
    editor::DocumentState document{};
    document.level.kind = project;
    document.level.header.version = 1;
    document.level.layers = {"Base"};
    window.setDocumentStateForTest(std::move(document));
    window.setProjectTypeForTest(project);
    window.setCurrentPathForTitleForTest(root / "watching.sed");
    window.setResourcePathsForTest({root}, {}, root / "watching.sed");
    window.resize(1100, 800);
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();
}

void checkCogExternalChange(const ProjectType project)
{
    QTemporaryDir temporary{};
    if ( !temporary.isValid() )
    {
        check(false, "COG external-change fixture has an isolated loose source directory");
        return;
    }
    const std::filesystem::path root(temporary.path().toStdWString());
    const QString leaf = QStringLiteral("WatchCase.cog");
    const std::filesystem::path path = root / "cog" / leaf.toStdWString();
    MainWindow window(nullptr, true);
    check(replaceWatchBytes(path, kCogWatchSource), "COG watch fixture writes exact independent script bytes before opening");
    bindWatchCogProject(window, project, root);
    window.openCogScriptEditorForTest(leaf.toStdString());
    QPointer<QWidget> page(watchedCogPage(window, leaf));
    QPointer<QPlainTextEdit> editor(page ? page->findChild<QPlainTextEdit*>() : nullptr);
    QPointer<QTextDocument> document(editor ? editor->document() : nullptr);
    const bool opened = page && editor && document && editor->toPlainText().toUtf8() == kCogWatchSource
        && !document->isModified();
    check(opened, "COG external-change fixture opens the exact clean source in its active project profile");
    if ( !opened )
    {
        return;
    }
    check(!window.findChild<QToolButton*>(QStringLiteral("cogEditorReloadButton")),
        "COG external-change fixture has no manual Reload toolbar button in its active project profile");
    appendWatchCogCommand(editor, QStringLiteral("# local unclosed \"\n"));
    appendWatchCogCommand(editor, QStringLiteral("# future \U0001f642\n"));
    editor->undo();
    QTextCursor selection(document);
    selection.setPosition(2);
    selection.setPosition(8, QTextCursor::KeepAnchor);
    editor->setTextCursor(selection);
    const QCborMap before = watchedCogDraft(window, leaf);
    const int undoSteps = document->availableUndoSteps();
    const int redoSteps = document->availableRedoSteps();
    WatchDialogs dialogs{};
    const QByteArray external = kCogWatchSource + "# external first\n";
    check(replaceWatchBytes(path, external), "an external editor atomically replaces an open COG source");
    const bool prompted = waitForWatch([&]() { return dialogs.changes == 1; });
    const bool kept = prompted && dialogs.lastObject == QString::fromLatin1(kChangePrompt)
        && dialogs.exactButtons && dialogs.lastTitle == QStringLiteral("File changed on disk")
        && dialogs.lastText == expectedWatchText(leaf) && dialogs.maximumVisible == 1
        && canonicalWatchState(watchedCogDraft(window, leaf)) == canonicalWatchState(before)
        && editor && editor->document() == document && document->availableUndoSteps() == undoSteps
        && document->availableRedoSteps() == redoSteps && readWatchBytes(path) == external;
    if ( !kept )
    {
        std::cerr << "Loose COG watch diagnostic: project=" << static_cast<int>(project)
            << " prompted=" << prompted << " object=" << dialogs.lastObject.toStdString()
            << " buttons=" << dialogs.exactButtons << " exactText=" << (dialogs.lastText == expectedWatchText(leaf))
            << " exact=" << (canonicalWatchState(watchedCogDraft(window, leaf)) == canonicalWatchState(before)) << '\n';
    }
    check(kept,
        "an automatic named COG Reload or Keep offer preserves the same native document raw selection clean marker and Redo branch when kept");
    if ( !kept )
    {
        return;
    }
    check(replaceWatchBytes(root / "cog" / "unrelated.tmp", QByteArray("directory event\n")),
        "COG acknowledgement fixture delivers a real unrelated parent-directory event");
    observeQuietWatchEvents();
    check(dialogs.changes == 1 && canonicalWatchState(watchedCogDraft(window, leaf)) == canonicalWatchState(before),
        "keeping an observed COG disk revision prevents repeated offers without changing its saved source baseline");

    dialogs.reply = WatchDialogs::Reply::reload;
    const QByteArray replacement = kCogWatchSource + "# reloaded second\n";
    check(replaceWatchBytes(path, replacement), "the same open COG receives a later distinct atomic replacement");
    const bool reloaded = waitForWatch([&]()
        {
            return dialogs.changes == 2 && editor && editor->toPlainText().toUtf8() == replacement;
        });
    const QCborMap reloadedDraft = watchedCogDraft(window, leaf);
    const QCborMap history = reloadedDraft.value(QStringLiteral("history")).toMap();
    check(reloaded && dialogs.lastTitle == QStringLiteral("File changed on disk")
            && dialogs.lastText == expectedWatchText(leaf)
            && document && editor->document() == document && !document->isModified()
            && !document->isUndoAvailable() && !document->isRedoAvailable()
            && history.value(QStringLiteral("cursor")).toInteger(-1) == 0
            && history.value(QStringLiteral("commands")).toArray().isEmpty() && dialogs.errors == 0,
        "accepting COG Reload establishes the exact replacement text as a clean source and resets its native local stack once");

    // Even a clean buffer must offer the choice. A silent clean reload loses
    // the explicit user decision requested for every open document.
    dialogs.reply = WatchDialogs::Reply::keep;
    const QCborMap cleanBefore = watchedCogDraft(window, leaf);
    check(replaceWatchBytes(path, external), "a clean open COG source receives a distinct external revision");
    check(waitForWatch([&]() { return dialogs.changes == 3; })
            && dialogs.lastTitle == QStringLiteral("File changed on disk") && dialogs.lastText == expectedWatchText(leaf)
            && canonicalWatchState(watchedCogDraft(window, leaf)) == canonicalWatchState(cleanBefore),
        "clean COG buffers also offer named Reload or Keep and never silently replace the text when kept");

    appendWatchCogCommand(editor, QStringLiteral("# retained after deletion\n"));
    const QCborMap retained = watchedCogDraft(window, leaf);
    check(QFile::remove(QString::fromStdWString(path.wstring())), "an external editor deletes the open COG source");
    check(waitForWatch([&]() { return dialogs.changes == 4; })
            && dialogs.lastTitle == QStringLiteral("File removed from disk") && dialogs.lastText == expectedWatchText(leaf, true)
            && canonicalWatchState(watchedCogDraft(window, leaf)) == canonicalWatchState(retained)
            && !std::filesystem::exists(path),
        "COG deletion offers one named choice and Keep neither clears the raw local timeline nor recreates its file");
    dialogs.reply = WatchDialogs::Reply::reload;
    check(replaceWatchBytes(path, replacement), "the deleted COG source is recreated by the external editor");
    check(waitForWatch([&]()
        {
            return dialogs.changes == 5 && editor && editor->toPlainText().toUtf8() == replacement
                && !document->isModified();
        }), "COG parent-directory watching survives atomic replacement and deletion to detect a recreated source");

    appendWatchCogCommand(editor, QStringLiteral("# own save\n"));
    const QByteArray saved = editor->toPlainText().toUtf8();
    const bool saveResult = window.saveCogScriptEditorForTest(0);
    observeQuietWatchEvents();
    check(saveResult && readWatchBytes(path) == saved && !document->isModified()
            && dialogs.changes == 5 && dialogs.errors == 0,
        "COG Save accepts the exact written revision before filesystem delivery without a time-window suppression or self-change offer");
}

void checkSharedWatchQueueDefersMouseGrabAndOwnerDeletion()
{
    QTemporaryDir temporary{};
    if ( !temporary.isValid() )
    {
        check(false, "the shared loose-document queue fixture has a private source directory");
        return;
    }
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::filesystem::path firstPath = root / "First.uni";
    const std::filesystem::path secondPath = root / "Second.snd";
    auto first = openWatchedResource(ResourceAuthoringKind::stringTable, ProjectType::JKDF2, firstPath);
    auto second = openWatchedResource(ResourceAuthoringKind::soundClass, ProjectType::JKDF2, secondPath);
    if ( !first || !second )
    {
        return;
    }
    const QCborMap firstBefore = watchedResourceDraft(first.get());
    const QCborMap secondBefore = watchedResourceDraft(second.get());
    WatchDialogs dialogs{};
    bool directlyDeletedPrompt{};
    dialogs.beforeAnswer = [&](QMessageBox* message)
        {
            dialogs.beforeAnswer = {};
            directlyDeletedPrompt = true;
            delete message;
        };
    first->grabMouse();
    const bool grabbed = QWidget::mouseGrabber() == first.get();
    const bool written = replaceWatchBytes(firstPath,
            resourceWatchBytes(ResourceAuthoringKind::stringTable, ProjectType::JKDF2, "external first"))
        && replaceWatchBytes(secondPath,
            resourceWatchBytes(ResourceAuthoringKind::soundClass, ProjectType::JKDF2, "external.wav"));
    observeQuietWatchEvents();
    check(grabbed && written && dialogs.changes == 0 && !QApplication::activeModalWidget(),
        "automatic loose-document prompts defer during a real mouse grab so editing or dragging is not interrupted");
    first->releaseMouse();
    const bool delivered = waitForWatch([&]() { return dialogs.changes == 2; });
    QStringList actualTexts = dialogs.changeTexts;
    actualTexts.sort();
    QStringList expectedTexts{expectedWatchText(QStringLiteral("First.uni")), expectedWatchText(QStringLiteral("Second.snd"))};
    expectedTexts.sort();
    check(delivered && directlyDeletedPrompt && dialogs.maximumVisible == 1 && dialogs.errors == 0
            && actualTexts == expectedTexts
            && canonicalWatchState(watchedResourceDraft(first.get())) == canonicalWatchState(firstBefore)
            && canonicalWatchState(watchedResourceDraft(second.get())) == canonicalWatchState(secondBefore),
        "the shared event-driven queue resumes after direct prompt deletion and serializes all open-resource offers after the mouse gesture finishes");
    if ( !delivered )
    {
        return;
    }
    const auto watchers = qApp->findChildren<QFileSystemWatcher*>(QStringLiteral("sed.loose_document_watcher"));
    check(watchers.size() == 1 && watchers.front()->files().size() == 2
            && watchers.front()->directories().size() == 1,
        "open loose documents share one deduplicated filesystem watcher and one reference-counted parent-directory watch");
    QPointer<QWidget> owner(first.get());
    QPointer<QTableWidget> child(first->findChild<QTableWidget*>(QStringLiteral("uni.entries")));
    dialogs.beforeAnswer = [&](QMessageBox*)
        {
            dialogs.beforeAnswer = {};
            first.reset();
        };
    check(replaceWatchBytes(firstPath,
            resourceWatchBytes(ResourceAuthoringKind::stringTable, ProjectType::JKDF2, "later owner deletion")),
        "a later revision opens an offer whose owning pane is synchronously destroyed");
    const bool ownerDeleted = waitForWatch([&]()
        {
            return dialogs.changes == 3 && !owner && !child && !QApplication::activeModalWidget();
        });
    observeQuietWatchEvents();
    check(ownerDeleted && dialogs.changes == 3 && second
            && canonicalWatchState(watchedResourceDraft(second.get())) == canonicalWatchState(secondBefore),
        "owner deletion closes the executing watch prompt and discards its stale callback without touching another document");
    first = openWatchedResource(ResourceAuthoringKind::stringTable, ProjectType::JKDF2, firstPath);
    const QCborMap rebound = watchedResourceDraft(first.get());
    const bool replaced = first && replaceWatchBytes(firstPath,
        resourceWatchBytes(ResourceAuthoringKind::stringTable, ProjectType::JKDF2, "new same-path incarnation"));
    check(replaced && waitForWatch([&]() { return dialogs.changes == 4; })
            && dialogs.lastTitle == QStringLiteral("File changed on disk")
            && dialogs.lastText == expectedWatchText(QStringLiteral("First.uni"))
            && canonicalWatchState(watchedResourceDraft(first.get())) == canonicalWatchState(rebound)
            && canonicalWatchState(watchedResourceDraft(second.get())) == canonicalWatchState(secondBefore),
        "re-registering a deleted document at the same path accepts only its new source incarnation during asynchronous fingerprint delivery");
}

void checkRecoveredNewerCogOffersAreForcedAndSerialized(const ProjectType project)
{
    QTemporaryDir temporary{};
    if ( !temporary.isValid() )
    {
        check(false, "the two-page recovered-newer COG fixture has an isolated source directory");
        return;
    }
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::array<QString, 2> leaves{QStringLiteral("FirstRecovered.cog"), QStringLiteral("SecondRecovered.cog")};
    const QString local = QStringLiteral("# recovered unclosed \"\n");
    const QString future = QStringLiteral("# recovered future \U0001f642\n");
    const QByteArray disk = kCogWatchSource + "# newer external source\n";
    std::array<QByteArray, 2> expected{};
    QByteArray payload{};
    bool validFixture{true};
    {
        MainWindow source(nullptr, true);
        bindWatchCogProject(source, project, root);
        for ( std::size_t index = 0; index < leaves.size(); ++index )
        {
            const std::filesystem::path path = root / "cog" / leaves[index].toStdWString();
            validFixture = validFixture && replaceWatchBytes(path, kCogWatchSource);
            source.openCogScriptEditorForTest(leaves[index].toStdString());
            auto* const page = watchedCogPage(source, leaves[index]);
            auto* const editor = page ? page->findChild<QPlainTextEdit*>() : nullptr;
            if ( !editor )
            {
                validFixture = false;
                continue;
            }
            appendWatchCogCommand(editor, local);
            appendWatchCogCommand(editor, future);
            editor->undo();
            QTextCursor selection(editor->document());
            selection.setPosition(3);
            selection.setPosition(8, QTextCursor::KeepAnchor);
            editor->setTextCursor(selection);
            const QCborMap state = watchedCogDraft(source, leaves[index]);
            const QCborMap history = state.value(QStringLiteral("history")).toMap();
            validFixture = validFixture && editor->toPlainText() == QString::fromUtf8(kCogWatchSource) + local
                && editor->document()->isModified() && editor->document()->isRedoAvailable()
                && history.value(QStringLiteral("cursor")).toInteger(-1) == 1
                && history.value(QStringLiteral("clean")).toInteger(-1) == 0
                && history.value(QStringLiteral("commands")).toArray().size() == 2;
            expected[index] = canonicalWatchState(state);
        }
        payload = source.captureCogEditorRecoveryStateForTest();
    }
    check(validFixture && !payload.isEmpty(),
        "both recovered-newer COG pages start with exact independent dirty text selections clean cursors and retained Redo commands");
    if ( !validFixture || payload.isEmpty() )
    {
        return;
    }
    qint64 checkpoint = std::numeric_limits<qint64>::max();
    for ( const QString& leaf : leaves )
    {
        const std::filesystem::path path = root / "cog" / leaf.toStdWString();
        validFixture = validFixture && replaceWatchBytes(path, disk);
        checkpoint = std::min(checkpoint,
            QFileInfo(QString::fromStdWString(path.wstring())).lastModified().toMSecsSinceEpoch() - 60'000);
    }
    MainWindow restored(nullptr, true);
    bindWatchCogProject(restored, project, root);
    restored.setNativeDocumentTimestampForTest(std::max<qint64>(1, checkpoint));
    WatchDialogs dialogs{};
    const bool loaded = validFixture && restored.restoreCogEditorRecoveryStateForTest(payload);
    const bool prompted = waitForWatch([&]() { return dialogs.changes == 2; });
    observeQuietWatchEvents();
    QStringList actual = dialogs.changeTexts;
    actual.sort();
    QStringList messages{expectedWatchText(leaves[0]), expectedWatchText(leaves[1])};
    messages.sort();
    bool exact = loaded && prompted && dialogs.changes == 2 && dialogs.errors == 0
        && dialogs.exactButtons && dialogs.lastTitle == QStringLiteral("File changed on disk")
        && dialogs.lastObject == QString::fromLatin1(kChangePrompt) && actual == messages
        && dialogs.maximumVisible == 1;
    for ( std::size_t index = 0; index < leaves.size(); ++index )
    {
        QWidget* const page = watchedCogPage(restored, leaves[index]);
        QPlainTextEdit* const editor = page ? page->findChild<QPlainTextEdit*>() : nullptr;
        exact = exact && editor && editor->toPlainText() == QString::fromUtf8(kCogWatchSource) + local
            && editor->document()->isModified() && editor->document()->isRedoAvailable()
            && canonicalWatchState(watchedCogDraft(restored, leaves[index])) == expected[index]
            && readWatchBytes(root / "cog" / leaves[index].toStdWString()) == disk;
    }
    if ( !exact )
    {
        std::cerr << "Recovered-newer watch diagnostic: project=" << static_cast<int>(project)
            << " loaded=" << loaded << " prompts=" << dialogs.changes << " errors=" << dialogs.errors
            << " maximumVisible=" << dialogs.maximumVisible << " object=" << dialogs.lastObject.toStdString()
            << " exactMessages=" << (actual == messages) << '\n';
    }
    check(exact,
        "two dirty recovered COG pages newer on disk force serial exact named Reload or Keep offers before initial fingerprints can swallow them and Keep preserves full raw histories and sources");
}

void checkQueuedExternalOfferIsRetiredByOwnSave()
{
    QTemporaryDir temporary{};
    if ( !temporary.isValid() )
    {
        check(false, "the queued external revision and own-Save fixture has an isolated source directory");
        return;
    }
    const std::filesystem::path path = std::filesystem::path(temporary.path().toStdWString()) / "Saved.uni";
    auto pane = openWatchedResource(ResourceAuthoringKind::stringTable, ProjectType::JKDF2, path);
    if ( !pane )
    {
        return;
    }
    editWatchedResource(pane.get(), ResourceAuthoringKind::stringTable, QStringLiteral("saved local value"));
    WatchDialogs dialogs{};
    dialogs.overwriteConflict = true;
    pane->grabMouse();
    const bool held = QWidget::mouseGrabber() == pane.get();
    check(replaceWatchBytes(path,
        resourceWatchBytes(ResourceAuthoringKind::stringTable, ProjectType::JKDF2, "queued external value")),
        "an external replacement reaches the real watcher while its named offer is deferred by a mouse grab");
    observeQuietWatchEvents();
    const bool saved = ui::resources::resourceAuthoringPaneSave(pane.get(), false);
    const QCborMap accepted = watchedResourceDraft(pane.get());
    pane->releaseMouse();
    observeQuietWatchEvents();
    const QByteArray expected("MSGS 1\n\n\"WatchKey\" -2147483648 \"saved local value\"\n\nEND\n");
    check(held && saved && dialogs.overwrites == 1 && dialogs.changes == 0 && dialogs.errors == 0
            && readWatchBytes(path) == expected && !accepted.value(QStringLiteral("modified")).toBool()
            && canonicalWatchState(watchedResourceDraft(pane.get())) == canonicalWatchState(accepted)
            && accepted.value(QStringLiteral("diskHash")).toByteArray()
                == QCryptographicHash::hash(expected, QCryptographicHash::Sha256)
            && ui::resources::resourceAuthoringPaneCanUndoRedo(pane.get(), false),
        "an accepted own Save retires an already queued external offer before the mouse grab ends without resetting document history");
}

} // namespace

void runLooseDocumentWatcherTests()
{
    for ( const ProjectType project : {ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM} )
    {
        for ( const ResourceAuthoringKind kind : {ResourceAuthoringKind::sprite, ResourceAuthoringKind::soundClass,
                 ResourceAuthoringKind::stringTable} )
        {
            checkResourceExternalChange(kind, project);
        }
        checkCogExternalChange(project);
        checkRecoveredNewerCogOffersAreForcedAndSerialized(project);
    }
    checkSharedWatchQueueDefersMouseGrabAndOwnerDeletion();
    checkQueuedExternalOfferIsRetiredByOwnSave();
}

} // namespace sed::app_tests
