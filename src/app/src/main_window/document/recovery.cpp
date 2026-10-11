#include "main_window/common.hpp"
#include "ui/components/scrub_float_edit.hpp"

#include "app_icon.hpp"
#include "sed/assets/sed.hpp"
#include "ui/audio/app_sound_player.hpp"
#include "ui/project/project_startup_utils.hpp"
#include "ui/recovery/architectural_lighting_workspace_utils.hpp"
#include "ui/recovery/partial_level_salvage.hpp"
#include "ui/layers/layer_color_utils.hpp"
#include "ui/resources/resource_authoring_editor.hpp"
#include "ui/viewport/cleave_shape_utils.hpp"

#include <QDateTime>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QScopeGuard>
#include <QShowEvent>
#include <QToolButton>
#include <QThread>
#include <QThreadPool>
#include <QWindow>
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QtConcurrent/QtConcurrentRun>

#include <cstdio>
#include <new>
#include <numeric>
#include <span>
#include <stdexcept>

namespace sed
{
namespace
{

constexpr int kDefaultRecoveryIntervalMinutes = 5;
constexpr int kMinimumRecoveryIntervalMinutes = 1;
constexpr int kMaximumRecoveryIntervalMinutes = 30;
constexpr int kRecoveryDebounceMilliseconds = 650;
constexpr int kRecoveryRetryInitialMilliseconds = 2'000;
constexpr int kRecoveryRetryMaximumMilliseconds = 30'000;
constexpr int kRecoveryShutdownImmediateRetries = 2;
constexpr std::size_t kMaximumAutosaveHistoryPerDocument = 10;
constexpr int kRecoveryEditorStateVersion = 1;
constexpr int kSurfaceSelectionContextVersion = 1;
constexpr int kRecoveryMainWindowStateVersion = 1;
constexpr int kRecoveryWorkspaceStateVersion = 1;
constexpr quint64 kMaximumRecoveryEditorCborNodes = 2'000'000;
constexpr int kMaximumRecoveryEditorCborDepth = 64;
constexpr std::size_t kMaximumCleaveHistorySnapshots = 64;
constexpr quint64 kMaximumCleaveHistoryBytes = 16ull * 1024ull * 1024ull;
constexpr std::size_t kMaximumCleaveLines = 100'000;
// EDIT can legitimately carry unsaved COG source. The semantic validator
// permits one script with up to 64 Mi UTF-16 code units; canonical UTF-8 needs
// at most three bytes per code unit. Keep byte strings at the complete native
// semantic-section ceiling because the nested COG workspace can contain more
// than one such script, while the total EDIT input remains bounded below.
constexpr quint64 kMaximumRecoveryEditorCborTextBytes =
    3ull * 64ull * 1024ull * 1024ull;
constexpr quint64 kMaximumRecoveryEditorCborByteStringBytes =
    assets::SedMaximumSemanticSectionBytes;
static_assert(assets::SedMaximumSemanticSectionBytes
    <= static_cast<std::uint64_t>(std::numeric_limits<qsizetype>::max()));

bool normalizeRecoveredViewportUvSplitterSizes(
    QList<int>& sizes,
    const int currentPaneCount)
{
    if (sizes.size() == currentPaneCount)
    {
        return true;
    }

    // Early V1 EDIT snapshots accidentally serialized the hidden UV docking
    // QRubberBand as a third QSplitter pane. It was always appended and had
    // an exact zero extent. Accept only that known legacy shape; arbitrary
    // count mismatches remain ignored rather than being guessed into a new
    // workspace layout.
    if (currentPaneCount == 2 && sizes.size() == 3 && sizes.back() == 0)
    {
        sizes.removeLast();
        return true;
    }
    return false;
}

QString recoveryDisplayName(const std::filesystem::path& sourcePath)
{
    if (sourcePath.empty())
    {
        return QStringLiteral("Untitled");
    }
    const QString basename = QFileInfo(
        QString::fromStdWString(sourcePath.wstring())).fileName().trimmed();
    return basename.isEmpty() ? QStringLiteral("Untitled") : basename;
}

// QCborValue's to* accessors are deliberately permissive (for example a
// boolean can be converted to 0/1 and a string can be converted to a default
// numeric value).  EDIT is a canonical V1 schema, not a best-effort settings
// import, so every scalar must be type checked before conversion.  Keep the
// finite-number check separate so an authored camera with valid=false can
// still preserve its finite, but otherwise non-renderable, values.
bool cborFiniteNumber(const QCborValue& value, double* out = nullptr)
{
    if (!value.isInteger() && !value.isDouble())
    {
        return false;
    }
    const double recovered = value.toDouble();
    if (!std::isfinite(recovered))
    {
        return false;
    }
    if (out)
    {
        *out = recovered;
    }
    return true;
}

bool cborNumberInRange(
    const QCborValue& value,
    const double minimum,
    const double maximum,
    double* out = nullptr)
{
    double recovered = 0.0;
    if (!cborFiniteNumber(value, &recovered)
        || recovered < minimum || recovered > maximum)
    {
        return false;
    }
    if (out)
    {
        *out = recovered;
    }
    return true;
}

bool cborIntegerInRange(
    const QCborValue& value,
    const qint64 minimum,
    const qint64 maximum,
    qint64* out = nullptr)
{
    if (!value.isInteger())
    {
        return false;
    }
    const qint64 recovered = value.toInteger();
    if (recovered < minimum || recovered > maximum)
    {
        return false;
    }
    if (out)
    {
        *out = recovered;
    }
    return true;
}

bool cborInt32(const QCborValue& value, std::int32_t* out = nullptr)
{
    if (!value.isInteger())
    {
        return false;
    }
    const qint64 recovered = value.toInteger();
    if (recovered < std::numeric_limits<std::int32_t>::min()
        || recovered > std::numeric_limits<std::int32_t>::max())
    {
        return false;
    }
    if (out)
    {
        *out = static_cast<std::int32_t>(recovered);
    }
    return true;
}

QCborArray vectorToCbor(const QVector3D& value)
{
    return QCborArray{ value.x(), value.y(), value.z() };
}

QCborArray vectorToCbor(const TVector& value)
{
    return QCborArray{ value.x, value.y, value.z };
}

bool vectorFromCbor(const QCborValue& value, TVector* out)
{
    if (!out || !value.isArray())
    {
        return false;
    }
    const QCborArray array = value.toArray();
    if (array.size() != 3)
    {
        return false;
    }
    double x{};
    double y{};
    double z{};
    if (!cborNumberInRange(array.at(0), -1.0e12, 1.0e12, &x)
        || !cborNumberInRange(array.at(1), -1.0e12, 1.0e12, &y)
        || !cborNumberInRange(array.at(2), -1.0e12, 1.0e12, &z))
    {
        return false;
    }
    *out = TVector{x, y, z};
    return true;
}

bool vectorFromCbor(const QCborValue& value, QVector3D* out)
{
    TVector documentValue{};
    if (!out || !vectorFromCbor(value, &documentValue))
    {
        return false;
    }
    *out = QVector3D(static_cast<float>(documentValue.x),
        static_cast<float>(documentValue.y), static_cast<float>(documentValue.z));
    return true;
}

QCborArray matrixToCbor(const QMatrix4x4& matrix)
{
    QCborArray array{};
    const float* values = matrix.constData();
    for (int i = 0; i < 16; ++i) array.append(values[i]);
    return array;
}

bool matrixFromCbor(const QCborValue& value, QMatrix4x4* out)
{
    if (!out || !value.isArray()) return false;
    const QCborArray array = value.toArray();
    if (array.size() != 16) return false;
    float values[16]{};
    for (int i = 0; i < 16; ++i)
    {
        double component{};
        if (!cborNumberInRange(array.at(i), -1.0e12, 1.0e12, &component))
        {
            return false;
        }
        values[i] = static_cast<float>(component);
    }
    // matrixToCbor writes QMatrix4x4's OpenGL-compatible column-major data.
    // The pointer constructor consumes row-major values, so assigning by
    // element avoids transposing captured projection/view matrices.
    QMatrix4x4 parsed{};
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            parsed(row, column) = values[column * 4 + row];
        }
    }
    *out = parsed;
    return true;
}

QCborArray orbitToCbor(const app::ViewportCamera::OrbitState& orbit)
{
    return QCborArray{
        vectorToCbor(orbit.center),
        vectorToCbor(orbit.pan),
        orbit.radius,
        orbit.distance,
        orbit.yawDeg,
        orbit.pitchDeg,
        orbit.rollDeg,
    };
}

bool orbitFromCbor(const QCborValue& value, app::ViewportCamera::OrbitState* out)
{
    if (!out || !value.isArray())
    {
        return false;
    }
    const QCborArray array = value.toArray();
    if (array.size() != 7
        || !vectorFromCbor(array.at(0), &out->center)
        || !vectorFromCbor(array.at(1), &out->pan))
    {
        return false;
    }
    double radius{};
    double distance{};
    double yaw{};
    double pitch{};
    double roll{};
    if (!cborNumberInRange(array.at(2), 0.0, 1.0e12, &radius)
        || !cborNumberInRange(array.at(3), 0.000001, 1.0e12, &distance)
        || !cborNumberInRange(array.at(4), -1.0e9, 1.0e9, &yaw)
        || !cborNumberInRange(array.at(5), -1.0e9, 1.0e9, &pitch)
        || !cborNumberInRange(array.at(6), -1.0e9, 1.0e9, &roll))
    {
        return false;
    }
    out->radius = static_cast<float>(radius);
    out->distance = static_cast<float>(distance);
    out->yawDeg = static_cast<float>(yaw);
    out->pitchDeg = static_cast<float>(pitch);
    out->rollDeg = static_cast<float>(roll);
    return true;
}

QCborArray cleaveLinesToCbor(
    const std::span<const GlView::RecoveryState::CleaveLine> lines)
{
    QCborArray encoded{};
    for (const GlView::RecoveryState::CleaveLine& line : lines)
    {
        encoded.append(QCborArray{
            vectorToCbor(line.first), vectorToCbor(line.second),
            vectorToCbor(line.planeNormal), vectorToCbor(line.constructionPlaneNormal),
            vectorToCbor(line.angleReferenceAxis),
            orbitToCbor(line.authoredCamera.orbit),
            static_cast<int>(line.authoredCamera.projectionType),
            line.authoredCamera.fovXDeg, line.authoredCamera.orthoScale,
            line.authoredCamera.nearPlane, line.authoredCamera.farPlane,
            line.authoredCamera.clipEnabled, line.authoredCamera.valid,
            line.linkedToPrevious, line.linkedToNext });
    }
    return encoded;
}

QCborArray cleaveContextToCbor(
    const GlView::RecoveryState::CleaveContext& context)
{
    QCborArray targetSectors{};
    for (const std::int32_t sector : context.targetSectors)
    {
        targetSectors.append(sector);
    }
    QCborArray targetSurfaces{};
    for (const SurfaceRef& surface : context.targetSurfaces)
    {
        targetSurfaces.append(QCborArray{ surface.sector, surface.surface });
    }
    return QCborArray{
        context.valid, context.surfaceMode,
        matrixToCbor(context.projection), matrixToCbor(context.view),
        context.viewportWidth, context.viewportHeight,
        context.sourceSurface.sector, context.sourceSurface.surface,
        targetSectors, targetSurfaces };
}

QCborMap cleaveDraftToCbor(
    const GlView::RecoveryState::CleaveDraftSnapshot& snapshot)
{
    QCborMap encoded{};
    encoded.insert(QStringLiteral("lines"), cleaveLinesToCbor(snapshot.lines));
    encoded.insert(QStringLiteral("context"), cleaveContextToCbor(snapshot.context));
    if (snapshot.chainOrigin)
    {
        encoded.insert(QStringLiteral("chainOrigin"), QCborArray{
            snapshot.chainOrigin->lineIndex, snapshot.chainOrigin->endpoint });
    }
    QCborArray order{};
    for (const std::int32_t index : snapshot.applyOrder)
    {
        order.append(index);
    }
    encoded.insert(QStringLiteral("applyOrder"), order);
    QCborArray selected{};
    for (const std::int32_t index : snapshot.selectedCleaveLines)
    {
        selected.append(index);
    }
    encoded.insert(QStringLiteral("selection"), QCborMap{
        { QStringLiteral("lines"), selected },
        { QStringLiteral("active"), snapshot.selectedCleaveLine },
    });
    return encoded;
}

bool cleaveLinesFromCbor(
    const QCborValue& value,
    std::vector<GlView::RecoveryState::CleaveLine>* out)
{
    if (!out || !value.isArray())
    {
        return false;
    }
    const QCborArray lines = value.toArray();
    if (static_cast<std::size_t>(lines.size()) > kMaximumCleaveLines)
    {
        return false;
    }
    std::vector<GlView::RecoveryState::CleaveLine> parsed{};
    parsed.reserve(static_cast<std::size_t>(lines.size()));
    for (const QCborValue& lineValue : lines)
    {
        if (!lineValue.isArray())
        {
            return false;
        }
        const QCborArray line = lineValue.toArray();
        GlView::RecoveryState::CleaveLine recovered{};
        if (line.size() != 15
            || !vectorFromCbor(line.at(0), &recovered.first)
            || !vectorFromCbor(line.at(1), &recovered.second)
            || !vectorFromCbor(line.at(2), &recovered.planeNormal)
            || !vectorFromCbor(line.at(3), &recovered.constructionPlaneNormal)
            || !vectorFromCbor(line.at(4), &recovered.angleReferenceAxis)
            || !orbitFromCbor(line.at(5), &recovered.authoredCamera.orbit))
        {
            return false;
        }
        qint64 projection = -1;
        if (!cborIntegerInRange(line.at(6), 0, 1, &projection)
            || !line.at(11).isBool() || !line.at(12).isBool()
            || !line.at(13).isBool() || !line.at(14).isBool())
        {
            return false;
        }
        recovered.authoredCamera.projectionType =
            static_cast<app::ViewportCamera::ProjectionType>(projection);
        double authoredFov = 0.0;
        double authoredOrthoScale = 0.0;
        double authoredNear = 0.0;
        double authoredFar = 0.0;
        if (!cborFiniteNumber(line.at(7), &authoredFov)
            || !cborFiniteNumber(line.at(8), &authoredOrthoScale)
            || !cborFiniteNumber(line.at(9), &authoredNear)
            || !cborFiniteNumber(line.at(10), &authoredFar))
        {
            return false;
        }
        recovered.authoredCamera.fovXDeg = static_cast<float>(authoredFov);
        recovered.authoredCamera.orthoScale = static_cast<float>(authoredOrthoScale);
        recovered.authoredCamera.nearPlane = static_cast<float>(authoredNear);
        recovered.authoredCamera.farPlane = static_cast<float>(authoredFar);
        recovered.authoredCamera.clipEnabled = line.at(11).toBool();
        recovered.authoredCamera.valid = line.at(12).toBool();
        if (!std::isfinite(recovered.authoredCamera.fovXDeg)
            || !std::isfinite(recovered.authoredCamera.orthoScale)
            || !std::isfinite(recovered.authoredCamera.nearPlane)
            || !std::isfinite(recovered.authoredCamera.farPlane)
            || (recovered.authoredCamera.valid
                && (recovered.authoredCamera.fovXDeg <= 0.0f
                    || recovered.authoredCamera.fovXDeg >= 180.0f
                    || recovered.authoredCamera.orthoScale <= 0.0f
                    || recovered.authoredCamera.nearPlane
                        >= recovered.authoredCamera.farPlane
                    || (recovered.authoredCamera.projectionType
                            == app::ViewportCamera::ProjectionType::Perspective
                        && recovered.authoredCamera.nearPlane <= 0.0f))))
        {
            return false;
        }
        recovered.linkedToPrevious = line.at(13).toBool();
        recovered.linkedToNext = line.at(14).toBool();
        const auto hasNonzeroFiniteVector = [](const TVector& vector)
        {
            return std::isfinite(vector.x)
                && std::isfinite(vector.y)
                && std::isfinite(vector.z)
                && (vector.x != 0.0 || vector.y != 0.0
                    || vector.z != 0.0);
        };
        if (recovered.first.x == recovered.second.x
            && recovered.first.y == recovered.second.y
            && recovered.first.z == recovered.second.z)
        {
            return false;
        }
        if (!hasNonzeroFiniteVector(recovered.planeNormal)
            || !hasNonzeroFiniteVector(recovered.constructionPlaneNormal)
            || !hasNonzeroFiniteVector(recovered.angleReferenceAxis))
        {
            return false;
        }
        parsed.push_back(std::move(recovered));
    }
    for (std::size_t index = 0; index < parsed.size(); ++index)
    {
        const std::size_t previous = index == 0 ? parsed.size() - 1u : index - 1u;
        if (parsed[index].linkedToPrevious != parsed[previous].linkedToNext)
        {
            return false;
        }
    }
    *out = std::move(parsed);
    return true;
}

bool cleaveContextFromCbor(
    const QCborValue& value,
    GlView::RecoveryState::CleaveContext* out)
{
    if (!out || !value.isArray())
    {
        return false;
    }
    const QCborArray context = value.toArray();
    if (context.size() != 10
        || !context.at(0).isBool() || !context.at(1).isBool()
        || !context.at(4).isInteger() || !context.at(5).isInteger()
        || context.at(4).toInteger() < 0 || context.at(4).toInteger() > 1'000'000
        || context.at(5).toInteger() < 0 || context.at(5).toInteger() > 1'000'000
        || !context.at(2).isArray() || !context.at(3).isArray()
        || !cborInt32(context.at(6))
        || !cborInt32(context.at(7))
        || !context.at(8).isArray() || !context.at(9).isArray())
    {
        return false;
    }
    const QCborArray sectors = context.at(8).toArray();
    const QCborArray surfaces = context.at(9).toArray();
    if (sectors.size() > 1'000'000 || surfaces.size() > 1'000'000)
    {
        return false;
    }
    GlView::RecoveryState::CleaveContext parsed{};
    if (!matrixFromCbor(context.at(2), &parsed.projection)
        || !matrixFromCbor(context.at(3), &parsed.view)
        || !cborInt32(context.at(6), &parsed.sourceSurface.sector)
        || !cborInt32(context.at(7), &parsed.sourceSurface.surface))
    {
        return false;
    }
    parsed.valid = context.at(0).toBool();
    parsed.surfaceMode = context.at(1).toBool();
    parsed.viewportWidth = static_cast<int>(context.at(4).toInteger());
    parsed.viewportHeight = static_cast<int>(context.at(5).toInteger());
    parsed.targetSectors.reserve(static_cast<std::size_t>(sectors.size()));
    for (const QCborValue& sector : sectors)
    {
        std::int32_t recoveredSector = -1;
        if (!cborInt32(sector, &recoveredSector))
        {
            return false;
        }
        parsed.targetSectors.push_back(recoveredSector);
    }
    parsed.targetSurfaces.reserve(static_cast<std::size_t>(surfaces.size()));
    for (const QCborValue& surfaceValue : surfaces)
    {
        if (!surfaceValue.isArray())
        {
            return false;
        }
        const QCborArray surface = surfaceValue.toArray();
        SurfaceRef recovered{};
        if (surface.size() != 2
            || !cborInt32(surface.at(0), &recovered.sector)
            || !cborInt32(surface.at(1), &recovered.surface))
        {
            return false;
        }
        parsed.targetSurfaces.push_back(recovered);
    }
    *out = std::move(parsed);
    return true;
}

bool cleaveIndicesFromCbor(
    const QCborValue& value,
    const std::size_t lineCount,
    const bool requirePermutation,
    std::vector<std::int32_t>* out)
{
    if (!out || !value.isArray() || value.toArray().size() != lineCount)
    {
        return false;
    }
    const QCborArray encoded = value.toArray();
    std::vector<std::int32_t> parsed{};
    parsed.reserve(lineCount);
    std::vector<bool> seen(lineCount, false);
    for (const QCborValue& item : encoded)
    {
        std::int32_t index = -1;
        if (!cborInt32(item, &index) || index < 0
            || static_cast<std::size_t>(index) >= lineCount
            || seen[static_cast<std::size_t>(index)])
        {
            return false;
        }
        seen[static_cast<std::size_t>(index)] = true;
        parsed.push_back(index);
    }
    if (requirePermutation && parsed.size() != lineCount)
    {
        return false;
    }
    *out = std::move(parsed);
    return true;
}

bool cleaveOriginFromCbor(
    const QCborValue& value,
    const std::size_t lineCount,
    std::optional<GlView::RecoveryState::CleaveChainOrigin>* out)
{
    if (!out || !value.isArray())
    {
        return false;
    }
    const QCborArray encoded = value.toArray();
    qint64 lineIndex = -1;
    qint64 endpoint = -1;
    if (encoded.size() != 2 || lineCount == 0
        || !cborIntegerInRange(encoded.at(0), 0,
            static_cast<qint64>(lineCount - 1u), &lineIndex)
        || !cborIntegerInRange(encoded.at(1), 0, 1, &endpoint))
    {
        return false;
    }
    *out = GlView::RecoveryState::CleaveChainOrigin{
        static_cast<std::int32_t>(lineIndex), static_cast<std::int32_t>(endpoint) };
    return true;
}

bool cleaveSelectionFromCbor(
    const QCborValue& value,
    const std::size_t lineCount,
    std::vector<std::int32_t>* selected,
    std::int32_t* active)
{
    if (!selected || !active || !value.isMap())
    {
        return false;
    }
    const QCborMap encoded = value.toMap();
    const QCborValue linesValue = encoded.value(QStringLiteral("lines"));
    const QCborValue activeValue = encoded.value(QStringLiteral("active"));
    if (!linesValue.isArray() || !activeValue.isInteger()
        || static_cast<std::size_t>(linesValue.toArray().size()) > lineCount)
    {
        return false;
    }
    std::vector<std::int32_t> parsed{};
    const QCborArray lines = linesValue.toArray();
    parsed.reserve(static_cast<std::size_t>(lines.size()));
    std::vector<bool> seen(lineCount, false);
    for (const QCborValue& item : lines)
    {
        std::int32_t index = -1;
        if (!cborInt32(item, &index) || index < 0
            || static_cast<std::size_t>(index) >= lineCount
            || seen[static_cast<std::size_t>(index)])
        {
            return false;
        }
        seen[static_cast<std::size_t>(index)] = true;
        parsed.push_back(index);
    }
    if (!std::is_sorted(parsed.begin(), parsed.end()))
    {
        return false;
    }
    std::int32_t activeIndex = -1;
    if (!cborInt32(activeValue, &activeIndex) || activeIndex < -1
        || (activeIndex >= 0
            && (static_cast<std::size_t>(activeIndex) >= lineCount
                || !seen[static_cast<std::size_t>(activeIndex)])))
    {
        return false;
    }
    if (activeIndex == -1 && !parsed.empty())
    {
        return false;
    }
    *selected = std::move(parsed);
    *active = activeIndex;
    return true;
}

bool sameCleaveAuthoredDraftFields(
    const GlView::RecoveryState::CleaveDraftSnapshot& lhs,
    const std::span<const GlView::RecoveryState::CleaveLine> rhsLines,
    const GlView::RecoveryState::CleaveContext& rhsContext,
    const std::optional<GlView::RecoveryState::CleaveChainOrigin>& rhsOrigin,
    const std::span<const std::int32_t> rhsOrder)
{
    if (lhs.lines.size() != rhsLines.size()
        || !std::ranges::equal(lhs.applyOrder, rhsOrder)
        || lhs.chainOrigin.has_value() != rhsOrigin.has_value()
        || lhs.context.valid != rhsContext.valid
        || lhs.context.surfaceMode != rhsContext.surfaceMode
        || lhs.context.sourceSurface.sector != rhsContext.sourceSurface.sector
        || lhs.context.sourceSurface.surface != rhsContext.sourceSurface.surface
        || lhs.context.targetSectors != rhsContext.targetSectors
        || lhs.context.targetSurfaces.size() != rhsContext.targetSurfaces.size())
    {
        return false;
    }
    if (lhs.chainOrigin
        && (lhs.chainOrigin->lineIndex != rhsOrigin->lineIndex
            || lhs.chainOrigin->endpoint != rhsOrigin->endpoint))
    {
        return false;
    }
    for (std::size_t index = 0; index < lhs.lines.size(); ++index)
    {
        const auto& left = lhs.lines[index];
        const auto& right = rhsLines[index];
        if (left.first.x != right.first.x || left.first.y != right.first.y
            || left.first.z != right.first.z || left.second.x != right.second.x
            || left.second.y != right.second.y || left.second.z != right.second.z
            || left.planeNormal.x != right.planeNormal.x
            || left.planeNormal.y != right.planeNormal.y
            || left.planeNormal.z != right.planeNormal.z
            || left.constructionPlaneNormal.x != right.constructionPlaneNormal.x
            || left.constructionPlaneNormal.y != right.constructionPlaneNormal.y
            || left.constructionPlaneNormal.z != right.constructionPlaneNormal.z
            || left.angleReferenceAxis.x != right.angleReferenceAxis.x
            || left.angleReferenceAxis.y != right.angleReferenceAxis.y
            || left.angleReferenceAxis.z != right.angleReferenceAxis.z
            || left.linkedToPrevious != right.linkedToPrevious
            || left.linkedToNext != right.linkedToNext)
        {
            return false;
        }
    }
    for (std::size_t index = 0; index < lhs.context.targetSurfaces.size(); ++index)
    {
        if (lhs.context.targetSurfaces[index].sector
                != rhsContext.targetSurfaces[index].sector
            || lhs.context.targetSurfaces[index].surface
                != rhsContext.targetSurfaces[index].surface)
        {
            return false;
        }
    }
    return true;
}

bool sameCleaveAuthoredDraft(
    const GlView::RecoveryState::CleaveDraftSnapshot& lhs,
    const GlView::RecoveryState& rhs)
{
    return sameCleaveAuthoredDraftFields(
        lhs, rhs.cleaveLines, rhs.cleaveContext,
        rhs.cleaveChainOrigin, rhs.applyOrder);
}

bool addCleaveOwnedBytes(
    quint64* total,
    const quint64 count,
    const std::size_t elementSize)
{
    if (!total || elementSize == 0
        || count > std::numeric_limits<quint64>::max() / elementSize)
    {
        return false;
    }
    const quint64 bytes = count * static_cast<quint64>(elementSize);
    if (bytes > std::numeric_limits<quint64>::max() - *total)
    {
        return false;
    }
    *total += bytes;
    return true;
}

bool addCurrentCleaveOwnedBytes(
    quint64* total,
    const GlView::RecoveryState& state)
{
    return addCleaveOwnedBytes(total, state.cleaveLines.capacity(),
               sizeof(GlView::RecoveryState::CleaveLine))
        && addCleaveOwnedBytes(total, state.cleaveContext.targetSectors.capacity(),
               sizeof(std::int32_t))
        && addCleaveOwnedBytes(total, state.cleaveContext.targetSurfaces.capacity(),
               sizeof(SurfaceRef))
        && addCleaveOwnedBytes(total, state.applyOrder.capacity(), sizeof(std::int32_t))
        && addCleaveOwnedBytes(total, state.selectedCleaveLines.capacity(),
               sizeof(std::int32_t));
}

bool addCurrentCleaveOwnedBytes(
    quint64* total,
    const GlView::RecoveryState::CleaveDraftSnapshot& snapshot)
{
    return addCleaveOwnedBytes(total, snapshot.lines.capacity(),
               sizeof(GlView::RecoveryState::CleaveLine))
        && addCleaveOwnedBytes(total, snapshot.context.targetSectors.capacity(),
               sizeof(std::int32_t))
        && addCleaveOwnedBytes(total, snapshot.context.targetSurfaces.capacity(),
               sizeof(SurfaceRef))
        && addCleaveOwnedBytes(total, snapshot.applyOrder.capacity(), sizeof(std::int32_t))
        && addCleaveOwnedBytes(total, snapshot.selectedCleaveLines.capacity(),
               sizeof(std::int32_t));
}

bool cleaveDraftOwnedBytesFromCbor(const QCborValue& value, quint64* bytes)
{
    if (!bytes || !value.isMap())
    {
        return false;
    }
    const QCborMap draft = value.toMap();
    const QCborValue linesValue = draft.value(QStringLiteral("lines"));
    const QCborValue contextValue = draft.value(QStringLiteral("context"));
    const QCborValue orderValue = draft.value(QStringLiteral("applyOrder"));
    const QCborValue selectionValue = draft.value(QStringLiteral("selection"));
    if (!linesValue.isArray() || !contextValue.isArray()
        || !orderValue.isArray() || !selectionValue.isMap())
    {
        return false;
    }
    const QCborArray lines = linesValue.toArray();
    const QCborArray context = contextValue.toArray();
    const QCborMap selection = selectionValue.toMap();
    const QCborValue selectedLinesValue = selection.value(QStringLiteral("lines"));
    if (static_cast<std::size_t>(lines.size()) > kMaximumCleaveLines
        || context.size() != 10 || !context.at(8).isArray()
        || !context.at(9).isArray() || !selectedLinesValue.isArray()
        || orderValue.toArray().size() != lines.size()
        || selectedLinesValue.toArray().size() > lines.size())
    {
        return false;
    }
    const QCborArray targetSectors = context.at(8).toArray();
    const QCborArray targetSurfaces = context.at(9).toArray();
    quint64 estimate = 0;
    if (!addCleaveOwnedBytes(&estimate,
            static_cast<quint64>(lines.size()),
            sizeof(GlView::RecoveryState::CleaveLine))
        || !addCleaveOwnedBytes(&estimate,
            static_cast<quint64>(targetSectors.size()), sizeof(std::int32_t))
        || !addCleaveOwnedBytes(&estimate,
            static_cast<quint64>(targetSurfaces.size()), sizeof(SurfaceRef))
        || !addCleaveOwnedBytes(&estimate,
            static_cast<quint64>(orderValue.toArray().size()), sizeof(std::int32_t))
        || !addCleaveOwnedBytes(&estimate,
            static_cast<quint64>(selectedLinesValue.toArray().size()),
            sizeof(std::int32_t)))
    {
        return false;
    }
    *bytes = estimate;
    return true;
}

bool addRecoveredCleaveDraftCapacities(
    quint64* total,
    const GlView::RecoveryState::CleaveDraftSnapshot& snapshot)
{
    return addCleaveOwnedBytes(total, snapshot.lines.capacity(),
               sizeof(GlView::RecoveryState::CleaveLine))
        && addCleaveOwnedBytes(total, snapshot.context.targetSectors.capacity(),
               sizeof(std::int32_t))
        && addCleaveOwnedBytes(total, snapshot.context.targetSurfaces.capacity(),
               sizeof(SurfaceRef))
        && addCleaveOwnedBytes(total, snapshot.applyOrder.capacity(), sizeof(std::int32_t))
        && addCleaveOwnedBytes(total, snapshot.selectedCleaveLines.capacity(),
               sizeof(std::int32_t));
}

bool cleaveDraftFromCbor(
    const QCborValue& value,
    GlView::RecoveryState::CleaveDraftSnapshot* out)
{
    if (!out || !value.isMap())
    {
        return false;
    }
    const QCborMap encoded = value.toMap();
    GlView::RecoveryState::CleaveDraftSnapshot parsed{};
    if (!cleaveLinesFromCbor(encoded.value(QStringLiteral("lines")), &parsed.lines)
        || !cleaveContextFromCbor(encoded.value(QStringLiteral("context")), &parsed.context)
        || !cleaveIndicesFromCbor(encoded.value(QStringLiteral("applyOrder")),
            parsed.lines.size(), true, &parsed.applyOrder)
        || !cleaveSelectionFromCbor(encoded.value(QStringLiteral("selection")),
            parsed.lines.size(), &parsed.selectedCleaveLines,
            &parsed.selectedCleaveLine))
    {
        return false;
    }
    const QCborValue origin = encoded.value(QStringLiteral("chainOrigin"));
    if (!origin.isUndefined()
        && !cleaveOriginFromCbor(origin, parsed.lines.size(), &parsed.chainOrigin))
    {
        return false;
    }
    *out = std::move(parsed);
    return true;
}

template <typename CurrentDraft>
bool cleaveHistoryFromCbor(
    const QCborValue& value,
    const CurrentDraft& current,
    GlView::RecoveryState::CleaveHistory* out)
{
    if (!out || !value.isMap())
    {
        return false;
    }
    const QCborMap encoded = value.toMap();
    qint64 cursor = -1;
    const QCborValue snapshotsValue = encoded.value(QStringLiteral("snapshots"));
    if (!cborIntegerInRange(encoded.value(QStringLiteral("version")), 1, 1)
        || !encoded.value(QStringLiteral("cursor")).isInteger()
        || !snapshotsValue.isArray())
    {
        return false;
    }
    const QCborArray snapshots = snapshotsValue.toArray();
    if (snapshots.isEmpty()
        || static_cast<std::size_t>(snapshots.size()) > kMaximumCleaveHistorySnapshots
        || !cborIntegerInRange(encoded.value(QStringLiteral("cursor")), 0,
            static_cast<qint64>(snapshots.size() - 1), &cursor))
    {
        return false;
    }
    quint64 ownedBytes = 0;
    if (!addCurrentCleaveOwnedBytes(&ownedBytes, current)
        || !addCleaveOwnedBytes(&ownedBytes,
            static_cast<quint64>(snapshots.size()),
            sizeof(GlView::RecoveryState::CleaveDraftSnapshot)))
    {
        return false;
    }
    // Preflight every snapshot's count-derived owned storage before reserve or
    // any nested DTO allocation. Wire CBOR size is a separate container-level
    // limit and is already bounded by the complete EDIT payload validator.
    for (const QCborValue& snapshotValue : snapshots)
    {
        quint64 snapshotBytes = 0;
        if (!cleaveDraftOwnedBytesFromCbor(snapshotValue, &snapshotBytes)
            || snapshotBytes > kMaximumCleaveHistoryBytes
            || ownedBytes > kMaximumCleaveHistoryBytes - snapshotBytes)
        {
            return false;
        }
        ownedBytes += snapshotBytes;
    }
    if (ownedBytes > kMaximumCleaveHistoryBytes)
    {
        return false;
    }
    GlView::RecoveryState::CleaveHistory parsed{};
    parsed.cursor = static_cast<std::size_t>(cursor);
    try
    {
        parsed.snapshots.reserve(static_cast<std::size_t>(snapshots.size()));
        for (qsizetype index = 0; index < snapshots.size(); ++index)
        {
            GlView::RecoveryState::CleaveDraftSnapshot snapshot{};
            if (!cleaveDraftFromCbor(snapshots.at(index), &snapshot))
            {
                return false;
            }
            parsed.snapshots.push_back(std::move(snapshot));
        }
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    catch (const std::length_error&)
    {
        return false;
    }
    quint64 actualOwnedBytes = 0;
    if (!addCurrentCleaveOwnedBytes(&actualOwnedBytes, current)
        || !addCleaveOwnedBytes(&actualOwnedBytes, parsed.snapshots.capacity(),
            sizeof(GlView::RecoveryState::CleaveDraftSnapshot)))
    {
        return false;
    }
    for (const GlView::RecoveryState::CleaveDraftSnapshot& snapshot
         : parsed.snapshots)
    {
        if (!addRecoveredCleaveDraftCapacities(&actualOwnedBytes, snapshot))
        {
            return false;
        }
    }
    if (actualOwnedBytes > kMaximumCleaveHistoryBytes
        || !sameCleaveAuthoredDraft(parsed.snapshots[parsed.cursor], current))
    {
        return false;
    }
    *out = std::move(parsed);
    return true;
}

bool cleaveContextReferencesValid(
    const GlView::RecoveryState::CleaveContext& context,
    const Level& level)
{
    const auto sectorIsValid = [&level](const std::int32_t sector)
    {
        return sector >= 0
            && static_cast<std::size_t>(sector) < level.sectors.size();
    };
    const auto surfaceIsValid = [&level, &sectorIsValid](const SurfaceRef& ref)
    {
        return sectorIsValid(ref.sector)
            && ref.surface >= 0
            && static_cast<std::size_t>(ref.surface)
                < level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size();
    };

    if (context.surfaceMode)
    {
        if (!surfaceIsValid(context.sourceSurface))
        {
            return false;
        }
    }
    else if (context.sourceSurface != SurfaceRef{ -1, -1 })
    {
        return false;
    }

    if (!std::is_sorted(context.targetSectors.begin(), context.targetSectors.end())
        || std::adjacent_find(context.targetSectors.begin(),
            context.targetSectors.end()) != context.targetSectors.end())
    {
        return false;
    }
    for (const std::int32_t sector : context.targetSectors)
    {
        if (!sectorIsValid(sector))
        {
            return false;
        }
    }

    const auto surfaceLess = [](const SurfaceRef& lhs, const SurfaceRef& rhs)
    {
        return lhs.sector == rhs.sector
            ? lhs.surface < rhs.surface
            : lhs.sector < rhs.sector;
    };
    if (!std::is_sorted(context.targetSurfaces.begin(),
            context.targetSurfaces.end(), surfaceLess))
    {
        return false;
    }
    for (std::size_t index = 0; index < context.targetSurfaces.size(); ++index)
    {
        if (!surfaceIsValid(context.targetSurfaces[index])
            || (index != 0
                && context.targetSurfaces[index - 1]
                    == context.targetSurfaces[index]))
        {
            return false;
        }
    }
    return true;
}

bool cleaveWorkspaceReferencesValid(
    const GlView::RecoveryState& state,
    const Level& level)
{
    if (!cleaveContextReferencesValid(state.cleaveContext, level))
    {
        return false;
    }
    if (!state.cleaveHistory)
    {
        return true;
    }
    return std::all_of(
        state.cleaveHistory->snapshots.begin(),
        state.cleaveHistory->snapshots.end(),
        [&level](const GlView::RecoveryState::CleaveDraftSnapshot& snapshot)
        {
            return cleaveContextReferencesValid(snapshot.context, level);
        });
}

bool addEncodedCleaveWorkspaceBytes(const QCborMap& workspace, quint64* total)
{
    const QCborValue lines = workspace.value(QStringLiteral("lines"));
    const QCborValue context = workspace.value(QStringLiteral("context"));
    const QCborValue order = workspace.value(QStringLiteral("applyOrder"));
    const QCborValue selection = workspace.value(QStringLiteral("selection"));
    if ( !total || !lines.isArray() || !context.isArray()
        || context.toArray().size() != 10 || static_cast<std::size_t>(lines.toArray().size()) > kMaximumCleaveLines
        || !context.toArray().at(8).isArray() || !context.toArray().at(9).isArray()
        || (!order.isUndefined() && !order.isArray())
        || (!selection.isUndefined() && (!selection.isMap()
            || !selection.toMap().value(QStringLiteral("lines")).isArray())) )
    {
        return false;
    }
    const auto lineCount = static_cast<quint64>(lines.toArray().size());
    const auto selectionCount = selection.isUndefined() ? (lineCount == 0u ? 0u : 1u)
        : static_cast<quint64>(selection.toMap().value(QStringLiteral("lines")).toArray().size());
    if ( !addCleaveOwnedBytes(total, lineCount, sizeof(GlView::RecoveryState::CleaveLine))
        || !addCleaveOwnedBytes(total, context.toArray().at(8).toArray().size(), sizeof(std::int32_t))
        || !addCleaveOwnedBytes(total, context.toArray().at(9).toArray().size(), sizeof(SurfaceRef))
        || !addCleaveOwnedBytes(total, order.isUndefined() ? lineCount : order.toArray().size(), sizeof(std::int32_t))
        || !addCleaveOwnedBytes(total, selectionCount, sizeof(std::int32_t)) )
    {
        return false;
    }
    const QCborValue history = workspace.value(QStringLiteral("history"));
    if ( !history.isUndefined() )
    {
        if ( !history.isMap() ) return false;
        const QCborValue snapshotsValue = history.toMap().value(QStringLiteral("snapshots"));
        if ( !snapshotsValue.isArray() || snapshotsValue.toArray().isEmpty()
            || static_cast<std::size_t>(snapshotsValue.toArray().size()) > kMaximumCleaveHistorySnapshots ) return false;
        const QCborArray snapshots = snapshotsValue.toArray();
        if ( !addCleaveOwnedBytes(total, snapshots.size(),
            sizeof(GlView::RecoveryState::CleaveDraftSnapshot)) ) return false;
        for ( const QCborValue& snapshot : snapshots )
        {
            quint64 bytes = 0;
            if ( !cleaveDraftOwnedBytesFromCbor(snapshot, &bytes)
                || !addCleaveOwnedBytes(total, bytes, 1u) ) return false;
        }
    }
    return *total <= kMaximumCleaveHistoryBytes;
}

bool cleaveSingleWorkspaceFromCbor(
    const QCborMap& cleave, GlView::RecoveryState* out)
{
    const QCborValue linesValue = cleave.value(QStringLiteral("lines"));
    const QCborValue contextValue = cleave.value(QStringLiteral("context"));
    const QCborValue optionsValue = cleave.value(QStringLiteral("options"));
    const QString boundedKey = QStringLiteral("bounded");
    const bool hasBoundedValue = cleave.contains(boundedKey);
    const QCborValue boundedValue = cleave.value(boundedKey);
    if (!linesValue.isArray() || !contextValue.isArray() || !optionsValue.isArray())
    {
        return false;
    }
    if ( hasBoundedValue && !boundedValue.isBool() )
    {
        return false;
    }
    const QCborArray options = optionsValue.toArray();
    if (options.size() != 8
        || !cleaveLinesFromCbor(linesValue, &out->cleaveLines)
        || !cleaveContextFromCbor(contextValue, &out->cleaveContext))
    {
        return false;
    }
    out->cleaveShapeOptions = {};
    out->cleaveTransformMode = GlView::StagedCleaveTransformMode::Move;
    out->cleaveChainOrigin.reset();
    if ( hasBoundedValue )
    {
        out->cleaveBounded = boundedValue.toBool();
    }
    else if ( !out->cleaveLines.empty() )
    {
        // Legacy staged arrangements used the global-plane behavior.
        out->cleaveBounded = false;
    }
    const QString chainOriginKey = QStringLiteral("chainOrigin");
    if ( cleave.contains(chainOriginKey) )
    {
        const QCborValue chainOriginValue = cleave.value(chainOriginKey);
        if ( !chainOriginValue.isArray() )
        {
            return false;
        }
        if (!cleaveOriginFromCbor(
                chainOriginValue, out->cleaveLines.size(),
                &out->cleaveChainOrigin))
        {
            return false;
        }
    }
    if (!options.at(0).isBool() || !options.at(1).isBool()
        || !options.at(2).isBool() || !options.at(6).isBool()
        || !options.at(7).isBool())
    {
        return false;
    }
    out->cleaveStagedMode = options.at(0).toBool();
    out->cleaveChainEnabled = options.at(1).toBool();
    out->cleaveMeasurementsVisible = options.at(2).toBool();
    // Legacy editor snapshots used one visibility choice for both
    // measurement guides.  A missing named guide field follows that
    // saved choice; line numbers are new and remain off by default.
    out->cleaveAnglesVisible = out->cleaveMeasurementsVisible;
    out->cleaveLengthsVisible = out->cleaveMeasurementsVisible;
    out->cleaveLineNumbersVisible = false;
    const QCborValue guidesValue = cleave.value(QStringLiteral("guides"));
    if (!guidesValue.isUndefined())
    {
        if (!guidesValue.isMap())
        {
            return false;
        }
        const QCborMap guides = guidesValue.toMap();
        const auto readGuide = [&guides](const QString& key, bool* destination)
        {
            if (!destination || !guides.contains(key))
            {
                return destination != nullptr;
            }
            const QCborValue value = guides.value(key);
            if (!value.isBool())
            {
                return false;
            }
            *destination = value.toBool();
            return true;
        };
        // Unknown names are ignored so later editor builds can extend
        // this presentation-only map without invalidating older EDIT.
        if (!readGuide(QStringLiteral("angles"), &out->cleaveAnglesVisible)
            || !readGuide(QStringLiteral("lengths"), &out->cleaveLengthsVisible)
            || !readGuide(QStringLiteral("numbers"), &out->cleaveLineNumbersVisible))
        {
            return false;
        }
    }
    qint64 unit = -1;
    if (!cborIntegerInRange(options.at(3), 0,
            static_cast<qint64>(GlView::CleaveMeasureUnit::None), &unit)) return false;
    const auto decodedUnit = static_cast<GlView::CleaveMeasureUnit>(unit);
    const bool legacyNoneUnit = decodedUnit == GlView::CleaveMeasureUnit::None;
    out->cleaveMeasurementsVisible = true;
    out->cleaveMeasureUnit = legacyNoneUnit
        ? GlView::CleaveMeasureUnit::Meters
        : decodedUnit;
    if (legacyNoneUnit)
    {
        // Normalize the removed None choice only after the independent
        // guide map was decoded. Named guide fields remain authoritative
        // when a real unit is present; options[2] supplied their legacy
        // fallback earlier when a named field was absent.
        out->cleaveLengthsVisible = false;
    }
    double cleaveAngle = 0.0;
    std::int32_t selectedCleaveLine = -1;
    if (!cborNumberInRange(options.at(4), 0.0, 360.0, &cleaveAngle)
        || !cborInt32(options.at(5), &selectedCleaveLine)
        || selectedCleaveLine < -1
        || selectedCleaveLine >= static_cast<std::int32_t>(out->cleaveLines.size()))
    {
        return false;
    }
    out->cleaveAngleStepDegrees = cleaveAngle;
    out->selectedCleaveLine = selectedCleaveLine;
    out->sectorCleaveToolEnabled = options.at(6).toBool();
    out->surfaceCleaveToolEnabled = options.at(7).toBool();

    out->applyOrder.clear();
    const QCborValue applyOrderValue = cleave.value(QStringLiteral("applyOrder"));
    if (applyOrderValue.isUndefined())
    {
        out->applyOrder.resize(out->cleaveLines.size());
        std::iota(out->applyOrder.begin(), out->applyOrder.end(), 0);
    }
    else if (!cleaveIndicesFromCbor(
                 applyOrderValue, out->cleaveLines.size(), true,
                 &out->applyOrder))
    {
        return false;
    }

    const QCborValue selectionValue = cleave.value(QStringLiteral("selection"));
    if (selectionValue.isUndefined())
    {
        out->selectedCleaveLines.clear();
        if (out->selectedCleaveLine >= 0)
        {
            out->selectedCleaveLines.push_back(out->selectedCleaveLine);
        }
    }
    else
    {
        std::vector<std::int32_t> selected{};
        std::int32_t active = -1;
        if (!cleaveSelectionFromCbor(
                selectionValue, out->cleaveLines.size(), &selected, &active)
            || active != out->selectedCleaveLine)
        {
            return false;
        }
        out->selectedCleaveLines = std::move(selected);
    }

    const QCborValue transformModeValue = cleave.value(
        QStringLiteral("transformMode"));
    if (!transformModeValue.isUndefined())
    {
        qint64 transformMode = -1;
        if (!cborIntegerInRange(transformModeValue, 0, 2, &transformMode))
        {
            return false;
        }
        out->cleaveTransformMode =
            static_cast<GlView::StagedCleaveTransformMode>(transformMode);
    }

    const QCborValue shapeValue = cleave.value(QStringLiteral("shape"));
    if (!shapeValue.isUndefined())
    {
        if (!shapeValue.isMap())
        {
            return false;
        }
        const QCborMap shape = shapeValue.toMap();
        qint64 shapeKind = -1;
        qint64 segments = -1;
        qint64 points = -1;
        double width = 0.0;
        double height = 0.0;
        const QCborValue dimensionsLinked = shape.value(
            QStringLiteral("dimensionsLinked"));
        const QCborValue placementArmed = shape.value(
            QStringLiteral("placementArmed"));
        const QCborValue cameraPlane = shape.value(QStringLiteral("cameraPlane"));
        const QCborValue rotation = shape.value(QStringLiteral("rotationDegrees"));
        double rotationDegrees = 0.0;
        if (!cborIntegerInRange(shape.value(QStringLiteral("version")), 1, 1)
            || !cborIntegerInRange(shape.value(QStringLiteral("kind")), 0, 4,
                &shapeKind)
            || !cborNumberInRange(shape.value(QStringLiteral("width")),
                0.005, 10'000.0, &width)
            || !cborNumberInRange(shape.value(QStringLiteral("height")),
                0.005, 10'000.0, &height)
            || !cborIntegerInRange(shape.value(QStringLiteral("segments")),
                3, 64, &segments)
            || !cborIntegerInRange(shape.value(QStringLiteral("points")),
                3, 64, &points)
            || !dimensionsLinked.isBool() || !placementArmed.isBool()
            || (!cameraPlane.isUndefined() && !cameraPlane.isBool())
            || (!rotation.isUndefined() && !cborFiniteNumber(rotation, &rotationDegrees)))
        {
            return false;
        }
        out->cleaveShapeOptions.kind =
            static_cast<ui::viewport::CleaveShapeKind>(shapeKind);
        out->cleaveShapeOptions.width = width;
        out->cleaveShapeOptions.height = height;
        out->cleaveShapeOptions.dimensionsLinked = dimensionsLinked.toBool();
        out->cleaveShapeOptions.segments = static_cast<int>(segments);
        out->cleaveShapeOptions.points = static_cast<int>(points);
        out->cleaveShapeOptions.placementArmed = placementArmed.toBool();
        out->cleaveShapeOptions.cameraPlane = cameraPlane.isBool() && cameraPlane.toBool();
        rotationDegrees = std::fmod(rotationDegrees, 360.0);
        if ( rotationDegrees >= 180.0 ) rotationDegrees -= 360.0;
        if ( rotationDegrees < -180.0 ) rotationDegrees += 360.0;
        out->cleaveShapeOptions.rotationDegrees = rotationDegrees == 0.0 ? 0.0 : rotationDegrees;
        if (!ui::viewport::makeCleaveShapePlan(
                ui::viewport::CleaveShapeSpec{
                    out->cleaveShapeOptions.kind,
                    out->cleaveShapeOptions.width,
                    out->cleaveShapeOptions.height,
                    out->cleaveShapeOptions.dimensionsLinked,
                    out->cleaveShapeOptions.segments,
                    out->cleaveShapeOptions.points }))
        {
            return false;
        }
    }

    out->cleaveHistory.reset();
    const QCborValue historyValue = cleave.value(QStringLiteral("history"));
    if (!historyValue.isUndefined())
    {
        GlView::RecoveryState::CleaveHistory history{};
        if (!cleaveHistoryFromCbor(historyValue, *out, &history))
        {
            return false;
        }
        out->cleaveHistory = std::move(history);
    }
    return true;
}

GlView::RecoveryState::CleaveWorkspace takeRecoveredCleaveWorkspace(
    GlView::RecoveryState& state)
{
    GlView::RecoveryState::CleaveWorkspace workspace{};
    workspace.draft = { std::move(state.cleaveLines), std::move(state.cleaveContext),
        state.cleaveChainOrigin, std::move(state.applyOrder),
        std::move(state.selectedCleaveLines), state.selectedCleaveLine };
    workspace.history = std::move(state.cleaveHistory);
    workspace.bounded = state.cleaveBounded;
    workspace.stagedMode = state.cleaveStagedMode;
    workspace.chainEnabled = state.cleaveChainEnabled;
    workspace.measurementsVisible = state.cleaveMeasurementsVisible;
    workspace.measureUnit = state.cleaveMeasureUnit;
    workspace.anglesVisible = state.cleaveAnglesVisible;
    workspace.lengthsVisible = state.cleaveLengthsVisible;
    workspace.lineNumbersVisible = state.cleaveLineNumbersVisible;
    workspace.angleStepDegrees = state.cleaveAngleStepDegrees;
    workspace.shapeOptions = state.cleaveShapeOptions;
    workspace.transformMode = state.cleaveTransformMode;
    return workspace;
}

bool cleaveFamilyMatches(
    const GlView::RecoveryState::CleaveContext& context,
    const std::optional<GlView::RecoveryState::CleaveHistory>& history,
    const GlView::CleaveWorkspaceFamily family)
{
    const bool surface = family == GlView::CleaveWorkspaceFamily::Surface;
    const auto matches = [surface](const auto& candidate)
    {
        return !candidate.valid || candidate.surfaceMode == surface;
    };
    return matches(context) && (!history || std::all_of(
        history->snapshots.begin(), history->snapshots.end(),
        [&matches](const auto& snapshot) { return matches(snapshot.context); }));
}

QCborArray viewportToCbor(const GlView::ViewportState& state)
{
    // The trailing map is the additive viewport-state extension point. Keep
    // preceding positional fields stable; new named flags belong here and do
    // not require a native SED version change.
    QCborMap viewportFlags{};
    viewportFlags.insert(
        QStringLiteral("thingCollisionShapes"),
        state.showThingCollisionShapes);
    viewportFlags.insert(
        QStringLiteral("thingVisualBounds"),
        state.showThingVisualBounds);
    viewportFlags.insert(
        QStringLiteral("thingModelLodEnabled"),
        state.thingModelLodEnabled);
    return QCborArray{
        orbitToCbor(state.orbit),
        static_cast<int>(state.projectionType),
        state.fovXDeg,
        state.orthoScale,
        state.clipEnabled,
        state.showWireframe,
        state.showVertices,
        state.showSolid,
        state.showTextures,
        state.showFog,
        state.showThings,
        state.showHiddenThings,
        state.showThingGizmos,
        static_cast<int>(state.thingViewMode),
        static_cast<int>(state.lightingMode),
        state.initialized,
        state.showArchThingGizmos,
        viewportFlags,
    };
}

bool viewportFromCbor(
    const QCborValue& value,
    GlView::ViewportState* out,
    const bool previewPipeline)
{
    if (!out || !value.isArray())
    {
        return false;
    }
    const QCborArray array = value.toArray();
    if ((array.size() != 16 && array.size() != 17 && array.size() != 18)
        || !orbitFromCbor(array.at(0), &out->orbit))
    {
        return false;
    }
    // An older workspace without this opt-in flag must keep LOD0 even when
    // the user's current application default enables model LOD.
    out->thingModelLodEnabled = false;
    if (array.size() == 18)
    {
        if (!array.at(17).isMap())
        {
            return false;
        }
        const QCborMap viewportFlags = array.at(17).toMap();
        const auto readOptionalBoolean = [&viewportFlags](
            const QString& key,
            bool* destination)
        {
            if (!destination)
            {
                return false;
            }
            if (!viewportFlags.contains(key))
            {
                return true;
            }
            const QCborValue field = viewportFlags.value(key);
            if (!field.isBool())
            {
                return false;
            }
            *destination = field.toBool();
            return true;
        };
        // Unknown text keys are intentionally ignored so newer builds can
        // add named flags without invalidating this EDIT schema.
        if (!readOptionalBoolean(
                QStringLiteral("thingCollisionShapes"),
                &out->showThingCollisionShapes)
            || !readOptionalBoolean(
                QStringLiteral("thingVisualBounds"),
                &out->showThingVisualBounds)
            || !readOptionalBoolean(
                QStringLiteral("thingModelLodEnabled"),
                &out->thingModelLodEnabled))
        {
            return false;
        }
    }
    qint64 projection = -1;
    qint64 thingMode = -1;
    qint64 lighting = -1;
    double fovXDeg = 0.0;
    double orthoScale = 0.0;
    if (!cborIntegerInRange(array.at(1), 0, 1, &projection)
        || !cborIntegerInRange(array.at(13), 0,
            static_cast<qint64>(GlView::ThingViewMode::Vertices), &thingMode)
        || !cborIntegerInRange(array.at(14), 0,
            static_cast<qint64>(GlView::LightingMode::Gouraud), &lighting)
        || !array.at(4).isBool() || !array.at(5).isBool()
        || !array.at(6).isBool() || !array.at(7).isBool()
        || !array.at(8).isBool() || !array.at(9).isBool()
        || !array.at(10).isBool() || !array.at(11).isBool()
        || !array.at(12).isBool() || !array.at(15).isBool()
        || (array.size() >= 17 && !array.at(16).isBool())
        || !cborNumberInRange(array.at(2), -1.0e12, 1.0e12, &fovXDeg)
        || !cborNumberInRange(array.at(3), -1.0e12, 1.0e12, &orthoScale))
    {
        return false;
    }
    out->projectionType = static_cast<app::ViewportCamera::ProjectionType>(projection);
    out->showArchThingGizmos = array.size() >= 17
        ? array.at(16).toBool()
        : !previewPipeline;
    out->fovXDeg = static_cast<float>(fovXDeg);
    out->orthoScale = static_cast<float>(orthoScale);
    if (!std::isfinite(out->fovXDeg) || out->fovXDeg <= 0.0f
        || out->fovXDeg >= 180.0f
        || !std::isfinite(out->orthoScale) || out->orthoScale <= 0.0f
        || out->orthoScale > 1.0e9f)
    {
        return false;
    }
    out->clipEnabled = array.at(4).toBool();
    out->showWireframe = array.at(5).toBool();
    out->showVertices = array.at(6).toBool();
    out->showSolid = array.at(7).toBool();
    out->showTextures = array.at(8).toBool();
    out->showFog = array.at(9).toBool();
    out->showThings = array.at(10).toBool();
    out->showHiddenThings = array.at(11).toBool();
    out->showThingGizmos = array.at(12).toBool();
    out->thingViewMode = static_cast<GlView::ThingViewMode>(thingMode);
    out->lightingMode = static_cast<GlView::LightingMode>(lighting);
    out->initialized = array.at(15).toBool();
    return true;
}

QCborMap cleaveWorkspaceToCbor(
    const GlView::RecoveryState::CleaveDraftView& draft,
    const std::optional<GlView::RecoveryState::CleaveHistory>& history,
    const GlView::RecoveryState::CleaveWorkspace& options,
    const GlView::CleaveWorkspaceFamily family,
    const bool sectorToolEnabled, const bool surfaceToolEnabled)
{
    QCborMap cleave{};
    cleave.insert(QStringLiteral("lines"), cleaveLinesToCbor(draft.lines));
    if ( draft.chainOrigin
        && draft.chainOrigin->lineIndex >= 0
        && static_cast<std::size_t>(draft.chainOrigin->lineIndex)
            < draft.lines.size()
        && (draft.chainOrigin->endpoint == 0
            || draft.chainOrigin->endpoint == 1) )
    {
        cleave.insert(QStringLiteral("chainOrigin"), QCborArray{
            draft.chainOrigin->lineIndex,
            draft.chainOrigin->endpoint });
    }
    if ( options.bounded.has_value() )
    {
        cleave.insert(QStringLiteral("bounded"), *options.bounded);
    }
    cleave.insert(QStringLiteral("context"), cleaveContextToCbor(*draft.context));
    cleave.insert(QStringLiteral("options"), QCborArray{
        options.stagedMode, options.chainEnabled,
        options.measurementsVisible, static_cast<int>(options.measureUnit),
        options.angleStepDegrees, draft.selectedCleaveLine,
        sectorToolEnabled, surfaceToolEnabled });
    cleave.insert(QStringLiteral("guides"), QCborMap{
        { QStringLiteral("angles"), options.anglesVisible },
        { QStringLiteral("lengths"), options.lengthsVisible },
        { QStringLiteral("numbers"), options.lineNumbersVisible },
    });
    QCborArray applyOrder{};
    for (const std::int32_t index : draft.applyOrder)
    {
        applyOrder.append(index);
    }
    cleave.insert(QStringLiteral("applyOrder"), applyOrder);
    QCborArray selectedCleaveLines{};
    for (const std::int32_t index : draft.selectedCleaveLines)
    {
        selectedCleaveLines.append(index);
    }
    cleave.insert(QStringLiteral("selection"), QCborMap{
        { QStringLiteral("lines"), selectedCleaveLines },
        { QStringLiteral("active"), draft.selectedCleaveLine },
    });
    cleave.insert(QStringLiteral("transformMode"),
        static_cast<int>(options.transformMode));
    const GlView::RecoveryState::CleaveShapeOptions& shape = options.shapeOptions;
    cleave.insert(QStringLiteral("shape"), QCborMap{
        { QStringLiteral("version"), 1 },
        { QStringLiteral("kind"), static_cast<int>(shape.kind) },
        { QStringLiteral("width"), shape.width },
        { QStringLiteral("height"), shape.height },
        { QStringLiteral("dimensionsLinked"), shape.dimensionsLinked },
        { QStringLiteral("segments"), shape.segments },
        { QStringLiteral("points"), shape.points },
        { QStringLiteral("placementArmed"), shape.placementArmed },
        { QStringLiteral("cameraPlane"), shape.cameraPlane },
        { QStringLiteral("rotationDegrees"), shape.rotationDegrees },
    });
    if (history)
    {
        QCborArray snapshots{};
        for (const GlView::RecoveryState::CleaveDraftSnapshot& snapshot
             : history->snapshots)
        {
            QCborMap encodedSnapshot = cleaveDraftToCbor(snapshot);
            snapshots.append(std::move(encodedSnapshot));
        }
        cleave.insert(QStringLiteral("history"), QCborMap{
            { QStringLiteral("version"), 1 },
            { QStringLiteral("cursor"), static_cast<qint64>(history->cursor) },
            { QStringLiteral("snapshots"), snapshots },
        });
    }

    cleave.insert(QStringLiteral("currentFamily"), static_cast<int>(family));
    return cleave;
}

QCborArray glRecoveryToCbor(const GlView::RecoveryState& state)
{
    quint64 ownedBytes = 0;
    if ( !addCurrentCleaveOwnedBytes(&ownedBytes, state) )
    {
        throw std::length_error("The Cleave workspace exceeds its owned-storage limit.");
    }
    const auto accountHistory = [&ownedBytes](const auto& history,
        const GlView::RecoveryState::CleaveDraftView& draft)
    {
        if ( !history ) return;
        if ( history->snapshots.empty()
            || history->snapshots.size() > kMaximumCleaveHistorySnapshots
            || history->cursor >= history->snapshots.size()
            || !sameCleaveAuthoredDraftFields(history->snapshots[history->cursor],
                draft.lines, *draft.context, draft.chainOrigin, draft.applyOrder)
            || !addCleaveOwnedBytes(&ownedBytes, history->snapshots.capacity(),
                sizeof(GlView::RecoveryState::CleaveDraftSnapshot)) )
        {
            throw std::length_error("The Cleave timeline is invalid or exceeds its persistence limits.");
        }
        for ( const auto& snapshot : history->snapshots )
        {
            if ( !addRecoveredCleaveDraftCapacities(&ownedBytes, snapshot) )
            {
                throw std::length_error("The Cleave timeline exceeds its owned-storage limit.");
            }
        }
    };
    accountHistory(state.cleaveHistory,
        { state.cleaveLines, &state.cleaveContext, state.cleaveChainOrigin,
            state.applyOrder, state.selectedCleaveLines, state.selectedCleaveLine });
    if ( state.cleavePeerWorkspace )
    {
        const auto& peer = *state.cleavePeerWorkspace;
        if ( !addCurrentCleaveOwnedBytes(&ownedBytes, peer.draft) )
        {
            throw std::length_error("The dormant Cleave workspace exceeds its owned-storage limit.");
        }
        accountHistory(peer.history,
            { peer.draft.lines, &peer.draft.context, peer.draft.chainOrigin,
                peer.draft.applyOrder, peer.draft.selectedCleaveLines, peer.draft.selectedCleaveLine });
    }
    if ( ownedBytes > kMaximumCleaveHistoryBytes )
    {
        throw std::length_error("The independent Cleave workspaces exceed 16 MiB of owned storage.");
    }

    QCborValue gridCenter{};
    if (state.gridCenterOverride)
    {
        gridCenter = vectorToCbor(*state.gridCenterOverride);
    }
    QCborArray categories{};
    for (const bool value : state.alwaysShowLightGizmoCategories)
    {
        categories.append(value);
    }
    GlView::RecoveryState::CleaveWorkspace options{};
    options.bounded = state.cleaveBounded;
    options.stagedMode = state.cleaveStagedMode;
    options.chainEnabled = state.cleaveChainEnabled;
    options.measurementsVisible = state.cleaveMeasurementsVisible;
    options.measureUnit = state.cleaveMeasureUnit;
    options.anglesVisible = state.cleaveAnglesVisible;
    options.lengthsVisible = state.cleaveLengthsVisible;
    options.lineNumbersVisible = state.cleaveLineNumbersVisible;
    options.angleStepDegrees = state.cleaveAngleStepDegrees;
    options.shapeOptions = state.cleaveShapeOptions;
    options.transformMode = state.cleaveTransformMode;
    QCborMap cleave = cleaveWorkspaceToCbor(
        { state.cleaveLines, &state.cleaveContext, state.cleaveChainOrigin,
            state.applyOrder, state.selectedCleaveLines, state.selectedCleaveLine },
        state.cleaveHistory, options, state.cleaveWorkspaceFamily,
        state.sectorCleaveToolEnabled, state.surfaceCleaveToolEnabled);
    const auto peerFamily = state.cleaveWorkspaceFamily == GlView::CleaveWorkspaceFamily::Sector
        ? GlView::CleaveWorkspaceFamily::Surface : GlView::CleaveWorkspaceFamily::Sector;
    const auto emptyPeer = GlView::defaultStagedCleaveWorkspaceOptions();
    const auto& peer = state.cleavePeerWorkspace ? *state.cleavePeerWorkspace : emptyPeer;
    const auto& peerDraft = peer.draft;
    cleave.insert(QStringLiteral("peerWorkspace"), cleaveWorkspaceToCbor(
        { peerDraft.lines, &peerDraft.context, peerDraft.chainOrigin, peerDraft.applyOrder,
            peerDraft.selectedCleaveLines, peerDraft.selectedCleaveLine },
        peer.history, peer, peerFamily, false, false));

    return QCborArray{
        viewportToCbor(state.editor), viewportToCbor(state.preview),
        static_cast<int>(state.pipeline), state.nearPlane, state.farPlane,
        state.showGrid, state.gridFollowCamera, state.gridRotateWithView,
        static_cast<int>(state.gridPlane), state.gridMajorSpacing, state.gridMinorSpacing,
        state.gridAdaptiveSpacing, state.gridOpacity,
        static_cast<qint64>(state.gridColor.rgba()),
        static_cast<qint64>(state.gridMajorColor.rgba()), state.gridLineWidthPx,
        state.gridVisibleThroughWalls, state.gridAlwaysBehind,
        state.gridResetOnCameraRotate, state.gridAlignToView, gridCenter,
        vectorToCbor(state.gridAlignAxisA), vectorToCbor(state.gridAlignAxisB),
        state.gridOverrideCamBasisValid, vectorToCbor(state.gridOverrideCamForward),
        vectorToCbor(state.gridOverrideCamUp), static_cast<int>(state.dynamicLightShading),
        state.showEditorCubes, state.showPreviewCubes, state.previewRespectHiddenLayers,
        state.xraySelectionCycling, state.orbitAroundSelection,
        state.showSectorThrustVectors, categories,
        static_cast<int>(state.transformTool), static_cast<int>(state.transformOrientation),
        state.transformGridSnap, state.transformGridPlaneStick, cleave,
        state.gridKeepViewAlignedVisible,
    };
}

bool glRecoveryFromCbor(
    const QCborValue& value,
    GlView::RecoveryState* out,
    const Level& currentLevel)
{
    if (!out || !value.isArray())
    {
        return false;
    }
    const QCborArray array = value.toArray();
    if ((array.size() != 39 && array.size() != 40)
        || !viewportFromCbor(array.at(0), &out->editor, false)
        || !viewportFromCbor(array.at(1), &out->preview, true)
        || !array.at(38).isMap()
        || (array.size() == 40 && !array.at(39).isBool()))
    {
        return false;
    }
    qint64 pipeline = -1;
    qint64 plane = -1;
    qint64 shading = -1;
    qint64 tool = -1;
    qint64 orientation = -1;
    if (!cborIntegerInRange(array.at(2), 0, 1, &pipeline)
        || !cborIntegerInRange(array.at(8), 0, 2, &plane)
        || !cborIntegerInRange(array.at(26), 0, 1, &shading)
        || !cborIntegerInRange(array.at(34), 0, 3, &tool)
        || !cborIntegerInRange(array.at(35), 0, 1, &orientation))
    {
        return false;
    }
    double nearPlane = 0.0;
    double farPlane = 0.0;
    double gridMajorSpacing = 0.0;
    double gridMinorSpacing = 0.0;
    double gridOpacity = 0.0;
    double gridLineWidth = 0.0;
    const app::ViewportCamera::ProjectionType activeProjection = pipeline == 0
        ? out->editor.projectionType : out->preview.projectionType;
    if (!cborNumberInRange(array.at(3), -1.0e9, 1.0e9, &nearPlane)
        || !cborNumberInRange(array.at(4), -1.0e9, 1.0e9, &farPlane)
        || nearPlane >= farPlane
        || (activeProjection == app::ViewportCamera::ProjectionType::Perspective
            && nearPlane <= 0.0)
        || !cborNumberInRange(array.at(9), 0.000001, 1.0e9, &gridMajorSpacing)
        || !cborNumberInRange(array.at(10), 0.000001, 1.0e9, &gridMinorSpacing)
        || !cborNumberInRange(array.at(12), 0.0, 1.0, &gridOpacity)
        || !cborNumberInRange(array.at(15), 0.01, 10.0, &gridLineWidth)
        || !array.at(13).isInteger() || array.at(13).toInteger() < 0
        || array.at(13).toInteger() > std::numeric_limits<quint32>::max()
        || !array.at(14).isInteger() || array.at(14).toInteger() < 0
        || array.at(14).toInteger() > std::numeric_limits<quint32>::max()
        || !array.at(5).isBool() || !array.at(6).isBool()
        || !array.at(7).isBool() || !array.at(11).isBool()
        || !array.at(16).isBool() || !array.at(17).isBool()
        || !array.at(18).isBool() || !array.at(19).isBool()
        || !array.at(23).isBool() || !array.at(27).isBool()
        || !array.at(28).isBool() || !array.at(29).isBool()
        || !array.at(30).isBool() || !array.at(31).isBool()
        || !array.at(32).isBool() || !array.at(36).isBool()
        || !array.at(37).isBool())
    {
        return false;
    }
    out->pipeline = static_cast<GlView::RenderPipeline>(pipeline);
    out->nearPlane = static_cast<float>(nearPlane);
    out->farPlane = static_cast<float>(farPlane);
    out->showGrid = array.at(5).toBool();
    out->gridFollowCamera = array.at(6).toBool();
    out->gridRotateWithView = array.at(7).toBool();
    out->gridPlane = static_cast<GlView::GridPlane>(plane);
    out->gridMajorSpacing = static_cast<float>(gridMajorSpacing);
    out->gridMinorSpacing = static_cast<float>(gridMinorSpacing);
    out->gridAdaptiveSpacing = array.at(11).toBool();
    out->gridOpacity = static_cast<float>(gridOpacity);
    out->gridColor = QColor::fromRgba(static_cast<QRgb>(array.at(13).toInteger()));
    out->gridMajorColor = QColor::fromRgba(static_cast<QRgb>(array.at(14).toInteger()));
    out->gridLineWidthPx = static_cast<float>(gridLineWidth);
    out->gridVisibleThroughWalls = array.at(16).toBool();
    out->gridAlwaysBehind = array.at(17).toBool();
    out->gridResetOnCameraRotate = array.at(18).toBool();
    out->gridAlignToView = array.at(19).toBool();
    if (!array.at(20).isNull() && !array.at(20).isUndefined())
    {
        QVector3D center{};
        if (!vectorFromCbor(array.at(20), &center))
        {
            return false;
        }
        out->gridCenterOverride = center;
    }
    else
    {
        out->gridCenterOverride.reset();
    }
    if (!vectorFromCbor(array.at(21), &out->gridAlignAxisA)
        || !vectorFromCbor(array.at(22), &out->gridAlignAxisB)
        || !vectorFromCbor(array.at(24), &out->gridOverrideCamForward)
        || !vectorFromCbor(array.at(25), &out->gridOverrideCamUp))
    {
        return false;
    }
    out->gridOverrideCamBasisValid = array.at(23).toBool();
    out->dynamicLightShading = static_cast<GlView::DynamicLightShading>(shading);
    out->showEditorCubes = array.at(27).toBool();
    out->showPreviewCubes = array.at(28).toBool();
    out->previewRespectHiddenLayers = array.at(29).toBool();
    out->xraySelectionCycling = array.at(30).toBool();
    out->orbitAroundSelection = array.at(31).toBool();
    out->showSectorThrustVectors = array.at(32).toBool();
    if (!array.at(33).isArray())
    {
        return false;
    }
    const QCborArray categories = array.at(33).toArray();
    if ((categories.size() != 3 && categories.size() != 4)
        || !categories.at(0).isBool()
        || !categories.at(1).isBool()
        || !categories.at(2).isBool()
        || (categories.size() == 4 && !categories.at(3).isBool()))
    {
        return false;
    }
    for (int i = 0; i < categories.size(); ++i)
    {
        out->alwaysShowLightGizmoCategories[static_cast<std::size_t>(i)] = categories.at(i).toBool();
    }
    if (categories.size() == 3)
    {
        // Recovery written before the ARCH family existed has no preference
        // to restore. Match the pipeline's default without disturbing the
        // three persisted legacy families: editor overlays start enabled,
        // while Game Preview keeps the preview overlay opt-in.
        out->alwaysShowLightGizmoCategories[
            static_cast<std::size_t>(render::LightGizmoCategory::Architectural)] =
            static_cast<GlView::RenderPipeline>(pipeline)
                == GlView::RenderPipeline::Editor;
    }
    out->transformTool = static_cast<GlView::TransformTool>(tool);
    out->transformOrientation = static_cast<GlView::TransformOrientation>(orientation);
    out->transformGridSnap = array.at(36).toBool();
    out->transformGridPlaneStick = array.at(37).toBool();
    // V1 editor-state arrays written before this display-only option remain
    // readable; their missing ARCH family follows the restored pipeline's
    // default above.
    out->gridKeepViewAlignedVisible = array.size() == 40
        && array.at(39).toBool();
    const QCborMap cleave = array.at(38).toMap();
    quint64 encodedOwnedBytes = 0;
    if ( !addEncodedCleaveWorkspaceBytes(cleave, &encodedOwnedBytes) ) return false;
    const QCborValue encodedPeer = cleave.value(QStringLiteral("peerWorkspace"));
    if ( !encodedPeer.isUndefined()
        && (!encodedPeer.isMap()
            || !addEncodedCleaveWorkspaceBytes(encodedPeer.toMap(), &encodedOwnedBytes)) ) return false;
    if ( !cleaveSingleWorkspaceFromCbor(cleave, out) )
    {
        return false;
    }
    using Family = GlView::CleaveWorkspaceFamily;
    const QString familyKey = QStringLiteral("currentFamily");
    if ( cleave.contains(familyKey) )
    {
        qint64 family = -1;
        if ( !cborIntegerInRange(cleave.value(familyKey), 0, 1, &family) ) return false;
        out->cleaveWorkspaceFamily = static_cast<Family>(family);
    }
    else
    {
        out->cleaveWorkspaceFamily = out->cleaveContext.valid
            ? (out->cleaveContext.surfaceMode ? Family::Surface : Family::Sector)
            : (out->surfaceCleaveToolEnabled && !out->sectorCleaveToolEnabled
                ? Family::Surface : Family::Sector);
    }
    if ( !cleaveFamilyMatches(out->cleaveContext, out->cleaveHistory, out->cleaveWorkspaceFamily)
        || (out->sectorCleaveToolEnabled && out->surfaceCleaveToolEnabled)
        || (out->sectorCleaveToolEnabled && out->cleaveWorkspaceFamily != Family::Sector)
        || (out->surfaceCleaveToolEnabled && out->cleaveWorkspaceFamily != Family::Surface) )
    {
        return false;
    }
    out->cleavePeerWorkspace.reset();
    const QString peerKey = QStringLiteral("peerWorkspace");
    if ( cleave.contains(peerKey) )
    {
        const QCborValue peerValue = cleave.value(peerKey);
        if ( !peerValue.isMap() ) return false;
        const QCborMap peer = peerValue.toMap();
        qint64 peerFamily = -1;
        if ( peer.contains(peerKey)
            || !cborIntegerInRange(peer.value(familyKey), 0, 1, &peerFamily)
            || static_cast<Family>(peerFamily) == out->cleaveWorkspaceFamily ) return false;
        GlView::RecoveryState parked{};
        if ( !cleaveSingleWorkspaceFromCbor(peer, &parked)
            || parked.sectorCleaveToolEnabled || parked.surfaceCleaveToolEnabled
            || !cleaveFamilyMatches(parked.cleaveContext, parked.cleaveHistory,
                static_cast<Family>(peerFamily))
            || !cleaveWorkspaceReferencesValid(parked, currentLevel) ) return false;
        out->cleavePeerWorkspace = takeRecoveredCleaveWorkspace(parked);
    }
    if ( !out->cleavePeerWorkspace )
    {
        out->cleavePeerWorkspace = GlView::defaultStagedCleaveWorkspaceOptions(&currentLevel);
    }
    quint64 aggregateBytes = 0;
    if ( !addCurrentCleaveOwnedBytes(&aggregateBytes, *out) ) return false;
    if ( out->cleaveHistory )
    {
        if ( !addCleaveOwnedBytes(&aggregateBytes, out->cleaveHistory->snapshots.capacity(),
            sizeof(GlView::RecoveryState::CleaveDraftSnapshot)) ) return false;
        for ( const auto& snapshot : out->cleaveHistory->snapshots )
        {
            if ( !addRecoveredCleaveDraftCapacities(&aggregateBytes, snapshot) ) return false;
        }
    }
    if ( out->cleavePeerWorkspace
        && !addCleaveOwnedBytes(&aggregateBytes,
            GlView::stagedCleaveWorkspaceOwnedBytesEstimate(*out->cleavePeerWorkspace), 1u) ) return false;
    if ( aggregateBytes > kMaximumCleaveHistoryBytes ) return false;
    return cleaveWorkspaceReferencesValid(*out, currentLevel);
}

QString absoluteDisplaySourcePath(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return {};
    }
    return QDir::cleanPath(QFileInfo(QString::fromStdWString(path.wstring())).absoluteFilePath());
}

QString normalizedRecoveryPath(const std::filesystem::path& path)
{
    if (path.empty()) return {};
    QString normalized = QDir::cleanPath(
        QFileInfo(QString::fromStdWString(path.wstring())).absoluteFilePath());
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

bool recoveryPathsEqual(
    const std::filesystem::path& lhs,
    const std::filesystem::path& rhs)
{
    if (lhs.empty() || rhs.empty()) return lhs.empty() && rhs.empty();
    return normalizedRecoveryPath(lhs) == normalizedRecoveryPath(rhs);
}

QString recoveryPromptText(const ui::recovery::RecoveryMetadata& metadata)
{
    const QString when = QDateTime::fromMSecsSinceEpoch(metadata.timestamp).toLocalTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString target = metadata.sourcePath.isEmpty()
        ? QStringLiteral("an unsaved level")
        : QDir::toNativeSeparators(metadata.sourcePath);
    QString text = QStringLiteral(
        "SED found recoverable changes for %1.\n\n"
        "Snapshot: %2\n"
        "The saved level is not overwritten unless you explicitly save after recovery.")
        .arg(target, when);
    if (ui::recovery::recoverySourceChanged(metadata))
    {
        text += QStringLiteral(
            "\n\nThe saved file changed after this editing session began. "
            "Recovering is still safe: SED opens the snapshot as unsaved work and will not overwrite the newer file automatically.");
    }
    if (!metadata.projectTypeKnown())
    {
        text += QStringLiteral(
            "\n\nThe project type could not be verified from the damaged snapshot. "
            "SED will ask you to choose JKDF2, MotS, or IJIM before loading it.");
    }
    return text;
}

QString recoveryProjectTypeLabel(const ProjectType kind)
{
    switch (kind)
    {
    case ProjectType::JKDF2: return QStringLiteral("JKDF2");
    case ProjectType::MOTS: return QStringLiteral("MotS");
    case ProjectType::IJIM: return QStringLiteral("IJIM");
    default: return QStringLiteral("Unknown");
    }
}

QString recoveryListText(const ui::recovery::RecoveryMetadata& metadata)
{
    const QString name = !metadata.displayName.trimmed().isEmpty()
        ? metadata.displayName.trimmed()
        : (metadata.sourcePath.trimmed().isEmpty()
            ? QStringLiteral("Untitled level")
            : QFileInfo(metadata.sourcePath).fileName());
    const QString when = QDateTime::fromMSecsSinceEpoch(metadata.timestamp).toLocalTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return QStringLiteral("%1\n%2").arg(name, when);
}

enum class RecoveryPromptAction : std::uint8_t
{
    later,
    recover,
    discard,
};

enum class RecoveryDialogMode : std::uint8_t
{
    startup,
    openSaved,
    history,
};

/** Fixed, content-sized recovery choice window used before MainWindow is visible. */
class RecoveryOfferDialog final : public QDialog
{
public:
    RecoveryOfferDialog(
        std::vector<ui::recovery::RecoveryDescriptor> descriptors,
        const RecoveryDialogMode mode,
        QWidget* parent = nullptr)
        : QDialog(parent), descriptors_(std::move(descriptors)), mode_(mode)
    {
        setObjectName(QStringLiteral("recovery_offer_dialog"));
        setWindowTitle(mode_ == RecoveryDialogMode::history
            ? QStringLiteral("Autosave history")
            : QStringLiteral("Recover unsaved changes"));
        setWindowIcon(loadApplicationIcon());
        setWindowModality(Qt::ApplicationModal);
        setWindowFlags(
            Qt::Window
            | Qt::CustomizeWindowHint
            | Qt::WindowTitleHint
            | Qt::WindowCloseButtonHint
            | Qt::MSWindowsFixedSizeDialogHint);
        // A recovery prompt can be displayed before MainWindow::show().
        // Force the native handle now so the executable's large icon is
        // applied before exec() enters the modal loop (the deferred apply
        // below remains for Windows recreating the handle after show).
        (void)winId();
        applyWindowsTaskbarIcon(*this);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(20, 18, 20, 16);
        root->setSpacing(14);
        root->setSizeConstraint(QLayout::SetFixedSize);

        auto* messageRow = new QHBoxLayout();
        messageRow->setSpacing(16);
        auto* icon = new QLabel(this);
        icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(48, 48));
        icon->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        icon->setFixedSize(52, 52);
        messageRow->addWidget(icon, 0, Qt::AlignTop);

        messageLabel_ = new QLabel(this);
        messageLabel_->setObjectName(QStringLiteral("recovery_offer_message"));
        messageLabel_->setWordWrap(true);
        messageLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        messageLabel_->setMinimumWidth(510);
        messageLabel_->setMaximumWidth(650);
        messageLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        messageRow->addWidget(messageLabel_, 1);
        root->addLayout(messageRow);

        if (descriptors_.size() > 1 || mode_ == RecoveryDialogMode::history)
        {
            otherRecoveriesButton_ = new QToolButton(this);
            otherRecoveriesButton_->setObjectName(QStringLiteral("recovery_other_button"));
            otherRecoveriesButton_->setText(mode_ == RecoveryDialogMode::history
                ? QStringLiteral("Available Autosaves (%1)").arg(descriptors_.size())
                : QStringLiteral("Other Recoveries (%1)")
                    .arg(descriptors_.size() - 1));
            otherRecoveriesButton_->setCheckable(true);
            otherRecoveriesButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            otherRecoveriesButton_->setArrowType(Qt::RightArrow);
            root->addWidget(otherRecoveriesButton_, 0, Qt::AlignLeft);

            recoveryList_ = new QListWidget(this);
            recoveryList_->setObjectName(QStringLiteral("recovery_other_list"));
            recoveryList_->setSelectionMode(QAbstractItemView::SingleSelection);
            recoveryList_->setMinimumWidth(590);
            recoveryList_->setFixedHeight(std::min<int>(220,
                48 * static_cast<int>(descriptors_.size()) + 6));
            const bool showHistoryList = mode_ == RecoveryDialogMode::history;
            recoveryList_->setVisible(showHistoryList);
            for (std::size_t i = 0; i < descriptors_.size(); ++i)
            {
                const auto& descriptor = descriptors_[i];
                auto* item = new QListWidgetItem(recoveryListText(descriptor.metadata));
                item->setData(Qt::UserRole, static_cast<int>(i));
                item->setToolTip(descriptor.metadata.sourcePath.trimmed().isEmpty()
                    ? QStringLiteral("Unsaved level")
                    : QDir::toNativeSeparators(descriptor.metadata.sourcePath));
                item->setSizeHint(QSize(0, 48));
                recoveryList_->addItem(item);
            }
            recoveryList_->setCurrentRow(0);
            root->addWidget(recoveryList_);
            otherRecoveriesButton_->setChecked(showHistoryList);
            otherRecoveriesButton_->setArrowType(
                showHistoryList ? Qt::DownArrow : Qt::RightArrow);

            connect(otherRecoveriesButton_, &QToolButton::toggled,
                this, [this](const bool expanded)
                {
                    otherRecoveriesButton_->setArrowType(
                        expanded ? Qt::DownArrow : Qt::RightArrow);
                    recoveryList_->setVisible(expanded);
                    QTimer::singleShot(0, this, [this]()
                        {
                            adjustSize();
                            setFixedSize(sizeHint());
                        });
                });
            connect(recoveryList_, &QListWidget::currentRowChanged,
                this, [this](const int row)
                {
                    if (row < 0 || row >= recoveryList_->count()) return;
                    selectedIndex_ = static_cast<std::size_t>(
                        recoveryList_->item(row)->data(Qt::UserRole).toInt());
                    updateMessage();
                });
        }

        auto* buttons = new QHBoxLayout();
        buttons->setSpacing(10);
        buttons->addStretch(1);
        auto* recoverButton = new QPushButton(
            mode_ == RecoveryDialogMode::history
                ? QStringLiteral("Open Autosave")
                : QStringLiteral("Recover"), this);
        auto* discardButton = new QPushButton(
            mode_ == RecoveryDialogMode::history
                ? QStringLiteral("Delete")
                : (mode_ == RecoveryDialogMode::startup
                    ? QStringLiteral("Discard Recovery")
                    : QStringLiteral("Discard && Open Saved")), this);
        auto* laterButton = new QPushButton(
            mode_ == RecoveryDialogMode::history
                ? QStringLiteral("Close")
                : (mode_ == RecoveryDialogMode::startup
                    ? QStringLiteral("Later")
                    : QStringLiteral("Cancel")), this);
        recoverButton->setDefault(true);
        recoverButton->setMinimumWidth(96);
        discardButton->setMinimumWidth(132);
        discardButton->setToolTip(mode_ == RecoveryDialogMode::history
            ? QStringLiteral("Permanently delete the selected autosave history entry.")
            : QStringLiteral(
                "Permanently delete recovery snapshots for this editing session and continue with the saved file."));
        laterButton->setMinimumWidth(82);
        buttons->addWidget(recoverButton);
        buttons->addWidget(discardButton);
        buttons->addWidget(laterButton);
        root->addLayout(buttons);

        connect(recoverButton, &QPushButton::clicked, this, [this]()
            {
                action_ = RecoveryPromptAction::recover;
                accept();
            });
        connect(discardButton, &QPushButton::clicked, this, [this]()
            {
                action_ = RecoveryPromptAction::discard;
                accept();
            });
        connect(laterButton, &QPushButton::clicked, this, &QDialog::reject);

        updateMessage();
        QTimer::singleShot(0, this, [this]()
            {
                adjustSize();
                setFixedSize(sizeHint());
                applyWindowsTaskbarIcon(*this);
            });
    }

    RecoveryPromptAction action() const noexcept { return action_; }

    const ui::recovery::RecoveryDescriptor& selectedDescriptor() const
    {
        return descriptors_.at(selectedIndex_);
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QDialog::showEvent(event);
        // Windows may recreate the native handle when a previously hidden
        // modal window is shown. Re-apply the executable icon after that
        // transition so a parentless startup offer always has a taskbar icon.
        setWindowIcon(loadApplicationIcon());
        (void)winId();
        applyWindowsTaskbarIcon(*this);
    }

private:
    void updateMessage()
    {
        if (messageLabel_ && selectedIndex_ < descriptors_.size())
        {
            messageLabel_->setText(mode_ == RecoveryDialogMode::history
                ? QStringLiteral(
                    "Open this autosave as unsaved work, or delete it from history.\n\n%1")
                    .arg(recoveryListText(descriptors_[selectedIndex_].metadata))
                : recoveryPromptText(descriptors_[selectedIndex_].metadata));
        }
    }

    std::vector<ui::recovery::RecoveryDescriptor> descriptors_{};
    RecoveryDialogMode mode_{RecoveryDialogMode::startup};
    std::size_t selectedIndex_{};
    RecoveryPromptAction action_{RecoveryPromptAction::later};
    QLabel* messageLabel_{};
    QToolButton* otherRecoveriesButton_{};
    QListWidget* recoveryList_{};
};

struct RecoveryLoadResult
{
    std::optional<ui::recovery::RecoveryRecord> record{};
    std::optional<ui::recovery::RecoveredDocument> recovered{};
    bool requiresProjectSelection{};
    QString error{};
};

RecoveryLoadResult readAndDecodeRecovery(const std::filesystem::path& recoveryPath)
{
    RecoveryLoadResult result{};
    try
    {
        result.record = ui::recovery::readRecoveryRecord(
            recoveryPath,
            &result.error,
            &ui::recovery::validateWorkspaceHistoryAttachmentForLevels);
        if (!result.record)
        {
            return result;
        }
        if (!result.record->metadata.projectTypeKnown())
        {
            // Do not decode HIST or construct a Document until the user has
            // selected the project kind.  The JKDF2 enum is only a storage
            // default and is never an implicit fallback for damaged files.
            result.requiresProjectSelection = true;
            return result;
        }
        result.recovered = ui::recovery::loadRecoveredDocument(
            std::move(*result.record),
            &result.error,
            &ui::recovery::validateWorkspaceHistoryAttachmentForLevels);
        return result;
    }
    catch (const std::exception& e)
    {
        result.record.reset();
        result.recovered.reset();
        result.error = QStringLiteral("Recovery failed safely: %1")
            .arg(QString::fromUtf8(e.what()));
        return result;
    }
    catch (...)
    {
        result.record.reset();
        result.recovered.reset();
        result.error = QStringLiteral("Recovery failed safely after an unknown error.");
        return result;
    }
}

void appendRecoveryAudit(
    const std::filesystem::path& directory,
    const ui::recovery::RecoveryRecord& record,
    const QString& outcome)
{
    constexpr qint64 kMaximumAuditLogBytes = 1024 * 1024;
    constexpr qsizetype kMaximumAuditEntryBytes = 64 * 1024;
    constexpr qsizetype kMaximumAuditFieldCharacters = 8192;
    const auto boundedField = [](const QString& source)
    {
        bool truncated = false;
        QString value = ui::recovery::detail::escapeForensicText(
            source,
            kMaximumAuditFieldCharacters,
            ui::recovery::detail::ForensicNewlinePolicy::replaceWithSpace,
            &truncated);
        if (!truncated) return value;

        const QString suffix = QStringLiteral(" ... [truncated]");
        value.truncate(kMaximumAuditFieldCharacters - suffix.size());
        value += suffix;
        return value;
    };

    QDir().mkpath(QString::fromStdWString(directory.wstring()));
    const QString logPath = QString::fromStdWString(
        (directory / "recovery.log").wstring());
    const QString rotatedPath = logPath + QStringLiteral(".1");
    QByteArray entry = QStringLiteral(
        "%1 identity=%2 outcome=%3 corrupted=%4 partial=%5 repaired_native_chunks=%6 repaired_metadata_shards=%7 lost_sections=%8 unidentified_sections=%9\n")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
            boundedField(record.metadata.identity), boundedField(outcome),
            record.corrupted ? QStringLiteral("yes") : QStringLiteral("no"),
            record.partialRecovery ? QStringLiteral("yes") : QStringLiteral("no"))
        .arg(record.repairedNativeChunkCount)
        .arg(record.repairedMetadataShardCount)
        .arg(record.lostSectionCount)
        .arg(record.unidentifiedMissingSectionCount)
        .toUtf8();
    bool warningTextTruncated = false;
    if (record.corrupted || record.partialRecovery || !record.warnings.isEmpty())
    {
        for (const QString& warning : record.warnings)
        {
            const QByteArray line = QStringLiteral("  %1\n")
                .arg(boundedField(warning)).toUtf8();
            if (entry.size() > kMaximumAuditEntryBytes - line.size())
            {
                warningTextTruncated = true;
                break;
            }
            entry += line;
        }
    }
    if (warningTextTruncated)
    {
        const QByteArray marker(
            "  ... [additional recovery diagnostics truncated]\n");
        if (entry.size() <= kMaximumAuditEntryBytes - marker.size())
        {
            entry += marker;
        }
    }
    if (entry.size() > kMaximumAuditEntryBytes)
    {
        entry.truncate(kMaximumAuditEntryBytes);
    }

    const qint64 existingBytes = QFileInfo(logPath).size();
    if (existingBytes < 0
        || existingBytes > kMaximumAuditLogBytes - entry.size())
    {
        (void)QFile::remove(rotatedPath);
        if (QFileInfo::exists(logPath) && !QFile::rename(logPath, rotatedPath))
        {
            return;
        }
    }

    QFile file(logPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    (void)file.write(entry);
    file.flush();
}

std::optional<std::filesystem::path> retainCorruptedRecoveryFile(
    const std::filesystem::path& recoveryPath,
    const bool preserveDiscoverableOriginal,
    QString* warning)
{
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmsszzz"));
    const std::filesystem::path retained = recoveryPath.parent_path()
        / (recoveryPath.stem().string() + ".corrupted-" + stamp.toStdString()
            + "-" + ui::recovery::newSessionId().toStdString() + ".sedrecovery");
    std::error_code error{};
    if (preserveDiscoverableOriginal)
    {
        std::filesystem::copy_file(recoveryPath, retained, error);
    }
    else
    {
        std::filesystem::rename(recoveryPath, retained, error);
    }
    if (error)
    {
        if (warning)
        {
            *warning = QStringLiteral("Could not retain the damaged recovery file: %1")
                .arg(QString::fromStdString(error.message()));
        }
        return std::nullopt;
    }
    return retained;
}

} // namespace

namespace ui::recovery
{

bool validateWorkspaceHistoryAttachmentForLevels(
    const QCborValue& cleaveAttachment,
    const QCborValue& layerVisualsAttachment,
    const Level* const beforeLevel,
    const Level* const afterLevel,
    QString* const error)
{
    const auto fail = [error](const QString& message)
    {
        if (error) *error = message;
        return false;
    };
    if (error) error->clear();

    const auto selectionReferences = [](
        const editor::Selection& selection, const Level& level) noexcept
    {
        const auto indexValid = [](const std::int32_t index,
                                   const std::size_t count) noexcept
        {
            return index >= 0 && static_cast<std::size_t>(index) < count;
        };
        const auto allIndicesValid = [&indexValid](const auto& values,
                                                    const std::size_t count)
        {
            return std::all_of(values.begin(), values.end(),
                [&indexValid, count](const std::int32_t index)
                {
                    return indexValid(index, count);
                });
        };
        const auto surfaceValid = [&level, &indexValid](const SurfaceRef ref)
        {
            return indexValid(ref.sector, level.sectors.size())
                && indexValid(ref.surface,
                    level.sectors[static_cast<std::size_t>(ref.sector)]
                        .surfaces.size());
        };
        if (!allIndicesValid(selection.sectors, level.sectors.size())
            || !allIndicesValid(selection.things, level.things.size())
            || !allIndicesValid(selection.lights, level.lights.size())
            || !allIndicesValid(selection.cogs, level.cogs.size())
            || !std::all_of(selection.surfaces.begin(), selection.surfaces.end(),
                surfaceValid))
        {
            return false;
        }
        if (!std::all_of(selection.surfaceVertices.begin(),
                selection.surfaceVertices.end(),
                [&level, &indexValid, &surfaceValid](
                    const editor::SurfaceVertexRef& ref)
                {
                    return surfaceValid(ref.surface)
                        && indexValid(ref.localVertex,
                            level.sectors[static_cast<std::size_t>(ref.surface.sector)]
                                .surfaces[static_cast<std::size_t>(ref.surface.surface)]
                                .vertices.size());
                }))
        {
            return false;
        }
        return std::all_of(selection.frames.begin(), selection.frames.end(),
            [&level, &indexValid](const level::FrameRef& ref)
            {
                if (!indexValid(ref.thing, level.things.size())) return false;
                const Thing& thing = level.things[static_cast<std::size_t>(ref.thing)];
                if (!indexValid(ref.value, thing.values.size())) return false;
                const TplValue& value = thing.values[static_cast<std::size_t>(ref.value)];
                return value.type == ValueType::frame
                    && text::iequalsAscii(value.name, "frame");
            });
    };

    if (!cleaveAttachment.isUndefined())
    {
        if (!cleaveAttachment.isMap() || !beforeLevel)
        {
            return fail(QStringLiteral(
                "A historical Cleave attachment has no validated pre-Apply Level."));
        }
        const QCborMap cleave = cleaveAttachment.toMap();
        const QCborValue workspaceValue = cleave.value(QStringLiteral("workspace"));
        const QCborValue selectionValue = cleave.value(
            QStringLiteral("documentSelection"));
        const QCborValue activeValue = cleave.value(QStringLiteral("active"));
        const QCborValue modeValue = cleave.value(QStringLiteral("mode"));
        if (!workspaceValue.isMap()
            || workspaceValue.toMap().contains(QStringLiteral("peerWorkspace"))
            || !selectionValue.isByteArray()
            || !activeValue.isArray() || activeValue.toArray().size() != 7
            || !modeValue.isArray() || modeValue.toArray().size() != 3)
        {
            return fail(QStringLiteral(
                "A historical Cleave attachment is structurally incomplete."));
        }

        GlView::RecoveryState scratch{};
        scratch.nearPlane = -1.0f;
        scratch.farPlane = 1.0f;
        QCborArray viewport = glRecoveryToCbor(scratch);
        viewport[38] = workspaceValue;
        if (!glRecoveryFromCbor(QCborValue(viewport), &scratch, *beforeLevel))
        {
            return fail(QStringLiteral(
                "A historical Cleave workspace is invalid for its pre-Apply Level."));
        }
        QString selectionError{};
        const std::optional<editor::Selection> selection = deserializeSelection(
            selectionValue.toByteArray(), &selectionError);
        if (!selection || !selectionReferences(*selection, *beforeLevel))
        {
            return fail(QStringLiteral(
                "A historical Cleave document selection is invalid for its pre-Apply Level."));
        }
        const QCborArray active = activeValue.toArray();
        std::array<std::int32_t, 7> activeIndices{};
        for (qsizetype index = 0; index < active.size(); ++index)
        {
            if (!cborInt32(active.at(index), &activeIndices[static_cast<std::size_t>(index)]))
            {
                return fail(QStringLiteral(
                    "A historical Cleave active item is malformed."));
            }
        }
        const auto optionalIndex = [](const std::int32_t index,
                                      const std::size_t count) noexcept
        {
            return index == -1
                || (index >= 0 && static_cast<std::size_t>(index) < count);
        };
        if (!optionalIndex(activeIndices[0], beforeLevel->sectors.size())
            || !optionalIndex(activeIndices[1], beforeLevel->sectors.size())
            || !optionalIndex(activeIndices[3], beforeLevel->things.size())
            || !optionalIndex(activeIndices[4], beforeLevel->lights.size()))
        {
            return fail(QStringLiteral(
                "A historical Cleave active item references a missing pre-Apply object."));
        }
        const std::int32_t surfaceSector = activeIndices[1];
        const std::int32_t surfaceIndex = activeIndices[2];
        if (surfaceIndex < -1
            || (surfaceSector == -1) != (surfaceIndex == -1)
            || (surfaceIndex >= 0
                && (surfaceSector < 0
                    || static_cast<std::size_t>(surfaceIndex)
                        >= beforeLevel->sectors[static_cast<std::size_t>(surfaceSector)]
                            .surfaces.size())))
        {
            return fail(QStringLiteral(
                "A historical Cleave active Surface is missing from its pre-Apply Level."));
        }
        const std::int32_t frameThing = activeIndices[5];
        const std::int32_t frameValue = activeIndices[6];
        if (frameThing < -1 || frameValue < -1
            || (frameThing == -1) != (frameValue == -1)
            || (frameThing >= 0 && activeIndices[3] != frameThing)
            || (frameThing >= 0
                && (!optionalIndex(frameThing, beforeLevel->things.size())
                    || !optionalIndex(frameValue,
                        beforeLevel->things[static_cast<std::size_t>(frameThing)]
                            .values.size())))
            || (frameThing >= 0
                && !selection->frames.contains(level::FrameRef{
                    frameThing, frameValue })))
        {
            return fail(QStringLiteral(
                "A historical Cleave active frame is invalid for its pre-Apply selection."));
        }
        const QCborArray mode = modeValue.toArray();
        if (!cborIntegerInRange(mode.at(0), 0, 5)
            || !cborIntegerInRange(mode.at(1), 0, 2)
            || !cborIntegerInRange(mode.at(2), 0, 3))
        {
            return fail(QStringLiteral(
                "A historical Cleave editor mode is out of range."));
        }
    }

    if (!layerVisualsAttachment.isUndefined())
    {
        if (!layerVisualsAttachment.isMap())
        {
            return fail(QStringLiteral(
                "A historical layer visual attachment is malformed."));
        }
        const QCborMap visuals = layerVisualsAttachment.toMap();
        const std::size_t maximumLayerCount = 1'000'000u;
        const auto validateSide = [maximumLayerCount](
            const QCborValue& sideValue, const Level* const level)
        {
            if (!sideValue.isMap())
            {
                return false;
            }
            const QCborMap side = sideValue.toMap();
            const QCborValue visibleValue = side.value(QStringLiteral("visible"));
            const QCborValue lockedValue = side.value(QStringLiteral("locked"));
            const QCborValue colorsValue = side.value(QStringLiteral("colors"));
            if (!visibleValue.isArray() || !lockedValue.isArray()
                || !colorsValue.isArray())
            {
                return false;
            }
            const std::size_t visibleCount = static_cast<std::size_t>(
                visibleValue.toArray().size());
            const std::size_t lockedCount = static_cast<std::size_t>(
                lockedValue.toArray().size());
            const std::size_t colorCount = static_cast<std::size_t>(
                colorsValue.toArray().size());
            if (visibleCount > maximumLayerCount
                || lockedCount != visibleCount || colorCount != visibleCount)
            {
                return false;
            }
            for (const QCborValue& value : visibleValue.toArray())
            {
                if (!value.isBool()) return false;
            }
            for (const QCborValue& value : lockedValue.toArray())
            {
                if (!value.isBool()) return false;
            }
            for (const QCborValue& value : colorsValue.toArray())
            {
                if (!cborIntegerInRange(value, 0,
                        std::numeric_limits<quint32>::max()))
                {
                    return false;
                }
            }
            if (level)
            {
                const std::size_t layerCount = ui::layers::normalizedLayerCount(
                    level->layers.size());
                if (layerCount != visibleCount)
                {
                    return false;
                }
            }
            return true;
        };
        if (!validateSide(visuals.value(QStringLiteral("before")), beforeLevel)
            || !validateSide(visuals.value(QStringLiteral("after")), afterLevel))
        {
            return fail(QStringLiteral(
                "Historical layer visuals do not match the exact adjacent Level layer rows."));
        }
    }
    return true;
}

} // namespace ui::recovery

void MainWindow::restoreWorkspaceHistoryAttachments(
    const ui::recovery::WorkspaceHistoryState& workspaceHistory,
    QStringList* const warnings)
{
    const auto warn = [warnings](const QString& message)
    {
        if (warnings)
        {
            warnings->push_back(message);
        }
    };
    if (workspaceHistory.attachments.empty())
    {
        return;
    }
    if (!glView_ || workspaceHistory.attachments.size() > 64u)
    {
        warn(QStringLiteral(
            "Historical editor workspace attachments were dropped because the current document cannot accept them."));
        return;
    }

    // A document replacement has already installed its fresh HIST records.
    // Remove any old-document attachments before measuring the complete
    // candidate import; live records are never evicted by this cleanup.
    pruneCleaveApplyWorkspaceMementos();

    const auto addBytes = [](std::uint64_t& total,
                             const std::uint64_t count,
                             const std::uint64_t elementBytes) noexcept
    {
        if (elementBytes != 0u
            && count > (std::numeric_limits<std::uint64_t>::max() - total)
                / elementBytes)
        {
            return false;
        }
        total += count * elementBytes;
        return true;
    };
    const auto selectionBytes = [&addBytes](
        const editor::Selection& selection,
        std::uint64_t& total) noexcept
    {
        constexpr std::uint64_t kSetNodeAllowance = 64u;
        return addBytes(total, selection.sectors.size(), sizeof(std::int32_t) + kSetNodeAllowance)
            && addBytes(total, selection.surfaces.size(), sizeof(SurfaceRef) + kSetNodeAllowance)
            && addBytes(total, selection.surfaceVertices.size(), sizeof(editor::SurfaceVertexRef) + kSetNodeAllowance)
            && addBytes(total, selection.things.size(), sizeof(std::int32_t) + kSetNodeAllowance)
            && addBytes(total, selection.frames.size(), sizeof(level::FrameRef) + kSetNodeAllowance)
            && addBytes(total, selection.lights.size(), sizeof(std::int32_t) + kSetNodeAllowance)
            && addBytes(total, selection.cogs.size(), sizeof(std::int32_t) + kSetNodeAllowance);
    };
    const auto snapshotBytes = [&addBytes](
        const GlView::RecoveryState::CleaveDraftSnapshot& snapshot,
        std::uint64_t& total) noexcept
    {
        return addBytes(total, 1u, sizeof(snapshot))
            && addBytes(total, snapshot.lines.capacity(),
                sizeof(GlView::RecoveryState::CleaveLine))
            && addBytes(total, snapshot.context.targetSectors.capacity(),
                sizeof(std::int32_t))
            && addBytes(total, snapshot.context.targetSurfaces.capacity(),
                sizeof(SurfaceRef))
            && addBytes(total, snapshot.applyOrder.capacity(),
                sizeof(std::int32_t))
            && addBytes(total, snapshot.selectedCleaveLines.capacity(),
                sizeof(std::int32_t));
    };
    const auto estimateMementoBytes = [&addBytes, &snapshotBytes, &selectionBytes](
        const CleaveApplyWorkspaceMemento& memento,
        std::uint64_t* bytes) noexcept
    {
        if (!bytes)
        {
            return false;
        }
        std::uint64_t total = sizeof(CleaveApplyWorkspaceMemento);
        if (!snapshotBytes(memento.draft, total)
            || !selectionBytes(memento.documentSelection, total))
        {
            return false;
        }
        if (memento.history)
        {
            if (!addBytes(total, 1u, sizeof(GlView::RecoveryState::CleaveHistory))
                || !addBytes(total, memento.history->snapshots.capacity(),
                    sizeof(GlView::RecoveryState::CleaveDraftSnapshot)))
            {
                return false;
            }
            for (const auto& snapshot : memento.history->snapshots)
            {
                if (!snapshotBytes(snapshot, total))
                {
                    return false;
                }
            }
        }
        *bytes = total;
        return true;
    };
    const auto decodeLayerSide = [](
        const QCborValue& value,
        LayerVisualHistoryState* state) -> bool
    {
        if (!state || !value.isMap())
        {
            return false;
        }
        const QCborMap side = value.toMap();
        const QCborValue visibleValue = side.value(QStringLiteral("visible"));
        const QCborValue lockedValue = side.value(QStringLiteral("locked"));
        const QCborValue colorsValue = side.value(QStringLiteral("colors"));
        if (!visibleValue.isArray() || !lockedValue.isArray()
            || !colorsValue.isArray())
        {
            return false;
        }
        const QCborArray visible = visibleValue.toArray();
        const QCborArray locked = lockedValue.toArray();
        const QCborArray colors = colorsValue.toArray();
        if (visible.isEmpty() || visible.size() != locked.size()
            || visible.size() != colors.size() || visible.size() > 1'000'000)
        {
            return false;
        }
        try
        {
            state->visible.reserve(static_cast<std::size_t>(visible.size()));
            state->locked.reserve(static_cast<std::size_t>(locked.size()));
            state->colors.reserve(static_cast<std::size_t>(colors.size()));
            for (qsizetype index = 0; index < visible.size(); ++index)
            {
                if (!visible.at(index).isBool() || !locked.at(index).isBool()
                    || !cborIntegerInRange(colors.at(index), 0,
                        std::numeric_limits<quint32>::max()))
                {
                    return false;
                }
                state->visible.push_back(visible.at(index).toBool());
                state->locked.push_back(locked.at(index).toBool());
                state->colors.push_back(QColor::fromRgba(
                    static_cast<QRgb>(colors.at(index).toInteger())));
            }
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
        catch (const std::length_error&)
        {
            return false;
        }
        return true;
    };

    // Refuse an oversized complete attachment set from CBOR array counts
    // before decoding any nested Cleave vectors, selection sets, or layer
    // rows. The final DTO capacity check below remains authoritative after
    // allocator capacities are known.
    constexpr std::uint64_t kMaximumImportedAttachmentBytes =
        16ull * 1024ull * 1024ull;
    const auto estimateSelectionOwnedBytes = [&addBytes](
        const QCborValue& encodedSelection,
        std::uint64_t* total) -> bool
    {
        if (!total || !encodedSelection.isByteArray())
        {
            return false;
        }
        constexpr ui::recovery::RecoveryCborLimits limits{
            assets::SedMaximumSemanticSectionBytes,
            2'000'000,
            64,
            4'096,
            0,
            assets::SedMaximumDecodedObjectBytes,
        };
        QString error{};
        const auto value = ui::recovery::deserializeCanonicalCbor(
            encodedSelection.toByteArray(), limits, &error);
        if (!value || !value->isMap())
        {
            return false;
        }
        const QCborMap selection = value->toMap();
        if (selection.size() != 7)
        {
            return false;
        }
        constexpr std::uint64_t kSetNodeAllowance = 64u;
        const auto addArray = [&selection, &addBytes, total](
            const QString& name,
            const std::uint64_t elementBytes) -> bool
        {
            const QCborValue field = selection.value(name);
            return field.isArray()
                && addBytes(*total,
                    static_cast<std::uint64_t>(field.toArray().size()),
                    elementBytes + kSetNodeAllowance);
        };
        return addArray(QStringLiteral("sectors"), sizeof(std::int32_t))
            && addArray(QStringLiteral("things"), sizeof(std::int32_t))
            && addArray(QStringLiteral("lights"), sizeof(std::int32_t))
            && addArray(QStringLiteral("cogs"), sizeof(std::int32_t))
            && addArray(QStringLiteral("surfaces"), sizeof(SurfaceRef))
            && addArray(QStringLiteral("vertices"), sizeof(editor::SurfaceVertexRef))
            && addArray(QStringLiteral("frames"), sizeof(level::FrameRef));
    };
    const auto estimateDraftOwnedBytes = [&addBytes](
        const QCborValue& encodedDraft,
        std::uint64_t* total) -> bool
    {
        if (!total)
        {
            return false;
        }
        quint64 vectorBytes = 0u;
        return cleaveDraftOwnedBytesFromCbor(encodedDraft, &vectorBytes)
            && addBytes(*total, 1u,
                sizeof(GlView::RecoveryState::CleaveDraftSnapshot))
            && addBytes(*total, vectorBytes, 1u);
    };
    const auto estimateLayerSideOwnedBytes = [&addBytes](
        const QCborValue& encodedSide,
        std::uint64_t* total) -> bool
    {
        if (!total || !encodedSide.isMap())
        {
            return false;
        }
        const QCborMap side = encodedSide.toMap();
        const QCborValue visibleValue = side.value(QStringLiteral("visible"));
        const QCborValue lockedValue = side.value(QStringLiteral("locked"));
        const QCborValue colorsValue = side.value(QStringLiteral("colors"));
        if (!visibleValue.isArray() || !lockedValue.isArray()
            || !colorsValue.isArray())
        {
            return false;
        }
        const std::uint64_t count = static_cast<std::uint64_t>(
            visibleValue.toArray().size());
        if (count > 1'000'000u
            || static_cast<std::uint64_t>(lockedValue.toArray().size()) != count
            || static_cast<std::uint64_t>(colorsValue.toArray().size()) != count)
        {
            return false;
        }
        const std::uint64_t bitBytes = count / 8u + (count % 8u != 0u ? 1u : 0u);
        return addBytes(*total, bitBytes, 2u)
            && addBytes(*total, count, sizeof(QColor));
    };

    std::uint64_t preflightAttachmentBytes = 0u;
    bool preflightValid = true;
    for (const ui::recovery::WorkspaceHistoryAttachment& attachment
         : workspaceHistory.attachments)
    {
        if (attachment.cleave.isMap())
        {
            const QCborMap encodedCleave = attachment.cleave.toMap();
            const QCborValue workspaceValue = encodedCleave.value(
                QStringLiteral("workspace"));
            const QCborValue selectionValue = encodedCleave.value(
                QStringLiteral("documentSelection"));
            preflightValid = workspaceValue.isMap()
                && addBytes(preflightAttachmentBytes, 1u,
                    sizeof(CleaveApplyWorkspaceMemento))
                && estimateDraftOwnedBytes(workspaceValue,
                    &preflightAttachmentBytes)
                && estimateSelectionOwnedBytes(selectionValue,
                    &preflightAttachmentBytes);
            const QCborValue historyValue = workspaceValue.isMap()
                ? workspaceValue.toMap().value(QStringLiteral("history"))
                : QCborValue{};
            if (preflightValid && !historyValue.isUndefined())
            {
                if (!historyValue.isMap())
                {
                    preflightValid = false;
                }
                else
                {
                    const QCborValue snapshotsValue = historyValue.toMap().value(
                        QStringLiteral("snapshots"));
                    if (!snapshotsValue.isArray()
                        || snapshotsValue.toArray().isEmpty()
                        || static_cast<std::size_t>(snapshotsValue.toArray().size())
                            > kMaximumCleaveHistorySnapshots
                        || !addBytes(preflightAttachmentBytes, 1u,
                            sizeof(GlView::RecoveryState::CleaveHistory))
                        || !addBytes(preflightAttachmentBytes,
                            static_cast<std::uint64_t>(snapshotsValue.toArray().size()),
                            sizeof(GlView::RecoveryState::CleaveDraftSnapshot)))
                    {
                        preflightValid = false;
                    }
                    else
                    {
                        for (const QCborValue& snapshotValue
                             : snapshotsValue.toArray())
                        {
                            if (!estimateDraftOwnedBytes(snapshotValue,
                                    &preflightAttachmentBytes))
                            {
                                preflightValid = false;
                                break;
                            }
                        }
                    }
                }
            }
        }
        if (preflightValid && attachment.layerVisuals.isMap())
        {
            const QCborMap visuals = attachment.layerVisuals.toMap();
            preflightValid = addBytes(preflightAttachmentBytes, 1u,
                    sizeof(LayerVisualHistoryTransition) + 64u)
                && estimateLayerSideOwnedBytes(
                    visuals.value(QStringLiteral("before")),
                    &preflightAttachmentBytes)
                && estimateLayerSideOwnedBytes(
                    visuals.value(QStringLiteral("after")),
                    &preflightAttachmentBytes);
        }
        if (!preflightValid
            || preflightAttachmentBytes > kMaximumImportedAttachmentBytes)
        {
            warn(QStringLiteral(
                "Historical Cleave/Layer workspace attachments exceed the bounded owned-memory limit and were dropped."));
            return;
        }
    }
    std::size_t existingTransitionCount = 0u;
    for (const CleaveApplyWorkspaceMemento& memento
         : cleaveApplyWorkspaceMementos_)
    {
        bool seen = false;
        for (const CleaveApplyWorkspaceMemento& previous
             : cleaveApplyWorkspaceMementos_)
        {
            if (&previous == &memento) break;
            seen = seen || previous.recordId == memento.recordId;
        }
        if (!seen)
        {
            ++existingTransitionCount;
        }
    }
    for (const auto& [signature, transition] : layerVisualHistoryByAfterSignature_)
    {
        (void)signature;
        const bool seenInCleave = std::any_of(
            cleaveApplyWorkspaceMementos_.begin(),
            cleaveApplyWorkspaceMementos_.end(),
            [&transition](const CleaveApplyWorkspaceMemento& memento)
            {
                return memento.recordId == transition.recordId;
            });
        bool seenBefore = false;
        for (auto iterator = layerVisualHistoryByAfterSignature_.begin();
             iterator != layerVisualHistoryByAfterSignature_.end(); ++iterator)
        {
            if (&iterator->second == &transition) break;
            seenBefore = seenBefore
                || iterator->second.recordId == transition.recordId;
        }
        if (!seenInCleave && !seenBefore)
        {
            ++existingTransitionCount;
        }
    }
    if (existingTransitionCount > 64u
        || workspaceHistory.attachments.size()
            > 64u - existingTransitionCount
        || !canFitCleaveWorkspaceAttachmentBudget(
            preflightAttachmentBytes, false))
    {
        warn(QStringLiteral(
            "Historical Cleave/Layer workspace attachments exceed the current 64-transition/16 MiB limit and were dropped."));
        return;
    }

    std::vector<CleaveApplyWorkspaceMemento> preparedCleave{};
    std::unordered_map<std::uint64_t, LayerVisualHistoryTransition>
        preparedLayersByAfter{};
    std::unordered_map<std::uint64_t, std::uint64_t>
        preparedLayersAfterByBefore{};
    std::vector<std::uint64_t> preparedRecordIds{};
    std::uint64_t preparedBytes = 0u;
    try
    {
        preparedCleave.reserve(workspaceHistory.attachments.size());
        preparedLayersByAfter.reserve(workspaceHistory.attachments.size());
        preparedLayersAfterByBefore.reserve(workspaceHistory.attachments.size());
        preparedRecordIds.reserve(workspaceHistory.attachments.size());

        for (const ui::recovery::WorkspaceHistoryAttachment& attachment
             : workspaceHistory.attachments)
        {
            const std::size_t depth =
                attachment.stack == editor::DocumentRecoveryHistoryStack::Undo
                ? (document_.undoDepth() > attachment.index
                    ? document_.undoDepth() - attachment.index : 0u)
                : (attachment.index < document_.redoDepth()
                    ? document_.undoDepth() + attachment.index + 1u : 0u);
            if (depth == 0u)
            {
                // The live Document applies its smaller interactive history
                // retention cap after checked import. A pruned distant record
                // cannot safely receive an attachment.
                continue;
            }
            const std::optional<editor::DocumentHistoryTransitionIdentity> identity =
                document_.historyTransitionIdentityAtDepth(depth);
            if (!identity || identity->recordId == 0u)
            {
                continue;
            }
            const std::uint64_t recordId = identity->recordId;
            if (std::find(preparedRecordIds.begin(), preparedRecordIds.end(), recordId)
                == preparedRecordIds.end())
            {
                preparedRecordIds.push_back(recordId);
            }

            if (attachment.cleave.isMap())
            {
                const QCborMap encodedCleave = attachment.cleave.toMap();
                const QCborValue workspaceValue = encodedCleave.value(
                    QStringLiteral("workspace"));
                const QCborValue selectionValue = encodedCleave.value(
                    QStringLiteral("documentSelection"));
                const QCborValue activeValue = encodedCleave.value(
                    QStringLiteral("active"));
                const QCborValue modeValue = encodedCleave.value(
                    QStringLiteral("mode"));
                if (!workspaceValue.isMap() || !selectionValue.isByteArray()
                    || !activeValue.isArray() || activeValue.toArray().size() != 7
                    || !modeValue.isArray() || modeValue.toArray().size() != 3)
                {
                    warn(QStringLiteral(
                        "A checked historical Cleave attachment could not be reconstructed; all imported workspace attachments were dropped."));
                    return;
                }

                CleaveApplyWorkspaceMemento memento{};
                memento.recordId = recordId;
                memento.documentGeneration = documentLifetimeGeneration_;
                const QCborMap workspace = workspaceValue.toMap();
                GlView::RecoveryState recovered{};
                if ( workspace.contains(QStringLiteral("peerWorkspace"))
                    || !cleaveSingleWorkspaceFromCbor(workspace, &recovered) )
                {
                    warn(QStringLiteral(
                        "A checked historical Cleave workspace could not be reconstructed; all imported workspace attachments were dropped."));
                    return;
                }
                const QCborValue family = workspace.value(QStringLiteral("currentFamily"));
                qint64 familyIndex = -1;
                if ( family.isUndefined() )
                {
                    memento.family = recovered.cleaveContext.valid
                        ? (recovered.cleaveContext.surfaceMode
                            ? GlView::CleaveWorkspaceFamily::Surface : GlView::CleaveWorkspaceFamily::Sector)
                        : (recovered.surfaceCleaveToolEnabled && !recovered.sectorCleaveToolEnabled
                            ? GlView::CleaveWorkspaceFamily::Surface : GlView::CleaveWorkspaceFamily::Sector);
                }
                else if ( !cborIntegerInRange(family, 0, 1, &familyIndex) )
                {
                    return;
                }
                else
                {
                    memento.family = static_cast<GlView::CleaveWorkspaceFamily>(familyIndex);
                }
                if ( !cleaveFamilyMatches(recovered.cleaveContext, recovered.cleaveHistory, memento.family) ) return;
                auto typed = takeRecoveredCleaveWorkspace(recovered);
                memento.draft = std::move(typed.draft);
                memento.history = std::move(typed.history);
                memento.bounded = typed.bounded;
                memento.stagedMode = typed.stagedMode;
                memento.chainEnabled = typed.chainEnabled;
                memento.measurementsVisible = typed.measurementsVisible;
                memento.measureUnit = typed.measureUnit;
                memento.anglesVisible = typed.anglesVisible;
                memento.lengthsVisible = typed.lengthsVisible;
                memento.lineNumbersVisible = typed.lineNumbersVisible;
                memento.angleStepDegrees = typed.angleStepDegrees;
                memento.shape = typed.shapeOptions;
                memento.transformMode = typed.transformMode;
                memento.sectorToolEnabled = recovered.sectorCleaveToolEnabled;
                memento.surfaceToolEnabled = recovered.surfaceCleaveToolEnabled;

                QString selectionError{};
                const std::optional<editor::Selection> selection =
                    ui::recovery::deserializeSelection(
                        selectionValue.toByteArray(), &selectionError);
                if (!selection)
                {
                    warn(QStringLiteral(
                        "A checked historical Cleave document selection could not be reconstructed; all imported workspace attachments were dropped."));
                    return;
                }
                memento.documentSelection = *selection;
                const QCborArray active = activeValue.toArray();
                const QCborArray modes = modeValue.toArray();
                std::array<std::int32_t, 7> activeItems{};
                for (qsizetype index = 0; index < active.size(); ++index)
                {
                    if (!cborInt32(active.at(index), &activeItems[static_cast<std::size_t>(index)]))
                    {
                        return;
                    }
                }
                qint64 editMode = -1;
                qint64 surfaceMode = -1;
                qint64 lightMode = -1;
                if (!cborIntegerInRange(modes.at(0), 0, 5, &editMode)
                    || !cborIntegerInRange(modes.at(1), 0, 2, &surfaceMode)
                    || !cborIntegerInRange(modes.at(2), 0, 3, &lightMode))
                {
                    warn(QStringLiteral(
                        "A checked historical Cleave editor mode could not be reconstructed; all imported workspace attachments were dropped."));
                    return;
                }
                memento.activeSector = activeItems[0];
                memento.activeSurfaceSector = activeItems[1];
                memento.activeSurface = activeItems[2];
                memento.activeThing = activeItems[3];
                memento.activeLight = activeItems[4];
                memento.activeFrameThing = activeItems[5];
                memento.activeFrameValue = activeItems[6];
                memento.editMode = static_cast<EditMode>(editMode);
                memento.surfaceSelectionMode =
                    static_cast<SurfaceSelectionMode>(surfaceMode);
                memento.lightEditSubMode = static_cast<LightEditSubMode>(lightMode);
                if (!estimateMementoBytes(memento, &memento.ownedBytes)
                    || memento.ownedBytes == 0u)
                {
                    warn(QStringLiteral(
                        "A checked historical Cleave attachment exceeded the owned-memory limit; all imported workspace attachments were dropped."));
                    return;
                }
                if (std::find_if(preparedCleave.begin(), preparedCleave.end(),
                        [recordId](const CleaveApplyWorkspaceMemento& previous)
                        {
                            return previous.recordId == recordId;
                        }) != preparedCleave.end())
                {
                    warn(QStringLiteral(
                        "Duplicate HIST records resolve to one Cleave workspace identity; all imported workspace attachments were dropped."));
                    return;
                }
                if (!addBytes(preparedBytes, 1u, memento.ownedBytes))
                {
                    warn(QStringLiteral(
                        "Historical Cleave attachments exceeded the owned-memory limit; all imported workspace attachments were dropped."));
                    return;
                }
                preparedCleave.push_back(std::move(memento));
            }

            if (attachment.layerVisuals.isMap())
            {
                const QCborMap encoded = attachment.layerVisuals.toMap();
                LayerVisualHistoryTransition transition{};
                transition.recordId = recordId;
                transition.beforeSignature = identity->beforeUndoSignature;
                transition.afterSignature = identity->afterUndoSignature;
                if (!decodeLayerSide(encoded.value(QStringLiteral("before")),
                        &transition.before)
                    || !decodeLayerSide(encoded.value(QStringLiteral("after")),
                        &transition.after))
                {
                    warn(QStringLiteral(
                        "A checked historical Layer presentation could not be reconstructed; all imported workspace attachments were dropped."));
                    return;
                }
                const std::uint64_t owned = layerVisualHistoryTransitionOwnedBytes(
                    transition.before, transition.after);
                if (!addBytes(preparedBytes, 1u, owned)
                    || preparedLayersByAfter.contains(transition.afterSignature)
                    || preparedLayersAfterByBefore.contains(transition.beforeSignature))
                {
                    warn(QStringLiteral(
                        "Historical Layer attachments exceeded the limit or duplicated a transition; all imported workspace attachments were dropped."));
                    return;
                }
                preparedLayersAfterByBefore.emplace(
                    transition.beforeSignature, transition.afterSignature);
                preparedLayersByAfter.emplace(
                    transition.afterSignature, std::move(transition));
            }
        }

        if (preparedCleave.empty() && preparedLayersByAfter.empty())
        {
            return;
        }
        if (!canFitCleaveWorkspaceAttachmentBudget(preparedBytes, false))
        {
            warn(QStringLiteral(
                "Historical Cleave/Layer workspace attachments exceed the current 64-entry/16 MiB limit and were dropped."));
            return;
        }

        std::vector<std::uint64_t> allRecordIds{};
        allRecordIds.reserve(cleaveApplyWorkspaceMementos_.size()
            + layerVisualHistoryByAfterSignature_.size()
            + preparedRecordIds.size());
        const auto addUniqueId = [&allRecordIds](const std::uint64_t id)
        {
            if (id != 0u
                && std::find(allRecordIds.begin(), allRecordIds.end(), id)
                    == allRecordIds.end())
            {
                allRecordIds.push_back(id);
            }
        };
        for (const CleaveApplyWorkspaceMemento& memento
             : cleaveApplyWorkspaceMementos_)
        {
            addUniqueId(memento.recordId);
        }
        for (const auto& [signature, transition] : layerVisualHistoryByAfterSignature_)
        {
            (void)signature;
            addUniqueId(transition.recordId);
        }
        for (const std::uint64_t id : preparedRecordIds)
        {
            addUniqueId(id);
        }
        if (allRecordIds.size() > 64u)
        {
            warn(QStringLiteral(
                "Historical Cleave/Layer workspace attachments exceed the 64-transition limit and were dropped."));
            return;
        }

        // Reserve every destination before publishing any candidate. The
        // unordered-map nodes are staged in same-allocator maps and transferred
        // by node merge after capacity is ready, so allocation failure leaves
        // the live attachment tables unchanged.
        cleaveApplyWorkspaceMementos_.reserve(
            cleaveApplyWorkspaceMementos_.size() + preparedCleave.size());
        layerVisualHistoryByAfterSignature_.reserve(
            layerVisualHistoryByAfterSignature_.size()
                + preparedLayersByAfter.size());
        layerVisualHistoryAfterByBeforeSignature_.reserve(
            layerVisualHistoryAfterByBeforeSignature_.size()
                + preparedLayersAfterByBefore.size());
    }
    catch (const std::bad_alloc&)
    {
        warn(QStringLiteral(
            "Historical Cleave/Layer workspace attachments could not be allocated and were dropped."));
        return;
    }
    catch (const std::length_error&)
    {
        warn(QStringLiteral(
            "Historical Cleave/Layer workspace attachments exceeded the container limit and were dropped."));
        return;
    }

    for (CleaveApplyWorkspaceMemento& memento : preparedCleave)
    {
        cleaveApplyWorkspaceMementos_.push_back(std::move(memento));
    }
    layerVisualHistoryByAfterSignature_.merge(preparedLayersByAfter);
    layerVisualHistoryAfterByBeforeSignature_.merge(
        preparedLayersAfterByBefore);

    if (workspaceHistory.pendingRedoIndex
        && *workspaceHistory.pendingRedoIndex == 0u
        && document_.redoDepth() > 0u)
    {
        const std::optional<std::uint64_t> recordId = document_.nextRedoRecordId();
        if (recordId)
        {
            const auto memento = std::find_if(
                cleaveApplyWorkspaceMementos_.begin(),
                cleaveApplyWorkspaceMementos_.end(),
                [recordId](const CleaveApplyWorkspaceMemento& value)
                {
                    return value.recordId == *recordId;
                });
            // pendingRedo is only a UI routing hint. HIST binding validates
            // that the attachment belongs to this exact RecordID; require
            // the saved current draft to equal that immutable pre-Apply
            // workspace as well, or a stale/forged hint could steal ordinary
            // local Undo/Redo priority after reopen.
            if ( memento != cleaveApplyWorkspaceMementos_.end() && glView_
                && glView_->cleaveWorkspaceBounded(memento->family) == memento->bounded.value_or(true) )
            {
                const auto liveDraft = glView_->stagedCleaveDraftView(memento->family);
                if ( liveDraft.context && sameCleaveAuthoredDraftFields(memento->draft,
                    liveDraft.lines, *liveDraft.context, liveDraft.chainOrigin, liveDraft.applyOrder) )
                {
                    pendingCleaveApplyRedoRecordId_ = *recordId;
                    pendingCleaveApplyRedoFamily_ = memento->family;
                    pendingCleaveApplyRedoAuthoredRevision_ =
                        glView_->stagedCleaveAuthoredRevision(memento->family);
                }
            }
        }
    }
    updateEditActions();
    syncCogScriptEditorActionFocus();
}

std::filesystem::path MainWindow::recoveryDirectory() const
{
    if (!recoveryDirectoryOverride_.empty())
    {
        return recoveryDirectoryOverride_;
    }
    return appRecoveryDirectoryPath();
}

std::vector<std::filesystem::path> MainWindow::recoverySearchDirectories() const
{
    if (!recoveryDirectoryOverride_.empty())
    {
        return { recoveryDirectoryOverride_ };
    }

    std::vector<std::filesystem::path> directories{};
    for (const std::filesystem::path& candidate : appRecoverySearchDirectoryPaths())
    {
        std::error_code existsError{};
        if (candidate == recoveryDirectory()
            || (std::filesystem::exists(candidate, existsError) && !existsError))
        {
            directories.push_back(candidate);
        }
    }
    return directories;
}

void MainWindow::observeCogScriptEditorRecoveryHosts()
{
    if ( recoveryShuttingDown_ )
    {
        return;
    }
    for ( const QPointer<QDockWidget>& dock : cogScriptEditorDocks() )
    {
        if ( !dock )
        {
            continue;
        }
        dock->setProperty("sed.cogScriptEditorRecoveryGeneration",
            QVariant::fromValue(static_cast<qulonglong>(resourceEditorDocumentContextGeneration_)));
        const auto currentHost = [this, dock]()
        {
            return !recoveryShuttingDown_ && dock
                && dock->property("sed.cogScriptEditorRecoveryGeneration").toULongLong()
                    == resourceEditorDocumentContextGeneration_;
        };
        if ( !dock->property("sed.cogScriptEditorRecoveryObserved").toBool() )
        {
            dock->setProperty("sed.cogScriptEditorRecoveryObserved", true);
            connect(dock, &QDockWidget::visibilityChanged, this, [this, currentHost](const bool)
                {
                    if ( currentHost() )
                    {
                        noteRecoveryStateChanged();
                    }
                });
            connect(dock, &QDockWidget::topLevelChanged, this, [this, currentHost](const bool)
                {
                    if ( currentHost() )
                    {
                        noteRecoveryStateChanged();
                    }
                });
            connect(dock, &QDockWidget::dockLocationChanged, this, [this, currentHost](const Qt::DockWidgetArea)
                {
                    if ( currentHost() )
                    {
                        noteRecoveryStateChanged();
                    }
                });
        }
        for ( QTabWidget* const tabs : dock->findChildren<QTabWidget*>() )
        {
            if ( !tabs->property("sed.cogScriptEditorHost").toBool()
                || tabs->property("sed.cogScriptEditorRecoveryObserved").toBool() )
            {
                continue;
            }
            tabs->setProperty("sed.cogScriptEditorRecoveryObserved", true);
            connect(tabs, &QTabWidget::currentChanged, this, [this, currentHost](const int)
                {
                    if ( !currentHost() )
                    {
                        return;
                    }
                    const QPointer<MainWindow> self(this);
                    updateCogScriptEditorFocusFromWidget(QApplication::focusWidget());
                    if ( self && currentHost() )
                    {
                        noteRecoveryStateChanged();
                    }
                });
            connect(tabs->tabBar(), &QTabBar::tabMoved, this, [this, currentHost](const int, const int)
                {
                    if ( currentHost() )
                    {
                        noteRecoveryStateChanged();
                    }
                });
        }
    }
}

void MainWindow::initializeRecoverySupport()
{
    recoveryPublicationGate_ = std::make_shared<ui::recovery::PublicationGate>();
    recoverySessionId_ = ui::recovery::newSessionId();
    switchRecoveryDocument(currentPath_, false, false);

    // Recovery encoding is throughput work, never interactive work. Isolate
    // it from the global pool used by resource/audio jobs and keep its worker
    // below GUI/audio priority so large HIST/FEC passes cannot steal an
    // audible frame from viewport interaction.
    recoveryThreadPool_ = new QThreadPool(this);
    recoveryThreadPool_->setMaxThreadCount(1);
    recoveryThreadPool_->setThreadPriority(QThread::LowPriority);
    recoveryThreadPool_->setExpiryTimeout(30'000);

    recoveryWriteWatcher_ = new QFutureWatcher<ui::recovery::WriteResult>(this);
    connect(recoveryWriteWatcher_, &QFutureWatcher<ui::recovery::WriteResult>::finished,
        recoveryWriteWatcher_, [this]() { handleRecoveryWriteFinished(); });

    recoveryTimer_ = new QTimer(this);
    recoveryTimer_->setSingleShot(false);
    connect(recoveryTimer_, &QTimer::timeout,
        this, [this]() { handleRecoveryTimerTimeout(); });

    recoveryDebounceTimer_ = new QTimer(this);
    recoveryDebounceTimer_->setObjectName(QStringLiteral("recoveryDebounceTimer"));
    recoveryDebounceTimer_->setSingleShot(true);
    recoveryDebounceTimer_->setInterval(kRecoveryDebounceMilliseconds);
    connect(recoveryDebounceTimer_, &QTimer::timeout,
        this, [this]() { scheduleRecoverySnapshot(); });

    recoveryRetryTimer_ = new QTimer(this);
    recoveryRetryTimer_->setSingleShot(true);
    connect(recoveryRetryTimer_, &QTimer::timeout,
        this, [this]()
        {
            const bool forceRetry = recoveryRetryForceCapture_;
            recoveryRetryForceCapture_ = false;
            scheduleRecoverySnapshot(forceRetry);
        });

    if (glView_)
    {
        connect(glView_, &GlView::viewportStateChanged,
            this, [this]() { noteRecoveryStateChanged(); });
        connect(glView_, &GlView::stagedCleaveAuthoredStateChanged,
            this, [this](const std::uint64_t revision)
            {
                noteCleaveAuthoredStateChanged(revision);
            });
        connect(glView_, &GlView::flyNavigationActiveChanged,
            this, [this](const bool active)
            {
                if (active)
                {
                    // Copying the coherent Level/undo/EDIT snapshot is the
                    // only recovery step that must touch GUI-owned state.
                    // Do not let a periodic/debounced capture interrupt a
                    // latency-sensitive fly/look gesture; encoding, FEC and
                    // disk publication already remain on the worker thread.
                    if (recoveryDebounceTimer_)
                    {
                        // Preserve whether authored work, rather than an
                        // editor-only camera change, was waiting on this
                        // debounce.  The distinction matters for a dirty
                        // document whose latest recovery checkpoint is
                        // already current.
                        recoveryAuthoredCaptureDeferredForFly_ =
                            recoveryAuthoredCaptureDeferredForFly_
                            || recoveryDebounceTimer_->isActive();
                        recoveryDebounceTimer_->stop();
                    }
                    recoveryCaptureDeferredForFly_ =
                        recoveryCaptureDeferredForFly_
                        || recoverySnapshotRequired_
                        || recoveryAuthoredCaptureDeferredForFly_;
                    return;
                }

                if (recoveryCaptureDeferredForFly_)
                {
                    const bool authoredRecoveryDeferred =
                        recoveryAuthoredCaptureDeferredForFly_;
                    recoveryCaptureDeferredForFly_ = false;
                    recoveryAuthoredCaptureDeferredForFly_ = false;
                    // A fly gesture normally changes only EDIT workspace
                    // state (camera/projection).  Do not immediately launch
                    // the expensive Level+HIST checkpoint for that case:
                    // editor-only changes are intentionally covered by the
                    // periodic recovery tick.  Authored edits that were
                    // already pending before fly mode still get the short
                    // post-gesture debounce so a hard termination cannot
                    // leave the Level window unnecessarily exposed.
                    if (authoredRecoveryDeferred
                        && recoveryDebounceTimer_
                        && recoveryTimer_ && recoveryTimer_->isActive())
                    {
                        recoveryDebounceTimer_->start();
                    }
                }
            });
    }
    if (outlinerTabs_)
    {
        connect(outlinerTabs_, &QTabBar::currentChanged,
            this, [this](int) { noteRecoveryStateChanged(); });
    }
    if (checksTabs_)
    {
        connect(checksTabs_, &QTabWidget::currentChanged,
            this, [this](int) { noteRecoveryStateChanged(); });
    }
    for (QSplitter* splitter : findChildren<QSplitter*>())
    {
        if (splitter)
        {
            connect(splitter, &QSplitter::splitterMoved,
                this, [this](int, int) { noteRecoveryStateChanged(); });
        }
    }
    observeCogScriptEditorRecoveryHosts();
    connect(qApp, &QGuiApplication::focusWindowChanged, this, [this](QWindow*)
        {
            if ( !recoveryShuttingDown_ )
            {
                updateCogScriptEditorFocusFromWidget(QApplication::focusWidget());
            }
        });
    for (const char* objectName : {
            "uvEditorRepeatButton", "uvEditorSnapButton", "uvEditorViewportFacingButton" })
    {
        if (auto* button = findChild<QToolButton*>(QString::fromLatin1(objectName)))
        {
            connect(button, &QToolButton::toggled,
                this, [this](bool) { noteRecoveryStateChanged(); });
        }
    }
    if (auto* grid = findChild<QDoubleSpinBox*>(QStringLiteral("uvEditorGridStepSpin")))
    {
        connect(grid, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { noteRecoveryStateChanged(); });
    }
    if (auto* subGrid = findChild<QCheckBox*>(QStringLiteral("uvEditorSubGridCheck")))
    {
        connect(subGrid, &QCheckBox::toggled,
            this, [this](bool) { noteRecoveryStateChanged(); });
    }
    if (auto* tileGrid = findChild<QCheckBox*>(
            QStringLiteral("uvEditorRepeatTileGridCheck")))
    {
        connect(tileGrid, &QCheckBox::toggled,
            this, [this](bool) { noteRecoveryStateChanged(); });
    }
    const auto connectDock = [this](QDockWidget* dock)
    {
        if ( !dock || dock->property("sed.cogScriptEditorDock").toBool() )
        {
            return;
        }
        connect(dock, &QDockWidget::visibilityChanged,
            this, [this](bool) { noteRecoveryStateChanged(); });
        connect(dock, &QDockWidget::topLevelChanged,
            this, [this](bool) { noteRecoveryStateChanged(); });
        connect(dock, &QDockWidget::dockLocationChanged,
            this, [this](Qt::DockWidgetArea) { noteRecoveryStateChanged(); });
    };
    for (QDockWidget* dock : findChildren<QDockWidget*>()) connectDock(dock);
    if (workspaceWindow_)
    {
        for (QDockWidget* dock : workspaceWindow_->findChildren<QDockWidget*>()) connectDock(dock);
    }
    configureRecoveryTimerFromSettings();
    // MainWindow construction has now installed the final viewport, dock,
    // splitter, tool, and preference state. The earlier undo/title marker in
    // core.cpp intentionally does not freeze a partial EDIT baseline.
    markRecoveryWorkspaceAsSaved();

    // Publish an initial coherent baseline even when the document has no undo
    // record and no modified COG tab. The queued invocation runs after the
    // remaining MainWindow/GlView setup has completed, while the regular
    // setting still controls whether autosave is enabled.
    if (recoveryAutomaticBaselineEnabled_
        && recoveryTimer_ && recoveryTimer_->isActive())
    {
        QTimer::singleShot(0, this, [this]() { scheduleRecoverySnapshot(true); });
    }
}

bool MainWindow::hasRecoverableChanges() const
{
    // Keep a clean baseline checkpoint as well. A hard process termination
    // cannot run Qt/Windows callbacks, so the last successfully published
    // snapshot is the only protection for the interval before termination.
    return recoverySnapshotRequired_ || hasUnsavedChanges() || hasUnsavedCogScriptChanges();
}

bool MainWindow::hasRecoveryOfferEligibleChanges() const
{
    refreshCleaveAuthoredDirtyCache();
    if (recoveryCleaveAuthoredDirty_)
    {
        return true;
    }
    const bool authoredLevelDirty = recoveryRequiresSave_
        || document_.undoSignature() != savedUndoSignature_;
    if (authoredLevelDirty)
    {
        return true;
    }
    if (!resourceEditorAuthoredStateMatchesSavedBaseline())
    {
        return true;
    }

    try
    {
        const QByteArray authoredCogState = captureAuthoredCogRecoveryState();
        const QByteArray authoredCogHash = QCryptographicHash::hash(
            authoredCogState, QCryptographicHash::Sha256);
        if (recoverySavedCogStateHash_.isEmpty())
        {
            if (hasUnsavedCogScriptChanges()) return true;
        }
        else if (authoredCogHash != recoverySavedCogStateHash_)
        {
            return true;
        }
    }
    catch (...)
    {
        // If the authored COG state cannot be compared, preserve a genuinely
        // modified buffer rather than misclassifying it as editor-only.
        if (hasUnsavedCogScriptChanges()) return true;
    }

    // The undo signature is authoritative for normal editing paths. Keep the
    // canonical Level comparison as a safety net for authored-property paths
    // that update serialized data without adding a distinct undo record.
    if (!recoverySavedLevelHashKnown_)
    {
        return recoveryAutomaticBaselineEnabled_;
    }
    try
    {
        const QByteArray currentLevelHash = recoveryCurrentLevelStateHash();
        const bool authoredLevelChanged = currentLevelHash.isEmpty()
            || currentLevelHash != recoverySavedLevelStateHash_;
        if (!authoredLevelChanged)
        {
            // A same-undo preview may have set the cheap pending marker while
            // the user subsequently returned to the saved Level. Do not let
            // that stale marker label the next clean checkpoint recoverable.
            recoveryAuthoredMutationPending_ = false;
            recoveryAuthoredLevelDirty_ = false;
        }
        return authoredLevelChanged;
    }
    catch (...)
    {
        // Failure to prove that the Level still matches its saved baseline is
        // conservative authored-work eligibility, never editor-only dirtiness.
        return true;
    }
}

void MainWindow::handleRecoveryTimerTimeout()
{
    // Frequent live checkpoints protect both authored data and editor-only
    // workspace state. Historical autosaves, however, are authored-document
    // restore points: camera/tool/layout/UV changes alone must not create one.
    // Keep this periodic path cheap. Exact Level-vs-saved-baseline hashing is
    // reserved for user-triggered replacement/close decisions; authored undo
    // markers, COG dirtiness, and the explicit same-undo token are sufficient
    // to decide whether to rotate a historical autosave here.
    if (glView_ && glView_->flyNavigationActive())
    {
        recoveryCaptureDeferredForFly_ = true;
        return;
    }
    (void)resourceEditorAuthoredStateMatchesSavedBaseline();
    const bool cheapAuthoredRecoveryPending = hasUnsavedChanges()
        || hasUnsavedCogScriptChanges()
        || recoveryAuthoredLevelDirty_;
    if (cheapAuthoredRecoveryPending)
    {
        recoveryArchivePreviousOnNextSnapshot_ = true;
        scheduleRecoverySnapshot(true);
        return;
    }
    scheduleRecoverySnapshot();
}

void MainWindow::noteRecoveryStateChanged()
{
    noteRecoveryStateChanged(false);
}

void MainWindow::noteCleaveAuthoredStateChanged(const std::uint64_t revision)
{
    if (pendingCleaveApplyRedoRecordId_
        && (!glView_ || glView_->stagedCleaveAuthoredRevision(pendingCleaveApplyRedoFamily_)
            != pendingCleaveApplyRedoAuthoredRevision_))
    {
        pendingCleaveApplyRedoRecordId_.reset();
        pendingCleaveApplyRedoAuthoredRevision_ = 0u;
        syncCogScriptEditorActionFocus();
    }
    if (!glView_ || revision == recoveryCleaveAuthoredRevision_)
    {
        return;
    }

    // The dedicated GlView signal is emitted only for settled draft edits or
    // local timeline navigation. Refresh the small authored projection here;
    // viewport camera/selection notifications remain on the O(1) cached path.
    refreshCleaveAuthoredDirtyCache();
    refreshWindowTitle();
    if (recoveryShuttingDown_ || !recoveryAutomaticBaselineEnabled_)
    {
        return;
    }

    ++recoveryEventSerial_;
    if (!recoveryTimer_ || !recoveryTimer_->isActive())
    {
        return;
    }
    recoverySnapshotRequired_ = true;

    if (glView_->flyNavigationActive())
    {
        recoveryCaptureDeferredForFly_ = true;
        recoveryAuthoredCaptureDeferredForFly_ = true;
        if (recoveryDebounceTimer_)
        {
            recoveryDebounceTimer_->stop();
        }
        return;
    }

    if (!recoveryLastPublishedValid_
        && (!recoveryWriteWatcher_ || !recoveryWriteWatcher_->isRunning()))
    {
        if (!recoveryImmediateSnapshotPending_)
        {
            recoveryImmediateSnapshotPending_ = true;
            QTimer::singleShot(0, this, [this]()
                {
                    recoveryImmediateSnapshotPending_ = false;
                    scheduleRecoverySnapshot();
                });
        }
        return;
    }
    if (recoveryDebounceTimer_)
    {
        recoveryDebounceTimer_->start();
    }
}

void MainWindow::noteRecoveryEditorStateChangedIfNeeded()
{
    if (!recoveryAutomaticBaselineEnabled_ || recoveryShuttingDown_)
    {
        return;
    }
    if (glView_ && glView_->flyNavigationActive())
    {
        // Key releases are still delivered through MainWindow while RMB fly
        // owns input. Avoid even the compact EDIT capture until the gesture
        // ends; the fly-state callback restarts the debounce.
        recoveryCaptureDeferredForFly_ = true;
        return;
    }

    // EDIT equality is checked once at the debounced snapshot boundary. This
    // callback is shared by viewport focus/key events and editor input, so it
    // must never serialize every open table just to discover unchanged bytes.
    noteRecoveryStateChanged(false);
}

void MainWindow::noteRecoveryStateChanged(const bool semanticMutation)
{
    if (recoveryShuttingDown_)
    {
        return;
    }

    // Generic MainWindow integration fixtures deliberately do not opt into
    // the asynchronous baseline writer.  Recovery-specific test helpers turn
    // this on before exercising recovery; production builds default it on.
    if (!recoveryAutomaticBaselineEnabled_)
    {
        return;
    }

    // The event serial is a semantic-generation discriminator for queued and
    // in-flight snapshots. Camera/layout/tool notifications on a clean
    // document can arrive repeatedly without changing the captured bytes, so
    // they must not bump this serial; the EDIT payload hash still captures a
    // real workspace-only change when the periodic checkpoint runs.
    // Most callers reach this hook from updateEditActions(), which is also
    // refreshed for camera/tool/layout-only changes.  Infer an authored edit
    // from the cheap undo/redo signatures plus the existing dirty marker so
    // those UI refreshes do not all enter the expensive 650 ms full-level
    // capture path.  The explicit semantic flag remains authoritative for
    // lifecycle/transaction callers whose mutation does not create a normal
    // history signature.
    const std::uint64_t currentUndoSignature = document_.undoSignature();
    const std::uint64_t currentRedoSignature = document_.redoSignature();
    const bool signaturesDifferFromPublished = recoveryLastPublishedValid_
        && (currentUndoSignature != recoveryLastPublishedUndoSignature_
            || currentRedoSignature != recoveryLastPublishedRedoSignature_);
    const bool signaturesDifferFromInFlight = recoveryWriteWatcher_
        && recoveryWriteWatcher_->isRunning()
        && (currentUndoSignature != recoveryInFlightUndoSignature_
            || currentRedoSignature != recoveryInFlightRedoSignature_);
    const bool inferredSemanticMutation =
        (hasUnsavedChanges() || hasUnsavedCogScriptChanges())
        && (signaturesDifferFromPublished || signaturesDifferFromInFlight);
    const bool historyAlreadyNoted = recoveryLastNotedSemanticHistoryValid_
        && currentUndoSignature == recoveryLastNotedSemanticUndoSignature_
        && currentRedoSignature == recoveryLastNotedSemanticRedoSignature_;
    // A single history transition can cause several refreshes (document
    // notifications, outliner rebuilds, and the final action-state pass).
    // Only the first inferred transition advances the canonical generation.
    // An explicit semantic notification is still allowed to advance when it
    // represents an in-place edit that deliberately kept the same history
    // record (the inferred path is false in that case).
    const bool newInferredSemanticMutation =
        inferredSemanticMutation && !historyAlreadyNoted;
    const bool newExplicitSemanticMutation =
        semanticMutation && (!historyAlreadyNoted || !inferredSemanticMutation);
    const bool authoredMutation =
        newExplicitSemanticMutation || newInferredSemanticMutation;
    if (authoredMutation)
    {
        ++recoverySemanticMutationSerial_;
        ++recoveryEventSerial_;
        recoveryLastNotedSemanticUndoSignature_ = currentUndoSignature;
        recoveryLastNotedSemanticRedoSignature_ = currentRedoSignature;
        recoveryLastNotedSemanticHistoryValid_ = true;
    }

    // The test seam can mutate the live DTO directly to exercise same-undo
    // safety. Production mutation paths use the explicit semantic hook or a
    // changed history signature; the worker's immutable document snapshot and
    // returned digest remain authoritative without a GUI-thread Level hash.
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    const bool possibleInPlaceAuthoredMutation = recoveryTestMutationPending_;
    recoveryTestMutationPending_ = false;
#else
    constexpr bool possibleInPlaceAuthoredMutation = false;
#endif
    const bool historyReturnedToClean = !hasUnsavedChanges()
        && !hasUnsavedCogScriptChanges()
        && (signaturesDifferFromPublished || signaturesDifferFromInFlight);
    if (historyReturnedToClean)
    {
        // Undo-to-saved (including a redo-only branch) changes the recovery
        // timeline but is not an authored dirty Level. Drop an older cheap
        // dirty marker before the next periodic rotation; the HIST payload is
        // still captured because the signatures/event serial changed.
        recoveryAuthoredMutationPending_ = false;
        recoveryAuthoredLevelDirty_ = false;
    }
    // Cleave draft dirtiness has its own revision/hash path and must not
    // poison the Level-only cheap marker. Generic viewport notifications are
    // common during local Cleave Undo/Redo; including hasUnsavedChanges()
    // here leaves recovery marked authored after the draft returns to its
    // saved baseline. Explicit semantic notifications still cover same-undo
    // Level, COG, and resource edits.
    const bool nonCleaveAuthoredDirty = recoveryRequiresSave_
        || document_.undoSignature() != savedUndoSignature_
        || resourceEditorAuthoredGeneration_
            != recoverySavedResourceEditorGeneration_
        || hasUnsavedCogScriptChanges();
    const bool authoredDirtySignal = !historyReturnedToClean
        && (possibleInPlaceAuthoredMutation
            || semanticMutation
            || nonCleaveAuthoredDirty);
    if ((authoredMutation || possibleInPlaceAuthoredMutation)
        && !historyReturnedToClean)
    {
        recoveryAuthoredMutationPending_ = true;
    }
    if (authoredDirtySignal)
    {
        recoveryAuthoredLevelDirty_ = true;
    }
    if (possibleInPlaceAuthoredMutation && !authoredMutation)
    {
        ++recoveryEventSerial_;
    }

    if (!recoveryTimer_ || !recoveryTimer_->isActive())
    {
        return;
    }
    recoverySnapshotRequired_ = true;

    if (glView_ && glView_->flyNavigationActive())
    {
        recoveryCaptureDeferredForFly_ = true;
        // This mutation was observed while fly navigation owned input. Keep
        // the authored debounce intent separate from the generic EDIT-only
        // deferred flag so a dirty-but-already-published document does not
        // trigger a full Level+HIST encode merely because its camera moved.
        if (authoredMutation || possibleInPlaceAuthoredMutation)
        {
            recoveryAuthoredCaptureDeferredForFly_ = true;
        }
        if (recoveryDebounceTimer_)
        {
            recoveryDebounceTimer_->stop();
        }
        return;
    }

    // The first recoverable state is published immediately. Authored Level,
    // COG, and history mutations use the short debounce so the safety copy is
    // available promptly. EDIT-only camera/tool/layout changes remain marked
    // required but wait for the periodic checkpoint (or shutdown flush); this
    // avoids launching a complete Level+HIST encode after every orbit/fly
    // gesture while still preserving the eventual editor workspace state.
    if (!recoveryLastPublishedValid_
        && (!recoveryWriteWatcher_ || !recoveryWriteWatcher_->isRunning()))
    {
        if (!recoveryImmediateSnapshotPending_)
        {
            recoveryImmediateSnapshotPending_ = true;
            QTimer::singleShot(0, this, [this]()
                {
                    recoveryImmediateSnapshotPending_ = false;
                    scheduleRecoverySnapshot();
                });
        }
        return;
    }
    if (authoredMutation && recoveryDebounceTimer_)
    {
        recoveryDebounceTimer_->start();
    }
}

void MainWindow::flushRecoverySnapshot(const bool forceFinalCapture)
{
    const bool writerRunning = recoveryWriteWatcher_ && recoveryWriteWatcher_->isRunning();
    if (!hasRecoverableChanges() && !forceFinalCapture
        && !writerRunning && !recoverySnapshotQueued_)
    {
        return;
    }
    recoveryFlushDraining_ = true;
    // Drain until the newest queued generation is published. A fixed retry
    // count can leave the final state unpublished when shutdown races a large
    // in-flight snapshot and its queued successor.
    int immediateFailureRetries = 0;
    bool forceCapture = forceFinalCapture;
    for (;;)
    {
        // A failed forced write must remain forced on the immediate shutdown
        // retry. Otherwise the canonical-key dedupe can mistake the older
        // published file for the just-failed final capture.
        forceCapture = forceCapture || recoveryRetryForceCapture_
            || recoverySnapshotQueued_ || recoveryForcedSnapshotQueued_;
        recoveryRetryForceCapture_ = false;
        scheduleRecoverySnapshot(forceCapture);
        forceCapture = false;
        if (recoveryWriteWatcher_ && recoveryWriteWatcher_->isRunning())
        {
            recoveryWriteWatcher_->waitForFinished();
        }
        // Deliver only the recovery watcher's queued completion events.
        // QFutureWatcher first receives a private FutureCallOut event and
        // emits `finished` while processing that event; with the same-thread
        // AutoConnection used above, the handler normally runs directly. The
        // second targeted pass is defensive for a queued delivery variant.
        // Flushing from MainWindow::~MainWindow() happens while child widgets
        // and recovery fields are still being torn down; a global event pump
        // would let timers, plugins, or a queued Open/New action re-enter a
        // partially destructed window. Targeted delivery handles the watcher
        // stages without admitting unrelated UI work.
        if (recoveryWriteWatcher_)
        {
            QCoreApplication::sendPostedEvents(
                recoveryWriteWatcher_);
            // The FutureCallOut handler can enqueue the signal callback while
            // the first targeted drain is running. A second pass is bounded
            // and consumes that callback without broad event-loop reentry.
            QCoreApplication::sendPostedEvents(recoveryWriteWatcher_);
        }
        if (!recoverySnapshotQueued_
            && (!recoveryWriteWatcher_ || !recoveryWriteWatcher_->isRunning()))
        {
            if (recoveryRetryTimer_ && recoveryRetryTimer_->isActive()
                && immediateFailureRetries < kRecoveryShutdownImmediateRetries)
            {
                recoveryRetryTimer_->stop();
                forceCapture = true;
                ++immediateFailureRetries;
                continue;
            }
            break;
        }
    }
    recoveryFlushDraining_ = false;
}

void MainWindow::prepareForSystemShutdown()
{
    recoverySystemShutdown_ = true;
    if (recoveryTimer_) recoveryTimer_->stop();
    if (recoveryDebounceTimer_) recoveryDebounceTimer_->stop();
    if (recoveryRetryTimer_) recoveryRetryTimer_->stop();
    // Even a clean baseline must be freshly captured here: shutdown can
    // arrive after an editor-only mutation that did not emit a notification,
    // and the canonical-key dedupe must not suppress this final coherent
    // document read.
    flushRecoverySnapshot(true);
}

void MainWindow::configureRecoveryTimerFromSettings()
{
    if (!recoveryTimer_)
    {
        return;
    }

    const QSettings settings = settingsStore();
    const bool enabled = settings.value(QStringLiteral("recovery/autosave_enabled"), true).toBool();
    const int minutes = std::clamp(
        settings.value(
            QStringLiteral("recovery/autosave_interval_minutes"),
            kDefaultRecoveryIntervalMinutes).toInt(),
        kMinimumRecoveryIntervalMinutes,
        kMaximumRecoveryIntervalMinutes);
    recoveryTimer_->setInterval(minutes * 60 * 1000);
    if (enabled && !recoveryShuttingDown_)
    {
        recoveryTimer_->start();
    }
    else
    {
        recoveryTimer_->stop();
    }
}

QByteArray MainWindow::captureRecoveryEditorState() const
{
    recoveryEditorStateCaptureError_.clear();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    ++recoveryEditorStateCaptureCountForTest_;
#endif
    if (!glView_)
    {
        return {};
    }

    QCborArray visible{};
    for (const bool value : layerVisible_)
    {
        visible.append(value);
    }
    QCborArray locked{};
    for (const bool value : layerLocked_)
    {
        locked.append(value);
    }
    QCborArray colors{};
    for (const QColor& color : layerColors_)
    {
        colors.append(static_cast<qint64>(color.rgba()));
    }

    const std::optional<SurfaceRef> activeSurface = glView_->selectedSurfaceRef();
    // Selection sets are intentionally unordered, while the frame editor has
    // one primary value. Capture that primary only when it is part of the
    // selected-frame set; otherwise emit the canonical no-primary sentinel.
    std::int32_t activeFrameThing = -1;
    std::int32_t activeFrameValue = -1;
    const std::int32_t candidateFrameThing = glView_->activeThingIndex();
    const std::int32_t candidateFrameValue = glView_->activeFrameValueIndex();
    if (candidateFrameThing >= 0 && candidateFrameValue >= 0
        && document_.selection().frames.contains(
            level::FrameRef{ candidateFrameThing, candidateFrameValue }))
    {
        activeFrameThing = candidateFrameThing;
        activeFrameValue = candidateFrameValue;
    }
    QCborMap root{};
    // EDIT is a complete workspace snapshot.  This is the first released
    // schema, so every field below is part of V1 and future incompatible
    // shapes must use a new version rather than being guessed at load time.
    // GlView's Select enum is also its neutral sentinel, so the explicit
    // transformActive bit distinguishes an active Select tool from no tool.
    root.insert(QStringLiteral("version"), kRecoveryEditorStateVersion);
    root.insert(QStringLiteral("engineExportPath"),
        QString::fromStdWString(lastEngineExportPath_.wstring()));
    root.insert(QStringLiteral("selection"),
        ui::recovery::serializeSelection(document_.selection()));
    root.insert(QStringLiteral("active"), QCborArray{
        glView_->activeSectorIndex(),
        activeSurface ? activeSurface->sector : -1,
        activeSurface ? activeSurface->surface : -1,
        glView_->activeThingIndex(),
        glView_->activeLightIndex(),
        activeFrameThing,
        activeFrameValue,
    });
    root.insert(QStringLiteral("edit"), QCborArray{
        static_cast<int>(editMode_),
        static_cast<int>(surfaceSelectionMode_),
        static_cast<int>(lightEditSubMode_),
    });

    const Level& level = document_.level();
    const auto serializeSurfaceRefs = [&level](const auto& references)
    {
        std::vector<SurfaceRef> sorted{};
        sorted.reserve(references.size());
        for (const SurfaceRef reference : references)
        {
            if (reference.sector >= 0 && reference.surface >= 0
                && static_cast<std::size_t>(reference.sector) < level.sectors.size()
                && static_cast<std::size_t>(reference.surface)
                    < level.sectors[static_cast<std::size_t>(reference.sector)]
                        .surfaces.size())
            {
                sorted.push_back(reference);
            }
        }
        std::sort(sorted.begin(), sorted.end(), [](const SurfaceRef left,
                                                   const SurfaceRef right)
        {
            return left.sector < right.sector
                || (left.sector == right.sector && left.surface < right.surface);
        });
        QCborArray encoded{};
        for (const SurfaceRef reference : sorted)
        {
            encoded.append(QCborArray{ reference.sector, reference.surface });
        }
        return encoded;
    };
    QCborMap surfaceSelectionContext{};
    surfaceSelectionContext.insert(QStringLiteral("version"),
        kSurfaceSelectionContextVersion);
    surfaceSelectionContext.insert(QStringLiteral("strictScope"),
        serializeSurfaceRefs(strictSurfaceSelectionScope_));
    surfaceSelectionContext.insert(QStringLiteral("rememberedFaceSelection"),
        serializeSurfaceRefs(rememberedSurfaceFaceSelection_));
    root.insert(QStringLiteral("surfaceSelectionContext"),
        surfaceSelectionContext);

    QCborMap layers{};
    layers.insert(QStringLiteral("visible"), visible);
    layers.insert(QStringLiteral("locked"), locked);
    layers.insert(QStringLiteral("colors"), colors);
    layers.insert(QStringLiteral("wireframeColorByLayer"), wireframeColorByLayer_);
    layers.insert(QStringLiteral("outlinerColorByLayer"), outlinerColorByLayer_);
    layers.insert(QStringLiteral("showHiddenInOutliner"), outlinerShowHiddenLayerEntities_);
    root.insert(QStringLiteral("layers"), layers);
    root.insert(QStringLiteral("previewOverlays"), QCborArray{
        previewShowEditModeOverlay_, previewShowTransformOverlay_, previewShowToolsOverlay_ });
    QList<int> capturedViewportUvSplitterSizes = viewportUvSplitter_
        ? viewportUvSplitter_->sizes()
        : QList<int>{};
    if (!documentEditingAllowed()
        && previewEditingUiVisibility_.valid
        && previewEditingUiVisibility_.documentEpoch == recoveryDocumentEpoch_
        && viewportUvSplitter_
        && previewEditingUiVisibility_.uvEditorSplitterSizes.size()
            == viewportUvSplitter_->count())
    {
        // Preview hides the UV child and QSplitter redistributes its extent.
        // EDIT must serialize the logical Editor allocation, not that
        // transient hidden-pipeline geometry.
        capturedViewportUvSplitterSizes =
            previewEditingUiVisibility_.uvEditorSplitterSizes;
    }
    QCborArray splitterSizes{};
    for (const int size : capturedViewportUvSplitterSizes)
    {
        splitterSizes.append(size);
    }
    QCborMap window{};
    window.insert(QStringLiteral("geometry"), saveGeometry());
    window.insert(QStringLiteral("state"), saveState(kRecoveryMainWindowStateVersion));
    QByteArray templateEditorGeometry{};
    if (QDialog* templateEditor = findChild<QDialog*>(
            QStringLiteral("templateEditorDialog"));
        templateEditor && templateEditor->isVisible())
    {
        templateEditorGeometry = templateEditor->saveGeometry();
    }
    else
    {
        templateEditorGeometry = settingsStore().value(
            QStringLiteral("templates/editor_geometry")).toByteArray();
    }
    if (!templateEditorGeometry.isEmpty())
    {
        window.insert(QStringLiteral("templateEditorGeometry"), templateEditorGeometry);
    }
    window.insert(QStringLiteral("workspace"), workspaceWindow_
        ? workspaceWindow_->saveState(kRecoveryWorkspaceStateVersion) : QByteArray{});
    window.insert(QStringLiteral("splitter"), splitterSizes);
    window.insert(QStringLiteral("outlinerTab"), outlinerTabs_ ? outlinerTabs_->currentIndex() : 0);
    window.insert(QStringLiteral("checksTab"), checksTabs_ ? checksTabs_->currentIndex() : 0);
    QCborMap namedSplitters{};
    for (const QSplitter* splitter : findChildren<QSplitter*>())
    {
        if (!splitter || splitter->objectName().isEmpty())
        {
            continue;
        }
        QCborArray sizes{};
        const QList<int> currentSizes = splitter == viewportUvSplitter_
            ? capturedViewportUvSplitterSizes
            : splitter->sizes();
        for (const int size : currentSizes)
        {
            sizes.append(size);
        }
        namedSplitters.insert(splitter->objectName(), sizes);
    }
    window.insert(QStringLiteral("splitters"), namedSplitters);
    window.insert(QStringLiteral("uvDock"), static_cast<int>(uvEditorDockPosition_));
    window.insert(QStringLiteral("uvExpanded"), uvEditorPanelExpanded_);
    window.insert(QStringLiteral("uvExtent"), uvEditorPanelExpandedHeight_);
    window.insert(QStringLiteral("uvViewportFacing"), uvEditorViewportFacing_);
    window.insert(QStringLiteral("uvScaleLinked"), uvScaleLinked_);
    const auto* repeat = findChild<QToolButton*>(QStringLiteral("uvEditorRepeatButton"));
    const auto* snap = findChild<QToolButton*>(QStringLiteral("uvEditorSnapButton"));
    const auto* grid = findChild<QDoubleSpinBox*>(QStringLiteral("uvEditorGridStepSpin"));
    const auto* subGrid = findChild<QCheckBox*>(QStringLiteral("uvEditorSubGridCheck"));
    const auto* uvMinorStyle = findChild<QComboBox*>(
        QStringLiteral("uvEditorSubGridStyleCombo"));
    const auto* uvMinorDotSize = findChild<QDoubleSpinBox*>(
        QStringLiteral("uvEditorSubGridDotSizeSpin"));
    const auto* tileGrid = findChild<QCheckBox*>(
        QStringLiteral("uvEditorRepeatTileGridCheck"));
    window.insert(QStringLiteral("uvRepeat"), repeat ? repeat->isChecked() : false);
    window.insert(QStringLiteral("uvSnap"), snap ? snap->isChecked() : false);
    window.insert(QStringLiteral("uvGridStep"), grid ? grid->value() : 0.25);
    window.insert(QStringLiteral("uvSubGrid"), subGrid && subGrid->isChecked());
    QCborMap uvGridAppearance{};
    uvGridAppearance.insert(QStringLiteral("minorDots"),
        uvMinorStyle && uvMinorStyle->currentData().toBool());
    uvGridAppearance.insert(QStringLiteral("minorDotSizePx"),
        uvMinorDotSize ? uvMinorDotSize->value() : 2.0);
    window.insert(QStringLiteral("uvGridAppearance"), uvGridAppearance);
    window.insert(QStringLiteral("uvRepeatTileGrid"),
        tileGrid && tileGrid->isChecked());
    window.insert(QStringLiteral("uvState"), captureUvEditorWorkspaceState());
    root.insert(QStringLiteral("window"), window);

    GlView::RecoveryState viewportState = glView_->recoveryState();
    QString activeModificationTool{};
    bool transformActive = false;
    QString activeTransformTool{};
    QString suspendedTransformTool{};
    const auto transformNameForAction = [](const QAction* action) -> QString
    {
        if (action == nullptr)
        {
            return {};
        }
        if (action->objectName() == QStringLiteral("viewport.transform.select")
            || action->objectName() == QStringLiteral("viewport.transform.move")
            || action->objectName() == QStringLiteral("viewport.transform.rotate")
            || action->objectName() == QStringLiteral("viewport.transform.scale"))
        {
            return action->objectName();
        }
        return {};
    };
    const bool capturingPreview = glView_
        && glView_->renderPipeline()
            == GlView::RenderPipeline::GamePreview;
    for (QAction* action : std::array<QAction*, 4>{
             viewportTransformSelectAction_,
             viewportTransformMoveAction_,
             viewportTransformRotateAction_,
             viewportTransformScaleAction_ })
    {
        // Preview's checked Select is an input sentinel, not an Editor
        // transform owner.  Keep the existing V1 no-owner representation for
        // Preview checkpoints instead of serializing a disabled tool as if it
        // were active.
        if (!capturingPreview
            && action && action->isCheckable() && action->isChecked())
        {
            transformActive = true;
            activeTransformTool = transformNameForAction(action);
            break;
        }
    }
    // Repair an internally staged state where GlView owns a non-Select
    // transform but its QAction was not synchronized. The fallback is mapped
    // to an explicit V1 name; an active transform with an empty name is never
    // emitted by the canonical capture.
    if (!transformActive && glView_
        && glView_->transformTool() != GlView::TransformTool::Select)
    {
        transformActive = true;
        switch (glView_->transformTool())
        {
            case GlView::TransformTool::Move:
                activeTransformTool = QStringLiteral("viewport.transform.move");
                break;
            case GlView::TransformTool::Rotate:
                activeTransformTool = QStringLiteral("viewport.transform.rotate");
                break;
            case GlView::TransformTool::Scale:
                activeTransformTool = QStringLiteral("viewport.transform.scale");
                break;
            case GlView::TransformTool::Select:
            default:
                transformActive = false;
                break;
        }
    }
    // QActionGroup is intentionally non-exclusive, but recovery must not
    // persist an arbitrary future QAction (or a menu action that happens to
    // be checked).  Keep the V1 interaction-owner contract explicit.
    for (QAction* action : std::array<QAction*, 5>{
             viewportSectorCleaveAction_,
             viewportSectorConnectAction_,
             viewportSectorThrustAction_,
             viewportSurfaceCleaveAction_,
             viewportFrameAddAction_ })
    {
        if (action && action->isCheckable() && action->isChecked())
        {
            activeModificationTool = action->objectName();
            break;
        }
    }

    // If a GlView tool flag exists without its QAction being checked, capture
    // the authoritative pointer owner so the complete V1 snapshot repairs the
    // visible state when it is reloaded.
    if (activeModificationTool.isEmpty() && glView_)
    {
        if (glView_->sectorCleaveToolEnabled() && viewportSectorCleaveAction_)
        {
            activeModificationTool = viewportSectorCleaveAction_->objectName();
        }
        else if (glView_->surfaceCleaveToolEnabled() && viewportSurfaceCleaveAction_)
        {
            activeModificationTool = viewportSurfaceCleaveAction_->objectName();
        }
    }
    // Join and Surface Extrude previews own transient geometry and cannot be
    // replayed from a checkpoint. Normalize those previews away. If they had
    // suspended a transform, promote that remembered action to the canonical
    // active transform so recovery does not silently lose the user's tool.
    const bool normalizedTransientPreview = sectorJoinPreviewActive_
        || surfaceExtrudePreviewActive_;
    if (normalizedTransientPreview)
    {
        activeModificationTool.clear();
        // The preview owns pointer input. Settled Cleave data remains a
        // dormant document workspace and must survive a checkpoint.
        viewportState.sectorCleaveToolEnabled = false;
        viewportState.surfaceCleaveToolEnabled = false;
    }

    // Placement and modification are exclusive pointer owners. Keep the
    // transform remembered underneath either owner, but never claim two
    // active owners in one payload. A malformed transient UI state is
    // canonicalized here rather than emitted for the strict V1 reader.
    if (placementMode_ != PlacementMode::none && !activeModificationTool.isEmpty())
    {
        activeModificationTool.clear();
    }
    if (placementMode_ != PlacementMode::none || !activeModificationTool.isEmpty())
    {
        suspendedTransformTool = transformNameForAction(
            rememberedViewportTransformAction_);
        if (suspendedTransformTool.isEmpty() && transformActive)
        {
            suspendedTransformTool = activeTransformTool;
        }
        transformActive = false;
        activeTransformTool.clear();
        viewportState.transformTool = GlView::TransformTool::Select;
    }
    else if (normalizedTransientPreview && !transformActive)
    {
        activeTransformTool = transformNameForAction(
            rememberedViewportTransformAction_);
        if (!activeTransformTool.isEmpty())
        {
            transformActive = true;
        }
    }

    // A stable no-owner checkpoint must agree with GlView's neutral enum.
    // Conversely, every active transform name maps back to the exact enum so
    // the reader can enforce the ownership invariant before mutation.
    if (transformActive)
    {
        if (activeTransformTool == QStringLiteral("viewport.transform.select"))
            viewportState.transformTool = GlView::TransformTool::Select;
        else if (activeTransformTool == QStringLiteral("viewport.transform.move"))
            viewportState.transformTool = GlView::TransformTool::Move;
        else if (activeTransformTool == QStringLiteral("viewport.transform.rotate"))
            viewportState.transformTool = GlView::TransformTool::Rotate;
        else if (activeTransformTool == QStringLiteral("viewport.transform.scale"))
            viewportState.transformTool = GlView::TransformTool::Scale;
        else
        {
            transformActive = false;
            activeTransformTool.clear();
            viewportState.transformTool = GlView::TransformTool::Select;
        }
    }
    else
    {
        viewportState.transformTool = GlView::TransformTool::Select;
    }

    const bool sectorCleave = activeModificationTool
        == QStringLiteral("viewport.sector.cleave");
    const bool surfaceCleave = activeModificationTool
        == QStringLiteral("viewport.surface.cleave");
    viewportState.sectorCleaveToolEnabled = sectorCleave;
    viewportState.surfaceCleaveToolEnabled = surfaceCleave;
    if (placementMode_ != PlacementMode::none)
    {
        viewportState.sectorCleaveToolEnabled = false;
        viewportState.surfaceCleaveToolEnabled = false;
    }
    QCborMap tools{};
    tools.insert(QStringLiteral("placement"), static_cast<int>(placementMode_));
    tools.insert(QStringLiteral("thingLightKind"), static_cast<int>(thingLightPlacementKind_));
    tools.insert(QStringLiteral("transformActive"), transformActive);
    tools.insert(QStringLiteral("transform"), activeTransformTool);
    tools.insert(QStringLiteral("suspendedTransform"), suspendedTransformTool);
    tools.insert(QStringLiteral("modification"), activeModificationTool);
    tools.insert(QStringLiteral("template"), QString::fromStdString(selectedThingTemplateName_));
    tools.insert(QStringLiteral("sectorShape"), QString::fromStdString(selectedSectorShapeName_));
    tools.insert(QStringLiteral("sectorPlaneAxis"), static_cast<int>(sectorDrawPlaneAxis_));
    tools.insert(QStringLiteral("sectorAtView"), sectorDrawAtCameraView_);
    tools.insert(QStringLiteral("sectorFreeView"), sectorDrawFreeView_);
    tools.insert(QStringLiteral("thingPanelVisible"), thingPlacementPanelVisible_);
    tools.insert(QStringLiteral("thingFiltersVisible"), thingPlacementFiltersVisible_);
    tools.insert(QStringLiteral("thingFilterHeight"), thingPlacementFilterContentHeight_);
    tools.insert(QStringLiteral("thingDock"), static_cast<int>(thingPlacementDockPosition_));
    tools.insert(QStringLiteral("thingDefaultMode"), static_cast<int>(thingPlacementDefaultMode_));
    tools.insert(QStringLiteral("thingToolMode"), static_cast<int>(thingPlacementToolMode_));
    tools.insert(QStringLiteral("thingLayer"), thingPlacementLayerOverride_);
    tools.insert(QStringLiteral("keepActive"), thingPlacementKeepActive_);
    tools.insert(QStringLiteral("snapThing"), thingPlacementSnapToThing_);
    tools.insert(QStringLiteral("snapRotate"), thingPlacementSnapRotate_);
    tools.insert(QStringLiteral("yaw"), thingPlacementYawOffset_);
    tools.insert(QStringLiteral("brush"), QCborArray{
        static_cast<int>(thingPlacementBrushShape_), thingPlacementBrushSize_,
        thingPlacementBrushCount_, thingPlacementBrushSpacing_, thingPlacementBrushScatter_,
        thingPlacementBrushRandomRotation_, thingPlacementBrushRotatePitch_,
        thingPlacementBrushRotateYaw_, thingPlacementBrushRotateRoll_,
        thingPlacementBrushFixedAngle_, thingPlacementBrushAllowAir_ });
    try
    {
        root.insert(QStringLiteral("viewport"), glRecoveryToCbor(viewportState));
    }
    catch (const std::exception& error)
    {
        recoveryEditorStateCaptureError_ = QString::fromUtf8(error.what());
        return {};
    }
    if (viewportState.architecturalLighting)
    {
        QCborValue architecturalLighting{};
        QString architecturalLightingError{};
        if (!ui::recovery::architecturalLightingWorkspaceToCbor(
                *viewportState.architecturalLighting,
                &architecturalLighting,
                &architecturalLightingError))
        {
            recoveryEditorStateCaptureError_ = architecturalLightingError.isEmpty()
                ? QStringLiteral("The MotS Arch workspace exceeds its persistence limit.")
                : architecturalLightingError;
            return {};
        }
        root.insert(QStringLiteral("architecturalLighting"), architecturalLighting);
    }
    QCborMap runtimeLighting{};
    runtimeLighting.insert(QStringLiteral("thingLights"),
        viewportState.thingLightRenderingEnabled);
    runtimeLighting.insert(QStringLiteral("sectorPointLights"),
        viewportState.sectorPointLightRenderingEnabled);
    root.insert(QStringLiteral("runtimeLighting"), runtimeLighting);
    QCborMap gridAppearance{};
    gridAppearance.insert(QStringLiteral("minorDots"),
        viewportState.gridMinorDots);
    gridAppearance.insert(QStringLiteral("minorDotSizePx"),
        static_cast<double>(viewportState.gridMinorDotSizePx));
    gridAppearance.insert(QStringLiteral("axisColors"), QCborArray{
        static_cast<qint64>(viewportState.gridAxisXColor.rgba()),
        static_cast<qint64>(viewportState.gridAxisYColor.rgba()),
        static_cast<qint64>(viewportState.gridAxisZColor.rgba()),
    });
    root.insert(QStringLiteral("gridAppearance"), gridAppearance);
    root.insert(QStringLiteral("tools"), tools);
    root.insert(QStringLiteral("cogEditors"), captureCogEditorRecoveryState());
    root.insert(QStringLiteral("resourceEditors"), captureResourceEditorRecoveryState());

    const auto cleaveWorkspaceForMemento = [](
        const CleaveApplyWorkspaceMemento& memento)
    {
        QCborMap cleave{};
        cleave.insert(QStringLiteral("currentFamily"), static_cast<int>(memento.family));
        cleave.insert(QStringLiteral("lines"),
            cleaveLinesToCbor(memento.draft.lines));
        if (memento.draft.chainOrigin)
        {
            cleave.insert(QStringLiteral("chainOrigin"), QCborArray{
                memento.draft.chainOrigin->lineIndex,
                memento.draft.chainOrigin->endpoint });
        }
        if (memento.bounded)
        {
            cleave.insert(QStringLiteral("bounded"), *memento.bounded);
        }
        cleave.insert(QStringLiteral("context"),
            cleaveContextToCbor(memento.draft.context));
        cleave.insert(QStringLiteral("options"), QCborArray{
            memento.stagedMode,
            memento.chainEnabled,
            memento.measurementsVisible,
            static_cast<int>(memento.measureUnit),
            memento.angleStepDegrees,
            memento.draft.selectedCleaveLine,
            memento.sectorToolEnabled,
            memento.surfaceToolEnabled,
        });
        cleave.insert(QStringLiteral("guides"), QCborMap{
            { QStringLiteral("angles"), memento.anglesVisible },
            { QStringLiteral("lengths"), memento.lengthsVisible },
            { QStringLiteral("numbers"), memento.lineNumbersVisible },
        });
        QCborArray order{};
        for (const std::int32_t index : memento.draft.applyOrder)
        {
            order.append(index);
        }
        cleave.insert(QStringLiteral("applyOrder"), order);
        QCborArray selected{};
        for (const std::int32_t index : memento.draft.selectedCleaveLines)
        {
            selected.append(index);
        }
        cleave.insert(QStringLiteral("selection"), QCborMap{
            { QStringLiteral("lines"), selected },
            { QStringLiteral("active"), memento.draft.selectedCleaveLine },
        });
        cleave.insert(QStringLiteral("transformMode"),
            static_cast<int>(memento.transformMode));
        cleave.insert(QStringLiteral("shape"), QCborMap{
            { QStringLiteral("version"), 1 },
            { QStringLiteral("kind"), static_cast<int>(memento.shape.kind) },
            { QStringLiteral("width"), memento.shape.width },
            { QStringLiteral("height"), memento.shape.height },
            { QStringLiteral("dimensionsLinked"), memento.shape.dimensionsLinked },
            { QStringLiteral("segments"), memento.shape.segments },
            { QStringLiteral("points"), memento.shape.points },
            { QStringLiteral("placementArmed"), memento.shape.placementArmed },
            { QStringLiteral("cameraPlane"), memento.shape.cameraPlane },
            { QStringLiteral("rotationDegrees"), memento.shape.rotationDegrees },
        });
        if (memento.history)
        {
            QCborArray snapshots{};
            for (const GlView::RecoveryState::CleaveDraftSnapshot& snapshot
                 : memento.history->snapshots)
            {
                snapshots.append(cleaveDraftToCbor(snapshot));
            }
            cleave.insert(QStringLiteral("history"), QCborMap{
                { QStringLiteral("version"), 1 },
                { QStringLiteral("cursor"), static_cast<qint64>(
                    memento.history->cursor) },
                { QStringLiteral("snapshots"), snapshots },
            });
        }
        return cleave;
    };
    const auto layerVisualStateToCbor = [](const LayerVisualHistoryState& state)
    {
        QCborArray visible{};
        for (const bool value : state.visible)
        {
            visible.append(value);
        }
        QCborArray locked{};
        for (const bool value : state.locked)
        {
            locked.append(value);
        }
        QCborArray colors{};
        for (const QColor& color : state.colors)
        {
            colors.append(static_cast<qint64>(color.rgba()));
        }
        return QCborMap{
            { QStringLiteral("visible"), visible },
            { QStringLiteral("locked"), locked },
            { QStringLiteral("colors"), colors },
        };
    };
    const auto appendWorkspaceAttachment = [this, &cleaveWorkspaceForMemento,
        &layerVisualStateToCbor](QCborArray& entries,
                                const std::size_t depth,
                                const int stack,
                                const std::size_t index)
    {
        const std::optional<std::uint64_t> recordId =
            document_.historyRecordIdAtDepth(depth);
        if (!recordId)
        {
            return false;
        }
        const auto memento = std::find_if(
            cleaveApplyWorkspaceMementos_.begin(),
            cleaveApplyWorkspaceMementos_.end(),
            [this, recordId](const CleaveApplyWorkspaceMemento& value)
            {
                return value.recordId == *recordId
                    && value.documentGeneration == documentLifetimeGeneration_;
            });
        const LayerVisualHistoryTransition* layerTransition = nullptr;
        for (const auto& [signature, transition] : layerVisualHistoryByAfterSignature_)
        {
            (void)signature;
            if (transition.recordId == *recordId)
            {
                layerTransition = &transition;
                break;
            }
        }
        if (memento == cleaveApplyWorkspaceMementos_.end()
            && !layerTransition)
        {
            return false;
        }

        QCborMap entry{};
        entry.insert(QStringLiteral("stack"), stack);
        entry.insert(QStringLiteral("index"), static_cast<qint64>(index));
        // The background HIST writer replaces these bounded placeholders
        // with hashes from the exact generated/reconstructed adjacent
        // HistoryNested frames before either native or recovery publication.
        entry.insert(QStringLiteral("beforeSha256"), QByteArray(32, '\0'));
        entry.insert(QStringLiteral("afterSha256"), QByteArray(32, '\0'));
        if (memento != cleaveApplyWorkspaceMementos_.end())
        {
            QCborArray active{
                memento->activeSector,
                memento->activeSurfaceSector,
                memento->activeSurface,
                memento->activeThing,
                memento->activeLight,
                memento->activeFrameThing,
                memento->activeFrameValue,
            };
            const QCborArray mode{
                static_cast<int>(memento->editMode),
                static_cast<int>(memento->surfaceSelectionMode),
                static_cast<int>(memento->lightEditSubMode),
            };
            entry.insert(QStringLiteral("cleave"), QCborMap{
                { QStringLiteral("workspace"),
                    cleaveWorkspaceForMemento(*memento) },
                { QStringLiteral("documentSelection"),
                    ui::recovery::serializeSelection(
                        memento->documentSelection) },
                { QStringLiteral("active"), active },
                { QStringLiteral("mode"), mode },
            });
        }
        if (layerTransition)
        {
            entry.insert(QStringLiteral("layerVisuals"), QCborMap{
                { QStringLiteral("before"),
                    layerVisualStateToCbor(layerTransition->before) },
                { QStringLiteral("after"),
                    layerVisualStateToCbor(layerTransition->after) },
            });
        }
        entries.append(std::move(entry));
        return stack == 1 && pendingCleaveApplyRedoRecordId_
            && *pendingCleaveApplyRedoRecordId_ == *recordId
            && hasPairedCleaveApplyRedo();
    };

    QCborArray workspaceHistoryEntries{};
    std::optional<QCborMap> pendingWorkspaceRedo{};
    for (std::size_t index = 0; index < document_.undoDepth(); ++index)
    {
        const std::size_t depth = document_.undoDepth() - index;
        if (appendWorkspaceAttachment(
                workspaceHistoryEntries, depth, 0, index))
        {
            pendingWorkspaceRedo = QCborMap{
                { QStringLiteral("stack"), 1 },
                { QStringLiteral("index"), static_cast<qint64>(index) },
            };
        }
    }
    for (std::size_t index = 0; index < document_.redoDepth(); ++index)
    {
        const std::size_t depth = document_.undoDepth() + index + 1u;
        if (appendWorkspaceAttachment(
                workspaceHistoryEntries, depth, 1, index))
        {
            pendingWorkspaceRedo = QCborMap{
                { QStringLiteral("stack"), 1 },
                { QStringLiteral("index"), static_cast<qint64>(index) },
            };
        }
    }
    if (!workspaceHistoryEntries.isEmpty())
    {
        QCborMap workspaceHistory{};
        workspaceHistory.insert(QStringLiteral("version"), 1);
        workspaceHistory.insert(QStringLiteral("undoCount"),
            static_cast<qint64>(document_.undoDepth()));
        workspaceHistory.insert(QStringLiteral("redoCount"),
            static_cast<qint64>(document_.redoDepth()));
        workspaceHistory.insert(QStringLiteral("entries"),
            workspaceHistoryEntries);
        if (pendingWorkspaceRedo)
        {
            workspaceHistory.insert(QStringLiteral("pendingRedo"),
                *pendingWorkspaceRedo);
        }
        root.insert(QStringLiteral("workspaceHistory"), workspaceHistory);
    }
    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

bool MainWindow::validateRecoveryEditorState(
    const QByteArray& payload,
    QString* warning)
{
    return processRecoveryEditorState(payload, warning, false, true);
}

bool MainWindow::restoreRecoveryEditorState(
    const QByteArray& payload,
    QString* warning,
    const bool deferViewportRebuild)
{
    return processRecoveryEditorState(
        payload, warning, deferViewportRebuild, false);
}

bool MainWindow::processRecoveryEditorState(
    const QByteArray& payload,
    QString* warning,
    const bool deferViewportRebuild,
    const bool validateOnly)
{
    const auto fail = [warning](const QString& message)
    {
        if (warning)
        {
            *warning = message;
        }
        return false;
    };

    // Clear stale caller diagnostics on success; every failure path below
    // replaces this value with its exact warning.
    if (warning)
    {
        warning->clear();
    }

    try
    {

    constexpr ui::recovery::RecoveryCborLimits editorLimits{
        assets::SedMaximumSemanticSectionBytes,
        kMaximumRecoveryEditorCborNodes,
        kMaximumRecoveryEditorCborDepth,
        kMaximumRecoveryEditorCborTextBytes,
        kMaximumRecoveryEditorCborByteStringBytes,
        assets::SedMaximumDecodedObjectBytes,
    };
    QString editorCborError{};
    const std::optional<QCborValue> decodedEditorState =
        ui::recovery::deserializeCanonicalCbor(
            payload, editorLimits, &editorCborError);
    if (!decodedEditorState || !decodedEditorState->isMap())
    {
        return fail(QStringLiteral(
            "The optional editor-state recovery chunk failed bounded canonical CBOR admission: %1; the level was still recovered.")
                .arg(editorCborError));
    }
    const QCborValue& rootValue = *decodedEditorState;
    const QCborMap root = rootValue.toMap();
    const QCborValue versionValue = root.value(QStringLiteral("version"));
    const qint64 recoveryVersion = versionValue.toInteger();
    if (!versionValue.isInteger() || recoveryVersion != kRecoveryEditorStateVersion)
    {
        return fail(QStringLiteral("The optional editor-state recovery version is unsupported; the level was still recovered."));
    }

    QString selectionError{};
    const QCborValue selectionValue = root.value(QStringLiteral("selection"));
    const std::optional<editor::Selection> selection = selectionValue.isByteArray()
        ? ui::recovery::deserializeSelection(selectionValue.toByteArray(), &selectionError)
        : std::optional<editor::Selection>{};
    if (!selection)
    {
        return fail(selectionError.isEmpty()
            ? QStringLiteral("The recovered selection is invalid; the level was still recovered.")
            : QStringLiteral("%1 The level was still recovered.").arg(selectionError));
    }
    const QCborValue activeValue = root.value(QStringLiteral("active"));
    const QCborValue editValue = root.value(QStringLiteral("edit"));
    const QCborValue engineExportPathValue = root.value(
        QStringLiteral("engineExportPath"));
    const QString recoveredEngineExportPath = engineExportPathValue.isString()
        ? engineExportPathValue.toString()
        : QString{};
    const QCborArray active = activeValue.isArray() ? activeValue.toArray() : QCborArray{};
    const QCborArray edit = editValue.isArray() ? editValue.toArray() : QCborArray{};
    const QCborMap layers = root.value(QStringLiteral("layers")).toMap();
    const QCborArray visible = layers.value(QStringLiteral("visible")).toArray();
    const QCborArray locked = layers.value(QStringLiteral("locked")).toArray();
    const QCborArray colors = layers.value(QStringLiteral("colors")).toArray();
    const QCborArray previewOverlays = root.value(QStringLiteral("previewOverlays")).toArray();
    const QCborMap recoveredWindow = root.value(QStringLiteral("window")).toMap();
    GlView::RecoveryState viewport{};
    bool runtimeThingLights = true;
    bool runtimeSectorPointLights = true;
    bool recoveredGridMinorDots = false;
    double recoveredGridMinorDotSizePx = 2.0;
    std::array<QColor, 3> recoveredGridAxisColors{};
    bool recoveredGridAxisColorsPresent = false;
    // An older EDIT with no staged lines inherits the independent app
    // preference. Seed the parse scratch state from settings so restoring a
    // previous document's explicit choice cannot become the fallback value.
    viewport.cleaveBounded = settingsStore().value(
        QStringLiteral("cleave/bounded"), true).toBool();
    const QCborValue runtimeLightingValue = root.value(
        QStringLiteral("runtimeLighting"));
    if (!runtimeLightingValue.isUndefined())
    {
        if (!runtimeLightingValue.isMap())
        {
            return fail(QStringLiteral(
                "The recovered runtime lighting state is invalid; the level was still recovered."));
        }
        const QCborMap runtimeLighting = runtimeLightingValue.toMap();
        const QCborValue thingLights = runtimeLighting.value(
            QStringLiteral("thingLights"));
        const QCborValue sectorPointLights = runtimeLighting.value(
            QStringLiteral("sectorPointLights"));
        if ((!thingLights.isUndefined() && !thingLights.isBool())
            || (!sectorPointLights.isUndefined() && !sectorPointLights.isBool()))
        {
            return fail(QStringLiteral(
                "The recovered runtime lighting state is invalid; the level was still recovered."));
        }
        if (thingLights.isBool())
        {
            runtimeThingLights = thingLights.toBool();
        }
        if (sectorPointLights.isBool())
        {
            runtimeSectorPointLights = sectorPointLights.toBool();
        }
    }
    const QCborValue gridAppearanceValue = root.value(
        QStringLiteral("gridAppearance"));
    if ( !gridAppearanceValue.isUndefined() )
    {
        if ( !gridAppearanceValue.isMap() )
        {
            return fail(QStringLiteral(
                "The recovered grid appearance state is invalid; the level was still recovered."));
        }
        const QCborMap gridAppearance = gridAppearanceValue.toMap();
        const QCborValue minorDots = gridAppearance.value(
            QStringLiteral("minorDots"));
        const QCborValue minorDotSizePx = gridAppearance.value(
            QStringLiteral("minorDotSizePx"));
        const QCborValue axisColorsValue = gridAppearance.value(
            QStringLiteral("axisColors"));
        if ( !minorDots.isUndefined() && !minorDots.isBool() )
        {
            return fail(QStringLiteral(
                "The recovered grid appearance state is invalid; the level was still recovered."));
        }
        if ( !minorDotSizePx.isUndefined()
            && !cborNumberInRange(minorDotSizePx, 0.5, 10.0,
                &recoveredGridMinorDotSizePx) )
        {
            return fail(QStringLiteral(
                "The recovered grid appearance state is invalid; the level was still recovered."));
        }
        if (!axisColorsValue.isUndefined())
        {
            if (!axisColorsValue.isArray())
            {
                return fail(QStringLiteral(
                    "The recovered grid axis colors are invalid; the level was still recovered."));
            }
            const QCborArray axisColors = axisColorsValue.toArray();
            if (axisColors.size() != 3)
            {
                return fail(QStringLiteral(
                    "The recovered grid axis colors are invalid; the level was still recovered."));
            }
            for (qsizetype index = 0; index < axisColors.size(); ++index)
            {
                qint64 rgba = -1;
                if (!cborIntegerInRange(axisColors.at(index), 0,
                        std::numeric_limits<quint32>::max(), &rgba))
                {
                    return fail(QStringLiteral(
                        "The recovered grid axis colors are invalid; the level was still recovered."));
                }
                recoveredGridAxisColors[static_cast<std::size_t>(index)] =
                    QColor::fromRgba(static_cast<QRgb>(rgba));
            }
            recoveredGridAxisColorsPresent = true;
        }
        if ( minorDots.isBool() )
        {
            recoveredGridMinorDots = minorDots.toBool();
        }
    }
    if (glView_)
    {
        // Optional overlay entries absent from older EDIT payloads inherit
        // the user's independent Editor and Preview app defaults.
        viewport.editor.showThingCollisionShapes =
            glView_->editorDefaultShowThingCollisionShapes();
        viewport.editor.showThingVisualBounds =
            glView_->editorDefaultShowThingVisualBounds();
        viewport.preview.showThingCollisionShapes =
            glView_->previewDefaultShowThingCollisionShapes();
        viewport.preview.showThingVisualBounds =
            glView_->previewDefaultShowThingVisualBounds();
    }
    if (!selectionValue.isByteArray()
        || !engineExportPathValue.isString()
        || recoveredEngineExportPath.size() > 32'768
        || recoveredEngineExportPath.contains(QChar('\0'))
        || !activeValue.isArray() || !editValue.isArray()
        || active.size() != 7 || edit.size() != 3
        || !root.value(QStringLiteral("layers")).isMap()
        || !layers.value(QStringLiteral("visible")).isArray()
        || !layers.value(QStringLiteral("locked")).isArray()
        || !layers.value(QStringLiteral("colors")).isArray()
        || previewOverlays.size() != 3
        || !previewOverlays.at(0).isBool()
        || !previewOverlays.at(1).isBool()
        || !previewOverlays.at(2).isBool()
        || !layers.value(QStringLiteral("wireframeColorByLayer")).isBool()
        || !layers.value(QStringLiteral("outlinerColorByLayer")).isBool()
        || !layers.value(QStringLiteral("showHiddenInOutliner")).isBool()
        || !root.value(QStringLiteral("window")).isMap()
        || visible.size() > 1'000'000 || locked.size() > 1'000'000 || colors.size() > 1'000'000
        || !glRecoveryFromCbor(
            root.value(QStringLiteral("viewport")), &viewport, document_.level()))
    {
        return fail(QStringLiteral("The optional editor-state recovery data is invalid; the level was still recovered."));
    }
    viewport.thingLightRenderingEnabled = runtimeThingLights;
    viewport.sectorPointLightRenderingEnabled = runtimeSectorPointLights;
    viewport.gridMinorDots = recoveredGridMinorDots;
    viewport.gridMinorDotSizePx = static_cast<float>(
        recoveredGridMinorDotSizePx);
    if (recoveredGridAxisColorsPresent)
    {
        viewport.gridAxisXColor = recoveredGridAxisColors[0];
        viewport.gridAxisYColor = recoveredGridAxisColors[1];
        viewport.gridAxisZColor = recoveredGridAxisColors[2];
    }
    const QCborValue architecturalLightingValue = root.value(
        QStringLiteral("architecturalLighting"));
    if (!architecturalLightingValue.isUndefined())
    {
        GlView::RecoveryState::ArchitecturalLightingState architecturalLighting{};
        QString architecturalLightingError{};
        if (!ui::recovery::architecturalLightingWorkspaceFromCbor(
                architecturalLightingValue,
                document_.level(),
                &architecturalLighting,
                &architecturalLightingError))
        {
            return fail(QStringLiteral(
                "The recovered MotS Arch workspace is invalid; the level was still recovered: %1")
                .arg(architecturalLightingError));
        }
        viewport.architecturalLighting = std::move(architecturalLighting);
    }
    for (const QCborValue& value : active)
    {
        if (!value.isInteger()
            || value.toInteger() < std::numeric_limits<std::int32_t>::min()
            || value.toInteger() > std::numeric_limits<std::int32_t>::max())
        {
            return fail(QStringLiteral("The recovered active selection is invalid; the level was still recovered."));
        }
    }

    qint64 editMode = -1;
    qint64 surfaceMode = -1;
    qint64 lightMode = -1;
    if (!cborIntegerInRange(edit.at(0), 0,
            static_cast<qint64>(EditMode::frame), &editMode)
        || !cborIntegerInRange(edit.at(1), 0,
            static_cast<qint64>(SurfaceSelectionMode::vertex), &surfaceMode)
        || !cborIntegerInRange(edit.at(2), 0,
            static_cast<qint64>(LightEditSubMode::architectural), &lightMode))
    {
        return fail(QStringLiteral("The optional editor modes are invalid; the level was still recovered."));
    }

    qint64 recoveredUvDock = -1;
    qint64 recoveredUvExtent = -1;
    bool recoveredUvRepeat = true;
    bool recoveredUvSnap = false;
    bool recoveredUvSubGrid = false;
    bool recoveredUvRepeatTileGrid = false;
    double recoveredUvGridStep = 0.25;
    bool recoveredUvMinorDots = false;
    double recoveredUvMinorDotSizePx = 2.0;
    QByteArray recoveredUvStatePayload{};
    QByteArray recoveredCogEditorsPayload{};
    QCborMap recoveredResourceEditorsPayload{};
    bool hasRecoveredResourceEditors = false;
    QByteArray recoveredTemplateEditorGeometry{};
    recoveredUvDock = recoveredWindow.value(QStringLiteral("uvDock")).toInteger(-1);
    recoveredUvExtent = recoveredWindow.value(QStringLiteral("uvExtent")).toInteger(-1);
    if (!recoveredWindow.value(QStringLiteral("uvDock")).isInteger()
        || !recoveredWindow.value(QStringLiteral("uvExtent")).isInteger()
        || !recoveredWindow.value(QStringLiteral("uvExpanded")).isBool()
        || !recoveredWindow.value(QStringLiteral("uvViewportFacing")).isBool()
        || !recoveredWindow.value(QStringLiteral("uvScaleLinked")).isBool()
        || recoveredUvDock < 0 || recoveredUvDock > 3
        || recoveredUvExtent < 1 || recoveredUvExtent > 100'000)
    {
        return fail(QStringLiteral(
            "The recovered UV editor layout is invalid; the level was still recovered."));
    }
    const QCborValue repeat = recoveredWindow.value(QStringLiteral("uvRepeat"));
    const QCborValue snap = recoveredWindow.value(QStringLiteral("uvSnap"));
    const QCborValue gridStep = recoveredWindow.value(QStringLiteral("uvGridStep"));
    const QCborValue subGrid = recoveredWindow.value(QStringLiteral("uvSubGrid"));
    const QCborValue repeatTileGrid = recoveredWindow.value(
        QStringLiteral("uvRepeatTileGrid"));
    if (!repeat.isBool() || !snap.isBool()
        || !cborNumberInRange(gridStep, 0.01, 16.0, &recoveredUvGridStep)
        || (!subGrid.isUndefined() && !subGrid.isBool())
        || (!repeatTileGrid.isUndefined() && !repeatTileGrid.isBool()))
    {
        return fail(QStringLiteral(
            "The recovered UV editor control state is invalid; the level was still recovered."));
    }
    recoveredUvRepeat = repeat.toBool();
    recoveredUvSnap = snap.toBool();
    recoveredUvSubGrid = subGrid.isBool() && subGrid.toBool();
    recoveredUvRepeatTileGrid = repeatTileGrid.isBool()
        && repeatTileGrid.toBool();
    const QCborValue uvGridAppearanceValue = recoveredWindow.value(
        QStringLiteral("uvGridAppearance"));
    if ( !uvGridAppearanceValue.isUndefined() )
    {
        if ( !uvGridAppearanceValue.isMap() )
        {
            return fail(QStringLiteral(
                "The recovered UV grid appearance is invalid; the level was still recovered."));
        }
        const QCborMap uvGridAppearance = uvGridAppearanceValue.toMap();
        const QCborValue minorDots = uvGridAppearance.value(
            QStringLiteral("minorDots"));
        const QCborValue minorDotSizePx = uvGridAppearance.value(
            QStringLiteral("minorDotSizePx"));
        if ( (!minorDots.isUndefined() && !minorDots.isBool())
            || (!minorDotSizePx.isUndefined()
                && !cborNumberInRange(
                    minorDotSizePx, 0.5, 10.0, &recoveredUvMinorDotSizePx)) )
        {
            return fail(QStringLiteral(
                "The recovered UV grid appearance is invalid; the level was still recovered."));
        }
        if ( minorDots.isBool() )
        {
            recoveredUvMinorDots = minorDots.toBool();
        }
    }
    const QCborValue uvState = recoveredWindow.value(QStringLiteral("uvState"));
    if (!uvState.isByteArray())
    {
        return fail(QStringLiteral(
            "The recovered UV editor workspace state is invalid; the level was still recovered."));
    }
    recoveredUvStatePayload = uvState.toByteArray();
    if (static_cast<std::uint64_t>(recoveredUvStatePayload.size())
        > 16u * 1024u * 1024u)
    {
        return fail(QStringLiteral(
            "The recovered UV editor workspace state is invalid; the level was still recovered."));
    }
    QString uvStateError{};
    if (!validateUvEditorWorkspaceState(
            recoveredUvStatePayload,
            *selection,
            static_cast<EditMode>(editMode),
            static_cast<SurfaceSelectionMode>(surfaceMode),
            recoveredWindow.value(QStringLiteral("uvViewportFacing")).toBool(),
            &uvStateError))
    {
        return fail(QStringLiteral(
            "The recovered UV editor workspace is invalid: %1 The level was still recovered.")
                .arg(uvStateError));
    }

    {
        constexpr qsizetype kMaximumQtLayoutBytes = 64 * 1024 * 1024;
        const QCborValue geometry = recoveredWindow.value(QStringLiteral("geometry"));
        const QCborValue state = recoveredWindow.value(QStringLiteral("state"));
        const QCborValue workspace = recoveredWindow.value(QStringLiteral("workspace"));
        const QCborValue legacySplitter = recoveredWindow.value(QStringLiteral("splitter"));
        const QCborValue outlinerTab = recoveredWindow.value(QStringLiteral("outlinerTab"));
        const QCborValue checksTab = recoveredWindow.value(QStringLiteral("checksTab"));
        const QCborValue cogEditors = root.value(QStringLiteral("cogEditors"));
        const QCborValue resourceEditors = root.value(QStringLiteral("resourceEditors"));
        if (!geometry.isByteArray() || geometry.toByteArray().size() > kMaximumQtLayoutBytes
            || !state.isByteArray() || state.toByteArray().size() > kMaximumQtLayoutBytes
            || !workspace.isByteArray() || workspace.toByteArray().size() > kMaximumQtLayoutBytes
            || !legacySplitter.isArray() || legacySplitter.toArray().size() > 32
            || !outlinerTab.isInteger() || !checksTab.isInteger()
            || !cogEditors.isByteArray()
            || static_cast<quint64>(cogEditors.toByteArray().size())
                > assets::SedMaximumSemanticSectionBytes
            || (root.contains(QStringLiteral("resourceEditors")) && !resourceEditors.isMap()))
        {
            return fail(QStringLiteral(
                "The recovered window/editor workspace state is invalid; the level was still recovered."));
        }
        recoveredCogEditorsPayload = cogEditors.toByteArray();
        const QCborValue templateEditorGeometry = recoveredWindow.value(
            QStringLiteral("templateEditorGeometry"));
        if (recoveredWindow.contains(QStringLiteral("templateEditorGeometry")))
        {
            if (!templateEditorGeometry.isByteArray()
                || templateEditorGeometry.toByteArray().isEmpty()
                || templateEditorGeometry.toByteArray().size() > 1024 * 1024)
            {
                return fail(QStringLiteral(
                    "The recovered Template Editor window geometry is invalid; the level was still recovered."));
            }
            recoveredTemplateEditorGeometry = templateEditorGeometry.toByteArray();
        }
        QString cogEditorError{};
        if (!validateCogEditorRecoveryState(
                recoveredCogEditorsPayload, &cogEditorError))
        {
            return fail(QStringLiteral(
                "The recovered COG workspace failed bounded canonical admission: %1. The level was still recovered.")
                    .arg(cogEditorError));
        }
        hasRecoveredResourceEditors = root.contains(QStringLiteral("resourceEditors"));
        if (hasRecoveredResourceEditors)
        {
            recoveredResourceEditorsPayload = resourceEditors.toMap();
            QString resourceEditorError{};
            if (!validateResourceEditorRecoveryState(
                    recoveredResourceEditorsPayload, &resourceEditorError))
            {
                return fail(QStringLiteral(
                    "The recovered resource-editor workspace failed bounded admission: %1. The level was still recovered.")
                        .arg(resourceEditorError));
            }
        }
        for (const QCborValue& size : legacySplitter.toArray())
        {
            if (!size.isInteger() || size.toInteger() < 0 || size.toInteger() > 1'000'000)
            {
                return fail(QStringLiteral(
                    "The recovered window splitter state is invalid; the level was still recovered."));
            }
        }
        const QCborValue splittersValue = recoveredWindow.value(QStringLiteral("splitters"));
        if (!splittersValue.isMap() || splittersValue.toMap().size() > 256)
        {
            return fail(QStringLiteral(
                "The recovered named splitter state is invalid; the level was still recovered."));
        }
        const QCborMap splitters = splittersValue.toMap();
        for (auto it = splitters.constBegin(); it != splitters.constEnd(); ++it)
        {
            if (!it.key().isString() || it.key().toString().isEmpty()
                || it.key().toString().size() > 1'024 || !it.value().isArray()
                || it.value().toArray().size() > 32)
            {
                return fail(QStringLiteral(
                    "The recovered named splitter state is malformed; the level was still recovered."));
            }
            for (const QCborValue& size : it.value().toArray())
            {
                if (!size.isInteger() || size.toInteger() < 0
                    || size.toInteger() > 1'000'000)
                {
                    return fail(QStringLiteral(
                        "The recovered named splitter size is invalid; the level was still recovered."));
                }
            }
        }
    }

    QCborMap tools{};
    QCborArray brush{};
    double recoveredYaw = 0.0;
    double recoveredBrushSize = 0.25;
    qint64 recoveredBrushCount = 5;
    double recoveredBrushSpacing = 0.55;
    double recoveredBrushScatter = 0.0;
    double recoveredBrushAngle = 0.0;
    bool recoveredTransformActive = false;
    QString recoveredTransformName{};
    QString recoveredSuspendedTransformName{};
    QString recoveredModificationName{};
    {
        const QCborValue toolsValue = root.value(QStringLiteral("tools"));
        if (!toolsValue.isMap())
        {
            return fail(QStringLiteral(
                "The recovered tool state is invalid; the level was still recovered."));
        }
        tools = toolsValue.toMap();
        const QCborValue brushValue = tools.value(QStringLiteral("brush"));
        brush = brushValue.isArray() ? brushValue.toArray() : QCborArray{};
        const qint64 brushShape = brush.size() == 11 ? brush.at(0).toInteger(-1) : -1;
        const qint64 placement = tools.value(QStringLiteral("placement")).toInteger(-1);
        const qint64 lightKind = tools.value(QStringLiteral("thingLightKind")).toInteger(-1);
        const qint64 dock = tools.value(QStringLiteral("thingDock")).toInteger(-1);
        const qint64 defaultMode = tools.value(QStringLiteral("thingDefaultMode")).toInteger(-1);
        const qint64 toolMode = tools.value(QStringLiteral("thingToolMode")).toInteger(-1);
        const qint64 layer = tools.value(QStringLiteral("thingLayer")).toInteger(-2);
        const QCborValue filterHeight = tools.value(QStringLiteral("thingFilterHeight"));
        const QCborValue transformActive = tools.value(QStringLiteral("transformActive"));
        const QCborValue transformName = tools.value(QStringLiteral("transform"));
        const QCborValue suspendedTransformName = tools.value(QStringLiteral("suspendedTransform"));
        const QCborValue modificationName = tools.value(QStringLiteral("modification"));
        if (!transformActive.isBool())
        {
            return fail(QStringLiteral(
                "The recovered transform-tool state is invalid; the level was still recovered."));
        }
        recoveredTransformActive = transformActive.toBool();
        // These three names are required V1 fields.  Undefined/missing values
        // were accepted by an unreleased compatibility path and could make a
        // pointer owner appear active with no canonical name.
        if (!tools.contains(QStringLiteral("transform"))
            || !tools.contains(QStringLiteral("suspendedTransform"))
            || !tools.contains(QStringLiteral("modification"))
            || !transformName.isString()
            || !suspendedTransformName.isString()
            || !modificationName.isString())
        {
            return fail(QStringLiteral(
                "The recovered transform/modification ownership state is invalid; the level was still recovered."));
        }
        recoveredTransformName = transformName.toString();
        recoveredSuspendedTransformName = suspendedTransformName.toString();
        recoveredModificationName = modificationName.toString();
        const auto isAllowedTransform = [](const QString& name)
        {
            return name.isEmpty()
                || name == QStringLiteral("viewport.transform.select")
                || name == QStringLiteral("viewport.transform.move")
                || name == QStringLiteral("viewport.transform.rotate")
                || name == QStringLiteral("viewport.transform.scale");
        };
        const auto isAllowedModification = [](const QString& name)
        {
            return name.isEmpty()
                || name == QStringLiteral("viewport.sector.cleave")
                || name == QStringLiteral("viewport.sector.join")
                || name == QStringLiteral("viewport.sector.thrust")
                || name == QStringLiteral("viewport.surface.cleave")
                || name == QStringLiteral("viewport.frame.add");
        };
        if (!isAllowedTransform(recoveredTransformName)
            || !isAllowedTransform(recoveredSuspendedTransformName)
            || !isAllowedModification(recoveredModificationName))
        {
            return fail(QStringLiteral(
                "The recovered interaction owner is not in the supported allow-list; the level was still recovered."));
        }
        if (!brushValue.isArray()
            || brush.size() != 11
            || !brush.at(0).isInteger()
            || !tools.value(QStringLiteral("placement")).isInteger()
            || !tools.value(QStringLiteral("thingLightKind")).isInteger()
            || !tools.value(QStringLiteral("thingDock")).isInteger()
            || !tools.value(QStringLiteral("thingDefaultMode")).isInteger()
            || !tools.value(QStringLiteral("thingToolMode")).isInteger()
            || !tools.value(QStringLiteral("thingLayer")).isInteger()
            || brushShape < 0 || brushShape > 2
            || placement < 0 || placement > static_cast<qint64>(PlacementMode::sectorLight)
            || lightKind < 0 || lightKind > 1
            || dock < 0 || dock > 3 || defaultMode < 0 || defaultMode > 2
            || toolMode < 0 || toolMode > 1
            || layer < -1 || layer > std::numeric_limits<std::int32_t>::max()
            || !tools.value(QStringLiteral("template")).isString()
            || !tools.value(QStringLiteral("sectorShape")).isString()
            || !tools.value(QStringLiteral("thingPanelVisible")).isBool()
            || !tools.value(QStringLiteral("thingFiltersVisible")).isBool()
            || !filterHeight.isInteger() || filterHeight.toInteger() < 1
            || filterHeight.toInteger() > 100'000
            || !tools.value(QStringLiteral("keepActive")).isBool()
            || !tools.value(QStringLiteral("snapThing")).isBool()
            || !tools.value(QStringLiteral("snapRotate")).isBool()
            || !cborNumberInRange(tools.value(QStringLiteral("yaw")), -360.0, 360.0, &recoveredYaw)
            || !cborNumberInRange(brush.at(1), 0.05, 256.0, &recoveredBrushSize)
            || !brush.at(2).isInteger()
            || (recoveredBrushCount = brush.at(2).toInteger()) < 1
            || recoveredBrushCount > 256
            || !cborNumberInRange(brush.at(3), 0.10, 2.0, &recoveredBrushSpacing)
            || !cborNumberInRange(brush.at(4), 0.0, 1.0, &recoveredBrushScatter)
            || !brush.at(5).isBool() || !brush.at(6).isBool()
            || !brush.at(7).isBool() || !brush.at(8).isBool()
            || !cborNumberInRange(brush.at(9), -360.0, 360.0, &recoveredBrushAngle)
            || !brush.at(10).isBool()
            || tools.value(QStringLiteral("template")).toString().size() > 4'096
            || tools.value(QStringLiteral("sectorShape")).toString().size() > 4'096
            || tools.value(QStringLiteral("modification")).toString().size() > 4'096)
        {
            return fail(QStringLiteral(
                "The recovered placement/brush state is invalid; the level was still recovered."));
        }

        const bool placementActive = placement != static_cast<qint64>(PlacementMode::none);
        const bool modificationActive = !recoveredModificationName.isEmpty();
        const bool suspendedTransformActive = !recoveredSuspendedTransformName.isEmpty();
        // Exactly one interaction owner may be active. A remembered transform
        // is sidecar state only and therefore requires placement/modification;
        // an active transform always has a non-empty canonical name.
        if ((placementActive && modificationActive)
            || (recoveredTransformActive && (placementActive || modificationActive
                || suspendedTransformActive))
            || (recoveredTransformActive != !recoveredTransformName.isEmpty())
            || (!recoveredTransformActive && recoveredTransformName != QString{})
            || (!placementActive && !modificationActive && suspendedTransformActive))
        {
            return fail(QStringLiteral(
                "The recovered tool ownership state violates the strict V1 pointer contract; the level was still recovered."));
        }

        const GlView::TransformTool expectedTransform =
            recoveredTransformName == QStringLiteral("viewport.transform.move")
                ? GlView::TransformTool::Move
                : recoveredTransformName == QStringLiteral("viewport.transform.rotate")
                    ? GlView::TransformTool::Rotate
                    : recoveredTransformName == QStringLiteral("viewport.transform.scale")
                        ? GlView::TransformTool::Scale
                        : GlView::TransformTool::Select;
        if (viewport.transformTool != expectedTransform
            || (!recoveredTransformActive
                && viewport.transformTool != GlView::TransformTool::Select))
        {
            return fail(QStringLiteral(
                "The recovered transform enum does not match its canonical owner; the level was still recovered."));
        }
        const bool sectorCleave = viewport.sectorCleaveToolEnabled;
        const bool surfaceCleave = viewport.surfaceCleaveToolEnabled;
        const bool expectedSectorCleave = recoveredModificationName
            == QStringLiteral("viewport.sector.cleave");
        const bool expectedSurfaceCleave = recoveredModificationName
            == QStringLiteral("viewport.surface.cleave");
        if (sectorCleave != expectedSectorCleave
            || surfaceCleave != expectedSurfaceCleave
            || (placementActive && (sectorCleave || surfaceCleave)))
        {
            return fail(QStringLiteral(
                "The recovered Cleave flags do not match the canonical modification owner; the level was still recovered."));
        }

        // Resolve action ownership while the payload is still in the
        // validation phase.  An unavailable owner is malformed input, not a
        // reason to partially reset the current workspace and continue.
        const auto transformActionAvailable = [this](const QString& name)
        {
            return name.isEmpty()
                || (name == QStringLiteral("viewport.transform.select")
                    && viewportTransformSelectAction_)
                || (name == QStringLiteral("viewport.transform.move")
                    && viewportTransformMoveAction_)
                || (name == QStringLiteral("viewport.transform.rotate")
                    && viewportTransformRotateAction_)
                || (name == QStringLiteral("viewport.transform.scale")
                    && viewportTransformScaleAction_);
        };
        const auto modificationActionAvailable = [this](const QString& name)
        {
            return name.isEmpty()
                || (name == QStringLiteral("viewport.sector.cleave")
                    && viewportSectorCleaveAction_)
                || (name == QStringLiteral("viewport.sector.join")
                    && viewportSectorConnectAction_)
                || (name == QStringLiteral("viewport.sector.thrust")
                    && viewportSectorThrustAction_)
                || (name == QStringLiteral("viewport.surface.cleave")
                    && viewportSurfaceCleaveAction_)
                || (name == QStringLiteral("viewport.frame.add")
                    && viewportFrameAddAction_);
        };
        if (!transformActionAvailable(recoveredTransformName)
            || !transformActionAvailable(recoveredSuspendedTransformName)
            || !modificationActionAvailable(recoveredModificationName))
        {
            return fail(QStringLiteral(
                "The recovered interaction owner is unavailable in this editor build; the level was still recovered."));
        }
    }

    std::vector<bool> recoveredVisible{};
    std::vector<bool> recoveredLocked{};
    std::vector<QColor> recoveredColors{};
    recoveredVisible.reserve(static_cast<std::size_t>(visible.size()));
    recoveredLocked.reserve(static_cast<std::size_t>(locked.size()));
    recoveredColors.reserve(static_cast<std::size_t>(colors.size()));
    for (const QCborValue& value : visible)
    {
        if (!value.isBool())
        {
            return fail(QStringLiteral("The recovered layer visibility data is invalid; the level was still recovered."));
        }
        recoveredVisible.push_back(value.toBool());
    }
    for (const QCborValue& value : locked)
    {
        if (!value.isBool())
        {
            return fail(QStringLiteral("The recovered layer lock data is invalid; the level was still recovered."));
        }
        recoveredLocked.push_back(value.toBool());
    }
    for (const QCborValue& value : colors)
    {
        if (!value.isInteger() || value.toInteger() < 0
            || value.toInteger() > std::numeric_limits<quint32>::max())
        {
            return fail(QStringLiteral("The recovered layer color data is invalid; the level was still recovered."));
        }
        recoveredColors.push_back(QColor::fromRgba(static_cast<QRgb>(value.toInteger())));
    }

    const Level& level = document_.level();
    const std::size_t expectedLayerCount = std::max<std::size_t>(1, level.layers.size());
    if (visible.size() != static_cast<qsizetype>(expectedLayerCount)
        || locked.size() != static_cast<qsizetype>(expectedLayerCount)
        || colors.size() != static_cast<qsizetype>(expectedLayerCount))
    {
        return fail(QStringLiteral(
            "The recovered layer arrays do not match the current level; the level was still recovered."));
    }
    const auto validIndex = [](const std::int32_t index, const std::size_t count)
    {
        return index >= 0 && static_cast<std::size_t>(index) < count;
    };
    const auto validSurface = [&](const SurfaceRef ref)
    {
        return validIndex(ref.sector, level.sectors.size())
            && validIndex(ref.surface,
                level.sectors[static_cast<std::size_t>(ref.sector)].surfaces.size());
    };
    const auto allIndicesValid = [&](const auto& indices, const std::size_t count)
    {
        return std::all_of(indices.begin(), indices.end(),
            [&](const std::int32_t index) { return validIndex(index, count); });
    };
    bool selectionValid = allIndicesValid(selection->sectors, level.sectors.size())
        && allIndicesValid(selection->things, level.things.size())
        && allIndicesValid(selection->lights, level.lights.size())
        && allIndicesValid(selection->cogs, level.cogs.size())
        && std::all_of(selection->surfaces.begin(), selection->surfaces.end(), validSurface);
    if (selectionValid)
    {
        selectionValid = std::all_of(
            selection->surfaceVertices.begin(), selection->surfaceVertices.end(),
            [&](const editor::SurfaceVertexRef& ref)
            {
                return validSurface(ref.surface)
                    && validIndex(ref.localVertex,
                        level.sectors[static_cast<std::size_t>(ref.surface.sector)]
                            .surfaces[static_cast<std::size_t>(ref.surface.surface)].vertices.size());
            })
            && std::all_of(selection->frames.begin(), selection->frames.end(),
                [&](const sed::level::FrameRef& ref)
                {
                    // A frame reference is valid only for an actual named
                    // `frame` value.  Merely checking the vector index would
                    // accept arbitrary TPL values (including numframes or
                    // inherited debris) and could later drive frame tools
                    // with malformed recovered state.
                    if (!validIndex(ref.thing, level.things.size()))
                    {
                        return false;
                    }
                    const Thing& thing = level.things[
                        static_cast<std::size_t>(ref.thing)];
                    if (!validIndex(ref.value, thing.values.size()))
                    {
                        return false;
                    }
                    const TplValue& value = thing.values[
                        static_cast<std::size_t>(ref.value)];
                    return value.type == ValueType::frame
                        && text::iequalsAscii(value.name, "frame");
                });
    }
    const std::int32_t activeSector = static_cast<std::int32_t>(active.at(0).toInteger());
    const SurfaceRef activeSurface{
        static_cast<std::int32_t>(active.at(1).toInteger()),
        static_cast<std::int32_t>(active.at(2).toInteger()) };
    const std::int32_t activeThing = static_cast<std::int32_t>(active.at(3).toInteger());
    const std::int32_t activeLight = static_cast<std::int32_t>(active.at(4).toInteger());
    const std::int32_t activeFrameThing = static_cast<std::int32_t>(active.at(5).toInteger());
    const std::int32_t activeFrameValue = static_cast<std::int32_t>(active.at(6).toInteger());
    const bool activeFrameUnset = activeFrameThing == -1 && activeFrameValue == -1;
    const bool activeFrameComplete = activeFrameThing >= 0 && activeFrameValue >= 0;
    if (!selectionValid
        || (activeSector != -1 && !validIndex(activeSector, level.sectors.size()))
        || ((activeSurface.sector == -1) != (activeSurface.surface == -1))
        || (activeSurface.sector != -1 && !validSurface(activeSurface))
        || (activeThing != -1 && !validIndex(activeThing, level.things.size()))
        || (activeLight != -1 && !validIndex(activeLight, level.lights.size()))
        || (!activeFrameUnset && !activeFrameComplete)
        || (activeFrameComplete
            && (!validIndex(activeFrameThing, level.things.size())
                || !selection->frames.contains(
                    level::FrameRef{ activeFrameThing, activeFrameValue })))
        || (activeFrameComplete && activeThing != activeFrameThing))
    {
        return fail(QStringLiteral(
            "The recovered selection references missing level objects; the level was still recovered."));
    }

    using SurfaceRefSet = std::unordered_set<SurfaceRef,
        editor::SurfaceRefHash, editor::SurfaceRefEq>;
    SurfaceRefSet recoveredStrictSurfaceScope{};
    SurfaceRefSet recoveredRememberedSurfaceFaces{};
    const QString surfaceSelectionContextKey =
        QStringLiteral("surfaceSelectionContext");
    const bool surfaceSelectionContextPresent =
        root.contains(surfaceSelectionContextKey);
    const QCborValue surfaceSelectionContextValue = root.value(
        surfaceSelectionContextKey);
    if (surfaceSelectionContextPresent)
    {
        if (!surfaceSelectionContextValue.isMap())
        {
            return fail(QStringLiteral(
                "The recovered Surface selection context is invalid; the level was still recovered."));
        }
        const QCborMap context = surfaceSelectionContextValue.toMap();
        const QCborValue contextVersion = context.value(QStringLiteral("version"));
        const QCborValue strictScopeValue = context.value(QStringLiteral("strictScope"));
        const QCborValue rememberedFacesValue = context.value(
            QStringLiteral("rememberedFaceSelection"));
        if (!contextVersion.isInteger()
            || contextVersion.toInteger() != kSurfaceSelectionContextVersion
            || !strictScopeValue.isArray()
            || !rememberedFacesValue.isArray())
        {
            return fail(QStringLiteral(
                "The recovered Surface selection context is invalid; the level was still recovered."));
        }

        const auto decodeSurfaceRefs = [&](const QCborArray& encoded,
                                           SurfaceRefSet& decoded)
        {
            for (const QCborValue& value : encoded)
            {
                if (!value.isArray())
                {
                    return false;
                }
                const QCborArray pair = value.toArray();
                if (pair.size() != 2
                    || !pair.at(0).isInteger()
                    || !pair.at(1).isInteger()
                    || pair.at(0).toInteger() < 0
                    || pair.at(1).toInteger() < 0
                    || pair.at(0).toInteger()
                        > std::numeric_limits<std::int32_t>::max()
                    || pair.at(1).toInteger()
                        > std::numeric_limits<std::int32_t>::max())
                {
                    return false;
                }
                const SurfaceRef reference{
                    static_cast<std::int32_t>(pair.at(0).toInteger()),
                    static_cast<std::int32_t>(pair.at(1).toInteger()) };
                if (!validSurface(reference) || !decoded.insert(reference).second)
                {
                    return false;
                }
            }
            return true;
        };
        if (!decodeSurfaceRefs(strictScopeValue.toArray(),
                recoveredStrictSurfaceScope)
            || !decodeSurfaceRefs(rememberedFacesValue.toArray(),
                recoveredRememberedSurfaceFaces))
        {
            return fail(QStringLiteral(
                "The recovered Surface selection context contains a duplicate or missing face reference; the level was still recovered."));
        }
    }
    else
    {
        // EDIT payloads predating the optional context can recover only the
        // owners still present in their decoded selection. This fallback is
        // intentionally unavailable when a newer payload stores explicit
        // empty arrays, because that empty scope can be meaningful.
        for (const SurfaceRef reference : selection->surfaces)
        {
            if (validSurface(reference))
            {
                recoveredStrictSurfaceScope.insert(reference);
                recoveredRememberedSurfaceFaces.insert(reference);
            }
        }
        for (const editor::SurfaceVertexRef& reference : selection->surfaceVertices)
        {
            if (validSurface(reference.surface))
            {
                recoveredStrictSurfaceScope.insert(reference.surface);
                recoveredRememberedSurfaceFaces.insert(reference.surface);
            }
        }
    }

    // Publication and restore share this exact schema path.  The writer stops
    // here, before it can replace the preceding durable checkpoint; restore
    // continues into the existing all-or-nothing workspace application below.
    if (validateOnly)
    {
        return true;
    }

    // Admit the simultaneous source, retained workspace and prepared copy
    // before changing any MainWindow state or clearing its current owner.
    QString viewportAdmissionError{};
    if (!glView_->canRestoreRecoveryState(viewport, &viewportAdmissionError))
    {
        return fail(viewportAdmissionError);
    }

    // Native SED workspace geometry wins over the application-wide fallback.
    // Older files omit this optional key and retain the user's last-open size.
    if (!recoveredTemplateEditorGeometry.isEmpty())
    {
        settingsStore().setValue(
            QStringLiteral("templates/editor_geometry"),
            recoveredTemplateEditorGeometry);
    }

    // Apply only after the complete optional payload has validated. An invalid
    // editor chunk must never compromise the already recovered level.  Clear
    // the current interaction owner first: restoreRecoveryEditorState is also
    // used directly by the .sed loader and by tests, where GlView's Level
    // pointer may remain unchanged and therefore cannot clear staged Cleave or
    // Sector Thrust state on its own.
    resetSelectionContextState();
    lastEngineExportPath_ = recoveredEngineExportPath.isEmpty()
        ? std::filesystem::path{}
        : std::filesystem::path(recoveredEngineExportPath.toStdWString());

    layerVisible_ = std::move(recoveredVisible);
    layerLocked_ = std::move(recoveredLocked);
    layerColors_ = std::move(recoveredColors);
    wireframeColorByLayer_ = layers.value(QStringLiteral("wireframeColorByLayer")).toBool(true);
    outlinerColorByLayer_ = layers.value(QStringLiteral("outlinerColorByLayer")).toBool(true);
    outlinerShowHiddenLayerEntities_ = layers.value(QStringLiteral("showHiddenInOutliner")).toBool();
    // Retain the three legacy CBOR fields for V1 compatibility, but Preview
    // is now unconditionally view-only and cannot present editor toolbars.
    previewShowEditModeOverlay_ = false;
    previewShowTransformOverlay_ = false;
    previewShowToolsOverlay_ = false;
    previewRespectHiddenLayers_ = viewport.previewRespectHiddenLayers;
    alwaysShowLightGizmoCategories_ = viewport.alwaysShowLightGizmoCategories;
    normalizeLayerVisualState();
    syncLayerViewState(!deferViewportRebuild);

    uvEditorDockPosition_ = static_cast<UvEditorDockPosition>(recoveredUvDock);
    uvEditorPanelExpanded_ = recoveredWindow.value(
        QStringLiteral("uvExpanded")).toBool(true);
    uvEditorPanelExpandedHeight_ = static_cast<int>(recoveredUvExtent);
    uvEditorViewportFacing_ = recoveredWindow.value(
        QStringLiteral("uvViewportFacing")).toBool();
    uvScaleLinked_ = recoveredWindow.value(QStringLiteral("uvScaleLinked")).toBool();
    applyUvEditorDockPosition(uvEditorDockPosition_, false);
    if (auto* facing = findChild<QToolButton*>(
            QStringLiteral("uvEditorViewportFacingButton")))
        facing->setChecked(uvEditorViewportFacing_);
    if (auto* repeat = findChild<QToolButton*>(
            QStringLiteral("uvEditorRepeatButton")))
    {
        repeat->setChecked(recoveredUvRepeat);
    }
    if (auto* snap = findChild<QToolButton*>(
            QStringLiteral("uvEditorSnapButton")))
    {
        snap->setChecked(recoveredUvSnap);
    }
    if (auto* grid = findChild<QDoubleSpinBox*>(
            QStringLiteral("uvEditorGridStepSpin")))
    {
        grid->setValue(recoveredUvGridStep);
    }
    if (auto* subGrid = findChild<QCheckBox*>(
            QStringLiteral("uvEditorSubGridCheck")))
    {
        subGrid->setChecked(recoveredUvSubGrid);
    }
    if ( auto* minorStyle = findChild<QComboBox*>(
            QStringLiteral("uvEditorSubGridStyleCombo")) )
    {
        const int index = minorStyle->findData(recoveredUvMinorDots);
        if ( index >= 0 )
        {
            minorStyle->setCurrentIndex(index);
        }
    }
    if ( auto* minorDotSize = findChild<QDoubleSpinBox*>(
            QStringLiteral("uvEditorSubGridDotSizeSpin")) )
    {
        minorDotSize->setValue(recoveredUvMinorDotSizePx);
        // Recovered combo selection can equal the current index, in which
        // case currentIndexChanged does not run to update this dependent
        // control. Keep its enabled state in sync with the validated style.
        minorDotSize->setEnabled(recoveredUvMinorDots);
    }
    if (auto* tileGrid = findChild<QCheckBox*>(
            QStringLiteral("uvEditorRepeatTileGridCheck")))
    {
        tileGrid->setChecked(recoveredUvRepeatTileGrid);
    }

    setPlacementMode(PlacementMode::none);
    {
        // The payload is already fully validated and this is not a user edit.
        // Restore its Editor mode sidecar even when the currently displayed
        // pipeline is Preview; the saved pipeline is published below only
        // after the complete workspace has been reconstructed.
        const QScopedValueRollback restoreModeGuard(
            recoveryEditorModeRestoreInProgress_, true);
        setLightEditSubMode(static_cast<LightEditSubMode>(lightMode));
        setSurfaceSelectionMode(static_cast<SurfaceSelectionMode>(surfaceMode));
        setEditMode(static_cast<EditMode>(editMode));
    }
    // Mode activation intentionally restores that mode's remembered selection.
    // The recovery selection is authoritative and must be applied afterwards.
    document_.selection() = *selection;
    strictSurfaceSelectionScope_ = std::move(recoveredStrictSurfaceScope);
    rememberedSurfaceFaceSelection_ =
        std::move(recoveredRememberedSurfaceFaces);
    glView_->setStrictSurfaceSelectionScope(
        surfaceStrictSelectionAction_ && surfaceStrictSelectionAction_->isChecked(),
        strictSurfaceSelectionScope_);
    // Mode activation computed tool availability against its remembered
    // selection. Refresh it against the authoritative recovered selection
    // before resolving the recovered pointer owner below; otherwise an
    // otherwise-valid Cleave action can remain disabled, be discarded during
    // ownership application, and clear its restored staged lines.
    updateEditActions();
    {
        // restoreRecoveryState emits viewportStateChanged.  The normal map
        // settings listener reads machine preferences on that signal, but a
        // recovered workspace carries its own authoritative four-family
        // values. Keep that listener quiet while the viewport and its actions
        // are published, without writing the user's preferences.
        const QScopedValueRollback restoreViewportGuard(
            recoveryViewportStateRestoreInProgress_, true);
        QString viewportRestoreError{};
        if (!glView_->restoreRecoveryState(
                viewport, !deferViewportRebuild, &viewportRestoreError))
        {
            return fail(viewportRestoreError.isEmpty()
                ? QStringLiteral(
                    "The recovered viewport state could not fit its bounded workspace; the level was still recovered.")
                : viewportRestoreError);
        }
        const std::array<QAction*, 4> lightGizmoActions{
            alwaysShowBakedLightGizmosAction_,
            alwaysShowThingLightGizmosAction_,
            alwaysShowSectorLightGizmosAction_,
            alwaysShowArchitecturalLightGizmosAction_ };
        for (std::size_t index = 0; index < lightGizmoActions.size(); ++index)
        {
            QAction* const action = lightGizmoActions[index];
            if (!action)
            {
                continue;
            }
            const QSignalBlocker blocker(action);
            action->setChecked(viewport.alwaysShowLightGizmoCategories[index]);
        }
    }
    // Cleave's option widgets normally drive the GlView values. During a
    // workspace restore the serialized viewport is authoritative, so mirror
    // it into those widgets with signals blocked before the final contextual
    // refresh. Otherwise syncCleaveOptionsOverlay() could overwrite restored
    // Multi-cut/chain state with the machine's current preference and hide a
    // valid staged session.
    if (viewportCleaveStagedButton_)
    {
        const QSignalBlocker blocker(viewportCleaveStagedButton_);
        viewportCleaveStagedButton_->setChecked(viewport.cleaveStagedMode);
    }
    if (viewportCleaveChainButton_)
    {
        const QSignalBlocker blocker(viewportCleaveChainButton_);
        viewportCleaveChainButton_->setChecked(viewport.cleaveChainEnabled);
    }
    if ( viewportCleaveBoundedButton_ && viewport.cleaveBounded.has_value() )
    {
        const QSignalBlocker blocker(viewportCleaveBoundedButton_);
        viewportCleaveBoundedButton_->setChecked(*viewport.cleaveBounded);
    }
    if (viewportCleaveUnitsCombo_)
    {
        const int unit = static_cast<int>(glView_->cleaveMeasureUnit());
        const int index = viewportCleaveUnitsCombo_->findData(unit);
        if (index >= 0)
        {
            const QSignalBlocker blocker(viewportCleaveUnitsCombo_);
            viewportCleaveUnitsCombo_->setCurrentIndex(index);
        }
    }
    if (viewportCleaveShowAnglesButton_)
    {
        const QSignalBlocker blocker(viewportCleaveShowAnglesButton_);
        viewportCleaveShowAnglesButton_->setChecked(viewport.cleaveAnglesVisible);
    }
    if (viewportCleaveShowLengthsButton_)
    {
        const QSignalBlocker blocker(viewportCleaveShowLengthsButton_);
        viewportCleaveShowLengthsButton_->setChecked(viewport.cleaveLengthsVisible);
    }
    if (viewportCleaveShowNumbersButton_)
    {
        const QSignalBlocker blocker(viewportCleaveShowNumbersButton_);
        viewportCleaveShowNumbersButton_->setChecked(viewport.cleaveLineNumbersVisible);
    }
    if (viewportCleaveAngleStepSpin_)
    {
        const QSignalBlocker blocker(viewportCleaveAngleStepSpin_);
        viewportCleaveAngleStepSpin_->setValue(viewport.cleaveAngleStepDegrees);
    }
    const std::int32_t activeSurfaceSector = static_cast<std::int32_t>(active.at(1).toInteger(-1));
    glView_->setSelectionState(
        activeSurfaceSector >= 0
            ? activeSurfaceSector
            : static_cast<std::int32_t>(active.at(0).toInteger(-1)),
        static_cast<std::int32_t>(active.at(2).toInteger(-1)),
        static_cast<std::int32_t>(active.at(3).toInteger(-1)),
        static_cast<std::int32_t>(active.at(4).toInteger(-1)),
        document_.selection());
    if (activeFrameComplete)
    {
        glView_->setSelectedFrameValueIndex(activeFrameValue);
    }
    else
    {
        glView_->clearFrameSelection();
    }

    // The GlView payload contains the authoritative Cleave flags, while the
    // action name covers tools without a GlView flag (Sector Thrust, Join,
    // Add Frame, and future modification tools).  Resolve one owner and then
    // publish the exact same state to every QAction.  Previously the payload
    // could restore Cleave internally while its button stayed unchecked,
    // leaving no usable way to turn the hidden tool off.
    const auto modificationActionForName = [this](const QString& name) -> QAction*
    {
        if (name == QStringLiteral("viewport.sector.cleave")) return viewportSectorCleaveAction_;
        if (name == QStringLiteral("viewport.sector.join")) return viewportSectorConnectAction_;
        if (name == QStringLiteral("viewport.sector.thrust")) return viewportSectorThrustAction_;
        if (name == QStringLiteral("viewport.surface.cleave")) return viewportSurfaceCleaveAction_;
        if (name == QStringLiteral("viewport.frame.add")) return viewportFrameAddAction_;
        return nullptr;
    };
    QAction* recoveredModificationAction = modificationActionForName(recoveredModificationName);
    if (recoveredModificationAction
        && (!recoveredModificationAction->isCheckable()
            || !recoveredModificationAction->isEnabled()))
    {
        recoveredModificationAction = nullptr;
    }
    if (viewportEditToolsGroup_)
    {
        for (QAction* action : viewportEditToolsGroup_->actions())
        {
            if (!action || !action->isCheckable())
            {
                continue;
            }
            const QSignalBlocker blocker(action);
            action->setChecked(action == recoveredModificationAction);
        }
    }
    if (glView_)
    {
        const bool sectorCleave = recoveredModificationAction == viewportSectorCleaveAction_;
        const bool surfaceCleave = recoveredModificationAction == viewportSurfaceCleaveAction_;
        glView_->setSectorCleaveToolEnabled(sectorCleave);
        glView_->setSurfaceCleaveToolEnabled(surfaceCleave);
        glView_->setSectorThrustToolEnabled(
            recoveredModificationAction == viewportSectorThrustAction_);
    }

    const auto transformActionForName = [this](const QString& name) -> QAction*
    {
        if (name == QStringLiteral("viewport.transform.select")) return viewportTransformSelectAction_;
        if (name == QStringLiteral("viewport.transform.move")) return viewportTransformMoveAction_;
        if (name == QStringLiteral("viewport.transform.rotate")) return viewportTransformRotateAction_;
        if (name == QStringLiteral("viewport.transform.scale")) return viewportTransformScaleAction_;
        return nullptr;
    };
    QAction* activeTransformAction = recoveredTransformActive
        ? transformActionForName(recoveredTransformName)
        : nullptr;
    if (recoveredTransformActive && !activeTransformAction)
    {
        // V1 names are authoritative; do not infer an action from GlView's
        // enum or silently fall back to a compatibility interpretation.
        return fail(QStringLiteral(
            "The recovered transform owner is unavailable in this editor build; the level was still recovered."));
    }
    QAction* suspendedTransformAction = transformActionForName(recoveredSuspendedTransformName);
    const bool recoveredPlacementActive = tools.value(QStringLiteral("placement")).toInteger(-1)
        != static_cast<qint64>(PlacementMode::none);
    if (recoveredModificationAction || recoveredPlacementActive)
    {
        // A modification/placement owner is the sole active pointer owner.
        // Keep the remembered transform sidecar-only and restore it only when
        // that owner is later deactivated.
        if (!suspendedTransformAction)
        {
            suspendedTransformAction = activeTransformAction;
        }
        setViewportTransformActionChecks(nullptr);
        rememberedViewportTransformAction_ = suspendedTransformAction;
        if (glView_)
        {
            glView_->setTransformTool(GlView::TransformTool::Select);
            glView_->setTransformInteractionEnabled(false);
        }
    }
    else
    {
        if (activeTransformAction && !activeTransformAction->isEnabled())
        {
            activeTransformAction = nullptr;
        }
        setViewportTransformActionChecks(activeTransformAction);
        rememberedViewportTransformAction_ = activeTransformAction;
        if (glView_)
        {
            glView_->setTransformTool(
                activeTransformAction == viewportTransformMoveAction_
                    ? GlView::TransformTool::Move
                    : activeTransformAction == viewportTransformRotateAction_
                        ? GlView::TransformTool::Rotate
                        : activeTransformAction == viewportTransformScaleAction_
                            ? GlView::TransformTool::Scale
                            : GlView::TransformTool::Select);
            glView_->setTransformInteractionEnabled(activeTransformAction != nullptr);
        }
    }
    if (viewportTransformOrientationLocalAction_ && viewportTransformOrientationWorldAction_)
    {
        const bool world = viewport.transformOrientation == GlView::TransformOrientation::World;
        viewportTransformOrientationLocalAction_->setChecked(!world);
        viewportTransformOrientationWorldAction_->setChecked(world);
        if (viewportTransformOrientationButton_)
        {
            viewportTransformOrientationButton_->setIcon(world
                    ? viewportTransformOrientationWorldAction_->icon()
                    : viewportTransformOrientationLocalAction_->icon());
        }
    }
    if (viewportTransformGridSnapAction_)
    {
        viewportTransformGridSnapAction_->setChecked(viewport.transformGridSnap);
    }
    if (viewportTransformGridStickAction_)
    {
        viewportTransformGridStickAction_->setChecked(viewport.transformGridPlaneStick);
    }
    syncViewportSelectionInputEnabled();

    std::optional<QList<int>> recoveredViewportUvSplitterSizes{};
    {
        selectedThingTemplateName_ = tools.value(QStringLiteral("template")).toString().toStdString();
        selectedSectorShapeName_ = tools.value(QStringLiteral("sectorShape")).toString().toStdString();
        sectorDrawPlaneAxis_ = static_cast<SectorDrawPlaneAxis>(std::clamp<qint64>(
            tools.value(QStringLiteral("sectorPlaneAxis")).toInteger(
                static_cast<int>(SectorDrawPlaneAxis::z)),
            static_cast<int>(SectorDrawPlaneAxis::x),
            static_cast<int>(SectorDrawPlaneAxis::z)));
        sectorDrawAtCameraView_ = tools.value(QStringLiteral("sectorAtView")).toBool(false);
        sectorDrawFreeView_ = tools.value(QStringLiteral("sectorFreeView")).toBool(false);
        thingPlacementPanelVisible_ = tools.value(QStringLiteral("thingPanelVisible")).toBool(true);
        thingPlacementFiltersVisible_ = tools.value(QStringLiteral("thingFiltersVisible")).toBool(true);
        thingPlacementFilterContentHeight_ = static_cast<int>(std::clamp<qint64>(
            tools.value(QStringLiteral("thingFilterHeight")).toInteger(92), 1, 100'000));
        thingPlacementDockPosition_ = static_cast<ThingPlacementDockPosition>(
            tools.value(QStringLiteral("thingDock")).toInteger());
        thingPlacementDefaultMode_ = static_cast<ThingPlacementDefaultMode>(
            tools.value(QStringLiteral("thingDefaultMode")).toInteger());
        thingPlacementToolMode_ = static_cast<ThingPlacementToolMode>(
            tools.value(QStringLiteral("thingToolMode")).toInteger());
        thingPlacementLayerOverride_ = static_cast<std::int32_t>(
            tools.value(QStringLiteral("thingLayer")).toInteger(-1));
        thingPlacementKeepActive_ = tools.value(QStringLiteral("keepActive")).toBool();
        thingPlacementSnapToThing_ = tools.value(QStringLiteral("snapThing")).toBool();
        thingPlacementSnapRotate_ = tools.value(QStringLiteral("snapRotate")).toBool(true);
        thingPlacementYawOffset_ = recoveredYaw;
        thingPlacementBrushShape_ = static_cast<ThingPlacementBrushShape>(brush.at(0).toInteger());
        thingPlacementBrushSize_ = recoveredBrushSize;
        thingPlacementBrushCount_ = static_cast<int>(recoveredBrushCount);
        thingPlacementBrushSpacing_ = recoveredBrushSpacing;
        thingPlacementBrushScatter_ = recoveredBrushScatter;
        thingPlacementBrushRandomRotation_ = brush.at(5).toBool();
        thingPlacementBrushRotatePitch_ = brush.at(6).toBool();
        thingPlacementBrushRotateYaw_ = brush.at(7).toBool();
        thingPlacementBrushRotateRoll_ = brush.at(8).toBool();
        thingPlacementBrushFixedAngle_ = recoveredBrushAngle;
        thingPlacementBrushAllowAir_ = brush.at(10).toBool();
        syncThingPlacementPanel();

        // A modification tool owns the pointer when it was present in the
        // validated payload.  Only restore placement when no such owner was
        // requested; this avoids rearming a placement mode underneath a
        // restored Cleave/Thrust action.
        const bool payloadRequestedModification =
            !tools.value(QStringLiteral("modification")).toString().isEmpty()
            || viewport.sectorCleaveToolEnabled
            || viewport.surfaceCleaveToolEnabled;
        setThingLightPlacementKind(static_cast<ThingLightPlacementKind>(
            tools.value(QStringLiteral("thingLightKind")).toInteger()));
        if (!payloadRequestedModification)
        {
            const qint64 placement = tools.value(QStringLiteral("placement")).toInteger();
            if (placement >= 0 && placement <= static_cast<qint64>(PlacementMode::sectorLight))
            {
                setPlacementMode(static_cast<PlacementMode>(placement));
            }
        }

        // Legacy EDIT may omit resourceEditors. Retire those previous pages
        // before preparing COG hosts so every restored host belongs to the
        // replacement document context, including detached native windows.
        if ( !hasRecoveredResourceEditors )
        {
            closeResourceEditorsAfterReplacement();
        }
        const QCborMap window = recoveredWindow;
        const QByteArray geometry = window.value(QStringLiteral("geometry")).toByteArray();
        const QByteArray state = window.value(QStringLiteral("state")).toByteArray();
        const QByteArray workspace = window.value(QStringLiteral("workspace")).toByteArray();
        QStringList layoutWarnings{};
        if (!geometry.isEmpty() && !restoreGeometry(geometry))
            layoutWarnings.push_back(QStringLiteral("Main window geometry could not be restored."));
        if (!state.isEmpty() && !restoreState(state, kRecoveryMainWindowStateVersion))
            layoutWarnings.push_back(QStringLiteral("Main dock layout could not be restored."));
        if (workspaceWindow_ && !workspace.isEmpty())
        {
            if ( !prepareCogEditorRecoveryHosts(recoveredCogEditorsPayload) )
            {
                layoutWarnings.push_back(QStringLiteral(
                    "COG editor host windows could not be prepared."));
            }
            if (hasRecoveredResourceEditors
                && !prepareResourceEditorRecoveryHosts(recoveredResourceEditorsPayload))
            {
                layoutWarnings.push_back(QStringLiteral(
                    "Resource editor host windows could not be prepared."));
            }
            if (!workspaceWindow_->restoreState(workspace, kRecoveryWorkspaceStateVersion))
                layoutWarnings.push_back(QStringLiteral("COG workspace dock layout could not be restored."));
        }

        bool foundNamedViewportSplitter = false;
        if (window.value(QStringLiteral("splitters")).isMap())
        {
            const QCborMap namedSplitters = window.value(QStringLiteral("splitters")).toMap();
            if (namedSplitters.size() > 256)
            {
                layoutWarnings.push_back(QStringLiteral(
                    "The recovered workspace contains too many splitter records."));
            }
            else
            {
                for (QSplitter* splitter : findChildren<QSplitter*>())
                {
                    if (!splitter || splitter->objectName().isEmpty()
                        || !namedSplitters.contains(splitter->objectName()))
                    {
                        continue;
                    }
                    const QCborValue sizesValue = namedSplitters.value(splitter->objectName());
                    if (!sizesValue.isArray())
                    {
                        layoutWarnings.push_back(QStringLiteral(
                            "Splitter %1 had invalid recovery data.").arg(splitter->objectName()));
                        continue;
                    }
                    const QCborArray sizes = sizesValue.toArray();
                    if (sizes.size() > 32)
                    {
                        layoutWarnings.push_back(QStringLiteral(
                            "Splitter %1 contains too many recovered panes.")
                            .arg(splitter->objectName()));
                        continue;
                    }
                    QList<int> restoredSizes{};
                    bool validSizes = true;
                    for (const QCborValue& size : sizes)
                    {
                        if (!size.isInteger() || size.toInteger() < 0
                            || size.toInteger() > 1'000'000)
                        {
                            validSizes = false;
                            break;
                        }
                        restoredSizes.push_back(static_cast<int>(size.toInteger()));
                    }
                    if (!validSizes)
                    {
                        layoutWarnings.push_back(QStringLiteral(
                            "Splitter %1 had out-of-range recovery data.")
                            .arg(splitter->objectName()));
                        continue;
                    }
                    if (splitter == viewportUvSplitter_)
                    {
                        // UV dock position/order is restored earlier and
                        // syncUvEditorPanel() finalizes this dynamic splitter.
                        // Applying or comparing its pane count before that
                        // point creates a false layout-mismatch warning.
                        foundNamedViewportSplitter = true;
                        if (normalizeRecoveredViewportUvSplitterSizes(
                                restoredSizes, splitter->count()))
                        {
                            recoveredViewportUvSplitterSizes = restoredSizes;
                        }
                        continue;
                    }
                    if (sizes.size() != splitter->count())
                    {
                        layoutWarnings.push_back(QStringLiteral(
                            "Splitter %1 no longer matches the recovered layout.")
                            .arg(splitter->objectName()));
                        continue;
                    }
                    splitter->setSizes(restoredSizes);
                }
            }
        }

        const QCborArray sizes = window.value(QStringLiteral("splitter")).toArray();
        if (!foundNamedViewportSplitter && viewportUvSplitter_
            && sizes.size() <= 32)
        {
            QList<int> restoredSizes{};
            bool validSizes = !sizes.isEmpty();
            for (const QCborValue& size : sizes)
            {
                if (!size.isInteger() || size.toInteger() < 0
                    || size.toInteger() > 1'000'000)
                {
                    validSizes = false;
                    break;
                }
                restoredSizes.push_back(static_cast<int>(size.toInteger()));
            }
            if (validSizes
                && normalizeRecoveredViewportUvSplitterSizes(
                    restoredSizes, viewportUvSplitter_->count()))
            {
                recoveredViewportUvSplitterSizes = restoredSizes;
            }
        }
        {
            if (uvEditorPanelHost_ && !uvEditorPanelHost_->isHidden())
            {
                setUvEditorPanelExpanded(uvEditorPanelExpanded_, false);
            }
            // Rebuild from the authoritative recovered Surface selection only
            // after camera, modes, layers, and splitters are final. Applying
            // the settled canvas state last also cancels any deferred Fit that
            // could otherwise overwrite its exact zoom/pan on the next event.
            syncUvEditorPanel();
            if (recoveredViewportUvSplitterSizes
                && viewportUvSplitter_
                && recoveredViewportUvSplitterSizes->size()
                    == viewportUvSplitter_->count())
            {
                viewportUvSplitter_->setSizes(
                    *recoveredViewportUvSplitterSizes);
            }
            QString uvStateWarning{};
            if (!restoreUvEditorWorkspaceState(
                    recoveredUvStatePayload,
                    document_.selection(),
                    static_cast<EditMode>(editMode),
                    static_cast<SurfaceSelectionMode>(surfaceMode),
                    uvEditorViewportFacing_,
                    &uvStateWarning))
            {
                layoutWarnings.push_back(QStringLiteral(
                    "UV editor view could not be restored: %1")
                        .arg(uvStateWarning));
                syncUvEditorViewportFacingOrientation();
            }
        }
        if (outlinerTabs_)
            outlinerTabs_->setCurrentIndex(std::clamp(
                static_cast<int>(window.value(QStringLiteral("outlinerTab")).toInteger()),
                0, std::max(0, outlinerTabs_->count() - 1)));
        if (checksTabs_)
            checksTabs_->setCurrentIndex(std::clamp(
                static_cast<int>(window.value(QStringLiteral("checksTab")).toInteger()),
                0, std::max(0, checksTabs_->count() - 1)));
        QStringList cogWarnings{};
        (void)restoreCogEditorRecoveryState(
            recoveredCogEditorsPayload, &cogWarnings);
        layoutWarnings.append(cogWarnings);
        if (hasRecoveredResourceEditors)
        {
            QStringList resourceWarnings{};
            (void)restoreResourceEditorRecoveryState(
                recoveredResourceEditorsPayload, &resourceWarnings);
            layoutWarnings.append(resourceWarnings);
        }
        if (warning && !layoutWarnings.isEmpty())
        {
            *warning = layoutWarnings.join(QLatin1Char('\n'));
        }
    }
    // Restored QAction ownership is applied with QSignalBlocker so loading a
    // workspace cannot replay destructive/undoable action handlers. Publish
    // the corresponding option bars explicitly after every tool, submode,
    // placement, viewport, and window-layout field has reached its final
    // state. This path is shared by .sed, .sedrecovery, and autosave loading.
    syncViewportContextualOverlays();
    syncThingPlacementPanel();
    if (!documentEditingAllowed())
    {
        // restoreRecoveryState publishes the recovered Preview pipeline before
        // the window, UV, Thing palette, and COG dock records are applied.
        // That transition necessarily captures the old/intermediate widget
        // visibility. Replace it only after the complete recovered workspace
        // is authoritative. The contextual sync above can legitimately show
        // the recovered Editor-side Thing palette, so Preview's transient hide
        // must be the final presentation step.
        const EditMode recoveredEditMode = static_cast<EditMode>(editMode);
        if (recoveredEditMode == EditMode::thing)
        {
            // Preview has already hidden the Thing widgets, but the recovered
            // snapshot must contain the canonical Editor endpoint for the
            // recovered dock/size settings rather than a stale pre-recovery
            // rectangle. Lay it out synchronously without animation, then
            // hide it again below before control returns to the event loop.
            const QScopedValueRollback<bool> restorePreviewUiGuard(
                previewEditingUiRestoreInProgress_, true);
            positionThingPlacementPanel();
        }
        ++previewEditingUiTransitionSerial_;
        previewEditingUiVisibility_ = {
            .valid = true,
            .documentEpoch = recoveryDocumentEpoch_,
            .uvEditor = recoveredEditMode == EditMode::surface,
            .uvEditorSplitterSizes = recoveredViewportUvSplitterSizes
                && viewportUvSplitter_
                && recoveredViewportUvSplitterSizes->size()
                    == viewportUvSplitter_->count()
                ? *recoveredViewportUvSplitterSizes
                : viewportUvSplitter_
                    ? viewportUvSplitter_->sizes()
                    : QList<int>{},
            .thingPlacement = recoveredEditMode == EditMode::thing
                && thingPlacementPanelVisible_,
            .thingPlacementCollapsed = recoveredEditMode == EditMode::thing
                && !thingPlacementPanelVisible_,
            .thingPlacementGeometry = thingPlacementPanel_
                ? thingPlacementPanel_->geometry()
                : QRect{},
            .thingPlacementCollapsedGeometry = thingPlacementCollapsedBar_
                ? thingPlacementCollapsedBar_->geometry()
                : QRect{},
            .cogScriptEditor = cogScriptEditorDock_
                && !cogScriptEditorDock_->isHidden(),
            .cogScriptEditorGeometry = cogScriptEditorDock_
                ? cogScriptEditorDock_->geometry()
                : QRect{},
            .cogScriptEditorWidth = cogScriptEditorDock_
                ? cogScriptEditorDock_->width()
                : 0,
        };
        for ( const QPointer<QDockWidget>& dock : cogScriptEditorDocks() )
        {
            if ( dock && dock != cogScriptEditorDock_ )
            {
                previewEditingUiVisibility_.detachedCogScriptEditorVisibility.emplace_back(
                    dock, !dock->isHidden());
            }
        }
        hideEditingUiForGamePreview();
    }
    return true;
    }
    catch (const std::exception& e)
    {
        return fail(QStringLiteral(
            "The optional editor-state recovery failed safely: %1. The level was still recovered.")
            .arg(QString::fromUtf8(e.what())));
    }
    catch (...)
    {
        return fail(QStringLiteral(
            "The optional editor-state recovery failed safely after an unknown error; the level was still recovered."));
    }
}

QByteArray MainWindow::recoveryCurrentLevelStateHash() const
{
    Level level = document_.level();
    level.lvisstring = buildLayerVisibilityString();
    return ui::recovery::authoredLevelStateHash(level);
}

void MainWindow::scheduleRecoverySnapshot(const bool force)
{
    if (!recoveryAutomaticBaselineEnabled_
        || recoveryShuttingDown_ || recoveryPublicationPaused_
        || !recoveryPublicationGate_
        || (!hasRecoverableChanges() && !force))
    {
        return;
    }

    if (glView_ && glView_->flyNavigationActive()
        && !recoveryFlushDraining_ && !recoverySystemShutdown_)
    {
        recoveryCaptureDeferredForFly_ = true;
        return;
    }

    bool requestedArchivePreviousSnapshot = false;
    std::filesystem::path requestedArchiveSourcePath{};
    bool requestedAuthoredMutationPending = false;
    try
    {
        // Resolve the destination before taking the key/payload.  This keeps
        // the identity in the key coherent with the path when a test or a
        // first untitled document reaches recovery before startup has filled
        // in the slot.
        if (recoveryIdentity_.isEmpty() || currentRecoveryPath_.empty())
        {
            switchRecoveryDocument(currentPath_, currentPath_.empty(), false);
        }

        const std::uint64_t undoSignature = document_.undoSignature();
        const std::uint64_t redoSignature = document_.redoSignature();
        const std::uint64_t eventSerial = recoveryEventSerial_;
        const std::uint64_t semanticMutationSerial = recoverySemanticMutationSerial_;
        const QByteArray editorStatePayload = captureRecoveryEditorState();
        QString editorStateValidationError{};
    if (!validateRecoveryEditorState(
                editorStatePayload, &editorStateValidationError))
    {
        throw std::runtime_error(QStringLiteral(
            "Cannot publish recovery snapshot because its EDIT payload failed exact validation: %1")
                .arg(recoveryEditorStateCaptureError_.isEmpty()
                    ? editorStateValidationError
                    : recoveryEditorStateCaptureError_)
                    .toStdString());
        }
        const QByteArray editorStateHash = QCryptographicHash::hash(
            editorStatePayload, QCryptographicHash::Sha256);
        const QByteArray authoredCogState = captureAuthoredCogRecoveryState();
        const QByteArray authoredCogStateHash = QCryptographicHash::hash(
            authoredCogState, QCryptographicHash::Sha256);
        const bool authoredMutationPending = recoveryAuthoredMutationPending_;
        requestedAuthoredMutationPending = authoredMutationPending;
        // Same-undo property/preview paths signal a cheap authored event from
        // noteRecoveryStateChanged(). Do not serialize or hash the complete
        // Level here: a large level is copied and encoded by the worker below,
        // while the event serial is sufficient to distinguish the pending
        // capture from its published/in-flight key on this thread.
        const bool sameAsLastPublished = recoveryLastPublishedValid_
            && recoveryLastPublishedIdentity_ == recoveryIdentity_
            && recoveryLastPublishedDocumentEpoch_ == recoveryDocumentEpoch_
            && recoveryPathsEqual(
                recoveryLastPublishedPath_, currentRecoveryPath_)
            && recoveryLastPublishedUndoSignature_ == undoSignature
            && recoveryLastPublishedRedoSignature_ == redoSignature
            && recoveryLastPublishedEventSerial_ == eventSerial
            && recoveryLastPublishedSemanticMutationSerial_ == semanticMutationSerial
            && recoveryLastPublishedEditorStateHash_ == editorStateHash;
        if (!force && !recoveryArchivePreviousOnNextSnapshot_
            && sameAsLastPublished
            && !authoredMutationPending
            && (!recoveryWriteWatcher_ || !recoveryWriteWatcher_->isRunning()))
        {
            // A notification may have set the required bit while the
            // canonical payload stayed unchanged.  Once no writer is active,
            // clear that stale requirement without publishing another file.
            recoverySnapshotRequired_ = false;
            recoverySnapshotQueued_ = false;
            recoveryForcedSnapshotQueued_ = false;
            return;
        }

        if (recoveryWriteWatcher_ && recoveryWriteWatcher_->isRunning())
        {
            if (!force && recoveryInFlightIdentity_ == recoveryIdentity_
                && recoveryInFlightDocumentEpoch_ == recoveryDocumentEpoch_
                && recoveryPathsEqual(
                    recoveryInFlightPath_, currentRecoveryPath_)
                && recoveryInFlightUndoSignature_ == undoSignature
                && recoveryInFlightRedoSignature_ == redoSignature
                && recoveryInFlightEventSerial_ == eventSerial
                && recoveryInFlightSemanticMutationSerial_ == semanticMutationSerial
                && recoveryInFlightEditorStateHash_ == editorStateHash
                && !authoredMutationPending)
            {
                return;
            }

        recoverySnapshotQueued_ = true;
        recoveryForcedSnapshotQueued_ = recoveryForcedSnapshotQueued_ || force;
        // There is only one recovery writer per MainWindow. Let this coherent
        // same-document checkpoint publish before capturing the queued newer
        // state. In particular, continuous edits during the first large encode
        // must not keep invalidating the only prospective recovery file. Save,
        // clean-state removal, and document switches explicitly advance the
        // generation and still cancel genuinely obsolete publications.
        return;
    }

    ui::recovery::WriteRequest request{};
    request.recoveryPath = currentRecoveryPath_;
    request.metadata.identity = recoveryIdentity_;
    request.metadata.sessionId = recoverySessionId_;
    request.metadata.sourcePath = absoluteDisplaySourcePath(currentPath_);
    request.metadata.displayName = recoveryDisplayName(currentPath_);
    request.metadata.timestamp = QDateTime::currentMSecsSinceEpoch();
    request.metadata.sourceModifiedAtMs = recoverySourceModifiedAtMs_;
    request.metadata.undoSignature = undoSignature;
    const bool resourceDraftDirty =
        !resourceEditorAuthoredStateMatchesSavedBaseline();
    request.metadata.requiresUserRecovery =
        recoveryRequiresSave_
        || document_.undoSignature() != savedUndoSignature_
        || recoveryCleaveAuthoredDirty_
        || resourceDraftDirty
        || (recoverySavedCogStateHash_.isEmpty()
                ? hasUnsavedCogScriptChanges()
                : authoredCogStateHash != recoverySavedCogStateHash_)
        || authoredMutationPending
        || recoveryAuthoredLevelDirty_;
    request.metadata.projectType = document_.level().kind;
    request.savedLevelStateHash = recoverySavedLevelHashKnown_
        ? recoverySavedLevelStateHash_ : QByteArray{};

    // Document is GUI-thread-owned. Make one coherent value snapshot here;
    // binary SED/FEC encoding, disk writes, flush, and atomic publication all
    // happen in the worker below.
    request.document = document_;
    request.document.level().lvisstring = buildLayerVisibilityString();
    request.editorStatePayload = editorStatePayload;
    request.publicationGate = recoveryPublicationGate_;
    request.generation = recoveryPublicationGate_->generation.fetch_add(
        1, std::memory_order_acq_rel) + 1;
    request.archivePreviousSnapshot = recoveryArchivePreviousOnNextSnapshot_;
    request.archiveSourcePath = recoveryArchiveSourcePath_;
    requestedArchivePreviousSnapshot = request.archivePreviousSnapshot;
    requestedArchiveSourcePath = request.archiveSourcePath;
    request.maximumHistoricalSnapshots = kMaximumAutosaveHistoryPerDocument;
#ifdef SED_APP_RECOVERY_TEST_ACCESS
    request.archivePublicationAllowedForTest =
        recoveryArchivePublicationAllowedForTest_;
    request.liveCommitAllowedForTest =
        recoveryLiveCommitAllowedForTest_;
#endif
    recoveryArchivePreviousOnNextSnapshot_ = false;
    recoveryArchiveSourcePath_.clear();

        recoveryInFlightIdentity_ = recoveryIdentity_;
        recoveryInFlightDocumentEpoch_ = recoveryDocumentEpoch_;
        recoveryInFlightPublicationGeneration_ = request.generation;
        recoveryInFlightPath_ = request.recoveryPath;
        recoveryInFlightUndoSignature_ = undoSignature;
        recoveryInFlightRedoSignature_ = redoSignature;
        recoveryInFlightEventSerial_ = eventSerial;
        recoveryInFlightSemanticMutationSerial_ = semanticMutationSerial;
        recoveryInFlightForced_ = force;
        recoveryInFlightArchivePreviousSnapshot_ =
            request.archivePreviousSnapshot;
        recoveryInFlightArchiveSourcePath_ = request.archiveSourcePath;
        recoveryInFlightEditorStateHash_ = editorStateHash;
        // The worker returns the authoritative Level digest in WriteResult.
        // Do not compute a second full Level digest on the GUI thread merely
        // to form a deduplication key.
        recoveryInFlightLevelStateHash_.clear();
        recoveryInFlightAuthoredMutationPending_ = authoredMutationPending;
        if (!recoveryPendingRetirementPath_.empty()
            && recoveryPendingRetirementDocumentEpoch_ == recoveryDocumentEpoch_
            && recoveryPathsEqual(
                recoveryPendingRetirementCurrentPath_, currentRecoveryPath_))
        {
            // A Save-As/recovery import binds retirement to the exact new
            // publication.  Stale callbacks cannot consume this binding.
            recoveryPendingRetirementPublicationGeneration_ = request.generation;
        }
    if (recoveryRetryTimer_) recoveryRetryTimer_->stop();

    if (!recoveryWriteWatcher_)
    {
        return;
    }
    recoveryWriteWatcher_->setFuture(QtConcurrent::run(
        recoveryThreadPool_,
        [request = std::move(request)]() mutable
        {
            return ui::recovery::writeRecoverySnapshot(std::move(request));
        }));
    // The pending bit belongs to this immutable request. Clear it only after
    // the worker has been accepted; a failed completion restores it below.
    recoveryAuthoredMutationPending_ = false;
    }
    catch (const std::exception& e)
    {
        if (requestedArchivePreviousSnapshot)
        {
            recoveryArchivePreviousOnNextSnapshot_ = true;
            if (recoveryArchiveSourcePath_.empty())
            {
                recoveryArchiveSourcePath_ = requestedArchiveSourcePath;
            }
        }
        recoveryRetryForceCapture_ = recoveryRetryForceCapture_ || force;
        recoveryAuthoredMutationPending_ = recoveryAuthoredMutationPending_
            || requestedAuthoredMutationPending;
        if (!recoveryFailureReported_)
        {
            recoveryFailureReported_ = true;
            logMessage(MessageSeverity::warning,
                QStringLiteral("Autosave recovery capture failed: %1")
                    .arg(QString::fromUtf8(e.what())));
        }
        if (recoveryRetryTimer_ && hasRecoverableChanges())
        {
            const int shift = std::min(recoveryRetryAttempt_, 4);
            recoveryRetryTimer_->start(std::min(
                kRecoveryRetryInitialMilliseconds * (1 << shift),
                kRecoveryRetryMaximumMilliseconds));
            ++recoveryRetryAttempt_;
        }
    }
    catch (...)
    {
        if (requestedArchivePreviousSnapshot)
        {
            recoveryArchivePreviousOnNextSnapshot_ = true;
            if (recoveryArchiveSourcePath_.empty())
            {
                recoveryArchiveSourcePath_ = requestedArchiveSourcePath;
            }
        }
        recoveryRetryForceCapture_ = recoveryRetryForceCapture_ || force;
        recoveryAuthoredMutationPending_ = recoveryAuthoredMutationPending_
            || requestedAuthoredMutationPending;
        if (!recoveryFailureReported_)
        {
            recoveryFailureReported_ = true;
            logMessage(MessageSeverity::warning,
                QStringLiteral("Autosave recovery capture failed unexpectedly."));
        }
        if (recoveryRetryTimer_ && hasRecoverableChanges())
        {
            const int shift = std::min(recoveryRetryAttempt_, 4);
            recoveryRetryTimer_->start(std::min(
                kRecoveryRetryInitialMilliseconds * (1 << shift),
                kRecoveryRetryMaximumMilliseconds));
            ++recoveryRetryAttempt_;
        }
    }
}

void MainWindow::handleRecoveryWriteFinished()
{
    if (!recoveryWriteWatcher_)
    {
        return;
    }

    const ui::recovery::WriteResult result = recoveryWriteWatcher_->result();
    // Snapshot the request identity before clearing it below.  A writer can
    // finish successfully just before Save As/document replacement advances
    // the gate, then deliver this callback afterwards.  Such a completion is
    // stale even though the worker reported `published` and must not update
    // the new document's baseline or retire its pending path.
    const QString inFlightIdentity = recoveryInFlightIdentity_;
    const std::uint64_t inFlightDocumentEpoch = recoveryInFlightDocumentEpoch_;
    const std::uint64_t inFlightGeneration = recoveryInFlightPublicationGeneration_;
    const std::filesystem::path inFlightPath = recoveryInFlightPath_;
    const std::uint64_t inFlightUndoSignature = recoveryInFlightUndoSignature_;
    const std::uint64_t inFlightRedoSignature = recoveryInFlightRedoSignature_;
    const std::uint64_t inFlightSemanticMutationSerial =
        recoveryInFlightSemanticMutationSerial_;
    const QByteArray inFlightEditorStateHash = recoveryInFlightEditorStateHash_;
    const bool inFlightAuthoredMutationPending =
        recoveryInFlightAuthoredMutationPending_;
    const bool inFlightArchivePreviousSnapshot =
        recoveryInFlightArchivePreviousSnapshot_;
    const std::filesystem::path inFlightArchiveSourcePath =
        recoveryInFlightArchiveSourcePath_;
    const bool completionMatchesCurrent = recoveryPathsEqual(
            result.recoveryPath, inFlightPath)
        && !inFlightPath.empty()
        && recoveryPathsEqual(inFlightPath, currentRecoveryPath_)
        && inFlightIdentity == recoveryIdentity_
        && inFlightDocumentEpoch == recoveryDocumentEpoch_
        && inFlightGeneration != 0
        && recoveryPublicationGate_
        && recoveryPublicationGate_->generation.load(std::memory_order_acquire)
            == inFlightGeneration;

    if (result.status == ui::recovery::WriteStatus::published)
    {
        if (completionMatchesCurrent)
        {
            // Compare the canonical key, not merely the event serial.  A
            // burst of identical viewport notifications while A is in flight
            // must not cause a second write, while A->B->A keeps the queued A
            // request alive until it is considered after the older worker.
            const std::uint64_t currentUndoSignature = document_.undoSignature();
            const std::uint64_t currentRedoSignature = document_.redoSignature();
            const QByteArray currentEditorStatePayload = captureRecoveryEditorState();
            const QByteArray currentEditorStateHash = QCryptographicHash::hash(
                currentEditorStatePayload, QCryptographicHash::Sha256);
            const bool currentKeyMatchesInFlight = currentUndoSignature == inFlightUndoSignature
                && currentRedoSignature == inFlightRedoSignature
                && recoveryEventSerial_ == recoveryInFlightEventSerial_
                && recoverySemanticMutationSerial_ == inFlightSemanticMutationSerial
                && currentEditorStateHash == inFlightEditorStateHash;
            const bool newerStatePending = recoverySnapshotQueued_
                || recoveryForcedSnapshotQueued_
                || !currentKeyMatchesInFlight;
            recoveryLastPublishedIdentity_ = inFlightIdentity;
            recoveryLastPublishedDocumentEpoch_ = inFlightDocumentEpoch;
            recoveryLastPublishedPath_ = inFlightPath;
            recoveryLastPublishedUndoSignature_ = inFlightUndoSignature;
            recoveryLastPublishedRedoSignature_ = inFlightRedoSignature;
            recoveryLastPublishedEventSerial_ = recoveryInFlightEventSerial_;
            recoveryLastPublishedSemanticMutationSerial_ =
                inFlightSemanticMutationSerial;
            recoveryLastPublishedEditorStateHash_ = inFlightEditorStateHash;
            recoveryLastPublishedLevelStateHash_ = result.levelStateHash;
            recoveryLastPublishedValid_ = true;
            // The published checkpoint is the new deduplication baseline.
            // A later Undo/Redo may legitimately return to a history pair
            // seen before this publication; do not treat that pair as the
            // already-accounted transition from the new baseline.
            recoveryLastNotedSemanticHistoryValid_ = false;
            recoverySnapshotRequired_ = newerStatePending;
            recoveryRetryForceCapture_ = false;
            recoveryFailureReported_ = false;
            recoveryRetryAttempt_ = 0;
            if (recoveryRetryTimer_) recoveryRetryTimer_->stop();

            // Save-As and imported legacy/copy snapshots bind retirement to
            // this exact current request.  A stale old-generation callback
            // cannot consume the pending path.
            const bool retirementBindingMatches = !recoveryPendingRetirementPath_.empty()
                && !ui::recovery::isAutosaveHistoryPath(
                    recoveryPendingRetirementPath_)
                && recoveryPathsEqual(
                    recoveryPendingRetirementCurrentPath_, currentRecoveryPath_)
                && recoveryPendingRetirementDocumentEpoch_ == recoveryDocumentEpoch_
                && recoveryPendingRetirementPublicationGeneration_ == inFlightGeneration;
            if (retirementBindingMatches)
            {
                QString retirementError{};
                if (ui::recovery::invalidateAndRemoveRecovery(
                        nullptr, recoveryPendingRetirementPath_,
                        &retirementError,
                        recoveryPendingRetirementMetadata_
                            ? &*recoveryPendingRetirementMetadata_
                            : nullptr))
                {
                    recoveryPendingRetirementPath_.clear();
                    recoveryPendingRetirementMetadata_.reset();
                    recoveryPendingRetirementDocumentEpoch_ = 0;
                    recoveryPendingRetirementPublicationGeneration_ = 0;
                    recoveryPendingRetirementCurrentPath_.clear();
                }
                else if (!recoveryShuttingDown_)
                {
                    logMessage(MessageSeverity::warning, retirementError);
                }
            }
            if (!result.warning.isEmpty() && !recoveryShuttingDown_)
            {
                logMessage(MessageSeverity::warning, result.warning);
            }
        }
    }
    else if (result.status == ui::recovery::WriteStatus::failed
        && completionMatchesCurrent && !recoveryShuttingDown_)
    {
        // The request consumed a same-undo authored marker before entering
        // the worker. Restore it on failure so the retry cannot publish a
        // misleading clean metadata record.
        recoveryAuthoredMutationPending_ = recoveryAuthoredMutationPending_
            || inFlightAuthoredMutationPending;
        recoveryRetryForceCapture_ = recoveryRetryForceCapture_ || recoveryInFlightForced_;
        if (!recoveryFailureReported_)
        {
            recoveryFailureReported_ = true;
            logMessage(
                MessageSeverity::warning,
                QStringLiteral("Autosave recovery failed and will retry: %1").arg(result.error));
        }
        if (recoveryRetryTimer_ && hasRecoverableChanges())
        {
            const int shift = std::min(recoveryRetryAttempt_, 4);
            const int delay = std::min(
                kRecoveryRetryInitialMilliseconds * (1 << shift),
                kRecoveryRetryMaximumMilliseconds);
            ++recoveryRetryAttempt_;
            recoveryRetryTimer_->start(delay);
        }
    }

    if (result.status == ui::recovery::WriteStatus::failed
        && completionMatchesCurrent
        && inFlightArchivePreviousSnapshot)
    {
        // A rotation request owns the obligation to preserve the preceding
        // checked live bytes. Capture, archive publication, and live commit
        // can all fail after the GUI state has been consumed into the async
        // request. Keep that obligation on the same epoch/generation until a
        // retry publishes successfully. A superseded callback is deliberately
        // excluded: document switches and explicit discard advance the gate
        // and must not resurrect an obsolete rotation. Repeating archive-first after a live-commit
        // failure can create a harmless duplicate, which successful pruning
        // immediately bounds; dropping the intent could overwrite the only
        // previous checkpoint.
        recoveryArchivePreviousOnNextSnapshot_ = true;
        if (recoveryArchiveSourcePath_.empty())
        {
            recoveryArchiveSourcePath_ = inFlightArchiveSourcePath;
        }
    }

    recoveryInFlightIdentity_.clear();
    recoveryInFlightDocumentEpoch_ = 0;
    recoveryInFlightPublicationGeneration_ = 0;
    recoveryInFlightPath_.clear();
    recoveryInFlightUndoSignature_ = 0;
    recoveryInFlightRedoSignature_ = 0;
    recoveryInFlightEventSerial_ = 0;
    recoveryInFlightSemanticMutationSerial_ = 0;
    recoveryInFlightForced_ = false;
    recoveryInFlightArchivePreviousSnapshot_ = false;
    recoveryInFlightArchiveSourcePath_.clear();
    recoveryInFlightEditorStateHash_.clear();
    recoveryInFlightLevelStateHash_.clear();
    recoveryInFlightAuthoredMutationPending_ = false;

    if (recoverySnapshotQueued_ && !recoveryShuttingDown_)
    {
        const bool forceQueued = recoveryForcedSnapshotQueued_;
        recoverySnapshotQueued_ = false;
        recoveryForcedSnapshotQueued_ = false;
        if (recoveryFlushDraining_)
        {
            scheduleRecoverySnapshot(forceQueued);
        }
        else
        {
            QTimer::singleShot(0, this, [this, forceQueued]()
                { scheduleRecoverySnapshot(forceQueued); });
        }
    }
    else if (!completionMatchesCurrent && !recoveryShuttingDown_
        && recoverySnapshotRequired_
        && (!recoveryWriteWatcher_ || !recoveryWriteWatcher_->isRunning()))
    {
        // A document switch can clear the queued bit while the old writer is
        // still in flight.  Once that stale callback arrives, make sure the
        // new epoch still gets a baseline even if its caller has already
        // returned.
        if (recoveryFlushDraining_)
        {
            scheduleRecoverySnapshot();
        }
        else
        {
            QTimer::singleShot(0, this, [this]() { scheduleRecoverySnapshot(); });
        }
    }
}

void MainWindow::switchRecoveryDocument(
    const std::filesystem::path& sourcePath,
    const bool newUntitledSession,
    const bool removePreviousRecovery,
    const bool preserveDocumentLifetime)
{
    if (!recoveryPublicationGate_)
    {
        recoveryPublicationGate_ = std::make_shared<ui::recovery::PublicationGate>();
    }

    // Recovery epochs bind asynchronous publication callbacks and destinations.
    // A successful native Save rotates that publication without replacing the
    // live Document, so its separate lifetime generation remains stable.
    ++recoveryDocumentEpoch_;
    if (!preserveDocumentLifetime)
    {
        ++documentLifetimeGeneration_;
    }
    // A Preview visibility snapshot belongs to one document lifetime. Never
    // resurrect an old document's editor panes after an A -> B switch.
    previewEditingUiVisibility_ = {};

    if (removePreviousRecovery && !currentRecoveryPath_.empty())
    {
        (void)ui::recovery::invalidateAndRemoveRecovery(
            recoveryPublicationGate_,
            currentRecoveryPath_);
        if (!recoveryPendingRetirementPath_.empty())
        {
            (void)ui::recovery::invalidateAndRemoveRecovery(
                nullptr, recoveryPendingRetirementPath_, nullptr,
                recoveryPendingRetirementMetadata_
                    ? &*recoveryPendingRetirementMetadata_
                    : nullptr);
            recoveryPendingRetirementPath_.clear();
            recoveryPendingRetirementMetadata_.reset();
            recoveryPendingRetirementDocumentEpoch_ = 0;
            recoveryPendingRetirementPublicationGeneration_ = 0;
            recoveryPendingRetirementCurrentPath_.clear();
        }
    }
    else
    {
        recoveryPublicationGate_->generation.fetch_add(1, std::memory_order_acq_rel);
        // A non-destructive document switch starts a new lifetime.  Any
        // retirement binding from the previous lifetime is no longer safe to
        // consume, even if its file remains discoverable for diagnostics.
        recoveryPendingRetirementPath_.clear();
        recoveryPendingRetirementMetadata_.reset();
        recoveryPendingRetirementDocumentEpoch_ = 0;
        recoveryPendingRetirementPublicationGeneration_ = 0;
        recoveryPendingRetirementCurrentPath_.clear();
    }
    if (recoveryPendingRetirementPath_.empty())
    {
        recoveryPendingRetirementDocumentEpoch_ = 0;
        recoveryPendingRetirementPublicationGeneration_ = 0;
        recoveryPendingRetirementCurrentPath_.clear();
    }

    if (newUntitledSession || recoverySessionId_.trimmed().isEmpty())
    {
        recoverySessionId_ = ui::recovery::newSessionId();
    }
    recoveryIdentity_ = ui::recovery::documentIdentity(sourcePath, recoverySessionId_);
    currentRecoveryPath_ = ui::recovery::recoveryFilePath(
        recoveryDirectory(), recoveryIdentity_, recoverySessionId_,
        recoveryDisplayName(sourcePath), document_.level().kind);
    recoverySourceModifiedAtMs_ = ui::recovery::sourceModifiedAtMs(sourcePath);
    recoveryEventSerial_ = 0;
    recoverySemanticMutationSerial_ = 0;
    recoveryAuthoredMutationPending_ = false;
    recoveryAuthoredLevelDirty_ = false;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    // A direct-mutation test seam can be used while constructing the Level
    // before this document lifetime begins.  That mutation is already part of
    // the saved baseline established by the caller; carrying the marker into
    // the new lifetime would misclassify the first EDIT-only change as
    // authored work and rotate a spurious historical autosave.
    recoveryTestMutationPending_ = false;
#endif
    recoveryInFlightAuthoredMutationPending_ = false;
    recoveryLastNotedSemanticUndoSignature_ = 0;
    recoveryLastNotedSemanticRedoSignature_ = 0;
    recoveryLastNotedSemanticHistoryValid_ = false;
    recoveryLastPublishedValid_ = false;
    recoveryLastPublishedIdentity_.clear();
    recoveryLastPublishedDocumentEpoch_ = 0;
    recoveryLastPublishedPath_.clear();
    recoveryLastPublishedUndoSignature_ = 0;
    recoveryLastPublishedRedoSignature_ = 0;
    recoveryLastPublishedEventSerial_ = 0;
    recoveryLastPublishedSemanticMutationSerial_ = 0;
    recoveryLastPublishedEditorStateHash_.clear();
    recoveryLastPublishedLevelStateHash_.clear();
    recoverySavedLevelStateHash_.clear();
    recoverySavedLevelHashKnown_ = false;
    recoverySavedCogState_.clear();
    recoverySavedCogStateHash_.clear();
    recoverySnapshotRequired_ = true;
    // Keep the old in-flight request fields until its callback is consumed.
    // Clearing them here would make the callback indistinguishable from a
    // missing request and would hide useful stale-generation diagnostics.
    recoverySnapshotQueued_ = false;
    recoveryForcedSnapshotQueued_ = false;
    recoveryRetryForceCapture_ = false;
    recoveryImmediateSnapshotPending_ = false;
    recoveryCaptureDeferredForFly_ = false;
    recoveryAuthoredCaptureDeferredForFly_ = false;
    recoveryArchivePreviousOnNextSnapshot_ = false;
    recoveryArchiveSourcePath_.clear();
    recoveryRetryAttempt_ = 0;
    recoveryFailureReported_ = false;
    if (recoveryRetryTimer_) recoveryRetryTimer_->stop();
}

void MainWindow::completeRecoveryAfterNormalSave(const std::filesystem::path& savedPath)
{
    // The just-committed native SED is already the authoritative, atomic
    // Level+EDIT(+HIST) checkpoint. EDIT includes every unsaved COG buffer, so
    // do not synchronously encode the same document a second time merely to
    // clean recovery metadata. Instead invalidate the old writer/session and
    // retire its live automatic offer immediately. The clean hard-kill
    // baseline is republished asynchronously below; existing periodic
    // `.sedautosave` history remains available to the user.
    const QString previousRecoveryIdentity = recoveryIdentity_;
    const QString previousRecoverySessionId = recoverySessionId_;
    const std::filesystem::path previousRecoveryPath = currentRecoveryPath_;
    const std::filesystem::path protectedRecoveryPath =
        recoveryProtectedPathDuringSave_;
    const PreviewEditingUiVisibility previewVisibilityBeforeSave =
        previewEditingUiVisibility_;
    const bool preservePreviewVisibility = glView_
        && glView_->renderPipeline() == GlView::RenderPipeline::GamePreview
        && previewVisibilityBeforeSave.valid
        && previewVisibilityBeforeSave.documentEpoch == recoveryDocumentEpoch_;
    currentPath_ = savedPath;
    for (const QPointer<QWidget>& page : resourceEditorPages())
    {
        if (page)
        {
            ui::resources::resourceAuthoringPaneRebindProjectDirectory(
                page.data(), savedPath.parent_path());
        }
    }
    switchRecoveryDocument(savedPath, false, false, true);
    if (preservePreviewVisibility)
    {
        // Save/Save As rekeys recovery publication but does not replace the
        // live document. Keep the transient Preview visibility contract and
        // bind it to the new publication epoch; true Open/New paths still
        // invalidate their old-document snapshot in switchRecoveryDocument.
        previewEditingUiVisibility_ = previewVisibilityBeforeSave;
        previewEditingUiVisibility_.documentEpoch = recoveryDocumentEpoch_;
    }

    // Close the check/commit race with an old in-flight writer at its exact
    // known slot first. A worker that had already reached publication cannot
    // resurrect the stale dirty offer after this call returns.
    QString retirementError{};
    if (!previousRecoveryPath.empty()
        && !recoveryPathsEqual(previousRecoveryPath, protectedRecoveryPath)
        && !ui::recovery::invalidateAndRemoveRecovery(
            recoveryPublicationGate_, previousRecoveryPath, &retirementError)
        && !retirementError.isEmpty())
    {
        logMessage(MessageSeverity::warning, retirementError);
    }
    discardRecoveryLiveFamily(
        previousRecoveryIdentity, previousRecoverySessionId,
        protectedRecoveryPath);
    if (previousRecoveryIdentity != recoveryIdentity_
        || previousRecoverySessionId != recoverySessionId_)
    {
        // An A -> B -> A path sequence in one process can otherwise expose an
        // older same-session A slot before the asynchronous clean baseline is
        // ready. Retire only live slots; never remove its periodic history.
        discardRecoveryLiveFamily(
            recoveryIdentity_, recoverySessionId_, protectedRecoveryPath);
    }

    recoveryPendingRetirementPath_.clear();
    recoveryPendingRetirementMetadata_.reset();
    recoveryPendingRetirementDocumentEpoch_ = 0;
    recoveryPendingRetirementPublicationGeneration_ = 0;
    recoveryPendingRetirementCurrentPath_.clear();

    // Establish the clean Level+EDIT baseline only after the new source/path
    // identity exists. Its metadata is explicitly ineligible for an automatic
    // recovery prompt.
    markCurrentDocumentAsSaved();
    markRecoveryWorkspaceAsSaved();
    ++recoveryEventSerial_;
    ++recoverySemanticMutationSerial_;
    recoverySnapshotRequired_ = true;
    if (recoveryTimer_ && recoveryTimer_->isActive())
    {
        QTimer::singleShot(0, this, [this]() { scheduleRecoverySnapshot(); });
    }
}

void MainWindow::discardCurrentRecovery(
    const std::filesystem::path& preservedPath)
{
    recoverySnapshotQueued_ = false;
    recoveryForcedSnapshotQueued_ = false;
    recoveryRetryForceCapture_ = false;
    recoverySnapshotRequired_ = false;
    recoveryAuthoredMutationPending_ = false;
    recoveryAuthoredLevelDirty_ = false;
    recoveryInFlightAuthoredMutationPending_ = false;
    recoveryCaptureDeferredForFly_ = false;
    recoveryAuthoredCaptureDeferredForFly_ = false;
    recoveryRetryAttempt_ = 0;
    recoveryFailureReported_ = false;
    if (recoveryRetryTimer_) recoveryRetryTimer_->stop();
    if (!recoveryPendingRetirementPath_.empty())
    {
        if (!recoveryPathsEqual(
                recoveryPendingRetirementPath_, preservedPath))
        {
            // A historical restore source is intentionally not retired here.
            // It remains a user-visible Autosave History point until an
            // explicit Delete or retention pruning removes it. Only a
            // non-history publication-bound recovery input may be consumed by
            // this automatic cleanup path.
            if (!ui::recovery::isAutosaveHistoryPath(
                    recoveryPendingRetirementPath_))
            {
                (void)ui::recovery::invalidateAndRemoveRecovery(
                    nullptr, recoveryPendingRetirementPath_, nullptr,
                    recoveryPendingRetirementMetadata_
                        ? &*recoveryPendingRetirementMetadata_
                        : nullptr);
            }
            recoveryPendingRetirementPath_.clear();
            recoveryPendingRetirementMetadata_.reset();
            recoveryPendingRetirementDocumentEpoch_ = 0;
            recoveryPendingRetirementPublicationGeneration_ = 0;
            recoveryPendingRetirementCurrentPath_.clear();
        }
    }
    if (!recoveryPublicationGate_)
    {
        return;
    }
    if (currentRecoveryPath_.empty())
    {
        recoveryPublicationGate_->generation.fetch_add(1, std::memory_order_acq_rel);
        recoveryLastPublishedValid_ = false;
        recoveryLastPublishedIdentity_.clear();
        recoveryLastPublishedDocumentEpoch_ = 0;
        recoveryLastPublishedPath_.clear();
        recoveryLastPublishedUndoSignature_ = 0;
        recoveryLastPublishedRedoSignature_ = 0;
        recoveryLastPublishedEventSerial_ = 0;
        recoveryLastPublishedSemanticMutationSerial_ = 0;
        recoveryLastNotedSemanticUndoSignature_ = 0;
        recoveryLastNotedSemanticRedoSignature_ = 0;
        recoveryLastNotedSemanticHistoryValid_ = false;
        recoveryLastPublishedEditorStateHash_.clear();
        recoveryLastPublishedLevelStateHash_.clear();
        return;
    }

    QString error{};
    if (!recoveryPathsEqual(currentRecoveryPath_, preservedPath)
        && !ui::recovery::invalidateAndRemoveRecovery(
            recoveryPublicationGate_, currentRecoveryPath_, &error)
        && !recoveryShuttingDown_)
    {
        logMessage(MessageSeverity::warning, error);
    }
    recoveryLastPublishedValid_ = false;
    recoveryLastPublishedIdentity_.clear();
    recoveryLastPublishedDocumentEpoch_ = 0;
    recoveryLastPublishedPath_.clear();
    recoveryLastPublishedUndoSignature_ = 0;
    recoveryLastPublishedRedoSignature_ = 0;
    recoveryLastPublishedEventSerial_ = 0;
    recoveryLastPublishedSemanticMutationSerial_ = 0;
    recoveryLastNotedSemanticUndoSignature_ = 0;
    recoveryLastNotedSemanticRedoSignature_ = 0;
    recoveryLastNotedSemanticHistoryValid_ = false;
    recoveryLastPublishedEditorStateHash_.clear();
    recoveryLastPublishedLevelStateHash_.clear();
}

void MainWindow::discardRecoveryFamily(
    const QString& identity,
    const QString& sessionId,
    const std::filesystem::path& preservedPath)
{
    const QString normalizedIdentity = identity.trimmed();
    const QString normalizedSessionId = sessionId.trimmed();
    if (normalizedIdentity.isEmpty() || normalizedSessionId.isEmpty()) return;

    // This legacy-named helper is used by Open/New/Discard callers. It is
    // deliberately live-only now: periodic `.sedautosave` entries are user
    // history and may be removed only by the explicit Autosave History Delete
    // action or by the writer's post-publication retention prune.
    if (normalizedIdentity == recoveryIdentity_
        && normalizedSessionId == recoverySessionId_
        && !recoveryPathsEqual(currentRecoveryPath_, preservedPath))
    {
        discardCurrentRecovery(preservedPath);
    }
    discardRecoveryLiveFamily(
        normalizedIdentity, normalizedSessionId, preservedPath);
}

void MainWindow::discardRecoveryLiveFamily(
    const QString& identity,
    const QString& sessionId,
    const std::filesystem::path& preservedPath)
{
    const QString normalizedIdentity = identity.trimmed();
    const QString normalizedSessionId = sessionId.trimmed();
    if (normalizedIdentity.isEmpty() || normalizedSessionId.isEmpty()) return;

    std::vector<std::filesystem::path> removedPaths{};
    for (const std::filesystem::path& directory : recoverySearchDirectories())
    {
        // Automatic discovery intentionally returns only `.sedrecovery` live
        // slots. Keeping this separate from scanAutosaveHistory() is what
        // preserves the user's bounded periodic restore points.
        for (const ui::recovery::RecoveryDescriptor& descriptor
             : ui::recovery::scanRecoveryFamily(
                 directory, normalizedIdentity, normalizedSessionId))
        {
            if (!QFileInfo(QString::fromStdWString(
                    descriptor.recoveryPath.wstring())).fileName().endsWith(
                        QStringLiteral(".sedrecovery"), Qt::CaseInsensitive))
            {
                continue;
            }
            if (descriptor.metadata.identity != normalizedIdentity
                || descriptor.metadata.sessionId != normalizedSessionId
                || recoveryPathsEqual(descriptor.recoveryPath, preservedPath)
                || std::any_of(removedPaths.begin(), removedPaths.end(),
                    [&descriptor](const std::filesystem::path& removedPath)
                    {
                        return recoveryPathsEqual(
                            removedPath, descriptor.recoveryPath);
                    }))
            {
                continue;
            }
            removedPaths.push_back(descriptor.recoveryPath);
            QString removalError{};
            const auto gate = recoveryPathsEqual(
                    descriptor.recoveryPath, currentRecoveryPath_)
                ? recoveryPublicationGate_
                : std::shared_ptr<ui::recovery::PublicationGate>{};
            if (!ui::recovery::invalidateAndRemoveRecovery(
                    gate, descriptor.recoveryPath, &removalError,
                    &descriptor.metadata)
                && !removalError.isEmpty() && !recoveryShuttingDown_)
            {
                logMessage(MessageSeverity::warning, removalError);
            }
        }
    }
}

bool MainWindow::removeRecoveryPathSafely(
    const std::filesystem::path& path,
    QString* error,
    const ui::recovery::RecoveryMetadata* expectedMetadata)
{
    if (path.empty()) return true;
    // This entry point is used by the explicit Delete action in Autosave
    // History. Keep the intent explicit at the call site and reject a live
    // recovery path even if a future UI scan accidentally supplies one.
    if (!ui::recovery::isAutosaveHistoryPath(path))
    {
        if (error)
        {
            *error = QStringLiteral(
                "Only .sedautosave history entries can be deleted from Autosave History.");
        }
        return false;
    }
    if (recoveryPathsEqual(path, currentRecoveryPath_))
    {
        // The history dialog is modal but the recovery worker remains live.
        // Authenticate the exact row under the publication gate: a newer
        // atomic replacement must survive a stale Delete click. In either
        // case the gate invalidation may supersede a staged newer capture, so
        // keep/re-arm the current workspace's crash checkpoint rather than
        // turning Delete into a permanent recovery disable for this session.
        const bool removed = ui::recovery::invalidateAndRemoveRecovery(
            recoveryPublicationGate_, path, error, expectedMetadata,
            ui::recovery::RecoveryRemovalReason::explicitHistoryDelete);
        if (!removed)
        {
            recoverySnapshotRequired_ = true;
            if (!recoveryPublicationPaused_ && !recoveryShuttingDown_)
            {
                QTimer::singleShot(0, this,
                    [this]() { scheduleRecoverySnapshot(); });
            }
            return false;
        }
        discardCurrentRecovery(path);
        recoverySnapshotRequired_ = true;
        if (!recoveryPublicationPaused_ && !recoveryShuttingDown_)
        {
            QTimer::singleShot(0, this,
                [this]() { scheduleRecoverySnapshot(true); });
        }
        return true;
    }
    return ui::recovery::invalidateAndRemoveRecovery(
        nullptr, path, error, expectedMetadata,
        ui::recovery::RecoveryRemovalReason::explicitHistoryDelete);
}

void MainWindow::showAutosaveHistory()
{
    for (;;)
    {
        std::vector<ui::recovery::RecoveryDescriptor> descriptors{};
        for (const std::filesystem::path& directory : recoverySearchDirectories())
        {
            for (ui::recovery::RecoveryDescriptor descriptor
                 : ui::recovery::scanAutosaveHistory(directory))
            {
                if (!descriptor.metadata.requiresUserRecovery)
                {
                    continue;
                }
                const bool duplicate = std::any_of(
                    descriptors.begin(), descriptors.end(),
                    [&descriptor](const ui::recovery::RecoveryDescriptor& value)
                    {
                        return recoveryPathsEqual(
                            value.recoveryPath, descriptor.recoveryPath);
                    });
                if (!duplicate) descriptors.push_back(std::move(descriptor));
            }
        }
        std::sort(descriptors.begin(), descriptors.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.metadata.timestamp > rhs.metadata.timestamp;
            });
        if (descriptors.empty())
        {
            QMessageBox::information(
                this,
                QStringLiteral("Autosave history"),
                QStringLiteral("No recoverable autosave history is available."));
            return;
        }

        RecoveryOfferDialog dialog(descriptors, RecoveryDialogMode::history, this);
        (void)dialog.exec();
        const auto selected = dialog.selectedDescriptor();
        if (dialog.action() == RecoveryPromptAction::recover)
        {
            const bool wasPublicationPaused = recoveryPublicationPaused_;
            recoveryPublicationPaused_ = true;
            if (recoveryDebounceTimer_) recoveryDebounceTimer_->stop();
            if (recoveryRetryTimer_) recoveryRetryTimer_->stop();
            if (recoveryPublicationGate_)
            {
                recoveryPublicationGate_->generation.fetch_add(
                    1, std::memory_order_acq_rel);
            }
            const auto publicationPauseGuard = qScopeGuard(
                [this, wasPublicationPaused]()
                {
                    recoveryPublicationPaused_ = wasPublicationPaused;
                    if (!recoveryPublicationPaused_ && !recoveryShuttingDown_)
                    {
                        configureRecoveryTimerFromSettings();
                        if (recoverySnapshotRequired_)
                        {
                            QTimer::singleShot(0, this,
                                [this]() { scheduleRecoverySnapshot(); });
                        }
                    }
                });
            QString deferredDiscardIdentity{};
            QString deferredDiscardSessionId{};
            if (!authorizeDocumentReplacement(
                    QStringLiteral("opening an autosave"),
                    selected.recoveryPath,
                    &deferredDiscardIdentity,
                    &deferredDiscardSessionId))
            {
                continue;
            }
            if (applyRecoveryFile(selected.recoveryPath)
                && !deferredDiscardIdentity.isEmpty())
            {
                discardRecoveryFamily(
                    deferredDiscardIdentity,
                    deferredDiscardSessionId,
                    currentRecoveryPath_);
            }
            return;
        }
        if (dialog.action() == RecoveryPromptAction::discard)
        {
            const QString source = selected.metadata.sourcePath.trimmed().isEmpty()
                ? QStringLiteral("Unsaved level")
                : QDir::toNativeSeparators(selected.metadata.sourcePath);
            const QString when = QDateTime::fromMSecsSinceEpoch(
                selected.metadata.timestamp).toLocalTime().toString(
                    QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            QMessageBox confirmation(
                QMessageBox::Warning,
                QStringLiteral("Delete autosave"),
                QStringLiteral(
                    "Permanently delete this autosave?\n\n%1\nSnapshot: %2\n\nThis cannot be undone.")
                    .arg(source, when),
                QMessageBox::Cancel,
                this);
            QAbstractButton* deleteButton = confirmation.addButton(
                QStringLiteral("Delete"), QMessageBox::DestructiveRole);
            confirmation.setDefaultButton(QMessageBox::Cancel);
            confirmation.exec();
            if (confirmation.clickedButton() != deleteButton)
            {
                continue;
            }
            QString removalError{};
            if (!removeRecoveryPathSafely(
                    selected.recoveryPath, &removalError,
                    &selected.metadata))
            {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Delete autosave"),
                    removalError.isEmpty()
                        ? QStringLiteral("Could not delete the selected autosave.")
                        : removalError);
            }
            continue;
        }
        return;
    }
}

void MainWindow::shutdownRecoverySupport(const bool removePublishedRecovery)
{
    recoveryShuttingDown_ = true;
    recoveryFlushDraining_ = false;
    recoverySnapshotQueued_ = false;
    recoveryForcedSnapshotQueued_ = false;
    if (recoveryTimer_)
    {
        recoveryTimer_->stop();
    }
    if (recoveryDebounceTimer_)
    {
        recoveryDebounceTimer_->stop();
    }
    if (recoveryRetryTimer_)
    {
        recoveryRetryTimer_->stop();
    }

    if (removePublishedRecovery)
    {
        discardCurrentRecovery();
        // A prior fallback directory or path spelling can still contain an
        // older live slot for this exact session. A deliberate clean close
        // must not leave it available as an automatic startup offer; bounded
        // `.sedautosave` history is intentionally retained.
        discardRecoveryLiveFamily(recoveryIdentity_, recoverySessionId_);
    }
    else if (recoveryPublicationGate_)
    {
        recoveryPublicationGate_->generation.fetch_add(1, std::memory_order_acq_rel);
    }
}

bool MainWindow::applyRecoveryFile(const std::filesystem::path& recoveryPath)
{
    using ui::audio::AppSoundPlayer;

    startupRecoveryEditorStateRestored_ = false;

    // Startup recovery runs before main.cpp shows the MainWindow. Keep its
    // progress and error/choice dialogs as visible top-level windows instead
    // of assigning them a hidden native owner. Normal in-window recovery keeps
    // the MainWindow as owner so its dialogs remain grouped with the editor.
    QWidget* const recoveryDialogParent = isVisible() ? this : nullptr;

    const auto replacementFingerprintGuard = qScopeGuard(
        [this]() { clearReplacementAuthorizationFingerprint(); });

    const bool wasPublicationPaused = recoveryPublicationPaused_;
    recoveryPublicationPaused_ = true;
    if (recoveryDebounceTimer_) recoveryDebounceTimer_->stop();
    if (recoveryRetryTimer_) recoveryRetryTimer_->stop();
    const auto publicationPauseGuard = qScopeGuard(
        [this, wasPublicationPaused]()
        {
            recoveryPublicationPaused_ = wasPublicationPaused;
            if (!recoveryPublicationPaused_ && !recoveryShuttingDown_)
            {
                configureRecoveryTimerFromSettings();
                if (recoverySnapshotRequired_)
                {
                    QTimer::singleShot(0, this,
                        [this]() { scheduleRecoverySnapshot(); });
                }
            }
        });

    QProgressDialog progress(recoveryDialogParent);
    progress.setObjectName(QStringLiteral("recovery_progress_dialog"));
    progress.setWindowTitle(QStringLiteral("Recovering SED session"));
    progress.setWindowIcon(loadApplicationIcon());
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setWindowFlags(
        Qt::Window
        | Qt::CustomizeWindowHint
        | Qt::WindowTitleHint
        | Qt::MSWindowsFixedSizeDialogHint);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.setRange(0, 0);
    progress.setLabelText(QStringLiteral("Validating and decoding the recovery file..."));
    progress.resize(560, 138);
    progress.show();
    QTimer::singleShot(0, &progress, [&progress]()
        {
            progress.setFixedSize(std::max(560, progress.sizeHint().width()),
                std::max(138, progress.sizeHint().height()));
            applyWindowsTaskbarIcon(progress);
        });

    auto loadFuture = QtConcurrent::run(
        [recoveryPath]() { return readAndDecodeRecovery(recoveryPath); });
    QFutureWatcher<RecoveryLoadResult> loadWatcher{};
    QEventLoop loadLoop{};
    QObject::connect(&loadWatcher, &QFutureWatcher<RecoveryLoadResult>::finished,
        &loadLoop, &QEventLoop::quit);
    loadWatcher.setFuture(loadFuture);
    if (!loadWatcher.isFinished())
    {
        loadLoop.exec();
    }
    RecoveryLoadResult loaded = loadFuture.takeResult();
    QString error = std::move(loaded.error);
    std::optional<ui::recovery::RecoveryRecord> record = std::move(loaded.record);
    if (!record)
    {
        progress.close();
        QMessageBox::critical(
            recoveryDialogParent, QStringLiteral("Recovery failed"), error);
        return false;
    }

    appendRecoveryAudit(recoveryDirectory(), *record, QStringLiteral("validated"));

    std::optional<ui::recovery::RecoveredDocument> recovered = std::move(loaded.recovered);
    if (loaded.requiresProjectSelection)
    {
        const QStringList projectChoices{
            recoveryProjectTypeLabel(ProjectType::JKDF2),
            recoveryProjectTypeLabel(ProjectType::MOTS),
            recoveryProjectTypeLabel(ProjectType::IJIM) };
        bool accepted = false;
        const QString selected = QInputDialog::getItem(
            recoveryDialogParent,
            QStringLiteral("Choose recovery project type"),
            QStringLiteral(
                "The recovery file lost its project discriminator.\n"
                "Select the project type before SED decodes history or loads resources."),
            projectChoices,
            0,
            false,
            &accepted);
        if (!accepted)
        {
            progress.close();
            return false;
        }
        const int selectedIndex = projectChoices.indexOf(selected);
        if (selectedIndex < 0 || selectedIndex > 2)
        {
            progress.close();
            return false;
        }
        record->metadata.projectType = static_cast<ProjectType>(selectedIndex);
        record->metadata.projectTypeTrust = ui::recovery::ProjectTypeTrust::user;
        error.clear();
        recovered = ui::recovery::loadRecoveredDocument(
            std::move(*record),
            &error,
            &ui::recovery::validateWorkspaceHistoryAttachmentForLevels);
    }
    if (!recovered)
    {
        appendRecoveryAudit(recoveryDirectory(), *record,
            QStringLiteral("level parse failed: %1").arg(error));
        // A failed project choice/HIST decode is retryable. Keep the selected
        // recovery at its original discoverable path so the user can reopen it
        // and choose another type; validation failure must not rename or
        // retire the only checkpoint.
        progress.close();
        QMessageBox::critical(
            recoveryDialogParent, QStringLiteral("Recovery failed"), error);
        return false;
    }
    for (const QString& missing : recovered->missingSections)
    {
        if (!record->missingSections.contains(missing))
        {
            record->missingSections.push_back(missing);
            ++record->lostSectionCount;
            record->partialRecovery = true;
            // A post-selection semantic failure was not visible to the raw
            // container reader. Mark it for clean republish/forensic retention
            // just like the same invalid HIST discovered with known metadata.
            record->corrupted = true;
        }
    }
    for (const QString& warning : recovered->warnings)
    {
        if (!record->warnings.contains(warning))
        {
            record->warnings.push_back(warning);
        }
    }
    if (record->missingSections.contains(QStringLiteral("hist")))
    {
        record->historyPayload.clear();
    }

    const auto updateProgress = [&progress](const QString& text, const int value)
    {
        if (progress.maximum() == 0)
        {
            progress.setRange(0, 100);
        }
        progress.setLabelText(text);
        progress.setValue(value);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    updateProgress(QStringLiteral("Preparing the recovered document..."), 56);

    std::filesystem::path retainedCorruptedPath{};
    bool corruptedForensicCopyRetained = false;
    if (record->corrupted)
    {
        QString retainWarning{};
        // Keep the normal recovery slot discoverable until a clean replacement
        // is atomically published; retain a separate forensic copy as well.
        std::optional<std::filesystem::path> retained{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        if (recoveryRetainCorruptedCopyForTest_
            && !recoveryRetainCorruptedCopyForTest_())
        {
            retainWarning = QStringLiteral(
                "Could not retain the damaged recovery file: injected copy failure.");
        }
        else
#endif
        {
            retained = retainCorruptedRecoveryFile(
                recoveryPath, true, &retainWarning);
        }
        if (retained)
        {
            retainedCorruptedPath = *retained;
            corruptedForensicCopyRetained = true;
        }
        if (!retainWarning.isEmpty())
        {
            recovered->warnings.push_back(QStringLiteral(
                "%1 The damaged original will remain in place; the repaired workspace will use a new recovery slot.")
                    .arg(retainWarning));
        }
        else
        {
            recovered->warnings.push_back(record->partialRecovery
                ? QStringLiteral(
                    "This was a partial recovery. The original damaged snapshot was retained as %1.")
                    .arg(QDir::toNativeSeparators(QString::fromStdWString(retainedCorruptedPath.wstring())))
                : QStringLiteral(
                    "The repaired snapshot was retained for diagnostics as %1.")
                    .arg(QDir::toNativeSeparators(QString::fromStdWString(retainedCorruptedPath.wstring()))));
        }
    }

    const QStringList missingRequiredSections =
        ui::recovery::lostRequiredLevelSections(record->missingSections);
    // Optional EDIT/HIST loss leaves the complete Level safe to associate.
    // Required or unidentified native loss does not: an unidentified unit may
    // conceal a required Level domain.
    const bool detachedPartialRecovery =
        !missingRequiredSections.isEmpty()
        || record->unidentifiedMissingSectionCount != 0;
    const std::filesystem::path recordedSourcePath =
        record->metadata.sourcePath.trimmed().isEmpty()
        ? std::filesystem::path{}
        : std::filesystem::path(record->metadata.sourcePath.toStdWString());
    // Keep a partial Level recovery detached so later Ctrl+S cannot overwrite
    // the damaged/source file. The recorded source remains useful for resource
    // discovery even when the recovered workspace becomes Untitled.
    const std::filesystem::path resourceDiscoveryPath = recordedSourcePath;
    const std::filesystem::path recoveredCurrentPath = detachedPartialRecovery
        ? std::filesystem::path{}
        : recordedSourcePath;
    StagedProjectResourceState stagedResources = stageProjectResources(
        recovered->current.level.kind, resourceDiscoveryPath);
    if (detachedPartialRecovery)
    {
        ui::recovery::PartialLevelSalvageContext salvageContext{};
        salvageContext.sourceRecoveryPath = recoveryPath;
        salvageContext.forensicRecoveryPath = retainedCorruptedPath.empty()
            ? recoveryPath
            : retainedCorruptedPath;
        salvageContext.missingRequiredSections = missingRequiredSections;
        salvageContext.unidentifiedMissingSectionCount =
            record->unidentifiedMissingSectionCount;
        salvageContext.recoveryWarnings = recovered->warnings;
        salvageContext.nativeExportResources = stagedResources.locatorSnapshot;
        const QString pendingSalvageWarning = QStringLiteral(
            "A non-authoritative partial-Level text salvage is being generated in the background beside %1; recovery will not wait for it.")
            .arg(QDir::toNativeSeparators(QString::fromStdWString(
                salvageContext.forensicRecoveryPath.wstring())));
        recovered->warnings.push_back(pendingSalvageWarning);
        record->warnings.push_back(pendingSalvageWarning);

        ui::recovery::RecoveryRecord salvageAudit{};
        salvageAudit.metadata = record->metadata;
        salvageAudit.warnings = record->warnings;
        salvageAudit.missingSections = record->missingSections;
        salvageAudit.unreadableRanges = record->unreadableRanges;
        salvageAudit.partialRecovery = record->partialRecovery;
        salvageAudit.corrupted = record->corrupted;
        salvageAudit.repairedNativeChunkCount = record->repairedNativeChunkCount;
        salvageAudit.repairedMetadataShardCount = record->repairedMetadataShardCount;
        salvageAudit.lostSectionCount = record->lostSectionCount;
        salvageAudit.unidentifiedMissingSectionCount =
            record->unidentifiedMissingSectionCount;
        salvageAudit.recoveryPath = record->recoveryPath;
        const std::filesystem::path salvageAuditDirectory = recoveryDirectory();
        const QByteArray salvagePayload = record->levelPayload;
        const ProjectType salvageProjectType = record->metadata.projectType;
        const bool salvageHeadMissing = record->missingSections.contains(QStringLiteral("head"));
        salvageContext.projectTypeSource = record->metadata.projectTypeTrust
            == ui::recovery::ProjectTypeTrust::user
            ? ui::recovery::PartialLevelProjectTypeSource::userSelected
            : ui::recovery::PartialLevelProjectTypeSource::checkedMetadata;

        auto* salvageWatcher =
            new QFutureWatcher<ui::recovery::PartialLevelSalvageResult>(this);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        recoverySalvageWatchersForTest_.push_back(salvageWatcher);
#endif
        connect(salvageWatcher,
            &QFutureWatcher<ui::recovery::PartialLevelSalvageResult>::finished,
            this,
            [this, salvageWatcher, salvageAuditDirectory,
             salvageAudit = std::move(salvageAudit)]() mutable
            {
                const ui::recovery::PartialLevelSalvageResult salvage =
                    salvageWatcher->result();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                if (!salvage.wroteManifest())
                {
                    const QByteArray diagnostic = QStringLiteral(
                        "FAIL: background partial-Level salvage did not publish a manifest: %1")
                        .arg(salvage.error)
                        .toUtf8();
                    std::fprintf(stderr, "%s\n", diagnostic.constData());
                    std::fflush(stderr);
                }
#endif
                const QString warning = salvage.recoveryWarning();
                if (!warning.isEmpty())
                {
                    logMessage(MessageSeverity::warning, warning);
                    salvageAudit.warnings.push_back(warning);
                }
                appendRecoveryAudit(
                    salvageAuditDirectory,
                    salvageAudit,
                    salvage.wroteManifest()
                        ? QStringLiteral("partial-Level text salvage written")
                        : QStringLiteral("partial-Level text salvage failed"));
                salvageWatcher->deleteLater();
            });
        salvageWatcher->setFuture(QtConcurrent::run(
            [salvagePayload, salvageProjectType, salvageHeadMissing,
             salvageContext = std::move(salvageContext)]()
            {
                try
                {
                    assets::SedRecoveryLoadResult decoded =
                        assets::recoverFromSedBytes(std::as_bytes(std::span(
                            salvagePayload.constData(),
                            static_cast<std::size_t>(salvagePayload.size()))));
                    // Native recovery uses a conservative JKDF2 framing when
                    // HEAD is gone.  The user-selected project kind is the
                    // only trusted discriminator for this non-authoritative
                    // text salvage path.
                    if (salvageHeadMissing)
                    {
                        ProjectType resolvedProjectType = salvageProjectType;
                        if (decoded.projectTypeTrusted && decoded.projectType)
                        {
                            if (*decoded.projectType != salvageProjectType)
                            {
                                throw std::runtime_error(
                                    "trusted native project type disagrees with recovery selection");
                            }
                            resolvedProjectType = *decoded.projectType;
                        }
                        // HEAD was lost as one semantic domain. Apply only its
                        // safe project/header defaults before JKL/NDY export;
                        // surviving Level collections remain untouched.
                        ui::project::applyNewProjectTypeDefaults(
                            decoded.level, resolvedProjectType);
                    }
                    return ui::recovery::writePartialLevelSalvage(
                        decoded.level, salvageContext);
                }
                catch (const std::exception& exception)
                {
                    ui::recovery::PartialLevelSalvageResult failure{};
                    failure.error = QStringLiteral(
                        "Background partial-Level text salvage failed safely: %1")
                        .arg(QString::fromUtf8(exception.what()));
                    return failure;
                }
                catch (...)
                {
                    ui::recovery::PartialLevelSalvageResult failure{};
                    failure.error = QStringLiteral(
                        "Background partial-Level text salvage failed safely after an unknown error.");
                    return failure;
                }
            }));
    }

    StagedGlobalTemplateState stagedTemplateState =
        stageGlobalTemplates(recovered->current.level);

    std::optional<editor::Document> stagedDocument;
    try
    {
        stagedDocument.emplace();
        stagedDocument->restoreRecoveryHistory(
            std::move(recovered->current),
            recovered->history ? std::move(*recovered->history)
                               : editor::DocumentRecoveryHistory{});
    }
    catch (const std::exception& exception)
    {
        progress.close();
        QMessageBox::critical(recoveryDialogParent, QStringLiteral("Recovery failed"),
            QStringLiteral("Could not prepare the recovered document and history: %1")
                .arg(QString::fromUtf8(exception.what())));
        return false;
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    if (recoveryApplyBeforeInstallForTest_
        && !recoveryApplyBeforeInstallForTest_())
    {
        progress.close();
        return false;
    }
#endif

    if (!replacementAuthorizationStillCurrent())
    {
        const QString warning = QStringLiteral(
            "The current workspace changed while recovery was loading; recovery was not applied.");
        logMessage(MessageSeverity::warning, warning);
        statusBar()->showMessage(warning);
        progress.close();
        clearReplacementAuthorizationFingerprint();
        return false;
    }

    const std::filesystem::path previousRecoveryPath = currentRecoveryPath_;
    const std::filesystem::path previousPendingRetirementPath =
        recoveryPendingRetirementPath_;
    const std::optional<ui::recovery::RecoveryMetadata>
        previousPendingRetirementMetadata =
            recoveryPendingRetirementMetadata_;
    if (thingPlacementBrushStrokeActive_)
    {
        (void)finalizeThingPlacementBrushStroke();
    }
    // Do not disturb the current workspace while validation or the
    // project-kind prompt can still be cancelled.  At this point every
    // recovery prompt/decoder check has succeeded and the old document is
    // about to be torn down, so stopping ambient audio is transactional with
    // the replacement rather than an eager side effect of opening a file.
    AppSoundPlayer::instance().stopAll();
    // Tear down interactions while they still refer to the old document.  A
    // recovery replaces the document storage just like Open/New; clearing
    // first would let cleave/join/extrude/selection previews interpret stale
    // indices against the recovered level.
    resetSelectionContextState();
    // The old tabs passed Save/Discard/Cancel before validation. Remove them
    // only at this install boundary; EDIT restore below then recreates the
    // recovered COG workspace without merging stale modified buffers.
    closeResourceEditorsAfterReplacement();
    closeCogScriptEditorsAfterReplacement();
    resourceKind_ = stagedResources.kind;
    resourceRoots_ = std::move(stagedResources.roots);
    resourceGobs_ = std::move(stagedResources.gobs);
    resources_ = std::move(stagedResources.locator);
    publishResourceLocatorSnapshot(
        std::move(stagedResources.locatorSnapshot));
    document_.swap(*stagedDocument);
    // Numeric Sector indices are document-local. Do not seed the recovered
    // Level's first ambience query with an overlap preference from the
    // replaced workspace.
    viewportSoundSector_ = -1;
    clearReplacementAuthorizationFingerprint();
    nativeDocumentTimestamp_ = recovered->documentTimestamp;
    resetLightBakeSourceTracking();
    resetLayerStatesFromLevel(false);
    publishGlobalTemplates(std::move(stagedTemplateState), false);

    // Commit recovery identity only after the recovered Level owns the
    // document. switchRecoveryDocument derives the project-tagged slot from
    // document_.level().kind; doing this before document_.clear used the old
    // workspace kind and could publish IJIM work into a JKDF2 slot.
    currentPath_ = recoveredCurrentPath;
    nativeSourceUsesLegacyUvFormat_ = false;
    if (!currentPath_.empty()
        && text::iequalsAscii(currentPath_.extension().string(), ".sed"))
    {
        try
        {
            nativeSourceUsesLegacyUvFormat_ =
                assets::sedBinaryVersionUsesTexturePixels(
                    assets::readSedBinaryVersion(currentPath_));
        }
        catch (const std::exception&)
        {
            // Recovery remains usable if its recorded source has disappeared
            // or is damaged. The normal Save path will validate the
            // destination again before replacing it.
        }
    }
    lastEngineExportPath_.clear();
    if (ui::project::isEngineLevelExportPath(
            recordedSourcePath, document_.level().kind))
    {
        lastEngineExportPath_ = recordedSourcePath;
    }
    // Recovery is a new live editing session, even when it resumes the same
    // authored source. Reusing the captured session ID lets two processes that
    // open the same checkpoint publish into one canonical filename and makes
    // one process's Delete/Discard race the other's newer atomic replacement.
    // Keep the selected bytes publication-bound below, but fork every restored
    // workspace to a collision-safe live slot of its own.
    recoverySessionId_ = ui::recovery::newSessionId();
    recoveryPendingRetirementPath_.clear();
    recoveryPendingRetirementMetadata_.reset();
    recoveryPendingRetirementDocumentEpoch_ = 0;
    recoveryPendingRetirementPublicationGeneration_ = 0;
    recoveryPendingRetirementCurrentPath_.clear();
    switchRecoveryDocument(currentPath_, false, false);
    // Preserve the source revision from which the crashed session actually
    // branched. Sampling the file now would hide an external-edit conflict
    // that occurred before the checkpoint was written.
    recoverySourceModifiedAtMs_ = record->metadata.sourceModifiedAtMs;
    const bool nonCanonicalRecoveryPath =
        !recoveryPathsEqual(recoveryPath, currentRecoveryPath_);
    if (nonCanonicalRecoveryPath
        && !ui::recovery::isAutosaveHistoryPath(recoveryPath)
        && (!record->corrupted || corruptedForensicCopyRetained))
    {
        // A legacy-directory or copied/manual snapshot has a non-canonical
        // physical filename. Keep it discoverable until publication wins;
        // handleRecoveryWriteFinished then retires it. Corrupted inputs retain
        // their byte-identical, clearly marked forensic copy. Detached partial
        // input follows the same publication-bound retirement: its unmarked
        // copy stays discoverable until the separate clean Untitled checkpoint
        // is durable, then retires so it cannot be offered again on every
        // startup while the excluded `.corrupted-*` evidence remains intact.
        recoveryPendingRetirementPath_ = recoveryPath;
        recoveryPendingRetirementMetadata_ = record->metadata;
        recoveryPendingRetirementDocumentEpoch_ = recoveryDocumentEpoch_;
        recoveryPendingRetirementPublicationGeneration_ = 0;
        recoveryPendingRetirementCurrentPath_ = currentRecoveryPath_;
    }

    // Only now that the replacement document and its correctly tagged slot
    // are installed may the previous workspace's checkpoint be retired.
    // switchRecoveryDocument already advanced the publication generation, so
    // an in-flight old writer cannot republish after these removals.
    const auto retirePreviousRecovery = [this, &recoveryPath](
        const std::filesystem::path& path,
        const ui::recovery::RecoveryMetadata* expectedMetadata = nullptr)
    {
        if (path.empty() || ui::recovery::isAutosaveHistoryPath(path)
            || recoveryPathsEqual(path, recoveryPath)
            || recoveryPathsEqual(path, currentRecoveryPath_)
            || recoveryPathsEqual(path, recoveryPendingRetirementPath_))
        {
            return;
        }
        QString removalError{};
        if (!ui::recovery::invalidateAndRemoveRecovery(
                nullptr, path, &removalError, expectedMetadata)
            && !removalError.isEmpty())
        {
            logMessage(MessageSeverity::warning, removalError);
        }
    };
    // Complete the new resource/template/Level generation without pumping the
    // Qt event loop. Timers and plugins must never observe a recovered Level
    // paired with the previous locator or template database.
    progress.setLabelText(
        QStringLiteral("Loading recovered resources and templates..."));
    progress.setValue(68);
    // Defer GPU upload until the recovered editor state has been applied;
    // uploading stale pre-recovery meshes only adds a redundant frame.
    glView_->setResourceLocator(&resources_, false);
    const assets::Templates* publishedTemplates =
        templates_.size() > 0 ? &templates_ : nullptr;
    // A recovery payload is the authoritative workspace, including camera and
    // tool state. Defer the expensive render-cache build until that payload has
    // been validated and applied. Recovery files without an editor payload
    // retain the ordinary one-pass clean-view behavior.
    const bool deferRecoveryRenderRebuild =
        !recovered->editorStatePayload.isEmpty();
    glView_->setLevel(
        &document_.level(),
        deferRecoveryRenderRebuild
            ? GlView::SetLevelMode::keepView
            : GlView::SetLevelMode::resetView,
        publishedTemplates,
        !deferRecoveryRenderRebuild);
    refreshPublishedGlobalTemplates();

    StagedProjectResourceState acceptedResourceSettings{};
    acceptedResourceSettings.kind = resourceKind_;
    acceptedResourceSettings.roots = resourceRoots_;
    acceptedResourceSettings.gobs = resourceGobs_;
    acceptedResourceSettings.detectedInstallDir =
        stagedResources.detectedInstallDir;
    persistStagedProjectResources(acceptedResourceSettings);
    progress.setLabelText(
        QStringLiteral("Restoring cameras, tools, and editor state..."));
    progress.setValue(80);
    bool restoredEditorState = false;
    if (!recovered->editorStatePayload.isEmpty())
    {
        QString warning{};
        restoredEditorState = restoreRecoveryEditorState(
            recovered->editorStatePayload,
            &warning,
            deferRecoveryRenderRebuild);
        startupRecoveryEditorStateRestored_ = restoredEditorState;
        if (!warning.isEmpty())
        {
            recovered->warnings.push_back(warning);
        }
        if (!restoredEditorState)
        {
            if (!record->missingSections.contains(QStringLiteral("edit")))
            {
                record->missingSections.push_back(QStringLiteral("edit"));
                ++record->lostSectionCount;
            }
            record->partialRecovery = true;
            record->corrupted = true;
            record->warnings.push_back(QStringLiteral(
                "Recovered EDIT workspace state was malformed; safe editor defaults were used and a clean checkpoint is required."));
        }
    }
    if (deferRecoveryRenderRebuild)
    {
        if (restoredEditorState)
        {
            progress.setLabelText(
                QStringLiteral("Publishing recovered viewport..."));
            progress.setValue(88);
            // The recovered editor state is now complete. Build and upload all
            // derived meshes exactly once, preserving its camera. Suppress
            // only UV View Face recomputation so resource/render warnings
            // emitted by the rebuild still reach the user.
            const QScopedValueRollback suppressUvViewTransformSync(
                uvEditorSuppressViewTransformSync_, true);
            glView_->notifyAllMeshesChanged();
        }
        else
        {
            // If the optional editor chunk is malformed or unsupported, do not
            // leave deferred caches behind. Fall back to the normal clean-view
            // path with one complete build.
            glView_->setLevel(
                &document_.level(),
                GlView::SetLevelMode::resetView,
                publishedTemplates,
                true);
        }
    }
    if (restoredEditorState)
    {
        // MainWindow construction arms a deferred initial level refit for the
        // first visible viewport. Without cancelling it, the correctly
        // restored camera is overwritten about 90 ms after startup.
        pendingEditorViewRefitAfterResize_ = false;
        editorViewRefitLastViewportSize_ = {};
        if (editorViewRefitTimer_)
        {
            editorViewRefitTimer_->stop();
        }
    }
    if (!restoredEditorState)
    {
        // A malformed/unsupported editor payload must fall back to a clean
        // workspace.  Grid alignment and center are temporary editor
        // overrides; retaining them after the authoritative recovery state
        // was rejected can make the new Level appear detached from its grid.
        glView_->resetGridOverrides();
        glView_->setRenderPipeline(GlView::RenderPipeline::Editor);
        glView_->resetPreviewViewportState();
        glView_->setLightingMode(GlView::LightingMode::Gouraud);
        glView_->setShowFog(glView_->editorDefaultShowFog());
        glView_->setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
        glView_->resetFlySpeedMultiplier();
        glView_->resetEditorTopDownView();
        glView_->setOrbitViewImmediate(0.0f, 90.0f, 0.0f);
        setPlacementMode(PlacementMode::none);
        setSurfaceSelectionMode(SurfaceSelectionMode::face);
        setEditMode(EditMode::none);
        scheduleEditorViewRefitAfterLayout();
    }
    if (restoredEditorState
        && !recovered->workspaceHistory.attachments.empty())
    {
        QStringList attachmentWarnings{};
        restoreWorkspaceHistoryAttachments(
            recovered->workspaceHistory, &attachmentWarnings);
        recovered->warnings.append(attachmentWarnings);
    }

    // Report staged discovery only after recovered/default EDIT state is
    // coherent; message widgets may synchronously notify observers.
    reportStagedProjectResources(stagedResources);
    reportStagedGlobalTemplates(stagedTemplateState);

    updateProgress(QStringLiteral("Rebuilding the recovered workspace..."), 91);
    rebuildOutliner();
    rebuildProperties();
    syncCogScriptWatches();
    rebuildChecks();
    savedUndoSignature_ = document_.undoSignature();
    recoveryRequiresSave_ = true;
    if (record->corrupted || nonCanonicalRecoveryPath)
    {
        recoveryLastPublishedValid_ = false;
        recoveryLastPublishedIdentity_.clear();
        recoveryLastPublishedDocumentEpoch_ = 0;
        recoveryLastPublishedPath_.clear();
        recoveryLastPublishedUndoSignature_ = 0;
        recoveryLastPublishedRedoSignature_ = 0;
        recoveryLastPublishedEventSerial_ = 0;
        recoveryLastPublishedSemanticMutationSerial_ = 0;
        recoveryLastNotedSemanticUndoSignature_ = 0;
        recoveryLastNotedSemanticRedoSignature_ = 0;
        recoveryLastNotedSemanticHistoryValid_ = false;
        recoveryLastPublishedEditorStateHash_.clear();
        recoveryLastPublishedLevelStateHash_.clear();
    }
    else
    {
        recoveryLastPublishedIdentity_ = recoveryIdentity_;
        recoveryLastPublishedDocumentEpoch_ = recoveryDocumentEpoch_;
        recoveryLastPublishedPath_ = currentRecoveryPath_;
        recoveryLastPublishedUndoSignature_ = document_.undoSignature();
        recoveryLastPublishedRedoSignature_ = document_.redoSignature();
        recoveryLastPublishedEventSerial_ = recoveryEventSerial_;
        recoveryLastPublishedSemanticMutationSerial_ = recoverySemanticMutationSerial_;
        recoveryLastPublishedEditorStateHash_ = QCryptographicHash::hash(
            captureRecoveryEditorState(), QCryptographicHash::Sha256);
        Level recoveredLevelForHash = document_.level();
        recoveredLevelForHash.lvisstring = buildLayerVisibilityString();
        recoveryLastPublishedLevelStateHash_ =
            ui::recovery::authoredLevelStateHash(recoveredLevelForHash);
        recoveryLastPublishedValid_ = true;
    }
    refreshWindowTitle();
    updateEditActions();
    resetLegacyUvCompatibilityStatus();
    scheduleLegacyUvPromotionRetry();

    // Purge any sound started against the interim camera/resource state. The
    // next regular viewport-audio tick evaluates the final recovered camera
    // against the recovered level from scratch.
    AppSoundPlayer::instance().stopAll();
    QTimer::singleShot(0, this, [this]() { updateSectorSoundPlayback(); });

    const QString display = currentPath_.empty()
        ? QStringLiteral("Untitled")
        : QDir::toNativeSeparators(QString::fromStdWString(currentPath_.wstring()));
    statusBar()->showMessage(QStringLiteral("Recovered unsaved changes for %1").arg(display));
    logMessage(MessageSeverity::warning,
        QStringLiteral("Recovered autosave snapshot for %1; save explicitly to replace the level file.").arg(display));
    for (const QString& warning : recovered->warnings)
    {
        logMessage(MessageSeverity::warning, warning);
    }
    appendRecoveryAudit(recoveryDirectory(), *record,
        record->partialRecovery ? QStringLiteral("partially loaded") : QStringLiteral("loaded"));
    updateProgress(QStringLiteral("Recovery complete"), 100);
    progress.close();
    if (record->corrupted || nonCanonicalRecoveryPath)
    {
        if (detachedPartialRecovery)
        {
            QMessageBox::warning(
                recoveryDialogParent,
                QStringLiteral("Partial recovery"),
                QStringLiteral(
                    "The recovery snapshot was too damaged to restore every level section. "
                    "SED loaded the healthy sections and retained the damaged snapshot. "
                    "See recovery.log and the Messages panel for the full recovery report."));
        }
        // If another crash happens before this asynchronous write completes,
        // the damaged original remains discoverable. Publication completion
        // retires that unmarked copy only after the clean checkpoint is durable
        // and a byte-identical `.corrupted-*` forensic copy has been retained.
        scheduleRecoverySnapshot();
    }
    retirePreviousRecovery(
        previousPendingRetirementPath,
        previousPendingRetirementMetadata
            ? &*previousPendingRetirementMetadata
            : nullptr);
    if (previousRecoveryPath != previousPendingRetirementPath)
    {
        retirePreviousRecovery(previousRecoveryPath);
    }
    return true;
}

MainWindow::RecoveryOfferResult MainWindow::offerRecoveryForPath(
    const std::filesystem::path& sourcePath,
    const bool startupOffer,
    std::filesystem::path* selectedRecoveryPath,
    QString* selectedRecoveryIdentity,
    QString* selectedRecoverySessionId)
{
    if (selectedRecoveryPath) selectedRecoveryPath->clear();
    if (selectedRecoveryIdentity) selectedRecoveryIdentity->clear();
    if (selectedRecoverySessionId) selectedRecoverySessionId->clear();
    if (sourcePath.empty())
    {
        return RecoveryOfferResult::none;
    }

    const QString identity = ui::recovery::documentIdentity(sourcePath, QString{});
    if (recoveryOfferSuppressedIdentitiesThisRun_.contains(identity))
    {
        // Startup Discard removes only the selected session so another running
        // editor's checkpoint is never silently destroyed. Suppress the older
        // same-document live slots for this process, however, or opening the
        // saved file immediately after startup would present the same recovery
        // question again. History and the next process start remain unaffected.
        return RecoveryOfferResult::none;
    }
    std::vector<ui::recovery::RecoveryDescriptor> descriptors{};
    for (const std::filesystem::path& directory : recoverySearchDirectories())
    {
        const std::vector<ui::recovery::RecoveryDescriptor> found =
            ui::recovery::scanRecoveries(directory);
        for (const ui::recovery::RecoveryDescriptor& candidate : found)
        {
            if (candidate.metadata.identity != identity)
            {
                continue;
            }
            if (!candidate.metadata.requiresUserRecovery) continue;
            const bool duplicate = std::any_of(
                descriptors.begin(), descriptors.end(),
                [&candidate](const ui::recovery::RecoveryDescriptor& value)
                {
                    return recoveryPathsEqual(
                        value.recoveryPath, candidate.recoveryPath);
                });
            if (!duplicate)
            {
                descriptors.push_back(candidate);
            }
        }
    }
    if (descriptors.empty())
    {
        return RecoveryOfferResult::none;
    }
    std::sort(descriptors.begin(), descriptors.end(),
        [](const auto& lhs, const auto& rhs)
        {
            return lhs.metadata.timestamp > rhs.metadata.timestamp;
        });

    // A live clean baseline closes the hard-kill window but contains no work
    // that warrants interrupting the user. Historical `.sedautosave` entries
    // are available only through File > Open Recent > Autosave History.
    RecoveryOfferDialog dialog(
        std::move(descriptors),
        startupOffer ? RecoveryDialogMode::startup
                     : RecoveryDialogMode::openSaved,
        isVisible() ? this : nullptr);
    (void)dialog.exec();
    const ui::recovery::RecoveryDescriptor selected =
        dialog.selectedDescriptor();
    if (selectedRecoveryPath) *selectedRecoveryPath = selected.recoveryPath;
    if (selectedRecoveryIdentity)
    {
        *selectedRecoveryIdentity = selected.metadata.identity;
    }
    if (selectedRecoverySessionId)
    {
        *selectedRecoverySessionId = selected.metadata.sessionId;
    }
    if (dialog.action() == RecoveryPromptAction::recover)
    {
        return RecoveryOfferResult::recovered;
    }
    if (dialog.action() == RecoveryPromptAction::discard)
    {
        return RecoveryOfferResult::openSaved;
    }
    return RecoveryOfferResult::cancelled;
}

bool MainWindow::offerStartupRecovery()
{
    std::vector<ui::recovery::RecoveryDescriptor> recoveries{};
    for (const std::filesystem::path& directory : recoverySearchDirectories())
    {
        std::vector<ui::recovery::RecoveryDescriptor> found =
            ui::recovery::scanRecoveries(directory);
        for (ui::recovery::RecoveryDescriptor& candidate : found)
        {
            if (!candidate.metadata.requiresUserRecovery)
            {
                continue;
            }
            if (recoveryOfferSuppressedIdentitiesThisRun_.contains(
                    candidate.metadata.identity))
            {
                continue;
            }
            const auto existing = std::find_if(
                recoveries.begin(), recoveries.end(),
                [&candidate](const ui::recovery::RecoveryDescriptor& value)
                {
                    // Startup offers only the newest live checkpoint for each
                    // document. Older periodic points remain available from
                    // the explicit Autosave History browser.
                    return value.metadata.identity == candidate.metadata.identity;
                });
            if (existing == recoveries.end())
            {
                recoveries.push_back(std::move(candidate));
            }
            else if (candidate.metadata.timestamp > existing->metadata.timestamp)
            {
                *existing = std::move(candidate);
            }
        }
    }
    std::sort(recoveries.begin(), recoveries.end(),
        [](const auto& lhs, const auto& rhs)
        {
            return lhs.metadata.timestamp > rhs.metadata.timestamp;
        });
    if (!recoveries.empty())
    {
        // Keep the pre-show startup prompt parentless so Windows can expose
        // it as the visible application taskbar entry. Once MainWindow is
        // already visible, offerRecoveryForPath uses it as the owner.
        RecoveryOfferDialog dialog(recoveries, RecoveryDialogMode::startup, nullptr);
        (void)dialog.exec();
        const std::filesystem::path selectedPath =
            dialog.selectedDescriptor().recoveryPath;
        if (dialog.action() == RecoveryPromptAction::recover)
        {
            return applyRecoveryFile(selectedPath);
        }
        if (dialog.action() == RecoveryPromptAction::discard)
        {
            const QString discardedIdentity =
                dialog.selectedDescriptor().metadata.identity;
            if (!discardedIdentity.trimmed().isEmpty()
                && !recoveryOfferSuppressedIdentitiesThisRun_.contains(
                    discardedIdentity))
            {
                recoveryOfferSuppressedIdentitiesThisRun_.push_back(
                    discardedIdentity);
            }
            discardRecoveryFamily(
                discardedIdentity,
                dialog.selectedDescriptor().metadata.sessionId);
            return false;
        }
        return false;
    }
    return false;
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void MainWindow::setRecoveryDirectoryForTest(const std::filesystem::path& directory)
{
    discardCurrentRecovery();
    recoveryDirectoryOverride_ = directory;
    switchRecoveryDocument(currentPath_, currentPath_.empty(), false, true);
}

void MainWindow::beginRecoveryDocumentForTest(
    const std::filesystem::path& sourcePath,
    const bool requiresSave)
{
    recoveryAutomaticBaselineEnabled_ = true;
    currentPath_ = sourcePath;
    switchRecoveryDocument(sourcePath, sourcePath.empty(), true);
    markCurrentDocumentAsSaved();
    markRecoveryWorkspaceAsSaved();
    recoveryRequiresSave_ = requiresSave;
    refreshWindowTitle();
}

void MainWindow::triggerRecoverySnapshotForTest()
{
    recoveryAutomaticBaselineEnabled_ = true;
    scheduleRecoverySnapshot();
}

void MainWindow::triggerForcedRecoverySnapshotForTest()
{
    recoveryAutomaticBaselineEnabled_ = true;
    scheduleRecoverySnapshot(true);
}

void MainWindow::triggerRecoveryRetryForTest()
{
    recoveryAutomaticBaselineEnabled_ = true;
    if (recoveryRetryTimer_) recoveryRetryTimer_->stop();
    const bool forceRetry = recoveryRetryForceCapture_;
    recoveryRetryForceCapture_ = false;
    scheduleRecoverySnapshot(forceRetry);
}

void MainWindow::waitForRecoverySnapshotForTest()
{
    recoveryAutomaticBaselineEnabled_ = true;
    if (recoveryDebounceTimer_ && recoveryDebounceTimer_->isActive())
    {
        recoveryDebounceTimer_->stop();
    }
    scheduleRecoverySnapshot();
    for (;;)
    {
        if (recoveryWriteWatcher_ && recoveryWriteWatcher_->isRunning())
        {
            recoveryWriteWatcher_->waitForFinished();
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        if (!recoverySnapshotQueued_
            && (!recoveryWriteWatcher_ || !recoveryWriteWatcher_->isRunning()))
        {
            break;
        }
    }
}

void MainWindow::waitForRecoverySalvageForTest()
{
    for (const QPointer<QObject>& object : recoverySalvageWatchersForTest_)
    {
        auto* watcher = static_cast<
            QFutureWatcher<ui::recovery::PartialLevelSalvageResult>*>(
                object.data());
        if (watcher && watcher->isRunning())
        {
            watcher->waitForFinished();
        }
    }

    QCoreApplication::processEvents(QEventLoop::AllEvents);
    std::erase_if(recoverySalvageWatchersForTest_,
        [](const QPointer<QObject>& object)
        {
            return object.isNull();
        });
}

bool MainWindow::recoveryRetryTimerActiveForTest() const noexcept
{
    return recoveryRetryTimer_ && recoveryRetryTimer_->isActive();
}

std::filesystem::path MainWindow::currentRecoveryPathForTest() const
{
    return currentRecoveryPath_;
}

bool MainWindow::applyRecoveryFileForTest(const std::filesystem::path& recoveryPath)
{
    return applyRecoveryFile(recoveryPath);
}

void MainWindow::discardCurrentRecoveryForTest()
{
    discardCurrentRecovery();
}
#endif

} // namespace sed
