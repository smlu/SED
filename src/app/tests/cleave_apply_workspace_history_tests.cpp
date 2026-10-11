#include "test_common.hpp"

#include "app_settings.hpp"
#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/sed.hpp"
#include "sed/editor/document.hpp"
#include "sed/level/build.hpp"
#include "sed/level/cleave.hpp"
#include "ui/project/sed_editor_file_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QCborArray>
#include <QCborMap>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QToolButton>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <unordered_set>
#include <string>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

class LayerColorPreferenceRestore final
{
public:
    LayerColorPreferenceRestore()
    {
        QSettings settings = settingsStore();
        hadValue_ = settings.contains(kLayerColorsKey);
        value_ = settings.value(kLayerColorsKey);
    }

    ~LayerColorPreferenceRestore()
    {
        QSettings settings = settingsStore();
        if (hadValue_)
        {
            settings.setValue(kLayerColorsKey, value_);
        }
        else
        {
            settings.remove(kLayerColorsKey);
        }
        settings.sync();
    }

private:
    static constexpr const char* kLayerColorsKey = "viewport/layers/colors";
    bool hadValue_{};
    QVariant value_{};
};

editor::DocumentState makeCleaveDocument()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.header.version = 1;
    level::levelCreateCube(
        state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    state.selection.sectors.insert(0);
    return state;
}

editor::DocumentState makeLayerDocument()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.header.version = 1;
    state.level.layers = { "Geometry", "Gameplay", "Lighting" };
    level::levelCreateCube(
        state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    state.selection.sectors.insert(0);
    return state;
}

void prepareCleaveWindow(MainWindow& window)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    // Establish the Sector editor before installing the selection. Entering
    // Sector mode after setDocumentStateForTest restores the remembered live
    // Sector selection (empty in a fresh window) over the fixture targets.
    window.activateSectorEditModeForTest();
    window.setDocumentStateForTest(makeCleaveDocument());
    window.activateSectorEditModeForTest();
    window.resize(1280, 900);

    GlView* const view = window.glViewForTest();
    if (!view)
    {
        return;
    }

    view->setLevel(
        &window.documentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->setSelectionState(
        0, -1, -1, -1, window.documentStateForTest().selection);
    view->setCameraProjectionType(
        sed::app::ViewportCamera::ProjectionType::Orthographic);
    view->resetEditorTopDownView();
    view->setCameraOrthoScale(16.0f);
    view->setShowGrid(false);
    view->setShowSolid(false);
    view->setShowTextures(false);
    view->setShowWireframe(false);
    view->setShowVertices(false);
    view->setShowFog(false);
    view->setShowThings(false);
    view->setSectorCleaveToolEnabled(true);
    view->setCleaveStagedMode(true);
    view->setCleaveChainEnabled(true);
    view->setCleaveBounded(false);
    view->captureStagedCleaveContextForTest(false);
    const GlView::RecoveryState::CleaveDraftSnapshot capturedContext =
        view->captureStagedCleaveDraftState();
    check(window.documentStateForTest().selection.sectors
                == std::unordered_set<std::int32_t>{ 0 }
            && capturedContext.context.targetSectors
                == std::vector<std::int32_t>{ 0 },
        "Apply workspace fixture preserves its selected Sector in the captured Cleave context");

    window.show();
    window.activateWindow();
    QApplication::processEvents(QEventLoop::AllEvents);
    window.updateEditActionsForTest();

    if (QToolButton* const multiCut = window.findChild<QToolButton*>(
            QStringLiteral("cleaveMultiCutButton")))
    {
        multiCut->setChecked(true);
    }
    if (QToolButton* const chain = window.findChild<QToolButton*>(
            QStringLiteral("cleaveChainButton")))
    {
        chain->setChecked(false);
    }
    if (QToolButton* const bounded = window.findChild<QToolButton*>(
            QStringLiteral("cleaveBoundedButton")))
    {
        bounded->setChecked(false);
    }
    QAction* const cleaveAction = window.findChild<QAction*>(
        QStringLiteral("viewport.sector.cleave"));
    if (cleaveAction && !cleaveAction->isChecked())
    {
        cleaveAction->setChecked(true);
    }
    check(cleaveAction && cleaveAction->isChecked()
            && view->sectorCleaveToolEnabled(),
        "Apply workspace fixture owns the checked viewport Cleave action");

    view->captureStagedCleaveContextForTest(false);
    QApplication::setActiveWindow(&window);
    view->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents(QEventLoop::AllEvents);
}

bool sameLine(
    const GlView::RecoveryState::CleaveLine& left,
    const GlView::RecoveryState::CleaveLine& right)
{
    return left.first.x == right.first.x
        && left.first.y == right.first.y
        && left.first.z == right.first.z
        && left.second.x == right.second.x
        && left.second.y == right.second.y
        && left.second.z == right.second.z
        && left.planeNormal.x == right.planeNormal.x
        && left.planeNormal.y == right.planeNormal.y
        && left.planeNormal.z == right.planeNormal.z
        && left.constructionPlaneNormal.x == right.constructionPlaneNormal.x
        && left.constructionPlaneNormal.y == right.constructionPlaneNormal.y
        && left.constructionPlaneNormal.z == right.constructionPlaneNormal.z
        && left.angleReferenceAxis.x == right.angleReferenceAxis.x
        && left.angleReferenceAxis.y == right.angleReferenceAxis.y
        && left.angleReferenceAxis.z == right.angleReferenceAxis.z
        && left.linkedToPrevious == right.linkedToPrevious
        && left.linkedToNext == right.linkedToNext
        && left.authoredCamera.valid == right.authoredCamera.valid
        && left.authoredCamera.projectionType == right.authoredCamera.projectionType
        && left.authoredCamera.orbit.center == right.authoredCamera.orbit.center
        && left.authoredCamera.orbit.pan == right.authoredCamera.orbit.pan
        && left.authoredCamera.orbit.radius == right.authoredCamera.orbit.radius
        && left.authoredCamera.orbit.distance == right.authoredCamera.orbit.distance
        && left.authoredCamera.orbit.yawDeg == right.authoredCamera.orbit.yawDeg
        && left.authoredCamera.orbit.pitchDeg == right.authoredCamera.orbit.pitchDeg
        && left.authoredCamera.orbit.rollDeg == right.authoredCamera.orbit.rollDeg
        && left.authoredCamera.fovXDeg == right.authoredCamera.fovXDeg
        && left.authoredCamera.orthoScale == right.authoredCamera.orthoScale
        && left.authoredCamera.nearPlane == right.authoredCamera.nearPlane
        && left.authoredCamera.farPlane == right.authoredCamera.farPlane
        && left.authoredCamera.clipEnabled == right.authoredCamera.clipEnabled;
}

bool sameDraft(
    const GlView::RecoveryState::CleaveDraftSnapshot& left,
    const GlView::RecoveryState::CleaveDraftSnapshot& right)
{
    if (left.lines.size() != right.lines.size()
        || left.applyOrder != right.applyOrder
        || left.selectedCleaveLines != right.selectedCleaveLines
        || left.selectedCleaveLine != right.selectedCleaveLine
        || left.chainOrigin.has_value() != right.chainOrigin.has_value()
        || left.context.valid != right.context.valid
        || left.context.surfaceMode != right.context.surfaceMode
        || left.context.viewportWidth != right.context.viewportWidth
        || left.context.viewportHeight != right.context.viewportHeight
        || left.context.sourceSurface.sector != right.context.sourceSurface.sector
        || left.context.sourceSurface.surface != right.context.sourceSurface.surface
        || left.context.targetSectors != right.context.targetSectors
        || left.context.targetSurfaces.size() != right.context.targetSurfaces.size()
        || left.context.projection != right.context.projection
        || left.context.view != right.context.view)
    {
        return false;
    }

    if (left.chainOrigin
        && (left.chainOrigin->lineIndex != right.chainOrigin->lineIndex
            || left.chainOrigin->endpoint != right.chainOrigin->endpoint))
    {
        return false;
    }

    for (std::size_t index = 0; index < left.lines.size(); ++index)
    {
        if (!sameLine(left.lines[index], right.lines[index]))
        {
            return false;
        }
    }
    for (std::size_t index = 0; index < left.context.targetSurfaces.size(); ++index)
    {
        if (left.context.targetSurfaces[index].sector
                != right.context.targetSurfaces[index].sector
            || left.context.targetSurfaces[index].surface
                != right.context.targetSurfaces[index].surface)
        {
            return false;
        }
    }
    return true;
}

bool sameHistory(
    const GlView::RecoveryState::CleaveHistory& left,
    const GlView::RecoveryState::CleaveHistory& right)
{
    if (left.cursor != right.cursor
        || left.snapshots.size() != right.snapshots.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.snapshots.size(); ++index)
    {
        if (!sameDraft(left.snapshots[index], right.snapshots[index]))
        {
            return false;
        }
    }
    return true;
}

bool sameWorkspace(
    const GlView::RecoveryState::CleaveWorkspace& left,
    const GlView::RecoveryState::CleaveWorkspace& right)
{
    const auto& leftShape = left.shapeOptions;
    const auto& rightShape = right.shapeOptions;
    return sameDraft(left.draft, right.draft)
        && left.history.has_value() == right.history.has_value()
        && (!left.history || sameHistory(*left.history, *right.history))
        && left.bounded == right.bounded
        && left.stagedMode == right.stagedMode
        && left.chainEnabled == right.chainEnabled
        && left.measurementsVisible == right.measurementsVisible
        && left.measureUnit == right.measureUnit
        && left.anglesVisible == right.anglesVisible
        && left.lengthsVisible == right.lengthsVisible
        && left.lineNumbersVisible == right.lineNumbersVisible
        && left.angleStepDegrees == right.angleStepDegrees
        && left.transformMode == right.transformMode
        && leftShape.kind == rightShape.kind
        && leftShape.width == rightShape.width
        && leftShape.height == rightShape.height
        && leftShape.dimensionsLinked == rightShape.dimensionsLinked
        && leftShape.segments == rightShape.segments
        && leftShape.points == rightShape.points
        && leftShape.placementArmed == rightShape.placementArmed
        && leftShape.cameraPlane == rightShape.cameraPlane
        && leftShape.rotationDegrees == rightShape.rotationDegrees;
}

GlView::RecoveryState::CleaveDraftSnapshot currentDraft(
    const GlView::RecoveryState& state)
{
    return GlView::RecoveryState::CleaveDraftSnapshot{
        state.cleaveLines,
        state.cleaveContext,
        state.cleaveChainOrigin,
        state.applyOrder,
        state.selectedCleaveLines,
        state.selectedCleaveLine,
    };
}

std::optional<assets::SedFile> loadNativeFile(
    const std::filesystem::path& path)
{
    try
    {
        return assets::loadSedFile(path);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

QByteArray nativeEditorPayload(const assets::SedFile& file)
{
    return ui::project::findSedOptionalSection(
        file.optionalSections, ui::project::SedEditorStateSection);
}

QCborMap editorRoot(const QByteArray& payload)
{
    const QCborValue root = QCborValue::fromCbor(payload);
    return root.isMap() ? root.toMap() : QCborMap{};
}

QCborMap currentCleaveMap(const QCborMap& root)
{
    const QCborArray viewport = root.value(QStringLiteral("viewport")).toArray();
    return viewport.size() > 38 && viewport.at(38).isMap()
        ? viewport.at(38).toMap()
        : QCborMap{};
}

int saveWithoutDialog(QAction* const action)
{
    int messageCount = 0;
    QTimer driver{};
    driver.setInterval(5);
    QObject::connect(&driver, &QTimer::timeout, [&messageCount]()
    {
        if (auto* const chooser = qobject_cast<QFileDialog*>(
                QApplication::activeModalWidget()))
        {
            chooser->reject();
        }
        else if (auto* const message = qobject_cast<QMessageBox*>(
                     QApplication::activeModalWidget()))
        {
            ++messageCount;
            message->accept();
        }
    });
    driver.start();
    if (action)
    {
        action->trigger();
    }
    driver.stop();
    return messageCount;
}

QCborMap sidecarEntry(const QByteArray& editorPayload, const char* payloadName)
{
    const QCborValue rootValue = QCborValue::fromCbor(editorPayload);
    if (!rootValue.isMap())
    {
        return {};
    }
    const QCborMap root = rootValue.toMap();
    const QCborValue historyValue = root.value(QStringLiteral("workspaceHistory"));
    if (!historyValue.isMap())
    {
        return {};
    }
    const QCborArray entries = historyValue.toMap()
        .value(QStringLiteral("entries")).toArray();
    for (const QCborValue& entryValue : entries)
    {
        if (!entryValue.isMap())
        {
            continue;
        }
        const QCborMap entry = entryValue.toMap();
        if (entry.value(QString::fromLatin1(payloadName)).isMap())
        {
            return entry;
        }
    }
    return {};
}

bool rewriteNativeEditorPayload(
    const assets::SedFile& source,
    const std::filesystem::path& destination,
    const QByteArray& editorPayload,
    const bool omitHistory)
{
    std::vector<assets::SedOptionalSection> sections{};
    sections.reserve(source.optionalSections.size());
    bool replacedEditor = false;
    for (const assets::SedOptionalSection& section : source.optionalSections)
    {
        if (section.type == ui::project::SedEditorStateSection)
        {
            sections.push_back(ui::project::makeSedOptionalSection(
                ui::project::SedEditorStateSection, editorPayload));
            replacedEditor = true;
        }
        else if (omitHistory
            && section.type == ui::project::SedHistorySection)
        {
            continue;
        }
        else
        {
            sections.push_back(section);
        }
    }
    if (!replacedEditor)
    {
        return false;
    }

    const std::vector<std::byte> bytes = assets::saveToSedBytes(
        source.level, sections,
        assets::SedSerializationProfile::ProtectedV1,
        std::max<std::int64_t>(0, source.timestamp));
    QFile output(QString::fromStdWString(destination.wstring()));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }
    const QByteArray encoded(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<qsizetype>(bytes.size()));
    return output.write(encoded) == encoded.size() && output.flush();
}

QByteArray mutateWorkspaceHistoryBinding(
    const QByteArray& editorPayload,
    const bool corruptHash,
    const bool duplicateEntry,
    const bool wrongCounts = false)
{
    QCborParserError parserError{};
    const QCborValue rootValue = QCborValue::fromCbor(
        editorPayload, &parserError);
    if (parserError.error != QCborError::NoError || !rootValue.isMap())
    {
        return {};
    }
    QCborMap root = rootValue.toMap();
    QCborMap workspaceHistory = root.value(
        QStringLiteral("workspaceHistory")).toMap();
    QCborArray entries = workspaceHistory.value(
        QStringLiteral("entries")).toArray();
    if (entries.isEmpty() || !entries.at(0).isMap())
    {
        return {};
    }
    if (corruptHash)
    {
        QCborMap entry = entries.at(0).toMap();
        QByteArray beforeHash = entry.value(
            QStringLiteral("beforeSha256")).toByteArray();
        if (beforeHash.size() != 32)
        {
            return {};
        }
        beforeHash[0] = static_cast<char>(beforeHash.at(0) ^ 0x01);
        entry.insert(QStringLiteral("beforeSha256"), beforeHash);
        entries[0] = entry;
    }
    if (duplicateEntry)
    {
        entries.append(entries.at(0));
    }
    workspaceHistory.insert(QStringLiteral("entries"), entries);
    if (wrongCounts)
    {
        workspaceHistory.insert(QStringLiteral("redoCount"), 2);
    }
    root.insert(QStringLiteral("workspaceHistory"), workspaceHistory);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray mutatePendingRedoIndex(const QByteArray& editorPayload)
{
    QCborMap root = editorRoot(editorPayload);
    QCborMap workspaceHistory = root.value(
        QStringLiteral("workspaceHistory")).toMap();
    QCborMap pendingRedo = workspaceHistory.value(
        QStringLiteral("pendingRedo")).toMap();
    if (root.isEmpty() || workspaceHistory.isEmpty() || pendingRedo.isEmpty())
    {
        return {};
    }
    // The fixture has one actual Redo transition, so index 1 is an invalid
    // positional hint. This exercises the checked reader without changing
    // the independent current Cleave draft or its local timeline.
    pendingRedo.insert(QStringLiteral("index"), 1);
    workspaceHistory.insert(QStringLiteral("pendingRedo"), pendingRedo);
    root.insert(QStringLiteral("workspaceHistory"), workspaceHistory);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray mutateApplyAttachmentSentinel(const QByteArray& editorPayload)
{
    QCborMap root = editorRoot(editorPayload);
    QCborMap workspaceHistory = root.value(
        QStringLiteral("workspaceHistory")).toMap();
    QCborArray entries = workspaceHistory.value(
        QStringLiteral("entries")).toArray();
    if (root.isEmpty() || entries.isEmpty() || !entries.at(0).isMap())
    {
        return {};
    }
    QCborMap entry = entries.at(0).toMap();
    QCborMap cleave = entry.value(QStringLiteral("cleave")).toMap();
    QCborArray active = cleave.value(QStringLiteral("active")).toArray();
    if (active.size() != 7)
    {
        return {};
    }
    // A Surface identity must be either the complete (-1,-1) sentinel or a
    // complete in-range (sector,surface) pair.
    active[1] = 0;
    active[2] = -2;
    cleave.insert(QStringLiteral("active"), active);
    entry.insert(QStringLiteral("cleave"), cleave);
    entries[0] = entry;
    workspaceHistory.insert(QStringLiteral("entries"), entries);
    root.insert(QStringLiteral("workspaceHistory"), workspaceHistory);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray mutateApplyAttachmentTimelinePreflight(const QByteArray& editorPayload)
{
    QCborMap root = editorRoot(editorPayload);
    QCborMap workspaceHistory = root.value(
        QStringLiteral("workspaceHistory")).toMap();
    QCborArray entries = workspaceHistory.value(
        QStringLiteral("entries")).toArray();
    if (root.isEmpty() || entries.isEmpty() || !entries.at(0).isMap())
    {
        return {};
    }
    QCborMap entry = entries.at(0).toMap();
    QCborMap cleave = entry.value(QStringLiteral("cleave")).toMap();
    QCborMap workspace = cleave.value(QStringLiteral("workspace")).toMap();
    QCborMap history = workspace.value(QStringLiteral("history")).toMap();
    QCborArray snapshots = history.value(QStringLiteral("snapshots")).toArray();
    const qint64 cursor = history.value(QStringLiteral("cursor")).toInteger(-1);
    if (snapshots.isEmpty() || cursor < 0 || cursor >= snapshots.size())
    {
        return {};
    }
    const QCborValue currentSnapshot = snapshots.at(static_cast<qsizetype>(cursor));
    // 65 snapshots violate the local-timeline count cap. Repeating the valid
    // cursor snapshot keeps the attachment structurally coherent apart from
    // the deliberate count excess and stays small enough for an ordinary
    // protected native SED payload.
    snapshots.clear();
    for (int index = 0; index < 65; ++index)
    {
        snapshots.append(currentSnapshot);
    }
    history.insert(QStringLiteral("cursor"), 0);
    history.insert(QStringLiteral("snapshots"), snapshots);
    workspace.insert(QStringLiteral("history"), history);
    cleave.insert(QStringLiteral("workspace"), workspace);
    entry.insert(QStringLiteral("cleave"), cleave);
    entries[0] = entry;
    workspaceHistory.insert(QStringLiteral("entries"), entries);
    root.insert(QStringLiteral("workspaceHistory"), workspaceHistory);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray mutateApplyAttachmentPeerWorkspace(const QByteArray& editorPayload)
{
    QCborMap root = editorRoot(editorPayload);
    QCborMap workspaceHistory = root.value(
        QStringLiteral("workspaceHistory")).toMap();
    QCborArray entries = workspaceHistory.value(
        QStringLiteral("entries")).toArray();
    const QCborValue peer = currentCleaveMap(root).value(
        QStringLiteral("peerWorkspace"));
    if ( root.isEmpty() || entries.isEmpty() || !entries.at(0).isMap()
        || !peer.isMap() )
    {
        return {};
    }
    QCborMap entry = entries.at(0).toMap();
    QCborMap cleave = entry.value(QStringLiteral("cleave")).toMap();
    QCborMap workspace = cleave.value(QStringLiteral("workspace")).toMap();
    if ( workspace.isEmpty() )
    {
        return {};
    }
    workspace.insert(QStringLiteral("peerWorkspace"), peer);
    cleave.insert(QStringLiteral("workspace"), workspace);
    entry.insert(QStringLiteral("cleave"), cleave);
    entries[0] = entry;
    workspaceHistory.insert(QStringLiteral("entries"), entries);
    root.insert(QStringLiteral("workspaceHistory"), workspaceHistory);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

QByteArray mutateCurrentCleaveForPendingMismatch(
    const QByteArray& editorPayload,
    const bool changeOrder,
    const bool changeBounded,
    const bool changeLine)
{
    QCborMap root = editorRoot(editorPayload);
    QCborArray viewport = root.value(QStringLiteral("viewport")).toArray();
    if (root.isEmpty() || viewport.size() <= 38 || !viewport.at(38).isMap())
    {
        return {};
    }
    QCborMap cleave = viewport.at(38).toMap();
    if (changeBounded)
    {
        const QCborValue bounded = cleave.value(QStringLiteral("bounded"));
        if (!bounded.isBool())
        {
            return {};
        }
        cleave.insert(QStringLiteral("bounded"), !bounded.toBool());
    }
    if (changeOrder)
    {
        QCborArray order{};
        order.append(1);
        order.append(0);
        cleave.insert(QStringLiteral("applyOrder"), order);

        QCborMap history = cleave.value(QStringLiteral("history")).toMap();
        QCborArray snapshots = history.value(QStringLiteral("snapshots")).toArray();
        const qint64 cursor = history.value(QStringLiteral("cursor")).toInteger(-1);
        if (snapshots.isEmpty() || cursor < 0 || cursor >= snapshots.size())
        {
            return {};
        }
        QCborMap currentSnapshot = snapshots.at(
            static_cast<qsizetype>(cursor)).toMap();
        currentSnapshot.insert(QStringLiteral("applyOrder"), order);
        snapshots[static_cast<qsizetype>(cursor)] = currentSnapshot;
        history.insert(QStringLiteral("snapshots"), snapshots);
        cleave.insert(QStringLiteral("history"), history);
    }
    if (changeLine)
    {
        const auto shiftFirstLine = [](QCborMap* const draft) -> bool
        {
            if (!draft)
            {
                return false;
            }
            QCborArray lines = draft->value(QStringLiteral("lines")).toArray();
            if (lines.isEmpty() || !lines.at(0).isArray())
            {
                return false;
            }
            QCborArray line = lines.at(0).toArray();
            if (line.size() != 15 || !line.at(0).isArray()
                || !line.at(1).isArray())
            {
                return false;
            }
            QCborArray first = line.at(0).toArray();
            QCborArray second = line.at(1).toArray();
            if (first.size() != 3 || second.size() != 3
                || !first.at(0).isDouble() || !second.at(0).isDouble())
            {
                return false;
            }
            first[0] = first.at(0).toDouble() + 0.125;
            second[0] = second.at(0).toDouble() + 0.125;
            line[0] = first;
            line[1] = second;
            lines[0] = line;
            draft->insert(QStringLiteral("lines"), lines);
            return true;
        };
        if (!shiftFirstLine(&cleave))
        {
            return {};
        }
        QCborMap history = cleave.value(QStringLiteral("history")).toMap();
        QCborArray snapshots = history.value(QStringLiteral("snapshots")).toArray();
        const qint64 cursor = history.value(QStringLiteral("cursor")).toInteger(-1);
        if (snapshots.isEmpty() || cursor < 0 || cursor >= snapshots.size())
        {
            return {};
        }
        QCborMap currentSnapshot = snapshots.at(
            static_cast<qsizetype>(cursor)).toMap();
        if (!shiftFirstLine(&currentSnapshot))
        {
            return {};
        }
        snapshots[static_cast<qsizetype>(cursor)] = currentSnapshot;
        history.insert(QStringLiteral("snapshots"), snapshots);
        cleave.insert(QStringLiteral("history"), history);
    }
    viewport[38] = cleave;
    root.insert(QStringLiteral("viewport"), viewport);
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

enum class ApplyPeerRedoRoute
{
    Dormant,
    Local,
    Document,
    History,
};

void runAppliedFamilyOnlyHistoryTest(
    const bool surfaceApply, const ApplyPeerRedoRoute peerRedoRoute)
{
    MainWindow window{};
    prepareCleaveWindow(window);
    GlView* const view = window.glViewForTest();
    if ( !view )
    {
        return;
    }
    check(level::levelCreateSectorPrimitiveBox(window.mutableDocumentStateForTest().level,
              level::SectorPrimitive::Cube, { 3, 0, 0 }, { 4, 1, 1 }) == 1,
        "family-only Apply fixture creates a separate untouched peer target");
    view->notifyLevelGeometryChanged();
    const auto appliedFamily = surfaceApply ? GlView::CleaveWorkspaceFamily::Surface
        : GlView::CleaveWorkspaceFamily::Sector;
    const auto peerFamily = surfaceApply ? GlView::CleaveWorkspaceFamily::Sector
        : GlView::CleaveWorkspaceFamily::Surface;
    if ( surfaceApply )
    {
        window.activateSurfaceEditModeForTest();
        window.activateSurfaceFaceSubModeForTest();
        auto& selection = window.mutableDocumentStateForTest().selection;
        selection.clear();
        selection.surfaces.insert({ 0, 0 });
        view->setSelectionState(0, 0, -1, -1, selection);
        QAction* const surfaceCleave = window.findChild<QAction*>(
            QStringLiteral("viewport.surface.cleave"));
        check(surfaceCleave != nullptr,
            "reverse family Apply fixture exposes the real Surface Cleave action");
        if ( !surfaceCleave )
        {
            return;
        }
        surfaceCleave->setChecked(true);
        if ( QToolButton* const multiCut = window.findChild<QToolButton*>(
                QStringLiteral("cleaveMultiCutButton")) )
        {
            multiCut->setChecked(true);
        }
        if ( QToolButton* const chain = window.findChild<QToolButton*>(
                QStringLiteral("cleaveChainButton")) )
        {
            chain->setChecked(false);
        }
        if ( QToolButton* const bounded = window.findChild<QToolButton*>(
                QStringLiteral("cleaveBoundedButton")) )
        {
            bounded->setChecked(false);
        }
    }
    view->captureStagedCleaveContextForTest(surfaceApply);
    view->addStagedCleaveLineForTest(
        QVector3D(0.5f, -1.0f, surfaceApply ? 0.0f : 0.5f),
        QVector3D(0.5f, 2.0f, surfaceApply ? 0.0f : 0.5f),
        QVector3D(1.0f, 0.0f, 0.0f));
    auto appliedWorkspace = view->captureStagedCleaveWorkspace(
        appliedFamily);
    check(appliedWorkspace.draft.lines.size() == 1u
            && appliedWorkspace.draft.context.valid
            && appliedWorkspace.draft.context.surfaceMode == surfaceApply,
        "family-only Apply fixture captures one valid authored line in the applied family");
    if ( appliedWorkspace.draft.lines.size() != 1u
        || !appliedWorkspace.draft.context.valid )
    {
        return;
    }
    auto future = appliedWorkspace.draft;
    future.lines[0].first.x += 0.125;
    future.lines[0].second.x += 0.125;
    appliedWorkspace.history = GlView::RecoveryState::CleaveHistory{
        { appliedWorkspace.draft, future }, 0 };
    check(view->restoreStagedCleaveWorkspace(
              appliedFamily,
              GlView::RecoveryState::CleaveWorkspace(appliedWorkspace)),
        "family-only Apply fixture retains a local Redo branch for the applied family draft");
    appliedWorkspace = view->captureStagedCleaveWorkspace(
        appliedFamily);

    auto peer = appliedWorkspace;
    peer.draft.context.surfaceMode = !surfaceApply;
    peer.draft.context.targetSectors = surfaceApply ? std::vector<std::int32_t>{ 1 }
        : std::vector<std::int32_t>{};
    peer.draft.context.sourceSurface = surfaceApply ? SurfaceRef{ -1, -1 }
        : SurfaceRef{ 1, 0 };
    peer.draft.context.targetSurfaces = surfaceApply ? std::vector<SurfaceRef>{}
        : std::vector<SurfaceRef>{ { 1, 0 } };
    peer.draft.lines[0].first = { 3.25, 0.0, 0.0 };
    peer.draft.lines[0].second = { 3.25, 1.0, 0.0 };
    peer.draft.lines[0].planeNormal = { 1.0, 0.0, 0.0 };
    peer.history = GlView::RecoveryState::CleaveHistory{ { peer.draft }, 0 };
    peer.bounded = true;
    peer.chainEnabled = true;
    peer.measurementsVisible = false;
    peer.anglesVisible = false;
    peer.lineNumbersVisible = true;
    peer.angleStepDegrees = 15.0;
    peer.shapeOptions.width = 0.375;
    peer.shapeOptions.height = 0.625;
    peer.shapeOptions.dimensionsLinked = false;
    peer.shapeOptions.rotationDegrees = 37.0;
    peer.transformMode = GlView::StagedCleaveTransformMode::Rotate;
    check(view->restoreStagedCleaveWorkspace(
              peerFamily,
              GlView::RecoveryState::CleaveWorkspace(peer)),
        "family-only Apply fixture retains a distinct compatible peer workspace");

    QAction* const undo = window.findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* const redo = window.findChild<QAction*>(QStringLiteral("edit.redo"));
    QToolButton* const apply = window.findChild<QToolButton*>(
        QStringLiteral("cleaveApplyButton"));
    check(undo && redo && apply && apply->isEnabled(),
        "family-only Apply fixture exposes the actual enabled Apply and history actions");
    if ( !undo || !redo || !apply || !apply->isEnabled() )
    {
        return;
    }
    const auto beforeLevel = assets::saveToSedBytes(window.documentStateForTest().level);
    window.markCurrentDocumentAsSavedForTest();
    apply->click();
    const auto afterLevel = assets::saveToSedBytes(window.documentStateForTest().level);
    check(window.undoDepthForTest() == 1 && afterLevel != beforeLevel
            && view->captureStagedCleaveWorkspace(
                appliedFamily).draft.lines.empty()
            && sameWorkspace(view->captureStagedCleaveWorkspace(
                peerFamily), peer),
        "actual Apply clears only its family and preserves the exact compatible peer");
    if ( window.undoDepthForTest() != 1 || afterLevel == beforeLevel )
    {
        return;
    }

    peer.draft.lines[0].first.x += 0.125;
    peer.draft.lines[0].second.x += 0.125;
    peer.history->snapshots.push_back(peer.draft);
    peer.history->cursor = 1;
    peer.shapeOptions.rotationDegrees = 73.0;
    check(view->restoreStagedCleaveWorkspace(
              peerFamily,
              GlView::RecoveryState::CleaveWorkspace(peer)),
        "the peer can be authored after Apply without changing the Level history");
    view->setFocus(Qt::OtherFocusReason);
    window.updateEditActionsForTest();
    undo->trigger();
    check(window.undoDepthForTest() == 0
            && assets::saveToSedBytes(window.documentStateForTest().level) == beforeLevel
            && window.editModeForTest() == (surfaceApply
                ? window.surfaceEditModeValueForTest() : window.sectorEditModeValueForTest())
            && (surfaceApply
                ? window.documentStateForTest().selection.surfaces
                        .size() == 1u
                    && window.documentStateForTest().selection.surfaces.contains({ 0, 0 })
                : window.documentStateForTest().selection.sectors
                    == std::unordered_set<std::int32_t>{ 0 })
            && sameWorkspace(view->captureStagedCleaveWorkspace(
                appliedFamily), appliedWorkspace)
            && sameWorkspace(view->captureStagedCleaveWorkspace(
                peerFamily), peer),
        "Apply Undo restores only the applied family and selection while preserving later peer edits");

    peer.draft.lines[0].first.x += 0.125;
    peer.draft.lines[0].second.x += 0.125;
    peer.history->snapshots.push_back(peer.draft);
    peer.history->cursor = 2;
    peer.shapeOptions.rotationDegrees = 109.0;
    check(view->restoreStagedCleaveWorkspace(
              peerFamily,
              GlView::RecoveryState::CleaveWorkspace(peer)),
        "the peer can be authored on the Undo side of the Apply transition");
    QAction* peerCleaveAction = nullptr;
    if ( peerRedoRoute != ApplyPeerRedoRoute::Dormant )
    {
        if ( peerRedoRoute == ApplyPeerRedoRoute::Local || peerRedoRoute == ApplyPeerRedoRoute::History )
        {
            peer.history->cursor = 1;
            peer.draft = peer.history->snapshots[1];
            check(view->restoreStagedCleaveWorkspace(peerFamily,
                      GlView::RecoveryState::CleaveWorkspace(peer)),
                "hot peer route fixture retains an available local Redo branch");
        }
        if ( surfaceApply )
        {
            window.activateSectorEditModeForTest();
        }
        else
        {
            window.activateSurfaceEditModeForTest();
            window.activateSurfaceFaceSubModeForTest();
        }
        auto& selection = window.mutableDocumentStateForTest().selection;
        selection.clear();
        if ( surfaceApply )
        {
            selection.sectors.insert(1);
        }
        else
        {
            selection.surfaces.insert({ 1, 0 });
        }
        view->setSelectionState(1, surfaceApply ? -1 : 0, -1, -1, selection);
        window.updateEditActionsForTest();
        peerCleaveAction = window.findChild<QAction*>(surfaceApply
            ? QStringLiteral("viewport.sector.cleave")
            : QStringLiteral("viewport.surface.cleave"));
        check(peerCleaveAction && peerCleaveAction->isEnabled(),
            "hot peer route fixture exposes the actual enabled peer Cleave action");
        if ( !peerCleaveAction )
        {
            return;
        }
        peerCleaveAction->setChecked(true);
        QApplication::setActiveWindow(&window);
        view->setFocus(Qt::OtherFocusReason);
        QApplication::processEvents(QEventLoop::AllEvents);
        check(view->cleaveWorkspaceFamily() == peerFamily
                && view->stagedCleaveHistoryOwnsUndo()
                && view->canRedoStagedCleaveHistory() == (peerRedoRoute != ApplyPeerRedoRoute::Document),
            "hot peer route owns Cleave input and exposes its exact local Redo availability");
    }
    window.updateEditActionsForTest();
    check(redo->isEnabled(),
        "family-aware routing keeps the requested local or document Redo enabled");
    if ( peerRedoRoute == ApplyPeerRedoRoute::History )
    {
        window.jumpToUndoDepthForTest(1);
    }
    else
    {
        redo->trigger();
    }
    if ( peerRedoRoute == ApplyPeerRedoRoute::Local )
    {
        peer.draft = peer.history->snapshots[2];
        peer.history->cursor = 2;
        check(window.undoDepthForTest() == 0 && window.redoDepthForTest() == 1
                && assets::saveToSedBytes(window.documentStateForTest().level) == beforeLevel
                && sameWorkspace(view->captureStagedCleaveWorkspace(appliedFamily),
                    appliedWorkspace)
                && sameWorkspace(view->captureStagedCleaveWorkspace(peerFamily), peer),
            "a hot peer's available local Redo wins without applying geometry or changing the applied family's exact draft");
        return;
    }
    check(window.undoDepthForTest() == 1
            && assets::saveToSedBytes(window.documentStateForTest().level) == afterLevel
            && view->captureStagedCleaveWorkspace(
                appliedFamily).draft.lines.empty()
            && sameWorkspace(view->captureStagedCleaveWorkspace(peerFamily), peer)
            && (peerRedoRoute == ApplyPeerRedoRoute::Dormant || (peerCleaveAction->isChecked()
                && (surfaceApply ? view->sectorCleaveToolEnabled()
                    : view->surfaceCleaveToolEnabled()))),
        "document Apply Redo clears only its applied family and preserves the peer's exact work and active tool");
}

void runImportedCleaveHistoryCursorTest(const std::filesystem::path& root)
{
    MainWindow source{};
    prepareCleaveWindow(source);
    GlView* const sourceView = source.glViewForTest();
    check(sourceView != nullptr,
        "workspace-history Cleave fixture owns a viewport");
    if (!sourceView)
    {
        return;
    }

    sourceView->addStagedCleaveLineForTest(
        QVector3D(0.5f, -1.0f, 0.5f), QVector3D(0.5f, 2.0f, 0.5f),
        QVector3D(1.0f, 0.0f, 0.0f));
    const GlView::RecoveryState::CleaveDraftSnapshot baseline =
        sourceView->captureStagedCleaveDraftState();
    sourceView->addStagedCleaveLineForTest(
        QVector3D(-1.0f, 0.5f, 0.5f), QVector3D(2.0f, 0.5f, 0.5f),
        QVector3D(0.0f, 1.0f, 0.0f));
    GlView::RecoveryState::CleaveDraftSnapshot current =
        sourceView->captureStagedCleaveDraftState();
    current.applyOrder = { 1, 0 };
    current.selectedCleaveLines = { 0, 1 };
    current.selectedCleaveLine = 1;
    current.chainOrigin = GlView::RecoveryState::CleaveChainOrigin{ 1, 0 };
    GlView::RecoveryState::CleaveDraftSnapshot future = current;
    future.lines[1].first.y += 0.125;
    future.lines[1].second.y += 0.125;
    GlView::RecoveryState::CleaveHistory preparedHistory{};
    preparedHistory.snapshots = { baseline, current, future };
    preparedHistory.cursor = 1;
    sourceView->restoreStagedCleaveDraftState(current, preparedHistory);
    auto sourceShape = sourceView->cleaveShapeOptions();
    sourceShape.cameraPlane = true;
    sourceView->setCleaveShapeOptions(sourceShape);

    const GlView::RecoveryState::CleaveDraftSnapshot expectedDraft =
        sourceView->captureStagedCleaveDraftState();
    const auto expectedHistory = sourceView->captureStagedCleaveHistory();
    check(expectedHistory.has_value(),
        "Apply workspace memento contains a retained local Redo branch");
    if (!expectedHistory)
    {
        return;
    }

    QAction* const applyUndo = source.findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* const applyRedo = source.findChild<QAction*>(QStringLiteral("edit.redo"));
    QToolButton* const applyButton = source.findChild<QToolButton*>(
        QStringLiteral("cleaveApplyButton"));
    check(applyUndo && applyRedo && applyButton && applyButton->isEnabled(),
        "valid two-cut Apply exposes the document Undo/Redo and Apply controls");
    if (!applyUndo || !applyRedo || !applyButton || !applyButton->isEnabled())
    {
        return;
    }

    const std::size_t sectorsBefore =
        source.documentStateForTest().level.sectors.size();
    source.markCurrentDocumentAsSavedForTest();
    applyButton->click();
    const std::size_t sectorsAfterApply =
        source.documentStateForTest().level.sectors.size();
    check(sourceView->stagedCleaveLineCount() == 0
            && sectorsAfterApply > sectorsBefore
            && source.undoDepthForTest() == 1,
        "actual Cleave Apply commits geometry and clears the staged workspace");
    if (sourceView->stagedCleaveLineCount() != 0
        || sectorsAfterApply <= sectorsBefore
        || source.undoDepthForTest() != 1)
    {
        return;
    }

    check(source.applyStructuralHistoryEditForTest(
            [](editor::DocumentState& state)
            {
                if (state.level.sectors.empty())
                {
                    return false;
                }
                state.level.sectors.front().flags ^= 0x20U;
                return true;
            })
            && source.undoDepthForTest() == 2,
        "a second exact HIST transition positions the Apply memento below the cursor");
    if (source.undoDepthForTest() != 2)
    {
        return;
    }

    const std::filesystem::path nativePath = root / "cleave-history-cursor.sed";
    source.setCurrentPathForTitleForTest(nativePath);
    const int saveErrors = saveWithoutDialog(
        source.findChild<QAction*>(QStringLiteral("file.save")));
    const auto native = loadNativeFile(nativePath);
    const QByteArray edit = native ? nativeEditorPayload(*native) : QByteArray{};
    const QCborMap cleaveEntry = sidecarEntry(edit, "cleave");
    const QCborMap encodedRoot = QCborValue::fromCbor(edit).toMap();
    const QCborMap workspaceHistory = encodedRoot
        .value(QStringLiteral("workspaceHistory")).toMap();
    check(saveErrors == 0 && native && !edit.isEmpty()
            && workspaceHistory.value(QStringLiteral("undoCount")).toInteger() == 2
            && cleaveEntry.value(QStringLiteral("beforeSha256")).toByteArray().size() == 32
            && cleaveEntry.value(QStringLiteral("afterSha256")).toByteArray().size() == 32,
        "native Save binds the exact Cleave workspace to its adjacent HIST transition hashes");
    if (!native || edit.isEmpty() || cleaveEntry.isEmpty())
    {
        return;
    }
    check(cleaveEntry.value(QStringLiteral("cleave")).toMap()
            .value(QStringLiteral("workspace")).toMap()
            .value(QStringLiteral("shape")).toMap()
            .value(QStringLiteral("cameraPlane")).toBool(),
        "native Save retains Camera-plane in the checked pre-Apply workspace attachment");

    MainWindow reopened{};
    reopened.setAttribute(Qt::WA_DontShowOnScreen);
    reopened.openLevelFromFilenameForTest(
        QString::fromStdWString(nativePath.wstring()));
    GlView* const reopenedView = reopened.glViewForTest();
    check(reopenedView && reopened.undoDepthForTest() == 2,
        "native HIST import restores the complete two-transition timeline");
    if (!reopenedView || reopened.undoDepthForTest() != 2)
    {
        return;
    }

    const bool movedToInitialHistoryRow = reopened.emitHistoryClickForTest(0);
    check(movedToInitialHistoryRow,
        "History dock click reaches the initial imported HIST row");
    check(reopened.undoDepthForTest() == 0,
        "History dock initial-row click moves the document cursor to depth zero");
    check(sameDraft(currentDraft(reopenedView->recoveryState()), expectedDraft),
        "History dock jump restores the exact imported Apply draft");
    check(reopenedView->cleaveShapeOptions().cameraPlane,
        "History dock Undo restores Camera-plane from the native Apply memento");
    check(reopenedView->recoveryState().cleaveHistory.has_value(),
        "History dock jump retains the imported local Cleave timeline");
    if (reopenedView->recoveryState().cleaveHistory)
    {
        check(sameHistory(*reopenedView->recoveryState().cleaveHistory,
                *expectedHistory),
            "History dock jump restores the exact local Redo branch and cursor");
    }

    check(reopened.emitHistoryClickForTest(1)
            && reopened.undoDepthForTest() == 1
            && reopenedView->stagedCleaveLineCount() == 0,
        "History dock Redo across the paired Apply clears the restored draft");

    QAction* const reopenedUndo = reopened.findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* const reopenedRedo = reopened.findChild<QAction*>(QStringLiteral("edit.redo"));
    check(reopenedUndo && reopenedRedo && reopenedUndo->isEnabled(),
        "imported Apply transition remains available through native Undo/Redo actions");
    if (reopenedUndo && reopenedRedo && reopenedUndo->isEnabled())
    {
        reopenedUndo->trigger();
        check(sameDraft(currentDraft(reopenedView->recoveryState()), expectedDraft)
                && reopenedView->recoveryState().cleaveHistory
                && sameHistory(*reopenedView->recoveryState().cleaveHistory,
                    *expectedHistory),
            "native Undo after HIST import restores exact workspace memento arrays");
        if (reopenedRedo->isEnabled())
        {
            reopenedRedo->trigger();
        }
        check(reopenedRedo->isEnabled()
                && reopenedView->stagedCleaveLineCount() == 0
                && reopened.documentStateForTest().level.sectors.size()
                    == sectorsAfterApply,
            "paired native Redo outranks the restored workspace's local Redo branch");
    }
}

void runImportedLayerHistoryTest(const std::filesystem::path& root)
{
    const LayerColorPreferenceRestore restoreLayerColors{};
    MainWindow source{};
    source.setAttribute(Qt::WA_DontShowOnScreen);
    source.setDocumentStateForTest(makeLayerDocument());
    source.resetLayerColorsToDefaultsForTest();
    source.setLayerVisualStateForTest(
        { true, false, true }, { false, true, false });

    const std::vector<bool> expectedBeforeVisible{ true, false, true };
    const std::vector<bool> expectedBeforeLocked{ false, true, false };
    std::vector<QColor> expectedBeforeColors{};
    for (std::size_t index = 0; index < 3; ++index)
    {
        expectedBeforeColors.push_back(source.layerColorForTest(index));
    }

    QAction* const managerAction = source.findChild<QAction*>(QStringLiteral("edit.layers"));
    bool managerFixtureReady = false;
    QTimer::singleShot(0, &source, [&source, &managerFixtureReady]()
    {
        auto* const dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        auto* const table = dialog
            ? dialog->findChild<QTreeWidget*>("layer_editor_table") : nullptr;
        auto* const buttonBox = dialog
            ? dialog->findChild<QDialogButtonBox*>() : nullptr;
        if (!table || !buttonBox || table->topLevelItemCount() != 3)
        {
            if (dialog)
            {
                dialog->reject();
            }
            return;
        }

        QTreeWidgetItem* const geometry = table->topLevelItem(0);
        QTreeWidgetItem* const gameplay = table->topLevelItem(1);
        QTreeWidgetItem* const lighting = table->topLevelItem(2);
        geometry->setText(0, QStringLiteral("Geometry renamed"));
        geometry->setCheckState(1, Qt::Unchecked);
        geometry->setCheckState(2, Qt::Checked);
        geometry->setData(3, Qt::UserRole, QColor(190, 35, 45));
        gameplay->setCheckState(1, Qt::Checked);
        gameplay->setCheckState(2, Qt::Unchecked);
        gameplay->setData(3, Qt::UserRole, QColor(25, 175, 80));
        lighting->setCheckState(1, Qt::Checked);
        lighting->setCheckState(2, Qt::Checked);
        lighting->setData(3, Qt::UserRole, QColor(35, 75, 205));
        managerFixtureReady = true;
        buttonBox->button(QDialogButtonBox::Ok)->click();
    });
    if (managerAction)
    {
        managerAction->trigger();
    }

    const std::vector<bool> expectedAfterVisible{ false, true, true };
    const std::vector<bool> expectedAfterLocked{ true, false, true };
    const std::vector<QColor> expectedAfterColors{
        QColor(190, 35, 45), QColor(25, 175, 80), QColor(35, 75, 205) };
    const Level& changedLevel = source.documentStateForTest().level;
    check(managerFixtureReady && managerAction && source.undoDepthForTest() == 1
            && changedLevel.layers.size() == 3
            && changedLevel.layers[0] == "Geometry renamed"
            && source.layerVisualStateSizeForTest() == 3,
        "Layer Manager authors a real layout transition with historical row state");
    if (!managerFixtureReady || source.undoDepthForTest() != 1)
    {
        return;
    }

    const std::filesystem::path nativePath = root / "layer-history.sed";
    source.setCurrentPathForTitleForTest(nativePath);
    const QByteArray capturedEdit = source.captureRecoveryEditorStateForTest();
    const QCborMap capturedLayerEntry = sidecarEntry(capturedEdit, "layerVisuals");
    check(!capturedLayerEntry.isEmpty(),
        "Layer Manager transition is present in captured EDIT before native Save");
    const int saveErrors = saveWithoutDialog(
        source.findChild<QAction*>(QStringLiteral("file.save")));
    const auto native = loadNativeFile(nativePath);
    const QByteArray edit = native ? nativeEditorPayload(*native) : QByteArray{};
    const QCborMap layerEntry = sidecarEntry(edit, "layerVisuals");
    const QCborMap layerVisuals = layerEntry.value(
        QStringLiteral("layerVisuals")).toMap();
    check(saveErrors == 0 && native && !edit.isEmpty()
            && !layerEntry.isEmpty()
            && layerVisuals.value(QStringLiteral("before")).isMap()
            && layerVisuals.value(QStringLiteral("after")).isMap(),
        "native Save includes the historical Layer row payload beside its exact HIST binding");
    if (!native || edit.isEmpty())
    {
        return;
    }

    MainWindow reopened{};
    reopened.setAttribute(Qt::WA_DontShowOnScreen);
    reopened.openLevelFromFilenameForTest(
        QString::fromStdWString(nativePath.wstring()));
    check(reopened.undoDepthForTest() == 1
            && reopened.layerVisualStateSizeForTest() == 3,
        "native Layer history fixture imports its Level record and current row arrays");
    QAction* const undo = reopened.findChild<QAction*>(QStringLiteral("edit.undo"));
    QAction* const redo = reopened.findChild<QAction*>(QStringLiteral("edit.redo"));
    if (undo && undo->isEnabled())
    {
        undo->trigger();
    }
    bool beforeRestored = undo && reopened.undoDepthForTest() == 0
        && reopened.documentStateForTest().level.layers
            == std::vector<std::string>({ "Geometry", "Gameplay", "Lighting" })
        && reopened.layerVisualStateSizeForTest() == 3;
    for (std::size_t index = 0; beforeRestored && index < 3; ++index)
    {
        beforeRestored = reopened.layerVisibleForTest(index)
                == expectedBeforeVisible[index]
            && reopened.layerLockedForTest(index) == expectedBeforeLocked[index]
            && reopened.layerColorForTest(index) == expectedBeforeColors[index];
    }
    check(beforeRestored,
        "native Undo restores historical Layer visibility, lock, color, and original row names");

    if (redo && redo->isEnabled())
    {
        redo->trigger();
    }
    bool afterRestored = redo && reopened.undoDepthForTest() == 1
        && reopened.documentStateForTest().level.layers.size() == 3
        && reopened.documentStateForTest().level.layers[0] == "Geometry renamed"
        && reopened.layerVisualStateSizeForTest() == 3;
    for (std::size_t index = 0; afterRestored && index < 3; ++index)
    {
        afterRestored = reopened.layerVisibleForTest(index)
                == expectedAfterVisible[index]
            && reopened.layerLockedForTest(index) == expectedAfterLocked[index]
            && reopened.layerColorForTest(index) == expectedAfterColors[index];
    }
    check(afterRestored,
        "native Redo restores the exact post-edit Layer row identities and presentation");
}

void runWorkspaceHistoryBindingDegradationTest(
    const std::filesystem::path& root)
{
    MainWindow source{};
    prepareCleaveWindow(source);
    GlView* const view = source.glViewForTest();
    QToolButton* const applyButton = source.findChild<QToolButton*>(
        QStringLiteral("cleaveApplyButton"));
    QAction* const undo = source.findChild<QAction*>(QStringLiteral("edit.undo"));
    check(view && applyButton && undo,
        "binding-degradation fixture exposes an authored Apply and Undo path");
    if (!view || !applyButton || !undo)
    {
        return;
    }

    view->addStagedCleaveLineForTest(
        QVector3D(0.5f, -1.0f, 0.5f), QVector3D(0.5f, 2.0f, 0.5f),
        QVector3D(1.0f, 0.0f, 0.0f));
    const GlView::RecoveryState::CleaveDraftSnapshot baseline =
        view->captureStagedCleaveDraftState();
    view->addStagedCleaveLineForTest(
        QVector3D(-1.0f, 0.5f, 0.5f), QVector3D(2.0f, 0.5f, 0.5f),
        QVector3D(0.0f, 1.0f, 0.0f));
    const GlView::RecoveryState::CleaveDraftSnapshot stagedDraft =
        view->captureStagedCleaveDraftState();
    GlView::RecoveryState::CleaveDraftSnapshot futureDraft = stagedDraft;
    futureDraft.lines[1].first.y += 0.125;
    futureDraft.lines[1].second.y += 0.125;
    GlView::RecoveryState::CleaveHistory fixtureHistory{};
    fixtureHistory.snapshots = { baseline, stagedDraft, futureDraft };
    fixtureHistory.cursor = 1u;
    view->restoreStagedCleaveDraftState(stagedDraft, fixtureHistory);
    source.updateEditActionsForTest();
    QApplication::processEvents(QEventLoop::AllEvents);
    const auto expectedDraft = view->captureStagedCleaveDraftState();
    const auto expectedHistory = view->captureStagedCleaveHistory();
    const std::size_t sectorsBefore =
        source.documentStateForTest().level.sectors.size();
    QAction* const cleaveAction = source.findChild<QAction*>(
        QStringLiteral("viewport.sector.cleave"));
    check(view->stagedCleaveLineCount() == 2
            && expectedDraft.context.valid
            && expectedDraft.context.targetSectors
                == std::vector<std::int32_t>{ 0 }
            && source.documentStateForTest().selection.sectors
                == std::unordered_set<std::int32_t>{ 0 }
            && cleaveAction && cleaveAction->isChecked()
            && view->sectorCleaveToolEnabled()
            && expectedHistory.has_value() && applyButton->isEnabled(),
        "binding-degradation fixture starts with an applicable Cleave draft");
    if (view->stagedCleaveLineCount() != 2
        || !expectedDraft.context.valid
        || expectedDraft.context.targetSectors
            != std::vector<std::int32_t>{ 0 }
        || !cleaveAction || !cleaveAction->isChecked()
        || !view->sectorCleaveToolEnabled()
        || !expectedHistory || !applyButton->isEnabled())
    {
        return;
    }

    applyButton->click();
    if (source.documentStateForTest().level.sectors.size() <= sectorsBefore
        || view->stagedCleaveLineCount() != 0 || !undo->isEnabled())
    {
        check(false,
            "binding-degradation fixture commits its real Apply before corrupting HIST bindings");
        return;
    }
    undo->trigger();
    check(sameDraft(currentDraft(view->recoveryState()), expectedDraft)
            && view->recoveryState().cleaveHistory
            && sameHistory(*view->recoveryState().cleaveHistory,
                *expectedHistory),
        "binding-degradation fixture is on the exact Cleave-bearing Apply Undo side");

    const std::filesystem::path validPath = root / "workspace-binding-valid.sed";
    source.setCurrentPathForTitleForTest(validPath);
    const int saveErrors = saveWithoutDialog(
        source.findChild<QAction*>(QStringLiteral("file.save")));
    const auto native = loadNativeFile(validPath);
    const QByteArray originalEdit = native
        ? nativeEditorPayload(*native) : QByteArray{};
    const QCborMap originalBinding = sidecarEntry(originalEdit, "cleave");
    const QCborMap originalWorkspaceHistory = editorRoot(originalEdit)
        .value(QStringLiteral("workspaceHistory")).toMap();
    const QCborMap originalPendingRedo = originalWorkspaceHistory
        .value(QStringLiteral("pendingRedo")).toMap();
    check(saveErrors == 0 && native && !originalEdit.isEmpty()
            && !originalBinding.isEmpty()
            && originalBinding.value(QStringLiteral("beforeSha256"))
                .toByteArray().size() == 32
            && originalWorkspaceHistory.value(QStringLiteral("redoCount"))
                .toInteger(-1) == 1
            && originalPendingRedo.value(QStringLiteral("stack"))
                .toInteger(-1) == 1
            && originalPendingRedo.value(QStringLiteral("index"))
                .toInteger(-1) == 0,
        "binding-degradation fixture writes a valid current draft and Apply attachment");
    if (saveErrors != 0 || !native || originalEdit.isEmpty()
        || originalBinding.isEmpty())
    {
        return;
    }

    const QByteArray validHistoryPayload = ui::project::findSedOptionalSection(
        native->optionalSections, ui::project::SedHistorySection);
    QByteArray unvalidatedEditorState{};
    QString unvalidatedHistoryError{};
    QString unvalidatedWorkspaceWarning{};
    ui::recovery::WorkspaceHistoryState unvalidatedWorkspaceHistory{};
    QByteArray validatedBaselineEditorState{};
    QString validatedBaselineHistoryError{};
    QString validatedBaselineWorkspaceWarning{};
    ui::recovery::WorkspaceHistoryState validatedBaselineWorkspaceHistory{};
    const auto unvalidatedHistory =
        ui::recovery::deserializeEditorDocumentHistoryForRestoreWithWorkspaceBindings(
            validHistoryPayload,
            native->level.kind,
            native->level,
            originalEdit,
            &unvalidatedEditorState,
            &unvalidatedHistoryError,
            &unvalidatedWorkspaceWarning,
            &unvalidatedWorkspaceHistory,
            nullptr);
    const auto validatedBaselineHistory =
        ui::recovery::deserializeEditorDocumentHistoryForRestoreWithWorkspaceBindings(
            validHistoryPayload,
            native->level.kind,
            native->level,
            originalEdit,
            &validatedBaselineEditorState,
            &validatedBaselineHistoryError,
            &validatedBaselineWorkspaceWarning,
            &validatedBaselineWorkspaceHistory,
            &ui::recovery::validateWorkspaceHistoryAttachmentForLevels);
    const QByteArray unvalidatedHistoryRoundTrip = unvalidatedHistory
        ? ui::recovery::serializeEditorDocumentHistory(
            native->level, *unvalidatedHistory)
        : QByteArray{};
    const QByteArray validatedBaselineHistoryRoundTrip = validatedBaselineHistory
        ? ui::recovery::serializeEditorDocumentHistory(
            native->level, *validatedBaselineHistory)
        : QByteArray{};
    const QCborMap unvalidatedEditorRoot = editorRoot(unvalidatedEditorState);
    check(!validHistoryPayload.isEmpty() && unvalidatedHistory
            && validatedBaselineHistory
            && unvalidatedHistory->undoEntries.size()
                == validatedBaselineHistory->undoEntries.size()
            && unvalidatedHistory->redoEntries.size()
                == validatedBaselineHistory->redoEntries.size()
            && !unvalidatedHistoryRoundTrip.isEmpty()
            && unvalidatedHistoryRoundTrip == validatedBaselineHistoryRoundTrip,
        "a reader without the app Level validator still imports the independent HIST timeline");
    check(unvalidatedWorkspaceHistory.attachments.empty()
            && unvalidatedWorkspaceWarning.contains(
                QStringLiteral("validator was unavailable"))
            && !unvalidatedEditorRoot.contains(QStringLiteral("workspaceHistory")),
        "a reader without the app Level validator drops historical attachments with an explicit warning");
    check(!unvalidatedEditorState.isEmpty()
            && unvalidatedEditorRoot.value(QStringLiteral("viewport"))
                == editorRoot(originalEdit).value(QStringLiteral("viewport")),
        "dropping unvalidated historical attachments preserves the exact current EDIT draft and local timeline");

    MainWindow validReopened{};
    validReopened.setAttribute(Qt::WA_DontShowOnScreen);
    validReopened.openLevelFromFilenameForTest(
        QString::fromStdWString(validPath.wstring()));
    QAction* const validRedo = validReopened.findChild<QAction*>(
        QStringLiteral("edit.redo"));
    GlView* const validReopenedView = validReopened.glViewForTest();
    check(validReopenedView && validRedo && validRedo->isEnabled()
            && sameDraft(currentDraft(validReopenedView->recoveryState()),
                expectedDraft)
            && validReopenedView->recoveryState().cleaveHistory
            && sameHistory(*validReopenedView->recoveryState().cleaveHistory,
                *expectedHistory),
        "valid position-zero pending attachment imports beside the exact current Cleave timeline");
    if (validReopenedView && validRedo && validRedo->isEnabled())
    {
        validRedo->trigger();
    }
    check(validReopened.undoDepthForTest() == 1
            && validReopened.redoDepthForTest() == 0
            && validReopenedView
            && validReopenedView->stagedCleaveLineCount() == 0,
        "matching position-zero pending attachment gives native document Redo priority");

    const auto verifyCurrentDraftPreserved = [&](const std::filesystem::path& path,
                                                  const bool expectDocumentRedo,
                                                  const char* description)
    {
        MainWindow reopened{};
        reopened.setAttribute(Qt::WA_DontShowOnScreen);
        reopened.openLevelFromFilenameForTest(
            QString::fromStdWString(path.wstring()));
        GlView* const reopenedView = reopened.glViewForTest();
        const GlView::RecoveryState restored = reopenedView
            ? reopenedView->recoveryState() : GlView::RecoveryState{};
        const bool cursorExpected = expectDocumentRedo
            ? reopened.undoDepthForTest() == 0
                && reopened.redoDepthForTest() == 1
            : reopened.undoDepthForTest() == 0
                && reopened.redoDepthForTest() == 0;
        const bool exactDraft = reopenedView
            && sameDraft(currentDraft(restored), expectedDraft)
            && restored.cleaveHistory
            && sameHistory(*restored.cleaveHistory, *expectedHistory);
        const QCborMap recapturedRoot = editorRoot(
            reopened.captureRecoveryEditorStateForTest());
        check(cursorExpected && exactDraft
                && !recapturedRoot.contains(QStringLiteral("workspaceHistory")),
            description);
    };

    const auto verifyStalePendingRedoDoesNotStealLocalRedo = [](
            const std::filesystem::path& path,
            const GlView::RecoveryState::CleaveDraftSnapshot& expectedCurrentDraft,
            const GlView::RecoveryState::CleaveHistory& expectedCurrentHistory,
            const std::optional<bool> expectedBounded,
            const char* description)
    {
        MainWindow reopened{};
        reopened.setAttribute(Qt::WA_DontShowOnScreen);
        reopened.openLevelFromFilenameForTest(
            QString::fromStdWString(path.wstring()));
        GlView* const reopenedView = reopened.glViewForTest();
        QAction* const redo = reopened.findChild<QAction*>(
            QStringLiteral("edit.redo"));
        if (!reopenedView || !redo)
        {
            check(false, description);
            return;
        }

        const GlView::RecoveryState before = reopenedView->recoveryState();
        const QByteArray recaptured = reopened.captureRecoveryEditorStateForTest();
        const bool currentDraftAndHistoryExact =
            sameDraft(currentDraft(before), expectedCurrentDraft)
            && before.cleaveHistory
            && sameHistory(*before.cleaveHistory, expectedCurrentHistory);
        const bool boundedExact = !expectedBounded
            || (before.cleaveBounded
                && *before.cleaveBounded == *expectedBounded);
        const bool validAttachmentRetained =
            !sidecarEntry(recaptured, "cleave").isEmpty();
        check(currentDraftAndHistoryExact,
            "stale pending-Redo fixture restores its exact current draft and local timeline");
        check(boundedExact,
            "stale pending-Redo fixture restores the expected current Bounded value");
        check(validAttachmentRetained,
            "stale pending-Redo fixture retains the independently valid Apply attachment");
        const std::size_t cursorBefore = before.cleaveHistory
            ? before.cleaveHistory->cursor : 0u;
        const std::uint64_t revisionBefore =
            reopenedView->stagedCleaveAuthoredRevision();
        const bool localRedoAvailable = reopenedView->canRedoStagedCleaveHistory();
        const bool redoActionEnabled = redo->isEnabled();
        check(localRedoAvailable,
            "stale pending-Redo fixture exposes the restored local Redo branch");
        check(redoActionEnabled,
            "stale pending-Redo fixture keeps the shared Redo action enabled");
        if (localRedoAvailable && redoActionEnabled)
        {
            redo->trigger();
        }
        const GlView::RecoveryState after = reopenedView->recoveryState();
        check(localRedoAvailable && redoActionEnabled
                && reopened.undoDepthForTest() == 0
                && reopened.redoDepthForTest() == 1,
            "stale pending-Redo hint leaves the document cursor unchanged when Redo is triggered");
        check(after.cleaveHistory
                && after.cleaveHistory->cursor == cursorBefore + 1u
                && reopenedView->stagedCleaveAuthoredRevision() != revisionBefore,
            "stale pending-Redo hint routes the action through local Cleave Redo");
        check(currentDraftAndHistoryExact && boundedExact
                && validAttachmentRetained
                && localRedoAvailable && redoActionEnabled,
            description);
    };

    const QByteArray corruptHashEdit = mutateWorkspaceHistoryBinding(
        originalEdit, true, false);
    const std::filesystem::path corruptHashPath =
        root / "workspace-binding-corrupt-hash.sed";
    check(!corruptHashEdit.isEmpty()
            && rewriteNativeEditorPayload(
                *native, corruptHashPath, corruptHashEdit, false),
        "test rewrites a 32-byte adjacent-Level hash under valid outer SED protection");
    if (!corruptHashEdit.isEmpty())
    {
        verifyCurrentDraftPreserved(
            corruptHashPath, true,
            "wrong adjacent-Level hash drops only the Apply attachment and preserves current Cleave EDIT/HIST");
    }

    const QByteArray duplicateEdit = mutateWorkspaceHistoryBinding(
        originalEdit, false, true);
    const std::filesystem::path duplicatePath =
        root / "workspace-binding-duplicate.sed";
    check(!duplicateEdit.isEmpty()
            && rewriteNativeEditorPayload(
                *native, duplicatePath, duplicateEdit, false),
        "test creates duplicate positional bindings under valid outer SED protection");
    if (!duplicateEdit.isEmpty())
    {
        verifyCurrentDraftPreserved(
            duplicatePath, true,
            "duplicate positional binding drops only dependent attachments and preserves current Cleave EDIT/HIST");
    }

    const QByteArray wrongCountEdit = mutateWorkspaceHistoryBinding(
        originalEdit, false, false, true);
    const std::filesystem::path wrongCountPath =
        root / "workspace-binding-wrong-count.sed";
    check(!wrongCountEdit.isEmpty()
            && rewriteNativeEditorPayload(
                *native, wrongCountPath, wrongCountEdit, false),
        "test mismatches a workspace binding count under valid outer SED protection");
    if (!wrongCountEdit.isEmpty())
    {
        verifyCurrentDraftPreserved(
            wrongCountPath, true,
            "wrong persisted HIST count drops dependent attachments and preserves current Cleave EDIT/HIST");
    }

    const QByteArray pendingIndexOneEdit = mutatePendingRedoIndex(originalEdit);
    const std::filesystem::path pendingIndexOnePath =
        root / "workspace-binding-pending-index-one.sed";
    check(!pendingIndexOneEdit.isEmpty()
            && rewriteNativeEditorPayload(
                *native, pendingIndexOnePath, pendingIndexOneEdit, false),
        "test writes an out-of-range pending Redo index with valid outer protection");
    if (!pendingIndexOneEdit.isEmpty())
    {
        verifyCurrentDraftPreserved(
            pendingIndexOnePath, true,
            "invalid pending Redo index drops its hint/attachment without losing the current Cleave draft or local timeline");
    }

    const QByteArray invalidSentinelEdit =
        mutateApplyAttachmentSentinel(originalEdit);
    const std::filesystem::path invalidSentinelPath =
        root / "workspace-binding-invalid-active-sentinel.sed";
    check(!invalidSentinelEdit.isEmpty()
            && rewriteNativeEditorPayload(
                *native, invalidSentinelPath, invalidSentinelEdit, false),
        "test writes an incomplete historical active-Surface sentinel under valid outer protection");
    if (!invalidSentinelEdit.isEmpty())
    {
        verifyCurrentDraftPreserved(
            invalidSentinelPath, true,
            "invalid historical active-reference sentinel drops only its attachment and preserves current Cleave EDIT/HIST");
    }

    const QByteArray oversizedTimelineEdit =
        mutateApplyAttachmentTimelinePreflight(originalEdit);
    const std::filesystem::path oversizedTimelinePath =
        root / "workspace-binding-oversized-timeline.sed";
    check(!oversizedTimelineEdit.isEmpty()
            && rewriteNativeEditorPayload(
                *native, oversizedTimelinePath, oversizedTimelineEdit, false),
        "test writes a 65-snapshot historical timeline under valid outer protection");
    if (!oversizedTimelineEdit.isEmpty())
    {
        verifyCurrentDraftPreserved(
            oversizedTimelinePath, true,
            "count-derived historical timeline preflight drops an oversized attachment before allocation and preserves current Cleave EDIT/HIST");
    }

    const QByteArray changedOrderEdit = mutateCurrentCleaveForPendingMismatch(
        originalEdit, true, false, false);
    const std::filesystem::path changedOrderPath =
        root / "workspace-binding-pending-order-mismatch.sed";
    check(!changedOrderEdit.isEmpty()
            && rewriteNativeEditorPayload(
                *native, changedOrderPath, changedOrderEdit, false),
        "test changes current and cursor-snapshot Apply order while retaining a valid attached memento");
    if (!changedOrderEdit.isEmpty())
    {
        auto expectedOrderDraft = expectedDraft;
        expectedOrderDraft.applyOrder = { 1, 0 };
        auto expectedOrderHistory = *expectedHistory;
        expectedOrderHistory.snapshots[expectedOrderHistory.cursor].applyOrder = { 1, 0 };
        verifyStalePendingRedoDoesNotStealLocalRedo(
            changedOrderPath, expectedOrderDraft, expectedOrderHistory,
            std::nullopt,
            "valid attachment with mismatched current Apply order keeps the draft, declines paired priority, and leaves Redo to the local branch");
    }

    const QByteArray changedBoundedEdit = mutateCurrentCleaveForPendingMismatch(
        originalEdit, false, true, false);
    const std::filesystem::path changedBoundedPath =
        root / "workspace-binding-pending-bounded-mismatch.sed";
    check(!changedBoundedEdit.isEmpty()
            && rewriteNativeEditorPayload(
                *native, changedBoundedPath, changedBoundedEdit, false),
        "test changes only current Bounded while retaining a valid attached memento");
    if (!changedBoundedEdit.isEmpty())
    {
        verifyStalePendingRedoDoesNotStealLocalRedo(
            changedBoundedPath, expectedDraft, *expectedHistory,
            std::optional<bool>{true},
            "valid attachment with mismatched current Bounded keeps the draft, declines paired priority, and leaves Redo to the local branch");
    }

    const QByteArray changedLineEdit = mutateCurrentCleaveForPendingMismatch(
        originalEdit, false, false, true);
    const std::filesystem::path changedLinePath =
        root / "workspace-binding-pending-current-draft-mismatch.sed";
    check(!changedLineEdit.isEmpty()
            && rewriteNativeEditorPayload(
                *native, changedLinePath, changedLineEdit, false),
        "test changes current and cursor-snapshot authored line geometry while retaining a valid attached memento");
    if (!changedLineEdit.isEmpty())
    {
        auto expectedLineDraft = expectedDraft;
        expectedLineDraft.lines[0].first.x += 0.125;
        expectedLineDraft.lines[0].second.x += 0.125;
        auto expectedLineHistory = *expectedHistory;
        expectedLineHistory.snapshots[expectedLineHistory.cursor].lines[0]
            = expectedLineDraft.lines[0];
        verifyStalePendingRedoDoesNotStealLocalRedo(
            changedLinePath, expectedLineDraft, expectedLineHistory,
            std::nullopt,
            "valid attachment with a changed current Cleave line keeps that draft, declines paired priority, and leaves Redo to the local branch");
    }

    const std::filesystem::path missingHistoryPath =
        root / "workspace-binding-missing-hist.sed";
    check(rewriteNativeEditorPayload(
            *native, missingHistoryPath, originalEdit, true),
        "test removes only HIST while retaining the independently valid EDIT workspace");
    verifyCurrentDraftPreserved(
        missingHistoryPath, false,
        "missing HIST drops dependent Apply attachments while preserving the current Cleave draft and local branch");

    const auto exactApplied = view->captureStagedCleaveWorkspace(
        GlView::CleaveWorkspaceFamily::Sector);
    auto peer = exactApplied;
    peer.draft.context.surfaceMode = true;
    peer.draft.context.targetSectors.clear();
    peer.draft.context.sourceSurface = { 0, 0 };
    peer.draft.context.targetSurfaces = { { 0, 0 } };
    peer.draft.lines.resize(1u);
    peer.draft.lines[0].first = { 0.25, 0.0, 0.0 };
    peer.draft.lines[0].second = { 0.25, 1.0, 0.0 };
    peer.draft.lines[0].linkedToPrevious = false;
    peer.draft.lines[0].linkedToNext = false;
    peer.draft.applyOrder = { 0 };
    peer.draft.chainOrigin.reset();
    peer.draft.selectedCleaveLines = { 0 };
    peer.draft.selectedCleaveLine = 0;
    peer.history = GlView::RecoveryState::CleaveHistory{ { peer.draft }, 0 };
    peer.chainEnabled = false;
    peer.bounded = true;
    peer.shapeOptions.rotationDegrees = 41.0;
    check(view->restoreStagedCleaveWorkspace(
              GlView::CleaveWorkspaceFamily::Surface,
              GlView::RecoveryState::CleaveWorkspace(peer)),
        "historical peer rejection fixture retains an independently valid current Surface workspace");
    const QByteArray dualEdit = source.captureRecoveryEditorStateForTest();
    const QByteArray historicalPeerEdit = mutateApplyAttachmentPeerWorkspace(dualEdit);
    const std::filesystem::path historicalPeerPath =
        root / "workspace-binding-unowned-historical-peer.sed";
    check(!historicalPeerEdit.isEmpty()
            && rewriteNativeEditorPayload(
                *native, historicalPeerPath, historicalPeerEdit, false),
        "test inserts a valid Surface peer into an applied-family-only attachment under valid outer protection");
    if ( !historicalPeerEdit.isEmpty() )
    {
        QByteArray checkedEdit{};
        QString historyError{};
        QString workspaceWarning{};
        ui::recovery::WorkspaceHistoryState checkedWorkspaceHistory{};
        const auto checkedHistory =
            ui::recovery::deserializeEditorDocumentHistoryForRestoreWithWorkspaceBindings(
                validHistoryPayload, native->level.kind, native->level,
                historicalPeerEdit, &checkedEdit, &historyError, &workspaceWarning,
                &checkedWorkspaceHistory,
                &ui::recovery::validateWorkspaceHistoryAttachmentForLevels);
        check(checkedHistory && checkedWorkspaceHistory.attachments.empty()
                && !workspaceWarning.isEmpty()
                && !editorRoot(checkedEdit).contains(QStringLiteral("workspaceHistory"))
                && editorRoot(checkedEdit).value(QStringLiteral("viewport"))
                    == editorRoot(dualEdit).value(QStringLiteral("viewport")),
            "an unowned historical peer drops the optional attachment transaction while preserving exact dual current EDIT and independent HIST");
        MainWindow reopened{};
        reopened.setAttribute(Qt::WA_DontShowOnScreen);
        reopened.openLevelFromFilenameForTest(
            QString::fromStdWString(historicalPeerPath.wstring()));
        GlView* const reopenedView = reopened.glViewForTest();
        check(reopenedView && reopened.undoDepthForTest() == 0
                && reopened.redoDepthForTest() == 1
                && sameWorkspace(reopenedView->captureStagedCleaveWorkspace(
                    GlView::CleaveWorkspaceFamily::Sector), exactApplied)
                && sameWorkspace(reopenedView->captureStagedCleaveWorkspace(
                    GlView::CleaveWorkspaceFamily::Surface), peer)
                && !editorRoot(reopened.captureRecoveryEditorStateForTest())
                    .contains(QStringLiteral("workspaceHistory")),
            "native Open rejects an unowned historical peer without changing either family's current draft, options or local history");
    }
}

void runTransientPreviewCaptureNormalizationTest()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);

    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.header.version = 1;
    level::levelCreateCube(
        state.level, 0.0, 0.0, 0.0, TVector::up, TVector::right);
    level::levelCreateCube(
        state.level, 1.0, 0.0, 0.0, TVector::up, TVector::right);
    check(level::levelConnectSectors(state.level, 0, 1),
        "transient preview fixture authors a real shared-face adjoin");
    state.selection.sectors.insert(0);
    state.selection.sectors.insert(1);
    window.activateSectorEditModeForTest();
    window.setDocumentStateForTest(std::move(state));
    window.activateSectorEditModeForTest();

    GlView* const view = window.glViewForTest();
    QAction* const joinAction = window.findChild<QAction*>(
        QStringLiteral("viewport.sector.join"));
    check(view && joinAction,
        "transient preview normalization fixture exposes viewport and Join action");
    if (!view || !joinAction)
    {
        return;
    }

    view->setLevel(
        &window.documentStateForTest().level,
        GlView::SetLevelMode::resetView);
    view->setSelectionState(
        0, -1, -1, -1, window.documentStateForTest().selection);
    view->captureStagedCleaveContextForTest(false);
    check(window.documentStateForTest().selection.sectors
                == std::unordered_set<std::int32_t>{ 0, 1 }
            && view->captureStagedCleaveDraftState().context.targetSectors
                == std::vector<std::int32_t>{ 0, 1 },
        "transient preview fixture preserves both joined Sector targets after mode activation");
    window.resize(1280, 900);
    window.show();
    window.activateWindow();
    QApplication::processEvents(QEventLoop::AllEvents);
    window.updateEditActionsForTest();
    check(joinAction->isEnabled(),
        "transient preview fixture enables Join for its real two-sector selection");
    if (!joinAction->isEnabled())
    {
        return;
    }
    QAction* const cleaveAction = window.findChild<QAction*>(
        QStringLiteral("viewport.sector.cleave"));
    if (QToolButton* const multiCut = window.findChild<QToolButton*>(
            QStringLiteral("cleaveMultiCutButton")))
    {
        multiCut->setChecked(true);
    }
    if (cleaveAction && cleaveAction->isEnabled() && !cleaveAction->isChecked())
    {
        cleaveAction->trigger();
    }
    view->captureStagedCleaveContextForTest(false);
    const GlView::RecoveryState::CleaveDraftSnapshot authoredCleaveContext =
        view->captureStagedCleaveDraftState();
    check(cleaveAction && cleaveAction->isChecked()
            && authoredCleaveContext.context.targetSectors
                == std::vector<std::int32_t>{ 0, 1 },
        "transient preview fixture first captures Cleave against both real selected Sectors");
    if (!cleaveAction || !cleaveAction->isChecked()
        || authoredCleaveContext.context.targetSectors
            != std::vector<std::int32_t>{ 0, 1 })
    {
        return;
    }
    joinAction->trigger();
    QApplication::processEvents(QEventLoop::AllEvents);
    QWidget* const joinOptions = window.findChild<QWidget*>(
        QStringLiteral("sectorJoinOptionsOverlay"));
    check(joinAction->isChecked() && joinOptions && joinOptions->isVisible(),
        "transient preview fixture activates and displays the real Join preview owner");
    if (!joinAction->isChecked() || !joinOptions || !joinOptions->isVisible())
    {
        return;
    }

    // Seed a coherent settled DTO beneath the still-active Join preview to
    // exercise capture's suspended-owner boundary independently of tool teardown.
    view->setCleaveStagedMode(true);
    view->setCleaveChainEnabled(true);
    view->setCleaveBounded(false);
    GlView::RecoveryState::CleaveShapeOptions shape{};
    shape.kind = ui::viewport::CleaveShapeKind::Rectangle;
    shape.width = 1.75;
    shape.height = 0.875;
    shape.dimensionsLinked = false;
    shape.placementArmed = true;
    view->setCleaveShapeOptions(shape);
    view->captureStagedCleaveContextForTest(false);

    GlView::RecoveryState::CleaveDraftSnapshot draft = authoredCleaveContext;
    GlView::RecoveryState::CleaveLine line{};
    line.first = TVector{ 0.5, -1.0, 0.5 };
    line.second = TVector{ 0.5, 2.0, 0.5 };
    line.planeNormal = TVector{ -1.0, 0.0, 0.0 };
    line.constructionPlaneNormal = TVector{ 0.0, 0.0, 1.0 };
    line.angleReferenceAxis = TVector{ 0.0, 1.0, 0.0 };
    draft.lines = { line };
    draft.context = authoredCleaveContext.context;
    draft.applyOrder = { 0 };
    draft.selectedCleaveLines = { 0 };
    draft.selectedCleaveLine = 0;
    GlView::RecoveryState::CleaveHistory history{};
    history.snapshots = { draft };
    history.cursor = 0;
    {
        const QSignalBlocker suppressPreviewOwnerSync(view);
        view->restoreStagedCleaveDraftState(draft, history);
        view->setStagedCleaveTransformMode(
            GlView::StagedCleaveTransformMode::Rotate);
    }
    check(view->stagedCleaveLineCount() == 1
            && view->captureStagedCleaveDraftState().context.targetSectors
                == std::vector<std::int32_t>{ 0, 1 }
            && view->recoveryState().cleaveHistory
            && view->recoveryState().cleaveHistory->snapshots.size() == 1,
        "transient preview fixture starts with a coherent nonempty Cleave history");

    check(joinAction->isChecked() && joinOptions->isVisible(),
        "transient preview remains active after injecting the stale Cleave DTO");
    const QByteArray captured = window.captureRecoveryEditorStateForTest();
    const QCborMap cleave = currentCleaveMap(editorRoot(captured));
    const QCborMap selection = cleave.value(QStringLiteral("selection")).toMap();
    const QCborMap historyMap = cleave.value(QStringLiteral("history")).toMap();
    const QCborMap shapeMap = cleave.value(QStringLiteral("shape")).toMap();
    const QCborArray lines = cleave.value(QStringLiteral("lines")).toArray();
    const QCborArray applyOrder = cleave.value(QStringLiteral("applyOrder")).toArray();
    const QCborArray selectedLines = selection.value(QStringLiteral("lines")).toArray();
    check(window.validateRecoveryEditorStateForTest(captured)
            && lines.size() == 1
            && applyOrder == QCborArray{ qint64(0) }
            && selectedLines == QCborArray{ qint64(0) }
            && selection.value(QStringLiteral("active")).toInteger(-2) == 0
            && historyMap.value(QStringLiteral("snapshots")).toArray().size() == 1
            && cleave.value(QStringLiteral("transformMode")).toInteger(-1) == 1
            && shapeMap.value(QStringLiteral("kind")).toInteger(-1) == 1
            && shapeMap.value(QStringLiteral("width")).toDouble() == 1.75
            && shapeMap.value(QStringLiteral("height")).toDouble() == 0.875
            && shapeMap.value(QStringLiteral("dimensionsLinked")).toBool()
                == shape.dimensionsLinked
            && shapeMap.value(QStringLiteral("placementArmed")).toBool()
                == shape.placementArmed,
        "transient Join preview retains settled dormant Cleave draft history, shape and transform configuration in EDIT");
}

} // namespace

void runCleaveApplyWorkspaceHistoryTests()
{
    QTemporaryDir directory{};
    check(directory.isValid(),
        "Apply workspace history tests use an isolated native document directory");
    if (!directory.isValid())
    {
        return;
    }

    const std::filesystem::path root(directory.path().toStdWString());
    for ( const bool surfaceApply : { false, true } )
    {
        for ( const ApplyPeerRedoRoute route : { ApplyPeerRedoRoute::Dormant,
                  ApplyPeerRedoRoute::Local, ApplyPeerRedoRoute::Document,
                  ApplyPeerRedoRoute::History } )
        {
            runAppliedFamilyOnlyHistoryTest(surfaceApply, route);
        }
    }
    runImportedCleaveHistoryCursorTest(root);
    runImportedLayerHistoryTest(root);
    runWorkspaceHistoryBindingDegradationTest(root);
    runTransientPreviewCaptureNormalizationTest();
}

} // namespace sed::app_tests
