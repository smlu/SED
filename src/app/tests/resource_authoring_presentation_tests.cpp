#include "main_window/main_window.hpp"
#include "test_common.hpp"

#include "gl_view.hpp"
#include "sed/assets/sed.hpp"
#include "sed/assets/snd.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/project/sed_editor_file_utils.hpp"
#include "ui/components/scrub_float_edit.hpp"
#include "ui/resources/resource_authoring_detail.hpp"
#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/resources/resource_authoring_pane.hpp"

#include <QApplication>
#include <QAbstractButton>
#include <QAction>
#include <QByteArray>
#include <QCborArray>
#include <QCoreApplication>
#include <QDockWidget>
#include <QDebug>
#include <QEvent>
#include <QEventLoop>
#include <QFileDialog>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QSize>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWindow>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

using ui::resources::ResourceAuthoringKind;

void initializeMainWindow(MainWindow& window, const std::filesystem::path& root);
QWidget* createMainWindowResourcePage(
    MainWindow& window,
    const QString& actionName,
    const QString& dockName,
    const QString& tabsName);
QCborMap nativeEditSectionRoundTrip(const QCborMap& state);

class SpriteGlViewEventBlocker final : public QObject
{
public:
    SpriteGlViewEventBlocker()
    {
        QCoreApplication::instance()->installEventFilter(this);
    }

    ~SpriteGlViewEventBlocker() override
    {
        QCoreApplication::instance()->removeEventFilter(this);
    }

    void watch(QWidget* const root)
    {
        if ( auto* const view = qobject_cast<GlView*>(root) )
        {
            views_.push_back(view);
        }
        if ( root )
        {
            for ( GlView* const view : root->findChildren<GlView*>() )
            {
                views_.push_back(view);
            }
        }
    }

protected:
    bool eventFilter(QObject* const watched, QEvent* const event) override
    {
        const bool graphicsEvent = event->type() == QEvent::Show
            || event->type() == QEvent::Resize || event->type() == QEvent::Paint;
        if ( graphicsEvent )
        {
            if ( qobject_cast<GlView*>(watched) )
            {
                return true;
            }
            for ( const QPointer<GlView>& view : views_ )
            {
                if ( view && (watched == view.data() || watched == view->windowHandle()) )
                {
                    // The presentation regression exercises real Sprite
                    // controls while keeping QOpenGLWidget out of the native
                    // graphics initialization path.
                    return true;
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::vector<QPointer<GlView>> views_{};
};

ui::resources::ResourceAuthoringContext testContext()
{
    ui::resources::ResourceAuthoringContext context{};
    context.projectType = ProjectType::JKDF2;
    context.resources = std::make_shared<assets::ResourceLocator>();
    return context;
}

std::unique_ptr<QWidget> makePane(const ResourceAuthoringKind kind)
{
    return std::unique_ptr<QWidget>(ui::resources::createResourceAuthoringPane(
        nullptr, kind, testContext()));
}

QCborArray soundRow(const QString& mode, const QString& source)
{
    return QCborArray{
        mode, source, QStringLiteral("0x40"), QStringLiteral("0.5"),
        QStringLiteral("2.5"), QStringLiteral("1")};
}

QCborArray stringRows(const int count, const int longKeyRow = -1)
{
    QCborArray rows{};
    for ( int row = 0; row < count; ++row )
    {
        const QString key = row == longKeyRow
            ? QStringLiteral("FILTER_%1_").arg(row, 3, 10, QLatin1Char('0'))
                + QString(260, QLatin1Char('x'))
            : QStringLiteral("FILTER_%1").arg(row, 3, 10, QLatin1Char('0'));
        rows.append(QCborArray{key, QStringLiteral("text_%1").arg(row), QString::number(row - 30)});
    }
    return rows;
}

bool seedPane(QWidget* const pane, const ResourceAuthoringKind kind)
{
    if ( !pane )
    {
        return false;
    }
    QCborMap state = ui::resources::captureResourceAuthoringPaneState(pane);
    state.remove(QStringLiteral("history"));
    state.insert(QStringLiteral("modified"), false);
    if ( kind == ResourceAuthoringKind::sprite )
    {
        QCborArray sprite = state.value(QStringLiteral("sprite")).toArray();
        if ( sprite.size() < 15 )
        {
            return false;
        }
        sprite[0] = QStringLiteral("presentation_original.mat");
        sprite[2] = QStringLiteral("12.25");
        sprite[3] = QStringLiteral("7.5");
        state.insert(QStringLiteral("sprite"), sprite);
    }
    else if ( kind == ResourceAuthoringKind::soundClass )
    {
        QCborArray rows{};
        for ( int row = 0; row < 120; ++row )
        {
            // The source order intentionally interleaves two mode groups.
            rows.append(soundRow(row % 2 == 0 ? QStringLiteral("moving") : QStringLiteral("create"),
                QStringLiteral("sound_%1.wav").arg(row)));
        }
        state.insert(QStringLiteral("rows"), rows);
        state.insert(QStringLiteral("collapsedModes"), QCborArray{});
        state.remove(QStringLiteral("soundPreviewLoops"));
    }
    else
    {
        state.insert(QStringLiteral("rows"), stringRows(120, 87));
        state.insert(QStringLiteral("capacity"), 120);
    }
    QString warning{};
    const bool restored = ui::resources::restoreResourceAuthoringPaneState(pane, state, &warning);
    if ( !restored )
    {
        qWarning().noquote() << "Could not seed resource presentation test pane:" << warning;
    }
    return restored;
}

void showOffscreen(QWidget* const pane, SpriteGlViewEventBlocker& blocker, const QSize size)
{
    blocker.watch(pane);
    pane->setAttribute(Qt::WA_DontShowOnScreen);
    pane->resize(size);
    pane->show();
    QApplication::processEvents(QEventLoop::AllEvents);
}

int sendVerticalScrollWheel(QScrollBar* const scrollBar, const int angleDelta)
{
    if ( !scrollBar )
    {
        return 0;
    }
    const QPointF position(scrollBar->rect().center());
    QWheelEvent event(position, scrollBar->mapToGlobal(position.toPoint()), QPoint{}, QPoint(0, angleDelta),
        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    event.setAccepted(false);
    QApplication::sendEvent(scrollBar, &event);
    return scrollBar->value();
}

QLineEdit* spriteLineEdit(QWidget* const pane, const QString& name)
{
    if ( auto* const scrub = pane->findChild<ui::ScrubFloatEdit*>(name) )
    {
        return scrub->lineEdit();
    }
    return pane->findChild<QLineEdit*>(name);
}

void setReverseSelection(QLineEdit* const edit, const int anchor, const int length)
{
    edit->setCursorPosition(anchor);
    edit->cursorBackward(true, length);
}

bool hasReverseSelection(const QLineEdit* const edit)
{
    return edit && edit->hasSelectedText() && edit->cursorPosition() == edit->selectionStart()
        && edit->selectionStart() + edit->selectedText().size() > edit->cursorPosition();
}

bool sameTableRanges(
    const QList<QTableWidgetSelectionRange>& actual,
    const QList<QTableWidgetSelectionRange>& expected)
{
    if ( actual.size() != expected.size() )
    {
        return false;
    }
    for ( qsizetype index = 0; index < actual.size(); ++index )
    {
        const QTableWidgetSelectionRange& left = actual.at(index);
        const QTableWidgetSelectionRange& right = expected.at(index);
        if ( left.topRow() != right.topRow() || left.bottomRow() != right.bottomRow()
            || left.leftColumn() != right.leftColumn() || left.rightColumn() != right.rightColumn() )
        {
            return false;
        }
    }
    return true;
}

QTreeWidgetItem* findSoundItem(
    QTreeWidget* const tree,
    const ui::resources::detail::SoundTreeItemKind kind,
    const int sourceOrdinal = -1)
{
    if ( !tree )
    {
        return nullptr;
    }
    const auto find = [&](const auto& self, QTreeWidgetItem* const item) -> QTreeWidgetItem*
    {
        if ( item->data(0, ui::resources::detail::kSoundTreeItemKindRole).toInt()
                == static_cast<int>(kind)
            && (sourceOrdinal < 0
                || item->data(0, ui::resources::detail::kSoundTreeSourceIndexRole).toInt()
                    == sourceOrdinal) )
        {
            return item;
        }
        for ( int child = 0; child < item->childCount(); ++child )
        {
            if ( QTreeWidgetItem* const match = self(self, item->child(child)) )
            {
                return match;
            }
        }
        return nullptr;
    };
    for ( int top = 0; top < tree->topLevelItemCount(); ++top )
    {
        if ( QTreeWidgetItem* const match = find(find, tree->topLevelItem(top)) )
        {
            return match;
        }
    }
    return nullptr;
}

QTreeWidgetItem* findSoundItemForMode(
    QTreeWidget* const tree,
    const ui::resources::detail::SoundTreeItemKind kind,
    const QByteArray& modeBytes)
{
    if ( !tree )
    {
        return nullptr;
    }
    const auto find = [&modeBytes, kind](const auto& self, QTreeWidgetItem* const item) -> QTreeWidgetItem*
    {
        if ( item->data(0, ui::resources::detail::kSoundTreeItemKindRole).toInt()
                == static_cast<int>(kind)
            && item->data(0, ui::resources::detail::kSoundTreeModeBytesRole).toByteArray() == modeBytes )
        {
            return item;
        }
        for ( int child = 0; child < item->childCount(); ++child )
        {
            if ( QTreeWidgetItem* const match = self(self, item->child(child)) )
            {
                return match;
            }
        }
        return nullptr;
    };
    for ( int top = 0; top < tree->topLevelItemCount(); ++top )
    {
        if ( QTreeWidgetItem* const match = find(find, tree->topLevelItem(top)) )
        {
            return match;
        }
    }
    return nullptr;
}

void testSpritePermanentCaretAndMalformedState()
{
    auto source = makePane(ResourceAuthoringKind::sprite);
    check(source && seedPane(source.get(), ResourceAuthoringKind::sprite),
        "resource presentation Sprite fixture seeds a clean authored profile");
    if ( !source )
    {
        return;
    }
    QLineEdit* const sourceMaterial = spriteLineEdit(source.get(), QStringLiteral("spr.material"));
    QLineEdit* const sourceWidth = spriteLineEdit(source.get(), QStringLiteral("spr.width"));
    check(sourceMaterial && sourceWidth,
        "resource presentation Sprite fixture finds permanent material and numeric line edits");
    if ( !sourceMaterial || !sourceWidth )
    {
        return;
    }

    setReverseSelection(sourceMaterial, 14, 5);
    setReverseSelection(sourceWidth, sourceWidth->text().size(), 2);
    const int materialCursor = sourceMaterial->cursorPosition();
    const int materialAnchor = sourceMaterial->selectionStart() + sourceMaterial->selectedText().size();
    const int widthCursor = sourceWidth->cursorPosition();
    const int widthAnchor = sourceWidth->selectionStart() + sourceWidth->selectedText().size();
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(source.get());
    const QCborMap presentation = captured.value(QStringLiteral("editorPresentation")).toMap();
    const QCborMap lineEdits = presentation.value(QStringLiteral("lineEdits")).toMap();
    const QCborMap materialState = lineEdits.value(QStringLiteral("spr.material")).toMap();
    const QCborMap widthState = lineEdits.value(QStringLiteral("spr.width")).toMap();
    check(presentation.value(QStringLiteral("version")).toInteger() == 1
              && materialState.value(QStringLiteral("cursor")).toInteger() == materialCursor
              && materialState.value(QStringLiteral("anchor")).toInteger() == materialAnchor
              && widthState.value(QStringLiteral("cursor")).toInteger() == widthCursor
              && widthState.value(QStringLiteral("anchor")).toInteger() == widthAnchor,
        "resource presentation Sprite capture records stable-keyed reverse carets and anchors for material and numeric editors");

    auto target = makePane(ResourceAuthoringKind::sprite);
    check(target && ui::resources::restoreResourceAuthoringPaneState(target.get(), captured),
        "resource presentation Sprite restore accepts the captured pane and authored state");
    if ( !target )
    {
        return;
    }
    QLineEdit* const restoredMaterial = target
        ? spriteLineEdit(target.get(), QStringLiteral("spr.material")) : nullptr;
    QLineEdit* const restoredWidth = target
        ? spriteLineEdit(target.get(), QStringLiteral("spr.width")) : nullptr;
    check(restoredMaterial && restoredMaterial->text() == sourceMaterial->text()
              && restoredMaterial->cursorPosition() == materialCursor
              && hasReverseSelection(restoredMaterial)
              && restoredWidth && restoredWidth->cursorPosition() == widthCursor
              && hasReverseSelection(restoredWidth)
              && !ui::resources::resourceAuthoringPaneIsModified(target.get()),
        "resource presentation Sprite restore applies material and numeric reverse selections without dirtying authored data");
    if ( !restoredMaterial || !restoredWidth )
    {
        return;
    }

    const QCborMap beforeRejectedRestore = ui::resources::captureResourceAuthoringPaneState(target.get());
    QCborMap malformed = captured;
    QCborArray changedSprite = malformed.value(QStringLiteral("sprite")).toArray();
    changedSprite[0] = QStringLiteral("must_not_install.mat");
    malformed.insert(QStringLiteral("sprite"), changedSprite);
    malformed.insert(QStringLiteral("editorPresentation"), QStringLiteral("not-a-map"));
    malformed.remove(QStringLiteral("history"));
    QString malformedWarning{};
    QString malformedRestoreWarning{};
    const bool malformedAccepted = ui::resources::validateResourceAuthoringPaneState(malformed, &malformedWarning);
    const bool malformedRestored = ui::resources::restoreResourceAuthoringPaneState(
        target.get(), malformed, &malformedRestoreWarning);
    check(!malformedAccepted && !malformedRestored
              && ui::resources::captureResourceAuthoringPaneState(target.get()) == beforeRejectedRestore
              && malformedWarning.contains(QStringLiteral("presentation"), Qt::CaseInsensitive)
              && malformedRestoreWarning.contains(QStringLiteral("presentation"), Qt::CaseInsensitive)
              && restoredMaterial->text() == QStringLiteral("presentation_original.mat"),
        "resource presentation rejects a malformed optional map before any Sprite state is installed");

    QCborMap malformedVersion = captured;
    QCborMap invalidVersionPresentation = malformedVersion.value(QStringLiteral("editorPresentation")).toMap();
    invalidVersionPresentation.insert(QStringLiteral("version"), 2);
    malformedVersion.insert(QStringLiteral("editorPresentation"), invalidVersionPresentation);
    malformedVersion.remove(QStringLiteral("history"));
    QString versionWarning{};
    const bool versionAccepted = ui::resources::validateResourceAuthoringPaneState(
        malformedVersion, &versionWarning);

    QCborMap malformedCaret = captured;
    QCborMap invalidCaretPresentation = malformedCaret.value(QStringLiteral("editorPresentation")).toMap();
    QCborMap invalidLineEdits = invalidCaretPresentation.value(QStringLiteral("lineEdits")).toMap();
    QCborMap incompleteCaret = invalidLineEdits.value(QStringLiteral("spr.material")).toMap();
    incompleteCaret.remove(QStringLiteral("anchor"));
    invalidLineEdits.insert(QStringLiteral("spr.material"), incompleteCaret);
    invalidCaretPresentation.insert(QStringLiteral("lineEdits"), invalidLineEdits);
    malformedCaret.insert(QStringLiteral("editorPresentation"), invalidCaretPresentation);
    malformedCaret.remove(QStringLiteral("history"));
    QString caretWarning{};
    const bool caretAccepted = ui::resources::validateResourceAuthoringPaneState(
        malformedCaret, &caretWarning);
    check(!versionAccepted && versionWarning.contains(QStringLiteral("presentation"), Qt::CaseInsensitive)
              && !caretAccepted && caretWarning.contains(QStringLiteral("caret"), Qt::CaseInsensitive),
        "resource presentation validates its version and rejects incomplete stable-key caret identities");

    QCborMap futureCompatible = captured;
    QCborMap futurePresentation = futureCompatible.value(QStringLiteral("editorPresentation")).toMap();
    futurePresentation.insert(QStringLiteral("futureExtension"), QCborMap{{QStringLiteral("ignored"), true}});
    futureCompatible.insert(QStringLiteral("editorPresentation"), futurePresentation);
    check(ui::resources::validateResourceAuthoringPaneState(futureCompatible)
              && ui::resources::restoreResourceAuthoringPaneState(target.get(), futureCompatible),
        "resource presentation accepts unknown optional presentation members for forward compatibility");

    QCborMap legacy = captured;
    legacy.remove(QStringLiteral("editorPresentation"));
    legacy.insert(QStringLiteral("futurePaneMember"), true);
    check(ui::resources::validateResourceAuthoringPaneState(legacy)
              && ui::resources::restoreResourceAuthoringPaneState(target.get(), legacy)
              && restoredMaterial->text() == QStringLiteral("presentation_original.mat"),
        "resource presentation reads legacy pane snapshots with no presentation map and ignores unknown extras");
}

void testSpriteHistoryUsesOnlyCurrentCaretLength()
{
    auto source = makePane(ResourceAuthoringKind::sprite);
    check(source != nullptr, "resource history caret fixture creates its Sprite pane");
    if ( !source )
    {
        return;
    }

    QCborMap baseline = ui::resources::captureResourceAuthoringPaneState(source.get(), false);
    baseline.remove(QStringLiteral("editorPresentation"));
    baseline.remove(QStringLiteral("history"));
    QCborArray baselineSprite = baseline.value(QStringLiteral("sprite")).toArray();
    check(baselineSprite.size() == 15,
        "resource history caret fixture has the complete Sprite baseline record");
    if ( baselineSprite.size() != 15 )
    {
        return;
    }
    baselineSprite[0] = QStringLiteral("new_sprite.mat");
    baseline.insert(QStringLiteral("sprite"), baselineSprite);
    baseline.insert(QStringLiteral("modified"), false);
    check(ui::resources::restoreResourceAuthoringPaneState(source.get(), baseline),
        "resource history caret fixture establishes a clean 14-character Sprite material baseline");

    QLineEdit* const material = spriteLineEdit(source.get(), QStringLiteral("spr.material"));
    const QString currentMaterial = QStringLiteral("native_baseline.mat");
    check(material != nullptr && currentMaterial.size() == 19,
        "resource history caret fixture finds the Sprite material editor and longer current name");
    if ( !material )
    {
        return;
    }
    material->setCursorPosition(material->text().size());
    material->setText(currentMaterial);
    material->setCursorPosition(currentMaterial.size());

    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(source.get());
    const QCborMap presentation = captured.value(QStringLiteral("editorPresentation")).toMap();
    const QCborMap lineEdits = presentation.value(QStringLiteral("lineEdits")).toMap();
    const QCborMap materialCaret = lineEdits.value(QStringLiteral("spr.material")).toMap();
    QCborMap currentWithoutHistory = captured;
    currentWithoutHistory.remove(QStringLiteral("history"));
    const bool currentPresentationValid = ui::resources::validateResourceAuthoringPaneState(
        currentWithoutHistory);
    check(currentPresentationValid && materialCaret.value(QStringLiteral("cursor")).toInteger() == 19,
        "resource history caret fixture proves the current 19-character Sprite and end caret are valid without history");

    auto target = makePane(ResourceAuthoringKind::sprite);
    QString historyWarning{};
    const bool historyValid = ui::resources::validateResourceAuthoringPaneState(captured, &historyWarning);
    const bool restored = target && ui::resources::restoreResourceAuthoringPaneState(
        target.get(), captured, &historyWarning);
    check(historyValid && restored,
        "resource history admits and restores an older 14-character Sprite command with the current caret at 19");
    if ( !historyValid || !restored )
    {
        return;
    }

    QLineEdit* const restoredMaterial = spriteLineEdit(target.get(), QStringLiteral("spr.material"));
    const QCborMap restoredState = ui::resources::captureResourceAuthoringPaneState(target.get());
    const QCborMap restoredCaret = restoredState.value(QStringLiteral("editorPresentation")).toMap()
        .value(QStringLiteral("lineEdits")).toMap().value(QStringLiteral("spr.material")).toMap();
    check(restoredMaterial && restoredMaterial->text() == currentMaterial
              && restoredMaterial->cursorPosition() == currentMaterial.size()
              && restoredCaret.value(QStringLiteral("cursor")).toInteger() == currentMaterial.size(),
        "resource history restore retains the exact current Sprite value and caret");

    if ( QWidget* const focused = QApplication::focusWidget() )
    {
        focused->clearFocus();
    }
    const bool undone = ui::resources::resourceAuthoringPaneUndoRedo(target.get(), false);
    const QCborMap afterUndo = ui::resources::captureResourceAuthoringPaneState(target.get());
    QString undoWarning{};
    const bool undoValid = ui::resources::validateResourceAuthoringPaneState(afterUndo, &undoWarning);
    check(undone && undoValid && restoredMaterial
              && restoredMaterial->text() == QStringLiteral("new_sprite.mat")
              && restoredMaterial->cursorPosition() == QStringLiteral("new_sprite.mat").size()
              && !ui::resources::resourceAuthoringPaneIsModified(target.get()),
        "resource history Undo restores the exact short material without an out-of-range current caret");

    const bool redone = ui::resources::resourceAuthoringPaneUndoRedo(target.get(), true);
    const QCborMap afterRedo = ui::resources::captureResourceAuthoringPaneState(target.get());
    QString redoWarning{};
    const bool redoValid = ui::resources::validateResourceAuthoringPaneState(afterRedo, &redoWarning);
    check(redone && redoValid && restoredMaterial && restoredMaterial->text() == currentMaterial
              && restoredMaterial->cursorPosition() == currentMaterial.size()
              && ui::resources::resourceAuthoringPaneIsModified(target.get()),
        "resource history Redo restores the exact long material and end caret with its history still valid");

    QCborMap malformedCurrent = afterRedo;
    QCborMap malformedPresentation = malformedCurrent.value(QStringLiteral("editorPresentation")).toMap();
    QCborMap malformedLineEdits = malformedPresentation.value(QStringLiteral("lineEdits")).toMap();
    malformedLineEdits.insert(QStringLiteral("spr.material"), QCborMap{
        {QStringLiteral("cursor"), currentMaterial.size() + 1},
        {QStringLiteral("anchor"), currentMaterial.size() + 1}});
    malformedPresentation.insert(QStringLiteral("lineEdits"), malformedLineEdits);
    malformedCurrent.insert(QStringLiteral("editorPresentation"), malformedPresentation);
    QString malformedWarning{};
    const QCborMap beforeRejectedCurrentRestore = ui::resources::captureResourceAuthoringPaneState(target.get());
    QString malformedRestoreWarning{};
    const bool malformedRestored = ui::resources::restoreResourceAuthoringPaneState(
        target.get(), malformedCurrent, &malformedRestoreWarning);
    check(!ui::resources::validateResourceAuthoringPaneState(malformedCurrent, &malformedWarning)
              && !malformedRestored
              && malformedWarning.contains(QStringLiteral("spr.material"), Qt::CaseInsensitive)
              && malformedWarning.contains(QStringLiteral("position"), Qt::CaseInsensitive)
              && malformedRestoreWarning.contains(QStringLiteral("position"), Qt::CaseInsensitive)
              && ui::resources::captureResourceAuthoringPaneState(target.get()) == beforeRejectedCurrentRestore,
        "resource history still rejects an out-of-range current Sprite caret without partially restoring the pane");

    QCborMap malformedHistory = afterRedo;
    QCborMap invalidHistory = malformedHistory.value(QStringLiteral("history")).toMap();
    QCborArray invalidCommands = invalidHistory.value(QStringLiteral("commands")).toArray();
    check(invalidCommands.size() == 1,
        "resource history malformed-record fixture has one prior Sprite command");
    if ( invalidCommands.size() != 1 )
    {
        return;
    }
    QCborMap invalidCommand = invalidCommands.at(0).toMap();
    QCborMap invalidBefore = invalidCommand.value(QStringLiteral("before")).toMap();
    QCborArray invalidHistoricalSprite = invalidBefore.value(QStringLiteral("sprite")).toArray();
    check(invalidHistoricalSprite.size() == 15,
        "resource history malformed-record fixture retains the historical Sprite record shape");
    if ( invalidHistoricalSprite.size() != 15 )
    {
        return;
    }
    invalidHistoricalSprite[2] = QStringLiteral("not-a-number");
    invalidBefore.insert(QStringLiteral("sprite"), invalidHistoricalSprite);
    invalidCommand.insert(QStringLiteral("before"), invalidBefore);
    invalidCommands[0] = invalidCommand;
    invalidHistory.insert(QStringLiteral("commands"), invalidCommands);
    malformedHistory.insert(QStringLiteral("history"), invalidHistory);
    const QCborMap beforeRejectedHistoryRestore = ui::resources::captureResourceAuthoringPaneState(target.get());
    QString invalidHistoryWarning{};
    const bool invalidHistoryAccepted = ui::resources::validateResourceAuthoringPaneState(
        malformedHistory, &invalidHistoryWarning);
    QString invalidHistoryRestoreWarning{};
    const bool invalidHistoryRestored = ui::resources::restoreResourceAuthoringPaneState(
        target.get(), malformedHistory, &invalidHistoryRestoreWarning);
    check(!invalidHistoryAccepted && !invalidHistoryRestored
              && invalidHistoryWarning.contains(QStringLiteral("history"), Qt::CaseInsensitive)
              && invalidHistoryRestoreWarning.contains(QStringLiteral("history"), Qt::CaseInsensitive)
              && ui::resources::captureResourceAuthoringPaneState(target.get()) == beforeRejectedHistoryRestore,
        "resource history rejects malformed historical Sprite numbers without partially restoring the pane");
}

void testUnicodeLineEditCaretRoundTrip()
{
    struct SelectionCase
    {
        QString text{};
        int start{};
        int length{};
    };
    const std::array<SelectionCase, 4> cases{
        SelectionCase{QStringLiteral("A\U0001F600B"), 1, 2},
        SelectionCase{QStringLiteral("A\U0001F600B"), 3, -2},
        SelectionCase{QStringLiteral("Ae\u0301B"), 1, 2},
        SelectionCase{QStringLiteral("Ae\u0301B"), 3, -2},
    };

    for ( const SelectionCase& selection : cases )
    {
        auto source = makePane(ResourceAuthoringKind::sprite);
        check(source && seedPane(source.get(), ResourceAuthoringKind::sprite),
            "resource presentation Unicode caret fixture seeds a Sprite pane");
        if ( !source )
        {
            continue;
        }
        QLineEdit* const sourceEdit = spriteLineEdit(source.get(), QStringLiteral("spr.material"));
        check(sourceEdit != nullptr, "resource presentation Unicode caret fixture finds the material editor");
        if ( !sourceEdit )
        {
            continue;
        }

        sourceEdit->setText(selection.text);
        sourceEdit->setSelection(selection.start, selection.length);
        const int expectedCursor = sourceEdit->cursorPosition();
        const int expectedSelectionStart = sourceEdit->selectionStart();
        const QString expectedSelectedText = sourceEdit->selectedText();
        const int expectedAnchor = expectedCursor == expectedSelectionStart
            ? expectedSelectionStart + expectedSelectedText.size()
            : expectedSelectionStart;
        const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(source.get());
        const QCborMap caret = captured.value(QStringLiteral("editorPresentation")).toMap()
            .value(QStringLiteral("lineEdits")).toMap()
            .value(QStringLiteral("spr.material")).toMap();

        auto target = makePane(ResourceAuthoringKind::sprite);
        QString restoreWarning{};
        check(target && seedPane(target.get(), ResourceAuthoringKind::sprite),
            "resource presentation Unicode caret target seeds a Sprite pane");
        if ( !target )
        {
            continue;
        }
        QLineEdit* const restoredEdit = spriteLineEdit(target.get(), QStringLiteral("spr.material"));
        const QWidget* const focusBeforeRestore = QApplication::focusWidget();
        const bool restored = ui::resources::restoreResourceAuthoringPaneState(
            target.get(), captured, &restoreWarning);
        const QCborMap restoredState = ui::resources::captureResourceAuthoringPaneState(target.get());
        check(caret.value(QStringLiteral("cursor")).toInteger() == expectedCursor
                  && caret.value(QStringLiteral("anchor")).toInteger() == expectedAnchor,
            "resource presentation captures exact UTF-16 cursor and anchor offsets for Unicode selections");
        check(restored && restoredEdit && restoredEdit->text() == selection.text
                  && restoredEdit->cursorPosition() == expectedCursor
                  && restoredEdit->selectionStart() == expectedSelectionStart
                  && restoredEdit->selectedText() == expectedSelectedText
                  && QApplication::focusWidget() == focusBeforeRestore
                  && restoredState.value(QStringLiteral("history")) == captured.value(QStringLiteral("history"))
                  && restoredState.value(QStringLiteral("modified")) == captured.value(QStringLiteral("modified")),
            "resource presentation restores forward and reverse Unicode selections at exact Qt cursor positions without focus or history changes");
    }
}

void testUniSelectionScrollFilterAndBadReference(SpriteGlViewEventBlocker& blocker)
{
    auto source = makePane(ResourceAuthoringKind::stringTable);
    check(source && seedPane(source.get(), ResourceAuthoringKind::stringTable),
        "resource presentation UNI fixture seeds rows including a long horizontal-scroll key");
    if ( !source )
    {
        return;
    }
    showOffscreen(source.get(), blocker, QSize(680, 390));
    auto* const sourceTable = source->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    auto* const sourceSearch = source->findChild<QLineEdit*>(QStringLiteral("uni.search"));
    check(sourceTable && sourceSearch && sourceTable->rowCount() == 120,
        "resource presentation UNI fixture exposes its real table and filter widgets");
    if ( !sourceTable || !sourceSearch )
    {
        return;
    }
    sourceTable->selectRow(117);
    sourceTable->selectionModel()->setCurrentIndex(
        sourceTable->model()->index(118, 1), QItemSelectionModel::NoUpdate);
    sourceSearch->setText(QStringLiteral("FILTER_0"));
    const QList<QTableWidgetSelectionRange> sourceRanges = sourceTable->selectedRanges();
    bool sourceSelectedEveryColumn = true;
    for ( int column = 0; column < sourceTable->columnCount(); ++column )
    {
        sourceSelectedEveryColumn = sourceSelectedEveryColumn
            && sourceTable->selectionModel()->isSelected(sourceTable->model()->index(117, column));
    }
    const bool sourceSelectionExact = sourceRanges.size() == 1
        && sourceRanges.front().topRow() == 117 && sourceRanges.front().bottomRow() == 117
        && sourceRanges.front().leftColumn() == 0 && sourceRanges.front().rightColumn() == 2
        && sourceTable->columnCount() == 3 && sourceTable->isColumnHidden(2);
    check(sourceTable->isRowHidden(117) && sourceTable->isRowHidden(118)
              && sourceTable->currentRow() == 118 && sourceTable->currentColumn() == 1
              && sourceSelectionExact && sourceSelectedEveryColumn
              && !sourceTable->selectionModel()->isSelected(sourceTable->model()->index(118, 1)),
        "resource presentation UNI fixture keeps a hidden selected row range distinct from its hidden current cell, including the legacy column");
    sourceTable->horizontalScrollBar()->setValue(sourceTable->horizontalScrollBar()->maximum());
    sourceTable->verticalScrollBar()->setValue(sourceTable->verticalScrollBar()->maximum());
    QApplication::processEvents(QEventLoop::AllEvents);
    const int sourceHorizontal = sourceTable->horizontalScrollBar()->value();
    const int sourceVertical = sourceTable->verticalScrollBar()->value();
    const QCborMap selectedState = ui::resources::captureResourceAuthoringPaneState(source.get());
    check(sourceHorizontal > 0 && sourceVertical > 0,
        "resource presentation UNI fixture exercises nonzero horizontal and vertical scroll offsets");

    auto target = makePane(ResourceAuthoringKind::stringTable);
    check(target && seedPane(target.get(), ResourceAuthoringKind::stringTable),
        "resource presentation UNI target starts with the same authored rows");
    if ( !target )
    {
        return;
    }
    showOffscreen(target.get(), blocker, QSize(680, 390));
    check(ui::resources::restoreResourceAuthoringPaneState(target.get(), selectedState),
        "resource presentation UNI restore accepts selection, filter and scroll state");
    QApplication::processEvents(QEventLoop::AllEvents);
    auto* const restoredTable = target->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    auto* const restoredSearch = target->findChild<QLineEdit*>(QStringLiteral("uni.search"));
    check(restoredTable && restoredSearch,
        "resource presentation UNI restored page retains its actual table and filter widgets");
    if ( !restoredTable || !restoredSearch )
    {
        return;
    }
    const QList<QTableWidgetSelectionRange> restoredRanges = restoredTable->selectedRanges();
    check(restoredSearch->text() == QStringLiteral("FILTER_0")
              && restoredTable->isRowHidden(117)
              && restoredTable->isRowHidden(118)
              && restoredTable->currentRow() == 118 && restoredTable->currentColumn() == 1
              && restoredTable->selectionModel()->hasSelection()
              && sameTableRanges(restoredRanges, sourceRanges)
              && restoredTable->horizontalScrollBar()->value() == sourceHorizontal
              && restoredTable->verticalScrollBar()->value() == sourceVertical,
        "resource presentation UNI restore applies the filtered current cell, row selection and both scroll offsets after layout");

    auto interruptedRestore = makePane(ResourceAuthoringKind::stringTable);
    check(interruptedRestore && seedPane(interruptedRestore.get(), ResourceAuthoringKind::stringTable),
        "resource presentation UNI delayed-scroll fixture has an independent table");
    if ( !interruptedRestore )
    {
        return;
    }
    showOffscreen(interruptedRestore.get(), blocker, QSize(680, 390));
    const bool interruptedStateRestored =
        ui::resources::restoreResourceAuthoringPaneState(interruptedRestore.get(), selectedState);
    auto* const interruptedTable = interruptedRestore->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    const int scrollBeforeUserAction = interruptedTable
        ? interruptedTable->verticalScrollBar()->value() : 0;
    const int userScrollValue = interruptedTable
        ? sendVerticalScrollWheel(interruptedTable->verticalScrollBar(), -120) : 0;
    QApplication::processEvents(QEventLoop::AllEvents);
    check(interruptedStateRestored && interruptedTable && userScrollValue != scrollBeforeUserAction
              && sourceVertical > userScrollValue
              && interruptedTable->verticalScrollBar()->value() == userScrollValue,
        "resource presentation UNI cancels a queued restored scroll when the user changes the viewport before it runs");

    restoredSearch->clear();
    QApplication::processEvents(QEventLoop::AllEvents);
    check(!restoredTable->isRowHidden(117) && !restoredTable->isRowHidden(118)
              && restoredTable->currentRow() == 118
              && restoredTable->currentColumn() == 1
              && sameTableRanges(restoredTable->selectedRanges(), sourceRanges),
        "resource presentation UNI reveals the separately selected row and current cell when its saved filter is later cleared");

    restoredTable->selectionModel()->setCurrentIndex(
        restoredTable->model()->index(118, 1), QItemSelectionModel::NoUpdate);
    restoredTable->clearSelection();
    check(restoredTable->currentRow() == 118 && restoredTable->currentColumn() == 1
              && !restoredTable->selectionModel()->hasSelection(),
        "resource presentation UNI empty-selection source establishes current independently before clearing selection last");
    const QCborMap explicitEmptySelection = ui::resources::captureResourceAuthoringPaneState(target.get());
    auto emptyTarget = makePane(ResourceAuthoringKind::stringTable);
    check(emptyTarget && seedPane(emptyTarget.get(), ResourceAuthoringKind::stringTable),
        "resource presentation UNI empty-selection target has an independent table model");
    if ( !emptyTarget )
    {
        return;
    }
    showOffscreen(emptyTarget.get(), blocker, QSize(680, 390));
    check(ui::resources::restoreResourceAuthoringPaneState(emptyTarget.get(), explicitEmptySelection),
        "resource presentation UNI restore accepts an explicit empty selection");
    auto* const emptyTable = emptyTarget->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    check(emptyTable != nullptr, "resource presentation UNI empty-selection target keeps its real table widget");
    if ( !emptyTable )
    {
        return;
    }
    check(emptyTable->currentRow() == 118 && emptyTable->currentColumn() == 1
              && !emptyTable->selectionModel()->hasSelection(),
        "resource presentation UNI distinguishes an explicit empty selection from its current cell");

    QCborMap currentOnly = explicitEmptySelection;
    QCborMap currentOnlyPresentation = currentOnly.value(QStringLiteral("editorPresentation")).toMap();
    QCborMap currentOnlyTables = currentOnlyPresentation.value(QStringLiteral("tables")).toMap();
    QCborMap currentOnlyTable = currentOnlyTables.value(QStringLiteral("uni.entries")).toMap();
    currentOnlyTable.remove(QStringLiteral("selection"));
    currentOnlyTable.insert(QStringLiteral("current"), QCborMap{
        {QStringLiteral("row"), 119}, {QStringLiteral("column"), 2}});
    currentOnlyTables.insert(QStringLiteral("uni.entries"), currentOnlyTable);
    currentOnlyPresentation.insert(QStringLiteral("tables"), currentOnlyTables);
    currentOnly.insert(QStringLiteral("editorPresentation"), currentOnlyPresentation);
    currentOnly.remove(QStringLiteral("history"));
    check(ui::resources::validateResourceAuthoringPaneState(currentOnly)
              && ui::resources::restoreResourceAuthoringPaneState(emptyTarget.get(), currentOnly)
              && emptyTable->currentRow() == 119 && emptyTable->currentColumn() == 2
              && !emptyTable->selectionModel()->hasSelection(),
        "resource presentation restores a valid UNI current cell independently when an optional selection field is absent");

    QCborMap badReference = selectedState;
    QCborMap presentation = badReference.value(QStringLiteral("editorPresentation")).toMap();
    QCborMap tables = presentation.value(QStringLiteral("tables")).toMap();
    QCborMap tableState = tables.value(QStringLiteral("uni.entries")).toMap();
    tableState.insert(QStringLiteral("current"), QCborMap{
        {QStringLiteral("row"), 9000}, {QStringLiteral("column"), 1}});
    tables.insert(QStringLiteral("uni.entries"), tableState);
    presentation.insert(QStringLiteral("tables"), tables);
    badReference.insert(QStringLiteral("editorPresentation"), presentation);
    QCborArray replacementRows = badReference.value(QStringLiteral("rows")).toArray();
    QCborArray replacementRow = replacementRows.at(0).toArray();
    replacementRow[0] = QStringLiteral("must_not_install");
    replacementRows[0] = replacementRow;
    badReference.insert(QStringLiteral("rows"), replacementRows);
    badReference.remove(QStringLiteral("history"));
    const QCborMap beforeBadReference = ui::resources::captureResourceAuthoringPaneState(emptyTarget.get());
    QString referenceWarning{};
    QString referenceRestoreWarning{};
    const bool referenceAccepted = ui::resources::validateResourceAuthoringPaneState(
        badReference, &referenceWarning);
    const bool referenceRestored = ui::resources::restoreResourceAuthoringPaneState(
        emptyTarget.get(), badReference, &referenceRestoreWarning);
    check(!referenceAccepted && !referenceRestored
              && (referenceWarning.contains(QStringLiteral("presentation"), Qt::CaseInsensitive)
                  || referenceWarning.contains(QStringLiteral("current"), Qt::CaseInsensitive))
              && (referenceRestoreWarning.contains(QStringLiteral("presentation"), Qt::CaseInsensitive)
                  || referenceRestoreWarning.contains(QStringLiteral("current"), Qt::CaseInsensitive))
              && ui::resources::captureResourceAuthoringPaneState(emptyTarget.get()) == beforeBadReference
              && emptyTable->item(0, 0)->text() != QStringLiteral("must_not_install"),
        "resource presentation rejects out-of-range table references atomically before changing authored UNI rows");

    QCborMap incompleteCurrent = selectedState;
    QCborMap currentPresentation = incompleteCurrent.value(QStringLiteral("editorPresentation")).toMap();
    QCborMap currentTables = currentPresentation.value(QStringLiteral("tables")).toMap();
    QCborMap currentTable = currentTables.value(QStringLiteral("uni.entries")).toMap();
    currentTable.insert(QStringLiteral("current"), QCborMap{});
    currentTables.insert(QStringLiteral("uni.entries"), currentTable);
    currentPresentation.insert(QStringLiteral("tables"), currentTables);
    incompleteCurrent.insert(QStringLiteral("editorPresentation"), currentPresentation);
    incompleteCurrent.remove(QStringLiteral("history"));
    QString incompleteCurrentWarning{};
    check(!ui::resources::validateResourceAuthoringPaneState(incompleteCurrent, &incompleteCurrentWarning)
              && incompleteCurrentWarning.contains(QStringLiteral("current"), Qt::CaseInsensitive),
        "resource presentation rejects a UNI current-cell map with missing stable row and column fields");
}

void testSndInterleavedIdentityAndManualWidths(SpriteGlViewEventBlocker& blocker)
{
    auto source = makePane(ResourceAuthoringKind::soundClass);
    check(source && seedPane(source.get(), ResourceAuthoringKind::soundClass),
        "resource presentation SND fixture seeds interleaved source ordinals in two mode groups");
    if ( !source )
    {
        return;
    }
    showOffscreen(source.get(), blocker, QSize(1120, 660));
    auto* const sourceTree = source->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    auto* const sourceSearch = source->findChild<QLineEdit*>(QStringLiteral("snd.search"));
    check(sourceTree && sourceSearch && sourceTree->selectionMode() == QAbstractItemView::NoSelection,
        "resource presentation SND fixture exposes its no-selection tree and search line edit");
    if ( !sourceTree || !sourceSearch )
    {
        return;
    }
    constexpr int sourceOrdinal = 83;
    QTreeWidgetItem* const current = findSoundItem(sourceTree,
        ui::resources::detail::SoundTreeItemKind::alternative, sourceOrdinal);
    check(current != nullptr, "resource presentation SND fixture resolves an interleaved source row by its stable ordinal role");
    if ( !current )
    {
        return;
    }
    sourceSearch->setText(QStringLiteral("create"));
    sourceTree->setCurrentItem(current, 4);
    const std::array<int, 6> requestedWidths{170, 300, 200, 140, 140, 350};
    for ( int column = 0; column < 6; ++column )
    {
        sourceTree->header()->resizeSection(column, requestedWidths[static_cast<std::size_t>(column)]);
    }
    sourceTree->doItemsLayout();
    QApplication::processEvents(QEventLoop::AllEvents);
    sourceTree->doItemsLayout();
    sourceTree->verticalScrollBar()->setValue(sourceTree->verticalScrollBar()->maximum());
    sourceTree->horizontalScrollBar()->setValue(sourceTree->horizontalScrollBar()->maximum());
    QApplication::processEvents(QEventLoop::AllEvents);
    sourceTree->verticalScrollBar()->setValue(sourceTree->verticalScrollBar()->maximum());
    sourceTree->horizontalScrollBar()->setValue(sourceTree->horizontalScrollBar()->maximum());
    QApplication::processEvents(QEventLoop::AllEvents);
    const std::array<int, 6> capturedWidths{
        sourceTree->columnWidth(0), sourceTree->columnWidth(1), sourceTree->columnWidth(2),
        sourceTree->columnWidth(3), sourceTree->columnWidth(4), sourceTree->columnWidth(5)};
    const int sourceHorizontal = sourceTree->horizontalScrollBar()->value();
    const int sourceVertical = sourceTree->verticalScrollBar()->value();
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(source.get());
    if ( sourceHorizontal == 0 || sourceVertical == 0 )
    {
        qWarning() << "SND presentation scroll fixture did not overflow both axes"
                   << "viewport" << sourceTree->viewport()->size()
                   << "scroll ranges" << sourceTree->horizontalScrollBar()->minimum()
                   << sourceTree->horizontalScrollBar()->maximum()
                   << sourceTree->verticalScrollBar()->minimum()
                   << sourceTree->verticalScrollBar()->maximum()
                   << "columns" << capturedWidths[0] << capturedWidths[1] << capturedWidths[2]
                   << capturedWidths[3] << capturedWidths[4] << capturedWidths[5];
    }
    check(!current->isHidden() && sourceHorizontal > 0 && sourceVertical > 0,
        "resource presentation SND fixture filters by the matching mode title and exercises both nonzero scroll offsets");

    QCborMap missingModeIdentity = captured;
    QCborMap invalidModePresentation = missingModeIdentity.value(QStringLiteral("editorPresentation")).toMap();
    QCborMap invalidModeTables = invalidModePresentation.value(QStringLiteral("tables")).toMap();
    QCborMap invalidModeTable = invalidModeTables.value(QStringLiteral("snd.entries")).toMap();
    invalidModeTable.insert(QStringLiteral("current"), QCborMap{
        {QStringLiteral("kind"), static_cast<int>(ui::resources::detail::SoundTreeItemKind::mode)}});
    invalidModeTables.insert(QStringLiteral("snd.entries"), invalidModeTable);
    invalidModePresentation.insert(QStringLiteral("tables"), invalidModeTables);
    missingModeIdentity.insert(QStringLiteral("editorPresentation"), invalidModePresentation);
    missingModeIdentity.remove(QStringLiteral("history"));
    QString modeWarning{};
    const bool modeAccepted = ui::resources::validateResourceAuthoringPaneState(
        missingModeIdentity, &modeWarning);

    QCborMap oversizedWidths = captured;
    QCborMap widthPresentation = oversizedWidths.value(QStringLiteral("editorPresentation")).toMap();
    QCborMap widthTables = widthPresentation.value(QStringLiteral("tables")).toMap();
    QCborMap widthTable = widthTables.value(QStringLiteral("snd.entries")).toMap();
    QCborArray invalidWidths = widthTable.value(QStringLiteral("widths")).toArray();
    if ( !invalidWidths.isEmpty() )
    {
        invalidWidths[0] = 20000;
    }
    widthTable.insert(QStringLiteral("widths"), invalidWidths);
    widthTables.insert(QStringLiteral("snd.entries"), widthTable);
    widthPresentation.insert(QStringLiteral("tables"), widthTables);
    oversizedWidths.insert(QStringLiteral("editorPresentation"), widthPresentation);
    oversizedWidths.remove(QStringLiteral("history"));
    QString widthWarning{};
    const bool widthAccepted = ui::resources::validateResourceAuthoringPaneState(
        oversizedWidths, &widthWarning);
    check(!modeAccepted && modeWarning.contains(QStringLiteral("mode"), Qt::CaseInsensitive)
              && !widthAccepted && widthWarning.contains(QStringLiteral("width"), Qt::CaseInsensitive),
        "resource presentation rejects incomplete SND group identities and widths beyond the restorable UI bound");

    const auto withSndTable = [&captured](const QCborMap& table)
    {
        QCborMap state = captured;
        QCborMap presentation = state.value(QStringLiteral("editorPresentation")).toMap();
        QCborMap tables = presentation.value(QStringLiteral("tables")).toMap();
        tables.insert(QStringLiteral("snd.entries"), table);
        presentation.insert(QStringLiteral("tables"), tables);
        state.insert(QStringLiteral("editorPresentation"), presentation);
        state.remove(QStringLiteral("history"));
        return state;
    };
    const QCborMap capturedSoundTable = captured.value(QStringLiteral("editorPresentation")).toMap()
        .value(QStringLiteral("tables")).toMap().value(QStringLiteral("snd.entries")).toMap();
    QCborArray capturedWidthArray = capturedSoundTable.value(QStringLiteral("widths")).toArray();
    if ( capturedWidthArray.size() != 6 )
    {
        for ( const int width : capturedWidths )
        {
            capturedWidthArray.append(width);
        }
    }
    check(capturedSoundTable.value(QStringLiteral("manualWidths")).toBool()
              && capturedSoundTable.value(QStringLiteral("widths")).toArray().size() == 6,
        "resource presentation SND fixture captures six manually resized column widths");
    const auto rejectSoundTable = [&withSndTable](const QCborMap& table, const QString& expectedWarning)
    {
        QString warning{};
        const bool accepted = ui::resources::validateResourceAuthoringPaneState(
            withSndTable(table), &warning);
        return !accepted && warning.contains(expectedWarning, Qt::CaseInsensitive);
    };
    QCborMap invalidOrdinalTable = capturedSoundTable;
    QCborMap invalidOrdinalIdentity = invalidOrdinalTable.value(QStringLiteral("current")).toMap();
    invalidOrdinalIdentity.insert(QStringLiteral("sourceOrdinal"), 120);
    invalidOrdinalTable.insert(QStringLiteral("current"), invalidOrdinalIdentity);
    QCborMap invalidColumnTable = capturedSoundTable;
    invalidColumnTable.insert(QStringLiteral("column"), 6);
    QCborMap shortWidthsTable = capturedSoundTable;
    QCborArray shortWidths = capturedWidthArray;
    shortWidths.removeAt(shortWidths.size() - 1);
    shortWidthsTable.insert(QStringLiteral("manualWidths"), true);
    shortWidthsTable.insert(QStringLiteral("widths"), shortWidths);
    QCborMap belowMinimumTable = capturedSoundTable;
    QCborArray belowMinimumWidths = capturedWidthArray;
    belowMinimumWidths[0] = 79;
    belowMinimumTable.insert(QStringLiteral("manualWidths"), true);
    belowMinimumTable.insert(QStringLiteral("widths"), belowMinimumWidths);
    QCborMap excessiveTotalTable = capturedSoundTable;
    QCborArray excessiveTotalWidths{};
    for ( int column = 0; column < 6; ++column )
    {
        excessiveTotalWidths.append(12000);
    }
    excessiveTotalTable.insert(QStringLiteral("manualWidths"), true);
    excessiveTotalTable.insert(QStringLiteral("widths"), excessiveTotalWidths);
    const bool rejectsInvalidOrdinal = rejectSoundTable(invalidOrdinalTable, QStringLiteral("ordinal"));
    const bool rejectsInvalidColumn = rejectSoundTable(invalidColumnTable, QStringLiteral("column"));
    const bool rejectsShortWidths = rejectSoundTable(shortWidthsTable, QStringLiteral("width"));
    const bool rejectsMinimumWidths = rejectSoundTable(belowMinimumTable, QStringLiteral("width"));
    const bool rejectsExcessiveTotal = rejectSoundTable(excessiveTotalTable, QStringLiteral("total"));
    check(rejectsInvalidOrdinal && rejectsInvalidColumn && rejectsShortWidths
              && rejectsMinimumWidths && rejectsExcessiveTotal,
        "resource presentation rejects invalid SND source ordinals, current columns and malformed manual-width bounds atomically");

    auto target = makePane(ResourceAuthoringKind::soundClass);
    check(target && seedPane(target.get(), ResourceAuthoringKind::soundClass),
        "resource presentation SND target starts from the identical interleaved authored rows");
    if ( !target )
    {
        return;
    }
    check(ui::resources::restoreResourceAuthoringPaneState(target.get(), captured),
        "resource presentation SND restores its stable identity, filter, widths and scroll while hidden");
    const QCborMap hiddenCheckpoint = ui::resources::captureResourceAuthoringPaneState(target.get());
    const QCborMap hiddenPresentation = hiddenCheckpoint.value(QStringLiteral("editorPresentation")).toMap();
    const QCborMap hiddenTables = hiddenPresentation.value(QStringLiteral("tables")).toMap();
    const QCborMap hiddenTreeState = hiddenTables.value(QStringLiteral("snd.entries")).toMap();
    const int hiddenHorizontal = static_cast<int>(hiddenTreeState.value(QStringLiteral("horizontalScroll")).toInteger());
    const int hiddenVertical = static_cast<int>(hiddenTreeState.value(QStringLiteral("verticalScroll")).toInteger());
    check(hiddenHorizontal == sourceHorizontal && hiddenVertical == sourceVertical,
        "resource presentation captures queued SND offsets from a restored hidden target before layout clamps its bars");
    showOffscreen(target.get(), blocker, QSize(1120, 660));
    QApplication::processEvents(QEventLoop::AllEvents);
    auto* const restoredTree = target->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    auto* const restoredSearch = target->findChild<QLineEdit*>(QStringLiteral("snd.search"));
    check(restoredTree && restoredSearch,
        "resource presentation SND restored page retains its actual tree and filter widgets");
    if ( !restoredTree || !restoredSearch )
    {
        return;
    }
    QTreeWidgetItem* const restoredCurrent = findSoundItem(restoredTree,
        ui::resources::detail::SoundTreeItemKind::alternative, sourceOrdinal);
    bool widthsRestored = true;
    for ( int column = 0; column < 6; ++column )
    {
        widthsRestored = widthsRestored && restoredTree->columnWidth(column)
            == capturedWidths[static_cast<std::size_t>(column)];
    }
    const bool currentRestored = restoredCurrent && restoredTree->currentItem() == restoredCurrent
        && restoredTree->currentColumn() == 4;
    const bool noSelectionRestored = restoredTree->selectedItems().isEmpty();
    const bool filterRestored = restoredSearch->text() == QStringLiteral("create");
    const int restoredHorizontal = restoredTree->horizontalScrollBar()->value();
    const int restoredVertical = restoredTree->verticalScrollBar()->value();
    if ( !currentRestored || !noSelectionRestored || !filterRestored || !widthsRestored
        || restoredHorizontal != sourceHorizontal || restoredVertical != sourceVertical )
    {
        QList<int> actualWidths{};
        for ( int column = 0; column < 6; ++column )
        {
            actualWidths.append(restoredTree->columnWidth(column));
        }
        std::cerr << "SND editor presentation restore mismatch"
                  << " currentFound=" << (restoredCurrent != nullptr)
                  << " currentMatches=" << currentRestored
                  << " currentColumn=" << restoredTree->currentColumn()
                  << " selectionCount=" << restoredTree->selectedItems().size()
                  << " filter=" << restoredSearch->text().toStdString()
                  << " manualWidths=" << capturedSoundTable.value(QStringLiteral("manualWidths")).toBool()
                  << " expectedWidths=" << capturedWidths[0] << ',' << capturedWidths[1] << ',' << capturedWidths[2]
                  << ',' << capturedWidths[3] << ',' << capturedWidths[4] << ',' << capturedWidths[5]
                  << " actualWidths=";
        for ( const int width : actualWidths )
        {
            std::cerr << width << ',';
        }
        std::cerr << " scroll=" << restoredHorizontal << ',' << restoredVertical
                  << " expectedScroll=" << sourceHorizontal << ',' << sourceVertical
                  << " scrollRanges=" << restoredTree->horizontalScrollBar()->minimum()
                  << ',' << restoredTree->horizontalScrollBar()->maximum()
                  << ',' << restoredTree->verticalScrollBar()->minimum()
                  << ',' << restoredTree->verticalScrollBar()->maximum() << '\n';
    }
    check(restoredCurrent && restoredTree->currentItem() == restoredCurrent
              && restoredTree->currentColumn() == 4
              && restoredTree->selectedItems().isEmpty()
              && restoredSearch->text() == QStringLiteral("create")
              && widthsRestored
              && restoredTree->horizontalScrollBar()->value() == sourceHorizontal
              && restoredTree->verticalScrollBar()->value() == sourceVertical,
        "resource presentation SND restores the interleaved source row, NoSelection, manual columns and both scroll offsets");

    target->resize(820, 520);
    QApplication::processEvents(QEventLoop::AllEvents);
    for ( int column = 0; column < 6; ++column )
    {
        const int minimum = std::array<int, 6>{80, 160, 100, 56, 56, 230}
            [static_cast<std::size_t>(column)];
        check(restoredTree->columnWidth(column) == capturedWidths[static_cast<std::size_t>(column)]
                  && restoredTree->columnWidth(column) >= minimum,
            "resource presentation SND keeps a manually chosen width above its established minimum after host resize");
    }

    auto interruptedSound = makePane(ResourceAuthoringKind::soundClass);
    check(interruptedSound && seedPane(interruptedSound.get(), ResourceAuthoringKind::soundClass),
        "resource presentation SND scroll-interruption fixture starts with independent rows");
    if ( interruptedSound )
    {
        showOffscreen(interruptedSound.get(), blocker, QSize(1120, 660));
        const bool interruptedSoundRestored =
            ui::resources::restoreResourceAuthoringPaneState(interruptedSound.get(), captured);
        auto* const interruptedTree = interruptedSound->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
        const int scrollBeforeUserAction = interruptedTree
            ? interruptedTree->verticalScrollBar()->value() : 0;
        // A real Qt wheel event models user input without invoking native
        // input. The queued restore must yield to this action even while the
        // tree's first-show layout is still settling.
        // The restored offset can clamp to maximum during first-show layout, so wheel back into range.
        const int wheelDelta = interruptedTree
            && scrollBeforeUserAction == interruptedTree->verticalScrollBar()->maximum() ? 120 : -120;
        const int userScrollValue = interruptedTree
            ? sendVerticalScrollWheel(interruptedTree->verticalScrollBar(), wheelDelta) : 0;
        QApplication::processEvents(QEventLoop::AllEvents);
        const int finalScrollValue = interruptedTree
            ? interruptedTree->verticalScrollBar()->value() : 0;
        if ( !(interruptedSoundRestored && interruptedTree
                  && userScrollValue != scrollBeforeUserAction
                  && userScrollValue != sourceVertical
                  && interruptedTree->verticalScrollBar()->value() == userScrollValue) )
        {
            const int scrollMinimum = interruptedTree
                ? interruptedTree->verticalScrollBar()->minimum() : 0;
            const int scrollMaximum = interruptedTree
                ? interruptedTree->verticalScrollBar()->maximum() : 0;
            std::cerr << "SND scroll interruption mismatch"
                      << " restoredState=" << interruptedSoundRestored
                      << " range=" << scrollMinimum << ',' << scrollMaximum
                      << " prior=" << scrollBeforeUserAction
                      << " savedSourceVertical=" << sourceVertical
                      << " wheelDelta=" << wheelDelta
                      << " wheelValue=" << userScrollValue
                      << " finalValue=" << finalScrollValue << '\n';
        }
        check(interruptedSoundRestored && interruptedTree
                  && userScrollValue != scrollBeforeUserAction
                  && userScrollValue != sourceVertical
                  && interruptedTree->verticalScrollBar()->value() == userScrollValue,
            "resource presentation SND cancels queued restored scroll on a genuine wheel action");
    }

    QCborMap legacyWidths = captured;
    QCborMap legacyPresentation = legacyWidths.value(QStringLiteral("editorPresentation")).toMap();
    QCborMap legacyTables = legacyPresentation.value(QStringLiteral("tables")).toMap();
    QCborMap legacyTree = legacyTables.value(QStringLiteral("snd.entries")).toMap();
    legacyTree.remove(QStringLiteral("manualWidths"));
    legacyTree.remove(QStringLiteral("widths"));
    legacyTables.insert(QStringLiteral("snd.entries"), legacyTree);
    legacyPresentation.insert(QStringLiteral("tables"), legacyTables);
    legacyWidths.insert(QStringLiteral("editorPresentation"), legacyPresentation);
    auto legacyTarget = makePane(ResourceAuthoringKind::soundClass);
    check(legacyTarget && seedPane(legacyTarget.get(), ResourceAuthoringKind::soundClass)
              && ui::resources::restoreResourceAuthoringPaneState(legacyTarget.get(), legacyWidths),
        "resource presentation accepts a legacy SND table map with no manual-width payload");
    if ( legacyTarget )
    {
        showOffscreen(legacyTarget.get(), blocker, QSize(1120, 660));
        const QCborMap automaticLayout = ui::resources::captureResourceAuthoringPaneState(legacyTarget.get());
        const QCborMap automaticPresentation = automaticLayout.value(QStringLiteral("editorPresentation")).toMap();
        const QCborMap automaticTables = automaticPresentation.value(QStringLiteral("tables")).toMap();
        const QCborMap automaticTree = automaticTables.value(QStringLiteral("snd.entries")).toMap();
        check(!automaticTree.value(QStringLiteral("manualWidths")).toBool()
                  && !automaticTree.contains(QStringLiteral("widths")),
            "resource presentation keeps automatic SND fitting when a legacy map omitted manual widths");
    }

    sourceSearch->clear();
    QApplication::processEvents(QEventLoop::AllEvents);
    const QByteArray createMode("create");
    const std::array<std::pair<ui::resources::detail::SoundTreeItemKind, int>, 3> identities{{
        {ui::resources::detail::SoundTreeItemKind::mode, 3},
        {ui::resources::detail::SoundTreeItemKind::columnHeader, 2},
        {ui::resources::detail::SoundTreeItemKind::addAlternative, 5},
    }};
    for ( const auto& [kind, column] : identities )
    {
        QTreeWidgetItem* const sourceItem = findSoundItemForMode(sourceTree, kind, createMode);
        check(sourceItem != nullptr,
            "resource presentation SND fixture finds a group, header or footer by exact mode bytes");
        if ( !sourceItem )
        {
            continue;
        }
        sourceTree->setCurrentItem(sourceItem, column);
        const QCborMap identityState = ui::resources::captureResourceAuthoringPaneState(source.get());
        auto identityTarget = makePane(ResourceAuthoringKind::soundClass);
        check(identityTarget && seedPane(identityTarget.get(), ResourceAuthoringKind::soundClass),
            "resource presentation SND exact-mode restore uses an independent sound tree");
        if ( !identityTarget )
        {
            continue;
        }
        check(ui::resources::restoreResourceAuthoringPaneState(identityTarget.get(), identityState),
            "resource presentation restores an SND item represented by exact mode bytes");
        auto* const identityTree = identityTarget->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
        QTreeWidgetItem* const restoredItem = findSoundItemForMode(identityTree, kind, createMode);
        check(identityTree && restoredItem && identityTree->currentItem() == restoredItem
                  && identityTree->currentColumn() == column
                  && identityTree->selectedItems().isEmpty(),
            "resource presentation preserves SND group, header and footer identity with distinct current columns");
    }
}

void testLiveUniDelegateCaptureAndDeferredCaret(SpriteGlViewEventBlocker& blocker)
{
    auto source = makePane(ResourceAuthoringKind::stringTable);
    check(source && seedPane(source.get(), ResourceAuthoringKind::stringTable),
        "resource presentation delegate fixture seeds a clean UNI document");
    if ( !source )
    {
        return;
    }
    showOffscreen(source.get(), blocker, QSize(760, 460));
    auto* const sourceTable = source->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    check(sourceTable && sourceTable->rowCount() > 0,
        "resource presentation delegate fixture finds the real UNI editable table");
    if ( !sourceTable )
    {
        return;
    }
    sourceTable->setCurrentCell(3, 1);
    sourceTable->editItem(sourceTable->item(3, 1));
    QApplication::processEvents(QEventLoop::AllEvents);
    QLineEdit* const sourceEditor = qobject_cast<QLineEdit*>(sourceTable->indexWidget(sourceTable->currentIndex()));
    check(sourceEditor != nullptr,
        "resource presentation delegate fixture opens an ordinary live UNI delegate editor");
    if ( !sourceEditor )
    {
        return;
    }
    const QString rawBuffer = QStringLiteral("unfinished table cell buffer");
    sourceEditor->setText(rawBuffer);
    setReverseSelection(sourceEditor, rawBuffer.size(), 7);
    const int rawCursor = sourceEditor->cursorPosition();
    const int rawAnchor = sourceEditor->selectionStart() + sourceEditor->selectedText().size();
    QWidget* const focusBeforeCapture = QApplication::focusWidget();
    const QString authoredCellBeforeCapture = sourceTable->item(3, 1)->text();
    const QCborMap beforeCapture = ui::resources::captureResourceAuthoringPaneState(source.get());
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(source.get());
    const QCborArray capturedRows = captured.value(QStringLiteral("rows")).toArray();
    const QLineEdit* const liveAfterCapture = qobject_cast<QLineEdit*>(sourceTable->indexWidget(sourceTable->currentIndex()));
    const bool rawBufferCaptured = capturedRows.size() > 3
        && capturedRows.at(3).toArray().size() > 1
        && capturedRows.at(3).toArray().at(1).toString() == rawBuffer;
    check(rawBufferCaptured && liveAfterCapture == sourceEditor
              && sourceEditor->text() == rawBuffer
              && sourceEditor->cursorPosition() == rawCursor && hasReverseSelection(sourceEditor)
              && QApplication::focusWidget() == focusBeforeCapture
              && sourceTable->item(3, 1)->text() == authoredCellBeforeCapture
              && captured.value(QStringLiteral("modified")) == beforeCapture.value(QStringLiteral("modified"))
              && captured.value(QStringLiteral("history")) == beforeCapture.value(QStringLiteral("history")),
        "resource presentation capture reads raw delegate text and caret without committing, refocusing or advancing history");

    auto target = makePane(ResourceAuthoringKind::stringTable);
    check(target && seedPane(target.get(), ResourceAuthoringKind::stringTable),
        "resource presentation delegate restore target begins with an independent table draft");
    if ( !target )
    {
        return;
    }
    auto* const targetTable = target->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    check(targetTable != nullptr, "resource presentation delegate target retains its actual UNI table");
    if ( !targetTable )
    {
        return;
    }
    QWidget* const focusBeforeRestore = QApplication::focusWidget();
    check(ui::resources::restoreResourceAuthoringPaneState(target.get(), captured)
              && QApplication::focusWidget() == focusBeforeRestore
              && targetTable->indexWidget(targetTable->model()->index(3, 1)) == nullptr,
        "resource presentation restore stores raw delegate state without creating an editor or changing focus");

    const QCborMap checkpointTwo = ui::resources::captureResourceAuthoringPaneState(target.get());
    const QCborMap checkpointTwoPresentation = checkpointTwo.value(QStringLiteral("editorPresentation")).toMap();
    const QCborMap checkpointTwoEditors = checkpointTwoPresentation.value(QStringLiteral("tableEditors")).toMap();
    const QCborMap checkpointTwoCaret = checkpointTwoEditors.value(QStringLiteral("uni.entries/3/1")).toMap();
    check(checkpointTwoCaret.value(QStringLiteral("cursor")).toInteger() == rawCursor
              && checkpointTwoCaret.value(QStringLiteral("anchor")).toInteger() == rawAnchor,
        "resource presentation preserves a deferred table-editor caret through a second native checkpoint");

    auto secondTarget = makePane(ResourceAuthoringKind::stringTable);
    check(secondTarget && seedPane(secondTarget.get(), ResourceAuthoringKind::stringTable),
        "resource presentation double-checkpoint restore target starts with an independent UNI document");
    if ( !secondTarget )
    {
        return;
    }
    auto* const secondTable = secondTarget->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    check(secondTable != nullptr, "resource presentation double-checkpoint target retains its actual table");
    if ( !secondTable )
    {
        return;
    }
    const QWidget* const focusBeforeSecondRestore = QApplication::focusWidget();
    check(ui::resources::restoreResourceAuthoringPaneState(secondTarget.get(), checkpointTwo)
              && QApplication::focusWidget() == focusBeforeSecondRestore
              && secondTable->indexWidget(secondTable->model()->index(3, 1)) == nullptr,
        "resource presentation second restore keeps deferred caret state without opening a delegate");

    showOffscreen(secondTarget.get(), blocker, QSize(760, 460));
    secondTable->setCurrentCell(3, 1);
    secondTable->editItem(secondTable->item(3, 1));
    QApplication::processEvents(QEventLoop::AllEvents);
    QLineEdit* const restoredEditor = qobject_cast<QLineEdit*>(secondTable->indexWidget(secondTable->currentIndex()));
    const bool caretRestoredOnOrdinaryEdit = restoredEditor
        && restoredEditor->text() == rawBuffer && restoredEditor->cursorPosition() == rawCursor
        && restoredEditor->selectionStart() + restoredEditor->selectedText().size() == rawAnchor
        && hasReverseSelection(restoredEditor);
    check(caretRestoredOnOrdinaryEdit,
        "resource presentation applies a saved delegate reverse caret only after the next ordinary setEditorData pass");
}

void testLiveSndDelegateCaptureAndDeferredCarets(SpriteGlViewEventBlocker& blocker)
{
    const auto exercise = [&blocker](const int column, const QString& rawBuffer)
    {
        auto source = makePane(ResourceAuthoringKind::soundClass);
        check(source && seedPane(source.get(), ResourceAuthoringKind::soundClass),
            "resource presentation SND delegate fixture seeds a clean source document");
        if ( !source )
        {
            return;
        }
        showOffscreen(source.get(), blocker, QSize(1120, 660));
        auto* const sourceTree = source->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
        QTreeWidgetItem* const sourceItem = findSoundItem(sourceTree,
            ui::resources::detail::SoundTreeItemKind::alternative, 83);
        check(sourceTree && sourceItem,
            "resource presentation SND delegate fixture finds its source row by the ordinal role");
        if ( !sourceTree || !sourceItem )
        {
            return;
        }
        sourceTree->setCurrentItem(sourceItem, column);
        sourceTree->editItem(sourceItem, column);
        QApplication::processEvents(QEventLoop::AllEvents);
        QWidget* const sourceWidget = sourceTree->indexWidget(sourceTree->currentIndex());
        QLineEdit* sourceEdit{};
        if ( auto* const scrub = qobject_cast<ui::ScrubFloatEdit*>(sourceWidget) )
        {
            sourceEdit = scrub->lineEdit();
        }
        else if ( sourceWidget )
        {
            sourceEdit = sourceWidget->findChild<QLineEdit*>(QStringLiteral("snd.volume.text"));
        }
        check(sourceWidget && sourceEdit,
            "resource presentation SND creates its ordinary Min scrub or Volume value editor");
        if ( !sourceWidget || !sourceEdit )
        {
            return;
        }
        sourceEdit->setText(rawBuffer);
        sourceWidget->setProperty("snd.numeric.pending", true);
        setReverseSelection(sourceEdit, rawBuffer.size(), 4);
        const int expectedCursor = rawBuffer.size() - 4;
        const int expectedAnchor = rawBuffer.size();
        const QString authoredBeforeCapture = sourceItem->text(column);
        QWidget* const focusBeforeCapture = QApplication::focusWidget();
        const QCborMap beforeCapture = ui::resources::captureResourceAuthoringPaneState(source.get());
        const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(source.get());
        const QCborArray rows = captured.value(QStringLiteral("rows")).toArray();
        const QCborMap editorPresentation = captured.value(QStringLiteral("editorPresentation")).toMap();
        const QCborMap tableEditors = editorPresentation.value(QStringLiteral("tableEditors")).toMap();
        const QString key = QStringLiteral("snd.entries/83/%1").arg(column);
        const QCborMap caret = tableEditors.value(key).toMap();
        check(rows.size() > 83 && rows.at(83).toArray().size() > column
                  && rows.at(83).toArray().at(column).toString() == rawBuffer
                  && caret.value(QStringLiteral("cursor")).toInteger() == expectedCursor
                  && caret.value(QStringLiteral("anchor")).toInteger() == expectedAnchor
                  && sourceEdit->text() == rawBuffer && hasReverseSelection(sourceEdit)
                  && sourceItem->text(column) == authoredBeforeCapture
                  && QApplication::focusWidget() == focusBeforeCapture
                  && captured.value(QStringLiteral("modified")) == beforeCapture.value(QStringLiteral("modified"))
                  && captured.value(QStringLiteral("history")) == beforeCapture.value(QStringLiteral("history")),
            "resource presentation captures invalid pending SND raw text and caret without commit, focus or history side effects");

        auto firstTarget = makePane(ResourceAuthoringKind::soundClass);
        check(firstTarget && seedPane(firstTarget.get(), ResourceAuthoringKind::soundClass)
                  && ui::resources::restoreResourceAuthoringPaneState(firstTarget.get(), captured),
            "resource presentation restores a pending SND delegate buffer without opening its editor");
        if ( !firstTarget )
        {
            return;
        }
        const QCborMap secondCheckpoint = ui::resources::captureResourceAuthoringPaneState(firstTarget.get());
        const QCborMap secondPresentation = secondCheckpoint.value(QStringLiteral("editorPresentation")).toMap();
        const QCborMap secondTableEditors = secondPresentation.value(QStringLiteral("tableEditors")).toMap();
        const QCborMap secondCaret = secondTableEditors.value(key).toMap();
        check(secondCaret.value(QStringLiteral("cursor")).toInteger() == expectedCursor
                  && secondCaret.value(QStringLiteral("anchor")).toInteger() == expectedAnchor,
            "resource presentation preserves the deferred SND caret across another capture");

        auto secondTarget = makePane(ResourceAuthoringKind::soundClass);
        check(secondTarget && seedPane(secondTarget.get(), ResourceAuthoringKind::soundClass)
                  && ui::resources::restoreResourceAuthoringPaneState(secondTarget.get(), secondCheckpoint),
            "resource presentation SND second checkpoint restores with no delegate editor created");
        if ( !secondTarget )
        {
            return;
        }
        showOffscreen(secondTarget.get(), blocker, QSize(1120, 660));
        auto* const restoredTree = secondTarget->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
        QTreeWidgetItem* const restoredItem = findSoundItem(restoredTree,
            ui::resources::detail::SoundTreeItemKind::alternative, 83);
        check(restoredTree && restoredItem,
            "resource presentation SND second restore retains the stable source row");
        if ( !restoredTree || !restoredItem )
        {
            return;
        }
        restoredTree->setCurrentItem(restoredItem, column);
        restoredTree->editItem(restoredItem, column);
        QApplication::processEvents(QEventLoop::AllEvents);
        QWidget* const restoredWidget = restoredTree->indexWidget(restoredTree->currentIndex());
        QLineEdit* restoredEdit{};
        if ( auto* const scrub = qobject_cast<ui::ScrubFloatEdit*>(restoredWidget) )
        {
            restoredEdit = scrub->lineEdit();
        }
        else if ( restoredWidget )
        {
            restoredEdit = restoredWidget->findChild<QLineEdit*>(QStringLiteral("snd.volume.text"));
        }
        check(restoredEdit && restoredEdit->text() == rawBuffer
                  && restoredEdit->cursorPosition() == expectedCursor
                  && restoredEdit->selectionStart() + restoredEdit->selectedText().size() == expectedAnchor
                  && hasReverseSelection(restoredEdit),
            "resource presentation restores deferred caret in SND Min and Volume editors after ordinary setEditorData");
    };

    exercise(3, QStringLiteral("unfinished Min buffer"));
    exercise(5, QStringLiteral("unfinished Volume buffer"));
}

void testFailedLooseReloadKeepsEditorPresentation(SpriteGlViewEventBlocker& blocker)
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "resource presentation failed-reload fixture gets a private source directory");
    if ( !temporary.isValid() )
    {
        return;
    }
    const std::filesystem::path sourcePath = std::filesystem::path(temporary.path().toStdWString())
        / "presentation.snd";
    {
        std::ofstream output(sourcePath, std::ios::binary);
        output << "moving reload.wav 0x00000040 0.5 2.5 1\n";
    }
    ui::resources::ResourceAuthoringContext context = testContext();
    context.projectDirectory = sourcePath.parent_path();
    context.startingDirectory = sourcePath.parent_path();
    std::unique_ptr<QWidget> pane(ui::resources::createResourceAuthoringPane(
        nullptr, ResourceAuthoringKind::soundClass, context));
    check(pane != nullptr, "resource presentation failed-reload fixture creates an SND pane");
    if ( !pane )
    {
        return;
    }
    showOffscreen(pane.get(), blocker, QSize(1120, 660));

    int fileSelections{};
    QTimer openTimer{};
    QObject::connect(&openTimer, &QTimer::timeout, [&]()
    {
        if ( auto* const dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()) )
        {
            ++fileSelections;
            dialog->selectFile(QString::fromStdWString(sourcePath.wstring()));
            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        }
        else if ( auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            message->reject();
        }
    });
    openTimer.start(5);
    const bool opened = ui::resources::resourceAuthoringPaneOpenLooseFileDirect(pane.get());
    openTimer.stop();
    auto* const tree = pane->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    check(opened && fileSelections == 1 && tree,
        "resource presentation failed-reload fixture opens a real loose SND source through the pane API");
    if ( !opened || !tree )
    {
        return;
    }
    tree->header()->resizeSection(1, 340);
    QTreeWidgetItem* const item = findSoundItem(tree,
        ui::resources::detail::SoundTreeItemKind::alternative, 0);
    check(item != nullptr, "resource presentation failed-reload fixture finds the loaded SND source row");
    if ( !item )
    {
        return;
    }
    tree->setCurrentItem(item, 3);
    tree->editItem(item, 3);
    QApplication::processEvents(QEventLoop::AllEvents);
    auto* const scrub = qobject_cast<ui::ScrubFloatEdit*>(tree->indexWidget(tree->currentIndex()));
    QLineEdit* const edit = scrub ? scrub->lineEdit() : nullptr;
    check(edit != nullptr, "resource presentation failed-reload fixture opens the ordinary SND Min editor");
    if ( !edit )
    {
        return;
    }
    setReverseSelection(edit, edit->text().size(), 1);
    const QCborMap liveState = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QString pendingKey = QStringLiteral("snd.entries/0/3");
    const QCborMap livePresentation = liveState.value(QStringLiteral("editorPresentation")).toMap();
    check(livePresentation.value(QStringLiteral("tableEditors")).toMap().contains(pendingKey)
              && livePresentation.value(QStringLiteral("tables")).toMap()
                  .value(QStringLiteral("snd.entries")).toMap().value(QStringLiteral("manualWidths")).toBool(),
        "resource presentation failed-reload fixture establishes both pending delegate caret and manual widths");
    QString restoreWarning{};
    const bool restoredDeferredState = ui::resources::restoreResourceAuthoringPaneState(
        pane.get(), liveState, &restoreWarning);
    const QCborMap beforeFailure = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap beforePresentation = beforeFailure.value(QStringLiteral("editorPresentation")).toMap();
    const bool deferredCaretRestored = beforePresentation.value(QStringLiteral("tableEditors")).toMap()
        .contains(pendingKey);
    {
        std::ofstream output(sourcePath, std::ios::binary | std::ios::trunc);
        output << "moving reload.wav 0x40 0.5 invalid 1\n";
    }
    QString reloadError{};
    const bool reloaded = ui::resources::resourceAuthoringPaneReloadCurrentLooseSourceForTest(
        pane.get(), &reloadError);
    const QCborMap afterFailure = ui::resources::captureResourceAuthoringPaneState(pane.get());
    const QCborMap afterPresentation = afterFailure.value(QStringLiteral("editorPresentation")).toMap();
    check(restoredDeferredState && deferredCaretRestored && !reloaded && !reloadError.isEmpty()
              && afterFailure == beforeFailure
              && afterPresentation.value(QStringLiteral("tableEditors")).toMap().contains(pendingKey)
              && afterPresentation.value(QStringLiteral("tables")).toMap()
                  .value(QStringLiteral("snd.entries")).toMap().value(QStringLiteral("manualWidths")).toBool(),
        "resource presentation failed loose reload leaves the complete captured pane, deferred caret and manual widths unchanged");
}

void testSuccessfulSndSaveInvalidatesDeferredCaret(SpriteGlViewEventBlocker& blocker)
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "resource presentation SND save fixture gets a private source directory");
    if ( !temporary.isValid() )
    {
        return;
    }
    const std::filesystem::path root(temporary.path().toStdWString());
    const std::filesystem::path sourcePath = root / "caret-save.snd";
    constexpr std::string_view originalBytes =
        "moving presentation.wav 0x00000040 0.5 2.5 1\n";
    bool sourceWritten{};
    {
        std::ofstream output(sourcePath, std::ios::binary);
        output << originalBytes;
        sourceWritten = static_cast<bool>(output);
    }
    check(sourceWritten, "resource presentation SND save fixture writes its synthetic source");
    if ( !sourceWritten )
    {
        return;
    }
    ui::resources::ResourceAuthoringContext context = testContext();
    context.projectDirectory = root;
    context.startingDirectory = root;
    std::unique_ptr<QWidget> source(ui::resources::createResourceAuthoringPane(
        nullptr, ResourceAuthoringKind::soundClass, context));
    check(source != nullptr, "resource presentation SND save fixture creates its source pane");
    if ( !source )
    {
        return;
    }
    showOffscreen(source.get(), blocker, QSize(1120, 660));

    int fileSelections{};
    QTimer openTimer{};
    QObject::connect(&openTimer, &QTimer::timeout, [&]()
    {
        if ( auto* const dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()) )
        {
            ++fileSelections;
            dialog->selectFile(QString::fromStdWString(sourcePath.wstring()));
            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        }
        else if ( auto* const message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()) )
        {
            message->reject();
        }
    });
    openTimer.start(5);
    const bool opened = ui::resources::resourceAuthoringPaneOpenLooseFileDirect(source.get());
    openTimer.stop();
    auto* const sourceTree = source->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    QTreeWidgetItem* const sourceItem = findSoundItem(sourceTree,
        ui::resources::detail::SoundTreeItemKind::alternative, 0);
    check(opened && fileSelections == 1 && sourceTree && sourceItem,
        "resource presentation SND save fixture opens the exact loose source row");
    if ( !opened || !sourceTree || !sourceItem )
    {
        return;
    }

    sourceTree->setCurrentItem(sourceItem, 3);
    sourceTree->editItem(sourceItem, 3);
    QApplication::processEvents(QEventLoop::AllEvents);
    auto* const sourceScrub = qobject_cast<ui::ScrubFloatEdit*>(
        sourceTree->indexWidget(sourceTree->currentIndex()));
    QLineEdit* const sourceEdit = sourceScrub ? sourceScrub->lineEdit() : nullptr;
    check(sourceEdit != nullptr, "resource presentation SND save fixture opens the ordinary Min editor");
    if ( !sourceEdit )
    {
        return;
    }
    sourceEdit->setText(QStringLiteral("+0.25"));
    sourceScrub->setProperty("snd.numeric.pending", true);
    sourceEdit->setCursorPosition(5);
    const QString editorKey = QStringLiteral("snd.entries/0/3");
    const QCborMap capturedSource = ui::resources::captureResourceAuthoringPaneState(source.get());
    const QCborArray sourceRows = capturedSource.value(QStringLiteral("rows")).toArray();
    const QCborArray sourceRow = sourceRows.size() == 1 ? sourceRows.at(0).toArray() : QCborArray{};
    const QCborMap sourceTableEditors = capturedSource.value(QStringLiteral("editorPresentation")).toMap()
        .value(QStringLiteral("tableEditors")).toMap();
    const QCborMap sourceCaret = sourceTableEditors.value(editorKey).toMap();
    const bool sourceDraftCaptured = sourceRow.size() > 3
        && sourceRow.at(3).toString() == QStringLiteral("+0.25")
              && sourceCaret.value(QStringLiteral("cursor")).toInteger() == 5
              && sourceCaret.value(QStringLiteral("anchor")).toInteger() == 5
              && ui::resources::validateResourceAuthoringPaneState(capturedSource);
    check(sourceDraftCaptured,
        "resource presentation captures the valid raw Min draft and its deferred end caret before restore");
    if ( !sourceDraftCaptured )
    {
        return;
    }

    int deniedDestinationChecks{};
    ui::resources::ResourceAuthoringContext deniedContext = context;
    deniedContext.destinationAvailable = [&deniedDestinationChecks](QWidget*, const std::filesystem::path&)
    {
        ++deniedDestinationChecks;
        return false;
    };
    std::unique_ptr<QWidget> failedPane(ui::resources::createResourceAuthoringPane(
        nullptr, ResourceAuthoringKind::soundClass, deniedContext));
    check(failedPane != nullptr, "resource presentation SND save fixture creates a denied-destination pane");
    if ( !failedPane )
    {
        return;
    }
    QString restoreWarning{};
    const bool restored = ui::resources::restoreResourceAuthoringPaneState(
        failedPane.get(), capturedSource, &restoreWarning);
    const QCborMap beforeFailedSave = ui::resources::captureResourceAuthoringPaneState(failedPane.get());
    const bool restoredCaretIsDeferred = beforeFailedSave.value(QStringLiteral("editorPresentation")).toMap()
        .value(QStringLiteral("tableEditors")).toMap().contains(editorKey);
    check(restored && restoredCaretIsDeferred
              && ui::resources::validateResourceAuthoringPaneState(beforeFailedSave),
        "resource presentation restores the SND Min draft and caret as deferred pane state");
    if ( !restored )
    {
        return;
    }
    const bool modifiedBeforeFailure = ui::resources::resourceAuthoringPaneIsModified(failedPane.get());
    const bool failedSave = !ui::resources::resourceAuthoringPaneSave(failedPane.get(), false);
    const QCborMap afterFailedSave = ui::resources::captureResourceAuthoringPaneState(failedPane.get());
    const assets::SoundClassFile diskAfterFailedSave = assets::loadFromSoundClass(
        sourcePath, assets::SoundClassProfile::classic);
    check(failedSave && deniedDestinationChecks == 1 && afterFailedSave == beforeFailedSave
              && diskAfterFailedSave.entries.size() == 1
              && diskAfterFailedSave.entries.front().minRadius == 0.5f
              && ui::resources::resourceAuthoringPaneIsModified(failedPane.get()) == modifiedBeforeFailure,
        "resource presentation preserves the complete deferred SND draft and caret when loose Save is denied");

    MainWindow targetWindow{};
    initializeMainWindow(targetWindow, root);
    blocker.watch(&targetWindow);
    QWidget* const target = createMainWindowResourcePage(targetWindow,
        QStringLiteral("tools.resource_editors.sound_class_snd.new"),
        QStringLiteral("soundClassResourceEditorsDock"), QStringLiteral("soundClassResourceEditorTabs"));
    check(target != nullptr, "resource presentation SND save fixture creates an aggregate EDIT pane");
    if ( !target )
    {
        return;
    }
    targetWindow.markCurrentDocumentAsSavedForTest();
    blocker.watch(target);
    const bool restoredToAggregate = ui::resources::restoreResourceAuthoringPaneState(
        target, capturedSource, &restoreWarning);
    check(restoredToAggregate,
        "resource presentation restores the SND raw draft into a real aggregate EDIT pane");
    if ( !restoredToAggregate )
    {
        return;
    }

    const bool successfulSave = ui::resources::resourceAuthoringPaneSave(target, false);
    const assets::SoundClassFile savedFile = assets::loadFromSoundClass(
        sourcePath, assets::SoundClassProfile::classic);
    const QCborMap afterSuccessfulSave = ui::resources::captureResourceAuthoringPaneState(target);
    const QCborArray savedRows = afterSuccessfulSave.value(QStringLiteral("rows")).toArray();
    const QCborArray savedRow = savedRows.size() == 1 ? savedRows.at(0).toArray() : QCborArray{};
    const QCborMap savedPresentation = afterSuccessfulSave.value(QStringLiteral("editorPresentation")).toMap();
    const bool savedPaneValid = ui::resources::validateResourceAuthoringPaneState(afterSuccessfulSave);
    const QByteArray aggregatePayload = targetWindow.captureRecoveryEditorStateForTest();
    QString aggregateWarning{};
    const bool aggregateValid = !aggregatePayload.isEmpty()
        && targetWindow.validateRecoveryEditorStateForTest(aggregatePayload, &aggregateWarning);
    check(successfulSave
              && savedFile.entries.size() == 1 && savedFile.entries.front().minRadius == 0.25f
              && savedRow.size() > 3 && savedRow.at(3).toString() == QStringLiteral("0.25")
              && !savedPresentation.value(QStringLiteral("tableEditors")).toMap().contains(editorKey)
              && savedPaneValid && aggregateValid,
        "resource presentation successful SND loose Save canonicalizes the Min draft, clears its stale caret, and keeps pane and aggregate EDIT valid");

    const QCborMap roundTripState = nativeEditSectionRoundTrip(
        QCborValue::fromCbor(aggregatePayload).toMap());
    const QByteArray transportedPayload = QCborValue(roundTripState).toCbor();
    MainWindow restoredWindow{};
    initializeMainWindow(restoredWindow, root);
    blocker.watch(&restoredWindow);
    QString transportedWarning{};
    check(!roundTripState.isEmpty()
              && restoredWindow.validateRecoveryEditorStateForTest(transportedPayload, &transportedWarning)
              && restoredWindow.restoreRecoveryEditorStateForTest(transportedPayload, &transportedWarning),
        "resource presentation successful SND Save remains valid through native EDIT transport and restore");
}

void testUndoRedoInvalidatesDeferredTableCaret(SpriteGlViewEventBlocker& blocker)
{
    auto source = makePane(ResourceAuthoringKind::stringTable);
    check(source && seedPane(source.get(), ResourceAuthoringKind::stringTable),
        "resource presentation history fixture seeds a clean UNI source");
    if ( !source )
    {
        return;
    }
    showOffscreen(source.get(), blocker, QSize(760, 460));
    auto* const sourceTable = source->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    check(sourceTable && sourceTable->item(3, 1),
        "resource presentation history fixture finds its real UNI editable row");
    if ( !sourceTable || !sourceTable->item(3, 1) )
    {
        return;
    }
    const QString longAuthoredText = QStringLiteral("a longer authored UNI value for deferred caret history");
    sourceTable->item(3, 1)->setText(longAuthoredText);
    sourceTable->setCurrentCell(3, 1);
    sourceTable->editItem(sourceTable->item(3, 1));
    QApplication::processEvents(QEventLoop::AllEvents);
    QLineEdit* const sourceEditor = qobject_cast<QLineEdit*>(sourceTable->indexWidget(sourceTable->currentIndex()));
    check(sourceEditor && sourceEditor->text() == longAuthoredText,
        "resource presentation history fixture opens a delegate over the longer authored cell");
    if ( !sourceEditor )
    {
        return;
    }
    setReverseSelection(sourceEditor, longAuthoredText.size(), 5);
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(source.get());

    auto target = makePane(ResourceAuthoringKind::stringTable);
    check(target && seedPane(target.get(), ResourceAuthoringKind::stringTable)
              && ui::resources::restoreResourceAuthoringPaneState(target.get(), captured),
        "resource presentation history target restores a pending caret without opening its delegate");
    if ( !target )
    {
        return;
    }
    auto* const targetTable = target->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    const QModelIndex targetIndex = targetTable ? targetTable->model()->index(3, 1) : QModelIndex{};
    check(targetTable && targetTable->indexWidget(targetIndex) == nullptr,
        "resource presentation history target keeps the restored delegate closed before Undo");
    if ( !targetTable )
    {
        return;
    }
    const bool undone = ui::resources::resourceAuthoringPaneUndoRedo(target.get(), false);
    const QCborMap afterUndo = ui::resources::captureResourceAuthoringPaneState(target.get());
    QString undoWarning{};
    const bool undoStateValid = ui::resources::validateResourceAuthoringPaneState(afterUndo, &undoWarning);
    const QCborMap afterUndoPresentation = afterUndo.value(QStringLiteral("editorPresentation")).toMap();
    const QCborMap afterUndoEditors = afterUndoPresentation.value(QStringLiteral("tableEditors")).toMap();
    const bool staleCaretClearedOnUndo = !afterUndoEditors.contains(QStringLiteral("uni.entries/3/1"));

    const bool redone = ui::resources::resourceAuthoringPaneUndoRedo(target.get(), true);
    const QCborMap afterRedo = ui::resources::captureResourceAuthoringPaneState(target.get());
    QString redoWarning{};
    const bool redoStateValid = ui::resources::validateResourceAuthoringPaneState(afterRedo, &redoWarning);
    const QCborMap afterRedoEditors = afterRedo.value(QStringLiteral("editorPresentation")).toMap()
        .value(QStringLiteral("tableEditors")).toMap();
    check(undone && undoStateValid && staleCaretClearedOnUndo
              && afterUndo.value(QStringLiteral("rows")).toArray().at(3).toArray().at(1).toString()
                  != longAuthoredText
              && redone && redoStateValid && !afterRedoEditors.contains(QStringLiteral("uni.entries/3/1")),
        "resource presentation invalidates stale deferred carets across nonstructural UNI Undo and Redo and keeps both captures valid");

    auto deletionTarget = makePane(ResourceAuthoringKind::stringTable);
    check(deletionTarget && seedPane(deletionTarget.get(), ResourceAuthoringKind::stringTable)
              && ui::resources::restoreResourceAuthoringPaneState(deletionTarget.get(), captured),
        "resource presentation structural-deletion target restores a pending table caret");
    if ( !deletionTarget )
    {
        return;
    }
    auto* const deletionTable = deletionTarget->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    auto* const removeButton = deletionTarget->findChild<QPushButton*>(QStringLiteral("uni.remove"));
    if ( deletionTable && removeButton )
    {
        deletionTable->setCurrentCell(3, 0);
        removeButton->click();
    }
    const QCborMap afterDeletion = ui::resources::captureResourceAuthoringPaneState(deletionTarget.get());
    const QCborMap deletionEditors = afterDeletion.value(QStringLiteral("editorPresentation")).toMap()
        .value(QStringLiteral("tableEditors")).toMap();
    check(deletionTable && removeButton && !deletionEditors.contains(QStringLiteral("uni.entries/3/1")),
        "resource presentation structural row deletion clears deferred carets before their source ordinals can shift");
}

void testSoundUndoRedoInvalidatesDeferredTableCaret(SpriteGlViewEventBlocker& blocker)
{
    auto source = makePane(ResourceAuthoringKind::soundClass);
    check(source && seedPane(source.get(), ResourceAuthoringKind::soundClass),
        "resource presentation SND history fixture seeds a clean sound document");
    if ( !source )
    {
        return;
    }
    showOffscreen(source.get(), blocker, QSize(1120, 660));
    auto* const sourceTree = source->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    QTreeWidgetItem* const sourceItem = findSoundItem(sourceTree,
        ui::resources::detail::SoundTreeItemKind::alternative, 83);
    check(sourceTree && sourceItem,
        "resource presentation SND history fixture finds its stable Min cell");
    if ( !sourceTree || !sourceItem )
    {
        return;
    }

    const QString longMin = QStringLiteral("1234567.890123456");
    sourceItem->setText(3, longMin);
    sourceTree->setCurrentItem(sourceItem, 3);
    sourceTree->editItem(sourceItem, 3);
    QApplication::processEvents(QEventLoop::AllEvents);
    QWidget* const sourceWidget = sourceTree->indexWidget(sourceTree->currentIndex());
    auto* const sourceScrub = qobject_cast<ui::ScrubFloatEdit*>(sourceWidget);
    QLineEdit* const sourceEdit = sourceScrub ? sourceScrub->lineEdit() : nullptr;
    check(sourceEdit && ui::resources::resourceAuthoringPaneCanUndoRedo(source.get(), false),
        "resource presentation SND history fixture has an ordinary Min editor and an undoable longer value");
    if ( !sourceEdit )
    {
        return;
    }
    sourceEdit->setText(longMin);
    sourceWidget->setProperty("snd.numeric.pending", true);
    sourceEdit->setCursorPosition(longMin.size());
    const QString editorKey = QStringLiteral("snd.entries/83/3");
    const QCborMap captured = ui::resources::captureResourceAuthoringPaneState(source.get());
    const QCborMap capturedEditors = captured.value(QStringLiteral("editorPresentation")).toMap()
        .value(QStringLiteral("tableEditors")).toMap();
    const QCborMap capturedCaret = capturedEditors.value(editorKey).toMap();
    check(capturedCaret.value(QStringLiteral("cursor")).toInteger() == longMin.size()
              && capturedCaret.value(QStringLiteral("anchor")).toInteger() == longMin.size(),
        "resource presentation captures an end caret in the longer live SND Min draft");

    auto target = makePane(ResourceAuthoringKind::soundClass);
    check(target && seedPane(target.get(), ResourceAuthoringKind::soundClass)
              && ui::resources::restoreResourceAuthoringPaneState(target.get(), captured),
        "resource presentation SND history target restores the pending Min caret without opening its editor");
    if ( !target )
    {
        return;
    }
    auto* const targetTree = target->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    QTreeWidgetItem* const targetItem = findSoundItem(targetTree,
        ui::resources::detail::SoundTreeItemKind::alternative, 83);
    const QModelIndex targetIndex = targetTree && targetItem
        ? targetTree->indexFromItem(targetItem, 3) : QModelIndex{};
    const QCborMap restoredBeforeUndo = ui::resources::captureResourceAuthoringPaneState(target.get());
    const QCborMap restoredBeforeUndoEditors = restoredBeforeUndo.value(QStringLiteral("editorPresentation")).toMap()
        .value(QStringLiteral("tableEditors")).toMap();
    check(targetTree && targetTree->indexWidget(targetIndex) == nullptr
              && restoredBeforeUndoEditors.contains(editorKey)
              && ui::resources::resourceAuthoringPaneCanUndoRedo(target.get(), false),
        "resource presentation keeps restored SND Min caret deferred while its sound history is undoable");

    const bool undone = ui::resources::resourceAuthoringPaneUndoRedo(target.get(), false);
    const QCborMap afterUndo = ui::resources::captureResourceAuthoringPaneState(target.get());
    QString undoWarning{};
    const bool undoStateValid = ui::resources::validateResourceAuthoringPaneState(afterUndo, &undoWarning);
    const QCborMap afterUndoEditors = afterUndo.value(QStringLiteral("editorPresentation")).toMap()
        .value(QStringLiteral("tableEditors")).toMap();
    const bool staleCaretClearedOnUndo = !afterUndoEditors.contains(editorKey);

    const bool redone = ui::resources::resourceAuthoringPaneUndoRedo(target.get(), true);
    const QCborMap afterRedo = ui::resources::captureResourceAuthoringPaneState(target.get());
    QString redoWarning{};
    const bool redoStateValid = ui::resources::validateResourceAuthoringPaneState(afterRedo, &redoWarning);
    const QCborArray undoRows = afterUndo.value(QStringLiteral("rows")).toArray();
    const QCborArray redoRows = afterRedo.value(QStringLiteral("rows")).toArray();
    const QCborMap afterRedoEditors = afterRedo.value(QStringLiteral("editorPresentation")).toMap()
        .value(QStringLiteral("tableEditors")).toMap();
    check(undone && undoStateValid && staleCaretClearedOnUndo
              && undoRows.size() > 83 && undoRows.at(83).toArray().at(3).toString() != longMin
              && redone && redoStateValid && !afterRedoEditors.contains(editorKey)
              && redoRows.size() > 83 && redoRows.at(83).toArray().at(3).toString() == longMin,
        "resource presentation invalidates the stale SND Min caret across nonstructural Undo and Redo and keeps both captures valid");
}

void initializeMainWindow(MainWindow& window, const std::filesystem::path& root)
{
    editor::DocumentState document{};
    document.level.kind = ProjectType::JKDF2;
    document.level.header.version = 1;
    document.level.layers = {"Base"};
    window.setDocumentStateForTest(std::move(document));
    window.setProjectTypeForTest(ProjectType::JKDF2);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setCurrentPathForTitleForTest(root / "presentation.sed");
    window.setResourcePathsForTest({root}, {}, root / "presentation.sed");
}

QWidget* createMainWindowResourcePage(
    MainWindow& window,
    const QString& actionName,
    const QString& dockName,
    const QString& tabsName)
{
    QAction* const create = window.findChild<QAction*>(actionName);
    if ( create )
    {
        create->trigger();
    }
    QDockWidget* const dock = window.findChild<QDockWidget*>(dockName);
    QTabWidget* const tabs = window.findChild<QTabWidget*>(tabsName);
    QToolButton* const newButton = dock
        ? dock->findChild<QToolButton*>(QStringLiteral("resource_authoring.new")) : nullptr;
    if ( tabs && tabs->count() == 0 && newButton )
    {
        newButton->click();
    }
    return tabs ? tabs->currentWidget() : nullptr;
}

QCborMap nativeEditSectionRoundTrip(const QCborMap& state)
{
    Level level{};
    level.kind = ProjectType::JKDF2;
    level.header.version = 1;
    level.layers = {"Base"};
    const QByteArray payload = QCborValue(state).toCbor();
    assets::SedOptionalSection section{};
    section.type = ui::project::SedEditorStateSection;
    section.payload.assign(
        reinterpret_cast<const std::byte*>(payload.constData()),
        reinterpret_cast<const std::byte*>(payload.constData()) + payload.size());
    const std::array<assets::SedOptionalSection, 1> sections{section};
    const assets::SedFile decoded = assets::loadSedFileBytes(assets::saveToSedBytes(level, sections));
    if ( decoded.optionalSections.size() != 1
        || decoded.optionalSections.front().type != ui::project::SedEditorStateSection )
    {
        return {};
    }
    const auto& bytes = decoded.optionalSections.front().payload;
    return QCborValue::fromCbor(QByteArray(
        reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size()))).toMap();
}

void testAggregateEditAndNativeSedTransport(SpriteGlViewEventBlocker& blocker)
{
    QTemporaryDir temporary{};
    check(temporary.isValid(), "resource presentation aggregate fixture has a private temporary project directory");
    if ( !temporary.isValid() )
    {
        return;
    }
    const std::filesystem::path root(temporary.path().toStdWString());
    MainWindow source{};
    initializeMainWindow(source, root);
    blocker.watch(&source);
    QWidget* const sprite = createMainWindowResourcePage(source,
        QStringLiteral("tools.resource_editors.sprite_spr.new"),
        QStringLiteral("resourceEditorsDock"), QStringLiteral("resourceEditorTabs"));
    QWidget* const sound = createMainWindowResourcePage(source,
        QStringLiteral("tools.resource_editors.sound_class_snd.new"),
        QStringLiteral("soundClassResourceEditorsDock"), QStringLiteral("soundClassResourceEditorTabs"));
    QWidget* const strings = createMainWindowResourcePage(source,
        QStringLiteral("tools.resource_editors.string_table_uni.new"),
        QStringLiteral("uniResourceEditorsDock"), QStringLiteral("uniResourceEditorTabs"));
    check(sprite && sound && strings
              && seedPane(sprite, ResourceAuthoringKind::sprite)
              && seedPane(sound, ResourceAuthoringKind::soundClass)
              && seedPane(strings, ResourceAuthoringKind::stringTable),
        "resource presentation aggregate fixture opens and seeds actual SPR, SND and UNI workspace pages");
    if ( !sprite || !sound || !strings )
    {
        return;
    }
    blocker.watch(sprite);
    blocker.watch(sound);
    blocker.watch(strings);

    QLineEdit* const material = spriteLineEdit(sprite, QStringLiteral("spr.material"));
    QLineEdit* const soundSearch = sound->findChild<QLineEdit*>(QStringLiteral("snd.search"));
    QLineEdit* const stringSearch = strings->findChild<QLineEdit*>(QStringLiteral("uni.search"));
    auto* const soundTree = sound->findChild<QTreeWidget*>(QStringLiteral("snd.entries"));
    auto* const stringTable = strings->findChild<QTableWidget*>(QStringLiteral("uni.entries"));
    check(material && soundSearch && stringSearch && soundTree && stringTable,
        "resource presentation aggregate fixture finds actual permanent controls in each resource page");
    if ( !material || !soundSearch || !stringSearch || !soundTree || !stringTable )
    {
        return;
    }
    source.markCurrentDocumentAsSavedForTest();
    check(source.recoveryWorkspaceHasSavedBaselineForTest()
              && !source.hasUnsavedChangesForTest()
              && !source.hasRecoveryOfferEligibleChangesForTest(),
        "resource presentation aggregate establishes a clean saved native baseline before presentation-only edits");
    setReverseSelection(material, 14, 5);
    soundSearch->setText(QStringLiteral("create"));
    soundTree->setCurrentItem(findSoundItem(soundTree,
        ui::resources::detail::SoundTreeItemKind::alternative, 83), 4);
    stringSearch->setText(QStringLiteral("FILTER_087"));
    stringTable->selectRow(87);
    stringTable->setCurrentCell(87, 1);
    check(!ui::resources::resourceAuthoringPaneIsModified(sprite)
              && !ui::resources::resourceAuthoringPaneIsModified(sound)
              && !ui::resources::resourceAuthoringPaneIsModified(strings)
              && !source.hasUnsavedChangesForTest()
              && !source.hasRecoveryOfferEligibleChangesForTest(),
        "resource presentation-only cursor, search, current-cell and selection changes preserve pane and saved recovery clean baselines");

    const QByteArray livePayload = source.captureRecoveryEditorStateForTest();
    QString warning{};
    check(!livePayload.isEmpty() && source.validateRecoveryEditorStateForTest(livePayload, &warning)
              && warning.isEmpty(),
        "resource presentation aggregate survives the production MainWindow EDIT capture and validator");
    const QCborMap capturedState = QCborValue::fromCbor(livePayload).toMap();
    const QCborMap roundTripState = nativeEditSectionRoundTrip(capturedState);
    check(!roundTripState.isEmpty(),
        "resource presentation aggregate traverses the real native SED optional EDIT section codec");
    const QByteArray restoredPayload = QCborValue(roundTripState).toCbor();

    MainWindow restored{};
    initializeMainWindow(restored, root);
    blocker.watch(&restored);
    check(restored.validateRecoveryEditorStateForTest(restoredPayload, &warning)
              && restored.restoreRecoveryEditorStateForTest(restoredPayload, &warning),
        "resource presentation aggregate restores through the production recovery EDIT application path");
    auto* const restoredSpriteTabs = restored.findChild<QTabWidget*>(QStringLiteral("resourceEditorTabs"));
    auto* const restoredSoundTabs = restored.findChild<QTabWidget*>(QStringLiteral("soundClassResourceEditorTabs"));
    auto* const restoredStringTabs = restored.findChild<QTabWidget*>(QStringLiteral("uniResourceEditorTabs"));
    QWidget* const restoredSprite = restoredSpriteTabs ? restoredSpriteTabs->currentWidget() : nullptr;
    QWidget* const restoredSound = restoredSoundTabs ? restoredSoundTabs->currentWidget() : nullptr;
    QWidget* const restoredStrings = restoredStringTabs ? restoredStringTabs->currentWidget() : nullptr;
    blocker.watch(restoredSprite);
    blocker.watch(restoredSound);
    blocker.watch(restoredStrings);
    QLineEdit* const restoredMaterial = restoredSprite
        ? spriteLineEdit(restoredSprite, QStringLiteral("spr.material")) : nullptr;
    QLineEdit* const restoredSoundSearch = restoredSound
        ? restoredSound->findChild<QLineEdit*>(QStringLiteral("snd.search")) : nullptr;
    QLineEdit* const restoredStringSearch = restoredStrings
        ? restoredStrings->findChild<QLineEdit*>(QStringLiteral("uni.search")) : nullptr;
    auto* const restoredSoundTree = restoredSound
        ? restoredSound->findChild<QTreeWidget*>(QStringLiteral("snd.entries")) : nullptr;
    auto* const restoredStringTable = restoredStrings
        ? restoredStrings->findChild<QTableWidget*>(QStringLiteral("uni.entries")) : nullptr;
    QTreeWidgetItem* const restoredSoundItem = findSoundItem(restoredSoundTree,
        ui::resources::detail::SoundTreeItemKind::alternative, 83);
    check(restoredMaterial && hasReverseSelection(restoredMaterial)
              && restoredSoundSearch && restoredSoundSearch->text() == QStringLiteral("create")
              && restoredSoundTree && restoredSoundItem
              && restoredSoundTree->currentItem() == restoredSoundItem
              && restoredStringSearch && restoredStringSearch->text() == QStringLiteral("FILTER_087")
              && restoredStringTable && restoredStringTable->currentRow() == 87
              && restoredStringTable->currentColumn() == 1
              && restoredStringTable->selectionModel()->hasSelection(),
        "resource presentation native EDIT round-trip restores real SPR, SND and UNI widgets from stable identities");
}

} // namespace

void runResourceAuthoringPresentationTests()
{
    SpriteGlViewEventBlocker blocker{};
    testSpritePermanentCaretAndMalformedState();
    testSpriteHistoryUsesOnlyCurrentCaretLength();
    testUnicodeLineEditCaretRoundTrip();
    testUniSelectionScrollFilterAndBadReference(blocker);
    testSndInterleavedIdentityAndManualWidths(blocker);
    testLiveUniDelegateCaptureAndDeferredCaret(blocker);
    testLiveSndDelegateCaptureAndDeferredCarets(blocker);
    testFailedLooseReloadKeepsEditorPresentation(blocker);
    testSuccessfulSndSaveInvalidatesDeferredCaret(blocker);
    testUndoRedoInvalidatesDeferredTableCaret(blocker);
    testSoundUndoRedoInvalidatesDeferredTableCaret(blocker);
    testAggregateEditAndNativeSedTransport(blocker);
}

void runResourceAuthoringHistoryCaretRegressionTests()
{
    testSpriteHistoryUsesOnlyCurrentCaretLength();
}

} // namespace sed::app_tests
