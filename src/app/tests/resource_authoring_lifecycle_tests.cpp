#include "test_common.hpp"

#include "native_caption_test_utils.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/image.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/snd.hpp"
#include "sed/assets/spr.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/dock_window_drop_filter.hpp"
#include "ui/components/viewport_icons.hpp"
#include "ui/recovery/document_recovery_utils.hpp"
#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/resources/resource_authoring_detail.hpp"

#include <QAbstractButton>
#include <QAbstractAnimation>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QDir>
#include <QDrag>
#include <QDockWidget>
#include <QEventLoop>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMargins>
#include <QMetaObject>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPoint>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QRegion>
#include <QTabBar>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QSlider>
#include <QScreen>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QWindow>
#include <QTreeWidgetItem>
#include <QToolButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QVector3D>

#include <filesystem>
#include <fstream>
#include <functional>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <system_error>
#include <string_view>
#include <utility>
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

bool nativeMaximizeButtonClickRoundTrip(QDockWidget* const dock)
{
    return native_caption_test::maximizeButtonClickRoundTrip(dock);
}

void createResourceDraftFromGroup(QAction* const groupAction)
{
    MainWindow* window = nullptr;
    for ( QObject* owner = groupAction; owner && !window; owner = owner->parent() )
    {
        window = qobject_cast<MainWindow*>(owner);
    }
    const QString actionName = groupAction ? groupAction->objectName() : QString{};
    const QString dockName = actionName.contains(QStringLiteral("sound_class"))
        ? QStringLiteral("soundClassResourceEditorsDock")
        : actionName.contains(QStringLiteral("string_table"))
            ? QStringLiteral("uniResourceEditorsDock") : QStringLiteral("resourceEditorsDock");
    QDockWidget* const dock = window ? window->findChild<QDockWidget*>(dockName) : nullptr;
    QToolButton* const newButton = dock
        ? dock->findChild<QToolButton*>(QStringLiteral("resource_authoring.new")) : nullptr;
    check(groupAction && dock && newButton && newButton->isEnabled(),
        "resource fixture resolves the explicit New control for its requested editor group");
    if ( dock && newButton && newButton->isEnabled() )
    {
        dock->show();
        dock->raise();
        newButton->click();
    }
}

using ModalMatch = std::function<bool(QDialog*)>;
using ModalReady = std::function<bool(QDialog*)>;
using ModalAction = std::function<void(QDialog*)>;

struct ModalStep
{
    ModalMatch matches{};
    ModalReady ready{};
    ModalAction interact{};
};

class BoundedModalSequence final
{
public:
    explicit BoundedModalSequence(std::vector<ModalStep> steps, const int deadlineMs = 4000)
        : steps_(std::move(steps)),
          poll_(&context_),
          deadline_(&context_),
          deadlineMs_(deadlineMs)
    {
        poll_.setInterval(5);
        QObject::connect(&poll_, &QTimer::timeout, &context_, [this]()
            {
                pollActiveModal();
            });
        deadline_.setSingleShot(true);
        QObject::connect(&deadline_, &QTimer::timeout, &context_, [this]()
            {
                timedOut_ = true;
                dismissActiveModals();
            });
    }

    ~BoundedModalSequence()
    {
        stop();
        if ( timedOut_ )
        {
            dismissActiveModals();
        }
    }

    BoundedModalSequence(const BoundedModalSequence&) = delete;
    BoundedModalSequence& operator=(const BoundedModalSequence&) = delete;

    void start()
    {
        deadline_.start(deadlineMs_);
        poll_.start();
    }

    void stop()
    {
        poll_.stop();
        deadline_.stop();
    }

    std::size_t completedSteps() const noexcept
    {
        return completedSteps_;
    }

    bool timedOut() const noexcept
    {
        return timedOut_;
    }

private:
    void pollActiveModal()
    {
        QDialog* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if ( !dialog || completedSteps_ >= steps_.size() )
        {
            return;
        }

        ModalStep& step = steps_[completedSteps_];
        if ( step.matches && !step.matches(dialog) )
        {
            return;
        }
        if ( step.ready && !step.ready(dialog) )
        {
            return;
        }

        ++completedSteps_;
        if ( step.interact )
        {
            step.interact(dialog);
        }
    }

    void dismissActiveModals()
    {
        for ( int attempt = 0; attempt < 4; ++attempt )
        {
            QWidget* const active = QApplication::activeModalWidget();
            if ( !active )
            {
                return;
            }
            if ( QDialog* const dialog = qobject_cast<QDialog*>(active) )
            {
                dialog->reject();
            }
            else
            {
                active->close();
            }
            QApplication::processEvents();
        }
    }

    QObject context_{};
    std::vector<ModalStep> steps_{};
    QTimer poll_{};
    QTimer deadline_{};
    int deadlineMs_{};
    std::size_t completedSteps_{};
    bool timedOut_{};
};

std::filesystem::path temporaryRoot(const QTemporaryDir& directory)
{
    return std::filesystem::path(directory.path().toStdWString());
}

bool writeFile(const std::filesystem::path& path, const std::string_view bytes)
{
    std::error_code error{};
    std::filesystem::create_directories(path.parent_path(), error);
    if ( error )
    {
        return false;
    }
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

std::optional<std::string> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if ( !input )
    {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(input), {});
}

bool writeOversizedValidUni(const std::filesystem::path& path)
{
    std::error_code error{};
    std::filesystem::create_directories(path.parent_path(), error);
    if ( error )
    {
        return false;
    }

    constexpr std::size_t maximumBytes = 16U * 1024U * 1024U;
    constexpr std::size_t commentBytes = maximumBytes + 1024U;
    constexpr std::size_t commentLineBytes = 1024U;
    static_assert(1U + 1022U + 1U == commentLineBytes);
    const std::string commentLine = "#" + std::string(1022U, 'x') + "\n";
    std::ofstream output(path, std::ios::binary);
    if ( !output )
    {
        return false;
    }
    for ( std::size_t written = 0; written < commentBytes; written += commentLineBytes )
    {
        output.write(
            commentLine.data(), static_cast<std::streamsize>(commentLine.size()));
    }
    constexpr std::string_view validTable =
        "MSGS 1\n\n\"KEY\" 0 \"value\"\n\nEND\n";
    output.write(validTable.data(), static_cast<std::streamsize>(validTable.size()));
    return static_cast<bool>(output);
}

bool writeSingleEntryGob(
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
    if ( error )
    {
        return false;
    }
    std::ofstream output(path, std::ios::binary);
    if ( !output )
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

bool makePickerResources(const std::filesystem::path& root)
{
    return writeFile(root / "mat" / "chosen.mat", "MAT ")
        && writeFile(root / "mat" / "same.mat", "MAT ")
        && writeFile(root / "sound" / "chosen.wav", "RIFF")
        && writeFile(root / "sound" / "shared.wav", "RIFF");
}

QByteArray makeRgbaMatCels(const std::vector<std::pair<int, std::vector<QColor>>>& cels)
{
    QByteArray bytes("MAT ", 4);
    const auto appendU32 = [&bytes](const std::uint32_t value)
    {
        bytes.append(static_cast<char>(value & 0xffu));
        bytes.append(static_cast<char>((value >> 8u) & 0xffu));
        bytes.append(static_cast<char>((value >> 16u) & 0xffu));
        bytes.append(static_cast<char>((value >> 24u) & 0xffu));
    };

    appendU32(0x32u);
    appendU32(2u);
    appendU32(static_cast<std::uint32_t>(cels.size()));
    appendU32(static_cast<std::uint32_t>(cels.size()));
    appendU32(static_cast<std::uint32_t>(sed::ColorMode::RGBA));
    appendU32(32u);
    appendU32(8u);
    appendU32(8u);
    appendU32(8u);
    appendU32(16u);
    appendU32(8u);
    appendU32(0u);
    appendU32(0u);
    appendU32(0u);
    appendU32(0u);
    appendU32(8u);
    appendU32(24u);
    appendU32(0u);

    for ( std::size_t index = 0; index < cels.size(); ++index )
    {
        for ( int field = 0; field < 10; ++field )
        {
            appendU32(field == 1 ? 6u : 0u);
        }
    }
    for ( const auto& [width, pixels] : cels )
    {
        appendU32(static_cast<std::uint32_t>(width));
        appendU32(2u);
        appendU32(0u);
        appendU32(0u);
        appendU32(0u);
        appendU32(1u);
        for ( const QColor& pixel : pixels )
        {
            bytes.append(static_cast<char>(pixel.blue()));
            bytes.append(static_cast<char>(pixel.green()));
            bytes.append(static_cast<char>(pixel.red()));
            bytes.append(static_cast<char>(pixel.alpha()));
        }
    }
    return bytes;
}

bool imageHasDominantPixels(const QImage& image, const bool blue)
{
    for ( int y = 0; y < image.height(); ++y )
    {
        for ( int x = 0; x < image.width(); ++x )
        {
            const QColor pixel = image.pixelColor(x, y);
            if ( blue
                ? pixel.blue() > pixel.red() + 40 && pixel.blue() > pixel.green() + 40
                : pixel.red() > pixel.blue() + 40 && pixel.red() > pixel.green() + 40 )
            {
                return true;
            }
        }
    }
    return false;
}

bool spriteInteriorMatchesTexel(GlView& preview, const QImage& frame, const bool blue)
{
    const auto corners = preview.thingSpriteCornersForTest(0u);
    if ( !corners || frame.isNull() || preview.width() <= 0 || preview.height() <= 0 )
    {
        return false;
    }
    const QVector3D center = ((*corners)[0] + (*corners)[1] + (*corners)[2] + (*corners)[3]) * 0.25f;
    const auto projected = preview.projectWorldToViewport(center);
    if ( !projected )
    {
        return false;
    }
    const int x = static_cast<int>(std::lround(projected->x() * frame.width() / preview.width()));
    const int y = static_cast<int>(std::lround(projected->y() * frame.height() / preview.height()));
    if ( x < 1 || y < 1 || x >= frame.width() - 1 || y >= frame.height() - 1 )
    {
        return false;
    }
    std::array<int, 3> channels{};
    for ( int dy = -1; dy <= 1; ++dy )
    {
        for ( int dx = -1; dx <= 1; ++dx )
        {
            const QColor pixel = frame.pixelColor(x + dx, y + dy);
            channels[0] += pixel.red();
            channels[1] += pixel.green();
            channels[2] += pixel.blue();
        }
    }
    const QColor expected = blue ? QColor(20, 40, 240) : QColor(240, 24, 32);
    constexpr int tolerance = 8;
    return std::abs(channels[0] / 9 - expected.red()) <= tolerance
        && std::abs(channels[1] / 9 - expected.green()) <= tolerance
        && std::abs(channels[2] / 9 - expected.blue()) <= tolerance;
}

std::shared_ptr<assets::ResourceLocator> makeLocator(
    const std::filesystem::path& root)
{
    auto resources = std::make_shared<assets::ResourceLocator>();
    if ( !root.empty() )
    {
        resources->addRoot(root);
    }
    return resources;
}

ui::resources::ResourceAuthoringContext testContext(
    const std::filesystem::path& root,
    const std::shared_ptr<assets::ResourceLocator>& resources)
{
    ui::resources::ResourceAuthoringContext context{};
    context.resources = resources;
    context.projectDirectory = root;
    context.masterCmp = "resource_lifecycle_master.cmp";
    return context;
}

QCborMap authoredDraftState(QCborMap state)
{
    state.remove(QStringLiteral("paneGeometry"));
    state.remove(QStringLiteral("editorPresentation"));
    state.remove(QStringLiteral("search"));
    state.remove(QStringLiteral("soundSearch"));
    state.remove(QStringLiteral("collapsedModes"));
    state.remove(QStringLiteral("presentation"));
    // Recovery writes canonical key order, while direct pane capture preserves
    // insertion order. Compare the same exact fields in one canonical order.
    return QCborValue::fromCbor(
        ui::recovery::serializeCanonicalCbor(QCborValue(state))).toMap();
}

void reportChangedStateKeys(
    const char* const label,
    const QCborMap& before,
    const QCborMap& after)
{
    std::cerr << label << " changed keys:";
    for ( const QCborValue& key : before.keys() )
    {
        if ( !after.contains(key) || before.value(key) != after.value(key) )
        {
            std::cerr << ' ' << key.toString().toStdString();
        }
    }
    for ( const QCborValue& key : after.keys() )
    {
        if ( !before.contains(key) )
        {
            std::cerr << ' ' << key.toString().toStdString();
        }
    }
    std::cerr << '\n';
}

QAbstractButton* button(QWidget* parent, const QString& objectName)
{
    return parent ? parent->findChild<QAbstractButton*>(objectName) : nullptr;
}

ModalStep fileDialogStep(
    const std::filesystem::path& path,
    const bool disableOverwriteConfirmation = false)
{
    ModalStep step{};
    step.matches = [](QDialog* dialog)
    {
        return dialog->objectName() == QStringLiteral("resource_authoring.file_dialog")
            && qobject_cast<QFileDialog*>(dialog);
    };
    step.interact = [path, disableOverwriteConfirmation](QDialog* dialog)
    {
        auto* const chooser = qobject_cast<QFileDialog*>(dialog);
        if ( !chooser )
        {
            dialog->reject();
            return;
        }
        if ( disableOverwriteConfirmation )
        {
            chooser->setOption(QFileDialog::DontConfirmOverwrite, true);
        }
        chooser->selectFile(QString::fromStdWString(path.wstring()));
        (void)QMetaObject::invokeMethod(
            chooser, "accept", Qt::DirectConnection);
    };
    return step;
}

ModalStep messageBoxStep(
    const QString& objectName,
    const QMessageBox::StandardButton answer,
    std::function<void()> beforeAnswer = {})
{
    ModalStep step{};
    step.matches = [objectName](QDialog* dialog)
    {
        return dialog->objectName() == objectName;
    };
    step.interact = [answer, beforeAnswer = std::move(beforeAnswer)](QDialog* dialog)
    {
        if ( beforeAnswer )
        {
            beforeAnswer();
        }
        auto* const message = qobject_cast<QMessageBox*>(dialog);
        QAbstractButton* const answerButton = message
            ? message->button(answer) : nullptr;
        if ( answerButton )
        {
            answerButton->click();
        }
        else
        {
            dialog->reject();
        }
    };
    return step;
}

ModalStep unsavedPromptStep(
    const QMessageBox::StandardButton answer,
    std::function<void()> beforeAnswer = {})
{
    ModalStep step{};
    step.matches = [](QDialog* dialog)
    {
        return dialog->objectName() == QStringLiteral("resource_authoring.unsaved_prompt");
    };
    step.interact = [answer, beforeAnswer = std::move(beforeAnswer)](QDialog* dialog)
    {
        if ( beforeAnswer )
        {
            beforeAnswer();
        }
        auto* const prompt = qobject_cast<QMessageBox*>(dialog);
        QAbstractButton* const answerButton = prompt ? prompt->button(answer) : nullptr;
        if ( answerButton )
        {
            answerButton->click();
        }
        else
        {
            dialog->reject();
        }
    };
    return step;
}

ModalStep pickerStep(
    const QString& titleFragment,
    const QString& resourceName,
    std::function<void()> beforeAccept = {},
    std::function<void()> afterAccepted = {})
{
    ModalStep step{};
    step.matches = [titleFragment](QDialog* dialog)
    {
        return dialog->windowTitle().contains(titleFragment, Qt::CaseInsensitive)
            && dialog->findChild<QListWidget*>() != nullptr;
    };
    step.ready = [](QDialog* dialog)
    {
        const QListWidget* const list = dialog->findChild<QListWidget*>();
        return list && list->isEnabled() && list->count() > 0;
    };
    step.interact = [
        resourceName,
        beforeAccept = std::move(beforeAccept),
        afterAccepted = std::move(afterAccepted)](QDialog* dialog)
    {
        auto* const list = dialog->findChild<QListWidget*>();
        if ( !list )
        {
            dialog->reject();
            return;
        }

        int selectedRow = -1;
        for ( int row = 0; row < list->count(); ++row )
        {
            const QListWidgetItem* const item = list->item(row);
            if ( item && item->text().contains(resourceName, Qt::CaseInsensitive) )
            {
                selectedRow = row;
                break;
            }
        }
        if ( selectedRow < 0 )
        {
            dialog->reject();
            return;
        }

        if ( beforeAccept )
        {
            beforeAccept();
        }
        list->setCurrentRow(selectedRow);
        auto* const buttons = dialog->findChild<QDialogButtonBox*>();
        QAbstractButton* const accept = buttons
            ? buttons->button(QDialogButtonBox::Ok)
            : nullptr;
        if ( accept && accept->isEnabled() )
        {
            if ( afterAccepted )
            {
                QObject::connect(
                    dialog,
                    &QDialog::accepted,
                    dialog,
                    [afterAccepted]()
                    {
                        afterAccepted();
                    },
                    Qt::DirectConnection);
            }
            accept->click();
        }
        else
        {
            dialog->reject();
        }
    };
    return step;
}

ModalStep projectSpritePickerStep()
{
    ModalStep step{};
    step.matches = [](QDialog* dialog)
    {
        return dialog->windowTitle().contains(QStringLiteral("Sprite"), Qt::CaseInsensitive)
            && dialog->findChild<QListWidget*>() != nullptr;
    };
    step.ready = [](QDialog* dialog)
    {
        const QListWidget* const list = dialog->findChild<QListWidget*>();
        return list && list->isEnabled() && list->count() > 0;
    };
    step.interact = [](QDialog* dialog)
    {
        auto* const list = dialog->findChild<QListWidget*>();
        auto* const buttons = dialog->findChild<QDialogButtonBox*>();
        if ( !list || !buttons )
        {
            dialog->reject();
            return;
        }
        list->setCurrentRow(0);
        QAbstractButton* const accept = buttons->button(QDialogButtonBox::Ok);
        if ( accept && accept->isEnabled() )
        {
            accept->click();
        }
        else
        {
            dialog->reject();
        }
    };
    return step;
}

QList<QTreeWidgetItem*> soundAlternativeItems(QTreeWidget* const tree)
{
    QList<QTreeWidgetItem*> alternatives{};
    if ( !tree )
    {
        return alternatives;
    }
    for ( int groupIndex = 0; groupIndex < tree->topLevelItemCount(); ++groupIndex )
    {
        QTreeWidgetItem* const group = tree->topLevelItem(groupIndex);
        for ( int childIndex = 0; group && childIndex < group->childCount(); ++childIndex )
        {
            QTreeWidgetItem* const child = group->child(childIndex);
            if ( child && child->data(0, Qt::UserRole + 20).toInt() == 1 )
            {
                alternatives.push_back(child);
            }
        }
    }
    std::sort(alternatives.begin(), alternatives.end(), [](const auto* left, const auto* right)
        {
            return left->data(0, Qt::UserRole + 21).toInt()
                < right->data(0, Qt::UserRole + 21).toInt();
        });
    return alternatives;
}

QTreeWidgetItem* soundAlternativeByWav(
    QTreeWidget* const tree,
    const QString& wav,
    const int occurrence = 0)
{
    int seen = 0;
    for ( QTreeWidgetItem* const item : soundAlternativeItems(tree) )
    {
        if ( item->text(1) == wav && seen++ == occurrence )
        {
            return item;
        }
    }
    return nullptr;
}

bool doubleClickSoundCell(QTreeWidget* const tree, QTreeWidgetItem* const item, const int column)
{
    return tree && item && QMetaObject::invokeMethod(
        tree,
        "itemDoubleClicked",
        Qt::DirectConnection,
        Q_ARG(QTreeWidgetItem*, item),
        Q_ARG(int, column));
}

void restoreSoundRows(
    QDialog* const editor,
    const std::vector<std::array<QString, 6>>& rows)
{
    if ( !editor )
    {
        return;
    }
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(editor, false);
    QCborArray rawRows{};
    for ( const std::array<QString, 6>& row : rows )
    {
        QCborArray cells{};
        for ( const QString& cell : row )
        {
            cells.append(cell);
        }
        rawRows.append(cells);
    }
    state.insert(QStringLiteral("rows"), rawRows);
    state.remove(QStringLiteral("soundPreviewLoops"));
    state.insert(QStringLiteral("collapsedModes"), QCborArray{});
    state.insert(QStringLiteral("modified"), false);
    QString warning{};
    check(ui::resources::restoreResourceAuthoringPaneState(editor, state, &warning),
        "resource authoring lifecycle fixture restores exact fixed SND row state");
}

void addSoundRows(QDialog* const editor, const QString& wavName)
{
    restoreSoundRows(editor, {
        { QStringLiteral("repeated-mode"), wavName, QStringLiteral("0x00000040"),
            QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("1") },
        { QStringLiteral("repeated-mode"), wavName, QStringLiteral("0x00000040"),
            QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("1") },
    });
}

void testPaneCaptureCountersCountRealRows()
{
    QDialog* const editor = ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::soundClass,
        {});
    check(editor != nullptr, "resource authoring lifecycle fixture creates a sound editor for row counters");
    if ( !editor )
    {
        return;
    }
    std::unique_ptr<QDialog> owner(editor);
    addSoundRows(editor, QStringLiteral("same.wav"));

    ui::resources::resetResourceAuthoringPaneTestCounters(editor);
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(editor);
    const ui::resources::ResourceAuthoringPaneTestCounters counters =
        ui::resources::resourceAuthoringPaneTestCounters(editor);
    check(!captured.isEmpty() && counters.captureCalls == 1 && counters.rowVisits == 2,
        "resource authoring pane counters report one full capture and each actual table row visit");
}

void testUnsavedCloseCancelPreservesDraft()
{
    QTemporaryDir temporaryDirectory{};
    const auto resources = makeLocator(temporaryRoot(temporaryDirectory));
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext(temporaryRoot(temporaryDirectory), resources)));
    auto* const material = editor
        ? editor->findChild<QLineEdit*>(QStringLiteral("spr.material"))
        : nullptr;
    check(temporaryDirectory.isValid() && editor && material,
        "resource authoring close-cancel fixture creates a sprite draft");
    if ( !editor || !material )
    {
        return;
    }

    material->setText(QStringLiteral("authored_before_close.mat"));
    const QCborMap before = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(editor.get()));
    bool discardedAny = true;
    BoundedModalSequence modal({ unsavedPromptStep(QMessageBox::Cancel) });
    modal.start();
    const bool closed = ui::resources::resourceAuthoringPaneConfirmClose(
        editor.get(), &discardedAny);
    modal.stop();

    check(modal.completedSteps() == 1 && !modal.timedOut(),
        "resource authoring close-cancel test observes and closes the unsaved prompt");
    check(!closed && !discardedAny
            && authoredDraftState(
                ui::resources::captureResourceAuthoringPaneState(editor.get())) == before
            && ui::resources::resourceAuthoringPaneIsModified(editor.get()),
        "canceling resource authoring close preserves exact draft state and clears the discarded output");
}

void testUnsavedNewCancelPreservesDraft()
{
    QTemporaryDir temporaryDirectory{};
    const auto resources = makeLocator(temporaryRoot(temporaryDirectory));
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext(temporaryRoot(temporaryDirectory), resources)));
    auto* const material = editor
        ? editor->findChild<QLineEdit*>(QStringLiteral("spr.material"))
        : nullptr;
    QAbstractButton* const newButton = button(editor.get(), QStringLiteral("resource_authoring.new"));
    check(temporaryDirectory.isValid() && editor && material && newButton,
        "resource authoring new-cancel fixture exposes its draft and New action");
    if ( !editor || !material || !newButton )
    {
        return;
    }

    material->setText(QStringLiteral("authored_before_new.mat"));
    const QCborMap before = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(editor.get()));
    BoundedModalSequence modal({ unsavedPromptStep(QMessageBox::Cancel) });
    modal.start();
    newButton->click();
    modal.stop();

    check(modal.completedSteps() == 1 && !modal.timedOut(),
        "resource authoring New-cancel test observes and closes the unsaved prompt");
    check(authoredDraftState(
                ui::resources::captureResourceAuthoringPaneState(editor.get())) == before
            && ui::resources::resourceAuthoringPaneIsModified(editor.get()),
        "canceling resource authoring New preserves the exact unsaved draft");
}

void testProjectOpenRejectsChangedContextAfterPrompt()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    assets::SprFile pickedSprite{};
    pickedSprite.material = "picked.mat";
    pickedSprite.width = 3.0f;
    pickedSprite.height = 2.0f;
    const std::string spriteText = assets::saveToSPRText(
        pickedSprite, assets::SprProfile::classic);
    const bool fixtureReady = temporaryDirectory.isValid()
        && writeFile(root / "misc" / "spr" / "picked.spr", spriteText)
        && writeFile(root / "mat" / "picked.mat", "MAT ");
    auto resources = makeLocator(root);
    bool projectIsCurrent = true;
    auto context = testContext(root, resources);
    context.projectContextStillCurrent = [&projectIsCurrent]()
    {
        return projectIsCurrent;
    };
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr, ui::resources::ResourceAuthoringKind::sprite, std::move(context)));
    auto* const material = editor
        ? editor->findChild<QLineEdit*>(QStringLiteral("spr.material"))
        : nullptr;
    check(fixtureReady && editor && material,
        "resource authoring stale-project fixture creates a project SPR resource and draft editor");
    if ( !fixtureReady || !editor || !material )
    {
        return;
    }

    material->setText(QStringLiteral("user_draft.mat"));
    const QCborMap before = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(editor.get()));
    BoundedModalSequence modal({
        projectSpritePickerStep(),
        unsavedPromptStep(QMessageBox::Discard, [&projectIsCurrent]()
            {
                projectIsCurrent = false;
            })
    });
    modal.start();
    (void)ui::resources::resourceAuthoringPaneOpenProjectResourceDirect(editor.get());
    modal.stop();

    check(modal.completedSteps() == 2 && !modal.timedOut(),
        "resource authoring project-open test observes the picker and subsequent discard prompt");
    check(!projectIsCurrent
            && authoredDraftState(
                ui::resources::captureResourceAuthoringPaneState(editor.get())) == before
            && material->text() == QStringLiteral("user_draft.mat")
            && ui::resources::resourceAuthoringPaneIsModified(editor.get()),
        "a project-context change during the unsaved prompt prevents the stale project resource from replacing the draft");
}

void testMaterialPickerRejectsChangedDraft()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const bool fixtureReady = temporaryDirectory.isValid() && makePickerResources(root);
    auto resources = makeLocator(root);
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext(root, resources)));
    auto* const material = editor
        ? editor->findChild<QLineEdit*>(QStringLiteral("spr.material"))
        : nullptr;
    QAbstractButton* const pick = button(editor.get(), QStringLiteral("spr.material.pick"));
    check(fixtureReady && editor && material && pick,
        "resource authoring material-picker fixture creates a candidate MAT and sprite control");
    if ( !fixtureReady || !editor || !material || !pick )
    {
        return;
    }

    material->setText(QStringLiteral("draft.mat"));
    ModalStep choose = pickerStep(
        QStringLiteral("Material Picker"),
        QStringLiteral("chosen.mat"),
        [material]()
            {
                material->setText(QStringLiteral("changed_while_picker_open.mat"));
            });
    BoundedModalSequence modal({ std::move(choose) });
    modal.start();
    pick->click();
    modal.stop();

    check(modal.completedSteps() == 1 && !modal.timedOut(),
        "resource authoring material-picker test waits for the populated picker and selects its fixed candidate");
    check(material->text() == QStringLiteral("changed_while_picker_open.mat"),
        "a material result is ignored when the authored sprite value changes while its picker is open");
}

void testMaterialPickerNoOpPreservesExactDraft()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const bool fixtureReady = temporaryDirectory.isValid() && makePickerResources(root);
    auto resources = makeLocator(root);
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext(root, resources)));
    auto* const material = editor
        ? editor->findChild<QLineEdit*>(QStringLiteral("spr.material"))
        : nullptr;
    QAbstractButton* const pick = button(editor.get(), QStringLiteral("spr.material.pick"));
    check(fixtureReady && editor && material && pick,
        "resource authoring material no-op fixture creates its picker candidate");
    if ( !fixtureReady || !editor || !material || !pick )
    {
        return;
    }

    material->setText(QStringLiteral("same.mat"));
    const QCborMap before = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(editor.get()));
    BoundedModalSequence modal({ pickerStep(
        QStringLiteral("Material Picker"), QStringLiteral("same.mat")) });
    modal.start();
    pick->click();
    modal.stop();

    check(modal.completedSteps() == 1 && !modal.timedOut(),
        "resource authoring material no-op test observes the selected existing resource");
    check(authoredDraftState(
              ui::resources::captureResourceAuthoringPaneState(editor.get())) == before,
        "choosing the already-authored material preserves exact source spelling and draft state");
}

void testWavPickerTargetsRepeatedNameRow()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const bool fixtureReady = temporaryDirectory.isValid() && makePickerResources(root);
    auto resources = makeLocator(root);
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::soundClass,
        testContext(root, resources)));
    QTreeWidget* const table = editor
        ? editor->findChild<QTreeWidget*>(QStringLiteral("snd.entries"))
        : nullptr;
    check(fixtureReady && editor && table,
        "resource authoring WAV row fixture creates repeated rows and a candidate WAV");
    if ( !fixtureReady || !editor || !table )
    {
        return;
    }

    addSoundRows(editor.get(), QStringLiteral("shared.wav"));
    QTreeWidgetItem* const target = soundAlternativeByWav(
        table, QStringLiteral("shared.wav"), 1);
    check(target != nullptr,
        "resource authoring WAV row fixture exposes its second identical source row");
    if ( !target )
    {
        return;
    }
    BoundedModalSequence modal({ pickerStep(
        QStringLiteral("Sound Picker"), QStringLiteral("chosen.wav")) });
    modal.start();
    const bool doubleClickSent = doubleClickSoundCell(table, target, 1);
    modal.stop();

    const QList<QTreeWidgetItem*> rows = soundAlternativeItems(table);
    const QString firstWav = rows.size() > 0 ? rows.at(0)->text(1) : QString{};
    const QString secondWav = rows.size() > 1 ? rows.at(1)->text(1) : QString{};
    check(doubleClickSent && modal.completedSteps() == 1 && !modal.timedOut(),
        "resource authoring WAV picker opens and selects the fixed candidate");
    check(rows.size() == 2
            && firstWav == QStringLiteral("shared.wav")
            && secondWav == QStringLiteral("chosen.wav"),
        "a WAV picker targets only the selected row when several rows repeat the same source name");
}

void testWavPickerRejectsChangedRow()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const bool fixtureReady = temporaryDirectory.isValid() && makePickerResources(root);
    auto resources = makeLocator(root);
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::soundClass,
        testContext(root, resources)));
    QTreeWidget* const table = editor
        ? editor->findChild<QTreeWidget*>(QStringLiteral("snd.entries"))
        : nullptr;
    check(fixtureReady && editor && table,
        "resource authoring stale WAV fixture creates its table and picker candidate");
    if ( !fixtureReady || !editor || !table )
    {
        return;
    }

    addSoundRows(editor.get(), QStringLiteral("shared.wav"));
    QTreeWidgetItem* const target = soundAlternativeByWav(
        table, QStringLiteral("shared.wav"), 1);
    check(target != nullptr,
        "resource authoring stale WAV fixture exposes the selected duplicate row");
    if ( !target )
    {
        return;
    }
    ModalStep choose = pickerStep(
        QStringLiteral("Sound Picker"),
        QStringLiteral("chosen.wav"),
        [target]()
            {
                target->setText(1, QStringLiteral("changed_while_picker_open.wav"));
            });
    BoundedModalSequence modal({ std::move(choose) });
    modal.start();
    const bool doubleClickSent = doubleClickSoundCell(table, target, 1);
    modal.stop();

    const QList<QTreeWidgetItem*> rows = soundAlternativeItems(table);
    const QString firstWav = rows.size() > 0 ? rows.at(0)->text(1) : QString{};
    const QString secondWav = rows.size() > 1 ? rows.at(1)->text(1) : QString{};
    check(doubleClickSent && modal.completedSteps() == 1 && !modal.timedOut(),
        "resource authoring stale WAV test observes the populated picker and accepts its candidate");
    check(firstWav == QStringLiteral("shared.wav")
            && secondWav == QStringLiteral("changed_while_picker_open.wav"),
        "a WAV picker result cannot overwrite a row whose authored contents changed while the picker was open");
}

void authorStringTableRow(
    QDialog* editor,
    const QString& key,
    const QString& value,
    const QString& number)
{
    QTableWidget* const table = editor
        ? editor->findChild<QTableWidget*>(QStringLiteral("uni.entries"))
        : nullptr;
    QAbstractButton* const add = button(editor, QStringLiteral("uni.add"));
    if ( !table || !add )
    {
        return;
    }
    add->click();
    if ( table->rowCount() < 1 )
    {
        return;
    }
    table->item(0, 0)->setText(key);
    table->item(0, 1)->setText(value);
    table->item(0, 2)->setText(number);
}

void testOversizedValidUniOpenPreservesDirtyDraft()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const std::filesystem::path oversized = root / "oversized.uni";
    const bool fixtureReady = temporaryDirectory.isValid()
        && writeOversizedValidUni(oversized);
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext(root, makeLocator(root))));
    QTableWidget* const table = editor
        ? editor->findChild<QTableWidget*>(QStringLiteral("uni.entries"))
        : nullptr;
    QAbstractButton* const open = button(
        editor.get(), QStringLiteral("resource_authoring.open"));
    check(fixtureReady && editor && table && open,
        "oversized UNI fixture creates a valid over-budget source and string-table draft editor");
    if ( !fixtureReady || !editor || !table || !open )
    {
        return;
    }

    authorStringTableRow(
        editor.get(),
        QStringLiteral("KEEP"),
        QStringLiteral("exact draft"),
        QStringLiteral("7"));
    const QCborMap before = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(editor.get()));
    const bool draftIsDirty = ui::resources::resourceAuthoringPaneIsModified(editor.get());
    bool budgetErrorShown = false;
    QString budgetErrorTitle{};
    QString budgetErrorText{};
    bool unexpectedUnsavedPrompt = false;
    ModalStep openOutcome{};
    openOutcome.matches = [](QDialog* dialog)
    {
        return dialog->objectName() == QStringLiteral("resource_authoring.error")
            || dialog->objectName() == QStringLiteral("resource_authoring.unsaved_prompt");
    };
    openOutcome.interact = [
        &budgetErrorShown,
        &budgetErrorTitle,
        &budgetErrorText,
        &unexpectedUnsavedPrompt](QDialog* dialog)
    {
        if ( auto* const message = qobject_cast<QMessageBox*>(dialog) )
        {
            if ( dialog->objectName() == QStringLiteral("resource_authoring.error") )
            {
                budgetErrorTitle = message->windowTitle();
                budgetErrorText = message->text();
                budgetErrorShown = true;
                if ( QAbstractButton* const ok = message->button(QMessageBox::Ok) )
                {
                    ok->click();
                    return;
                }
            }
            else
            {
                unexpectedUnsavedPrompt = true;
                if ( QAbstractButton* const cancel = message->button(QMessageBox::Cancel) )
                {
                    cancel->click();
                    return;
                }
            }
        }
        dialog->reject();
    };
    BoundedModalSequence modal({ fileDialogStep(oversized), std::move(openOutcome) });
    modal.start();
    open->click();
    modal.stop();

    const QString expectedBudgetError = QStringLiteral("Resource exceeds byte budget: %1")
        .arg(QDir::fromNativeSeparators(QString::fromStdWString(oversized.wstring())));
    const bool budgetExpectationMet = draftIsDirty
        && modal.completedSteps() == 2
        && !modal.timedOut()
        && budgetErrorShown
        && budgetErrorTitle == QStringLiteral("Could not open resource")
        && budgetErrorText == expectedBudgetError
        && !unexpectedUnsavedPrompt;
    if ( !budgetExpectationMet )
    {
        std::cerr
            << "Oversized UNI diagnostic: actual title=[" << budgetErrorTitle.toStdString()
            << "] actual detail=[" << budgetErrorText.toStdString()
            << "] expected title=[Could not open resource] expected detail=["
            << expectedBudgetError.toStdString()
            << "] completedSteps=" << modal.completedSteps()
            << " timedOut=" << modal.timedOut()
            << " dirty=" << draftIsDirty
            << " errorDialog=" << budgetErrorShown
            << " unexpectedUnsavedPrompt=" << unexpectedUnsavedPrompt << '\n';
    }
    check(draftIsDirty && modal.completedSteps() == 2 && !modal.timedOut()
            && budgetErrorShown
            && budgetErrorTitle == QStringLiteral("Could not open resource")
            && budgetErrorText == expectedBudgetError
            && !unexpectedUnsavedPrompt,
        "Open rejects a syntactically valid UNI whose comment bytes exceed the resource budget before prompting to replace a dirty draft");
    check(authoredDraftState(
              ui::resources::captureResourceAuthoringPaneState(editor.get())) == before
            && ui::resources::resourceAuthoringPaneIsModified(editor.get()),
        "rejecting an over-budget UNI preserves the exact existing raw draft and dirty state");
}

void testProjectOpenCatchesTruncatedWinningArchiveRead()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const std::filesystem::path gobPath = root / "truncated.gob";
    constexpr std::string_view entryName = "misc/truncated.uni";
    constexpr std::string_view archiveBytes =
        "MSGS 1\n\n\"KEY\" 0 \"archive value\"\n\nEND\n";
    const bool fixtureReady = temporaryDirectory.isValid()
        && writeSingleEntryGob(gobPath, entryName, archiveBytes);
    auto resources = makeLocator(root);
    if ( fixtureReady )
    {
        try
        {
            resources->mountGob(gobPath);
        }
        catch ( const std::exception& )
        {
        }
    }
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext(root, resources)));
    QTableWidget* const table = editor
        ? editor->findChild<QTableWidget*>(QStringLiteral("uni.entries"))
        : nullptr;
    check(fixtureReady && resources && resources->gobs().size() == 1
            && editor && table,
        "truncated project archive fixture mounts a valid UNI entry and creates a string-table editor");
    if ( !fixtureReady || !resources || resources->gobs().empty()
        || !editor || !table )
    {
        return;
    }

    authorStringTableRow(
        editor.get(),
        QStringLiteral("KEEP"),
        QStringLiteral("exact dirty draft"),
        QStringLiteral("11"));
    const QCborMap before = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(editor.get()));
    const bool draftIsDirty = ui::resources::resourceAuthoringPaneIsModified(editor.get());
    bool archiveWasTruncatedAfterPickerPopulation = false;
    bool errorDialogShown = false;
    QString errorTitle{};
    QString errorText{};
    ModalStep errorOutcome{};
    errorOutcome.matches = [](QDialog* dialog)
    {
        return dialog->objectName() == QStringLiteral("resource_authoring.error");
    };
    errorOutcome.interact = [&errorDialogShown, &errorTitle, &errorText](QDialog* dialog)
    {
        if ( auto* const message = qobject_cast<QMessageBox*>(dialog) )
        {
            errorDialogShown = true;
            errorTitle = message->windowTitle();
            errorText = message->text();
            if ( QAbstractButton* const ok = message->button(QMessageBox::Ok) )
            {
                ok->click();
                return;
            }
        }
        dialog->reject();
    };
    ModalStep chooseEntry = pickerStep(
        QStringLiteral("String Table Picker"),
        QStringLiteral("truncated.uni"),
        {},
        [&gobPath, &archiveWasTruncatedAfterPickerPopulation]()
        {
            archiveWasTruncatedAfterPickerPopulation = writeFile(gobPath, "GOB ");
        });
    BoundedModalSequence modal({ std::move(chooseEntry), std::move(errorOutcome) });
    modal.start();
    bool escapedException = false;
    try
    {
        (void)ui::resources::resourceAuthoringPaneOpenProjectResourceDirect(editor.get());
    }
    catch ( const std::exception& )
    {
        escapedException = true;
    }
    modal.stop();

    const QString expectedErrorTitle = QStringLiteral("Could not open project resource");
    const QString expectedErrorText = QStringLiteral(
        "Unexpected EOF while reading GOB entry: misc/truncated.uni");
    const bool truncatedArchiveExpectationMet = draftIsDirty
        && archiveWasTruncatedAfterPickerPopulation
        && modal.completedSteps() == 2
        && !modal.timedOut()
        && !escapedException
        && errorDialogShown
        && errorTitle == expectedErrorTitle
        && errorText == expectedErrorText;
    if ( !truncatedArchiveExpectationMet )
    {
        std::cerr
            << "Truncated GOB diagnostic: actual title=[" << errorTitle.toStdString()
            << "] actual detail=[" << errorText.toStdString()
            << "] expected title=[" << expectedErrorTitle.toStdString()
            << "] expected detail=[" << expectedErrorText.toStdString()
            << "] completedSteps=" << modal.completedSteps()
            << " timedOut=" << modal.timedOut()
            << " dirty=" << draftIsDirty
            << " errorDialog=" << errorDialogShown
            << " archiveTruncated=" << archiveWasTruncatedAfterPickerPopulation
            << " exceptionEscaped=" << escapedException << '\n';
    }
    check(draftIsDirty && archiveWasTruncatedAfterPickerPopulation
            && modal.completedSteps() == 2 && !modal.timedOut()
            && !escapedException && errorDialogShown
            && errorTitle == expectedErrorTitle
            && errorText == expectedErrorText,
        "project Open reports a truncated selected archive through its error dialog instead of letting the read exception escape the click handler");
    check(authoredDraftState(
              ui::resources::captureResourceAuthoringPaneState(editor.get())) == before
            && ui::resources::resourceAuthoringPaneIsModified(editor.get()),
        "a failed truncated-archive project Open preserves the exact existing raw draft and dirty state");
}

void testOpenParsesAndHashesOneReadSnapshot()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const std::filesystem::path selected = root / "snapshot.uni";
    constexpr std::string_view beforeBytes =
        "MSGS 1\n\n\"KEY\" 0 \"before read\"\n\nEND\n";
    constexpr std::string_view afterBytes =
        "MSGS 1\n\n\"KEY\" 0 \"after read\"\n\nEND\n";
    const bool fixtureReady = temporaryDirectory.isValid()
        && writeFile(selected, beforeBytes);
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext(root, makeLocator(root))));
    QAbstractButton* const open = button(
        editor.get(), QStringLiteral("resource_authoring.open"));
    check(fixtureReady && editor && open,
        "single-read snapshot fixture creates a loose valid UNI and clean string-table editor");
    if ( !fixtureReady || !editor || !open )
    {
        return;
    }

    bool diskChangedAfterRead = false;
    ui::resources::setResourceAuthoringPaneAfterReadHook(
        editor.get(),
        [&selected, &diskChangedAfterRead, afterBytes](
            const std::filesystem::path& readPath)
        {
            if ( readPath == selected )
            {
                diskChangedAfterRead = writeFile(selected, afterBytes);
            }
        });
    BoundedModalSequence openModal({ fileDialogStep(selected) });
    openModal.start();
    open->click();
    openModal.stop();

    QTableWidget* const table = editor->findChild<QTableWidget*>(
        QStringLiteral("uni.entries"));
    const QString loadedValue = table && table->item(0, 1)
        ? table->item(0, 1)->text() : QString{};
    bool saveResult = true;
    BoundedModalSequence saveModal({ messageBoxStep(
        QStringLiteral("resource_authoring.external_change_prompt"),
        QMessageBox::Cancel) });
    saveModal.start();
    saveResult = ui::resources::resourceAuthoringPaneSave(editor.get(), false);
    saveModal.stop();
    const std::optional<std::string> diskAfterSave = readFile(selected);

    check(diskChangedAfterRead && openModal.completedSteps() == 1
            && !openModal.timedOut() && loadedValue == QStringLiteral("before read"),
        "Open parses the immutable byte snapshot read before the selected path changes");
    check(saveModal.completedSteps() == 1 && !saveModal.timedOut() && !saveResult
            && diskAfterSave && *diskAfterSave == afterBytes
            && loadedValue == QStringLiteral("before read"),
        "the disk baseline hashes the same bytes that were parsed, so a later Save detects and preserves the changed file");
}

void testProjectOpenUsesWinningArchiveSourceForIdentity()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const std::filesystem::path gobPath = root / "fixture.gob";
    const std::filesystem::path laterDiskAlias = root
        / "ui" / "precedence.uni";
    constexpr std::string_view archiveBytes =
        "MSGS 1\n\n\"KEY\" 0 \"archive winner\"\n\nEND\n";
    constexpr std::string_view diskBytes =
        "MSGS 1\n\n\"KEY\" 0 \"later disk alias\"\n\nEND\n";
    const bool fixtureReady = temporaryDirectory.isValid()
        && writeSingleEntryGob(gobPath, "misc/precedence.uni", archiveBytes)
        && writeFile(laterDiskAlias, diskBytes);
    auto resources = makeLocator(root);
    if ( fixtureReady )
    {
        try
        {
            resources->mountGob(gobPath);
        }
        catch ( const std::exception& )
        {
        }
    }
    auto context = testContext(root, resources);
    context.destinationAvailable = [](QWidget*, const std::filesystem::path&)
    {
        return false;
    };
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        std::move(context)));
    check(fixtureReady && resources && resources->gobs().size() == 1
            && editor,
        "project-source precedence fixture mounts an archive plus a later candidate-path disk alias");
    if ( !fixtureReady || !editor || resources->gobs().empty() )
    {
        return;
    }

    BoundedModalSequence modal({ pickerStep(
        QStringLiteral("String Table Picker"),
        QStringLiteral("precedence.uni")) });
    modal.start();
    (void)ui::resources::resourceAuthoringPaneOpenProjectResourceDirect(editor.get());
    modal.stop();

    QTableWidget* const table = editor->findChild<QTableWidget*>(
        QStringLiteral("uni.entries"));
    const QString openedValue = table && table->item(0, 1)
        ? table->item(0, 1)->text() : QString{};
    const QString identity = ui::resources::resourceAuthoringPaneIdentity(editor.get());
    const QString loosePath =
        ui::resources::resourceAuthoringPaneLooseFilePath(editor.get());
    const QString archiveTooltip = QStringLiteral("fixture.gob: misc/precedence.uni");
    const bool exactWinningTooltip = editor
        && editor->property("resource_authoring.sourceTooltip").toString() == archiveTooltip;
    if ( resources )
    {
        resources->clearGobs();
    }
    const bool failedSave = editor && !ui::resources::resourceAuthoringPaneSave(editor.get(), false);
    check(modal.completedSteps() == 1 && !modal.timedOut()
            && openedValue == QStringLiteral("archive winner"),
        "project Open keeps the archive bytes selected by candidate-before-root lookup when a later disk alias exists");
    check(loosePath.isEmpty()
            && identity == QStringLiteral("project:precedence.uni")
            && exactWinningTooltip,
        "project Open classifies its source and archive tooltip from the winning candidate-before-root lookup result");
    check(failedSave && editor
            && editor->property("resource_authoring.sourceTooltip").toString() == archiveTooltip
            && ui::resources::resourceAuthoringPaneIdentity(editor.get()) == identity
            && !std::filesystem::exists(root / "misc" / "precedence.uni"),
        "a denied loose Save keeps the cached winning archive tooltip and project identity after the mount is removed");
}

void testArchiveBackedResourceTabTooltipsIdentifyMountedSource()
{
    assets::SprFile sprite{};
    sprite.material = "archive.mat";
    sprite.width = 2.0f;
    sprite.height = 1.0f;
    assets::SoundClassFile sound{};
    sound.entries.push_back(assets::SoundClassEntry{
        "moving", "Foo.WAV", 0x40U, 0.5f, 2.5f, 1.0f });
    struct Fixture
    {
        QString action{};
        QString tabs{};
        QString dock{};
        QString picker{};
        ui::resources::ResourceAuthoringKind kind{};
        QString selectedName{};
        std::string entryName{};
        std::string bytes{};
    };
    const std::array<Fixture, 3> fixtures{
        Fixture{ QStringLiteral("tools.resource_editors.sprite_spr.new"),
            QStringLiteral("resourceEditorTabs"), QStringLiteral("resourceEditorsDock"),
            QStringLiteral("Sprite Picker"), ui::resources::ResourceAuthoringKind::sprite,
            QStringLiteral("Foo.SPR"), "misc/spr/Foo.SPR",
            assets::saveToSPRText(sprite, assets::SprProfile::classic) },
        Fixture{ QStringLiteral("tools.resource_editors.sound_class_snd.new"),
            QStringLiteral("soundClassResourceEditorTabs"), QStringLiteral("soundClassResourceEditorsDock"),
            QStringLiteral("Sound Class Picker"), ui::resources::ResourceAuthoringKind::soundClass,
            QStringLiteral("Foo.SND"), "misc/snd/Foo.SND",
            assets::saveToSoundClassText(sound, assets::SoundClassProfile::classic) },
        Fixture{ QStringLiteral("tools.resource_editors.string_table_uni.new"),
            QStringLiteral("uniResourceEditorTabs"), QStringLiteral("uniResourceEditorsDock"),
            QStringLiteral("String Table Picker"), ui::resources::ResourceAuthoringKind::stringTable,
            QStringLiteral("Foo.UNI"), "misc/Foo.UNI",
            "MSGS 1\n\n\"KEY\" 0 \"archive value\"\n\nEND\n" } };

    for ( const Fixture& fixture : fixtures )
    {
        QTemporaryDir temporary{};
        const std::filesystem::path root = temporaryRoot(temporary);
        const QString archiveName = fixture.kind == ui::resources::ResourceAuthoringKind::stringTable
            ? QStringLiteral("test.goo") : QStringLiteral("test.gob");
        const std::filesystem::path archive = root / archiveName.toStdString();
        const bool fixtureReady = temporary.isValid()
            && writeSingleEntryGob(archive, fixture.entryName, fixture.bytes);
        MainWindow window(nullptr, true);
        window.setAnimated(false);
        window.setProjectTypeForTest(ProjectType::JKDF2);
        window.setCurrentPathForTitleForTest(root / "project.sed");
        window.setResourcePathsForTest({ root }, { archive }, root / "project.sed");
        window.resize(1280, 860);
        window.show();
        window.activateWindow();
        QAction* const create = window.findChild<QAction*>(fixture.action);
        if ( create )
        {
            createResourceDraftFromGroup(create);
        }
        QTabWidget* const tabs = window.findChild<QTabWidget*>(fixture.tabs);
        QDockWidget* const dock = window.findChild<QDockWidget*>(fixture.dock);
        QToolButton* const open = dock
            ? dock->findChild<QToolButton*>(QStringLiteral("resource_authoring.open")) : nullptr;
        check(fixtureReady && create && tabs && open,
            "archive tooltip fixture creates a real mounted archive and resource group");
        if ( !fixtureReady || !create || !tabs || !open )
        {
            continue;
        }

        BoundedModalSequence modal({ pickerStep(fixture.picker, fixture.selectedName) });
        modal.start();
        open->click();
        modal.stop();
        const int index = tabs->currentIndex();
        QWidget* const page = index >= 0 ? tabs->widget(index) : nullptr;
        const QString expectedTooltip = archiveName + QStringLiteral(": ")
            + QString::fromStdString(fixture.entryName);
        const QString identity = page
            ? ui::resources::resourceAuthoringPaneIdentity(page) : QString{};
        const QCborMap sourceState = page
            ? ui::resources::captureResourceAuthoringPaneState(page) : QCborMap{};
        auto unmountedResources = makeLocator(root);
        std::unique_ptr<QWidget> restoredPane(ui::resources::createResourceAuthoringPane(
            nullptr, fixture.kind, testContext(root, unmountedResources)));
        QString restoreWarning{};
        const bool restoredWithoutMount = restoredPane
            && ui::resources::restoreResourceAuthoringPaneState(
                restoredPane.get(), sourceState, &restoreWarning);
        check(modal.completedSteps() == 1 && !modal.timedOut()
                && tabs->count() == 2 && page
                && tabs->tabToolTip(index) == expectedTooltip
                && identity == QStringLiteral("project:") + fixture.selectedName.toCaseFolded(),
            "an archive-backed SPR, SND, or UNI tab tooltip identifies the exact mounted archive basename and case-preserved entry without changing project identity");
        check(restoredWithoutMount && restoreWarning.isEmpty()
                && restoredPane->property("resource_authoring.sourceTooltip").toString() == expectedTooltip
                && ui::resources::resourceAuthoringPaneIdentity(restoredPane.get()) == identity,
            "resource archive tooltip recovery keeps the recorded origin when the destination editor has no mounted archive");

        ModalStep cancelSaveAs{};
        cancelSaveAs.matches = [](QDialog* dialog)
        {
            return dialog->objectName() == QStringLiteral("resource_authoring.file_dialog")
                && qobject_cast<QFileDialog*>(dialog);
        };
        cancelSaveAs.interact = [](QDialog* dialog) { dialog->reject(); };
        BoundedModalSequence cancelSaveAsModal({ std::move(cancelSaveAs) }, 2000);
        cancelSaveAsModal.start();
        const bool canceledSave = page
            && !ui::resources::resourceAuthoringPaneSave(page, true);
        cancelSaveAsModal.stop();
        check(canceledSave && cancelSaveAsModal.completedSteps() == 1
                && !cancelSaveAsModal.timedOut() && tabs->indexOf(page) >= 0
                && tabs->tabToolTip(tabs->indexOf(page)) == expectedTooltip,
            "canceling Save As preserves the archive source tooltip on the live resource tab");

        const std::filesystem::path saveDirectory = root / "misc"
            / (fixture.kind == ui::resources::ResourceAuthoringKind::sprite ? "spr"
                : fixture.kind == ui::resources::ResourceAuthoringKind::soundClass ? "snd" : "");
        const std::filesystem::path expectedLoosePath = saveDirectory / fixture.selectedName.toStdString();
        const bool savedLoose = page && ui::resources::resourceAuthoringPaneSave(page, false);
        const QString expectedLooseTooltip = QDir::cleanPath(
            QString::fromStdWString(expectedLoosePath.wstring()));
        check(savedLoose && page
                && QDir::cleanPath(page->property("resource_authoring.sourceTooltip").toString())
                    == expectedLooseTooltip
                && tabs->indexOf(page) >= 0
                && QDir::cleanPath(tabs->tabToolTip(tabs->indexOf(page))) == expectedLooseTooltip
                && ui::resources::resourceAuthoringPaneIdentity(page).startsWith(QStringLiteral("loose:")),
            "a successful archive-resource Save changes the tooltip to its full loose path and retains the normal loose source identity");
    }
}

void testSaveAsRechecksDestinationAfterOverwritePrompt()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const std::filesystem::path destination = root
        / "misc" / "spr" / "claimed.spr";
    constexpr std::string_view sentinel = "destination must remain unchanged";
    const bool fixtureReady = temporaryDirectory.isValid()
        && writeFile(destination, sentinel);
    const auto resources = makeLocator(root);
    auto context = testContext(root, resources);
    bool destinationClaimed = false;
    int ownershipChecks = 0;
    context.destinationAvailable = [&destinationClaimed, &ownershipChecks](
        QWidget*, const std::filesystem::path&)
    {
        ++ownershipChecks;
        return !destinationClaimed;
    };
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        std::move(context)));
    check(fixtureReady && editor,
        "Save As destination-race fixture creates an existing sentinel file and sprite editor");
    if ( !fixtureReady || !editor )
    {
        return;
    }

    BoundedModalSequence modal({
        fileDialogStep(destination, true),
        messageBoxStep(
            QStringLiteral("resource_authoring.overwrite_prompt"),
            QMessageBox::Yes,
            [&destinationClaimed]()
            {
                destinationClaimed = true;
            }) });
    modal.start();
    const bool saved = ui::resources::resourceAuthoringPaneSave(editor.get(), true);
    modal.stop();
    const std::optional<std::string> finalBytes = readFile(destination);

    check(modal.completedSteps() == 2 && !modal.timedOut()
            && destinationClaimed && !saved,
        "Save As rechecks destination ownership after the overwrite prompt changes it");
    check(ownershipChecks >= 2 && finalBytes && *finalBytes == sentinel,
        "Save As leaves the existing file untouched when another resource editor claims it during confirmation");
}

void testPaneDeletionDuringUnsavedPrompt()
{
    QTemporaryDir temporaryDirectory{};
    auto resources = makeLocator(temporaryRoot(temporaryDirectory));
    QDialog* const editor = ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext(temporaryRoot(temporaryDirectory), resources));
    auto* const material = editor
        ? editor->findChild<QLineEdit*>(QStringLiteral("spr.material"))
        : nullptr;
    check(temporaryDirectory.isValid() && editor && material,
        "resource authoring destruction fixture creates a sprite draft");
    if ( !editor || !material )
    {
        delete editor;
        return;
    }

    material->setText(QStringLiteral("delete_during_prompt.mat"));
    QPointer<QDialog> owner(editor);
    ModalStep destroyOwner{};
    destroyOwner.matches = [](QDialog* dialog)
    {
        return dialog->objectName() == QStringLiteral("resource_authoring.unsaved_prompt");
    };
    destroyOwner.interact = [owner](QDialog*)
    {
        if ( owner )
        {
            owner->deleteLater();
        }
    };
    BoundedModalSequence modal({ std::move(destroyOwner) }, 2000);
    modal.start();
    (void)ui::resources::resourceAuthoringPaneConfirmClose(editor);
    modal.stop();

    check(modal.completedSteps() == 1 && !modal.timedOut() && owner.isNull(),
        "destroying a resource editor while its unsaved prompt is modal returns safely without retaining the pane");
}

void testSpritePaneUsesProjectProfileAndFocusedControls()
{
    struct ProfileCase
    {
        ProjectType projectType;
        assets::SprProfile profile;
        std::array<QString, 4> lightLabels;
    };

    const std::array<ProfileCase, 3> cases{{
        {
            ProjectType::JKDF2,
            assets::SprProfile::classic,
            { QStringLiteral("Fully lit"), QStringLiteral("Not lit"),
                QStringLiteral("Diffuse"), QStringLiteral("Gouraud") }
        },
        {
            ProjectType::MOTS,
            assets::SprProfile::classic,
            { QStringLiteral("Fully lit"), QStringLiteral("Not lit"),
                QStringLiteral("Diffuse"), QStringLiteral("Gouraud") }
        },
        {
            ProjectType::IJIM,
            assets::SprProfile::ijim,
            { QStringLiteral("None"), QStringLiteral("Lit"),
                QStringLiteral("Diffuse"), QStringLiteral("Gouraud") }
        }
    }};

    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const auto resources = makeLocator(root);
    check(temporaryDirectory.isValid() && resources,
        "SPR project-profile fixture creates isolated resource roots");
    if ( !temporaryDirectory.isValid() || !resources )
    {
        return;
    }

    for ( const ProfileCase& profileCase : cases )
    {
        QWidget host{};
        auto* const layout = new QVBoxLayout(&host);
        host.resize(1200, 900);

        auto context = testContext(root, resources);
        context.projectType = profileCase.projectType;
        QWidget* const pane = ui::resources::createResourceAuthoringPane(
            &host,
            ui::resources::ResourceAuthoringKind::sprite,
            std::move(context));
        if ( pane )
        {
            layout->addWidget(pane);
            pane->show();
            host.show();
            layout->activate();
            QApplication::processEvents();
        }

        check(pane != nullptr,
            "SPR pane can be created for each supported project profile");
        if ( !pane )
        {
            continue;
        }

        const QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane);
        check(state.value(QStringLiteral("profile")).toInteger()
                == static_cast<int>(profileCase.profile),
            "SPR pane captures the profile selected by its owning project");
        check(pane->findChild<QComboBox*>(QStringLiteral("spr.profile")) == nullptr,
            "SPR pane does not expose a profile dropdown that can disagree with its project");

        auto* const width = pane->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("spr.width"));
        auto* const height = pane->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("spr.height"));
        check(width && height,
            "SPR width and height use scrub-float controls");
        if ( width && height )
        {
            width->setValue(-1.0);
            height->setValue(-1.0);
            check(width->value() >= 0.0 && height->value() >= 0.0,
                "SPR width and height controls reject negative dimensions");
        }

        std::array<ui::ScrubFloatEdit*, 3> offsets{
            pane->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.offset.x")),
            pane->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.offset.y")),
            pane->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.offset.z")) };
        const bool allOffsetsPresent = std::all_of(
            offsets.begin(), offsets.end(), [](const auto* control)
            {
                return control != nullptr;
            });
        check(allOffsetsPresent,
            "SPR offset X, Y, and Z use scrub-float controls");
        if ( allOffsetsPresent )
        {
            const int x = offsets[0]->mapTo(pane, QPoint{}).y();
            const int y = offsets[1]->mapTo(pane, QPoint{}).y();
            const int z = offsets[2]->mapTo(pane, QPoint{}).y();
            check(x < y && y < z,
                "SPR offset X, Y, and Z controls are stacked vertically in axis order");
        }

        auto* const lightMode = pane->findChild<QComboBox*>(
            QStringLiteral("spr.light_mode"));
        bool exactLightModes = lightMode && lightMode->count() == 4;
        for ( int index = 0; exactLightModes && index < 4; ++index )
        {
            exactLightModes = lightMode->itemData(index).toInt() == index
                && lightMode->itemText(index) == profileCase.lightLabels[
                    static_cast<std::size_t>(index)];
        }
        check(exactLightModes,
            "SPR light mode choices match the exact four-value vector for the project profile");

        QAbstractButton* const bounds = pane->findChild<QAbstractButton*>(
            QStringLiteral("spr.preview.bounds"));
        QSlider* const fps = pane->findChild<QSlider*>(
            QStringLiteral("spr.preview.fps"));
        QToolButton* const pause = pane->findChild<QToolButton*>(
            QStringLiteral("spr.preview.pause"));
        QToolButton* const stop = pane->findChild<QToolButton*>(
            QStringLiteral("spr.preview.stop"));
        QAbstractButton* const recenter = button(
            pane, QStringLiteral("spr.preview.recenter"));
        QToolButton* const materialPick = pane->findChild<QToolButton*>(
            QStringLiteral("spr.material.pick"));
        check(bounds && bounds->isCheckable(),
            "SPR preview exposes a checkable bounds toggle");
        check(fps && fps->minimum() == 0 && fps->maximum() == 99,
            "SPR preview FPS slider accepts the exact 0 through 99 range");
        check(pause && pause->isCheckable() && stop,
            "SPR preview exposes separate Pause and Stop tool controls");
        check(recenter != nullptr,
            "SPR preview exposes a recenter action");
        check(materialPick != nullptr,
            "SPR material picker uses a narrow ellipsis tool button");

        if ( profileCase.projectType == ProjectType::IJIM )
        {
            ui::ScrubFloatEdit* const scalar = pane->findChild<ui::ScrubFloatEdit*>(
                QStringLiteral("spr.extra_light.scalar"));
            QFormLayout* const form = pane->findChild<QFormLayout*>();
            QLabel* const scalarLabel = form && scalar
                ? qobject_cast<QLabel*>(form->labelForField(scalar->parentWidget()))
                : nullptr;
            check(scalar && scalar->parentWidget()
                    && scalar->parentWidget()->isHidden()
                    && scalarLabel && scalarLabel->isHidden(),
                "IJIM hides both the classic scalar field and its form label");
        }
    }
}

void testSpriteAnimationControlsAndPreviewPicking()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const auto resources = makeLocator(root);
    const std::vector<QColor> redPixels(4, QColor(240, 24, 32));
    const std::vector<QColor> bluePixels(6, QColor(20, 40, 240));
    const QByteArray animatedMat = makeRgbaMatCels({
        { 2, redPixels },
        { 3, bluePixels },
    });
    const QByteArray staticMat = makeRgbaMatCels({ { 2, redPixels } });
    const bool fixturesReady = temporaryDirectory.isValid()
        && writeFile(root / "mat" / "animated.mat",
            std::string_view(animatedMat.constData(), static_cast<std::size_t>(animatedMat.size())))
        && writeFile(root / "mat" / "static.mat",
            std::string_view(staticMat.constData(), static_cast<std::size_t>(staticMat.size())))
        && writeFile(root / "mat" / "invalid.mat", "MAT ");
    check(fixturesReady && resources,
        "SPR animation fixture writes exact two-cel and one-cel RGBA MAT resources");
    if ( !fixturesReady || !resources )
    {
        return;
    }

    QWidget host{};
    auto* const layout = new QVBoxLayout(&host);
    host.resize(1200, 840);
    auto context = testContext(root, resources);
    QWidget* const pane = ui::resources::createResourceAuthoringPane(
        &host,
        ui::resources::ResourceAuthoringKind::sprite,
        std::move(context));
    if ( pane )
    {
        layout->addWidget(pane);
    }
    check(pane != nullptr,
        "SPR animation fixture creates its embedded resource pane");
    if ( !pane )
    {
        return;
    }

    const auto setMaterialInCleanDraft = [pane](const QString& name)
    {
        QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane, false);
        state.remove(QStringLiteral("editorPresentation"));
        QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
        if ( sprite.size() != 15 )
        {
            return false;
        }
        sprite[0] = name;
        sprite[5] = 0;
        state.insert(QStringLiteral("sprite"), sprite);
        state.insert(QStringLiteral("modified"), false);
        QString warning{};
        return ui::resources::restoreResourceAuthoringPaneState(pane, state, &warning);
    };
    check(setMaterialInCleanDraft(QStringLiteral("animated.mat")),
        "SPR animation fixture installs a clean authored MAT reference through bounded pane state");

    auto* const material = pane->findChild<QLineEdit*>(QStringLiteral("spr.material"));
    QToolButton* const ellipsis = pane->findChild<QToolButton*>(
        QStringLiteral("spr.material.pick"));
    QSlider* const fps = pane->findChild<QSlider*>(QStringLiteral("spr.preview.fps"));
    QToolButton* const pause = pane->findChild<QToolButton*>(QStringLiteral("spr.preview.pause"));
    QToolButton* const stop = pane->findChild<QToolButton*>(QStringLiteral("spr.preview.stop"));
    GlView* const preview = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    check(material && ellipsis && fps && pause && stop && preview,
        "SPR animation pane exposes material, ellipsis, FPS, Pause, Stop, and isolated preview controls");
    if ( !material || !ellipsis || !fps || !pause || !stop || !preview )
    {
        return;
    }

    fps->setValue(0);
    pane->show();
    host.show();
    layout->activate();
    QElapsedTimer renderDeadline{};
    renderDeadline.start();
    QImage firstFrame{};
    while ( renderDeadline.elapsed() < 3000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
        if ( preview->isValid() )
        {
            firstFrame = preview->grabFramebuffer();
            if ( imageHasDominantPixels(firstFrame, false)
                && spriteInteriorMatchesTexel(*preview, firstFrame, false) )
            {
                break;
            }
        }
    }
    const bool firstCelRendered = imageHasDominantPixels(firstFrame, false)
        && spriteInteriorMatchesTexel(*preview, firstFrame, false);
    check(firstCelRendered,
        "SPR animation preview renders the exact red first cel from the synthetic MAT");
    if ( !firstCelRendered )
    {
        return;
    }

    QImage blueTexture(3, 2, QImage::Format_RGBA8888);
    blueTexture.fill(QColor(20, 40, 240));
    const std::string previewColormap = preview->property(
        "resource_authoring.preview_cmp").toString().toStdString();
    const auto* const attachedPreviewLevel = preview->attachedLevelForTest();
    const bool rejectedNullTexture = !preview->updatePreviewMaterialTexture(
        "animated.mat", previewColormap, QImage{}, 8);
    const bool rejectedEmptyMaterial = !preview->updatePreviewMaterialTexture(
        "", previewColormap, blueTexture, 8);
    preview->setProperty("spr.preview.isolated_resource", false);
    const bool rejectedNonIsolatedTexture = !preview->updatePreviewMaterialTexture(
        "animated.mat", previewColormap, blueTexture, 8);
    preview->setProperty("spr.preview.isolated_resource", true);
    const QImage afterRejectedTexture = preview->grabFramebuffer();
    check(rejectedNullTexture && rejectedEmptyMaterial && rejectedNonIsolatedTexture
            && afterRejectedTexture == firstFrame,
        "SPR preview rejects null, empty-name, and non-isolated texture updates without changing rendered pixels");

    const bool acceptedTextureUpdate = preview->updatePreviewMaterialTexture(
        "animated.mat", previewColormap, blueTexture, 8);
    QImage directlyUpdatedFrame{};
    QElapsedTimer directUpdateDeadline{};
    directUpdateDeadline.start();
    while ( directUpdateDeadline.elapsed() < 1200 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
        directlyUpdatedFrame = preview->grabFramebuffer();
        if ( imageHasDominantPixels(directlyUpdatedFrame, true)
            && spriteInteriorMatchesTexel(*preview, directlyUpdatedFrame, true) )
        {
            break;
        }
    }
    check(acceptedTextureUpdate && attachedPreviewLevel == preview->attachedLevelForTest()
            && imageHasDominantPixels(directlyUpdatedFrame, true)
            && spriteInteriorMatchesTexel(*preview, directlyUpdatedFrame, true),
        "SPR preview replaces a valid cel texture without rebuilding the private preview Level");
    const QImage redTexture(2, 2, QImage::Format_RGBA8888);
    QImage resetRedTexture = redTexture;
    resetRedTexture.fill(QColor(240, 24, 32));
    const bool restoredFirstTexture = preview->updatePreviewMaterialTexture(
        "animated.mat", previewColormap, resetRedTexture, 8);
    QElapsedTimer resetTextureDeadline{};
    resetTextureDeadline.start();
    while ( resetTextureDeadline.elapsed() < 1200 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
        firstFrame = preview->grabFramebuffer();
        if ( imageHasDominantPixels(firstFrame, false)
            && spriteInteriorMatchesTexel(*preview, firstFrame, false) )
        {
            break;
        }
    }
    check(restoredFirstTexture && imageHasDominantPixels(firstFrame, false)
            && spriteInteriorMatchesTexel(*preview, firstFrame, false),
        "SPR preview can restore the exact first-cel pixels before playback assertions");

    const bool cleanBeforePlayback = !ui::resources::resourceAuthoringPaneIsModified(pane);
    fps->setValue(99);
    QImage blueFrame{};
    QElapsedTimer animationDeadline{};
    animationDeadline.start();
    while ( animationDeadline.elapsed() < 2000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
        blueFrame = preview->grabFramebuffer();
        if ( imageHasDominantPixels(blueFrame, true)
            && spriteInteriorMatchesTexel(*preview, blueFrame, true) )
        {
            break;
        }
    }
    const bool secondCelRendered = imageHasDominantPixels(blueFrame, true)
        && spriteInteriorMatchesTexel(*preview, blueFrame, true);
    check(secondCelRendered && blueFrame != firstFrame,
        "SPR animation advances to the distinct blue cel with its different 3 by 2 texture size");
    check(pause->toolTip() == QStringLiteral("Pause animation"),
        "SPR playback exposes the exact Pause animation command description");

    const int selectedFps = fps->value();
    pause->click();
    const bool pauseStateVisible = pause->toolTip()
        == QStringLiteral("Play animation");
    const QImage pausedFrame = preview->grabFramebuffer();
    QElapsedTimer pauseDeadline{};
    pauseDeadline.start();
    while ( pauseDeadline.elapsed() < 80 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    const QImage stillPausedFrame = preview->grabFramebuffer();
    check(pauseStateVisible && fps->value() == selectedFps && pausedFrame == stillPausedFrame,
        "SPR Pause holds the selected cel and preserves the FPS value");

    stop->click();
    QImage stoppedFrame{};
    QElapsedTimer stopDeadline{};
    stopDeadline.start();
    while ( stopDeadline.elapsed() < 1500 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
        stoppedFrame = preview->grabFramebuffer();
        if ( imageHasDominantPixels(stoppedFrame, false)
            && spriteInteriorMatchesTexel(*preview, stoppedFrame, false) )
        {
            break;
        }
    }
    check(imageHasDominantPixels(stoppedFrame, false) && fps->value() == selectedFps
            && pause->toolTip() == QStringLiteral("Play animation")
            && spriteInteriorMatchesTexel(*preview, stoppedFrame, false),
        "SPR Stop returns to cel zero, pauses playback, and retains the selected FPS value");
    fps->setValue(0);
    const QImage zeroFpsFrame = preview->grabFramebuffer();
    QElapsedTimer zeroFpsDeadline{};
    zeroFpsDeadline.start();
    while ( zeroFpsDeadline.elapsed() < 80 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    check(zeroFpsFrame == preview->grabFramebuffer()
            && !ui::resources::resourceAuthoringPaneIsModified(pane)
            && cleanBeforePlayback,
        "SPR FPS zero pauses without changing authored state");

    const auto waitForAnimationControlsHidden = [fps, pause, stop]()
    {
        QElapsedTimer deadline{};
        deadline.start();
        while ( deadline.elapsed() < 1500 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
            if ( fps->isHidden() && pause->isHidden() && stop->isHidden() )
            {
                return true;
            }
        }
        return false;
    };

    check(setMaterialInCleanDraft(QStringLiteral("static.mat")),
        "SPR static MAT switch remains a presentation-only preview change");
    check(waitForAnimationControlsHidden(),
        "SPR animation controls hide together for a single-cel MAT");
    check(setMaterialInCleanDraft(QStringLiteral("invalid.mat")),
        "SPR invalid MAT switch remains a presentation-only preview change");
    check(waitForAnimationControlsHidden(),
        "SPR animation controls stay hidden when the material cannot be decoded");
    check(setMaterialInCleanDraft(QStringLiteral("animated.mat")),
        "SPR animation controls can be restored by selecting a valid animated MAT");
    QElapsedTimer restoreControlsDeadline{};
    restoreControlsDeadline.start();
    while ( restoreControlsDeadline.elapsed() < 1500
        && (fps->isHidden() || pause->isHidden() || stop->isHidden()) )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    check(!fps->isHidden() && !pause->isHidden() && !stop->isHidden(),
        "SPR animation controls reappear together for a valid multi-cel MAT");

    const auto cancelMaterialPicker = [](QDialog* dialog)
    {
        dialog->reject();
    };
    ModalStep picker = pickerStep(QStringLiteral("Material Picker"), QStringLiteral("static.mat"));
    picker.interact = cancelMaterialPicker;
    BoundedModalSequence materialDoubleClick({ picker });
    materialDoubleClick.start();
    QMouseEvent materialDblClick(
        QEvent::MouseButtonDblClick,
        QPointF(material->rect().center()),
        QPointF(material->mapToGlobal(material->rect().center())),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(material, &materialDblClick);
    materialDoubleClick.stop();
    check(materialDoubleClick.completedSteps() == 1 && !materialDoubleClick.timedOut()
            && material->text() == QStringLiteral("animated.mat"),
        "double-clicking the SPR material field opens the shared material picker and cancel preserves its exact token");

    ModalStep previewPicker = pickerStep(
        QStringLiteral("Material Picker"), QStringLiteral("static.mat"));
    previewPicker.interact = cancelMaterialPicker;
    BoundedModalSequence previewDoubleClick({ previewPicker });
    previewDoubleClick.start();
    const QPoint previewPoint = preview->rect().center();
    QMouseEvent previewDblClick(
        QEvent::MouseButtonDblClick,
        QPointF(previewPoint),
        QPointF(preview->mapToGlobal(previewPoint)),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(preview, &previewDblClick);
    previewDoubleClick.stop();
    check(previewDoubleClick.completedSteps() == 1 && !previewDoubleClick.timedOut()
            && material->text() == QStringLiteral("animated.mat"),
        "double-clicking the SPR preview opens the same picker without changing the draft on cancel");
}

void testSpritePausedPresentationRecoverySchema()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const auto resources = makeLocator(root);
    QWidget host{};
    auto* const layout = new QVBoxLayout(&host);
    auto context = testContext(root, resources);
    QWidget* const pane = ui::resources::createResourceAuthoringPane(
        &host,
        ui::resources::ResourceAuthoringKind::sprite,
        std::move(context));
    if ( pane )
    {
        layout->addWidget(pane);
    }
    check(temporaryDirectory.isValid() && pane,
        "SPR paused-state fixture creates a hidden profile-bound pane");
    if ( !temporaryDirectory.isValid() || !pane )
    {
        return;
    }

    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane);
    QCborMap presentation = state.value(QStringLiteral("presentation")).toMap();
    const bool defaultPausedIsExplicit = presentation.value(
        QStringLiteral("paused")).isBool()
        && !presentation.value(QStringLiteral("paused")).toBool();
    presentation.insert(QStringLiteral("paused"), true);
    state.insert(QStringLiteral("presentation"), presentation);
    state.insert(QStringLiteral("modified"), false);

    QString warning{};
    const bool restoredPausedState =
        ui::resources::restoreResourceAuthoringPaneState(pane, state, &warning);
    const QCborMap afterRestore =
        ui::resources::captureResourceAuthoringPaneState(pane);
    const QCborMap afterPresentation = afterRestore.value(
        QStringLiteral("presentation")).toMap();
    check(restoredPausedState && warning.isEmpty()
            && afterPresentation.value(QStringLiteral("paused")).isBool()
            && afterPresentation.value(QStringLiteral("paused")).toBool()
            && !ui::resources::resourceAuthoringPaneIsModified(pane),
        "SPR restore immediately retains a paused nonzero-FPS presentation before preview attachment without dirtying authored content");
    check(defaultPausedIsExplicit,
        "SPR presentation writes an explicit false paused default for older-state compatibility");

    QCborMap malformed = afterRestore;
    QCborMap malformedPresentation = malformed.value(
        QStringLiteral("presentation")).toMap();
    malformedPresentation.insert(QStringLiteral("paused"), 1);
    malformed.insert(QStringLiteral("presentation"), malformedPresentation);
    const bool malformedRejected =
        !ui::resources::restoreResourceAuthoringPaneState(pane, malformed, &warning);
    const QCborMap afterRejectedRestore =
        ui::resources::captureResourceAuthoringPaneState(pane);
    check(malformedRejected && !warning.isEmpty()
            && afterRejectedRestore == afterRestore
            && !ui::resources::resourceAuthoringPaneIsModified(pane),
        "SPR non-boolean paused state is rejected atomically and leaves the clean draft unchanged");
}

void testSpriteCameraPresentationBoundaryValidation()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const auto resources = makeLocator(root);
    QWidget host{};
    auto* const layout = new QVBoxLayout(&host);
    auto context = testContext(root, resources);
    QWidget* const pane = ui::resources::createResourceAuthoringPane(
        &host,
        ui::resources::ResourceAuthoringKind::sprite,
        std::move(context));
    if ( pane )
    {
        layout->addWidget(pane);
    }
    check(temporaryDirectory.isValid() && pane,
        "SPR camera-boundary fixture creates a pane for exact malformed-state checks");
    if ( !temporaryDirectory.isValid() || !pane )
    {
        return;
    }

    QCborMap cleanState = ui::resources::captureResourceAuthoringPaneState(pane);
    cleanState.insert(QStringLiteral("modified"), false);
    QString warning{};
    const bool cleanRestore =
        ui::resources::restoreResourceAuthoringPaneState(pane, cleanState, &warning);
    const QCborMap cleanBaseline =
        ui::resources::captureResourceAuthoringPaneState(pane);
    check(cleanRestore && warning.isEmpty(),
        "SPR camera-boundary fixture establishes a clean presentation baseline");
    if ( !cleanRestore )
    {
        return;
    }

    struct InvalidCameraCase
    {
        QString label{};
        QString expectedWarning{};
        std::function<void(QCborMap&, QCborArray&)> mutate{};
    };
    std::vector<InvalidCameraCase> cases{};
    cases.push_back({
        QStringLiteral("non-numeric orbit center"),
        QStringLiteral("Sprite presentation camera orbit is out of range."),
        [](QCborMap&, QCborArray& orbit)
        {
            orbit[0] = QStringLiteral("zero");
        }});
    cases.push_back({
        QStringLiteral("oversized center"),
        QStringLiteral("Sprite presentation camera orbit is out of range."),
        [](QCborMap&, QCborArray& orbit)
        {
            orbit[0] = 1.000000000001e12;
        }});
    cases.push_back({
        QStringLiteral("float-underflow radius"),
        QStringLiteral("Sprite presentation camera distance is out of range."),
        [](QCborMap&, QCborArray& orbit)
        {
            orbit[6] = 1.0e-50;
        }});
    cases.push_back({
        QStringLiteral("subminimum distance"),
        QStringLiteral("Sprite presentation camera distance is out of range."),
        [](QCborMap&, QCborArray& orbit)
        {
            orbit[7] = 1.0e-7;
        }});
    cases.push_back({
        QStringLiteral("angle above bound"),
        QStringLiteral("Sprite presentation camera orbit is out of range."),
        [](QCborMap&, QCborArray& orbit)
        {
            orbit[8] = 1.000000001e9;
        }});
    cases.push_back({
        QStringLiteral("FOV above bound"),
        QStringLiteral("Sprite presentation camera settings are malformed."),
        [](QCborMap& camera, QCborArray&)
        {
            camera.insert(QStringLiteral("fovXDeg"), 180.0);
        }});
    cases.push_back({
        QStringLiteral("orthographic scale underflow"),
        QStringLiteral("Sprite presentation camera settings are malformed."),
        [](QCborMap& camera, QCborArray&)
        {
            camera.insert(QStringLiteral("orthoScale"), 1.0e-50);
        }});
    cases.push_back({
        QStringLiteral("wrong clipping type"),
        QStringLiteral("Sprite presentation camera settings are malformed."),
        [](QCborMap& camera, QCborArray&)
        {
            camera.insert(QStringLiteral("clipEnabled"), QStringLiteral("true"));
        }});

    for ( const InvalidCameraCase& testCase : cases )
    {
        QCborMap malformed = cleanBaseline;
        QCborMap presentation = malformed.value(QStringLiteral("presentation")).toMap();
        QCborMap camera = presentation.value(QStringLiteral("camera")).toMap();
        QCborArray orbit = camera.value(QStringLiteral("orbit")).toArray();
        testCase.mutate(camera, orbit);
        camera.insert(QStringLiteral("orbit"), orbit);
        presentation.insert(QStringLiteral("camera"), camera);
        malformed.insert(QStringLiteral("presentation"), presentation);

        warning = QStringLiteral("stale warning");
        const bool rejected =
            !ui::resources::restoreResourceAuthoringPaneState(pane, malformed, &warning);
        const QCborMap unchanged =
            ui::resources::captureResourceAuthoringPaneState(pane);
        check(rejected && warning == testCase.expectedWarning
                && unchanged == cleanBaseline,
            (QStringLiteral("SPR camera boundary rejects %1 with an exact warning and no state mutation: ")
                .arg(testCase.label)
                + warning).toUtf8().constData());
    }
}

void testConfiguredResourceRootSeedsSaveAsWithoutWriting()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const std::filesystem::path resourceRoot = root / "Resource";
    std::error_code directoryError{};
    std::filesystem::create_directories(resourceRoot, directoryError);
    check(temporaryDirectory.isValid() && !directoryError,
        "configured-resource-root fixture creates only its pre-existing Resource root");
    if ( !temporaryDirectory.isValid() || directoryError )
    {
        return;
    }

    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.setResourcePathsForTest({ resourceRoot }, {});
    window.resize(1100, 760);
    window.show();
    QApplication::processEvents();

    struct SuggestionCase
    {
        ProjectType profile{};
        QString actionName{};
        std::filesystem::path expected{};
    };
    const std::array<SuggestionCase, 4> cases{
        SuggestionCase{
            ProjectType::JKDF2,
            QStringLiteral("tools.resource_editors.sprite_spr.new"),
            resourceRoot / "misc" / "spr" / "new_sprite.spr" },
        SuggestionCase{
            ProjectType::JKDF2,
            QStringLiteral("tools.resource_editors.sound_class_snd.new"),
            resourceRoot / "misc" / "snd" / "new_sound_class.snd" },
        SuggestionCase{
            ProjectType::JKDF2,
            QStringLiteral("tools.resource_editors.string_table_uni.new"),
            resourceRoot / "misc" / "cogstrings.uni" },
        SuggestionCase{
            ProjectType::IJIM,
            QStringLiteral("tools.resource_editors.string_table_uni.new"),
            resourceRoot / "misc" / "jonesstrings.uni" },
    };

    for ( const SuggestionCase& testCase : cases )
    {
        window.setProjectTypeForTest(testCase.profile);
        QAction* const action = window.findChild<QAction*>(testCase.actionName);
        if ( action )
        {
            createResourceDraftFromGroup(action);
        }
        QTabWidget* const tabs = window.findChild<QTabWidget*>(
            testCase.actionName.contains(QStringLiteral("sound_class"))
                ? QStringLiteral("soundClassResourceEditorTabs")
                : testCase.actionName.contains(QStringLiteral("string_table"))
                    ? QStringLiteral("uniResourceEditorTabs") : QStringLiteral("resourceEditorTabs"));
        QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
        check(action && page,
            "configured resource root creates the requested direct Tools draft");
        if ( !action || !page )
        {
            continue;
        }

        const QCborMap before = authoredDraftState(
            ui::resources::captureResourceAuthoringPaneState(page));
        QString selectedPath{};
        ModalStep cancelSaveAs{};
        cancelSaveAs.matches = [](QDialog* dialog)
        {
            return dialog->objectName()
                == QStringLiteral("resource_authoring.file_dialog")
                && qobject_cast<QFileDialog*>(dialog);
        };
        cancelSaveAs.interact = [&selectedPath](QDialog* dialog)
        {
            auto* const chooser = qobject_cast<QFileDialog*>(dialog);
            if ( chooser && !chooser->selectedFiles().isEmpty() )
            {
                selectedPath = chooser->selectedFiles().front();
            }
            dialog->reject();
        };
        BoundedModalSequence modal({ std::move(cancelSaveAs) }, 2000);
        modal.start();
        const bool saved = ui::resources::resourceAuthoringPaneSave(page, true);
        modal.stop();

        const std::filesystem::path selected(selectedPath.toStdWString());
        check(!saved && modal.completedSteps() == 1 && !modal.timedOut()
                && selected == testCase.expected,
            "untitled Save As opens at the exact configured Resource default path and cancellation performs no save");
        check(!std::filesystem::exists(resourceRoot / "misc")
                && !std::filesystem::exists(testCase.expected)
                && authoredDraftState(
                       ui::resources::captureResourceAuthoringPaneState(page)) == before,
            "New and canceled Save As leave configured resource folders, files, and raw pane state untouched");
    }
}

void testResourceDraftPeriodicAutosaveDiskRoundTrip()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const std::filesystem::path recoveryDirectory = root / "recovery";
    const std::filesystem::path sourcePath = root / "project.sed";
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    window.setRecoveryDirectoryForTest(recoveryDirectory);
    window.beginRecoveryDocumentForTest(sourcePath, false);
    window.flushRecoverySnapshotForTest(true);
    window.waitForRecoverySnapshotForTest();
    check(temporaryDirectory.isValid()
            && std::filesystem::exists(window.currentRecoveryPathForTest()),
        "resource autosave fixture publishes a real clean .sedrecovery checkpoint first");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    QAction* const createUni = window.findChild<QAction*>(
        QStringLiteral("tools.resource_editors.string_table_uni.new"));
    if ( createUni )
    {
        createResourceDraftFromGroup(createUni);
    }
    QTabWidget* const tabs = window.findChild<QTabWidget*>(
        QStringLiteral("uniResourceEditorTabs"));
    QWidget* const pane = tabs ? tabs->currentWidget() : nullptr;
    QTableWidget* const table = pane
        ? pane->findChild<QTableWidget*>(QStringLiteral("uni.entries"))
        : nullptr;
    QAbstractButton* const add = button(pane, QStringLiteral("uni.add"));
    check(createUni && pane && table && add,
        "resource autosave fixture creates a real UNI draft with an editable table");
    if ( !createUni || !pane || !table || !add )
    {
        return;
    }

    add->click();
    if ( table->rowCount() == 1 && table->item(0, 1) )
    {
        QWidget* const activeKeyEditor = table->indexWidget(
            table->model()->index(0, 0));
        QPointer<QLineEdit> keyEditor(qobject_cast<QLineEdit*>(activeKeyEditor));
        if ( keyEditor )
        {
            keyEditor->setText(QStringLiteral("AUTOSAVE_RAW"));
            QKeyEvent commitKey(
                QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(keyEditor.data(), &commitKey);
            QKeyEvent commitRelease(
                QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
            if ( keyEditor )
            {
                QApplication::sendEvent(keyEditor.data(), &commitRelease);
            }
        }
        else
        {
            table->item(0, 0)->setText(QStringLiteral("AUTOSAVE_RAW"));
        }
        table->item(0, 1)->setText(QStringLiteral("17x"));
        table->item(0, 2)->setText(QStringLiteral("4"));
        table->setCurrentCell(0, 1);
    }
    const QCborMap authored = ui::resources::captureResourceAuthoringPaneState(pane);
    const QCborArray rows = authored.value(QStringLiteral("rows")).toArray();
    const QCborArray row = rows.size() == 1 ? rows.at(0).toArray() : QCborArray{};
    const bool invalidRawDraftReady = row.size() == 3
        && row.at(0).toString() == QStringLiteral("AUTOSAVE_RAW")
        && row.at(1).toString() == QStringLiteral("17x")
        && row.at(2).toString() == QStringLiteral("4")
        && ui::resources::resourceAuthoringPaneIsModified(pane);
    check(invalidRawDraftReady,
        "resource autosave captures the exact dirty invalid numeric draft before publication");
    if ( !invalidRawDraftReady )
    {
        return;
    }

    window.flushRecoverySnapshotForTest(true);
    window.waitForRecoverySnapshotForTest();
    const std::optional<std::string> firstLiveBytes =
        readFile(window.currentRecoveryPathForTest());
    if ( table->item(0, 1) )
    {
        table->item(0, 1)->setText(QStringLiteral("18x"));
    }
    const QCborArray changedRows =
        ui::resources::captureResourceAuthoringPaneState(pane)
            .value(QStringLiteral("rows")).toArray();
    const QCborArray changedRow = changedRows.size() == 1
        ? changedRows.at(0).toArray()
        : QCborArray{};
    const bool changedRawDraftReady = firstLiveBytes.has_value()
        && changedRow.size() == 3
        && changedRow.at(1).toString() == QStringLiteral("18x");
    check(changedRawDraftReady,
        "resource autosave writes the first 17x draft checkpoint before a later raw edit to 18x");
    if ( !changedRawDraftReady )
    {
        return;
    }

    window.triggerPeriodicRecoverySnapshotForTest();
    window.waitForRecoverySnapshotForTest();
    const std::vector<ui::recovery::RecoveryDescriptor> history =
        ui::recovery::scanAutosaveHistory(recoveryDirectory);
    check(!history.empty(),
        "a periodic resource-only authored change is written to on-disk .sedautosave history");
    if ( history.empty() )
    {
        return;
    }

    QString readError{};
    const std::optional<ui::recovery::RecoveryRecord> record =
        ui::recovery::readRecoveryRecord(history.back().recoveryPath, &readError);
    const QCborMap editRoot = record
        ? QCborValue::fromCbor(record->editorStatePayload).toMap()
        : QCborMap{};
    const QCborMap resourceState = editRoot.value(
        QStringLiteral("resourceEditors")).toMap();
    const QCborArray savedTabs = resourceState.value(QStringLiteral("tabs")).toArray();
    const QCborMap savedPage = savedTabs.size() == 1
        ? savedTabs.at(0).toMap().value(QStringLiteral("page")).toMap()
        : QCborMap{};
    const QCborArray savedRows = savedPage.value(QStringLiteral("rows")).toArray();
    const QCborArray savedRow = savedRows.size() == 1
        ? savedRows.at(0).toArray()
        : QCborArray{};
    const bool diskSnapshotHasDraft = record
        && history.back().recoveryPath.extension() == ".sedautosave"
        && savedPage.value(QStringLiteral("kind")).toInteger() == 2
        && savedPage.value(QStringLiteral("profile")).toInteger() == 0
        && savedPage.value(QStringLiteral("sourceKind")).toInteger() == 0
        && !savedPage.value(QStringLiteral("draftIdentity")).toString().isEmpty()
        && savedRow.size() == 3
        && savedRow.at(0).toString() == QStringLiteral("AUTOSAVE_RAW")
        && savedRow.at(1).toString() == QStringLiteral("17x")
        && savedRow.at(2).toString() == QStringLiteral("4");
    check(diskSnapshotHasDraft,
        "the .sedautosave archive preserves resource kind, profile, draft identity, source kind, ordered cells, and the exact preceding invalid 17x draft");
    if ( !readError.isEmpty() )
    {
        std::cerr << "resource autosave read error: " << readError.toStdString() << '\n';
    }

    if ( !record )
    {
        return;
    }
    const auto restoredTextFromRecord = [](const QByteArray& payload,
                                           const QString& expectedText,
                                           const QString& expectedIdentity)
    {
        MainWindow restored(nullptr, true);
        restored.setProjectTypeForTest(ProjectType::JKDF2);
        QString restoreWarning{};
        const bool restoredWorkspace =
            restored.restoreRecoveryEditorStateForTest(payload, &restoreWarning);
        QTabWidget* const restoredTabs = restored.findChild<QTabWidget*>(
            QStringLiteral("uniResourceEditorTabs"));
        QWidget* const restoredPane = restoredTabs
            ? restoredTabs->currentWidget()
            : nullptr;
        const QCborMap restoredState = restoredPane
            ? ui::resources::captureResourceAuthoringPaneState(restoredPane)
            : QCborMap{};
        const QCborArray restoredRows = restoredState.value(
            QStringLiteral("rows")).toArray();
        const QCborArray restoredRow = restoredRows.size() == 1
            ? restoredRows.at(0).toArray()
            : QCborArray{};
        return restoredWorkspace && restoreWarning.isEmpty()
            && restoredState.value(QStringLiteral("draftIdentity")).toString()
                == expectedIdentity
            && restoredRow.size() == 3
            && restoredRow.at(0).toString() == QStringLiteral("AUTOSAVE_RAW")
            && restoredRow.at(1).toString() == expectedText
            && restoredRow.at(2).toString() == QStringLiteral("4")
            && ui::resources::resourceAuthoringPaneIsModified(restoredPane);
    };
    check(restoredTextFromRecord(
              record->editorStatePayload,
              QStringLiteral("17x"),
              savedPage.value(QStringLiteral("draftIdentity")).toString()),
        "restoring the actual .sedautosave payload recovers its exact prior raw UNI draft");

    QString liveReadError{};
    const std::optional<ui::recovery::RecoveryRecord> liveRecord =
        ui::recovery::readRecoveryRecord(
            window.currentRecoveryPathForTest(), &liveReadError);
    const QCborMap liveRoot = liveRecord
        ? QCborValue::fromCbor(liveRecord->editorStatePayload).toMap()
        : QCborMap{};
    const QCborArray liveTabs = liveRoot.value(
        QStringLiteral("resourceEditors")).toMap()
            .value(QStringLiteral("tabs")).toArray();
    const QCborMap livePage = liveTabs.size() == 1
        ? liveTabs.at(0).toMap().value(QStringLiteral("page")).toMap()
        : QCborMap{};
    check(liveRecord
            && restoredTextFromRecord(
                   liveRecord->editorStatePayload,
                   QStringLiteral("18x"),
                   livePage.value(QStringLiteral("draftIdentity")).toString()),
        "restoring the current on-disk .sedrecovery payload recovers the newest exact 18x raw UNI draft");
    if ( !liveReadError.isEmpty() )
    {
        std::cerr << "resource live recovery read error: "
            << liveReadError.toStdString() << '\n';
    }
}

void testResourceWorkspaceNewOpenAndToolbarShell()
{
    MainWindow window(nullptr, true);
    QMenu* toolsMenu = nullptr;
    for ( QAction* const action : window.menuBar()->actions() )
    {
        QString menuText = action->text();
        menuText.remove(QLatin1Char('&'));
        if ( menuText == QStringLiteral("Tools") )
        {
            toolsMenu = action->menu();
            break;
        }
    }

    const auto directToolsAction = [toolsMenu](const QString& text) -> QAction*
    {
        if ( !toolsMenu )
        {
            return nullptr;
        }
        for ( QAction* const action : toolsMenu->actions() )
        {
            QString actionText = action->text();
            actionText.remove(QLatin1Char('&'));
            if ( !action->menu() && actionText == text )
            {
                return action;
            }
        }
        return nullptr;
    };

    QAction* const spriteTool = directToolsAction(QStringLiteral("Sprite Editor"));
    QAction* const soundTool = directToolsAction(QStringLiteral("Sound Class Editor"));
    QAction* const stringTool = directToolsAction(QStringLiteral("UNI Editor"));
    QAction* const cogTool = directToolsAction(QStringLiteral("COG Script Editor"));
    check(spriteTool && soundTool && stringTool,
        "resource shell: Tools directly exposes Sprite Editor, Sound Class Editor, and UNI Editor commands");
    const bool noLegacyOpenResourceSubmenu = toolsMenu
        && toolsMenu->findChild<QMenu*>(QStringLiteral("tools.resource_editors.open")) == nullptr;
    check(noLegacyOpenResourceSubmenu,
        "resource shell: Tools exposes no per-kind Open Resource submenu beside the three editor commands");
    check(toolsMenu && toolsMenu->actions().size() == 4
            && toolsMenu->actions().at(0) == spriteTool
            && toolsMenu->actions().at(1) == soundTool
            && toolsMenu->actions().at(2) == stringTool
            && toolsMenu->actions().at(3) == cogTool,
        "resource shell: Tools contains exactly the four direct resource and COG editor commands");
    check(spriteTool && soundTool && stringTool && cogTool
            && !spriteTool->icon().isNull() && !soundTool->icon().isNull()
            && !stringTool->icon().isNull() && !cogTool->icon().isNull(),
        "resource shell: all four direct Tools editor actions have theme icons");
    QDockWidget* const cogDock = window.findChild<QDockWidget*>(QStringLiteral("cogScriptEditorDock"));
    QMenu* const viewWindows = window.findChild<QMenu*>(QStringLiteral("view.windows"));
    check(cogDock && cogTool == cogDock->toggleViewAction()
            && viewWindows && !viewWindows->actions().contains(cogDock->toggleViewAction()),
        "resource shell: COG Script Editor moves its existing dock toggle from View Windows into Tools");

    QAction* const legacySprite = window.findChild<QAction*>(
        QStringLiteral("edit.resources.sprite_spr"));
    QAction* const initialSprite = spriteTool ? spriteTool : legacySprite;
    check(initialSprite != nullptr,
        "resource shell fixture can open a Sprite pane before exercising New");
    if ( !initialSprite )
    {
        return;
    }
    createResourceDraftFromGroup(initialSprite);

    QTabWidget* const tabs = window.findChild<QTabWidget*>(
        QStringLiteral("resourceEditorTabs"));
    QWidget* const callerPage = tabs && tabs->count() > 0
        ? tabs->widget(0) : nullptr;
    QLineEdit* const callerMaterial = callerPage
        ? callerPage->findChild<QLineEdit*>(QStringLiteral("spr.material"))
        : nullptr;
    check(tabs && callerPage && callerMaterial,
        "resource shell: first Sprite tab exposes its material draft field");
    if ( !tabs || !callerPage || !callerMaterial )
    {
        return;
    }

    callerMaterial->setText(QStringLiteral("caller raw dirty draft.mat"));
    const bool callerDirty = ui::resources::resourceAuthoringPaneIsModified(callerPage);
    const QString callerRawText = callerMaterial->text();
    const QCborMap beforeNew = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(callerPage));
    check(callerDirty && callerRawText == QStringLiteral("caller raw dirty draft.mat"),
        "resource shell: caller pane holds a dirty raw draft before New");

    QDockWidget* const groupDock = window.findChild<QDockWidget*>(QStringLiteral("resourceEditorsDock"));
    QAbstractButton* const newButton = groupDock
        ? groupDock->findChild<QAbstractButton*>(QStringLiteral("resource_authoring.new")) : nullptr;
    check(newButton != nullptr,
        "resource shell: the active Sprite pane exposes its New control");
    if ( newButton )
    {
        BoundedModalSequence oldNewPrompt({ unsavedPromptStep(QMessageBox::Cancel) }, 2000);
        oldNewPrompt.start();
        newButton->click();
        oldNewPrompt.stop();
        check(oldNewPrompt.completedSteps() == 0 && !oldNewPrompt.timedOut(),
            "resource shell: New creates a sibling tab without prompting to replace the dirty caller");
    }

    QWidget* const newPage = tabs->currentWidget();
    check(tabs->count() == 2 && newPage && newPage != callerPage,
        "resource shell: New adds a second tab instead of replacing the active draft");
    const QCborMap afterNew = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(callerPage));
    check(callerMaterial->text() == callerRawText && beforeNew == afterNew
            && ui::resources::resourceAuthoringPaneIsModified(callerPage),
        "resource shell: New preserves the caller tab's complete authored projection and dirty flag");

    const std::array<QString, 3> toolbarNames{
        QStringLiteral("resource_authoring.new"),
        QStringLiteral("resource_authoring.open"),
        QStringLiteral("resource_authoring.save") };
    bool iconOnlyChrome = newPage != nullptr;
    std::vector<QString> commonButtonNames{};
    QWidget* const groupToolbar = groupDock
        ? groupDock->findChild<QWidget*>(QStringLiteral("resource_authoring.group_toolbar")) : nullptr;
    if ( groupToolbar )
    {
        for ( QAbstractButton* const control : groupToolbar->findChildren<QAbstractButton*>(QString{}, Qt::FindDirectChildrenOnly) )
        {
            if ( control->objectName().startsWith(QStringLiteral("resource_authoring.")) && !control->isHidden() )
            {
                commonButtonNames.push_back(control->objectName());
            }
        }
    }
    for ( const QString& objectName : toolbarNames )
    {
        QAbstractButton* const control = groupToolbar
            ? groupToolbar->findChild<QAbstractButton*>(objectName, Qt::FindDirectChildrenOnly) : nullptr;
        iconOnlyChrome = iconOnlyChrome && control
            && control->text().isEmpty() && !control->icon().isNull();
    }
    std::sort(commonButtonNames.begin(), commonButtonNames.end());
    std::array<QString, 3> expectedCommonButtonNames = toolbarNames;
    std::sort(expectedCommonButtonNames.begin(), expectedCommonButtonNames.end());
    const bool onlyThreeCommonControls = commonButtonNames.size() == 3
        && std::equal(
            commonButtonNames.begin(), commonButtonNames.end(),
            expectedCommonButtonNames.begin());
    check(iconOnlyChrome && onlyThreeCommonControls,
        "resource shell: common toolbar has only icon-only New, Open, and Save controls");
    const bool noLegacyButtons = newPage
        && newPage->findChild<QAbstractButton*>(QStringLiteral("resource_authoring.open_project")) == nullptr
        && newPage->findChild<QAbstractButton*>(QStringLiteral("resource_authoring.save_as")) == nullptr
        && newPage->findChild<QAbstractButton*>(QStringLiteral("resource_authoring.close")) == nullptr
        && newPage->findChild<QAbstractButton*>(QStringLiteral("resource_authoring.resource")) == nullptr;
    check(noLegacyButtons,
        "resource shell: Project Open, Save As, Close, and Resource buttons are absent from common chrome");

    QAbstractButton* const open = groupDock
        ? groupDock->findChild<QAbstractButton*>(QStringLiteral("resource_authoring.open"))
        : nullptr;
    const QCborMap beforeOpen = newPage
        ? authoredDraftState(ui::resources::captureResourceAuthoringPaneState(newPage))
        : QCborMap{};
    const QPointer<QWidget> guardedNewPage(newPage);
    const int tabsBeforeOpen = tabs->count();
    const bool draftWasModified = newPage
        && ui::resources::resourceAuthoringPaneIsModified(newPage);
    bool sawSpritePicker = false;
    if ( open && guardedNewPage )
    {
        ModalStep cancelOpen{};
        cancelOpen.matches = [](QDialog* dialog)
        {
            return dialog->windowTitle() == QStringLiteral("Sprite Picker");
        };
        cancelOpen.interact = [&sawSpritePicker](QDialog* dialog)
        {
            sawSpritePicker = dialog->windowTitle() == QStringLiteral("Sprite Picker");
            dialog->reject();
        };
        BoundedModalSequence modal({ std::move(cancelOpen) }, 2000);
        modal.start();
        open->click();
        modal.stop();
        check(modal.completedSteps() == 1 && !modal.timedOut()
                && sawSpritePicker,
            "resource shell: Open cancel is handled by the project Sprite Picker");
    }
    const QCborMap afterOpen = guardedNewPage
        ? authoredDraftState(ui::resources::captureResourceAuthoringPaneState(
            guardedNewPage.data()))
        : QCborMap{};
    check(open && guardedNewPage && tabs->count() == tabsBeforeOpen
            && tabs->currentWidget() == guardedNewPage.data()
            && beforeOpen == afterOpen
            && ui::resources::resourceAuthoringPaneIsModified(guardedNewPage.data())
                == draftWasModified,
        "resource shell: canceling Open leaves the tab set, authored projection, and dirty flag unchanged");
}

void testToolsResourceGroupsOpenEmptyAndSaveOnlyEdits()
{
    QTemporaryDir temporary{};
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.resize(1280, 860);
    window.show();
    window.activateWindow();
    QApplication::processEvents();
    const std::array<const char*, 3> actionNames{
        "tools.resource_editors.sprite_spr.new",
        "tools.resource_editors.sound_class_snd.new",
        "tools.resource_editors.string_table_uni.new" };
    const std::array<const char*, 3> dockNames{
        "resourceEditorsDock", "soundClassResourceEditorsDock", "uniResourceEditorsDock" };
    const std::array<const char*, 3> tabNames{
        "resourceEditorTabs", "soundClassResourceEditorTabs", "uniResourceEditorTabs" };
    std::array<QDockWidget*, 3> docks{};
    std::array<QTabWidget*, 3> tabs{};
    bool complete = temporary.isValid();
    for ( std::size_t index = 0; index < actionNames.size(); ++index )
    {
        QAction* const openGroup = window.findChild<QAction*>(QString::fromLatin1(actionNames[index]));
        docks[index] = window.findChild<QDockWidget*>(QString::fromLatin1(dockNames[index]));
        tabs[index] = window.findChild<QTabWidget*>(QString::fromLatin1(tabNames[index]));
        complete = complete && openGroup && docks[index] && tabs[index];
        if ( openGroup )
        {
            openGroup->trigger();
            openGroup->trigger();
        }
        QApplication::processEvents();
        QToolButton* const newButton = docks[index]
            ? docks[index]->findChild<QToolButton*>(QStringLiteral("resource_authoring.new")) : nullptr;
        QToolButton* const openButton = docks[index]
            ? docks[index]->findChild<QToolButton*>(QStringLiteral("resource_authoring.open")) : nullptr;
        QToolButton* const saveButton = docks[index]
            ? docks[index]->findChild<QToolButton*>(QStringLiteral("resource_authoring.save")) : nullptr;
        QAction* const saveAll = docks[index]
            ? docks[index]->findChild<QAction*>(QStringLiteral("resource_authoring.save_all")) : nullptr;
        check(openGroup && docks[index] && tabs[index] && !docks[index]->isHidden()
                && tabs[index]->count() == 0 && newButton && newButton->isEnabled()
                && openButton && openButton->isEnabled() && saveButton && !saveButton->isEnabled()
                && saveAll && !saveAll->isEnabled(),
            "Tools opens each resource group empty and repeatedly activates it without creating file drafts");
    }
    check(complete, "resource group empty-open fixture resolves its three typed groups and temporary destination");
    if ( !complete )
    {
        return;
    }
    QToolButton* const newSprite = docks[0]->findChild<QToolButton*>(QStringLiteral("resource_authoring.new"));
    QToolButton* const openSprite = docks[0]->findChild<QToolButton*>(QStringLiteral("resource_authoring.open"));
    QToolButton* const saveSprite = docks[0]->findChild<QToolButton*>(QStringLiteral("resource_authoring.save"));
    QAction* const saveAllSprite = docks[0]->findChild<QAction*>(QStringLiteral("resource_authoring.save_all"));
    check(newSprite && openSprite && saveSprite && saveAllSprite,
        "resource group empty-open fixture exposes the single Sprite toolbar");
    if ( !newSprite || !openSprite || !saveSprite || !saveAllSprite )
    {
        return;
    }
    const int countBeforeOpen = tabs[0]->count();
    ModalStep cancelPicker{};
    cancelPicker.matches = [](QDialog* dialog)
    {
        return dialog->windowTitle() == QStringLiteral("Sprite Picker");
    };
    cancelPicker.interact = [](QDialog* dialog)
    {
        dialog->reject();
    };
    BoundedModalSequence picker({ std::move(cancelPicker) });
    picker.start();
    openSprite->click();
    picker.stop();
    check(picker.completedSteps() == 1 && !picker.timedOut() && countBeforeOpen == 0
            && tabs[0]->count() == 0,
        "canceling explicit Open from an empty resource group leaves it empty without a placeholder draft");
    for ( std::size_t index = 0; index < docks.size(); ++index )
    {
        QToolButton* const newButton = docks[index]->findChild<QToolButton*>(QStringLiteral("resource_authoring.new"));
        QToolButton* const saveButton = docks[index]->findChild<QToolButton*>(QStringLiteral("resource_authoring.save"));
        QAction* const saveAll = docks[index]->findChild<QAction*>(QStringLiteral("resource_authoring.save_all"));
        if ( newButton )
        {
            newButton->click();
        }
        check(tabs[index]->count() == 1 && tabs[index]->currentWidget()
                && !ui::resources::resourceAuthoringPaneIsModified(tabs[index]->currentWidget())
                && saveButton && !saveButton->isEnabled() && saveAll && !saveAll->isEnabled(),
            "explicit New creates one clean resource file tab with Save and Save All disabled until an edit");
    }
    if ( tabs[0]->count() != 1 )
    {
        return;
    }
    QWidget* const untouched = tabs[0]->currentWidget();
    const QCborMap untouchedBefore = authoredDraftState(ui::resources::captureResourceAuthoringPaneState(untouched));
    newSprite->click();
    QWidget* const edited = tabs[0]->currentWidget();
    ui::ScrubFloatEdit* const width = edited
        ? edited->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width")) : nullptr;
    check(tabs[0]->count() == 2 && edited && edited != untouched && width && width->lineEdit(),
        "dirty-only Save fixture creates a second explicit Sprite file tab and its numeric edit");
    if ( tabs[0]->count() != 2 || !edited || edited == untouched || !width || !width->lineEdit() )
    {
        return;
    }
    docks[0]->raise();
    width->lineEdit()->setFocus(Qt::OtherFocusReason);
    width->lineEdit()->selectAll();
    QKeyEvent number(QEvent::KeyPress, Qt::Key_2, Qt::NoModifier, QStringLiteral("2.625"));
    QApplication::sendEvent(width->lineEdit(), &number);
    const QCborMap pending = ui::resources::captureResourceAuthoringPaneState(edited);
    check(pending.value(QStringLiteral("spritePendingNumericEdits")).toMap()
                .value(QStringLiteral("width")).toString() == QStringLiteral("2.625")
            && ui::resources::resourceAuthoringPaneIsModified(edited)
            && saveSprite->isEnabled() && saveAllSprite->isEnabled(),
        "a focused uncommitted numeric edit immediately enables Save and group Save All");
    tabs[0]->setCurrentWidget(untouched);
    check(saveSprite->isEnabled() && saveAllSprite->isEnabled()
            && !ui::resources::resourceAuthoringPaneIsModified(untouched),
        "a clean selected file keeps the Save All dropdown reachable while its same-kind neighbor is modified");
    ModalStep unexpectedCleanSave{};
    unexpectedCleanSave.matches = [](QDialog* dialog)
    {
        return dialog->objectName() == QStringLiteral("resource_authoring.file_dialog");
    };
    unexpectedCleanSave.interact = [](QDialog* dialog)
    {
        dialog->reject();
    };
    BoundedModalSequence cleanSave({ std::move(unexpectedCleanSave) });
    cleanSave.start();
    saveSprite->click();
    cleanSave.stop();
    check(cleanSave.completedSteps() == 0 && !cleanSave.timedOut()
            && ui::resources::resourceAuthoringPaneLooseFilePath(untouched).isEmpty()
            && authoredDraftState(ui::resources::captureResourceAuthoringPaneState(untouched)) == untouchedBefore,
        "plain group Save on a clean selected file opens no destination and writes none of its content");
    const std::filesystem::path destination = std::filesystem::path(temporary.path().toStdWString()) / "only_edited.spr";
    BoundedModalSequence save({ fileDialogStep(destination) });
    save.start();
    saveAllSprite->trigger();
    save.stop();
    const QCborMap untouchedAfterSave = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(untouched));
    const QCborMap editedAfterSave = ui::resources::captureResourceAuthoringPaneState(edited);
    const bool savedOnlyModified = save.completedSteps() == 1 && !save.timedOut()
            && std::filesystem::is_regular_file(destination)
            && ui::resources::resourceAuthoringPaneLooseFilePath(untouched).isEmpty()
            && untouchedAfterSave == untouchedBefore
            && tabs[0]->currentWidget() == untouched
            && !ui::resources::resourceAuthoringPaneIsModified(edited)
            && !saveSprite->isEnabled() && !saveAllSprite->isEnabled();
    if ( !savedOnlyModified )
    {
        std::cerr << "modified-only Save All: steps=" << save.completedSteps()
            << " timeout=" << save.timedOut()
            << " exists=" << std::filesystem::is_regular_file(destination)
            << " untouchedLoose=" << !ui::resources::resourceAuthoringPaneLooseFilePath(untouched).isEmpty()
            << " untouchedSame=" << (untouchedAfterSave == untouchedBefore)
            << " selection=" << (tabs[0]->currentWidget() == untouched)
            << " editedModified=" << ui::resources::resourceAuthoringPaneIsModified(edited)
            << " saveEnabled=" << saveSprite->isEnabled()
            << " saveAllEnabled=" << saveAllSprite->isEnabled()
            << " pendingWidth=" << editedAfterSave.value(QStringLiteral("spritePendingNumericEdits"))
                .toMap().value(QStringLiteral("width")).toString().toStdString()
            << " canonicalWidth=" << editedAfterSave.value(QStringLiteral("sprite"))
                .toArray().at(2).toString().toStdString() << '\n';
        reportChangedStateKeys("modified-only untouched Sprite", untouchedBefore, untouchedAfterSave);
        reportChangedStateKeys("modified-only edited Sprite", pending, editedAfterSave);
    }
    check(savedOnlyModified,
        "Save All writes only modified files, skips untouched new tabs, and disables Save after successful writes");
}

void testResourceMajorGroupsOwnOneToolbarAndIndependentDocks()
{
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    QMainWindow* const workspace = window.findChild<QMainWindow*>(
        QStringLiteral("workspaceWindow"));
    struct GroupFixture
    {
        const char* actionName{};
        const char* dockName{};
        const char* tabsName{};
        ui::resources::ResourceAuthoringKind kind{};
    };
    const std::array<GroupFixture, 3> fixtures{
        GroupFixture{ "tools.resource_editors.sprite_spr.new", "resourceEditorsDock",
            "resourceEditorTabs", ui::resources::ResourceAuthoringKind::sprite },
        GroupFixture{ "tools.resource_editors.sound_class_snd.new", "soundClassResourceEditorsDock",
            "soundClassResourceEditorTabs", ui::resources::ResourceAuthoringKind::soundClass },
        GroupFixture{ "tools.resource_editors.string_table_uni.new", "uniResourceEditorsDock",
            "uniResourceEditorTabs", ui::resources::ResourceAuthoringKind::stringTable } };
    std::array<QDockWidget*, 3> docks{};
    std::array<QTabWidget*, 3> tabs{};
    bool completeGroups = workspace != nullptr;
    for ( std::size_t index = 0; index < fixtures.size(); ++index )
    {
        const GroupFixture& fixture = fixtures[index];
        docks[index] = window.findChild<QDockWidget*>(QString::fromLatin1(fixture.dockName));
        tabs[index] = window.findChild<QTabWidget*>(QString::fromLatin1(fixture.tabsName));
        completeGroups = completeGroups && docks[index] && tabs[index];
    }
    check(completeGroups,
        "resource groups: Sprite, Sound Class, and UNI each own a persistent independently dockable file-tab host");
    if ( !completeGroups )
    {
        return;
    }
    const QByteArray emptySnapshot = window.captureRecoveryEditorStateForTest();
    QCborMap emptyRoot = QCborValue::fromCbor(emptySnapshot).toMap();
    QCborMap emptyResources = emptyRoot.value(QStringLiteral("resourceEditors")).toMap();
    QCborMap emptyGroups = emptyResources.value(QStringLiteral("groups")).toMap();
    QCborMap emptyUni = emptyGroups.value(QStringLiteral("uni")).toMap();
    emptyUni.insert(QStringLiteral("active"), 0);
    emptyGroups.insert(QStringLiteral("uni"), emptyUni);
    emptyResources.insert(QStringLiteral("groups"), emptyGroups);
    emptyRoot.insert(QStringLiteral("resourceEditors"), emptyResources);
    QString emptyWarning{};
    check(!window.validateRecoveryEditorStateForTest(
              ui::recovery::serializeCanonicalCbor(QCborValue(emptyRoot)), &emptyWarning)
            && !emptyWarning.isEmpty(),
        "resource groups: an empty UNI group rejects active file zero instead of restoring a nonexistent tab");
    window.resize(1280, 860);
    window.show();
    for ( QDockWidget* const dock : docks )
    {
        dock->show();
    }
    QElapsedTimer tabGroupDeadline{};
    tabGroupDeadline.start();
    while ( (!workspace->tabifiedDockWidgets(docks[0]).contains(docks[1])
                || !workspace->tabifiedDockWidgets(docks[0]).contains(docks[2]))
        && tabGroupDeadline.elapsed() < 2000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    check(workspace->tabifiedDockWidgets(docks[0]).contains(docks[1])
            && workspace->tabifiedDockWidgets(docks[0]).contains(docks[2]),
        "resource groups: the three editor docks form the default major tab group");
    docks[0]->raise();
    const auto majorGroupTabBar = [workspace, docks]() -> QTabBar*
    {
        for ( QTabBar* const bar : workspace->findChildren<QTabBar*>() )
        {
            if ( bar->count() != static_cast<int>(docks.size()) )
            {
                continue;
            }
            bool matchesGroups = true;
            for ( QDockWidget* const dock : docks )
            {
                bool titleFound = false;
                for ( int tab = 0; tab < bar->count(); ++tab )
                {
                    QString title = bar->tabText(tab);
                    title.remove(QLatin1Char('&'));
                    titleFound = titleFound || title == dock->windowTitle();
                }
                matchesGroups = matchesGroups && titleFound;
            }
            if ( matchesGroups )
            {
                return bar;
            }
        }
        return nullptr;
    };
    QElapsedTimer topTabsDeadline{};
    topTabsDeadline.start();
    while ( (!majorGroupTabBar() || !majorGroupTabBar()->isVisible())
        && topTabsDeadline.elapsed() < 2000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    QTabBar* const groupTabBar = majorGroupTabBar();
    QWidget* const groupHeader = docks[0]->titleBarWidget();
    bool allAreasUseTopTabs = true;
    for ( const Qt::DockWidgetArea area : { Qt::LeftDockWidgetArea, Qt::RightDockWidgetArea,
              Qt::TopDockWidgetArea, Qt::BottomDockWidgetArea } )
    {
        allAreasUseTopTabs = allAreasUseTopTabs
            && workspace->tabPosition(area) == QTabWidget::North;
    }
    check(allAreasUseTopTabs && groupTabBar && groupTabBar->isVisible() && groupHeader
            && groupTabBar->mapToGlobal(groupTabBar->rect().bottomLeft()).y()
                < groupHeader->mapToGlobal(groupHeader->rect().topLeft()).y(),
        "resource groups: the native major dock tabs sit above the editor header in every dock area");

    std::vector<QPointer<QWidget>> pages{};
    std::vector<QCborMap> drafts{};
    bool groupToolbarContract = true;
    for ( std::size_t index = 0; index < fixtures.size(); ++index )
    {
        const GroupFixture& fixture = fixtures[index];
        QAction* const create = window.findChild<QAction*>(
            QString::fromLatin1(fixture.actionName));
        QToolButton* const newButton = docks[index]->findChild<QToolButton*>(
            QStringLiteral("resource_authoring.new"));
        QToolButton* const openButton = docks[index]->findChild<QToolButton*>(
            QStringLiteral("resource_authoring.open"));
        QToolButton* const saveButton = docks[index]->findChild<QToolButton*>(
            QStringLiteral("resource_authoring.save"));
        QAction* const saveAll = docks[index]->findChild<QAction*>(
            QStringLiteral("resource_authoring.save_all"));
        groupToolbarContract = groupToolbarContract && create && newButton
            && openButton && saveButton && saveAll
            && newButton->isEnabled() && openButton->isEnabled()
            && !saveButton->isEnabled() && !saveAll->isEnabled();
        if ( !create || !newButton || !openButton || !saveButton || !saveAll )
        {
            continue;
        }
        createResourceDraftFromGroup(create);
        QWidget* const original = tabs[index]->currentWidget();
        newButton->click();
        groupToolbarContract = groupToolbarContract && tabs[index]->count() == 2
            && tabs[index]->widget(0) == original && tabs[index]->currentWidget() != original
            && !saveButton->isEnabled() && !saveAll->isEnabled()
            && saveButton->popupMode() == QToolButton::MenuButtonPopup
            && saveButton->menu() && saveButton->menu()->actions().contains(saveAll);
        for ( int tab = 0; tab < tabs[index]->count(); ++tab )
        {
            QWidget* const page = tabs[index]->widget(tab);
            const auto hiddenPaneControl = [page](const char* name)
            {
                QToolButton* const control = page->findChild<QToolButton*>(QString::fromLatin1(name));
                return !control || control->isHidden();
            };
            groupToolbarContract = groupToolbarContract
                && ui::resources::resourceAuthoringPaneKind(page) == fixture.kind
                && hiddenPaneControl("resource_authoring.new")
                && hiddenPaneControl("resource_authoring.open")
                && hiddenPaneControl("resource_authoring.save");
            pages.emplace_back(page);
            drafts.push_back(ui::resources::captureResourceAuthoringPaneState(page));
        }
    }
    check(groupToolbarContract && pages.size() == 6,
        "resource groups: each group owns exactly one icon New/Open/Save toolbar with Save All, and homogeneous file panes contain no duplicate toolbar");
    if ( !groupToolbarContract || pages.size() != 6 )
    {
        return;
    }

    window.resize(1280, 860);
    window.show();
    QApplication::processEvents();
    for ( int cycle = 0; cycle < 3; ++cycle )
    {
        docks[0]->setFloating(true);
        docks[0]->show();
        QApplication::processEvents();
        check(docks[0]->isFloating() && !docks[1]->isFloating()
                && !docks[2]->isFloating(),
            "resource groups: floating Sprite leaves Sound Class and UNI docked");
        docks[0]->setFloating(false);
        workspace->tabifyDockWidget(docks[1], docks[0]);
        QApplication::processEvents();
    }
    bool everyFloatingGroupHasNativeMaximize = true;
    std::array<bool, 3> nativeMaximizeByGroup{};
    for ( std::size_t index = 0; index < docks.size(); ++index )
    {
        QDockWidget* const dock = docks[index];
        dock->setFloating(true);
        dock->show();
        QApplication::processEvents(QEventLoop::AllEvents);
        QToolButton* const dockReturn = dock->findChild<QToolButton*>(QStringLiteral("resource_group.undock"));
        QWidget* const toolbar = dock->findChild<QWidget*>(QStringLiteral("resource_authoring.group_toolbar"));
        const Qt::WindowFlags flags = dock->windowFlags();
        nativeMaximizeByGroup[index] = nativeMaximizeButtonClickRoundTrip(dock);
        everyFloatingGroupHasNativeMaximize = everyFloatingGroupHasNativeMaximize
            && dock->isWindow() && dock->isFloating() && !dock->titleBarWidget()
            && !flags.testFlag(Qt::FramelessWindowHint)
            && flags.testFlag(Qt::WindowTitleHint)
            && flags.testFlag(Qt::WindowSystemMenuHint)
            && flags.testFlag(Qt::WindowCloseButtonHint)
            && flags.testFlag(Qt::WindowMaximizeButtonHint)
            && dock->features().testFlag(QDockWidget::DockWidgetMovable)
            && dock->features().testFlag(QDockWidget::DockWidgetFloatable)
            && dock->features().testFlag(QDockWidget::DockWidgetClosable)
            && toolbar && toolbar->isVisible()
            && dockReturn && dockReturn->isVisible()
            && dockReturn->toolTip().startsWith(QStringLiteral("Dock "))
            && nativeMaximizeByGroup[index];
        dock->setFloating(false);
        if ( dock != docks[0] )
        {
            workspace->tabifyDockWidget(docks[0], dock);
        }
        QApplication::processEvents(QEventLoop::AllEvents);
    }
    check(everyFloatingGroupHasNativeMaximize,
        "each whole Sprite, Sound Class, and UNI editor group floats with native maximize, decorated controls, toolbar, and Dock return");
    docks[1]->setFloating(true);
    docks[1]->show();
    const QByteArray snapshot = window.captureRecoveryEditorStateForTest();
    const QCborMap root = QCborValue::fromCbor(snapshot).toMap();
    const QCborMap resourceState = root.value(QStringLiteral("resourceEditors")).toMap();
    const QCborArray hosts = resourceState.value(QStringLiteral("hosts")).toArray();
    check(hosts.size() == 3 && resourceState.value(QStringLiteral("groups")).isMap()
            && resourceState.value(QStringLiteral("tabs")).toArray().size() == 6,
        "resource groups: EDIT captures all three major groups and all six file drafts");
    QString warning{};
    check(window.validateRecoveryEditorStateForTest(snapshot, &warning)
            && window.restoreRecoveryEditorStateForTest(snapshot, &warning)
            && warning.isEmpty(),
        "resource groups: the complete group workspace validates and restores through the native recovery path");
    bool preservedDrafts = true;
    std::size_t flatIndex = 0;
    for ( std::size_t group = 0; group < tabs.size(); ++group )
    {
        preservedDrafts = preservedDrafts && tabs[group]->count() == 2;
        for ( int tab = 0; tab < tabs[group]->count(); ++tab, ++flatIndex )
        {
            preservedDrafts = preservedDrafts && flatIndex < drafts.size()
                && authoredDraftState(ui::resources::captureResourceAuthoringPaneState(
                       tabs[group]->widget(tab))) == authoredDraftState(drafts[flatIndex]);
        }
    }
    const bool floatingMembershipPreserved = docks[1]->isFloating()
        && !docks[0]->isFloating() && !docks[2]->isFloating();
    const bool floatingMaximizePreserved = docks[1]->windowFlags().testFlag(Qt::WindowMaximizeButtonHint);
    const bool nativeMaximizePreserved = nativeMaximizeButtonClickRoundTrip(docks[1]);
    if ( !(preservedDrafts && flatIndex == 6 && floatingMembershipPreserved
            && floatingMaximizePreserved && nativeMaximizePreserved) )
    {
        std::cerr << "resource group recovery diagnostic: drafts=" << preservedDrafts
            << " flatIndex=" << flatIndex << " hostCount=";
        for ( std::size_t index = 0; index < docks.size(); ++index )
        {
            const QRect frame = docks[index]->frameGeometry();
            const QRect client(docks[index]->mapToGlobal(QPoint{}), docks[index]->size());
            std::cerr << " [" << index << ":tabs=" << tabs[index]->count()
                << ",floating=" << docks[index]->isFloating()
                << ",maximize=" << docks[index]->windowFlags().testFlag(Qt::WindowMaximizeButtonHint)
                << ",nativeMaximize=" << nativeMaximizeByGroup[index]
                << ",frame=" << frame.x() << ',' << frame.y() << ',' << frame.width() << ',' << frame.height()
                << ",client=" << client.x() << ',' << client.y() << ',' << client.width() << ',' << client.height()
                << ']';
        }
        std::cerr << '\n';
    }
    check(preservedDrafts && flatIndex == 6 && docks[1]->isFloating()
            && !docks[0]->isFloating() && !docks[2]->isFloating()
            && docks[1]->windowFlags().testFlag(Qt::WindowMaximizeButtonHint)
            && nativeMaximizePreserved,
        "resource groups: restore preserves each file identity/raw draft and floating native maximize state");
}

void testResourceGroupSaveAllSuccessCancelAndFailure()
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "resource group Save All fixture owns temporary destinations");
    if ( !temporary.isValid() )
    {
        return;
    }
    const std::filesystem::path root(temporary.path().toStdWString());
    MainWindow window(nullptr, true);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    QAction* const createSprite = window.findChild<QAction*>(
        QStringLiteral("tools.resource_editors.sprite_spr.new"));
    QAction* const createSound = window.findChild<QAction*>(
        QStringLiteral("tools.resource_editors.sound_class_snd.new"));
    if ( createSprite )
    {
        createResourceDraftFromGroup(createSprite);
        createResourceDraftFromGroup(createSprite);
    }
    if ( createSound )
    {
        createResourceDraftFromGroup(createSound);
    }
    QTabWidget* const spriteTabs = window.findChild<QTabWidget*>(QStringLiteral("resourceEditorTabs"));
    QTabWidget* const soundTabs = window.findChild<QTabWidget*>(QStringLiteral("soundClassResourceEditorTabs"));
    QDockWidget* const spriteDock = window.findChild<QDockWidget*>(QStringLiteral("resourceEditorsDock"));
    QAction* const saveAll = spriteDock
        ? spriteDock->findChild<QAction*>(QStringLiteral("resource_authoring.save_all")) : nullptr;
    check(spriteTabs && spriteTabs->count() == 2 && soundTabs && soundTabs->count() == 1 && saveAll,
        "resource group Save All fixture creates two Sprite drafts and an unrelated Sound Class draft");
    if ( !spriteTabs || spriteTabs->count() != 2 || !soundTabs || soundTabs->count() != 1 || !saveAll )
    {
        return;
    }
    QWidget* const first = spriteTabs->widget(0);
    QWidget* const second = spriteTabs->widget(1);
    QWidget* const unrelated = soundTabs->widget(0);
    QLineEdit* const firstMaterial = first->findChild<QLineEdit*>(QStringLiteral("spr.material"));
    QLineEdit* const secondMaterial = second->findChild<QLineEdit*>(QStringLiteral("spr.material"));
    ui::ScrubFloatEdit* const secondWidth = second->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"));
    check(firstMaterial && secondMaterial && secondWidth && secondWidth->lineEdit(),
        "resource group Save All fixture exposes authored and live numeric Sprite controls");
    if ( !firstMaterial || !secondMaterial || !secondWidth || !secondWidth->lineEdit() )
    {
        return;
    }
    firstMaterial->setText(QStringLiteral("first.mat"));
    secondMaterial->setText(QStringLiteral("second.mat"));
    window.show();
    spriteDock->show();
    spriteDock->raise();
    spriteTabs->setCurrentWidget(second);
    secondWidth->lineEdit()->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    const QCborMap unrelatedBefore = ui::resources::captureResourceAuthoringPaneState(unrelated);
    const bool unrelatedDirty = ui::resources::resourceAuthoringPaneIsModified(unrelated);
    secondWidth->lineEdit()->selectAll();
    for ( const QChar character : QStringLiteral("7.125") )
    {
        const int key = character == QLatin1Char('.')
            ? Qt::Key_Period : Qt::Key_0 + character.digitValue();
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString(character));
        QApplication::sendEvent(secondWidth->lineEdit(), &press);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
        QApplication::sendEvent(secondWidth->lineEdit(), &release);
    }
    const auto firstPath = root / "first.spr";
    const auto secondPath = root / "second.spr";
    BoundedModalSequence saveModal({ fileDialogStep(firstPath), fileDialogStep(secondPath) });
    saveModal.start();
    saveAll->trigger();
    saveModal.stop();
    const bool savedBoth = std::filesystem::is_regular_file(firstPath)
        && std::filesystem::is_regular_file(secondPath);
    const float savedWidth = savedBoth ? assets::loadFromSPR(secondPath).width : 0.0f;
    check(saveModal.completedSteps() == 2 && !saveModal.timedOut() && savedBoth
            && savedWidth == 7.125f
            && !ui::resources::resourceAuthoringPaneIsModified(first)
            && !ui::resources::resourceAuthoringPaneIsModified(second)
            && spriteTabs->count() == 2 && spriteTabs->currentWidget() == second
            && ui::resources::captureResourceAuthoringPaneState(unrelated) == unrelatedBefore
            && ui::resources::resourceAuthoringPaneIsModified(unrelated) == unrelatedDirty,
        "resource group Save All writes every member including active numeric text and preserves selection and the unrelated group");
    if ( !savedBoth )
    {
        return;
    }

    firstMaterial->setText(QStringLiteral("changed-first.mat"));
    secondMaterial->setText(QStringLiteral("invalid token.mat"));
    createResourceDraftFromGroup(createSprite);
    QWidget* const third = spriteTabs->currentWidget();
    QLineEdit* const thirdMaterial = third->findChild<QLineEdit*>(QStringLiteral("spr.material"));
    if ( thirdMaterial )
    {
        thirdMaterial->setText(QStringLiteral("third.mat"));
    }
    const QCborMap secondBeforeFailure = authoredDraftState(ui::resources::captureResourceAuthoringPaneState(second));
    const QCborMap thirdBeforeFailure = authoredDraftState(ui::resources::captureResourceAuthoringPaneState(third));
    const std::optional<std::string> secondBytesBeforeFailure = readFile(secondPath);
    BoundedModalSequence failureModal({ messageBoxStep(QStringLiteral("resource_authoring.error"), QMessageBox::Ok) });
    failureModal.start();
    saveAll->trigger();
    failureModal.stop();
    check(failureModal.completedSteps() == 1 && !failureModal.timedOut()
            && assets::loadFromSPR(firstPath).material == "changed-first.mat"
            && !ui::resources::resourceAuthoringPaneIsModified(first)
            && ui::resources::resourceAuthoringPaneIsModified(second)
            && ui::resources::resourceAuthoringPaneIsModified(third)
            && secondBytesBeforeFailure.has_value()
            && readFile(secondPath) == secondBytesBeforeFailure
            && authoredDraftState(ui::resources::captureResourceAuthoringPaneState(second)) == secondBeforeFailure
            && authoredDraftState(ui::resources::captureResourceAuthoringPaneState(third)) == thirdBeforeFailure
            && spriteTabs->count() == 3 && spriteTabs->currentWidget() == third,
        "resource group Save All stops on a later validation error, keeps prior successful Save clean, and retains later raw drafts and selection");

    MainWindow cancelWindow(nullptr, true);
    QAction* const createCancelSprite = cancelWindow.findChild<QAction*>(QStringLiteral("tools.resource_editors.sprite_spr.new"));
    if ( createCancelSprite )
    {
        createResourceDraftFromGroup(createCancelSprite);
        createResourceDraftFromGroup(createCancelSprite);
    }
    QTabWidget* const cancelTabs = cancelWindow.findChild<QTabWidget*>(QStringLiteral("resourceEditorTabs"));
    QDockWidget* const cancelDock = cancelWindow.findChild<QDockWidget*>(QStringLiteral("resourceEditorsDock"));
    QAction* const cancelSaveAll = cancelDock
        ? cancelDock->findChild<QAction*>(QStringLiteral("resource_authoring.save_all")) : nullptr;
    if ( !cancelTabs || cancelTabs->count() != 2 || !cancelSaveAll )
    {
        check(false, "resource group Save All cancel fixture creates its group and two drafts");
        return;
    }
    for ( int tab = 0; tab < cancelTabs->count(); ++tab )
    {
        QLineEdit* const material = cancelTabs->widget(tab)->findChild<QLineEdit*>(
            QStringLiteral("spr.material"));
        check(material != nullptr,
            "resource group Save All cancel fixture exposes each explicit draft's material edit");
        if ( !material )
        {
            return;
        }
        material->setText(QStringLiteral("cancel_%1.mat").arg(tab));
    }
    const QPointer<QWidget> firstCancelPage(cancelTabs->widget(0));
    const QPointer<QWidget> secondCancelPage(cancelTabs->widget(1));
    const QString firstCancelIdentity = ui::resources::resourceAuthoringPaneIdentity(firstCancelPage.data());
    const QString secondCancelIdentity = ui::resources::resourceAuthoringPaneIdentity(secondCancelPage.data());
    const bool firstModifiedBeforeCancel = ui::resources::resourceAuthoringPaneIsModified(firstCancelPage.data());
    const bool secondModifiedBeforeCancel = ui::resources::resourceAuthoringPaneIsModified(secondCancelPage.data());
    const QCborMap firstBeforeCancel = ui::resources::captureResourceAuthoringPaneState(cancelTabs->widget(0));
    const QCborMap secondBeforeCancel = ui::resources::captureResourceAuthoringPaneState(cancelTabs->widget(1));
    QWidget* const selectedBeforeCancel = cancelTabs->currentWidget();
    ModalStep cancelChooser{};
    cancelChooser.matches = [](QDialog* dialog)
    {
        return dialog->objectName() == QStringLiteral("resource_authoring.file_dialog");
    };
    cancelChooser.interact = [](QDialog* dialog)
    {
        dialog->reject();
    };
    BoundedModalSequence cancelModal({ std::move(cancelChooser) });
    cancelModal.start();
    cancelSaveAll->trigger();
    cancelModal.stop();
    const QCborMap firstAfterCancel = ui::resources::captureResourceAuthoringPaneState(cancelTabs->widget(0));
    const QCborMap secondAfterCancel = ui::resources::captureResourceAuthoringPaneState(cancelTabs->widget(1));
    const bool canceledWithoutChanges = cancelModal.completedSteps() == 1 && !cancelModal.timedOut()
            && cancelTabs->count() == 2 && cancelTabs->currentWidget() == selectedBeforeCancel
            && firstCancelPage && secondCancelPage
            && cancelTabs->widget(0) == firstCancelPage.data()
            && cancelTabs->widget(1) == secondCancelPage.data()
            && ui::resources::resourceAuthoringPaneIdentity(firstCancelPage.data()) == firstCancelIdentity
            && ui::resources::resourceAuthoringPaneIdentity(secondCancelPage.data()) == secondCancelIdentity
            && firstModifiedBeforeCancel && secondModifiedBeforeCancel
            && ui::resources::resourceAuthoringPaneIsModified(firstCancelPage.data()) == firstModifiedBeforeCancel
            && ui::resources::resourceAuthoringPaneIsModified(secondCancelPage.data()) == secondModifiedBeforeCancel
            && authoredDraftState(firstAfterCancel) == authoredDraftState(firstBeforeCancel)
            && authoredDraftState(secondAfterCancel) == authoredDraftState(secondBeforeCancel);
    if ( !canceledWithoutChanges )
    {
        std::cerr << "canceled Save All: steps=" << cancelModal.completedSteps()
            << " timeout=" << cancelModal.timedOut() << " count=" << cancelTabs->count()
            << " selection=" << (cancelTabs->currentWidget() == selectedBeforeCancel)
            << " firstModified=" << ui::resources::resourceAuthoringPaneIsModified(cancelTabs->widget(0))
            << " secondModified=" << ui::resources::resourceAuthoringPaneIsModified(cancelTabs->widget(1))
            << " firstAuthoredSame=" << (authoredDraftState(firstAfterCancel) == authoredDraftState(firstBeforeCancel))
            << " secondAuthoredSame=" << (authoredDraftState(secondAfterCancel) == authoredDraftState(secondBeforeCancel)) << '\n';
        reportChangedStateKeys("canceled first Sprite", firstBeforeCancel, firstAfterCancel);
        reportChangedStateKeys("canceled second Sprite", secondBeforeCancel, secondAfterCancel);
    }
    check(canceledWithoutChanges,
        "resource group Save All cancellation stops before later files and preserves every draft and modified marker");
}

QTabWidget* resourceTabHostContaining(QWidget* const root, QWidget* const page)
{
    if ( !root || !page )
    {
        return nullptr;
    }
    for ( QTabWidget* const tabs : root->findChildren<QTabWidget*>() )
    {
        if ( tabs->indexOf(page) >= 0 )
        {
            return tabs;
        }
    }
    return nullptr;
}

QWidget* resourcePageWithIdentity(QWidget* const root, const QString& identity)
{
    if ( !root || identity.isEmpty() )
    {
        return nullptr;
    }
    for ( QTabWidget* const tabs : root->findChildren<QTabWidget*>() )
    {
        for ( int index = 0; index < tabs->count(); ++index )
        {
            QWidget* const page = tabs->widget(index);
            if ( ui::resources::resourceAuthoringPaneIdentity(page) == identity )
            {
                return page;
            }
        }
    }
    return nullptr;
}

std::vector<QTabWidget*> resourceTabHosts(QWidget* const root)
{
    std::vector<QTabWidget*> hosts{};
    if ( !root )
    {
        return hosts;
    }
    for ( QTabWidget* const tabs : root->findChildren<QTabWidget*>() )
    {
        bool containsResourcePane = false;
        for ( int index = 0; index < tabs->count(); ++index )
        {
            if ( !ui::resources::resourceAuthoringPaneIdentity(tabs->widget(index)).isEmpty() )
            {
                containsResourcePane = true;
                break;
            }
        }
        if ( containsResourcePane )
        {
            hosts.push_back(tabs);
        }
    }
    return hosts;
}

QDockWidget* dockForResourceTabHost(QTabWidget* const tabs)
{
    for ( QWidget* current = tabs; current; current = current->parentWidget() )
    {
        if ( QDockWidget* const dock = qobject_cast<QDockWidget*>(current) )
        {
            return dock;
        }
    }
    return nullptr;
}

struct TabDragOutcome
{
    bool terminalEventsDispatched{};
    bool timedOut{};
    bool cancelTargetHadFocus{};
};

TabDragOutcome sendTabDrag(
    QTabBar* const tabBar,
    const QPoint& startLocal,
    const QPoint& releaseGlobal,
    const bool cancelWithEscape,
    QObject* const escapeTarget = nullptr,
    const bool cancelWithWindowDeactivate = false,
    const std::function<void()>& duringGesture = {},
    const std::function<void()>& afterPress = {})
{
    TabDragOutcome outcome{};
    if ( !tabBar )
    {
        return outcome;
    }
    const QPoint releaseLocal = tabBar->mapFromGlobal(releaseGlobal);
    const QPoint startGlobal = tabBar->mapToGlobal(startLocal);
    QMouseEvent press(
        QEvent::MouseButtonPress,
        QPointF(startLocal),
        QPointF(startLocal),
        QPointF(startGlobal),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(tabBar, &press);
    if ( afterPress )
    {
        afterPress();
    }

    QEventLoop loop{};
    QPointer<QTabBar> guardedTabBar(tabBar);
    QTimer watchdog{};
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, [&outcome, &loop]()
        {
            outcome.timedOut = true;
            QDrag::cancel();
            loop.quit();
        });
    watchdog.start(2000);
    QPointer<QObject> guardedEscapeTarget(
        escapeTarget ? escapeTarget : static_cast<QObject*>(tabBar));

    // Queue terminal input before the movement event. A platform drag can enter
    // a nested event loop on that move; the queued Escape/release remain live there.
    QTimer::singleShot(0, guardedTabBar, [
        guardedTabBar,
        releaseLocal,
        releaseGlobal,
        cancelWithEscape,
        cancelWithWindowDeactivate,
        guardedEscapeTarget,
        duringGesture,
        &outcome,
        &loop]()
        {
            if ( !guardedTabBar )
            {
                loop.quit();
                return;
            }
            QTimer::singleShot(0, guardedTabBar, [
                guardedTabBar,
                releaseLocal,
                releaseGlobal,
                cancelWithEscape,
                cancelWithWindowDeactivate,
                guardedEscapeTarget,
                &outcome,
                &loop]()
                {
                    if ( !guardedTabBar )
                    {
                        loop.quit();
                        return;
                    }
                    if ( cancelWithEscape )
                    {
                        if ( QWidget* const target = qobject_cast<QWidget*>(guardedEscapeTarget.data()) )
                        {
                            target->setFocus(Qt::OtherFocusReason);
                        }
                        outcome.cancelTargetHadFocus = guardedEscapeTarget
                            && QApplication::focusWidget()
                                == qobject_cast<QWidget*>(guardedEscapeTarget.data());
                        if ( guardedEscapeTarget )
                        {
                            QKeyEvent escape(
                                QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                            QApplication::sendEvent(
                                guardedEscapeTarget.data(), &escape);
                            QKeyEvent escapeRelease(
                                QEvent::KeyRelease, Qt::Key_Escape, Qt::NoModifier);
                            QApplication::sendEvent(
                                guardedEscapeTarget.data(), &escapeRelease);
                        }
                    }
                    if ( cancelWithWindowDeactivate )
                    {
                        if ( QWidget* const sourceWindow =
                                 guardedTabBar->window() )
                        {
                            QEvent deactivate(QEvent::WindowDeactivate);
                            QApplication::sendEvent(sourceWindow, &deactivate);
                        }
                    }
                    QMouseEvent release(
                        QEvent::MouseButtonRelease,
                        QPointF(releaseLocal),
                        QPointF(releaseLocal),
                        QPointF(releaseGlobal),
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
                    QApplication::sendEvent(guardedTabBar.data(), &release);
                    outcome.terminalEventsDispatched = true;
                    loop.quit();
                });

            QMouseEvent move(
                QEvent::MouseMove,
                QPointF(releaseLocal),
                QPointF(releaseLocal),
                QPointF(releaseGlobal),
                Qt::LeftButton,
                Qt::LeftButton,
                Qt::NoModifier);
            QApplication::sendEvent(guardedTabBar.data(), &move);
            if ( duringGesture )
            {
                duringGesture();
            }
        });
    loop.exec();
    watchdog.stop();
    QApplication::processEvents();
    return outcome;
}

std::optional<QPoint> resourceTabDropPoint(
    QMainWindow* const workspace,
    QTabBar* const tabBar)
{
    if ( !workspace || !tabBar )
    {
        return std::nullopt;
    }
    const QRect tabBarOnScreen(tabBar->mapToGlobal(QPoint{}), tabBar->size());
    QWidget* const center = workspace->centralWidget();
    if ( center )
    {
        const QPoint candidate = center->mapToGlobal(center->rect().center());
        if ( !tabBarOnScreen.contains(candidate) )
        {
            return candidate;
        }
    }
    const QPoint candidate = workspace->mapToGlobal(
        QPoint(workspace->width() / 2, workspace->height() / 2));
    return tabBarOnScreen.contains(candidate) ? std::nullopt : std::optional<QPoint>(candidate);
}

void testDocumentTabJoinFeedbackAcrossAllKinds()
{
    struct Fixture
    {
        const char* action{};
        const char* primaryTabs{};
        const char* label{};
        ui::resources::ResourceAuthoringKind kind{};
        bool cog{};
    };
    const std::array<Fixture, 4> fixtures{
        Fixture{ "tools.resource_editors.sprite_spr.new", "resourceEditorTabs",
            "Join Sprite file tabs", ui::resources::ResourceAuthoringKind::sprite, false },
        Fixture{ "tools.resource_editors.sound_class_snd.new", "soundClassResourceEditorTabs",
            "Join Sound Class file tabs", ui::resources::ResourceAuthoringKind::soundClass, false },
        Fixture{ "tools.resource_editors.string_table_uni.new", "uniResourceEditorTabs",
            "Join UNI file tabs", ui::resources::ResourceAuthoringKind::stringTable, false },
        Fixture{ "tools.cog_script_editor", "cogScriptEditorTabs",
            "Join COG file tabs", ui::resources::ResourceAuthoringKind::sprite, true } };
    QScreen* const screen = QGuiApplication::primaryScreen();
    check(screen != nullptr, "document-tab feedback fixture has an available display");
    if ( !screen )
    {
        return;
    }
    const QRect available = screen->availableGeometry();

    for ( const Fixture& fixture : fixtures )
    {
        MainWindow window(nullptr, true);
        window.setProjectTypeForTest(ProjectType::JKDF2);
        const QSize maximumSize(std::max(1, available.width() - 40), std::max(1, available.height() - 80));
        window.setMinimumSize(window.minimumSize().boundedTo(maximumSize));
        window.resize(QSize(1500, 900).boundedTo(maximumSize));
        window.move(available.topLeft() + QPoint(20, 20));
        QMainWindow* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
        window.show();
        window.raise();
        window.activateWindow();
        QApplication::processEvents();

        if ( fixture.cog )
        {
            window.createNewCogScriptForTest(QStringLiteral("join_feedback_source.cog"));
            window.createNewCogScriptForTest(QStringLiteral("join_feedback_target.cog"));
        }
        else
        {
            QAction* const create = window.findChild<QAction*>(QString::fromLatin1(fixture.action));
            createResourceDraftFromGroup(create);
            createResourceDraftFromGroup(create);
        }
        QTabWidget* const primary = window.findChild<QTabWidget*>(QString::fromLatin1(fixture.primaryTabs));
        QDockWidget* const primaryDock = dockForResourceTabHost(primary);
        check(workspace && primary && primaryDock && primary->count() == 2,
            "document-tab feedback fixture creates two real documents in the primary host");
        if ( !workspace || !primary || !primaryDock || primary->count() != 2 )
        {
            continue;
        }

        QPointer<QWidget> targetPage(primary->widget(0));
        QPointer<QWidget> sourcePage(primary->widget(1));
        if ( fixture.cog )
        {
            check(window.triggerCogScriptTabContextActionForTest(primary->indexOf(targetPage.data()),
                    QStringLiteral("Pin tab")),
                "COG join-feedback fixture pins the destination before its file-tab transfer");
            QPlainTextEdit* const editor = sourcePage ? sourcePage->findChild<QPlainTextEdit*>() : nullptr;
            check(editor != nullptr, "COG join-feedback fixture exposes its real source editor");
            if ( !editor )
            {
                continue;
            }
            QTextCursor cursor(editor->document());
            cursor.movePosition(QTextCursor::End);
            cursor.insertText(QStringLiteral("\n# feedback history checkpoint\n"));
            cursor.insertText(QStringLiteral("# feedback redo branch\n"));
            editor->undo();
            check(editor->document()->isRedoAvailable(),
                "COG join-feedback fixture retains an authored source checkpoint and Redo branch");
        }
        else if ( fixture.kind == ui::resources::ResourceAuthoringKind::sprite )
        {
            QLineEdit* const material = sourcePage->findChild<QLineEdit*>(QStringLiteral("spr.material"));
            check(material != nullptr, "Sprite join-feedback fixture exposes its real source field");
            if ( !material )
            {
                continue;
            }
            material->setText(QStringLiteral("feedback_checkpoint.mat"));
            material->setText(QStringLiteral("feedback_redo.mat"));
            (void)ui::resources::resourceAuthoringPaneDocumentUndoRedo(sourcePage.data(), false);
        }
        else if ( fixture.kind == ui::resources::ResourceAuthoringKind::soundClass )
        {
            restoreSoundRows(qobject_cast<QDialog*>(sourcePage.data()), {
                { QStringLiteral("moving"), QStringLiteral("feedback.wav"), QStringLiteral("0x00000040"),
                    QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("1") } });
            QTreeWidget* const tree = sourcePage->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
            const QList<QTreeWidgetItem*> rows = soundAlternativeItems(tree);
            check(rows.size() == 1, "Sound Class join-feedback fixture exposes one real source row");
            if ( rows.size() != 1 )
            {
                continue;
            }
            rows.front()->setText(1, QStringLiteral("feedback_checkpoint.wav"));
            rows.front()->setText(1, QStringLiteral("feedback_redo.wav"));
            (void)ui::resources::resourceAuthoringPaneDocumentUndoRedo(sourcePage.data(), false);
        }
        else
        {
            authorStringTableRow(qobject_cast<QDialog*>(sourcePage.data()),
                QStringLiteral("JOIN_FEEDBACK"), QStringLiteral("checkpoint"), QStringLiteral("0"));
            QTableWidget* const table = sourcePage->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
            check(table && table->rowCount() == 1 && table->item(0, 1),
                "UNI join-feedback fixture exposes its real source row");
            if ( !table || table->rowCount() != 1 || !table->item(0, 1) )
            {
                continue;
            }
            table->item(0, 1)->setText(QStringLiteral("redo branch"));
            (void)ui::resources::resourceAuthoringPaneDocumentUndoRedo(sourcePage.data(), false);
        }
        QApplication::processEvents();

        primary->setCurrentWidget(targetPage.data());
        const std::optional<QPoint> outside = resourceTabDropPoint(workspace, primary->tabBar());
        check(outside.has_value(), "document-tab feedback fixture has a genuine detach point outside its source host");
        if ( !outside )
        {
            continue;
        }
        const TabDragOutcome detached = sendTabDrag(primary->tabBar(),
            primary->tabBar()->tabRect(primary->indexOf(targetPage.data())).center(), *outside, false);
        const auto tabHostForPage = [&window](QWidget* const page) -> QTabWidget*
        {
            for ( QTabWidget* const tabs : window.findChildren<QTabWidget*>() )
            {
                if ( tabs->indexOf(page) >= 0 )
                {
                    return tabs;
                }
            }
            return nullptr;
        };
        QPointer<QTabWidget> targetTabs(tabHostForPage(targetPage.data()));
        QPointer<QDockWidget> targetDock(dockForResourceTabHost(targetTabs.data()));
        check(detached.terminalEventsDispatched && !detached.timedOut && targetTabs && targetTabs != primary
                && targetDock && targetDock->isFloating() && primary->count() == 1 && targetTabs->count() == 1,
            "document-tab feedback fixture creates a real same-kind destination host by dragging a file tab");
        if ( !targetTabs || targetTabs == primary || !targetDock || !sourcePage || !targetPage )
        {
            continue;
        }

        if ( !fixture.cog )
        {
            check(ui::resources::resourceAuthoringPaneIsModified(sourcePage.data())
                    && ui::resources::resourceAuthoringPaneCanUndoRedo(sourcePage.data(), true),
                "resource join-feedback fixture retains an exact dirty draft with a Redo branch");
        }
        primary->setCurrentWidget(sourcePage.data());
        targetTabs->setCurrentWidget(targetPage.data());
        primaryDock->show();
        targetDock->show();
        targetDock->raise();
        targetDock->activateWindow();
        QApplication::processEvents();

        const auto captureCogPages = [&window]()
        {
            QCborMap state = QCborValue::fromCbor(window.captureCogEditorRecoveryStateForTest()).toMap();
            state.remove(QStringLiteral("active"));
            state.remove(QStringLiteral("hosts"));
            return ui::recovery::serializeCanonicalCbor(QCborValue(state));
        };
        const QByteArray sourceCogBefore = fixture.cog ? captureCogPages() : QByteArray{};
        const QCborMap sourceResourceBefore = fixture.cog ? QCborMap{}
            : authoredDraftState(ui::resources::captureResourceAuthoringPaneState(sourcePage.data()));
        const QCborMap targetResourceBefore = fixture.cog ? QCborMap{}
            : authoredDraftState(ui::resources::captureResourceAuthoringPaneState(targetPage.data()));
        const QString sourceIdentity = fixture.cog ? QString{}
            : ui::resources::resourceAuthoringPaneIdentity(sourcePage.data());
        const QString targetIdentity = fixture.cog ? QString{}
            : ui::resources::resourceAuthoringPaneIdentity(targetPage.data());
        const QImage beforePaint = targetTabs->grab().toImage();
        const QPoint validPoint = targetTabs->tabBar()->mapToGlobal(
            targetTabs->tabBar()->tabRect(0).center());
        std::optional<QPoint> invalidPoint{};
        const QRect primaryBounds(primaryDock->mapToGlobal(QPoint{}), primaryDock->size());
        const QRect targetBounds = targetDock->isFloating() ? targetDock->frameGeometry()
            : QRect(targetDock->mapToGlobal(QPoint{}), targetDock->size());
        const std::array<QPoint, 4> outsideCandidates{
            available.topLeft() + QPoint(8, 8), available.topRight() + QPoint(-8, 8),
            available.bottomLeft() + QPoint(8, -8), available.bottomRight() + QPoint(-8, -8) };
        for ( const QPoint& candidate : outsideCandidates )
        {
            if ( !primaryBounds.contains(candidate) && !targetBounds.contains(candidate) )
            {
                invalidPoint = candidate;
                break;
            }
        }
        check(invalidPoint.has_value(),
            "document-tab feedback fixture finds a point outside every registered same-kind dock rectangle");
        if ( !invalidPoint )
        {
            continue;
        }
        bool validFeedbackObserved = false;
        bool invalidFeedbackCleared = false;
        bool feedbackReentered = false;
        QPointer<QWidget> activeAtMove{};
        int observedPaintPixelCount{};
        bool observedPreviewParentMatch{};
        bool observedMousePassThrough{};
        QString observedPreviewText{};
        QString observedMouseHit{};
        QString observedValidPointAncestry{};
        QString observedOverlayAncestry{};
        QRect observedPreviewGeometry{};
        bool observedPreviewVisible{};
        qreal observedBeforePaintDpr{};
        qreal observedDuringPaintDpr{};
        const auto widgetAncestry = [](QWidget* widget)
        {
            QString ancestry{};
            for ( ; widget; widget = widget->parentWidget() )
            {
                if ( !ancestry.isEmpty() )
                {
                    ancestry += QStringLiteral(" <- ");
                }
                ancestry += widget->objectName() + QLatin1Char(':')
                    + QString::fromLatin1(widget->metaObject()->className());
            }
            return ancestry;
        };
        const auto dockVisibleRegion = [](QDockWidget* dock)
        {
            if ( !dock )
            {
                return QStringLiteral("<null>");
            }
            const QRegion visible = dock->visibleRegion();
            const QRect bounds = visible.boundingRect();
            return QStringLiteral("visible=%1 region=%2,%3,%4,%5 pieces=%6")
                .arg(dock->isVisible()).arg(bounds.x()).arg(bounds.y())
                .arg(bounds.width()).arg(bounds.height()).arg(visible.rectCount());
        };
        const auto sendMove = [bar = primary->tabBar()](const QPoint globalPosition)
        {
            const QPoint local = bar->mapFromGlobal(globalPosition);
            QMouseEvent move(QEvent::MouseMove, QPointF(local), QPointF(local), QPointF(globalPosition),
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(bar, &move);
        };
        const TabDragOutcome canceled = sendTabDrag(primary->tabBar(),
            primary->tabBar()->tabRect(0).center(), validPoint, true, nullptr, false,
            [&]()
            {
                QPointer<QLabel> preview(targetTabs->findChild<QLabel*>(QStringLiteral("documentTabJoinPreview")));
                bool painted = false;
                bool passesMouse = false;
                if ( preview )
                {
                    const QRect overlayRect = preview->geometry().intersected(targetTabs->rect());
                    const QRect tabBarRect = targetTabs->tabBar()->geometry();
                    const QImage duringPaint = targetTabs->grab().toImage();
                    observedBeforePaintDpr = beforePaint.devicePixelRatio();
                    observedDuringPaintDpr = duringPaint.devicePixelRatio();
                    int changedPixels = 0;
                    for ( int y = overlayRect.top(); y <= overlayRect.bottom(); ++y )
                    {
                        for ( int x = overlayRect.left(); x <= overlayRect.right(); ++x )
                        {
                            const int beforePixelX = static_cast<int>(std::floor(
                                (static_cast<qreal>(x) + 0.5) * observedBeforePaintDpr));
                            const int beforePixelY = static_cast<int>(std::floor(
                                (static_cast<qreal>(y) + 0.5) * observedBeforePaintDpr));
                            const int duringPixelX = static_cast<int>(std::floor(
                                (static_cast<qreal>(x) + 0.5) * observedDuringPaintDpr));
                            const int duringPixelY = static_cast<int>(std::floor(
                                (static_cast<qreal>(y) + 0.5) * observedDuringPaintDpr));
                            if ( beforePixelX >= 0 && beforePixelY >= 0
                                && beforePixelX < beforePaint.width() && beforePixelY < beforePaint.height()
                                && duringPixelX >= 0 && duringPixelY >= 0
                                && duringPixelX < duringPaint.width() && duringPixelY < duringPaint.height()
                                && beforePaint.pixel(beforePixelX, beforePixelY)
                                    != duringPaint.pixel(duringPixelX, duringPixelY) )
                            {
                                ++changedPixels;
                            }
                        }
                    }
                    const QPoint overlayCenter = preview->mapToGlobal(preview->rect().center());
                    QWidget* const hit = targetTabs->childAt(targetTabs->mapFromGlobal(overlayCenter));
                    passesMouse = hit && (hit == targetTabs->tabBar() || targetTabs->tabBar()->isAncestorOf(hit));
                    painted = !overlayRect.isEmpty() && changedPixels > 8;
                    observedPaintPixelCount = changedPixels;
                    observedPreviewParentMatch = preview->parentWidget() == targetTabs.data();
                    observedMousePassThrough = passesMouse;
                    observedPreviewText = preview->text();
                    observedPreviewGeometry = preview->geometry();
                    observedPreviewVisible = preview->isVisible();
                    observedMouseHit = hit
                        ? hit->objectName() + QLatin1Char(':')
                            + QString::fromLatin1(hit->metaObject()->className())
                        : QStringLiteral("<null>");
                    observedValidPointAncestry = widgetAncestry(QApplication::widgetAt(validPoint));
                    observedOverlayAncestry = widgetAncestry(QApplication::widgetAt(overlayCenter));
                    validFeedbackObserved = preview->isVisible()
                        && observedPreviewParentMatch
                        && preview->text() == QString::fromLatin1(fixture.label)
                        && preview->geometry().intersects(tabBarRect)
                        && preview->testAttribute(Qt::WA_TransparentForMouseEvents)
                        && preview->focusPolicy() == Qt::NoFocus && !preview->hasFocus()
                        && painted && passesMouse
                        && activeAtMove && QApplication::activeWindow() == activeAtMove.data();
                }
                if ( !validFeedbackObserved )
                {
                    QWidget* const activeWindow = QApplication::activeWindow();
                    std::cerr << "document-tab join feedback diagnostic: kind=" << static_cast<int>(fixture.kind)
                        << " labelPresent=" << static_cast<bool>(preview)
                        << " expectedLabel=" << fixture.label
                        << " labelText=" << observedPreviewText.toStdString()
                        << " visible=" << observedPreviewVisible
                        << " geometry=" << observedPreviewGeometry.x() << ',' << observedPreviewGeometry.y()
                        << ',' << observedPreviewGeometry.width() << ',' << observedPreviewGeometry.height()
                        << " parentMatch=" << observedPreviewParentMatch
                        << " paintedPixels=" << observedPaintPixelCount
                        << " beforeDpr=" << observedBeforePaintDpr
                        << " duringDpr=" << observedDuringPaintDpr
                        << " mouseHit=" << observedMouseHit.toStdString()
                        << " mousePassThrough=" << observedMousePassThrough
                        << " activeAtMove=" << static_cast<const void*>(activeAtMove.data())
                        << " activeWindow=" << static_cast<const void*>(activeWindow)
                        << " sourceDock{" << dockVisibleRegion(primaryDock).toStdString() << '}'
                        << " targetDock{" << dockVisibleRegion(targetDock.data()).toStdString() << '}'
                        << " widgetAtValid=" << observedValidPointAncestry.toStdString()
                        << " widgetAtOverlay=" << observedOverlayAncestry.toStdString() << '\n';
                }
                check(validFeedbackObserved,
                    "a genuine held document-tab drag paints the exact same-kind join label over its visible target bar with mouse and focus pass-through");
                sendMove(*invalidPoint);
                invalidFeedbackCleared = !preview || !preview->isVisible();
                check(invalidFeedbackCleared,
                    "leaving the registered same-kind document-tab target clears its join feedback before release");
                sendMove(validPoint);
                QPointer<QLabel> reentered(targetTabs->findChild<QLabel*>(QStringLiteral("documentTabJoinPreview")));
                feedbackReentered = reentered && reentered->isVisible()
                    && reentered->text() == QString::fromLatin1(fixture.label);
            },
            [&activeAtMove]() { activeAtMove = QApplication::activeWindow(); });
        QApplication::processEvents();

        bool exactAfterCancel = primary->count() == 1 && primary->widget(0) == sourcePage.data()
            && primary->currentWidget() == sourcePage.data() && targetTabs->count() == 1
            && targetTabs->widget(0) == targetPage.data() && targetTabs->currentWidget() == targetPage.data();
        if ( fixture.cog )
        {
            exactAfterCancel = exactAfterCancel
                && captureCogPages() == sourceCogBefore
                && targetPage->property("cogPinned").toBool();
        }
        else
        {
            exactAfterCancel = exactAfterCancel
                && authoredDraftState(ui::resources::captureResourceAuthoringPaneState(sourcePage.data()))
                    == sourceResourceBefore
                && authoredDraftState(ui::resources::captureResourceAuthoringPaneState(targetPage.data()))
                    == targetResourceBefore
                && ui::resources::resourceAuthoringPaneIdentity(sourcePage.data()) == sourceIdentity
                && ui::resources::resourceAuthoringPaneIdentity(targetPage.data()) == targetIdentity
                && ui::resources::resourceAuthoringPaneIsModified(sourcePage.data());
        }
        const QLabel* const finalPreview = targetTabs->findChild<QLabel*>(QStringLiteral("documentTabJoinPreview"));
        check(canceled.terminalEventsDispatched && !canceled.timedOut && validFeedbackObserved
                && invalidFeedbackCleared && feedbackReentered && exactAfterCancel
                && (!finalPreview || !finalPreview->isVisible()),
            "Escape clears the re-entered tab-join feedback and preserves exact source, history, page order, pin and selected documents");
    }
}

void testDocumentTabJoinFeedbackOnOpenEmptyTarget()
{
    QScreen* const screen = QGuiApplication::primaryScreen();
    check(screen != nullptr, "empty-target join feedback fixture has an available display");
    if ( !screen )
    {
        return;
    }
    const QRect available = screen->availableGeometry();
    MainWindow window(nullptr, true);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    const QSize maximumSize(std::max(1, available.width() - 40), std::max(1, available.height() - 80));
    window.setMinimumSize(window.minimumSize().boundedTo(maximumSize));
    window.resize(QSize(1500, 900).boundedTo(maximumSize));
    window.move(available.topLeft() + QPoint(20, 20));
    QMainWindow* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();

    QAction* const create = window.findChild<QAction*>(QStringLiteral("tools.resource_editors.sprite_spr.new"));
    createResourceDraftFromGroup(create);
    QTabWidget* const primary = window.findChild<QTabWidget*>(QStringLiteral("resourceEditorTabs"));
    QDockWidget* const primaryDock = dockForResourceTabHost(primary);
    QPointer<QWidget> page(primary ? primary->currentWidget() : nullptr);
    QLineEdit* const material = page ? page->findChild<QLineEdit*>(QStringLiteral("spr.material")) : nullptr;
    check(workspace && primary && primaryDock && page && material && primary->count() == 1,
        "empty-target join feedback fixture opens a real Sprite document in the primary host");
    if ( !workspace || !primary || !primaryDock || !page || !material || primary->count() != 1 )
    {
        return;
    }

    material->setText(QStringLiteral("empty_target_checkpoint.mat"));
    material->setText(QStringLiteral("empty_target_redo.mat"));
    (void)ui::resources::resourceAuthoringPaneDocumentUndoRedo(page.data(), false);
    material->setSelection(0, 6);
    QApplication::processEvents();
    const auto outside = resourceTabDropPoint(workspace, primary->tabBar());
    check(outside.has_value(), "empty-target join feedback fixture finds a real on-screen detach point");
    if ( !outside )
    {
        return;
    }
    const TabDragOutcome detached = sendTabDrag(primary->tabBar(),
        primary->tabBar()->tabRect(primary->indexOf(page.data())).center(), *outside, false);
    QPointer<QTabWidget> sourceTabs(resourceTabHostContaining(workspace, page.data()));
    QPointer<QDockWidget> sourceDock(dockForResourceTabHost(sourceTabs.data()));
    check(detached.terminalEventsDispatched && !detached.timedOut && sourceTabs && sourceTabs != primary
            && sourceDock && sourceDock->isFloating() && primary->count() == 0 && primaryDock->isHidden(),
        "the real file-tab drag leaves one live Sprite source and an empty hidden primary host");
    if ( !detached.terminalEventsDispatched || detached.timedOut || !sourceTabs || !sourceDock
        || sourceTabs == primary || primary->count() != 0 )
    {
        return;
    }

    const QCborMap sourceBefore = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(page.data()));
    const QString sourceIdentity = ui::resources::resourceAuthoringPaneIdentity(page.data());
    const QString materialText = material->text();
    const int cursorPosition = material->cursorPosition();
    const int selectionStart = material->selectionStart();
    const QString selectedText = material->selectedText();
    primaryDock->show();
    primaryDock->raise();
    QElapsedTimer targetDeadline{};
    targetDeadline.start();
    while ( (!primaryDock->isVisible() || primary->width() <= 0 || primary->height() <= 0)
        && targetDeadline.elapsed() < 2000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    const QPoint emptyTargetPoint = primary->mapToGlobal(primary->rect().center());
    const QTabBar* const emptyBar = primary->tabBar();
    const bool emptyTargetOpen = primaryDock->isVisible() && primary->count() == 0
        && emptyBar && (!emptyBar->isVisible() || emptyBar->size().isEmpty())
        && QRect(primary->mapToGlobal(QPoint{}), primary->size()).contains(emptyTargetPoint);
    check(emptyTargetOpen,
        "the primary resource host reopens as a visible empty target whose file tab bar is hidden or zero-sized");
    if ( !emptyTargetOpen )
    {
        return;
    }

    QPointer<QLabel> firstPreview{};
    QPointer<QLabel> reopenedPreview{};
    bool emptyTargetFeedback = false;
    bool targetHideClearedFeedback = false;
    bool targetReshowRestoredFeedback = false;
    const auto moveHeldPointer = [&sourceTabs, &emptyTargetPoint]()
    {
        QTabBar* const bar = sourceTabs ? sourceTabs->tabBar() : nullptr;
        if ( !bar )
        {
            return;
        }
        const QPoint local = bar->mapFromGlobal(emptyTargetPoint);
        QMouseEvent move(QEvent::MouseMove, QPointF(local), QPointF(local),
            QPointF(emptyTargetPoint), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(bar, &move);
    };
    const TabDragOutcome canceled = sendTabDrag(sourceTabs->tabBar(),
        sourceTabs->tabBar()->tabRect(sourceTabs->indexOf(page.data())).center(),
        emptyTargetPoint, true, nullptr, false,
        [&]()
        {
            firstPreview = primary->findChild<QLabel*>(QStringLiteral("documentTabJoinPreview"));
            emptyTargetFeedback = firstPreview && firstPreview->isVisible()
                && firstPreview->parentWidget() == primary
                && firstPreview->text() == QStringLiteral("Join Sprite file tabs")
                && firstPreview->testAttribute(Qt::WA_TransparentForMouseEvents)
                && firstPreview->focusPolicy() == Qt::NoFocus
                && firstPreview->geometry().intersects(primary->rect())
                && sourceTabs->count() == 1 && sourceTabs->currentWidget() == page.data()
                && primary->count() == 0;
            check(emptyTargetFeedback,
                "a held Sprite file-tab drag paints mouse-transparent Join feedback over the visible empty target");
            if ( !firstPreview )
            {
                return;
            }

            primaryDock->close();
            targetHideClearedFeedback = primaryDock->isHidden() && !firstPreview
                && !primary->findChild<QLabel*>(QStringLiteral("documentTabJoinPreview"));
            check(targetHideClearedFeedback,
                "closing the empty target synchronously clears its held-gesture feedback");
            primaryDock->show();
            primaryDock->raise();
            moveHeldPointer();
            reopenedPreview = primary->findChild<QLabel*>(QStringLiteral("documentTabJoinPreview"));
            targetReshowRestoredFeedback = primaryDock->isVisible() && primary->count() == 0
                && reopenedPreview && reopenedPreview->isVisible()
                && reopenedPreview->text() == QStringLiteral("Join Sprite file tabs");
        });
    QApplication::processEvents();

    const QCborMap sourceAfter = page
        ? authoredDraftState(ui::resources::captureResourceAuthoringPaneState(page.data())) : QCborMap{};
    const bool sourceExact = page && sourceTabs && sourceTabs->count() == 1
        && sourceTabs->widget(0) == page.data() && sourceTabs->currentWidget() == page.data()
        && ui::resources::resourceAuthoringPaneIdentity(page.data()) == sourceIdentity
        && sourceAfter == sourceBefore
        && material->text() == materialText && material->cursorPosition() == cursorPosition
        && material->selectionStart() == selectionStart && material->selectedText() == selectedText
        && ui::resources::resourceAuthoringPaneIsModified(page.data());
    check(canceled.terminalEventsDispatched && !canceled.timedOut && emptyTargetFeedback
            && targetHideClearedFeedback && targetReshowRestoredFeedback && sourceExact
            && primary->count() == 0 && primaryDock->isVisible()
            && !primary->findChild<QLabel*>(QStringLiteral("documentTabJoinPreview")),
        "hiding and reopening an empty feedback target preserves the source page, order, raw draft, cursor, selection, and history through Escape");
}

void testSpritePreviewSurvivesRepeatedFloatAndRedock()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const std::vector<QColor> redPixels(4, QColor(240, 24, 32));
    const QByteArray redMat = makeRgbaMatCels({ { 2, redPixels } });
    const bool fixtureReady = temporaryDirectory.isValid()
        && writeFile(root / "mat" / "red.mat",
            std::string_view(redMat.constData(), static_cast<std::size_t>(redMat.size())));
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    window.setResourcePathsForTest({ root }, {});
    QMainWindow* const workspace = window.findChild<QMainWindow*>(
        QStringLiteral("workspaceWindow"));
    if ( workspace )
    {
        workspace->setAnimated(false);
    }
    window.resize(1280, 860);
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();

    QAction* const createSprite = window.findChild<QAction*>(
        QStringLiteral("tools.resource_editors.sprite_spr.new"));
    check(fixtureReady && workspace && createSprite,
        "SPR reparent fixture writes one exact MAT and resolves the direct Sprite New command");
    if ( !fixtureReady || !workspace || !createSprite )
    {
        return;
    }

    createResourceDraftFromGroup(createSprite);
    QTabWidget* const primaryTabs = window.findChild<QTabWidget*>(
        QStringLiteral("resourceEditorTabs"));
    QWidget* const spritePane = primaryTabs ? primaryTabs->currentWidget() : nullptr;
    QLineEdit* const material = spritePane
        ? spritePane->findChild<QLineEdit*>(QStringLiteral("spr.material"))
        : nullptr;
    QCheckBox* const bounds = spritePane
        ? spritePane->findChild<QCheckBox*>(QStringLiteral("spr.preview.bounds"))
        : nullptr;
    QSlider* const fps = spritePane
        ? spritePane->findChild<QSlider*>(QStringLiteral("spr.preview.fps"))
        : nullptr;
    QComboBox* const light = spritePane
        ? spritePane->findChild<QComboBox*>(QStringLiteral("spr.light_mode"))
        : nullptr;
    const int fullyLitIndex = light ? light->findData(0) : -1;
    GlView* const preview = spritePane
        ? spritePane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"))
        : nullptr;
    const QPointer<GlView> guardedPreview(preview);
    const QPointer<QWidget> guardedSpritePane(spritePane);
    check(primaryTabs && spritePane && material && bounds && fps && light && fullyLitIndex >= 0 && preview,
        "SPR reparent fixture creates the real preview pane and its stable material/playback controls");
    if ( !primaryTabs || !spritePane || !material || !bounds || !fps || !light || fullyLitIndex < 0 || !preview )
    {
        return;
    }

    material->setText(QStringLiteral("red.mat"));
    light->setCurrentIndex(fullyLitIndex);
    bounds->setChecked(true);
    fps->setValue(0);
    createResourceDraftFromGroup(createSprite);
    QDockWidget* const primaryDock = dockForResourceTabHost(primaryTabs);
    check(primaryTabs->count() == 2 && primaryDock,
        "SPR reparent fixture retains a sibling tab in the primary dock during floating transitions");
    if ( primaryTabs->count() != 2 || !primaryDock )
    {
        return;
    }

    const auto waitForRedFrame = [guardedPreview]()
    {
        QElapsedTimer deadline{};
        deadline.start();
        while ( deadline.elapsed() < 4000 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
            if ( guardedPreview && guardedPreview->isValid() )
            {
                const QImage frame = guardedPreview->grabFramebuffer();
                if ( imageHasDominantPixels(frame, false)
                    && spriteInteriorMatchesTexel(*guardedPreview, frame, false) )
                {
                    return true;
                }
            }
        }
        return false;
    };
    const bool initialFrameRendered = waitForRedFrame();
    const QCborMap authoredBefore = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(guardedSpritePane.data()));
    check(initialFrameRendered && guardedPreview
            && imageHasDominantPixels(guardedPreview->grabFramebuffer(), false)
            && spriteInteriorMatchesTexel(*guardedPreview, guardedPreview->grabFramebuffer(), false),
        "SPR preview renders the fixed red pixel fixture before any host reparenting");
    if ( !initialFrameRendered )
    {
        return;
    }

    bool everyCyclePreserved = true;
    for ( int cycle = 0; cycle < 2; ++cycle )
    {
        const auto outsidePoint = resourceTabDropPoint(
            workspace, primaryTabs->tabBar());
        if ( !outsidePoint )
        {
            everyCyclePreserved = false;
            break;
        }
        const int sourceIndex = primaryTabs->indexOf(guardedSpritePane.data());
        if ( sourceIndex < 0 )
        {
            everyCyclePreserved = false;
            break;
        }
        const TabDragOutcome floatDrag = sendTabDrag(
            primaryTabs->tabBar(),
            primaryTabs->tabBar()->tabRect(sourceIndex).center(),
            *outsidePoint,
            false);
        QTabWidget* const floatingTabs = resourceTabHostContaining(
            workspace, guardedSpritePane.data());
        QDockWidget* const floatingDock = dockForResourceTabHost(floatingTabs);
        const bool floatingState = floatDrag.terminalEventsDispatched
            && !floatDrag.timedOut
            && floatingTabs && floatingTabs != primaryTabs
            && floatingDock && floatingDock->isFloating()
            && guardedPreview && guardedPreview.data() == preview
            && waitForRedFrame();
        const QCborMap floatingAuthored = guardedSpritePane
            ? authoredDraftState(ui::resources::captureResourceAuthoringPaneState(
                guardedSpritePane.data()))
            : QCborMap{};
        const QCborMap floatingPresentation = guardedSpritePane
            ? ui::resources::captureResourceAuthoringPaneState(
                guardedSpritePane.data()).value(QStringLiteral("presentation")).toMap()
            : QCborMap{};
        everyCyclePreserved = everyCyclePreserved && floatingState
            && floatingAuthored == authoredBefore
            && floatingPresentation.value(QStringLiteral("bounds")).toBool()
            && floatingPresentation.value(QStringLiteral("fps")).toInteger() == 0;
        if ( !floatingState || !floatingTabs || !floatingDock )
        {
            break;
        }

        const QPoint redockPoint = primaryDock->mapToGlobal(
            primaryDock->rect().center());
        const int floatingIndex = floatingTabs->indexOf(guardedSpritePane.data());
        if ( floatingIndex < 0 )
        {
            everyCyclePreserved = false;
            break;
        }
        const TabDragOutcome redockDrag = sendTabDrag(
            floatingTabs->tabBar(),
            floatingTabs->tabBar()->tabRect(floatingIndex).center(),
            redockPoint,
            false);
        const QTabWidget* const returnedHost = resourceTabHostContaining(
            workspace, guardedSpritePane.data());
        everyCyclePreserved = everyCyclePreserved
            && redockDrag.terminalEventsDispatched
            && !redockDrag.timedOut
            && returnedHost == primaryTabs
            && primaryTabs->indexOf(guardedSpritePane.data()) >= 0
            && guardedPreview && guardedPreview.data() == preview
            && waitForRedFrame()
            && authoredDraftState(
                   ui::resources::captureResourceAuthoringPaneState(
                       guardedSpritePane.data()))
                == authoredBefore
            && ui::resources::captureResourceAuthoringPaneState(
                   guardedSpritePane.data())
                   .value(QStringLiteral("presentation")).toMap()
                   .value(QStringLiteral("bounds")).toBool();
    }

    check(everyCyclePreserved
            && guardedSpritePane
            && resourceTabHostContaining(workspace, guardedSpritePane.data()) == primaryTabs
            && primaryTabs->indexOf(guardedSpritePane.data()) >= 0
            && guardedPreview
            && imageHasDominantPixels(guardedPreview->grabFramebuffer(), false)
            && spriteInteriorMatchesTexel(*guardedPreview, guardedPreview->grabFramebuffer(), false),
        "repeated embedded-to-floating-to-embedded Sprite reparenting recreates its GL context while retaining the same widget, exact draft, bounds state, and red MAT pixels");
}


void testResourceEditorTabDragTransferAndEscape()
{
    const auto makeWindow = []()
    {
        auto window = std::make_unique<MainWindow>(nullptr, true);
        window->setAnimated(false);
        if ( QMainWindow* const workspace = window->findChild<QMainWindow*>(
                QStringLiteral("workspaceWindow")) )
        {
            workspace->setAnimated(false);
        }
        window->resize(1280, 860);
        window->show();
        window->raise();
        window->activateWindow();
        QApplication::processEvents();
        QAction* const sprite = window->findChild<QAction*>(
            QStringLiteral("tools.resource_editors.sprite_spr.new"));
        if ( sprite )
        {
            createResourceDraftFromGroup(sprite);
        }
        QApplication::processEvents();
        return window;
    };

    const auto prepareTab = [](MainWindow* const window,
                               QMainWindow** workspaceOut,
                               QTabWidget** tabsOut,
                               QWidget** pageOut,
                               QLineEdit** materialOut) -> bool
    {
        if ( workspaceOut )
        {
            *workspaceOut = window
                ? window->findChild<QMainWindow*>(QStringLiteral("workspaceWindow"))
                : nullptr;
        }
        if ( tabsOut )
        {
            *tabsOut = window
                ? window->findChild<QTabWidget*>(QStringLiteral("resourceEditorTabs"))
                : nullptr;
        }
        if ( pageOut )
        {
            *pageOut = tabsOut && *tabsOut ? (*tabsOut)->currentWidget() : nullptr;
        }
        if ( materialOut )
        {
            *materialOut = pageOut && *pageOut
                ? (*pageOut)->findChild<QLineEdit*>(QStringLiteral("spr.material"))
                : nullptr;
        }
        if ( !window || !workspaceOut || !*workspaceOut || !tabsOut || !*tabsOut
            || !pageOut || !*pageOut || !materialOut || !*materialOut )
        {
            return false;
        }
        (*materialOut)->setText(QStringLiteral("raw transfer draft.mat"));
        return true;
    };

    {
        std::unique_ptr<MainWindow> window = makeWindow();
        QMainWindow* workspace = nullptr;
        QTabWidget* primaryTabs = nullptr;
        QWidget* page = nullptr;
        QLineEdit* material = nullptr;
        const bool prepared = prepareTab(
            window.get(), &workspace, &primaryTabs, &page, &material);
        check(prepared,
            "resource tab transfer fixture opens a visible dirty Sprite tab");
        if ( prepared )
        {
            QTemporaryDir saveDirectory{};
            check(saveDirectory.isValid(),
                "detached Save fixture owns an isolated loose-file directory");
            if ( !saveDirectory.isValid() )
            {
                return;
            }
            const std::filesystem::path savedResourcePath = temporaryRoot(saveDirectory)
                / "detached_save_baseline.spr";
            material->setText(QStringLiteral("transfer_seed.mat"));
            BoundedModalSequence initialSave({ fileDialogStep(savedResourcePath) });
            initialSave.start();
            const bool initialSaveSucceeded =
                ui::resources::resourceAuthoringPaneSave(page, false);
            initialSave.stop();
            const std::optional<std::string> savedResourceBytes = readFile(savedResourcePath);
            check(saveDirectory.isValid() && initialSaveSucceeded
                    && initialSave.completedSteps() == 1 && !initialSave.timedOut()
                    && savedResourceBytes.has_value(),
                "detached Save fixture establishes a real loose file before its invalid draft");
            if ( !saveDirectory.isValid() || !initialSaveSucceeded
                || !savedResourceBytes.has_value() )
            {
                return;
            }
            material->setText(QStringLiteral("raw transfer draft.mat"));
            QTabBar* const tabBar = primaryTabs->tabBar();
            QDockWidget* const originalDock = dockForResourceTabHost(primaryTabs);
            const int originalIndex = primaryTabs->indexOf(page);
            const QCborMap authoredBefore = authoredDraftState(
                ui::resources::captureResourceAuthoringPaneState(page));
            const auto dropPoint = resourceTabDropPoint(workspace, tabBar);
            check(tabBar && originalDock && originalIndex >= 0 && dropPoint.has_value(),
                "resource tab transfer fixture has a docked tab and a real outside-host release point");
            if ( tabBar && originalDock && originalIndex >= 0 && dropPoint )
            {
                const QRect tabRect = tabBar->tabRect(originalIndex);
                const TabDragOutcome drag = sendTabDrag(
                    tabBar, tabRect.center(), *dropPoint, false);
                QTabWidget* const transferredHost = resourceTabHostContaining(workspace, page);
                QDockWidget* const transferredDock = dockForResourceTabHost(transferredHost);
                const auto hosts = resourceTabHosts(workspace);
                check(drag.terminalEventsDispatched && !drag.timedOut
                        && transferredHost && transferredHost != primaryTabs
                        && transferredDock && transferredDock != originalDock
                        && transferredDock->isFloating()
                        && primaryTabs->count() == 0
                        && transferredHost->indexOf(page) >= 0,
                    "resource tab drag outside a host floats a new dock and transfers the same pane");
                const QCborMap authoredAfter = authoredDraftState(
                    ui::resources::captureResourceAuthoringPaneState(page));
                check(drag.terminalEventsDispatched && !drag.timedOut
                        && authoredAfter == authoredBefore
                        && ui::resources::resourceAuthoringPaneIsModified(page)
                        && hosts.size() == 1,
                    "resource tab transfer preserves the exact raw draft and registers one live host");

                const QByteArray nativeSnapshot =
                    window->captureRecoveryEditorStateForTest();
                const QCborValue rootValue = QCborValue::fromCbor(nativeSnapshot);
                const QCborMap root = rootValue.toMap();
                const QCborMap resourceWorkspace = root.value(
                    QStringLiteral("resourceEditors")).toMap();
                const QCborArray records = resourceWorkspace.value(
                    QStringLiteral("tabs")).toArray();
                const bool membershipCaptured = records.size() == 1
                    && authoredDraftState(records.at(0).toMap().value(
                           QStringLiteral("page")).toMap()) == authoredAfter;
                check(membershipCaptured,
                    "native EDIT capture keeps the detached resource tab and raw draft in workspace membership");

                // A floating resource page remains the owner of Save shortcuts.
                if ( transferredHost && transferredDock
                    && transferredHost->indexOf(page) >= 0 )
                {
                    transferredDock->show();
                    transferredDock->raise();
                    transferredDock->activateWindow();
                    material->setFocus(Qt::OtherFocusReason);
                    QApplication::processEvents();
                    const bool detachedFocusEstablished =
                        QApplication::focusWidget() == material;
                    check(detachedFocusEstablished,
                        "the floating resource pane can retain keyboard focus in its authored material field");

                    QAction* const nativeSave = window->findChild<QAction*>(
                        QStringLiteral("file.save"));
                    int nativeSaveTriggers = 0;
                    if ( nativeSave )
                    {
                        QObject::connect(nativeSave, &QAction::triggered, window.get(),
                            [&nativeSaveTriggers]() { ++nativeSaveTriggers; });
                    }
                    bool sawResourceSaveDialog = false;
                    bool exactValidationMessage = false;
                    const QCborMap draftBeforeFailedSave = authoredDraftState(
                        ui::resources::captureResourceAuthoringPaneState(page));
                    ModalStep cancelSave{};
                    cancelSave.matches = [](QDialog* dialog)
                    {
                        return dialog->objectName()
                            == QStringLiteral("resource_authoring.error");
                    };
                    cancelSave.interact = [&sawResourceSaveDialog, &exactValidationMessage](QDialog* dialog)
                    {
                        sawResourceSaveDialog = true;
                        QMessageBox* const message = qobject_cast<QMessageBox*>(dialog);
                        exactValidationMessage = message
                            && message->windowTitle() == QStringLiteral("Resource validation failed")
                            && message->text() == QStringLiteral("SPR material must be a safe resource token.");
                        dialog->reject();
                    };
                    BoundedModalSequence saveModal({ std::move(cancelSave) }, 2000);
                    saveModal.start();
                    if ( detachedFocusEstablished )
                    {
                        QKeyEvent savePress(
                            QEvent::KeyPress,
                            Qt::Key_S,
                            Qt::ControlModifier);
                        QApplication::sendEvent(material, &savePress);
                        QKeyEvent saveRelease(
                            QEvent::KeyRelease,
                            Qt::Key_S,
                            Qt::ControlModifier);
                        QApplication::sendEvent(material, &saveRelease);
                    }
                    saveModal.stop();
                    check(nativeSave && nativeSave->isEnabled()
                            && detachedFocusEstablished
                            && saveModal.completedSteps() == 1
                            && !saveModal.timedOut()
                            && sawResourceSaveDialog
                            && exactValidationMessage && nativeSaveTriggers == 0
                            && readFile(savedResourcePath) == savedResourceBytes
                            && authoredDraftState(ui::resources::captureResourceAuthoringPaneState(page))
                                == draftBeforeFailedSave,
                        "Ctrl+S from a detached invalid SPR reports its exact resource error without Level Save, file replacement, or draft loss");
                }

                const QString detachedIdentity =
                    ui::resources::resourceAuthoringPaneIdentity(page);
                MainWindow restored(nullptr, true);
                QString restoreWarning{};
                const bool restoredSnapshot =
                    restored.restoreRecoveryEditorStateForTest(
                        nativeSnapshot, &restoreWarning);
                QMainWindow* const restoredWorkspace = restored.findChild<QMainWindow*>(
                    QStringLiteral("workspaceWindow"));
                QWidget* const restoredPage = resourcePageWithIdentity(
                    restoredWorkspace, detachedIdentity);
                QTabWidget* const restoredHost = resourceTabHostContaining(
                    restoredWorkspace, restoredPage);
                QDockWidget* const restoredDock = dockForResourceTabHost(restoredHost);
                check(restoredSnapshot && restoreWarning.isEmpty() && restoredPage
                        && authoredDraftState(
                               ui::resources::captureResourceAuthoringPaneState(restoredPage))
                            == authoredAfter,
                    "native EDIT restore retains a detached tab's identity and exact raw draft");
                check(restoredDock && restoredDock->isFloating(),
                    "native EDIT restore retains the detached host placement");

                if ( transferredHost && transferredDock && transferredHost->indexOf(page) >= 0 )
                {
                    QAbstractButton* const newButton = page->findChild<QAbstractButton*>(
                        QStringLiteral("resource_authoring.new"));
                    if ( newButton )
                    {
                        transferredHost->setCurrentWidget(page);
                        newButton->click();
                        QApplication::processEvents();
                    }
                    const bool siblingCreated = newButton
                        && transferredHost->count() == 2
                        && transferredHost->indexOf(page) >= 0
                        && authoredDraftState(
                               ui::resources::captureResourceAuthoringPaneState(page))
                            == authoredAfter;
                    check(siblingCreated,
                        "a detached resource host can create a sibling tab without changing the transferred draft");
                    originalDock->show();
                    originalDock->raise();
                    QApplication::processEvents();
                    const QPoint targetHostPoint = originalDock->mapToGlobal(
                        originalDock->rect().center());
                    QTabBar* const sourceBar = transferredHost->tabBar();
                    const int transferredIndex = transferredHost->indexOf(page);
                    const TabDragOutcome returnDrag = sourceBar && transferredIndex >= 0
                        ? sendTabDrag(
                              sourceBar,
                              sourceBar->tabRect(transferredIndex).center(),
                              targetHostPoint,
                              false)
                        : TabDragOutcome{};
                    check(returnDrag.terminalEventsDispatched && !returnDrag.timedOut
                            && resourceTabHostContaining(workspace, page) == primaryTabs
                            && primaryTabs->indexOf(page) >= 0
                            && transferredHost->count() == 1
                            && authoredDraftState(
                                   ui::resources::captureResourceAuthoringPaneState(page))
                                == authoredAfter,
                        "the same pane transfers back to another live host with its exact draft intact");
                }

            }
        }
    }

    {
        std::unique_ptr<MainWindow> window = makeWindow();
        QMainWindow* workspace = nullptr;
        QTabWidget* primaryTabs = nullptr;
        QWidget* page = nullptr;
        QLineEdit* material = nullptr;
        const bool prepared = prepareTab(
            window.get(), &workspace, &primaryTabs, &page, &material);
        check(prepared,
            "resource tab Escape fixture opens a visible dirty Sprite tab");
        if ( prepared )
        {
            QAction* const sprite = window->findChild<QAction*>(
                QStringLiteral("tools.resource_editors.sprite_spr.new"));
            if ( sprite )
            {
                createResourceDraftFromGroup(sprite);
                QApplication::processEvents();
            }
            QWidget* const secondPage = primaryTabs->count() > 1
                ? primaryTabs->widget(1)
                : nullptr;
            QTabBar* const tabBar = primaryTabs->tabBar();
            QDockWidget* const originalDock = dockForResourceTabHost(primaryTabs);
            const int originalIndex = primaryTabs->indexOf(page);
            const std::vector<QPointer<QWidget>> originalOrder{
                primaryTabs->count() > 0 ? primaryTabs->widget(0) : nullptr,
                primaryTabs->count() > 1 ? primaryTabs->widget(1) : nullptr};
            const QCborMap authoredBefore = authoredDraftState(
                ui::resources::captureResourceAuthoringPaneState(page));
            const auto dropPoint = resourceTabDropPoint(workspace, tabBar);
            primaryTabs->setCurrentWidget(page);
            material->setFocus(Qt::OtherFocusReason);
            QApplication::processEvents();
            const bool focusedChildReady = QApplication::focusWidget() == material;
            if ( tabBar && originalDock && originalIndex >= 0 && dropPoint
                && sprite && secondPage && focusedChildReady )
            {
                const TabDragOutcome drag = sendTabDrag(
                    tabBar,
                    tabBar->tabRect(originalIndex).center(),
                    *dropPoint,
                    true,
                    material);
                QTabWidget* const currentHost = resourceTabHostContaining(workspace, page);
                if ( !(drag.terminalEventsDispatched && !drag.timedOut
                        && drag.cancelTargetHadFocus && currentHost == primaryTabs
                        && currentHost && currentHost->indexOf(page) == originalIndex
                        && primaryTabs->count() == 2
                        && primaryTabs->widget(0) == originalOrder[0].data()
                        && primaryTabs->widget(1) == originalOrder[1].data()
                        && resourceTabHosts(workspace).size() == 1
                        && dockForResourceTabHost(currentHost) == originalDock) )
                {
                    std::cerr << "resource drag Escape: terminal=" << drag.terminalEventsDispatched
                        << " timeout=" << drag.timedOut << " focused=" << drag.cancelTargetHadFocus
                        << " sameHost=" << (currentHost == primaryTabs)
                        << " index=" << (currentHost ? currentHost->indexOf(page) : -1)
                        << " count=" << primaryTabs->count()
                        << " order0=" << (primaryTabs->widget(0) == originalOrder[0].data())
                        << " order1=" << (primaryTabs->widget(1) == originalOrder[1].data())
                        << " hosts=" << resourceTabHosts(workspace).size()
                        << " trace=" << tabBar->property("resource_authoring.dragTrace").toString().toStdString() << '\n';
                }
                check(drag.terminalEventsDispatched && !drag.timedOut
                        && drag.cancelTargetHadFocus
                        && currentHost == primaryTabs
                        && currentHost->indexOf(page) == originalIndex
                        && primaryTabs->count() == 2
                        && primaryTabs->widget(0) == originalOrder[0].data()
                        && primaryTabs->widget(1) == originalOrder[1].data()
                        && resourceTabHosts(workspace).size() == 1
                        && dockForResourceTabHost(currentHost) == originalDock,
                    "Escape from the focused resource field cancels drag and restores exact tab order");
                check(authoredDraftState(
                          ui::resources::captureResourceAuthoringPaneState(page)) == authoredBefore
                        && ui::resources::resourceAuthoringPaneIsModified(page),
                    "canceled resource-tab drag preserves the exact dirty raw draft");

                const TabDragOutcome deactivatedDrag = sendTabDrag(
                    tabBar,
                    tabBar->tabRect(originalIndex).center(),
                    *dropPoint,
                    false,
                    nullptr,
                    true);
                QTabWidget* const afterDeactivateHost =
                    resourceTabHostContaining(workspace, page);
                if ( !(deactivatedDrag.terminalEventsDispatched && !deactivatedDrag.timedOut
                        && afterDeactivateHost == primaryTabs && primaryTabs->count() == 2
                        && primaryTabs->widget(0) == originalOrder[0].data()
                        && primaryTabs->widget(1) == originalOrder[1].data()
                        && resourceTabHosts(workspace).size() == 1) )
                {
                    std::cerr << "resource drag deactivation: terminal=" << deactivatedDrag.terminalEventsDispatched
                        << " timeout=" << deactivatedDrag.timedOut
                        << " sameHost=" << (afterDeactivateHost == primaryTabs)
                        << " count=" << primaryTabs->count()
                        << " order0=" << (primaryTabs->widget(0) == originalOrder[0].data())
                        << " order1=" << (primaryTabs->widget(1) == originalOrder[1].data())
                        << " hosts=" << resourceTabHosts(workspace).size()
                        << " trace=" << tabBar->property("resource_authoring.dragTrace").toString().toStdString() << '\n';
                }
                check(deactivatedDrag.terminalEventsDispatched
                        && !deactivatedDrag.timedOut
                        && afterDeactivateHost == primaryTabs
                        && primaryTabs->count() == 2
                        && primaryTabs->widget(0) == originalOrder[0].data()
                        && primaryTabs->widget(1) == originalOrder[1].data()
                        && resourceTabHosts(workspace).size() == 1,
                    "window deactivation cancels an active drag and keeps both original tabs in order");
            }
            else
            {
                check(false,
                    "resource tab Escape fixture establishes focused child, two-tab order, and outside target");
            }
        }
    }
}

void testResourceEditorHostAdmissionAt64Hosts()
{
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    QMainWindow* const workspace = window.findChild<QMainWindow*>(
        QStringLiteral("workspaceWindow"));
    if ( workspace )
    {
        workspace->setAnimated(false);
    }
    window.resize(1280, 860);
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();

    QAction* const createUni = window.findChild<QAction*>(
        QStringLiteral("tools.resource_editors.string_table_uni.new"));
    check(workspace && createUni,
        "resource host-cap fixture has the workspace and direct UNI New command");
    if ( !workspace || !createUni )
    {
        return;
    }
    for ( int index = 0; index < 64; ++index )
    {
        createResourceDraftFromGroup(createUni);
    }

    QTabWidget* const primaryTabs = window.findChild<QTabWidget*>(
        QStringLiteral("uniResourceEditorTabs"));
    const bool maximumPaneCountCreated = primaryTabs && primaryTabs->count() == 64;
    check(maximumPaneCountCreated,
        "the maximum admitted resource pane count can occupy the primary host before detaching");
    if ( !maximumPaneCountCreated )
    {
        return;
    }

    QCborMap root = QCborValue::fromCbor(
        window.captureRecoveryEditorStateForTest()).toMap();
    QCborMap resourceState = root.value(QStringLiteral("resourceEditors")).toMap();
    QCborArray tabRecords = resourceState.value(QStringLiteral("tabs")).toArray();
    const QCborArray currentHosts = resourceState.value(
        QStringLiteral("hosts")).toArray();
    const QCborMap primaryTemplate = currentHosts.size() == 1
        ? currentHosts.at(0).toMap()
        : QCborMap{};
    const QByteArray primaryGeometry = primaryTemplate.value(
        QStringLiteral("geometry")).toByteArray();
    const bool primaryVisible = primaryTemplate.value(
        QStringLiteral("visible")).toBool();
    QCborArray hostRecords{};
    for ( int hostIndex = 0; hostIndex < 64; ++hostIndex )
    {
        QCborMap tab = tabRecords.at(hostIndex).toMap();
        tab.insert(QStringLiteral("host"), hostIndex);
        tabRecords[hostIndex] = tab;

        QCborMap host{};
        host.insert(QStringLiteral("primary"), hostIndex == 0);
        host.insert(QStringLiteral("id"), hostIndex == 0
                ? QStringLiteral("primary")
                : QStringLiteral("host_cap_%1").arg(hostIndex));
        host.insert(QStringLiteral("visible"), hostIndex == 0
                ? primaryVisible
                : false);
        host.insert(QStringLiteral("floating"), hostIndex != 0);
        host.insert(QStringLiteral("geometry"), primaryGeometry);
        host.insert(QStringLiteral("active"), 0);
        host.insert(QStringLiteral("tabs"), QCborArray{ hostIndex });
        hostRecords.append(host);
    }
    resourceState.insert(QStringLiteral("active"), 0);
    resourceState.insert(QStringLiteral("tabs"), tabRecords);
    resourceState.insert(QStringLiteral("hosts"), hostRecords);
    resourceState.remove(QStringLiteral("groups"));
    resourceState.insert(QStringLiteral("dockVisible"), primaryVisible);
    resourceState.insert(QStringLiteral("dockGeometry"), primaryGeometry);
    root.insert(QStringLiteral("resourceEditors"), resourceState);
    const QByteArray synthetic = ui::recovery::serializeCanonicalCbor(
        QCborValue(root));
    QString warning{};
    const bool validSynthetic =
        window.validateRecoveryEditorStateForTest(synthetic, &warning);
    const bool restoredSynthetic = validSynthetic
        && window.restoreRecoveryEditorStateForTest(synthetic, &warning);
    check(validSynthetic && restoredSynthetic && warning.isEmpty()
            && resourceTabHosts(workspace).size() == 64,
        "a bounded valid EDIT fixture restores exactly 64 live resource tab hosts");
    if ( !restoredSynthetic )
    {
        if ( !warning.isEmpty() )
        {
            std::cerr << "64-host restore warning: " << warning.toStdString() << '\n';
        }
        return;
    }

    QTabBar* const tabBar = primaryTabs->tabBar();
    const QPointer<QTabBar> guardedTabBar(tabBar);
    const QPointer<QDockWidget> primaryDock(dockForResourceTabHost(primaryTabs));
    const QPointer<QWidget> dragOwner(tabBar ? tabBar->window() : nullptr);
    const auto widgetIdentity = [](QWidget* const widget)
    {
        return widget
            ? QString::fromLatin1(widget->metaObject()->className())
                + QLatin1Char('#') + widget->objectName()
            : QStringLiteral("<none>");
    };
    QElapsedTimer readinessDeadline{};
    readinessDeadline.start();
    // Floating recovery hosts can leave native activation and layout events
    // queued. Settle them before the document drag owns deactivation events.
    QApplication::processEvents(QEventLoop::AllEvents, 25);
    if ( primaryDock && dragOwner && guardedTabBar )
    {
        primaryDock->show();
        primaryDock->raise();
        dragOwner->raise();
        dragOwner->activateWindow();
        guardedTabBar->setFocus(Qt::OtherFocusReason);
    }
    QRect stableBarGeometry{};
    QRect stableTabGeometry{};
    int stableSamples = 0;
    while ( stableSamples < 2 && readinessDeadline.elapsed() < 2000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
        if ( !primaryDock || !dragOwner || !guardedTabBar )
        {
            break;
        }
        const QRect barGeometry(guardedTabBar->mapToGlobal(QPoint{}), guardedTabBar->size());
        const QRect tabGeometry = guardedTabBar->tabRect(0);
        const bool ready = primaryDock->isVisible() && dragOwner->isVisible()
            && QApplication::activeWindow() == dragOwner.data()
            && QApplication::focusWidget() == guardedTabBar.data()
            && barGeometry.isValid() && tabGeometry.isValid()
            && guardedTabBar->visibleRegion().contains(tabGeometry.center())
            && guardedTabBar->tabAt(tabGeometry.center()) == 0;
        if ( ready && barGeometry == stableBarGeometry && tabGeometry == stableTabGeometry )
        {
            ++stableSamples;
        }
        else
        {
            stableSamples = 0;
        }
        stableBarGeometry = barGeometry;
        stableTabGeometry = tabGeometry;
    }
    const bool gestureReady = stableSamples >= 2;
    if ( !gestureReady )
    {
        std::cerr << "64-host gesture readiness: elapsedMs=" << readinessDeadline.elapsed()
            << " stableSamples=" << stableSamples
            << " dockVisible=" << (primaryDock && primaryDock->isVisible())
            << " owner=" << widgetIdentity(dragOwner.data()).toStdString()
            << " activeWindow=" << widgetIdentity(QApplication::activeWindow()).toStdString()
            << " focusWidget=" << widgetIdentity(QApplication::focusWidget()).toStdString()
            << " barExposed=" << (guardedTabBar && !guardedTabBar->visibleRegion().isEmpty())
            << " barGeometry=" << stableBarGeometry.x() << ',' << stableBarGeometry.y()
                << ',' << stableBarGeometry.width() << ',' << stableBarGeometry.height()
            << " tabGeometry=" << stableTabGeometry.x() << ',' << stableTabGeometry.y()
                << ',' << stableTabGeometry.width() << ',' << stableTabGeometry.height()
            << " primaryCount=" << primaryTabs->count()
            << " hosts=" << resourceTabHosts(workspace).size() << '\n';
    }
    check(gestureReady,
        "the 64-host fixture settles restore activation, focused visible tab, and stable drag geometry");
    if ( !gestureReady )
    {
        return;
    }

    QWidget* const primaryPage = primaryTabs->currentWidget();
    const QPointer<QWidget> guardedPage(primaryPage);
    const QString primaryIdentity =
        ui::resources::resourceAuthoringPaneIdentity(primaryPage);
    const QCborMap rawBefore = authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(primaryPage));
    const auto outsidePoint = resourceTabDropPoint(workspace, tabBar);
    check(primaryPage && tabBar && outsidePoint && !primaryIdentity.isEmpty(),
        "the 64-host fixture resolves one primary tab and a real outside release point");
    if ( !primaryPage || !tabBar || !outsidePoint )
    {
        return;
    }

    const QPoint startLocal = tabBar->tabRect(0).center();
    const QPoint startGlobal = tabBar->mapToGlobal(startLocal);
    const QString releaseTargetBefore = widgetIdentity(QApplication::widgetAt(*outsidePoint));
    const QString statusBefore = window.statusBar()->currentMessage();
    std::array<QString, 8> observedMessages{};
    std::size_t observedMessageCount = 0;
    const QMetaObject::Connection statusConnection = QObject::connect(
        window.statusBar(), &QStatusBar::messageChanged, &window,
        [&observedMessages, &observedMessageCount](const QString& message)
        {
            if ( observedMessageCount < observedMessages.size() )
            {
                observedMessages[observedMessageCount++] = message;
            }
        });
    QElapsedTimer gestureElapsed{};
    gestureElapsed.start();
    const TabDragOutcome drag = sendTabDrag(
        tabBar, startLocal, *outsidePoint, false);
    QObject::disconnect(statusConnection);
    QWidget* const stillPrimary = resourcePageWithIdentity(workspace, primaryIdentity);
    const QString expectedMessage =
        QStringLiteral("The workspace already has the maximum of 64 resource editor windows.");
    const bool pageAlive = !guardedPage.isNull();
    const bool pageExact = pageAlive && stillPrimary == guardedPage.data();
    const int primaryCount = primaryTabs->count();
    const int primaryIndex = primaryTabs->indexOf(guardedPage.data());
    const std::size_t hostCount = resourceTabHosts(workspace).size();
    const bool draftExact = stillPrimary && authoredDraftState(
        ui::resources::captureResourceAuthoringPaneState(stillPrimary)) == rawBefore;
    const QString actualMessage = window.statusBar()->currentMessage();
    const bool refused = drag.terminalEventsDispatched && !drag.timedOut
        && pageExact && primaryCount == 1 && primaryIndex == 0
        && hostCount == 64 && draftExact && actualMessage == expectedMessage;
    if ( !refused )
    {
        std::cerr << "64-host detach refusal: terminal=" << drag.terminalEventsDispatched
            << " timeout=" << drag.timedOut << " elapsedMs=" << gestureElapsed.elapsed()
            << " pageAlive=" << pageAlive << " pageExact=" << pageExact
            << " primaryCount=" << primaryCount << " primaryIndex=" << primaryIndex
            << " hosts=" << hostCount << " draftExact=" << draftExact
            << " statusExact=" << (actualMessage == expectedMessage)
            << " statusBefore=\"" << statusBefore.toStdString()
            << "\" status=\"" << actualMessage.toStdString() << '"'
            << " observedMessages=[";
        for ( std::size_t index = 0; index < observedMessageCount; ++index )
        {
            std::cerr << (index == 0 ? "" : ",") << '"'
                << observedMessages[index].toStdString() << '"';
        }
        std::cerr << "] owner=" << widgetIdentity(dragOwner.data()).toStdString()
            << " activeWindow=" << widgetIdentity(QApplication::activeWindow()).toStdString()
            << " focusWidget=" << widgetIdentity(QApplication::focusWidget()).toStdString()
            << " barExposed=" << (guardedTabBar && !guardedTabBar->visibleRegion().isEmpty())
            << " start=" << startGlobal.x() << ',' << startGlobal.y()
            << " release=" << outsidePoint->x() << ',' << outsidePoint->y()
            << " releaseTargetBefore=" << releaseTargetBefore.toStdString()
            << " releaseTargetAfter=" << widgetIdentity(QApplication::widgetAt(*outsidePoint)).toStdString()
            << " trace=" << (guardedTabBar
                    ? guardedTabBar->property("resource_authoring.dragTrace").toString().toStdString()
                    : std::string{}) << '\n';
    }
    check(drag.terminalEventsDispatched && !drag.timedOut
            && guardedPage && stillPrimary == guardedPage.data()
            && primaryTabs->count() == 1
            && primaryTabs->indexOf(guardedPage.data()) == 0
            && resourceTabHosts(workspace).size() == 64
            && authoredDraftState(
                   ui::resources::captureResourceAuthoringPaneState(stillPrimary))
                == rawBefore
            && window.statusBar()->currentMessage() == expectedMessage,
        "detaching at the 64-host bound is refused with exact status while preserving the tab, host count, order, and draft");
}

void testSpritePendingNumericEditTracksNativeBaseline()
{
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    window.resize(1120, 760);
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();

    QAction* const createSprite = window.findChild<QAction*>(
        QStringLiteral("tools.resource_editors.sprite_spr.new"));
    if ( createSprite )
    {
        createResourceDraftFromGroup(createSprite);
    }
    QTabWidget* const tabs = window.findChild<QTabWidget*>(
        QStringLiteral("resourceEditorTabs"));
    QWidget* const pane = tabs ? tabs->currentWidget() : nullptr;
    ui::ScrubFloatEdit* const width = pane
        ? pane->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"))
        : nullptr;
    QLineEdit* const edit = width ? width->lineEdit() : nullptr;
    check(createSprite && tabs && pane && width && edit,
        "SPR baseline fixture creates the project-owned width editor and its live text field");
    if ( !createSprite || !tabs || !pane || !width || !edit )
    {
        return;
    }

    window.markCurrentDocumentAsSavedForTest();
    const QByteArray baselineSnapshot = window.captureRecoveryEditorStateForTest();
    const QCborMap baselineRoot = QCborValue::fromCbor(baselineSnapshot).toMap();
    const QCborArray baselineTabs = baselineRoot.value(
        QStringLiteral("resourceEditors")).toMap().value(
            QStringLiteral("tabs")).toArray();
    const QCborMap baselinePane = baselineTabs.size() == 1
        ? baselineTabs.at(0).toMap().value(QStringLiteral("page")).toMap()
        : QCborMap{};
    const bool cleanBaseline = !baselineSnapshot.isEmpty()
        && !baselinePane.value(QStringLiteral("spritePendingNumericEdits")).isMap()
        && !window.hasUnsavedChangesForTest()
        && !window.hasRecoveryOfferEligibleChangesForTest();
    check(cleanBaseline,
        "SPR pending numeric edit fixture establishes a clean saved native baseline with no sparse edit map");
    if ( !cleanBaseline )
    {
        return;
    }

    edit->setFocus(Qt::OtherFocusReason);
    QElapsedTimer focusDeadline{};
    focusDeadline.start();
    while ( QApplication::focusWidget() != edit && focusDeadline.elapsed() < 2000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    const bool fieldFocused = QApplication::focusWidget() == edit;
    check(fieldFocused,
        "SPR pending numeric edit fixture focuses the actual numeric line editor");
    if ( !fieldFocused )
    {
        return;
    }

    const QString committedText = edit->text();
    edit->selectAll();
    const QString pendingText = QStringLiteral("29.375");
    for ( const QChar character : pendingText )
    {
        const int key = character == QLatin1Char('.')
            ? Qt::Key_Period
            : Qt::Key_0 + character.digitValue();
        QKeyEvent press(
            QEvent::KeyPress, key, Qt::NoModifier, QString(character));
        QApplication::sendEvent(edit, &press);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
        QApplication::sendEvent(edit, &release);
    }

    const QByteArray editedSnapshot = window.captureRecoveryEditorStateForTest();
    const QCborMap editedRoot = QCborValue::fromCbor(editedSnapshot).toMap();
    const QCborArray editedTabs = editedRoot.value(
        QStringLiteral("resourceEditors")).toMap().value(
            QStringLiteral("tabs")).toArray();
    const QCborMap editedPane = editedTabs.size() == 1
        ? editedTabs.at(0).toMap().value(QStringLiteral("page")).toMap()
        : QCborMap{};
    const QCborMap pendingEdits = editedPane.value(
        QStringLiteral("spritePendingNumericEdits")).toMap();
    const bool pendingEditAuthored = edit->text() == pendingText
        && editedPane.value(QStringLiteral("sprite")).toArray().at(2)
            == baselinePane.value(QStringLiteral("sprite")).toArray().at(2)
        && pendingEdits.value(QStringLiteral("width")).toString() == pendingText
        && window.hasUnsavedChangesForTest()
        && window.hasRecoveryOfferEligibleChangesForTest();
    check(pendingEditAuthored,
        "live uncommitted SPR width text is captured exactly, leaves its finite model float untouched, and changes the saved authored/recovery baseline");

    QKeyEvent undoPress(
        QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
    QApplication::sendEvent(edit, &undoPress);
    QKeyEvent undoRelease(
        QEvent::KeyRelease, Qt::Key_Z, Qt::ControlModifier);
    QApplication::sendEvent(edit, &undoRelease);
    const bool revertedToCommittedText = edit->text() == committedText;
    const QByteArray revertedSnapshot = window.captureRecoveryEditorStateForTest();
    const QCborMap revertedRoot = QCborValue::fromCbor(revertedSnapshot).toMap();
    const QCborArray revertedTabs = revertedRoot.value(
        QStringLiteral("resourceEditors")).toMap().value(
            QStringLiteral("tabs")).toArray();
    const QCborMap revertedPane = revertedTabs.size() == 1
        ? revertedTabs.at(0).toMap().value(QStringLiteral("page")).toMap()
        : QCborMap{};
    const bool noPendingEditAfterUndo = revertedToCommittedText
        && !revertedPane.value(QStringLiteral("spritePendingNumericEdits")).isMap();
    const bool eligibleAfterRevert = window.hasRecoveryOfferEligibleChangesForTest();
    check(noPendingEditAfterUndo && !eligibleAfterRevert
            && !window.hasUnsavedChangesForTest(),
        "undoing raw SPR text restores the exact saved baseline and clears recovery eligibility");
}

void testSpritePreviewFitKeepsBoundsInsideWideAndTallFrames()
{
    QTemporaryDir temporaryDirectory{};
    const std::filesystem::path root = temporaryRoot(temporaryDirectory);
    const QByteArray materialBytes = makeRgbaMatCels({
        { 2, std::vector<QColor>(4, QColor(236, 28, 36)) }});
    const bool fixtureReady = temporaryDirectory.isValid()
        && writeFile(root / "mat" / "fit_red.mat",
            std::string_view(
                materialBytes.constData(),
                static_cast<std::size_t>(materialBytes.size())));

    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    window.setResourcePathsForTest({ root }, {});
    window.resize(1280, 900);
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();
    QAction* const createSprite = window.findChild<QAction*>(
        QStringLiteral("tools.resource_editors.sprite_spr.new"));
    if ( createSprite )
    {
        createResourceDraftFromGroup(createSprite);
    }
    QTabWidget* const tabs = window.findChild<QTabWidget*>(
        QStringLiteral("resourceEditorTabs"));
    QWidget* const pane = tabs ? tabs->currentWidget() : nullptr;
    QDockWidget* const dock = window.findChild<QDockWidget*>(
        QStringLiteral("resourceEditorsDock"));
    QLineEdit* const material = pane
        ? pane->findChild<QLineEdit*>(QStringLiteral("spr.material"))
        : nullptr;
    QCheckBox* const bounds = pane
        ? pane->findChild<QCheckBox*>(QStringLiteral("spr.preview.bounds"))
        : nullptr;
    QPushButton* const recenter = pane
        ? pane->findChild<QPushButton*>(QStringLiteral("spr.preview.recenter"))
        : nullptr;
    QSplitter* const splitter = pane
        ? pane->findChild<QSplitter*>(QStringLiteral("spr.editor_splitter"))
        : nullptr;
    GlView* const preview = pane
        ? pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"))
        : nullptr;
    check(fixtureReady && createSprite && pane && dock && material && bounds
            && recenter && splitter && preview,
        "sprite fit fixture creates a real pane, synthetic MAT, visual bounds control, and Recenter button");
    if ( !fixtureReady || !createSprite || !pane || !dock || !material
        || !bounds || !recenter || !splitter || !preview )
    {
        return;
    }

    dock->setFloating(true);
    dock->show();
    dock->raise();
    material->setText(QStringLiteral("fit_red.mat"));
    bounds->setChecked(true);

    const auto projectedBoundsFit = [preview]()
    {
        if ( !preview || !preview->isValid() || preview->width() <= 0
            || preview->height() <= 0 )
        {
            return false;
        }
        const std::optional<std::array<double, 6>> boundsAabb =
            preview->thingVisualBoundsOverlayAabbForTest(0, true);
        if ( !boundsAabb )
        {
            return false;
        }
        constexpr double kFramePaddingPixels = 5.0;
        for ( int corner = 0; corner < 8; ++corner )
        {
            const int xIndex = (corner & 1) ? 3 : 0;
            const int yIndex = (corner & 2) ? 4 : 1;
            const int zIndex = (corner & 4) ? 5 : 2;
            const auto point = preview->projectWorldToViewport(QVector3D(
                static_cast<float>((*boundsAabb)[static_cast<std::size_t>(xIndex)]),
                static_cast<float>((*boundsAabb)[static_cast<std::size_t>(yIndex)]),
                static_cast<float>((*boundsAabb)[static_cast<std::size_t>(zIndex)])));
            if ( !point || point->x() < kFramePaddingPixels
                || point->y() < kFramePaddingPixels
                || point->x() > preview->width() - kFramePaddingPixels
                || point->y() > preview->height() - kFramePaddingPixels )
            {
                return false;
            }
        }
        return !preview->grabFramebuffer().isNull();
    };

    const auto runAspect = [&](const QSize dockSize,
                               const bool wide,
                               const char* const aspectLabel)
    {
        dock->resize(dockSize);
        splitter->setSizes({ std::max(240, dockSize.width() / 3),
                             std::max(320, dockSize.width() * 2 / 3) });
        QElapsedTimer deadline{};
        deadline.start();
        bool aspectReady = false;
        while ( deadline.elapsed() < 4000 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
            const double aspect = static_cast<double>(preview->width())
                / static_cast<double>(std::max(1, preview->height()));
            aspectReady = wide ? aspect >= 1.25 : aspect <= 0.8;
            if ( preview->isValid() && aspectReady )
            {
                break;
            }
        }
        recenter->click();
        QElapsedTimer fitDeadline{};
        fitDeadline.start();
        bool fitReady = false;
        while ( fitDeadline.elapsed() < 4000 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
            fitReady = projectedBoundsFit();
            if ( fitReady )
            {
                break;
            }
        }
        const char* const aspectMessage = wide
            ? "Recenter keeps the full visual-bounds outline inside the wide preview frame with padding"
            : "Recenter keeps the full visual-bounds outline inside the tall preview frame with padding";
        check(aspectReady && fitReady, aspectMessage);
    };

    runAspect(QSize(1320, 620), true, "wide");
    runAspect(QSize(700, 1040), false, "tall");
}

void testResourceEditorCaptureSelectsFloatingHostWithoutEditorFocus()
{
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    QMainWindow* const workspace = window.findChild<QMainWindow*>(
        QStringLiteral("workspaceWindow"));
    if ( workspace )
    {
        workspace->setAnimated(false);
    }
    window.resize(1280, 860);
    window.show();
    window.raise();
    window.activateWindow();
    QApplication::processEvents();

    QAction* const sprite = window.findChild<QAction*>(
        QStringLiteral("tools.resource_editors.sprite_spr.new"));
    check(workspace && sprite,
        "detached recovery fixture has a workspace and direct Sprite command");
    if ( !workspace || !sprite )
    {
        return;
    }
    createResourceDraftFromGroup(sprite);
    QTabWidget* const primaryTabs = window.findChild<QTabWidget*>(
        QStringLiteral("resourceEditorTabs"));
    QWidget* const page = primaryTabs ? primaryTabs->currentWidget() : nullptr;
    QLineEdit* const material = page
        ? page->findChild<QLineEdit*>(QStringLiteral("spr.material"))
        : nullptr;
    check(primaryTabs && page && material,
        "detached recovery fixture creates a source page before moving its saved host record");
    if ( !primaryTabs || !page || !material )
    {
        return;
    }
    const QString pageIdentity =
        ui::resources::resourceAuthoringPaneIdentity(page);
    const QPointer<QWidget> sourcePage(page);
    material->setText(QStringLiteral("floating_raw_draft.mat"));

    const QCborValue initialValue = QCborValue::fromCbor(
        window.captureRecoveryEditorStateForTest());
    QCborMap root = initialValue.toMap();
    QCborMap resourceState = root.value(QStringLiteral("resourceEditors")).toMap();
    QCborArray tabs = resourceState.value(QStringLiteral("tabs")).toArray();
    QCborArray hosts = resourceState.value(QStringLiteral("hosts")).toArray();
    QWidget offscreenGeometrySource{};
    offscreenGeometrySource.setGeometry(-30000, -30000, 680, 460);
    const QByteArray offscreenGeometry = offscreenGeometrySource.saveGeometry();
    bool sourceReady = tabs.size() == 1 && hosts.size() == 1
        && tabs.at(0).isMap() && hosts.at(0).isMap()
        && !offscreenGeometry.isEmpty();
    if ( sourceReady )
    {
        QCborMap tab = tabs.at(0).toMap();
        QCborMap primaryHost = hosts.at(0).toMap();
        const QByteArray primaryGeometry = primaryHost.value(
            QStringLiteral("geometry")).toByteArray();
        const bool primaryVisible = primaryHost.value(
            QStringLiteral("visible")).toBool();

        primaryHost.insert(QStringLiteral("active"), -1);
        primaryHost.insert(QStringLiteral("tabs"), QCborArray{});

        QCborMap floatingHost{};
        floatingHost.insert(QStringLiteral("primary"), false);
        floatingHost.insert(QStringLiteral("id"), QStringLiteral("detached_test"));
        floatingHost.insert(QStringLiteral("visible"), true);
        floatingHost.insert(QStringLiteral("floating"), true);
        floatingHost.insert(QStringLiteral("geometry"), offscreenGeometry);
        floatingHost.insert(QStringLiteral("active"), 0);
        floatingHost.insert(QStringLiteral("tabs"), QCborArray{ 0 });
        tab.insert(QStringLiteral("host"), 1);

        tabs[0] = tab;
        hosts[0] = primaryHost;
        hosts.append(floatingHost);
        resourceState.insert(QStringLiteral("active"), 0);
        resourceState.insert(QStringLiteral("tabs"), tabs);
        resourceState.insert(QStringLiteral("hosts"), hosts);
        resourceState.remove(QStringLiteral("groups"));
        resourceState.insert(QStringLiteral("dockVisible"), primaryVisible);
        resourceState.insert(QStringLiteral("dockGeometry"), primaryGeometry);
        root.insert(QStringLiteral("resourceEditors"), resourceState);
    }

    const QByteArray syntheticFloatingState = sourceReady
        ? ui::recovery::serializeCanonicalCbor(QCborValue(root))
        : QByteArray{};
    QString restoreWarning{};
    const bool syntheticStateValid = sourceReady
        && window.validateRecoveryEditorStateForTest(syntheticFloatingState, &restoreWarning);
    const bool restoredFloatingState = syntheticStateValid
        && window.restoreRecoveryEditorStateForTest(syntheticFloatingState, &restoreWarning);
    QApplication::processEvents();
    QWidget* const restoredPage = resourcePageWithIdentity(workspace, pageIdentity);
    QTabWidget* const detachedTabs = resourceTabHostContaining(workspace, restoredPage);
    QDockWidget* const detachedDock = dockForResourceTabHost(detachedTabs);
    check(sourceReady && syntheticStateValid && restoredFloatingState
            && primaryTabs->count() == 0
            && detachedTabs && detachedTabs != primaryTabs
            && restoredPage && restoredPage != sourcePage.data()
            && detachedTabs->currentWidget() == restoredPage
            && detachedDock && detachedDock->isFloating(),
        "recovery setup restores every resource tab into a separate floating host");
    bool floatingDockIntersectsScreen = false;
    bool floatingCaptionReachable = false;
    if ( detachedDock )
    {
        const QRect dockGlobalRect(
            detachedDock->mapToGlobal(QPoint{}), detachedDock->size());
        const QRect frame = detachedDock->frameGeometry();
        const QRect caption(dockGlobalRect.left(), frame.top(), dockGlobalRect.width(),
            std::max(0, dockGlobalRect.top() - frame.top()));
        for ( QScreen* const screen : QGuiApplication::screens() )
        {
            if ( screen && dockGlobalRect.intersects(screen->availableGeometry()) )
            {
                floatingDockIntersectsScreen = true;
            }
            if ( screen && caption.height() > 0 && screen->availableGeometry().contains(caption) )
            {
                floatingCaptionReachable = true;
            }
        }
        if ( !floatingCaptionReachable )
        {
            std::cerr << "floating resource caption: frame=" << frame.x() << ',' << frame.y()
                << ',' << frame.width() << ',' << frame.height()
                << " client=" << dockGlobalRect.x() << ',' << dockGlobalRect.y()
                << ',' << dockGlobalRect.width() << ',' << dockGlobalRect.height()
                << " caption=" << caption.x() << ',' << caption.y()
                << ',' << caption.width() << ',' << caption.height() << '\n';
        }
    }
    check(syntheticStateValid && restoredFloatingState
            && floatingDockIntersectsScreen,
        "a detached resource dock restored from off-screen geometry remains reachable on an available screen");
    check(syntheticStateValid && restoredFloatingState
            && nativeMaximizeButtonClickRoundTrip(detachedDock),
        "a detached resource dock restored from EDIT exposes an enabled native Maximize caption button");
    check(syntheticStateValid && restoredFloatingState && floatingCaptionReachable,
        "a restored detached resource window keeps its complete native caption reachable on an available screen");
    if ( !restoredFloatingState || !detachedTabs || !detachedDock )
    {
        if ( !restoreWarning.isEmpty() )
        {
            std::cerr << "floating host restore warning: "
                << restoreWarning.toStdString() << '\n';
        }
        return;
    }

    QSplitter* const viewportSplitter = qobject_cast<QSplitter*>(
        workspace->centralWidget());
    QWidget* const viewport = viewportSplitter
        ? viewportSplitter->widget(0)
        : nullptr;
    check(viewport != nullptr,
        "detached recovery fixture can resolve the main viewport for focus routing");
    if ( viewport )
    {
        window.activateWindow();
        workspace->activateWindow();
        viewport->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents();
    }

    const bool viewportOwnsFocus = viewport
        && QApplication::focusWidget() == viewport;
    const QByteArray captured = window.captureRecoveryEditorStateForTest();
    const QCborMap capturedRoot = QCborValue::fromCbor(captured).toMap();
    const QCborMap capturedWorkspace = capturedRoot.value(
        QStringLiteral("resourceEditors")).toMap();
    const QCborArray capturedTabs = capturedWorkspace.value(
        QStringLiteral("tabs")).toArray();
    const QCborMap capturedTab = capturedTabs.size() == 1
        ? capturedTabs.at(0).toMap()
        : QCborMap{};
    const QCborArray capturedHosts = capturedWorkspace.value(
        QStringLiteral("hosts")).toArray();
    const qint64 globalActive = capturedWorkspace.value(
        QStringLiteral("active")).toInteger(-1);
    QString capturedWarning{};
    const bool captureValid = window.validateRecoveryEditorStateForTest(
        captured, &capturedWarning);
    check(viewportOwnsFocus,
        "detached resource active-index regression establishes viewport focus while all editor tabs float");
    check(capturedTabs.size() == 1 && capturedHosts.size() == 1
            && capturedWorkspace.value(QStringLiteral("active")).isInteger()
            && globalActive >= 0 && globalActive < capturedTabs.size()
            && capturedTab.value(QStringLiteral("host")).toInteger() == 0
            && authoredDraftState(capturedTab.value(QStringLiteral("page")).toMap())
                == authoredDraftState(
                    ui::resources::captureResourceAuthoringPaneState(restoredPage)),
        "resource EDIT capture selects a valid global active index for a nonempty detached host while viewport has focus");
    check(captureValid,
        "resource EDIT validation accepts the captured active detached tab after viewport focus");
    if ( !capturedWarning.isEmpty() )
    {
        std::cerr << "floating host capture warning: "
            << capturedWarning.toStdString() << '\n';
    }
}

void testResourceTabClosePromptNamesItsDirtyDraft()
{
    QTemporaryDir temporary{};
    MainWindow window(nullptr, true);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    window.setAnimated(false);
    window.resize(1440, 960);
    window.show();
    window.activateWindow();
    struct DraftFixture
    {
        QString actionName{};
        QString tabName{};
        ui::resources::ResourceAuthoringKind kind{};
    };
    const std::array<DraftFixture, 3> fixtures{
        DraftFixture{ QStringLiteral("tools.resource_editors.sprite_spr.new"),
            QStringLiteral("resourceEditorTabs"), ui::resources::ResourceAuthoringKind::sprite },
        DraftFixture{ QStringLiteral("tools.resource_editors.sound_class_snd.new"),
            QStringLiteral("soundClassResourceEditorTabs"), ui::resources::ResourceAuthoringKind::soundClass },
        DraftFixture{ QStringLiteral("tools.resource_editors.string_table_uni.new"),
            QStringLiteral("uniResourceEditorTabs"), ui::resources::ResourceAuthoringKind::stringTable } };
    for ( const DraftFixture& fixture : fixtures )
    {
        QAction* const create = window.findChild<QAction*>(fixture.actionName);
        createResourceDraftFromGroup(create);
        QTabWidget* const tabs = window.findChild<QTabWidget*>(fixture.tabName);
        QPointer<QWidget> page(tabs ? tabs->currentWidget() : nullptr);
        check(create && tabs && page,
            "named-close fixture creates each resource through its real group New button");
        if ( !create || !tabs || !page )
        {
            continue;
        }
        if ( fixture.kind == ui::resources::ResourceAuthoringKind::sprite )
        {
            QLineEdit* const material = page->findChild<QLineEdit*>(QStringLiteral("spr.material"));
            check(material != nullptr, "named-close Sprite fixture exposes its authored material");
            if ( !material )
            {
                continue;
            }
            material->setText(QStringLiteral("named_close.mat"));
        }
        else if ( fixture.kind == ui::resources::ResourceAuthoringKind::soundClass )
        {
            restoreSoundRows(qobject_cast<QDialog*>(page.data()), {
                { QStringLiteral("moving"), QStringLiteral("before.wav"), QStringLiteral("0x00000040"),
                    QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("1") } });
            QTreeWidget* const tree = page->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
            const QList<QTreeWidgetItem*> alternatives = soundAlternativeItems(tree);
            check(alternatives.size() == 1,
                "named-close Sound Class fixture resolves a real authored alternative");
            if ( alternatives.size() != 1 )
            {
                continue;
            }
            alternatives.front()->setText(1, QStringLiteral("changed.wav"));
        }
        else
        {
            QAbstractButton* const add = button(page.data(), QStringLiteral("uni.add"));
            QTableWidget* const table = page->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
            check(add && table, "named-close UNI fixture exposes the real Add and string table");
            if ( !add || !table )
            {
                continue;
            }
            add->click();
            check(table->rowCount() == 1 && table->item(0, 0) && table->item(0, 1),
                "named-close UNI fixture creates an editable row");
            if ( table->rowCount() != 1 || !table->item(0, 0) || !table->item(0, 1) )
            {
                continue;
            }
            table->item(0, 0)->setText(QStringLiteral("NAMED_CLOSE"));
            table->item(0, 1)->setText(QStringLiteral("edited text"));
        }
        QApplication::processEvents();
        const QCborMap before = ui::resources::captureResourceAuthoringPaneState(page.data());
        const QString identityBefore = ui::resources::resourceAuthoringPaneIdentity(page.data());
        const QString filename = before.value(QStringLiteral("name")).toString();
        const int index = tabs->indexOf(page.data());
        const int count = tabs->count();
        bool namedPrompt = false;
        ModalStep closePrompt{};
        closePrompt.matches = [](QDialog* dialog)
        {
            return dialog->objectName() == QStringLiteral("resource_authoring.unsaved_prompt");
        };
        closePrompt.interact = [&namedPrompt, filename](QDialog* dialog)
        {
            QMessageBox* const prompt = qobject_cast<QMessageBox*>(dialog);
            namedPrompt = prompt && !filename.isEmpty() && prompt->text().contains(filename);
            QAbstractButton* const cancel = prompt ? prompt->button(QMessageBox::Cancel) : nullptr;
            if ( cancel )
            {
                cancel->click();
            }
            else
            {
                dialog->reject();
            }
        };
        BoundedModalSequence modal({ std::move(closePrompt) });
        modal.start();
        const bool closeDispatched = QMetaObject::invokeMethod(
            tabs, "tabCloseRequested", Qt::DirectConnection, Q_ARG(int, index));
        modal.stop();
        check(closeDispatched && modal.completedSteps() == 1 && !modal.timedOut() && namedPrompt,
            "closing a dirty Sprite, Sound Class, or UNI file tab identifies its current resource filename");
        check(page && tabs->count() == count && tabs->indexOf(page.data()) == index
                && tabs->currentWidget() == page.data()
                && ui::resources::resourceAuthoringPaneIdentity(page.data()) == identityBefore
                && authoredDraftState(ui::resources::captureResourceAuthoringPaneState(page.data()))
                    == authoredDraftState(before)
                && ui::resources::resourceAuthoringPaneIsModified(page.data()),
            "Cancel in each named resource tab-close prompt preserves its exact dirty draft, identity, order, and selection");
    }

    const std::filesystem::path loosePath = temporaryRoot(temporary) / "loose_named.uni";
    const std::string looseBytes = "MSGS 1\n\"LOOSE_CLOSE\" -17 \"before\"\nEND\n";
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"));
    QPointer<QWidget> page(tabs ? tabs->currentWidget() : nullptr);
    check(temporary.isValid() && page && writeFile(loosePath, looseBytes),
        "named-close loose UNI fixture writes only a synthetic source file");
    if ( !temporary.isValid() || !page || !std::filesystem::is_regular_file(loosePath) )
    {
        return;
    }
    BoundedModalSequence open({ fileDialogStep(loosePath) });
    open.start();
    const bool opened = ui::resources::resourceAuthoringPaneOpenLooseFileDirect(page.data());
    open.stop();
    QTableWidget* const table = page ? page->findChild<QTableWidget*>(QStringLiteral("uni.entries")) : nullptr;
    check(opened && open.completedSteps() == 1 && !open.timedOut()
            && table && table->rowCount() == 1 && table->item(0, 1),
        "named-close loose UNI fixture opens its exact source through the real file chooser");
    if ( !opened || !table || table->rowCount() != 1 || !table->item(0, 1) )
    {
        return;
    }
    table->item(0, 1)->setText(QStringLiteral("changed loose text"));
    const QCborMap before = ui::resources::captureResourceAuthoringPaneState(page.data());
    const QString identityBefore = ui::resources::resourceAuthoringPaneIdentity(page.data());
    const int index = tabs->indexOf(page.data());
    const int count = tabs->count();
    bool namedLoosePrompt = false;
    ModalStep closePrompt{};
    closePrompt.matches = [](QDialog* dialog)
    {
        return dialog->objectName() == QStringLiteral("resource_authoring.unsaved_prompt");
    };
    closePrompt.interact = [&namedLoosePrompt](QDialog* dialog)
    {
        QMessageBox* const prompt = qobject_cast<QMessageBox*>(dialog);
        namedLoosePrompt = prompt && prompt->text().contains(QStringLiteral("loose_named.uni"));
        QAbstractButton* const cancel = prompt ? prompt->button(QMessageBox::Cancel) : nullptr;
        if ( cancel )
        {
            cancel->click();
        }
        else
        {
            dialog->reject();
        }
    };
    BoundedModalSequence close({ std::move(closePrompt) });
    close.start();
    const bool closeDispatched = QMetaObject::invokeMethod(
        tabs, "tabCloseRequested", Qt::DirectConnection, Q_ARG(int, index));
    close.stop();
    check(closeDispatched && close.completedSteps() == 1 && !close.timedOut() && namedLoosePrompt,
        "closing a modified loose resource includes its current basename in the unsaved prompt");
    check(page && tabs->count() == count && tabs->indexOf(page.data()) == index
            && tabs->currentWidget() == page.data()
            && ui::resources::resourceAuthoringPaneIdentity(page.data()) == identityBefore
            && authoredDraftState(ui::resources::captureResourceAuthoringPaneState(page.data()))
                == authoredDraftState(before)
            && ui::resources::resourceAuthoringPaneIsModified(page.data())
            && readFile(loosePath) == std::optional<std::string>{ looseBytes },
        "Cancel in the named loose-file close prompt preserves exact raw draft, source identity, tab selection, and on-disk bytes");
}

void testDetachedResourceFilesRetainWindowChromeAndDocking()
{
    struct KindFixture
    {
        QString action{};
        QString tabs{};
        QString otherAction{};
        QString otherTabs{};
        ui::resources::ResourceAuthoringKind kind{};
    };
    const std::array<KindFixture, 3> fixtures{
        KindFixture{ QStringLiteral("tools.resource_editors.sprite_spr.new"), QStringLiteral("resourceEditorTabs"),
            QStringLiteral("tools.resource_editors.string_table_uni.new"), QStringLiteral("uniResourceEditorTabs"),
            ui::resources::ResourceAuthoringKind::sprite },
        KindFixture{ QStringLiteral("tools.resource_editors.sound_class_snd.new"), QStringLiteral("soundClassResourceEditorTabs"),
            QStringLiteral("tools.resource_editors.string_table_uni.new"), QStringLiteral("uniResourceEditorTabs"),
            ui::resources::ResourceAuthoringKind::soundClass },
        KindFixture{ QStringLiteral("tools.resource_editors.string_table_uni.new"), QStringLiteral("uniResourceEditorTabs"),
            QStringLiteral("tools.resource_editors.sprite_spr.new"), QStringLiteral("resourceEditorTabs"),
            ui::resources::ResourceAuthoringKind::stringTable } };
    QScreen* const screen = QGuiApplication::primaryScreen();
    check(screen != nullptr, "detached resource chrome: real available screen exists");
    if ( !screen )
    {
        return;
    }
    const QRect available = screen->availableGeometry();
    for ( const KindFixture& fixture : fixtures )
    {
        MainWindow window(nullptr, true);
        window.setProjectTypeForTest(ProjectType::JKDF2);
        const QSize maximumSize(std::max(1, available.width() - 40), std::max(1, available.height() - 80));
        window.setMinimumSize(window.minimumSize().boundedTo(maximumSize));
        window.resize(QSize(2000, 960).boundedTo(maximumSize));
        window.move(available.topLeft() + QPoint(20, 20));
        window.show();
        window.activateWindow();
        QMainWindow* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
        QAction* const create = window.findChild<QAction*>(fixture.action);
        createResourceDraftFromGroup(create);
        createResourceDraftFromGroup(create);
        QTabWidget* const primaryTabs = window.findChild<QTabWidget*>(fixture.tabs);
        QPointer<QWidget> page(primaryTabs ? primaryTabs->currentWidget() : nullptr);
        QDockWidget* const primaryDock = dockForResourceTabHost(primaryTabs);
        check(workspace && primaryTabs && primaryTabs->count() == 2 && page && primaryDock,
            "detached resource chrome: real group New creates two same-kind files");
        if ( !workspace || !primaryTabs || primaryTabs->count() != 2 || !page || !primaryDock )
        {
            continue;
        }
        if ( fixture.kind == ui::resources::ResourceAuthoringKind::sprite )
        {
            QLineEdit* const material = page->findChild<QLineEdit*>(QStringLiteral("spr.material"));
            check(material != nullptr, "detached resource chrome: Sprite material edit exists");
            if ( !material )
            {
                continue;
            }
            material->setText(QStringLiteral("detached_chrome.mat"));
        }
        else if ( fixture.kind == ui::resources::ResourceAuthoringKind::soundClass )
        {
            restoreSoundRows(qobject_cast<QDialog*>(page.data()), {
                { QStringLiteral("moving"), QStringLiteral("before.wav"), QStringLiteral("0x00000040"),
                    QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("1") } });
            QTreeWidget* const tree = page->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
            const QList<QTreeWidgetItem*> rows = soundAlternativeItems(tree);
            check(rows.size() == 1, "detached resource chrome: Sound authored alternative exists");
            if ( rows.size() != 1 )
            {
                continue;
            }
            rows.front()->setText(1, QStringLiteral("detached_chrome.wav"));
        }
        else
        {
            QAbstractButton* const add = button(page.data(), QStringLiteral("uni.add"));
            QTableWidget* const table = page->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
            check(add && table, "detached resource chrome: UNI Add and table exist");
            if ( !add || !table )
            {
                continue;
            }
            add->click();
            check(table->rowCount() == 1 && table->item(0, 0) && table->item(0, 1),
                "detached resource chrome: UNI creates one real authored row");
            if ( table->rowCount() != 1 || !table->item(0, 0) || !table->item(0, 1) )
            {
                continue;
            }
            table->item(0, 0)->setText(QStringLiteral("DETACHED_CHROME"));
            table->item(0, 1)->setText(QStringLiteral("Exact detached draft"));
        }
        QApplication::processEvents();
        const QCborMap before = ui::resources::captureResourceAuthoringPaneState(page.data());
        const QString identity = ui::resources::resourceAuthoringPaneIdentity(page.data());
        const auto unchanged = [&page, &before, &identity]()
        {
            const QCborMap current = page
                ? ui::resources::captureResourceAuthoringPaneState(page.data()) : QCborMap{};
            return page && authoredDraftState(current) == authoredDraftState(before)
                && current.value(QStringLiteral("history")) == before.value(QStringLiteral("history"))
                && ui::resources::resourceAuthoringPaneIdentity(page.data()) == identity
                && ui::resources::resourceAuthoringPaneIsModified(page.data());
        };
        const auto outside = resourceTabDropPoint(workspace, primaryTabs->tabBar());
        check(outside.has_value(), "detached resource chrome: file tab has a real outside-host release point");
        if ( !outside )
        {
            continue;
        }
        const TabDragOutcome detachedDrag = sendTabDrag(primaryTabs->tabBar(),
            primaryTabs->tabBar()->tabRect(primaryTabs->indexOf(page.data())).center(), *outside, false);
        QPointer<QTabWidget> detachedTabs(resourceTabHostContaining(workspace, page.data()));
        QPointer<QDockWidget> detachedDock(dockForResourceTabHost(detachedTabs.data()));
        check(detachedDrag.terminalEventsDispatched && !detachedDrag.timedOut
                && detachedTabs && detachedTabs != primaryTabs && detachedDock && detachedDock->isFloating()
                && primaryTabs->count() == 1 && detachedTabs->currentWidget() == page.data() && unchanged(),
            "detached resource chrome: real file-tab drag creates a same-kind floating editor without changing draft or history");
        if ( !detachedTabs || !detachedDock || detachedTabs == primaryTabs )
        {
            continue;
        }
        QPointer<QWidget> groupTools(detachedDock->findChild<QWidget*>(QStringLiteral("resource_authoring.group_toolbar")));
        QWidget* const documentTools = page->findChild<QWidget*>(QStringLiteral("resource_authoring.document_tools"));
        QToolButton* const undo = page->findChild<QToolButton*>(QStringLiteral("resource_authoring.undo"));
        QToolButton* const redo = page->findChild<QToolButton*>(QStringLiteral("resource_authoring.redo"));
        check(detachedDock->isWindow() && !detachedDock->windowFlags().testFlag(Qt::FramelessWindowHint)
                && !detachedDock->titleBarWidget() && !detachedDock->windowIcon().isNull()
                && detachedDock->windowFlags().testFlag(Qt::WindowTitleHint)
                && detachedDock->windowFlags().testFlag(Qt::WindowSystemMenuHint)
                && detachedDock->windowFlags().testFlag(Qt::WindowCloseButtonHint)
                && detachedDock->windowFlags().testFlag(Qt::WindowMaximizeButtonHint)
                && detachedDock->features().testFlag(QDockWidget::DockWidgetMovable)
                && detachedDock->features().testFlag(QDockWidget::DockWidgetFloatable)
                && detachedDock->features().testFlag(QDockWidget::DockWidgetClosable),
            "detached resource chrome: each file has a decorated movable, floatable, closable native editor window with maximize");
        check(groupTools && groupTools->isVisible()
                && groupTools->findChildren<QToolButton*>(QString{}, Qt::FindDirectChildrenOnly).size() == 3
                && documentTools && documentTools->isVisible() && undo && undo->isEnabled() && redo
                && detachedTabs->isVisible() && unchanged(),
            "detached resource chrome: floating retains exactly New Open Save and independent document history controls");

        createResourceDraftFromGroup(window.findChild<QAction*>(fixture.otherAction));
        QTabWidget* const otherTabs = window.findChild<QTabWidget*>(fixture.otherTabs);
        QDockWidget* const otherDock = dockForResourceTabHost(otherTabs);
        check(otherTabs && otherDock && otherTabs->count() == 1,
            "detached resource chrome: another kind has a real visible file bar for rejected transfer");
        if ( otherTabs && otherDock && otherTabs->count() == 1 )
        {
            otherDock->show();
            otherDock->raise();
            QApplication::processEvents();
            const QPointer<QTabWidget> originalHost(detachedTabs);
            const TabDragOutcome rejected = sendTabDrag(detachedTabs->tabBar(),
                detachedTabs->tabBar()->tabRect(detachedTabs->indexOf(page.data())).center(),
                otherTabs->tabBar()->mapToGlobal(otherTabs->tabBar()->tabRect(0).center()), false);
            check(rejected.terminalEventsDispatched && !rejected.timedOut && originalHost
                    && resourceTabHostContaining(workspace, page.data()) == originalHost.data()
                    && originalHost->currentWidget() == page.data() && otherTabs->count() == 1 && unchanged(),
                "detached resource chrome: dropping onto another resource kind leaves the original host and exact draft atomic");
            detachedTabs = resourceTabHostContaining(workspace, page.data());
            detachedDock = dockForResourceTabHost(detachedTabs.data());
            groupTools = detachedDock ? detachedDock->findChild<QWidget*>(
                QStringLiteral("resource_authoring.group_toolbar")) : nullptr;
            // The wrong-kind target has served its rejection check. It is not
            // part of the following two-window native split workspace.
            otherDock->hide();
        }
        if ( !detachedTabs || !detachedDock )
        {
            continue;
        }
        QToolButton* const returnDock = detachedDock->findChild<QToolButton*>(QStringLiteral("resource_group.undock"));
        check(returnDock && returnDock->isVisible() && returnDock->isEnabled()
                && returnDock->toolTip().startsWith(QStringLiteral("Dock ")),
            "detached resource chrome: every floating file exposes a reachable Dock return in its grip");
        if ( returnDock )
        {
            returnDock->click();
        }
        QApplication::processEvents();
        check(!detachedDock->isFloating() && detachedDock->titleBarWidget()
                && detachedDock->titleBarWidget()->isVisible() && unchanged(),
            "detached resource chrome: the real Dock control restores a movable native workspace dock and its grip");
        QPointer<QWidget> canceledHeader(detachedDock->titleBarWidget());
        if ( canceledHeader )
        {
            const QPoint start(canceledHeader->width() / 2, canceledHeader->height() / 2);
            const QPoint startGlobal = canceledHeader->mapToGlobal(start);
            const QPoint cursorBefore = QCursor::pos();
            QCursor::setPos(startGlobal);
            QMouseEvent press(QEvent::MouseButtonPress, QPointF(start), QPointF(startGlobal),
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(canceledHeader.data(), &press);
            QCursor::setPos(startGlobal + QPoint(24, 12));
            QMouseEvent move(QEvent::MouseMove, QPointF(start + QPoint(24, 12)),
                QPointF(startGlobal + QPoint(24, 12)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(canceledHeader.data(), &move);
            check(QWidget::mouseGrabber() == detachedDock.data(),
                "detached resource chrome: cancellation fixture starts a genuine Qt grip drag");
            // setFloating aborts Qt's active docking gesture without a left
            // release. The resulting lost grab must also finish our chrome.
            detachedDock->setFloating(true);
            detachedDock->show();
            QElapsedTimer canceledDeadline{};
            canceledDeadline.start();
            while ( detachedDock && (detachedDock->titleBarWidget()
                    || detachedDock->windowFlags().testFlag(Qt::FramelessWindowHint))
                && canceledDeadline.elapsed() < 2000 )
            {
                QApplication::processEvents(QEventLoop::AllEvents, 25);
            }
            if ( !detachedDock || detachedDock->titleBarWidget()
                || detachedDock->windowFlags().testFlag(Qt::FramelessWindowHint)
                || !returnDock->isVisible() || !returnDock->toolTip().startsWith(QStringLiteral("Dock "))
                || !unchanged() )
            {
                std::cerr << "resource grip abort: kind=" << static_cast<int>(fixture.kind)
                    << " alive=" << static_cast<bool>(detachedDock)
                    << " floating=" << (detachedDock && detachedDock->isFloating())
                    << " customTitle=" << (detachedDock && detachedDock->titleBarWidget())
                    << " frameless=" << (detachedDock && detachedDock->windowFlags().testFlag(Qt::FramelessWindowHint))
                    << " mouseGrab=" << (detachedDock && QWidget::mouseGrabber() == detachedDock.data())
                    << " returnVisible=" << returnDock->isVisible()
                    << " returnTip=" << returnDock->toolTip().toStdString()
                    << " authoredAndHistoryExact=" << unchanged() << '\n';
            }
            check(detachedDock && detachedDock->isFloating() && !detachedDock->titleBarWidget()
                    && !detachedDock->windowFlags().testFlag(Qt::FramelessWindowHint)
                    && returnDock->isVisible() && returnDock->toolTip().startsWith(QStringLiteral("Dock "))
                    && unchanged(),
                "detached resource chrome: aborting a native grip drag without release restores window decorations and Dock return");
            // Complete the original synthetic pointer sequence so a failed
            // cancellation assertion cannot contaminate the next real drag.
            if ( canceledHeader )
            {
                const QPoint releaseGlobal = startGlobal + QPoint(24, 12);
                QMouseEvent release(QEvent::MouseButtonRelease,
                    QPointF(canceledHeader->mapFromGlobal(releaseGlobal)), QPointF(releaseGlobal),
                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(canceledHeader.data(), &release);
            }
            QApplication::processEvents();
            QCursor::setPos(cursorBefore);
            if ( returnDock && detachedDock && detachedDock->isFloating() )
            {
                returnDock->click();
            }
            QApplication::processEvents();
        }
        const QPointer<QWidget> nextGestureHeader(detachedDock->titleBarWidget());
        check(nextGestureHeader && nextGestureHeader->isVisible() && !detachedDock->isFloating(),
            "detached resource chrome: the queued-finish fixture starts with the real returned dock grip");
        if ( nextGestureHeader && nextGestureHeader->isVisible() && !detachedDock->isFloating() )
        {
            const QPoint start(nextGestureHeader->width() / 2, nextGestureHeader->height() / 2);
            const QPoint startGlobal = nextGestureHeader->mapToGlobal(start);
            const QPoint cursorBefore = QCursor::pos();
            QCursor::setPos(startGlobal);
            QMouseEvent firstPress(QEvent::MouseButtonPress, QPointF(start), QPointF(startGlobal),
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(nextGestureHeader.data(), &firstPress);
            QMouseEvent firstRelease(QEvent::MouseButtonRelease, QPointF(start), QPointF(startGlobal),
                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(nextGestureHeader.data(), &firstRelease);
            // The first release queues chrome completion. Start the next
            // gesture before draining it, while still below Qt's drag threshold.
            QMouseEvent secondPress(QEvent::MouseButtonPress, QPointF(start), QPointF(startGlobal),
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(nextGestureHeader.data(), &secondPress);
            check(QWidget::mouseGrabber() != detachedDock.data(),
                "detached resource chrome: the second queued-finish press has not crossed the native drag threshold");
            QApplication::processEvents(QEventLoop::AllEvents);
            const QPoint beginGlobal = startGlobal + QPoint(24, 12);
            QCursor::setPos(beginGlobal);
            QMouseEvent beginMove(QEvent::MouseMove,
                QPointF(nextGestureHeader->mapFromGlobal(beginGlobal)), QPointF(beginGlobal),
                Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(nextGestureHeader.data(), &beginMove);
            const bool nativeGrab = QWidget::mouseGrabber() == detachedDock.data();
            const bool originalGrip = detachedDock->titleBarWidget() == nextGestureHeader.data();
            const bool customDragFlags = detachedDock->windowFlags().testFlag(Qt::FramelessWindowHint);
            if ( !nativeGrab || !originalGrip || !customDragFlags )
            {
                std::cerr << "resource queued grip finish: kind=" << static_cast<int>(fixture.kind)
                    << " nativeGrab=" << nativeGrab << " originalGrip=" << originalGrip
                    << " customDragFlags=" << customDragFlags
                    << " floating=" << detachedDock->isFloating() << " authoredAndHistoryExact=" << unchanged() << '\n';
            }
            check(nativeGrab && originalGrip && customDragFlags && unchanged(),
                "detached resource chrome: an old queued finish cannot replace the original grip or recreate native flags during the next live drag");
            const QPoint dropGlobal(available.left() + 2, available.center().y());
            check(available.contains(dropGlobal) && !window.frameGeometry().contains(dropGlobal),
                "detached resource chrome: the queued-finish gesture has a real on-screen drop outside its MainWindow");
            const QPointer<QWidget> moveReceiver(QWidget::mouseGrabber()
                ? QWidget::mouseGrabber() : nextGestureHeader.data());
            QCursor::setPos(dropGlobal);
            QMouseEvent dropMove(QEvent::MouseMove, QPointF(moveReceiver->mapFromGlobal(dropGlobal)),
                QPointF(dropGlobal), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(moveReceiver.data(), &dropMove);
            const QPointer<QWidget> releaseReceiver(QWidget::mouseGrabber()
                ? QWidget::mouseGrabber() : nextGestureHeader.data());
            QMouseEvent release(QEvent::MouseButtonRelease, QPointF(releaseReceiver->mapFromGlobal(dropGlobal)),
                QPointF(dropGlobal), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(releaseReceiver.data(), &release);
            const auto completed = [&]()
            {
                return detachedDock && detachedDock->isFloating() && !detachedDock->titleBarWidget()
                    && !detachedDock->windowFlags().testFlag(Qt::FramelessWindowHint)
                    && returnDock->isVisible() && returnDock->toolTip().startsWith(QStringLiteral("Dock "));
            };
            QElapsedTimer completionDeadline{};
            completionDeadline.start();
            while ( !completed() && completionDeadline.elapsed() < 2000 )
            {
                QApplication::processEvents(QEventLoop::AllEvents, 25);
            }
            QCursor::setPos(cursorBefore);
            check(completed() && unchanged(),
                "detached resource chrome: the second real gesture releases to decorated chrome with a reachable Dock return and exact raw history");
            if ( returnDock && detachedDock && detachedDock->isFloating() )
            {
                returnDock->click();
            }
            QApplication::processEvents();
        }
        primaryDock->show();
        primaryDock->raise();
        QApplication::processEvents();
        const auto hasActiveNativeDockGeometryAnimation = [](QObject* animationOwner,
                                                              QObject* animationTarget)
        {
            if ( !animationOwner || !animationTarget )
            {
                return false;
            }
            for ( QPropertyAnimation* const animation : animationOwner->findChildren<QPropertyAnimation*>() )
            {
                if ( animation->targetObject() == animationTarget
                    && animation->propertyName() == QByteArrayLiteral("geometry")
                    && animation->state() != QAbstractAnimation::Stopped )
                {
                    return true;
                }
            }
            return false;
        };
        QPointer<QMainWindow> workspaceGuard(workspace);
        QPointer<QDockWidget> primaryDockGuard(primaryDock);
        QPointer<QDockWidget> detachedDockGuard(detachedDock.data());
        const auto hasActiveWorkspaceGeometryAnimation = [&]()
        {
            if ( !workspaceGuard )
            {
                return false;
            }
            for ( QPropertyAnimation* const animation : workspaceGuard->findChildren<QPropertyAnimation*>() )
            {
                QWidget* const target = qobject_cast<QWidget*>(animation->targetObject());
                if ( target && (target == workspaceGuard.data() || workspaceGuard->isAncestorOf(target))
                    && animation->propertyName() == QByteArrayLiteral("geometry")
                    && animation->state() != QAbstractAnimation::Stopped )
                {
                    return true;
                }
            }
            return false;
        };
        const auto returnedDockReady = [&]()
        {
            QWidget* const returnedHeader = detachedDock->titleBarWidget();
            const QRect headerBounds = returnedHeader
                ? QRect(returnedHeader->mapToGlobal(QPoint{}), returnedHeader->size()) : QRect{};
            const QRect toolbarBounds = groupTools
                ? QRect(groupTools->mapToGlobal(QPoint{}), groupTools->size()) : QRect{};
            return workspace->isAnimated() && !detachedDock->isFloating()
                && !hasActiveNativeDockGeometryAnimation(detachedDock.data(), detachedDock.data())
                && !hasActiveNativeDockGeometryAnimation(workspace, detachedDock.data())
                && returnedHeader && returnedHeader->isVisible()
                && (returnedHeader->parentWidget() == detachedDock.data()
                    || detachedDock->isAncestorOf(returnedHeader))
                && groupTools && groupTools->isVisible() && detachedDock->isAncestorOf(groupTools.data())
                && returnDock && returnDock->isVisible()
                && !detachedDock->visibleRegion().isEmpty()
                && available.contains(headerBounds) && available.contains(toolbarBounds);
        };
        QElapsedTimer returnedDockDeadline{};
        returnedDockDeadline.start();
        while ( !returnedDockReady() && returnedDockDeadline.elapsed() < 2000 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
        }
        const bool returnedDockSettled = returnedDockReady();
        check(returnedDockSettled,
            "detached resource chrome: the real returned dock geometry animation completes with reachable grip and toolbar before a new native drag");
        if ( !returnedDockSettled )
        {
            const QWidget* const returnedHeader = detachedDock->titleBarWidget();
            std::cerr << "returned resource dock completion: kind=" << static_cast<int>(fixture.kind)
                << " animated=" << workspace->isAnimated()
                << " floating=" << detachedDock->isFloating()
                << " activeSourceGeometry=" << hasActiveNativeDockGeometryAnimation(detachedDock.data(), detachedDock.data())
                << " activeWorkspaceGeometry=" << hasActiveNativeDockGeometryAnimation(workspace, detachedDock.data())
                << " headerVisible=" << (returnedHeader && returnedHeader->isVisible())
                << " headerParent=" << (returnedHeader && returnedHeader->parentWidget() == detachedDock.data())
                << " toolbarVisible=" << (groupTools && groupTools->isVisible())
                << " returnVisible=" << (returnDock && returnDock->isVisible())
                << " dockVisibleRegionEmpty=" << detachedDock->visibleRegion().isEmpty()
                << " unchanged=" << unchanged() << '\n';
            continue;
        }
        const QPointer<QWidget> header(detachedDock->titleBarWidget());
        QWidget* const central = workspace->centralWidget();
        const int requiredWidth = (central ? std::max(central->minimumWidth(), central->minimumSizeHint().width()) : 0)
            + std::max(primaryDock->minimumWidth(), primaryDock->minimumSizeHint().width())
            + std::max(detachedDock->minimumWidth(), detachedDock->minimumSizeHint().width())
            + 2 * std::max(1, workspace->style()->pixelMetric(QStyle::PM_DockWidgetSeparatorExtent));
        const auto growOwnerForWorkspaceDeficit = [&]()
        {
            const int deficit = requiredWidth - workspace->width();
            if ( deficit <= 0 )
            {
                return;
            }
            const QRect frame(window.frameGeometry());
            const int frameWidthExtra = frame.width() - window.width();
            const int maximumClientWidth = available.right() + 1 - frame.left() - frameWidthExtra;
            const int requestedWidth = window.width() + deficit;
            if ( requestedWidth <= maximumClientWidth )
            {
                window.resize(requestedWidth, window.height());
            }
        };
        const auto nativeSplitSpaceAvailable = [&]()
        {
            return workspace->width() >= requiredWidth
                && available.contains(window.frameGeometry())
                && !hasActiveWorkspaceGeometryAnimation();
        };
        growOwnerForWorkspaceDeficit();
        QElapsedTimer workspaceDeadline{};
        workspaceDeadline.start();
        while ( !nativeSplitSpaceAvailable() && workspaceDeadline.elapsed() < 2000 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
        }
        const bool enoughSpace = nativeSplitSpaceAvailable();
        check(enoughSpace, "detached resource chrome: actual split has sufficient native minimum-width space");
        const QRect primaryBounds(primaryDock->mapToGlobal(QPoint{}), primaryDock->size());
        const QRect workspaceBounds(workspace->mapToGlobal(QPoint{}), workspace->size());
        const QRect returnedBounds(detachedDock->mapToGlobal(QPoint{}), detachedDock->size());
        const QRect dockStackBounds = primaryBounds.united(returnedBounds).intersected(workspaceBounds);
        QPoint destination(primaryBounds.right() - 6, dockStackBounds.center().y());
        const auto splitEstablished = [&]()
        {
            if ( !workspaceGuard || !primaryDockGuard || !detachedDockGuard )
            {
                return false;
            }
            const QRect bounds(detachedDock->mapToGlobal(QPoint{}), detachedDock->size());
            const QRect primary(primaryDock->mapToGlobal(QPoint{}), primaryDock->size());
            return !detachedDock->isFloating() && !workspace->tabifiedDockWidgets(primaryDock).contains(detachedDock.data())
                && bounds.left() >= primary.right() && bounds.intersects(QRect(
                    primary.right(), primary.top(), bounds.width() + 2, primary.height()));
        };
        bool headerDragDispatched = false;
        bool nativeGapVisible = false;
        QRect nativeGapGeometry{};
        bool nativeGrabStarted = false;
        bool nativeGrabAtTarget = false;
        bool targetMoveAccepted = false;
        bool releaseAccepted = false;
        QRect boundsAfterBegin{};
        QRect primaryBoundsAfterBegin{};
        QRect boundsAtTarget{};
        QString destinationWidget{};
        QString destinationClass{};
        QString destinationAncestors{};
        bool destinationInPrimary = false;
        bool destinationInMainWindow = false;
        bool modalAtTarget = false;
        bool nativeHoverSettled = false;
        bool nativePlugSettled = false;
        bool originalStackMiddleThird = false;
        QRect primaryBoundsAfterHover{};
        if ( enoughSpace && header && header->isVisible() && available.contains(destination) )
        {
            const QPoint start(header->width() / 2, header->height() / 2);
            const QPoint startGlobal = header->mapToGlobal(start);
            const QPoint cursorBefore = QCursor::pos();
            QCursor::setPos(startGlobal);
            QMouseEvent press(QEvent::MouseButtonPress, QPointF(start), QPointF(startGlobal),
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(header.data(), &press);
            QCursor::setPos(startGlobal + QPoint(24, 12));
            QMouseEvent beginMove(QEvent::MouseMove, QPointF(start + QPoint(24, 12)),
                QPointF(startGlobal + QPoint(24, 12)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(header.data(), &beginMove);
            const QPointer<QWidget> grabbed(QWidget::mouseGrabber());
            nativeGrabStarted = grabbed == detachedDock.data();
            boundsAfterBegin = QRect(detachedDock->mapToGlobal(QPoint{}), detachedDock->size());
            check(nativeGrabStarted,
                "detached resource chrome: the returned-file grip starts a real Qt dock mouse grab before its edge drop");
            // Unplug computes the prospective dock layout without applying it
            // to the visible widgets. Use the public union of the original
            // stack for the first real hover. Keep that accepted gap target
            // through release while Qt applies its prospective geometry.
            primaryBoundsAfterBegin = QRect(primaryDock->mapToGlobal(QPoint{}), primaryDock->size());
            check(available.contains(destination) && workspaceBounds.contains(destination)
                    && dockStackBounds.contains(destination) && destination.x() == primaryBounds.right() - 6,
                "detached resource chrome: first held move targets the reachable native right edge at the full dock-stack center");
            if ( QWidget* const target = QApplication::widgetAt(destination) )
            {
                destinationWidget = target->objectName();
                destinationClass = QString::fromLatin1(target->metaObject()->className());
                destinationInPrimary = target == primaryDock || primaryDock->isAncestorOf(target);
                destinationInMainWindow = target->window() == &window;
                for ( QWidget* ancestor = target; ancestor; ancestor = ancestor->parentWidget() )
                {
                    if ( !destinationAncestors.isEmpty() )
                    {
                        destinationAncestors += QStringLiteral(" <- ");
                    }
                    destinationAncestors += ancestor->objectName() + QLatin1Char(':')
                        + QString::fromLatin1(ancestor->metaObject()->className());
                }
            }
            modalAtTarget = QApplication::activeModalWidget() != nullptr;
            QCursor::setPos(destination);
            // Qt's dock startDrag grabs the dock itself. Follow-on native mouse
            // input goes to that grabber, rather than the original child grip.
            // sendEvent has an explicit receiver and does not retarget it.
            const QPointer<QWidget> moveReceiver(grabbed ? grabbed.data() : header.data());
            const QPoint targetLocal = moveReceiver->mapFromGlobal(destination);
            QMouseEvent move(QEvent::MouseMove, QPointF(targetLocal), QPointF(destination),
                Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(moveReceiver.data(), &move);
            targetMoveAccepted = move.isAccepted();
            nativeGrabAtTarget = QWidget::mouseGrabber() == detachedDock.data();
            const auto nativeHoverComplete = [&]()
            {
                if ( !workspaceGuard || !primaryDockGuard || !detachedDockGuard
                    || QWidget::mouseGrabber() != detachedDockGuard.data()
                    || hasActiveWorkspaceGeometryAnimation() )
                {
                    return false;
                }
                QWidget* const gap = workspaceGuard->findChild<QWidget*>(QStringLiteral("qt_rubberband"));
                return gap && gap->isVisible() && !gap->size().isEmpty();
            };
            QElapsedTimer hoverDeadline{};
            hoverDeadline.start();
            while ( !nativeHoverComplete() && hoverDeadline.elapsed() < 2000 )
            {
                QApplication::processEvents(QEventLoop::AllEvents, 25);
            }
            nativeHoverSettled = nativeHoverComplete();
            primaryBoundsAfterHover = QRect(primaryDock->mapToGlobal(QPoint{}), primaryDock->size());
            boundsAtTarget = QRect(detachedDock->mapToGlobal(QPoint{}), detachedDock->size());
            check(nativeGrabAtTarget,
                "detached resource chrome: the native dock grab remains active at the real split target");
            check(nativeHoverSettled,
                "detached resource chrome: actual native hover animation settles with the dock gap visible before release");
            originalStackMiddleThird = destination.y() - dockStackBounds.top() > dockStackBounds.height() / 3
                && destination.y() - dockStackBounds.top() < 2 * dockStackBounds.height() / 3;
            check(originalStackMiddleThird,
                "detached resource chrome: native split target stays in the original dock stack's middle third");
            if ( QWidget* const nativeGap = workspace->findChild<QWidget*>(QStringLiteral("qt_rubberband")) )
            {
                nativeGapVisible = nativeGap->isVisible();
                nativeGapGeometry = QRect(nativeGap->mapToGlobal(QPoint{}), nativeGap->size());
            }
            const QPointer<QWidget> releaseReceiver(QWidget::mouseGrabber()
                ? QWidget::mouseGrabber() : header.data());
            QMouseEvent release(QEvent::MouseButtonRelease,
                QPointF(releaseReceiver->mapFromGlobal(destination)), QPointF(destination),
                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(releaseReceiver.data(), &release);
            releaseAccepted = release.isAccepted();
            QElapsedTimer deadline{};
            deadline.start();
            const auto nativePlugComplete = [&]()
            {
                if ( !workspaceGuard || !primaryDockGuard || !detachedDockGuard
                    || !splitEstablished() || hasActiveWorkspaceGeometryAnimation() )
                {
                    return false;
                }
                QWidget* const settledHeader = detachedDockGuard->titleBarWidget();
                QWidget* const settledToolbar = groupTools.data();
                return settledHeader && settledHeader->isVisible()
                    && settledToolbar && settledToolbar->isVisible();
            };
            while ( !nativePlugComplete() && deadline.elapsed() < 2000 )
            {
                QApplication::processEvents(QEventLoop::AllEvents, 25);
            }
            nativePlugSettled = nativePlugComplete();
            QCursor::setPos(cursorBefore);
            headerDragDispatched = nativeGrabStarted && nativeGrabAtTarget;
            const QWidget* const settledHeader = detachedDock->titleBarWidget();
            const bool splitAfterRelease = splitEstablished();
            const bool headerVisibleAfterRelease = settledHeader && settledHeader->isVisible();
            const bool toolbarVisibleAfterRelease = groupTools && groupTools->isVisible();
            const bool unchangedAfterRelease = unchanged();
            if ( !headerDragDispatched || !splitAfterRelease || !nativePlugSettled
                || !originalStackMiddleThird
                || !headerVisibleAfterRelease || !toolbarVisibleAfterRelease || !unchangedAfterRelease )
            {
                const QRect primaryAfterRelease(primaryDock->mapToGlobal(QPoint{}), primaryDock->size());
                const QRect returnedAfterRelease(detachedDock->mapToGlobal(QPoint{}), detachedDock->size());
                std::cerr << "returned resource post-release completion: kind=" << static_cast<int>(fixture.kind)
                    << " dispatched=" << headerDragDispatched << " split=" << splitAfterRelease
                    << " settled=" << nativePlugSettled
                    << " originalStackMiddleThird=" << originalStackMiddleThird
                    << " activeWorkspaceGeometry=" << hasActiveWorkspaceGeometryAnimation()
                    << " activeReturnedGeometry=" << hasActiveNativeDockGeometryAnimation(workspace,
                        detachedDock.data())
                    << " headerVisible=" << headerVisibleAfterRelease
                    << " toolbarVisible=" << toolbarVisibleAfterRelease
                    << " unchanged=" << unchangedAfterRelease
                    << " primary=" << primaryAfterRelease.x() << ',' << primaryAfterRelease.y()
                    << ',' << primaryAfterRelease.width() << ',' << primaryAfterRelease.height()
                    << " returned=" << returnedAfterRelease.x() << ',' << returnedAfterRelease.y()
                    << ',' << returnedAfterRelease.width() << ',' << returnedAfterRelease.height()
                    << " originalStack=" << dockStackBounds.x() << ',' << dockStackBounds.y()
                    << ',' << dockStackBounds.width() << ',' << dockStackBounds.height()
                    << " target=" << destination.x() << ',' << destination.y() << '\n';
            }
        }
        if ( enoughSpace && (!headerDragDispatched || !splitEstablished() || !nativePlugSettled
                || !originalStackMiddleThird
                || !detachedDock->titleBarWidget() || !detachedDock->titleBarWidget()->isVisible()
                || !groupTools || !groupTools->isVisible() || !unchanged()) )
        {
            const QRect first(primaryDock->mapToGlobal(QPoint{}), primaryDock->size());
            const QRect second(detachedDock->mapToGlobal(QPoint{}), detachedDock->size());
            std::cerr << "returned resource grip split: kind=" << static_cast<int>(fixture.kind)
                << " dispatched=" << headerDragDispatched << " floating=" << detachedDock->isFloating()
                << " tabified=" << workspace->tabifiedDockWidgets(primaryDock).contains(detachedDock.data())
                << " primary=" << first.x() << ',' << first.y() << ',' << first.width() << ',' << first.height()
                << " returned=" << second.x() << ',' << second.y() << ',' << second.width() << ',' << second.height()
                << " destination=" << destination.x() << ',' << destination.y()
                << " workspaceWidth=" << workspace->width() << " requiredWidth=" << requiredWidth
                << " workspaceHeight=" << workspace->height()
                << " primaryMinimumHeight=" << std::max(primaryDock->minimumHeight(), primaryDock->minimumSizeHint().height())
                << " returnedMinimumHeight=" << std::max(detachedDock->minimumHeight(), detachedDock->minimumSizeHint().height())
                << " nativeGrabStarted=" << nativeGrabStarted << " nativeGrabAtTarget=" << nativeGrabAtTarget
                << " targetMoveAccepted=" << targetMoveAccepted << " releaseAccepted=" << releaseAccepted
                << " destinationWidget=" << destinationWidget.toStdString() << ':' << destinationClass.toStdString()
                << " destinationInPrimary=" << destinationInPrimary
                << " destinationInMainWindow=" << destinationInMainWindow << " activeModal=" << modalAtTarget
                << " destinationAncestors=" << destinationAncestors.toStdString()
                << " primaryBefore=" << primaryBounds.x() << ',' << primaryBounds.y()
                << ',' << primaryBounds.width() << ',' << primaryBounds.height()
                << " primaryAfterBegin=" << primaryBoundsAfterBegin.x() << ',' << primaryBoundsAfterBegin.y()
                << ',' << primaryBoundsAfterBegin.width() << ',' << primaryBoundsAfterBegin.height()
                << " primaryAfterHover=" << primaryBoundsAfterHover.x() << ',' << primaryBoundsAfterHover.y()
                << ',' << primaryBoundsAfterHover.width() << ',' << primaryBoundsAfterHover.height()
                << " afterBegin=" << boundsAfterBegin.x() << ',' << boundsAfterBegin.y()
                << ',' << boundsAfterBegin.width() << ',' << boundsAfterBegin.height()
                << " atTarget=" << boundsAtTarget.x() << ',' << boundsAtTarget.y()
                << ',' << boundsAtTarget.width() << ',' << boundsAtTarget.height()
                << " nativeGapVisible=" << nativeGapVisible
                << " nativeHoverSettled=" << nativeHoverSettled
                << " nativePlugSettled=" << nativePlugSettled
                << " originalStackMiddleThird=" << originalStackMiddleThird
                << " nativeGap=" << nativeGapGeometry.x() << ',' << nativeGapGeometry.y()
                << ',' << nativeGapGeometry.width() << ',' << nativeGapGeometry.height() << '\n';
            for ( QDockWidget* const dock : workspace->findChildren<QDockWidget*>() )
            {
                if ( dock->isVisible() && !dock->isFloating() )
                {
                    const QSize minimum = dock->minimumSize().expandedTo(dock->minimumSizeHint());
                    std::cerr << "returned resource visible minimum: " << dock->objectName().toStdString()
                        << " minimum=" << minimum.width() << ',' << minimum.height()
                        << " size=" << dock->width() << ',' << dock->height() << '\n';
                }
            }
        }
        check(headerDragDispatched && nativePlugSettled && originalStackMiddleThird
                && splitEstablished() && unchanged()
                && detachedDock->titleBarWidget() && detachedDock->titleBarWidget()->isVisible()
                && groupTools && groupTools->isVisible(),
            "detached resource chrome: a real returned-file grip drag docks into a native split with its toolbar and exact journal");
        if ( !detachedTabs || !page )
        {
            continue;
        }
        primaryDock->show();
        primaryDock->raise();
        QApplication::processEvents();
        const TabDragOutcome returned = sendTabDrag(detachedTabs->tabBar(),
            detachedTabs->tabBar()->tabRect(detachedTabs->indexOf(page.data())).center(),
            primaryTabs->tabBar()->mapToGlobal(primaryTabs->tabBar()->tabRect(0).center()), false);
        check(returned.terminalEventsDispatched && !returned.timedOut
                && resourceTabHostContaining(workspace, page.data()) == primaryTabs
                && primaryTabs->count() == 2 && primaryTabs->currentWidget() == page.data() && unchanged(),
            "detached resource chrome: real same-kind document-bar transfer returns the exact file and history to its primary group");
    }
}

void testResourceGroupNativeSplitsAndCanvasContrast()
{
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    QScreen* const screen = QGuiApplication::primaryScreen();
    check(screen != nullptr, "native resource split fixture has an available screen");
    if ( !screen )
    {
        return;
    }
    const QRect available = screen->availableGeometry();
    const QSize maximumClientSize(std::max(1, available.width() - 40), std::max(1, available.height() - 80));
    window.setMinimumSize(window.minimumSize().boundedTo(maximumClientSize));
    window.resize(QSize(2000, 960).boundedTo(maximumClientSize));
    window.move(available.topLeft() + QPoint(20, 20));
    window.show();
    window.activateWindow();
    QApplication::processEvents();
    const bool windowContained = available.contains(window.frameGeometry());
    check(windowContained, "native resource split fixture keeps the entire main window on its available screen");
    if ( !windowContained )
    {
        std::cerr << "native resource split screen: available=" << available.x() << ',' << available.y()
            << ',' << available.width() << ',' << available.height()
            << " window=" << window.frameGeometry().x() << ',' << window.frameGeometry().y()
            << ',' << window.frameGeometry().width() << ',' << window.frameGeometry().height() << '\n';
        return;
    }
    QMainWindow* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
    QDockWidget* const sprite = window.findChild<QDockWidget*>(QStringLiteral("resourceEditorsDock"));
    QDockWidget* const sound = window.findChild<QDockWidget*>(QStringLiteral("soundClassResourceEditorsDock"));
    QDockWidget* const uni = window.findChild<QDockWidget*>(QStringLiteral("uniResourceEditorsDock"));
    check(workspace && sprite && sound && uni,
        "native resource split fixture resolves all editor groups");
    if ( !workspace || !sprite || !sound || !uni )
    {
        return;
    }
    workspace->setAnimated(false);
    uni->hide();
    sprite->show();
    sprite->raise();
    sound->show();
    QApplication::processEvents();
    QWidget* const central = workspace->centralWidget();
    const int centralMinimumWidth = central
        ? std::max(central->minimumWidth(), central->minimumSizeHint().width()) : 0;
    const int separatorWidth = std::max(1,
        workspace->style()->pixelMetric(QStyle::PM_DockWidgetSeparatorExtent));
    const int horizontalMinimumWidth = centralMinimumWidth
        + std::max(sprite->minimumWidth(), sprite->minimumSizeHint().width())
        + std::max(sound->minimumWidth(), sound->minimumSizeHint().width())
        + separatorWidth * 2;
    const bool enoughHorizontalSpace = workspace->width() >= horizontalMinimumWidth;
    check(enoughHorizontalSpace,
        "native resource split fixture has room for both dock minima, the central widget, and native separators");
    if ( !enoughHorizontalSpace )
    {
        std::cerr << "native resource split space: workspaceWidth=" << workspace->width()
            << " requiredWidth=" << horizontalMinimumWidth
            << " centralMinimum=" << centralMinimumWidth << " separator=" << separatorWidth << '\n';
        return;
    }
    const auto globalRect = [](QWidget* widget)
    {
        return QRect(widget->mapToGlobal(QPoint{}), widget->size());
    };
    const auto headerAndToolbarVisible = [globalRect](QDockWidget* dock)
    {
        QWidget* const header = dock->titleBarWidget();
        QWidget* const toolbar = dock->findChild<QWidget*>(QStringLiteral("resource_authoring.group_toolbar"));
        QLabel* const title = header ? header->findChild<QLabel*>(QStringLiteral("resource_group.title")) : nullptr;
        return dock->isVisible() && header && header->isVisible() && toolbar && toolbar->isVisible()
            && globalRect(dock).contains(globalRect(header)) && globalRect(dock).contains(globalRect(toolbar))
            && title && title->text() == dock->windowTitle()
            && title->contentsRect().width() >= title->fontMetrics().horizontalAdvance(title->text());
    };
    for ( const Qt::Orientation orientation : { Qt::Vertical, Qt::Horizontal } )
    {
        sound->setFloating(true);
        sound->show();
        const auto captionReady = [sound]()
        {
            return sound->isFloating() && !sound->titleBarWidget()
                && !sound->windowFlags().testFlag(Qt::FramelessWindowHint)
                && sound->geometry().top() > sound->frameGeometry().top();
        };
        QElapsedTimer captionDeadline{};
        captionDeadline.start();
        while ( !captionReady() && captionDeadline.elapsed() < 2000 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
        }
        sound->move(available.topLeft() + QPoint(
            std::max(0, available.width() - sound->frameGeometry().width() - 20), 80));
        sprite->show();
        sprite->raise();
        QApplication::processEvents();
        const QRect nativeFrame = sound->frameGeometry();
        const QRect nativeClient = sound->geometry();
        const QRect nativeCaption(nativeClient.left(), nativeFrame.top(), nativeClient.width(),
            std::max(0, nativeClient.top() - nativeFrame.top()));
        bool dragDispatched = false;
        bool nativeGapVisible = false;
        QRect nativeGapGeometry{};
        QPoint dragDestination{};
        QString destinationWidget{};
        QString destinationClass{};
        const auto splitEstablished = [workspace, sprite, sound, orientation, globalRect]()
        {
            if ( sound->isFloating() || workspace->tabifiedDockWidgets(sprite).contains(sound) )
            {
                return false;
            }
            const QRect first = globalRect(sprite);
            const QRect second = globalRect(sound);
            const int overlapX = std::max(0, std::min(first.right(), second.right())
                - std::max(first.left(), second.left()) + 1);
            const int overlapY = std::max(0, std::min(first.bottom(), second.bottom())
                - std::max(first.top(), second.top()) + 1);
            return orientation == Qt::Vertical
                ? second.top() >= first.bottom() && overlapX * 4 >= std::min(first.width(), second.width()) * 3
                : second.left() >= first.right() && overlapY * 4 >= std::min(first.height(), second.height()) * 3;
        };
        const bool decoratedCaption = sound->isFloating() && !sound->titleBarWidget()
            && !sound->windowFlags().testFlag(Qt::FramelessWindowHint) && nativeCaption.height() > 0;
        check(decoratedCaption, "native resource split fixture drags the actual decorated floating caption");
        if ( !decoratedCaption )
        {
            std::cerr << "native resource caption precondition: orientation=" << static_cast<int>(orientation)
                << " floating=" << sound->isFloating()
                << " customTitle=" << (sound->titleBarWidget() != nullptr)
                << " frameless=" << sound->windowFlags().testFlag(Qt::FramelessWindowHint)
                << " frame=" << nativeFrame.x() << ',' << nativeFrame.y()
                << ',' << nativeFrame.width() << ',' << nativeFrame.height()
                << " client=" << nativeClient.x() << ',' << nativeClient.y()
                << ',' << nativeClient.width() << ',' << nativeClient.height()
                << " mouseGrab=" << (QWidget::mouseGrabber() == sound) << '\n';
        }
        if ( decoratedCaption && sprite->isVisible() )
        {
            const QPoint startGlobal = nativeCaption.center();
            const QPoint start = sound->mapFromGlobal(startGlobal);
            const QPoint frameGrabOffset = startGlobal - sound->pos();
            const QRect target = globalRect(sprite);
            QPoint destination = orientation == Qt::Vertical
                ? QPoint(target.center().x(), target.bottom() - 6)
                : QPoint(target.right() - 6, target.center().y());
            const bool dragOnScreen = available.contains(startGlobal) && available.contains(destination);
            check(dragOnScreen,
                "native resource split fixture places both header press and edge drop inside the available screen");
            if ( !dragOnScreen )
            {
                continue;
            }
            const QPoint cursorBefore = QCursor::pos();
            QCursor::setPos(startGlobal);
            QMouseEvent press(QEvent::NonClientAreaMouseButtonPress, QPointF(start), QPointF(startGlobal),
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(sound, &press);
            // Unplugging can resize the previous native group. Aim at its
            // actual edge after the press, using client-local NC coordinates
            // separately from the frame offset used to move the window.
            const QRect targetAfterPress = globalRect(sprite);
            destination = orientation == Qt::Vertical
                ? QPoint(targetAfterPress.center().x(), targetAfterPress.bottom() - 6)
                : QPoint(targetAfterPress.right() - 6, targetAfterPress.center().y());
            dragDestination = destination;
            check(available.contains(destination),
                "native resource split fixture keeps the actual post-press edge target inside the available screen");
            if ( QWidget* const targetWidget = QApplication::widgetAt(destination) )
            {
                destinationWidget = targetWidget->objectName();
                destinationClass = QString::fromLatin1(targetWidget->metaObject()->className());
            }
            QCursor::setPos(destination);
            // The Windows caption drag sends window moves while Qt tracks the
            // docking target, then a non-client move terminates the gesture.
            // This exercises that event route without selecting a dock layout.
            sound->move(destination - frameGrabOffset);
            if ( QWidget* const nativeGap = workspace->findChild<QWidget*>(QStringLiteral("qt_rubberband")) )
            {
                nativeGapVisible = nativeGap->isVisible();
                nativeGapGeometry = globalRect(nativeGap);
            }
            QMouseEvent release(QEvent::NonClientAreaMouseMove, QPointF(start), QPointF(destination),
                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(sound, &release);
            QElapsedTimer deadline{};
            deadline.start();
            // Qt can establish the native relation before the group's queued
            // custom-header restoration runs. Await the complete existing
            // visibility oracle, rather than sampling that intermediate turn.
            while ( (!splitEstablished() || !headerAndToolbarVisible(sprite)
                    || !headerAndToolbarVisible(sound)) && deadline.elapsed() < 2000 )
            {
                QApplication::processEvents(QEventLoop::AllEvents, 25);
            }
            QCursor::setPos(cursorBefore);
            dragDispatched = true;
        }
        const bool split = dragDispatched && splitEstablished();
        if ( !split )
        {
            const QRect first = globalRect(sprite);
            const QRect second = globalRect(sound);
            std::cerr << "native resource split: orientation=" << static_cast<int>(orientation)
                << " dispatched=" << dragDispatched << " floating=" << sound->isFloating()
                << " tabified=" << workspace->tabifiedDockWidgets(sprite).contains(sound)
                << " available=" << available.x() << ',' << available.y() << ',' << available.width() << ',' << available.height()
                << " sprite=" << first.x() << ',' << first.y() << ',' << first.width() << ',' << first.height()
                << " sound=" << second.x() << ',' << second.y() << ',' << second.width() << ',' << second.height()
                << " destination=" << dragDestination.x() << ',' << dragDestination.y()
                << " target=" << destinationWidget.toStdString() << ':' << destinationClass.toStdString()
                << " nativeGapVisible=" << nativeGapVisible
                << " nativeGap=" << nativeGapGeometry.x() << ',' << nativeGapGeometry.y()
                << ',' << nativeGapGeometry.width() << ',' << nativeGapGeometry.height() << '\n';
            const auto reportMinimums = [](const char* const name, QWidget* const widget)
            {
                if ( !widget )
                {
                    std::cerr << "native resource minimum: " << name << " missing\n";
                    return;
                }
                const QSize minimum = widget->minimumSize();
                const QSize hint = widget->minimumSizeHint();
                const QSize layoutMinimum = widget->layout() ? widget->layout()->minimumSize() : QSize{};
                std::cerr << "native resource minimum: " << name
                    << " size=" << widget->width() << ',' << widget->height()
                    << " minimum=" << minimum.width() << ',' << minimum.height()
                    << " hint=" << hint.width() << ',' << hint.height()
                    << " layout=" << layoutMinimum.width() << ',' << layoutMinimum.height() << '\n';
            };
            reportMinimums("workspace", workspace);
            reportMinimums("central", workspace->centralWidget());
            reportMinimums("Sprite dock", sprite);
            reportMinimums("Sprite body", sprite->widget());
            reportMinimums("Sound dock", sound);
            reportMinimums("Sound body", sound->widget());
        }
        check(split, orientation == Qt::Vertical
                ? "a real resource header drag into the lower dock edge establishes a vertical native split"
                : "a real resource header drag into the right dock edge establishes a horizontal native split");
        const bool spriteChromeVisible = headerAndToolbarVisible(sprite);
        const bool soundChromeVisible = headerAndToolbarVisible(sound);
        if ( !split || !spriteChromeVisible || !soundChromeVisible )
        {
            const auto reportChrome = [globalRect, workspace, orientation](const char* name,
                                                                         QDockWidget* dock)
            {
                QWidget* const header = dock->titleBarWidget();
                QWidget* const cachedHeader = dock->findChild<QWidget*>(
                    QStringLiteral("resource_authoring.group_header"));
                QWidget* const toolbar = dock->findChild<QWidget*>(
                    QStringLiteral("resource_authoring.group_toolbar"));
                QLabel* const title = header ? header->findChild<QLabel*>(
                    QStringLiteral("resource_group.title")) : nullptr;
                const QRect dockBounds = globalRect(dock);
                const QRect headerBounds = header ? globalRect(header) : QRect{};
                const QRect cachedHeaderBounds = cachedHeader ? globalRect(cachedHeader) : QRect{};
                const QRect toolbarBounds = toolbar ? globalRect(toolbar) : QRect{};
                const QSize minimum = dock->minimumSize();
                const QSize hint = dock->minimumSizeHint();
                const QSize layoutMinimum = dock->layout() ? dock->layout()->minimumSize() : QSize{};
                std::cerr << "native resource chrome: orientation=" << static_cast<int>(orientation)
                          << " name=" << name << " floating=" << dock->isFloating()
                          << " visible=" << dock->isVisible()
                          << " customTitle=" << (header != nullptr)
                          << " headerVisible=" << (header && header->isVisible())
                          << " cachedHeaderVisible=" << (cachedHeader && cachedHeader->isVisible())
                          << " toolbarVisible=" << (toolbar && toolbar->isVisible())
                          << " headerContained=" << (header && dockBounds.contains(headerBounds))
                          << " toolbarContained=" << (toolbar && dockBounds.contains(toolbarBounds))
                          << " titleMatches=" << (title && title->text() == dock->windowTitle())
                          << " titleWidth=" << (title ? title->contentsRect().width() : -1)
                          << " requiredTitleWidth=" << (title
                              ? title->fontMetrics().horizontalAdvance(title->text()) : -1)
                          << " dock=" << dockBounds.x() << ',' << dockBounds.y() << ','
                          << dockBounds.width() << ',' << dockBounds.height()
                          << " header=" << headerBounds.x() << ',' << headerBounds.y() << ','
                          << headerBounds.width() << ',' << headerBounds.height()
                          << " cachedHeader=" << cachedHeaderBounds.x() << ',' << cachedHeaderBounds.y() << ','
                          << cachedHeaderBounds.width() << ',' << cachedHeaderBounds.height()
                          << " toolbar=" << toolbarBounds.x() << ',' << toolbarBounds.y() << ','
                          << toolbarBounds.width() << ',' << toolbarBounds.height()
                          << " minimum=" << minimum.width() << ',' << minimum.height()
                          << " hint=" << hint.width() << ',' << hint.height()
                          << " layoutMinimum=" << layoutMinimum.width() << ',' << layoutMinimum.height()
                          << " workspace=" << workspace->width() << ',' << workspace->height()
                          << " mouseGrab=" << (QWidget::mouseGrabber() == dock) << '\n';
                if ( toolbar )
                {
                    for ( QToolButton* const button : toolbar->findChildren<QToolButton*>() )
                    {
                        const QRect bounds = globalRect(button);
                        std::cerr << "native resource chrome button: name="
                                  << button->objectName().toStdString()
                                  << " visible=" << button->isVisible()
                                  << " contained=" << toolbarBounds.contains(bounds)
                                  << " bounds=" << bounds.x() << ',' << bounds.y() << ','
                                  << bounds.width() << ',' << bounds.height() << '\n';
                    }
                }
            };
            reportChrome("Sprite", sprite);
            reportChrome("Sound", sound);
        }
        check(split && spriteChromeVisible && soundChromeVisible,
            "both native split orientations retain fully visible resource group titles, grips, and common toolbars");
    }
    for ( QDockWidget* const dock : { sprite, sound, uni } )
    {
        dock->show();
        dock->raise();
        QApplication::processEvents();
        QWidget* const header = dock->titleBarWidget() ? dock->titleBarWidget()
            : dock->findChild<QWidget*>(QStringLiteral("resource_authoring.group_header"));
        QWidget* const body = dock->widget();
        const QImage headerImage = header ? header->grab().toImage() : QImage{};
        const QImage bodyImage = body ? body->grab().toImage() : QImage{};
        const int headerGray = headerImage.width() > 2 && headerImage.height() > 2
            ? qGray(headerImage.pixel(1, 1)) : -1;
        const int bodyGray = bodyImage.width() > 2 && bodyImage.height() > 2
            ? qGray(bodyImage.pixel(1, 1)) : -1;
        check(headerGray >= 0 && bodyGray >= headerGray + 4 && bodyGray <= 100,
            "each resource editor paints a lighter dark canvas than its titlebar without turning into a bright panel");
    }
}

void testResourceGroupTopDockHeaderAndTabGrouping()
{
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.resize(1440, 960);
    window.show();
    window.activateWindow();
    QMainWindow* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
    QDockWidget* const sprite = window.findChild<QDockWidget*>(QStringLiteral("resourceEditorsDock"));
    QDockWidget* const sound = window.findChild<QDockWidget*>(QStringLiteral("soundClassResourceEditorsDock"));
    QDockWidget* const uni = window.findChild<QDockWidget*>(QStringLiteral("uniResourceEditorsDock"));
    check(workspace && sprite && sound && uni,
        "major resource docking fixture resolves all three native editor groups");
    if ( !workspace || !sprite || !sound || !uni )
    {
        return;
    }
    workspace->setAnimated(false);
    for ( QDockWidget* const dock : { sprite, sound, uni } )
    {
        dock->show();
    }
    uni->raise();
    QApplication::processEvents();
    const auto visibleGroupCount = [workspace, sprite, sound, uni]()
    {
        int count = 0;
        const QRect workspaceBounds(workspace->mapToGlobal(QPoint{}), workspace->size());
        for ( QDockWidget* const dock : { sprite, sound, uni } )
        {
            const QRect dockBounds(dock->mapToGlobal(QPoint{}), dock->size());
            if ( !dock->isHidden() && !dock->isFloating() && workspaceBounds.intersects(dockBounds) )
            {
                ++count;
            }
        }
        return count;
    };
    check(workspace->dockOptions().testFlag(QMainWindow::AllowTabbedDocks)
            && workspace->tabifiedDockWidgets(sprite).contains(sound)
            && workspace->tabifiedDockWidgets(sprite).contains(uni)
            && visibleGroupCount() == 1,
        "major resource groups use one visible editor and a shared Qt dock tab bar instead of vertical splits");
    sound->setFloating(true);
    sound->show();
    QApplication::processEvents();
    check(sound->isFloating(), "Sound Class group can float independently before top redocking");
    sound->setFloating(false);
    workspace->addDockWidget(Qt::TopDockWidgetArea, sound);
    sound->show();
    sound->raise();
    QApplication::processEvents();
    QWidget* const header = sound->titleBarWidget();
    QWidget* const toolbar = sound->findChild<QWidget*>(QStringLiteral("resource_authoring.group_toolbar"));
    check(!sound->isFloating() && workspace->dockWidgetArea(sound) == Qt::TopDockWidgetArea
            && header && header->isVisible() && header->height() > 0
            && toolbar && toolbar->isVisible() && toolbar->height() > 0
            && sound->rect().contains(header->mapTo(sound, header->rect().center()))
            && sound->rect().contains(toolbar->mapTo(sound, toolbar->rect().center()))
            && sound->features().testFlag(QDockWidget::DockWidgetMovable)
            && sound->features().testFlag(QDockWidget::DockWidgetFloatable),
        "float then TopDock preserves the Sound Class title/grip and toolbar inside a dock that can move again");

    const auto headerPopupActions = [](QDockWidget* const dock)
    {
        std::vector<QString> labels{};
        QWidget* const target = dock->titleBarWidget() ? dock->titleBarWidget() : dock;
        QTimer poll{};
        QTimer watchdog{};
        watchdog.setSingleShot(true);
        const auto capturePopup = [&labels]()
        {
            QMenu* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if ( !menu )
            {
                return;
            }
            for ( QAction* const action : menu->actions() )
            {
                if ( action && !action->isSeparator() && action->isVisible() )
                {
                    labels.push_back(action->text().remove(QLatin1Char('&')));
                }
            }
            menu->close();
        };
        QObject::connect(&poll, &QTimer::timeout, &poll, capturePopup);
        QObject::connect(&watchdog, &QTimer::timeout, &poll, []()
            {
                if ( QWidget* const popup = QApplication::activePopupWidget() )
                {
                    popup->close();
                }
            });
        poll.start(5);
        watchdog.start(2000);
        const QPoint position(target->width() / 2, std::min(8, target->height() / 2));
        QContextMenuEvent context(QContextMenuEvent::Mouse, position, target->mapToGlobal(position));
        QApplication::sendEvent(target, &context);
        capturePopup();
        poll.stop();
        watchdog.stop();
        return labels;
    };
    check(headerPopupActions(sound) == std::vector<QString>{ QStringLiteral("Undock") },
        "a docked resource group header right-click offers only Undock and no text or unrelated dock actions");
    sound->setFloating(true);
    sound->show();
    QApplication::processEvents();
    check(headerPopupActions(sound).empty(),
        "a floating resource group header right-click does not open a context menu");
    sound->setFloating(false);
    workspace->addDockWidget(Qt::TopDockWidgetArea, sound);
    sound->show();
    sprite->raise();
    QApplication::processEvents();
    QWidget* const dragHeader = sound->titleBarWidget();
    bool dragDispatched = false;
    if ( dragHeader && dragHeader->isVisible() )
    {
        const QPoint start(dragHeader->width() / 2, dragHeader->height() / 2);
        const QPoint startGlobal = dragHeader->mapToGlobal(start);
        const QPoint releaseGlobal = sprite->mapToGlobal(sprite->rect().center());
        const QPoint releaseLocal = dragHeader->mapFromGlobal(releaseGlobal);
        const QPoint cursorBefore = QCursor::pos();
        QCursor::setPos(startGlobal);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(start), QPointF(startGlobal),
            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(dragHeader, &press);
        QCursor::setPos(startGlobal + QPoint(24, 12));
        QMouseEvent beginMove(QEvent::MouseMove, QPointF(start + QPoint(24, 12)),
            QPointF(startGlobal + QPoint(24, 12)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(dragHeader, &beginMove);
        QCursor::setPos(releaseGlobal);
        QMouseEvent move(QEvent::MouseMove, QPointF(releaseLocal), QPointF(releaseGlobal),
            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(dragHeader, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(releaseLocal), QPointF(releaseGlobal),
            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(dragHeader, &release);
        QElapsedTimer dragDeadline{};
        dragDeadline.start();
        while ( !workspace->tabifiedDockWidgets(sprite).contains(sound)
            && dragDeadline.elapsed() < 2000 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
        }
        QCursor::setPos(cursorBefore);
        dragDispatched = true;
    }
    check(dragDispatched && !sound->isFloating()
            && workspace->tabifiedDockWidgets(sprite).contains(sound),
        "the visible Sound Class header can drag from TopDock into the Sprite major dock tab group");
    workspace->tabifyDockWidget(sprite, uni);
    uni->raise();
    QApplication::processEvents();
    check(workspace->tabifiedDockWidgets(sprite).contains(sound)
            && workspace->tabifiedDockWidgets(sprite).contains(uni) && visibleGroupCount() == 1,
        "resource groups remain tabified with one visible editor after top redocking and repeated tab grouping");
    sound->setFloating(false);
    workspace->addDockWidget(Qt::TopDockWidgetArea, sound);
    sound->show();
    sound->raise();
    QApplication::processEvents();
    const QPointer<QDockWidget> guardedDock(sound);
    const QPointer<QWidget> guardedHeader(sound->titleBarWidget());
    bool popupObserved = false;
    bool popupTimedOut = false;
    QTimer destroyOwner{};
    QTimer popupDeadline{};
    popupDeadline.setSingleShot(true);
    QObject::connect(&destroyOwner, &QTimer::timeout, &destroyOwner, [&]()
        {
            if ( qobject_cast<QMenu*>(QApplication::activePopupWidget()) && guardedDock )
            {
                popupObserved = true;
                destroyOwner.stop();
                delete guardedDock.data();
            }
        });
    QObject::connect(&popupDeadline, &QTimer::timeout, &destroyOwner, [&]()
        {
            popupTimedOut = true;
            if ( QWidget* const popup = QApplication::activePopupWidget() )
            {
                popup->close();
            }
        });
    if ( guardedHeader )
    {
        destroyOwner.start(5);
        popupDeadline.start(2000);
        const QPoint position(guardedHeader->width() / 2, guardedHeader->height() / 2);
        QContextMenuEvent context(QContextMenuEvent::Mouse, position, guardedHeader->mapToGlobal(position));
        QApplication::sendEvent(guardedHeader.data(), &context);
        destroyOwner.stop();
        popupDeadline.stop();
    }
    check(popupObserved && !popupTimedOut && !guardedDock && !guardedHeader
            && QApplication::activePopupWidget() == nullptr,
        "destroying a resource group while its header menu is open safely closes the parentless popup");
}

void testResourceGroupSaveAllIncludesDetachedMembers()
{
    QTemporaryDir temporary{};
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.resize(1280, 860);
    window.show();
    QApplication::processEvents();
    QAction* const createSprite = window.findChild<QAction*>(QStringLiteral("tools.resource_editors.sprite_spr.new"));
    QAction* const createUni = window.findChild<QAction*>(QStringLiteral("tools.resource_editors.string_table_uni.new"));
    QMainWindow* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
    if ( createSprite )
    {
        createResourceDraftFromGroup(createSprite);
        createResourceDraftFromGroup(createSprite);
    }
    if ( createUni )
    {
        createResourceDraftFromGroup(createUni);
    }
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("resourceEditorTabs"));
    QTabWidget* const uniTabs = window.findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"));
    QDockWidget* const dock = window.findChild<QDockWidget*>(QStringLiteral("resourceEditorsDock"));
    QAction* const saveAll = dock ? dock->findChild<QAction*>(QStringLiteral("resource_authoring.save_all")) : nullptr;
    check(temporary.isValid() && workspace && tabs && tabs->count() == 2
            && uniTabs && uniTabs->count() == 1 && saveAll,
        "detached group Save All fixture creates two Sprite drafts and one unrelated UNI draft");
    if ( !temporary.isValid() || !workspace || !tabs || tabs->count() != 2
        || !uniTabs || uniTabs->count() != 1 || !saveAll )
    {
        return;
    }
    QWidget* const first = tabs->widget(0);
    QPointer<QWidget> detached(tabs->widget(1));
    QWidget* const unrelated = uniTabs->widget(0);
    const QCborMap unrelatedBefore = ui::resources::captureResourceAuthoringPaneState(unrelated);
    QLineEdit* const firstMaterial = first->findChild<QLineEdit*>(QStringLiteral("spr.material"));
    QLineEdit* const secondMaterial = detached->findChild<QLineEdit*>(QStringLiteral("spr.material"));
    check(firstMaterial && secondMaterial,
        "detached group Save All fixture exposes both Sprite material fields");
    if ( !firstMaterial || !secondMaterial )
    {
        return;
    }
    firstMaterial->setText(QStringLiteral("group_a.mat"));
    secondMaterial->setText(QStringLiteral("group_b.mat"));
    dock->show();
    dock->raise();
    tabs->setCurrentWidget(detached.data());
    QApplication::processEvents();
    const auto outside = resourceTabDropPoint(workspace, tabs->tabBar());
    if ( !outside )
    {
        check(false, "detached group Save All fixture has an outside-host drop point");
        return;
    }
    const TabDragOutcome drag = sendTabDrag(tabs->tabBar(), tabs->tabBar()->tabRect(1).center(), *outside, false);
    QTabWidget* const detachedTabs = detached ? resourceTabHostContaining(workspace, detached.data()) : nullptr;
    check(drag.terminalEventsDispatched && !drag.timedOut && detachedTabs && detachedTabs != tabs,
        "detached group Save All fixture transfers the existing Sprite pane to its own host");
    if ( !detachedTabs || detachedTabs == tabs )
    {
        return;
    }
    QDockWidget* const detachedDock = dockForResourceTabHost(detachedTabs);
    QSplitter* const viewportSplitter = qobject_cast<QSplitter*>(workspace->centralWidget());
    QWidget* const viewport = viewportSplitter ? viewportSplitter->widget(0) : nullptr;
    const auto focusAndWait = [](QWidget* const topLevel, QWidget* const target)
    {
        if ( !topLevel || !target )
        {
            return false;
        }
        topLevel->raise();
        topLevel->activateWindow();
        target->setFocus(Qt::OtherFocusReason);
        QElapsedTimer deadline{};
        deadline.start();
        while ( QApplication::focusWidget() != target && deadline.elapsed() < 2000 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
        }
        return QApplication::focusWidget() == target;
    };
    const auto capturedActiveIdentity = [&window]()
    {
        const QCborMap resources = QCborValue::fromCbor(window.captureRecoveryEditorStateForTest()).toMap()
            .value(QStringLiteral("resourceEditors")).toMap();
        const QCborArray records = resources.value(QStringLiteral("tabs")).toArray();
        const qint64 active = resources.value(QStringLiteral("active")).toInteger(-1);
        if ( active < 0 || active >= records.size() )
        {
            return QString{};
        }
        const QCborMap state = records.at(active).toMap().value(QStringLiteral("page")).toMap();
        return QStringLiteral("draft:") + state.value(QStringLiteral("draftIdentity")).toString();
    };
    check(tabs->currentWidget() == first,
        "primary focus fixture retains its already-selected file after detaching its sibling");
    dock->show();
    dock->raise();
    QElapsedTimer primaryVisibleDeadline{};
    primaryVisibleDeadline.start();
    while ( firstMaterial->visibleRegion().isEmpty() && primaryVisibleDeadline.elapsed() < 2000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    const bool primaryFocused = focusAndWait(&window, firstMaterial);
    const bool viewportAfterPrimary = focusAndWait(&window, viewport);
    const QString primaryCapturedIdentity = capturedActiveIdentity();
    const bool primaryRemainedActive = primaryFocused && viewportAfterPrimary
        && primaryCapturedIdentity == ui::resources::resourceAuthoringPaneIdentity(first);
    if ( !primaryRemainedActive )
    {
        std::cerr << "primary resource focus: focused=" << primaryFocused
            << " viewportFocused=" << viewportAfterPrimary
            << " fieldVisible=" << !firstMaterial->visibleRegion().isEmpty()
            << " dockVisible=" << dock->isVisible()
            << " lastHost=" << window.property("resource_authoring.lastActiveHostId").toString().toStdString()
            << " activeWindow=" << (QApplication::activeWindow()
                    ? QApplication::activeWindow()->objectName().toStdString() : std::string{})
            << " primaryCurrent=" << ui::resources::resourceAuthoringPaneIdentity(tabs->currentWidget()).toStdString()
            << " captured=" << primaryCapturedIdentity.toStdString()
            << " expected=" << ui::resources::resourceAuthoringPaneIdentity(first).toStdString() << '\n';
    }
    check(primaryRemainedActive,
        "focus on an already-selected primary resource file remains the global active file after viewport focus");
    const bool detachedFocused = focusAndWait(detachedDock, secondMaterial);
    const bool viewportAfterDetached = focusAndWait(&window, viewport);
    check(detachedFocused && viewportAfterDetached && detached
            && capturedActiveIdentity() == ui::resources::resourceAuthoringPaneIdentity(detached.data()),
        "focus on an already-selected floating resource file remains the global active file after viewport focus");
    const qint64 activeBefore = QCborValue::fromCbor(window.captureRecoveryEditorStateForTest()).toMap()
        .value(QStringLiteral("resourceEditors")).toMap().value(QStringLiteral("active")).toInteger();
    const std::filesystem::path root(temporary.path().toStdWString());
    const auto firstPath = root / "group_a.spr";
    const auto secondPath = root / "group_b.spr";
    BoundedModalSequence modal({ fileDialogStep(firstPath), fileDialogStep(secondPath) });
    modal.start();
    saveAll->trigger();
    modal.stop();
    const qint64 activeAfter = QCborValue::fromCbor(window.captureRecoveryEditorStateForTest()).toMap()
        .value(QStringLiteral("resourceEditors")).toMap().value(QStringLiteral("active")).toInteger();
    check(modal.completedSteps() == 2 && !modal.timedOut()
            && std::filesystem::is_regular_file(firstPath) && std::filesystem::is_regular_file(secondPath)
            && !ui::resources::resourceAuthoringPaneIsModified(first)
            && detached && !ui::resources::resourceAuthoringPaneIsModified(detached.data())
            && detachedTabs->currentWidget() == detached.data() && activeBefore == activeAfter
            && ui::resources::captureResourceAuthoringPaneState(unrelated) == unrelatedBefore,
        "Save All Sprite includes its detached member, preserves global active selection, and leaves UNI unchanged");
}

} // namespace

void testResourceEditorToolsOnlyAndNativeGroupTabIcons()
{
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.resize(1440, 960);
    QMainWindow* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
    QMenu* const windowsMenu = window.findChild<QMenu*>(QStringLiteral("view.windows"));
    const std::array<const char*, 4> dockNames{
        "resourceEditorsDock", "soundClassResourceEditorsDock", "uniResourceEditorsDock", "cogScriptEditorDock" };
    const std::array<const char*, 4> actionNames{
        "tools.resource_editors.sprite_spr.new", "tools.resource_editors.sound_class_snd.new",
        "tools.resource_editors.string_table_uni.new", "tools.cog_script_editor" };
    std::array<QDockWidget*, 4> docks{};
    bool complete = workspace && windowsMenu;
    for ( std::size_t index = 0; index < docks.size(); ++index )
    {
        docks[index] = window.findChild<QDockWidget*>(QString::fromLatin1(dockNames[index]));
        QAction* const tool = window.findChild<QAction*>(QString::fromLatin1(actionNames[index]));
        complete = complete && docks[index] && tool;
        check(docks[index] && tool && windowsMenu
                && !windowsMenu->actions().contains(docks[index]->toggleViewAction()),
            "all four resource editors are reachable through Tools and absent from View Windows");
        check(docks[index] && tool && !tool->icon().isNull() && !docks[index]->windowIcon().isNull()
                && docks[index]->windowIcon().pixmap(20, 20).toImage() == tool->icon().pixmap(20, 20).toImage(),
            "each resource editor dock and floating window uses the same theme icon as its Tools action");
    }
    if ( !complete )
    {
        return;
    }
    workspace->setAnimated(false);
    workspace->tabifyDockWidget(docks[0], docks[3]);
    for ( QDockWidget* const dock : docks )
    {
        dock->show();
    }
    window.show();
    docks[0]->raise();
    const auto groupTabs = [&]() -> QTabBar*
    {
        for ( QTabBar* const bar : workspace->findChildren<QTabBar*>() )
        {
            if ( bar->count() != static_cast<int>(docks.size()) )
            {
                continue;
            }
            bool matching = true;
            for ( QDockWidget* const dock : docks )
            {
                bool titleFound = false;
                for ( int index = 0; index < bar->count(); ++index )
                {
                    QString title = bar->tabText(index);
                    title.remove(QLatin1Char('&'));
                    titleFound = titleFound || title == dock->windowTitle();
                }
                matching = matching && titleFound;
            }
            if ( matching )
            {
                return bar;
            }
        }
        return nullptr;
    };
    QElapsedTimer deadline{};
    deadline.start();
    while ( (!groupTabs() || !groupTabs()->isVisible()) && deadline.elapsed() < 2000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    QTabBar* const tabs = groupTabs();
    check(tabs && tabs->isVisible(), "four resource editors can share the actual native grouped dock tab bar");
    if ( !tabs )
    {
        return;
    }
    for ( QDockWidget* const dock : docks )
    {
        bool matchingIcon = false;
        for ( int index = 0; index < tabs->count(); ++index )
        {
            QString title = tabs->tabText(index);
            title.remove(QLatin1Char('&'));
            if ( title == dock->windowTitle() )
            {
                matchingIcon = !tabs->tabIcon(index).isNull()
                    && tabs->tabIcon(index).pixmap(20, 20).toImage() == dock->windowIcon().pixmap(20, 20).toImage();
                break;
            }
        }
        check(matchingIcon, "every grouped native resource editor tab paints its matching dock title icon");
    }
}

void testExternalResourceSaveInvalidatesNativeDraftBaseline()
{
    QTemporaryDir temporary{};
    const std::filesystem::path directory = temporaryRoot(temporary);
    MainWindow window(nullptr, true);
    window.setAnimated(false);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    const std::filesystem::path nativePath = directory / "draft_workspace.sed";
    const std::filesystem::path resourcePath = directory / "renamed_source.spr";
    window.setCurrentPathForTitleForTest(nativePath);
    window.resize(1200, 800);
    window.show();
    window.raise();
    window.activateWindow();
    QAction* const group = window.findChild<QAction*>(QStringLiteral("tools.resource_editors.sprite_spr.new"));
    createResourceDraftFromGroup(group);
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("resourceEditorTabs"));
    QWidget* const pane = tabs ? tabs->currentWidget() : nullptr;
    QLineEdit* const material = pane ? pane->findChild<QLineEdit*>(QStringLiteral("spr.material")) : nullptr;
    GlView* const viewport = window.findChild<GlView*>();
    QAction* const nativeSave = window.findChild<QAction*>(QStringLiteral("file.save"));
    check(temporary.isValid() && pane && material && viewport && nativeSave,
        "native resource baseline: real Sprite draft, viewport and native Save action exist");
    if ( !temporary.isValid() || !pane || !material || !viewport || !nativeSave )
    {
        return;
    }
    material->setText(QStringLiteral("native_baseline.mat"));
    const QCborMap draft = ui::resources::captureResourceAuthoringPaneState(pane);
    check(ui::resources::resourceAuthoringPaneIsModified(pane)
            && draft.value(QStringLiteral("sourceKind")).toInteger(-1) == 0,
        "native resource baseline: an authored dirty virtual draft is ready before native Save");
    QString nativeModalText{};
    int nativeModalCount = 0;
    const auto saveNative = [&]()
    {
        native_caption_test::ScopedTestFocusRestore restoreFocus{};
        if ( !native_caption_test::focusTestWidget(viewport, viewport, 2000) )
        {
            return false;
        }
        ModalStep unexpected{};
        unexpected.matches = [](QDialog* dialog)
        {
            return qobject_cast<QMessageBox*>(dialog) || qobject_cast<QFileDialog*>(dialog);
        };
        unexpected.interact = [&](QDialog* dialog)
        {
            ++nativeModalCount;
            if ( QMessageBox* const message = qobject_cast<QMessageBox*>(dialog) )
            {
                nativeModalText = message->text() + QLatin1Char('\n') + message->informativeText()
                    + QLatin1Char('\n') + message->detailedText();
            }
            else
            {
                nativeModalText = dialog->windowTitle();
            }
            dialog->reject();
        };
        BoundedModalSequence modal({ std::move(unexpected) });
        modal.start();
        nativeSave->trigger();
        modal.stop();
        return modal.completedSteps() == 0 && !modal.timedOut();
    };
    const bool initialSaved = saveNative();
    const std::optional<std::string> nativeBefore = readFile(nativePath);
    const bool initialBaseline = initialSaved && nativeBefore && !nativeBefore->empty()
        && !window.hasUnsavedChangesForTest() && !window.hasRecoveryOfferEligibleChangesForTest()
        && ui::resources::resourceAuthoringPaneIsModified(pane);
    if ( !initialBaseline )
    {
        const QCborMap currentPane = ui::resources::captureResourceAuthoringPaneState(pane);
        const QCborMap history = currentPane.value(QStringLiteral("history")).toMap();
        const QByteArray recovery = window.captureRecoveryEditorStateForTest();
        QString warning{};
        const bool validRecovery = window.validateRecoveryEditorStateForTest(recovery, &warning);
        std::cerr << "[native baseline setup] saved=" << initialSaved
            << " fileExists=" << nativeBefore.has_value()
            << " fileSize=" << (nativeBefore ? nativeBefore->size() : 0)
            << " nativeDirty=" << window.hasUnsavedChangesForTest()
            << " recoveryDirty=" << window.hasRecoveryOfferEligibleChangesForTest()
            << " resourceDirty=" << ui::resources::resourceAuthoringPaneIsModified(pane)
            << " saveEnabled=" << nativeSave->isEnabled()
            << " viewportFocus=" << viewport->hasFocus()
            << " modals=" << nativeModalCount
            << " modal=" << nativeModalText.toStdString()
            << " status=" << window.statusBar()->currentMessage().toStdString()
            << " recoveryValid=" << validRecovery
            << " warning=" << warning.toStdString()
            << " cursor=" << history.value(QStringLiteral("cursor")).toInteger(-1)
            << " clean=" << history.value(QStringLiteral("clean")).toInteger(-2)
            << " baselineDirty=" << history.value(QStringLiteral("baselineDirty")).toBool()
            << " commands=" << history.value(QStringLiteral("commands")).toArray().size()
            << " paneCbor=" << QCborValue(currentPane).toCbor().toHex().toStdString() << '\n';
    }
    check(initialBaseline,
        "native resource baseline: actual SED Save records the dirty external draft without clearing its resource marker");
    if ( !initialSaved || !nativeBefore )
    {
        return;
    }
    BoundedModalSequence external({ fileDialogStep(resourcePath) });
    external.start();
    const bool externalSaved = ui::resources::resourceAuthoringPaneSave(pane, false);
    external.stop();
    const QCborMap loose = ui::resources::captureResourceAuthoringPaneState(pane);
    std::error_code canonicalError{};
    const std::filesystem::path canonicalResourcePath = std::filesystem::weakly_canonical(resourcePath, canonicalError);
    check(!canonicalError && !canonicalResourcePath.empty(),
        "native resource baseline: independent expected loose destination canonicalizes successfully");
    const QString expectedLoosePath = QString::fromStdWString(canonicalResourcePath.wstring());
    const bool exactExternalSave = externalSaved && external.completedSteps() == 1 && !external.timedOut()
        && !ui::resources::resourceAuthoringPaneIsModified(pane)
        && loose.value(QStringLiteral("sourceKind")).toInteger(-1) == 1
        && !canonicalError && ui::resources::resourceAuthoringPaneLooseFilePath(pane) == expectedLoosePath
        && loose.value(QStringLiteral("sprite")) == draft.value(QStringLiteral("sprite"))
        && std::filesystem::is_regular_file(resourcePath) && readFile(nativePath) == nativeBefore;
    if ( !exactExternalSave )
    {
        std::cerr << "[native baseline external Save] saved=" << externalSaved
            << " chooserSteps=" << external.completedSteps()
            << " timeout=" << external.timedOut()
            << " resourceDirty=" << ui::resources::resourceAuthoringPaneIsModified(pane)
            << " sourceKind=" << loose.value(QStringLiteral("sourceKind")).toInteger(-1)
            << " actualPath=" << ui::resources::resourceAuthoringPaneLooseFilePath(pane).toStdString()
            << " expectedPath=" << expectedLoosePath.toStdString()
            << " spriteSame=" << (loose.value(QStringLiteral("sprite")) == draft.value(QStringLiteral("sprite")))
            << " resourceExists=" << std::filesystem::is_regular_file(resourcePath)
            << " nativeBytesSame=" << (readFile(nativePath) == nativeBefore) << '\n';
    }
    check(exactExternalSave,
        "native resource baseline: external Save commits the exact authored SPR to its loose source without rewriting SED");
    check(window.hasUnsavedChangesForTest() && window.hasRecoveryOfferEligibleChangesForTest(),
        "native resource baseline: clean external Save source reassociation invalidates the saved dirty-draft generation and requires a new native Save");
    const bool finalSaved = saveNative();
    const bool finalNativeDirty = window.hasUnsavedChangesForTest();
    const bool finalRecoveryDirty = window.hasRecoveryOfferEligibleChangesForTest();
    const bool finalResourceDirty = ui::resources::resourceAuthoringPaneIsModified(pane);
    if ( !finalSaved || finalNativeDirty || finalRecoveryDirty || finalResourceDirty )
    {
        const QCborMap finalPane = ui::resources::captureResourceAuthoringPaneState(pane);
        const QByteArray finalRecovery = window.captureRecoveryEditorStateForTest();
        const QCborMap finalRecoveryMap = QCborValue::fromCbor(finalRecovery).toMap();
        const QCborMap recoveredResources = finalRecoveryMap.value(QStringLiteral("resourceEditors")).toMap();
        QString recoveryWarning{};
        const bool finalRecoveryValid = window.validateRecoveryEditorStateForTest(finalRecovery, &recoveryWarning);
        const auto widgetName = [](QWidget* const widget)
        {
            return widget
                ? QString::fromLatin1(widget->metaObject()->className()) + QLatin1Char('#') + widget->objectName()
                : QStringLiteral("<none>");
        };
        QWidget* const activeWindow = QApplication::activeWindow();
        QWidget* const focusWidget = QApplication::focusWidget();
        const QByteArray paneBaselineHash = pane->property("resource_authoring.nativeBaselineHash").toByteArray();
        std::cerr << "[native baseline final Save] saved=" << finalSaved
            << " viewportFocus=" << viewport->hasFocus()
            << " activeWindow=" << widgetName(activeWindow).toStdString()
            << " windowActive=" << window.isActiveWindow()
            << " focusWidget=" << widgetName(focusWidget).toStdString()
            << " nativeDirty=" << finalNativeDirty
            << " recoveryDirty=" << finalRecoveryDirty
            << " resourceDirty=" << finalResourceDirty
            << " saveEnabled=" << nativeSave->isEnabled()
            << " modalCount=" << nativeModalCount
            << " modalText=" << nativeModalText.toStdString()
            << " status=" << window.statusBar()->currentMessage().toStdString()
            << " recoveryValid=" << finalRecoveryValid
            << " recoveryWarning=" << recoveryWarning.toStdString()
            << " nativeBaselineHash=" << paneBaselineHash.toHex().toStdString()
            << " paneCbor=" << QCborValue(finalPane).toCbor().toHex().toStdString()
            << " recoveryResourceEditorsCbor=" << QCborValue(recoveredResources).toCbor().toHex().toStdString()
            << '\n';
    }
    check(finalSaved && !window.hasUnsavedChangesForTest() && !window.hasRecoveryOfferEligibleChangesForTest()
            && !ui::resources::resourceAuthoringPaneIsModified(pane),
        "native resource baseline: the next actual SED Save records the new clean loose identity and establishes a new baseline");
}

void testResourceDocumentHistoryToolbars()
{
    MainWindow window(nullptr, true);
    window.resize(1280, 900);
    window.show();
    window.activateWindow();
    QAction* const configuredUndo = window.findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* const configuredRedo = window.findChild<QAction*>(QStringLiteral("edit.redo"));
    check(configuredUndo && configuredRedo, "resource document tools fixture finds configured history actions");
    if ( !configuredUndo || !configuredRedo )
    {
        return;
    }
    configuredUndo->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+U")));
    configuredRedo->setShortcut(QKeySequence(QStringLiteral("Ctrl+Alt+R")));
    const std::array<const char*, 3> actions{
        "tools.resource_editors.sprite_spr.new", "tools.resource_editors.sound_class_snd.new",
        "tools.resource_editors.string_table_uni.new" };
    const std::array<const char*, 3> tabNames{
        "resourceEditorTabs", "soundClassResourceEditorTabs", "uniResourceEditorTabs" };
    for ( std::size_t index = 0; index < actions.size(); ++index )
    {
        QAction* const action = window.findChild<QAction*>(QString::fromLatin1(actions[index]));
        check(action, "resource document tools fixture finds each editor group");
        if ( !action )
        {
            continue;
        }
        action->trigger();
        createResourceDraftFromGroup(action);
        QTabWidget* const tabs = window.findChild<QTabWidget*>(QString::fromLatin1(tabNames[index]));
        QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
        QWidget* const tools = page ? page->findChild<QWidget*>(QStringLiteral("resource_authoring.document_tools")) : nullptr;
        QToolButton* const undo = tools ? tools->findChild<QToolButton*>(QStringLiteral("resource_authoring.undo")) : nullptr;
        QToolButton* const redo = tools ? tools->findChild<QToolButton*>(QStringLiteral("resource_authoring.redo")) : nullptr;
        check(page && tools && page->isAncestorOf(tools) && undo && redo
                && !undo->icon().isNull() && !redo->icon().isNull()
                && undo->iconSize() == QSize(20, 20) && redo->iconSize() == QSize(20, 20)
                && undo->focusPolicy() == Qt::NoFocus && redo->focusPolicy() == Qt::NoFocus
                && !undo->isEnabled() && !redo->isEnabled(),
            "resource document tools: each clean SPR SND UNI page owns truthful disabled Undo Redo icons separate from group file actions");
        check(undo && redo && undo->toolTip().contains(QStringLiteral("Ctrl Alt U"))
                && redo->toolTip().contains(QStringLiteral("Ctrl Alt R")),
            "resource document tools: tooltips show the actual configured history bindings");
        if ( !page || !tools || !undo || !redo || index != 0 )
        {
            continue;
        }
        ui::ScrubFloatEdit* const width = page->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"));
        QLineEdit* const line = width ? width->lineEdit() : nullptr;
        check(line, "resource document tools: Sprite width has a real native text editor");
        if ( !line )
        {
            continue;
        }
        const QCborMap baseline = ui::resources::captureResourceAuthoringPaneState(page);
        line->setFocus(Qt::OtherFocusReason);
        QElapsedTimer focusDeadline;
        focusDeadline.start();
        while ( !line->hasFocus() && focusDeadline.elapsed() < 2000 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
        }
        check(line->hasFocus(), "resource document tools: numeric field retains actual focus before document Undo");
        line->selectAll();
        const QString raw = QStringLiteral("2.625");
        for ( const QChar character : raw )
        {
            QKeyEvent press(QEvent::KeyPress, character == QLatin1Char('.') ? Qt::Key_Period : Qt::Key_0 + character.digitValue(),
                Qt::NoModifier, QString(character));
            QApplication::sendEvent(line, &press);
        }
        check(undo->isEnabled() && !redo->isEnabled(),
            "resource document tools: pending raw Sprite typing makes document Undo available");
        undo->click();
        const QCborMap undone = ui::resources::captureResourceAuthoringPaneState(page);
        check(line->hasFocus() && undone.value(QStringLiteral("sprite")) == baseline.value(QStringLiteral("sprite"))
                && !undone.contains(QStringLiteral("spritePendingNumericEdits")) && !undo->isEnabled() && redo->isEnabled(),
            "resource document tools: icon Undo uses document history while a numeric line retains focus and restores exact accepted fields");
        redo->click();
        const QCborMap redone = ui::resources::captureResourceAuthoringPaneState(page);
        check(redone.value(QStringLiteral("spritePendingNumericEdits")).toMap().value(QStringLiteral("width")).toString() == raw
                && undo->isEnabled() && !redo->isEnabled(),
            "resource document tools: icon Redo restores the exact unfinished raw edit without saving or opening a dialog");
    }
}

class WindowDropCallbackFixtureDock final : public QDockWidget
{
public:
    using QDockWidget::QDockWidget;

    int noButtonMoveCount() const noexcept
    {
        return noButtonMoveCount_;
    }

    QPoint noButtonMoveGlobalPosition() const noexcept
    {
        return noButtonMoveGlobalPosition_;
    }

protected:
    bool event(QEvent* const event) override
    {
        if ( (event->type() == QEvent::MouseMove || event->type() == QEvent::NonClientAreaMouseMove)
            && static_cast<QMouseEvent*>(event)->buttons() == Qt::NoButton )
        {
            ++noButtonMoveCount_;
            noButtonMoveGlobalPosition_ = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
            return true;
        }
        // This focused callback test supplies the completed public layout
        // state itself. Real native gesture/merge coverage follows separately.
        if ( event->type() == QEvent::NonClientAreaMouseButtonPress
            || event->type() == QEvent::NonClientAreaMouseMove )
        {
            return true;
        }
        return QDockWidget::event(event);
    }

private:
    int noButtonMoveCount_{};
    QPoint noButtonMoveGlobalPosition_{};
};

void testDockWindowDropCallbackGuards()
{
    enum class Boundary
    {
        completed,
        movedAfterRelease,
        movedAfterNativeRelease,
        noGesture,
        rejectedFloatingThenProgrammaticDock,
        canceled,
        changedContext,
        deletedSource,
        deletedTarget,
        deletedOwner,
        ownerDeletedByProvider,
        ownerDeletedByHandler
    };
    const std::array<Boundary, 12> boundaries{
        Boundary::completed, Boundary::movedAfterRelease, Boundary::movedAfterNativeRelease,
        Boundary::noGesture, Boundary::canceled,
        Boundary::rejectedFloatingThenProgrammaticDock,
        Boundary::changedContext, Boundary::deletedSource, Boundary::deletedTarget,
        Boundary::deletedOwner, Boundary::ownerDeletedByProvider,
        Boundary::ownerDeletedByHandler };
    for ( const Boundary boundary : boundaries )
    {
        QPointer<QMainWindow> owner(new QMainWindow);
        owner->setAnimated(false);
        owner->resize(700, 500);
        QPointer<WindowDropCallbackFixtureDock> source(
            new WindowDropCallbackFixtureDock(QStringLiteral("Source"), owner.data()));
        if ( boundary == Boundary::movedAfterRelease )
        {
            // QApplication discards button-free client moves before widget
            // filters unless tracking is enabled. Witness actual delivery.
            source->setMouseTracking(true);
        }
        QPointer<QDockWidget> target(new QDockWidget(QStringLiteral("Target"), owner.data()));
        source->setWidget(new QLabel(QStringLiteral("Source body"), source.data()));
        target->setWidget(new QLabel(QStringLiteral("Target body"), target.data()));
        owner->addDockWidget(Qt::RightDockWidgetArea, target.data());
        owner->addDockWidget(Qt::RightDockWidgetArea, source.data());
        source->setFloating(true);
        owner->show();
        target->show();
        source->show();
        QApplication::processEvents();
        std::uint64_t generation = 7;
        int callbacks = 0;
        bool exactArguments = false;
        new ui::DockWindowDropFilter(source.data(),
            [&generation]()
            {
                return generation;
            },
            [owner, target, boundary]()
            {
                if ( boundary == Boundary::ownerDeletedByProvider && owner )
                {
                    delete owner.data();
                }
                return std::vector<QPointer<QDockWidget>>{ target };
            },
            [owner, source, target, boundary, &callbacks, &exactArguments](
                QDockWidget* const dropped, QDockWidget* const peer, const std::uint64_t context)
            {
                ++callbacks;
                exactArguments = dropped == source.data() && peer == target.data() && context == 7;
                if ( boundary == Boundary::ownerDeletedByHandler && owner )
                {
                    delete owner.data();
                }
            });
        if ( boundary != Boundary::noGesture )
        {
            const QPoint start = source->mapToGlobal(QPoint(10, -10));
            QMouseEvent press(QEvent::NonClientAreaMouseButtonPress, QPointF(10, -10), QPointF(start),
                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(source.data(), &press);
        }
        if ( source && owner && target )
        {
            if ( boundary == Boundary::canceled )
            {
                QKeyEvent cancel(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QApplication::sendEvent(source.data(), &cancel);
            }
            if ( boundary == Boundary::changedContext )
            {
                ++generation;
            }
            if ( boundary == Boundary::rejectedFloatingThenProgrammaticDock )
            {
                const QPoint rejectedEnd = target->mapToGlobal(target->rect().center());
                QMouseEvent rejectedRelease(QEvent::NonClientAreaMouseMove,
                    QPointF(source->mapFromGlobal(rejectedEnd)), QPointF(rejectedEnd),
                    Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(source.data(), &rejectedRelease);
                QApplication::processEvents();
                check(source && source->isFloating() && callbacks == 0,
                    "a rejected native caption gesture remains floating and publishes no completed drop");
            }
            source->setFloating(false);
            owner->tabifyDockWidget(target.data(), source.data());
            const QPoint end = target->mapToGlobal(target->rect().center());
            QMouseEvent release(QEvent::NonClientAreaMouseMove, QPointF(source->mapFromGlobal(end)), QPointF(end),
                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(source.data(), &release);
            if ( boundary == Boundary::movedAfterRelease || boundary == Boundary::movedAfterNativeRelease )
            {
                const QPoint cursorAfterRelease = target->mapToGlobal(QPoint(-20, -20));
                const QRect targetBounds(target->mapToGlobal(QPoint{}), target->size());
                check(targetBounds.contains(end) && !targetBounds.contains(cursorAfterRelease),
                    "native window drop callback records a release inside the peer before the cursor moves outside");
                const bool nativeMove = boundary == Boundary::movedAfterNativeRelease;
                check(callbacks == 0 && (nativeMove || source->hasMouseTracking()),
                    "native window drop completion remains queued before a delivered post-release cursor move");
                const int movesBefore = source->noButtonMoveCount();
                QMouseEvent laterMove(nativeMove ? QEvent::NonClientAreaMouseMove : QEvent::MouseMove,
                    QPointF(source->mapFromGlobal(cursorAfterRelease)), QPointF(cursorAfterRelease),
                    Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(source.data(), &laterMove);
                check(source && source->noButtonMoveCount() == movesBefore + 1
                        && source->noButtonMoveGlobalPosition() == cursorAfterRelease,
                    "the button-free post-release cursor move reaches the source widget and its event filters");
            }
            if ( boundary == Boundary::deletedSource )
            {
                owner->removeDockWidget(source.data());
                delete source.data();
            }
            if ( boundary == Boundary::deletedTarget )
            {
                owner->removeDockWidget(target.data());
                delete target.data();
            }
            if ( boundary == Boundary::deletedOwner )
            {
                delete owner.data();
            }
        }
        QApplication::processEvents();
        const bool expectsCallback = boundary == Boundary::completed
            || boundary == Boundary::movedAfterRelease
            || boundary == Boundary::movedAfterNativeRelease
            || boundary == Boundary::ownerDeletedByHandler;
        if ( callbacks != (expectsCallback ? 1 : 0) || (expectsCallback && !exactArguments) )
        {
            std::cerr << "native drop callback boundary=" << static_cast<int>(boundary)
                << " callbacks=" << callbacks << " exactArguments=" << exactArguments << '\n';
        }
        check(callbacks == (expectsCallback ? 1 : 0) && (!expectsCallback || exactArguments),
            "native window drop callback requires a completed same-owner gesture and survives cancellation, stale context, and synchronous owner/source/target deletion");
        if ( boundary == Boundary::ownerDeletedByProvider || boundary == Boundary::ownerDeletedByHandler )
        {
            check(!owner && !source && !target,
                "native window drop callback permits terminal owner deletion without retaining stale widgets");
        }
        if ( owner )
        {
            delete owner.data();
        }
    }
}

bool dragNativeResourceWindowToCenter(
    QDockWidget* const source,
    QDockWidget* const target,
    const std::function<bool()>& completed)
{
    if ( !source || !target || !source->isFloating() || source->titleBarWidget() )
    {
        return false;
    }
    QPointer<QDockWidget> guardedSource(source);
    QPointer<QDockWidget> guardedTarget(target);
    const auto decorated = [guardedSource]()
    {
        return guardedSource && guardedSource->isFloating() && !guardedSource->titleBarWidget()
            && !guardedSource->windowFlags().testFlag(Qt::FramelessWindowHint)
            && guardedSource->geometry().top() > guardedSource->frameGeometry().top();
    };
    QElapsedTimer deadline{};
    deadline.start();
    while ( !decorated() && deadline.elapsed() < 2000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    if ( !decorated() || !guardedTarget || !guardedTarget->isVisible() )
    {
        if ( guardedSource )
        {
            const QRect frame = guardedSource->frameGeometry();
            const QRect client = guardedSource->geometry();
            QWindow* const native = guardedSource->windowHandle();
            const QMargins margins = native ? native->frameMargins() : QMargins{};
            std::cerr << "resource window center caption: object=" << guardedSource->objectName().toStdString()
                << " floating=" << guardedSource->isFloating()
                << " customTitle=" << (guardedSource->titleBarWidget() != nullptr)
                << " flags=" << static_cast<unsigned int>(guardedSource->windowFlags().toInt())
                << " nativeId=" << guardedSource->internalWinId()
                << " platformFlags=" << (native ? static_cast<unsigned int>(native->flags().toInt()) : 0u)
                << " platformFrameMargins=" << margins.left() << ',' << margins.top()
                << ',' << margins.right() << ',' << margins.bottom()
                << " nativeParent=" << (native && native->parent() != nullptr)
                << " frame=" << frame.x() << ',' << frame.y() << ',' << frame.width() << ',' << frame.height()
                << " client=" << client.x() << ',' << client.y() << ',' << client.width() << ',' << client.height()
                << " visible=" << guardedSource->isVisible()
                << " targetVisible=" << (guardedTarget && guardedTarget->isVisible()) << '\n';
#ifdef Q_OS_WIN
            // Inspect only the existing HWND. Creating a native handle here
            // would change the lifecycle and conceal the frame failure.
            const HWND handle = reinterpret_cast<HWND>(guardedSource->internalWinId());
            if ( handle && IsWindow(handle) )
            {
                const LONG_PTR style = GetWindowLongPtrW(handle, GWL_STYLE);
                const LONG_PTR extendedStyle = GetWindowLongPtrW(handle, GWL_EXSTYLE);
                RECT nativeFrame{};
                RECT nativeClient{};
                POINT clientOrigin{};
                const bool frameRead = GetWindowRect(handle, &nativeFrame) != FALSE;
                const bool clientRead = GetClientRect(handle, &nativeClient) != FALSE;
                const bool originRead = ClientToScreen(handle, &clientOrigin) != FALSE;
                RECT expectedFrame = nativeClient;
                const bool expectedRead = AdjustWindowRectEx(&expectedFrame,
                    static_cast<DWORD>(style), GetMenu(handle) != nullptr,
                    static_cast<DWORD>(extendedStyle)) != FALSE;
                std::cerr << "resource window native HWND: style=" << static_cast<unsigned long long>(style)
                    << " exStyle=" << static_cast<unsigned long long>(extendedStyle)
                    << " caption=" << ((style & WS_CAPTION) == WS_CAPTION)
                    << " thickFrame=" << ((style & WS_THICKFRAME) != 0)
                    << " child=" << ((style & WS_CHILD) != 0)
                    << " popup=" << ((style & WS_POPUP) != 0)
                    << " nativeVisible=" << (IsWindowVisible(handle) != FALSE)
                    << " maximized=" << (IsZoomed(handle) != FALSE)
                    << " frameRead=" << frameRead << " clientRead=" << clientRead << " originRead=" << originRead
                    << " frame=" << nativeFrame.left << ',' << nativeFrame.top
                    << ',' << nativeFrame.right - nativeFrame.left << ',' << nativeFrame.bottom - nativeFrame.top
                    << " client=" << clientOrigin.x << ',' << clientOrigin.y
                    << ',' << nativeClient.right - nativeClient.left << ',' << nativeClient.bottom - nativeClient.top
                    << " actualFullMargins=" << clientOrigin.x - nativeFrame.left << ',' << clientOrigin.y - nativeFrame.top
                    << ',' << nativeFrame.right - clientOrigin.x - nativeClient.right
                    << ',' << nativeFrame.bottom - clientOrigin.y - nativeClient.bottom
                    << " expectedRead=" << expectedRead
                    << " expectedFullMargins=" << -expectedFrame.left << ',' << -expectedFrame.top
                    << ',' << expectedFrame.right - nativeClient.right << ',' << expectedFrame.bottom - nativeClient.bottom
                    << " parent=" << reinterpret_cast<std::uintptr_t>(GetAncestor(handle, GA_PARENT))
                    << " owner=" << reinterpret_cast<std::uintptr_t>(GetWindow(handle, GW_OWNER)) << '\n';
            }
#endif
        }
        return false;
    }
    const QRect frame = guardedSource->frameGeometry();
    const QRect client = guardedSource->geometry();
    const QRect caption(client.left(), frame.top(), client.width(), client.top() - frame.top());
    const QPoint startGlobal = caption.center();
    const QPoint start = guardedSource->mapFromGlobal(startGlobal);
    const QPoint frameGrabOffset = startGlobal - guardedSource->pos();
    const QRect targetBefore(guardedTarget->mapToGlobal(QPoint{}), guardedTarget->size());
    QPoint destination = targetBefore.center();
    QScreen* const screen = guardedSource->screen();
    if ( !screen || !screen->availableGeometry().contains(startGlobal)
        || !screen->availableGeometry().contains(destination) )
    {
        return false;
    }
    const QPoint cursorBefore = QCursor::pos();
    QCursor::setPos(startGlobal);
    QMouseEvent press(QEvent::NonClientAreaMouseButtonPress, QPointF(start), QPointF(startGlobal),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(guardedSource.data(), &press);
    QRect targetAfter{};
    bool nativeGapVisible = false;
    QRect nativeGapGeometry{};
    bool terminalDispatched = false;
    QMainWindow* const workspace = qobject_cast<QMainWindow*>(target->parentWidget());
    if ( guardedSource && guardedTarget )
    {
        // The native press may unplug a dock's saved position and resize its
        // former group. Aim at the actual visible target after that mutation.
        targetAfter = QRect(guardedTarget->mapToGlobal(QPoint{}), guardedTarget->size());
        destination = targetAfter.center();
        QCursor::setPos(destination);
        guardedSource->move(destination - frameGrabOffset);
        QWidget* const gap = workspace ? workspace->findChild<QWidget*>(QStringLiteral("qt_rubberband")) : nullptr;
        nativeGapVisible = gap && gap->isVisible();
        nativeGapGeometry = gap ? QRect(gap->mapToGlobal(QPoint{}), gap->size()) : QRect{};
    }
    if ( guardedSource )
    {
        // Qt's Windows non-client route ends its native caption drag when a
        // non-client move reports no buttons. The actual docking hover was
        // supplied by the preceding public QWidget move, not a layout API.
        QMouseEvent release(QEvent::NonClientAreaMouseMove, QPointF(start), QPointF(destination),
            Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(guardedSource.data(), &release);
        terminalDispatched = true;
    }
    deadline.restart();
    while ( !completed() && deadline.elapsed() < 2000 )
    {
        QApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    QCursor::setPos(cursorBefore);
    const bool complete = completed();
    if ( !complete )
    {
        const bool nativeTabified = workspace && guardedSource && guardedTarget
            && workspace->tabifiedDockWidgets(guardedTarget.data()).contains(guardedSource.data());
        std::cerr << "resource window center native route: dispatched=" << terminalDispatched
            << " pressAccepted=" << press.isAccepted()
            << " nativeTabified=" << nativeTabified
            << " sourceFloating=" << (guardedSource && guardedSource->isFloating())
            << " nativeGapVisible=" << nativeGapVisible
            << " nativeGap=" << nativeGapGeometry.x() << ',' << nativeGapGeometry.y()
            << ',' << nativeGapGeometry.width() << ',' << nativeGapGeometry.height()
            << " targetBefore=" << targetBefore.x() << ',' << targetBefore.y()
            << ',' << targetBefore.width() << ',' << targetBefore.height()
            << " targetAfter=" << targetAfter.x() << ',' << targetAfter.y()
            << ',' << targetAfter.width() << ',' << targetAfter.height()
            << " destination=" << destination.x() << ',' << destination.y()
            << " targetVisibleRegion=" << (guardedTarget && !guardedTarget->visibleRegion().isEmpty())
            << " mouseGrab=" << (guardedSource && QWidget::mouseGrabber() == guardedSource.data()) << '\n';
    }
    return complete;
}

void testResourceNativeWindowDropsMergeSameKindDocuments()
{
    struct KindFixture
    {
        QString action{};
        QString tabs{};
        QString otherAction{};
        QString otherTabs{};
        ui::resources::ResourceAuthoringKind kind{};
    };
    const std::array<KindFixture, 3> fixtures{
        KindFixture{ QStringLiteral("tools.resource_editors.sprite_spr.new"), QStringLiteral("resourceEditorTabs"),
            QStringLiteral("tools.resource_editors.string_table_uni.new"), QStringLiteral("uniResourceEditorTabs"),
            ui::resources::ResourceAuthoringKind::sprite },
        KindFixture{ QStringLiteral("tools.resource_editors.sound_class_snd.new"), QStringLiteral("soundClassResourceEditorTabs"),
            QStringLiteral("tools.resource_editors.string_table_uni.new"), QStringLiteral("uniResourceEditorTabs"),
            ui::resources::ResourceAuthoringKind::soundClass },
        KindFixture{ QStringLiteral("tools.resource_editors.string_table_uni.new"), QStringLiteral("uniResourceEditorTabs"),
            QStringLiteral("tools.resource_editors.sprite_spr.new"), QStringLiteral("resourceEditorTabs"),
            ui::resources::ResourceAuthoringKind::stringTable } };
    QScreen* const screen = QGuiApplication::primaryScreen();
    check(screen != nullptr, "resource window merge: an actual available screen exists");
    if ( !screen )
    {
        return;
    }
    const QRect available = screen->availableGeometry();
    for ( const KindFixture& fixture : fixtures )
    {
        MainWindow window(nullptr, true);
        window.setProjectTypeForTest(ProjectType::JKDF2);
        window.setAnimated(false);
        const QSize maximumSize(std::max(1, available.width() - 40), std::max(1, available.height() - 80));
        window.setMinimumSize(window.minimumSize().boundedTo(maximumSize));
        window.resize(QSize(2000, 960).boundedTo(maximumSize));
        window.move(available.topLeft() + QPoint(20, 20));
        window.show();
        window.activateWindow();
        QMainWindow* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow"));
        QAction* const create = window.findChild<QAction*>(fixture.action);
        check(workspace && create, "resource window merge: the requested real editor group exists");
        if ( !workspace || !create )
        {
            continue;
        }
        workspace->setAnimated(false);
        for ( int index = 0; index < 3; ++index )
        {
            createResourceDraftFromGroup(create);
        }
        QTabWidget* const primaryTabs = window.findChild<QTabWidget*>(fixture.tabs);
        QDockWidget* const primaryDock = dockForResourceTabHost(primaryTabs);
        check(primaryTabs && primaryTabs->count() == 3 && primaryDock,
            "resource window merge: explicit New creates three same-kind document tabs");
        if ( !primaryTabs || primaryTabs->count() != 3 || !primaryDock )
        {
            continue;
        }
        std::array<QPointer<QWidget>, 3> pages{
            primaryTabs->widget(0), primaryTabs->widget(1), primaryTabs->widget(2) };
        std::array<QString, 3> identities{};
        std::array<QCborMap, 3> authored{};
        bool authoredFixture = true;
        for ( std::size_t index = 0; index < pages.size(); ++index )
        {
            QWidget* const page = pages[index].data();
            const QString suffix = QString::number(index);
            if ( fixture.kind == ui::resources::ResourceAuthoringKind::sprite )
            {
                QLineEdit* const material = page->findChild<QLineEdit*>(QStringLiteral("spr.material"));
                check(material != nullptr, "resource window merge: each Sprite exposes its real material editor");
                if ( !material )
                {
                    authoredFixture = false;
                    break;
                }
                material->setText(QStringLiteral("window_merge_") + suffix + QStringLiteral(".mat"));
                material->setText(QStringLiteral("window_future_") + suffix + QStringLiteral(".mat"));
            }
            else if ( fixture.kind == ui::resources::ResourceAuthoringKind::soundClass )
            {
                restoreSoundRows(qobject_cast<QDialog*>(page), {
                    { QStringLiteral("moving"), QStringLiteral("base.wav"), QStringLiteral("0x00000040"),
                        QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("1") } });
                QTreeWidget* const tree = page->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
                const QList<QTreeWidgetItem*> rows = soundAlternativeItems(tree);
                check(rows.size() == 1, "resource window merge: each Sound Class exposes one real authored alternative");
                if ( rows.size() != 1 )
                {
                    authoredFixture = false;
                    break;
                }
                rows.front()->setText(1, QStringLiteral("window_merge_") + suffix + QStringLiteral(".wav"));
                rows.front()->setText(1, QStringLiteral("window_future_") + suffix + QStringLiteral(".wav"));
            }
            else
            {
                QAbstractButton* const add = button(page, QStringLiteral("uni.add"));
                QTableWidget* const table = page->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
                check(add && table, "resource window merge: each UNI exposes its real Add control and table");
                if ( !add || !table )
                {
                    authoredFixture = false;
                    break;
                }
                add->click();
                check(table->rowCount() == 1 && table->item(0, 0) && table->item(0, 1),
                    "resource window merge: each UNI creates a real editable string row");
                if ( table->rowCount() != 1 || !table->item(0, 0) || !table->item(0, 1) )
                {
                    authoredFixture = false;
                    break;
                }
                table->item(0, 0)->setText(QStringLiteral("WINDOW_MERGE_") + suffix);
                table->item(0, 1)->setText(QStringLiteral("retained text ") + suffix);
                table->item(0, 1)->setText(QStringLiteral("future text ") + suffix);
            }
            const bool undone = ui::resources::resourceAuthoringPaneDocumentUndoRedo(page, false);
            QApplication::processEvents();
            identities[index] = ui::resources::resourceAuthoringPaneIdentity(page);
            authored[index] = authoredDraftState(ui::resources::captureResourceAuthoringPaneState(page));
            const bool hasDirtyHistory = undone && !identities[index].isEmpty()
                && ui::resources::resourceAuthoringPaneIsModified(page)
                && ui::resources::resourceAuthoringPaneCanUndoRedo(page, true)
                && !authored[index].value(QStringLiteral("history")).toMap().isEmpty();
            check(hasDirtyHistory,
                "resource window merge: every fixture has an exact dirty draft with an independent retained Redo branch");
            authoredFixture = authoredFixture && hasDirtyHistory;
        }
        if ( !authoredFixture )
        {
            continue;
        }
        const auto unchanged = [&pages, &identities, &authored]()
        {
            for ( std::size_t index = 0; index < pages.size(); ++index )
            {
                QWidget* const page = pages[index].data();
                if ( !page || ui::resources::resourceAuthoringPaneIdentity(page) != identities[index]
                    || authoredDraftState(ui::resources::captureResourceAuthoringPaneState(page)) != authored[index]
                    || !ui::resources::resourceAuthoringPaneIsModified(page)
                    || !ui::resources::resourceAuthoringPaneCanUndoRedo(page, true) )
                {
                    return false;
                }
            }
            return true;
        };
        primaryDock->show();
        primaryDock->raise();
        primaryTabs->setCurrentWidget(pages[0].data());
        QApplication::processEvents();
        const std::optional<QPoint> outside = resourceTabDropPoint(workspace, primaryTabs->tabBar());
        check(outside.has_value(), "resource window merge: a real outside-host tab release point exists");
        if ( !outside )
        {
            continue;
        }
        const TabDragOutcome detached = sendTabDrag(primaryTabs->tabBar(),
            primaryTabs->tabBar()->tabRect(primaryTabs->indexOf(pages[0].data())).center(), *outside, false);
        QPointer<QTabWidget> sourceTabs(resourceTabHostContaining(workspace, pages[0].data()));
        QPointer<QDockWidget> sourceDock(dockForResourceTabHost(sourceTabs.data()));
        check(detached.terminalEventsDispatched && !detached.timedOut && sourceTabs
                && sourceTabs != primaryTabs && sourceDock && sourceDock->isFloating() && unchanged(),
            "resource window merge: a real file-tab gesture creates the native floating source window");
        if ( !sourceTabs || sourceTabs == primaryTabs || !sourceDock )
        {
            continue;
        }
        sourceDock->move(available.topLeft() + QPoint(60, 100));
        sourceDock->show();
        QApplication::processEvents();
        primaryTabs->setCurrentWidget(pages[1].data());
        const TabDragOutcome secondFile = sendTabDrag(primaryTabs->tabBar(),
            primaryTabs->tabBar()->tabRect(primaryTabs->indexOf(pages[1].data())).center(),
            sourceTabs->tabBar()->mapToGlobal(sourceTabs->tabBar()->tabRect(0).center()), false);
        check(secondFile.terminalEventsDispatched && !secondFile.timedOut && sourceTabs->count() == 2
                && sourceTabs->widget(0) == pages[1].data() && sourceTabs->widget(1) == pages[0].data()
                && primaryTabs->count() == 1 && primaryTabs->widget(0) == pages[2].data() && unchanged(),
            "resource window merge: the same-kind document bar accepts a second unchanged file before the window drop");
        if ( sourceTabs->count() != 2 || primaryTabs->count() != 1 )
        {
            continue;
        }
        sourceTabs->setCurrentWidget(pages[1].data());
        sourceDock->raise();
        sourceDock->activateWindow();
        QApplication::processEvents();
        QWidget* const groupTools = sourceDock->findChild<QWidget*>(QStringLiteral("resource_authoring.group_toolbar"));
        const auto visibleGroupButton = [groupTools](const char* const name)
        {
            QToolButton* const item = groupTools ? groupTools->findChild<QToolButton*>(QString::fromLatin1(name)) : nullptr;
            return item && item->isVisible() && !item->icon().isNull();
        };
        QWidget* const documentTools = pages[1]->findChild<QWidget*>(QStringLiteral("resource_authoring.document_tools"));
        check(groupTools && groupTools->isVisible() && documentTools && documentTools->isVisible()
                && visibleGroupButton("resource_authoring.new") && visibleGroupButton("resource_authoring.open")
                && visibleGroupButton("resource_authoring.save") && !sourceDock->titleBarWidget()
                && !sourceDock->windowFlags().testFlag(Qt::FramelessWindowHint) && unchanged(),
            "resource window merge: the decorated source window retains its own New Open Save and document history controls");

        // The primary same-kind dock is a hidden peer of this default mixed
        // native group. Dropping onto its visible other-kind editor must retain
        // window tabs rather than merge into an arbitrary hidden same-kind peer.
        QAction* const createOther = window.findChild<QAction*>(fixture.otherAction);
        check(createOther != nullptr, "resource window merge: a different-kind control group exists");
        if ( !createOther )
        {
            continue;
        }
        createResourceDraftFromGroup(createOther);
        QTabWidget* const otherTabs = window.findChild<QTabWidget*>(fixture.otherTabs);
        QDockWidget* const otherDock = dockForResourceTabHost(otherTabs);
        QPointer<QWidget> otherPage(otherTabs ? otherTabs->currentWidget() : nullptr);
        check(otherTabs && otherDock && otherPage && workspace->tabifiedDockWidgets(primaryDock).contains(otherDock),
            "resource window merge: the different-kind visible target has a hidden same-kind native peer");
        if ( !otherTabs || !otherDock || !otherPage )
        {
            continue;
        }
        const QCborMap otherBefore = authoredDraftState(ui::resources::captureResourceAuthoringPaneState(otherPage.data()));
        otherDock->show();
        otherDock->raise();
        QApplication::processEvents();
        const bool mixedDrop = dragNativeResourceWindowToCenter(sourceDock.data(), otherDock,
            [workspace, sourceDock, otherDock]()
            {
                return sourceDock && !sourceDock->isFloating()
                    && workspace->tabifiedDockWidgets(otherDock).contains(sourceDock.data());
            });
        check(mixedDrop && sourceTabs && sourceTabs->count() == 2 && primaryTabs->count() == 1
                && resourceTabHostContaining(workspace, pages[0].data()) == sourceTabs.data()
                && resourceTabHostContaining(workspace, pages[1].data()) == sourceTabs.data()
                && sourceTabs->currentWidget() == pages[1].data() && otherPage
                && authoredDraftState(ui::resources::captureResourceAuthoringPaneState(otherPage.data())) == otherBefore
                && unchanged(),
            "resource window merge: a real center drop onto the visible other-kind native tab keeps its files separate despite a hidden same-kind peer");
        if ( !sourceDock || !sourceTabs )
        {
            continue;
        }
        sourceDock->setFloating(true);
        sourceDock->move(available.topLeft() + QPoint(60, 100));
        sourceDock->show();
        primaryDock->show();
        primaryDock->raise();
        QApplication::processEvents();
        const bool sameKindDrop = dragNativeResourceWindowToCenter(sourceDock.data(), primaryDock,
            [workspace, primaryTabs, &pages]()
            {
                return primaryTabs->count() == 3
                    && resourceTabHostContaining(workspace, pages[0].data()) == primaryTabs
                    && resourceTabHostContaining(workspace, pages[1].data()) == primaryTabs;
            });
        const bool merged = sameKindDrop && primaryTabs->count() == 3
            && primaryTabs->widget(0) == pages[2].data() && primaryTabs->widget(1) == pages[1].data()
            && primaryTabs->widget(2) == pages[0].data() && primaryTabs->currentWidget() == pages[1].data()
            && window.findChild<QTabWidget*>(fixture.tabs) == primaryTabs && unchanged();
        if ( !merged )
        {
            std::cerr << "resource native window merge: kind=" << static_cast<int>(fixture.kind)
                << " nativeDrop=" << sameKindDrop << " primaryCount=" << primaryTabs->count()
                << " sourceCount=" << (sourceTabs ? sourceTabs->count() : -1)
                << " sourceFloating=" << (sourceDock && sourceDock->isFloating())
                << " tabified=" << (sourceDock && workspace->tabifiedDockWidgets(primaryDock).contains(sourceDock.data()))
                << " exactDrafts=" << unchanged() << '\n';
        }
        check(merged,
            "a real same-kind editor WINDOW center drop joins ordered document tabs with the same widgets, identities, independent histories, and dropped current file");
        if ( merged )
        {
            QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QWidget* const retainedTools = primaryDock->findChild<QWidget*>(QStringLiteral("resource_authoring.group_toolbar"));
            check(!sourceTabs && !sourceDock && !primaryDock->isFloating()
                    && retainedTools && retainedTools->isVisible(),
                "resource window merge removes only the empty detached host and retains the primary native dock and its file toolbar");
        }
    }
}

void testSpriteEditorIconRepresentsSpriteRatherThanMaterial()
{
    MainWindow window(nullptr, true);
    QAction* const spriteAction = window.findChild<QAction*>(QStringLiteral("tools.resource_editors.sprite_spr.new"));
    QDockWidget* const spriteDock = window.findChild<QDockWidget*>(QStringLiteral("resourceEditorsDock"));
    check(spriteAction && spriteDock && !spriteAction->icon().isNull() && !spriteDock->windowIcon().isNull(),
        "Sprite Editor icon exists on the Tools action and editor window");
    if ( !spriteAction || !spriteDock )
    {
        return;
    }
    const QImage action = spriteAction->icon().pixmap(QSize(20, 20)).toImage().convertToFormat(QImage::Format_ARGB32);
    const QImage dock = spriteDock->windowIcon().pixmap(QSize(20, 20)).toImage().convertToFormat(QImage::Format_ARGB32);
    const QImage material = ui::makeViewportIcon(ui::ViewportIcon::textured)
        .pixmap(QSize(20, 20)).toImage().convertToFormat(QImage::Format_ARGB32);
    check(!action.isNull() && action == dock && action != material,
        "Sprite Editor uses one dedicated sprite glyph for its Tools and window icons rather than the MAT texture checker");
}

void testHiddenDockedResourceGeometrySurvivesNativeRecovery()
{
    struct Fixture
    {
        const char* action{};
        const char* dock{};
        const char* tabs{};
    };
    const std::array<Fixture, 3> fixtures{
        Fixture{ "tools.resource_editors.sprite_spr.new", "resourceEditorsDock", "resourceEditorTabs" },
        Fixture{ "tools.resource_editors.sound_class_snd.new", "soundClassResourceEditorsDock", "soundClassResourceEditorTabs" },
        Fixture{ "tools.resource_editors.string_table_uni.new", "uniResourceEditorsDock", "uniResourceEditorTabs" } };
    for ( const Fixture& fixture : fixtures )
    {
        MainWindow window(nullptr, true);
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setProjectTypeForTest(ProjectType::JKDF2);
        window.resize(1440, 900);
        window.show();
        QAction* const action = window.findChild<QAction*>(QString::fromLatin1(fixture.action));
        QDockWidget* const dock = window.findChild<QDockWidget*>(QString::fromLatin1(fixture.dock));
        QTabWidget* const tabs = window.findChild<QTabWidget*>(QString::fromLatin1(fixture.tabs));
        check(action && dock && tabs,
            "hidden native resource geometry fixture resolves its real group and file host");
        if ( !action || !dock || !tabs )
        {
            continue;
        }
        createResourceDraftFromGroup(action);
        QApplication::processEvents();
        check(!dock->isFloating() && tabs->count() == 1 && tabs->currentWidget(),
            "hidden native resource geometry fixture opens one actual docked resource file");
        if ( dock->isFloating() || tabs->count() != 1 || !tabs->currentWidget() )
        {
            continue;
        }
        dock->hide();
        QRect stableGeometry{};
        QSize stableMinimum{};
        int stableSamples = 0;
        QElapsedTimer deadline{};
        deadline.start();
        while ( stableSamples < 2 && deadline.elapsed() < 1000 )
        {
            QApplication::processEvents(QEventLoop::AllEvents, 25);
            const QRect geometry = dock->geometry();
            const QSize minimum = dock->minimumSizeHint();
            if ( dock->isHidden() && !dock->isFloating() && geometry.isValid()
                && geometry == stableGeometry && minimum == stableMinimum )
            {
                ++stableSamples;
            }
            else
            {
                stableGeometry = geometry;
                stableMinimum = minimum;
                stableSamples = 0;
            }
        }
        check(stableSamples >= 2,
            "hidden native resource geometry fixture captures a settled hidden dock-local rectangle");
        const QRect expectedGeometry = dock->geometry();
        const QCborMap expectedPage = authoredDraftState(
            ui::resources::captureResourceAuthoringPaneState(tabs->currentWidget()));
        const QByteArray payload = window.captureRecoveryEditorStateForTest();
        QString warning{};
        const bool valid = window.validateRecoveryEditorStateForTest(payload, &warning);
        const bool restored = valid && window.restoreRecoveryEditorStateForTest(payload, &warning);
        QApplication::processEvents();
        const bool pageExact = tabs->count() == 1 && tabs->currentWidget()
            && authoredDraftState(ui::resources::captureResourceAuthoringPaneState(
                tabs->currentWidget())) == expectedPage;
        const bool geometryExact = dock->geometry() == expectedGeometry;
        const bool preserved = stableSamples >= 2 && restored && warning.isEmpty()
            && !dock->isFloating() && dock->isHidden() && geometryExact && pageExact;
        if ( !preserved )
        {
            const QRect actual = dock->geometry();
            std::cerr << "hidden resource native geometry: group=" << fixture.dock
                      << " valid=" << valid << " restored=" << restored
                      << " warning=" << warning.toStdString()
                      << " hidden=" << dock->isHidden() << " floating=" << dock->isFloating()
                      << " pageExact=" << pageExact << " stableSamples=" << stableSamples
                      << " expected=" << expectedGeometry.x() << ',' << expectedGeometry.y() << ','
                      << expectedGeometry.width() << ',' << expectedGeometry.height()
                      << " actual=" << actual.x() << ',' << actual.y() << ','
                      << actual.width() << ',' << actual.height() << '\n';
        }
        check(preserved,
            "native EDIT recovery preserves each hidden docked SPR SND and UNI local rectangle and exact resource history");
    }
}

void testSoundColumnDistributionAndHoverIconTargets()
{
    ui::resources::ResourceAuthoringContext context{};
    context.resources = std::make_shared<assets::ResourceLocator>();
    std::unique_ptr<QWidget> pane(ui::resources::createResourceAuthoringPane(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, context));
    check(pane != nullptr, "Sound column fixture creates the real authoring pane");
    if ( !pane )
    {
        return;
    }
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
    state.remove(QStringLiteral("history"));
    state.insert(QStringLiteral("rows"), QCborArray{QCborValue(QCborArray{
        QStringLiteral("moving"), QStringLiteral("a_long_archive_sound_name.wav"),
        QStringLiteral("0x00000040"), QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("1")})});
    state.insert(QStringLiteral("soundPreviewLoops"), QCborArray{false});
    state.insert(QStringLiteral("modified"), false);
    QString warning{};
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning),
        "Sound column fixture restores one authored alternative through native draft state");
    auto* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    check(tree && tree->topLevelItemCount() == 1,
        "Sound column fixture exposes its authored mode and alternative rows");
    if ( !tree || tree->topLevelItemCount() == 0 )
    {
        return;
    }
    QTreeWidgetItem* group = nullptr;
    QTreeWidgetItem* row = nullptr;
    for ( int groupIndex = 0; groupIndex < tree->topLevelItemCount() && !row; ++groupIndex )
    {
        QTreeWidgetItem* const candidate = tree->topLevelItem(groupIndex);
        if ( !candidate || candidate->data(0, ui::resources::detail::kSoundTreeItemKindRole).toInt()
                != static_cast<int>(ui::resources::detail::SoundTreeItemKind::mode) )
        {
            continue;
        }
        for ( int childIndex = 0; childIndex < candidate->childCount(); ++childIndex )
        {
            QTreeWidgetItem* const child = candidate->child(childIndex);
            if ( child && child->data(0, ui::resources::detail::kSoundTreeItemKindRole).toInt()
                    == static_cast<int>(ui::resources::detail::SoundTreeItemKind::alternative) )
            {
                group = candidate;
                row = child;
                break;
            }
        }
    }
    check(group && row,
        "Sound column fixture locates the authored alternative past its headings and add-row footer");
    if ( !group || !row )
    {
        return;
    }

    struct ColumnSample
    {
        int sound{};
        int volume{};
        int viewport{};
        int minimum{};
    };
    std::array<ColumnSample, 3> samples{};
    pane->show();
    const std::array<int, 3> paneWidths{900, 1080, 1260};
    const std::array<int, 6> columnMinimums{80, 160, 100, 56, 56, 230};
    for ( std::size_t sampleIndex = 0; sampleIndex < paneWidths.size(); ++sampleIndex )
    {
        pane->resize(paneWidths[sampleIndex], 700);
        QElapsedTimer settle{};
        settle.start();
        while ( settle.elapsed() < 40 )
        {
            QApplication::processEvents(QEventLoop::AllEvents);
        }
        ColumnSample& sample = samples[sampleIndex];
        sample.sound = tree->columnWidth(1);
        sample.volume = tree->columnWidth(5);
        sample.viewport = tree->viewport()->width();
        sample.minimum = pane->minimumWidth();
        for ( int column = 0; column < tree->columnCount(); ++column )
        {
            check(tree->columnWidth(column) >= columnMinimums[static_cast<std::size_t>(column)],
                "Sound pane resizing preserves each column's usable minimum");
        }
        check(tree->header()->length() <= sample.viewport,
            "Automatic Sound columns remain inside the viewport without overlap or horizontal scrolling");
    }
    const int soundGrowth = samples[2].sound - samples[1].sound;
    const int volumeGrowth = samples[2].volume - samples[1].volume;
    check(soundGrowth > volumeGrowth && volumeGrowth >= 0,
        "Sound pane width first uses the 65/35 split, then transfers 40% of Volume width into Sound while respecting the minimum");

    tree->setColumnWidth(1, tree->columnWidth(1) + 60);
    QApplication::processEvents(QEventLoop::AllEvents);
    check(tree->columnWidth(1) >= samples[2].sound + 59,
        "A manually enlarged Sound column keeps its requested width");
    group->setExpanded(true);
    const QRect rowRect = tree->visualItemRect(row);
    const QPoint local = rowRect.center();
    const QPoint global = tree->viewport()->mapToGlobal(local);
    QMouseEvent move(QEvent::MouseMove, QPointF(local), QPointF(local), QPointF(global),
        Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &move);
    QApplication::processEvents(QEventLoop::AllEvents);
    auto* const play = pane->findChild<QToolButton*>(QStringLiteral("snd.play"));
    auto* const loop = pane->findChild<QToolButton*>(QStringLiteral("snd.loop"));
    auto* const remove = pane->findChild<QToolButton*>(QStringLiteral("snd.remove"));
    check(play && loop && remove && play->isVisible() && loop->isVisible() && remove->isVisible()
            && play->geometry().size() == QSize(22, 22)
            && loop->geometry().size() == QSize(22, 22)
            && remove->geometry().size() == QSize(22, 22),
        "Play, Loop and Remove keep their 22-pixel hover click targets after column resizing");
    check(play && loop && remove && play->iconSize() == QSize(13, 13)
            && loop->iconSize() == QSize(13, 13) && remove->iconSize() == QSize(13, 13),
        "Sound action glyphs use 13-pixel icons while preserving the existing 22-pixel hover hit areas");
    if ( loop )
    {
        const QImage loopImage = loop->icon().pixmap(QSize(13, 13)).toImage();
        int antialiasedPixels = 0;
        for ( int y = 0; y < loopImage.height(); ++y )
        {
            for ( int x = 0; x < loopImage.width(); ++x )
            {
                const int alpha = loopImage.pixelColor(x, y).alpha();
                antialiasedPixels += alpha > 0 && alpha < 255 ? 1 : 0;
            }
        }
        check(antialiasedPixels > 0,
            "The smaller Sound loop glyph retains antialiased arc and arrow edges");
    }
}

void runResourceAuthoringLifecycleTests()
{
    testSoundColumnDistributionAndHoverIconTargets();
    testHiddenDockedResourceGeometrySurvivesNativeRecovery();
    testDockWindowDropCallbackGuards();
    testResourceNativeWindowDropsMergeSameKindDocuments();
    testDocumentTabJoinFeedbackAcrossAllKinds();
    testDocumentTabJoinFeedbackOnOpenEmptyTarget();
    testSpriteEditorIconRepresentsSpriteRatherThanMaterial();
    testExternalResourceSaveInvalidatesNativeDraftBaseline();
    testResourceEditorToolsOnlyAndNativeGroupTabIcons();
    testSpritePaneUsesProjectProfileAndFocusedControls();
    testSpriteAnimationControlsAndPreviewPicking();
    testSpritePreviewSurvivesRepeatedFloatAndRedock();
    testSpritePreviewFitKeepsBoundsInsideWideAndTallFrames();
    testSpritePausedPresentationRecoverySchema();
    testSpriteCameraPresentationBoundaryValidation();
    testPaneCaptureCountersCountRealRows();
    testUnsavedCloseCancelPreservesDraft();
    testUnsavedNewCancelPreservesDraft();
    testProjectOpenRejectsChangedContextAfterPrompt();
    testMaterialPickerRejectsChangedDraft();
    testMaterialPickerNoOpPreservesExactDraft();
    testWavPickerTargetsRepeatedNameRow();
    testWavPickerRejectsChangedRow();
    testOversizedValidUniOpenPreservesDirtyDraft();
    testOpenParsesAndHashesOneReadSnapshot();
    testProjectOpenUsesWinningArchiveSourceForIdentity();
    testArchiveBackedResourceTabTooltipsIdentifyMountedSource();
    testProjectOpenCatchesTruncatedWinningArchiveRead();
    testSaveAsRechecksDestinationAfterOverwritePrompt();
    testPaneDeletionDuringUnsavedPrompt();
    testResourceWorkspaceNewOpenAndToolbarShell();
    testResourceDocumentHistoryToolbars();
    testToolsResourceGroupsOpenEmptyAndSaveOnlyEdits();
    testResourceMajorGroupsOwnOneToolbarAndIndependentDocks();
    testResourceTabClosePromptNamesItsDirtyDraft();
    testDetachedResourceFilesRetainWindowChromeAndDocking();
    testResourceGroupNativeSplitsAndCanvasContrast();
    testResourceGroupTopDockHeaderAndTabGrouping();
    testResourceGroupSaveAllSuccessCancelAndFailure();
    testResourceGroupSaveAllIncludesDetachedMembers();
    testConfiguredResourceRootSeedsSaveAsWithoutWriting();
    testResourceDraftPeriodicAutosaveDiskRoundTrip();
    testResourceEditorHostAdmissionAt64Hosts();
    testSpritePendingNumericEditTracksNativeBaseline();
    testResourceEditorCaptureSelectsFloatingHostWithoutEditorFocus();
    testResourceEditorTabDragTransferAndEscape();
}

} // namespace sed::app_tests
