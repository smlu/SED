#include "main_window/main_window.hpp"
#include "native_caption_test_utils.hpp"
#include "resource_release_test_fixtures.hpp"
#include "sed/assets/episode.hpp"
#include "sed/assets/snd.hpp"
#include "sed/assets/spr.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/sed.hpp"
#include "sed/assets/string_table.hpp"
#include "sed/image.hpp"
#include "test_common.hpp"
#include "gl_view.hpp"
#include "ui/episode/episode_editor.hpp"
#include "ui/components/live_color_popup.hpp"
#include "ui/recovery/document_recovery_utils.hpp"
#include "ui/project/sed_editor_file_utils.hpp"
#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/resources/resource_picker.hpp"
#include "ui/audio/app_sound_player.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/shared_ui_styles.hpp"

#include <QApplication>
#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QPersistentModelIndex>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyleOptionSlider>
#include <QCursor>
#include <QScrollBar>
#include <QScreen>
#include <QPainter>
#include <QAbstractButton>
#include <QAction>
#include <QByteArray>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QListWidget>
#include <QDockWidget>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHelpEvent>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMenu>
#include <QMetaObject>
#include <QInputDialog>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QToolButton>
#include <QToolTip>
#include <QWidget>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace sed::app_tests
{

namespace
{

QString pathForDialog(const std::filesystem::path& path)
{
    return QString::fromStdWString(path.wstring());
}

bool writeBytes(const std::filesystem::path& path, const std::string_view bytes)
{
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

bool writeFixtureBytes(const std::filesystem::path& path, const QByteArray& bytes)
{
    return writeBytes(
        path,
        std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
}

QByteArray makeSolidRgbaMatFixture(const QColor& color)
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
    appendU32(1u);
    appendU32(1u);
    appendU32(static_cast<std::uint32_t>(ColorMode::RGBA));
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
    bytes.append(QByteArray(40, '\0'));
    appendU32(2u);
    appendU32(2u);
    appendU32(0u);
    appendU32(0u);
    appendU32(0u);
    appendU32(1u);
    for ( int index = 0; index < 4; ++index )
    {
        bytes.append(static_cast<char>(color.blue()));
        bytes.append(static_cast<char>(color.green()));
        bytes.append(static_cast<char>(color.red()));
        bytes.append(static_cast<char>(color.alpha()));
    }
    return bytes;
}

std::string readBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
}

bool hasRedSpritePixels(const QImage& image)
{
    if ( image.isNull() )
    {
        return false;
    }
    for ( int y = 0; y < image.height(); ++y )
    {
        for ( int x = 0; x < image.width(); ++x )
        {
            const QColor pixel = image.pixelColor(x, y);
            if ( pixel.red() > pixel.green() + 35
                && pixel.red() > pixel.blue() + 35
                && pixel.red() > 60 )
            {
                return true;
            }
        }
    }
    return false;
}

QAbstractButton* button(QWidget* const parent, const char* const objectName)
{
    return parent->findChild<QAbstractButton*>(QString::fromLatin1(objectName));
}

QAction* action(QWidget* const parent, const char* const objectName)
{
    return parent->findChild<QAction*>(QString::fromLatin1(objectName));
}

void runWithModalDriver(
    const std::function<void()>& action,
    const std::optional<std::filesystem::path>& fileToSelect,
    const std::optional<int> messageResult,
    bool rejectOtherDialog,
    const std::function<void(QFileDialog*)>& fileDialogHandler,
    const std::function<void(QWidget*)>& dialogHandler,
    const std::function<void(QMessageBox*)>& messageDialogHandler);

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

QTreeWidgetItem* soundAlternativeByWav(QTreeWidget* const tree, const QString& wav)
{
    for ( QTreeWidgetItem* const item : soundAlternativeItems(tree) )
    {
        if ( item->text(1) == wav )
        {
            return item;
        }
    }
    return nullptr;
}

bool setSoundPreviewLoop(QTreeWidget* const tree, QTreeWidgetItem* const item, const bool loop)
{
    if ( !tree || !item || !item->parent() )
    {
        return false;
    }
    tree->window()->show();
    tree->setAnimated(false);
    item->parent()->setExpanded(true);
    tree->scrollToItem(item);
    QApplication::processEvents(QEventLoop::AllEvents);
    const QPoint local = tree->visualRect(tree->indexFromItem(item, 0)).center();
    QMouseEvent move(QEvent::MouseMove, QPointF(local), QPointF(tree->viewport()->mapToGlobal(local)), Qt::NoButton,
                     Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &move);
    QToolButton* const button = tree->findChild<QToolButton*>(QStringLiteral("snd.loop"));
    if ( !button || !button->isVisible() )
    {
        return false;
    }
    if ( button->isChecked() != loop )
    {
        button->click();
    }
    return button->isChecked() == loop;
}

bool clickSoundTreeItem(QTreeWidget* const tree, QTreeWidgetItem* const item, const int column = 1)
{
    return tree && item && QMetaObject::invokeMethod(
        tree,
        "itemClicked",
        Qt::DirectConnection,
        Q_ARG(QTreeWidgetItem*, item),
        Q_ARG(int, column));
}

bool addSoundAlternative(QTreeWidget* tree, const QString& mode);

bool addSoundMode(QWidget* const editor, QTreeWidget* const tree, const QString& mode)
{
    QAbstractButton* const add = editor ? button(editor, "snd.add_mode") : nullptr;
    if ( !tree || !add )
    {
        return false;
    }
    bool promptOpened = false;
    const int beforeCount = soundAlternativeItems(tree).size();
    runWithModalDriver(
        [add]()
        {
            add->click();
        },
        std::nullopt, std::nullopt, false, {},
        [&](QWidget* active)
        {
            auto* const popup = qobject_cast<QDialog*>(active);
            QListWidget* const list = popup ? popup->findChild<QListWidget*>(QStringLiteral("snd.mode.list")) : nullptr;
            if ( !popup || !list )
            {
                return;
            }
            promptOpened = true;
            const QList<QListWidgetItem*> matches = list->findItems(mode, Qt::MatchExactly);
            if ( !matches.isEmpty() )
            {
                list->setCurrentItem(matches.front());
                popup->accept();
            }
            else
            {
                popup->reject();
            }
        },
        {});
    return promptOpened && addSoundAlternative(tree, mode) && soundAlternativeItems(tree).size() == beforeCount + 1;
}

bool addSoundAlternative(QTreeWidget* const tree, const QString& mode)
{
    if ( !tree )
    {
        return false;
    }
    for ( int groupIndex = 0; groupIndex < tree->topLevelItemCount(); ++groupIndex )
    {
        QTreeWidgetItem* const group = tree->topLevelItem(groupIndex);
        if ( group && group->data(0, Qt::UserRole + 22).toByteArray() == mode.toUtf8() )
        {
            for ( int childIndex = 0; childIndex < group->childCount(); ++childIndex )
            {
                QTreeWidgetItem* const child = group->child(childIndex);
                if ( child && child->text(0) == QStringLiteral("＋ Add Entry") )
                {
                    const int beforeCount = soundAlternativeItems(tree).size();
                    return clickSoundTreeItem(tree, child) && soundAlternativeItems(tree).size() == beforeCount + 1;
                }
            }
        }
    }
    return false;
}

void typeIntoLineEdit(QLineEdit* const edit, const QString& text)
{
    if ( !edit )
    {
        return;
    }
    edit->selectAll();
    for ( const QChar character : text )
    {
        const int key = character.isDigit()
            ? Qt::Key_0 + character.digitValue()
            : character.toUpper().unicode();
        const QString keyText(character);
        QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, keyText);
        QApplication::sendEvent(edit, &press);
        QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, keyText);
        QApplication::sendEvent(edit, &release);
    }
}

void setScrubValue(ui::ScrubFloatEdit* const edit, const double value)
{
    if ( !edit )
    {
        return;
    }
    edit->setValue(value);
    edit->valueCommitted(edit->value());
}

void runWithModalDriver(
    const std::function<void()>& action,
    const std::optional<std::filesystem::path>& fileToSelect = std::nullopt,
    const std::optional<int> messageResult = std::nullopt,
    const bool rejectOtherDialog = false,
    const std::function<void(QFileDialog*)>& fileDialogHandler = {},
    const std::function<void(QWidget*)>& dialogHandler = {},
    const std::function<void(QMessageBox*)>& messageDialogHandler = {})
{
    QTimer timer{};
    timer.setInterval(5);
    bool watchdogExpired = false;
    QTimer watchdog{};
    watchdog.setSingleShot(true);
    QPointer<QWidget> lastHandled{};
    QObject::connect(&timer, &QTimer::timeout, QApplication::instance(), [&]()
        {
            QWidget* const active = QApplication::activeModalWidget();
            if ( !active )
            {
                return;
            }
            if ( watchdogExpired )
            {
                if ( auto* const fileDialog = qobject_cast<QFileDialog*>(active) )
                {
                    fileDialog->reject();
                }
                else if ( auto* const message = qobject_cast<QMessageBox*>(active) )
                {
                    message->done(QMessageBox::Cancel);
                }
                else if ( auto* const dialog = qobject_cast<QDialog*>(active) )
                {
                    dialog->done(QDialog::Rejected);
                }
                else if ( auto* const menu = qobject_cast<QMenu*>(active) )
                {
                    menu->close();
                }
                else
                {
                    active->close();
                }
                return;
            }
            if ( lastHandled == active )
            {
                return;
            }
            if ( auto* const fileDialog = qobject_cast<QFileDialog*>(active) )
            {
                if ( fileDialogHandler )
                {
                    lastHandled = active;
                    fileDialogHandler(fileDialog);
                    return;
                }
                if ( fileToSelect )
                {
                    fileDialog->setOption(QFileDialog::DontConfirmOverwrite, true);
                    fileDialog->selectFile(pathForDialog(*fileToSelect));
                    lastHandled = active;
                    (void)QMetaObject::invokeMethod(
                        fileDialog,
                        "accept",
                        Qt::DirectConnection);
                }
                return;
            }
            if ( auto* const message = qobject_cast<QMessageBox*>(active) )
            {
                if ( messageDialogHandler )
                {
                    lastHandled = active;
                    messageDialogHandler(message);
                }
                if ( messageResult )
                {
                    lastHandled = active;
                    message->done(*messageResult);
                }
                else if ( watchdogExpired )
                {
                    lastHandled = active;
                    message->done(QMessageBox::Cancel);
                }
                return;
            }
            if ( dialogHandler )
            {
                if ( auto* const dialog = qobject_cast<QDialog*>(active) )
                {
                    lastHandled = active;
                    dialogHandler(dialog);
                    return;
                }
            }
            if ( rejectOtherDialog )
            {
                if ( auto* const dialog = qobject_cast<QDialog*>(active) )
                {
                    lastHandled = active;
                    dialog->reject();
                }
            }
        });
    QObject::connect(&watchdog, &QTimer::timeout, QApplication::instance(), [&]()
        {
            QWidget* active = QApplication::activeModalWidget();
            if ( !active )
            {
                active = QApplication::activePopupWidget();
            }
            if ( !active )
            {
                return;
            }

            watchdogExpired = true;
            std::cerr << "resource authoring modal watchdog: title=\""
                      << active->windowTitle().toStdString()
                      << "\" objectName=\"" << active->objectName().toStdString()
                      << "\"\n";
            if ( auto* const menu = qobject_cast<QMenu*>(active) )
            {
                menu->close();
            }
            else if ( auto* const fileDialog = qobject_cast<QFileDialog*>(active) )
            {
                fileDialog->reject();
            }
            else if ( auto* const message = qobject_cast<QMessageBox*>(active) )
            {
                message->done(QMessageBox::Cancel);
            }
            else if ( auto* const dialog = qobject_cast<QDialog*>(active) )
            {
                dialog->done(QDialog::Rejected);
            }
            else
            {
                active->close();
            }
        });
    timer.start();
    watchdog.start(2500);
    action();
    timer.stop();
    watchdog.stop();
    check(!watchdogExpired,
        "resource authoring modal driver completes before its diagnostic watchdog");
    QApplication::processEvents(QEventLoop::AllEvents);
}

bool waitUntil(const std::function<bool()>& condition, const int timeoutMs = 2500)
{
    QElapsedTimer elapsed{};
    elapsed.start();
    while ( elapsed.elapsed() < timeoutMs )
    {
        QApplication::processEvents(QEventLoop::AllEvents);
        if ( condition() )
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    return condition();
}

ui::resources::ResourceAuthoringContext testContext(const ProjectType projectType = ProjectType::JKDF2)
{
    return ui::resources::ResourceAuthoringContext{
        std::make_shared<const assets::ResourceLocator>(),
        projectType,
        {},
        {},
        "editor_preview_master.cmp" };
}

void openResourceFile(QWidget* const editor, const std::filesystem::path& path)
{
    QAbstractButton* const open = button(editor, "resource_authoring.open");
    if ( !open )
    {
        return;
    }
    runWithModalDriver([open]() { open->click(); }, path);
}

void saveAsResourceFile(QWidget* const editor, const std::filesystem::path& path)
{
    QAction* const saveAs = action(editor, "resource_authoring.save_as");
    if ( !saveAs )
    {
        return;
    }
    runWithModalDriver([saveAs]() { saveAs->trigger(); }, path);
}

void checkEditorContract(
    const ui::resources::ResourceAuthoringKind kind,
    const QString& expectedTitle,
    const char* const* requiredObjectNames,
    const std::size_t requiredObjectNameCount)
{
    const ui::resources::ResourceAuthoringContext context{
        std::make_shared<const assets::ResourceLocator>(),
        ProjectType::JKDF2,
        {},
        {},
        "editor_preview_master.cmp" };
    std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
        nullptr,
        kind,
        context));

    check(editor != nullptr, "resource authoring editor factory returns a dialog");
    if ( !editor )
    {
        return;
    }

    check(editor->windowTitle() == expectedTitle,
        "resource authoring dialog has a resource-specific title");
    check(
        (editor->windowFlags() & Qt::WindowMinMaxButtonsHint) == 0,
        "resource authoring dialog hides minimize and maximize controls");

    const QString expectedFileName = kind == ui::resources::ResourceAuthoringKind::sprite
        ? QStringLiteral("new_sprite.spr")
        : kind == ui::resources::ResourceAuthoringKind::soundClass
            ? QStringLiteral("new_sound_class.snd") : QStringLiteral("cogstrings.uni");
    const QCborMap draft = ui::resources::captureResourceAuthoringPaneState(editor.get());
    const QString draftIdentity = draft.value(QStringLiteral("draftIdentity")).toString();
    check(!editor->findChild<QLabel*>(QStringLiteral("resource_authoring.document"))
            && draft.value(QStringLiteral("name")).toString() == expectedFileName
            && draft.value(QStringLiteral("path")).toString().isEmpty()
            && !draftIdentity.isEmpty()
            && ui::resources::resourceAuthoringPaneTitle(editor.get()) == expectedFileName
            && ui::resources::resourceAuthoringPaneIdentity(editor.get())
                == QStringLiteral("draft:") + draftIdentity,
        "resource authoring retains its exact named draft identity without a permanent filename or path label");

    for ( std::size_t index = 0; index < requiredObjectNameCount; ++index )
    {
        const QObject* const control = editor->findChild<QObject*>(
            QString::fromLatin1(requiredObjectNames[index]));
        check(control != nullptr, requiredObjectNames[index]);
    }
}

void checkSpriteRoundTripsAndIsolatedPreview()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(), "resource authoring: temporary sprite fixture directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path root(temporaryDirectory.path().toStdWString());
    const QByteArray materialFixture = fixtures::makeIndexedMatFixture(6u);
    const QByteArray paletteFixture =
        fixtures::makeClassicCmpFixture(6u, QColor(240, 24, 32));
    check(writeFixtureBytes(root / "fx.mat", materialFixture)
            && writeFixtureBytes(root / "editor_preview_master.cmp", paletteFixture),
        "resource authoring: synthetic indexed MAT and master-CMP preview dependencies are available");
    auto sourceResources = std::make_shared<assets::ResourceLocator>();
    sourceResources->addRoot(root);
    const std::shared_ptr<const assets::ResourceLocator> originalResources = sourceResources;
    auto classicContext = testContext();
    classicContext.resources = originalResources;
    const std::filesystem::path classicPath = root / "precision.spr";
    constexpr std::string_view classicText =
        "fx.mat 0 0.50000006 3.25 4 3 0 0.125 0.25 -0.375 0.5\n";
    constexpr std::string_view classicExpected =
        "fx.mat 0 0.50000006 3.25 4 3 0 0.125 0.25 -0.375 0.5\n";
    check(writeBytes(classicPath, classicText), "resource authoring: classic SPR fixture writes");

    auto classicEditor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        std::move(classicContext)));
    if ( classicEditor )
    {
        openResourceFile(classicEditor.get(), classicPath);
        auto* const material = classicEditor->findChild<QLineEdit*>(QStringLiteral("spr.material"));
        auto* const width = classicEditor->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"));
        auto* const save = button(classicEditor.get(), "resource_authoring.save");
        auto* const preview = classicEditor->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
        check(material && material->text() == QStringLiteral("fx.mat"),
            "resource authoring: opening a SPR loads its material into the editor");
        check(width != nullptr, "resource authoring: SPR width control is available for precision regression");
        check(preview != nullptr, "resource authoring: sprite authoring uses a real GlView preview");
        QImage firstFrame{};
        QImage stableFrame{};
        const auto previewResourceMessages = std::make_shared<std::vector<std::string>>();
        const auto dumpPreviewDiagnostics = [&](const char* reason)
        {
            if ( !preview )
            {
                return;
            }
            temporaryDirectory.setAutoRemove(false);
            const std::filesystem::path diagnosticPath =
                root / "resource_authoring_preview_failure.png";
            const QImage diagnosticFrame = preview->grabFramebuffer();
            (void)diagnosticFrame.save(pathForDialog(diagnosticPath));
            const GlView::RecoveryState state = preview->recoveryState();
            const std::string serializedDraft = preview
                ->property("spr.preview.serialized_resource")
                .toString()
                .toStdString();
            std::cerr << "resource authoring preview diagnostic (" << reason << ")"
                      << " draft=[" << serializedDraft << "]"
                      << " camera[yaw=" << state.preview.orbit.yawDeg
                      << ", pitch=" << state.preview.orbit.pitchDeg
                      << ", distance=" << state.preview.orbit.distance
                      << ", center=" << state.preview.orbit.center.x() << ','
                      << state.preview.orbit.center.y() << ','
                      << state.preview.orbit.center.z() << "]"
                      << " framebuffer=" << diagnosticFrame.width() << 'x'
                      << diagnosticFrame.height()
                      << " image=" << diagnosticPath.string() << '\n';
            for ( const std::string& message : *previewResourceMessages )
            {
                std::cerr << "  " << message << '\n';
            }
        };
        if ( preview )
        {
            QObject::connect(
                preview,
                &GlView::resourceWarningMessage,
                preview,
                [previewResourceMessages](const QString& message)
                {
                    previewResourceMessages->push_back("warning: " + message.toStdString());
                });
            QObject::connect(
                preview,
                &GlView::resourceErrorMessage,
                preview,
                [previewResourceMessages](const QString& message)
                {
                    previewResourceMessages->push_back("error: " + message.toStdString());
                });
            check(preview->previewInteractionEnabled(),
                "resource authoring: sprite preview keeps camera interaction enabled");
            check(preview->property("resource_authoring.preview_cmp").toString()
                    == QStringLiteral("editor_preview_master.cmp"),
                "resource authoring: active master CMP is applied to the isolated preview level");
            check(preview->property("spr.preview.isolated_resource_locator").toBool(),
                "resource authoring: draft sprite preview uses a private resource-locator snapshot");
            classicEditor->resize(1180, 760);
            classicEditor->show();
            const bool rendered = waitUntil([&]()
                {
                    firstFrame = preview->grabFramebuffer();
                    if ( firstFrame.isNull()
                        || !preview->property("spr.preview.serialized_resource")
                                .toString()
                                .contains(QStringLiteral("fx.mat 0 0.50000006 3.25 4 3 0")) )
                    {
                        return false;
                    }
                    for ( int y = 0; y < firstFrame.height(); ++y )
                    {
                        for ( int x = 0; x < firstFrame.width(); ++x )
                        {
                            const QColor pixel = firstFrame.pixelColor(x, y);
                            if ( pixel.red() > pixel.green() + 35
                                && pixel.red() > pixel.blue() + 35
                                && pixel.red() > 60 )
                            {
                                return true;
                            }
                        }
                    }
                    return false;
                });
            check(rendered,
                "resource authoring: isolated draft sprite renders colored pixels after its draft resource is published");
            if ( !rendered )
            {
                dumpPreviewDiagnostics("initial frame");
            }
            if ( rendered )
            {
                std::size_t redPixelCount = 0;
                for ( int y = 0; y < firstFrame.height(); ++y )
                {
                    for ( int x = 0; x < firstFrame.width(); ++x )
                    {
                        const QColor pixel = firstFrame.pixelColor(x, y);
                        if ( pixel.red() > pixel.green() + 35
                            && pixel.red() > pixel.blue() + 35
                            && pixel.red() > 60 )
                        {
                            ++redPixelCount;
                        }
                    }
                }
                check(redPixelCount > 0,
                    "resource authoring: current master CMP palette reaches the rendered sprite draft");

                const float yawBefore = preview->recoveryState().preview.orbit.yawDeg;
                const QVector3D cameraEyeBefore = preview->cameraEye();
                const auto cornersBefore = preview->thingSpriteCornersForTest(0U);
                const QPoint pressPoint(60, 60);
                const QPoint movePoint(96, 78);
                QMouseEvent press(
                    QEvent::MouseButtonPress,
                    QPointF(pressPoint),
                    QPointF(preview->mapToGlobal(pressPoint)),
                    Qt::LeftButton,
                    Qt::LeftButton,
                    Qt::NoModifier);
                QApplication::sendEvent(preview, &press);
                QMouseEvent move(
                    QEvent::MouseMove,
                    QPointF(movePoint),
                    QPointF(preview->mapToGlobal(movePoint)),
                    Qt::NoButton,
                    Qt::LeftButton,
                    Qt::NoModifier);
                QApplication::sendEvent(preview, &move);
                QMouseEvent release(
                    QEvent::MouseButtonRelease,
                    QPointF(movePoint),
                    QPointF(preview->mapToGlobal(movePoint)),
                    Qt::LeftButton,
                    Qt::NoButton,
                    Qt::NoModifier);
                QApplication::sendEvent(preview, &release);
                const float yawAfter = preview->recoveryState().preview.orbit.yawDeg;
                check(yawAfter != yawBefore,
                    "resource authoring: drag interaction orbits the isolated sprite preview");
                const bool orbitGeometryChanged = waitUntil([&]()
                    {
                        stableFrame = preview->grabFramebuffer();
                        const auto cornersAfter = preview->thingSpriteCornersForTest(0U);
                        if ( stableFrame.isNull() || !cornersBefore || !cornersAfter )
                        {
                            return false;
                        }
                        const float edgeBefore = ((*cornersBefore)[1] - (*cornersBefore)[0]).length();
                        const float edgeAfter = ((*cornersAfter)[1] - (*cornersAfter)[0]).length();
                        return preview->cameraEye() != cameraEyeBefore
                            && *cornersAfter != *cornersBefore
                            && edgeBefore > 0.0f
                            && std::abs(edgeAfter - edgeBefore) < 0.00001f;
                    });
                check(orbitGeometryChanged,
                    "resource authoring: orbit moves the camera and drawn billboard corners while retaining authored card dimensions");
            }
        }

        if ( save )
        {
            save->click();
            check(readBytes(classicPath) == classicExpected,
                "resource authoring: Save writes the complete fixed classic SPR text vector");
            try
            {
                std::istringstream originalInput{ std::string(classicText) };
                std::istringstream savedInput{ readBytes(classicPath) };
                const assets::SprFile original = assets::loadFromSPR(originalInput);
                const assets::SprFile saved = assets::loadFromSPR(savedInput);
                check(std::bit_cast<std::uint32_t>(saved.width)
                        == std::bit_cast<std::uint32_t>(original.width)
                        && std::bit_cast<std::uint32_t>(saved.offsetXYZ[0])
                            == std::bit_cast<std::uint32_t>(original.offsetXYZ[0]),
                    "resource authoring: untouched SPR floats retain their exact binary32 values");
            }
            catch (const std::exception&)
            {
                check(false, "resource authoring: fixed classic SPR vector remains parseable after Save");
            }
        }

        if ( width && preview )
        {
            setScrubValue(width, 9.25);
            QImage editedFrame{};
            const bool previewUpdated = waitUntil([&]()
                {
                    if ( !preview->property("spr.preview.serialized_resource").toString()
                            .contains(QStringLiteral("fx.mat 0 9.25 3.25")) )
                    {
                        return false;
                    }
                    editedFrame = preview->grabFramebuffer();
                    return !stableFrame.isNull()
                        && !editedFrame.isNull()
                        && editedFrame != stableFrame;
                });
            check(previewUpdated,
                "resource authoring: width edits reach the draft resource and change the rendered sprite frame");
            if ( !previewUpdated )
            {
                dumpPreviewDiagnostics("width edit");
            }
            check(!std::filesystem::exists(root / "draft.spr")
                    && !originalResources->primaryResourceOverrideRevision("draft.spr").has_value(),
                "resource authoring: preview drafts never touch disk or the caller's resource locator");
        }

        // The material picker is a project-aware resource selector. Cancelling it
        // must leave the authored token alone.
        if ( material )
        {
            const QString before = material->text();
            if ( QAbstractButton* const pick = button(classicEditor.get(), "spr.material.pick") )
            {
                runWithModalDriver([pick]() { pick->click(); }, std::nullopt, std::nullopt, true);
                check(material->text() == before,
                    "resource authoring: cancelling the material picker preserves the sprite material");
            }
            else
            {
                check(false, "resource authoring: sprite material picker button exists");
            }
        }
    }

    // Unknown modes remain lossless in the data editor even though the preview
    // fixture above uses drawable geometry values for the real 3D render check.
    const std::filesystem::path unusualModesPath = root / "unusual_modes.spr";
    constexpr std::string_view unusualModesText =
        "odd.mat 2 0.50000006 3.25 5 -3 167 0.125 8.25 -9.5 10.75\n";
    check(writeBytes(unusualModesPath, unusualModesText),
        "resource authoring: unusual SPR mode fixture writes");
    auto unusualModesEditor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext()));
    if ( unusualModesEditor )
    {
        openResourceFile(unusualModesEditor.get(), unusualModesPath);
        if ( QAbstractButton* const save = button(unusualModesEditor.get(), "resource_authoring.save") )
        {
            save->click();
            check(readBytes(unusualModesPath) == unusualModesText,
                "resource authoring: unusual sprite type and mode integers round-trip without flattening");
        }
        else
        {
            check(false, "resource authoring: unusual mode editor exposes Save");
        }
    }
    else
    {
        check(false, "resource authoring: unusual mode preservation editor exists");
    }

    const std::filesystem::path ijimPath = root / "colored.spr";
    constexpr std::string_view ijimText =
        "glow.mat 1 2.125 4.25 4 5 6 0.125 0.25 0.375 0.5 -7 8 9\n";
    constexpr std::string_view ijimExpected =
        "glow.mat 1 2.125 4.25 4 5 6 0.125 0.25 0.375 0.5 -7 8 9\n";
    check(writeBytes(ijimPath, ijimText), "resource authoring: IJIM SPR fixture writes");
    auto ijimEditor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext(ProjectType::IJIM)));
    if ( ijimEditor )
    {
        openResourceFile(ijimEditor.get(), ijimPath);
        const QCborMap paneState = ui::resources::captureResourceAuthoringPaneState(
            ijimEditor.get());
        auto* const swatch = ijimEditor->findChild<QPushButton*>(
            QStringLiteral("spr.extra_light.swatch"));
        auto* const save = button(ijimEditor.get(), "resource_authoring.save");
        check(paneState.value(QStringLiteral("profile")).toInteger()
                == static_cast<int>(assets::SprProfile::ijim)
                && !ijimEditor->findChild<QComboBox*>(QStringLiteral("spr.profile")),
            "resource authoring: SPR format follows the active IJIM project without a profile selector");
        check(swatch != nullptr,
            "resource authoring: IJIM extra light is represented by a color swatch");
        if ( swatch )
        {
            swatch->click();
            auto* const popup = dynamic_cast<ui::LiveColorPopup*>(
                ijimEditor->findChild<QWidget*>(QStringLiteral("spr.extra_light.popup")));
            check(popup && popup->isVisible(),
                "resource authoring: IJIM color swatch opens its live RGBA popup");
            if ( popup )
            {
                check(popup->currentColor().isValid() && popup->findChild<QSlider*>(
                        QStringLiteral("liveColorAlpha")) != nullptr,
                    "resource authoring: IJIM color popup exposes alpha control");
                popup->close();
                QApplication::processEvents();
            }
        }
        if ( save )
        {
            save->click();
            check(readBytes(ijimPath) == ijimExpected,
                "resource authoring: Save writes the complete fixed IJIM RGBA SPR text vector");
            try
            {
                std::istringstream savedInput{ readBytes(ijimPath) };
                const assets::SprFile saved = assets::loadFromSPR(savedInput);
                check(saved.extraLightRGBA == std::array<float, 4>{ 0.125f, 0.25f, 0.375f, 0.5f }
                        && saved.offsetXYZ == std::array<float, 3>{ -7.0f, 8.0f, 9.0f },
                    "resource authoring: IJIM save/load retains RGBA and offset fields");
            }
            catch (const std::exception&)
            {
                check(false, "resource authoring: fixed IJIM SPR vector remains parseable after Save");
            }
        }
    }
}

void checkSpriteOnlyPreviewAcrossProjectsAndVisibility()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource authoring: temporary sprite-only profile directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path root(temporaryDirectory.path().toStdWString());
    const QByteArray materialFixture =
        makeSolidRgbaMatFixture(QColor(240, 24, 32));
    const QByteArray paletteFixture =
        fixtures::makeClassicCmpFixture(6u, QColor(240, 24, 32));
    const std::filesystem::path spritePath = root / "sprite_only.spr";
    constexpr std::string_view spriteText =
        "fx.mat 0 0.5 3.25 4 3 0 0 0 0 0\n";
    check(writeFixtureBytes(root / "fx.mat", materialFixture)
            && writeFixtureBytes(root / "editor_preview_master.cmp", paletteFixture)
            && writeBytes(spritePath, spriteText),
        "resource authoring: sprite-only MAT, CMP, and SPR fixtures are available");

    auto sourceResources = std::make_shared<assets::ResourceLocator>();
    sourceResources->addRoot(root);
    const std::shared_ptr<const assets::ResourceLocator> resources = sourceResources;

    constexpr std::array<ProjectType, 3> projectTypes{
        ProjectType::JKDF2,
        ProjectType::MOTS,
        ProjectType::IJIM,
    };
    for ( const ProjectType projectType : projectTypes )
    {
        auto context = testContext(projectType);
        context.resources = resources;
        auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
            nullptr,
            ui::resources::ResourceAuthoringKind::sprite,
            std::move(context)));
        check(editor != nullptr,
            "resource authoring: sprite-only profile editor is created");
        if ( !editor )
        {
            continue;
        }

        openResourceFile(editor.get(), spritePath);
        auto* const preview = editor->findChild<GlView*>(
            QStringLiteral("spr.preview.gl_view"));
        check(preview != nullptr,
            "resource authoring: sprite-only profile exposes its GlView");
        if ( !preview )
        {
            continue;
        }

        editor->resize(1180, 760);
        editor->show();
        QImage frame{};
        const bool spriteRendered = waitUntil([&]()
            {
                if ( !preview->property("spr.preview.serialized_resource")
                        .toString()
                        .contains(QStringLiteral("fx.mat 0 0.5 3.25")) )
                {
                    return false;
                }
                frame = preview->grabFramebuffer();
                return hasRedSpritePixels(frame);
            });
        check(spriteRendered,
            projectType == ProjectType::JKDF2
                ? "resource authoring: JKDF2 sprite-only scene renders its draft sprite"
                : (projectType == ProjectType::MOTS
                    ? "resource authoring: MOTS sprite-only scene renders its draft sprite"
                    : "resource authoring: IJIM sprite-only scene renders its draft sprite"));

        const auto* const previewLevel = preview->attachedLevelForTest();
        check(previewLevel
                && previewLevel->things.size() == 1u
                && previewLevel->sectors.size() == 1u
                && previewLevel->sectors.front().surfaces.empty(),
            "resource authoring: profile preview fixture contains only its sprite Thing");

        preview->setShowThings(false);
        const bool hidden = waitUntil([&]()
            {
                frame = preview->grabFramebuffer();
                return !hasRedSpritePixels(frame);
            });
        check(hidden,
            projectType == ProjectType::JKDF2
                ? "resource authoring: hidden JKDF2 Things suppress sprite-only solid rendering"
                : (projectType == ProjectType::MOTS
                    ? "resource authoring: hidden MOTS Things suppress sprite-only solid rendering"
                    : "resource authoring: hidden IJIM Things suppress sprite-only solid rendering"));
        editor->hide();
    }
}

void checkSoundAndStringRoundTrips()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(), "resource authoring: temporary sound/string fixture directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path root(temporaryDirectory.path().toStdWString());
    const std::filesystem::path soundPath = root / "actor.snd";
    constexpr std::string_view soundText =
        "create walk.wav 0x00000020 0.25 1.5 0.75\n"
        "futuremode custom.wav 0xDEADBEEF 0.5 2.25 3.5\n"
        "activate greet.wav\n";
    constexpr std::string_view soundExpected =
        "create walk.wav 0x00000020 0.25 1.5 0.75\n"
        "futuremode custom.wav 0xDEADBEEF 0.5 2.25 3.5\n"
        "activate greet.wav 0x00000040 0.5 2.5 1\n";
    check(writeBytes(soundPath, soundText), "resource authoring: SND fixture writes");
    auto soundEditor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::soundClass,
        testContext()));
    if ( soundEditor )
    {
        openResourceFile(soundEditor.get(), soundPath);
        auto* const rows = soundEditor->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
        auto* const save = button(soundEditor.get(), "resource_authoring.save");
        const QList<QTreeWidgetItem*> alternatives = soundAlternativeItems(rows);
        check(rows && alternatives.size() == 3 && rows->columnCount() == 6,
            "resource authoring: sound-class editor exposes grouped ordered alternatives and all fields");
        if ( alternatives.size() == 3 )
        {
            QTreeWidgetItem* const unknown = soundAlternativeByWav(rows, QStringLiteral("custom.wav"));
            QTreeWidgetItem* const defaulted = soundAlternativeByWav(rows, QStringLiteral("greet.wav"));
            check(unknown && unknown->parent()
                    && unknown->parent()->text(0) == QStringLiteral("futuremode")
                    && unknown->parent()->data(0, Qt::UserRole + 23).toInt() == 0
                    && unknown->text(2).contains(QStringLiteral("DEADBEEF")),
                "resource authoring: unknown sound mode and explicit flags remain editable and visible");
            check(defaulted && defaulted->text(2).contains(QStringLiteral("00000040")),
                "resource authoring: classic omitted sound flags use the per-row engine default");
        }
        if ( save )
        {
            save->click();
            check(readBytes(soundPath) == soundExpected,
                "resource authoring: Save writes every sound field and preserves row order in fixed output");
            try
            {
                std::istringstream savedInput{ readBytes(soundPath) };
                const assets::SoundClassFile saved = assets::loadFromSoundClass(
                    savedInput,
                    assets::SoundClassProfile::classic);
                check(saved.entries.size() == 3
                        && saved.entries[1].mode == "futuremode"
                        && saved.entries[1].playFlags == 0xDEADBEEFU
                        && saved.entries[2].playFlags == 0x40U,
                    "resource authoring: sound-class save/load retains unknown modes, explicit flags, and classic defaults");
            }
            catch (const std::exception&)
            {
                check(false, "resource authoring: fixed sound-class vector remains parseable after Save");
            }
        }

        if ( rows )
        {
            QTreeWidgetItem* const first = soundAlternativeByWav(rows, QStringLiteral("walk.wav"));
            if ( first )
            {
                const QString before = first->text(1);
                bool pickerOpened = false;
                runWithModalDriver(
                    [&]()
                    {
                        (void)QMetaObject::invokeMethod(
                            rows,
                            "itemDoubleClicked",
                            Qt::DirectConnection,
                            Q_ARG(QTreeWidgetItem*, first),
                            Q_ARG(int, 1));
                    },
                    std::nullopt,
                    std::nullopt,
                    false,
                    {},
                    [&](QWidget* dialog)
                    {
                        pickerOpened = dialog != nullptr;
                        if ( auto* const picker = qobject_cast<QDialog*>(dialog) )
                        {
                            picker->reject();
                        }
                    });
                check(pickerOpened && first->text(1) == before,
                    "resource authoring: cancelling the WAV picker preserves the sound-class token");
            }
            else
            {
                check(false, "resource authoring: first sound alternative is available for WAV picker");
            }
        }
    }

    const std::filesystem::path stringsPath = root / "cogstrings.uni";
    constexpr std::string_view stringsText = "MSGS 1\n\n\"HELLO\" 17 \"caf\xE9\"\n\nEND\n";
    constexpr std::string_view stringsExpected = "MSGS 1\n\n\"HELLO\" 17 \"caf\xE9\"\n\nEND\n";
    const QString expectedInvalidTextSaveStatus = QStringLiteral(
        "Resource validation failed: String-table row 1 must use game-byte text and a signed decimal legacy number.");
    const QString expectedDuplicateKeySaveStatus = QStringLiteral(
        "Resource validation failed: UNI writer: duplicate message key HELLO");
    check(writeBytes(stringsPath, stringsText), "resource authoring: UNI Latin-1 fixture writes");
    auto stringsEditor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext()));
    if ( stringsEditor )
    {
        openResourceFile(stringsEditor.get(), stringsPath);
        auto* const rows = stringsEditor->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
        auto* const search = stringsEditor->findChild<QLineEdit*>(QStringLiteral("uni.search"));
        auto* const status = stringsEditor->findChild<QLabel*>(
            QStringLiteral("resource_authoring.status"));
        auto* const save = button(stringsEditor.get(), "resource_authoring.save");
        check(rows && rows->rowCount() == 1 && rows->columnCount() == 3,
            "resource authoring: string-table editor exposes key, text, and retained metadata");
        if ( rows && rows->rowCount() == 1 )
        {
            check(rows->item(0, 0) && rows->item(0, 1) && rows->item(0, 2)
                    && rows->item(0, 0)->text() == QStringLiteral("HELLO")
                    && rows->item(0, 1)->text() == QString::fromLatin1("caf\xE9")
                    && rows->item(0, 2)->text() == QStringLiteral("17"),
                "resource authoring: UNI rows decode game bytes as reversible Latin-1 and retain metadata");
        }
        if ( search && rows )
        {
            search->setText(QStringLiteral("missing"));
            check(rows->isRowHidden(0), "resource authoring: UNI search filters rows without removing data");
            search->clear();
            check(!rows->isRowHidden(0), "resource authoring: clearing UNI search restores the row");
        }
        if ( save )
        {
            save->click();
            check(readBytes(stringsPath) == stringsExpected,
                "resource authoring: Save writes exact Latin-1 UNI bytes and preserves the metadata field");
            try
            {
                const assets::StringTableFile saved = assets::loadFromStringTableUni(
                    stringsPath,
                    assets::UniProfile::classic);
                check(saved.entries.size() == 1 && saved.entries[0].key == "HELLO"
                        && saved.entries[0].value == std::string("caf\xE9")
                        && saved.entries[0].numericValue == 17,
                    "resource authoring: UNI save/load preserves key, Latin-1 text, and unused number");
            }
            catch (const std::exception&)
            {
                check(false, "resource authoring: fixed UNI vector remains parseable after Save");
            }
        }

        if ( rows && rows->rowCount() == 1 )
        {
            const QString unsupported = QString::fromUtf8("unsupported \xF0\x9F\x8C\x8B");
            rows->item(0, 1)->setText(unsupported);
            const QCborArray rawRows = ui::resources::captureResourceAuthoringPaneState(
                stringsEditor.get()).value(QStringLiteral("rows")).toArray();
            check(rows->item(0, 1)->text() == unsupported
                    && rawRows.size() == 1
                    && rawRows.at(0).toArray().at(1).toString() == unsupported,
                "resource authoring: unsupported Unicode stays in the raw draft until Save");
            check(status
                    && status->text()
                        == QStringLiteral("Text must use characters in the game's one-byte encoding."),
                "resource authoring: unsupported Unicode reports the single-byte text limit");
            if ( save )
            {
                runWithModalDriver([save]() { save->click(); }, std::nullopt, QMessageBox::Ok);
                check(readBytes(stringsPath) == stringsExpected
                        && status
                        && status->text() == expectedInvalidTextSaveStatus,
                    "resource authoring: failed Unicode Save preserves the exact existing file");
            }
            else
            {
                check(false, "resource authoring: Unicode Save control is available");
            }

            rows->item(0, 1)->setText(QString::fromLatin1("caf\xE9"));
        }

        if ( rows && button(stringsEditor.get(), "uni.add") && save )
        {
            button(stringsEditor.get(), "uni.add")->click();
            if ( rows->rowCount() == 2 )
            {
                rows->setCurrentCell(1, 1);
                if ( rows->item(1, 0) )
                {
                    rows->item(1, 0)->setText(QStringLiteral("HELLO"));
                }
                else
                {
                    check(false, "resource authoring: new UNI rows contain editable key cells");
                    return;
                }
                runWithModalDriver([save]() { save->click(); }, std::nullopt, QMessageBox::Ok);
                check(readBytes(stringsPath) == stringsExpected,
                    "resource authoring: duplicate UNI keys fail without replacing the existing file");
                check(status && status->text() == expectedDuplicateKeySaveStatus,
                    "resource authoring: duplicate-key validation is shown in the editor status");
            }
            else
            {
                check(false, "resource authoring: Add inserts a new editable UNI row");
            }
        }
    }
}

void checkDirtyCancelAndOpenFailurePreserveDraft()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(), "resource authoring: temporary dirty-state fixture directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path root(temporaryDirectory.path().toStdWString());
    const std::filesystem::path validPath = root / "current.spr";
    const std::filesystem::path invalidPath = root / "invalid.spr";
    check(writeBytes(validPath, "current.mat 0 1 1 4 3 0 0 0 0 0\n"),
        "resource authoring: dirty-state SPR fixture writes");
    check(writeBytes(invalidPath, "broken.mat 0 not-a-number\n"),
        "resource authoring: invalid SPR fixture writes");

    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext()));
    if ( !editor )
    {
        return;
    }

    openResourceFile(editor.get(), validPath);
    auto* const material = editor->findChild<QLineEdit*>(QStringLiteral("spr.material"));
    auto* const newButton = button(editor.get(), "resource_authoring.new");
    check(material && newButton, "resource authoring: dirty-state controls are available");
    if ( !material || !newButton )
    {
        return;
    }

    material->setText(QStringLiteral("draft.mat"));
    runWithModalDriver([newButton]() { newButton->click(); }, std::nullopt, QMessageBox::Cancel);
    check(material->text() == QStringLiteral("draft.mat"),
        "resource authoring: cancelling New leaves the exact draft unchanged");

    editor->show();
    bool closed = false;
    runWithModalDriver([&]() { closed = editor->close(); }, std::nullopt, QMessageBox::Cancel);
    check(!closed && editor->isVisible() && material->text() == QStringLiteral("draft.mat"),
        "resource authoring: cancelling dirty close keeps the window and draft open");

    // Opening an invalid source is a non-destructive failure, even when there
    // is already an edited draft in the document.
    if ( QAbstractButton* const open = button(editor.get(), "resource_authoring.open") )
    {
        runWithModalDriver([open]() { open->click(); }, invalidPath, QMessageBox::Ok);
        check(material->text() == QStringLiteral("draft.mat"),
            "resource authoring: failed Open preserves the current draft");
    }
    else
    {
        check(false, "resource authoring: Open control remains available after a cancelled close");
    }
}

void checkResourceAuthoringToolbarUsesThemeIcons()
{
    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext(ProjectType::JKDF2)));
    check(editor != nullptr,
        "resource authoring toolbar: isolated Sprite editor is available");
    if ( !editor )
    {
        return;
    }

    const std::array<std::pair<QString, QString>, 3> expected{
        std::pair{ QStringLiteral("resource_authoring.new"), QStringLiteral("New resource") },
        std::pair{
            QStringLiteral("resource_authoring.open"),
            QStringLiteral("Open resource from the active project") },
        std::pair{ QStringLiteral("resource_authoring.save"), QStringLiteral("Save resource") },
    };
    int commonActionCount = 0;
    bool controlsMatch = true;
    for ( QToolButton* const button : editor->findChildren<QToolButton*>() )
    {
        if ( !button->objectName().startsWith(QStringLiteral("resource_authoring.")) )
        {
            continue;
        }
        ++commonActionCount;
        const auto found = std::find_if(expected.begin(), expected.end(), [button](const auto& item)
            {
                return button->objectName() == item.first;
            });
        if ( found == expected.end() )
        {
            controlsMatch = false;
            continue;
        }
        const QPixmap icon = button->icon().pixmap(QSize(20, 20));
        controlsMatch = controlsMatch
            && button->toolButtonStyle() == Qt::ToolButtonIconOnly
            && button->iconSize() == QSize(20, 20)
            && !button->icon().isNull()
            && !icon.isNull()
            && button->toolTip() == found->second
            && button->accessibleName() == found->second
            && button->size() == QSize(30, 30);
    }
    check(commonActionCount == 3 && controlsMatch,
        "resource authoring toolbar: exactly three icon-only themed New, Open, and Save actions have stable accessible labels");
}

void checkFreshOpenAndRejectedCloseBehavior()
{
    auto freshEditor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext()));
    if ( !freshEditor )
    {
        check(false, "resource authoring: fresh editor exists for Open dialog mode check");
        return;
    }

    bool sawOpenChooser = false;
    bool chooserUsesOpenMode = false;
    bool chooserRequiresExistingFile = false;
    if ( QAbstractButton* const open = button(freshEditor.get(), "resource_authoring.open") )
    {
        runWithModalDriver(
            [open]() { open->click(); },
            std::nullopt,
            std::nullopt,
            false,
            [&](QFileDialog* dialog)
            {
                sawOpenChooser = true;
                chooserUsesOpenMode = dialog->acceptMode() == QFileDialog::AcceptOpen;
                chooserRequiresExistingFile = dialog->fileMode() == QFileDialog::ExistingFile;
                dialog->reject();
            });
    }
    else
    {
        check(false, "resource authoring: fresh editor exposes Open");
    }
    check(sawOpenChooser, "resource authoring: fresh Open displays a file chooser");
    check(chooserUsesOpenMode && chooserRequiresExistingFile,
        "resource authoring: fresh Open uses an existing-file Open chooser");

    const auto verifyDirtyCancel = [](const bool useEscape)
    {
        auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
            nullptr,
            ui::resources::ResourceAuthoringKind::sprite,
            testContext()));
        if ( !editor )
        {
            check(false, "resource authoring: dirty editor exists for rejected-close check");
            return;
        }
        auto* const width = editor->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"));
        if ( !width )
        {
            check(false, "resource authoring: dirty editor exposes sprite width");
            return;
        }
        setScrubValue(width, 1.25);
        editor->show();
        QApplication::processEvents(QEventLoop::AllEvents);

        runWithModalDriver(
            [editor = editor.get(), useEscape]()
            {
                if ( useEscape )
                {
                    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                    QApplication::sendEvent(editor, &escape);
                }
                else
                {
                    editor->reject();
                }
            },
            std::nullopt,
            QMessageBox::Cancel);

        check(editor->isVisible() && width->value() == 1.25,
            useEscape
                ? "resource authoring: Cancel on Escape keeps the dirty window and draft unchanged"
                : "resource authoring: Cancel on reject keeps the dirty window and draft unchanged");
        editor->hide();
    };
    verifyDirtyCancel(true);
    verifyDirtyCancel(false);

    auto enterEditor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext()));
    if ( enterEditor )
    {
        auto* const width = enterEditor->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"));
        if ( width )
        {
            setScrubValue(width, 1.5);
            enterEditor->show();
            width->setFocus();
            QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(width, &enter);
            QKeyEvent enterRelease(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(width, &enterRelease);
            QApplication::processEvents(QEventLoop::AllEvents);
            check(enterEditor->isVisible() && width->value() == 1.5,
                "resource authoring: Enter commits a field without accepting the editor");
            enterEditor->hide();
        }
        else
        {
            check(false, "resource authoring: Enter test can focus the sprite width control");
        }
    }
    else
    {
        check(false, "resource authoring: editor exists for Enter behavior check");
    }
}

void checkInvalidSoundEditIsNotDiscarded()
{
    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::soundClass,
        testContext()));
    if ( !editor )
    {
        check(false, "resource authoring: sound-class editor exists for invalid-cell check");
        return;
    }
    auto* const rows = editor->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    auto* const status = editor->findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
    if ( !rows || !status )
    {
        check(false, "resource authoring: sound-class invalid-cell controls are available");
        return;
    }

    if ( !addSoundMode(editor.get(), rows, QStringLiteral("create"))
        || !addSoundAlternative(rows, QStringLiteral("create")) )
    {
        check(false, "resource authoring: a sound mode and alternative can be added");
        return;
    }
    QTreeWidgetItem* const first = soundAlternativeItems(rows).value(0);
    if ( !first )
    {
        check(false, "resource authoring: new sound row has an editable minimum radius");
        return;
    }
    first->setText(3, QStringLiteral("not-finite"));
    check(status->text().contains(QStringLiteral("finite"), Qt::CaseInsensitive),
        "resource authoring: invalid sound numeric input keeps its validation message visible");

    const QCborArray rawBeforeAppend = ui::resources::captureResourceAuthoringPaneState(
        editor.get()).value(QStringLiteral("rows")).toArray();
    const bool appended = addSoundAlternative(rows, QStringLiteral("create"));
    const QList<QTreeWidgetItem*> updatedRows = soundAlternativeItems(rows);
    const QCborArray rawAfterAppend = ui::resources::captureResourceAuthoringPaneState(
        editor.get()).value(QStringLiteral("rows")).toArray();
    bool priorRowsRetained = rawAfterAppend.size() == rawBeforeAppend.size() + 1;
    for ( int index = 0; priorRowsRetained && index < rawBeforeAppend.size(); ++index )
    {
        priorRowsRetained = rawAfterAppend.at(index) == rawBeforeAppend.at(index);
    }
    const bool retainedInvalidCell = appended && priorRowsRetained
        && updatedRows.size() == rawBeforeAppend.size() + 1
        && updatedRows.at(0)->text(3) == QStringLiteral("not-finite");
    check(retainedInvalidCell,
        "resource authoring: adding a sound row cannot silently erase an invalid numeric draft");
}

void checkInvalidStringNumberEditIsNotDiscarded()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource authoring: invalid-number Save fixture directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path rejectedSavePath =
        std::filesystem::path(temporaryDirectory.path().toStdWString()) / "invalid.uni";
    const QString expectedInvalidNumberSaveStatus = QStringLiteral(
        "Resource validation failed: String-table row 1 must use game-byte text and a signed decimal legacy number.");
    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext()));
    if ( !editor )
    {
        check(false, "resource authoring: string-table editor exists for invalid-number check");
        return;
    }
    auto* const rows = editor->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    auto* const status = editor->findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
    QAbstractButton* const add = button(editor.get(), "uni.add");
    if ( !rows || !status || !add )
    {
        check(false, "resource authoring: string-table invalid-number controls are available");
        return;
    }

    QAbstractButton* const save = button(editor.get(), "resource_authoring.save");
    add->click();
    if ( !rows->item(0, 2) )
    {
        check(false, "resource authoring: new string row has a legacy-number cell");
        return;
    }
    rows->setCurrentCell(0, 2);
    rows->item(0, 2)->setText(QStringLiteral("17x"));
    check(status->text() == QStringLiteral("The legacy number must be a signed decimal integer."),
        "resource authoring: invalid UNI metadata keeps its validation message visible");

    add->click();
    check(rows->rowCount() == 2 && rows->item(0, 2) && rows->item(1, 2)
            && rows->item(0, 2)->text() == QStringLiteral("17x")
            && rows->item(1, 2)->text() == QStringLiteral("0"),
        "resource authoring: Add appends a row without replacing invalid metadata text");
    if ( save )
    {
        runWithModalDriver([save]() { save->click(); }, std::nullopt, QMessageBox::Ok);
        check(!std::filesystem::exists(rejectedSavePath)
                && status->text() == expectedInvalidNumberSaveStatus,
            "resource authoring: invalid metadata Save fails without creating a resource file");
    }
    else
    {
        check(false, "resource authoring: Save control is available for invalid metadata check");
    }
}

void checkSoundFlagsAcceptEitherHexPrefixCase()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource authoring: temporary sound flag fixture directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path sourcePath =
        std::filesystem::path(temporaryDirectory.path().toStdWString()) / "source.snd";
    constexpr std::string_view sourceText =
        "create first.wav 0x00000040 0.5 2.5 1\n"
        "create second.wav 0x00000040 0.5 2.5 1\n";
    check(writeBytes(sourcePath, sourceText),
        "resource authoring: fixed source with two sound alternatives writes");

    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::soundClass,
        testContext()));
    if ( !editor )
    {
        check(false, "resource authoring: sound-class editor exists for hexadecimal prefix check");
        return;
    }
    openResourceFile(editor.get(), sourcePath);
    auto* const rows = editor->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    QList<QTreeWidgetItem*> alternatives = soundAlternativeItems(rows);
    if ( !rows || alternatives.size() != 2 )
    {
        check(false, "resource authoring: two grouped sound rows exist for hexadecimal prefix check");
        return;
    }

    alternatives.at(0)->setText(2, QStringLiteral("0xabcdef01"));
    alternatives.at(1)->setText(2, QStringLiteral("0XAbCdEf01"));

    const std::filesystem::path outputPath =
        std::filesystem::path(temporaryDirectory.path().toStdWString()) / "flags.snd";
    saveAsResourceFile(editor.get(), outputPath);
    constexpr std::string_view expected =
        "create first.wav 0xABCDEF01 0.5 2.5 1\n"
        "create second.wav 0xABCDEF01 0.5 2.5 1\n";
    check(readBytes(outputPath) == expected,
        "resource authoring: lower- and upper-case hexadecimal prefixes save identically");
}

bool addInvalidStringNumber(QWidget* const editor)
{
    auto* const rows = editor->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    auto* const add = button(editor, "uni.add");
    if ( !rows || !add )
    {
        std::cerr << "invalid UNI fixture diagnostic: editor=" << static_cast<const void*>(editor)
                  << " table=" << static_cast<const void*>(rows)
                  << " add=" << static_cast<const void*>(add)
                  << " rowCount=" << (rows ? rows->rowCount() : -1) << '\n';
        return false;
    }

    add->click();
    if ( rows->rowCount() != 1 || !rows->item(0, 2) )
    {
        std::cerr << "invalid UNI fixture diagnostic: editor=" << static_cast<const void*>(editor)
                  << " table=" << static_cast<const void*>(rows)
                  << " add=" << static_cast<const void*>(add)
                  << " rowCount=" << (rows ? rows->rowCount() : -1) << '\n';
        return false;
    }

    rows->item(0, 2)->setText(QStringLiteral("17x"));
    return rows->item(0, 2)->text() == QStringLiteral("17x");
}

QDockWidget* resourceEditorsDock(MainWindow& window,
    const ui::resources::ResourceAuthoringKind kind = ui::resources::ResourceAuthoringKind::sprite)
{
    const QString name = kind == ui::resources::ResourceAuthoringKind::stringTable
        ? QStringLiteral("uniResourceEditorsDock")
        : kind == ui::resources::ResourceAuthoringKind::soundClass
            ? QStringLiteral("soundClassResourceEditorsDock")
            : QStringLiteral("resourceEditorsDock");
    return window.findChild<QDockWidget*>(name);
}

void createResourceDraft(MainWindow& window, const ui::resources::ResourceAuthoringKind kind)
{
    QDockWidget* const dock = resourceEditorsDock(window, kind);
    QAbstractButton* create = nullptr;
    for ( QAbstractButton* const candidate : dock
            ? dock->findChildren<QAbstractButton*>(QStringLiteral("resource_authoring.new"))
            : QList<QAbstractButton*>{} )
    {
        if ( !candidate->isHidden() )
        {
            create = candidate;
            break;
        }
    }
    check(create, "resource workspace fixture: explicit group New control is available");
    if ( create )
    {
        create->click();
    }
}

void checkResourceEditorsDockAndRawDraftRecovery()
{
    MainWindow window(nullptr, true);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    QAction* const stringTableAction = window.findChild<QAction*>(
        QStringLiteral("edit.resources.string_table_uni"));
    check(stringTableAction != nullptr,
        "resource workspace: existing string-table menu command is available to trigger");
    if ( !stringTableAction )
    {
        return;
    }

    createResourceDraft(window, ui::resources::ResourceAuthoringKind::stringTable);

    QDockWidget* const dock = resourceEditorsDock(window, ui::resources::ResourceAuthoringKind::stringTable);
    check(dock != nullptr,
        "resource workspace: String Table opens inside the shared Resource Editors dock");
    QByteArray capturedState{};
    bool capturedInvalidDraft = false;
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"));
    if ( dock && tabs && tabs->currentWidget() )
    {
        capturedInvalidDraft = addInvalidStringNumber(tabs->currentWidget());
        capturedState = window.captureRecoveryEditorStateForTest();
    }
    check(capturedInvalidDraft,
        "resource workspace: invalid intermediate UNI number remains an editable draft");

    const QCborValue captured = QCborValue::fromCbor(capturedState);
    check(captured.isMap()
            && captured.toMap().contains(QStringLiteral("resourceEditors")),
        "resource workspace: EDIT checkpoint contains the optional resourceEditors extension");
    check(capturedState.contains(QByteArrayLiteral("17x")),
        "resource workspace: EDIT checkpoint preserves raw invalid UNI cell text");

    QAction* const spriteAction = window.findChild<QAction*>(
        QStringLiteral("edit.resources.sprite_spr"));
    QAction* const soundAction = window.findChild<QAction*>(
        QStringLiteral("edit.resources.sound_class_snd"));
    if (spriteAction && soundAction)
    {
        createResourceDraft(window, ui::resources::ResourceAuthoringKind::sprite);
        createResourceDraft(window, ui::resources::ResourceAuthoringKind::soundClass);
    }
    const QTabWidget* const sprites = window.findChild<QTabWidget*>(QStringLiteral("resourceEditorTabs"));
    const QTabWidget* const sounds = window.findChild<QTabWidget*>(QStringLiteral("soundClassResourceEditorTabs"));
    const QCborMap workspace = QCborValue::fromCbor(window.captureRecoveryEditorStateForTest())
        .toMap().value(QStringLiteral("resourceEditors")).toMap();
    check(tabs && tabs->count() == 1 && sprites && sprites->count() == 1
            && sounds && sounds->count() == 1
            && workspace.value(QStringLiteral("tabs")).toArray().size() == 3,
        "resource workspace: Sprite, Sound Class, and String Table drafts occupy their own groups and all three remain in EDIT");
    check(window.findChild<QAction*>(QStringLiteral("tools.resource_editors.sprite_spr.new"))
            && window.findChild<QAction*>(QStringLiteral("tools.resource_editors.sound_class_snd.new"))
            && window.findChild<QAction*>(QStringLiteral("tools.resource_editors.string_table_uni.new")),
        "resource workspace: Tools exposes New commands for each resource kind");
}

void checkResourceWorkspaceCapturesLiveTableDelegate()
{
    MainWindow window(nullptr, true);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    QAction* const openTable = window.findChild<QAction*>(
        QStringLiteral("edit.resources.string_table_uni"));
    check(openTable != nullptr, "resource workspace delegate: String Table command is available");
    if ( !openTable )
    {
        return;
    }
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::stringTable);
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"));
    QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
    QTableWidget* const table = page
        ? page->findChild<QTableWidget*>(QStringLiteral("uni.entries")) : nullptr;
    QAbstractButton* const add = page ? button(page, "uni.add") : nullptr;
    if ( !page || !table || !add )
    {
        check(false, "resource workspace delegate: String Table cell editor is available");
        return;
    }
    QApplication::processEvents();
    add->click();
    if ( !table->item(0, 1) )
    {
        check(false, "resource workspace delegate: new row has a text cell");
        return;
    }
    table->item(0, 1)->setText(QStringLiteral("0"));
    QCborMap cleanBaseline = ui::resources::captureResourceAuthoringPaneState(page);
    cleanBaseline.remove(QStringLiteral("history")); // The authored setup row defines a new clean baseline.
    cleanBaseline.insert(QStringLiteral("modified"), false);
    QString restoreWarning{};
    const bool restoredClean = ui::resources::restoreResourceAuthoringPaneState(
        page, cleanBaseline, &restoreWarning);
    check(restoredClean && !ui::resources::resourceAuthoringPaneIsModified(page),
        "resource workspace delegate: setup restores a clean raw-row baseline");
    if ( !restoredClean )
    {
        return;
    }
    const bool wasModified = ui::resources::resourceAuthoringPaneIsModified(page);
    QTableWidgetItem* const textCell = table->item(0, 1);
    if ( !textCell )
    {
        check(false, "resource workspace delegate: restored row retains its text cell");
        return;
    }
    table->setCurrentCell(0, 1);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
    table->editItem(textCell);
    QApplication::processEvents();
    QPointer<QLineEdit> liveEditor = qobject_cast<QLineEdit*>(
        table->indexWidget(table->model()->index(0, 1)));
    if ( !liveEditor || liveEditor->text() != QStringLiteral("0") )
    {
        const QModelIndex textIndex = table->model()->index(0, 1);
        QWidget* const indexedEditor = table->indexWidget(textIndex);
        std::cerr << "resource delegate fixture: rows=" << table->rowCount()
                  << " currentRow=" << table->currentRow()
                  << " currentColumn=" << table->currentColumn()
                  << " textValue=" << table->item(0, 1)->text().toStdString()
                  << " indexedEditor=" << static_cast<const void*>(indexedEditor)
                  << " indexedEditorClass="
                  << (indexedEditor ? indexedEditor->metaObject()->className() : "<null>")
                  << " focus=" << static_cast<const void*>(QApplication::focusWidget())
                  << " tableVisible=" << table->isVisible() << '\n';
        check(false, "resource workspace delegate: text cell opens its own live line editor with initial text 0");
        return;
    }

    liveEditor->setText(QString{});
    typeIntoLineEdit(liveEditor, QStringLiteral("29x"));
    const QPointer<QWidget> focusBeforeCapture = QApplication::focusWidget();
    const bool editorHadFocusBeforeCapture = focusBeforeCapture.data() == liveEditor.data();
    liveEditor->setSelection(1, 1);
    const int cursorBeforeCapture = liveEditor->cursorPosition();
    const int selectionBeforeCapture = liveEditor->selectionStart();
    const QString selectedBeforeCapture = liveEditor->selectedText();
    const bool undoBeforeCapture = liveEditor->isUndoAvailable();
    const auto editorIsUnchanged = [&]()
        {
            return liveEditor
                && QApplication::focusWidget() == focusBeforeCapture.data()
                && liveEditor->text() == QStringLiteral("29x")
                && liveEditor->cursorPosition() == cursorBeforeCapture
                && liveEditor->selectionStart() == selectionBeforeCapture
                && liveEditor->selectedText() == selectedBeforeCapture
                && liveEditor->isUndoAvailable() == undoBeforeCapture;
        };
    const bool modifiedBeforeCapture = ui::resources::resourceAuthoringPaneIsModified(page);

    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(page);
    const bool unchangedAfterPaneCapture = editorIsUnchanged();
    const QCborArray rows = captured.value(QStringLiteral("rows")).toArray();
    const QCborArray cells = rows.isEmpty() ? QCborArray{} : rows.at(0).toArray();
    check(cells.size() > 2 && cells.at(1).toString() == QStringLiteral("29x") && cells.at(2).toString() == QStringLiteral("0"),
        "resource workspace delegate: EDIT captures raw text before cell delegate commit");
    const QCborMap capturedAgain = ui::resources::captureResourceAuthoringPaneState(page);
    const bool unchangedAfterSecondCapture = editorIsUnchanged();
    const QCborArray rowsAgain = capturedAgain.value(QStringLiteral("rows")).toArray();
    const QCborArray cellsAgain = rowsAgain.isEmpty() ? QCborArray{} : rowsAgain.at(0).toArray();
    const QByteArray recoveryCapture = window.captureRecoveryEditorStateForTest();
    const bool unchangedAfterRecoveryCapture = editorIsUnchanged();
    check(!wasModified && modifiedBeforeCapture,
        "resource workspace delegate: user typing makes a clean draft modified before any state capture");
    check(!editorHadFocusBeforeCapture,
        "resource workspace delegate: hidden capture fixture starts with no focus in the active editor");
    check(unchangedAfterPaneCapture,
        "resource workspace delegate: first EDIT capture preserves non-focused editor text and navigation");
    check(unchangedAfterSecondCapture,
        "resource workspace delegate: repeated EDIT capture preserves non-focused editor text and navigation");
    check(unchangedAfterRecoveryCapture,
        "resource workspace delegate: recovery capture preserves non-focused editor text and navigation");
    check(cellsAgain.size() > 2 && cellsAgain.at(1).toString() == QStringLiteral("29x") && cellsAgain.at(2).toString() == QStringLiteral("0")
            && recoveryCapture.contains(QByteArrayLiteral("29x")),
        "resource workspace delegate: EDIT and recovery snapshots preserve raw 29x");

    liveEditor->selectAll();
    QKeyEvent revertHiddenEditor(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(liveEditor, &revertHiddenEditor);
    const QModelIndex hiddenTextIndex = table->model()->index(0, 1);
    const bool hiddenEditorReverted = liveEditor
        && table->indexWidget(hiddenTextIndex) == nullptr
        && table->item(0, 1)->text() == QStringLiteral("0");
    check(hiddenEditorReverted,
        "resource workspace delegate: hidden capture editor is explicitly reverted before the active-focus case");
    if ( !hiddenEditorReverted )
    {
        return;
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();

    window.setAnimated(false);
    if ( QMainWindow* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow")) )
    {
        workspace->setAnimated(false);
    }
    window.resize(920, 680);
    window.show();
    if ( QDockWidget* const dock = resourceEditorsDock(window, ui::resources::ResourceAuthoringKind::stringTable) )
    {
        dock->show();
    }
    if ( tabs )
    {
        tabs->setCurrentWidget(page);
    }
    window.raise();
    window.activateWindow();
    QApplication::processEvents();
    const bool activePageReady = waitUntil([&]()
        {
            return window.isVisible() && table->isVisible()
                && table->width() > 0 && table->height() > 0;
        });
    check(activePageReady,
        "resource workspace delegate: MainWindow and active resource table settle before focus is requested");
    const bool resourcePaneRemainsEmbedded = !page->isWindow() && page->window() == &window;
    check(resourcePaneRemainsEmbedded,
        "resource workspace delegate: polished pane remains a child of the MainWindow dock");
    if ( !activePageReady || !resourcePaneRemainsEmbedded )
    {
        if ( !resourcePaneRemainsEmbedded )
        {
            const QWidget* const pageWindow = page->window();
            std::cerr << "resource pane attachment diagnostic: pane="
                      << static_cast<const void*>(page)
                      << " paneIsWindow=" << page->isWindow()
                      << " paneFlags=" << static_cast<int>(page->windowFlags())
                      << " paneWindow=" << static_cast<const void*>(pageWindow)
                      << " paneWindowClass="
                      << (pageWindow ? pageWindow->metaObject()->className() : "<null>")
                      << " paneWindowName="
                      << (pageWindow ? pageWindow->objectName().toStdString() : "<null>")
                      << " mainWindow=" << static_cast<const void*>(&window) << '\n';
        }
        window.hide();
        return;
    }
    table->item(0, 1)->setText(QStringLiteral("0"));
    table->setCurrentCell(0, 1);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
    table->editItem(table->item(0, 1));
    QApplication::processEvents();
    QPointer<QLineEdit> activeEditor = qobject_cast<QLineEdit*>(
        table->indexWidget(table->model()->index(0, 1)));
    if ( !activeEditor || activeEditor->text() != QStringLiteral("0") )
    {
        check(false,
            "resource workspace delegate: fresh visible text editor is available with initial text 0");
        window.hide();
        return;
    }
    bool activeEditorPreserved = false;
    activeEditor->setFocus(Qt::OtherFocusReason);
    const bool editorObtainedFocus = waitUntil(
        [&]() { return QApplication::focusWidget() == activeEditor.data(); });
    check(editorObtainedFocus,
        "resource workspace delegate: active capture fixture gives the text editor real focus");
    if ( !editorObtainedFocus )
    {
        QWidget* const focused = QApplication::focusWidget();
        QDockWidget* const dock = resourceEditorsDock(window, ui::resources::ResourceAuthoringKind::stringTable);
        std::cerr << "resource active-editor focus diagnostic: mainVisible=" << window.isVisible()
                  << " mainActive=" << window.isActiveWindow()
                  << " dockVisible=" << (dock && dock->isVisible())
                  << " tableVisible=" << table->isVisible()
                  << " editor=" << static_cast<const void*>(activeEditor.data())
                  << " editorClass=" << (activeEditor ? activeEditor->metaObject()->className() : "<null>")
                  << " editorText=" << (activeEditor ? activeEditor->text().toStdString() : "<null>")
                  << " editorFocus=" << (activeEditor && activeEditor->hasFocus())
                  << " focused=" << static_cast<const void*>(focused)
                  << " focusClass=" << (focused ? focused->metaObject()->className() : "<null>")
                  << " focusName=" << (focused ? focused->objectName().toStdString() : "<null>")
                  << " indexedEditor=" << static_cast<const void*>(
                        table->indexWidget(table->model()->index(0, 1))) << '\n';
    }
    if ( editorObtainedFocus )
    {
        activeEditor->setText(QString{});
        typeIntoLineEdit(activeEditor, QStringLiteral("31x"));
        activeEditor->setSelection(1, 1);
        const int activeCursorBeforeCapture = activeEditor->cursorPosition();
        const int activeSelectionBeforeCapture = activeEditor->selectionStart();
        const QString activeSelectedText = activeEditor->selectedText();
        const bool activeUndoBeforeCapture = activeEditor->isUndoAvailable();
        const QCborMap activeCapture = ui::resources::captureResourceAuthoringPaneState(page);
        const QCborArray activeRows = activeCapture.value(QStringLiteral("rows")).toArray();
        const QCborArray activeCells = activeRows.isEmpty() ? QCborArray{} : activeRows.at(0).toArray();
        activeEditorPreserved = activeEditor
            && QApplication::focusWidget() == activeEditor.data()
            && activeEditor->text() == QStringLiteral("31x")
            && activeEditor->cursorPosition() == activeCursorBeforeCapture
            && activeEditor->selectionStart() == activeSelectionBeforeCapture
            && activeEditor->selectedText() == activeSelectedText
            && activeEditor->isUndoAvailable() == activeUndoBeforeCapture
            && activeCells.size() > 2
            && activeCells.at(1).toString() == QStringLiteral("31x")
            && activeCells.at(2).toString() == QStringLiteral("0");
    }
    if ( editorObtainedFocus )
    {
        check(activeEditorPreserved,
            "resource workspace delegate: EDIT capture preserves active editor focus, cursor, selection, and local undo");
    }
}

void checkResourceWorkspaceSavesLiveTableDelegate()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace live Save: temporary directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path path =
        std::filesystem::path(temporaryDirectory.path().toStdWString()) / "live.uni";
    constexpr std::string_view source = "MSGS 1\n\n\"KEY\" 17 \"0\"\n\nEND\n";
    constexpr std::string_view expected = "MSGS 1\n\n\"KEY\" 17 \"29\"\n\nEND\n";
    check(writeBytes(path, source), "resource workspace live Save: fixed UNI source writes");

    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext()));
    if ( !editor )
    {
        check(false, "resource workspace live Save: String Table pane is available");
        return;
    }
    openResourceFile(editor.get(), path);
    editor->show();
    editor->activateWindow();
    QApplication::processEvents();
    QTableWidget* const table = editor->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    if ( !table || table->rowCount() != 1 || !table->item(0, 1) )
    {
        check(false, "resource workspace live Save: one-row source is open");
        return;
    }

    const QModelIndex textIndex = table->model()->index(0, 1);
    table->setCurrentCell(0, 1);
    table->scrollToItem(table->item(0, 1));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    editor->raise();
    editor->activateWindow();
    table->setFocus(Qt::OtherFocusReason);
    const bool tableFocused = waitUntil([&]()
        {
            return editor->isActiveWindow() && table->hasFocus()
                && table->currentIndex() == textIndex;
        });
    check(tableFocused,
        "resource workspace live Save: the visible text cell owns actual keyboard edit focus");
    if ( !tableFocused )
    {
        return;
    }
    QKeyEvent editPress(QEvent::KeyPress, Qt::Key_F2, Qt::NoModifier);
    QApplication::sendEvent(table, &editPress);
    QKeyEvent editRelease(QEvent::KeyRelease, Qt::Key_F2, Qt::NoModifier);
    QApplication::sendEvent(table, &editRelease);
    QPointer<QLineEdit> liveEditor{};
    (void)waitUntil([&]()
        {
            liveEditor = qobject_cast<QLineEdit*>(table->indexWidget(textIndex));
            return liveEditor && liveEditor->text() == QStringLiteral("0")
                && liveEditor->hasFocus();
        });
    if ( !liveEditor || liveEditor->text() != QStringLiteral("0") || !liveEditor->hasFocus() )
    {
        const QModelIndex textIndex = table->model()->index(0, 1);
        QWidget* const indexedEditor = table->indexWidget(textIndex);
        std::cerr << "resource Save fixture: rows=" << table->rowCount()
                  << " currentRow=" << table->currentRow()
                  << " currentColumn=" << table->currentColumn()
                  << " textValue=" << table->item(0, 1)->text().toStdString()
                  << " indexedEditor=" << static_cast<const void*>(indexedEditor)
                  << " indexedEditorClass="
                  << (indexedEditor ? indexedEditor->metaObject()->className() : "<null>")
                  << " focus=" << static_cast<const void*>(QApplication::focusWidget())
                  << " tableVisible=" << table->isVisible() << '\n';
        check(false, "resource workspace live Save: exact text cell delegate is available with initial text 0");
        return;
    }
    liveEditor->setFocus(Qt::OtherFocusReason);
    liveEditor->setText(QString{});
    typeIntoLineEdit(liveEditor, QStringLiteral("29"));
    const bool modifiedBeforeSave = ui::resources::resourceAuthoringPaneIsModified(editor.get());
    const bool saved = ui::resources::resourceAuthoringPaneSave(editor.get(), false);
    const std::string actual = readBytes(path);
    check(modifiedBeforeSave,
        "resource workspace live Save: active valid text value marks the pane dirty immediately");
    check(saved && std::string_view(actual) == expected,
        "resource workspace live Save: Save writes the exact current delegate value without requiring focus-out");
}

void checkResourceWorkspaceIgnoresClosedTableDelegate()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace closed delegate: temporary directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path path =
        std::filesystem::path(temporaryDirectory.path().toStdWString()) / "closed.uni";
    constexpr std::string_view source = "MSGS 1\n\n\"KEY\" -31 \"0\"\n\nEND\n";
    constexpr std::string_view expected = "MSGS 1\n\n\"KEY\" -31 \"31\"\n\nEND\n";
    check(writeBytes(path, source),
        "resource workspace closed delegate: fixed UNI source writes");

    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext()));
    if ( !editor )
    {
        check(false, "resource workspace closed delegate: String Table pane is available");
        return;
    }
    openResourceFile(editor.get(), path);
    editor->show();
    editor->activateWindow();
    QApplication::processEvents();
    QTableWidget* const table = editor->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    if ( !table || table->rowCount() != 1 || !table->item(0, 1) )
    {
        check(false, "resource workspace closed delegate: one-row source is open");
        return;
    }

    const QModelIndex textIndex = table->model()->index(0, 1);
    table->setCurrentCell(0, 1);
    table->editItem(table->item(0, 1));
    QApplication::processEvents();
    QPointer<QLineEdit> liveEditor = qobject_cast<QLineEdit*>(table->indexWidget(textIndex));
    if ( !liveEditor || liveEditor->text() != QStringLiteral("0") )
    {
        check(false,
            "resource workspace closed delegate: text editor has the exact target cell and initial text 0");
        return;
    }

    liveEditor->setFocus(Qt::OtherFocusReason);
    liveEditor->selectAll();
    typeIntoLineEdit(liveEditor, QStringLiteral("29"));
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(liveEditor, &escape);
    const bool revertedEditorIsDetached = liveEditor
        && table->indexWidget(textIndex) == nullptr
        && table->item(0, 1)->text() == QStringLiteral("0");
    check(revertedEditorIsDetached,
        "resource workspace closed delegate: Escape reverts and detaches the still-live cell editor");
    if ( !revertedEditorIsDetached )
    {
        return;
    }

    table->item(0, 1)->setText(QStringLiteral("31"));
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(editor.get());
    const QCborArray rows = captured.value(QStringLiteral("rows")).toArray();
    const QCborArray cells = rows.isEmpty() ? QCborArray{} : rows.at(0).toArray();
    check(cells.size() > 2 && cells.at(1).toString() == QStringLiteral("31") && cells.at(2).toString() == QStringLiteral("-31"),
        "resource workspace closed delegate: detached editor text cannot override the current raw row");

    const bool saved = ui::resources::resourceAuthoringPaneSave(editor.get(), false);
    const std::string actual = readBytes(path);
    check(saved && std::string_view(actual) == expected,
        "resource workspace closed delegate: explicit Save ignores reverted editor text and writes current row 31");
}

void checkResourceWorkspaceSpriteProfileAndIntegerRestore()
{
    auto source = std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr, ui::resources::ResourceAuthoringKind::sprite, testContext(ProjectType::JKDF2)));
    if ( !source )
    {
        check(false, "resource workspace schema: SPR source pane is available");
        return;
    }
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(source.get());
    state.remove(QStringLiteral("history")); // The unknown type is an independent imported baseline.
    check(state.value(QStringLiteral("profile")).toInteger()
                == static_cast<int>(assets::SprProfile::classic)
            && !source->findChild<QComboBox*>(QStringLiteral("spr.profile")),
        "resource workspace schema: SPR profile follows the active classic project without a selector");
    state.insert(QStringLiteral("profile"), static_cast<int>(assets::SprProfile::ijim));
    QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
    sprite[1] = 17;
    state.insert(QStringLiteral("sprite"), sprite);

    auto target = std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr, ui::resources::ResourceAuthoringKind::sprite, testContext(ProjectType::JKDF2)));
    QString warning{};
    check(target && ui::resources::restoreResourceAuthoringPaneState(target.get(), state, &warning),
        "resource workspace schema: in-range unknown sprite type restores losslessly");
    if ( target )
    {
        const QCborMap restored = ui::resources::captureResourceAuthoringPaneState(target.get());
        const QCborArray restoredSprite = restored.value(QStringLiteral("sprite")).toArray();
        check(restored.value(QStringLiteral("profile")).toInteger()
                == static_cast<int>(assets::SprProfile::classic)
                && !target->findChild<QComboBox*>(QStringLiteral("spr.profile")),
            "resource workspace schema: stale SPR profile metadata cannot change the active project format");
        check(restoredSprite.size() > 1 && restoredSprite.at(1).toInteger() == 17,
            "resource workspace schema: supported unknown sprite type survives restore and recapture");
    }

    sprite[1] = qint64{4294967296LL};
    state.insert(QStringLiteral("sprite"), sprite);
    check(!ui::resources::validateResourceAuthoringPaneState(state, &warning),
        "resource workspace schema: out-of-domain CBOR integer is rejected before int narrowing");
}

void checkResourceWorkspaceActiveSpriteProfileOnOpen()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace SPR profile: temporary profile-crossing directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path root(temporaryDirectory.path().toStdWString());
    const std::filesystem::path ijimSource = root / "rgba.spr";
    const std::filesystem::path classicSource = root / "scalar.spr";
    constexpr std::string_view ijimText =
        "rgba.mat 1 2 3 4 5 6 0.125 0.25 0.375 0.5 -7 8 9\n";
    constexpr std::string_view classicText =
        "scalar.mat 0 2 3.25 4 3 0 0.25 0 0 0\n";
    check(writeBytes(ijimSource, ijimText) && writeBytes(classicSource, classicText),
        "resource workspace SPR profile: fixed classic and IJIM sources are available");

    auto classicProject = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext(ProjectType::JKDF2)));
    check(classicProject != nullptr,
        "resource workspace SPR profile: classic project pane is available");
    if ( classicProject )
    {
        openResourceFile(classicProject.get(), ijimSource);
        const QCborMap state = ui::resources::captureResourceAuthoringPaneState(
            classicProject.get());
        check(state.value(QStringLiteral("profile")).toInteger()
                == static_cast<int>(assets::SprProfile::classic)
                && !classicProject->findChild<QComboBox*>(QStringLiteral("spr.profile")),
            "resource workspace SPR profile: opening IJIM bytes cannot change the active classic output profile");

        bool saved = true;
        runWithModalDriver(
            [&]()
            {
                saved = ui::resources::resourceAuthoringPaneSave(classicProject.get(), false);
            },
            std::nullopt,
            QMessageBox::Ok);
        check(!saved && readBytes(ijimSource) == ijimText,
            "resource workspace SPR profile: classic rejects non-scalar RGBA without changing the source file");
    }

    auto ijimProject = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext(ProjectType::IJIM)));
    check(ijimProject != nullptr,
        "resource workspace SPR profile: IJIM project pane is available");
    if ( ijimProject )
    {
        openResourceFile(ijimProject.get(), classicSource);
        const QCborMap state = ui::resources::captureResourceAuthoringPaneState(
            ijimProject.get());
        check(state.value(QStringLiteral("profile")).toInteger()
                == static_cast<int>(assets::SprProfile::ijim)
                && !ijimProject->findChild<QComboBox*>(QStringLiteral("spr.profile")),
            "resource workspace SPR profile: opening classic bytes keeps IJIM as the active output profile");

        check(ui::resources::resourceAuthoringPaneSave(ijimProject.get(), false),
            "resource workspace SPR profile: representable classic values save through the active IJIM profile");
        std::istringstream savedInput{ readBytes(classicSource) };
        const assets::SprFile saved = assets::loadFromSPR(savedInput);
        std::istringstream savedText{ readBytes(classicSource) };
        std::string outputLine{};
        std::getline(savedText, outputLine);
        std::istringstream fields(outputLine);
        std::vector<std::string> tokens{};
        std::copy(std::istream_iterator<std::string>(fields),
            std::istream_iterator<std::string>(), std::back_inserter(tokens));
        const std::string savedBytes = readBytes(classicSource);
        const std::string expectedText =
            "scalar.mat 0 2 3.25 4 3 0 0.25 0.25 0.25 1 0 0 0\n";
        check(saved.width == 2.0f && saved.height == 3.25f && tokens.size() == 14
                && saved.extraLightRGBA == std::array<float, 4>{ 0.25f, 0.25f, 0.25f, 1.0f }
                && savedBytes == expectedText,
            "resource workspace SPR profile: IJIM Save writes the RGBA record shape while preserving scalar fields");
    }
}

void checkResourceWorkspaceSpritePresentationRoundTrip()
{
    const auto createPane = []()
    {
        return std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
            nullptr,
            ui::resources::ResourceAuthoringKind::sprite,
            testContext(ProjectType::JKDF2)));
    };
    auto source = createPane();
    check(source != nullptr,
        "resource workspace SPR presentation: source pane is available");
    if ( !source )
    {
        return;
    }

    QCborMap cleanSourceState = ui::resources::captureResourceAuthoringPaneState(source.get());
    cleanSourceState.insert(QStringLiteral("modified"), false);
    QString cleanSourceWarning{};
    check(ui::resources::restoreResourceAuthoringPaneState(
              source.get(), cleanSourceState, &cleanSourceWarning),
        "resource workspace SPR presentation: fixture restores a clean authored baseline before presentation edits");
    source->resize(920, 640);
    source->show();
    QApplication::processEvents(QEventLoop::AllEvents);

    QCheckBox* const bounds = source->findChild<QCheckBox*>(QStringLiteral("spr.preview.bounds"));
    QSlider* const fps = source->findChild<QSlider*>(QStringLiteral("spr.preview.fps"));
    QToolButton* const pause = source->findChild<QToolButton*>(QStringLiteral("spr.preview.pause"));
    QToolButton* const stop = source->findChild<QToolButton*>(QStringLiteral("spr.preview.stop"));
    QSplitter* const splitter = source->findChild<QSplitter*>(QStringLiteral("spr.editor_splitter"));
    GlView* const preview = source->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
    check(bounds && fps && fps->minimum() == 0 && fps->maximum() == 99
            && pause && stop && splitter && preview,
        "resource workspace SPR presentation: bounds, FPS, splitter, and isolated camera controls are available");
    if ( !bounds || !fps || !pause || !stop || !splitter || !preview )
    {
        return;
    }

    bounds->setChecked(true);
    fps->setValue(23);
    splitter->setSizes({ 327, 281 });
    GlView::RecoveryState camera = preview->recoveryState();
    camera.preview.orbit.center = QVector3D(3.0f, -2.0f, 5.0f);
    camera.preview.orbit.pan = QVector3D(0.5f, 1.0f, -0.25f);
    camera.preview.orbit.radius = 12.5f;
    camera.preview.orbit.distance = 21.0f;
    camera.preview.orbit.yawDeg = 33.0f;
    camera.preview.orbit.pitchDeg = 18.0f;
    camera.preview.orbit.rollDeg = -4.0f;
    camera.preview.orthoScale = 48.0f;
    preview->restoreRecoveryState(camera, false);

    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(source.get());
    const QCborMap presentation = captured.value(QStringLiteral("presentation")).toMap();
    const QCborMap cameraMap = presentation.value(QStringLiteral("camera")).toMap();
    check(presentation.value(QStringLiteral("version")).toInteger() == 1
            && presentation.value(QStringLiteral("bounds")).toBool()
            && presentation.value(QStringLiteral("fps")).toInteger() == 23
            && presentation.value(QStringLiteral("splitterSizes")).toArray().size() == 2
            && cameraMap.value(QStringLiteral("orbit")).toArray().size() == 11
            && !ui::resources::resourceAuthoringPaneIsModified(source.get()),
        "resource workspace SPR presentation: camera and display settings are captured without dirtying authored content");

    auto target = createPane();
    if ( target )
    {
        target->resize(920, 640);
        target->show();
        QApplication::processEvents(QEventLoop::AllEvents);
    }
    QString warning{};
    const bool restored = target
        && ui::resources::restoreResourceAuthoringPaneState(target.get(), captured, &warning);
    if ( restored )
    {
        QApplication::processEvents(QEventLoop::AllEvents);
    }
    const QCborMap restoredState = restored
        ? ui::resources::captureResourceAuthoringPaneState(target.get()) : QCborMap{};
    const bool presentationRoundTripped = restored
        && restoredState.value(QStringLiteral("presentation")).toMap() == presentation
        && !ui::resources::resourceAuthoringPaneIsModified(target.get());
    if ( !presentationRoundTripped )
    {
        const auto printMapDifferences = [](const char* const label, const QCborMap& before, const QCborMap& after)
        {
            QList<QCborValue> keys = before.keys();
            for ( const QCborValue& key : after.keys() )
            {
                if ( !keys.contains(key) )
                {
                    keys.append(key);
                }
            }
            for ( const QCborValue& key : keys )
            {
                if ( before.value(key) != after.value(key) )
                {
                    std::cerr << "SPR presentation round-trip diagnostic: " << label
                              << " key=" << key.toString().toStdString()
                              << " beforeCbor=" << before.value(key).toCbor().toHex().toStdString()
                              << " afterCbor=" << after.value(key).toCbor().toHex().toStdString() << '\n';
                }
            }
        };
        std::cerr << "SPR presentation round-trip diagnostic: restored=" << restored
                  << " warning=" << warning.toStdString()
                  << " modified=" << (target && ui::resources::resourceAuthoringPaneIsModified(target.get())) << '\n';
        printMapDifferences("pane", captured, restoredState);
        printMapDifferences("presentation", presentation, restoredState.value(QStringLiteral("presentation")).toMap());
    }
    check(presentationRoundTripped,
        "resource workspace SPR presentation: pane restore round-trips bounds, FPS, splitter, and camera without authored edits");

    if ( target )
    {
        QCborMap malformed = captured;
        QCborMap badPresentation = presentation;
        badPresentation.insert(QStringLiteral("fps"), 1000);
        malformed.insert(QStringLiteral("presentation"), badPresentation);
        QString malformedWarning{};
        const QCborMap before = ui::resources::captureResourceAuthoringPaneState(target.get());
        check(!ui::resources::restoreResourceAuthoringPaneState(
                    target.get(), malformed, &malformedWarning)
                && !malformedWarning.isEmpty()
                && ui::resources::captureResourceAuthoringPaneState(target.get()) == before,
            "resource workspace SPR presentation: malformed known fields reject atomically");
    }
}

void checkResourceWorkspacePreservesRawGameBytesAndTinySpriteFloats()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace byte preservation: temporary resource directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path root(temporaryDirectory.path().toStdWString());
    const std::filesystem::path soundPath = root / "latin1.snd";
    constexpr std::string_view soundText =
        "create caf\xE9.wav 0x00000040 0.5 2.5 1\n";
    check(writeBytes(soundPath, soundText),
        "resource workspace byte preservation: fixed Latin-1 SND source writes");
    auto soundEditor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::soundClass,
        testContext()));
    if ( !soundEditor )
    {
        check(false, "resource workspace byte preservation: Sound Class editor is available");
        return;
    }
    openResourceFile(soundEditor.get(), soundPath);
    const bool soundSaved = ui::resources::resourceAuthoringPaneSave(soundEditor.get(), false);
    check(soundSaved && readBytes(soundPath) == soundText,
        "resource workspace byte preservation: explicit no-op SND Save preserves exact fixed 0xE9 bytes");

    const std::filesystem::path spritePath = root / "latin1_tiny.spr";
    constexpr std::string_view spriteText =
        "caf\xE9.mat 0 1.0e-12 0.50000006 4 3 0 1.0e-30 1.0e-40 -2.0e-30 3.0e-25\n";
    check(writeBytes(spritePath, spriteText),
        "resource workspace byte preservation: fixed Latin-1 and tiny-float SPR source writes");

    auto spriteEditor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext()));
    if ( !spriteEditor )
    {
        check(false, "resource workspace byte preservation: Sprite editor is available");
        return;
    }
    openResourceFile(spriteEditor.get(), spritePath);
    const std::array<float, 6> expectedFloats{
        1.0e-12f, 0.50000006f, 1.0e-30f, 1.0e-40f, -2.0e-30f, 3.0e-25f };
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(spriteEditor.get());
    const QCborArray capturedSprite = captured.value(QStringLiteral("sprite")).toArray();
    const auto floatMatches = [](const QCborArray& values, const int index, const float expected)
        {
            if ( index < 0 || values.size() <= index || !values.at(index).isString() )
            {
                return false;
            }
            bool valid = false;
            const float actual = values.at(index).toString().toFloat(&valid);
            return valid
                && std::bit_cast<std::uint32_t>(actual)
                    == std::bit_cast<std::uint32_t>(expected);
        };
    check(floatMatches(capturedSprite, 2, expectedFloats[0])
            && floatMatches(capturedSprite, 3, expectedFloats[1])
            && floatMatches(capturedSprite, 7, expectedFloats[2])
            && floatMatches(capturedSprite, 12, expectedFloats[3])
            && floatMatches(capturedSprite, 13, expectedFloats[4])
            && floatMatches(capturedSprite, 14, expectedFloats[5]),
        "resource workspace byte preservation: settled EDIT capture retains tiny and untouched binary32 SPR fields");

    auto restoredSprite = std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext()));
    QString warning{};
    const bool restored = restoredSprite
        && ui::resources::restoreResourceAuthoringPaneState(
            restoredSprite.get(), captured, &warning);
    check(restored,
        "resource workspace byte preservation: tiny-float SPR EDIT draft restores");
    if ( !restored )
    {
        return;
    }
    const QCborArray restoredSpriteValues = ui::resources::captureResourceAuthoringPaneState(
        restoredSprite.get()).value(QStringLiteral("sprite")).toArray();
    check(floatMatches(restoredSpriteValues, 2, expectedFloats[0])
            && floatMatches(restoredSpriteValues, 3, expectedFloats[1])
            && floatMatches(restoredSpriteValues, 7, expectedFloats[2])
            && floatMatches(restoredSpriteValues, 12, expectedFloats[3])
            && floatMatches(restoredSpriteValues, 13, expectedFloats[4])
            && floatMatches(restoredSpriteValues, 14, expectedFloats[5]),
        "resource workspace byte preservation: restored EDIT keeps tiny and untouched binary32 SPR fields");

    if ( ui::ScrubFloatEdit* const width = restoredSprite->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("spr.width")) )
    {
        setScrubValue(width, 2.25);
    }
    const bool saved = ui::resources::resourceAuthoringPaneSave(restoredSprite.get(), false);
    std::istringstream savedInput{ readBytes(spritePath) };
    try
    {
        const assets::SprFile savedSprite = assets::loadFromSPR(savedInput);
        check(saved && savedSprite.width == 2.25f
                && std::bit_cast<std::uint32_t>(savedSprite.height)
                    == std::bit_cast<std::uint32_t>(expectedFloats[1])
                && std::bit_cast<std::uint32_t>(savedSprite.extraLightRGBA[0])
                    == std::bit_cast<std::uint32_t>(expectedFloats[2])
                && std::bit_cast<std::uint32_t>(savedSprite.offsetXYZ[0])
                    == std::bit_cast<std::uint32_t>(expectedFloats[3])
                && std::bit_cast<std::uint32_t>(savedSprite.offsetXYZ[1])
                    == std::bit_cast<std::uint32_t>(expectedFloats[4])
                && std::bit_cast<std::uint32_t>(savedSprite.offsetXYZ[2])
                    == std::bit_cast<std::uint32_t>(expectedFloats[5]),
            "resource workspace byte preservation: editing width preserves exact untouched SPR floats on Save");
    }
    catch ( const std::exception& )
    {
        check(false, "resource workspace byte preservation: saved fixed SPR remains parseable");
    }
    check(readBytes(spritePath).find("caf\xE9.mat") != std::string::npos,
        "resource workspace byte preservation: SPR EDIT restore and Save preserve raw material byte 0xE9");
}

void checkResourceWorkspaceSoundGroupsAndProjectProfiles()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace sound groups: temporary resource directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path soundPath =
        std::filesystem::path(temporaryDirectory.path().toStdWString()) / "groups.snd";
    constexpr std::string_view soundText = "moving old.wav 0x00000040 0.5 2.5 1\n";
    check(writeBytes(soundPath, soundText),
        "resource workspace sound groups: fixed one-mode SND source writes");
    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::soundClass,
        testContext()));
    if ( !editor )
    {
        check(false, "resource workspace sound groups: Sound Class editor is available");
        return;
    }
    openResourceFile(editor.get(), soundPath);
    QTreeWidget* const entries = editor->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    QTreeWidgetItem* const alternative = soundAlternativeByWav(
        entries, QStringLiteral("old.wav"));
    if ( !entries || !alternative || !alternative->parent() )
    {
        check(false, "resource workspace sound groups: one grouped alternative is available");
        return;
    }

    check(alternative->parent()->text(0) == QStringLiteral("moving") &&
              alternative->parent()->data(0, Qt::UserRole + 23).toInt() == 5,
        "resource workspace sound groups: the mode header uses its fixed engine slot");

    bool doubleClickOpenedPicker = false;
    const QString before = alternative->text(1);
    bool signalInvoked = false;
    runWithModalDriver(
        [&]()
        {
            signalInvoked = QMetaObject::invokeMethod(
                entries,
                "itemDoubleClicked",
                Qt::DirectConnection,
                Q_ARG(QTreeWidgetItem*, alternative),
                Q_ARG(int, 1));
        },
        std::nullopt,
        std::nullopt,
        false,
        {},
        [&](QWidget* dialog)
        {
            doubleClickOpenedPicker = dialog != nullptr;
            if ( auto* const picker = qobject_cast<QDialog*>(dialog) )
            {
                picker->reject();
            }
        });
    check(signalInvoked && doubleClickOpenedPicker && alternative->text(1) == before,
        "resource workspace sound groups: WAV double-click opens the picker and cancel preserves the token");

    auto ijimStrings = std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext(ProjectType::IJIM)));
    const QCborMap ijimState = ijimStrings
        ? ui::resources::captureResourceAuthoringPaneState(ijimStrings.get()) : QCborMap{};
    check(ijimStrings
            && ijimState.value(QStringLiteral("profile")).toInteger()
                == static_cast<int>(assets::UniProfile::ijim)
            && !ijimStrings->findChild<QComboBox*>(QStringLiteral("uni.profile")),
        "resource workspace profiles: UNI uses the active IJIM profile without a disabled profile selector");
    if ( ijimStrings )
    {
        const QCborMap baseline = ui::resources::captureResourceAuthoringPaneState(ijimStrings.get());
        QCborMap hostile = baseline;
        hostile.insert(QStringLiteral("profile"), static_cast<int>(assets::UniProfile::classic));
        QString warning{};
        check(!ui::resources::restoreResourceAuthoringPaneState(
                ijimStrings.get(), hostile, &warning)
                && warning == QStringLiteral("String-table profile does not match the active project.")
                && ui::resources::captureResourceAuthoringPaneState(ijimStrings.get()) == baseline,
            "resource workspace profiles: mismatched EDIT profile is rejected with exact warning and leaves IJIM pane unchanged");
    }

    auto ijimSound = std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr,
        ui::resources::ResourceAuthoringKind::soundClass,
        testContext(ProjectType::IJIM)));
    const QCborMap soundState = ijimSound
        ? ui::resources::captureResourceAuthoringPaneState(ijimSound.get()) : QCborMap{};
    check(ijimSound
            && soundState.value(QStringLiteral("profile")).toInteger()
                == static_cast<int>(assets::SoundClassProfile::ijim)
            && !ijimSound->findChild<QComboBox*>(QStringLiteral("snd.profile")),
        "resource workspace profiles: SND stays bound to its project profile without an alternate selector");
}

void checkResourceWorkspaceHierarchicalSoundAndIjimMetadataPresentation()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace authoring: temporary grouped-resource directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path root(temporaryDirectory.path().toStdWString());
    const std::filesystem::path soundPath = root / "hierarchical.snd";
    constexpr std::string_view soundText =
        "moving first.wav 0x00000040 0.5 2.5 1\n"
        "moving none 0x00000040 0.5 2.5 1\n"
        "moving tiny.wav 0x00000040 0.01 2.5 1\n"
        "create create.wav 0x00000040 0.5 2.5 1\n";
    check(writeBytes(soundPath, soundText),
        "resource workspace authoring: fixed repeated-mode SND source writes");

    auto soundEditor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::soundClass,
        testContext()));
    check(soundEditor != nullptr,
        "resource workspace authoring: Sound Class editor is available for grouped rows");
    if ( soundEditor )
    {
        openResourceFile(soundEditor.get(), soundPath);
        QTreeWidget* const entries = soundEditor->findChild<QTreeWidget*>(
            QStringLiteral("snd.entries"));
        check(entries != nullptr,
            "resource workspace authoring: SND entries are represented by the actual tree editor");
        check(soundEditor->findChildren<QTableWidget*>().isEmpty(),
            "resource workspace authoring: SND does not keep a hidden table mirror");
        const QList<QTreeWidget*> soundTrees = soundEditor->findChildren<QTreeWidget*>();
        check(soundTrees.size() == 1,
            "resource workspace authoring: one visible SND tree owns groups and alternatives");

        if ( entries )
        {
            QTreeWidgetItem* movingGroup = nullptr;
            for ( int index = 0; index < entries->topLevelItemCount(); ++index )
            {
                QTreeWidgetItem* const group = entries->topLevelItem(index);
                if ( group && group->text(0).startsWith(QStringLiteral("moving")) )
                {
                    movingGroup = group;
                    break;
                }
            }
            check(movingGroup && movingGroup->text(0) == QStringLiteral("moving") &&
                      movingGroup->data(0, Qt::UserRole + 23).toInt() == 5,
                "resource workspace authoring: the moving header shows its exact engine slot 5");

            QTreeWidgetItem* addAlternativeRow = nullptr;
            for ( int index = 0; movingGroup && index < movingGroup->childCount(); ++index )
            {
                QTreeWidgetItem* const child = movingGroup->child(index);
                if ( child && child->text(0) == QStringLiteral("＋ Add Entry") )
                {
                    addAlternativeRow = child;
                    break;
                }
            }
            QAbstractButton* const addModeButton = button(soundEditor.get(), "snd.add_mode");
            QLineEdit* const modeSearch = soundEditor->findChild<QLineEdit*>(
                QStringLiteral("snd.search"));
            soundEditor->show();
            QApplication::processEvents(QEventLoop::AllEvents);
            bool resizableColumns = entries->columnCount() == 6;
            for ( int column = 0; resizableColumns && column < entries->columnCount(); ++column )
            {
                resizableColumns = entries->header()->sectionResizeMode(column)
                    == QHeaderView::Interactive;
            }
            const std::array<std::pair<QString, int>, 2> expectedModeHeaders{{
                {QStringLiteral("create"), 1},
                {QStringLiteral("moving"), 5},
            }};
            bool exactModeHeaders = entries->topLevelItemCount() == static_cast<int>(expectedModeHeaders.size());
            for ( int index = 0; exactModeHeaders && index < entries->topLevelItemCount(); ++index )
            {
                const QTreeWidgetItem* const group = entries->topLevelItem(index);
                const auto& expected = expectedModeHeaders[static_cast<std::size_t>(index)];
                exactModeHeaders = group && group->text(0) == expected.first
                    && group->data(0, Qt::UserRole + 23).toInt() == expected.second;
            }
            std::cerr << "SND grouped layout diagnostic: exactHeaders=" << exactModeHeaders
                      << " collapsed=" << (movingGroup && !movingGroup->isExpanded())
                      << " animated=" << entries->isAnimated()
                      << " groupSpanned=" << (movingGroup && movingGroup->isFirstColumnSpanned())
                      << " footerSpanned=" << (addAlternativeRow && addAlternativeRow->isFirstColumnSpanned())
                      << " addVisible=" << (addModeButton && !addModeButton->isHidden())
                      << " addAboveSearch=" << (addModeButton && modeSearch
                          && addModeButton->geometry().bottom() < modeSearch->geometry().top())
                      << " resizableSixColumns=" << resizableColumns << '\n';
            check(movingGroup && !movingGroup->isExpanded() && entries->isAnimated()
                    && exactModeHeaders
                    && movingGroup->isFirstColumnSpanned()
                    && addAlternativeRow && addAlternativeRow->isFirstColumnSpanned()
                    && addModeButton && !addModeButton->isHidden() && modeSearch
                    && addModeButton->geometry().bottom() < modeSearch->geometry().top()
                    && resizableColumns,
                "resource workspace authoring: mode groups start collapsed with animated full-width headers and six resizable data columns");

            int firstAlternativeIndex = -1;
            int noneAlternativeIndex = -1;
            int sourceAlternativeCount = 0;
            for ( int index = 0; movingGroup && index < movingGroup->childCount(); ++index )
            {
                const QTreeWidgetItem* const child = movingGroup->child(index);
                const QString wav = child ? child->text(1) : QString{};
                if ( wav == QStringLiteral("first.wav") )
                {
                    firstAlternativeIndex = index;
                    ++sourceAlternativeCount;
                }
                else if ( wav == QStringLiteral("none") )
                {
                    noneAlternativeIndex = index;
                    ++sourceAlternativeCount;
                }
            }
            check(sourceAlternativeCount == 2
                    && firstAlternativeIndex >= 0
                    && noneAlternativeIndex > firstAlternativeIndex,
                "resource workspace authoring: exact WAV cells retain source-ordered alternatives, including none");

            const QCborArray capturedSoundRows = ui::resources::captureResourceAuthoringPaneState(
                soundEditor.get()).value(QStringLiteral("rows")).toArray();
            bool capturedOrder = capturedSoundRows.size() == 4;
            const std::array<std::array<QString, 2>, 4> expectedSoundRows{
                std::array<QString, 2>{ QStringLiteral("moving"), QStringLiteral("first.wav") },
                std::array<QString, 2>{ QStringLiteral("moving"), QStringLiteral("none") },
                std::array<QString, 2>{ QStringLiteral("moving"), QStringLiteral("tiny.wav") },
                std::array<QString, 2>{ QStringLiteral("create"), QStringLiteral("create.wav") },
            };
            for ( int index = 0; capturedOrder && index < capturedSoundRows.size(); ++index )
            {
                const QCborArray row = capturedSoundRows.at(index).toArray();
                capturedOrder = row.size() == 6
                    && row.at(0).toString() == expectedSoundRows[static_cast<std::size_t>(index)][0]
                    && row.at(1).toString() == expectedSoundRows[static_cast<std::size_t>(index)][1];
            }
            check(capturedOrder,
                "resource workspace authoring: captured SND rows retain exact source order");

            QTreeWidgetItem* tinyAlternative = nullptr;
            for ( int index = 0; movingGroup && index < movingGroup->childCount(); ++index )
            {
                QTreeWidgetItem* const child = movingGroup->child(index);
                if ( child && child->text(1) == QStringLiteral("tiny.wav") )
                {
                    tinyAlternative = child;
                    break;
                }
            }
            const bool tinyDisplayedShortest = tinyAlternative
                && tinyAlternative->text(3) == QStringLiteral("0.01");
            check(tinyDisplayedShortest,
                "resource workspace authoring: binary32 radius 0.01 uses its shortest decimal display");

            QTreeWidgetItem* const firstAlternative = soundAlternativeByWav(
                entries, QStringLiteral("first.wav"));
            const QString flagsBeforeLoop = firstAlternative
                ? firstAlternative->text(2) : QString{};
            if ( firstAlternative )
            {
                (void)setSoundPreviewLoop(entries, firstAlternative, true);
                const QCborArray loopCapture = ui::resources::captureResourceAuthoringPaneState(
                    soundEditor.get()).value(QStringLiteral("rows")).toArray();
                const QCborArray loopCells = loopCapture.isEmpty()
                    ? QCborArray{} : loopCapture.at(0).toArray();
                check(loopCells.size() == 6
                        && loopCells.at(2).toString() == flagsBeforeLoop
                        && !ui::resources::resourceAuthoringPaneIsModified(soundEditor.get()),
                    "resource workspace authoring: preview loop state does not edit authored play flags or dirty the SND");
                (void)setSoundPreviewLoop(entries, firstAlternative, false);
            }

            soundEditor->show();
            entries->setAnimated(false);
            if ( movingGroup )
            {
                movingGroup->setExpanded(true);
            }
            QApplication::processEvents(QEventLoop::AllEvents);
            if ( tinyAlternative )
            {
                entries->setCurrentItem(tinyAlternative, 3);
                entries->editItem(tinyAlternative, 3);
                QApplication::processEvents(QEventLoop::AllEvents);
                const QModelIndex minimumIndex = entries->indexFromItem(
                    tinyAlternative, 3);
                const auto* const numericEditor = qobject_cast<ui::ScrubFloatEdit*>(
                    entries->indexWidget(minimumIndex));
                check(numericEditor && numericEditor->lineEdit()->text() == QStringLiteral("0.01"),
                    "resource workspace authoring: SND numeric columns use an editable precise scrub control");
            }

            if ( firstAlternative )
            {
                QTreeWidgetItem* const item = firstAlternative;
                const QRect initialRowRect = entries->visualItemRect(item);
                const auto movePointerToColumn = [entries, item](const int column)
                {
                    const QModelIndex index = entries->indexFromItem(item, column);
                    const QPoint local = entries->visualRect(index).center();
                    QMouseEvent move(
                        QEvent::MouseMove,
                        QPointF(local),
                        QPointF(entries->viewport()->mapToGlobal(local)),
                        Qt::NoButton,
                        Qt::NoButton,
                        Qt::NoModifier);
                    QApplication::sendEvent(entries->viewport(), &move);
                    QApplication::processEvents(QEventLoop::AllEvents);
                };
                movePointerToColumn(0);
                const auto* const playButton = entries->findChild<QToolButton*>(
                    QStringLiteral("snd.play"));
                const bool playAppearsOnHover = playButton
                    && playButton->objectName() == QStringLiteral("snd.play")
                    && playButton->isVisible();
                const auto* const loopButton = entries->findChild<QToolButton*>(
                    QStringLiteral("snd.loop"));
                movePointerToColumn(5);
                const auto* const removeButton = entries->findChild<QToolButton*>(
                    QStringLiteral("snd.remove"));
                const bool removeAppearsOnHover = removeButton
                    && removeButton->objectName() == QStringLiteral("snd.remove")
                    && removeButton->isVisible();
                entries->setCurrentItem(item, 5);
                entries->editItem(item, 5);
                QApplication::processEvents(QEventLoop::AllEvents);
                QWidget* const volumeEditor = entries->indexWidget(
                    entries->indexFromItem(item, 5));
                QSlider* const volumeSlider = volumeEditor
                    ? volumeEditor->findChild<QSlider*>(QStringLiteral("snd.volume.slider"))
                    : nullptr;
                bool volumeEditCommitted = false;
                if ( volumeSlider )
                {
                    const QPoint pressPosition(
                        volumeSlider->width() / 2,
                        volumeSlider->height() / 2);
                    const QPoint dragPosition(
                        volumeSlider->width() / 4,
                        volumeSlider->height() / 2);
                    QMouseEvent press(
                        QEvent::MouseButtonPress,
                        QPointF(pressPosition),
                        QPointF(volumeSlider->mapToGlobal(pressPosition)),
                        Qt::LeftButton,
                        Qt::LeftButton,
                        Qt::NoModifier);
                    QMouseEvent move(
                        QEvent::MouseMove,
                        QPointF(dragPosition),
                        QPointF(volumeSlider->mapToGlobal(dragPosition)),
                        Qt::NoButton,
                        Qt::LeftButton,
                        Qt::NoModifier);
                    QMouseEvent release(
                        QEvent::MouseButtonRelease,
                        QPointF(dragPosition),
                        QPointF(volumeSlider->mapToGlobal(dragPosition)),
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
                    QApplication::sendEvent(volumeSlider, &press);
                    QApplication::sendEvent(volumeSlider, &move);
                    QApplication::sendEvent(volumeSlider, &release);
                    QApplication::processEvents(QEventLoop::AllEvents);
                    const QCborArray editedRows = ui::resources::captureResourceAuthoringPaneState(
                        soundEditor.get()).value(QStringLiteral("rows")).toArray();
                    const QCborArray editedFirst = editedRows.isEmpty()
                        ? QCborArray{} : editedRows.at(0).toArray();
                    bool volumeTextValid = false;
                    const float savedVolume = editedFirst.size() == 6
                        ? editedFirst.at(5).toString().toFloat(&volumeTextValid)
                        : -1.0f;
                    volumeEditCommitted = editedFirst.size() == 6
                        && volumeTextValid && savedVolume >= 0.0f && savedVolume <= 1.0f
                        && std::abs(static_cast<double>(savedVolume)
                            - static_cast<double>(volumeSlider->value()) / 1000.0) <= 0.001
                        && ui::resources::resourceAuthoringPaneIsModified(soundEditor.get());
                }
                check(playAppearsOnHover && removeAppearsOnHover
                        && loopButton && loopButton->isCheckable()
                        && playButton->geometry().right() < loopButton->geometry().left()
                        && loopButton->geometry().right() < entries->visualRect(
                            entries->indexFromItem(item, 1)).left()
                        && removeButton->geometry().left() >= entries->viewport()->width() - 30,
                    "resource workspace authoring: shared hover play and loop stay left and remove stays right");
                check(entries->viewport()->findChildren<QToolButton*>(QString{}, Qt::FindDirectChildrenOnly).size() == 5,
                    "resource workspace authoring: exactly five shared hover and playing controls serve every row");
                check(volumeSlider
                        && volumeSlider->objectName() == QStringLiteral("snd.volume.slider")
                        && volumeSlider->minimum() == 0 && volumeSlider->maximum() == 1000
                        && volumeEditCommitted,
                    "resource workspace authoring: explicit normalized volume slider editing commits its displayed value");
                check(entries->visualItemRect(item) == initialRowRect,
                    "resource workspace authoring: hovered controls and volume editing preserve row geometry");
            }

            const assets::SoundClassFile original = assets::loadFromSoundClass(
                soundPath, assets::SoundClassProfile::classic);
            const std::uint32_t sourceTinyBits = std::bit_cast<std::uint32_t>(
                original.entries.at(2).minRadius);
            const bool savedSound = ui::resources::resourceAuthoringPaneSave(
                soundEditor.get(), false);
            std::uint32_t savedTinyBits{};
            bool savedTinyValid = false;
            try
            {
                const assets::SoundClassFile saved = assets::loadFromSoundClass(
                    soundPath, assets::SoundClassProfile::classic);
                savedTinyBits = std::bit_cast<std::uint32_t>(saved.entries.at(2).minRadius);
                savedTinyValid = saved.entries.size() == expectedSoundRows.size();
            }
            catch ( const std::exception& )
            {
                savedTinyValid = false;
            }
            check(savedSound && savedTinyValid && sourceTinyBits == savedTinyBits,
                "resource workspace authoring: shortest radius output preserves its exact binary32 value");
        }
    }

    const std::filesystem::path stringPath = root / "ijim_metadata.uni";
    constexpr std::string_view stringText =
        "MSGS 1\n\n\"HELLO\" -19 \"Text\"\n\nEND\n";
    check(writeBytes(stringPath, stringText),
        "resource workspace authoring: fixed IJIM UNI metadata source writes");

    auto ijimEditor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext(ProjectType::IJIM)));
    check(ijimEditor != nullptr,
        "resource workspace authoring: IJIM String Table editor is available");
    if ( ijimEditor )
    {
        openResourceFile(ijimEditor.get(), stringPath);
        QTableWidget* const ijimRows = ijimEditor->findChild<QTableWidget*>(
            QStringLiteral("uni.entries"));
        check(ijimRows != nullptr && ijimRows->columnCount() == 3
                && ijimRows->isColumnHidden(2),
            "resource workspace authoring: IJIM hides only the legacy-number presentation column");
        const QCborArray capturedRows = ui::resources::captureResourceAuthoringPaneState(
            ijimEditor.get()).value(QStringLiteral("rows")).toArray();
        const QCborArray capturedEntry = capturedRows.isEmpty()
            ? QCborArray{} : capturedRows.at(0).toArray();
        check(capturedEntry.size() == 3
                && capturedEntry.at(2).toString() == QStringLiteral("-19"),
            "resource workspace authoring: hidden IJIM numeric metadata remains in the EDIT draft");
        check(ui::resources::resourceAuthoringPaneSave(ijimEditor.get(), false)
                && readBytes(stringPath) == stringText,
            "resource workspace authoring: no-op IJIM Save preserves hidden signed metadata exactly");
    }

    auto classicEditor = std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext(ProjectType::JKDF2)));
    QTableWidget* const classicRows = classicEditor
        ? classicEditor->findChild<QTableWidget*>(QStringLiteral("uni.entries")) : nullptr;
    check(classicRows != nullptr && classicRows->columnCount() == 3
            && classicRows->isColumnHidden(2),
        "resource workspace authoring: classic UNI also hides retained unused metadata");
}

void checkResourceWorkspaceSoundStructuralUndoRedo()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace SND history: temporary sound source directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path path =
        std::filesystem::path(temporaryDirectory.path().toStdWString()) / "history.snd";
    constexpr std::string_view source =
        "moving one.wav 0x00000040 0.5 2.5 1\n"
        "create two.wav 0x00000020 0.25 1.5 0.75\n";
    check(writeBytes(path, source),
        "resource workspace SND history: fixed interleaved source writes");

    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::soundClass,
        testContext()));
    check(editor != nullptr,
        "resource workspace SND history: isolated Sound Class editor is available");
    if ( !editor )
    {
        return;
    }
    openResourceFile(editor.get(), path);
    QTreeWidget* const tree = editor->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    check(tree != nullptr,
        "resource workspace SND history: the actual grouped tree is available");
    if ( !tree )
    {
        return;
    }
    editor->show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const int initialRows = soundAlternativeItems(tree).size();
    check(initialRows == 2 && !ui::resources::resourceAuthoringPaneIsModified(editor.get()),
        "resource workspace SND history: the fixed interleaved source starts clean with two rows");

    const bool added = addSoundAlternative(tree, QStringLiteral("moving"));
    const int afterAdd = soundAlternativeItems(tree).size();
    QTreeWidgetItem* const newItem = soundAlternativeByWav(
        tree, QStringLiteral("new_sound.wav"));
    if ( newItem )
    {
        QWidget* const cellEditor = tree->indexWidget(tree->indexFromItem(newItem, 1));
        if ( cellEditor )
        {
            QKeyEvent commit(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(cellEditor, &commit);
        }
    }
    tree->setFocus(Qt::OtherFocusReason);
    QKeyEvent undo(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
    QApplication::sendEvent(tree, &undo);
    QApplication::processEvents(QEventLoop::AllEvents);
    const int afterUndo = soundAlternativeItems(tree).size();
    const bool cleanAfterUndo = !ui::resources::resourceAuthoringPaneIsModified(editor.get());

    QKeyEvent redo(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    QApplication::sendEvent(tree, &redo);
    QApplication::processEvents(QEventLoop::AllEvents);
    const int afterRedo = soundAlternativeItems(tree).size();
    const bool dirtyAfterRedo = ui::resources::resourceAuthoringPaneIsModified(editor.get());
    check(added && afterAdd == initialRows + 1
            && afterUndo == initialRows && cleanAfterUndo
            && afterRedo == initialRows + 1 && dirtyAfterRedo,
        "resource workspace SND history: local Ctrl+Z and Ctrl+Shift+Z undo and redo one structural edit");
}

void checkSoundSearchSurvivesStructuralHistory()
{
    QTemporaryDir directory{};
    if ( !directory.isValid() )
    {
        check(false, "resource workspace SND search history: temporary directory is available");
        return;
    }
    const std::filesystem::path path = std::filesystem::path(directory.path().toStdWString()) / "search.snd";
    check(writeBytes(path, "moving one.wav 0x80000040 0.5 2.5 1\ncreate two.wav 0x40 0.5 2.5 1\nmoving three.wav 0x20 "
                           "0.25 1.5 0.75\n"),
          "resource workspace SND search history: interleaved source writes");
    auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext()));
    if ( !pane )
    {
        check(false, "resource workspace SND search history: pane is available");
        return;
    }
    openResourceFile(pane.get(), path);
    QTreeWidget* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    QLineEdit* const search = pane->findChild<QLineEdit*>(QStringLiteral("snd.search"));
    QTreeWidgetItem* const first = soundAlternativeByWav(tree, QStringLiteral("one.wav"));
    if ( !tree || !search || !first )
    {
        check(false, "resource workspace SND search history: row and search controls are available");
        return;
    }
    pane->show();
    tree->setAnimated(false);
    search->setText(QStringLiteral("moving"));
    QApplication::processEvents(QEventLoop::AllEvents);
    const QCborMap before = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QPoint local = tree->visualRect(tree->indexFromItem(first, 0)).center();
    QMouseEvent move(QEvent::MouseMove, QPointF(local), QPointF(tree->viewport()->mapToGlobal(local)), Qt::NoButton,
                     Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &move);
    QToolButton* const remove = tree->findChild<QToolButton*>(QStringLiteral("snd.remove"));
    if ( !remove || !remove->isVisible() )
    {
        check(false, "resource workspace SND search history: hovered row has its remove control");
        return;
    }
    remove->click();
    QTreeWidgetItem* remaining = soundAlternativeByWav(tree, QStringLiteral("three.wav"));
    const bool expandedAfterRemove = remaining && remaining->parent()->isExpanded();
    const QCborMap removed = ui::resources::captureResourceAuthoringPaneState(pane.get());
    tree->setFocus(Qt::OtherFocusReason);
    const bool undone = ui::resources::resourceAuthoringPaneUndoRedo(pane.get(), false);
    remaining = soundAlternativeByWav(tree, QStringLiteral("three.wav"));
    const QCborMap restored = ui::resources::captureResourceAuthoringPaneState(pane.get());
    std::cerr << "SND search history diagnostic: expandedAfterRemove=" << expandedAfterRemove << " undone=" << undone
              << " expandedAfterUndo=" << (remaining && remaining->parent()->isExpanded()) << " removedCollapse="
              << (removed.value(QStringLiteral("collapsedModes")) == before.value(QStringLiteral("collapsedModes")))
              << " restoredRows=" << (restored.value(QStringLiteral("rows")) == before.value(QStringLiteral("rows")))
              << " restoredCollapse="
              << (restored.value(QStringLiteral("collapsedModes")) == before.value(QStringLiteral("collapsedModes")))
              << " focus="
              << (QApplication::focusWidget() ? QApplication::focusWidget()->metaObject()->className() : "null")
              << '\n';
    check(expandedAfterRemove && undone && remaining && remaining->parent()->isExpanded() &&
              removed.value(QStringLiteral("collapsedModes")) == before.value(QStringLiteral("collapsedModes")) &&
              restored.value(QStringLiteral("rows")) == before.value(QStringLiteral("rows")) &&
              restored.value(QStringLiteral("collapsedModes")) == before.value(QStringLiteral("collapsedModes")),
          "resource workspace SND search history: remove and Undo keep matching modes expanded and preserve exact "
          "source order and saved collapse choices");
    search->clear();
    remaining = soundAlternativeByWav(tree, QStringLiteral("three.wav"));
    check(remaining && !remaining->parent()->isExpanded(),
          "resource workspace SND search history: clearing the search restores the user's collapsed mode");
}

void checkSpriteZeroDimensionsRemainDraftOnly()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace sprite dimensions: temporary directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path sourcePath =
        std::filesystem::path(temporaryDirectory.path().toStdWString()) / "positive.spr";
    constexpr std::string_view sourceText = "fx.mat 0 1 1 4 3 0 0 0 0 0\n";
    check(writeBytes(sourcePath, sourceText),
        "resource workspace sprite dimensions: fixed positive-dimension SPR writes");

    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext()));
    check(editor != nullptr,
        "resource workspace sprite dimensions: Sprite editor is available");
    if ( editor )
    {
        openResourceFile(editor.get(), sourcePath);
        ui::ScrubFloatEdit* const width = editor->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("spr.width"));
        ui::ScrubFloatEdit* const height = editor->findChild<ui::ScrubFloatEdit*>(
            QStringLiteral("spr.height"));
        QLabel* const status = editor->findChild<QLabel*>(
            QStringLiteral("resource_authoring.status"));
        check(width && height && status,
            "resource workspace sprite dimensions: width, height, and status controls are available");
        if ( width && height && status )
        {
            setScrubValue(width, 0.0);
            bool zeroWidthSave = true;
            bool zeroWidthMessage = false;
            runWithModalDriver(
                [&]()
                {
                    zeroWidthSave = ui::resources::resourceAuthoringPaneSave(
                        editor.get(), false);
                },
                std::nullopt,
                std::nullopt,
                false,
                {},
                {},
                [&](QMessageBox* message)
                {
                    zeroWidthMessage = message->windowTitle()
                            == QStringLiteral("Resource validation failed")
                        && message->text()
                            == QStringLiteral("Sprite width and height must be greater than zero.");
                    message->accept();
                });
            check(!zeroWidthSave && zeroWidthMessage
                    && readBytes(sourcePath) == sourceText
                    && status->text() == QStringLiteral(
                        "Resource validation failed: Sprite width and height must be greater than zero."),
                "resource workspace sprite dimensions: zero width is rejected before external Save");

            setScrubValue(width, 1.0);
            setScrubValue(height, 0.0);
            bool zeroHeightSave = true;
            bool zeroHeightMessage = false;
            runWithModalDriver(
                [&]()
                {
                    zeroHeightSave = ui::resources::resourceAuthoringPaneSave(
                        editor.get(), false);
                },
                std::nullopt,
                std::nullopt,
                false,
                {},
                {},
                [&](QMessageBox* message)
                {
                    zeroHeightMessage = message->windowTitle()
                            == QStringLiteral("Resource validation failed")
                        && message->text()
                            == QStringLiteral("Sprite width and height must be greater than zero.");
                    message->accept();
                });
            check(!zeroHeightSave && zeroHeightMessage
                    && readBytes(sourcePath) == sourceText
                    && status->text() == QStringLiteral(
                        "Resource validation failed: Sprite width and height must be greater than zero."),
                "resource workspace sprite dimensions: zero height is rejected before external Save");
        }
    }

    auto sourceDraft = std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext()));
    ui::ScrubFloatEdit* const draftWidth = sourceDraft
        ? sourceDraft->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width")) : nullptr;
    ui::ScrubFloatEdit* const draftHeight = sourceDraft
        ? sourceDraft->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.height")) : nullptr;
    if ( draftWidth && draftHeight )
    {
        setScrubValue(draftWidth, 0.0);
        setScrubValue(draftHeight, 0.0);
    }
    const QCborMap capturedDraft = sourceDraft
        ? ui::resources::captureResourceAuthoringPaneState(sourceDraft.get()) : QCborMap{};
    const QCborArray capturedSprite = capturedDraft.value(QStringLiteral("sprite")).toArray();
    check(capturedSprite.size() == 15
            && capturedSprite.at(2).toString() == QStringLiteral("0")
            && capturedSprite.at(3).toString() == QStringLiteral("0"),
        "resource workspace sprite dimensions: native EDIT retains zero width and height drafts");

    auto restoredDraft = std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext()));
    QString warning{};
    const bool restored = restoredDraft
        && ui::resources::restoreResourceAuthoringPaneState(
            restoredDraft.get(), capturedDraft, &warning);
    const QCborArray restoredSprite = restored
        ? ui::resources::captureResourceAuthoringPaneState(restoredDraft.get())
            .value(QStringLiteral("sprite")).toArray()
        : QCborArray{};
    check(restored
            && restoredSprite.size() == 15
            && restoredSprite.at(2).toString() == QStringLiteral("0")
            && restoredSprite.at(3).toString() == QStringLiteral("0"),
        "resource workspace sprite dimensions: zero dimensions restore in native EDIT without clamping");
}

void checkResourceWorkspaceColorPopupStagesAuthoredValues()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace color: temporary sprite directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path spritePath =
        std::filesystem::path(temporaryDirectory.path().toStdWString()) / "hdr.spr";
    constexpr std::string_view spriteText =
        "glow.mat 1 1 1 4 5 6 -2.25 3.5 1.25 -0.5 0 0 0\n";
    check(writeBytes(spritePath, spriteText),
        "resource workspace color: fixed HDR IJIM SPR source writes");
    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::sprite,
        testContext(ProjectType::IJIM)));
    if ( !editor )
    {
        check(false, "resource workspace color: IJIM Sprite editor is available");
        return;
    }
    openResourceFile(editor.get(), spritePath);
    const QCborMap baseline = ui::resources::captureResourceAuthoringPaneState(editor.get());
    const QCborArray baselineSprite = baseline.value(QStringLiteral("sprite")).toArray();
    check(baselineSprite.size() >= 12
            && baselineSprite.at(8).toString() == QStringLiteral("-2.25")
            && baselineSprite.at(9).toString() == QStringLiteral("3.5")
            && baselineSprite.at(10).toString() == QStringLiteral("1.25")
            && baselineSprite.at(11).toString() == QStringLiteral("-0.5"),
        "resource workspace color: source baseline retains signed HDR and alpha channel values");

    QPushButton* const swatch = editor->findChild<QPushButton*>(
        QStringLiteral("spr.extra_light.swatch"));
    check(swatch != nullptr
            && !editor->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.extra_light.r"))
            && !editor->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.extra_light.g"))
            && !editor->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.extra_light.b"))
            && !editor->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.extra_light.a")),
        "resource workspace color: IJIM values use one swatch instead of four RGBA spinboxes");
    if ( !swatch )
    {
        return;
    }

    swatch->click();
    auto* popup = dynamic_cast<ui::LiveColorPopup*>(editor->findChild<QWidget*>(
        QStringLiteral("spr.extra_light.popup")));
    check(popup != nullptr,
        "resource workspace color: swatch opens the resource-local LiveColorPopup");
    if ( !popup )
    {
        return;
    }
    popup->setCurrentColor(QColor(32, 96, 224, 80), true);
    const QCborArray previewState = ui::resources::captureResourceAuthoringPaneState(editor.get())
        .value(QStringLiteral("sprite")).toArray();
    check(previewState.size() >= 12
            && previewState.at(8) == baselineSprite.at(8)
            && previewState.at(9) == baselineSprite.at(9)
            && previewState.at(10) == baselineSprite.at(10)
            && previewState.at(11) == baselineSprite.at(11)
            && !ui::resources::resourceAuthoringPaneIsModified(editor.get()),
        "resource workspace color: live popup preview leaves authored HDR channels and dirty baseline staged");

    QKeyEvent escapePopup(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(popup, &escapePopup);
    QApplication::processEvents();
    const QCborArray escapedState = ui::resources::captureResourceAuthoringPaneState(editor.get())
        .value(QStringLiteral("sprite")).toArray();
    check(escapedState.size() >= 12
            && escapedState.at(8) == baselineSprite.at(8)
            && escapedState.at(9) == baselineSprite.at(9)
            && escapedState.at(10) == baselineSprite.at(10)
            && escapedState.at(11) == baselineSprite.at(11)
            && !ui::resources::resourceAuthoringPaneIsModified(editor.get()),
        "resource workspace color: Escape discards staged color without changing exact values or dirty state");
}

void checkSpritePopupCommitCanDeleteOwner()
{
    const QColor selected(32, 96, 224, 80);
    const std::array<float, 4> expected{
        static_cast<float>(selected.redF()), static_cast<float>(selected.greenF()),
        static_cast<float>(selected.blueF()), static_cast<float>(selected.alphaF()) };
    const auto hasExactChannels = [&expected](const QCborArray& sprite)
    {
        if ( sprite.size() != 15 )
        {
            return false;
        }
        for ( std::size_t channel = 0; channel < expected.size(); ++channel )
        {
            bool valid = false;
            const float value = sprite.at(static_cast<qsizetype>(8 + channel)).toString().toFloat(&valid);
            if ( !valid || value != expected[channel] )
            {
                return false;
            }
        }
        return true;
    };

    // Cover ordinary commit and Escape alongside the owner-deleting commit.
    // The unfixed close-handler tail can depend on allocator reuse to fail.
    for ( const int closeCase : { 0, 1, 2 } )
    {
        QPointer<QWidget> pane;
        bool armed = false;
        int publications = 0;
        int paneDestructions = 0;
        int popupDestructions = 0;
        QCborArray committedSprite;
        bool committedModified = false;
        QWidget* const created = ui::resources::createResourceAuthoringPane(
            nullptr, ui::resources::ResourceAuthoringKind::sprite, testContext(ProjectType::IJIM),
            [&](QWidget* changed)
            {
                if ( !armed || !pane || changed != pane.data() )
                {
                    return;
                }
                ++publications;
                committedSprite = ui::resources::captureResourceAuthoringPaneState(changed)
                    .value(QStringLiteral("sprite")).toArray();
                committedModified = ui::resources::resourceAuthoringPaneIsModified(changed);
                if ( closeCase == 2 )
                {
                    delete changed;
                }
            });
        pane = created;
        check(pane != nullptr,
            "resource workspace Sprite popup lifetime: IJIM pane is available");
        if ( !pane )
        {
            continue;
        }
        QObject::connect(created, &QObject::destroyed, QApplication::instance(), [&paneDestructions]()
            {
                ++paneDestructions;
            });
        QCborMap state = ui::resources::captureResourceAuthoringPaneState(created);
        QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
        if ( sprite.size() != 15 )
        {
            check(false, "resource workspace Sprite popup lifetime: canonical channels are available");
            delete created;
            continue;
        }
        sprite[5] = 2;
        sprite[7] = QStringLiteral("0.125");
        sprite[8] = QStringLiteral("0.125");
        sprite[9] = QStringLiteral("0.25");
        sprite[10] = QStringLiteral("0.5");
        sprite[11] = QStringLiteral("0.75");
        state.insert(QStringLiteral("sprite"), sprite);
        state.remove(QStringLiteral("history")); // The popup cases use these channels as an imported baseline.
        state.insert(QStringLiteral("modified"), false);
        QString warning;
        const bool restored = ui::resources::restoreResourceAuthoringPaneState(created, state, &warning);
        QPushButton* const swatch = created->findChild<QPushButton*>(QStringLiteral("spr.extra_light.swatch"));
        check(restored && swatch,
            "resource workspace Sprite popup lifetime: exact IJIM channel baseline restores");
        if ( !restored || !swatch )
        {
            delete created;
            continue;
        }
        const QCborArray baseline = ui::resources::captureResourceAuthoringPaneState(created)
            .value(QStringLiteral("sprite")).toArray();
        swatch->click();
        auto* const popup = dynamic_cast<ui::LiveColorPopup*>(created->findChild<QWidget*>(
            QStringLiteral("spr.extra_light.popup")));
        QPointer<ui::LiveColorPopup> guardedPopup(popup);
        check(popup != nullptr,
            "resource workspace Sprite popup lifetime: real color popup opens");
        if ( !popup )
        {
            delete created;
            continue;
        }
        QObject::connect(popup, &QObject::destroyed, QApplication::instance(), [&popupDestructions]()
            {
                ++popupDestructions;
            });
        armed = true;
        popup->setCurrentColor(selected, true);
        check(pane && guardedPopup && popup->currentColor() == selected && publications == 0
                && ui::resources::captureResourceAuthoringPaneState(created)
                       .value(QStringLiteral("sprite")).toArray() == baseline
                && !ui::resources::resourceAuthoringPaneIsModified(created),
            "resource workspace Sprite popup lifetime: actual live color stages exact channels without publication");
        if ( !pane || !guardedPopup )
        {
            armed = false;
            if ( pane )
            {
                delete pane.data();
            }
            continue;
        }

        if ( closeCase == 1 )
        {
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(popup, &escape);
        }
        else
        {
            popup->close();
        }
        if ( guardedPopup )
        {
            QCoreApplication::sendPostedEvents(guardedPopup.data(), QEvent::DeferredDelete);
        }
        if ( closeCase == 2 )
        {
            check(publications == 1 && committedModified && hasExactChannels(committedSprite)
                    && pane.isNull() && guardedPopup.isNull()
                    && paneDestructions == 1 && popupDestructions == 1,
                "resource workspace Sprite popup lifetime: RGB close commits exact channels once while its callback synchronously deletes pane and popup");
        }
        else if ( closeCase == 1 )
        {
            check(pane && guardedPopup.isNull() && popupDestructions == 1 && paneDestructions == 0
                    && publications == 0 && ui::resources::captureResourceAuthoringPaneState(pane.data())
                        .value(QStringLiteral("sprite")).toArray() == baseline
                    && !ui::resources::resourceAuthoringPaneIsModified(pane.data()),
                "resource workspace Sprite popup lifetime: Escape destroys only the popup and preserves exact authored channels");
        }
        else
        {
            check(pane && guardedPopup.isNull() && popupDestructions == 1 && paneDestructions == 0
                    && publications == 1 && committedModified && hasExactChannels(committedSprite)
                    && hasExactChannels(ui::resources::captureResourceAuthoringPaneState(pane.data())
                        .value(QStringLiteral("sprite")).toArray()),
                "resource workspace Sprite popup lifetime: ordinary close commits exact channels once and retains its pane");
        }
        armed = false;
        if ( pane )
        {
            delete pane.data();
        }
    }
}

void checkResourceWorkspaceRecoveryAndNativeSaveBaselines()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace recovery: isolated project and recovery storage are available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path root(temporaryDirectory.path().toStdWString());
    const std::filesystem::path nativePath = root / "resource_draft.sed";
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setRecoveryDirectoryForTest(root / "recovery");
    editor::DocumentState documentState{};
    documentState.level.kind = ProjectType::JKDF2;
    documentState.level.header.version = 1;
    documentState.level.layers = { "Base" };
    window.setDocumentStateForTest(std::move(documentState));
    window.setProjectTypeForTest(ProjectType::JKDF2);
    window.setRecoveryDirectoryForTest(root / "recovery");
    window.beginRecoveryDocumentForTest(nativePath, false);
    window.waitForRecoverySnapshotForTest();

    const auto recoveryEditorState = [](const std::filesystem::path& recoveryPath)
        {
            QString error{};
            const std::optional<ui::recovery::RecoveryRecord> record =
                ui::recovery::readRecoveryRecord(recoveryPath, &error);
            if ( !record )
            {
                return QCborMap{};
            }
            const QCborValue value = QCborValue::fromCbor(record->editorStatePayload);
            return value.isMap() ? value.toMap() : QCborMap{};
        };
    const auto resourceDraftHasRawNumber = [](const QCborValue& resourceWorkspace, const QString& expected)
        {
            if ( !resourceWorkspace.isMap() )
            {
                return false;
            }
            const QCborArray resourceTabs = resourceWorkspace.toMap()
                .value(QStringLiteral("tabs")).toArray();
            for ( const QCborValue& tabValue : resourceTabs )
            {
                const QCborMap tab = tabValue.toMap();
                const QCborArray rows = tab.value(QStringLiteral("page"))
                    .toMap().value(QStringLiteral("rows")).toArray();
                for ( const QCborValue& rowValue : rows )
                {
                    const QCborArray cells = rowValue.toArray();
                    if ( cells.size() > 2 && cells.at(2).toString() == expected )
                    {
                        return true;
                    }
                }
            }
            return false;
        };
    const std::filesystem::path recoveryPath = window.currentRecoveryPathForTest();
    const std::optional<ui::recovery::RecoveryRecord> cleanBaseline =
        ui::recovery::readRecoveryRecord(recoveryPath);
    check(cleanBaseline.has_value() && !cleanBaseline->metadata.requiresUserRecovery,
        "resource workspace recovery: initial clean project checkpoint is not offer-eligible");

    QAction* const newStringTable = window.findChild<QAction*>(
        QStringLiteral("edit.resources.string_table_uni"));
    check(newStringTable != nullptr,
        "resource workspace recovery: String Table New command is available");
    if ( !newStringTable )
    {
        return;
    }
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::stringTable);
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"));
    QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
    check(page && addInvalidStringNumber(page),
        "resource workspace recovery: raw invalid UNI cell is available for recovery capture");
    if ( !page )
    {
        return;
    }
    const QByteArray authoredStateBeforeReplacement =
        window.captureRecoveryEditorStateForTest();
    const bool resourceDraftModified = ui::resources::resourceAuthoringPaneIsModified(page);
    check(resourceDraftModified,
        "resource workspace recovery: editing raw UNI cells marks the external resource draft modified");
    check(window.hasUnsavedChangesForTest(),
        "resource workspace recovery: resource-only draft participates in MainWindow dirty state");
    check(window.hasRecoveryOfferEligibleChangesForTest(),
        "resource workspace recovery: resource-only draft is eligible for a recovery offer");

    window.triggerRecoverySnapshotForTest();
    window.waitForRecoverySnapshotForTest();
    QString recoveryError{};
    const std::optional<ui::recovery::RecoveryRecord> authoredRecovery =
        ui::recovery::readRecoveryRecord(recoveryPath, &recoveryError);
    const QCborMap recoveredEditor = recoveryEditorState(recoveryPath);
    check(authoredRecovery.has_value()
            && authoredRecovery->metadata.requiresUserRecovery
            && recoveredEditor.contains(QStringLiteral("resourceEditors"))
            && resourceDraftHasRawNumber(
                recoveredEditor.value(QStringLiteral("resourceEditors")), QStringLiteral("17x")),
        "resource workspace recovery: recovery publication includes invalid raw draft and offers it after restart");

    bool replacementAuthorized = true;
    runWithModalDriver(
        [&]()
        {
            replacementAuthorized = window.authorizeDocumentReplacementForTest(
                QStringLiteral("opening another project"), {}, nullptr);
        },
        std::nullopt,
        QMessageBox::Cancel);
    check(!replacementAuthorized
            && window.captureRecoveryEditorStateForTest() == authoredStateBeforeReplacement
            && tabs->currentWidget() == page
            && ui::resources::resourceAuthoringPaneIsModified(page),
        "resource workspace recovery: cancelling replacement preserves the exact dirty raw draft and tab");

    window.setAttribute(Qt::WA_DontShowOnScreen, false);
    if ( QDockWidget* const dock = resourceEditorsDock(window, ui::resources::ResourceAuthoringKind::stringTable) )
    {
        dock->show();
    }
    window.resize(920, 680);
    window.show();
    window.raise();
    window.activateWindow();
    GlView* const viewport = window.glViewForTest();
    if ( viewport )
    {
        viewport->setFocus(Qt::OtherFocusReason);
    }
    const bool viewportOwnsFocus = waitUntil(
        [&]() { return viewport && QApplication::focusWidget() == viewport; });
    check(viewportOwnsFocus,
        "resource workspace native Save: viewport is focused to request the native document command");
    QAction* const nativeSave = window.findChild<QAction*>(QStringLiteral("file.save"));
    check(nativeSave && nativeSave->isEnabled(),
        "resource workspace native Save: native document Save action is enabled");
    if ( viewportOwnsFocus && nativeSave && nativeSave->isEnabled() )
    {
        nativeSave->trigger();
        std::optional<assets::SedFile> savedDocument{};
        try
        {
            if ( std::filesystem::is_regular_file(nativePath) )
            {
                savedDocument = assets::loadSedFile(nativePath);
            }
        }
        catch ( const std::exception& )
        {
            savedDocument.reset();
        }
        QCborMap savedEditor{};
        if ( savedDocument )
        {
            const auto editSection = std::find_if(
                savedDocument->optionalSections.begin(),
                savedDocument->optionalSections.end(), [](const assets::SedOptionalSection& section)
                {
                    return section.type == ui::project::SedEditorStateSection;
                });
            if ( editSection != savedDocument->optionalSections.end() )
            {
                const QByteArray payload(
                    reinterpret_cast<const char*>(editSection->payload.data()),
                    static_cast<qsizetype>(editSection->payload.size()));
                const QCborValue value = QCborValue::fromCbor(payload);
                if ( value.isMap() )
                {
                    savedEditor = value.toMap();
                }
            }
        }
        check(savedDocument.has_value()
                && savedEditor.contains(QStringLiteral("resourceEditors"))
                && resourceDraftHasRawNumber(
                    savedEditor.value(QStringLiteral("resourceEditors")), QStringLiteral("17x")),
            "resource workspace native Save: SED embeds invalid raw resource draft in EDIT");
        check(!window.hasUnsavedChangesForTest()
                && !window.hasRecoveryOfferEligibleChangesForTest(),
            "resource workspace native Save: captured resource draft becomes clean at the native baseline");
        check(ui::resources::resourceAuthoringPaneIsModified(page),
            "resource workspace native Save: saving Level does not clear a resource pane's external dirty flag");
        QTableWidget* const table = page->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
        QTableWidgetItem* const number = table ? table->item(0, 2) : nullptr;
        check(number != nullptr,
            "resource workspace native Save: raw table cell remains available after checkpoint");
        if ( number )
        {
            number->setText(QStringLiteral("18x"));
            check(window.hasUnsavedChangesForTest()
                    && window.hasRecoveryOfferEligibleChangesForTest(),
                "resource workspace native Save: later resource edit reopens native dirty and recovery eligibility");
            window.triggerRecoverySnapshotForTest();
            window.waitForRecoverySnapshotForTest();
            const auto laterRecovery = ui::recovery::readRecoveryRecord(recoveryPath);
            const QCborMap laterEditor = recoveryEditorState(recoveryPath);
            check(laterRecovery.has_value() && laterRecovery->metadata.requiresUserRecovery
                    && resourceDraftHasRawNumber(
                        laterEditor.value(QStringLiteral("resourceEditors")), QStringLiteral("18x")),
                "resource workspace native Save: subsequent raw draft edit publishes an eligible recovery offer");
        }
    }
    window.hide();
}

void checkResourceWorkspacePresentationDoesNotDirtyCleanResource()
{
    QTemporaryDir saveDirectory{};
    check(saveDirectory.isValid(),
        "resource workspace presentation: isolated Save As destination is available");
    if ( !saveDirectory.isValid() )
    {
        return;
    }

    auto source = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext()));
    if ( !source )
    {
        check(false, "resource workspace presentation: source String Table editor is available");
        return;
    }
    const QCborMap sourceState = ui::resources::captureResourceAuthoringPaneState(source.get());
    QCborMap cleanState = sourceState;
    cleanState.insert(QStringLiteral("modified"), false);

    MainWindow window(nullptr, true);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    QAction* const newStringTable = window.findChild<QAction*>(
        QStringLiteral("edit.resources.string_table_uni"));
    if ( !newStringTable )
    {
        check(false, "resource workspace presentation: String Table New command is available");
        return;
    }
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::stringTable);
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"));
    QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
    QString warning{};
    const bool restoredClean = page
        && ui::resources::restoreResourceAuthoringPaneState(page, cleanState, &warning);
    if (!restoredClean && !warning.isEmpty())
    {
        std::cerr << "resource workspace presentation restore warning: "
            << warning.toStdString() << '\n';
    }
    check(restoredClean && !ui::resources::resourceAuthoringPaneIsModified(page),
        "resource workspace presentation: clean resource baseline restores before presentation changes");
    if ( !restoredClean )
    {
        return;
    }

    QAbstractButton* const addString = button(page, "uni.add");
    if ( addString )
    {
        addString->click();
        QCborMap withRow = ui::resources::captureResourceAuthoringPaneState(page);
        withRow.remove(QStringLiteral("history")); // Adopt the setup row as an independent clean baseline.
        withRow.insert(QStringLiteral("modified"), false);
        warning.clear();
        check(ui::resources::restoreResourceAuthoringPaneState(page, withRow, &warning),
            "resource workspace presentation: test row becomes a clean baseline");
    }

    createResourceDraft(window, ui::resources::ResourceAuthoringKind::stringTable);
    QWidget* const secondUniPage = tabs ? tabs->currentWidget() : nullptr;
    if ( secondUniPage )
    {
        QCborMap secondClean = ui::resources::captureResourceAuthoringPaneState(secondUniPage);
        secondClean.insert(QStringLiteral("modified"), false);
        check(ui::resources::restoreResourceAuthoringPaneState(secondUniPage, secondClean, &warning),
            "resource workspace presentation: second UNI file starts from a clean baseline");
    }
    QWidget* soundPage = nullptr;
    QTabWidget* soundTabs = nullptr;
    QAction* const newSound = window.findChild<QAction*>(
        QStringLiteral("edit.resources.sound_class_snd"));
    check(newSound != nullptr,
        "resource workspace presentation: Sound Class command is available");
    if ( newSound )
    {
        createResourceDraft(window, ui::resources::ResourceAuthoringKind::soundClass);
        soundTabs = window.findChild<QTabWidget*>(QStringLiteral("soundClassResourceEditorTabs"));
        soundPage = soundTabs ? soundTabs->currentWidget() : nullptr;
        QTreeWidget* const soundTree = soundPage
            ? soundPage->findChild<QTreeWidget*>(QStringLiteral("snd.entries"))
            : nullptr;
        if ( soundTree
            && addSoundMode(soundPage, soundTree, QStringLiteral("create"))
            && addSoundAlternative(soundTree, QStringLiteral("create")) )
        {
            QCborMap withSoundRow = ui::resources::captureResourceAuthoringPaneState(soundPage);
            withSoundRow.remove(QStringLiteral("history")); // Adopt the setup alternative as a clean baseline.
            withSoundRow.insert(QStringLiteral("modified"), false);
            warning.clear();
            check(ui::resources::restoreResourceAuthoringPaneState(
                    soundPage, withSoundRow, &warning),
                "resource workspace presentation: test Sound row becomes a clean baseline");
        }
    }
    if ( tabs && secondUniPage )
    {
        tabs->setCurrentWidget(secondUniPage);
    }

    window.markCurrentDocumentAsSavedForTest();
    QLineEdit* const search = page->findChild<QLineEdit*>(QStringLiteral("uni.search"));
    QDockWidget* const dock = resourceEditorsDock(window, ui::resources::ResourceAuthoringKind::stringTable);
    const QByteArray beforePresentation = window.captureRecoveryEditorStateForTest();
    const int activeBeforePresentation = tabs ? tabs->currentIndex() : -1;
    window.captureReplacementAuthorizationFingerprintForTest();
    if ( search )
    {
        search->setText(QStringLiteral("presentation-only query"));
    }
    if ( tabs && tabs->count() > 1 )
    {
        tabs->setCurrentWidget(page);
    }
    page->setProperty("cogPinned", true);
    if ( tabs && tabs->count() > 1 )
    {
        if ( QTreeWidget* const groups = soundPage
                ? soundPage->findChild<QTreeWidget*>(QStringLiteral("snd.entries"))
                : nullptr )
        {
            groups->collapseAll();
        }
    }
    if ( dock )
    {
        dock->resize(520, 360);
        dock->hide();
        dock->show();
    }
    const QByteArray afterPresentation = window.captureRecoveryEditorStateForTest();
    const auto findPage = [](const QByteArray& editorPayload, const int kind)
        {
            const QCborValue rootValue = QCborValue::fromCbor(editorPayload);
            const QCborArray workspaceTabs = rootValue.isMap()
                ? rootValue.toMap().value(QStringLiteral("resourceEditors")).toMap()
                    .value(QStringLiteral("tabs")).toArray()
                : QCborArray{};
            for ( const QCborValue& tabValue : workspaceTabs )
            {
                const QCborMap pageState = tabValue.toMap()
                    .value(QStringLiteral("page")).toMap();
                if ( pageState.value(QStringLiteral("kind")).toInteger() == kind )
                {
                    return tabValue.toMap();
                }
            }
            return QCborMap{};
        };
    const QCborMap beforeRoot = QCborValue::fromCbor(beforePresentation).toMap();
    const QCborMap afterRoot = QCborValue::fromCbor(afterPresentation).toMap();
    const QCborMap beforeWorkspace = beforeRoot.value(QStringLiteral("resourceEditors")).toMap();
    const QCborMap afterWorkspace = afterRoot.value(QStringLiteral("resourceEditors")).toMap();
    const QCborMap beforeUni = findPage(beforePresentation,
        static_cast<int>(ui::resources::ResourceAuthoringKind::stringTable));
    const QCborMap afterUni = findPage(afterPresentation,
        static_cast<int>(ui::resources::ResourceAuthoringKind::stringTable));
    const QCborMap afterSound = findPage(afterPresentation,
        static_cast<int>(ui::resources::ResourceAuthoringKind::soundClass));
    bool soundGroupCollapsed = false;
    for ( const QCborValue& mode : afterSound.value(QStringLiteral("page")).toMap()
             .value(QStringLiteral("collapsedModes")).toArray() )
    {
        soundGroupCollapsed = soundGroupCollapsed || mode.toString() == QStringLiteral("create");
    }
    const bool presentationFieldsPersisted = search && dock
        && beforeUni.value(QStringLiteral("page")).toMap()
            .value(QStringLiteral("search")).toString().isEmpty()
        && afterUni.value(QStringLiteral("page")).toMap()
            .value(QStringLiteral("search")).toString() == QStringLiteral("presentation-only query")
        && !beforeUni.value(QStringLiteral("pinned")).toBool()
        && afterUni.value(QStringLiteral("pinned")).toBool()
        && beforeWorkspace.value(QStringLiteral("groups")).toMap()
            .value(QStringLiteral("uni")).toMap().value(QStringLiteral("active")).toInteger()
                == activeBeforePresentation
        && afterWorkspace.value(QStringLiteral("groups")).toMap()
            .value(QStringLiteral("uni")).toMap().value(QStringLiteral("active")).toInteger()
                == (tabs ? tabs->indexOf(page) : -1)
        && (tabs ? tabs->indexOf(page) : -1) != activeBeforePresentation
        && soundGroupCollapsed
        && beforeWorkspace.value(QStringLiteral("groups")).toMap()
            .value(QStringLiteral("uni")).toMap().value(QStringLiteral("geometry")).toByteArray()
            != afterWorkspace.value(QStringLiteral("groups")).toMap()
                .value(QStringLiteral("uni")).toMap().value(QStringLiteral("geometry")).toByteArray();
    check(presentationFieldsPersisted && beforePresentation != afterPresentation,
        "resource workspace presentation: EDIT retains exact search, pin, active tab, collapsed mode, and dock geometry changes");
    check(window.replacementAuthorizationStillCurrentForTest(),
        "resource workspace presentation: presentation changes preserve the exact authored replacement fingerprint");
    check(search && dock && !ui::resources::resourceAuthoringPaneIsModified(page)
            && !window.hasUnsavedChangesForTest()
            && !window.hasRecoveryOfferEligibleChangesForTest(),
        "resource workspace presentation: search and dock layout do not dirty authored resource data");

    QTableWidget* const table = page->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    QTableWidgetItem* const key = table ? table->item(0, 0) : nullptr;
    QTableWidgetItem* const value = table ? table->item(0, 1) : nullptr;
    QTableWidgetItem* numeric = table ? table->item(0, 2) : nullptr;
    if ( key && value && numeric && search && dock )
    {
        key->setText(QStringLiteral("PRESENTATION_KEY"));
        value->setText(QStringLiteral("fixed saved value"));
        numeric->setText(QStringLiteral("0"));
        QCborMap validSaveState = ui::resources::captureResourceAuthoringPaneState(page);
        validSaveState.insert(QStringLiteral("capacity"), 1);
        warning.clear();
        const bool restoredValidRow = ui::resources::restoreResourceAuthoringPaneState(
            page, validSaveState, &warning);
        const std::filesystem::path savedPath = std::filesystem::path(
            saveDirectory.path().toStdWString()) / "presentation.uni";
        bool presentationSaveSucceeded = false;
        bool changedPresentationInsideDialog = false;
        const std::function<void(QFileDialog*)> mutateWhileChoosing =
            [&](QFileDialog* dialog)
            {
                if ( !changedPresentationInsideDialog )
                {
                    search->setText(QStringLiteral("changed during Save dialog"));
                    dock->resize(640, 420);
                    changedPresentationInsideDialog = true;
                }
                dialog->setOption(QFileDialog::DontConfirmOverwrite, true);
                dialog->selectFile(pathForDialog(savedPath));
                (void)QMetaObject::invokeMethod(
                    dialog, "accept", Qt::DirectConnection);
            };
        if ( restoredValidRow )
        {
            runWithModalDriver(
                [&]()
                {
                    presentationSaveSucceeded =
                        ui::resources::resourceAuthoringPaneSave(page, true);
                },
                std::nullopt,
                QMessageBox::Ok,
                false,
                mutateWhileChoosing);
        }
        const std::string actualSavedBytes = readBytes(savedPath);
        constexpr std::string_view kExpectedSavedBytes =
            "MSGS 1\n\n\"PRESENTATION_KEY\" 0 \"fixed saved value\"\n\nEND\n";
        check(restoredValidRow && changedPresentationInsideDialog
                && presentationSaveSucceeded
                && std::string_view(actualSavedBytes) == kExpectedSavedBytes,
            "resource workspace presentation: search and dock changes during Save As preserve exact authored bytes");

        window.markCurrentDocumentAsSavedForTest();
        numeric = table->item(0, 2);
        const QString original = numeric ? numeric->text() : QString{};
        window.captureReplacementAuthorizationFingerprintForTest();
        if ( numeric )
        {
            numeric->setText(QStringLiteral("37"));
            numeric->setText(original);
        }
        check(numeric && ui::resources::resourceAuthoringPaneIsModified(page),
            "resource workspace presentation: external-resource modified flag remains set after a content revert");
        const bool revertedDraftOfferEligible =
            window.hasRecoveryOfferEligibleChangesForTest();
        check(numeric && !revertedDraftOfferEligible
                && !window.hasUnsavedChangesForTest()
                && window.replacementAuthorizationStillCurrentForTest(),
            "resource workspace presentation: reverting exact authored cells to baseline clears native dirty and replacement differences");
    }
    else
    {
        check(false, "resource workspace presentation: reversible raw UNI test cell is available");
    }
}

void checkResourceWorkspaceContextSurvivesSameDocumentNativeSave()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace context: isolated project files are available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path root(temporaryDirectory.path().toStdWString());
    const std::filesystem::path nativePath = root / "same_document.sed";
    const std::filesystem::path firstPath = root / "first.uni";
    const std::filesystem::path secondPath = root / "second.uni";
    constexpr std::string_view firstSource =
        "MSGS 1\n\n\"FIRST\" 0 \"before\"\n\nEND\n";
    constexpr std::string_view firstExpected =
        "MSGS 1\n\n\"FIRST\" 0 \"after native Save\"\n\nEND\n";
    constexpr std::string_view secondSource =
        "MSGS 1\n\n\"SECOND\" 0 \"opened after native Save\"\n\nEND\n";
    check(writeBytes(firstPath, firstSource) && writeBytes(secondPath, secondSource),
        "resource workspace context: fixed UNI sources are written");

    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    editor::DocumentState documentState{};
    documentState.level.kind = ProjectType::JKDF2;
    documentState.level.header.version = 1;
    documentState.level.layers = { "Base" };
    window.setDocumentStateForTest(std::move(documentState));
    window.setProjectTypeForTest(ProjectType::JKDF2);
    window.beginRecoveryDocumentForTest(nativePath, false);

    QAction* const newStringTable = window.findChild<QAction*>(
        QStringLiteral("edit.resources.string_table_uni"));
    check(newStringTable != nullptr,
        "resource workspace context: String Table command is available");
    if ( !newStringTable )
    {
        return;
    }
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::stringTable);
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"));
    QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
    check(page != nullptr,
        "resource workspace context: a pane captures the active project context");
    if ( !page )
    {
        return;
    }
    bool firstOpenSucceeded = false;
    runWithModalDriver(
        [&]()
        {
            firstOpenSucceeded = ui::resources::resourceAuthoringPaneOpenLooseFileDirect(page);
        },
        firstPath);
    QTableWidget* const table = page->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    QTableWidgetItem* const text = table ? table->item(0, 1) : nullptr;
    check(firstOpenSucceeded && text && text->text() == QStringLiteral("before"),
        "resource workspace context: first loose UNI source is open before native Save");
    if ( !text )
    {
        return;
    }
    text->setText(QStringLiteral("after native Save"));

    window.setAttribute(Qt::WA_DontShowOnScreen, false);
    window.resize(920, 680);
    window.show();
    window.raise();
    window.activateWindow();
    GlView* const viewport = window.glViewForTest();
    if ( viewport )
    {
        viewport->setFocus(Qt::OtherFocusReason);
    }
    const bool viewportOwnsFocus = waitUntil(
        [&]() { return viewport && QApplication::focusWidget() == viewport; });
    QAction* const nativeSave = window.findChild<QAction*>(QStringLiteral("file.save"));
    if ( !viewportOwnsFocus || !nativeSave || !nativeSave->isEnabled() )
    {
        QWidget* const focused = QApplication::focusWidget();
        QWidget* const popup = QApplication::activePopupWidget();
        QWidget* const modal = QApplication::activeModalWidget();
        QWidget* const active = QApplication::activeWindow();
        std::cerr << "Resource native Save focus diagnostic: viewport=" << viewport
                  << " focus=" << focused
                  << " focusClass=" << (focused ? focused->metaObject()->className() : "none")
                  << " focusName=" << (focused ? focused->objectName().toStdString() : "none")
                  << " focusParent=" << (focused ? focused->parentWidget() : nullptr)
                  << " activeWindow=" << active
                  << " activeName=" << (active ? active->objectName().toStdString() : "none")
                  << " popup=" << popup << " modal=" << modal
                  << " nativeSaveExists=" << static_cast<bool>(nativeSave)
                  << " nativeSaveEnabled=" << (nativeSave && nativeSave->isEnabled()) << '\n';
    }
    check(viewportOwnsFocus && nativeSave && nativeSave->isEnabled(),
        "resource workspace context: native Save command is available while the Level viewport owns focus");
    if ( !viewportOwnsFocus || !nativeSave || !nativeSave->isEnabled() )
    {
        return;
    }
    nativeSave->trigger();
    const bool nativeFileWritten = waitUntil(
        [&]() { return std::filesystem::is_regular_file(nativePath); });
    check(nativeFileWritten,
        "resource workspace context: native Save completes for the same active project");
    if ( !nativeFileWritten )
    {
        return;
    }

    bool resourceSaved = false;
    runWithModalDriver(
        [&]() { resourceSaved = ui::resources::resourceAuthoringPaneSave(page, false); },
        std::nullopt,
        QMessageBox::Ok);
    const std::string savedResourceBytes = readBytes(firstPath);
    check(resourceSaved && std::string_view(savedResourceBytes) == firstExpected,
        "resource workspace context: existing pane can atomically Save exact bytes after same-document native Save");

    const std::filesystem::path reboundProjectDirectory = root / "rebound_project";
    const std::filesystem::path reboundMiscDirectory = reboundProjectDirectory / "misc";
    std::filesystem::create_directories(reboundMiscDirectory);
    window.completeRecoveryAfterNormalSaveForTest(
        reboundProjectDirectory / "renamed_project.sed");
    QAction* const resourceSaveAs = action(page, "resource_authoring.save_as");
    QString resourceSaveAsDirectory{};
    if ( resourceSaveAs )
    {
        runWithModalDriver(
            [resourceSaveAs]() { resourceSaveAs->trigger(); },
            std::nullopt,
            std::nullopt,
            false,
            [&](QFileDialog* dialog)
            {
                resourceSaveAsDirectory = dialog->directory().absolutePath();
                dialog->reject();
            });
    }
    check(resourceSaveAs
            && QDir::cleanPath(resourceSaveAsDirectory)
                == QDir::cleanPath(QString::fromStdWString(reboundMiscDirectory.wstring())),
        "resource workspace context: native Save As rebinds the pane's first-save directory to the new project");

    QCborMap openBaseline = ui::resources::captureResourceAuthoringPaneState(page);
    openBaseline.remove(QStringLiteral("history")); // The Open scenario adopts the settled buffer as its baseline.
    openBaseline.insert(QStringLiteral("modified"), false);
    QString openBaselineWarning{};
    const bool paneReadyForOpen = ui::resources::restoreResourceAuthoringPaneState(
        page, openBaseline, &openBaselineWarning);
    check(paneReadyForOpen,
        "resource workspace context: Open fixture starts from a settled pane buffer");
    bool reopenedLooseSource = false;
    if ( paneReadyForOpen )
    {
        runWithModalDriver(
            [&]()
            {
                reopenedLooseSource = ui::resources::resourceAuthoringPaneOpenLooseFileDirect(page);
            },
            secondPath);
    }
    const QCborMap reopened = ui::resources::captureResourceAuthoringPaneState(page);
    const QCborArray rows = reopened.value(QStringLiteral("rows")).toArray();
    const QCborArray cells = rows.isEmpty() ? QCborArray{} : rows.at(0).toArray();
    const std::filesystem::path reopenedPath(
        reopened.value(QStringLiteral("path")).toString().toStdWString());
    check(reopenedLooseSource && paneReadyForOpen && reopenedPath == secondPath
            && cells.size() > 1 && cells.at(0).toString() == QStringLiteral("SECOND")
            && cells.at(1).toString() == QStringLiteral("opened after native Save"),
        "resource workspace context: same-document native Save leaves the pane able to Open and capture exact replacement content");
    window.hide();
}

void checkResourceWorkspaceRecoveryCaptureHotPath()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace hot path: isolated recovery directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    MainWindow window(nullptr, true);
    const std::filesystem::path root(temporaryDirectory.path().toStdWString());
    const std::filesystem::path nativePath = root / "large_workspace.sed";
    window.setProjectTypeForTest(ProjectType::JKDF2);
    window.setRecoveryDirectoryForTest(root / "recovery");
    window.beginRecoveryDocumentForTest(nativePath, false);
    QAction* const newStringTable = window.findChild<QAction*>(
        QStringLiteral("edit.resources.string_table_uni"));
    check(newStringTable != nullptr,
        "resource workspace hot path: String Table New command is available");
    if ( !newStringTable )
    {
        return;
    }
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::stringTable);
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"));
    QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
    if ( !page )
    {
        check(false, "resource workspace hot path: large String Table pane is available");
        return;
    }

    QCborMap largeState = ui::resources::captureResourceAuthoringPaneState(page);
    largeState.remove(QStringLiteral("history")); // The synthetic 4096 rows replace the authored baseline.
    QCborArray largeRows{};
    constexpr int kLargeRowCount = 4096;
    for ( int row = 0; row < kLargeRowCount; ++row )
    {
        QCborArray cells{};
        cells.append(QStringLiteral("KEY_%1").arg(row));
        cells.append(QStringLiteral("text_%1").arg(row));
        cells.append(QString::number(row));
        largeRows.append(cells);
    }
    largeState.insert(QStringLiteral("rows"), largeRows);
    largeState.insert(QStringLiteral("capacity"), kLargeRowCount);
    largeState.insert(QStringLiteral("modified"), false);
    QString restoreWarning{};
    const bool restoredLarge = ui::resources::restoreResourceAuthoringPaneState(
        page, largeState, &restoreWarning);
    QTableWidget* const table = page->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    check(restoredLarge && table && table->rowCount() == kLargeRowCount,
        "resource workspace hot path: exact 4096-row hidden-workspace fixture restores");
    if ( !restoredLarge || !table || table->rowCount() != kLargeRowCount )
    {
        if ( !restoreWarning.isEmpty() )
        {
            std::cerr << "resource workspace hot path restore warning: "
                      << restoreWarning.toStdString() << '\n';
        }
        return;
    }
    window.waitForRecoverySnapshotForTest();
    window.pauseRecoveryPublicationForTest(true);

    QDockWidget* const dock = resourceEditorsDock(window, ui::resources::ResourceAuthoringKind::stringTable);
    window.resize(920, 680);
    window.show();
    window.raise();
    window.activateWindow();
    if ( dock )
    {
        dock->show();
    }
    table->setCurrentCell(kLargeRowCount - 1, 1);
    table->scrollToItem(table->item(kLargeRowCount - 1, 1));
    table->editItem(table->item(kLargeRowCount - 1, 1));
    QApplication::processEvents();
    QPointer<QLineEdit> liveEditor = qobject_cast<QLineEdit*>(
        table->indexWidget(table->model()->index(kLargeRowCount - 1, 1)));
    check(liveEditor && liveEditor->text() == QStringLiteral("text_4095"),
        "resource workspace hot path: the final visible text cell has its own active delegate");
    if ( !liveEditor )
    {
        window.hide();
        return;
    }

    ui::resources::resetResourceAuthoringPaneTestCounters(page);
    window.resetRecoveryEditorStateCaptureCountForTest();
    liveEditor->setFocus(Qt::OtherFocusReason);
    typeIntoLineEdit(liveEditor, QStringLiteral("29x"));
    const ui::resources::ResourceAuthoringPaneTestCounters afterTyping =
        ui::resources::resourceAuthoringPaneTestCounters(page);
    const std::uint64_t editCapturesAfterTyping =
        window.recoveryEditorStateCaptureCountForTest();
    check(afterTyping.captureCalls == 0 && afterTyping.rowVisits == 0
            && editCapturesAfterTyping == 0,
        "resource workspace hot path: typing into one large-draft row performs no full row traversal or EDIT capture");

    if ( dock )
    {
        dock->hide();
    }
    ui::resources::resetResourceAuthoringPaneTestCounters(page);
    window.resetRecoveryEditorStateCaptureCountForTest();
    if ( GlView* const viewport = window.glViewForTest() )
    {
        viewport->setFocus(Qt::OtherFocusReason);
        QFocusEvent focusOut(QEvent::FocusOut, Qt::OtherFocusReason);
        QApplication::sendEvent(viewport, &focusOut);
        QKeyEvent keyRelease(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier);
        QApplication::sendEvent(viewport, &keyRelease);
        QMouseEvent mouseRelease(
            QEvent::MouseButtonRelease,
            QPointF(1.0, 1.0),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(viewport, &mouseRelease);
    }
    QApplication::processEvents();
    const ui::resources::ResourceAuthoringPaneTestCounters afterViewportEvents =
        ui::resources::resourceAuthoringPaneTestCounters(page);
    check(afterViewportEvents.captureCalls == 0 && afterViewportEvents.rowVisits == 0
            && window.recoveryEditorStateCaptureCountForTest() == 0,
        "resource workspace hot path: hidden large drafts remain unvisited on generic viewport focus, key, and mouse events");

    ui::resources::resetResourceAuthoringPaneTestCounters(page);
    window.resetRecoveryEditorStateCaptureCountForTest();
    const QByteArray forcedSnapshot = window.captureRecoveryEditorStateForTest();
    const ui::resources::ResourceAuthoringPaneTestCounters afterForcedSnapshot =
        ui::resources::resourceAuthoringPaneTestCounters(page);
    const QCborValue rootValue = QCborValue::fromCbor(forcedSnapshot);
    const QCborArray tabsValue = rootValue.isMap()
        ? rootValue.toMap().value(QStringLiteral("resourceEditors")).toMap()
            .value(QStringLiteral("tabs")).toArray()
        : QCborArray{};
    const QCborMap resourceTab = tabsValue.isEmpty() ? QCborMap{} : tabsValue.at(0).toMap();
    const QCborArray rawRows = resourceTab.value(QStringLiteral("page")).toMap()
        .value(QStringLiteral("rows")).toArray();
    const QCborArray lastCells = rawRows.isEmpty() ? QCborArray{} : rawRows.at(kLargeRowCount - 1).toArray();
    check(window.recoveryEditorStateCaptureCountForTest() == 1
            && afterForcedSnapshot.captureCalls == 1
            && afterForcedSnapshot.rowVisits == kLargeRowCount,
        "resource workspace hot path: an explicit semantic recovery snapshot performs one measured full traversal");
    check(lastCells.size() == 3 && lastCells.at(0).toString() == QStringLiteral("KEY_4095")
            && lastCells.at(1).toString() == QStringLiteral("29x")
            && lastCells.at(2).toString() == QStringLiteral("4095"),
        "resource workspace hot path: forced EDIT snapshot preserves the exact raw draft after deferred input");
    window.hide();
}

void checkResourceWorkspaceEscapeRespectsPinnedTab()
{
    MainWindow window(nullptr, true);
    QAction* const openSprite = window.findChild<QAction*>(QStringLiteral("edit.resources.sprite_spr"));
    if ( !openSprite )
    {
        check(false, "resource workspace Escape: Sprite command is available");
        return;
    }
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::sprite);
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("resourceEditorTabs"));
    QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
    QLineEdit* const material = page
        ? page->findChild<QLineEdit*>(QStringLiteral("spr.material")) : nullptr;
    if ( !tabs || !page || !material )
    {
        check(false, "resource workspace Escape: active Sprite tab is available");
        return;
    }
    material->setText(QStringLiteral("changed.mat"));
    page->setProperty("cogPinned", true);
    QPointer<QWidget> pinnedPage(page);
    window.show();
    QApplication::processEvents();
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    runWithModalDriver([&]() { QApplication::sendEvent(page, &escape); }, std::nullopt,
        QMessageBox::Discard);
    check(pinnedPage && tabs->indexOf(pinnedPage.data()) >= 0
            && tabs->currentWidget() == pinnedPage.data() && !pinnedPage->isHidden(),
        "resource workspace Escape: pinned pane remains open under tab-close policy");
    window.hide();
}

void checkResourceTabMenuRejectsStaleTabOrder()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);

    QAction* const openSprite = window.findChild<QAction*>(
        QStringLiteral("edit.resources.sprite_spr"));
    QAction* const openSound = window.findChild<QAction*>(
        QStringLiteral("edit.resources.sound_class_snd"));
    QAction* const openStrings = window.findChild<QAction*>(
        QStringLiteral("edit.resources.string_table_uni"));
    check(openSprite && openSound && openStrings,
        "resource tab menu lifetime: all resource pane commands are available");
    if ( !openSprite || !openSound || !openStrings )
    {
        return;
    }
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::sprite);
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::sprite);
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::sprite);

    QTabWidget* const tabs = window.findChild<QTabWidget*>(
        QStringLiteral("resourceEditorTabs"));
    QTabBar* const tabBar = tabs ? tabs->tabBar() : nullptr;
    check(tabs && tabBar && tabs->count() == 3,
        "resource tab menu lifetime: three sibling Sprite files are available");
    if ( !tabs || !tabBar || tabs->count() != 3 )
    {
        return;
    }

    for ( int index = 0; index < tabs->count(); ++index )
    {
        QWidget* const page = tabs->widget(index);
        QCborMap state = ui::resources::captureResourceAuthoringPaneState(page);
        state.insert(QStringLiteral("modified"), false);
        QString warning{};
        check(ui::resources::restoreResourceAuthoringPaneState(page, state, &warning),
            "resource tab menu lifetime: each tab starts from a clean close baseline");
    }

    window.show();
    const int originalContextIndex = 1;
    const QPointer<QWidget> contextPage(tabs->widget(originalContextIndex));
    const QPointer<QWidget> leftPage(tabs->widget(0));
    const QPointer<QWidget> rightPage(tabs->widget(2));
    const bool tabBarReady = waitUntil([&]()
    {
        return contextPage && tabBar->isVisible()
            && !tabBar->tabRect(originalContextIndex).isEmpty();
    });
    check(tabBarReady && contextPage,
        "resource tab menu lifetime: contextual tab is ready");
    if ( !tabBarReady || !contextPage )
    {
        return;
    }

    bool menuSeen = false;
    bool closeRightTriggered = false;
    bool tabOrderChangedWhileMenuOpen = false;
    QTimer menuDriver{};
    menuDriver.setInterval(5);
    QObject::connect(&menuDriver, &QTimer::timeout, &window, [&]()
    {
        auto* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if ( !menu )
        {
            return;
        }
        QAction* closeRight = nullptr;
        for ( QAction* const action : menu->actions() )
        {
            if ( action && action->text() == QStringLiteral("Close Tabs to the Right") )
            {
                closeRight = action;
                break;
            }
        }
        if ( !closeRight )
        {
            menu->close();
            return;
        }

        menuSeen = true;
        QObject::connect(closeRight, &QAction::triggered, &window, [&]()
        {
            closeRightTriggered = true;
        });
        tabBar->moveTab(2, 0);
        tabOrderChangedWhileMenuOpen = tabs->indexOf(contextPage.data()) == 2;

        const QRect actionRect = menu->actionGeometry(closeRight);
        const QPoint local = actionRect.center();
        const QPoint scene = menu->mapTo(menu->window(), local);
        const QPoint global = menu->mapToGlobal(local);
        QMouseEvent press(
            QEvent::MouseButtonPress,
            QPointF(local),
            QPointF(scene),
            QPointF(global),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QMouseEvent release(
            QEvent::MouseButtonRelease,
            QPointF(local),
            QPointF(scene),
            QPointF(global),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(menu, &press);
        QApplication::sendEvent(menu, &release);
        menuDriver.stop();
    });

    QTimer watchdog{};
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &window, []()
    {
        if ( QWidget* const popup = QApplication::activePopupWidget() )
        {
            popup->close();
        }
    });
    menuDriver.start();
    watchdog.start(2000);
    const QRect contextRect = tabBar->tabRect(originalContextIndex);
    const QPoint localPosition = contextRect.center();
    QContextMenuEvent contextEvent(
        QContextMenuEvent::Mouse,
        localPosition,
        tabBar->mapToGlobal(localPosition),
        Qt::NoModifier);
    QApplication::sendEvent(tabBar, &contextEvent);
    menuDriver.stop();
    watchdog.stop();

    const bool contextTabRetained = contextPage
        && tabs->indexOf(contextPage.data()) == 2;
    check(menuSeen && closeRightTriggered && tabOrderChangedWhileMenuOpen,
        "resource tab menu lifetime: Close Right action runs after the tab order changes");
    check(contextTabRetained && tabs->count() == 3
            && leftPage && rightPage
            && tabs->indexOf(leftPage.data()) >= 0
            && tabs->indexOf(rightPage.data()) >= 0,
        "resource tab menu lifetime: reordered context does not close a stale-index tab");
    window.hide();
}

void checkResourceTabCloseResolvesPageAfterPromptReorder()
{
    MainWindow window(nullptr, true);
    QAction* const openSprite = window.findChild<QAction*>(
        QStringLiteral("edit.resources.sprite_spr"));
    QAction* const openSound = window.findChild<QAction*>(
        QStringLiteral("edit.resources.sound_class_snd"));
    check(openSprite && openSound,
        "resource tab close lifetime: Sprite and Sound commands are available for independent resource groups");
    if ( !openSprite || !openSound )
    {
        return;
    }
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::sprite);
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::sprite);

    QTabWidget* const tabs = window.findChild<QTabWidget*>(
        QStringLiteral("resourceEditorTabs"));
    const QPointer<QWidget> target(
        tabs && tabs->count() > 0 ? tabs->widget(0) : nullptr);
    const QPointer<QWidget> neighbor(
        tabs && tabs->count() > 1 ? tabs->widget(1) : nullptr);
    QLineEdit* const material = target
        ? target->findChild<QLineEdit*>(QStringLiteral("spr.material")) : nullptr;
    check(tabs && tabs->count() == 2 && target && neighbor && material,
        "resource tab close lifetime: target and neighboring pages are available");
    if ( !tabs || !target || !neighbor || !material )
    {
        return;
    }
    material->setText(QStringLiteral("unsaved-before-tab-reorder.mat"));
    window.show();

    bool promptSeen = false;
    bool reorderedDuringPrompt = false;
    bool closeSignalInvoked = false;
    runWithModalDriver(
        [&]()
        {
            closeSignalInvoked = QMetaObject::invokeMethod(
                tabs,
                "tabCloseRequested",
                Qt::DirectConnection,
                Q_ARG(int, 0));
        },
        std::nullopt,
        QMessageBox::Discard,
        false,
        {},
        {},
        [&](QMessageBox* const prompt)
        {
            if ( prompt->objectName() != QStringLiteral("resource_authoring.unsaved_prompt") )
            {
                return;
            }
            promptSeen = true;
            tabs->tabBar()->moveTab(0, 1);
            reorderedDuringPrompt = tabs->widget(1) == target.data()
                && tabs->widget(0) == neighbor.data();
        });

    check(closeSignalInvoked && promptSeen && reorderedDuringPrompt,
        "resource tab close lifetime: the tab order changes during Discard confirmation");
    check(tabs->count() == 1 && tabs->widget(0) == neighbor.data()
            && tabs->indexOf(target.data()) < 0,
        "resource tab close lifetime: close removes the confirmed page after its index moves");
    window.hide();
}

void checkResourceTabMenuIgnoresRemovedContextForPin()
{
    MainWindow window(nullptr, true);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    QAction* const openSprite = window.findChild<QAction*>(
        QStringLiteral("edit.resources.sprite_spr"));
    QAction* const openSound = window.findChild<QAction*>(
        QStringLiteral("edit.resources.sound_class_snd"));
    QAction* const openStrings = window.findChild<QAction*>(
        QStringLiteral("edit.resources.string_table_uni"));
    check(openSprite && openSound && openStrings,
        "resource tab menu lifetime: all resource pane commands are available for Pin test");
    if ( !openSprite || !openSound || !openStrings )
    {
        return;
    }
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::sprite);
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::sprite);
    createResourceDraft(window, ui::resources::ResourceAuthoringKind::sprite);

    QTabWidget* const tabs = window.findChild<QTabWidget*>(
        QStringLiteral("resourceEditorTabs"));
    QTabBar* const tabBar = tabs ? tabs->tabBar() : nullptr;
    check(tabs && tabBar && tabs->count() == 3,
        "resource tab menu lifetime: three pages are available before Pin test");
    if ( !tabs || !tabBar || tabs->count() != 3 )
    {
        return;
    }
    for ( int index = 0; index < tabs->count(); ++index )
    {
        QWidget* const page = tabs->widget(index);
        QCborMap state = ui::resources::captureResourceAuthoringPaneState(page);
        state.insert(QStringLiteral("modified"), false);
        QString warning{};
        check(ui::resources::restoreResourceAuthoringPaneState(page, state, &warning),
            "resource tab menu lifetime: Pin pages start from clean close baselines");
    }

    constexpr int contextIndex = 1;
    const QPointer<QWidget> contextPage(tabs->widget(contextIndex));
    check(contextPage && !contextPage->property("cogPinned").toBool(),
        "resource tab menu lifetime: context page starts unpinned");
    if ( !contextPage )
    {
        return;
    }
    window.show();
    const bool tabBarReady = waitUntil([&]()
    {
        return tabBar->isVisible() && !tabBar->tabRect(contextIndex).isEmpty();
    });
    check(tabBarReady,
        "resource tab menu lifetime: Pin context tab is visible before opening its menu");
    if ( !tabBarReady )
    {
        return;
    }

    bool menuSeen = false;
    bool pinTriggered = false;
    bool contextRemovedWhileMenuOpen = false;
    QTimer menuDriver{};
    menuDriver.setInterval(5);
    QObject::connect(&menuDriver, &QTimer::timeout, &window, [&]()
    {
        auto* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if ( !menu )
        {
            return;
        }
        QAction* pin = nullptr;
        for ( QAction* const action : menu->actions() )
        {
            if ( action && action->text() == QStringLiteral("Pin Tab") )
            {
                pin = action;
                break;
            }
        }
        if ( !pin )
        {
            menu->close();
            return;
        }

        menuSeen = true;
        QObject::connect(pin, &QAction::triggered, &window, [&]()
        {
            pinTriggered = true;
        });
        const int liveContextIndex = tabs->indexOf(contextPage.data());
        if ( liveContextIndex >= 0 )
        {
            tabs->removeTab(liveContextIndex);
        }
        contextRemovedWhileMenuOpen = tabs->indexOf(contextPage.data()) < 0;

        const QRect actionRect = menu->actionGeometry(pin);
        const QPoint local = actionRect.center();
        const QPoint scene = menu->mapTo(menu->window(), local);
        const QPoint global = menu->mapToGlobal(local);
        QMouseEvent press(
            QEvent::MouseButtonPress,
            QPointF(local),
            QPointF(scene),
            QPointF(global),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QMouseEvent release(
            QEvent::MouseButtonRelease,
            QPointF(local),
            QPointF(scene),
            QPointF(global),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(menu, &press);
        QApplication::sendEvent(menu, &release);
        menuDriver.stop();
    });

    QTimer watchdog{};
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &window, []()
    {
        if ( QWidget* const popup = QApplication::activePopupWidget() )
        {
            popup->close();
        }
    });
    menuDriver.start();
    watchdog.start(2000);
    const QPoint localPosition = tabBar->tabRect(contextIndex).center();
    QContextMenuEvent contextEvent(
        QContextMenuEvent::Mouse,
        localPosition,
        tabBar->mapToGlobal(localPosition),
        Qt::NoModifier);
    QApplication::sendEvent(tabBar, &contextEvent);
    menuDriver.stop();
    watchdog.stop();

    check(menuSeen && pinTriggered && contextRemovedWhileMenuOpen,
        "resource tab menu lifetime: Pin is selected after its context tab is removed");
    check(contextPage && tabs->indexOf(contextPage.data()) < 0
            && !contextPage->property("cogPinned").toBool(),
        "resource tab menu lifetime: removed context page is not pinned by a stale menu");
    window.hide();
}

void checkFailedResourceSaveConsumesNativeSave()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace: temporary directory is available for save-routing regression");
    if (!temporaryDirectory.isValid())
    {
        return;
    }

    MainWindow window(nullptr, true);
    window.setProjectTypeForTest(ProjectType::JKDF2);
    const std::filesystem::path nativePath =
        std::filesystem::path(temporaryDirectory.path().toStdWString()) / "must_not_save.sed";
    editor::DocumentState documentState{};
    documentState.level.kind = ProjectType::JKDF2;
    documentState.level.header.version = 1;
    documentState.level.layers = { "Base" };
    window.setDocumentStateForTest(std::move(documentState));
    window.setCurrentPathForTitleForTest(nativePath);
    QAction* const stringTableAction = window.findChild<QAction*>(
        QStringLiteral("edit.resources.string_table_uni"));
    QAction* const nativeSave = window.findChild<QAction*>(QStringLiteral("file.save"));
    check(stringTableAction && nativeSave,
        "resource workspace: resource New and native Save commands are available");
    if (!stringTableAction || !nativeSave)
    {
        return;
    }

    createResourceDraft(window, ui::resources::ResourceAuthoringKind::stringTable);
    QTabWidget* const tabs = window.findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"));
    QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
    check(page && addInvalidStringNumber(page),
        "resource workspace: invalid raw UNI cell can be used as the failed-save trigger");
    if (!page)
    {
        return;
    }

    QTableWidget* const table = page->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    QDockWidget* const dock = resourceEditorsDock(window, ui::resources::ResourceAuthoringKind::stringTable);
    window.setAnimated(false);
    if ( QMainWindow* const workspace = window.findChild<QMainWindow*>(QStringLiteral("workspaceWindow")) )
    {
        workspace->setAnimated(false);
    }
    window.resize(920, 680);
    window.show();
    if (dock)
    {
        dock->show();
        dock->raise();
    }
    if (tabs)
    {
        tabs->setCurrentWidget(page);
    }
    window.raise();
    window.activateWindow();
    QApplication::processEvents();
    const bool pageRemainsEmbedded = !page->isWindow() && page->window() == &window;
    check(pageRemainsEmbedded,
        "resource workspace: polished pane remains a child of the MainWindow dock");
    if ( !pageRemainsEmbedded )
    {
        const QWidget* const pageWindow = page->window();
        std::cerr << "resource pane attachment diagnostic: pane="
                  << static_cast<const void*>(page)
                  << " paneIsWindow=" << page->isWindow()
                  << " paneFlags=" << static_cast<int>(page->windowFlags())
                  << " paneWindow=" << static_cast<const void*>(pageWindow)
                  << " paneWindowClass="
                  << (pageWindow ? pageWindow->metaObject()->className() : "<null>")
                  << " paneWindowName="
                  << (pageWindow ? pageWindow->objectName().toStdString() : "<null>")
                  << " mainWindow=" << static_cast<const void*>(&window)
                  << " dock=" << static_cast<const void*>(dock)
                  << " dockVisible=" << (dock && dock->isVisible()) << '\n';
        window.hide();
        return;
    }
    const bool windowActivated = waitUntil([&]()
        {
            return window.isVisible() && window.isActiveWindow();
        });
    check(windowActivated,
        "resource workspace: failed-save fixture activates its visible MainWindow");
    const bool dockVisible = dock && waitUntil([&]() { return dock->isVisible(); });
    check(dockVisible,
        "resource workspace: resource editor dock is visible before routing focus is requested");
    QSize previousTableSize{};
    int stableLayoutPolls = 0;
    const bool tableLaidOut = window.isVisible() && dockVisible && table
        && waitUntil([&]()
            {
                const QSize currentSize = table->size();
                if ( table->isVisible() && !currentSize.isEmpty() && currentSize == previousTableSize )
                {
                    ++stableLayoutPolls;
                }
                else
                {
                    stableLayoutPolls = 0;
                    previousTableSize = currentSize;
                }
                return table->isVisible() && stableLayoutPolls >= 2;
            });
    check(tableLaidOut,
        "resource workspace: resource table is visible and laid out before routing focus is requested");
    if ( tableLaidOut )
    {
        table->setFocus(Qt::OtherFocusReason);
    }
    const bool resourceOwnsFocus = tableLaidOut && tabs
        && waitUntil([&]()
            {
                const QWidget* const focused = QApplication::focusWidget();
                return focused && tabs->isAncestorOf(focused);
            });
    const QWidget* const focusedWidget = QApplication::focusWidget();
    check(resourceOwnsFocus && focusedWidget,
        "resource workspace: failed-save routing fixture has actual focus inside the resource tab");
    if ( !resourceOwnsFocus )
    {
        std::cerr << "resource routing focus diagnostic: mainVisible=" << window.isVisible()
                  << " mainActive=" << window.isActiveWindow()
                  << " activeWindow=" << static_cast<const void*>(QApplication::activeWindow())
                  << " activeWindowName="
                  << (QApplication::activeWindow()
                          ? QApplication::activeWindow()->objectName().toStdString()
                          : "<null>")
                  << " dock=" << static_cast<const void*>(dock)
                  << " dockVisible=" << (dock && dock->isVisible())
                  << " dockSize=" << (dock ? dock->width() : 0) << 'x'
                  << (dock ? dock->height() : 0)
                  << " tabs=" << static_cast<const void*>(tabs)
                  << " currentPage=" << static_cast<const void*>(tabs ? tabs->currentWidget() : nullptr)
                  << " pageSize=" << (page ? page->width() : 0) << 'x'
                  << (page ? page->height() : 0)
                  << " table=" << static_cast<const void*>(table)
                  << " tableVisible=" << (table && table->isVisible())
                  << " tableEnabled=" << (table && table->isEnabled())
                  << " tableFocusPolicy=" << (table ? static_cast<int>(table->focusPolicy()) : -1)
                  << " currentCell=" << (table ? table->currentRow() : -1) << ','
                  << (table ? table->currentColumn() : -1)
                  << " tableSize=" << (table ? table->width() : 0) << 'x'
                  << (table ? table->height() : 0)
                  << " focus=" << static_cast<const void*>(focusedWidget)
                  << " focusClass=" << (focusedWidget ? focusedWidget->metaObject()->className() : "<null>")
                  << " focusName=" << (focusedWidget ? focusedWidget->objectName().toStdString() : "<null>")
                  << " activeModal=" << static_cast<const void*>(QApplication::activeModalWidget())
                  << " activeModalName="
                  << (QApplication::activeModalWidget()
                          ? QApplication::activeModalWidget()->objectName().toStdString()
                          : "<null>")
                  << " nativeSaveEnabled=" << nativeSave->isEnabled() << '\n';
    }
    if (!resourceOwnsFocus || !nativeSave->isEnabled())
    {
        check(nativeSave->isEnabled(),
            "resource workspace: native Save action is enabled before the routing trigger");
        window.hide();
        return;
    }
    check(nativeSave->isEnabled(),
        "resource workspace: native Save action is enabled before the routing trigger");
    bool sawResourceValidation = false;
    runWithModalDriver(
        [nativeSave]() { nativeSave->trigger(); },
        std::nullopt,
        QMessageBox::Ok,
        false,
        {},
        {},
        [&sawResourceValidation](QMessageBox* message)
        {
            if ( message->objectName() == QStringLiteral("resource_authoring.error") )
            {
                sawResourceValidation = message->windowTitle()
                    == QStringLiteral("Resource validation failed");
            }
        });
    std::error_code error{};
    const bool nativeFileAbsent = !std::filesystem::exists(nativePath, error) && !error;
    check(sawResourceValidation,
        "resource workspace: focused failed resource Save displays the resource validation error");
    check(sawResourceValidation && nativeFileAbsent,
        "resource workspace: failed resource Ctrl+S ownership cannot fall through to native Level Save");
    window.hide();
}

void checkIJimQuotedStringTableOpensWithSourceSpelling()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace: temporary IJIM quoted-table directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }

    const std::filesystem::path root(temporaryDirectory.path().toStdWString());
    const std::filesystem::path tablePath = root / "jonesstrings.uni";
    constexpr std::string_view tableText =
        "MSGS 1\n\n\"MixedCaseKey\" 23 \"Text with \"quoted\" words inside\"\n\nEND\n";
    check(writeBytes(tablePath, tableText),
        "resource workspace: IJIM quoted UNI fixture writes");

    ui::resources::ResourceAuthoringContext context = testContext(ProjectType::IJIM);
    auto editor = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        std::move(context)));
    if ( !editor )
    {
        check(false, "resource workspace: IJIM String Table editor is available");
        return;
    }

    bool sawOpenFailure = false;
    QAbstractButton* const open = button(editor.get(), "resource_authoring.open");
    check(open != nullptr, "resource workspace: IJIM String Table exposes Open");
    if ( !open )
    {
        return;
    }

    runWithModalDriver(
        [open]() { open->click(); },
        tablePath,
        QMessageBox::Ok,
        false,
        {},
        {},
        [&](QMessageBox*) { sawOpenFailure = true; });

    auto* const rows = editor->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    const bool preservedSourceSpelling = rows
        && rows->rowCount() == 1
        && rows->item(0, 0)
        && rows->item(0, 1)
        && rows->item(0, 0)->text() == QStringLiteral("MixedCaseKey")
        && rows->item(0, 1)->text() == QStringLiteral("Text with \"quoted\" words inside");
    check(!sawOpenFailure && preservedSourceSpelling,
        "resource workspace: IJIM UNI keeps key case and literal quotes inside message text");
}

void checkResourceWorkspaceSpritePendingNumericDraftRoundTrip()
{
    const auto createPane = []()
    {
        return std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
            nullptr,
            ui::resources::ResourceAuthoringKind::sprite,
            testContext(ProjectType::JKDF2)));
    };
    auto source = createPane();
    auto* const width = source
        ? source->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"))
        : nullptr;
    check(width && width->lineEdit(),
        "resource workspace SPR pending draft: width scrub control is available");
    if ( !width || !width->lineEdit() )
    {
        return;
    }

    source->show();
    QApplication::processEvents(QEventLoop::AllEvents);
    const float canonicalWidth = static_cast<float>(width->value());
    const auto typeWithoutCommit = [](QLineEdit* const edit, const QString& text)
    {
        if ( !edit )
        {
            return;
        }
        edit->setFocus();
        edit->selectAll();
        if ( text.isEmpty() )
        {
            QKeyEvent press(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
            QApplication::sendEvent(edit, &press);
            QKeyEvent release(QEvent::KeyRelease, Qt::Key_Backspace, Qt::NoModifier);
            QApplication::sendEvent(edit, &release);
            return;
        }
        for ( const QChar character : text )
        {
            const int key = character == QLatin1Char('.')
                ? Qt::Key_Period
                : Qt::Key_0 + character.digitValue();
            const QString keyText(character);
            QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, keyText);
            QApplication::sendEvent(edit, &press);
            QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, keyText);
            QApplication::sendEvent(edit, &release);
        }
    };
    typeWithoutCommit(width->lineEdit(), QStringLiteral("0.01"));
    const QCborMap validDraft = ui::resources::captureResourceAuthoringPaneState(source.get());
    const QCborMap validPending = validDraft.value(
        QStringLiteral("spritePendingNumericEdits")).toMap();
    const QCborArray validCanonical = validDraft.value(QStringLiteral("sprite")).toArray();
    bool canonicalValid = false;
    const float capturedCanonical = validCanonical.size() > 2
        ? validCanonical.at(2).toString().toFloat(&canonicalValid)
        : 0.0f;
    check(width->lineEdit()->text() == QStringLiteral("0.01")
            && validPending.value(QStringLiteral("width")).toString() == QStringLiteral("0.01")
            && canonicalValid
            && std::bit_cast<std::uint32_t>(capturedCanonical)
                == std::bit_cast<std::uint32_t>(canonicalWidth),
        "resource workspace SPR pending draft: valid uncommitted text is captured exactly without changing the finite model value");

    auto restored = createPane();
    QString restoreWarning{};
    const bool restoredValid = restored
        && ui::resources::restoreResourceAuthoringPaneState(
            restored.get(), validDraft, &restoreWarning);
    auto* const restoredWidth = restored
        ? restored->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"))
        : nullptr;
    check(restoredValid && restoredWidth && restoredWidth->lineEdit()->text() == QStringLiteral("0.01"),
        "resource workspace SPR pending draft: valid uncommitted text restores verbatim");

    typeWithoutCommit(width->lineEdit(), QStringLiteral("."));
    const QCborMap invalidDraft = ui::resources::captureResourceAuthoringPaneState(source.get());
    const QCborMap invalidPending = invalidDraft.value(
        QStringLiteral("spritePendingNumericEdits")).toMap();
    auto restoredInvalid = createPane();
    QString invalidWarning{};
    const bool restoredIntermediate = restoredInvalid
        && ui::resources::restoreResourceAuthoringPaneState(
            restoredInvalid.get(), invalidDraft, &invalidWarning);
    auto* const invalidWidth = restoredInvalid
        ? restoredInvalid->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"))
        : nullptr;
    check(width->lineEdit()->text() == QStringLiteral(".")
            && invalidPending.value(QStringLiteral("width")).toString() == QStringLiteral(".")
            && restoredIntermediate && invalidWidth
            && invalidWidth->lineEdit()->text() == QStringLiteral("."),
        "resource workspace SPR pending draft: invalid intermediate text survives native capture and restore without entering the canonical model");

    typeWithoutCommit(width->lineEdit(), QString{});
    const QCborMap emptyDraft = ui::resources::captureResourceAuthoringPaneState(source.get());
    auto restoredEmpty = createPane();
    QString emptyWarning{};
    const bool restoredEmptyText = restoredEmpty
        && ui::resources::restoreResourceAuthoringPaneState(
            restoredEmpty.get(), emptyDraft, &emptyWarning);
    auto* const emptyWidth = restoredEmpty
        ? restoredEmpty->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"))
        : nullptr;
    check(emptyDraft.value(QStringLiteral("spritePendingNumericEdits")).toMap()
                .value(QStringLiteral("width")).toString().isEmpty()
            && restoredEmptyText && emptyWidth && emptyWidth->lineEdit()->text().isEmpty(),
        "resource workspace SPR pending draft: empty numeric text remains a valid bounded intermediate edit");
}

void checkResourceWorkspaceSoundLoopPresentationRoundTrip()
{
    QTemporaryDir temporaryDirectory{};
    check(temporaryDirectory.isValid(),
        "resource workspace SND preview state: temporary directory is available");
    if ( !temporaryDirectory.isValid() )
    {
        return;
    }
    const std::filesystem::path sourcePath =
        std::filesystem::path(temporaryDirectory.path().toStdWString()) / "preview.snd";
    constexpr std::string_view sourceText =
        "moving first.wav 0x00000040 0.5 2.5 1\n"
        "create second.wav 0x00000040 0.25 1.5 0.75\n";
    check(writeBytes(sourcePath, sourceText),
        "resource workspace SND preview state: fixed two-row source writes");

    const auto createPane = []()
    {
        return std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
            nullptr,
            ui::resources::ResourceAuthoringKind::soundClass,
            testContext(ProjectType::JKDF2)));
    };
    auto source = createPane();
    if ( !source )
    {
        check(false, "resource workspace SND preview state: source pane is available");
        return;
    }
    openResourceFile(source.get(), sourcePath);
    QTreeWidget* const sourceTree = source->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    QTreeWidgetItem* const first = soundAlternativeByWav(sourceTree, QStringLiteral("first.wav"));
    QTreeWidgetItem* const second = soundAlternativeByWav(sourceTree, QStringLiteral("second.wav"));
    check(first && second && !ui::resources::resourceAuthoringPaneIsModified(source.get()),
        "resource workspace SND preview state: clean source rows are available");
    if ( !first || !second )
    {
        return;
    }
    (void)setSoundPreviewLoop(sourceTree, first, true);
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(source.get());
    const QCborArray loops = captured.value(QStringLiteral("soundPreviewLoops")).toArray();
    check(loops.size() == 2 && loops.at(0).isBool() && loops.at(0).toBool()
            && loops.at(1).isBool() && !loops.at(1).toBool()
            && !ui::resources::resourceAuthoringPaneIsModified(source.get()),
        "resource workspace SND preview state: loop preview choices persist by source row without changing authored content");

    auto restored = createPane();
    QString warning{};
    const bool restoredOk = restored
        && ui::resources::restoreResourceAuthoringPaneState(restored.get(), captured, &warning);
    QTreeWidget* const restoredTree = restored
        ? restored->findChild<QTreeWidget*>(QStringLiteral("snd.entries"))
        : nullptr;
    QTreeWidgetItem* const restoredFirst =
        soundAlternativeByWav(restoredTree, QStringLiteral("first.wav"));
    QTreeWidgetItem* const restoredSecond =
        soundAlternativeByWav(restoredTree, QStringLiteral("second.wav"));
    check(restoredOk && restoredFirst && restoredSecond
            && restoredFirst->data(0, Qt::UserRole + 24).toBool()
            && !restoredSecond->data(0, Qt::UserRole + 24).toBool()
            && !ui::resources::resourceAuthoringPaneIsModified(restored.get()),
        "resource workspace SND preview state: loop choices restore without starting playback or dirtying the page");

    if ( restored )
    {
        QCborMap malformed = captured;
        QCborArray invalidLoops{};
        invalidLoops.append(true);
        malformed.insert(QStringLiteral("soundPreviewLoops"), invalidLoops);
        const QCborMap before = ui::resources::captureResourceAuthoringPaneState(restored.get());
        QString malformedWarning{};
        check(!ui::resources::restoreResourceAuthoringPaneState(
                    restored.get(), malformed, &malformedWarning)
                && !malformedWarning.isEmpty()
                && ui::resources::captureResourceAuthoringPaneState(restored.get()) == before,
            "resource workspace SND preview state: malformed loop vector rejects atomically");
    }

    auto legacy = createPane();
    QCborMap legacyState = captured;
    legacyState.remove(QStringLiteral("history")); // Older pane data predates the native document journal.
    legacyState.remove(QStringLiteral("soundPreviewLoops"));
    QString legacyWarning{};
    const bool restoredLegacy = legacy
        && ui::resources::restoreResourceAuthoringPaneState(legacy.get(), legacyState, &legacyWarning);
    QTreeWidget* const legacyTree = legacy
        ? legacy->findChild<QTreeWidget*>(QStringLiteral("snd.entries"))
        : nullptr;
    QTreeWidgetItem* const legacyFirst =
        soundAlternativeByWav(legacyTree, QStringLiteral("first.wav"));
    QTreeWidgetItem* const legacySecond =
        soundAlternativeByWav(legacyTree, QStringLiteral("second.wav"));
    check(restoredLegacy && legacyFirst && legacySecond
            && !legacyFirst->data(0, Qt::UserRole + 24).toBool()
            && !legacySecond->data(0, Qt::UserRole + 24).toBool(),
        "resource workspace SND preview state: older pane data defaults missing loop choices to off");
}

void checkSpritePendingUndoAndSourceReplacement()
{
    QTemporaryDir directory{};
    check(directory.isValid(), "resource workspace SPR pending boundary: temporary directory is available");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    const std::filesystem::path tiny = root / "tiny.spr";
    const std::filesystem::path replacement = root / "replacement.spr";
    check(writeBytes(tiny, "tiny.mat 0 1.23456789e-12 2 4 3 0 0 0 0 0\n") &&
              writeBytes(replacement, "replacement.mat 0 7.25 8.5 4 3 0 0 0 0 0\n"),
          "resource workspace SPR pending boundary: synthetic sources write");
    auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr, ui::resources::ResourceAuthoringKind::sprite, testContext(ProjectType::JKDF2)));
    if ( !pane )
    {
        check(false, "resource workspace SPR pending boundary: pane is available");
        return;
    }
    openResourceFile(pane.get(), tiny);
    const QCborMap baseline = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborArray baselineSprite = baseline.value(QStringLiteral("sprite")).toArray();
    ui::ScrubFloatEdit* const width = pane->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width"));
    const QPointer<ui::ScrubFloatEdit> guardedWidth(width);
    if ( !width || baselineSprite.size() < 3 )
    {
        check(false, "resource workspace SPR pending boundary: width and canonical source are available");
        return;
    }
    pane->show();
    width->lineEdit()->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
    const QString initialText = width->lineEdit()->text();
    typeIntoLineEdit(width->lineEdit(), QStringLiteral("29.375"));
    QKeyEvent undo(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
    QApplication::sendEvent(width->lineEdit(), &undo);
    const QCborMap reverted = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborArray revertedSprite = reverted.value(QStringLiteral("sprite")).toArray();
    check(width->lineEdit()->text() == initialText &&
              reverted.value(QStringLiteral("spritePendingNumericEdits")).isUndefined() &&
              revertedSprite.size() == baselineSprite.size() && revertedSprite.at(2) == baselineSprite.at(2),
          "resource workspace SPR pending boundary: text Undo clears the sparse draft and retains untouched tiny "
          "canonical float bits");
    width->lineEdit()->clearFocus();
    QApplication::processEvents(QEventLoop::AllEvents);
    const QCborMap unfocused = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborArray unfocusedSprite = unfocused.value(QStringLiteral("sprite")).toArray();
    check(unfocusedSprite.size() == baselineSprite.size() && unfocusedSprite.at(2) == baselineSprite.at(2) &&
              ui::resources::resourceAuthoringPaneSave(pane.get(), false),
          "resource workspace SPR pending boundary: focusing away after text Undo preserves the tiny canonical float "
          "through external Save");
    for ( const int navigationKey : {Qt::Key_Tab, Qt::Key_Left} )
    {
        QString navigationWarning{};
        check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), baseline, &navigationWarning),
              "resource workspace SPR pending boundary: tiny source resets for navigation after Undo");
        width->lineEdit()->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents(QEventLoop::AllEvents);
        typeIntoLineEdit(width->lineEdit(), QStringLiteral("29.375"));
        QKeyEvent navigationUndo(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
        QApplication::sendEvent(width->lineEdit(), &navigationUndo);
        QKeyEvent navigation(QEvent::KeyPress, navigationKey, Qt::NoModifier);
        QApplication::sendEvent(width->lineEdit(), &navigation);
        if ( navigationKey == Qt::Key_Left )
        {
            width->lineEdit()->clearFocus();
        }
        QApplication::processEvents(QEventLoop::AllEvents);
        const QCborMap afterNavigation = ui::resources::captureResourceAuthoringPaneState(pane.get());
        const QCborArray afterSprite = afterNavigation.value(QStringLiteral("sprite")).toArray();
        check(afterSprite.size() == baselineSprite.size() && afterSprite.at(2) == baselineSprite.at(2) &&
                  afterNavigation.value(QStringLiteral("spritePendingNumericEdits")).isUndefined(),
              navigationKey == Qt::Key_Tab
                  ? "resource workspace SPR pending boundary: Tab after native Undo retains tiny canonical float bits"
                  : "resource workspace SPR pending boundary: cursor navigation then focus-out after native Undo "
                    "retains tiny canonical float bits");
    }
    QCborMap pending = baseline;
    pending.remove(QStringLiteral("history")); // Seed an independently restored pending numeric draft.
    pending.remove(QStringLiteral("editorPresentation"));
    QCborMap numeric{};
    numeric.insert(QStringLiteral("width"), QStringLiteral("29.375"));
    pending.insert(QStringLiteral("spritePendingNumericEdits"), numeric);
    pending.insert(QStringLiteral("modified"), true);
    QString warning{};
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), pending, &warning),
          "resource workspace SPR pending boundary: restored raw edit is available before source replacement");
    runWithModalDriver(
        [&]()
        {
            (void)ui::resources::resourceAuthoringPaneOpenLooseFile(pane.get());
        },
        replacement, QMessageBox::Discard);
    const QCborMap opened = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborArray openedSprite = opened.value(QStringLiteral("sprite")).toArray();
    std::cerr << "SPR source replacement diagnostic: path="
              << opened.value(QStringLiteral("path")).toString().toStdString()
              << " expected=" << pathForDialog(replacement).toStdString()
              << " pendingAbsent=" << opened.value(QStringLiteral("spritePendingNumericEdits")).isUndefined()
              << " material=" << (openedSprite.size() > 0 ? openedSprite.at(0).toString().toStdString() : "missing")
              << " width=" << (openedSprite.size() > 2 ? openedSprite.at(2).toString().toStdString() : "missing")
              << " status="
              << pane->findChild<QLabel*>(QStringLiteral("resource_authoring.status"))->text().toStdString() << '\n';
    check(opened.value(QStringLiteral("path")).toString() == QDir::fromNativeSeparators(pathForDialog(replacement)) &&
              opened.value(QStringLiteral("spritePendingNumericEdits")).isUndefined() && openedSprite.size() >= 4 &&
              openedSprite.at(0).toString() == QStringLiteral("replacement.mat") &&
              openedSprite.at(2).toString() == QStringLiteral("7.25"),
          "resource workspace SPR pending boundary: discarding then opening another source clears the prior numeric "
          "draft");

    QCborMap restoredZero = baseline;
    restoredZero.remove(QStringLiteral("history")); // This native pending-zero fixture defines its own baseline.
    restoredZero.remove(QStringLiteral("editorPresentation"));
    QCborMap zeroDraft{};
    zeroDraft.insert(QStringLiteral("width"), QStringLiteral("0"));
    restoredZero.insert(QStringLiteral("spritePendingNumericEdits"), zeroDraft);
    std::cerr << "[SPR zero boundary] restore pending zero: start widthAlive=" << static_cast<bool>(guardedWidth)
              << '\n';
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), restoredZero, &warning),
          "resource workspace SPR pending boundary: restored explicit zero draft is admitted");
    std::cerr << "[SPR zero boundary] restore pending zero: done\n";
    const QCborMap restoredZeroCapture = ui::resources::captureResourceAuthoringPaneState(pane.get());
    std::cerr << "[SPR zero boundary] capture pending zero: done\n";
    check(restoredZeroCapture.value(QStringLiteral("spritePendingNumericEdits"))
                      .toMap()
                      .value(QStringLiteral("width"))
                      .toString() == QStringLiteral("0") &&
              restoredZeroCapture.value(QStringLiteral("sprite")).toArray().at(2) == baselineSprite.at(2),
          "resource workspace SPR pending boundary: restored zero retains its authored raw intent while the canonical "
          "tiny float remains uncommitted");
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), baseline, &warning),
          "resource workspace SPR pending boundary: tiny source resets before explicit typing");
    std::cerr << "[SPR zero boundary] restore canonical baseline: done widthAlive=" << static_cast<bool>(guardedWidth)
              << '\n';
    check(guardedWidth,
          "resource workspace SPR pending boundary: width editor remains available for explicit zero input");
    if ( !guardedWidth )
    {
        return;
    }
    pane->show();
    width->lineEdit()->setFocus(Qt::OtherFocusReason);
    std::cerr << "[SPR zero boundary] focus width: done\n";
    typeIntoLineEdit(width->lineEdit(), QStringLiteral("0"));
    std::cerr << "[SPR zero boundary] type zero: done\n";
    const QCborMap typedZero = ui::resources::captureResourceAuthoringPaneState(pane.get());
    QKeyEvent commitZero(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(width->lineEdit(), &commitZero);
    std::cerr << "[SPR zero boundary] commit zero: done\n";
    const QCborMap committedZero = ui::resources::captureResourceAuthoringPaneState(pane.get());
    check(typedZero.value(QStringLiteral("spritePendingNumericEdits"))
                      .toMap()
                      .value(QStringLiteral("width"))
                      .toString() == QStringLiteral("0") &&
              typedZero.value(QStringLiteral("sprite")).toArray().at(2) == baselineSprite.at(2) &&
              committedZero.value(QStringLiteral("sprite")).toArray().at(2).toString() == QStringLiteral("0"),
          "resource workspace SPR pending boundary: explicitly typing zero authors zero even when the tiny original "
          "display rounds to zero");
    std::cerr << "[SPR zero boundary] destroy focused pane: start\n";
    pane.reset();
    check(guardedWidth.isNull(), "resource workspace SPR pending boundary: focused numeric controls destroy safely");
    std::cerr << "[SPR zero boundary] destroy focused pane: done\n";
}

void checkFocusedSpritePaneDestruction()
{
    for ( const bool liveScrub : {false, true} )
    {
        QPointer<QWidget> pane;
        int publications = 0;
        QWidget* const created = ui::resources::createResourceAuthoringPane(
            nullptr, ui::resources::ResourceAuthoringKind::sprite, testContext(),
            [&pane, &publications](QWidget* changed)
            {
                if ( pane && pane.data() == changed )
                {
                    ++publications;
                }
            });
        pane = created;
        ui::ScrubFloatEdit* const width =
            created ? created->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.width")) : nullptr;
        check(created && width, "resource workspace focused destruction: Sprite pane and numeric control exist");
        if ( !created || !width )
        {
            delete created;
            continue;
        }
        QPointer<ui::ScrubFloatEdit> heldWidth(width);
        created->show();
        created->activateWindow();
        width->lineEdit()->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents(QEventLoop::AllEvents);
        check(width->lineEdit()->hasFocus(),
              "resource workspace focused destruction: the numeric line owns focus before deletion");
        int started = 0;
        QObject::connect(width, &ui::ScrubFloatEdit::scrubStarted, created,
                         [&started]()
                         {
                             ++started;
                         });
        if ( liveScrub )
        {
            QLineEdit* const line = width->lineEdit();
            const QPoint origin = line->rect().center();
            const QPoint moved = origin + QPoint(28, 0);
            QMouseEvent press(QEvent::MouseButtonPress, QPointF(origin), QPointF(line->mapToGlobal(origin)),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(line, &press);
            QMouseEvent move(QEvent::MouseMove, QPointF(moved), QPointF(line->mapToGlobal(moved)), Qt::NoButton,
                             Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(line, &move);
            check(started == 1,
                  "resource workspace focused destruction: actual numeric scrub is active before deletion");
        }
        else
        {
            typeIntoLineEdit(width->lineEdit(), QStringLiteral("29x"));
            const QCborMap state = ui::resources::captureResourceAuthoringPaneState(created);
            check(state.value(QStringLiteral("spritePendingNumericEdits"))
                          .toMap()
                          .value(QStringLiteral("width"))
                          .toString() == QStringLiteral("29x"),
                  "resource workspace focused destruction: invalid pending numeric text exists before deletion");
        }
        const int beforeDestruction = publications;
        delete created;
        check(pane.isNull() && heldWidth.isNull() && publications == beforeDestruction,
              "resource workspace focused destruction: pending focus and active scrub teardown publish no late "
              "authored changes");
    }
}

QTreeWidgetItem* soundModeByBytes(QTreeWidget* const tree, const QByteArray& mode)
{
    for ( int index = 0; tree && index < tree->topLevelItemCount(); ++index )
    {
        QTreeWidgetItem* const group = tree->topLevelItem(index);
        if ( group && group->data(0, Qt::UserRole + 22).toByteArray() == mode )
        {
            return group;
        }
    }
    return nullptr;
}

void checkSoundModePresentationAndEmptyGroups()
{
    QTemporaryDir directory{};
    check(directory.isValid(), "resource workspace SND mode presentation: temporary directory exists");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path path = std::filesystem::path(directory.path().toStdWString()) / "mode_order.snd";
    constexpr std::string_view source = "surprise last.wav 0x80000040 0.5 2.5 1\n"
                                        "moving walk.wav 0x00000040 0.5 2.5 1\n"
                                        "create first.wav 0x00000040 0.5 2.5 1\n"
                                        "CUSTOM_Mode exact.wav 0x80000040 0.5 2.5 1\n";
    check(writeBytes(path, source), "resource workspace SND mode presentation: fixed unsorted source writes");
    auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext()));
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    check(pane && tree, "resource workspace SND mode presentation: actual pane and tree exist");
    if ( !pane || !tree )
    {
        return;
    }
    openResourceFile(pane.get(), path);
    const QCborMap original = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborArray rows = original.value(QStringLiteral("rows")).toArray();
    const std::array<QByteArray, 4> modes{QByteArrayLiteral("create"), QByteArrayLiteral("moving"),
                                          QByteArrayLiteral("surprise"), QByteArrayLiteral("CUSTOM_Mode")};
    const std::array<int, 4> slots{1, 5, 87, 0};
    bool ordered = tree->topLevelItemCount() == 4;
    bool exactNames = ordered;
    for ( int index = 0; index < 4 && ordered; ++index )
    {
        QTreeWidgetItem* const group = tree->topLevelItem(index);
        ordered = group->data(0, Qt::UserRole + 22).toByteArray() == modes[static_cast<std::size_t>(index)] &&
                  group->data(0, Qt::UserRole + 23).toInt() == slots[static_cast<std::size_t>(index)];
        exactNames = exactNames && group->text(0) == QString::fromLatin1(modes[static_cast<std::size_t>(index)]);
    }
    check(ordered && exactNames && rows.size() == 4 &&
              rows.at(0).toArray().at(0).toString() == QStringLiteral("surprise") &&
              rows.at(3).toArray().at(0).toString() == QStringLiteral("CUSTOM_Mode"),
          "resource workspace SND mode presentation: engine order and separate slot badges preserve exact source order "
          "and literals");
    pane->resize(1050, 650);
    pane->show();
    tree->setAnimated(false);
    QApplication::processEvents(QEventLoop::AllEvents);
    QTreeWidgetItem* moving = soundModeByBytes(tree, QByteArrayLiteral("moving"));
    if ( !moving )
    {
        check(false, "resource workspace SND mode presentation: moving mode exists");
        return;
    }
    const QPoint title = tree->visualItemRect(moving).center();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(title), QPointF(tree->viewport()->mapToGlobal(title)),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(title), QPointF(tree->viewport()->mapToGlobal(title)),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &release);
    check(moving->isExpanded() && !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "resource workspace SND mode presentation: one left click toggles the whole mode header without authoring "
          "data");
    QAbstractButton* const expand = button(pane.get(), "snd.expand_all");
    QAbstractButton* const collapse = button(pane.get(), "snd.collapse_all");
    check(expand && collapse && expand->text() == QStringLiteral("Expand All") &&
              collapse->text() == QStringLiteral("Collapse All"),
          "resource workspace SND mode presentation: compact top Expand All and Collapse All controls exist");
    if ( expand && collapse )
    {
        expand->click();
        bool expanded = true;
        for ( int index = 0; index < tree->topLevelItemCount(); ++index )
        {
            expanded = expanded && tree->topLevelItem(index)->isExpanded();
        }
        collapse->click();
        bool collapsed = true;
        for ( int index = 0; index < tree->topLevelItemCount(); ++index )
        {
            collapsed = collapsed && !tree->topLevelItem(index)->isExpanded();
        }
        check(
            expanded && collapsed &&
                ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")).toArray() ==
                    rows &&
                !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
            "resource workspace SND mode presentation: bulk expansion affects presentation only");
    }
    bool popupSeen = false;
    runWithModalDriver(
        [&]()
        {
            button(pane.get(), "snd.add_mode")->click();
        },
        std::nullopt, std::nullopt, false, {},
        [&](QWidget* active)
        {
            auto* const popup = qobject_cast<QDialog*>(active);
            QListWidget* const list = popup ? popup->findChild<QListWidget*>(QStringLiteral("snd.mode.list")) : nullptr;
            if ( !popup || !list )
            {
                return;
            }
            for ( int index = 0; index < list->count(); ++index )
            {
                if ( list->item(index)->text() == QStringLiteral("activate") )
                {
                    popupSeen = true;
                    list->setCurrentItem(list->item(index));
                    popup->accept();
                    return;
                }
            }
            popup->reject();
        });
    QTreeWidgetItem* const added = soundModeByBytes(tree, QByteArrayLiteral("activate"));
    const QCborMap emptyModeState = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const bool onlyPresentation =
        popupSeen && added && added->isExpanded() && added->data(0, Qt::UserRole + 23).toInt() == 2 &&
        tree->currentItem() == added && tree->viewport()->rect().intersects(tree->visualItemRect(added)) &&
        soundAlternativeItems(tree).size() == 4 && emptyModeState.value(QStringLiteral("rows")).toArray() == rows &&
        !ui::resources::resourceAuthoringPaneIsModified(pane.get());
    check(onlyPresentation, "resource workspace SND mode presentation: adding a mode creates an expanded visible "
                            "zero-entry group without a placeholder WAV");
    check(ui::resources::resourceAuthoringPaneSave(pane.get(), false) && readBytes(path) == source,
          "resource workspace SND mode presentation: zero-entry groups never become external SND records");
    auto restored = std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext()));
    QString warning{};
    const bool restoredOk =
        restored && ui::resources::restoreResourceAuthoringPaneState(restored.get(), emptyModeState, &warning);
    QTreeWidget* const restoredTree =
        restored ? restored->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    check(
        restoredOk && soundModeByBytes(restoredTree, QByteArrayLiteral("activate")) &&
            ui::resources::captureResourceAuthoringPaneState(restored.get()).value(QStringLiteral("rows")).toArray() ==
                rows,
        "resource workspace SND mode presentation: native pane recovery retains empty UI modes without authoring "
        "entries");
}

void checkHiddenUniMetadataAndExactExport()
{
    QTemporaryDir directory;
    check(directory.isValid(), "resource workspace UNI hidden metadata: temporary directory exists");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    const QCborArray rows{QCborArray{QStringLiteral("MIN"), QStringLiteral("Minimum"), QStringLiteral("-2147483648")},
                          QCborArray{QStringLiteral("ZERO"), QStringLiteral("Zero"), QStringLiteral("0")},
                          QCborArray{QStringLiteral("MAX"), QStringLiteral("Maximum"), QStringLiteral("2147483647")}};
    constexpr std::string_view expected =
        "MSGS 3\n\n\"MIN\" -2147483648 \"Minimum\"\n\"ZERO\" 0 \"Zero\"\n\"MAX\" 2147483647 \"Maximum\"\n\nEND\n";
    int ordinal = 0;
    for ( const ProjectType profile : {ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM} )
    {
        auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
            nullptr, ui::resources::ResourceAuthoringKind::stringTable, testContext(profile)));
        QTableWidget* const table = pane ? pane->findChild<QTableWidget*>(QStringLiteral("uni.entries")) : nullptr;
        check(table, "resource workspace UNI hidden metadata: real table exists in each project");
        if ( !table )
        {
            continue;
        }
        QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
        state.remove(QStringLiteral("history")); // These signed-metadata rows define an independent baseline.
        state.insert(QStringLiteral("rows"), rows);
        state.insert(QStringLiteral("capacity"), 7);
        state.insert(QStringLiteral("modified"), true);
        QString warning;
        const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning);
        check(
            restored && table->columnCount() == 3 && table->isColumnHidden(2) && table->item(0, 2) &&
                table->item(0, 2)->text() == QStringLiteral("-2147483648") && table->item(1, 2) &&
                table->item(1, 2)->text() == QStringLiteral("0") && table->item(2, 2) &&
                table->item(2, 2)->text() == QStringLiteral("2147483647") &&
                ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")).toArray() ==
                    rows,
            "resource workspace UNI hidden metadata: all projects hide the unused number while retaining exact signed "
            "boundary cells");
        if ( !restored )
        {
            continue;
        }
        const std::filesystem::path output = root / ("metadata" + std::to_string(ordinal++) + ".uni");
        bool saved = false;
        runWithModalDriver(
            [&]()
            {
                saved = ui::resources::resourceAuthoringPaneSave(pane.get(), true);
            },
            output);
        check(
            saved && readBytes(output) == expected &&
                ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")).toArray() ==
                    rows,
            "resource workspace UNI hidden metadata: external Save writes the live count and exact signed metadata "
            "bytes in every project");
    }
}

void checkEngineNameExportFailureIsAtomic()
{
    QTemporaryDir directory;
    check(directory.isValid(), "resource workspace engine export boundary: temporary directory exists");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    int ordinal = 0;
    for ( const ProjectType profile : {ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM} )
    {
        for ( const auto kind :
              {ui::resources::ResourceAuthoringKind::sprite, ui::resources::ResourceAuthoringKind::soundClass} )
        {
            const bool sprite = kind == ui::resources::ResourceAuthoringKind::sprite;
            const int maximumBytes =
                sprite ? (profile == ProjectType::IJIM ? 63 : 31) : (profile == ProjectType::IJIM ? 121 : 31);
            const std::string original = sprite
                                             ? (profile == ProjectType::IJIM ? "short.mat 0 1 1 4 0 0 0 0 0 1 0 0 0\n"
                                                                             : "short.mat 0 1 1 4 0 0 0 0 0 0\n")
                                             : "moving short.wav 0x80000040 0.5 2.5 0.375\n";
            const std::filesystem::path source =
                root / ("boundary" + std::to_string(ordinal++) + (sprite ? ".spr" : ".snd"));
            check(writeBytes(source, original),
                  "resource workspace engine export boundary: existing destination writes");
            auto pane = std::unique_ptr<QDialog>(
                ui::resources::createResourceAuthoringEditor(nullptr, kind, testContext(profile)));
            check(pane && button(pane.get(), "resource_authoring.open"),
                  "resource workspace engine export boundary: real editor and Open exist");
            if ( !pane || !button(pane.get(), "resource_authoring.open") )
            {
                continue;
            }
            openResourceFile(pane.get(), source);
            QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
            state.remove(QStringLiteral("history")); // The boundary token replaces the imported baseline.
            const QString maximumToken = QString(maximumBytes, QLatin1Char('a'));
            const QString longToken = QString(maximumBytes + 1, QLatin1Char('a'));
            if ( sprite )
            {
                QCborArray values = state.value(QStringLiteral("sprite")).toArray();
                check(values.size() == 15, "resource workspace engine export boundary: exact SPR raw shape opens");
                if ( values.size() != 15 )
                {
                    continue;
                }
                values[0] = maximumToken;
                state.insert(QStringLiteral("sprite"), values);
            }
            else
            {
                QCborArray rows = state.value(QStringLiteral("rows")).toArray();
                check(rows.size() == 1, "resource workspace engine export boundary: exact SND source row opens");
                if ( rows.size() != 1 )
                {
                    continue;
                }
                QCborArray cells = rows.at(0).toArray();
                cells[1] = maximumToken;
                rows[0] = cells;
                state.insert(QStringLiteral("rows"), rows);
            }
            state.insert(QStringLiteral("modified"), true);
            QString warning;
            const bool boundaryRestored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning);
            check(boundaryRestored, "resource workspace engine export boundary: maximum-byte raw name restores");
            if ( !boundaryRestored )
            {
                continue;
            }
            bool boundarySaved = false;
            runWithModalDriver(
                [&]()
                {
                    boundarySaved = ui::resources::resourceAuthoringPaneSave(pane.get(), false);
                },
                std::nullopt, QMessageBox::Ok);
            const std::string expectedBoundary =
                maximumToken.toStdString() +
                (sprite ? (profile == ProjectType::IJIM ? " 0 1 1 4 0 0 0 0 0 1 0 0 0\n" : " 0 1 1 4 0 0 0 0 0 0\n")
                        : "");
            const std::string expectedBytes =
                sprite ? expectedBoundary : "moving " + maximumToken.toStdString() + " 0x80000040 0.5 2.5 0.375\n";
            check(boundarySaved && readBytes(source) == expectedBytes &&
                      !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
                  "resource workspace engine export boundary: classic maximum 31-byte names and IJIM maximum 63-byte "
                  "MAT or 121-byte WAV names save exact bytes");
            state = ui::resources::captureResourceAuthoringPaneState(pane.get());
            state.remove(QStringLiteral("history")); // The over-limit token is a separate native-draft fixture.
            if ( sprite )
            {
                QCborArray values = state.value(QStringLiteral("sprite")).toArray();
                values[0] = longToken;
                state.insert(QStringLiteral("sprite"), values);
            }
            else
            {
                QCborArray rows = state.value(QStringLiteral("rows")).toArray();
                QCborArray cells = rows.at(0).toArray();
                cells[1] = longToken;
                rows[0] = cells;
                state.insert(QStringLiteral("rows"), rows);
            }
            state.insert(QStringLiteral("modified"), true);
            const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning);
            check(restored, "resource workspace engine export boundary: portable/native raw token above engine limit "
                            "remains admitted");
            if ( !restored )
            {
                continue;
            }
            const QCborMap before = ui::resources::captureResourceAuthoringPaneState(pane.get());
            const std::string existingBytes = readBytes(source);
            bool saved = true;
            QString validationModalTitle;
            QString validationModalText;
            runWithModalDriver(
                [&]()
                {
                    saved = ui::resources::resourceAuthoringPaneSave(pane.get(), false);
                },
                std::nullopt, QMessageBox::Ok, false, {}, {},
                [&](QMessageBox* message)
                {
                    validationModalTitle = message->windowTitle();
                    validationModalText = message->text();
                });
            const QCborMap after = ui::resources::captureResourceAuthoringPaneState(pane.get());
            QLabel* const status = pane->findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
            const QString expectedError =
                sprite
                    ? (profile == ProjectType::IJIM
                           ? QStringLiteral(
                                 "Resource validation failed: IJIM SPR material names are limited to 63 bytes.")
                           : QStringLiteral(
                                 "Resource validation failed: Classic SPR material names are limited to 31 bytes."))
                    : (profile == ProjectType::IJIM
                           ? QStringLiteral(
                                 "Resource validation failed: IJIM sound-class WAV names are limited to 121 bytes.")
                           : QStringLiteral(
                                 "Resource validation failed: Classic sound-class WAV names are limited to 31 bytes."));
            bool identityRetained = true;
            for ( const QString& key :
                  {QStringLiteral("sourceKind"), QStringLiteral("path"), QStringLiteral("name"),
                   QStringLiteral("projectResource"), QStringLiteral("draftIdentity"), QStringLiteral("diskHash")} )
            {
                identityRetained = identityRetained && before.value(key) == after.value(key);
            }
            const QString contentKey = sprite ? QStringLiteral("sprite") : QStringLiteral("rows");
            const bool exactStatus = status && status->text() == expectedError;
            const bool exactModal = validationModalTitle + QStringLiteral(": ") + validationModalText == expectedError;
            const bool bytesRetained = readBytes(source) == existingBytes;
            const bool contentRetained = after.value(contentKey) == before.value(contentKey);
            const bool pendingRetained = after.value(QStringLiteral("spritePendingNumericEdits")) ==
                                         before.value(QStringLiteral("spritePendingNumericEdits"));
            const bool capturedModified = after.value(QStringLiteral("modified")).toBool();
            const bool paneModified = ui::resources::resourceAuthoringPaneIsModified(pane.get());
            const bool atomicFailure = !saved && exactStatus && exactModal && bytesRetained && identityRetained &&
                                       contentRetained && pendingRetained && capturedModified && paneModified;
            if ( !atomicFailure )
            {
                std::cerr << "Resource export atomic diagnostic: project=" << static_cast<int>(profile)
                          << " kind=" << (sprite ? "SPR" : "SND") << " saved=" << saved
                          << " exactStatus=" << exactStatus << " exactModal=" << exactModal
                          << " bytesRetained=" << bytesRetained << " identityRetained=" << identityRetained
                          << " contentRetained=" << contentRetained << " pendingRetained=" << pendingRetained
                          << " capturedModified=" << capturedModified << " paneModified=" << paneModified
                          << " status=" << (status ? status->text().toStdString() : "<missing>")
                          << " modalTitle=" << validationModalTitle.toStdString()
                          << " modalText=" << validationModalText.toStdString()
                          << " expected=" << expectedError.toStdString() << '\n';
                for ( const QString& key :
                      {QStringLiteral("sourceKind"), QStringLiteral("path"), QStringLiteral("name"),
                       QStringLiteral("projectResource"), QStringLiteral("draftIdentity"), QStringLiteral("diskHash")} )
                {
                    if ( before.value(key) != after.value(key) )
                    {
                        std::cerr << "Resource export identity changed: " << key.toStdString() << '\n';
                    }
                }
            }
            check(atomicFailure,
                  "resource workspace engine export boundary: over-limit SPR MAT or SND WAV Save preserves existing "
                  "bytes, raw content, source identity and modified state");
        }
    }
}

void checkClassicPendingExtraLightScalarSave()
{
    QTemporaryDir directory;
    check(directory.isValid(), "resource workspace pending classic scalar: temporary directory exists");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    int ordinal = 0;
    for ( const ProjectType profile : {ProjectType::JKDF2, ProjectType::MOTS} )
    {
        for ( const QString& scalar : {QStringLiteral("0.25"), QStringLiteral("-0.25"), QStringLiteral("1.5")} )
        {
            const std::filesystem::path source = root / ("scalar_" + std::to_string(ordinal++) + ".spr");
            const std::string original = "source.mat 0 1 1 4 0 0 0 0 0 0\n";
            check(writeBytes(source, original), "resource workspace pending classic scalar: original loose bytes write");
            auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
                nullptr, ui::resources::ResourceAuthoringKind::sprite, testContext(profile)));
            check(pane && button(pane.get(), "resource_authoring.open"),
                  "resource workspace pending classic scalar: real editor and Open exist");
            if ( !pane || !button(pane.get(), "resource_authoring.open") )
            {
                return;
            }
            openResourceFile(pane.get(), source);
            ui::ScrubFloatEdit* const scrub =
                pane->findChild<ui::ScrubFloatEdit*>(QStringLiteral("spr.extra_light.scalar"));
            QLineEdit* const line = scrub ? scrub->lineEdit() : nullptr;
            check(line, "resource workspace pending classic scalar: actual scalar text editor exists");
            if ( !line )
            {
                return;
            }
            pane->resize(1000, 750);
            pane->show();
            pane->activateWindow();
            line->setFocus(Qt::OtherFocusReason);
            check(waitUntil([line]() { return line->hasFocus(); }),
                  "resource workspace pending classic scalar: the real scalar field owns focus");
            typeIntoLineEdit(line, scalar);
            const QCborMap pending = ui::resources::captureResourceAuthoringPaneState(pane.get());
            check(line->hasFocus() &&
                      pending.value(QStringLiteral("spritePendingNumericEdits")).toMap()
                              .value(QStringLiteral("extraLightScalar")).toString() == scalar &&
                      pending.value(QStringLiteral("modified")).toBool(),
                  "resource workspace pending classic scalar: positive, signed and HDR text remains an authored pending edit");
            bool saved = false;
            runWithModalDriver([&]() { saved = ui::resources::resourceAuthoringPaneSave(pane.get(), false); },
                               std::nullopt, QMessageBox::Ok);
            const QCborMap after = ui::resources::captureResourceAuthoringPaneState(pane.get());
            const QCborArray values = after.value(QStringLiteral("sprite")).toArray();
            const std::string expected = "source.mat 0 1 1 4 0 0 " + scalar.toStdString() + " 0 0 0\n";
            check(saved && readBytes(source) == expected && values.size() == 15 &&
                      values.at(7).toString() == scalar && values.at(8).toString() == scalar &&
                      values.at(9).toString() == scalar && values.at(10).toString() == scalar &&
                      values.at(11).toString() == QStringLiteral("1") &&
                      !after.contains(QStringLiteral("spritePendingNumericEdits")) &&
                      !after.value(QStringLiteral("modified")).toBool() &&
                      !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
                  "resource workspace pending classic scalar: focused Save writes exact scalar bytes, commits RGB together, retains alpha and clears the pending dirty state");
            auto restoredPane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
                nullptr, ui::resources::ResourceAuthoringKind::sprite, testContext(profile)));
            if ( restoredPane )
            {
                restoredPane->resize(pane->size());
                restoredPane->show();
                QApplication::processEvents(QEventLoop::AllEvents);
            }
            QString warning;
            const bool restored = restoredPane &&
                ui::resources::restoreResourceAuthoringPaneState(restoredPane.get(), after, &warning);
            if ( restored )
            {
                QApplication::processEvents(QEventLoop::AllEvents);
            }
            const QCborMap restoredState = restoredPane
                ? ui::resources::captureResourceAuthoringPaneState(restoredPane.get()) : QCborMap{};
            const bool nativeRestoredExactly = restored && restoredState == after;
            if ( !nativeRestoredExactly )
            {
                std::cerr << "Pending classic scalar native restore diagnostic: profile=" << static_cast<int>(profile)
                          << " scalar=" << scalar.toStdString() << " restored=" << restored
                          << " warning=" << warning.toStdString() << '\n';
                const auto printDifferences = [](const char* const label, const QCborMap& expected,
                                                 const QCborMap& actual)
                {
                    QList<QCborValue> keys = expected.keys();
                    for ( const QCborValue& key : actual.keys() )
                    {
                        if ( !keys.contains(key) )
                        {
                            keys.append(key);
                        }
                    }
                    for ( const QCborValue& key : keys )
                    {
                        if ( expected.value(key) != actual.value(key) )
                        {
                            std::cerr << "Pending classic scalar native restore diagnostic: " << label
                                      << " key=" << key.toString().toStdString()
                                      << " expectedCbor=" << expected.value(key).toCbor().toHex().toStdString()
                                      << " actualCbor=" << actual.value(key).toCbor().toHex().toStdString() << '\n';
                        }
                    }
                };
                printDifferences("pane", after, restoredState);
                printDifferences("presentation", after.value(QStringLiteral("presentation")).toMap(),
                                 restoredState.value(QStringLiteral("presentation")).toMap());
            }
            check(nativeRestoredExactly,
                  "resource workspace pending classic scalar: the complete saved native state restores exactly");
        }
    }
}

void checkSpriteValidationStatusSurvivesPendingPreview()
{
    QTemporaryDir directory;
    check(directory.isValid(), "resource workspace deferred validation status: temporary directory exists");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    for ( const ProjectType profile : {ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM} )
    {
        const bool ijim = profile == ProjectType::IJIM;
        const std::filesystem::path source = root / ("pending_" + std::to_string(static_cast<int>(profile)) + ".spr");
        const std::string original = ijim ? "source.mat 0 1 1 4 0 0 0 0 0 1 0 0 0\n"
                                          : "source.mat 0 1 1 4 0 0 0 0 0 0\n";
        check(writeBytes(source, original), "resource workspace deferred validation status: original SPR bytes write");
        auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
            nullptr, ui::resources::ResourceAuthoringKind::sprite, testContext(profile)));
        check(pane && button(pane.get(), "resource_authoring.open"),
              "resource workspace deferred validation status: real Sprite editor and Open exist");
        if ( !pane || !button(pane.get(), "resource_authoring.open") )
        {
            return;
        }
        openResourceFile(pane.get(), source);
        const QPointer<GlView> preview = pane->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
        const bool originalPreviewBuilt = preview && waitUntil(
            [&]()
            {
                return preview && preview->property("spr.preview.serialized_resource").toString() ==
                                      QString::fromStdString(original);
            });
        check(originalPreviewBuilt,
              "resource workspace deferred validation status: the valid original production preview is built first");
        if ( !originalPreviewBuilt )
        {
            return;
        }
        QCborMap invalid = ui::resources::captureResourceAuthoringPaneState(pane.get());
        invalid.remove(QStringLiteral("history")); // The invalid native token defines this error fixture's baseline.
        QCborArray sprite = invalid.value(QStringLiteral("sprite")).toArray();
        check(sprite.size() == 15, "resource workspace deferred validation status: original native SPR shape opens");
        if ( sprite.size() != 15 )
        {
            return;
        }
        sprite[0] = QString(ijim ? 64 : 32, QLatin1Char('a'));
        invalid.insert(QStringLiteral("sprite"), sprite);
        invalid.insert(QStringLiteral("modified"), true);
        QString warning;
        const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), invalid, &warning);
        check(restored, "resource workspace deferred validation status: over-limit native MAT token remains admitted");
        if ( !restored )
        {
            return;
        }
        QPointer<QTimer> previewTimer;
        int matchingTimers = 0;
        for ( QTimer* const timer : pane->findChildren<QTimer*>(QString{}, Qt::FindDirectChildrenOnly) )
        {
            if ( timer->isSingleShot() && timer->interval() == 33 && timer->timerType() == Qt::PreciseTimer )
            {
                previewTimer = timer;
                ++matchingTimers;
            }
        }
        check(previewTimer && matchingTimers == 1,
              "resource workspace deferred validation status: the actual 33 ms production preview timer is identified");
        if ( !previewTimer || matchingTimers != 1 )
        {
            return;
        }
        const QCborMap before = ui::resources::captureResourceAuthoringPaneState(pane.get());
        int completedPreviews = 0;
        const QMetaObject::Connection previewFinished = QObject::connect(
            previewTimer.data(), &QTimer::timeout, pane.get(),
            [&completedPreviews]()
            {
                ++completedPreviews;
            });
        bool saved = true;
        bool exactModal = false;
        bool previewArmedBeforeDismissal = false;
        const QString expectedDetail = ijim ? QStringLiteral("IJIM SPR material names are limited to 63 bytes.")
                                            : QStringLiteral("Classic SPR material names are limited to 31 bytes.");
        runWithModalDriver(
            [&]()
            {
                saved = ui::resources::resourceAuthoringPaneSave(pane.get(), false);
            },
            std::nullopt, QMessageBox::Ok, false, {}, {},
            [&](QMessageBox* message)
            {
                exactModal = message->objectName() == QStringLiteral("resource_authoring.error") &&
                             message->windowTitle() == QStringLiteral("Resource validation failed") &&
                             message->text() == expectedDetail;
                // Restart the real queued work at its unchanged production interval,
                // then the modal driver dismisses the error in this same callback.
                completedPreviews = 0;
                previewTimer->start();
                previewArmedBeforeDismissal = previewTimer->isActive() && previewTimer->interval() == 33;
            });
        const bool previewSettled = waitUntil(
            [&]()
            {
                return previewTimer && !previewTimer->isActive() && preview &&
                       !preview->property("spr.preview.serialized_resource").isValid();
            });
        QObject::disconnect(previewFinished);
        const QCborMap after = ui::resources::captureResourceAuthoringPaneState(pane.get());
        QLabel* const status = pane->findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
        const QString expectedStatus = QStringLiteral("Resource validation failed: ") + expectedDetail;
        const bool exactStatus = status && status->text() == expectedStatus;
        const bool atomicState = after == before && readBytes(source) == original &&
                                 after.value(QStringLiteral("modified")).toBool() &&
                                 ui::resources::resourceAuthoringPaneIsModified(pane.get());
        if ( !exactStatus || !atomicState || !previewSettled )
        {
            std::cerr << "Deferred SPR validation diagnostic: project=" << static_cast<int>(profile)
                      << " exactStatus=" << exactStatus << " atomicState=" << atomicState
                      << " previewSettled=" << previewSettled << " completedPreviews=" << completedPreviews
                      << " status=" << (status ? status->text().toStdString() : "<missing>")
                      << " expected=" << expectedStatus.toStdString() << '\n';
        }
        check(!saved && exactModal && previewArmedBeforeDismissal && previewSettled && exactStatus && atomicState,
              "resource workspace deferred validation status: immediate dismissal and the completed production preview "
              "retain the exact terminal Save error and every native/source field");
    }
}

void checkResourceValidationModalKeepsCurrentOwnerState()
{
    QTemporaryDir directory;
    check(directory.isValid(), "resource workspace validation modal: temporary directory exists");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    const std::filesystem::path originalPath = root / "original.snd";
    const std::filesystem::path replacementPath = root / "replacement.snd";
    const std::string originalBytes = "moving original.wav 0x00000040 0.5 2.5 1\n";
    const std::string replacementBytes = "moving replacement.wav 0x80000040 0.25 0.75 0.625\n";
    check(writeBytes(originalPath, originalBytes) && writeBytes(replacementPath, replacementBytes),
          "resource workspace validation modal: exact original and replacement source bytes write");
    for ( const bool destroyOwner : {true, false} )
    {
        auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
            nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext(ProjectType::JKDF2)));
        check(pane && button(pane.get(), "resource_authoring.open"),
              "resource workspace validation modal: real sound editor and Open exist");
        if ( !pane || !button(pane.get(), "resource_authoring.open") )
        {
            return;
        }
        openResourceFile(pane.get(), replacementPath);
        const QCborMap replacement = ui::resources::captureResourceAuthoringPaneState(pane.get());
        openResourceFile(pane.get(), originalPath);
        QCborMap invalid = ui::resources::captureResourceAuthoringPaneState(pane.get());
        invalid.remove(QStringLiteral("history")); // Replace the source row with an independent invalid draft.
        QCborArray rows = invalid.value(QStringLiteral("rows")).toArray();
        check(rows.size() == 1 && replacement.value(QStringLiteral("rows")).toArray().size() == 1,
              "resource workspace validation modal: real one-row sources are available");
        if ( rows.size() != 1 || replacement.value(QStringLiteral("rows")).toArray().size() != 1 )
        {
            return;
        }
        QCborArray cells = rows.at(0).toArray();
        cells[1] = QString(32, QLatin1Char('a'));
        rows[0] = cells;
        invalid.insert(QStringLiteral("rows"), rows);
        invalid.insert(QStringLiteral("modified"), true);
        QString warning;
        const bool invalidRestored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), invalid, &warning);
        check(invalidRestored, "resource workspace validation modal: over-limit raw WAV remains admitted");
        if ( !invalidRestored )
        {
            return;
        }
        const QPointer<QDialog> owner(pane.get());
        int destructionCount = 0;
        QObject::connect(pane.get(), &QObject::destroyed,
                         [&destructionCount]()
                         {
                             ++destructionCount;
                         });
        bool saved = true;
        bool exactModal = false;
        bool replacementRestored = false;
        QCborMap replacementDuringModal;
        runWithModalDriver(
            [&]()
            {
                saved = ui::resources::resourceAuthoringPaneSave(pane.get(), false);
            },
            std::nullopt, QMessageBox::Ok, false, {}, {},
            [&](QMessageBox* message)
            {
                exactModal = message->objectName() == QStringLiteral("resource_authoring.error") &&
                             message->windowTitle() == QStringLiteral("Resource validation failed") &&
                             message->text() ==
                                 QStringLiteral("Classic sound-class WAV names are limited to 31 bytes.");
                if ( destroyOwner )
                {
                    pane.reset();
                    return;
                }
                replacementRestored =
                    ui::resources::restoreResourceAuthoringPaneState(pane.get(), replacement, &warning);
                check(replacementRestored, "resource workspace validation modal: host restores the replacement source");
                QTreeWidget* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
                QTreeWidgetItem* const row =
                    tree ? soundAlternativeByWav(tree, QStringLiteral("replacement.wav")) : nullptr;
                check(row, "resource workspace validation modal: replacement entry exists for a genuine new edit");
                if ( row )
                {
                    row->setText(5, QStringLiteral("0.75"));
                    replacementDuringModal = ui::resources::captureResourceAuthoringPaneState(pane.get());
                }
            });
        if ( destroyOwner )
        {
            check(exactModal && !saved && !owner && destructionCount == 1 && readBytes(originalPath) == originalBytes &&
                      readBytes(replacementPath) == replacementBytes,
                  "resource workspace validation modal: owner deletion safely returns false and preserves both source files");
        }
        else
        {
            if ( !owner )
            {
                (void)pane.release();
                check(false, "resource workspace validation modal: replacement owner survives the modal");
                return;
            }
            QLabel* const status = pane->findChild<QLabel*>(QStringLiteral("resource_authoring.status"));
            const QCborMap after = ui::resources::captureResourceAuthoringPaneState(pane.get());
            check(exactModal && !saved && owner && replacementRestored && !replacementDuringModal.isEmpty() &&
                      after == replacementDuringModal &&
                      after.value(QStringLiteral("path")).toString() ==
                          QDir::fromNativeSeparators(pathForDialog(replacementPath)) &&
                      after.value(QStringLiteral("rows")).toArray().at(0).toArray().at(5).toString() ==
                          QStringLiteral("0.75") &&
                      after.value(QStringLiteral("modified")).toBool() && status &&
                      status->text() == QStringLiteral("Unsaved changes.") && readBytes(originalPath) == originalBytes &&
                      readBytes(replacementPath) == replacementBytes,
                  "resource workspace validation modal: replacement raw state, source and new status survive without a stale error");
            pane.reset();
        }
    }
}

void checkNamedResourceReplacementPrompts()
{
    QTemporaryDir directory;
    check(directory.isValid(), "resource workspace named unsaved prompt: temporary directory exists");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    const std::array kinds{ui::resources::ResourceAuthoringKind::sprite,
                           ui::resources::ResourceAuthoringKind::soundClass,
                           ui::resources::ResourceAuthoringKind::stringTable};
    const std::array<QString, 3> names{QStringLiteral("new_sprite.spr"), QStringLiteral("new_sound_class.snd"),
                                       QStringLiteral("jonesstrings.uni")};
    const std::array<QString, 3> promptTitles{QStringLiteral("Unsaved sprite"),
                                              QStringLiteral("Unsaved sound class"),
                                              QStringLiteral("Unsaved string table")};
    for ( std::size_t index = 0; index < kinds.size(); ++index )
    {
        for ( const bool loose : {false, true} )
        {
            auto pane = std::unique_ptr<QDialog>(
                ui::resources::createResourceAuthoringEditor(nullptr, kinds[index], testContext(ProjectType::IJIM)));
            check(pane != nullptr, "resource workspace named unsaved prompt: each real resource editor exists");
            if ( !pane )
            {
                continue;
            }
            QString title = names[index];
            if ( loose )
            {
                const std::filesystem::path source = root / ("named_" + names[index].toStdString());
                const std::string text = index == 0   ? "short.mat 0 1 1 4 0 0 0 0 0 1 0 0 0\n"
                                         : index == 1 ? "moving short.wav 0x00000040 0.5 2.5 1\n"
                                                      : "MSGS 1\n\n\"KEY\" 0 \"Value\"\n\nEND\n";
                check(writeBytes(source, text) && button(pane.get(), "resource_authoring.open"),
                      "resource workspace named unsaved prompt: loose resource source and Open exist");
                openResourceFile(pane.get(), source);
                title = QDir::fromNativeSeparators(pathForDialog(source));
            }
            QCborMap dirty = ui::resources::captureResourceAuthoringPaneState(pane.get());
            dirty.remove(QStringLiteral("history")); // The prompt fixture imports an independently unsaved baseline.
            dirty.insert(QStringLiteral("modified"), true);
            QString warning;
            const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), dirty, &warning);
            check(restored, "resource workspace named unsaved prompt: unsaved raw draft restores");
            if ( !restored )
            {
                continue;
            }
            const QCborMap before = ui::resources::captureResourceAuthoringPaneState(pane.get());
            bool matched = false;
            bool promptIdentityMatched = false;
            bool promptPlacementMatched = false;
            bool promptMinimumMatched = false;
            bool promptSizeMatched = false;
            bool promptQuestionStyleMatched = false;
            bool promptButtonsMatched = false;
            bool allowed = true;
            bool discarded = true;
            runWithModalDriver(
                [&]()
                {
                    allowed = ui::resources::resourceAuthoringPaneConfirmClose(pane.get(), &discarded);
                },
                std::nullopt, QMessageBox::Cancel, false, {}, {},
                [&](QMessageBox* prompt)
                {
                    promptIdentityMatched =
                        prompt->objectName() == QStringLiteral("resource_authoring.unsaved_prompt") &&
                        prompt->windowTitle() == promptTitles[index] &&
                        prompt->text() == QStringLiteral("Save changes to %1 before closing?").arg(title);
                    promptPlacementMatched = prompt->parent() == nullptr
                        && prompt->windowModality() == Qt::ApplicationModal;
                    promptMinimumMatched = prompt->minimumSize().width() >= 560
                        && prompt->minimumSize().height() >= 220;
                    promptSizeMatched = prompt->size().width() >= 560 && prompt->size().height() >= 220;
                    promptQuestionStyleMatched = prompt->icon() == QMessageBox::Question
                        && !prompt->windowFlags().testFlag(Qt::WindowMinimizeButtonHint)
                        && !prompt->windowFlags().testFlag(Qt::WindowMaximizeButtonHint);
                    promptButtonsMatched = prompt->button(QMessageBox::Save)
                        && prompt->button(QMessageBox::Discard)
                        && prompt->button(QMessageBox::Cancel)
                        && prompt->defaultButton() == prompt->button(QMessageBox::Save)
                        && prompt->escapeButton() == prompt->button(QMessageBox::Cancel);
                    matched = promptIdentityMatched && promptPlacementMatched && promptMinimumMatched
                        && promptSizeMatched && promptQuestionStyleMatched && promptButtonsMatched;
                });
            const QCborMap after = ui::resources::captureResourceAuthoringPaneState(pane.get());
            check(promptIdentityMatched,
                  "resource workspace named unsaved prompt: each kind uses the shared title and exact filename text");
            check(promptPlacementMatched,
                  "resource workspace named unsaved prompt: the question has no owner parent and is application-modal");
            check(promptMinimumMatched,
                  "resource workspace named unsaved prompt: minimum dimensions are at least 560 by 220");
            check(promptSizeMatched,
                  "resource workspace named unsaved prompt: visible dimensions are at least 560 by 220");
            check(promptQuestionStyleMatched,
                  "resource workspace named unsaved prompt: Question icon and no minimize/maximize buttons");
            check(promptButtonsMatched,
                  "resource workspace named unsaved prompt: Save/Discard/Cancel with Save default and Escape Cancel");
            check(matched && !allowed && !discarded && after == before &&
                      ui::resources::resourceAuthoringPaneIsModified(pane.get()),
                  "resource workspace named unsaved prompt: exact draft or loose filename is shown and Cancel "
                  "preserves every raw and identity field");
        }
    }
}

void checkUniClearSearchRestoresCurrentRow()
{
    constexpr int rowCount = 200;
    constexpr int lateRow = 184;
    QCborArray rawRows{};
    for ( int row = 0; row < rowCount; ++row )
    {
        QCborArray cells{};
        cells.append(QStringLiteral("KEY_%1").arg(row, 3, 10, QLatin1Char('0')));
        cells.append(QStringLiteral("voice_%1").arg(row));
        cells.append(QString::number(row - 100));
        rawRows.append(cells);
    }

    auto pane = std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr, ui::resources::ResourceAuthoringKind::stringTable, testContext(ProjectType::IJIM)));
    QTableWidget* const table = pane ? pane->findChild<QTableWidget*>(QStringLiteral("uni.entries")) : nullptr;
    QLineEdit* const search = pane ? pane->findChild<QLineEdit*>(QStringLiteral("uni.search")) : nullptr;
    check(pane && table && search, "resource workspace UNI clear search: actual editor controls exist");
    if ( !pane || !table || !search )
    {
        return;
    }

    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
    state.remove(QStringLiteral("history"));
    state.insert(QStringLiteral("rows"), rawRows);
    state.insert(QStringLiteral("capacity"), rowCount);
    state.insert(QStringLiteral("modified"), false);
    QString restoreWarning{};
    const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &restoreWarning);
    check(restored && table->rowCount() == rowCount,
          "resource workspace UNI clear search: 200 exact key/text/legacy rows restore");
    if ( !restored || table->rowCount() != rowCount )
    {
        return;
    }

    pane->resize(960, 680);
    pane->show();
    QApplication::processEvents(QEventLoop::AllEvents);
    search->setText(QStringLiteral("voice_184"));
    table->selectRow(lateRow);
    table->setCurrentCell(lateRow, 1);
    table->editItem(table->item(lateRow, 1));
    QApplication::processEvents(QEventLoop::AllEvents);
    const QPersistentModelIndex selectedCell(table->currentIndex());
    QPointer<QLineEdit> const editBuffer = qobject_cast<QLineEdit*>(table->indexWidget(selectedCell));
    check(selectedCell.isValid() && selectedCell.row() == lateRow && selectedCell.column() == 1 && editBuffer,
          "resource workspace UNI clear search: the late voice cell has its live editor and exact current index");
    if ( !editBuffer )
    {
        return;
    }
    editBuffer->setText(QStringLiteral("unfinished voice buffer"));
    const QCborMap beforeClear = ui::resources::captureResourceAuthoringPaneState(pane.get());
    QToolButton* const clear = search->findChild<QToolButton*>();
    const bool matchingResult = clear && clear->isVisible() && !table->isRowHidden(lateRow) &&
        table->isRowHidden(lateRow - 1) && table->item(lateRow, 1)->text() == QStringLiteral("voice_184");
    check(matchingResult,
          "resource workspace UNI clear search: searching voice_184 exposes its late Text result before selection");
    if ( !matchingResult )
    {
        return;
    }
    clear->click();
    QApplication::processEvents(QEventLoop::AllEvents);

    QRect viewportRect = table->viewport()->rect();
    const QRect selectedRect = table->visualRect(selectedCell);
    const QRect previousRect = table->visualRect(table->model()->index(lateRow - 1, 1));
    const QRect nextRect = table->visualRect(table->model()->index(lateRow + 1, 1));
    const int centeredDelta = std::abs(selectedRect.center().y() - viewportRect.center().y());
    const QCborMap afterClear = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborArray afterRows = afterClear.value(QStringLiteral("rows")).toArray();
    const bool queryWasCleared = search->text().isEmpty();
    const bool selectedRowWasRevealed = !table->isRowHidden(lateRow);
    const bool currentCellWasPreserved = table->currentIndex() == selectedCell;
    const bool currentColumnWasPreserved = table->currentColumn() == 1;
    const bool selectionWasPreserved = table->selectionModel()->hasSelection();
    const bool selectedCellIsVisible = viewportRect.contains(selectedRect);
    const bool selectedCellIsCentered = centeredDelta <= selectedRect.height();
    const bool previousRowIsVisible = viewportRect.contains(previousRect);
    const bool nextRowIsVisible = viewportRect.contains(nextRect);
    const QCborArray beforeRows = beforeClear.value(QStringLiteral("rows")).toArray();
    const bool allRawRowsPreserved = afterRows.size() == rowCount && afterRows == beforeRows;
    const bool selectedLegacyNumberPreserved = afterRows.size() == rowCount
        && afterRows.at(lateRow).toArray().size() == 3
        && afterRows.at(lateRow).toArray().at(2).toString() == QString::number(lateRow - 100);
    const bool dirtyStatePreserved =
        afterClear.value(QStringLiteral("modified")) == beforeClear.value(QStringLiteral("modified"));
    const bool historyPreserved =
        afterClear.value(QStringLiteral("history")) == beforeClear.value(QStringLiteral("history"));
    const bool editBufferPreserved = editBuffer
        && editBuffer->text() == QStringLiteral("unfinished voice buffer");
    const bool draftPreserved = allRawRowsPreserved && selectedLegacyNumberPreserved && dirtyStatePreserved
        && historyPreserved && editBufferPreserved;
    check(queryWasCleared,
          "resource workspace UNI clear search: actual clear control empties the query");
    check(selectedRowWasRevealed,
          "resource workspace UNI clear search: selected voice_184 row is revealed");
    check(currentCellWasPreserved && currentColumnWasPreserved,
          "resource workspace UNI clear search: exact selected model index and Text column remain current");
    check(selectionWasPreserved,
          "resource workspace UNI clear search: the selected row remains selected");
    check(selectedCellIsVisible && selectedCellIsCentered,
          "resource workspace UNI clear search: selected Text cell is visible and centered");
    check(previousRowIsVisible && nextRowIsVisible,
          "resource workspace UNI clear search: rows adjacent to the selected cell remain visible");
    check(allRawRowsPreserved,
          "resource workspace UNI clear search: all raw Key/Text/legacy rows are unchanged");
    check(selectedLegacyNumberPreserved,
          "resource workspace UNI clear search: selected signed legacy number remains unchanged");
    check(dirtyStatePreserved,
          "resource workspace UNI clear search: dirty state is unchanged");
    check(historyPreserved,
          "resource workspace UNI clear search: document history is unchanged");
    check(editBufferPreserved,
          "resource workspace UNI clear search: unfinished live cell editor text is unchanged");
    check(queryWasCleared && selectedRowWasRevealed && currentCellWasPreserved && currentColumnWasPreserved &&
              selectionWasPreserved &&
              selectedCellIsVisible && selectedCellIsCentered && previousRowIsVisible && nextRowIsVisible &&
              draftPreserved,
          "resource workspace UNI clear search: clearing after selecting voice_184 centers the late Text row with neighbors and retains its live edit, history, dirty state, raw fields and legacy metadata");

    const int originalViewportHeight = viewportRect.height();
    pane->resize(960, 760);
    QApplication::processEvents(QEventLoop::AllEvents);
    viewportRect = table->viewport()->rect();
    const bool viewportHeightChanged = viewportRect.height() != originalViewportHeight;
    check(viewportHeightChanged,
          "resource workspace UNI clear search: second centering case uses a different viewport height");
    search->setText(QStringLiteral("NO_MATCH"));
    const bool hiddenNoMatch = table->isRowHidden(lateRow);
    QToolButton* const noMatchClear = search->findChild<QToolButton*>();
    const bool noMatchClearWasVisible = noMatchClear && noMatchClear->isVisible();
    if ( noMatchClearWasVisible )
    {
        noMatchClear->click();
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    const QRect resizedSelectedRect = table->visualRect(selectedCell);
    const int resizedCenteredDelta =
        std::abs(resizedSelectedRect.center().y() - viewportRect.center().y());
    check(hiddenNoMatch && noMatchClearWasVisible && !table->isRowHidden(lateRow) &&
              table->currentIndex() == selectedCell && table->currentColumn() == 1,
          "resource workspace UNI clear search: the no-match boundary also restores the hidden selected row and its column");
    check(viewportHeightChanged && !resizedSelectedRect.isEmpty()
              && resizedCenteredDelta <= resizedSelectedRect.height(),
          "resource workspace UNI clear search: selected Text cell centers at the resized viewport height");

    const auto checkEndpoint = [&](const int row, const bool startAtOppositeEnd)
    {
        table->selectRow(row);
        table->setCurrentCell(row, 1);
        table->verticalScrollBar()->setValue(startAtOppositeEnd
            ? table->verticalScrollBar()->maximum() : table->verticalScrollBar()->minimum());
        search->setText(QStringLiteral("NO_MATCH"));
        const bool hidden = table->isRowHidden(row);
        QToolButton* const endpointClear = search->findChild<QToolButton*>();
        const bool endpointClearWasVisible = endpointClear && endpointClear->isVisible();
        if ( endpointClear )
        {
            endpointClear->click();
        }
        QApplication::processEvents(QEventLoop::AllEvents);
        const QModelIndex current = table->model()->index(row, 1);
        const QModelIndex neighbor = table->model()->index(row == 0 ? 1 : row - 1, 1);
        const QRect currentRect = table->visualRect(current);
        const QRect neighborRect = table->visualRect(neighbor);
        check(hidden && endpointClearWasVisible && !table->isRowHidden(row) &&
                  table->currentIndex() == current && table->currentColumn() == 1 &&
                  viewportRect.contains(currentRect) && viewportRect.contains(neighborRect),
              row == 0
                  ? "resource workspace UNI clear search: a selected first-row endpoint and its next row are revealed"
                  : "resource workspace UNI clear search: a selected last-row endpoint and its previous row are revealed");
    };
    checkEndpoint(0, true);
    checkEndpoint(rowCount - 1, false);

    table->setCurrentCell(-1, -1);
    table->clearSelection();
    table->verticalScrollBar()->setValue(table->verticalScrollBar()->minimum());
    search->setText(QStringLiteral("KEY_00"));
    QToolButton* const noSelectionClear = search->findChild<QToolButton*>();
    const int scrollBeforeClear = table->verticalScrollBar()->value();
    if ( noSelectionClear )
    {
        noSelectionClear->click();
    }
    QApplication::processEvents(QEventLoop::AllEvents);
    check(noSelectionClear && table->currentRow() == -1 && table->selectedItems().isEmpty() &&
              table->verticalScrollBar()->value() == scrollBeforeClear,
          "resource workspace UNI clear search: clearing without a selection does not jump the viewport");
    pane->hide();
}

QCborArray soundFollowupRow(const QString& mode, const QString& wav, const QString& volume = QStringLiteral("0.375"))
{
    QCborArray row;
    for ( const QString& value :
          {mode, wav, QStringLiteral("0x80000040"), QStringLiteral("0.5"), QStringLiteral("2.5"), volume} )
    {
        row.append(value);
    }
    return row;
}

std::unique_ptr<QWidget> soundFollowupPane(
    const QCborArray& rows, ui::resources::ResourceAuthoringContext context = testContext(ProjectType::IJIM),
    ui::resources::ResourceAuthoringChangedCallback changed = {})
{
    auto pane = std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, std::move(context), std::move(changed)));
    check(pane != nullptr, "resource workspace SND follow-up: actual pane exists");
    if ( !pane )
    {
        return nullptr;
    }
    QCborMap baseline = ui::resources::captureResourceAuthoringPaneState(pane.get());
    baseline.remove(QStringLiteral("history")); // The caller supplies independent authored Sound rows.
    baseline.insert(QStringLiteral("rows"), rows);
    baseline.insert(QStringLiteral("collapsedModes"), QCborArray{});
    baseline.insert(QStringLiteral("modified"), false);
    baseline.remove(QStringLiteral("soundPreviewLoops"));
    baseline.remove(QStringLiteral("soundEmptyModes"));
    QString warning;
    const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), baseline, &warning);
    check(restored, "resource workspace SND follow-up: independent raw rows restore");
    if ( !restored )
    {
        return nullptr;
    }
    pane->resize(1120, 650);
    pane->show();
    QApplication::processEvents(QEventLoop::AllEvents);
    return pane;
}

void checkSoundAppThemePaintAndHoverControls()
{
    ui::ensureGlobalComponentStyles();
    QCborArray rows;
    rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("first.wav")));
    rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("second.wav"), QStringLiteral("0.7")));
    auto pane = soundFollowupPane(rows);
    check(pane != nullptr, "resource workspace themed Sound paint: actual pane exists");
    if ( !pane )
    {
        return;
    }
    pane->setStyleSheet(ui::applicationWindowStyleSheet());
    QTreeWidget* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    QTreeWidgetItem* const first = tree ? soundAlternativeByWav(tree, QStringLiteral("first.wav")) : nullptr;
    QTreeWidgetItem* const second = tree ? soundAlternativeByWav(tree, QStringLiteral("second.wav")) : nullptr;
    check(tree && first && second, "resource workspace themed Sound paint: real mode and entries exist");
    if ( !tree || !first || !second )
    {
        return;
    }
    tree->setAnimated(false);
    first->parent()->setExpanded(true);
    tree->setCurrentItem(first, 0);
    pane->activateWindow();
    tree->setFocus(Qt::OtherFocusReason);
    const bool focused = waitUntil([tree]() { return tree->hasFocus(); });
    check(focused, "resource workspace themed Sound paint: tree owns actual focus and selected entry");
    const auto pixel = [](const QImage& image, const int x, const int y)
    {
        const qreal ratio = image.devicePixelRatio();
        return image.pixelColor(static_cast<int>(std::floor((x + 0.5) * ratio)),
                                static_cast<int>(std::floor((y + 0.5) * ratio)));
    };
    const auto cyanStripe = [&](const QImage& image, const QRect& row)
    {
        bool cyan = false;
        for ( int y = row.top() + 3; y < row.bottom() - 2; ++y )
        {
            for ( int x = 1; x < 12; ++x )
            {
                const QColor color = pixel(image, x, y);
                cyan = cyan || (color.blue() > color.red() + 35 && color.green() > color.red() + 25);
            }
        }
        return cyan;
    };
    const QRect firstRect = tree->visualItemRect(first);
    const QPoint hover = tree->visualRect(tree->indexFromItem(first, 0)).center();
    QMouseEvent move(QEvent::MouseMove, QPointF(hover), QPointF(tree->viewport()->mapToGlobal(hover)),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &move);
    const QImage firstCurrent = tree->viewport()->grab().toImage();
    check(!firstCurrent.isNull(), "resource workspace themed Sound paint: actual viewport renders");
    if ( firstCurrent.isNull() )
    {
        return;
    }
    const auto hoverControlRects = [tree]()
    {
        std::array<QRect, 3> rectangles{};
        const std::array<const char*, 3> names{"snd.play", "snd.loop", "snd.remove"};
        for ( int index = 0; index < static_cast<int>(names.size()); ++index )
        {
            QToolButton* const control = tree->findChild<QToolButton*>(QString::fromLatin1(names[index]));
            if ( control && control->isVisible() )
            {
                rectangles[static_cast<std::size_t>(index)] = control->geometry();
            }
        }
        return rectangles;
    };
    const std::array<QRect, 3> firstRowControls = hoverControlRects();
    const QRect firstWav = tree->visualRect(tree->indexFromItem(first, 1));
    const QRect secondWav = tree->visualRect(tree->indexFromItem(second, 1));
    QMouseEvent otherHover(QEvent::MouseMove,
                           QPointF(tree->visualRect(tree->indexFromItem(second, 1)).center()),
                           QPointF(tree->viewport()->mapToGlobal(tree->visualRect(tree->indexFromItem(second, 1)).center())),
                           Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &otherHover);
    tree->setCurrentItem(second, 1);
    const QImage secondCurrent = tree->viewport()->grab().toImage();
    const std::array<QRect, 3> secondRowControls = hoverControlRects();
    const auto sameAlternativePixelsOutsideControls = [&](const QImage& before, const QImage& after,
                                                         const QRect& row, const std::array<QRect, 6>& controls,
                                                         QPoint* const firstDifference,
                                                         QColor* const beforeColor,
                                                         QColor* const afterColor)
    {
        for ( int y = row.top(); y <= row.bottom(); ++y )
        {
            for ( int x = 0; x < tree->viewport()->width(); ++x )
            {
                const QPoint point(x, y);
                if ( std::any_of(controls.begin(), controls.end(), [&point](const QRect& control)
                                 {
                                     return control.contains(point);
                                 }) )
                {
                    continue;
                }
                const QColor beforePixel = pixel(before, x, y);
                const QColor afterPixel = pixel(after, x, y);
                if ( beforePixel != afterPixel )
                {
                    if ( firstDifference )
                    {
                        *firstDifference = point;
                    }
                    if ( beforeColor )
                    {
                        *beforeColor = beforePixel;
                    }
                    if ( afterColor )
                    {
                        *afterColor = afterPixel;
                    }
                    return false;
                }
            }
        }
        return true;
    };
    const std::array<QRect, 6> bothHoverControlRects{
        firstRowControls[0], firstRowControls[1], firstRowControls[2],
        secondRowControls[0], secondRowControls[1], secondRowControls[2]};
    const QRect firstFullRow(0, firstRect.top(), tree->viewport()->width(), firstRect.height());
    const QRect secondFullRow(0, tree->visualItemRect(second).top(), tree->viewport()->width(),
                              tree->visualItemRect(second).height());
    QPoint firstDifference(-1, -1);
    QPoint secondDifference(-1, -1);
    QColor firstBeforeColor;
    QColor firstAfterColor;
    QColor secondBeforeColor;
    QColor secondAfterColor;
    const bool firstRowStable = sameAlternativePixelsOutsideControls(
        firstCurrent, secondCurrent, firstFullRow, bothHoverControlRects,
        &firstDifference, &firstBeforeColor, &firstAfterColor);
    const bool secondRowStable = sameAlternativePixelsOutsideControls(
        firstCurrent, secondCurrent, secondFullRow, bothHoverControlRects,
        &secondDifference, &secondBeforeColor, &secondAfterColor);
    const bool noGutterOrBodyHighlight = firstRowStable && secondRowStable;
    if ( !noGutterOrBodyHighlight )
    {
        QTemporaryDir diagnosticDirectory(QDir::tempPath() + QStringLiteral("/resource-snd-gutter-XXXXXX"));
        if ( diagnosticDirectory.isValid() )
        {
            diagnosticDirectory.setAutoRemove(false);
            firstCurrent.save(diagnosticDirectory.filePath(QStringLiteral("before.png")));
            secondCurrent.save(diagnosticDirectory.filePath(QStringLiteral("after.png")));
        }
        const auto rectText = [](const QRect& rect)
        {
            return QStringLiteral("(%1,%2 %3x%4)").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
        };
        std::cerr << "Sound full-row pixel mismatch: first row stable=" << firstRowStable
                  << " row=" << rectText(firstFullRow).toStdString()
                  << " diff=(" << firstDifference.x() << ',' << firstDifference.y() << ')'
                  << " before=" << firstBeforeColor.name(QColor::HexArgb).toStdString()
                  << " after=" << firstAfterColor.name(QColor::HexArgb).toStdString()
                  << "; second row stable=" << secondRowStable
                  << " row=" << rectText(secondFullRow).toStdString()
                  << " diff=(" << secondDifference.x() << ',' << secondDifference.y() << ')'
                  << " before=" << secondBeforeColor.name(QColor::HexArgb).toStdString()
                  << " after=" << secondAfterColor.name(QColor::HexArgb).toStdString() << "\n";
        for ( std::size_t index = 0; index < bothHoverControlRects.size(); ++index )
        {
            std::cerr << "  control[" << index << "]="
                      << rectText(bothHoverControlRects[index]).toStdString() << "\n";
        }
        if ( diagnosticDirectory.isValid() )
        {
            std::cerr << "  viewport images=" << diagnosticDirectory.path().toStdString() << "\n";
        }
    }
    const bool noRowHighlight =
        pixel(firstCurrent, firstWav.left() + 120, firstWav.top() + 3) ==
            pixel(secondCurrent, firstWav.left() + 120, firstWav.top() + 3) &&
        pixel(firstCurrent, secondWav.left() + 120, secondWav.top() + 3) ==
            pixel(secondCurrent, secondWav.left() + 120, secondWav.top() + 3);
    const bool keyboardCurrentWithoutSelection = tree->selectionMode() == QAbstractItemView::NoSelection &&
        tree->selectedItems().isEmpty() && tree->currentItem() == second && tree->hasFocus();
    check(focused && keyboardCurrentWithoutSelection && noRowHighlight && noGutterOrBodyHighlight &&
              !cyanStripe(firstCurrent, firstRect),
          "resource workspace themed Sound paint: entry rows remain unhighlighted while keyboard focus and current-item navigation work");
    const QRect volume = tree->visualRect(tree->indexFromItem(second, 5));
    bool plainCanvas = true;
    for ( int x = volume.left() + 6; x < volume.left() + 36; ++x )
    {
        plainCanvas = plainCanvas && pixel(secondCurrent, x, volume.top() + 3) == QColor(48, 48, 48);
    }
    check(plainCanvas,
          "resource workspace themed Sound paint: inactive volume keeps gray canvas outside its groove, handle and value without a tree frame box");
    const auto glyphBounds = [](const QImage& image, const int glyph)
    {
        QRect bounds;
        for ( int y = 0; y < image.height(); ++y )
        {
            for ( int x = 0; x < image.width(); ++x )
            {
                const QColor color = image.pixelColor(x, y);
                const bool colored = glyph == 0 ? color.green() > color.red() + 35 && color.green() > color.blue() + 20
                                     : glyph == 1 ? color.blue() > color.red() + 40 && color.green() > color.red() + 30
                                                  : color.red() > color.green() + 70 && color.red() > color.blue() + 70;
                if ( colored )
                {
                    bounds = bounds.united(QRect(x, y, 1, 1));
                }
            }
        }
        return QSizeF(bounds.width() / image.devicePixelRatio(), bounds.height() / image.devicePixelRatio());
    };
    const std::array<const char*, 3> names{"snd.play", "snd.loop", "snd.remove"};
    for ( int glyph = 0; glyph < static_cast<int>(names.size()); ++glyph )
    {
        QToolButton* const button = tree->findChild<QToolButton*>(QString::fromLatin1(names[glyph]));
        check(button && button->isVisible(), "resource workspace themed Sound paint: shared hover control is visible");
        if ( !button || !button->isVisible() )
        {
            continue;
        }
        const QSizeF painted = glyphBounds(button->grab().toImage(), glyph);
        const QSizeF source = glyphBounds(button->icon().pixmap(QSize(13, 13)).toImage(), glyph);
        check(button->size() == QSize(22, 22) && button->iconSize() == QSize(13, 13) &&
                  painted.width() >= source.width() - 1 && painted.height() >= source.height() - 1,
              "resource workspace themed Sound paint: actual 22 px hover button paints its full 13 px icon geometry without global padding shrinkage");
        const QPixmap highDensityPixmap = button->icon().pixmap(QSize(13, 13), 2.0);
        const QImage highDensity = highDensityPixmap.toImage();
        bool antialiasedEdge = false;
        for ( int y = 0; y < highDensity.height() && !antialiasedEdge; ++y )
        {
            for ( int x = 0; x < highDensity.width(); ++x )
            {
                const int alpha = highDensity.pixelColor(x, y).alpha();
                if ( alpha > 0 && alpha < 255 )
                {
                    antialiasedEdge = true;
                    break;
                }
            }
        }
        check(highDensityPixmap.size() == QSize(26, 26) && highDensityPixmap.devicePixelRatio() == 2.0 &&
                  antialiasedEdge,
              "resource workspace themed Sound paint: exact 13 px icons provide smooth antialiased curves at 2x density");
    }
    QTreeWidgetItem* const mode = first->parent();
    tree->setCurrentItem(mode, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    const QImage modes = tree->viewport()->grab().toImage();
    const QRect modeRect = tree->visualItemRect(mode);
    const bool modeFocused = tree->hasFocus();
    const bool modeCurrent = tree->currentItem() == mode;
    // The native accent runs from one-quarter to three-quarters of the row height.
    // These fixed x probes sample its upper/lower stem, outside the centered caret.
    const std::array<QPoint, 4> markerProbes{
        QPoint(3, modeRect.top() + modeRect.height() / 4 + 2),
        QPoint(4, modeRect.top() + modeRect.height() / 4 + 2),
        QPoint(3, modeRect.top() + 3 * modeRect.height() / 4 - 2),
        QPoint(4, modeRect.top() + 3 * modeRect.height() / 4 - 2)};
    bool modeHasStripe = false;
    for ( const QPoint& probe : markerProbes )
    {
        const QColor color = pixel(modes, probe.x(), probe.y());
        modeHasStripe = modeHasStripe || (color.blue() > color.red() + 35 && color.green() > color.red() + 25);
    }
    const QPoint modeSample(tree->indentation() + 120, modeRect.top() + 3);
    const QColor modeBackground = pixel(modes, modeSample.x(), modeSample.y());
    tree->setCurrentItem(second, 1);
    QApplication::processEvents(QEventLoop::AllEvents);
    const QImage otherCurrentMode = tree->viewport()->grab().toImage();
    const bool modeBackgroundStable = modeBackground == pixel(otherCurrentMode, modeSample.x(), modeSample.y());
    tree->setCurrentItem(mode, 0);
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool modeWithoutSelection = tree->selectionMode() == QAbstractItemView::NoSelection &&
        tree->selectedItems().isEmpty();
    if ( !modeFocused || !modeCurrent || modeHasStripe || !modeBackgroundStable || !modeWithoutSelection )
    {
        const qreal ratio = modes.devicePixelRatio();
        const QRect physicalGutter(static_cast<int>(std::floor(ratio)),
                                   static_cast<int>(std::floor((modeRect.top() + 3) * ratio)),
                                   static_cast<int>(std::ceil(12 * ratio)) - static_cast<int>(std::floor(ratio)),
                                   static_cast<int>(std::ceil((modeRect.bottom() - 2) * ratio)) -
                                       static_cast<int>(std::floor((modeRect.top() + 3) * ratio)));
        QRect stripeBounds;
        for ( int y = physicalGutter.top(); y <= physicalGutter.bottom(); ++y )
        {
            for ( int x = physicalGutter.left(); x <= physicalGutter.right(); ++x )
            {
                const QColor color = modes.pixelColor(x, y);
                if ( color.blue() > color.red() + 35 && color.green() > color.red() + 25 )
                {
                    stripeBounds = stripeBounds.united(QRect(x, y, 1, 1));
                }
            }
        }
        std::cerr << "Themed Sound selected mode diagnostic: focused=" << modeFocused
                  << " current=" << modeCurrent << " cyanStripe=" << modeHasStripe
                  << " stableBackground=" << modeBackgroundStable
                  << " spanned=" << mode->isFirstColumnSpanned() << " rowLeft=" << modeRect.left()
                  << " rowTop=" << modeRect.top() << " rowHeight=" << modeRect.height()
                  << " dpr=" << ratio << " stripePhysicalLeft=" << stripeBounds.left()
                  << " stripePhysicalTop=" << stripeBounds.top() << " stripePhysicalWidth=" << stripeBounds.width()
                  << " stripePhysicalHeight=" << stripeBounds.height() << '\n';
        for ( const QPoint& probe : markerProbes )
        {
            std::cerr << "Themed Sound marker probe: x=" << probe.x() << " y=" << probe.y()
                      << " color=" << pixel(modes, probe.x(), probe.y()).name().toStdString() << '\n';
        }
        const QRect physicalRow(0, static_cast<int>(std::floor(modeRect.top() * ratio)), modes.width(),
                                static_cast<int>(std::ceil(modeRect.height() * ratio)));
        modes.copy(physicalRow).save(
            QDir(QDir::tempPath()).filePath(QStringLiteral("resource-snd-selected-mode-diagnostic.png")));
    }
    check(modeFocused && modeCurrent && !modeHasStripe && modeBackgroundStable && modeWithoutSelection,
          "resource workspace themed Sound paint: the mode expander retains focus without row selection or highlighting");
    const QRect caretRect(1, modeRect.center().y() - 4, 12, 9);
    const auto paintedCaret = [&](const QImage& image)
    {
        bool ink = false;
        for ( int y = caretRect.top(); y <= caretRect.bottom(); ++y )
        {
            for ( int x = caretRect.left(); x <= caretRect.right(); ++x )
            {
                const QColor color = pixel(image, x, y);
                ink = ink || (color.red() >= 160 && color.green() >= 160 && color.blue() >= 160);
            }
        }
        const qreal ratio = image.devicePixelRatio();
        const QRect physical(static_cast<int>(std::floor(caretRect.left() * ratio)),
                             static_cast<int>(std::floor(caretRect.top() * ratio)),
                             static_cast<int>(std::ceil(caretRect.width() * ratio)),
                             static_cast<int>(std::ceil(caretRect.height() * ratio)));
        return std::pair(ink, image.copy(physical));
    };
    const auto openCaret = paintedCaret(modes);
    mode->setExpanded(false);
    QApplication::processEvents(QEventLoop::AllEvents);
    const auto closedCaret = paintedCaret(tree->viewport()->grab().toImage());
    check(openCaret.first && closedCaret.first && openCaret.second != closedCaret.second &&
              tree->hasFocus() && tree->currentItem() == mode && !mode->isExpanded(),
          "resource workspace themed Sound paint: the selected mode retains distinct painted open and closed expanders");
    mode->setExpanded(true);
}

void checkSoundPresentationPolishAndHoverTransition()
{
    QCborArray rows;
    rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("first.wav")));
    rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("second.wav")));
    auto pane = soundFollowupPane(rows);
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    QTreeWidgetItem* const first = tree ? soundAlternativeByWav(tree, QStringLiteral("first.wav")) : nullptr;
    check(tree && first, "resource workspace Sound polish: real entries exist");
    if ( !tree || !first )
    {
        return;
    }
    pane->setStyleSheet(ui::applicationWindowStyleSheet());
    tree->setAnimated(false);
    QTreeWidgetItem* const mode = first->parent();
    mode->setExpanded(true);
    pane->activateWindow();
    tree->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
    QTreeWidgetItem* const heading = mode->child(0);
    QTreeWidgetItem* const footer = mode->child(mode->childCount() - 1);
    check(heading && heading->text(1) == QStringLiteral("Sound") &&
              tree->headerItem()->text(1) == QStringLiteral("Sound") &&
              heading->text(3) == QStringLiteral("Min") && heading->text(4) == QStringLiteral("Max") &&
              tree->headerItem()->text(3) == QStringLiteral("Min") &&
              tree->headerItem()->text(4) == QStringLiteral("Max"),
          "resource workspace Sound polish: Sound, Min and Max use the requested plain column captions");
    if ( !heading )
    {
        return;
    }
    bool centeredFlagsAndRadii = true;
    for ( const int column : {2, 3, 4} )
    {
        centeredFlagsAndRadii = centeredFlagsAndRadii &&
            (first->textAlignment(column) & Qt::AlignHorizontal_Mask) == Qt::AlignHCenter &&
            (heading->textAlignment(column) & Qt::AlignHorizontal_Mask) == Qt::AlignHCenter;
    }
    const bool soundAndVolumeLeftAligned =
        (first->textAlignment(1) & Qt::AlignHorizontal_Mask) == Qt::AlignLeft &&
        (heading->textAlignment(1) & Qt::AlignHorizontal_Mask) == Qt::AlignLeft &&
        (first->textAlignment(5) & Qt::AlignHorizontal_Mask) == Qt::AlignLeft &&
        (heading->textAlignment(5) & Qt::AlignHorizontal_Mask) == Qt::AlignLeft;
    check(centeredFlagsAndRadii && soundAndVolumeLeftAligned,
          "resource workspace Sound polish: Flags, Min and Max center in headings and rows while Sound and Volume stay left aligned");
    const auto radiusHelp = [](const QString& tip)
    {
        return tip.contains(QStringLiteral("radius"), Qt::CaseInsensitive) &&
               tip.contains(QStringLiteral("0.1 world unit = 1 m"), Qt::CaseInsensitive);
    };
    check(radiusHelp(heading->toolTip(3)) && radiusHelp(heading->toolTip(4)),
          "resource workspace Sound polish: Min and Max heading help describes radius and the SED distance units");
    check(footer && footer->text(0) == QStringLiteral("＋ Add Entry") && footer->isFirstColumnSpanned() &&
              (footer->textAlignment(0) & Qt::AlignHorizontal_Mask) == Qt::AlignHCenter,
          "resource workspace Sound polish: the spanned Add Entry caption is centered");

    const QPoint rowPoint = tree->visualRect(tree->indexFromItem(first, 1)).center();
    QMouseEvent hover(QEvent::MouseMove, QPointF(rowPoint), QPointF(tree->viewport()->mapToGlobal(rowPoint)),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &hover);
    const std::array<const char*, 2> glyphNames{"snd.play", "snd.loop"};
    for ( int glyph = 0; glyph < static_cast<int>(glyphNames.size()); ++glyph )
    {
        QToolButton* const control = tree->findChild<QToolButton*>(QString::fromLatin1(glyphNames[glyph]));
        check(control && control->isVisible(), "resource workspace Sound polish: shared preview icon is visible");
        if ( !control || !control->isVisible() )
        {
            continue;
        }
        const QImage painted = control->grab().toImage();
        const auto coloredInkBounds = [glyph](const QImage& image)
        {
            QRect bounds;
            for ( int y = 0; y < image.height(); ++y )
            {
                for ( int x = 0; x < image.width(); ++x )
                {
                    const QColor color = image.pixelColor(x, y);
                    const bool visible = glyph == 0
                        ? color.green() > color.red() + 35 && color.green() > color.blue() + 20
                        : color.blue() > color.red() + 40 && color.green() > color.red() + 30;
                    if ( visible )
                    {
                        bounds = bounds.united(QRect(x, y, 1, 1));
                    }
                }
            }
            return bounds;
        };
        const QRect ink = coloredInkBounds(painted);
        const QSizeF logicalInk(ink.width() / painted.devicePixelRatio(),
                                ink.height() / painted.devicePixelRatio());
        const bool glyphVisible = control->size() == QSize(22, 22)
            && control->iconSize() == QSize(13, 13)
            && logicalInk.width() >= 9.5 && logicalInk.height() >= 9.5;
        check(glyphVisible,
              "resource workspace Sound polish: exact 13 px Play and Loop glyphs remain visible inside unchanged 22 px buttons");
        if ( !glyphVisible )
        {
            const QImage source = control->icon().pixmap(QSize(13, 13)).toImage();
            const QRect sourceInk = coloredInkBounds(source);
            std::fprintf(stderr,
                "resource workspace Sound glyph paint diagnostic: name=%s control=%dx%d icon=%dx%d painted=%dx%d dpr=%.3f ink=%d,%d,%d,%d logical=%.3f,%.3f source=%dx%d sourceDpr=%.3f sourceInk=%d,%d,%d,%d\n",
                glyphNames[glyph], control->width(), control->height(), control->iconSize().width(),
                control->iconSize().height(), painted.width(), painted.height(),
                static_cast<double>(painted.devicePixelRatio()), ink.x(), ink.y(), ink.width(), ink.height(),
                logicalInk.width(), logicalInk.height(), source.width(), source.height(),
                static_cast<double>(source.devicePixelRatio()), sourceInk.x(), sourceInk.y(),
                sourceInk.width(), sourceInk.height());
        }
    }

    QToolButton* const remove = tree->findChild<QToolButton*>(QStringLiteral("snd.remove"));
    check(remove && remove->isVisible(), "resource workspace Sound hover transition: Remove begins visible");
    if ( !remove || !remove->isVisible() )
    {
        return;
    }
    const QPoint originalCursor = QCursor::pos();
    QCursor::setPos(tree->viewport()->mapToGlobal(rowPoint));
    const QPoint buttonPoint = remove->rect().center();
    const QPoint globalPoint = remove->mapToGlobal(buttonPoint);
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(tree->viewport(), &leave);
    QEnterEvent enter(QPointF(buttonPoint), QPointF(remove->mapTo(pane.get(), buttonPoint)), QPointF(globalPoint));
    QApplication::sendEvent(remove, &enter);
    const bool visibleOnEnter = remove->isVisible();
    QMouseEvent buttonMove(QEvent::MouseMove, QPointF(buttonPoint), QPointF(globalPoint),
                           Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(remove, &buttonMove);
    const bool hoverRetained = visibleOnEnter && remove->isVisible();
    if ( hoverRetained )
    {
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(buttonPoint), QPointF(globalPoint),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(remove, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(buttonPoint), QPointF(globalPoint),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(remove, &release);
    }
    QCursor::setPos(originalCursor);
    const QCborArray after = ui::resources::captureResourceAuthoringPaneState(pane.get())
                                .value(QStringLiteral("rows")).toArray();
    check(hoverRetained && after == QCborArray{rows.at(1)} &&
              soundAlternativeByWav(tree, QStringLiteral("second.wav")),
          "resource workspace Sound hover transition: entering Remove with event coordinates retains the control and removes only its current source row");
}

void checkSoundHeaderAnimationReversesOnSecondClick()
{
    QCborArray rows;
    for ( int row = 0; row < 20; ++row )
    {
        rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("entry%1.wav").arg(row)));
    }
    auto pane = soundFollowupPane(rows);
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    QTreeWidgetItem* const mode = tree ? soundModeByBytes(tree, QByteArrayLiteral("moving")) : nullptr;
    check(tree && mode, "resource workspace Sound reversible animation: real mode exists");
    if ( !tree || !mode )
    {
        return;
    }
    pane->setStyleSheet(ui::applicationWindowStyleSheet());
    tree->setAnimated(false);
    mode->setExpanded(false);
    QApplication::processEvents(QEventLoop::AllEvents);
    tree->setAnimated(true);
    const QPoint title = tree->visualItemRect(mode).center();
    const auto clickHeader = [&](const QEvent::Type pressType = QEvent::MouseButtonPress)
    {
        QMouseEvent press(pressType, QPointF(title), QPointF(tree->viewport()->mapToGlobal(title)),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(title), QPointF(tree->viewport()->mapToGlobal(title)),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &release);
    };
    clickHeader();
    const bool expanding = mode->isExpanded();
    clickHeader();
    const bool reversed = !mode->isExpanded();
    const QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
    check(expanding && reversed && tree->isAnimated() && state.value(QStringLiteral("rows")).toArray() == rows &&
              !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "resource workspace Sound reversible animation: a second real header click reverses the in-progress expansion without changing entries or disabling animation");
    clickHeader();
    const bool doubleClickExpanding = mode->isExpanded();
    clickHeader(QEvent::MouseButtonDblClick);
    const QCborMap doubleClickState = ui::resources::captureResourceAuthoringPaneState(pane.get());
    check(doubleClickExpanding && !mode->isExpanded() && tree->isAnimated() &&
              doubleClickState.value(QStringLiteral("rows")).toArray() == rows &&
              !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "resource workspace Sound reversible animation: a native Press Release DblClick Release sequence reverses the mode target once and retains animated clean entries");
}

void checkSoundInactiveVolumeMatchesEditorAtNarrowWidths()
{
    for ( const int width : {74, 96, 170} )
    {
        QCborArray rows;
        rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("none")));
        auto pane = soundFollowupPane(rows);
        QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
        QTreeWidgetItem* const row = tree ? soundAlternativeByWav(tree, QStringLiteral("none")) : nullptr;
        check(tree && row, "resource workspace Sound inactive volume: real alternative exists");
        if ( !tree || !row )
        {
            return;
        }
        pane->setStyleSheet(ui::applicationWindowStyleSheet());
        pane->resize(800, 500);
        tree->setAnimated(false);
        tree->setAutoScroll(false);
        row->parent()->setExpanded(true);
        tree->setColumnWidth(5, width);
        check(tree->columnWidth(5) == 230,
              "resource workspace Sound inactive volume: subminimum section requests clamp to the fixed 230-pixel Volume minimum");
        tree->setCurrentItem(row->parent(), 0);
        const QModelIndex volumeIndex = tree->indexFromItem(row, 5);
        tree->scrollTo(volumeIndex, QAbstractItemView::EnsureVisible);
        QApplication::processEvents(QEventLoop::AllEvents);
        const QRect cell = tree->visualRect(volumeIndex);
        const int horizontalScroll = tree->horizontalScrollBar()->value();
        const bool cellVisible = !cell.isEmpty() && tree->viewport()->rect().contains(cell);
        check(cellVisible, "resource workspace Sound inactive volume: the complete volume cell is visible before capture");
        if ( !cellVisible )
        {
            return;
        }
        const QImage inactive = tree->viewport()->grab().toImage();
        tree->setCurrentItem(row, 5);
        tree->editItem(row, 5);
        QApplication::processEvents(QEventLoop::AllEvents);
        QWidget* const editor = tree->indexWidget(tree->indexFromItem(row, 5));
        QSlider* const slider = editor ? editor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) : nullptr;
        QLineEdit* const value = editor ? editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) : nullptr;
        check(editor && slider && value && editor->isVisible(),
              "resource workspace Sound inactive volume: the actual compound editor renders");
        if ( !editor || !slider || !value )
        {
            return;
        }
        const QImage active = tree->viewport()->grab().toImage();
        const bool sameLayout = tree->visualRect(volumeIndex) == cell &&
                                tree->horizontalScrollBar()->value() == horizontalScroll;
        check(sameLayout && !inactive.isNull() && !active.isNull(),
              "resource workspace Sound inactive volume: activation retains identical visible cell and scroll coordinates");
        if ( !sameLayout || inactive.isNull() || active.isNull() )
        {
            return;
        }
        QStyleOptionSlider controls;
        controls.initFrom(slider);
        controls.rect = slider->rect();
        controls.orientation = Qt::Horizontal;
        controls.minimum = 0;
        controls.maximum = 1000;
        controls.sliderPosition = slider->value();
        controls.sliderValue = slider->value();
        const QPoint origin = slider->mapTo(tree->viewport(), QPoint{});
        const QRect sliderRect(origin, slider->size());
        const QRect groove = slider->style()->subControlRect(QStyle::CC_Slider, &controls,
                                                             QStyle::SC_SliderGroove, slider).translated(origin);
        const QRect handle = slider->style()->subControlRect(QStyle::CC_Slider, &controls,
                                                             QStyle::SC_SliderHandle, slider).translated(origin);
        const QRect valueRect(value->mapTo(tree->viewport(), QPoint{}), value->size());
        const QRect grooveLane(sliderRect.left(), sliderRect.center().y() - 3, sliderRect.width(), 7);
        const auto pixel = [](const QImage& image, const int x, const int y) -> std::optional<QColor>
        {
            const QPoint physical(static_cast<int>(std::floor((x + 0.5) * image.devicePixelRatio())),
                                  static_cast<int>(std::floor((y + 0.5) * image.devicePixelRatio())));
            return image.rect().contains(physical) ? std::optional<QColor>(image.pixelColor(physical)) : std::nullopt;
        };
        const QRect editorRect(editor->mapTo(tree->viewport(), QPoint{}), editor->size());
        // The reserved Remove strip retains the selected row backdrop; only the control canvas is gray.
        const QRect canvas = editorRect.adjusted(2, 2, -2, -2).intersected(cell);
        bool probesInBounds = true;
        int canvasProbes = 0;
        bool inactiveCanvas = true;
        bool activeCanvas = true;
        QRect inactiveInk;
        QRect activeInk;
        for ( int y = canvas.top(); y <= canvas.bottom(); ++y )
        {
            for ( int x = canvas.left(); x <= canvas.right(); ++x )
            {
                const QPoint point(x, y);
                const std::optional<QColor> before = pixel(inactive, x, y);
                const std::optional<QColor> after = pixel(active, x, y);
                if ( !before || !after )
                {
                    probesInBounds = false;
                    continue;
                }
                if ( !groove.intersected(grooveLane).adjusted(-1, -1, 1, 1).contains(point) &&
                     !handle.adjusted(-1, -1, 1, 1).contains(point) &&
                     !valueRect.adjusted(-1, -1, 1, 1).contains(point) )
                {
                    ++canvasProbes;
                    inactiveCanvas = inactiveCanvas && *before == QColor(48, 48, 48);
                    activeCanvas = activeCanvas && *after == QColor(48, 48, 48);
                }
                if ( !valueRect.contains(point) && before->blue() > before->red() + 35 &&
                     before->green() > before->red() + 25 )
                {
                    inactiveInk = inactiveInk.united(QRect(point, QSize(1, 1)));
                }
                if ( !valueRect.contains(point) && after->blue() > after->red() + 35 &&
                     after->green() > after->red() + 25 )
                {
                    activeInk = activeInk.united(QRect(point, QSize(1, 1)));
                }
            }
        }
        const bool stableGeometry = !inactiveInk.isEmpty() && !activeInk.isEmpty() &&
                                    std::abs(inactiveInk.left() - activeInk.left()) <= 1 &&
                                    std::abs(inactiveInk.top() - activeInk.top()) <= 1 &&
                                    std::abs(inactiveInk.right() - activeInk.right()) <= 1 &&
                                    std::abs(inactiveInk.bottom() - activeInk.bottom()) <= 1;
        if ( !probesInBounds || canvasProbes == 0 || !inactiveCanvas || !activeCanvas || !stableGeometry )
        {
            std::cerr << "Sound narrow volume diagnostic: width=" << width
                      << " inactiveCanvas=" << inactiveCanvas << " activeCanvas=" << activeCanvas
                      << " probesInBounds=" << probesInBounds << " canvasProbes=" << canvasProbes
                      << " stableGeometry=" << stableGeometry << " inactiveInk=" << inactiveInk.left() << ','
                      << inactiveInk.top() << ',' << inactiveInk.width() << ',' << inactiveInk.height()
                      << " activeInk=" << activeInk.left() << ',' << activeInk.top() << ','
                      << activeInk.width() << ',' << activeInk.height() << " dpr=" << inactive.devicePixelRatio()
                      << '\n';
            inactive.copy().save(QDir(QDir::tempPath()).filePath(
                QStringLiteral("resource-snd-inactive-volume-%1.png").arg(width)));
            active.copy().save(QDir(QDir::tempPath()).filePath(
                QStringLiteral("resource-snd-active-volume-%1.png").arg(width)));
        }
        check(probesInBounds && canvasProbes > 0 && inactiveCanvas && activeCanvas && stableGeometry &&
                  value->text() == QStringLiteral("0.375"),
              "resource workspace Sound inactive volume: narrow and normal cells paint only matching groove, handle and value on the plain gray canvas before and after activation");
    }
}

void checkSoundVolumeCentersMarginsAndRemoveAtViewportEdge()
{
    ui::ensureGlobalComponentStyles();
    for ( const int requestedWidth : {229, 230, 231, 320, 850} )
    {
        for ( const bool clipRight : {false, true} )
        {
            // Exercise the requested narrow size and an 18 px viewport clip;
            // any necessary minimum belongs only to the volume column.
            QCborArray rows;
            rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("none")));
            auto pane = soundFollowupPane(rows);
            QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
            QTreeWidgetItem* const row = tree ? soundAlternativeByWav(tree, QStringLiteral("none")) : nullptr;
            check(tree && row, "resource workspace Sound volume geometry: actual mode and alternative exist");
            if ( !tree || !row )
            {
                return;
            }
            pane->setStyleSheet(ui::applicationWindowStyleSheet());
            pane->resize(requestedWidth == 850 ? QSize(1340, 700) : QSize(800, 500));
            tree->setAnimated(false);
            tree->setAutoScroll(false);
            tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
            tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            row->parent()->setExpanded(true);
            tree->setColumnWidth(5, requestedWidth);
            check(tree->columnWidth(5) == std::max(230, requestedWidth),
                  "resource workspace Sound volume geometry: boundary and wide section requests retain the fixed 230-pixel Volume minimum");
            tree->setCurrentItem(row->parent(), 0);
            const QModelIndex index = tree->indexFromItem(row, 5);
            tree->scrollTo(index, QAbstractItemView::EnsureVisible);
            QApplication::processEvents(QEventLoop::AllEvents);
            int horizontalScroll = tree->horizontalScrollBar()->value();
            const auto restoreScroll = [&]()
            {
                QScrollBar* const bar = tree->horizontalScrollBar();
                // A changed header layout can leave its offset stale while
                // the bar value is unchanged. Exercise normal range-driven
                // scrolling so Qt publishes the corresponding offset again.
                if ( bar->value() == horizontalScroll && tree->header()->offset() != horizontalScroll )
                {
                    const int other = horizontalScroll == bar->minimum() ? bar->maximum() : bar->minimum();
                    bar->setValue(other);
                }
                bar->setValue(horizontalScroll);
            };
            if ( clipRight )
            {
                const QRect original = tree->visualRect(index);
                const int growth = std::max(0, tree->viewport()->rect().right() + 18 - original.right());
                tree->setColumnWidth(4, tree->columnWidth(4) + growth);
                tree->setColumnWidth(5, std::max(230, requestedWidth));
                int wantedScroll = 0;
                QScrollBar* const horizontalBar = tree->horizontalScrollBar();
                const bool scrollRangeReady = waitUntil([&]()
                {
                    tree->viewport()->grab();
                    wantedScroll = tree->header()->sectionPosition(5) + tree->columnWidth(5) - 1 -
                                   (tree->viewport()->rect().right() + 18);
                    return wantedScroll >= horizontalBar->minimum() && wantedScroll <= horizontalBar->maximum();
                });
                check(scrollRangeReady,
                      "resource workspace Sound volume geometry: native range settles after creating the exact right-side clip");
                if ( !scrollRangeReady )
                {
                    return;
                }
                check(wantedScroll >= tree->horizontalScrollBar()->minimum() &&
                          wantedScroll <= tree->horizontalScrollBar()->maximum(),
                      "resource workspace Sound volume geometry: actual Volume bounds admit the exact eighteen-pixel clip");
                if ( wantedScroll < tree->horizontalScrollBar()->minimum() ||
                     wantedScroll > tree->horizontalScrollBar()->maximum() )
                {
                    return;
                }
                horizontalScroll = wantedScroll;
            }
            restoreScroll();
            const bool viewportSegmentReady = waitUntil([&]()
            {
                restoreScroll();
                tree->viewport()->grab();
                const QRect currentCell = tree->visualRect(index);
                const QRect currentVisible = currentCell.intersected(tree->viewport()->rect());
                return tree->horizontalScrollBar()->value() == horizontalScroll &&
                       tree->header()->offset() == horizontalScroll && currentCell.left() >= 0 &&
                       (clipRight ? currentCell.width() - currentVisible.width() == 18
                                  : currentVisible == currentCell);
            });
            const QRect cell = tree->visualRect(index);
            const int headerOffset = tree->header()->offset();
            const QRect visible = cell.intersected(tree->viewport()->rect());
            check(viewportSegmentReady && !visible.isEmpty() && cell.left() >= 0 &&
                      headerOffset == horizontalScroll &&
                      (clipRight ? cell.width() - visible.width() == 18 : visible == cell),
                  "resource workspace Sound volume geometry: exact full and right-clipped viewport cases are exercised");
            if ( visible.isEmpty() || cell.left() < 0 )
            {
                return;
            }
            const QPoint hover(visible.left() + 4, cell.center().y());
            QMouseEvent move(QEvent::MouseMove, QPointF(hover), QPointF(tree->viewport()->mapToGlobal(hover)),
                             Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(tree->viewport(), &move);
            QToolButton* const remove = tree->findChild<QToolButton*>(QStringLiteral("snd.remove"));
            check(remove && remove->isVisible(), "resource workspace Sound volume geometry: real shared Remove is visible");
            if ( !remove || !remove->isVisible() )
            {
                return;
            }
            const QImage inactive = tree->viewport()->grab().toImage();
            const QImage paneInactive = pane->grab().toImage();
            tree->setCurrentItem(row, 5);
            tree->editItem(row, 5);
            QApplication::processEvents(QEventLoop::AllEvents);
            // Native editor activation scrolls its column into view. Restore
            // the chosen viewport clip, then inspect the actual resized editor.
            const bool layoutCompleted = waitUntil([&]()
            {
                restoreScroll();
                tree->viewport()->grab();
                return tree->horizontalScrollBar()->value() == horizontalScroll &&
                       tree->header()->offset() == headerOffset && tree->visualRect(index) == cell;
            });
            QWidget* const editor = tree->indexWidget(index);
            QSlider* const slider = editor ? editor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) : nullptr;
            QLineEdit* const value = editor ? editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) : nullptr;
            check(editor && slider && value && editor->isVisible(),
                  "resource workspace Sound volume geometry: focused slider and exact value are rendered");
            if ( !editor || !slider || !value )
            {
                return;
            }
            const QImage active = tree->viewport()->grab().toImage();
            const QImage paneActive = pane->grab().toImage();
            const QRect sliderRect(slider->mapTo(tree->viewport(), QPoint{}), slider->size());
            const QRect valueRect(value->mapTo(tree->viewport(), QPoint{}), value->size());
            const QRect paneValueRect(value->mapTo(pane.get(), QPoint{}), value->size());
            const QRect removeRect(remove->mapTo(tree->viewport(), QPoint{}), remove->size());
            const int contentRight = std::min(visible.right() - 4, removeRect.left() - 5);
            const QRect usefulCanvas(QPoint(visible.left() + 4, cell.top() + 2),
                                     QPoint(contentRight, cell.bottom() - 2));
            const bool stableLayout = layoutCompleted && tree->visualRect(index) == cell &&
                                      tree->horizontalScrollBar()->value() == horizontalScroll;
            const bool margins = sliderRect.left() >= visible.left() + 4 &&
                                 sliderRect.width() >= 12 &&
                                 valueRect.right() <= contentRight &&
                                 sliderRect.right() + 4 <= valueRect.left() &&
                                 visible.contains(sliderRect) && visible.contains(valueRect);
            const bool separate = !removeRect.intersects(sliderRect) && !removeRect.intersects(valueRect) &&
                                  valueRect.right() + 4 < removeRect.left();
            const auto pixel = [](const QImage& image, const QPoint& logical) -> std::optional<QColor>
            {
                const QPoint physical(static_cast<int>(std::floor((logical.x() + 0.5) * image.devicePixelRatio())),
                                      static_cast<int>(std::floor((logical.y() + 0.5) * image.devicePixelRatio())));
                return image.rect().contains(physical) ? std::optional<QColor>(image.pixelColor(physical)) : std::nullopt;
            };
            const auto inkBounds = [&](const QImage& image, const QRect& region, const bool cyan)
            {
                QRect ink;
                for ( int y = region.top(); y <= region.bottom(); ++y )
                {
                    for ( int x = region.left(); x <= region.right(); ++x )
                    {
                        const auto color = pixel(image, QPoint(x, y));
                        if ( !color )
                        {
                            continue;
                        }
                        const bool painted = cyan
                            ? color->blue() > color->red() + 35 && color->green() > color->red() + 25
                            : color->red() > 180 && color->green() > 180 && color->blue() > 180;
                        if ( painted )
                        {
                            ink = ink.united(QRect(QPoint(x, y), QSize(1, 1)));
                        }
                    }
                }
                return ink;
            };
            const QRect beforeSlider = inkBounds(inactive, sliderRect.intersected(visible), true);
            const QRect afterSlider = inkBounds(active, sliderRect.intersected(visible), true);
            const QRect beforeValue = inkBounds(inactive, valueRect.intersected(visible), false);
            const QRect afterValue = inkBounds(active, valueRect.intersected(visible), false);
            const QRect paneBeforeValue = inkBounds(paneInactive, paneValueRect, false);
            const QRect paneAfterValue = inkBounds(paneActive, paneValueRect, false);
            const bool paneValueVisible = !paneBeforeValue.isEmpty() && !paneAfterValue.isEmpty()
                && std::abs(paneBeforeValue.left() - paneAfterValue.left()) <= 1
                && std::abs(paneBeforeValue.top() - paneAfterValue.top()) <= 1
                && std::abs(paneBeforeValue.right() - paneAfterValue.right()) <= 1
                && std::abs(paneBeforeValue.bottom() - paneAfterValue.bottom()) <= 1;
            const auto centered = [&cell](const QRect& ink)
            {
                return !ink.isEmpty() && std::abs((ink.top() + ink.bottom()) - (cell.top() + cell.bottom())) <= 2;
            };
            const bool centeredControls = centered(beforeSlider) && centered(afterSlider) &&
                                          centered(beforeValue) && centered(afterValue);
            // QTextLine painting in QLineEdit rounds its natural text width;
            // QPainter::drawText aligns it independently. At fractional DPR,
            // the final AA edge can occupy the adjacent sampled logical bin.
            // Keep exact origin, baseline-height and slider geometry, permitting
            // only that one device-pixel bin at the far text edge.
            const int edgeBin = static_cast<int>(std::ceil(1.0 / std::min(inactive.devicePixelRatio(), active.devicePixelRatio())));
            const bool samePaintedGeometry = beforeSlider == afterSlider &&
                                             beforeValue.topLeft() == afterValue.topLeft() &&
                                             beforeValue.height() == afterValue.height() &&
                                             std::abs(beforeValue.right() - afterValue.right()) <= edgeBin;
            bool completeTrack = true;
            if ( requestedWidth == 320 && !clipRight )
            {
                // A 0.375 slider has an unfilled right segment. Its actual
                // painted track must be visible in both representations.
                for ( int x = sliderRect.center().x(); x < sliderRect.right() - 4; ++x )
                {
                    const auto before = pixel(inactive, QPoint(x, cell.center().y()));
                    const auto after = pixel(active, QPoint(x, cell.center().y()));
                    completeTrack = completeTrack && before && after &&
                                    *before != QColor(48, 48, 48) && *after != QColor(48, 48, 48);
                }
            }
            bool plainMargins = usefulCanvas.isValid();
            for ( const int y : {usefulCanvas.top(), usefulCanvas.bottom()} )
            {
                for ( int x = usefulCanvas.left(); x <= usefulCanvas.right(); ++x )
                {
                    const auto before = pixel(inactive, QPoint(x, y));
                    const auto after = pixel(active, QPoint(x, y));
                    plainMargins = plainMargins && before && after &&
                                   *before == QColor(48, 48, 48) && *after == QColor(48, 48, 48);
                }
            }
            const bool fullValue = value->text() == QStringLiteral("0.375") &&
                                   valueRect.width() >= QFontMetrics(value->font()).horizontalAdvance(QStringLiteral("0.375"));
            if ( !stableLayout || !margins || !separate || !centeredControls || !samePaintedGeometry ||
                 !completeTrack || !plainMargins || !fullValue || !paneValueVisible )
            {
                std::cerr << "Sound centered volume diagnostic: requestedWidth=" << requestedWidth
                          << " clipped=" << clipRight << " stable=" << stableLayout << " margins=" << margins
                          << " separate=" << separate << " centered=" << centeredControls
                          << " samePaint=" << samePaintedGeometry << " completeTrack=" << completeTrack
                          << " plainMargins=" << plainMargins << " fullValue=" << fullValue
                          << " paneValueVisible=" << paneValueVisible
                          << " rowY=" << cell.top() << ',' << cell.height() << " sliderInkY="
                          << beforeSlider.top() << ',' << beforeSlider.height() << " valueInkY="
                          << beforeValue.top() << ',' << beforeValue.height() << " removeX=" << removeRect.left()
                          << " valueRight=" << valueRect.right() << " dpr=" << inactive.devicePixelRatio() << '\n';
                const auto printRect = [](const char* const name, const QRect& rect)
                {
                    std::cerr << ' ' << name << '=' << rect.left() << ',' << rect.top() << ','
                              << rect.width() << ',' << rect.height();
                };
                std::cerr << "Sound exact paint bounds:";
                printRect("beforeSlider", beforeSlider);
                printRect("afterSlider", afterSlider);
                printRect("beforeValue", beforeValue);
                printRect("afterValue", afterValue);
                printRect("paneBeforeValue", paneBeforeValue);
                printRect("paneAfterValue", paneAfterValue);
                printRect("beforeCell", cell);
                printRect("afterCell", tree->visualRect(index));
                printRect("editor", QRect(editor->mapTo(tree->viewport(), QPoint{}), editor->size()));
                std::cerr << " beforeScroll=" << horizontalScroll
                          << " afterScroll=" << tree->horizontalScrollBar()->value()
                          << " beforeOffset=" << headerOffset << " afterOffset=" << tree->header()->offset()
                          << " scrollMaximum=" << tree->horizontalScrollBar()->maximum()
                          << " layoutCompleted=" << layoutCompleted << '\n';
                const QString suffix = QStringLiteral("%1-%2").arg(requestedWidth).arg(clipRight ? "clipped" : "full");
                inactive.save(QDir(QDir::tempPath()).filePath(
                    QStringLiteral("resource-snd-centered-inactive-%1.png").arg(suffix)));
                active.save(QDir(QDir::tempPath()).filePath(
                    QStringLiteral("resource-snd-centered-active-%1.png").arg(suffix)));
            }
            check(stableLayout && centeredControls && samePaintedGeometry && fullValue,
                  "resource workspace Sound volume geometry: inactive and focused groove handle and full value stay vertically centered without a layout jump");
            check(paneValueVisible,
                  "resource workspace Sound volume geometry: inactive and focused exact value glyphs remain visible in both the viewport and whole-pane capture");
            check(margins && separate && plainMargins,
                  "resource workspace Sound volume geometry: full and partially clipped narrow cells retain four-pixel margins and keep Remove clear of slider and value");
            check(completeTrack,
                  "resource workspace Sound volume geometry: inactive and focused sliders both paint the complete unfilled right track");
            check(ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")).toArray() == rows &&
                      !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
                  "resource workspace Sound volume geometry: focus hover resize and paint preserve exact clean raw rows");
        }
    }
}

void checkSoundRemoveDoesNotCoverOtherColumns()
{
    for ( const int volumeOffset : {8, -18} )
    {
        QCborArray rows;
        rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("none")));
        auto pane = soundFollowupPane(rows);
        QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
        QTreeWidgetItem* const row = tree ? soundAlternativeByWav(tree, QStringLiteral("none")) : nullptr;
        check(tree && row, "resource workspace Sound Remove reservation: actual mode and alternative exist");
        if ( !tree || !row )
        {
            return;
        }
        pane->setStyleSheet(ui::applicationWindowStyleSheet());
        pane->resize(800, 500);
        tree->setAnimated(false);
        tree->setAutoScroll(false);
        row->parent()->setExpanded(true);
        tree->setCurrentItem(row->parent(), 0);
        tree->horizontalScrollBar()->setValue(0);
        QApplication::processEvents(QEventLoop::AllEvents);
        const QModelIndex volumeIndex = tree->indexFromItem(row, 5);
        const int growth = tree->viewport()->width() + volumeOffset - tree->visualRect(volumeIndex).left();
        tree->setColumnWidth(4, tree->columnWidth(4) + growth);
        tree->setColumnWidth(5, 255);
        tree->horizontalScrollBar()->setValue(0);
        const bool visibleSegmentReady = waitUntil([&]()
        {
            tree->viewport()->grab();
            const QRect currentVolume = tree->visualRect(volumeIndex);
            const QRect currentVisible = currentVolume.intersected(tree->viewport()->rect());
            return tree->horizontalScrollBar()->value() == 0 && tree->header()->offset() == 0 &&
                   (volumeOffset > 0 ? currentVisible.isEmpty() : currentVisible.width() == 18);
        });
        const QRect volume = tree->visualRect(volumeIndex);
        const QPoint hover(tree->viewport()->width() - 40, tree->visualItemRect(row).center().y());
        const QModelIndex hovered = tree->indexAt(hover);
        const QRect visible = volume.intersected(tree->viewport()->rect());
        check(visibleSegmentReady && hovered.isValid() && hovered.column() == 4 && tree->itemFromIndex(hovered) == row &&
                  (volumeOffset > 0 ? visible.isEmpty() : visible.width() == 18),
              "resource workspace Sound Remove reservation: Max radius is visible while volume is offscreen or clipped to eighteen pixels");
        QMouseEvent move(QEvent::MouseMove, QPointF(hover), QPointF(tree->viewport()->mapToGlobal(hover)),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &move);
        QToolButton* const remove = tree->findChild<QToolButton*>(QStringLiteral("snd.remove"));
        check(remove && !remove->isVisible(),
              "resource workspace Sound Remove reservation: an unavailable right-side slot hides Remove instead of covering another editable column");
        check(ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")).toArray() == rows &&
                  !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
              "resource workspace Sound Remove reservation: resizing and hovering retain the exact clean source row");
    }
}

void checkSoundWholeEditorNarrowVolumeReservation()
{
    ui::ensureGlobalComponentStyles();
    for ( const int requestedPaneWidth : {520, 420, 350} )
    {
        for ( const int requestedVisibleVolume : {8, 26, 40, 60, 92, 170} )
        {
            QCborArray rows;
            rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("none")));
            auto context = testContext(ProjectType::IJIM);
            context.groupedWorkspaceChrome = true;
            auto pane = soundFollowupPane(rows, std::move(context));
            QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
            QTreeWidgetItem* const row = tree ? soundAlternativeByWav(tree, QStringLiteral("none")) : nullptr;
            check(tree && row, "resource workspace Sound whole-editor narrowing: actual embedded pane and entry exist");
            if ( !tree || !row )
            {
                return;
            }
            pane->setStyleSheet(ui::applicationWindowStyleSheet());
            tree->setAnimated(false);
            tree->setAutoScroll(false);
            tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
            tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            row->parent()->setExpanded(true);
            tree->setCurrentItem(row->parent(), 0);
            pane->resize(requestedPaneWidth, 500);
            QApplication::processEvents(QEventLoop::AllEvents);
            tree->viewport()->grab();
            check(pane->width() == std::max(requestedPaneWidth, pane->minimumWidth()),
                  "resource workspace Sound whole-editor narrowing: a below-minimum request retains the compact native pane constraint");
            QTreeWidgetItem* const headings = row->parent()->child(0);
            check(headings && headings->text(5) == QStringLiteral("Volume") &&
                      tree->headerItem()->text(5) == QStringLiteral("Volume"),
                  "resource workspace Sound whole-editor narrowing: every mode uses the exact Volume heading");
            const QModelIndex volumeIndex = tree->indexFromItem(row, 5);
            QScrollBar* const bar = tree->horizontalScrollBar();
            tree->setColumnWidth(5, 255);
            QApplication::processEvents(QEventLoop::AllEvents);
            tree->viewport()->grab();
            const int missingPrefix = tree->viewport()->width() - requestedVisibleVolume + 32
                                      - tree->header()->sectionPosition(5);
            if ( missingPrefix > 0 )
            {
                tree->setColumnWidth(4, tree->columnWidth(4) + missingPrefix);
            }
            tree->setColumnWidth(5, 255);
            // Header resizing queues the native scroll-range update even if
            // this segment needs no additional prefix. Settle it before using
            // maximum(), retaining the same pane and exact visible widths.
            QApplication::processEvents(QEventLoop::AllEvents);
            tree->viewport()->grab();
            const int wantedScroll = tree->header()->sectionPosition(5) -
                                     (tree->viewport()->width() - requestedVisibleVolume);
            if ( wantedScroll < bar->minimum() || wantedScroll > bar->maximum() )
            {
                std::cerr << "Sound whole-editor scroll-range diagnostic: requestedPane=" << requestedPaneWidth
                          << " actualPane=" << pane->width() << " viewport=" << tree->viewport()->width()
                          << " visibleVolume=" << requestedVisibleVolume << " prefix="
                          << tree->header()->sectionPosition(5) << " volumeWidth=" << tree->columnWidth(5)
                          << " headerLength=" << tree->header()->length() << " headerOffset="
                          << tree->header()->offset() << " wantedScroll=" << wantedScroll << " range="
                          << bar->minimum() << ',' << bar->maximum() << '\n';
            }
            check(wantedScroll >= bar->minimum() && wantedScroll <= bar->maximum(),
                  "resource workspace Sound whole-editor narrowing: native scroll range admits the requested visible Volume segment");
            if ( wantedScroll < bar->minimum() || wantedScroll > bar->maximum() )
            {
                return;
            }
            const auto restoreScroll = [&]()
            {
                if ( bar->value() == wantedScroll && tree->header()->offset() != wantedScroll )
                {
                    bar->setValue(wantedScroll == bar->minimum() ? bar->maximum() : bar->minimum());
                }
                bar->setValue(wantedScroll);
            };
            const bool inactiveReady = waitUntil([&]()
            {
                restoreScroll();
                tree->viewport()->grab();
                return tree->header()->offset() == wantedScroll &&
                       tree->visualRect(volumeIndex).intersected(tree->viewport()->rect()).width() == requestedVisibleVolume;
            });
            check(inactiveReady, "resource workspace Sound whole-editor narrowing: resized inactive Volume occupies the exact visible scroll segment");
            if ( !inactiveReady )
            {
                return;
            }
            const QRect cell = tree->visualRect(volumeIndex);
            const QRect visible = cell.intersected(tree->viewport()->rect());
            const QPoint hover(visible.left() + 3, cell.center().y());
            QMouseEvent move(QEvent::MouseMove, QPointF(hover), QPointF(tree->viewport()->mapToGlobal(hover)),
                             Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(tree->viewport(), &move);
            QToolButton* const remove = tree->findChild<QToolButton*>(QStringLiteral("snd.remove"));
            check(remove, "resource workspace Sound whole-editor narrowing: actual shared Remove exists");
            if ( !remove )
            {
                return;
            }
            const QImage inactive = tree->viewport()->grab().toImage();
            bool inactiveDoesNotEscape = true;
            // The preceding radius has its value at its left edge. The six
            // pixels immediately before Volume must remain free of the
            // Volume value or slider, even when only a small segment is shown.
            for ( int y = cell.top() + 4; y <= cell.bottom() - 4; ++y )
            {
                for ( int x = std::max(0, visible.left() - 6); x < visible.left(); ++x )
                {
                    const QPoint physical(static_cast<int>(std::floor((x + 0.5) * inactive.devicePixelRatio())),
                                          static_cast<int>(std::floor((y + 0.5) * inactive.devicePixelRatio())));
                    if ( !inactive.rect().contains(physical) )
                    {
                        inactiveDoesNotEscape = false;
                        continue;
                    }
                    const QColor color = inactive.pixelColor(physical);
                    const bool lightValue = color.red() > 180 && color.green() > 180 && color.blue() > 180;
                    const bool cyanSlider = color.blue() > color.red() + 35 && color.green() > color.red() + 25;
                    inactiveDoesNotEscape = inactiveDoesNotEscape && !lightValue && !cyanSlider;
                }
            }
            tree->setCurrentItem(row, 5);
            tree->editItem(row, 5);
            const bool activeReady = waitUntil([&]()
            {
                restoreScroll();
                tree->viewport()->grab();
                return tree->header()->offset() == wantedScroll && tree->visualRect(volumeIndex) == cell &&
                       tree->indexWidget(volumeIndex);
            });
            QWidget* const editor = tree->indexWidget(volumeIndex);
            QSlider* const slider = editor ? editor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) : nullptr;
            QLineEdit* const value = editor ? editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) : nullptr;
            check(activeReady && editor && slider && value,
                  "resource workspace Sound whole-editor narrowing: actual active editor retains the resized cell and native scroll position");
            if ( !editor || !slider || !value )
            {
                return;
            }
            QApplication::sendEvent(tree->viewport(), &move);
            const QRect editorRect(editor->mapTo(tree->viewport(), QPoint{}), editor->size());
            const QRect sliderRect(slider->mapTo(tree->viewport(), QPoint{}), slider->size());
            const QRect valueRect(value->mapTo(tree->viewport(), QPoint{}), value->size());
            const QRect removeRect(remove->mapTo(tree->viewport(), QPoint{}), remove->size());
            const bool controlsContained = visible.contains(editorRect) &&
                                           (valueRect.isEmpty() ? value->size().isEmpty() : editorRect.contains(valueRect)) &&
                                           (sliderRect.isEmpty() || editorRect.contains(sliderRect));
            const bool removeSafe = !remove->isVisible() ||
                                    (visible.contains(removeRect) && !removeRect.intersects(editorRect) &&
                                     !removeRect.intersects(valueRect) && !removeRect.intersects(sliderRect) &&
                                     sliderRect.width() >= 12 &&
                                     valueRect.width() >= QFontMetrics(value->font()).horizontalAdvance(QStringLiteral("0.375")));
            if ( !controlsContained || !removeSafe || !inactiveDoesNotEscape )
            {
                std::cerr << "Sound whole-editor narrowing diagnostic: requestedPane=" << requestedPaneWidth
                          << " actualPane=" << pane->width() << " viewport=" << tree->viewport()->width()
                          << " visibleVolume=" << requestedVisibleVolume << " removeVisible=" << remove->isVisible()
                          << " contained=" << controlsContained << " removeSafe=" << removeSafe
                          << " inactiveContained=" << inactiveDoesNotEscape
                          << " visibleX=" << visible.left() << ',' << visible.width()
                          << " editorX=" << editorRect.left() << ',' << editorRect.width()
                          << " sliderX=" << sliderRect.left() << ',' << sliderRect.width()
                          << " valueX=" << valueRect.left() << ',' << valueRect.width()
                          << " removeX=" << removeRect.left() << ',' << removeRect.width() << '\n';
                const QString suffix = QStringLiteral("%1-%2").arg(requestedPaneWidth).arg(requestedVisibleVolume);
                inactive.save(QDir(QDir::tempPath()).filePath(
                    QStringLiteral("resource-snd-whole-narrow-inactive-%1.png").arg(suffix)));
                tree->viewport()->grab().save(QDir(QDir::tempPath()).filePath(
                    QStringLiteral("resource-snd-whole-narrow-active-%1.png").arg(suffix)));
            }
            check(controlsContained && removeSafe && inactiveDoesNotEscape && value->text() == QStringLiteral("0.375"),
                  "resource workspace Sound whole-editor narrowing: focused value and slider stay in visible Volume and Remove never overlaps them or a preceding editable column");
            if ( requestedVisibleVolume <= 40 )
            {
                check(!remove->isVisible() && editorRect == visible &&
                          (requestedVisibleVolume != 8 || value->size().isEmpty()),
                      "resource workspace Sound whole-editor narrowing: extreme visible segments hide Remove and keep empty or clipped controls inside their own canvas while retaining full raw text");
            }
            if ( remove->isVisible() )
            {
                const QPoint local = remove->rect().center();
                const QPoint global = remove->mapToGlobal(local);
                QEnterEvent enter{QPointF(local), QPointF(local), QPointF(global)};
                QApplication::sendEvent(remove, &enter);
                check(remove->isVisible() && tree->visualRect(volumeIndex) == cell,
                      "resource workspace Sound whole-editor narrowing: hovering its reserved Remove button retains the resized row target");
            }
            check(ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")).toArray() == rows &&
                      !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
                  "resource workspace Sound whole-editor narrowing: window resize scroll focus hover and paint preserve exact clean raw values");
        }
    }
}

void checkSoundClippedVolumeTooltipRetainsRawValue()
{
    QCborArray rows;
    rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("none")));
    auto pane = soundFollowupPane(rows);
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    QTreeWidgetItem* const row = tree ? soundAlternativeByWav(tree, QStringLiteral("none")) : nullptr;
    check(tree && row, "resource workspace Sound clipped tooltip: actual pane and fixed raw entry exist");
    if ( !tree || !row )
    {
        return;
    }
    pane->setStyleSheet(ui::applicationWindowStyleSheet());
    pane->resize(800, 500);
    tree->setAnimated(false);
    tree->setAutoScroll(false);
    tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    tree->setCurrentItem(row->parent(), 0);
    row->parent()->setExpanded(true);
    QApplication::processEvents(QEventLoop::AllEvents);
    tree->viewport()->grab();
    const QModelIndex volumeIndex = tree->indexFromItem(row, 5);
    QScrollBar* const bar = tree->horizontalScrollBar();
    const QString help = QStringLiteral(
        "Maximum volume. Explicit slider edits use 0 to 1. A higher imported value is retained until you edit it.");
    for ( const int visibleWidth : {170, 26} )
    {
        // A right-clipped last column needs enough preceding content to put
        // that edge inside the native scrollbar range. Widen an ordinary
        // radius column as a user can, retaining this pane and Volume width.
        const int missingPrefix = tree->viewport()->width() - visibleWidth + 32
                                  - tree->header()->sectionPosition(5);
        if ( missingPrefix > 0 )
        {
            tree->setColumnWidth(4, tree->columnWidth(4) + missingPrefix
                                    + std::max(0, tree->columnWidth(1) - 160));
            QApplication::processEvents(QEventLoop::AllEvents);
            tree->viewport()->grab();
        }
        const int wantedScroll = tree->header()->sectionPosition(5) - (tree->viewport()->width() - visibleWidth);
        check(wantedScroll >= bar->minimum() && wantedScroll <= bar->maximum(),
              "resource workspace Sound clipped tooltip: native scrolling admits the fixed visible segment");
        if ( wantedScroll < bar->minimum() || wantedScroll > bar->maximum() )
        {
            return;
        }
        const bool ready = waitUntil([&]()
        {
            if ( bar->value() == wantedScroll && tree->header()->offset() != wantedScroll )
            {
                bar->setValue(wantedScroll == bar->minimum() ? bar->maximum() : bar->minimum());
            }
            bar->setValue(wantedScroll);
            tree->viewport()->grab();
            return tree->header()->offset() == wantedScroll &&
                   tree->visualRect(volumeIndex).intersected(tree->viewport()->rect()).width() == visibleWidth;
        });
        check(ready, "resource workspace Sound clipped tooltip: rendered Volume uses the exact native visible width");
        if ( !ready )
        {
            return;
        }
        const QRect visible = tree->visualRect(volumeIndex).intersected(tree->viewport()->rect());
        const QPoint point = visible.center();
        check(tree->indexAt(point) == volumeIndex &&
                  (visibleWidth != 26 || QFontMetrics(tree->font()).horizontalAdvance(QStringLiteral("0.375"))
                      > visible.width() - 12),
              "resource workspace Sound clipped tooltip: the clipped ordinary value cannot fit its painted label");
        QHelpEvent event(QEvent::ToolTip, point, tree->viewport()->mapToGlobal(point));
        QApplication::sendEvent(tree->viewport(), &event);
        const QString expected = visibleWidth == 26 ? help + QStringLiteral("\n0.375") : help;
        if ( QToolTip::text() != expected )
        {
            std::cerr << "Sound clipped tooltip: visibleWidth=" << visibleWidth
                      << " actual=" << QToolTip::text().toStdString()
                      << " expected=" << expected.toStdString() << '\n';
        }
        check(QToolTip::text() == expected,
              "resource workspace Sound clipped tooltip: a clipped value exposes exact help and full raw text while an unelided value keeps ordinary help");
    }
    QToolTip::hideText();
    check(ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")).toArray() == rows &&
              !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "resource workspace Sound clipped tooltip: help and native scrolling preserve exact clean raw values");
}

void checkSoundCompactMinimumKeepsVolumeAndRemoveVisible()
{
    // These fixed logical-pixel vectors describe the compact column contract,
    // independently of the implementation's clamping helper.
    constexpr std::array<int, 6> minima{80, 160, 100, 56, 56, 230};
    constexpr std::array<int, 6> defaults{80, 220, 100, 64, 64, 230};
    constexpr std::array<int, 6> fittedDefaults{80, 220, 100, 64, 64, 230};
    constexpr int minimumColumns = 80 + 160 + 100 + 56 + 56 + 230;
    constexpr int defaultFixedColumns = 80 + 100 + 64 + 64;
    QCborArray rows;
    rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("none")));
    for ( int index = 0; index < 20; ++index )
    {
        rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("entry_%1.wav").arg(index)));
    }
    auto context = testContext(ProjectType::IJIM);
    context.groupedWorkspaceChrome = true;
    auto pane = soundFollowupPane(rows, std::move(context));
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    QTreeWidgetItem* row = tree ? soundAlternativeByWav(tree, QStringLiteral("none")) : nullptr;
    check(tree && row, "resource workspace Sound compact minimum: actual embedded pane and fixed raw row exist");
    if ( !tree || !row )
    {
        return;
    }
    pane->setStyleSheet(ui::applicationWindowStyleSheet());
    tree->setAnimated(false);
    tree->setAutoScroll(false);
    tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    row->parent()->setExpanded(true);
    tree->setCurrentItem(row->parent(), 0);
    pane->resize(1120, 500);
    QApplication::processEvents(QEventLoop::AllEvents);
    tree->viewport()->grab();
    const int defaultChrome = pane->width() - tree->viewport()->width();
    constexpr std::array<int, 4> fittedViewports{682, 758, 1000, 1400};
    constexpr std::array<std::array<int, 6>, 4> fittedVectors{{
        {{80, 160, 100, 56, 56, 230}},
        {{80, 220, 100, 64, 64, 230}},
        {{80, 462, 100, 64, 64, 230}},
        {{80, 806, 100, 64, 64, 286}},
    }};
    bool exactVectors = true;
    for ( std::size_t vector = 0; vector < fittedViewports.size(); ++vector )
    {
        const int viewportWidth = fittedViewports[vector];
        pane->resize(viewportWidth + defaultChrome, 500);
        const bool settled = waitUntil([&]()
        {
            tree->viewport()->grab();
            return tree->viewport()->width() == viewportWidth;
        });
        bool exactVector = settled;
        for ( int column = 0; column < static_cast<int>(fittedVectors[vector].size()); ++column )
        {
            exactVector = exactVector
                && tree->columnWidth(column) == fittedVectors[vector][static_cast<std::size_t>(column)];
        }
        exactVectors = exactVectors && exactVector;
    }
    check(exactVectors,
          "resource workspace Sound compact minimum: fitted viewport vectors are 682=80/160/100/56/56/230, 758=80/220/100/64/64/230, 1000=80/462/100/64/64/230 and 1400=80/806/100/64/64/286 after moving 40% of Volume width into Sound");
    pane->resize(1120, 500);
    const bool settledWide = waitUntil([&]()
    {
        tree->viewport()->grab();
        return pane->width() == 1120;
    });
    const int wideExtra = std::max(0, tree->viewport()->width() - defaultFixedColumns
        - minima[1] - minima[5]);
    const int initialWideSound = minima[1] + static_cast<int>(std::lround(wideExtra * 0.65));
    const int initialWideVolume = std::max(
        minima[5], tree->viewport()->width() - defaultFixedColumns - initialWideSound);
    const int transferredWidth = std::min(
        static_cast<int>(std::lround(initialWideVolume * 0.40)), initialWideVolume - minima[5]);
    const int expectedWideSound = initialWideSound + transferredWidth;
    const int expectedWideVolume = initialWideVolume - transferredWidth;
    check(settledWide && expectedWideVolume >= minima[5]
              && tree->columnWidth(1) == expectedWideSound
              && tree->columnWidth(5) == expectedWideVolume,
          "resource workspace Sound compact minimum: wider fitted viewports first use the 65/35 split, then transfer 40% of Volume width into Sound while keeping Volume at its minimum");
    check(tree->verticalScrollBar()->isVisible(),
          "resource workspace Sound compact minimum: native vertical scrollbar participates in the real viewport chrome");
    const int wideSoundWidth = tree->columnWidth(1);
    QModelIndex volumeIndex = tree->indexFromItem(row, 5);
    const QRect wideVolume = tree->visualRect(volumeIndex);
    const int intendedWideVolumeWidth = tree->columnWidth(5);
    const QPoint wideHover(tree->viewport()->width() - 40, tree->visualItemRect(row).center().y());
    QMouseEvent wideMove(QEvent::MouseMove, QPointF(wideHover), QPointF(tree->viewport()->mapToGlobal(wideHover)),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &wideMove);
    QToolButton* const wideRemove = tree->findChild<QToolButton*>(QStringLiteral("snd.remove"));
    const QRect wideRemoveRect =
        wideRemove ? QRect(wideRemove->mapTo(tree->viewport(), QPoint{}), wideRemove->size()) : QRect{};
    check(wideRemove && wideRemove->isVisible() && !tree->horizontalScrollBar()->isVisible() &&
              wideVolume.right() == tree->viewport()->rect().right() && wideVolume.contains(wideRemoveRect),
          "resource workspace Sound compact minimum: without horizontal scrolling, the real Remove button is reserved inside the filled final Volume cell");

    // Resize Sound last while preserving the manually sized final Volume column.
    const std::array<int, 6> intendedManualWidths{
        minima[0], minima[1], minima[2], minima[3], minima[4], intendedWideVolumeWidth};
    for ( const int column : {0, 2, 3, 4} )
    {
        tree->setColumnWidth(column, minima[static_cast<std::size_t>(column)]);
    }
    tree->setColumnWidth(1, minima[1]);
    QApplication::processEvents(QEventLoop::AllEvents);
    const std::array<int, 6> manualWidthsAfterResize{
        tree->columnWidth(0), tree->columnWidth(1), tree->columnWidth(2),
        tree->columnWidth(3), tree->columnWidth(4), tree->columnWidth(5)};
    check(manualWidthsAfterResize == intendedManualWidths,
        "resource workspace Sound compact minimum: resizing Sound last preserves all six intended column widths");
    QTreeWidgetItem* const wideTarget = soundAlternativeByWav(tree, QStringLiteral("entry_19.wav"));
    const QModelIndex wideTargetVolume = wideTarget ? tree->indexFromItem(wideTarget, 5) : QModelIndex{};
    if ( wideTarget )
    {
        tree->scrollToItem(wideTarget, QAbstractItemView::PositionAtCenter);
    }
    const bool wideTargetVisible = wideTarget && waitUntil([&]()
    {
        tree->viewport()->grab();
        const QRect targetRow = tree->visualItemRect(wideTarget);
        const QRect targetVolume = tree->visualRect(wideTargetVolume);
        return !targetRow.isEmpty() && tree->viewport()->rect().contains(targetRow) &&
               tree->viewport()->rect().contains(targetVolume);
    });
    const QRect soundLastVolume = wideTarget ? tree->visualRect(wideTargetVolume) : QRect{};
    const QPoint soundLastHover(soundLastVolume.isEmpty() ? 0 : soundLastVolume.right() - 40,
                                wideTarget ? tree->visualItemRect(wideTarget).center().y() : 0);
    QMouseEvent soundLastMove(QEvent::MouseMove, QPointF(soundLastHover),
                              QPointF(tree->viewport()->mapToGlobal(soundLastHover)), Qt::NoButton,
                              Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &soundLastMove);
    QToolButton* const soundLastRemove = tree->findChild<QToolButton*>(QStringLiteral("snd.remove"));
    const bool soundLastRemoveVisible = wideTargetVisible && soundLastRemove && waitUntil([&]()
    {
        tree->viewport()->grab();
        const QRect remove = QRect(soundLastRemove->mapTo(tree->viewport(), QPoint{}), soundLastRemove->size());
        return soundLastRemove->isVisible() && tree->viewport()->rect().contains(remove) &&
               soundLastVolume.contains(remove) && tree->itemAt(soundLastHover) == wideTarget;
    });
    const QRect soundLastRemoveRect = soundLastRemove
        ? QRect(soundLastRemove->mapTo(tree->viewport(), QPoint{}), soundLastRemove->size()) : QRect{};
    const std::array<int, 6> manualWidthsAfterHover{
        tree->columnWidth(0), tree->columnWidth(1), tree->columnWidth(2),
        tree->columnWidth(3), tree->columnWidth(4), tree->columnWidth(5)};
    const QCborMap beforeWideRemove = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap beforeWideHistory = beforeWideRemove.value(QStringLiteral("history")).toMap();
    const QCborArray beforeWideRows = beforeWideRemove.value(QStringLiteral("rows")).toArray();
    const bool soundLastContained = wideTargetVisible && soundLastRemoveVisible &&
        manualWidthsAfterResize == intendedManualWidths &&
        manualWidthsAfterHover == manualWidthsAfterResize &&
        !tree->horizontalScrollBar()->isVisible() &&
        tree->viewport()->rect().contains(soundLastVolume) &&
        soundLastVolume.contains(soundLastRemoveRect) && soundLastRemove && soundLastRemove->isVisible();
    if ( !soundLastContained )
    {
        const QRect viewportRect = tree->viewport()->rect();
        const QTreeWidgetItem* const hoverItem = tree->itemAt(soundLastHover);
        std::cerr << "SND last-resize containment mismatch widths=";
        for ( int column = 0; column < 6; ++column )
        {
            std::cerr << tree->columnWidth(column) << (column == 5 ? ' ' : ',');
        }
        std::cerr << " viewport=" << viewportRect.x() << ',' << viewportRect.y()
                  << ',' << viewportRect.width() << ',' << viewportRect.height()
                  << " volume=" << soundLastVolume.x() << ',' << soundLastVolume.y()
                  << ',' << soundLastVolume.width() << ',' << soundLastVolume.height()
                  << " hover=" << soundLastHover.x() << ',' << soundLastHover.y()
                  << " hoverMatchesTarget=" << (hoverItem == wideTarget)
                  << " expectedWidths=";
        for ( std::size_t column = 0; column < intendedManualWidths.size(); ++column )
        {
            std::cerr << intendedManualWidths[column]
                      << (column + 1 == intendedManualWidths.size() ? ' ' : ',');
        }
        std::cerr << " widthsAfterResizeMatch="
                  << (manualWidthsAfterResize == intendedManualWidths)
                  << " widthsStable=" << (manualWidthsAfterHover == manualWidthsAfterResize)
                  << " volumeFullyVisible=" << tree->viewport()->rect().contains(soundLastVolume)
                  << " wideTargetVisible=" << wideTargetVisible
                  << " soundLastRemoveVisible=" << soundLastRemoveVisible
                  << " removeRect=" << soundLastRemoveRect.x() << ',' << soundLastRemoveRect.y()
                  << ',' << soundLastRemoveRect.width() << ',' << soundLastRemoveRect.height() << '\n';
    }
    check(soundLastContained,
          "resource workspace Sound compact minimum: shrinking Sound last preserves the manually sized Volume row with its actual Remove control inside the viewport");
    if ( soundLastContained )
    {
        const QPoint removePoint = soundLastRemove->rect().center();
        const QPoint removeGlobal = soundLastRemove->mapToGlobal(removePoint);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(removePoint), QPointF(removeGlobal), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(soundLastRemove, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(removePoint), QPointF(removeGlobal), Qt::LeftButton,
                           Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(soundLastRemove, &release);
        QCborArray expectedWideRows;
        for ( qsizetype index = 0; index + 1 < beforeWideRows.size(); ++index )
        {
            expectedWideRows.append(beforeWideRows.at(index));
        }
        const QCborMap afterWideRemove = ui::resources::captureResourceAuthoringPaneState(pane.get());
        const QCborMap wideRemoveHistory = afterWideRemove.value(QStringLiteral("history")).toMap();
        const QCborArray wideRemoveCommands = wideRemoveHistory.value(QStringLiteral("commands")).toArray();
        const QCborMap wideRemoveCommand = wideRemoveCommands.size() == 1
            ? wideRemoveCommands.at(0).toMap() : QCborMap{};
        const QCborMap wideRemovedEntry = wideRemoveCommand.value(QStringLiteral("before")).toMap();
        const bool wideRemovedExactly =
            afterWideRemove.value(QStringLiteral("rows")).toArray() == expectedWideRows &&
            afterWideRemove.value(QStringLiteral("modified")).toBool() &&
            wideRemoveHistory.value(QStringLiteral("cursor")).toInteger() == 1 && wideRemoveCommands.size() == 1 &&
            wideRemoveCommand.value(QStringLiteral("row")).toInteger() == beforeWideRows.size() - 1 &&
            wideRemovedEntry.value(QStringLiteral("raw")).toArray() == beforeWideRows.last().toArray() &&
            wideRemoveCommand.value(QStringLiteral("after")).isNull();
        check(wideRemovedExactly,
          "resource workspace Sound compact minimum: the wide-pane Remove click without horizontal scrolling records only the exact final raw row");
        const bool wideUndone = ui::resources::resourceAuthoringPaneUndoRedo(pane.get(), false);
        const QCborMap afterWideUndo = ui::resources::captureResourceAuthoringPaneState(pane.get());
        const QCborMap wideUndoHistory = afterWideUndo.value(QStringLiteral("history")).toMap();
        check(wideUndone && afterWideUndo.value(QStringLiteral("rows")).toArray() == beforeWideRows &&
                  !afterWideUndo.value(QStringLiteral("modified")).toBool() &&
                  wideUndoHistory.value(QStringLiteral("commands")).toArray() == wideRemoveCommands &&
                  wideUndoHistory.value(QStringLiteral("cursor")).toInteger() ==
                      beforeWideHistory.value(QStringLiteral("cursor")).toInteger() &&
                  wideUndoHistory.value(QStringLiteral("clean")).toInteger() ==
                      beforeWideHistory.value(QStringLiteral("clean")).toInteger(),
              "resource workspace Sound compact minimum: wide-pane Undo restores exact raw rows and the prior clean history state");
    }
    row = soundAlternativeByWav(tree, QStringLiteral("none"));
    check(row, "resource workspace Sound compact minimum: original row is restored after the wide-pane Undo");
    if ( !row )
    {
        return;
    }
    tree->scrollToItem(row, QAbstractItemView::PositionAtTop);
    const bool originalRowVisible = waitUntil([&]()
    {
        tree->viewport()->grab();
        const QRect originalRowRect = tree->visualItemRect(row);
        return !originalRowRect.isEmpty() && tree->viewport()->rect().contains(originalRowRect) &&
               tree->itemAt(originalRowRect.center()) == row;
    });
    check(originalRowVisible,
          "resource workspace Sound compact minimum: the original row is visibly restored before later minimum and Play/Loop checks");
    volumeIndex = tree->indexFromItem(row, 5);
    for ( const int column : {0, 2, 3, 4} )
    {
        tree->setColumnWidth(column, defaults[static_cast<std::size_t>(column)]);
    }
    tree->setColumnWidth(1, wideSoundWidth);
    QApplication::processEvents(QEventLoop::AllEvents);

    const int wideChromeWidth = pane->width() - tree->viewport()->width();
    const int minimumPaneWidth = pane->minimumWidth();
    check(minimumPaneWidth == minimumColumns + wideChromeWidth && minimumPaneWidth < 800,
          "resource workspace Sound compact minimum: whole-pane minimum includes every compact column and actual frame scrollbar and pane chrome");
    pane->resize(minimumPaneWidth, 500);
    const bool settledMinimumPane = waitUntil([&]()
    {
        tree->viewport()->grab();
        return pane->width() == minimumPaneWidth && tree->viewport()->width() == minimumColumns;
    });
    check(settledMinimumPane,
          "resource workspace Sound compact minimum: resizing to the stable pane minimum leaves exactly the six-column minimum viewport");
    for ( const int column : {0, 2, 3, 4, 5} )
    {
        tree->setColumnWidth(column, 1);
        check(tree->columnWidth(column) == minima[static_cast<std::size_t>(column)],
              "resource workspace Sound compact minimum: each column clamps to its fixed independent minimum and radii stay smaller than Volume");
    }
    // Sound is resized after every other section. This catches the previous
    // header-resize path that changed Sound while the user was sizing another
    // field and let the fixed Remove control drift over the last row.
    tree->setColumnWidth(1, 1);
    bool exactMinimums = true;
    for ( int column = 0; column < static_cast<int>(minima.size()); ++column )
    {
        exactMinimums = exactMinimums && tree->columnWidth(column) == minima[static_cast<std::size_t>(column)];
    }
    check(exactMinimums,
          "resource workspace Sound compact minimum: shrinking Sound last clamps all six real sections to 80/160/100/56/56/230");
    const QRect playCell = tree->visualRect(tree->indexFromItem(row, 0));
    const QPoint playHover = playCell.center();
    QMouseEvent playMove(QEvent::MouseMove, QPointF(playHover), QPointF(tree->viewport()->mapToGlobal(playHover)),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &playMove);
    QToolButton* const play = tree->findChild<QToolButton*>(QStringLiteral("snd.play"));
    QToolButton* const loop = tree->findChild<QToolButton*>(QStringLiteral("snd.loop"));
    const QRect playRect = play ? QRect(play->mapTo(tree->viewport(), QPoint{}), play->size()) : QRect{};
    const QRect loopRect = loop ? QRect(loop->mapTo(tree->viewport(), QPoint{}), loop->size()) : QRect{};
    check(play && loop && play->isVisible() && loop->isVisible() && play->size() == QSize(22, 22) &&
              loop->size() == QSize(22, 22) && playCell.contains(playRect) && playCell.contains(loopRect),
          "resource workspace Sound compact minimum: 80-pixel Play column contains the indented 22-pixel Play and Loop buttons");
    QApplication::processEvents(QEventLoop::AllEvents);
    tree->viewport()->grab();
    const int chromeWidth = pane->width() - tree->viewport()->width();
    check(minimumPaneWidth == minimumColumns + chromeWidth,
          "resource workspace Sound compact minimum: minimum width continues to include the live vertical scrollbar and pane frame after clamping");
    for ( const int requestedWidth : {minimumPaneWidth + 1, minimumPaneWidth, minimumPaneWidth - 1} )
    {
        pane->resize(requestedWidth, 500);
        const bool settled = waitUntil([&]()
        {
            tree->viewport()->grab();
            return pane->width() == std::max(requestedWidth, minimumPaneWidth) &&
                   tree->horizontalScrollBar()->value() == tree->horizontalScrollBar()->minimum();
        });
        const QRect volume = tree->visualRect(volumeIndex);
        const bool visibleVolume = tree->viewport()->rect().contains(volume);
        const QPoint hover(volume.center());
        QMouseEvent move(QEvent::MouseMove, QPointF(hover), QPointF(tree->viewport()->mapToGlobal(hover)),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &move);
        QToolButton* const remove = tree->findChild<QToolButton*>(QStringLiteral("snd.remove"));
        const bool responsiveSound = tree->columnWidth(1) >= 160 && tree->columnWidth(1) < wideSoundWidth;
        check(settled && visibleVolume && responsiveSound && remove && remove->isVisible() &&
                  tree->viewport()->rect().contains(remove->geometry()) && volume.contains(remove->geometry()),
              "resource workspace Sound compact minimum: minimum plus or minus one keeps the whole Volume and reachable Remove visible while Sound contracts");
        tree->setCurrentItem(row, 5);
        tree->editItem(row, 5);
        QWidget* const editor = tree->indexWidget(volumeIndex);
        QSlider* const slider = editor ? editor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) : nullptr;
        QLineEdit* const value = editor ? editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) : nullptr;
        const bool controls = editor && slider && value && remove;
        check(controls, "resource workspace Sound compact minimum: the real value slider and Remove controls remain available");
        if ( controls )
        {
            const QRect editorRect(editor->mapTo(tree->viewport(), QPoint{}), editor->size());
            const QRect sliderRect(slider->mapTo(tree->viewport(), QPoint{}), slider->size());
            const QRect valueRect(value->mapTo(tree->viewport(), QPoint{}), value->size());
            check(volume.contains(editorRect) && editorRect.contains(sliderRect) && editorRect.contains(valueRect) &&
                      sliderRect.width() >= 12 && valueRect.width() >= QFontMetrics(value->font()).horizontalAdvance(QStringLiteral("0.375")) &&
                      remove->isVisible() && !remove->geometry().intersects(editorRect) &&
                      !remove->geometry().intersects(sliderRect) && !remove->geometry().intersects(valueRect) &&
                      value->text() == QStringLiteral("0.375"),
                  "resource workspace Sound compact minimum: focused controls retain full ordinary raw value margins and visible nonoverlapping Remove at whole-pane minimum");
        }
        tree->setCurrentItem(row->parent(), 0);
        tree->setFocus();
        QApplication::processEvents(QEventLoop::AllEvents);
    }
    check(ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")).toArray() == rows &&
              !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "resource workspace Sound compact minimum: column resize whole-pane clamp focus and hover preserve exact clean raw rows");

    // Exercise the shared Remove control after horizontal scrolling has moved
    // the actual last Volume cell back under its viewport-anchored button.
    tree->setColumnWidth(2, minima[2] + 20);
    tree->setColumnWidth(1, defaults[1]);
    tree->setColumnWidth(1, minima[1]);
    QScrollBar* const horizontal = tree->horizontalScrollBar();
    const bool horizontalRange = waitUntil([&]()
    {
        tree->viewport()->grab();
        return horizontal->isVisible() && horizontal->maximum() >= 20;
    });
    check(horizontalRange,
          "resource workspace Sound compact minimum: widening Flags keeps Sound at its minimum and exposes native horizontal scrolling");
    if ( !horizontalRange )
    {
        return;
    }
    horizontal->setValue(horizontal->maximum());
    const bool scrolledToVolume = waitUntil([&]()
    {
        tree->viewport()->grab();
        return tree->header()->offset() == horizontal->maximum();
    });
    const QRect volumeCell = tree->visualRect(volumeIndex);
    const QRect visibleVolume = volumeCell.intersected(tree->viewport()->rect());
    check(scrolledToVolume && visibleVolume.width() == tree->columnWidth(5) &&
              visibleVolume.right() == tree->viewport()->rect().right(),
          "resource workspace Sound compact minimum: horizontal scrolling presents the complete real Volume row at the viewport edge");

    const QCborMap beforeRemove = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap beforeHistory = beforeRemove.value(QStringLiteral("history")).toMap();
    const QCborArray beforeRows = beforeRemove.value(QStringLiteral("rows")).toArray();
    const QPoint rowHover(tree->viewport()->width() - 40, tree->visualItemRect(row).center().y());
    QMouseEvent move(QEvent::MouseMove, QPointF(rowHover), QPointF(tree->viewport()->mapToGlobal(rowHover)),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &move);
    QToolButton* const remove = tree->findChild<QToolButton*>(QStringLiteral("snd.remove"));
    const QRect removeRect = remove ? QRect(remove->mapTo(tree->viewport(), QPoint{}), remove->size()) : QRect{};
    check(remove && remove->isVisible() && visibleVolume.contains(removeRect),
          "resource workspace Sound compact minimum: the real Remove button stays inside the visible Volume cell after horizontal scrolling");
    if ( !remove || !remove->isVisible() || !visibleVolume.contains(removeRect) )
    {
        return;
    }
    const QPoint removePoint = remove->rect().center();
    const QPoint removeGlobal = remove->mapToGlobal(removePoint);
    QMouseEvent removePress(QEvent::MouseButtonPress, QPointF(removePoint), QPointF(removeGlobal), Qt::LeftButton,
                           Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(remove, &removePress);
    QMouseEvent removeRelease(QEvent::MouseButtonRelease, QPointF(removePoint), QPointF(removeGlobal), Qt::LeftButton,
                              Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(remove, &removeRelease);

    QCborArray rowsAfterRemove;
    for ( qsizetype index = 1; index < beforeRows.size(); ++index )
    {
        rowsAfterRemove.append(beforeRows.at(index));
    }
    const QCborMap afterRemove = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap removedHistory = afterRemove.value(QStringLiteral("history")).toMap();
    const QCborArray removedCommands = removedHistory.value(QStringLiteral("commands")).toArray();
    const QCborMap removeCommand = removedCommands.size() == 1 ? removedCommands.at(0).toMap() : QCborMap{};
    const QCborMap removedEntry = removeCommand.value(QStringLiteral("before")).toMap();
    const bool removedExactly =
        afterRemove.value(QStringLiteral("rows")).toArray() == rowsAfterRemove &&
        afterRemove.value(QStringLiteral("modified")).toBool() &&
        removedHistory.value(QStringLiteral("cursor")).toInteger() == 1 && removedCommands.size() == 1 &&
        removeCommand.value(QStringLiteral("row")).toInteger() == 0 &&
        removedEntry.value(QStringLiteral("raw")).toArray() == beforeRows.at(0).toArray() &&
        removeCommand.value(QStringLiteral("after")).isNull();
    check(removedExactly,
          "resource workspace Sound compact minimum: an actual Remove click deletes only the hovered raw row and records its exact undo command");

    const bool undone = ui::resources::resourceAuthoringPaneUndoRedo(pane.get(), false);
    const QCborMap afterUndo = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap undoHistory = afterUndo.value(QStringLiteral("history")).toMap();
    check(undone && afterUndo.value(QStringLiteral("rows")).toArray() == beforeRows &&
              !afterUndo.value(QStringLiteral("modified")).toBool() &&
              undoHistory.value(QStringLiteral("commands")).toArray() == removedCommands &&
              undoHistory.value(QStringLiteral("cursor")).toInteger() ==
                  beforeHistory.value(QStringLiteral("cursor")).toInteger() &&
              undoHistory.value(QStringLiteral("clean")).toInteger() == beforeHistory.value(QStringLiteral("clean")).toInteger(),
          "resource workspace Sound compact minimum: Undo restores every exact raw field and returns the original clean history cursor");
}

void checkSoundActualHeaderAndActiveVolumePainting()
{
    QCborArray rows;
    rows.append(soundFollowupRow(QStringLiteral("lwalkhard"), QStringLiteral("none")));
    rows.append(soundFollowupRow(QStringLiteral("lwalkwoodecho"), QStringLiteral("second.wav")));
    auto pane = soundFollowupPane(rows);
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    check(tree, "resource workspace SND actual painting: tree exists");
    if ( !tree )
    {
        return;
    }
    tree->setAnimated(false);
    QTreeWidgetItem* const mode = soundModeByBytes(tree, QByteArrayLiteral("lwalkhard"));
    check(mode, "resource workspace SND actual painting: independent lwalkhard slot exists");
    if ( !mode )
    {
        return;
    }
    const QRect title = tree->visualItemRect(mode);
    const QImage titles = tree->viewport()->grab().toImage();
    // Item geometry is logical, while grab() retains the display's pixel ratio.
    const int scanY = static_cast<int>(std::lround((title.center().y() - 9) * titles.devicePixelRatio()));
    int badgeRuns = 0;
    bool inBadge = false;
    for ( int x = 0; x < titles.width() && scanY >= 0 && scanY < titles.height(); ++x )
    {
        const bool badge = titles.pixelColor(x, scanY) == QColor(58, 66, 75);
        if ( badge && !inBadge )
        {
            ++badgeRuns;
        }
        inBadge = badge;
    }
    const bool exactTitle = mode->text(0) == QStringLiteral("lwalkhard");
    const bool exactSlot = mode->data(0, Qt::UserRole + 23).toInt() == 6;
    const bool fullWidth = title.right() >= tree->viewport()->width() - 2;
    if ( !exactTitle || !exactSlot || !fullWidth || badgeRuns != 1 )
    {
        std::cerr << "SND actual mode paint diagnostic: exactTitle=" << exactTitle << " exactSlot=" << exactSlot
                  << " fullWidth=" << fullWidth << " badgeRuns=" << badgeRuns
                  << " spanned=" << mode->isFirstColumnSpanned() << " titleRight=" << title.right()
                  << " viewportWidth=" << tree->viewport()->width() << " scanY=" << scanY
                  << " imageWidth=" << titles.width() << " imageHeight=" << titles.height()
                  << " dpr=" << titles.devicePixelRatio() << " titleTop=" << title.top()
                  << " titleHeight=" << title.height() << '\n';
        titles.save(QDir(QDir::tempPath()).filePath(QStringLiteral("resource-snd-header-paint-diagnostic.png")));
    }
    check(exactTitle && exactSlot && fullWidth && badgeRuns == 1,
          "resource workspace SND actual painting: a full-width exact mode caption paints one slot badge across six "
          "columns");
    check(mode->toolTip(0) == QStringLiteral("Sound mode: 6"),
          "resource workspace SND actual painting: the mode tooltip contains only the exact sound mode number");
    mode->setExpanded(true);
    QApplication::processEvents(QEventLoop::AllEvents);
    QTreeWidgetItem* const row = soundAlternativeByWav(tree, QStringLiteral("none"));
    check(row, "resource workspace SND actual painting: volume alternative exists");
    if ( !row )
    {
        return;
    }
    tree->setCurrentItem(row, 5);
    tree->editItem(row, 5);
    QApplication::processEvents(QEventLoop::AllEvents);
    QWidget* const editor = tree->indexWidget(tree->indexFromItem(row, 5));
    QSlider* const slider = editor ? editor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) : nullptr;
    QLineEdit* const value = editor ? editor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) : nullptr;
    check(editor && slider && value && editor->isVisible() && slider->isVisible(),
          "resource workspace SND actual painting: the real compound volume editor is visible");
    if ( !editor || !slider || !value )
    {
        return;
    }
    QImage underlay(220, 26, QImage::Format_ARGB32);
    QImage blank(220, 26, QImage::Format_ARGB32);
    underlay.fill(Qt::transparent);
    blank.fill(Qt::transparent);
    QStyleOptionViewItem option;
    option.rect = underlay.rect();
    option.widget = tree;
    option.palette = tree->palette();
    option.state = QStyle::State_Enabled;
    {
        QPainter painter(&underlay);
        tree->itemDelegate()->paint(&painter, option, tree->indexFromItem(row, 5));
    }
    {
        QPainter painter(&blank);
        tree->itemDelegate()->paint(&painter, option, tree->indexFromItem(row, 0));
    }
    bool oneSlider = true;
    for ( int y = 3; y < 22; ++y )
    {
        for ( int x = 3; x < 130; ++x )
        {
            oneSlider = oneSlider && underlay.pixelColor(x, y) == blank.pixelColor(x, y);
        }
    }
    const QImage actual = tree->viewport()->grab().toImage();
    const QRect cell = tree->visualRect(tree->indexFromItem(row, 5));
    check(oneSlider && !actual.isNull() && cell.contains(editor->geometry()) &&
              value->text() == QStringLiteral("0.375"),
          "resource workspace SND actual painting: active volume paints one slider without an inactive handle or box "
          "beneath it");
    const QRect finalVolume = tree->visualRect(tree->indexFromItem(row, 5));
    check(tree->columnWidth(5) >= 230 && finalVolume.right() == tree->viewport()->rect().right(),
          "resource workspace SND actual painting: final Volume keeps its fixed minimum and fills the wide-pane remainder");
}

void checkSoundBulkIdentityFocusAndHoverGlyphs()
{
    QCborArray rows;
    const auto modes = assets::soundClassModeNames(assets::SoundClassProfile::ijim);
    check(modes.size() >= 105, "resource workspace SND bulk work: IJIM offers at least 105 engine modes");
    if ( modes.size() < 105 )
    {
        return;
    }
    for ( std::size_t index = 0; index < 105; ++index )
    {
        const QString mode = QString::fromLatin1(modes[index].data(), static_cast<int>(modes[index].size()));
        for ( int entry = 0; entry < (index < 28 ? 2 : 1); ++entry )
        {
            rows.append(soundFollowupRow(mode, QStringLiteral("%1_%2.wav").arg(mode).arg(entry)));
        }
    }
    int publications = 0;
    auto pane = soundFollowupPane(rows, testContext(ProjectType::IJIM),
                                  [&publications](QWidget*)
                                  {
                                      ++publications;
                                  });
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    QAbstractButton* const expand = pane ? button(pane.get(), "snd.expand_all") : nullptr;
    QAbstractButton* const collapse = pane ? button(pane.get(), "snd.collapse_all") : nullptr;
    check(tree && expand && collapse && tree->topLevelItemCount() == 105 && rows.size() == 133,
          "resource workspace SND bulk work: actual controls represent the fixed 133-entry and 105-mode fixture");
    if ( !tree || !expand || !collapse )
    {
        return;
    }
    std::vector<QPersistentModelIndex> identities;
    for ( QTreeWidgetItem* const row : soundAlternativeItems(tree) )
    {
        identities.emplace_back(tree->indexFromItem(row, 1));
    }
    int resets = 0;
    const QMetaObject::Connection resetConnection =
        QObject::connect(tree->model(), &QAbstractItemModel::modelReset, pane.get(),
                         [&resets]()
                         {
                             ++resets;
                         });
    publications = 0;
    QElapsedTimer elapsed;
    elapsed.start();
    expand->click();
    QApplication::processEvents(QEventLoop::AllEvents);
    const qint64 expandMicroseconds = elapsed.nsecsElapsed() / 1000;
    const int expandPublications = publications;
    publications = 0;
    elapsed.restart();
    collapse->click();
    QApplication::processEvents(QEventLoop::AllEvents);
    const qint64 collapseMicroseconds = elapsed.nsecsElapsed() / 1000;
    const bool stable = std::all_of(identities.begin(), identities.end(),
                                    [](const QPersistentModelIndex& index)
                                    {
                                        return index.isValid();
                                    });
    check(stable && resets == 0 && expandPublications == 1 && publications == 1 && tree->updatesEnabled() &&
              ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")).toArray() ==
                  rows,
          "resource workspace SND bulk work: expand and collapse publish once, retain all row identities and restore "
          "painting");
    tree->setAnimated(false);
    QTreeWidgetItem* row = soundAlternativeByWav(tree, QStringLiteral("moving_0.wav"));
    check(row, "resource workspace SND hover paint: actual moving alternative exists");
    if ( !row )
    {
        QObject::disconnect(resetConnection);
        return;
    }
    row->parent()->setExpanded(true);
    publications = 0;
    elapsed.restart();
    const bool appended = addSoundAlternative(tree, QStringLiteral("moving"));
    QApplication::processEvents(QEventLoop::AllEvents);
    const qint64 appendMicroseconds = elapsed.nsecsElapsed() / 1000;
    const QCborArray appendedRows =
        ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")).toArray();
    const bool appendIdentity = std::all_of(identities.begin(), identities.end(),
                                            [](const QPersistentModelIndex& index)
                                            {
                                                return index.isValid();
                                            });
    check(appended && appendIdentity && resets == 0 && publications == 1 && appendedRows.size() == 134 &&
              appendedRows.at(133).toArray().at(0).toString() == QStringLiteral("moving") &&
              appendedRows.at(133).toArray().at(1).toString() == QStringLiteral("new_sound.wav") &&
              std::equal(rows.begin(), rows.end(), appendedRows.begin()),
          "resource workspace SND bulk work: Add Entry preserves all 133 original row identities and source bytes with "
          "one publication");
    QObject::disconnect(resetConnection);
    std::cerr << "SND 133-entry/105-mode work: expand_us=" << expandMicroseconds
              << " collapse_us=" << collapseMicroseconds << " append_us=" << appendMicroseconds << '\n';
    row = soundAlternativeByWav(tree, QStringLiteral("moving_0.wav"));
    check(row, "resource workspace SND hover paint: the original moving entry remains available after insertion");
    if ( !row )
    {
        return;
    }
    tree->setCurrentItem(row, 0);
    tree->scrollToItem(row);
    pane->activateWindow();
    tree->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
    const QPoint position = tree->visualItemRect(row).center();
    QMouseEvent move(QEvent::MouseMove, QPointF(position), QPointF(tree->viewport()->mapToGlobal(position)),
                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &move);
    const QImage painted = tree->viewport()->grab().toImage();
    const QRect rowRect = tree->visualItemRect(row);
    bool cyanFocusStripe = false;
    for ( int y = rowRect.top() + 3; y < rowRect.bottom() - 2 && y < painted.height(); ++y )
    {
        for ( int x = 1; x < 12 && x < painted.width(); ++x )
        {
            const QColor pixel = painted.pixelColor(x, y);
            cyanFocusStripe = cyanFocusStripe || (pixel.blue() > pixel.red() + 35 && pixel.green() > pixel.red() + 25);
        }
    }
    check(tree->hasFocus() && tree->currentItem() == row && !cyanFocusStripe,
          "resource workspace SND hover paint: keyboard focus and current-item navigation remain without a cyan left-edge stripe");
    QToolButton* const play = tree->findChild<QToolButton*>(QStringLiteral("snd.play"));
    QToolButton* const loop = tree->findChild<QToolButton*>(QStringLiteral("snd.loop"));
    int greenPixels = 0;
    int bluePixels = 0;
    if ( play )
    {
        const QImage glyph = play->icon().pixmap(play->iconSize()).toImage();
        for ( int y = 0; y < glyph.height(); ++y )
        {
            for ( int x = 0; x < glyph.width(); ++x )
            {
                const QColor pixel = glyph.pixelColor(x, y);
                greenPixels += pixel.alpha() > 200 && pixel.green() > pixel.red() + 15;
            }
        }
    }
    if ( loop )
    {
        const QImage glyph = loop->icon().pixmap(loop->iconSize()).toImage();
        for ( int y = 0; y < glyph.height(); ++y )
        {
            for ( int x = 0; x < glyph.width(); ++x )
            {
                const QColor pixel = glyph.pixelColor(x, y);
                bluePixels +=
                    pixel.alpha() > 200 && pixel.blue() > pixel.red() + 40 && pixel.green() > pixel.red() + 20;
            }
        }
    }
    check(play && loop && play->isVisible() && loop->isVisible() && play->width() == 22 && loop->width() == 22 &&
              play->iconSize() == QSize(13, 13) && loop->iconSize() == QSize(13, 13) && !loop->icon().isNull() &&
              greenPixels >= 50 && bluePixels >= 38,
          "resource workspace SND hover paint: 13-pixel green play and blue loop glyphs retain their reserved 22-pixel "
              "buttons");
}

void checkSoundModePopupPolicyAndLocalHistory()
{
    QCborArray rows;
    rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("none"), QStringLiteral("1")));
    auto pane = soundFollowupPane(rows);
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    QAbstractButton* const addMode = pane ? button(pane.get(), "snd.add_mode") : nullptr;
    check(tree && addMode, "resource workspace SND local history: actual tree and Add Mode exist");
    if ( !tree || !addMode )
    {
        return;
    }
    const QPoint originalCursor = QCursor::pos();
    QScreen* const screen = QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry() : QRect{};
    const QPoint requestedAnchor = available.topLeft() + QPoint(48, 48);
    const auto popupFitsFrom = [&available](const QPoint point)
    {
        return available.isValid() && available.right() - point.x() + 1 >= 340
            && available.bottom() - point.y() + 1 >= 410;
    };
    const bool availableRoom = popupFitsFrom(requestedAnchor);
    check(availableRoom,
          "resource workspace SND mode popup: primary available geometry can contain the complete mode dialog");
    if ( !availableRoom )
    {
        return;
    }
    QCursor::setPos(requestedAnchor);
    const QPoint anchor = QCursor::pos();
    const bool cursorAnchorSafe = available.contains(anchor) && popupFitsFrom(anchor);
    check(cursorAnchorSafe,
          "resource workspace SND mode popup: actual cursor remains at a point with full dialog clearance");
    if ( !cursorAnchorSafe )
    {
        QCursor::setPos(originalCursor);
        return;
    }
    bool popupNearCursor = false;
    bool popupWithinAvailable = false;
    bool popupNoMinimize = false;
    bool popupNoMaximize = false;
    bool chosen = false;
    QRect popupFrameForDiagnostic{};
    Qt::WindowFlags popupFlagsForDiagnostic{};
    runWithModalDriver(
        [addMode]()
        {
            addMode->click();
        },
        std::nullopt, std::nullopt, false, {},
        [&](QWidget* active)
        {
            QDialog* const popup = qobject_cast<QDialog*>(active);
            QListWidget* const list = popup ? popup->findChild<QListWidget*>(QStringLiteral("snd.mode.list")) : nullptr;
            if ( !popup || !list )
            {
                return;
            }
            const QRect popupFrame = popup->frameGeometry();
            popupFrameForDiagnostic = popupFrame;
            popupFlagsForDiagnostic = popup->windowFlags();
            const QPoint origin = popupFrame.topLeft();
            popupNearCursor = std::abs(origin.x() - anchor.x()) <= 32
                && std::abs(origin.y() - anchor.y()) <= 32;
            popupWithinAvailable = available.contains(popupFrame.topLeft())
                && available.contains(popupFrame.bottomRight());
            popupNoMinimize = !popup->windowFlags().testFlag(Qt::WindowMinimizeButtonHint);
            popupNoMaximize = !popup->windowFlags().testFlag(Qt::WindowMaximizeButtonHint);
            const auto found = list->findItems(QStringLiteral("activate"), Qt::MatchExactly);
            chosen = found.size() == 1;
            if ( chosen )
            {
                list->setCurrentItem(found.front());
                popup->accept();
            }
            else
            {
                popup->reject();
            }
        });
    QCursor::setPos(originalCursor);
    if ( !(chosen && popupNearCursor && popupWithinAvailable && popupNoMinimize && popupNoMaximize) )
    {
        std::cerr << "SND Add Mode popup diagnostic: chosen=" << chosen
                  << " nearCursor=" << popupNearCursor << " withinAvailable=" << popupWithinAvailable
                  << " noMinimize=" << popupNoMinimize << " noMaximize=" << popupNoMaximize
                  << " anchor=" << anchor.x() << ',' << anchor.y()
                  << " frame=" << popupFrameForDiagnostic.x() << ',' << popupFrameForDiagnostic.y() << ','
                  << popupFrameForDiagnostic.width() << ',' << popupFrameForDiagnostic.height()
                  << " flags=" << static_cast<int>(popupFlagsForDiagnostic)
                  << " available=" << available.x() << ',' << available.y() << ','
                  << available.width() << ',' << available.height() << '\n';
    }
    check(chosen && popupNearCursor,
          "resource workspace SND mode popup: Add Mode opens within 32 px of the actual cursor");
    check(chosen && popupWithinAvailable,
          "resource workspace SND mode popup: complete Add Mode geometry stays within available screen bounds");
    check(chosen && popupNoMinimize,
          "resource workspace SND mode popup: Add Mode omits its minimize control");
    check(chosen && popupNoMaximize,
          "resource workspace SND mode popup: Add Mode omits its maximize control");
    check(chosen && addSoundAlternative(tree, QStringLiteral("activate")),
          "resource workspace SND local history: presentation mode then a real entry are added");
    QTreeWidgetItem* const added = soundAlternativeByWav(tree, QStringLiteral("new_sound.wav"));
    check(added, "resource workspace SND local history: newly authored entry exists");
    if ( !added )
    {
        return;
    }
    added->setText(5, QStringLiteral("0.375"));
    const QCborMap authored = ui::resources::captureResourceAuthoringPaneState(pane.get());
    QAction* const undo = action(pane.get(), "snd.undo");
    QAction* const redo = action(pane.get(), "snd.redo");
    check(undo && redo && undo->text() == QStringLiteral("Undo") && redo->text() == QStringLiteral("Redo") &&
              undo->isEnabled() && !redo->isEnabled() &&
              undo->toolTip() == QStringLiteral("Undo sound entry, value or mode change (Ctrl Z)") &&
              redo->toolTip() == QStringLiteral("Redo sound entry, value or mode change (Ctrl Shift Z)"),
          "resource workspace SND local history: actions expose exact local Undo and Redo shortcut guidance");
    bool historyMenu = false;
    QTimer menuDriver;
    menuDriver.setInterval(5);
    QObject::connect(&menuDriver, &QTimer::timeout, pane.get(),
                     [&]()
                     {
                         QMenu* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                         if ( menu )
                         {
                             historyMenu =
                                 undo && redo && menu->actions().contains(undo) && menu->actions().contains(redo);
                             menuDriver.stop();
                             menu->close();
                         }
                     });
    menuDriver.start();
    const QPoint body = tree->visualItemRect(added).center();
    QContextMenuEvent contextEvent(QContextMenuEvent::Mouse, body, tree->viewport()->mapToGlobal(body));
    QApplication::sendEvent(tree->viewport(), &contextEvent);
    (void)waitUntil(
        [&]()
        {
            return historyMenu;
        });
    menuDriver.stop();
    check(historyMenu, "resource workspace SND local history: the entry body context menu exposes Undo and Redo "
                       "without extra group toolbar buttons");
    tree->setCurrentItem(added, 5);
    tree->editItem(added, 5);
    QApplication::processEvents(QEventLoop::AllEvents);
    QWidget* const valueEditor = tree->indexWidget(tree->indexFromItem(added, 5));
    QLineEdit* const valueText =
        valueEditor ? valueEditor->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")) : nullptr;
    check(valueText && valueText->text() == QStringLiteral("0.375"),
          "resource workspace SND local history: the real volume line opens the authored value");
    bool valueEditorFocused = false;
    bool textUndoRestored = false;
    bool rowsStayedAuthored = false;
    bool paneUndoStayedBlocked = false;
    if ( valueText )
    {
        QPointer<QLineEdit> guardedValueText(valueText);
        {
            native_caption_test::ScopedTestFocusRestore restoreFocus{};
            valueEditorFocused = native_caption_test::focusTestWidget(valueText, valueText, 2000);
            check(valueEditorFocused,
                  "resource workspace SND local history: actual volume QLineEdit owns Qt and native focus");
            if ( valueEditorFocused && guardedValueText )
            {
                typeIntoLineEdit(guardedValueText.data(), QStringLiteral("0.625"));
                QKeyEvent shortcut(QEvent::ShortcutOverride, Qt::Key_Z, Qt::ControlModifier);
                QApplication::sendEvent(guardedValueText.data(), &shortcut);
                QKeyEvent nativeUndo(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
                QApplication::sendEvent(guardedValueText.data(), &nativeUndo);
                textUndoRestored = guardedValueText
                    && guardedValueText->text() == QStringLiteral("0.375");
                const QCborMap afterTextUndo =
                    ui::resources::captureResourceAuthoringPaneState(pane.get());
                rowsStayedAuthored = afterTextUndo.value(QStringLiteral("rows"))
                    == authored.value(QStringLiteral("rows"));
                if ( guardedValueText && QApplication::focusWidget() == guardedValueText )
                {
                    paneUndoStayedBlocked =
                        !ui::resources::resourceAuthoringPaneUndoRedo(pane.get(), false);
                }
            }
        }
    }
    check(textUndoRestored,
          "resource workspace SND local history: Ctrl+Z in the actual line restores its authored text");
    check(rowsStayedAuthored,
          "resource workspace SND local history: line-edit Ctrl+Z leaves the pane history rows unchanged");
    check(paneUndoStayedBlocked,
          "resource workspace SND local history: focused line-edit ownership blocks the pane Undo command");
    native_caption_test::ScopedTestFocusRestore treeFocusRestore{};
    const bool treeFocused = native_caption_test::focusTestWidget(tree, tree, 2000);
    check(treeFocused,
          "resource workspace SND local history: actual tree body regains exact focus before structural Undo");
    QKeyEvent undoBody(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
    const bool undoValue = treeFocused && QApplication::sendEvent(tree, &undoBody);
    const QCborMap valueUndone = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const bool undoEntry = treeFocused
        && ui::resources::resourceAuthoringPaneUndoRedo(pane.get(), false);
    const QCborMap entryUndone = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const bool undoMode = treeFocused
        && ui::resources::resourceAuthoringPaneUndoRedo(pane.get(), false);
    const QCborMap modeUndone = ui::resources::captureResourceAuthoringPaneState(pane.get());
    check(undoValue && undoEntry && undoMode && valueUndone.value(QStringLiteral("rows")).toArray().size() == 2 &&
              valueUndone.value(QStringLiteral("rows")).toArray().at(1).toArray().at(5).toString() ==
                  QStringLiteral("1") &&
              entryUndone.value(QStringLiteral("rows")).toArray() == rows &&
              entryUndone.value(QStringLiteral("soundEmptyModes")).toArray() ==
                  QCborArray{QStringLiteral("activate")} &&
              modeUndone.value(QStringLiteral("rows")).toArray() == rows &&
              modeUndone.value(QStringLiteral("soundEmptyModes")).toArray().isEmpty() &&
              !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "resource workspace SND local history: Undo value, entry and empty mode returns exact clean authored and "
          "presentation baselines");
    const bool redoMode = treeFocused
        && ui::resources::resourceAuthoringPaneUndoRedo(pane.get(), true);
    const QCborMap modeRedone = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const bool redoEntry = treeFocused
        && ui::resources::resourceAuthoringPaneUndoRedo(pane.get(), true);
    const bool treeFocusedForRedo = treeFocused
        && native_caption_test::focusTestWidget(tree, tree, 2000);
    QKeyEvent redoBody(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
    const bool redoValue = treeFocusedForRedo && QApplication::sendEvent(tree, &redoBody);
    const QCborMap redone = ui::resources::captureResourceAuthoringPaneState(pane.get());
    check(redoMode && redoEntry && redoValue && modeRedone.value(QStringLiteral("rows")).toArray() == rows &&
              modeRedone.value(QStringLiteral("soundEmptyModes")).toArray() == QCborArray{QStringLiteral("activate")} &&
              redone.value(QStringLiteral("rows")) == authored.value(QStringLiteral("rows")) &&
              ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "resource workspace SND local history: Redo mode, entry and value preserves exact source order, flags and "
          "raw values");
}

class SoundPlaybackRequestScope
{
  public:
    using Player = ui::audio::AppSoundPlayer;
    using Request = Player::RequestForTest;
    using Operation = Player::RequestOperationForTest;

    SoundPlaybackRequestScope()
    {
        Player::instance().setRequestObserverForTest({});
        Player::instance().setDeviceFreePlaybackForTest(true);
        Player::instance().setRequestObserverForTest(
            [this](const Request& request)
            {
                requests.push_back(request);
            });
    }
    ~SoundPlaybackRequestScope()
    {
        Player::instance().setRequestObserverForTest({});
        Player::instance().setDeviceFreePlaybackForTest(false);
    }
    std::size_t playCount() const
    {
        return static_cast<std::size_t>(std::count_if(requests.begin(), requests.end(),
                                                      [](const Request& request)
                                                      {
                                                          return request.operation == Operation::playResource ||
                                                                 request.operation == Operation::playBytes ||
                                                                 request.operation == Operation::replay;
                                                      }));
    }
    std::vector<Request> requests;
};

std::string soundFollowupPcmWave()
{
    std::string bytes;
    const auto word = [&bytes](const std::uint16_t value)
    {
        bytes.push_back(static_cast<char>(value & 0xffU));
        bytes.push_back(static_cast<char>((value >> 8U) & 0xffU));
    };
    const auto dword = [&bytes](const std::uint32_t value)
    {
        for ( unsigned shift = 0; shift < 32; shift += 8 )
        {
            bytes.push_back(static_cast<char>((value >> shift) & 0xffU));
        }
    };
    bytes += "RIFF";
    dword(44U);
    bytes += "WAVEfmt ";
    dword(16U);
    word(1U);
    word(1U);
    dword(8000U);
    dword(8000U);
    word(1U);
    word(8U);
    bytes += "data";
    dword(8U);
    bytes.append(8U, static_cast<char>(128));
    return bytes;
}

void checkSoundPickerPlaybackRequiresUserIntent()
{
    SoundPlaybackRequestScope audio;
    QTemporaryDir directory;
    check(directory.isValid(), "resource workspace WAV intent: temporary resources exist");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    std::filesystem::create_directories(root / "sound");
    check(writeBytes(root / "sound" / "first.wav", soundFollowupPcmWave()) &&
              writeBytes(root / "sound" / "second.wav", soundFollowupPcmWave()) &&
              writeBytes(root / "ijimwavs.jlh",
                         "SUB Sounds\n  LIST first First set\n  LIST second Second set\nSUBEND\n") &&
              writeBytes(root / "ijimwavs.jll", "[first]\nfirst.wav\n[second]\nsecond.wav\n"),
          "resource workspace WAV intent: valid PCM and independent categories write");
    auto resources = std::make_shared<assets::ResourceLocator>();
    resources->addRoot(root);
    ui::ResourcePickerOptions options;
    options.kind = ui::ResourcePickerKind::wav;
    options.initialName = "first.wav";
    options.projectDirectory = root;
    options.preferredDiskRoot = root;
    options.previewProjectType = ProjectType::IJIM;
    bool inspected = false;
    QTimer inspect;
    inspect.setInterval(10);
    QElapsedTimer timeout;
    timeout.start();
    QObject::connect(
        &inspect, &QTimer::timeout, QApplication::instance(),
        [&]()
        {
            QDialog* const picker = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QListWidget* const names = picker ? picker->findChild<QListWidget*>() : nullptr;
            QTreeWidget* const categories = picker ? picker->findChild<QTreeWidget*>() : nullptr;
            if ( !picker || !names || !categories || !names->isEnabled() || names->count() == 0 )
            {
                if ( timeout.elapsed() > 6000 && picker )
                {
                    check(false, "resource workspace WAV intent: picker population completes");
                    picker->reject();
                }
                return;
            }
            inspected = true;
            inspect.stop();
            check(audio.playCount() == 0,
                  "resource workspace WAV intent: opening and initial selection make zero playback requests");
            QToolButton* const play = picker->findChild<QToolButton*>(QStringLiteral("resourcePickerReplayButton"));
            QSlider* const volume = picker->findChild<QSlider*>(QStringLiteral("resourcePickerVolumeSlider"));
            check(play && volume && play->isEnabled(),
                  "resource workspace WAV intent: a valid silent initial selection still enables explicit Play");
            const int originalVolume = volume ? volume->value() : 0;
            if ( volume )
            {
                volume->setValue(37);
            }
            const std::size_t beforePlay = audio.playCount();
            if ( play )
            {
                play->click();
            }
            check(audio.playCount() == beforePlay + 1 &&
                      ui::audio::AppSoundPlayer::instance().isOwnedBy(ui::audio::SoundPlaybackOwner::resourcePicker) &&
                      ui::audio::AppSoundPlayer::instance().playbackVolumeForTest() == 0.37,
                  "resource workspace WAV intent: explicit Play requests the selected sound once at picker volume");
            QTreeWidgetItem* second = nullptr;
            QTreeWidgetItem* all = nullptr;
            const auto locate = [&](const auto& self, QTreeWidgetItem* item) -> void
            {
                if ( item->text(0) == QStringLiteral("Second set") )
                {
                    second = item;
                }
                if ( item->text(0) == QStringLiteral("All") )
                {
                    all = item;
                }
                for ( int index = 0; index < item->childCount(); ++index )
                {
                    self(self, item->child(index));
                }
            };
            for ( int index = 0; index < categories->topLevelItemCount(); ++index )
            {
                locate(locate, categories->topLevelItem(index));
            }
            check(second && all, "resource workspace WAV intent: exact All and Second set categories exist");
            const std::size_t beforeCategory = audio.playCount();
            if ( second && all )
            {
                categories->setCurrentItem(second);
                categories->setCurrentItem(all);
            }
            check(second && all && audio.playCount() == beforeCategory,
                  "resource workspace WAV intent: category changes and automatic reselection remain silent");
            const auto first = names->findItems(QStringLiteral("first.wav"), Qt::MatchExactly);
            const std::size_t beforeUser = audio.playCount();
            if ( first.size() == 1 )
            {
                const QPoint point = names->visualItemRect(first.front()).center();
                QMouseEvent press(QEvent::MouseButtonPress, QPointF(point),
                                  QPointF(names->viewport()->mapToGlobal(point)), Qt::LeftButton, Qt::LeftButton,
                                  Qt::NoModifier);
                QApplication::sendEvent(names->viewport(), &press);
                QMouseEvent release(QEvent::MouseButtonRelease, QPointF(point),
                                    QPointF(names->viewport()->mapToGlobal(point)), Qt::LeftButton, Qt::NoButton,
                                    Qt::NoModifier);
                QApplication::sendEvent(names->viewport(), &release);
            }
            check(first.size() == 1 && audio.playCount() == beforeUser + 1,
                  "resource workspace WAV intent: a real user selection change requests playback exactly once");
            if ( volume )
            {
                volume->setValue(originalVolume);
            }
            picker->reject();
        });
    inspect.start();
    (void)ui::pickResource(nullptr, resources, options);
    inspect.stop();
    check(inspected, "resource workspace WAV intent: actual modal picker was exercised");
}

void checkSoundLivePreviewVolumeLease()
{
    SoundPlaybackRequestScope audio;
    QTemporaryDir directory;
    check(directory.isValid(), "resource workspace SND live volume: temporary resources exist");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    std::filesystem::create_directories(root / "sound");
    check(writeBytes(root / "sound" / "loop.wav", soundFollowupPcmWave()),
          "resource workspace SND live volume: valid PCM writes");
    auto resources = std::make_shared<assets::ResourceLocator>();
    resources->addRoot(root);
    auto context = testContext(ProjectType::IJIM);
    context.resources = resources;
    QCborArray rows;
    rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("loop.wav"), QStringLiteral("0.25")));
    auto first = soundFollowupPane(rows, context);
    QTreeWidget* const tree = first ? first->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    QTreeWidgetItem* const row = tree ? soundAlternativeByWav(tree, QStringLiteral("loop.wav")) : nullptr;
    check(row && setSoundPreviewLoop(tree, row, true), "resource workspace SND live volume: row loop is enabled");
    QToolButton* const play = tree ? tree->findChild<QToolButton*>(QStringLiteral("snd.play")) : nullptr;
    if ( !row || !play )
    {
        check(false, "resource workspace SND live volume: real hover Play exists");
        return;
    }
    play->click();
    const auto started =
        std::find_if(audio.requests.begin(), audio.requests.end(),
                     [](const auto& request)
                     {
                         return request.operation == SoundPlaybackRequestScope::Operation::playResource;
                     });
    const std::uint64_t lease = started == audio.requests.end() ? 0U : started->lease;
    check(lease != 0U && started->loop && started->volume == 0.25 &&
              ui::audio::AppSoundPlayer::instance().isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor, lease),
          "resource workspace SND live volume: the real row starts its own lease at authored loop volume");
    tree->setCurrentItem(row, 5);
    tree->editItem(row, 5);
    QApplication::processEvents(QEventLoop::AllEvents);
    QWidget* const editor = tree->indexWidget(tree->indexFromItem(row, 5));
    QSlider* const slider = editor ? editor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) : nullptr;
    check(slider, "resource workspace SND live volume: real volume editor exists");
    if ( !slider )
    {
        return;
    }
    const std::size_t beforeDrag = audio.playCount();
    const std::size_t beforeVolumeRequests = audio.requests.size();
    slider->setSliderDown(true);
    slider->setValue(750);
    const QCborMap held = ui::resources::captureResourceAuthoringPaneState(first.get());
    const bool exactVolumeRequest =
        std::any_of(audio.requests.begin() + beforeVolumeRequests, audio.requests.end(),
                    [lease](const auto& request)
                    {
                        return request.operation == SoundPlaybackRequestScope::Operation::volume &&
                               request.owner == ui::audio::SoundPlaybackOwner::resourceEditor &&
                               request.lease == lease && request.volume == 0.75;
                    });
    check(held.value(QStringLiteral("rows")).toArray().at(0).toArray().at(5).toString() == QStringLiteral("0.75") &&
              exactVolumeRequest && ui::audio::AppSoundPlayer::instance().playbackVolumeForTest() == 0.75 &&
              ui::audio::AppSoundPlayer::instance().isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor, lease) &&
              audio.playCount() == beforeDrag,
          "resource workspace SND live volume: held row drag changes the active loop volume without restarting or "
          "using global preview volume");
    slider->setSliderDown(false);
    tree->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
    const std::size_t beforeHistory = audio.playCount();
    const bool undone = ui::resources::resourceAuthoringPaneUndoRedo(first.get(), false);
    const QCborArray undoneRows =
        ui::resources::captureResourceAuthoringPaneState(first.get()).value(QStringLiteral("rows")).toArray();
    check(undone && undoneRows.at(0).toArray().at(5).toString() == QStringLiteral("0.25") &&
              ui::audio::AppSoundPlayer::instance().playbackVolumeForTest() == 0.25 &&
              ui::audio::AppSoundPlayer::instance().isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor, lease) &&
              audio.playCount() == beforeHistory,
          "resource workspace SND live volume: Undo restores the active loop volume on the same lease without playback "
          "requests");
    const bool redone = ui::resources::resourceAuthoringPaneUndoRedo(first.get(), true);
    const QCborArray redoneRows =
        ui::resources::captureResourceAuthoringPaneState(first.get()).value(QStringLiteral("rows")).toArray();
    check(redone && redoneRows.at(0).toArray().at(5).toString() == QStringLiteral("0.75") &&
              ui::audio::AppSoundPlayer::instance().playbackVolumeForTest() == 0.75 &&
              ui::audio::AppSoundPlayer::instance().isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor, lease) &&
              audio.playCount() == beforeHistory,
          "resource workspace SND live volume: Redo restores the active loop volume on the same lease without playback "
          "requests");
    auto second = soundFollowupPane(rows, context);
    QTreeWidget* const otherTree = second ? second->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    QTreeWidgetItem* const otherRow =
        otherTree ? soundAlternativeByWav(otherTree, QStringLiteral("loop.wav")) : nullptr;
    check(otherRow && setSoundPreviewLoop(otherTree, otherRow, true),
          "resource workspace SND live volume: second real row is ready");
    QToolButton* const otherPlay = otherTree ? otherTree->findChild<QToolButton*>(QStringLiteral("snd.play")) : nullptr;
    if ( !otherPlay )
    {
        check(false, "resource workspace SND live volume: second hover Play exists");
        return;
    }
    otherPlay->click();
    const auto latest = std::find_if(audio.requests.rbegin(), audio.requests.rend(),
                                     [](const auto& request)
                                     {
                                         return request.operation == SoundPlaybackRequestScope::Operation::playResource;
                                     });
    const std::uint64_t otherLease = latest == audio.requests.rend() ? 0U : latest->lease;
    QWidget* const oldEditor = tree->indexWidget(tree->indexFromItem(row, 5));
    QSlider* const oldSlider =
        oldEditor ? oldEditor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) : nullptr;
    if ( oldSlider )
    {
        oldSlider->setValue(125);
    }
    else
    {
        row->setText(5, QStringLiteral("0.125"));
    }
    const QCborArray oldRows =
        ui::resources::captureResourceAuthoringPaneState(first.get()).value(QStringLiteral("rows")).toArray();
    const bool onlyEntryOwners =
        std::none_of(audio.requests.begin(), audio.requests.end(),
                     [](const auto& request)
                     {
                         return request.operation == SoundPlaybackRequestScope::Operation::volume &&
                                request.owner != ui::audio::SoundPlaybackOwner::resourceEditor;
                     });
    check(
        otherLease != 0U && otherLease != lease && onlyEntryOwners &&
            oldRows.at(0).toArray().at(5).toString() == QStringLiteral("0.125") &&
            ui::audio::AppSoundPlayer::instance().playbackVolumeForTest() == 0.25 &&
            ui::audio::AppSoundPlayer::instance().isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor, otherLease),
        "resource workspace SND live volume: stale pane edits cannot change the new preview lease or a global owner");
}

void checkSoundPlayingControlsPersistAcrossPointerExit()
{
    SoundPlaybackRequestScope audio;
    QTemporaryDir directory{};
    check(directory.isValid(), "resource workspace SND playing controls: temporary resources exist");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root(directory.path().toStdWString());
    std::filesystem::create_directories(root / "sound");
    check(writeBytes(root / "sound" / "loop.wav", soundFollowupPcmWave()),
          "resource workspace SND playing controls: valid PCM writes");
    auto resources = std::make_shared<assets::ResourceLocator>();
    resources->addRoot(root);
    auto context = testContext(ProjectType::IJIM);
    context.resources = resources;
    QCborArray rows;
    rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("loop.wav")));
    rows.append(soundFollowupRow(QStringLiteral("moving"), QStringLiteral("other.wav")));
    auto pane = soundFollowupPane(rows, context);
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    QTreeWidgetItem* const playingRow = tree ? soundAlternativeByWav(tree, QStringLiteral("loop.wav")) : nullptr;
    QTreeWidgetItem* const hoveredRow = tree ? soundAlternativeByWav(tree, QStringLiteral("other.wav")) : nullptr;
    check(tree && playingRow && hoveredRow && setSoundPreviewLoop(tree, playingRow, true),
          "resource workspace SND playing controls: playing and independently hovered rows are available");
    if ( !tree || !playingRow || !hoveredRow )
    {
        return;
    }
    QToolButton* const play = tree->findChild<QToolButton*>(QStringLiteral("snd.play"));
    QToolButton* const playingStop = tree->findChild<QToolButton*>(QStringLiteral("snd.playing.stop"));
    QToolButton* const playingLoop = tree->findChild<QToolButton*>(QStringLiteral("snd.playing.loop"));
    check(play && play->isVisible(), "resource workspace SND playing controls: shared Play is visible");
    check(playingStop && playingLoop, "resource workspace SND playing controls: pinned Stop and Loop exist");
    if ( !play || !playingStop || !playingLoop )
    {
        return;
    }
    play->click();
    const auto request = std::find_if(audio.requests.rbegin(), audio.requests.rend(),
                                      [](const auto& item)
                                      {
                                          return item.operation == SoundPlaybackRequestScope::Operation::playResource;
                                      });
    const std::uint64_t lease = request == audio.requests.rend() ? 0U : request->lease;
    const bool started = lease != 0U && request->loop &&
        ui::audio::AppSoundPlayer::instance().isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor, lease);
    check(started && playingStop->isVisible() && playingLoop->isVisible() &&
              play->size() == QSize(22, 22) && playingStop->size() == QSize(22, 22) &&
              playingLoop->size() == QSize(22, 22) && play->iconSize() == QSize(13, 13) &&
              playingStop->iconSize() == QSize(13, 13) && playingLoop->iconSize() == QSize(13, 13),
          "resource workspace SND playing controls: all five shared glyphs use 13-pixel icons inside unchanged 22-pixel buttons");

    const auto pinnedControlsVisible = [&]()
    {
        const QRect row = tree->visualItemRect(playingRow);
        bool stop = false;
        bool loop = false;
        for ( QToolButton* const control : tree->findChildren<QToolButton*>() )
        {
            if ( !control->isVisible() || !row.contains(control->geometry()) )
            {
                continue;
            }
            stop = stop || control->toolTip() == QStringLiteral("Stop this preview");
            loop = loop || (control->isCheckable() && control->isChecked() &&
                            control->toolTip() == QStringLiteral("Loop this preview"));
        }
        return stop && loop;
    };
    const QPoint otherPosition = tree->visualRect(tree->indexFromItem(hoveredRow, 1)).center();
    QMouseEvent hoverOther(QEvent::MouseMove, QPointF(otherPosition),
                           QPointF(tree->viewport()->mapToGlobal(otherPosition)), Qt::NoButton, Qt::NoButton,
                           Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &hoverOther);
    const bool pinnedWhileHoveringOtherRow = pinnedControlsVisible();
    const bool otherRowControlsVisible = play->isVisible() && tree->visualItemRect(hoveredRow).contains(play->geometry());

    const QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());

    const QPoint outside(-8, -8);
    QMouseEvent leaveViewport(QEvent::MouseMove, QPointF(outside),
                              QPointF(tree->viewport()->mapToGlobal(outside)), Qt::NoButton, Qt::NoButton,
                              Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &leaveViewport);
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool pinnedAfterPointerExit = pinnedControlsVisible();
    QMouseEvent hoverOtherAgain(QEvent::MouseMove, QPointF(otherPosition),
                                QPointF(tree->viewport()->mapToGlobal(otherPosition)), Qt::NoButton, Qt::NoButton,
                                Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &hoverOtherAgain);
    const bool otherRowControlsRestored = play->isVisible() && tree->visualItemRect(hoveredRow).contains(play->geometry());

    playingLoop->click();
    const auto pinnedLoopRequest = std::find_if(audio.requests.rbegin(), audio.requests.rend(),
                                                [](const auto& item)
                                                {
                                                    return item.operation ==
                                                        SoundPlaybackRequestScope::Operation::playResource;
                                                });
    const bool pinnedLoopTargetsItsPlayingRow = pinnedLoopRequest != audio.requests.rend() &&
        pinnedLoopRequest->lease == lease && !pinnedLoopRequest->loop &&
        pinnedLoopRequest->soundName == "loop.wav" && play->isVisible() &&
        tree->visualItemRect(hoveredRow).contains(play->geometry());
    playingStop->click();
    const auto stopRequest = std::find_if(audio.requests.rbegin(), audio.requests.rend(),
                                          [](const auto& item)
                                          {
                                              return item.operation == SoundPlaybackRequestScope::Operation::stop;
                                          });
    const bool pinnedStopTargetsItsPlayingLease = stopRequest != audio.requests.rend() &&
        stopRequest->owner == ui::audio::SoundPlaybackOwner::resourceEditor && stopRequest->lease == lease &&
        !ui::audio::AppSoundPlayer::instance().isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor, lease) &&
        !playingStop->isVisible() && play->isVisible() && tree->visualItemRect(hoveredRow).contains(play->geometry());

    QApplication::sendEvent(tree->viewport(), &hoverOtherAgain);
    play->click();
    const bool failedPlaybackKeepsNoPinnedControls = !playingStop->isVisible() && !playingLoop->isVisible() &&
        !ui::audio::AppSoundPlayer::instance().isPlaying(ui::audio::SoundPlaybackOwner::resourceEditor, lease);

    const QPoint playingPosition = tree->visualRect(tree->indexFromItem(playingRow, 1)).center();
    QMouseEvent hoverPlaying(QEvent::MouseMove, QPointF(playingPosition),
                             QPointF(tree->viewport()->mapToGlobal(playingPosition)), Qt::NoButton, Qt::NoButton,
                             Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &hoverPlaying);
    play->click();
    const auto completionRequest = std::find_if(audio.requests.rbegin(), audio.requests.rend(),
                                                [](const auto& item)
                                                {
                                                    return item.operation ==
                                                        SoundPlaybackRequestScope::Operation::playResource;
                                                });
    const std::uint64_t completionLease = completionRequest == audio.requests.rend() ? 0U
                                                                                       : completionRequest->lease;
    const bool completionStarted = completionLease == lease && playingStop->isVisible();
    ui::audio::AppSoundPlayer::instance().stop(ui::audio::SoundPlaybackOwner::resourceEditor, lease);
    const bool completionCleared = waitUntil([playingStop]() { return !playingStop->isVisible(); });

    QApplication::sendEvent(tree->viewport(), &hoverPlaying);
    play->click();
    const auto replacementPreview = std::find_if(audio.requests.rbegin(), audio.requests.rend(),
                                                 [](const auto& item)
                                                 {
                                                     return item.operation ==
                                                         SoundPlaybackRequestScope::Operation::playResource;
                                                 });
    const std::uint64_t paneLease = replacementPreview == audio.requests.rend() ? 0U : replacementPreview->lease;
    const std::uint64_t replacementLease = paneLease + 1U;
    const bool replacementStarted = paneLease == lease &&
        ui::audio::AppSoundPlayer::instance().playResource(ui::audio::SoundPlaybackOwner::resourceEditor,
                                                           *resources, "loop.wav", true, 0.4, replacementLease);
    const bool replacementCleared = waitUntil([playingStop]() { return !playingStop->isVisible(); });
    ui::audio::AppSoundPlayer::instance().stop(ui::audio::SoundPlaybackOwner::resourceEditor, replacementLease);

    QApplication::sendEvent(tree->viewport(), &hoverPlaying);
    play->click();
    const auto paneRestart = std::find_if(audio.requests.rbegin(), audio.requests.rend(),
                                          [](const auto& item)
                                          {
                                              return item.operation == SoundPlaybackRequestScope::Operation::playResource;
                                          });
    const bool paneRestarted = paneRestart != audio.requests.rend() && paneRestart->lease == lease;
    const bool ownerReplacementStarted = paneRestarted &&
        ui::audio::AppSoundPlayer::instance().playResource(ui::audio::SoundPlaybackOwner::resourcePicker,
                                                           *resources, "loop.wav", true, 0.4);
    const bool ownerReplacementCleared = waitUntil([playingStop]() { return !playingStop->isVisible(); }) &&
        ui::audio::AppSoundPlayer::instance().isPlaying(ui::audio::SoundPlaybackOwner::resourcePicker);
    ui::audio::AppSoundPlayer::instance().stop(ui::audio::SoundPlaybackOwner::resourcePicker);

    const bool boundedControls = tree->findChildren<QToolButton*>().size() <= 5;
    check(started && pinnedWhileHoveringOtherRow && otherRowControlsVisible && pinnedAfterPointerExit &&
              otherRowControlsRestored && pinnedLoopTargetsItsPlayingRow && pinnedStopTargetsItsPlayingLease &&
              failedPlaybackKeepsNoPinnedControls && completionStarted && completionCleared && replacementStarted &&
              replacementCleared && ownerReplacementStarted && ownerReplacementCleared && boundedControls &&
              state.value(QStringLiteral("rows")).toArray() == rows &&
              state.value(QStringLiteral("soundPreviewLoops")).toArray().at(0).toBool() &&
              !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "resource workspace SND playing controls: shared Stop and Loop stay on the playing row across other-row hover and pointer exit without changing authored rows");
}

void checkSoundAddModeClearsBlockingSearch()
{
    auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext()));
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    QLineEdit* const search = pane ? pane->findChild<QLineEdit*>(QStringLiteral("snd.search")) : nullptr;
    QAbstractButton* const add = pane ? button(pane.get(), "snd.add_mode") : nullptr;
    check(pane && tree && search && add, "resource workspace SND filtered Add Mode: pane and public controls exist");
    if ( !pane || !tree || !search || !add )
    {
        return;
    }
    QCborMap baseline = ui::resources::captureResourceAuthoringPaneState(pane.get());
    QCborArray row;
    for ( const QString& cell : {QStringLiteral("moving"), QStringLiteral("none"), QStringLiteral("0x80000040"),
                                 QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("1")} )
    {
        row.append(cell);
    }
    QCborArray rows;
    rows.append(row);
    baseline.insert(QStringLiteral("rows"), rows);
    baseline.remove(QStringLiteral("history")); // The selected source row defines an independent baseline.
    baseline.remove(QStringLiteral("soundPreviewLoops"));
    QString warning;
    const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), baseline, &warning);
    check(restored, "resource workspace SND filtered Add Mode: exact source row restores");
    if ( !restored )
    {
        return;
    }
    pane->show();
    tree->setAnimated(false);
    search->setText(QStringLiteral("moving"));
    QApplication::processEvents(QEventLoop::AllEvents);
    bool chosen = false;
    runWithModalDriver([add]() { add->click(); }, std::nullopt, std::nullopt, false, {},
                       [&](QWidget* active)
                       {
                           QDialog* const popup = qobject_cast<QDialog*>(active);
                           QListWidget* const modes =
                               popup ? popup->findChild<QListWidget*>(QStringLiteral("snd.mode.list")) : nullptr;
                           if ( !popup || !modes )
                           {
                               return;
                           }
                           const QList<QListWidgetItem*> choices =
                               modes->findItems(QStringLiteral("activate"), Qt::MatchExactly);
                           chosen = choices.size() == 1;
                           if ( chosen )
                           {
                               modes->setCurrentItem(choices.front());
                               popup->accept();
                           }
                           else
                           {
                               popup->reject();
                           }
                       });
    QTreeWidgetItem* const added = soundModeByBytes(tree, QByteArrayLiteral("activate"));
    const QCborMap after = ui::resources::captureResourceAuthoringPaneState(pane.get());
    check(chosen && added && !added->isHidden() && added->isExpanded() && tree->currentItem() == added &&
              tree->viewport()->rect().intersects(tree->visualItemRect(added)) && search->text().isEmpty() &&
              after.value(QStringLiteral("soundSearch")).toString().isEmpty() &&
              after.value(QStringLiteral("rows")).toArray() == rows &&
              !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
          "resource workspace SND filtered Add Mode: adding a mode outside the query clears search and selects its "
          "visible expanded empty group without authored changes");
}

void checkSoundEntryInsertionAndVolumeClipping()
{
    QTemporaryDir directory{};
    check(directory.isValid(), "resource workspace SND entry geometry: temporary directory exists");
    if ( !directory.isValid() )
    {
        return;
    }
    std::string source{};
    for ( int row = 0; row < 120; ++row )
    {
        source += "moving entry" + std::to_string(row) + ".wav 0x40 0.5 2.5 " + (row == 0 ? "0.123456789\n" : "1\n");
    }
    const std::filesystem::path path = std::filesystem::path(directory.path().toStdWString()) / "many.snd";
    check(writeBytes(path, source), "resource workspace SND entry geometry: many-row source writes");
    auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext()));
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    check(pane && tree, "resource workspace SND entry geometry: actual pane and tree exist");
    if ( !pane || !tree )
    {
        return;
    }
    openResourceFile(pane.get(), path);
    pane->resize(1000, 450);
    pane->show();
    tree->setAnimated(false);
    QTreeWidgetItem* const group = soundModeByBytes(tree, QByteArrayLiteral("moving"));
    check(group, "resource workspace SND entry geometry: moving group exists");
    if ( !group )
    {
        return;
    }
    group->setExpanded(true);
    QApplication::processEvents(QEventLoop::AllEvents);
    QTreeWidgetItem* footer = nullptr;
    for ( int index = 0; index < group->childCount(); ++index )
    {
        QTreeWidgetItem* const child = group->child(index);
        if ( child->data(0, Qt::UserRole + 20).toInt() == 2 )
        {
            footer = child;
        }
    }
    check(footer && footer->text(0) == QStringLiteral("＋ Add Entry"),
          "resource workspace SND entry geometry: each expanded mode has a spanned Add Entry footer");
    if ( footer )
    {
        tree->scrollToItem(footer, QAbstractItemView::PositionAtBottom);
        QApplication::processEvents(QEventLoop::AllEvents);
        const int beforeScroll = tree->verticalScrollBar()->value();
        const QPersistentModelIndex originalGroup(tree->indexFromItem(group, 0));
        const QPersistentModelIndex originalLast(tree->indexFromItem(soundAlternativeByWav(tree, QStringLiteral("entry119.wav")), 1));
        int modelResets = 0;
        const QMetaObject::Connection resetConnection = QObject::connect(
            tree->model(), &QAbstractItemModel::modelReset, pane.get(), [&modelResets]() { ++modelResets; });
        clickSoundTreeItem(tree, footer);
        QApplication::processEvents(QEventLoop::AllEvents);
        QTreeWidgetItem* const added = soundAlternativeByWav(tree, QStringLiteral("new_sound.wav"));
        const QCborArray rows =
            ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")).toArray();
        check(added && rows.size() == 121 &&
                  rows.at(119).toArray().at(1).toString() == QStringLiteral("entry119.wav") &&
                  rows.at(120).toArray().at(1).toString() == QStringLiteral("new_sound.wav") &&
                  tree->viewport()->rect().intersects(tree->visualItemRect(added)) &&
                  std::abs(tree->verticalScrollBar()->value() - beforeScroll) <= 4,
              "resource workspace SND entry geometry: Add Entry retains source order and keeps the inserted row near "
              "the previous scroll position");
        check(originalGroup.isValid() && originalLast.isValid() && modelResets == 0,
            "resource workspace SND entry geometry: Add Entry inserts locally without recreating existing groups or alternatives");
        QObject::disconnect(resetConnection);
        const int settledScroll = tree->verticalScrollBar()->value();
        tree->viewport()->update();
        QApplication::processEvents(QEventLoop::AllEvents);
        check(tree->verticalScrollBar()->value() == settledScroll,
            "resource workspace SND entry geometry: queued painting does not move the inserted-row scroll position again");
    }
    QTreeWidgetItem* const row = soundAlternativeByWav(tree, QStringLiteral("entry0.wav"));
    const QModelIndex index = row ? tree->indexFromItem(row, 5) : QModelIndex{};
    check(index.isValid(), "resource workspace SND entry geometry: volume cell exists");
    if ( !index.isValid() )
    {
        return;
    }
    QImage canvas(120, 26, QImage::Format_ARGB32);
    canvas.fill(QColor(32, 32, 32));
    QStyleOptionViewItem canvasOption;
    canvasOption.rect = canvas.rect();
    canvasOption.widget = tree;
    canvasOption.palette = tree->palette();
    canvasOption.state = QStyle::State_Enabled;
    {
        QPainter painter(&canvas);
        tree->itemDelegate()->paint(&painter, canvasOption, tree->indexFromItem(row, 1));
    }
    check(canvas.pixelColor(115, 24) == QColor(48, 48, 48),
          "resource workspace SND entry geometry: expanded entries paint a slightly lighter dark-gray canvas");
    QImage painted(260, 40, QImage::Format_ARGB32);
    const QColor untouched(203, 47, 177);
    painted.fill(untouched);
    QStyleOptionViewItem option;
    option.rect = QRect(130, 5, 74, 26);
    option.widget = tree;
    option.palette = tree->palette();
    option.state = QStyle::State_Enabled;
    {
        QPainter painter(&painted);
        tree->itemDelegate()->paint(&painter, option, index);
    }
    bool clipped = true;
    for ( int y = 0; y < painted.height(); ++y )
    {
        for ( int x = 0; x < painted.width(); ++x )
        {
            if ( !option.rect.contains(x, y) && painted.pixelColor(x, y) != untouched )
            {
                clipped = false;
            }
        }
    }
    check(clipped, "resource workspace SND entry geometry: resized narrow volume cells never paint sliders or values "
                   "over neighboring fields");
}

void checkSoundWavPickerProjectCategoriesAndAll()
{
    QTemporaryDir directory{};
    check(directory.isValid(), "resource workspace WAV picker: temporary directory exists");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path root = std::filesystem::path(directory.path().toStdWString());
    const std::filesystem::path project = root / "project";
    const std::filesystem::path mounted = root / "assets";
    std::filesystem::create_directories(project / "sound");
    std::filesystem::create_directories(mounted / "sound");
    check(writeBytes(project / "ijimwavs.jlh", "SUB Project sounds\n  LIST steps Project footsteps\nSUBEND\n") &&
              writeBytes(project / "ijimwavs.jll", "[steps]\nproject.wav\n") &&
              writeBytes(project / "sound" / "project.wav", "enumeration fixture") &&
              writeBytes(mounted / "sound" / "mounted.wav", "enumeration fixture") &&
              writeBytes(mounted / "flat.wav", "enumeration fixture") &&
              writeBytes(project / "picker.snd", "moving none 0x40 0.5 2.5 1\n"),
          "resource workspace WAV picker: independent project pair and loose-root resources write");
    auto resources = std::make_shared<assets::ResourceLocator>();
    resources->addRoot(mounted);
    resources->addRoot(project);
    ui::resources::ResourceAuthoringContext context = testContext(ProjectType::IJIM);
    context.resources = resources;
    context.projectDirectory = project;
    auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, context));
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    check(pane && tree, "resource workspace WAV picker: IJIM authoring pane exists");
    if ( !pane || !tree )
    {
        return;
    }
    openResourceFile(pane.get(), project / "picker.snd");
    QTreeWidgetItem* const row = soundAlternativeByWav(tree, QStringLiteral("none"));
    check(row, "resource workspace WAV picker: source row exists");
    if ( !row )
    {
        return;
    }
    bool inspected = false;
    bool allCorrect = false;
    bool projectCorrect = false;
    QTimer inspect;
    inspect.setInterval(10);
    QObject::connect(
        &inspect, &QTimer::timeout, QApplication::instance(),
        [&]()
        {
            QDialog* const picker = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if ( inspected || !picker || picker->windowTitle() != QStringLiteral("Sound Picker") )
            {
                return;
            }
            QTreeWidget* const categories = picker->findChild<QTreeWidget*>();
            QListWidget* const names = picker->findChild<QListWidget*>();
            if ( !categories || !names || !names->isEnabled() || categories->topLevelItemCount() == 0 )
            {
                return;
            }
            inspected = true;
            const auto visibleNames = [names]()
            {
                QStringList result;
                for ( int index = 0; index < names->count(); ++index )
                {
                    result.append(names->item(index)->text());
                }
                result.sort(Qt::CaseSensitive);
                return result;
            };
            QTreeWidgetItem* all = nullptr;
            QTreeWidgetItem* projectCategory = nullptr;
            for ( int index = 0; index < categories->topLevelItemCount(); ++index )
            {
                QTreeWidgetItem* const category = categories->topLevelItem(index);
                if ( category->text(0) == QStringLiteral("All") )
                {
                    all = category;
                }
                if ( category->text(0) == QStringLiteral("Project sounds") && category->childCount() == 1 &&
                     category->child(0)->text(0) == QStringLiteral("Project footsteps") )
                {
                    projectCategory = category->child(0);
                }
            }
            if ( all )
            {
                categories->setCurrentItem(all);
                allCorrect = visibleNames() == QStringList{QStringLiteral("flat.wav"), QStringLiteral("mounted.wav"),
                                                           QStringLiteral("project.wav")};
            }
            if ( projectCategory )
            {
                categories->setCurrentItem(projectCategory);
                projectCorrect = visibleNames() == QStringList{QStringLiteral("project.wav")};
            }
            picker->reject();
        });
    inspect.start();
    runWithModalDriver(
        [&]()
        {
            tree->setCurrentItem(row, 1);
            (void)QMetaObject::invokeMethod(tree, "itemDoubleClicked", Qt::DirectConnection,
                                            Q_ARG(QTreeWidgetItem*, row), Q_ARG(int, 1));
        });
    inspect.stop();
    check(inspected && allCorrect,
          "resource workspace WAV picker: All includes mounted and project WAVs including flat-root files");
    check(inspected && projectCorrect,
          "resource workspace WAV picker: the authoring popup uses the exact project-local IJIM category pair");
    check(!ui::resources::resourceAuthoringPaneIsModified(pane.get()) &&
              readBytes(project / "picker.snd") == "moving none 0x40 0.5 2.5 1\n",
          "resource workspace WAV picker: canceling category inspection leaves the authored source untouched");
}

void checkSoundModeHeadersAndInlineVolume()
{
    QTemporaryDir directory{};
    check(directory.isValid(), "resource workspace SND inline layout: temporary directory is available");
    if ( !directory.isValid() )
    {
        return;
    }
    const std::filesystem::path path = std::filesystem::path(directory.path().toStdWString()) / "inline.snd";
    check(writeBytes(path, "moving first.wav 0x80000040 0.5 2.5 1.5\ncreate second.wav 0x40 0.25 1.5 0.75\n"),
          "resource workspace SND inline layout: two-mode source writes");
    auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext()));
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    check(pane && tree, "resource workspace SND inline layout: pane and tree are available");
    if ( !pane || !tree )
    {
        return;
    }
    openResourceFile(pane.get(), path);
    QTreeWidgetItem* const first = soundAlternativeByWav(tree, QStringLiteral("first.wav"));
    QTreeWidgetItem* const second = soundAlternativeByWav(tree, QStringLiteral("second.wav"));
    check(first && second, "resource workspace SND inline layout: both source rows are available");
    if ( !first || !second )
    {
        return;
    }
    pane->resize(1050, 650);
    pane->show();
    pane->activateWindow();
    const bool animated = tree->isAnimated();
    tree->setAnimated(false);
    first->parent()->setExpanded(true);
    second->parent()->setExpanded(true);
    QApplication::processEvents(QEventLoop::AllEvents);
    check(animated && tree->header()->isHidden() && tree->visualItemRect(first->parent()).height() >= 30 &&
              QFontMetrics(first->parent()->font(0)).height() > QFontMetrics(tree->font()).height(),
          "resource workspace SND inline layout: animated mode titles are taller and larger while the global column "
          "header is hidden");
    const auto modeHeader = [](QTreeWidgetItem* mode)
    {
        for ( int index = 0; mode && index < mode->childCount(); ++index )
        {
            QTreeWidgetItem* const child = mode->child(index);
            if ( child->text(1) == QStringLiteral("Sound") && child->text(2) == QStringLiteral("Flags") )
            {
                return child;
            }
        }
        return static_cast<QTreeWidgetItem*>(nullptr);
    };
    QTreeWidgetItem* const firstHeader = modeHeader(first->parent());
    QTreeWidgetItem* const secondHeader = modeHeader(second->parent());
    check(firstHeader && secondHeader && !firstHeader->isHidden() && !secondHeader->isHidden() &&
              first->parent()->indexOfChild(firstHeader) == 0 && second->parent()->indexOfChild(secondHeader) == 0,
          "resource workspace SND inline layout: each expanded mode starts with its own column headings");
    if ( firstHeader && secondHeader )
    {
        const QRect cell = tree->visualRect(tree->indexFromItem(firstHeader, 1));
        const QPoint press(cell.right(), cell.center().y());
        const QPoint moved = press + QPoint(27, 0);
        const int beforeWidth = tree->columnWidth(1);
        QMouseEvent resizePress(QEvent::MouseButtonPress, QPointF(press), QPointF(tree->viewport()->mapToGlobal(press)),
                                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent resizeMove(QEvent::MouseMove, QPointF(moved), QPointF(tree->viewport()->mapToGlobal(moved)),
                               Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent resizeRelease(QEvent::MouseButtonRelease, QPointF(moved),
                                  QPointF(tree->viewport()->mapToGlobal(moved)), Qt::LeftButton, Qt::NoButton,
                                  Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &resizePress);
        QApplication::sendEvent(tree->viewport(), &resizeMove);
        QApplication::sendEvent(tree->viewport(), &resizeRelease);
        check(tree->columnWidth(1) == beforeWidth + 27 &&
                  tree->visualRect(tree->indexFromItem(firstHeader, 2)).left() ==
                      tree->visualRect(tree->indexFromItem(secondHeader, 2)).left(),
              "resource workspace SND inline layout: dragging a local header resizes and aligns columns in every mode");
        const int settledWidth = tree->columnWidth(1);
        const QPoint resizeStart =
            tree->visualRect(tree->indexFromItem(firstHeader, 1)).topRight() + QPoint(0, cell.height() / 2);
        const auto beginResize = [&]()
        {
            QMouseEvent pressAgain(QEvent::MouseButtonPress, QPointF(resizeStart),
                                   QPointF(tree->viewport()->mapToGlobal(resizeStart)), Qt::LeftButton, Qt::LeftButton,
                                   Qt::NoModifier);
            QApplication::sendEvent(tree->viewport(), &pressAgain);
        };
        const auto moveAfterInterruption = [&](const Qt::MouseButtons buttons)
        {
            const QPoint changed = resizeStart + QPoint(39, 0);
            QMouseEvent afterInterruption(QEvent::MouseMove, QPointF(changed),
                                          QPointF(tree->viewport()->mapToGlobal(changed)), Qt::NoButton, buttons,
                                          Qt::NoModifier);
            QApplication::sendEvent(tree->viewport(), &afterInterruption);
        };
        beginResize();
        moveAfterInterruption(Qt::NoButton);
        check(tree->columnWidth(1) == settledWidth, "resource workspace SND inline layout: buttonless pointer movement "
                                                    "ends header resizing without changing width");
        for ( const QEvent::Type interruption : {QEvent::UngrabMouse, QEvent::WindowDeactivate} )
        {
            tree->setColumnWidth(1, settledWidth);
            beginResize();
            QEvent interrupted(interruption);
            QApplication::sendEvent(tree->viewport(), &interrupted);
            moveAfterInterruption(Qt::LeftButton);
            check(tree->columnWidth(1) == settledWidth,
                  "resource workspace SND inline layout: lost mouse ownership or deactivation ends header resizing");
        }
        QMouseEvent cleanupRelease(QEvent::MouseButtonRelease, QPointF(resizeStart),
                                   QPointF(tree->viewport()->mapToGlobal(resizeStart)), Qt::LeftButton, Qt::NoButton,
                                   Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &cleanupRelease);
        tree->setColumnWidth(1, settledWidth);
    }
    check(first->toolTip(3) == QStringLiteral("Minimum radius in engine world units. SED distance display: 0.1 world "
                                              "unit = 1 m. Full volume inside this radius.") &&
              first->toolTip(4) ==
                  QStringLiteral("Maximum radius in engine world units. SED distance display: 0.1 world unit = 1 m. "
                                 "Sound is silent beyond this radius; it must be at least the minimum radius.") &&
              first->toolTip(5) == QStringLiteral("Maximum volume. Explicit slider edits use 0 to 1. A higher imported "
                                                  "value is retained until you edit it."),
          "resource workspace SND inline layout: fixed range and volume hover help explains units and authored limits");
    const QCborMap beforeVolume = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QRect volume = tree->visualRect(tree->indexFromItem(first, 5));
    const QPoint press(volume.left() + 18, volume.center().y());
    const QPoint drag(volume.left() + std::max(30, (volume.width() - 100) / 2), volume.center().y());
    QMouseEvent sliderPress(QEvent::MouseButtonPress, QPointF(press), QPointF(tree->viewport()->mapToGlobal(press)),
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent sliderMove(QEvent::MouseMove, QPointF(drag), QPointF(tree->viewport()->mapToGlobal(drag)), Qt::NoButton,
                           Qt::LeftButton, Qt::NoModifier);
    QMouseEvent sliderRelease(QEvent::MouseButtonRelease, QPointF(drag), QPointF(tree->viewport()->mapToGlobal(drag)),
                              Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &sliderPress);
    QApplication::sendEvent(tree->viewport(), &sliderMove);
    const QCborMap pendingVolume = ui::resources::captureResourceAuthoringPaneState(pane.get());
    QWidget* const pendingEditor = tree->indexWidget(tree->indexFromItem(first, 5));
    QSlider* const pendingSlider =
        pendingEditor ? pendingEditor->findChild<QSlider*>(QStringLiteral("snd.volume.slider")) : nullptr;
    const QCborArray pendingRows = pendingVolume.value(QStringLiteral("rows")).toArray();
    bool pendingParsed = false;
    const float pendingValue =
        pendingRows.size() == 2 ? pendingRows.at(0).toArray().at(5).toString().toFloat(&pendingParsed) : -1.0f;
    check(pendingSlider && pendingParsed &&
              std::bit_cast<std::uint32_t>(pendingValue) ==
                  std::bit_cast<std::uint32_t>(static_cast<float>(pendingSlider->value()) / 1000.0f) &&
              pendingValue != 1.5f && pendingVolume.value(QStringLiteral("modified")).toBool(),
          "resource workspace SND inline layout: held volume drag captures exact live value and modified state before "
          "release");
    QApplication::sendEvent(tree->viewport(), &sliderRelease);
    QApplication::processEvents(QEventLoop::AllEvents);
    const QCborMap afterVolume = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborArray rows = afterVolume.value(QStringLiteral("rows")).toArray();
    bool parsed = false;
    const float value = rows.size() == 2 ? rows.at(0).toArray().at(5).toString().toFloat(&parsed) : -1.0f;
    check(beforeVolume.value(QStringLiteral("rows")).toArray().at(0).toArray().at(5).toString() ==
                  QStringLiteral("1.5") &&
              parsed && value >= 0.0f && value <= 1.0f && ui::resources::resourceAuthoringPaneIsModified(pane.get()) &&
              rows.size() == 2 && rows.at(1) == beforeVolume.value(QStringLiteral("rows")).toArray().at(1),
          "resource workspace SND inline layout: a visible volume slider accepts a direct drag without double-clicking "
          "and preserves untouched rows");
}

void checkSoundAddModeFiltersExistingModes()
{
    for ( const ProjectType project : {ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM} )
    {
        auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
            nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext(project)));
        QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
        QAbstractButton* const add = pane ? button(pane.get(), "snd.add_mode") : nullptr;
        QLineEdit* const search = pane ? pane->findChild<QLineEdit*>(QStringLiteral("snd.search")) : nullptr;
        check(pane && tree && add && search,
              "resource workspace SND add mode: dedicated top button and mode search are available");
        if ( !pane || !tree || !add || !search )
        {
            continue;
        }
        QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
        QCborArray rows{};
        for ( const QString& mode : {QStringLiteral("moving"), QStringLiteral("create")} )
        {
            QCborArray row{};
            for ( const QString& cell : {mode, QStringLiteral("none"), QStringLiteral("0x80000040"),
                                         QStringLiteral("0.5"), QStringLiteral("2.5"), QStringLiteral("1")} )
            {
                row.append(cell);
            }
            rows.append(row);
        }
        state.insert(QStringLiteral("rows"), rows);
        state.remove(QStringLiteral("history")); // Seed independent modes before testing the Add Mode popup.
        state.remove(QStringLiteral("soundPreviewLoops"));
        QString warning{};
        const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning);
        pane->show();
        QApplication::processEvents(QEventLoop::AllEvents);
        check(restored && add->mapTo(pane.get(), QPoint{}).y() < search->mapTo(pane.get(), QPoint{}).y(),
              "resource workspace SND add mode: Add Mode appears above the search field");
        bool popupSeen = false;
        bool offeredExactModes = false;
        bool searchFiltered = false;
        runWithModalDriver(
            [add]()
            {
                add->click();
            },
            std::nullopt, std::nullopt, false, {},
            [&](QWidget* active)
            {
                auto* const popup = qobject_cast<QDialog*>(active);
                QLineEdit* const filter =
                    popup ? popup->findChild<QLineEdit*>(QStringLiteral("snd.mode.search")) : nullptr;
                QListWidget* const list =
                    popup ? popup->findChild<QListWidget*>(QStringLiteral("snd.mode.list")) : nullptr;
                if ( !popup || !filter || !list )
                {
                    return;
                }
                popupSeen = true;
                const int expected = project == ProjectType::IJIM ? 138 : 93;
                offeredExactModes = list->count() == expected;
                for ( int index = 0; offeredExactModes && index < list->count(); ++index )
                {
                    const QString mode = list->item(index)->text();
                    offeredExactModes = mode != QStringLiteral("moving") && mode != QStringLiteral("create") &&
                                        mode != QStringLiteral("--reserved--");
                }
                filter->setText(QStringLiteral("reserved"));
                int visible = 0;
                QListWidgetItem* chosen = nullptr;
                for ( int index = 0; index < list->count(); ++index )
                {
                    QListWidgetItem* const item = list->item(index);
                    if ( !item->isHidden() )
                    {
                        ++visible;
                        if ( item->text() == QStringLiteral("reserved1") )
                        {
                            chosen = item;
                        }
                    }
                }
                searchFiltered = visible == 8 && chosen;
                if ( chosen )
                {
                    list->setCurrentItem(chosen);
                    popup->accept();
                }
                else
                {
                    popup->reject();
                }
            });
        const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(pane.get());
        const QCborArray after = captured.value(QStringLiteral("rows")).toArray();
        const QCborArray emptyModes = captured.value(QStringLiteral("soundEmptyModes")).toArray();
        check(popupSeen && offeredExactModes && searchFiltered && after == rows && emptyModes.size() == 1 &&
                  emptyModes.at(0).toString() == QStringLiteral("reserved1"),
              "resource workspace SND add mode: searchable project list excludes represented modes and retains valid "
              "named reserved modes");
    }
}

void checkSoundRadiusScrubCommitsOnRelease()
{
    auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext()));
    QTreeWidget* const tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    check(pane && tree, "resource workspace SND scrub: pane and table are available");
    if ( !pane || !tree || !addSoundMode(pane.get(), tree, QStringLiteral("moving")) )
    {
        check(false, "resource workspace SND scrub: initial alternative can be authored");
        return;
    }
    QCborMap clean = ui::resources::captureResourceAuthoringPaneState(pane.get());
    clean.remove(QStringLiteral("history")); // Adopt the setup mode and alternative as a clean scrub baseline.
    clean.insert(QStringLiteral("modified"), false);
    QString warning{};
    check(ui::resources::restoreResourceAuthoringPaneState(pane.get(), clean, &warning),
          "resource workspace SND scrub: canonical clean baseline restores");
    QTreeWidgetItem* const row = soundAlternativeItems(tree).value(0);
    pane->show();
    pane->activateWindow();
    QApplication::processEvents(QEventLoop::AllEvents);
    tree->setAnimated(false);
    row->parent()->setExpanded(true);
    tree->setCurrentItem(row, 3);
    tree->editItem(row, 3);
    QApplication::processEvents(QEventLoop::AllEvents);
    auto* const scrub = qobject_cast<ui::ScrubFloatEdit*>(tree->indexWidget(tree->indexFromItem(row, 3)));
    check(scrub, "resource workspace SND scrub: radius editor opens");
    if ( !scrub )
    {
        return;
    }
    QLineEdit* const line = scrub->lineEdit();
    const QPointer<ui::ScrubFloatEdit> guardedScrub(scrub);
    const QPointer<QLineEdit> guardedLine(line);
    line->setFocus(Qt::OtherFocusReason);
    const QPoint physicalCursor = QCursor::pos();
    QCursor::setPos(line->mapToGlobal(line->rect().center()));
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool ready = guardedScrub && guardedLine && scrub->isEnabled() && line->isEnabled() && line->isVisible() &&
                       line->hasFocus() && (line->alignment() & Qt::AlignHorizontal_Mask) == Qt::AlignHCenter &&
                       tree->indexWidget(tree->indexFromItem(row, 3)) == scrub;
    check(ready,
          "resource workspace SND scrub: the live radius editor is enabled, visible, and focused before dragging");
    if ( !ready )
    {
        std::cerr << "[SND scrub precondition] editorAlive=" << static_cast<bool>(guardedScrub)
                  << " lineAlive=" << static_cast<bool>(guardedLine)
                  << " enabled=" << (guardedLine && guardedLine->isEnabled())
                  << " visible=" << (guardedLine && guardedLine->isVisible())
                  << " focused=" << (guardedLine && guardedLine->hasFocus())
                  << " modal=" << (QApplication::activeModalWidget() != nullptr) << '\n';
        QCursor::setPos(physicalCursor);
        return;
    }
    const QString initialText = line->text();
    int scrubStarts = 0;
    int scrubFinishes = 0;
    class RadiusInputTrace final : public QObject
    {
    public:
        std::array<int, 3> mouseEvents{};

    protected:
        bool eventFilter(QObject*, QEvent* event) override
        {
            if ( event->type() == QEvent::MouseButtonPress )
            {
                ++mouseEvents[0];
            }
            else if ( event->type() == QEvent::MouseMove )
            {
                ++mouseEvents[1];
            }
            else if ( event->type() == QEvent::MouseButtonRelease )
            {
                ++mouseEvents[2];
            }
            return false;
        }
    } inputTrace;
    line->installEventFilter(&inputTrace);
    QObject::connect(scrub, &ui::ScrubFloatEdit::scrubStarted, &inputTrace,
                     [&](double)
                     {
                         ++scrubStarts;
                     });
    QObject::connect(scrub, &ui::ScrubFloatEdit::scrubFinished, &inputTrace,
                     [&](double)
                     {
                         ++scrubFinishes;
                     });
    const QPoint start = line->rect().center();
    const QPoint moved = start + QPoint(28, 0);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(start), QPointF(line->mapToGlobal(start)), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    const int pressGlobalX = press.globalPosition().toPoint().x();
    std::cerr << "[SND scrub input] press globalX=" << pressGlobalX << '\n';
    QApplication::sendEvent(line, &press);
    QMouseEvent move(QEvent::MouseMove, QPointF(moved), QPointF(line->mapToGlobal(moved)), Qt::NoButton, Qt::LeftButton,
                     Qt::NoModifier);
    const int moveGlobalX = move.globalPosition().toPoint().x();
    std::cerr << "[SND scrub input] move globalX=" << moveGlobalX << '\n';
    check(moveGlobalX - pressGlobalX == 28,
          "resource workspace SND scrub: sequential input dispatch retains the exact 28-pixel gesture");
    QApplication::sendEvent(line, &move);
    const QString movedText = guardedLine ? guardedLine->text() : QStringLiteral("<destroyed>");
    if ( guardedLine )
    {
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(moved), QPointF(guardedLine->mapToGlobal(moved)),
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        std::cerr << "[SND scrub input] release globalX=" << release.globalPosition().toPoint().x() << '\n';
        QApplication::sendEvent(guardedLine.data(), &release);
    }
    QCursor::setPos(physicalCursor);
    const QCborMap edited = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const bool modified = ui::resources::resourceAuthoringPaneIsModified(pane.get());
    const bool changedRows = edited.value(QStringLiteral("rows")) != clean.value(QStringLiteral("rows"));
    if ( !modified || !changedRows )
    {
        std::cerr << "[SND scrub diagnostic] starts=" << scrubStarts << " finishes=" << scrubFinishes
                  << " globalX=" << pressGlobalX << ',' << moveGlobalX << " delivered=" << inputTrace.mouseEvents[0]
                  << ',' << inputTrace.mouseEvents[1] << ',' << inputTrace.mouseEvents[2]
                  << " editorAlive=" << static_cast<bool>(guardedScrub) << " initial=" << initialText.toStdString()
                  << " moved=" << movedText.toStdString() << " modified=" << modified << " changedRows=" << changedRows
                  << '\n';
    }
    check(modified && changedRows,
          "resource workspace SND scrub: releasing a radius drag immediately commits a modified history gesture");
    tree->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
    const bool undone = ui::resources::resourceAuthoringPaneUndoRedo(pane.get(), false);
    check(undone && ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows")) ==
                        clean.value(QStringLiteral("rows")),
          "resource workspace SND scrub: one local Undo restores the exact row before a radius drag");
}

void checkSoundProjectFlagPopups()
{
    QTemporaryDir directory{};
    if ( !directory.isValid() )
    {
        check(false, "resource workspace SND project flags: temporary directory is available");
        return;
    }
    const std::filesystem::path path = std::filesystem::path(directory.path().toStdWString()) / "flags.snd";
    check(writeBytes(path, "moving flags.wav 0x80000040 0.5 2.5 1\n"),
          "resource workspace SND project flags: unknown-bit fixture writes");
    const QStringList classicNames{QStringLiteral("Loop"),
                                   QStringLiteral("Remove after fadeout"),
                                   QStringLiteral("Ambient (no panning)"),
                                   QStringLiteral("Doppler FX"),
                                   QStringLiteral("Fade in"),
                                   QStringLiteral("Fade out"),
                                   QStringLiteral("Absolute position"),
                                   QStringLiteral("Follow Thing"),
                                   QStringLiteral("High priority"),
                                   QStringLiteral("Highest priority"),
                                   QStringLiteral("No duplicate playback"),
                                   QStringLiteral("No duplicates on Thing"),
                                   QStringLiteral("Precache")};
    const QStringList ijimNames{QStringLiteral("Loop"),
                                QStringLiteral("Remove after fadeout"),
                                QStringLiteral("Ambient"),
                                QStringLiteral("Doppler FX"),
                                QStringLiteral("Fade in"),
                                QStringLiteral("Fade out"),
                                QStringLiteral("Absolute position"),
                                QStringLiteral("Thing position"),
                                QStringLiteral("High priority"),
                                QStringLiteral("Highest priority"),
                                QStringLiteral("Play once"),
                                QStringLiteral("Play thing once")};
    for ( const ProjectType project : {ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM} )
    {
        auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
            nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext(project)));
        if ( !pane )
        {
            check(false, "resource workspace SND project flags: project pane is available");
            continue;
        }
        openResourceFile(pane.get(), path);
        QTreeWidget* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
        QTreeWidgetItem* const row = soundAlternativeByWav(tree, QStringLiteral("flags.wav"));
        if ( !tree || !row )
        {
            check(false, "resource workspace SND project flags: fixed source row is available");
            continue;
        }
        const bool ijim = project == ProjectType::IJIM;
        const QStringList expected = ijim ? ijimNames : classicNames;
        const QStringList selected = ijim ? QStringList{QStringLiteral("Loop"), QStringLiteral("Absolute position"),
                                                        QStringLiteral("Play thing once")}
                                          : QStringList{QStringLiteral("Loop"),
                                                        QStringLiteral("Ambient (no panning)"),
                                                        QStringLiteral("Absolute position"),
                                                        QStringLiteral("High priority"),
                                                        QStringLiteral("No duplicates on Thing"),
                                                        QStringLiteral("Precache")};
        QStringList actual{};
        bool popupSeen = false;
        runWithModalDriver(
            [&]()
            {
                (void)QMetaObject::invokeMethod(tree, "itemDoubleClicked", Qt::DirectConnection,
                                                Q_ARG(QTreeWidgetItem*, row), Q_ARG(int, 2));
            },
            std::nullopt, std::nullopt, false, {},
            [&](QWidget* active)
            {
                auto* const popup = qobject_cast<QDialog*>(active);
                if ( !popup || popup->objectName() != QStringLiteral("snd.flags.popup") )
                {
                    return;
                }
                popupSeen = true;
                for ( QCheckBox* const flag : popup->findChildren<QCheckBox*>() )
                {
                    actual.append(flag->text());
                    flag->setChecked(selected.contains(flag->text()));
                }
                popup->accept();
            });
        check(popupSeen && actual == expected &&
                  row->text(2) == (ijim ? QStringLiteral("0x80000841") : QStringLiteral("0x80004945")),
              ijim ? "resource workspace SND project flags: IJIM offers exactly its twelve authored bits and Apply "
                     "retains unknown bits"
                   : "resource workspace SND project flags: classic and MotS offer Precache plus accurate no-pan and "
                     "duplicate flags while Apply retains unknown bits");
    }
}

void checkSoundModalAndSearchLifetimes()
{
    QTemporaryDir directory{};
    if ( !directory.isValid() )
    {
        check(false, "resource workspace SND lifetime: temporary directory is available");
        return;
    }
    const std::filesystem::path source = std::filesystem::path(directory.path().toStdWString()) / "lifetime.snd";
    check(writeBytes(source, "moving first.wav 0x80000040 0.5 2.5 1\ncreate second.wav 0x40 0.5 2.5 1\n"),
          "resource workspace SND lifetime: synthetic source writes");
    auto pane = std::unique_ptr<QDialog>(ui::resources::createResourceAuthoringEditor(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext()));
    if ( !pane )
    {
        check(false, "resource workspace SND lifetime: pane is available");
        return;
    }
    openResourceFile(pane.get(), source);
    QTreeWidget* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    QTreeWidgetItem* const first = soundAlternativeByWav(tree, QStringLiteral("first.wav"));
    if ( !tree || !first )
    {
        check(false, "resource workspace SND lifetime: tree and source row are available");
        return;
    }
    bool popupOpened = false;
    runWithModalDriver(
        [&]()
        {
            (void)QMetaObject::invokeMethod(tree, "itemDoubleClicked", Qt::DirectConnection,
                                            Q_ARG(QTreeWidgetItem*, first), Q_ARG(int, 2));
        },
        std::nullopt, std::nullopt, false, {},
        [&](QWidget* active)
        {
            QDialog* const popup = qobject_cast<QDialog*>(active);
            if ( !popup || popup->objectName() != QStringLiteral("snd.flags.popup") )
            {
                return;
            }
            popupOpened = true;
            const QList<QCheckBox*> checks = popup->findChildren<QCheckBox*>();
            if ( !checks.isEmpty() )
            {
                checks.front()->setChecked(true);
            }
            first->setText(2, QStringLiteral("0x80000060"));
            popup->accept();
        });
    check(popupOpened && first->text(2) == QStringLiteral("0x80000060"),
          "resource workspace SND lifetime: flags Apply rejects a stale modal snapshot and retains unknown bits from "
          "the intervening edit");

    QPointer<QWidget> callbackPane{};
    int publications = 0;
    QWidget* const created = ui::resources::createResourceAuthoringPane(
        nullptr, ui::resources::ResourceAuthoringKind::soundClass, testContext(),
        [&callbackPane, &publications](QWidget* changed)
        {
            if ( callbackPane && callbackPane.data() == changed )
            {
                ++publications;
                delete callbackPane.data();
            }
        });
    QPointer<QWidget> held(created);
    // Build the state before arming the owner-deleting callback.
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(created);
    QCborArray rows{};
    for ( const QString& mode : {QStringLiteral("moving"), QStringLiteral("create")} )
    {
        QCborArray row{};
        for ( const QString& value : {mode, QStringLiteral("none"), QStringLiteral("0x40"), QStringLiteral("0.5"),
                                      QStringLiteral("2.5"), QStringLiteral("1")} )
        {
            row.append(value);
        }
        rows.append(row);
    }
    state.insert(QStringLiteral("rows"), rows);
    state.remove(QStringLiteral("history")); // Seed source rows before arming the owner-deleting search callback.
    state.remove(QStringLiteral("soundPreviewLoops"));
    QString warning{};
    const bool restored = ui::resources::restoreResourceAuthoringPaneState(created, state, &warning);
    callbackPane = held;
    QLineEdit* const search = held ? held->findChild<QLineEdit*>(QStringLiteral("snd.search")) : nullptr;
    QTreeWidget* const callbackTree = held ? held->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    check(search && callbackTree, "resource workspace SND lifetime: deferred search fixture has its public controls");
    if ( search )
    {
        search->setText(QStringLiteral("moving"));
        search->setText(QStringLiteral("create"));
        search->setText(QStringLiteral("moving"));
    }
    QTreeWidgetItem* moving = nullptr;
    QTreeWidgetItem* create = nullptr;
    for ( int index = 0; callbackTree && index < callbackTree->topLevelItemCount(); ++index )
    {
        QTreeWidgetItem* const group = callbackTree->topLevelItem(index);
        const QByteArray mode = group->data(0, Qt::UserRole + 22).toByteArray();
        if ( mode == QByteArrayLiteral("moving") )
        {
            moving = group;
        }
        else if ( mode == QByteArrayLiteral("create") )
        {
            create = group;
        }
    }
    check(restored && held && publications == 0 && search && search->text() == QStringLiteral("moving") && moving &&
              !moving->isHidden() && moving->isExpanded() && create && create->isHidden(),
          "resource workspace SND lifetime: search filters synchronously before any coalesced host publication");
    QCoreApplication::processEvents();
    check(restored && callbackPane.isNull() && publications == 1,
          "resource workspace SND lifetime: mode search coalesces one safe deferred publication that may delete its "
          "owner");
}

void checkResourcePaneChangedCallbackCanDeleteOwner()
{
    QPointer<QWidget> pane{};
    QWidget* const created = ui::resources::createResourceAuthoringPane(
        nullptr,
        ui::resources::ResourceAuthoringKind::stringTable,
        testContext(),
        [&pane](QWidget* changedPane)
        {
            if ( pane && pane.data() == changedPane )
            {
                delete pane.data();
            }
        });
    pane = created;
    check(pane != nullptr,
        "resource workspace callback lifetime: isolated pane is created");
    if ( !pane )
    {
        return;
    }
    QAbstractButton* const add = button(pane.data(), "uni.add");
    check(add != nullptr,
        "resource workspace callback lifetime: Add control is available");
    if ( !add )
    {
        return;
    }
    add->click();
    check(pane.isNull(),
        "resource workspace callback lifetime: deleting owner from its synchronous change callback completes safely");
}

} // namespace

void runResourceAuthoringEditorTests()
{
    static constexpr const char* kCommonControls[] = {
        "resource_authoring.new",
        "resource_authoring.open",
        "resource_authoring.save",
        "resource_authoring.save_as",
        "resource_authoring.status",
    };
    checkEditorContract(
        ui::resources::ResourceAuthoringKind::sprite,
        QStringLiteral("Sprite Editor"),
        kCommonControls,
        std::size(kCommonControls));

    static constexpr const char* kSpriteControls[] = {
        "spr.material",
        "spr.material.pick",
        "spr.type",
        "spr.width",
        "spr.height",
        "spr.geometry_mode",
        "spr.light_mode",
        "spr.texture_mode",
        "spr.extra_light.swatch",
        "spr.offset.x",
        "spr.offset.y",
        "spr.offset.z",
        "spr.preview.gl_view",
    };
    {
        const ui::resources::ResourceAuthoringContext context{
            std::make_shared<const assets::ResourceLocator>(),
            ProjectType::JKDF2,
            {},
            {},
            "editor_preview_master.cmp" };
        std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
            nullptr,
            ui::resources::ResourceAuthoringKind::sprite,
            context));
        check(editor != nullptr, "sprite editor factory returns a dialog");
        if ( editor )
        {
            GlView* const preview = editor->findChild<GlView*>(QStringLiteral("spr.preview.gl_view"));
            check(
                preview
                    && preview->property("resource_authoring.preview_cmp").toString()
                        == QStringLiteral("editor_preview_master.cmp"),
                "sprite preview uses the current level master CMP");
            for ( const char* objectName : kSpriteControls )
            {
                check(
                    editor->findChild<QObject*>(QString::fromLatin1(objectName)) != nullptr,
                    objectName);
            }
        }
    }

    static constexpr const char* kSoundControls[] = {
        "snd.entries",
        "snd.search",
    };
    checkEditorContract(
        ui::resources::ResourceAuthoringKind::soundClass,
        QStringLiteral("Sound Class Editor"),
        kCommonControls,
        std::size(kCommonControls));
    {
        const ui::resources::ResourceAuthoringContext context{
            std::make_shared<const assets::ResourceLocator>(),
            ProjectType::JKDF2,
            {},
            {},
            "editor_preview_master.cmp" };
        std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
            nullptr,
            ui::resources::ResourceAuthoringKind::soundClass,
            context));
        check(editor != nullptr, "sound-class editor factory returns a dialog");
        if ( editor )
        {
            for ( const char* objectName : kSoundControls )
            {
                check(
                    editor->findChild<QObject*>(QString::fromLatin1(objectName)) != nullptr,
                    objectName);
            }
        }
    }

    static constexpr const char* kStringControls[] = {
        "uni.search",
        "uni.entries",
        "uni.add",
        "uni.remove",
    };
    checkEditorContract(
        ui::resources::ResourceAuthoringKind::stringTable,
        QStringLiteral("String Table Editor"),
        kCommonControls,
        std::size(kCommonControls));
    {
        const ui::resources::ResourceAuthoringContext context{
            std::make_shared<const assets::ResourceLocator>(),
            ProjectType::JKDF2,
            {},
            {},
            "editor_preview_master.cmp" };
        std::unique_ptr<QDialog> editor(ui::resources::createResourceAuthoringEditor(
            nullptr,
            ui::resources::ResourceAuthoringKind::stringTable,
            context));
        check(editor != nullptr, "string-table editor factory returns a dialog");
        if ( editor )
        {
            for ( const char* objectName : kStringControls )
            {
                check(
                    editor->findChild<QObject*>(QString::fromLatin1(objectName)) != nullptr,
                    objectName);
            }
        }
    }

    MainWindow mainWindow(nullptr, true);
    mainWindow.setProjectTypeForTest(ProjectType::JKDF2);
    static constexpr const char* kResourceMenuActions[] = {
        "edit.resources.sprite_spr",
        "edit.resources.sound_class_snd",
        "edit.resources.string_table_uni",
    };
    for ( const char* objectName : kResourceMenuActions )
    {
        check(
            mainWindow.findChild<QAction*>(QString::fromLatin1(objectName)) != nullptr,
            objectName);
    }

    struct ResourceTabContract
    {
        ui::resources::ResourceAuthoringKind kind{};
        const char* tabs{};
        const char* fileName{};
    };
    const std::array<ResourceTabContract, 3> tabContracts{{
        { ui::resources::ResourceAuthoringKind::sprite, "resourceEditorTabs", "new_sprite.spr" },
        { ui::resources::ResourceAuthoringKind::soundClass, "soundClassResourceEditorTabs", "new_sound_class.snd" },
        { ui::resources::ResourceAuthoringKind::stringTable, "uniResourceEditorTabs", "cogstrings.uni" },
    }};
    for ( const ResourceTabContract& contract : tabContracts )
    {
        createResourceDraft(mainWindow, contract.kind);
        QTabWidget* const tabs = mainWindow.findChild<QTabWidget*>(QString::fromLatin1(contract.tabs));
        QWidget* const page = tabs ? tabs->currentWidget() : nullptr;
        const QCborMap draft = page ? ui::resources::captureResourceAuthoringPaneState(page) : QCborMap{};
        const QString fileName = QString::fromLatin1(contract.fileName);
        const QString draftIdentity = draft.value(QStringLiteral("draftIdentity")).toString();
        const int index = page && tabs ? tabs->indexOf(page) : -1;
        check(index >= 0 && tabs->count() == 1 && tabs->tabText(index) == fileName
                && tabs->tabToolTip(index) == fileName
                && draft.value(QStringLiteral("name")).toString() == fileName
                && draft.value(QStringLiteral("path")).toString().isEmpty()
                && !draftIdentity.isEmpty()
                && ui::resources::resourceAuthoringPaneIdentity(page) == QStringLiteral("draft:") + draftIdentity
                && ui::resources::resourceAuthoringPaneTitle(page) == fileName
                && !page->findChild<QLabel*>(QStringLiteral("resource_authoring.document")),
            "resource authoring file tabs retain the exact filename, source hover, and draft identity without a duplicate permanent label");
    }

    checkSpriteRoundTripsAndIsolatedPreview();
    checkSpriteOnlyPreviewAcrossProjectsAndVisibility();
    checkSoundAndStringRoundTrips();
    checkDirtyCancelAndOpenFailurePreserveDraft();
    checkResourceAuthoringToolbarUsesThemeIcons();
    checkFreshOpenAndRejectedCloseBehavior();
    checkInvalidSoundEditIsNotDiscarded();
    checkInvalidStringNumberEditIsNotDiscarded();
    checkSoundFlagsAcceptEitherHexPrefixCase();
    checkResourceEditorsDockAndRawDraftRecovery();
    checkIJimQuotedStringTableOpensWithSourceSpelling();
    checkResourceTabMenuRejectsStaleTabOrder();
    checkResourceTabCloseResolvesPageAfterPromptReorder();
    checkResourceTabMenuIgnoresRemovedContextForPin();
}

void runResourceWorkspaceRegressionTests()
{
    std::cerr << "[resource-workspace] dock and raw EDIT draft: start\n";
    checkResourceEditorsDockAndRawDraftRecovery();
    std::cerr << "[resource-workspace] dock and raw EDIT draft: done\n";

    std::cerr << "[resource-workspace] IJIM quoted UNI source: start\n";
    checkIJimQuotedStringTableOpensWithSourceSpelling();
    std::cerr << "[resource-workspace] IJIM quoted UNI source: done\n";

    std::cerr << "[resource-workspace] failed resource Save routing: start\n";
    checkFailedResourceSaveConsumesNativeSave();
    std::cerr << "[resource-workspace] failed resource Save routing: done\n";

    std::cerr << "[resource-workspace] live delegate capture: start\n";
    checkResourceWorkspaceCapturesLiveTableDelegate();
    std::cerr << "[resource-workspace] live delegate capture: done\n";

    std::cerr << "[resource-workspace] live delegate Save: start\n";
    checkResourceWorkspaceSavesLiveTableDelegate();
    std::cerr << "[resource-workspace] live delegate Save: done\n";

    std::cerr << "[resource-workspace] closed delegate isolation: start\n";
    checkResourceWorkspaceIgnoresClosedTableDelegate();
    std::cerr << "[resource-workspace] closed delegate isolation: done\n";

    std::cerr << "[resource-workspace] SPR profile and integer restore: start\n";
    checkResourceWorkspaceSpriteProfileAndIntegerRestore();
    std::cerr << "[resource-workspace] SPR profile and integer restore: done\n";

    std::cerr << "[resource-workspace] active SPR profile on Open: start\n";
    checkResourceWorkspaceActiveSpriteProfileOnOpen();
    std::cerr << "[resource-workspace] active SPR profile on Open: done\n";

    std::cerr << "[resource-workspace] SPR presentation round trip: start\n";
    checkResourceWorkspaceSpritePresentationRoundTrip();
    std::cerr << "[resource-workspace] SPR presentation round trip: done\n";

    std::cerr << "[resource-workspace] raw game bytes and tiny SPR floats: start\n";
    checkResourceWorkspacePreservesRawGameBytesAndTinySpriteFloats();
    std::cerr << "[resource-workspace] raw game bytes and tiny SPR floats: done\n";

    std::cerr << "[resource-workspace] SND groups and project profiles: start\n";
    checkResourceWorkspaceSoundGroupsAndProjectProfiles();
    std::cerr << "[resource-workspace] SND groups and project profiles: done\n";

    std::cerr << "[resource-workspace] hierarchical SND and IJIM metadata: start\n";
    checkResourceWorkspaceHierarchicalSoundAndIjimMetadataPresentation();
    std::cerr << "[resource-workspace] hierarchical SND and IJIM metadata: done\n";

    std::cerr << "[resource-workspace] SND structural undo and redo: start\n";
    checkResourceWorkspaceSoundStructuralUndoRedo();
    checkSoundSearchSurvivesStructuralHistory();
    checkSoundModalAndSearchLifetimes();
    checkSoundModePresentationAndEmptyGroups();
    checkHiddenUniMetadataAndExactExport();
    checkEngineNameExportFailureIsAtomic();
    checkClassicPendingExtraLightScalarSave();
    checkSpriteValidationStatusSurvivesPendingPreview();
    checkResourceValidationModalKeepsCurrentOwnerState();
    checkNamedResourceReplacementPrompts();
    checkUniClearSearchRestoresCurrentRow();
    checkSoundActualHeaderAndActiveVolumePainting();
    checkSoundBulkIdentityFocusAndHoverGlyphs();
    checkSoundModePopupPolicyAndLocalHistory();
    checkSoundPickerPlaybackRequiresUserIntent();
    checkSoundLivePreviewVolumeLease();
    checkSoundPlayingControlsPersistAcrossPointerExit();
    checkSoundAddModeClearsBlockingSearch();
    checkSoundEntryInsertionAndVolumeClipping();
    checkSoundWavPickerProjectCategoriesAndAll();
    checkSoundModeHeadersAndInlineVolume();
    checkSoundAddModeFiltersExistingModes();
    checkSoundRadiusScrubCommitsOnRelease();
    checkSoundProjectFlagPopups();
    std::cerr << "[resource-workspace] SND structural undo and redo: done\n";

    std::cerr << "[resource-workspace] zero sprite dimensions: start\n";
    checkSpriteZeroDimensionsRemainDraftOnly();
    std::cerr << "[resource-workspace] zero sprite dimensions: done\n";

    std::cerr << "[resource-workspace] staged color popup: start\n";
    checkResourceWorkspaceColorPopupStagesAuthoredValues();
    std::cerr << "[resource-workspace] staged color popup: done\n";

    std::cerr << "[resource-workspace] Sprite popup owner deletion: start\n";
    checkSpritePopupCommitCanDeleteOwner();
    std::cerr << "[resource-workspace] Sprite popup owner deletion: done\n";

    std::cerr << "[resource-workspace] resource recovery and native baseline: start\n";
    checkResourceWorkspaceRecoveryAndNativeSaveBaselines();
    std::cerr << "[resource-workspace] resource recovery and native baseline: done\n";

    std::cerr << "[resource-workspace] presentation does not dirty resource: start\n";
    checkResourceWorkspacePresentationDoesNotDirtyCleanResource();
    std::cerr << "[resource-workspace] presentation does not dirty resource: done\n";

    std::cerr << "[resource-workspace] SPR pending numeric draft: start\n";
    checkResourceWorkspaceSpritePendingNumericDraftRoundTrip();
    checkSpritePendingUndoAndSourceReplacement();
    checkFocusedSpritePaneDestruction();
    std::cerr << "[resource-workspace] SPR pending numeric draft: done\n";

    std::cerr << "[resource-workspace] SND loop presentation: start\n";
    checkResourceWorkspaceSoundLoopPresentationRoundTrip();
    std::cerr << "[resource-workspace] SND loop presentation: done\n";

    std::cerr << "[resource-workspace] same-document native Save context: start\n";
    checkResourceWorkspaceContextSurvivesSameDocumentNativeSave();
    std::cerr << "[resource-workspace] same-document native Save context: done\n";

    std::cerr << "[resource-workspace] hot-path capture counts: start\n";
    checkResourceWorkspaceRecoveryCaptureHotPath();
    std::cerr << "[resource-workspace] hot-path capture counts: done\n";

    std::cerr << "[resource-workspace] Escape and pinned tab: start\n";
    checkResourceWorkspaceEscapeRespectsPinnedTab();
    std::cerr << "[resource-workspace] Escape and pinned tab: done\n";

    std::cerr << "[resource-workspace] tab menu stale order: start\n";
    checkResourceTabMenuRejectsStaleTabOrder();
    std::cerr << "[resource-workspace] tab menu stale order: done\n";

    std::cerr << "[resource-workspace] tab close prompt reorder: start\n";
    checkResourceTabCloseResolvesPageAfterPromptReorder();
    std::cerr << "[resource-workspace] tab close prompt reorder: done\n";

    std::cerr << "[resource-workspace] removed context Pin: start\n";
    checkResourceTabMenuIgnoresRemovedContextForPin();
    std::cerr << "[resource-workspace] removed context Pin: done\n";

    checkSoundAppThemePaintAndHoverControls();
    checkSoundPresentationPolishAndHoverTransition();
    checkSoundHeaderAnimationReversesOnSecondClick();
    checkSoundInactiveVolumeMatchesEditorAtNarrowWidths();
    checkSoundVolumeCentersMarginsAndRemoveAtViewportEdge();
    checkSoundRemoveDoesNotCoverOtherColumns();
    checkSoundWholeEditorNarrowVolumeReservation();
    checkSoundClippedVolumeTooltipRetainsRawValue();
    checkSoundCompactMinimumKeepsVolumeAndRemoveVisible();

    // Keep synchronous owner deletion last: an unfixed callback UAF can abort the process.
    std::cerr << "[resource-workspace] callback owner deletion: start\n";
    checkResourcePaneChangedCallbackCanDeleteOwner();
    std::cerr << "[resource-workspace] callback owner deletion: done\n";
}

} // namespace sed::app_tests
