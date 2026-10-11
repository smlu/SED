#include "gl_view_common.hpp"
#include "sed/level/thing.hpp"

#include "render/gizmos/gizmo_icon_utils.hpp"
#include "render/fast_update/thing_transform_update.hpp"
#include "render/lighting/dynamic_light_utils.hpp"
#include "render/mesh/render_lighting_utils.hpp"
#include "render/mesh/thing_render_geometry_utils.hpp"
#include "render/overlays/thing_shape_overlay_utils.hpp"
#include "render/overlays/template_collision_bounds_guide_utils.hpp"
#include "render/selection/surface_selection_utils.hpp"
#include "render/texture/texture_alpha_utils.hpp"
#include "render/translucency/translucent_order_utils.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <iostream>
#include <limits>
#include <ranges>
#include <unordered_set>

namespace sed
{

namespace
{

int originalSpriteRenderType(const ProjectType project, const int storedType) noexcept
{
    // Classic rdSprite_Draw has one camera-facing branch and ignores the
    // admitted SPR type token. IJIM implements distinct type-0/type-2 frames.
    return project != ProjectType::IJIM && storedType >= 0 && storedType <= 2
        ? 0
        : storedType;
}

struct GeometrySelectionEdgeKeyHash
{
    std::size_t operator()(const render::GeometryWireEdgeKey& edge) const noexcept
    {
        std::size_t seed = std::hash<std::int32_t>{}(edge.sector);
        seed ^= std::hash<std::int32_t>{}(edge.first)
            + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
        seed ^= std::hash<std::int32_t>{}(edge.second)
            + 0x9e3779b9u + (seed << 6u) + (seed >> 2u);
        return seed;
    }
};

bool wireframeSurfaceCanMatchDepthFromFill(
    const ProjectType project,
    const Surface& surface) noexcept
{
    const bool filledGeometry =
        surface.geo == static_cast<std::int32_t>(TGeoMode::Solid)
        || surface.geo == static_cast<std::int32_t>(TGeoMode::Texture);
    if (!filledGeometry
        || (surface.surfaceFlags & (SF_SkyHorizon | SF_SkyCeiling)) != 0
        || surface.vertices.size() < 3
        || surface.texVertices.size() != surface.vertices.size()
        || !std::isfinite(surface.uScale)
        || !std::isfinite(surface.vScale))
    {
        return false;
    }

    const std::uint32_t renderFaceFlags = render::levelSurfaceRenderFaceFlags(
        project,
        surface.surfaceFlags,
        surface.faceFlags);
    // RD_Adjoin identifies portal routing, not an unconditional no-depth-write
    // rule. Opaque adjoins remain in the solid pass; only authored face flags
    // that disable depth ownership are excluded here. MAT alpha is resolved by
    // the later render queue and is not available during mesh construction.
    constexpr std::uint32_t kNonDepthWritingFaceFlags =
        FF_Transluent | FF_ZWriteDisabled;
    if ((renderFaceFlags & kNonDepthWritingFaceFlags) != 0u)
    {
        return false;
    }

    return !std::ranges::any_of(
        surface.texVertices,
        [](const TexVertex& texVertex)
        {
            return !std::isfinite(texVertex.u)
                || !std::isfinite(texVertex.v);
        });
}

const Vertex* wireframeSurfacePlaneReference(
    const Sector& sector,
    const Surface& surface,
    const std::size_t boundaryEdgeIndex) noexcept
{
    const std::optional<std::size_t> referenceIndex =
        render::wireframeSurfacePlaneReferenceVertexIndex(
            sector,
            surface,
            boundaryEdgeIndex);
    return referenceIndex ? &sector.vertices[*referenceIndex] : nullptr;
}

SpriteBasis makeCameraFacingSpriteBasis(
    const app::ViewportCamera::OrbitState& orbit) noexcept
{
    const app::ViewportCamera::OrbitBasis cameraBasis =
        app::ViewportCamera::BasisFromOrbit(
            orbit.yawDeg, orbit.pitchDeg, orbit.rollDeg);
    return SpriteBasis{
        .right = cameraBasis.right,
        .forward = cameraBasis.forward,
        .up = cameraBasis.up,
    };
}

} // namespace

void GlView::refreshArchitecturalLightingRenderCache()
{
    architecturalLightingRenderCache_.reset();
    architecturalLightingSelectedOwnerCaches_.clear();
    architecturalLightingSelectedOwnerCacheByThing_.clear();
    // Model publication and Thing transforms also change the owner-marker
    // bounds. Rebuild the small dedicated overlay cache on its next draw;
    // mouse queries then see the same anchor that was uploaded.
    architecturalLightingOwnerGizmoMetadataValid_ = false;
    architecturalLightingOwnerGizmoCacheValid_ = false;
    architecturalLightingOwnerGizmoEntries_.clear();
    architecturalLightingOwnerGizmoEntryByThing_.clear();
    architecturalLightingOwnerGizmoVertices_.clear();
    const bool hadVertexSelection =
        !architecturalLightingSelectedVertices_.empty();
    if (!architecturalLightingEditingActive_ || !level_)
    {
        architecturalLightingSelectedVertices_.clear();
        architecturalLightingSelectedVertexSet_.clear();
        architecturalLightingSelectedVerticesImplicitOwnerExpansion_ = false;
        if (hadVertexSelection)
        {
            Q_EMIT architecturalLightingVertexSelectionChanged();
        }
        return;
    }

    std::vector<std::int32_t> ownerIndices{};
    ownerIndices.reserve(highlightedSelection_.things.size() + 1u);
    if (selectedThing_ >= 0
        && level_
        && static_cast<std::size_t>(selectedThing_) < level_->things.size())
    {
        ownerIndices.push_back(selectedThing_);
    }
    for (const std::int32_t thingIndex : highlightedSelection_.things)
    {
        if (thingIndex >= 0
            && level_
            && static_cast<std::size_t>(thingIndex) < level_->things.size())
        {
            ownerIndices.push_back(thingIndex);
        }
    }
    std::sort(ownerIndices.begin(), ownerIndices.end());
    ownerIndices.erase(
        std::unique(ownerIndices.begin(), ownerIndices.end()),
        ownerIndices.end());

    const auto buildCache = [this](const std::int32_t thingIndex)
        -> std::optional<ArchitecturalLightingRenderCache>
    {
        if (!level_ || thingIndex < 0
            || static_cast<std::size_t>(thingIndex) >= level_->things.size())
        {
            return std::nullopt;
        }

        const Thing& thing = level_->things[static_cast<std::size_t>(thingIndex)];
        if (!isLayerVisible(thing.layer) || isLayerLocked(thing.layer))
        {
            return std::nullopt;
        }
        if (!std::isfinite(thing.x)
            || !std::isfinite(thing.y)
            || !std::isfinite(thing.z)
            || !std::isfinite(thing.pch)
            || !std::isfinite(thing.yaw)
            || !std::isfinite(thing.rol))
        {
            return std::nullopt;
        }

        const std::string modelKey =
            thing::thingModel3doName(*level_, templates_, thing);
        if (modelKey.empty())
        {
            return std::nullopt;
        }

        std::shared_ptr<const assets::Model3do> model{};
        std::string cachedModelKey{};
        if (const auto found = models3do_.find(modelKey);
            found != models3do_.end())
        {
            model = found->second;
            cachedModelKey = found->first;
        }
        else
        {
            // Resource names are authored case-insensitively, while the
            // existing resource cache preserves its first spelling.
            const auto fallbackFound = std::find_if(
                models3do_.begin(),
                models3do_.end(),
                [&modelKey](const auto& entry)
                {
                    return sed::text::iequalsAscii(entry.first, modelKey);
                });
            if (fallbackFound == models3do_.end())
            {
                return std::nullopt;
            }
            model = fallbackFound->second;
            cachedModelKey = fallbackFound->first;
        }
        if (!model || model->meshes.empty())
        {
            return std::nullopt;
        }

        const MotsArchLightObject* archLighting =
            render::resolveMotsArchLightingPreview(
                *level_, templates_, thing, *model);
        std::optional<MotsArchLightObject> defaultArchLighting{};
        if (!archLighting && thing.archLightingIndex == -1
            && sed::level::isMotsArchThing(
                *level_, thing, templates_))
        {
            defaultArchLighting =
                render::makeDefaultMotsArchLightingPreview(*model);
            if (defaultArchLighting)
            {
                archLighting = &*defaultArchLighting;
            }
        }
        if (!archLighting)
        {
            return std::nullopt;
        }

        TVector pyr{};
        pyr.pitch = thing.pch;
        pyr.yaw = thing.yaw;
        pyr.roll = thing.rol;
        const TMatrix43 thingXf =
            TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, pyr);

        ArchitecturalLightingRenderCache cache{};
        cache.model = model;
        cache.modelIdentity = model.get();
        cache.modelKey = std::move(cachedModelKey);
        cache.archLightingRevision =
            render::motsArchLightingRevision(*model, *archLighting);
        cache.archLightingIndex = thing.archLightingIndex;
        cache.thingGeometryRevision = thingGeometrySignature(thing);
        cache.thingIndex = thingIndex;

        std::size_t vertexCount = 0;
        for (const assets::Model3doMesh& mesh : model->meshes)
        {
            vertexCount += mesh.vertices.size();
        }
        cache.vertices.reserve(vertexCount);

        for (std::size_t meshIndex = 0;
             meshIndex < model->meshes.size();
             ++meshIndex)
        {
            const assets::Model3doMesh& mesh = model->meshes[meshIndex];
            const std::optional<std::size_t> sourceMeshIndex =
                render::motsArchLightingSourceMeshIndex(*model, meshIndex);
            if (!sourceMeshIndex
                || *sourceMeshIndex >= archLighting->meshes.size()
                || archLighting->meshes[*sourceMeshIndex].vertices.size()
                    < mesh.vertices.size())
            {
                return std::nullopt;
            }
            const MotsArchLightMesh& archMesh =
                archLighting->meshes[*sourceMeshIndex];
            for (std::size_t vertexIndex = 0;
                 vertexIndex < mesh.vertices.size();
                 ++vertexIndex)
            {
                const assets::Model3doVertex& modelVertex =
                    mesh.vertices[vertexIndex];
                const TVector world = thingXf.TransformPoint(
                    TVector{ modelVertex.x, modelVertex.y, modelVertex.z });
                if (!std::isfinite(world.x)
                    || !std::isfinite(world.y)
                    || !std::isfinite(world.z))
                {
                    return std::nullopt;
                }

                cache.vertices.push_back(ArchitecturalLightingRenderVertex{
                    .thingIndex = thingIndex,
                    .lodIndex = 0,
                    // ARCHLIGHTING edits use the authored/source mesh
                    // ordinal. A hierarchy clone may have a different
                    // render-mesh position while consuming this archive mesh.
                    .meshIndex = static_cast<std::int32_t>(*sourceMeshIndex),
                    .vertexIndex = static_cast<std::int32_t>(vertexIndex),
                    .worldPosition = QVector3D(
                        static_cast<float>(world.x),
                        static_cast<float>(world.y),
                        static_cast<float>(world.z)),
                    .lighting = archMesh.vertices[vertexIndex],
                });
            }
        }

        return cache.vertices.empty()
            ? std::nullopt
            : std::optional<ArchitecturalLightingRenderCache>{std::move(cache)};
    };

    architecturalLightingSelectedOwnerCaches_.reserve(ownerIndices.size());
    for (const std::int32_t thingIndex : ownerIndices)
    {
        std::optional<ArchitecturalLightingRenderCache> cache =
            buildCache(thingIndex);
        if (!cache)
        {
            continue;
        }

        const std::size_t cacheIndex =
            architecturalLightingSelectedOwnerCaches_.size();
        architecturalLightingSelectedOwnerCacheByThing_.emplace(
            thingIndex, cacheIndex);
        if (thingIndex == selectedThing_)
        {
            architecturalLightingRenderCache_ = *cache;
        }
        architecturalLightingSelectedOwnerCaches_.push_back(std::move(*cache));
    }

    // Resource/model publication, owner deselection, and hierarchy changes
    // can invalidate an exact authored identity. Prune against the newly
    // published cache in one set pass so no stale vertex can be edited after
    // a replacement or selection change.
    std::unordered_set<ArchitecturalLightingVertexIdentity,
        ArchitecturalLightingVertexIdentityHash> validIdentities{};
    std::size_t vertexCount = 0;
    for (const ArchitecturalLightingRenderCache& cache
         : architecturalLightingSelectedOwnerCaches_)
    {
        vertexCount += cache.vertices.size();
    }
    validIdentities.reserve(vertexCount);
    for (const ArchitecturalLightingRenderCache& cache
         : architecturalLightingSelectedOwnerCaches_)
    {
        for (const ArchitecturalLightingRenderVertex& vertex : cache.vertices)
        {
            validIdentities.insert(architecturalLightingVertexIdentity(vertex));
        }
    }
    if ( pendingArchitecturalLightingRecovery_
        && pendingArchitecturalLightingSelectionReady_ )
    {
        (void)tryApplyPendingArchitecturalLightingRecovery();
    }
    std::vector<ArchitecturalLightingVertexIdentity> retained{};
    retained.reserve(architecturalLightingSelectedVertices_.size());
    for (const ArchitecturalLightingVertexIdentity& identity
         : architecturalLightingSelectedVertices_)
    {
        if (validIdentities.contains(identity))
        {
            retained.push_back(identity);
        }
    }
    const bool vertexSelectionChanged =
        retained.size() != architecturalLightingSelectedVertices_.size();
    if (vertexSelectionChanged)
    {
        architecturalLightingSelectedVertices_ = std::move(retained);
        architecturalLightingSelectedVerticesImplicitOwnerExpansion_ = false;
        architecturalLightingSelectedVertexSet_.clear();
        architecturalLightingSelectedVertexSet_.insert(
            architecturalLightingSelectedVertices_.begin(),
            architecturalLightingSelectedVertices_.end());
    }
    if (vertexSelectionChanged)
    {
        Q_EMIT architecturalLightingVertexSelectionChanged();
    }
}

GlView::ArchitecturalLightingRecoveryRestoreResult
GlView::tryApplyPendingArchitecturalLightingRecovery()
{
    if ( !pendingArchitecturalLightingRecovery_ )
    {
        return ArchitecturalLightingRecoveryRestoreResult::NoPending;
    }
    if ( !pendingArchitecturalLightingSelectionReady_
        || renderPipeline_ != RenderPipeline::Editor
        || !architecturalLightingEditingActive_
        || !level_ )
    {
        return ArchitecturalLightingRecoveryRestoreResult::Deferred;
    }

    const RecoveryState::ArchitecturalLightingState& requested =
        *pendingArchitecturalLightingRecovery_;
    const auto reject = [this]()
    {
        pendingArchitecturalLightingRecovery_.reset();
        pendingArchitecturalLightingSelectionReady_ = false;
        pendingArchitecturalLightingOwnerSelection_.clear();
        showToast(
            "ARCH lighting: restored vertex selection no longer matches the loaded model",
            3600,
            ToastPlacement::BottomCenter,
            ToastGroup::Error);
        Q_EMIT viewportStateChanged();
        return ArchitecturalLightingRecoveryRestoreResult::Rejected;
    };

    std::unordered_set<ArchitecturalLightingVertexIdentity,
        ArchitecturalLightingVertexIdentityHash> available{};
    std::size_t totalVertices = 0;
    for ( const ArchitecturalLightingRenderCache& cache
        : architecturalLightingSelectedOwnerCaches_ )
    {
        totalVertices += cache.vertices.size();
    }
    available.reserve(totalVertices);
    for ( const ArchitecturalLightingRenderCache& cache
        : architecturalLightingSelectedOwnerCaches_ )
    {
        for ( const ArchitecturalLightingRenderVertex& vertex : cache.vertices )
        {
            available.insert(architecturalLightingVertexIdentity(vertex));
        }
    }

    const auto hasCachedModel = [this](const std::int32_t owner)
    {
        if ( owner < 0 || static_cast<std::size_t>(owner) >= level_->things.size() )
        {
            return false;
        }
        const std::string modelKey = thing::thingModel3doName(
            *level_,
            templates_,
            level_->things[static_cast<std::size_t>(owner)]);
        if ( modelKey.empty() )
        {
            return false;
        }
        return std::any_of(
            models3do_.begin(),
            models3do_.end(),
            [&modelKey](const auto& entry)
            {
                return sed::text::iequalsAscii(entry.first, modelKey);
            });
    };

    std::vector<ArchitecturalLightingVertexIdentity> restored{};
    restored.reserve(requested.selectedVertices.size());
    for ( const ArchitecturalLightingVertexIdentity& identity
        : requested.selectedVertices )
    {
        if ( identity.thingIndex < 0
            || static_cast<std::size_t>(identity.thingIndex) >= level_->things.size() )
        {
            return reject();
        }
        if ( !std::binary_search(
            pendingArchitecturalLightingOwnerSelection_.begin(),
            pendingArchitecturalLightingOwnerSelection_.end(),
            identity.thingIndex) )
        {
            return reject();
        }
        if ( !available.contains(identity) )
        {
            if ( !hasCachedModel(identity.thingIndex) )
            {
                return ArchitecturalLightingRecoveryRestoreResult::Deferred;
            }
            return reject();
        }
        restored.push_back(identity);
    }

    if ( requested.implicitOwnerExpansion
        && requested.selectedVertices.empty() )
    {
        if ( pendingArchitecturalLightingOwnerSelection_.empty() )
        {
            return reject();
        }
        for ( const std::int32_t owner
            : pendingArchitecturalLightingOwnerSelection_ )
        {
            if ( architecturalLightingSelectedOwnerCacheByThing_.contains(owner) )
            {
                continue;
            }
            if ( !hasCachedModel(owner) )
            {
                return ArchitecturalLightingRecoveryRestoreResult::Deferred;
            }
            return reject();
        }
    }

    const bool changed = architecturalLightingAuthoringMode_ != requested.authoringMode
        || architecturalLightingTargetMode_ != requested.targetMode
        || architecturalLightingSelectedVertices_ != restored
        || architecturalLightingSelectedVerticesImplicitOwnerExpansion_
            != requested.implicitOwnerExpansion;
    architecturalLightingAuthoringMode_ = requested.authoringMode;
    architecturalLightingTargetMode_ = requested.targetMode;
    architecturalLightingSelectedVertices_ = std::move(restored);
    architecturalLightingSelectedVertexSet_.clear();
    architecturalLightingSelectedVertexSet_.insert(
        architecturalLightingSelectedVertices_.begin(),
        architecturalLightingSelectedVertices_.end());
    architecturalLightingSelectedVerticesImplicitOwnerExpansion_ =
        requested.implicitOwnerExpansion;
    pendingArchitecturalLightingRecovery_.reset();
    pendingArchitecturalLightingSelectionReady_ = false;
    pendingArchitecturalLightingOwnerSelection_.clear();
    if ( changed )
    {
        Q_EMIT architecturalLightingVertexSelectionChanged();
        Q_EMIT viewportStateChanged();
    }
    return ArchitecturalLightingRecoveryRestoreResult::Applied;
}


void GlView::ensureTextures(
    const LayerMeshProgressPulse& progressPulse)
{
    if (!glReady_ || !resources_ || !level_)
    {
        return;
    }

    const std::size_t textureCountBefore = textures_.size();

    auto makeThingIconTexture = [&](const std::string_view type) -> TextureInfo
    {
        TextureInfo info{};
        info.missing = false;
        info.alphaBpp = 0;

        const QImage img = render::renderGizmoIconImage(type, thingTypeColors_, thingIconCubeTintColor_);

        // Mirror vertically to match the rest of our OpenGL textures (Qt image coords are top-left).
        info.width = img.width();
        info.height = img.height();
        info.texture = std::make_unique<QOpenGLTexture>(img.mirrored());
        info.texture->setWrapMode(QOpenGLTexture::ClampToEdge);
        info.texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        info.texture->setMagnificationFilter(QOpenGLTexture::Linear);
        info.texture->generateMipMaps();
        return info;
    };

    auto ensureForBatches = [&](const std::vector<TriBatch>& batches)
    {
        for (const TriBatch& batch : batches)
        {
            if (progressPulse)
            {
                progressPulse();
            }
            if (textures_.find(batch.key) != textures_.end())
            {
                continue;
            }

            if (const auto prepared = preparedTextureSources_.find(batch.key);
                prepared != preparedTextureSources_.end()
                && !prepared->second.image.isNull())
            {
                const QImage& image = prepared->second.image;
                TextureInfo info{};
                info.width = image.width();
                info.height = image.height();
                info.alphaBpp = prepared->second.alphaBpp;
                info.solidColor = prepared->second.solidColor;
                info.classicSolidLightRamp =
                    prepared->second.classicSolidLightRamp;
                info.texture = std::make_unique<QOpenGLTexture>(image);
                if (batch.isSprite)
                {
                    info.texture->setWrapMode(QOpenGLTexture::ClampToEdge);
                    info.texture->setMinificationFilter(QOpenGLTexture::Linear);
                    info.texture->setMagnificationFilter(QOpenGLTexture::Linear);
                }
                else
                {
                    info.texture->setWrapMode(QOpenGLTexture::Repeat);
                    info.texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
                    info.texture->setMagnificationFilter(QOpenGLTexture::Linear);
                    info.texture->generateMipMaps();
                }
                info.missing = false;
                textures_.emplace(batch.key, std::move(info));
                continue;
            }

            TextureInfo info{};
            info.missing = true;

            const std::string_view matName = batch.material;
            if (!matName.empty() && matName.rfind(kThingIconMatPrefix, 0) == 0)
            {
                const std::string_view type = matName.substr(kThingIconMatPrefix.size());
                info = makeThingIconTexture(type);
                textures_.emplace(batch.key, std::move(info));
                continue;
            }

            if (batch.material.empty() || batch.material == "-")
            {
                textures_.emplace(batch.key, std::move(info));
                continue;
            }

            watchResourceFile(batch.material, ".mat");

            std::optional<std::vector<std::byte>> matBytes;
            try
            {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
                ++materialResourceReadCount_;
#endif
                matBytes = resources_->tryReadBinaryWithExt(batch.material, ".mat");
            }
            catch (...)
            {
                matBytes.reset();
            }

            if (!matBytes)
            {
                if (missingMats_.emplace(batch.material).second)
                {
                    Q_EMIT resourceWarningMessage(
                        QString("Missing MAT: %1").arg(QString::fromStdString(batch.material)));
                }

                textures_.emplace(batch.key, std::move(info));
                continue;
            }

            try
            {
                const assets::CmpFile* cmp = nullptr;
                const assets::CmpPalette* palette = nullptr;
                if (!batch.colormap.empty() && batch.colormap != "-")
                {
                    const std::string& cmpName = batch.colormap;
                    watchResourceFile(cmpName, ".cmp");
                    auto paletteIt = palettes_.find(cmpName);
                    if (paletteIt != palettes_.end())
                    {
                        palette = &paletteIt->second;
                        if (const auto cmpIt = cmpFiles_.find(cmpName);
                            cmpIt != cmpFiles_.end())
                        {
                            cmp = &cmpIt->second;
                        }
                    }
                    else if (!missingPalettes_.contains(cmpName))
                    {
                        if (const auto cmpBytes = resources_->tryReadBinaryWithExt(cmpName, ".cmp"))
                        {
                            if (const auto paletteOpt =
                                    assets::parseCmpPalette(*cmpBytes))
                            {
                                paletteIt = palettes_.emplace(
                                    cmpName, *paletteOpt).first;
                                palette = &paletteIt->second;
                                if (const auto cmpOpt =
                                        assets::parseCmp(*cmpBytes))
                                {
                                    const auto cmpIt = cmpFiles_.emplace(
                                        cmpName, *cmpOpt).first;
                                    cmp = &cmpIt->second;
                                }
                            }
                            else
                            {
                                if (missingPalettes_.insert(cmpName).second)
                                {
                                    Q_EMIT resourceWarningMessage(
                                        QString("Failed to parse CMP: %1").arg(QString::fromStdString(cmpName)));
                                }
                            }
                        }
                        else
                        {
                            if (missingPalettes_.insert(cmpName).second)
                            {
                                Q_EMIT resourceWarningMessage(
                                    QString("Missing CMP: %1").arg(QString::fromStdString(cmpName)));
                            }
                        }
                    }
                }

                assets::MatFile mat = assets::MatFile::parse(std::move(*matBytes), palette);

                const assets::MatFile::Cel& cel = mat.cel(0);
                info.solidColor = QVector3D(
                    static_cast<float>(cel.solidColor.r) / 255.0f,
                    static_cast<float>(cel.solidColor.g) / 255.0f,
                    static_cast<float>(cel.solidColor.b) / 255.0f);
                if (cmp)
                {
                    const assets::CmpLightRamp ramp =
                        assets::makeCmpLightRamp(*cmp, cel.solidColorIndex);
                    ClassicSolidLightRamp gpuRamp{};
                    for (std::size_t level = 0; level < ramp.size(); ++level)
                    {
                        gpuRamp[level] = QVector3D(
                            static_cast<float>(ramp[level].r) / 255.0f,
                            static_cast<float>(ramp[level].g) / 255.0f,
                            static_cast<float>(ramp[level].b) / 255.0f);
                    }
                    info.classicSolidLightRamp = std::move(gpuRamp);
                }
                if (cel.mipmaps.empty())
                {
                    textures_.emplace(batch.key, std::move(info));
                    continue;
                }

                const sed::Image8& img = cel.mipmaps[0];
                if (img.empty())
                {
                    textures_.emplace(batch.key, std::move(info));
                    continue;
                }

                const int w = static_cast<int>(img.width);
                const int h = static_cast<int>(img.height);
                const int bpl = w * static_cast<int>(img.bytesPerPixel());
                const QImage::Format format = (img.format == sed::PixelFormat::Rgb8) ? QImage::Format_RGB888 : QImage::Format_RGBA8888;

                QImage image(img.pixels.data(), w, h, bpl, format);
                QImage copy = image.copy();
                if (kFlipV)
                {
                    copy = copy.mirrored();
                }

                // Alpha classification:
                // 1) Start from MAT header alpha bits.
                // 2) For indexed mats (and explicit 1-bit header alpha), refine with decoded pixels so
                //    soft-alpha sprites are treated as blended translucency instead of cutout alpha-test.
                // 3) Keep a fallback for headers that miss alpha flags entirely.
                const std::int32_t inferredAlphaBpp = render::inferTextureAlphaBpp(copy);
                {
                    const sed::ColorInfo& ci = mat.colorInfo();
                    const bool indexed =
                        (ci.mode == sed::ColorMode::Indexed)
                        || (ci.mode == sed::ColorMode::IndexedCols);
                    const std::int32_t headerAlphaBpp =
                        (ci.alphaBPP <= 0) ? 0 : static_cast<std::int32_t>(ci.alphaBPP);

                    info.alphaBpp = headerAlphaBpp;

                    if (indexed || headerAlphaBpp == 1)
                    {
                        if (inferredAlphaBpp > 0)
                        {
                            info.alphaBpp = inferredAlphaBpp;
                        }
                    }
                    else if (info.alphaBpp == 0)
                    {
                        info.alphaBpp = inferredAlphaBpp;
                    }
                }

                info.width = img.width;
                info.height = img.height;
                info.texture = std::make_unique<QOpenGLTexture>(copy);
                if (batch.isSprite)
                {
                    // Billboards are alpha-heavy and screen-facing; avoid mip/edge sampling instability.
                    info.texture->setWrapMode(QOpenGLTexture::ClampToEdge);
                    info.texture->setMinificationFilter(QOpenGLTexture::Linear);
                    info.texture->setMagnificationFilter(QOpenGLTexture::Linear);
                }
                else
                {
                    info.texture->setWrapMode(QOpenGLTexture::Repeat);
                    info.texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
                    info.texture->setMagnificationFilter(QOpenGLTexture::Linear);
                    info.texture->generateMipMaps();
                }
                info.missing = false;
            }
            catch (const std::exception& e)
            {
                if (missingMats_.emplace(batch.material).second)
                {
                    Q_EMIT resourceErrorMessage(QString("Failed to load MAT %1: %2")
                            .arg(QString::fromStdString(batch.material))
                            .arg(QString::fromUtf8(e.what())));
                }
                info.missing = true;
            }
            catch (...)
            {
                if (missingMats_.emplace(batch.material).second)
                {
                    Q_EMIT resourceErrorMessage(
                        QString("Failed to load MAT %1").arg(QString::fromStdString(batch.material)));
                }
                info.missing = true;
            }

            textures_.emplace(batch.key, std::move(info));
        }
    };

    ensureForBatches(triBatches_);
    ensureForBatches(spriteTriBatches_);
    if (textures_.size() != textureCountBefore)
    {
        alphaAdjoinWireSupportsDirty_ = true;
    }
    if (progressPulse)
    {
        progressPulse();
    }
}

bool GlView::updateIsolatedSpritePreviewGeometry(const assets::SprFile& sprite, const ThingBox& bounds)
{
    // This fixed-topology path belongs only to the authoring pane's own
    // one-Thing Level. Normal viewport transforms never enter it.
    if ( !property("spr.preview.isolated_resource").toBool() || !level_
        || level_->things.size() != 1u || renderPipeline_ != RenderPipeline::GamePreview
        || level_->things.front().x != 0.0 || level_->things.front().y != 0.0
        || level_->things.front().z != 0.0 || level_->things.front().pch != 0.0
        || level_->things.front().yaw != 0.0 || level_->things.front().rol != 0.0
        || spriteInstances_.size() != 1u || spriteTriVertices_.size() != 6u
        || spriteTriBatches_.size() != 1u || spriteTriRangeByInstanceIndex_.size() != 1u
        || spriteTriBatches_.front().firstVertex != 0 || spriteTriBatches_.front().vertexCount != 6
        || spriteTriRangeByInstanceIndex_.front().first != 0
        || spriteTriRangeByInstanceIndex_.front().count != 6
        || spriteInstanceBounds_.size() != 1u || spriteThingBounds_.size() != 1u
        || spriteBatchExtremeOwners_.size() != 1u || spriteBatchExtremeOwnersValid_.size() != 1u
        || !glReady_ || !isValid() || !context() || !context()->isValid()
        || !spriteTriVbo_ || !spriteTriVbo_->isCreated()
        || spriteTriVboAllocatedBytes_ < static_cast<int>(6u * sizeof(TriVertex))
        || sprite.width <= 0.0f || sprite.height <= 0.0f )
    {
        return false;
    }
    const std::string name = property("spr.preview.resource_name").toString().toStdString();
    const auto cached = sprites_.find(name);
    if ( cached == sprites_.end() || cached->second.material != sprite.material
        || cached->second.type != sprite.type
        || cached->second.lightMode != sprite.lightMode || cached->second.textureMode != sprite.textureMode
        || spriteInstances_.front().type != originalSpriteRenderType(level_->kind, sprite.type) )
    {
        return false;
    }
    for ( const float component : { sprite.width, sprite.height, sprite.offsetXYZ[0], sprite.offsetXYZ[1],
              sprite.offsetXYZ[2], sprite.extraLightRGBA[0], sprite.extraLightRGBA[1],
              sprite.extraLightRGBA[2], sprite.extraLightRGBA[3] } )
    {
        if ( !std::isfinite(component) )
        {
            return false;
        }
    }

    SpriteInstance replacement = spriteInstances_.front();
    if ( level_->kind != ProjectType::IJIM )
    {
        // The isolated classic preview keeps a fixed two-triangle card, but
        // the fragment shader still uses the authored face geometry to choose
        // MAT texturing versus the material's palette-derived Solid color.
        replacement.pickGeo = static_cast<float>(std::min(
            sprite.geometryMode, static_cast<int>(TGeoMode::Texture)));
    }
    replacement.width = sprite.width;
    replacement.height = sprite.height;
    replacement.offsetXYZ = QVector3D(sprite.offsetXYZ[0], sprite.offsetXYZ[1], sprite.offsetXYZ[2]);
    replacement.extraLightRGBA = QVector4D(sprite.extraLightRGBA[0], sprite.extraLightRGBA[1],
        sprite.extraLightRGBA[2], sprite.extraLightRGBA[3]);
    std::array<TriVertex, 6> vertices{};
    if ( !buildSpriteInstanceVertices(replacement, vertices) )
    {
        return false;
    }
    ThingBounds cardBounds{};
    for ( const TriVertex& vertex : vertices )
    {
        if ( !std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)
            || !std::isfinite(vertex.r) || !std::isfinite(vertex.g) || !std::isfinite(vertex.b)
            || !std::isfinite(vertex.a) )
        {
            return false;
        }
        render::includePoint(cardBounds, vertex.x, vertex.y, vertex.z);
    }

    const int pickId = render::fast_update::pickIdKey(replacement.pickId);
    const auto overlay = thingVisualBoundsOverlayRangesByPickId_.find(pickId);
    const bool hasOverlay = overlay != thingVisualBoundsOverlayRangesByPickId_.end();
    DrawRange localLines{};
    int globalFirst = 0;
    int lineOffsetBytes = 0;
    constexpr int lineBytes = static_cast<int>(24u * sizeof(LineVertex));
    std::array<LineVertex, 24> lines{};
    if ( hasOverlay )
    {
        if ( overlay->second.size() != 1u || overlay->second.front().first < 0
            || overlay->second.front().count != 24 || thingOverlayLineFirstVertex_ < 0 )
        {
            return false;
        }
        localLines = overlay->second.front();
        if ( static_cast<std::size_t>(localLines.first) + lines.size()
                > thingMeshCache_.overlayLineVertices.size()
            || localLines.first > std::numeric_limits<int>::max() - thingOverlayLineFirstVertex_ )
        {
            return false;
        }
        globalFirst = thingOverlayLineFirstVertex_ + localLines.first;
        const std::int64_t byteOffset = static_cast<std::int64_t>(globalFirst)
            * static_cast<std::int64_t>(sizeof(LineVertex));
        if ( static_cast<std::size_t>(globalFirst) + lines.size() > lineVertices_.size()
            || !lineVbo_ || !lineVbo_->isCreated() || byteOffset > std::numeric_limits<int>::max()
            || byteOffset + lineBytes > lineVboAllocatedBytes_ )
        {
            return false;
        }
        lineOffsetBytes = static_cast<int>(byteOffset);
        const render::thing_overlay::Bounds3d wireBounds{
            TVector{ bounds.x1, bounds.y1, bounds.z1 }, TVector{ bounds.x2, bounds.y2, bounds.z2 } };
        const auto points = render::thing_overlay::makeBoundsWirePoints(wireBounds, TMatrix43::identity());
        for ( std::size_t i = 0u; i < points.size(); ++i )
        {
            lines[i] = thingMeshCache_.overlayLineVertices[static_cast<std::size_t>(localLines.first) + i];
            lines[i].x = static_cast<float>(points[i].x);
            lines[i].y = static_cast<float>(points[i].y);
            lines[i].z = static_cast<float>(points[i].z);
            if ( !std::isfinite(lines[i].x) || !std::isfinite(lines[i].y) || !std::isfinite(lines[i].z) )
            {
                return false;
            }
        }
    }
    else if ( showThingVisualBounds_ || thingOverlayGeometryPrepared_ )
    {
        return false;
    }

    makeCurrent();
    struct CurrentContextGuard
    {
        GlView& view;
        ~CurrentContextGuard()
        {
            view.doneCurrent();
        }
    } guard{ *this };
    if ( QOpenGLContext::currentContext() != context() )
    {
        return false;
    }
    if ( hasOverlay )
    {
        if ( !lineVbo_->bind() )
        {
            return false;
        }
        lineVbo_->write(lineOffsetBytes, lines.data(), lineBytes);
        lineVbo_->release();
    }
    if ( !spriteTriVbo_->bind() )
    {
        return false;
    }
    spriteTriVbo_->write(0, vertices.data(), static_cast<int>(vertices.size() * sizeof(TriVertex)));
    spriteTriVbo_->release();
    // Every finite coordinate and CPU/GPU range is valid before publication.
    cached->second.width = sprite.width;
    cached->second.height = sprite.height;
    cached->second.geometryMode = sprite.geometryMode;
    cached->second.offsetXYZ = sprite.offsetXYZ;
    cached->second.extraLightRGBA = sprite.extraLightRGBA;
    spriteInstances_.front() = std::move(replacement);
    std::copy(vertices.begin(), vertices.end(), spriteTriVertices_.begin());
    spriteInstanceBounds_.front() = cardBounds;
    spriteThingBounds_.front() = cardBounds;
    ++spriteInstanceVertexGenerationCount_;
    TriBatch& batch = spriteTriBatches_.front();
    batch.boundsMin = cardBounds.min;
    batch.boundsMax = cardBounds.max;
    batch.boundsCenter = cardBounds.min * 0.5f + cardBounds.max * 0.5f;
    batch.minAlpha = sprite.extraLightRGBA[3];
    spriteBatchExtremeOwners_.front().fill(0u);
    spriteBatchExtremeOwnersValid_.front() = 1u;
    if ( thingRenderState_.size() == 1u )
    {
        Thing updated = level_->things.front();
        updated.bbox = bounds;
        thingRenderState_.front().signature = thingGeometrySignature(updated);
    }
    if ( hasOverlay )
    {
        std::copy(lines.begin(), lines.end(), thingMeshCache_.overlayLineVertices.begin() + localLines.first);
        std::copy(lines.begin(), lines.end(), lineVertices_.begin() + globalFirst);
    }
    spriteTrianglesDirty_ = false;
    levelPeelCacheValid_ = false;
    thingPeelCacheValid_ = false;
    update();
    return true;
}

bool GlView::buildSpriteInstanceVertices(
    const SpriteInstance& inst,
    std::array<TriVertex, 6>& vertices) const
{
    const float widthHalf = inst.width * 0.5f;
    const float heightHalf = inst.height * 0.5f;
    QVector3D center = inst.position;
    QVector3D p0{};
    QVector3D p1{};
    QVector3D p2{};
    QVector3D p3{};

    if (inst.type == 0)
    {
        QVector3D right{};
        QVector3D up{};
        QVector3D forward{};
        const bool billboard = render::fast_update::usesOriginalSpriteCameraFacingFrame(
            inst.type,
            renderPipeline_ == RenderPipeline::GamePreview);
        if (billboard)
        {
            const SpriteBasis cameraBasis =
                makeCameraFacingSpriteBasis(camera_.orbit);
            right = cameraBasis.right;
            up = cameraBasis.up;
            forward = cameraBasis.forward;
        }
        else
        {
            right = inst.basisRight;
            forward = inst.basisForward;
            if (!normalizeInPlace(right)) right = QVector3D(1.0f, 0.0f, 0.0f);
            if (!normalizeInPlace(forward)) forward = QVector3D(0.0f, 1.0f, 0.0f);
            up = inst.basisUp;
            if (!normalizeInPlace(up))
            {
                up = QVector3D::crossProduct(right, forward);
                if (!normalizeInPlace(up)) up = QVector3D(0.0f, 0.0f, 1.0f);
            }
        }

        if (inst.rollDeg != 0.0f)
        {
            const float rollRad = DegToRad(inst.rollDeg);
            right = rotateAroundAxis(right, forward, rollRad);
            up = rotateAroundAxis(up, forward, rollRad);
        }

        center = inst.position
            + right * inst.offsetXYZ.x()
            + forward * inst.offsetXYZ.y()
            + up * inst.offsetXYZ.z();
        const QVector3D r = right * widthHalf;
        const QVector3D u = up * heightHalf;
        p0 = center - r - u;
        p1 = center + r - u;
        p2 = center + r + u;
        p3 = center - r + u;
    }
    else if (inst.type == 2)
    {
        const render::fast_update::OriginalSpriteType2Frame frame =
            render::fast_update::makeOriginalSpriteType2Frame(inst.basisForward);
        const QVector3D rightHalf = frame.right * widthHalf;
        const QVector3D upHalf = frame.up * heightHalf;
        QVector3D v0 = -rightHalf - upHalf;
        QVector3D v1 = rightHalf - upHalf;
        QVector3D v2 = rightHalf + upHalf;
        QVector3D v3 = -rightHalf + upHalf;
        if (inst.rollDeg != 0.0f)
        {
            const float rollRad = DegToRad(inst.rollDeg);
            v0 = rotateAroundAxis(v0, frame.forward, rollRad);
            v1 = rotateAroundAxis(v1, frame.forward, rollRad);
            v2 = rotateAroundAxis(v2, frame.forward, rollRad);
            v3 = rotateAroundAxis(v3, frame.forward, rollRad);
        }
        p0 = center + v0;
        p1 = center + v1;
        p2 = center + v2;
        p3 = center + v3;
    }
    else
    {
        return false;
    }

    std::int32_t lightSector = inst.sectorIndex;
    if (transformPreviewActive_)
    {
        const int pid = render::fast_update::pickIdKey(inst.pickId);
        if (pid > 0 && static_cast<std::size_t>(pid) < pickObjects_.size())
        {
            const PickObject& obj = pickObjects_[static_cast<std::size_t>(pid)];
            if (obj.kind == PickKind::Thing)
            {
                if (const auto it = previewThingSectors_.find(obj.index);
                    it != previewThingSectors_.end())
                {
                    lightSector = it->second;
                }
            }
        }
    }

    const QVector3D editorSectorTintScale = inst.invalidPlacementPreview
        ? QVector3D(1.0f, 1.0f, 1.0f)
        : render::classicEditorSectorTintScale(level_, lightSector);

    QVector3D litRgb(1.0f, 1.0f, 1.0f);
    if (inst.invalidPlacementPreview)
    {
        litRgb = QVector3D(1.0f, 0.16f, 0.12f);
    }
    else if (level_ && inst.lightMode > static_cast<std::int32_t>(TLightMode::None))
    {
        ColorF ambientLight = calcThingSectorLighting(level_, lightSector).ambientLight;
        if constexpr (render::lighting::kRuntimeDynamicLightPreviewEnabled)
        {
            render::lighting::RuntimeThingLight regularSelf{};
            regularSelf.source = render::lighting::RuntimeThingLightSource::Regular;
            regularSelf.classification = (level_->kind == ProjectType::IJIM)
                ? render::lighting::ThingLightClass::Flat
                : render::lighting::ThingLightClass::Dynamic;
            regularSelf.color = inst.regularSelfLight;
            ambientLight = render::lighting::applyRuntimeThingSelfLight(
                level_->kind,
                ambientLight,
                regularSelf,
                thingLightRenderingEnabled_);
        }
        const ColorF extraLight{
            inst.extraLightRGBA.x(), inst.extraLightRGBA.y(),
            inst.extraLightRGBA.z(), inst.extraLightRGBA.w() };
        // IJIM rdSprite_Draw caps sprites at Diffuse before rdCache applies
        // flat RGB extra/camera light. Sprites have no Gouraud vertex input.
        const int spriteLightMode = level_->kind == ProjectType::IJIM
            ? std::min(inst.lightMode, static_cast<int>(TLightMode::Diffuse))
            : inst.lightMode;
        const ColorF lit = polyCalcLitColor(
            ColorBlack,
            spriteLightMode,
            ambientLight,
            extraLight,
            level_->kind,
            static_cast<TLightMode>(static_cast<std::int32_t>(lightingMode_)));
        litRgb = QVector3D(lit.r, lit.g, lit.b);
    }

    auto makeVertex = [&](const QVector3D& p, const float u, const float v)
    {
        TriVertex tv{};
        tv.x = p.x();
        tv.y = p.y();
        tv.z = p.z();
        tv.u = u;
        tv.v = v;
        tv.r = litRgb.x();
        tv.g = litRgb.y();
        tv.b = litRgb.z();
        tv.a = inst.extraLightRGBA.w();
        tv.pickId = inst.pickId;
        tv.pickGeo = inst.pickGeo;
        tv.sky = 0.0f;
        tv.editorSectorTintR = editorSectorTintScale.x();
        tv.editorSectorTintG = editorSectorTintScale.y();
        tv.editorSectorTintB = editorSectorTintScale.z();
        return tv;
    };
    vertices = {
        makeVertex(p0, 0.0f, 0.0f), makeVertex(p1, 1.0f, 0.0f),
        makeVertex(p2, 1.0f, 1.0f), makeVertex(p0, 0.0f, 0.0f),
        makeVertex(p2, 1.0f, 1.0f), makeVertex(p3, 0.0f, 1.0f),
    };
    return true;
}

void GlView::refitSpriteBatch(
    const std::size_t batchIndex,
    const std::vector<std::size_t>* changedInstanceIndices)
{
    if (batchIndex >= spriteTriBatches_.size()) return;
    TriBatch& batch = spriteTriBatches_[batchIndex];
    if (batchIndex >= spriteInstanceIndicesByBatch_.size()
        || batchIndex >= spriteBatchExtremeOwners_.size()
        || batchIndex >= spriteBatchExtremeOwnersValid_.size()
        || spriteInstanceBounds_.size() != spriteInstances_.size())
    {
        spriteBatchExtremeOwnersValid_.assign(spriteTriBatches_.size(), 0u);
        return;
    }

    bool fullRefit = !changedInstanceIndices
        || spriteBatchExtremeOwnersValid_[batchIndex] == 0u;
    if (!fullRefit)
    {
        const auto& owners = spriteBatchExtremeOwners_[batchIndex];
        fullRefit = std::any_of(
            changedInstanceIndices->begin(), changedInstanceIndices->end(),
            [&](const std::size_t changedIndex)
            {
                return std::find(owners.begin(), owners.end(), changedIndex) != owners.end();
            });
    }

    if (!fullRefit)
    {
        QVector3D bmin = batch.boundsMin;
        QVector3D bmax = batch.boundsMax;
        auto& owners = spriteBatchExtremeOwners_[batchIndex];
        for (const std::size_t spriteIndex : *changedInstanceIndices)
        {
            if (spriteIndex >= spriteInstanceBounds_.size()) continue;
            const ThingBounds& bounds = spriteInstanceBounds_[spriteIndex];
            if (!bounds.valid) continue;
            ++spriteBatchBoundsVisitCount_;
            if (bounds.min.x() < bmin.x()) { bmin.setX(bounds.min.x()); owners[0] = spriteIndex; }
            if (bounds.min.y() < bmin.y()) { bmin.setY(bounds.min.y()); owners[1] = spriteIndex; }
            if (bounds.min.z() < bmin.z()) { bmin.setZ(bounds.min.z()); owners[2] = spriteIndex; }
            if (bounds.max.x() > bmax.x()) { bmax.setX(bounds.max.x()); owners[3] = spriteIndex; }
            if (bounds.max.y() > bmax.y()) { bmax.setY(bounds.max.y()); owners[4] = spriteIndex; }
            if (bounds.max.z() > bmax.z()) { bmax.setZ(bounds.max.z()); owners[5] = spriteIndex; }
        }
        batch.boundsMin = bmin;
        batch.boundsMax = bmax;
        batch.boundsCenter = (bmin + bmax) * 0.5f;
        return;
    }

    QVector3D bmin{};
    QVector3D bmax{};
    std::array<std::size_t, 6> owners{};
    bool haveAny = false;
    for (const std::size_t spriteIndex : spriteInstanceIndicesByBatch_[batchIndex])
    {
        if (spriteIndex >= spriteInstanceBounds_.size()) continue;
        const ThingBounds& bounds = spriteInstanceBounds_[spriteIndex];
        if (!bounds.valid) continue;
        ++spriteBatchBoundsVisitCount_;
        if (!haveAny)
        {
            bmin = bounds.min;
            bmax = bounds.max;
            owners.fill(spriteIndex);
            haveAny = true;
            continue;
        }
        if (bounds.min.x() < bmin.x()) { bmin.setX(bounds.min.x()); owners[0] = spriteIndex; }
        if (bounds.min.y() < bmin.y()) { bmin.setY(bounds.min.y()); owners[1] = spriteIndex; }
        if (bounds.min.z() < bmin.z()) { bmin.setZ(bounds.min.z()); owners[2] = spriteIndex; }
        if (bounds.max.x() > bmax.x()) { bmax.setX(bounds.max.x()); owners[3] = spriteIndex; }
        if (bounds.max.y() > bmax.y()) { bmax.setY(bounds.max.y()); owners[4] = spriteIndex; }
        if (bounds.max.z() > bmax.z()) { bmax.setZ(bounds.max.z()); owners[5] = spriteIndex; }
    }
    if (!haveAny)
    {
        batch.boundsMin = QVector3D{};
        batch.boundsMax = QVector3D{};
        batch.boundsCenter = QVector3D{};
        batch.minAlpha = 1.0f;
        spriteBatchExtremeOwnersValid_[batchIndex] = 0u;
        return;
    }
    batch.boundsMin = bmin;
    batch.boundsMax = bmax;
    batch.boundsCenter = (bmin + bmax) * 0.5f;
    spriteBatchExtremeOwners_[batchIndex] = owners;
    spriteBatchExtremeOwnersValid_[batchIndex] = 1u;
}

void GlView::rebuildSpriteVertices(
    const LayerMeshProgressPulse& progressPulse)
{
    std::size_t progressWorkUnits = 0;
    const auto reportProgress = [&progressPulse, &progressWorkUnits]()
    {
        if (progressPulse && ((progressWorkUnits++ & 31u) == 0u))
        {
            progressPulse();
        }
    };
    reportProgress();
    std::vector<TriVertex> previousVertices{};
    std::unordered_map<int, std::vector<DrawRange>> previousRangesByPickId{};
    if (spriteReuseExistingVertices_)
    {
        previousVertices = std::move(spriteTriVertices_);
        previousRangesByPickId = std::move(spriteTriRangesByPickId_);
    }
    spriteReuseExistingVertices_ = false;
    spriteTriVertices_.clear();
    spriteTriRangesByPickId_.clear();
    spriteTriRangeByInstanceIndex_.clear();
    spriteTriBatchByInstanceIndex_.clear();
    spriteInstanceBounds_.clear();
    spriteInstanceIndicesByBatch_.clear();
    spriteBatchExtremeOwners_.clear();
    spriteBatchExtremeOwnersValid_.clear();
    spriteInstanceIndicesByPickId_.clear();
    placementPreviewSpriteIndices_.clear();
    placementPreviewSpriteIndices_.resize(placementPreviewRenderedThings_.size());
    spriteTriVertexCount_ = 0;
    spriteHasType0Billboards_ = false;

    for (TriBatch& batch : spriteTriBatches_)
    {
        batch.firstVertex = 0;
        batch.vertexCount = 0;
        batch.boundsMin = QVector3D(0.0f, 0.0f, 0.0f);
        batch.boundsMax = QVector3D(0.0f, 0.0f, 0.0f);
        batch.boundsCenter = QVector3D(0.0f, 0.0f, 0.0f);
        batch.minAlpha = 1.0f;
    }

    if (!level_ || spriteInstances_.empty())
    {
        spriteThingBounds_.clear();
        spriteTrianglesDirty_ = false;
        return;
    }

    std::unordered_map<std::string, std::size_t> batchByKey{};
    batchByKey.reserve(spriteTriBatches_.size());
    for (std::size_t i = 0; i < spriteTriBatches_.size(); ++i)
    {
        batchByKey.emplace(spriteTriBatches_[i].key, i);
    }

    // Sort instances by batch key so that all instances sharing the same material
    // form a contiguous range in the vertex buffer.  Without this, interleaved
    // batches produce overlapping firstVertex/vertexCount ranges, causing some
    // sprites to be drawn with a wrong material or not drawn at all.
    std::stable_sort(spriteInstances_.begin(), spriteInstances_.end(),
        [](const SpriteInstance& a, const SpriteInstance& b) {
            return a.key < b.key;
        });
    if (progressPulse)
    {
        progressPulse();
    }

    spriteTriRangeByInstanceIndex_.assign(spriteInstances_.size(), DrawRange{});
    spriteTriBatchByInstanceIndex_.assign(
        spriteInstances_.size(), std::numeric_limits<std::size_t>::max());
    spriteInstanceBounds_.assign(spriteInstances_.size(), ThingBounds{});
    spriteInstanceIndicesByBatch_.resize(spriteTriBatches_.size());
    spriteBatchExtremeOwners_.resize(spriteTriBatches_.size());
    spriteBatchExtremeOwnersValid_.assign(spriteTriBatches_.size(), 0u);

    for (std::size_t spriteIndex = 0; spriteIndex < spriteInstances_.size(); ++spriteIndex)
    {
        reportProgress();
        const std::int32_t previewIndex = spriteInstances_[spriteIndex].placementPreviewIndex;
        if (previewIndex >= 0
            && static_cast<std::size_t>(previewIndex) < placementPreviewSpriteIndices_.size())
        {
            placementPreviewSpriteIndices_[static_cast<std::size_t>(previewIndex)].push_back(spriteIndex);
        }

        const int pickId = render::fast_update::pickIdKey(
            spriteInstances_[spriteIndex].pickId);
        if (pickId > 0)
        {
            spriteInstanceIndicesByPickId_[pickId].push_back(spriteIndex);
        }
    }

    for (std::size_t spriteIndex = 0; spriteIndex < spriteInstances_.size(); ++spriteIndex)
    {
        reportProgress();
        const SpriteInstance& inst = spriteInstances_[spriteIndex];
        const auto it = batchByKey.find(inst.key);
        if (it == batchByKey.end())
        {
            continue;
        }

        TriBatch& batch = spriteTriBatches_[it->second];
        if (batch.vertexCount == 0)
        {
            batch.firstVertex = spriteTriVertexCount_;
        }

        std::array<TriVertex, 6> vertices{};
        bool reused = false;
        const int persistentPickId = render::fast_update::pickIdKey(inst.pickId);
        if (inst.placementPreviewIndex < 0 && persistentPickId > 0)
        {
            if (const auto previousIt = previousRangesByPickId.find(persistentPickId);
                previousIt != previousRangesByPickId.end()
                && previousIt->second.size() == 1u)
            {
                const DrawRange oldRange = previousIt->second.front();
                if (oldRange.first >= 0 && oldRange.count == 6
                    && oldRange.first + oldRange.count
                        <= static_cast<int>(previousVertices.size()))
                {
                    std::copy_n(
                        previousVertices.begin() + oldRange.first,
                        vertices.size(),
                        vertices.begin());
                    reused = true;
                }
            }
        }
        if (!reused && !buildSpriteInstanceVertices(inst, vertices))
        {
            continue;
        }
        if (!reused)
        {
            ++spriteInstanceVertexGenerationCount_;
        }
        const int first = spriteTriVertexCount_;
        spriteTriVertices_.insert(
            spriteTriVertices_.end(), vertices.begin(), vertices.end());
        spriteTriVertexCount_ += static_cast<int>(vertices.size());
        spriteTriRangeByInstanceIndex_[spriteIndex] = DrawRange{
            first, static_cast<int>(vertices.size()) };
        spriteTriBatchByInstanceIndex_[spriteIndex] = it->second;
        spriteInstanceIndicesByBatch_[it->second].push_back(spriteIndex);
        for (const TriVertex& vertex : vertices)
        {
            render::includePoint(
                spriteInstanceBounds_[spriteIndex], vertex.x, vertex.y, vertex.z);
        }
        if (inst.type == 0) spriteHasType0Billboards_ = true;

        batch.vertexCount += 6;
    }

    // Precompute exact per-batch extrema for translucent sorting. Targeted
    // transforms can then refit an interior owner in O(changed sprites), with
    // a batch-local rescan only when a current extreme owner moves.
    for (std::size_t batchIndex = 0; batchIndex < spriteTriBatches_.size(); ++batchIndex)
    {
        reportProgress();
        TriBatch& batch = spriteTriBatches_[batchIndex];
        if (batch.vertexCount <= 0)
        {
            batch.boundsMin = QVector3D(0.0f, 0.0f, 0.0f);
            batch.boundsMax = QVector3D(0.0f, 0.0f, 0.0f);
            batch.boundsCenter = QVector3D(0.0f, 0.0f, 0.0f);
            batch.minAlpha = 1.0f;
            continue;
        }

        const int first = std::max(0, batch.firstVertex);
        const int end = std::min(first + batch.vertexCount, static_cast<int>(spriteTriVertices_.size()));
        if (end <= first)
        {
            batch.boundsMin = QVector3D(0.0f, 0.0f, 0.0f);
            batch.boundsMax = QVector3D(0.0f, 0.0f, 0.0f);
            batch.boundsCenter = QVector3D(0.0f, 0.0f, 0.0f);
            batch.minAlpha = 1.0f;
            continue;
        }

        float minA = 1.0f;
        for (int i = first; i < end; ++i)
        {
            const TriVertex& v = spriteTriVertices_[static_cast<std::size_t>(i)];
            minA = std::min(minA, v.a);
        }
        batch.minAlpha = minA;
        refitSpriteBatch(batchIndex);
    }

    spriteThingBounds_.assign(level_->things.size(), ThingBounds{});
    for (std::size_t vertexIndex = 0; vertexIndex < spriteTriVertices_.size(); ++vertexIndex)
    {
        reportProgress();
        const TriVertex& vertex = spriteTriVertices_[vertexIndex];
        const int pickId = render::pickIdKey(vertex.pickId);
        if (pickId <= 0 || static_cast<std::size_t>(pickId) >= pickObjects_.size())
        {
            continue;
        }
        const PickObject& object = pickObjects_[static_cast<std::size_t>(pickId)];
        if (object.kind != PickKind::Thing
            || object.index < 0
            || static_cast<std::size_t>(object.index) >= spriteThingBounds_.size())
        {
            continue;
        }
        render::includePoint(
            spriteThingBounds_[static_cast<std::size_t>(object.index)],
            vertex.x,
            vertex.y,
            vertex.z);
        render::appendOrExtendDrawRange(
            spriteTriRangesByPickId_[pickId],
            static_cast<int>(vertexIndex),
            1);
    }

    spriteTrianglesDirty_ = false;
    if (progressPulse)
    {
        progressPulse();
    }
}

void GlView::rebuildMeshes(
    const bool resetPickIds,
    const GlView::MeshRebuildKind kind,
    const LayerMeshProgressPulse& progressPulse)
{
    // A preview patch lives only in the composed triangle buffer. Restore it
    // before rebuilding any cache so no stale sparse color is copied into a
    // new model publication. The replacement is reapplied after the new
    // build-to-global ranges are known.
    restoreArchitecturalLightingTrianglePreview();
    architecturalLightingTriangleDirtyIndices_.clear();
    architecturalLightingTriangleDirtyIndexSet_.clear();
    ++meshRebuildCount_;
    std::size_t progressWorkUnits = 0;
    const auto pulseProgress = [&progressPulse]()
    {
        if (progressPulse)
        {
            progressPulse();
        }
    };
    const auto reportProgress = [
        &progressPulse, &pulseProgress, &progressWorkUnits]()
    {
        // This helper sits in per-vertex loops. Keep the ordinary no-callback
        // rebuild path to one predictable branch without touching the counter.
        if (progressPulse && ((progressWorkUnits++ & 31u) == 0u))
        {
            pulseProgress();
        }
    };
    reportProgress();
    MeshRebuildKind rebuildKind = kind;
    if (resetPickIds)
    {
        // When pick IDs are reset, all cached mesh parts become invalid because IDs change.
        rebuildKind = MeshRebuildKind::All;
    }

    // If a partial rebuild is requested but the other part is not cached, fall back to a full rebuild.
    if (rebuildKind == MeshRebuildKind::LevelOnly && !thingMeshCache_.valid)
    {
        rebuildKind = MeshRebuildKind::All;
    }
    if (rebuildKind == MeshRebuildKind::ThingsOnly && !levelMeshCache_.valid)
    {
        rebuildKind = MeshRebuildKind::All;
    }

    const bool buildLevel = (rebuildKind == MeshRebuildKind::All || rebuildKind == MeshRebuildKind::LevelOnly);
    const bool buildThings = (rebuildKind == MeshRebuildKind::All || rebuildKind == MeshRebuildKind::ThingsOnly);

    const MeshPartCache* previousLevelCache = levelMeshCache_.valid
        ? &levelMeshCache_
        : nullptr;
    std::size_t preservedLevelTriBatchCount = 0;
    std::size_t expectedLevelTriVertexCount = 0;
    bool expectedLevelTriVertexCountValid = previousLevelCache != nullptr;
    if (previousLevelCache)
    {
        for (const TriBuild& build : previousLevelCache->triBuilds)
        {
            if (build.vertices.size()
                > std::numeric_limits<std::size_t>::max() - expectedLevelTriVertexCount)
            {
                expectedLevelTriVertexCountValid = false;
                break;
            }
            expectedLevelTriVertexCount += build.vertices.size();
        }
    }
    while (preservedLevelTriBatchCount < triBatches_.size()
        && triBatches_[preservedLevelTriBatchCount].kind == TriKind::Level)
    {
        ++preservedLevelTriBatchCount;
    }

    const bool composedTriCountsValid =
        levelTriVertexCount_ >= 0 && thingTriVertexCount_ >= 0
        && static_cast<std::size_t>(levelTriVertexCount_)
            <= std::numeric_limits<std::size_t>::max()
                - static_cast<std::size_t>(thingTriVertexCount_)
        && triVertices_.size()
            == static_cast<std::size_t>(levelTriVertexCount_)
                + static_cast<std::size_t>(thingTriVertexCount_);
    const bool composedLineCountsValid =
        wireLineVertexCount_ >= 0 && thingLineVertexCount_ >= 0
        && thingOverlayLineVertexCount_ >= 0
        && (wireLineVertexCount_ % 2) == 0
        && (thingLineVertexCount_ % 2) == 0
        && (thingOverlayLineVertexCount_ % 2) == 0
        && (lineVertices_.size() % 2u) == 0u
        && static_cast<std::size_t>(wireLineVertexCount_)
            <= std::numeric_limits<std::size_t>::max()
                - static_cast<std::size_t>(thingLineVertexCount_)
        && static_cast<std::size_t>(wireLineVertexCount_)
                + static_cast<std::size_t>(thingLineVertexCount_)
            <= std::numeric_limits<std::size_t>::max()
                - static_cast<std::size_t>(thingOverlayLineVertexCount_)
        && lineVertices_.size()
            == static_cast<std::size_t>(wireLineVertexCount_)
                + static_cast<std::size_t>(thingLineVertexCount_)
                + static_cast<std::size_t>(thingOverlayLineVertexCount_);
    const bool composedPointCountsValid =
        levelPointVertexCount_ >= 0 && thingPointVertexCount_ >= 0
        && (pointPositions_.size() % 3u) == 0u
        && static_cast<std::size_t>(levelPointVertexCount_)
            <= std::numeric_limits<std::size_t>::max()
                - static_cast<std::size_t>(thingPointVertexCount_)
        && static_cast<std::size_t>(levelPointVertexCount_)
                + static_cast<std::size_t>(thingPointVertexCount_)
            <= std::numeric_limits<std::size_t>::max() / 3u
        && pointPositions_.size()
            == (static_cast<std::size_t>(levelPointVertexCount_)
                + static_cast<std::size_t>(thingPointVertexCount_)) * 3u;

    const bool preserveStaticComposition =
        rebuildKind == MeshRebuildKind::ThingsOnly
        && !resetPickIds
        && previousLevelCache
        && expectedLevelTriVertexCountValid
        && expectedLevelTriVertexCount
            <= static_cast<std::size_t>(std::numeric_limits<int>::max())
        && previousLevelCache->lineVertices.size()
            <= static_cast<std::size_t>(std::numeric_limits<int>::max())
        && (previousLevelCache->lineVertices.size() % 2u) == 0u
        && (previousLevelCache->pointPositions.size() % 3u) == 0u
        && previousLevelCache->pointPositions.size() / 3u
            <= static_cast<std::size_t>(std::numeric_limits<int>::max())
        && levelTriVertexCount_ == static_cast<int>(expectedLevelTriVertexCount)
        && preservedLevelTriBatchCount == previousLevelCache->triBuilds.size()
        && wireLineVertexCount_
            == static_cast<int>(previousLevelCache->lineVertices.size())
        && levelPointVertexCount_
            == static_cast<int>(previousLevelCache->pointPositions.size() / 3u)
        && composedTriCountsValid
        && composedLineCountsValid
        && composedPointCountsValid;

    bool reusableStaticLineLayerRanges = preserveStaticComposition;
    if (reusableStaticLineLayerRanges && !baseLineLayerRanges_.empty())
    {
        // Ranges are ordered by vertex offset. The last range in the whole
        // vector is normally a Thing range, so validate the last Level range
        // at the prefix boundary rather than requiring the overall tail to
        // end inside the static prefix.
        const auto firstThingRange = std::lower_bound(
            baseLineLayerRanges_.begin(),
            baseLineLayerRanges_.end(),
            wireLineVertexCount_,
            [](const LayerLineRange& range, const int vertexOffset)
            {
                return range.first < vertexOffset;
            });
        reusableStaticLineLayerRanges =
            firstThingRange == baseLineLayerRanges_.end()
            || firstThingRange->first >= wireLineVertexCount_;
        if (reusableStaticLineLayerRanges && firstThingRange != baseLineLayerRanges_.begin())
        {
            const LayerLineRange& lastLevelRange = *std::prev(firstThingRange);
            reusableStaticLineLayerRanges = lastLevelRange.first >= 0
                && lastLevelRange.count >= 0
                && (lastLevelRange.first % 2) == 0
                && (lastLevelRange.count % 2) == 0
                && lastLevelRange.first <= wireLineVertexCount_
                && lastLevelRange.count <= wireLineVertexCount_ - lastLevelRange.first;
        }
    }

    if (buildLevel
        && renderPipeline_ != RenderPipeline::GamePreview
        && surfaceSubMode_ == SurfaceSubMode::Edge)
    {
        (void)revalidateSurfaceEdgeSelection(&highlightedSelection_);
    }
    if (buildThings)
    {
        architecturalLightingRenderCache_.reset();
        architecturalLightingSelectedOwnerCaches_.clear();
        architecturalLightingSelectedOwnerCacheByThing_.clear();
        architecturalLightingOwnerGizmoMetadataValid_ = false;
        architecturalLightingOwnerGizmoCacheValid_ = false;
        architecturalLightingOwnerGizmoEntries_.clear();
        architecturalLightingOwnerGizmoEntryByThing_.clear();
        architecturalLightingOwnerGizmoVertices_.clear();
        thingBoundsExtremeOwnersValid_ = false;
        thingTypeGizmoMetadataValid_ = false;
        thingTypeGizmoCacheValid_ = false;
        thingTypeGizmoEntryByPickId_.clear();
        thingTypeGizmoDirtyOwners_.clear();
        thingLightGizmoEntriesCacheValid_ = false;
    }
    if (buildLevel)
    {
        sectorThrustCenters_.clear();
        if (level_)
        {
            // These spatial-index builders are opaque synchronous calls. Give
            // the delayed layer-progress widget a guaranteed paint boundary
            // on both sides; unlike the controllable loops below, they cannot
            // safely yield from inside the builder.
            pulseProgress();
            sectorContainmentIndex_.rebuild(*level_);
            pulseProgress();

            // Placement is an editor interaction and must not spend mouse-move
            // time intersecting geometry the user deliberately hid. Building
            // the visibility-filtered broad phase here also prevents hidden
            // surfaces from unexpectedly receiving a placed Thing.
            pulseProgress();
            surfacePickIndex_.rebuild(
                *level_,
                [this](const std::int32_t sectorIndex)
                {
                    return sectorIndex >= 0
                        && static_cast<std::size_t>(sectorIndex) < level_->sectors.size()
                        && isLayerVisible(level_->sectors[static_cast<std::size_t>(sectorIndex)].layer);
                });
            pulseProgress();
        }
        else
        {
            surfacePickIndex_ = sed::level::SurfacePickIndex{};
            sectorContainmentIndex_ = sed::level::SectorContainmentIndex{};
        }
    }

    // Fast-path for "append a few things": reuse the existing thing mesh cache and only build the new tail.
    // This avoids rebuilding all thing geometry when placing things in large levels.
    bool incrementalThingAppend = false;
    std::size_t incrementalThingStartIndex = 0;
    std::size_t incrementalPrevThingLineCount = 0;
    std::size_t incrementalPrevThingPointCount = 0;
    std::size_t incrementalPrevThingTriBuildCount = 0;
    if (buildThings
        && (rebuildKind == MeshRebuildKind::ThingsOnly)
        && !resetPickIds
        && level_
        && thingMeshCache_.valid
        && !thingRenderState_.empty()
        && (!showThings_ || (thingBounds_.size() == thingRenderState_.size()))
        && (thingPickIds_.size() >= thingRenderState_.size())
        && (level_->things.size() > thingRenderState_.size()))
    {
        // A single brush stamp can author up to 256 Things. Appending that
        // tail is still strictly cheaper than rebuilding every persistent
        // Thing in a large level, and the validation below guarantees that
        // the existing prefix has not changed.
        constexpr std::size_t kMaxIncrementalAppend = 256;
        const std::size_t prevCount = thingRenderState_.size();
        const std::size_t delta = level_->things.size() - prevCount;
        if (delta <= kMaxIncrementalAppend)
        {
            bool ok = true;
            const bool trustedAppend =
                trustedThingAppendStart_.has_value()
                && *trustedThingAppendStart_ == prevCount;
            for (std::size_t i = 0; !trustedAppend && i < prevCount; ++i)
            {
                if (thingGeometrySignature(level_->things[i]) != thingRenderState_[i].signature)
                {
                    ok = false;
                    break;
                }
            }

            // Camera/header edits can be pending when a placement appends a
            // Thing before the next paint. Reuse the old prefix only if its
            // selected geosets still match the current view.
            if (ok && thingModelLodEnabled_
                && level_->kind != ProjectType::IJIM
                && !multiLodThingIndices_.empty()
                && (!thingLodHeaderChecked_
                    || thingLodCheckedViewSerial_ != viewSerial_
                    || thingLodCheckedHeaderDistances_
                        != level_->header.lodDistances))
            {
                const ThingLodView lodView = thingLodView();
                for (const std::int32_t owner : multiLodThingIndices_)
                {
                    if (owner < 0 || static_cast<std::size_t>(owner) >= prevCount)
                    {
                        continue;
                    }
                    const ThingRenderState& oldState =
                        thingRenderState_[static_cast<std::size_t>(owner)];
                    if (selectedThingModelLod(
                            oldState.geosetCount,
                            level_->things[static_cast<std::size_t>(owner)],
                            lodView) != oldState.requestedLodIndex)
                    {
                        ok = false;
                        break;
                    }
                }
            }

            const bool havePlacementPreview =
                placementPreviewThing_.has_value() || !placementPreviewThings_.empty();
            if (ok && havePlacementPreview)
            {
                ok = placementPreviewRenderStateValid_
                    && !placementPreviewRenderRanges_.empty()
                    && placementPreviewRenderedThings_.size() == placementPreviewRenderRanges_.size()
                    && placementPreviewRenderedThings_.size()
                        == placementPreviewLightingSignatures_.size();
            }

            if (ok)
            {
                if (havePlacementPreview)
                {
                    const PlacementPreviewRenderRange& first = placementPreviewRenderRanges_.front();
                    const PlacementPreviewRenderRange& last = placementPreviewRenderRanges_.back();
                    const bool rangesValid =
                        first.triBuildFirst <= thingMeshCache_.triBuilds.size()
                        && first.lineFirst <= thingMeshCache_.lineVertices.size()
                        && last.triBuildFirst + last.triBuildCount <= thingMeshCache_.triBuilds.size()
                        && last.lineFirst + last.lineCount <= thingMeshCache_.lineVertices.size()
                        && placementPreviewSpriteBatchFirst_ <= spriteTriBatches_.size();
                    if (!rangesValid)
                    {
                        ok = false;
                    }
                    else
                    {
                        // Placement preview geometry is always emitted as the tail of the Thing
                        // caches. Strip that transient tail before incrementally appending newly
                        // committed Things, then emit the current preview once again below.
                        thingMeshCache_.triBuilds.resize(first.triBuildFirst);
                        thingMeshCache_.lineVertices.resize(first.lineFirst);
                        std::erase_if(spriteInstances_, [](const SpriteInstance& instance)
                            {
                                return instance.placementPreviewIndex >= 0;
                            });
                        spriteTriBatches_.resize(placementPreviewSpriteBatchFirst_);
                        spriteHasType0Billboards_ = std::any_of(
                            spriteInstances_.begin(), spriteInstances_.end(),
                            [](const SpriteInstance& inst) { return inst.type == 0; });

                        QVector3D cachedMin(
                            std::numeric_limits<float>::max(),
                            std::numeric_limits<float>::max(),
                            std::numeric_limits<float>::max());
                        QVector3D cachedMax(
                            -std::numeric_limits<float>::max(),
                            -std::numeric_limits<float>::max(),
                            -std::numeric_limits<float>::max());
                        bool cachedHaveAny = false;
                        for (const ThingBounds& bounds : thingBounds_)
                        {
                            if (bounds.valid)
                            {
                                render::fast_update::includeBounds(
                                    cachedMin, cachedMax, cachedHaveAny, bounds.min, bounds.max);
                            }
                        }
                        thingMeshCache_.min = cachedMin;
                        thingMeshCache_.max = cachedMax;
                        thingMeshCache_.haveAny = cachedHaveAny;
                    }
                }
            }

            if (ok)
            {
                incrementalThingAppend = true;
                incrementalThingStartIndex = prevCount;
                incrementalPrevThingLineCount = thingMeshCache_.lineVertices.size();
                incrementalPrevThingPointCount = thingMeshCache_.pointPositions.size();
                incrementalPrevThingTriBuildCount = thingMeshCache_.triBuilds.size();
            }
        }
    }

    lastMeshRebuildKind_ = rebuildKind;
    if (!preserveStaticComposition)
    {
        levelTriVertexCount_ = 0;
        lineVertices_.clear();
        baseLineLayerRanges_.clear();
        wireLineVertexCount_ = 0;
        thingLineFirstVertex_ = 0;
    }
    else
    {
        // Keep each static Level prefix and crop the previous Thing/overlay
        // suffixes before appending the newly selected Thing tiers.
        lineVertices_.resize(static_cast<std::size_t>(wireLineVertexCount_));
        triVertices_.resize(static_cast<std::size_t>(levelTriVertexCount_));
        triBatches_.resize(preservedLevelTriBatchCount);
        pointPositions_.resize(static_cast<std::size_t>(levelPointVertexCount_) * 3u);
    }
    thingTriVertexCount_ = 0;
    thingLineVertexCount_ = 0;
    highlightLineVertices_.clear();
    highlightLineVertexCount_ = 0;
    if (!preserveStaticComposition)
    {
        triVertices_.clear();
        triBatches_.clear();
    }
    thingTriBuildGlobalFirstVertex_.clear();

    if (buildThings)
    {
        spriteTrianglesDirty_ = true;
        // An append leaves the existing prefix authoritative. Preserve its
        // expanded cards until rebuildSpriteVertices() can reuse them; only
        // newly committed and re-emitted preview sprites need frame/lighting
        // generation. The upload remains one contiguous write after topology
        // changes, but its CPU cost no longer scales with all placed sprites.
        spriteReuseExistingVertices_ = incrementalThingAppend
            && !spriteTriVertices_.empty()
            && !spriteTriRangesByPickId_.empty();
        if (!spriteReuseExistingVertices_)
        {
            spriteTriVertices_.clear();
            spriteTriVertexCount_ = 0;
        }

        if (!incrementalThingAppend)
        {
            spriteHasType0Billboards_ = false;
            spriteInstances_.clear();
            spriteTriBatches_.clear();
        }
    }
    if (!preserveStaticComposition)
    {
        levelPickRanges_.clear();
    }
    thingPickRanges_.clear();
    triVertexCount_ = 0;
    outlineTriVertices_.clear();
    outlineSourceRanges_.clear();
    outlineTriVertexCount_ = 0;
    outlineTriMultiVertexCount_ = 0;
    geometrySelectionStencilRanges_.clear();
    geometrySelectionLineRanges_.clear();
    outlinedThing_ = -1;
    if (!preserveStaticComposition)
    {
        pointPositions_.clear();
        levelPointVertexCount_ = 0;
        thingPointFirstVertex_ = 0;
    }
    pointVertexCount_ = 0;
    thingPointVertexCount_ = 0;

    if (buildLevel)
    {
        alphaAdjoinWireLineRanges_.clear();
        alphaAdjoinWireSupportsDirty_ = true;
    }

    if (resetPickIds)
    {
        pickObjects_.clear();
        pickObjects_.push_back(PickObject{});
        surfacePickIds_.clear();
        thingPickIds_.clear();
        surfaceLineRangesByPickId_.clear();
    }
    else
    {
        if (pickObjects_.empty())
        {
            pickObjects_.push_back(PickObject{});
        }
        if (buildLevel)
        {
            surfaceLineRangesByPickId_.clear();
        }
    }

    levelPeelCacheValid_ = false;
    levelPeelCache_.hits.clear();
    thingPeelCacheValid_ = false;
    thingPeelCache_.hits.clear();

    if (!level_)
    {
        camera_.orbit.center = QVector3D(0.0f, 0.0f, 0.0f);
        camera_.orbit.radius = 1.0f;
        surfaceLineRangesByPickId_.clear();
        alphaAdjoinWireLineRanges_.clear();
        alphaAdjoinWireSupportsDirty_ = true;
        thingRenderState_.clear();
        multiLodThingIndices_.clear();
        placementPreviewLodStates_.clear();
        placementPreviewRenderStateValid_ = false;
        thingSectorLightCache_.clear();
        thingTriBuildRangesByPickId_.clear();
        thingLineRangesByPickId_.clear();
        thingCollisionOverlayRangesByPickId_.clear();
        thingBroadPhaseTreeGuideRangesByPickId_.clear();
        thingVisualBoundsOverlayRangesByPickId_.clear();
        thingTemplateCollisionBoundsGuideRangesByPickId_.clear();
        thingCollisionOverlayLineRanges_.clear();
        thingBroadPhaseTreeGuideLineRanges_.clear();
        thingVisualBoundsOverlayLineRanges_.clear();
        thingTemplateCollisionBoundsGuideLineRanges_.clear();
        thingOverlayLineFirstVertex_ = 0;
        thingOverlayLineVertexCount_ = 0;
        thingBounds_.clear();
        spriteThingBounds_.clear();
        thingPointIndexByThingIndex_.clear();
        return;
    }

    if (buildLevel)
    {
        sectorThrustCenters_.reserve(level_->sectors.size());
        for (const Sector& sector : level_->sectors)
        {
            const TVector center = sed::level::sectorCalcCenter(sector);
            sectorThrustCenters_.emplace_back(center.x, center.y, center.z);
        }
    }

    const auto maxLightMode = static_cast<TLightMode>(static_cast<std::int32_t>(lightingMode_));

    QVector3D levelMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector3D levelMax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    bool levelHaveAny = false;

    QVector3D thingMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector3D thingMax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    bool thingHaveAny = false;

    std::vector<LineVertex> levelLinePositions{};
    std::vector<LineVertex> thingLinePositions{};
    std::vector<LineVertex> thingOverlayLinePositions{};
    std::unordered_map<int, std::vector<render::DrawRange>> collisionOverlayRangesByPickId{};
    std::unordered_map<int, std::vector<render::DrawRange>> broadPhaseTreeGuideRangesByPickId{};
    std::unordered_map<int, std::vector<render::DrawRange>> visualBoundsOverlayRangesByPickId{};
    std::unordered_map<int, std::vector<render::DrawRange>> templateCollisionBoundsGuideRangesByPickId{};

    std::vector<float> levelPointPositions{};
    std::vector<float> thingPointPositions{};

    // Record only visible model Things with multiple geosets. Camera and
    // header edits inspect this compact list instead of resolving templates
    // or resources on every frame.
    std::vector<std::size_t> builtThingGeosetCounts{};
    std::vector<std::size_t> builtThingLodIndices{};
    std::vector<std::size_t> builtThingRequestedLodIndices{};
    std::vector<std::uint8_t> builtThingArchFlags{};
    std::optional<ThingLodView> lodView{};
    if (buildThings && level_ && thingModelLodEnabled_
        && level_->kind != ProjectType::IJIM)
    {
        builtThingGeosetCounts.resize(level_->things.size(), 0u);
        builtThingLodIndices.resize(level_->things.size(), 0u);
        builtThingRequestedLodIndices.resize(level_->things.size(), 0u);
        builtThingArchFlags.resize(level_->things.size(), 0u);
        lodView = thingLodView();
        if (incrementalThingAppend)
        {
            for (std::size_t i = 0;
                 i < incrementalThingStartIndex && i < thingRenderState_.size();
                 ++i)
            {
                builtThingGeosetCounts[i] = thingRenderState_[i].geosetCount;
                builtThingLodIndices[i] = thingRenderState_[i].lodIndex;
                builtThingRequestedLodIndices[i] =
                    thingRenderState_[i].requestedLodIndex;
                builtThingArchFlags[i] = thingRenderState_[i].archThing ? 1u : 0u;
            }
        }
    }

    struct BatchBuild
    {
        TriKind kind{TriKind::Level};
        std::string key{};
        std::string material{};
        std::string colormap{};
        bool uvNormalized{};
        std::int32_t skyGroup{};
        bool doubleSided{};
        std::uint32_t faceFlags{};
        std::vector<GlView::TriVertex> vertices{};
        std::vector<GlView::TriShadeInput> shadeInputs{};
    };

    std::vector<BatchBuild> builds;
    std::unordered_map<std::string, std::size_t> buildIndices;
    const bool perThingTextureBatches = showTextures_ && (thingViewMode_ == ThingViewMode::Shaded);
    std::int32_t translucentFaceGroupSerial = 0;

    auto getBuildIndex = [&](const TriKind kind,
                             const std::string_view material,
                             const std::string_view colormap,
                             const bool uvNormalized = false,
                             const std::int32_t skyGroup = 0,
                             const bool doubleSided = false,
                             const std::int32_t thingIndex = -1,
                             const std::uint32_t faceFlagsKey = 0,
                             const std::int32_t uniqueGroup = -1) -> std::size_t
    {
        const std::string key = makeBatchKey(material, colormap);
        const std::uint32_t flagsKey = faceFlagsKey & (FF_Transluent | FF_ZWriteDisabled | FF_RD_Adjoin | FF_IJIM_FogEnabled);
        const std::string mapKey =
            (kind == TriKind::Thing)
            ? (
                std::string(uvNormalized ? "T1|" : "T0|")
                + (doubleSided ? "D1|" : "D0|")
                + "F" + std::to_string(flagsKey) + "|"
                + ((uniqueGroup >= 0) ? ("G" + std::to_string(uniqueGroup) + "|") : std::string{})
                + ((perThingTextureBatches && thingIndex >= 0 && !material.empty() && material != "-") ? ("I" + std::to_string(thingIndex) + "|") : std::string{})
                + key)
            : (
                std::string("L") + std::to_string(skyGroup) + "|"
                + (uvNormalized ? "N1|" : "N0|")
                + (doubleSided ? "D1|" : "D0|")
                + "F" + std::to_string(flagsKey) + "|"
                + ((uniqueGroup >= 0) ? ("G" + std::to_string(uniqueGroup) + "|") : std::string{})
                + key);
        const auto [it, inserted] = buildIndices.emplace(mapKey, builds.size());
        if (inserted)
        {
            BatchBuild build{};
            build.kind = kind;
            build.key = key;
            build.material = std::string(material);
            build.colormap = std::string(colormap);
            build.uvNormalized = uvNormalized;
            build.skyGroup = skyGroup;
            build.doubleSided = doubleSided;
            build.faceFlags = flagsKey;
            builds.push_back(std::move(build));
        }
        return it->second;
    };

    auto extendBounds = [](QVector3D& minv, QVector3D& maxv, bool& haveAny, const float x, const float y, const float z)
    {
        minv.setX(std::min(minv.x(), x));
        minv.setY(std::min(minv.y(), y));
        minv.setZ(std::min(minv.z(), z));
        maxv.setX(std::max(maxv.x(), x));
        maxv.setY(std::max(maxv.y(), y));
        maxv.setZ(std::max(maxv.z(), z));
        haveAny = true;
    };

    auto extendLevel = [&](const float x, const float y, const float z)
    {
        extendBounds(levelMin, levelMax, levelHaveAny, x, y, z);
    };

    auto extendThing = [&](const float x, const float y, const float z)
    {
        extendBounds(thingMin, thingMax, thingHaveAny, x, y, z);
    };

    if (incrementalThingAppend)
    {
        // Start from the previous thing cache and only build the newly appended tail.
        if (thingMeshCache_.haveAny)
        {
            thingMin = thingMeshCache_.min;
            thingMax = thingMeshCache_.max;
            thingHaveAny = true;
        }

        thingLinePositions = std::move(thingMeshCache_.lineVertices);
        thingOverlayLinePositions = std::move(thingMeshCache_.overlayLineVertices);
        thingPointPositions = std::move(thingMeshCache_.pointPositions);
    }

    auto pushLinePoint = [&](std::vector<LineVertex>& positions,
                             const double x,
                             const double y,
                             const double z,
                             const float pickId = 0.0f,
                             const Vertex* planeReference = nullptr)
    {
        const float px = static_cast<float>(x);
        const float py = static_cast<float>(y);
        const float pz = static_cast<float>(z);
        LineVertex v{};
        v.x = px;
        v.y = py;
        v.z = pz;
        v.pickId = pickId;
        if (planeReference)
        {
            v.planeX = static_cast<float>(planeReference->x);
            v.planeY = static_cast<float>(planeReference->y);
            v.planeZ = static_cast<float>(planeReference->z);
            v.planeValid = 1.0f;
        }
        positions.push_back(v);
        if (&positions == &levelLinePositions)
        {
            extendLevel(px, py, pz);
        }
        else
        {
            extendThing(px, py, pz);
        }
    };

    auto appendWirePoints = [&](std::vector<LineVertex>& positions, const auto& wirePoints)
    {
        for (const render::ThingWirePoint& point : wirePoints)
        {
            pushLinePoint(positions, point.x, point.y, point.z, point.pickId);
        }
    };

    auto appendOverlayWirePoints = [&](const std::vector<TVector>& points,
                                       std::unordered_map<int, std::vector<render::DrawRange>>& rangesByPickId,
                                       const int pickId)
    {
        if (pickId <= 0 || points.empty()
            || points.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
            || thingOverlayLinePositions.size()
                > static_cast<std::size_t>(std::numeric_limits<int>::max()) - points.size())
        {
            return;
        }

        const std::vector<TVector> segments = render::thing_overlay::finiteWireSegments(points);
        if (segments.empty()
            || thingOverlayLinePositions.size()
                > static_cast<std::size_t>(std::numeric_limits<int>::max()) - segments.size())
        {
            return;
        }

        const int first = static_cast<int>(thingOverlayLinePositions.size());
        for (const TVector& point : segments)
        {
            LineVertex vertex{};
            vertex.x = static_cast<float>(point.x);
            vertex.y = static_cast<float>(point.y);
            vertex.z = static_cast<float>(point.z);
            // GPU pick passes omit this overlay range. Retain its owner ID so
            // live Thing Move/Rotate previews transform the line in the shader.
            vertex.pickId = static_cast<float>(pickId);
            thingOverlayLinePositions.push_back(vertex);
        }
        render::appendOrExtendDrawRange(
            rangesByPickId[pickId], first, static_cast<int>(segments.size()));
    };

    auto pushLineVertex = [&](std::vector<LineVertex>& positions,
                              const Vertex& v,
                              const float pickId = 0.0f,
                              const Vertex* planeReference = nullptr)
    {
        pushLinePoint(positions, v.x, v.y, v.z, pickId, planeReference);
    };

    auto pushTriVertex = [&](BatchBuild& build,
                             const Vertex& v,
                             const TexVertex& tv,
                             const ColorF& litColor,
                             const float pickId,
                             const float pickGeo,
                             const float sky,
                             const TriShadeInput* shadeInput = nullptr,
                             const TVector* normal = nullptr,
                             const float dynamicLightTarget = 0.0f,
                             const PointLight* sectorPointLight = nullptr,
                             const std::int32_t sectorPointLightOwner = -1,
                             const Vertex* faceLightAnchor = nullptr,
                             const ColorF* classicExtraLight = nullptr,
                             const ColorF* classicAmbientFloor = nullptr,
                             const QVector3D* editorSectorTintScale = nullptr,
                             const QVector3D* surfaceWireBarycentric = nullptr,
                             const QVector4D* surfaceWireMetadata = nullptr) -> void
    {
        TriVertex out{};
        out.x = static_cast<float>(v.x);
        out.y = static_cast<float>(v.y);
        out.z = static_cast<float>(v.z);

        out.u = tv.u;
        out.v = tv.v;

        out.r = litColor.r;
        out.g = litColor.g;
        out.b = litColor.b;
        out.a = litColor.a;

        out.pickId  = pickId;
        out.pickGeo = pickGeo;
        out.sky     = sky;
        if (editorSectorTintScale)
        {
            out.editorSectorTintR = editorSectorTintScale->x();
            out.editorSectorTintG = editorSectorTintScale->y();
            out.editorSectorTintB = editorSectorTintScale->z();
        }

        if (normal)
        {
            out.normalX = static_cast<float>(normal->x);
            out.normalY = static_cast<float>(normal->y);
            out.normalZ = static_cast<float>(normal->z);
        }
        out.dynamicLightTarget = dynamicLightTarget;
        if (sectorPointLight)
        {
            const render::lighting::RuntimeSectorPointLight runtimeSectorLight =
                render::lighting::resolveRuntimeSectorPointLight(*sectorPointLight);
            out.sectorLightX = runtimeSectorLight.position.x();
            out.sectorLightY = runtimeSectorLight.position.y();
            out.sectorLightZ = runtimeSectorLight.position.z();
            out.sectorLightMinRadius = runtimeSectorLight.minRadius;
            out.sectorLightR = runtimeSectorLight.color.x();
            out.sectorLightG = runtimeSectorLight.color.y();
            out.sectorLightB = runtimeSectorLight.color.z();
            out.sectorLightMaxRadius = runtimeSectorLight.maxRadius;
            out.sectorLightOwner = static_cast<float>(sectorPointLightOwner);
        }
        else
        {
            if (classicExtraLight)
            {
                out.sectorLightX = classicExtraLight->r;
                out.sectorLightY = classicExtraLight->g;
                out.sectorLightZ = classicExtraLight->b;
            }
            if (classicAmbientFloor)
            {
                out.sectorLightR = classicAmbientFloor->r;
                out.sectorLightG = classicAmbientFloor->g;
                out.sectorLightB = classicAmbientFloor->b;
            }
        }
        if (faceLightAnchor)
        {
            out.faceLightAnchorX = static_cast<float>(faceLightAnchor->x);
            out.faceLightAnchorY = static_cast<float>(faceLightAnchor->y);
            out.faceLightAnchorZ = static_cast<float>(faceLightAnchor->z);
        }
        if (surfaceWireBarycentric)
        {
            out.surfaceWireBarycentricX = surfaceWireBarycentric->x();
            out.surfaceWireBarycentricY = surfaceWireBarycentric->y();
            out.surfaceWireBarycentricZ = surfaceWireBarycentric->z();
        }
        if (surfaceWireMetadata)
        {
            out.surfaceWireBoundaryX = surfaceWireMetadata->x();
            out.surfaceWireBoundaryY = surfaceWireMetadata->y();
            out.surfaceWireBoundaryZ = surfaceWireMetadata->z();
            out.surfaceWireLayer = surfaceWireMetadata->w();
        }

        build.vertices.push_back(out);
        if (shadeInput)
        {
            build.shadeInputs.push_back(*shadeInput);
        }
        else if (!build.shadeInputs.empty())
        {
            // Keep shadeInputs aligned with vertices if a caller ever mixes shaded/unshaded pushes.
            build.shadeInputs.push_back(TriShadeInput{});
        }
        if (build.kind == TriKind::Level)
        {
            extendLevel(out.x, out.y, out.z);
        }
        else
        {
            extendThing(out.x, out.y, out.z);
        }
    };

    auto appendLevelSectorGeometry =
        [&](const Sector& sector,
            const std::int32_t sectorIndex32,
            const bool pickable,
            const bool includePoints,
            const bool previewOnly)
    {
        if (!isLayerVisible(sector.layer))
        {
            return;
        }

        if (includePoints)
        {
            for (const Vertex& v : sector.vertices)
            {
                const float x = static_cast<float>(v.x);
                const float y = static_cast<float>(v.y);
                const float z = static_cast<float>(v.z);

                levelPointPositions.push_back(x);
                levelPointPositions.push_back(y);
                levelPointPositions.push_back(z);

                extendLevel(x, y, z);
            }
        }

        // Keep the edit preview translucent enough to see the source geometry,
        // but opaque enough to read its new volume against textured sectors.
        const float surfaceExtrudePreviewAlpha = surfaceExtrudePreviewOpacity_;
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            reportProgress();
            const Surface& surface = sector.surfaces[surfaceIndex];
            const auto surfaceIndex32 = static_cast<std::int32_t>(surfaceIndex);
            const QVector3D editorSectorTintScale =
                render::classicEditorSurfaceTintScale(
                    level_, sectorIndex32, surface);
            const bool hideSourcePreviewSurface =
                !previewOnly
                && placementPreviewHiddenSurface_
                && placementPreviewHiddenSurface_->sector == sectorIndex32
                && placementPreviewHiddenSurface_->surface == surfaceIndex32;
            if (hideSourcePreviewSurface)
            {
                continue;
            }

            if (previewOnly && surface.geo <= static_cast<std::int32_t>(TGeoMode::NotDrawn))
            {
                continue;
            }

            const auto& vxs = surface.vertices;
            if (vxs.size() < 2)
            {
                continue;
            }

            const std::uint32_t surfacePickId = pickable
                ? registerSurfacePickId(sectorIndex32, surfaceIndex32)
                : 0u;
            const float surfacePickIdF = static_cast<float>(surfacePickId);
            const int surfaceLineFirst = static_cast<int>(levelLinePositions.size());
            const bool surfaceHasDepthWritingFill = !previewOnly
                && wireframeSurfaceCanMatchDepthFromFill(level_->kind, surface);

            for (std::size_t i = 0; i < vxs.size(); ++i)
            {
                const std::int32_t aidx = vxs[i];
                const std::int32_t bidx = vxs[(i + 1) % vxs.size()];
                if (aidx < 0 || bidx < 0)
                {
                    continue;
                }
                if (static_cast<std::size_t>(aidx) >= sector.vertices.size()
                    || static_cast<std::size_t>(bidx) >= sector.vertices.size())
                {
                    continue;
                }
                const Vertex* planeReference = surfaceHasDepthWritingFill
                    ? wireframeSurfacePlaneReference(sector, surface, i)
                    : nullptr;
                pushLineVertex(
                    levelLinePositions,
                    sector.vertices[static_cast<std::size_t>(aidx)],
                    surfacePickIdF,
                    planeReference);
                pushLineVertex(
                    levelLinePositions,
                    sector.vertices[static_cast<std::size_t>(bidx)],
                    surfacePickIdF,
                    planeReference);
            }

            const int surfaceLineCount = static_cast<int>(levelLinePositions.size()) - surfaceLineFirst;
            if (pickable && surfaceLineCount > 0)
            {
                surfaceLineRangesByPickId_[surfacePickId] = DrawRange{ surfaceLineFirst, surfaceLineCount };
            }
            const std::uint32_t surfaceRenderFaceFlags = render::levelSurfaceRenderFaceFlags(
                level_->kind,
                surface.surfaceFlags,
                surface.faceFlags);
            const bool surfaceIsAdjoin = surface.adjoin.has_value()
                || ((surfaceRenderFaceFlags & FF_RD_Adjoin) != 0u);
            if (!previewOnly && surfaceHasDepthWritingFill && surfaceIsAdjoin && surfaceLineCount > 0)
            {
                alphaAdjoinWireLineRanges_.push_back(AdjoinWireLineRange{
                    .range = DrawRange{ surfaceLineFirst, surfaceLineCount },
                    .textureKey = render::makeBatchKey(
                        surface.material,
                        sectorRenderColormap(*level_, sector)),
                });
            }

            const auto& tvxs = surface.texVertices;
            if (vxs.size() < 3
                || tvxs.size() != vxs.size()
                || !std::isfinite(surface.uScale)
                || !std::isfinite(surface.vScale)
                || std::ranges::any_of(
                    tvxs,
                    [](const TexVertex& texVertex)
                    {
                        return !std::isfinite(texVertex.u)
                            || !std::isfinite(texVertex.v);
                    }))
            {
                // Keep the topology available to the wireframe editor, but
                // never invent or truncate UV corners for a filled polygon.
                continue;
            }
            const std::size_t count = vxs.size();
            const auto polygonCornersShareBoundaryEdge = [count](const std::size_t first, const std::size_t second)
            {
                return ((first + 1) % count) == second
                    || ((second + 1) % count) == first;
            };

            std::int32_t polyLightMode = surface.light;
            if ((sector.flags & SECF_3DO) != 0
                || (surface.surfaceFlags & (SF_SkyHorizon | SF_SkyCeiling)) != 0)
            {
                polyLightMode = static_cast<std::int32_t>(TLightMode::None);
            }

            const bool dynamicVertexLighting =
                polyLightMode >= static_cast<std::int32_t>(TLightMode::Gouraud)
                && maxLightMode >= TLightMode::Gouraud;
            const bool ijimDynamicWorldLighting =
                level_->kind == ProjectType::IJIM && dynamicVertexLighting;
            const bool classicDynamicWorldLighting =
                level_->kind != ProjectType::IJIM && dynamicVertexLighting;
            const Vertex ijimWorldExtra{
                static_cast<double>(sector.extraLight.r + surface.extraLight.r),
                static_cast<double>(sector.extraLight.g + surface.extraLight.g),
                static_cast<double>(sector.extraLight.b + surface.extraLight.b) };
            ColorF classicWorldAmbientFloor = sector.extraLight;
            classicWorldAmbientFloor.r = std::clamp(classicWorldAmbientFloor.r, 0.0f, 1.0f);
            classicWorldAmbientFloor.g = std::clamp(classicWorldAmbientFloor.g, 0.0f, 1.0f);
            classicWorldAmbientFloor.b = std::clamp(classicWorldAmbientFloor.b, 0.0f, 1.0f);

            std::vector<ColorF> litColors{};
            std::vector<TriShadeInput> levelShadeInputs{};
            litColors.reserve(count);
            levelShadeInputs.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                ColorF intensity = tvxs[i].color;
                intensity.a = surface.extraLight.a;
                TriShadeInput shade{};
                shade.intensity = intensity;
                shade.extraLight = surface.extraLight;
                shade.lightMode = polyLightMode;
                if (!previewOnly && sectorIndex32 >= 0)
                {
                    shade.sourceSector = sectorIndex32;
                    shade.sourceSurface = surfaceIndex32;
                    shade.sourceTexVertex = static_cast<std::int32_t>(i);
                }
                levelShadeInputs.push_back(shade);
                litColors.push_back(polyCalcLitColor(
                    intensity,
                    polyLightMode,
                    ijimDynamicWorldLighting ? ColorBlack : sector.extraLight, // ambient light is only applied to things
                    ijimDynamicWorldLighting ? ColorBlack : surface.extraLight,
                    level_->kind,
                    maxLightMode,
                    dynamicVertexLighting));
                litColors.back().a = render::classicFaceOpacity(
                    level_->kind,
                    surface.faceFlags,
                    litColors.back().a);
                if (previewOnly)
                {
                    litColors.back().a = std::min(litColors.back().a, surfaceExtrudePreviewAlpha);
                }
            }

            if (const std::int32_t v0 = vxs[0];
                v0 < 0 || static_cast<std::size_t>(v0) >= sector.vertices.size())
            {
                continue;
            }

            const std::string colormap = sectorRenderColormap(*level_, sector);
            const std::int32_t skyGroup =
                (surface.surfaceFlags & SF_SkyCeiling) != 0
                ? 1
                : (((surface.surfaceFlags & SF_SkyHorizon) != 0) ? 2 : 0);

            std::uint32_t faceFlagsKey =
                levelSurfaceRenderFaceFlags(level_->kind, surface.surfaceFlags, surface.faceFlags);
            const bool doubleSided = (faceFlagsKey & sed::FF_DoubleSided) != 0;

            if (surface.adjoin)
            {
                faceFlagsKey |= FF_RD_Adjoin;
            }
            if (previewOnly)
            {
                faceFlagsKey |= FF_Transluent | FF_ZWriteDisabled;
            }

            const bool separateTranslucentSurface =
                render::translucency::shouldSeparateLevelSurfaceForTranslucency(
                    surface.adjoin.has_value(),
                    previewOnly,
                    skyGroup != 0,
                    (faceFlagsKey & FF_Transluent) != 0u,
                    (faceFlagsKey & FF_ZWriteDisabled) != 0u);
            const std::int32_t uniqueGroup = separateTranslucentSurface ? translucentFaceGroupSerial++ : -1;

            const std::size_t batchIndex =
                getBuildIndex(
                    TriKind::Level,
                    surface.material,
                    colormap,
                    level_->uvCoordinateSpace
                        == UvCoordinateSpace::Normalized,
                    skyGroup,
                    doubleSided,
                    -1,
                    faceFlagsKey,
                    uniqueGroup);

            BatchBuild& build = builds[batchIndex];
            const float pickId = surfacePickIdF;
            const float pickGeo = static_cast<float>(surface.geo);
            const float sky = static_cast<float>(skyGroup);
            triangulatePolyZigZag(static_cast<int>(count), [&](const int i0, const int i1, const int i2)
            {
                const std::size_t idx0 = static_cast<std::size_t>(i0);
                const std::size_t idx1 = static_cast<std::size_t>(i1);
                const std::size_t idx2 = static_cast<std::size_t>(i2);

                const std::int32_t v0 = vxs[idx0];
                const std::int32_t v1 = vxs[idx1];
                const std::int32_t v2 = vxs[idx2];
                if (v0 < 0 || v1 < 0 || v2 < 0)
                {
                    return;
                }
                if (static_cast<std::size_t>(v0) >= sector.vertices.size()
                    || static_cast<std::size_t>(v1) >= sector.vertices.size()
                    || static_cast<std::size_t>(v2) >= sector.vertices.size())
                {
                    return;
                }

                const float dynamicTarget = (skyGroup == 0 && dynamicVertexLighting) ? 1.0f : 0.0f;
                const Vertex* dynamicAux = ijimDynamicWorldLighting ? &ijimWorldExtra : nullptr;
                const ColorF* classicExtra = classicDynamicWorldLighting ? &surface.extraLight : nullptr;
                const ColorF* classicAmbient = classicDynamicWorldLighting ? &classicWorldAmbientFloor : nullptr;
                const QVector3D barycentric0(1.0f, 0.0f, 0.0f);
                const QVector3D barycentric1(0.0f, 1.0f, 0.0f);
                const QVector3D barycentric2(0.0f, 0.0f, 1.0f);
                const bool markSurfaceWireEdges = surfaceHasDepthWritingFill;
                const QVector4D surfaceWireMetadata(
                    markSurfaceWireEdges && polygonCornersShareBoundaryEdge(idx1, idx2) ? 1.0f : 0.0f,
                    markSurfaceWireEdges && polygonCornersShareBoundaryEdge(idx2, idx0) ? 1.0f : 0.0f,
                    markSurfaceWireEdges && polygonCornersShareBoundaryEdge(idx0, idx1) ? 1.0f : 0.0f,
                    markSurfaceWireEdges ? static_cast<float>(sector.layer) : -1.0f);
                const QVector4D* surfaceWireMetadataPtr = markSurfaceWireEdges ? &surfaceWireMetadata : nullptr;
                pushTriVertex(
                    build, sector.vertices[static_cast<std::size_t>(v0)], tvxs[idx0], litColors[idx0],
                    pickId, pickGeo, sky, &levelShadeInputs[idx0], &surface.normal, dynamicTarget,
                    nullptr, -1, dynamicAux, classicExtra, classicAmbient, &editorSectorTintScale,
                    markSurfaceWireEdges ? &barycentric0 : nullptr, surfaceWireMetadataPtr);
                pushTriVertex(
                    build, sector.vertices[static_cast<std::size_t>(v1)], tvxs[idx1], litColors[idx1],
                    pickId, pickGeo, sky, &levelShadeInputs[idx1], &surface.normal, dynamicTarget,
                    nullptr, -1, dynamicAux, classicExtra, classicAmbient, &editorSectorTintScale,
                    markSurfaceWireEdges ? &barycentric1 : nullptr, surfaceWireMetadataPtr);
                pushTriVertex(
                    build, sector.vertices[static_cast<std::size_t>(v2)], tvxs[idx2], litColors[idx2],
                    pickId, pickGeo, sky, &levelShadeInputs[idx2], &surface.normal, dynamicTarget,
                    nullptr, -1, dynamicAux, classicExtra, classicAmbient, &editorSectorTintScale,
                    markSurfaceWireEdges ? &barycentric2 : nullptr, surfaceWireMetadataPtr);
            });
        }
    };

    if (buildLevel)
    {
        for (std::size_t sectorIndex = 0; sectorIndex < level_->sectors.size(); ++sectorIndex)
        {
            reportProgress();
            appendLevelSectorGeometry(
                level_->sectors[sectorIndex],
                static_cast<std::int32_t>(sectorIndex),
                true,
                true,
                false);
        }

        if (placementPreviewSourcePatch_)
        {
            appendLevelSectorGeometry(
                *placementPreviewSourcePatch_,
                -2,
                false,
                false,
                true);
        }

        if (placementPreviewSector_)
        {
            appendLevelSectorGeometry(
                *placementPreviewSector_,
                -1,
                false,
                false,
                true);
        }
    }

    auto resolveThingBox = [&](const Thing& thing) -> ThingBox
    {
        return render::resolveThingRenderBox(*level_, templates_, thing);
    };

    auto addBox = [&](std::vector<LineVertex>& positions, const TMatrix43& xf, const ThingBox& box, const float pickId = 0.0f)
    {
        const auto wirePoints = render::makeThingBoxWirePoints(xf, box, pickId);
        appendWirePoints(positions, wirePoints);
    };

    auto addThingCube = [&](std::vector<LineVertex>& linePositions,
                            const TMatrix43& xf,
                            const float halfSize,
                            const float pickId,
                            const std::string_view typeName,
                            const ColorF& color = ColorWhite)
    {
        const double h = static_cast<double>(halfSize);

        std::string iconMat{};
        iconMat.reserve(kThingIconMatPrefix.size() + typeName.size());
        iconMat.append(kThingIconMatPrefix);
        iconMat.append(typeName);

        const float geo = static_cast<float>(TGeoMode::Texture);
        auto& build = builds[getBuildIndex(TriKind::Thing, iconMat, "-", true, 0, true)];

        const std::array<render::ThingCubeFaceVertex, 36> faceVertices =
            render::makeThingCubeFaceVertices(xf, h);
        for (const render::ThingCubeFaceVertex& cubeVertex : faceVertices)
        {
            Vertex vx{
                cubeVertex.position.x,
                cubeVertex.position.y,
                cubeVertex.position.z,
                0,
            };
            TexVertex tv{};
            tv.u = cubeVertex.u;
            tv.v = cubeVertex.v;
            pushTriVertex(build, vx, tv, color, pickId, geo, 0.0f);
        }

        if (thingViewMode_ != ThingViewMode::Shaded)
        {
            const auto wirePoints = render::makeThingCubeWirePoints(xf, h, pickId);
            appendWirePoints(linePositions, wirePoints);
        }
    };

    auto resolveThingTypeNameLower = [&](const Thing& thing) -> std::string
    {
        const TplValue* typeVal = sed::level::thingFindValue(*level_, templates_, thing, "type");
        std::string typeName = "free";
        if (typeVal)
        {
            typeName = typeVal->stringValue.empty() ? typeVal->asString() : typeVal->stringValue;
        }
        std::string lowered = sed::text::lowerAscii(sed::text::trimView(typeName));
        if (lowered.empty())
        {
            lowered = "free";
        }
        return lowered;
    };

    auto addModelWireframe = [&](std::vector<LineVertex>& positions, const assets::Model3do& model, const TMatrix43& xf, const float pickId = 0.0f)
    {
        std::vector<render::ThingWirePoint> wirePoints{};
        render::appendModelWireframeWirePoints(wirePoints, model, xf, pickId);
        appendWirePoints(positions, wirePoints);
    };

    const std::string masterCmp = (level_->kind == ProjectType::IJIM) ? std::string{} : level_->getMasterCMP();

    auto addModelTriangles = [&](const assets::Model3do& model,
                                  const TMatrix43& xf,
                                  const std::int32_t thingIndex,
                                  const float thingPickId,
                                  const ColorF& ambientLight,
                                  std::vector<GlView::TriVertex>* outlineOut,
                                  const ColorF* overrideLitColor = nullptr,
                                  const MotsArchLightObject* archLighting = nullptr,
                                  const PointLight* sectorPointLight = nullptr,
                                  const std::int32_t owningSector = -1)
    {
        const bool uvNormalized =
            std::abs(model.version - 2.3) <= 1.0e-6;
        const QVector3D editorSectorTintScale = overrideLitColor
            ? QVector3D(1.0f, 1.0f, 1.0f)
            : render::classicEditorSectorTintScale(level_, owningSector);

        for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
        {
            const assets::Model3doMesh& mesh = model.meshes[meshIndex];
            const std::optional<std::size_t> sourceMeshIndex =
                render::motsArchLightingSourceMeshIndex(model, meshIndex);
            const std::int32_t archMeshIndex = sourceMeshIndex
                && *sourceMeshIndex <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())
                ? static_cast<std::int32_t>(*sourceMeshIndex)
                : static_cast<std::int32_t>(meshIndex);

            if (mesh.geo == static_cast<std::int32_t>(TGeoMode::NotDrawn))
            {
                continue;
            }

            const MotsArchLightMesh* archMesh = nullptr;
            if (archLighting != nullptr
                && sourceMeshIndex
                && *sourceMeshIndex < archLighting->meshes.size()
                && archLighting->meshes[*sourceMeshIndex].vertices.size()
                    >= mesh.vertices.size())
            {
                archMesh = &archLighting->meshes[*sourceMeshIndex];
            }

            // Hot path: pre-transform mesh vertices once per thing instance instead of re-transforming
            // the same vertex multiple times across faces.
            std::vector<Vertex> xformedVertices{};
            xformedVertices.resize(mesh.vertices.size());

            std::vector<ColorF> vertexColors{};
            vertexColors.resize(mesh.vertices.size());

            std::vector<TVector> xformedNormals{};
            xformedNormals.resize(mesh.vertices.size());

            for (std::size_t vi = 0; vi < mesh.vertices.size(); ++vi)
            {
                const assets::Model3doVertex& mv = mesh.vertices[vi];
                const TVector p = xf.TransformPoint(TVector{ mv.x, mv.y, mv.z });
                xformedVertices[vi] = Vertex{ p.x, p.y, p.z, 0 };
                vertexColors[vi] = mv.color;
                if (archMesh != nullptr)
                {
                    const MotsArchLightVertex& archVertex = archMesh->vertices[vi];
                    // ARCHLIGHTING mono is an authored brightness channel,
                    // not model opacity. The accelerated OpenGL viewport
                    // follows the engine's RGB vertex-color path and retains
                    // the cached 3DO alpha/model authoring value untouched.
                    if (std::isfinite(archVertex.mono)
                        && std::isfinite(archVertex.red)
                        && std::isfinite(archVertex.green)
                        && std::isfinite(archVertex.blue))
                    {
                        vertexColors[vi] = render::motsArchLightingVertexColor(
                            mv.color,
                            archVertex,
                            sed::level::MotsArchLightingVertexMode::Rgb);
                    }
                }
                if (vi < mesh.vertexNormals.size())
                {
                    xformedNormals[vi] = xf.TransformVector(mesh.vertexNormals[vi]);
                    xformedNormals[vi].Normalize();
                }
            }

            for (std::size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex)
            {
                const assets::Model3doFace& face = mesh.faces[faceIndex];
                const std::size_t count = face.vertices.size();
                if (count < 3)
                {
                    continue;
                }

                const bool faceNeedsTex = (face.geo >= static_cast<std::int32_t>(TGeoMode::Texture));
                if (faceNeedsTex && face.texVertices.size() < count)
                {
                    continue;
                }

                std::string_view materialName = "-";
                if (face.material >= 0 && static_cast<std::size_t>(face.material) < model.materials.size())
                {
                    materialName = model.materials[static_cast<std::size_t>(face.material)];
                }

                const std::uint32_t faceFlagsKey = modelFaceRenderFaceFlags(level_->kind, face.faceFlags);
                const bool doubleSided = (faceFlagsKey & sed::FF_DoubleSided) != 0;

                // Translucent faces are order-dependent; avoid batching them together so they can be depth-sorted
                // at a finer granularity (helps complex models with many alpha surfaces).
                const bool explicitTranslucentFace = (faceFlagsKey & (FF_Transluent | FF_ZWriteDisabled)) != 0;
                const std::int32_t uniqueGroup = explicitTranslucentFace ? translucentFaceGroupSerial++ : -1;

                const std::size_t batchIndex = getBuildIndex(
                    TriKind::Thing,
                    materialName,
                    masterCmp,
                    uvNormalized,
                    0,
                    doubleSided,
                    thingIndex,
                    faceFlagsKey,
                    uniqueGroup);
                BatchBuild& build = builds[batchIndex];

                std::int32_t geo = face.geo;
                if (geo > static_cast<std::int32_t>(TGeoMode::NotDrawn) && geo < static_cast<std::int32_t>(TGeoMode::Solid))
                {
                    geo = static_cast<std::int32_t>(TGeoMode::Solid);
                }

                const float pickGeo = static_cast<float>(geo);
                const std::int32_t polyLightMode = render::lighting::effectiveModelLightMode(
                    face.light,
                    mesh.light,
                    maxLightMode);
                const float modelDynamicLightTarget =
                    render::lighting::modelDynamicLightTarget(polyLightMode);
                const bool dynamicVertexLighting = modelDynamicLightTarget > 1.5f
                    && modelDynamicLightTarget < 2.5f;
                const bool dynamicFaceLighting = modelDynamicLightTarget > 2.5f;

                std::vector<TexVertex> faceTexVertices{};
                std::vector<ColorF> faceLitColors{};
                std::vector<TriShadeInput> faceShadeInputs{};
                faceTexVertices.reserve(count);
                faceLitColors.reserve(count);
                faceShadeInputs.reserve(count);

                bool faceValid = true;
                for (std::size_t i = 0; i < count; ++i)
                {
                    const std::int32_t vxIndex = face.vertices[i];
                    if (vxIndex < 0 || static_cast<std::size_t>(vxIndex) >= xformedVertices.size())
                    {
                        faceValid = false;
                        break;
                    }

                    TexVertex tv{};
                    tv.color = vertexColors[static_cast<std::size_t>(vxIndex)];

                    if (faceNeedsTex)
                    {
                        const std::int32_t tvIndex = face.texVertices[i];
                        if (tvIndex < 0 || static_cast<std::size_t>(tvIndex) >= mesh.texVertices.size())
                        {
                            faceValid = false;
                            break;
                        }

                        const assets::Model3doTexVertex& mtv = mesh.texVertices[static_cast<std::size_t>(tvIndex)];
                        tv.u = mtv.u;
                        tv.v = mtv.v;
                    }
                    else
                    {
                        tv.u = 0.0f;
                        tv.v = 0.0f;
                    }

                    TriShadeInput shade{};
                    shade.intensity = tv.color;
                    shade.extraLight = face.extraLight;
                    shade.lightMode = polyLightMode;
                    shade.modelColor = mesh.vertices[static_cast<std::size_t>(vxIndex)].color;
                    shade.ambientLight = ambientLight;
                    shade.archMeshIndex = archMeshIndex;
                    shade.archVertexIndex = vxIndex;

                    faceTexVertices.push_back(tv);
                    ColorF lit = polyCalcLitColor(
                        tv.color,
                        polyLightMode,
                        ambientLight,
                        face.extraLight,
                        level_->kind,
                        maxLightMode,
                        dynamicVertexLighting || dynamicFaceLighting);

                    // 3DO vertex alpha often stores authoring/light data (many IJIM assets use 0.0), not opacity.
                    // Keep thing meshes opaque unless the face explicitly opts into translucent rendering.
                    if (!explicitTranslucentFace)
                    {
                        lit.a = 1.0f;
                    }
                    else
                    {
                        lit.a = render::classicFaceOpacity(
                            level_->kind,
                            faceFlagsKey,
                            lit.a);
                    }
                    if (overrideLitColor)
                    {
                        lit = *overrideLitColor;
                    }

                    faceLitColors.push_back(lit);
                    faceShadeInputs.push_back(shade);
                }

                if (!faceValid)
                {
                    continue;
                }

                TVector faceNormal{};
                if (faceIndex < mesh.faceNormals.size())
                {
                    faceNormal = xf.TransformVector(mesh.faceNormals[faceIndex]);
                    faceNormal.Normalize();
                }
                if (faceNormal.IsZero() && count >= 3)
                {
                    const Vertex& p0 = xformedVertices[static_cast<std::size_t>(face.vertices[0])];
                    const Vertex& p1 = xformedVertices[static_cast<std::size_t>(face.vertices[1])];
                    const Vertex& p2 = xformedVertices[static_cast<std::size_t>(face.vertices[2])];
                    const TVector edge1{ p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
                    const TVector edge2{ p2.x - p0.x, p2.y - p0.y, p2.z - p0.z };
                    faceNormal = edge1.Cross(edge2);
                    faceNormal.Normalize();
                }

                triangulatePolyZigZag(static_cast<int>(count), [&](const int i0, const int i1, const int i2)
                {
                    const std::size_t idx0 = static_cast<std::size_t>(i0);
                    const std::size_t idx1 = static_cast<std::size_t>(i1);
                    const std::size_t idx2 = static_cast<std::size_t>(i2);

                    const std::size_t base = build.vertices.size();

                    const std::int32_t vx0 = face.vertices[idx0];
                    const std::int32_t vx1 = face.vertices[idx1];
                    const std::int32_t vx2 = face.vertices[idx2];
                    const Vertex& faceLightAnchor =
                        xformedVertices[static_cast<std::size_t>(face.vertices.front())];
                    const float dynamicLightTarget = modelDynamicLightTarget;
                    const bool classicDynamicLighting = level_->kind != ProjectType::IJIM
                        && (dynamicVertexLighting || dynamicFaceLighting);
                    const ColorF* classicExtra = classicDynamicLighting ? &face.extraLight : nullptr;
                    const ColorF* classicAmbient = classicDynamicLighting ? &ambientLight : nullptr;

                    const auto normalFor = [&](const std::int32_t vertexIndex) -> const TVector*
                    {
                        if (dynamicFaceLighting)
                        {
                            // rdLight_CalcFaceIntensity uses the flat face
                            // normal. The fragment shader selects its negated
                            // counterpart for a rendered backface.
                            return &faceNormal;
                        }
                        const TVector& vertexNormal = xformedNormals[static_cast<std::size_t>(vertexIndex)];
                        return vertexNormal.IsZero() ? &faceNormal : &vertexNormal;
                    };

                    pushTriVertex(
                        build,
                        xformedVertices[static_cast<std::size_t>(vx0)],
                        faceTexVertices[idx0],
                        faceLitColors[idx0],
                        thingPickId,
                        pickGeo,
                        0.0f,
                        &faceShadeInputs[idx0],
                        normalFor(vx0),
                        dynamicLightTarget,
                        sectorPointLight,
                        owningSector,
                        dynamicFaceLighting ? &faceLightAnchor : nullptr,
                        classicExtra,
                        classicAmbient,
                        &editorSectorTintScale);

                    pushTriVertex(
                        build,
                        xformedVertices[static_cast<std::size_t>(vx1)],
                        faceTexVertices[idx1],
                        faceLitColors[idx1],
                        thingPickId,
                        pickGeo,
                        0.0f,
                        &faceShadeInputs[idx1],
                        normalFor(vx1),
                        dynamicLightTarget,
                        sectorPointLight,
                        owningSector,
                        dynamicFaceLighting ? &faceLightAnchor : nullptr,
                        classicExtra,
                        classicAmbient,
                        &editorSectorTintScale);

                    pushTriVertex(
                        build,
                        xformedVertices[static_cast<std::size_t>(vx2)],
                        faceTexVertices[idx2],
                        faceLitColors[idx2],
                        thingPickId,
                        pickGeo,
                        0.0f,
                        &faceShadeInputs[idx2],
                        normalFor(vx2),
                        dynamicLightTarget,
                        sectorPointLight,
                        owningSector,
                        dynamicFaceLighting ? &faceLightAnchor : nullptr,
                        classicExtra,
                        classicAmbient,
                        &editorSectorTintScale);

                    if (outlineOut)
                    {
                        if (base + 2 < build.vertices.size())
                        {
                            outlineOut->push_back(build.vertices[base]);
                            outlineOut->push_back(build.vertices[base + 1]);
                            outlineOut->push_back(build.vertices[base + 2]);
                        }
                    }
                });
            }
        }
    };

    if (buildThings && showThings_)
    {
        const std::size_t startIndex = incrementalThingAppend ? incrementalThingStartIndex : 0;
        for (std::size_t thingIndex = startIndex; thingIndex < level_->things.size(); ++thingIndex)
        {
            reportProgress();
            const Thing& thing = level_->things[thingIndex];
            if (!render::thing_overlay::shouldDrawThingOverlay(
                    showThings_, isLayerVisible(thing.layer), shouldShowThing(thing)))
            {
                continue;
            }

            if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
            {
                continue;
            }

            const auto thingIndex32 = static_cast<std::int32_t>(thingIndex);
            const std::uint32_t thingPickIdValue = registerThingPickId(thingIndex32);
            const float thingPickId = static_cast<float>(thingPickIdValue);

            TVector pyr{};
            pyr.pitch = thing.pch;
            pyr.yaw = thing.yaw;
            pyr.roll = thing.rol;
            const TMatrix43 thingXf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, pyr);
            const TplValue* modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3d");
            if (!modelValue)
            {
                modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3do");
            }
            const std::string modelName = modelValue
                ? (modelValue->stringValue.empty() ? modelValue->asString() : modelValue->stringValue)
                : std::string{};
            std::shared_ptr<const assets::Model3do> overlayModelPtr{};

            const render::thing_overlay::ThingOverlayGeometryPlan overlayPlan =
                render::thing_overlay::thingOverlayGeometryPlan(
                    showThingCollisionShapes_,
                    showThingVisualBounds_);
            if (overlayPlan.collisionShapes || overlayPlan.visualBounds
                || (showTemplateCollisionBoundsGuide_
                    && renderPipeline_ == RenderPipeline::GamePreview))
            {
                const auto collision = render::thing_overlay::resolveCollisionDescription(
                    *level_, templates_, thing);
                const int overlayOwner = static_cast<int>(thingPickIdValue);
                if (overlayPlan.collisionShapes
                    && collision.shape == render::thing_overlay::CollisionShape::sphere
                    && collision.hasRadius)
                {
                    std::vector<TVector> spherePoints =
                        render::thing_overlay::makeSphereWirePoints(collision.radius);
                    for (TVector& point : spherePoints)
                    {
                        point.x += thing.x;
                        point.y += thing.y;
                        point.z += thing.z;
                    }
                    appendOverlayWirePoints(
                        spherePoints, collisionOverlayRangesByPickId, overlayOwner);
                }
                if (overlayPlan.collisionShapes && collision.broadPhaseRadius)
                {
                    std::vector<TVector> broadPhasePoints =
                        render::thing_overlay::makeSphereWirePoints(*collision.broadPhaseRadius);
                    for (TVector& point : broadPhasePoints)
                    {
                        point = thingXf.TransformPoint(point);
                    }
                    appendOverlayWirePoints(
                        broadPhasePoints, broadPhaseTreeGuideRangesByPickId, overlayOwner);
                }

                std::optional<double> spriteRadius{};
                std::optional<render::thing_overlay::Bounds3d> spriteBounds{};
                if (overlayPlan.visualBounds)
                {
                    const TplValue* spriteValue =
                        sed::level::thingFindValue(*level_, templates_, thing, "sprite");
                    if (spriteValue)
                    {
                        const std::string spriteName = spriteValue->stringValue.empty()
                            ? spriteValue->asString()
                            : spriteValue->stringValue;
                        if (!spriteName.empty())
                        {
                            // Match the rendered SPR policy: classic projects
                            // resolve the original engine's frame policy before
                            // using the same sprite geometry as the card renderer.
                            if (const assets::SprFile* sprite = loadSprite(spriteName);
                                sprite && (originalSpriteRenderType(level_->kind, sprite->type) != 2
                                    || level_->kind == ProjectType::IJIM))
                            {
                                const int spriteType = originalSpriteRenderType(level_->kind, sprite->type);
                                const bool exactSpriteFrame =
                                    (spriteType == 0
                                        && renderPipeline_ == RenderPipeline::Editor)
                                    || spriteType == 2;
                                if (exactSpriteFrame)
                                {
                                    TVector placementNoRoll = pyr;
                                    placementNoRoll.roll = 0.0;
                                    const SpriteBasis spriteBasis = makeSpriteBasis(placementNoRoll);

                                    SpriteInstance boundsInstance{};
                                    boundsInstance.type = spriteType;
                                    boundsInstance.width = sprite->width;
                                    boundsInstance.height = sprite->height;
                                    boundsInstance.rollDeg = normalizeRollDeg(
                                        static_cast<float>(thing.rol));
                                    boundsInstance.offsetXYZ = QVector3D(
                                        sprite->offsetXYZ[0],
                                        sprite->offsetXYZ[1],
                                        sprite->offsetXYZ[2]);
                                    boundsInstance.position = QVector3D(
                                        static_cast<float>(thing.x),
                                        static_cast<float>(thing.y),
                                        static_cast<float>(thing.z));
                                    boundsInstance.basisRight = spriteBasis.right;
                                    boundsInstance.basisForward = spriteType == 2
                                        ? makeSpriteType2Forward(pyr)
                                        : spriteBasis.forward;
                                    boundsInstance.basisUp = spriteBasis.up;
                                    boundsInstance.pickId = thingPickId;
                                    boundsInstance.pickGeo =
                                        static_cast<float>(TGeoMode::Texture);

                                    std::array<TriVertex, 6> spriteVertices{};
                                    if (buildSpriteInstanceVertices(boundsInstance, spriteVertices))
                                    {
                                        const auto worldPoint = [](const TriVertex& vertex)
                                        {
                                            return TVector{
                                                static_cast<double>(vertex.x),
                                                static_cast<double>(vertex.y),
                                                static_cast<double>(vertex.z),
                                            };
                                        };
                                        spriteBounds = render::thing_overlay::spriteQuadWorldBounds(
                                            std::array<TVector, 4>{
                                                worldPoint(spriteVertices[0]),
                                                worldPoint(spriteVertices[1]),
                                                worldPoint(spriteVertices[2]),
                                                worldPoint(spriteVertices[5]),
                                            });
                                    }
                                }
                                else
                                {
                                    // Type-0 Game Preview billboards change with orbit. Keep
                                    // their bounds camera-independent until that cache can be
                                    // refreshed with each billboard frame.
                                    spriteRadius = render::thing_overlay::spriteBillboardConservativeRadius(
                                        sprite->width,
                                        sprite->height,
                                        std::array<double, 3>{
                                            sprite->offsetXYZ[0],
                                            sprite->offsetXYZ[1],
                                            sprite->offsetXYZ[2],
                                        });
                                }
                            }
                        }
                    }
                }

                const bool needModelForCollision =
                    overlayPlan.collisionShapes
                    && (collision.shape == render::thing_overlay::CollisionShape::face
                        || collision.shape == render::thing_overlay::CollisionShape::sphereTree);
                const bool needModelForBounds =
                    overlayPlan.visualBounds && !spriteRadius && !spriteBounds;
                if ((needModelForCollision || needModelForBounds) && !modelName.empty())
                {
                    overlayModelPtr = loadModel3do(modelName);
                }
                if (overlayPlan.collisionShapes
                    && collision.shape == render::thing_overlay::CollisionShape::sphereTree
                    && overlayModelPtr)
                {
                    std::vector<TVector> sphereTreePoints{};
                    const std::size_t sphereCount =
                        render::thing_overlay::appendSphereTreeWirePoints(
                            sphereTreePoints,
                            *overlayModelPtr,
                            thingXf,
                            motsTreeCollisionPreviewMeshRadiusFactor_);
                    if (sphereCount > 0u)
                    {
                        appendOverlayWirePoints(
                            sphereTreePoints, collisionOverlayRangesByPickId, overlayOwner);
                    }
                }

                if (needModelForCollision && overlayModelPtr
                    && collision.shape == render::thing_overlay::CollisionShape::face)
                {
                    std::vector<TVector> facePoints{};
                    render::thing_overlay::appendModelFaceWirePoints(
                        facePoints, *overlayModelPtr, thingXf);
                    appendOverlayWirePoints(
                        facePoints, collisionOverlayRangesByPickId, overlayOwner);
                }

                if (overlayPlan.visualBounds)
                {
                    if (spriteBounds)
                    {
                        const auto spritePoints = render::thing_overlay::makeBoundsWirePoints(
                            *spriteBounds, TMatrix43::identity());
                        appendOverlayWirePoints(
                            std::vector<TVector>(spritePoints.begin(), spritePoints.end()),
                            visualBoundsOverlayRangesByPickId,
                            overlayOwner);
                    }
                    else if (spriteRadius)
                    {
                        const double radius = *spriteRadius;
                        const render::thing_overlay::Bounds3d billboardBounds{
                            TVector{ -radius, -radius, -radius },
                            TVector{ radius, radius, radius },
                        };
                        // This enclosing cube is independent of camera orbit, so it remains
                        // correct while a camera-facing sprite rotates in place.
                        const auto spritePoints = render::thing_overlay::makeBoundsWirePoints(
                            billboardBounds, thingXf);
                        appendOverlayWirePoints(
                            std::vector<TVector>(spritePoints.begin(), spritePoints.end()),
                            visualBoundsOverlayRangesByPickId,
                            overlayOwner);
                    }
                    else if (overlayModelPtr)
                    {
                        if (const auto bounds =
                                render::thing_overlay::modelLocalVertexBounds(*overlayModelPtr))
                        {
                            const auto boundsPoints =
                                render::thing_overlay::makeBoundsWirePoints(*bounds, thingXf);
                            appendOverlayWirePoints(
                                std::vector<TVector>(boundsPoints.begin(), boundsPoints.end()),
                                visualBoundsOverlayRangesByPickId,
                                overlayOwner);
                        }
                    }
                }

                if (showTemplateCollisionBoundsGuide_
                    && renderPipeline_ == RenderPipeline::GamePreview)
                {
                    if (const auto guide =
                            render::thing_overlay::resolveIjimAuthoredCollisionBoundsGuide(
                                *level_, templates_, thing))
                    {
                        auto guidePoints =
                            render::thing_overlay::makeAuthoredCollisionBoundsGuideWirePoints(
                                *guide, thingXf);
                        appendOverlayWirePoints(
                            guidePoints,
                            templateCollisionBoundsGuideRangesByPickId,
                            overlayOwner);
                    }
                }
            }

            if (thingViewMode_ == ThingViewMode::Dots)
            {
                const float x = static_cast<float>(thing.x);
                const float y = static_cast<float>(thing.y);
                const float z = static_cast<float>(thing.z);
                thingPointPositions.push_back(x);
                thingPointPositions.push_back(y);
                thingPointPositions.push_back(z);
                extendThing(x, y, z);
                continue;
            }

            std::vector<LineVertex>& positions = thingLinePositions;

            if (thingViewMode_ == ThingViewMode::Boxes)
            {
                // Check if this thing has a renderable sprite; if so, fall through
                // to the sprite handling below instead of drawing a box.
                bool hasSpriteToRender = false;
                {
                    const TplValue* sv = sed::level::thingFindValue(*level_, templates_, thing, "sprite");
                    if (sv)
                    {
                        const std::string sn = sv->stringValue.empty() ? sv->asString() : sv->stringValue;
                        if (!sn.empty())
                        {
                            if (const assets::SprFile* sp = loadSprite(sn))
                            {
                                hasSpriteToRender = originalSpriteRenderType(level_->kind, sp->type) != 2
                                    || level_->kind == ProjectType::IJIM;
                            }
                        }
                    }
                }

                if (!hasSpriteToRender)
                {
                    const ThingBox box = resolveThingBox(thing);

                    if (box.IsZero())
                    {
                        if (renderPipeline_ == RenderPipeline::GamePreview)
                        {
                            if (!showPreviewCubes_)
                            {
                                continue;
                            }
                        }
                        else
                        {
                            if (!showEditorCubes_)
                            {
                                continue;
                            }
                        }

                        const std::string typeLower = resolveThingTypeNameLower(thing);
                        addThingCube(positions, thingXf, thingCubeHalfSize_, thingPickId, typeLower);
                        continue;
                    }

                    addBox(positions, thingXf, box, thingPickId);
                    continue;
                }

                // Otherwise, fall through to sprite handling below.
            }

            const TplValue* spriteValue = sed::level::thingFindValue(*level_, templates_, thing, "sprite");
            if (spriteValue)
            {
                const std::string spriteName =
                    spriteValue->stringValue.empty() ? spriteValue->asString() : spriteValue->stringValue;

                if (!spriteName.empty())
                {
                    if (const assets::SprFile* spr = loadSprite(spriteName))
                    {
                        const int spriteType = originalSpriteRenderType(level_->kind, spr->type);
                        const bool type2Supported = (spriteType != 2) || (level_->kind == ProjectType::IJIM);
                        if (type2Supported)
                        {
                            const float rollDeg = normalizeRollDeg(static_cast<float>(thing.rol));

                        TVector placementNoRoll = pyr;
                        placementNoRoll.roll = 0.0;
                        const SpriteBasis basis = makeSpriteBasis(placementNoRoll);
                        const QVector3D type2Forward = makeSpriteType2Forward(pyr);

                        const std::string masterCmp = level_->getMasterCMP();
                        // Keep sprite texture state isolated from surface texture state
                        // (sampler settings differ, especially for alpha billboards).
                        const std::string batchKey = std::string("spr|") + makeBatchKey(spr->material, masterCmp);

                        auto findOrAddBatch = [&](const std::string_view material, const std::string_view colormap) -> TriBatch&
                        {
                            for (TriBatch& b : spriteTriBatches_)
                            {
                                if (b.key == batchKey)
                                {
                                    return b;
                                }
                            }

                            TriBatch batch{};
                            batch.kind = TriKind::Thing;
                            batch.key = batchKey;
                            batch.material = std::string(material);
                            batch.colormap = std::string(colormap);
                            batch.firstVertex = 0;
                            batch.vertexCount = 0;
                            batch.uvNormalized = true;
                            batch.skyGroup = 0;
                            batch.doubleSided = true;
                            batch.isSprite = true;
                            spriteTriBatches_.push_back(std::move(batch));
                            return spriteTriBatches_.back();
                        };

                        (void)findOrAddBatch(spr->material, masterCmp);

                        SpriteInstance inst{};
                        inst.key = batchKey;
                        inst.material = spr->material;
                        inst.colormap = masterCmp;
                        inst.uvNormalized = true;
                        inst.type = spriteType;
                        inst.lightMode = spr->lightMode;
                        inst.sectorIndex = thing.sector;
                        if (transformPreviewActive_)
                        {
                            if (const auto it = previewThingSectors_.find(thingIndex32); it != previewThingSectors_.end())
                            {
                                inst.sectorIndex = it->second;
                            }
                        }
                        inst.width = spr->width;
                        inst.height = spr->height;
                        inst.rollDeg = rollDeg;
                        inst.extraLightRGBA = QVector4D(
                            spr->extraLightRGBA[0],
                            spr->extraLightRGBA[1],
                            spr->extraLightRGBA[2],
                            spr->extraLightRGBA[3]);
                        const render::lighting::RuntimeThingLight spriteSelfLight =
                            render::lighting::resolveRuntimeThingLight(
                                *level_,
                                templates_,
                                thing,
                                thingIndex32,
                                inst.sectorIndex,
                                render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
                        if ((level_->kind == ProjectType::IJIM
                                && spriteSelfLight.classification == render::lighting::ThingLightClass::Flat)
                            || (level_->kind != ProjectType::IJIM
                                && spriteSelfLight.classification == render::lighting::ThingLightClass::Dynamic))
                        {
                            inst.regularSelfLight = spriteSelfLight.color;
                        }
                        inst.offsetXYZ = QVector3D(spr->offsetXYZ[0], spr->offsetXYZ[1], spr->offsetXYZ[2]);
                        inst.position = QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));
                        inst.basisRight = basis.right;
                        inst.basisForward = basis.forward;
                        inst.basisUp = basis.up;
                        if (inst.type == 2)
                        {
                            inst.basisForward = type2Forward;
                        }
                        inst.pickId = thingPickId;
                        inst.pickGeo = static_cast<float>(TGeoMode::Texture);
                        if ( level_->kind != ProjectType::IJIM
                            && property("spr.preview.isolated_resource").toBool() )
                        {
                            // The classic engine caps geometry by the SPR
                            // face. The isolated view keeps a six-vertex card
                            // for every mode; its shader draws corner/wire
                            // coverage without rebuilding topology per scrub.
                            inst.pickGeo = static_cast<float>(std::min(
                                spr->geometryMode, static_cast<int>(TGeoMode::Texture)));
                        }
                        if (inst.type == 0)
                        {
                            spriteHasType0Billboards_ = true;
                        }
                        spriteInstances_.push_back(std::move(inst));

                        if (thingViewMode_ != ThingViewMode::Shaded)
                        {
                            QVector3D center(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));
                            QVector3D right{};
                            QVector3D up{};
                            QVector3D forward{};

                            if (spriteType == 0)
                            {
                                const bool billboard =
                                    render::fast_update::usesOriginalSpriteCameraFacingFrame(
                                        spriteType,
                                        renderPipeline_ == RenderPipeline::GamePreview);
                                const SpriteBasis displayBasis = billboard
                                    ? makeCameraFacingSpriteBasis(camera_.orbit)
                                    : basis;
                                right = displayBasis.right;
                                up = displayBasis.up;
                                forward = displayBasis.forward;

                                if (rollDeg != 0.0f)
                                {
                                    const float rollRad = DegToRad(rollDeg);
                                    right = rotateAroundAxis(right, forward, rollRad);
                                    up = rotateAroundAxis(up, forward, rollRad);
                                }

                                center =
                                    center
                                    + right * spr->offsetXYZ[0]
                                    + forward * spr->offsetXYZ[1]
                                    + up * spr->offsetXYZ[2];
                            }
                            else if (spriteType == 2)
                            {
                                QVector3D yVec = type2Forward;
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
                                    continue;
                                }

                                QVector3D zVec = QVector3D::crossProduct(xVec, yVec);
                                if (!normalizeInPlace(zVec))
                                {
                                    continue;
                                }

                                const float widthHalf = spr->width * 0.5f;
                                const float heightHalf = spr->height * 0.5f;
                                const QVector3D rightHalf = xVec * widthHalf;
                                const QVector3D upHalf = zVec * heightHalf;
                                const QVector3D leftHalf = -rightHalf;

                                QVector3D v0 = leftHalf - upHalf;
                                QVector3D v1 = rightHalf - upHalf;
                                QVector3D v2 = rightHalf + upHalf;
                                QVector3D v3 = leftHalf + upHalf;

                                if (rollDeg != 0.0f)
                                {
                                    const float rollRad = DegToRad(rollDeg);
                                    v0 = rotateAroundAxis(v0, yVec, rollRad);
                                    v1 = rotateAroundAxis(v1, yVec, rollRad);
                                    v2 = rotateAroundAxis(v2, yVec, rollRad);
                                    v3 = rotateAroundAxis(v3, yVec, rollRad);
                                }

                                const QVector3D p0 = center + v0;
                                const QVector3D p1 = center + v1;
                                const QVector3D p2 = center + v2;
                                const QVector3D p3 = center + v3;

                                const auto addEdge = [&](const QVector3D& a, const QVector3D& b)
                                {
                                    pushLinePoint(positions, static_cast<double>(a.x()), static_cast<double>(a.y()), static_cast<double>(a.z()), thingPickId);
                                    pushLinePoint(positions, static_cast<double>(b.x()), static_cast<double>(b.y()), static_cast<double>(b.z()), thingPickId);
                                };

                                addEdge(p0, p1);
                                addEdge(p1, p2);
                                addEdge(p2, p3);
                                addEdge(p3, p0);
                                continue;
                            }

                            const float widthHalf = spr->width * 0.5f;
                            const float heightHalf = spr->height * 0.5f;

                            const QVector3D r = right * widthHalf;
                            const QVector3D u = up * heightHalf;

                            const QVector3D p0 = center - r - u;
                            const QVector3D p1 = center + r - u;
                            const QVector3D p2 = center + r + u;
                            const QVector3D p3 = center - r + u;

                            const auto addEdge = [&](const QVector3D& a, const QVector3D& b)
                            {
                                pushLinePoint(positions, static_cast<double>(a.x()), static_cast<double>(a.y()), static_cast<double>(a.z()), thingPickId);
                                pushLinePoint(positions, static_cast<double>(b.x()), static_cast<double>(b.y()), static_cast<double>(b.z()), thingPickId);
                            };

                            addEdge(p0, p1);
                            addEdge(p1, p2);
                            addEdge(p2, p3);
                            addEdge(p3, p0);
                        }

                        continue;
                    }
                }
            }
        }

        const std::shared_ptr<const assets::Model3do> hqModelPtr = overlayModelPtr
            ? overlayModelPtr
            : loadModel3do(modelName);
        std::shared_ptr<const assets::Model3do> modelPtr = hqModelPtr;
        if (hqModelPtr && !hqModelPtr->meshes.empty()
            && !builtThingGeosetCounts.empty())
        {
            constexpr std::int32_t kArchThingFlag = 0x00000008;
            const bool archThing =
                (sed::level::thingFlags(*level_, templates_, thing)
                    & kArchThingFlag) != 0;
            const std::size_t lodIndex = archThing
                ? 0u
                : selectedThingModelLod(
                    hqModelPtr->geosetCount, thing, *lodView);
            builtThingGeosetCounts[thingIndex] = hqModelPtr->geosetCount;
            builtThingRequestedLodIndices[thingIndex] = lodIndex;
            builtThingLodIndices[thingIndex] = lodIndex;
            builtThingArchFlags[thingIndex] = archThing ? 1u : 0u;
            if (lodIndex > 0u)
            {
                const std::shared_ptr<const assets::Model3do> lodModel =
                    loadModel3do(modelName, lodIndex);
                // Damaged lower-detail geosets must not make an otherwise
                // valid Thing disappear from the editor.
                if (lodModel && !lodModel->meshes.empty())
                {
                    modelPtr = lodModel;
                }
                else
                {
                    builtThingLodIndices[thingIndex] = 0u;
                }
            }
        }
        if (!modelPtr || modelPtr->meshes.empty())
        {
            const ThingBox box = resolveThingBox(thing);
            if (!box.IsZero())
            {
                addBox(positions, thingXf, box, thingPickId);
            }
            else
            {
                const bool allowCube =
                    (renderPipeline_ == RenderPipeline::GamePreview) ? showPreviewCubes_ : showEditorCubes_;
                if (allowCube)
                {
                    const std::string typeLower = resolveThingTypeNameLower(thing);
                    addThingCube(positions, thingXf, thingCubeHalfSize_, thingPickId, typeLower);
                }
            }
            continue;
        }

        if (thingViewMode_ == ThingViewMode::Vertices)
        {
            for (const assets::Model3doMesh& mesh : modelPtr->meshes)
            {
                for (const assets::Model3doVertex& vertex : mesh.vertices)
                {
                    const TVector point = thingXf.TransformPoint(
                        TVector{ vertex.x, vertex.y, vertex.z });
                    if (!std::isfinite(point.x)
                        || !std::isfinite(point.y)
                        || !std::isfinite(point.z))
                    {
                        continue;
                    }
                    const float px = static_cast<float>(point.x);
                    const float py = static_cast<float>(point.y);
                    const float pz = static_cast<float>(point.z);
                    thingPointPositions.push_back(px);
                    thingPointPositions.push_back(py);
                    thingPointPositions.push_back(pz);
                    extendThing(px, py, pz);
                }
            }
        }

        // Keep 3DO triangles in memory for GPU picking, regardless of view mode.
        std::int32_t lightSector = thing.sector;
        if (transformPreviewActive_)
        {
            if (const auto it = previewThingSectors_.find(thingIndex32); it != previewThingSectors_.end())
            {
                lightSector = it->second;
            }
        }
        const ThingSectorLighting lights = calcThingSectorLighting(level_, lightSector);
        ColorF thingAmbient = lights.ambientLight;
        const render::lighting::RuntimeThingLight selfLight = render::lighting::resolveRuntimeThingLight(
            *level_,
            templates_,
            thing,
            thingIndex32,
            lightSector,
            render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
        if constexpr (render::lighting::kRuntimeDynamicLightPreviewEnabled)
        {
            thingAmbient = render::lighting::applyRuntimeThingSelfLight(
                level_->kind,
                thingAmbient,
                selfLight,
                thingLightRenderingEnabled_);
        }

        const PointLight* sectorPointLight = nullptr;
        if (level_->kind == ProjectType::IJIM
            && lightSector >= 0
            && static_cast<std::size_t>(lightSector) < level_->sectors.size())
        {
            sectorPointLight = &level_->sectors[static_cast<std::size_t>(lightSector)].pointLight;
        }

        const MotsArchLightObject* archLighting = render::resolveMotsArchLightingPreview(
            *level_, templates_, thing, *modelPtr);
        std::optional<MotsArchLightObject> defaultArchLighting{};
        if (!archLighting && thing.archLightingIndex == -1
            && sed::level::isMotsArchThing(
                *level_, thing, templates_))
        {
            defaultArchLighting =
                render::makeDefaultMotsArchLightingPreview(*modelPtr);
            if (defaultArchLighting)
            {
                archLighting = &*defaultArchLighting;
            }
        }

        addModelTriangles(
            *modelPtr,
            thingXf,
            thingIndex32,
            thingPickId,
            thingAmbient,
            nullptr,
            nullptr,
            archLighting,
            sectorPointLight,
            lightSector);

        if (thingViewMode_ == ThingViewMode::Wireframes)
        {
            addModelWireframe(positions, *modelPtr, thingXf, thingPickId);
        }
    }
    }

    if (buildThings)
    {
        // The selected-owner cache is rebuilt from the same model map and
        // transform inputs used above. It stays one-owner wide and performs
        // no resource read on the interactive accessor path.
        refreshArchitecturalLightingRenderCache();
    }

    if (buildThings)
    {
        placementPreviewRenderRanges_.clear();
        placementPreviewLodStates_.clear();
        placementPreviewRenderedThings_.clear();
        placementPreviewLightingSignatures_.clear();
        placementPreviewSpriteIndices_.clear();
        placementPreviewSpriteBatchFirst_ = spriteTriBatches_.size();
        placementPreviewRenderedInvalid_ = placementPreviewThingInvalid_;
        placementPreviewRenderStateValid_ = false;
    }

    if (buildThings && (placementPreviewThing_ || !placementPreviewThings_.empty()))
    {
        auto addPlacementPreviewThing = [&](const Thing& thing)
        {
            if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
            {
                return;
            }

            TVector pyr{};
            pyr.pitch = thing.pch;
            pyr.yaw = thing.yaw;
            pyr.roll = thing.rol;
            const TMatrix43 thingXf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, pyr);
            constexpr float previewPickId = 0.0f;
            constexpr std::int32_t previewThingIndex = -1;
            const bool invalidPreviewThing = placementPreviewThingInvalid_;
            const ColorF invalidPreviewColor{ 1.0f, 0.16f, 0.12f, 0.45f };

            bool renderedPreviewThing = false;

            const TplValue* spriteValue = sed::level::thingFindValue(*level_, templates_, thing, "sprite");
            if (spriteValue)
            {
                const std::string spriteName =
                    spriteValue->stringValue.empty() ? spriteValue->asString() : spriteValue->stringValue;
                if (!spriteName.empty())
                {
                    if (const assets::SprFile* spr = loadSprite(spriteName))
                    {
                        const int spriteType = originalSpriteRenderType(level_->kind, spr->type);
                        const bool type2Supported = (spriteType != 2) || (level_->kind == ProjectType::IJIM);
                        if (type2Supported)
                        {
                            const float rollDeg = normalizeRollDeg(static_cast<float>(thing.rol));

                            TVector placementNoRoll = pyr;
                            placementNoRoll.roll = 0.0;
                            const SpriteBasis basis = makeSpriteBasis(placementNoRoll);
                            const QVector3D type2Forward = makeSpriteType2Forward(pyr);

                            const std::string masterCmp = level_->getMasterCMP();
                            // Keep transient cards out of the persistent
                            // material batch. Cursor motion then refits only
                            // the placement/brush preview sprites rather than
                            // every placed sprite using the same MAT.
                            const std::string batchKey = std::string("spr-preview|")
                                + makeBatchKey(spr->material, masterCmp);

                            auto findOrAddBatch = [&](const std::string_view material, const std::string_view colormap) -> TriBatch&
                            {
                                for (TriBatch& b : spriteTriBatches_)
                                {
                                    if (b.key == batchKey)
                                    {
                                        return b;
                                    }
                                }

                                TriBatch batch{};
                                batch.kind = TriKind::Thing;
                                batch.key = batchKey;
                                batch.material = std::string(material);
                                batch.colormap = std::string(colormap);
                                batch.firstVertex = 0;
                                batch.vertexCount = 0;
                                batch.uvNormalized = true;
                                batch.skyGroup = 0;
                                batch.doubleSided = true;
                                batch.isSprite = true;
                                spriteTriBatches_.push_back(std::move(batch));
                                return spriteTriBatches_.back();
                            };

                            (void)findOrAddBatch(spr->material, masterCmp);

                            SpriteInstance inst{};
                            inst.key = batchKey;
                            inst.material = spr->material;
                            inst.colormap = masterCmp;
                            inst.uvNormalized = true;
                            inst.type = spriteType;
                            inst.lightMode = spr->lightMode;
                            inst.sectorIndex = thing.sector;
                            inst.width = spr->width;
                            inst.height = spr->height;
                            inst.rollDeg = rollDeg;
                            inst.extraLightRGBA = QVector4D(
                                spr->extraLightRGBA[0],
                                spr->extraLightRGBA[1],
                                spr->extraLightRGBA[2],
                                spr->extraLightRGBA[3]);
                            const render::lighting::RuntimeThingLight spriteSelfLight =
                                render::lighting::resolveRuntimeThingLight(
                                    *level_,
                                    templates_,
                                    thing,
                                    previewThingIndex,
                                    std::nullopt,
                                    render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
                            if ((level_->kind == ProjectType::IJIM
                                    && spriteSelfLight.classification == render::lighting::ThingLightClass::Flat)
                                || (level_->kind != ProjectType::IJIM
                                    && spriteSelfLight.classification == render::lighting::ThingLightClass::Dynamic))
                            {
                                inst.regularSelfLight = spriteSelfLight.color;
                            }
                            if (invalidPreviewThing)
                            {
                                inst.extraLightRGBA.setW(std::min(inst.extraLightRGBA.w(), 0.45f));
                            }
                            inst.offsetXYZ = QVector3D(spr->offsetXYZ[0], spr->offsetXYZ[1], spr->offsetXYZ[2]);
                            inst.position = QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));
                            inst.basisRight = basis.right;
                            inst.basisForward = basis.forward;
                            inst.basisUp = basis.up;
                            inst.invalidPlacementPreview = invalidPreviewThing;
                            inst.placementPreviewIndex = static_cast<std::int32_t>(
                                placementPreviewRenderRanges_.size());
                            if (inst.type == 2)
                            {
                                inst.basisForward = type2Forward;
                            }
                            inst.pickId = previewPickId;
                            inst.pickGeo = static_cast<float>(TGeoMode::Texture);
                            if (inst.type == 0)
                            {
                                spriteHasType0Billboards_ = true;
                            }
                            spriteInstances_.push_back(std::move(inst));
                            renderedPreviewThing = true;
                        }
                    }
                }
            }

            if (!renderedPreviewThing)
            {
                const TplValue* modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3d");
                if (!modelValue)
                {
                    modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3do");
                }

                const std::string modelName =
                    modelValue ? (modelValue->stringValue.empty() ? modelValue->asString() : modelValue->stringValue) : std::string{};
                std::shared_ptr<const assets::Model3do> modelPtr =
                    loadModel3do(modelName);
                if (modelPtr && !modelPtr->meshes.empty() && lodView)
                {
                    PlacementPreviewLodState& state =
                        placementPreviewLodStates_.back();
                    constexpr std::int32_t kArchThingFlag = 0x00000008;
                    state.geosetCount = modelPtr->geosetCount;
                    state.archThing =
                        (sed::level::thingFlags(*level_, templates_, thing)
                            & kArchThingFlag) != 0;
                    if (!state.archThing)
                    {
                        state.requestedLodIndex =
                            selectedThingModelLod(
                                state.geosetCount, thing, *lodView);
                        if (state.requestedLodIndex > 0u)
                        {
                            const auto lower = loadModel3do(
                                modelName, state.requestedLodIndex);
                            if (lower && !lower->meshes.empty())
                            {
                                modelPtr = lower;
                                state.renderedLodIndex =
                                    state.requestedLodIndex;
                            }
                        }
                    }
                }
                if (modelPtr && !modelPtr->meshes.empty())
                {
                    const ThingSectorLighting lights = calcThingSectorLighting(level_, thing.sector);
                    ColorF thingAmbient = lights.ambientLight;
                    const render::lighting::RuntimeThingLight selfLight = render::lighting::resolveRuntimeThingLight(
                        *level_,
                        templates_,
                        thing,
                        previewThingIndex,
                        std::nullopt,
                        render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
                    if constexpr (render::lighting::kRuntimeDynamicLightPreviewEnabled)
                    {
                        thingAmbient = render::lighting::applyRuntimeThingSelfLight(
                            level_->kind,
                            thingAmbient,
                            selfLight,
                            thingLightRenderingEnabled_);
                    }
                    const PointLight* sectorPointLight = nullptr;
                    if (level_->kind == ProjectType::IJIM
                        && thing.sector >= 0
                        && static_cast<std::size_t>(thing.sector) < level_->sectors.size())
                    {
                        sectorPointLight = &level_->sectors[static_cast<std::size_t>(thing.sector)].pointLight;
                    }
                    const MotsArchLightObject* archLighting = render::resolveMotsArchLightingPreview(
                        *level_, templates_, thing, *modelPtr);
                    std::optional<MotsArchLightObject> defaultArchLighting{};
                    if (!archLighting && thing.archLightingIndex == -1
                        && sed::level::isMotsArchThing(
                            *level_, thing, templates_))
                    {
                        defaultArchLighting =
                            render::makeDefaultMotsArchLightingPreview(*modelPtr);
                        if (defaultArchLighting)
                        {
                            archLighting = &*defaultArchLighting;
                        }
                    }
                    addModelTriangles(
                        *modelPtr,
                        thingXf,
                        previewThingIndex,
                        previewPickId,
                        thingAmbient,
                        nullptr,
                        invalidPreviewThing ? &invalidPreviewColor : nullptr,
                        archLighting,
                        sectorPointLight,
                        thing.sector);
                    renderedPreviewThing = true;
                }
            }

            if (!renderedPreviewThing)
            {
                std::vector<LineVertex>& positions = thingLinePositions;
                const ThingBox box = resolveThingBox(thing);
                if (!box.IsZero())
                {
                    addBox(positions, thingXf, box, previewPickId);
                }
                else
                {
                    const bool allowCube =
                        (renderPipeline_ == RenderPipeline::GamePreview) ? showPreviewCubes_ : showEditorCubes_;
                    if (allowCube)
                    {
                        const std::string typeLower = resolveThingTypeNameLower(thing);
                        addThingCube(
                            positions,
                            thingXf,
                            thingCubeHalfSize_,
                            previewPickId,
                            typeLower,
                            invalidPreviewThing ? invalidPreviewColor : ColorWhite);
                    }
                }
            }

            (void)renderedPreviewThing;
        };

        const std::size_t cachedThingBuildCount =
            incrementalThingAppend ? thingMeshCache_.triBuilds.size() : 0;
        const std::size_t newThingBuildCount = static_cast<std::size_t>(std::count_if(
            builds.begin(), builds.end(),
            [](const BatchBuild& build) { return build.kind == TriKind::Thing; }));
        std::size_t nextThingBuild = cachedThingBuildCount + newThingBuildCount;

        auto addRecordedPlacementPreviewThing = [&](const Thing& thing)
        {
            // Do not merge preview triangles with persistent Thing batches. Keeping
            // each preview's builds contiguous lets cursor motion update only its
            // recorded tail ranges without touching the rest of the level.
            buildIndices.clear();

            PlacementPreviewRenderRange range{};
            range.triBuildFirst = nextThingBuild;
            range.lineFirst = thingLinePositions.size();
            range.spriteFirst = spriteInstances_.size();
            const std::size_t buildStart = builds.size();

            placementPreviewLodStates_.emplace_back();
            addPlacementPreviewThing(thing);

            range.triBuildCount = builds.size() - buildStart;
            range.lineCount = thingLinePositions.size() - range.lineFirst;
            range.spriteCount = spriteInstances_.size() - range.spriteFirst;
            nextThingBuild += range.triBuildCount;
            placementPreviewRenderRanges_.push_back(range);
            placementPreviewRenderedThings_.push_back(thing);
            placementPreviewLightingSignatures_.push_back(
                placementPreviewLightingSignature(thing));
        };

        if (placementPreviewThing_)
        {
            addRecordedPlacementPreviewThing(*placementPreviewThing_);
        }
        for (const Thing& thing : placementPreviewThings_)
        {
            addRecordedPlacementPreviewThing(thing);
        }

        // Publish a usable transient-sprite owner map even before an OpenGL
        // upload sorts instances by material. rebuildSpriteVertices refreshes
        // these indices after that sort when a context is available.
        placementPreviewSpriteIndices_.assign(
            placementPreviewRenderedThings_.size(), {});
        for (std::size_t spriteIndex = 0; spriteIndex < spriteInstances_.size(); ++spriteIndex)
        {
            const std::int32_t previewIndex =
                spriteInstances_[spriteIndex].placementPreviewIndex;
            if (previewIndex >= 0
                && static_cast<std::size_t>(previewIndex) < placementPreviewSpriteIndices_.size())
            {
                placementPreviewSpriteIndices_[static_cast<std::size_t>(previewIndex)].push_back(spriteIndex);
            }
        }
    }

    struct Bounds
    {
        QVector3D min{};
        QVector3D max{};
        QVector3D center{};
        bool valid{};
    };

    auto calcBounds = [&reportProgress](
        const std::vector<TriVertex>& vertices) -> Bounds
    {
        Bounds out{};

        QVector3D bmin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
        QVector3D bmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
        bool have = false;
        for (const TriVertex& v : vertices)
        {
            reportProgress();
            bmin.setX(std::min(bmin.x(), v.x));
            bmin.setY(std::min(bmin.y(), v.y));
            bmin.setZ(std::min(bmin.z(), v.z));
            bmax.setX(std::max(bmax.x(), v.x));
            bmax.setY(std::max(bmax.y(), v.y));
            bmax.setZ(std::max(bmax.z(), v.z));
            have = true;
        }

        if (!have)
        {
            out.min = QVector3D(0.0f, 0.0f, 0.0f);
            out.max = QVector3D(0.0f, 0.0f, 0.0f);
            out.center = QVector3D(0.0f, 0.0f, 0.0f);
            out.valid = false;
            return out;
        }

        out.min = bmin;
        out.max = bmax;
        out.center = (bmin + bmax) * 0.5f;
        out.valid = true;
        return out;
    };

    // Commit rebuilt parts into caches.
    reportProgress();
    if (buildLevel)
    {
        levelMeshCache_.lineVertices = std::move(levelLinePositions);
        levelMeshCache_.pointPositions = std::move(levelPointPositions);
        levelMeshCache_.triBuilds.clear();
        levelMeshCache_.min = levelMin;
        levelMeshCache_.max = levelMax;
        levelMeshCache_.haveAny = levelHaveAny;
        levelMeshCache_.valid = true;
    }
    if (buildThings)
    {
        thingMeshCache_.lineVertices = std::move(thingLinePositions);
        thingMeshCache_.overlayLineVertices = std::move(thingOverlayLinePositions);
        thingMeshCache_.pointPositions = std::move(thingPointPositions);
        if (!incrementalThingAppend)
        {
            thingMeshCache_.triBuilds.clear();
        }
        thingMeshCache_.min = thingMin;
        thingMeshCache_.max = thingMax;
        thingMeshCache_.haveAny = thingHaveAny;
        thingMeshCache_.valid = true;
    }

    for (BatchBuild& build : builds)
    {
        reportProgress();
        TriBuild out{};
        out.kind = build.kind;
        out.key = std::move(build.key);
        out.material = std::move(build.material);
        out.colormap = std::move(build.colormap);
        out.uvNormalized = build.uvNormalized;
        out.skyGroup = build.skyGroup;
        out.doubleSided = build.doubleSided;
        out.faceFlags = build.faceFlags;
        out.vertices = std::move(build.vertices);
        out.shadeInputs = std::move(build.shadeInputs);
        const Bounds bounds = calcBounds(out.vertices);
        out.boundsMin = bounds.min;
        out.boundsMax = bounds.max;
        out.boundsCenter = bounds.center;
        out.minAlpha = 1.0f;
        for (const TriVertex& v : out.vertices)
        {
            reportProgress();
            out.minAlpha = std::min(out.minAlpha, v.a);
        }

        if (out.kind == TriKind::Level)
        {
            if (buildLevel)
            {
                levelMeshCache_.triBuilds.push_back(std::move(out));
            }
        }
        else
        {
            if (buildThings)
            {
                thingMeshCache_.triBuilds.push_back(std::move(out));
            }
        }

    }

    if (buildThings)
    {
        placementPreviewRenderStateValid_ =
            placementPreviewRenderedThings_.size() == placementPreviewRenderRanges_.size()
            && placementPreviewRenderedThings_.size()
                == placementPreviewLodStates_.size()
            && placementPreviewRenderedThings_.size()
                == placementPreviewLightingSignatures_.size();
    }

    // Build per-thing vertex range maps and per-thing bounds for fast thing transform updates.
    if (buildThings)
    {
        const bool canIncrementalMaps =
            incrementalThingAppend
            && showThings_
            && level_
            && !level_->things.empty()
            && incrementalThingStartIndex > 0
            && (thingBounds_.size() == incrementalThingStartIndex)
            && (level_->things.size() > incrementalThingStartIndex);

        if (canIncrementalMaps)
        {
            std::unordered_map<int, ThingBounds> boundsByPid{};
            pulseProgress();
            boundsByPid.reserve(level_->things.size() - incrementalThingStartIndex);
            pulseProgress();

            auto extendPidBounds = [&](const int pid, const float x, const float y, const float z) -> void
            {
                if (pid <= 0)
                {
                    return;
                }
                ThingBounds& b = boundsByPid[pid];
                render::includePoint(b, x, y, z);
            };

            // Line ranges and bounds: scan only the appended tail.
            {
                const auto& lines = thingMeshCache_.lineVertices;
                const std::size_t startIndex = std::min(incrementalPrevThingLineCount, lines.size());
                const int start = static_cast<int>(startIndex);
                int runPid = 0;
                int runStart = start;
                bool haveRun = false;

                auto flushRun = [&](const int end) -> void
                {
                    if (!haveRun)
                    {
                        return;
                    }
                    const int count = end - runStart;
                    if (runPid > 0 && count > 0)
                    {
                        render::appendOrExtendDrawRange(thingLineRangesByPickId_[runPid], runStart, count);
                    }
                };

                for (int i = start; i < static_cast<int>(lines.size()); ++i)
                {
                    reportProgress();
                    const LineVertex& v = lines[static_cast<std::size_t>(i)];
                    const int pid = render::pickIdKey(v.pickId);
                    extendPidBounds(pid, v.x, v.y, v.z);

                    if (!haveRun)
                    {
                        runPid = pid;
                        runStart = i;
                        haveRun = true;
                        continue;
                    }

                    if (pid != runPid)
                    {
                        flushRun(i);
                        runPid = pid;
                        runStart = i;
                    }
                }
                flushRun(static_cast<int>(lines.size()));
            }

            // TriBuild ranges and bounds: scan only newly appended builds.
            {
                const std::size_t startBuild = std::min(incrementalPrevThingTriBuildCount, thingMeshCache_.triBuilds.size());
                for (std::uint32_t bidx = static_cast<std::uint32_t>(startBuild);
                     bidx < static_cast<std::uint32_t>(thingMeshCache_.triBuilds.size());
                     ++bidx)
                {
                    reportProgress();
                    const auto& verts = thingMeshCache_.triBuilds[static_cast<std::size_t>(bidx)].vertices;
                    int runPid = 0;
                    int runStart = 0;
                    bool haveRun = false;

                    auto flushRun = [&](const int end) -> void
                    {
                        if (!haveRun)
                        {
                            return;
                        }
                        const int count = end - runStart;
                        if (runPid > 0 && count > 0)
                        {
                            render::appendOrExtendTriBuildRange(
                                thingTriBuildRangesByPickId_[runPid],
                                bidx,
                                runStart,
                                count);
                        }
                    };

                    for (int i = 0; i < static_cast<int>(verts.size()); ++i)
                    {
                        reportProgress();
                        const TriVertex& v = verts[static_cast<std::size_t>(i)];
                        const int pid = render::pickIdKey(v.pickId);
                        extendPidBounds(pid, v.x, v.y, v.z);

                        if (!haveRun)
                        {
                            runPid = pid;
                            runStart = i;
                            haveRun = true;
                            continue;
                        }

                        if (pid != runPid)
                        {
                            flushRun(i);
                            runPid = pid;
                            runStart = i;
                        }
                    }
                    flushRun(static_cast<int>(verts.size()));
                }
            }

            // In Dots view mode, update mapping from thing index to point index in the thing-point cache.
            if (thingViewMode_ == ThingViewMode::Dots)
            {
                if (thingPointIndexByThingIndex_.size() != incrementalThingStartIndex)
                {
                    // Fallback: rebuild mapping (cheap; O(numThings)).
                    pulseProgress();
                    thingPointIndexByThingIndex_.assign(level_->things.size(), -1);
                    pulseProgress();
                    int pIndex = 0;
                    for (std::size_t i = 0; i < level_->things.size(); ++i)
                    {
                        reportProgress();
                        const Thing& t = level_->things[i];
                        if (!isLayerVisible(t.layer))
                        {
                            continue;
                        }
                        if (!shouldShowThing(t))
                        {
                            continue;
                        }
                        if (!std::isfinite(t.x) || !std::isfinite(t.y) || !std::isfinite(t.z))
                        {
                            continue;
                        }
                        thingPointIndexByThingIndex_[i] = pIndex;
                        ++pIndex;
                    }
                    if (static_cast<std::size_t>(pIndex) * 3 != thingMeshCache_.pointPositions.size())
                    {
                        thingPointIndexByThingIndex_.clear();
                    }
                }
                else
                {
                    pulseProgress();
                    thingPointIndexByThingIndex_.resize(level_->things.size(), -1);
                    pulseProgress();
                    int pIndex = static_cast<int>(incrementalPrevThingPointCount / 3);
                    for (std::size_t i = incrementalThingStartIndex; i < level_->things.size(); ++i)
                    {
                        reportProgress();
                        const Thing& t = level_->things[i];
                        if (!isLayerVisible(t.layer))
                        {
                            continue;
                        }
                        if (!shouldShowThing(t))
                        {
                            continue;
                        }
                        if (!std::isfinite(t.x) || !std::isfinite(t.y) || !std::isfinite(t.z))
                        {
                            continue;
                        }
                        thingPointIndexByThingIndex_[i] = pIndex;
                        ++pIndex;
                    }
                    if (static_cast<std::size_t>(pIndex) * 3 != thingMeshCache_.pointPositions.size())
                    {
                        thingPointIndexByThingIndex_.clear();
                    }
                }
            }
            else
            {
                thingPointIndexByThingIndex_.clear();
            }

            // Append per-thing bounds for the new tail.
            pulseProgress();
            thingBounds_.resize(level_->things.size());
            pulseProgress();
            for (std::size_t i = incrementalThingStartIndex; i < level_->things.size(); ++i)
            {
                reportProgress();
                ThingBounds b{};

                const int pid = (i < thingPickIds_.size()) ? static_cast<int>(thingPickIds_[i]) : 0;
                if (pid > 0)
                {
                    if (const auto it = boundsByPid.find(pid); it != boundsByPid.end() && it->second.valid)
                    {
                        b = it->second;
                    }
                }

                if (!b.valid)
                {
                    const Thing& t = level_->things[i];
                    if (isLayerVisible(t.layer)
                        && shouldShowThing(t)
                        && std::isfinite(t.x) && std::isfinite(t.y) && std::isfinite(t.z))
                    {
                        const QVector3D p(static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z));
                        b.min = p;
                        b.max = p;
                        b.valid = true;
                    }
                }

                thingBounds_[i] = b;

                if (b.valid)
                {
                    if (!thingMeshCache_.haveAny)
                    {
                        thingMeshCache_.min = b.min;
                        thingMeshCache_.max = b.max;
                        thingMeshCache_.haveAny = true;
                    }
                    else
                    {
                        thingMeshCache_.min.setX(std::min(thingMeshCache_.min.x(), b.min.x()));
                        thingMeshCache_.min.setY(std::min(thingMeshCache_.min.y(), b.min.y()));
                        thingMeshCache_.min.setZ(std::min(thingMeshCache_.min.z(), b.min.z()));

                        thingMeshCache_.max.setX(std::max(thingMeshCache_.max.x(), b.max.x()));
                        thingMeshCache_.max.setY(std::max(thingMeshCache_.max.y(), b.max.y()));
                        thingMeshCache_.max.setZ(std::max(thingMeshCache_.max.z(), b.max.z()));
                    }
                }
            }
        }
        else
        {
            thingTriBuildRangesByPickId_.clear();
            thingLineRangesByPickId_.clear();
            thingBounds_.clear();
            thingPointIndexByThingIndex_.clear();

            if (showThings_ && level_ && !level_->things.empty())
        {
            std::unordered_map<int, ThingBounds> boundsByPid{};
            pulseProgress();
            boundsByPid.reserve(level_->things.size());
            pulseProgress();

            auto extendPidBounds = [&](const int pid, const float x, const float y, const float z) -> void
            {
                if (pid <= 0)
                {
                    return;
                }
                ThingBounds& b = boundsByPid[pid];
                render::includePoint(b, x, y, z);
            };

            // Line ranges and bounds.
            {
                const auto& lines = thingMeshCache_.lineVertices;
                int runPid = 0;
                int runStart = 0;
                bool haveRun = false;

                auto flushRun = [&](const int end) -> void
                {
                    if (!haveRun)
                    {
                        return;
                    }
                    const int count = end - runStart;
                    if (runPid > 0 && count > 0)
                    {
                        render::appendOrExtendDrawRange(thingLineRangesByPickId_[runPid], runStart, count);
                    }
                };

                for (int i = 0; i < static_cast<int>(lines.size()); ++i)
                {
                    reportProgress();
                    const LineVertex& v = lines[static_cast<std::size_t>(i)];
                    const int pid = render::pickIdKey(v.pickId);
                    extendPidBounds(pid, v.x, v.y, v.z);

                    if (!haveRun)
                    {
                        runPid = pid;
                        runStart = i;
                        haveRun = true;
                        continue;
                    }

                    if (pid != runPid)
                    {
                        flushRun(i);
                        runPid = pid;
                        runStart = i;
                    }
                }
                flushRun(static_cast<int>(lines.size()));
            }

            // TriBuild ranges and bounds.
            for (std::uint32_t bidx = 0; bidx < static_cast<std::uint32_t>(thingMeshCache_.triBuilds.size()); ++bidx)
            {
                reportProgress();
                const auto& verts = thingMeshCache_.triBuilds[static_cast<std::size_t>(bidx)].vertices;
                int runPid = 0;
                int runStart = 0;
                bool haveRun = false;

                auto flushRun = [&](const int end) -> void
                {
                    if (!haveRun)
                    {
                        return;
                    }
                    const int count = end - runStart;
                    if (runPid > 0 && count > 0)
                    {
                        render::appendOrExtendTriBuildRange(
                            thingTriBuildRangesByPickId_[runPid],
                            bidx,
                            runStart,
                            count);
                    }
                };

                for (int i = 0; i < static_cast<int>(verts.size()); ++i)
                {
                    reportProgress();
                    const TriVertex& v = verts[static_cast<std::size_t>(i)];
                    const int pid = render::pickIdKey(v.pickId);
                    extendPidBounds(pid, v.x, v.y, v.z);

                    if (!haveRun)
                    {
                        runPid = pid;
                        runStart = i;
                        haveRun = true;
                        continue;
                    }

                    if (pid != runPid)
                    {
                        flushRun(i);
                        runPid = pid;
                        runStart = i;
                    }
                }
                flushRun(static_cast<int>(verts.size()));
            }

            // In Dots view mode, keep a mapping from thing index to its point index in the thing-point cache.
            if (thingViewMode_ == ThingViewMode::Dots)
            {
                pulseProgress();
                thingPointIndexByThingIndex_.assign(level_->things.size(), -1);
                pulseProgress();
                int pIndex = 0;
                for (std::size_t i = 0; i < level_->things.size(); ++i)
                {
                    reportProgress();
                    const Thing& t = level_->things[i];
                    if (!isLayerVisible(t.layer))
                    {
                        continue;
                    }
                    if (!shouldShowThing(t))
                    {
                        continue;
                    }
                    if (!std::isfinite(t.x) || !std::isfinite(t.y) || !std::isfinite(t.z))
                    {
                        continue;
                    }
                    thingPointIndexByThingIndex_[i] = pIndex;
                    ++pIndex;
                }

                if (static_cast<std::size_t>(pIndex) * 3 != thingMeshCache_.pointPositions.size())
                {
                    thingPointIndexByThingIndex_.clear();
                }
            }

            // Convert boundsByPid to a dense per-thing array and refresh cached thing bounds union.
            pulseProgress();
            thingBounds_.resize(level_->things.size());
            pulseProgress();
            for (std::size_t i = 0; i < level_->things.size(); ++i)
            {
                reportProgress();
                ThingBounds b{};

                const int pid = (i < thingPickIds_.size()) ? static_cast<int>(thingPickIds_[i]) : 0;
                if (pid > 0)
                {
                    if (const auto it = boundsByPid.find(pid); it != boundsByPid.end() && it->second.valid)
                    {
                        b = it->second;
                    }
                }

                if (!b.valid)
                {
                    const Thing& t = level_->things[i];
                    if (isLayerVisible(t.layer)
                        && shouldShowThing(t)
                        && std::isfinite(t.x) && std::isfinite(t.y) && std::isfinite(t.z))
                    {
                        const QVector3D p(static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z));
                        b.min = p;
                        b.max = p;
                        b.valid = true;
                    }
                }

                thingBounds_[i] = b;
            }

            QVector3D mn(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
            QVector3D mx(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
            bool haveAny = false;
            for (const ThingBounds& b : thingBounds_)
            {
                reportProgress();
                if (!b.valid)
                {
                    continue;
                }
                if (!haveAny)
                {
                    mn = b.min;
                    mx = b.max;
                    haveAny = true;
                    continue;
                }
                mn.setX(std::min(mn.x(), b.min.x()));
                mn.setY(std::min(mn.y(), b.min.y()));
                mn.setZ(std::min(mn.z(), b.min.z()));
                mx.setX(std::max(mx.x(), b.max.x()));
                mx.setY(std::max(mx.y(), b.max.y()));
                mx.setZ(std::max(mx.z(), b.max.z()));
            }

            if (haveAny)
            {
                thingMeshCache_.min = mn;
                thingMeshCache_.max = mx;
                thingMeshCache_.haveAny = true;
            }
            else
            {
                thingMeshCache_.haveAny = false;
            }
        }
        }
    }

    if (buildThings)
    {
        if (incrementalThingAppend)
        {
            for (auto& [pickId, ranges] : collisionOverlayRangesByPickId)
            {
                auto& destination = thingCollisionOverlayRangesByPickId_[pickId];
                destination.insert(destination.end(), ranges.begin(), ranges.end());
            }
            for (auto& [pickId, ranges] : broadPhaseTreeGuideRangesByPickId)
            {
                auto& destination = thingBroadPhaseTreeGuideRangesByPickId_[pickId];
                destination.insert(destination.end(), ranges.begin(), ranges.end());
            }
            for (auto& [pickId, ranges] : visualBoundsOverlayRangesByPickId)
            {
                auto& destination = thingVisualBoundsOverlayRangesByPickId_[pickId];
                destination.insert(destination.end(), ranges.begin(), ranges.end());
            }
            for (auto& [pickId, ranges] : templateCollisionBoundsGuideRangesByPickId)
            {
                auto& destination = thingTemplateCollisionBoundsGuideRangesByPickId_[pickId];
                destination.insert(destination.end(), ranges.begin(), ranges.end());
            }
        }
        else
        {
            thingCollisionOverlayRangesByPickId_ = std::move(collisionOverlayRangesByPickId);
            thingBroadPhaseTreeGuideRangesByPickId_ = std::move(broadPhaseTreeGuideRangesByPickId);
            thingVisualBoundsOverlayRangesByPickId_ = std::move(visualBoundsOverlayRangesByPickId);
            thingTemplateCollisionBoundsGuideRangesByPickId_ =
                std::move(templateCollisionBoundsGuideRangesByPickId);
        }
    }

    // Compose draw buffers from caches (enables partial rebuilds).
    reportProgress();
    const MeshPartCache* levelCache = levelMeshCache_.valid ? &levelMeshCache_ : nullptr;
    const MeshPartCache* thingCache = thingMeshCache_.valid ? &thingMeshCache_ : nullptr;

    const std::size_t levelLineCount = levelCache ? levelCache->lineVertices.size() : 0;
    const std::size_t thingLineCount = thingCache ? thingCache->lineVertices.size() : 0;
    const std::size_t thingOverlayLineCount = thingCache ? thingCache->overlayLineVertices.size() : 0;

    if (!preserveStaticComposition)
    {
        lineVertices_.clear();
    }
    thingCollisionOverlayLineRanges_.clear();
    thingBroadPhaseTreeGuideLineRanges_.clear();
    thingVisualBoundsOverlayLineRanges_.clear();
    thingTemplateCollisionBoundsGuideLineRanges_.clear();
    pulseProgress();
    lineVertices_.reserve(levelLineCount + thingLineCount + thingOverlayLineCount);
    pulseProgress();
    if (levelCache && !preserveStaticComposition)
    {
        pulseProgress();
        lineVertices_.insert(lineVertices_.end(), levelCache->lineVertices.begin(), levelCache->lineVertices.end());
        pulseProgress();
    }
    wireLineVertexCount_ = static_cast<int>(levelLineCount);
    thingLineFirstVertex_ = wireLineVertexCount_;
    if (thingCache)
    {
        pulseProgress();
        lineVertices_.insert(lineVertices_.end(), thingCache->lineVertices.begin(), thingCache->lineVertices.end());
        pulseProgress();
    }
    thingLineVertexCount_ = static_cast<int>(thingLineCount);
    thingOverlayLineFirstVertex_ = thingLineFirstVertex_ + thingLineVertexCount_;
    if (thingCache)
    {
        lineVertices_.insert(
            lineVertices_.end(),
            thingCache->overlayLineVertices.begin(),
            thingCache->overlayLineVertices.end());
    }
    thingOverlayLineVertexCount_ = static_cast<int>(thingOverlayLineCount);
    for (const auto& [pickId, ranges] : thingCollisionOverlayRangesByPickId_)
    {
        (void)pickId;
        for (const render::DrawRange& range : ranges)
        {
            thingCollisionOverlayLineRanges_.push_back(render::DrawRange{
                thingOverlayLineFirstVertex_ + range.first,
                range.count,
            });
        }
    }
    for (const auto& [pickId, ranges] : thingBroadPhaseTreeGuideRangesByPickId_)
    {
        (void)pickId;
        for (const render::DrawRange& range : ranges)
        {
            thingBroadPhaseTreeGuideLineRanges_.push_back(render::DrawRange{
                thingOverlayLineFirstVertex_ + range.first,
                range.count,
            });
        }
    }
    for (const auto& [pickId, ranges] : thingVisualBoundsOverlayRangesByPickId_)
    {
        (void)pickId;
        for (const render::DrawRange& range : ranges)
        {
            thingVisualBoundsOverlayLineRanges_.push_back(render::DrawRange{
                thingOverlayLineFirstVertex_ + range.first,
                range.count,
            });
        }
    }
    for (const auto& [pickId, ranges] : thingTemplateCollisionBoundsGuideRangesByPickId_)
    {
        (void)pickId;
        for (const render::DrawRange& range : ranges)
        {
            thingTemplateCollisionBoundsGuideLineRanges_.push_back(render::DrawRange{
                thingOverlayLineFirstVertex_ + range.first,
                range.count,
            });
        }
    }

    if (preserveStaticComposition && reusableStaticLineLayerRanges)
    {
        while (!baseLineLayerRanges_.empty()
            && baseLineLayerRanges_.back().first >= wireLineVertexCount_)
        {
            baseLineLayerRanges_.pop_back();
        }
    }
    else
    {
        baseLineLayerRanges_.clear();
    }
    const int totalLineVertices = std::min(
        wireLineVertexCount_ + thingLineVertexCount_,
        static_cast<int>(lineVertices_.size()));
    auto resolveLineLayer = [this](const float pickId) -> std::int32_t
    {
        const int id = static_cast<int>(std::lround(pickId));
        if (id <= 0 || static_cast<std::size_t>(id) >= pickObjects_.size() || !level_)
        {
            return -1;
        }

        const PickObject& obj = pickObjects_[static_cast<std::size_t>(id)];
        if (obj.kind == PickKind::Surface)
        {
            if (obj.sector >= 0 && static_cast<std::size_t>(obj.sector) < level_->sectors.size())
            {
                return level_->sectors[static_cast<std::size_t>(obj.sector)].layer;
            }
            return -1;
        }

        if (obj.kind == PickKind::Thing)
        {
            if (obj.index >= 0 && static_cast<std::size_t>(obj.index) < level_->things.size())
            {
                return level_->things[static_cast<std::size_t>(obj.index)].layer;
            }
            return -1;
        }

        return -1;
    };

    if (totalLineVertices >= 2)
    {
        pulseProgress();
        baseLineLayerRanges_.reserve(static_cast<std::size_t>(totalLineVertices / 2));
        pulseProgress();
        int first = preserveStaticComposition && reusableStaticLineLayerRanges
            ? wireLineVertexCount_
            : 0;
        while ((first + 1) < totalLineVertices)
        {
            reportProgress();
            const std::int32_t layer = resolveLineLayer(lineVertices_[static_cast<std::size_t>(first)].pickId);
            const int rangeLimit = first < wireLineVertexCount_
                ? wireLineVertexCount_
                : totalLineVertices;
            int end = first + 2;
            while ((end + 1) < rangeLimit)
            {
                reportProgress();
                const std::int32_t nextLayer = resolveLineLayer(lineVertices_[static_cast<std::size_t>(end)].pickId);
                if (nextLayer != layer)
                {
                    break;
                }
                end += 2;
            }

            LayerLineRange range{};
            range.first = first;
            range.count = end - first;
            range.layer = layer;
            baseLineLayerRanges_.push_back(range);
            first = end;
        }
    }

    const std::size_t levelPointCount = levelCache ? levelCache->pointPositions.size() : 0;
    const std::size_t thingPointCount = thingCache ? thingCache->pointPositions.size() : 0;

    if (!preserveStaticComposition)
    {
        pointPositions_.clear();
    }
    pulseProgress();
    pointPositions_.reserve(levelPointCount + thingPointCount);
    pulseProgress();
    if (levelCache && !preserveStaticComposition)
    {
        pulseProgress();
        pointPositions_.insert(pointPositions_.end(), levelCache->pointPositions.begin(), levelCache->pointPositions.end());
        pulseProgress();
    }
    levelPointVertexCount_ = static_cast<int>(levelPointCount / 3);
    thingPointFirstVertex_ = levelPointVertexCount_;
    if (thingCache)
    {
        pulseProgress();
        pointPositions_.insert(pointPositions_.end(), thingCache->pointPositions.begin(), thingCache->pointPositions.end());
        pulseProgress();
    }
    thingPointVertexCount_ = static_cast<int>(thingPointCount / 3);
    pointVertexCount_ = static_cast<int>(pointPositions_.size() / 3);

    std::size_t totalTriVertices = preserveStaticComposition
        ? static_cast<std::size_t>(levelTriVertexCount_)
        : 0u;
    std::size_t totalTriBatches = preserveStaticComposition
        ? preservedLevelTriBatchCount
        : 0u;

    auto countPart = [&](const MeshPartCache* cache)
    {
        if (!cache)
        {
            return;
        }
        totalTriBatches += cache->triBuilds.size();
        for (const TriBuild& b : cache->triBuilds)
        {
            reportProgress();
            totalTriVertices += b.vertices.size();
        }
    };
    if (!preserveStaticComposition)
    {
        countPart(levelCache);
    }
    countPart(thingCache);

    if (!preserveStaticComposition)
    {
        triVertices_.clear();
        triBatches_.clear();
    }
    else
    {
        triVertices_.resize(static_cast<std::size_t>(levelTriVertexCount_));
        triBatches_.resize(preservedLevelTriBatchCount);
    }
    pulseProgress();
    triVertices_.reserve(totalTriVertices);
    triBatches_.reserve(totalTriBatches);
    pulseProgress();
    if (!preserveStaticComposition)
    {
        surfaceUvRanges_.clear();
    }

    const auto surfaceUvKey = [](const std::int32_t sector, const std::int32_t surface) noexcept
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(sector)) << 32u)
            | static_cast<std::uint32_t>(surface);
    };

    auto appendTriBuilds = [&](
        const std::vector<TriBuild>& triBuilds,
        int& vertexOffset,
        const bool indexLevelUvs)
    {
        for (std::size_t buildIndex = 0; buildIndex < triBuilds.size(); ++buildIndex)
        {
            reportProgress();
            const TriBuild& b = triBuilds[buildIndex];
            TriBatch batch{};
            batch.kind = b.kind;
            batch.key = b.key;
            batch.material = b.material;
            batch.colormap = b.colormap;
            batch.firstVertex = vertexOffset;
            batch.vertexCount = static_cast<int>(b.vertices.size());
            batch.boundsMin = b.boundsMin;
            batch.boundsMax = b.boundsMax;
            batch.boundsCenter = b.boundsCenter;
            batch.uvNormalized = b.uvNormalized;
            batch.skyGroup = b.skyGroup;
            batch.doubleSided = b.doubleSided;
            batch.faceFlags = b.faceFlags;
            batch.minAlpha = b.minAlpha;
            const int buildGlobalFirst = vertexOffset;
            vertexOffset += batch.vertexCount;

            if (indexLevelUvs && b.shadeInputs.size() == b.vertices.size())
            {
                std::uint64_t runKey{};
                int runFirst = -1;
                int runCount = 0;
                const auto flushRun = [&]()
                {
                    if (runFirst >= 0 && runCount > 0)
                    {
                        surfaceUvRanges_[runKey].push_back(SurfaceUvRange{
                            buildIndex,
                            runFirst,
                            buildGlobalFirst + runFirst,
                            runCount });
                    }
                    runFirst = -1;
                    runCount = 0;
                };

                for (std::size_t vertexIndex = 0; vertexIndex < b.shadeInputs.size(); ++vertexIndex)
                {
                    reportProgress();
                    const TriShadeInput& source = b.shadeInputs[vertexIndex];
                    if (source.sourceSector < 0 || source.sourceSurface < 0
                        || source.sourceTexVertex < 0)
                    {
                        flushRun();
                        continue;
                    }
                    const std::uint64_t key = surfaceUvKey(
                        source.sourceSector, source.sourceSurface);
                    if (runFirst >= 0 && key == runKey
                        && runFirst + runCount == static_cast<int>(vertexIndex))
                    {
                        ++runCount;
                    }
                    else
                    {
                        flushRun();
                        runKey = key;
                        runFirst = static_cast<int>(vertexIndex);
                        runCount = 1;
                    }
                }
                flushRun();
            }

            reportProgress();
            triVertices_.insert(triVertices_.end(), b.vertices.begin(), b.vertices.end());
            reportProgress();
            triBatches_.push_back(std::move(batch));
        }
    };

    int vertexOffset = preserveStaticComposition ? levelTriVertexCount_ : 0;
    if (levelCache && !preserveStaticComposition)
    {
        appendTriBuilds(levelCache->triBuilds, vertexOffset, true);
        levelTriVertexCount_ = vertexOffset;
    }
    const std::size_t currentLevelTriBatchCount = preserveStaticComposition
        ? preservedLevelTriBatchCount
        : (levelCache ? levelCache->triBuilds.size() : 0u);
    if (thingCache)
    {
        appendTriBuilds(thingCache->triBuilds, vertexOffset, false);
        thingTriVertexCount_ = vertexOffset - levelTriVertexCount_;
    }

    triVertexCount_ = static_cast<int>(triVertices_.size());

    // Update mapping from thing TriBuild index to its first vertex in the composed triangle buffer.
    thingTriBuildGlobalFirstVertex_.clear();
    if (thingCache)
    {
        pulseProgress();
        thingTriBuildGlobalFirstVertex_.reserve(thingCache->triBuilds.size());
        pulseProgress();
        for (std::size_t batchIndex = currentLevelTriBatchCount;
             batchIndex < triBatches_.size(); ++batchIndex)
        {
            reportProgress();
            const TriBatch& batch = triBatches_[batchIndex];
            if (batch.kind == TriKind::Thing)
            {
                thingTriBuildGlobalFirstVertex_.push_back(batch.firstVertex);
            }
        }
    }

    auto rebuildPickRanges = [&](
        const TriKind kind,
        std::vector<DrawRange>& out,
        const std::size_t firstBatch) -> void
    {
        out.clear();
        for (std::size_t batchIndex = firstBatch;
             batchIndex < triBatches_.size(); ++batchIndex)
        {
            reportProgress();
            const TriBatch& batch = triBatches_[batchIndex];
            if (batch.kind != kind || batch.vertexCount <= 0)
            {
                continue;
            }

            if (!out.empty())
            {
                DrawRange& last = out.back();
                if (last.first + last.count == batch.firstVertex)
                {
                    last.count += batch.vertexCount;
                    continue;
                }
            }

            out.push_back(DrawRange{ batch.firstVertex, batch.vertexCount });
        }
    };

    if (!preserveStaticComposition)
    {
        rebuildPickRanges(TriKind::Level, levelPickRanges_, 0u);
    }
    rebuildPickRanges(
        TriKind::Thing,
        thingPickRanges_,
        currentLevelTriBatchCount);

    QVector3D minv(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector3D maxv(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    bool haveAny = false;

    auto includeBounds = [&](const MeshPartCache* cache)
    {
        if (!cache || !cache->valid || !cache->haveAny)
        {
            return;
        }

        if (!haveAny)
        {
            minv = cache->min;
            maxv = cache->max;
            haveAny = true;
            return;
        }

        minv.setX(std::min(minv.x(), cache->min.x()));
        minv.setY(std::min(minv.y(), cache->min.y()));
        minv.setZ(std::min(minv.z(), cache->min.z()));

        maxv.setX(std::max(maxv.x(), cache->max.x()));
        maxv.setY(std::max(maxv.y(), cache->max.y()));
        maxv.setZ(std::max(maxv.z(), cache->max.z()));
    };

    includeBounds(levelCache);
    includeBounds(thingCache);

    if (buildThings)
    {
        thingRenderState_.clear();
        multiLodThingIndices_.clear();
        pulseProgress();
        thingRenderState_.reserve(level_->things.size());
        if (!builtThingGeosetCounts.empty())
        {
            multiLodThingIndices_.reserve(level_->things.size());
        }
        pulseProgress();
        for (std::size_t thingIndex = 0;
             thingIndex < level_->things.size();
             ++thingIndex)
        {
            reportProgress();
            const Thing& thing = level_->things[thingIndex];
            ThingRenderState st{};
            st.x = thing.x;
            st.y = thing.y;
            st.z = thing.z;
            st.pch = thing.pch;
            st.yaw = thing.yaw;
            st.rol = thing.rol;
            st.sector = thing.sector;
            st.signature = thingGeometrySignature(thing);
            st.runtimeLightSignature = thingRuntimeLightSignature(thing);
            if (!builtThingGeosetCounts.empty())
            {
                st.geosetCount = builtThingGeosetCounts[thingIndex];
                st.requestedLodIndex =
                    builtThingRequestedLodIndices[thingIndex];
                st.lodIndex = builtThingLodIndices[thingIndex];
                st.archThing = builtThingArchFlags[thingIndex] != 0u;
                if (st.geosetCount > 1u && !st.archThing)
                {
                    multiLodThingIndices_.push_back(
                        static_cast<std::int32_t>(thingIndex));
                }
            }
            const render::thing_overlay::CollisionDescription collision =
                render::thing_overlay::resolveCollisionDescription(
                    *level_, templates_, thing);
            st.collisionOverlayShape = static_cast<std::uint8_t>(collision.shape);
            st.collisionOverlayRadius = collision.radius;
            st.collisionOverlayHasRadius = collision.hasRadius;
            st.collisionOverlayBroadPhaseRadius = collision.broadPhaseRadius;
            thingRenderState_.push_back(std::move(st));
        }
        thingLodCheckedViewSerial_ = viewSerial_;
        thingLodCheckedHeaderDistances_ = level_->header.lodDistances;
        thingLodHeaderChecked_ = true;

        // Seed the fast-update baseline at the same time as the Thing
        // vertices. Lazily initializing this after a Sector/Ambient edit
        // would record the new values as both "before" and "after", leaving
        // the first live sample's cached Thing shading stale.
        thingSectorLightCache_.clear();
        pulseProgress();
        thingSectorLightCache_.reserve(level_->sectors.size());
        pulseProgress();
        for (std::size_t sectorIndex = 0; sectorIndex < level_->sectors.size(); ++sectorIndex)
        {
            reportProgress();
            const ThingSectorLighting lights = calcThingSectorLighting(
                level_, static_cast<std::int32_t>(sectorIndex));
            SectorThingLightCache cache{};
            cache.ambient = lights.ambientLight;
            cache.pointLight = level_->sectors[sectorIndex].pointLight;
            cache.editorSectorTint = render::classicEditorSectorTintScale(
                level_, static_cast<std::int32_t>(sectorIndex));
            cache.valid = true;
            thingSectorLightCache_.push_back(cache);
        }
    }

    // Apply the sparse ARCH preview only after the composed triangle buffer
    // and its exact Thing build ranges are complete. This preserves authored
    // corner identity through triangulation and keeps the preview out of the
    // immutable model/cache payloads.
    applyArchitecturalLightingTrianglePreview();

    if (!haveAny)
    {
        camera_.orbit.center = QVector3D(0.0f, 0.0f, 0.0f);
        camera_.orbit.radius = 1.0f;
        pulseProgress();
        rebuildSelectionOverlays();
        pulseProgress();
        return;
    }

    camera_.orbit.center = (minv + maxv) * 0.5f;
    camera_.orbit.radius = std::max((maxv - minv).length() * 0.5f, 1.0f);
    pulseProgress();
    rebuildSelectionOverlays();
    pulseProgress();
}

void GlView::rebuildSelectionOverlays()
{
    const SurfaceSubMode selectionSurfaceMode =
        renderPipeline_ == RenderPipeline::GamePreview
        ? SurfaceSubMode::Face
        : surfaceSubMode_;
    // Step 1: reset all overlay buffers and split counters before rebuilding from the current selection.
    highlightLineVertices_.clear();
    highlightLineVertexCount_ = 0;
    highlightLineMultiVertexCount_ = 0;
    activeEdgeHighlightFirstVertex_ = 0;
    activeEdgeHighlightVertexCount_ = 0;
    secondaryEdgeHighlightFirstVertex_ = 0;
    secondaryEdgeHighlightVertexCount_ = 0;
    activeSurfaceOwnerHighlightFirstVertex_ = 0;
    activeSurfaceOwnerHighlightVertexCount_ = 0;
    surfaceNormalHighlightFirstVertex_ = 0;
    surfaceNormalHighlightVertexCount_ = 0;
    surfaceNormalArrowheads_.clear();
    surfaceNormalArrowheadsViewSerial_ = ~std::uint64_t{0};

    highlightPointPositions_.clear();
    highlightPointVertexCount_ = 0;
    highlightPointMultiVertexCount_ = 0;
    highlightPointSelectedFirstPoint_ = 0;
    highlightPointSelectedPointCount_ = 0;
    activeVertexHighlightFirstPoint_ = -1;
    activeVertexHighlightPointCount_ = 0;
    architecturalLightingFirstPoint_ = 0;
    architecturalLightingPointCount_ = 0;
    architecturalLightingSelectedFirstPoint_ = 0;
    architecturalLightingSelectedPointCount_ = 0;
    architecturalLightingPointVertices_.clear();
    architecturalLightingPointIdentities_.clear();

    outlineTriVertices_.clear();
    outlineSourceRanges_.clear();
    outlineTriVertexCount_ = 0;
    outlineTriMultiVertexCount_ = 0;
    geometrySelectionStencilRanges_.clear();
    geometrySelectionLineRanges_.clear();

    outlinedThing_ = -1;
    outlinedThingCenter_ = QVector3D(0.0f, 0.0f, 0.0f);
    outlinedThingDoubleSided_ = false;

    if (!level_)
    {
        return;
    }

    // Step 2: keep visibility checks and append helpers local so the later stages read as selection logic.
    auto isSectorVisible = [this](const std::int32_t sectorIndex) -> bool
    {
        if (sectorIndex < 0)
        {
            return false;
        }
        const std::size_t idx = static_cast<std::size_t>(sectorIndex);
        if (idx >= level_->sectors.size())
        {
            return false;
        }
        return isLayerVisible(level_->sectors[idx].layer);
    };

    auto isThingVisible = [this](const std::int32_t thingIndex) -> bool
    {
        if (thingIndex < 0)
        {
            return false;
        }
        const std::size_t idx = static_cast<std::size_t>(thingIndex);
        if (idx >= level_->things.size())
        {
            return false;
        }
        const Thing& thing = level_->things[idx];
        return isLayerVisible(thing.layer) && shouldShowThing(thing);
    };

    auto pushLinePoint = [&](const double x,
                             const double y,
                             const double z,
                             const float pickId = 0.0f,
                             const Vertex* planeReference = nullptr) -> void
    {
        LineVertex v{};
        v.x = static_cast<float>(x);
        v.y = static_cast<float>(y);
        v.z = static_cast<float>(z);
        v.pickId = pickId;
        if (planeReference)
        {
            v.planeX = static_cast<float>(planeReference->x);
            v.planeY = static_cast<float>(planeReference->y);
            v.planeZ = static_cast<float>(planeReference->z);
            v.planeValid = 1.0f;
        }
        highlightLineVertices_.push_back(v);
    };

    auto appendGeometryHighlightRange = [&](const int first, const int count) -> void
    {
        if (first < 0 || count <= 0)
        {
            return;
        }
        render::appendOrExtendDrawRange(geometrySelectionLineRanges_, first, count);
    };

    auto appendWirePoints = [&](const auto& wirePoints) -> void
    {
        for (const render::ThingWirePoint& point : wirePoints)
        {
            pushLinePoint(point.x, point.y, point.z, point.pickId);
        }
    };

    auto pushHighlightPoint = [&](const double x, const double y, const double z) -> void
    {
        highlightPointPositions_.push_back(static_cast<float>(x));
        highlightPointPositions_.push_back(static_cast<float>(y));
        highlightPointPositions_.push_back(static_cast<float>(z));
    };

    auto pushLineVertex = [&](const Vertex& v) -> void
    {
        pushLinePoint(v.x, v.y, v.z);
    };

    auto lookupSurfacePickId = [&](const SurfaceRef ref) -> float
    {
        if (ref.sector < 0 || ref.surface < 0)
        {
            return 0.0f;
        }

        const std::uint64_t key =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32)
            | static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.surface));

        if (const auto it = surfacePickIds_.find(key); it != surfacePickIds_.end())
        {
            return static_cast<float>(it->second);
        }

        return 0.0f;
    };

    // Keep selected geometry pick ids separate from the line overlay.  The
    // ids let the stencil pass reuse the composed level triangle VBO without
    // copying or rebuilding the level mesh when selection changes.
    std::unordered_set<int> selectedGeometryPickIds{};
    auto rememberSelectedSurface = [&](const SurfaceRef ref) -> void
    {
        const int pickId = static_cast<int>(std::lround(lookupSurfacePickId(ref)));
        if (pickId > 0)
        {
            selectedGeometryPickIds.insert(pickId);
        }
    };

    // Record the active geometry's authored edges before secondary selection
    // is emitted.  Adjacent selected surfaces share the same geometric edge;
    // drawing that edge in translucent multi blue and again in primary yellow
    // produces a green anti-aliased blend.  The active edge belongs solely to
    // the primary pass.
    std::unordered_set<render::GeometryWireEdgeKey,
        GeometrySelectionEdgeKeyHash> primaryGeometryEdges{};
    std::vector<std::pair<QVector3D, QVector3D>> primaryGeometrySegments{};
    const auto rememberPrimarySurfaceEdges = [
        &primaryGeometryEdges,
        &primaryGeometrySegments](
        const std::int32_t sectorIndex,
        const Sector& sector,
        const Surface& surface)
    {
        render::forEachPolygonWireEdge(
            surface.vertices.size(),
            [&](const std::size_t index) { return surface.vertices[index]; },
            [&](const std::int32_t first, const std::int32_t second)
            {
                if (first >= 0 && second >= 0)
                {
                    primaryGeometryEdges.insert(
                        render::geometryWireEdgeKey(sectorIndex, first, second));
                    if (static_cast<std::size_t>(first) < sector.vertices.size()
                        && static_cast<std::size_t>(second) < sector.vertices.size())
                    {
                        const Vertex& a = sector.vertices[static_cast<std::size_t>(first)];
                        const Vertex& b = sector.vertices[static_cast<std::size_t>(second)];
                        primaryGeometrySegments.emplace_back(
                            QVector3D(a.x, a.y, a.z),
                            QVector3D(b.x, b.y, b.z));
                    }
                }
            });
    };
    if (selectedSurfaceSector_ >= 0 && selectedSurface_ >= 0
        && static_cast<std::size_t>(selectedSurfaceSector_) < level_->sectors.size())
    {
        const Sector& sector =
            level_->sectors[static_cast<std::size_t>(selectedSurfaceSector_)];
        if (static_cast<std::size_t>(selectedSurface_) < sector.surfaces.size())
        {
            rememberPrimarySurfaceEdges(
                selectedSurfaceSector_,
                sector,
                sector.surfaces[static_cast<std::size_t>(selectedSurface_)]);
        }
    }
    else if (selectedSector_ >= 0
        && static_cast<std::size_t>(selectedSector_) < level_->sectors.size())
    {
        const Sector& sector = level_->sectors[static_cast<std::size_t>(selectedSector_)];
        for (const Surface& surface : sector.surfaces)
        {
            rememberPrimarySurfaceEdges(selectedSector_, sector, surface);
        }
    }

    const auto edgeMatchesPrimary = [
        &primaryGeometryEdges,
        &primaryGeometrySegments](
        const std::int32_t sectorIndex,
        const Sector& sector,
        const std::int32_t first,
        const std::int32_t second)
    {
        if (first < 0 || second < 0
            || static_cast<std::size_t>(first) >= sector.vertices.size()
            || static_cast<std::size_t>(second) >= sector.vertices.size())
        {
            return false;
        }
        if (primaryGeometryEdges.contains(
                render::geometryWireEdgeKey(sectorIndex, first, second)))
        {
            return true;
        }
        const Vertex& firstVertex = sector.vertices[static_cast<std::size_t>(first)];
        const Vertex& secondVertex = sector.vertices[static_cast<std::size_t>(second)];
        const QVector3D a(firstVertex.x, firstVertex.y, firstVertex.z);
        const QVector3D b(secondVertex.x, secondVertex.y, secondVertex.z);
        return std::any_of(
            primaryGeometrySegments.begin(),
            primaryGeometrySegments.end(),
            [&a, &b](const auto& segment)
            {
                return render::geometryWireSegmentsCoincide(
                    a, b, segment.first, segment.second);
            });
    };

    const auto surfaceSharesPrimaryEdge = [&edgeMatchesPrimary](
        const SurfaceRef ref,
        const Sector& sector,
        const Surface& surface)
    {
        bool shares = false;
        render::forEachPolygonWireEdge(
            surface.vertices.size(),
            [&](const std::size_t index) { return surface.vertices[index]; },
            [&](const std::int32_t first, const std::int32_t second)
            {
                shares = shares || edgeMatchesPrimary(
                    ref.sector, sector, first, second);
            });
        return shares;
    };

    // Reuse cached surface line ranges when possible; fall back to rebuilding edges if a cache entry is missing.
    const auto edgeCoincidesWithExplicitSelection = [this, selectionSurfaceMode](
        const SurfaceRef ref,
        const std::size_t localEdge,
        const std::int32_t first,
        const std::int32_t second)
    {
        if (selectionSurfaceMode != SurfaceSubMode::Edge)
        {
            return false;
        }
        if (selectedSurfaceEdges_.contains(SurfaceEdgeSelectionRef{
                ref, static_cast<std::int32_t>(localEdge), first, second }))
        {
            return true;
        }
        if (!level_ || ref.sector < 0 || first < 0 || second < 0
            || static_cast<std::size_t>(ref.sector) >= level_->sectors.size())
        {
            return false;
        }
        const Sector& owner = level_->sectors[
            static_cast<std::size_t>(ref.sector)];
        if (static_cast<std::size_t>(first) >= owner.vertices.size()
            || static_cast<std::size_t>(second) >= owner.vertices.size())
        {
            return false;
        }
        const Vertex& firstVertex = owner.vertices[
            static_cast<std::size_t>(first)];
        const Vertex& secondVertex = owner.vertices[
            static_cast<std::size_t>(second)];
        const QVector3D a(firstVertex.x, firstVertex.y, firstVertex.z);
        const QVector3D b(secondVertex.x, secondVertex.y, secondVertex.z);

        return std::any_of(
            selectedSurfaceEdges_.begin(),
            selectedSurfaceEdges_.end(),
            [this, &a, &b](const SurfaceEdgeSelectionRef& selectedEdge)
            {
                if (!surfaceEdgeSelectionRefValid(selectedEdge))
                {
                    return false;
                }
                const Sector& selectedOwner = level_->sectors[
                    static_cast<std::size_t>(selectedEdge.surface.sector)];
                const Vertex& selectedFirst = selectedOwner.vertices[
                    static_cast<std::size_t>(selectedEdge.firstVertex)];
                const Vertex& selectedSecond = selectedOwner.vertices[
                    static_cast<std::size_t>(selectedEdge.secondVertex)];
                return render::geometryWireSegmentsCoincide(
                    a,
                    b,
                    QVector3D(selectedFirst.x, selectedFirst.y, selectedFirst.z),
                    QVector3D(selectedSecond.x, selectedSecond.y, selectedSecond.z));
            });
    };

    auto surfaceUsesAlphaAdjoinNoDepthPass = [this](
        const Sector& sector,
        const Surface& surface) -> bool
    {
        if (!level_ || !showTextures_)
        {
            return false;
        }

        const std::uint32_t renderFaceFlags = render::levelSurfaceRenderFaceFlags(
            level_->kind,
            surface.surfaceFlags,
            surface.faceFlags);
        const bool isAdjoin = surface.adjoin.has_value()
            || ((renderFaceFlags & FF_RD_Adjoin) != 0u);
        if (!isAdjoin)
        {
            return false;
        }

        const std::string textureKey = render::makeBatchKey(
            surface.material,
            sectorRenderColormap(*level_, sector));
        const auto texture = textures_.find(textureKey);
        return texture != textures_.end()
            && texture->second.texture
            && texture->second.alphaBpp > 0;
    };

    auto appendCachedSurfaceEdges = [&](const SurfaceRef ref,
                                        const bool secondary) -> bool
    {
        if (ref.sector < 0
            || ref.surface < 0
            || !levelMeshCache_.valid
            || static_cast<std::size_t>(ref.sector) >= level_->sectors.size())
        {
            return false;
        }
        const Sector& ownerSector = level_->sectors[static_cast<std::size_t>(ref.sector)];
        if (static_cast<std::size_t>(ref.surface) >= ownerSector.surfaces.size())
        {
            return false;
        }
        const Surface& ownerSurface = ownerSector.surfaces[static_cast<std::size_t>(ref.surface)];
        const bool suppressSupport =
            surfaceUsesAlphaAdjoinNoDepthPass(ownerSector, ownerSurface);

        const std::uint64_t key =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.sector)) << 32)
            | static_cast<std::uint64_t>(static_cast<std::uint32_t>(ref.surface));

        const auto pickIt = surfacePickIds_.find(key);
        if (pickIt == surfacePickIds_.end())
        {
            return false;
        }

        const auto rangeIt = surfaceLineRangesByPickId_.find(pickIt->second);
        if (rangeIt == surfaceLineRangesByPickId_.end())
        {
            return false;
        }

        if (secondary
            && static_cast<std::size_t>(ref.sector) < level_->sectors.size())
        {
            const Sector& sector =
                level_->sectors[static_cast<std::size_t>(ref.sector)];
            if (static_cast<std::size_t>(ref.surface) < sector.surfaces.size()
                && surfaceSharesPrimaryEdge(
                    ref,
                    sector,
                    sector.surfaces[static_cast<std::size_t>(ref.surface)]))
            {
                // Cached ranges are whole-surface spans.  Rebuild this one so
                // its shared primary edge can be omitted selectively.
                return false;
            }
        }

        if (selectionSurfaceMode == SurfaceSubMode::Edge
            && std::any_of(
                selectedSurfaceEdges_.begin(), selectedSurfaceEdges_.end(),
                [ref](const SurfaceEdgeSelectionRef& edge)
                {
                    return edge.surface == ref;
                }))
        {
            // Selected edges must be omitted from the owning face wire and
            // emitted once in their dedicated top-layer range.
            return false;
        }

        const DrawRange range = rangeIt->second;
        // Empty, negative, or overrun cache entries are not valid authored
        // surface ranges.  Let the caller rebuild the polygon wireframe so a
        // malformed cache can never suppress a selected surface's edges.
        if (!render::isValidDrawRange(range, levelMeshCache_.lineVertices.size()))
        {
            return false;
        }

        const std::size_t first = static_cast<std::size_t>(range.first);
        const std::size_t count = static_cast<std::size_t>(range.count);

        const int highlightFirst = static_cast<int>(highlightLineVertices_.size());
        highlightLineVertices_.insert(
            highlightLineVertices_.end(),
            levelMeshCache_.lineVertices.begin() + static_cast<std::ptrdiff_t>(first),
            levelMeshCache_.lineVertices.begin() + static_cast<std::ptrdiff_t>(first + count));
        if (suppressSupport)
        {
            for (std::size_t i = highlightLineVertices_.size() - count;
                 i < highlightLineVertices_.size();
                 ++i)
            {
                highlightLineVertices_[i].planeValid = 0.0f;
            }
        }
        appendGeometryHighlightRange(
            highlightFirst,
            static_cast<int>(count));
        return true;
    };

    auto pushSurfaceEdges = [&](const SurfaceRef ref,
                                const Sector& sector,
                                const Surface& surface,
                                const float pickId,
                                const bool secondary,
                                const std::optional<std::pair<std::int32_t, std::int32_t>>& skippedEdge = std::nullopt) -> void
    {
        const int highlightFirst = static_cast<int>(highlightLineVertices_.size());
        const bool surfaceUsesAlphaAdjoinPass =
            surfaceUsesAlphaAdjoinNoDepthPass(sector, surface);
        const bool surfaceHasDepthWritingFill = level_
            && !surfaceUsesAlphaAdjoinPass
            && wireframeSurfaceCanMatchDepthFromFill(level_->kind, surface);
        const auto& vxs = surface.vertices;
        for (std::size_t edge = 0; edge < vxs.size(); ++edge)
        {
            const std::int32_t aidx = vxs[edge];
            const std::int32_t bidx = vxs[(edge + 1u) % vxs.size()];
            if (aidx < 0 || bidx < 0)
            {
                continue;
            }
            if (secondary && edgeMatchesPrimary(
                    ref.sector, sector, aidx, bidx))
            {
                continue;
            }
            if (skippedEdge
                && ((aidx == skippedEdge->first && bidx == skippedEdge->second)
                    || (aidx == skippedEdge->second && bidx == skippedEdge->first)))
            {
                continue;
            }
            if (edgeCoincidesWithExplicitSelection(ref, edge, aidx, bidx))
            {
                continue;
            }
            if (static_cast<std::size_t>(aidx) >= sector.vertices.size()
                || static_cast<std::size_t>(bidx) >= sector.vertices.size())
            {
                continue;
            }

            const Vertex& va = sector.vertices[static_cast<std::size_t>(aidx)];
            const Vertex& vb = sector.vertices[static_cast<std::size_t>(bidx)];
            const Vertex* planeReference = surfaceHasDepthWritingFill
                ? wireframeSurfacePlaneReference(sector, surface, edge)
                : nullptr;
            pushLinePoint(va.x, va.y, va.z, pickId, planeReference);
            pushLinePoint(vb.x, vb.y, vb.z, pickId, planeReference);
        }
        appendGeometryHighlightRange(
            highlightFirst,
            static_cast<int>(highlightLineVertices_.size()) - highlightFirst);
    };

    auto highlightSector = [&](const std::int32_t sectorIndex,
                               const bool secondary) -> void
    {
        if (!isSectorVisible(sectorIndex))
        {
            return;
        }

        const std::size_t secIndex = static_cast<std::size_t>(sectorIndex);
        const Sector& sector = level_->sectors[secIndex];
        for (std::size_t surfaceIndex = 0; surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
        {
            const auto surfaceIndex32 = static_cast<std::int32_t>(surfaceIndex);
            if (secondary && render::isPrimarySelectionSurface(
                    selectedSector_,
                    selectedSurfaceSector_,
                    selectedSurface_,
                    sectorIndex,
                    surfaceIndex32))
            {
                // The active geometry is emitted after the multi split.  Do
                // not also emit it in blue merely because the owning Sector
                // is present in the aggregate Selection.
                continue;
            }
            const SurfaceRef ref{ sectorIndex, surfaceIndex32 };
            rememberSelectedSurface(ref);
            if (!appendCachedSurfaceEdges(ref, secondary))
            {
                const float pickId = lookupSurfacePickId(ref);
                pushSurfaceEdges(
                    ref,
                    sector,
                    sector.surfaces[surfaceIndex],
                    pickId,
                    secondary);
            }
        }
    };

    auto highlightSurface = [&](const SurfaceRef ref,
                                const bool secondary,
                                const std::optional<std::pair<std::int32_t, std::int32_t>>& skippedEdge = std::nullopt) -> void
    {
        if (ref.sector < 0 || ref.surface < 0 || !isSectorVisible(ref.sector))
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

        rememberSelectedSurface(ref);
        if (skippedEdge || !appendCachedSurfaceEdges(ref, secondary))
        {
            const float pickId = lookupSurfacePickId(ref);
            pushSurfaceEdges(
                ref,
                sector,
                sector.surfaces[surfIndex],
                pickId,
                secondary,
                skippedEdge);
        }
    };

    const bool haveSelectedSurface = (selectedSurfaceSector_ >= 0) && (selectedSurface_ >= 0);
    const SurfaceRef selectedRef{ selectedSurfaceSector_, selectedSurface_ };
    const bool vertexMode = (selectionSurfaceMode == SurfaceSubMode::Vertex);
    std::optional<TVector> activeVertexPoint{};
    std::optional<std::pair<std::int32_t, std::int32_t>> selectedActiveEdge{};
    std::vector<SurfaceEdgeSelectionRef> selectedSecondaryEdges{};

    // Exact edges are transient viewport selection state. The active member
    // owns the active color; every other member receives a separate secondary
    // top-layer range. All of them are omitted from owning face wires below so
    // anti-aliased colors never blend on the same segment.
    if (selectionSurfaceMode == SurfaceSubMode::Edge
        && haveSelectedSurface
        && highlightedSelection_.surfaces.contains(selectedRef)
        && activeSurfaceEdge_
        && selectedSurfaceEdges_.contains(*activeSurfaceEdge_)
        && activeSurfaceEdge_->surface == selectedRef
        && surfaceEdgeSelectionRefValid(*activeSurfaceEdge_))
    {
        selectedActiveEdge = std::pair{
            activeSurfaceEdge_->firstVertex,
            activeSurfaceEdge_->secondVertex };
    }
    if (selectionSurfaceMode == SurfaceSubMode::Edge)
    {
        selectedSecondaryEdges.reserve(selectedSurfaceEdges_.size());
        for (const SurfaceEdgeSelectionRef& edge : selectedSurfaceEdges_)
        {
            if ((!activeSurfaceEdge_ || edge != *activeSurfaceEdge_)
                && surfaceEdgeSelectionRefValid(edge)
                && highlightedSelection_.surfaces.contains(edge.surface))
            {
                selectedSecondaryEdges.push_back(edge);
            }
        }
        std::sort(selectedSecondaryEdges.begin(), selectedSecondaryEdges.end(),
            [](const SurfaceEdgeSelectionRef& lhs,
               const SurfaceEdgeSelectionRef& rhs)
            {
                if (lhs.surface.sector != rhs.surface.sector)
                {
                    return lhs.surface.sector < rhs.surface.sector;
                }
                if (lhs.surface.surface != rhs.surface.surface)
                {
                    return lhs.surface.surface < rhs.surface.surface;
                }
                return lhs.localEdge < rhs.localEdge;
            });

        const auto edgeSegment = [this](const SurfaceEdgeSelectionRef& edge)
            -> std::optional<std::pair<QVector3D, QVector3D>>
        {
            if (!surfaceEdgeSelectionRefValid(edge))
            {
                return std::nullopt;
            }
            const Sector& sector = level_->sectors[
                static_cast<std::size_t>(edge.surface.sector)];
            const Vertex& first = sector.vertices[
                static_cast<std::size_t>(edge.firstVertex)];
            const Vertex& second = sector.vertices[
                static_cast<std::size_t>(edge.secondVertex)];
            return std::pair{
                QVector3D(first.x, first.y, first.z),
                QVector3D(second.x, second.y, second.z) };
        };
        const auto edgesCoincide = [&edgeSegment](
            const SurfaceEdgeSelectionRef& lhs,
            const SurfaceEdgeSelectionRef& rhs)
        {
            const auto a = edgeSegment(lhs);
            const auto b = edgeSegment(rhs);
            return a && b && render::geometryWireSegmentsCoincide(
                a->first, a->second, b->first, b->second);
        };
        std::vector<SurfaceEdgeSelectionRef> uniqueSecondaryEdges{};
        uniqueSecondaryEdges.reserve(selectedSecondaryEdges.size());
        for (const SurfaceEdgeSelectionRef& edge : selectedSecondaryEdges)
        {
            if ((activeSurfaceEdge_
                    && surfaceEdgeSelectionRefValid(*activeSurfaceEdge_)
                    && edgesCoincide(edge, *activeSurfaceEdge_))
                || std::any_of(
                    uniqueSecondaryEdges.begin(),
                    uniqueSecondaryEdges.end(),
                    [&edge, &edgesCoincide](const SurfaceEdgeSelectionRef& emitted)
                    {
                        return edgesCoincide(edge, emitted);
                    }))
            {
                continue;
            }
            uniqueSecondaryEdges.push_back(edge);
        }
        selectedSecondaryEdges = std::move(uniqueSecondaryEdges);
    }

    // Step 3: draw only secondary sector/surface highlights before the color
    // split. The active owner is appended later so it receives primary color
    // in Face, Edge, and Vertex modes alike.
    for (const std::int32_t sectorIndex : highlightedSelection_.sectors)
    {
        if (sectorIndex == selectedSector_)
        {
            continue;
        }
        highlightSector(sectorIndex, true);
    }

    for (const SurfaceRef ref : highlightedSelection_.surfaces)
    {
        if (render::isPrimarySelectionSurface(
                selectedSector_,
                selectedSurfaceSector_,
                selectedSurface_,
                ref.sector,
                ref.surface))
        {
            continue;
        }
        highlightSurface(ref, true);
    }

    // Step 4: append the active owner face after the color split, then append
    // its explicit active edge. The owner therefore stays primary/yellow in
    // Edge and Vertex modes while the active sub-element alone is red.
    auto highlightSelectedSurface = [&]() -> void
    {
        if (!haveSelectedSurface || !isSectorVisible(selectedRef.sector))
        {
            return;
        }

        activeSurfaceOwnerHighlightFirstVertex_ =
            static_cast<int>(highlightLineVertices_.size());
        highlightSurface(selectedRef, false, selectedActiveEdge);
        activeSurfaceOwnerHighlightVertexCount_ =
            static_cast<int>(highlightLineVertices_.size())
            - activeSurfaceOwnerHighlightFirstVertex_;

        if (selectionSurfaceMode == SurfaceSubMode::Edge && selectedActiveEdge)
        {
            const std::size_t secIndex = static_cast<std::size_t>(selectedRef.sector);
            if (secIndex < level_->sectors.size())
            {
                const Sector& sector = level_->sectors[secIndex];
                const std::int32_t bestA = selectedActiveEdge->first;
                const std::int32_t bestB = selectedActiveEdge->second;
                if (bestA >= 0 && bestB >= 0
                    && static_cast<std::size_t>(bestA) < sector.vertices.size()
                    && static_cast<std::size_t>(bestB) < sector.vertices.size())
                {
                    activeEdgeHighlightFirstVertex_ = static_cast<int>(highlightLineVertices_.size());
                    const Vertex& va = sector.vertices[static_cast<std::size_t>(bestA)];
                    const Vertex& vb = sector.vertices[static_cast<std::size_t>(bestB)];
                    const float pickId = lookupSurfacePickId(selectedRef);
                    pushLinePoint(va.x, va.y, va.z, pickId);
                    pushLinePoint(vb.x, vb.y, vb.z, pickId);
                    activeEdgeHighlightVertexCount_ =
                        static_cast<int>(highlightLineVertices_.size()) - activeEdgeHighlightFirstVertex_;
                    appendGeometryHighlightRange(
                        activeEdgeHighlightFirstVertex_,
                        activeEdgeHighlightVertexCount_);
                }
            }
        }
    };

    const float crossSize = std::max(camera_.orbit.radius * 0.02f, 0.25f);

    // Step 5: thing highlight emitters mirror the render priority: dots, sprite frames, boxes, then model wireframes.
    auto addCross = [&](const float x, const float y, const float z, const float size, const float pickId = 0.0f) -> void
    {
        pushLinePoint(static_cast<double>(x - size), y, z, pickId);
        pushLinePoint(static_cast<double>(x + size), y, z, pickId);
        pushLinePoint(x, static_cast<double>(y - size), z, pickId);
        pushLinePoint(x, static_cast<double>(y + size), z, pickId);
        pushLinePoint(x, y, static_cast<double>(z - size), pickId);
        pushLinePoint(x, y, static_cast<double>(z + size), pickId);
    };

    auto resolveThingBox = [&](const Thing& thing) -> ThingBox
    {
        return render::resolveThingRenderBox(*level_, templates_, thing);
    };

    auto addBox = [&](const TMatrix43& xf, const ThingBox& box, const float pickId = 0.0f) -> void
    {
        const auto wirePoints = render::makeThingBoxWirePoints(xf, box, pickId);
        appendWirePoints(wirePoints);
    };

    auto addCubeOutline = [&](const TMatrix43& xf, const float halfSize, const float pickId = 0.0f) -> void
    {
        const auto wirePoints = render::makeThingCubeWirePoints(xf, static_cast<double>(halfSize), pickId);
        appendWirePoints(wirePoints);
    };

    auto addModelWireframe = [&](const assets::Model3do& model, const TMatrix43& xf, const float pickId = 0.0f) -> void
    {
        std::vector<render::ThingWirePoint> wirePoints{};
        render::appendModelWireframeWirePoints(wirePoints, model, xf, pickId);
        appendWirePoints(wirePoints);
    };

    auto highlightThing = [&](const std::int32_t thingIndex) -> void
    {
        for (int once = 0; once < 1; ++once)
        {
        if (!isThingVisible(thingIndex))
        {
            continue;
        }

        if (thingIndex < 0 || static_cast<std::size_t>(thingIndex) >= level_->things.size())
        {
            continue;
        }

        const Thing& thing = level_->things[static_cast<std::size_t>(thingIndex)];
        if (!std::isfinite(thing.x) || !std::isfinite(thing.y) || !std::isfinite(thing.z))
        {
            continue;
        }

        const float x = static_cast<float>(thing.x);
        const float y = static_cast<float>(thing.y);
        const float z = static_cast<float>(thing.z);

        float thingPickId = 0.0f;
        if (static_cast<std::size_t>(thingIndex) < thingPickIds_.size())
        {
            thingPickId = static_cast<float>(thingPickIds_[static_cast<std::size_t>(thingIndex)]);
        }

        TVector pyr{};
        pyr.pitch = thing.pch;
        pyr.yaw   = thing.yaw;
        pyr.roll  = thing.rol;
        const TMatrix43 thingXf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, pyr);

        if (thingViewMode_ == ThingViewMode::Dots)
        {
            addCross(x, y, z, crossSize, thingPickId);
            continue;
        }

        // Shaded selection is emitted by the filled outline pass below. Do
        // not resolve or load the selected Thing's 3DO merely to discover
        // that this wire-overlay path has nothing to add.
        if (thingViewMode_ == ThingViewMode::Shaded)
        {
            continue;
        }

        // In non-Shaded modes, check for sprites first and draw wireframe
        // edges.  This matches the rendering priority (sprites before models)
        // and ensures the highlight frame matches the rendered sprite shape.
        if (thingViewMode_ != ThingViewMode::Shaded)
        {
            const TplValue* sprValue = sed::level::thingFindValue(*level_, templates_, thing, "sprite");
            if (sprValue)
            {
                const std::string sprName = sprValue->stringValue.empty() ? sprValue->asString() : sprValue->stringValue;
                if (!sprName.empty())
                {
                    if (const assets::SprFile* spr = loadSprite(sprName))
                    {
                        const int spriteType = originalSpriteRenderType(level_->kind, spr->type);
                        const bool type2Supported = (spriteType != 2) || (level_->kind == ProjectType::IJIM);
                        if (type2Supported)
                        {
                            const float rollDeg = normalizeRollDeg(static_cast<float>(thing.rol));
                            const float widthHalf = spr->width * 0.5f;
                            const float heightHalf = spr->height * 0.5f;
                            QVector3D center(x, y, z);

                            auto addEdge = [&](const QVector3D& a, const QVector3D& b)
                            {
                                pushLinePoint(static_cast<double>(a.x()), static_cast<double>(a.y()), static_cast<double>(a.z()), thingPickId);
                                pushLinePoint(static_cast<double>(b.x()), static_cast<double>(b.y()), static_cast<double>(b.z()), thingPickId);
                            };

                            if (spriteType == 0)
                            {
                                TVector placementNoRoll = pyr;
                                placementNoRoll.roll = 0.0;
                                const SpriteBasis authoredBasis = makeSpriteBasis(placementNoRoll);
                                const bool billboard =
                                    render::fast_update::usesOriginalSpriteCameraFacingFrame(
                                        spriteType,
                                        renderPipeline_ == RenderPipeline::GamePreview);
                                const SpriteBasis displayBasis = billboard
                                    ? makeCameraFacingSpriteBasis(camera_.orbit)
                                    : authoredBasis;
                                QVector3D right = displayBasis.right;
                                QVector3D up = displayBasis.up;
                                QVector3D forward = displayBasis.forward;

                                if (rollDeg != 0.0f)
                                {
                                    const float rollRad = DegToRad(rollDeg);
                                    right = rotateAroundAxis(right, forward, rollRad);
                                    up = rotateAroundAxis(up, forward, rollRad);
                                }

                                center = center
                                    + right * spr->offsetXYZ[0]
                                    + forward * spr->offsetXYZ[1]
                                    + up * spr->offsetXYZ[2];

                                const QVector3D r = right * widthHalf;
                                const QVector3D u = up * heightHalf;
                                const QVector3D p0 = center - r - u;
                                const QVector3D p1 = center + r - u;
                                const QVector3D p2 = center + r + u;
                                const QVector3D p3 = center - r + u;

                                addEdge(p0, p1);
                                addEdge(p1, p2);
                                addEdge(p2, p3);
                                addEdge(p3, p0);
                            }
                            else if (spriteType == 2)
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
                                if (normalizeInPlace(xVec))
                                {
                                    QVector3D zVec = QVector3D::crossProduct(xVec, yVec);
                                    if (normalizeInPlace(zVec))
                                    {
                                        const QVector3D rightHalf = xVec * widthHalf;
                                        const QVector3D upHalf = zVec * heightHalf;
                                        const QVector3D leftHalf = -rightHalf;

                                        QVector3D v0 = leftHalf - upHalf;
                                        QVector3D v1 = rightHalf - upHalf;
                                        QVector3D v2 = rightHalf + upHalf;
                                        QVector3D v3 = leftHalf + upHalf;

                                        if (rollDeg != 0.0f)
                                        {
                                            const float rollRad = DegToRad(rollDeg);
                                            v0 = rotateAroundAxis(v0, yVec, rollRad);
                                            v1 = rotateAroundAxis(v1, yVec, rollRad);
                                            v2 = rotateAroundAxis(v2, yVec, rollRad);
                                            v3 = rotateAroundAxis(v3, yVec, rollRad);
                                        }

                                        const QVector3D p0 = center + v0;
                                        const QVector3D p1 = center + v1;
                                        const QVector3D p2 = center + v2;
                                        const QVector3D p3 = center + v3;

                                        addEdge(p0, p1);
                                        addEdge(p1, p2);
                                        addEdge(p2, p3);
                                        addEdge(p3, p0);
                                    }
                                }
                            }

                            continue;
                        }
                    }
                }
            }
        }

        if (thingViewMode_ == ThingViewMode::Boxes)
        {
            const ThingBox box = resolveThingBox(thing);
            if (box.IsZero())
            {
                addCubeOutline(thingXf, thingCubeHalfSize_, thingPickId);
                continue;
            }

            addBox(thingXf, box, thingPickId);
            continue;
        }

        const TplValue* modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3d");
        if (!modelValue)
        {
            modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3do");
        }

        const std::string modelName = modelValue ? (modelValue->stringValue.empty() ? modelValue->asString() : modelValue->stringValue) : std::string{};
        const std::size_t visibleLod =
            static_cast<std::size_t>(thingIndex) < thingRenderState_.size()
            ? thingRenderState_[static_cast<std::size_t>(thingIndex)].lodIndex
            : 0u;
        const std::shared_ptr<const assets::Model3do> modelPtr =
            loadModel3do(modelName, visibleLod);

        if (modelPtr && !modelPtr->meshes.empty() && thingViewMode_ == ThingViewMode::Wireframes)
        {
            addModelWireframe(*modelPtr, thingXf, thingPickId);
        }
        else if (thingViewMode_ != ThingViewMode::Shaded)
        {
            const ThingBox box = resolveThingBox(thing);
            if (!box.IsZero())
            {
                addBox(thingXf, box, thingPickId);
            }
            else
            {
                addCubeOutline(thingXf, thingCubeHalfSize_, thingPickId);
            }
        }
        }
    };

    // Step 6: queue highlighted non-primary things in stable order and draw them as multi-selection wire overlays.
    std::vector<std::int32_t> multiThings{};
    multiThings.reserve(highlightedSelection_.things.size());
    for (const std::int32_t idx : highlightedSelection_.things)
    {
        if (idx == selectedThing_)
        {
            continue;
        }
        multiThings.push_back(idx);
    }
    std::sort(multiThings.begin(), multiThings.end());
    multiThings.erase(std::unique(multiThings.begin(), multiThings.end()), multiThings.end());

    for (const std::int32_t idx : multiThings)
    {
        highlightThing(idx);
    }

    secondaryEdgeHighlightFirstVertex_ =
        static_cast<int>(highlightLineVertices_.size());
    for (const SurfaceEdgeSelectionRef& edge : selectedSecondaryEdges)
    {
        const Sector& sector = level_->sectors[
            static_cast<std::size_t>(edge.surface.sector)];
        const Vertex& first = sector.vertices[
            static_cast<std::size_t>(edge.firstVertex)];
        const Vertex& second = sector.vertices[
            static_cast<std::size_t>(edge.secondVertex)];
        const int firstHighlight = static_cast<int>(highlightLineVertices_.size());
        const float pickId = lookupSurfacePickId(edge.surface);
        pushLinePoint(first.x, first.y, first.z, pickId);
        pushLinePoint(second.x, second.y, second.z, pickId);
        appendGeometryHighlightRange(firstHighlight, 2);
    }
    secondaryEdgeHighlightVertexCount_ =
        static_cast<int>(highlightLineVertices_.size())
        - secondaryEdgeHighlightFirstVertex_;

    // Split point between multi-selection and the primary selection for coloring.
    highlightLineMultiVertexCount_ = static_cast<int>(highlightLineVertices_.size());

    // Step 7: draw the primary sector/surface/thing after the multi-selection split so it renders brighter.
    if (selectedSector_ >= 0)
    {
        highlightSector(selectedSector_, false);
    }
    highlightSelectedSurface();
    if (selectedThing_ >= 0)
    {
        highlightThing(selectedThing_);
    }

    if (showSelectedSurfaceNormals_)
    {
        // Keep the same primary -> vertex-surfaces -> face order used by the
        // transform gizmo.  A single selected face therefore shares the exact
        // gizmo pivot; in a multi-selection each arrow remains attached to its
        // own face while the gizmo uses the aggregate selection pivot.
        std::vector<SurfaceRef> normalSurfaces{};
        auto addNormalSurface = [&](const SurfaceRef ref)
        {
            if (std::find(normalSurfaces.begin(), normalSurfaces.end(), ref)
                == normalSurfaces.end())
            {
                normalSurfaces.push_back(ref);
            }
        };
        if (haveSelectedSurface)
        {
            addNormalSurface(selectedRef);
        }
        for (const editor::SurfaceVertexRef& vertexRef : highlightedSelection_.surfaceVertices)
        {
            addNormalSurface(vertexRef.surface);
        }
        for (const SurfaceRef ref : highlightedSelection_.surfaces)
        {
            addNormalSurface(ref);
        }

        surfaceNormalHighlightFirstVertex_ = static_cast<int>(highlightLineVertices_.size());
        for (const SurfaceRef ref : normalSurfaces)
        {
            if (ref.sector < 0 || ref.surface < 0 || !isSectorVisible(ref.sector)
                || static_cast<std::size_t>(ref.sector) >= level_->sectors.size())
            {
                continue;
            }
            const Sector& sector = level_->sectors[static_cast<std::size_t>(ref.sector)];
            if (static_cast<std::size_t>(ref.surface) >= sector.surfaces.size())
            {
                continue;
            }
            const Surface& surface = sector.surfaces[static_cast<std::size_t>(ref.surface)];
            QVector3D minv{};
            QVector3D maxv{};
            bool havePoint = false;
            for (const std::int32_t vertexIndex : surface.vertices)
            {
                if (vertexIndex < 0
                    || static_cast<std::size_t>(vertexIndex) >= sector.vertices.size())
                {
                    continue;
                }
                const Vertex& vertex = sector.vertices[static_cast<std::size_t>(vertexIndex)];
                const QVector3D point(
                    static_cast<float>(vertex.x),
                    static_cast<float>(vertex.y),
                    static_cast<float>(vertex.z));
                if (!std::isfinite(point.x()) || !std::isfinite(point.y())
                    || !std::isfinite(point.z()))
                {
                    continue;
                }
                if (!havePoint)
                {
                    minv = maxv = point;
                    havePoint = true;
                }
                else
                {
                    minv.setX(std::min(minv.x(), point.x()));
                    minv.setY(std::min(minv.y(), point.y()));
                    minv.setZ(std::min(minv.z(), point.z()));
                    maxv.setX(std::max(maxv.x(), point.x()));
                    maxv.setY(std::max(maxv.y(), point.y()));
                    maxv.setZ(std::max(maxv.z(), point.z()));
                }
            }
            if (!havePoint)
            {
                continue;
            }
            // A normal describes the face even while the editor is in edge or
            // vertex submode. Keep it at the same canonical area centroid
            // used by the single-face transform gizmo and cleave marker,
            // rather than following the last hit edge/vertex.
            const std::optional<QVector3D> center =
                render::surfaceFaceCenter(sector, surface);
            if (!center)
            {
                continue;
            }
            const std::optional<QVector3D> displayNormal =
                render::surfaceDisplayNormal(sector, surface);
            if (!displayNormal)
            {
                continue;
            }
            const QVector3D normal = *displayNormal;

            const float baseLength = std::clamp((maxv - minv).length() * 0.28f, 0.15f, 2.0f);
            const float length = std::clamp(
                baseLength * surfaceNormalLengthScale_,
                0.02f,
                10.0f);
            // Keep the mathematical origin exactly on the shared transform
            // pivot.  Depth bias is handled by the dedicated overlay passes;
            // moving the origin here made the marker disagree with the gizmo.
            const QVector3D origin = *center;
            const QVector3D tip = origin + normal * length;
            const SpriteBasis cameraBasis =
                makeCameraFacingSpriteBasis(camera_.orbit);
            const std::optional<QVector3D> initialSide =
                render::surfaceNormalArrowheadSide(
                    normal, camera_.eye() - tip, cameraBasis.right,
                    cameraBasis.up);
            const float pickId = lookupSurfacePickId(ref);
            auto appendNormalLine = [&](const QVector3D& a, const QVector3D& b)
            {
                pushLinePoint(a.x(), a.y(), a.z(), pickId);
                pushLinePoint(b.x(), b.y(), b.z(), pickId);
            };
            appendNormalLine(origin, tip);
            if (showSurfaceNormalArrowhead_ && initialSide)
            {
                const float headLength = length * 0.25f * surfaceNormalArrowheadScale_;
                const float headWidth = length * 0.12f * surfaceNormalArrowheadScale_;
                const QVector3D headBase = tip - normal * headLength;
                const int firstArrowheadVertex =
                    static_cast<int>(highlightLineVertices_.size());
                appendNormalLine(tip, headBase + *initialSide * headWidth);
                appendNormalLine(tip, headBase - *initialSide * headWidth);
                surfaceNormalArrowheads_.push_back(SurfaceNormalArrowhead{
                    firstArrowheadVertex, tip, normal, headLength, headWidth});
            }
        }

        // Edge mode shows a second, edge-specific normal at the selected edge
        // midpoint.  For a normal manifold edge this is the normalized
        // bisector of the two owning face normals; it falls back to the active
        // face normal on incomplete recovery geometry.
        if (selectionSurfaceMode == SurfaceSubMode::Edge && selectedActiveEdge
            && haveSelectedSurface && selectedRef.sector >= 0
            && static_cast<std::size_t>(selectedRef.sector) < level_->sectors.size())
        {
            const Sector& sector =
                level_->sectors[static_cast<std::size_t>(selectedRef.sector)];
            const std::int32_t firstIndex = selectedActiveEdge->first;
            const std::int32_t secondIndex = selectedActiveEdge->second;
            if (firstIndex >= 0 && secondIndex >= 0
                && static_cast<std::size_t>(firstIndex) < sector.vertices.size()
                && static_cast<std::size_t>(secondIndex) < sector.vertices.size()
                && selectedRef.surface >= 0
                && static_cast<std::size_t>(selectedRef.surface) < sector.surfaces.size())
            {
                const Vertex& first = sector.vertices[static_cast<std::size_t>(firstIndex)];
                const Vertex& second = sector.vertices[static_cast<std::size_t>(secondIndex)];
                const QVector3D a(static_cast<float>(first.x),
                    static_cast<float>(first.y), static_cast<float>(first.z));
                const QVector3D b(static_cast<float>(second.x),
                    static_cast<float>(second.y), static_cast<float>(second.z));
                QVector3D edgeNormal = render::surfaceDisplayNormal(
                    sector, sector.surfaces[static_cast<std::size_t>(selectedRef.surface)])
                    .value_or(QVector3D{});
                for (std::size_t surfaceIndex = 0;
                    surfaceIndex < sector.surfaces.size(); ++surfaceIndex)
                {
                    if (surfaceIndex == static_cast<std::size_t>(selectedRef.surface))
                    {
                        continue;
                    }
                    const Surface& candidate = sector.surfaces[surfaceIndex];
                    bool sharesEdge = false;
                    for (std::size_t edge = 0; edge < candidate.vertices.size(); ++edge)
                    {
                        const std::int32_t ca = candidate.vertices[edge];
                        const std::int32_t cb = candidate.vertices[
                            (edge + 1) % candidate.vertices.size()];
                        sharesEdge = (ca == firstIndex && cb == secondIndex)
                            || (ca == secondIndex && cb == firstIndex);
                        if (sharesEdge)
                        {
                            break;
                        }
                    }
                    if (!sharesEdge)
                    {
                        continue;
                    }
                    if (const std::optional<QVector3D> adjacentNormal =
                            render::surfaceDisplayNormal(sector, candidate))
                    {
                        const QVector3D bisector = edgeNormal + *adjacentNormal;
                        if (bisector.lengthSquared() > sed::math::kTinyEpsilonF)
                        {
                            edgeNormal = bisector.normalized();
                        }
                    }
                    break;
                }
                if (edgeNormal.lengthSquared() > sed::math::kTinyEpsilonF)
                {
                    edgeNormal.normalize();
                    const QVector3D origin = (a + b) * 0.5f;
                    const float length = std::clamp(
                        (b - a).length() * 0.35f * surfaceNormalLengthScale_,
                        0.02f, 10.0f);
                    const QVector3D tip = origin + edgeNormal * length;
                    const float pickId = lookupSurfacePickId(selectedRef);
                    auto appendEdgeNormalLine = [&](const QVector3D& from,
                        const QVector3D& to)
                    {
                        pushLinePoint(from.x(), from.y(), from.z(), pickId);
                        pushLinePoint(to.x(), to.y(), to.z(), pickId);
                    };
                    appendEdgeNormalLine(origin, tip);
                    const SpriteBasis cameraBasis =
                        makeCameraFacingSpriteBasis(camera_.orbit);
                    const std::optional<QVector3D> side =
                        render::surfaceNormalArrowheadSide(edgeNormal,
                            camera_.eye() - tip, cameraBasis.right,
                            cameraBasis.up);
                    if (showSurfaceNormalArrowhead_ && side)
                    {
                        const float headLength = length * 0.25f
                            * surfaceNormalArrowheadScale_;
                        const float headWidth = length * 0.12f
                            * surfaceNormalArrowheadScale_;
                        const QVector3D headBase = tip - edgeNormal * headLength;
                        const int firstArrowheadVertex =
                            static_cast<int>(highlightLineVertices_.size());
                        appendEdgeNormalLine(tip, headBase + *side * headWidth);
                        appendEdgeNormalLine(tip, headBase - *side * headWidth);
                        surfaceNormalArrowheads_.push_back(SurfaceNormalArrowhead{
                            firstArrowheadVertex, tip, edgeNormal,
                            headLength, headWidth });
                    }
                }
            }
        }
        surfaceNormalHighlightVertexCount_ =
            static_cast<int>(highlightLineVertices_.size())
            - surfaceNormalHighlightFirstVertex_;
        // Normals are a separate visual marker, not selected geometry.  Keep
        // their range out of geometrySelectionLineRanges_: the visible pass
        // draws the marker in its own cyan pass and the occluded pass does the
        // same with reduced alpha.  Including it in the ordinary geometry
        // ranges would first paint it with the primary selection color and
        // then paint it again, producing the red/yellow mix seen on arrows.
    }

    if (vertexMode)
    {
        // Step 8: add selected surface-vertex point markers, then append the active vertex point on top.
        highlightPointSelectedFirstPoint_ = static_cast<int>(highlightPointPositions_.size() / 3);
        highlightPointSelectedPointCount_ = 0;

        std::optional<editor::SurfaceVertexRef> activeSelectedVertexRef{};
        auto validSurfaceVertexRef = [&](const editor::SurfaceVertexRef& vref) -> bool
        {
            if (vref.surface.sector < 0 || vref.surface.surface < 0
                || vref.localVertex < 0 || !isSectorVisible(vref.surface.sector)
                || static_cast<std::size_t>(vref.surface.sector)
                    >= level_->sectors.size())
            {
                return false;
            }
            const Sector& sector = level_->sectors[
                static_cast<std::size_t>(vref.surface.sector)];
            if (static_cast<std::size_t>(vref.surface.surface)
                >= sector.surfaces.size())
            {
                return false;
            }
            const Surface& surface = sector.surfaces[
                static_cast<std::size_t>(vref.surface.surface)];
            if (static_cast<std::size_t>(vref.localVertex)
                >= surface.vertices.size())
            {
                return false;
            }
            const std::int32_t vertexIndex = surface.vertices[
                static_cast<std::size_t>(vref.localVertex)];
            if (vertexIndex < 0
                || static_cast<std::size_t>(vertexIndex)
                    >= sector.vertices.size())
            {
                return false;
            }
            const Vertex& vertex = sector.vertices[
                static_cast<std::size_t>(vertexIndex)];
            return std::isfinite(vertex.x) && std::isfinite(vertex.y)
                && std::isfinite(vertex.z);
        };
        auto considerActiveVertexRef = [&](const editor::SurfaceVertexRef& vref) -> void
        {
            if (!validSurfaceVertexRef(vref))
            {
                return;
            }
            if (!activeSelectedVertexRef)
            {
                activeSelectedVertexRef = vref;
                return;
            }

            const editor::SurfaceVertexRef& cur = *activeSelectedVertexRef;
            if (vref.surface.sector < cur.surface.sector
                || (vref.surface.sector == cur.surface.sector && vref.surface.surface < cur.surface.surface)
                || (vref.surface == cur.surface && vref.localVertex < cur.localVertex))
            {
                activeSelectedVertexRef = vref;
            }
        };

        if (activeSurfaceVertexHintValid_ && highlightedSelection_.surfaceVertices.contains(activeSurfaceVertexHint_))
        {
            considerActiveVertexRef(activeSurfaceVertexHint_);
        }

        if (!highlightedSelection_.surfaceVertices.empty())
        {
            if (!activeSelectedVertexRef && haveSelectedSurface)
            {
                for (const editor::SurfaceVertexRef& vref : highlightedSelection_.surfaceVertices)
                {
                    if (vref.surface == selectedRef)
                    {
                        considerActiveVertexRef(vref);
                    }
                }
            }

            if (!activeSelectedVertexRef)
            {
                for (const editor::SurfaceVertexRef& vref : highlightedSelection_.surfaceVertices)
                {
                    considerActiveVertexRef(vref);
                }
            }
        }

        std::optional<std::uint64_t> activeSectorVertexKey{};
        if (activeSelectedVertexRef
            && validSurfaceVertexRef(*activeSelectedVertexRef))
        {
            const editor::SurfaceVertexRef& ref = *activeSelectedVertexRef;
            const Sector& sector = level_->sectors[
                static_cast<std::size_t>(ref.surface.sector)];
            const Surface& surface = sector.surfaces[
                static_cast<std::size_t>(ref.surface.surface)];
            const std::int32_t vertexIndex = surface.vertices[
                static_cast<std::size_t>(ref.localVertex)];
            const Vertex& vertex = sector.vertices[
                static_cast<std::size_t>(vertexIndex)];
            activeVertexPoint = TVector{ vertex.x, vertex.y, vertex.z };
            activeSectorVertexKey =
                (static_cast<std::uint64_t>(
                    static_cast<std::uint32_t>(ref.surface.sector)) << 32u)
                | static_cast<std::uint64_t>(
                    static_cast<std::uint32_t>(vertexIndex));
        }

        std::unordered_set<std::uint64_t> emittedSectorVertices{};
        emittedSectorVertices.reserve(
            highlightedSelection_.surfaceVertices.size());
        std::vector<TVector> emittedVertexPoints{};
        emittedVertexPoints.reserve(
            highlightedSelection_.surfaceVertices.size());
        for (const editor::SurfaceVertexRef& vref : highlightedSelection_.surfaceVertices)
        {
            if (vref.surface.sector < 0 || vref.surface.surface < 0 || vref.localVertex < 0)
            {
                continue;
            }
            if (!isSectorVisible(vref.surface.sector))
            {
                continue;
            }

            const std::size_t secIndex = static_cast<std::size_t>(vref.surface.sector);
            if (secIndex >= level_->sectors.size())
            {
                continue;
            }

            const Sector& sector = level_->sectors[secIndex];
            const std::size_t surfIndex = static_cast<std::size_t>(vref.surface.surface);
            if (surfIndex >= sector.surfaces.size())
            {
                continue;
            }

            const Surface& surface = sector.surfaces[surfIndex];
            const std::size_t local = static_cast<std::size_t>(vref.localVertex);
            if (local >= surface.vertices.size())
            {
                continue;
            }

            const std::int32_t vi = surface.vertices[local];
            if (vi < 0 || static_cast<std::size_t>(vi) >= sector.vertices.size())
            {
                continue;
            }

            const Vertex& v = sector.vertices[static_cast<std::size_t>(vi)];
            if (!std::isfinite(v.x) || !std::isfinite(v.y)
                || !std::isfinite(v.z))
            {
                continue;
            }
            const std::uint64_t key =
                (static_cast<std::uint64_t>(
                    static_cast<std::uint32_t>(vref.surface.sector)) << 32u)
                | static_cast<std::uint64_t>(
                    static_cast<std::uint32_t>(vi));
            // The active physical vertex owns the red point exclusively.
            // Adjacent faces can reference that same sector vertex; none of
            // those aliases may also enter the secondary blue range.
            if (activeSectorVertexKey && key == *activeSectorVertexKey)
            {
                continue;
            }
            const TVector point{ v.x, v.y, v.z };
            const auto coincides = [&point](const TVector& other)
            {
                const TVector delta = point - other;
                return delta.Dot(delta) <= 1.0e-10;
            };
            if ((activeVertexPoint && coincides(*activeVertexPoint))
                || std::any_of(
                    emittedVertexPoints.begin(),
                    emittedVertexPoints.end(),
                    coincides))
            {
                continue;
            }
            if (!emittedSectorVertices.insert(key).second)
            {
                continue;
            }
            pushHighlightPoint(v.x, v.y, v.z);
            emittedVertexPoints.push_back(point);
        }

        const int after = static_cast<int>(highlightPointPositions_.size() / 3);
        highlightPointSelectedPointCount_ = std::max(0, after - highlightPointSelectedFirstPoint_);

        if (activeVertexPoint && std::isfinite(activeVertexPoint->x) && std::isfinite(activeVertexPoint->y) && std::isfinite(activeVertexPoint->z))
        {
            activeVertexHighlightFirstPoint_ = static_cast<int>(highlightPointPositions_.size() / 3);
            pushHighlightPoint(activeVertexPoint->x, activeVertexPoint->y, activeVertexPoint->z);
            activeVertexHighlightPointCount_ = 1;
        }
    }

    // Step 9: in shaded thing view, build filled outline triangles from cached meshes, sprites, models, or fallback cubes.
    if (showThings_ && thingViewMode_ == ThingViewMode::Shaded)
    {
        // Picking/rendering publishes current sprite vertices before selection
        // reaches this path. Do not regenerate every sprite in a large level
        // merely to extract the selected Thing's outline.

        std::vector<std::int32_t> outlineCandidates{};
        outlineCandidates.reserve(highlightedSelection_.things.size() + 1);
        std::optional<std::size_t> activeOutlineFirstTri{};

        // Adds one visible thing to the outline pass at most once. Candidate collection is separated
        // from drawing so the primary selected thing can be appended last and get the active color split.
        auto appendOutlineCandidate = [&](const std::int32_t idx) -> void
        {
            if (idx < 0 || static_cast<std::size_t>(idx) >= level_->things.size())
            {
                return;
            }
            if (!isThingVisible(idx))
            {
                return;
            }
            if (std::find(outlineCandidates.begin(), outlineCandidates.end(), idx) != outlineCandidates.end())
            {
                return;
            }
            outlineCandidates.push_back(idx);
        };

        // Step 9a: queue multi-selected things first; the primary selected thing is skipped here and appended last.
        for (const std::int32_t idx : highlightedSelection_.things)
        {
            if (idx == selectedThing_)
            {
                continue;
            }
            appendOutlineCandidate(idx);
        }
        appendOutlineCandidate(selectedThing_);

        // Step 9b: build each thing outline in queue order, recording where the primary selected outline begins.
        for (const std::int32_t outlineCandidate : outlineCandidates)
        {
            if (outlineCandidate == selectedThing_ && !activeOutlineFirstTri.has_value())
            {
                activeOutlineFirstTri = outlineTriVertices_.size();
            }

            const Thing& thing = level_->things[static_cast<std::size_t>(outlineCandidate)];
            if (std::isfinite(thing.x) && std::isfinite(thing.y) && std::isfinite(thing.z))
            {
                const std::size_t candidateFirstTri = outlineTriVertices_.size();
                float outlinePickId = 0.0f;
                if (static_cast<std::size_t>(outlineCandidate) < thingPickIds_.size())
                {
                    outlinePickId = static_cast<float>(thingPickIds_[static_cast<std::size_t>(outlineCandidate)]);
                }

                bool haveOutlineBounds = false;
                TVector outlineMin{};
                TVector outlineMax{};
                auto expandOutlineBounds = [&](const TVector& p) -> void
                {
                    if (!haveOutlineBounds)
                    {
                        outlineMin = p;
                        outlineMax = p;
                        haveOutlineBounds = true;
                        return;
                    }

                    outlineMin.x = std::min(outlineMin.x, p.x);
                    outlineMin.y = std::min(outlineMin.y, p.y);
                    outlineMin.z = std::min(outlineMin.z, p.z);

                    outlineMax.x = std::max(outlineMax.x, p.x);
                    outlineMax.y = std::max(outlineMax.y, p.y);
                    outlineMax.z = std::max(outlineMax.z, p.z);
                };

                bool outlineBuilt = false;

                // Fast path: reference the selected Thing's exact ranges in the
                // already-uploaded scene buffers. The former implementation
                // copied every vertex into outlineTriVertices_ and uploaded a
                // second VBO on every click, making selection cost proportional
                // to 3DO complexity.
                {
                    const int pid = static_cast<int>(outlinePickId + 0.5f);
                    if (pid > 0 && thingMeshCache_.valid)
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
                                if (r.first < 0 || r.count <= 0
                                    || (static_cast<std::size_t>(r.first + r.count) > b.vertices.size()))
                                {
                                    continue;
                                }

                                if (r.buildIndex >= thingTriBuildGlobalFirstVertex_.size())
                                {
                                    continue;
                                }

                                const int globalFirst =
                                    thingTriBuildGlobalFirstVertex_[static_cast<std::size_t>(r.buildIndex)];
                                if (globalFirst < 0
                                    || globalFirst + r.first + r.count > static_cast<int>(triVertices_.size()))
                                {
                                    continue;
                                }

                                outlinedThingDoubleSided_ = outlinedThingDoubleSided_ || b.doubleSided;
                                outlineSourceRanges_.push_back(OutlineSourceRange{
                                    OutlineSource::ThingTriangles,
                                    globalFirst + r.first,
                                    r.count,
                                    outlineCandidate == selectedThing_,
                                });
                                outlineBuilt = true;
                            }
                        }
                    }
                }

                // Sprites live in their own persistent VBO. Reuse its exact
                // rendered ranges as well, avoiding sprite/template lookup on
                // the selection path.
                if (!outlineBuilt)
                {
                    const int pid = static_cast<int>(outlinePickId + 0.5f);
                    if (pid > 0)
                    {
                        if (const auto it = spriteTriRangesByPickId_.find(pid);
                            it != spriteTriRangesByPickId_.end())
                        {
                            for (const DrawRange& range : it->second)
                            {
                                if (range.first < 0 || range.count <= 0
                                    || range.first + range.count > static_cast<int>(spriteTriVertices_.size()))
                                {
                                    continue;
                                }
                                outlineSourceRanges_.push_back(OutlineSourceRange{
                                    OutlineSource::SpriteTriangles,
                                    range.first,
                                    range.count,
                                    outlineCandidate == selectedThing_,
                                });
                                outlineBuilt = true;
                            }
                        }
                    }
                    if (outlineBuilt)
                    {
                        outlinedThingDoubleSided_ = true;
                    }
                }

                if (outlineBuilt)
                {
                    ++selectionOutlineCacheHitCount_;
                    outlinedThing_ = outlineCandidate;

                    const std::size_t thingIdx = static_cast<std::size_t>(outlineCandidate);
                    const ThingBounds* cachedBounds = nullptr;
                    if (thingIdx < spriteThingBounds_.size() && spriteThingBounds_[thingIdx].valid)
                    {
                        cachedBounds = &spriteThingBounds_[thingIdx];
                    }
                    else if (thingIdx < thingBounds_.size() && thingBounds_[thingIdx].valid)
                    {
                        cachedBounds = &thingBounds_[thingIdx];
                    }

                    if (cachedBounds)
                    {
                        outlineMin = TVector{
                            static_cast<double>(cachedBounds->min.x()),
                            static_cast<double>(cachedBounds->min.y()),
                            static_cast<double>(cachedBounds->min.z()),
                        };
                        outlineMax = TVector{
                            static_cast<double>(cachedBounds->max.x()),
                            static_cast<double>(cachedBounds->max.y()),
                            static_cast<double>(cachedBounds->max.z()),
                        };
                        haveOutlineBounds = true;
                    }

                    if (haveOutlineBounds)
                    {
                        outlinedThingCenter_ = QVector3D(
                            static_cast<float>((outlineMin.x + outlineMax.x) * 0.5),
                            static_cast<float>((outlineMin.y + outlineMax.y) * 0.5),
                            static_cast<float>((outlineMin.z + outlineMax.z) * 0.5));

                        // If the object is very thin (plane-like), don't cull the outline so it remains visible
                        // from both sides.
                        const QVector3D ext(
                            static_cast<float>(outlineMax.x - outlineMin.x),
                            static_cast<float>(outlineMax.y - outlineMin.y),
                            static_cast<float>(outlineMax.z - outlineMin.z));
                        const float maxE = std::max({ std::abs(ext.x()), std::abs(ext.y()), std::abs(ext.z()) });
                        const float minE = std::min({ std::abs(ext.x()), std::abs(ext.y()), std::abs(ext.z()) });
                        if (maxE > 1e-6f && (minE / maxE) < 0.02f)
                        {
                            outlinedThingDoubleSided_ = true;
                        }
                    }
                    else
                    {
                        outlinedThingCenter_ =
                            QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));
                    }
                }

                if (!outlineBuilt)
                {
                ++selectionOutlineFallbackBuildCount_;
                // Check for sprite first: sprite takes rendering priority over model3do.
                // The rendering path processes sprites before models, so the outline
                // must use the same priority to avoid mismatches.
                bool hasSupportedSprite = false;
                {
                    const TplValue* sv = sed::level::thingFindValue(*level_, templates_, thing, "sprite");
                    if (sv)
                    {
                        const std::string sn = sv->stringValue.empty() ? sv->asString() : sv->stringValue;
                        if (!sn.empty())
                        {
                            if (const assets::SprFile* sp = loadSprite(sn))
                            {
                                hasSupportedSprite = originalSpriteRenderType(level_->kind, sp->type) != 2
                                    || level_->kind == ProjectType::IJIM;
                            }
                        }
                    }
                }

                const TplValue* modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3d");
                if (!modelValue)
                {
                    modelValue = sed::level::thingFindValue(*level_, templates_, thing, "model3do");
                }

                const std::string modelName =
                    modelValue ? (modelValue->stringValue.empty() ? modelValue->asString() : modelValue->stringValue) : std::string{};
                const std::size_t visibleLod =
                    static_cast<std::size_t>(outlineCandidate) < thingRenderState_.size()
                    ? thingRenderState_[static_cast<std::size_t>(outlineCandidate)].lodIndex
                    : 0u;
                const std::shared_ptr<const assets::Model3do> modelPtr =
                    loadModel3do(modelName, visibleLod);

                if (!hasSupportedSprite && modelPtr && !modelPtr->meshes.empty())
                {
                    TVector pyr{};
                    pyr.pitch = thing.pch;
                    pyr.yaw = thing.yaw;
                    pyr.roll = thing.rol;
                    const TMatrix43 thingXf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, pyr);

                    for (const assets::Model3doMesh& mesh : modelPtr->meshes)
                    {
                        for (const assets::Model3doFace& face : mesh.faces)
                        {
                            const std::size_t count = face.vertices.size();
                            if (count < 3)
                            {
                                continue;
                            }

                            std::vector<TVector> facePoints{};
                            facePoints.reserve(count);

                            bool faceValid = true;
                            for (std::size_t i = 0; i < count; ++i)
                            {
                                const std::int32_t vertexIndex = face.vertices[i];
                                if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= mesh.vertices.size())
                                {
                                    faceValid = false;
                                    break;
                                }

                                const assets::Model3doVertex& mv = mesh.vertices[static_cast<std::size_t>(vertexIndex)];
                                facePoints.push_back(thingXf.TransformPoint(TVector{ mv.x, mv.y, mv.z }));
                            }

                            if (!faceValid)
                            {
                                continue;
                            }

                            triangulatePolyZigZag(static_cast<int>(count), [&](const int i0, const int i1, const int i2)
                            {
                                const TVector& p0 = facePoints[static_cast<std::size_t>(i0)];
                                const TVector& p1 = facePoints[static_cast<std::size_t>(i1)];
                                const TVector& p2 = facePoints[static_cast<std::size_t>(i2)];

                                expandOutlineBounds(p0);
                                expandOutlineBounds(p1);
                                expandOutlineBounds(p2);

                                TriVertex out0{};
                                out0.x = static_cast<float>(p0.x);
                                out0.y = static_cast<float>(p0.y);
                                out0.z = static_cast<float>(p0.z);
                                out0.r = 1.0f;
                                out0.g = 1.0f;
                                out0.b = 1.0f;
                                out0.a = 1.0f;
                                out0.pickId = outlinePickId;

                                TriVertex out1 = out0;
                                out1.x = static_cast<float>(p1.x);
                                out1.y = static_cast<float>(p1.y);
                                out1.z = static_cast<float>(p1.z);

                                TriVertex out2 = out0;
                                out2.x = static_cast<float>(p2.x);
                                out2.y = static_cast<float>(p2.y);
                                out2.z = static_cast<float>(p2.z);

                                outlineTriVertices_.push_back(out0);
                                outlineTriVertices_.push_back(out1);
                                outlineTriVertices_.push_back(out2);
                            });
                        }
                    }

                    if (outlineTriVertices_.size() > candidateFirstTri)
                    {
                        outlinedThing_ = outlineCandidate;
                        if (haveOutlineBounds)
                        {
                            outlinedThingCenter_ = QVector3D(
                                static_cast<float>((outlineMin.x + outlineMax.x) * 0.5),
                                static_cast<float>((outlineMin.y + outlineMax.y) * 0.5),
                                static_cast<float>((outlineMin.z + outlineMax.z) * 0.5));
                        }
                        else
                        {
                            outlinedThingCenter_ =
                                QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));
                        }
                    }
                }
                else
                {
                    const TplValue* spriteValue = sed::level::thingFindValue(*level_, templates_, thing, "sprite");
                    const std::string spriteName =
                        spriteValue ? (spriteValue->stringValue.empty() ? spriteValue->asString() : spriteValue->stringValue) : std::string{};

                    if (!spriteName.empty())
                    {
                        if (const assets::SprFile* spr = loadSprite(spriteName))
                        {
                            const int spriteType = originalSpriteRenderType(level_->kind, spr->type);
                            const bool type2Supported = (spriteType != 2) || (level_->kind == ProjectType::IJIM);
                            if (type2Supported)
                            {
                                outlinedThingDoubleSided_ = true;
                                bool outlinedFromRenderedSprite = false;
                                if (const std::size_t thingIdx = static_cast<std::size_t>(outlineCandidate);
                                    thingIdx < thingPickIds_.size())
                                {
                                    const std::uint32_t pickId = thingPickIds_[thingIdx];
                                    if (pickId != 0 && spriteTriVertices_.size() >= 3)
                                    {
                                        const auto rangesIt = spriteTriRangesByPickId_.find(
                                            static_cast<int>(pickId));
                                        if (rangesIt != spriteTriRangesByPickId_.end())
                                        {
                                            for (const DrawRange& range : rangesIt->second)
                                            {
                                                const int end = std::min(
                                                    range.first + range.count,
                                                    static_cast<int>(spriteTriVertices_.size()));
                                                for (int i = range.first; i + 2 < end; i += 3)
                                                {
                                                    const TriVertex& sv0 = spriteTriVertices_[static_cast<std::size_t>(i)];
                                                    const TriVertex& sv1 = spriteTriVertices_[static_cast<std::size_t>(i + 1)];
                                                    const TriVertex& sv2 = spriteTriVertices_[static_cast<std::size_t>(i + 2)];

                                                    expandOutlineBounds(TVector{ static_cast<double>(sv0.x), static_cast<double>(sv0.y), static_cast<double>(sv0.z) });
                                                    expandOutlineBounds(TVector{ static_cast<double>(sv1.x), static_cast<double>(sv1.y), static_cast<double>(sv1.z) });
                                                    expandOutlineBounds(TVector{ static_cast<double>(sv2.x), static_cast<double>(sv2.y), static_cast<double>(sv2.z) });

                                                    TriVertex out0 = sv0;
                                                    out0.r = 1.0f;
                                                    out0.g = 1.0f;
                                                    out0.b = 1.0f;
                                                    out0.a = 1.0f;
                                                    out0.u = 0.0f;
                                                    out0.v = 0.0f;
                                                    out0.pickGeo = 0.0f;

                                                    TriVertex out1 = sv1;
                                                    out1.r = 1.0f;
                                                    out1.g = 1.0f;
                                                    out1.b = 1.0f;
                                                    out1.a = 1.0f;
                                                    out1.u = 0.0f;
                                                    out1.v = 0.0f;
                                                    out1.pickGeo = 0.0f;

                                                    TriVertex out2 = sv2;
                                                    out2.r = 1.0f;
                                                    out2.g = 1.0f;
                                                    out2.b = 1.0f;
                                                    out2.a = 1.0f;
                                                    out2.u = 0.0f;
                                                    out2.v = 0.0f;
                                                    out2.pickGeo = 0.0f;

                                                    outlineTriVertices_.push_back(out0);
                                                    outlineTriVertices_.push_back(out1);
                                                    outlineTriVertices_.push_back(out2);
                                                }
                                            }
                                        }
                                    }
                                }

                                if (outlineTriVertices_.size() > candidateFirstTri)
                                {
                                    outlinedThing_ = outlineCandidate;
                                    if (haveOutlineBounds)
                                    {
                                        outlinedThingCenter_ = QVector3D(
                                            static_cast<float>((outlineMin.x + outlineMax.x) * 0.5),
                                            static_cast<float>((outlineMin.y + outlineMax.y) * 0.5),
                                            static_cast<float>((outlineMin.z + outlineMax.z) * 0.5));
                                    }
                                    else
                                    {
                                        outlinedThingCenter_ =
                                            QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));
                                    }
                                    outlinedFromRenderedSprite = true;
                                    outlinedThingDoubleSided_ = true;
                                }

                                if (!outlinedFromRenderedSprite)
                                {
                                const auto pushOutlineTri = [&](const QVector3D& a, const QVector3D& b, const QVector3D& c) -> void
                                {
                                    const TVector pa{ static_cast<double>(a.x()), static_cast<double>(a.y()), static_cast<double>(a.z()) };
                                    const TVector pb{ static_cast<double>(b.x()), static_cast<double>(b.y()), static_cast<double>(b.z()) };
                                    const TVector pc{ static_cast<double>(c.x()), static_cast<double>(c.y()), static_cast<double>(c.z()) };
                                    expandOutlineBounds(pa);
                                    expandOutlineBounds(pb);
                                    expandOutlineBounds(pc);

                                    TriVertex out0{};
                                    out0.x = a.x();
                                    out0.y = a.y();
                                    out0.z = a.z();
                                    out0.r = 1.0f;
                                    out0.g = 1.0f;
                                    out0.b = 1.0f;
                                    out0.a = 1.0f;

                                    TriVertex out1 = out0;
                                    out1.x = b.x();
                                    out1.y = b.y();
                                    out1.z = b.z();

                                    TriVertex out2 = out0;
                                    out2.x = c.x();
                                    out2.y = c.y();
                                    out2.z = c.z();

                                    outlineTriVertices_.push_back(out0);
                                    outlineTriVertices_.push_back(out1);
                                    outlineTriVertices_.push_back(out2);
                                };

                                TVector thingPyr{};
                                thingPyr.pitch = thing.pch;
                                thingPyr.yaw = thing.yaw;
                                thingPyr.roll = thing.rol;

                                const float rollDeg = normalizeRollDeg(static_cast<float>(thing.rol));
                                const float widthHalf = spr->width * 0.5f;
                                const float heightHalf = spr->height * 0.5f;

                                const QVector3D center(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));
                                QVector3D p0{};
                                QVector3D p1{};
                                QVector3D p2{};
                                QVector3D p3{};

                                if (spriteType == 0)
                                {
                                    const bool billboard =
                                        render::fast_update::usesOriginalSpriteCameraFacingFrame(
                                            spriteType,
                                            renderPipeline_ == RenderPipeline::GamePreview);
                                    TVector placementNoRoll = thingPyr;
                                    placementNoRoll.roll = 0.0;
                                    const SpriteBasis displayBasis = billboard
                                        ? makeCameraFacingSpriteBasis(camera_.orbit)
                                        : makeSpriteBasis(placementNoRoll);
                                    QVector3D right = displayBasis.right;
                                    QVector3D up = displayBasis.up;
                                    QVector3D forward = displayBasis.forward;

                                    if (rollDeg != 0.0f)
                                    {
                                        const float rollRad = DegToRad(rollDeg);
                                        right = rotateAroundAxis(right, forward, rollRad);
                                        up = rotateAroundAxis(up, forward, rollRad);
                                    }

                                    const QVector3D spriteCenter =
                                        center
                                        + right * spr->offsetXYZ[0]
                                        + forward * spr->offsetXYZ[1]
                                        + up * spr->offsetXYZ[2];

                                    const QVector3D r = right * widthHalf;
                                    const QVector3D u = up * heightHalf;
                                    p0 = spriteCenter - r - u;
                                    p1 = spriteCenter + r - u;
                                    p2 = spriteCenter + r + u;
                                    p3 = spriteCenter - r + u;
                                }
                                else
                                {
                                    QVector3D yVec = makeSpriteType2Forward(thingPyr);
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
                                        xVec = QVector3D(1.0f, 0.0f, 0.0f);
                                    }

                                    QVector3D zVec = QVector3D::crossProduct(xVec, yVec);
                                    if (!normalizeInPlace(zVec))
                                    {
                                        zVec = QVector3D(0.0f, 0.0f, 1.0f);
                                    }

                                    const QVector3D rightHalf = xVec * widthHalf;
                                    const QVector3D upHalf = zVec * heightHalf;
                                    const QVector3D leftHalf = -rightHalf;

                                    QVector3D v0 = leftHalf - upHalf;
                                    QVector3D v1 = rightHalf - upHalf;
                                    QVector3D v2 = rightHalf + upHalf;
                                    QVector3D v3 = leftHalf + upHalf;

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

                                pushOutlineTri(p0, p1, p2);
                                pushOutlineTri(p0, p2, p3);

                                if (outlineTriVertices_.size() > candidateFirstTri)
                                {
                                    outlinedThing_ = outlineCandidate;
                                    if (haveOutlineBounds)
                                    {
                                        outlinedThingCenter_ = QVector3D(
                                            static_cast<float>((outlineMin.x + outlineMax.x) * 0.5),
                                            static_cast<float>((outlineMin.y + outlineMax.y) * 0.5),
                                            static_cast<float>((outlineMin.z + outlineMax.z) * 0.5));
                                    }
                                    else
                                    {
                                        outlinedThingCenter_ =
                                            QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));
                                    }
                                }
                                }
                            }
                        }
                    }
                }

                // Cube outline fallback for things with no model and no sprite.
                if (outlineTriVertices_.size() == candidateFirstTri)
                {
                    TVector thingPyr{};
                    thingPyr.pitch = thing.pch;
                    thingPyr.yaw = thing.yaw;
                    thingPyr.roll = thing.rol;
                    const TMatrix43 cubeXf = TMatrix43::Create(TVector{ thing.x, thing.y, thing.z }, thingPyr);

                    const std::array<render::ThingCubeFaceVertex, 36> cubeVertices =
                        render::makeThingCubeFaceVertices(cubeXf, static_cast<double>(thingCubeHalfSize_));
                    for (const render::ThingCubeFaceVertex& cubeVertex : cubeVertices)
                    {
                        expandOutlineBounds(cubeVertex.position);
                        TriVertex out{};
                        out.x = static_cast<float>(cubeVertex.position.x);
                        out.y = static_cast<float>(cubeVertex.position.y);
                        out.z = static_cast<float>(cubeVertex.position.z);
                        out.u = 0.0f;
                        out.v = 0.0f;
                        out.r = 1.0f;
                        out.g = 1.0f;
                        out.b = 1.0f;
                        out.a = 1.0f;
                        out.pickId = outlinePickId;
                        out.pickGeo = 4.0f;
                        out.sky = 0.0f;
                        outlineTriVertices_.push_back(out);
                    }

                    outlinedThing_ = outlineCandidate;
                    if (haveOutlineBounds)
                    {
                        outlinedThingCenter_ = QVector3D(
                            static_cast<float>((outlineMin.x + outlineMax.x) * 0.5),
                            static_cast<float>((outlineMin.y + outlineMax.y) * 0.5),
                            static_cast<float>((outlineMin.z + outlineMax.z) * 0.5));
                    }
                    else
                    {
                        outlinedThingCenter_ =
                            QVector3D(static_cast<float>(thing.x), static_cast<float>(thing.y), static_cast<float>(thing.z));
                    }
                }
                } // if (!outlineBuilt)
            }
        }

        // Step 9c: split outline triangles so multi-selected things and the active thing can use different colors.
        if (activeOutlineFirstTri.has_value())
        {
            outlineTriMultiVertexCount_ = static_cast<int>(std::min(*activeOutlineFirstTri, outlineTriVertices_.size()));
        }
        else
        {
            outlineTriMultiVertexCount_ = static_cast<int>(outlineTriVertices_.size());
        }
    }

    // Build contiguous ranges for selected level triangles in the composed
    // scene VBO.  The regular highlight line list deliberately remains the
    // authoritative full polygon wireframe; these ranges only provide the
    // stencil mask used for its outer halo.  Scan once when selection/mesh
    // state changes instead of rebuilding or uploading a duplicate mesh every
    // frame.
    if (!selectedGeometryPickIds.empty() && levelTriVertexCount_ >= 3)
    {
        const std::size_t boundedTriVertexCount = std::min(
            triVertices_.size(),
            static_cast<std::size_t>(std::numeric_limits<int>::max()));
        const int levelVertexEnd = std::min(
            levelTriVertexCount_,
            static_cast<int>(boundedTriVertexCount));
        render::appendSelectedTriangleRanges(
            geometrySelectionStencilRanges_,
            levelVertexEnd,
            selectedGeometryPickIds,
            [this](const int vertexIndex) {
                return triVertices_[static_cast<std::size_t>(vertexIndex)].pickId;
        });
    }

    // ARCHLIGHTING vertices are an editor-only sparse overlay. They use a
    // dedicated position/color VBO so channel colors do not require one
    // uniform update and draw call per model vertex.
    rebuildArchitecturalLightingPointVertices();

    // Step 10: publish final vertex counts for the upload/draw path.
    highlightLineVertexCount_ = static_cast<int>(highlightLineVertices_.size());
    highlightPointVertexCount_ = static_cast<int>(highlightPointPositions_.size() / 3);
    outlineTriVertexCount_ = static_cast<int>(outlineTriVertices_.size());
}

void GlView::rebuildArchitecturalLightingPointVertices()
{
    architecturalLightingFirstPoint_ = 0;
    architecturalLightingPointCount_ = 0;
    architecturalLightingSelectedFirstPoint_ = 0;
    architecturalLightingSelectedPointCount_ = 0;
    architecturalLightingPointVertices_.clear();
    architecturalLightingPointIdentities_.clear();

    if (!architecturalLightingEditingActive_
        || architecturalLightingSelectedOwnerCaches_.empty())
    {
        return;
    }

    const auto previewValueFor = [this](
        const ArchitecturalLightingRenderVertex& identity)
        -> const MotsArchLightVertex*
    {
        const auto cacheIndex =
            architecturalLightingSelectedOwnerCacheByThing_.find(
                identity.thingIndex);
        const auto revision =
            architecturalLightingPreviewOwnerRevisions_.find(
                identity.thingIndex);
        if (cacheIndex == architecturalLightingSelectedOwnerCacheByThing_.end()
            || cacheIndex->second >= architecturalLightingSelectedOwnerCaches_.size()
            || revision == architecturalLightingPreviewOwnerRevisions_.end())
        {
            return nullptr;
        }
        const ArchitecturalLightingRenderCache& cache =
            architecturalLightingSelectedOwnerCaches_[cacheIndex->second];
        if (revision->second.modelIdentity != cache.modelIdentity
            || revision->second.archLightingRevision
                != cache.archLightingRevision)
        {
            return nullptr;
        }

        const auto it = architecturalLightingPreviewByIdentity_.find(
            architecturalLightingVertexIdentity(identity));
        return it == architecturalLightingPreviewByIdentity_.end()
            ? nullptr
            : &it->second;
    };

    const auto finiteChannel = [](const float value) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
    };
    const bool mono = renderPipeline_ == RenderPipeline::Editor
        && architecturalLightingAuthoringMode_
            == level::MotsArchLightingVertexMode::Mono;

    for (const ArchitecturalLightingRenderCache& ownerCache
         : architecturalLightingSelectedOwnerCaches_)
    {
        for (const ArchitecturalLightingRenderVertex& identity
             : ownerCache.vertices)
        {
            const QVector3D& point = identity.worldPosition;
            if (!std::isfinite(point.x()) || !std::isfinite(point.y())
                || !std::isfinite(point.z()))
            {
                continue;
            }

            const MotsArchLightVertex* const preview = previewValueFor(identity);
            const MotsArchLightVertex& value = preview ? *preview : identity.lighting;
            const float red = mono ? value.mono : value.red;
            const float green = mono ? value.mono : value.green;
            const float blue = mono ? value.mono : value.blue;
            architecturalLightingPointVertices_.push_back(
                ArchitecturalLightingPointVertex{
                    point.x(),
                    point.y(),
                    point.z(),
                    finiteChannel(red),
                    finiteChannel(green),
                    finiteChannel(blue) });
            architecturalLightingPointIdentities_.push_back(identity);
        }
    }

    // Keep selected points contiguous at the end of the same VBO. The draw
    // path can add one batched ring pass for the complete selection without
    // one GL draw call per selected authored vertex.
    const std::size_t basePointCount = architecturalLightingPointIdentities_.size();
    architecturalLightingSelectedFirstPoint_ = static_cast<int>(std::min(
        basePointCount,
        static_cast<std::size_t>(std::numeric_limits<int>::max())));
    for (std::size_t index = 0; index < basePointCount; ++index)
    {
        const ArchitecturalLightingVertexIdentity identity =
            architecturalLightingVertexIdentity(
                architecturalLightingPointIdentities_[index]);
        if (!architecturalLightingSelectedVertexSet_.contains(identity))
        {
            continue;
        }
        architecturalLightingPointVertices_.push_back(
            architecturalLightingPointVertices_[index]);
        architecturalLightingPointIdentities_.push_back(
            architecturalLightingPointIdentities_[index]);
    }
    architecturalLightingSelectedPointCount_ = static_cast<int>(
        std::min(
            architecturalLightingPointIdentities_.size() - basePointCount,
            static_cast<std::size_t>(std::numeric_limits<int>::max())));

    architecturalLightingPointCount_ = static_cast<int>(
        std::min<std::size_t>(
            architecturalLightingPointVertices_.size(),
            static_cast<std::size_t>(std::numeric_limits<int>::max())));
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
std::optional<GlView::SelectionWirePlaneSupportForTest>
GlView::selectionWirePlaneSupportForTest(
    const QVector3D& start,
    const QVector3D& end) const noexcept
{
    const auto matches = [](const LineVertex& vertex, const QVector3D& point)
    {
        return vertex.x == point.x()
            && vertex.y == point.y()
            && vertex.z == point.z();
    };

    const auto dumpMatches = [&matches, &start, &end](
        const char* label, const std::vector<LineVertex>& vertices)
    {
        bool found = false;
        for (std::size_t index = 0; index + 1u < vertices.size(); index += 2u)
        {
            const LineVertex& first = vertices[index];
            const LineVertex& second = vertices[index + 1u];
            if (!((matches(first, start) && matches(second, end))
                  || (matches(first, end) && matches(second, start))))
            {
                continue;
            }
            found = true;
            std::cerr << "[wireframe support scan] " << label
                      << " pair=" << index << '/' << index + 1u
                      << " planeValid=" << first.planeValid << '/'
                      << second.planeValid
                      << " reference=(" << first.planeX << ',' << first.planeY
                      << ',' << first.planeZ << ")/(" << second.planeX << ','
                      << second.planeY << ',' << second.planeZ << ")\n";
        }
        if (!found)
        {
            std::cerr << "[wireframe support scan] " << label
                      << " segment=missing\n";
        }
    };
    dumpMatches("base", lineVertices_);
    dumpMatches("selection", highlightLineVertices_);
    std::cerr << "[wireframe support scan] multiVertexCount="
              << highlightLineMultiVertexCount_ << '\n';

    for (std::size_t index = 0; index + 1u < highlightLineVertices_.size(); index += 2u)
    {
        const LineVertex& first = highlightLineVertices_[index];
        const LineVertex& second = highlightLineVertices_[index + 1u];
        if (!((matches(first, start) && matches(second, end))
              || (matches(first, end) && matches(second, start))))
        {
            continue;
        }
        return SelectionWirePlaneSupportForTest{
            first.planeValid > 0.5f && second.planeValid > 0.5f,
            QVector3D(first.planeX, first.planeY, first.planeZ),
        };
    }
    return std::nullopt;
}

std::optional<std::array<float, 3>>
GlView::architecturalLightingPointColorForTest(
    const ArchitecturalLightingVertexIdentity& identity) const noexcept
{
    for (std::size_t index = 0;
         index < architecturalLightingPointIdentities_.size()
             && index < architecturalLightingPointVertices_.size();
         ++index)
    {
        const ArchitecturalLightingRenderVertex& candidate =
            architecturalLightingPointIdentities_[index];
        if (candidate.thingIndex == identity.thingIndex
            && candidate.lodIndex == identity.lodIndex
            && candidate.meshIndex == identity.meshIndex
            && candidate.vertexIndex == identity.vertexIndex)
        {
            const ArchitecturalLightingPointVertex& point =
                architecturalLightingPointVertices_[index];
            return std::array<float, 3>{ point.r, point.g, point.b };
        }
    }
    return std::nullopt;
}
#endif

void GlView::restoreArchitecturalLightingTrianglePreview()
{
    const auto markDirty = [this](const std::int32_t globalIndex)
    {
        if (architecturalLightingTriangleDirtyIndexSet_.insert(globalIndex).second)
        {
            architecturalLightingTriangleDirtyIndices_.push_back(globalIndex);
        }
    };

    for (const ArchitecturalLightingTrianglePatch& patch
         : architecturalLightingTrianglePatches_)
    {
        if (patch.globalVertexIndex < 0
            || static_cast<std::size_t>(patch.globalVertexIndex) >= triVertices_.size())
        {
            continue;
        }

        TriVertex& vertex = triVertices_[static_cast<std::size_t>(patch.globalVertexIndex)];
        vertex.r = patch.originalColor.r;
        vertex.g = patch.originalColor.g;
        vertex.b = patch.originalColor.b;
        markDirty(patch.globalVertexIndex);
    }
    architecturalLightingTrianglePatches_.clear();
    architecturalLightingTrianglePatchIndexSet_.clear();
}

void GlView::applyArchitecturalLightingTrianglePreview()
{
    restoreArchitecturalLightingTrianglePreview();

    if (renderPipeline_ != RenderPipeline::Editor)
    {
        return;
    }
    const bool monoAuthoringPreview =
        architecturalLightingEditingActive_
        && architecturalLightingAuthoringMode_
            == level::MotsArchLightingVertexMode::Mono;
    if (!architecturalLightingEditingActive_
        || (!monoAuthoringPreview && architecturalLightingPreview_.empty())
        || architecturalLightingSelectedOwnerCaches_.empty()
        || !level_)
    {
        return;
    }
    const level::MotsArchLightingVertexMode displayMode =
        monoAuthoringPreview
        ? level::MotsArchLightingVertexMode::Mono
        : architecturalLightingPreviewMode_;

    const TLightMode maxLightMode =
        static_cast<TLightMode>(static_cast<std::int32_t>(lightingMode_));
    const auto markDirty = [this](const std::int32_t globalIndex)
    {
        if (architecturalLightingTriangleDirtyIndexSet_.insert(globalIndex).second)
        {
            architecturalLightingTriangleDirtyIndices_.push_back(globalIndex);
        }
    };

    const auto invalidatePreview = [this]()
    {
        // A later invalid identity must not leave earlier entries in the
        // composed triangle VBO. Restore any patches before dropping the
        // staging records, including publication/Undo invalidation during a
        // multi-owner edit.
        restoreArchitecturalLightingTrianglePreview();
        architecturalLightingPreview_.clear();
        architecturalLightingPreviewByIdentity_.clear();
        architecturalLightingPreviewOwnerRevisions_.clear();
        architecturalLightingPreviewMode_ = level::MotsArchLightingVertexMode::Rgb;
        architecturalLightingPreviewModelIdentity_ = nullptr;
        architecturalLightingPreviewModelRevision_ = 0;
    };

    std::unordered_set<ArchitecturalLightingVertexIdentity,
        ArchitecturalLightingVertexIdentityHash> validIdentities{};
    std::unordered_map<ArchitecturalLightingVertexIdentity,
        const ArchitecturalLightingRenderVertex*,
        ArchitecturalLightingVertexIdentityHash> cachedVertices{};
    std::size_t cachedVertexCount = 0;
    for (const ArchitecturalLightingRenderCache& cache
         : architecturalLightingSelectedOwnerCaches_)
    {
        cachedVertexCount += cache.vertices.size();
    }
    validIdentities.reserve(cachedVertexCount);
    cachedVertices.reserve(cachedVertexCount);
    for (const ArchitecturalLightingRenderCache& cache
         : architecturalLightingSelectedOwnerCaches_)
    {
        for (const ArchitecturalLightingRenderVertex& vertex : cache.vertices)
        {
            const ArchitecturalLightingVertexIdentity identity =
                architecturalLightingVertexIdentity(vertex);
            validIdentities.insert(identity);
            cachedVertices.insert_or_assign(identity, &vertex);
        }
    }

    std::unordered_set<std::int32_t> previewOwners{};
    previewOwners.reserve(architecturalLightingPreview_.size());
    for (const ArchitecturalLightingPreviewVertex& preview
         : architecturalLightingPreview_)
    {
        const ArchitecturalLightingRenderVertex& identity = preview.identity;
        if (identity.thingIndex < 0
            || identity.lodIndex != 0
            || identity.meshIndex < 0
            || identity.vertexIndex < 0
            || static_cast<std::size_t>(identity.thingIndex)
                >= level_->things.size()
            || !validIdentities.contains(
                architecturalLightingVertexIdentity(identity)))
        {
            invalidatePreview();
            return;
        }

        const auto cacheIndex =
            architecturalLightingSelectedOwnerCacheByThing_.find(
                identity.thingIndex);
        const auto revision =
            architecturalLightingPreviewOwnerRevisions_.find(
                identity.thingIndex);
        if (cacheIndex == architecturalLightingSelectedOwnerCacheByThing_.end()
            || cacheIndex->second >= architecturalLightingSelectedOwnerCaches_.size()
            || revision == architecturalLightingPreviewOwnerRevisions_.end())
        {
            invalidatePreview();
            return;
        }
        const ArchitecturalLightingRenderCache& cache =
            architecturalLightingSelectedOwnerCaches_[cacheIndex->second];
        const std::size_t thingIndex = static_cast<std::size_t>(identity.thingIndex);
        if (!cache.model
            || cache.modelIdentity != cache.model.get()
            || revision->second.modelIdentity != cache.modelIdentity
            || revision->second.archLightingRevision
                != cache.archLightingRevision
            || level_->things[thingIndex].archLightingIndex
                != cache.archLightingIndex
            || thingGeometrySignature(level_->things[thingIndex])
                != cache.thingGeometryRevision)
        {
            invalidatePreview();
            return;
        }
        previewOwners.insert(identity.thingIndex);
    }

    if (monoAuthoringPreview)
    {
        for (const ArchitecturalLightingRenderCache& cache
             : architecturalLightingSelectedOwnerCaches_)
        {
            if (cache.thingIndex < 0
                || static_cast<std::size_t>(cache.thingIndex)
                    >= level_->things.size()
                || !cache.model
                || cache.modelIdentity != cache.model.get())
            {
                invalidatePreview();
                return;
            }
            const Thing& thing = level_->things[
                static_cast<std::size_t>(cache.thingIndex)];
            if (thing.archLightingIndex != cache.archLightingIndex
                || thingGeometrySignature(thing)
                    != cache.thingGeometryRevision)
            {
                invalidatePreview();
                return;
            }
            previewOwners.insert(cache.thingIndex);
        }
    }

    if (thingTriBuildGlobalFirstVertex_.size()
        != thingMeshCache_.triBuilds.size())
    {
        // A rebuild may temporarily have no published triangle ranges (for
        // example, a vertex-only model). Keep the staged authoring data and
        // point preview alive; a later mesh publication can render it.
        return;
    }

    // Validate every owner's composed range before changing triangle colors.
    // Missing/hidden/non-renderable geometry is not a stale ARCH identity:
    // keep its staged values for point markers and eventual commit. A malformed
    // range only disables triangle preview for that owner.
    std::unordered_set<std::int32_t> renderablePreviewOwners{};
    renderablePreviewOwners.reserve(previewOwners.size());
    for (const std::int32_t owner : previewOwners)
    {
        if (owner < 0 || static_cast<std::size_t>(owner) >= thingPickIds_.size()
            || thingPickIds_[static_cast<std::size_t>(owner)] == 0u)
        {
            continue;
        }
        const int thingPickId = static_cast<int>(
            thingPickIds_[static_cast<std::size_t>(owner)]);
        const auto rangesIt = thingTriBuildRangesByPickId_.find(thingPickId);
        if (rangesIt == thingTriBuildRangesByPickId_.end())
        {
            continue;
        }
        bool rangesValid = true;
        for (const TriBuildRange& range : rangesIt->second)
        {
            if (range.buildIndex >= thingMeshCache_.triBuilds.size()
                || range.first < 0 || range.count <= 0
                || static_cast<std::size_t>(range.first)
                       + static_cast<std::size_t>(range.count)
                    > thingMeshCache_.triBuilds[range.buildIndex].vertices.size()
                || range.buildIndex >= thingTriBuildGlobalFirstVertex_.size())
            {
                rangesValid = false;
                break;
            }
            const TriBuild& build = thingMeshCache_.triBuilds[range.buildIndex];
            if (build.shadeInputs.size() != build.vertices.size())
            {
                rangesValid = false;
                break;
            }
        }
        if (rangesValid)
        {
            renderablePreviewOwners.insert(owner);
        }
    }

    // Walk each renderable owner's triangle ranges once and perform O(1)
    // identity lookups. This avoids scanning every range once per selected vertex.
    for (const std::int32_t owner : renderablePreviewOwners)
    {
        const int thingPickId = static_cast<int>(
            thingPickIds_[static_cast<std::size_t>(owner)]);
        const auto rangesIt = thingTriBuildRangesByPickId_.find(thingPickId);
        for (const TriBuildRange& range : rangesIt->second)
        {
            const TriBuild& build = thingMeshCache_.triBuilds[range.buildIndex];
            const int globalBase = thingTriBuildGlobalFirstVertex_[range.buildIndex];
            for (int offset = 0; offset < range.count; ++offset)
            {
                const int localIndex = range.first + offset;
                const TriShadeInput& shade =
                    build.shadeInputs[static_cast<std::size_t>(localIndex)];
                const ArchitecturalLightingVertexIdentity identity{
                    owner,
                    0,
                    shade.archMeshIndex,
                    shade.archVertexIndex};
                const auto previewIt =
                    architecturalLightingPreviewByIdentity_.find(identity);
                if (previewIt == architecturalLightingPreviewByIdentity_.end()
                    && !monoAuthoringPreview)
                {
                    continue;
                }
                const auto cachedIt = cachedVertices.find(identity);
                if (cachedIt == cachedVertices.end() || !cachedIt->second)
                {
                    continue;
                }
                const MotsArchLightVertex& lighting =
                    previewIt != architecturalLightingPreviewByIdentity_.end()
                    ? previewIt->second
                    : cachedIt->second->lighting;

                const int globalIndex = globalBase + localIndex;
                if (globalIndex < 0
                    || static_cast<std::size_t>(globalIndex) >= triVertices_.size()
                    || !architecturalLightingTrianglePatchIndexSet_.insert(
                        globalIndex)
                        .second)
                {
                    continue;
                }

                TriVertex& vertex = triVertices_[static_cast<std::size_t>(globalIndex)];
                architecturalLightingTrianglePatches_.push_back(
                    ArchitecturalLightingTrianglePatch{
                        globalIndex,
                        ColorF{ vertex.r, vertex.g, vertex.b, vertex.a } });

                const ColorF previewBase = render::motsArchLightingVertexColor(
                    shade.modelColor,
                    lighting,
                    displayMode);
                const ColorF lit = render::polyCalcLitColor(
                    previewBase,
                    shade.lightMode,
                    shade.ambientLight,
                    shade.extraLight,
                    level_->kind,
                    maxLightMode,
                    vertex.dynamicLightTarget > 1.5f);
                vertex.r = lit.r;
                vertex.g = lit.g;
                vertex.b = lit.b;
                markDirty(globalIndex);
            }
        }
    }
}

void GlView::uploadArchitecturalLightingTrianglePreview()
{
    if (!glReady_ || !triVbo_ || architecturalLightingTriangleDirtyIndices_.empty())
    {
        return;
    }

    triVbo_->bind();
    for (const std::int32_t globalIndex
         : architecturalLightingTriangleDirtyIndices_)
    {
        if (globalIndex < 0
            || static_cast<std::size_t>(globalIndex) >= triVertices_.size())
        {
            continue;
        }

        const int offsetBytes = globalIndex * static_cast<int>(sizeof(TriVertex));
        const int bytes = static_cast<int>(sizeof(TriVertex));
        if (offsetBytes < 0
            || offsetBytes + bytes > triVboAllocatedBytes_)
        {
            continue;
        }
        triVbo_->write(
            offsetBytes,
            triVertices_.data() + globalIndex,
            bytes);
    }
    triVbo_->release();
    architecturalLightingTriangleDirtyIndices_.clear();
    architecturalLightingTriangleDirtyIndexSet_.clear();
}

} // namespace sed
