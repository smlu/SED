#pragma once

QCursor makeGeneratedEyeCursor()
{
    constexpr int kCursorSize = 32;
    constexpr int kHotSpot = kCursorSize / 2;

    QPixmap pixmap(kCursorSize, kCursorSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen outline(QColor(245, 248, 252, 235));
    outline.setWidthF(2.2);
    outline.setCapStyle(Qt::RoundCap);
    outline.setJoinStyle(Qt::RoundJoin);
    painter.setPen(outline);
    painter.setBrush(QColor(17, 23, 30, 175));

    QPainterPath eye;
    eye.moveTo(4.5, 16.0);
    eye.cubicTo(8.0, 9.0, 13.0, 6.5, 16.0, 6.5);
    eye.cubicTo(19.0, 6.5, 24.0, 9.0, 27.5, 16.0);
    eye.cubicTo(24.0, 23.0, 19.0, 25.5, 16.0, 25.5);
    eye.cubicTo(13.0, 25.5, 8.0, 23.0, 4.5, 16.0);
    eye.closeSubpath();
    painter.drawPath(eye);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(84, 156, 255, 245));
    painter.drawEllipse(QPointF(16.0, 16.0), 5.2, 5.2);
    painter.setBrush(QColor(236, 244, 255, 250));
    painter.drawEllipse(QPointF(14.2, 14.0), 1.4, 1.4);

    return QCursor(pixmap, kHotSpot, kHotSpot);
}

QCursor loadEyeCursor()
{
    // Shipped editor artwork must not depend on the launch directory or a
    // separately deployed res folder. The CUR file is compiled into the Qt
    // resource collection alongside the application icons.
    const QPixmap pixmap(QStringLiteral(":/sed/cursors/eye.cur"));
    if (!pixmap.isNull())
    {
        const int hotX = std::max(0, pixmap.width() / 2);
        const int hotY = std::max(0, pixmap.height() / 2);
        return QCursor(pixmap, hotX, hotY);
    }

    // Retain a code-generated safety fallback for platforms whose Qt build
    // cannot decode Windows CUR images. It never reads from the filesystem.
    return makeGeneratedEyeCursor();
}

template <typename EmitTri>
void triangulatePolyZigZag(const int numVerts, EmitTri emitTri)
{
    if (numVerts < 3)
    {
        return;
    }

    if (numVerts == 3)
    {
        emitTri(0, 1, 2);
        return;
    }

    const int totalTris = numVerts - 2;
    int ofsTriVert0 = 0;
    int ofsTriVert1 = 1;
    int ofsTriVert2 = numVerts - 1;

    for (int idx = 0; idx < totalTris; ++idx)
    {
        emitTri(ofsTriVert0, ofsTriVert1, ofsTriVert2);
        if ((idx & 1) == 0)
        {
            ofsTriVert0 = ofsTriVert1;
            ++ofsTriVert1;
        }
        else
        {
            ofsTriVert0 = ofsTriVert2;
            --ofsTriVert2;
        }
    }
}

float normalizeRollDeg(float deg) noexcept
{
    deg = std::fmod(deg, 360.0f);
    if (deg < 0.0f)
    {
        deg += 360.0f;
    }
    return deg;
}

struct SpriteBasis
{
    QVector3D right{ 1.0f, 0.0f, 0.0f };
    QVector3D forward{ 0.0f, 1.0f, 0.0f };
    QVector3D up{ 0.0f, 0.0f, 1.0f };
};

bool normalizeInPlace(QVector3D& v) noexcept
{
    if (v.lengthSquared() < sed::math::kTinyEpsilonF)
    {
        return false;
    }
    v.normalize();
    return true;
}

bool normalizeInPlace(TVector& value) noexcept
{
    return render::cleaveNormalize(value);
}

QVector3D rotateAroundAxis(const QVector3D& v, const QVector3D& axisUnit, const float angleRad) noexcept
{
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    return v * c + QVector3D::crossProduct(axisUnit, v) * s + axisUnit * (QVector3D::dotProduct(axisUnit, v) * (1.0f - c));
}

SpriteBasis makeSpriteBasis(const TVector& placementPyr) noexcept
{
    const TMatrix43 placementRot = TMatrix43::CreateRotate(placementPyr);
    const TVector& r = placementRot.rvec;
    const TVector& f = placementRot.lvec;
    const TVector& u = placementRot.uvec;

    SpriteBasis basis{};
    basis.right = QVector3D(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.z));
    basis.forward = QVector3D(static_cast<float>(f.x), static_cast<float>(f.y), static_cast<float>(f.z));
    if (!normalizeInPlace(basis.right))
    {
        basis.right = QVector3D(1.0f, 0.0f, 0.0f);
    }
    if (!normalizeInPlace(basis.forward))
    {
        basis.forward = QVector3D(0.0f, 1.0f, 0.0f);
    }

    basis.up = QVector3D(
        static_cast<float>(u.x),
        static_cast<float>(u.y),
        static_cast<float>(u.z));
    if (!normalizeInPlace(basis.up))
    {
        basis.up = QVector3D::crossProduct(basis.right, basis.forward);
        if (!normalizeInPlace(basis.up))
        {
            basis.up = QVector3D(0.0f, 0.0f, 1.0f);
        }
    }
    return basis;
}

QVector3D makeSpriteType2Forward(const TVector& placementPyr) noexcept
{
    TMatrix43 rot = TMatrix43::CreateRotate(placementPyr);
    QVector3D forward(static_cast<float>(rot.lvec.x), static_cast<float>(rot.lvec.y), static_cast<float>(rot.lvec.z));
    if (!normalizeInPlace(forward))
    {
        forward = QVector3D(0.0f, 1.0f, 0.0f);
    }
    return forward;
}

QPoint mouseEventPos(const QMouseEvent& event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event.position().toPoint();
#else
    return event.pos();
#endif
}

QMatrix4x4 matrix43ToQt(const TMatrix43& m) noexcept
{
    QMatrix4x4 out;
    out.setColumn(0, QVector4D(static_cast<float>(m.rvec.x), static_cast<float>(m.rvec.y), static_cast<float>(m.rvec.z), 0.0f));
    out.setColumn(1, QVector4D(static_cast<float>(m.lvec.x), static_cast<float>(m.lvec.y), static_cast<float>(m.lvec.z), 0.0f));
    out.setColumn(2, QVector4D(static_cast<float>(m.uvec.x), static_cast<float>(m.uvec.y), static_cast<float>(m.uvec.z), 0.0f));
    out.setColumn(3, QVector4D(static_cast<float>(m.dvec.x), static_cast<float>(m.dvec.y), static_cast<float>(m.dvec.z), 1.0f));
    return out;
}

struct SegmentClosestPoints
{
    double s{};
    double t{};
    TVector p{};
    TVector q{};
    double distSqr{};
};

SegmentClosestPoints segmentCalcClosestPoints(
    const TVector& p0,
    const TVector& p1,
    const TVector& q0,
    const TVector& q1) noexcept
{
    constexpr double kEps = sed::math::kTinyEpsilon;

    const TVector u = p1 - p0;
    const TVector v = q1 - q0;
    const TVector w = p0 - q0;

    const double a = u.Dot(u);
    const double b = u.Dot(v);
    const double c = v.Dot(v);
    const double d = u.Dot(w);
    const double e = v.Dot(w);

    double sc{};
    double tc{};

    if (c <= kEps)
    {
        sc = (a > kEps) ? std::clamp(-d / a, 0.0, 1.0) : 0.0;
        tc = 0.0;
    }
    else
    {
        const double D = a * c - b * b;

        double sN{};
        double sD = D;
        double tN{};
        double tD = D;

        if (D <= kEps)
        {
            sN = 0.0;
            sD = 1.0;
            tN = e;
            tD = c;
        }
        else
        {
            sN = (b * e - c * d);
            tN = (a * e - b * d);

            if (sN < 0.0)
            {
                sN = 0.0;
                tN = e;
                tD = c;
            }
            else if (sN > sD)
            {
                sN = sD;
                tN = e + b;
                tD = c;
            }
        }

        if (tN < 0.0)
        {
            tN = 0.0;

            if (-d < 0.0)
            {
                sN = 0.0;
            }
            else if (-d > a)
            {
                sN = sD;
            }
            else
            {
                sN = -d;
                sD = a;
            }
        }
        else if (tN > tD)
        {
            tN = tD;

            if ((-d + b) < 0.0)
            {
                sN = 0.0;
            }
            else if ((-d + b) > a)
            {
                sN = sD;
            }
            else
            {
                sN = (-d + b);
                sD = a;
            }
        }

        sc = (std::abs(sN) <= kEps) ? 0.0 : (sN / sD);
        tc = (std::abs(tN) <= kEps) ? 0.0 : (tN / tD);
    }

    SegmentClosestPoints result{};
    result.s = sc;
    result.t = tc;
    result.p = p0 + u * sc;
    result.q = q0 + v * tc;
    const TVector diff = result.p - result.q;
    result.distSqr = diff.Dot(diff);
    return result;
}

struct ProjectedPoint
{
    QPointF pos{};
    float ndcZ{};
    float clipW{};
    bool valid{};
};

QVector3D affineVec4(const QVector4D& v) noexcept
{
    if (qFuzzyIsNull(v.w()))
    {
        return QVector3D(0.0f, 0.0f, 0.0f);
    }
    return QVector3D(v.x() / v.w(), v.y() / v.w(), v.z() / v.w());
}

ProjectedPoint projectWorldToWidget(const QMatrix4x4& mvp, const QVector3D& world, const int w, const int h) noexcept
{
    ProjectedPoint out{};
    if (w <= 0 || h <= 0)
    {
        return out;
    }

    const QVector4D clip = mvp * QVector4D(world, 1.0f);
    out.clipW = clip.w();
    if (qFuzzyIsNull(clip.w()))
    {
        return out;
    }

    const QVector3D ndc(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());
    out.pos = QPointF(
        (ndc.x() * 0.5f + 0.5f) * static_cast<float>(w),
        (1.0f - (ndc.y() * 0.5f + 0.5f)) * static_cast<float>(h));
    out.ndcZ = ndc.z();
    out.valid = std::isfinite(out.pos.x()) && std::isfinite(out.pos.y()) && std::isfinite(out.ndcZ);
    return out;
}

ProjectedPoint projectWorldToWidget(const QMatrix4x4& mvp, const TVector& world, const int w, const int h) noexcept
{
    ProjectedPoint out{};
    if (w <= 0 || h <= 0)
    {
        return out;
    }
    const auto clip = render::frame_path::detail::worldToClip(mvp, world);
    if (!render::frame_path::finiteClipPoint(clip) || clip.w == 0.0)
    {
        return out;
    }
    out.clipW = static_cast<float>(clip.w);
    out.ndcZ = static_cast<float>(clip.z / clip.w);
    out.pos = QPointF((clip.x / clip.w * 0.5 + 0.5) * w,
        (0.5 - clip.y / clip.w * 0.5) * h);
    out.valid = std::isfinite(out.pos.x()) && std::isfinite(out.pos.y()) && std::isfinite(out.ndcZ);
    return out;
}

QVector3D unprojectNdc(const QMatrix4x4& invMvp, const float ndcX, const float ndcY, const float ndcZ) noexcept
{
    return affineVec4(invMvp * QVector4D(ndcX, ndcY, ndcZ, 1.0f));
}

float worldUnitsPerPixelAt(const QVector3D& world, const QMatrix4x4& mvp, const QMatrix4x4& invMvp, const int w, const int h) noexcept
{
    if (w <= 1 || h <= 1)
    {
        return 0.0f;
    }

    const ProjectedPoint p = projectWorldToWidget(mvp, world, w, h);
    if (!p.valid)
    {
        return 0.0f;
    }

    const float ndcX = (2.0f * static_cast<float>(p.pos.x()) / static_cast<float>(w)) - 1.0f;
    const float ndcY = 1.0f - (2.0f * static_cast<float>(p.pos.y()) / static_cast<float>(h));

    // One pixel to the right in widget coordinates.
    const float ndcX2 = (2.0f * static_cast<float>(p.pos.x() + 1.0f) / static_cast<float>(w)) - 1.0f;

    const QVector3D w0 = unprojectNdc(invMvp, ndcX, ndcY, p.ndcZ);
    const QVector3D w1 = unprojectNdc(invMvp, ndcX2, ndcY, p.ndcZ);
    const QVector3D d = w1 - w0;
    const float len = d.length();
    return std::isfinite(len) ? len : 0.0f;
}

float distanceSqPointToSegment(const QPointF& p, const QPointF& a, const QPointF& b) noexcept
{
    const double abx = b.x() - a.x();
    const double aby = b.y() - a.y();
    const double apx = p.x() - a.x();
    const double apy = p.y() - a.y();
    const double abLen2 = abx * abx + aby * aby;
    if (abLen2 <= sed::math::kTinyEpsilon)
    {
        return static_cast<float>(apx * apx + apy * apy);
    }

    double t = (apx * abx + apy * aby) / abLen2;
    t = std::clamp(t, 0.0, 1.0);
    const double cx = a.x() + abx * t;
    const double cy = a.y() + aby * t;
    const double dx = p.x() - cx;
    const double dy = p.y() - cy;
    return static_cast<float>(dx * dx + dy * dy);
}

constexpr std::uint64_t kFnv1aOffset64 = 14695981039346656037ull;
constexpr std::uint64_t kFnv1aPrime64 = 1099511628211ull;

std::uint64_t hashAppendBytes(std::uint64_t h, const void* data, const std::size_t size) noexcept
{
    const auto* p = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i)
    {
        h ^= static_cast<std::uint64_t>(p[i]);
        h *= kFnv1aPrime64;
    }
    return h;
}

template <typename T>
std::uint64_t hashAppendPod(std::uint64_t h, const T& v) noexcept
{
    static_assert(std::is_trivially_copyable_v<T>);
    return hashAppendBytes(h, &v, sizeof(T));
}

std::uint64_t hashAppendString(std::uint64_t h, const std::string_view s) noexcept
{
    const std::uint64_t len = static_cast<std::uint64_t>(s.size());
    h = hashAppendPod(h, len);
    if (!s.empty())
    {
        h = hashAppendBytes(h, s.data(), s.size());
    }
    return h;
}

std::uint64_t hashAppendDouble(std::uint64_t h, const double v) noexcept
{
    std::uint64_t bits{};
    static_assert(sizeof(bits) == sizeof(v));
    std::memcpy(&bits, &v, sizeof(bits));
    return hashAppendPod(h, bits);
}

std::uint64_t thingGeometrySignature(const Thing& thing) noexcept
{
    std::uint64_t h = kFnv1aOffset64;

    // Geometry-affecting identity (exclude transform/sector so we can detect pure xform edits).
    h = hashAppendString(h, thing.templateName);
    h = hashAppendString(h, thing.name);
    h = hashAppendPod(h, thing.layer);

    h = hashAppendDouble(h, thing.bbox.x1);
    h = hashAppendDouble(h, thing.bbox.y1);
    h = hashAppendDouble(h, thing.bbox.z1);
    h = hashAppendDouble(h, thing.bbox.x2);
    h = hashAppendDouble(h, thing.bbox.y2);
    h = hashAppendDouble(h, thing.bbox.z2);

    // Hash only values that can change the rendered representation itself.
    // Runtime-light fields are intentionally excluded: their UBO/gizmo state
    // is resolved from the live level every frame, while the emitting owner's
    // cached self-light is refreshed through thingRuntimeLightSignature().
    // Folding those fields into the geometry signature forced every light
    // palette/scrub sample through a complete Things mesh rebuild.
    auto isRenderValue = [](const std::string_view name) -> bool
    {
        return text::iequalsAscii(name, "model3d")
            || text::iequalsAscii(name, "model3do")
            || text::iequalsAscii(name, "sprite")
            || text::iequalsAscii(name, "thingflags")
            || text::iequalsAscii(name, "type");
    };

    std::uint64_t geoValueCount = 0;
    for (const TplValue& v : thing.values)
    {
        if (isRenderValue(v.name))
        {
            ++geoValueCount;
        }
    }

    h = hashAppendPod(h, geoValueCount);
    for (const TplValue& v : thing.values)
    {
        if (!isRenderValue(v.name))
        {
            continue;
        }

        h = hashAppendString(h, v.name);
        h = hashAppendPod(h, static_cast<std::int32_t>(v.type));
        h = hashAppendPod(h, static_cast<std::int32_t>(v.adjType));
        h = hashAppendPod(h, v.intValue);
        h = hashAppendDouble(h, v.floatValue);
        h = hashAppendString(h, v.stringValue);
    }

    return h;
}

std::uint64_t thingRuntimeLightSignature(const Thing& thing) noexcept
{
    std::uint64_t h = kFnv1aOffset64;

    auto isLightValue = [](const std::string_view name) -> bool
    {
        return text::iequalsAscii(name, "light")
            || text::iequalsAscii(name, "lightintensity")
            || text::iequalsAscii(name, "lightoffset")
            || text::iequalsAscii(name, "typeflags");
    };

    std::uint64_t lightValueCount = 0;
    for (const TplValue& value : thing.values)
    {
        if (isLightValue(value.name))
        {
            ++lightValueCount;
        }
    }

    h = hashAppendPod(h, lightValueCount);
    for (const TplValue& value : thing.values)
    {
        if (!isLightValue(value.name))
        {
            continue;
        }

        h = hashAppendString(h, value.name);
        h = hashAppendPod(h, static_cast<std::int32_t>(value.type));
        h = hashAppendPod(h, static_cast<std::int32_t>(value.adjType));
        h = hashAppendPod(h, value.intValue);
        h = hashAppendDouble(h, value.floatValue);
        h = hashAppendString(h, value.stringValue);
    }

    return h;
}
