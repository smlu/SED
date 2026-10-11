#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"
#include "sed/level/transform.hpp"
#include "ui/project/sed_editor_file_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

#include <QAction>
#include <QApplication>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QDateTime>
#include <QFile>
#include <QMenu>
#include <QSettings>
#include <QTemporaryDir>

#include <array>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace sed::app_tests
{

namespace
{

constexpr SurfaceRef kStrictScopeFaceA{ 0, 0 };
constexpr SurfaceRef kStrictScopeFaceB{ 0, 1 };

using SurfaceRefPairs = std::set<std::pair<std::int32_t, std::int32_t>>;

SurfaceRefPairs surfaceRefPairs(
    const std::unordered_set<SurfaceRef, editor::SurfaceRefHash,
        editor::SurfaceRefEq>& references)
{
    SurfaceRefPairs result{};
    for (const SurfaceRef reference : references)
    {
        result.emplace(reference.sector, reference.surface);
    }
    return result;
}

assets::SedOptionalSection optionalSection(
    const std::array<char, 4> type,
    const QByteArray& payload)
{
    assets::SedOptionalSection section{};
    section.type = type;
    if (!payload.isEmpty())
    {
        const auto* const first = reinterpret_cast<const std::byte*>(
            payload.constData());
        section.payload.assign(first, first + payload.size());
    }
    return section;
}

bool writeNativeHistoryFixture(
    const std::filesystem::path& path,
    const editor::Document& document,
    const QByteArray& editorPayload)
{
    const QByteArray historyPayload =
        ui::recovery::serializeEditorDocumentHistoryForSave(document);
    if (editorPayload.isEmpty() || historyPayload.isEmpty())
    {
        return false;
    }

    const std::array<assets::SedOptionalSection, 2> optionalSections{
        optionalSection({ 'e', 'd', 'i', 't' }, editorPayload),
        optionalSection({ 'h', 'i', 's', 't' }, historyPayload),
    };
    const std::vector<std::byte> bytes = assets::saveToSedBytes(
        document.current().level, optionalSections);
    if (bytes.empty())
    {
        return false;
    }

    QFile file(QString::fromStdWString(path.wstring()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    const QByteArray nativeBytes(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()));
    return file.write(nativeBytes) == nativeBytes.size();
}

editor::DocumentState strictScopeLevelState()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.layers.emplace_back("Layer0");
    Sector& sector = level::levelCreateCube(
        state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    sector.layer = 0;
    return state;
}

editor::DocumentState strictScopeThreeSectorLevelState()
{
    editor::DocumentState state = strictScopeLevelState();
    for (const double x : { 8.0, 16.0 })
    {
        Sector& sector = level::levelCreateCube(
            state.level, x, 0.0, 0.0, TVector::up, TVector::right);
        sector.layer = 0;
    }
    return state;
}

QCborArray surfaceReferenceValue(const SurfaceRef reference)
{
    return QCborArray{ reference.sector, reference.surface };
}

QCborArray surfaceReferenceArray(const std::vector<SurfaceRef>& references)
{
    QCborArray result{};
    for (const SurfaceRef reference : references)
    {
        result.append(surfaceReferenceValue(reference));
    }
    return result;
}

QByteArray withoutSurfaceSelectionContext(const QByteArray& payload)
{
    QCborParserError parseError{};
    QCborMap root = QCborValue::fromCbor(payload, &parseError).toMap();
    if (parseError.error != QCborError::NoError)
    {
        return {};
    }
    root.remove(QStringLiteral("surfaceSelectionContext"));
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

struct SurfaceScopeCapture
{
    QByteArray payload{};
    editor::DocumentState document{};
    SurfaceRefPairs liveStrictScope{};
    bool liveStrictEnabled{};
};

struct SurfaceScopeRestore
{
    bool accepted{};
    bool strictEnabled{};
    int surfaceMode{};
    SurfaceRefPairs strictScope{};
    SurfaceRefPairs selectedFaces{};
};

SurfaceScopeCapture captureStrictScopeState(
    const bool vertexMode,
    const bool seedFrozenFaces = true,
    const bool retainCurrentOwner = true)
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(strictScopeLevelState());
    window.activateSurfaceEditModeForTest();

    if (seedFrozenFaces)
    {
        auto& selection = window.mutableDocumentStateForTest().selection;
        selection.surfaces = { kStrictScopeFaceA, kStrictScopeFaceB };
    }
    window.activateSurfaceFaceSubModeForTest();
    if (vertexMode)
    {
        window.activateSurfaceVertexSubModeForTest();
    }
    else
    {
        window.activateSurfaceEdgeSubModeForTest();
    }

    auto& currentSelection = window.mutableDocumentStateForTest().selection;
    currentSelection.surfaces.clear();
    if (retainCurrentOwner)
    {
        if (vertexMode)
        {
            currentSelection.surfaceVertices.insert(
                editor::SurfaceVertexRef{ kStrictScopeFaceA, 0 });
        }
        else
        {
            currentSelection.surfaces.insert(kStrictScopeFaceA);
        }
    }

    GlView* const view = window.glViewForTest();
    return SurfaceScopeCapture{
        window.captureRecoveryEditorStateForTest(),
        window.documentStateForTest(),
        view ? surfaceRefPairs(view->strictSurfaceSelectionScope())
             : SurfaceRefPairs{},
        view && view->strictSurfaceSelectionEnabled() };
}

SurfaceScopeRestore restoreStrictScopeState(
    const QByteArray& payload,
    const Level& level,
    const std::filesystem::path& recoveryDirectory,
    const bool returnToFaceMode = false)
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setRecoveryDirectoryForTest(recoveryDirectory);
    editor::DocumentState state{};
    state.level = level;
    window.setDocumentStateForTest(std::move(state));
    QString warning{};
    const bool accepted = window.restoreRecoveryEditorStateForTest(payload, &warning);
    GlView* const view = window.glViewForTest();
    SurfaceScopeRestore result{
        accepted,
        view && view->strictSurfaceSelectionEnabled(),
        window.surfaceSelectionModeForTest(),
        view ? surfaceRefPairs(view->strictSurfaceSelectionScope())
             : SurfaceRefPairs{},
        surfaceRefPairs(window.documentStateForTest().selection.surfaces) };
    if (accepted && returnToFaceMode)
    {
        window.activateSurfaceFaceSubModeForTest();
        result.selectedFaces = surfaceRefPairs(
            window.documentStateForTest().selection.surfaces);
    }
    return result;
}

} // namespace

void runStrictSurfaceSelectionRecoveryTests()
{
    constexpr auto strictSelectionKey = "viewport/surface_strict_selection";
    QSettings settings = settingsStore();
    const QString strictKey = QString::fromLatin1(strictSelectionKey);
    const bool hadStrictPreference = settings.contains(strictKey);
    const QVariant priorStrictPreference = settings.value(strictKey);
    settings.setValue(strictKey, true);
    settings.sync();
    struct StrictSettingRestore final
    {
        QSettings& settings;
        QString key;
        bool wasPresent{};
        QVariant value{};

        ~StrictSettingRestore()
        {
            if (wasPresent)
            {
                settings.setValue(key, value);
            }
            else
            {
                settings.remove(key);
            }
            settings.sync();
        }
    } restoreSetting{
        settings, strictKey, hadStrictPreference, priorStrictPreference };

    QTemporaryDir testDirectory{};
    check(testDirectory.isValid(),
        "strict Surface selection recovery fixture has an isolated temporary directory");
    if (!testDirectory.isValid())
    {
        return;
    }
    const std::filesystem::path root(
        testDirectory.path().toStdWString());

    const SurfaceRefPairs expectedFrozenScope{
        { kStrictScopeFaceA.sector, kStrictScopeFaceA.surface },
        { kStrictScopeFaceB.sector, kStrictScopeFaceB.surface } };
    const SurfaceScopeCapture edge = captureStrictScopeState(false);
    const SurfaceScopeCapture vertex = captureStrictScopeState(true);
    check(edge.liveStrictEnabled && edge.liveStrictScope == expectedFrozenScope
            && vertex.liveStrictEnabled
            && vertex.liveStrictScope == expectedFrozenScope,
        "entering Edge or Vertex freezes the selected A/B faces while current component ownership retains only A");

    auto inspectSidecar = [](const SurfaceScopeCapture& capture,
                              const bool expectVertex)
    {
        QCborParserError parseError{};
        const QCborValue decoded = QCborValue::fromCbor(
            capture.payload, &parseError);
        const QCborMap rootMap = decoded.toMap();
        const QCborMap context = rootMap.value(
            QStringLiteral("surfaceSelectionContext")).toMap();
        const QCborValue strictScope = context.value(
            QStringLiteral("strictScope"));
        const QCborValue rememberedFaces = context.value(
            QStringLiteral("rememberedFaceSelection"));
        const QCborArray edit = rootMap.value(QStringLiteral("edit")).toArray();
        const QCborArray expectedReferences{
            surfaceReferenceValue(kStrictScopeFaceA),
            surfaceReferenceValue(kStrictScopeFaceB) };
        return parseError.error == QCborError::NoError
            && decoded.isMap()
            && context.value(QStringLiteral("version")).toInteger() == 1
            && strictScope.isArray()
            && strictScope.toArray() == expectedReferences
            && rememberedFaces.isArray()
            && rememberedFaces.toArray() == expectedReferences
            && edit.size() == 3
            && edit.at(1).toInteger() == (expectVertex ? 2 : 1);
    };
    check(inspectSidecar(edge, false) && inspectSidecar(vertex, true),
        "captured EDIT records checked frozen-scope and remembered-face context apart from current selection");

    QCborParserError extensionParseError{};
    QCborMap extensionRoot = QCborValue::fromCbor(
        edge.payload, &extensionParseError).toMap();
    QCborMap extensionContext = extensionRoot.value(
        QStringLiteral("surfaceSelectionContext")).toMap();
    extensionContext.insert(QStringLiteral("futureOptionalHint"), true);
    extensionRoot.insert(QStringLiteral("surfaceSelectionContext"),
        extensionContext);
    const QByteArray futureContextPayload =
        ui::recovery::serializeCanonicalCbor(QCborValue(extensionRoot));
    MainWindow futureContextWindow{};
    futureContextWindow.setAttribute(Qt::WA_DontShowOnScreen);
    futureContextWindow.setRecoveryDirectoryForTest(root / "future-context");
    editor::DocumentState futureContextState{};
    futureContextState.level = edge.document.level;
    futureContextWindow.setDocumentStateForTest(std::move(futureContextState));
    const bool futureContextAccepted =
        extensionParseError.error == QCborError::NoError
        && futureContextWindow.validateRecoveryEditorStateForTest(
            futureContextPayload)
        && futureContextWindow.restoreRecoveryEditorStateForTest(
            futureContextPayload);
    const QCborMap rewrittenContext = QCborValue::fromCbor(
        futureContextWindow.captureRecoveryEditorStateForTest()).toMap()
            .value(QStringLiteral("surfaceSelectionContext")).toMap();
    const GlView* const futureContextView =
        futureContextWindow.glViewForTest();
    check(futureContextAccepted && futureContextView
            && surfaceRefPairs(
                futureContextView->strictSurfaceSelectionScope())
                == expectedFrozenScope
            && !rewrittenContext.contains(QStringLiteral("futureOptionalHint")),
        "bounded unknown Surface context extensions are ignored while known references restore and are dropped by this writer");

    const auto edgeManual = restoreStrictScopeState(
        edge.payload, edge.document.level, root / "manual-edge");
    const auto vertexManual = restoreStrictScopeState(
        vertex.payload, vertex.document.level, root / "manual-vertex", true);
    check(edgeManual.accepted && edgeManual.strictEnabled
            && edgeManual.surfaceMode == 1
            && edgeManual.strictScope == expectedFrozenScope,
        "manual EDIT restore keeps exact A/B Strict scope in Edge mode");
    check(vertexManual.accepted && vertexManual.strictEnabled
            && vertexManual.surfaceMode == 2
            && vertexManual.strictScope == expectedFrozenScope
            && vertexManual.selectedFaces == expectedFrozenScope,
        "manual EDIT restore keeps exact A/B Strict scope in Vertex mode and restores remembered faces when returning to Face");

    const std::array nativeEditSection{
        ui::project::makeSedOptionalSection(
            ui::project::SedEditorStateSection, edge.payload) };
    const std::vector<std::byte> nativeBytes = assets::saveToSedBytes(
        edge.document.level, nativeEditSection);
    const assets::SedFile nativeFile = assets::loadSedFileBytes(nativeBytes);
    const QByteArray nativeEditorState = ui::project::findSedOptionalSection(
        nativeFile.optionalSections, ui::project::SedEditorStateSection);
    const auto nativeRestore = restoreStrictScopeState(
        nativeEditorState, nativeFile.level, root / "native-open");
    check(nativeRestore.accepted && nativeRestore.strictEnabled
            && nativeRestore.surfaceMode == 1
            && nativeRestore.strictScope == expectedFrozenScope,
        "native SED EDIT section round trip restores exact Edge Strict scope");

    const std::filesystem::path liveRecoveryPath =
        root / "strict-scope.sedrecovery";
    const QString sessionId =
        QStringLiteral("strict-surface-scope-test-session");
    auto publicationGate = std::make_shared<ui::recovery::PublicationGate>();
    publicationGate->generation.store(1, std::memory_order_release);
    auto makeRecoveryRequest = [&]()
    {
        ui::recovery::WriteRequest request{};
        request.recoveryPath = liveRecoveryPath;
        request.publicationGate = publicationGate;
        request.generation = publicationGate->generation.load(
            std::memory_order_acquire);
        request.metadata.identity = QStringLiteral("untitled:") + sessionId;
        request.metadata.sessionId = sessionId;
        request.metadata.displayName = QStringLiteral("Strict Surface Scope");
        request.metadata.timestamp = QDateTime::currentMSecsSinceEpoch();
        request.metadata.sourceModifiedAtMs = -1;
        request.metadata.requiresUserRecovery = true;
        request.metadata.projectType = edge.document.level.kind;
        request.metadata.projectTypeTrust =
            ui::recovery::ProjectTypeTrust::metadata;
        request.editorStatePayload = edge.payload;
        request.document.clear(edge.document);
        return request;
    };
    ui::recovery::WriteRequest liveRequest = makeRecoveryRequest();
    const ui::recovery::WriteResult liveWrite =
        ui::recovery::writeRecoverySnapshot(std::move(liveRequest));
    QString recoveryError{};
    const auto liveRecord = liveWrite.status == ui::recovery::WriteStatus::published
        ? ui::recovery::readRecoveryRecord(liveRecoveryPath, &recoveryError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    const auto recoveredDocument = liveRecord
        ? ui::recovery::loadRecoveredDocument(*liveRecord, &recoveryError)
        : std::optional<ui::recovery::RecoveredDocument>{};
    const auto recoveryRestore = recoveredDocument
        ? restoreStrictScopeState(
            recoveredDocument->editorStatePayload,
            recoveredDocument->current.level,
            root / "recovery-open")
        : SurfaceScopeRestore{};
    check(liveWrite.status == ui::recovery::WriteStatus::published
            && liveRecord && recoveredDocument
            && recoveredDocument->editorStatePayload == edge.payload
            && recoveryRestore.accepted && recoveryRestore.strictEnabled
            && recoveryRestore.surfaceMode == 1
            && recoveryRestore.strictScope == expectedFrozenScope,
        "live recovery publication and recovery restore preserve exact EDIT Strict scope");

    publicationGate->generation.store(2, std::memory_order_release);
    ui::recovery::WriteRequest periodicRequest = makeRecoveryRequest();
    periodicRequest.archivePreviousSnapshot = true;
    periodicRequest.archiveSourcePath = liveRecoveryPath;
    periodicRequest.maximumHistoricalSnapshots = 1;
    const ui::recovery::WriteResult periodicWrite =
        ui::recovery::writeRecoverySnapshot(std::move(periodicRequest));
    const auto history = ui::recovery::scanAutosaveHistory(root);
    const auto archivedRecord = !history.empty()
        ? ui::recovery::readRecoveryRecord(history.front().recoveryPath, &recoveryError)
        : std::optional<ui::recovery::RecoveryRecord>{};
    const auto autosaveRestore = archivedRecord
        ? restoreStrictScopeState(
            archivedRecord->editorStatePayload,
            edge.document.level,
            root / "autosave-open")
        : SurfaceScopeRestore{};
    check(periodicWrite.status == ui::recovery::WriteStatus::published
            && history.size() == 1 && archivedRecord
            && archivedRecord->editorStatePayload == edge.payload
            && autosaveRestore.accepted && autosaveRestore.strictEnabled
            && autosaveRestore.strictScope == expectedFrozenScope,
        "periodic autosave history preserves and restores the same exact EDIT Strict scope");

    const auto absentSidecarPayload = withoutSurfaceSelectionContext(edge.payload);
    const auto legacyOwnerFallback = restoreStrictScopeState(
        absentSidecarPayload, edge.document.level, root / "legacy-owner");
    check(legacyOwnerFallback.accepted
            && legacyOwnerFallback.strictEnabled
            && legacyOwnerFallback.strictScope == SurfaceRefPairs{
                { kStrictScopeFaceA.sector, kStrictScopeFaceA.surface } },
        "legacy EDIT without the optional context infers only its valid current component owner");

    const SurfaceScopeCapture emptyFrozen = captureStrictScopeState(
        true, false, true);
    const auto emptySidecarRestore = restoreStrictScopeState(
        emptyFrozen.payload, emptyFrozen.document.level,
        root / "explicit-empty");
    const auto emptyLegacyRestore = restoreStrictScopeState(
        withoutSurfaceSelectionContext(emptyFrozen.payload),
        emptyFrozen.document.level, root / "legacy-empty");
    check(emptyFrozen.liveStrictEnabled && emptyFrozen.liveStrictScope.empty()
            && emptySidecarRestore.accepted
            && emptySidecarRestore.strictEnabled
            && emptySidecarRestore.strictScope.empty(),
        "an explicitly empty Strict scope remains empty when the optional context is present");
    check(emptyLegacyRestore.accepted && emptyLegacyRestore.strictEnabled
            && emptyLegacyRestore.strictScope == SurfaceRefPairs{
                { kStrictScopeFaceA.sector, kStrictScopeFaceA.surface } },
        "legacy absent-context fallback uses a valid current Vertex owner when available");

    MainWindow atomicWindow{};
    atomicWindow.setAttribute(Qt::WA_DontShowOnScreen);
    atomicWindow.setRecoveryDirectoryForTest(root / "malformed-target");
    atomicWindow.setDocumentStateForTest(strictScopeLevelState());
    const QByteArray beforeMalformed =
        atomicWindow.captureRecoveryEditorStateForTest();
    QCborParserError edgeParseError{};
    QCborMap edgeRoot = QCborValue::fromCbor(
        edge.payload, &edgeParseError).toMap();
    QCborMap context = edgeRoot.value(
        QStringLiteral("surfaceSelectionContext")).toMap();
    const auto rejectsAtomically = [&atomicWindow, &beforeMalformed](
        const QCborMap& malformedRoot)
    {
        const QByteArray malformed = ui::recovery::serializeCanonicalCbor(
            QCborValue(malformedRoot));
        QString warning{};
        return !atomicWindow.validateRecoveryEditorStateForTest(malformed, &warning)
            && !atomicWindow.restoreRecoveryEditorStateForTest(malformed, &warning)
            && atomicWindow.captureRecoveryEditorStateForTest() == beforeMalformed;
    };
    QCborMap malformedType = edgeRoot;
    QCborMap malformedContext = context;
    malformedContext.insert(QStringLiteral("strictScope"),
        QStringLiteral("not-an-array"));
    malformedType.insert(QStringLiteral("surfaceSelectionContext"),
        malformedContext);
    QCborMap duplicateReference = edgeRoot;
    QCborMap duplicateContext = context;
    duplicateContext.insert(QStringLiteral("strictScope"),
        surfaceReferenceArray({ kStrictScopeFaceA, kStrictScopeFaceA }));
    duplicateReference.insert(QStringLiteral("surfaceSelectionContext"),
        duplicateContext);
    QCborMap outOfLevelReference = edgeRoot;
    QCborMap outOfLevelContext = context;
    outOfLevelContext.insert(QStringLiteral("strictScope"),
        surfaceReferenceArray({ SurfaceRef{ 0, 99 } }));
    outOfLevelReference.insert(QStringLiteral("surfaceSelectionContext"),
        outOfLevelContext);
    check(edgeParseError.error == QCborError::NoError
            && rejectsAtomically(malformedType)
            && rejectsAtomically(duplicateReference)
            && rejectsAtomically(outOfLevelReference),
        "malformed, duplicate, and out-of-Level optional context references reject EDIT atomically");
}

void runStrictSurfaceContextTopologyTests()
{
    constexpr auto strictSelectionKey = "viewport/surface_strict_selection";
    QSettings settings = settingsStore();
    const QString strictKey = QString::fromLatin1(strictSelectionKey);
    const bool hadStrictPreference = settings.contains(strictKey);
    const QVariant priorStrictPreference = settings.value(strictKey);
    settings.setValue(strictKey, true);
    settings.sync();
    struct StrictSettingRestore final
    {
        QSettings& settings;
        QString key;
        bool wasPresent{};
        QVariant value{};

        ~StrictSettingRestore()
        {
            if (wasPresent)
            {
                settings.setValue(key, value);
            }
            else
            {
                settings.remove(key);
            }
            settings.sync();
        }
    } restoreSetting{
        settings, strictKey, hadStrictPreference, priorStrictPreference };

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setDocumentStateForTest(strictScopeThreeSectorLevelState());
    window.activateSurfaceEditModeForTest();
    window.activateSurfaceFaceSubModeForTest();
    window.mutableDocumentStateForTest().selection.surfaces = {
        SurfaceRef{ 1, 0 } };
    window.activateSurfaceEdgeSubModeForTest();

    GlView* const view = window.glViewForTest();
    const SurfaceRefPairs originalFrozenFace{ { 1, 0 } };
    check(view && view->strictSurfaceSelectionEnabled()
            && surfaceRefPairs(view->strictSurfaceSelectionScope())
                == originalFrozenFace,
        "Strict Edge starts with the selected face on sector 1 frozen");

    const std::vector<std::int32_t> transformedSector{ 1 };
    window.pushUndoForTitleForTest(editor::DocumentDirty::LevelGeometry);
    const bool translated = level::levelTranslateSectorsChecked(
        window.mutableDocumentStateForTest().level,
        transformedSector, 0.5, 0.0, 0.0);
    window.pushUndoForTitleForTest(editor::DocumentDirty::LevelGeometry);
    const bool rotated = level::levelRotateSectorsChecked(
        window.mutableDocumentStateForTest().level,
        transformedSector, TVector::up, 8.5, 0.0, 0.0, 15.0);
    check(translated && rotated && view
            && view->strictSurfaceSelectionEnabled()
            && surfaceRefPairs(view->strictSurfaceSelectionScope())
                == originalFrozenFace
            && window.surfaceSelectionModeForTest()
                != window.surfaceFaceModeValueForTest(),
        "geometry-only sector move and rotation preserve the valid frozen Strict Edge scope");

    window.activateSurfaceFaceSubModeForTest();
    check(surfaceRefPairs(window.documentStateForTest().selection.surfaces)
            == originalFrozenFace,
        "geometry-only sector move and rotation preserve the remembered face selection");
    window.activateSurfaceEdgeSubModeForTest();

    window.activateSectorEditModeForTest();
    auto& selection = window.mutableDocumentStateForTest().selection;
    selection.clear();
    selection.sectors.insert(0);
    window.updateEditActionsForTest();
    QAction* const deleteAction = window.findChild<QAction*>(
        QStringLiteral("edit.delete"));
    check(deleteAction && deleteAction->isEnabled(),
        "Delete is enabled for sector 0 in Sector mode");
    if (deleteAction && deleteAction->isEnabled())
    {
        deleteAction->trigger();
    }
    check(window.documentStateForTest().level.sectors.size() == 2,
        "the real Delete action removes sector 0 and shifts the remaining sector indices");

    const SurfaceRefPairs shiftedOriginalFace{ { 0, 0 } };
    const SurfaceRefPairs strictScopeWhileSectorMode = view
        ? surfaceRefPairs(view->strictSurfaceSelectionScope())
        : SurfaceRefPairs{};
    check(view && view->strictSurfaceSelectionEnabled()
            && (strictScopeWhileSectorMode == shiftedOriginalFace
                || (strictScopeWhileSectorMode.empty()
                    && window.surfaceSelectionModeForTest()
                        == window.surfaceFaceModeValueForTest())),
        "topology invalidation while Sector mode is active either remaps the exact face or switches the Surface submode to usable Face");

    window.activateSurfaceEditModeForTest();
    const auto contextIsSafeFor = [&window, view](
                                      const SurfaceRefPairs& expectedFace)
    {
        if (!view || !view->strictSurfaceSelectionEnabled())
        {
            return false;
        }
        const SurfaceRefPairs currentScope = surfaceRefPairs(
            view->strictSurfaceSelectionScope());
        return currentScope == expectedFace
            || (currentScope.empty()
                && window.surfaceSelectionModeForTest()
                    == window.surfaceFaceModeValueForTest());
    };
    const auto rememberedFaceIsSafeFor = [&window](
                                               const SurfaceRefPairs& expectedFace)
    {
        window.activateSurfaceFaceSubModeForTest();
        const SurfaceRefPairs restoredFaces = surfaceRefPairs(
            window.documentStateForTest().selection.surfaces);
        return restoredFaces.empty() || restoredFaces == expectedFace;
    };

    check(contextIsSafeFor(shiftedOriginalFace),
        "after deleting an earlier sector, Strict scope is invalidated with a usable Face submode or follows the original face instead of retargeting another room");
    check(rememberedFaceIsSafeFor(shiftedOriginalFace),
        "returning to Surface after Delete does not restore a stale last-mode face ordinal or retarget another sector");

    QAction* const undoAction = window.findChild<QAction*>(
        QStringLiteral("edit.undo"));
    window.updateEditActionsForTest();
    const bool undoReady = undoAction && undoAction->isEnabled();
    if (undoReady)
    {
        undoAction->trigger();
    }
    check(undoReady && window.documentStateForTest().level.sectors.size() == 3,
        "Undo restores the deleted first sector through the real history action");
    check(contextIsSafeFor(originalFrozenFace),
        "Undo keeps Strict context cleared with a usable Face submode or remaps it to the restored original face");
    check(rememberedFaceIsSafeFor(originalFrozenFace),
        "returning to Face after Undo does not retarget a remembered face");

    QAction* const redoAction = window.findChild<QAction*>(
        QStringLiteral("edit.redo"));
    window.updateEditActionsForTest();
    const bool redoReady = redoAction && redoAction->isEnabled();
    if (redoReady)
    {
        redoAction->trigger();
    }
    check(redoReady && window.documentStateForTest().level.sectors.size() == 2,
        "Redo reapplies the first-sector deletion through the real history action");
    check(contextIsSafeFor(shiftedOriginalFace),
        "Redo keeps Strict context cleared with a usable Face submode or remaps it to the shifted original face");
    check(rememberedFaceIsSafeFor(shiftedOriginalFace),
        "returning to Face after Redo does not retarget a remembered face");

    QTemporaryDir importedHistoryDirectory{};
    check(importedHistoryDirectory.isValid(),
        "Strict Surface native HIST fixtures have an isolated temporary directory");
    if (importedHistoryDirectory.isValid())
    {
        const std::filesystem::path importedHistoryRoot(
            importedHistoryDirectory.path().toStdWString());
        const SurfaceRef frozenHistoryFace{ 1, 0 };
        const SurfaceRefPairs frozenHistoryScope{ { 1, 0 } };
        const editor::SurfaceVertexRef frozenHistoryVertex{
            frozenHistoryFace, 0 };

        const auto fixtureState = [](const bool includeThing)
        {
            editor::DocumentState state = strictScopeThreeSectorLevelState();
            if (includeThing)
            {
                Thing thing{};
                thing.name = "+strict_history_thing";
                thing.templateName = thing.name;
                thing.layer = 0;
                state.level.things.push_back(std::move(thing));
            }
            return state;
        };
        const auto configureStrictVertexContext = [&fixtureState,
                                                       frozenHistoryFace,
                                                       frozenHistoryVertex](
                                                       MainWindow& target,
                                                       const bool includeThing)
        {
            target.setAttribute(Qt::WA_DontShowOnScreen);
            target.setDocumentStateForTest(fixtureState(includeThing));
            target.activateSurfaceEditModeForTest();
            target.mutableDocumentStateForTest().selection.surfaces = {
                frozenHistoryFace };
            target.activateSurfaceFaceSubModeForTest();
            target.activateSurfaceVertexSubModeForTest();
            editor::Selection& selection =
                target.mutableDocumentStateForTest().selection;
            selection.surfaces.clear();
            selection.surfaceVertices = { frozenHistoryVertex };
        };
        const auto translateThirdSector = [](editor::DocumentState& state)
        {
            const std::vector<std::int32_t> sectors{ 2 };
            return level::levelTranslateSectorsChecked(
                state.level, sectors, 0.5, 0.0, 0.0);
        };
        const auto deleteFirstSector = [](editor::DocumentState& state)
        {
            if (state.level.sectors.size() != 3u)
            {
                return false;
            }
            state.level.sectors.erase(state.level.sectors.begin());
            return true;
        };
        const auto moveOnlyThing = [](editor::DocumentState& state)
        {
            if (state.level.things.empty())
            {
                return false;
            }
            state.level.things.front().x += 1.0;
            return true;
        };

        const auto hasImportedFrozenContext = [&frozenHistoryScope,
                                                   frozenHistoryVertex](
                                                   const MainWindow& target)
        {
            const GlView* const targetView = target.glViewForTest();
            const editor::Selection& selection =
                target.documentStateForTest().selection;
            return targetView && targetView->strictSurfaceSelectionEnabled()
                && surfaceRefPairs(
                       targetView->strictSurfaceSelectionScope())
                    == frozenHistoryScope
                && target.surfaceSelectionModeForTest()
                    != target.surfaceFaceModeValueForTest()
                && selection.surfaceVertices.size() == 1u
                && selection.surfaceVertices.contains(frozenHistoryVertex);
        };
        const auto hasInvalidatedTopologyContext = [](
                                                   const MainWindow& target)
        {
            const GlView* const targetView = target.glViewForTest();
            const editor::Selection& selection =
                target.documentStateForTest().selection;
            return targetView && targetView->strictSurfaceSelectionEnabled()
                && targetView->strictSurfaceSelectionScope().empty()
                && target.surfaceSelectionModeForTest()
                    == target.surfaceFaceModeValueForTest()
                && selection.surfaceVertices.empty()
                && selection.surfaces.empty();
        };
        const auto hasKnownTopologyInvalidatedContext = [&frozenHistoryScope](
                                                            const MainWindow& target)
        {
            const GlView* const targetView = target.glViewForTest();
            const editor::Selection& selection =
                target.documentStateForTest().selection;
            return targetView && targetView->strictSurfaceSelectionEnabled()
                && targetView->strictSurfaceSelectionScope().empty()
                && target.surfaceSelectionModeForTest()
                    == target.surfaceFaceModeValueForTest()
                && selection.surfaceVertices.empty()
                && surfaceRefPairs(selection.surfaces) == frozenHistoryScope;
        };

        const auto runImportedHistoryCase = [&](
                                                   const char* caseName,
                                                   const editor::DocumentDirty dirty,
                                                   const bool topologyChange,
                                                   const bool geometryPrelude,
                                                   const bool thingOnly,
                                                   const std::size_t expectedSectorCount,
                                                   const bool pairedTopologyPrelude)
        {
            const std::string name(caseName);
            MainWindow editSource{};
            configureStrictVertexContext(editSource, thingOnly);
            // MainWindow setup normalizes the persisted layer visibility string
            // when it initializes the layer view. Seed the independent HIST
            // timeline only after that stage so EDIT and HIST share the exact
            // same serialized Level before either history path is exercised.
            editor::Document timeline(editSource.documentStateForTest());

            if (geometryPrelude || pairedTopologyPrelude)
            {
                const bool historyMove = timeline.applyUndoable(
                    pairedTopologyPrelude
                        ? "Structure and geometry history prelude"
                        : "Geometry-only history prelude",
                    pairedTopologyPrelude
                        ? editor::DocumentDirty::Structure
                            | editor::DocumentDirty::LevelGeometry
                        : editor::DocumentDirty::LevelGeometry,
                    translateThirdSector);
                const bool editMove = translateThirdSector(
                    editSource.mutableDocumentStateForTest());
                check(historyMove && editMove,
                    pairedTopologyPrelude
                        ? "native topology HIST fixture records a valid Structure and LevelGeometry prelude"
                        : "native topology HIST fixture records a valid LevelGeometry-only prelude");
            }

            bool historyMutation = false;
            bool editMutation = false;
            const char* mutationLabel = "LevelGeometry-only history";
            if (topologyChange)
            {
                mutationLabel = "Full HIST sector deletion";
                historyMutation = timeline.applyUndoable(
                    mutationLabel, dirty, deleteFirstSector);
                editMutation = deleteFirstSector(
                    editSource.mutableDocumentStateForTest());
            }
            else if (thingOnly)
            {
                mutationLabel = "Thing-only Structure history";
                historyMutation = timeline.applyUndoable(
                    mutationLabel, dirty, moveOnlyThing);
                editMutation = moveOnlyThing(
                    editSource.mutableDocumentStateForTest());
            }
            else
            {
                historyMutation = timeline.applyUndoable(
                    mutationLabel, dirty, translateThirdSector);
                editMutation = translateThirdSector(
                    editSource.mutableDocumentStateForTest());
            }
            check(historyMutation && editMutation,
                "native Strict Surface HIST fixture records its accepted full Level transition");
            check(assets::sedLevelsEqual(
                    timeline.current().level,
                    editSource.documentStateForTest().level),
                "native Strict Surface HIST fixture pairs EDIT with the exact current Level");

            const std::filesystem::path nativePath =
                importedHistoryRoot / (name + ".sed");
            check(writeNativeHistoryFixture(
                    nativePath,
                    timeline,
                    editSource.captureRecoveryEditorStateForTest()),
                "native Strict Surface HIST fixture writes checked Level, EDIT, and HIST sections");

            MainWindow reopened{};
            reopened.setAttribute(Qt::WA_DontShowOnScreen);
            reopened.setRecoveryDirectoryForTest(
                importedHistoryRoot / (name + "-recovery"));
            reopened.openLevelFromFilenameForTest(
                QString::fromStdWString(nativePath.wstring()));
            const std::size_t expectedDepth =
                geometryPrelude || pairedTopologyPrelude ? 2u : 1u;
            check(reopened.undoDepthForTest() == expectedDepth
                    && reopened.documentStateForTest().level.sectors.size()
                        == expectedSectorCount
                    && hasImportedFrozenContext(reopened),
                "production native Open admits the HIST transition and restores its frozen Strict Vertex context");
            if (reopened.undoDepthForTest() != expectedDepth
                || !hasImportedFrozenContext(reopened))
            {
                return;
            }

            const auto triggerHistoryAction = [&reopened](
                                                   const QString& objectName)
            {
                reopened.updateEditActionsForTest();
                QAction* const action = reopened.findChild<QAction*>(objectName);
                if (!action || !action->isEnabled())
                {
                    return false;
                }
                action->trigger();
                return true;
            };
            const auto reseedStrictVertexContext = [&reopened,
                                                       frozenHistoryFace,
                                                       frozenHistoryVertex]()
            {
                reopened.activateSurfaceEditModeForTest();
                reopened.activateSurfaceFaceSubModeForTest();
                reopened.mutableDocumentStateForTest().selection.clear();
                reopened.mutableDocumentStateForTest().selection.surfaces = {
                    frozenHistoryFace };
                reopened.activateSurfaceVertexSubModeForTest();
                editor::Selection& selection =
                    reopened.mutableDocumentStateForTest().selection;
                selection.surfaces.clear();
                selection.surfaceVertices = { frozenHistoryVertex };
            };
            const auto reseedStrictFaceContext = [&]()
            {
                reseedStrictVertexContext();
                reopened.activateSurfaceFaceSubModeForTest();
                const GlView* const targetView = reopened.glViewForTest();
                const editor::Selection& selection =
                    reopened.documentStateForTest().selection;
                check(targetView
                        && targetView->strictSurfaceSelectionEnabled()
                        && reopened.surfaceSelectionModeForTest()
                            == reopened.surfaceFaceModeValueForTest()
                        && surfaceRefPairs(selection.surfaces)
                            == frozenHistoryScope,
                    "mixed-jump fixture restores a committed Strict Face selection before navigation");
            };
            const auto checkTopologyNavigation = [&]()
            {
                check(hasInvalidatedTopologyContext(reopened),
                    "native topology HIST navigation clears Strict scope and components and returns to Face");
                reopened.activateSectorEditModeForTest();
                reopened.activateSurfaceEditModeForTest();
                reopened.activateSurfaceFaceSubModeForTest();
                check(reopened.documentStateForTest().selection.surfaces.empty()
                        && reopened.documentStateForTest().selection.surfaceVertices.empty(),
                    "returning to Surface after imported topology navigation does not restore its stale Face or vertex");
            };
            const bool knownTopologyTransition =
                editor::hasAny(dirty, editor::DocumentDirty::Structure)
                && editor::hasAny(dirty, editor::DocumentDirty::LevelGeometry);
            constexpr std::uint32_t knownDirtyMask =
                static_cast<std::uint32_t>(editor::DocumentDirty::LevelGeometry)
                | static_cast<std::uint32_t>(editor::DocumentDirty::Things)
                | static_cast<std::uint32_t>(editor::DocumentDirty::Lights)
                | static_cast<std::uint32_t>(editor::DocumentDirty::Cogs)
                | static_cast<std::uint32_t>(editor::DocumentDirty::Structure);
            const std::uint32_t dirtyMask = static_cast<std::uint32_t>(dirty);
            const bool hasUnsupportedDirtyBits =
                dirty != editor::DocumentDirty::All
                && (dirtyMask & ~knownDirtyMask) != 0u;
            const auto hasExpectedTransitionContext = [&]()
            {
                if (topologyChange)
                {
                    return hasInvalidatedTopologyContext(reopened);
                }
                if (knownTopologyTransition)
                {
                    return hasKnownTopologyInvalidatedContext(reopened);
                }
                return hasImportedFrozenContext(reopened);
            };
            const auto meshRebuildCount = [&reopened]()
            {
                const GlView* const targetView = reopened.glViewForTest();
                return targetView ? targetView->meshRebuildCountForTest() : 0u;
            };

            const std::uint64_t rebuildsBeforeUndo = meshRebuildCount();
            check(triggerHistoryAction(QStringLiteral("edit.undo")),
                "native Strict Surface HIST fixture exposes enabled Undo");
            check(hasExpectedTransitionContext(),
                topologyChange
                    ? "Undo of an imported topology transition clears Strict scope and components and returns to Face"
                    : knownTopologyTransition
                        ? "Undo of a known topology transition preserves its committed Face selection"
                        : "Undo of an imported control transition preserves the frozen Strict Vertex scope");
            if (hasUnsupportedDirtyBits)
            {
                check(meshRebuildCount() > rebuildsBeforeUndo,
                    "native HIST unsupported dirty bits trigger a mesh refresh after Undo");
            }
            if (topologyChange)
            {
                checkTopologyNavigation();
                reseedStrictVertexContext();
            }

            const std::uint64_t rebuildsBeforeRedo = meshRebuildCount();
            check(triggerHistoryAction(QStringLiteral("edit.redo")),
                "native Strict Surface HIST fixture exposes enabled Redo");
            check(hasExpectedTransitionContext(),
                topologyChange
                    ? "Redo of an imported topology transition clears Strict scope and components and returns to Face"
                    : knownTopologyTransition
                        ? "Redo of a known topology transition preserves its committed Face selection"
                        : "Redo of an imported control transition preserves the frozen Strict Vertex scope");
            if (hasUnsupportedDirtyBits)
            {
                check(meshRebuildCount() > rebuildsBeforeRedo,
                    "native HIST unsupported dirty bits trigger a mesh refresh after Redo");
            }
            if (topologyChange)
            {
                checkTopologyNavigation();
                reseedStrictVertexContext();
                const std::uint64_t rebuildsBeforeUndoJump = meshRebuildCount();
                check(reopened.goToHistoryDepthForTest(1u, false)
                        && hasInvalidatedTopologyContext(reopened),
                    "History jump Undo across an imported topology transition clears Strict scope and components and returns to Face");
                if (hasUnsupportedDirtyBits)
                {
                    check(meshRebuildCount() > rebuildsBeforeUndoJump,
                        "native HIST unsupported dirty bits trigger a mesh refresh after History jump Undo");
                }
                checkTopologyNavigation();
                reseedStrictVertexContext();
                const std::uint64_t rebuildsBeforeRedoJump = meshRebuildCount();
                check(reopened.goToHistoryDepthForTest(2u, false)
                        && hasInvalidatedTopologyContext(reopened),
                    "History jump Redo across an imported topology transition clears Strict scope and components and returns to Face");
                if (hasUnsupportedDirtyBits)
                {
                    check(meshRebuildCount() > rebuildsBeforeRedoJump,
                        "native HIST unsupported dirty bits trigger a mesh refresh after History jump Redo");
                }
                checkTopologyNavigation();

            }
            else
            {
                check(reopened.documentStateForTest().level.sectors.size()
                        == expectedSectorCount
                        && reopened.undoDepthForTest() == expectedDepth,
                    "imported geometry-only or Thing-only Structure control retains sector topology after navigation");
                if (knownTopologyTransition)
                {
                    reopened.activateSectorEditModeForTest();
                    reopened.activateSurfaceEditModeForTest();
                    reopened.activateSurfaceFaceSubModeForTest();
                    check(surfaceRefPairs(
                            reopened.documentStateForTest().selection.surfaces)
                            == frozenHistoryScope,
                        "returning to Surface after a known topology transition restores its committed Face selection");
                }
            }

            if (pairedTopologyPrelude)
            {
                const auto hasExpectedMixedJumpContext = [&]()
                {
                    return topologyChange
                        ? hasInvalidatedTopologyContext(reopened)
                        : hasKnownTopologyInvalidatedContext(reopened);
                };
                const char* const mixedJumpExpectation = topologyChange
                    ? "multi-step History jump across an actual ambiguous topology change clears stale Strict context"
                    : "multi-step History jump with an ambiguous no-topology control preserves the committed Face selection";

                reseedStrictFaceContext();
                reopened.jumpToUndoDepthForTest(0u);
                check(reopened.undoDepthForTest() == 0u
                        && hasExpectedMixedJumpContext(),
                    topologyChange
                        ? "multi-step History jump Undo across ambiguous then known transitions clears stale Strict context"
                        : mixedJumpExpectation);
                if (topologyChange)
                {
                    checkTopologyNavigation();
                }

                reseedStrictFaceContext();
                reopened.jumpToUndoDepthForTest(2u);
                check(reopened.undoDepthForTest() == 2u
                        && hasExpectedMixedJumpContext(),
                    topologyChange
                        ? "multi-step History jump Redo across known then ambiguous transitions clears stale Strict context"
                        : mixedJumpExpectation);
                if (topologyChange)
                {
                    checkTopologyNavigation();
                }
            }
        };

        runImportedHistoryCase(
            "none-dirty-sector-delete",
            editor::DocumentDirty::None,
            true,
            true,
            false,
            2u,
            false);
        runImportedHistoryCase(
            "structure-only-sector-delete",
            editor::DocumentDirty::Structure,
            true,
            true,
            false,
            2u,
            false);
        runImportedHistoryCase(
            "unknown-dirty-bit-sector-delete",
            static_cast<editor::DocumentDirty>(1u << 5),
            true,
            true,
            false,
            2u,
            false);
        runImportedHistoryCase(
            "unknown-dirty-bit-with-known-topology-bits-sector-delete",
            static_cast<editor::DocumentDirty>(
                (1u << 5)
                | static_cast<std::uint32_t>(editor::DocumentDirty::Structure)
                | static_cast<std::uint32_t>(editor::DocumentDirty::LevelGeometry)),
            true,
            true,
            false,
            2u,
            false);
        runImportedHistoryCase(
            "known-structure-geometry-prelude-sector-delete",
            editor::DocumentDirty::None,
            true,
            false,
            false,
            2u,
            true);
        runImportedHistoryCase(
            "level-geometry-only-control",
            editor::DocumentDirty::LevelGeometry,
            false,
            false,
            false,
            3u,
            false);
        runImportedHistoryCase(
            "thing-only-structure-control",
            editor::DocumentDirty::Structure,
            false,
            false,
            true,
            3u,
            false);
        runImportedHistoryCase(
            "known-structure-geometry-control",
            editor::DocumentDirty::Structure
                | editor::DocumentDirty::LevelGeometry,
            false,
            false,
            false,
            3u,
            false);
        runImportedHistoryCase(
            "all-dirty-sentinel-coordinate-control",
            editor::DocumentDirty::All,
            false,
            false,
            false,
            3u,
            false);
        runImportedHistoryCase(
            "known-prelude-level-geometry-only-control",
            editor::DocumentDirty::LevelGeometry,
            false,
            false,
            false,
            3u,
            true);
        runImportedHistoryCase(
            "known-prelude-thing-only-structure-control",
            editor::DocumentDirty::Structure,
            false,
            false,
            true,
            3u,
            true);
    }

    editor::DocumentState edgeCleaveState{};
    edgeCleaveState.level.kind = ProjectType::JKDF2;
    edgeCleaveState.level.uvCoordinateSpace = UvCoordinateSpace::Normalized;
    Sector& edgeCleaveSector = edgeCleaveState.level.newSector();
    const std::int32_t v0 = edgeCleaveSector.newVertex(0.0, 0.0, 0.0);
    const std::int32_t v1 = edgeCleaveSector.newVertex(2.0, 0.0, 0.0);
    const std::int32_t v2 = edgeCleaveSector.newVertex(2.0, 2.0, 0.0);
    const std::int32_t v3 = edgeCleaveSector.newVertex(0.0, 2.0, 0.0);
    Surface edgeCleaveFace{};
    edgeCleaveFace.num = 0;
    edgeCleaveFace.vertices = { v0, v1, v2, v3 };
    edgeCleaveFace.texVertices.resize(edgeCleaveFace.vertices.size());
    (void)edgeCleaveSector.addSurface(std::move(edgeCleaveFace));
    edgeCleaveState.selection.surfaces.insert(SurfaceRef{ 0, 0 });

    MainWindow edgeCleaveWindow{};
    edgeCleaveWindow.setAttribute(Qt::WA_DontShowOnScreen);
    edgeCleaveWindow.setDocumentStateForTest(std::move(edgeCleaveState));
    edgeCleaveWindow.activateSurfaceEditModeForTest();
    edgeCleaveWindow.activateSurfaceFaceSubModeForTest();
    auto& edgeCleaveInitialSelection =
        edgeCleaveWindow.mutableDocumentStateForTest().selection;
    edgeCleaveInitialSelection.surfaces = { SurfaceRef{ 0, 0 } };
    const SurfaceRefPairs expectedEdgeCleaveFace{ { 0, 0 } };
    check(surfaceRefPairs(edgeCleaveInitialSelection.surfaces)
            == expectedEdgeCleaveFace,
        "Edge Cleave fixture has the expected current face selected before entering Edge mode");
    QAction* const edgeCleaveStrictAction = edgeCleaveWindow.findChild<QAction*>(
        QStringLiteral("viewport.surface.strict_scope"));
    if (edgeCleaveStrictAction && !edgeCleaveStrictAction->isChecked())
    {
        edgeCleaveStrictAction->setChecked(true);
    }
    const bool strictEnabledBeforeEdge = edgeCleaveStrictAction
        && edgeCleaveStrictAction->isChecked();
    check(strictEnabledBeforeEdge,
        "Edge Cleave fixture enters Edge mode with Strict enabled through its QAction");
    edgeCleaveWindow.activateSurfaceEdgeSubModeForTest();
    GlView* const edgeCleaveView = edgeCleaveWindow.glViewForTest();
    const SurfaceRefPairs edgeCleaveOwnerBeforeSignal = surfaceRefPairs(
        edgeCleaveWindow.documentStateForTest().selection.surfaces);
    check(edgeCleaveOwnerBeforeSignal == expectedEdgeCleaveFace
            && edgeCleaveView
            && surfaceRefPairs(edgeCleaveView->strictSurfaceSelectionScope())
                == expectedEdgeCleaveFace,
        "Edge mode has the expected current owner and frozen scope immediately before Edge Cleave");
    QCborParserError edgeCleaveContextParseError{};
    const QCborMap edgeCleaveContext = QCborValue::fromCbor(
        edgeCleaveWindow.captureRecoveryEditorStateForTest(),
        &edgeCleaveContextParseError).toMap()
            .value(QStringLiteral("surfaceSelectionContext")).toMap();
    const QCborValue edgeCleaveRememberedFaces = edgeCleaveContext.value(
        QStringLiteral("rememberedFaceSelection"));
    const QCborArray expectedEdgeCleaveFaceArray = surfaceReferenceArray({ SurfaceRef{ 0, 0 } });
    check(edgeCleaveContextParseError.error == QCborError::NoError
            && edgeCleaveRememberedFaces.isArray()
            && edgeCleaveRememberedFaces.toArray()
                == expectedEdgeCleaveFaceArray,
        "Edge mode remembers the expected face immediately before Edge Cleave");
    if (edgeCleaveView)
    {
        edgeCleaveView->surfaceEdgeCleaveRequested(
            0, 0, 0, TVector{ 1.0, 0.0, 0.0 }, TVector{ 1.0, 0.0, 0.0 });
    }
    const editor::DocumentState& edgeCleaveResult =
        edgeCleaveWindow.documentStateForTest();
    const SurfaceRefPairs edgeCleaveSelection = surfaceRefPairs(
        edgeCleaveResult.selection.surfaces);
    check(edgeCleaveView
            && edgeCleaveWindow.undoDepthForTest() == 1
            && edgeCleaveResult.level.sectors.size() == 1
            && edgeCleaveResult.level.sectors.front().vertices.size() == 5,
        "successful Edge Cleave commits its topology change");
    check(edgeCleaveWindow.surfaceSelectionModeForTest()
            == edgeCleaveWindow.surfaceFaceModeValueForTest(),
        "successful Edge Cleave returns the Surface submode to Face");
    check(edgeCleaveSelection == SurfaceRefPairs{ { 0, 0 } },
        "successful Edge Cleave preserves its valid current face selection");
    check(edgeCleaveStrictAction && edgeCleaveStrictAction->isChecked()
            && edgeCleaveView && edgeCleaveView->strictSurfaceSelectionEnabled(),
        "successful Edge Cleave preserves the checked app-wide Strict toggle");
}

void runViewportThingOverlayStateTests()
{
    constexpr auto editorCollisionKey =
        "viewport/editor_show_thing_collision_shapes";
    constexpr auto editorBoundsKey =
        "viewport/editor_show_thing_visual_bounds";
    constexpr auto previewCollisionKey =
        "viewport/preview_show_thing_collision_shapes";
    constexpr auto previewBoundsKey =
        "viewport/preview_show_thing_visual_bounds";
    constexpr auto editorModelLodKey =
        "viewport/editor_thing_model_lod_enabled";
    constexpr auto previewModelLodKey =
        "viewport/preview_thing_model_lod_enabled";

    QSettings settings = settingsStore();
    const QStringList keys{
        QString::fromLatin1(editorCollisionKey),
        QString::fromLatin1(editorBoundsKey),
        QString::fromLatin1(previewCollisionKey),
        QString::fromLatin1(previewBoundsKey),
        QString::fromLatin1(editorModelLodKey),
        QString::fromLatin1(previewModelLodKey),
    };
    struct SettingsRestore final
    {
        QSettings& settings;
        QStringList keys;
        std::vector<std::pair<bool, QVariant>> values;

        ~SettingsRestore()
        {
            for (qsizetype index = 0; index < keys.size(); ++index)
            {
                const auto& value = values[static_cast<std::size_t>(index)];
                if (value.first)
                {
                    settings.setValue(keys.at(index), value.second);
                }
                else
                {
                    settings.remove(keys.at(index));
                }
            }
            settings.sync();
        }
    } restoreSettings{ settings, keys, {} };
    restoreSettings.values.reserve(static_cast<std::size_t>(keys.size()));
    for (const QString& key : keys)
    {
        restoreSettings.values.emplace_back(settings.contains(key), settings.value(key));
        settings.remove(key);
    }
    settings.sync();

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    GlView* const view = window.glViewForTest();
    QAction* const collisionAction = window.findChild<QAction*>(
        QStringLiteral("viewport.things.collision_shapes"));
    QAction* const boundsAction = window.findChild<QAction*>(
        QStringLiteral("viewport.things.visual_bounds"));
    QAction* const modelLodAction = window.findChild<QAction*>(
        QStringLiteral("viewport.things.model_lod"));
    QMenu* const viewThingsMenu = window.findChild<QMenu*>(
        QStringLiteral("view.things"));
    QMenu* const viewportThingMenu = window.findChild<QMenu*>(
        QStringLiteral("viewportThingRenderingMenu"));
    if (!view || !collisionAction || !boundsAction || !modelLodAction
        || !viewThingsMenu || !viewportThingMenu)
    {
        check(false, "Thing overlay and model LOD actions are available in both menus");
        return;
    }

    window.setProjectTypeForTest(ProjectType::JKDF2);
    QMetaObject::invokeMethod(viewThingsMenu, "aboutToShow", Qt::DirectConnection);
    check(viewThingsMenu->actions().contains(modelLodAction)
            && viewportThingMenu->actions().contains(modelLodAction)
            && modelLodAction->isVisible() && modelLodAction->isEnabled()
            && !modelLodAction->isChecked() && !view->thingModelLodEnabled(),
        "JKDF2 3DO model LOD is available in both Thing menus and defaults off");
    modelLodAction->setChecked(true);
    check(view->thingModelLodEnabled(),
        "checking model LOD applies the live Editor viewport option");

    check(view->showThingCollisionShapes() && view->showThingVisualBounds()
            && collisionAction->isChecked() && boundsAction->isChecked(),
        "fresh Editor viewport defaults Collision Shapes and Visual Bounds on");

    collisionAction->setChecked(false);
    // Exercise a real toggle back to the Editor's default-on value so the
    // setting is written, rather than treating an untouched default as an
    // explicit user choice.
    boundsAction->setChecked(false);
    boundsAction->setChecked(true);
    view->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    check(!view->showThingCollisionShapes() && !view->showThingVisualBounds()
            && !collisionAction->isChecked() && !boundsAction->isChecked(),
        "fresh Game Preview viewport defaults both Thing overlays off independently");
    check(!view->thingModelLodEnabled() && !modelLodAction->isChecked(),
        "fresh Game Preview keeps model LOD off independently of Editor");

    collisionAction->setChecked(true);
    boundsAction->setChecked(true);
    modelLodAction->setChecked(true);
    settings.sync();
    check(settings.value(QString::fromLatin1(editorCollisionKey)).toBool() == false
            && settings.value(QString::fromLatin1(editorBoundsKey)).toBool() == true
            && settings.value(QString::fromLatin1(previewCollisionKey)).toBool() == true
            && settings.value(QString::fromLatin1(previewBoundsKey)).toBool() == true
            && settings.value(QString::fromLatin1(editorModelLodKey)).toBool() == true
            && settings.value(QString::fromLatin1(previewModelLodKey)).toBool() == true,
        "Thing overlay and model LOD toggles persist per-pipeline app settings");

    MainWindow settingsReloadWindow{};
    settingsReloadWindow.setAttribute(Qt::WA_DontShowOnScreen);
    GlView* const settingsReloadView = settingsReloadWindow.glViewForTest();
    QAction* const settingsReloadLodAction = settingsReloadWindow.findChild<QAction*>(
        QStringLiteral("viewport.things.model_lod"));
    check(settingsReloadView && settingsReloadLodAction
            && !settingsReloadView->showThingCollisionShapes()
            && settingsReloadView->showThingVisualBounds()
            && settingsReloadView->thingModelLodEnabled()
            && settingsReloadLodAction->isChecked(),
        "fresh application workspace loads the saved Editor Thing options and menu check");
    if (settingsReloadView)
    {
        settingsReloadView->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    }
    check(settingsReloadView && settingsReloadLodAction
            && settingsReloadView->showThingCollisionShapes()
            && settingsReloadView->showThingVisualBounds()
            && settingsReloadView->thingModelLodEnabled()
            && settingsReloadLodAction->isChecked(),
        "fresh application workspace loads separate saved Preview Thing options and menu check");

    view->setRenderPipeline(GlView::RenderPipeline::Editor);
    modelLodAction->setChecked(false);
    view->setShowArchThingGizmos(false);
    view->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    view->setShowArchThingGizmos(true);
    const QByteArray savedWorkspace = window.captureRecoveryEditorStateForTest();
    modelLodAction->setChecked(false);
    view->setShowThingCollisionShapes(false);
    view->setShowThingVisualBounds(false);
    view->setShowArchThingGizmos(false);
    view->setRenderPipeline(GlView::RenderPipeline::Editor);
    view->setShowThingCollisionShapes(true);
    view->setShowThingVisualBounds(false);
    view->setShowArchThingGizmos(true);
    modelLodAction->setChecked(true);
    QString restoreWarning{};
    const bool workspaceRestored =
        window.restoreRecoveryEditorStateForTest(savedWorkspace, &restoreWarning);
    check(workspaceRestored && view->renderPipeline() == GlView::RenderPipeline::GamePreview
            && view->showThingCollisionShapes() && view->showThingVisualBounds()
            && view->showArchThingGizmos() && view->thingModelLodEnabled()
            && modelLodAction->isChecked(),
        "serialized workspace restores active Preview overlays, model LOD, and Arch Thing gizmo state");
    view->setRenderPipeline(GlView::RenderPipeline::Editor);
    check(!view->showThingCollisionShapes() && view->showThingVisualBounds()
            && !view->showArchThingGizmos()
            && !view->thingModelLodEnabled()
            && !collisionAction->isChecked() && boundsAction->isChecked()
            && !modelLodAction->isChecked(),
        "serialized workspace independently restores Editor overlays, model LOD, Arch Thing gizmo, and menu checks");

    QCborParserError parseError{};
    QCborValue decoded = QCborValue::fromCbor(savedWorkspace, &parseError);
    QCborMap root = decoded.toMap();
    QCborArray viewport = root.value(QStringLiteral("viewport")).toArray();
    check(parseError.error == QCborError::NoError && decoded.isMap()
            && viewport.size() >= 2
            && viewport.at(0).isArray() && viewport.at(1).isArray(),
        "captured SED EDIT workspace contains both viewport records");
    check(viewport.size() >= 2
            && viewport.at(0).toArray().size() == 18
            && viewport.at(0).toArray().at(16).isBool()
            && !viewport.at(0).toArray().at(16).toBool()
            && viewport.at(1).toArray().size() == 18
            && viewport.at(1).toArray().at(16).isBool()
            && viewport.at(1).toArray().at(16).toBool(),
        "18-entry viewport records retain distinct Arch Thing gizmo values at legacy slot 16");
    if (viewport.size() >= 2 && viewport.at(0).isArray()
        && viewport.at(1).isArray())
    {
        const QCborArray editor = viewport.at(0).toArray();
        const QCborArray preview = viewport.at(1).toArray();
        check(editor.size() == 18 && preview.size() == 18
                && editor.at(17).toMap().value(
                    QStringLiteral("thingModelLodEnabled")).isBool()
                && !editor.at(17).toMap().value(
                    QStringLiteral("thingModelLodEnabled")).toBool()
                && preview.at(17).toMap().value(
                    QStringLiteral("thingModelLodEnabled")).toBool(),
            "named SED EDIT flags serialize distinct Editor and Preview model LOD states");
    }
    if (parseError.error != QCborError::NoError || viewport.size() < 2
        || !viewport.at(0).isArray() || !viewport.at(1).isArray())
    {
        return;
    }

    const auto legacyViewportRecord = [](QCborValue value, const bool truncateTo16)
    {
        QCborArray record = value.toArray();
        if (record.size() == 18)
        {
            record.removeLast();
        }
        if (truncateTo16 && record.size() == 17)
        {
            record.removeLast();
        }
        return record;
    };
    viewport[0] = legacyViewportRecord(viewport.at(0), true);
    viewport[1] = legacyViewportRecord(viewport.at(1), false);
    root.insert(QStringLiteral("viewport"), viewport);
    const QByteArray legacyPayload = ui::recovery::serializeCanonicalCbor(
        QCborValue(root));

    settings.setValue(QString::fromLatin1(editorCollisionKey), true);
    settings.setValue(QString::fromLatin1(editorBoundsKey), false);
    settings.setValue(QString::fromLatin1(previewCollisionKey), false);
    settings.setValue(QString::fromLatin1(previewBoundsKey), true);
    settings.setValue(QString::fromLatin1(editorModelLodKey), true);
    settings.setValue(QString::fromLatin1(previewModelLodKey), true);
    settings.sync();
    MainWindow legacyWindow{};
    legacyWindow.setAttribute(Qt::WA_DontShowOnScreen);
    GlView* const legacyView = legacyWindow.glViewForTest();
    QString legacyWarning{};
    const bool legacyRestored = legacyWindow.restoreRecoveryEditorStateForTest(
        legacyPayload, &legacyWarning);
    if (legacyView)
    {
        legacyView->setRenderPipeline(GlView::RenderPipeline::Editor);
    }
    check(legacyRestored && legacyView
            && legacyView->showThingCollisionShapes()
            && !legacyView->showThingVisualBounds()
            && !legacyView->thingModelLodEnabled(),
        "legacy 16-entry Editor viewport keeps model LOD off despite app settings");
    if (legacyView)
    {
        legacyView->setRenderPipeline(GlView::RenderPipeline::GamePreview);
    }
    check(legacyView && !legacyView->showThingCollisionShapes()
            && legacyView->showThingVisualBounds()
            && !legacyView->thingModelLodEnabled(),
        "legacy 17-entry Preview viewport keeps model LOD off despite app settings");

    decoded = QCborValue::fromCbor(savedWorkspace, &parseError);
    root = decoded.toMap();
    viewport = root.value(QStringLiteral("viewport")).toArray();
    for (qsizetype index = 0; index < 2; ++index)
    {
        QCborArray record = viewport.at(index).toArray();
        QCborMap flags = record.at(17).toMap();
        flags.remove(QStringLiteral("thingModelLodEnabled"));
        record[17] = flags;
        viewport[index] = record;
    }
    root.insert(QStringLiteral("viewport"), viewport);
    const QByteArray missingLodPayload = ui::recovery::serializeCanonicalCbor(
        QCborValue(root));
    MainWindow missingLodWindow{};
    missingLodWindow.setAttribute(Qt::WA_DontShowOnScreen);
    GlView* const missingLodView = missingLodWindow.glViewForTest();
    const bool missingLodRestored = missingLodWindow.restoreRecoveryEditorStateForTest(
        missingLodPayload);
    check(missingLodRestored && missingLodView
            && !missingLodView->thingModelLodEnabled(),
        "missing named model LOD flag restores off even when app default is on");
    if (missingLodView)
    {
        missingLodView->setRenderPipeline(GlView::RenderPipeline::Editor);
    }
    check(missingLodView && !missingLodView->thingModelLodEnabled(),
        "missing named model LOD flag restores off in Editor too");

    decoded = QCborValue::fromCbor(savedWorkspace, &parseError);
    root = decoded.toMap();
    viewport = root.value(QStringLiteral("viewport")).toArray();
    QCborArray editorRecord = viewport.at(0).toArray();
    QCborMap editorFlags = editorRecord.at(17).toMap();
    editorFlags.insert(QStringLiteral("showArchThingGizmos"), true);
    editorRecord[17] = editorFlags;
    viewport[0] = editorRecord;
    QCborArray previewRecord = viewport.at(1).toArray();
    QCborMap previewFlags = previewRecord.at(17).toMap();
    previewFlags.insert(QStringLiteral("showWireframeOverlay"), false);
    previewRecord[17] = previewFlags;
    viewport[1] = previewRecord;
    root.insert(QStringLiteral("viewport"), viewport);
    const QByteArray unknownOptionalPayload = ui::recovery::serializeCanonicalCbor(
        QCborValue(root));
    QString unknownFieldWarning{};
    const bool unknownFieldAccepted = window.validateRecoveryEditorStateForTest(
        unknownOptionalPayload, &unknownFieldWarning);
    check(unknownFieldAccepted,
        "future named gizmo and render flags are ignored in both viewport records");
    if (unknownFieldAccepted)
    {
        (void)window.restoreRecoveryEditorStateForTest(unknownOptionalPayload);
        const QCborMap resaved = QCborValue::fromCbor(
            window.captureRecoveryEditorStateForTest()).toMap();
        const QCborArray resavedViewports = resaved.value(
            QStringLiteral("viewport")).toArray();
        const QCborMap resavedEditorFlags = resavedViewports.at(0).toArray()
            .at(17).toMap();
        const QCborMap resavedPreviewFlags = resavedViewports.at(1).toArray()
            .at(17).toMap();
        check(!resavedEditorFlags.contains(QStringLiteral("showArchThingGizmos"))
                && !resavedPreviewFlags.contains(QStringLiteral("showWireframeOverlay")),
            "an older writer drops unknown future viewport flags when re-saving");
    }

    decoded = QCborValue::fromCbor(savedWorkspace, &parseError);
    root = decoded.toMap();
    viewport = root.value(QStringLiteral("viewport")).toArray();
    editorRecord = viewport.at(0).toArray();
    editorFlags = editorRecord.at(17).toMap();
    editorFlags.insert(QStringLiteral("thingCollisionShapes"), QStringLiteral("yes"));
    editorRecord[17] = editorFlags;
    viewport[0] = editorRecord;
    root.insert(QStringLiteral("viewport"), viewport);
    const QByteArray malformedKnownFieldPayload = ui::recovery::serializeCanonicalCbor(
        QCborValue(root));
    QString malformedWarning{};
    check(!window.validateRecoveryEditorStateForTest(
            malformedKnownFieldPayload, &malformedWarning),
        "malformed known overlay values reject the optional workspace state");

    decoded = QCborValue::fromCbor(savedWorkspace, &parseError);
    root = decoded.toMap();
    viewport = root.value(QStringLiteral("viewport")).toArray();
    editorRecord = viewport.at(0).toArray();
    editorFlags = editorRecord.at(17).toMap();
    editorFlags.insert(QStringLiteral("thingModelLodEnabled"),
        QStringLiteral("true"));
    editorRecord[17] = editorFlags;
    viewport[0] = editorRecord;
    root.insert(QStringLiteral("viewport"), viewport);
    const QByteArray malformedModelLodPayload = ui::recovery::serializeCanonicalCbor(
        QCborValue(root));
    check(!window.validateRecoveryEditorStateForTest(malformedModelLodPayload),
        "non-boolean model LOD flag rejects the optional workspace state");

    window.setProjectTypeForTest(ProjectType::IJIM);
    QMetaObject::invokeMethod(viewThingsMenu, "aboutToShow", Qt::DirectConnection);
    check(!modelLodAction->isVisible() && !modelLodAction->isEnabled(),
        "model LOD option is unavailable for IJIM projects");
    window.setProjectTypeForTest(ProjectType::MOTS);
    QMetaObject::invokeMethod(viewThingsMenu, "aboutToShow", Qt::DirectConnection);
    check(modelLodAction->isVisible() && modelLodAction->isEnabled(),
        "model LOD option is available for MotS projects");
}

} // namespace sed::app_tests
