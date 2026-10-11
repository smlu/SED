#include "test_common.hpp"

#include "app_settings.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/sed.hpp"
#include "sed/assets/tpl.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/components/ui_decimal_format.hpp"
#include "ui/project/sed_editor_file_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"
#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/resources/resource_authoring_detail.hpp"

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QCborArray>
#include <QCryptographicHash>
#include <QDialog>
#include <QFile>
#include <QHelpEvent>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QScrollBar>
#include <QSlider>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QToolButton>
#include <QToolTip>

#include <array>
#include <filesystem>
#include <memory>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace sed
{
std::vector<ThingTemplate> templateEditorTemplatesForTest(QDialog* dialog);
}

namespace sed::app_tests
{
namespace
{

using ui::resources::ResourceAuthoringKind;

class ResourceNumericPolicyScope final
{
public:
    ResourceNumericPolicyScope() : previous_(ui::uiDecimalSeparator()) {}
    ~ResourceNumericPolicyScope() { ui::applyUiDecimalSeparator(previous_); }

private:
    UiDecimalSeparator previous_;
};

QByteArray readDecimalResource(const std::filesystem::path& path)
{
    QFile file(QString::fromStdWString(path.wstring()));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

void resourceDecimalPhase(const ProjectType project, const char* phase)
{
    std::cerr << "[resource decimals phase] project=" << static_cast<int>(project)
        << " phase=" << phase << std::endl;
}

std::unique_ptr<QWidget> decimalResourcePane(const ResourceAuthoringKind kind,
    const ProjectType project, const std::filesystem::path& path, const QByteArray& source)
{
    QFile file(QString::fromStdWString(path.wstring()));
    const bool written = file.open(QIODevice::WriteOnly) && file.write(source) == source.size();
    file.close();
    check(written, "resource decimals: isolated literal source is written");
    if ( !written ) return {};

    ui::resources::ResourceAuthoringContext context{};
    context.projectType = project;
    context.projectDirectory = path.parent_path();
    context.startingDirectory = path.parent_path();
    context.resources = std::make_shared<const assets::ResourceLocator>();
    resourceDecimalPhase(project, kind == ResourceAuthoringKind::sprite ? "create Sprite pane" : "create Sound pane");
    std::unique_ptr<QWidget> pane(ui::resources::createResourceAuthoringPane(nullptr, kind, context));
    if ( !pane ) return {};
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane.get());
    state.remove(QStringLiteral("history"));
    state.remove(QStringLiteral("presentation"));
    state.remove(QStringLiteral("editorPresentation"));
    state.insert(QStringLiteral("sourceKind"), 1); // Existing loose source; Save requires no chooser.
    state.insert(QStringLiteral("path"), QString::fromStdWString(path.wstring()));
    state.insert(QStringLiteral("name"), QString::fromStdWString(path.filename().wstring()));
    state.insert(QStringLiteral("draftIdentity"), QString{});
    state.insert(QStringLiteral("projectResource"), QString{});
    state.insert(QStringLiteral("diskHash"), QCryptographicHash::hash(source, QCryptographicHash::Sha256));
    state.insert(QStringLiteral("modified"), false);
    if ( kind == ResourceAuthoringKind::sprite )
    {
        state.insert(QStringLiteral("sprite"), QCborArray{
            QStringLiteral("decimal.mat"), 0, QStringLiteral("3.25"), QStringLiteral("4.5"), 4, 0, 0,
            QStringLiteral("0.125"), QStringLiteral("0.125"),
            project == ProjectType::IJIM ? QStringLiteral("0.25") : QStringLiteral("0.125"),
            project == ProjectType::IJIM ? QStringLiteral("0.375") : QStringLiteral("0.125"),
            project == ProjectType::IJIM ? QStringLiteral("0.5") : QStringLiteral("1"),
            QStringLiteral("8.25"), QStringLiteral("-9.5"), QStringLiteral("10.75") });
    }
    else
    {
        QCborArray rows{};
        QCborArray loops{};
        for ( qsizetype index = 0; index < source.count('\n'); ++index )
        {
            rows.append(QCborArray{ QStringLiteral("moving"), QStringLiteral("decimal.wav"),
                QStringLiteral("0x80000040"), QStringLiteral("0.125"),
                QStringLiteral("2.5"), QStringLiteral("0.75") });
            loops.append(false);
        }
        state.insert(QStringLiteral("rows"), rows);
        state.insert(QStringLiteral("soundPreviewLoops"), loops);
        state.insert(QStringLiteral("collapsedModes"), QCborArray{});
    }
    QString warning{};
    resourceDecimalPhase(project, "restore literal pane baseline");
    const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane.get(), state, &warning);
    resourceDecimalPhase(project, "literal pane baseline restored");
    check(restored && warning.isEmpty(), "resource decimals: real pane restores a complete clean literal baseline");
    return restored ? std::move(pane) : std::unique_ptr<QWidget>{};
}

QCborMap resourceNumericContent(QWidget* pane)
{
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(pane);
    QCborMap content{};
    for ( const QString& key : { QStringLiteral("sprite"), QStringLiteral("spritePendingNumericEdits"),
        QStringLiteral("rows"), QStringLiteral("history"), QStringLiteral("modified") } )
    {
        if ( captured.contains(key) ) content.insert(key, captured.value(key));
    }
    return content;
}

QCborMap nativeDecimalRoundTrip(const QCborMap& state, const ProjectType project)
{
    Level level{};
    level.kind = project;
    level.header.version = project == ProjectType::IJIM ? 3 : 1;
    level.layers = { "Base" };
    const QByteArray payload = ui::recovery::serializeCanonicalCbor(QCborValue(state));
    assets::SedOptionalSection section{};
    section.type = ui::project::SedEditorStateSection;
    const auto* bytes = reinterpret_cast<const std::byte*>(payload.constData());
    section.payload.assign(bytes, bytes + payload.size());
    const std::array<assets::SedOptionalSection, 1> sections{ section };
    const auto encoded = assets::saveToSedBytes(level, sections,
        assets::SedSerializationProfile::ProtectedV1, 1725000000123ll);
    const assets::SedFile decoded = assets::loadSedFileBytes(encoded);
    check(decoded.optionalSections.size() == 1, "resource decimals: native SED retains the exact raw resource section");
    if ( decoded.optionalSections.size() != 1 ) return {};
    const auto& restored = decoded.optionalSections.front().payload;
    return QCborValue::fromCbor(QByteArray(reinterpret_cast<const char*>(restored.data()),
        static_cast<qsizetype>(restored.size()))).toMap();
}

bool saveDecimalPane(QWidget* pane, const ProjectType project, const char* phase)
{
    resourceDecimalPhase(project, phase);
    const QPointer<QWidget> guardedPane(pane);
    int unexpectedDialogs = 0;
    QTimer guard{};
    QObject::connect(&guard, &QTimer::timeout, [&]()
    {
        QPointer<QMessageBox> message(qobject_cast<QMessageBox*>(QApplication::activeModalWidget()));
        if ( !message )
        {
            // A native window can be visible without becoming the active
            // window. Close this isolated process's visible message boxes too.
            for ( QWidget* const widget : QApplication::topLevelWidgets() )
            {
                auto* const candidate = qobject_cast<QMessageBox*>(widget);
                if ( candidate && candidate->isVisible() )
                {
                    message = candidate;
                    break;
                }
            }
        }
        if ( message && message->isVisible() )
        {
            ++unexpectedDialogs;
            std::cerr << "[resource decimals modal] project=" << static_cast<int>(project)
                << " phase=" << phase << " object=" << message->objectName().toStdString()
                << " title=" << message->windowTitle().toStdString()
                << " paneAlive=" << !guardedPane.isNull() << std::endl;
            message->reject();
        }
    });
    guard.start(0);
    const bool saved = ui::resources::resourceAuthoringPaneSave(pane, false);
    guard.stop();
    std::cerr << "[resource decimals Save completed] project=" << static_cast<int>(project)
        << " phase=" << phase << " saved=" << saved << " dialogs=" << unexpectedDialogs
        << " paneAlive=" << !guardedPane.isNull() << std::endl;
    return guardedPane && saved && unexpectedDialogs == 0;
}

void typeResourceNumber(QLineEdit& edit, const QString& text, const bool commit)
{
    edit.setText(text);
    edit.setModified(true);
    QMetaObject::invokeMethod(&edit, "textEdited", Qt::DirectConnection, Q_ARG(QString, text));
    if ( commit ) QMetaObject::invokeMethod(&edit, "editingFinished", Qt::DirectConnection);
}

struct ResourceEscapeReceipt
{
    bool paneAlive{};
    int prompts{};
    int rejected{};
    int finished{};
};

ResourceEscapeReceipt sendResourceNumericEscape(QWidget* pane, QLineEdit* line,
    const ProjectType project, const char* phase)
{
    resourceDecimalPhase(project, phase);
    const QPointer<QWidget> guardedPane(pane);
    ResourceEscapeReceipt receipt{};
    QTimer guard{};
    if ( auto* const dialog = qobject_cast<QDialog*>(pane) )
    {
        QObject::connect(dialog, &QDialog::rejected, &guard, [&]() { ++receipt.rejected; });
        QObject::connect(dialog, &QDialog::finished, &guard, [&](int) { ++receipt.finished; });
    }
    // The guard runs inside any unexpected modal loop reached by synchronous
    // dispatch. Reject the prompt and fail ownership rather than hang the test.
    QObject::connect(&guard, &QTimer::timeout, [&]()
    {
        for ( QWidget* const widget : QApplication::topLevelWidgets() )
        {
            auto* const message = qobject_cast<QMessageBox*>(widget);
            if ( message && message->isVisible() )
            {
                ++receipt.prompts;
                std::cerr << "[resource decimals Escape modal] project=" << static_cast<int>(project)
                    << " phase=" << phase << " object=" << message->objectName().toStdString()
                    << " title=" << message->windowTitle().toStdString() << std::endl;
                message->reject();
            }
        }
    });
    guard.start(0);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(line, &escape);
    QApplication::processEvents();
    guard.stop();
    receipt.paneAlive = !guardedPane.isNull();
    return receipt;
}

void checkSpriteResourceDecimals(const ProjectType project, const std::filesystem::path& root)
{
    resourceDecimalPhase(project, "Sprite project begin");
    const bool ijim = project == ProjectType::IJIM;
    const QByteArray source = ijim
        ? QByteArrayLiteral("decimal.mat 0 3.25 4.5 4 0 0 0.125 0.25 0.375 0.5 8.25 -9.5 10.75\n")
        : QByteArrayLiteral("decimal.mat 0 3.25 4.5 4 0 0 0.125 8.25 -9.5 10.75\n");
    const auto path = root / ("decimal-" + std::to_string(static_cast<int>(project)) + ".spr");
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    auto pane = decimalResourcePane(ResourceAuthoringKind::sprite, project, path, source);
    check(pane != nullptr, "resource decimals: actual Sprite pane exists");
    if ( !pane ) return;
    const std::array<const char*, 6> names{
        "spr.width", "spr.height", "spr.extra_light.scalar", "spr.offset.x", "spr.offset.y", "spr.offset.z" };
    const std::array<double, 6> values{ 3.25, 4.5, 0.125, 8.25, -9.5, 10.75 };
    std::array<ui::ScrubFloatEdit*, 6> controls{};
    for ( std::size_t index = 0; index < names.size(); ++index )
    {
        controls[index] = pane->findChild<ui::ScrubFloatEdit*>(QString::fromLatin1(names[index]));
        check(controls[index] != nullptr, "resource decimals: real Sprite dimension/light/offset control exists");
        if ( !controls[index] ) return;
    }
    const QCborMap clean = resourceNumericContent(pane.get());
    for ( const auto separator : { UiDecimalSeparator::comma, UiDecimalSeparator::dot, UiDecimalSeparator::system } )
    {
        ui::resources::resetResourceAuthoringPaneTestCounters(pane.get());
        ui::applyUiDecimalSeparator(separator);
        const QString punctuation = separator == UiDecimalSeparator::comma ? QStringLiteral(",")
            : separator == UiDecimalSeparator::dot ? QStringLiteral(".") : QLocale::system().decimalPoint();
        bool correct = true;
        for ( std::size_t index = 0; index < controls.size(); ++index )
        {
            const QString text = controls[index]->lineEdit()->text();
            correct = correct && text.contains(punctuation) && controls[index]->value() == values[index];
        }
        check(correct, "resource decimals: Dot Comma System refresh actual Sprite dimensions light and offsets");
        const auto counters = ui::resources::resourceAuthoringPaneTestCounters(pane.get());
        check(counters.captureCalls == 0 && counters.rowVisits == 0 && resourceNumericContent(pane.get()) == clean
            && !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
            "resource decimals: Sprite locale refresh preserves exact accepted RGBA/model/history and clean state");
    }
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    check(saveDecimalPane(pane.get(), project, "Sprite unchanged Save") && readDecimalResource(path) == source,
        "resource decimals: actual comma Sprite Save preserves every exact dot engine-file byte");

    typeResourceNumber(*controls[0]->lineEdit(), QStringLiteral("6,125"), false);
    const QCborMap pending = resourceNumericContent(pane.get());
    const int cursor = controls[0]->lineEdit()->cursorPosition();
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    check(controls[0]->lineEdit()->text() == QStringLiteral("6,125")
        && controls[0]->lineEdit()->cursorPosition() == cursor && resourceNumericContent(pane.get()) == pending,
        "resource decimals: live preference changes preserve exact pending comma Sprite text and history");
    const QByteArray changed = ijim
        ? QByteArrayLiteral("decimal.mat 0 6.125 4.5 4 0 0 0.125 0.25 0.375 0.5 8.25 -9.5 10.75\n")
        : QByteArrayLiteral("decimal.mat 0 6.125 4.5 4 0 0 0.125 8.25 -9.5 10.75\n");
    check(saveDecimalPane(pane.get(), project, "Sprite old-locale pending Save") && readDecimalResource(path) == changed,
        "resource decimals: Sprite Save parses pending text in its original comma locale and emits exact dot bytes");
    check(ui::resources::resourceAuthoringPaneDocumentUndoRedo(pane.get(), false)
        && ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("sprite"))
            .toArray().at(2) == QCborValue(QStringLiteral("3.25")),
        "resource decimals: document Undo restores the exact original Sprite float after comma Save");
    check(ui::resources::resourceAuthoringPaneDocumentUndoRedo(pane.get(), true),
        "resource decimals: document Redo retains the committed comma Sprite command");

    const QCborValue acceptedSprite = ui::resources::captureResourceAuthoringPaneState(pane.get())
        .value(QStringLiteral("sprite"));
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    typeResourceNumber(*controls[0]->lineEdit(), QStringLiteral("7,875"), false);
    const ResourceEscapeReceipt escapedReceipt = sendResourceNumericEscape(
        pane.get(), controls[0]->lineEdit(), project, "Sprite Escape dispatch");
    check(escapedReceipt.paneAlive && escapedReceipt.prompts == 0
        && escapedReceipt.rejected == 0 && escapedReceipt.finished == 0,
        "resource decimals: pending Sprite Escape belongs to its field without prompting or closing the document");
    if ( !escapedReceipt.paneAlive )
    {
        pane.release();
        return;
    }
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    const QCborMap escaped = ui::resources::captureResourceAuthoringPaneState(pane.get());
    check(controls[0]->lineEdit()->text().contains(QLatin1Char('.')) && controls[0]->value() == 6.125
        && escaped.value(QStringLiteral("sprite")) == acceptedSprite
        && escaped.value(QStringLiteral("spritePendingNumericEdits")).isUndefined(),
        "resource decimals: Sprite Escape restores settled formatter so later Dot refresh preserves exact accepted floats");

    const QCborMap beforeScrubEscape = resourceNumericContent(pane.get());
    int scrubStarted = 0;
    int scrubCanceled = 0;
    int scrubFinished = 0;
    int valueCommitted = 0;
    QObject gestureTrace{};
    QObject::connect(controls[0], &ui::ScrubFloatEdit::scrubStarted, &gestureTrace,
        [&](double) { ++scrubStarted; });
    QObject::connect(controls[0], &ui::ScrubFloatEdit::scrubCanceled, &gestureTrace,
        [&]() { ++scrubCanceled; });
    QObject::connect(controls[0], &ui::ScrubFloatEdit::scrubFinished, &gestureTrace,
        [&](double) { ++scrubFinished; });
    QObject::connect(controls[0], &ui::ScrubFloatEdit::valueCommitted, &gestureTrace,
        [&](double) { ++valueCommitted; });
    QLineEdit* const scrubLine = controls[0]->lineEdit();
    const QPoint scrubStart = scrubLine->rect().center();
    const QPoint scrubMove = scrubStart + QPoint(40, 0);
    QMouseEvent scrubPress(QEvent::MouseButtonPress, QPointF(scrubStart),
        QPointF(scrubLine->mapToGlobal(scrubStart)), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(scrubLine, &scrubPress);
    QMouseEvent scrubMotion(QEvent::MouseMove, QPointF(scrubMove),
        QPointF(scrubLine->mapToGlobal(scrubMove)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(scrubLine, &scrubMotion);
    check(scrubStarted == 1 && controls[0]->value() != 6.125
        && resourceNumericContent(pane.get()).value(QStringLiteral("sprite")) != acceptedSprite,
        "resource decimals: Sprite Escape fixture owns a real active scrub with changed live model");
    const ResourceEscapeReceipt scrubReceipt = sendResourceNumericEscape(
        pane.get(), scrubLine, project, "Sprite active scrub Escape dispatch");
    check(scrubReceipt.paneAlive && scrubReceipt.prompts == 0
        && scrubReceipt.rejected == 0 && scrubReceipt.finished == 0
        && scrubCanceled == 1 && scrubFinished == 0 && valueCommitted == 0
        && controls[0]->value() == 6.125 && resourceNumericContent(pane.get()) == beforeScrubEscape,
        "resource decimals: Sprite scrub Escape restores exact accepted model/history and cancels once without closing");
    if ( !scrubReceipt.paneAlive )
    {
        pane.release();
        return;
    }
    const QPoint lateMove = scrubMove + QPoint(40, 0);
    QMouseEvent lateMotion(QEvent::MouseMove, QPointF(lateMove),
        QPointF(scrubLine->mapToGlobal(lateMove)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(scrubLine, &lateMotion);
    QMouseEvent scrubRelease(QEvent::MouseButtonRelease, QPointF(lateMove),
        QPointF(scrubLine->mapToGlobal(lateMove)), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(scrubLine, &scrubRelease);
    check(scrubCanceled == 1 && scrubFinished == 0 && valueCommitted == 0
        && controls[0]->value() == 6.125 && resourceNumericContent(pane.get()) == beforeScrubEscape,
        "resource decimals: late Sprite scrub movement and release cannot resume the canceled gesture");

    QToolButton* increase = nullptr;
    for ( QToolButton* const button : controls[0]->findChildren<QToolButton*>() )
    {
        if ( button->text() == QStringLiteral(">") ) increase = button;
    }
    check(increase != nullptr, "resource decimals: Sprite stepper Escape fixture has the real increase control");
    if ( increase )
    {
        // Press the real signal while held; no auto-repeat delay is necessary.
        increase->setDown(true);
        QMetaObject::invokeMethod(increase, "pressed", Qt::DirectConnection);
        check(scrubStarted == 2 && controls[0]->value() != 6.125,
            "resource decimals: Sprite Escape fixture owns a real active stepper gesture");
        const ResourceEscapeReceipt stepperReceipt = sendResourceNumericEscape(
            pane.get(), scrubLine, project, "Sprite active stepper Escape dispatch");
        check(stepperReceipt.paneAlive && stepperReceipt.prompts == 0
            && stepperReceipt.rejected == 0 && stepperReceipt.finished == 0
            && scrubCanceled == 2 && scrubFinished == 0 && valueCommitted == 0
            && !increase->isDown() && controls[0]->value() == 6.125
            && resourceNumericContent(pane.get()) == beforeScrubEscape,
            "resource decimals: Sprite stepper Escape releases held button and restores exact model/history");
        if ( !stepperReceipt.paneAlive )
        {
            pane.release();
            return;
        }
        QMetaObject::invokeMethod(increase, "released", Qt::DirectConnection);
        check(scrubCanceled == 2 && scrubFinished == 0 && valueCommitted == 0
            && controls[0]->value() == 6.125 && resourceNumericContent(pane.get()) == beforeScrubEscape,
            "resource decimals: eventual Sprite stepper release cannot commit after Escape");
    }

    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    typeResourceNumber(*controls[0]->lineEdit(), QStringLiteral("-0,"), false);
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap transported = nativeDecimalRoundTrip(captured, project);
    auto restored = decimalResourcePane(ResourceAuthoringKind::sprite, project,
        root / ("restored-" + std::to_string(static_cast<int>(project)) + ".spr"), changed);
    QString warning{};
    resourceDecimalPhase(project, "Sprite native pending restore");
    const bool restoredState = restored
        && ui::resources::restoreResourceAuthoringPaneState(restored.get(), transported, &warning);
    resourceDecimalPhase(project, "Sprite native pending restore completed");
    const QCborMap restoredContent = restoredState ? resourceNumericContent(restored.get()) : QCborMap{};
    const QCborMap expectedContent = resourceNumericContent(pane.get());
    const bool exactContent = restoredContent == expectedContent;
    const bool exactCanonicalContent = ui::recovery::serializeCanonicalCbor(QCborValue(restoredContent))
        == ui::recovery::serializeCanonicalCbor(QCborValue(expectedContent));
    if ( !restoredState || !warning.isEmpty() || !exactCanonicalContent )
    {
        std::cerr << "[resource decimals Sprite native] project=" << static_cast<int>(project)
            << " restore=" << restoredState << " warning=" << warning.toStdString()
            << " mapEqual=" << exactContent << " canonicalEqual=" << exactCanonicalContent
            << " spriteEqual=" << (restoredContent.value(QStringLiteral("sprite"))
                == expectedContent.value(QStringLiteral("sprite")))
            << " pendingEqual=" << (restoredContent.value(QStringLiteral("spritePendingNumericEdits"))
                == expectedContent.value(QStringLiteral("spritePendingNumericEdits")))
            << " historyCanonicalEqual=" << (ui::recovery::serializeCanonicalCbor(restoredContent.value(QStringLiteral("history")))
                == ui::recovery::serializeCanonicalCbor(expectedContent.value(QStringLiteral("history")))) << '\n';
    }
    check(restoredState && warning.isEmpty() && exactCanonicalContent
        && transported.value(QStringLiteral("spritePendingNumericEdits")).toMap()
            .value(QStringLiteral("width")) == QCborValue(QStringLiteral("-0,")),
        "resource decimals: native restore preserves invalid comma Sprite raw text plus exact accepted floats/history");
}

void checkSoundResourceDecimals(const ProjectType project, const std::filesystem::path& root)
{
    resourceDecimalPhase(project, "Sound project begin");
    const QByteArray sourceRow = QByteArrayLiteral("moving decimal.wav 0x80000040 0.125 2.5 0.75\n");
    const QByteArray source = sourceRow.repeated(48);
    const auto path = root / ("decimal-" + std::to_string(static_cast<int>(project)) + ".snd");
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    auto pane = decimalResourcePane(ResourceAuthoringKind::soundClass, project, path, source);
    auto* tree = pane ? pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    const auto findFirstAlternative = [tree]() -> QTreeWidgetItem*
    {
        for ( int group = 0; tree && group < tree->topLevelItemCount(); ++group )
        {
            auto* mode = tree->topLevelItem(group);
            for ( int child = 0; mode && child < mode->childCount(); ++child )
            {
                auto* item = mode->child(child);
                if ( item->data(0, ui::resources::detail::kSoundTreeItemKindRole).toInt()
                        == static_cast<int>(ui::resources::detail::SoundTreeItemKind::alternative)
                    && item->text(1) == QStringLiteral("decimal.wav")
                    && item->data(0, ui::resources::detail::kSoundTreeSourceIndexRole).toInt() == 0 )
                {
                    return item;
                }
            }
        }
        return nullptr;
    };
    QTreeWidgetItem* first = findFirstAlternative();
    check(first && first->text(3) == QStringLiteral("0.125") && first->text(4) == QStringLiteral("2.5")
        && first->text(5) == QStringLiteral("0.75"),
        "resource decimals: real Sound Class Alternative role/source row has its exact authored numeric fields");
    if ( !first ) return;
    tree->topLevelItem(0)->setExpanded(true);
    pane->show();
    QApplication::processEvents();
    tree->verticalScrollBar()->setValue(tree->verticalScrollBar()->maximum());
    const QPersistentModelIndex firstIndex(tree->indexFromItem(first, 3));
    const QCborMap clean = resourceNumericContent(pane.get());
    const int scroll = tree->verticalScrollBar()->value();
    check(scroll > 0, "resource decimals: Sound refresh fixture retains a meaningful nonzero scroll position");
    for ( const auto separator : { UiDecimalSeparator::comma, UiDecimalSeparator::dot, UiDecimalSeparator::system } )
    {
        ui::resources::resetResourceAuthoringPaneTestCounters(pane.get());
        ui::applyUiDecimalSeparator(separator);
        const QString punctuation = separator == UiDecimalSeparator::comma ? QStringLiteral(",")
            : separator == UiDecimalSeparator::dot ? QStringLiteral(".") : QLocale::system().decimalPoint();
        bool correct = true;
        for ( int column = 3; column <= 5; ++column )
        {
            const QModelIndex index = tree->indexFromItem(first, column);
            auto* delegate = qobject_cast<QStyledItemDelegate*>(tree->itemDelegateForColumn(column));
            const QString displayed = delegate
                ? delegate->displayText(index.data(Qt::DisplayRole), tree->locale()) : QString{};
            correct = correct && displayed.contains(punctuation);
        }
        check(correct, "resource decimals: actual Sound radius and volume delegates display Dot Comma System");
        bool correctHelp = true;
        for ( const int column : {3, 4} )
        {
            const QModelIndex index = tree->indexFromItem(first, column);
            auto* delegate = qobject_cast<QStyledItemDelegate*>(tree->itemDelegateForColumn(column));
            QStyleOptionViewItem option{};
            option.initFrom(tree);
            option.rect = tree->viewport()->rect();
            QHelpEvent help(QEvent::ToolTip, option.rect.center(),
                tree->viewport()->mapToGlobal(option.rect.center()));
            correctHelp = correctHelp && delegate && delegate->helpEvent(&help, tree, option, index)
                && QToolTip::text().contains(ui::formatUiNumber(0.1) + QStringLiteral(" world unit = 1 m."));
            QToolTip::hideText();
        }
        check(correctHelp, "resource decimals: actual Sound radius help follows Dot Comma System without changing raw rows");
        const auto counters = ui::resources::resourceAuthoringPaneTestCounters(pane.get());
        check(counters.captureCalls == 0 && counters.rowVisits == 0 && firstIndex.isValid()
            && tree->itemFromIndex(firstIndex) == first && tree->verticalScrollBar()->value() == scroll
            && resourceNumericContent(pane.get()) == clean && !ui::resources::resourceAuthoringPaneIsModified(pane.get()),
            "resource decimals: Sound locale refresh retains tree identity scroll exact rows/history and clean state");
    }
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    check(saveDecimalPane(pane.get(), project, "Sound unchanged Save") && readDecimalResource(path) == source,
        "resource decimals: unchanged comma Sound Save preserves exact dot engine-file bytes");
    tree->scrollToItem(first);
    tree->editItem(first, 3);
    resourceDecimalPhase(project, "Sound radius editor opening");
    QApplication::processEvents();
    auto* radius = tree->findChild<ui::ScrubFloatEdit*>(QStringLiteral("snd.numeric.editor"));
    check(radius && radius->lineEdit()->text().contains(QLatin1Char(',')),
        "resource decimals: actual Sound radius editor opens with comma display");
    if ( !radius ) return;
    typeResourceNumber(*radius->lineEdit(), QStringLiteral("0,625"), true);
    resourceDecimalPhase(project, "Sound radius commit processing");
    QApplication::processEvents();
    const QCborMap radiusState = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborArray models = radiusState.value(QStringLiteral("history")).toMap()
        .value(QStringLiteral("models")).toArray();
    check(!models.isEmpty() && models.at(0).toArray().at(3) == QCborValue(QStringLiteral("0.625")),
        "resource decimals: Sound comma radius commit retains exact dot accepted history model");
    tree->editItem(first, 5);
    resourceDecimalPhase(project, "Sound volume editor opening");
    QApplication::processEvents();
    const QPersistentModelIndex volumeIndex(tree->indexFromItem(first, 5));
    QPointer<QLineEdit> volume(tree->findChild<QLineEdit*>(QStringLiteral("snd.volume.text")));
    check(volume && volume->text() == QStringLiteral("0,75"),
        "resource decimals: real Sound volume editor opens with localized shortest-float text");
    if ( !volume ) return;
    auto* volumeSlider = volume->parentWidget()->findChild<QSlider*>(QStringLiteral("snd.volume.slider"));
    check(volumeSlider != nullptr, "resource decimals: pending volume tooltip fixture uses the actual delegate slider");
    if ( volumeSlider )
    {
        volumeSlider->setValue(875);
        check(volume->text() == QStringLiteral("0,875") && volume->toolTip() == volume->text(),
            "resource decimals: Sound volume tooltip follows the actual comma slider value");
    }
    typeResourceNumber(*volume.data(), QStringLiteral("0,8750"), false);
    check(volume->toolTip() == QStringLiteral("0,8750"),
        "resource decimals: Sound volume tooltip follows exact pending typed numeric text");
    typeResourceNumber(*volume.data(), QStringLiteral("0,875"), false);
    const QCborMap pending = resourceNumericContent(pane.get());
    check(ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows"))
        .toArray().at(0).toArray().at(5) == QCborValue(QStringLiteral("0,875")),
        "resource decimals: active Sound volume delegate captures exact pending comma text");
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap transported = nativeDecimalRoundTrip(captured, project);
    // Exercise the original live editor before another pane opens a modal
    // validation dialog, which legitimately closes transient table editors.
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    const bool volumeStillOwned = volume && volumeIndex.isValid()
        && tree->indexWidget(volumeIndex) == volume->parentWidget();
    if ( !volumeStillOwned )
    {
        std::cerr << "[resource decimals Sound editor] project=" << static_cast<int>(project)
            << " live=" << !volume.isNull() << " indexValid=" << volumeIndex.isValid()
            << " currentEditor=" << (volume && tree->indexWidget(volumeIndex) == volume->parentWidget()) << '\n';
    }
    check(volumeStillOwned && volume->text() == QStringLiteral("0,875")
        && resourceNumericContent(pane.get()) == pending,
        "resource decimals: live preference change preserves pending Sound input and virtual Undo command");
    if ( !volumeStillOwned ) return;
    QMetaObject::invokeMethod(volume.data(), "editingFinished", Qt::DirectConnection);
    check(saveDecimalPane(pane.get(), project, "Sound old-locale live pending Save") && readDecimalResource(path)
        == QByteArrayLiteral("moving decimal.wav 0x80000040 0.625 2.5 0.875\n") + sourceRow.repeated(47),
        "resource decimals: real Sound comma volume commit parses original locale and emits exact dot engine bytes");
    check(ui::resources::resourceAuthoringPaneDocumentUndoRedo(pane.get(), false)
        && ui::resources::captureResourceAuthoringPaneState(pane.get()).value(QStringLiteral("rows"))
            .toArray().at(0).toArray().at(5) == QCborValue(QStringLiteral("0.75")),
        "resource decimals: Sound document Undo restores exact original source row after comma edit");
    check(ui::resources::resourceAuthoringPaneDocumentUndoRedo(pane.get(), true),
        "resource decimals: Sound document Redo retains the accepted comma edit");

    first = findFirstAlternative();
    check(first != nullptr, "resource decimals: Sound Undo/Redo restored the actual Alternative for invalid raw presentation");
    if ( !first ) return;
    first->setText(3, QStringLiteral("1.2.3"));
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    const QModelIndex invalidIndex = tree->indexFromItem(first, 3);
    auto* invalidDelegate = qobject_cast<QStyledItemDelegate*>(tree->itemDelegateForColumn(3));
    check(invalidDelegate && invalidDelegate->displayText(invalidIndex.data(Qt::DisplayRole), tree->locale())
        == QStringLiteral("1.2.3") && ui::resources::captureResourceAuthoringPaneState(pane.get())
            .value(QStringLiteral("rows")).toArray().at(0).toArray().at(3) == QCborValue(QStringLiteral("1.2.3")),
        "resource decimals: invalid Sound numeric raw draft paints and captures verbatim under Comma");

    auto restored = decimalResourcePane(ResourceAuthoringKind::soundClass, project,
        root / ("restored-" + std::to_string(static_cast<int>(project)) + ".snd"), source);
    QString warning{};
    resourceDecimalPhase(project, "Sound native pending restore");
    const bool restoredState = restored
        && ui::resources::restoreResourceAuthoringPaneState(restored.get(), transported, &warning);
    resourceDecimalPhase(project, "Sound native pending restore completed");
    const QCborMap restoredContent = restoredState ? resourceNumericContent(restored.get()) : QCborMap{};
    if ( !restoredState || !warning.isEmpty() || restoredContent != pending )
    {
        std::cerr << "[resource decimals Sound native] project=" << static_cast<int>(project)
            << " restore=" << restoredState << " warning=" << warning.toStdString()
            << " canonicalEqual=" << (ui::recovery::serializeCanonicalCbor(QCborValue(restoredContent))
                == ui::recovery::serializeCanonicalCbor(QCborValue(pending)))
            << " rowsEqual=" << (restoredContent.value(QStringLiteral("rows"))
                == pending.value(QStringLiteral("rows"))) << '\n';
    }
    check(ui::recovery::serializeCanonicalCbor(QCborValue(transported))
            == ui::recovery::serializeCanonicalCbor(QCborValue(captured))
        && restoredState && warning.isEmpty() && restoredContent == pending,
        "resource decimals: native Sound restore preserves exact pending comma row and accepted dot history model");

    const auto savePath = root / ("restored-save-" + std::to_string(static_cast<int>(project)) + ".snd");
    auto restoredSave = decimalResourcePane(ResourceAuthoringKind::soundClass, project, savePath, source);
    QCborMap saveState = transported;
    saveState.insert(QStringLiteral("path"), QString::fromStdWString(savePath.wstring()));
    saveState.insert(QStringLiteral("name"), QString::fromStdWString(savePath.filename().wstring()));
    QString saveWarning{};
    resourceDecimalPhase(project, "Sound copied pending Save fixture restore");
    const bool saveRestored = restoredSave && ui::resources::restoreResourceAuthoringPaneState(
        restoredSave.get(), saveState, &saveWarning);
    resourceDecimalPhase(project, "Sound copied pending Save fixture restored");
    check(saveRestored && saveWarning.isEmpty(),
        "resource decimals: exact restored Sound pending row is isolated at a real copied source for Save");
    if ( saveRestored )
    {
        ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
        const auto beforeRefusal = ui::recovery::serializeCanonicalCbor(
            QCborValue(ui::resources::captureResourceAuthoringPaneState(restoredSave.get())));
        check(!saveDecimalPane(restoredSave.get(), project, "Sound expected Dot refusal") && readDecimalResource(savePath) == source
            && beforeRefusal == ui::recovery::serializeCanonicalCbor(
                QCborValue(ui::resources::captureResourceAuthoringPaneState(restoredSave.get()))),
            "resource decimals: restored comma Sound draft refused under Dot preserves full raw/history state and original bytes");
        ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
        const bool saved = saveDecimalPane(restoredSave.get(), project, "Sound restored Comma pending Save");
        const QCborMap afterSave = ui::resources::captureResourceAuthoringPaneState(restoredSave.get());
        check(saved && readDecimalResource(savePath)
                == QByteArrayLiteral("moving decimal.wav 0x80000040 0.625 2.5 0.875\n") + sourceRow.repeated(47)
            && afterSave.value(QStringLiteral("rows")).toArray().at(0).toArray().at(5)
                == QCborValue(QStringLiteral("0.875"))
            && afterSave.value(QStringLiteral("history")).toMap().value(QStringLiteral("models"))
                .toArray().at(0).toArray().at(5) == QCborValue(QStringLiteral("0.875"))
            && !ui::resources::resourceAuthoringPaneIsModified(restoredSave.get()),
            "resource decimals: restored comma Sound Save commits in current locale to exact dot bytes and accepted history model");
    }
    pane->hide();
}

std::string decimalTemplateBytes(const std::vector<ThingTemplate>& templates)
{
    assets::Templates database{};
    for ( const ThingTemplate& value : templates ) database.addOrUpdate(value);
    std::ostringstream output{};
    assets::saveTemplates(output, database);
    return output.str();
}

void checkTemplateResourceDecimals()
{
    resourceDecimalPhase(ProjectType::JKDF2, "Template MainWindow constructing");
    MainWindow window{};
    resourceDecimalPhase(ProjectType::JKDF2, "Template MainWindow constructed");
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    ThingTemplate original{};
    original.name = "decimal_template";
    original.parent = "none";
    original.bbox.Set(0, 0, 0, 0, 0, 0);
    TplValue mass{};
    mass.name = "mass";
    mass.type = ValueType::floating;
    mass.floatValue = 2.5;
    original.values.push_back(mass);
    const auto token = [&original](const char* name, const ValueType type, const char* text)
    {
        TplValue value{};
        value.name = name;
        value.type = type;
        value.stringValue = text;
        original.values.push_back(value);
    };
    token("size", ValueType::str, "3.25");
    token("lightoffset", ValueType::vect, "(0.125/-2.5/3.75)");
    TplValue numFrames{};
    numFrames.name = "numframes";
    numFrames.type = ValueType::integer;
    numFrames.intValue = 1;
    original.values.push_back(numFrames);
    token("frame", ValueType::frame, "(0.125/2.5/-3.75:4.5/5.625/6.75)");
    token("customtext", ValueType::str, "asset.1,25.mat");
    resourceDecimalPhase(ProjectType::JKDF2, "Template dialog constructing");
    std::unique_ptr<QDialog> dialog(window.createTemplateEditorDialogForTest({original}));
    resourceDecimalPhase(ProjectType::JKDF2, "Template dialog constructed");
    auto* table = dialog ? dialog->findChild<QTableWidget*>(QStringLiteral("templateParametersTable")) : nullptr;
    const auto editor = [table](const char* name) -> QLineEdit*
    {
        for ( int row = 0; table && row < table->rowCount(); ++row )
        {
            if ( table->item(row, 0) && table->item(row, 0)->text() == QString::fromLatin1(name) )
            {
                auto* cell = table->cellWidget(row, 2);
                return cell ? cell->findChild<QLineEdit*>(QStringLiteral("templateParameterInlineValueEdit")) : nullptr;
            }
        }
        return nullptr;
    };
    QPointer<QLineEdit> massEdit(editor("mass"));
    QPointer<QLineEdit> sizeEdit(editor("size"));
    check(massEdit && sizeEdit && editor("lightoffset") && editor("frame") && editor("customtext"),
        "resource decimals: real Template inline scalar tuple frame and opaque fields exist");
    if ( !massEdit || !sizeEdit || !editor("lightoffset") || !editor("frame") || !editor("customtext") ) return;
    check(massEdit->text() == QStringLiteral("2,5") && sizeEdit->text() == QStringLiteral("3,25")
        && editor("lightoffset")->text() == QStringLiteral("(0,125/-2,5/3,75)")
        && editor("frame")->text() == QStringLiteral("(0,125/2,5/-3,75:4,5/5,625/6,75)")
        && editor("customtext")->text() == QStringLiteral("asset.1,25.mat"),
        "resource decimals: actual Template numeric fields display comma while opaque text remains verbatim");
    const std::string baseline =
        "# DESC: \n# BBOX: 0 0 0 0 0 0\n"
        "decimal_template none mass=2.5 size=3.25 lightoffset=(0.125/-2.5/3.75)"
        " numframes=1 frame=(0.125/2.5/-3.75:4.5/5.625/6.75) customtext=asset.1,25.mat\n";
    check(decimalTemplateBytes(templateEditorTemplatesForTest(dialog.get())) == baseline,
        "resource decimals: comma Template presentation preserves complete literal dot engine bytes");
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    check(massEdit && sizeEdit && editor("mass") == massEdit && editor("size") == sizeEdit
        && massEdit->text() == QStringLiteral("2.5") && sizeEdit->text() == QStringLiteral("3.25")
        && decimalTemplateBytes(templateEditorTemplatesForTest(dialog.get())) == baseline,
        "resource decimals: live Template locale refresh retains editor identity and exact accepted engine data");
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    typeResourceNumber(*massEdit, QStringLiteral("7,125"), false);
    const int cursor = massEdit->cursorPosition();
    ui::applyUiDecimalSeparator(UiDecimalSeparator::dot);
    check(massEdit && massEdit->text() == QStringLiteral("7,125") && massEdit->cursorPosition() == cursor
        && decimalTemplateBytes(templateEditorTemplatesForTest(dialog.get())) == baseline,
        "resource decimals: pending Template comma input survives live Dot without committing or altering canonical data");
    resourceDecimalPhase(ProjectType::JKDF2, "Template old-locale mass commit");
    if ( massEdit ) QMetaObject::invokeMethod(massEdit, "editingFinished", Qt::DirectConnection);
    QApplication::processEvents();
    check(decimalTemplateBytes(templateEditorTemplatesForTest(dialog.get()))
        == std::string(baseline).replace(baseline.find("mass=2.5"), 8, "mass=7.125"),
        "resource decimals: Template float commits captured comma input to exact dot engine bytes");
    ui::applyUiDecimalSeparator(UiDecimalSeparator::comma);
    const auto editField = [&](const char* name, const QString& text)
    {
        resourceDecimalPhase(ProjectType::JKDF2, name);
        if ( auto* field = editor(name) ) typeResourceNumber(*field, text, true);
        QApplication::processEvents();
    };
    editField("size", QStringLiteral("8,625"));
    editField("lightoffset", QStringLiteral("(0,625/-2,875/4,125)"));
    editField("frame", QStringLiteral("(0,625/2,875/-4,125:5,25/6,375/7,5)"));
    const auto accepted = templateEditorTemplatesForTest(dialog.get());
    const std::string changed =
        "# DESC: \n# BBOX: 0 0 0 0 0 0\n"
        "decimal_template none mass=7.125 size=8.625 lightoffset=(0.625/-2.875/4.125)"
        " numframes=1 frame=(0.625/2.875/-4.125:5.25/6.375/7.5) customtext=asset.1,25.mat\n";
    const std::string acceptedBytes = decimalTemplateBytes(accepted);
    const bool exactAccepted = accepted.size() == 1 && accepted[0].values.size() == original.values.size()
        && accepted[0].values[1].type == ValueType::str && accepted[0].values[1].stringValue == "8.625"
        && accepted[0].values[3].name == "numframes" && accepted[0].values[3].type == ValueType::integer
        && accepted[0].values[3].intValue == 1
        && accepted[0].values[4].name == "frame" && accepted[0].values[4].type == ValueType::frame
        && acceptedBytes == changed;
    if ( !exactAccepted )
    {
        std::cerr << "[resource decimals Template] expected=" << changed << " actual=" << acceptedBytes;
        for ( const ThingTemplate& value : accepted )
        {
            std::cerr << " template=" << value.name << " valueCount=" << value.values.size() << '\n';
            for ( const TplValue& field : value.values )
            {
                std::cerr << " field=" << field.name << " type=" << static_cast<int>(field.type)
                    << " adjType=" << static_cast<int>(field.adjType) << " canonical=" << field.asString() << '\n';
            }
        }
        for ( const char* name : {"mass", "size", "lightoffset", "frame", "customtext"} )
        {
            if ( auto* field = editor(name) )
            {
                std::cerr << " editor=" << name << " display=" << field->text().toStdString()
                    << " baseline=" << field->property("template.numeric.displayBaseline").toString().toStdString()
                    << " canonicalBaseline=" << field->property("sed.typedNumericCanonicalText").toString().toStdString()
                    << " modified=" << field->isModified() << " inputLocale=" << field->locale().name().toStdString()
                    << " error=" << field->styleSheet().toStdString() << '\n';
            }
        }
    }
    check(exactAccepted,
        "resource decimals: Template comma scalar tuple and frame edits retain original storage types and complete canonical bytes");
    resourceDecimalPhase(ProjectType::JKDF2, "Template complete");
}

} // namespace

void runResourceUiDecimalTests()
{
    check(appSettingsIniPathIsOverriddenForTest(), "resource decimals: application settings are isolated");
    const ResourceNumericPolicyScope policyScope{};
    QTemporaryDir directory{};
    check(directory.isValid(), "resource decimals: temporary external resources are isolated");
    if ( !directory.isValid() ) return;
    const std::filesystem::path root(directory.path().toStdWString());
    for ( const auto project : { ProjectType::JKDF2, ProjectType::MOTS, ProjectType::IJIM } )
    {
        checkSpriteResourceDecimals(project, root);
        checkSoundResourceDecimals(project, root);
    }
    checkTemplateResourceDecimals();
}

} // namespace sed::app_tests
