#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "sed/math.hpp"
#include "sed/math_utils.hpp"

namespace sed
{

inline constexpr double CloseEnough = 10e-5;

inline bool SameValue(const double a, const double b, const double epsilon) noexcept
{
    return sed::math::equal(a, b, epsilon);
}

inline bool IsZero(const double v) noexcept
{
    return sed::math::isZero(v, std::numeric_limits<double>::epsilon());
}

inline constexpr double DegToRad(const double deg) noexcept
{
    return sed::math::degToRad(deg);
}

inline constexpr double RadToDeg(const double rad) noexcept
{
    return sed::math::radToDeg(rad);
}

// Polygon face flags
inline constexpr std::uint32_t FF_DoubleSided = 0x01;
inline constexpr std::uint32_t FF_Transluent = 0x02;
inline constexpr std::uint32_t FF_TexClampX = 0x04;
inline constexpr std::uint32_t FF_TexClampY = 0x08;
inline constexpr std::uint32_t FF_TexNoFiltering = 0x10;
inline constexpr std::uint32_t FF_ZWriteDisabled = 0x20;

// IJIM flags
inline constexpr std::uint32_t FF_IJIM_3doLedge = 0x40;
inline constexpr std::uint32_t FF_IJIM_FogEnabled = 0x100;
inline constexpr std::uint32_t FF_IJIM_3doWhipAim = 0x200;

// Renderer flags
inline constexpr std::uint32_t FF_RD_CeilingSky = 1u << 24;
inline constexpr std::uint32_t FF_RD_HorizonSky = 1u << 25;
inline constexpr std::uint32_t FF_RD_SpecialSurfaceWf = 1u << 26;
inline constexpr std::uint32_t FF_RD_SolidSurfaceWf = 1u << 27;
inline constexpr std::uint32_t FF_RD_FloorWf = 1u << 28;
inline constexpr std::uint32_t FF_RD_AlphaRef = 1u << 29;
inline constexpr std::uint32_t FF_RD_Adjoin = 1u << 30;

// SED flags
inline constexpr std::uint32_t FF_SF_FLIP = 1u << 31;

enum class TGeoMode : std::int32_t
{
    NotDrawn  = 0,
    Vertex    = 1,
    Wireframe = 2,
    Solid     = 3,
    Texture   = 4
};

enum class TLightMode : std::int32_t
{
    None     = 0,
    Lit      = 1,
    Diffuse  = 2,
    Gouraud  = 3,
    Gouraud2 = 4,
    Gouraud3 = 5,
    VertexDiffuseOnly = -1
};

struct TPoint2
{
    double x{};
    double y{};

    static constexpr TPoint2 Create(const double px, const double py) noexcept
    {
        return TPoint2{ px, py };
    }
};

struct TVector2
{
    double x{};
    double y{};

    bool IsZero() const noexcept
    {
        return sed::IsZero(x) && sed::IsZero(y);
    }

    TVector2 Add(const TVector2& v) const noexcept;
    double Cross(const TVector2& v) const noexcept;
    double Distance(const TVector2& v) const noexcept;
    double Distance(const double x, const double y) const noexcept;
    double DistanceSqr(const TVector2& v) const noexcept;
    double DistanceSqr(const double x, const double y) const noexcept;
    double Dot(const TVector2& v) const noexcept;
    double Length() const noexcept;
    double LengthSqr() const noexcept;
    double Normalize() noexcept;
    TVector2 Normalized() const noexcept;
    void Scale(const double scalar) noexcept;
    TVector2 Scaled(const double scalar) const noexcept;
    TVector2 Sub(const TVector2& v) const noexcept;

    friend TVector2 operator+(const TVector2& a, const TVector2& b) noexcept
    {
        return a.Add(b);
    }
    friend TVector2 operator-(const TVector2& a, const TVector2& b) noexcept
    {
        return a.Sub(b);
    }
    friend TVector2 operator-(const TVector2& v) noexcept
    {
        return TVector2{ -v.x, -v.y };
    }
    friend TVector2 operator*(const TVector2& v, const double scalar) noexcept
    {
        return v.Scaled(scalar);
    }
    friend TVector2 operator*(const double scalar, const TVector2& v) noexcept
    {
        return v.Scaled(scalar);
    }
    friend bool operator==(const TVector2& a, const TVector2& b) noexcept
    {
        return SameValue(a.x, b.x, CloseEnough) && SameValue(a.y, b.y, CloseEnough);
    }
    friend bool operator!=(const TVector2& a, const TVector2& b) noexcept
    {
        return !(a == b);
    }
};

struct TVector
{
#if defined(_MSC_VER) || defined(__clang__) || defined(__GNUC__)
    union
    {
        struct
        {
            double dx, dy, dz;
        };
        struct
        {
            double x, y, z;
        };
        struct
        {
            double pitch, yaw, roll;
        };
    };
#else
    double x{};
    double y{};
    double z{};
#endif

    constexpr TVector() noexcept : x(0.0), y(0.0), z(0.0)
    {
    }
    constexpr TVector(const double x_, const double y_, const double z_) noexcept : x(x_), y(y_), z(z_)
    {
    }

    static const TVector back;
    static const TVector down;
    static const TVector forward;
    static const TVector left;
    static const TVector right;
    static const TVector up;
    static const TVector zero;

    static constexpr TVector Create(const double x, const double y, const double z) noexcept
    {
        return TVector{ x, y, z };
    }

    bool IsZero() const noexcept
    {
        return sed::IsZero(x) && sed::IsZero(y) && sed::IsZero(z);
    }
    void GetCoords(double& xOut, double& yOut, double& zOut) const noexcept;
    void SetCoords(const double dx, const double dy, const double dz) noexcept;

    TVector Add(const TVector& v) const noexcept;
    TVector Add(const double x, const double y, const double z) const noexcept;
    TVector Cross(const TVector& v) const noexcept;
    TVector Cross(const double x, const double y, const double z) const noexcept;
    double Distance(const TVector& v) const noexcept;
    double Distance(const double x, const double y, const double z) const noexcept;
    double DistanceSqr(const TVector& v) const noexcept;
    double DistanceSqr(const double x, const double y, const double z) const noexcept;
    double Dot(const TVector& v) const noexcept;
    double Dot(const double x, const double y, const double z) const noexcept;
    double Length() const noexcept;
    double LengthSqr() const noexcept;
    double Normalize() noexcept;
    TVector Normalized() const noexcept;
    TVector Project(const TVector& v) const noexcept;
    TVector Project(const double x, const double y, const double z) const noexcept;
    TVector ProjectOnToNormal(const TVector& normal) const noexcept;
    TVector ProjectOnToNormal(const double nx, const double ny, const double nz) const noexcept;
    void Scale(const double scalar) noexcept;
    TVector Scaled(const double scalar) const noexcept;
    TVector Sub(const TVector& v) const noexcept;
    TVector Sub(const double x, const double y, const double z) const noexcept;

    // Preferred lower-camel aliases for new/refactored code. The legacy
    // PascalCase API remains available because the editor still has many
    // existing call sites across core, app, UI, and render code.
    bool isZero() const noexcept
    {
        return IsZero();
    }
    void getCoords(double& xOut, double& yOut, double& zOut) const noexcept
    {
        GetCoords(xOut, yOut, zOut);
    }
    void setCoords(const double dx, const double dy, const double dz) noexcept
    {
        SetCoords(dx, dy, dz);
    }
    TVector add(const TVector& v) const noexcept
    {
        return Add(v);
    }
    TVector add(const double x, const double y, const double z) const noexcept
    {
        return Add(x, y, z);
    }
    TVector cross(const TVector& v) const noexcept
    {
        return Cross(v);
    }
    TVector cross(const double x, const double y, const double z) const noexcept
    {
        return Cross(x, y, z);
    }
    double distance(const TVector& v) const noexcept
    {
        return Distance(v);
    }
    double distance(const double x, const double y, const double z) const noexcept
    {
        return Distance(x, y, z);
    }
    double distanceSqr(const TVector& v) const noexcept
    {
        return DistanceSqr(v);
    }
    double distanceSqr(const double x, const double y, const double z) const noexcept
    {
        return DistanceSqr(x, y, z);
    }
    double dot(const TVector& v) const noexcept
    {
        return Dot(v);
    }
    double dot(const double x, const double y, const double z) const noexcept
    {
        return Dot(x, y, z);
    }
    double length() const noexcept
    {
        return Length();
    }
    double lengthSqr() const noexcept
    {
        return LengthSqr();
    }
    double normalize() noexcept
    {
        return Normalize();
    }
    TVector normalized() const noexcept
    {
        return Normalized();
    }
    TVector project(const TVector& v) const noexcept
    {
        return Project(v);
    }
    TVector project(const double x, const double y, const double z) const noexcept
    {
        return Project(x, y, z);
    }
    TVector projectOnToNormal(const TVector& normal) const noexcept
    {
        return ProjectOnToNormal(normal);
    }
    TVector projectOnToNormal(const double nx, const double ny, const double nz) const noexcept
    {
        return ProjectOnToNormal(nx, ny, nz);
    }
    void scale(const double scalar) noexcept
    {
        Scale(scalar);
    }
    TVector scaled(const double scalar) const noexcept
    {
        return Scaled(scalar);
    }
    TVector sub(const TVector& v) const noexcept
    {
        return Sub(v);
    }
    TVector sub(const double x, const double y, const double z) const noexcept
    {
        return Sub(x, y, z);
    }

    friend TVector operator+(const TVector& a, const TVector& b) noexcept
    {
        return a.Add(b);
    }
    friend TVector operator-(const TVector& a, const TVector& b) noexcept
    {
        return a.Sub(b);
    }
    friend TVector operator-(const TVector& v) noexcept
    {
        return TVector{ -v.x, -v.y, -v.z };
    }
    friend TVector operator*(const TVector& v, const double scalar) noexcept
    {
        return v.Scaled(scalar);
    }
    friend TVector operator*(const double scalar, const TVector& v) noexcept
    {
        return v.Scaled(scalar);
    }
    friend bool operator==(const TVector& a, const TVector& b) noexcept
    {
        return SameValue(a.x, b.x, CloseEnough) && SameValue(a.y, b.y, CloseEnough) && SameValue(a.z, b.z, CloseEnough);
    }
    friend bool operator!=(const TVector& a, const TVector& b) noexcept
    {
        return !(a == b);
    }
};

inline constexpr TVector TVector::back{ 0.0, -1.0, 0.0 };
inline constexpr TVector TVector::down{ 0.0, 0.0, -1.0 };
inline constexpr TVector TVector::forward{ 0.0, 1.0, 0.0 };
inline constexpr TVector TVector::left{ -1.0, 0.0, 0.0 };
inline constexpr TVector TVector::right{ 1.0, 0.0, 0.0 };
inline constexpr TVector TVector::up{ 0.0, 0.0, 1.0 };
inline constexpr TVector TVector::zero{ 0.0, 0.0, 0.0 };

template <typename T>
struct Box2
{
    T x1{};
    T y1{};
    T x2{};
    T y2{};

    constexpr bool IsZero() const noexcept
    {
        return sed::IsZero(static_cast<double>(x1)) && sed::IsZero(static_cast<double>(y1))
            && sed::IsZero(static_cast<double>(x2)) && sed::IsZero(static_cast<double>(y2));
    }

    constexpr void Set(const T ax1, const T ay1, const T ax2, const T ay2) noexcept
    {
        x1 = ax1;
        y1 = ay1;
        x2 = ax2;
        y2 = ay2;
    }
};

template <typename T>
struct Box3
{
    T x1{};
    T y1{};
    T z1{};
    T x2{};
    T y2{};
    T z2{};

    constexpr bool IsZero() const noexcept
    {
        return sed::IsZero(static_cast<double>(x1)) && sed::IsZero(static_cast<double>(y1)) && sed::IsZero(static_cast<double>(z1))
            && sed::IsZero(static_cast<double>(x2)) && sed::IsZero(static_cast<double>(y2)) && sed::IsZero(static_cast<double>(z2));
    }

    constexpr void Set(const T ax1, const T ay1, const T az1, const T ax2, const T ay2, const T az2) noexcept
    {
        x1 = ax1;
        y1 = ay1;
        z1 = az1;
        x2 = ax2;
        y2 = ay2;
        z2 = az2;
    }
};

using TBox2 = Box2<double>;
using TBox = Box3<double>;

struct TMatrix43
{
    TVector rvec{};
    TVector lvec{};
    TVector uvec{};
    TVector dvec{};

    static TMatrix43 identity() noexcept;

    friend TMatrix43 operator*(const TMatrix43& a, const TMatrix43& b) noexcept;

    TVector ExtractPYR() const noexcept;
    void LookAt(const TVector& eyePos, const TVector& lookPos, double angle) noexcept;
    void Normalize() noexcept;
    TVector TransformPoint(const TVector& point) const noexcept;
    TVector TransformVector(const TVector& vec) const noexcept;
    TVector TransformVectorOrtho(const TVector& vec) const noexcept;
    TMatrix43 InvertOrtho() const noexcept;

    static TMatrix43 Create(const TVector& pos, const TVector& pyr) noexcept;
    static TMatrix43 CreateFromLook(const TVector& lookDir) noexcept;
    static TMatrix43 CreateFromVectorAngle(const TVector& axis, float angle) noexcept;
    static TMatrix43 CreateRotate(const TVector& pyr) noexcept;
    static TMatrix43 CreateScale(const TVector& scale) noexcept;
    static TMatrix43 CreateTranslate(const TVector& vec) noexcept;

    TMatrix43 PreRotate(const TVector& pyr) const noexcept;
    TMatrix43 PostRotate(const TVector& pyr) const noexcept;
    TMatrix43 PreTranslate(const TVector& vec) const noexcept;
    TMatrix43 PostTranslate(const TVector& vec) const noexcept;
    TMatrix43 PreScale(const TVector& vec) const noexcept;
    TMatrix43 PostScale(const TVector& vec) const noexcept;
};

struct VecRotData
{
    TVector rvec{};
    TVector lvec{};
    TVector uvec{};
};

struct RotateData
{
    TVector center{};
    TMatrix43 m{};
};

void calcRotVecData(VecRotData& rd, double pch, double yaw, double rol) noexcept;
void rotVecByData(TVector& vec, const VecRotData& rd) noexcept;
void rotateVector(TVector& vec, double pch, double yaw, double rol) noexcept;
void getJKPYR(const TVector& x, const TVector& y, const TVector& z, double& pch, double& yaw, double& rol) noexcept;

void rotatePoint(double ax1, double ay1, double az1, double ax2, double ay2, double az2, double angleDeg, double& x, double& y, double& z) noexcept;

void prepareRotateData(RotateData& rd, const TVector& axis, double cx, double cy, double cz, double angleDeg) noexcept;
void rotateXYZ(double& x, double& y, double& z, const RotateData& rd) noexcept;

enum class ScaleHow : std::int32_t
{
    xyz,
    x,
    y,
    z,
    vec,
};

struct ScaleData
{
    double cx{};
    double cy{};
    double cz{};
    double sfactor{ 1.0 };
    // Direction for ScaleHow::vec. Callers may provide any finite non-zero
    // magnitude; transform primitives normalize it before projecting points.
    TVector vec{};
    ScaleHow how{ ScaleHow::xyz };
};

void scaleXYZ(double& x, double& y, double& z, const ScaleData& sd) noexcept;
/** Mirrors a point across a plane; planeNormal may have any finite non-zero magnitude. */
void flipXYZ(double& x, double& y, double& z, const TVector& planeNormal, double cx, double cy, double cz) noexcept;

bool CheckSphereFaceHit(
    const TVector& startPos,
    const TVector& moveNorm,
    double moveDistance,
    double radius,
    const TVector& normal,
    const TVector& point,
    double& sphereHitDist,
    std::int32_t flags) noexcept;

} // namespace sed
