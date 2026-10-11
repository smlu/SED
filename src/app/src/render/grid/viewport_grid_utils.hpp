#pragma once

#include "sed/geometry.hpp"

#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>

#include <QColor>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

namespace sed::render
{

inline constexpr float kGridLineWidthMinPx = 0.5f;
inline constexpr float kGridLineWidthMaxPx = 10.0f;
inline constexpr float kGridMinorDotSizeMinPx = 0.5f;
inline constexpr float kGridMinorDotSizeMaxPx = 10.0f;
inline constexpr std::size_t kGridMinorPointVertexBudget = 65536;

/** Compact line vertex used by CPU-generated viewport grid fallback geometry. */
struct GridLineVertex
{
    float x{};
    float y{};
    float z{};
    float pickId{};
};

/** Inputs needed to build the cached line-grid fallback for one viewport frame. */
struct GridLineBuildParams
{
    QVector3D axisA{ 1.0f, 0.0f, 0.0f };
    QVector3D axisB{ 0.0f, 1.0f, 0.0f };
    QVector3D coordOrigin{};
    QVector3D viewCenter{};
    float halfVisible{ 1.0f };
    double majorSpacing{ 1.0 };
    double minorSpacing{ 0.1 };
    std::int64_t majorEvery{ 10 };
    bool minorDots{};
};

/** CPU grid line output grouped in the exact draw order used by GlView. */
struct GridLineBuild
{
    std::vector<GridLineVertex> vertices{};
    int minorPointVertexCount{};
    int minorVertexCount{};
    int majorVertexCount{};
    int axisAVertexCount{};
    int axisBVertexCount{};
};

/** Effective major/minor spacing after applying the viewport's adaptive zoom scale. */
struct EffectiveGridSpacing
{
    double major{ 1.0 };
    double minor{ 0.1 };
    std::int64_t majorEvery{ 10 };
};

/** User-visible grid/scene occlusion modes. */
enum class GridDrawMode
{
    Occluded,
    VisibleThroughWalls,
    AlwaysBehind,
};

/** Draw ordering and depth behavior selected by the grid occlusion mode. */
struct GridDrawPolicy
{
    bool drawBeforeGeometry{};
    bool depthTestScene{};
    /**
     * Tiny NDC pull used only by the normal late, depth-tested grid pass.
     * It resolves the otherwise undefined winner when the analytic grid plane
     * is exactly coplanar with an authored surface without turning the grid
     * into a through-walls overlay.
     */
    float coplanarNdcBias{};
};

/** Screen-space limits for the colored grid axes in the analytic shader. */
struct GridAxisFilterPolicy
{
    /** Directional color is enough emphasis; do not amplify the requested grid width. */
    float lineWidthScale{ 1.0f };
    /** Thick grids must not turn the directional axes into broad colored bands. */
    float maximumLineWidthPx{ 4.0f };
};

/** Homogeneous screen-line equation for one projected world-space grid axis. */
struct GridProjectedAxisLine
{
    QVector3D equation{};
    bool valid{};
};

/**
 * Screen-space grid fade band immediately before the camera's clip plane.
 *
 * The ratios are measured along each reconstructed view ray rather than in
 * non-linear depth-buffer space.  That keeps an equally sized, front-facing
 * grid equally bright in perspective and orthographic projections while still
 * softening the final few percent before hard clipping.
 */
struct GridClipFadePolicy
{
    float startRatio{ 0.96f };
    float endRatio{ 0.995f };
};

/**
 * Screen-space anti-alias band for repeated grid lines.
 *
 * A repeated line pattern becomes phase-unstable before it reaches the formal
 * two-pixel Nyquist limit (especially while the camera is moving). Minor lines
 * therefore disappear early. Major lines hand off to exact 10x subdivisions,
 * like a DCC viewport, instead of leaving one increasingly dense fixed scale
 * at a grazing angle. The policy is deliberately depth-independent: the local
 * screen footprint, not perspective depth or opacity, selects the visible LOD.
 */
struct GridFootprintFadePolicy
{
    float minorHiddenPixelsPerCell{ 4.0f };
    float minorFullyVisiblePixelsPerCell{ 12.0f };
    float majorHiddenPixelsPerCell{ 3.0f };
    float majorFullyVisiblePixelsPerCell{ 10.0f };
    int majorHierarchyLevels{ 4 };
    double majorHierarchyRatio{ 10.0 };
};

/**
 * Point-lattice footprint thresholds derived from a circular dot's radial
 * coverage support and the existing minor-grid density floor. The hidden
 * period is at least one support radius, which bounds the fixed +/-1
 * lattice-candidate search; dots are fully visible by at least four support
 * radii, where neighboring glyphs are clearly separated.
 */
struct GridPointFootprintFadeBand
{
    float hiddenPixelsPerCell{};
    float fullyVisiblePixelsPerCell{};
};

/** Frozen grid lattice used by one transform drag. */
struct TransformGridFrame
{
    QVector3D origin{};
    QVector3D axisA{ 1.0f, 0.0f, 0.0f };
    QVector3D axisB{ 0.0f, 1.0f, 0.0f };
    QVector3D normal{ 0.0f, 0.0f, 1.0f };
    double spacing{ 0.1 };
    bool valid{};
};

/** Stable XYZ grid orientation controls used by Map Settings. */
struct GridTransformAngles
{
    float yawDeg{};
    float pitchDeg{};
    float rollDeg{};
};

/** Orbit-camera angles whose screen right/up axes match an authored grid. */
struct GridViewAngles
{
    float yawDeg{};
    float pitchDeg{};
    float rollDeg{};
    bool valid{};
};

/** Camera-facing frame produced from a view basis without NaN/near-pole axes. */
struct ViewAlignedGridFrame
{
    QVector3D axisA{1.0f, 0.0f, 0.0f};
    QVector3D axisB{0.0f, 1.0f, 0.0f};
    QVector3D normal{0.0f, 0.0f, 1.0f};
    bool valid{};
};

/**
 * Display-only plane used when a view-aligned authored grid reaches or crosses
 * the camera near plane. The proxy keeps the visible lattice phase anchored to
 * the authored origin, but deliberately does not change the transform/snapping
 * plane owned by GlView.
 */
struct ViewAlignedGridDisplayPlane
{
    QVector3D planePoint{};
    QVector3D coordOrigin{};
    bool proxied{};
};

/** Converts Z-Y-X Euler angles (degrees) into an orthonormal grid basis. */
bool gridAxesFromEulerAngles(
    const GridTransformAngles& angles,
    QVector3D& axisA,
    QVector3D& axisB,
    QVector3D& normal) noexcept;

/** Recovers deterministic Z-Y-X Euler angles from an orthonormal grid basis. */
GridTransformAngles gridEulerAnglesFromAxes(
    const QVector3D& axisA,
    const QVector3D& axisB) noexcept;

/**
 * Resolves the orbit view perpendicular to a grid without changing its origin.
 * Axis A becomes screen-right and axis B becomes screen-up, matching the
 * original JED "View to Grid" contract even for rolled/custom grid planes.
 */
GridViewAngles viewAnglesForGridAxes(
    const QVector3D& axisA,
    const QVector3D& axisB) noexcept;

/** Sanitizes a camera right/up/forward basis for rotate-grid-to-view. */
ViewAlignedGridFrame makeViewAlignedGridFrame(
    const QVector3D& cameraRight,
    const QVector3D& cameraUp,
    const QVector3D& cameraForward) noexcept;

/**
 * Keeps an optional visual view-aligned grid guide in front of the near plane.
 *
 * When the authored plane is already visible, both returned points remain the
 * authored origin. Otherwise the rendered plane moves along the normalized
 * view direction only; coordOrigin is the authored origin's normal projection
 * onto that proxy plane, so grid-line phase remains stable. This function is
 * render-only and must never be used to build a transform snapping frame.
 */
ViewAlignedGridDisplayPlane resolveViewAlignedGridDisplayPlane(
    const QVector3D& authoredOrigin,
    const QVector3D& cameraEye,
    const QVector3D& cameraForward,
    float nearPlane,
    bool keepVisible) noexcept;

/** Projection-independent orthographic world-units-per-pixel fallback. */
float orthographicGridUnitsPerPixel(float orthoScale, int viewportHeight) noexcept;

/**
 * Selects one finite positive camera footprint for adaptive grid spacing.
 *
 * The centre-ray plane measurement is preferred.  Edge-on planes may not
 * intersect those rays, so callers then use projection-derived scale,
 * orthographic scale/height, and finally the renderer's deterministic
 * last-resort value in that order.  Rendering, Move, and Cleave all use this
 * policy so they cannot silently choose different adaptive lattices.
 */
float resolveGridUnitsPerPixel(
    float planeMeasured,
    float projectionMeasured,
    bool orthographic,
    float orthoScale,
    int viewportHeight) noexcept;

/** Keeps a shader-grid footprint visible at extreme camera zoom. */
float gridMinimumVisibleHalfExtent(double majorSpacing) noexcept;

/**
 * Sanitizes a persisted/view-aligned pair into a right-handed orthonormal
 * frame.  View-aligned axes can come from recovery data, so callers must not
 * assume that they are finite, normalized, or mutually perpendicular.
 */
bool orthonormalizeGridAxes(
    const QVector3D& axisAInput,
    const QVector3D& axisBInput,
    QVector3D& axisA,
    QVector3D& axisB,
    QVector3D& normal) noexcept;

/** Per-drag hysteresis for the optional grid-plane stick-through behavior. */
struct GridPlaneStickState
{
    bool holding{};
    bool released{};
    double direction{};
    double contactTranslation{};
    double releaseOffset{};
};

/** Chooses the configured X/Y/Z color for a world-space grid axis. */
QColor gridAxisColorForDirection(
    const QVector3D& axis,
    const QColor& xColor,
    const QColor& yColor,
    const QColor& zColor);
/** Uses the default Night Blue axis colors. */
QColor gridAxisColorForDirection(const QVector3D& axis);

/** Rounds an arbitrary zoom scale to the 1/2/5/10 steps used by adaptive grid spacing. */
double niceGridStep(double raw) noexcept;

/** Resolves the exact major/minor spacing displayed at the current zoom. */
EffectiveGridSpacing effectiveGridSpacing(
    double baseMajor,
    double baseMinor,
    bool adaptive,
    double unitsPerPixel) noexcept;

/** Resolves the grid pass semantics without depending on the active view mode. */
GridDrawPolicy gridDrawPolicy(GridDrawMode mode) noexcept;

/** Returns the screen-filter limits used for the colored analytic grid axes. */
GridAxisFilterPolicy gridAxisFilterPolicy() noexcept;

/** Clamps the persisted/user-entered grid thickness to the renderer contract. */
float clampedGridLineWidthPx(float widthPx) noexcept;

/** Resolves a bounded colored-axis width independently of thick ordinary lines. */
float gridAxisLineWidthPx(float gridLineWidthPx) noexcept;

/** Converts the ordinary-grid logical-pixel setting to physical framebuffer pixels. */
float gridPhysicalLineWidthPx(float gridLineWidthPx, float devicePixelRatio) noexcept;

/** Converts the independently capped colored-axis width to physical framebuffer pixels. */
float gridPhysicalAxisLineWidthPx(float gridLineWidthPx, float devicePixelRatio) noexcept;

/** Clamps the logical-pixel diameter of an optional minor-grid point. */
float clampedGridMinorDotSizePx(float dotSizePx) noexcept;

/** Converts a logical minor-grid point diameter to physical framebuffer pixels. */
float gridPhysicalMinorDotSizePx(float dotSizePx, float devicePixelRatio) noexcept;

/** Mirrors the shader's radially symmetric, one-pixel-filtered point edge. */
float gridAnalyticPointCoverage(float distancePx, float diameterPx) noexcept;

/**
 * Evaluates the rotation-invariant, one-pixel box-filtered line profile used
 * by the analytic grid. Both arguments are physical framebuffer pixels.
 */
float gridAnalyticLineCoverage(float distancePx, float lineWidthPx) noexcept;

/** Mirrors the shader's rotation-invariant coordinate derivative. */
float gridEuclideanDerivative(float derivativeX, float derivativeY) noexcept;

/** Mirrors colored-axis RGB replacement without reducing existing grid alpha. */
QVector4D gridAxisReplacementComposite(
    const QVector4D& ordinaryColor,
    const QVector4D& axisColor,
    float axisCoverage) noexcept;

/**
 * Projects an infinite world line to a normalized homogeneous screen line.
 * The direction is transformed with w=0, avoiding a large `origin+direction`
 * addition before projection.
 */
GridProjectedAxisLine projectedGridAxisLine(
    const QMatrix4x4& mvp,
    const QVector3D& origin,
    const QVector3D& direction) noexcept;

/** Mirrors the shader's aspect/DPI-correct distance to a projected axis. */
float projectedGridAxisDistancePx(
    const GridProjectedAxisLine& line,
    const QVector2D& ndc,
    int viewportWidthPx,
    int viewportHeightPx) noexcept;

/** Returns the projection-neutral far-clip fade band used by the grid shader. */
GridClipFadePolicy gridClipFadePolicy() noexcept;

/** Evaluates the far-clip fade policy for focused render-policy tests. */
float gridClipFade(float hitDistance, float rayFarDistance) noexcept;

/** Returns the derivative/footprint fade bands used by the analytic grid shader. */
GridFootprintFadePolicy gridFootprintFadePolicy() noexcept;

/** Returns dot-footprint thresholds including half-pixel AA support and the grid's density floors. */
GridPointFootprintFadeBand gridPointFootprintFadeBand(float diameterPx) noexcept;

/** Evaluates the point-specific pixels-per-cell fade with the shader's smoothstep curve. */
float gridPointFootprintFade(float pixelsPerCell, float diameterPx) noexcept;

/**
 * Evaluates one pixels-per-cell footprint band with the shader's smoothstep
 * curve. `lineWidthPixels` is the physical framebuffer width after DPR
 * scaling. Wider requested lines hand off to a coarser scale earlier so their
 * larger coverage cannot collect into a distant solid/Moire band.
 */
float gridFootprintFade(
    float pixelsPerCell,
    float hiddenPixelsPerCell,
    float fullyVisiblePixelsPerCell,
    float lineWidthPixels = 1.0f) noexcept;

/** Returns an exact coarser major-grid spacing without changing the snap lattice. */
double gridHierarchySpacing(
    double baseMajorSpacing,
    int level,
    double hierarchyRatio = 10.0) noexcept;

/** Orthogonalizes a transform-grid frame and validates its spacing. */
TransformGridFrame makeTransformGridFrame(
    const QVector3D& origin,
    const QVector3D& axisA,
    const QVector3D& axisB,
    double spacing) noexcept;

/**
 * Snaps one active object reference to the grid lattice without changing its
 * signed distance from the grid plane (legacy JED drag semantics).
 */
QVector3D snapTransformTranslationToGrid(
    const QVector3D& referencePoint,
    const QVector3D& rawTranslation,
    const TransformGridFrame& frame) noexcept;

/** Double document-reference counterpart; only the relative result is presented as float. */
QVector3D snapTransformTranslationToGrid(
    const TVector& referencePoint,
    const QVector3D& rawTranslation,
    const TransformGridFrame& frame,
    TVector* documentTranslation = nullptr) noexcept;

/** Double authoring input preserves which side of a grid half-cell the cursor occupies. */
QVector3D snapTransformTranslationToGrid(
    const TVector& referencePoint,
    const TVector& rawTranslation,
    const TransformGridFrame& frame,
    TVector* documentTranslation = nullptr) noexcept;

/**
 * Snaps a point while keeping it on an interaction plane.
 *
 * Cleave authoring must remain on its face/construction plane, but its lattice
 * phase, orientation, and spacing must come from the same authoritative grid
 * frame used for drawing and transforms.  Parallel grid/interaction planes
 * therefore produce exactly the displayed lattice (including an authored
 * non-zero origin).  For an oblique interaction plane the grid axes are
 * projected deterministically into that plane.
 */
QVector3D snapPointToGridOnPlane(
    const QVector3D& point,
    const QVector3D& planePoint,
    const QVector3D& planeNormal,
    const TransformGridFrame& frame) noexcept;

/** Document-precision counterpart for Cleave and other authored plane points. */
TVector snapPointToGridOnPlane(
    const TVector& point,
    const TVector& planePoint,
    const TVector& planeNormal,
    const TransformGridFrame& frame) noexcept;

/**
 * Applies normal-only grid-plane contact and smooth stick-through release.
 *
 * `movingPoints` are the original world-space support points of the moved
 * selection. Only points on the current approach side participate, so a
 * sector that already spans the plane can still make directional contact.
 * Once released, the state is latched for the rest of the drag so continued
 * movement cannot re-capture and snap back to the held point.
 */
TVector applyGridPlaneStickThrough(
    const TVector& translation,
    std::span<const TVector> movingPoints,
    const TransformGridFrame& frame,
    double captureDistance,
    double releaseDistance,
    GridPlaneStickState& state,
    const TVector& constraintAxis = {}) noexcept;

QVector3D applyGridPlaneStickThrough(
    const QVector3D& translation,
    std::span<const QVector3D> movingPoints,
    const TransformGridFrame& frame,
    float captureDistance,
    float releaseDistance,
    GridPlaneStickState& state,
    const QVector3D& constraintAxis = {}) noexcept;

/**
 * Wraps an absolute grid coordinate to one spacing period around zero.
 *
 * Shader grid rendering uses this phase with camera-relative coordinates so
 * the GPU never has to run `fract()` on very large world-space values.
 */
float stableGridCoordPhase(double coord, double spacing) noexcept;

/**
 * Builds the cached line-grid fallback geometry.
 *
 * Vertices are grouped as minor, major, axis-A, then axis-B lines so the caller
 * can draw each group with a different color without extra sorting.
 */
GridLineBuild buildGridLineVertices(const GridLineBuildParams& params);

} // namespace sed::render
