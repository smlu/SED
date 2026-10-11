#include "test_common.hpp"

#include "app_settings.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/snd.hpp"
#include "sed/assets/spr.hpp"
#include "sed/assets/string_table.hpp"
#include "ui/cog/cog_script_format_utils.hpp"
#include "ui/components/loose_document_watcher.hpp"
#include "ui/resources/resource_authoring_editor.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDialog>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPointer>
#include <QSettings>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QTreeWidget>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

using ui::resources::ResourceAuthoringKind;

std::filesystem::path temporaryPath(const QTemporaryDir& directory)
{
    return std::filesystem::path(directory.path().toStdWString());
}

bool writeBytes(const std::filesystem::path& path, const std::string_view bytes)
{
    std::error_code error{};
    std::filesystem::create_directories(path.parent_path(), error);
    if ( error )
    {
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

void writeU32LE(std::ostream& output, const std::uint32_t value)
{
    for ( unsigned int shift = 0; shift < 32; shift += 8 )
    {
        output.put(static_cast<char>((value >> shift) & 0xffu));
    }
}

bool writeGobV2(
    const std::filesystem::path& path,
    const std::vector<std::pair<std::string, std::string_view>>& entries)
{
    if ( entries.size() > (std::numeric_limits<std::uint32_t>::max() - 16u) / 136u )
    {
        return false;
    }

    std::uint64_t nextDataOffset = 16u + entries.size() * 136u;
    for ( const auto& [name, bytes] : entries )
    {
        if ( name.size() > 128u
            || bytes.size() > std::numeric_limits<std::uint32_t>::max()
            || nextDataOffset + bytes.size() > std::numeric_limits<std::uint32_t>::max() )
        {
            return false;
        }
        nextDataOffset += bytes.size();
    }

    std::error_code error{};
    std::filesystem::create_directories(path.parent_path(), error);
    if ( error )
    {
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if ( !output )
    {
        return false;
    }

    output.write("GOB ", 4);
    writeU32LE(output, 0x14u);
    writeU32LE(output, 0x0cu);
    writeU32LE(output, static_cast<std::uint32_t>(entries.size()));

    std::uint32_t dataOffset = static_cast<std::uint32_t>(16u + entries.size() * 136u);
    for ( const auto& [name, bytes] : entries )
    {
        writeU32LE(output, dataOffset);
        writeU32LE(output, static_cast<std::uint32_t>(bytes.size()));
        std::array<char, 128> nameBytes{};
        std::copy(name.begin(), name.end(), nameBytes.begin());
        output.write(nameBytes.data(), static_cast<std::streamsize>(nameBytes.size()));
        dataOffset += static_cast<std::uint32_t>(bytes.size());
    }
    for ( const auto& [name, bytes] : entries )
    {
        (void)name;
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    return static_cast<bool>(output);
}

std::string makeCommentCogSource(const std::size_t byteCount)
{
    constexpr std::size_t kChunkBytes = 4u * 1024u;
    std::string source{};
    source.reserve(byteCount);
    while ( source.size() < byteCount )
    {
        const std::size_t count = std::min(kChunkBytes, byteCount - source.size());
        std::string line(count, ' ');
        line.front() = '#';
        line.back() = '\n';
        source += line;
    }
    return source;
}

std::optional<std::string> readBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if ( !input )
    {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(input), {});
}

class LooseResourceDialogDriver final
{
public:
    explicit LooseResourceDialogDriver(std::filesystem::path selectedPath)
        : selectedPath_(QString::fromStdWString(selectedPath.wstring()))
    {
        poll_.setInterval(5);
        QObject::connect(&poll_, &QTimer::timeout, [this]()
            {
                driveActiveDialog();
            });
        deadline_.setSingleShot(true);
        QObject::connect(&deadline_, &QTimer::timeout, [this]()
            {
                timedOut_ = true;
                if ( QDialog* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()) )
                {
                    dialog->reject();
                }
            });
    }

    void start()
    {
        deadline_.start(5000);
        poll_.start();
    }

    void stop()
    {
        poll_.stop();
        deadline_.stop();
    }

    bool fileSelected() const noexcept
    {
        return fileSelected_;
    }

    bool timedOut() const noexcept
    {
        return timedOut_;
    }

    const QString& lastModalDescription() const noexcept
    {
        return lastModalDescription_;
    }

    const QString& lastErrorTitle() const noexcept
    {
        return lastErrorTitle_;
    }

    const QString& lastErrorText() const noexcept
    {
        return lastErrorText_;
    }

private:
    void recordModal(QDialog* const dialog)
    {
        if ( !dialog )
        {
            return;
        }
        lastModalDescription_ = QStringLiteral("%1 object=%2 title=%3")
            .arg(QString::fromLatin1(dialog->metaObject()->className()),
                dialog->objectName(),
                dialog->windowTitle());
    }

    void driveActiveDialog()
    {
        QDialog* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if ( !dialog )
        {
            return;
        }
        recordModal(dialog);

        if ( auto* const chooser = qobject_cast<QFileDialog*>(dialog) )
        {
            if ( dialog->objectName() != QStringLiteral("resource_authoring.file_dialog") )
            {
                return;
            }
            chooser->selectFile(selectedPath_);
            fileSelected_ = QMetaObject::invokeMethod(
                chooser, "accept", Qt::DirectConnection);
            return;
        }

        if ( auto* const message = qobject_cast<QMessageBox*>(dialog) )
        {
            if ( message->objectName() == QStringLiteral("resource_authoring.error") )
            {
                lastErrorTitle_ = message->windowTitle();
                lastErrorText_ = message->text();
                QAbstractButton* const ok = message->button(QMessageBox::Ok);
                if ( ok )
                {
                    ok->click();
                }
                else
                {
                    message->reject();
                }
            }
            return;
        }

    }

    QString selectedPath_{};
    QTimer poll_{};
    QTimer deadline_{};
    QString lastModalDescription_{};
    QString lastErrorTitle_{};
    QString lastErrorText_{};
    bool fileSelected_{};
    bool timedOut_{};
};

std::filesystem::path resourceRelativePath(
    const ProjectType project,
    const ResourceAuthoringKind kind)
{
    switch ( kind )
    {
        case ResourceAuthoringKind::sprite:
            return std::filesystem::path("misc") / "spr" / "audit.spr";
        case ResourceAuthoringKind::soundClass:
            return std::filesystem::path("misc") / "snd" / "audit.snd";
        case ResourceAuthoringKind::stringTable:
            return std::filesystem::path("misc")
                / (project == ProjectType::IJIM ? "jonesstrings.uni" : "cogstrings.uni");
    }
    return {};
}

struct ResourceVectors
{
    std::string source{};
    std::string expected{};
};

ResourceVectors resourceVectors(
    const ProjectType project,
    const ResourceAuthoringKind kind)
{
    if ( kind == ResourceAuthoringKind::sprite )
    {
        const auto profile = project == ProjectType::IJIM
            ? assets::SprProfile::ijim
            : assets::SprProfile::classic;
        assets::SprFile source{};
        source.material = "before.mat";
        source.type = 0;
        source.width = 4.0f;
        source.height = 3.0f;
        source.geometryMode = 3;
        source.lightMode = 2;
        source.textureMode = 1;
        source.extraLightRGBA = project == ProjectType::IJIM
            ? std::array<float, 4>{ 0.125f, 0.25f, 0.5f, 1.0f }
            : std::array<float, 4>{ 0.125f, 0.125f, 0.125f, 1.0f };
        source.offsetXYZ = { 1.0f, -2.0f, 3.0f };
        assets::SprFile expected = source;
        expected.material = "after.mat";
        return {
            assets::saveToSPRText(source, profile),
            assets::saveToSPRText(expected, profile)
        };
    }

    if ( kind == ResourceAuthoringKind::soundClass )
    {
        const auto profile = project == ProjectType::IJIM
            ? assets::SoundClassProfile::ijim
            : assets::SoundClassProfile::classic;
        assets::SoundClassFile source{};
        source.entries.push_back({ "moving", "before.wav", 0x40U, 0.5f, 2.5f, 1.0f });
        assets::SoundClassFile expected = source;
        expected.entries.front().wav = "after.wav";
        return {
            assets::saveToSoundClassText(source, profile),
            assets::saveToSoundClassText(expected, profile)
        };
    }

    const auto profile = project == ProjectType::IJIM
        ? assets::UniProfile::ijim
        : assets::UniProfile::classic;
    assets::StringTableFile source{};
    source.capacity = 1;
    source.entries.push_back({ "HELLO", 17, "before" });
    assets::StringTableFile expected = source;
    expected.entries.front().value = "after";
    return {
        assets::saveToStringTableUniText(source, profile),
        assets::saveToStringTableUniText(expected, profile)
    };
}

bool editResourceDraft(QWidget* const pane, const ResourceAuthoringKind kind)
{
    if ( kind == ResourceAuthoringKind::sprite )
    {
        auto* const material = pane->findChild<QLineEdit*>(QStringLiteral("spr.material"));
        if ( !material )
        {
            return false;
        }
        material->setText(QStringLiteral("after.mat"));
        return material->text() == QStringLiteral("after.mat");
    }

    if ( kind == ResourceAuthoringKind::soundClass )
    {
        QTreeWidget* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
        if ( !tree )
        {
            return false;
        }
        for ( int groupIndex = 0; groupIndex < tree->topLevelItemCount(); ++groupIndex )
        {
            QTreeWidgetItem* const group = tree->topLevelItem(groupIndex);
            for ( int row = 0; group && row < group->childCount(); ++row )
            {
                QTreeWidgetItem* const entry = group->child(row);
                if ( entry && entry->text(1) == QStringLiteral("before.wav") )
                {
                    entry->setText(1, QStringLiteral("after.wav"));
                    return entry->text(1) == QStringLiteral("after.wav");
                }
            }
        }
        return false;
    }

    QTableWidget* const table = pane->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    if ( !table || table->rowCount() != 1 || !table->item(0, 1) )
    {
        return false;
    }
    table->item(0, 1)->setText(QStringLiteral("after"));
    return table->item(0, 1)->text() == QStringLiteral("after");
}

QString normalizedQtPath(const QString& path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QString normalizedQtPath(const std::filesystem::path& path)
{
    std::error_code error{};
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    return normalizedQtPath(QString::fromStdWString(
        (error ? path : absolute.lexically_normal()).wstring()));
}

void testExistingLooseResourceSaveAcrossProjectBoundaries()
{
    constexpr std::array projects{
        ProjectType::JKDF2,
        ProjectType::MOTS,
        ProjectType::IJIM,
    };
    constexpr std::array kinds{
        ResourceAuthoringKind::sprite,
        ResourceAuthoringKind::soundClass,
        ResourceAuthoringKind::stringTable,
    };

    for ( const ProjectType project : projects )
    {
        for ( const ResourceAuthoringKind kind : kinds )
        {
            QTemporaryDir temporary{};
            const std::filesystem::path root = temporaryPath(temporary);
            const std::filesystem::path projectDirectory = root / "Project";
            const std::filesystem::path resourceRoot = root / "Resource";
            const std::filesystem::path relative = resourceRelativePath(project, kind);
            const std::filesystem::path sourcePath = resourceRoot / relative;
            const std::filesystem::path projectOverride = projectDirectory / relative;
            const QString label = QStringLiteral("existing loose resource Save (%1/%2)")
                .arg(static_cast<int>(project))
                .arg(static_cast<int>(kind));
            std::optional<ResourceVectors> vectors{};
            try
            {
                vectors = resourceVectors(project, kind);
            }
            catch ( const std::exception& exception )
            {
                const QString failure = label + QStringLiteral(": fixture serialization failed: ")
                    + QString::fromUtf8(exception.what());
                check(false, failure.toUtf8().constData());
                continue;
            }
            std::error_code projectError{};
            if ( temporary.isValid() )
            {
                std::filesystem::create_directories(projectDirectory, projectError);
            }
            const bool fixtureReady = temporary.isValid() && !projectError
                && writeBytes(sourcePath, vectors->source);
            check(fixtureReady, (label + QStringLiteral(": sibling project and resource fixture are ready"))
                .toUtf8().constData());
            if ( !fixtureReady )
            {
                continue;
            }

            auto locator = std::make_shared<assets::ResourceLocator>();
            locator->addRoot(resourceRoot);
            ui::resources::ResourceAuthoringContext context{};
            context.resources = locator;
            context.projectType = project;
            context.projectDirectory = projectDirectory;
            context.startingDirectory = resourceRoot;
            context.transientProjectOpenTarget = true;
            std::unique_ptr<QWidget> pane(ui::resources::createResourceAuthoringPane(
                nullptr, kind, std::move(context)));
            LooseResourceDialogDriver modal(sourcePath);
            bool opened = false;
            if ( pane )
            {
                modal.start();
                opened = ui::resources::resourceAuthoringPaneOpenLooseFileDirect(pane.get());
                modal.stop();
            }
            const bool openedThroughChooser = pane && opened && modal.fileSelected() && !modal.timedOut();
            if ( !openedThroughChooser )
            {
                std::cerr << "Loose resource Open diagnostic: project="
                    << static_cast<int>(project)
                    << " kind=" << static_cast<int>(kind)
                    << " pane=" << (pane != nullptr)
                    << " opened=" << opened
                    << " chooserAccepted=" << modal.fileSelected()
                    << " timedOut=" << modal.timedOut()
                    << " modal=[" << modal.lastModalDescription().toStdString() << "]"
                    << " errorTitle=[" << modal.lastErrorTitle().toStdString() << "]"
                    << " errorText=[" << modal.lastErrorText().toStdString() << "]\n";
            }
            check(openedThroughChooser,
                "configured resource root opens the exact external loose file through the real file chooser");
            if ( !pane || !opened )
            {
                continue;
            }

            const bool draftEdited = editResourceDraft(pane.get(), kind)
                && ui::resources::resourceAuthoringPaneIsModified(pane.get());
            check(draftEdited, "opened external resource has a genuine authored edit before ordinary Save");
            if ( !draftEdited )
            {
                continue;
            }

            modal.start();
            const bool saved = ui::resources::resourceAuthoringPaneSave(pane.get(), false);
            modal.stop();
            const std::optional<std::string> actualBytes = readBytes(sourcePath);
            const QString retainedSource = ui::resources::resourceAuthoringPaneLooseFilePath(pane.get());
            const QString normalizedRetainedSource = normalizedQtPath(retainedSource);
            const QString expectedSource = normalizedQtPath(sourcePath);
            const bool saveExpectationMet = saved
                && actualBytes == std::optional<std::string>(vectors->expected)
                    && !ui::resources::resourceAuthoringPaneIsModified(pane.get())
                    && normalizedRetainedSource == expectedSource
                    && !std::filesystem::exists(projectOverride);
            if ( !saveExpectationMet )
            {
                std::cerr << "Loose resource Save diagnostic: project="
                    << static_cast<int>(project)
                    << " kind=" << static_cast<int>(kind)
                    << " saved=" << saved
                    << " expectedBytes=" << vectors->expected.size()
                    << " actualBytes=" << (actualBytes ? actualBytes->size() : 0)
                    << " dirty=" << ui::resources::resourceAuthoringPaneIsModified(pane.get())
                    << " retainedSource=[" << retainedSource.toStdString() << "]"
                    << " normalizedRetainedSource=[" << normalizedRetainedSource.toStdString() << "]"
                    << " expectedSource=[" << expectedSource.toStdString() << "]"
                    << " errorTitle=[" << modal.lastErrorTitle().toStdString() << "]"
                    << " errorText=[" << modal.lastErrorText().toStdString() << "]"
                    << " projectOverride=" << std::filesystem::exists(projectOverride) << '\n';
            }
            check(saveExpectationMet,
                "ordinary Save writes exact expected bytes in place, advances the clean checkpoint, and creates no project override");
        }
    }
}

constexpr qint64 kMaximumCogSourceBytes = 16 * 1024 * 1024;
constexpr qint64 kOversizedCogSourceBytes = 17 * 1024 * 1024;
constexpr auto kCogExternalSaveConflictPrompt = "sed.cog_external_save_conflict_prompt";

bool writeLargeCogComment(const std::filesystem::path& path)
{
    std::error_code error{};
    std::filesystem::create_directories(path.parent_path(), error);
    if ( error )
    {
        return false;
    }

    QFile output(QString::fromStdWString(path.wstring()));
    if ( !output.open(QIODevice::WriteOnly | QIODevice::Truncate) )
    {
        return false;
    }

    QByteArray chunk(64 * 1024, ' ');
    chunk[0] = '#';
    chunk[chunk.size() - 1] = '\n';
    qint64 written = 0;
    while ( written < kOversizedCogSourceBytes )
    {
        const qint64 count = std::min<qint64>(chunk.size(), kOversizedCogSourceBytes - written);
        if ( output.write(chunk.constData(), count) != count )
        {
            return false;
        }
        written += count;
        chunk[0] = ' ';
    }
    return output.flush() && output.size() == kOversizedCogSourceBytes;
}

bool writeSmallCogSource(const std::filesystem::path& path, const QString& text)
{
    const QByteArray bytes = text.toUtf8();
    return writeBytes(path, std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
}

class ScopedCogFormattingPreferences final
{
public:
    explicit ScopedCogFormattingPreferences(const bool formatOnSave)
        : settings_(settingsStore())
    {
        static const std::array<QString, 13> keys{
            QStringLiteral("cog_editor/format_on_save"),
            QStringLiteral("cog_editor/format_use_tabs"),
            QStringLiteral("cog_editor/format_indent_width"),
            QStringLiteral("cog_editor/format_brace_style"),
            QStringLiteral("cog_editor/format_indent_sections"),
            QStringLiteral("cog_editor/format_indent_code_bodies"),
            QStringLiteral("cog_editor/format_keyword_spacing"),
            QStringLiteral("cog_editor/format_inside_parentheses_spacing"),
            QStringLiteral("cog_editor/format_function_parentheses_spacing"),
            QStringLiteral("cog_editor/format_for_semicolon_spacing"),
            QStringLiteral("cog_editor/format_align_final_return_label"),
            QStringLiteral("cog_editor/format_trim_excess_blank_lines"),
            QStringLiteral("cog_editor/format_indent_comments"),
        };
        for ( const QString& key : keys )
        {
            originals_.push_back({key, settings_.contains(key), settings_.value(key)});
        }
        settings_.setValue(QStringLiteral("cog_editor/format_on_save"), formatOnSave);
        settings_.setValue(QStringLiteral("cog_editor/format_use_tabs"), false);
        settings_.setValue(QStringLiteral("cog_editor/format_indent_width"), 4);
        settings_.setValue(QStringLiteral("cog_editor/format_brace_style"), QStringLiteral("next_line"));
        settings_.setValue(QStringLiteral("cog_editor/format_indent_sections"), true);
        settings_.setValue(QStringLiteral("cog_editor/format_indent_code_bodies"), true);
        settings_.setValue(QStringLiteral("cog_editor/format_keyword_spacing"), true);
        settings_.setValue(QStringLiteral("cog_editor/format_inside_parentheses_spacing"), false);
        settings_.setValue(QStringLiteral("cog_editor/format_function_parentheses_spacing"), false);
        settings_.setValue(QStringLiteral("cog_editor/format_for_semicolon_spacing"), true);
        settings_.setValue(QStringLiteral("cog_editor/format_align_final_return_label"), false);
        settings_.setValue(QStringLiteral("cog_editor/format_trim_excess_blank_lines"), true);
        settings_.setValue(QStringLiteral("cog_editor/format_indent_comments"), false);
        settings_.sync();
    }

    ScopedCogFormattingPreferences(const ScopedCogFormattingPreferences&) = delete;
    ScopedCogFormattingPreferences& operator=(const ScopedCogFormattingPreferences&) = delete;

    ~ScopedCogFormattingPreferences()
    {
        for ( const OriginalSetting& original : originals_ )
        {
            if ( original.existed )
            {
                settings_.setValue(original.key, original.value);
            }
            else
            {
                settings_.remove(original.key);
            }
        }
        settings_.sync();
    }

private:
    struct OriginalSetting
    {
        QString key{};
        bool existed{};
        QVariant value{};
    };

    QSettings settings_;
    std::vector<OriginalSetting> originals_{};
};

struct CogBufferSnapshot
{
    QString text{};
    QCborMap history{};
    bool modified{};
    bool undoAvailable{};
    bool redoAvailable{};
    int cursorPosition{};
    int cursorAnchor{};
    QString selection{};
};

QCborMap cogDocumentHistoryState(const MainWindow& window)
{
    const QCborArray tabs = QCborValue::fromCbor(
        window.captureCogEditorRecoveryStateForTest()).toMap().value(QStringLiteral("tabs")).toArray();
    if ( tabs.size() != 1 )
    {
        return {};
    }
    return tabs.at(0).toMap().value(QStringLiteral("history")).toMap();
}

bool validCogDocumentHistory(const QCborMap& history)
{
    return history.value(QStringLiteral("version")).isInteger()
        && history.value(QStringLiteral("version")).toInteger() == 1
        && history.value(QStringLiteral("cursor")).isInteger()
        && history.value(QStringLiteral("clean")).isInteger()
        && history.value(QStringLiteral("baselineDirty")).isBool()
        && history.value(QStringLiteral("commands")).isArray();
}

CogBufferSnapshot snapshotCogBuffer(MainWindow& window, QPlainTextEdit* const editor)
{
    // CogDocumentHistory records edits synchronously, but boundary trimming
    // and editor-derived refreshes can be queued after a document change.
    // Settle those before taking the editor and serialized history snapshot.
    QApplication::processEvents();
    const QTextCursor cursor = editor->textCursor();
    return {
        editor->toPlainText(),
        cogDocumentHistoryState(window),
        editor->document()->isModified(),
        editor->document()->isUndoAvailable(),
        editor->document()->isRedoAvailable(),
        cursor.position(),
        cursor.anchor(),
        cursor.selectedText(),
    };
}

bool sameCogBuffer(
    MainWindow& window,
    QPlainTextEdit* const editor,
    const CogBufferSnapshot& expected)
{
    const CogBufferSnapshot actual = snapshotCogBuffer(window, editor);
    return actual.text == expected.text
        && actual.history == expected.history
        && actual.modified == expected.modified
        && actual.undoAvailable == expected.undoAvailable
        && actual.redoAvailable == expected.redoAvailable
        && actual.cursorPosition == expected.cursorPosition
        && actual.cursorAnchor == expected.cursorAnchor
        && actual.selection == expected.selection;
}

void selectLocalWord(QPlainTextEdit* const editor)
{
    const int start = editor->toPlainText().indexOf(QStringLiteral("local"));
    QTextCursor cursor(editor->document());
    cursor.setPosition(std::max(0, start));
    cursor.setPosition(std::max(0, start) + 5, QTextCursor::KeepAnchor);
    editor->setTextCursor(cursor);
}

void appendLocalEdit(QPlainTextEdit* const editor)
{
    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("\n# local"));
    editor->setTextCursor(cursor);
    selectLocalWord(editor);
}

class CogModalDriver final
{
public:
    CogModalDriver()
    {
        poll_.setInterval(5);
        QObject::connect(&poll_, &QTimer::timeout, [this]()
            {
                driveActiveDialog();
            });
        deadline_.setSingleShot(true);
        QObject::connect(&deadline_, &QTimer::timeout, [this]()
            {
                timedOut_ = true;
                if ( QDialog* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget()) )
                {
                    dialog->reject();
                }
            });
    }

    void start()
    {
        timedOut_ = false;
        deadline_.start(5000);
        poll_.start();
    }

    void stop()
    {
        poll_.stop();
        deadline_.stop();
    }

    void setBeforeDiscard(std::function<void()> callback)
    {
        beforeDiscard_ = std::move(callback);
    }

    void setSaveConflictChoice(QString buttonText)
    {
        saveConflictChoice_ = std::move(buttonText);
    }

    void setBeforeSaveConflictChoice(std::function<void()> callback)
    {
        beforeSaveConflictChoice_ = std::move(callback);
    }

    void clearObservations()
    {
        sawReloadQuestion_ = false;
        sawLimitError_ = false;
        sawOpenFailure_ = false;
        sawSaveConflict_ = false;
        saveConflictCount_ = 0;
        looseWatchPromptCount_ = 0;
        saveConflictButtonsValid_ = false;
        saveConflictCancelPolicyValid_ = false;
        lastSaveConflict_ = nullptr;
        lastModalDescription_.clear();
        lastErrorText_.clear();
        timedOut_ = false;
    }

    bool sawReloadQuestion() const noexcept { return sawReloadQuestion_; }
    bool sawLimitError() const noexcept { return sawLimitError_; }
    bool sawOpenFailure() const noexcept { return sawOpenFailure_; }
    bool sawSaveConflict() const noexcept { return sawSaveConflict_; }
    int saveConflictCount() const noexcept { return saveConflictCount_; }
    int looseWatchPromptCount() const noexcept { return looseWatchPromptCount_; }
    bool saveConflictButtonsValid() const noexcept { return saveConflictButtonsValid_; }
    bool saveConflictCancelPolicyValid() const noexcept { return saveConflictCancelPolicyValid_; }
    bool timedOut() const noexcept { return timedOut_; }
    const QString& lastModalDescription() const noexcept { return lastModalDescription_; }
    const QString& lastErrorText() const noexcept { return lastErrorText_; }

private:
    void recordModal(QMessageBox* const message)
    {
        QStringList buttons{};
        for ( const QAbstractButton* const button : message->buttons() )
        {
            if ( button )
            {
                buttons.push_back(button->text());
            }
        }
        lastModalDescription_ = QStringLiteral("%1 object=%2 title=%3 text=[%4] buttons=[%5]")
            .arg(QString::fromLatin1(message->metaObject()->className()),
                message->objectName(),
                message->windowTitle(),
                message->text(),
                buttons.join(QStringLiteral(", ")));
    }

    static void clickButton(QMessageBox* const message, const QString& text)
    {
        for ( QAbstractButton* const button : message->buttons() )
        {
            if ( button && button->text().compare(text, Qt::CaseInsensitive) == 0 )
            {
                button->click();
                return;
            }
        }
        message->reject();
    }

    static void clickButton(
        QMessageBox* const message,
        const QMessageBox::StandardButton standardButton)
    {
        if ( QAbstractButton* const button = message->button(standardButton) )
        {
            button->click();
        }
        else
        {
            message->reject();
        }
    }

    void driveActiveDialog()
    {
        auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if ( !message )
        {
            return;
        }
        recordModal(message);

        if ( message->objectName() == QStringLiteral("sed.external_document_change_prompt") )
        {
            ++looseWatchPromptCount_;
            clickButton(message, QStringLiteral("Keep"));
            return;
        }

        if ( message->objectName() == QString::fromLatin1(kCogExternalSaveConflictPrompt) )
        {
            if ( lastSaveConflict_ != message )
            {
                sawSaveConflict_ = true;
                ++saveConflictCount_;
                lastSaveConflict_ = message;
                QStringList buttons{};
                for ( const QAbstractButton* const button : message->buttons() )
                {
                    if ( button )
                    {
                        buttons.push_back(button->text());
                    }
                }
                saveConflictButtonsValid_ = buttons.contains(QStringLiteral("Reload from Disk"))
                    && buttons.contains(QStringLiteral("Overwrite Disk"))
                    && buttons.contains(QStringLiteral("Cancel"));
                QAbstractButton* cancelButton = nullptr;
                for ( QAbstractButton* const button : message->buttons() )
                {
                    if ( button && button->text() == QStringLiteral("Cancel") )
                    {
                        cancelButton = button;
                        break;
                    }
                }
                saveConflictCancelPolicyValid_ = cancelButton
                    && message->defaultButton() == cancelButton
                    && message->escapeButton() == cancelButton;
                if ( beforeSaveConflictChoice_ )
                {
                    auto beforeChoice = std::move(beforeSaveConflictChoice_);
                    beforeChoice();
                }
            }
            clickButton(message, saveConflictChoice_);
            return;
        }

        const QString text = message->text();
        if ( message->windowTitle() == QStringLiteral("Reload COG script")
            && text == QStringLiteral("Reload from disk and discard local edits?") )
        {
            sawReloadQuestion_ = true;
            if ( beforeDiscard_ )
            {
                auto beforeDiscard = std::move(beforeDiscard_);
                beforeDiscard();
            }
            clickButton(message, QMessageBox::Discard);
            return;
        }

        if ( text.contains(QStringLiteral("16 MiB"), Qt::CaseInsensitive) )
        {
            sawLimitError_ = true;
            lastErrorText_ = text;
        }
        else if ( message->windowTitle() == QStringLiteral("Edit script") )
        {
            sawOpenFailure_ = true;
            lastErrorText_ = text;
        }

        clickButton(message, QMessageBox::Ok);
    }

    QTimer poll_{};
    QTimer deadline_{};
    std::function<void()> beforeDiscard_{};
    std::function<void()> beforeSaveConflictChoice_{};
    QString saveConflictChoice_{QStringLiteral("Cancel")};
    QPointer<QMessageBox> lastSaveConflict_{};
    QString lastModalDescription_{};
    QString lastErrorText_{};
    bool sawReloadQuestion_{};
    bool sawLimitError_{};
    bool sawOpenFailure_{};
    bool sawSaveConflict_{};
    bool saveConflictButtonsValid_{};
    bool saveConflictCancelPolicyValid_{};
    int saveConflictCount_{};
    int looseWatchPromptCount_{};
    bool timedOut_{};
};

QPlainTextEdit* currentCogEditor(QTabWidget* const tabs)
{
    return tabs && tabs->currentWidget()
        ? tabs->currentWidget()->findChild<QPlainTextEdit*>()
        : nullptr;
}

void triggerCogReload(
    MainWindow& window,
    QPlainTextEdit* const editor,
    QAction* const reloadAction,
    CogModalDriver& modal)
{
    window.raise();
    window.activateWindow();
    editor->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    const bool editorOwnsFocus = QApplication::focusWidget() == editor;
    const bool reloadEnabled = reloadAction && reloadAction->isEnabled();
    const bool cogFocusActive = window.cogEditorFocusActiveForTest();
    check(editorOwnsFocus && reloadEnabled && cogFocusActive,
        "the real COG Reload action is enabled while its current source editor owns focus");
    if ( !editorOwnsFocus || !reloadEnabled || !cogFocusActive )
    {
        std::cerr << "COG Reload dispatch diagnostic: editorFocus=" << editorOwnsFocus
            << " actionEnabled=" << reloadEnabled
            << " cogFocusActive=" << cogFocusActive
            << " focusWidget=" << (QApplication::focusWidget()
                ? QApplication::focusWidget()->metaObject()->className() : "<none>") << '\n';
    }
    modal.start();
    if ( reloadAction )
    {
        reloadAction->trigger();
    }
    modal.stop();
    QCoreApplication::processEvents();
}

bool waitForCogTestState(const std::function<bool()>& predicate, const int timeoutMs = 2500)
{
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

void observeQuietCogTestWatchEvents(const int durationMs = 450)
{
    QEventLoop loop{};
    QTimer::singleShot(durationMs, &loop, &QEventLoop::quit);
    loop.exec();
}

QByteArray cogSourceHash(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

std::string cogByteString(const QByteArray& bytes)
{
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QCborMap cogRecoveryTab(const QByteArray& payload)
{
    const QCborArray tabs = QCborValue::fromCbor(payload).toMap()
        .value(QStringLiteral("tabs")).toArray();
    return tabs.size() == 1 ? tabs.at(0).toMap() : QCborMap{};
}

void testOversizedCogOpenDoesNotInstallATab()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root = temporaryPath(temporary);
    const std::filesystem::path path = root / "cog" / "too_large.cog";
    const bool sourceReady = temporary.isValid() && writeLargeCogComment(path);
    check(sourceReady, "COG oversized Open fixture writes a 17 MiB safe comment source");
    if ( !sourceReady )
    {
        return;
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "oversized.sed");
    CogModalDriver modal{};
    modal.start();
    window.openCogScriptEditorForTest("too_large.cog");
    modal.stop();

    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    if ( !tabs || tabs->count() != 0 || !modal.sawLimitError() )
    {
        std::cerr << "COG oversized Open diagnostic: tabCount="
            << (tabs ? tabs->count() : -1)
            << " sawLimitError=" << modal.sawLimitError()
            << " timedOut=" << modal.timedOut()
            << " modal=[" << modal.lastModalDescription().toStdString() << "]\n";
    }
    check(tabs && tabs->count() == 0,
        "MainWindow loose COG Open rejects an oversized source before installing an editor tab");
    check(modal.sawLimitError() && modal.lastErrorText().contains(QStringLiteral("16 MiB"), Qt::CaseInsensitive),
        "oversized COG Open reports the source byte limit explicitly");
}

void testCogArchiveOpenAdmissionPrecedesExtraction()
{
    constexpr std::size_t kCogByteBudget = 16u * 1024u * 1024u;
    QTemporaryDir temporary{};
    const std::filesystem::path root = temporaryPath(temporary);
    const std::filesystem::path olderArchive = root / "archives" / "older.gob";
    const std::filesystem::path newestArchive = root / "archives" / "newest.gob";
    const std::filesystem::path rejectedLooseRoot = root / "rejected-resources";
    const std::filesystem::path acceptedLooseRoot = root / "accepted-resources";
    const std::filesystem::path primaryLooseRoot = root / "primary-resources";
    const std::filesystem::path suffixFallbackLooseRoot = root / "suffix-fallback-resources";
    const std::filesystem::path readFailureLooseRoot = root / "read-failure-resources";
    const std::filesystem::path readFailureOlderArchive = root / "archives" / "read-old.gob";
    const std::filesystem::path readFailureWinningArchive = root / "archives" / "read-new.gob";
    const std::string shadowedSource = "symbols\nint shadowed\nend\n";
    const std::string oversizedSource = makeCommentCogSource(kCogByteBudget + 1u);
    const std::string exactLimitSource = makeCommentCogSource(kCogByteBudget);
    const std::string normalSource = "symbols\nint normal\nend\n";
    std::error_code rootError{};
    if ( temporary.isValid() )
    {
        std::filesystem::create_directories(rejectedLooseRoot, rootError);
        if ( !rootError )
        {
            std::filesystem::create_directories(acceptedLooseRoot, rootError);
        }
        if ( !rootError )
        {
            std::filesystem::create_directories(primaryLooseRoot, rootError);
        }
        if ( !rootError )
        {
            std::filesystem::create_directories(suffixFallbackLooseRoot, rootError);
        }
        if ( !rootError )
        {
            std::filesystem::create_directories(readFailureLooseRoot, rootError);
        }
    }
    const bool archivesReady = temporary.isValid()
        && !rootError
        && writeBytes(suffixFallbackLooseRoot / "base.cog", normalSource)
        && writeGobV2(olderArchive,
            {
                { "cog/precedence.cog", shadowedSource },
                { "base", shadowedSource },
            })
        && writeGobV2(newestArchive,
            {
                { "cog/precedence.cog", oversizedSource },
                { "base", oversizedSource },
                { "cog/exact.cog", exactLimitSource },
                { "cog/normal.cog", normalSource },
                { "cog/primary.cog", normalSource },
            })
        && writeGobV2(readFailureOlderArchive,
            { { "cog/readfailure.cog", shadowedSource } })
        && writeGobV2(readFailureWinningArchive,
            { { "cog/readfailure.cog", normalSource } });
    check(archivesReady,
        "COG archive admission fixture writes synthetic older and newest GOB entries");
    if ( !archivesReady )
    {
        return;
    }

    const std::vector<std::filesystem::path> archives{ olderArchive, newestArchive };
    const std::filesystem::path rejectedOutputDirectory = rejectedLooseRoot / "cog";
    MainWindow rejectedWindow{};
    rejectedWindow.setAttribute(Qt::WA_DontShowOnScreen);
    rejectedWindow.setResourcePathsForTest({ rejectedLooseRoot }, archives);
    CogModalDriver modal{};
    modal.start();
    rejectedWindow.openCogScriptEditorForTest("precedence");
    modal.stop();

    QTabWidget* const rejectedTabs = rejectedWindow.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    check(modal.sawLimitError()
            && modal.lastErrorText().contains(QStringLiteral("16 MiB"), Qt::CaseInsensitive),
        "an oversized winning GOB COG entry reports the 16 MiB limit");
    check(rejectedTabs && rejectedTabs->count() == 0,
        "an oversized winning GOB COG entry does not install an editor tab");
    check(!std::filesystem::exists(rejectedOutputDirectory)
            && !std::filesystem::exists(rejectedOutputDirectory / "precedence.cog"),
        "an oversized winning GOB COG entry is rejected before creating an extraction directory or file");

    const std::filesystem::path primaryOutputDirectory = primaryLooseRoot / "cog";
    MainWindow primaryWindow{};
    primaryWindow.setAttribute(Qt::WA_DontShowOnScreen);
    primaryWindow.setResourcePathsForTest({ primaryLooseRoot }, archives);
    primaryWindow.setPrimaryResourceOverrideForTest(
        "primary",
        [kCogByteBudget]() -> std::optional<std::vector<std::byte>>
        {
            return std::vector<std::byte>(kCogByteBudget + 1u);
        });
    CogModalDriver primaryModal{};
    primaryModal.start();
    primaryWindow.openCogScriptEditorForTest("primary");
    primaryModal.stop();

    QTabWidget* const primaryTabs = primaryWindow.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    check(primaryModal.sawLimitError()
            && primaryModal.lastErrorText().contains(QStringLiteral("16 MiB"), Qt::CaseInsensitive),
        "an oversized primary COG override reports the 16 MiB limit");
    check(primaryTabs && primaryTabs->count() == 0
            && !std::filesystem::exists(primaryOutputDirectory),
        "an oversized primary COG override does not fall back to a smaller .cog GOB entry or extract");

    const std::filesystem::path suffixFallbackOutputDirectory = suffixFallbackLooseRoot / "cog";
    MainWindow suffixFallbackWindow{};
    suffixFallbackWindow.setAttribute(Qt::WA_DontShowOnScreen);
    suffixFallbackWindow.setResourcePathsForTest({ suffixFallbackLooseRoot }, archives);
    CogModalDriver suffixFallbackModal{};
    suffixFallbackModal.start();
    suffixFallbackWindow.openCogScriptEditorForTest("base");
    suffixFallbackModal.stop();

    QTabWidget* const suffixFallbackTabs = suffixFallbackWindow.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    check(suffixFallbackModal.sawLimitError()
            && suffixFallbackModal.lastErrorText().contains(
                QStringLiteral("16 MiB"), Qt::CaseInsensitive),
        "an oversized exact-name GOB winner is rejected before a loose .cog suffix fallback");
    check(suffixFallbackTabs && suffixFallbackTabs->count() == 0
            && !std::filesystem::exists(suffixFallbackOutputDirectory)
            && readBytes(suffixFallbackLooseRoot / "base.cog")
                == std::optional<std::string>(normalSource),
        "a loose base.cog fallback stays untouched when the requested base resource is oversized");

    MainWindow readFailureWindow{};
    readFailureWindow.setAttribute(Qt::WA_DontShowOnScreen);
    readFailureWindow.setResourcePathsForTest(
        { readFailureLooseRoot },
        { readFailureOlderArchive, readFailureWinningArchive });
    const bool winningArchiveTruncated = writeBytes(readFailureWinningArchive, "");
    check(winningArchiveTruncated,
        "COG read-failure fixture truncates the already-mounted winning archive");
    if ( !winningArchiveTruncated )
    {
        return;
    }

    CogModalDriver readFailureModal{};
    bool openThrew = false;
    std::string escapedException{};
    readFailureModal.start();
    try
    {
        readFailureWindow.openCogScriptEditorForTest("readfailure.cog");
    }
    catch ( const std::exception& exception )
    {
        openThrew = true;
        escapedException = exception.what();
    }
    catch ( ... )
    {
        openThrew = true;
        escapedException = "non-standard exception";
    }
    readFailureModal.stop();

    QTabWidget* const readFailureTabs = readFailureWindow.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    if ( openThrew || !readFailureModal.sawOpenFailure() || readFailureModal.timedOut() )
    {
        std::cerr << "COG bounded archive read failure diagnostic: threw=" << openThrew
            << " exception=[" << escapedException << "] sawOpenFailure="
            << readFailureModal.sawOpenFailure()
            << " timedOut=" << readFailureModal.timedOut()
            << " modal=[" << readFailureModal.lastModalDescription().toStdString() << "]\n";
    }
    check(!openThrew,
        "a failed bounded archive read is reported without an exception escaping COG Open");
    check(readFailureModal.sawOpenFailure() && !readFailureModal.lastErrorText().isEmpty()
            && !readFailureModal.timedOut(),
        "a failed bounded archive read produces a driven COG Open diagnostic");
    check(readFailureTabs && readFailureTabs->count() == 0
            && !std::filesystem::exists(readFailureLooseRoot / "cog"),
        "a failed bounded archive read creates no COG tab or extraction directory");

    const std::filesystem::path acceptedOutputDirectory = acceptedLooseRoot / "cog";
    MainWindow acceptedWindow{};
    acceptedWindow.setAttribute(Qt::WA_DontShowOnScreen);
    acceptedWindow.setResourcePathsForTest({ acceptedLooseRoot }, archives);
    const auto openAcceptedCog = [&](const char* const name, const char* const description)
    {
        modal.clearObservations();
        modal.start();
        acceptedWindow.openCogScriptEditorForTest(name);
        modal.stop();

        const bool noUnexpectedModal = modal.lastModalDescription().isEmpty()
            && !modal.sawOpenFailure()
            && !modal.sawLimitError()
            && !modal.timedOut();
        if ( !noUnexpectedModal )
        {
            std::cerr << "COG accepted archive Open diagnostic (" << description << "): "
                << "sawOpenFailure=" << modal.sawOpenFailure()
                << " sawLimitError=" << modal.sawLimitError()
                << " timedOut=" << modal.timedOut()
                << " modal=[" << modal.lastModalDescription().toStdString() << "]"
                << " error=[" << modal.lastErrorText().toStdString() << "]\n";
        }
        check(noUnexpectedModal,
            "successful archive COG Open has no error modal or timeout");
    };
    openAcceptedCog("normal", "normal source via .cog suffix fallback");
    openAcceptedCog("exact.cog", "exact-limit source");

    QTabWidget* const acceptedTabs = acceptedWindow.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    check(acceptedTabs && acceptedTabs->count() == 2,
        "normal and exact-limit archive COG sources both open into editor tabs");
    check(readBytes(acceptedOutputDirectory / "normal.cog")
            == std::optional<std::string>(normalSource),
        "normal archive COG Open writes the complete source to its working copy");
    check(readBytes(acceptedOutputDirectory / "exact.cog")
            == std::optional<std::string>(exactLimitSource),
        "exact-limit archive COG Open writes all 16 MiB to its working copy");
}

void testCogArchiveWinnerPrecedesLooseAliases()
{
    constexpr std::size_t kCogByteBudget = 16u * 1024u * 1024u;
    QTemporaryDir temporary{};
    const std::filesystem::path root = temporaryPath(temporary);
    const std::filesystem::path looseRoot = root / "resources";
    const std::filesystem::path archive = root / "archives" / "aliases.gob";
    const std::filesystem::path explicitProject = root / "explicit-project";
    const std::filesystem::path suffixProject = root / "suffix-project";
    const std::filesystem::path controlProject = root / "control-project";
    const std::string oversizedSource = makeCommentCogSource(kCogByteBudget + 1u);
    const std::string shadowedSource = "symbols\nint shadowed_alias\nend\n";
    const std::string archiveControlSource = "symbols\nint archive_alias_control\nend\n";
    std::error_code error{};
    if ( temporary.isValid() )
    {
        std::filesystem::create_directories(looseRoot / "cog", error);
        if ( !error )
        {
            std::filesystem::create_directories(explicitProject, error);
        }
        if ( !error )
        {
            std::filesystem::create_directories(suffixProject, error);
        }
        if ( !error )
        {
            std::filesystem::create_directories(controlProject, error);
        }
    }

    const bool fixtureReady = temporary.isValid()
        && !error
        && writeBytes(looseRoot / "cog" / "explicit-alias.cog", shadowedSource)
        && writeBytes(looseRoot / "cog" / "suffix-alias.cog", shadowedSource)
        && writeBytes(looseRoot / "cog" / "control-alias.cog", shadowedSource)
        && writeGobV2(archive,
            {
                { "explicit-alias.cog", oversizedSource },
                { "suffix-alias.cog", oversizedSource },
                { "control-alias.cog", archiveControlSource },
            });
    check(fixtureReady,
        "COG winner-precedence fixture creates isolated project destinations, loose aliases, and archive entries");
    if ( !fixtureReady )
    {
        return;
    }

    const auto configureWindow = [&](MainWindow& window, const std::filesystem::path& project)
    {
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setResourcePathsForTest({ looseRoot }, { archive });
        window.setCurrentPathForTitleForTest(project / "fixture.sed");
    };

    MainWindow explicitWindow{};
    configureWindow(explicitWindow, explicitProject);
    CogModalDriver explicitModal{};
    explicitModal.start();
    explicitWindow.openCogScriptEditorForTest("explicit-alias.cog");
    explicitModal.stop();
    QTabWidget* const explicitTabs = explicitWindow.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    check(explicitModal.sawLimitError()
            && explicitModal.lastErrorText().contains(QStringLiteral("16 MiB"), Qt::CaseInsensitive),
        "an oversized exact-name GOB COG winner is refused before a later cog/ loose alias");
    check(explicitTabs && explicitTabs->count() == 0
            && !std::filesystem::exists(explicitProject / "cog")
            && readBytes(looseRoot / "cog" / "explicit-alias.cog")
                == std::optional<std::string>(shadowedSource),
        "exact-name GOB refusal installs no tab, extracts nothing, and preserves the later loose alias");

    MainWindow suffixWindow{};
    configureWindow(suffixWindow, suffixProject);
    CogModalDriver suffixModal{};
    suffixModal.start();
    suffixWindow.openCogScriptEditorForTest("suffix-alias");
    suffixModal.stop();
    QTabWidget* const suffixTabs = suffixWindow.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    check(suffixModal.sawLimitError()
            && suffixModal.lastErrorText().contains(QStringLiteral("16 MiB"), Qt::CaseInsensitive),
        "an oversized GOB .cog suffix winner is refused before a later cog/ loose alias");
    check(suffixTabs && suffixTabs->count() == 0
            && !std::filesystem::exists(suffixProject / "cog")
            && readBytes(looseRoot / "cog" / "suffix-alias.cog")
                == std::optional<std::string>(shadowedSource),
        "suffix-winner refusal installs no tab, extracts nothing, and preserves the later loose alias");

    MainWindow controlWindow{};
    configureWindow(controlWindow, controlProject);
    CogModalDriver controlModal{};
    controlModal.start();
    controlWindow.openCogScriptEditorForTest("control-alias.cog");
    controlModal.stop();
    QTabWidget* const controlTabs = controlWindow.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    check(controlTabs && controlTabs->count() == 1
            && !controlModal.sawOpenFailure()
            && !controlModal.sawLimitError()
            && !controlModal.timedOut(),
        "an in-budget exact-name GOB winner opens successfully when a later cog/ loose alias exists");
    QWidget* const controlPage = controlTabs ? controlTabs->currentWidget() : nullptr;
    check(controlPage && !controlPage->property("cogLooseFile").toBool()
            && controlPage->property("cogArchiveTooltip").toString().contains(
                QStringLiteral("aliases.gob")),
        "COG source metadata identifies the selected archive winner instead of a later loose alias");
    check(readBytes(controlProject / "cog" / "control-alias.cog")
            == std::optional<std::string>(archiveControlSource)
            && readBytes(looseRoot / "cog" / "control-alias.cog")
                == std::optional<std::string>(shadowedSource),
        "the in-budget archive winner is extracted as the working copy without changing its loose alias");

    const std::filesystem::path primaryFallbackRoot = root / "primary-fallback-resources";
    const std::filesystem::path primaryFallbackProject = root / "primary-fallback-project";
    const std::filesystem::path primaryFallbackPath =
        primaryFallbackRoot / "cog" / "primary-fallback.cog";
    std::error_code primaryFallbackError{};
    std::filesystem::create_directories(primaryFallbackRoot / "cog", primaryFallbackError);
    if ( !primaryFallbackError )
    {
        std::filesystem::create_directories(primaryFallbackProject, primaryFallbackError);
    }
    const bool primaryFallbackReady = !primaryFallbackError
        && writeBytes(primaryFallbackPath, archiveControlSource);
    check(primaryFallbackReady,
        "COG primary-override fallback fixture creates an isolated loose suffix source");
    if ( !primaryFallbackReady )
    {
        return;
    }
    MainWindow primaryFallbackWindow{};
    primaryFallbackWindow.setAttribute(Qt::WA_DontShowOnScreen);
    primaryFallbackWindow.setResourcePathsForTest({ primaryFallbackRoot }, {});
    primaryFallbackWindow.setCurrentPathForTitleForTest(primaryFallbackProject / "fixture.sed");
    int primaryFallbackReads = 0;
    primaryFallbackWindow.setPrimaryResourceOverrideForTest(
        "primary-fallback",
        [&primaryFallbackReads]() -> std::optional<std::vector<std::byte>>
        {
            ++primaryFallbackReads;
            return std::nullopt;
        });
    CogModalDriver primaryFallbackModal{};
    primaryFallbackModal.start();
    primaryFallbackWindow.openCogScriptEditorForTest("primary-fallback");
    primaryFallbackModal.stop();
    QTabWidget* const primaryFallbackTabs = primaryFallbackWindow.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    QWidget* const primaryFallbackPage = primaryFallbackTabs
        ? primaryFallbackTabs->currentWidget() : nullptr;
    check(primaryFallbackTabs && primaryFallbackTabs->count() == 1
            && primaryFallbackPage && primaryFallbackPage->property("cogLooseFile").toBool()
            && primaryFallbackReads == 1
            && !primaryFallbackModal.sawOpenFailure()
            && !primaryFallbackModal.sawLimitError()
            && !primaryFallbackModal.timedOut(),
        "a missing primary override falls through to the actual loose .cog source and reports it as loose");

    const std::filesystem::path activeLooseRoot = root / "active-resources";
    const std::filesystem::path activeLooseProject = root / "active-project";
    const std::filesystem::path activeLoosePath = activeLooseRoot / "loose-active.cog";
    std::error_code activeError{};
    std::filesystem::create_directories(activeLooseRoot, activeError);
    if ( !activeError )
    {
        std::filesystem::create_directories(activeLooseProject, activeError);
    }
    const bool activeFixtureReady = !activeError
        && writeBytes(activeLoosePath, "symbols\nint initial_loose\nend\n");
    check(activeFixtureReady,
        "COG active-loose fixture creates a source and isolated project destination");
    if ( !activeFixtureReady )
    {
        return;
    }

    MainWindow activeLooseWindow{};
    activeLooseWindow.setAttribute(Qt::WA_DontShowOnScreen);
    activeLooseWindow.setResourcePathsForTest({ activeLooseRoot }, {});
    activeLooseWindow.setCurrentPathForTitleForTest(activeLooseProject / "fixture.sed");
    CogModalDriver initialLooseModal{};
    initialLooseModal.start();
    activeLooseWindow.openCogScriptEditorForTest("loose-active.cog");
    initialLooseModal.stop();
    QTabWidget* activeLooseTabs = activeLooseWindow.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    QPlainTextEdit* activeLooseEditor = currentCogEditor(activeLooseTabs);
    QWidget* const activeLoosePage = activeLooseTabs ? activeLooseTabs->currentWidget() : nullptr;
    check(activeLooseTabs && activeLooseTabs->count() == 1 && activeLooseEditor
            && activeLoosePage && activeLoosePage->property("cogLooseFile").toBool()
            && !initialLooseModal.sawOpenFailure()
            && !initialLooseModal.sawLimitError()
            && !initialLooseModal.timedOut(),
        "an in-budget loose COG source opens as an editable tab");
    if ( !activeLooseEditor )
    {
        return;
    }
    activeLooseEditor->setPlainText(QStringLiteral("symbols\nint live_edit\nend\n"));
    const QString liveBuffer = activeLooseEditor->toPlainText();
    ui::stopLooseDocumentWatch(activeLooseTabs->currentWidget());
    const bool activeSourceReplaced = writeBytes(activeLoosePath, oversizedSource);
    check(activeSourceReplaced,
        "COG active-loose fixture replaces the source with an over-budget external version");
    if ( !activeSourceReplaced )
    {
        return;
    }

    CogModalDriver activeAgainModal{};
    activeAgainModal.start();
    activeLooseWindow.openCogScriptEditorForTest("loose-active.cog");
    activeAgainModal.stop();
    activeLooseTabs = activeLooseWindow.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    activeLooseEditor = currentCogEditor(activeLooseTabs);
    check(activeLooseTabs && activeLooseTabs->count() == 1 && activeLooseEditor
            && activeLooseEditor->toPlainText() == liveBuffer
            && !activeAgainModal.sawOpenFailure()
            && !activeAgainModal.sawLimitError()
            && !activeAgainModal.timedOut(),
        "reopening an existing loose COG tab after external growth preserves its live buffer without rereading the source");
}

void testCogArchiveTabActivationSkipsChangedSourceRead()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root = temporaryPath(temporary);
    const std::filesystem::path resourceRoot = root / "archive-active-resources";
    const std::filesystem::path archive = root / "archives" / "active-exact.gob";
    const std::string archiveSource = "symbols\nint archive_source\nend\n";
    std::error_code error{};
    if ( temporary.isValid() )
    {
        std::filesystem::create_directories(resourceRoot, error);
    }
    const bool fixtureReady = temporary.isValid()
        && !error
        && writeGobV2(archive, { { "active-archive.cog", archiveSource } });
    check(fixtureReady,
        "COG active archive fixture writes an exact-name GOB entry and mounted resource root");
    if ( !fixtureReady )
    {
        return;
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ resourceRoot }, { archive });
    CogModalDriver initialModal{};
    initialModal.start();
    window.openCogScriptEditorForTest("active-archive.cog");
    initialModal.stop();

    QTabWidget* const initialTabs = window.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    QPlainTextEdit* const initialEditor = currentCogEditor(initialTabs);
    QWidget* const page = initialTabs ? initialTabs->currentWidget() : nullptr;
    const std::filesystem::path workingCopy =
        resourceRoot / "cog" / "active-archive.cog";
    check(initialTabs && initialTabs->count() == 1 && initialEditor
            && page && !page->property("cogLooseFile").toBool()
            && !initialModal.sawOpenFailure()
            && !initialModal.sawLimitError()
            && !initialModal.timedOut()
            && readBytes(workingCopy) == std::optional<std::string>(archiveSource),
        "an exact-name archive COG opens and extracts its working copy under a mounted resource root");
    if ( !initialEditor || !page )
    {
        return;
    }

    appendLocalEdit(initialEditor);
    const CogBufferSnapshot liveBuffer = snapshotCogBuffer(window, initialEditor);
    check(liveBuffer.modified
            && validCogDocumentHistory(liveBuffer.history)
            && !liveBuffer.history.value(QStringLiteral("commands")).toArray().isEmpty()
            && liveBuffer.selection == QStringLiteral("local"),
        "archive-tab activation fixture captures a dirty buffer with history and a selected caret range");
    ui::stopLooseDocumentWatch(page);

    const bool archiveTruncated = writeBytes(archive, "");
    check(archiveTruncated,
        "COG active archive fixture truncates the mounted archive after opening its editor tab");
    if ( !archiveTruncated )
    {
        return;
    }

    CogModalDriver activationModal{};
    activationModal.start();
    window.openCogScriptEditorForTest("active-archive.cog");
    activationModal.stop();
    QTabWidget* const activeTabs = window.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    QPlainTextEdit* const activeEditor = currentCogEditor(activeTabs);
    if ( activationModal.sawOpenFailure() || activationModal.sawLimitError()
        || activationModal.timedOut() )
    {
        std::cerr << "COG existing archive-tab activation diagnostic: sawOpenFailure="
            << activationModal.sawOpenFailure()
            << " sawLimitError=" << activationModal.sawLimitError()
            << " timedOut=" << activationModal.timedOut()
            << " modal=[" << activationModal.lastModalDescription().toStdString() << "]\n";
    }
    check(!activationModal.sawOpenFailure()
            && !activationModal.sawLimitError()
            && !activationModal.timedOut(),
        "activating an existing exact-name archive COG tab does not reread the truncated mounted archive");
    check(activeTabs && activeTabs->count() == 1
            && activeTabs->currentWidget() == page
            && activeEditor == initialEditor
            && sameCogBuffer(window, activeEditor, liveBuffer)
            && readBytes(workingCopy) == std::optional<std::string>(archiveSource),
        "existing archive-tab activation preserves the live history, dirty state, caret selection, and extracted working copy");
}

void testCogOversizedArchivePreservesExistingExtractionTarget()
{
    constexpr std::size_t kCogByteBudget = 16u * 1024u * 1024u;
    QTemporaryDir temporary{};
    const std::filesystem::path root = temporaryPath(temporary);
    const std::filesystem::path resourceRoot = root / "guard-resources";
    const std::filesystem::path archive = root / "archives" / "guard.gob";
    const std::filesystem::path existingTarget = resourceRoot / "cog" / "guard.cog";
    const std::string authoredTarget = "symbols\nint keep_authored_target\nend\n";
    const std::string oversizedSource = makeCommentCogSource(kCogByteBudget + 1u);
    std::error_code error{};
    if ( temporary.isValid() )
    {
        std::filesystem::create_directories(existingTarget.parent_path(), error);
    }
    const bool fixtureReady = temporary.isValid()
        && !error
        && writeBytes(existingTarget, authoredTarget)
        && writeGobV2(archive, { { "guard.cog", oversizedSource } });
    check(fixtureReady,
        "COG oversized extraction-target fixture creates an authored target and oversized exact archive winner");
    if ( !fixtureReady )
    {
        return;
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ resourceRoot }, { archive });
    CogModalDriver modal{};
    modal.start();
    window.openCogScriptEditorForTest("guard.cog");
    modal.stop();

    QTabWidget* const tabs = window.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    check(modal.sawLimitError()
            && modal.lastErrorText().contains(QStringLiteral("16 MiB"), Qt::CaseInsensitive)
            && !modal.timedOut(),
        "an oversized exact-name archive winner is refused before using an existing extraction target");
    check(tabs && tabs->count() == 0
            && readBytes(existingTarget) == std::optional<std::string>(authoredTarget),
        "refusing the oversized winner installs no tab and leaves the pre-existing authored target unchanged");
}

void testCogReloadSizeBoundariesPreserveEditorState()
{
    QTemporaryDir temporary{};
    const std::filesystem::path root = temporaryPath(temporary);
    const std::filesystem::path path = root / "cog" / "reload.cog";
    const QString initial = QStringLiteral("symbols\nint baseline\nend\n");
    const QString external = QStringLiteral("symbols\nint external\nend\n");
    const bool sourceReady = temporary.isValid() && writeSmallCogSource(path, initial);
    check(sourceReady, "COG reload fixture writes a valid in-budget source");
    if ( !sourceReady )
    {
        return;
    }

    MainWindow window{};
    window.setResourcePathsForTest({ root }, {}, root / "reload.sed");
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();
    window.openCogScriptEditorForTest("reload.cog");
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QPlainTextEdit* const editor = currentCogEditor(tabs);
    QAction* const reloadAction = window.findChild<QAction*>(QStringLiteral("cog_editor.reload"));
    check(tabs && tabs->count() == 1 && editor && reloadAction,
        "COG reload fixture opens one real loose editor and resolves its configured Reload action");
    if ( !editor || !reloadAction )
    {
        return;
    }
    // The loose-document watcher is owned by the COG page, not its inner
    // QPlainTextEdit. Stop the real watcher so fixture rewrites cannot trigger
    // an unrelated external-change prompt.
    ui::stopLooseDocumentWatch(tabs->currentWidget());

    CogModalDriver modal{};
    const bool oversizedReady = writeLargeCogComment(path);
    check(oversizedReady
            && std::filesystem::file_size(path) > static_cast<std::uintmax_t>(kMaximumCogSourceBytes)
            && std::filesystem::file_size(path) == static_cast<std::uintmax_t>(kOversizedCogSourceBytes),
        "COG reload boundary source is exactly 17 MiB");
    if ( !oversizedReady )
    {
        return;
    }

    QTextCursor cleanCursor(editor->document());
    cleanCursor.setPosition(2);
    cleanCursor.setPosition(5, QTextCursor::KeepAnchor);
    editor->setTextCursor(cleanCursor);
    const CogBufferSnapshot cleanBefore = snapshotCogBuffer(window, editor);
    check(validCogDocumentHistory(cleanBefore.history),
        "clean reload snapshot includes the serialized CogDocumentHistory state");
    triggerCogReload(window, editor, reloadAction, modal);
    check(modal.sawLimitError() && modal.lastErrorText().contains(QStringLiteral("16 MiB"), Qt::CaseInsensitive),
        "manual Reload reports the explicit limit for an oversized source and does not read it all");
    if ( !modal.sawLimitError() )
    {
        std::cerr << "COG clean Reload modal diagnostic: "
            << modal.lastModalDescription().toStdString() << '\n';
    }
    check(sameCogBuffer(window, editor, cleanBefore),
        "rejecting an oversized reload leaves the clean buffer, exact CogDocumentHistory timeline, and selection unchanged");

    editor->setPlainText(initial);
    editor->document()->setModified(false);
    QApplication::processEvents();
    appendLocalEdit(editor);
    const CogBufferSnapshot dirtyBefore = snapshotCogBuffer(window, editor);
    const QCborArray dirtyCommands = dirtyBefore.history.value(QStringLiteral("commands")).toArray();
    check(validCogDocumentHistory(dirtyBefore.history)
            && dirtyCommands.size() > 0
            && dirtyBefore.history.value(QStringLiteral("cursor")).toInteger() > 0,
        "dirty reload snapshot contains an authored CogDocumentHistory command and cursor");
    modal.clearObservations();
    triggerCogReload(window, editor, reloadAction, modal);
    check(modal.sawLimitError() && modal.lastErrorText().contains(QStringLiteral("16 MiB"), Qt::CaseInsensitive),
        "manual Reload reports the size error before it can discard a dirty COG buffer");
    if ( !modal.sawLimitError() )
    {
        std::cerr << "COG dirty Reload modal diagnostic: "
            << modal.lastModalDescription().toStdString()
            << " modified=" << editor->document()->isModified() << '\n';
    }
    check(sameCogBuffer(window, editor, dirtyBefore),
        "rejecting an oversized reload preserves dirty text, exact CogDocumentHistory timeline, and selection");

    editor->setPlainText(initial);
    editor->document()->setModified(false);
    QApplication::processEvents();
    appendLocalEdit(editor);
    const CogBufferSnapshot postPromptBefore = snapshotCogBuffer(window, editor);
    check(validCogDocumentHistory(postPromptBefore.history)
            && postPromptBefore.history.value(QStringLiteral("commands")).toArray().size() > 0,
        "post-confirm snapshot captures the dirty CogDocumentHistory timeline before the file grows");
    modal.clearObservations();
    modal.setBeforeDiscard([&path]()
        {
            (void)writeLargeCogComment(path);
        });
    const bool externalReady = writeSmallCogSource(path, external);
    check(externalReady, "post-confirm COG reload fixture publishes a small external revision");
    if ( !externalReady )
    {
        return;
    }
    triggerCogReload(window, editor, reloadAction, modal);
    check(modal.sawReloadQuestion(),
        "dirty manual Reload asks before replacing the buffer when the initial disk snapshot fits the limit");
    if ( !modal.sawReloadQuestion() )
    {
        std::cerr << "COG dirty confirmation modal diagnostic: "
            << modal.lastModalDescription().toStdString()
            << " modified=" << editor->document()->isModified()
            << " actionEnabled=" << reloadAction->isEnabled() << '\n';
    }
    check(modal.sawLimitError() && modal.lastErrorText().contains(QStringLiteral("16 MiB"), Qt::CaseInsensitive),
        "manual Reload rechecks the source limit after confirmation when the file grows during the prompt");
    check(sameCogBuffer(window, editor, postPromptBefore),
        "a source that grows past the limit during confirmation leaves the exact dirty buffer, CogDocumentHistory timeline, and selection intact");
}

void testCogLooseSaveExternalRevisionCancelPreservesFormattedDirtyBuffer()
{
    ScopedCogFormattingPreferences formatting(true);
    QTemporaryDir temporary{};
    const std::filesystem::path root = temporaryPath(temporary);
    const std::filesystem::path path = root / "cog" / "external_cancel.cog";
    const QString initial = QStringLiteral(
        "code\n"
        "startup:\n"
        "if(score) {\n"
        "return;\n"
        "}\n"
        "end\n");
    const QString external = QStringLiteral("symbols\nint externalRevision\nend\n");
    const bool sourceReady = temporary.isValid() && writeSmallCogSource(path, initial);
    check(sourceReady, "ordinary loose COG Save race fixture starts from an exact UTF-8 source revision");
    if ( !sourceReady )
    {
        return;
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "external_cancel.sed");
    window.openCogScriptEditorForTest("external_cancel.cog");
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QPlainTextEdit* const editor = currentCogEditor(tabs);
    check(tabs && tabs->count() == 1 && editor,
        "ordinary loose COG Save race opens one isolated real source editor");
    if ( !editor )
    {
        return;
    }
    ui::stopLooseDocumentWatch(tabs->currentWidget());

    appendLocalEdit(editor);
    const CogBufferSnapshot beforeSave = snapshotCogBuffer(window, editor);
    ui::cog::CogScriptFormatOptions formatOptions{};
    formatOptions.indentWidth = 4;
    formatOptions.braceStyle = ui::cog::BraceStyle::nextLine;
    formatOptions.indentSectionBodies = true;
    formatOptions.indentCodeBodies = true;
    formatOptions.keywordSpacing = true;
    formatOptions.insideParenthesesSpacing = false;
    formatOptions.functionParenthesesSpacing = false;
    formatOptions.forSemicolonSpacing = true;
    formatOptions.alignFinalReturnLabel = false;
    formatOptions.trimExcessBlankLines = true;
    formatOptions.indentComments = false;
    check(validCogDocumentHistory(beforeSave.history)
            && ui::cog::formatCogScriptText(beforeSave.text, formatOptions) != beforeSave.text,
        "the cancellation fixture has a real authored history and format-on-save would visibly alter its buffer");
    const QByteArray externalBytes = external.toUtf8();
    const bool externalReady = writeSmallCogSource(path, external);
    check(externalReady, "an external writer replaces the open COG source without dispatching editor events");
    if ( !externalReady )
    {
        return;
    }

    CogModalDriver modal{};
    modal.setSaveConflictChoice(QStringLiteral("Cancel"));
    modal.start();
    const bool saved = window.saveCogScriptEditorForTest(0);
    check(modal.sawSaveConflict() && modal.saveConflictButtonsValid()
            && modal.saveConflictCancelPolicyValid() && !modal.timedOut(),
        "ordinary loose COG Save detects a raced disk revision synchronously and offers Reload from Disk, Overwrite Disk, and a default/Escape Cancel");
    const bool bufferUnchanged = sameCogBuffer(window, editor, beforeSave);
    modal.stop();
    check(!saved
            && readBytes(path) == std::optional<std::string>(
                std::string(externalBytes.constData(), static_cast<std::size_t>(externalBytes.size()))),
        "Cancel leaves the external revision byte-for-byte on disk instead of overwriting it");
    check(bufferUnchanged,
        "Cancel runs before format-on-save and preserves exact dirty text, caret, selection, native history, and Undo/Redo state");
}

void testCogLooseSaveReloadsLatestBoundedExternalRevision()
{
    ScopedCogFormattingPreferences formatting(false);
    QTemporaryDir temporary{};
    const std::filesystem::path root = temporaryPath(temporary);
    const std::filesystem::path path = root / "cog" / "external_reload.cog";
    const QString initial = QStringLiteral("symbols\nint baseline\nend\n");
    const QString externalBeforePrompt = QStringLiteral("symbols\nint external\nend\n");
    const QString externalLatest = QStringLiteral("symbols\nint latest\nend\n");
    const bool sourceReady = temporary.isValid() && writeSmallCogSource(path, initial);
    check(sourceReady, "COG conflict Reload fixture writes its exact initial source");
    if ( !sourceReady )
    {
        return;
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "external_reload.sed");
    window.openCogScriptEditorForTest("external_reload.cog");
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QPlainTextEdit* editor = currentCogEditor(tabs);
    check(tabs && tabs->count() == 1 && editor,
        "COG conflict Reload fixture opens its isolated loose editor");
    if ( !editor )
    {
        return;
    }
    ui::stopLooseDocumentWatch(tabs->currentWidget());
    appendLocalEdit(editor);
    check(writeSmallCogSource(path, externalBeforePrompt),
        "an external COG revision arrives before synchronous conflict resolution");

    CogModalDriver modal{};
    modal.setSaveConflictChoice(QStringLiteral("Reload from Disk"));
    modal.setBeforeSaveConflictChoice([&path, externalLatest]()
        {
            (void)writeSmallCogSource(path, externalLatest);
        });
    modal.start();
    const bool saved = window.saveCogScriptEditorForTest(0);
    const QByteArray latestBytes = externalLatest.toUtf8();
    const CogBufferSnapshot reloaded = snapshotCogBuffer(window, editor);
    modal.stop();
    check(!saved && modal.sawSaveConflict() && modal.saveConflictCount() == 1,
        "choosing Reload from Disk resolves the pending Save conflict without writing the local buffer");
    check(editor->toPlainText() == externalLatest && !editor->document()->isModified()
            && !editor->document()->isUndoAvailable() && !editor->document()->isRedoAvailable()
            && reloaded.history.value(QStringLiteral("cursor")).toInteger(-1) == 0
            && reloaded.history.value(QStringLiteral("commands")).toArray().isEmpty()
            && readBytes(path) == std::optional<std::string>(
                std::string(latestBytes.constData(), static_cast<std::size_t>(latestBytes.size()))),
        "Reload rereads the latest bounded disk bytes after the choice and installs them as the clean source baseline");

    appendLocalEdit(editor);
    const CogBufferSnapshot beforeOversizedReload = snapshotCogBuffer(window, editor);
    const QString secondExternal = QStringLiteral("symbols\nint secondExternal\nend\n");
    check(writeSmallCogSource(path, secondExternal),
        "a second external COG revision prepares the post-prompt byte-limit check");
    modal.clearObservations();
    modal.setSaveConflictChoice(QStringLiteral("Reload from Disk"));
    modal.setBeforeSaveConflictChoice([&path]()
        {
            (void)writeLargeCogComment(path);
        });
    modal.start();
    const bool oversizedSave = window.saveCogScriptEditorForTest(0);
    const bool oversizedBufferPreserved = sameCogBuffer(window, editor, beforeOversizedReload);
    modal.stop();
    check(!oversizedSave && modal.sawSaveConflict() && modal.sawLimitError()
            && modal.lastErrorText().contains(QStringLiteral("16 MiB"), Qt::CaseInsensitive)
            && std::filesystem::file_size(path) == static_cast<std::uintmax_t>(kOversizedCogSourceBytes),
        "Reload rechecks the existing 16 MiB COG source bound after its nested choice");
    check(oversizedBufferPreserved,
        "an oversized latest COG revision leaves the exact authored buffer and native history untouched");
}

void testCogLooseSaveExplicitOverwriteCommitsAndAdvancesHash()
{
    ScopedCogFormattingPreferences formatting(false);
    QTemporaryDir temporary{};
    const std::filesystem::path root = temporaryPath(temporary);
    const std::filesystem::path path = root / "cog" / "external_overwrite.cog";
    const QString initial = QStringLiteral("symbols\nint baseline\nend\n");
    const QString external = QStringLiteral("symbols\nint external\nend\n");
    const bool sourceReady = temporary.isValid() && writeSmallCogSource(path, initial);
    check(sourceReady, "COG conflict Overwrite fixture writes a loose source baseline");
    if ( !sourceReady )
    {
        return;
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "external_overwrite.sed");
    window.openCogScriptEditorForTest("external_overwrite.cog");
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QPlainTextEdit* const editor = currentCogEditor(tabs);
    check(tabs && tabs->count() == 1 && editor,
        "COG conflict Overwrite fixture opens the real loose document");
    if ( !editor )
    {
        return;
    }
    ui::stopLooseDocumentWatch(tabs->currentWidget());
    appendLocalEdit(editor);
    const QByteArray localBytes = editor->toPlainText().toUtf8();
    check(writeSmallCogSource(path, external),
        "an external editor publishes a conflicting COG revision before explicit Overwrite");

    CogModalDriver modal{};
    modal.setSaveConflictChoice(QStringLiteral("Overwrite Disk"));
    modal.start();
    const bool saved = window.saveCogScriptEditorForTest(0);
    const QByteArray captured = window.captureCogEditorRecoveryStateForTest();
    const QCborMap tab = cogRecoveryTab(captured);
    const QByteArray expectedHash = cogSourceHash(localBytes);
    const CogBufferSnapshot afterSave = snapshotCogBuffer(window, editor);
    modal.stop();
    const int historyCursor = static_cast<int>(afterSave.history.value(QStringLiteral("cursor")).toInteger(-1));
    check(saved && modal.sawSaveConflict() && modal.saveConflictButtonsValid()
            && readBytes(path) == std::optional<std::string>(
                std::string(localBytes.constData(), static_cast<std::size_t>(localBytes.size())))
            && !afterSave.modified && afterSave.text.toUtf8() == localBytes
            && afterSave.history.value(QStringLiteral("clean")).toInteger(-2) == historyCursor
            && historyCursor > 0 && afterSave.undoAvailable,
        "explicit Overwrite writes exact authored COG bytes and advances the native clean marker without discarding Undo");
    check(tab.value(QStringLiteral("acceptedSourceHash")).toByteArray() == expectedHash,
        "a committed COG Overwrite persists the SHA-256 of exactly the bytes it wrote as the accepted source hash");
    const bool restoredSavedState = window.restoreCogEditorRecoveryStateForTest(captured);
    QTabWidget* const restoredTabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QPlainTextEdit* const restoredEditor = currentCogEditor(restoredTabs);
    check(restoredSavedState && restoredEditor && sameCogBuffer(window, restoredEditor, afterSave)
            && cogRecoveryTab(window.captureCogEditorRecoveryStateForTest())
                .value(QStringLiteral("acceptedSourceHash")).toByteArray() == expectedHash,
        "COG recovery Save/restore retains the exact accepted hash together with the native saved Undo timeline");
}

void testCogLooseSaveWatcherKeepDoesNotRebaseAndOwnSaveStaysQuiet()
{
    ScopedCogFormattingPreferences formatting(false);
    QTemporaryDir temporary{};
    const std::filesystem::path root = temporaryPath(temporary);
    const std::filesystem::path path = root / "cog" / "watch_keep_save.cog";
    const QString initial = QStringLiteral("symbols\nint baseline\nend\n");
    const QString external = QStringLiteral("symbols\nint watcherRevision\nend\n");
    const bool sourceReady = temporary.isValid() && writeSmallCogSource(path, initial);
    check(sourceReady, "COG watcher Keep and Save fixture writes an isolated initial source");
    if ( !sourceReady )
    {
        return;
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "watch_keep_save.sed");
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();
    window.openCogScriptEditorForTest("watch_keep_save.cog");
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QPlainTextEdit* const editor = currentCogEditor(tabs);
    check(tabs && tabs->count() == 1 && editor,
        "COG watcher Keep and Save fixture opens one watched loose page");
    if ( !editor )
    {
        return;
    }

    appendLocalEdit(editor);
    const CogBufferSnapshot beforeExternalChange = snapshotCogBuffer(window, editor);
    CogModalDriver modal{};
    modal.start();
    const bool externalReady = writeSmallCogSource(path, external);
    const bool watcherOffered = externalReady && waitForCogTestState([&]()
        {
            return modal.looseWatchPromptCount() == 1;
        });
    check(watcherOffered && sameCogBuffer(window, editor, beforeExternalChange),
        "the asynchronous watcher Keep acknowledgement preserves the local COG text and native history");
    observeQuietCogTestWatchEvents();
    check(modal.looseWatchPromptCount() == 1,
        "acknowledging a COG external revision prevents duplicate watcher offers for that revision");
    modal.stop();

    modal.setSaveConflictChoice(QStringLiteral("Cancel"));
    modal.start();
    const bool canceled = window.saveCogScriptEditorForTest(0);
    const bool cancelPreservedBuffer = sameCogBuffer(window, editor, beforeExternalChange);
    observeQuietCogTestWatchEvents();
    modal.stop();
    check(!canceled && modal.saveConflictCount() == 1
            && readBytes(path) == std::optional<std::string>(cogByteString(external.toUtf8()))
            && cancelPreservedBuffer,
        "watcher Keep acknowledges its notice without rebasing the COG Save baseline, so synchronous Save still protects the external revision");

    modal.setSaveConflictChoice(QStringLiteral("Overwrite Disk"));
    modal.start();
    const bool overwritten = window.saveCogScriptEditorForTest(0);
    observeQuietCogTestWatchEvents();
    modal.stop();
    const QByteArray localBytes = beforeExternalChange.text.toUtf8();
    check(overwritten && modal.saveConflictCount() == 2
            && readBytes(path) == std::optional<std::string>(
                std::string(localBytes.constData(), static_cast<std::size_t>(localBytes.size())))
            && modal.looseWatchPromptCount() == 1,
        "an explicit COG Overwrite accepts its committed bytes before watcher delivery and produces no self-change offer");
}

void testCogAcceptedSourceHashSurvivesRecoveryAndLegacyIsUnknown()
{
    ScopedCogFormattingPreferences formatting(false);
    QTemporaryDir temporary{};
    const std::filesystem::path root = temporaryPath(temporary);
    const std::filesystem::path path = root / "cog" / "recovered_source_hash.cog";
    const QString initial = QStringLiteral("symbols\nint baseline\nend\n");
    const QString external = QStringLiteral("symbols\nint externalAfterCheckpoint\nend\n");
    const QByteArray initialBytes = initial.toUtf8();
    const bool sourceReady = temporary.isValid() && writeSmallCogSource(path, initial);
    check(sourceReady, "COG recovery source hash fixture writes a known loose disk baseline");
    if ( !sourceReady )
    {
        return;
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "recovered_source_hash.sed");
    window.openCogScriptEditorForTest("recovered_source_hash.cog");
    QTabWidget* tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    QPlainTextEdit* editor = currentCogEditor(tabs);
    check(tabs && tabs->count() == 1 && editor,
        "COG recovery source hash fixture opens one ordinary loose document");
    if ( !editor )
    {
        return;
    }
    ui::stopLooseDocumentWatch(tabs->currentWidget());
    const CogBufferSnapshot cleanBeforeMetadataChange = snapshotCogBuffer(window, editor);
    const QByteArray cleanCheckpoint = window.captureCogEditorRecoveryStateForTest();
    QCborMap metadataRoot = QCborValue::fromCbor(cleanCheckpoint).toMap();
    QCborArray metadataTabs = metadataRoot.value(QStringLiteral("tabs")).toArray();
    QCborMap metadataTab = metadataTabs.isEmpty() ? QCborMap{} : metadataTabs.at(0).toMap();
    const QByteArray independentHash(32, 'z');
    metadataTab.insert(QStringLiteral("acceptedSourceHash"), independentHash);
    if ( !metadataTabs.isEmpty() )
    {
        metadataTabs[0] = metadataTab;
    }
    metadataRoot.insert(QStringLiteral("tabs"), metadataTabs);
    const bool restoredMetadataOnly = window.restoreCogEditorRecoveryStateForTest(QCborValue(metadataRoot).toCbor());
    tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    editor = currentCogEditor(tabs);
    if ( tabs && tabs->currentWidget() )
    {
        ui::stopLooseDocumentWatch(tabs->currentWidget());
    }
    check(restoredMetadataOnly && editor && sameCogBuffer(window, editor, cleanBeforeMetadataChange)
            && cogRecoveryTab(window.captureCogEditorRecoveryStateForTest())
                .value(QStringLiteral("acceptedSourceHash")).toByteArray() == independentHash,
        "changing only accepted-source metadata does not dirty the recovered COG buffer or enter its authored history");
    const bool restoredOriginalClean = window.restoreCogEditorRecoveryStateForTest(cleanCheckpoint);
    tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    editor = currentCogEditor(tabs);
    if ( tabs && tabs->currentWidget() )
    {
        ui::stopLooseDocumentWatch(tabs->currentWidget());
    }
    check(restoredOriginalClean && editor && sameCogBuffer(window, editor, cleanBeforeMetadataChange)
            && cogRecoveryTab(window.captureCogEditorRecoveryStateForTest())
                .value(QStringLiteral("acceptedSourceHash")).toByteArray() == cogSourceHash(initialBytes),
        "restoring the original clean checkpoint restores its accepted COG source hash without changing its native clean state");
    if ( !restoredOriginalClean || !editor )
    {
        return;
    }
    appendLocalEdit(editor);
    const CogBufferSnapshot authored = snapshotCogBuffer(window, editor);
    const QByteArray captured = window.captureCogEditorRecoveryStateForTest();
    const QCborMap tab = cogRecoveryTab(captured);
    check(tab.value(QStringLiteral("acceptedSourceHash")).toByteArray() == cogSourceHash(initialBytes),
        "COG recovery capture stores the source SHA-256 accepted when the loose file was opened");

    const auto malformedHashRejectedWithoutMutation = [&](const QCborValue& invalidHash)
    {
        QCborMap invalidRoot = QCborValue::fromCbor(captured).toMap();
        QCborArray invalidTabs = invalidRoot.value(QStringLiteral("tabs")).toArray();
        if ( invalidTabs.isEmpty() )
        {
            return false;
        }
        QCborMap invalidTab = invalidTabs.at(0).toMap();
        invalidTab.insert(QStringLiteral("acceptedSourceHash"), invalidHash);
        invalidTabs[0] = invalidTab;
        invalidRoot.insert(QStringLiteral("tabs"), invalidTabs);
        const QByteArray before = window.captureCogEditorRecoveryStateForTest();
        const bool restored = window.restoreCogEditorRecoveryStateForTest(QCborValue(invalidRoot).toCbor());
        return !restored && window.captureCogEditorRecoveryStateForTest() == before;
    };
    check(malformedHashRejectedWithoutMutation(QCborValue(QStringLiteral("not bytes"))),
        "COG recovery rejects a non-byte accepted source hash without changing the live workspace");
    check(malformedHashRejectedWithoutMutation(QCborValue(QByteArray(31, 'x'))),
        "COG recovery rejects a 31-byte accepted source hash without changing the live workspace");
    check(malformedHashRejectedWithoutMutation(QCborValue(QByteArray(33, 'x'))),
        "COG recovery rejects a 33-byte accepted source hash without changing the live workspace");

    check(writeSmallCogSource(path, external),
        "the loose COG source changes after its unsaved recovery checkpoint is captured");
    const bool restoredKnown = window.restoreCogEditorRecoveryStateForTest(captured);
    tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    editor = currentCogEditor(tabs);
    if ( tabs && tabs->currentWidget() )
    {
        ui::stopLooseDocumentWatch(tabs->currentWidget());
    }
    const QByteArray afterKnownRestore = window.captureCogEditorRecoveryStateForTest();
    check(restoredKnown && editor && sameCogBuffer(window, editor, authored)
            && cogRecoveryTab(afterKnownRestore).value(QStringLiteral("acceptedSourceHash")).toByteArray()
                == cogSourceHash(initialBytes),
        "restoring a COG checkpoint preserves its authored history and the original accepted source hash instead of adopting newer disk bytes");

    QCborMap emptyRoot = QCborValue::fromCbor(captured).toMap();
    QCborArray emptyTabs = emptyRoot.value(QStringLiteral("tabs")).toArray();
    QCborMap emptyTab = emptyTabs.isEmpty() ? QCborMap{} : emptyTabs.at(0).toMap();
    emptyTab.insert(QStringLiteral("acceptedSourceHash"), QByteArray{});
    if ( !emptyTabs.isEmpty() )
    {
        emptyTabs[0] = emptyTab;
    }
    emptyRoot.insert(QStringLiteral("tabs"), emptyTabs);
    const bool restoredEmpty = window.restoreCogEditorRecoveryStateForTest(QCborValue(emptyRoot).toCbor());
    tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    editor = currentCogEditor(tabs);
    if ( tabs && tabs->currentWidget() )
    {
        ui::stopLooseDocumentWatch(tabs->currentWidget());
    }
    const CogBufferSnapshot beforeEmptySave = editor ? snapshotCogBuffer(window, editor) : CogBufferSnapshot{};
    CogModalDriver emptyModal{};
    emptyModal.setSaveConflictChoice(QStringLiteral("Cancel"));
    emptyModal.start();
    const bool emptySave = window.saveCogScriptEditorForTest(0);
    const bool emptyStatePreserved = editor && sameCogBuffer(window, editor, beforeEmptySave);
    emptyModal.stop();
    check(restoredEmpty && editor && !emptySave && emptyModal.sawSaveConflict()
            && emptyStatePreserved
            && readBytes(path) == std::optional<std::string>(cogByteString(external.toUtf8())),
        "an empty accepted-source hash is an unknown baseline and requires explicit resolution without changing the recovered buffer");

    QCborMap legacyRoot = QCborValue::fromCbor(captured).toMap();
    QCborArray legacyTabs = legacyRoot.value(QStringLiteral("tabs")).toArray();
    QCborMap legacyTab = legacyTabs.isEmpty() ? QCborMap{} : legacyTabs.at(0).toMap();
    legacyTab.remove(QStringLiteral("acceptedSourceHash"));
    if ( !legacyTabs.isEmpty() )
    {
        legacyTabs[0] = legacyTab;
    }
    legacyRoot.insert(QStringLiteral("tabs"), legacyTabs);
    const QByteArray legacyPayload = QCborValue(legacyRoot).toCbor();
    const bool restoredLegacy = window.restoreCogEditorRecoveryStateForTest(legacyPayload);
    tabs = window.findChild<QTabWidget*>(QStringLiteral("cogScriptEditorTabs"));
    editor = currentCogEditor(tabs);
    if ( tabs && tabs->currentWidget() )
    {
        ui::stopLooseDocumentWatch(tabs->currentWidget());
    }
    const CogBufferSnapshot beforeLegacySave = editor ? snapshotCogBuffer(window, editor) : CogBufferSnapshot{};
    CogModalDriver modal{};
    modal.setSaveConflictChoice(QStringLiteral("Cancel"));
    modal.start();
    const bool legacySave = window.saveCogScriptEditorForTest(0);
    const bool legacyStatePreserved = editor && sameCogBuffer(window, editor, beforeLegacySave);
    modal.stop();
    check(restoredLegacy && editor && !legacySave && modal.sawSaveConflict()
            && legacyStatePreserved
            && readBytes(path) == std::optional<std::string>(cogByteString(external.toUtf8())),
        "a legacy dirty loose COG recovery with no accepted hash treats its baseline as unknown and requires explicit conflict resolution without changing its buffer or history");
}

} // namespace

void runResourceIoBoundaryTests()
{
    testExistingLooseResourceSaveAcrossProjectBoundaries();
    testOversizedCogOpenDoesNotInstallATab();
    testCogArchiveOpenAdmissionPrecedesExtraction();
    testCogArchiveWinnerPrecedesLooseAliases();
    testCogArchiveTabActivationSkipsChangedSourceRead();
    testCogOversizedArchivePreservesExistingExtractionTarget();
    testCogReloadSizeBoundariesPreserveEditorState();
    testCogLooseSaveExternalRevisionCancelPreservesFormattedDirtyBuffer();
    testCogLooseSaveReloadsLatestBoundedExternalRevision();
    testCogLooseSaveExplicitOverwriteCommitsAndAdvancesHash();
    testCogLooseSaveWatcherKeepDoesNotRebaseAndOwnSaveStaysQuiet();
    testCogAcceptedSourceHashSurvivesRecoveryAndLegacyIsUnknown();
}

} // namespace sed::app_tests
