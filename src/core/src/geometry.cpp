#include "sed/geometry.hpp"

namespace sed
{

TVector2 TVector2::Add(const TVector2& v) const noexcept
{
    TVector2 result{};
    VectorAdd2(x, y, v.x, v.y, result.x, result.y);
    return result;
}

double TVector2::Cross(const TVector2& v) const noexcept
{
    return VectorCross2(x, y, v.x, v.y);
}

double TVector2::Distance(const TVector2& v) const noexcept
{
    return Distance(v.x, v.y);
}

double TVector2::Distance(const double x2, const double y2) const noexcept
{
    return VectorDist2(x, y, x2, y2);
}

double TVector2::DistanceSqr(const TVector2& v) const noexcept
{
    return DistanceSqr(v.x, v.y);
}

double TVector2::DistanceSqr(const double x2, const double y2) const noexcept
{
    return VectorDistSqr2(x, y, x2, y2);
}

double TVector2::Dot(const TVector2& v) const noexcept
{
    return VectorDot2(x, y, v.x, v.y);
}

double TVector2::Length() const noexcept
{
    return VectorLen2(x, y);
}

double TVector2::LengthSqr() const noexcept
{
    return VectorLenSqr2(x, y);
}

double TVector2::Normalize() noexcept
{
    return VectorNormalize2(x, y);
}

TVector2 TVector2::Normalized() const noexcept
{
    TVector2 result{};
    VectorNormalize2(x, y, result.x, result.y);
    return result;
}

void TVector2::Scale(const double scalar) noexcept
{
    VectorScale2(x, y, scalar, x, y);
}

TVector2 TVector2::Scaled(const double scalar) const noexcept
{
    TVector2 result = *this;
    result.Scale(scalar);
    return result;
}

TVector2 TVector2::Sub(const TVector2& v) const noexcept
{
    TVector2 result{};
    VectorSub2(x, y, v.x, v.y, result.x, result.y);
    return result;
}

void TVector::GetCoords(double& xOut, double& yOut, double& zOut) const noexcept
{
    xOut = dx;
    yOut = dy;
    zOut = dz;
}

void TVector::SetCoords(const double dx_, const double dy_, const double dz_) noexcept
{
    dx = dx_;
    dy = dy_;
    dz = dz_;
}

TVector TVector::Add(const TVector& v) const noexcept
{
    return Add(v.x, v.y, v.z);
}

TVector TVector::Add(const double x2, const double y2, const double z2) const noexcept
{
    TVector result{};
    VectorAdd3(x, y, z, x2, y2, z2, result.x, result.y, result.z);
    return result;
}

TVector TVector::Cross(const TVector& v) const noexcept
{
    return Cross(v.x, v.y, v.z);
}

TVector TVector::Cross(const double x2, const double y2, const double z2) const noexcept
{
    TVector result{};
    VectorCross3(x, y, z, x2, y2, z2, result.x, result.y, result.z);
    return result;
}

double TVector::Distance(const TVector& v) const noexcept
{
    return Distance(v.x, v.y, v.z);
}

double TVector::Distance(const double x2, const double y2, const double z2) const noexcept
{
    return VectorDist3(x, y, z, x2, y2, z2);
}

double TVector::DistanceSqr(const TVector& v) const noexcept
{
    return DistanceSqr(v.x, v.y, v.z);
}

double TVector::DistanceSqr(const double x2, const double y2, const double z2) const noexcept
{
    return VectorDistSqr3(x, y, z, x2, y2, z2);
}

double TVector::Dot(const TVector& v) const noexcept
{
    return Dot(v.x, v.y, v.z);
}

double TVector::Dot(const double x2, const double y2, const double z2) const noexcept
{
    return VectorDot3(x, y, z, x2, y2, z2);
}

double TVector::Length() const noexcept
{
    return VectorLen3(x, y, z);
}

double TVector::LengthSqr() const noexcept
{
    return VectorLenSqr3(x, y, z);
}

double TVector::Normalize() noexcept
{
    return VectorNormalize3(x, y, z);
}

TVector TVector::Normalized() const noexcept
{
    TVector result{};
    VectorNormalize3(x, y, z, result.x, result.y, result.z);
    return result;
}

TVector TVector::Project(const TVector& v) const noexcept
{
    return Project(v.x, v.y, v.z);
}

TVector TVector::Project(const double x2, const double y2, const double z2) const noexcept
{
    TVector result{};
    VectorProject3(x, y, z, x2, y2, z2, result.x, result.y, result.z);
    return result;
}

TVector TVector::ProjectOnToNormal(const TVector& normal) const noexcept
{
    return ProjectOnToNormal(normal.x, normal.y, normal.z);
}

TVector TVector::ProjectOnToNormal(const double nx, const double ny, const double nz) const noexcept
{
    TVector result{};
    VectorProjectOnToNormal3(x, y, z, nx, ny, nz, result.x, result.y, result.z);
    return result;
}

void TVector::Scale(const double scalar) noexcept
{
    VectorScale3(x, y, z, scalar, x, y, z);
}

TVector TVector::Scaled(const double scalar) const noexcept
{
    TVector result = *this;
    result.Scale(scalar);
    return result;
}

TVector TVector::Sub(const TVector& v) const noexcept
{
    return Sub(v.x, v.y, v.z);
}

TVector TVector::Sub(const double x2, const double y2, const double z2) const noexcept
{
    TVector result{};
    VectorSub3(x, y, z, x2, y2, z2, result.x, result.y, result.z);
    return result;
}

TMatrix43 TMatrix43::identity() noexcept
{
    TMatrix43 result{};
    result.rvec.SetCoords(1.0, 0.0, 0.0);
    result.lvec.SetCoords(0.0, 1.0, 0.0);
    result.uvec.SetCoords(0.0, 0.0, 1.0);
    result.dvec.SetCoords(0.0, 0.0, 0.0);
    return result;
}

TMatrix43 operator*(const TMatrix43& a, const TMatrix43& b) noexcept
{
    TMatrix43 result{};
    result.rvec = a.TransformVector(b.rvec);
    result.lvec = a.TransformVector(b.lvec);
    result.uvec = a.TransformVector(b.dvec);
    result.dvec = a.TransformPoint(b.uvec);
    return result;
}

TVector TMatrix43::ExtractPYR() const noexcept
{
    TVector result{};

    const double sqrLvecX = lvec.x * lvec.x;
    const double dotLvecXY = lvec.y * lvec.y + sqrLvecX;
    double lenLvecXY = std::sqrt(dotLvecXY);

    if (lenLvecXY >= 0.001)
    {
        const double ratio = std::clamp(lvec.y / lenLvecXY, -1.0, 1.0);
        result.yaw = 90.0 - RadToDeg(std::asin(ratio));
        if (lvec.x > 0.0)
        {
            result.yaw = -result.yaw;
        }
    }
    else
    {
        result.yaw = 0.0;
        const double ratio = std::clamp(rvec.x, -1.0, 1.0);
        result.roll = 90.0 - RadToDeg(std::asin(ratio));
        if (((-rvec.y > 0.0) && (lvec.z > 0.0)) || ((-rvec.y < 0.0) && (lvec.z < 0.0)))
        {
            result.roll = -result.roll;
        }
    }

    result.pitch = 90.0;
    if (lenLvecXY >= 0.001)
    {
        const double costh = dotLvecXY / lenLvecXY;
        if (costh < 1.0)
        {
            result.pitch = 90.0 - RadToDeg(std::asin(std::clamp(costh, -1.0, 1.0)));
        }
        else
        {
            result.pitch = 0.0;
        }
    }

    if (lvec.z < 0.0)
    {
        result.pitch = -result.pitch;
    }

    lenLvecXY = std::sqrt((-lvec.y) * (-lvec.y) + sqrLvecX);
    if (lenLvecXY >= 0.001)
    {
        result.roll = 0.0;

        const double costh = ((-rvec.y * lvec.x) + ((-lvec.y) * (-rvec.x))) / lenLvecXY;
        if (costh < 1.0)
        {
            if (costh > -1.0)
            {
                result.roll = 90.0 - RadToDeg(std::asin(std::clamp(costh, -1.0, 1.0)));
            }
            else
            {
                result.roll = 180.0;
            }
        }

        if (-rvec.z < 0.0)
        {
            result.roll = -result.roll;
        }
    }

    return result;
}

void TMatrix43::LookAt(const TVector& eyePos, const TVector& lookPos, const double angle) noexcept
{
    TMatrix43 orient{};
    TVector dest{};

    lvec = lookPos - eyePos;
    lvec.Normalize();

    orient = TMatrix43::CreateFromVectorAngle(lvec, static_cast<float>(angle));

    const double dotProduct = lvec.Dot(TVector::up);

    if (std::fabs(dotProduct) <= 0.999)
    {
        dest = orient.TransformVector(TVector::up);
    }
    else
    {
        if (lvec.z <= 0.0)
        {
            dest = orient.TransformVector(TVector::forward);
        }
        else
        {
            dest = orient.TransformVector(-TVector::forward);
        }
    }

    rvec.x = lvec.y * dest.z - lvec.z * dest.y;
    rvec.y = lvec.z * dest.x - lvec.x * dest.z;
    rvec.z = lvec.x * dest.y - lvec.y * dest.x;
    rvec.Normalize();

    uvec.x = rvec.y * lvec.z - rvec.z * lvec.y;
    uvec.y = lvec.x * rvec.z - lvec.z * rvec.x;
    uvec.z = rvec.x * lvec.y - lvec.x * rvec.y;
    uvec.Normalize();

    dvec = eyePos;
}

void TMatrix43::Normalize() noexcept
{
    uvec.x = rvec.y * lvec.z - rvec.z * lvec.y;
    uvec.y = rvec.z * lvec.x - rvec.x * lvec.z;
    uvec.z = rvec.x * lvec.y - rvec.y * lvec.x;

    lvec.Normalize();
    uvec.Normalize();

    rvec.x = uvec.z * lvec.y - uvec.y * lvec.z;
    rvec.y = lvec.z * uvec.x - uvec.z * lvec.x;
    rvec.z = uvec.y * lvec.x - lvec.y * uvec.x;
}

TVector TMatrix43::TransformPoint(const TVector& point) const noexcept
{
    TVector result{};
    result.x = uvec.x * point.z + lvec.x * point.y + rvec.x * point.x + dvec.x;
    result.y = uvec.y * point.z + lvec.y * point.y + rvec.y * point.x + dvec.y;
    result.z = lvec.z * point.y + uvec.z * point.z + rvec.z * point.x + dvec.z;
    return result;
}

TVector TMatrix43::TransformVector(const TVector& vec) const noexcept
{
    TVector result{};
    result.x = uvec.x * vec.z + lvec.x * vec.y + rvec.x * vec.x;
    result.y = uvec.y * vec.z + lvec.y * vec.y + rvec.y * vec.x;
    result.z = lvec.z * vec.y + uvec.z * vec.z + rvec.z * vec.x;
    return result;
}

TVector TMatrix43::TransformVectorOrtho(const TVector& vec) const noexcept
{
    TVector result{};
    result.x = rvec.z * vec.z + rvec.y * vec.y + vec.x * rvec.x;
    result.y = lvec.y * vec.y + lvec.z * vec.z + lvec.x * vec.x;
    result.z = uvec.y * vec.y + uvec.z * vec.z + uvec.x * vec.x;
    return result;
}

TMatrix43 TMatrix43::InvertOrtho() const noexcept
{
    TMatrix43 result{};

    result.rvec.y = lvec.x;
    result.rvec.z = uvec.x;
    result.lvec.z = uvec.y;

    result.lvec.x = rvec.y;
    result.uvec.x = rvec.z;
    result.uvec.y = lvec.z;

    result.rvec.x = rvec.x;
    result.lvec.y = lvec.y;
    result.uvec.z = uvec.z;

    const double y = dvec.y;
    const double z = dvec.z;
    const double x = dvec.x;

    result.dvec.x = -(rvec.z * z + rvec.y * y + rvec.x * x);
    result.dvec.y = -(lvec.z * z + lvec.x * x + lvec.y * y);
    result.dvec.z = -(uvec.x * x + uvec.y * y + uvec.z * z);

    return result;
}

TMatrix43 TMatrix43::CreateRotate(const TVector& pyr) noexcept
{
    const double sinX = std::sin(DegToRad(pyr.pitch));
    const double cosX = std::cos(DegToRad(pyr.pitch));
    const double sinY = std::sin(DegToRad(pyr.yaw));
    const double cosY = std::cos(DegToRad(pyr.yaw));
    const double sinZ = std::sin(DegToRad(pyr.roll));
    const double cosZ = std::cos(DegToRad(pyr.roll));

    TMatrix43 result{};
    result.rvec.x = sinX * -(sinY * sinZ) + cosZ * cosY;
    result.rvec.y = sinX * sinZ * cosY + sinY * cosZ;
    result.rvec.z = -sinZ * cosX;

    result.lvec.x = -sinY * cosX;
    result.lvec.y = cosY * cosX;
    result.lvec.z = sinX;

    result.uvec.x = cosZ * sinY * sinX + sinZ * cosY;
    result.uvec.y = -sinX * cosZ * cosY + sinZ * sinY;
    result.uvec.z = cosZ * cosX;

    result.dvec = TVector::zero;
    return result;
}

TMatrix43 TMatrix43::Create(const TVector& pos, const TVector& pyr) noexcept
{
    TMatrix43 result = CreateRotate(pyr);
    result.dvec = pos;
    return result;
}

TMatrix43 TMatrix43::CreateFromLook(const TVector& lookDir) noexcept
{
    TMatrix43 result{};

    result.lvec = lookDir;
    result.rvec = result.lvec.Cross(TVector::up);

    if (result.rvec.Normalize() == 0.0)
    {
        result.uvec = result.lvec.Cross(TVector::right);
        result.uvec.Normalize();
        result.rvec = result.uvec.Cross(result.lvec);
    }
    else
    {
        result.uvec = result.lvec.Cross(result.rvec);
    }

    result.dvec = TVector::zero;
    return result;
}

TMatrix43 TMatrix43::CreateFromVectorAngle(const TVector& axis, const float angle) noexcept
{
    // Note: This is a standard Rodrigues' rotation formula implementation.
    // The matrix is stored column-major (rvec/lvec/uvec are basis columns),
    // matching TransformVector/TransformPoint.
    TVector axisNorm = axis;
    if (axisNorm.Normalize() == 0.0)
    {
        return TMatrix43::identity();
    }

    const double theta = DegToRad(static_cast<double>(angle));
    const double angleSin = std::sin(theta);
    const double angleCos = std::cos(theta);
    const double oneMinusCos = 1.0 - angleCos;

    const double x = axisNorm.x;
    const double y = axisNorm.y;
    const double z = axisNorm.z;

    TMatrix43 result{};
    result.rvec = TVector::Create(
        (oneMinusCos * x * x) + angleCos,
        (oneMinusCos * x * y) + (angleSin * z),
        (oneMinusCos * x * z) - (angleSin * y));

    result.lvec = TVector::Create(
        (oneMinusCos * x * y) - (angleSin * z),
        (oneMinusCos * y * y) + angleCos,
        (oneMinusCos * y * z) + (angleSin * x));

    result.uvec = TVector::Create(
        (oneMinusCos * x * z) + (angleSin * y),
        (oneMinusCos * y * z) - (angleSin * x),
        (oneMinusCos * z * z) + angleCos);

    result.dvec = TVector::zero;
    return result;
}

TMatrix43 TMatrix43::CreateScale(const TVector& scale) noexcept
{
    TMatrix43 result{};
    result.rvec = TVector::Create(scale.x, 0.0, 0.0);
    result.lvec = TVector::Create(0.0, scale.y, 0.0);
    result.uvec = TVector::Create(0.0, 0.0, scale.z);
    result.dvec = TVector::zero;
    return result;
}

TMatrix43 TMatrix43::CreateTranslate(const TVector& vec) noexcept
{
    TMatrix43 result = TMatrix43::identity();
    result.dvec = vec;
    return result;
}

TMatrix43 TMatrix43::PreRotate(const TVector& pyr) const noexcept
{
    TMatrix43 result = TMatrix43::CreateRotate(pyr);
    return (*this) * result;
}

TMatrix43 TMatrix43::PostRotate(const TVector& pyr) const noexcept
{
    TMatrix43 result = TMatrix43::CreateRotate(pyr);
    return result * (*this);
}

TMatrix43 TMatrix43::PreTranslate(const TVector& vec) const noexcept
{
    TMatrix43 result = TMatrix43::CreateTranslate(vec);
    return (*this) * result;
}

TMatrix43 TMatrix43::PostTranslate(const TVector& vec) const noexcept
{
    TMatrix43 result = TMatrix43::CreateTranslate(vec);
    return result * (*this);
}

TMatrix43 TMatrix43::PreScale(const TVector& vec) const noexcept
{
    TMatrix43 result = TMatrix43::CreateScale(vec);
    return (*this) * result;
}

TMatrix43 TMatrix43::PostScale(const TVector& vec) const noexcept
{
    TMatrix43 result = TMatrix43::CreateScale(vec);
    return result * (*this);
}

void calcRotVecData(VecRotData& rd, const double pch, const double yaw, const double rol) noexcept
{
    const TVector pyr{ pch, yaw, rol };
    const TMatrix43 rot = TMatrix43::CreateRotate(pyr);
    rd.rvec = rot.rvec;
    rd.lvec = rot.lvec;
    rd.uvec = rot.uvec;
}

void rotVecByData(TVector& vec, const VecRotData& rd) noexcept
{
    const double x = rd.uvec.x * vec.z + rd.lvec.x * vec.y + rd.rvec.x * vec.x;
    const double y = rd.uvec.y * vec.z + rd.lvec.y * vec.y + rd.rvec.y * vec.x;
    const double z = rd.lvec.z * vec.y + rd.uvec.z * vec.z + rd.rvec.z * vec.x;

    vec.x = x;
    vec.y = y;
    vec.z = z;
}

void rotateVector(TVector& vec, const double pch, const double yaw, const double rol) noexcept
{
    VecRotData rd{};
    calcRotVecData(rd, pch, yaw, rol);
    rotVecByData(vec, rd);
}

void getJKPYR(const TVector& x, const TVector& y, const TVector& z, double& pch, double& yaw, double& rol) noexcept
{
    (void)x;

    const double l = std::sqrt(sqr(y.y) + sqr(y.x));

    if (l == 0.0)
    {
        yaw = 0.0;
    }
    else
    {
        yaw = RadToDeg(std::acos(std::clamp(y.y / l, -1.0, 1.0)));
    }

    if (y.x > 0.0)
    {
        yaw = 360.0 - yaw;
    }

    TVector ny = y;
    rotateVector(ny, 0.0, -yaw, 0.0);

    pch = RadToDeg(std::acos(std::clamp(ny.y, -1.0, 1.0)));
    if (ny.z < 0.0)
    {
        pch = 360.0 - pch;
    }

    TVector nz = z;
    rotateVector(nz, 0.0, -yaw, 0.0);
    rotateVector(nz, -pch, 0.0, 0.0);

    rol = RadToDeg(std::acos(std::clamp(nz.z, -1.0, 1.0)));
    if (nz.x < 0.0)
    {
        rol = 360.0 - rol;
    }
}

void rotatePoint(
    const double ax1,
    const double ay1,
    const double az1,
    const double ax2,
    const double ay2,
    const double az2,
    const double angleDeg,
    double& x,
    double& y,
    double& z) noexcept
{
    TVector nz{};
    nz.x = ax2 - ax1;
    nz.y = ay2 - ay1;
    nz.z = az2 - az1;
    if (nz.Normalize() == 0.0)
    {
        return;
    }

    const double t = nz.Dot(x - ax1, y - ay1, z - az1);
    const double px = ax1 + nz.x * t;
    const double py = ay1 + nz.y * t;
    const double pz = az1 + nz.z * t;

    TVector nx{};
    nx.x = x - px;
    nx.y = y - py;
    nx.z = z - pz;
    const double l = nx.Normalize();
    if (std::abs(l) <= CloseEnough)
    {
        return;
    }

    const TVector ny = nz.Cross(nx);

    const double u = std::cos(DegToRad(angleDeg)) * l;
    const double v = std::sin(DegToRad(angleDeg)) * l;

    x = px + nx.x * u + ny.x * v;
    y = py + nx.y * u + ny.y * v;
    z = pz + nx.z * u + ny.z * v;
}

void prepareRotateData(
    RotateData& rd,
    const TVector& axis,
    const double cx,
    const double cy,
    const double cz,
    const double angleDeg) noexcept
{
    rd.center.SetCoords(cx, cy, cz);

    TVector axisDir = axis;
    if (axisDir.Normalize() == 0.0)
    {
        rd.m = TMatrix43::identity();
        return;
    }

    TVector vx{ 1.0, 0.0, 0.0 };
    TVector vy{ 0.0, 1.0, 0.0 };
    TVector vz{ 0.0, 0.0, 1.0 };

    rotatePoint(0.0, 0.0, 0.0, axisDir.x, axisDir.y, axisDir.z, angleDeg, vx.x, vx.y, vx.z);
    rotatePoint(0.0, 0.0, 0.0, axisDir.x, axisDir.y, axisDir.z, angleDeg, vy.x, vy.y, vy.z);
    rotatePoint(0.0, 0.0, 0.0, axisDir.x, axisDir.y, axisDir.z, angleDeg, vz.x, vz.y, vz.z);

    rd.m.rvec.SetCoords(vx.x, vy.x, vz.x);
    rd.m.lvec.SetCoords(vx.y, vy.y, vz.y);
    rd.m.uvec.SetCoords(vx.z, vy.z, vz.z);
    rd.m.dvec = TVector::zero;
}

void rotateXYZ(double& x, double& y, double& z, const RotateData& rd) noexcept
{
    TVector v{};
    v.SetCoords(x - rd.center.x, y - rd.center.y, z - rd.center.z);
    v = rd.m.TransformVector(v);
    x = v.x + rd.center.x;
    y = v.y + rd.center.y;
    z = v.z + rd.center.z;
}

void scaleXYZ(double& x, double& y, double& z, const ScaleData& sd) noexcept
{
    switch (sd.how)
    {
    case ScaleHow::xyz:
        x = sd.cx + sd.sfactor * (x - sd.cx);
        y = sd.cy + sd.sfactor * (y - sd.cy);
        z = sd.cz + sd.sfactor * (z - sd.cz);
        return;
    case ScaleHow::x:
        x = sd.cx + sd.sfactor * (x - sd.cx);
        return;
    case ScaleHow::y:
        y = sd.cy + sd.sfactor * (y - sd.cy);
        return;
    case ScaleHow::z:
        z = sd.cz + sd.sfactor * (z - sd.cz);
        return;
    case ScaleHow::vec:
    default:
        break;
    }

    TVector axis = sd.vec;
    if ( axis.Normalize() == 0.0 )
    {
        return;
    }

    const double distanceAlongAxis = axis.Dot(x - sd.cx, y - sd.cy, z - sd.cz);
    const double scaledDistance = (sd.sfactor - 1.0) * distanceAlongAxis;
    x = x + scaledDistance * axis.x;
    y = y + scaledDistance * axis.y;
    z = z + scaledDistance * axis.z;
}

void flipXYZ(double& x, double& y, double& z, const TVector& planeNormal, const double cx, const double cy, const double cz) noexcept
{
    TVector normal = planeNormal;
    if ( !std::isfinite(normal.x) || !std::isfinite(normal.y) ||
         !std::isfinite(normal.z) || normal.Normalize() == 0.0 )
    {
        return;
    }

    const double d = normal.Dot(x - cx, y - cy, z - cz);
    x = x - normal.x * 2.0 * d;
    y = y - normal.y * 2.0 * d;
    z = z - normal.z * 2.0 * d;
}

bool CheckSphereFaceHit(
    const TVector& startPos,
    const TVector& moveNorm,
    const double moveDistance,
    const double radius,
    const TVector& normal,
    const TVector& point,
    double& sphereHitDist,
    const std::int32_t flags) noexcept
{
    double distToPlane = (startPos - point).Dot(normal);
    if (std::abs(distToPlane) <= 0.00001)
    {
        distToPlane = 0.0;
    }

    if (distToPlane < 0.0)
    {
        return false;
    }

    const double distToSphere = distToPlane - radius;
    if (distToSphere > moveDistance)
    {
        return false;
    }

    const double dot = -moveNorm.Dot(normal);

    if (distToSphere < 0.0)
    {
        if ((flags & 0x400) != 0)
        {
            sphereHitDist = sphereHitDist + radius;
        }
        else
        {
            sphereHitDist = 0.0;
        }
        return true;
    }

    if (dot > 0.0)
    {
        if (dot * moveDistance >= distToSphere)
        {
            sphereHitDist = distToSphere / dot;
            if (sphereHitDist < 0.0)
            {
                sphereHitDist = 0.0;
            }
            return true;
        }
    }

    return false;
}

} // namespace sed
