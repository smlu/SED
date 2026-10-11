#include "viewport_camera.hpp"

#include "sed/math_utils.hpp"

#include <QQuaternion>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace sed::app
{
namespace
{

constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;

constexpr float degToRad(const float deg) noexcept
{
    return deg * kDegToRad;
}

constexpr float radToDeg(const float rad) noexcept
{
    return rad * kRadToDeg;
}

float wrapAngle180(float deg) noexcept
{
    deg = std::fmod(deg + 180.0f, 360.0f);
    if (deg < 0.0f)
    {
        deg += 360.0f;
    }
    return deg - 180.0f;
}

float unwrapAngleNear(const float angleDeg, const float referenceDeg) noexcept
{
    return referenceDeg + wrapAngle180(angleDeg - referenceDeg);
}

} // namespace

float ViewportCamera::ClampPitch(const float pitchDeg) noexcept
{
    return std::clamp(pitchDeg, -90.0f, 90.0f);
}

float ViewportCamera::NormalizePitch(const float pitchDeg) noexcept
{
    if (!std::isfinite(pitchDeg))
    {
        return 0.0f;
    }

    float wrapped = std::fmod(pitchDeg + 180.0f, 360.0f);
    if (wrapped < 0.0f)
    {
        wrapped += 360.0f;
    }
    return wrapped - 180.0f;
}

QVector3D ViewportCamera::SphericalDirection(const float yawDeg, const float pitchDeg) noexcept
{
    const float yawRad = degToRad(yawDeg);
    const float pitchRad = degToRad(pitchDeg);
    const float cp = std::cos(pitchRad);
    // Editor/world convention: X = right, Y = forward, Z = up.
    //
    // Match the game's yaw convention (rdMatrix_BuildRotate34):
    // - yaw=0 looks toward +Y (forward)
    // - yaw=+90 looks toward -X
    //
    // Here we return the orbit direction from target -> camera.
    // So yaw=0 places the camera on -Y looking toward +Y.
    return QVector3D(cp * std::sin(yawRad), -cp * std::cos(yawRad), std::sin(pitchRad));
}

ViewportCamera::OrbitBasis ViewportCamera::BasisFromOrbit(
    const float yawDeg,
    const float pitchDeg,
    const float rollDeg) noexcept
{
    const float safeYawDeg = std::isfinite(yawDeg) ? yawDeg : 0.0f;
    const float safePitchDeg = NormalizePitch(pitchDeg);
    const float safeRollDeg = std::isfinite(rollDeg) ? rollDeg : 0.0f;

    OrbitBasis basis{};
    basis.forward = -SphericalDirection(safeYawDeg, safePitchDeg);
    if (basis.forward.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        basis.forward = QVector3D(0.0f, 1.0f, 0.0f);
    }
    else
    {
        basis.forward.normalize();
    }

    // Keep yaw in charge of screen-right over the poles.  The common
    // cross(forward, worldUp) construction changes sign as cos(pitch) does,
    // which mirrors the viewport immediately after +/-90 degrees.
    const float yawRad = degToRad(safeYawDeg);
    basis.right = QVector3D(std::cos(yawRad), std::sin(yawRad), 0.0f);
    if (basis.right.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        basis.right = QVector3D(1.0f, 0.0f, 0.0f);
    }
    else
    {
        basis.right.normalize();
    }

    basis.up = QVector3D::crossProduct(basis.right, basis.forward);
    if (basis.up.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        basis.up = QVector3D(0.0f, 0.0f, 1.0f);
    }
    else
    {
        basis.up.normalize();
    }

    if (std::abs(safeRollDeg) > 1.0e-3f)
    {
        const QQuaternion rotation = QQuaternion::fromAxisAndAngle(
            basis.forward, safeRollDeg);
        basis.right = rotation.rotatedVector(basis.right).normalized();
        basis.up = rotation.rotatedVector(basis.up).normalized();
    }
    return basis;
}

namespace
{

ViewportCamera::OrbitAngles orbitAnglesFromBasis(
    QVector3D forward,
    QVector3D right,
    const float referenceYaw,
    const float referencePitch,
    const float referenceRoll) noexcept
{
    if (forward.lengthSquared() <= sed::math::kTinyEpsilonF
        || right.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return {};
    }
    forward.normalize();
    right -= forward * QVector3D::dotProduct(right, forward);
    if (right.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return {};
    }
    right.normalize();

    const QVector3D direction = -forward;
    const float horizontal = std::hypot(direction.x(), direction.y());
    const float principalPitch = radToDeg(std::atan2(direction.z(), horizontal));
    const float principalYaw = horizontal > 1.0e-5f
        ? radToDeg(std::atan2(direction.x(), -direction.y()))
        : referenceYaw;

    const auto makeCandidate = [&](const float candidateYaw,
                                   const float candidatePitch)
    {
        const ViewportCamera::OrbitBasis unrolled =
            ViewportCamera::BasisFromOrbit(
                candidateYaw, candidatePitch, 0.0f);
        const float candidateRoll = radToDeg(std::atan2(
            -QVector3D::dotProduct(right, unrolled.up),
            QVector3D::dotProduct(right, unrolled.right)));
        return ViewportCamera::OrbitAngles{
            .yawDeg = unwrapAngleNear(candidateYaw, referenceYaw),
            .pitchDeg = ViewportCamera::NormalizePitch(candidatePitch),
            .rollDeg = unwrapAngleNear(candidateRoll, referenceRoll),
            .valid = true,
        };
    };

    const ViewportCamera::OrbitAngles principal = makeCandidate(
        principalYaw, principalPitch);
    const float alternatePitch = principalPitch >= 0.0f
        ? 180.0f - principalPitch
        : -180.0f - principalPitch;
    const ViewportCamera::OrbitAngles alternate = makeCandidate(
        principalYaw + 180.0f, alternatePitch);

    const auto candidateCost = [&](const ViewportCamera::OrbitAngles& candidate)
    {
        const float yawDistance = wrapAngle180(
            candidate.yawDeg - referenceYaw);
        const float pitchDistance = wrapAngle180(
            candidate.pitchDeg - referencePitch);
        const float rollDistance = wrapAngle180(
            candidate.rollDeg - referenceRoll);
        return yawDistance * yawDistance
            + pitchDistance * pitchDistance
            + rollDistance * rollDistance;
    };
    return candidateCost(alternate) < candidateCost(principal)
        ? alternate
        : principal;
}

} // namespace

ViewportCamera::OrbitAngles ViewportCamera::RotatedOrbit(
    const float yawDeg,
    const float pitchDeg,
    const float rollDeg,
    const float yawDeltaDeg,
    const float pitchDeltaDeg) noexcept
{
    const bool finite = std::isfinite(yawDeg)
        && std::isfinite(pitchDeg)
        && std::isfinite(rollDeg)
        && std::isfinite(yawDeltaDeg)
        && std::isfinite(pitchDeltaDeg);
    if (!finite)
    {
        return {};
    }
    if (yawDeltaDeg == 0.0f && pitchDeltaDeg == 0.0f)
    {
        return OrbitAngles{
            .yawDeg = yawDeg,
            .pitchDeg = pitchDeg,
            .rollDeg = rollDeg,
            .valid = true,
        };
    }

    const OrbitBasis current = BasisFromOrbit(yawDeg, pitchDeg, rollDeg);
    const QQuaternion yawRotation = QQuaternion::fromAxisAndAngle(
        QVector3D(0.0f, 0.0f, 1.0f), yawDeltaDeg);
    QVector3D pitchAxis = yawRotation.rotatedVector(current.right);
    if (pitchAxis.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return {};
    }
    pitchAxis.normalize();
    const QQuaternion pitchRotation = QQuaternion::fromAxisAndAngle(
        pitchAxis, -pitchDeltaDeg);
    const QQuaternion frameRotation = pitchRotation * yawRotation;

    const float referenceYaw = yawDeg + yawDeltaDeg;
    const float referencePitch = NormalizePitch(pitchDeg + pitchDeltaDeg);
    return orbitAnglesFromBasis(
        frameRotation.rotatedVector(current.forward),
        frameRotation.rotatedVector(current.right),
        referenceYaw,
        referencePitch,
        rollDeg);
}

ViewportCamera::OrbitAngles ViewportCamera::UprightFlyOrbit(
    const float yawDeg,
    const float pitchDeg,
    const float rollDeg) noexcept
{
    const bool finite = std::isfinite(yawDeg)
        && std::isfinite(pitchDeg)
        && std::isfinite(rollDeg);
    if (!finite)
    {
        return {};
    }

    const OrbitBasis current = BasisFromOrbit(yawDeg, pitchDeg, rollDeg);
    const QVector3D direction = -current.forward;
    const float horizontal = std::hypot(direction.x(), direction.y());
    float uprightYawDeg = yawDeg;
    if (horizontal > 1.0e-5f)
    {
        uprightYawDeg = radToDeg(std::atan2(
            direction.x(), -direction.y()));
    }
    else
    {
        // At a pole the forward vector cannot own yaw. Prefer the horizontal
        // component of the actually visible right axis so entering Fly has a
        // deterministic, minimally surprising heading before roll is removed.
        QVector3D horizontalRight(
            current.right.x(), current.right.y(), 0.0f);
        if (horizontalRight.lengthSquared() > sed::math::kTinyEpsilonF)
        {
            horizontalRight.normalize();
            uprightYawDeg = radToDeg(std::atan2(
                horizontalRight.y(), horizontalRight.x()));
        }
    }

    return OrbitAngles{
        .yawDeg = unwrapAngleNear(uprightYawDeg, yawDeg),
        .pitchDeg = ClampPitch(radToDeg(std::atan2(
            direction.z(), horizontal))),
        .rollDeg = 0.0f,
        .valid = true,
    };
}

float ViewportCamera::CalcVFovFromHFov(const float hfovDeg, const float aspect) noexcept
{
    if (aspect <= 1e-6f)
    {
        return hfovDeg;
    }

    const float hfovRad = degToRad(hfovDeg);
    const float vfovRad = 2.0f * std::atan(std::tan(hfovRad * 0.5f) / aspect);
    return radToDeg(vfovRad);
}

void ViewportCamera::Reset() noexcept
{
    canvasWidth_    = 1;
    canvasHeight_   = 1;
    aspect_         = 1.0f;
    projectionType_ = ProjectionType::Orthographic;
    fovXDeg_        = 90.0f;
    orthoScale_     = 64.0f;

    perspNearPlane_ = kPerspectiveNearPlaneDefault;
    perspFarPlane_  = kPerspectiveFarPlaneDefault;
    orthoNearPlane_ = kOrthoNearPlaneDefault;
    orthoFarPlane_  = kOrthoFarPlaneDefault;
    clipEnabled_ = true;

    orbit = OrbitState{};

    position_ = QVector3D(0.0f, 0.0f, 0.0f);
    target_   = QVector3D(0.0f, 0.0f, 0.0f);
    pyrDeg_   = QVector3D(0.0f, 0.0f, 0.0f);
    focalLength_ = 0.0f;

    view_.setToIdentity();
    projection_.setToIdentity();

    derivedDirty_ = true;
    viewDirty_ = true;
    projectionDirty_ = true;
}

void ViewportCamera::SetCanvas(const int pixelWidth, const int pixelHeight) noexcept
{
    const int w = std::max(1, pixelWidth);
    const int h = std::max(1, pixelHeight);

    if (canvasWidth_ == w && canvasHeight_ == h)
    {
        return;
    }

    canvasWidth_  = w;
    canvasHeight_ = h;
    derivedDirty_ = true;
}

void ViewportCamera::SetProjectionType(const ProjectionType type) noexcept
{
    if (projectionType_ == type)
    {
        return;
    }

    projectionType_ = type;

    if (projectionType_ == ProjectionType::Perspective)
    {
        perspNearPlane_ = std::max(perspNearPlane_, kPerspectiveNearPlaneMin);
        perspFarPlane_ = std::max(perspFarPlane_, perspNearPlane_ + 0.01f);
    }
    else
    {
        orthoNearPlane_ = std::min(orthoNearPlane_, 0.0f);
        orthoFarPlane_ = std::max(orthoFarPlane_, orthoNearPlane_ + 0.01f);
    }

    derivedDirty_ = true;
    projectionDirty_ = true;
}

void ViewportCamera::SetFovXDeg(const float fovXDeg) noexcept
{
    const float clamped = std::clamp(fovXDeg, 1.0f, 179.0f);
    if (qFuzzyCompare(fovXDeg_, clamped))
    {
        return;
    }

    fovXDeg_ = clamped;
    derivedDirty_ = true;
    projectionDirty_ = true;
}

void ViewportCamera::SetOrthoScale(const float scale) noexcept
{
    if (scale <= 0.0f)
    {
        return;
    }

    if (qFuzzyCompare(orthoScale_, scale))
    {
        return;
    }

    orthoScale_ = scale;
    projectionDirty_ = true;
}

void ViewportCamera::SetNearPlane(const float nearPlane) noexcept
{
    const float clamped =
        (projectionType_ == ProjectionType::Perspective)
            ? std::max(nearPlane, kPerspectiveNearPlaneMin)
            : std::min(nearPlane, 0.0f);

    float& dstNear = (projectionType_ == ProjectionType::Perspective) ? perspNearPlane_ : orthoNearPlane_;
    float& dstFar = (projectionType_ == ProjectionType::Perspective) ? perspFarPlane_ : orthoFarPlane_;

    if (qFuzzyCompare(dstNear, clamped))
    {
        return;
    }

    dstNear = clamped;
    dstFar = std::max(dstFar, dstNear + 0.01f);
    projectionDirty_ = true;
}

void ViewportCamera::SetFarPlane(const float farPlane) noexcept
{
    float& dstNear = (projectionType_ == ProjectionType::Perspective) ? perspNearPlane_ : orthoNearPlane_;
    float& dstFar = (projectionType_ == ProjectionType::Perspective) ? perspFarPlane_ : orthoFarPlane_;

    const float clamped = std::max(farPlane, dstNear + 0.01f);

    if (qFuzzyCompare(dstFar, clamped))
    {
        return;
    }

    dstFar = clamped;
    projectionDirty_ = true;
}

void ViewportCamera::SetNearFar(const float nearPlane, const float farPlane) noexcept
{
    SetNearPlane(nearPlane);
    SetFarPlane(farPlane);
}

void ViewportCamera::SetClipEnabled(const bool enabled) noexcept
{
    if (clipEnabled_ == enabled)
    {
        return;
    }

    clipEnabled_ = enabled;
    projectionDirty_ = true;
}

float ViewportCamera::orthoLeft() const noexcept
{
    const float hwidth = static_cast<float>(canvasWidth_) * 0.5f;
    return -((hwidth / orthoScale_) / std::max(aspect_, 1e-6f));
}

float ViewportCamera::orthoRight() const noexcept
{
    const float hwidth = static_cast<float>(canvasWidth_) * 0.5f;
    return ((hwidth / orthoScale_) / std::max(aspect_, 1e-6f));
}

float ViewportCamera::orthoTop() const noexcept
{
    const float hheight = static_cast<float>(canvasHeight_) * 0.5f;
    return (hheight / orthoScale_) / std::max(aspect_, 1e-6f);
}

float ViewportCamera::orthoBottom() const noexcept
{
    const float hheight = static_cast<float>(canvasHeight_) * 0.5f;
    return -((hheight / orthoScale_) / std::max(aspect_, 1e-6f));
}

void ViewportCamera::UpdateFromOrbit() noexcept
{
    orbit.pitchDeg = NormalizePitch(orbit.pitchDeg);

    target_ = orbit.target();
    const QVector3D dir = SphericalDirection(orbit.yawDeg, orbit.pitchDeg);
    position_ = target_ + dir * orbit.distance;

    QVector3D look = target_ - position_;
    if (look.lengthSquared() > sed::math::kTinyEpsilonF)
    {
        look.normalize();
    }

    // Match the game's yaw convention where yaw=0 means looking along +Y.
    //
    // At exact poles (pitch +/-90), horizontal look components are near zero and the
    // atan2 result becomes numerically unstable. Keep the orbit yaw there so yaw edits
    // at top/bottom views are preserved instead of snapping to 0.
    float yawDeg = orbit.yawDeg;
    const float horizontalLen2 = (look.x() * look.x()) + (look.y() * look.y());
    if (horizontalLen2 > sed::math::kTinyEpsilonF)
    {
        yawDeg = radToDeg(std::atan2(-look.x(), look.y()));
    }
    const float pitchDeg = radToDeg(std::asin(std::clamp(look.z(), -1.0f, 1.0f)));
    // Beyond a pole, the same look direction has a canonical yaw shifted by
    // 180 degrees.  Carry the corresponding half-turn into roll so sky and
    // other view-oriented rendering follows the continuous camera frame.
    const bool inverted = std::cos(degToRad(orbit.pitchDeg)) < -1.0e-6f;
    pyrDeg_ = QVector3D(
        pitchDeg,
        yawDeg,
        orbit.rollDeg + (inverted ? 180.0f : 0.0f));

    viewDirty_ = true;
    Update();
}

void ViewportCamera::Update() noexcept
{
    if (derivedDirty_)
    {
        UpdateDerived();
        derivedDirty_ = false;
        projectionDirty_ = true;
    }

    if (viewDirty_)
    {
        UpdateViewMatrix();
        viewDirty_ = false;
    }

    if (projectionDirty_)
    {
        UpdateProjectionMatrix();
        projectionDirty_ = false;
    }
}

void ViewportCamera::UpdateDerived() noexcept
{
    aspect_ = (canvasHeight_ > 0) ? (static_cast<float>(canvasWidth_) / static_cast<float>(canvasHeight_)) : 1.0f;

    if (projectionType_ == ProjectionType::Orthographic)
    {
        focalLength_ = 0.0f;
        return;
    }

    const float hwidth = static_cast<float>(canvasWidth_) * 0.5f;
    const float halfFovRad = degToRad(fovXDeg_) * 0.5f;
    const float tanHalfFov = std::tan(std::max(halfFovRad, 1e-6f));
    focalLength_ = (tanHalfFov > 0.0f) ? (hwidth / tanHalfFov) : 0.0f;
}

void ViewportCamera::UpdateViewMatrix() noexcept
{
    view_.setToIdentity();

    const OrbitBasis basis = BasisFromOrbit(
        orbit.yawDeg, orbit.pitchDeg, orbit.rollDeg);
    view_.lookAt(position_, target_, basis.up);
}

void ViewportCamera::UpdateProjectionMatrix() noexcept
{
    float nearZ = (projectionType_ == ProjectionType::Perspective) ? perspNearPlane_ : orthoNearPlane_;
    float farZ = (projectionType_ == ProjectionType::Perspective) ? perspFarPlane_ : orthoFarPlane_;

    if (!clipEnabled_)
    {
        if (projectionType_ == ProjectionType::Perspective)
        {
            nearZ = kPerspectiveNearPlaneMin;
            farZ = 100000.0f;
        }
        else
        {
            nearZ = -100000.0f;
            farZ = 100000.0f;
        }
    }

    projection_.setToIdentity();

    if (projectionType_ == ProjectionType::Perspective)
    {
        nearZ = std::max(nearZ, kPerspectiveNearPlaneMin);
        farZ = std::max(farZ, nearZ + 0.01f);

        const float fovYDeg = CalcVFovFromHFov(fovXDeg_, aspect_);
        projection_.perspective(fovYDeg, aspect_, nearZ, farZ);
        return;
    }

    if (farZ < nearZ)
    {
        std::swap(nearZ, farZ);
    }
    if (qFuzzyCompare(farZ, nearZ))
    {
        farZ = nearZ + 1.0f;
    }

    projection_.ortho(orthoLeft(), orthoRight(), orthoBottom(), orthoTop(), nearZ, farZ);
}

} // namespace sed::app
