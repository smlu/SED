#include "gl_view_common.hpp"
#include "render/picking/depth_readback_utils.hpp"
#include "render/picking/viewport_pick_cycle_utils.hpp"

#include <QPointer>
#include <QSurface>

namespace sed
{


std::uint32_t GlView::registerSurfacePickId(const std::int32_t sectorIndex, const std::int32_t surfaceIndex)
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
        return it->second;
    }

    const std::uint32_t id = static_cast<std::uint32_t>(pickObjects_.size());
    pickObjects_.push_back(PickObject{PickKind::Surface, sectorIndex, surfaceIndex, -1});
    surfacePickIds_.emplace(key, id);
    return id;
}

std::uint32_t GlView::registerThingPickId(const std::int32_t thingIndex)
{
    if (thingIndex < 0)
    {
        return 0;
    }

    const std::size_t idx = static_cast<std::size_t>(thingIndex);
    if (idx >= thingPickIds_.size())
    {
        thingPickIds_.resize(idx + 1, 0);
    }

    std::uint32_t& id = thingPickIds_[idx];
    if (id != 0)
    {
        return id;
    }

    id = static_cast<std::uint32_t>(pickObjects_.size());
    pickObjects_.push_back(PickObject{PickKind::Thing, -1, -1, thingIndex});
    return id;
}

void GlView::ensurePickFbo(const int pixelWidth, const int pixelHeight)
{
    if (pixelWidth <= 0 || pixelHeight <= 0)
    {
        pickFbo_.reset();
        pickFboWidth_ = 0;
        pickFboHeight_ = 0;
        return;
    }

    if (pickFbo_ && pickFboWidth_ == pixelWidth && pickFboHeight_ == pixelHeight)
    {
        return;
    }

    pickFbo_.reset();

    QOpenGLFramebufferObjectFormat format{};
    format.setAttachment(QOpenGLFramebufferObject::Depth);
    format.setSamples(0);
    format.setInternalTextureFormat(GL_RGBA8);

    pickFbo_ = std::make_unique<QOpenGLFramebufferObject>(QSize(pixelWidth, pixelHeight), format);
    if (!pickFbo_ || !pickFbo_->isValid())
    {
        pickFbo_.reset();
        pickFboWidth_ = 0;
        pickFboHeight_ = 0;
        return;
    }

    pickFboWidth_ = pixelWidth;
    pickFboHeight_ = pixelHeight;
}

std::vector<float> GlView::pickSceneDepthsAtPoints(
    const QMatrix4x4& mvp,
    const std::vector<QPointF>& widgetPoints,
    const bool includeSolidLevel,
    const bool includeShadedThings,
    const bool cullSingleSidedLevelBackFaces)
{
    struct ContextGuard
    {
        GlView& view;
        QPointer<QOpenGLContext> previousContext{};
        QSurface* previousSurface{};
        bool acquired{};
        explicit ContextGuard(GlView& v)
            : view(v),
              previousContext(QOpenGLContext::currentContext()),
              previousSurface(previousContext
                    ? previousContext->surface()
                    : nullptr)
        {
            if (previousContext.data() != view.context())
            {
                view.makeCurrent();
                acquired = true;
            }
        }

        ~ContextGuard()
        {
            release();
        }

        void release() noexcept
        {
            if (!acquired)
            {
                return;
            }
            acquired = false;
            view.doneCurrent();
            if (previousContext && previousSurface)
            {
                previousContext->makeCurrent(previousSurface);
            }
        }

        [[nodiscard]] bool ready() const noexcept
        {
            return view.context() != nullptr
                && QOpenGLContext::currentContext() == view.context();
        }
    };

    std::vector<float> depths(widgetPoints.size(), 1.0f);
    if (widgetPoints.empty()
        || (!includeSolidLevel && !includeShadedThings)
        || width() <= 0
        || height() <= 0)
    {
        return depths;
    }

    const bool gpuDepthPrerequisites = glReady_
        && pickProgram_
        && triVao_
        && triVertexCount_ > 0;

    // Headless/software startup can reach the CPU marquee and supplemental
    // wire paths before the pick framebuffer exists. Preserve the same solid
    // wall boundary in that branch instead of treating the default depth of
    // one as an unobstructed scene. A live GPU pass below remains authoritative
    // (and additionally includes shaded Things when requested).
    if (!gpuDepthPrerequisites && includeSolidLevel && level_)
    {
        bool invertible = false;
        const QMatrix4x4 inverseMvp = mvp.inverted(&invertible);
        if (invertible)
        {
            for (std::size_t pointIndex = 0;
                 pointIndex < widgetPoints.size(); ++pointIndex)
            {
                const QPointF& point = widgetPoints[pointIndex];
                if (!std::isfinite(point.x()) || !std::isfinite(point.y())
                    || point.x() < 0.0 || point.y() < 0.0
                    || point.x() >= static_cast<double>(width())
                    || point.y() >= static_cast<double>(height()))
                {
                    continue;
                }

                const float ndcX = 2.0f * static_cast<float>(
                    point.x() / static_cast<double>(width())) - 1.0f;
                const float ndcY = 1.0f - 2.0f * static_cast<float>(
                    point.y() / static_cast<double>(height()));
                const QVector3D nearWorld = unprojectNdc(
                    inverseMvp, ndcX, ndcY, -1.0f);
                const QVector3D farWorld = unprojectNdc(
                    inverseMvp, ndcX, ndcY, 1.0f);
                QVector3D rayDirection = farWorld - nearWorld;
                const float rayLength = rayDirection.length();
                if (!std::isfinite(rayLength)
                    || rayLength <= std::numeric_limits<float>::epsilon())
                {
                    continue;
                }
                rayDirection /= rayLength;

                const TVector rayOrigin{
                    static_cast<double>(nearWorld.x()),
                    static_cast<double>(nearWorld.y()),
                    static_cast<double>(nearWorld.z()) };
                const TVector direction{
                    static_cast<double>(rayDirection.x()),
                    static_cast<double>(rayDirection.y()),
                    static_cast<double>(rayDirection.z()) };
                const auto hits = surfacePickIndex_.pickSurfaces(
                    *level_, rayOrigin, direction,
                    static_cast<double>(rayLength));
                for (const sed::level::SurfacePickHit& hit : hits)
                {
                    if (hit.surface.sector < 0 || hit.surface.surface < 0
                        || static_cast<std::size_t>(hit.surface.sector)
                            >= level_->sectors.size())
                    {
                        continue;
                    }
                    const Sector& sector = level_->sectors[
                        static_cast<std::size_t>(hit.surface.sector)];
                    if (!isLayerVisible(sector.layer)
                        || static_cast<std::size_t>(hit.surface.surface)
                            >= sector.surfaces.size())
                    {
                        continue;
                    }
                    const Surface& surface = sector.surfaces[
                        static_cast<std::size_t>(hit.surface.surface)];
                    if (surface.geo < static_cast<std::int32_t>(TGeoMode::Solid))
                    {
                        continue;
                    }
                    if (cullSingleSidedLevelBackFaces
                        && (surface.faceFlags & FF_DoubleSided) == 0u
                        && surface.normal.Dot(direction) > 1.0e-9)
                    {
                        continue;
                    }

                    const QVector4D clip = mvp * QVector4D(
                        static_cast<float>(hit.position.x),
                        static_cast<float>(hit.position.y),
                        static_cast<float>(hit.position.z),
                        1.0f);
                    if (!std::isfinite(clip.z()) || !std::isfinite(clip.w())
                        || std::abs(clip.w())
                            <= std::numeric_limits<float>::epsilon())
                    {
                        continue;
                    }
                    const float ndcDepth = clip.z() / clip.w();
                    if (std::isfinite(ndcDepth)
                        && ndcDepth >= -1.0f && ndcDepth <= 1.0f)
                    {
                        depths[pointIndex] = ndcDepth * 0.5f + 0.5f;
                    }
                    break;
                }
            }
        }
    }

    if (!gpuDepthPrerequisites)
    {
        return depths;
    }

    ContextGuard guard(*this);
    if (!guard.ready())
    {
        return depths;
    }

    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));
    ensurePickFbo(pixelW, pixelH);
    if (!pickFbo_)
    {
        return depths;
    }

    std::vector<QPoint> samples(widgetPoints.size(), QPoint(-1, -1));
    bool haveSample = false;
    for (std::size_t i = 0; i < widgetPoints.size(); ++i)
    {
        const std::optional<QPoint> framebufferPixel =
            render::picking::framebufferPixelForWidgetPoint(
                widgetPoints[i],
                width(),
                height(),
                static_cast<double>(dpr),
                pixelW,
                pixelH);
        if (!framebufferPixel)
        {
            continue;
        }

        samples[i] = *framebufferPixel;
        haveSample = true;
    }
    if (!haveSample)
    {
        return depths;
    }

    auto* f = QOpenGLContext::currentContext()->functions();

    GLint previousDrawFramebuffer = 0;
    GLint previousReadFramebuffer = 0;
    f->glGetIntegerv(
        GL_DRAW_FRAMEBUFFER_BINDING,
        &previousDrawFramebuffer);
    f->glGetIntegerv(
        GL_READ_FRAMEBUFFER_BINDING,
        &previousReadFramebuffer);

    const GLboolean previousBlend = f->glIsEnabled(GL_BLEND);
    const GLboolean previousDither = f->glIsEnabled(GL_DITHER);
    const GLboolean previousStencil = f->glIsEnabled(GL_STENCIL_TEST);
    const GLboolean previousDepthTest = f->glIsEnabled(GL_DEPTH_TEST);
    const GLboolean previousCull = f->glIsEnabled(GL_CULL_FACE);
    GLboolean previousDepthMask = GL_TRUE;
    GLboolean previousColorMask[4]{ GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
    GLint previousDepthFunc = GL_LESS;
    GLint previousCullMode = GL_BACK;
    GLint previousFrontFace = GL_CCW;
    GLint previousViewport[4]{ 0, 0, pixelW, pixelH };
    GLfloat previousClearColor[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
    f->glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    f->glGetBooleanv(GL_COLOR_WRITEMASK, previousColorMask);
    f->glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
    f->glGetIntegerv(GL_CULL_FACE_MODE, &previousCullMode);
    f->glGetIntegerv(GL_FRONT_FACE, &previousFrontFace);
    f->glGetIntegerv(GL_VIEWPORT, previousViewport);
    f->glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClearColor);

    pickFbo_->bind();
    f->glViewport(0, 0, pickFboWidth_, pickFboHeight_);
    f->glDisable(GL_BLEND);
    f->glDisable(GL_DITHER);
    f->glDisable(GL_STENCIL_TEST);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthMask(GL_TRUE);
    f->glDepthFunc(GL_LESS);
    f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    f->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    f->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    pickProgram_->bind();
    pickProgram_->setUniformValue("u_mvp", mvp);
    pickProgram_->setUniformValue("u_geo_min", static_cast<int>(TGeoMode::Solid));
    pickProgram_->setUniformValue("u_exclude_count", 0);
    pickProgram_->setUniformValue("u_frame_xform_enabled", 0);
    pickProgram_->setUniformValue("u_frame_xform_id", 0);
    QMatrix4x4 identity;
    identity.setToIdentity();
    pickProgram_->setUniformValue("u_frame_xform_matrix", identity);

    triVao_->bind();
    bool cullEnabled = false;
    f->glDisable(GL_CULL_FACE);
    for (const TriBatch& batch : triBatches_)
    {
        const bool drawBatch = batch.vertexCount > 0
            && ((batch.kind == TriKind::Level && includeSolidLevel)
                || (batch.kind == TriKind::Thing && includeShadedThings));
        if (!drawBatch)
        {
            continue;
        }

        // Sector/object-volume callers use a two-sided Level boundary.
        // Ordinary Surface-element callers request front-face culling so an
        // invisible single-sided backface cannot occlude eligible geometry.
        // Thing geometry continues to match shaded rendering.
        const bool wantCull = render::picking::sceneDepthBatchCullBackFaces(
            batch.kind == TriKind::Level,
            batch.isSprite,
            batch.doubleSided,
            cullSingleSidedLevelBackFaces);
        if (wantCull != cullEnabled)
        {
            if (wantCull)
            {
                f->glEnable(GL_CULL_FACE);
                f->glCullFace(GL_BACK);
                f->glFrontFace(GL_CCW);
            }
            else
            {
                f->glDisable(GL_CULL_FACE);
            }
            cullEnabled = wantCull;
        }
        f->glDrawArrays(GL_TRIANGLES, batch.firstVertex, batch.vertexCount);
    }
    triVao_->release();
    pickProgram_->release();

    // Render the depth scene once. Keep sparse queries as individual pixels,
    // but replace hundreds or thousands of synchronous reads with one bounded
    // rectangle read when the candidate set densely occupies a manageable
    // framebuffer region. Pixel coordinates retain the DPR scaling and GL Y
    // flip established above, so each returned value is identical to a 1x1 read.
    const std::optional<render::picking::DepthReadbackRegion> readbackRegion =
        render::picking::denseDepthReadbackRegion(
            samples,
            pixelW,
            pixelH);
    if (readbackRegion)
    {
        std::vector<float> sampledDepths(
            static_cast<std::size_t>(readbackRegion->pixelCount),
            1.0f);
        f->glReadPixels(
            readbackRegion->left,
            readbackRegion->bottom,
            readbackRegion->width,
            readbackRegion->height,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            sampledDepths.data());
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            const QPoint sample = samples[i];
            if (sample.x() < 0 || sample.y() < 0)
            {
                continue;
            }
            const std::optional<std::size_t> sampleIndex =
                render::picking::depthReadbackSampleIndex(
                    *readbackRegion,
                    sample);
            if (!sampleIndex || *sampleIndex >= sampledDepths.size())
            {
                continue;
            }
            const float sampledDepth = sampledDepths[*sampleIndex];
            if (std::isfinite(sampledDepth))
            {
                depths[i] = sampledDepth;
            }
        }
    }
    else
    {
        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            const QPoint sample = samples[i];
            if (sample.x() < 0 || sample.y() < 0)
            {
                continue;
            }
            float sampledDepth = 1.0f;
            f->glReadPixels(
                sample.x(),
                sample.y(),
                1,
                1,
                GL_DEPTH_COMPONENT,
                GL_FLOAT,
                &sampledDepth);
            if (std::isfinite(sampledDepth))
            {
                depths[i] = sampledDepth;
            }
        }
    }

    pickFbo_->release();
    // Restore the framebuffer that the caller had bound. ARCH owner and
    // vertex picks can run while another render/pick pass owns a custom FBO;
    // rebinding the widget's default target here would redirect that pass's
    // subsequent draws and made the helper appear to consume the foreign
    // framebuffer state.
    f->glBindFramebuffer(
        GL_DRAW_FRAMEBUFFER,
        static_cast<GLuint>(previousDrawFramebuffer));
    f->glBindFramebuffer(
        GL_READ_FRAMEBUFFER,
        static_cast<GLuint>(previousReadFramebuffer));
    f->glViewport(
        previousViewport[0],
        previousViewport[1],
        previousViewport[2],
        previousViewport[3]);
    f->glClearColor(
        previousClearColor[0],
        previousClearColor[1],
        previousClearColor[2],
        previousClearColor[3]);
    f->glDepthMask(previousDepthMask);
    f->glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    f->glColorMask(
        previousColorMask[0],
        previousColorMask[1],
        previousColorMask[2],
        previousColorMask[3]);
    f->glCullFace(static_cast<GLenum>(previousCullMode));
    f->glFrontFace(static_cast<GLenum>(previousFrontFace));
    if (previousBlend) { f->glEnable(GL_BLEND); } else { f->glDisable(GL_BLEND); }
    if (previousDither) { f->glEnable(GL_DITHER); } else { f->glDisable(GL_DITHER); }
    if (previousStencil) { f->glEnable(GL_STENCIL_TEST); } else { f->glDisable(GL_STENCIL_TEST); }
    if (previousDepthTest) { f->glEnable(GL_DEPTH_TEST); } else { f->glDisable(GL_DEPTH_TEST); }
    if (previousCull) { f->glEnable(GL_CULL_FACE); } else { f->glDisable(GL_CULL_FACE); }
    return depths;
}

std::vector<SurfaceRef> GlView::pickSurfacesInScreenRect(
    const QMatrix4x4& mvp,
    int left,
    int top,
    int right,
    int bottom,
    const int geoMin,
    const bool cullBackFaces)
{
    struct ContextGuard
    {
        GlView& view;
        bool acquired{};
        explicit ContextGuard(GlView& v)
            : view(v)
        {
            if (QOpenGLContext::currentContext() != view.context())
            {
                view.makeCurrent();
                acquired = true;
            }
        }
        ~ContextGuard()
        {
            if (acquired)
            {
                view.doneCurrent();
            }
        }
    };

    std::vector<SurfaceRef> picked{};

    if (!level_ || width() <= 0 || height() <= 0 || left > right || top > bottom)
    {
        return picked;
    }

    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));

    if (!glReady_ || !pickProgram_ || !triVao_ || triVertexCount_ <= 0)
    {
        return picked;
    }

    ContextGuard guard(*this);

    ensurePickFbo(pixelW, pixelH);
    if (!pickFbo_)
    {
        return picked;
    }

    const int wLeft = std::clamp(left, 0, width() - 1);
    const int wTop = std::clamp(top, 0, height() - 1);
    const int wRight = std::clamp(right, 0, width() - 1);
    const int wBottom = std::clamp(bottom, 0, height() - 1);
    if (wLeft > wRight || wTop > wBottom)
    {
        return picked;
    }

    int pxMin = static_cast<int>(std::lround(static_cast<double>(wLeft) * dpr));
    int pxMax = static_cast<int>(std::lround(static_cast<double>(wRight) * dpr));
    int pyMin = static_cast<int>(std::lround(static_cast<double>(wTop) * dpr));
    int pyMax = static_cast<int>(std::lround(static_cast<double>(wBottom) * dpr));

    pxMin = std::clamp(pxMin, 0, pixelW - 1);
    pxMax = std::clamp(pxMax, 0, pixelW - 1);
    pyMin = std::clamp(pyMin, 0, pixelH - 1);
    pyMax = std::clamp(pyMax, 0, pixelH - 1);
    if (pxMin > pxMax || pyMin > pyMax)
    {
        return picked;
    }

    const int readX = pxMin;
    const int readY = (pixelH - 1) - pyMax;
    const int readW = pxMax - pxMin + 1;
    const int readH = pyMax - pyMin + 1;
    if (readW <= 0 || readH <= 0)
    {
        return picked;
    }

    auto* f = QOpenGLContext::currentContext()->functions();

    pickFbo_->bind();
    f->glViewport(0, 0, pickFboWidth_, pickFboHeight_);

    f->glDisable(GL_BLEND);
    f->glDisable(GL_DITHER);
    f->glDisable(GL_STENCIL_TEST);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthMask(GL_TRUE);
    f->glDepthFunc(GL_LESS);
    f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    if (cullBackFaces)
    {
        f->glEnable(GL_CULL_FACE);
        f->glCullFace(GL_BACK);
        f->glFrontFace(GL_CCW);
    }
    else
    {
        f->glDisable(GL_CULL_FACE);
    }

    f->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    pickProgram_->bind();
    pickProgram_->setUniformValue("u_mvp", mvp);
    pickProgram_->setUniformValue("u_geo_min", geoMin);
    pickProgram_->setUniformValue("u_exclude_count", 0);
    pickProgram_->setUniformValue("u_frame_xform_enabled", 0);
    pickProgram_->setUniformValue("u_frame_xform_id", 0);
    pickProgram_->setUniformValue("u_frame_xform_matrix", QMatrix4x4());

    triVao_->bind();
    if (cullBackFaces)
    {
        bool cullEnabled = true;
        for (const TriBatch& batch : triBatches_)
        {
            if (batch.kind != TriKind::Level || batch.vertexCount <= 0)
            {
                continue;
            }

            const bool wantCull = !batch.doubleSided;
            if (wantCull != cullEnabled)
            {
                if (wantCull)
                {
                    f->glEnable(GL_CULL_FACE);
                    f->glCullFace(GL_BACK);
                    f->glFrontFace(GL_CCW);
                }
                else
                {
                    f->glDisable(GL_CULL_FACE);
                }
                cullEnabled = wantCull;
            }

            f->glDrawArrays(GL_TRIANGLES, batch.firstVertex, batch.vertexCount);
        }

        if (!cullEnabled)
        {
            f->glEnable(GL_CULL_FACE);
            f->glCullFace(GL_BACK);
            f->glFrontFace(GL_CCW);
        }
    }
    else
    {
        for (const DrawRange& r : levelPickRanges_)
        {
            if (r.count <= 0)
            {
                continue;
            }
            f->glDrawArrays(GL_TRIANGLES, r.first, r.count);
        }
    }
    triVao_->release();

    pickProgram_->release();

    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(readW) * static_cast<std::size_t>(readH) * 4u, 0u);
    f->glReadPixels(readX, readY, readW, readH, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    pickFbo_->release();
    f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    f->glViewport(0, 0, pixelW, pixelH);
    f->glClearColor(
        backgroundClearColor_.redF(),
        backgroundClearColor_.greenF(),
        backgroundClearColor_.blueF(),
        1.0f);

    std::unordered_set<std::uint64_t> seen{};
    seen.reserve(static_cast<std::size_t>(readW) * static_cast<std::size_t>(readH));

    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4)
    {
        const std::uint32_t id = static_cast<std::uint32_t>(rgba[i + 0])
            | (static_cast<std::uint32_t>(rgba[i + 1]) << 8)
            | (static_cast<std::uint32_t>(rgba[i + 2]) << 16)
            | (static_cast<std::uint32_t>(rgba[i + 3]) << 24);
        if (id == 0 || id >= pickObjects_.size())
        {
            continue;
        }

        const PickObject& obj = pickObjects_[id];
        if (obj.kind != PickKind::Surface || obj.sector < 0 || obj.surface < 0)
        {
            continue;
        }

        const std::uint64_t key =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(obj.sector)) << 32)
            | static_cast<std::uint64_t>(static_cast<std::uint32_t>(obj.surface));
        if (!seen.insert(key).second)
        {
            continue;
        }

        picked.push_back(SurfaceRef{ obj.sector, obj.surface });
    }

    std::sort(picked.begin(), picked.end(), [](const SurfaceRef& a, const SurfaceRef& b)
        {
            if (a.sector != b.sector)
            {
                return a.sector < b.sector;
            }
            return a.surface < b.surface;
        });

    return picked;
}

std::unordered_set<std::int32_t> GlView::pickThingsInScreenRect(
    const QMatrix4x4& mvp,
    int left,
    int top,
    int right,
    int bottom,
    const bool occludeBySolidLevel)
{
    struct ContextGuard
    {
        GlView& view;
        bool acquired{};
        explicit ContextGuard(GlView& v)
            : view(v)
        {
            if (QOpenGLContext::currentContext() != view.context())
            {
                view.makeCurrent();
                acquired = true;
            }
        }
        ~ContextGuard()
        {
            if (acquired)
            {
                view.doneCurrent();
            }
        }
    };

    std::unordered_set<std::int32_t> picked{};

    if (!level_ || width() <= 0 || height() <= 0 || left > right || top > bottom)
    {
        return picked;
    }

    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));

    if (!glReady_ || !pickProgram_)
    {
        return picked;
    }

    ContextGuard guard(*this);

    ensurePickFbo(pixelW, pixelH);

    const bool haveTris = triVao_ && triVertexCount_ > 0;
    const bool haveSprites = spriteTriVao_ && spriteTriVbo_ && !spriteInstances_.empty();
    if (!pickFbo_ || (!haveTris && !haveSprites))
    {
        return picked;
    }

    const int wLeft = std::clamp(left, 0, width() - 1);
    const int wTop = std::clamp(top, 0, height() - 1);
    const int wRight = std::clamp(right, 0, width() - 1);
    const int wBottom = std::clamp(bottom, 0, height() - 1);
    if (wLeft > wRight || wTop > wBottom)
    {
        return picked;
    }

    int pxMin = static_cast<int>(std::lround(static_cast<double>(wLeft) * dpr));
    int pxMax = static_cast<int>(std::lround(static_cast<double>(wRight) * dpr));
    int pyMin = static_cast<int>(std::lround(static_cast<double>(wTop) * dpr));
    int pyMax = static_cast<int>(std::lround(static_cast<double>(wBottom) * dpr));

    pxMin = std::clamp(pxMin, 0, pixelW - 1);
    pxMax = std::clamp(pxMax, 0, pixelW - 1);
    pyMin = std::clamp(pyMin, 0, pixelH - 1);
    pyMax = std::clamp(pyMax, 0, pixelH - 1);
    if (pxMin > pxMax || pyMin > pyMax)
    {
        return picked;
    }

    const int readX = pxMin;
    const int readY = (pixelH - 1) - pyMax;
    const int readW = pxMax - pxMin + 1;
    const int readH = pyMax - pyMin + 1;
    if (readW <= 0 || readH <= 0)
    {
        return picked;
    }

    auto* f = QOpenGLContext::currentContext()->functions();

    pickFbo_->bind();
    f->glViewport(0, 0, pickFboWidth_, pickFboHeight_);

    f->glDisable(GL_BLEND);
    f->glDisable(GL_DITHER);
    f->glDisable(GL_STENCIL_TEST);
    f->glDisable(GL_CULL_FACE);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthMask(GL_TRUE);
    f->glDepthFunc(GL_LESS);
    f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    f->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    pickProgram_->bind();
    pickProgram_->setUniformValue("u_mvp", mvp);
    // Match shaded thing rendering: only filled thing geometry should contribute.
    pickProgram_->setUniformValue("u_geo_min", static_cast<int>(TGeoMode::Solid));
    pickProgram_->setUniformValue("u_exclude_count", 0);

    int frameXformEnabled = 0;
    int frameXformPickId = 0;
    QMatrix4x4 frameXformMatrix;
    frameXformMatrix.setToIdentity();

    if (frameEditEnabled_)
    {
        const std::int32_t thingIndex = selectedThing_;
        if (thingIndex >= 0 && static_cast<std::size_t>(thingIndex) < level_->things.size())
        {
            const Thing& thing = level_->things[static_cast<std::size_t>(thingIndex)];
            if (selectedFrameValueIndex_ >= 0 && static_cast<std::size_t>(selectedFrameValueIndex_) < thing.values.size())
            {
                const TplValue& fv = thing.values[static_cast<std::size_t>(selectedFrameValueIndex_)];
                if (fv.type == ValueType::frame && sed::text::iequalsAscii(fv.name, "frame"))
                {
                    const render::frame_ghost::FramePose* const framePose =
                        findFramePose(thingIndex, selectedFrameValueIndex_);
                    const double x = framePose
                        ? framePose->x
                        : std::numeric_limits<double>::quiet_NaN();
                    const double y = framePose
                        ? framePose->y
                        : std::numeric_limits<double>::quiet_NaN();
                    const double z = framePose
                        ? framePose->z
                        : std::numeric_limits<double>::quiet_NaN();
                    const double pch = framePose
                        ? framePose->pitch
                        : std::numeric_limits<double>::quiet_NaN();
                    const double yaw = framePose
                        ? framePose->yaw
                        : std::numeric_limits<double>::quiet_NaN();
                    const double rol = framePose
                        ? framePose->roll
                        : std::numeric_limits<double>::quiet_NaN();

                    const std::uint32_t pickIdU32 =
                        (static_cast<std::size_t>(thingIndex) < thingPickIds_.size())
                            ? thingPickIds_[static_cast<std::size_t>(thingIndex)]
                            : 0u;

                    if (pickIdU32 != 0
                        && std::isfinite(thing.x) && std::isfinite(thing.y) && std::isfinite(thing.z)
                        && std::isfinite(thing.pch) && std::isfinite(thing.yaw) && std::isfinite(thing.rol)
                        && std::isfinite(x) && std::isfinite(y) && std::isfinite(z)
                        && std::isfinite(pch) && std::isfinite(yaw) && std::isfinite(rol))
                    {
                        TVector thingPyr{};
                        thingPyr.pitch = thing.pch;
                        thingPyr.yaw = thing.yaw;
                        thingPyr.roll = thing.rol;

                        TVector framePyr{};
                        framePyr.pitch = pch;
                        framePyr.yaw = yaw;
                        framePyr.roll = rol;

                        const TMatrix43 thingXf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, thingPyr);
                        const TMatrix43 frameXf = TMatrix43::Create(TVector{ x, y, z }, framePyr);

                        const QMatrix4x4 mThing = matrix43ToQt(thingXf);
                        bool invOk = false;
                        const QMatrix4x4 invThing = mThing.inverted(&invOk);
                        if (invOk)
                        {
                            frameXformEnabled = 1;
                            frameXformPickId = static_cast<int>(pickIdU32);
                            frameXformMatrix = matrix43ToQt(frameXf) * invThing;
                        }
                    }
                }
            }
        }
    }

    pickProgram_->setUniformValue("u_frame_xform_enabled", frameXformEnabled);
    pickProgram_->setUniformValue("u_frame_xform_id", frameXformPickId);
    pickProgram_->setUniformValue("u_frame_xform_matrix", frameXformMatrix);
    const bool pickTransformPreview =
        transformPreviewActive_ && !transformPreviewVertexFilter_;
    pickProgram_->setUniformValue("u_xform_enabled", pickTransformPreview ? 1 : 0);
    pickProgram_->setUniformValue(
        "u_xform_count", pickTransformPreview ? transformPreviewIdCount_ : 0);
    if (pickTransformPreview && transformPreviewIdCount_ > 0)
    {
        pickProgram_->setUniformValueArray(
            "u_xform_ids", transformPreviewIds_.data(), transformPreviewIdCount_);
    }
    pickProgram_->setUniformValue(
        "u_xform_matrix",
        pickTransformPreview ? transformPreviewMatrix_ : QMatrix4x4());

    if (occludeBySolidLevel)
    {
        f->glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        f->glEnable(GL_CULL_FACE);
        f->glCullFace(GL_BACK);
        f->glFrontFace(GL_CCW);

        pickProgram_->setUniformValue("u_geo_min", static_cast<int>(TGeoMode::Solid));
        if (haveTris)
        {
            triVao_->bind();
            bool cullEnabled = true;
            for (const TriBatch& batch : triBatches_)
            {
                if (batch.kind != TriKind::Level || batch.vertexCount <= 0)
                {
                    continue;
                }

                const bool wantCull = !batch.doubleSided;
                if (wantCull != cullEnabled)
                {
                    if (wantCull)
                    {
                        f->glEnable(GL_CULL_FACE);
                        f->glCullFace(GL_BACK);
                        f->glFrontFace(GL_CCW);
                    }
                    else
                    {
                        f->glDisable(GL_CULL_FACE);
                    }
                    cullEnabled = wantCull;
                }

                f->glDrawArrays(GL_TRIANGLES, batch.firstVertex, batch.vertexCount);
            }

            triVao_->release();
        }

        f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        f->glDisable(GL_CULL_FACE);
        pickProgram_->setUniformValue("u_geo_min", static_cast<int>(TGeoMode::Solid));
    }

    if (haveTris)
    {
        triVao_->bind();
        for (const DrawRange& r : thingPickRanges_)
        {
            if (r.count <= 0)
            {
                continue;
            }
            f->glDrawArrays(GL_TRIANGLES, r.first, r.count);
        }
        triVao_->release();
    }

    if (haveSprites)
    {
        if (spriteTrianglesDirty_)
        {
            rebuildSpriteVertices();
            spriteTriVao_->bind();
            spriteTriVbo_->bind();
            const std::size_t spriteByteCount = spriteTriVertices_.size() * sizeof(TriVertex);
            if (spriteByteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            {
                spriteTriVertexCount_ = 0;
                spriteTrianglesDirty_ = false;
                spriteHasType0Billboards_ = false;
                for (TriBatch& batch : spriteTriBatches_)
                {
                    batch.firstVertex = 0;
                    batch.vertexCount = 0;
                }
            }
            else
            {
                const int spriteBytes = static_cast<int>(spriteByteCount);
                if (spriteBytes > 0)
                {
                    if (spriteTriVboAllocatedBytes_ < spriteBytes)
                    {
                        const int half = spriteTriVboAllocatedBytes_ / 2;
                        const int grown = spriteTriVboAllocatedBytes_ + std::max(half, 4096);
                        const int newCap =
                            (spriteTriVboAllocatedBytes_ <= 0)
                                ? spriteBytes
                                : std::max(spriteBytes, grown);
                        spriteTriVbo_->allocate(nullptr, newCap);
                        spriteTriVboAllocatedBytes_ = newCap;
                    }
                    spriteTriVbo_->write(0, spriteTriVertices_.data(), spriteBytes);
                }
                else
                {
                    spriteTriVertexCount_ = 0;
                    spriteTrianglesDirty_ = false;
                    spriteHasType0Billboards_ = false;
                    for (TriBatch& batch : spriteTriBatches_)
                    {
                        batch.firstVertex = 0;
                        batch.vertexCount = 0;
                    }
                }
            }
            spriteTriVbo_->release();
        }

        if (spriteTriVertexCount_ > 0)
        {
            f->glDrawArrays(GL_TRIANGLES, 0, spriteTriVertexCount_);
        }
        spriteTriVao_->release();
    }

    pickProgram_->release();

    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(readW) * static_cast<std::size_t>(readH) * 4u, 0u);
    f->glReadPixels(readX, readY, readW, readH, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    pickFbo_->release();
    f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    f->glViewport(0, 0, pixelW, pixelH);
    f->glClearColor(
        backgroundClearColor_.redF(),
        backgroundClearColor_.greenF(),
        backgroundClearColor_.blueF(),
        1.0f);

    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4)
    {
        const std::uint32_t id = static_cast<std::uint32_t>(rgba[i + 0])
            | (static_cast<std::uint32_t>(rgba[i + 1]) << 8)
            | (static_cast<std::uint32_t>(rgba[i + 2]) << 16)
            | (static_cast<std::uint32_t>(rgba[i + 3]) << 24);
        if (id == 0 || id >= pickObjects_.size())
        {
            continue;
        }

        const PickObject& obj = pickObjects_[id];
        if (obj.kind != PickKind::Thing || obj.index < 0)
        {
            continue;
        }

        picked.insert(obj.index);
    }

    return picked;
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
std::optional<float> GlView::architecturalLightingSceneDepthForTest(
    const QPointF& widgetPoint)
{
    if (!glReady_ || !pickProgram_ || !triVao_ || triVertexCount_ <= 0)
    {
        return std::nullopt;
    }

    QMatrix4x4 projection{};
    QMatrix4x4 cameraView{};
    QVector3D eye{};
    QVector3D target{};
    float nearPlane{};
    float farPlane{};
    buildCameraMatrices(
        projection, cameraView, eye, target, nearPlane, farPlane);
    const std::vector<float> depths = pickSceneDepthsAtPoints(
        projection * cameraView,
        std::vector<QPointF>{ widgetPoint },
        true,
        true,
        render::picking::solidLevelOccludersCullBackFaces(
            render::picking::SolidLevelOcclusionPickFamily::Thing,
            false));
    if (depths.empty() || !pickFbo_ || !pickFbo_->isValid())
    {
        return std::nullopt;
    }
    return depths.front();
}
#endif

std::optional<GlView::PickHit> GlView::pickReadPixel(
    const QMatrix4x4& mvp,
    const TriKind kind,
    const int geoMin,
    const bool cullBackFaces,
    const int pixelX,
    const int pixelY,
    const std::array<std::uint32_t, 16>& excludeIds,
    const int excludeCount,
    const bool includeThingHelpers)
{
    const bool haveTris = triVao_ && triVertexCount_ > 0;
    const bool haveSprites = includeThingHelpers
        && (kind == TriKind::Thing)
        && spriteTriVao_
        && spriteTriVbo_
        && !spriteInstances_.empty();

    if (!glReady_ || !pickProgram_ || !pickFbo_ || (!haveTris && !haveSprites))
    {
        return std::nullopt;
    }

    if (pixelX < 0 || pixelY < 0 || pixelX >= pickFboWidth_ || pixelY >= pickFboHeight_)
    {
        return std::nullopt;
    }

    if (excludeCount < 0 || excludeCount > static_cast<int>(excludeIds.size()))
    {
        return std::nullopt;
    }

    auto* f = QOpenGLContext::currentContext()->functions();

    pickFbo_->bind();
    f->glViewport(0, 0, pickFboWidth_, pickFboHeight_);

    f->glDisable(GL_BLEND);
    f->glDisable(GL_DITHER);
    f->glDisable(GL_STENCIL_TEST);
    f->glEnable(GL_DEPTH_TEST);
    f->glDepthMask(GL_TRUE);
    f->glDepthFunc(GL_LESS);
    f->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    if (cullBackFaces)
    {
        f->glEnable(GL_CULL_FACE);
        f->glCullFace(GL_BACK);
        f->glFrontFace(GL_CCW);
    }
    else
    {
        f->glDisable(GL_CULL_FACE);
    }

    f->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    pickProgram_->bind();
    pickProgram_->setUniformValue("u_mvp", mvp);
    pickProgram_->setUniformValue("u_geo_min", geoMin);
    pickProgram_->setUniformValue("u_exclude_count", excludeCount);

    int frameXformEnabled = 0;
    int frameXformPickId = 0;
    QMatrix4x4 frameXformMatrix;
    frameXformMatrix.setToIdentity();

    if (frameEditEnabled_ && level_)
    {
        const std::int32_t thingIndex = selectedThing_;
        if (thingIndex >= 0 && static_cast<std::size_t>(thingIndex) < level_->things.size())
        {
            const Thing& thing = level_->things[static_cast<std::size_t>(thingIndex)];
            if (selectedFrameValueIndex_ >= 0 && static_cast<std::size_t>(selectedFrameValueIndex_) < thing.values.size())
            {
                const TplValue& fv = thing.values[static_cast<std::size_t>(selectedFrameValueIndex_)];
                if (fv.type == ValueType::frame && sed::text::iequalsAscii(fv.name, "frame"))
                {
                    const render::frame_ghost::FramePose* const framePose =
                        findFramePose(thingIndex, selectedFrameValueIndex_);
                    const double x = framePose
                        ? framePose->x
                        : std::numeric_limits<double>::quiet_NaN();
                    const double y = framePose
                        ? framePose->y
                        : std::numeric_limits<double>::quiet_NaN();
                    const double z = framePose
                        ? framePose->z
                        : std::numeric_limits<double>::quiet_NaN();
                    const double pch = framePose
                        ? framePose->pitch
                        : std::numeric_limits<double>::quiet_NaN();
                    const double yaw = framePose
                        ? framePose->yaw
                        : std::numeric_limits<double>::quiet_NaN();
                    const double rol = framePose
                        ? framePose->roll
                        : std::numeric_limits<double>::quiet_NaN();

                    const std::uint32_t pickIdU32 =
                        (static_cast<std::size_t>(thingIndex) < thingPickIds_.size())
                            ? thingPickIds_[static_cast<std::size_t>(thingIndex)]
                            : 0u;

                    if (pickIdU32 != 0
                        && std::isfinite(thing.x) && std::isfinite(thing.y) && std::isfinite(thing.z)
                        && std::isfinite(thing.pch) && std::isfinite(thing.yaw) && std::isfinite(thing.rol)
                        && std::isfinite(x) && std::isfinite(y) && std::isfinite(z)
                        && std::isfinite(pch) && std::isfinite(yaw) && std::isfinite(rol))
                    {
                        TVector thingPyr{};
                        thingPyr.pitch = thing.pch;
                        thingPyr.yaw = thing.yaw;
                        thingPyr.roll = thing.rol;

                        TVector framePyr{};
                        framePyr.pitch = pch;
                        framePyr.yaw = yaw;
                        framePyr.roll = rol;

                        const TMatrix43 thingXf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, thingPyr);
                        const TMatrix43 frameXf = TMatrix43::Create(TVector{ x, y, z }, framePyr);

                        const QMatrix4x4 mThing = matrix43ToQt(thingXf);
                        bool invOk = false;
                        const QMatrix4x4 invThing = mThing.inverted(&invOk);
                        if (invOk)
                        {
                            frameXformEnabled = 1;
                            frameXformPickId = static_cast<int>(pickIdU32);
                            frameXformMatrix = matrix43ToQt(frameXf) * invThing;
                        }
                    }
                }
            }
        }
    }

    pickProgram_->setUniformValue("u_frame_xform_enabled", frameXformEnabled);
    pickProgram_->setUniformValue("u_frame_xform_id", frameXformPickId);
    pickProgram_->setUniformValue("u_frame_xform_matrix", frameXformMatrix);

    if (excludeCount > 0)
    {
        std::array<int, 16> excludeInts{};
        for (int i = 0; i < excludeCount; ++i)
        {
            excludeInts[static_cast<std::size_t>(i)] = static_cast<int>(excludeIds[static_cast<std::size_t>(i)]);
        }
        pickProgram_->setUniformValueArray("u_exclude_ids", excludeInts.data(), excludeCount);
    }

    if (haveTris)
    {
        triVao_->bind();

        // Match rendering behavior for culling: double-sided faces must be pickable from both sides.
        if (kind == TriKind::Level && cullBackFaces)
        {
            bool cullEnabled = true; // enabled above when cullBackFaces == true
            for (const TriBatch& batch : triBatches_)
            {
                if (batch.kind != TriKind::Level || batch.vertexCount <= 0)
                {
                    continue;
                }

                const bool wantCull = !batch.doubleSided;
                if (wantCull != cullEnabled)
                {
                    if (wantCull)
                    {
                        f->glEnable(GL_CULL_FACE);
                        f->glCullFace(GL_BACK);
                        f->glFrontFace(GL_CCW);
                    }
                    else
                    {
                        f->glDisable(GL_CULL_FACE);
                    }
                    cullEnabled = wantCull;
                }

                f->glDrawArrays(GL_TRIANGLES, batch.firstVertex, batch.vertexCount);
            }

            // Restore the expected cull state for any subsequent draws in this pass.
            if (!cullEnabled)
            {
                f->glEnable(GL_CULL_FACE);
                f->glCullFace(GL_BACK);
                f->glFrontFace(GL_CCW);
            }
        }
        else
        {
            const std::vector<DrawRange>& ranges = (kind == TriKind::Level) ? levelPickRanges_ : thingPickRanges_;
            for (const DrawRange& r : ranges)
            {
                if (r.count <= 0)
                {
                    continue;
                }
                f->glDrawArrays(GL_TRIANGLES, r.first, r.count);
            }
        }

        triVao_->release();
    }

    if (haveSprites)
    {
        if (spriteTrianglesDirty_)
        {
            rebuildSpriteVertices();
            spriteTriVao_->bind();
            spriteTriVbo_->bind();
            const std::size_t spriteByteCount = spriteTriVertices_.size() * sizeof(TriVertex);
            if (spriteByteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            {
                spriteTriVertexCount_ = 0;
                spriteTrianglesDirty_ = false;
                spriteHasType0Billboards_ = false;
                for (TriBatch& batch : spriteTriBatches_)
                {
                    batch.firstVertex = 0;
                    batch.vertexCount = 0;
                }
            }
            else
            {
                const int spriteBytes = static_cast<int>(spriteByteCount);
                if (spriteBytes > 0)
                {
                    if (spriteTriVboAllocatedBytes_ < spriteBytes)
                    {
                        const int half = spriteTriVboAllocatedBytes_ / 2;
                        const int grown = spriteTriVboAllocatedBytes_ + std::max(half, 4096);
                        const int newCap =
                            (spriteTriVboAllocatedBytes_ <= 0)
                                ? spriteBytes
                                : std::max(spriteBytes, grown);
                        spriteTriVbo_->allocate(nullptr, newCap);
                        spriteTriVboAllocatedBytes_ = newCap;
                    }
                    spriteTriVbo_->write(0, spriteTriVertices_.data(), spriteBytes);
                }
                else
                {
                    spriteTriVertexCount_ = 0;
                    spriteTrianglesDirty_ = false;
                    spriteHasType0Billboards_ = false;
                    for (TriBatch& batch : spriteTriBatches_)
                    {
                        batch.firstVertex = 0;
                        batch.vertexCount = 0;
                    }
                }
            }
            spriteTriVbo_->release();
        }

        if (spriteTriVertexCount_ > 0)
        {
            f->glDrawArrays(GL_TRIANGLES, 0, spriteTriVertexCount_);
        }
        spriteTriVao_->release();
    }

    // Allow clicking gizmo icons to select things (pick pass).
    if (includeThingHelpers
        && kind == TriKind::Thing
        && (showThingGizmos_
            || (showArchThingGizmos_ && level_
                && level_->kind == ProjectType::JKDF2))
        && showThings_
        && level_
        && !level_->things.empty())
    {
        QMatrix4x4 invView{};
        bool invertOk = false;
        invView = camera_.viewMatrix().inverted(&invertOk);
        if (invertOk)
        {
            if (thingTypeGizmoCacheValid_
                && !thingTypeGizmoVertices_.empty()
                && thingGizmoTriVao_ && thingGizmoTriVbo_)
            {
                // The visible type-icon VBO already carries the exact pick IDs
                // and camera-facing geometry. Reuse it directly: selection
                // must not rewalk every Thing/resource or upload a second copy.
                pickProgram_->setUniformValue(
                    "u_xform_enabled",
                    thingTypeGizmoPreviewIdCount_ > 0 ? 1 : 0);
                pickProgram_->setUniformValue(
                    "u_xform_count", thingTypeGizmoPreviewIdCount_);
                if (thingTypeGizmoPreviewIdCount_ > 0)
                {
                    pickProgram_->setUniformValueArray(
                        "u_xform_ids",
                        thingTypeGizmoPreviewIds_.data(),
                        thingTypeGizmoPreviewIdCount_);
                }
                pickProgram_->setUniformValue(
                    "u_xform_matrix", thingTypeGizmoPreviewMatrix_);
                thingGizmoTriVao_->bind();
                if (showThingGizmos_ && thingTypeGizmoIconVertexCount_ > 0u)
                {
                    f->glDrawArrays(
                        GL_TRIANGLES,
                        0,
                        static_cast<int>(thingTypeGizmoIconVertexCount_));
                }
                if (showArchThingGizmos_
                    && thingTypeGizmoArchThingMarkerVertexCount_ > 0u)
                {
                    f->glDrawArrays(
                        GL_TRIANGLES,
                        static_cast<int>(thingTypeGizmoIconVertexCount_),
                        static_cast<int>(thingTypeGizmoArchThingMarkerVertexCount_));
                }
                thingGizmoTriVao_->release();
            }
            else
            {
            // The conservative cold-cache fallback below already applies the
            // transient matrix while constructing its vertices.
            pickProgram_->setUniformValue("u_xform_enabled", 0);
            pickProgram_->setUniformValue("u_xform_count", 0);
            QVector3D camRight(invView(0, 0), invView(1, 0), invView(2, 0));
            QVector3D camUp(invView(0, 1), invView(1, 1), invView(2, 1));
            if (camRight.lengthSquared() > sed::math::kTinyEpsilonF) { camRight.normalize(); }
            if (camUp.lengthSquared() > sed::math::kTinyEpsilonF) { camUp.normalize(); }

            const float gizmoHalfSize = std::clamp(thingGizmoWorldSize_, 0.001f, 1.0f) * 0.5f;
            const float lightStackHalfSize =
                std::clamp(lightGizmoWorldSize_, 0.001f, 1.0f) * 0.732f;
            const float archThingHalfSize =
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

            auto isTransformPreviewPickId = [&](const std::uint32_t pickId) -> bool
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

            auto mapPreviewPoint = [&](const QVector3D& p, const bool previewed) -> QVector3D
            {
                if (!previewed)
                {
                    return p;
                }

                return transformPreviewMatrix_.map(p);
            };

            struct WorldBounds
            {
                QVector3D min{};
                QVector3D max{};
                bool valid{};
            };

            auto calcThingWorldBounds = [&](const Thing& thing, const std::size_t thingIndex) -> WorldBounds
            {
                WorldBounds out{};

                std::uint32_t thingPickId = 0;
                if (thingIndex < thingPickIds_.size())
                {
                    thingPickId = thingPickIds_[thingIndex];
                }
                const bool previewed = isTransformPreviewPickId(thingPickId);

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
                    QVector3D q(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z));
                    q = mapPreviewPoint(q, previewed);

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

            std::vector<TriVertex> gizmoPickVerts{};
            gizmoPickVerts.reserve(level_->things.size() * 12);

            auto pushPV = [&](const QVector3D& p, const float pickId)
            {
                TriVertex tv{};
                tv.x = p.x();
                tv.y = p.y();
                tv.z = p.z();
                tv.u = 0.0f;
                tv.v = 0.0f;
                tv.r = 1.0f;
                tv.g = 1.0f;
                tv.b = 1.0f;
                tv.a = 1.0f;
                tv.pickId = pickId;
                tv.pickGeo = 4.0f;
                tv.sky = 0.0f;
                gizmoPickVerts.push_back(tv);
            };

            for (std::size_t thingIndex = 0; thingIndex < level_->things.size(); ++thingIndex)
            {
                const Thing& thing = level_->things[thingIndex];
                if (!isLayerVisible(thing.layer))
                {
                    continue;
                }
                if (!shouldShowThing(thing))
                {
                    continue;
                }
                if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
                {
                    continue;
                }

                float pickId = 0.0f;
                if (thingIndex < thingPickIds_.size())
                {
                    pickId = static_cast<float>(thingPickIds_[thingIndex]);
                }
                if (pickId <= 0.0f)
                {
                    continue;
                }

                const bool archThing = level_->kind == ProjectType::JKDF2
                    && (sed::level::thingFlags(*level_, templates_, thing)
                        & 0x00000008u) != 0u;
                // Keep the cold-cache fallback consistent with the cached
                // JKDF2 Arch marker batch. IJIM's bit 0x8 is Whip climb.
                const bool showArchThingMarker =
                    showArchThingGizmos_ && archThing;
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
                QVector3D ownerPosition(
                    static_cast<float>(thing.x),
                    static_cast<float>(thing.y),
                    static_cast<float>(thing.z));
                const bool previewed = isTransformPreviewPickId(
                    static_cast<std::uint32_t>(pickId));
                ownerPosition = mapPreviewPoint(ownerPosition, previewed);
                render::ThingGizmoStackLayout stackLayout{};
                stackLayout.thingTypeVisible = showThingGizmos_;
                stackLayout.thingLightVisible = visibleThingLightOwners.contains(
                    static_cast<std::int32_t>(thingIndex));
                stackLayout.archThingVisible = showArchThingMarker;
                stackLayout.thingTypeOffset = thingGizmoHeightAboveModel_;
                stackLayout.thingLightOffset = thingLightGizmoOffsetAboveModel_;
                stackLayout.archThingOffset = archThingGizmoOffsetAboveModel_;
                stackLayout.spacing = thingGizmoStackSpacing_;
                const auto pushMarker = [&](const QVector3D& center,
                                            const float halfSize)
                {
                    const QVector3D r = camRight * halfSize;
                    const QVector3D u = camUp * halfSize;
                    const QVector3D p0 = center - r - u;
                    const QVector3D p1 = center + r - u;
                    const QVector3D p2 = center + r + u;
                    const QVector3D p3 = center - r + u;
                    pushPV(p0, pickId);
                    pushPV(p1, pickId);
                    pushPV(p2, pickId);
                    pushPV(p0, pickId);
                    pushPV(p2, pickId);
                    pushPV(p3, pickId);
                };

                if (showThingGizmos_)
                {
                    const QVector3D center = render::thingGizmoStackDisplayPosition(
                        ownerPosition,
                        &ownerBounds,
                        render::ThingGizmoStackLane::ThingType,
                        gizmoHalfSize,
                        lightStackHalfSize,
                        archThingHalfSize,
                        stackLayout);
                    pushMarker(center, gizmoHalfSize);
                }
                if (showArchThingMarker)
                {
                    const QVector3D center = render::thingGizmoStackDisplayPosition(
                        ownerPosition,
                        &ownerBounds,
                        render::ThingGizmoStackLane::ArchThing,
                        gizmoHalfSize,
                        lightStackHalfSize,
                        archThingHalfSize,
                        stackLayout);
                    pushMarker(center, archThingHalfSize);
                }
            }

            if (!gizmoPickVerts.empty() && thingGizmoPickTriVao_ && thingGizmoPickTriVbo_)
            {
                thingGizmoPickTriVao_->bind();
                thingGizmoPickTriVbo_->bind();
                bool uploaded = false;
                const std::size_t byteCount = gizmoPickVerts.size() * sizeof(TriVertex);
                if (byteCount <= static_cast<std::size_t>(std::numeric_limits<int>::max()))
                {
                    const int bytes = static_cast<int>(byteCount);
                    if (bytes > 0)
                    {
                        if (thingGizmoPickTriVboAllocatedBytes_ < bytes)
                        {
                            const int half = thingGizmoPickTriVboAllocatedBytes_ / 2;
                            const int grown = thingGizmoPickTriVboAllocatedBytes_ + std::max(half, 4096);
                            const int newCap =
                                (thingGizmoPickTriVboAllocatedBytes_ <= 0)
                                    ? bytes
                                    : std::max(bytes, grown);
                            thingGizmoPickTriVbo_->allocate(nullptr, newCap);
                            thingGizmoPickTriVboAllocatedBytes_ = newCap;
                        }
                        thingGizmoPickTriVbo_->write(0, gizmoPickVerts.data(), bytes);
                        uploaded = true;
                    }
                }
                thingGizmoPickTriVbo_->release();
                if (uploaded)
                {
                    f->glDrawArrays(GL_TRIANGLES, 0, static_cast<int>(gizmoPickVerts.size()));
                }
                thingGizmoPickTriVao_->release();
            }
            }
        }
    }

    pickProgram_->release();

    std::array<std::uint8_t, 4> rgba{};
    f->glReadPixels(pixelX, pixelY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    float depth = 1.0f;
    f->glReadPixels(pixelX, pixelY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

    pickFbo_->release();
    f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

    const float dpr = devicePixelRatioF();
    const int viewW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int viewH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));
    f->glViewport(0, 0, viewW, viewH);
    f->glClearColor(
        backgroundClearColor_.redF(),
        backgroundClearColor_.greenF(),
        backgroundClearColor_.blueF(),
        1.0f);

    const std::uint32_t id = static_cast<std::uint32_t>(rgba[0])
        | (static_cast<std::uint32_t>(rgba[1]) << 8)
        | (static_cast<std::uint32_t>(rgba[2]) << 16)
        | (static_cast<std::uint32_t>(rgba[3]) << 24);

    if (id == 0)
    {
        return std::nullopt;
    }

    PickHit hit{};
    hit.id = id;
    hit.depth = depth;
    return hit;
}

std::vector<GlView::PickHit> GlView::pickPeel(
    const QMatrix4x4& mvp,
    const QPoint& widgetPos,
    const TriKind kind,
    const int geoMin,
    const bool cullBackFaces,
    const int maxHits,
    const bool includeThingHelpers)
{
    constexpr int kMaxPeels = 16;

    std::vector<PickHit> hits{};
    if (maxHits <= 0)
    {
        return hits;
    }

    const float dpr = devicePixelRatioF();
    const int pixelW = std::max(1, static_cast<int>(std::lround(static_cast<double>(width()) * dpr)));
    const int pixelH = std::max(1, static_cast<int>(std::lround(static_cast<double>(height()) * dpr)));

    ensurePickFbo(pixelW, pixelH);
    if (!pickFbo_)
    {
        return hits;
    }

    int px = static_cast<int>(std::lround(static_cast<double>(widgetPos.x()) * dpr));
    int py = static_cast<int>(std::lround(static_cast<double>(widgetPos.y()) * dpr));
    px = std::clamp(px, 0, pixelW - 1);
    py = std::clamp(py, 0, pixelH - 1);

    const int glX = px;
    const int glY = (pixelH - 1) - py;

    const int peelCount = std::min(maxHits, kMaxPeels);
    hits.reserve(static_cast<std::size_t>(peelCount));

    std::array<std::uint32_t, kMaxPeels> exclude{};
    int excludeCount = 0;

    while (static_cast<int>(hits.size()) < peelCount && excludeCount < kMaxPeels)
    {
        const std::optional<PickHit> hit = pickReadPixel(
            mvp,
            kind,
            geoMin,
            cullBackFaces,
            glX,
            glY,
            exclude,
            excludeCount,
            includeThingHelpers);
        if (!hit)
        {
            break;
        }

        hits.push_back(*hit);
        exclude[static_cast<std::size_t>(excludeCount)] = hit->id;
        ++excludeCount;
    }

    return hits;
}

} // namespace sed
