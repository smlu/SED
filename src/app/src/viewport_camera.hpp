#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>

namespace sed::app
{

class ViewportCamera final
{
public:
    enum class ProjectionType : std::uint8_t
    {
        Perspective,
        Orthographic
    };

    static constexpr float kPerspectiveNearPlaneMin     = 0.001f; // 1/64
    static constexpr float kPerspectiveNearPlaneMax     = 0.001f; // 1/64
    static constexpr float kPerspectiveNearPlaneDefault = 0.015625f; // 1/64

    static constexpr float kPerspectiveFarPlaneMax     = 10000.0f;
    static constexpr float kPerspectiveFarPlaneDefault = 5000.0f;

    static constexpr float kOrthoNearPlaneMin     = -100000.0f;
    static constexpr float kOrthoNearPlaneMax     = 0.0f;
    static constexpr float kOrthoNearPlaneDefault = -10000.0f;

    static constexpr float kOrthoFarPlaneDefault  = 10000.0f;



    struct OrbitState
    {
        QVector3D center{ 0.0f, 0.0f, 0.0f };
        QVector3D pan{ 0.0f, 0.0f, 0.0f };
        float radius{ 1.0f };
        float distance{ 10.0f };
        float yawDeg{ -45.0f };
        float pitchDeg{ 30.0f };
        float rollDeg{ 0.0f };

        QVector3D target() const noexcept
        {
            return center + pan;
        }
    };

    /**
     * Complete camera frame derived from the orbit controls.
     *
     * Unlike a world-up look-at reconstruction, this frame remains continuous
     * when pitch passes through either pole.  In particular, screen-right is
     * owned by yaw instead of changing sign with cos(pitch).
     */
    struct OrbitBasis
    {
        QVector3D forward{ 0.0f, 1.0f, 0.0f };
        QVector3D right{ 1.0f, 0.0f, 0.0f };
        QVector3D up{ 0.0f, 0.0f, 1.0f };
    };

    struct OrbitAngles
    {
        float yawDeg{};
        float pitchDeg{};
        float rollDeg{};
        bool valid{};
    };

    /** Fly-look limiter; free orbit deliberately does not use this clamp. */
    static float ClampPitch(float pitchDeg) noexcept;
    /** Wraps free-orbit pitch to one equivalent revolution without pole clamps. */
    static float NormalizePitch(float pitchDeg) noexcept;
    static QVector3D SphericalDirection(float yawDeg, float pitchDeg) noexcept;
    static OrbitBasis BasisFromOrbit(
        float yawDeg,
        float pitchDeg,
        float rollDeg = 0.0f) noexcept;
    /**
     * Applies one free-orbit input sample to the complete camera frame.
     * Horizontal motion remains a world-Z yaw; vertical motion revolves
     * around the yawed current screen-right, including a bank installed by F2.
     */
    static OrbitAngles RotatedOrbit(
        float yawDeg,
        float pitchDeg,
        float rollDeg,
        float yawDeltaDeg,
        float pitchDeltaDeg) noexcept;
    /**
     * Re-expresses the current visible forward direction as conventional
     * world-upright Fly yaw/pitch.  Roll is removed and pitch is returned in
     * the constrained [-90, 90] range without changing the look direction.
     */
    static OrbitAngles UprightFlyOrbit(
        float yawDeg,
        float pitchDeg,
        float rollDeg) noexcept;
    static float CalcVFovFromHFov(float hfovDeg, float aspect) noexcept;

    void Reset() noexcept;

    void SetCanvas(int pixelWidth, int pixelHeight) noexcept;
    void SetProjectionType(ProjectionType type) noexcept;
    void SetFovXDeg(float fovXDeg) noexcept;
    void SetOrthoScale(float scale) noexcept;
    void SetNearPlane(float nearPlane) noexcept;
    void SetFarPlane(float farPlane) noexcept;
    void SetNearFar(float nearPlane, float farPlane) noexcept;
    void SetClipEnabled(bool enabled) noexcept;

    void UpdateFromOrbit() noexcept;
    void Update() noexcept;

    const QMatrix4x4& viewMatrix() const noexcept
    {
        return view_;
    }

    const QMatrix4x4& projectionMatrix() const noexcept
    {
        return projection_;
    }

    const QVector3D& eye() const noexcept
    {
        return position_;
    }

    const QVector3D& target() const noexcept
    {
        return target_;
    }

    const QVector3D& pyrDeg() const noexcept
    {
        return pyrDeg_;
    }

    int canvasWidth() const noexcept
    {
        return canvasWidth_;
    }

    int canvasHeight() const noexcept
    {
        return canvasHeight_;
    }

    float aspect() const noexcept
    {
        return aspect_;
    }

    ProjectionType projectionType() const noexcept
    {
        return projectionType_;
    }

    float fovXDeg() const noexcept
    {
        return fovXDeg_;
    }

    float nearPlaneMin() const noexcept
    {
        return (projectionType_ == ProjectionType::Perspective) ? kPerspectiveNearPlaneMin : kOrthoNearPlaneMin;
    }

    float nearPlaneMax() const noexcept
    {
        return (projectionType_ == ProjectionType::Perspective) ? kPerspectiveNearPlaneMax : kOrthoNearPlaneMax;
    }

    float nearPlaneDefault() const noexcept
    {
        return (projectionType_ == ProjectionType::Perspective) ? kPerspectiveNearPlaneDefault : kOrthoNearPlaneDefault;
    }

    float nearPlane() const noexcept
    {
        return (projectionType_ == ProjectionType::Perspective) ? perspNearPlane_ : orthoNearPlane_;
    }

    float farPlaneMin() const noexcept
    {
        return (projectionType_ == ProjectionType::Perspective) ? kPerspectiveNearPlaneMin + 0.01f : kOrthoNearPlaneMin + 0.01f;
    }

    float farPlaneMax() const noexcept
    {
        return (projectionType_ == ProjectionType::Perspective) ? kPerspectiveFarPlaneMax : std::numeric_limits<float>::max();
    }

    float farPlaneDefault() const noexcept
    {
        return (projectionType_ == ProjectionType::Perspective) ? kPerspectiveFarPlaneDefault : kOrthoFarPlaneDefault;
    }

    float farPlane() const noexcept
    {
        return (projectionType_ == ProjectionType::Perspective) ? perspFarPlane_ : orthoFarPlane_;
    }

    float focalLength() const noexcept
    {
        return focalLength_;
    }

    float orthoScale() const noexcept
    {
        return orthoScale_;
    }

    bool clipEnabled() const noexcept
    {
        return clipEnabled_;
    }

    float orthoLeft() const noexcept;
    float orthoRight() const noexcept;
    float orthoTop() const noexcept;
    float orthoBottom() const noexcept;

    OrbitState orbit{};

private:
    void UpdateDerived() noexcept;
    void UpdateViewMatrix() noexcept;
    void UpdateProjectionMatrix() noexcept;

    int canvasWidth_{ 1 };
    int canvasHeight_{ 1 };
    float aspect_{ 1.0f };

    ProjectionType projectionType_{ ProjectionType::Orthographic };
    float fovXDeg_{ 90.0f };
    float orthoScale_{ 64.0f };
    float perspNearPlane_{ kPerspectiveNearPlaneDefault };
    float perspFarPlane_{ kPerspectiveFarPlaneDefault };
    float orthoNearPlane_{ kOrthoNearPlaneDefault };
    float orthoFarPlane_{ kOrthoFarPlaneDefault };
    bool clipEnabled_{ true };

    QVector3D position_{ 0.0f, 0.0f, 0.0f };
    QVector3D target_{ 0.0f, 0.0f, 0.0f };
    QVector3D pyrDeg_{ 0.0f, 0.0f, 0.0f };
    float focalLength_{ 0.0f };

    QMatrix4x4 view_{};
    QMatrix4x4 projection_{};

    bool derivedDirty_{ true };
    bool viewDirty_{ true };
    bool projectionDirty_{ true };
};

} // namespace sed::app
