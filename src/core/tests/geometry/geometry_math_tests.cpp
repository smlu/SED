#include "test_common.hpp"

#include <limits>
#include <numbers>

namespace sed::tests
{

void runGeometryMathTests()
{
    {
        check(sed::math::kRotationEpsilon > sed::math::kGeometryEpsilon, "math.kRotationEpsilon larger than geometry");
        check(sed::math::equal(0.0, sed::math::kRotationEpsilon, kRotationEpsilon),
            "math.equal rotation epsilon true");
        check(!sed::math::equal(0.0, sed::math::kRotationEpsilon * 2.0, kRotationEpsilon),
            "math.equal rotation epsilon false");
    }

    {
        check(sed::math::isZero(sed::math::kTinyEpsilon), "math.isZero default double boundary true");
        check(!sed::math::isZero(sed::math::kTinyEpsilon * 2.0), "math.isZero default double beyond false");
        check(sed::math::isZero(sed::math::kTinyEpsilonF), "math.isZero default float boundary true");
        check(!sed::math::isZero(sed::math::kTinyEpsilonF * 2.0f), "math.isZero default float beyond false");

        check(sed::math::equal(0.0, sed::math::kTinyEpsilon), "math.equal default double boundary true");
        check(!sed::math::equal(0.0, sed::math::kTinyEpsilon * 2.0), "math.equal default double beyond false");
        check(sed::math::equal(0.0f, sed::math::kTinyEpsilonF), "math.equal default float boundary true");
        check(!sed::math::equal(0.0f, sed::math::kTinyEpsilonF * 2.0f),
            "math.equal default float beyond false");

        check(sed::math::isZero(sed::math::kGeometryEpsilon, sed::math::kGeometryEpsilon),
            "math.isZero explicit double boundary true");
        check(!sed::math::isZero(sed::math::kGeometryEpsilon * 2.0, sed::math::kGeometryEpsilon),
            "math.isZero explicit double beyond false");
        check(sed::math::equal(0.0, sed::math::kGeometryEpsilon, sed::math::kGeometryEpsilon),
            "math.equal explicit double boundary true");
        check(!sed::math::equal(0.0, sed::math::kGeometryEpsilon * 2.0, sed::math::kGeometryEpsilon),
            "math.equal explicit double beyond false");

    }

    {
        check(close(sed::math::degToRad(180.0), std::numbers::pi_v<double>, kExactEpsilon), "math.degToRad.180");
        check(close(sed::math::radToDeg(std::numbers::pi_v<double>), 180.0, kExactEpsilon), "math.radToDeg.pi");
        check(close(sed::DegToRad(180.0), std::numbers::pi_v<double>, kExactEpsilon), "DegToRad.180");
        check(close(sed::RadToDeg(std::numbers::pi_v<double>), 180.0, kExactEpsilon), "RadToDeg.pi");
        check(sed::SameValue(1.0, 1.0 + sed::CloseEnough, sed::CloseEnough), "SameValue boundary true");
        check(!sed::SameValue(1.0, 1.0 + sed::CloseEnough * 2.0, sed::CloseEnough), "SameValue beyond false");
        check(sed::IsZero(std::numeric_limits<double>::epsilon()), "IsZero boundary true");
        check(!sed::IsZero(std::numeric_limits<double>::epsilon() * 2.0), "IsZero beyond false");
    }

    {
        const sed::TVector2 a{ 3.0, 4.0 };
        check(close(a.Length(), 5.0), "TVector2.Length");

        const sed::TVector2 b = a.Normalized();
        check(close(b.x, 0.6), "TVector2.Normalized.x");
        check(close(b.y, 0.8), "TVector2.Normalized.y");
    }

    {
        const sed::TVector a{ 1.0, 0.0, 0.0 };
        const sed::TVector b{ 0.0, 1.0, 0.0 };
        const sed::TVector c = a.Cross(b);
        check(close(c.x, 0.0), "TVector.Cross.x");
        check(close(c.y, 0.0), "TVector.Cross.y");
        check(close(c.z, 1.0), "TVector.Cross.z");
    }

    {
        const sed::TMatrix43 id = sed::TMatrix43::identity();
        const sed::TVector p{ 1.0, 2.0, 3.0 };
        const sed::TVector tp = id.TransformPoint(p);
        check(tp == p, "TMatrix43.identity.TransformPoint");
    }

    {
        const sed::TMatrix43 rot = sed::TMatrix43::CreateRotate(sed::TVector{ 10.0, 20.0, 30.0 });
        const sed::TMatrix43 id = sed::TMatrix43::identity();
        const sed::TMatrix43 combined = id * rot;

        check(combined.rvec == rot.rvec, "TMatrix43.Multiply.rvec");
        check(combined.lvec == rot.lvec, "TMatrix43.Multiply.lvec");
        check(combined.uvec == sed::TVector::zero, "TMatrix43.Multiply.uvec.compat");
        check(combined.dvec == rot.uvec, "TMatrix43.Multiply.dvec.compat");
    }

    {
        const sed::TMatrix43 t = sed::TMatrix43::CreateTranslate(sed::TVector{ 1.0, 2.0, 3.0 });
        const sed::TVector p{ 4.0, 5.0, 6.0 };
        const sed::TVector tp = t.TransformPoint(p);
        check(closeVec(tp, sed::TVector{ 5.0, 7.0, 9.0 }), "TMatrix43.CreateTranslate.TransformPoint");

        const sed::TMatrix43 inv = t.InvertOrtho();
        const sed::TVector roundtrip = inv.TransformPoint(tp);
        check(closeVec(roundtrip, p), "TMatrix43.InvertOrtho.Translate.Roundtrip");
    }

    {
        sed::TMatrix43 look{};
        look.LookAt(sed::TVector::zero, sed::TVector::forward, 0.0);
        check(closeVec(look.rvec, sed::TVector::right, kRotationEpsilon), "TMatrix43.LookAt.rvec");
        check(closeVec(look.lvec, sed::TVector::forward, kRotationEpsilon), "TMatrix43.LookAt.lvec");
        check(closeVec(look.uvec, sed::TVector::up, kRotationEpsilon), "TMatrix43.LookAt.uvec");
        check(closeVec(look.dvec, sed::TVector::zero), "TMatrix43.LookAt.dvec");
    }

    {
        const sed::TVector pyr{ 10.0, 20.0, 30.0 };
        const sed::TMatrix43 m = sed::TMatrix43::CreateRotate(pyr);
        const sed::TVector extracted = m.ExtractPYR();
        const sed::TMatrix43 m2 = sed::TMatrix43::CreateRotate(extracted);

        check(closeVec(m.rvec, m2.rvec, kRotationEpsilon), "TMatrix43.ExtractPYR.Roundtrip.rvec");
        check(closeVec(m.lvec, m2.lvec, kRotationEpsilon), "TMatrix43.ExtractPYR.Roundtrip.lvec");
        check(closeVec(m.uvec, m2.uvec, kRotationEpsilon), "TMatrix43.ExtractPYR.Roundtrip.uvec");
    }

    {
        const sed::TVector startPos{ 0.0, 0.0, 10.0 };
        const sed::TVector moveNorm{ 0.0, 0.0, -1.0 };
        const double moveDistance = 100.0;
        const double radius = 1.0;
        const sed::TVector normal{ 0.0, 0.0, 1.0 };
        const sed::TVector point{ 0.0, 0.0, 0.0 };

        double sphereHitDist = 0.0;
        const bool hit =
            sed::CheckSphereFaceHit(startPos, moveNorm, moveDistance, radius, normal, point, sphereHitDist, 0);
        check(hit, "CheckSphereFaceHit.hit");
        check(sphereHitDist >= 0.0, "CheckSphereFaceHit.distance");
    }

    {
        sed::VecRotData rd{};
        sed::calcRotVecData(rd, 10.0, 20.0, 30.0);

        sed::TVector a{ 1.0, 2.0, 3.0 };
        sed::rotVecByData(a, rd);

        sed::TVector b{ 1.0, 2.0, 3.0 };
        sed::rotateVector(b, 10.0, 20.0, 30.0);

        check(closeVec(a, b, kGeometryEpsilon), "rotateVector matches rotVecByData");
    }

    {
        const sed::TVector pyr{ 10.0, 20.0, 30.0 };
        const sed::TMatrix43 m = sed::TMatrix43::CreateRotate(pyr);

        sed::TVector xAxis = sed::TVector::right;
        sed::TVector yAxis = sed::TVector::forward;
        sed::TVector zAxis = sed::TVector::up;

        sed::rotateVector(xAxis, pyr.pitch, pyr.yaw, pyr.roll);
        sed::rotateVector(yAxis, pyr.pitch, pyr.yaw, pyr.roll);
        sed::rotateVector(zAxis, pyr.pitch, pyr.yaw, pyr.roll);

        check(closeVec(xAxis, m.rvec, kGeometryEpsilon), "rotateVector xAxis == matrix rvec");
        check(closeVec(yAxis, m.lvec, kGeometryEpsilon), "rotateVector yAxis == matrix lvec");
        check(closeVec(zAxis, m.uvec, kGeometryEpsilon), "rotateVector zAxis == matrix uvec");
    }

    {
        const double inPitch = 10.0;
        const double inYaw = 20.0;
        const double inRoll = 30.0;

        sed::TVector xAxis = sed::TVector::right;
        sed::TVector yAxis = sed::TVector::forward;
        sed::TVector zAxis = sed::TVector::up;

        sed::rotateVector(xAxis, inPitch, inYaw, inRoll);
        sed::rotateVector(yAxis, inPitch, inYaw, inRoll);
        sed::rotateVector(zAxis, inPitch, inYaw, inRoll);

        double outPitch = 0.0;
        double outYaw = 0.0;
        double outRoll = 0.0;
        sed::getJKPYR(xAxis, yAxis, zAxis, outPitch, outYaw, outRoll);

        check(close(outPitch, inPitch, kRotationEpsilon), "getJKPYR pitch");
        check(close(outYaw, inYaw, kRotationEpsilon), "getJKPYR yaw");
        check(close(outRoll, inRoll, kRotationEpsilon), "getJKPYR roll");
    }

    {
        sed::RotateData rd{};
        sed::prepareRotateData(rd, sed::TVector::up, 0.0, 0.0, 0.0, 90.0);

        double x = 1.0;
        double y = 0.0;
        double z = 0.0;
        sed::rotateXYZ(x, y, z, rd);

        check(close(x, 0.0, kRotationEpsilon), "rotateXYZ.zAxis.x");
        check(close(y, -1.0, kRotationEpsilon), "rotateXYZ.zAxis.y");
        check(close(z, 0.0, kRotationEpsilon), "rotateXYZ.zAxis.z");
    }

    {
        sed::RotateData rd{};
        sed::prepareRotateData(rd, sed::TVector::up, 10.0, 0.0, 0.0, 180.0);

        double x = 11.0;
        double y = 0.0;
        double z = 0.0;
        sed::rotateXYZ(x, y, z, rd);

        check(close(x, 9.0, kRotationEpsilon), "rotateXYZ.centered.x");
        check(close(y, 0.0, kRotationEpsilon), "rotateXYZ.centered.y");
        check(close(z, 0.0, kRotationEpsilon), "rotateXYZ.centered.z");
    }

    {
        sed::RotateData rd{};
        sed::prepareRotateData(rd, sed::TVector::zero, 0.0, 0.0, 0.0, 90.0);

        double x = 1.0;
        double y = 2.0;
        double z = 3.0;
        sed::rotateXYZ(x, y, z, rd);

        check(close(x, 1.0, kExactEpsilon), "rotateXYZ.zeroAxis.x");
        check(close(y, 2.0, kExactEpsilon), "rotateXYZ.zeroAxis.y");
        check(close(z, 3.0, kExactEpsilon), "rotateXYZ.zeroAxis.z");
    }

    {
        double x = 1.0;
        double y = 0.0;
        double z = 0.0;
        sed::rotatePoint(0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 90.0, x, y, z);

        check(close(x, 0.0, kRotationEpsilon), "rotatePoint.zAxis.x");
        check(close(y, 1.0, kRotationEpsilon), "rotatePoint.zAxis.y");
        check(close(z, 0.0, kRotationEpsilon), "rotatePoint.zAxis.z");
    }

    {
        const sed::TVector axis{ 0.0, 0.0, 1.0 };
        const sed::TVector center{ 10.0, 0.0, 0.0 };
        const double angle = 25.0;

        double px1 = 11.0;
        double py1 = 2.0;
        double pz1 = 3.0;

        double px2 = px1;
        double py2 = py1;
        double pz2 = pz1;

        sed::RotateData rd{};
        sed::prepareRotateData(rd, axis, center.x, center.y, center.z, angle);
        sed::rotateXYZ(px1, py1, pz1, rd);

        sed::rotatePoint(center.x, center.y, center.z, center.x + axis.x, center.y + axis.y, center.z + axis.z, -angle, px2, py2, pz2);

        check(close(px1, px2, kRotationEpsilon), "rotateXYZ == rotatePoint(-angle).x");
        check(close(py1, py2, kRotationEpsilon), "rotateXYZ == rotatePoint(-angle).y");
        check(close(pz1, pz2, kRotationEpsilon), "rotateXYZ == rotatePoint(-angle).z");
    }

    {
        sed::ScaleData sd{};
        sd.cx = 10.0;
        sd.cy = 20.0;
        sd.cz = 30.0;
        sd.sfactor = 2.0;
        sd.how = sed::ScaleHow::xyz;

        double x = 11.0;
        double y = 22.0;
        double z = 33.0;
        sed::scaleXYZ(x, y, z, sd);

        check(close(x, 12.0, kExactEpsilon), "scaleXYZ.xyz.x");
        check(close(y, 24.0, kExactEpsilon), "scaleXYZ.xyz.y");
        check(close(z, 36.0, kExactEpsilon), "scaleXYZ.xyz.z");
    }

    {
        sed::ScaleData sd{};
        sd.cx = 10.0;
        sd.cy = 20.0;
        sd.cz = 30.0;
        sd.sfactor = 2.0;
        sd.how = sed::ScaleHow::x;

        double x = 11.0;
        double y = 22.0;
        double z = 33.0;
        sed::scaleXYZ(x, y, z, sd);

        check(close(x, 12.0, kExactEpsilon), "scaleXYZ.x.x");
        check(close(y, 22.0, kExactEpsilon), "scaleXYZ.x.y");
        check(close(z, 33.0, kExactEpsilon), "scaleXYZ.x.z");
    }

    {
        sed::ScaleData sd{};
        sd.cx = 0.0;
        sd.cy = 0.0;
        sd.cz = 0.0;
        sd.sfactor = 4.0;
        sd.vec = sed::TVector{ 1.0, 0.0, 0.0 };
        sd.how = sed::ScaleHow::vec;

        double x = 2.0;
        double y = 2.0;
        double z = 2.0;
        sed::scaleXYZ(x, y, z, sd);

        check(close(x, 8.0, kExactEpsilon), "scaleXYZ.vec.x");
        check(close(y, 2.0, kExactEpsilon), "scaleXYZ.vec.y");
        check(close(z, 2.0, kExactEpsilon), "scaleXYZ.vec.z");
    }

    {
        sed::ScaleData sd{};
        sd.sfactor = 2.0;
        sd.vec = sed::TVector{ 8.0, 8.0, 0.0 };
        sd.how = sed::ScaleHow::vec;

        double x = 3.0;
        double y = 1.0;
        double z = 5.0;
        sed::scaleXYZ(x, y, z, sd);

        check(close(x, 5.0, kExactEpsilon), "scaleXYZ.vec preserves and scales oblique projections: x");
        check(close(y, 3.0, kExactEpsilon), "scaleXYZ.vec preserves and scales oblique projections: y");
        check(close(z, 5.0, kExactEpsilon), "scaleXYZ.vec preserves and scales oblique projections: z");
    }

    {
        sed::ScaleData unitScale{};
        unitScale.sfactor = 4.0;
        unitScale.vec = sed::TVector::right;
        unitScale.how = sed::ScaleHow::vec;

        double unitX = 2.0;
        double unitY = 2.0;
        double unitZ = 2.0;
        sed::scaleXYZ(unitX, unitY, unitZ, unitScale);

        sed::ScaleData scaledNormal = unitScale;
        scaledNormal.vec = sed::TVector{ 7.0, 0.0, 0.0 };
        double scaledX = 2.0;
        double scaledY = 2.0;
        double scaledZ = 2.0;
        sed::scaleXYZ(scaledX, scaledY, scaledZ, scaledNormal);

        check(close(scaledX, unitX, kExactEpsilon)
                && close(scaledY, unitY, kExactEpsilon)
                && close(scaledZ, unitZ, kExactEpsilon),
            "scaleXYZ vector-axis result is independent of finite axis magnitude");
    }

    {
        double x = 1.0;
        double y = 2.0;
        double z = 3.0;
        sed::flipXYZ(x, y, z, sed::TVector::right, 0.0, 0.0, 0.0);

        check(close(x, -1.0, kExactEpsilon), "flipXYZ.planeX.x");
        check(close(y, 2.0, kExactEpsilon), "flipXYZ.planeX.y");
        check(close(z, 3.0, kExactEpsilon), "flipXYZ.planeX.z");
    }


    {
        double unitX = 3.0;
        double unitY = -4.0;
        double unitZ = 5.0;
        sed::flipXYZ(
            unitX,
            unitY,
            unitZ,
            sed::TVector::right,
            1.0,
            2.0,
            3.0);

        double scaledX = 3.0;
        double scaledY = -4.0;
        double scaledZ = 5.0;
        sed::flipXYZ(
            scaledX,
            scaledY,
            scaledZ,
            sed::TVector{ 7.0, 0.0, 0.0 },
            1.0,
            2.0,
            3.0);

        check(close(scaledX, unitX, kExactEpsilon)
                && close(scaledY, unitY, kExactEpsilon)
                && close(scaledZ, unitZ, kExactEpsilon),
            "flipXYZ arbitrary-plane reflection is independent of finite normal magnitude");
    }
}

} // namespace sed::tests
