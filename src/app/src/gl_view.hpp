#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QMetaObject>
#include <QFunctionPointer>
#include <QOpenGLBuffer>
#include <QOpenGLWidget>
#include <QList>
#include <QKeySequence>
#include <QMatrix4x4>
#include <QPoint>
#include <QPointF>
#include <QPixmap>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QColor>
#include <QImage>
#include <QVector3D>
#include <QVector4D>
#include <QVector>
#include <QCursor>

#include "sed/assets/cmp.hpp"
#include "sed/assets/spr.hpp"
#include "sed/editor/selection.hpp"
#include "sed/level.hpp"
#include "sed/level/arch_lighting.hpp"
#include "sed/level/geometry.hpp"
#include "sed/level/transform.hpp"

#include "app_settings.hpp"
#include "render/grid/viewport_grid_utils.hpp"
#include "render/cleave/cleave_hud_presentation.hpp"
#include "render/cleave/cleave_measurement_utils.hpp"
#include "render/cleave/cleave_wheel_session.hpp"
#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "render/selection/vertex_marker_utils.hpp"
#include "render/mesh/render_mesh_utils.hpp"
#include "render/overlays/frame_ghost_topology.hpp"
#include "render/overlays/frame_pose_cache_utils.hpp"
#include "render/snap/surface_transform_snap_utils.hpp"
#include "thing/thing_snap_utils.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"
#include "ui/viewport/cleave_shape_utils.hpp"
#include "viewport_camera.hpp"

class QMouseEvent;
class QKeyEvent;
class QLineEdit;
class QWheelEvent;
class QTimer;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;
class QOpenGLVertexArrayObject;
class QOpenGLTexture;

namespace sed
{

using ClassicSolidLightRamp = std::array<QVector3D, 64>;

struct Level;
namespace ui::detail
{
class PreviewResourceDiskWatcher;
}
namespace assets
{
class ResourceLocator;
class Templates;
struct Model3do;
} // namespace assets

namespace render
{

/** Cached source geometry used by Sector/Surface Move snapping. */
struct GeometrySnapSurface
{
    SurfaceRef ref{};
    TVector point{};
    TVector center{};
    QVector3D normal{};
    TVector documentNormal{};
    QVector3D boundsMin{};
    QVector3D boundsMax{};
    float boundingRadius{};
    std::vector<TVector> vertices{};
};

/**
 * Stationary and moving snap geometry captured at transform-drag start.
 *
 * The Level remains unchanged while a transform preview is active, so the
 * expensive all-surface collection only needs to run once per drag.  The
 * solver then iterates these compact vectors instead of reconstructing every
 * target surface on every mouse sample.
 */
struct GeometryMoveSnapCache
{
    TVector documentOrigin{};
    std::vector<GeometrySnapSurface> movingSurfaces{};
    std::unordered_set<std::int32_t> movingSectors{};
    std::vector<GeometrySnapSurface> stationarySurfaces{};
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> stationaryBuckets{};
    std::vector<std::size_t> largeStationarySurfaces{};
    std::optional<SurfaceSnapVertex> movingVertex{};
    std::optional<SurfaceSnapEdge> movingEdge{};
    std::vector<SurfaceSnapVertex> stationaryVertices{};
    std::vector<SurfaceSnapEdge> stationaryEdges{};
    float broadPhaseCellSize{16.0f};

    void clear() noexcept
    {
        documentOrigin = TVector{};
        movingSurfaces.clear();
        movingSectors.clear();
        stationarySurfaces.clear();
        stationaryBuckets.clear();
        largeStationarySurfaces.clear();
        movingVertex.reset();
        movingEdge.reset();
        stationaryVertices.clear();
        stationaryEdges.clear();
        broadPhaseCellSize = 16.0f;
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return !movingSurfaces.empty() && !stationarySurfaces.empty();
    }

    [[nodiscard]] bool subElementValid() const noexcept
    {
        return (movingVertex && !stationaryVertices.empty())
            || (movingEdge && !stationaryEdges.empty());
    }
};

/**
 * Immutable per-gesture data used by Thing Move snapping.
 *
 * The Level is not edited while a transform preview is active. Keeping only
 * indices, poses, resolved local boxes, connectors, and derived static bounds
 * here avoids copying complete Thing records for every pointer sample while
 * retaining the authored Level as the source of truth for the eventual
 * commit.
 */
struct ThingMoveSnapEntry
{
    std::int32_t index{ -1 };
    TVector relativeOrigin{};
    TVector pose{};
    std::optional<ThingBox> localBox{};
    std::optional<ThingBox> modelBox{};
    std::vector<thing::SnapConnector> connectors{};
    std::vector<TVector> surfaceStickPoints{};
    thing::LocalBounds3 staticBounds{};
    double broadRadius{};
    bool visible{};
    bool finite{};
    bool localBoxReady{};
    bool modelBoxReady{};
    bool connectorsReady{};
    bool surfaceStickPointsReady{};
};

struct ThingMoveSnapCache
{
    TVector documentOrigin{};
    std::vector<std::int32_t> movingIndices{};
    std::vector<std::size_t> movingEntries{};
    std::vector<ThingMoveSnapEntry> entries{};
    std::unordered_map<std::int32_t, std::size_t> entryByIndex{};
    std::unordered_map<std::string, std::optional<ThingBox>> localBoxesByKey{};
    std::unordered_map<std::string, std::optional<ThingBox>> modelBoxesByKey{};
    std::unordered_map<std::string, std::vector<thing::SnapConnector>> connectorsByKey{};
    double movingBroadRadius{};

    void clear() noexcept
    {
        documentOrigin = TVector{};
        movingIndices.clear();
        movingEntries.clear();
        entries.clear();
        entryByIndex.clear();
        localBoxesByKey.clear();
        modelBoxesByKey.clear();
        connectorsByKey.clear();
        movingBroadRadius = 0.0;
    }

    [[nodiscard]] const ThingMoveSnapEntry* find(
        const std::int32_t index) const noexcept
    {
        const auto it = entryByIndex.find(index);
        return it == entryByIndex.end() ? nullptr : &entries[it->second];
    }

    [[nodiscard]] ThingMoveSnapEntry* find(
        const std::int32_t index) noexcept
    {
        const auto it = entryByIndex.find(index);
        return it == entryByIndex.end() ? nullptr : &entries[it->second];
    }
};

/** Cached virtual mover/supports for one selected path Frame. */
struct FrameMovePoseSnapshot
{
    std::int32_t ownerThingIndex{-1};
    std::int32_t frameValueIndex{-1};
    Thing virtualPose{};
    // Lazily captured, Frame-stripped authored poses for stationary snap
    // anchors reached during this gesture.
    std::unordered_map<std::int32_t, Thing> stationaryAnchorPoses{};
    // Relative to the virtual pose origin, matching Thing surface-stick probes.
    std::vector<TVector> supportPoints{};
    bool valid{};
};

} // namespace render

class GlView final : public QOpenGLWidget
{
    Q_OBJECT

public:
    static constexpr float kArchitecturalLightingMarkerMinSizePx = 3.0f;
    static constexpr float kArchitecturalLightingMarkerMaxSizePx = 24.0f;
    static constexpr float kArchitecturalLightingMarkerDefaultSizePx = 14.0f;
    static constexpr float kLevelGeometryVertexMarkerMinSizePx =
        render::kLevelGeometryVertexMarkerMinSizePx;
    static constexpr float kLevelGeometryVertexMarkerMaxSizePx =
        render::kLevelGeometryVertexMarkerMaxSizePx;
    static constexpr float kLevelGeometryVertexMarkerDefaultSizePx =
        render::kLevelGeometryVertexMarkerDefaultSizePx;

    enum class SetLevelMode : std::uint8_t
    {
        resetView,
        keepView
    };

    enum class RenderPipeline : std::uint8_t
    {
        Editor,
        GamePreview
    };

    enum class TransformTool : std::uint8_t
    {
        Select,
        Move,
        Rotate,
        Scale,
    };

    enum class TransformOrientation : std::uint8_t
    {
        Local,
        World,
    };

    enum class PickPreference : std::uint8_t
    {
        Auto,
        PreferSurface,
        PreferThing,
        PreferLight
    };

    enum class PickMode : std::uint8_t
    {
        Auto,
        Sector,
        Surface,
        Thing,
        ArchitecturalLighting,
        Light
    };

    enum class SelectionBoxMode : std::uint8_t
    {
        Replace,
        Add,
        Remove
    };

    /**
     * Target under the cursor while the MotS ARCHLIGHTING tool is active.
     * Owner selection and authored vertex selection are separate interaction
     * contracts so a click on a model body cannot accidentally edit a vertex.
     */
    enum class ArchitecturalLightingTargetMode : std::uint8_t
    {
        Owner,
        Vertices
    };

    using CleaveMeasureUnit = render::CleaveMeasureUnit;

    enum class StagedCleaveTransformMode : std::uint8_t
    {
        Move,
        Rotate,
        Scale,
    };

    enum class CleaveWorkspaceFamily : std::uint8_t
    {
        Sector,
        Surface,
    };

    enum class ArchitecturalLightingRecoveryRestoreResult : std::uint8_t
    {
        NoPending,
        Deferred,
        Applied,
        Rejected,
    };

    enum class ThingViewMode : std::uint8_t
    {
        Dots,
        Boxes,
        Wireframes,
        Shaded,
        Vertices,
    };

    enum class SurfaceSubMode : std::uint8_t
    {
        Face,
        Edge,
        Vertex
    };

    /**
     * Read-only world-space geometry exposed by the active MotS
     * ARCHLIGHTING renderer cache.  The cache owner supplies the selected
     * model's hierarchy/LOD transform; interactive picking must not resolve or
     * load a 3DO resource for each mouse sample.
     */
    struct ArchitecturalLightingRenderVertex
    {
        std::int32_t thingIndex{ -1 };
        std::int32_t lodIndex{};
        // Authored MotS ARCHLIGHTING mesh ordinal. This is the source mesh
        // index, not necessarily the expanded render-mesh position used by a
        // hierarchy clone.
        std::int32_t meshIndex{ -1 };
        std::int32_t vertexIndex{ -1 };
        QVector3D worldPosition{};
        MotsArchLightVertex lighting{};

        constexpr bool operator==(const ArchitecturalLightingRenderVertex&) const = default;
    };

    /** Stable authored identity used by the viewport's vertex selection set. */
    struct ArchitecturalLightingVertexIdentity
    {
        std::int32_t thingIndex{ -1 };
        std::int32_t lodIndex{};
        std::int32_t meshIndex{ -1 };
        std::int32_t vertexIndex{ -1 };

        constexpr bool operator==(const ArchitecturalLightingVertexIdentity&) const = default;
    };

    struct ArchitecturalLightingVertexIdentityHash
    {
        std::size_t operator()(
            const ArchitecturalLightingVertexIdentity& identity) const noexcept
        {
            std::size_t seed = std::hash<std::int32_t>{}(identity.thingIndex);
            seed ^= std::hash<std::int32_t>{}(identity.lodIndex)
                + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
            seed ^= std::hash<std::int32_t>{}(identity.meshIndex)
                + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
            seed ^= std::hash<std::int32_t>{}(identity.vertexIndex)
                + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
            return seed;
        }
    };

    struct ArchitecturalLightingPreviewVertex
    {
        ArchitecturalLightingRenderVertex identity{};
        MotsArchLightVertex lighting{};

        constexpr bool operator==(const ArchitecturalLightingPreviewVertex&) const = default;
    };

    struct SurfaceEdgeSelectionRef
    {
        SurfaceRef surface{};
        std::int32_t localEdge{-1};
        std::int32_t firstVertex{-1};
        std::int32_t secondVertex{-1};

        constexpr bool operator==(const SurfaceEdgeSelectionRef&) const noexcept = default;
    };

    enum class LightingMode : std::uint8_t
    {
        FullyLit = 0,
        Lit      = 1,
        Diffuse  = 2,
        Gouraud  = 3
    };

    enum class DynamicLightShading : std::uint8_t
    {
        Gouraud = 0,
        Phong   = 1
    };

    enum class GridPlane : std::uint8_t;

    struct ViewportState
    {
        app::ViewportCamera::OrbitState orbit{};
        app::ViewportCamera::ProjectionType projectionType{ app::ViewportCamera::ProjectionType::Orthographic };
        float fovXDeg{ 90.0f };
        float orthoScale{ 64.0f };
        bool clipEnabled{ true };
        bool showWireframe{ true };
        bool showVertices{};
        bool showSolid{ true };
        bool showTextures{ true };
        bool showFog{ true };
        bool showThings{ true };
        bool thingModelLodEnabled{};
        bool showHiddenThings{ true };
        bool showThingGizmos{};
        bool showArchThingGizmos{true};
        bool showThingCollisionShapes{ true };
        bool showThingVisualBounds{ true };
        ThingViewMode thingViewMode{ ThingViewMode::Shaded };
        LightingMode lightingMode{ LightingMode::FullyLit };
        bool initialized{};
    };

    struct RecoveryState
    {
        struct CleaveChainOrigin
        {
            std::int32_t lineIndex{-1};
            std::int32_t endpoint{-1};
        };
        struct CleaveCamera
        {
            app::ViewportCamera::OrbitState orbit{};
            app::ViewportCamera::ProjectionType projectionType{
                app::ViewportCamera::ProjectionType::Orthographic};
            float fovXDeg{90.0f};
            float orthoScale{64.0f};
            float nearPlane{};
            float farPlane{};
            bool clipEnabled{true};
            bool valid{};
        };
        struct CleaveLine
        {
            TVector first{};
            TVector second{};
            TVector planeNormal{};
            TVector constructionPlaneNormal{};
            TVector angleReferenceAxis{};
            CleaveCamera authoredCamera{};
            bool linkedToPrevious{};
            bool linkedToNext{};
        };
        struct CleaveContext
        {
            bool valid{};
            bool surfaceMode{};
            QMatrix4x4 projection{};
            QMatrix4x4 view{};
            int viewportWidth{};
            int viewportHeight{};
            SurfaceRef sourceSurface{-1, -1};
            std::vector<std::int32_t> targetSectors{};
            std::vector<SurfaceRef> targetSurfaces{};
        };
        struct CleaveDraftSnapshot
        {
            std::vector<CleaveLine> lines{};
            CleaveContext context{};
            std::optional<CleaveChainOrigin> chainOrigin{};
            std::vector<std::int32_t> applyOrder{};
            std::vector<std::int32_t> selectedCleaveLines{};
            std::int32_t selectedCleaveLine{-1};
        };
        struct CleaveDraftView
        {
            std::span<const CleaveLine> lines{};
            const CleaveContext* context{};
            std::optional<CleaveChainOrigin> chainOrigin{};
            std::span<const std::int32_t> applyOrder{};
            std::span<const std::int32_t> selectedCleaveLines{};
            std::int32_t selectedCleaveLine{-1};
        };
        struct CleaveHistory
        {
            std::vector<CleaveDraftSnapshot> snapshots{};
            std::size_t cursor{};
        };
        struct CleaveShapeOptions
        {
            ui::viewport::CleaveShapeKind kind{ui::viewport::CleaveShapeKind::Rectangle};
            double width{0.1};
            double height{0.1};
            bool dimensionsLinked{true};
            int segments{16};
            int points{5};
            bool placementArmed{};
            bool cameraPlane{};
            double rotationDegrees{};
        };
        struct CleaveWorkspace
        {
            CleaveDraftSnapshot draft{};
            std::optional<CleaveHistory> history{};
            std::optional<bool> bounded{true};
            bool stagedMode{};
            bool chainEnabled{};
            bool measurementsVisible{true};
            CleaveMeasureUnit measureUnit{CleaveMeasureUnit::Meters};
            bool anglesVisible{true};
            bool lengthsVisible{true};
            bool lineNumbersVisible{};
            double angleStepDegrees{};
            CleaveShapeOptions shapeOptions{};
            StagedCleaveTransformMode transformMode{StagedCleaveTransformMode::Move};
        };
        struct ArchitecturalLightingState
        {
            level::MotsArchLightingVertexMode authoringMode{
                level::MotsArchLightingVertexMode::Rgb};
            ArchitecturalLightingTargetMode targetMode{
                ArchitecturalLightingTargetMode::Owner};
            std::vector<ArchitecturalLightingVertexIdentity> selectedVertices{};
            bool implicitOwnerExpansion{};
        };
        ViewportState editor{};
        ViewportState preview{};
        RenderPipeline pipeline{ RenderPipeline::Editor };
        float nearPlane{};
        float farPlane{};
        bool showGrid{};
        bool gridFollowCamera{};
        bool gridRotateWithView{};
        bool gridKeepViewAlignedVisible{};
        GridPlane gridPlane{ GridPlane::XY };
        float gridMajorSpacing{1.0f};
        float gridMinorSpacing{0.1f};
        bool gridAdaptiveSpacing{true};
        float gridOpacity{0.7f};
        QColor gridColor{};
        QColor gridMajorColor{};
        QColor gridAxisXColor{};
        QColor gridAxisYColor{};
        QColor gridAxisZColor{};
        float gridLineWidthPx{1.0f};
        bool gridMinorDots{};
        float gridMinorDotSizePx{2.0f};
        bool gridVisibleThroughWalls{};
        bool gridAlwaysBehind{};
        bool gridResetOnCameraRotate{};
        bool gridAlignToView{};
        std::optional<QVector3D> gridCenterOverride{};
        QVector3D gridAlignAxisA{1.0f, 0.0f, 0.0f};
        QVector3D gridAlignAxisB{0.0f, 1.0f, 0.0f};
        bool gridOverrideCamBasisValid{};
        QVector3D gridOverrideCamForward{0.0f, 1.0f, 0.0f};
        QVector3D gridOverrideCamUp{0.0f, 0.0f, 1.0f};
        DynamicLightShading dynamicLightShading{DynamicLightShading::Gouraud};
        bool thingLightRenderingEnabled{true};
        bool sectorPointLightRenderingEnabled{true};
        bool showEditorCubes{true};
        bool showPreviewCubes{};
        bool previewRespectHiddenLayers{};
        bool xraySelectionCycling{};
        bool orbitAroundSelection{true};
        bool showSectorThrustVectors{};
        std::array<bool, 4> alwaysShowLightGizmoCategories{};
        TransformTool transformTool{TransformTool::Select};
        TransformOrientation transformOrientation{TransformOrientation::Local};
        bool transformGridSnap{};
        bool transformGridPlaneStick{true};
        std::vector<CleaveLine> cleaveLines{};
        CleaveContext cleaveContext{};
        std::optional<CleaveChainOrigin> cleaveChainOrigin{};
        std::optional<bool> cleaveBounded{};
        bool cleaveStagedMode{};
        bool cleaveChainEnabled{};
        bool cleaveMeasurementsVisible{true};
        CleaveMeasureUnit cleaveMeasureUnit{CleaveMeasureUnit::Meters};
        bool cleaveAnglesVisible{true};
        bool cleaveLengthsVisible{true};
        bool cleaveLineNumbersVisible{};
        double cleaveAngleStepDegrees{};
        std::vector<std::int32_t> applyOrder{};
        std::vector<std::int32_t> selectedCleaveLines{};
        std::int32_t selectedCleaveLine{-1};
        std::optional<CleaveHistory> cleaveHistory{};
        CleaveShapeOptions cleaveShapeOptions{};
        StagedCleaveTransformMode cleaveTransformMode{StagedCleaveTransformMode::Move};
        CleaveWorkspaceFamily cleaveWorkspaceFamily{CleaveWorkspaceFamily::Sector};
        std::optional<CleaveWorkspace> cleavePeerWorkspace{};
        std::optional<ArchitecturalLightingState> architecturalLighting{};
        bool sectorCleaveToolEnabled{};
        bool surfaceCleaveToolEnabled{};
    };

    enum class FlyDirection : std::uint8_t
    {
        Forward,
        Backward,
        Left,
        Right,
        Down,
        Up
    };

    enum class ToastPlacement : std::uint8_t
    {
        TopLeft,
        Center,
        BottomCenter,
    };

    enum class ToastGroup : std::uint8_t
    {
        General,
        Projection,
        FlySpeed,
        Error,
        PlacementYaw,
        Count,
    };

    enum class GridPlane : std::uint8_t
    {
        XY,
        XZ,
        YZ,
    };

    explicit GlView(QWidget* parent = nullptr);
    ~GlView() override;

    void showToast(const QString& text, int durationMs, ToastPlacement placement = ToastPlacement::TopLeft, ToastGroup group = ToastGroup::General);

    void setLevel(const Level* level, SetLevelMode mode = SetLevelMode::resetView);
    // Atomically publish a level and its already-populated template database.
    // Normal callers retain the two-argument behavior above.  Startup uses
    // this overload after resource/template staging so the expensive mesh
    // build runs exactly once for the new document.
    void setLevel(
        const Level* level,
        SetLevelMode mode,
        const assets::Templates* templates);
    void setLevel(
        const Level* level,
        SetLevelMode mode,
        const assets::Templates* templates,
        bool rebuildRenderCache);
    void notifyLevelGeometryChanged();
    void notifyThingsChanged();
    /** Publish changed stored Frame values for the addressed owners. */
    void notifyThingFramesChanged(const std::vector<std::int32_t>& thingIndices);
    /** Publish authored Thing appearance/table changes without the transform fast path. */
    void notifyThingAppearanceChanged();
    /** Publish pure position/orientation/sector changes for a known small Thing set. */
    void notifyThingTransformsChanged(const std::vector<std::int32_t>& thingIndices);
    /** Publish an append-only Thing tail without validating the unchanged prefix. */
    void notifyThingsAppended(std::size_t firstThingIndex);
    [[nodiscard]] std::int32_t findContainingSector(
        const QVector3D& point,
        std::int32_t preferredSector = -1) const noexcept;
    [[nodiscard]] std::int32_t findContainingSector(
        const TVector& point, std::int32_t preferredSector = -1) const noexcept;
    /** Preview one Thing's authored position/orientation without mutating the Level or
     * walking the complete Thing render cache. Used by continuous item-editor scrubs;
     * the caller publishes the authoritative value once the gesture finishes.
     */
    bool setThingPropertyTransformPreview(
        std::int32_t thingIndex,
        const Thing& source,
        const Thing& preview);
    bool setThingPropertyDeltaPreview(
        const std::vector<std::int32_t>& thingIndices,
        std::int32_t primaryThingIndex,
        const QMatrix4x4& transform,
        bool rotating);
    void clearThingPropertyTransformPreview();
    void notifyLightsChanged();
    void notifyBakedLightingChanged(
        const std::vector<std::int32_t>& sectorIndices,
        bool thingShadingMayHaveChanged);
    /** Publish texture-coordinate-only changes for existing level Surfaces.
     * Topology, material batching, lighting, picking, and bounds remain valid,
     * so modal UV previews can patch only the affected VBO ranges.
     */
    void notifySurfaceUvsChanged(const std::vector<SurfaceRef>& surfaceRefs);
    void notifyAllMeshesChanged();
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    struct FrameOverlayWorkForTest
    {
        std::uint64_t frameValueParseCalls{};
        std::uint64_t framePickValueParseCalls{};
        std::uint64_t modelCacheResolveCalls{};
        std::uint64_t sourceFaceEdgeVisits{};
        std::uint64_t topologyCacheBuilds{};
        std::uint64_t uniqueGhostEdgesBuilt{};
        std::uint64_t ghostInstancesSubmitted{};
        std::uint64_t ghostDrawPasses{};
        std::uint64_t lastGhostInstanceCount{};
        std::uint64_t lastFramePathPoseCount{};
        std::uint64_t validFramePosesParsed{};
        std::uint64_t persistentBatchUploads{};
    };

    [[nodiscard]] FrameOverlayWorkForTest frameOverlayWorkForTest() const noexcept
    {
        return frameOverlayWorkForTest_;
    }
    void resetFrameOverlayWorkForTest() noexcept
    {
        frameOverlayWorkForTest_ = {};
    }
    void setFrameGhostTopologyEdgeLimitForTest(
        const std::size_t maximumEdges) noexcept
    {
        frameGhostTopologyEdgeLimitForTest_ = std::min(
            maximumEdges,
            render::frame_ghost::kMaximumGhostEdgeCount);
        invalidateAllFrameRenderCaches();
    }
    void recreateFrameGhostGpuResourcesForTest()
    {
        releaseFrameGhostOpenGlResources();
        initializeFrameGhostOpenGlResources();
    }
    [[nodiscard]] std::optional<render::frame_ghost::FramePose>
    cachedFramePoseForTest(
        const std::int32_t thingIndex,
        const std::int32_t valueIndex)
    {
        const render::frame_ghost::FramePose* const pose = findFramePose(
            thingIndex,
            valueIndex);
        return pose
            ? std::optional<render::frame_ghost::FramePose>{ *pose }
            : std::nullopt;
    }
    void setForceCpuPickForTest(const bool enabled) noexcept
    {
        forceCpuPickForTest_ = enabled;
    }
    [[nodiscard]] bool thingPeelCacheWarmForTest() const noexcept
    {
        return thingPeelCacheValid_ && !thingPeelCache_.hits.empty();
    }
    [[nodiscard]] ui::detail::PreviewResourceDiskWatcher*
    resourceDiskWatcherForTest() const noexcept
    {
        return resourceDiskWatcher_;
    }

    struct CachedLevelSurfaceUvVertexForTest
    {
        std::int32_t sourceTexVertex{-1};
        float cachedU{};
        float cachedV{};
        float publishedU{};
        float publishedV{};
        bool uvNormalized{};

        constexpr bool operator==(
            const CachedLevelSurfaceUvVertexForTest&) const noexcept = default;
    };

    [[nodiscard]] std::vector<CachedLevelSurfaceUvVertexForTest>
    cachedLevelSurfaceUvsForTest(const SurfaceRef ref) const
    {
        if (!levelMeshCache_.valid
            || ref.sector < 0 || ref.surface < 0)
        {
            return {};
        }

        const std::uint64_t key =
            (static_cast<std::uint64_t>(
                 static_cast<std::uint32_t>(ref.sector))
                << 32u)
            | static_cast<std::uint32_t>(ref.surface);
        const auto ranges = surfaceUvRanges_.find(key);
        if (ranges == surfaceUvRanges_.end())
        {
            return {};
        }

        std::vector<CachedLevelSurfaceUvVertexForTest> out{};
        for (const SurfaceUvRange& range : ranges->second)
        {
            if (range.buildIndex >= levelMeshCache_.triBuilds.size()
                || range.localFirst < 0 || range.globalFirst < 0
                || range.count <= 0)
            {
                return {};
            }

            const TriBuild& build = levelMeshCache_.triBuilds[range.buildIndex];
            const std::size_t localFirst =
                static_cast<std::size_t>(range.localFirst);
            const std::size_t globalFirst =
                static_cast<std::size_t>(range.globalFirst);
            const std::size_t count = static_cast<std::size_t>(range.count);
            if (build.shadeInputs.size() != build.vertices.size()
                || levelTriVertexCount_ < 0
                || localFirst > build.vertices.size()
                || count > build.vertices.size() - localFirst
                || globalFirst > triVertices_.size()
                || count > triVertices_.size() - globalFirst
                || globalFirst > static_cast<std::size_t>(levelTriVertexCount_)
                || count > static_cast<std::size_t>(levelTriVertexCount_)
                    - globalFirst)
            {
                return {};
            }

            out.reserve(out.size() + count);
            for (std::size_t offset = 0; offset < count; ++offset)
            {
                const std::size_t localIndex = localFirst + offset;
                const std::size_t globalIndex = globalFirst + offset;
                const TriShadeInput& source = build.shadeInputs[localIndex];
                if (source.sourceSector != ref.sector
                    || source.sourceSurface != ref.surface
                    || source.sourceTexVertex < 0)
                {
                    return {};
                }

                const TriVertex& cached = build.vertices[localIndex];
                const TriVertex& published = triVertices_[globalIndex];
                out.push_back(CachedLevelSurfaceUvVertexForTest{
                    source.sourceTexVertex,
                    cached.u,
                    cached.v,
                    published.u,
                    published.v,
                    build.uvNormalized,
                });
            }
        }
        return out;
    }

    bool previewConnectedSurfaceUvsForTest(const level::ConnectedSurfaceUvPlan& plan,
                                           const level::ConnectedSurfaceUvTransform& transform)
    {
        restoreTransformSurfaceUvPreview();
        transformSurfaceUvPlan_ = plan;
        const level::ConnectedSurfaceUvEvaluation evaluation = level::evaluateConnectedSurfaceUvPlan(plan, transform);
        return publishTransformSurfaceUvPreview(evaluation);
    }

    void restoreConnectedSurfaceUvsForTest() { restoreTransformSurfaceUvPreview(); }

    [[nodiscard]] QVector3D pendingFrameTargetForTest() const noexcept
    {
        return viewAnim_.active ? camera_.orbit.center + viewAnim_.targetPan : camera_.orbit.target();
    }
    [[nodiscard]] bool lastMeshRebuildWasFullForTest() const noexcept
    {
        return lastMeshRebuildKind_ == MeshRebuildKind::All;
    }
    [[nodiscard]] std::uint64_t meshRebuildCountForTest() const noexcept
    {
        return meshRebuildCount_;
    }
    [[nodiscard]] std::uint64_t meshUploadCountForTest() const noexcept
    {
        return meshUploadCount_;
    }
    [[nodiscard]] bool thingMeshCacheValidForTest() const noexcept
    {
        return thingMeshCache_.valid;
    }
    [[nodiscard]] std::optional<double> thingCollisionOverlayExtentForTest(
        const std::size_t thingIndex,
        const bool composedBuffer) const noexcept
    {
        if (!level_ || thingIndex >= level_->things.size()
            || thingIndex >= thingPickIds_.size())
        {
            return std::nullopt;
        }

        const std::uint32_t pickId = thingPickIds_[thingIndex];
        if (pickId == 0u
            || pickId > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
        {
            return std::nullopt;
        }
        const auto rangeIt = thingCollisionOverlayRangesByPickId_.find(
            static_cast<int>(pickId));
        if (rangeIt == thingCollisionOverlayRangesByPickId_.end()
            || rangeIt->second.size() != 1u)
        {
            return std::nullopt;
        }

        const auto& range = rangeIt->second.front();
        if (range.first < 0 || range.count <= 0
            || (composedBuffer && thingOverlayLineFirstVertex_ < 0))
        {
            return std::nullopt;
        }

        const std::vector<LineVertex>& vertices = composedBuffer
            ? lineVertices_
            : thingMeshCache_.overlayLineVertices;
        const std::size_t first = composedBuffer
            ? static_cast<std::size_t>(thingOverlayLineFirstVertex_)
                + static_cast<std::size_t>(range.first)
            : static_cast<std::size_t>(range.first);
        if (first > vertices.size()
            || static_cast<std::size_t>(range.count) > vertices.size() - first)
        {
            return std::nullopt;
        }

        const Thing& thing = level_->things[thingIndex];
        double extent = 0.0;
        for (int offset = 0; offset < range.count; ++offset)
        {
            const LineVertex& vertex = vertices[first + static_cast<std::size_t>(offset)];
            const double dx = static_cast<double>(vertex.x) - thing.x;
            const double dy = static_cast<double>(vertex.y) - thing.y;
            const double dz = static_cast<double>(vertex.z) - thing.z;
            const double radius = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (!std::isfinite(radius))
            {
                return std::nullopt;
            }
            extent = std::max(extent, radius);
        }
        return extent;
    }
    [[nodiscard]] std::optional<double> thingBroadPhaseTreeGuideExtentForTest(
        const std::size_t thingIndex,
        const bool composedBuffer) const noexcept
    {
        if (!level_ || thingIndex >= level_->things.size()
            || thingIndex >= thingPickIds_.size())
        {
            return std::nullopt;
        }

        const std::uint32_t pickId = thingPickIds_[thingIndex];
        if (pickId == 0u
            || pickId > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
        {
            return std::nullopt;
        }
        const auto rangeIt = thingBroadPhaseTreeGuideRangesByPickId_.find(
            static_cast<int>(pickId));
        if (rangeIt == thingBroadPhaseTreeGuideRangesByPickId_.end()
            || rangeIt->second.size() != 1u)
        {
            return std::nullopt;
        }

        const render::DrawRange& range = rangeIt->second.front();
        if (range.first < 0 || range.count <= 0
            || (composedBuffer && thingOverlayLineFirstVertex_ < 0))
        {
            return std::nullopt;
        }
        const std::vector<LineVertex>& vertices = composedBuffer
            ? lineVertices_
            : thingMeshCache_.overlayLineVertices;
        const std::size_t first = composedBuffer
            ? static_cast<std::size_t>(thingOverlayLineFirstVertex_)
                + static_cast<std::size_t>(range.first)
            : static_cast<std::size_t>(range.first);
        if (first > vertices.size()
            || static_cast<std::size_t>(range.count) > vertices.size() - first)
        {
            return std::nullopt;
        }

        const Thing& thing = level_->things[thingIndex];
        double extent = 0.0;
        for (int offset = 0; offset < range.count; ++offset)
        {
            const LineVertex& vertex = vertices[first + static_cast<std::size_t>(offset)];
            const double dx = static_cast<double>(vertex.x) - thing.x;
            const double dy = static_cast<double>(vertex.y) - thing.y;
            const double dz = static_cast<double>(vertex.z) - thing.z;
            const double radius = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (!std::isfinite(radius))
            {
                return std::nullopt;
            }
            extent = std::max(extent, radius);
        }
        return extent;
    }
    [[nodiscard]] std::optional<std::array<double, 6>>
    thingVisualBoundsOverlayAabbForTest(
        const std::size_t thingIndex,
        const bool composedBuffer) const noexcept
    {
        if (!level_ || thingIndex >= level_->things.size()
            || thingIndex >= thingPickIds_.size())
        {
            return std::nullopt;
        }

        const std::uint32_t pickId = thingPickIds_[thingIndex];
        if (pickId == 0u
            || pickId > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
        {
            return std::nullopt;
        }
        const auto rangeIt = thingVisualBoundsOverlayRangesByPickId_.find(
            static_cast<int>(pickId));
        if (rangeIt == thingVisualBoundsOverlayRangesByPickId_.end()
            || rangeIt->second.empty())
        {
            return std::nullopt;
        }

        const std::vector<LineVertex>& vertices = composedBuffer
            ? lineVertices_
            : thingMeshCache_.overlayLineVertices;
        std::array<double, 6> bounds{};
        bool havePoint = false;
        for (const DrawRange& range : rangeIt->second)
        {
            if (range.first < 0 || range.count <= 0
                || (composedBuffer && thingOverlayLineFirstVertex_ < 0))
            {
                return std::nullopt;
            }
            const std::size_t first = composedBuffer
                ? static_cast<std::size_t>(thingOverlayLineFirstVertex_)
                    + static_cast<std::size_t>(range.first)
                : static_cast<std::size_t>(range.first);
            if (first > vertices.size()
                || static_cast<std::size_t>(range.count) > vertices.size() - first)
            {
                return std::nullopt;
            }

            for (int offset = 0; offset < range.count; ++offset)
            {
                const LineVertex& vertex = vertices[first + static_cast<std::size_t>(offset)];
                const std::array<double, 3> point{
                    static_cast<double>(vertex.x),
                    static_cast<double>(vertex.y),
                    static_cast<double>(vertex.z),
                };
                if (!std::isfinite(point[0])
                    || !std::isfinite(point[1])
                    || !std::isfinite(point[2]))
                {
                    return std::nullopt;
                }
                if (!havePoint)
                {
                    bounds = { point[0], point[1], point[2],
                        point[0], point[1], point[2] };
                    havePoint = true;
                    continue;
                }
                for (std::size_t axis = 0; axis < 3u; ++axis)
                {
                    bounds[axis] = std::min(bounds[axis], point[axis]);
                    bounds[axis + 3u] = std::max(bounds[axis + 3u], point[axis]);
                }
            }
        }
        return havePoint ? std::optional<std::array<double, 6>>{ bounds } : std::nullopt;
    }

    [[nodiscard]] std::optional<std::array<double, 6>>
    thingTemplateCollisionBoundsGuideBoundsForTest(
        const std::size_t thingIndex,
        const bool composedBuffer) const noexcept
    {
        if (!level_ || thingIndex >= level_->things.size()
            || thingIndex >= thingPickIds_.size())
        {
            return std::nullopt;
        }
        const std::uint32_t pickId = thingPickIds_[thingIndex];
        if (pickId == 0u
            || pickId > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
        {
            return std::nullopt;
        }
        const auto rangeIt = thingTemplateCollisionBoundsGuideRangesByPickId_.find(
            static_cast<int>(pickId));
        if (rangeIt == thingTemplateCollisionBoundsGuideRangesByPickId_.end()
            || rangeIt->second.empty()
            || (composedBuffer && thingOverlayLineFirstVertex_ < 0))
        {
            return std::nullopt;
        }

        const std::vector<LineVertex>& vertices = composedBuffer
            ? lineVertices_
            : thingMeshCache_.overlayLineVertices;
        std::array<double, 6> bounds{
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity(),
            -std::numeric_limits<double>::infinity(),
        };
        bool haveVertex = false;
        for (const render::DrawRange& range : rangeIt->second)
        {
            if (range.first < 0 || range.count <= 0)
            {
                continue;
            }
            const std::size_t first = composedBuffer
                ? static_cast<std::size_t>(thingOverlayLineFirstVertex_)
                    + static_cast<std::size_t>(range.first)
                : static_cast<std::size_t>(range.first);
            if (first > vertices.size()
                || static_cast<std::size_t>(range.count) > vertices.size() - first)
            {
                return std::nullopt;
            }
            for (int offset = 0; offset < range.count; ++offset)
            {
                const LineVertex& vertex = vertices[first + static_cast<std::size_t>(offset)];
                const std::array<double, 3> point{ vertex.x, vertex.y, vertex.z };
                if (!std::isfinite(point[0]) || !std::isfinite(point[1])
                    || !std::isfinite(point[2]))
                {
                    return std::nullopt;
                }
                for (std::size_t axis = 0; axis < point.size(); ++axis)
                {
                    bounds[axis] = std::min(bounds[axis], point[axis]);
                    bounds[axis + 3u] = std::max(bounds[axis + 3u], point[axis]);
                }
                haveVertex = true;
            }
        }
        return haveVertex ? std::optional<std::array<double, 6>>{ bounds } : std::nullopt;
    }
    [[nodiscard]] std::size_t thingRenderedLodIndexForTest(
        const std::size_t thingIndex) const noexcept
    {
        return thingIndex < thingRenderState_.size()
            ? thingRenderState_[thingIndex].lodIndex : 0u;
    }
    [[nodiscard]] std::size_t thingPickTriangleVertexCountForTest(
        const std::size_t thingIndex) const noexcept
    {
        if (thingIndex >= thingPickIds_.size())
        {
            return 0u;
        }
        const auto found = thingTriBuildRangesByPickId_.find(
            static_cast<int>(thingPickIds_[thingIndex]));
        if (found == thingTriBuildRangesByPickId_.end())
        {
            return 0u;
        }
        std::size_t count = 0u;
        for (const auto& range : found->second)
        {
            if (range.count > 0)
            {
                count += static_cast<std::size_t>(range.count);
            }
        }
        return count;
    }
    [[nodiscard]] std::size_t levelTriangleCacheBatchCountForTest() const noexcept
    {
        return levelMeshCache_.triBuilds.size();
    }
    [[nodiscard]] std::size_t thingTriangleCacheVertexCountForTest() const noexcept
    {
        std::size_t count = 0;
        for (const auto& build : thingMeshCache_.triBuilds)
        {
            count += build.vertices.size();
        }
        return count;
    }
    [[nodiscard]] std::uint64_t thingTriangleCacheFingerprintForTest() const noexcept
    {
        std::uint64_t hash = 14695981039346656037ull;
        const auto appendFloat = [&hash](const float value) noexcept
        {
            std::uint32_t bits{};
            static_assert(sizeof(bits) == sizeof(value));
            std::memcpy(&bits, &value, sizeof(bits));
            hash ^= bits;
            hash *= 1099511628211ull;
        };
        for (const auto& build : thingMeshCache_.triBuilds)
        {
            hash ^= static_cast<std::uint64_t>(build.vertices.size());
            hash *= 1099511628211ull;
            for (const auto& vertex : build.vertices)
            {
                appendFloat(vertex.x);
                appendFloat(vertex.y);
                appendFloat(vertex.z);
                appendFloat(vertex.u);
                appendFloat(vertex.v);
                appendFloat(vertex.r);
                appendFloat(vertex.g);
                appendFloat(vertex.b);
                appendFloat(vertex.a);
            }
        }
        return hash;
    }
    [[nodiscard]] const Level* attachedLevelForTest() const noexcept
    {
        return level_;
    }
    [[nodiscard]] std::uint64_t surfaceUvFastPublicationCountForTest() const noexcept
    {
        return surfaceUvFastPublicationCount_;
    }
    [[nodiscard]] std::uint64_t resourceLocatorUploadCountForTest() const noexcept
    {
        return resourceLocatorUploadCount_;
    }
    [[nodiscard]] std::uint64_t resourceLocatorDeferredUploadCountForTest() const noexcept
    {
        return resourceLocatorDeferredUploadCount_;
    }
    [[nodiscard]] std::uint64_t placementPreviewFastUpdateCountForTest() const noexcept
    {
        return placementPreviewFastUpdateCount_;
    }
    [[nodiscard]] std::uint64_t spriteBatchBoundsVisitCountForTest() const noexcept
    {
        return spriteBatchBoundsVisitCount_;
    }
    [[nodiscard]] std::uint64_t spriteInstanceVertexGenerationCountForTest() const noexcept
    {
        return spriteInstanceVertexGenerationCount_;
    }
    void rebuildSpriteVerticesForTest()
    {
        rebuildSpriteVertices();
    }
    [[nodiscard]] std::uint64_t thingTypeGizmoBuildCountForTest() const noexcept
    {
        return thingTypeGizmoBuildCount_;
    }
    [[nodiscard]] std::uint64_t thingTypeGizmoMetadataBuildCountForTest() const noexcept
    {
        return thingTypeGizmoMetadataBuildCount_;
    }
    /** Return the exact world-space center used by the uploaded Thing or ARCH
     * owner icon. The ARCH owner-picking regression uses this seam to click
     * the marker that was actually drawn instead of reconstructing a hidden
     * origin or an approximate model center in the test.
     */
    [[nodiscard]] std::optional<QVector3D>
    architecturalLightingOwnerGizmoDisplayPositionForTest(
        const std::int32_t thingIndex) const noexcept
    {
        const auto found = architecturalLightingOwnerGizmoEntryByThing_.find(thingIndex);
        if (found == architecturalLightingOwnerGizmoEntryByThing_.end()
            || found->second >= architecturalLightingOwnerGizmoEntries_.size())
        {
            return std::nullopt;
        }
        return architecturalLightingOwnerGizmoEntries_[found->second].center;
    }

    [[nodiscard]] std::optional<render::ThingBounds>
    architecturalLightingOwnerGizmoBoundsForTest(
        const std::int32_t thingIndex) const noexcept
    {
        const auto found = architecturalLightingOwnerGizmoEntryByThing_.find(thingIndex);
        if (found == architecturalLightingOwnerGizmoEntryByThing_.end()
            || found->second >= architecturalLightingOwnerGizmoEntries_.size())
        {
            return std::nullopt;
        }
        const ArchitecturalLightingOwnerGizmoEntry& entry =
            architecturalLightingOwnerGizmoEntries_[found->second];
        return render::ThingBounds{ entry.boundsMin, entry.boundsMax, true };
    }

    /** Read the scene depth used by ARCH vertex picking at one widget point. */
    [[nodiscard]] std::optional<float> architecturalLightingSceneDepthForTest(
        const QPointF& widgetPoint);

    [[nodiscard]] std::optional<QVector3D>
    thingTypeGizmoDisplayPositionForTest(const std::int32_t thingIndex) const noexcept
    {
        if ((renderPipeline_ == RenderPipeline::Editor
             && architecturalLightingEditingActive_)
            || (level_ && level_->kind == ProjectType::MOTS
                && showArchThingGizmos_))
        {
            const auto archEntry = architecturalLightingOwnerGizmoEntryByThing_.find(
                thingIndex);
            if (archEntry != architecturalLightingOwnerGizmoEntryByThing_.end()
                && archEntry->second < architecturalLightingOwnerGizmoEntries_.size())
            {
                return architecturalLightingOwnerGizmoEntries_[archEntry->second].center;
            }
        }
        if (!thingTypeGizmoMetadataValid_
            || thingIndex < 0
            || static_cast<std::size_t>(thingIndex) >= thingPickIds_.size())
        {
            return std::nullopt;
        }
        const std::uint32_t pickId =
            thingPickIds_[static_cast<std::size_t>(thingIndex)];
        const auto found = thingTypeGizmoEntryByPickId_.find(pickId);
        if (found == thingTypeGizmoEntryByPickId_.end()
            || found->second >= thingTypeGizmoEntries_.size())
        {
            return std::nullopt;
        }
        return thingTypeGizmoEntries_[found->second].center;
    }
    [[nodiscard]] std::optional<QVector3D>
    archThingGizmoDisplayPositionForTest(
        const std::int32_t thingIndex) const noexcept
    {
        if (!showArchThingGizmos_
            || !level_
            || level_->kind != ProjectType::JKDF2
            || !thingTypeGizmoMetadataValid_
            || thingIndex < 0
            || static_cast<std::size_t>(thingIndex) >= thingPickIds_.size())
        {
            return std::nullopt;
        }
        const std::uint32_t pickId =
            thingPickIds_[static_cast<std::size_t>(thingIndex)];
        const auto found = thingTypeGizmoEntryByPickId_.find(pickId);
        if (found == thingTypeGizmoEntryByPickId_.end()
            || found->second >= thingTypeGizmoEntries_.size())
        {
            return std::nullopt;
        }
        const auto& entry = thingTypeGizmoEntries_[found->second];
        return entry.archThing
            ? std::optional<QVector3D>{entry.archThingMarkerCenter}
            : std::nullopt;
    }
    [[nodiscard]] bool boxSelectActiveForTest() const noexcept
    {
        return boxSelectActive_;
    }
    [[nodiscard]] bool boxSelectVisibleForTest() const noexcept
    {
        return boxSelectVisible_;
    }
    [[nodiscard]] std::size_t selectionOutlineSourceVertexCountForTest() const noexcept
    {
        std::size_t count = 0;
        for (const OutlineSourceRange& range : outlineSourceRanges_)
        {
            if (range.count > 0)
            {
                count += static_cast<std::size_t>(range.count);
            }
        }
        return count;
    }
    [[nodiscard]] std::size_t selectionOutlineCopiedVertexCountForTest() const noexcept
    {
        return outlineTriVertices_.size();
    }
    [[nodiscard]] std::uint64_t selectionOutlineCacheHitCountForTest() const noexcept
    {
        return selectionOutlineCacheHitCount_;
    }
    [[nodiscard]] std::uint64_t selectionOutlineFallbackBuildCountForTest() const noexcept
    {
        return selectionOutlineFallbackBuildCount_;
    }
    [[nodiscard]] std::uint64_t thingTransformFastPublicationCountForTest() const noexcept
    {
        return thingTransformFastPublicationCount_;
    }
    [[nodiscard]] std::uint64_t thingTransformFallbackRebuildCountForTest() const noexcept
    {
        return thingTransformFallbackRebuildCount_;
    }
    [[nodiscard]] int thingTransformLastFallbackReasonForTest() const noexcept
    {
        return static_cast<int>(lastThingTransformFastFallbackReason_);
    }
    [[nodiscard]] std::uint64_t thingTransformFastFallbackAttemptCountForTest() const noexcept
    {
        std::uint64_t count = 0;
        for (const std::uint64_t reasonCount : thingTransformFastFallbackReasonCounts_)
        {
            count += reasonCount;
        }
        return count;
    }
    [[nodiscard]] std::size_t validThingBoundsCountForTest() const noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            thingBounds_.begin(),
            thingBounds_.end(),
            [](const ThingBounds& bounds) { return bounds.valid; }));
    }
    [[nodiscard]] bool thingBoundsValidForTest(const std::size_t index) const noexcept
    {
        return index < thingBounds_.size() && thingBounds_[index].valid;
    }
    [[nodiscard]] std::size_t placementPreviewRenderedThingCountForTest() const noexcept
    {
        return placementPreviewRenderedThings_.size();
    }
    /** RGB is the per-vertex local Sector Tint scale; A is rendered face opacity. */
    [[nodiscard]] std::optional<QVector4D> levelSurfaceRenderStateForTest(
        const SurfaceRef ref) const noexcept
    {
        if (ref.sector < 0 || ref.surface < 0)
        {
            return std::nullopt;
        }
        const std::uint64_t key =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32u)
            | static_cast<std::uint32_t>(ref.surface);
        const auto ranges = surfaceUvRanges_.find(key);
        if (ranges == surfaceUvRanges_.end())
        {
            return std::nullopt;
        }
        for (const SurfaceUvRange& range : ranges->second)
        {
            if (range.buildIndex >= levelMeshCache_.triBuilds.size()
                || range.localFirst < 0)
            {
                continue;
            }
            const auto& vertices = levelMeshCache_.triBuilds[range.buildIndex].vertices;
            if (static_cast<std::size_t>(range.localFirst) >= vertices.size())
            {
                continue;
            }
            const TriVertex& vertex = vertices[static_cast<std::size_t>(range.localFirst)];
            return QVector4D(
                vertex.editorSectorTintR,
                vertex.editorSectorTintG,
                vertex.editorSectorTintB,
                vertex.a);
        }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<QVector3D> thingModelEditorSectorTintForTest(
        const std::size_t thingIndex) const noexcept
    {
        if (thingIndex >= thingPickIds_.size())
        {
            return std::nullopt;
        }
        const auto ranges = thingTriBuildRangesByPickId_.find(
            static_cast<int>(thingPickIds_[thingIndex]));
        if (ranges == thingTriBuildRangesByPickId_.end())
        {
            return std::nullopt;
        }
        for (const auto& range : ranges->second)
        {
            if (range.buildIndex >= thingMeshCache_.triBuilds.size()
                || range.first < 0
                || range.count <= 0)
            {
                continue;
            }
            const auto& vertices =
                thingMeshCache_.triBuilds[range.buildIndex].vertices;
            if (static_cast<std::size_t>(range.first) >= vertices.size())
            {
                continue;
            }
            const auto& vertex = vertices[static_cast<std::size_t>(range.first)];
            return QVector3D(
                vertex.editorSectorTintR,
                vertex.editorSectorTintG,
                vertex.editorSectorTintB);
        }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<ColorF> thingModelVertexColorForTest(
        const std::size_t thingIndex) const noexcept
    {
        if (thingIndex >= thingPickIds_.size())
        {
            return std::nullopt;
        }
        const auto ranges = thingTriBuildRangesByPickId_.find(
            static_cast<int>(thingPickIds_[thingIndex]));
        if (ranges == thingTriBuildRangesByPickId_.end())
        {
            return std::nullopt;
        }
        for (const auto& range : ranges->second)
        {
            if (range.buildIndex >= thingMeshCache_.triBuilds.size()
                || range.first < 0
                || range.count <= 0)
            {
                continue;
            }
            const auto& vertices =
                thingMeshCache_.triBuilds[range.buildIndex].vertices;
            if (static_cast<std::size_t>(range.first) >= vertices.size())
            {
                continue;
            }
            const auto& vertex = vertices[static_cast<std::size_t>(range.first)];
            return ColorF{ vertex.r, vertex.g, vertex.b, vertex.a };
        }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<ColorF> thingModelVertexColorForTest(
        const std::size_t thingIndex,
        const std::int32_t meshIndex,
        const std::int32_t vertexIndex) const noexcept
    {
        if (thingIndex >= thingPickIds_.size()
            || meshIndex < 0 || vertexIndex < 0)
        {
            return std::nullopt;
        }

        const auto ranges = thingTriBuildRangesByPickId_.find(
            static_cast<int>(thingPickIds_[thingIndex]));
        if (ranges == thingTriBuildRangesByPickId_.end()
            || thingTriBuildGlobalFirstVertex_.size()
                != thingMeshCache_.triBuilds.size())
        {
            return std::nullopt;
        }

        for (const auto& range : ranges->second)
        {
            if (range.buildIndex >= thingMeshCache_.triBuilds.size()
                || range.first < 0 || range.count <= 0
                || static_cast<std::size_t>(range.first + range.count)
                    > thingMeshCache_.triBuilds[range.buildIndex].shadeInputs.size())
            {
                continue;
            }

            const TriBuild& build = thingMeshCache_.triBuilds[range.buildIndex];
            if (build.shadeInputs.size() != build.vertices.size())
            {
                continue;
            }
            const int globalBase =
                thingTriBuildGlobalFirstVertex_[range.buildIndex];
            for (int offset = 0; offset < range.count; ++offset)
            {
                const int localIndex = range.first + offset;
                const TriShadeInput& shade =
                    build.shadeInputs[static_cast<std::size_t>(localIndex)];
                if (shade.archMeshIndex != meshIndex
                    || shade.archVertexIndex != vertexIndex)
                {
                    continue;
                }
                const int globalIndex = globalBase + localIndex;
                if (globalIndex < 0
                    || static_cast<std::size_t>(globalIndex) >= triVertices_.size())
                {
                    continue;
                }
                const TriVertex& published =
                    triVertices_[static_cast<std::size_t>(globalIndex)];
                return ColorF{
                    published.r,
                    published.g,
                    published.b,
                    published.a };
            }
        }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<QVector3D> thingSpriteEditorSectorTintForTest(
        const std::size_t thingIndex) const noexcept
    {
        if (thingIndex >= thingPickIds_.size())
        {
            return std::nullopt;
        }
        const auto ranges = spriteTriRangesByPickId_.find(
            static_cast<int>(thingPickIds_[thingIndex]));
        if (ranges == spriteTriRangesByPickId_.end())
        {
            return std::nullopt;
        }
        for (const auto& range : ranges->second)
        {
            if (range.first < 0 || range.count <= 0
                || static_cast<std::size_t>(range.first) >= spriteTriVertices_.size())
            {
                continue;
            }
            const auto& vertex =
                spriteTriVertices_[static_cast<std::size_t>(range.first)];
            return QVector3D(
                vertex.editorSectorTintR,
                vertex.editorSectorTintG,
                vertex.editorSectorTintB);
        }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::array<QVector3D, 4>> thingSpriteCornersForTest(
        const std::size_t thingIndex) const noexcept
    {
        if ( thingIndex >= thingPickIds_.size() )
        {
            return std::nullopt;
        }
        const auto ranges = spriteTriRangesByPickId_.find(
            static_cast<int>(thingPickIds_[thingIndex]));
        if ( ranges == spriteTriRangesByPickId_.end() )
        {
            return std::nullopt;
        }
        for ( const auto& range : ranges->second )
        {
            if ( range.first < 0 || range.count < 6
                || static_cast<std::size_t>(range.first) + 6u > spriteTriVertices_.size() )
            {
                continue;
            }
            const auto pointAt = [this, &range](const std::size_t corner)
            {
                const TriVertex& vertex = spriteTriVertices_[
                    static_cast<std::size_t>(range.first) + corner];
                return QVector3D(vertex.x, vertex.y, vertex.z);
            };
            return std::array<QVector3D, 4>{
                pointAt(0u), pointAt(1u), pointAt(2u), pointAt(5u) };
        }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<TVector> placementPreviewRenderedThingPositionForTest(
        const std::size_t index) const noexcept
    {
        if (index >= placementPreviewRenderedThings_.size())
        {
            return std::nullopt;
        }
        const Thing& thing = placementPreviewRenderedThings_[index];
        return TVector{ thing.x, thing.y, thing.z };
    }
    [[nodiscard]] std::optional<Thing> placementPreviewThingForTest() const
    {
        return placementPreviewThing_;
    }
    [[nodiscard]] bool mainShaderReadyForTest() const noexcept
    {
        return program_ != nullptr;
    }
    [[nodiscard]] bool analyticGridShaderReadyForTest() const noexcept
    {
        return gridProgram_ != nullptr && gridQuadVao_ != nullptr;
    }
    [[nodiscard]] bool dynamicLightUniformBufferReadyForTest() const noexcept
    {
        return dynamicLightUniformBufferReady_;
    }
    [[nodiscard]] QString toastTextForTest(const ToastGroup group) const
    {
        const std::size_t index = static_cast<std::size_t>(group);
        return index < toastStates_.size() ? toastStates_[index].text : QString{};
    }
    void captureStagedCleaveContextForTest(const bool surfaceMode)
    {
        captureStagedCleaveContext(surfaceMode);
    }
    [[nodiscard]] bool stagedCleaveContextMatchesCurrentForTest(const bool surfaceMode)
    {
        return stagedCleaveContextMatchesCurrent(surfaceMode);
    }
    [[nodiscard]] bool stagedCleaveTargetsMatchCurrentForTest(const bool surfaceMode) const
    {
        return stagedCleaveTargetsMatchCurrent(surfaceMode);
    }
    void previewThingLightTransformForTest(
        render::LightGizmoKind kind,
        const QVector3D& translation);
    void beginBakedLightTransformHandleClickForTest(
        std::int32_t lightIndex,
        const QPoint& pressPosition);
    void clearTransformPreviewForTest();
    bool beginGeometryMoveSnapForTest(SurfaceRef moving, SurfaceRef stationary,
        bool sectorTarget, const TVector& rawTranslation, const TVector& pivot);
    void beginThingMoveTransformForTest(const QVector3D& translation);
    /**
     * Seed one complete Thing transform gesture for hot-path tests. Unlike
     * updateThingMovePointerForTest(), this leaves the drag active so several
     * samples exercise the same captured selection and camera state.
     */
    bool beginThingMoveDragForTest(
        const std::vector<std::int32_t>& thingIndices,
        bool surfaceStick,
        bool thingSnap);
    void updateThingMoveDragSampleForTest(int pointerSteps);
    /** Seed one selected-frame Move gesture against the production drag solver. */
    bool beginFrameMoveDragForTest(
        std::int32_t thingIndex,
        std::int32_t valueIndex,
        bool surfaceStick,
        bool thingSnap);
    void updateFrameMoveDragSampleForTest(int pointerSteps);
    void updateThingMovePointerForTest(float origin, bool gridSnap, bool surfaceStick = false, bool thingSnap = false, bool surfaceAlign = false, int pointerSteps = 1, bool gridPlaneStick = false, bool geometrySnap = false, float yawDeg = 0.0f, float pitchDeg = 0.0f, bool perspective = false, bool actualCamera = false);
    void updateThingMovePointerCameraForTest(
        float origin,
        bool gridSnap,
        bool geometrySnap,
        bool surfaceStick,
        bool surfaceAlign,
        float yawDeg,
        float pitchDeg,
        bool perspective,
        float desiredScreenRightWorldDistance);
    void beginThingRotateTransformForTest(
        const QVector3D& axis,
        float angleDeg,
        const QVector3D& pivot);
    void beginSectorScaleTransformForTest(float scale, const TVector& pivot = TVector{});
    void endTransformDragForTest();
    [[nodiscard]] std::uint64_t geometryMoveSnapCacheBuildCountForTest() const noexcept
    {
        return geometryMoveSnapCacheBuildCount_;
    }
    [[nodiscard]] std::size_t geometryMoveSnapCacheSurfaceCountForTest() const noexcept
    {
        return transformDrag_.geometryMoveSnapCache.movingSurfaces.size()
            + transformDrag_.geometryMoveSnapCache.stationarySurfaces.size();
    }
    void resetThingMoveHotPathCountersForTest() noexcept
    {
        thingMoveDragSampleCount_ = 0;
        thingMoveSnapAnchorVisitCount_ = 0;
        thingMoveBoundsResolveCount_ = 0;
        thingMoveResourceResolveCount_ = 0;
        thingMoveSurfacePickQueryCount_ = 0;
    }
    [[nodiscard]] std::uint64_t thingMoveDragSampleCountForTest() const noexcept
    {
        return thingMoveDragSampleCount_;
    }
    [[nodiscard]] std::uint64_t thingMoveSnapAnchorVisitCountForTest() const noexcept
    {
        return thingMoveSnapAnchorVisitCount_;
    }
    [[nodiscard]] std::uint64_t thingMoveBoundsResolveCountForTest() const noexcept
    {
        return thingMoveBoundsResolveCount_;
    }
    [[nodiscard]] std::uint64_t thingMoveResourceResolveCountForTest() const noexcept
    {
        return thingMoveResourceResolveCount_;
    }
    [[nodiscard]] std::uint64_t thingMoveSurfacePickQueryCountForTest() const noexcept
    {
        return thingMoveSurfacePickQueryCount_;
    }
    [[nodiscard]] bool transformPreviewActiveForTest() const noexcept
    {
        return transformPreviewActive_;
    }
    [[nodiscard]] bool thingPropertyTransformPreviewActiveForTest() const noexcept
    {
        return thingPropertyTransformPreviewActive_;
    }
    [[nodiscard]] QVector3D thingPropertyTransformPreviewPointForTest(
        const QVector3D& point) const
    {
        return thingPropertyTransformPreviewActive_
            ? transformPreviewMatrix_.map(point)
            : point;
    }
    [[nodiscard]] QVector3D transformPreviewPointForTest(
        const QVector3D& point) const
    {
        return transformPreviewMatrix_.map(point);
    }
    [[nodiscard]] QVector3D thingLightGizmoDisplayPositionForTest(
        const render::LightGizmoEntry& entry,
        const QVector3D& cameraUp,
        const float baseHalfSize) const noexcept
    {
        return lightGizmoDisplayPositionForEntry(entry, cameraUp, baseHalfSize);
    }
    [[nodiscard]] std::optional<QVector3D> selectionOrbitDragPivotForTest() const noexcept
    {
        return selectionOrbitDragPivot_;
    }
    [[nodiscard]] int selectionHighlightPointCountForTest() const noexcept
    {
        return static_cast<int>(highlightPointPositions_.size() / 3u);
    }
    [[nodiscard]] int selectionVertexBasePointCountForTest() const noexcept
    {
        return std::clamp(
            highlightPointSelectedFirstPoint_,
            0,
            selectionHighlightPointCountForTest());
    }
    [[nodiscard]] int selectionVertexSelectedPointCountForTest() const noexcept
    {
        return highlightPointSelectedPointCount_;
    }
    [[nodiscard]] int selectionVertexActivePointCountForTest() const noexcept
    {
        return activeVertexHighlightPointCount_;
    }
    struct SelectionWirePlaneSupportForTest
    {
        bool valid{};
        QVector3D reference{};
    };
    [[nodiscard]] std::optional<SelectionWirePlaneSupportForTest>
    selectionWirePlaneSupportForTest(
        const QVector3D& start,
        const QVector3D& end) const noexcept;
    [[nodiscard]] int selectionHighlightLineCountForTest() const noexcept
    {
        return static_cast<int>(highlightLineVertices_.size());
    }
    [[nodiscard]] int selectionSecondaryLineCountForTest() const noexcept
    {
        return highlightLineMultiVertexCount_;
    }
    [[nodiscard]] int selectionActiveEdgeLineCountForTest() const noexcept
    {
        return activeEdgeHighlightVertexCount_;
    }
    [[nodiscard]] int selectionSecondaryEdgeLineCountForTest() const noexcept
    {
        return secondaryEdgeHighlightVertexCount_;
    }
    [[nodiscard]] int selectionActiveOwnerSurfaceLineCountForTest() const noexcept
    {
        return activeSurfaceOwnerHighlightVertexCount_;
    }
    [[nodiscard]] std::size_t selectedSurfaceEdgeCountForTest() const noexcept
    {
        return selectedSurfaceEdges_.size();
    }
    [[nodiscard]] bool activeSurfaceEdgeIsSelectedForTest() const noexcept
    {
        return activeSurfaceEdge_
            && selectedSurfaceEdges_.contains(*activeSurfaceEdge_);
    }
    [[nodiscard]] std::optional<SurfaceRef>
    activeSurfaceEdgeOwnerForTest() const noexcept
    {
        return activeSurfaceEdgeOwner();
    }
    [[nodiscard]] std::optional<std::pair<SurfaceRef, std::int32_t>>
    activeSurfaceEdgeForTest() const noexcept
    {
        if (!activeSurfaceEdge_)
        {
            return std::nullopt;
        }
        return std::pair{
            activeSurfaceEdge_->surface, activeSurfaceEdge_->localEdge };
    }
    [[nodiscard]] std::optional<editor::SurfaceVertexRef>
    activeSurfaceVertexForTest() const noexcept
    {
        return activeSurfaceVertex();
    }
    void setSelectedSurfaceEdgesForTest(
        const std::vector<std::pair<SurfaceRef, std::int32_t>>& edges,
        std::optional<std::size_t> activeIndex = std::nullopt);
    void applySurfaceEdgePointSelectionForTest(
        SurfaceRef surface,
        std::int32_t localEdge,
        Qt::KeyboardModifiers modifiers);
    void applySurfaceEdgeBoxSelectionForTest(
        const std::vector<std::pair<SurfaceRef, std::int32_t>>& edges,
        SelectionBoxMode mode);
    void setLastSurfaceHitForTest(
        const SurfaceRef ref,
        const TVector& position)
    {
        haveLastSurfaceHit_ = true;
        lastSurfaceHitSurface_ = ref;
        lastSurfaceHitPos_ = position;
        if (surfaceSubMode_ == SurfaceSubMode::Edge)
        {
            if (const auto edge = surfaceEdgeAtHit(ref, position))
            {
                selectedSurfaceEdges_.insert(*edge);
                activeSurfaceEdge_ = *edge;
            }
        }
        rebuildSelectionOverlays();
    }
#endif
    void reloadCachedResources();
    /**
     * Seed the renderer's model cache with an immutable 3DO that has already
     * been parsed by the caller. Preview/thumbnail workers use this seam to
     * keep parsing and hierarchy expansion off the GUI thread while OpenGL
     * objects remain owned by this widget's thread.
     */
    void primeModel3do(
        std::string_view name,
        std::shared_ptr<const assets::Model3do> model,
        std::size_t lodIndex = 0u);
    /** Seed decoded, vertically oriented MAT pixels prepared off-thread. */
    void primeMaterialTexture(
        std::string_view material,
        std::string_view colormap,
        const QImage& image,
        std::int32_t alphaBpp,
        const QVector3D& solidColor = QVector3D(1.0f, 1.0f, 1.0f),
        const std::optional<ClassicSolidLightRamp>& classicSolidLightRamp =
            std::nullopt);
    /** Replace one already-used preview MAT cel without rebuilding its meshes. */
    bool updatePreviewMaterialTexture(
        std::string_view material,
        std::string_view colormap,
        const QImage& image,
        std::int32_t alphaBpp,
        const QVector3D& solidColor = QVector3D(1.0f, 1.0f, 1.0f),
        const std::optional<ClassicSolidLightRamp>& classicSolidLightRamp =
            std::nullopt);
    /** Patch numeric geometry in the isolated one-Thing Sprite authoring view. */
    bool updateIsolatedSpritePreviewGeometry(const assets::SprFile& sprite, const ThingBox& bounds);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    [[nodiscard]] std::uint64_t model3doResourceReadCountForTest() const noexcept
    {
        return model3doResourceReadCount_;
    }
    [[nodiscard]] std::uint64_t materialResourceReadCountForTest() const noexcept
    {
        return materialResourceReadCount_;
    }
    [[nodiscard]] bool materialTextureMissingForTest(
        std::string_view material,
        std::string_view colormap) const noexcept;
    [[nodiscard]] std::optional<std::int32_t> materialTextureAlphaBppForTest(
        std::string_view material,
        std::string_view colormap) const noexcept;
#endif
    /**
     * Publish the resource locator and invalidate resource-backed caches.
     *
     * Normal callers retain the historical behavior (including an immediate
     * GPU upload).  Level/recovery loading can pass false while it stages a
     * new document: uploading the old document's meshes after clearing their
     * textures is wasted work, because the final deferred mesh publication
     * uploads the new cache once the editor state has been restored.
     */
    void setResourceLocator(
        const assets::ResourceLocator* locator,
        bool uploadRenderCache = true);
    /**
     * Install the owner transaction used when a mounted archive changes.
     * Returning false retains the watcher event for a backoff retry; the
     * resourceArchivesChanged signal is emitted only after a true result.
     */
    void setResourceArchiveReloadCallback(std::function<bool()> callback);
    /**
     * Disable mounted archive file watching for previews whose owner already
     * watches and remounts the archive. Loose dependency files and their
     * candidate directories remain watched.
     */
    void setMountedArchiveWatchingEnabled(bool enabled);
    void setTemplateDatabase(const assets::Templates* templates);
    void setSelection(std::int32_t sectorIndex, std::int32_t surfaceIndex, std::int32_t thingIndex, std::int32_t lightIndex);
    void setHighlightedSelection(const editor::Selection& selection);
    void setSelectionState(
        std::int32_t sectorIndex,
        std::int32_t surfaceIndex,
        std::int32_t thingIndex,
        std::int32_t lightIndex,
        const editor::Selection& selection);
    void setSelectedSector(std::int32_t sectorIndex);
    void setSelectedSurface(std::int32_t sectorIndex, std::int32_t surfaceIndex);
    void setSelectedThing(std::int32_t thingIndex);
    void setSelectedLight(std::int32_t lightIndex);
    std::optional<SurfaceRef> selectedSurfaceRef() const noexcept
    {
        if (selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0)
        {
            return SurfaceRef{ selectedSurfaceSector_, selectedSurface_ };
        }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<SurfaceRef>
    activeSurfaceEdgeOwner() const noexcept
    {
        return activeSurfaceEdge_
            ? std::optional<SurfaceRef>{ activeSurfaceEdge_->surface }
            : std::nullopt;
    }

    // Thing-frame/path editing support.
    void setFrameEditEnabled(bool enabled);
    bool frameEditEnabled() const noexcept { return frameEditEnabled_; }
    void clearFrameSelection();
    void setSelectedFrameValueIndex(std::int32_t valueIndex);
    std::int32_t activeSectorIndex() const noexcept { return selectedSector_; }
    std::int32_t activeSurfaceSectorIndex() const noexcept { return selectedSurfaceSector_; }
    std::int32_t activeSurfaceIndex() const noexcept { return selectedSurface_; }
    std::int32_t activeThingIndex() const noexcept { return selectedThing_; }
    std::int32_t activeLightIndex() const noexcept { return selectedLight_; }
    // The frame editor keeps the primary frame value separately from the
    // unordered Selection::frames set. Recovery needs this exact value to
    // round-trip the active frame rather than choosing an arbitrary set item.
    std::int32_t activeFrameValueIndex() const noexcept
    {
        return selectedFrameValueIndex_;
    }
    void fitViewToLevel();
    bool fitSpritePreviewToLevel(bool preserveView = false);
    void frameWorldBounds(const QVector3D& min, const QVector3D& max, bool animate = true, int durationMs = 280);
    void setShowWireframe(bool enabled);
    void setShowVertices(bool enabled);
    void setShowSolid(bool enabled);
    void setShowTextures(bool enabled);
    void setShowFog(bool enabled);
    void setDefaultShowFog(RenderPipeline pipeline, bool enabled);
    void setShowThings(bool enabled);
    void setShowHiddenThings(bool enabled);
    void setShowThingCollisionShapes(bool enabled);
    void setMotsTreeCollisionPreviewMeshRadiusFactor(float factor);
    void setShowThingVisualBounds(bool enabled);
    void setDefaultShowThingCollisionShapes(RenderPipeline pipeline, bool enabled);
    void setDefaultShowThingVisualBounds(RenderPipeline pipeline, bool enabled);
    // Private preview-only guide for IJIM's authored collwidth/collheight values.
    // It is not persisted or exposed through viewport settings.
    void setShowTemplateCollisionBoundsGuide(bool enabled);
    void setShowAxisWidget(bool enabled);
    void setShowToastOverlays(bool enabled);
    void setShowGrid(bool enabled);
    void setGridFollowCamera(bool enabled);
    void setGridRotateWithView(bool enabled);
    void setGridKeepViewAlignedVisible(bool enabled);
    void setGridPlane(GridPlane plane);
    void setGridMajorSpacing(float spacing);
    void setGridMinorSpacing(float spacing);
    void setGridAdaptiveSpacing(bool enabled);
    void setGridOpacity(float opacity);
    void setSurfaceExtrudePreviewOpacity(float opacity);
    void setGridColor(const QColor& color);
    void setGridMajorColor(const QColor& color);
    void setGridAxisXColor(const QColor& color);
    void setGridAxisYColor(const QColor& color);
    void setGridAxisZColor(const QColor& color);
    void setGridLineWidthPx(float widthPx);
    void setGridMinorDots(bool enabled);
    void setGridMinorDotSizePx(float sizePx);
    void setGridVisibleThroughWalls(bool enabled);
    void setGridAlwaysBehind(bool enabled);
    void setGridResetOnCameraRotate(bool enabled);
    void alignViewToGridPlane();
    void alignGridPlaneToView();
    void snapGridToObject();
    void orientGridToSelection();
    void centerGridOnSelection();
    void clearGridCenterOverride();
    void resetGridOverrides();
    QVector3D gridTransformOrigin() const noexcept;
    render::GridTransformAngles gridTransformAngles() const noexcept;
    void setGridTransformOrigin(const QVector3D& origin);
    void setGridTransformAngles(const render::GridTransformAngles& angles);
    void setGridTransformPlane(GridPlane plane);

    void setSectorCleaveToolEnabled(bool enabled);
    bool sectorCleaveToolEnabled() const noexcept { return sectorCleaveToolEnabled_; }
    void setCleaveStagedMode(bool enabled);
    bool cleaveStagedMode() const noexcept { return cleaveStagedMode_; }
    bool cleaveStagedEndpointEditing() const noexcept
    {
        return editingStagedCleaveLine_ >= 0 && editingStagedCleaveEndpoint_ >= 0;
    }
    void setCleaveChainEnabled(bool enabled);
    bool cleaveChainEnabled() const noexcept { return cleaveChainEnabled_; }
    void setCleaveBounded(bool enabled);
    bool cleaveBounded() const noexcept { return cleaveBounded_; }
    void setCleaveMeasurementsVisible(bool enabled);
    bool cleaveMeasurementsVisible() const noexcept { return cleaveMeasurementsVisible_; }
    void setCleaveMeasureUnit(CleaveMeasureUnit unit);
    CleaveMeasureUnit cleaveMeasureUnit() const noexcept { return cleaveMeasureUnit_; }
    void setCleaveAnglesVisible(bool enabled);
    bool cleaveAnglesVisible() const noexcept { return cleaveAnglesVisible_; }
    void setCleaveLengthsVisible(bool enabled);
    bool cleaveLengthsVisible() const noexcept { return cleaveLengthsVisible_; }
    void setCleaveLineNumbersVisible(bool enabled);
    bool cleaveLineNumbersVisible() const noexcept { return cleaveLineNumbersVisible_; }
    void setCleaveAngleStepDegrees(double stepDegrees);
    double cleaveAngleStepDegrees() const noexcept { return cleaveAngleStepDegrees_; }
    void refreshCleaveSnapPreviewForModifiers(Qt::KeyboardModifiers modifiers);
    std::size_t stagedCleaveLineCount() const noexcept { return stagedCleaveLines_.size(); }
    bool stagedCleaveHistoryOwnsUndo() const noexcept;
    [[nodiscard]] bool isStagedCleaveInteractionActive() const noexcept;
    bool canUndoStagedCleaveHistory() const;
    bool canRedoStagedCleaveHistory() const;
    bool undoStagedCleaveHistory();
    bool redoStagedCleaveHistory();
    bool hasStagedCleaveDraft() const noexcept;
    void requestApplyStagedCleave();
    void cancelStagedCleave();
    bool cancelActiveStagedCleaveInteraction();
    void clearStagedCleaveDraft();
    bool deleteSelectedStagedCleaveLine();
    [[nodiscard]] StagedCleaveTransformMode stagedCleaveTransformMode() const noexcept;
    void setStagedCleaveTransformMode(StagedCleaveTransformMode mode);
    [[nodiscard]] bool canTransformSelectedStagedCleave(
        StagedCleaveTransformMode mode) const noexcept;
    [[nodiscard]] const RecoveryState::CleaveShapeOptions& cleaveShapeOptions() const noexcept
    {
        return cleaveShapeOptions_;
    }
    void setCleaveShapeOptions(const RecoveryState::CleaveShapeOptions& options);
    bool setStagedCleaveShapePlan(const ui::viewport::CleaveShapePlan& plan);
    [[nodiscard]] RecoveryState::CleaveDraftSnapshot captureStagedCleaveDraftState() const;
    [[nodiscard]] std::optional<RecoveryState::CleaveHistory>
    captureStagedCleaveHistory() const;
    void restoreStagedCleaveDraftState(
        const RecoveryState::CleaveDraftSnapshot& snapshot,
        bool restoreSelection = true);
    void restoreStagedCleaveDraftState(
        const RecoveryState::CleaveDraftSnapshot& snapshot,
        const RecoveryState::CleaveHistory& history,
        bool restoreSelection = true);
    void restoreStagedCleaveDraftState(
        RecoveryState::CleaveDraftSnapshot&& snapshot,
        RecoveryState::CleaveHistory&& history,
        bool restoreSelection = true);
    [[nodiscard]] std::uint64_t stagedCleaveAuthoredRevision() const noexcept;
    [[nodiscard]] std::uint64_t stagedCleaveAuthoredRevision(
        CleaveWorkspaceFamily family) const noexcept;
    [[nodiscard]] CleaveWorkspaceFamily cleaveWorkspaceFamily() const noexcept
    {
        return cleaveWorkspaceFamily_;
    }
    [[nodiscard]] RecoveryState::CleaveWorkspace captureStagedCleaveWorkspace(
        CleaveWorkspaceFamily family) const;
    // Borrowed views remain valid only until the next workspace mutation.
    [[nodiscard]] RecoveryState::CleaveDraftView stagedCleaveDraftView(
        CleaveWorkspaceFamily family) const noexcept;
    [[nodiscard]] bool cleaveWorkspaceBounded(CleaveWorkspaceFamily family) const noexcept;
    bool restoreStagedCleaveWorkspace(
        CleaveWorkspaceFamily family,
        RecoveryState::CleaveWorkspace&& workspace,
        QString* error = nullptr);
    void cancelStagedCleaveWorkspace(CleaveWorkspaceFamily family);
    void cancelAllStagedCleaveWorkspaces();
    void resetStagedCleaveWorkspaces();
    [[nodiscard]] static RecoveryState::CleaveWorkspace defaultStagedCleaveWorkspaceOptions(
        const Level* level = nullptr);
    void setStagedCleaveTransformShortcutBindings(
        QList<QKeySequence> move,
        QList<QKeySequence> rotate,
        QList<QKeySequence> scale);
    [[nodiscard]] std::optional<StagedCleaveTransformMode> stagedCleaveTransformShortcutForKey(
        int key, Qt::KeyboardModifiers modifiers) const;
    [[nodiscard]] static std::size_t stagedCleaveWorkspaceOwnedBytesEstimate(
        const RecoveryState::CleaveWorkspace& workspace) noexcept;
    [[nodiscard]] std::size_t stagedCleaveWorkspaceOwnedBytesEstimate(
        CleaveWorkspaceFamily family) const noexcept;
    [[nodiscard]] std::size_t stagedCleaveHistoryStorageBytesEstimate() const noexcept;
    [[nodiscard]] std::size_t stagedCleaveRecoveryWorkspaceOwnedBytesEstimate() const noexcept;
    [[nodiscard]] QString stagedCleaveTransformHudText() const;

    void setSectorThrustToolEnabled(bool enabled);
    bool sectorThrustToolEnabled() const noexcept { return sectorThrustToolEnabled_; }
    void setShowAllSectorThrustGizmos(bool enabled);
    bool showAllSectorThrustGizmos() const noexcept { return showAllSectorThrustGizmos_; }
    void setShowSectorThrustVectors(bool enabled);
    bool showSectorThrustVectors() const noexcept { return showSectorThrustVectors_; }
    const std::unordered_set<std::int32_t>& selectedSectorThrusts() const noexcept
    {
        return selectedSectorThrusts_;
    }

    void setSurfaceCleaveToolEnabled(bool enabled);
    bool surfaceCleaveToolEnabled() const noexcept { return surfaceCleaveToolEnabled_; }

    void setShowThingGizmos(bool enabled);
    void setEditorDefaultShowThingGizmos(bool enabled);
    void setShowArchThingGizmos(bool enabled);
    void setEditorDefaultShowArchThingGizmos(bool enabled);
    void setPreviewDefaultShowArchThingGizmos(bool enabled);
    void setThingIconCubeTintColor(const QColor& color);
    void setBackgroundClearColor(const QColor& color);
    void setWireframeColor(const QColor& color);
    void setWireframeLineWidthPx(float widthPx);
    void setWireframeAntialiasingEnabled(bool enabled);
    void setFramePathLineWorldSize(float size);
    void setFramePathPointWorldSize(float size);
    void setSelectionPrimaryColor(const QColor& color);
    void setSelectionMultiColor(const QColor& color);
    void setSelectionEdgeMultiColor(const QColor& color);
    void setSelectionActiveColor(const QColor& color);
    void setSelectionVertexColor(const QColor& color);
    void setSelectionVertexMultiColor(const QColor& color);
    void setCogEntityPickPrimaryColor(const QColor& color);
    void setCogEntityPickActiveColor(const QColor& color);
    void setCogEntityPickHighlightEnabled(bool enabled);
    void setActiveSurfaceVertexHint(std::int32_t sectorIndex, std::int32_t surfaceIndex, std::int32_t localVertexIndex);
    void clearActiveSurfaceVertexHint();
    [[nodiscard]] std::optional<editor::SurfaceVertexRef>
    activeSurfaceVertex() const noexcept
    {
        return activeSurfaceVertexHintValid_
            ? std::optional<editor::SurfaceVertexRef>{ activeSurfaceVertexHint_ }
            : std::nullopt;
    }
    void setThingCubeHalfSize(float halfSize);
    void setThingGizmoWorldSize(float size);
    void setThingGizmoHeightAboveModel(float height);
    void setThingGizmoStackSpacing(float spacing);
    void setThingLightGizmoOffsetAboveModel(float offset);
    void setArchThingGizmoOffsetAboveModel(float offset);
    void setLevelGeometryVertexMarkerSizePx(float sizePx);
    void setLightGizmoWorldSize(float size);
    void setLightGizmoUseLightColor(bool enabled);
    void setArchitecturalLightingMarkerSizePx(float sizePx);
    void setArchitecturalLightingMarkerUseAuthoredColor(bool enabled);
    void setArchitecturalLightingMarkerColor(const QColor& color);
    void setLightGizmoCategory(render::LightGizmoCategory category);
    void setLightGizmoEditingActive(bool enabled);
    void setArchitecturalLightingEditingActive(bool enabled);
    void setArchitecturalLightingAuthoringMode(
        level::MotsArchLightingVertexMode mode);
    [[nodiscard]] level::MotsArchLightingVertexMode
    architecturalLightingAuthoringMode() const noexcept
    {
        return architecturalLightingAuthoringMode_;
    }
    void setArchitecturalLightingTargetMode(ArchitecturalLightingTargetMode mode);
    [[nodiscard]] ArchitecturalLightingTargetMode
    architecturalLightingTargetMode() const noexcept
    {
        return architecturalLightingTargetMode_;
    }
    void setArchitecturalLightingSelectedVertices(
        std::span<const ArchitecturalLightingVertexIdentity> vertices);
    void clearArchitecturalLightingSelectedVertices();
    [[nodiscard]] std::span<const ArchitecturalLightingVertexIdentity>
    architecturalLightingSelectedVertices() const noexcept
    {
        return architecturalLightingSelectedVertices_;
    }
    void setArchitecturalLightingPreview(
        std::span<const ArchitecturalLightingPreviewVertex> vertices);
    void setArchitecturalLightingPreview(
        std::span<const ArchitecturalLightingPreviewVertex> vertices,
        level::MotsArchLightingVertexMode mode);
    void clearArchitecturalLightingPreview(
        bool restoreAuthoringPresentation = true) noexcept;
    [[nodiscard]] bool architecturalLightingPreviewActive() const noexcept
    {
        return !architecturalLightingPreview_.empty();
    }
    bool architecturalLightingEditingActive() const noexcept
    {
        return architecturalLightingEditingActive_;
    }
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    /** Effective owner-gizmo visibility seam for the interactive ARCH mode. */
    bool architecturalLightingOwnerGizmosVisibleForTest() const noexcept
    {
        return (renderPipeline_ == RenderPipeline::Editor
                && architecturalLightingEditingActive_)
            || (level_ && level_->kind == ProjectType::MOTS
                && showArchThingGizmos_);
    }
    [[nodiscard]] std::size_t architecturalLightingOwnerGizmoEntryCountForTest() const noexcept
    {
        return architecturalLightingOwnerGizmoEntries_.size();
    }
    [[nodiscard]] std::optional<std::int32_t>
    architecturalLightingLastOwnerClickForTest() const noexcept
    {
        return architecturalLightingLastOwnerClick_;
    }
    [[nodiscard]] int architecturalLightingPointCountForTest() const noexcept
    {
        return architecturalLightingPointCount_;
    }
    [[nodiscard]] level::MotsArchLightingVertexMode
    architecturalLightingDisplayModeForTest() const noexcept
    {
        return renderPipeline_ == RenderPipeline::Editor
                && architecturalLightingEditingActive_
            ? architecturalLightingAuthoringMode_
            : level::MotsArchLightingVertexMode::Rgb;
    }
    [[nodiscard]] std::optional<std::array<float, 3>>
    architecturalLightingPointColorForTest(
        const ArchitecturalLightingVertexIdentity& identity) const noexcept;
#endif
    void setAlwaysShowLightGizmoCategory(render::LightGizmoCategory category, bool enabled);
    void setShowEditorCubes(bool enabled);
    void setShowPreviewCubes(bool enabled);
    void setPreviewDefaultShowThingGizmos(bool enabled);
    void setThingTypeColors(const std::unordered_map<std::string, QColor>& colors);
    void setLayerColors(const std::vector<QColor>& colors);
    void setLayerWireframeColoringEnabled(bool enabled);
    void setThingViewMode(ThingViewMode mode);
    void setThingModelLodEnabled(bool enabled);
    void setDefaultThingModelLodEnabled(RenderPipeline pipeline, bool enabled);
    void setLightingMode(LightingMode mode);
    void setDynamicLightShading(DynamicLightShading shading);
    void setThingLightRenderingEnabled(bool enabled);
    void setSectorPointLightRenderingEnabled(bool enabled);
    enum class LayerMeshPublication : std::uint8_t
    {
        rebuild,
        defer
    };
    using LayerMeshProgressPulse = std::function<void()>;
    void setLayerVisibilityState(std::unordered_set<std::int32_t> hiddenLayers, bool previewRespectHiddenLayers,
                                 LayerMeshPublication publication = LayerMeshPublication::rebuild,
                                 LayerMeshProgressPulse progressPulse = {});
    void setHiddenLayers(std::unordered_set<std::int32_t> hiddenLayers);
    void setLockedLayers(std::unordered_set<std::int32_t> lockedLayers);
    void setPreviewRespectHiddenLayers(bool enabled);
    void setGeometryTransformSnapEnabled(bool enabled);
    void setGeometryTransformRotateToNearbyEnabled(bool enabled);
    void setPreserveConnectedSurfaceUvsEnabled(bool enabled);
    void reportConnectedSurfaceUvFailure(const QString& detail = {});
    void setGeometryTransformSnapInvertModifierBinding(Qt::KeyboardModifiers toggle);
    void setGeometryTransformSnapInvertModifierBinding(
        const ui::shortcuts::ModifierBindings& toggles);
    void setTransformGridSnapEnabled(bool enabled);
    void setTransformGridSnapInvertModifierBinding(Qt::KeyboardModifiers toggle);
    void setTransformGridSnapInvertModifierBinding(
        const ui::shortcuts::ModifierBindings& toggles);
    void setTransformGridStickInvertModifierBinding(Qt::KeyboardModifiers toggle);
    void setTransformGridStickInvertModifierBinding(
        const ui::shortcuts::ModifierBindings& toggles);
    void setTransformGridPlaneStickEnabled(bool enabled);
    void setSectorMoveSnapAlignEnabled(bool enabled);
    void setSectorMoveSnapAlignInvertModifierBinding(Qt::KeyboardModifiers toggle);
    void setSectorMoveSnapAlignInvertModifierBinding(
        const ui::shortcuts::ModifierBindings& toggles);
    void setThingMoveSurfaceStickEnabled(bool enabled);
    void setThingMoveSurfaceAlignEnabled(bool enabled);
    void setThingMoveSurfaceStickInvertModifierBinding(Qt::KeyboardModifiers toggle);
    void setThingMoveSurfaceStickInvertModifierBinding(
        const ui::shortcuts::ModifierBindings& toggles);
    void setThingMoveSurfaceAlignInvertModifierBinding(Qt::KeyboardModifiers toggle);
    void setThingMoveSurfaceAlignInvertModifierBinding(
        const ui::shortcuts::ModifierBindings& toggles);
    void setThingMoveSnapToThingEnabled(bool enabled);
    void setThingMoveSnapInvertModifierBinding(Qt::KeyboardModifiers toggle);
    void setThingMoveSnapInvertModifierBinding(
        const ui::shortcuts::ModifierBindings& toggles);
    void setThingMoveSnapRotateEnabled(bool enabled);
    void setThingMoveSnapRotateInvertModifierBinding(Qt::KeyboardModifiers toggle);
    void setThingMoveSnapRotateInvertModifierBinding(
        const ui::shortcuts::ModifierBindings& toggles);
    void refreshTransformDragForModifiers(Qt::KeyboardModifiers modifiers);
    bool isLayerVisible(std::int32_t layer) const noexcept;
    bool isLayerLocked(std::int32_t layer) const noexcept;
    void setRenderPipeline(RenderPipeline pipeline);
    [[nodiscard]] RecoveryState recoveryState() const;
    [[nodiscard]] bool canRestoreRecoveryState(
        const RecoveryState& state,
        QString* error = nullptr) const;
    bool restoreRecoveryState(
        const RecoveryState& state,
        bool rebuildRenderCache = true,
        QString* error = nullptr);
    void resetPreviewViewportState();
    void resetEditorTopDownView();
    void resetFlySpeedMultiplier() noexcept;
    void setTransformTool(TransformTool tool);
    TransformTool transformTool() const noexcept { return transformTool_; }
    void setTransformInteractionEnabled(bool enabled);
    bool transformInteractionEnabled() const noexcept { return transformInteractionEnabled_; }
    /** Whether the active transform preview semantically moves selected Thing owners.
     * Headlight Move is intentionally false: it edits only `lightoffset`.
     */
    bool transformPreviewMovesThingOwner() const noexcept;
    void setTransformOrientation(TransformOrientation orientation);
    TransformOrientation transformOrientation() const noexcept { return transformOrientation_; }

    bool showWireframe() const noexcept { return showWireframe_; }
    bool showVertices() const noexcept { return showVertices_; }
    bool showSolid() const noexcept { return showSolid_; }
    bool showTextures() const noexcept { return showTextures_; }
    bool showFog() const noexcept { return showFog_; }
    bool editorDefaultShowFog() const noexcept { return editorDefaultShowFog_; }
    bool showThings() const noexcept { return showThings_; }
    bool showHiddenThings() const noexcept { return showHiddenThings_; }
    bool showThingCollisionShapes() const noexcept { return showThingCollisionShapes_; }
    [[nodiscard]] float motsTreeCollisionPreviewMeshRadiusFactor() const noexcept
    {
        return motsTreeCollisionPreviewMeshRadiusFactor_;
    }
    bool showThingVisualBounds() const noexcept { return showThingVisualBounds_; }
    bool editorDefaultShowThingCollisionShapes() const noexcept
    {
        return editorDefaultShowThingCollisionShapes_;
    }
    bool previewDefaultShowThingCollisionShapes() const noexcept
    {
        return previewDefaultShowThingCollisionShapes_;
    }
    bool editorDefaultShowThingVisualBounds() const noexcept
    {
        return editorDefaultShowThingVisualBounds_;
    }
    bool previewDefaultShowThingVisualBounds() const noexcept
    {
        return previewDefaultShowThingVisualBounds_;
    }
    bool showTemplateCollisionBoundsGuide() const noexcept
    {
        return showTemplateCollisionBoundsGuide_;
    }
    bool showGrid() const noexcept { return showGrid_; }
    bool gridFollowCamera() const noexcept { return gridFollowCamera_; }
    bool gridRotateWithView() const noexcept { return gridRotateWithView_; }
    bool gridKeepViewAlignedVisible() const noexcept { return gridKeepViewAlignedVisible_; }
    GridPlane gridPlane() const noexcept { return gridPlane_; }
    float gridMajorSpacing() const noexcept { return gridMajorSpacing_; }
    float gridMinorSpacing() const noexcept { return gridMinorSpacing_; }
    bool gridAdaptiveSpacing() const noexcept { return gridAdaptiveSpacing_; }
    /** Effective minor spacing shared by the rendered grid and scalar tools. */
    double activeGridSnapSpacing();
    /** Frozen origin, axes, plane, and displayed minor spacing for one interaction. */
    [[nodiscard]] render::TransformGridFrame activeGridSnapFrame();
    float gridOpacity() const noexcept { return gridOpacity_; }
    float surfaceExtrudePreviewOpacity() const noexcept { return surfaceExtrudePreviewOpacity_; }
    QColor gridColor() const noexcept { return gridColor_; }
    QColor gridMajorColor() const noexcept { return gridMajorColor_; }
    QColor gridAxisXColor() const noexcept { return gridAxisXColor_; }
    QColor gridAxisYColor() const noexcept { return gridAxisYColor_; }
    QColor gridAxisZColor() const noexcept { return gridAxisZColor_; }
    float gridLineWidthPx() const noexcept { return gridLineWidthPx_; }
    bool gridMinorDots() const noexcept { return gridMinorDots_; }
    float gridMinorDotSizePx() const noexcept { return gridMinorDotSizePx_; }
    bool gridVisibleThroughWalls() const noexcept { return gridVisibleThroughWalls_; }
    bool gridAlwaysBehind() const noexcept { return gridAlwaysBehind_; }
    bool gridResetOnCameraRotate() const noexcept { return gridResetOnCameraRotate_; }
    bool gridCenterOverridden() const noexcept { return gridCenterOverride_.has_value(); }
    bool gridOverridesActive() const noexcept { return gridCenterOverride_.has_value() || gridAlignToView_; }
    bool showThingGizmos() const noexcept { return showThingGizmos_; }
    bool editorDefaultShowThingGizmos() const noexcept { return editorState_.showThingGizmos; }
    bool showArchThingGizmos() const noexcept { return showArchThingGizmos_; }
    bool editorDefaultShowArchThingGizmos() const noexcept
    {
        return editorState_.showArchThingGizmos;
    }
    QColor thingIconCubeTintColor() const noexcept { return thingIconCubeTintColor_; }
    QColor backgroundClearColor() const noexcept { return backgroundClearColor_; }
    QColor wireframeColor() const noexcept { return wireframeColor_; }
    float wireframeLineWidthPx() const noexcept { return wireframeLineWidthPx_; }
    bool wireframeAntialiasingEnabled() const noexcept { return wireframeAntialiasingEnabled_; }
    float framePathLineWorldSize() const noexcept { return framePathLineWorldSize_; }
    float framePathPointWorldSize() const noexcept { return framePathPointWorldSize_; }
    QColor selectionPrimaryColor() const noexcept { return selectionPrimaryColor_; }
    QColor selectionMultiColor() const noexcept { return selectionMultiColor_; }
    QColor selectionEdgeMultiColor() const noexcept { return selectionEdgeMultiColor_; }
    QColor selectionActiveColor() const noexcept { return selectionActiveColor_; }
    QColor selectionVertexColor() const noexcept { return selectionVertexColor_; }
    QColor selectionVertexMultiColor() const noexcept { return selectionVertexMultiColor_; }
    QColor cogEntityPickPrimaryColor() const noexcept { return cogEntityPickPrimaryColor_; }
    QColor cogEntityPickActiveColor() const noexcept { return cogEntityPickActiveColor_; }
    bool layerWireframeColoringEnabled() const noexcept { return layerWireframeColoringEnabled_; }
    bool cogEntityPickHighlightEnabled() const noexcept { return cogEntityPickHighlightEnabled_; }
    float thingCubeHalfSize() const noexcept { return thingCubeHalfSize_; }
    float thingGizmoWorldSize() const noexcept { return thingGizmoWorldSize_; }
    float thingGizmoHeightAboveModel() const noexcept
    {
        return thingGizmoHeightAboveModel_;
    }
    float thingGizmoStackSpacing() const noexcept
    {
        return thingGizmoStackSpacing_;
    }
    float thingLightGizmoOffsetAboveModel() const noexcept
    {
        return thingLightGizmoOffsetAboveModel_;
    }
    float archThingGizmoOffsetAboveModel() const noexcept
    {
        return archThingGizmoOffsetAboveModel_;
    }
    float levelGeometryVertexMarkerSizePx() const noexcept
    {
        return levelGeometryVertexMarkerSizePx_;
    }
    float lightGizmoWorldSize() const noexcept { return lightGizmoWorldSize_; }
    bool lightGizmoUseLightColor() const noexcept { return lightGizmoUseLightColor_; }
    float architecturalLightingMarkerSizePx() const noexcept
    {
        return architecturalLightingMarkerSizePx_;
    }
    float architecturalLightingMarkerHitRadiusPx() const noexcept
    {
        return std::max(2.0f, architecturalLightingMarkerSizePx_ * 0.75f);
    }
    bool architecturalLightingMarkerUseAuthoredColor() const noexcept
    {
        return architecturalLightingMarkerUseAuthoredColor_;
    }
    QColor architecturalLightingMarkerColor() const noexcept
    {
        return architecturalLightingMarkerColor_;
    }
    render::LightGizmoCategory lightGizmoCategory() const noexcept { return lightGizmoCategory_; }
    bool lightGizmoEditingActive() const noexcept { return lightGizmoEditingActive_; }
    bool alwaysShowLightGizmoCategory(render::LightGizmoCategory category) const noexcept
    {
        return alwaysShowLightGizmoCategories_[static_cast<std::size_t>(category)];
    }
    bool showEditorCubes() const noexcept { return showEditorCubes_; }
    bool showPreviewCubes() const noexcept { return showPreviewCubes_; }
    bool previewDefaultShowThingGizmos() const noexcept { return previewDefaultShowThingGizmos_; }
    bool previewDefaultShowArchThingGizmos() const noexcept
    {
        return previewDefaultShowArchThingGizmos_;
    }
    ThingViewMode thingViewMode() const noexcept { return thingViewMode_; }
    bool thingModelLodEnabled() const noexcept { return thingModelLodEnabled_; }
    LightingMode lightingMode() const noexcept { return lightingMode_; }
    DynamicLightShading dynamicLightShading() const noexcept { return dynamicLightShading_; }
    bool thingLightRenderingEnabled() const noexcept
    {
        return thingLightRenderingEnabled_;
    }
    bool sectorPointLightRenderingEnabled() const noexcept
    {
        return sectorPointLightRenderingEnabled_;
    }
    bool previewRespectHiddenLayers() const noexcept { return previewRespectHiddenLayers_; }
    bool geometryTransformSnapEnabled() const noexcept { return geometryTransformSnapEnabled_; }
    bool geometryTransformRotateToNearbyEnabled() const noexcept
    {
        return geometryTransformRotateToNearbyEnabled_;
    }
    bool preserveConnectedSurfaceUvsEnabled() const noexcept
    {
        return preserveConnectedSurfaceUvsEnabled_;
    }
    bool transformGridSnapEnabled() const noexcept { return transformGridSnapEnabled_; }
    bool transformGridPlaneStickEnabled() const noexcept { return transformGridPlaneStickEnabled_; }
    bool sectorMoveSnapAlignEnabled() const noexcept { return sectorMoveSnapAlignEnabled_; }
    float vertexSnapRadius() const noexcept { return vertexSnapRadius_; }
    void setVertexSnapRadius(const float value) noexcept
    {
        vertexSnapRadius_ = std::isfinite(value)
            ? std::clamp(value, 0.000001f, 1000.0f) : 0.20f;
    }
    float edgeSnapRadius() const noexcept { return edgeSnapRadius_; }
    void setEdgeSnapRadius(const float value) noexcept
    {
        edgeSnapRadius_ = std::isfinite(value)
            ? std::clamp(value, 0.000001f, 1000.0f) : 0.20f;
    }
    float faceSnapRadius() const noexcept { return faceSnapRadius_; }
    void setFaceSnapRadius(const float value) noexcept
    {
        faceSnapRadius_ = std::isfinite(value)
            ? std::clamp(value, 0.000001f, 1000.0f) : 2.0f;
    }
    float sectorSnapRadius() const noexcept { return sectorSnapRadius_; }
    void setSectorSnapRadius(const float value) noexcept
    {
        sectorSnapRadius_ = std::isfinite(value)
            ? std::clamp(value, 0.000001f, 1000.0f) : 2.0f;
    }
    float thingSnapRadius() const noexcept { return thingSnapRadius_; }
    void setThingSnapRadius(const float value) noexcept
    {
        thingSnapRadius_ = std::isfinite(value)
            ? std::clamp(value, 0.000001f, 1000.0f) : 0.16f;
    }
    float snapRotationMaxDegrees() const noexcept
    {
        return snapRotationMaxDegrees_;
    }
    void setSnapRotationMaxDegrees(const float value) noexcept
    {
        snapRotationMaxDegrees_ = std::isfinite(value)
            ? std::clamp(value, 0.0f, 180.0f) : 180.0f;
    }
    float surfaceStickRadius() const noexcept { return surfaceStickRadius_; }
    void setSurfaceStickRadius(const float value) noexcept
    {
        surfaceStickRadius_ = std::isfinite(value)
            ? std::clamp(value, 0.000001f, 1000.0f) : 0.22f;
    }
    float gridPlaneStickRadius() const noexcept { return gridPlaneStickRadius_; }
    void setGridPlaneStickRadius(const float value) noexcept
    {
        gridPlaneStickRadius_ = std::isfinite(value)
            ? std::clamp(value, 0.000001f, 1000.0f) : 0.08f;
    }
    RenderPipeline renderPipeline() const noexcept { return renderPipeline_; }
    PickMode pickMode() const noexcept { return pickMode_; }

    void setViewportPickingEnabled(bool enabled);
    bool viewportPickingEnabled() const noexcept { return viewportPickingEnabled_; }
    const ViewportNavigationSettings& viewportNavigationSettings() const noexcept
    {
        return viewportNavigationSettings_;
    }
    void setViewportNavigationSettings(const ViewportNavigationSettings& settings);
    [[nodiscard]] const ViewportBannerSettings& viewportBannerSettings() const noexcept
    {
        return viewportBannerSettings_;
    }
    [[nodiscard]] bool setViewportBannerSettings(
        const ViewportBannerSettings& settings);
    void cancelViewportMouseGesture();
    Qt::MouseButton viewportMouseGestureButton() const noexcept
    {
        return viewportMouseGesture_.button;
    }
    void setPlacementGestureEnabled(bool enabled) noexcept;
    bool placementGestureEnabled() const noexcept { return placementGestureEnabled_; }
    void setHoverPickEnabled(bool enabled);
    void setPreviewInteractionEnabled(bool enabled);
    bool previewInteractionEnabled() const noexcept { return previewInteractionEnabled_; }
    void setSelectionModifierBindings(
        Qt::KeyboardModifiers addModifiers,
        Qt::KeyboardModifiers removeModifiers,
        Qt::KeyboardModifiers surfaceBypassFiltersModifiers);
    void setSelectionModifierBindings(
        const ui::shortcuts::ModifierBindings& addModifiers,
        const ui::shortcuts::ModifierBindings& removeModifiers,
        const ui::shortcuts::ModifierBindings& surfaceBypassFiltersModifiers);
    Qt::KeyboardModifiers selectionAddModifiers() const noexcept
    {
        return selectionAddModifiers_.isEmpty()
            ? Qt::NoModifier
            : selectionAddModifiers_.front();
    }
    Qt::KeyboardModifiers selectionRemoveModifiers() const noexcept
    {
        return selectionRemoveModifiers_.isEmpty()
            ? Qt::NoModifier
            : selectionRemoveModifiers_.front();
    }
    Qt::KeyboardModifiers selectionSurfaceBypassFiltersModifiers() const noexcept
    {
        return selectionSurfaceBypassFiltersModifiers_.isEmpty()
            ? Qt::NoModifier
            : selectionSurfaceBypassFiltersModifiers_.front();
    }
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    const ui::shortcuts::ModifierBindings& selectionAddModifierBindingsForTest() const noexcept
    {
        return selectionAddModifiers_;
    }
    const ui::shortcuts::ModifierBindings& selectionRemoveModifierBindingsForTest() const noexcept
    {
        return selectionRemoveModifiers_;
    }
    const ui::shortcuts::ModifierBindings& selectionSurfaceBypassModifierBindingsForTest() const noexcept
    {
        return selectionSurfaceBypassFiltersModifiers_;
    }
    const ui::shortcuts::ModifierBindings& transformGridSnapModifierBindingsForTest() const noexcept
    {
        return transformGridSnapInvertModifiers_;
    }
#endif

    // Controls what `surfaceClicked` emits when a surface hit exists.
    // - true  (default): prefer the surface hit position (placement on geometry)
    // - false: prefer a point on the click ray near the camera (placement in air)
    void setSurfaceClickPreferSurfaceHit(bool enabled) noexcept;
    bool surfaceClickPreferSurfaceHit() const noexcept { return surfaceClickPreferSurfaceHit_; }
    void setSurfaceClickSelectionEnabled(bool enabled) noexcept;
    bool surfaceClickSelectionEnabled() const noexcept { return surfaceClickSelectionEnabled_; }
    void setXraySelectionCyclingEnabled(bool enabled) noexcept;
    bool xraySelectionCyclingEnabled() const noexcept { return xraySelectionCyclingEnabled_; }
    void setOrbitAroundSelectionEnabled(bool enabled) noexcept;
    bool orbitAroundSelectionEnabled() const noexcept { return orbitAroundSelectionEnabled_; }

    void setPickMode(PickMode mode);
    void setPickPreference(PickPreference preference);
    SurfaceSubMode surfaceSubMode() const noexcept;
    void setSurfaceSubMode(SurfaceSubMode mode);
    /** Returns the existing transformed vertices for one selected Thing. */
    [[nodiscard]] std::span<const ArchitecturalLightingRenderVertex>
    architecturalLightingVerticesForThing(std::int32_t thingIndex) const noexcept;
    void setStrictSurfaceSelectionScope(
        bool enabled,
        const std::unordered_set<SurfaceRef, editor::SurfaceRefHash,
            editor::SurfaceRefEq>& surfaces);
    bool strictSurfaceSelectionEnabled() const noexcept
    {
        return strictSurfaceSelectionEnabled_;
    }
    const auto& strictSurfaceSelectionScope() const noexcept
    {
        return strictSurfaceSelectionScope_;
    }
    [[nodiscard]] std::unordered_set<SurfaceRef, editor::SurfaceRefHash,
        editor::SurfaceRefEq> selectedSurfaceEdgeOwners() const;
    void setShowSelectedSurfaceNormals(bool enabled);
    bool showSelectedSurfaceNormals() const noexcept
    {
        return showSelectedSurfaceNormals_;
    }
    void setSurfaceNormalLengthScale(float scale);
    float surfaceNormalLengthScale() const noexcept
    {
        return surfaceNormalLengthScale_;
    }
    void setSurfaceNormalArrowheadScale(float scale);
    float surfaceNormalArrowheadScale() const noexcept
    {
        return surfaceNormalArrowheadScale_;
    }
    void setShowSurfaceNormalArrowhead(bool enabled);
    bool showSurfaceNormalArrowhead() const noexcept
    {
        return showSurfaceNormalArrowhead_;
    }

    app::ViewportCamera::ProjectionType cameraProjectionType() const noexcept;
    void setCameraProjectionType(app::ViewportCamera::ProjectionType type);

    float cameraOrthoScale() const noexcept;
    void setCameraOrthoScale(float scale);
    float cameraOrbitDistance() const noexcept;
    void setCameraOrbitDistance(float distance);
    /** Projection-aware camera zoom; independent of wheel binding preferences. */
    void zoomCameraBySteps(float steps);
    float cameraFovXDeg() const noexcept;
    void setCameraFovXDeg(float fovXDeg);
    float cameraNearPlane() const noexcept;
    float cameraFarPlane() const noexcept;
    void setCameraNearFar(float nearPlane, float farPlane);
    void tightenOrthoClipToWorldBounds(const QVector3D& min, const QVector3D& max, float paddingScale = 1.08f);
    bool cameraClipEnabled() const noexcept;
    void setCameraClipEnabled(bool enabled);
    QVector3D cameraEye() const noexcept;
    QVector3D cameraTarget() const noexcept;
    QVector3D cameraForward() const noexcept;
    QVector3D cameraRight() const noexcept;
    QVector3D cameraUp() const noexcept;
    std::optional<QPointF> projectWorldToViewport(const QVector3D& world);
    std::optional<QVector3D> viewportPointOnPlane(const QPoint& widgetPos, const QVector3D& planePoint, const QVector3D& planeNormal);
    std::optional<TVector> viewportPointOnPlane(const QPoint& widgetPos, const TVector& planePoint, const TVector& planeNormal);
    std::optional<QVector3D> placementPointForViewport(
        const QPoint& widgetPos,
        bool* haveSurfaceHit = nullptr,
        SurfaceRef* surfaceRef = nullptr,
        QVector3D* surfaceNormal = nullptr,
        bool snapToSurfaceHit = false,
        bool freePlacement = false,
        TVector* documentPosition = nullptr);
    [[nodiscard]] std::vector<sed::level::SurfacePickHit> placementSurfaceHits(
        const TVector& rayOrigin,
        const TVector& rayDirection,
        double maxDistance) const;
    float worldUnitsPerPixelAtWorld(const QVector3D& world);
    double worldUnitsPerPixelAtWorld(const TVector& world);
    void setPlacementPreviewLines(std::vector<QVector3D> lines, bool cursorGuide = false, bool invalid = false);
    void setPlacementPreviewLines(std::vector<TVector> lines, bool cursorGuide = false, bool invalid = false);
    void clearPlacementPreviewLines();
    void setPlacementPreviewThing(std::optional<Thing> thing, bool invalid = false);
    void setPlacementPreviewThings(std::vector<Thing> things, bool invalid = false);
    void clearPlacementPreviewThing();
    void setPlacementPreviewSector(
        std::optional<Sector> sector,
        bool invalid = false,
        std::optional<SurfaceRef> hiddenSurface = std::nullopt,
        std::optional<Sector> sourcePatch = std::nullopt);
    void clearPlacementPreviewSector();
    void setPlacementPreviewPersistent(bool persistent) noexcept { placementPreviewPersistent_ = persistent; }

    void orbitRotate(float yawDeltaDeg, float pitchDeltaDeg);
    void setOrbitViewImmediate(float yawDeg, float pitchDeg, float rollDeg = 0.0f);
    void orbitPanPixels(const QPoint& deltaPixels);
    void orbitViewOpposite();
    void orbitSetView(float yawDeg, float pitchDeg);
    void orbitRoll(float rollDeltaDeg);

    void orbitRotateLeft();
    void orbitRotateRight();
    void orbitRotateUp();
    void orbitRotateDown();

    void orbitPanLeft();
    void orbitPanRight();
    void orbitPanUp();
    void orbitPanDown();

    void orbitRollLeft();
    void orbitRollRight();
    void orbitRollReset();

    void orbitViewFront();
    void orbitViewBack();
    void orbitViewRight();
    void orbitViewLeft();
    void orbitViewTop();
    void orbitViewBottom();

    bool flyNavigationActive() const noexcept;
    void cancelFlyNavigation();
    void setFlyNavigationFast(bool fast) noexcept;
    void setFlyNavigationSlow(bool slow) noexcept;
    /** Persistent speed steps are admitted only while Fly owns input. */
    void adjustFlyNavigationSpeedBySteps(float steps);
    void setFlyNavigationSpeedModifierBindings(
        const QList<QKeySequence>& fast, const QList<QKeySequence>& slow) noexcept;
    void refreshFlyNavigationSpeedModifiers(Qt::KeyboardModifiers held) noexcept;
    void setFlyNavigationDirection(FlyDirection dir, bool pressed) noexcept;
    bool finishStagedCleaveWheelSession();
    bool cancelStagedCleaveWheelSession();
    bool handleStagedCleaveWheelModifierRelease(int key, bool autoRepeat);
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    void tickFlyNavigationForTest()
    {
        // Trigger the existing fixed maximum step without timing sleeps or
        // changing production navigation behavior.
        flyNav_.lastTick = std::chrono::steady_clock::now() - std::chrono::milliseconds(100);
        tickFlyNavigation();
    }
#endif

Q_SIGNALS:
    void clickPicked(std::int32_t sectorIndex, std::int32_t surfaceIndex, std::int32_t thingIndex, std::int32_t lightIndex);
    void selectionPicked(
        std::int32_t sectorIndex,
        std::int32_t surfaceIndex,
        std::int32_t thingIndex,
        std::int32_t lightIndex,
        Qt::KeyboardModifiers modifiers);
    void hoverPicked(std::int32_t sectorIndex, std::int32_t surfaceIndex, std::int32_t thingIndex, std::int32_t lightIndex);
    void selectionBoxPicked(sed::editor::Selection selection, SelectionBoxMode mode);
    void framePicked(std::int32_t thingIndex, std::int32_t valueIndex);
    /** A selected MotS ARCHLIGHTING model vertex was picked in the viewport. */
    void architecturalLightingVertexPicked(
        std::int32_t thingIndex,
        std::int32_t lodIndex,
        std::int32_t meshIndex,
        std::int32_t vertexIndex,
        Qt::KeyboardModifiers modifiers);
    /** Emitted after the renderer applies Replace/Add/Remove vertex selection. */
    void architecturalLightingVertexSelectionChanged();
    /** Emitted by a double-click; a single click only changes selection. */
    void architecturalLightingVertexEditRequested(
        std::int32_t thingIndex,
        std::int32_t lodIndex,
        std::int32_t meshIndex,
        std::int32_t vertexIndex,
        Qt::KeyboardModifiers modifiers);
    void surfaceClicked(
        const QVector3D& position,
        std::int32_t sectorIndex,
        std::int32_t surfaceIndex,
        const QPoint& widgetPos,
        Qt::KeyboardModifiers modifiers);
    void placementPreviewMouseMoved(const QPoint& widgetPos, Qt::KeyboardModifiers modifiers);
    void placementGestureStarted(
        const QVector3D& position,
        bool haveSurfaceHit,
        const QPoint& widgetPos,
        Qt::KeyboardModifiers modifiers);
    void placementGestureDragged(const QPoint& widgetPos, Qt::KeyboardModifiers modifiers);
    void placementGestureReleased(const QPoint& widgetPos, Qt::KeyboardModifiers modifiers, bool dragged);
    void placementGestureCanceled();
    void escapePressed();
    void infoMessage(const QString& message);
    void warningMessage(const QString& message);
    void errorMessage(const QString& message);
    void resourceWarningMessage(const QString& message);
    void resourceErrorMessage(const QString& message);
    /** A watched 3DO/SPR/MAT/CMP dependency changed on disk. */
    void resourceFilesChanged();
    /** A mounted GOB/GOO was replaced and must be re-indexed by its owner. */
    void resourceArchivesChanged();
    /** Emitted whenever the camera/view transform changes, including orbit drags. */
    void viewTransformChanged();
    void flyNavigationActiveChanged(bool active);
    void viewportStateChanged();
    void stagedCleaveAuthoredStateChanged(std::uint64_t revision);
    void thingsTranslateRequested(double dx, double dy, double dz);
    void thingsSnapTransformRequested(const TVector& translation, const QVector3D& axis, double angleDeg, const TVector& pivot);
    void thingsRotateRequested(const QVector3D& axis, double angleDeg, const TVector& pivot);
    void frameTranslateRequested(std::int32_t thingIndex, std::int32_t valueIndex, double dx, double dy, double dz);
    void frameSnapTransformRequested(
        std::int32_t thingIndex,
        std::int32_t valueIndex,
        const TVector& translation,
        const QVector3D& axis,
        double angleDeg,
        const TVector& pivot);
    void frameRotateRequested(std::int32_t thingIndex, std::int32_t valueIndex, const QVector3D& axis, double angleDeg, const TVector& pivot);
    void sectorsTranslateRequested(double dx, double dy, double dz);
    void sectorsSnapTransformRequested(const TVector& translation, const QVector3D& axis, double angleDeg, const TVector& pivot);
    void sectorsRotateRequested(const QVector3D& axis, double angleDeg, const TVector& pivot);
    void sectorsScaleRequested(const QVector3D& axis, double scaleFactor, const TVector& pivot);
    void sectorsCleaveRequested(const TVector& planeNormal, const TVector& pointOnPlane);
    void sectorsCleaveBatchRequested(
        const QList<TVector>& planeNormals,
        const QList<TVector>& pointsOnPlane,
        bool closedShape,
        const TVector& interiorPoint,
        const QList<TVector>& strokeSecondPoints = {},
        bool bounded = false);
    void sectorThrustVectorsRequested(
        const QList<std::int32_t>& sectorIndices,
        const QVector3D& thrust);
    void sectorThrustPrimaryChanged(std::int32_t sectorIndex);
    void surfacesCleaveRequested(const TVector& planeNormal, const TVector& pointOnPlane);
    void surfacesCleaveBatchRequested(
        const QList<TVector>& planeNormals,
        const QList<TVector>& pointsOnPlane,
        bool closedShape,
        const TVector& interiorPoint,
        const QList<TVector>& strokeSecondPoints = {},
        bool bounded = false);
    void surfaceEdgeCleaveRequested(std::int32_t sectorIndex, std::int32_t surfaceIndex, std::int32_t edgeIndex, const TVector& planeNormal, const TVector& pointOnPlane);
    void surfacesTranslateRequested(double dx, double dy, double dz);
    void surfacesSnapTransformRequested(const TVector& translation, const QVector3D& axis, double angleDeg, const TVector& pivot);
    void surfacesRotateRequested(const QVector3D& axis, double angleDeg, const TVector& pivot);
    void surfacesScaleRequested(const QVector3D& axis, double scaleFactor, const TVector& pivot);
    void surfaceEdgeTranslateRequested(std::int32_t sectorIndex, std::int32_t surfaceIndex, std::int32_t edgeIndex, double dx, double dy, double dz);
    void surfaceEdgeSnapTransformRequested(
        std::int32_t sectorIndex,
        std::int32_t surfaceIndex,
        std::int32_t edgeIndex,
        const TVector& translation,
        const QVector3D& axis,
        double angleDeg,
        const TVector& pivot);
    void surfaceVertexTranslateRequested(std::int32_t sectorIndex, std::int32_t vertexIndex, double dx, double dy, double dz);
    void lightsTranslateRequested(double dx, double dy, double dz);
    void lightsRangeScaleRequested(double scaleFactor);
    void thingLightTranslateRequested(std::int32_t kind, double dx, double dy, double dz);
    void thingLightRangeScaleRequested(std::int32_t kind, double scaleFactor);
    void sectorLightTranslateRequested(double dx, double dy, double dz);
    void sectorLightRangeScaleRequested(double scaleFactor);
    void transformPreviewCommitStarting();
    void transformPreviewChanged(
        bool active,
        TransformTool tool,
        const QVector3D& translation,
        const QVector3D& axis,
        double angleDeg,
        const QVector3D& pivot,
        double scaleFactor);

protected:
    bool event(QEvent* event) override;
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    enum class PickKind : std::uint8_t
    {
        None,
        Surface,
        Thing,
        Light
    };

    struct PickObject
    {
        PickKind kind{PickKind::None};
        std::int32_t sector{-1};
        std::int32_t surface{-1};
        std::int32_t index{-1};
    };

    enum class TriKind : std::uint8_t
    {
        Level,
        Thing
    };

    struct TriVertex
    {
        float x{};
        float y{};
        float z{};
        float u{};
        float v{};
        float r{};
        float g{};
        float b{};
        float a{};
        float pickId{};
        float pickGeo{};
        float sky{};

        // The normal editor viewport visualizes local classic Sector Tint on
        // world and actual Thing geometry even when the camera is outside.
        // Game Preview uses the separate camera-sector-wide uniform instead.
        float editorSectorTintR{ 1.0f };
        float editorSectorTintG{ 1.0f };
        float editorSectorTintB{ 1.0f };

        // Runtime-lighting payload. target is 0 for unlit helper/sprite
        // geometry, 1 for world geometry, 2 for Gouraud 3DO geometry, and 3
        // for Diffuse 3DO geometry.
        float normalX{};
        float normalY{};
        float normalZ{};
        float dynamicLightTarget{};

        // IJIM uses these two vec4s for the owning sector's point light. In
        // JK/MotS their RGB components carry signed face/surface extra light
        // and the camera ambient floor, respectively. Project-specific shader
        // branches keep the payloads disjoint without growing each vertex.
        float sectorLightX{};
        float sectorLightY{};
        float sectorLightZ{};
        float sectorLightMinRadius{};
        float sectorLightR{};
        float sectorLightG{};
        float sectorLightB{};
        float sectorLightMaxRadius{};
        float sectorLightOwner{ -1.0f };

        // Target 1 uses this vec3 for signed surface+sector extra light so the
        // shader can preserve IJIM's static+dynamic clamp before adding extra.
        // Target 3 uses it as rdLight_CalcFaceIntensity's first-face-vertex
        // anchor; all three triangle vertices carry the same value.
        float faceLightAnchorX{};
        float faceLightAnchorY{};
        float faceLightAnchorZ{};

        // Level-surface fills carry screen-linear barycentrics and a flat
        // source-polygon boundary mask. The fill shader uses these only for
        // the editor wire overlay, so opaque edge visibility follows the
        // actual filled sample and never depends on a later line-depth guess.
        float surfaceWireBarycentricX{};
        float surfaceWireBarycentricY{};
        float surfaceWireBarycentricZ{};
        // xyz marks the triangle sides which are edges of the source polygon;
        // w stores the owning layer index. All three vertices share this data.
        float surfaceWireBoundaryX{};
        float surfaceWireBoundaryY{};
        float surfaceWireBoundaryZ{};
        float surfaceWireLayer{ -1.0f };
    };

    struct TriShadeInput
    {
        ColorF intensity{};
        ColorF extraLight{};
        std::int32_t lightMode{};
        // Nonnegative source fields identify a level texture vertex whose
        // baked color can be refreshed without rebuilding geometry/material
        // batches. Thing geometry leaves these at -1 and uses the cached
        // intensity/extra-light values above.
        std::int32_t sourceSector{ -1 };
        std::int32_t sourceSurface{ -1 };
        std::int32_t sourceTexVertex{ -1 };
        // Thing-model metadata retained for the renderer-only ARCH preview.
        // The preview replaces one exact authored (mesh, vertex) identity and
        // then runs the same ambient/extra/light-mode composition as a normal
        // mesh rebuild. Level shade inputs leave these at their defaults.
        ColorF modelColor{ ColorWhite };
        ColorF ambientLight{ ColorWhite };
        std::int32_t archMeshIndex{ -1 };
        std::int32_t archVertexIndex{ -1 };
    };

    struct ArchitecturalLightingTrianglePatch
    {
        std::int32_t globalVertexIndex{ -1 };
        ColorF originalColor{};
    };

    struct ArchitecturalLightingPreviewOwnerRevision
    {
        const assets::Model3do* modelIdentity{};
        std::uint64_t archLightingRevision{};
    };

    struct LineVertex
    {
        float x{};
        float y{};
        float z{};
        float pickId{};
        // For level-surface edges, one non-collinear point on the supporting
        // surface lets the thick-line shader match that surface's projected
        // depth across the screen-space line width. Non-surface lines leave
        // planeValid at zero and use the normal fixed-bias path.
        float planeX{};
        float planeY{};
        float planeZ{};
        float planeValid{};
    };

    struct TriBatch
    {
        TriKind kind{TriKind::Level};
        std::string key{};
        std::string material{};
        std::string colormap{};
        int firstVertex{};
        int vertexCount{};
        QVector3D boundsMin{};
        QVector3D boundsMax{};
        QVector3D boundsCenter{};
        bool uvNormalized{};
        std::int32_t skyGroup{};
        bool doubleSided{};
        bool isSprite{};
        std::uint32_t faceFlags{};
        float minAlpha{ 1.0f };
    };

    enum class MeshRebuildKind : std::uint8_t
    {
        All,
        LevelOnly,
        ThingsOnly
    };

    struct TriBuild
    {
        TriKind kind{TriKind::Level};
        std::string key{};
        std::string material{};
        std::string colormap{};
        QVector3D boundsMin{};
        QVector3D boundsMax{};
        QVector3D boundsCenter{};
        bool uvNormalized{};
        std::int32_t skyGroup{};
        bool doubleSided{};
        std::uint32_t faceFlags{};
        float minAlpha{ 1.0f };
        std::vector<TriVertex> vertices{};
        // Parallel to vertices. Thing entries support fast ambient/self-light
        // recoloring; level entries additionally retain the source texture-
        // vertex indices used by incremental baked-light publication.
        std::vector<TriShadeInput> shadeInputs{};
    };

    struct MeshPartCache
    {
        std::vector<LineVertex> lineVertices{};
        std::vector<LineVertex> overlayLineVertices{};
        std::vector<float> pointPositions{};
        std::vector<TriBuild> triBuilds{};
        QVector3D min{};
        QVector3D max{};
        bool haveAny{};
        bool valid{};
    };

    // The interactive ARCHLIGHTING picker reads one selected owner's already
    // rendered model. Keep this cache narrow and immutable between rebuilds:
    // the model is held alive, while the identity/signatures reject a stale
    // span after a document or resource publication.
    struct ArchitecturalLightingRenderCache
    {
        std::shared_ptr<const assets::Model3do> model{};
        const assets::Model3do* modelIdentity{};
        std::string modelKey{};
        // Includes model/ARCH topology and every authored channel bit.
        std::uint64_t archLightingRevision{};
        std::int32_t archLightingIndex{ -1 };
        std::uint64_t thingGeometryRevision{};
        std::int32_t thingIndex{ -1 };
        std::vector<ArchitecturalLightingRenderVertex> vertices{};
    };

    // Dedicated ARCH marker payload. Keeping channel color in the point VBO
    // avoids a uniform update and draw call for every model vertex.
    struct ArchitecturalLightingPointVertex
    {
        float x{};
        float y{};
        float z{};
        float r{};
        float g{};
        float b{};
    };

    struct ThingRenderState
    {
        double x{};
        double y{};
        double z{};
        double pch{};
        double yaw{};
        double rol{};
        std::int32_t sector{-1};
        std::uint64_t signature{};
        std::uint64_t runtimeLightSignature{};
        std::size_t geosetCount{};
        std::size_t requestedLodIndex{};
        std::size_t lodIndex{};
        bool archThing{};
        std::uint8_t collisionOverlayShape{};
        double collisionOverlayRadius{};
        bool collisionOverlayHasRadius{};
        std::optional<double> collisionOverlayBroadPhaseRadius{};
    };

    enum class ThingTransformFastFallbackReason : std::uint8_t
    {
        none,
        missingLevel,
        invalidMeshCache,
        missingRenderState,
        topologyMismatch,
        pickMappingMismatch,
        geometrySignatureChanged,
        boundsMappingMismatch,
        nonFiniteThingTransform,
        nonFiniteCachedTransform,
        nonInvertibleTransform,
        boundsOwnerOutOfRange,
        triBuildMappingMismatch,
        triBuildRangeInvalid,
        lineRangeInvalid,
        pointMappingInvalid,
        composedBufferRangeInvalid,
        unsupportedViewMode,
        count,
    };

    struct SectorThingLightCache
    {
        ColorF ambient{};
        PointLight pointLight{};
        QVector3D editorSectorTint{ 1.0f, 1.0f, 1.0f };
        bool valid{};
    };

    struct PickHit
    {
        std::uint32_t id{};
        float depth{};
    };

    struct PlacementPoint
    {
        QVector3D position{};
        TVector documentPosition{};
        QVector3D surfaceNormal{};
        SurfaceRef surface{};
        bool haveSurfaceHit{};
    };

    struct TextureInfo
    {
        std::unique_ptr<QOpenGLTexture> texture{};
        std::int32_t width{};
        std::int32_t height{};
        std::int32_t alphaBpp{};
        QVector3D solidColor{1.0f, 1.0f, 1.0f};
        std::optional<ClassicSolidLightRamp> classicSolidLightRamp{};
        bool missing{};
    };

    struct PreparedTextureSource
    {
        QImage image{};
        std::int32_t alphaBpp{};
        QVector3D solidColor{1.0f, 1.0f, 1.0f};
        std::optional<ClassicSolidLightRamp> classicSolidLightRamp{};
    };

    void rebuildMeshes(
        bool resetPickIds = true,
        MeshRebuildKind kind = MeshRebuildKind::All,
        const LayerMeshProgressPulse& progressPulse = {});
    void refreshArchitecturalLightingRenderCache();
    [[nodiscard]] ArchitecturalLightingRecoveryRestoreResult
    tryApplyPendingArchitecturalLightingRecovery();
    [[nodiscard]] std::optional<ArchitecturalLightingRenderVertex>
    architecturalLightingVertexAt(
        const QPoint& widgetPosition,
        const QMatrix4x4& mvp,
        bool includeOccluded);
    [[nodiscard]] std::vector<ArchitecturalLightingVertexIdentity>
    architecturalLightingVerticesInScreenRect(
        const QRect& rect,
        const QMatrix4x4& mvp,
        bool includeOccluded);
    void applyArchitecturalLightingVertexSelection(
        const std::vector<ArchitecturalLightingVertexIdentity>& vertices,
        SelectionBoxMode mode,
        bool implicitOwnerExpansion = false);
    void applyArchitecturalLightingVertexClick(
        const ArchitecturalLightingVertexIdentity& vertex,
        Qt::KeyboardModifiers modifiers);
    [[nodiscard]] static ArchitecturalLightingVertexIdentity
    architecturalLightingVertexIdentity(
        const ArchitecturalLightingRenderVertex& vertex) noexcept
    {
        return ArchitecturalLightingVertexIdentity{
            vertex.thingIndex,
            vertex.lodIndex,
            vertex.meshIndex,
            vertex.vertexIndex};
    }
    void rebuildArchitecturalLightingPointVertices();
    void uploadArchitecturalLightingPointVertices();
    void restoreArchitecturalLightingTrianglePreview();
    void applyArchitecturalLightingTrianglePreview();
    void uploadArchitecturalLightingTrianglePreview();
    void drawArchitecturalLightingVertices(const QMatrix4x4& mvp);
    void drawArchitecturalLightingOwnerGizmos(const QMatrix4x4& mvp);
    void notifyThingsChangedImpl(bool attemptFastUpdate);
    bool shouldShowThing(const Thing& thing) const noexcept;
    static const char* thingTransformFastFallbackReasonName(
        ThingTransformFastFallbackReason reason) noexcept;
    bool tryFastUpdateThingTransforms(
        const std::vector<std::int32_t>* changedThingIndices = nullptr);
    bool tryFastUpdatePlacementPreviewThings(const std::vector<Thing>& things, bool invalid);
    [[nodiscard]] std::uint64_t placementPreviewLightingSignature(const Thing& thing) const;
    void rebuildSelectionOverlays();
    [[nodiscard]] std::optional<SurfaceEdgeSelectionRef> surfaceEdgeAtHit(
        SurfaceRef surface,
        const TVector& hit) const;
    [[nodiscard]] std::optional<TVector> surfaceEdgeMidpoint(
        const SurfaceEdgeSelectionRef& edge) const;
    [[nodiscard]] bool surfaceEdgeSelectionRefValid(
        const SurfaceEdgeSelectionRef& edge) const noexcept;
    bool revalidateSurfaceEdgeSelection(
        const editor::Selection* owners = nullptr,
        std::optional<SurfaceRef> preferredOwner = std::nullopt);
    [[nodiscard]] bool surfaceVertexSelectionRefValid(
        const editor::SurfaceVertexRef& vertex) const noexcept;
    bool revalidateActiveSurfaceVertex(
        const editor::Selection& selection,
        std::optional<SurfaceRef> preferredOwner = std::nullopt);
    void clearSurfaceEdgeSelection() noexcept;
    void applySurfaceEdgePointSelection(
        std::optional<SurfaceEdgeSelectionRef> edge,
        Qt::KeyboardModifiers modifiers);
    void applySurfaceEdgeBoxSelection(
        const std::vector<SurfaceEdgeSelectionRef>& edges,
        SelectionBoxMode mode);
    void uploadMeshes(
        const LayerMeshProgressPulse& progressPulse = {});
    void uploadSelectionOverlays(
        const LayerMeshProgressPulse& progressPulse = {});
    void refreshAlphaAdjoinWireSupports(
        const LayerMeshProgressPulse& progressPulse = {});
    void updateSurfaceNormalArrowheadsForView(
        const QVector3D& eye,
        const QVector3D& target);
    void clearTextures();
    void watchResourceFile(std::string_view name, std::string_view ext);
    void releaseDynamicLightUniformBuffer();
    void ensureTextures(
        const LayerMeshProgressPulse& progressPulse = {});
    std::shared_ptr<const assets::Model3do> loadModel3do(
        std::string_view name,
        std::size_t lodIndex = 0u);
    struct ThingLodView
    {
        QVector3D target{};
        QVector3D forward{};
        double eyeDistance{};
    };
    [[nodiscard]] ThingLodView thingLodView() const noexcept;
    [[nodiscard]] std::size_t selectedThingModelLod(
        std::size_t geosetCount,
        const Thing& thing,
        const ThingLodView& view) const noexcept;
    [[nodiscard]] bool refreshThingModelLodForView();
    const assets::SprFile* loadSprite(const std::string_view name);
    void rebuildSpriteVertices(
        const LayerMeshProgressPulse& progressPulse = {});
    void drawTransformGizmo(const QMatrix4x4& proj, const QMatrix4x4& view, const QVector3D& eye, const QVector3D& target);
    void drawSectorThrustGizmos(const QMatrix4x4& mvp, const QVector3D& eye);
    std::optional<std::int32_t> hitTestSectorThrustArrow(
        const QPoint& mousePos,
        const QMatrix4x4& mvp,
        const QMatrix4x4& invMvp,
        const QVector3D& eye);
    bool beginSectorThrustDrag(
        std::int32_t sectorIndex,
        const QPoint& mousePos,
        const QMatrix4x4& mvp,
        const QMatrix4x4& view,
        const QVector3D& eye,
        const QVector3D& target);
    void updateSectorThrustDrag(const QPoint& mousePos);
    void endSectorThrustDrag();
    bool beginTransformDrag(const QPoint& mousePos, const QMatrix4x4& proj, const QMatrix4x4& view, const QVector3D& eye, const QVector3D& target, float farPlane);
    void updateTransformDrag(
        const QPoint& mousePos,
        const QMatrix4x4& proj,
        const QMatrix4x4& view,
        const QVector3D& eye,
        const QVector3D& target,
        float farPlane,
        Qt::KeyboardModifiers modifiers);
    void endTransformDrag();
    void refreshTransformSurfaceUvPreview();
    void restoreTransformSurfaceUvPreview();
    bool publishTransformSurfaceUvPreview(
        const level::ConnectedSurfaceUvEvaluation& evaluation);
    bool applyPreviewThingLightingOverride(std::int32_t thingIndex, std::int32_t sectorIndex);
    void restorePreviewThingLightingOverride(std::int32_t thingIndex);
    void restoreAllPreviewThingLightingOverrides();

    std::uint32_t registerSurfacePickId(std::int32_t sectorIndex, std::int32_t surfaceIndex);
    std::uint32_t registerThingPickId(std::int32_t thingIndex);

    void ensurePickFbo(int pixelWidth, int pixelHeight);
    std::vector<float> pickSceneDepthsAtPoints(
        const QMatrix4x4& mvp,
        const std::vector<QPointF>& widgetPoints,
        bool includeSolidLevel,
        bool includeShadedThings,
        bool cullSingleSidedLevelBackFaces = false);
    std::optional<PickHit> pickReadPixel(
        const QMatrix4x4& mvp,
        TriKind kind,
        int geoMin,
        bool cullBackFaces,
        int pixelX,
        int pixelY,
        const std::array<std::uint32_t, 16>& excludeIds,
        int excludeCount,
        bool includeThingHelpers);
    std::vector<PickHit> pickPeel(
        const QMatrix4x4& mvp,
        const QPoint& widgetPos,
        TriKind kind,
        int geoMin,
        bool cullBackFaces,
        int maxHits,
        bool includeThingHelpers = true);
    std::vector<SurfaceRef> pickSurfacesInScreenRect(
        const QMatrix4x4& mvp,
        int left,
        int top,
        int right,
        int bottom,
        int geoMin,
        bool cullBackFaces);
    std::optional<PlacementPoint> placementPointAt(const QPoint& widgetPos, bool snapToSurfaceHit = false, bool freePlacement = false);
    std::unordered_set<std::int32_t> pickThingsInScreenRect(
        const QMatrix4x4& mvp,
        int left,
        int top,
        int right,
        int bottom,
        bool occludeBySolidLevel = false);

    const Level* level_{};
    const assets::ResourceLocator* resources_{};
    const assets::Templates* templates_{};
    app::ViewportCamera camera_{};

    QPoint lastMousePos_{};
    QPoint leftPressPos_{};
    bool leftDragged_{};
    // Qt delivers a MouseButtonRelease after MouseButtonDblClick. ARCH
    // double-click opens the vertex editor from the latter, so the trailing
    // release must not reapply a plain vertex selection and cancel the popup.
    bool suppressNextArchitecturalLightingVertexRelease_{};
    // The release path has already resolved the displayed ARCH owner using
    // the same marker/body footprint and level-depth test as a normal click.
    // Retain that identity briefly so a following double-click can open the
    // batch editor without running a second, divergent picker.
    std::optional<std::int32_t> architecturalLightingLastOwnerClick_{};
    QPoint architecturalLightingLastOwnerClickPosition_{};
    bool axisWidgetPressed_{};
    bool axisWidgetDragging_{};
    QPoint axisWidgetLastPos_{};
    bool boxSelectActive_{};
    bool boxSelectVisible_{};
    bool boxSelectFromTransformTool_{};
    QPoint boxSelectStart_{};
    QPoint boxSelectEnd_{};

    TransformTool transformTool_{ TransformTool::Select };
    TransformOrientation transformOrientation_{ TransformOrientation::Local };

    enum class TransformHandle : std::uint8_t
    {
        None,
        MoveX,
        MoveY,
        MoveZ,
        MoveFree,
        ScaleX,
        ScaleY,
        ScaleZ,
        ScaleUniform,
        RotateX,
        RotateY,
        RotateZ,
        RotateView,
    };

    enum class TransformTargetKind : std::uint8_t
    {
        Thing,
        Frame,
        Sector,
        Surface,
        Light,
        ThingLight,
        SectorLight,
    };

    struct TransformDragState
    {
        bool active{};
        TransformTargetKind target{ TransformTargetKind::Thing };
        TransformHandle handle{ TransformHandle::None };
        SurfaceSubMode surfaceMode{ SurfaceSubMode::Face };
        std::int32_t frameThing{-1};
        std::int32_t frameValue{-1};
        render::FrameMovePoseSnapshot frameMovePoseSnapshot{};
        std::int32_t surfaceSector{-1};
        std::int32_t surfaceIndex{-1};
        std::int32_t surfaceVertex{-1};
        std::int32_t surfaceEdge{-1};
        std::int32_t lightOwner{-1};
        render::LightGizmoKind lightKind{};
        QPoint startMouse{};
        QVector3D pivot{};
        std::optional<TVector> documentPivot{};
        QVector3D axis{};
        QVector3D startHit{};
        std::optional<TVector> documentStartHit{};
        QVector3D currentHit{};
        QVector3D rawTranslation{};
        QVector3D gridTranslation{};
        QVector3D translation{};
        std::optional<TVector> documentTranslation{};
        TVector gridSnapReference{};
        render::TransformGridFrame gridFrame{};
        render::GridPlaneStickState gridPlaneStick{};
        std::vector<TVector> gridStickPoints{};
        render::GeometryMoveSnapCache geometryMoveSnapCache{};
        render::ThingMoveSnapCache thingMoveSnapCache{};
        QVector3D snapRotateAxis{};
        QVector3D snapRotatePivot{};
        std::optional<TVector> documentSnapRotatePivot{};
        TVector surfaceStickAdjusted{};
        TVector surfaceStickReleaseRaw{};
        TVector surfaceStickReleaseOffset{};
        float angleDeg{};
        float snapRotateAngleDeg{};
        float scaleFactor{1.0f};
        float unitsPerPixel{};
        bool surfaceStickActive{};
        bool surfaceStickReleased{};
    };

    TransformDragState transformDrag_{};
    std::uint64_t geometryMoveSnapCacheBuildCount_{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    std::uint64_t thingMoveDragSampleCount_{};
    std::uint64_t thingMoveSnapAnchorVisitCount_{};
    std::uint64_t thingMoveBoundsResolveCount_{};
    std::uint64_t thingMoveResourceResolveCount_{};
    std::uint64_t thingMoveSurfacePickQueryCount_{};
#endif
    bool transformPreviewActive_{};
    bool thingPropertyTransformPreviewActive_{};
    std::int32_t thingPropertyTransformPreviewThing_{-1};
    std::unordered_set<std::int32_t> thingPropertyTransformPreviewThings_{};
    bool thingPropertyTransformPreviewRotating_{};
    bool transformPreviewVertexFilter_{};
    QMatrix4x4 transformPreviewMatrix_{};
    std::optional<level::ConnectedSurfaceUvPlan> transformSurfaceUvPlan_{};
    std::vector<SurfaceRef> transformSurfaceUvPreviewSurfaces_{};
    bool transformSurfaceUvFailureReported_{};
    std::array<int, 256> transformPreviewIds_{};
    // Positive IDs apply to a complete render owner and use the payload as a
    // color multiplier. Negative complemented IDs apply only where the source
    // position equals the payload. This supports complete faces and shared
    // face corners in one topology-faithful preview without another large
    // shader-uniform array.
    std::array<QVector3D, 256> transformPreviewPayloads_{};
    int transformPreviewIdCount_{};
    std::unordered_map<std::int32_t, std::int32_t> previewThingSectors_{};
    std::unordered_set<std::int32_t> previewRelitThings_{};

    bool glReady_{};
    bool glResourcesContextLost_{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    bool forceCpuPickForTest_{};
    FrameOverlayWorkForTest frameOverlayWorkForTest_{};
#endif
    editor::Selection highlightedSelection_{};
    std::int32_t selectedSector_{ -1 };
    std::int32_t selectedSurfaceSector_{ -1 };
    std::int32_t selectedSurface_{ -1 };
    std::int32_t selectedThing_{ -1 };
    std::int32_t selectedLight_{ -1 };
    render::LightGizmoCategory lightGizmoCategory_{};
    render::LightGizmoKind selectedLightGizmoKind_{};
    bool lightGizmoEditingActive_{};
    std::array<bool, 4> alwaysShowLightGizmoCategories_{};
    QPoint lastLightGizmoPickPosition_{};
    render::LightGizmoCategory lastLightGizmoPickCategory_{};
    bool haveLastLightGizmoPick_{};
    bool frameEditEnabled_{};
    std::int32_t selectedFrameValueIndex_{-1};

    int wireLineVertexCount_{};
    int thingLineFirstVertex_{};
    int thingLineVertexCount_{};
    int thingOverlayLineFirstVertex_{};
    int thingOverlayLineVertexCount_{};
    int highlightLineVertexCount_{};
    int highlightLineMultiVertexCount_{};
    int triVertexCount_{};
    MeshRebuildKind lastMeshRebuildKind_{ MeshRebuildKind::All };
    std::uint64_t meshRebuildCount_{};
    std::uint64_t meshUploadCount_{};
    std::uint64_t resourceLocatorUploadCount_{};
    std::uint64_t resourceLocatorDeferredUploadCount_{};
    std::uint64_t thingTransformFastPublicationCount_{};
    std::uint64_t thingTransformFallbackRebuildCount_{};
    ThingTransformFastFallbackReason lastThingTransformFastFallbackReason_{
        ThingTransformFastFallbackReason::none};
    std::array<std::uint64_t,
        static_cast<std::size_t>(ThingTransformFastFallbackReason::count)>
        thingTransformFastFallbackReasonCounts_{};
    int levelTriVertexCount_{};
    int thingTriVertexCount_{};
    int outlineTriVertexCount_{};
    int outlineTriMultiVertexCount_{};
    int levelPointVertexCount_{};
    int thingPointFirstVertex_{};
    int thingPointVertexCount_{};
    int pointVertexCount_{};
    int highlightPointVertexCount_{};
    int highlightPointMultiVertexCount_{};
    int highlightPointSelectedFirstPoint_{};
    int highlightPointSelectedPointCount_{};
    int architecturalLightingFirstPoint_{};
    int architecturalLightingPointCount_{};
    int architecturalLightingSelectedFirstPoint_{};
    int architecturalLightingSelectedPointCount_{};
    std::vector<ArchitecturalLightingPointVertex>
        architecturalLightingPointVertices_{};
    std::vector<ArchitecturalLightingRenderVertex>
        architecturalLightingPointIdentities_{};
    int activeEdgeHighlightFirstVertex_{};
    int activeEdgeHighlightVertexCount_{};
    int secondaryEdgeHighlightFirstVertex_{};
    int secondaryEdgeHighlightVertexCount_{};
    int activeSurfaceOwnerHighlightFirstVertex_{};
    int activeSurfaceOwnerHighlightVertexCount_{};
    int surfaceNormalHighlightFirstVertex_{};
    int surfaceNormalHighlightVertexCount_{};
    struct SurfaceNormalArrowhead
    {
        int firstVertex{};
        QVector3D tip{};
        QVector3D normal{};
        float length{};
        float width{};
    };
    std::vector<SurfaceNormalArrowhead> surfaceNormalArrowheads_{};
    std::uint64_t surfaceNormalArrowheadsViewSerial_{~std::uint64_t{0}};
    int activeVertexHighlightFirstPoint_{};
    int activeVertexHighlightPointCount_{};

    // Triangle ranges in the already-composed level VBO that belong to the
    // selected Sector/Surface geometry.  The render pass uses these ranges
    // for a small stencil mask before drawing the full selected wireframe;
    // no duplicate geometry buffer or per-frame mesh rebuild is required.
    std::vector<render::DrawRange> geometrySelectionStencilRanges_{};

    // Matching ranges in highlightLineVertices_.  They identify only the
    // selected Sector/Surface wireframe (not selected Thing overlays), so the
    // occluded geometry pass can mirror the Thing visible/occluded outline
    // behavior without changing ordinary wireframe occlusion or drawing Thing
    // highlights a second time.
    std::vector<render::DrawRange> geometrySelectionLineRanges_{};

    bool haveLastSurfaceHit_{};
    SurfaceRef lastSurfaceHitSurface_{};
    TVector lastSurfaceHitPos_{};
    struct SurfaceEdgeSelectionRefHash
    {
        std::size_t operator()(const SurfaceEdgeSelectionRef& ref) const noexcept
        {
            std::size_t seed = editor::SurfaceRefHash{}(ref.surface);
            seed ^= std::hash<std::int32_t>{}(ref.localEdge)
                + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
            seed ^= std::hash<std::int32_t>{}(ref.firstVertex)
                + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
            seed ^= std::hash<std::int32_t>{}(ref.secondVertex)
                + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
            return seed;
        }
    };
    std::unordered_set<SurfaceEdgeSelectionRef, SurfaceEdgeSelectionRefHash>
        selectedSurfaceEdges_{};
    std::optional<SurfaceEdgeSelectionRef> activeSurfaceEdge_{};

    MeshPartCache levelMeshCache_{};
    MeshPartCache thingMeshCache_{};
    sed::level::SurfacePickIndex surfacePickIndex_{};
    sed::level::SectorContainmentIndex sectorContainmentIndex_{};
    std::int32_t classicCameraSector_{-1};
    std::int32_t lastPlacementSector_{-1};
    std::vector<ThingRenderState> thingRenderState_{};
    std::optional<std::size_t> trustedThingAppendStart_{};
    std::vector<SectorThingLightCache> thingSectorLightCache_{};

    struct PlacementPreviewRenderRange
    {
        std::size_t triBuildFirst{};
        std::size_t triBuildCount{};
        std::size_t lineFirst{};
        std::size_t lineCount{};
        std::size_t spriteFirst{};
        std::size_t spriteCount{};
    };
    std::vector<PlacementPreviewRenderRange> placementPreviewRenderRanges_{};
    struct PlacementPreviewLodState
    {
        std::size_t geosetCount{};
        std::size_t requestedLodIndex{};
        std::size_t renderedLodIndex{};
        bool archThing{};
    };
    std::vector<PlacementPreviewLodState> placementPreviewLodStates_{};
    std::vector<Thing> placementPreviewRenderedThings_{};
    std::vector<std::uint64_t> placementPreviewLightingSignatures_{};
    std::vector<std::vector<std::size_t>> placementPreviewSpriteIndices_{};
    std::size_t placementPreviewSpriteBatchFirst_{};
    bool placementPreviewRenderedInvalid_{};
    bool placementPreviewRenderStateValid_{};
    std::uint64_t placementPreviewFastUpdateCount_{};

    std::vector<LineVertex> lineVertices_{};
    struct LayerLineRange
    {
        int first{};
        int count{};
        std::int32_t layer{-1};
    };
    std::vector<LayerLineRange> baseLineLayerRanges_{};
    std::vector<LineVertex> highlightLineVertices_{};
    std::vector<float> pointPositions_{};
    std::vector<float> highlightPointPositions_{};
    std::vector<TriVertex> triVertices_{};
    std::uint64_t surfaceUvFastPublicationCount_{};
    std::vector<TriVertex> outlineTriVertices_{};
    std::vector<TriBatch> triBatches_{};

    enum class OutlineSource : std::uint8_t
    {
        ThingTriangles,
        SpriteTriangles,
    };

    struct OutlineSourceRange
    {
        OutlineSource source{OutlineSource::ThingTriangles};
        int first{};
        int count{};
        bool primary{};
    };

    // Shaded Thing outlines normally reference immutable ranges in the already
    // uploaded scene buffers. Only malformed/missing-cache fallback geometry is
    // copied into outlineTriVertices_.
    std::vector<OutlineSourceRange> outlineSourceRanges_{};
    std::uint64_t selectionOutlineCacheHitCount_{};
    std::uint64_t selectionOutlineFallbackBuildCount_{};

    using DrawRange = render::DrawRange;
    using TriBuildRange = render::TriBuildRange;
    using ThingBounds = render::ThingBounds;

    struct AdjoinWireLineRange
    {
        DrawRange range{};
        std::string textureKey{};
    };

    struct SurfaceUvRange
    {
        std::size_t buildIndex{};
        int localFirst{};
        int globalFirst{};
        int count{};
    };

    std::vector<DrawRange> levelPickRanges_{};
    std::vector<DrawRange> thingPickRanges_{};
    std::unordered_map<std::uint32_t, DrawRange> surfaceLineRangesByPickId_{};
    // Base wire ranges are recorded independently of pickability. Once a MAT
    // is ready, alpha adjoins routed through the no-depth-write pass lose the
    // support-plane correction that belongs to an opaque fill.
    std::vector<AdjoinWireLineRange> alphaAdjoinWireLineRanges_{};
    bool alphaAdjoinWireSupportsDirty_{ true };
    // Texture coordinates change far more often than level topology while the
    // UV modal tools are active. Keep direct ranges so each pointer update is
    // proportional to the edited surfaces, not to the entire level.
    std::unordered_map<std::uint64_t, std::vector<SurfaceUvRange>> surfaceUvRanges_{};

    // Cached mapping from thing pick-id to the vertex ranges it occupies in the thing caches.
    // This allows fast per-thing transform updates without scanning the entire buffers.
    std::unordered_map<int, std::vector<TriBuildRange>> thingTriBuildRangesByPickId_{};
    std::unordered_map<int, std::vector<DrawRange>> thingLineRangesByPickId_{};
    std::unordered_map<int, std::vector<DrawRange>> thingCollisionOverlayRangesByPickId_{};
    std::unordered_map<int, std::vector<DrawRange>> thingBroadPhaseTreeGuideRangesByPickId_{};
    std::unordered_map<int, std::vector<DrawRange>> thingVisualBoundsOverlayRangesByPickId_{};
    std::unordered_map<int, std::vector<DrawRange>> thingTemplateCollisionBoundsGuideRangesByPickId_{};
    std::unordered_map<int, std::vector<DrawRange>> spriteTriRangesByPickId_{};

    // Map each `thingMeshCache_.triBuilds[i]` to its first vertex in the composed `triVertices_` buffer.
    // Recomputed whenever draw buffers are composed.
    std::vector<int> thingTriBuildGlobalFirstVertex_{};

    // Per-thing bounds for fast updates (world-space AABB of rendered thing geometry).
    std::vector<ThingBounds> thingBounds_{};
    // Owners of minX/minY/minZ/maxX/maxY/maxZ in thingMeshCache_.  A targeted
    // transform can update the aggregate bounds in O(changed Things) unless it
    // moves an owner that currently defines an extreme, in which case one
    // exact rescan is required.
    std::array<std::size_t, 6> thingBoundsExtremeOwners_{};
    bool thingBoundsExtremeOwnersValid_{};
    // Camera/orientation-aware bounds for rendered sprite quads. Sprites use
    // a separate vertex buffer, so they cannot be folded into thingBounds_
    // while the ordinary Thing mesh cache is composed.
    std::vector<ThingBounds> spriteThingBounds_{};

    // Cached OpenGL multi-draw commands for stable Thing overlay ranges. They
    // are rebuilt with the composed line geometry, not during each paint.
    struct ThingOverlayDrawCommands
    {
        std::vector<GLint> firsts{};
        std::vector<GLsizei> counts{};
    };
    ThingOverlayDrawCommands thingCollisionOverlayDrawCommands_{};
    ThingOverlayDrawCommands thingBroadPhaseOverlayDrawCommands_{};
    ThingOverlayDrawCommands thingVisualBoundsOverlayDrawCommands_{};
    ThingOverlayDrawCommands thingTemplateBoundsOverlayDrawCommands_{};
    ThingOverlayDrawCommands thingSelectedCollisionDrawCommands_{};
    ThingOverlayDrawCommands thingSelectedBroadPhaseDrawCommands_{};
    ThingOverlayDrawCommands thingSelectedVisualBoundsDrawCommands_{};
    QFunctionPointer thingOverlayMultiDrawArraysFunction_{};
    std::uint64_t thingOverlayDrawCacheMeshRevision_{};
    bool thingOverlayDrawCacheMeshRevisionValid_{};
    std::vector<std::int32_t> thingOverlayDrawCacheSelectedOwners_{};
    std::int32_t thingOverlayDrawCacheSelectedThing_{-1};
    bool thingOverlayDrawCacheSelectionValid_{};

    // In Dots view mode, maps `thingIndex -> point index` within `thingMeshCache_.pointPositions` (or -1 if absent).
    std::vector<int> thingPointIndexByThingIndex_{};
    std::vector<render::DrawRange> thingCollisionOverlayLineRanges_{};
    std::vector<render::DrawRange> thingBroadPhaseTreeGuideLineRanges_{};
    std::vector<render::DrawRange> thingVisualBoundsOverlayLineRanges_{};
    std::vector<render::DrawRange> thingTemplateCollisionBoundsGuideLineRanges_{};
    bool thingOverlayGeometryPrepared_{};

    std::int32_t outlinedThing_{-1};
    QVector3D outlinedThingCenter_{};
    bool outlinedThingDoubleSided_{};

    bool showWireframe_{true};
    bool showVertices_{};
    bool showSolid_{true};
    bool showTextures_{true};
    bool showFog_{};
    bool showThings_{true};
    bool showHiddenThings_{true};
    bool showThingCollisionShapes_{ true };
    float motsTreeCollisionPreviewMeshRadiusFactor_{ 0.75f };
    bool showThingVisualBounds_{ true };
    bool showTemplateCollisionBoundsGuide_{};
    bool showAxisWidget_{true};
    bool showToastOverlays_{true};
    bool showGrid_{true};
    bool gridFollowCamera_{true};
    bool gridRotateWithView_{};
    bool gridKeepViewAlignedVisible_{};
    GridPlane gridPlane_{GridPlane::XY};
    std::optional<QVector3D> gridCenterOverride_{};
    bool gridAlignToView_{};
    QVector3D gridAlignAxisA_{ 1.0f, 0.0f, 0.0f };
    QVector3D gridAlignAxisB_{ 0.0f, 1.0f, 0.0f };
    float gridMajorSpacing_{1.0f};
    float gridMinorSpacing_{0.1f};
    bool gridAdaptiveSpacing_{};
    float gridOpacity_{0.70f};
    float surfaceExtrudePreviewOpacity_{0.74f};
    QColor gridColor_{ QColor::fromRgb(112, 128, 156) };
    QColor gridMajorColor_{ QColor::fromRgb(170, 184, 208) };
    QColor gridAxisXColor_{ QColor(210, 75, 70) };
    QColor gridAxisYColor_{ QColor(95, 190, 95) };
    QColor gridAxisZColor_{ QColor(90, 140, 215) };
    float gridLineWidthPx_{1.0f};
    bool gridMinorDots_{};
    float gridMinorDotSizePx_{2.0f};
    bool gridVisibleThroughWalls_{};
    bool gridAlwaysBehind_{};
    bool gridResetOnCameraRotate_{true};
    bool gridOverrideCamBasisValid_{};
    QVector3D gridOverrideCamForward_{ 0.0f, 1.0f, 0.0f };
    QVector3D gridOverrideCamUp_{ 0.0f, 0.0f, 1.0f };
    bool sectorThrustToolEnabled_{};
    bool showAllSectorThrustGizmos_{true};
    bool showSectorThrustVectors_{true};
    // Sector geometry centers are invariant under thrust edits. Reuse the
    // level-mesh rebuild result instead of rescanning every sector's vertices
    // for every painted frame and pointer hover.
    std::vector<QVector3D> sectorThrustCenters_{};
    std::unordered_set<std::int32_t> selectedSectorThrusts_{};
    std::int32_t hoveredSectorThrust_{-1};
    struct SectorThrustDragState
    {
        bool active{};
        bool moved{};
        std::int32_t activeSector{-1};
        QPoint startMouse{};
        QPointF originScreen{};
        QPointF endpointOffset{};
        QVector3D cameraRight{ 1.0f, 0.0f, 0.0f };
        QVector3D cameraUp{ 0.0f, 0.0f, 1.0f };
        QVector3D cameraForward{ 0.0f, 1.0f, 0.0f };
        float activeDepthPx{};
        float baselineHandleRadiusPx{};
        std::vector<std::pair<std::int32_t, QVector3D>> baselines{};
        std::vector<std::pair<std::int32_t, QVector3D>> previews{};
    } sectorThrustDrag_{};
    struct SectorThrustLabel
    {
        QPointF position{};
        QString text{};
        bool selected{};
    };
    std::vector<SectorThrustLabel> sectorThrustLabels_{};
    using StagedCleaveCameraState = RecoveryState::CleaveCamera;
    using StagedCleaveLine = RecoveryState::CleaveLine;
    using StagedCleaveChainOrigin = RecoveryState::CleaveChainOrigin;
    using StagedCleaveContext = RecoveryState::CleaveContext;
    using StagedCleaveDraftState = RecoveryState::CleaveDraftSnapshot;
    struct StagedCleaveDraftHistory
    {
        std::vector<StagedCleaveDraftState> states{};
        std::size_t cursor{};
        std::size_t ownedBytes{};
        bool active{};
        bool gestureActive{};
        std::optional<StagedCleaveDraftState> gestureBefore{};
    };
    struct StagedCleaveEndpointRef
    {
        std::int32_t line{-1};
        std::int32_t endpoint{-1};
    };
    struct StagedCleaveLineMove
    {
        std::int32_t line{-1};
        std::uint8_t endpointMask{};
    };
    static constexpr std::size_t kStagedCleaveHistoryMaxSnapshots = 64;
    static constexpr std::size_t kStagedCleaveHistoryMaxBytes = 16u * 1024u * 1024u;
    struct StagedCleavePlaneDrag
    {
        bool active{};
        bool translationReady{};
        bool restoreAuthoredCameraOnClick{};
        bool disconnectedForDrag{};
        bool preserveRigidLinePlacement{};
        std::int32_t line{-1};
        TVector planePoint{};
        TVector planeNormal{};
        TVector initialHit{};
        std::vector<std::int32_t> movingLines{};
        std::vector<StagedCleaveLineMove> affectedLines{};
        std::int32_t grabbedEndpoint{-1};
    };
    struct StagedCleaveTransformDrag
    {
        bool active{};
        StagedCleaveTransformMode mode{StagedCleaveTransformMode::Move};
        QPoint startPosition{};
        QPoint currentPosition{};
        TVector pivot{};
        TVector planeNormal{};
        TVector initialHit{};
        double initialAngleDegrees{};
        double initialRadius{};
        double appliedAngleDegrees{};
        double appliedScale{1.0};
        std::vector<std::int32_t> selectedLines{};
        std::vector<StagedCleaveLineMove> affectedLines{};
        std::vector<StagedCleaveLine> previewLines{};
    };
    struct StagedCleaveMarqueeSelection
    {
        bool active{};
        QPoint startPosition{};
        QPoint currentPosition{};
        SelectionBoxMode mode{SelectionBoxMode::Replace};
        std::vector<std::int32_t> baselineSelection{};
    };
    struct StagedCleaveShapePlacementFrame
    {
        TVector center{};
        TVector planeNormal{};
        TVector axisA{};
        TVector axisB{};
    };
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
public:
    void addStagedCleaveLineForTest(
        const QVector3D& first, const QVector3D& second, const QVector3D& planeNormal,
        const bool linkedToPrevious = false, const bool linkedToNext = false)
    {
        addStagedCleaveDocumentLineForTest(render::cleaveDocumentPoint(first),
            render::cleaveDocumentPoint(second), render::cleaveDocumentPoint(planeNormal),
            linkedToPrevious, linkedToNext);
    }
    void addStagedCleaveDocumentLineForTest(
        const TVector& first,
        const TVector& second,
        const TVector& planeNormal,
        const bool linkedToPrevious = false,
        const bool linkedToNext = false)
    {
        StagedCleaveLine line{};
        line.first = first;
        line.second = second;
        line.planeNormal = planeNormal;
        line.linkedToPrevious = linkedToPrevious;
        line.linkedToNext = linkedToNext;
        initializeStagedCleaveLineFrame(line, second - first);
        captureStagedCleaveLineCamera(line);
        stagedCleaveLines_.push_back(line);
        const std::int32_t index = static_cast<std::int32_t>(stagedCleaveLines_.size() - 1);
        stagedCleaveApplyOrder_.push_back(index);
        setSelectedStagedCleaveLines({index}, index);
    }
    void selectStagedCleaveLineForTest(const std::int32_t index) noexcept
    {
        selectedStagedCleaveLines_.clear();
        if (index >= 0 && static_cast<std::size_t>(index) < stagedCleaveLines_.size())
        {
            selectedStagedCleaveLines_.push_back(index);
            selectedStagedCleaveLine_ = index;
        }
        else
        {
            selectedStagedCleaveLine_ = -1;
        }
    }
    [[nodiscard]] bool stagedCleaveLinesFormSingleOpenChainForTest() const noexcept
    {
        return stagedCleaveLinesFormSingleOpenChain();
    }
    [[nodiscard]] bool stagedCleaveLinesFormSingleClosedChainForTest() const noexcept
    {
        return stagedCleaveLinesFormSingleClosedChain();
    }
    [[nodiscard]] std::optional<TVector> stagedCleaveFirstPointForTest() const noexcept
    {
        if ( surfaceCleaveHaveFirstPoint_ )
        {
            return surfaceCleaveFirstPoint_;
        }
        if ( sectorCleaveHaveFirstPoint_ )
        {
            return sectorCleaveFirstPoint_;
        }
        return std::nullopt;
    }
    [[nodiscard]] QVector3D stagedCleavePlaneNormalForTest(const std::size_t index) const
    {
        return index < stagedCleaveLines_.size()
            ? render::cleaveRenderPoint(stagedCleaveLines_[index].planeNormal)
            : QVector3D{};
    }
    [[nodiscard]] QVector3D stagedCleavePlanePointForTest(const std::size_t index) const
    {
        return index < stagedCleaveLines_.size()
            ? render::cleaveRenderPoint(stagedCleaveLines_[index].first)
            : QVector3D{};
    }
    [[nodiscard]] auto stagedCleavePlanePolygonsForTest(const std::size_t index) const
    {
        return stagedCleavePlanePolygons(stagedCleaveLines_.at(index));
    }
    bool reshapeStagedCleaveEndpointForTest(
        const std::size_t index,
        const std::int32_t endpoint,
        const QVector3D& position)
    {
        return reshapeStagedCleaveEndpoint(index, endpoint, render::cleaveDocumentPoint(position));
    }
    bool reshapeDisjointedStagedCleaveEndpointForTest(
        const std::size_t index,
        const std::int32_t endpoint,
        const QVector3D& position)
    {
        return reshapeStagedCleaveEndpointWithOptionalDisjoint(
            stagedCleaveLines_,
            index,
            endpoint,
            render::cleaveDocumentPoint(position),
            true);
    }
    [[nodiscard]] bool stagedCleaveEndpointLinkedForTest(
        const std::size_t index,
        const std::int32_t endpoint) const noexcept
    {
        if (index >= stagedCleaveLines_.size())
        {
            return false;
        }
        return endpoint == 0
            ? stagedCleaveLines_[index].linkedToPrevious
            : endpoint == 1 && stagedCleaveLines_[index].linkedToNext;
    }
    bool translateStagedCleavePlaneForTest(
        const std::size_t index,
        const QVector3D& translation)
    {
        return translateStagedCleavePlane(index, render::cleaveDocumentPoint(translation));
    }
    bool restoreStagedCleaveLineCameraForTest(const std::size_t index)
    {
        return restoreStagedCleaveLineCamera(index);
    }
    bool attachStagedSectorCleaveLineToCameraEdgeForTest(const std::size_t index)
    {
        return attachStagedSectorCleaveLineToCameraEdge(index);
    }
    void refreshCleaveSnapPreviewAtForTest(
        const QPoint& position,
        const Qt::KeyboardModifiers modifiers)
    {
        refreshCleaveSnapPreview(position, modifiers);
    }
    [[nodiscard]] std::optional<render::CleaveSnapTargetKind>
    cleaveSnapMarkerKindForTest() const noexcept
    {
        return cleaveSnapMarker_
            ? std::optional<render::CleaveSnapTargetKind>(cleaveSnapMarker_->kind)
            : std::nullopt;
    }
    [[nodiscard]] bool cleaveSnapGuidesVisibleForTest() const noexcept
    {
        return cleaveSnapGuidesVisible_;
    }
    [[nodiscard]] std::size_t cleaveSnapGuideMarkerCountForTest(
        const render::CleaveSnapTargetKind kind)
    {
        if (!cleaveSnapGuidesVisible_)
        {
            return 0;
        }

        QMatrix4x4 projection{};
        QMatrix4x4 view{};
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        buildCameraMatrices(
            projection,
            view,
            eye,
            target,
            nearPlane,
            farPlane);
        const std::vector<render::CleaveSnapCandidate> markers =
            cleaveGeometrySnapTargets(
                projection * view,
                surfaceCleaveToolEnabled_
                    && surfaceSubMode_ == SurfaceSubMode::Face);
        return static_cast<std::size_t>(std::count_if(
            markers.begin(),
            markers.end(),
            [kind](const render::CleaveSnapCandidate& marker)
            {
                return marker.kind == kind;
            }));
    }
    [[nodiscard]] QVector3D stagedCleaveLineSecondForTest(const std::size_t index) const
    {
        return index < stagedCleaveLines_.size()
            ? render::cleaveRenderPoint(stagedCleaveLines_[index].second)
            : QVector3D{};
    }
    [[nodiscard]] QVector3D stagedCleaveLineFirstForTest(const std::size_t index) const
    {
        return index < stagedCleaveLines_.size()
            ? render::cleaveRenderPoint(stagedCleaveLines_[index].first)
            : QVector3D{};
    }
    [[nodiscard]] bool stagedCleavePlaneDraggingForTest() const noexcept
    {
        return stagedCleavePlaneDrag_.active;
    }
    [[nodiscard]] bool stagedCleaveShapePlacementActiveForTest() const noexcept
    {
        return cleaveShapeOptions_.placementArmed
            && stagedCleaveShapePlacementPlan_.has_value();
    }
    [[nodiscard]] const std::optional<ui::viewport::CleaveShapePlan>&
    stagedCleaveShapePlacementPlanForTest() const noexcept
    {
        return stagedCleaveShapePlacementPlan_;
    }
    [[nodiscard]] std::optional<QVector3D> stagedCleaveShapePreviewCenterForTest() const
    {
        return stagedCleaveShapePreviewCenter_;
    }
    [[nodiscard]] QString stagedCleaveTransformHudTextForTest() const
    {
        return stagedCleaveTransformHudText();
    }
    [[nodiscard]] std::uint64_t stagedCleaveBaselineCaptureCountForTest() const noexcept
    {
        return stagedCleaveBaselineCaptureCountForTest_;
    }
    [[nodiscard]] std::uint64_t stagedCleaveTransformPreparationCountForTest() const noexcept
    {
        return stagedCleaveTransformPreparationCountForTest_;
    }
    [[nodiscard]] std::uint64_t stagedCleaveHistoryCurrentCaptureCountForTest() const noexcept
    {
        return stagedCleaveHistoryCurrentCaptureCountForTest_;
    }
    [[nodiscard]] QRectF stagedCleaveTransformHudRectForTest() const
    {
        return stagedCleaveTransformHudRect_;
    }
    [[nodiscard]] const std::vector<render::CleaveJointAngle>&
    stagedCleaveJointAnglesForTest() const noexcept
    {
        return stagedCleaveJointGuides_.joints;
    }
    [[nodiscard]] QRect stagedCleaveOrderLabelRectForTest(
        const std::int32_t line) const
    {
        auto* const mutableThis = const_cast<GlView*>(this);
        QMatrix4x4 projection{};
        QMatrix4x4 view{};
        QVector3D eye{};
        QVector3D target{};
        float nearPlane{};
        float farPlane{};
        mutableThis->buildCameraMatrices(
            projection,
            view,
            eye,
            target,
            nearPlane,
            farPlane);
        return mutableThis->stagedCleaveOrderLabelRect(line, projection * view);
    }
    void beginCleaveRecoveryRestoreOwnedArrayTrackingForTest() noexcept
    {
        cleaveRecoveryRestoreOwnedArrayTrackingForTest_ = true;
        cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_ = 0;
        cleaveRecoveryRestorePeakOwnedArrayBytesForTest_ = 0;
    }
    void endCleaveRecoveryRestoreOwnedArrayTrackingForTest() noexcept
    {
        cleaveRecoveryRestoreOwnedArrayTrackingForTest_ = false;
    }
    [[nodiscard]] std::size_t
    cleaveRecoveryRestoreSourceOwnedArrayBytesForTest() const noexcept
    {
        return cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_;
    }
    [[nodiscard]] std::size_t
    cleaveRecoveryLiveOwnedArrayBytesForTest() const noexcept
    {
        return cleaveRecoveryLiveOwnedArraysForTest();
    }
    [[nodiscard]] std::size_t
    cleaveRecoveryRestorePeakOwnedArrayBytesForTest() const noexcept
    {
        return cleaveRecoveryRestorePeakOwnedArrayBytesForTest_;
    }
private:
#endif
    std::vector<StagedCleaveLine> stagedCleaveLines_{};
    StagedCleaveContext stagedCleaveContext_{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    bool cleaveRecoveryRestoreOwnedArrayTrackingForTest_{};
    std::size_t cleaveRecoveryRestoreSourceOwnedArrayBytesForTest_{};
    std::size_t cleaveRecoveryRestorePeakOwnedArrayBytesForTest_{};
    std::uint64_t stagedCleaveBaselineCaptureCountForTest_{};
    std::uint64_t stagedCleaveTransformPreparationCountForTest_{};
    std::uint64_t stagedCleaveHistoryCurrentCaptureCountForTest_{};
#endif
    StagedCleavePlaneDrag stagedCleavePlaneDrag_{};
    StagedCleaveTransformDrag stagedCleaveTransformDrag_{};
    render::CleaveWheelSession stagedCleaveWheelSession_{};
    std::optional<RecoveryState::CleaveShapeOptions> stagedCleaveWheelShapeBefore_{};
    bool stagedCleaveWheelShapeOptionsApplying_{};
    StagedCleaveMarqueeSelection stagedCleaveMarqueeSelection_{};
    std::optional<ui::viewport::CleaveShapePlan> stagedCleaveShapePlacementPlan_{};
    std::optional<QVector3D> stagedCleaveShapePreviewCenter_{};
    std::optional<StagedCleaveShapePlacementFrame> stagedCleaveShapePreviewFrame_{};
    bool stagedCleaveShapePlacementClickPending_{};
    QString stagedCleaveTransformHudText_{};
    render::cleave::CleaveHudPresentation stagedCleaveTransformHudPresentation_{};
    ViewportBannerKind stagedCleaveTransformHudBannerKind_{
        ViewportBannerKind::cleaveScale};
    std::optional<TVector> stagedCleaveTransformHudAnchor_{};
    QRectF stagedCleaveTransformHudRect_{};
    std::unique_ptr<QTimer> stagedCleaveTransformHudHideTimer_{};
    std::unique_ptr<QTimer> stagedCleaveTransformHudPresentationTimer_{};
    render::CleaveJointGuideCache stagedCleaveJointGuides_{};
    std::uint64_t stagedCleaveJointGuidePreviewRevision_{};
    std::vector<std::int32_t> stagedCleaveRankByLine_{};
    bool stagedCleaveRankCacheValid_{};
    bool suppressStagedCleavePublication_{};
    QPointer<QLineEdit> stagedCleaveOrderEditor_{};
    std::int32_t stagedCleaveOrderEditorLine_{-1};
    std::vector<std::int32_t> stagedCleaveOrderEditorBaseline_{};
    std::optional<StagedCleaveChainOrigin> stagedCleaveChainOrigin_{};
    StagedCleaveDraftHistory stagedCleaveHistory_{};
    std::vector<std::int32_t> stagedCleaveApplyOrder_{};
    std::vector<std::int32_t> selectedStagedCleaveLines_{};
    StagedCleaveTransformMode stagedCleaveTransformMode_{StagedCleaveTransformMode::Move};
    std::array<QList<QKeySequence>, 3> stagedCleaveTransformShortcutBindings_{
        QList<QKeySequence>{ QKeySequence(Qt::Key_G) },
        QList<QKeySequence>{ QKeySequence(Qt::Key_R) },
        QList<QKeySequence>{ QKeySequence(Qt::Key_S) } };
    RecoveryState::CleaveShapeOptions cleaveShapeOptions_{};
    std::uint64_t stagedCleaveAuthoredRevision_{};
    CleaveWorkspaceFamily cleaveWorkspaceFamily_{CleaveWorkspaceFamily::Sector};
    std::optional<RecoveryState::CleaveWorkspace> stagedCleavePeerWorkspace_{};
    bool stagedCleavePeerHistoryActive_{};
    std::array<std::uint64_t, 2> stagedCleaveFamilyAuthoredRevisions_{};
    std::optional<RecoveryState::ArchitecturalLightingState>
        pendingArchitecturalLightingRecovery_{};
    bool pendingArchitecturalLightingSelectionReady_{};
    std::vector<std::int32_t> pendingArchitecturalLightingOwnerSelection_{};
    bool cleaveStagedMode_{};
    bool cleaveChainEnabled_{};
    bool cleaveBounded_{true};
    bool cleaveMeasurementsVisible_{true};
    CleaveMeasureUnit cleaveMeasureUnit_{CleaveMeasureUnit::Meters};
    bool cleaveAnglesVisible_{true};
    bool cleaveLengthsVisible_{true};
    bool cleaveLineNumbersVisible_{};
    double cleaveAngleStepDegrees_{};
    std::int32_t selectedStagedCleaveLine_{-1};
    std::int32_t editingStagedCleaveLine_{-1};
    std::int32_t editingStagedCleaveEndpoint_{-1};
    bool editingDisjointStagedCleaveEndpoint_{};
    bool editingStagedCleaveEndpointPreservePlacement_{};
    bool stagedCleaveEndpointOriginClickCandidate_{};
    bool stagedCleaveLineClickCandidate_{};
    bool stagedCleaveDragSelectionApplied_{};
    bool stagedCleaveJointCycleClickCandidate_{};
    std::vector<std::int32_t> stagedCleaveJointCycleCandidates_{};
    std::int32_t stagedCleaveJointCycleActiveBefore_{-1};
    Qt::KeyboardModifiers stagedCleavePressModifiers_{};
    std::optional<render::CleaveSnapCandidate> cleaveSnapMarker_{};
    bool cleaveSnapGuidesVisible_{};
    bool sectorCleaveToolEnabled_{};
    bool sectorCleaveHaveFirstPoint_{};
    TVector sectorCleaveFirstPoint_{};
    TVector sectorCleaveCurrentPoint_{};
    std::vector<TVector> placementPreviewLines_{};
    bool placementPreviewCursorGuide_{};
    bool placementPreviewLinesInvalid_{};
    std::optional<Thing> placementPreviewThing_{};
    std::vector<Thing> placementPreviewThings_{};
    bool placementPreviewThingInvalid_{};
    std::optional<Sector> placementPreviewSector_{};
    std::optional<Sector> placementPreviewSourcePatch_{};
    std::optional<SurfaceRef> placementPreviewHiddenSurface_{};
    bool placementPreviewSectorInvalid_{};
    bool placementPreviewPersistent_{};
    bool surfaceCleaveToolEnabled_{};
    bool surfaceCleaveHaveFirstPoint_{};
    TVector surfaceCleaveFirstPoint_{};
    TVector surfaceCleaveCurrentPoint_{};
    bool showThingGizmos_{false};
    bool showArchThingGizmos_{true};
    bool architecturalLightingEditingActive_{};
    ArchitecturalLightingTargetMode architecturalLightingTargetMode_{
        ArchitecturalLightingTargetMode::Owner};
    std::vector<ArchitecturalLightingVertexIdentity>
        architecturalLightingSelectedVertices_{};
    // Owner/body selection intentionally expands to every vertex of the
    // selected owner. Keep that provenance separate from an explicit vertex
    // batch so the first plain click on an expanded model can narrow to the
    // clicked vertex while a double-click on an explicit batch preserves it.
    bool architecturalLightingSelectedVerticesImplicitOwnerExpansion_{};
    std::unordered_set<ArchitecturalLightingVertexIdentity,
        ArchitecturalLightingVertexIdentityHash>
        architecturalLightingSelectedVertexSet_{};
    std::optional<ArchitecturalLightingRenderCache>
        architecturalLightingRenderCache_{};
    // Published spans for every currently selected ARCH owner.  The primary
    // cache above remains the active Thing's payload for the sparse shaded
    // preview, while this bounded table lets hover/click resolve an exact
    // source vertex from any selected owner without loading or hashing model
    // resources on the pointer path.
    std::vector<ArchitecturalLightingRenderCache>
        architecturalLightingSelectedOwnerCaches_{};
    std::unordered_map<std::int32_t, std::size_t>
        architecturalLightingSelectedOwnerCacheByThing_{};
    std::vector<ArchitecturalLightingPreviewVertex>
        architecturalLightingPreview_{};
    std::unordered_map<ArchitecturalLightingVertexIdentity,
        MotsArchLightVertex,
        ArchitecturalLightingVertexIdentityHash>
        architecturalLightingPreviewByIdentity_{};
    std::unordered_map<std::int32_t, ArchitecturalLightingPreviewOwnerRevision>
        architecturalLightingPreviewOwnerRevisions_{};
    const assets::Model3do* architecturalLightingPreviewModelIdentity_{};
    std::uint64_t architecturalLightingPreviewModelRevision_{};
    std::vector<ArchitecturalLightingTrianglePatch>
        architecturalLightingTrianglePatches_{};
    std::vector<std::int32_t>
        architecturalLightingTriangleDirtyIndices_{};
    std::unordered_set<std::int32_t>
        architecturalLightingTriangleDirtyIndexSet_{};
    std::unordered_set<std::int32_t>
        architecturalLightingTrianglePatchIndexSet_{};
    // The persistent editor-only channel used to display bound ARCH models.
    // Popup staging below remains independent and is cleared on commit/cancel.
    level::MotsArchLightingVertexMode architecturalLightingAuthoringMode_{
        level::MotsArchLightingVertexMode::Rgb};
    level::MotsArchLightingVertexMode architecturalLightingPreviewMode_{
        level::MotsArchLightingVertexMode::Rgb};
    std::optional<ArchitecturalLightingRenderVertex>
        hoveredArchitecturalLightingVertex_{};
    QColor thingIconCubeTintColor_{ Qt::white };
    QColor backgroundClearColor_{ QColor::fromRgbF(0.08, 0.09, 0.12, 1.0) };
    QColor wireframeColor_{ QColor::fromRgbF(0.86, 0.87, 0.92, 1.0) };
    float wireframeLineWidthPx_{1.35f};
    bool wireframeAntialiasingEnabled_{true};
    float framePathLineWorldSize_{0.03f};
    float framePathPointWorldSize_{0.18f};
    QColor selectionPrimaryColor_{ QColor::fromRgbF(0.98, 0.80, 0.25, 1.0) };
    QColor selectionMultiColor_{ QColor::fromRgbF(0.20, 0.72, 0.95, 1.0) };
    QColor selectionEdgeMultiColor_{ QColor::fromRgb(255, 152, 56) };
    QColor selectionActiveColor_{ QColor::fromRgbF(0.95, 0.20, 0.20, 1.0) };
    QColor selectionVertexColor_{ QColor::fromRgbF(0.98, 0.80, 0.25, 1.0) };
    QColor selectionVertexMultiColor_{ QColor::fromRgbF(0.20, 0.72, 0.95, 1.0) };
    QColor cogEntityPickPrimaryColor_{ QColor::fromRgbF(0.20, 0.86, 0.96, 1.0) };
    QColor cogEntityPickActiveColor_{ QColor::fromRgbF(0.98, 0.66, 0.20, 1.0) };
    std::vector<QColor> layerColors_{};
    bool layerWireframeColoringEnabled_{true};
    bool cogEntityPickHighlightEnabled_{};
    bool activeSurfaceVertexHintValid_{};
    editor::SurfaceVertexRef activeSurfaceVertexHint_{};
    float thingCubeHalfSize_{0.0125f};
    float thingGizmoWorldSize_{0.03f};
    float thingGizmoHeightAboveModel_{};
    float thingGizmoStackSpacing_{
        render::kThingGizmoStackSpacingDefault};
    float thingLightGizmoOffsetAboveModel_{};
    float archThingGizmoOffsetAboveModel_{};
    float levelGeometryVertexMarkerSizePx_{
        kLevelGeometryVertexMarkerDefaultSizePx};
    float lightGizmoWorldSize_{0.04f};
    bool lightGizmoUseLightColor_{true};
    float architecturalLightingMarkerSizePx_{
        kArchitecturalLightingMarkerDefaultSizePx};
    bool architecturalLightingMarkerUseAuthoredColor_{true};
    QColor architecturalLightingMarkerColor_{QColor::fromRgb(255, 204, 51)};
    bool showEditorCubes_{true};
    bool showPreviewCubes_{};
    bool editorDefaultShowFog_{};
    bool previewDefaultShowFog_{true};
    bool editorDefaultShowThingCollisionShapes_{ true };
    bool previewDefaultShowThingCollisionShapes_{};
    bool editorDefaultShowThingVisualBounds_{ true };
    bool previewDefaultShowThingVisualBounds_{};
    bool editorDefaultThingModelLodEnabled_{};
    bool previewDefaultThingModelLodEnabled_{};
    bool previewDefaultShowThingGizmos_{false};
    bool previewDefaultShowArchThingGizmos_{};
    std::unordered_set<std::int32_t> hiddenLayers_{};
    std::unordered_set<std::int32_t> lockedLayers_{};
    bool previewRespectHiddenLayers_{};
    bool geometryTransformSnapEnabled_{true};
    bool geometryTransformRotateToNearbyEnabled_{true};
    bool preserveConnectedSurfaceUvsEnabled_{true};
    float vertexSnapRadius_{0.20f};
    float edgeSnapRadius_{0.20f};
    float faceSnapRadius_{2.0f};
    float sectorSnapRadius_{2.0f};
    float thingSnapRadius_{0.16f};
    float snapRotationMaxDegrees_{180.0f};
    float surfaceStickRadius_{0.22f};
    float gridPlaneStickRadius_{0.08f};
    ui::shortcuts::ModifierBindings geometryTransformSnapInvertModifiers_{Qt::ShiftModifier};
    bool transformGridSnapEnabled_{};
    ui::shortcuts::ModifierBindings transformGridSnapInvertModifiers_{Qt::ControlModifier};
    ui::shortcuts::ModifierBindings transformGridStickInvertModifiers_{
        Qt::ShiftModifier | Qt::AltModifier};
    bool transformGridPlaneStickEnabled_{true};
    bool sectorMoveSnapAlignEnabled_{true};
    ui::shortcuts::ModifierBindings sectorMoveSnapAlignInvertModifiers_{Qt::ControlModifier};
    bool thingMoveSurfaceStickEnabled_{true};
    bool thingMoveSurfaceAlignEnabled_{};
    ui::shortcuts::ModifierBindings thingMoveSurfaceStickInvertModifiers_{Qt::ShiftModifier};
    ui::shortcuts::ModifierBindings thingMoveSurfaceAlignInvertModifiers_{
        Qt::ControlModifier | Qt::ShiftModifier};
    bool thingMoveSnapToThingEnabled_{};
    ui::shortcuts::ModifierBindings thingMoveSnapInvertModifiers_{Qt::ControlModifier};
    bool thingMoveSnapRotateEnabled_{true};
    ui::shortcuts::ModifierBindings thingMoveSnapRotateInvertModifiers_{
        Qt::ControlModifier | Qt::AltModifier};
    ThingViewMode thingViewMode_{ThingViewMode::Shaded};
    bool thingModelLodEnabled_{};
    LightingMode lightingMode_{LightingMode::FullyLit};
    DynamicLightShading dynamicLightShading_{DynamicLightShading::Gouraud};
    bool thingLightRenderingEnabled_{true};
    bool sectorPointLightRenderingEnabled_{true};
    RenderPipeline renderPipeline_{RenderPipeline::Editor};
    ViewportState editorState_{};
    ViewportState previewState_{};
    PickMode pickMode_{PickMode::Auto};
    PickPreference pickPreference_{PickPreference::Auto};
    SurfaceSubMode surfaceSubMode_{SurfaceSubMode::Face};
    bool strictSurfaceSelectionEnabled_{};
    std::unordered_set<SurfaceRef, editor::SurfaceRefHash,
        editor::SurfaceRefEq> strictSurfaceSelectionScope_{};
    bool showSelectedSurfaceNormals_{};
    float surfaceNormalLengthScale_{1.0f};
    float surfaceNormalArrowheadScale_{1.0f};
    bool showSurfaceNormalArrowhead_{true};
    bool viewportPickingEnabled_{true};
    bool transformInteractionEnabled_{true};
    bool placementGestureEnabled_{};
    bool hoverPickEnabled_{};
    bool previewInteractionEnabled_{};
    bool previewInteractionDragging_{};
    ui::shortcuts::ModifierBindings selectionAddModifiers_{Qt::ShiftModifier};
    ui::shortcuts::ModifierBindings selectionRemoveModifiers_{Qt::ControlModifier};
    ui::shortcuts::ModifierBindings selectionSurfaceBypassFiltersModifiers_{Qt::AltModifier};
    bool surfaceClickPreferSurfaceHit_{true};
    bool surfaceClickSelectionEnabled_{true};
    bool xraySelectionCyclingEnabled_{};
    bool orbitAroundSelectionEnabled_{true};
    std::optional<QVector3D> selectionOrbitDragPivot_{};
    bool selectionOrbitDragRejected_{};
    std::int32_t hoverPickSector_{-1};
    std::int32_t hoverPickSurface_{-1};
    std::int32_t hoverPickThing_{-1};
    std::int32_t hoverPickLight_{-1};

    struct ViewportPickCycleState
    {
        QPoint widgetPosition{};
        PickMode pickMode{PickMode::Auto};
        SurfaceSubMode surfaceSubMode{SurfaceSubMode::Face};
        TransformTool transformTool{TransformTool::Select};
        Qt::KeyboardModifiers selectionModifiers{Qt::NoModifier};
        std::uint64_t viewSerial{};
        std::chrono::steady_clock::time_point timestamp{};
        bool valid{};
    };

    [[nodiscard]] bool continuePickCycle(
        const QPoint& widgetPosition,
        PickMode mode,
        Qt::KeyboardModifiers selectionModifiers) noexcept;
    void resetPickCycle() noexcept;

    ViewportPickCycleState pickCycleState_{};
    std::uint64_t viewSerial_{};
    std::uint64_t thingLodCheckedViewSerial_{std::numeric_limits<std::uint64_t>::max()};
    std::array<double, 4> thingLodCheckedHeaderDistances_{};
    bool thingLodHeaderChecked_{};
    std::vector<std::int32_t> multiLodThingIndices_{};

    struct PeelCache
    {
        QPoint pos{};
        PickMode mode{PickMode::Auto};
        std::uint64_t viewSerial{};
        int geoMin{};
        bool cullBackFaces{};
        std::vector<PickHit> hits{};
    };

    bool levelPeelCacheValid_{};
    PeelCache levelPeelCache_{};
    bool thingPeelCacheValid_{};
    PeelCache thingPeelCache_{};

    std::unique_ptr<QOpenGLShaderProgram> program_{};
    std::unique_ptr<QOpenGLShaderProgram> thickLineProgram_{};
    std::unique_ptr<QOpenGLShaderProgram> gridProgram_{};
    std::unique_ptr<QOpenGLShaderProgram> frameGhostProgram_{};
    GLuint dynamicLightUniformBuffer_{};
    bool dynamicLightUniformBufferReady_{};
    bool dynamicLightUniformBufferWarningEmitted_{};
    std::unique_ptr<QOpenGLVertexArrayObject> lineVao_{};
    std::unique_ptr<QOpenGLBuffer> lineVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> highlightLineVao_{};
    std::unique_ptr<QOpenGLBuffer> highlightLineVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> gridQuadVao_{};
    std::unique_ptr<QOpenGLVertexArrayObject> gridLineVao_{};
    std::unique_ptr<QOpenGLBuffer> gridLineVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> triVao_{};
    std::unique_ptr<QOpenGLBuffer> triVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> translucentTriVao_{};
    std::unique_ptr<QOpenGLBuffer> translucentTriVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> thingGizmoTriVao_{};
    std::unique_ptr<QOpenGLBuffer> thingGizmoTriVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> thingGizmoPickTriVao_{};
    std::unique_ptr<QOpenGLBuffer> thingGizmoPickTriVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> lightGizmoTriVao_{};
    std::unique_ptr<QOpenGLBuffer> lightGizmoTriVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> spriteTriVao_{};
    std::unique_ptr<QOpenGLBuffer> spriteTriVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> outlineTriVao_{};
    std::unique_ptr<QOpenGLBuffer> outlineTriVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> pointVao_{};
    std::unique_ptr<QOpenGLBuffer> pointVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> highlightPointVao_{};
    std::unique_ptr<QOpenGLBuffer> highlightPointVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> architecturalLightingPointVao_{};
    std::unique_ptr<QOpenGLBuffer> architecturalLightingPointVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> transformGizmoLineVao_{};
    std::unique_ptr<QOpenGLBuffer> transformGizmoLineVbo_{};
    std::unique_ptr<QOpenGLVertexArrayObject> frameGhostLineVao_{};
    std::unique_ptr<QOpenGLBuffer> frameGhostLineVbo_{};
    std::array<std::unique_ptr<QOpenGLBuffer>, 64u> frameGhostInstanceVbos_{};

    int triVboAllocatedBytes_{};
    int lineVboAllocatedBytes_{};
    int pointVboAllocatedBytes_{};
    int translucentTriVboAllocatedBytes_{};
    int spriteTriVboAllocatedBytes_{};
    int highlightLineVboAllocatedBytes_{};
    int outlineTriVboAllocatedBytes_{};
    int highlightPointVboAllocatedBytes_{};
    int architecturalLightingPointVboAllocatedBytes_{};
    int thingGizmoTriVboAllocatedBytes_{};
    int thingGizmoPickTriVboAllocatedBytes_{};
    int lightGizmoTriVboAllocatedBytes_{};
    int transformGizmoLineVboAllocatedBytes_{};
    int frameGhostLineVboAllocatedBytes_{};
    std::array<int, 64u> frameGhostInstanceVboAllocatedBytes_{};

    struct FrameGhostInstance
    {
        std::array<float, 16u> matrix{};
        std::int32_t valueIndex{-1};
    };
    struct FrameOwnerPoseCache
    {
        bool posesValid{};
        bool posesComplete{};
        bool modelResolved{};
        bool ghostInstancesDirty{true};
        std::int32_t instanceBufferSlot{-1};
        std::size_t poseBytes{};
        std::uint64_t lastUseOrdinal{};
        std::string modelName{};
        std::shared_ptr<const assets::Model3do> model{};
        std::vector<render::frame_ghost::FramePose> poses{};
        std::optional<render::frame_ghost::FramePose> overflowFramePose{};
        std::int32_t failedOverflowValueIndex{-1};
        std::vector<FrameGhostInstance> ghostInstances{};
    };
    struct FrameModelTopologyCache
    {
        std::shared_ptr<const assets::Model3do> model{};
        render::frame_ghost::ModelTopology topology{};
        std::vector<render::frame_ghost::LocalVertex> lineVertices{};
        render::frame_ghost::LocalVertex boundsMin{};
        render::frame_ghost::LocalVertex boundsMax{};
        bool haveFiniteBounds{};
        std::uint64_t lastUseOrdinal{};
    };
    struct FrameGhostInstanceBufferSlot
    {
        std::int32_t ownerThingIndex{-1};
        std::uint64_t lastUseOrdinal{};
    };
    static constexpr std::size_t kMaximumFramePoseOwnerCaches = 64u;
    static constexpr std::size_t kMaximumFrameTopologyCaches = 8u;
    static constexpr std::size_t kMaximumFramePoseCacheBytes = 32u * 1024u * 1024u;
    static constexpr std::size_t kMaximumFrameTopologyCacheBytes = 64u * 1024u * 1024u;
    static constexpr std::size_t kMaximumFrameGhostGpuBytes = 64u * 1024u * 1024u;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    std::size_t frameGhostTopologyEdgeLimitForTest_{
        render::frame_ghost::kMaximumGhostEdgeCount};
#endif
    std::unordered_map<std::int32_t, FrameOwnerPoseCache> frameOwnerPoseCaches_{};
    std::unordered_map<const assets::Model3do*, FrameModelTopologyCache>
        frameModelTopologyCaches_{};
    std::array<FrameGhostInstanceBufferSlot, 64u> frameGhostInstanceBufferSlots_{};
    std::shared_ptr<const assets::Model3do> frameGhostUploadedModel_{};
    std::size_t framePoseCacheBytes_{};
    std::size_t frameTopologyCacheBytes_{};
    std::size_t frameGhostGpuBytes_{};
    std::size_t frameGhostLineVertexCount_{};
    std::uint64_t frameRenderCacheUseOrdinal_{};

    [[nodiscard]] FrameOwnerPoseCache* ensureFrameOwnerPoseCache(
        std::int32_t thingIndex);
    [[nodiscard]] std::shared_ptr<const assets::Model3do> ensureFrameOwnerModel(
        FrameOwnerPoseCache& ownerCache,
        const Thing& thing);
    [[nodiscard]] FrameModelTopologyCache* ensureFrameModelTopologyCache(
        const std::shared_ptr<const assets::Model3do>& model);
    [[nodiscard]] const render::frame_ghost::FramePose* findFramePose(
        std::int32_t thingIndex,
        std::int32_t valueIndex);
    void invalidateFramePoseCaches(
        const std::vector<std::int32_t>& thingIndices,
        bool invalidateModelIdentity) noexcept;
    void invalidateAllFrameRenderCaches() noexcept;
    void drawFrameGhostBatch(
        FrameOwnerPoseCache& ownerCache,
        std::int32_t activeValueIndex,
        const QMatrix4x4& mvp);
    void initializeFrameGhostOpenGlResources();
    void releaseFrameGhostOpenGlResources() noexcept;
    int gridLineVboAllocatedBytes_{};

    // Camera-facing Thing type icons are static until the Thing set, visible
    // layers, icon size, or camera basis changes. Keep both their resolved
    // inherited type and uploaded vertices out of ordinary paint/selection
    // paths; property-panel transforms use one pick-id translation in the
    // shader instead of rebuilding every visible icon.
    struct ThingTypeGizmoEntry
    {
        QVector3D center{};
        QVector3D boundsMin{};
        QVector3D boundsMax{};
        std::string type{};
        std::uint32_t pickId{};
        std::int32_t thingIndex{ -1 };
        bool archThing{};
        QVector3D ownerPosition{};
        QVector3D archThingMarkerCenter{};
        int typeFirstVertex{ -1 };
        int archThingMarkerFirstVertex{ -1 };
    };
    struct ThingTypeGizmoBatch
    {
        std::string type{};
        int firstVertex{};
        int vertexCount{};
    };
    std::vector<ThingTypeGizmoEntry> thingTypeGizmoEntries_{};
    std::unordered_map<std::uint32_t, std::size_t> thingTypeGizmoEntryByPickId_{};
    std::unordered_set<std::int32_t> thingTypeGizmoDirtyOwners_{};
    std::vector<ThingTypeGizmoBatch> thingTypeGizmoBatches_{};
    std::vector<TriVertex> thingTypeGizmoVertices_{};
    std::size_t thingTypeGizmoIconVertexCount_{};
    std::size_t thingTypeGizmoArchThingMarkerVertexCount_{};
    QVector3D thingTypeGizmoCameraRight_{};
    QVector3D thingTypeGizmoCameraUp_{};
    float thingTypeGizmoHalfSize_{};
    float thingTypeGizmoLightHalfSize_{};
    float thingTypeGizmoArchitecturalHalfSize_{};
    QMatrix4x4 thingTypeGizmoPreviewMatrix_{};
    std::array<int, 256> thingTypeGizmoPreviewIds_{};
    int thingTypeGizmoPreviewIdCount_{};
    bool thingTypeGizmoMetadataValid_{};
    bool thingTypeGizmoCacheValid_{};
    std::uint64_t thingTypeGizmoMetadataBuildCount_{};
    std::uint64_t thingTypeGizmoBuildCount_{};

    // ARCH owner markers are a separate overlay from ordinary Thing type
    // icons. Their anchors are also the authoritative marker-picking points;
    // keeping a dedicated cache prevents an ARCH-mode filter from replacing
    // the user's ordinary CO icons and allows the persisted ARCH visibility
    // preference to work outside the editing submode.
    struct ArchitecturalLightingOwnerGizmoEntry
    {
        std::int32_t thingIndex{-1};
        std::uint32_t pickId{};
        QVector3D center{};
        QVector3D boundsMin{};
        QVector3D boundsMax{};
        QVector3D ownerPosition{};
    };
    std::vector<ArchitecturalLightingOwnerGizmoEntry>
        architecturalLightingOwnerGizmoEntries_{};
    std::unordered_map<std::int32_t, std::size_t>
        architecturalLightingOwnerGizmoEntryByThing_{};
    std::vector<TriVertex> architecturalLightingOwnerGizmoVertices_{};
    QVector3D architecturalLightingOwnerGizmoCameraRight_{};
    QVector3D architecturalLightingOwnerGizmoCameraUp_{};
    float architecturalLightingOwnerGizmoHalfSize_{};
    float architecturalLightingOwnerThingTypeHalfSize_{};
    float architecturalLightingOwnerThingLightHalfSize_{};
    bool architecturalLightingOwnerGizmoMetadataValid_{};
    bool architecturalLightingOwnerGizmoCacheValid_{};
    bool architecturalLightingOwnerGizmoTransformPreview_{};
    std::uint64_t architecturalLightingOwnerGizmoBuildCount_{};
    std::vector<render::LightGizmoEntry> thingLightGizmoEntriesCache_{};
    bool thingLightGizmoEntriesCacheValid_{};

    std::uint64_t gridCacheViewSerial_{};
    int gridCacheWidgetW_{};
    int gridCacheWidgetH_{};
    bool gridCacheValid_{};

    std::vector<LineVertex> gridLineVertices_{};
    int gridLineVertexCount_{};
    int gridLineMinorPointVertexCount_{};
    int gridLineMinorVertexCount_{};
    int gridLineMajorVertexCount_{};
    int gridLineAxisAVertexCount_{};
    int gridLineAxisBVertexCount_{};

    std::unordered_map<std::string, TextureInfo> textures_{};
    std::unordered_map<std::string, PreparedTextureSource> preparedTextureSources_{};
    std::unordered_map<std::string, assets::CmpPalette> palettes_{};
    std::unordered_map<std::string, assets::CmpFile> cmpFiles_{};
    std::unordered_set<std::string> missingPalettes_{};
    std::unordered_set<std::string> missingMats_{};
    ui::detail::PreviewResourceDiskWatcher* resourceDiskWatcher_{};
    std::function<bool()> resourceArchiveReloadCallback_{};

    std::unordered_map<std::string, std::shared_ptr<const assets::Model3do>> models3do_{};
    struct Model3doLodCache
    {
        std::vector<std::byte> sourceBytes{};
        bool sourceRead{};
        bool primedSource{};
        std::array<std::shared_ptr<const assets::Model3do>, 4> models{};
        std::array<bool, 4> attempted{};
    };
    std::unordered_map<std::string, Model3doLodCache> model3doLods_{};
    std::unordered_set<std::string> missingModels3do_{};
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    std::uint64_t model3doResourceReadCount_{};
    std::uint64_t materialResourceReadCount_{};
#endif
    struct ModelBounds
    {
        TVector min{};
        TVector max{};
        bool valid{};
    };
    std::unordered_map<std::string, ModelBounds> modelBounds3do_{};
    std::unordered_map<std::string, assets::SprFile> sprites_{};
    std::unordered_set<std::string> missingSprites_{};

    struct SpriteInstance
    {
        std::string key{};
        std::string material{};
        std::string colormap{};
        bool uvNormalized{};
        int type{};
        int lightMode{};
        std::int32_t sectorIndex{ -1 };
        float width{};
        float height{};
        float rollDeg{};
        QVector4D extraLightRGBA{ 0.0f, 0.0f, 0.0f, 1.0f };
        QVector3D regularSelfLight{};
        QVector3D offsetXYZ{ 0.0f, 0.0f, 0.0f };
        QVector3D position{ 0.0f, 0.0f, 0.0f };
        QVector3D basisRight{ 1.0f, 0.0f, 0.0f };   // editor (non-billboard) basis
        QVector3D basisForward{ 0.0f, 1.0f, 0.0f }; // forward/axis used by type 0 and 2 sprites
        QVector3D basisUp{ 0.0f, 0.0f, 1.0f };
        bool invalidPlacementPreview{};
        // Stable owner for transient placement sprites. rebuildSpriteVertices()
        // sorts instances by material, so vector offsets cannot identify a
        // preview after the first upload.
        std::int32_t placementPreviewIndex{ -1 };
        float pickId{};
        float pickGeo{};
      };

    std::vector<SpriteInstance> spriteInstances_{};
    std::vector<TriVertex> spriteTriVertices_{};
    std::vector<TriBatch> spriteTriBatches_{};
    // Stable instance ownership published by rebuildSpriteVertices().  Sprite
    // placement and transform previews can then regenerate the owner's six
    // vertices without scanning/rebuilding the shared SPR buffer.
    std::vector<DrawRange> spriteTriRangeByInstanceIndex_{};
    std::vector<std::size_t> spriteTriBatchByInstanceIndex_{};
    std::vector<ThingBounds> spriteInstanceBounds_{};
    std::vector<std::vector<std::size_t>> spriteInstanceIndicesByBatch_{};
    std::vector<std::array<std::size_t, 6>> spriteBatchExtremeOwners_{};
    std::vector<std::uint8_t> spriteBatchExtremeOwnersValid_{};
    std::unordered_map<int, std::vector<std::size_t>> spriteInstanceIndicesByPickId_{};
    std::uint64_t spriteBatchBoundsVisitCount_{};
    std::uint64_t spriteInstanceVertexGenerationCount_{};
    bool spriteReuseExistingVertices_{};
    int spriteTriVertexCount_{};
    bool spriteTrianglesDirty_{ true };
    bool spriteHasType0Billboards_{};

    [[nodiscard]] bool buildSpriteInstanceVertices(
        const SpriteInstance& instance,
        std::array<TriVertex, 6>& vertices) const;
    void refitSpriteBatch(
        std::size_t batchIndex,
        const std::vector<std::size_t>* changedInstanceIndices = nullptr);

    std::vector<PickObject> pickObjects_{};
    std::unordered_map<std::uint64_t, std::uint32_t> surfacePickIds_{};
    std::vector<std::uint32_t> thingPickIds_{};

    std::unique_ptr<QOpenGLShaderProgram> pickProgram_{};
    std::unique_ptr<QOpenGLFramebufferObject> pickFbo_{};
    int pickFboWidth_{};
    int pickFboHeight_{};

    struct ToastState
    {
        QString text{};
        std::chrono::steady_clock::time_point shownAt{};
        std::chrono::steady_clock::time_point expires{};
        int durationMs{};
        ToastPlacement placement{ ToastPlacement::TopLeft };
    };

    static constexpr std::size_t kToastGroupCount = static_cast<std::size_t>(ToastGroup::Count);
    [[nodiscard]] static constexpr ViewportBannerKind viewportBannerKindForToastGroup(
        const ToastGroup group) noexcept
    {
        switch ( group )
        {
        case ToastGroup::General:
            return ViewportBannerKind::general;
        case ToastGroup::Projection:
            return ViewportBannerKind::projection;
        case ToastGroup::FlySpeed:
            return ViewportBannerKind::flySpeed;
        case ToastGroup::Error:
            return ViewportBannerKind::error;
        case ToastGroup::PlacementYaw:
            return ViewportBannerKind::placementYaw;
        case ToastGroup::Count:
            break;
        }
        return ViewportBannerKind::general;
    }
    std::array<ToastState, kToastGroupCount> toastStates_{};
    std::unique_ptr<QTimer> toastTimer_{};

    struct ViewAnimState
    {
        bool active{};
        float startYawDeg{};
        float startPitchDeg{};
        float startRollDeg{};
        float targetYawDeg{};
        float targetPitchDeg{};
        float targetRollDeg{};
        QVector3D startPan{};
        QVector3D targetPan{};
        float startDistance{};
        float targetDistance{};
        float startOrthoScale{};
        float targetOrthoScale{};
        int durationMs{ 180 };
        // View to Grid is a programmatic camera alignment to the authored
        // grid. Its animation must not look like manual camera rotation to
        // the optional "Reset align/center on camera rotate" policy.
        bool preserveGridOverrides{};
        std::chrono::steady_clock::time_point startedAt{};
    };

    void startViewAnimation(float yawDeg, float pitchDeg, float rollDeg, int durationMs);
    void cancelViewAnimation() noexcept;
    void tickViewAnimation();
    static float lerpAngleDeg(float aDeg, float bDeg, float t) noexcept;

    ViewAnimState viewAnim_{};
    std::unique_ptr<QTimer> viewAnimTimer_{};

    struct FlyNavState
    {
        bool active{};
        bool forward{};
        bool backward{};
        bool left{};
        bool right{};
        bool down{};
        bool up{};
        bool fast{};
        bool slow{};
        bool fastKeyHeld{};
        bool slowKeyHeld{};
        float speedMultiplier{ 1.0f };
        std::chrono::steady_clock::time_point lastTick{};
    };

    void startFlyNavigation(Qt::KeyboardModifiers modifiers);
    void stopFlyNavigation();
    void tickFlyNavigation();
    void flyLook(const QPointF& mouseDelta);

    FlyNavState flyNav_{};
    std::unique_ptr<QTimer> flyTimer_{};
    QCursor flyCursor_{};
    QCursor prevCursor_{};
    bool havePrevCursor_{};
    // Four modifier bits admit at most fifteen distinct nonempty chords.
    // Decode only when bindings change; input refresh and camera ticks allocate nothing.
    std::array<Qt::KeyboardModifiers, 15> flyFastModifierAlternatives_{ Qt::ShiftModifier };
    std::array<Qt::KeyboardModifiers, 15> flySlowModifierAlternatives_{ Qt::ControlModifier };
    std::size_t flyFastModifierAlternativeCount_{ 1 };
    std::size_t flySlowModifierAlternativeCount_{ 1 };
    Qt::KeyboardModifiers flySpeedHeldModifiers_{};

    struct ViewportMouseGesture
    {
        Qt::MouseButton button{ Qt::NoButton };
        ViewportMouseAction action{ ViewportMouseAction::none };
        Qt::KeyboardModifiers activationModifiers{};
    };
    ViewportNavigationSettings viewportNavigationSettings_{};
    ViewportBannerSettings viewportBannerSettings_{};
    ViewportMouseGesture viewportMouseGesture_{};
    const ViewportNavigationSettings& effectiveViewportNavigationSettings() const noexcept;
    QPointF viewportCameraMouseDelta(const QPoint& delta) const noexcept;
    void orbitPanPixels(const QPointF& deltaPixels);
    Qt::MouseButton viewportMouseSemanticButton() const noexcept;
    Qt::KeyboardModifiers viewportMouseSemanticModifiers(
        Qt::KeyboardModifiers modifiers) const noexcept;

    void buildCameraMatrices(QMatrix4x4& proj, QMatrix4x4& view, QVector3D& eye, QVector3D& target, float& nearPlane, float& farPlane) noexcept;
    [[nodiscard]] render::TransformGridFrame activeTransformGridFrame(
        const QMatrix4x4& cameraView,
        const QMatrix4x4& mvp,
        const QMatrix4x4& inverseMvp,
        const QVector3D& eye,
        const QVector3D& target,
        bool orthographic) const noexcept;
    void captureGridOverrideCameraBasis();
    void captureStagedCleaveContext(bool surfaceMode);
    void captureStagedCleaveLineCamera(StagedCleaveLine& line) const noexcept;
    bool restoreStagedCleaveLineCamera(std::size_t index);
    [[nodiscard]] bool stagedCleaveTargetsMatchCurrent(bool surfaceMode) const;
    [[nodiscard]] bool stagedCleaveTargetsMatchContext(
        const StagedCleaveContext& context) const;
    [[nodiscard]] bool stagedCleaveContextMatchesCurrent(bool surfaceMode);
    [[nodiscard]] bool stagedCleaveLinesFormSingleOpenChain() const noexcept;
    [[nodiscard]] bool stagedCleaveLinesFormSingleClosedChain() const noexcept;
    [[nodiscard]] TVector stagedCleaveChainOriginPoint(
        const std::vector<StagedCleaveLine>& lines) const noexcept;
    [[nodiscard]] TVector stagedCleaveChainOriginIncomingDirection(
        const std::vector<StagedCleaveLine>& lines) const noexcept;
    [[nodiscard]] bool stagedCleaveChainOriginIsCanonicalTailSecond() const noexcept;
    void initializeStagedCleaveLineFrame(
        StagedCleaveLine& line,
        const TVector& preferredAngleReference) const noexcept;
    void refreshStagedCleaveLinePlane(StagedCleaveLine& line) const noexcept;
    [[nodiscard]] TVector snapStagedCleaveEndpointToAngleStep(
        const TVector& fixed,
        const TVector& candidate,
        const TVector& constructionPlaneNormal,
        const TVector& fallbackReference) const noexcept;
    bool reshapeStagedCleaveEndpoint(
        std::size_t index,
        std::int32_t endpoint,
        const TVector& position);
    bool reshapeStagedCleaveEndpointIn(
        std::vector<StagedCleaveLine>& lines,
        std::size_t index,
        std::int32_t endpoint,
        const TVector& position) const;
    bool disjointStagedCleaveEndpointIn(
        std::vector<StagedCleaveLine>& lines,
        std::size_t index,
        std::int32_t endpoint) const;
    bool reshapeStagedCleaveEndpointWithOptionalDisjoint(
        std::vector<StagedCleaveLine>& lines,
        std::size_t index,
        std::int32_t endpoint,
        const TVector& position,
        bool disjointSelected) const;
    bool translateStagedCleavePlane(
        std::size_t index,
        const TVector& translation,
        const StagedCleavePlaneDrag* drag = nullptr);
    void setSelectedStagedCleaveLines(
        std::vector<std::int32_t> lines,
        std::int32_t activeLine = -1,
        bool publishState = true);
    void applyStagedCleaveClickSelection(
        std::int32_t line,
        Qt::KeyboardModifiers modifiers);
    [[nodiscard]] bool stagedCleaveLineSelected(std::int32_t line) const noexcept;
    bool reconnectStagedCleaveLineLinks(std::size_t index);
    [[nodiscard]] StagedCleaveDraftState stagedCleaveDraftState() const;
    [[nodiscard]] const std::vector<StagedCleaveLine>&
    stagedCleaveGestureBaselineLines() const noexcept;
    [[nodiscard]] static bool stagedCleaveDraftStatesEqual(
        const StagedCleaveDraftState& left,
        const StagedCleaveDraftState& right) noexcept;
    [[nodiscard]] static std::size_t stagedCleaveDraftStateOwnedBytes(
        const StagedCleaveDraftState& state) noexcept;
    [[nodiscard]] std::size_t stagedCleaveDraftStateCurrentOwnedBytesEstimate() const noexcept;
    [[nodiscard]] std::size_t stagedCleaveHistoryStorageBytes() const noexcept;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    [[nodiscard]] std::size_t cleaveRecoveryLiveOwnedArraysForTest() const noexcept;
#endif
    void noteStagedCleaveAuthoredChange() noexcept;
    bool beginStagedCleaveHistoryGesture();
    bool finishStagedCleaveHistoryGesture();
    bool prepareStagedCleaveGroupTransform();
    bool updateStagedCleaveGroupTransform(double angleDegrees, double scale);
    bool applyStagedCleaveWheelTransform(double steps);
    void clearStagedCleaveTransformHud() noexcept;
    void startStagedCleaveTransformHudExpiry();
    void scheduleStagedCleaveTransformHudPresentationWake();
    bool applyStagedCleaveShapeWheelTransform(
        double steps, bool rotate, const QPoint& position,
        Qt::KeyboardModifiers modifiers);
    bool stagedCleaveWheelTargetsMatchContext() const noexcept;
    void discardStagedCleaveHistoryGesture() noexcept;
    bool restoreAndDiscardStagedCleaveHistoryGesture();
    void resetStagedCleaveHistory() noexcept;
    void suspendStagedCleaveInteraction();
    void validateRetainedStagedCleaveReferences();
    void normalizeStagedCleaveTransformMode() noexcept;
    [[nodiscard]] RecoveryState::CleaveWorkspace currentStagedCleaveWorkspaceOptions() const;
    [[nodiscard]] RecoveryState::CleaveWorkspace takeCurrentStagedCleaveWorkspace();
    void installCurrentStagedCleaveWorkspace(
        RecoveryState::CleaveWorkspace&& workspace, bool historyActive);
    void selectStagedCleaveWorkspace(CleaveWorkspaceFamily family);
    void noteStagedCleaveAuthoredChange(CleaveWorkspaceFamily family) noexcept;
    void installStagedCleaveDraftState(const StagedCleaveDraftState& state);
    void installStagedCleaveDraftState(StagedCleaveDraftState&& state);
    [[nodiscard]] std::optional<StagedCleaveShapePlacementFrame>
    stagedCleaveShapePlacementFrameAt(
        const QPoint& position,
        Qt::KeyboardModifiers modifiers);
    bool appendStagedCleaveShape(
        const StagedCleaveShapePlacementFrame& frame);
    [[nodiscard]] QRect stagedCleaveOrderLabelRect(
        std::int32_t line,
        const QMatrix4x4& mvp) const;
    [[nodiscard]] QRect stagedCleaveOrderLabelRectForSegment(
        const TVector& first,
        const TVector& second,
        std::int32_t rank,
        const QMatrix4x4& mvp) const;
    void rebuildStagedCleaveRankCache();
    bool beginStagedCleaveOrderEdit(std::int32_t line, const QRect& rect);
    void finishStagedCleaveOrderEdit(bool commit, bool returnFocus = true);
    void trimStagedCleaveHistory();
    [[nodiscard]] std::optional<render::CleaveSnapCandidate> closestCleaveSnapTarget(
        const QPoint& widgetPosition,
        const QMatrix4x4& mvp,
        bool surfaceMode,
        double tolerancePx,
        std::int32_t excludedStagedLine = -1) const;
    [[nodiscard]] std::vector<render::CleaveSnapCandidate> cleaveSnapTargets(
        const QMatrix4x4& mvp,
        bool surfaceMode,
        std::int32_t excludedStagedLine = -1) const;
    [[nodiscard]] std::vector<render::CleaveSnapCandidate> cleaveGeometrySnapTargets(
        const QMatrix4x4& mvp,
        bool surfaceMode) const;
    void updateCleaveSnapGuideVisibility(Qt::KeyboardModifiers modifiers);
    void refreshCleaveSnapPreview(
        const QPoint& widgetPosition,
        Qt::KeyboardModifiers modifiers);
    bool attachStagedSectorCleaveLineToCameraEdge(std::size_t index);
    [[nodiscard]] std::vector<std::vector<TVector>> stagedCleavePlanePolygons(
        const StagedCleaveLine& line) const;
    void markViewChanged(bool preserveGridOverrides = false) noexcept;
    void fitViewToLevel(float yawDegrees, float pitchDegrees);
    void deferOpenGlResourceCleanup() noexcept;
    void releaseOpenGlResources() noexcept;

    QOpenGLTexture* getLightTexture();
    QOpenGLTexture* getLightTexture(render::LightGizmoKind kind);
    QOpenGLTexture* getArchitecturalLightingTexture();
    [[nodiscard]] QVector3D lightGizmoDisplayPositionForEntry(
        const render::LightGizmoEntry& entry,
        QVector3D cameraUp,
        float baseHalfSize,
        bool entryPositionAlreadyPreviewed = false) const noexcept;
    [[nodiscard]] QVector3D lightGizmoDisplayPositionForEntry(
        const render::LightGizmoEntry& entry,
        const QMatrix4x4& view) const noexcept;
    void drawLightGizmos(const QMatrix4x4& mvp);
    void drawSelectedLightRange(const QMatrix4x4& mvp);
    void drawThingGizmos(const QMatrix4x4& mvp);
    QOpenGLTexture* getGizmoTexture(const std::string& type);
    QOpenGLTexture* getArchThingGizmoTexture();
    std::unique_ptr<QOpenGLTexture> lightTexture_;
    std::unique_ptr<QOpenGLTexture> thingLightTexture_;
    std::unique_ptr<QOpenGLTexture> thingHeadLightTexture_;
    std::unique_ptr<QOpenGLTexture> sectorLightTexture_;
    std::unique_ptr<QOpenGLTexture> architecturalLightingTexture_;
    std::unique_ptr<QOpenGLTexture> archThingGizmoTexture_;
    std::unique_ptr<QOpenGLTexture> surfaceWireColorTexture_;
    QMetaObject::Connection contextCleanupConnection_{};
    int surfaceWireColorMaxTextureSize_{};
    int surfaceWireColorTextureLayerCount_{ -1 };
    bool surfaceWireColorTextureDirty_{ true };
    std::unordered_map<std::string, std::unique_ptr<QOpenGLTexture>> gizmoTextures_;
    std::unordered_map<std::string, QColor> thingTypeColors_;
};

} // namespace sed
