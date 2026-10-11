#include "render/grid/viewport_grid_utils.hpp"

#include <algorithm>
#include <type_traits>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <limits>

#include "sed/math_utils.hpp"

namespace sed::render
{
namespace
{

bool isFiniteVector(const QVector3D& value) noexcept
{
    return std::isfinite(value.x()) && std::isfinite(value.y()) && std::isfinite(value.z());
}

void pushGridLine(std::vector<GridLineVertex>& dst, const QVector3D& p0, const QVector3D& p1)
{
    GridLineVertex a{};
    a.x = p0.x();
    a.y = p0.y();
    a.z = p0.z();
    a.pickId = 0.0f;

    GridLineVertex b{};
    b.x = p1.x();
    b.y = p1.y();
    b.z = p1.z();
    b.pickId = 0.0f;

    dst.push_back(a);
    dst.push_back(b);
}

void pushGridPoint(std::vector<GridLineVertex>& dst, const QVector3D& point)
{
    GridLineVertex vertex{};
    vertex.x = point.x();
    vertex.y = point.y();
    vertex.z = point.z();
    dst.push_back(vertex);
}

} // namespace

bool orthonormalizeGridAxes(
    const QVector3D& axisAInput,
    const QVector3D& axisBInput,
    QVector3D& axisA,
    QVector3D& axisB,
    QVector3D& normal) noexcept
{
    axisA = axisAInput;
    axisB = axisBInput;
    normal = {};

    if (!isFiniteVector(axisA) || !isFiniteVector(axisB)
        || axisA.lengthSquared() <= sed::math::kTinyEpsilonF
        || axisB.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return false;
    }

    axisA.normalize();
    axisB -= axisA * QVector3D::dotProduct(axisB, axisA);
    if (!isFiniteVector(axisB)
        || axisB.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return false;
    }
    axisB.normalize();

    normal = QVector3D::crossProduct(axisA, axisB);
    if (!isFiniteVector(normal)
        || normal.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return false;
    }
    normal.normalize();
    return isFiniteVector(normal);
}

bool gridAxesFromEulerAngles(
    const GridTransformAngles& angles,
    QVector3D& axisA,
    QVector3D& axisB,
    QVector3D& normal) noexcept
{
    if (!std::isfinite(angles.yawDeg)
        || !std::isfinite(angles.pitchDeg)
        || !std::isfinite(angles.rollDeg))
    {
        axisA = QVector3D(1.0f, 0.0f, 0.0f);
        axisB = QVector3D(0.0f, 1.0f, 0.0f);
        normal = QVector3D(0.0f, 0.0f, 1.0f);
        return false;
    }

    constexpr float kPiOver180 = 0.017453292519943295769f;
    const float yaw = angles.yawDeg * kPiOver180;
    const float pitch = angles.pitchDeg * kPiOver180;
    const float roll = angles.rollDeg * kPiOver180;
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cr = std::cos(roll);
    const float sr = std::sin(roll);

    // R = Rz(yaw) * Ry(pitch) * Rx(roll); columns 0 and 1 are the
    // in-plane axes.  Writing the matrix explicitly avoids Qt's historical
    // Euler-order ambiguity and makes save/load round trips deterministic.
    axisA = QVector3D(cy * cp, sy * cp, -sp);
    axisB = QVector3D(
        cy * sp * sr - sy * cr,
        sy * sp * sr + cy * cr,
        cp * sr);
    return orthonormalizeGridAxes(axisA, axisB, axisA, axisB, normal);
}

GridTransformAngles gridEulerAnglesFromAxes(
    const QVector3D& axisAInput,
    const QVector3D& axisBInput) noexcept
{
    GridTransformAngles result{};
    QVector3D axisA{};
    QVector3D axisB{};
    QVector3D normal{};
    if (!orthonormalizeGridAxes(axisAInput, axisBInput, axisA, axisB, normal))
    {
        return result;
    }

    constexpr float kRadToDeg = 57.2957795130823208768f;
    const float yaw = std::atan2(axisA.y(), axisA.x());
    const float horizontal = std::hypot(axisA.x(), axisA.y());
    const float pitch = std::atan2(-axisA.z(), horizontal);

    float roll = 0.0f;
    if (horizontal > 1.0e-5f)
    {
        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);
        // For the explicit Rz*Ry*Rx matrix, these are cos(roll) and
        // sin(roll), respectively.  The division remains bounded because
        // horizontal is the cosine of pitch.
        const float cosRoll = -sy * axisB.x() + cy * axisB.y();
        const float sinRoll = axisB.z() / horizontal;
        roll = std::atan2(sinRoll, cosRoll);
    }

    result.yawDeg = yaw * kRadToDeg;
    result.pitchDeg = pitch * kRadToDeg;
    result.rollDeg = roll * kRadToDeg;
    return result;
}

GridViewAngles viewAnglesForGridAxes(
    const QVector3D& axisAInput,
    const QVector3D& axisBInput) noexcept
{
    GridViewAngles result{};
    QVector3D axisA{};
    QVector3D axisB{};
    QVector3D normal{};
    if (!orthonormalizeGridAxes(
            axisAInput, axisBInput, axisA, axisB, normal))
    {
        return result;
    }

    // The orbit direction points from target to camera, while the view's
    // forward direction points toward the target. Looking perpendicular to
    // the grid therefore places the camera on its +normal side.
    constexpr float kRadToDeg = 57.2957795130823208768f;
    const float horizontal = std::hypot(normal.x(), normal.y());
    const float pitch = std::atan2(normal.z(), horizontal);
    float yaw = 0.0f;
    if (horizontal > 1.0e-5f)
    {
        yaw = std::atan2(normal.x(), -normal.y());
    }
    else
    {
        // At a camera pole yaw does not affect forward, but it does define
        // the unrolled screen-right axis. Choose it from authored grid axis A
        // so the subsequent roll remains deterministic.
        yaw = std::atan2(axisA.y(), axisA.x());
    }

    const QVector3D forward = -normal;
    QVector3D baseRight = QVector3D::crossProduct(
        forward, QVector3D(0.0f, 0.0f, 1.0f));
    if (baseRight.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        // This is the same pole condition used by ViewportCamera. Axis A is
        // already the exact authored screen-right direction used to choose
        // yaw; rebuilding it through sin/cos would turn a float residual into
        // a spurious non-zero roll.
        baseRight = axisA;
    }
    if (!isFiniteVector(baseRight)
        || baseRight.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return result;
    }
    baseRight.normalize();

    const float roll = std::atan2(
        QVector3D::dotProduct(
            QVector3D::crossProduct(baseRight, axisA), forward),
        QVector3D::dotProduct(baseRight, axisA));
    result.yawDeg = yaw * kRadToDeg;
    result.pitchDeg = pitch * kRadToDeg;
    result.rollDeg = roll * kRadToDeg;
    result.valid = std::isfinite(result.yawDeg)
        && std::isfinite(result.pitchDeg)
        && std::isfinite(result.rollDeg);
    return result;
}

ViewAlignedGridFrame makeViewAlignedGridFrame(
    const QVector3D& cameraRight,
    const QVector3D& cameraUp,
    const QVector3D& cameraForward) noexcept
{
    ViewAlignedGridFrame result{};
    QVector3D right{};
    QVector3D up{};
    QVector3D normal{};
    if (orthonormalizeGridAxes(cameraRight, cameraUp, right, up, normal))
    {
        result.axisA = right;
        result.axisB = up;
        result.normal = normal;
        result.valid = true;
        return result;
    }

    // Near a camera pole the extracted up vector can be parallel to right.
    // Rebuild a deterministic orthogonal up from the forward direction and
    // world Z/Y fallbacks instead of allowing a frame-sized grid to vanish.
    QVector3D forward = cameraForward;
    if (!isFiniteVector(forward) || forward.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        forward = QVector3D(0.0f, 1.0f, 0.0f);
    }
    forward.normalize();

    right = cameraRight;
    if (!isFiniteVector(right) || right.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        right = QVector3D::crossProduct(forward, QVector3D(0.0f, 0.0f, 1.0f));
        if (right.lengthSquared() <= sed::math::kTinyEpsilonF)
        {
            right = QVector3D::crossProduct(forward, QVector3D(0.0f, 1.0f, 0.0f));
        }
    }
    right -= forward * QVector3D::dotProduct(right, forward);
    if (!isFiniteVector(right) || right.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        right = QVector3D(1.0f, 0.0f, 0.0f);
    }
    right.normalize();

    up = QVector3D::crossProduct(forward, right);
    if (!isFiniteVector(up) || up.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        up = QVector3D(0.0f, 0.0f, 1.0f);
    }
    up.normalize();
    if (!orthonormalizeGridAxes(right, up, result.axisA, result.axisB, result.normal))
    {
        result.axisA = QVector3D(1.0f, 0.0f, 0.0f);
        result.axisB = QVector3D(0.0f, 1.0f, 0.0f);
        result.normal = QVector3D(0.0f, 0.0f, 1.0f);
        result.valid = false;
        return result;
    }
    result.valid = true;
    return result;
}

ViewAlignedGridDisplayPlane resolveViewAlignedGridDisplayPlane(
    const QVector3D& authoredOrigin,
    const QVector3D& cameraEye,
    const QVector3D& cameraForwardInput,
    const float nearPlane,
    const bool keepVisible) noexcept
{
    ViewAlignedGridDisplayPlane result{
        .planePoint = authoredOrigin,
        .coordOrigin = authoredOrigin,
        .proxied = false,
    };
    if (!keepVisible || !isFiniteVector(authoredOrigin)
        || !isFiniteVector(cameraEye) || !isFiniteVector(cameraForwardInput))
    {
        return result;
    }

    QVector3D cameraForward = cameraForwardInput;
    if (cameraForward.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return result;
    }
    cameraForward.normalize();

    // Four near-plane distances leave enough room for the analytic-grid
    // reconstruction and its small coplanar depth bias. Retain a small world
    // floor for malformed/tiny clip distances without pushing a normal visible
    // plane away from its authored location.
    const float safeDepth = std::max(
        std::isfinite(nearPlane) ? std::abs(nearPlane) * 4.0f : 0.0f,
        0.05f);
    const float authoredDepth = QVector3D::dotProduct(
        authoredOrigin - cameraEye, cameraForward);
    if (std::isfinite(authoredDepth) && authoredDepth > safeDepth)
    {
        return result;
    }

    result.planePoint = cameraEye + cameraForward * safeDepth;
    // Project the authored lattice origin along the plane normal. Its A/B
    // coordinates therefore do not change when the visual proxy activates.
    result.coordOrigin = authoredOrigin
        + cameraForward * (safeDepth - authoredDepth);
    result.proxied = isFiniteVector(result.planePoint)
        && isFiniteVector(result.coordOrigin);
    if (!result.proxied)
    {
        result.planePoint = authoredOrigin;
        result.coordOrigin = authoredOrigin;
    }
    return result;
}

float orthographicGridUnitsPerPixel(
    const float orthoScale,
    const int viewportHeight) noexcept
{
    if (!std::isfinite(orthoScale) || orthoScale <= 0.0f || viewportHeight <= 0)
    {
        return 0.0f;
    }
    const float result = orthoScale / static_cast<float>(viewportHeight);
    return std::isfinite(result) && result > 0.0f ? result : 0.0f;
}

float resolveGridUnitsPerPixel(
    const float planeMeasured,
    const float projectionMeasured,
    const bool orthographic,
    const float orthoScale,
    const int viewportHeight) noexcept
{
    const auto usable = [](const float value) noexcept
    {
        return std::isfinite(value) && value > 0.0f;
    };
    if (usable(planeMeasured))
    {
        return planeMeasured;
    }
    if (usable(projectionMeasured))
    {
        return projectionMeasured;
    }
    if (orthographic)
    {
        const float fallback = orthographicGridUnitsPerPixel(
            orthoScale, viewportHeight);
        if (usable(fallback))
        {
            return fallback;
        }
    }

    // This value is also the renderer's historical valid default. Keeping it
    // centralized makes a degenerate perspective view deterministic without
    // letting an initializer accidentally bypass better fallbacks.
    return 0.01f;
}

float gridMinimumVisibleHalfExtent(const double majorSpacing) noexcept
{
    if (!std::isfinite(majorSpacing) || majorSpacing <= 0.0)
    {
        return 8.0f;
    }
    const double result = std::clamp(majorSpacing * 8.0, 1.0e-4, 1.0e9);
    return static_cast<float>(result);
}

QColor gridAxisColorForDirection(
    const QVector3D& axis,
    const QColor& xColor,
    const QColor& yColor,
    const QColor& zColor)
{
    if (std::abs(axis.x()) > 0.5f)
    {
        return xColor.isValid() ? xColor : QColor(210, 75, 70);
    }
    if (std::abs(axis.y()) > 0.5f)
    {
        return yColor.isValid() ? yColor : QColor(95, 190, 95);
    }
    return zColor.isValid() ? zColor : QColor(90, 140, 215);
}

QColor gridAxisColorForDirection(const QVector3D& axis)
{
    return gridAxisColorForDirection(
        axis, QColor(210, 75, 70), QColor(95, 190, 95), QColor(90, 140, 215));
}

double niceGridStep(const double raw) noexcept
{
    if (!std::isfinite(raw) || raw <= 0.0)
    {
        return 1.0;
    }

    const double exp10 = std::floor(std::log10(raw));
    const double base = std::pow(10.0, exp10);
    const double f = raw / base;

    double nf = 1.0;
    if (f < 1.5)
    {
        nf = 1.0;
    }
    else if (f < 3.0)
    {
        nf = 2.0;
    }
    else if (f < 7.0)
    {
        nf = 5.0;
    }
    else
    {
        nf = 10.0;
    }

    return nf * base;
}

EffectiveGridSpacing effectiveGridSpacing(
    const double baseMajor,
    const double baseMinor,
    const bool adaptive,
    const double unitsPerPixel) noexcept
{
    constexpr double kDesiredPixels = 32.0;

    EffectiveGridSpacing out{};
    out.major = std::isfinite(baseMajor) ? std::max(1.0e-9, std::abs(baseMajor)) : 1.0;
    out.minor = std::isfinite(baseMinor)
        ? std::max(1.0e-9, std::min(std::abs(baseMinor), out.major))
        : std::min(0.1, out.major);

    if (adaptive && std::isfinite(unitsPerPixel) && unitsPerPixel > 0.0)
    {
        const double scale = niceGridStep((unitsPerPixel * kDesiredPixels) / out.major);
        out.major *= scale;
        out.minor *= scale;
    }

    out.minor = std::min(out.minor, out.major);
    const double ratio = out.major / out.minor;
    constexpr std::int64_t kMaxMajorEvery =
        std::numeric_limits<std::int64_t>::max();
    if (!std::isfinite(ratio)
        || ratio >= static_cast<double>(kMaxMajorEvery))
    {
        // Preserve the finite major spacing even when an artificial direct
        // caller supplies a ratio beyond the fallback renderer's integer
        // indexing range. Production UI ranges remain exactly representable.
        out.majorEvery = kMaxMajorEvery;
    }
    else
    {
        out.majorEvery = std::max<std::int64_t>(
            1, static_cast<std::int64_t>(std::llround(ratio)));
        out.major = out.minor * static_cast<double>(out.majorEvery);
    }
    return out;
}

GridDrawPolicy gridDrawPolicy(const GridDrawMode mode) noexcept
{
    switch (mode)
    {
    case GridDrawMode::VisibleThroughWalls:
        // A late pass without depth rejection deliberately overlays everything.
        return GridDrawPolicy{
            .drawBeforeGeometry = false,
            .depthTestScene = false,
            .coplanarNdcBias = 0.0f,
        };
    case GridDrawMode::AlwaysBehind:
        // Drawing first lets all later scene geometry cover the grid.
        return GridDrawPolicy{
            .drawBeforeGeometry = true,
            .depthTestScene = true,
            .coplanarNdcBias = 0.0f,
        };
    case GridDrawMode::Occluded:
    default:
        // Draw after opaque geometry and Things, but retain the scene depth test.
        // This keeps nearby/inside grid fragments visible while solid surfaces
        // correctly occlude fragments behind them.  A two-micro-NDC pull is
        // enough to beat depth-buffer/reconstruction rounding for a genuinely
        // coplanar surface while remaining below a meaningful scene gap.
        return GridDrawPolicy{
            .drawBeforeGeometry = false,
            .depthTestScene = true,
            .coplanarNdcBias = 2.0e-6f,
        };
    }
}

GridAxisFilterPolicy gridAxisFilterPolicy() noexcept
{
    return {};
}

float clampedGridLineWidthPx(const float widthPx) noexcept
{
    return std::clamp(
        std::isfinite(widthPx) ? widthPx : 1.0f,
        kGridLineWidthMinPx,
        kGridLineWidthMaxPx);
}

float gridAxisLineWidthPx(const float gridLineWidthPx) noexcept
{
    const GridAxisFilterPolicy policy = gridAxisFilterPolicy();
    return std::clamp(
        clampedGridLineWidthPx(gridLineWidthPx) * policy.lineWidthScale,
        kGridLineWidthMinPx,
        policy.maximumLineWidthPx);
}

float gridPhysicalLineWidthPx(
    const float gridLineWidthPx,
    const float devicePixelRatio) noexcept
{
    const float dpr = std::isfinite(devicePixelRatio)
        ? std::max(devicePixelRatio, 1.0f)
        : 1.0f;
    return clampedGridLineWidthPx(gridLineWidthPx) * dpr;
}

float gridPhysicalAxisLineWidthPx(
    const float gridLineWidthPx,
    const float devicePixelRatio) noexcept
{
    const float dpr = std::isfinite(devicePixelRatio)
        ? std::max(devicePixelRatio, 1.0f)
        : 1.0f;
    return gridAxisLineWidthPx(gridLineWidthPx) * dpr;
}

float clampedGridMinorDotSizePx(const float dotSizePx) noexcept
{
    return std::clamp(
        std::isfinite(dotSizePx) ? dotSizePx : 2.0f,
        kGridMinorDotSizeMinPx,
        kGridMinorDotSizeMaxPx);
}

float gridPhysicalMinorDotSizePx(
    const float dotSizePx,
    const float devicePixelRatio) noexcept
{
    const float dpr = std::isfinite(devicePixelRatio) && devicePixelRatio > 0.0f
        ? devicePixelRatio
        : 1.0f;
    return clampedGridMinorDotSizePx(dotSizePx) * dpr;
}

float gridAnalyticPointCoverage(
    const float distancePx,
    const float diameterPx) noexcept
{
    if ( !std::isfinite(distancePx) || distancePx < 0.0f
        || !std::isfinite(diameterPx) || diameterPx <= 0.0f )
    {
        return 0.0f;
    }

    // Filter the signed distance to the circular edge over one physical pixel.
    // Unlike independent A/B line coverages this remains round under rotation.
    return std::clamp(0.5f * diameterPx + 0.5f - distancePx, 0.0f, 1.0f);
}

float gridAnalyticLineCoverage(
    const float distancePx,
    const float lineWidthPx) noexcept
{
    if (!std::isfinite(distancePx) || distancePx < 0.0f
        || !std::isfinite(lineWidthPx) || lineWidthPx <= 0.0f)
    {
        return 0.0f;
    }

    // Exact overlap of a centred line interval and a one-pixel box filter.
    // For subpixel lines the centre coverage is the requested fractional
    // width instead of jumping to opaque whenever the line lands on a pixel.
    const float halfLine = 0.5f * lineWidthPx;
    constexpr float kHalfPixel = 0.5f;
    const float plateau = std::min(lineWidthPx, 1.0f);
    const float transitionStart = std::abs(halfLine - kHalfPixel);
    const float transitionEnd = halfLine + kHalfPixel;
    if (distancePx <= transitionStart)
    {
        return plateau;
    }
    if (distancePx >= transitionEnd)
    {
        return 0.0f;
    }

    return plateau * (transitionEnd - distancePx)
        / std::max(transitionEnd - transitionStart, sed::math::kTinyEpsilonF);
}

float gridEuclideanDerivative(
    const float derivativeX,
    const float derivativeY) noexcept
{
    if (!std::isfinite(derivativeX) || !std::isfinite(derivativeY))
    {
        return 0.0f;
    }
    return std::hypot(derivativeX, derivativeY);
}

QVector4D gridAxisReplacementComposite(
    const QVector4D& ordinaryColor,
    const QVector4D& axisColor,
    const float axisCoverage) noexcept
{
    const float dstAlpha = std::clamp(
        std::isfinite(ordinaryColor.w()) ? ordinaryColor.w() : 0.0f,
        0.0f,
        1.0f);
    const float srcAlpha = std::clamp(
        (std::isfinite(axisColor.w()) ? axisColor.w() : 0.0f)
            * (std::isfinite(axisCoverage) ? axisCoverage : 0.0f),
        0.0f,
        1.0f);
    const float outAlpha = std::max(dstAlpha, srcAlpha);
    if (outAlpha <= sed::math::kTinyEpsilonF)
    {
        return QVector4D();
    }

    const float replacement = std::clamp(srcAlpha / outAlpha, 0.0f, 1.0f);
    const QVector3D rgb = ordinaryColor.toVector3D() * (1.0f - replacement)
        + axisColor.toVector3D() * replacement;
    return QVector4D(rgb, outAlpha);
}

GridProjectedAxisLine projectedGridAxisLine(
    const QMatrix4x4& mvp,
    const QVector3D& origin,
    const QVector3D& direction) noexcept
{
    GridProjectedAxisLine out{};
    if (!isFiniteVector(origin) || !isFiniteVector(direction)
        || direction.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return out;
    }

    const QVector4D clipOrigin = mvp * QVector4D(origin, 1.0f);
    const QVector4D clipDirection = mvp * QVector4D(direction, 0.0f);
    const QVector3D projectedOrigin(clipOrigin.x(), clipOrigin.y(), clipOrigin.w());
    const QVector3D projectedDirection(
        clipDirection.x(), clipDirection.y(), clipDirection.w());
    QVector3D equation = QVector3D::crossProduct(projectedOrigin, projectedDirection);
    if (!isFiniteVector(equation))
    {
        return out;
    }

    const float normalLength = std::hypot(equation.x(), equation.y());
    if (!std::isfinite(normalLength) || normalLength <= sed::math::kTinyEpsilonF)
    {
        return out;
    }

    equation /= normalLength;
    out.equation = equation;
    out.valid = true;
    return out;
}

float projectedGridAxisDistancePx(
    const GridProjectedAxisLine& line,
    const QVector2D& ndc,
    const int viewportWidthPx,
    const int viewportHeightPx) noexcept
{
    if (!line.valid || !isFiniteVector(line.equation)
        || !std::isfinite(ndc.x()) || !std::isfinite(ndc.y())
        || viewportWidthPx <= 0 || viewportHeightPx <= 0)
    {
        return std::numeric_limits<float>::infinity();
    }

    const QVector2D gradientPerPixel(
        line.equation.x() * (2.0f / static_cast<float>(viewportWidthPx)),
        line.equation.y() * (2.0f / static_cast<float>(viewportHeightPx)));
    const float gradientLength = gradientPerPixel.length();
    if (!std::isfinite(gradientLength) || gradientLength <= sed::math::kTinyEpsilonF)
    {
        return std::numeric_limits<float>::infinity();
    }

    const float evaluated = line.equation.x() * ndc.x()
        + line.equation.y() * ndc.y()
        + line.equation.z();
    return std::abs(evaluated) / gradientLength;
}

GridClipFadePolicy gridClipFadePolicy() noexcept
{
    return {};
}

float gridClipFade(const float hitDistance, const float rayFarDistance) noexcept
{
    const GridClipFadePolicy policy = gridClipFadePolicy();
    if (!std::isfinite(hitDistance) || !std::isfinite(rayFarDistance)
        || rayFarDistance <= sed::math::kTinyEpsilonF)
    {
        // An invalid reconstructed far point should not make the whole grid
        // disappear. The normal clip/depth test remains the safe fallback.
        return 1.0f;
    }

    const float ratio = std::clamp(hitDistance / rayFarDistance, 0.0f, 1.0f);
    const float width = policy.endRatio - policy.startRatio;
    if (width <= sed::math::kTinyEpsilonF)
    {
        return ratio < policy.endRatio ? 1.0f : 0.0f;
    }

    const float x = std::clamp((ratio - policy.startRatio) / width, 0.0f, 1.0f);
    const float smooth = x * x * (3.0f - (2.0f * x));
    return 1.0f - smooth;
}

GridFootprintFadePolicy gridFootprintFadePolicy() noexcept
{
    return {};
}

float gridFootprintFade(
    const float pixelsPerCell,
    const float hiddenPixelsPerCell,
    const float fullyVisiblePixelsPerCell,
    const float lineWidthPixels) noexcept
{
    if (!std::isfinite(pixelsPerCell) || pixelsPerCell <= 0.0f)
    {
        return std::isinf(pixelsPerCell) && pixelsPerCell > 0.0f ? 1.0f : 0.0f;
    }

    if (!std::isfinite(hiddenPixelsPerCell)
        || !std::isfinite(fullyVisiblePixelsPerCell)
        || fullyVisiblePixelsPerCell <= hiddenPixelsPerCell)
    {
        return pixelsPerCell > hiddenPixelsPerCell ? 1.0f : 0.0f;
    }

    const float widthScale = std::isfinite(lineWidthPixels)
        ? std::max(lineWidthPixels, 1.0f)
        : 1.0f;
    const float scaledHidden = hiddenPixelsPerCell * widthScale;
    const float scaledFullyVisible = fullyVisiblePixelsPerCell * widthScale;

    const float x = std::clamp(
        (pixelsPerCell - scaledHidden)
            / (scaledFullyVisible - scaledHidden),
        0.0f,
        1.0f);
    return x * x * (3.0f - (2.0f * x));
}

GridPointFootprintFadeBand gridPointFootprintFadeBand(
    const float diameterPx) noexcept
{
    const float safeDiameterPx = std::isfinite(diameterPx)
            && diameterPx > 0.0f
        ? diameterPx
        : 2.0f;
    const float radialSupportRadiusPx = 0.5f * safeDiameterPx + 0.5f;
    const GridFootprintFadePolicy minimumBand = gridFootprintFadePolicy();
    return GridPointFootprintFadeBand{
        std::max(minimumBand.minorHiddenPixelsPerCell,
            radialSupportRadiusPx),
        std::max(minimumBand.minorFullyVisiblePixelsPerCell,
            4.0f * radialSupportRadiusPx)
    };
}

float gridPointFootprintFade(
    const float pixelsPerCell,
    const float diameterPx) noexcept
{
    if ( !std::isfinite(pixelsPerCell) || pixelsPerCell <= 0.0f )
    {
        return std::isinf(pixelsPerCell) && pixelsPerCell > 0.0f
            ? 1.0f
            : 0.0f;
    }

    const GridPointFootprintFadeBand band =
        gridPointFootprintFadeBand(diameterPx);
    const float x = std::clamp(
        (pixelsPerCell - band.hiddenPixelsPerCell)
            / (band.fullyVisiblePixelsPerCell - band.hiddenPixelsPerCell),
        0.0f,
        1.0f);
    return x * x * (3.0f - (2.0f * x));
}

double gridHierarchySpacing(
    const double baseMajorSpacing,
    const int level,
    const double hierarchyRatio) noexcept
{
    if (!std::isfinite(baseMajorSpacing) || baseMajorSpacing <= 0.0
        || !std::isfinite(hierarchyRatio) || hierarchyRatio <= 1.0
        || level < 0)
    {
        return 0.0;
    }

    const double spacing = baseMajorSpacing * std::pow(hierarchyRatio, static_cast<double>(level));
    return std::isfinite(spacing) ? spacing : 0.0;
}

TransformGridFrame makeTransformGridFrame(
    const QVector3D& origin,
    const QVector3D& axisAInput,
    const QVector3D& axisBInput,
    const double spacing) noexcept
{
    TransformGridFrame out{};
    out.origin = origin;
    out.spacing = spacing;

    if (!isFiniteVector(origin)
        || !std::isfinite(spacing) || spacing <= 1.0e-9)
    {
        return out;
    }

    QVector3D axisA{};
    QVector3D axisB{};
    QVector3D normal{};
    if (!orthonormalizeGridAxes(axisAInput, axisBInput, axisA, axisB, normal))
    {
        return out;
    }

    out.axisA = axisA;
    out.axisB = axisB;
    out.normal = normal;
    out.valid = true;
    return out;
}

QVector3D snapTransformTranslationToGrid(
    const QVector3D& referencePoint,
    const QVector3D& rawTranslation,
    const TransformGridFrame& frame) noexcept
{
    return snapTransformTranslationToGrid(
        TVector{referencePoint.x(), referencePoint.y(), referencePoint.z()}, rawTranslation, frame);
}

QVector3D snapTransformTranslationToGrid(
    const TVector& referencePoint,
    const QVector3D& rawTranslation,
    const TransformGridFrame& frame,
    TVector* documentTranslation) noexcept
{
    return snapTransformTranslationToGrid(referencePoint,
        TVector{rawTranslation.x(), rawTranslation.y(), rawTranslation.z()}, frame, documentTranslation);
}

QVector3D snapTransformTranslationToGrid(
    const TVector& referencePoint,
    const TVector& rawTranslation,
    const TransformGridFrame& frame,
    TVector* documentTranslation) noexcept
{
    const QVector3D rawDisplay(static_cast<float>(rawTranslation.x),
        static_cast<float>(rawTranslation.y), static_cast<float>(rawTranslation.z));
    if (documentTranslation)
    {
        *documentTranslation = TVector{rawTranslation.x, rawTranslation.y, rawTranslation.z};
    }
    if (!frame.valid || !std::isfinite(referencePoint.x) || !std::isfinite(referencePoint.y)
        || !std::isfinite(referencePoint.z) || (!std::isfinite(rawTranslation.x) || !std::isfinite(rawTranslation.y) || !std::isfinite(rawTranslation.z))
        || !isFiniteVector(frame.origin) || !isFiniteVector(frame.axisA) || !isFiniteVector(frame.axisB)
        || !std::isfinite(frame.spacing) || frame.spacing <= 1.0e-9)
    {
        return rawDisplay;
    }

    const TVector relative = referencePoint - TVector{frame.origin.x(), frame.origin.y(), frame.origin.z()};
    const auto correction = [&](const QVector3D& axis)
    {
        const double reference = relative.x * axis.x() + relative.y * axis.y() + relative.z * axis.z();
        const double delta = static_cast<double>(rawTranslation.x) * axis.x()
            + static_cast<double>(rawTranslation.y) * axis.y() + static_cast<double>(rawTranslation.z) * axis.z();
        // Round in lattice coordinates, then measure only the local cell
        // displacement. Never add a fine move to an absolute float position.
        const double baseCell = std::round(reference / frame.spacing);
        const double phase = reference - baseCell * frame.spacing;
        const double localCell = (phase + delta) / frame.spacing;
        double cellDelta = std::floor(localCell);
        const double fraction = localCell - cellDelta;
        // Match global round-half-away-from-zero without adding a small
        // fractional cell to a large integer world-cell index.
        if (fraction > 0.5 || (fraction == 0.5 && reference + delta >= 0.0))
        {
            cellDelta += 1.0;
        }
        return cellDelta * frame.spacing - phase - delta;
    };
    const double deltaU = correction(frame.axisA);
    const double deltaV = correction(frame.axisB);
    const TVector precise{
        static_cast<double>(rawTranslation.x) + frame.axisA.x() * deltaU + frame.axisB.x() * deltaV,
        static_cast<double>(rawTranslation.y) + frame.axisA.y() * deltaU + frame.axisB.y() * deltaV,
        static_cast<double>(rawTranslation.z) + frame.axisA.z() * deltaU + frame.axisB.z() * deltaV};
    const QVector3D snapped(static_cast<float>(precise.x), static_cast<float>(precise.y), static_cast<float>(precise.z));
    if (!isFiniteVector(snapped))
    {
        return rawDisplay;
    }
    if (documentTranslation)
    {
        *documentTranslation = precise;
    }
    return snapped;
}

QVector3D snapPointToGridOnPlane(
    const QVector3D& point,
    const QVector3D& planePoint,
    const QVector3D& planeNormalInput,
    const TransformGridFrame& frame) noexcept
{
    if (!frame.valid || !isFiniteVector(point) || !isFiniteVector(planePoint)
        || !isFiniteVector(planeNormalInput) || !isFiniteVector(frame.origin)
        || !isFiniteVector(frame.axisA) || !isFiniteVector(frame.axisB)
        || !std::isfinite(frame.spacing) || frame.spacing <= 1.0e-9)
    {
        return point;
    }

    QVector3D planeNormal = planeNormalInput;
    if (planeNormal.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return point;
    }
    planeNormal.normalize();

    // Keep the authored lattice phase when a parallel construction/surface
    // plane sits at a different depth. This is the common F3 -> F2 cleave
    // workflow: both planes project to the same visible grid in orthographic
    // view, so snapping must not silently fall back to world-zero phase.
    const QVector3D projectedOrigin = frame.origin
        - planeNormal * QVector3D::dotProduct(frame.origin - planePoint, planeNormal);

    QVector3D axisA = frame.axisA
        - planeNormal * QVector3D::dotProduct(frame.axisA, planeNormal);
    if (axisA.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        axisA = frame.axisB
            - planeNormal * QVector3D::dotProduct(frame.axisB, planeNormal);
    }
    if (!isFiniteVector(axisA) || axisA.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return point;
    }
    axisA.normalize();

    QVector3D axisB = QVector3D::crossProduct(planeNormal, axisA);
    if (!isFiniteVector(axisB) || axisB.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return point;
    }
    axisB.normalize();
    if (QVector3D::dotProduct(axisB, frame.axisB) < 0.0f)
    {
        axisB = -axisB;
    }

    const QVector3D relative = point - projectedOrigin;
    const double u = static_cast<double>(QVector3D::dotProduct(relative, axisA));
    const double v = static_cast<double>(QVector3D::dotProduct(relative, axisB));
    if (!std::isfinite(u) || !std::isfinite(v))
    {
        return point;
    }

    const double snappedU = std::round(u / frame.spacing) * frame.spacing;
    const double snappedV = std::round(v / frame.spacing) * frame.spacing;
    const QVector3D snapped = projectedOrigin
        + axisA * static_cast<float>(snappedU)
        + axisB * static_cast<float>(snappedV);
    return isFiniteVector(snapped) ? snapped : point;
}

TVector snapPointToGridOnPlane(
    const TVector& point,
    const TVector& planePoint,
    const TVector& planeNormalInput,
    const TransformGridFrame& frame) noexcept
{
    const auto finite = [](const TVector& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    const auto normalize = [&finite](TVector& value)
    {
        const double scale = std::max({std::abs(value.x), std::abs(value.y), std::abs(value.z)});
        if (!finite(value) || !(scale > 0.0))
        {
            return false;
        }
        value = TVector{value.x / scale, value.y / scale, value.z / scale};
        value = value * (1.0 / std::sqrt(value.dot(value)));
        return true;
    };
    if (!frame.valid || !finite(point) || !finite(planePoint)
        || !finite(planeNormalInput) || !isFiniteVector(frame.origin)
        || !isFiniteVector(frame.axisA) || !isFiniteVector(frame.axisB)
        || !std::isfinite(frame.spacing) || frame.spacing <= 1.0e-9)
    {
        return point;
    }
    TVector normal = planeNormalInput;
    if (!normalize(normal))
    {
        return point;
    }
    const TVector origin{frame.origin.x(), frame.origin.y(), frame.origin.z()};
    const TVector sourceA{frame.axisA.x(), frame.axisA.y(), frame.axisA.z()};
    const TVector sourceB{frame.axisB.x(), frame.axisB.y(), frame.axisB.z()};
    const TVector projectedOrigin = origin - normal * (origin - planePoint).dot(normal);
    TVector axisA = sourceA - normal * sourceA.dot(normal);
    if (axisA.lengthSqr() <= sed::math::kTinyEpsilon)
    {
        axisA = sourceB - normal * sourceB.dot(normal);
    }
    if (!normalize(axisA))
    {
        return point;
    }
    TVector axisB = normal.cross(axisA);
    if (!normalize(axisB))
    {
        return point;
    }
    if (axisB.dot(sourceB) < 0.0)
    {
        axisB = -axisB;
    }
    const TVector projectedPoint = point - normal * (point - planePoint).dot(normal);
    const TVector relative = projectedPoint - projectedOrigin;
    const double u = relative.dot(axisA);
    const double v = relative.dot(axisB);
    if (!std::isfinite(u) || !std::isfinite(v))
    {
        return point;
    }
    // Apply local cell corrections to the document point rather than
    // reconstructing an absolute point from large lattice coordinates.
    const TVector snapped = projectedPoint
        + axisA * (std::round(u / frame.spacing) * frame.spacing - u)
        + axisB * (std::round(v / frame.spacing) * frame.spacing - v);
    return finite(snapped) ? snapped : point;
}

template<typename Point>
TVector applyGridPlaneStickThroughImpl(
    const TVector& translation,
    const std::span<const Point> movingPoints,
    const TransformGridFrame& frame,
    const double captureDistance,
    const double releaseDistance,
    GridPlaneStickState& state,
    const TVector& constraintAxisInput) noexcept
{
    const auto finite = [](const TVector& value)
    { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); };
    const TVector origin{frame.origin.x(), frame.origin.y(), frame.origin.z()};
    TVector normal{frame.normal.x(), frame.normal.y(), frame.normal.z()};
    const double normalLength = std::hypot(normal.x, normal.y, normal.z);
    if (!std::isfinite(normalLength) || normalLength == 0.0) { state = {}; return translation; }
    normal = normal * (1.0 / normalLength);
    if (!frame.valid || !finite(translation) || !finite(constraintAxisInput) || !isFiniteVector(frame.origin)
        || !isFiniteVector(frame.normal) || movingPoints.empty()
        || !std::isfinite(captureDistance) || !std::isfinite(releaseDistance)
        || captureDistance < 0.0f || releaseDistance <= captureDistance)
    {
        state = {};
        return translation;
    }

    const double rawNormal = translation.Dot(normal);
    TVector constraintAxis = constraintAxisInput;
    const bool constrained = finite(constraintAxis)
        && constraintAxis.Dot(constraintAxis) > sed::math::kTinyEpsilonF;
    double constraintNormal = 1.0f;
    if (constrained)
    {
        constraintAxis = constraintAxis * (1.0 / std::hypot(constraintAxis.x, constraintAxis.y, constraintAxis.z));
        constraintNormal = constraintAxis.Dot(normal);
        if (std::abs(constraintNormal) <= sed::math::kTinyEpsilonF)
        {
            // This handle cannot change the distance to the grid plane.
            state = {};
            return translation;
        }
    }

    auto withNormalTranslation = [&](const double desiredNormal)
    {
        const double normalCorrection = desiredNormal - rawNormal;
        if (constrained)
        {
            return translation + constraintAxis * (normalCorrection / constraintNormal);
        }
        return translation + normal * normalCorrection;
    };

    if (state.released)
    {
        // Keep a constant normal-only clutch offset after release. This makes
        // the first released sample equal the held point and subsequent motion
        // pass through continuously without re-capturing this plane.
        return withNormalTranslation(rawNormal + state.releaseOffset);
    }

    if (state.holding)
    {
        const double progress = state.direction * (rawNormal - state.contactTranslation);
        if (progress >= releaseDistance)
        {
            state.holding = false;
            state.released = true;
            state.releaseOffset = state.contactTranslation - rawNormal;
            return withNormalTranslation(state.contactTranslation);
        }

        if (progress < -captureDistance)
        {
            // The pointer backed away before breaking through; abandon this
            // tentative hold and allow a later approach during the same drag.
            state = {};
            return translation;
        }
        return withNormalTranslation(state.contactTranslation);
    }

    if (rawNormal == 0.0)
    {
        return translation;
    }

    const double direction = rawNormal > 0.0f ? 1.0f : -1.0f;
    double bestAlong = std::numeric_limits<double>::infinity();
    double bestContact = 0.0f;
    for (const Point& sourcePoint : movingPoints)
    {
        const TVector point = [&]()
        {
            if constexpr (std::is_same_v<Point, TVector>) return sourcePoint;
            else return TVector{sourcePoint.x(), sourcePoint.y(), sourcePoint.z()};
        }();
        if (!finite(point))
        {
            continue;
        }

        const double signedDistance = (point - origin).Dot(normal);
        if (!std::isfinite(signedDistance))
        {
            continue;
        }

        const double contact = -signedDistance;
        const double along = direction * contact;
        // Only support points on the current approach side can contact the
        // plane while moving in this direction. Keep this directional filter
        // even when the selection already spans the plane: room sectors often
        // surround the grid, and their approaching-side vertices still define
        // an unambiguous next contact.
        if (along < 0.0 || along >= bestAlong)
        {
            continue;
        }
        bestAlong = along;
        bestContact = contact;
    }

    if (!std::isfinite(bestAlong))
    {
        return translation;
    }

    const double progress = direction * (rawNormal - bestContact);
    if (progress < -captureDistance)
    {
        return translation;
    }

    // A low-frequency mouse sample can sweep all the way across the capture
    // and release bands. Still present one held sample at contact; the next
    // same-direction sample releases through the normal-only clutch above.
    const double rawAlong = direction * rawNormal;
    const bool sweptAcrossContact = bestAlong <= rawAlong;
    if (progress >= releaseDistance && !sweptAcrossContact)
    {
        return translation;
    }

    state.holding = true;
    state.direction = direction;
    state.contactTranslation = bestContact;
    return withNormalTranslation(bestContact);
}

QVector3D applyGridPlaneStickThrough(
    const QVector3D& translation, const std::span<const QVector3D> movingPoints,
    const TransformGridFrame& frame, const float captureDistance, const float releaseDistance,
    GridPlaneStickState& state, const QVector3D& constraintAxis) noexcept
{
    const TVector result = applyGridPlaneStickThroughImpl(TVector{translation.x(), translation.y(), translation.z()},
        movingPoints, frame, captureDistance, releaseDistance, state,
        TVector{constraintAxis.x(), constraintAxis.y(), constraintAxis.z()});
    return QVector3D(static_cast<float>(result.x), static_cast<float>(result.y), static_cast<float>(result.z));
}

TVector applyGridPlaneStickThrough(
    const TVector& translation, const std::span<const TVector> movingPoints,
    const TransformGridFrame& frame, const double captureDistance, const double releaseDistance,
    GridPlaneStickState& state, const TVector& constraintAxis) noexcept
{
    return applyGridPlaneStickThroughImpl(translation, movingPoints, frame,
        captureDistance, releaseDistance, state, constraintAxis);
}

float stableGridCoordPhase(const double coord, const double spacing) noexcept
{
    if (!std::isfinite(coord) || !std::isfinite(spacing) || spacing <= sed::math::kTinyEpsilon)
    {
        return 0.0f;
    }

    double phase = std::remainder(coord, spacing);
    if (!std::isfinite(phase))
    {
        phase = 0.0;
    }
    return static_cast<float>(phase);
}

GridLineBuild buildGridLineVertices(const GridLineBuildParams& params)
{
    GridLineBuild out{};
    if (!isFiniteVector(params.axisA)
        || !isFiniteVector(params.axisB)
        || !isFiniteVector(params.coordOrigin)
        || !isFiniteVector(params.viewCenter)
        || !std::isfinite(params.halfVisible)
        || !std::isfinite(params.majorSpacing)
        || !std::isfinite(params.minorSpacing)
        || params.halfVisible <= 0.0f
        || params.majorSpacing <= 0.0
        || params.minorSpacing <= 0.0
        || params.majorEvery <= 0)
    {
        return out;
    }

    const double halfWorldMajor =
        std::max(params.majorSpacing, params.majorSpacing * std::ceil(static_cast<double>(params.halfVisible) / params.majorSpacing));
    const double rawMajorCount = std::ceil(halfWorldMajor / params.majorSpacing);
    const int nMajor = static_cast<int>(std::clamp(rawMajorCount, 2.0, 256.0));

    const double extentWorld = params.majorSpacing * static_cast<double>(nMajor);
    if (!std::isfinite(extentWorld)
        || extentWorld > static_cast<double>(std::numeric_limits<float>::max()))
    {
        return out;
    }
    const double rawMinorCount = std::ceil(extentWorld / params.minorSpacing);
    const int n = static_cast<int>(std::clamp(rawMinorCount, 2.0, 4096.0));

    const float extent = static_cast<float>(extentWorld);

    std::vector<GridLineVertex> minor{};
    std::vector<GridLineVertex> minorPoints{};
    std::vector<GridLineVertex> major{};
    std::vector<GridLineVertex> axisAline{};
    std::vector<GridLineVertex> axisBline{};
    const std::size_t latticeSide = static_cast<std::size_t>(2 * n + 1);
    const bool pointBudgetFits = latticeSide <= kGridMinorPointVertexBudget / latticeSide;
    if ( params.minorDots && pointBudgetFits )
    {
        minorPoints.reserve(latticeSide * latticeSide);
    }
    else if ( !params.minorDots )
    {
        minor.reserve(static_cast<std::size_t>((2 * n + 1) * 4));
    }
    major.reserve(static_cast<std::size_t>((2 * n + 1) * 4) / static_cast<std::size_t>(params.majorEvery));
    axisAline.reserve(2);
    axisBline.reserve(2);

    // A fallback VBO has a strict line-count budget. When the requested
    // minor lattice is far denser than that budget, drawing only a narrow
    // 4096-cell strip around the view centre is visibly wrong and can omit
    // every major line. Minor cells are unresolved in this case anyway, so
    // retain the exact major lattice across the complete visible extent.
    if (rawMinorCount > 4096.0)
    {
        const double invMajor = 1.0 / params.majorSpacing;
        const QVector3D rel = params.viewCenter - params.coordOrigin;
        const double baseAValue = std::floor(
            static_cast<double>(QVector3D::dotProduct(rel, params.axisA)) * invMajor);
        const double baseBValue = std::floor(
            static_cast<double>(QVector3D::dotProduct(rel, params.axisB)) * invMajor);
        // Keep the margin larger than double's ULP near int64 limits so the
        // subsequent +/- nMajor arithmetic is provably in range before cast.
        constexpr double kIndexMargin = 4096.0;
        const double minIndex = static_cast<double>(
            std::numeric_limits<std::int64_t>::min()) + kIndexMargin;
        const double maxIndex = static_cast<double>(
            std::numeric_limits<std::int64_t>::max()) - kIndexMargin;
        if (!std::isfinite(baseAValue) || !std::isfinite(baseBValue)
            || baseAValue < minIndex || baseAValue > maxIndex
            || baseBValue < minIndex || baseBValue > maxIndex)
        {
            return out;
        }

        const std::int64_t baseAi = static_cast<std::int64_t>(baseAValue);
        const std::int64_t baseBi = static_cast<std::int64_t>(baseBValue);
        const QVector3D origin = params.coordOrigin
            + params.axisA * static_cast<float>(baseAValue * params.majorSpacing)
            + params.axisB * static_cast<float>(baseBValue * params.majorSpacing);
        if (!isFiniteVector(origin))
        {
            return out;
        }

        for (int i = -nMajor; i <= nMajor; ++i)
        {
            const float offset = static_cast<float>(
                static_cast<double>(i) * params.majorSpacing);
            const std::int64_t indexB = baseBi + static_cast<std::int64_t>(i);
            pushGridLine(
                indexB == 0 ? axisAline : major,
                origin + params.axisB * offset - params.axisA * extent,
                origin + params.axisB * offset + params.axisA * extent);

            const std::int64_t indexA = baseAi + static_cast<std::int64_t>(i);
            pushGridLine(
                indexA == 0 ? axisBline : major,
                origin + params.axisA * offset - params.axisB * extent,
                origin + params.axisA * offset + params.axisB * extent);
        }

        out.vertices.reserve(major.size() + axisAline.size() + axisBline.size());
        out.vertices.insert(out.vertices.end(), major.begin(), major.end());
        out.majorVertexCount = static_cast<int>(major.size());
        out.vertices.insert(out.vertices.end(), axisAline.begin(), axisAline.end());
        out.axisAVertexCount = static_cast<int>(axisAline.size());
        out.vertices.insert(out.vertices.end(), axisBline.begin(), axisBline.end());
        out.axisBVertexCount = static_cast<int>(axisBline.size());
        return out;
    }

    const double invSpacing =
        (std::isfinite(params.minorSpacing) && std::abs(params.minorSpacing) > sed::math::kTinyEpsilon)
            ? (1.0 / params.minorSpacing)
            : 0.0;
    const QVector3D rel = params.viewCenter - params.coordOrigin;
    const double baseAValue = std::floor(
        static_cast<double>(QVector3D::dotProduct(rel, params.axisA)) * invSpacing);
    const double baseBValue = std::floor(
        static_cast<double>(QVector3D::dotProduct(rel, params.axisB)) * invSpacing);
    constexpr double kIndexMargin = 4096.0;
    const double minIndex = static_cast<double>(
        std::numeric_limits<std::int64_t>::min()) + kIndexMargin;
    const double maxIndex = static_cast<double>(
        std::numeric_limits<std::int64_t>::max()) - kIndexMargin;
    if (!std::isfinite(baseAValue) || !std::isfinite(baseBValue)
        || baseAValue < minIndex || baseAValue > maxIndex
        || baseBValue < minIndex || baseBValue > maxIndex)
    {
        return out;
    }
    const std::int64_t baseAi = static_cast<std::int64_t>(baseAValue);
    const std::int64_t baseBi = static_cast<std::int64_t>(baseBValue);

    const QVector3D origin =
        params.coordOrigin
        + params.axisA * static_cast<float>(static_cast<double>(baseAi) * params.minorSpacing)
        + params.axisB * static_cast<float>(static_cast<double>(baseBi) * params.minorSpacing);
    if (!isFiniteVector(origin))
    {
        return out;
    }

    if ( params.minorDots && pointBudgetFits )
    {
        // Emit the complete square of intersections or none of it. The check
        // above is deliberately performed before allocation and before this
        // nested loop, so dense zoom levels never create a partial lattice.
        for ( int a = -n; a <= n; ++a )
        {
            const float offsetA = static_cast<float>(static_cast<double>(a) * params.minorSpacing);
            for ( int b = -n; b <= n; ++b )
            {
                const float offsetB = static_cast<float>(static_cast<double>(b) * params.minorSpacing);
                pushGridPoint(
                    minorPoints,
                    origin + params.axisA * offsetA + params.axisB * offsetB);
            }
        }
    }

    // Step across both axes and bucket each line by visual importance.
    for (int i = -n; i <= n; ++i)
    {
        const float ofs = static_cast<float>(static_cast<double>(i) * params.minorSpacing);

        {
            const std::int64_t idxB = baseBi + static_cast<std::int64_t>(i);
            const bool isMajorB = idxB == 0 || (idxB % params.majorEvery) == 0;
            if ( !params.minorDots || isMajorB )
            {
                std::vector<GridLineVertex>& dst = idxB == 0
                    ? axisAline
                    : (isMajorB ? major : minor);
                pushGridLine(
                    dst,
                    origin + params.axisB * ofs - params.axisA * extent,
                    origin + params.axisB * ofs + params.axisA * extent);
            }
        }

        {
            const std::int64_t idxA = baseAi + static_cast<std::int64_t>(i);
            const bool isMajorA = idxA == 0 || (idxA % params.majorEvery) == 0;
            if ( !params.minorDots || isMajorA )
            {
                std::vector<GridLineVertex>& dst = idxA == 0
                    ? axisBline
                    : (isMajorA ? major : minor);
                pushGridLine(
                    dst,
                    origin + params.axisA * ofs - params.axisB * extent,
                    origin + params.axisA * ofs + params.axisB * extent);
            }
        }
    }

    out.vertices.reserve(minorPoints.size() + minor.size() + major.size() + axisAline.size() + axisBline.size());
    out.vertices.insert(out.vertices.end(), minorPoints.begin(), minorPoints.end());
    out.minorPointVertexCount = static_cast<int>(minorPoints.size());
    out.vertices.insert(out.vertices.end(), minor.begin(), minor.end());
    out.minorVertexCount = static_cast<int>(minor.size());
    out.vertices.insert(out.vertices.end(), major.begin(), major.end());
    out.majorVertexCount = static_cast<int>(major.size());
    out.vertices.insert(out.vertices.end(), axisAline.begin(), axisAline.end());
    out.axisAVertexCount = static_cast<int>(axisAline.size());
    out.vertices.insert(out.vertices.end(), axisBline.begin(), axisBline.end());
    out.axisBVertexCount = static_cast<int>(axisBline.size());
    return out;
}

} // namespace sed::render
