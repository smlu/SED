#include "gl_view_common.hpp"
#include "ui/components/ui_decimal_format.hpp"

#include "sed/level/arch_lighting.hpp"

#include "render/gizmos/gizmo_icon_utils.hpp"
#include "render/picking/viewport_ray_utils.hpp"
#include "render/gizmos/light_category_gizmo_utils.hpp"
#include "render/gizmos/light_range_gizmo_utils.hpp"
#include "render/gizmos/sector_thrust_gizmo_utils.hpp"
#include "render/selection/surface_selection_utils.hpp"
#include "render/transform/geometry_transform_preview_utils.hpp"
#include "render/transform/authoring_transform_utils.hpp"
#include "ui/components/viewport_icons.hpp"
#include "thing/thing_light_authoring_utils.hpp"
#include "ui/shortcuts/selection_modifier_utils.hpp"

#include <bit>
#include <numeric>

namespace sed
{


namespace
{

template <typename TVertex>
bool uploadGlViewGizmoTriBuffer(
    QOpenGLBuffer& buf,
    int& allocatedBytes,
    const std::vector<TVertex>& verts)
{
    const std::size_t byteCount = verts.size() * sizeof(TVertex);
    if (byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return false;
    }

    const int bytes = static_cast<int>(byteCount);
    if (bytes <= 0)
    {
        return true;
    }

    if (allocatedBytes < bytes)
    {
        const int half = allocatedBytes / 2;
        const int grown = allocatedBytes + std::max(half, 4096);
        const int newCap =
            (allocatedBytes <= 0)
                ? bytes
                : std::max(bytes, grown);
        buf.allocate(nullptr, newCap);
        allocatedBytes = newCap;
    }

    buf.write(0, verts.data(), bytes);
    return true;
}

bool uploadGlViewGizmoFloatBuffer(
    QOpenGLBuffer& buf,
    int& allocatedBytes,
    const std::vector<float>& verts)
{
    const std::size_t byteCount = verts.size() * sizeof(float);
    if (byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        return false;
    }

    const int bytes = static_cast<int>(byteCount);
    if (bytes <= 0)
    {
        return true;
    }

    if (allocatedBytes < bytes)
    {
        const int half = allocatedBytes / 2;
        const int grown = allocatedBytes + std::max(half, 4096);
        const int newCap =
            (allocatedBytes <= 0)
                ? bytes
                : std::max(bytes, grown);
        buf.allocate(nullptr, newCap);
        allocatedBytes = newCap;
    }

    buf.write(0, verts.data(), bytes);
    return true;
}

} // namespace

QOpenGLTexture* GlView::getLightTexture()
{
    if (lightTexture_)
    {
        return lightTexture_.get();
    }

    constexpr int size = 128;
    QImage img(size, size, QImage::Format_RGBA8888_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPointF c(size * 0.5, size * 0.5);
    const qreal coreR = size * 0.20;

    // "Sun"/bulb-like sprite (Unity-style): white mask tinted at draw time by vertex color.
    {
        const int rays = 8;
        const qreal rayW = std::max<qreal>(1.0, size * 0.045);
        const qreal inner = coreR * 1.15;
        const qreal outer = coreR * 2.25;

        // Rays (outlined for readability).
        {
            QPen rayOutline(QColor(0, 0, 0, 255));
            rayOutline.setWidthF(rayW * 1.35);
            rayOutline.setCapStyle(Qt::RoundCap);
            p.setPen(rayOutline);

            for (int i = 0; i < rays; ++i)
            {
                const double a = (static_cast<double>(i) / static_cast<double>(rays)) * (2.0 * std::numbers::pi_v<double>);
                const qreal ca = static_cast<qreal>(std::cos(a));
                const qreal sa = static_cast<qreal>(std::sin(a));
                const QPointF p0 = c + QPointF(ca * inner, sa * inner);
                const QPointF p1 = c + QPointF(ca * outer, sa * outer);
                p.drawLine(p0, p1);
            }

            QPen rayPen(QColor(255, 255, 255, 255));
            rayPen.setWidthF(rayW);
            rayPen.setCapStyle(Qt::RoundCap);
            p.setPen(rayPen);

            for (int i = 0; i < rays; ++i)
            {
                const double a = (static_cast<double>(i) / static_cast<double>(rays)) * (2.0 * std::numbers::pi_v<double>);
                const qreal ca = static_cast<qreal>(std::cos(a));
                const qreal sa = static_cast<qreal>(std::sin(a));
                const QPointF p0 = c + QPointF(ca * inner, sa * inner);
                const QPointF p1 = c + QPointF(ca * outer, sa * outer);
                p.drawLine(p0, p1);
            }
        }

        // Core disc.
        const qreal discR = coreR * 1.35;
        QRadialGradient core(c, discR);
        core.setColorAt(0.0, QColor(255, 255, 255, 255));
        core.setColorAt(0.75, QColor(255, 255, 255, 255));
        core.setColorAt(1.0, QColor(255, 255, 255, 255));
        p.setPen(Qt::NoPen);
        p.setBrush(QBrush(core));
        p.drawEllipse(c, discR, discR);

        // Disc outline.
        {
            QPen discOutline(QColor(0, 0, 0, 255));
            discOutline.setWidthF(std::max<qreal>(1.0, size * 0.028));
            discOutline.setCapStyle(Qt::RoundCap);
            discOutline.setJoinStyle(Qt::RoundJoin);
            p.setPen(discOutline);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, discR, discR);
        }

        // A thin halo distinguishes the authored source body from the rays.
        // The white mask is tinted by the light color at draw time.
        {
            const qreal haloR = discR * 1.55;
            QPen haloOutline(QColor(0, 0, 0, 255));
            haloOutline.setWidthF(std::max<qreal>(1.0, size * 0.030));
            p.setPen(haloOutline);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(c, haloR, haloR);
            QPen halo(QColor(255, 255, 255, 255));
            halo.setWidthF(std::max<qreal>(1.0, size * 0.014));
            p.setPen(halo);
            p.drawEllipse(c, haloR, haloR);
        }

        // Tiny highlight.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 255));
        p.drawEllipse(QPointF(c.x() - discR * 0.35, c.y() - discR * 0.35), discR * 0.35, discR * 0.35);
    }

    p.end();

    // Upload as straight-alpha and use standard blending (SRC_ALPHA) in the shader pipeline.
    // This avoids premultiplied-alpha edge artifacts when additional tinting is applied in the fragment shader.
    const QImage upload = img.mirrored().convertToFormat(QImage::Format_RGBA8888);
    lightTexture_ = std::make_unique<QOpenGLTexture>(upload, QOpenGLTexture::DontGenerateMipMaps);
    lightTexture_->setMinificationFilter(QOpenGLTexture::Linear);
    lightTexture_->setMagnificationFilter(QOpenGLTexture::Linear);
    lightTexture_->setWrapMode(QOpenGLTexture::ClampToEdge);
    return lightTexture_.get();
}

QOpenGLTexture* GlView::getLightTexture(const render::LightGizmoKind kind)
{
    if (kind == render::LightGizmoKind::Baked)
    {
        return getLightTexture();
    }

    std::unique_ptr<QOpenGLTexture>* target = nullptr;
    switch (kind)
    {
        case render::LightGizmoKind::ThingRegular:
            target = &thingLightTexture_;
            break;
        case render::LightGizmoKind::ThingHead:
            target = &thingHeadLightTexture_;
            break;
        case render::LightGizmoKind::SectorPoint:
            target = &sectorLightTexture_;
            break;
        case render::LightGizmoKind::Baked:
            break;
    }
    if (!target)
    {
        return nullptr;
    }
    if (*target)
    {
        return target->get();
    }

    constexpr int size = 128;
    QImage image{};
    if (kind == render::LightGizmoKind::ThingRegular)
    {
        image = render::renderThingRegularLightIconImage(Qt::white, size);
    }
    else if (kind == render::LightGizmoKind::ThingHead)
    {
        image = render::renderThingHeadlightIconImage(Qt::white, size);
    }
    else
    {
        // Sector point lights use the same simple rounded-square source and
        // outward rays as the toolbar action. The white mask remains tintable
        // with the authored IJIM sector-light RGB at draw time.
        image = render::renderSectorPointLightIconImage(Qt::white, size);
    }

    const QImage upload = image.mirrored().convertToFormat(QImage::Format_RGBA8888);
    *target = std::make_unique<QOpenGLTexture>(upload, QOpenGLTexture::DontGenerateMipMaps);
    (*target)->setMinificationFilter(QOpenGLTexture::Linear);
    (*target)->setMagnificationFilter(QOpenGLTexture::Linear);
    (*target)->setWrapMode(QOpenGLTexture::ClampToEdge);
    return target->get();
}

QOpenGLTexture* GlView::getArchitecturalLightingTexture()
{
    if (architecturalLightingTexture_)
    {
        return architecturalLightingTexture_.get();
    }

    constexpr int size = 128;
    const QImage image = render::renderArchitecturalLightingIconImage(
        Qt::white,
        size);
    const QImage upload = image.mirrored().convertToFormat(QImage::Format_RGBA8888);
    architecturalLightingTexture_ = std::make_unique<QOpenGLTexture>(
        upload,
        QOpenGLTexture::DontGenerateMipMaps);
    architecturalLightingTexture_->setMinificationFilter(QOpenGLTexture::Linear);
    architecturalLightingTexture_->setMagnificationFilter(QOpenGLTexture::Linear);
    architecturalLightingTexture_->setWrapMode(QOpenGLTexture::ClampToEdge);
    return architecturalLightingTexture_.get();
}

QOpenGLTexture* GlView::getGizmoTexture(const std::string& type)
{
    auto it = gizmoTextures_.find(type);
    if (it != gizmoTextures_.end())
    {
        return it->second.get();
    }

    constexpr int size = 256;
    QImage img(size, size, QImage::Format_RGBA8888_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QColor bg = render::gizmoIconColorForType(type, thingTypeColors_);

    // Map-pin marker: colored pin with a white center and a label.
    const qreal cx = size * 0.5;
    const qreal cy = size * 0.35;
    const qreal r = size * 0.33;     // wider circle
    const qreal tipY = size * 0.80;  // shorter pin

    QPainterPath pin;
    pin.setFillRule(Qt::WindingFill);
    pin.addEllipse(QPointF(cx, cy), r, r);
    QPolygonF tail;
    tail << QPointF(cx - r * 0.48, cy + r * 0.38) << QPointF(cx + r * 0.48, cy + r * 0.38) << QPointF(cx, tipY);
    pin.addPolygon(tail);
    pin = pin.simplified();

    // Outer pin (filled) with subtle dark outline for readability.
    const qreal outerPenW = std::max<qreal>(3.0, size * 0.03125); // ~4px at 128x128
    p.setPen(QPen(QColor(0, 0, 0, 170), outerPenW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(bg);
    p.drawPath(pin);

    // Inner circle: white fill, outlined with the type color.
    {
        const qreal innerR = r * 0.82; // wider center
        QColor innerOutline = bg;
        innerOutline.setAlpha(230);
        const qreal innerPenW = std::max<qreal>(1.25, size * 0.008); // thinner border
        p.setPen(QPen(innerOutline, innerPenW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(QColor(250, 250, 250, 255));
        p.drawEllipse(QPointF(cx, cy), innerR, innerR);

        // Center label.
        const QString label = render::gizmoIconLabelForType(type);
        QFont font = p.font();
        font.setBold(true);
        const int px = std::clamp(static_cast<int>(std::lround(innerR * 1.15)), 14, 48);
        font.setPixelSize(px);
        p.setFont(font);

        const QRectF textRect(cx - innerR, cy - innerR, innerR * 2.0, innerR * 2.0);

        // Tiny shadow to help readability in bright scenes.
        p.setPen(QColor(0, 0, 0, 70));
        p.drawText(textRect.translated(0.0, 1.0), Qt::AlignCenter, label);

        p.setPen(QColor(20, 20, 20, 235));
        p.drawText(textRect, Qt::AlignCenter, label);
    }

    p.end();

    // Upload as straight-alpha; gizmos are tinted/blended in the main shader pipeline.
    const QImage upload = img.mirrored().convertToFormat(QImage::Format_RGBA8888);
    auto tex = std::make_unique<QOpenGLTexture>(upload, QOpenGLTexture::DontGenerateMipMaps);
    tex->setMinificationFilter(QOpenGLTexture::Linear);
    tex->setMagnificationFilter(QOpenGLTexture::Linear);
    tex->setWrapMode(QOpenGLTexture::ClampToEdge);

    QOpenGLTexture* raw = tex.get();
    gizmoTextures_[type] = std::move(tex);
    return raw;
}

QOpenGLTexture* GlView::getArchThingGizmoTexture()
{
    if (archThingGizmoTexture_)
    {
        return archThingGizmoTexture_.get();
    }

    const QImage image = render::renderArchThingBadgeImage(128);
    const QImage upload = image.mirrored().convertToFormat(QImage::Format_RGBA8888);
    archThingGizmoTexture_ = std::make_unique<QOpenGLTexture>(
        upload, QOpenGLTexture::DontGenerateMipMaps);
    archThingGizmoTexture_->setMinificationFilter(QOpenGLTexture::Linear);
    archThingGizmoTexture_->setMagnificationFilter(QOpenGLTexture::Linear);
    archThingGizmoTexture_->setWrapMode(QOpenGLTexture::ClampToEdge);
    return archThingGizmoTexture_.get();
}

QVector3D GlView::lightGizmoDisplayPositionForEntry(
    const render::LightGizmoEntry& entry,
    QVector3D,
    const float baseHalfSize,
    const bool entryPositionAlreadyPreviewed) const noexcept
{
    render::LightGizmoEntry displayEntry = entry;
    ThingBounds combinedBounds{};
    QVector3D ownerPosition{};
    const QVector3D* ownerPositionPtr = nullptr;
    bool ownerHasArchThing = false;
    const auto includeBounds = [&combinedBounds](const ThingBounds& candidate)
    {
        if (!candidate.valid)
        {
            return;
        }
        render::includePoint(combinedBounds, candidate.min);
        render::includePoint(combinedBounds, candidate.max);
    };
    if (entry.category == render::LightGizmoCategory::Thing
        && entry.ownerIndex >= 0)
    {
        const std::size_t owner = static_cast<std::size_t>(entry.ownerIndex);
        const bool propertyPreviewOwner =
            thingPropertyTransformPreviewActive_
            && thingPropertyTransformPreviewThings_.contains(entry.ownerIndex);
        bool pointerPreviewOwner = false;
        if (transformDrag_.active && transformPreviewActive_
            && owner < thingPickIds_.size())
        {
            const int ownerPickId = static_cast<int>(thingPickIds_[owner]);
            for (int i = 0; i < transformPreviewIdCount_; ++i)
            {
                if (transformPreviewIds_[static_cast<std::size_t>(i)] == ownerPickId)
                {
                    pointerPreviewOwner = true;
                    break;
                }
            }
        }
        const bool transformPreviewOwner = propertyPreviewOwner || pointerPreviewOwner;
        if (propertyPreviewOwner
            || (pointerPreviewOwner && !entryPositionAlreadyPreviewed))
        {
            displayEntry.position = transformPreviewMatrix_.map(entry.position);
        }
        const auto includeOwnerBounds = [&](const ThingBounds& candidate)
        {
            if (!candidate.valid)
            {
                return;
            }
            if (!transformPreviewOwner)
            {
                includeBounds(candidate);
                return;
            }

            const QVector3D mn = candidate.min;
            const QVector3D mx = candidate.max;
            const std::array<QVector3D, 8> corners{
                QVector3D(mn.x(), mn.y(), mn.z()),
                QVector3D(mx.x(), mn.y(), mn.z()),
                QVector3D(mx.x(), mx.y(), mn.z()),
                QVector3D(mn.x(), mx.y(), mn.z()),
                QVector3D(mn.x(), mn.y(), mx.z()),
                QVector3D(mx.x(), mn.y(), mx.z()),
                QVector3D(mx.x(), mx.y(), mx.z()),
                QVector3D(mn.x(), mx.y(), mx.z()),
            };
            for (const QVector3D& corner : corners)
            {
                render::includePoint(combinedBounds, transformPreviewMatrix_.map(corner));
            }
        };
        if (owner < thingBounds_.size())
        {
            includeOwnerBounds(thingBounds_[owner]);
        }
        if (owner < spriteThingBounds_.size())
        {
            includeOwnerBounds(spriteThingBounds_[owner]);
        }
        if (level_ && owner < level_->things.size())
        {
            const Thing& thing = level_->things[owner];
            ownerHasArchThing = level_->kind == ProjectType::JKDF2
                && showArchThingGizmos_
                && (sed::level::thingFlags(*level_, templates_, thing)
                    & 0x00000008u) != 0u;
            ownerPosition = QVector3D(
                static_cast<float>(thing.x),
                static_cast<float>(thing.y),
                static_cast<float>(thing.z));
            if (render::finiteLightGizmoPosition(ownerPosition))
            {
                if (propertyPreviewOwner)
                {
                    ownerPosition = transformPreviewMatrix_.map(ownerPosition);
                }
                else if (pointerPreviewOwner)
                {
                    // The caller either passed its already-previewed emitter
                    // position or this helper mapped a raw source entry above.
                    ownerPosition = displayEntry.position;
                }
                ownerPositionPtr = &ownerPosition;
            }
        }
    }
    const float thingTypeHalfSize =
        std::clamp(thingGizmoWorldSize_, 0.001f, 1.0f) * 0.5f;
    const float architecturalHalfSize =
        std::clamp(lightGizmoWorldSize_, 0.001f, 1.0f) * 0.61f;
    render::ThingGizmoStackLayout stackLayout{};
    stackLayout.thingTypeVisible = showThingGizmos_;
    stackLayout.archThingVisible = ownerHasArchThing;
    stackLayout.thingLightVisible = true;
    stackLayout.thingTypeOffset = thingGizmoHeightAboveModel_;
    stackLayout.thingLightOffset = thingLightGizmoOffsetAboveModel_;
    stackLayout.archThingOffset = archThingGizmoOffsetAboveModel_;
    stackLayout.spacing = thingGizmoStackSpacing_;
    return render::lightGizmoDisplayPosition(
        displayEntry,
        baseHalfSize,
        combinedBounds.valid ? &combinedBounds : nullptr,
        ownerPositionPtr,
        thingTypeHalfSize,
        architecturalHalfSize,
        stackLayout);
}

QVector3D GlView::lightGizmoDisplayPositionForEntry(
    const render::LightGizmoEntry& entry,
    const QMatrix4x4& view) const noexcept
{
    bool inverted = false;
    const QMatrix4x4 inverseView = view.inverted(&inverted);
    QVector3D cameraUp = inverted
        ? QVector3D(inverseView(0, 1), inverseView(1, 1), inverseView(2, 1))
        : QVector3D(0.0f, 0.0f, 1.0f);
    const float halfSize = std::clamp(lightGizmoWorldSize_, 0.001f, 1.0f) * 0.5f;
    return lightGizmoDisplayPositionForEntry(entry, cameraUp, halfSize);
}

void GlView::drawLightGizmos(const QMatrix4x4& mvp)
{
    if (!level_)
    {
        return;
    }

    auto* f = QOpenGLContext::currentContext()->functions();
    if (!f || !program_ || !lightGizmoTriVao_ || !lightGizmoTriVbo_)
    {
        return;
    }

    QMatrix4x4 invView{};
    bool invOk = false;
    invView = camera_.viewMatrix().inverted(&invOk);
    if (!invOk)
    {
        return;
    }

    QVector3D camRight(invView(0, 0), invView(1, 0), invView(2, 0));
    QVector3D camUp(invView(0, 1), invView(1, 1), invView(2, 1));
    if (camRight.lengthSquared() > sed::math::kTinyEpsilonF) { camRight.normalize(); }
    if (camUp.lengthSquared() > sed::math::kTinyEpsilonF) { camUp.normalize(); }

    const float halfSizeBase = std::clamp(lightGizmoWorldSize_, 0.001f, 1.0f) * 0.5f;

    struct LightQuad
    {
        QVector3D center{};
        QVector4D color{};
        float halfSize{};
        render::LightGizmoKind kind{ render::LightGizmoKind::Baked };
    };

    struct CategoryBatch
    {
        render::LightGizmoCategory category{ render::LightGizmoCategory::Baked };
        std::vector<LightQuad> quads{};
    };
    std::array<CategoryBatch, 3> batches{
        CategoryBatch{ render::LightGizmoCategory::Baked, {} },
        CategoryBatch{ render::LightGizmoCategory::Thing, {} },
        CategoryBatch{ render::LightGizmoCategory::Sector, {} },
    };

    const auto appendEntries = [&](CategoryBatch& batch, const std::vector<render::LightGizmoEntry>& entries)
    {
        const bool alwaysVisible =
            alwaysShowLightGizmoCategories_[static_cast<std::size_t>(batch.category)];
        if (!render::lightGizmoCategoryVisible(
                batch.category,
                lightGizmoCategory_,
                alwaysVisible,
                renderPipeline_ == RenderPipeline::Editor
                    && lightGizmoEditingActive_))
        {
            return;
        }
        batch.quads.reserve(entries.size());
        const bool previewMoveHandle =
            transformDrag_.handle == TransformHandle::MoveX
            || transformDrag_.handle == TransformHandle::MoveY
            || transformDrag_.handle == TransformHandle::MoveZ
            || transformDrag_.handle == TransformHandle::MoveFree;
        const bool previewRotateHandle =
            transformDrag_.handle == TransformHandle::RotateX
            || transformDrag_.handle == TransformHandle::RotateY
            || transformDrag_.handle == TransformHandle::RotateZ
            || transformDrag_.handle == TransformHandle::RotateView;
        std::unordered_set<std::int32_t> regularOwnerMoveOwners{};
        if (batch.category == render::LightGizmoCategory::Thing
            && transformDrag_.target == TransformTargetKind::ThingLight
            && transformDrag_.lightKind == render::LightGizmoKind::ThingRegular
            && previewMoveHandle)
        {
            regularOwnerMoveOwners.reserve(entries.size());
            for (const render::LightGizmoEntry& candidate : entries)
            {
                if (candidate.kind == render::LightGizmoKind::ThingRegular)
                {
                    regularOwnerMoveOwners.emplace(candidate.ownerIndex);
                }
            }
        }
        for (const render::LightGizmoEntry& entry : entries)
        {
            if (!isLayerVisible(entry.layer))
            {
                continue;
            }

            bool selected = false;
            bool inSelection = false;
            switch (batch.category)
            {
                case render::LightGizmoCategory::Baked:
                    selected = selectedLight_ == entry.ownerIndex;
                    inSelection = selected || highlightedSelection_.lights.contains(entry.ownerIndex);
                    break;
                case render::LightGizmoCategory::Thing:
                    selected = selectedThing_ == entry.ownerIndex
                        && entry.kind == selectedLightGizmoKind_;
                    inSelection = selectedThing_ == entry.ownerIndex
                        || highlightedSelection_.things.contains(entry.ownerIndex);
                    break;
                case render::LightGizmoCategory::Sector:
                    selected = selectedSector_ == entry.ownerIndex;
                    inSelection = selected || highlightedSelection_.sectors.contains(entry.ownerIndex);
                    break;
                case render::LightGizmoCategory::Architectural:
                    // Architectural owners are drawn by the ARCH overlay
                    // path, not by this runtime-emitter batch.
                    break;
            }

            render::LightGizmoEntry previewEntry = entry;
            const bool propertyPreviewOwner =
                batch.category == render::LightGizmoCategory::Thing
                && thingPropertyTransformPreviewActive_
                && thingPropertyTransformPreviewThings_.contains(entry.ownerIndex);
            const bool movableTarget =
                (batch.category == render::LightGizmoCategory::Baked
                    && transformDrag_.target == TransformTargetKind::Light)
                || (batch.category == render::LightGizmoCategory::Thing
                    && (transformDrag_.target == TransformTargetKind::Thing
                        || transformDrag_.target == TransformTargetKind::ThingLight))
                || (batch.category == render::LightGizmoCategory::Sector
                    && transformDrag_.target == TransformTargetKind::SectorLight);
            const bool moveHandle = previewMoveHandle;
            const bool rotateHandle = previewRotateHandle;
            const bool regularOwnerMove =
                transformDrag_.target == TransformTargetKind::ThingLight
                && transformDrag_.lightKind == render::LightGizmoKind::ThingRegular
                && moveHandle
                && regularOwnerMoveOwners.contains(entry.ownerIndex);
            const bool sameEmitter =
                transformDrag_.target != TransformTargetKind::ThingLight
                || entry.kind == transformDrag_.lightKind
                || regularOwnerMove;
            bool entryPositionAlreadyPreviewed = false;
            // Item-editor previews are mapped inside
            // lightGizmoDisplayPositionForEntry(), together with the owner
            // bounds used for regular-icon presentation.
            if (!propertyPreviewOwner
                && inSelection && transformDrag_.active && movableTarget && sameEmitter)
            {
                if (batch.category == render::LightGizmoCategory::Thing
                    && transformDrag_.target == TransformTargetKind::Thing
                    && (moveHandle || rotateHandle))
                {
                    // Thing lights are not independent entities. Preview the
                    // emitter with the exact matrix used for its owning Thing,
                    // including rotation of a Thing-local headlight offset.
                    previewEntry.position = transformPreviewMatrix_.map(entry.position);
                    entryPositionAlreadyPreviewed = true;
                }
                else if (moveHandle)
                {
                    previewEntry.position += transformDrag_.translation;
                    entryPositionAlreadyPreviewed = true;
                }
            }
            const QVector3D position = lightGizmoDisplayPositionForEntry(
                previewEntry,
                camUp,
                halfSizeBase,
                entryPositionAlreadyPreviewed);

            float halfSize = halfSizeBase;
            if (batch.category != render::LightGizmoCategory::Baked)
            {
                halfSize *= batch.category == render::LightGizmoCategory::Sector ? 1.42f : 1.22f;
            }
            if (selected)
            {
                halfSize *= 1.20f;
            }
            if (!std::isfinite(halfSize) || halfSize <= 0.0f)
            {
                continue;
            }

            // Source bodies stay fully opaque; selection remains clear through
            // the existing size emphasis, range ring, and active-family draw
            // order rather than making idle lights look washed out.
            constexpr float alpha = 1.0f;
            QVector4D tint(1.0f, 1.0f, 1.0f, alpha);
            if (lightGizmoUseLightColor_ || batch.category != render::LightGizmoCategory::Baked)
            {
                const float peak = std::max({ entry.color.x(), entry.color.y(), entry.color.z(), 1.0e-6f });
                const float normalize = peak > 1.0f ? (1.0f / peak) : 1.0f;
                tint.setX(std::clamp(entry.color.x() * normalize, 0.08f, 1.0f));
                tint.setY(std::clamp(entry.color.y() * normalize, 0.08f, 1.0f));
                tint.setZ(std::clamp(entry.color.z() * normalize, 0.08f, 1.0f));
            }
            else
            {
                tint = QVector4D(1.00f, 0.92f, 0.20f, alpha);
            }

            batch.quads.push_back(LightQuad{ position, tint, halfSize, entry.kind });
        }
    };

    if (!thingLightGizmoEntriesCacheValid_)
    {
        thingLightGizmoEntriesCache_ =
            render::thingLightGizmoEntries(*level_, templates_);
        thingLightGizmoEntriesCacheValid_ = true;
    }
    appendEntries(batches[0], render::bakedLightGizmoEntries(*level_));
    appendEntries(batches[1], thingLightGizmoEntriesCache_);
    appendEntries(batches[2], render::sectorLightGizmoEntries(*level_));

    const bool haveAny = std::any_of(batches.begin(), batches.end(), [](const CategoryBatch& batch)
    {
        return !batch.quads.empty();
    });
    if (!haveAny)
    {
        return;
    }

    std::vector<TriVertex> verts{};

    auto pushV = [&](const QVector3D& p, const float u, const float v, const QVector4D& color)
    {
        TriVertex tv{};
        tv.x = p.x();
        tv.y = p.y();
        tv.z = p.z();
        tv.u = u;
        tv.v = v;
        tv.r = color.x();
        tv.g = color.y();
        tv.b = color.z();
        tv.a = color.w();
        tv.pickId = 0.0f;
        tv.pickGeo = 4.0f;
        tv.sky = 0.0f;
        verts.push_back(tv);
    };

    auto buildVertices = [&](const std::vector<LightQuad>& quads)
    {
        verts.clear();
        verts.reserve(quads.size() * 6);
        for (const LightQuad& q : quads)
        {
            const QVector3D r = camRight * q.halfSize;
            const QVector3D u = camUp * q.halfSize;
            const QVector3D p0 = q.center - r - u;
            const QVector3D p1 = q.center + r - u;
            const QVector3D p2 = q.center + r + u;
            const QVector3D p3 = q.center - r + u;

            pushV(p0, 0.0f, 0.0f, q.color);
            pushV(p1, 1.0f, 0.0f, q.color);
            pushV(p2, 1.0f, 1.0f, q.color);
            pushV(p0, 0.0f, 0.0f, q.color);
            pushV(p2, 1.0f, 1.0f, q.color);
            pushV(p3, 0.0f, 1.0f, q.color);
        }
    };

    // Render through the main shader with depth testing (occluded by world/3DO geometry).
    program_->bind();
    program_->setUniformValue("u_mvp", mvp);
    program_->setUniformValue("u_lighting_mode", 1); // use v_color as tint
    program_->setUniformValue("u_geo_min", 0);
    program_->setUniformValue("u_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    program_->setUniformValue("u_textured", 1);
    program_->setUniformValue("u_alpha_test", 0);
    program_->setUniformValue("u_uv_scale", QVector2D(1.0f, 1.0f));
    program_->setUniformValue("u_uv_offset", QVector2D(0.0f, 0.0f));
    program_->setUniformValue("u_outline_px", 0.0f);
    program_->setUniformValue("u_flat_shade", false);
    program_->setUniformValue("u_frame_xform_enabled", 0);
    program_->setUniformValue("u_frame_xform_id", 0);
    program_->setUniformValue("u_frame_xform_color_mul", QVector3D(1.0f, 1.0f, 1.0f));
    program_->setUniformValue("u_frame_xform_matrix", QMatrix4x4());
    program_->setUniformValue("u_xform_enabled", 0);
    program_->setUniformValue("u_xform_count", 0);
    program_->setUniformValue("u_xform_matrix", QMatrix4x4());

    const GLboolean depthTestWasEnabled = f->glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = f->glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = f->glIsEnabled(GL_CULL_FACE);
    GLboolean previousDepthMask = GL_TRUE;
    GLint previousDepthFunc = GL_LESS;
    GLint previousCullMode = GL_BACK;
    GLint previousBlendSrc = GL_ONE;
    GLint previousBlendDst = GL_ZERO;
    GLint previousBlendSrcAlpha = GL_ONE;
    GLint previousBlendDstAlpha = GL_ZERO;
    f->glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    f->glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
    f->glGetIntegerv(GL_CULL_FACE_MODE, &previousCullMode);
    f->glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrc);
    f->glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDst);
    f->glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    f->glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);

    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LEQUAL);
    f->glDepthMask(GL_FALSE);
    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glDisable(GL_CULL_FACE);

    bool uploadFailed = false;
    std::array<std::size_t, 3> drawOrder{ 0, 1, 2 };
    const std::size_t activeBatch = static_cast<std::size_t>(lightGizmoCategory_);
    std::stable_sort(drawOrder.begin(), drawOrder.end(), [activeBatch](const std::size_t a, const std::size_t b)
    {
        return (a == activeBatch) < (b == activeBatch);
    });
    const std::array<render::LightGizmoKind, 4> kindDrawOrder{
        render::LightGizmoKind::Baked,
        render::LightGizmoKind::ThingRegular,
        render::LightGizmoKind::ThingHead,
        render::LightGizmoKind::SectorPoint,
    };
    std::vector<LightQuad> kindQuads{};
    for (const std::size_t batchIndex : drawOrder)
    {
        const CategoryBatch& batch = batches[batchIndex];
        if (batch.quads.empty())
        {
            continue;
        }
        for (const render::LightGizmoKind kind : kindDrawOrder)
        {
            kindQuads.clear();
            kindQuads.reserve(batch.quads.size());
            for (const LightQuad& quad : batch.quads)
            {
                if (quad.kind == kind)
                {
                    kindQuads.push_back(quad);
                }
            }
            if (kindQuads.empty())
            {
                continue;
            }

            QOpenGLTexture* texture = getLightTexture(kind);
            if (!texture)
            {
                continue;
            }

            buildVertices(kindQuads);
            lightGizmoTriVao_->bind();
            lightGizmoTriVbo_->bind();
            if (!uploadGlViewGizmoTriBuffer(*lightGizmoTriVbo_, lightGizmoTriVboAllocatedBytes_, verts))
            {
                lightGizmoTriVbo_->release();
                lightGizmoTriVao_->release();
                uploadFailed = true;
                break;
            }
            lightGizmoTriVbo_->release();
            texture->bind(0);
            f->glDrawArrays(GL_TRIANGLES, 0, static_cast<int>(verts.size()));
            lightGizmoTriVao_->release();
        }
        if (uploadFailed)
        {
            break;
        }
    }

    if (cullWasEnabled) { f->glEnable(GL_CULL_FACE); }
    else { f->glDisable(GL_CULL_FACE); }
    f->glCullFace(static_cast<GLenum>(previousCullMode));
    f->glDepthMask(previousDepthMask);
    f->glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    if (depthTestWasEnabled) { f->glEnable(GL_DEPTH_TEST); }
    else { f->glDisable(GL_DEPTH_TEST); }
    f->glBlendFuncSeparate(
        static_cast<GLenum>(previousBlendSrc),
        static_cast<GLenum>(previousBlendDst),
        static_cast<GLenum>(previousBlendSrcAlpha),
        static_cast<GLenum>(previousBlendDstAlpha));
    if (blendWasEnabled) { f->glEnable(GL_BLEND); }
    else { f->glDisable(GL_BLEND); }

    program_->release();
    if (uploadFailed)
    {
        return;
    }
}

void GlView::drawSelectedLightRange(const QMatrix4x4& mvp)
{
    if (!level_)
    {
        return;
    }

    std::vector<render::LightGizmoRangeRing> selectedRanges{};

    const auto appendEntryRings = [&](
        const render::LightGizmoEntry& entry,
        const bool ownerPrimary,
        const bool outerScalable = true,
        const bool innerScalable = true)
    {
        std::vector<render::LightGizmoRangeRing> rings = render::lightGizmoRangeRings(
            entry,
            ownerPrimary,
            selectedLightGizmoKind_,
            outerScalable,
            innerScalable);
        selectedRanges.insert(
            selectedRanges.end(),
            rings.begin(),
            rings.end());
    };

    if (lightGizmoCategory_ == render::LightGizmoCategory::Baked)
    {
        const std::vector<render::SelectedLightRange> bakedSelection =
            render::selectedLightRangeDrawOrder(
                selectedLight_,
                highlightedSelection_.lights,
                level_->lights.size());
        const std::vector<render::LightGizmoEntry> entries = render::bakedLightGizmoEntries(*level_);
        for (const render::SelectedLightRange& selected : bakedSelection)
        {
            const auto entry = std::find_if(entries.begin(), entries.end(), [&](const render::LightGizmoEntry& candidate)
            {
                return candidate.ownerIndex == selected.lightIndex;
            });
            if (entry != entries.end())
            {
                appendEntryRings(*entry, selected.primary);
            }
        }
    }
    else
    {
        const bool thingCategory = lightGizmoCategory_ == render::LightGizmoCategory::Thing;
        if (thingCategory && !thingLightGizmoEntriesCacheValid_)
        {
            thingLightGizmoEntriesCache_ =
                render::thingLightGizmoEntries(*level_, templates_);
            thingLightGizmoEntriesCacheValid_ = true;
        }
        const std::vector<render::LightGizmoEntry> entries = thingCategory
            ? thingLightGizmoEntriesCache_
            : render::sectorLightGizmoEntries(*level_);
        std::int32_t primaryOwner = thingCategory ? selectedThing_ : selectedSector_;
        if (!thingCategory)
        {
            const auto primaryEntry = std::find_if(entries.begin(), entries.end(), [this, primaryOwner](const render::LightGizmoEntry& entry)
            {
                return entry.ownerIndex == primaryOwner && isLayerVisible(entry.layer);
            });
            if (primaryEntry == entries.end())
            {
                const auto fallback = std::find_if(entries.begin(), entries.end(), [this](const render::LightGizmoEntry& entry)
                {
                    return highlightedSelection_.sectors.contains(entry.ownerIndex)
                        && isLayerVisible(entry.layer);
                });
                primaryOwner = fallback == entries.end() ? -1 : fallback->ownerIndex;
            }
        }
        for (const render::LightGizmoEntry& entry : entries)
        {
            const bool selected = thingCategory
                ? (entry.ownerIndex == selectedThing_ || highlightedSelection_.things.contains(entry.ownerIndex))
                : (entry.ownerIndex == selectedSector_ || highlightedSelection_.sectors.contains(entry.ownerIndex));
            if (!selected)
            {
                continue;
            }
            bool outerScalable = true;
            bool innerScalable = true;
            if (!thingCategory
                && entry.ownerIndex >= 0
                && static_cast<std::size_t>(entry.ownerIndex) < level_->sectors.size())
            {
                const PointLight& authored =
                    level_->sectors[static_cast<std::size_t>(entry.ownerIndex)].pointLight;
                const render::LightGizmoRangeScalability scalability =
                    render::sectorLightRangeScalability(entry, authored);
                outerScalable = scalability.outer;
                innerScalable = scalability.inner;
            }
            appendEntryRings(
                entry,
                entry.ownerIndex == primaryOwner,
                outerScalable,
                innerScalable);
        }
    }
    if (selectedRanges.empty())
    {
        return;
    }
    std::stable_sort(selectedRanges.begin(), selectedRanges.end(), [](const render::LightGizmoRangeRing& a, const render::LightGizmoRangeRing& b)
    {
        return a.primary > b.primary;
    });
    std::unordered_set<std::int32_t> regularThingLightOwners{};
    if (lightGizmoCategory_ == render::LightGizmoCategory::Thing)
    {
        if (!thingLightGizmoEntriesCacheValid_)
        {
            thingLightGizmoEntriesCache_ =
                render::thingLightGizmoEntries(*level_, templates_);
            thingLightGizmoEntriesCacheValid_ = true;
        }
        for (const render::LightGizmoEntry& entry : thingLightGizmoEntriesCache_)
        {
            if (entry.kind == render::LightGizmoKind::ThingRegular)
            {
                regularThingLightOwners.emplace(entry.ownerIndex);
            }
        }
    }

    if (!transformGizmoLineVao_ || !transformGizmoLineVbo_)
    {
        return;
    }

    auto* f = QOpenGLContext::currentContext()->functions();
    if (!f)
    {
        return;
    }

    QMatrix4x4 invView{};
    bool invOk = false;
    invView = camera_.viewMatrix().inverted(&invOk);
    if (!invOk)
    {
        return;
    }

    QVector3D camRight(invView(0, 0), invView(1, 0), invView(2, 0));
    QVector3D camUp(invView(0, 1), invView(1, 1), invView(2, 1));
    if (camRight.lengthSquared() > sed::math::kTinyEpsilonF) { camRight.normalize(); }
    if (camUp.lengthSquared() > sed::math::kTinyEpsilonF) { camUp.normalize(); }

    constexpr int kSegs = 64;

    f->glDisable(GL_DEPTH_TEST);
    f->glDepthMask(GL_FALSE);
    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glDisable(GL_CULL_FACE);

    auto drawRange = [&](const render::LightGizmoRangeRing& selectedRange) -> bool
    {
        const render::LightGizmoEntry& entry = selectedRange.entry;
        if (!isLayerVisible(entry.layer) || !render::finiteLightGizmoPosition(entry.position))
        {
            return true;
        }

        float radius = selectedRange.radius;
        if (!std::isfinite(radius) || radius <= 0.0f)
        {
            return true;
        }

        QVector3D center = entry.position;
        if (entry.category == render::LightGizmoCategory::Thing
            && thingPropertyTransformPreviewActive_
            && thingPropertyTransformPreviewThings_.contains(entry.ownerIndex))
        {
            center = transformPreviewMatrix_.map(center);
        }
        const TransformTargetKind expectedTarget =
            entry.category == render::LightGizmoCategory::Baked
                ? TransformTargetKind::Light
                : (entry.category == render::LightGizmoCategory::Thing
                    ? TransformTargetKind::ThingLight
                    : TransformTargetKind::SectorLight);
        const bool moveHandle = transformDrag_.handle == TransformHandle::MoveX
            || transformDrag_.handle == TransformHandle::MoveY
            || transformDrag_.handle == TransformHandle::MoveZ
            || transformDrag_.handle == TransformHandle::MoveFree;
        const bool rotateHandle = transformDrag_.handle == TransformHandle::RotateX
            || transformDrag_.handle == TransformHandle::RotateY
            || transformDrag_.handle == TransformHandle::RotateZ
            || transformDrag_.handle == TransformHandle::RotateView;
        const bool regularOwnerMove =
            transformDrag_.target == TransformTargetKind::ThingLight
            && transformDrag_.lightKind == render::LightGizmoKind::ThingRegular
            && moveHandle
            && regularThingLightOwners.contains(entry.ownerIndex);
        const bool sameEmitter =
            transformDrag_.target != TransformTargetKind::ThingLight
            || entry.category != render::LightGizmoCategory::Thing
            || entry.kind == transformDrag_.lightKind
            || regularOwnerMove;
        if (transformDrag_.active
            && transformDrag_.target == expectedTarget
            && sameEmitter)
        {
            if (entry.category == render::LightGizmoCategory::Thing && (moveHandle || rotateHandle))
            {
                center = transformPreviewMatrix_.map(center);
            }
            else if (moveHandle)
            {
                center += transformDrag_.translation;
            }
            else if (transformDrag_.handle == TransformHandle::ScaleUniform && selectedRange.scalable)
            {
                const float s = transformDrag_.scaleFactor;
                if (std::isfinite(s) && s > 0.0f)
                {
                    radius *= s;
                }
            }
        }

        std::vector<float> verts{};
        verts.reserve(static_cast<std::size_t>(kSegs) * 2u * 3u);
        render::appendLightRangeRingLineVertices(
            verts,
            center,
            camRight,
            camUp,
            radius,
            kSegs,
            selectedRange.dashed);
        if (verts.empty())
        {
            return true;
        }

        const QVector4D color = render::selectedLightRangeColor(selectedRange.primary);

        if (thickLineProgram_)
        {
            thickLineProgram_->bind();
            thickLineProgram_->setUniformValue("u_mvp", mvp);
            thickLineProgram_->setUniformValue("u_viewport_size", QVector2D(static_cast<float>(width()), static_cast<float>(height())));
            thickLineProgram_->setUniformValue("u_line_width_px", selectedRange.primary ? 2.4f : 1.8f);
            thickLineProgram_->setUniformValue("u_color", color);
            thickLineProgram_->setUniformValue("u_frame_xform_enabled", 0);
            thickLineProgram_->setUniformValue("u_frame_xform_id", 0);
            thickLineProgram_->setUniformValue("u_frame_xform_matrix", QMatrix4x4());
            thickLineProgram_->setUniformValue("u_xform_enabled", 0);
            thickLineProgram_->setUniformValue("u_xform_count", 0);
            thickLineProgram_->setUniformValue("u_xform_matrix", QMatrix4x4());

            f->glVertexAttrib1f(3, 0.0f);

            transformGizmoLineVao_->bind();
            transformGizmoLineVbo_->bind();
            if (!uploadGlViewGizmoFloatBuffer(*transformGizmoLineVbo_, transformGizmoLineVboAllocatedBytes_, verts))
            {
                transformGizmoLineVbo_->release();
                transformGizmoLineVao_->release();
                thickLineProgram_->release();
                return false;
            }
            transformGizmoLineVbo_->release();

            f->glDrawArrays(GL_LINES, 0, static_cast<int>(verts.size() / 3));
            transformGizmoLineVao_->release();
            thickLineProgram_->release();
            return true;
        }

        if (!program_)
        {
            return true;
        }

        program_->bind();
        program_->setUniformValue("u_mvp", mvp);
        program_->setUniformValue("u_color", color);
        program_->setUniformValue("u_textured", 0);
        program_->setUniformValue("u_alpha_test", 0);
        program_->setUniformValue("u_lighting_mode", 0);
        program_->setUniformValue("u_geo_min", 0);
        program_->setUniformValue("u_outline_px", 0.0f);
        program_->setUniformValue("u_flat_shade", false);
        program_->setUniformValue("u_frame_xform_enabled", 0);
        program_->setUniformValue("u_frame_xform_id", 0);
        program_->setUniformValue("u_frame_xform_color_mul", QVector3D(1.0f, 1.0f, 1.0f));
        program_->setUniformValue("u_frame_xform_matrix", QMatrix4x4());
        program_->setUniformValue("u_xform_enabled", 0);
        program_->setUniformValue("u_xform_count", 0);
        program_->setUniformValue("u_xform_matrix", QMatrix4x4());

        f->glLineWidth(selectedRange.primary ? 2.0f : 1.0f);
        f->glVertexAttrib4f(2, 1.0f, 1.0f, 1.0f, 1.0f);
        f->glVertexAttrib1f(3, 0.0f);

        transformGizmoLineVao_->bind();
        transformGizmoLineVbo_->bind();
        if (!uploadGlViewGizmoFloatBuffer(*transformGizmoLineVbo_, transformGizmoLineVboAllocatedBytes_, verts))
        {
            transformGizmoLineVbo_->release();
            transformGizmoLineVao_->release();
            f->glLineWidth(1.0f);
            program_->release();
            return false;
        }
        transformGizmoLineVbo_->release();

        f->glDrawArrays(GL_LINES, 0, static_cast<int>(verts.size() / 3));
        transformGizmoLineVao_->release();

        f->glLineWidth(1.0f);
        program_->release();
        return true;
    };

    // Draw secondary dashed rings first so an overlapping primary range stays
    // visibly continuous and retains the existing solid emphasis.
    bool uploadOk = true;
    for (std::size_t i = 1; i < selectedRanges.size() && uploadOk; ++i)
    {
        uploadOk = drawRange(selectedRanges[i]);
    }
    if (uploadOk)
    {
        (void)drawRange(selectedRanges.front());
    }

    f->glEnable(GL_CULL_FACE);
    f->glCullFace(GL_BACK);
    f->glDisable(GL_BLEND);
    f->glDepthMask(GL_TRUE);
    f->glEnable(GL_DEPTH_TEST);
}

void GlView::drawArchitecturalLightingVertices(const QMatrix4x4& mvp)
{
    if (renderPipeline_ != RenderPipeline::Editor
        || !architecturalLightingEditingActive_
        || architecturalLightingSelectedOwnerCaches_.empty()
        || architecturalLightingPointCount_ <= 0
        || !architecturalLightingPointVao_
        || !program_)
    {
        return;
    }

    auto* const f = QOpenGLContext::currentContext()
        ? QOpenGLContext::currentContext()->functions()
        : nullptr;
    if (!f)
    {
        return;
    }

    const int count = std::min(
        architecturalLightingPointCount_,
        static_cast<int>(architecturalLightingPointVertices_.size()));
    if (count <= 0
        || architecturalLightingPointIdentities_.size()
            < static_cast<std::size_t>(count))
    {
        return;
    }

    const int selectedFirst = std::clamp(
        architecturalLightingSelectedFirstPoint_,
        0,
        count);
    const int selectedCount = std::clamp(
        architecturalLightingSelectedPointCount_,
        0,
        count - selectedFirst);
    const int baseCount = selectedFirst;

    int hoveredPoint = -1;
    if (hoveredArchitecturalLightingVertex_)
    {
        const ArchitecturalLightingRenderVertex& hovered =
            *hoveredArchitecturalLightingVertex_;
        for (int pointIndex = 0; pointIndex < baseCount; ++pointIndex)
        {
            const ArchitecturalLightingRenderVertex& candidate =
                architecturalLightingPointIdentities_[static_cast<std::size_t>(pointIndex)];
            if (candidate.thingIndex == hovered.thingIndex
                && candidate.lodIndex == hovered.lodIndex
                && candidate.meshIndex == hovered.meshIndex
                && candidate.vertexIndex == hovered.vertexIndex)
            {
                hoveredPoint = pointIndex;
                break;
            }
        }
    }

    // The sparse marker payload carries channel colors in one dedicated VBO.
    // Draw a light halo and dark outline before the configurable
    // authored/fallback color so both black and bright markers remain visible
    // against the viewport. A hovered marker gets one optional white pass so
    // hover emphasis does not reintroduce per-vertex draws.
    program_->setUniformValue("u_mvp", mvp);
    program_->setUniformValue("u_lighting_mode", 1);
    program_->setUniformValue("u_geo_min", 0);
    program_->setUniformValue("u_textured", 0);
    program_->setUniformValue("u_alpha_test", 0);
    program_->setUniformValue("u_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    // Keep marker depth aligned with the authored vertex. Visibility remains
    // governed by the same scene-depth pass used by the click picker.
    program_->setUniformValue("u_ndc_z_bias", 0.0f);
    program_->setUniformValue("u_force_opaque_alpha", true);

    const bool xray = xraySelectionCyclingEnabled_;
    if (xray)
    {
        f->glDisable(GL_DEPTH_TEST);
    }
    else
    {
        f->glEnable(GL_DEPTH_TEST);
        f->glDepthFunc(GL_LEQUAL);
    }
    f->glDepthMask(GL_FALSE);
    f->glDisable(GL_BLEND);

    architecturalLightingPointVao_->bind();
    const float dpr = std::max(
        1.0f, static_cast<float>(devicePixelRatioF()));
    const float markerSizePx = std::clamp(
        architecturalLightingMarkerSizePx_,
        kArchitecturalLightingMarkerMinSizePx,
        kArchitecturalLightingMarkerMaxSizePx);
    const QVector4D haloColor(0.94f, 0.96f, 1.0f, 1.0f);
    program_->setUniformValue("u_lighting_mode", 0);
    program_->setUniformValue("u_color", haloColor);
    program_->setUniformValue(
        "u_point_size", markerSizePx * dpr + 4.0f * dpr);
    f->glDrawArrays(GL_POINTS, 0, baseCount);

    const QVector4D outlineColor(0.02f, 0.025f, 0.035f, 1.0f);
    program_->setUniformValue("u_lighting_mode", 0);
    program_->setUniformValue("u_color", outlineColor);
    program_->setUniformValue(
        "u_point_size", markerSizePx * dpr + 2.0f * dpr);
    f->glDrawArrays(GL_POINTS, 0, baseCount);

    if (architecturalLightingMarkerUseAuthoredColor_)
    {
        program_->setUniformValue("u_lighting_mode", 1);
        program_->setUniformValue(
            "u_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    }
    else
    {
        program_->setUniformValue("u_lighting_mode", 0);
        program_->setUniformValue(
            "u_color",
            QVector4D(
                architecturalLightingMarkerColor_.redF(),
                architecturalLightingMarkerColor_.greenF(),
                architecturalLightingMarkerColor_.blueF(),
                1.0f));
    }
    program_->setUniformValue("u_point_size", markerSizePx * dpr);
    f->glDrawArrays(GL_POINTS, 0, baseCount);
    if (selectedCount > 0)
    {
        // A white/dark batched pair around the selected range makes the
        // selection clear without replacing the Mono/RGB preview color. The
        // fill is restored below after the contrasting ring passes.
        program_->setUniformValue("u_lighting_mode", 0);
        program_->setUniformValue(
            "u_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
        program_->setUniformValue(
            "u_point_size", (markerSizePx + 7.0f) * dpr);
        f->glDrawArrays(GL_POINTS, selectedFirst, selectedCount);
        program_->setUniformValue(
            "u_color", QVector4D(0.02f, 0.025f, 0.035f, 1.0f));
        program_->setUniformValue(
            "u_point_size", (markerSizePx + 4.0f) * dpr);
        f->glDrawArrays(GL_POINTS, selectedFirst, selectedCount);

        // The ring passes deliberately cover the selected range with
        // contrasting colors. Restore the authored or configured marker fill
        // at the normal size so selection never replaces the channel color
        // with a white/dark disc.
        if (architecturalLightingMarkerUseAuthoredColor_)
        {
            program_->setUniformValue("u_lighting_mode", 1);
            program_->setUniformValue(
                "u_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
        }
        else
        {
            program_->setUniformValue("u_lighting_mode", 0);
            program_->setUniformValue(
                "u_color",
                QVector4D(
                    architecturalLightingMarkerColor_.redF(),
                    architecturalLightingMarkerColor_.greenF(),
                    architecturalLightingMarkerColor_.blueF(),
                    1.0f));
        }
        program_->setUniformValue("u_point_size", markerSizePx * dpr);
        f->glDrawArrays(GL_POINTS, selectedFirst, selectedCount);
    }
    if (hoveredPoint >= 0)
    {
        // Hover uses the same ring treatment as selection. Restore the fill
        // after both contrasting passes so the authored or configured marker
        // color remains readable under the cursor.
        program_->setUniformValue("u_lighting_mode", 0);
        program_->setUniformValue(
            "u_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
        program_->setUniformValue(
            "u_point_size", (markerSizePx + 6.0f) * dpr);
        f->glDrawArrays(GL_POINTS, hoveredPoint, 1);
        program_->setUniformValue(
            "u_color", QVector4D(0.02f, 0.025f, 0.035f, 1.0f));
        program_->setUniformValue(
            "u_point_size", (markerSizePx + 3.0f) * dpr);
        f->glDrawArrays(GL_POINTS, hoveredPoint, 1);

        if (architecturalLightingMarkerUseAuthoredColor_)
        {
            program_->setUniformValue("u_lighting_mode", 1);
            program_->setUniformValue(
                "u_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
        }
        else
        {
            program_->setUniformValue("u_lighting_mode", 0);
            program_->setUniformValue(
                "u_color",
                QVector4D(
                    architecturalLightingMarkerColor_.redF(),
                    architecturalLightingMarkerColor_.greenF(),
                    architecturalLightingMarkerColor_.blueF(),
                    1.0f));
        }
        program_->setUniformValue("u_point_size", markerSizePx * dpr);
        f->glDrawArrays(GL_POINTS, hoveredPoint, 1);
    }
    architecturalLightingPointVao_->release();

    f->glDepthMask(GL_TRUE);
    f->glDepthFunc(GL_LESS);
    f->glEnable(GL_DEPTH_TEST);
    program_->setUniformValue("u_lighting_mode", 0);
    program_->setUniformValue("u_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    program_->setUniformValue("u_force_opaque_alpha", false);
}

void GlView::drawThingGizmos(const QMatrix4x4& mvp)
{
    const bool showStandaloneArchMarkers = showArchThingGizmos_
        && level_ && level_->kind == ProjectType::JKDF2;
    if ((!showThingGizmos_ && !showStandaloneArchMarkers)
        || !level_ || level_->things.empty())
    {
        return;
    }

    auto* f = QOpenGLContext::currentContext()->functions();
    if (!f || !program_ || !thingGizmoTriVao_ || !thingGizmoTriVbo_)
    {
        return;
    }

    // Fixed world-space size (user preference).
    const float gizmoHalfSize = std::clamp(thingGizmoWorldSize_, 0.001f, 1.0f) * 0.5f;
    const float lightStackHalfSize = std::clamp(lightGizmoWorldSize_, 0.001f, 1.0f) * 0.732f;
    const float architecturalStackHalfSize =
        std::clamp(lightGizmoWorldSize_, 0.001f, 1.0f) * 0.61f;
    const bool thingLightCategoryVisible = render::lightGizmoCategoryVisible(
        render::LightGizmoCategory::Thing,
        lightGizmoCategory_,
        alwaysShowLightGizmoCategories_[
            static_cast<std::size_t>(render::LightGizmoCategory::Thing)],
        renderPipeline_ == RenderPipeline::Editor && lightGizmoEditingActive_);
    std::unordered_set<std::int32_t> visibleThingLightOwners{};
    if (thingLightCategoryVisible)
    {
        if (!thingLightGizmoEntriesCacheValid_)
        {
            thingLightGizmoEntriesCache_ =
                render::thingLightGizmoEntries(*level_, templates_);
            thingLightGizmoEntriesCacheValid_ = true;
        }
        for (const render::LightGizmoEntry& lightEntry : thingLightGizmoEntriesCache_)
        {
            if (lightEntry.kind == render::LightGizmoKind::ThingRegular
                && isLayerVisible(lightEntry.layer))
            {
                visibleThingLightOwners.insert(lightEntry.ownerIndex);
            }
        }
    }
    const auto stackLayoutForMarkers = [this, &visibleThingLightOwners](
        const bool thingTypeVisible,
        const bool archThingMarkerVisible,
        const std::int32_t ownerThingIndex)
    {
        render::ThingGizmoStackLayout layout{};
        layout.thingTypeVisible = thingTypeVisible;
        layout.thingLightVisible = visibleThingLightOwners.contains(ownerThingIndex);
        layout.archThingVisible = archThingMarkerVisible;
        layout.thingTypeOffset = thingGizmoHeightAboveModel_;
        layout.thingLightOffset = thingLightGizmoOffsetAboveModel_;
        layout.archThingOffset = archThingGizmoOffsetAboveModel_;
        layout.spacing = thingGizmoStackSpacing_;
        return layout;
    };

    // Compute billboard axes from the camera.
    QMatrix4x4 invView{};
    bool invertOk = false;
    invView = camera_.viewMatrix().inverted(&invertOk);
    if (!invertOk)
    {
        return;
    }

    QVector3D camRight(invView(0, 0), invView(1, 0), invView(2, 0));
    QVector3D camUp(invView(0, 1), invView(1, 1), invView(2, 1));
    if (camRight.lengthSquared() > sed::math::kTinyEpsilonF) { camRight.normalize(); }
    if (camUp.lengthSquared() > sed::math::kTinyEpsilonF) { camUp.normalize(); }

    const bool pointerTransformPreview =
        transformDrag_.active && transformPreviewActive_;
    const bool pointerMovePreview = pointerTransformPreview
        && (transformDrag_.handle == TransformHandle::MoveX
            || transformDrag_.handle == TransformHandle::MoveY
            || transformDrag_.handle == TransformHandle::MoveZ
            || transformDrag_.handle == TransformHandle::MoveFree);
    const bool pointerCpuPreview = pointerTransformPreview && !pointerMovePreview;
    const bool propertyMovePreview =
        thingPropertyTransformPreviewActive_
        && !thingPropertyTransformPreviewRotating_;
    const bool propertyCpuPreview =
        thingPropertyTransformPreviewActive_
        && thingPropertyTransformPreviewRotating_;
    const bool cpuTransformPreview = pointerCpuPreview || propertyCpuPreview;
    const bool rebuildTypeGizmoMetadata = !thingTypeGizmoMetadataValid_;
    const bool rebuildTypeGizmoVertices =
        rebuildTypeGizmoMetadata
        || !thingTypeGizmoCacheValid_
        || cpuTransformPreview
        || std::abs(QVector3D::dotProduct(camRight, thingTypeGizmoCameraRight_) - 1.0f) > 1.0e-6f
        || std::abs(QVector3D::dotProduct(camUp, thingTypeGizmoCameraUp_) - 1.0f) > 1.0e-6f
        || std::abs(gizmoHalfSize - thingTypeGizmoHalfSize_) > 1.0e-6f
        || std::abs(lightStackHalfSize - thingTypeGizmoLightHalfSize_) > 1.0e-6f
        || std::abs(architecturalStackHalfSize
                    - thingTypeGizmoArchitecturalHalfSize_) > 1.0e-6f;

    // Keep resolved inherited types, projected-bounds stack centers, batches,
    // and the uploaded VBO across ordinary paints and selection changes. Pointer tools
    // retain their exact multi-owner CPU preview; an item-editor preview is
    // applied to only one cached icon below.
    const auto isTransformPreviewPickId = [&](const std::uint32_t pickId) -> bool
    {
        if (!transformPreviewActive_ || pickId == 0 || transformPreviewIdCount_ <= 0)
        {
            return false;
        }

        const int id = static_cast<int>(pickId);
        for (int i = 0; i < transformPreviewIdCount_; ++i)
        {
            if (transformPreviewIds_[static_cast<std::size_t>(i)] == id)
            {
                return true;
            }
        }

        return false;
    };

    if (rebuildTypeGizmoMetadata)
    {
    ++thingTypeGizmoMetadataBuildCount_;

    struct WorldBounds
    {
        QVector3D min{};
        QVector3D max{};
        bool valid{};
    };

    auto calcThingWorldBounds = [&](const Thing& thing, const std::size_t thingIndex) -> WorldBounds
    {
        WorldBounds out{};

        // Persistent mesh/sprite construction already resolved resources and
        // recorded exact world bounds per owner. Reuse those bounds here so a
        // camera move cannot reload models, walk model vertices, or resolve a
        // template bbox for every visible Thing merely to billboard its type
        // icon.
        ThingBounds cachedBounds{};
        const auto includeCached = [&](const ThingBounds& candidate)
        {
            if (!candidate.valid) { return; }
            render::includePoint(cachedBounds, candidate.min);
            render::includePoint(cachedBounds, candidate.max);
        };
        if (thingIndex < thingBounds_.size())
        {
            includeCached(thingBounds_[thingIndex]);
        }
        if (thingIndex < spriteThingBounds_.size())
        {
            includeCached(spriteThingBounds_[thingIndex]);
        }
        if (cachedBounds.valid)
        {
            const QVector3D mn = cachedBounds.min;
            const QVector3D mx = cachedBounds.max;
            const std::array<QVector3D, 8> corners{
                QVector3D(mn.x(), mn.y(), mn.z()), QVector3D(mx.x(), mn.y(), mn.z()),
                QVector3D(mx.x(), mx.y(), mn.z()), QVector3D(mn.x(), mx.y(), mn.z()),
                QVector3D(mn.x(), mn.y(), mx.z()), QVector3D(mx.x(), mn.y(), mx.z()),
                QVector3D(mx.x(), mx.y(), mx.z()), QVector3D(mn.x(), mx.y(), mx.z()),
            };
            for (const QVector3D& corner : corners)
            {
                const QVector3D point = corner;
                if (!out.valid)
                {
                    out.min = point;
                    out.max = point;
                    out.valid = true;
                }
                else
                {
                    out.min.setX(std::min(out.min.x(), point.x()));
                    out.min.setY(std::min(out.min.y(), point.y()));
                    out.min.setZ(std::min(out.min.z(), point.z()));
                    out.max.setX(std::max(out.max.x(), point.x()));
                    out.max.setY(std::max(out.max.y(), point.y()));
                    out.max.setZ(std::max(out.max.z(), point.z()));
                }
            }
            return out;
        }

        bool haveBounds = false;
        TVector localMin{};
        TVector localMax{};

        const TplValue* modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3d");
        if (!modelValue)
        {
            modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3do");
        }

        const std::string modelName =
            modelValue ? (modelValue->stringValue.empty() ? modelValue->asString() : modelValue->stringValue) : std::string{};
        if (!modelName.empty())
        {
            const std::shared_ptr<const assets::Model3do> modelPtr = loadModel3do(modelName);
            if (modelPtr && !modelPtr->meshes.empty())
            {
                ModelBounds bounds{};
                if (const auto it = modelBounds3do_.find(modelName); it != modelBounds3do_.end() && it->second.valid)
                {
                    bounds = it->second;
                }
                else
                {
                    bool have = false;
                    TVector mn{};
                    TVector mx{};
                    for (const assets::Model3doMesh& mesh : modelPtr->meshes)
                    {
                        for (const assets::Model3doVertex& v : mesh.vertices)
                        {
                            const TVector p{ v.x, v.y, v.z };
                            if (!have)
                            {
                                mn = p;
                                mx = p;
                                have = true;
                                continue;
                            }
                            mn.x = std::min(mn.x, p.x);
                            mn.y = std::min(mn.y, p.y);
                            mn.z = std::min(mn.z, p.z);
                            mx.x = std::max(mx.x, p.x);
                            mx.y = std::max(mx.y, p.y);
                            mx.z = std::max(mx.z, p.z);
                        }
                    }

                    bounds.valid = have;
                    bounds.min = mn;
                    bounds.max = mx;
                    modelBounds3do_[modelName] = bounds;
                }

                if (bounds.valid)
                {
                    localMin = bounds.min;
                    localMax = bounds.max;
                    haveBounds = true;
                }
            }
        }

        if (!haveBounds)
        {
            const std::string_view tplName =
                !thing.templateName.empty() ? std::string_view(thing.templateName)
                                            : std::string_view(thing.name);
            ThingBox box = thing.bbox;
            for (const ThingTemplate& tpl : level_->templates)
            {
                if (tpl.name == tplName)
                {
                    box = tpl.bbox;
                    break;
                }
            }
            if (templates_)
            {
                if (const ThingTemplate* tpl = templates_->find(tplName))
                {
                    box = tpl->bbox;
                }
            }

            const bool zeroBox = sed::IsZero(box.x1) && sed::IsZero(box.y1) && sed::IsZero(box.z1)
                              && sed::IsZero(box.x2) && sed::IsZero(box.y2) && sed::IsZero(box.z2);

            if (!zeroBox)
            {
                localMin = TVector{ box.x1, box.y1, box.z1 };
                localMax = TVector{ box.x2, box.y2, box.z2 };
                haveBounds = true;
            }
            else
            {
                const double h = static_cast<double>(thingCubeHalfSize_);
                localMin = TVector{ -h, -h, -h };
                localMax = TVector{ h, h, h };
                haveBounds = true;
            }
        }

        if (!haveBounds)
        {
            return out;
        }

        TVector pyr{};
        pyr.pitch = thing.pch;
        pyr.yaw = thing.yaw;
        pyr.roll = thing.rol;
        const TMatrix43 xf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, pyr);

        const TVector mn = localMin;
        const TVector mx = localMax;
        const TVector localCorners[8] = {
            TVector{ mn.x, mn.y, mn.z },
            TVector{ mx.x, mn.y, mn.z },
            TVector{ mx.x, mx.y, mn.z },
            TVector{ mn.x, mx.y, mn.z },
            TVector{ mn.x, mn.y, mx.z },
            TVector{ mx.x, mn.y, mx.z },
            TVector{ mx.x, mx.y, mx.z },
            TVector{ mn.x, mx.y, mx.z },
        };

        bool any = false;
        QVector3D minV{};
        QVector3D maxV{};
        for (const TVector& c : localCorners)
        {
            const TVector p = xf.TransformPoint(c);
            const QVector3D q(
                static_cast<float>(p.x),
                static_cast<float>(p.y),
                static_cast<float>(p.z));

            if (!any)
            {
                minV = q;
                maxV = q;
                any = true;
                continue;
            }

            minV.setX(std::min(minV.x(), q.x()));
            minV.setY(std::min(minV.y(), q.y()));
            minV.setZ(std::min(minV.z(), q.z()));

            maxV.setX(std::max(maxV.x(), q.x()));
            maxV.setY(std::max(maxV.y(), q.y()));
            maxV.setZ(std::max(maxV.z(), q.z()));
        }

        out.valid = any;
        out.min = minV;
        out.max = maxV;
        return out;
    };

    // Group gizmo quads by type for batched texture binding.
    struct GizmoQuad
    {
        QVector3D center;
        QVector3D boundsMin;
        QVector3D boundsMax;
        std::string type;
        float halfSize{};
        std::uint32_t pickId{};
        std::int32_t thingIndex{ -1 };
        bool archThing{};
        QVector3D ownerPosition{};
        QVector3D archThingCenter{};
    };

    std::vector<GizmoQuad> quads;
    quads.reserve(level_->things.size());

    for (std::size_t thingIndex = 0; thingIndex < level_->things.size(); ++thingIndex)
    {
        const Thing& thing = level_->things[thingIndex];
        if (!isLayerVisible(thing.layer))
        {
            continue;
        }
        if (!showThings_ || !shouldShowThing(thing))
        {
            continue;
        }
        if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
        {
            continue;
        }

        const bool classicProject = level_->kind == ProjectType::JKDF2
            || level_->kind == ProjectType::MOTS;
        const bool archThing = classicProject
            && (sed::level::thingFlags(*level_, templates_, thing) & 0x00000008u) != 0u;
        // JKDF2 uses the standalone Arch marker. MotS draws its owner through
        // the ARCH path, while IJIM bit 0x8 means Whip climb and has no marker.
        const bool showArchThingMarker =
            showArchThingGizmos_ && archThing
            && level_->kind == ProjectType::JKDF2;
        if (!showThingGizmos_ && !showArchThingMarker)
        {
            continue;
        }

        const WorldBounds b = calcThingWorldBounds(thing, thingIndex);
        if (!b.valid)
        {
            continue;
        }

        const ThingBounds ownerBounds{ b.min, b.max, true };
        const QVector3D ownerPosition(
            static_cast<float>(thing.x),
            static_cast<float>(thing.y),
            static_cast<float>(thing.z));
        render::ThingGizmoStackLayout stackLayout{};
        stackLayout.thingTypeVisible = showThingGizmos_;
        stackLayout.thingLightVisible = visibleThingLightOwners.contains(
            static_cast<std::int32_t>(thingIndex));
        stackLayout.archThingVisible = showArchThingMarker;
        stackLayout.thingTypeOffset = thingGizmoHeightAboveModel_;
        stackLayout.thingLightOffset = thingLightGizmoOffsetAboveModel_;
        stackLayout.archThingOffset = archThingGizmoOffsetAboveModel_;
        stackLayout.spacing = thingGizmoStackSpacing_;
        const QVector3D center = showThingGizmos_
            ? render::thingGizmoStackDisplayPosition(
                ownerPosition,
                &ownerBounds,
                render::ThingGizmoStackLane::ThingType,
                gizmoHalfSize,
                lightStackHalfSize,
                architecturalStackHalfSize,
                stackLayout)
            : QVector3D{};
        const QVector3D archThingCenter = showArchThingMarker
            ? render::thingGizmoStackDisplayPosition(
                ownerPosition,
                &ownerBounds,
                render::ThingGizmoStackLane::ArchThing,
                gizmoHalfSize,
                lightStackHalfSize,
                architecturalStackHalfSize,
                stackLayout)
            : QVector3D{};

        std::string typeName;
        if (showThingGizmos_)
        {
            const TplValue* typeVal =
                sed::level::thingFindValue(*level_, templates_, thing, "type");
            typeName = "free";
            if (typeVal)
            {
                typeName = typeVal->stringValue.empty()
                    ? typeVal->asString()
                    : typeVal->stringValue;
                typeName = sed::text::lowerAscii(sed::text::trimView(typeName));
                if (typeName.empty())
                {
                    typeName = "free";
                }
            }
        }

        const std::uint32_t pickId = thingIndex < thingPickIds_.size()
            ? thingPickIds_[thingIndex]
            : 0u;
        quads.push_back(GizmoQuad{
            center,
            b.min,
            b.max,
            std::move(typeName),
            gizmoHalfSize,
            pickId,
            static_cast<std::int32_t>(thingIndex),
            archThing,
            ownerPosition,
            archThingCenter });
    }

    if (quads.empty())
    {
        thingTypeGizmoEntries_.clear();
        thingTypeGizmoEntryByPickId_.clear();
        thingTypeGizmoDirtyOwners_.clear();
        thingTypeGizmoBatches_.clear();
        thingTypeGizmoVertices_.clear();
        thingTypeGizmoIconVertexCount_ = 0u;
        thingTypeGizmoArchThingMarkerVertexCount_ = 0u;
        thingTypeGizmoCameraRight_ = camRight;
        thingTypeGizmoCameraUp_ = camUp;
        thingTypeGizmoHalfSize_ = gizmoHalfSize;
        thingTypeGizmoLightHalfSize_ = lightStackHalfSize;
        thingTypeGizmoArchitecturalHalfSize_ = architecturalStackHalfSize;
        thingTypeGizmoMetadataValid_ = true;
        thingTypeGizmoCacheValid_ = !cpuTransformPreview;
        ++thingTypeGizmoBuildCount_;
        return;
    }

    // Sort by type name for batching.
    std::stable_sort(quads.begin(), quads.end(), [](const GizmoQuad& a, const GizmoQuad& b) {
        if (a.type != b.type) { return a.type < b.type; }
        return a.pickId < b.pickId;
    });

    thingTypeGizmoEntries_.clear();
    thingTypeGizmoEntryByPickId_.clear();
    thingTypeGizmoEntries_.reserve(quads.size());
    for (const GizmoQuad& quad : quads)
    {
        thingTypeGizmoEntries_.push_back(ThingTypeGizmoEntry{
            quad.center,
            quad.boundsMin,
            quad.boundsMax,
            quad.type,
            quad.pickId,
            quad.thingIndex,
            quad.archThing,
            quad.ownerPosition,
            quad.archThingCenter });
        if (quad.pickId != 0u)
        {
            thingTypeGizmoEntryByPickId_.emplace(
                quad.pickId,
                thingTypeGizmoEntries_.size() - 1u);
        }
    }
    thingTypeGizmoMetadataValid_ = true;
    thingTypeGizmoDirtyOwners_.clear();
    }

    std::vector<std::size_t> dirtyTypeGizmoEntries{};
    if (thingTypeGizmoMetadataValid_ && !thingTypeGizmoDirtyOwners_.empty())
    {
        dirtyTypeGizmoEntries.reserve(thingTypeGizmoDirtyOwners_.size());
        for (const std::int32_t ownerIndex : thingTypeGizmoDirtyOwners_)
        {
            if (ownerIndex < 0
                || static_cast<std::size_t>(ownerIndex) >= thingPickIds_.size())
            {
                continue;
            }
            const std::uint32_t pickId =
                thingPickIds_[static_cast<std::size_t>(ownerIndex)];
            const auto entryIndex = thingTypeGizmoEntryByPickId_.find(pickId);
            if (entryIndex == thingTypeGizmoEntryByPickId_.end()
                || entryIndex->second >= thingTypeGizmoEntries_.size())
            {
                continue;
            }
            ThingBounds bounds{};
            const auto includeBounds = [&](const ThingBounds& candidate)
            {
                if (!candidate.valid) { return; }
                render::includePoint(bounds, candidate.min);
                render::includePoint(bounds, candidate.max);
            };
            const std::size_t owner = static_cast<std::size_t>(ownerIndex);
            if (owner < thingBounds_.size()) { includeBounds(thingBounds_[owner]); }
            if (owner < spriteThingBounds_.size()) { includeBounds(spriteThingBounds_[owner]); }
            if (bounds.valid)
            {
                ThingTypeGizmoEntry& entry =
                    thingTypeGizmoEntries_[entryIndex->second];
                entry.boundsMin = bounds.min;
                entry.boundsMax = bounds.max;
                if (owner < level_->things.size())
                {
                    const Thing& thing = level_->things[owner];
                    entry.ownerPosition = QVector3D(
                        static_cast<float>(thing.x),
                        static_cast<float>(thing.y),
                        static_cast<float>(thing.z));
                }
                dirtyTypeGizmoEntries.push_back(entryIndex->second);
            }
        }
        thingTypeGizmoDirtyOwners_.clear();
    }

    const auto makeQuadVertices = [](
        const QVector3D& center,
        const QVector3D& cameraRight,
        const QVector3D& cameraUp,
        const float halfSize,
        const std::uint32_t pickId)
    {
        const QVector3D r = cameraRight * halfSize;
        const QVector3D u = cameraUp * halfSize;
        const QVector3D p0 = center - r - u;
        const QVector3D p1 = center + r - u;
        const QVector3D p2 = center + r + u;
        const QVector3D p3 = center - r + u;
        const auto makeVertex = [](const QVector3D& p,
                                   const float texU,
                                   const float texV,
                                   const std::uint32_t vertexPickId)
        {
            TriVertex vertex{};
            vertex.x = p.x();
            vertex.y = p.y();
            vertex.z = p.z();
            vertex.u = texU;
            vertex.v = texV;
            vertex.r = 1.0f;
            vertex.g = 1.0f;
            vertex.b = 1.0f;
            vertex.a = 1.0f;
            vertex.pickId = static_cast<float>(vertexPickId);
            vertex.pickGeo = 4.0f;
            return vertex;
        };
        return std::array<TriVertex, 6>{
            makeVertex(p0, 0.0f, 0.0f, pickId),
            makeVertex(p1, 1.0f, 0.0f, pickId),
            makeVertex(p2, 1.0f, 1.0f, pickId),
            makeVertex(p0, 0.0f, 0.0f, pickId),
            makeVertex(p2, 1.0f, 1.0f, pickId),
            makeVertex(p3, 0.0f, 1.0f, pickId),
        };
    };

    // A pure targeted Thing transform changes only its marker vertices. The
    // type icons stay in the first region; Arch Thing markers occupy a second
    // region so either family can be drawn and picked independently.
    if (!rebuildTypeGizmoVertices && !dirtyTypeGizmoEntries.empty())
    {
        const std::size_t expectedVertexCount =
            thingTypeGizmoIconVertexCount_
            + thingTypeGizmoArchThingMarkerVertexCount_;
        const std::size_t expectedByteCount =
            expectedVertexCount * sizeof(TriVertex);
        if (thingTypeGizmoVertices_.size() != expectedVertexCount
            || expectedByteCount
                > static_cast<std::size_t>(std::max(thingGizmoTriVboAllocatedBytes_, 0)))
        {
            thingTypeGizmoCacheValid_ = false;
        }
        else
        {
            std::sort(dirtyTypeGizmoEntries.begin(), dirtyTypeGizmoEntries.end());
            dirtyTypeGizmoEntries.erase(
                std::unique(
                    dirtyTypeGizmoEntries.begin(),
                    dirtyTypeGizmoEntries.end()),
                dirtyTypeGizmoEntries.end());

            thingGizmoTriVbo_->bind();
            for (const std::size_t entryIndex : dirtyTypeGizmoEntries)
            {
                if (entryIndex >= thingTypeGizmoEntries_.size())
                {
                    continue;
                }
                ThingTypeGizmoEntry& entry =
                    thingTypeGizmoEntries_[entryIndex];
                ThingBounds ownerBounds{};
                ownerBounds.min = entry.boundsMin;
                ownerBounds.max = entry.boundsMax;
                ownerBounds.valid = true;

                if (entry.typeFirstVertex >= 0)
                {
                    entry.center = render::thingGizmoStackDisplayPosition(
                        entry.ownerPosition,
                        &ownerBounds,
                        render::ThingGizmoStackLane::ThingType,
                        gizmoHalfSize,
                        lightStackHalfSize,
                        architecturalStackHalfSize,
                        stackLayoutForMarkers(
                            showThingGizmos_,
                            level_->kind == ProjectType::JKDF2
                                && showArchThingGizmos_ && entry.archThing,
                            entry.thingIndex));
                    const auto vertices = makeQuadVertices(
                        entry.center,
                        camRight,
                        camUp,
                        gizmoHalfSize,
                        entry.pickId);
                    const std::size_t firstVertex =
                        static_cast<std::size_t>(entry.typeFirstVertex);
                    std::copy(
                        vertices.begin(),
                        vertices.end(),
                        thingTypeGizmoVertices_.begin()
                            + static_cast<std::ptrdiff_t>(firstVertex));
                    thingGizmoTriVbo_->write(
                        static_cast<int>(firstVertex * sizeof(TriVertex)),
                        vertices.data(),
                        static_cast<int>(vertices.size() * sizeof(TriVertex)));
                }

                if (entry.archThingMarkerFirstVertex >= 0)
                {
                    entry.archThingMarkerCenter =
                        render::thingGizmoStackDisplayPosition(
                            entry.ownerPosition,
                            &ownerBounds,
                            render::ThingGizmoStackLane::ArchThing,
                            gizmoHalfSize,
                            lightStackHalfSize,
                            architecturalStackHalfSize,
                            stackLayoutForMarkers(
                                showThingGizmos_,
                                level_->kind == ProjectType::JKDF2
                                    && showArchThingGizmos_ && entry.archThing,
                                entry.thingIndex));
                    const auto vertices = makeQuadVertices(
                        entry.archThingMarkerCenter,
                        camRight,
                        camUp,
                        architecturalStackHalfSize,
                        entry.pickId);
                    const std::size_t firstVertex =
                        static_cast<std::size_t>(entry.archThingMarkerFirstVertex);
                    std::copy(
                        vertices.begin(),
                        vertices.end(),
                        thingTypeGizmoVertices_.begin()
                            + static_cast<std::ptrdiff_t>(firstVertex));
                    thingGizmoTriVbo_->write(
                        static_cast<int>(firstVertex * sizeof(TriVertex)),
                        vertices.data(),
                        static_cast<int>(vertices.size() * sizeof(TriVertex)));
                }
            }
            thingGizmoTriVbo_->release();
        }
    }

    if (rebuildTypeGizmoVertices)
    {
        // Build the type and Arch Thing marker regions from cached per-Thing
        // metadata. Camera movement does no resource/template resolution.
        thingTypeGizmoVertices_.clear();
        thingTypeGizmoBatches_.clear();
        thingTypeGizmoIconVertexCount_ = 0u;
        thingTypeGizmoArchThingMarkerVertexCount_ = 0u;
        thingTypeGizmoVertices_.reserve(thingTypeGizmoEntries_.size() * 12u);

        auto pushQuad = [&](const QVector3D& center,
                            const float halfSize,
                            const std::uint32_t pickId)
        {
            const QVector3D r = camRight * halfSize;
            const QVector3D u = camUp * halfSize;
            const QVector3D p0 = center - r - u;
            const QVector3D p1 = center + r - u;
            const QVector3D p2 = center + r + u;
            const QVector3D p3 = center - r + u;
            auto pushVertex = [&](const QVector3D& p, const float texU, const float texV)
            {
                TriVertex vertex{};
                vertex.x = p.x();
                vertex.y = p.y();
                vertex.z = p.z();
                vertex.u = texU;
                vertex.v = texV;
                vertex.r = 1.0f;
                vertex.g = 1.0f;
                vertex.b = 1.0f;
                vertex.a = 1.0f;
                vertex.pickId = static_cast<float>(pickId);
                vertex.pickGeo = 4.0f;
                thingTypeGizmoVertices_.push_back(vertex);
            };
            pushVertex(p0, 0.0f, 0.0f);
            pushVertex(p1, 1.0f, 0.0f);
            pushVertex(p2, 1.0f, 1.0f);
            pushVertex(p0, 0.0f, 0.0f);
            pushVertex(p2, 1.0f, 1.0f);
            pushVertex(p3, 0.0f, 1.0f);
        };

        const auto displayedBounds = [&](const ThingTypeGizmoEntry& entry)
        {
            ThingBounds bounds{};
            bounds.min = entry.boundsMin;
            bounds.max = entry.boundsMax;
            bounds.valid = true;
            if (cpuTransformPreview && isTransformPreviewPickId(entry.pickId))
            {
                ThingBounds transformed{};
                const QVector3D mn = bounds.min;
                const QVector3D mx = bounds.max;
                const std::array<QVector3D, 8> corners{
                    QVector3D(mn.x(), mn.y(), mn.z()),
                    QVector3D(mx.x(), mn.y(), mn.z()),
                    QVector3D(mx.x(), mx.y(), mn.z()),
                    QVector3D(mn.x(), mx.y(), mn.z()),
                    QVector3D(mn.x(), mn.y(), mx.z()),
                    QVector3D(mx.x(), mn.y(), mx.z()),
                    QVector3D(mx.x(), mx.y(), mx.z()),
                    QVector3D(mn.x(), mx.y(), mx.z()),
                };
                for (const QVector3D& corner : corners)
                {
                    render::includePoint(
                        transformed,
                        transformPreviewMatrix_.map(corner));
                }
                bounds = transformed;
            }
            return bounds;
        };
        const auto displayedOwnerPosition = [&](const ThingTypeGizmoEntry& entry)
        {
            return cpuTransformPreview && isTransformPreviewPickId(entry.pickId)
                ? transformPreviewMatrix_.map(entry.ownerPosition)
                : entry.ownerPosition;
        };

        if (showThingGizmos_)
        {
            std::string currentType;
            bool haveCurrentBatch = false;
            int batchStart = 0;
            for (ThingTypeGizmoEntry& entry : thingTypeGizmoEntries_)
            {
                if (!haveCurrentBatch || entry.type != currentType)
                {
                    if (haveCurrentBatch)
                    {
                        thingTypeGizmoBatches_.push_back(ThingTypeGizmoBatch{
                            currentType,
                            batchStart,
                            static_cast<int>(thingTypeGizmoVertices_.size())
                                - batchStart });
                    }
                    currentType = entry.type;
                    haveCurrentBatch = true;
                    batchStart = static_cast<int>(thingTypeGizmoVertices_.size());
                }

                const ThingBounds bounds = displayedBounds(entry);
                const QVector3D ownerPosition = displayedOwnerPosition(entry);
                entry.center = render::thingGizmoStackDisplayPosition(
                    ownerPosition,
                    &bounds,
                    render::ThingGizmoStackLane::ThingType,
                    gizmoHalfSize,
                    lightStackHalfSize,
                    architecturalStackHalfSize,
                    stackLayoutForMarkers(
                        true,
                        level_->kind == ProjectType::JKDF2
                            && showArchThingGizmos_ && entry.archThing,
                        entry.thingIndex));
                entry.typeFirstVertex =
                    static_cast<int>(thingTypeGizmoVertices_.size());
                pushQuad(entry.center, gizmoHalfSize, entry.pickId);
            }
            if (haveCurrentBatch)
            {
                thingTypeGizmoBatches_.push_back(ThingTypeGizmoBatch{
                    currentType,
                    batchStart,
                    static_cast<int>(thingTypeGizmoVertices_.size()) - batchStart });
            }
        }
        else
        {
            for (ThingTypeGizmoEntry& entry : thingTypeGizmoEntries_)
            {
                entry.center = {};
                entry.typeFirstVertex = -1;
            }
        }
        thingTypeGizmoIconVertexCount_ = thingTypeGizmoVertices_.size();

        for (ThingTypeGizmoEntry& entry : thingTypeGizmoEntries_)
        {
            entry.archThingMarkerCenter = {};
            entry.archThingMarkerFirstVertex = -1;
            if (!showArchThingGizmos_ || !entry.archThing
                || level_->kind != ProjectType::JKDF2)
            {
                continue;
            }
            const ThingBounds bounds = displayedBounds(entry);
            const QVector3D ownerPosition = displayedOwnerPosition(entry);
            entry.archThingMarkerCenter = render::thingGizmoStackDisplayPosition(
                ownerPosition,
                &bounds,
                render::ThingGizmoStackLane::ArchThing,
                gizmoHalfSize,
                lightStackHalfSize,
                architecturalStackHalfSize,
                stackLayoutForMarkers(
                    showThingGizmos_,
                    level_->kind == ProjectType::JKDF2
                        && showArchThingGizmos_ && entry.archThing,
                    entry.thingIndex));
            entry.archThingMarkerFirstVertex =
                static_cast<int>(thingTypeGizmoVertices_.size());
            pushQuad(
                entry.archThingMarkerCenter,
                architecturalStackHalfSize,
                entry.pickId);
        }
        thingTypeGizmoArchThingMarkerVertexCount_ =
            thingTypeGizmoVertices_.size() - thingTypeGizmoIconVertexCount_;

        if (thingTypeGizmoVertices_.empty())
        {
            thingTypeGizmoCameraRight_ = camRight;
            thingTypeGizmoCameraUp_ = camUp;
            thingTypeGizmoHalfSize_ = gizmoHalfSize;
            thingTypeGizmoLightHalfSize_ = lightStackHalfSize;
            thingTypeGizmoArchitecturalHalfSize_ = architecturalStackHalfSize;
            thingTypeGizmoCacheValid_ = !cpuTransformPreview;
            ++thingTypeGizmoBuildCount_;
            return;
        }

        thingTypeGizmoCameraRight_ = camRight;
        thingTypeGizmoCameraUp_ = camUp;
        thingTypeGizmoHalfSize_ = gizmoHalfSize;
        thingTypeGizmoLightHalfSize_ = lightStackHalfSize;
        thingTypeGizmoArchitecturalHalfSize_ = architecturalStackHalfSize;
        thingTypeGizmoCacheValid_ = !cpuTransformPreview;
        ++thingTypeGizmoBuildCount_;

        thingGizmoTriVao_->bind();
        thingGizmoTriVbo_->bind();
        const bool uploaded = uploadGlViewGizmoTriBuffer(
            *thingGizmoTriVbo_,
            thingGizmoTriVboAllocatedBytes_,
            thingTypeGizmoVertices_);
        thingGizmoTriVbo_->release();
        thingGizmoTriVao_->release();
        if (!uploaded)
        {
            thingTypeGizmoCacheValid_ = false;
            return;
        }
    }

    if (thingTypeGizmoVertices_.empty())
    {
        return;
    }

    // Move previews remain camera-facing and use the same one-matrix filtered
    // shader path for every selected owner. Rotation previews were folded into
    // cpuTransformPreview above so their billboard quads keep facing the view.
    const bool shaderMovePreview =
        (pointerMovePreview || propertyMovePreview)
        && transformPreviewIdCount_ > 0;
    const int iconPreviewCount = shaderMovePreview
        ? transformPreviewIdCount_
        : 0;
    QMatrix4x4 identityIconMatrix{};
    identityIconMatrix.setToIdentity();
    const QMatrix4x4& iconPreviewMatrix = shaderMovePreview
        ? transformPreviewMatrix_
        : identityIconMatrix;
    thingTypeGizmoPreviewMatrix_ = iconPreviewMatrix;
    thingTypeGizmoPreviewIdCount_ = iconPreviewCount;
    if (shaderMovePreview)
    {
        std::copy_n(
            transformPreviewIds_.begin(),
            static_cast<std::size_t>(transformPreviewIdCount_),
            thingTypeGizmoPreviewIds_.begin());
    }
    std::array<QVector3D, 256> iconPreviewColorMul{};
    std::fill_n(
        iconPreviewColorMul.begin(),
        static_cast<std::size_t>(std::max(iconPreviewCount, 0)),
        QVector3D(1.0f, 1.0f, 1.0f));

    // Render through the main shader with depth testing.
    program_->bind();
    program_->setUniformValue("u_mvp", mvp);
    program_->setUniformValue("u_lighting_mode", 0);
    program_->setUniformValue("u_geo_min", 0);
    program_->setUniformValue("u_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    program_->setUniformValue("u_textured", 1);
    program_->setUniformValue("u_alpha_test", 0);
    program_->setUniformValue("u_uv_scale", QVector2D(1.0f, 1.0f));
    program_->setUniformValue("u_uv_offset", QVector2D(0.0f, 0.0f));
    program_->setUniformValue("u_outline_px", 0.0f);
    program_->setUniformValue("u_flat_shade", false);
    // Cached gizmos are CPU-billboarded; shared move deltas translate their
    // anchors without rotating the quads themselves.
    program_->setUniformValue("u_frame_xform_enabled", 0);
    program_->setUniformValue("u_frame_xform_id", 0);
    program_->setUniformValue("u_frame_xform_color_mul", QVector3D(1.0f, 1.0f, 1.0f));
    program_->setUniformValue("u_frame_xform_matrix", QMatrix4x4());
    program_->setUniformValue("u_xform_enabled", iconPreviewCount > 0 ? 1 : 0);
    program_->setUniformValue("u_xform_count", iconPreviewCount);
    if (shaderMovePreview)
    {
        program_->setUniformValueArray(
            "u_xform_ids", transformPreviewIds_.data(), transformPreviewIdCount_);
        program_->setUniformValueArray(
            "u_xform_color_mul",
            iconPreviewColorMul.data(),
            transformPreviewIdCount_);
    }
    program_->setUniformValue("u_xform_matrix", iconPreviewMatrix);

    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LEQUAL);
    f->glDepthMask(GL_FALSE);
    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glDisable(GL_CULL_FACE);

    thingGizmoTriVao_->bind();
    for (const ThingTypeGizmoBatch& batch : thingTypeGizmoBatches_)
    {
        if (QOpenGLTexture* tex = getGizmoTexture(batch.type))
        {
            tex->bind(0);
        }
        f->glDrawArrays(GL_TRIANGLES, batch.firstVertex, batch.vertexCount);
    }
    if (thingTypeGizmoArchThingMarkerVertexCount_ > 0u)
    {
        if (QOpenGLTexture* tex = getArchThingGizmoTexture())
        {
            tex->bind(0);
        }
        f->glDrawArrays(
            GL_TRIANGLES,
            static_cast<int>(thingTypeGizmoIconVertexCount_),
            static_cast<int>(thingTypeGizmoArchThingMarkerVertexCount_));
    }

    thingGizmoTriVao_->release();

    f->glEnable(GL_CULL_FACE);
    f->glCullFace(GL_BACK);
    f->glDepthMask(GL_TRUE);
    f->glDepthFunc(GL_LESS);
    f->glDisable(GL_BLEND);

    program_->release();
}

void GlView::drawArchitecturalLightingOwnerGizmos(const QMatrix4x4& mvp)
{
    if (!level_ || level_->kind != ProjectType::MOTS || level_->things.empty())
    {
        return;
    }

    const bool editorArchitecturalMode = renderPipeline_ == RenderPipeline::Editor
        && architecturalLightingEditingActive_;
    const bool showOwners = editorArchitecturalMode || showArchThingGizmos_;
    if (!showOwners)
    {
        return;
    }

    auto* context = QOpenGLContext::currentContext();
    auto* f = context ? context->functions() : nullptr;
    if (!f || !program_ || !lightGizmoTriVao_ || !lightGizmoTriVbo_)
    {
        return;
    }

    QMatrix4x4 invView{};
    bool invertOk = false;
    invView = camera_.viewMatrix().inverted(&invertOk);
    if (!invertOk)
    {
        return;
    }
    QVector3D camRight(invView(0, 0), invView(1, 0), invView(2, 0));
    QVector3D camUp(invView(0, 1), invView(1, 1), invView(2, 1));
    if (camRight.lengthSquared() <= sed::math::kTinyEpsilonF
        || camUp.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return;
    }
    camRight.normalize();
    camUp.normalize();
    const float halfSize = std::clamp(lightGizmoWorldSize_, 0.001f, 1.0f) * 0.61f;
    const float thingTypeHalfSize =
        std::clamp(thingGizmoWorldSize_, 0.001f, 1.0f) * 0.5f;
    const float thingLightHalfSize =
        std::clamp(lightGizmoWorldSize_, 0.001f, 1.0f) * 0.732f;
    const bool thingLightCategoryVisible = render::lightGizmoCategoryVisible(
        render::LightGizmoCategory::Thing,
        lightGizmoCategory_,
        alwaysShowLightGizmoCategories_[
            static_cast<std::size_t>(render::LightGizmoCategory::Thing)],
        renderPipeline_ == RenderPipeline::Editor && lightGizmoEditingActive_);
    std::unordered_set<std::int32_t> visibleThingLightOwners{};
    if (thingLightCategoryVisible)
    {
        if (!thingLightGizmoEntriesCacheValid_)
        {
            thingLightGizmoEntriesCache_ =
                render::thingLightGizmoEntries(*level_, templates_);
            thingLightGizmoEntriesCacheValid_ = true;
        }
        for (const render::LightGizmoEntry& lightEntry : thingLightGizmoEntriesCache_)
        {
            if (lightEntry.kind == render::LightGizmoKind::ThingRegular
                && isLayerVisible(lightEntry.layer))
            {
                visibleThingLightOwners.insert(lightEntry.ownerIndex);
            }
        }
    }

    if (!architecturalLightingOwnerGizmoMetadataValid_)
    {
        architecturalLightingOwnerGizmoEntries_.clear();
        architecturalLightingOwnerGizmoEntryByThing_.clear();
        architecturalLightingOwnerGizmoEntries_.reserve(level_->things.size());

        for (std::size_t thingIndex = 0; thingIndex < level_->things.size(); ++thingIndex)
        {
            const Thing& thing = level_->things[thingIndex];
            if (!isLayerVisible(thing.layer)
                || !shouldShowThing(thing)
                || !sed::level::isMotsArchThing(
                    *level_, thing, templates_)
                || !std::isfinite(thing.x) || !std::isfinite(thing.y)
                || !std::isfinite(thing.z))
            {
                continue;
            }

            ThingBounds bounds{};
            const auto includeBounds = [&bounds](const ThingBounds& candidate)
            {
                if (!candidate.valid)
                {
                    return;
                }
                if (!bounds.valid)
                {
                    bounds = candidate;
                    return;
                }
                bounds.min.setX(std::min(bounds.min.x(), candidate.min.x()));
                bounds.min.setY(std::min(bounds.min.y(), candidate.min.y()));
                bounds.min.setZ(std::min(bounds.min.z(), candidate.min.z()));
                bounds.max.setX(std::max(bounds.max.x(), candidate.max.x()));
                bounds.max.setY(std::max(bounds.max.y(), candidate.max.y()));
                bounds.max.setZ(std::max(bounds.max.z(), candidate.max.z()));
            };
            if (thingIndex < thingBounds_.size())
            {
                includeBounds(thingBounds_[thingIndex]);
            }
            if (thingIndex < spriteThingBounds_.size())
            {
                includeBounds(spriteThingBounds_[thingIndex]);
            }

            if (!bounds.valid)
            {
                const ThingBox& box = thing.bbox;
                const bool zeroBox = sed::IsZero(box.x1) && sed::IsZero(box.y1)
                    && sed::IsZero(box.z1) && sed::IsZero(box.x2)
                    && sed::IsZero(box.y2) && sed::IsZero(box.z2);
                const double h = static_cast<double>(thingCubeHalfSize_);
                const TVector localMin = zeroBox
                    ? TVector{ -h, -h, -h }
                    : TVector{ box.x1, box.y1, box.z1 };
                const TVector localMax = zeroBox
                    ? TVector{ h, h, h }
                    : TVector{ box.x2, box.y2, box.z2 };
                TVector pyr{};
                pyr.pitch = thing.pch;
                pyr.yaw = thing.yaw;
                pyr.roll = thing.rol;
                const TMatrix43 xf = TMatrix43::Create(
                    TVector{ thing.x, thing.y, thing.z }, pyr);
                const std::array<TVector, 8> corners{
                    TVector{ localMin.x, localMin.y, localMin.z },
                    TVector{ localMax.x, localMin.y, localMin.z },
                    TVector{ localMax.x, localMax.y, localMin.z },
                    TVector{ localMin.x, localMax.y, localMin.z },
                    TVector{ localMin.x, localMin.y, localMax.z },
                    TVector{ localMax.x, localMin.y, localMax.z },
                    TVector{ localMax.x, localMax.y, localMax.z },
                    TVector{ localMin.x, localMax.y, localMax.z },
                };
                for (const TVector& corner : corners)
                {
                    const TVector world = xf.TransformPoint(corner);
                    const QVector3D point(
                        static_cast<float>(world.x),
                        static_cast<float>(world.y),
                        static_cast<float>(world.z));
                    if (!bounds.valid)
                    {
                        bounds.min = point;
                        bounds.max = point;
                        bounds.valid = true;
                    }
                    else
                    {
                        bounds.min.setX(std::min(bounds.min.x(), point.x()));
                        bounds.min.setY(std::min(bounds.min.y(), point.y()));
                        bounds.min.setZ(std::min(bounds.min.z(), point.z()));
                        bounds.max.setX(std::max(bounds.max.x(), point.x()));
                        bounds.max.setY(std::max(bounds.max.y(), point.y()));
                        bounds.max.setZ(std::max(bounds.max.z(), point.z()));
                    }
                }
            }
            if (!bounds.valid)
            {
                continue;
            }

            const std::uint32_t pickId = thingIndex < thingPickIds_.size()
                ? thingPickIds_[thingIndex]
                : 0u;
            const std::size_t entryIndex = architecturalLightingOwnerGizmoEntries_.size();
            architecturalLightingOwnerGizmoEntries_.push_back(
                ArchitecturalLightingOwnerGizmoEntry{
                    static_cast<std::int32_t>(thingIndex),
                    pickId,
                    {},
                    bounds.min,
                    bounds.max,
                    QVector3D(
                        static_cast<float>(thing.x),
                        static_cast<float>(thing.y),
                        static_cast<float>(thing.z)) });
            architecturalLightingOwnerGizmoEntryByThing_.emplace(
                static_cast<std::int32_t>(thingIndex), entryIndex);
        }
        architecturalLightingOwnerGizmoMetadataValid_ = true;
        architecturalLightingOwnerGizmoCacheValid_ = false;
    }

    const auto ownerIsInTransformPreview = [this](const std::uint32_t pickId)
    {
        if (!transformPreviewActive_ || pickId == 0u || transformPreviewIdCount_ <= 0)
        {
            return false;
        }
        const int ownerPickId = static_cast<int>(pickId);
        for (int i = 0; i < transformPreviewIdCount_; ++i)
        {
            if (transformPreviewIds_[static_cast<std::size_t>(i)] == ownerPickId)
            {
                return true;
            }
        }
        return false;
    };
    const bool anyOwnerTransformPreview = std::any_of(
        architecturalLightingOwnerGizmoEntries_.begin(),
        architecturalLightingOwnerGizmoEntries_.end(),
        [&ownerIsInTransformPreview](const ArchitecturalLightingOwnerGizmoEntry& entry)
        {
            return ownerIsInTransformPreview(entry.pickId);
        });
    const bool cameraChanged = std::abs(
            QVector3D::dotProduct(
                camRight, architecturalLightingOwnerGizmoCameraRight_) - 1.0f) > 1.0e-6f
        || std::abs(
            QVector3D::dotProduct(
                camUp, architecturalLightingOwnerGizmoCameraUp_) - 1.0f) > 1.0e-6f;
    const bool rebuildVertices = !architecturalLightingOwnerGizmoCacheValid_
        || cameraChanged
        || architecturalLightingOwnerGizmoTransformPreview_ != anyOwnerTransformPreview
        || anyOwnerTransformPreview
        || std::abs(halfSize - architecturalLightingOwnerGizmoHalfSize_) > 1.0e-6f
        || std::abs(thingTypeHalfSize
                    - architecturalLightingOwnerThingTypeHalfSize_) > 1.0e-6f
        || std::abs(thingLightHalfSize
                    - architecturalLightingOwnerThingLightHalfSize_) > 1.0e-6f;
    if (rebuildVertices)
    {
        architecturalLightingOwnerGizmoVertices_.clear();
        architecturalLightingOwnerGizmoVertices_.reserve(
            architecturalLightingOwnerGizmoEntries_.size() * 6u);
        const QVector3D r = camRight * halfSize;
        const QVector3D u = camUp * halfSize;
        const auto pushVertex = [this](const QVector3D& p, const float texU, const float texV,
                                       const std::uint32_t pickId)
        {
            TriVertex vertex{};
            vertex.x = p.x();
            vertex.y = p.y();
            vertex.z = p.z();
            vertex.u = texU;
            vertex.v = texV;
            vertex.r = 1.0f;
            vertex.g = 0.90f;
            vertex.b = 0.12f;
            vertex.a = 1.0f;
            vertex.pickId = static_cast<float>(pickId);
            vertex.pickGeo = 4.0f;
            architecturalLightingOwnerGizmoVertices_.push_back(vertex);
        };
        for (ArchitecturalLightingOwnerGizmoEntry& entry
             : architecturalLightingOwnerGizmoEntries_)
        {
            ThingBounds ownerBounds{};
            ownerBounds.min = entry.boundsMin;
            ownerBounds.max = entry.boundsMax;
            ownerBounds.valid = true;
            const bool ownerTransformPreview = ownerIsInTransformPreview(entry.pickId);
            const QVector3D sourceOwnerPosition = entry.ownerPosition;
            QVector3D ownerPosition = ownerTransformPreview
                ? transformPreviewMatrix_.map(sourceOwnerPosition)
                : sourceOwnerPosition;
            if (ownerTransformPreview)
            {
                const QVector3D mn = ownerBounds.min;
                const QVector3D mx = ownerBounds.max;
                const std::array<QVector3D, 8> sourceCorners{
                    QVector3D(mn.x(), mn.y(), mn.z()),
                    QVector3D(mx.x(), mn.y(), mn.z()),
                    QVector3D(mx.x(), mx.y(), mn.z()),
                    QVector3D(mn.x(), mx.y(), mn.z()),
                    QVector3D(mn.x(), mn.y(), mx.z()),
                    QVector3D(mx.x(), mn.y(), mx.z()),
                    QVector3D(mx.x(), mx.y(), mx.z()),
                    QVector3D(mn.x(), mx.y(), mx.z()),
                };
                ThingBounds transformedBounds{};
                for (const QVector3D& sourceCorner : sourceCorners)
                {
                    render::includePoint(
                        transformedBounds,
                        transformPreviewMatrix_.map(sourceCorner));
                }
                ownerBounds = transformedBounds;
            }

            render::ThingGizmoStackLayout stackLayout{};
            stackLayout.thingTypeVisible = showThingGizmos_;
            stackLayout.thingLightVisible =
                visibleThingLightOwners.contains(entry.thingIndex);
            stackLayout.archThingVisible = true;
            stackLayout.thingTypeOffset = thingGizmoHeightAboveModel_;
            stackLayout.thingLightOffset = thingLightGizmoOffsetAboveModel_;
            stackLayout.archThingOffset = archThingGizmoOffsetAboveModel_;
            stackLayout.spacing = thingGizmoStackSpacing_;
            entry.center = render::thingGizmoStackDisplayPosition(
                ownerPosition,
                &ownerBounds,
                render::ThingGizmoStackLane::ArchThing,
                thingTypeHalfSize,
                thingLightHalfSize,
                halfSize,
                stackLayout);
            const QVector3D p0 = entry.center - r - u;
            const QVector3D p1 = entry.center + r - u;
            const QVector3D p2 = entry.center + r + u;
            const QVector3D p3 = entry.center - r + u;
            pushVertex(p0, 0.0f, 0.0f, entry.pickId);
            pushVertex(p1, 1.0f, 0.0f, entry.pickId);
            pushVertex(p2, 1.0f, 1.0f, entry.pickId);
            pushVertex(p0, 0.0f, 0.0f, entry.pickId);
            pushVertex(p2, 1.0f, 1.0f, entry.pickId);
            pushVertex(p3, 0.0f, 1.0f, entry.pickId);
        }
        architecturalLightingOwnerGizmoCameraRight_ = camRight;
        architecturalLightingOwnerGizmoCameraUp_ = camUp;
        architecturalLightingOwnerGizmoHalfSize_ = halfSize;
        architecturalLightingOwnerThingTypeHalfSize_ = thingTypeHalfSize;
        architecturalLightingOwnerThingLightHalfSize_ = thingLightHalfSize;
        architecturalLightingOwnerGizmoCacheValid_ = true;
        architecturalLightingOwnerGizmoTransformPreview_ = anyOwnerTransformPreview;
        ++architecturalLightingOwnerGizmoBuildCount_;

    }

    if (architecturalLightingOwnerGizmoVertices_.empty())
    {
        return;
    }
    QOpenGLTexture* texture = getArchitecturalLightingTexture();
    if (!texture)
    {
        return;
    }

    // drawLightGizmos() uses this shared light-icon buffer earlier in the
    // frame. Re-upload the cached ARCH batch immediately before its draw so a
    // static camera and unchanged metadata cannot accidentally draw the other
    // light family's vertices. This is a bounded batch upload, not a metadata
    // rebuild or per-marker draw call.
    lightGizmoTriVao_->bind();
    lightGizmoTriVbo_->bind();
    const bool uploaded = uploadGlViewGizmoTriBuffer(
        *lightGizmoTriVbo_,
        lightGizmoTriVboAllocatedBytes_,
        architecturalLightingOwnerGizmoVertices_);
    lightGizmoTriVbo_->release();
    lightGizmoTriVao_->release();
    if (!uploaded)
    {
        architecturalLightingOwnerGizmoCacheValid_ = false;
        return;
    }

    program_->bind();
    program_->setUniformValue("u_mvp", mvp);
    program_->setUniformValue("u_lighting_mode", 1);
    program_->setUniformValue("u_geo_min", 0);
    program_->setUniformValue("u_color", QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    program_->setUniformValue("u_textured", 1);
    program_->setUniformValue("u_alpha_test", 0);
    program_->setUniformValue("u_uv_scale", QVector2D(1.0f, 1.0f));
    program_->setUniformValue("u_uv_offset", QVector2D(0.0f, 0.0f));
    program_->setUniformValue("u_outline_px", 0.0f);
    program_->setUniformValue("u_flat_shade", false);
    program_->setUniformValue("u_frame_xform_enabled", 0);
    program_->setUniformValue("u_frame_xform_id", 0);
    program_->setUniformValue("u_frame_xform_color_mul", QVector3D(1.0f, 1.0f, 1.0f));
    program_->setUniformValue("u_frame_xform_matrix", QMatrix4x4());
    // ARCH owner anchors are already rebuilt from the transiently transformed
    // owner origin and bounds. Keep shader-side Thing transforms off here so
    // the icon and its pick center are never transformed twice.
    program_->setUniformValue("u_xform_enabled", 0);
    program_->setUniformValue("u_xform_count", 0);
    program_->setUniformValue("u_xform_matrix", QMatrix4x4());

    const GLboolean depthTestWasEnabled = f->glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = f->glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = f->glIsEnabled(GL_CULL_FACE);
    GLboolean previousDepthMask = GL_TRUE;
    GLint previousDepthFunc = GL_LESS;
    GLint previousCullMode = GL_BACK;
    GLint previousBlendSrc = GL_ONE;
    GLint previousBlendDst = GL_ZERO;
    GLint previousBlendSrcAlpha = GL_ONE;
    GLint previousBlendDstAlpha = GL_ZERO;
    f->glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    f->glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
    f->glGetIntegerv(GL_CULL_FACE_MODE, &previousCullMode);
    f->glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrc);
    f->glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDst);
    f->glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    f->glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthFunc(GL_LEQUAL);
    f->glDepthMask(GL_FALSE);
    f->glEnable(GL_BLEND);
    f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    f->glDisable(GL_CULL_FACE);
    texture->bind(0);
    lightGizmoTriVao_->bind();
    f->glDrawArrays(
        GL_TRIANGLES,
        0,
        static_cast<int>(architecturalLightingOwnerGizmoVertices_.size()));
    lightGizmoTriVao_->release();
    f->glCullFace(static_cast<GLenum>(previousCullMode));
    f->glDepthMask(previousDepthMask);
    f->glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    if (cullWasEnabled) { f->glEnable(GL_CULL_FACE); }
    else { f->glDisable(GL_CULL_FACE); }
    if (depthTestWasEnabled) { f->glEnable(GL_DEPTH_TEST); }
    else { f->glDisable(GL_DEPTH_TEST); }
    f->glBlendFuncSeparate(
        static_cast<GLenum>(previousBlendSrc),
        static_cast<GLenum>(previousBlendDst),
        static_cast<GLenum>(previousBlendSrcAlpha),
        static_cast<GLenum>(previousBlendDstAlpha));
    if (blendWasEnabled) { f->glEnable(GL_BLEND); }
    else { f->glDisable(GL_BLEND); }
    program_->release();
}

std::optional<std::int32_t> GlView::hitTestSectorThrustArrow(
    const QPoint& mousePos,
    const QMatrix4x4& mvp,
    const QMatrix4x4& invMvp,
    const QVector3D& eye)
{
    if (!sectorThrustToolEnabled_ || renderPipeline_ != RenderPipeline::Editor
        || pickMode_ != PickMode::Sector || !level_ || level_->kind != ProjectType::IJIM)
    {
        return std::nullopt;
    }

    struct ScreenSegmentHit
    {
        double distanceSquared{};
        double parameter{};
    };
    auto distanceToSegmentSquared = [](const QPointF& p, const QPointF& a, const QPointF& b) -> ScreenSegmentHit
    {
        const QPointF ab = b - a;
        const double lengthSquared = ab.x() * ab.x() + ab.y() * ab.y();
        if (lengthSquared <= 1.0e-9)
        {
            const QPointF d = p - a;
            return ScreenSegmentHit{ d.x() * d.x() + d.y() * d.y(), 0.0 };
        }
        const QPointF ap = p - a;
        const double t = std::clamp((ap.x() * ab.x() + ap.y() * ab.y()) / lengthSquared, 0.0, 1.0);
        const QPointF d = p - (a + ab * t);
        return ScreenSegmentHit{ d.x() * d.x() + d.y() * d.y(), t };
    };

    std::unordered_map<std::int32_t, QVector3D> previewVectors{};
    if (sectorThrustDrag_.active)
    {
        previewVectors.reserve(sectorThrustDrag_.previews.size());
        for (const auto& [previewIndex, vector] : sectorThrustDrag_.previews)
        {
            previewVectors.emplace(previewIndex, vector);
        }
    }
    auto previewVector = [&](const std::int32_t sectorIndex, const Sector& sector) -> QVector3D
    {
        if (const auto it = previewVectors.find(sectorIndex); it != previewVectors.end())
        {
            return it->second;
        }
        return QVector3D(sector.thrust.x, sector.thrust.y, sector.thrust.z);
    };

    const QPointF mouse(mousePos);
    struct Candidate
    {
        std::int32_t sectorIndex{-1};
        double distanceSquared{};
        float depth{};
        QPointF screenPoint{};
    };
    std::vector<Candidate> candidates{};
    for (std::size_t i = 0; i < level_->sectors.size(); ++i)
    {
        const Sector& sector = level_->sectors[i];
        if (!isLayerVisible(sector.layer) || isLayerLocked(sector.layer))
        {
            continue;
        }
        const QVector3D authoredThrust(sector.thrust.x, sector.thrust.y, sector.thrust.z);
        if (!showAllSectorThrustGizmos_
            && !render::sectorThrustIsSet(sector.flags, authoredThrust, SECF_IJIM_UseThrust))
        {
            continue;
        }
        const QVector3D center = i < sectorThrustCenters_.size()
            ? sectorThrustCenters_[i]
            : [&sector]()
              {
                  const TVector value = sed::level::sectorCalcCenter(sector);
                  return QVector3D(value.x, value.y, value.z);
              }();
        const ProjectedPoint center2d = projectWorldToWidget(mvp, center, width(), height());
        constexpr double kArrowScreenMarginPx =
            static_cast<double>(render::kSectorThrustArrowMaxLengthPx) + 24.0;
        if (!center2d.valid || center2d.ndcZ < -1.0f || center2d.ndcZ > 1.0f
            || center2d.pos.x() < -kArrowScreenMarginPx
            || center2d.pos.x() > static_cast<double>(width()) + kArrowScreenMarginPx
            || center2d.pos.y() < -kArrowScreenMarginPx
            || center2d.pos.y() > static_cast<double>(height()) + kArrowScreenMarginPx)
        {
            continue;
        }

        const std::int32_t sectorIndex = static_cast<std::int32_t>(i);
        const QVector3D thrust = previewVector(sectorIndex, sector);
        const float magnitude = thrust.length();
        const QPointF centerDelta = mouse - center2d.pos;
        double candidate = centerDelta.x() * centerDelta.x() + centerDelta.y() * centerDelta.y();
        float candidateDepth = center2d.ndcZ;
        QPointF candidateScreenPoint = center2d.pos;
        auto considerSegment = [&](const ProjectedPoint& screenA, const ProjectedPoint& screenB)
        {
            if (!screenA.valid || !screenB.valid)
            {
                return;
            }
            const ScreenSegmentHit hit = distanceToSegmentSquared(mouse, screenA.pos, screenB.pos);
            if (hit.distanceSquared >= candidate)
            {
                return;
            }

            const QPointF screenPoint = screenA.pos + (screenB.pos - screenA.pos) * hit.parameter;
            const float ndcZ = static_cast<float>(
                static_cast<double>(screenA.ndcZ)
                + (static_cast<double>(screenB.ndcZ) - static_cast<double>(screenA.ndcZ)) * hit.parameter);
            candidate = hit.distanceSquared;
            candidateDepth = ndcZ;
            candidateScreenPoint = screenPoint;
        };
        if (std::isfinite(magnitude) && magnitude > 1.0e-9f)
        {
            float unitsPerPx = worldUnitsPerPixelAt(center, mvp, invMvp, width(), height());
            if (!std::isfinite(unitsPerPx) || unitsPerPx <= 0.0f)
            {
                unitsPerPx = 0.01f;
            }
            if (const auto arrow = render::sectorThrustArrowGeometry(center, thrust, eye, unitsPerPx))
            {
                const ProjectedPoint tip = projectWorldToWidget(mvp, arrow->tip, width(), height());
                const ProjectedPoint left = projectWorldToWidget(mvp, arrow->headLeft, width(), height());
                const ProjectedPoint right = projectWorldToWidget(mvp, arrow->headRight, width(), height());
                if (tip.valid)
                {
                    considerSegment(center2d, tip);
                    if (left.valid)
                    {
                        considerSegment(tip, left);
                    }
                    if (right.valid)
                    {
                        considerSegment(tip, right);
                    }
                }
            }
        }

        if (candidate <= 13.0 * 13.0)
        {
            candidates.push_back(Candidate{
                sectorIndex, candidate, candidateDepth, candidateScreenPoint });
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
        {
            if (std::abs(a.distanceSquared - b.distanceSquared) > 0.25)
            {
                return a.distanceSquared < b.distanceSquared;
            }
            return a.depth < b.depth;
        });
    std::vector<QPointF> samplePoints{};
    samplePoints.reserve(candidates.size());
    for (const Candidate& candidate : candidates)
    {
        samplePoints.push_back(candidate.screenPoint);
    }
    const std::vector<float> sceneDepths = pickSceneDepthsAtPoints(
        mvp,
        samplePoints,
        true,
        showThings_ && thingViewMode_ == ThingViewMode::Shaded);
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        const Candidate& candidate = candidates[i];
        const float candidateDepth01 = candidate.depth * 0.5f + 0.5f;
        if (i < sceneDepths.size()
            && render::lightGizmoDepthVisible(candidateDepth01, sceneDepths[i]))
        {
            return candidate.sectorIndex;
        }
    }
    return std::nullopt;
}

void GlView::drawSectorThrustGizmos(const QMatrix4x4& mvp, const QVector3D& eye)
{
    sectorThrustLabels_.clear();
    if ((!sectorThrustToolEnabled_ && !showSectorThrustVectors_)
        || renderPipeline_ != RenderPipeline::Editor || pickMode_ != PickMode::Sector
        || !level_ || level_->kind != ProjectType::IJIM
        || !transformGizmoLineVao_ || !transformGizmoLineVbo_)
    {
        return;
    }

    auto* f = QOpenGLContext::currentContext()->functions();
    if (!f)
    {
        return;
    }

    bool invertible = false;
    const QMatrix4x4 invMvp = mvp.inverted(&invertible);
    if (!invertible)
    {
        return;
    }

    std::vector<float> regularLines{};
    std::vector<float> selectedLines{};
    regularLines.reserve(std::min<std::size_t>(level_->sectors.size(), 2048U) * 18U);
    selectedLines.reserve(std::min<std::size_t>(selectedSectorThrusts_.size(), 256U) * 18U);

    struct PendingLabel
    {
        QPointF anchor{};
        float depth{};
        QString text{};
        bool selected{};
    };
    std::vector<PendingLabel> pendingLabels{};
    // Magnitude text is useful for the active handles, but painting and depth
    // sampling a label for every sector dominates large-level frame time.
    // Keep a generous cap for multi-editing while arrows remain uncapped.
    constexpr std::size_t kMaxSectorThrustLabels = 64U;
    pendingLabels.reserve(std::min<std::size_t>(
        selectedSectorThrusts_.size() + (hoveredSectorThrust_ >= 0 ? 1U : 0U),
        kMaxSectorThrustLabels));

    auto appendLine = [](std::vector<float>& dst, const QVector3D& a, const QVector3D& b)
    {
        dst.push_back(a.x()); dst.push_back(a.y()); dst.push_back(a.z());
        dst.push_back(b.x()); dst.push_back(b.y()); dst.push_back(b.z());
    };
    std::unordered_map<std::int32_t, QVector3D> previewVectors{};
    if (sectorThrustDrag_.active)
    {
        previewVectors.reserve(sectorThrustDrag_.previews.size());
        for (const auto& [previewIndex, vector] : sectorThrustDrag_.previews)
        {
            previewVectors.emplace(previewIndex, vector);
        }
    }
    auto previewVector = [&](const std::int32_t sectorIndex, const Sector& sector) -> QVector3D
    {
        if (const auto it = previewVectors.find(sectorIndex); it != previewVectors.end())
        {
            return it->second;
        }
        return QVector3D(sector.thrust.x, sector.thrust.y, sector.thrust.z);
    };

    for (std::size_t i = 0; i < level_->sectors.size(); ++i)
    {
        const Sector& sector = level_->sectors[i];
        if (!isLayerVisible(sector.layer) || isLayerLocked(sector.layer))
        {
            continue;
        }
        const std::int32_t sectorIndex = static_cast<std::int32_t>(i);
        const bool selected = selectedSectorThrusts_.contains(sectorIndex);
        const QVector3D authoredThrust(sector.thrust.x, sector.thrust.y, sector.thrust.z);
        const bool vectorIsSet = sectorThrustToolEnabled_
            ? render::sectorThrustIsSet(
                  sector.flags, authoredThrust, SECF_IJIM_UseThrust)
            : render::sectorThrustVectorIsFiniteNonzero(authoredThrust);
        if (!render::sectorThrustGizmoVisible(
                sectorThrustToolEnabled_,
                showSectorThrustVectors_,
                showAllSectorThrustGizmos_,
                vectorIsSet))
        {
            continue;
        }
        const QVector3D center = i < sectorThrustCenters_.size()
            ? sectorThrustCenters_[i]
            : [&sector]()
              {
                  const TVector value = sed::level::sectorCalcCenter(sector);
                  return QVector3D(value.x, value.y, value.z);
              }();
        if (!std::isfinite(center.x()) || !std::isfinite(center.y()) || !std::isfinite(center.z()))
        {
            continue;
        }

        const ProjectedPoint projectedCenter = projectWorldToWidget(mvp, center, width(), height());
        constexpr double kArrowScreenMarginPx =
            static_cast<double>(render::kSectorThrustArrowMaxLengthPx) + 24.0;
        if (!projectedCenter.valid || projectedCenter.ndcZ < -1.0f || projectedCenter.ndcZ > 1.0f
            || projectedCenter.pos.x() < -kArrowScreenMarginPx
            || projectedCenter.pos.x() > static_cast<double>(width()) + kArrowScreenMarginPx
            || projectedCenter.pos.y() < -kArrowScreenMarginPx
            || projectedCenter.pos.y() > static_cast<double>(height()) + kArrowScreenMarginPx)
        {
            continue;
        }

        float unitsPerPx = worldUnitsPerPixelAt(center, mvp, invMvp, width(), height());
        if (!std::isfinite(unitsPerPx) || unitsPerPx <= 0.0f)
        {
            unitsPerPx = 0.01f;
        }

        std::vector<float>& lines = selected ? selectedLines : regularLines;
        const QVector3D thrust = previewVector(sectorIndex, sector);
        const bool hasFiniteNonzeroThrust =
            render::sectorThrustVectorIsFiniteNonzero(thrust);
        const float magnitude = thrust.length();
        const bool drawArrow = sectorThrustToolEnabled_
            ? std::isfinite(magnitude) && magnitude > 1.0e-9f
            : hasFiniteNonzeroThrust;
        QVector3D labelPoint = center;
        if (!drawArrow)
        {
            // A small screen-scale center handle keeps zero-thrust sectors
            // directly editable without inventing a stored direction.
            const float radius = unitsPerPx * (selected ? 11.0f : 8.0f);
            appendLine(lines, center - QVector3D(radius, 0.0f, 0.0f), center + QVector3D(radius, 0.0f, 0.0f));
            appendLine(lines, center - QVector3D(0.0f, radius, 0.0f), center + QVector3D(0.0f, radius, 0.0f));
            appendLine(lines, center - QVector3D(0.0f, 0.0f, radius), center + QVector3D(0.0f, 0.0f, radius));
        }
        else if (const auto arrow = render::sectorThrustArrowGeometry(center, thrust, eye, unitsPerPx))
        {
            appendLine(lines, center, arrow->tip);
            appendLine(lines, arrow->tip, arrow->headLeft);
            appendLine(lines, arrow->tip, arrow->headRight);
            labelPoint = arrow->tip;
        }

        const bool wantsLabel = sectorThrustToolEnabled_
            && (selected || sectorIndex == hoveredSectorThrust_);
        if (wantsLabel && pendingLabels.size() < kMaxSectorThrustLabels)
        {
            const ProjectedPoint projected = projectWorldToWidget(mvp, labelPoint, width(), height());
            if (projected.valid && projected.ndcZ >= -1.0f && projected.ndcZ <= 1.0f
                && projected.pos.x() >= 0.0 && projected.pos.x() < static_cast<double>(width())
                && projected.pos.y() >= 0.0 && projected.pos.y() < static_cast<double>(height()))
            {
                pendingLabels.push_back(PendingLabel{
                    projected.pos,
                    projected.ndcZ * 0.5f + 0.5f,
                    QStringLiteral("|T| %1").arg(ui::formatUiNumber(
                        std::isfinite(magnitude) ? magnitude : 0.0f, 'g', 5)),
                    selected });
            }
        }
    }

    if (!pendingLabels.empty())
    {
        std::vector<QPointF> labelSamplePoints{};
        labelSamplePoints.reserve(pendingLabels.size());
        for (const PendingLabel& label : pendingLabels)
        {
            labelSamplePoints.push_back(label.anchor);
        }
        const std::vector<float> sceneDepths = pickSceneDepthsAtPoints(
            mvp,
            labelSamplePoints,
            true,
            showThings_ && thingViewMode_ == ThingViewMode::Shaded);
        for (std::size_t i = 0; i < pendingLabels.size() && i < sceneDepths.size(); ++i)
        {
            const PendingLabel& label = pendingLabels[i];
            if (!render::lightGizmoDepthVisible(label.depth, sceneDepths[i]))
            {
                continue;
            }
            sectorThrustLabels_.push_back(SectorThrustLabel{
                label.anchor + QPointF(10.0, -23.0), label.text, label.selected });
        }
    }

    const GLboolean depthTestWasEnabled = f->glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = f->glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = f->glIsEnabled(GL_CULL_FACE);
    GLboolean previousDepthMask = GL_TRUE;
    GLint previousDepthFunc = GL_LESS;
    GLint previousCullMode = GL_BACK;
    GLint previousBlendSrcRgb = GL_ONE;
    GLint previousBlendDstRgb = GL_ZERO;
    GLint previousBlendSrcAlpha = GL_ONE;
    GLint previousBlendDstAlpha = GL_ZERO;
    GLfloat previousLineWidth = 1.0f;
    GLfloat previousAttrib2[4]{};
    GLfloat previousAttrib3[4]{};
    f->glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    f->glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
    f->glGetIntegerv(GL_CULL_FACE_MODE, &previousCullMode);
    f->glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRgb);
    f->glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRgb);
    f->glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
    f->glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);
    f->glGetFloatv(GL_LINE_WIDTH, &previousLineWidth);
    f->glGetVertexAttribfv(2, GL_CURRENT_VERTEX_ATTRIB, previousAttrib2);
    f->glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, previousAttrib3);

    auto drawLineSet = [&](const std::vector<float>& verts,
                           const QVector4D& outlineColor,
                           const float outlineWidthPx,
                           const QVector4D& fillColor,
                           const float fillWidthPx)
    {
        if (verts.empty())
        {
            return;
        }
        QOpenGLShaderProgram* lineProgram = thickLineProgram_ ? thickLineProgram_.get() : program_.get();
        if (!lineProgram)
        {
            return;
        }

        // Thrust is a scene handle, not an X-ray annotation. Retaining depth
        // testing is what prevents arrows in other sectors leaking through
        // solid level walls.
        f->glEnable(GL_DEPTH_TEST);
        f->glDepthFunc(GL_LEQUAL);
        f->glDepthMask(GL_FALSE);
        f->glEnable(GL_BLEND);
        f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        f->glDisable(GL_CULL_FACE);

        lineProgram->bind();
        lineProgram->setUniformValue("u_mvp", mvp);
        lineProgram->setUniformValue("u_frame_xform_enabled", 0);
        lineProgram->setUniformValue("u_frame_xform_id", 0);
        lineProgram->setUniformValue("u_frame_xform_matrix", QMatrix4x4());
        lineProgram->setUniformValue("u_xform_enabled", 0);
        lineProgram->setUniformValue("u_xform_count", 0);
        lineProgram->setUniformValue("u_xform_matrix", QMatrix4x4());
        lineProgram->setUniformValue("u_ndc_z_bias", 0.0f);
        if (thickLineProgram_)
        {
            lineProgram->setUniformValue(
                "u_viewport_size", QVector2D(static_cast<float>(width()), static_cast<float>(height())));
            lineProgram->setUniformValue("u_surface_depth_match", 0);
        }
        else
        {
            lineProgram->setUniformValue("u_textured", 0);
            lineProgram->setUniformValue("u_alpha_test", 0);
            lineProgram->setUniformValue("u_lighting_mode", 0);
            lineProgram->setUniformValue("u_geo_min", 0);
            lineProgram->setUniformValue("u_outline_px", 0.0f);
            lineProgram->setUniformValue("u_flat_shade", false);
            f->glVertexAttrib4f(2, 1.0f, 1.0f, 1.0f, 1.0f);
        }
        f->glVertexAttrib1f(3, 0.0f);

        transformGizmoLineVao_->bind();
        transformGizmoLineVbo_->bind();
        if (uploadGlViewGizmoFloatBuffer(*transformGizmoLineVbo_, transformGizmoLineVboAllocatedBytes_, verts))
        {
            const int vertexCount = static_cast<int>(verts.size() / 3);
            auto drawPass = [&](const QVector4D& color, const float widthPx)
            {
                lineProgram->setUniformValue("u_color", color);
                if (thickLineProgram_)
                {
                    lineProgram->setUniformValue("u_line_width_px", widthPx);
                }
                else
                {
                    f->glLineWidth(std::max(1.0f, widthPx));
                }
                f->glDrawArrays(GL_LINES, 0, vertexCount);
            };
            // Upload each line set once, then draw its dark silhouette and
            // bright core from the same buffer. The former four uploads per
            // frame were visible as stalls on large sector counts.
            drawPass(outlineColor, outlineWidthPx);
            drawPass(fillColor, fillWidthPx);
        }
        transformGizmoLineVbo_->release();
        transformGizmoLineVao_->release();
        lineProgram->release();
    };

    drawLineSet(
        regularLines,
        QVector4D(0.06f, 0.01f, 0.01f, 0.88f),
        7.5f,
        QVector4D(0.89f, 0.10f, 0.08f, 0.96f),
        4.8f);
    drawLineSet(
        selectedLines,
        QVector4D(0.06f, 0.01f, 0.01f, 0.96f),
        9.5f,
        QVector4D(1.00f, 0.25f, 0.18f, 1.00f),
        6.4f);

    f->glLineWidth(previousLineWidth);
    f->glVertexAttrib4fv(2, previousAttrib2);
    f->glVertexAttrib4fv(3, previousAttrib3);
    f->glCullFace(static_cast<GLenum>(previousCullMode));
    if (cullWasEnabled) { f->glEnable(GL_CULL_FACE); }
    else { f->glDisable(GL_CULL_FACE); }
    f->glDepthMask(previousDepthMask);
    f->glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    if (depthTestWasEnabled) { f->glEnable(GL_DEPTH_TEST); }
    else { f->glDisable(GL_DEPTH_TEST); }
    f->glBlendFuncSeparate(
        static_cast<GLenum>(previousBlendSrcRgb),
        static_cast<GLenum>(previousBlendDstRgb),
        static_cast<GLenum>(previousBlendSrcAlpha),
        static_cast<GLenum>(previousBlendDstAlpha));
    if (blendWasEnabled) { f->glEnable(GL_BLEND); }
    else { f->glDisable(GL_BLEND); }
}

void GlView::drawTransformGizmo(const QMatrix4x4& proj, const QMatrix4x4& view, const QVector3D& eye, const QVector3D& target)
{
    if (renderPipeline_ != RenderPipeline::Editor || transformTool_ == TransformTool::Select)
    {
        return;
    }

    if (!level_ || !transformGizmoLineVao_ || !transformGizmoLineVbo_)
    {
        return;
    }

    auto* f = QOpenGLContext::currentContext()->functions();
    if (!f)
    {
        return;
    }

    TransformTargetKind targetKind = TransformTargetKind::Thing;
    QVector3D pivot{};

    QVector3D basisX(1.0f, 0.0f, 0.0f);
    QVector3D basisY(0.0f, 1.0f, 0.0f);
    QVector3D basisZ(0.0f, 0.0f, 1.0f);

    auto computeGridLocalBasis = [&](QVector3D& outX, QVector3D& outY, QVector3D& outZ) -> bool
    {
        if (gridAlignToView_)
        {
            outX = gridAlignAxisA_;
            outY = gridAlignAxisB_;
        }
        else if (gridRotateWithView_)
        {
            bool invViewOk = false;
            const QMatrix4x4 invView = view.inverted(&invViewOk);
            if (!invViewOk)
            {
                return false;
            }

            const QVector4D r4 = invView.column(0);
            const QVector4D u4 = invView.column(1);
            outX = QVector3D(r4.x(), r4.y(), r4.z());
            outY = QVector3D(u4.x(), u4.y(), u4.z());
        }
        else
        {
            switch (gridPlane_)
            {
                case GridPlane::XY:
                    outX = QVector3D(1.0f, 0.0f, 0.0f);
                    outY = QVector3D(0.0f, 1.0f, 0.0f);
                    break;
                case GridPlane::XZ:
                    outX = QVector3D(1.0f, 0.0f, 0.0f);
                    outY = QVector3D(0.0f, 0.0f, 1.0f);
                    break;
                case GridPlane::YZ:
                    outX = QVector3D(0.0f, 1.0f, 0.0f);
                    outY = QVector3D(0.0f, 0.0f, 1.0f);
                    break;
            }
        }

        outZ = QVector3D::crossProduct(outX, outY);
        sed::render::normalizeBasis(outX, outY, outZ);
        return sed::render::basisIsUsable(outX, outY, outZ);
    };

    auto computeSectorLocalBasis = [&](QVector3D& outX, QVector3D& outY, QVector3D& outZ) -> bool
    {
        auto sectorSelected = [this](const std::int32_t sectorIndex) -> bool
        {
            return sectorIndex >= 0
                && (sectorIndex == selectedSector_ || highlightedSelection_.sectors.contains(sectorIndex));
        };

        if (selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0 && sectorSelected(selectedSurfaceSector_))
        {
            if (sed::render::computeSurfaceLocalBasis(*level_, SurfaceRef{ selectedSurfaceSector_, selectedSurface_ }, outX, outY, outZ))
            {
                return true;
            }
        }

        if (haveLastSurfaceHit_ && sectorSelected(lastSurfaceHitSurface_.sector))
        {
            if (sed::render::computeSurfaceLocalBasis(*level_, lastSurfaceHitSurface_, outX, outY, outZ))
            {
                return true;
            }
        }

        std::int32_t primarySector = selectedSector_;
        if (primarySector < 0 && !highlightedSelection_.sectors.empty())
        {
            primarySector = *highlightedSelection_.sectors.begin();
        }

        if (primarySector >= 0 && static_cast<std::size_t>(primarySector) < level_->sectors.size())
        {
            const Sector& sector = level_->sectors[static_cast<std::size_t>(primarySector)];
            for (std::int32_t surfaceIndex = 0; surfaceIndex < static_cast<std::int32_t>(sector.surfaces.size()); ++surfaceIndex)
            {
                if (sed::render::computeSurfaceLocalBasis(*level_, SurfaceRef{ primarySector, surfaceIndex }, outX, outY, outZ))
                {
                    return true;
                }
            }
        }

        return computeGridLocalBasis(outX, outY, outZ);
    };

    auto resolveTarget = [&]() -> bool
    {
        auto resolveFrame = [&]() -> bool
        {
            if (!showThings_)
            {
                return false;
            }

            // Frames are position/orientation points; scaling isn't meaningful here.
            if (transformTool_ == TransformTool::Scale)
            {
                return false;
            }

            const std::int32_t activeThing = selectedThing_;
            if (activeThing < 0 || static_cast<std::size_t>(activeThing) >= level_->things.size())
            {
                return false;
            }

            const Thing& thing = level_->things[static_cast<std::size_t>(activeThing)];
            if (!isLayerVisible(thing.layer) || !shouldShowThing(thing))
            {
                return false;
            }
            if (selectedFrameValueIndex_ < 0 || static_cast<std::size_t>(selectedFrameValueIndex_) >= thing.values.size())
            {
                return false;
            }

            const TplValue& fv = thing.values[static_cast<std::size_t>(selectedFrameValueIndex_)];
            if (fv.type != ValueType::frame || !sed::text::iequalsAscii(fv.name, "frame"))
            {
                return false;
            }

            const render::frame_ghost::FramePose* pose =
                findFramePose(activeThing, selectedFrameValueIndex_);
            if (pose == nullptr || !pose->finiteOrientation)
            {
                return false;
            }

            const double x = pose->x;
            const double y = pose->y;
            const double z = pose->z;
            const double pch = pose->pitch;
            const double yaw = pose->yaw;
            const double rol = pose->roll;

            targetKind = TransformTargetKind::Frame;
            pivot = QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));

            if (transformOrientation_ == TransformOrientation::Local)
            {
                TVector pyr{};
                pyr.pitch = pch;
                pyr.yaw = yaw;
                pyr.roll = rol;
                const TMatrix43 rot = TMatrix43::CreateRotate(pyr);
                basisX = QVector3D(static_cast<float>(rot.rvec.x), static_cast<float>(rot.rvec.y), static_cast<float>(rot.rvec.z));
                basisY = QVector3D(static_cast<float>(rot.lvec.x), static_cast<float>(rot.lvec.y), static_cast<float>(rot.lvec.z));
                basisZ = QVector3D(static_cast<float>(rot.uvec.x), static_cast<float>(rot.uvec.y), static_cast<float>(rot.uvec.z));
                sed::render::normalizeBasis(basisX, basisY, basisZ);
            }

            return true;
        };

        auto resolveLight = [&]() -> bool
        {
            std::int32_t activeLight = selectedLight_;
            if (activeLight < 0 && !highlightedSelection_.lights.empty())
            {
                activeLight = *highlightedSelection_.lights.begin();
            }
            if (activeLight < 0 || static_cast<std::size_t>(activeLight) >= level_->lights.size())
            {
                return false;
            }

            const SedLight& light = level_->lights[static_cast<std::size_t>(activeLight)];
            if (!isLayerVisible(light.layer))
            {
                return false;
            }
            if (!std::isfinite(light.position.x) || !std::isfinite(light.position.y) || !std::isfinite(light.position.z))
            {
                return false;
            }

            targetKind = TransformTargetKind::Light;
            pivot = QVector3D(
                static_cast<float>(light.position.x),
                static_cast<float>(light.position.y),
                static_cast<float>(light.position.z));
            return true;
        };

        auto resolveSectorLight = [&]() -> bool
        {
            if (transformTool_ == TransformTool::Rotate)
            {
                return false;
            }
            std::int32_t owner = selectedSector_;
            const std::vector<render::LightGizmoEntry> entries = render::sectorLightGizmoEntries(*level_);
            auto entry = std::find_if(entries.begin(), entries.end(), [owner](const render::LightGizmoEntry& candidate)
            {
                return candidate.ownerIndex == owner;
            });
            if (entry == entries.end() || !isLayerVisible(entry->layer))
            {
                // A stale/mixed sector selection can retain a non-emitter as
                // the primary item. Fall back deterministically to the first
                // selected sector that actually owns an editable point light.
                entry = std::find_if(entries.begin(), entries.end(), [this](const render::LightGizmoEntry& candidate)
                {
                    return highlightedSelection_.sectors.contains(candidate.ownerIndex)
                        && isLayerVisible(candidate.layer);
                });
            }
            if (entry == entries.end() || !isLayerVisible(entry->layer))
            {
                return false;
            }
            targetKind = TransformTargetKind::SectorLight;
            pivot = entry->position;
            return true;
        };

        auto resolveThingLight = [&]() -> bool
        {
            if (transformTool_ == TransformTool::Rotate)
            {
                return false;
            }
            std::int32_t owner = selectedThing_;
            if (owner < 0 && !highlightedSelection_.things.empty())
            {
                owner = *highlightedSelection_.things.begin();
            }
            const std::vector<render::LightGizmoEntry> entries =
                render::thingLightGizmoEntries(*level_, templates_);
            auto entry = std::find_if(entries.begin(), entries.end(), [&](const render::LightGizmoEntry& candidate)
            {
                return candidate.ownerIndex == owner
                    && candidate.kind == selectedLightGizmoKind_;
            });
            if (entry == entries.end())
            {
                entry = std::find_if(entries.begin(), entries.end(), [owner](const render::LightGizmoEntry& candidate)
                {
                    return candidate.ownerIndex == owner;
                });
            }
            if (entry == entries.end() || !isLayerVisible(entry->layer))
            {
                return false;
            }

            selectedLightGizmoKind_ = entry->kind;
            targetKind = TransformTargetKind::ThingLight;
            pivot = render::thingLightTransformPivot(
                *entry,
                lightGizmoDisplayPositionForEntry(*entry, view));

            if (transformOrientation_ == TransformOrientation::Local
                && owner >= 0
                && static_cast<std::size_t>(owner) < level_->things.size())
            {
                const Thing& thing = level_->things[static_cast<std::size_t>(owner)];
                TVector pyr{};
                pyr.pitch = thing.pch;
                pyr.yaw = thing.yaw;
                pyr.roll = thing.rol;
                const TMatrix43 rot = TMatrix43::CreateRotate(pyr);
                basisX = QVector3D(static_cast<float>(rot.rvec.x), static_cast<float>(rot.rvec.y), static_cast<float>(rot.rvec.z));
                basisY = QVector3D(static_cast<float>(rot.lvec.x), static_cast<float>(rot.lvec.y), static_cast<float>(rot.lvec.z));
                basisZ = QVector3D(static_cast<float>(rot.uvec.x), static_cast<float>(rot.uvec.y), static_cast<float>(rot.uvec.z));
                sed::render::normalizeBasis(basisX, basisY, basisZ);
            }
            return true;
        };

        auto resolveThing = [&]() -> bool
        {
            const bool thingLightOwner =
                pickMode_ == PickMode::Thing
                && lightGizmoCategory_ == render::LightGizmoCategory::Thing;
            if (!showThings_ && !thingLightOwner)
            {
                return false;
            }

            std::int32_t activeThing = thingPropertyTransformPreviewActive_
                ? thingPropertyTransformPreviewThing_
                : selectedThing_;
            if (activeThing < 0 && !highlightedSelection_.things.empty())
            {
                activeThing = *highlightedSelection_.things.begin();
            }
            if (activeThing < 0 || static_cast<std::size_t>(activeThing) >= level_->things.size())
            {
                return false;
            }

            const Thing& thing = level_->things[static_cast<std::size_t>(activeThing)];
            if (!isLayerVisible(thing.layer) || !shouldShowThing(thing))
            {
                return false;
            }
            if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
            {
                return false;
            }
            if (!std::isfinite(thing.pch) || !std::isfinite(thing.yaw) || !std::isfinite(thing.rol))
            {
                return false;
            }

            targetKind = TransformTargetKind::Thing;
            pivot = QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));

            if (transformOrientation_ == TransformOrientation::Local)
            {
                TVector pyr{};
                pyr.pitch = thing.pch;
                pyr.yaw = thing.yaw;
                pyr.roll = thing.rol;
                const TMatrix43 rot = TMatrix43::CreateRotate(pyr);
                basisX = QVector3D(static_cast<float>(rot.rvec.x), static_cast<float>(rot.rvec.y), static_cast<float>(rot.rvec.z));
                basisY = QVector3D(static_cast<float>(rot.lvec.x), static_cast<float>(rot.lvec.y), static_cast<float>(rot.lvec.z));
                basisZ = QVector3D(static_cast<float>(rot.uvec.x), static_cast<float>(rot.uvec.y), static_cast<float>(rot.uvec.z));
                sed::render::normalizeBasis(basisX, basisY, basisZ);
            }
            return true;
        };

        auto resolveSector = [&]() -> bool
        {
            QVector3D mn{};
            QVector3D mx{};
            bool have = false;

            auto addSector = [&](const std::int32_t sectorIndex)
            {
                if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level_->sectors.size())
                {
                    return;
                }
                const Sector& sector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
                for (const Vertex& v : sector.vertices)
                {
                    sed::render::addPointToBounds(mn, mx, have, static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
                }
            };

            for (const std::int32_t s : highlightedSelection_.sectors)
            {
                addSector(s);
            }
            addSector(selectedSector_);

            if (!have)
            {
                return false;
            }

            targetKind = TransformTargetKind::Sector;
            pivot = (mn + mx) * 0.5f;

            if (transformOrientation_ == TransformOrientation::Local)
            {
                QVector3D sx, sy, sz;
                if (computeSectorLocalBasis(sx, sy, sz))
                {
                    basisX = sx;
                    basisY = sy;
                    basisZ = sz;
                    sed::render::normalizeBasis(basisX, basisY, basisZ);
                }
            }

            return true;
        };

        auto resolveSurface = [&]() -> bool
        {
            QVector3D mn{};
            QVector3D mx{};
            bool have = false;

            SurfaceRef activeRef{};
            bool haveActiveRef = false;
            std::vector<SurfaceRef> selectionSurfaces{};
            selectionSurfaces.reserve(
                1 + highlightedSelection_.surfaceVertices.size()
                + highlightedSelection_.surfaces.size());

            auto addSurface = [&](const SurfaceRef ref)
            {
                if (ref.sector < 0 || ref.surface < 0)
                {
                    return;
                }
                const std::size_t secIndex = static_cast<std::size_t>(ref.sector);
                if (secIndex >= level_->sectors.size())
                {
                    return;
                }
                const Sector& sector = level_->sectors[secIndex];
                const std::size_t surfIndex = static_cast<std::size_t>(ref.surface);
                if (surfIndex >= sector.surfaces.size())
                {
                    return;
                }

                const Surface& surface = sector.surfaces[surfIndex];
                selectionSurfaces.push_back(ref);
                for (const std::int32_t vi : surface.vertices)
                {
                    if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                    {
                        continue;
                    }
                    const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
                    sed::render::addPointToBounds(mn, mx, have, static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
                }

                if (!haveActiveRef)
                {
                    activeRef = ref;
                    haveActiveRef = true;
                }
            };

            if (selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0)
            {
                addSurface(SurfaceRef{ selectedSurfaceSector_, selectedSurface_ });
            }
            for (const editor::SurfaceVertexRef& ref : highlightedSelection_.surfaceVertices)
            {
                addSurface(ref.surface);
            }
            for (const SurfaceRef ref : highlightedSelection_.surfaces)
            {
                addSurface(ref);
            }

            if (!have)
            {
                return false;
            }

            targetKind = TransformTargetKind::Surface;
            const auto pivotMode = surfaceSubMode_ == SurfaceSubMode::Vertex
                ? render::SurfaceSelectionPivotMode::Vertex
                : surfaceSubMode_ == SurfaceSubMode::Edge
                    ? render::SurfaceSelectionPivotMode::Edge
                    : render::SurfaceSelectionPivotMode::Face;
            std::optional<SurfaceRef> activeSurface = haveActiveRef
                ? std::optional<SurfaceRef>{activeRef}
                : std::nullopt;
            std::optional<SurfaceRef> hitSurface = haveLastSurfaceHit_
                ? std::optional<SurfaceRef>{lastSurfaceHitSurface_}
                : std::nullopt;
            std::optional<QVector3D> hitPosition = haveLastSurfaceHit_
                ? std::optional<QVector3D>{QVector3D(
                    static_cast<float>(lastSurfaceHitPos_.x),
                    static_cast<float>(lastSurfaceHitPos_.y),
                    static_cast<float>(lastSurfaceHitPos_.z))}
                : std::nullopt;
            // Exact sub-element identity is stable across a move; the old
            // world-space click is not. Resolve the active ref against the
            // current Level so the gizmo follows the moved Vertex/Edge.
            if (surfaceSubMode_ == SurfaceSubMode::Vertex
                && activeSurfaceVertexHintValid_
                && highlightedSelection_.surfaceVertices.contains(
                    activeSurfaceVertexHint_))
            {
                const editor::SurfaceVertexRef& vertexRef =
                    activeSurfaceVertexHint_;
                if (vertexRef.surface.sector >= 0
                    && vertexRef.surface.surface >= 0
                    && vertexRef.localVertex >= 0
                    && static_cast<std::size_t>(vertexRef.surface.sector)
                        < level_->sectors.size())
                {
                    const Sector& sector = level_->sectors[
                        static_cast<std::size_t>(vertexRef.surface.sector)];
                    if (static_cast<std::size_t>(vertexRef.surface.surface)
                            < sector.surfaces.size())
                    {
                        const Surface& surface = sector.surfaces[
                            static_cast<std::size_t>(vertexRef.surface.surface)];
                        if (static_cast<std::size_t>(vertexRef.localVertex)
                                < surface.vertices.size())
                        {
                            const std::int32_t vertexIndex = surface.vertices[
                                static_cast<std::size_t>(vertexRef.localVertex)];
                            if (vertexIndex >= 0
                                && static_cast<std::size_t>(vertexIndex)
                                    < sector.vertices.size())
                            {
                                const Vertex& vertex = sector.vertices[
                                    static_cast<std::size_t>(vertexIndex)];
                                activeSurface = vertexRef.surface;
                                hitSurface = vertexRef.surface;
                                hitPosition = QVector3D(
                                    static_cast<float>(vertex.x),
                                    static_cast<float>(vertex.y),
                                    static_cast<float>(vertex.z));
                            }
                        }
                    }
                }
            }
            else if (surfaceSubMode_ == SurfaceSubMode::Edge
                && activeSurfaceEdge_
                && selectedSurfaceEdges_.contains(*activeSurfaceEdge_))
            {
                if (const std::optional<TVector> midpoint =
                        surfaceEdgeMidpoint(*activeSurfaceEdge_))
                {
                    activeSurface = activeSurfaceEdge_->surface;
                    hitSurface = activeSurfaceEdge_->surface;
                    hitPosition = QVector3D(
                        static_cast<float>(midpoint->x),
                        static_cast<float>(midpoint->y),
                        static_cast<float>(midpoint->z));
                }
            }
            const std::optional<QVector3D> resolvedPivot = render::surfaceSelectionPivot(
                *level_, selectionSurfaces, activeSurface, pivotMode, hitSurface, hitPosition);
            if (!resolvedPivot)
            {
                return false;
            }
            pivot = *resolvedPivot;

            if (transformOrientation_ == TransformOrientation::Local
                && activeSurface)
            {
                QVector3D sx, sy, sz;
                if (sed::render::computeSurfaceLocalBasis(
                        *level_, *activeSurface, sx, sy, sz))
                {
                    basisX = sx;
                    basisY = sy;
                    basisZ = sz;
                    sed::render::normalizeBasis(basisX, basisY, basisZ);
                }
            }

            return true;
        };

        if (frameEditEnabled_)
        {
            return resolveFrame();
        }

        switch (pickMode_)
        {
            case PickMode::Sector:
                return lightGizmoEditingActive_
                        && lightGizmoCategory_ == render::LightGizmoCategory::Sector
                    ? resolveSectorLight()
                    : resolveSector();
            case PickMode::Surface:
                return resolveSurface();
            case PickMode::Thing:
                return lightGizmoEditingActive_
                        && lightGizmoCategory_ == render::LightGizmoCategory::Thing
                    ? resolveThingLight()
                    : resolveThing();
            case PickMode::Light:
                return resolveLight();
            default:
                break;
        }

        if (resolveLight())
        {
            return true;
        }
        if (resolveThing())
        {
            return true;
        }
        if (resolveSurface())
        {
            return true;
        }
        return resolveSector();
    };

    if (!resolveTarget())
    {
        return;
    }

    QVector3D drawPivot = pivot;
    if (thingPropertyTransformPreviewActive_
        && targetKind == TransformTargetKind::Thing)
    {
        const QVector3D mappedPivot = transformPreviewMatrix_.map(pivot);
        if (transformOrientation_ == TransformOrientation::Local)
        {
            auto mapBasis = [&](const QVector3D& basis) -> QVector3D
            {
                QVector3D mapped = transformPreviewMatrix_.map(pivot + basis)
                    - mappedPivot;
                if (mapped.lengthSquared() > sed::math::kTinyEpsilonF)
                {
                    mapped.normalize();
                }
                return mapped;
            };
            basisX = mapBasis(basisX);
            basisY = mapBasis(basisY);
            basisZ = mapBasis(basisZ);
            sed::render::normalizeBasis(basisX, basisY, basisZ);
        }
        drawPivot = mappedPivot;
    }
    else if (transformTool_ == TransformTool::Move)
    {
        drawPivot += transformDrag_.translation;
    }

    const QMatrix4x4 mvp = proj * view;
    bool invertible = false;
    const QMatrix4x4 invMvp = mvp.inverted(&invertible);
    if (!invertible)
    {
        return;
    }

    float unitsPerPx = worldUnitsPerPixelAt(drawPivot, mvp, invMvp, width(), height());
    if (!std::isfinite(unitsPerPx) || unitsPerPx <= 0.0f)
    {
        unitsPerPx = 0.01f;
    }

    QMatrix4x4 invView = view.inverted(&invertible);
    if (!invertible)
    {
        return;
    }

    QVector3D camRight(invView(0, 0), invView(1, 0), invView(2, 0));
    QVector3D camUp(invView(0, 1), invView(1, 1), invView(2, 1));
    QVector3D camForward = target - eye;
    if (camRight.lengthSquared() > sed::math::kTinyEpsilonF) { camRight.normalize(); }
    if (camUp.lengthSquared() > sed::math::kTinyEpsilonF) { camUp.normalize(); }
    if (camForward.lengthSquared() > sed::math::kTinyEpsilonF) { camForward.normalize(); } else { camForward = QVector3D(0.0f, 0.0f, -1.0f); }

    constexpr float kBaseWidthPx = 2.4f;
    constexpr float kHighlightWidthPx = 3.9f;
    const float axisLenW = unitsPerPx * 127.0f;
    const float headLenW = unitsPerPx * 18.0f;
    const float headW = unitsPerPx * 10.5f;
    const float centerRadiusW = unitsPerPx * 18.0f;
    const float ringRadiusW = unitsPerPx * 104.0f;
    const float viewRingRadiusW = unitsPerPx * 127.0f;
    constexpr int kRingSegs = 64;

    auto appendLine = [](std::vector<float>& dst, const QVector3D& a, const QVector3D& b)
    {
        dst.push_back(a.x()); dst.push_back(a.y()); dst.push_back(a.z());
        dst.push_back(b.x()); dst.push_back(b.y()); dst.push_back(b.z());
    };

    auto appendCircle = [&](std::vector<float>& dst, const QVector3D& c, QVector3D u, QVector3D v, const float r, const int segs)
    {
        if (u.lengthSquared() > sed::math::kTinyEpsilonF) { u.normalize(); }
        if (v.lengthSquared() > sed::math::kTinyEpsilonF) { v.normalize(); }
        QVector3D prev = c + u * r;
        for (int i = 1; i <= segs; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(segs);
            const float a = t * 2.0f * std::numbers::pi_v<float>;
            const float ca = std::cos(a);
            const float sa = std::sin(a);
            const QVector3D cur = c + (u * (ca * r)) + (v * (sa * r));
            appendLine(dst, prev, cur);
            prev = cur;
        }
    };

    auto drawLines = [&](const std::vector<float>& verts, const QVector4D& color, const float widthPx)
    {
        if (verts.empty())
        {
            return;
        }

        f->glDisable(GL_DEPTH_TEST);
        f->glDepthMask(GL_FALSE);
        f->glEnable(GL_BLEND);
        f->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        f->glDisable(GL_CULL_FACE);

        if (thickLineProgram_)
        {
            thickLineProgram_->bind();
            thickLineProgram_->setUniformValue("u_mvp", mvp);
            thickLineProgram_->setUniformValue("u_viewport_size", QVector2D(static_cast<float>(width()), static_cast<float>(height())));
            thickLineProgram_->setUniformValue("u_line_width_px", widthPx);
            thickLineProgram_->setUniformValue("u_color", color);
            thickLineProgram_->setUniformValue("u_frame_xform_enabled", 0);
            thickLineProgram_->setUniformValue("u_frame_xform_id", 0);
            thickLineProgram_->setUniformValue("u_frame_xform_matrix", QMatrix4x4());
            thickLineProgram_->setUniformValue("u_xform_enabled", 0);
            thickLineProgram_->setUniformValue("u_xform_count", 0);
            thickLineProgram_->setUniformValue("u_xform_matrix", QMatrix4x4());

            // Ensure disabled pick-id attributes read as 0 for this draw.
            f->glVertexAttrib1f(3, 0.0f);

            transformGizmoLineVao_->bind();
            transformGizmoLineVbo_->bind();
            if (!uploadGlViewGizmoFloatBuffer(*transformGizmoLineVbo_, transformGizmoLineVboAllocatedBytes_, verts))
            {
                transformGizmoLineVbo_->release();
                transformGizmoLineVao_->release();
                thickLineProgram_->release();
                f->glEnable(GL_CULL_FACE);
                f->glCullFace(GL_BACK);
                f->glDisable(GL_BLEND);
                f->glDepthMask(GL_TRUE);
                f->glEnable(GL_DEPTH_TEST);
                return;
            }
            transformGizmoLineVbo_->release();

            f->glDrawArrays(GL_LINES, 0, static_cast<int>(verts.size() / 3));
            transformGizmoLineVao_->release();

            thickLineProgram_->release();
        }
        else if (program_)
        {
            // Fallback: thin GL lines through the main shader.
            program_->bind();
            program_->setUniformValue("u_mvp", mvp);
            program_->setUniformValue("u_color", color);
            program_->setUniformValue("u_textured", 0);
            program_->setUniformValue("u_alpha_test", 0);
            program_->setUniformValue("u_lighting_mode", 0);
            program_->setUniformValue("u_geo_min", 0);
            program_->setUniformValue("u_outline_px", 0.0f);
            program_->setUniformValue("u_flat_shade", false);
            program_->setUniformValue("u_frame_xform_enabled", 0);
            program_->setUniformValue("u_frame_xform_id", 0);
            program_->setUniformValue("u_frame_xform_color_mul", QVector3D(1.0f, 1.0f, 1.0f));
            program_->setUniformValue("u_frame_xform_matrix", QMatrix4x4());
            program_->setUniformValue("u_xform_enabled", 0);
            program_->setUniformValue("u_xform_count", 0);
            program_->setUniformValue("u_xform_matrix", QMatrix4x4());

            f->glLineWidth(std::max(1.0f, widthPx));
            f->glVertexAttrib4f(2, 1.0f, 1.0f, 1.0f, 1.0f);
            f->glVertexAttrib1f(3, 0.0f);

            transformGizmoLineVao_->bind();
            transformGizmoLineVbo_->bind();
            if (!uploadGlViewGizmoFloatBuffer(*transformGizmoLineVbo_, transformGizmoLineVboAllocatedBytes_, verts))
            {
                transformGizmoLineVbo_->release();
                transformGizmoLineVao_->release();
                f->glLineWidth(1.0f);
                program_->release();
                f->glEnable(GL_CULL_FACE);
                f->glCullFace(GL_BACK);
                f->glDisable(GL_BLEND);
                f->glDepthMask(GL_TRUE);
                f->glEnable(GL_DEPTH_TEST);
                return;
            }
            transformGizmoLineVbo_->release();

            f->glDrawArrays(GL_LINES, 0, static_cast<int>(verts.size() / 3));
            transformGizmoLineVao_->release();

            f->glLineWidth(1.0f);
            program_->release();
        }

        f->glEnable(GL_CULL_FACE);
        f->glCullFace(GL_BACK);
        f->glDisable(GL_BLEND);
        f->glDepthMask(GL_TRUE);
        f->glEnable(GL_DEPTH_TEST);
    };

    const bool dragging = transformDrag_.active;
    const TransformHandle activeHandle = transformDrag_.handle;

    if (transformTool_ == TransformTool::Move)
    {
        struct AxisDraw
        {
            QVector3D dir{};
            QVector4D color{};
            TransformHandle handle{ TransformHandle::None };
        };

        const std::array<AxisDraw, 3> axes = {
            AxisDraw{ basisX, QVector4D(0.90f, 0.30f, 0.25f, 0.90f), TransformHandle::MoveX },
            AxisDraw{ basisY, QVector4D(0.25f, 0.85f, 0.35f, 0.90f), TransformHandle::MoveY },
            AxisDraw{ basisZ, QVector4D(0.30f, 0.55f, 1.00f, 0.90f), TransformHandle::MoveZ },
        };

        for (const AxisDraw& ax : axes)
        {
            std::vector<float> verts;
            verts.reserve(3 * 6);

            const QVector3D start = drawPivot;
            const QVector3D end = drawPivot + ax.dir * axisLenW;
            appendLine(verts, start, end);

            QVector3D perp = QVector3D::crossProduct(ax.dir, camForward);
            if (perp.lengthSquared() < 1e-8f)
            {
                perp = QVector3D::crossProduct(ax.dir, camUp);
            }
            if (perp.lengthSquared() > sed::math::kTinyEpsilonF)
            {
                perp.normalize();
            }

            const QVector3D base = end - ax.dir * headLenW;
            appendLine(verts, end, base + perp * headW);
            appendLine(verts, end, base - perp * headW);

            const float widthPx = (dragging && activeHandle == ax.handle) ? kHighlightWidthPx : kBaseWidthPx;
            drawLines(verts, ax.color, widthPx);
        }

        // Center free-move handle.
        {
            std::vector<float> centerVerts;
            centerVerts.reserve(static_cast<std::size_t>(kRingSegs) * 6);
            appendCircle(centerVerts, drawPivot, camRight, camUp, centerRadiusW, 24);
            const float widthPx = (dragging && activeHandle == TransformHandle::MoveFree) ? kHighlightWidthPx : kBaseWidthPx;
            drawLines(centerVerts, QVector4D(0.92f, 0.92f, 0.92f, 0.70f), widthPx);
        }
    }
    else if (transformTool_ == TransformTool::Scale)
    {
        auto appendBillboardSquare = [&](std::vector<float>& verts, const QVector3D& center, const float halfSizeW)
        {
            const QVector3D r = camRight * halfSizeW;
            const QVector3D u = camUp * halfSizeW;
            const QVector3D p0 = center - r - u;
            const QVector3D p1 = center + r - u;
            const QVector3D p2 = center + r + u;
            const QVector3D p3 = center - r + u;
            appendLine(verts, p0, p1);
            appendLine(verts, p1, p2);
            appendLine(verts, p2, p3);
            appendLine(verts, p3, p0);
        };

        constexpr float kHandleHalfPx = 8.0f;
        constexpr float kCenterHalfPx = 10.0f;
        const float handleHalfW = unitsPerPx * kHandleHalfPx;
        const float centerHalfW = unitsPerPx * kCenterHalfPx;

        // Light mode: scale adjusts range; draw a single handle on a camera-aligned axis.
        const bool lightRangeTarget = targetKind == TransformTargetKind::Light
            || targetKind == TransformTargetKind::ThingLight
            || targetKind == TransformTargetKind::SectorLight;
        if (lightRangeTarget && level_)
        {
            float range = 0.0f;
            std::int32_t layer = 0;
            bool haveRange = false;
            if (targetKind == TransformTargetKind::Light
                && selectedLight_ >= 0
                && static_cast<std::size_t>(selectedLight_) < level_->lights.size())
            {
                const SedLight& light = level_->lights[static_cast<std::size_t>(selectedLight_)];
                range = static_cast<float>(light.range);
                layer = light.layer;
                haveRange = true;
            }
            else if (targetKind == TransformTargetKind::ThingLight)
            {
                const std::vector<render::LightGizmoEntry> entries =
                    render::thingLightGizmoEntries(*level_, templates_);
                const auto entry = std::find_if(entries.begin(), entries.end(), [&](const render::LightGizmoEntry& candidate)
                {
                    return candidate.ownerIndex == selectedThing_
                        && candidate.kind == selectedLightGizmoKind_;
                });
                if (entry != entries.end())
                {
                    range = std::max(entry->minRadius, entry->maxRadius);
                    layer = entry->layer;
                    haveRange = true;
                }
            }
            else if (targetKind == TransformTargetKind::SectorLight
                && selectedSector_ >= 0
                && static_cast<std::size_t>(selectedSector_) < level_->sectors.size())
            {
                const Sector& sector = level_->sectors[static_cast<std::size_t>(selectedSector_)];
                const double explicitRange = std::max(
                    sector.pointLight.minRange == 0.0 ? 0.0 : sector.pointLight.minRange,
                    sector.pointLight.maxRange == 0.0 ? 0.0 : sector.pointLight.maxRange);
                if (std::isfinite(explicitRange) && explicitRange > 0.0)
                {
                    range = static_cast<float>(explicitRange);
                    layer = sector.layer;
                    haveRange = true;
                }
            }
            if (!haveRange || !isLayerVisible(layer))
            {
                return;
            }
            if (!std::isfinite(range) || range <= 0.0f)
            {
                range = axisLenW * 0.30f;
            }
            if (dragging && activeHandle == TransformHandle::ScaleUniform)
            {
                const float s = transformDrag_.scaleFactor;
                if (std::isfinite(s) && s > 0.0f)
                {
                    range *= s;
                }
            }

            const QVector3D dir = camRight;
            const float lenW = std::clamp(range * 0.80f, axisLenW * 0.25f, axisLenW);
            const QVector3D end = drawPivot + dir * lenW;

            std::vector<float> verts;
            verts.reserve(3 * 16);
            appendLine(verts, drawPivot, end);
            appendBillboardSquare(verts, end, handleHalfW);

            const float widthPx = (dragging && activeHandle == TransformHandle::ScaleUniform) ? kHighlightWidthPx : kBaseWidthPx;
            drawLines(verts, QVector4D(0.95f, 0.90f, 0.35f, 0.90f), widthPx);

            return;
        }

        struct AxisDraw
        {
            QVector3D dir{};
            QVector4D color{};
            TransformHandle handle{ TransformHandle::None };
        };

        const std::array<AxisDraw, 3> axes = {
            AxisDraw{ QVector3D(1.0f, 0.0f, 0.0f), QVector4D(0.90f, 0.30f, 0.25f, 0.90f), TransformHandle::ScaleX },
            AxisDraw{ QVector3D(0.0f, 1.0f, 0.0f), QVector4D(0.25f, 0.85f, 0.35f, 0.90f), TransformHandle::ScaleY },
            AxisDraw{ QVector3D(0.0f, 0.0f, 1.0f), QVector4D(0.30f, 0.55f, 1.00f, 0.90f), TransformHandle::ScaleZ },
        };

        for (const AxisDraw& ax : axes)
        {
            std::vector<float> verts;
            verts.reserve(3 * 18);

            const QVector3D start = drawPivot;
            const QVector3D end = drawPivot + ax.dir * axisLenW;
            appendLine(verts, start, end);
            appendBillboardSquare(verts, end, handleHalfW);

            const float widthPx = (dragging && activeHandle == ax.handle) ? kHighlightWidthPx : kBaseWidthPx;
            drawLines(verts, ax.color, widthPx);
        }

        // Center uniform scale handle.
        {
            std::vector<float> verts;
            verts.reserve(3 * 8);
            appendBillboardSquare(verts, drawPivot, centerHalfW);
            const float widthPx = (dragging && activeHandle == TransformHandle::ScaleUniform) ? kHighlightWidthPx : kBaseWidthPx;
            drawLines(verts, QVector4D(0.92f, 0.92f, 0.92f, 0.75f), widthPx);
        }
    }
    else if (transformTool_ == TransformTool::Rotate)
    {
        const bool draggingRotate =
            dragging
            && (activeHandle == TransformHandle::RotateX
                || activeHandle == TransformHandle::RotateY
                || activeHandle == TransformHandle::RotateZ
                || activeHandle == TransformHandle::RotateView);

        QVector3D toEye = eye - drawPivot;
        if (toEye.lengthSquared() > sed::math::kTinyEpsilonF)
        {
            toEye.normalize();
        }
        else
        {
            toEye = QVector3D(0.0f, 0.0f, 1.0f);
        }

        auto appendCircleSplit = [&](
            std::vector<float>& outFront,
            std::vector<float>& outBack,
            const QVector3D& c,
            QVector3D u,
            QVector3D v,
            const float r,
            const int segs)
        {
            if (u.lengthSquared() > sed::math::kTinyEpsilonF) { u.normalize(); }
            if (v.lengthSquared() > sed::math::kTinyEpsilonF) { v.normalize(); }

            QVector3D prev = c + u * r;
            for (int i = 1; i <= segs; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(segs);
                const float a = t * 2.0f * std::numbers::pi_v<float>;
                const float ca = std::cos(a);
                const float sa = std::sin(a);
                const QVector3D cur = c + (u * (ca * r)) + (v * (sa * r));

                const QVector3D mid = (prev + cur) * 0.5f;
                const bool front = QVector3D::dotProduct(mid - c, toEye) >= 0.0f;

                if (front)
                {
                    appendLine(outFront, prev, cur);
                }
                else
                {
                    // Dashed back half to match common DCC gizmo readability.
                    if ((i & 1) == 0)
                    {
                        appendLine(outBack, prev, cur);
                    }
                }

                prev = cur;
            }
        };

        struct RingDraw
        {
            QVector3D u{};
            QVector3D v{};
            float radiusScale{1.0f};
            QVector4D color{};
            TransformHandle handle{ TransformHandle::None };
        };

        const std::array<RingDraw, 3> rings = {
            RingDraw{ basisY, basisZ, 1.00f, QVector4D(0.90f, 0.30f, 0.25f, 0.85f), TransformHandle::RotateX },
            RingDraw{ basisX, basisZ, 0.94f, QVector4D(0.25f, 0.85f, 0.35f, 0.85f), TransformHandle::RotateY },
            RingDraw{ basisX, basisY, 1.08f, QVector4D(0.30f, 0.55f, 1.00f, 0.85f), TransformHandle::RotateZ },
        };

        for (const RingDraw& ring : rings)
        {
            if (draggingRotate && activeHandle != ring.handle)
            {
                continue;
            }

            const float widthPx = (dragging && activeHandle == ring.handle) ? kHighlightWidthPx : kBaseWidthPx;

            std::vector<float> frontVerts;
            std::vector<float> backVerts;
            frontVerts.reserve(static_cast<std::size_t>(kRingSegs) * 6);
            backVerts.reserve(static_cast<std::size_t>(kRingSegs) * 6);

            appendCircleSplit(frontVerts, backVerts, drawPivot, ring.u, ring.v, ringRadiusW * ring.radiusScale, kRingSegs);

            QVector4D backColor = ring.color;
            backColor.setW(ring.color.w() * 0.28f);
            drawLines(backVerts, backColor, widthPx * 0.90f);
            drawLines(frontVerts, ring.color, widthPx);
        }

        // View-aligned ring.
        if (!draggingRotate || activeHandle == TransformHandle::RotateView)
        {
            std::vector<float> verts;
            verts.reserve(static_cast<std::size_t>(kRingSegs) * 6);
            appendCircle(verts, drawPivot, camRight, camUp, viewRingRadiusW, kRingSegs);
            const float widthPx = (dragging && activeHandle == TransformHandle::RotateView) ? kHighlightWidthPx : kBaseWidthPx;
            drawLines(verts, QVector4D(0.94f, 0.94f, 0.94f, 0.55f), widthPx);
        }

        // While dragging a rotation handle, show an angle indicator (spokes + arc) and hide other rings.
        if (draggingRotate && transformDrag_.handle != TransformHandle::None)
        {
            const QVector3D dragAxis = transformDrag_.axis;
            const QVector3D axisUnit =
                (dragAxis.lengthSquared() > sed::math::kTinyEpsilonF) ? dragAxis.normalized() : QVector3D(0.0f, 0.0f, 1.0f);

            const float radiusW = (activeHandle == TransformHandle::RotateView) ? viewRingRadiusW : ringRadiusW;

            const QVector3D startDir = transformDrag_.startHit;
            const QVector3D curDir = transformDrag_.currentHit;

            if (startDir.lengthSquared() > 1e-8f && curDir.lengthSquared() > 1e-8f)
            {
                const QVector3D p0 = drawPivot + startDir.normalized() * radiusW;
                const QVector3D p1 = drawPivot + curDir.normalized() * radiusW;

                std::vector<float> indicator;
                indicator.reserve(3 * 2 * (kRingSegs + 8));

                // Spokes.
                appendLine(indicator, drawPivot, p0);
                appendLine(indicator, drawPivot, p1);

                // Arc from start -> current, following the signed angle.
                const float angleRad = static_cast<float>(deg2Rad(transformDrag_.angleDeg));
                const float absAngle = std::abs(angleRad);
                const float full = 2.0f * std::numbers::pi_v<float>;
                const int segs = std::clamp(static_cast<int>(std::ceil((absAngle / full) * static_cast<float>(kRingSegs))), 8, kRingSegs);

                QVector3D prev = p0;
                for (int i = 1; i <= segs; ++i)
                {
                    const float t = static_cast<float>(i) / static_cast<float>(segs);
                    const float a = angleRad * t;
                    const QVector3D d = rotateAroundAxis(startDir.normalized(), axisUnit, a);
                    const QVector3D cur = drawPivot + d * radiusW;
                    appendLine(indicator, prev, cur);
                    prev = cur;
                }

                drawLines(indicator, QVector4D(0.98f, 0.98f, 0.98f, 0.85f), kBaseWidthPx);
            }
        }
    }
}

namespace
{

using render::GeometrySnapSurface;

std::uint64_t geometrySnapCellKey(
    const std::int64_t x,
    const std::int64_t y,
    const std::int64_t z) noexcept
{
    // A deterministic 64-bit hash is sufficient here: collisions only add
    // extra exact candidates; they can never produce an incorrect snap.
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash](const std::int64_t value) noexcept {
        std::uint64_t bits = static_cast<std::uint64_t>(value);
        for (int byte = 0; byte < 8; ++byte)
        {
            hash ^= bits & 0xffu;
            hash *= 1099511628211ull;
            bits >>= 8u;
        }
    };
    mix(x);
    mix(y);
    mix(z);
    return hash;
}

struct GeometryMoveSnapResult
{
    TVector translation{};
    QVector3D rotationAxis{};
    TVector rotationPivot{};
    float rotationAngleDeg{};
};

bool collectGeometrySnapSurface(const Level& level, const SurfaceRef ref, GeometrySnapSurface& out, const TVector& origin = TVector{})
{
    if (ref.sector < 0 || ref.surface < 0 || static_cast<std::size_t>(ref.sector) >= level.sectors.size())
    {
        return false;
    }

    const Sector& sector = level.sectors[static_cast<std::size_t>(ref.sector)];
    if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
    {
        return false;
    }

    const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
    if (surface.vertices.empty())
    {
        return false;
    }

    out = GeometrySnapSurface{};
    out.ref = ref;
    out.vertices.reserve(surface.vertices.size());
    out.boundsMin = QVector3D(
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity());
    out.boundsMax = QVector3D(
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity());

    TVector center{};
    for (const std::int32_t vi : surface.vertices)
    {
        if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
        {
            return false;
        }

        const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) return false;
        const TVector p = TVector{v.x, v.y, v.z} - origin;
        if (out.vertices.empty())
        {
            out.point = p;
        }
        out.vertices.push_back(p);
        out.boundsMin.setX(std::min(out.boundsMin.x(), static_cast<float>(p.x)));
        out.boundsMin.setY(std::min(out.boundsMin.y(), static_cast<float>(p.y)));
        out.boundsMin.setZ(std::min(out.boundsMin.z(), static_cast<float>(p.z)));
        out.boundsMax.setX(std::max(out.boundsMax.x(), static_cast<float>(p.x)));
        out.boundsMax.setY(std::max(out.boundsMax.y(), static_cast<float>(p.y)));
        out.boundsMax.setZ(std::max(out.boundsMax.z(), static_cast<float>(p.z)));
        center = center + p;
    }

    if (out.vertices.empty())
    {
        return false;
    }

    out.center = center * (1.0 / static_cast<double>(out.vertices.size()));
    out.boundingRadius = 0.0f;
    for (const TVector& p : out.vertices)
    {
        out.boundingRadius = std::max(out.boundingRadius, static_cast<float>(std::sqrt(p.DistanceSqr(out.center))));
    }
    TVector n = surface.normal;
    if (n.Length() == 0.0)
    {
        n = sed::level::surfaceCalcNormalNewell(sector, surface);
    }
    const double normalLength = std::hypot(n.x, n.y, n.z);
    if (!std::isfinite(normalLength) || normalLength == 0.0) return false;
    n.Scale(1.0 / normalLength);
    out.documentNormal = n;
    out.normal = QVector3D(static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z));
    return normalizeInPlace(out.normal);
}

TVector mapSnapPoint(
    const TVector& point, const TVector& pivot, const TVector& translation,
    const QVector3D& rotationAxis, const float rotationAngleDeg)
{
    return pivot + render::rotateAuthoringVector(point - pivot,
        render::authoringVector(rotationAxis), rotationAngleDeg) + translation;
}

bool buildSnapPlaneBasis(const QVector3D& normal, QVector3D& outX, QVector3D& outY)
{
    QVector3D n = normal;
    if (!normalizeInPlace(n))
    {
        return false;
    }

    const QVector3D refAxis = (std::abs(n.z()) < 0.9f)
        ? QVector3D(0.0f, 0.0f, 1.0f)
        : QVector3D(1.0f, 0.0f, 0.0f);

    outX = QVector3D::crossProduct(refAxis, n);
    if (!normalizeInPlace(outX))
    {
        outX = QVector3D::crossProduct(QVector3D(0.0f, 1.0f, 0.0f), n);
        if (!normalizeInPlace(outX))
        {
            return false;
        }
    }

    outY = QVector3D::crossProduct(n, outX);
    return normalizeInPlace(outY);
}

float computeProjectedBoundsOverlapRatio(
    const std::vector<TVector>& a,
    const std::vector<TVector>& b,
    const QVector3D& planeNormal)
{
    if (a.empty() || b.empty())
    {
        return 0.0f;
    }

    QVector3D axisX{};
    QVector3D axisY{};
    if (!buildSnapPlaneBasis(planeNormal, axisX, axisY))
    {
        return 0.0f;
    }

    auto computeBounds = [&](const std::vector<TVector>& vertices, float& minX, float& minY, float& maxX, float& maxY)
    {
        minX = std::numeric_limits<float>::infinity();
        minY = std::numeric_limits<float>::infinity();
        maxX = -std::numeric_limits<float>::infinity();
        maxY = -std::numeric_limits<float>::infinity();

        for (const TVector& p : vertices)
        {
            const float x = static_cast<float>(p.Dot(render::authoringVector(axisX)));
            const float y = static_cast<float>(p.Dot(render::authoringVector(axisY)));
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    };

    float aMinX{};
    float aMinY{};
    float aMaxX{};
    float aMaxY{};
    float bMinX{};
    float bMinY{};
    float bMaxX{};
    float bMaxY{};
    computeBounds(a, aMinX, aMinY, aMaxX, aMaxY);
    computeBounds(b, bMinX, bMinY, bMaxX, bMaxY);

    const float overlapX = std::max(0.0f, std::min(aMaxX, bMaxX) - std::max(aMinX, bMinX));
    const float overlapY = std::max(0.0f, std::min(aMaxY, bMaxY) - std::max(aMinY, bMinY));
    const float overlapArea = overlapX * overlapY;
    if (overlapArea <= 1.0e-6f)
    {
        return 0.0f;
    }

    const float areaA = std::max(1.0e-6f, (aMaxX - aMinX) * (aMaxY - aMinY));
    const float areaB = std::max(1.0e-6f, (bMaxX - bMinX) * (bMaxY - bMinY));
    return overlapArea / std::min(areaA, areaB);
}

float computeProjectedBoundsAreaRatio(
    const std::vector<TVector>& a,
    const std::vector<TVector>& b,
    const QVector3D& planeNormal)
{
    if (a.empty() || b.empty())
    {
        return 0.0f;
    }

    QVector3D axisX{};
    QVector3D axisY{};
    if (!buildSnapPlaneBasis(planeNormal, axisX, axisY))
    {
        return 0.0f;
    }

    auto computeBounds = [&](const std::vector<TVector>& vertices, float& minX, float& minY, float& maxX, float& maxY)
    {
        minX = std::numeric_limits<float>::infinity();
        minY = std::numeric_limits<float>::infinity();
        maxX = -std::numeric_limits<float>::infinity();
        maxY = -std::numeric_limits<float>::infinity();

        for (const TVector& p : vertices)
        {
            const float x = static_cast<float>(p.Dot(render::authoringVector(axisX)));
            const float y = static_cast<float>(p.Dot(render::authoringVector(axisY)));
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    };

    float aMinX{};
    float aMinY{};
    float aMaxX{};
    float aMaxY{};
    float bMinX{};
    float bMinY{};
    float bMaxX{};
    float bMaxY{};
    computeBounds(a, aMinX, aMinY, aMaxX, aMaxY);
    computeBounds(b, bMinX, bMinY, bMaxX, bMaxY);

    const float areaA = std::max(1.0e-6f, (aMaxX - aMinX) * (aMaxY - aMinY));
    const float areaB = std::max(1.0e-6f, (bMaxX - bMinX) * (bMaxY - bMinY));
    return std::min(areaA, areaB) / std::max(areaA, areaB);
}

render::GeometryMoveSnapCache buildGeometryMoveSnapCache(
    const Level& level,
    const bool sectorTarget,
    const GlView::SurfaceSubMode surfaceMode,
    const editor::Selection& highlightedSelection,
    const std::int32_t selectedSector,
    const std::int32_t selectedSurfaceSector,
    const std::int32_t selectedSurface,
    const std::int32_t activeSurfaceSector,
    const std::int32_t activeSurfaceIndex,
    const std::int32_t activeSurfaceVertex,
    const std::int32_t activeSurfaceEdge,
    const std::unordered_set<std::int32_t>& hiddenLayers,
    const std::unordered_set<std::int32_t>& lockedLayers,
    const float broadPhaseCellSize,
    const TVector& documentOrigin)
{
    render::GeometryMoveSnapCache cache{};
    cache.documentOrigin = documentOrigin;
    cache.broadPhaseCellSize = std::max(1.0f, std::isfinite(broadPhaseCellSize) ? broadPhaseCellSize : 16.0f);
    std::vector<SurfaceRef> movingRefs{};
    std::unordered_set<std::uint64_t> movingRefKeys{};
    std::unordered_set<std::uint64_t> movingVertexKeys{};

    const auto sectorAllowed = [&](const std::int32_t sectorIndex) noexcept
    {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            return false;
        }
        const std::int32_t layer = level.sectors[static_cast<std::size_t>(sectorIndex)].layer;
        return !hiddenLayers.contains(layer) && !lockedLayers.contains(layer);
    };

    const auto refKey = [](const SurfaceRef ref) -> std::uint64_t
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32u)
            ^ static_cast<std::uint32_t>(ref.surface);
    };
    const auto vertexKey = [](const std::int32_t sector,
                              const std::int32_t vertex) -> std::uint64_t
    {
        return (static_cast<std::uint64_t>(
                    static_cast<std::uint32_t>(sector)) << 32u)
            ^ static_cast<std::uint32_t>(vertex);
    };

    const auto addSurfaceRef = [&](const SurfaceRef ref)
    {
        if (!sectorAllowed(ref.sector) || ref.surface < 0)
        {
            return;
        }
        if (!movingRefKeys.insert(refKey(ref)).second)
        {
            return;
        }
        movingRefs.push_back(ref);
        cache.movingSectors.insert(ref.sector);
    };

    if (sectorTarget)
    {
        std::unordered_set<std::int32_t> sectors = highlightedSelection.sectors;
        if (selectedSector >= 0)
        {
            sectors.insert(selectedSector);
        }

        for (const std::int32_t sectorIndex : sectors)
        {
            if (!sectorAllowed(sectorIndex))
            {
                continue;
            }
            const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
            for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(sector.surfaces.size()); ++sf)
            {
                addSurfaceRef(SurfaceRef{ sectorIndex, sf });
            }
        }
    }
    else
    {
        for (const editor::SurfaceVertexRef& selected :
             highlightedSelection.surfaceVertices)
        {
            addSurfaceRef(selected.surface);
            if (selected.surface.sector < 0 || selected.surface.surface < 0
                || selected.localVertex < 0
                || static_cast<std::size_t>(selected.surface.sector)
                    >= level.sectors.size())
            {
                continue;
            }
            const Sector& sector = level.sectors[
                static_cast<std::size_t>(selected.surface.sector)];
            if (static_cast<std::size_t>(selected.surface.surface)
                    >= sector.surfaces.size())
            {
                continue;
            }
            const Surface& surface = sector.surfaces[
                static_cast<std::size_t>(selected.surface.surface)];
            if (static_cast<std::size_t>(selected.localVertex)
                    < surface.vertices.size())
            {
                movingVertexKeys.insert(vertexKey(
                    selected.surface.sector,
                    surface.vertices[static_cast<std::size_t>(
                        selected.localVertex)]));
            }
        }
        for (const SurfaceRef ref : highlightedSelection.surfaces)
        {
            addSurfaceRef(ref);
        }
        if (selectedSurfaceSector >= 0 && selectedSurface >= 0)
        {
            addSurfaceRef(SurfaceRef{ selectedSurfaceSector, selectedSurface });
        }
    }

    if (!sectorTarget && surfaceMode == GlView::SurfaceSubMode::Vertex
        && activeSurfaceSector >= 0 && activeSurfaceVertex >= 0
        && static_cast<std::size_t>(activeSurfaceSector) < level.sectors.size()
        && static_cast<std::size_t>(activeSurfaceVertex)
            < level.sectors[static_cast<std::size_t>(activeSurfaceSector)]
                .vertices.size())
    {
        const Vertex& vertex = level.sectors[
            static_cast<std::size_t>(activeSurfaceSector)]
            .vertices[static_cast<std::size_t>(activeSurfaceVertex)];
        cache.movingVertex = render::SurfaceSnapVertex{
            QVector3D(static_cast<float>(vertex.x),
                      static_cast<float>(vertex.y),
                      static_cast<float>(vertex.z)),
            activeSurfaceSector,
            activeSurfaceVertex,
            TVector{vertex.x, vertex.y, vertex.z} };
        movingVertexKeys.insert(vertexKey(
            activeSurfaceSector, activeSurfaceVertex));
    }

    if (!sectorTarget && surfaceMode == GlView::SurfaceSubMode::Edge
        && activeSurfaceSector >= 0 && activeSurfaceIndex >= 0
        && activeSurfaceEdge >= 0
        && static_cast<std::size_t>(activeSurfaceSector) < level.sectors.size())
    {
        const Sector& sector = level.sectors[
            static_cast<std::size_t>(activeSurfaceSector)];
        if (static_cast<std::size_t>(activeSurfaceIndex) < sector.surfaces.size())
        {
            const Surface& surface = sector.surfaces[
                static_cast<std::size_t>(activeSurfaceIndex)];
            if (surface.vertices.size() >= 2u
                && static_cast<std::size_t>(activeSurfaceEdge)
                    < surface.vertices.size())
            {
                const std::int32_t first = surface.vertices[
                    static_cast<std::size_t>(activeSurfaceEdge)];
                const std::int32_t second = surface.vertices[
                    (static_cast<std::size_t>(activeSurfaceEdge) + 1u)
                    % surface.vertices.size()];
                if (first >= 0 && second >= 0
                    && static_cast<std::size_t>(first) < sector.vertices.size()
                    && static_cast<std::size_t>(second) < sector.vertices.size())
                {
                    const Vertex& a = sector.vertices[static_cast<std::size_t>(first)];
                    const Vertex& b = sector.vertices[static_cast<std::size_t>(second)];
                    cache.movingEdge = render::SurfaceSnapEdge{
                        QVector3D(static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z)),
                        QVector3D(static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.z)),
                        activeSurfaceSector,
                        first,
                        second,
                        TVector{a.x, a.y, a.z}, TVector{b.x, b.y, b.z} };
                    movingVertexKeys.insert(vertexKey(activeSurfaceSector, first));
                    movingVertexKeys.insert(vertexKey(activeSurfaceSector, second));
                }
            }
        }
    }

    cache.movingSurfaces.reserve(movingRefs.size());
    for (const SurfaceRef ref : movingRefs)
    {
        GeometrySnapSurface data{};
        if (collectGeometrySnapSurface(level, ref, data, documentOrigin))
        {
            cache.movingSurfaces.push_back(std::move(data));
        }
    }

    // Stationary geometry is captured once for the drag.  Excluding hidden or
    // locked layers here also keeps snapping consistent with what the user can
    // currently see and edit in the viewport.
    for (std::int32_t sectorIndex = 0;
         sectorIndex < static_cast<std::int32_t>(level.sectors.size());
         ++sectorIndex)
    {
        if (!sectorAllowed(sectorIndex)
            || (sectorTarget && cache.movingSectors.contains(sectorIndex)))
        {
            continue;
        }
        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(sector.surfaces.size()); ++sf)
        {
            const SurfaceRef ref{ sectorIndex, sf };
            if (movingRefKeys.contains(refKey(ref)))
            {
                continue;
            }
            const Surface& surface = sector.surfaces[static_cast<std::size_t>(sf)];
            const bool sharesMovedVertex = std::any_of(
                surface.vertices.begin(), surface.vertices.end(),
                [&](const std::int32_t vertex)
                {
                    return movingVertexKeys.contains(vertexKey(sectorIndex, vertex));
                });
            if (sharesMovedVertex)
            {
                continue;
            }
            GeometrySnapSurface data{};
            if (collectGeometrySnapSurface(level, ref, data, documentOrigin))
            {
                cache.stationarySurfaces.push_back(std::move(data));
            }
        }

        if (!sectorTarget
            && (surfaceMode == GlView::SurfaceSubMode::Vertex
                || surfaceMode == GlView::SurfaceSubMode::Edge))
        {
            for (std::int32_t vertexIndex = 0;
                 vertexIndex < static_cast<std::int32_t>(sector.vertices.size());
                 ++vertexIndex)
            {
                if (movingVertexKeys.contains(vertexKey(sectorIndex, vertexIndex)))
                {
                    continue;
                }
                const Vertex& vertex = sector.vertices[
                    static_cast<std::size_t>(vertexIndex)];
                cache.stationaryVertices.push_back(render::SurfaceSnapVertex{
                    QVector3D(static_cast<float>(vertex.x),
                              static_cast<float>(vertex.y),
                              static_cast<float>(vertex.z)),
                    sectorIndex,
                    vertexIndex,
                    TVector{vertex.x, vertex.y, vertex.z} });
            }

            if (surfaceMode == GlView::SurfaceSubMode::Edge)
            {
                std::unordered_set<std::uint64_t> seenEdges{};
                for (const Surface& surface : sector.surfaces)
                {
                    for (std::size_t edge = 0; edge < surface.vertices.size(); ++edge)
                    {
                        const std::int32_t first = surface.vertices[edge];
                        const std::int32_t second = surface.vertices[
                            (edge + 1u) % surface.vertices.size()];
                        if (first < 0 || second < 0
                            || static_cast<std::size_t>(first) >= sector.vertices.size()
                            || static_cast<std::size_t>(second) >= sector.vertices.size()
                            || movingVertexKeys.contains(vertexKey(sectorIndex, first))
                            || movingVertexKeys.contains(vertexKey(sectorIndex, second)))
                        {
                            continue;
                        }
                        const std::uint32_t low = static_cast<std::uint32_t>(
                            std::min(first, second));
                        const std::uint32_t high = static_cast<std::uint32_t>(
                            std::max(first, second));
                        const std::uint64_t edgeKey =
                            (static_cast<std::uint64_t>(low) << 32u) ^ high;
                        if (!seenEdges.insert(edgeKey).second)
                        {
                            continue;
                        }
                        const Vertex& a = sector.vertices[static_cast<std::size_t>(first)];
                        const Vertex& b = sector.vertices[static_cast<std::size_t>(second)];
                        cache.stationaryEdges.push_back(render::SurfaceSnapEdge{
                            QVector3D(static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z)),
                            QVector3D(static_cast<float>(b.x), static_cast<float>(b.y), static_cast<float>(b.z)),
                            sectorIndex,
                            first,
                            second,
                            TVector{a.x, a.y, a.z}, TVector{b.x, b.y, b.z} });
                    }
                }
            }
        }
    }

    // Index stationary surfaces by center cell.  Very large surfaces are
    // kept in a side list so a single wall cannot populate thousands of
    // cells.  The exact plane/vertex tests below still decide every match.
    for (std::size_t index = 0; index < cache.stationarySurfaces.size(); ++index)
    {
        const GeometrySnapSurface& surface = cache.stationarySurfaces[index];
        if (surface.boundingRadius > cache.broadPhaseCellSize * 4.0f)
        {
            cache.largeStationarySurfaces.push_back(index);
            continue;
        }
        const std::int64_t cellX = render::surfaceSnapBroadPhaseCellCoordinate(static_cast<float>(surface.center.x), cache.broadPhaseCellSize);
        const std::int64_t cellY = render::surfaceSnapBroadPhaseCellCoordinate(static_cast<float>(surface.center.y), cache.broadPhaseCellSize);
        const std::int64_t cellZ = render::surfaceSnapBroadPhaseCellCoordinate(static_cast<float>(surface.center.z), cache.broadPhaseCellSize);
        cache.stationaryBuckets[geometrySnapCellKey(cellX, cellY, cellZ)].push_back(index);
    }

    return cache;
}

std::optional<GeometryMoveSnapResult> computeGeometryMoveSnap(
    const render::GeometryMoveSnapCache& cache,
    const bool sectorTarget,
    const bool allowOrientation,
    const TVector& documentPivot,
    const TVector& rawTranslation,
    const float searchRadius,
    const float rotationLimitDegrees)
{
    const TVector pivot = documentPivot - cache.documentOrigin;
    if (!std::isfinite(rawTranslation.x) || !std::isfinite(rawTranslation.y) || !std::isfinite(rawTranslation.z)
        || (rawTranslation.x == 0.0 && rawTranslation.y == 0.0 && rawTranslation.z == 0.0))
    {
        return std::nullopt;
    }

    if (!cache.valid())
    {
        return std::nullopt;
    }

    const std::vector<GeometrySnapSurface>& movingSurfaces = cache.movingSurfaces;
    const std::vector<GeometrySnapSurface>& stationarySurfaces = cache.stationarySurfaces;

    const float centerThreshold = std::max(searchRadius, 0.000001f);
    const float planeThreshold = centerThreshold * 0.06f;
    const float vertexThreshold = centerThreshold * 0.175f;
    // Sector alignment remains a conservative correction because it compares
    // every moving face in a complete volume. Face Move is an explicit
    // designer operation: once a nearby face has been spatially acquired,
    // either parallel normal sign is valid and the closest one can require up
    // to a quarter turn.
    const float maxAngleDeg = std::min(
        std::clamp(rotationLimitDegrees, 0.0f, 180.0f),
        sectorTarget ? 35.0f : 90.0f);
    const float minCos = static_cast<float>(std::cos(deg2Rad(static_cast<double>(maxAngleDeg))));

    bool found = false;
    GeometryMoveSnapResult best{};
    double bestScore = std::numeric_limits<float>::infinity();

    for (const GeometrySnapSurface& moving : movingSurfaces)
    {
        std::vector<std::size_t> candidateIndices{};
        const auto appendAllCandidates = [&]() {
            candidateIndices.resize(stationarySurfaces.size());
            std::iota(candidateIndices.begin(), candidateIndices.end(), std::size_t{ 0 });
        };

        if (cache.stationaryBuckets.empty())
        {
            appendAllCandidates();
        }
        else
        {
            // Rotation is target-normal dependent, so use the raw translated
            // center for the broad phase and conservatively include the full
            // moving surface radius plus the largest indexed target radius.
            const TVector movedCenter = mapSnapPoint(
                moving.center, pivot, rawTranslation, QVector3D{}, 0.0f);
            const float pivotDistance = static_cast<float>(std::sqrt(moving.center.DistanceSqr(pivot)));
            const float maxRotationDisplacement = allowOrientation
                ? 2.0f * pivotDistance
                    * std::sin(static_cast<float>(
                        deg2Rad(static_cast<double>(maxAngleDeg))) * 0.5f)
                : 0.0f;
            const auto roundingPad = [](const double value)
            {
                const float display = static_cast<float>(value);
                return std::isfinite(display)
                    ? std::abs(std::nextafter(display, std::numeric_limits<float>::infinity()) - display) * 2.0f
                    : std::numeric_limits<float>::infinity();
            };
            const float queryRadius =
                std::max({roundingPad(movedCenter.x), roundingPad(movedCenter.y), roundingPad(movedCenter.z)})
                + (moving.boundingRadius * 2.0f)
                + (cache.broadPhaseCellSize * 4.0f)
                + maxRotationDisplacement
                + std::max({ planeThreshold, vertexThreshold, centerThreshold });
            const float cellSize = cache.broadPhaseCellSize;
            const std::int64_t minX = render::surfaceSnapBroadPhaseCellCoordinate(static_cast<float>(movedCenter.x) - queryRadius, cellSize);
            const std::int64_t minY = render::surfaceSnapBroadPhaseCellCoordinate(static_cast<float>(movedCenter.y) - queryRadius, cellSize);
            const std::int64_t minZ = render::surfaceSnapBroadPhaseCellCoordinate(static_cast<float>(movedCenter.z) - queryRadius, cellSize);
            const std::int64_t maxX = render::surfaceSnapBroadPhaseCellCoordinate(static_cast<float>(movedCenter.x) + queryRadius, cellSize);
            const std::int64_t maxY = render::surfaceSnapBroadPhaseCellCoordinate(static_cast<float>(movedCenter.y) + queryRadius, cellSize);
            const std::int64_t maxZ = render::surfaceSnapBroadPhaseCellCoordinate(static_cast<float>(movedCenter.z) + queryRadius, cellSize);
            const auto rangeCount = [](const std::int64_t minValue, const std::int64_t maxValue) noexcept -> std::uint64_t {
                if (maxValue < minValue)
                {
                    return 0;
                }
                constexpr std::uint64_t maximumCandidateCells = 8192u;
                const std::uint64_t difference =
                    static_cast<std::uint64_t>(maxValue)
                    - static_cast<std::uint64_t>(minValue);
                return difference >= maximumCandidateCells
                    ? maximumCandidateCells + 1u
                    : difference + 1u;
            };
            constexpr std::uint64_t maxCandidateCells = 8192u;
            const std::uint64_t countX = rangeCount(minX, maxX);
            const std::uint64_t countY = rangeCount(minY, maxY);
            const std::uint64_t countZ = rangeCount(minZ, maxZ);
            const auto productExceedsLimit = [maxCandidateCells](const std::uint64_t lhs, const std::uint64_t rhs) noexcept {
                return lhs != 0u && rhs > maxCandidateCells / lhs;
            };
            const bool cellCountTooLarge = productExceedsLimit(countX, countY)
                || productExceedsLimit(countX * std::min(countY, maxCandidateCells), countZ);
            const std::uint64_t cellCount = cellCountTooLarge
                ? maxCandidateCells + 1u
                : countX * countY * countZ;

            // A pathological large query is cheaper and safer to handle as a
            // full scan than to allocate an enormous candidate list.  This is
            // only a broad-phase choice; exact matching remains unchanged.
            if (cellCount == 0u || cellCount > maxCandidateCells)
            {
                appendAllCandidates();
            }
            else
            {
                candidateIndices.reserve(static_cast<std::size_t>(cellCount) * 2u + cache.largeStationarySurfaces.size());
                for (std::uint64_t ix = 0; ix < countX; ++ix)
                {
                    const std::int64_t x = minX + static_cast<std::int64_t>(ix);
                    for (std::uint64_t iy = 0; iy < countY; ++iy)
                    {
                        const std::int64_t y = minY + static_cast<std::int64_t>(iy);
                        for (std::uint64_t iz = 0; iz < countZ; ++iz)
                        {
                            const std::int64_t z = minZ + static_cast<std::int64_t>(iz);
                            const auto it = cache.stationaryBuckets.find(geometrySnapCellKey(x, y, z));
                            if (it == cache.stationaryBuckets.end())
                            {
                                continue;
                            }
                            candidateIndices.insert(candidateIndices.end(), it->second.begin(), it->second.end());
                        }
                    }
                }
                candidateIndices.insert(
                    candidateIndices.end(),
                    cache.largeStationarySurfaces.begin(),
                    cache.largeStationarySurfaces.end());
            }
        }

        for (const std::size_t targetIndex : candidateIndices)
        {
                if (targetIndex >= stationarySurfaces.size())
                {
                    continue;
                }
                const GeometrySnapSurface& target = stationarySurfaces[targetIndex];
                const float authoredAlignment = QVector3D::dotProduct(
                    moving.normal, target.normal);
                const float alignment = sectorTarget
                    ? -authoredAlignment
                    : std::abs(authoredAlignment);
                if (alignment < minCos)
                {
                    continue;
                }

                QVector3D rotationAxis{};
                float rotationAngleDeg = 0.0f;
                // Face Move is where a nearby surface can be acquired by
                // position. Finish its closest parallel-normal orientation
                // here, rather than waiting for a Rotate drag that may no
                // longer be spatially close to the intended target.
                if (allowOrientation)
                {
                    if (const std::optional<render::SurfaceMoveOrientationSnapResult>
                        orientation = render::snapSurfaceMoveOrientation(
                            moving.normal, target.normal, maxAngleDeg))
                    {
                        rotationAxis = orientation->rotationAxis;
                        rotationAngleDeg = orientation->rotationAngleDegrees;
                    }
                }

                const TVector movedCenter = mapSnapPoint(moving.center, pivot, rawTranslation, rotationAxis, rotationAngleDeg);
                const TVector targetNormal = target.documentNormal;
                const double signedPlaneDist = targetNormal.Dot(movedCenter - target.point);
                if (std::abs(signedPlaneDist) > planeThreshold)
                {
                    continue;
                }

                const TVector planeCorrection = targetNormal * -signedPlaneDist;

                TVector bestPlaneDelta{};
                double bestPlaneDeltaLen = std::numeric_limits<float>::infinity();
                std::vector<TVector> movedVerticesOnPlane{};
                movedVerticesOnPlane.reserve(moving.vertices.size());
                for (const TVector& mv : moving.vertices)
                {
                    const TVector movedVertex =
                        mapSnapPoint(mv, pivot, rawTranslation, rotationAxis, rotationAngleDeg) + planeCorrection;
                    movedVerticesOnPlane.push_back(movedVertex);
                    for (const TVector& tv : target.vertices)
                    {
                        const TVector delta = tv - movedVertex;
                        const TVector planeDelta =
                            delta - targetNormal * delta.Dot(targetNormal);
                        const double len = std::sqrt(planeDelta.Dot(planeDelta));
                        if (len < bestPlaneDeltaLen)
                        {
                            bestPlaneDeltaLen = len;
                            bestPlaneDelta = planeDelta;
                        }
                    }
                }

                const TVector movedCenterOnPlane = movedCenter + planeCorrection;
                const TVector centerDelta = target.center - movedCenterOnPlane;
                const TVector centerPlaneDelta =
                    centerDelta - targetNormal * centerDelta.Dot(targetNormal);

                TVector inPlaneCorrection{};
                double inPlaneScore = std::numeric_limits<float>::infinity();
                float overlapRatio = 0.0f;
                if (sectorTarget)
                {
                    overlapRatio = computeProjectedBoundsOverlapRatio(movedVerticesOnPlane, target.vertices, target.normal);
                    if (overlapRatio >= 0.15f)
                    {
                        inPlaneCorrection = TVector{};
                        inPlaneScore = 0.0f;
                    }
                    else if (bestPlaneDeltaLen <= vertexThreshold)
                    {
                        inPlaneCorrection = bestPlaneDelta;
                        inPlaneScore = bestPlaneDeltaLen;
                    }
                    else
                    {
                        continue;
                    }
                }
                else if (bestPlaneDeltaLen <= vertexThreshold)
                {
                    inPlaneCorrection = bestPlaneDelta;
                    inPlaneScore = bestPlaneDeltaLen;
                }
                else if (std::sqrt(centerPlaneDelta.Dot(centerPlaneDelta)) <= centerThreshold)
                {
                    inPlaneCorrection = centerPlaneDelta;
                    inPlaneScore = std::sqrt(centerPlaneDelta.Dot(centerPlaneDelta)) * 0.75f;
                }
                else
                {
                    continue;
                }

                GeometryMoveSnapResult candidate{};
                candidate.translation = rawTranslation + planeCorrection + inPlaneCorrection;
                candidate.rotationAxis = rotationAxis;
                candidate.rotationAngleDeg = rotationAngleDeg;
                candidate.rotationPivot = documentPivot;

                const double score =
                    std::abs(signedPlaneDist) + inPlaneScore + (rotationAngleDeg * 0.02f) - (sectorTarget ? overlapRatio * 0.2f : 0.0f);
                if (!found || score < bestScore)
                {
                    best = candidate;
                    bestScore = score;
                    found = true;
                }
        }
    }

    if (!found)
    {
        return std::nullopt;
    }

    const TVector delta = best.translation - rawTranslation;
    if (delta.x == 0.0 && delta.y == 0.0 && delta.z == 0.0 && best.rotationAngleDeg == 0.0f)
    {
        return std::nullopt;
    }

    return best;
}

bool signedAngleAboutAxis(
    QVector3D from,
    QVector3D to,
    const QVector3D& axisUnit,
    float& outDegrees) noexcept
{
    from -= axisUnit * QVector3D::dotProduct(from, axisUnit);
    to -= axisUnit * QVector3D::dotProduct(to, axisUnit);
    if (!normalizeInPlace(from) || !normalizeInPlace(to))
    {
        return false;
    }

    const float sinv = QVector3D::dotProduct(axisUnit, QVector3D::crossProduct(from, to));
    const float cosv = std::clamp(QVector3D::dotProduct(from, to), -1.0f, 1.0f);
    outDegrees = std::atan2(sinv, cosv) * 180.0f / std::numbers::pi_v<float>;
    return std::isfinite(outDegrees);
}

[[maybe_unused]] std::optional<float> computeGeometryRotateSnapAngle(
    const Level& level,
    const editor::Selection& highlightedSelection,
    const std::int32_t selectedSector,
    const QVector3D& pivot,
    QVector3D rotationAxis,
    const float rawAngleDeg,
    const float minorSpacing) noexcept
{
    if (rotationAxis.lengthSquared() <= sed::math::kTinyEpsilonF)
    {
        return std::nullopt;
    }
    rotationAxis.normalize();

    std::vector<SurfaceRef> movingRefs{};
    std::unordered_set<std::int32_t> movingSectors{};
    std::unordered_set<std::uint64_t> movingRefKeys{};

    auto refKey = [](const SurfaceRef ref) -> std::uint64_t
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32u)
            ^ static_cast<std::uint32_t>(ref.surface);
    };

    auto addSurfaceRef = [&](const SurfaceRef ref)
    {
        if (ref.sector < 0 || ref.surface < 0)
        {
            return;
        }
        const std::uint64_t key = refKey(ref);
        if (!movingRefKeys.insert(key).second)
        {
            return;
        }
        movingRefs.push_back(ref);
        movingSectors.insert(ref.sector);
    };

    std::unordered_set<std::int32_t> sectors = highlightedSelection.sectors;
    if (selectedSector >= 0)
    {
        sectors.insert(selectedSector);
    }

    for (const std::int32_t sectorIndex : sectors)
    {
        if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level.sectors.size())
        {
            continue;
        }

        const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
        for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(sector.surfaces.size()); ++sf)
        {
            addSurfaceRef(SurfaceRef{ sectorIndex, sf });
        }
    }

    if (movingRefs.empty())
    {
        return std::nullopt;
    }

    std::vector<GeometrySnapSurface> movingSurfaces{};
    movingSurfaces.reserve(movingRefs.size());
    for (const SurfaceRef ref : movingRefs)
    {
        GeometrySnapSurface data{};
        if (collectGeometrySnapSurface(level, ref, data))
        {
            movingSurfaces.push_back(std::move(data));
        }
    }
    if (movingSurfaces.empty())
    {
        return std::nullopt;
    }

    const float planeThreshold = std::max(0.12f, minorSpacing * 1.25f);
    const float maxDeltaDeg = 6.0f;
    const float targetAlignmentCos = static_cast<float>(std::cos(deg2Rad(6.0)));

    bool found = false;
    float bestAngleDeg = rawAngleDeg;
    float bestScore = std::numeric_limits<float>::infinity();

    for (const GeometrySnapSurface& moving : movingSurfaces)
    {
        const QVector3D rawNormal = rotateAroundAxis(moving.normal, rotationAxis, static_cast<float>(deg2Rad(rawAngleDeg)));

        for (std::int32_t sectorIndex = 0; sectorIndex < static_cast<std::int32_t>(level.sectors.size()); ++sectorIndex)
        {
            if (movingSectors.contains(sectorIndex))
            {
                continue;
            }

            const Sector& sector = level.sectors[static_cast<std::size_t>(sectorIndex)];
            for (std::int32_t sf = 0; sf < static_cast<std::int32_t>(sector.surfaces.size()); ++sf)
            {
                GeometrySnapSurface target{};
                if (!collectGeometrySnapSurface(level, SurfaceRef{ sectorIndex, sf }, target))
                {
                    continue;
                }

                float deltaDeg = 0.0f;
                if (!signedAngleAboutAxis(rawNormal, -target.normal, rotationAxis, deltaDeg))
                {
                    continue;
                }
                if (std::abs(deltaDeg) > maxDeltaDeg)
                {
                    continue;
                }

                const float candidateAngleDeg = rawAngleDeg + deltaDeg;
                const QVector3D candidateNormal =
                    rotateAroundAxis(moving.normal, rotationAxis, static_cast<float>(deg2Rad(candidateAngleDeg)));
                if (QVector3D::dotProduct(candidateNormal, -target.normal) < targetAlignmentCos)
                {
                    continue;
                }

                const TVector movedCenter = mapSnapPoint(moving.center, render::authoringVector(pivot), TVector{}, rotationAxis, candidateAngleDeg);
                const float planeDist = std::abs((movedCenter - target.point).Dot(render::authoringVector(target.normal)));
                if (planeDist > planeThreshold)
                {
                    continue;
                }

                std::vector<TVector> movedVertices{};
                movedVertices.reserve(moving.vertices.size());
                for (const TVector& v : moving.vertices)
                {
                    movedVertices.push_back(mapSnapPoint(v, render::authoringVector(pivot), TVector{}, rotationAxis, candidateAngleDeg));
                }

                const float overlapRatio = computeProjectedBoundsOverlapRatio(movedVertices, target.vertices, target.normal);
                const float areaRatio = computeProjectedBoundsAreaRatio(movedVertices, target.vertices, target.normal);
                if (overlapRatio < 0.35f || areaRatio < 0.55f)
                {
                    continue;
                }

                const float score =
                    planeDist + std::abs(deltaDeg) * 0.12f - overlapRatio * 0.25f - areaRatio * 0.10f;
                if (!found || score < bestScore)
                {
                    bestScore = score;
                    bestAngleDeg = candidateAngleDeg;
                    found = true;
                }
            }
        }
    }

    if (!found || bestAngleDeg == rawAngleDeg)
    {
        return std::nullopt;
    }

    return bestAngleDeg;
}

} // namespace

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
bool GlView::beginGeometryMoveSnapForTest(const SurfaceRef moving, const SurfaceRef stationary,
    const bool sectorTarget, const TVector& rawTranslation, const TVector& pivot)
{
    if (!level_) return false;
    render::GeometryMoveSnapCache cache{};
    cache.documentOrigin = pivot;
    GeometrySnapSurface first{}, second{};
    if (!collectGeometrySnapSurface(*level_, moving, first, pivot)
        || !collectGeometrySnapSurface(*level_, stationary, second, pivot)) return false;
    cache.movingSurfaces.push_back(first);
    cache.stationarySurfaces.push_back(second);
    const auto snap = computeGeometryMoveSnap(cache, sectorTarget, true, pivot, rawTranslation, 0.02f, 35.0f);
    if (!snap) return false;
    restoreTransformSurfaceUvPreview();
    transformDrag_ = {};
    transformDrag_.active = true;
    transformDrag_.target = sectorTarget ? TransformTargetKind::Sector : TransformTargetKind::Surface;
    transformDrag_.handle = TransformHandle::MoveFree;
    transformDrag_.surfaceMode = SurfaceSubMode::Face;
    transformDrag_.surfaceSector = moving.sector;
    transformDrag_.surfaceIndex = moving.surface;
    transformDrag_.translation = render::displayVector(snap->translation);
    transformDrag_.documentTranslation = snap->translation;
    transformDrag_.snapRotateAxis = snap->rotationAxis;
    transformDrag_.snapRotateAngleDeg = snap->rotationAngleDeg;
    transformDrag_.snapRotatePivot = render::displayVector(snap->rotationPivot);
    transformDrag_.documentSnapRotatePivot = snap->rotationPivot;
    transformPreviewMatrix_.setToIdentity();
    return true;
}
#endif

bool GlView::beginTransformDrag(
    const QPoint& mousePos,
    const QMatrix4x4& proj,
    const QMatrix4x4& view,
    const QVector3D& eye,
    const QVector3D& target,
    const float farPlane)
{
    if (renderPipeline_ != RenderPipeline::Editor || transformTool_ == TransformTool::Select)
    {
        return false;
    }

    if (!level_ || width() <= 0 || height() <= 0)
    {
        return false;
    }

    TransformTargetKind targetKind = TransformTargetKind::Thing;
    std::int32_t activeThing = -1;
    std::int32_t activeLight = -1;
    std::int32_t activeLightOwner = -1;
    render::LightGizmoKind activeLightKind = render::LightGizmoKind::Baked;
    std::int32_t activeFrameThing = -1;
    std::int32_t activeFrameValue = -1;
    const render::frame_ghost::FramePose* activeFramePose = nullptr;
    std::int32_t activeSurfaceSector = -1;
    std::int32_t activeSurfaceIndex = -1;
    std::int32_t activeSurfaceVertex = -1;
    std::int32_t activeSurfaceEdge = -1;
    QVector3D pivot{};
    std::optional<TVector> documentPivot{};

    const QMatrix4x4 mvp = proj * view;
    bool invertible = false;
    const QMatrix4x4 invMvp = mvp.inverted(&invertible);
    if (!invertible)
    {
        return false;
    }

    QVector3D basisX(1.0f, 0.0f, 0.0f);
    QVector3D basisY(0.0f, 1.0f, 0.0f);
    QVector3D basisZ(0.0f, 0.0f, 1.0f);

    auto computeGridLocalBasis = [&](QVector3D& outX, QVector3D& outY, QVector3D& outZ) -> bool
    {
        if (gridAlignToView_)
        {
            outX = gridAlignAxisA_;
            outY = gridAlignAxisB_;
        }
        else if (gridRotateWithView_)
        {
            bool invViewOk = false;
            const QMatrix4x4 invView = view.inverted(&invViewOk);
            if (!invViewOk)
            {
                return false;
            }

            const QVector4D r4 = invView.column(0);
            const QVector4D u4 = invView.column(1);
            outX = QVector3D(r4.x(), r4.y(), r4.z());
            outY = QVector3D(u4.x(), u4.y(), u4.z());
        }
        else
        {
            switch (gridPlane_)
            {
                case GridPlane::XY:
                    outX = QVector3D(1.0f, 0.0f, 0.0f);
                    outY = QVector3D(0.0f, 1.0f, 0.0f);
                    break;
                case GridPlane::XZ:
                    outX = QVector3D(1.0f, 0.0f, 0.0f);
                    outY = QVector3D(0.0f, 0.0f, 1.0f);
                    break;
                case GridPlane::YZ:
                    outX = QVector3D(0.0f, 1.0f, 0.0f);
                    outY = QVector3D(0.0f, 0.0f, 1.0f);
                    break;
            }
        }

        outZ = QVector3D::crossProduct(outX, outY);
        sed::render::normalizeBasis(outX, outY, outZ);
        return sed::render::basisIsUsable(outX, outY, outZ);
    };

    auto computeSectorLocalBasis = [&](QVector3D& outX, QVector3D& outY, QVector3D& outZ) -> bool
    {
        auto sectorSelected = [this](const std::int32_t sectorIndex) -> bool
        {
            return sectorIndex >= 0
                && (sectorIndex == selectedSector_ || highlightedSelection_.sectors.contains(sectorIndex));
        };

        if (selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0 && sectorSelected(selectedSurfaceSector_))
        {
            if (sed::render::computeSurfaceLocalBasis(*level_, SurfaceRef{ selectedSurfaceSector_, selectedSurface_ }, outX, outY, outZ))
            {
                return true;
            }
        }

        if (haveLastSurfaceHit_ && sectorSelected(lastSurfaceHitSurface_.sector))
        {
            if (sed::render::computeSurfaceLocalBasis(*level_, lastSurfaceHitSurface_, outX, outY, outZ))
            {
                return true;
            }
        }

        std::int32_t primarySector = selectedSector_;
        if (primarySector < 0 && !highlightedSelection_.sectors.empty())
        {
            primarySector = *highlightedSelection_.sectors.begin();
        }

        if (primarySector >= 0 && static_cast<std::size_t>(primarySector) < level_->sectors.size())
        {
            const Sector& sector = level_->sectors[static_cast<std::size_t>(primarySector)];
            for (std::int32_t surfaceIndex = 0; surfaceIndex < static_cast<std::int32_t>(sector.surfaces.size()); ++surfaceIndex)
            {
                if (sed::render::computeSurfaceLocalBasis(*level_, SurfaceRef{ primarySector, surfaceIndex }, outX, outY, outZ))
                {
                    return true;
                }
            }
        }

        return computeGridLocalBasis(outX, outY, outZ);
    };

    auto resolveTarget = [&]() -> bool
    {
        auto resolveLight = [&]() -> bool
        {
            activeLight = selectedLight_;
            if (activeLight < 0 && !highlightedSelection_.lights.empty())
            {
                activeLight = *highlightedSelection_.lights.begin();
            }
            if (activeLight < 0 || static_cast<std::size_t>(activeLight) >= level_->lights.size())
            {
                return false;
            }

            const SedLight& light = level_->lights[static_cast<std::size_t>(activeLight)];
            if (!isLayerVisible(light.layer))
            {
                return false;
            }
            if (!std::isfinite(light.position.x) || !std::isfinite(light.position.y) || !std::isfinite(light.position.z))
            {
                return false;
            }

            activeLightOwner = activeLight;
            targetKind = TransformTargetKind::Light;
            documentPivot = light.position;
            pivot = QVector3D(
                static_cast<float>(light.position.x),
                static_cast<float>(light.position.y),
                static_cast<float>(light.position.z));
            return true;
        };

        auto resolveSectorLight = [&]() -> bool
        {
            if (transformTool_ == TransformTool::Rotate)
            {
                return false;
            }
            activeLightOwner = selectedSector_;
            const std::vector<render::LightGizmoEntry> entries = render::sectorLightGizmoEntries(*level_);
            auto entry = std::find_if(entries.begin(), entries.end(), [activeLightOwner](const render::LightGizmoEntry& candidate)
            {
                return candidate.ownerIndex == activeLightOwner;
            });
            if (entry == entries.end() || !isLayerVisible(entry->layer))
            {
                entry = std::find_if(entries.begin(), entries.end(), [this](const render::LightGizmoEntry& candidate)
                {
                    return highlightedSelection_.sectors.contains(candidate.ownerIndex)
                        && isLayerVisible(candidate.layer);
                });
            }
            if (entry == entries.end() || !isLayerVisible(entry->layer))
            {
                return false;
            }
            activeLightOwner = entry->ownerIndex;
            activeLightKind = entry->kind;
            selectedLightGizmoKind_ = entry->kind;
            targetKind = TransformTargetKind::SectorLight;
            documentPivot = level_->sectors[static_cast<std::size_t>(activeLightOwner)].pointLight.position;
            pivot = entry->position;
            return true;
        };

        auto resolveThingLight = [&]() -> bool
        {
            if (transformTool_ == TransformTool::Rotate)
            {
                return false;
            }
            activeLightOwner = selectedThing_;
            if (activeLightOwner < 0 && !highlightedSelection_.things.empty())
            {
                activeLightOwner = *highlightedSelection_.things.begin();
            }
            const std::vector<render::LightGizmoEntry> entries =
                render::thingLightGizmoEntries(*level_, templates_);
            auto entry = std::find_if(entries.begin(), entries.end(), [&](const render::LightGizmoEntry& candidate)
            {
                return candidate.ownerIndex == activeLightOwner
                    && candidate.kind == selectedLightGizmoKind_;
            });
            if (entry == entries.end())
            {
                entry = std::find_if(entries.begin(), entries.end(), [activeLightOwner](const render::LightGizmoEntry& candidate)
                {
                    return candidate.ownerIndex == activeLightOwner;
                });
            }
            if (entry == entries.end() || !isLayerVisible(entry->layer))
            {
                return false;
            }

            activeLightOwner = entry->ownerIndex;
            activeThing = entry->ownerIndex;
            activeLightKind = entry->kind;
            selectedLightGizmoKind_ = entry->kind;
            targetKind = TransformTargetKind::ThingLight;
            pivot = render::thingLightTransformPivot(
                *entry,
                lightGizmoDisplayPositionForEntry(*entry, view));
            const Thing& authoredOwner = level_->things[static_cast<std::size_t>(activeLightOwner)];
            documentPivot = TVector{authoredOwner.x, authoredOwner.y, authoredOwner.z};
            if (activeLightKind == render::LightGizmoKind::ThingHead)
            {
                if (const TplValue* value = level::thingFindValue(*level_, templates_, authoredOwner, "lightoffset"))
                {
                    if (const auto offset = thing::light_authoring::parseAuthoredOffset(*value))
                    {
                        TVector pyr{};
                        pyr.pitch = authoredOwner.pch; pyr.yaw = authoredOwner.yaw; pyr.roll = authoredOwner.rol;
                        *documentPivot = *documentPivot + TMatrix43::CreateRotate(pyr).TransformVector(*offset);
                    }
                }
            }

            if (transformOrientation_ == TransformOrientation::Local
                && activeThing >= 0
                && static_cast<std::size_t>(activeThing) < level_->things.size())
            {
                const Thing& thing = level_->things[static_cast<std::size_t>(activeThing)];
                TVector pyr{};
                pyr.pitch = thing.pch;
                pyr.yaw = thing.yaw;
                pyr.roll = thing.rol;
                const TMatrix43 rot = TMatrix43::CreateRotate(pyr);
                basisX = QVector3D(static_cast<float>(rot.rvec.x), static_cast<float>(rot.rvec.y), static_cast<float>(rot.rvec.z));
                basisY = QVector3D(static_cast<float>(rot.lvec.x), static_cast<float>(rot.lvec.y), static_cast<float>(rot.lvec.z));
                basisZ = QVector3D(static_cast<float>(rot.uvec.x), static_cast<float>(rot.uvec.y), static_cast<float>(rot.uvec.z));
                sed::render::normalizeBasis(basisX, basisY, basisZ);
            }
            return true;
        };

        auto resolveThing = [&]() -> bool
        {
            const bool thingLightOwner =
                pickMode_ == PickMode::Thing
                && lightGizmoCategory_ == render::LightGizmoCategory::Thing;
            if (!showThings_ && !thingLightOwner)
            {
                return false;
            }

            activeThing = selectedThing_;
            if (activeThing < 0 && !highlightedSelection_.things.empty())
            {
                activeThing = *highlightedSelection_.things.begin();
            }
            if (activeThing < 0 || static_cast<std::size_t>(activeThing) >= level_->things.size())
            {
                return false;
            }

            const Thing& thing = level_->things[static_cast<std::size_t>(activeThing)];
            if (!isLayerVisible(thing.layer) || !shouldShowThing(thing))
            {
                return false;
            }
            if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
            {
                return false;
            }
            if (!std::isfinite(thing.pch) || !std::isfinite(thing.yaw) || !std::isfinite(thing.rol))
            {
                return false;
            }

            targetKind = TransformTargetKind::Thing;
            documentPivot = TVector{thing.x, thing.y, thing.z};
            pivot = QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));

            if (transformOrientation_ == TransformOrientation::Local)
            {
                TVector pyr{};
                pyr.pitch = thing.pch;
                pyr.yaw = thing.yaw;
                pyr.roll = thing.rol;
                const TMatrix43 rot = TMatrix43::CreateRotate(pyr);
                basisX = QVector3D(static_cast<float>(rot.rvec.x), static_cast<float>(rot.rvec.y), static_cast<float>(rot.rvec.z));
                basisY = QVector3D(static_cast<float>(rot.lvec.x), static_cast<float>(rot.lvec.y), static_cast<float>(rot.lvec.z));
                basisZ = QVector3D(static_cast<float>(rot.uvec.x), static_cast<float>(rot.uvec.y), static_cast<float>(rot.uvec.z));
                sed::render::normalizeBasis(basisX, basisY, basisZ);
            }

            return true;
        };

        auto resolveFrame = [&]() -> bool
        {
            if (!showThings_)
            {
                return false;
            }

            // Frames are position/orientation points; scaling isn't meaningful here.
            if (transformTool_ == TransformTool::Scale)
            {
                return false;
            }

            const std::int32_t activeThing = selectedThing_;
            if (activeThing < 0 || static_cast<std::size_t>(activeThing) >= level_->things.size())
            {
                return false;
            }

            const Thing& thing = level_->things[static_cast<std::size_t>(activeThing)];
            if (!isLayerVisible(thing.layer) || !shouldShowThing(thing))
            {
                return false;
            }
            if (selectedFrameValueIndex_ < 0 || static_cast<std::size_t>(selectedFrameValueIndex_) >= thing.values.size())
            {
                return false;
            }

            const TplValue& fv = thing.values[static_cast<std::size_t>(selectedFrameValueIndex_)];
            if (fv.type != ValueType::frame || !sed::text::iequalsAscii(fv.name, "frame"))
            {
                return false;
            }
            activeFramePose = findFramePose(activeThing, selectedFrameValueIndex_);
            if (activeFramePose == nullptr || !activeFramePose->finiteOrientation)
            {
                return false;
            }
            const double x = activeFramePose->x;
            const double y = activeFramePose->y;
            const double z = activeFramePose->z;
            const double pch = activeFramePose->pitch;
            const double yaw = activeFramePose->yaw;
            const double rol = activeFramePose->roll;

            targetKind = TransformTargetKind::Frame;
            activeFrameThing = activeThing;
            activeFrameValue = selectedFrameValueIndex_;
            documentPivot = TVector{x, y, z};
            pivot = QVector3D(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));

            if (transformOrientation_ == TransformOrientation::Local)
            {
                TVector pyr{};
                pyr.pitch = pch;
                pyr.yaw = yaw;
                pyr.roll = rol;
                const TMatrix43 rot = TMatrix43::CreateRotate(pyr);
                basisX = QVector3D(static_cast<float>(rot.rvec.x), static_cast<float>(rot.rvec.y), static_cast<float>(rot.rvec.z));
                basisY = QVector3D(static_cast<float>(rot.lvec.x), static_cast<float>(rot.lvec.y), static_cast<float>(rot.lvec.z));
                basisZ = QVector3D(static_cast<float>(rot.uvec.x), static_cast<float>(rot.uvec.y), static_cast<float>(rot.uvec.z));
                sed::render::normalizeBasis(basisX, basisY, basisZ);
            }

            return true;
        };

        auto resolveSector = [&]() -> bool
        {
            TVector mn{};
            TVector mx{};
            bool have = false;

            auto addSector = [&](const std::int32_t sectorIndex)
            {
                if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level_->sectors.size())
                {
                    return;
                }
                const Sector& sector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
                for (const Vertex& v : sector.vertices)
                {
                    if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) continue;
                    if (!have) { mn = TVector{v.x, v.y, v.z}; mx = mn; have = true; }
                    else
                    {
                        mn.x = std::min(mn.x, v.x); mn.y = std::min(mn.y, v.y); mn.z = std::min(mn.z, v.z);
                        mx.x = std::max(mx.x, v.x); mx.y = std::max(mx.y, v.y); mx.z = std::max(mx.z, v.z);
                    }
                }
            };

            for (const std::int32_t s : highlightedSelection_.sectors)
            {
                addSector(s);
            }
            addSector(selectedSector_);

            if (!have)
            {
                return false;
            }

            targetKind = TransformTargetKind::Sector;
            documentPivot = mn + (mx - mn) * 0.5;
            pivot = QVector3D(static_cast<float>(documentPivot->x), static_cast<float>(documentPivot->y), static_cast<float>(documentPivot->z));

            if (transformOrientation_ == TransformOrientation::Local)
            {
                QVector3D sx, sy, sz;
                if (computeSectorLocalBasis(sx, sy, sz))
                {
                    basisX = sx;
                    basisY = sy;
                    basisZ = sz;
                    sed::render::normalizeBasis(basisX, basisY, basisZ);
                }
            }

            return true;
        };

        auto resolveSurface = [&]() -> bool
        {
            QVector3D mn{};
            QVector3D mx{};
            bool have = false;

            SurfaceRef activeRef{};
            bool haveActiveRef = false;
            std::vector<SurfaceRef> selectionSurfaces{};
            selectionSurfaces.reserve(
                1 + highlightedSelection_.surfaceVertices.size()
                + highlightedSelection_.surfaces.size());

            auto addSurface = [&](const SurfaceRef ref)
            {
                if (ref.sector < 0 || ref.surface < 0)
                {
                    return;
                }
                const std::size_t secIndex = static_cast<std::size_t>(ref.sector);
                if (secIndex >= level_->sectors.size())
                {
                    return;
                }
                const Sector& sector = level_->sectors[secIndex];
                const std::size_t surfIndex = static_cast<std::size_t>(ref.surface);
                if (surfIndex >= sector.surfaces.size())
                {
                    return;
                }

                const Surface& surface = sector.surfaces[surfIndex];
                selectionSurfaces.push_back(ref);
                for (const std::int32_t vi : surface.vertices)
                {
                    if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
                    {
                        continue;
                    }
                    const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
                    sed::render::addPointToBounds(mn, mx, have, static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
                }

                if (!haveActiveRef)
                {
                    activeRef = ref;
                    haveActiveRef = true;
                }
            };

            if (selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0)
            {
                addSurface(SurfaceRef{ selectedSurfaceSector_, selectedSurface_ });
            }
            for (const editor::SurfaceVertexRef& ref : highlightedSelection_.surfaceVertices)
            {
                addSurface(ref.surface);
            }
            for (const SurfaceRef ref : highlightedSelection_.surfaces)
            {
                addSurface(ref);
            }

            if (!have)
            {
                return false;
            }

            targetKind = TransformTargetKind::Surface;
            const auto pivotMode = surfaceSubMode_ == SurfaceSubMode::Vertex
                ? render::SurfaceSelectionPivotMode::Vertex
                : surfaceSubMode_ == SurfaceSubMode::Edge
                    ? render::SurfaceSelectionPivotMode::Edge
                    : render::SurfaceSelectionPivotMode::Face;
            std::optional<SurfaceRef> activeSurface = haveActiveRef
                ? std::optional<SurfaceRef>{activeRef}
                : std::nullopt;
            std::optional<SurfaceRef> hitSurface = haveLastSurfaceHit_
                ? std::optional<SurfaceRef>{lastSurfaceHitSurface_}
                : std::nullopt;
            std::optional<TVector> hitPosition = haveLastSurfaceHit_
                ? std::optional<TVector>{lastSurfaceHitPos_} : std::nullopt;
            if (surfaceSubMode_ == SurfaceSubMode::Vertex
                && activeSurfaceVertexHintValid_
                && highlightedSelection_.surfaceVertices.contains(
                    activeSurfaceVertexHint_))
            {
                const editor::SurfaceVertexRef& vertexRef =
                    activeSurfaceVertexHint_;
                if (vertexRef.surface.sector >= 0
                    && vertexRef.surface.surface >= 0
                    && vertexRef.localVertex >= 0
                    && static_cast<std::size_t>(vertexRef.surface.sector)
                        < level_->sectors.size())
                {
                    const Sector& sector = level_->sectors[
                        static_cast<std::size_t>(vertexRef.surface.sector)];
                    if (static_cast<std::size_t>(vertexRef.surface.surface)
                            < sector.surfaces.size())
                    {
                        const Surface& surface = sector.surfaces[
                            static_cast<std::size_t>(vertexRef.surface.surface)];
                        if (static_cast<std::size_t>(vertexRef.localVertex)
                                < surface.vertices.size())
                        {
                            const std::int32_t vertexIndex = surface.vertices[
                                static_cast<std::size_t>(vertexRef.localVertex)];
                            if (vertexIndex >= 0
                                && static_cast<std::size_t>(vertexIndex)
                                    < sector.vertices.size())
                            {
                                const Vertex& vertex = sector.vertices[
                                    static_cast<std::size_t>(vertexIndex)];
                                activeSurface = vertexRef.surface;
                                hitSurface = vertexRef.surface;
                                hitPosition = TVector{vertex.x, vertex.y, vertex.z};
                            }
                        }
                    }
                }
            }
            else if (surfaceSubMode_ == SurfaceSubMode::Edge
                && activeSurfaceEdge_
                && selectedSurfaceEdges_.contains(*activeSurfaceEdge_))
            {
                if (const std::optional<TVector> midpoint =
                        surfaceEdgeMidpoint(*activeSurfaceEdge_))
                {
                    activeSurface = activeSurfaceEdge_->surface;
                    hitSurface = activeSurfaceEdge_->surface;
                    hitPosition = *midpoint;
                }
            }
            const std::optional<TVector> resolvedPivot = render::surfaceSelectionDocumentPivot(
                *level_, selectionSurfaces, activeSurface, pivotMode, hitSurface, hitPosition);
            if (!resolvedPivot)
            {
                return false;
            }
            documentPivot = *resolvedPivot;
            pivot = QVector3D(static_cast<float>(resolvedPivot->x), static_cast<float>(resolvedPivot->y), static_cast<float>(resolvedPivot->z));

            if (transformOrientation_ == TransformOrientation::Local
                && activeSurface)
            {
                QVector3D sx, sy, sz;
                if (sed::render::computeSurfaceLocalBasis(
                        *level_, *activeSurface, sx, sy, sz))
                {
                    basisX = sx;
                    basisY = sy;
                    basisZ = sz;
                    sed::render::normalizeBasis(basisX, basisY, basisZ);
                }
            }

            return true;
        };

        if (frameEditEnabled_)
        {
            return resolveFrame();
        }

        switch (pickMode_)
        {
            case PickMode::Sector:
                return lightGizmoEditingActive_
                        && lightGizmoCategory_ == render::LightGizmoCategory::Sector
                    ? resolveSectorLight()
                    : resolveSector();
            case PickMode::Surface:
                return resolveSurface();
            case PickMode::Thing:
                return lightGizmoEditingActive_
                        && lightGizmoCategory_ == render::LightGizmoCategory::Thing
                    ? resolveThingLight()
                    : resolveThing();
            case PickMode::Light:
                return resolveLight();
            default:
                break;
        }

        if (resolveLight())
        {
            return true;
        }
        if (resolveThing())
        {
            return true;
        }
        if (resolveSurface())
        {
            return true;
        }
        return resolveSector();
    };

    if (!resolveTarget())
    {
        return false;
    }

    render::GeometryMoveSnapCache geometryMoveSnapCache{};
    render::ThingMoveSnapCache thingMoveSnapCache{};
    render::FrameMovePoseSnapshot frameMovePoseSnapshot{};

    const ProjectedPoint center2d = projectWorldToWidget(mvp, pivot, width(), height());
    if (!center2d.valid)
    {
        return false;
    }

    float unitsPerPx = worldUnitsPerPixelAt(pivot, mvp, invMvp, width(), height());
    if (!std::isfinite(unitsPerPx) || unitsPerPx <= 0.0f)
    {
        unitsPerPx = 0.01f;
    }

    TVector gridSnapReference{pivot.x(), pivot.y(), pivot.z()};
    std::vector<TVector> gridStickPoints{};

    auto documentVertexPoint = [&](const std::int32_t sectorIndex, const std::int32_t vertexIndex) -> std::optional<TVector>
    {
        if (sectorIndex < 0 || vertexIndex < 0
            || static_cast<std::size_t>(sectorIndex) >= level_->sectors.size())
        {
            return std::nullopt;
        }
        const Sector& sector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
        if (static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
        {
            return std::nullopt;
        }
        const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z))
        {
            return std::nullopt;
        }
        return TVector{vertex.x, vertex.y, vertex.z};
    };
    const auto& vertexPoint = documentVertexPoint;

    auto validSurface = [&](const SurfaceRef ref) -> bool
    {
        return ref.sector >= 0 && ref.surface >= 0
            && static_cast<std::size_t>(ref.sector) < level_->sectors.size()
            && static_cast<std::size_t>(ref.surface)
                < level_->sectors[static_cast<std::size_t>(ref.sector)].surfaces.size();
    };

    auto surfaceVertexPoint = [&](const editor::SurfaceVertexRef& ref) -> std::optional<std::pair<std::int32_t, TVector>>
    {
        if (!validSurface(ref.surface) || ref.localVertex < 0)
        {
            return std::nullopt;
        }
        const Sector& sector = level_->sectors[static_cast<std::size_t>(ref.surface.sector)];
        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface.surface)];
        if (static_cast<std::size_t>(ref.localVertex) >= surface.vertices.size())
        {
            return std::nullopt;
        }
        const std::int32_t vertexIndex = surface.vertices[static_cast<std::size_t>(ref.localVertex)];
        const std::optional<TVector> point = vertexPoint(ref.surface.sector, vertexIndex);
        if (!point)
        {
            return std::nullopt;
        }
        return std::pair{ vertexIndex, *point };
    };

    auto firstSelectedSurface = [&]() -> std::optional<SurfaceRef>
    {
        const SurfaceRef selected{ selectedSurfaceSector_, selectedSurface_ };
        if (validSurface(selected))
        {
            return selected;
        }
        std::optional<SurfaceRef> best{};
        auto consider = [&](const SurfaceRef ref)
        {
            if (!validSurface(ref))
            {
                return;
            }
            if (!best || ref.sector < best->sector
                || (ref.sector == best->sector && ref.surface < best->surface))
            {
                best = ref;
            }
        };
        for (const SurfaceRef ref : highlightedSelection_.surfaces)
        {
            consider(ref);
        }
        for (const editor::SurfaceVertexRef& ref : highlightedSelection_.surfaceVertices)
        {
            consider(ref.surface);
        }
        if (haveLastSurfaceHit_)
        {
            consider(lastSurfaceHitSurface_);
        }
        return best;
    };

    auto appendSurfacePoints = [&](const SurfaceRef ref)
    {
        if (!validSurface(ref))
        {
            return;
        }
        const Sector& sector = level_->sectors[static_cast<std::size_t>(ref.sector)];
        const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
        for (const std::int32_t vertexIndex : surface.vertices)
        {
            if (const std::optional<TVector> point = vertexPoint(ref.sector, vertexIndex))
            {
                gridStickPoints.push_back(*point);
            }
        }
    };

    switch (targetKind)
    {
        case TransformTargetKind::Thing:
        {
            const std::vector<std::int32_t> things = sed::thing::uniqueValidThingIndices(
                highlightedSelection_.things, activeThing, level_->things.size());
            const TVector cacheOrigin = documentPivot.value_or(
                render::authoringVector(pivot));
            thingMoveSnapCache.documentOrigin = cacheOrigin;
            thingMoveSnapCache.movingIndices = things;
            thingMoveSnapCache.entries.reserve(level_->things.size());
            thingMoveSnapCache.entryByIndex.reserve(level_->things.size());

            for (std::size_t thingIndex = 0; thingIndex < level_->things.size(); ++thingIndex)
            {
                const Thing& thing = level_->things[thingIndex];
                render::ThingMoveSnapEntry entry{};
                entry.index = static_cast<std::int32_t>(thingIndex);
                entry.relativeOrigin = TVector{
                    thing.x - cacheOrigin.x,
                    thing.y - cacheOrigin.y,
                    thing.z - cacheOrigin.z};
                entry.pose.pitch = thing.pch;
                entry.pose.yaw = thing.yaw;
                entry.pose.roll = thing.rol;
                entry.visible = isLayerVisible(thing.layer)
                    && shouldShowThing(thing);
                entry.finite = sed::thing::finiteThingTransform(thing);

                const std::size_t entryIndex = thingMoveSnapCache.entries.size();
                thingMoveSnapCache.entryByIndex.emplace(
                    entry.index, entryIndex);
                thingMoveSnapCache.entries.push_back(std::move(entry));
            }

            for (const std::int32_t thingIndex : things)
            {
                const Thing& thing = level_->things[static_cast<std::size_t>(thingIndex)];
                if (thingIndex == activeThing)
                {
                    gridSnapReference = TVector{thing.x, thing.y, thing.z};
                }
            }
            for (const std::int32_t thingIndex : thingMoveSnapCache.movingIndices)
            {
                if (const auto it = thingMoveSnapCache.entryByIndex.find(thingIndex);
                    it != thingMoveSnapCache.entryByIndex.end())
                {
                    thingMoveSnapCache.movingEntries.push_back(it->second);
                }
            }
            break;
        }
        case TransformTargetKind::Frame:
        {
            gridSnapReference = documentPivot.value_or(render::authoringVector(pivot));
            if (activeFrameThing >= 0
                && activeFrameValue >= 0
                && static_cast<std::size_t>(activeFrameThing) < level_->things.size()
                && activeFramePose != nullptr)
            {
                const Thing& owner = level_->things[static_cast<std::size_t>(activeFrameThing)];
                Thing virtualPose = owner;
                std::erase_if(
                    virtualPose.values,
                    [](const TplValue& value)
                    {
                        return value.type == ValueType::frame
                            && sed::text::iequalsAscii(value.name, "frame");
                    });
                virtualPose.x = activeFramePose->x;
                virtualPose.y = activeFramePose->y;
                virtualPose.z = activeFramePose->z;
                virtualPose.pch = activeFramePose->pitch;
                virtualPose.yaw = activeFramePose->yaw;
                virtualPose.rol = activeFramePose->roll;

                frameMovePoseSnapshot.ownerThingIndex = activeFrameThing;
                frameMovePoseSnapshot.frameValueIndex = activeFrameValue;
                frameMovePoseSnapshot.virtualPose = std::move(virtualPose);
                frameMovePoseSnapshot.valid = true;

                const TVector cacheOrigin{
                    frameMovePoseSnapshot.virtualPose.x,
                    frameMovePoseSnapshot.virtualPose.y,
                    frameMovePoseSnapshot.virtualPose.z};
                thingMoveSnapCache.documentOrigin = cacheOrigin;
                thingMoveSnapCache.entries.reserve(level_->things.size());
                thingMoveSnapCache.entryByIndex.reserve(level_->things.size());
                for (std::size_t thingIndex = 0; thingIndex < level_->things.size(); ++thingIndex)
                {
                    const Thing& thing = level_->things[thingIndex];
                    render::ThingMoveSnapEntry entry{};
                    entry.index = static_cast<std::int32_t>(thingIndex);
                    entry.relativeOrigin = TVector{
                        thing.x - cacheOrigin.x,
                        thing.y - cacheOrigin.y,
                        thing.z - cacheOrigin.z};
                    entry.pose.pitch = thing.pch;
                    entry.pose.yaw = thing.yaw;
                    entry.pose.roll = thing.rol;
                    entry.visible = isLayerVisible(thing.layer)
                        && shouldShowThing(thing);
                    entry.finite = sed::thing::finiteThingTransform(thing);
                    const std::size_t entryIndex = thingMoveSnapCache.entries.size();
                    thingMoveSnapCache.entryByIndex.emplace(entry.index, entryIndex);
                    thingMoveSnapCache.entries.push_back(std::move(entry));
                }

                gridSnapReference = cacheOrigin;
                gridStickPoints = sed::thing::resolveThingDocumentSupportPoints(
                    *level_, templates_, resources_,
                    frameMovePoseSnapshot.virtualPose,
                    static_cast<double>(thingCubeHalfSize_));
                frameMovePoseSnapshot.supportPoints.reserve(gridStickPoints.size());
                for (const TVector& point : gridStickPoints)
                {
                    frameMovePoseSnapshot.supportPoints.push_back(point - cacheOrigin);
                }
                if (frameMovePoseSnapshot.supportPoints.empty())
                {
                    frameMovePoseSnapshot.supportPoints.push_back(TVector{});
                }
                if (render::ThingMoveSnapEntry* ownerEntry =
                        thingMoveSnapCache.find(activeFrameThing))
                {
                    ownerEntry->surfaceStickPoints = frameMovePoseSnapshot.supportPoints;
                    ownerEntry->surfaceStickPointsReady = true;
                }
            }
            if (gridStickPoints.empty())
            {
                gridStickPoints.push_back(documentPivot.value_or(render::authoringVector(pivot)));
            }
            break;
        }
        case TransformTargetKind::Light:
        {
            auto addLight = [&](const std::int32_t lightIndex)
            {
                if (lightIndex < 0 || static_cast<std::size_t>(lightIndex) >= level_->lights.size())
                {
                    return;
                }
                const SedLight& light = level_->lights[static_cast<std::size_t>(lightIndex)];
                const TVector point = light.position;
                gridStickPoints.push_back(point);
                if (lightIndex == activeLight)
                {
                    gridSnapReference = light.position;
                }
            };
            for (const std::int32_t lightIndex : highlightedSelection_.lights)
            {
                addLight(lightIndex);
            }
            addLight(activeLight);
            break;
        }
        case TransformTargetKind::ThingLight:
        case TransformTargetKind::SectorLight:
            gridSnapReference = documentPivot.value_or(render::authoringVector(pivot));
            gridStickPoints.push_back(documentPivot.value_or(render::authoringVector(pivot)));
            break;
        case TransformTargetKind::Sector:
        {
            std::int32_t referenceSector = selectedSector_;
            if (referenceSector < 0 || static_cast<std::size_t>(referenceSector) >= level_->sectors.size())
            {
                referenceSector = -1;
                for (const std::int32_t sectorIndex : highlightedSelection_.sectors)
                {
                    if (sectorIndex >= 0 && static_cast<std::size_t>(sectorIndex) < level_->sectors.size()
                        && (referenceSector < 0 || sectorIndex < referenceSector))
                    {
                        referenceSector = sectorIndex;
                    }
                }
            }

            auto addSector = [&](const std::int32_t sectorIndex)
            {
                if (sectorIndex < 0 || static_cast<std::size_t>(sectorIndex) >= level_->sectors.size())
                {
                    return;
                }
                const Sector& sector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
                for (std::size_t vertexIndex = 0; vertexIndex < sector.vertices.size(); ++vertexIndex)
                {
                    if (const std::optional<TVector> point = vertexPoint(
                            sectorIndex, static_cast<std::int32_t>(vertexIndex)))
                    {
                        gridStickPoints.push_back(*point);
                    }
                }
            };
            for (const std::int32_t sectorIndex : highlightedSelection_.sectors)
            {
                addSector(sectorIndex);
            }
            addSector(selectedSector_);

            if (referenceSector >= 0 && !level_->sectors[static_cast<std::size_t>(referenceSector)].vertices.empty())
            {
                if (const std::optional<TVector> point = vertexPoint(referenceSector, 0))
                {
                    gridSnapReference = *documentVertexPoint(referenceSector, 0);
                }
            }
            break;
        }
        case TransformTargetKind::Surface:
        {
            const std::optional<SurfaceRef> activeRef = firstSelectedSurface();
            if (activeRef)
            {
                activeSurfaceSector = activeRef->sector;
                activeSurfaceIndex = activeRef->surface;
            }

            if (surfaceSubMode_ == SurfaceSubMode::Vertex)
            {
                std::optional<editor::SurfaceVertexRef> activeVertexRef{};
                auto consider = [&](const editor::SurfaceVertexRef& ref)
                {
                    if (!surfaceVertexPoint(ref))
                    {
                        return;
                    }
                    if (!activeVertexRef)
                    {
                        activeVertexRef = ref;
                        return;
                    }

                    const int refRank = activeRef && ref.surface == *activeRef ? 0 : 1;
                    const int currentRank = activeRef && activeVertexRef->surface == *activeRef ? 0 : 1;
                    if (refRank < currentRank
                        || (refRank == currentRank
                            && (ref.surface.sector < activeVertexRef->surface.sector
                                || (ref.surface.sector == activeVertexRef->surface.sector
                                    && (ref.surface.surface < activeVertexRef->surface.surface
                                        || (ref.surface.surface == activeVertexRef->surface.surface
                                            && ref.localVertex < activeVertexRef->localVertex))))))
                    {
                        activeVertexRef = ref;
                    }
                };

                bool activeHintLocked = false;
                if (activeSurfaceVertexHintValid_
                    && highlightedSelection_.surfaceVertices.contains(activeSurfaceVertexHint_)
                    && surfaceVertexPoint(activeSurfaceVertexHint_))
                {
                    activeVertexRef = activeSurfaceVertexHint_;
                    activeHintLocked = true;
                }
                for (const editor::SurfaceVertexRef& ref : highlightedSelection_.surfaceVertices)
                {
                    if (!activeHintLocked)
                    {
                        consider(ref);
                    }
                    if (const auto resolved = surfaceVertexPoint(ref))
                    {
                        gridStickPoints.push_back(resolved->second);
                    }
                }

                if (!activeVertexRef && activeRef && haveLastSurfaceHit_
                    && lastSurfaceHitSurface_ == *activeRef)
                {
                    const Sector& sector = level_->sectors[static_cast<std::size_t>(activeRef->sector)];
                    const Surface& surface = sector.surfaces[static_cast<std::size_t>(activeRef->surface)];
                    double bestDistance = std::numeric_limits<double>::infinity();
                    for (std::size_t localVertex = 0; localVertex < surface.vertices.size(); ++localVertex)
                    {
                        const std::int32_t vertexIndex = surface.vertices[localVertex];
                        if (const std::optional<TVector> point = vertexPoint(activeRef->sector, vertexIndex))
                        {
                            const TVector delta = *point - lastSurfaceHitPos_;
                            const double distance = delta.Dot(delta);
                            if (distance < bestDistance)
                            {
                                bestDistance = distance;
                                activeVertexRef = editor::SurfaceVertexRef{
                                    *activeRef, static_cast<std::int32_t>(localVertex) };
                            }
                        }
                    }
                }

                if (activeVertexRef)
                {
                    if (const auto resolved = surfaceVertexPoint(*activeVertexRef))
                    {
                        activeSurfaceSector = activeVertexRef->surface.sector;
                        activeSurfaceIndex = activeVertexRef->surface.surface;
                        activeSurfaceVertex = resolved->first;
                        gridSnapReference = *documentVertexPoint(activeSurfaceSector, resolved->first);
                        if (gridStickPoints.empty())
                        {
                            gridStickPoints.push_back(resolved->second);
                        }
                    }
                }
            }
            else if (surfaceSubMode_ == SurfaceSubMode::Edge && activeRef)
            {
                const Sector& sector = level_->sectors[static_cast<std::size_t>(activeRef->sector)];
                const Surface& surface = sector.surfaces[static_cast<std::size_t>(activeRef->surface)];
                if (surface.vertices.size() >= 2)
                {
                    activeSurfaceEdge = 0;
                    const bool exactActiveEdge = activeSurfaceEdge_
                        && selectedSurfaceEdges_.contains(*activeSurfaceEdge_)
                        && activeSurfaceEdge_->surface == *activeRef
                        && activeSurfaceEdge_->localEdge >= 0
                        && static_cast<std::size_t>(activeSurfaceEdge_->localEdge)
                            < surface.vertices.size();
                    if (exactActiveEdge)
                    {
                        activeSurfaceEdge = activeSurfaceEdge_->localEdge;
                    }
                    else if (haveLastSurfaceHit_ && lastSurfaceHitSurface_ == *activeRef)
                    {
                        double bestDistance = std::numeric_limits<double>::infinity();
                        for (std::size_t edge = 0; edge < surface.vertices.size(); ++edge)
                        {
                            const std::optional<TVector> a = vertexPoint(activeRef->sector, surface.vertices[edge]);
                            const std::optional<TVector> b = vertexPoint(
                                activeRef->sector, surface.vertices[(edge + 1) % surface.vertices.size()]);
                            if (!a || !b)
                            {
                                continue;
                            }
                            const TVector ab = *b - *a;
                            const double ab2 = ab.Dot(ab);
                            if (!std::isfinite(ab2) || ab2 == 0.0)
                            {
                                continue;
                            }
                            const TVector hit = lastSurfaceHitPos_;
                            const double t = std::clamp((hit - *a).Dot(ab) / ab2, 0.0, 1.0);
                            const TVector delta = hit - (*a + ab * t);
                            const double distance = delta.Dot(delta);
                            if (distance < bestDistance)
                            {
                                bestDistance = distance;
                                activeSurfaceEdge = static_cast<std::int32_t>(edge);
                            }
                        }
                    }

                    const std::size_t edge = static_cast<std::size_t>(activeSurfaceEdge);
                    if (const std::optional<TVector> a = vertexPoint(activeRef->sector, surface.vertices[edge]))
                    {
                        gridSnapReference = *documentVertexPoint(activeRef->sector, surface.vertices[edge]);
                        gridStickPoints.push_back(*a);
                    }
                    if (const std::optional<TVector> b = vertexPoint(
                            activeRef->sector, surface.vertices[(edge + 1) % surface.vertices.size()]))
                    {
                        gridStickPoints.push_back(*b);
                    }
                }
            }
            else if (activeRef)
            {
                for (const SurfaceRef ref : highlightedSelection_.surfaces)
                {
                    appendSurfacePoints(ref);
                }
                appendSurfacePoints(SurfaceRef{ selectedSurfaceSector_, selectedSurface_ });
                if (gridStickPoints.empty())
                {
                    appendSurfacePoints(*activeRef);
                }

                const Sector& sector = level_->sectors[static_cast<std::size_t>(activeRef->sector)];
                const Surface& surface = sector.surfaces[static_cast<std::size_t>(activeRef->surface)];
                if (!surface.vertices.empty())
                {
                    if (const std::optional<TVector> point = vertexPoint(activeRef->sector, surface.vertices.front()))
                    {
                        gridSnapReference = *documentVertexPoint(activeRef->sector, surface.vertices.front());
                    }
                }
            }
            break;
        }
    }

    // Capture stationary geometry once after the exact active Vertex/Edge has
    // been resolved. The preview does not mutate Level until commit, so every
    // live sample can use this compact cache without rescanning the document.
    const bool geometrySnapTarget =
        targetKind == TransformTargetKind::Sector
        || targetKind == TransformTargetKind::Surface;
    if (geometrySnapTarget)
    {
        ++geometryMoveSnapCacheBuildCount_;
        geometryMoveSnapCache = buildGeometryMoveSnapCache(
            *level_,
            targetKind == TransformTargetKind::Sector,
            surfaceSubMode_,
            highlightedSelection_,
            selectedSector_,
            selectedSurfaceSector_,
            selectedSurface_,
            activeSurfaceSector,
            activeSurfaceIndex,
            activeSurfaceVertex,
            activeSurfaceEdge,
            hiddenLayers_,
            lockedLayers_,
            std::max(1.0f, gridMajorSpacing_ * 2.0f),
            documentPivot.value_or(render::authoringVector(pivot)));
    }

    // Freeze the same authoritative lattice used by the renderer and cleave.
    // Keeping this resolver shared is important at edge-on camera angles:
    // direct centre-ray intersections can fail there, and the orthographic /
    // deterministic footprint fallbacks still need to select the same
    // adaptive spacing that is visibly drawn.
    const bool isOrtho = camera_.projectionType()
        == app::ViewportCamera::ProjectionType::Orthographic;
    const render::TransformGridFrame transformGridFrame =
        activeTransformGridFrame(view, mvp, invMvp, eye, target, isOrtho);

    constexpr float kTolPx = 9.0f;
    const float tol2 = kTolPx * kTolPx;
    const QPointF mp(mousePos);

    TransformHandle handle = TransformHandle::None;
    QVector3D axis(0.0f, 0.0f, 0.0f);

    auto selectHandle = [&](const TransformHandle h, const QVector3D& a)
    {
        handle = h;
        axis = a;
    };

    if (transformTool_ == TransformTool::Move)
    {
        constexpr float kCenterRadiusPx = 18.0f;
        const QPointF dc = mp - center2d.pos;
        const float dc2 = static_cast<float>(dc.x() * dc.x() + dc.y() * dc.y());
        if (dc2 <= kCenterRadiusPx * kCenterRadiusPx)
        {
            selectHandle(TransformHandle::MoveFree, (target - eye));
        }
        else
        {
            // Match drawTransformGizmo() sizing so the arrow head can be clicked reliably.
            constexpr float kAxisLenPx = 127.0f;
            constexpr float kAxisHeadRadiusPx = 18.0f;
            const float headTol2 = kAxisHeadRadiusPx * kAxisHeadRadiusPx;
            struct AxisCandidate
            {
                TransformHandle handle{ TransformHandle::None };
                QVector3D dir{};
                float dist2{ std::numeric_limits<float>::infinity() };
            };

            AxisCandidate best{};
            const std::array<std::pair<TransformHandle, QVector3D>, 3> axes = {
                std::pair{ TransformHandle::MoveX, basisX },
                std::pair{ TransformHandle::MoveY, basisY },
                std::pair{ TransformHandle::MoveZ, basisZ },
            };

            for (const auto& [h, dir] : axes)
            {
                const QVector3D endW = pivot + dir * (unitsPerPx * kAxisLenPx);
                const ProjectedPoint end2d = projectWorldToWidget(mvp, endW, width(), height());
                if (!end2d.valid)
                {
                    continue;
                }

                const float d2Line = distanceSqPointToSegment(mp, center2d.pos, end2d.pos);
                const QPointF dh = mp - end2d.pos;
                const float d2Head = static_cast<float>(dh.x() * dh.x() + dh.y() * dh.y());
                const float d2 = std::min(d2Line, d2Head);

                if ((d2Line <= tol2 || d2Head <= headTol2) && d2 < best.dist2)
                {
                    best.handle = h;
                    best.dir = dir;
                    best.dist2 = d2;
                }
            }

            if (best.handle != TransformHandle::None)
            {
                selectHandle(best.handle, best.dir);
            }
        }
    }
    else if (transformTool_ == TransformTool::Scale)
    {
        // Light scaling adjusts range with a camera-facing handle.
        if (targetKind == TransformTargetKind::Light
            || targetKind == TransformTargetKind::ThingLight
            || targetKind == TransformTargetKind::SectorLight)
        {
            QMatrix4x4 invView{};
            bool invOk = false;
            invView = view.inverted(&invOk);
            QVector3D camRight(invView(0, 0), invView(1, 0), invView(2, 0));
            if (!invOk || camRight.lengthSquared() < sed::math::kTinyEpsilonF)
            {
                camRight = QVector3D(1.0f, 0.0f, 0.0f);
            }
            else
            {
                camRight.normalize();
            }

            const float axisLenW = unitsPerPx * 127.0f;
            float range = axisLenW * 0.30f;
            if (level_ && activeLight >= 0 && static_cast<std::size_t>(activeLight) < level_->lights.size())
            {
                const SedLight& light = level_->lights[static_cast<std::size_t>(activeLight)];
                if (!isLayerVisible(light.layer))
                {
                    return false;
                }
                const float r = static_cast<float>(light.range);
                if (std::isfinite(r) && r > 0.0f)
                {
                    range = r;
                }
            }
            else if (level_ && targetKind != TransformTargetKind::Light)
            {
                const std::vector<render::LightGizmoEntry> entries =
                    targetKind == TransformTargetKind::ThingLight
                        ? render::thingLightGizmoEntries(*level_, templates_)
                        : render::sectorLightGizmoEntries(*level_);
                const auto entry = std::find_if(entries.begin(), entries.end(), [&](const render::LightGizmoEntry& candidate)
                {
                    return candidate.ownerIndex == activeLightOwner
                        && (targetKind != TransformTargetKind::ThingLight
                            || candidate.kind == activeLightKind);
                });
                if (entry != entries.end() && isLayerVisible(entry->layer))
                {
                    const float r = std::max(entry->minRadius, entry->maxRadius);
                    if (std::isfinite(r) && r > 0.0f)
                    {
                        range = r;
                    }
                }
            }

            const float lenW = std::clamp(range * 0.80f, axisLenW * 0.25f, axisLenW);
            const QVector3D endW = pivot + camRight * lenW;
            const ProjectedPoint end2d = projectWorldToWidget(mvp, endW, width(), height());
            if (end2d.valid)
            {
                constexpr float kHeadRadiusPx = 18.0f;
                const float headTol2 = kHeadRadiusPx * kHeadRadiusPx;

                const float d2Line = distanceSqPointToSegment(mp, center2d.pos, end2d.pos);
                const QPointF dh = mp - end2d.pos;
                const float d2Head = static_cast<float>(dh.x() * dh.x() + dh.y() * dh.y());

                if (d2Line <= tol2 || d2Head <= headTol2)
                {
                    selectHandle(TransformHandle::ScaleUniform, camRight);
                }
            }
        }
        else
        {
            // Uniform scale handle at the center (click the square).
            constexpr float kCenterRadiusPx = 18.0f;
            const QPointF dc = mp - center2d.pos;
            const float dc2 = static_cast<float>(dc.x() * dc.x() + dc.y() * dc.y());
            if (dc2 <= kCenterRadiusPx * kCenterRadiusPx)
            {
                selectHandle(TransformHandle::ScaleUniform, QVector3D(0.0f, 0.0f, 0.0f));
            }
            else
            {
                // Axis handles: world-space only (Scale X/Y/Z).
                constexpr float kAxisLenPx = 127.0f;
                constexpr float kAxisHeadRadiusPx = 18.0f;
                const float headTol2 = kAxisHeadRadiusPx * kAxisHeadRadiusPx;

                struct AxisCandidate
                {
                    TransformHandle handle{ TransformHandle::None };
                    QVector3D dir{};
                    float dist2{ std::numeric_limits<float>::infinity() };
                };

                AxisCandidate best{};
                const std::array<std::pair<TransformHandle, QVector3D>, 3> axes = {
                    std::pair{ TransformHandle::ScaleX, QVector3D(1.0f, 0.0f, 0.0f) },
                    std::pair{ TransformHandle::ScaleY, QVector3D(0.0f, 1.0f, 0.0f) },
                    std::pair{ TransformHandle::ScaleZ, QVector3D(0.0f, 0.0f, 1.0f) },
                };

                for (const auto& [h, dir] : axes)
                {
                    const QVector3D endW = pivot + dir * (unitsPerPx * kAxisLenPx);
                    const ProjectedPoint end2d = projectWorldToWidget(mvp, endW, width(), height());
                    if (!end2d.valid)
                    {
                        continue;
                    }

                    const float d2Line = distanceSqPointToSegment(mp, center2d.pos, end2d.pos);
                    const QPointF dh = mp - end2d.pos;
                    const float d2Head = static_cast<float>(dh.x() * dh.x() + dh.y() * dh.y());
                    const float d2 = std::min(d2Line, d2Head);

                    if ((d2Line <= tol2 || d2Head <= headTol2) && d2 < best.dist2)
                    {
                        best.handle = h;
                        best.dir = dir;
                        best.dist2 = d2;
                    }
                }

                if (best.handle != TransformHandle::None)
                {
                    selectHandle(best.handle, best.dir);
                }
            }
        }
    }
    else if (transformTool_ == TransformTool::Rotate)
    {
        constexpr int kSegs = 48;
        constexpr float kRingRadiusPx = 104.0f;
        constexpr float kViewRingRadiusPx = 127.0f;

        auto ringDist2 = [&](const QVector3D& u, const QVector3D& v, const float radiusPx) -> float
        {
            const float r = unitsPerPx * radiusPx;
            float best = std::numeric_limits<float>::infinity();

            bool havePrev = false;
            QPointF prev{};
            for (int i = 0; i <= kSegs; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(kSegs);
                const float a = t * 2.0f * std::numbers::pi_v<float>;
                const float ca = std::cos(a);
                const float sa = std::sin(a);
                const QVector3D w = pivot + (u * (ca * r)) + (v * (sa * r));
                const ProjectedPoint p = projectWorldToWidget(mvp, w, width(), height());
                if (!p.valid)
                {
                    havePrev = false;
                    continue;
                }

                if (havePrev)
                {
                    best = std::min(best, distanceSqPointToSegment(mp, prev, p.pos));
                }
                prev = p.pos;
                havePrev = true;
            }

            return best;
        };

        struct RotCandidate
        {
            TransformHandle handle{ TransformHandle::None };
            QVector3D axis{};
            float dist2{ std::numeric_limits<float>::infinity() };
        };

        RotCandidate best{};

        // View ring in screen space.
        {
            const QPointF d = mp - center2d.pos;
            const float dist = std::sqrt(static_cast<float>(d.x() * d.x() + d.y() * d.y()));
            const float err = std::abs(dist - kViewRingRadiusPx);
            const float err2 = err * err;
            if (err2 <= tol2)
            {
                best.handle = TransformHandle::RotateView;
                best.axis = (target - eye);
                best.dist2 = err2;
            }
        }

        const std::array<RotCandidate, 3> ringCandidates = {
            RotCandidate{ TransformHandle::RotateX, basisX, ringDist2(basisY, basisZ, kRingRadiusPx * 1.00f) },
            RotCandidate{ TransformHandle::RotateY, basisY, ringDist2(basisX, basisZ, kRingRadiusPx * 0.94f) },
            RotCandidate{ TransformHandle::RotateZ, basisZ, ringDist2(basisX, basisY, kRingRadiusPx * 1.08f) },
        };

        for (const RotCandidate& cand : ringCandidates)
        {
            if (cand.dist2 <= tol2 && cand.dist2 < best.dist2)
            {
                best = cand;
            }
        }

        // Free rotate inside the sphere (Blender-like): click inside the rings, but not on them.
        if (best.handle == TransformHandle::None)
        {
            const QPointF d = mp - center2d.pos;
            const float dist2 = static_cast<float>(d.x() * d.x() + d.y() * d.y());
            constexpr float kFreeRotateRadiusPx = kRingRadiusPx - (2.0f * kTolPx);
            if (dist2 <= kFreeRotateRadiusPx * kFreeRotateRadiusPx)
            {
                best.handle = TransformHandle::RotateView;
                best.axis = (target - eye);
                best.dist2 = dist2;
            }
        }

        if (best.handle != TransformHandle::None)
        {
            selectHandle(best.handle, best.axis);
        }
    }

    if (handle == TransformHandle::None)
    {
        return false;
    }

    const auto documentRay = render::viewportRayForWidget(
        proj, view, eye, mousePos, width(), height(), isOrtho);
    if (!documentRay)
    {
        return false;
    }
    const double raySegmentMaxDistance = render::viewportRayMaxDistanceForWidget(
        proj, view, QPointF(mousePos), width(), height(), isOrtho)
        .value_or(std::max(0.0, static_cast<double>(farPlane)));

    QVector3D axisNorm = axis;
    if (axisNorm.lengthSquared() > sed::math::kTinyEpsilonF)
    {
        axisNorm.normalize();
    }

    QVector3D startHit{};
    std::optional<TVector> documentStartHit{};
    if (handle == TransformHandle::ScaleUniform && axisNorm.lengthSquared() < sed::math::kTinyEpsilonF)
    {
        // Uniform scale uses mouse delta; no ray/plane hit needed.
        startHit = pivot;
    }
    else if (handle == TransformHandle::MoveFree)
    {
        const TVector n{axisNorm.x(), axisNorm.y(), axisNorm.z()};
        const double denom = documentRay->direction.Dot(n);
        if (std::abs(denom) < 1e-6f)
        {
            return false;
        }

        const TVector center = documentPivot.value_or(TVector{pivot.x(), pivot.y(), pivot.z()});
        const double t = (center - documentRay->origin).Dot(n) / denom;
        documentStartHit = documentRay->origin + documentRay->direction * t;
        startHit = QVector3D(static_cast<float>(documentStartHit->x),
            static_cast<float>(documentStartHit->y), static_cast<float>(documentStartHit->z));
    }
    else if (handle == TransformHandle::MoveX || handle == TransformHandle::MoveY || handle == TransformHandle::MoveZ
             || handle == TransformHandle::ScaleX || handle == TransformHandle::ScaleY || handle == TransformHandle::ScaleZ
             || (handle == TransformHandle::ScaleUniform
                 && (targetKind == TransformTargetKind::Light
                     || targetKind == TransformTargetKind::ThingLight
                     || targetKind == TransformTargetKind::SectorLight)))
    {
        const double L = std::max(1.0, raySegmentMaxDistance);
        const TVector ro = documentRay->origin;
        const TVector rd = documentRay->direction;
        const TVector p0 = ro;
        const TVector p1 = ro + rd * L;

        const TVector ax{ static_cast<double>(axisNorm.x()), static_cast<double>(axisNorm.y()), static_cast<double>(axisNorm.z()) };
        const TVector c = documentPivot.value_or(TVector{pivot.x(), pivot.y(), pivot.z()});
        const TVector q0 = c - ax * L;
        const TVector q1 = c + ax * L;

        const SegmentClosestPoints cp = segmentCalcClosestPoints(p0, p1, q0, q1);
        documentStartHit = cp.q;
        startHit = QVector3D(static_cast<float>(cp.q.x), static_cast<float>(cp.q.y), static_cast<float>(cp.q.z));
    }
    else
    {
        // Rotation handles: store normalized start vector on the rotation plane.
        const TVector n{axisNorm.x(), axisNorm.y(), axisNorm.z()};
        const double denom = documentRay->direction.Dot(n);
        if (std::abs(denom) < 1e-6f)
        {
            return false;
        }

        const TVector center = documentPivot.value_or(TVector{pivot.x(), pivot.y(), pivot.z()});
        const double t = (center - documentRay->origin).Dot(n) / denom;
        const TVector relative = documentRay->origin + documentRay->direction * t - center;
        QVector3D v(static_cast<float>(relative.x), static_cast<float>(relative.y), static_cast<float>(relative.z));
        if (v.lengthSquared() < 1e-10f)
        {
            return false;
        }
        v.normalize();
        startHit = v;
    }

    // Collect pick IDs for shader preview.
    transformPreviewIdCount_ = 0;
    transformPreviewVertexFilter_ = false;
    previewThingSectors_.clear();
    restoreTransformSurfaceUvPreview();
    transformSurfaceUvFailureReported_ = false;
    std::vector<VertexRef> uvMovedVertices{};
    const auto addUvMovedVertex = [&uvMovedVertices](const VertexRef ref)
    {
        const bool duplicate = std::ranges::any_of(
            uvMovedVertices,
            [ref](const VertexRef existing)
            {
                return existing.sector == ref.sector
                    && existing.vertex == ref.vertex;
            });
        if (!duplicate)
        {
            uvMovedVertices.push_back(ref);
        }
    };

    auto tryAddPickId = [&](const int id)
    {
        if (id <= 0)
        {
            return;
        }

        for (int i = 0; i < transformPreviewIdCount_; ++i)
        {
            const int encodedId =
                transformPreviewIds_[static_cast<std::size_t>(i)];
            if (!render::transformPreviewPickIdIsFiltered(encodedId)
                && encodedId == id)
            {
                return;
            }
        }

        if (transformPreviewIdCount_ < static_cast<int>(transformPreviewIds_.size()))
        {
            const std::size_t slot =
                static_cast<std::size_t>(transformPreviewIdCount_);
            transformPreviewIds_[slot] = id;
            transformPreviewPayloads_[slot] = QVector3D(1.0f, 1.0f, 1.0f);
            ++transformPreviewIdCount_;
        }
    };

    auto lookupSurfacePickId = [&](const std::int32_t sectorIndex, const std::int32_t surfaceIndex) -> int
    {
        if (sectorIndex < 0 || surfaceIndex < 0)
        {
            return 0;
        }

        const std::uint64_t key =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(sectorIndex)) << 32)
            | static_cast<std::uint64_t>(static_cast<std::uint32_t>(surfaceIndex));

        if (const auto it = surfacePickIds_.find(key); it != surfacePickIds_.end())
        {
            return static_cast<int>(it->second);
        }
        return 0;
    };

    auto tryAddSurfaceVertexPreview = [&](const int id, const QVector3D& sourcePoint)
    {
        if (id < 0
            || !std::isfinite(sourcePoint.x())
            || !std::isfinite(sourcePoint.y())
            || !std::isfinite(sourcePoint.z()))
        {
            return;
        }

        const int encodedId =
            render::encodeFilteredTransformPreviewPickId(id);
        for (int i = 0; i < transformPreviewIdCount_; ++i)
        {
            const std::size_t slot = static_cast<std::size_t>(i);
            if (transformPreviewIds_[slot] == encodedId
                && (transformPreviewPayloads_[slot] - sourcePoint)
                    .lengthSquared() <= 1.0e-10f)
            {
                return;
            }
        }

        if (transformPreviewIdCount_ < static_cast<int>(transformPreviewIds_.size()))
        {
            const std::size_t slot = static_cast<std::size_t>(transformPreviewIdCount_);
            transformPreviewIds_[slot] = encodedId;
            transformPreviewPayloads_[slot] = sourcePoint;
            ++transformPreviewIdCount_;
        }
    };

    bool previewCapacityExceeded = false;
    const auto addGeometryPreviewPlan = [&](
        const render::GeometryTransformPreviewPlan& plan)
    {
        const int initialCount = transformPreviewIdCount_;
        for (const SurfaceRef ref : plan.wholeSurfaces)
        {
            const int before = transformPreviewIdCount_;
            tryAddPickId(lookupSurfacePickId(ref.sector, ref.surface));
            if (transformPreviewIdCount_ == before
                && before >= static_cast<int>(transformPreviewIds_.size()))
            {
                previewCapacityExceeded = true;
                break;
            }
        }
        if (!previewCapacityExceeded)
        {
            for (const render::GeometryTransformPreviewCorner& corner :
                 plan.partialCorners)
            {
                const int before = transformPreviewIdCount_;
                tryAddSurfaceVertexPreview(
                    lookupSurfacePickId(
                        corner.surface.sector,
                        corner.surface.surface),
                    QVector3D(
                        static_cast<float>(corner.sourcePoint.x),
                        static_cast<float>(corner.sourcePoint.y),
                        static_cast<float>(corner.sourcePoint.z)));
                if (transformPreviewIdCount_ == before
                    && before >= static_cast<int>(transformPreviewIds_.size()))
                {
                    previewCapacityExceeded = true;
                    break;
                }
            }
        }
        if (!previewCapacityExceeded)
        {
            for (const render::GeometryTransformPreviewVertex& vertex :
                 plan.movedVertices)
            {
                addUvMovedVertex(VertexRef{ vertex.sector, vertex.vertex });
                const int before = transformPreviewIdCount_;
                tryAddSurfaceVertexPreview(
                    0,
                    QVector3D(
                        static_cast<float>(vertex.sourcePoint.x),
                        static_cast<float>(vertex.sourcePoint.y),
                        static_cast<float>(vertex.sourcePoint.z)));
                if (transformPreviewIdCount_ == before
                    && before >= static_cast<int>(transformPreviewIds_.size()))
                {
                    previewCapacityExceeded = true;
                    break;
                }
            }
        }

        if (previewCapacityExceeded)
        {
            // A partial admission would visibly tear shared topology. Disable
            // this transient preview instead; the checked core commit remains
            // authoritative and complete.
            transformPreviewIdCount_ = initialCount;
        }
    };

    if (targetKind == TransformTargetKind::Thing
        || (targetKind == TransformTargetKind::ThingLight
            && activeLightKind == render::LightGizmoKind::ThingRegular
            && transformTool_ == TransformTool::Move))
    {
        auto tryAddThingIndex = [&](const std::int32_t thingIndex)
        {
            if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= thingPickIds_.size())
            {
                return;
            }
            if (targetKind == TransformTargetKind::ThingLight)
            {
                const render::lighting::RuntimeThingLight light =
                    render::lighting::resolveRuntimeThingLight(
                        *level_,
                        templates_,
                        level_->things[static_cast<std::size_t>(thingIndex)],
                        thingIndex,
                        std::nullopt,
                        render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
                if (light.classification != render::lighting::ThingLightClass::Dynamic)
                {
                    return;
                }
            }
            tryAddPickId(static_cast<int>(thingPickIds_[static_cast<std::size_t>(thingIndex)]));
        };

        for (const std::int32_t idx : highlightedSelection_.things)
        {
            tryAddThingIndex(idx);
        }
        tryAddThingIndex(activeThing);
    }
    else if (targetKind == TransformTargetKind::Frame)
    {
        // Frame/path editing previews by rendering the owning thing at the selected frame pose.
        // Reuse the standard shader preview transform (`u_xform_*`) so the thing representation moves/rotates
        // live while dragging the frame gizmo.
        if (activeFrameThing >= 0 && static_cast<std::size_t>(activeFrameThing) < thingPickIds_.size())
        {
            tryAddPickId(static_cast<int>(thingPickIds_[static_cast<std::size_t>(activeFrameThing)]));
        }
    }
    else if (targetKind == TransformTargetKind::Surface)
    {
        const bool filterToMovedVertices =
            transformTool_ == TransformTool::Move
            && (surfaceSubMode_ == SurfaceSubMode::Edge || surfaceSubMode_ == SurfaceSubMode::Vertex);

        if (filterToMovedVertices)
        {
            transformPreviewVertexFilter_ = true;

            auto addSectorVertex = [&](const std::int32_t sectorIndex, const std::int32_t vertexIndex) -> bool
            {
                const std::optional<TVector> sourcePoint = vertexPoint(sectorIndex, vertexIndex);
                if (!sourcePoint)
                {
                    return false;
                }

                // Point buffers have no pick ID. An ID-zero entry keeps the regular and
                // selected vertex markers attached to the same moved sector vertex.
                addUvMovedVertex(VertexRef{ sectorIndex, vertexIndex });
                tryAddSurfaceVertexPreview(0, render::displayVector(*sourcePoint));

                const Sector& sector = level_->sectors[static_cast<std::size_t>(sectorIndex)];
                for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
                {
                    const Surface& surface = sector.surfaces[surfaceIndex];
                    if (std::find(surface.vertices.begin(), surface.vertices.end(), vertexIndex) == surface.vertices.end())
                    {
                        continue;
                    }

                    // A sector vertex may be shared by several faces. Include every
                    // connected surface so the preview deforms the same topology that
                    // the eventual vertex/edge commit changes.
                    tryAddSurfaceVertexPreview(
                        lookupSurfacePickId(sectorIndex, static_cast<std::int32_t>(surfaceIndex)),
                        render::displayVector(*sourcePoint));
                }
                return true;
            };

            bool addedMovedVertex = false;
            if (surfaceSubMode_ == SurfaceSubMode::Vertex)
            {
                for (const editor::SurfaceVertexRef& ref : highlightedSelection_.surfaceVertices)
                {
                    const std::optional<std::pair<std::int32_t, TVector>> resolved = surfaceVertexPoint(ref);
                    if (resolved)
                    {
                        addedMovedVertex = addSectorVertex(ref.surface.sector, resolved->first) || addedMovedVertex;
                    }
                }

                if (!addedMovedVertex && activeSurfaceSector >= 0 && activeSurfaceVertex >= 0)
                {
                    addedMovedVertex = addSectorVertex(activeSurfaceSector, activeSurfaceVertex);
                }
            }
            else if (activeSurfaceSector >= 0 && activeSurfaceIndex >= 0 && activeSurfaceEdge >= 0
                && static_cast<std::size_t>(activeSurfaceSector) < level_->sectors.size()
                && static_cast<std::size_t>(activeSurfaceIndex)
                    < level_->sectors[static_cast<std::size_t>(activeSurfaceSector)].surfaces.size())
            {
                const Sector& sector = level_->sectors[static_cast<std::size_t>(activeSurfaceSector)];
                const Surface& surface = sector.surfaces[static_cast<std::size_t>(activeSurfaceIndex)];
                if (surface.vertices.size() >= 2
                    && static_cast<std::size_t>(activeSurfaceEdge) < surface.vertices.size())
                {
                    const std::size_t edge = static_cast<std::size_t>(activeSurfaceEdge);
                    addedMovedVertex = addSectorVertex(activeSurfaceSector, surface.vertices[edge]);
                    addedMovedVertex = addSectorVertex(
                        activeSurfaceSector,
                        surface.vertices[(edge + 1) % surface.vertices.size()]) || addedMovedVertex;
                }
            }

            if (!addedMovedVertex)
            {
                transformPreviewVertexFilter_ = false;
                transformPreviewIdCount_ = 0;
            }
        }
        else
        {
            std::vector<SurfaceRef> selectedSurfaces(
                highlightedSelection_.surfaces.begin(),
                highlightedSelection_.surfaces.end());
            const SurfaceRef activeSurface{
                selectedSurfaceSector_, selectedSurface_ };
            if (selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0
                && std::ranges::find(selectedSurfaces, activeSurface)
                    == selectedSurfaces.end())
            {
                selectedSurfaces.push_back(activeSurface);
            }

            transformPreviewVertexFilter_ = true;
            addGeometryPreviewPlan(
                render::buildSurfaceTransformPreviewPlan(
                    *level_, selectedSurfaces));
        }
    }
    else if (targetKind == TransformTargetKind::Sector)
    {
        std::vector<std::int32_t> selectedSectors(
            highlightedSelection_.sectors.begin(),
            highlightedSelection_.sectors.end());
        if (selectedSector_ >= 0
            && std::ranges::find(selectedSectors, selectedSector_)
                == selectedSectors.end())
        {
            selectedSectors.push_back(selectedSector_);
        }

        transformPreviewVertexFilter_ = true;
        addGeometryPreviewPlan(
            render::buildSectorTransformPreviewPlan(*level_, selectedSectors));
    }

    if ( previewCapacityExceeded )
    {
        transformPreviewIdCount_ = 0;
        transformPreviewVertexFilter_ = false;
    }

    const bool geometryUvPreview =
        preserveConnectedSurfaceUvsEnabled_ &&
        (targetKind == TransformTargetKind::Sector || targetKind == TransformTargetKind::Surface) &&
        (transformTool_ == TransformTool::Move || transformTool_ == TransformTool::Rotate ||
         transformTool_ == TransformTool::Scale);
    if ( geometryUvPreview && !previewCapacityExceeded )
    {
        transformSurfaceUvPlan_ =
            level::captureConnectedSurfaceUvPlan(*level_, uvMovedVertices, transformTool_ == TransformTool::Scale);
        if ( !transformSurfaceUvPlan_->valid )
        {
            transformSurfaceUvPlan_.reset();
            transformPreviewIdCount_ = 0;
            transformPreviewVertexFilter_ = false;
            reportConnectedSurfaceUvFailure(QStringLiteral("The source texture mapping is invalid"));
            return false;
        }
    }

    transformPreviewActive_ = (transformPreviewIdCount_ > 0);
    transformPreviewMatrix_.setToIdentity();
    transformDrag_ = {};
    transformDrag_.active = true;
    transformDrag_.target = targetKind;
    transformDrag_.handle = handle;
    transformDrag_.surfaceMode = surfaceSubMode_;
    transformDrag_.frameThing = activeFrameThing;
    transformDrag_.frameValue = activeFrameValue;
    transformDrag_.frameMovePoseSnapshot = std::move(frameMovePoseSnapshot);
    transformDrag_.surfaceSector = activeSurfaceSector;
    transformDrag_.surfaceIndex = activeSurfaceIndex;
    transformDrag_.surfaceVertex = activeSurfaceVertex;
    transformDrag_.surfaceEdge = activeSurfaceEdge;
    transformDrag_.lightOwner = activeLightOwner;
    transformDrag_.lightKind = activeLightKind;
    transformDrag_.startMouse = mousePos;
    transformDrag_.pivot = pivot;
    transformDrag_.documentPivot = documentPivot;
    transformDrag_.axis = axisNorm;
    transformDrag_.startHit = startHit;
    transformDrag_.documentStartHit = documentStartHit;
    transformDrag_.currentHit = startHit;
    transformDrag_.rawTranslation = QVector3D(0.0f, 0.0f, 0.0f);
    transformDrag_.gridTranslation = QVector3D(0.0f, 0.0f, 0.0f);
    transformDrag_.translation = QVector3D(0.0f, 0.0f, 0.0f);
    transformDrag_.gridSnapReference = gridSnapReference;
    transformDrag_.gridFrame = transformGridFrame;
    transformDrag_.gridStickPoints = std::move(gridStickPoints);
    transformDrag_.geometryMoveSnapCache = std::move(geometryMoveSnapCache);
    transformDrag_.thingMoveSnapCache = std::move(thingMoveSnapCache);
    transformDrag_.angleDeg = 0.0f;
    transformDrag_.unitsPerPixel = unitsPerPx;

    refreshTransformSurfaceUvPreview();

    Q_EMIT transformPreviewChanged(
        true,
        transformTool_,
        transformDrag_.translation,
        transformDrag_.axis,
        static_cast<double>(transformDrag_.angleDeg),
        transformDrag_.pivot,
        static_cast<double>(transformDrag_.scaleFactor));

    update();
    return true;
}

bool GlView::applyPreviewThingLightingOverride(const std::int32_t thingIndex, const std::int32_t sectorIndex)
{
    if (!glReady_ || !level_)
    {
        return false;
    }

    if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= thingPickIds_.size())
    {
        return false;
    }

    const int pid = static_cast<int>(thingPickIds_[static_cast<std::size_t>(thingIndex)]);
    if (pid <= 0)
    {
        return false;
    }

    const auto itRanges = thingTriBuildRangesByPickId_.find(pid);
    if (itRanges == thingTriBuildRangesByPickId_.end())
    {
        return false;
    }

    if (!triVbo_ || triVboAllocatedBytes_ <= 0 || triVertexCount_ != static_cast<int>(triVertices_.size()))
    {
        return false;
    }

    if (thingTriBuildGlobalFirstVertex_.size() != thingMeshCache_.triBuilds.size())
    {
        return false;
    }

    const ThingSectorLighting sectorLights = calcThingSectorLighting(level_, sectorIndex);
    const QVector3D editorSectorTint =
        render::classicEditorSectorTintScale(level_, sectorIndex);
    ColorF thingAmbient = sectorLights.ambientLight;
    if constexpr (render::lighting::kRuntimeDynamicLightPreviewEnabled)
    {
        if (thingIndex >= 0 && static_cast<std::size_t>(thingIndex) < level_->things.size())
        {
            const render::lighting::RuntimeThingLight selfLight =
                render::lighting::resolveRuntimeThingLight(
                    *level_,
                    templates_,
                    level_->things[static_cast<std::size_t>(thingIndex)],
                    thingIndex,
                    sectorIndex,
                    render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
            thingAmbient = render::lighting::applyRuntimeThingSelfLight(
                level_->kind,
                thingAmbient,
                selfLight,
                thingLightRenderingEnabled_);
        }
    }
    const auto maxLightMode = static_cast<TLightMode>(static_cast<std::int32_t>(lightingMode_));
    render::lighting::RuntimeSectorPointLight sectorPointLight{};
    if (level_->kind == ProjectType::IJIM
        && sectorIndex >= 0
        && static_cast<std::size_t>(sectorIndex) < level_->sectors.size())
    {
        sectorPointLight = render::lighting::resolveRuntimeSectorPointLight(
            level_->sectors[static_cast<std::size_t>(sectorIndex)].pointLight);
    }

    makeCurrent();

    bool wroteAny = false;
    bool fallbackUpload = false;

    triVbo_->bind();
    for (const TriBuildRange& r : itRanges->second)
    {
        if (r.first < 0 || r.count <= 0)
        {
            continue;
        }

        if (r.buildIndex >= thingMeshCache_.triBuilds.size())
        {
            continue;
        }

        const TriBuild& build = thingMeshCache_.triBuilds[static_cast<std::size_t>(r.buildIndex)];
        if ((static_cast<std::size_t>(r.first + r.count) > build.vertices.size())
            || (static_cast<std::size_t>(r.first + r.count) > build.shadeInputs.size()))
        {
            continue;
        }

        const int globalBase = thingTriBuildGlobalFirstVertex_[static_cast<std::size_t>(r.buildIndex)];
        const int globalFirst = globalBase + r.first;
        if (globalFirst < 0 || (globalFirst + r.count) > static_cast<int>(triVertices_.size()))
        {
            continue;
        }

        const int offsetBytes = globalFirst * static_cast<int>(sizeof(TriVertex));
        const int bytes = r.count * static_cast<int>(sizeof(TriVertex));
        if (offsetBytes < 0 || (offsetBytes + bytes) > triVboAllocatedBytes_)
        {
            fallbackUpload = true;
            break;
        }

        std::vector<TriVertex> tmp{};
        tmp.reserve(static_cast<std::size_t>(r.count));
        for (int i = 0; i < r.count; ++i)
        {
            const int vi = r.first + i;
            TriVertex v = build.vertices[static_cast<std::size_t>(vi)];
            const TriShadeInput& si = build.shadeInputs[static_cast<std::size_t>(vi)];
            const ColorF lit = polyCalcLitColor(
                si.intensity,
                si.lightMode,
                thingAmbient,
                si.extraLight,
                level_->kind,
                maxLightMode,
                v.dynamicLightTarget > 1.5f);
            v.r = lit.r;
            v.g = lit.g;
            v.b = lit.b;
            v.editorSectorTintR = editorSectorTint.x();
            v.editorSectorTintG = editorSectorTint.y();
            v.editorSectorTintB = editorSectorTint.z();
            if (level_->kind == ProjectType::IJIM && v.dynamicLightTarget > 1.5f)
            {
                v.sectorLightX = sectorPointLight.position.x();
                v.sectorLightY = sectorPointLight.position.y();
                v.sectorLightZ = sectorPointLight.position.z();
                v.sectorLightMinRadius = sectorPointLight.minRadius;
                v.sectorLightR = sectorPointLight.color.x();
                v.sectorLightG = sectorPointLight.color.y();
                v.sectorLightB = sectorPointLight.color.z();
                v.sectorLightMaxRadius = sectorPointLight.maxRadius;
                v.sectorLightOwner = static_cast<float>(sectorIndex);
            }
            else if (level_->kind != ProjectType::IJIM && v.dynamicLightTarget > 1.5f)
            {
                v.sectorLightX = si.extraLight.r;
                v.sectorLightY = si.extraLight.g;
                v.sectorLightZ = si.extraLight.b;
                v.sectorLightR = thingAmbient.r;
                v.sectorLightG = thingAmbient.g;
                v.sectorLightB = thingAmbient.b;
            }
            tmp.push_back(v);
        }

        triVbo_->write(offsetBytes, tmp.data(), bytes);
        wroteAny = true;
    }
    triVbo_->release();

    if (fallbackUpload)
    {
        uploadMeshes();
        wroteAny = true;
    }
    else if (wroteAny)
    {
        uploadSelectionOverlays();
    }

    doneCurrent();

    if (wroteAny)
    {
        previewRelitThings_.insert(thingIndex);
    }

    return wroteAny;
}

void GlView::restorePreviewThingLightingOverride(const std::int32_t thingIndex)
{
    if (!previewRelitThings_.contains(thingIndex))
    {
        return;
    }

    if (!glReady_)
    {
        previewRelitThings_.erase(thingIndex);
        return;
    }

    if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= thingPickIds_.size())
    {
        previewRelitThings_.erase(thingIndex);
        return;
    }

    const int pid = static_cast<int>(thingPickIds_[static_cast<std::size_t>(thingIndex)]);
    if (pid <= 0)
    {
        previewRelitThings_.erase(thingIndex);
        return;
    }

    const auto itRanges = thingTriBuildRangesByPickId_.find(pid);
    if (itRanges == thingTriBuildRangesByPickId_.end())
    {
        previewRelitThings_.erase(thingIndex);
        return;
    }

    if (!triVbo_ || triVboAllocatedBytes_ <= 0 || triVertexCount_ != static_cast<int>(triVertices_.size()))
    {
        previewRelitThings_.erase(thingIndex);
        return;
    }

    if (thingTriBuildGlobalFirstVertex_.size() != thingMeshCache_.triBuilds.size())
    {
        previewRelitThings_.erase(thingIndex);
        return;
    }

    makeCurrent();

    bool fallbackUpload = false;
    triVbo_->bind();
    for (const TriBuildRange& r : itRanges->second)
    {
        if (r.first < 0 || r.count <= 0)
        {
            continue;
        }

        if (r.buildIndex >= thingMeshCache_.triBuilds.size())
        {
            continue;
        }

        const int globalBase = thingTriBuildGlobalFirstVertex_[static_cast<std::size_t>(r.buildIndex)];
        const int globalFirst = globalBase + r.first;
        if (globalFirst < 0 || (globalFirst + r.count) > static_cast<int>(triVertices_.size()))
        {
            continue;
        }

        const int offsetBytes = globalFirst * static_cast<int>(sizeof(TriVertex));
        const int bytes = r.count * static_cast<int>(sizeof(TriVertex));
        if (offsetBytes < 0 || (offsetBytes + bytes) > triVboAllocatedBytes_)
        {
            fallbackUpload = true;
            break;
        }

        triVbo_->write(offsetBytes, triVertices_.data() + globalFirst, bytes);
    }
    triVbo_->release();

    if (fallbackUpload)
    {
        uploadMeshes();
    }
    else
    {
        uploadSelectionOverlays();
    }

    doneCurrent();

    previewRelitThings_.erase(thingIndex);
}

void GlView::restoreAllPreviewThingLightingOverrides()
{
    if (previewRelitThings_.empty())
    {
        return;
    }

    if (!glReady_)
    {
        previewRelitThings_.clear();
        return;
    }

    if (!triVbo_ || triVboAllocatedBytes_ <= 0 || triVertexCount_ != static_cast<int>(triVertices_.size()))
    {
        previewRelitThings_.clear();
        return;
    }

    if (thingTriBuildGlobalFirstVertex_.size() != thingMeshCache_.triBuilds.size())
    {
        previewRelitThings_.clear();
        return;
    }

    makeCurrent();

    bool fallbackUpload = false;
    triVbo_->bind();

    for (const std::int32_t thingIndex : previewRelitThings_)
    {
        if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= thingPickIds_.size())
        {
            continue;
        }

        const int pid = static_cast<int>(thingPickIds_[static_cast<std::size_t>(thingIndex)]);
        if (pid <= 0)
        {
            continue;
        }

        const auto itRanges = thingTriBuildRangesByPickId_.find(pid);
        if (itRanges == thingTriBuildRangesByPickId_.end())
        {
            continue;
        }

        for (const TriBuildRange& r : itRanges->second)
        {
            if (r.first < 0 || r.count <= 0)
            {
                continue;
            }

            if (r.buildIndex >= thingMeshCache_.triBuilds.size())
            {
                continue;
            }

            const int globalBase = thingTriBuildGlobalFirstVertex_[static_cast<std::size_t>(r.buildIndex)];
            const int globalFirst = globalBase + r.first;
            if (globalFirst < 0 || (globalFirst + r.count) > static_cast<int>(triVertices_.size()))
            {
                continue;
            }

            const int offsetBytes = globalFirst * static_cast<int>(sizeof(TriVertex));
            const int bytes = r.count * static_cast<int>(sizeof(TriVertex));
            if (offsetBytes < 0 || (offsetBytes + bytes) > triVboAllocatedBytes_)
            {
                fallbackUpload = true;
                break;
            }

            triVbo_->write(offsetBytes, triVertices_.data() + globalFirst, bytes);
        }

        if (fallbackUpload)
        {
            break;
        }
    }

    triVbo_->release();

    if (fallbackUpload)
    {
        uploadMeshes();
    }
    else
    {
        uploadSelectionOverlays();
    }

    doneCurrent();
    previewRelitThings_.clear();
}

void GlView::refreshTransformDragForModifiers(const Qt::KeyboardModifiers modifiers)
{
    if (!transformDrag_.active || transformDrag_.handle == TransformHandle::None)
    {
        return;
    }

    QMatrix4x4 proj;
    QMatrix4x4 view;
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(proj, view, eye, target, nearPlane, farPlane);
    updateTransformDrag(lastMousePos_, proj, view, eye, target, farPlane, modifiers);
}

void GlView::updateTransformDrag(
    const QPoint& mousePos,
    const QMatrix4x4& proj,
    const QMatrix4x4& view,
    const QVector3D& eye,
    const QVector3D&,
    const float farPlane,
    const Qt::KeyboardModifiers modifiers)
{
    if (!transformDrag_.active || transformDrag_.handle == TransformHandle::None)
    {
        return;
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    ++thingMoveDragSampleCount_;
#endif

    const QVector3D pivot = transformDrag_.pivot;
    const TVector authoringPivot = transformDrag_.documentPivot.value_or(TVector{pivot.x(), pivot.y(), pivot.z()});
    const QVector3D axis = transformDrag_.axis;

    const bool isOrtho = camera_.projectionType() == app::ViewportCamera::ProjectionType::Orthographic;
    const auto documentRay = render::viewportRayForWidget(
        proj, view, eye, mousePos, width(), height(), isOrtho);
    if (!documentRay)
    {
        return;
    }
    const double raySegmentMaxDistance = render::viewportRayMaxDistanceForWidget(
        proj, view, QPointF(mousePos), width(), height(), isOrtho)
        .value_or(std::max(0.0, static_cast<double>(farPlane)));
    const TVector documentStart = transformDrag_.documentStartHit.value_or(
        TVector{transformDrag_.startHit.x(), transformDrag_.startHit.y(), transformDrag_.startHit.z()});
    std::optional<TVector> documentRawTranslation{};

    const TransformHandle handle = transformDrag_.handle;

    auto applyScaleMatrix = [&](const float sx, const float sy, const float sz)
    {
        transformPreviewMatrix_.setToIdentity();
        transformPreviewMatrix_.translate(pivot);
        transformPreviewMatrix_.scale(sx, sy, sz);
        transformPreviewMatrix_.translate(-pivot);
    };

    if (handle == TransformHandle::ScaleUniform
        || handle == TransformHandle::ScaleX
        || handle == TransformHandle::ScaleY
        || handle == TransformHandle::ScaleZ)
    {
        transformDrag_.rawTranslation = QVector3D(0.0f, 0.0f, 0.0f);
        transformDrag_.translation = QVector3D(0.0f, 0.0f, 0.0f);
        transformDrag_.angleDeg = 0.0f;

        float scale = 1.0f;

        // Light scaling: adjust range via an axis handle (camera-facing).
        const bool lightRangeScale =
            transformDrag_.target == TransformTargetKind::Light
            || transformDrag_.target == TransformTargetKind::ThingLight
            || transformDrag_.target == TransformTargetKind::SectorLight;

        if (handle == TransformHandle::ScaleUniform && !lightRangeScale)
        {
            const int dy = mousePos.y() - transformDrag_.startMouse.y();
            scale = std::exp(-static_cast<float>(dy) * 0.010f);
            scale = std::clamp(scale, 0.01f, 100.0f);
        }
        else
        {
            const double L = std::max(1.0, raySegmentMaxDistance);
            const TVector ro = documentRay->origin;
            const TVector rd = documentRay->direction;
            const TVector p0 = ro;
            const TVector p1 = ro + rd * L;

            const TVector ax{ static_cast<double>(axis.x()), static_cast<double>(axis.y()), static_cast<double>(axis.z()) };
            const TVector c = authoringPivot;
            const TVector q0 = c - ax * L;
            const TVector q1 = c + ax * L;

            const SegmentClosestPoints cp = segmentCalcClosestPoints(p0, p1, q0, q1);
            const double startDist = (documentStart - c).Dot(ax);
            const double curDist = (cp.q - c).Dot(ax);
            if (std::abs(startDist) < 1e-6f)
            {
                return;
            }

            scale = static_cast<float>(curDist / startDist);
            if (!std::isfinite(scale))
            {
                return;
            }
            scale = std::clamp(scale, 0.01f, 100.0f);
        }

        transformDrag_.scaleFactor = scale;

        if (lightRangeScale)
        {
            transformPreviewMatrix_.setToIdentity();
        }
        else
        {
            float sx = 1.0f;
            float sy = 1.0f;
            float sz = 1.0f;
            if (handle == TransformHandle::ScaleX) sx = scale;
            if (handle == TransformHandle::ScaleY) sy = scale;
            if (handle == TransformHandle::ScaleZ) sz = scale;
            if (handle == TransformHandle::ScaleUniform) { sx = scale; sy = scale; sz = scale; }
            applyScaleMatrix(sx, sy, sz);
        }
    }
    else if (handle == TransformHandle::MoveFree)
    {
        const TVector n{axis.x(), axis.y(), axis.z()};
        const double denom = documentRay->direction.Dot(n);
        if (std::abs(denom) < 1e-6f)
        {
            return;
        }

        const TVector center = authoringPivot;
        const double t = (center - documentRay->origin).Dot(n) / denom;
        documentRawTranslation = documentRay->origin + documentRay->direction * t - documentStart;
        transformDrag_.rawTranslation = QVector3D(static_cast<float>(documentRawTranslation->x),
            static_cast<float>(documentRawTranslation->y), static_cast<float>(documentRawTranslation->z));
        transformDrag_.translation = transformDrag_.rawTranslation;

        transformPreviewMatrix_.setToIdentity();
        transformPreviewMatrix_.translate(transformDrag_.translation);
    }
    else if (handle == TransformHandle::MoveX || handle == TransformHandle::MoveY || handle == TransformHandle::MoveZ)
    {
        const double L = std::max(1.0, raySegmentMaxDistance);
        const TVector ro = documentRay->origin;
        const TVector rd = documentRay->direction;
        const TVector p0 = ro;
        const TVector p1 = ro + rd * L;

        const TVector ax{ static_cast<double>(axis.x()), static_cast<double>(axis.y()), static_cast<double>(axis.z()) };
        const TVector c = authoringPivot;
        const TVector q0 = c - ax * L;
        const TVector q1 = c + ax * L;

        const SegmentClosestPoints cp = segmentCalcClosestPoints(p0, p1, q0, q1);
        documentRawTranslation = ax * (cp.q - documentStart).Dot(ax);
        transformDrag_.rawTranslation = QVector3D(static_cast<float>(documentRawTranslation->x),
            static_cast<float>(documentRawTranslation->y), static_cast<float>(documentRawTranslation->z));
        transformDrag_.translation = transformDrag_.rawTranslation;

        transformPreviewMatrix_.setToIdentity();
        transformPreviewMatrix_.translate(transformDrag_.translation);
    }
    else
    {
        const TVector n{axis.x(), axis.y(), axis.z()};
        const double denom = documentRay->direction.Dot(n);
        if (std::abs(denom) < 1e-6f)
        {
            return;
        }

        const TVector center = authoringPivot;
        const double t = (center - documentRay->origin).Dot(n) / denom;
        const TVector relative = documentRay->origin + documentRay->direction * t - center;
        QVector3D v(static_cast<float>(relative.x), static_cast<float>(relative.y), static_cast<float>(relative.z));
        if (v.lengthSquared() < 1e-10f)
        {
            return;
        }
        v.normalize();
        transformDrag_.currentHit = v;

        const QVector3D a = transformDrag_.startHit;
        const QVector3D b = v;
        const QVector3D cross = QVector3D::crossProduct(a, b);
        const float sinv = QVector3D::dotProduct(axis, cross);
        const float cosv = QVector3D::dotProduct(a, b);
        const float angleRad = std::atan2(sinv, cosv);
        transformDrag_.angleDeg = angleRad * 180.0f / std::numbers::pi_v<float>;

        transformPreviewMatrix_.setToIdentity();
        transformPreviewMatrix_.translate(pivot);
        transformPreviewMatrix_.rotate(transformDrag_.angleDeg, axis);
        transformPreviewMatrix_.translate(-pivot);
    }

    const bool isMoveHandle =
        handle == TransformHandle::MoveFree
        || handle == TransformHandle::MoveX
        || handle == TransformHandle::MoveY
        || handle == TransformHandle::MoveZ;
    const Qt::KeyboardModifiers held = modifiers & ui::shortcuts::editorModifierMask();
    const ui::shortcuts::SurfaceTransformSnapModifierState surfaceSnapState =
        ui::shortcuts::surfaceTransformSnapModifierState(
            transformGridSnapEnabled_,
            geometryTransformSnapEnabled_,
            held,
            transformGridSnapInvertModifiers_,
            geometryTransformSnapInvertModifiers_);
    const bool gridSnapActive =
        transformDrag_.target == TransformTargetKind::Surface
        ? surfaceSnapState.gridSnap
        : ui::shortcuts::modifierToggledSetting(
            transformGridSnapEnabled_,
            held,
            transformGridSnapInvertModifiers_);
    const bool gridStickActive = ui::shortcuts::modifierToggledSetting(
        transformGridPlaneStickEnabled_,
        held,
        transformGridStickInvertModifiers_);

    if (isMoveHandle)
    {
        transformDrag_.documentTranslation = documentRawTranslation;
        QVector3D gridTranslation = transformDrag_.rawTranslation;
        if (gridSnapActive && transformDrag_.gridFrame.valid)
        {
            TVector preciseGrid{};
            const QVector3D snapped = render::snapTransformTranslationToGrid(
                transformDrag_.gridSnapReference,
                documentRawTranslation.value_or(TVector{gridTranslation.x(), gridTranslation.y(), gridTranslation.z()}),
                transformDrag_.gridFrame, &preciseGrid);
            if (handle == TransformHandle::MoveFree)
            {
                gridTranslation = snapped;
                transformDrag_.documentTranslation = preciseGrid;
            }
            else
            {
                // Axis drags remain one-dimensional. World/grid-aligned axes
                // still land exactly on their corresponding lattice line;
                // oblique axes take the closest correction available on the
                // selected handle.
                const TVector raw = documentRawTranslation.value_or(
                    TVector{gridTranslation.x(), gridTranslation.y(), gridTranslation.z()});
                TVector direction{axis.x(), axis.y(), axis.z()};
                const double axisLength = std::hypot(direction.x, direction.y, direction.z);
                if (axisLength > 0.0) direction = TVector{direction.x / axisLength, direction.y / axisLength, direction.z / axisLength};
                const TVector preciseAxis = raw + direction * (preciseGrid - raw).Dot(direction);
                gridTranslation = QVector3D(static_cast<float>(preciseAxis.x),
                    static_cast<float>(preciseAxis.y), static_cast<float>(preciseAxis.z));
                transformDrag_.documentTranslation = preciseAxis;
            }
        }

        const bool gridStickTarget =
            transformDrag_.target == TransformTargetKind::Sector
            || transformDrag_.target == TransformTargetKind::Surface
            || transformDrag_.target == TransformTargetKind::Thing
            || transformDrag_.target == TransformTargetKind::Frame
            || transformDrag_.target == TransformTargetKind::Light
            || transformDrag_.target == TransformTargetKind::ThingLight
            || transformDrag_.target == TransformTargetKind::SectorLight;
        if (gridStickActive
            && transformDrag_.target == TransformTargetKind::Thing
            && transformDrag_.gridStickPoints.empty())
        {
            // Grid Plane Stick can be enabled by a modifier after the drag
            // starts. Defer support construction until that first transition,
            // while preserving the same per-gesture authored snapshot.
            for (const std::int32_t thingIndex :
                transformDrag_.thingMoveSnapCache.movingIndices)
            {
                if (thingIndex < 0
                    || static_cast<std::size_t>(thingIndex) >= level_->things.size())
                {
                    continue;
                }
                const std::vector<TVector> supports =
                    sed::thing::resolveThingDocumentSupportPoints(
                        *level_,
                        templates_,
                        resources_,
                        level_->things[static_cast<std::size_t>(thingIndex)],
                        static_cast<double>(thingCubeHalfSize_));
                transformDrag_.gridStickPoints.insert(
                    transformDrag_.gridStickPoints.end(),
                    supports.begin(),
                    supports.end());
            }
        }
        if (gridStickActive && gridStickTarget && transformDrag_.gridFrame.valid)
        {
            const float captureDistance = gridPlaneStickRadius_;
            const float releaseDistance = captureDistance * 2.5f;
            const QVector3D constraintAxis =
                handle == TransformHandle::MoveFree ? QVector3D{} : axis;
            const TVector preciseGrid = render::applyGridPlaneStickThrough(
                transformDrag_.documentTranslation.value_or(render::authoringVector(gridTranslation)),
                transformDrag_.gridStickPoints, transformDrag_.gridFrame,
                captureDistance, releaseDistance, transformDrag_.gridPlaneStick,
                render::authoringVector(constraintAxis));
            transformDrag_.documentTranslation = preciseGrid;
            gridTranslation = render::displayVector(preciseGrid);
        }
        else
        {
            transformDrag_.gridPlaneStick = {};
        }

        transformDrag_.gridTranslation = gridTranslation;
        transformDrag_.translation = gridTranslation;
        transformPreviewMatrix_.setToIdentity();
        transformPreviewMatrix_.translate(gridTranslation);
    }

    if (level_
        && (transformDrag_.target == TransformTargetKind::Thing
            || transformDrag_.target == TransformTargetKind::Frame
            || (transformDrag_.target == TransformTargetKind::ThingLight
                && transformDrag_.lightKind == render::LightGizmoKind::ThingRegular))
        && isMoveHandle)
    {
        transformDrag_.snapRotateAxis = QVector3D{};
        transformDrag_.snapRotatePivot = QVector3D{};
        transformDrag_.documentSnapRotatePivot.reset();
        transformDrag_.snapRotateAngleDeg = 0.0f;

        const bool surfaceStickActive = ui::shortcuts::modifierToggledSetting(
            thingMoveSurfaceStickEnabled_,
            held,
            thingMoveSurfaceStickInvertModifiers_);
        const bool surfaceAlignActive = ui::shortcuts::modifierToggledSetting(
            thingMoveSurfaceAlignEnabled_,
            held,
            thingMoveSurfaceAlignInvertModifiers_);
        const bool thingSnapActive = ui::shortcuts::modifierToggledSetting(
            thingMoveSnapToThingEnabled_,
            held,
            thingMoveSnapInvertModifiers_);
        const bool thingSnapRotateActive = ui::shortcuts::modifierToggledSetting(
            thingMoveSnapRotateEnabled_,
            held,
            thingMoveSnapRotateInvertModifiers_);
        const bool gridPlaneClutchActive =
            transformDrag_.gridPlaneStick.holding
            || transformDrag_.gridPlaneStick.released;
        QVector3D adjustedTranslation = transformDrag_.translation;

        render::FrameMovePoseSnapshot& frameMovePoseSnapshot =
            transformDrag_.frameMovePoseSnapshot;
        const Thing* movingFramePose =
            transformDrag_.target == TransformTargetKind::Frame
                && frameMovePoseSnapshot.valid
                && frameMovePoseSnapshot.ownerThingIndex == transformDrag_.frameThing
                && frameMovePoseSnapshot.frameValueIndex == transformDrag_.frameValue
            ? &frameMovePoseSnapshot.virtualPose
            : nullptr;

        std::vector<std::int32_t> movingThings{};
        if (movingFramePose)
        {
            movingThings.push_back(transformDrag_.frameThing);
        }
        else if (transformDrag_.target == TransformTargetKind::Thing
            && !transformDrag_.thingMoveSnapCache.movingIndices.empty())
        {
            movingThings = transformDrag_.thingMoveSnapCache.movingIndices;
        }
        else
        {
            movingThings = sed::thing::uniqueValidThingIndices(
                highlightedSelection_.things,
                transformDrag_.target == TransformTargetKind::ThingLight
                    ? transformDrag_.lightOwner
                    : selectedThing_,
                level_->things.size());
        }
        if (transformDrag_.target == TransformTargetKind::ThingLight)
        {
            movingThings.erase(
                std::remove_if(movingThings.begin(), movingThings.end(), [&](const std::int32_t thingIndex)
                {
                    const render::lighting::RuntimeThingLight light =
                        render::lighting::resolveRuntimeThingLight(
                            *level_,
                            templates_,
                            level_->things[static_cast<std::size_t>(thingIndex)],
                            thingIndex,
                            std::nullopt,
                            render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
                    return light.classification != render::lighting::ThingLightClass::Dynamic;
                }),
                movingThings.end());
        }

        auto movingThingForIndex = [&](const std::int32_t thingIndex) -> const Thing&
        {
            if (movingFramePose && thingIndex == frameMovePoseSnapshot.ownerThingIndex)
            {
                return *movingFramePose;
            }
            return level_->things[static_cast<std::size_t>(thingIndex)];
        };
        auto stationaryAnchorPoseForIndex = [&](const std::int32_t thingIndex)
            -> const Thing&
        {
            if (!movingFramePose)
            {
                return level_->things[static_cast<std::size_t>(thingIndex)];
            }
            if (thingIndex < 0
                || static_cast<std::size_t>(thingIndex) >= level_->things.size())
            {
                return *movingFramePose;
            }

            auto& cachedPoses = frameMovePoseSnapshot.stationaryAnchorPoses;
            if (const auto found = cachedPoses.find(thingIndex);
                found != cachedPoses.end())
            {
                return found->second;
            }

            const Thing& source = level_->things[static_cast<std::size_t>(thingIndex)];
            Thing compact{};
            if (thingIndex == frameMovePoseSnapshot.ownerThingIndex)
            {
                // The captured virtual pose already preserves the owner's
                // local model/SPR overrides without its repeated Frame data.
                compact = *movingFramePose;
                compact.x = source.x;
                compact.y = source.y;
                compact.z = source.z;
                compact.pch = source.pch;
                compact.yaw = source.yaw;
                compact.rol = source.rol;
            }
            else
            {
                compact.id = source.id;
                compact.num = source.num;
                compact.sector = source.sector;
                compact.archLightingIndex = source.archLightingIndex;
                compact.templateName = source.templateName;
                compact.name = source.name;
                compact.x = source.x;
                compact.y = source.y;
                compact.z = source.z;
                compact.pch = source.pch;
                compact.yaw = source.yaw;
                compact.rol = source.rol;
                compact.layer = source.layer;
                compact.bbox = source.bbox;
                for (const TplValue& value : source.values)
                {
                    if (value.type == ValueType::frame
                        && sed::text::iequalsAscii(value.name, "frame"))
                    {
                        continue;
                    }
                    compact.values.push_back(value);
                }
            }

            const auto [cachedPose, inserted] = cachedPoses.emplace(
                thingIndex,
                std::move(compact));
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
            if (inserted && thingIndex != frameMovePoseSnapshot.ownerThingIndex)
            {
                const qulonglong previousCapacity = property(
                    "sed.frameMoveStationaryProxyNonOwnerValueCapacity").toULongLong();
                setProperty(
                    "sed.frameMoveStationaryProxyNonOwnerValueCapacity",
                    QVariant::fromValue<qulonglong>(std::max(
                        previousCapacity,
                        static_cast<qulonglong>(cachedPose->second.values.capacity()))));
            }
#else
            Q_UNUSED(inserted);
#endif
            return cachedPose->second;
        };
        auto isMovingAnchorIndex = [&](const std::int32_t thingIndex)
        {
            // A frame pose is virtual: its owner remains a stationary Thing in
            // the level and is therefore a valid alignment/snap target.
            return !movingFramePose && sed::thing::containsThingIndex(movingThings, thingIndex);
        };

        // Resource revision checks are deliberately kept out of the pointer
        // sample loop. A drag owns an immutable Level snapshot, so resolve a
        // resource once per distinct visual identity and let each entry retain
        // the exact resulting box/connectors for the remainder of the gesture.
        auto appendThingMoveKeyPart = [](std::string& key, const std::string_view part)
        {
            key += std::to_string(part.size());
            key.push_back(':');
            key.append(part);
            key.push_back('|');
        };
        auto appendThingMoveKeyDouble = [](std::string& key, const double value)
        {
            key += std::to_string(std::bit_cast<std::uint64_t>(value));
            key.push_back('|');
        };
        auto thingMoveLocalBoxKey = [&](const Thing& thing)
        {
            std::string key{};
            appendThingMoveKeyPart(
                key,
                sed::thing::thingModel3doName(*level_, templates_, thing));
            appendThingMoveKeyPart(
                key,
                sed::thing::tplValueString(
                    sed::level::thingFindValue(*level_, templates_, thing, "sprite")));
            appendThingMoveKeyPart(key, sed::thing::thingTemplateKey(thing));
            appendThingMoveKeyDouble(key, thing.bbox.x1);
            appendThingMoveKeyDouble(key, thing.bbox.y1);
            appendThingMoveKeyDouble(key, thing.bbox.z1);
            appendThingMoveKeyDouble(key, thing.bbox.x2);
            appendThingMoveKeyDouble(key, thing.bbox.y2);
            appendThingMoveKeyDouble(key, thing.bbox.z2);
            return key;
        };
        auto thingMoveConnectorKey = [&](const Thing& thing)
        {
            std::string key{};
            appendThingMoveKeyPart(
                key,
                sed::thing::thingModel3doName(*level_, templates_, thing));
            appendThingMoveKeyPart(key, sed::thing::thingTemplateKey(thing));
            return key;
        };
        auto rebuildThingMoveEntryBounds = [&](
            render::ThingMoveSnapEntry& entry,
            const Thing& thing)
        {
            entry.staticBounds = {};
            entry.broadRadius = 0.0;
            if (!entry.localBox)
            {
                // An unresolved visual has no safe finite broad phase. Keep
                // it eligible for exact fallback rather than risking a false
                // negative cull.
                entry.broadRadius = std::numeric_limits<double>::infinity();
                return;
            }

            // Transform only local box coordinates here.  Transforming a
            // document-space Thing and subtracting documentOrigin afterwards
            // loses low bits for large translated levels before the broad
            // phase ever sees them.  The entry origin is already retained in
            // the cache frame, so add each rotated local corner to that exact
            // relative origin instead.
            Thing localThing{};
            if (!movingFramePose)
            {
                localThing = thing;
            }
            else
            {
                localThing.pch = thing.pch;
                localThing.yaw = thing.yaw;
                localThing.rol = thing.rol;
            }
            localThing.x = 0.0;
            localThing.y = 0.0;
            localThing.z = 0.0;
            const std::vector<TVector> localCorners =
                sed::thing::transformThingBoxDocumentPoints(
                    localThing,
                    *entry.localBox,
                    TVector{},
                    TVector{},
                    0.0,
                    false);
            for (const TVector& corner : localCorners)
            {
                sed::thing::addPointToBounds(
                    entry.staticBounds,
                    entry.relativeOrigin + corner);
                entry.broadRadius = std::max(
                    entry.broadRadius,
                    std::sqrt(corner.Dot(corner)));
            }
        };
        auto ensureThingMoveLocalBox = [&](const std::int32_t thingIndex)
            -> render::ThingMoveSnapEntry*
        {
            render::ThingMoveSnapEntry* entry =
                transformDrag_.thingMoveSnapCache.find(thingIndex);
            if (!entry
                || thingIndex < 0
                || static_cast<std::size_t>(thingIndex) >= level_->things.size())
            {
                return entry;
            }
            if (entry->localBoxReady)
            {
                return entry;
            }
            if (!entry->visible || !entry->finite)
            {
                entry->localBoxReady = true;
                return entry;
            }

            const Thing& thing = level_->things[static_cast<std::size_t>(thingIndex)];
            const std::string key = thingMoveLocalBoxKey(thing);
            auto cacheIt = transformDrag_.thingMoveSnapCache.localBoxesByKey.find(key);
            if (cacheIt == transformDrag_.thingMoveSnapCache.localBoxesByKey.end())
            {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                ++thingMoveBoundsResolveCount_;
                ++thingMoveResourceResolveCount_;
#endif
                const std::optional<ThingBox> resolved = sed::thing::resolveThingLocalBox(
                    *level_,
                    templates_,
                    resources_,
                    thing,
                    static_cast<double>(thingCubeHalfSize_));
                cacheIt = transformDrag_.thingMoveSnapCache.localBoxesByKey.emplace(
                    key,
                    resolved).first;
            }
            entry->localBox = cacheIt->second;
            entry->localBoxReady = true;
            rebuildThingMoveEntryBounds(
                *entry,
                thing);
            return entry;
        };
        auto ensureThingMoveModelBox = [&](const std::int32_t thingIndex)
            -> render::ThingMoveSnapEntry*
        {
            render::ThingMoveSnapEntry* entry =
                transformDrag_.thingMoveSnapCache.find(thingIndex);
            if (!entry
                || thingIndex < 0
                || static_cast<std::size_t>(thingIndex) >= level_->things.size())
            {
                return entry;
            }
            if (entry->modelBoxReady)
            {
                return entry;
            }
            if (!entry->visible || !entry->finite)
            {
                entry->modelBoxReady = true;
                return entry;
            }

            const Thing& thing = level_->things[static_cast<std::size_t>(thingIndex)];
            const std::string key = sed::thing::thingModel3doName(
                *level_, templates_, thing);
            auto cacheIt = transformDrag_.thingMoveSnapCache.modelBoxesByKey.find(key);
            if (cacheIt == transformDrag_.thingMoveSnapCache.modelBoxesByKey.end())
            {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                ++thingMoveResourceResolveCount_;
#endif
                const std::optional<ThingBox> resolved = resources_
                    ? sed::thing::loadModel3doBox(*resources_, key)
                    : std::optional<ThingBox>{};
                cacheIt = transformDrag_.thingMoveSnapCache.modelBoxesByKey.emplace(
                    key,
                    resolved).first;
            }
            entry->modelBox = cacheIt->second;
            entry->modelBoxReady = true;
            return entry;
        };
        auto ensureThingMoveConnectors = [&](const std::int32_t thingIndex)
            -> render::ThingMoveSnapEntry*
        {
            render::ThingMoveSnapEntry* entry =
                transformDrag_.thingMoveSnapCache.find(thingIndex);
            if (!entry
                || thingIndex < 0
                || static_cast<std::size_t>(thingIndex) >= level_->things.size())
            {
                return entry;
            }
            if (entry->connectorsReady)
            {
                return entry;
            }
            if (!entry->visible || !entry->finite)
            {
                entry->connectorsReady = true;
                return entry;
            }

            const Thing& thing = level_->things[static_cast<std::size_t>(thingIndex)];
            const std::string key = thingMoveConnectorKey(thing);
            auto cacheIt = transformDrag_.thingMoveSnapCache.connectorsByKey.find(key);
            if (cacheIt == transformDrag_.thingMoveSnapCache.connectorsByKey.end())
            {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                ++thingMoveResourceResolveCount_;
#endif
                const std::vector<sed::thing::SnapConnector> resolved =
                    sed::thing::resolveThingSnapConnectors(
                        *level_,
                        templates_,
                        resources_,
                        thing);
                cacheIt = transformDrag_.thingMoveSnapCache.connectorsByKey.emplace(
                    key,
                    resolved).first;
            }
            entry->connectors = cacheIt->second;
            entry->connectorsReady = true;
            return entry;
        };
        auto refreshThingMoveBroadRadius = [&]()
        {
            if (movingFramePose)
            {
                render::ThingMoveSnapEntry* entry = ensureThingMoveLocalBox(
                    frameMovePoseSnapshot.ownerThingIndex);
                if (!entry
                    || !entry->localBoxReady
                    || !std::isfinite(entry->broadRadius))
                {
                    transformDrag_.thingMoveSnapCache.movingBroadRadius =
                        std::numeric_limits<double>::infinity();
                    return;
                }
                // The frame and owning Thing share the same local visual box;
                // Euclidean corner radii are rotation invariant. Center the
                // mover at the virtual frame origin for the culling bound.
                transformDrag_.thingMoveSnapCache.movingBroadRadius =
                    entry->broadRadius;
                return;
            }

            double groupRadius = 0.0;
            for (const std::size_t entryIndex : transformDrag_.thingMoveSnapCache.movingEntries)
            {
                if (entryIndex >= transformDrag_.thingMoveSnapCache.entries.size())
                {
                    continue;
                }
                const std::int32_t thingIndex =
                    transformDrag_.thingMoveSnapCache.entries[entryIndex].index;
                render::ThingMoveSnapEntry* entry =
                    ensureThingMoveLocalBox(thingIndex);
                if (!entry)
                {
                    transformDrag_.thingMoveSnapCache.movingBroadRadius =
                        std::numeric_limits<double>::infinity();
                    return;
                }
                if (!entry->localBoxReady || !std::isfinite(entry->broadRadius))
                {
                    transformDrag_.thingMoveSnapCache.movingBroadRadius =
                        std::numeric_limits<double>::infinity();
                    return;
                }
                const double originRadius = std::sqrt(
                    entry->relativeOrigin.Dot(entry->relativeOrigin));
                groupRadius = std::max(
                    groupRadius,
                    originRadius + entry->broadRadius);
            }
            transformDrag_.thingMoveSnapCache.movingBroadRadius = groupRadius;
        };

        auto spriteWorldPoints =
            [&](const Thing& thing, const assets::SprFile& spr, const QVector3D& translation, const float yawDeltaDeg)
        {
            std::vector<QVector3D> out{};

            TVector pyr{};
            pyr.pitch = thing.pch;
            pyr.yaw = thing.yaw + static_cast<double>(yawDeltaDeg);
            pyr.roll = thing.rol;

            const QVector3D rotatedOrigin = sed::thing::rotatePointZ(
                QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z)),
                pivot,
                yawDeltaDeg);
            QVector3D center = rotatedOrigin + translation;

            const float widthHalf = std::max(0.1f, spr.width * 0.5f);
            const float heightHalf = std::max(0.1f, spr.height * 0.5f);
            const float rollDeg = normalizeRollDeg(static_cast<float>(pyr.roll));

            QVector3D p0{};
            QVector3D p1{};
            QVector3D p2{};
            QVector3D p3{};

            if (spr.type == 2)
            {
                QVector3D yVec = makeSpriteType2Forward(pyr);
                if (!normalizeInPlace(yVec))
                {
                    yVec = QVector3D(0.0f, 1.0f, 0.0f);
                }

                QVector3D xVec{};
                if (std::abs(yVec.z()) < 0.99989998f)
                {
                    xVec = QVector3D::crossProduct(QVector3D(0.0f, 0.0f, 1.0f), yVec);
                }
                else
                {
                    xVec = QVector3D::crossProduct(QVector3D(1.0f, 0.0f, 0.0f), yVec);
                }
                if (!normalizeInPlace(xVec))
                {
                    return out;
                }

                QVector3D zVec = QVector3D::crossProduct(xVec, yVec);
                if (!normalizeInPlace(zVec))
                {
                    return out;
                }

                QVector3D v0 = -xVec * widthHalf - zVec * heightHalf;
                QVector3D v1 = xVec * widthHalf - zVec * heightHalf;
                QVector3D v2 = xVec * widthHalf + zVec * heightHalf;
                QVector3D v3 = -xVec * widthHalf + zVec * heightHalf;
                if (rollDeg != 0.0f)
                {
                    const float rollRad = DegToRad(rollDeg);
                    v0 = rotateAroundAxis(v0, yVec, rollRad);
                    v1 = rotateAroundAxis(v1, yVec, rollRad);
                    v2 = rotateAroundAxis(v2, yVec, rollRad);
                    v3 = rotateAroundAxis(v3, yVec, rollRad);
                }

                p0 = center + v0;
                p1 = center + v1;
                p2 = center + v2;
                p3 = center + v3;
            }
            else
            {
                TVector placementNoRoll = pyr;
                placementNoRoll.roll = 0.0;
                const SpriteBasis basis = makeSpriteBasis(placementNoRoll);
                QVector3D right = basis.right;
                QVector3D up = basis.up;
                QVector3D forward = basis.forward;
                if (!normalizeInPlace(right))
                {
                    right = QVector3D(1.0f, 0.0f, 0.0f);
                }
                if (!normalizeInPlace(forward))
                {
                    forward = QVector3D(0.0f, 1.0f, 0.0f);
                }
                if (!normalizeInPlace(up))
                {
                    up = QVector3D::crossProduct(right, forward);
                    if (!normalizeInPlace(up))
                    {
                        up = QVector3D(0.0f, 0.0f, 1.0f);
                    }
                }

                if (rollDeg != 0.0f)
                {
                    const float rollRad = DegToRad(rollDeg);
                    right = rotateAroundAxis(right, forward, rollRad);
                    up = rotateAroundAxis(up, forward, rollRad);
                }

                center =
                    center
                    + right * spr.offsetXYZ[0]
                    + forward * spr.offsetXYZ[1]
                    + up * spr.offsetXYZ[2];
                const QVector3D r = right * widthHalf;
                const QVector3D u = up * heightHalf;
                p0 = center - r - u;
                p1 = center + r - u;
                p2 = center + r + u;
                p3 = center - r + u;
            }

            out.reserve(5);
            out.push_back(p0);
            out.push_back(p1);
            out.push_back(p2);
            out.push_back(p3);
            out.push_back((p0 + p1 + p2 + p3) * 0.25f);
            return out;
        };

        auto thingSurfaceStickProbePoints = [&](const std::int32_t thingIndex)
        {
            render::ThingMoveSnapEntry* cached =
                transformDrag_.thingMoveSnapCache.find(thingIndex);
            if (cached && cached->surfaceStickPointsReady)
            {
                return cached->surfaceStickPoints;
            }

            auto finish = [cached](std::vector<TVector> points)
            {
                if (cached)
                {
                    cached->surfaceStickPoints = points;
                    cached->surfaceStickPointsReady = true;
                }
                return points;
            };
            std::vector<TVector> out{};
            if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= level_->things.size())
            {
                return finish(std::move(out));
            }
            const Thing& sourceThing = movingThingForIndex(thingIndex);
            Thing thing = sourceThing;
            thing.x -= authoringPivot.x;
            thing.y -= authoringPivot.y;
            thing.z -= authoringPivot.z;
            const std::string spriteName = sed::thing::tplValueString(
                sed::level::thingFindValue(*level_, templates_, thing, "sprite"));
            if (!sed::text::trim(spriteName).empty() && !sed::text::iequalsAscii(sed::text::trim(spriteName), "none"))
            {
                if (const assets::SprFile* spr = loadSprite(spriteName))
                {
                    // Billboard construction is presentation-oriented, so form
                    // it at a zero origin before restoring the authored offset.
                    Thing centered = thing;
                    centered.x = centered.y = centered.z = 0.0;
                    const TVector origin{thing.x, thing.y, thing.z};
                    for (const QVector3D& point : spriteWorldPoints(centered, *spr, QVector3D{}, 0.0f))
                        out.push_back(origin + render::authoringVector(point));
                    return finish(std::move(out));
                }
            }
            if (cached)
            {
                if (render::ThingMoveSnapEntry* ensured =
                        ensureThingMoveModelBox(thingIndex);
                    ensured && ensured->modelBox)
                {
                    return finish(sed::thing::transformThingBoxDocumentPoints(
                        thing,
                        *ensured->modelBox,
                        TVector{},
                        TVector{},
                        0.0,
                        true));
                }
                // The legacy Surface Stick fallback is the authored origin.
                // resolveThingLocalBox may return an editor bbox when a model
                // is missing, but that fallback must not become a rendered
                // support point for Surface Stick.
                return finish(std::vector<TVector>{TVector{thing.x, thing.y, thing.z}});
            }
            if (resources_)
            {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                ++thingMoveResourceResolveCount_;
#endif
                if (const auto box = sed::thing::loadModel3doBox(*resources_,
                        sed::thing::thingModel3doName(*level_, templates_, thing)))
                    return finish(sed::thing::transformThingBoxDocumentPoints(
                        thing, *box, TVector{}, TVector{}, 0.0, true));
            }
            out.push_back(TVector{thing.x, thing.y, thing.z});
            return finish(std::move(out));
        };

        if (surfaceStickActive
            && !gridSnapActive
            && !gridPlaneClutchActive)
        {
            const TVector moveVec = transformDrag_.documentTranslation
                    && render::displayVector(*transformDrag_.documentTranslation) == adjustedTranslation
                ? *transformDrag_.documentTranslation : render::authoringVector(adjustedTranslation);
            const double moveLen = std::hypot(moveVec.x, moveVec.y, moveVec.z);
            const float stickReach = std::clamp(
                std::max(surfaceStickRadius_ * 0.25f,
                         transformDrag_.unitsPerPixel * 22.0f),
                surfaceStickRadius_ * 0.25f,
                surfaceStickRadius_);
            const float releaseDistance = stickReach * 2.75f;
            if (std::isfinite(moveLen) && moveLen > 0.0)
            {
                const TVector moveDir3{moveVec.x / moveLen, moveVec.y / moveLen, moveVec.z / moveLen};
                std::vector<TVector> supportPoints{};
                double bestSupportDot = -std::numeric_limits<double>::infinity();
                for (const std::int32_t idx : movingThings)
                {
                    for (const TVector& corner : thingSurfaceStickProbePoints(idx))
                    {
                        const double dot = corner.Dot(moveDir3);
                        const double eps = std::max(0.001, static_cast<double>(stickReach) * 0.18);
                        if (supportPoints.empty() || dot > bestSupportDot + eps)
                        {
                            bestSupportDot = dot;
                            supportPoints.clear();
                            supportPoints.push_back(corner);
                        }
                        else if (std::abs(dot - bestSupportDot) <= eps)
                        {
                            supportPoints.push_back(corner);
                        }
                    }
                }

                if (supportPoints.empty())
                {
                    supportPoints.push_back(TVector{});
                }

                const TVector rd = moveDir3;

                bool haveStick = false;
                double bestHitAlong = std::numeric_limits<double>::infinity();
                TVector bestHitPos{};
                TVector bestSupport{};
                QVector3D bestSurfaceNormal{};
                const float contactEpsilon = std::clamp(
                    transformDrag_.unitsPerPixel * 2.0f,
                    0.001f,
                    0.015f);
                for (const TVector& supportStart : supportPoints)
                {
                    const TVector ro = authoringPivot + supportStart
                        - moveDir3 * (stickReach * 0.25);
                    const std::vector<sed::level::SurfacePickHit> hits =
                        surfacePickIndex_.pickSurfaces(
                            *level_,
                            ro,
                            rd,
                            static_cast<double>(moveLen + stickReach + releaseDistance));

                    for (const sed::level::SurfacePickHit& hit : hits)
                    {
                        if (sed::thing::isPassThroughPlacementSurface(*level_, hit))
                        {
                            continue;
                        }

                        const std::optional<TVector> normal = sed::thing::surfaceNormalForHit(*level_, hit);
                        if (!normal || normal->Dot(rd) >= -0.025)
                        {
                            continue;
                        }

                        // The picker backs the ray away from the support to
                        // discover nearby faces. Reconstruct the accepted
                        // contact in this authored local frame so subtracting
                        // that artificial ray offset cannot erase fine motion.
                        const Sector& hitSector = level_->sectors[static_cast<std::size_t>(hit.surface.sector)];
                        const Surface& hitSurface = hitSector.surfaces[static_cast<std::size_t>(hit.surface.surface)];
                        const Vertex& planeVertex = hitSector.vertices[static_cast<std::size_t>(hitSurface.vertices.front())];
                        const TVector planePoint = TVector{planeVertex.x,planeVertex.y,planeVertex.z} - authoringPivot;
                        const double hitAlong = normal->Dot(planePoint-supportStart) / normal->Dot(moveDir3);
                        if (!std::isfinite(hitAlong)) continue;
                        const TVector hitPos = supportStart + moveDir3*hitAlong;

                        // Stick when the moved thing's leading rendered bound reaches the surface,
                        // then release after a short push-through distance.
                        if (hitAlong >= -contactEpsilon
                            && moveLen + contactEpsilon >= hitAlong
                            && moveLen <= hitAlong + releaseDistance
                            && (!haveStick || hitAlong < bestHitAlong))
                        {
                            haveStick = true;
                            bestHitAlong = hitAlong;
                            bestHitPos = hitPos;
                            bestSupport = supportStart;
                            bestSurfaceNormal = QVector3D(
                                static_cast<float>(normal->x),
                                static_cast<float>(normal->y),
                                static_cast<float>(normal->z));
                            break;
                        }
                    }
                }

                if (haveStick)
                {
                    transformDrag_.documentTranslation = bestHitPos - bestSupport;
                    adjustedTranslation = render::displayVector(*transformDrag_.documentTranslation);
                    transformDrag_.surfaceStickActive = true;
                    transformDrag_.surfaceStickReleased = false;
                    transformDrag_.surfaceStickAdjusted = *transformDrag_.documentTranslation;
                    transformDrag_.surfaceStickReleaseRaw = moveVec;
                    transformDrag_.surfaceStickReleaseOffset = TVector{};

                    // Surface Alignment is intentionally independent from
                    // Surface Stick. Its held inverter can enable or disable
                    // rotation without changing whether contact is captured.
                    if (surfaceAlignActive
                        && handle == TransformHandle::MoveFree
                        && !movingThings.empty()
                        && bestSurfaceNormal.lengthSquared() > 1.0e-10f)
                    {
                        const std::int32_t primary = movingThings.front();
                        if (primary >= 0 && static_cast<std::size_t>(primary) < level_->things.size())
                        {
                            const Thing& thing = movingThingForIndex(primary);
                            TVector pyr{};
                            pyr.pitch = thing.pch;
                            pyr.yaw = thing.yaw;
                            pyr.roll = thing.rol;
                            const TMatrix43 rotation = TMatrix43::CreateRotate(pyr);

                            const std::string spriteName = sed::text::trim(
                                sed::thing::tplValueString(
                                    sed::level::thingFindValue(*level_, templates_, thing, "sprite")));
                            const bool surfaceAlignedSprite =
                                !spriteName.empty()
                                && !sed::text::iequalsAscii(spriteName, "none")
                                && loadSprite(spriteName) != nullptr;
                            const TVector& localContactAxis = surfaceAlignedSprite
                                ? rotation.lvec
                                : rotation.uvec;
                            QVector3D currentAxis(
                                static_cast<float>(localContactAxis.x),
                                static_cast<float>(localContactAxis.y),
                                static_cast<float>(localContactAxis.z));
                            QVector3D desiredAxis = bestSurfaceNormal;
                            if (normalizeInPlace(currentAxis) && normalizeInPlace(desiredAxis))
                            {
                                const float dot = std::clamp(
                                    QVector3D::dotProduct(currentAxis, desiredAxis),
                                    -1.0f,
                                    1.0f);
                                const TVector exactCross{
                                    static_cast<double>(currentAxis.y())*desiredAxis.z()-static_cast<double>(currentAxis.z())*desiredAxis.y(),
                                    static_cast<double>(currentAxis.z())*desiredAxis.x()-static_cast<double>(currentAxis.x())*desiredAxis.z(),
                                    static_cast<double>(currentAxis.x())*desiredAxis.y()-static_cast<double>(currentAxis.y())*desiredAxis.x()};
                                const double crossLength = std::hypot(exactCross.x, exactCross.y, exactCross.z);
                                QVector3D rotateAxis = crossLength > 0.0
                                    ? QVector3D(static_cast<float>(exactCross.x/crossLength), static_cast<float>(exactCross.y/crossLength), static_cast<float>(exactCross.z/crossLength))
                                    : QVector3D{};
                                if (crossLength == 0.0 && dot < -0.9999f)
                                {
                                    rotateAxis = QVector3D::crossProduct(currentAxis, QVector3D(1.0f, 0.0f, 0.0f));
                                    if (!normalizeInPlace(rotateAxis))
                                    {
                                        rotateAxis = QVector3D::crossProduct(currentAxis, QVector3D(0.0f, 1.0f, 0.0f));
                                        normalizeInPlace(rotateAxis);
                                    }
                                }

                                const float angleDeg = static_cast<float>(std::atan2(crossLength,
                                    static_cast<double>(currentAxis.x())*desiredAxis.x()+static_cast<double>(currentAxis.y())*desiredAxis.y()+static_cast<double>(currentAxis.z())*desiredAxis.z()) * 180.0 / std::numbers::pi);
                                if (rotateAxis.lengthSquared() > 1.0e-10f
                                    && angleDeg > 0.0f
                                    && angleDeg <= snapRotationMaxDegrees_ + 1.0e-4f)
                                {
                                    transformDrag_.snapRotateAxis = rotateAxis;
                                    transformDrag_.snapRotatePivot = render::displayVector(authoringPivot);
                                    transformDrag_.documentSnapRotatePivot = authoringPivot;
                                    transformDrag_.snapRotateAngleDeg = angleDeg;
                                    transformDrag_.documentTranslation = bestHitPos - render::rotateAuthoringVector(
                                        bestSupport, render::authoringVector(rotateAxis), angleDeg);
                                    transformDrag_.surfaceStickAdjusted = *transformDrag_.documentTranslation;
                                    adjustedTranslation = render::displayVector(*transformDrag_.documentTranslation);
                                }
                            }
                        }
                    }
                }
                else if (transformDrag_.surfaceStickActive)
                {
                    transformDrag_.surfaceStickActive = false;
                    transformDrag_.surfaceStickReleased = true;
                    transformDrag_.surfaceStickReleaseRaw = moveVec;
                    transformDrag_.surfaceStickReleaseOffset = transformDrag_.surfaceStickAdjusted - moveVec;
                    transformDrag_.documentTranslation = moveVec + transformDrag_.surfaceStickReleaseOffset;
                    adjustedTranslation = render::displayVector(*transformDrag_.documentTranslation);
                }
                else if (transformDrag_.surfaceStickReleased)
                {
                    const TVector sinceRelease = moveVec - transformDrag_.surfaceStickReleaseRaw;
                    const double releaseLength = std::hypot(transformDrag_.surfaceStickReleaseRaw.x,
                        transformDrag_.surfaceStickReleaseRaw.y, transformDrag_.surfaceStickReleaseRaw.z);
                    const TVector releaseDir = releaseLength > 0.0
                        ? transformDrag_.surfaceStickReleaseRaw * (1.0 / releaseLength) : TVector{};
                    if (releaseDir.Dot(releaseDir) > 1.0e-10f
                        && sinceRelease.Dot(releaseDir) < -contactEpsilon)
                    {
                        transformDrag_.surfaceStickReleased = false;
                        transformDrag_.surfaceStickReleaseOffset = TVector{};
                    }
                    else
                    {
                        transformDrag_.documentTranslation = moveVec + transformDrag_.surfaceStickReleaseOffset;
                        adjustedTranslation = render::displayVector(*transformDrag_.documentTranslation);
                    }
                }
            }
        }
        else
        {
            transformDrag_.surfaceStickActive = false;
            transformDrag_.surfaceStickReleased = false;
            transformDrag_.surfaceStickReleaseOffset = TVector{};
        }

        if (thingSnapActive
            && !gridSnapActive
            && !gridPlaneClutchActive
            && !movingThings.empty()
            && isMoveHandle)
        {
            // All connector/bounds scoring shares a local frame. Only the
            // completed displacement and pivot cross back into document space.
            const QVector3D pivot{};
            const TVector localStartHit = documentStart - authoringPivot;
            const TVector preciseRaw = transformDrag_.documentTranslation
                    && render::displayVector(*transformDrag_.documentTranslation) == adjustedTranslation
                ? *transformDrag_.documentTranslation : render::authoringVector(adjustedTranslation);
            const auto localThing = [&](const Thing& source)
            {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                if (movingFramePose)
                {
                    std::uint64_t copiedFrameValueCount = 0;
                    for (const TplValue& value : source.values)
                    {
                        copiedFrameValueCount +=
                            value.type == ValueType::frame
                                && sed::text::iequalsAscii(value.name, "frame")
                            ? 1u
                            : 0u;
                    }
                    if (copiedFrameValueCount > 0u)
                    {
                        const qulonglong previousCount = property(
                            "sed.frameMoveCopiedFrameValues").toULongLong();
                        setProperty(
                            "sed.frameMoveCopiedFrameValues",
                            QVariant::fromValue<qulonglong>(
                                previousCount + copiedFrameValueCount));
                    }
                }
#endif
                Thing thing = source;
                thing.x -= authoringPivot.x;
                thing.y -= authoringPivot.y;
                thing.z -= authoringPivot.z;
                return thing;
            };
            const float snapReach = std::clamp(
                std::max(thingSnapRadius_ * 0.3125f,
                         transformDrag_.unitsPerPixel * 16.0f),
                thingSnapRadius_ * 0.3125f,
                thingSnapRadius_);
            // The preferred 3DO solver scores an out-of-plane separation with
            // a weighted ellipsoid.  Its accepted candidate can therefore be
            // farther from an anchor center than the old three-radius sphere:
            // sqrt(1 + 2.5^2) + sqrt(2) is less than 5.  The cursor's initial
            // local offset is part of that motion as well.  A nonfinite
            // offset disables this cull rather than dropping every candidate.
            const double localStartHitLength = std::hypot(
                static_cast<double>(localStartHit.x),
                static_cast<double>(localStartHit.y),
                static_cast<double>(localStartHit.z));
            const double broadPhasePadding = std::isfinite(localStartHitLength)
                ? static_cast<double>(snapReach) * 5.0 + localStartHitLength
                : std::numeric_limits<double>::infinity();
            const std::int32_t primary = movingThings.front();
            const Thing* const primaryPose = primary >= 0
                && static_cast<std::size_t>(primary) < level_->things.size()
                ? &movingThingForIndex(primary)
                : nullptr;
            const bool primaryMayUseConnectors = primaryPose
                && sed::thing::isRailOrTrackThingKey(
                    sed::thing::thingModel3doName(
                        *level_,
                        templates_,
                        *primaryPose),
                    sed::thing::thingTemplateKey(*primaryPose));
            std::vector<std::int32_t> thingSnapAnchorIndices{};
            if ((transformDrag_.target == TransformTargetKind::Thing
                    || transformDrag_.target == TransformTargetKind::Frame)
                && !transformDrag_.thingMoveSnapCache.entries.empty())
            {
                refreshThingMoveBroadRadius();
                const bool canUseBroadPhase =
                    std::isfinite(transformDrag_.thingMoveSnapCache.movingBroadRadius)
                    && !primaryMayUseConnectors;
                thingSnapAnchorIndices.reserve(level_->things.size());
                for (const render::ThingMoveSnapEntry& entry :
                    transformDrag_.thingMoveSnapCache.entries)
                {
                    if (isMovingAnchorIndex(entry.index)
                        || !entry.visible
                        || !entry.finite)
                    {
                        continue;
                    }
                    if (!canUseBroadPhase)
                    {
                        thingSnapAnchorIndices.push_back(entry.index);
                        continue;
                    }
                    render::ThingMoveSnapEntry* resolvedAnchor =
                        ensureThingMoveLocalBox(entry.index);
                    if (!resolvedAnchor
                        || !std::isfinite(resolvedAnchor->broadRadius))
                    {
                        thingSnapAnchorIndices.push_back(entry.index);
                        continue;
                    }
                    const TVector movingCenter = preciseRaw;
                    const TVector anchorCenter = resolvedAnchor->relativeOrigin;
                    const double reach =
                        transformDrag_.thingMoveSnapCache.movingBroadRadius
                        + resolvedAnchor->broadRadius
                        + broadPhasePadding;
                    if (movingCenter.DistanceSqr(anchorCenter) <= reach * reach)
                    {
                        thingSnapAnchorIndices.push_back(entry.index);
                    }
                }
            }
            else
            {
                thingSnapAnchorIndices.reserve(level_->things.size());
                for (std::int32_t anchorIndex = 0;
                    anchorIndex < static_cast<std::int32_t>(level_->things.size());
                    ++anchorIndex)
                {
                    thingSnapAnchorIndices.push_back(anchorIndex);
                }
            }
            const bool canRotate =
                thingSnapRotateActive
                && primary >= 0
                && static_cast<std::size_t>(primary) < level_->things.size()
                && handle == TransformHandle::MoveFree;
            const auto candidateYaw = [](const double degrees)
            {
                const float stored = static_cast<float>(std::remainder(degrees, 360.0));
                return stored;
            };
            const auto rotationWithinLimit = [this](const float angleDegrees)
            {
                return std::abs(angleDegrees)
                    <= snapRotationMaxDegrees_ + 1.0e-4f;
            };

            sed::thing::TransformSnapCandidate best{};

            using ThingMoveConnector = sed::thing::SnapConnector;
            bool primaryUsesConnectors = false;
            if (primary >= 0 && static_cast<std::size_t>(primary) < level_->things.size())
            {
                const Thing movingThing = localThing(movingThingForIndex(primary));
                const render::ThingMoveSnapEntry* cachedMoving =
                    ensureThingMoveConnectors(primary);
                const std::vector<ThingMoveConnector> resolvedMovingConnectors =
                    cachedMoving
                    ? std::vector<ThingMoveConnector>{}
                    : [&]()
                    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                        ++thingMoveResourceResolveCount_;
#endif
                        return sed::thing::resolveThingSnapConnectors(
                            *level_, templates_, resources_, movingThing);
                    }();
                const std::vector<ThingMoveConnector>& movingConnectors =
                    cachedMoving
                    ? cachedMoving->connectors
                    : resolvedMovingConnectors;
                primaryUsesConnectors = !movingConnectors.empty();
                if (!movingConnectors.empty()
                    && sed::thing::finiteThingTransform(movingThing))
                {
                    const TVector probeVec{
                        localStartHit.x + preciseRaw.x,
                        localStartHit.y + preciseRaw.y,
                        localStartHit.z + preciseRaw.z
                    };
                    TVector basePyr{};
                    basePyr.pitch = movingThing.pch;
                    basePyr.yaw = movingThing.yaw;
                    basePyr.roll = movingThing.rol;
                    const TVector baseOrigin{
                        movingThing.x + preciseRaw.x,
                        movingThing.y + preciseRaw.y,
                        movingThing.z + preciseRaw.z
                    };
                    const TMatrix43 baseXf = TMatrix43::Create(baseOrigin, basePyr);
                    const TMatrix43 baseRot = TMatrix43::Create(TVector::zero, basePyr);
                    for (const std::int32_t anchorIndex : thingSnapAnchorIndices)
                    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                        ++thingMoveSnapAnchorVisitCount_;
#endif
                        if (isMovingAnchorIndex(anchorIndex))
                        {
                            continue;
                        }
                        const Thing& anchorSource =
                            level_->things[static_cast<std::size_t>(anchorIndex)];
                        const render::ThingMoveSnapEntry* cachedAnchor =
                            ensureThingMoveConnectors(anchorIndex);
                        if (cachedAnchor
                            ? (!cachedAnchor->visible || !cachedAnchor->finite)
                            : !shouldShowThing(anchorSource))
                        {
                            continue;
                        }
                        const Thing anchorThing = localThing(
                            stationaryAnchorPoseForIndex(anchorIndex));
                        const std::vector<ThingMoveConnector> resolvedAnchorConnectors =
                            cachedAnchor
                            ? std::vector<ThingMoveConnector>{}
                            : [&]()
                            {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                                ++thingMoveResourceResolveCount_;
#endif
                                return sed::thing::resolveThingSnapConnectors(
                                    *level_, templates_, resources_, anchorThing);
                            }();
                        const std::vector<ThingMoveConnector>& anchorConnectors =
                            cachedAnchor
                            ? cachedAnchor->connectors
                            : resolvedAnchorConnectors;
                        if (anchorConnectors.empty()
                            || !sed::thing::finiteThingTransform(anchorThing))
                        {
                            continue;
                        }

                        TVector anchorPyr{};
                        anchorPyr.pitch = anchorThing.pch;
                        anchorPyr.yaw = anchorThing.yaw;
                        anchorPyr.roll = anchorThing.rol;
                        const TMatrix43 anchorXf = TMatrix43::Create(TVector{ anchorThing.x, anchorThing.y, anchorThing.z }, anchorPyr);

                        for (const ThingMoveConnector& anchorConnector : anchorConnectors)
                        {
                            const TVector anchorWorld = anchorXf.TransformPoint(anchorConnector.position);
                            const double cursorDistSq = probeVec.DistanceSqr(anchorWorld);
                            std::optional<TVector> anchorTangentWorld =
                                sed::thing::flattenNormalize(anchorXf.TransformVector(anchorConnector.tangent));
                            if (!anchorTangentWorld)
                            {
                                continue;
                            }
                            const TVector desiredMovingTangent = -*anchorTangentWorld;

                            for (const ThingMoveConnector& movingConnector : movingConnectors)
                            {
                                const TVector baseConnectorWorld = baseXf.TransformPoint(movingConnector.position);
                                const double connectorDistSq = baseConnectorWorld.DistanceSqr(anchorWorld);
                                const bool railPairSnap =
                                    sed::thing::connectorHasRailPair(anchorConnector)
                                    && sed::thing::connectorHasRailPair(movingConnector);
                                const double connectorReach =
                                    sed::thing::connectorSnapReach(
                                        static_cast<double>(transformDrag_.unitsPerPixel),
                                        railPairSnap,
                                        static_cast<double>(thingSnapRadius_));
                                const double connectorReachSq = connectorReach * connectorReach;
                                const double probeDistSq = sed::thing::connectorSnapProbeDistanceSq(
                                    connectorDistSq,
                                    cursorDistSq,
                                    railPairSnap);
                                if (probeDistSq > connectorReachSq)
                                {
                                    continue;
                                }

                                if (railPairSnap)
                                {
                                    const std::vector<sed::thing::RailPairSnapCandidate> railCandidates =
                                        sed::thing::buildRailPairSnapCandidates(
                                            movingConnector,
                                            anchorConnector,
                                            basePyr,
                                            baseRot,
                                            anchorXf,
                                            desiredMovingTangent,
                                            canRotate,
                                            0.45,
                                            static_cast<double>(
                                                snapRotationMaxDegrees_), true);

                                    for (const sed::thing::RailPairSnapCandidate& railSnap : railCandidates)
                                    {
                                        const TVector& newOrigin = railSnap.origin;
                                        const float yawDelta = candidateYaw(railSnap.pyr.yaw - movingThing.yaw);
                                        const TVector rotatedMovingOrigin = render::rotateAuthoringVector(
                                TVector{movingThing.x, movingThing.y, movingThing.z}, TVector{0.0, 0.0, 1.0}, yawDelta);
                                        const TVector preciseTranslation = newOrigin - rotatedMovingOrigin;
                                        const QVector3D candidateTranslation = render::displayVector(preciseTranslation);

                                        const double originJumpSq = baseOrigin.DistanceSqr(newOrigin);
                                        const double score =
                                            railSnap.pairResidualSq * 260.0
                                            + connectorDistSq * 0.65
                                            + originJumpSq * 0.030
                                            + railSnap.tangentPenalty;
                                        if (score >= static_cast<double>(best.score))
                                        {
                                            continue;
                                        }

                                        best.translation = candidateTranslation;
                                        best.documentTranslation = preciseTranslation;
                                        best.rotateAngleDeg = yawDelta;
                                        best.rotateAxis = std::abs(yawDelta) > 0.0f
                                            ? QVector3D(0.0f, 0.0f, 1.0f)
                                            : QVector3D{};
                                        best.rotatePivot = pivot;
                                        best.score = score;
                                    }
                                    continue;
                                }

                                TVector newPyr = basePyr;
                                TMatrix43 newRot = baseRot;
                                double tangentPenalty = 0.0;
                                bool rotatedToConnector = false;
                                if (canRotate)
                                {
                                    std::optional<TVector> currentMovingTangent =
                                        sed::thing::flattenNormalize(newRot.TransformVector(movingConnector.tangent));
                                    if (!currentMovingTangent)
                                    {
                                        continue;
                                    }
                                    const float proposedYaw = candidateYaw(sed::thing::yawDeltaDeg(
                                        *currentMovingTangent, desiredMovingTangent));
                                    if (rotationWithinLimit(proposedYaw))
                                    {
                                        newPyr.yaw += proposedYaw;
                                        newRot = TMatrix43::Create(
                                            TVector::zero, newPyr);
                                        rotatedToConnector = true;
                                    }
                                }
                                if (!rotatedToConnector)
                                {
                                    std::optional<TVector> movingTangentWorld =
                                        sed::thing::flattenNormalize(newRot.TransformVector(movingConnector.tangent));
                                    if (movingTangentWorld)
                                    {
                                        const double tangentDot = std::clamp(
                                            movingTangentWorld->Dot(*anchorTangentWorld),
                                            -1.0,
                                            1.0);
                                        tangentPenalty = (1.0 + tangentDot) * (1.0 + tangentDot) * 0.015;
                                    }
                                }

                                const TVector newOrigin = anchorWorld - newRot.TransformVector(movingConnector.position);
                                const double originJumpSq = baseOrigin.DistanceSqr(newOrigin);
                                const double score =
                                    probeDistSq
                                    + originJumpSq * 0.035
                                    + (cursorDistSq > connectorReachSq ? connectorDistSq * 0.25 : cursorDistSq * 0.05)
                                    + tangentPenalty;
                                if (score >= static_cast<double>(best.score))
                                {
                                    continue;
                                }

                                const float yawDelta = candidateYaw(newPyr.yaw - movingThing.yaw);
                                const TVector rotatedMovingOrigin = render::rotateAuthoringVector(
                                TVector{movingThing.x, movingThing.y, movingThing.z}, TVector{0.0, 0.0, 1.0}, yawDelta);
                                best.documentTranslation = newOrigin - rotatedMovingOrigin;
                                best.translation = render::displayVector(*best.documentTranslation);
                                best.rotateAngleDeg = yawDelta;
                                best.rotateAxis = std::abs(yawDelta) > 0.0f
                                    ? QVector3D(0.0f, 0.0f, 1.0f)
                                    : QVector3D{};
                                best.rotatePivot = pivot;
                                best.score = score;
                            }
                        }
                    }
                }
            }

            if (!std::isfinite(best.score)
                && handle == TransformHandle::MoveFree
                && !primaryUsesConnectors
                && primary >= 0
                && static_cast<std::size_t>(primary) < level_->things.size())
            {
                const Thing movingThing = localThing(movingThingForIndex(primary));
                const render::ThingMoveSnapEntry* cachedMoving =
                    ensureThingMoveModelBox(primary);
                if (!cachedMoving)
                {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                    ++thingMoveResourceResolveCount_;
#endif
                }
                const std::optional<ThingBox> movingBox = cachedMoving
                    ? cachedMoving->modelBox
                    : resources_
                        ? sed::thing::loadModel3doBox(
                            *resources_,
                            sed::thing::thingModel3doName(*level_, templates_, movingThing))
                        : std::optional<ThingBox>{};
                refreshThingMoveBroadRadius();
                if (movingBox && !movingBox->IsZero()
                    && sed::thing::finiteThingTransform(movingThing))
                {
                    const sed::thing::BoxMinMax movingBounds = sed::thing::boxMinMax(*movingBox);
                    const std::array<double, 3>& movingMin = movingBounds.min;
                    const std::array<double, 3>& movingMax = movingBounds.max;
                    const std::array<double, 3> movingCenter = sed::thing::boxCenter(movingBounds);

                    const TVector probeVec{
                        localStartHit.x + preciseRaw.x,
                        localStartHit.y + preciseRaw.y,
                        localStartHit.z + preciseRaw.z
                    };
                    TVector basePyr{};
                    basePyr.pitch = movingThing.pch;
                    basePyr.yaw = movingThing.yaw;
                    basePyr.roll = movingThing.rol;
                    const TVector baseOrigin{
                        movingThing.x + preciseRaw.x,
                        movingThing.y + preciseRaw.y,
                        movingThing.z + preciseRaw.z
                    };
                    const TMatrix43 baseRot = TMatrix43::Create(TVector::zero, basePyr);

                    for (const std::int32_t anchorIndex : thingSnapAnchorIndices)
                    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                        ++thingMoveSnapAnchorVisitCount_;
#endif
                        if (isMovingAnchorIndex(anchorIndex))
                        {
                            continue;
                        }

                        const Thing& anchorSource =
                            level_->things[static_cast<std::size_t>(anchorIndex)];
                        const render::ThingMoveSnapEntry* cachedAnchor =
                            ensureThingMoveModelBox(anchorIndex);
                        if (cachedAnchor
                            ? (!cachedAnchor->visible || !cachedAnchor->finite)
                            : !shouldShowThing(anchorSource))
                        {
                            continue;
                        }
                        const Thing anchorThing = localThing(
                            stationaryAnchorPoseForIndex(anchorIndex));
                        if (!cachedAnchor)
                        {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                            ++thingMoveBoundsResolveCount_;
                            ++thingMoveResourceResolveCount_;
#endif
                        }
                        const std::optional<ThingBox> anchorBox = cachedAnchor
                            ? cachedAnchor->modelBox
                            : resources_
                                ? sed::thing::loadModel3doBox(
                                    *resources_,
                                    sed::thing::thingModel3doName(*level_, templates_, anchorThing))
                                : std::optional<ThingBox>{};
                        if (!anchorBox || anchorBox->IsZero()
                            || !sed::thing::finiteThingTransform(anchorThing))
                        {
                            continue;
                        }

                        if (cachedAnchor
                            && !canRotate
                            && std::isfinite(transformDrag_.thingMoveSnapCache.movingBroadRadius)
                            && std::isfinite(cachedAnchor->broadRadius))
                        {
                            const TVector movingCenter = preciseRaw;
                            const TVector anchorCenter = cachedAnchor->relativeOrigin;
                            const double reach =
                                transformDrag_.thingMoveSnapCache.movingBroadRadius
                                + cachedAnchor->broadRadius
                                + broadPhasePadding;
                            if (movingCenter.DistanceSqr(anchorCenter) > reach * reach)
                            {
                                continue;
                            }
                        }

                        const sed::thing::BoxMinMax anchorBounds = sed::thing::boxMinMax(*anchorBox);
                        const std::array<double, 3>& anchorMin = anchorBounds.min;
                        const std::array<double, 3>& anchorMax = anchorBounds.max;
                        const std::array<double, 3> anchorCenter = sed::thing::boxCenter(anchorBounds);

                        TVector anchorPyr{};
                        anchorPyr.pitch = anchorThing.pch;
                        anchorPyr.yaw = anchorThing.yaw;
                        anchorPyr.roll = anchorThing.rol;
                        const TMatrix43 anchorXf = TMatrix43::Create(TVector{ anchorThing.x, anchorThing.y, anchorThing.z }, anchorPyr);
                        const TMatrix43 invAnchorXf = anchorXf.InvertOrtho();
                        const TVector localProbe = invAnchorXf.TransformPoint(probeVec);

                        // For ordinary 3DO models, prefer horizontal side-to-side joining. Vertical stacking
                        // still works through surface placement, while move snapping stays predictable for blocks.
                        constexpr int normalAxis = 2;
                        for (int snapAxis = 0; snapAxis < 3; ++snapAxis)
                        {
                            if (snapAxis == normalAxis)
                            {
                                continue;
                            }

                            const double pAxis = sed::thing::vectorComponent(localProbe, snapAxis);
                            const double distToMin = std::abs(pAxis - anchorMin[static_cast<std::size_t>(snapAxis)]);
                            const double distToMax = std::abs(pAxis - anchorMax[static_cast<std::size_t>(snapAxis)]);
                            const int sign =
                                (pAxis > anchorMax[static_cast<std::size_t>(snapAxis)]) ? 1
                                : (pAxis < anchorMin[static_cast<std::size_t>(snapAxis)]) ? -1
                                : (distToMax <= distToMin ? 1 : -1);
                            const double faceDistance = sign > 0 ? distToMax : distToMin;

                            double lateralPenaltySq = 0.0;
                            for (int axis = 0; axis < 3; ++axis)
                            {
                                if (axis == snapAxis)
                                {
                                    continue;
                                }
                                const double allowedReach = axis == normalAxis
                                    ? static_cast<double>(snapReach) * 2.5
                                    : static_cast<double>(snapReach);
                                const double outside = sed::thing::outsideDistance(
                                    sed::thing::vectorComponent(localProbe, axis),
                                    anchorMin[static_cast<std::size_t>(axis)] - allowedReach,
                                    anchorMax[static_cast<std::size_t>(axis)] + allowedReach);
                                lateralPenaltySq += outside * outside * (axis == normalAxis ? 0.5 : 2.0);
                            }

                            const double faceScore = (faceDistance * faceDistance) + lateralPenaltySq;
                            if (faceScore > static_cast<double>(snapReach) * static_cast<double>(snapReach))
                            {
                                continue;
                            }

                            const double anchorFace = sign > 0
                                ? anchorMax[static_cast<std::size_t>(snapAxis)]
                                : anchorMin[static_cast<std::size_t>(snapAxis)];
                            TVector anchorContactLocal{};
                            for (int axis = 0; axis < 3; ++axis)
                            {
                                sed::thing::setVectorComponent(anchorContactLocal, axis, anchorCenter[static_cast<std::size_t>(axis)]);
                            }
                            sed::thing::setVectorComponent(anchorContactLocal, snapAxis, anchorFace);
                            const TVector anchorContactWorld = anchorXf.TransformPoint(anchorContactLocal);

                            TVector newPyr = basePyr;
                            TMatrix43 newRot = baseRot;
                            int attachAxis = 0;
                            int attachSign = 1;
                            const float proposedYawDelta = candidateYaw(anchorPyr.yaw - movingThing.yaw);
                            const bool rotateThisCandidate = canRotate
                                && rotationWithinLimit(proposedYawDelta);
                            if (rotateThisCandidate)
                            {
                                newPyr = basePyr;
                                newPyr.yaw += proposedYawDelta;
                                newRot = TMatrix43::Create(TVector::zero, newPyr);
                                attachAxis = snapAxis;
                                attachSign = -sign;
                            }
                            else
                            {
                                TVector anchorOut = sed::thing::matrixAxisVector(anchorXf, snapAxis).Scaled(static_cast<double>(sign));
                                if (anchorOut.Normalize() == 0.0)
                                {
                                    continue;
                                }

                                double bestDot = -std::numeric_limits<double>::infinity();
                                for (int axis = 0; axis < 3; ++axis)
                                {
                                    TVector movingAxis = sed::thing::matrixAxisVector(newRot, axis);
                                    if (movingAxis.Normalize() == 0.0)
                                    {
                                        continue;
                                    }
                                    for (const int sideSign : { -1, 1 })
                                    {
                                        const double dot = movingAxis.Scaled(static_cast<double>(sideSign)).Dot(-anchorOut);
                                        if (dot > bestDot)
                                        {
                                            bestDot = dot;
                                            attachAxis = axis;
                                            attachSign = sideSign;
                                        }
                                    }
                                }
                            }

                            // This preferred face-center solver is only valid
                            // when the pose our yaw-only commit can represent
                            // aligns both boxes. Otherwise the bounds solver
                            // below computes support using the actual pose.
                            if (newRot.rvec.DistanceSqr(anchorXf.rvec) > 1e-20
                                || newRot.lvec.DistanceSqr(anchorXf.lvec) > 1e-20
                                || newRot.uvec.DistanceSqr(anchorXf.uvec) > 1e-20)
                                continue;

                            TVector movingContactLocal{};
                            for (int axis = 0; axis < 3; ++axis)
                            {
                                sed::thing::setVectorComponent(movingContactLocal, axis, movingCenter[static_cast<std::size_t>(axis)]);
                            }
                            sed::thing::setVectorComponent(
                                movingContactLocal,
                                attachAxis,
                                attachSign > 0
                                    ? movingMax[static_cast<std::size_t>(attachAxis)]
                                    : movingMin[static_cast<std::size_t>(attachAxis)]);

                            const TVector newOrigin = anchorContactWorld - newRot.TransformVector(movingContactLocal);
                            const float yawDelta = candidateYaw(newPyr.yaw - movingThing.yaw);
                            const TVector rotatedMovingOrigin = render::rotateAuthoringVector(
                                TVector{movingThing.x, movingThing.y, movingThing.z}, TVector{0.0, 0.0, 1.0}, yawDelta);
                            const TVector preciseTranslation = newOrigin - rotatedMovingOrigin;
                            const QVector3D candidateTranslation = render::displayVector(preciseTranslation);

                            const TVector finalOrigin = rotatedMovingOrigin + preciseTranslation;
                            const TVector finalContact = finalOrigin + newRot.TransformVector(movingContactLocal);
                            const double contactResidualSq = finalContact.DistanceSqr(anchorContactWorld);
                            const double originJumpSq = baseOrigin.DistanceSqr(newOrigin);
                            const double score =
                                contactResidualSq * 200.0
                                + faceScore
                                + originJumpSq * 0.025;
                            if (score >= static_cast<double>(best.score))
                            {
                                continue;
                            }

                            best.translation = candidateTranslation;
                            best.documentTranslation = preciseTranslation;
                            best.rotateAngleDeg = yawDelta;
                            best.rotateAxis = std::abs(yawDelta) > 0.0f
                                ? QVector3D(0.0f, 0.0f, 1.0f)
                                : QVector3D{};
                            best.rotatePivot = pivot;
                            best.score = score;
                        }
                    }
                }
            }

            if (!std::isfinite(best.score)
                && handle == TransformHandle::MoveFree
                && !primaryUsesConnectors
                && primary >= 0
                && static_cast<std::size_t>(primary) < level_->things.size())
            {
                const Thing movingThing = localThing(movingThingForIndex(primary));
                const render::ThingMoveSnapEntry* cachedMoving =
                    ensureThingMoveLocalBox(primary);
                if (!cachedMoving)
                {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                    ++thingMoveBoundsResolveCount_;
                    ++thingMoveResourceResolveCount_;
#endif
                }
                const std::optional<ThingBox> movingBox = cachedMoving
                    ? cachedMoving->localBox
                    : sed::thing::resolveThingLocalBox(
                        *level_,
                        templates_,
                        resources_,
                        movingThing,
                        static_cast<double>(thingCubeHalfSize_));
                refreshThingMoveBroadRadius();
                if (movingBox && !movingBox->IsZero())
                {
                    for (const std::int32_t anchorIndex : thingSnapAnchorIndices)
                    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                        ++thingMoveSnapAnchorVisitCount_;
#endif
                        if (isMovingAnchorIndex(anchorIndex))
                        {
                            continue;
                        }

                        const Thing& anchorSource =
                            level_->things[static_cast<std::size_t>(anchorIndex)];
                        const render::ThingMoveSnapEntry* cachedAnchor =
                            ensureThingMoveLocalBox(anchorIndex);
                        if (cachedAnchor
                            ? (!cachedAnchor->visible || !cachedAnchor->finite)
                            : !shouldShowThing(anchorSource))
                        {
                            continue;
                        }
                        const Thing anchorThing = localThing(
                            stationaryAnchorPoseForIndex(anchorIndex));
                        if (!cachedAnchor)
                        {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                            ++thingMoveBoundsResolveCount_;
                            ++thingMoveResourceResolveCount_;
#endif
                        }
                        const std::optional<ThingBox> anchorBox = cachedAnchor
                            ? cachedAnchor->localBox
                            : sed::thing::resolveThingLocalBox(
                                *level_,
                                templates_,
                                resources_,
                                anchorThing,
                                static_cast<double>(thingCubeHalfSize_));
                        if (!anchorBox || anchorBox->IsZero())
                        {
                            continue;
                        }
                        if (!sed::thing::finiteThingTransform(movingThing)
                            || !sed::thing::finiteThingTransform(anchorThing))
                        {
                            continue;
                        }

                        if (cachedAnchor
                            && !canRotate
                            && std::isfinite(transformDrag_.thingMoveSnapCache.movingBroadRadius)
                            && std::isfinite(cachedAnchor->broadRadius))
                        {
                            const TVector movingCenter = preciseRaw;
                            const TVector anchorCenter = cachedAnchor->relativeOrigin;
                            const double reach =
                                transformDrag_.thingMoveSnapCache.movingBroadRadius
                                + cachedAnchor->broadRadius
                                + broadPhasePadding;
                            if (movingCenter.DistanceSqr(anchorCenter) > reach * reach)
                            {
                                continue;
                            }
                        }

                        float yawDeltaDeg = 0.0f;
                        if (canRotate)
                        {
                            yawDeltaDeg = candidateYaw(anchorThing.yaw - movingThing.yaw);
                            if (!rotationWithinLimit(yawDeltaDeg))
                            {
                                yawDeltaDeg = 0.0f;
                            }
                        }

                        TVector movingPyr{};
                        movingPyr.pitch = movingThing.pch;
                        movingPyr.yaw = movingThing.yaw + static_cast<double>(yawDeltaDeg);
                        movingPyr.roll = movingThing.rol;

                        const TVector rotatedMovingOrigin = render::rotateAuthoringVector(
                            TVector{movingThing.x, movingThing.y, movingThing.z}, TVector{0.0,0.0,1.0}, yawDeltaDeg);
                        const TMatrix43 movingXf = TMatrix43::Create(rotatedMovingOrigin + preciseRaw, movingPyr);

                        TVector anchorPyr{};
                        anchorPyr.pitch = anchorThing.pch;
                        anchorPyr.yaw = anchorThing.yaw;
                        anchorPyr.roll = anchorThing.rol;
                        const TMatrix43 anchorXf = TMatrix43::Create(TVector{ anchorThing.x, anchorThing.y, anchorThing.z }, anchorPyr);
                        const TMatrix43 invAnchorXf = anchorXf.InvertOrtho();

                        sed::thing::LocalBounds3 movingInAnchor{};
                        for (const TVector& corner : sed::thing::thingBoxCorners(*movingBox))
                        {
                            sed::thing::addPointToBounds(
                                movingInAnchor,
                                invAnchorXf.TransformPoint(movingXf.TransformPoint(corner)));
                        }
                        if (!movingInAnchor.valid)
                        {
                            continue;
                        }

                        const sed::thing::BoxMinMax anchorBounds = sed::thing::boxMinMax(*anchorBox);
                        const std::array<double, 3>& anchorMin = anchorBounds.min;
                        const std::array<double, 3>& anchorMax = anchorBounds.max;
                        for (int axis = 0; axis < 2; ++axis)
                        {
                            const std::array<double, 2> corrections = {
                                anchorMin[static_cast<std::size_t>(axis)] - movingInAnchor.max[static_cast<std::size_t>(axis)],
                                anchorMax[static_cast<std::size_t>(axis)] - movingInAnchor.min[static_cast<std::size_t>(axis)],
                            };

                            for (const double correction : corrections)
                            {
                                if (std::abs(correction) > static_cast<double>(snapReach))
                                {
                                    continue;
                                }

                                double lateralPenalty = 0.0;
                                for (int other = 0; other < 3; ++other)
                                {
                                    if (other == axis)
                                    {
                                        continue;
                                    }
                                    const double outside = sed::thing::intervalOutsideDistance(
                                        movingInAnchor.min[static_cast<std::size_t>(other)],
                                        movingInAnchor.max[static_cast<std::size_t>(other)],
                                        anchorMin[static_cast<std::size_t>(other)] - static_cast<double>(snapReach),
                                        anchorMax[static_cast<std::size_t>(other)] + static_cast<double>(snapReach));
                                    lateralPenalty += static_cast<double>(outside) * static_cast<double>(outside);
                                }
                                if (lateralPenalty > static_cast<double>(snapReach) * static_cast<double>(snapReach))
                                {
                                    continue;
                                }

                                const double score = correction * correction + lateralPenalty * 2.0;
                                if (score >= static_cast<double>(best.score))
                                {
                                    continue;
                                }

                                TVector localCorrection{};
                                sed::thing::setVectorComponent(localCorrection, axis, correction);
                                const TVector worldCorrection = anchorXf.TransformVector(localCorrection);
                                best.documentTranslation = preciseRaw + worldCorrection;
                                best.translation = render::displayVector(*best.documentTranslation);
                                best.rotateAngleDeg = yawDeltaDeg;
                                best.rotateAxis = std::abs(yawDeltaDeg) > 0.0f
                                    ? QVector3D(0.0f, 0.0f, 1.0f)
                                    : QVector3D{};
                                best.rotatePivot = pivot;
                                best.score = score;
                            }
                        }
                    }
                }
            }

            if (!std::isfinite(best.score)
                && handle == TransformHandle::MoveFree)
            {
            std::optional<sed::thing::LocalBounds3> cachedMovingBounds{};
            if (!movingFramePose && !canRotate
                && transformDrag_.target == TransformTargetKind::Thing
                && !transformDrag_.thingMoveSnapCache.movingEntries.empty())
            {
                refreshThingMoveBroadRadius();
                sed::thing::LocalBounds3 movingBounds{};
                for (const std::size_t entryIndex : transformDrag_.thingMoveSnapCache.movingEntries)
                {
                    if (entryIndex >= transformDrag_.thingMoveSnapCache.entries.size())
                    {
                        continue;
                    }
                    const render::ThingMoveSnapEntry& entry =
                        transformDrag_.thingMoveSnapCache.entries[entryIndex];
                    if (!entry.localBox)
                    {
                        continue;
                    }
                    const Thing& sourceThing = level_->things[static_cast<std::size_t>(entry.index)];
                    Thing localThing = sourceThing;
                    localThing.x = entry.relativeOrigin.x;
                    localThing.y = entry.relativeOrigin.y;
                    localThing.z = entry.relativeOrigin.z;
                    for (const TVector& point : sed::thing::transformThingBoxDocumentPoints(
                             localThing,
                             *entry.localBox,
                             TVector{},
                             preciseRaw,
                             0.0,
                             false))
                    {
                        sed::thing::addPointToBounds(
                            movingBounds,
                            point);
                    }
                }
                if (movingBounds.valid)
                {
                    cachedMovingBounds = movingBounds;
                }
            }
            for (const std::int32_t anchorIndex : thingSnapAnchorIndices)
            {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                ++thingMoveSnapAnchorVisitCount_;
#endif
                if (isMovingAnchorIndex(anchorIndex)) continue;
                const Thing& anchorSource =
                    level_->things[static_cast<std::size_t>(anchorIndex)];
                const render::ThingMoveSnapEntry* cachedAnchor =
                    ensureThingMoveLocalBox(anchorIndex);
                if (cachedAnchor
                    ? (!cachedAnchor->visible || !cachedAnchor->finite)
                    : (!shouldShowThing(anchorSource)
                        || !sed::thing::finiteThingTransform(anchorSource)))
                {
                    continue;
                }
                const Thing anchorThing = localThing(
                    stationaryAnchorPoseForIndex(anchorIndex));
                if (!cachedAnchor)
                {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                    ++thingMoveBoundsResolveCount_;
                    ++thingMoveResourceResolveCount_;
#endif
                }
                const std::optional<ThingBox> anchorBox = cachedAnchor
                    ? cachedAnchor->localBox
                    : sed::thing::resolveThingLocalBox(
                        *level_,
                        templates_,
                        resources_,
                        anchorThing,
                        static_cast<double>(thingCubeHalfSize_));
                if (!anchorBox) continue;
                sed::thing::LocalBounds3 anchorBounds{};
                if (cachedAnchor && cachedAnchor->staticBounds.valid)
                {
                    anchorBounds = cachedAnchor->staticBounds;
                }
                else
                {
                    for (const TVector& point : sed::thing::transformThingBoxDocumentPoints(
                             anchorThing,
                             *anchorBox,
                             TVector{},
                             TVector{},
                             0.0,
                             false))
                    {
                        sed::thing::addPointToBounds(anchorBounds, point);
                    }
                }
                if (!anchorBounds.valid) continue;

                if (cachedAnchor
                    && cachedMovingBounds
                    && std::isfinite(cachedAnchor->broadRadius)
                    && std::isfinite(transformDrag_.thingMoveSnapCache.movingBroadRadius))
                {
                    const TVector movingCenter = preciseRaw;
                    const TVector anchorCenter = cachedAnchor->relativeOrigin;
                    const double reach =
                        transformDrag_.thingMoveSnapCache.movingBroadRadius
                        + cachedAnchor->broadRadius
                        + broadPhasePadding;
                    if (movingCenter.DistanceSqr(anchorCenter) > reach * reach)
                    {
                        continue;
                    }
                }

                float yawDeltaDeg = 0.0f;
                if (canRotate)
                {
                    const Thing movingThing = localThing(movingThingForIndex(primary));
                    const Thing anchorThing = localThing(
                        stationaryAnchorPoseForIndex(anchorIndex));
                    if (std::isfinite(movingThing.yaw) && std::isfinite(anchorThing.yaw))
                    {
                        yawDeltaDeg = candidateYaw(anchorThing.yaw - movingThing.yaw);
                        if (!rotationWithinLimit(yawDeltaDeg))
                        {
                            yawDeltaDeg = 0.0f;
                        }
                    }
                }

                sed::thing::LocalBounds3 moving{};
                if (cachedMovingBounds && !canRotate)
                {
                    moving = *cachedMovingBounds;
                }
                else
                {
                    for (const std::int32_t thingIndex : movingThings)
                    {
                        const Thing thing = localThing(movingThingForIndex(thingIndex));
                        const render::ThingMoveSnapEntry* cachedMoving =
                            ensureThingMoveLocalBox(thingIndex);
                        if (!cachedMoving)
                        {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                            ++thingMoveBoundsResolveCount_;
                            ++thingMoveResourceResolveCount_;
#endif
                        }
                        const std::optional<ThingBox> box = cachedMoving
                            ? cachedMoving->localBox
                            : sed::thing::resolveThingLocalBox(
                                *level_,
                                templates_,
                                resources_,
                                thing,
                                static_cast<double>(thingCubeHalfSize_));
                        if (!box) continue;
                        for (const TVector& point : sed::thing::transformThingBoxDocumentPoints(
                                 thing,
                                 *box,
                                 TVector{},
                                 preciseRaw,
                                 yawDeltaDeg,
                                 false))
                        {
                            sed::thing::addPointToBounds(moving, point);
                        }
                    }
                }
                if (!moving.valid)
                {
                    continue;
                }

                const auto intervalSeparation = [](double minA, double maxA, double minB, double maxB)
                { return std::max({0.0, minA - maxB, minB - maxA}); };
                for (int axis = 0; axis < 2; ++axis)
                {
                    const double moveMin = moving.min[static_cast<std::size_t>(axis)];
                    const double moveMax = moving.max[static_cast<std::size_t>(axis)];
                    const double anchorMin = anchorBounds.min[static_cast<std::size_t>(axis)];
                    const double anchorMax = anchorBounds.max[static_cast<std::size_t>(axis)];
                    const std::array<double, 2> corrections = {
                        anchorMin - moveMax,
                        anchorMax - moveMin,
                    };

                    for (const double correction : corrections)
                    {
                        if (std::abs(correction) > snapReach)
                        {
                            continue;
                        }

                        double lateralPenalty = 0.0;
                        for (int other = 0; other < 3; ++other)
                        {
                            if (other == axis)
                            {
                                continue;
                            }
                            const double outside = intervalSeparation(
                                moving.min[static_cast<std::size_t>(other)],
                                moving.max[static_cast<std::size_t>(other)],
                                anchorBounds.min[static_cast<std::size_t>(other)] - snapReach,
                                anchorBounds.max[static_cast<std::size_t>(other)] + snapReach);
                            lateralPenalty += outside * outside;
                        }
                        if (lateralPenalty > snapReach * snapReach)
                        {
                            continue;
                        }

                        const double score = correction * correction + lateralPenalty * 2.0;
                        if (score >= best.score)
                        {
                            continue;
                        }

                        best.documentTranslation = preciseRaw
                            + render::authoringVector(sed::thing::worldAxisVector(axis)) * correction;
                        best.translation = render::displayVector(*best.documentTranslation);
                        best.rotateAngleDeg = yawDeltaDeg;
                        best.rotateAxis = std::abs(yawDeltaDeg) > 0.0f
                            ? QVector3D(0.0f, 0.0f, 1.0f)
                            : QVector3D{};
                        best.rotatePivot = pivot;
                        best.score = score;
                    }
                }
            }
            }

            if (std::isfinite(best.score))
            {
                adjustedTranslation = best.translation;
                transformDrag_.snapRotateAxis = best.rotateAxis;
                transformDrag_.documentTranslation = best.documentTranslation.value_or(render::authoringVector(best.translation));
                transformDrag_.snapRotatePivot = render::displayVector(authoringPivot);
                transformDrag_.documentSnapRotatePivot = authoringPivot;
                transformDrag_.snapRotateAngleDeg = best.rotateAngleDeg;
            }
        }

        transformDrag_.translation = adjustedTranslation;
        transformPreviewMatrix_.setToIdentity();
        if (transformDrag_.snapRotateAxis.lengthSquared() > 1.0e-10f
            && std::abs(transformDrag_.snapRotateAngleDeg) > 0.0f)
        {
            transformPreviewMatrix_.translate(transformDrag_.translation);
            transformPreviewMatrix_.translate(transformDrag_.snapRotatePivot);
            transformPreviewMatrix_.rotate(transformDrag_.snapRotateAngleDeg, transformDrag_.snapRotateAxis);
            transformPreviewMatrix_.translate(-transformDrag_.snapRotatePivot);
        }
        else
        {
            transformPreviewMatrix_.translate(transformDrag_.translation);
        }
    }
    else if (level_ && isMoveHandle)
    {
        const bool geometryMoveTarget =
            transformDrag_.target == TransformTargetKind::Sector
            || transformDrag_.target == TransformTargetKind::Surface;
        if (geometryMoveTarget)
        {
            transformDrag_.snapRotateAxis = QVector3D{};
            transformDrag_.snapRotatePivot = QVector3D{};
            transformDrag_.snapRotateAngleDeg = 0.0f;

            QVector3D adjustedTranslation = transformDrag_.gridTranslation;

            // Step 1: Resolve the live snap state. Sector move snap uses the
            // viewport option button as its default and the configured
            // modifier as a temporary inverter while dragging.
            const bool sectorAlignSnapActive =
                transformDrag_.target != TransformTargetKind::Sector
                || ui::shortcuts::modifierToggledSetting(
                    sectorMoveSnapAlignEnabled_,
                    held,
                    sectorMoveSnapAlignInvertModifiers_);
            // The exact combined grid/nearby binding is an explicit
            // composition chord: grid first, then nearby geometry. The shared
            // resolver keeps ordinary shortcut inverters exact-match-only.
            const bool surfaceNearbySnapActive =
                transformDrag_.target != TransformTargetKind::Surface
                || surfaceSnapState.nearbyGeometrySnap;

            // Step 2: Reuse the existing geometry snap solver to preview the
            // exact commit transform whenever snapping is active.
            // Once the grid-plane clutch has captured, keep it authoritative
            // for the rest of this drag. A later nearby-geometry correction
            // would otherwise pull the held support point off the plane or
            // introduce a jump on the first released sample.
            const bool gridPlaneClutchActive =
                transformDrag_.gridPlaneStick.holding
                || transformDrag_.gridPlaneStick.released;
            if (sectorAlignSnapActive
                && surfaceNearbySnapActive
                && (!gridSnapActive
                    || transformDrag_.target == TransformTargetKind::Surface)
                && !gridPlaneClutchActive)
            {
                const QVector3D constraintAxis =
                    handle == TransformHandle::MoveFree ? QVector3D{} : axis;
                const float captureDistance =
                    transformDrag_.surfaceMode == SurfaceSubMode::Vertex
                    ? vertexSnapRadius_
                    : transformDrag_.surfaceMode == SurfaceSubMode::Edge
                    ? edgeSnapRadius_
                    : faceSnapRadius_;
                const render::SurfaceMoveSnapSolver surfaceSnapSolver =
                    transformDrag_.surfaceMode == SurfaceSubMode::Vertex
                    ? render::resolveSurfaceMoveSnapSolver(
                        render::SurfaceMoveSnapElement::Vertex,
                        transformDrag_.geometryMoveSnapCache.movingVertex
                            .has_value())
                    : transformDrag_.surfaceMode == SurfaceSubMode::Edge
                    ? render::resolveSurfaceMoveSnapSolver(
                        render::SurfaceMoveSnapElement::Edge,
                        transformDrag_.geometryMoveSnapCache.movingEdge
                            .has_value())
                    : render::resolveSurfaceMoveSnapSolver(
                        render::SurfaceMoveSnapElement::Face, true);
                if (transformDrag_.target == TransformTargetKind::Surface
                    && surfaceSnapSolver
                        == render::SurfaceMoveSnapSolver::Vertex)
                {
                    TVector preciseVertexTranslation{};
                    if (const std::optional<QVector3D> snap =
                            render::snapVertexMoveTranslation(
                                transformDrag_.geometryMoveSnapCache
                                    .movingVertex.value(),
                                transformDrag_.documentTranslation.value_or(render::authoringVector(transformDrag_.gridTranslation)),
                                transformDrag_.geometryMoveSnapCache
                                    .stationaryVertices,
                                captureDistance,
                                constraintAxis,
                                &preciseVertexTranslation))
                    {
                        adjustedTranslation = *snap;
                        transformDrag_.documentTranslation = preciseVertexTranslation;
                    }
                }
                else if (transformDrag_.target == TransformTargetKind::Surface
                    && surfaceSnapSolver
                        == render::SurfaceMoveSnapSolver::Edge)
                {
                    if (const std::optional<render::SurfaceEdgeMoveSnapResult> snap =
                            render::snapEdgeMoveTransform(
                                *transformDrag_.geometryMoveSnapCache.movingEdge,
                                transformDrag_.documentTranslation.value_or(render::authoringVector(transformDrag_.gridTranslation)),
                                transformDrag_.geometryMoveSnapCache
                                    .stationaryEdges,
                                captureDistance,
                                static_cast<float>(std::cos(deg2Rad(
                                    static_cast<double>(std::min(
                                        snapRotationMaxDegrees_,
                                        5.731967f))))),
                                constraintAxis))
                    {
                        adjustedTranslation = snap->translation;
                        transformDrag_.documentTranslation = snap->documentTranslation;
                        if (render::nearbyMoveOrientationAllowed(
                                render::SurfaceMoveSnapElement::Edge,
                                geometryTransformRotateToNearbyEnabled_,
                                constraintAxis))
                        {
                            transformDrag_.snapRotateAxis = snap->rotationAxis;
                            transformDrag_.snapRotatePivot = snap->rotationPivot;
                            transformDrag_.documentSnapRotatePivot = snap->documentRotationPivot;
                            transformDrag_.snapRotateAngleDeg =
                                snap->rotationAngleDegrees;
                        }
                    }
                }
                else if (transformDrag_.target == TransformTargetKind::Surface
                         && surfaceSnapSolver
                             == render::SurfaceMoveSnapSolver::None)
                {
                    // Vertex and Edge modes are entity-specific. Never fall
                    // back to the whole-surface solver if an active element
                    // could not be resolved: that looked like a vertex
                    // snapping to an arbitrary nearby edge/plane.
                }
                else if (const std::optional<GeometryMoveSnapResult> snap =
                             computeGeometryMoveSnap(
                                 transformDrag_.geometryMoveSnapCache,
                                 transformDrag_.target
                                     == TransformTargetKind::Sector,
                                 transformDrag_.target
                                     == TransformTargetKind::Sector
                                     || render::nearbyMoveOrientationAllowed(
                                         render::SurfaceMoveSnapElement::Face,
                                         geometryTransformRotateToNearbyEnabled_,
                                         constraintAxis),
                                 authoringPivot,
                                 transformDrag_.documentTranslation.value_or(render::authoringVector(transformDrag_.gridTranslation)),
                                 transformDrag_.target
                                     == TransformTargetKind::Sector
                                     ? sectorSnapRadius_
                                     : faceSnapRadius_,
                                 snapRotationMaxDegrees_))
                {
                    const std::optional<QVector3D> constrainedTranslation =
                        transformDrag_.target == TransformTargetKind::Surface
                        ? render::constrainMoveSnapTranslation(
                            transformDrag_.gridTranslation,
                            render::displayVector(snap->translation),
                            captureDistance,
                            constraintAxis)
                        : std::optional<QVector3D>{render::displayVector(snap->translation)};
                    if (constrainedTranslation)
                    {
                        adjustedTranslation = *constrainedTranslation;
                        if (constraintAxis.lengthSquared() == 0.0f)
                        {
                            transformDrag_.documentTranslation = snap->translation;
                        }
                        else
                        {
                            TVector axis = render::authoringVector(constraintAxis);
                            const double axisLength = std::hypot(axis.x, axis.y, axis.z);
                            if (axisLength > 0.0) axis = TVector{axis.x / axisLength, axis.y / axisLength, axis.z / axisLength};
                            const TVector raw = transformDrag_.documentTranslation.value_or(render::authoringVector(transformDrag_.gridTranslation));
                            transformDrag_.documentTranslation = raw + axis * (snap->translation - raw).Dot(axis);
                        }
                        adjustedTranslation = render::displayVector(*transformDrag_.documentTranslation);
                        transformDrag_.snapRotateAxis = snap->rotationAxis;
                        transformDrag_.snapRotatePivot = render::displayVector(snap->rotationPivot);
                        transformDrag_.documentSnapRotatePivot = snap->rotationPivot;
                        transformDrag_.snapRotateAngleDeg = snap->rotationAngleDeg;
                    }
                }
            }

            // Step 3: Apply the same translate-then-rotate preview convention
            // used by thing snapping, so sector align rotations show live.
            transformDrag_.translation = adjustedTranslation;
            transformPreviewMatrix_.setToIdentity();
            if (transformDrag_.snapRotateAxis.lengthSquared() > 1.0e-10f
                && std::abs(transformDrag_.snapRotateAngleDeg) > 0.0f)
            {
                transformPreviewMatrix_.translate(transformDrag_.translation);
                transformPreviewMatrix_.translate(transformDrag_.snapRotatePivot);
                transformPreviewMatrix_.rotate(transformDrag_.snapRotateAngleDeg, transformDrag_.snapRotateAxis);
                transformPreviewMatrix_.translate(-transformDrag_.snapRotatePivot);
            }
            else
            {
                transformPreviewMatrix_.translate(transformDrag_.translation);
            }
        }
    }

    // While preview-transforming things, update per-thing sector overrides and a lightweight lighting multiplier.
    //
    // Rebuilding/uploading the full mesh when crossing sector boundaries causes visible hitches. Instead,
    // we keep the existing vertex colors and apply an approximate sector-light ratio in the shader for
    // previewed objects. The committed edit still rebuilds normally when the drag ends.
    for (int i = 0; i < transformPreviewIdCount_; ++i)
    {
        if (!render::transformPreviewPickIdIsFiltered(
                transformPreviewIds_[static_cast<std::size_t>(i)]))
        {
            transformPreviewPayloads_[static_cast<std::size_t>(i)] =
                QVector3D(1.0f, 1.0f, 1.0f);
        }
    }

    if (level_
        && (transformDrag_.target == TransformTargetKind::Thing
            || (transformDrag_.target == TransformTargetKind::ThingLight
                && transformDrag_.lightKind == render::LightGizmoKind::ThingRegular)))
    {
        auto updateOverrideForThing = [&](const std::int32_t thingIndex, const int previewSlot, const int pid)
        {
            if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= level_->things.size())
            {
                return;
            }

            const Thing& thing = level_->things[static_cast<std::size_t>(thingIndex)];
            if (!isLayerVisible(thing.layer) || !shouldShowThing(thing))
            {
                return;
            }
            if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
            {
                return;
            }

            const QVector3D pos(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));
            const QVector3D mapped = transformPreviewMatrix_.map(pos);
            const std::int32_t newSector = findContainingSector(
                mapped,
                thing.sector);

            const std::optional<std::int32_t> prevOverride = [&]() -> std::optional<std::int32_t>
            {
                if (const auto it = previewThingSectors_.find(thingIndex); it != previewThingSectors_.end())
                {
                    return it->second;
                }
                return std::nullopt;
            }();

            if (newSector == thing.sector)
            {
                if (const auto it = previewThingSectors_.find(thingIndex); it != previewThingSectors_.end())
                {
                    previewThingSectors_.erase(it);
                }
            }
            else
            {
                previewThingSectors_[thingIndex] = newSector;
            }

            if (previewSlot >= 0 && previewSlot < transformPreviewIdCount_)
            {
                bool skipMul = false;

                // Non-visual representation cubes are rendered fully lit; don't apply sector-light mul during preview.
                if (!skipMul)
                {
                    if (const auto it = thingTriBuildRangesByPickId_.find(pid); it != thingTriBuildRangesByPickId_.end())
                    {
                        for (const TriBuildRange& r : it->second)
                        {
                            if (r.buildIndex >= thingMeshCache_.triBuilds.size())
                            {
                                continue;
                            }

                            const TriBuild& b = thingMeshCache_.triBuilds[static_cast<std::size_t>(r.buildIndex)];
                            if (std::string_view(b.material).rfind(kThingIconMatPrefix, 0) == 0)
                            {
                                skipMul = true;
                                break;
                            }
                        }
                    }
                }

                // Sprite colors already include CPU lighting.  Use the stable
                // owner map instead of walking every sprite for every mouse
                // sample in a transform preview.
                if (!skipMul && spriteInstanceIndicesByPickId_.contains(pid))
                {
                    skipMul = true;
                }
                else if (!skipMul && spriteTrianglesDirty_)
                {
                    // During cache invalidation the published owner map may
                    // not exist yet. Preserve lighting correctness on this
                    // uncommon rebuild boundary; steady pointer motion uses
                    // the constant-time map above.
                    skipMul = std::any_of(
                        spriteInstances_.begin(), spriteInstances_.end(),
                        [pid](const SpriteInstance& instance)
                        {
                            return render::fast_update::pickIdKey(instance.pickId) == pid;
                        });
                }

                bool usedCpuRelight = false;
                const bool sectorDifferent = (newSector != thing.sector);
                if (!skipMul)
                {
                    if (!sectorDifferent)
                    {
                        // Back in the original sector: restore any preview relighting we may have applied.
                        restorePreviewThingLightingOverride(thingIndex);
                    }
                    else if (!prevOverride || *prevOverride != newSector)
                    {
                        // Sector override changed: relight on CPU using cached shade inputs for accurate preview.
                        usedCpuRelight = applyPreviewThingLightingOverride(thingIndex, newSector);
                    }
                    else if (previewRelitThings_.contains(thingIndex))
                    {
                        usedCpuRelight = true;
                    }
                }
                else
                {
                    // If this thing was previously CPU-relit but is now handled by another path (e.g. sprite),
                    // restore it so we don't leave stale colors in the VBO.
                    restorePreviewThingLightingOverride(thingIndex);
                }

                if (!skipMul && sectorDifferent && !usedCpuRelight)
                {
                    // Fallback: approximate lighting change via a multiplicative ratio in the shader.
                    transformPreviewPayloads_[static_cast<std::size_t>(previewSlot)] =
                        calcThingSectorLightingMul(level_, thing.sector, newSector);
                }
            }
        };

        for (int i = 0; i < transformPreviewIdCount_; ++i)
        {
            const int pid = transformPreviewIds_[static_cast<std::size_t>(i)];
            if (pid <= 0 || static_cast<std::size_t>(pid) >= pickObjects_.size())
            {
                continue;
            }

            const PickObject& obj = pickObjects_[static_cast<std::size_t>(pid)];
            if (obj.kind != PickKind::Thing)
            {
                continue;
            }

            updateOverrideForThing(obj.index, i, pid);
        }
    }

    if (transformDrag_.documentTranslation)
    {
        const TVector& precise = *transformDrag_.documentTranslation;
        const QVector3D displayed(static_cast<float>(precise.x), static_cast<float>(precise.y), static_cast<float>(precise.z));
        // A subsequent stick/axis/orientation solver owns any changed result.
        // Keep the double companion only while it describes the final preview.
        if (displayed != transformDrag_.translation)
        {
            transformDrag_.documentTranslation.reset();
        }
    }
    transformPreviewActive_ = (transformPreviewIdCount_ > 0);
    refreshTransformSurfaceUvPreview();
    Q_EMIT transformPreviewChanged(
        true,
        transformTool_,
        transformDrag_.translation,
        transformDrag_.axis,
        static_cast<double>(transformDrag_.angleDeg),
        transformDrag_.pivot,
        static_cast<double>(transformDrag_.scaleFactor));
    update();
}

void GlView::endTransformDrag()
{
    if (!transformDrag_.active || transformDrag_.handle == TransformHandle::None)
    {
        return;
    }

    restoreTransformSurfaceUvPreview();
    Q_EMIT transformPreviewCommitStarting();

    const TransformTargetKind targetKind = transformDrag_.target;
    const render::LightGizmoKind lightKind = transformDrag_.lightKind;
    const TransformHandle handle = transformDrag_.handle;
    const std::int32_t frameThing = transformDrag_.frameThing;
    const std::int32_t frameValue = transformDrag_.frameValue;
    const std::int32_t surfaceSector = transformDrag_.surfaceSector;
    const std::int32_t surfaceIndex = transformDrag_.surfaceIndex;
    const std::int32_t surfaceVertex = transformDrag_.surfaceVertex;
    const std::int32_t surfaceEdge = transformDrag_.surfaceEdge;
    const SurfaceSubMode surfaceMode = transformDrag_.surfaceMode;
    const QVector3D axis = transformDrag_.axis;
    const QVector3D pivot = transformDrag_.pivot;
    const TVector documentPivot = transformDrag_.documentPivot.value_or(TVector{pivot.x(), pivot.y(), pivot.z()});
    const QVector3D translation = transformDrag_.translation;
    const TVector documentTranslation = transformDrag_.documentTranslation.value_or(
        TVector{translation.x(), translation.y(), translation.z()});
    const QVector3D snapRotateAxis = transformDrag_.snapRotateAxis;
    const TVector snapRotatePivot = transformDrag_.documentSnapRotatePivot.value_or(
        render::authoringVector(transformDrag_.snapRotatePivot));
    const float angleDeg = transformDrag_.angleDeg;
    const float snapRotateAngleDeg = transformDrag_.snapRotateAngleDeg;
    const float scaleFactor = transformDrag_.scaleFactor;

    auto clearPreviewState = [&]()
    {
        restoreAllPreviewThingLightingOverrides();
        transformDrag_ = {};
        transformPreviewActive_ = false;
        transformPreviewVertexFilter_ = false;
        transformPreviewIdCount_ = 0;
        transformPreviewMatrix_.setToIdentity();
        previewThingSectors_.clear();
    };

    if ( transformSurfaceUvFailureReported_ )
    {
        clearPreviewState();
        Q_EMIT transformPreviewChanged(false, transformTool_, QVector3D{}, QVector3D{}, 0.0, QVector3D{}, 1.0);
        update();
        return;
    }

    const bool isMoveHandle = (handle == TransformHandle::MoveX || handle == TransformHandle::MoveY ||
                               handle == TransformHandle::MoveZ || handle == TransformHandle::MoveFree);
    const bool isScaleHandle = (handle == TransformHandle::ScaleX || handle == TransformHandle::ScaleY ||
                                handle == TransformHandle::ScaleZ || handle == TransformHandle::ScaleUniform);

    if ( isMoveHandle )
    {
        const QVector3D commitBaseTranslation = translation;

        if (std::isfinite(documentTranslation.x) && std::isfinite(documentTranslation.y)
            && std::isfinite(documentTranslation.z)
            && (documentTranslation.x != 0.0 || documentTranslation.y != 0.0 || documentTranslation.z != 0.0
                || (std::isfinite(snapRotateAngleDeg) && snapRotateAngleDeg != 0.0f
                    && snapRotateAxis.lengthSquared() > 1.0e-10f)))
        {
            const TVector committedTranslation = documentTranslation;
            const QVector3D committedSnapAxis = snapRotateAxis;
            const TVector committedSnapPivot = snapRotatePivot;
            const float committedSnapAngleDeg = snapRotateAngleDeg;

            switch (targetKind)
            {
                case TransformTargetKind::Thing:
                    if (snapRotateAxis.lengthSquared() > 1.0e-10f && std::abs(snapRotateAngleDeg) > 0.0f)
                    {
                        Q_EMIT thingsSnapTransformRequested(
                            committedTranslation,
                            snapRotateAxis,
                            static_cast<double>(snapRotateAngleDeg),
                            snapRotatePivot);
                    }
                    else
                    {
                        Q_EMIT thingsTranslateRequested(
                            documentTranslation.x,
                            documentTranslation.y,
                            documentTranslation.z);
                    }
                    break;
                case TransformTargetKind::Frame:
                    if (committedSnapAxis.lengthSquared() > 1.0e-10f
                        && std::abs(committedSnapAngleDeg) > 0.0f)
                    {
                        Q_EMIT frameSnapTransformRequested(
                            frameThing,
                            frameValue,
                            committedTranslation,
                            committedSnapAxis,
                            static_cast<double>(committedSnapAngleDeg),
                            committedSnapPivot);
                    }
                    else
                    {
                        Q_EMIT frameTranslateRequested(
                            frameThing,
                            frameValue,
                            documentTranslation.x,
                            documentTranslation.y,
                            documentTranslation.z);
                    }
                    break;
                case TransformTargetKind::Sector:
                    if (std::abs(committedSnapAngleDeg) > 0.0f && committedSnapAxis.lengthSquared() > 1.0e-10f)
                    {
                        Q_EMIT sectorsSnapTransformRequested(
                            committedTranslation,
                            committedSnapAxis,
                            static_cast<double>(-committedSnapAngleDeg),
                            committedSnapPivot);
                    }
                    else
                    {
                        Q_EMIT sectorsTranslateRequested(
                            documentTranslation.x,
                            documentTranslation.y,
                            documentTranslation.z);
                    }
                    break;
                case TransformTargetKind::Surface:
                {
                    const bool edgeMode = (surfaceMode == SurfaceSubMode::Edge);
                    const bool vertexMode = (surfaceMode == SurfaceSubMode::Vertex);

                    if (vertexMode && surfaceSector >= 0 && surfaceVertex >= 0)
                    {
                        Q_EMIT surfaceVertexTranslateRequested(
                            surfaceSector,
                            surfaceVertex,
                            documentTranslation.x,
                            documentTranslation.y,
                            documentTranslation.z);
                        break;
                    }
                    if (edgeMode && surfaceSector >= 0 && surfaceIndex >= 0 && surfaceEdge >= 0)
                    {
                        if (std::abs(committedSnapAngleDeg) > 0.0f
                            && committedSnapAxis.lengthSquared() > 1.0e-10f)
                        {
                            Q_EMIT surfaceEdgeSnapTransformRequested(
                                surfaceSector,
                                surfaceIndex,
                                surfaceEdge,
                                committedTranslation,
                                committedSnapAxis,
                                static_cast<double>(-committedSnapAngleDeg),
                                committedSnapPivot);
                        }
                        else
                        {
                            Q_EMIT surfaceEdgeTranslateRequested(
                                surfaceSector,
                                surfaceIndex,
                                surfaceEdge,
                                documentTranslation.x,
                                documentTranslation.y,
                                documentTranslation.z);
                        }
                        break;
                    }

                    if (std::abs(committedSnapAngleDeg) > 0.0f && committedSnapAxis.lengthSquared() > 1.0e-10f)
                    {
                        Q_EMIT surfacesSnapTransformRequested(
                            committedTranslation,
                            committedSnapAxis,
                            static_cast<double>(-committedSnapAngleDeg),
                            committedSnapPivot);
                    }
                    else
                    {
                        Q_EMIT surfacesTranslateRequested(
                            documentTranslation.x,
                            documentTranslation.y,
                            documentTranslation.z);
                    }
                    break;
                }
                case TransformTargetKind::Light:
                    Q_EMIT lightsTranslateRequested(
                        documentTranslation.x,
                        documentTranslation.y,
                        documentTranslation.z);
                    break;
                case TransformTargetKind::ThingLight:
                    Q_EMIT thingLightTranslateRequested(
                        static_cast<std::int32_t>(lightKind),
                        documentTranslation.x,
                        documentTranslation.y,
                        documentTranslation.z);
                    break;
                case TransformTargetKind::SectorLight:
                    Q_EMIT sectorLightTranslateRequested(
                        documentTranslation.x,
                        documentTranslation.y,
                        documentTranslation.z);
                    break;
                default:
                    break;
            }
        }
    }
    else if (isScaleHandle)
    {
        if (std::isfinite(scaleFactor) && scaleFactor != 1.0f)
        {
            switch (targetKind)
            {
                case TransformTargetKind::Sector:
                    Q_EMIT sectorsScaleRequested(axis, static_cast<double>(scaleFactor), documentPivot);
                    break;
                case TransformTargetKind::Surface:
                    Q_EMIT surfacesScaleRequested(axis, static_cast<double>(scaleFactor), documentPivot);
                    break;
                case TransformTargetKind::Light:
                    Q_EMIT lightsRangeScaleRequested(static_cast<double>(scaleFactor));
                    break;
                case TransformTargetKind::ThingLight:
                    Q_EMIT thingLightRangeScaleRequested(
                        static_cast<std::int32_t>(lightKind),
                        static_cast<double>(scaleFactor));
                    break;
                case TransformTargetKind::SectorLight:
                    Q_EMIT sectorLightRangeScaleRequested(static_cast<double>(scaleFactor));
                    break;
                default:
                    break;
            }
        }
    }
    else
    {
        if (std::isfinite(angleDeg) && angleDeg != 0.0f)
        {
            const float committedAngleDeg = angleDeg;

            switch (targetKind)
            {
                case TransformTargetKind::Thing:
                    Q_EMIT thingsRotateRequested(axis, static_cast<double>(committedAngleDeg), documentPivot);
                    break;
                case TransformTargetKind::Frame:
                    Q_EMIT frameRotateRequested(frameThing, frameValue, axis, static_cast<double>(committedAngleDeg), documentPivot);
                    break;
                case TransformTargetKind::Sector:
                    Q_EMIT sectorsRotateRequested(axis, static_cast<double>(committedAngleDeg), documentPivot);
                    break;
                case TransformTargetKind::Surface:
                    Q_EMIT surfacesRotateRequested(axis, static_cast<double>(committedAngleDeg), documentPivot);
                    break;
                default:
                    break;
            }
        }
    }

    // The commit signals are direct MainWindow transactions. Keep the exact
    // final shader pose installed until those handlers have updated Level and
    // patched the persistent render caches; clearing it first exposed one old
    // or directionally offset frame while containment/check/property work ran.
    clearPreviewState();
    Q_EMIT transformPreviewChanged(false, transformTool_, QVector3D(), QVector3D(), 0.0, QVector3D(), 1.0);

    update();
}

} // namespace sed
