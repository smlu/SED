#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "app_settings.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/build.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

#include <QAction>
#include <QScopeGuard>
#include <QSettings>
#include <QToolButton>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>

#include <array>
#include <cstddef>
#include <utility>

namespace sed::app_tests
{
namespace
{

const QString kCleaveBoundedPreference = QStringLiteral("cleave/bounded");
const QString kCleaveBoundedButton = QStringLiteral("cleaveBoundedButton");

QCborMap rootFromEditorState(const QByteArray& editorState)
{
    const QCborValue value = QCborValue::fromCbor(editorState);
    return value.isMap() ? value.toMap() : QCborMap{};
}

QCborMap cleaveMapFromRoot(const QCborMap& root)
{
    const QCborArray viewport = root.value(QStringLiteral("viewport")).toArray();
    return viewport.size() > 38 && viewport.at(38).isMap()
        ? viewport.at(38).toMap()
        : QCborMap{};
}

QByteArray editorStateWithCleaveMap(QCborMap root, const QCborMap& cleave)
{
    QCborArray viewport = root.value(QStringLiteral("viewport")).toArray();
    if ( viewport.size() <= 38 )
    {
        return {};
    }
    viewport[38] = cleave;
    root.insert(QStringLiteral("viewport"), viewport);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

} // namespace

void runCleaveBoundedRecoveryTests()
{
    QSettings originalSettings = settingsStore();
    const bool hadBoundedPreference = originalSettings.contains(kCleaveBoundedPreference);
    const QVariant savedBoundedPreference = originalSettings.value(kCleaveBoundedPreference);
    originalSettings.remove(kCleaveBoundedPreference);
    originalSettings.sync();
    const auto restoreBoundedPreference = qScopeGuard(
        [hadBoundedPreference, savedBoundedPreference]()
        {
            QSettings settings = settingsStore();
            if ( hadBoundedPreference )
            {
                settings.setValue(kCleaveBoundedPreference, savedBoundedPreference);
            }
            else
            {
                settings.remove(kCleaveBoundedPreference);
            }
            settings.sync();
        });

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.activateSectorEditModeForTest();
    editor::DocumentState documentState{};
    documentState.level.kind = ProjectType::JKDF2;
    level::levelCreateCube(
        documentState.level,
        0.0,
        0.0,
        0.0,
        TVector::up,
        TVector::right);
    documentState.selection.sectors.insert(0);
    window.setDocumentStateForTest(std::move(documentState));
    // Re-entering the current mode refreshes tool availability without
    // replacing the selection installed with the test document.
    window.activateSectorEditModeForTest();
    window.pushUndoForTitleForTest();

    GlView* const view = window.glViewForTest();
    QToolButton* const boundedButton = window.findChild<QToolButton*>(kCleaveBoundedButton);
    check(view != nullptr && boundedButton != nullptr && boundedButton->isCheckable(),
        "bounded recovery: fixture exposes the viewport and checkable Bounded button");
    if ( !view || !boundedButton || !boundedButton->isCheckable() )
    {
        return;
    }

    check(boundedButton->isChecked() && view->cleaveBounded(),
        "bounded recovery: a fresh app preference defaults Bounded on");

    editor::Selection targets{};
    targets.sectors.insert(0);
    view->setSelectionState(0, -1, -1, -1, targets);
    view->setSectorCleaveToolEnabled(true);
    view->setCleaveStagedMode(true);
    view->setCleaveChainEnabled(true);
    window.updateEditActionsForTest();
    QAction* const cleaveAction = window.findChild<QAction*>(
        QStringLiteral("viewport.sector.cleave"));
    if (cleaveAction && cleaveAction->isEnabled() && !cleaveAction->isChecked())
    {
        cleaveAction->trigger();
    }
    view->captureStagedCleaveContextForTest(false);
    GlView::RecoveryState::CleaveDraftSnapshot coherentDraft =
        view->captureStagedCleaveDraftState();
    GlView::RecoveryState::CleaveLine coherentLine{};
    coherentLine.first = TVector{ 0.125, 0.25, 0.375 };
    coherentLine.second = TVector{ 1.125, 0.25, 0.375 };
    coherentLine.planeNormal = TVector{ 0.0, 1.0, 0.0 };
    coherentLine.constructionPlaneNormal = TVector{ 0.0, 0.0, 1.0 };
    coherentLine.angleReferenceAxis = TVector{ 1.0, 0.0, 0.0 };
    coherentDraft.lines = { coherentLine };
    coherentDraft.applyOrder = { 0 };
    coherentDraft.selectedCleaveLines = { 0 };
    coherentDraft.selectedCleaveLine = 0;
    GlView::RecoveryState::CleaveHistory coherentHistory{};
    coherentHistory.snapshots.push_back(coherentDraft);
    coherentHistory.cursor = 0;
    view->restoreStagedCleaveDraftState(coherentDraft, coherentHistory);
    check(cleaveAction && cleaveAction->isChecked()
            && view->sectorCleaveToolEnabled()
            && view->stagedCleaveLineCount() == 1
            && view->captureStagedCleaveHistory()
            && view->captureStagedCleaveHistory()->snapshots.size() == 1,
        "bounded recovery: restore installs the line and coherent history under the checked action");

    boundedButton->click();
    const QByteArray liveUiCapture = window.captureRecoveryEditorStateForTest();
    const QCborMap liveUiCleave = cleaveMapFromRoot(rootFromEditorState(liveUiCapture));
    check(!view->cleaveBounded()
            && !boundedButton->isChecked()
            && liveUiCleave.value(QStringLiteral("bounded")).isBool()
            && !liveUiCleave.value(QStringLiteral("bounded")).toBool(),
        "bounded recovery: EDIT capture reflects the live Bounded button choice");

    QSettings preferences = settingsStore();
    preferences.setValue(kCleaveBoundedPreference, true);
    preferences.sync();
    boundedButton->click();
    check(view->cleaveBounded()
            && boundedButton->isChecked()
            && preferences.value(kCleaveBoundedPreference).toBool(),
        "bounded recovery: an explicit user choice updates the live viewport and app preference");

    const QCborMap originalRoot = rootFromEditorState(liveUiCapture);
    QCborMap cleave = cleaveMapFromRoot(originalRoot);
    const QCborArray stagedLines = cleave.value(QStringLiteral("lines")).toArray();
    if (stagedLines.size() != 1)
    {
        const QString captureError = window.recoveryEditorStateCaptureErrorForTest();
        std::cerr << "[diagnostic] bounded EDIT capture bytes="
                  << liveUiCapture.size()
                  << " lines=" << stagedLines.size()
                  << " captureError=" << captureError.toStdString()
                  << " runtimeLines=" << view->stagedCleaveLineCount()
                  << " historySnapshots="
                  << view->captureStagedCleaveHistory().value_or(
                         GlView::RecoveryState::CleaveHistory{}).snapshots.size()
                  << '\n';
    }
    check(stagedLines.size() == 1,
        "bounded recovery: fixture captures one staged line before restoration");
    if ( stagedLines.size() != 1 )
    {
        return;
    }

    const QCborMap expectedGuides{
        { QStringLiteral("angles"), false },
        { QStringLiteral("lengths"), true },
        { QStringLiteral("numbers"), true },
    };
    const QCborArray expectedChainOrigin{ qint64(0), qint64(1) };
    cleave.insert(QStringLiteral("guides"), expectedGuides);
    cleave.insert(QStringLiteral("chainOrigin"), expectedChainOrigin);
    cleave.insert(QStringLiteral("bounded"), false);
    QCborMap encodedCoherentHistory = cleave.value(QStringLiteral("history")).toMap();
    QCborArray coherentSnapshots = encodedCoherentHistory.value(
        QStringLiteral("snapshots")).toArray();
    if (!coherentSnapshots.isEmpty())
    {
        QCborMap cursor = coherentSnapshots.at(
            encodedCoherentHistory.value(QStringLiteral("cursor")).toInteger()).toMap();
        cursor.insert(QStringLiteral("chainOrigin"), expectedChainOrigin);
        coherentSnapshots[static_cast<qsizetype>(
            encodedCoherentHistory.value(QStringLiteral("cursor")).toInteger())] = cursor;
        encodedCoherentHistory.insert(QStringLiteral("snapshots"), coherentSnapshots);
        cleave.insert(QStringLiteral("history"), encodedCoherentHistory);
    }
    const QByteArray explicitFalseState = editorStateWithCleaveMap(originalRoot, cleave);
    check(window.validateRecoveryEditorStateForTest(explicitFalseState),
        "bounded recovery: a strict Boolean value validates in the existing named Cleave map");

    const std::size_t undoDepthBeforeRestore = window.undoDepthForTest();
    const std::size_t redoDepthBeforeRestore = window.redoDepthForTest();
    const auto selectedSectorsBeforeRestore =
        window.documentStateForTest().selection.sectors;
    const auto sectorFlagsBeforeRestore =
        window.documentStateForTest().level.sectors[0].flags;
    const bool explicitFalseRestored = window.restoreRecoveryEditorStateForTest(explicitFalseState);
    const QByteArray explicitFalseCapture = window.captureRecoveryEditorStateForTest();
    const QCborMap explicitFalseCleave = cleaveMapFromRoot(
        rootFromEditorState(explicitFalseCapture));
    preferences.sync();
    check(explicitFalseRestored
            && !view->cleaveBounded()
            && !boundedButton->isChecked()
            && preferences.value(kCleaveBoundedPreference).toBool(),
        "bounded recovery: present false is applied without changing the app preference");
    check(explicitFalseCleave.value(QStringLiteral("lines")).toArray() == stagedLines
            && explicitFalseCleave.value(QStringLiteral("guides")).toMap() == expectedGuides
            && explicitFalseCleave.value(QStringLiteral("chainOrigin")).toArray()
                == expectedChainOrigin,
        "bounded recovery: changing the bounded option preserves staged geometry, guides, and chain origin");
    check(window.undoDepthForTest() == undoDepthBeforeRestore
            && window.redoDepthForTest() == redoDepthBeforeRestore
            && window.documentStateForTest().selection.sectors == selectedSectorsBeforeRestore
            && window.documentStateForTest().level.sectors[0].flags == sectorFlagsBeforeRestore,
        "bounded recovery: restoring viewport options leaves document selection and undo history unchanged");

    const QByteArray beforeMalformedRestore = window.captureRecoveryEditorStateForTest();
    const std::array<QCborValue, 5> malformedBoundedValues{
        QCborValue::Null,
        QCborValue(QStringLiteral("false")),
        QCborValue(qint64(0)),
        QCborValue(QCborArray{ false }),
        QCborValue(QCborMap{ { QStringLiteral("value"), true } }),
    };
    for ( const QCborValue& malformedValue : malformedBoundedValues )
    {
        QCborMap malformedCleave = cleave;
        malformedCleave.insert(QStringLiteral("bounded"), malformedValue);
        const QByteArray malformedState = editorStateWithCleaveMap(originalRoot, malformedCleave);
        const bool malformedValidationRejected =
            !window.validateRecoveryEditorStateForTest(malformedState);
        const bool malformedRestoreRejected =
            !window.restoreRecoveryEditorStateForTest(malformedState);
        preferences.sync();
        check(malformedValidationRejected
                && malformedRestoreRejected
                && window.captureRecoveryEditorStateForTest() == beforeMalformedRestore
                && preferences.value(kCleaveBoundedPreference).toBool(),
            "bounded recovery: malformed known values reject before changing editor or preference state");
    }

    QCborMap futureCleave = cleave;
    futureCleave.insert(QStringLiteral("bounded"), true);
    futureCleave.insert(QStringLiteral("futureOption"), QStringLiteral("ignored"));
    const QByteArray futureState = editorStateWithCleaveMap(originalRoot, futureCleave);
    const bool futureStateRestored = window.restoreRecoveryEditorStateForTest(futureState);
    const QCborMap futureCaptureCleave = cleaveMapFromRoot(
        rootFromEditorState(window.captureRecoveryEditorStateForTest()));
    check(futureStateRestored
            && view->cleaveBounded()
            && boundedButton->isChecked()
            && futureCaptureCleave.value(QStringLiteral("bounded")).isBool()
            && futureCaptureCleave.value(QStringLiteral("bounded")).toBool()
            && !futureCaptureCleave.contains(QStringLiteral("futureOption")),
        "bounded recovery: present true is honored while unknown Cleave keys are ignored");

    QCborMap legacyPendingCleave = cleave;
    legacyPendingCleave.remove(QStringLiteral("bounded"));
    // Legacy EDIT V1 has neither the additive history branch nor the named
    // chain origin. This is a legacy fixture, not a new payload with an
    // intentionally divergent current/cursor association.
    legacyPendingCleave.remove(QStringLiteral("history"));
    const QByteArray legacyPendingState = editorStateWithCleaveMap(
        originalRoot, legacyPendingCleave);
    const bool legacyPendingRestored = window.restoreRecoveryEditorStateForTest(legacyPendingState);
    preferences.sync();
    check(legacyPendingRestored
            && view->stagedCleaveLineCount() == 1
            && !view->cleaveBounded()
            && !boundedButton->isChecked()
            && preferences.value(kCleaveBoundedPreference).toBool(),
        "bounded recovery: legacy records with staged lines keep global-plane behavior");

    QCborMap emptyLegacyCleave = legacyPendingCleave;
    emptyLegacyCleave.insert(QStringLiteral("lines"), QCborArray{});
    emptyLegacyCleave.remove(QStringLiteral("chainOrigin"));
    emptyLegacyCleave.remove(QStringLiteral("applyOrder"));
    emptyLegacyCleave.remove(QStringLiteral("selection"));
    emptyLegacyCleave.remove(QStringLiteral("history"));
    QCborArray emptyOptions = emptyLegacyCleave.value(QStringLiteral("options")).toArray();
    emptyOptions[5] = qint64(-1);
    emptyLegacyCleave.insert(QStringLiteral("options"), emptyOptions);
    const QByteArray emptyLegacyState = editorStateWithCleaveMap(originalRoot, emptyLegacyCleave);
    preferences.setValue(kCleaveBoundedPreference, true);
    preferences.sync();
    const bool emptyLegacyRestored = window.restoreRecoveryEditorStateForTest(emptyLegacyState);
    check(emptyLegacyRestored
            && view->stagedCleaveLineCount() == 0
            && view->cleaveBounded()
            && boundedButton->isChecked()
            && preferences.value(kCleaveBoundedPreference).toBool(),
        "bounded recovery: legacy records without staged lines restore the app preference, not the preceding document value");

    preferences.setValue(kCleaveBoundedPreference, false);
    preferences.sync();
    const bool disabledPreferenceRestored = window.restoreRecoveryEditorStateForTest(emptyLegacyState);
    check(disabledPreferenceRestored
            && !view->cleaveBounded()
            && !boundedButton->isChecked()
            && !preferences.value(kCleaveBoundedPreference).toBool(),
        "bounded recovery: an absent value with no staged lines honors an explicitly disabled app preference");

    const bool explicitTrueWithDisabledPreference =
        window.restoreRecoveryEditorStateForTest(futureState);
    preferences.sync();
    check(explicitTrueWithDisabledPreference
            && view->cleaveBounded()
            && boundedButton->isChecked()
            && !preferences.value(kCleaveBoundedPreference).toBool(),
        "bounded recovery: restoring an explicit true value does not rewrite a disabled app preference");

    const bool emptyLegacyRestoredAgain = window.restoreRecoveryEditorStateForTest(emptyLegacyState);
    preferences.sync();
    check(emptyLegacyRestoredAgain
            && !view->cleaveBounded()
            && !boundedButton->isChecked()
            && !preferences.value(kCleaveBoundedPreference).toBool(),
        "bounded recovery: the app preference remains authoritative after an explicit document value");
}

} // namespace sed::app_tests
