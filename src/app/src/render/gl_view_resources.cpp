#include "gl_view.hpp"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLTexture>
#include <QPointer>
#include <QSurface>
#include <QThread>

#include "sed/assets/3do.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/spr.hpp"
#include "sed/text.hpp"
#include "render/mesh/render_lighting_utils.hpp"
#include "ui/components/preview_resource_disk_watcher.hpp"

#include <exception>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace sed
{
namespace
{

class ScopedGlViewContext final
{
public:
    explicit ScopedGlViewContext(GlView& view)
        : view_(view),
          previousContext_(QOpenGLContext::currentContext()),
          previousSurface_(previousContext_ ? previousContext_->surface() : nullptr)
    {
        if ( previousContext_.data() == view_.context() )
        {
            return;
        }

        view_.makeCurrent();
        acquired_ = QOpenGLContext::currentContext() == view_.context();
    }

    ~ScopedGlViewContext()
    {
        if ( !acquired_ )
        {
            return;
        }

        view_.doneCurrent();
        if ( previousContext_ && previousSurface_ )
        {
            previousContext_->makeCurrent(previousSurface_);
        }
    }

    ScopedGlViewContext(const ScopedGlViewContext&) = delete;
    ScopedGlViewContext& operator=(const ScopedGlViewContext&) = delete;

    [[nodiscard]] bool ready() const noexcept
    {
        return QOpenGLContext::currentContext() == view_.context();
    }

private:
    GlView& view_;
    QPointer<QOpenGLContext> previousContext_{};
    QSurface* previousSurface_{};
    bool acquired_{};
};

std::string materialTextureBatchKey(
    const GlView& view,
    const std::string_view material,
    const std::string_view colormap)
{
    std::string key = render::makeBatchKey(material, colormap);
    if ( view.property("spr.preview.isolated_resource").toBool() )
    {
        key.insert(0, "spr|");
    }
    return key;
}

} // namespace

std::shared_ptr<const assets::Model3do> GlView::loadModel3do(
    const std::string_view name,
    const std::size_t lodIndex)
{
    const std::string_view trimmed = sed::text::trimView(name);
    if (!resources_ || trimmed.empty() || trimmed == "-")
    {
        return {};
    }

    watchResourceFile(trimmed, ".3do");

    std::string cacheKey(trimmed);
    if (!sed::text::iendsWithAscii(cacheKey, ".3do"))
    {
        cacheKey += ".3do";
    }

    // Keep LOD0 in the established model cache. Collision guides, bounds,
    // placement and ARCH vertex identities must continue to use its source
    // model even while the visible geometry chooses another geoset.
    if (lodIndex > 0u)
    {
        if (lodIndex >= 4u)
        {
            return {};
        }
        Model3doLodCache& lodCache = model3doLods_[cacheKey];
        if (lodCache.models[lodIndex])
        {
            return lodCache.models[lodIndex];
        }
        if (lodCache.attempted[lodIndex])
        {
            return {};
        }
        lodCache.attempted[lodIndex] = true;
        // A manually primed HQ model has no 3DO byte revision. Never combine
        // it with a lower geoset read from an unrelated disk revision.
        if (lodCache.primedSource)
        {
            return {};
        }
        try
        {
            if (!lodCache.sourceRead)
            {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                ++model3doResourceReadCount_;
#endif
                const auto bytes = resources_->tryReadBinaryWithExt(trimmed, ".3do");
                if (!bytes)
                {
                    Q_EMIT resourceWarningMessage(
                        QString("Missing 3DO: %1").arg(QString::fromStdString(cacheKey)));
                    return {};
                }
                lodCache.sourceBytes = *bytes;
                lodCache.sourceRead = true;
            }

            assets::Model3doLoadOptions options{};
            options.lod = static_cast<std::int32_t>(lodIndex);
            auto model = std::make_shared<assets::Model3do>(
                assets::loadFrom3DO(
                    std::span<const std::byte>(lodCache.sourceBytes),
                    options));
            lodCache.models[lodIndex] = model;
            return model;
        }
        catch (const std::exception& e)
        {
            Q_EMIT resourceErrorMessage(
                QString("Failed to load 3DO %1 LOD %2: %3")
                    .arg(QString::fromStdString(cacheKey))
                    .arg(static_cast<int>(lodIndex))
                    .arg(QString::fromUtf8(e.what())));
            return {};
        }
        catch (...)
        {
            Q_EMIT resourceErrorMessage(
                QString("Failed to load 3DO %1 LOD %2")
                    .arg(QString::fromStdString(cacheKey))
                    .arg(static_cast<int>(lodIndex)));
            return {};
        }
    }

    if (const auto it = models3do_.find(cacheKey); it != models3do_.end())
    {
        return it->second;
    }
    if (missingModels3do_.contains(cacheKey))
    {
        return {};
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    ++model3doResourceReadCount_;
#endif
    try
    {
        auto bytes = resources_->tryReadBinaryWithExt(trimmed, ".3do");
        if (!bytes)
        {
            if (missingModels3do_.insert(cacheKey).second)
            {
                Q_EMIT resourceWarningMessage(
                    QString("Missing 3DO: %1").arg(QString::fromStdString(cacheKey)));
            }
            return {};
        }

        // A multi-geoset HQ model and every lower variant must come from
        // the same resource revision, even when LOD is enabled later. Keep
        // its source bytes once, then parse each requested lower tier once.
        // Single-geoset models need no retained copy.
        const std::span<const std::byte> sourceBytes(*bytes);
        auto model = std::make_shared<assets::Model3do>(
            assets::loadFrom3DO(sourceBytes));
        if (model->geosetCount > 1u)
        {
            Model3doLodCache lodCache{};
            lodCache.sourceBytes = std::move(*bytes);
            lodCache.sourceRead = true;
            // An independently primed lower variant cannot be proven to
            // belong to these HQ bytes; discard it on source publication.
            model3doLods_.insert_or_assign(
                cacheKey, std::move(lodCache));
        }
        else
        {
            model3doLods_.erase(cacheKey);
        }
        models3do_.emplace(cacheKey, model);
        return model;
    }
    catch (const std::exception& e)
    {
        if (missingModels3do_.insert(cacheKey).second)
        {
            Q_EMIT resourceErrorMessage(QString("Failed to load 3DO %1: %2")
                                    .arg(QString::fromStdString(cacheKey))
                                    .arg(QString::fromUtf8(e.what())));
        }
        return {};
    }
    catch (...)
    {
        if (missingModels3do_.insert(cacheKey).second)
        {
            Q_EMIT resourceErrorMessage(
                QString("Failed to load 3DO %1").arg(QString::fromStdString(cacheKey)));
        }
        return {};
    }
}

void GlView::primeModel3do(
    const std::string_view name,
    std::shared_ptr<const assets::Model3do> model,
    const std::size_t lodIndex)
{
    const std::string_view trimmed = sed::text::trimView(name);
    if (!model || trimmed.empty() || trimmed == "-")
    {
        return;
    }

    // An immutable model publication can replace frame-ghost source geometry
    // while retaining the same Level and owner value strings.
    invalidateAllFrameRenderCaches();

    // A model publication invalidates authored source identities and any
    // renderer-only triangle patches that refer to the old resource.
    clearArchitecturalLightingPreview(false);

    std::string cacheKey(trimmed);
    if (!sed::text::iendsWithAscii(cacheKey, ".3do"))
    {
        cacheKey += ".3do";
    }

    if (lodIndex == 0u)
    {
        // Replacing HQ invalidates every alternative parsed from the previous
        // resource revision. A selected LOD must never mix old/new geometry.
        model3doLods_.erase(cacheKey);
        model3doLods_[cacheKey].primedSource = true;
        missingModels3do_.erase(cacheKey);
        models3do_.insert_or_assign(cacheKey, std::move(model));
    }
    else if (lodIndex < 4u)
    {
        Model3doLodCache& lodCache = model3doLods_[cacheKey];
        lodCache.models[lodIndex] = std::move(model);
        lodCache.attempted[lodIndex] = true;
    }
    architecturalLightingRenderCache_.reset();
    architecturalLightingSelectedOwnerCaches_.clear();
    architecturalLightingSelectedOwnerCacheByThing_.clear();
    clearArchitecturalLightingSelectedVertices();
    architecturalLightingOwnerGizmoMetadataValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoEntries_.clear();
    architecturalLightingOwnerGizmoEntryByThing_.clear();
    architecturalLightingOwnerGizmoVertices_.clear();
}

void GlView::primeMaterialTexture(
    const std::string_view material,
    const std::string_view colormap,
    const QImage& image,
    const std::int32_t alphaBpp,
    const QVector3D& solidColor,
    const std::optional<ClassicSolidLightRamp>& classicSolidLightRamp)
{
    if (material.empty() || material == "-" || image.isNull())
    {
        return;
    }

    const std::string batchKey = materialTextureBatchKey(*this, material, colormap);
    if (const auto existing = textures_.find(batchKey);
        existing != textures_.end() && existing->second.missing)
    {
        textures_.erase(existing);
    }
    missingMats_.erase(std::string(material));
    preparedTextureSources_.insert_or_assign(
        batchKey,
        PreparedTextureSource{
            .image = image,
            .alphaBpp = std::max<std::int32_t>(0, alphaBpp),
            .solidColor = solidColor,
            .classicSolidLightRamp = classicSolidLightRamp,
        });
    alphaAdjoinWireSupportsDirty_ = true;
}

bool GlView::updatePreviewMaterialTexture(
    const std::string_view material,
    const std::string_view colormap,
    const QImage& image,
    const std::int32_t alphaBpp,
    const QVector3D& solidColor,
    const std::optional<ClassicSolidLightRamp>& classicSolidLightRamp)
{
    const std::string_view materialName = sed::text::trimView(material);
    if ( QThread::currentThread() != thread()
        || !property("spr.preview.isolated_resource").toBool()
        || materialName.empty()
        || materialName == "-"
        || image.isNull()
        || image.width() <= 0
        || image.height() <= 0
        || !std::isfinite(solidColor.x())
        || !std::isfinite(solidColor.y())
        || !std::isfinite(solidColor.z()) )
    {
        return false;
    }

    QImage rgbaImage = image.format() == QImage::Format_RGBA8888
        ? image
        : image.convertToFormat(QImage::Format_RGBA8888);
    if ( rgbaImage.isNull() )
    {
        return false;
    }

    const std::string batchKey = materialTextureBatchKey(*this, materialName, colormap);
    PreparedTextureSource prepared{
        .image = rgbaImage,
        .alphaBpp = std::max<std::int32_t>(0, alphaBpp),
        .solidColor = solidColor,
        .classicSolidLightRamp = classicSolidLightRamp,
    };

    if ( !glReady_ || !context() || !context()->isValid() )
    {
        const auto existing = textures_.find(batchKey);
        if ( existing != textures_.end() && existing->second.missing )
        {
            textures_.erase(existing);
        }
        preparedTextureSources_.insert_or_assign(batchKey, std::move(prepared));
        missingMats_.erase(std::string(materialName));
        alphaAdjoinWireSupportsDirty_ = true;
        update();
        return true;
    }

    ScopedGlViewContext contextGuard(*this);
    if ( !contextGuard.ready() )
    {
        return false;
    }

    try
    {
        auto existing = textures_.find(batchKey);
        if ( existing != textures_.end()
            && existing->second.texture
            && !existing->second.missing
            && existing->second.width == rgbaImage.width()
            && existing->second.height == rgbaImage.height() )
        {
            existing->second.texture->setData(
                0,
                0,
                QOpenGLTexture::RGBA,
                QOpenGLTexture::UInt8,
                rgbaImage.constBits());
            existing->second.alphaBpp = prepared.alphaBpp;
            existing->second.solidColor = solidColor;
            existing->second.classicSolidLightRamp = classicSolidLightRamp;
        }
        else
        {
            auto texture = std::make_unique<QOpenGLTexture>(rgbaImage);
            if ( !texture->isCreated() )
            {
                return false;
            }
            texture->setWrapMode(QOpenGLTexture::ClampToEdge);
            texture->setMinificationFilter(QOpenGLTexture::Linear);
            texture->setMagnificationFilter(QOpenGLTexture::Linear);

            TextureInfo info{};
            info.texture = std::move(texture);
            info.width = rgbaImage.width();
            info.height = rgbaImage.height();
            info.alphaBpp = prepared.alphaBpp;
            info.solidColor = solidColor;
            info.classicSolidLightRamp = classicSolidLightRamp;
            info.missing = false;
            textures_.insert_or_assign(batchKey, std::move(info));
        }

        preparedTextureSources_.insert_or_assign(batchKey, std::move(prepared));
        missingMats_.erase(std::string(materialName));
        alphaAdjoinWireSupportsDirty_ = true;
    }
    catch ( const std::exception& )
    {
        return false;
    }
    catch ( ... )
    {
        return false;
    }

    update();
    return true;
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
bool GlView::materialTextureMissingForTest(
    const std::string_view material,
    const std::string_view colormap) const noexcept
{
    const auto found = textures_.find(render::makeBatchKey(material, colormap));
    return found != textures_.end() && found->second.missing;
}

std::optional<std::int32_t> GlView::materialTextureAlphaBppForTest(
    const std::string_view material,
    const std::string_view colormap) const noexcept
{
    const auto found = textures_.find(render::makeBatchKey(material, colormap));
    if (found == textures_.end())
    {
        return std::nullopt;
    }
    return found->second.alphaBpp;
}

#endif

const assets::SprFile* GlView::loadSprite(const std::string_view name)
{
    const std::string_view trimmed = sed::text::trimView(name);
    if (!resources_ || trimmed.empty() || trimmed == "-")
    {
        return nullptr;
    }

    watchResourceFile(trimmed, ".spr");

    std::string cacheKey(trimmed);
    if (!sed::text::iendsWithAscii(cacheKey, ".spr"))
    {
        cacheKey += ".spr";
    }

    if (const auto it = sprites_.find(cacheKey); it != sprites_.end())
    {
        return &it->second;
    }
    if (missingSprites_.contains(cacheKey))
    {
        return nullptr;
    }

    try
    {
        const auto text = resources_->tryReadTextWithExt(trimmed, ".spr");
        if (!text)
        {
            if (missingSprites_.insert(cacheKey).second)
            {
                Q_EMIT resourceWarningMessage(
                    QString("Missing SPR: %1").arg(QString::fromStdString(cacheKey)));
            }
            return nullptr;
        }

        std::istringstream iss(*text);
        assets::SprFile spr = assets::loadFromSPR(iss);
        const auto [it, inserted] = sprites_.emplace(std::move(cacheKey), std::move(spr));
        (void)inserted;
        return &it->second;
    }
    catch (const std::exception& e)
    {
        if (missingSprites_.insert(cacheKey).second)
        {
            Q_EMIT resourceErrorMessage(QString("Failed to load SPR %1: %2")
                                    .arg(QString::fromStdString(cacheKey))
                                    .arg(QString::fromUtf8(e.what())));
        }
        return nullptr;
    }
    catch (...)
    {
        if (missingSprites_.insert(cacheKey).second)
        {
            Q_EMIT resourceErrorMessage(
                QString("Failed to load SPR %1").arg(QString::fromStdString(cacheKey)));
        }
        return nullptr;
    }
}

void GlView::clearTextures()
{
    // Resource roots, archive revisions, and watched-file reloads can all
    // replace the model identity used by cached Frame ghosts.
    invalidateAllFrameRenderCaches();

    clearArchitecturalLightingPreview(false);
    textures_.clear();
    preparedTextureSources_.clear();
    alphaAdjoinWireSupportsDirty_ = true;
    palettes_.clear();
    cmpFiles_.clear();
    missingPalettes_.clear();
    missingMats_.clear();
    models3do_.clear();
    model3doLods_.clear();
    missingModels3do_.clear();
    architecturalLightingRenderCache_.reset();
    architecturalLightingSelectedOwnerCaches_.clear();
    architecturalLightingSelectedOwnerCacheByThing_.clear();
    clearArchitecturalLightingSelectedVertices();
    architecturalLightingOwnerGizmoMetadataValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoEntries_.clear();
    architecturalLightingOwnerGizmoEntryByThing_.clear();
    architecturalLightingOwnerGizmoVertices_.clear();
    modelBounds3do_.clear();
    sprites_.clear();
    missingSprites_.clear();
    gizmoTextures_.clear();
    lightTexture_.reset();
    thingLightTexture_.reset();
    thingHeadLightTexture_.reset();
    sectorLightTexture_.reset();
    architecturalLightingTexture_.reset();
}

void GlView::watchResourceFile(
    const std::string_view name,
    const std::string_view ext)
{
    if (resourceDiskWatcher_)
    {
        resourceDiskWatcher_->watchResource(name, ext);
    }
}

void GlView::releaseDynamicLightUniformBuffer()
{
    if ( dynamicLightUniformBuffer_ != 0 )
    {
        QOpenGLContext* const activeContext = QOpenGLContext::currentContext();
        if ( activeContext && activeContext == context() )
        {
            if ( QOpenGLExtraFunctions* extra = activeContext->extraFunctions() )
            {
                extra->initializeOpenGLFunctions();
                extra->glDeleteBuffers(1, &dynamicLightUniformBuffer_);
            }
        }
    }
    dynamicLightUniformBuffer_ = 0;
    dynamicLightUniformBufferReady_ = false;
}

void GlView::reloadCachedResources()
{
    // A resource publication replaces model identities and can invalidate the
    // selected ARCH owner without changing the document Thing. Restore and
    // discard the sparse staged preview before dropping those caches.
    clearArchitecturalLightingPreview(false);
    if (glReady_)
    {
        makeCurrent();
        clearTextures();
        doneCurrent();
    }
    else
    {
        clearTextures();
    }

    notifyAllMeshesChanged();
}

} // namespace sed
