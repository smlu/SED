#include "test_common.hpp"

#include "gl_view.hpp"
#include "main_window/main_window.hpp"
#include "sed/assets/3do.hpp"
#include "ui/recovery/architectural_lighting_workspace_utils.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

#include <QApplication>
#include <QByteArray>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QEventLoop>
#include <QMouseEvent>

#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace sed::app_tests
{
namespace
{

editor::DocumentState makeArchWorkspaceDocument()
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::MOTS;
    state.level.layers.emplace_back("Arch workspace fixture");

    Thing owner{};
    owner.name = "arch_workspace_fixture";
    owner.templateName = owner.name;
    owner.layer = 0;
    owner.sector = -1;
    owner.archLightingIndex = 0;

    TplValue modelName{};
    modelName.name = "model3do";
    modelName.type = ValueType::str;
    modelName.stringValue = "arch_workspace_fixture.3do";
    owner.values.push_back(std::move(modelName));

    TplValue flags{};
    flags.name = "thingflags";
    flags.type = ValueType::integer;
    flags.intValue = 0x8;
    owner.values.push_back(std::move(flags));

    state.level.things.push_back(std::move(owner));

    MotsArchLightMesh lightingMesh{};
    lightingMesh.wireIndex = 0;
    lightingMesh.vertices = {
        MotsArchLightVertex{ 0.1f, 0.2f, 0.3f, 1.0f },
        MotsArchLightVertex{ 0.2f, 0.3f, 0.4f, 1.0f },
        MotsArchLightVertex{ 0.3f, 0.4f, 0.5f, 1.0f },
    };
    MotsArchLightObject lightingObject{};
    lightingObject.wireIndex = 0;
    lightingObject.meshes.push_back(std::move(lightingMesh));
    state.level.motsArchLighting.push_back(std::move(lightingObject));
    return state;
}

std::shared_ptr<const assets::Model3do> makeArchWorkspaceModel()
{
    auto model = std::make_shared<assets::Model3do>();
    model->version = 2.1;

    assets::Model3doMesh mesh{};
    mesh.name = "arch_workspace_fixture";
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    mesh.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    mesh.vertices = {
        assets::Model3doVertex{ -0.25, -0.10, 0.0, ColorWhite },
        assets::Model3doVertex{ 0.25, -0.10, 0.0, ColorWhite },
        assets::Model3doVertex{ 0.0, 0.25, 0.0, ColorWhite },
    };

    assets::Model3doFace face{};
    face.geo = static_cast<std::int32_t>(TGeoMode::Solid);
    face.light = static_cast<std::int32_t>(TLightMode::VertexDiffuseOnly);
    face.faceFlags |= FF_DoubleSided;
    face.vertices = { 0, 1, 2 };
    mesh.faces.push_back(std::move(face));
    model->meshes.push_back(std::move(mesh));
    return model;
}

void prepareArchWorkspaceWindow(
    MainWindow& window,
    const bool activateArchMode)
{
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.resize(640, 480);
    window.setDocumentStateForTest(makeArchWorkspaceDocument());
    GlView* const view = window.glViewForTest();
    if ( !view )
    {
        return;
    }

    view->setShowThings(true);
    view->setThingViewMode(GlView::ThingViewMode::Shaded);
    view->setResourceLocator(&window.resourceLocatorForTest(), false);
    view->primeModel3do(
        "arch_workspace_fixture.3do",
        makeArchWorkspaceModel());
    view->setLevel(&window.mutableDocumentStateForTest().level);
    if ( activateArchMode )
    {
        window.activateArchitecturalLightingEditModeForTest();
        view->setSelectedThing(0);
    }
    view->update();
    window.show();
    QApplication::processEvents(QEventLoop::AllEvents);
}

bool clickArchOwnerMarker(GlView& view)
{
    const std::optional<QVector3D> anchor =
        view.architecturalLightingOwnerGizmoDisplayPositionForTest(0);
    if ( !anchor )
    {
        return false;
    }
    const std::optional<QPointF> projected = view.projectWorldToViewport(*anchor);
    if ( !projected )
    {
        return false;
    }

    const QPoint point(qRound(projected->x()), qRound(projected->y()));
    QMouseEvent press(
        QEvent::MouseButtonPress,
        QPointF(point),
        QPointF(point),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    QApplication::sendEvent(&view, &press);
    QMouseEvent release(
        QEvent::MouseButtonRelease,
        QPointF(point),
        QPointF(point),
        Qt::LeftButton,
        Qt::NoButton,
        Qt::NoModifier);
    QApplication::sendEvent(&view, &release);
    QApplication::processEvents(QEventLoop::AllEvents);
    return true;
}

struct ArchWorkspaceCase
{
    level::MotsArchLightingVertexMode authoringMode{
        level::MotsArchLightingVertexMode::Rgb};
    GlView::ArchitecturalLightingTargetMode targetMode{
        GlView::ArchitecturalLightingTargetMode::Owner};
    std::vector<GlView::ArchitecturalLightingVertexIdentity> selectedVertices{};
    bool implicitOwnerExpansion{};
};

bool matchesCborIdentities(
    const QCborValue& value,
    const std::vector<GlView::ArchitecturalLightingVertexIdentity>& expected)
{
    if ( !value.isArray() )
    {
        return false;
    }

    const QCborArray actual = value.toArray();
    if ( actual.size() != static_cast<qsizetype>(expected.size()) )
    {
        return false;
    }

    for ( qsizetype index = 0; index < actual.size(); ++index )
    {
        if ( !actual.at(index).isArray() )
        {
            return false;
        }
        const QCborArray identity = actual.at(index).toArray();
        const GlView::ArchitecturalLightingVertexIdentity& wanted =
            expected[static_cast<std::size_t>(index)];
        if ( identity.size() != 4
            || !identity.at(0).isInteger()
            || !identity.at(1).isInteger()
            || !identity.at(2).isInteger()
            || !identity.at(3).isInteger()
            || identity.at(0).toInteger() != wanted.thingIndex
            || identity.at(1).toInteger() != wanted.lodIndex
            || identity.at(2).toInteger() != wanted.meshIndex
            || identity.at(3).toInteger() != wanted.vertexIndex )
        {
            return false;
        }
    }
    return true;
}

QCborMap makeArchWorkspaceMap(
    const QCborArray& selectedVertices,
    const int authoringMode = 1,
    const int targetMode = 0,
    const bool implicitOwnerExpansion = false)
{
    QCborMap encoded{};
    encoded.insert(QStringLiteral("version"), 1);
    encoded.insert(QStringLiteral("authoringMode"), authoringMode);
    encoded.insert(QStringLiteral("targetMode"), targetMode);
    encoded.insert(QStringLiteral("selectedVertices"), selectedVertices);
    encoded.insert(
        QStringLiteral("implicitOwnerExpansion"), implicitOwnerExpansion);
    return encoded;
}

bool matchesLiveArchWorkspace(
    const GlView& view,
    const ArchWorkspaceCase& expected)
{
    const GlView::RecoveryState recovered = view.recoveryState();
    if ( !recovered.architecturalLighting.has_value() )
    {
        return false;
    }

    const GlView::RecoveryState::ArchitecturalLightingState& actual =
        *recovered.architecturalLighting;
    if (actual.authoringMode != expected.authoringMode
        || actual.targetMode != expected.targetMode
        || actual.implicitOwnerExpansion != expected.implicitOwnerExpansion
        || actual.selectedVertices != expected.selectedVertices
        || view.architecturalLightingAuthoringMode()
            != static_cast<level::MotsArchLightingVertexMode>(
                expected.authoringMode)
        || view.architecturalLightingTargetMode()
            != static_cast<GlView::ArchitecturalLightingTargetMode>(
                expected.targetMode))
    {
        return false;
    }
    const auto liveVertices = view.architecturalLightingSelectedVertices();
    if (liveVertices.size() != expected.selectedVertices.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < liveVertices.size(); ++index)
    {
        if (liveVertices[index] != expected.selectedVertices[index])
        {
            return false;
        }
    }
    return true;
}

} // namespace

void runMotsArchLightingWorkspaceRecoveryTests()
{
    const Level fixtureLevel = makeArchWorkspaceDocument().level;
    QCborArray oneIdentity{};
    oneIdentity.append(QCborArray{ 0, 0, 0, 2 });
    QCborMap extendedMap = makeArchWorkspaceMap(oneIdentity, 0, 1, false);
    extendedMap.insert(QStringLiteral("futureOptionalSetting"), true);

    GlView::RecoveryState::ArchitecturalLightingState decodedWorkspace{};
    QString decodeError{};
    check(ui::recovery::architecturalLightingWorkspaceFromCbor(
                QCborValue(extendedMap), fixtureLevel, &decodedWorkspace,
                &decodeError)
            && decodedWorkspace.authoringMode
                == level::MotsArchLightingVertexMode::Mono
            && decodedWorkspace.targetMode
                == GlView::ArchitecturalLightingTargetMode::Vertices
            && decodedWorkspace.selectedVertices
                == std::vector<GlView::ArchitecturalLightingVertexIdentity>{
                    { 0, 0, 0, 2 } }
            && !decodedWorkspace.implicitOwnerExpansion,
        "MotS Arch workspace decoding tolerates unknown optional keys without losing known selections");

    constexpr int oversizedIdentityCount = 90'000;
    QCborArray oversizedIdentities{};
    for ( int vertexIndex = 0; vertexIndex < oversizedIdentityCount; ++vertexIndex )
    {
        oversizedIdentities.append(QCborArray{ 0, 0, 0, vertexIndex });
    }
    const GlView::RecoveryState::ArchitecturalLightingState previousWorkspace{
        level::MotsArchLightingVertexMode::Rgb,
        GlView::ArchitecturalLightingTargetMode::Owner,
        { GlView::ArchitecturalLightingVertexIdentity{ 0, 0, 0, 1 } },
        true };
    decodedWorkspace = previousWorkspace;
    check(!ui::recovery::architecturalLightingWorkspaceFromCbor(
                QCborValue(makeArchWorkspaceMap(oversizedIdentities)),
                fixtureLevel, &decodedWorkspace, &decodeError)
            && decodedWorkspace.authoringMode == previousWorkspace.authoringMode
            && decodedWorkspace.targetMode == previousWorkspace.targetMode
            && decodedWorkspace.selectedVertices
                == previousWorkspace.selectedVertices
            && decodedWorkspace.implicitOwnerExpansion
                == previousWorkspace.implicitOwnerExpansion,
        "oversized MotS Arch identity lists are rejected before publishing any partial selection");

    GlView::RecoveryState::ArchitecturalLightingState oversizedWorkspace{};
    oversizedWorkspace.selectedVertices.reserve(oversizedIdentityCount);
    for ( int vertexIndex = 0; vertexIndex < oversizedIdentityCount; ++vertexIndex )
    {
        oversizedWorkspace.selectedVertices.push_back(
            GlView::ArchitecturalLightingVertexIdentity{
                0, 0, 0, vertexIndex });
    }
    QCborValue encodedWorkspace{QStringLiteral("unchanged")};
    check(!ui::recovery::architecturalLightingWorkspaceToCbor(
                oversizedWorkspace, &encodedWorkspace, &decodeError)
            && encodedWorkspace.toString() == QStringLiteral("unchanged")
            && !decodeError.isEmpty(),
        "oversized MotS Arch workspace encoding fails atomically with a useful error");

    GlView::RecoveryState::ArchitecturalLightingState invalidEncodedMode{};
    invalidEncodedMode.authoringMode =
        static_cast<level::MotsArchLightingVertexMode>(2);
    encodedWorkspace = QCborValue(QStringLiteral("unchanged"));
    check(!ui::recovery::architecturalLightingWorkspaceToCbor(
                invalidEncodedMode, &encodedWorkspace, &decodeError)
            && encodedWorkspace.toString() == QStringLiteral("unchanged")
            && !decodeError.isEmpty(),
        "MotS Arch workspace encoding rejects an invalid authoring enum without replacing prior output");

    GlView::RecoveryState::ArchitecturalLightingState invalidEncodedTarget{};
    invalidEncodedTarget.targetMode =
        static_cast<GlView::ArchitecturalLightingTargetMode>(2);
    encodedWorkspace = QCborValue(QStringLiteral("unchanged"));
    check(!ui::recovery::architecturalLightingWorkspaceToCbor(
                invalidEncodedTarget, &encodedWorkspace, &decodeError)
            && encodedWorkspace.toString() == QStringLiteral("unchanged")
            && !decodeError.isEmpty(),
        "MotS Arch workspace encoding rejects an invalid target enum without replacing prior output");

    const std::array cases{
        ArchWorkspaceCase{
            level::MotsArchLightingVertexMode::Mono,
            GlView::ArchitecturalLightingTargetMode::Vertices,
            {
                GlView::ArchitecturalLightingVertexIdentity{ 0, 0, 0, 2 },
                GlView::ArchitecturalLightingVertexIdentity{ 0, 0, 0, 0 },
            },
            false,
        },
        ArchWorkspaceCase{
            level::MotsArchLightingVertexMode::Rgb,
            GlView::ArchitecturalLightingTargetMode::Owner,
            {
                GlView::ArchitecturalLightingVertexIdentity{ 0, 0, 0, 0 },
                GlView::ArchitecturalLightingVertexIdentity{ 0, 0, 0, 1 },
                GlView::ArchitecturalLightingVertexIdentity{ 0, 0, 0, 2 },
            },
            true,
        },
    };

    for ( const ArchWorkspaceCase& expected : cases )
    {
        MainWindow source{};
        prepareArchWorkspaceWindow(source, true);
        GlView* const sourceView = source.glViewForTest();
        check(sourceView != nullptr,
            "MotS Arch workspace capture fixture exposes its viewport");
        if ( !sourceView )
        {
            continue;
        }

        sourceView->setArchitecturalLightingAuthoringMode(expected.authoringMode);
        sourceView->setArchitecturalLightingTargetMode(expected.targetMode);
        if ( expected.implicitOwnerExpansion )
        {
            check(clickArchOwnerMarker(*sourceView),
                "MotS Arch workspace fixture expands the owner through a real viewport click");
        }
        else
        {
            sourceView->setArchitecturalLightingSelectedVertices(
                expected.selectedVertices);
        }
        check(matchesLiveArchWorkspace(*sourceView, expected),
            "MotS Arch workspace fixture seeds the exact channel, target, identities, and selection provenance");

        const QByteArray payload = source.captureRecoveryEditorStateForTest();
        const QCborValue decoded = QCborValue::fromCbor(payload);
        const QCborMap root = decoded.toMap();
        const QCborValue archValue = root.value(
            QStringLiteral("architecturalLighting"));
        check(archValue.isMap(),
            "EDIT capture stores the optional MotS Arch workspace map");

        const QCborMap arch = archValue.toMap();
        check(arch.value(QStringLiteral("version")).toInteger(-1) == 1
                && arch.value(QStringLiteral("authoringMode")).toInteger(-1)
                    == static_cast<int>(expected.authoringMode)
                && arch.value(QStringLiteral("targetMode")).toInteger(-1)
                    == static_cast<int>(expected.targetMode)
                && arch.value(QStringLiteral("implicitOwnerExpansion")).isBool()
                && arch.value(QStringLiteral("implicitOwnerExpansion")).toBool()
                    == expected.implicitOwnerExpansion
                && matchesCborIdentities(
                    arch.value(QStringLiteral("selectedVertices")),
                    expected.selectedVertices),
            "EDIT capture preserves Arch channel, target, exact ordered vertex IDs, and implicit-owner provenance");

        QString warning{};
        check(source.validateRecoveryEditorStateForTest(payload, &warning),
            "captured MotS Arch workspace passes EDIT validation");

        MainWindow restored{};
        prepareArchWorkspaceWindow(restored, false);
        const bool didRestore =
            restored.restoreRecoveryEditorStateForTest(payload, &warning);
        GlView* const restoredView = restored.glViewForTest();
        check(didRestore && restoredView != nullptr,
            "MotS Arch workspace payload restores into a matching document");
        if ( didRestore && restoredView )
        {
            check(matchesLiveArchWorkspace(*restoredView, expected),
                "EDIT restore reproduces the exact Arch channel, target, identities, and selection provenance");
            const QByteArray recaptured =
                restored.captureRecoveryEditorStateForTest();
            const QCborMap recapturedRoot =
                QCborValue::fromCbor(recaptured).toMap();
            check(recapturedRoot.value(
                        QStringLiteral("architecturalLighting"))
                        == archValue,
                "restored MotS Arch workspace re-captures without changing its exact identity set");
        }

        if ( archValue.isMap() )
        {
            QCborMap malformedArch = arch;
            malformedArch.insert(QStringLiteral("authoringMode"), 2);
            QCborMap malformedRoot = root;
            malformedRoot.insert(
                QStringLiteral("architecturalLighting"), malformedArch);
            const QByteArray malformed = ui::recovery::serializeCanonicalCbor(
                QCborValue(malformedRoot));
            check(!source.validateRecoveryEditorStateForTest(malformed, &warning),
                "EDIT validation rejects an out-of-domain MotS Arch authoring mode atomically");
        }
    }
}

} // namespace sed::app_tests
