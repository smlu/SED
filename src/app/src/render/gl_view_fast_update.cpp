#include "gl_view_common.hpp"
#include "render/lighting/dynamic_light_utils.hpp"
#include "render/overlays/thing_shape_overlay_utils.hpp"
#include "render/upload/buffer_upload_utils.hpp"

#include <numeric>

namespace sed
{

void GlView::notifySurfaceUvsChanged(const std::vector<SurfaceRef>& surfaceRefs)
{
    if (!level_ || surfaceRefs.empty())
    {
        return;
    }

    if (!levelMeshCache_.valid)
    {
        notifyLevelGeometryChanged();
        return;
    }

    const auto surfaceKey = [](const std::int32_t sector, const std::int32_t surface) noexcept
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(sector)) << 32u)
            | static_cast<std::uint32_t>(surface);
    };

    std::unordered_set<std::uint64_t> targets{};
    targets.reserve(surfaceRefs.size());
    for (const SurfaceRef ref : surfaceRefs)
    {
        if (ref.sector < 0 || ref.surface < 0
            || static_cast<std::size_t>(ref.sector) >= level_->sectors.size()
            || static_cast<std::size_t>(ref.surface)
                >= level_->sectors[static_cast<std::size_t>(ref.sector)].surfaces.size())
        {
            continue;
        }
        targets.insert(surfaceKey(ref.sector, ref.surface));
    }
    if (targets.empty())
    {
        return;
    }

    std::vector<DrawRange> dirtyRanges{};
    bool mappingValid = true;
    for (const std::uint64_t key : targets)
    {
        const auto rangesIt = surfaceUvRanges_.find(key);
        if (rangesIt == surfaceUvRanges_.end())
        {
            // A selected surface can intentionally have no solid triangle
            // range (for example Geometry Mode None). Its document UVs still
            // update, but there is nothing to publish to this VBO.
            continue;
        }
        for (const SurfaceUvRange& range : rangesIt->second)
        {
            if (range.buildIndex >= levelMeshCache_.triBuilds.size()
                || range.localFirst < 0 || range.globalFirst < 0 || range.count <= 0)
            {
                mappingValid = false;
                break;
            }
            TriBuild& build = levelMeshCache_.triBuilds[range.buildIndex];
            if (build.shadeInputs.size() != build.vertices.size()
                || range.localFirst + range.count > static_cast<int>(build.vertices.size())
                || range.globalFirst + range.count > levelTriVertexCount_
                || range.globalFirst + range.count > static_cast<int>(triVertices_.size()))
            {
                mappingValid = false;
                break;
            }
            for (int offset = 0; offset < range.count; ++offset)
            {
                const int localIndex = range.localFirst + offset;
                const int globalIndex = range.globalFirst + offset;
                const TriShadeInput& source = build.shadeInputs[static_cast<std::size_t>(localIndex)];
                if (source.sourceSector < 0 || source.sourceSurface < 0
                    || source.sourceTexVertex < 0
                    || surfaceKey(source.sourceSector, source.sourceSurface) != key
                    || static_cast<std::size_t>(source.sourceSector) >= level_->sectors.size())
                {
                    mappingValid = false;
                    break;
                }
                const Sector& sector = level_->sectors[static_cast<std::size_t>(source.sourceSector)];
                if (source.sourceSurface < 0
                    || static_cast<std::size_t>(source.sourceSurface) >= sector.surfaces.size())
                {
                    mappingValid = false;
                    break;
                }
                const Surface& surface = sector.surfaces[static_cast<std::size_t>(source.sourceSurface)];
                if (static_cast<std::size_t>(source.sourceTexVertex) >= surface.texVertices.size())
                {
                    mappingValid = false;
                    break;
                }
                const TexVertex& uv = surface.texVertices[static_cast<std::size_t>(source.sourceTexVertex)];
                build.vertices[static_cast<std::size_t>(localIndex)].u = uv.u;
                build.vertices[static_cast<std::size_t>(localIndex)].v = uv.v;
                triVertices_[static_cast<std::size_t>(globalIndex)].u = uv.u;
                triVertices_[static_cast<std::size_t>(globalIndex)].v = uv.v;
            }
            dirtyRanges.push_back(DrawRange{range.globalFirst, range.count});
            if (!mappingValid)
            {
                break;
            }
        }
        if (!mappingValid)
        {
            break;
        }
    }

    if (!mappingValid)
    {
        notifyLevelGeometryChanged();
        return;
    }

    render::mergeDrawRanges(dirtyRanges);
    bool uploadFailed = false;
    if (glReady_ && !dirtyRanges.empty())
    {
        makeCurrent();
        if (!triVbo_ || triVboAllocatedBytes_ <= 0
            || triVertexCount_ != static_cast<int>(triVertices_.size()))
        {
            uploadFailed = true;
        }
        else
        {
            triVbo_->bind();
            for (const DrawRange& range : dirtyRanges)
            {
                const int offsetBytes = range.first * static_cast<int>(sizeof(TriVertex));
                const int byteCount = range.count * static_cast<int>(sizeof(TriVertex));
                if (range.first < 0 || range.count <= 0
                    || range.first + range.count > levelTriVertexCount_
                    || offsetBytes < 0
                    || offsetBytes + byteCount > triVboAllocatedBytes_)
                {
                    uploadFailed = true;
                    break;
                }
                triVbo_->write(
                    offsetBytes,
                    triVertices_.data() + range.first,
                    byteCount);
            }
            triVbo_->release();
        }
        doneCurrent();
    }

    if (uploadFailed)
    {
        notifyLevelGeometryChanged();
        return;
    }

    ++surfaceUvFastPublicationCount_;
    // UV-only publication is not a camera/view transform. Emitting
    // viewTransformChanged here would re-orient View Face, schedule recovery,
    // invalidate picking peel caches, and dirty camera-facing sprites on every
    // mouse sample.
    update();
}

bool GlView::publishTransformSurfaceUvPreview(
    const level::ConnectedSurfaceUvEvaluation& evaluation)
{
    if (!level_ || !evaluation.valid
        || level_->uvCoordinateSpace != evaluation.coordinateSpace)
    {
        return false;
    }
    if (!levelMeshCache_.valid)
    {
        return false;
    }

    const auto keyFor = [](const SurfaceRef ref) noexcept
    {
        return (static_cast<std::uint64_t>(
                    static_cast<std::uint32_t>(ref.sector))
                << 32u)
            | static_cast<std::uint32_t>(ref.surface);
    };
    std::unordered_map<std::uint64_t, const level::ConnectedSurfaceUvSurface*>
        updates{};
    updates.reserve(evaluation.surfaces.size());
    for (const level::ConnectedSurfaceUvSurface& update : evaluation.surfaces)
    {
        if (update.surface.sector < 0 || update.surface.surface < 0
            || static_cast<std::size_t>(update.surface.sector)
                >= level_->sectors.size())
        {
            return false;
        }
        const Sector& sector = level_->sectors[
            static_cast<std::size_t>(update.surface.sector)];
        if (static_cast<std::size_t>(update.surface.surface)
                >= sector.surfaces.size()
            || update.texVertices.size()
                != sector.surfaces[static_cast<std::size_t>(
                    update.surface.surface)].texVertices.size()
            || update.vertices
                != sector.surfaces[static_cast<std::size_t>(
                    update.surface.surface)].vertices
            || !updates.emplace(keyFor(update.surface), &update).second)
        {
            return false;
        }
    }

    std::vector<DrawRange> dirtyRanges{};
    // Validate the complete source-to-VBO map before changing any transient
    // CPU cache. A rejected preview leaves authored and rendered state exact.
    for (const auto& [key, update] : updates)
    {
        const auto rangesIt = surfaceUvRanges_.find(key);
        if (rangesIt == surfaceUvRanges_.end())
        {
            continue;
        }
        for (const SurfaceUvRange& range : rangesIt->second)
        {
            if (range.buildIndex >= levelMeshCache_.triBuilds.size()
                || range.localFirst < 0 || range.globalFirst < 0
                || range.count <= 0)
            {
                return false;
            }
            TriBuild& build = levelMeshCache_.triBuilds[range.buildIndex];
            if (build.shadeInputs.size() != build.vertices.size()
                || range.localFirst + range.count
                    > static_cast<int>(build.vertices.size())
                || range.globalFirst + range.count > levelTriVertexCount_
                || range.globalFirst + range.count
                    > static_cast<int>(triVertices_.size()))
            {
                return false;
            }
            for (int offset = 0; offset < range.count; ++offset)
            {
                const int localIndex = range.localFirst + offset;
                const int globalIndex = range.globalFirst + offset;
                const TriShadeInput& source = build.shadeInputs[
                    static_cast<std::size_t>(localIndex)];
                if (source.sourceSector < 0 || source.sourceSurface < 0
                    || keyFor(SurfaceRef{
                        source.sourceSector, source.sourceSurface }) != key
                    || source.sourceTexVertex < 0
                    || static_cast<std::size_t>(source.sourceTexVertex)
                        >= update->texVertices.size())
                {
                    return false;
                }
            }
            dirtyRanges.push_back(DrawRange{ range.globalFirst, range.count });
        }
    }

    render::mergeDrawRanges(dirtyRanges);
    if (glReady_ && !dirtyRanges.empty()
        && (!triVbo_ || triVboAllocatedBytes_ <= 0
            || triVertexCount_ != static_cast<int>(triVertices_.size())))
    {
        return false;
    }
    for (const DrawRange& range : dirtyRanges)
    {
        const int offsetBytes =
            range.first * static_cast<int>(sizeof(TriVertex));
        const int byteCount =
            range.count * static_cast<int>(sizeof(TriVertex));
        if (range.first < 0 || range.count <= 0
            || range.first + range.count > levelTriVertexCount_
            || offsetBytes < 0
            || (glReady_
                && offsetBytes + byteCount > triVboAllocatedBytes_))
        {
            return false;
        }
    }

    for (const auto& [key, update] : updates)
    {
        const auto rangesIt = surfaceUvRanges_.find(key);
        if (rangesIt == surfaceUvRanges_.end())
        {
            continue;
        }
        for (const SurfaceUvRange& range : rangesIt->second)
        {
            TriBuild& build = levelMeshCache_.triBuilds[range.buildIndex];
            for (int offset = 0; offset < range.count; ++offset)
            {
                const int localIndex = range.localFirst + offset;
                const int globalIndex = range.globalFirst + offset;
                const TriShadeInput& source = build.shadeInputs[
                    static_cast<std::size_t>(localIndex)];
                const TexVertex& uv = update->texVertices[
                    static_cast<std::size_t>(source.sourceTexVertex)];
                build.vertices[static_cast<std::size_t>(localIndex)].u = uv.u;
                build.vertices[static_cast<std::size_t>(localIndex)].v = uv.v;
                triVertices_[static_cast<std::size_t>(globalIndex)].u = uv.u;
                triVertices_[static_cast<std::size_t>(globalIndex)].v = uv.v;
            }
        }
    }

    if (glReady_ && !dirtyRanges.empty())
    {
        makeCurrent();
        triVbo_->bind();
        for (const DrawRange& range : dirtyRanges)
        {
            const int offsetBytes =
                range.first * static_cast<int>(sizeof(TriVertex));
            const int byteCount =
                range.count * static_cast<int>(sizeof(TriVertex));
            triVbo_->write(
                offsetBytes,
                triVertices_.data() + range.first,
                byteCount);
        }
        triVbo_->release();
        doneCurrent();
    }

    transformSurfaceUvPreviewSurfaces_.clear();
    transformSurfaceUvPreviewSurfaces_.reserve(evaluation.surfaces.size());
    for ( const level::ConnectedSurfaceUvSurface& update : evaluation.surfaces )
    {
        transformSurfaceUvPreviewSurfaces_.push_back(update.surface);
    }
    update();
    return true;
}

void GlView::refreshTransformSurfaceUvPreview()
{
    if ( !level_ || !preserveConnectedSurfaceUvsEnabled_ || !transformSurfaceUvPlan_ ||
         !transformSurfaceUvPlan_->valid )
    {
        return;
    }

    const TVector documentPivot = transformDrag_.documentPivot.value_or(
        TVector{transformDrag_.pivot.x(), transformDrag_.pivot.y(), transformDrag_.pivot.z()});
    level::ConnectedSurfaceUvTransform transform{};
    const TransformHandle handle = transformDrag_.handle;
    const bool move = handle == TransformHandle::MoveFree || handle == TransformHandle::MoveX ||
                      handle == TransformHandle::MoveY || handle == TransformHandle::MoveZ;
    const bool scale = handle == TransformHandle::ScaleUniform || handle == TransformHandle::ScaleX ||
                       handle == TransformHandle::ScaleY || handle == TransformHandle::ScaleZ;
    if ( move )
    {
        transform.translation = transformDrag_.documentTranslation.value_or(
            TVector{static_cast<double>(transformDrag_.translation.x()),
                    static_cast<double>(transformDrag_.translation.y()),
                    static_cast<double>(transformDrag_.translation.z())});
        if ( transformDrag_.snapRotateAxis.lengthSquared() > 1.0e-10f &&
             std::abs(transformDrag_.snapRotateAngleDeg) > 0.0f )
        {
            transform.kind = level::ConnectedSurfaceUvTransformKind::rotateThenTranslate;
            transform.rotationAxis = TVector{static_cast<double>(transformDrag_.snapRotateAxis.x()),
                                             static_cast<double>(transformDrag_.snapRotateAxis.y()),
                                             static_cast<double>(transformDrag_.snapRotateAxis.z())};
            transform.pivot = transformDrag_.documentSnapRotatePivot.value_or(
                TVector{transformDrag_.snapRotatePivot.x(), transformDrag_.snapRotatePivot.y(), transformDrag_.snapRotatePivot.z()});
            // Nearby snap solvers publish their viewport-axis angle; the
            // matching geometry commit signals use its negated core angle.
            transform.angleDegrees = -static_cast<double>(transformDrag_.snapRotateAngleDeg);
        }
        else
        {
            transform.kind = level::ConnectedSurfaceUvTransformKind::translate;
        }
    }
    else if ( scale )
    {
        transform.kind = level::ConnectedSurfaceUvTransformKind::scale;
        transform.scale.cx = documentPivot.x;
        transform.scale.cy = documentPivot.y;
        transform.scale.cz = documentPivot.z;
        transform.scale.sfactor = static_cast<double>(transformDrag_.scaleFactor);
        transform.scale.how = handle == TransformHandle::ScaleX   ? ScaleHow::x
                              : handle == TransformHandle::ScaleY ? ScaleHow::y
                              : handle == TransformHandle::ScaleZ ? ScaleHow::z
                                                                  : ScaleHow::xyz;
    }
    else
    {
        transform.kind = level::ConnectedSurfaceUvTransformKind::rotate;
        transform.rotationAxis =
            TVector{static_cast<double>(transformDrag_.axis.x()), static_cast<double>(transformDrag_.axis.y()),
                    static_cast<double>(transformDrag_.axis.z())};
        transform.pivot = documentPivot;
        transform.angleDegrees = static_cast<double>(transformDrag_.angleDeg);
    }

    const level::ConnectedSurfaceUvEvaluation evaluation =
        level::evaluateConnectedSurfaceUvPlan(*transformSurfaceUvPlan_, transform);
    if ( !publishTransformSurfaceUvPreview(evaluation) )
    {
        if ( !transformSurfaceUvPreviewSurfaces_.empty() )
        {
            const std::vector<SurfaceRef> restore = std::move(transformSurfaceUvPreviewSurfaces_);
            transformSurfaceUvPreviewSurfaces_.clear();
            notifySurfaceUvsChanged(restore);
        }
        // Never display transformed geometry with a failed requested UV
        // correction. A later valid sample rebuilds this matrix normally.
        transformPreviewMatrix_.setToIdentity();
        reportConnectedSurfaceUvFailure(QStringLiteral("The live UV correction could not be evaluated"));
        update();
    }
}

void GlView::reportConnectedSurfaceUvFailure(const QString& detail)
{
    if ( transformSurfaceUvFailureReported_ && transformDrag_.active )
    {
        return;
    }
    transformSurfaceUvFailureReported_ = true;

    QString message = QStringLiteral("Transform cancelled: connected surface UVs could not be preserved");
    if ( !detail.isEmpty() )
    {
        message += QStringLiteral(". ") + detail;
    }
    showToast(message, 4000, ToastPlacement::BottomCenter, ToastGroup::Error);
    Q_EMIT errorMessage(message);
}

void GlView::restoreTransformSurfaceUvPreview()
{
    transformSurfaceUvPlan_.reset();
    if ( transformSurfaceUvPreviewSurfaces_.empty() )
    {
        return;
    }
    const std::vector<SurfaceRef> restore = std::move(transformSurfaceUvPreviewSurfaces_);
    transformSurfaceUvPreviewSurfaces_.clear();
    notifySurfaceUvsChanged(restore);
}

void GlView::notifyBakedLightingChanged(const std::vector<std::int32_t>& sectorIndices,
                                        const bool thingShadingMayHaveChanged)
{
    if (!level_)
    {
        return;
    }

    if (!levelMeshCache_.valid)
    {
        notifyAllMeshesChanged();
        return;
    }

    std::unordered_set<std::int32_t> targets{};
    targets.reserve(sectorIndices.size());
    for (const std::int32_t sectorIndex : sectorIndices)
    {
        if (sectorIndex >= 0
            && static_cast<std::size_t>(sectorIndex) < level_->sectors.size())
        {
            targets.insert(sectorIndex);
        }
    }

    const auto maxLightMode =
        static_cast<TLightMode>(static_cast<std::int32_t>(lightingMode_));
    std::vector<DrawRange> dirtyRanges{};
    int globalFirstVertex = 0;
    bool mappingValid = true;

    for (TriBuild& build : levelMeshCache_.triBuilds)
    {
        if (build.shadeInputs.size() != build.vertices.size())
        {
            mappingValid = false;
            break;
        }

        int dirtyFirst = -1;
        int dirtyCount = 0;
        const auto flushDirty = [&]()
        {
            if (dirtyFirst >= 0 && dirtyCount > 0)
            {
                dirtyRanges.push_back(
                    DrawRange{ globalFirstVertex + dirtyFirst, dirtyCount });
            }
            dirtyFirst = -1;
            dirtyCount = 0;
        };

        for (std::size_t vertexIndex = 0; vertexIndex < build.vertices.size(); ++vertexIndex)
        {
            TriShadeInput& shade = build.shadeInputs[vertexIndex];
            if (shade.sourceSector < 0 || !targets.contains(shade.sourceSector))
            {
                flushDirty();
                continue;
            }
            if (static_cast<std::size_t>(shade.sourceSector) >= level_->sectors.size())
            {
                mappingValid = false;
                break;
            }

            const Sector& sector =
                level_->sectors[static_cast<std::size_t>(shade.sourceSector)];
            if (shade.sourceSurface < 0
                || static_cast<std::size_t>(shade.sourceSurface) >= sector.surfaces.size())
            {
                mappingValid = false;
                break;
            }
            const Surface& surface =
                sector.surfaces[static_cast<std::size_t>(shade.sourceSurface)];
            if (shade.sourceTexVertex < 0
                || static_cast<std::size_t>(shade.sourceTexVertex) >= surface.texVertices.size())
            {
                mappingValid = false;
                break;
            }

            ColorF intensity = surface.texVertices[
                static_cast<std::size_t>(shade.sourceTexVertex)].color;
            intensity.a = surface.extraLight.a;
            const bool dynamicVertexLighting =
                shade.lightMode >= static_cast<std::int32_t>(TLightMode::Gouraud)
                && maxLightMode >= TLightMode::Gouraud;
            const bool ijimDynamicWorldLighting =
                level_->kind == ProjectType::IJIM && dynamicVertexLighting;
            const ColorF lit = polyCalcLitColor(
                intensity,
                shade.lightMode,
                ijimDynamicWorldLighting ? ColorBlack : sector.extraLight,
                ijimDynamicWorldLighting ? ColorBlack : surface.extraLight,
                level_->kind,
                maxLightMode,
                dynamicVertexLighting);

            TriVertex& vertex = build.vertices[vertexIndex];
            vertex.r = lit.r;
            vertex.g = lit.g;
            vertex.b = lit.b;
            vertex.a = render::classicFaceOpacity(
                level_->kind,
                surface.faceFlags,
                lit.a);
            shade.intensity = intensity;
            shade.extraLight = surface.extraLight;

            const int globalIndex =
                globalFirstVertex + static_cast<int>(vertexIndex);
            if (globalIndex < 0
                || static_cast<std::size_t>(globalIndex) >= triVertices_.size())
            {
                mappingValid = false;
                break;
            }
            triVertices_[static_cast<std::size_t>(globalIndex)] = vertex;

            if (dirtyFirst < 0)
            {
                dirtyFirst = static_cast<int>(vertexIndex);
                dirtyCount = 1;
            }
            else if (dirtyFirst + dirtyCount == static_cast<int>(vertexIndex))
            {
                ++dirtyCount;
            }
            else
            {
                flushDirty();
                dirtyFirst = static_cast<int>(vertexIndex);
                dirtyCount = 1;
            }
        }
        flushDirty();
        if (!mappingValid)
        {
            break;
        }
        globalFirstVertex += static_cast<int>(build.vertices.size());
    }

    if (!mappingValid || globalFirstVertex != levelTriVertexCount_)
    {
        notifyAllMeshesChanged();
        return;
    }

    render::mergeDrawRanges(dirtyRanges);
    bool uploadFailed = false;
    if (glReady_ && !dirtyRanges.empty())
    {
        makeCurrent();
        if (!triVbo_
            || triVboAllocatedBytes_ <= 0
            || triVertexCount_ != static_cast<int>(triVertices_.size()))
        {
            uploadFailed = true;
        }
        else
        {
            triVbo_->bind();
            for (const DrawRange& range : dirtyRanges)
            {
                const int offsetBytes =
                    range.first * static_cast<int>(sizeof(TriVertex));
                const int byteCount =
                    range.count * static_cast<int>(sizeof(TriVertex));
                if (range.first < 0 || range.count <= 0
                    || range.first + range.count > static_cast<int>(triVertices_.size())
                    || offsetBytes < 0
                    || offsetBytes + byteCount > triVboAllocatedBytes_)
                {
                    uploadFailed = true;
                    break;
                }
                triVbo_->write(
                    offsetBytes,
                    triVertices_.data() + range.first,
                    byteCount);
            }
            triVbo_->release();
        }
        doneCurrent();
    }

    if (uploadFailed)
    {
        notifyAllMeshesChanged();
        return;
    }

    if (thingShadingMayHaveChanged && !level_->things.empty())
    {
        // Sector Ambient is a Thing camera-light input. The existing fast
        // Thing path detects just the affected sector cache entries and
        // recolors their cached owner ranges in place.
        notifyThingsChanged();
        return;
    }

    markViewChanged();
    update();
}


const char* GlView::thingTransformFastFallbackReasonName(
    const ThingTransformFastFallbackReason reason) noexcept
{
    switch (reason)
    {
        case ThingTransformFastFallbackReason::none: return "none";
        case ThingTransformFastFallbackReason::missingLevel: return "missing-level";
        case ThingTransformFastFallbackReason::invalidMeshCache: return "invalid-mesh-cache";
        case ThingTransformFastFallbackReason::missingRenderState: return "missing-render-state";
        case ThingTransformFastFallbackReason::topologyMismatch: return "topology-mismatch";
        case ThingTransformFastFallbackReason::pickMappingMismatch: return "pick-mapping-mismatch";
        case ThingTransformFastFallbackReason::geometrySignatureChanged: return "geometry-signature-changed";
        case ThingTransformFastFallbackReason::boundsMappingMismatch: return "bounds-mapping-mismatch";
        case ThingTransformFastFallbackReason::nonFiniteThingTransform: return "non-finite-thing-transform";
        case ThingTransformFastFallbackReason::nonFiniteCachedTransform: return "non-finite-cached-transform";
        case ThingTransformFastFallbackReason::nonInvertibleTransform: return "non-invertible-transform";
        case ThingTransformFastFallbackReason::boundsOwnerOutOfRange: return "bounds-owner-out-of-range";
        case ThingTransformFastFallbackReason::triBuildMappingMismatch: return "tri-build-mapping-mismatch";
        case ThingTransformFastFallbackReason::triBuildRangeInvalid: return "tri-build-range-invalid";
        case ThingTransformFastFallbackReason::lineRangeInvalid: return "line-range-invalid";
        case ThingTransformFastFallbackReason::pointMappingInvalid: return "point-mapping-invalid";
        case ThingTransformFastFallbackReason::composedBufferRangeInvalid: return "composed-buffer-range-invalid";
        case ThingTransformFastFallbackReason::unsupportedViewMode: return "unsupported-view-mode";
        case ThingTransformFastFallbackReason::count: break;
    }
    return "unknown";
}

bool GlView::tryFastUpdateThingTransforms(
    const std::vector<std::int32_t>* changedThingIndices)
{
    lastThingTransformFastFallbackReason_ =
        ThingTransformFastFallbackReason::none;
    const auto fail = [this](const ThingTransformFastFallbackReason reason)
    {
        lastThingTransformFastFallbackReason_ = reason;
        const std::size_t index = static_cast<std::size_t>(reason);
        if (index < thingTransformFastFallbackReasonCounts_.size())
        {
            ++thingTransformFastFallbackReasonCounts_[index];
        }
        return false;
    };

    if (!level_)
    {
        return fail(ThingTransformFastFallbackReason::missingLevel);
    }

    // Model-vertex mode stores a variable number of point vertices per Thing.
    // The compact Dots mapping is deliberately one-point-per-Thing, so use the
    // conservative rebuild until a ranged point fast path exists.
    if (thingViewMode_ == ThingViewMode::Vertices)
    {
        return fail(ThingTransformFastFallbackReason::unsupportedViewMode);
    }

    if (!thingMeshCache_.valid)
    {
        return fail(ThingTransformFastFallbackReason::invalidMeshCache);
    }

    // An empty Thing cache is a valid, fully up-to-date cache when the level
    // has no Things. Treating it as invalid made an ambient-only bake publish
    // rebuild the empty Things mesh on every automatic result.
    if (level_->things.empty() && thingRenderState_.empty())
    {
        return true;
    }
    if (thingRenderState_.empty())
    {
        return fail(ThingTransformFastFallbackReason::missingRenderState);
    }

    if (level_->things.size() != thingRenderState_.size())
    {
        return fail(ThingTransformFastFallbackReason::topologyMismatch);
    }

    const bool targeted = changedThingIndices != nullptr;
    std::vector<std::size_t> targetIndices{};
    if (targeted)
    {
        targetIndices.reserve(changedThingIndices->size());
        for (const std::int32_t index : *changedThingIndices)
        {
            if (index < 0 || static_cast<std::size_t>(index) >= level_->things.size())
            {
                continue;
            }
            targetIndices.push_back(static_cast<std::size_t>(index));
        }
        std::sort(targetIndices.begin(), targetIndices.end());
        targetIndices.erase(
            std::unique(targetIndices.begin(), targetIndices.end()),
            targetIndices.end());
        if (targetIndices.empty())
        {
            return true;
        }
    }
    else
    {
        targetIndices.resize(level_->things.size());
        std::iota(targetIndices.begin(), targetIndices.end(), std::size_t{0});
    }

    // Bail out if any thing changed its geometry-affecting properties; those require a rebuild.
    for (const std::size_t i : targetIndices)
    {
        const Thing& t = level_->things[i];
        if (thingGeometrySignature(t) != thingRenderState_[i].signature)
        {
            return fail(ThingTransformFastFallbackReason::geometrySignatureChanged);
        }
    }

    // If things are hidden, just refresh cached transforms/sectors.
    if (!showThings_)
    {
        for (const std::size_t i : targetIndices)
        {
            const Thing& t = level_->things[i];
            ThingRenderState& st = thingRenderState_[i];
            st.x = t.x;
            st.y = t.y;
            st.z = t.z;
            st.pch = t.pch;
            st.yaw = t.yaw;
            st.rol = t.rol;
            st.sector = t.sector;
            st.runtimeLightSignature = thingRuntimeLightSignature(t);
            const render::thing_overlay::CollisionDescription collision =
                render::thing_overlay::resolveCollisionDescription(*level_, templates_, t);
            st.collisionOverlayShape = static_cast<std::uint8_t>(collision.shape);
            st.collisionOverlayRadius = collision.radius;
            st.collisionOverlayHasRadius = collision.hasRadius;
            st.collisionOverlayBroadPhaseRadius = collision.broadPhaseRadius;
        }
        return true;
    }

    // Pick ids are intentionally sparse when Things live on hidden layers:
    // getThingPickId() is never called for geometry that was not emitted. A
    // visible owner, however, must always have a non-zero dense mapping.
    for (const std::size_t i : targetIndices)
    {
        const Thing& thing = level_->things[i];
        if (isLayerVisible(thing.layer)
            && shouldShowThing(thing)
            && (i >= thingPickIds_.size() || thingPickIds_[i] == 0u))
        {
            return fail(ThingTransformFastFallbackReason::pickMappingMismatch);
        }
    }

    std::vector<DrawRange> lineDirty{};
    struct CollisionSpherePatch
    {
        int localFirst{};
        int globalFirst{};
        std::vector<LineVertex> vertices{};
    };
    std::vector<CollisionSpherePatch> collisionSpherePatches{};
    collisionSpherePatches.reserve(targetIndices.size());

    // Radius scrubs preserve a sphere's fixed line topology, so patch only that
    // owner's 144 cached line vertices. A collision mode/topology transition
    // still falls back to the ordinary Things-only rebuild.
    if (thingOverlayGeometryPrepared_ && showThings_)
    {
        for (const std::size_t i : targetIndices)
        {
            const Thing& thing = level_->things[i];
            const ThingRenderState& cached = thingRenderState_[i];
            const render::thing_overlay::CollisionDescription previous{
                static_cast<render::thing_overlay::CollisionShape>(cached.collisionOverlayShape),
                cached.collisionOverlayRadius,
                cached.collisionOverlayHasRadius,
                cached.collisionOverlayBroadPhaseRadius,
            };
            const render::thing_overlay::CollisionDescription current =
                render::thing_overlay::resolveCollisionDescription(
                    *level_, templates_, thing);
            if (render::thing_overlay::sameCollisionDescription(previous, current)
                || !isLayerVisible(thing.layer)
                || !shouldShowThing(thing))
            {
                continue;
            }

            if (!render::thing_overlay::canUpdateCollisionSphereInPlace(previous, current))
            {
                return fail(ThingTransformFastFallbackReason::geometrySignatureChanged);
            }

            const std::uint32_t pickIdValue = thingPickIds_[i];
            if (pickIdValue == 0u
                || pickIdValue > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
            {
                return fail(ThingTransformFastFallbackReason::pickMappingMismatch);
            }
            const auto rangeIt = thingCollisionOverlayRangesByPickId_.find(
                static_cast<int>(pickIdValue));
            if (rangeIt == thingCollisionOverlayRangesByPickId_.end()
                || rangeIt->second.size() != 1u)
            {
                return fail(ThingTransformFastFallbackReason::lineRangeInvalid);
            }

            const DrawRange& range = rangeIt->second.front();
            if (range.first < 0 || range.count <= 0
                || !std::isfinite(cached.x) || !std::isfinite(cached.y) || !std::isfinite(cached.z)
                || static_cast<std::size_t>(range.first) + static_cast<std::size_t>(range.count)
                    > thingMeshCache_.overlayLineVertices.size())
            {
                return fail(ThingTransformFastFallbackReason::lineRangeInvalid);
            }

            std::vector<TVector> points =
                render::thing_overlay::makeSphereWirePoints(current.radius);
            for (TVector& point : points)
            {
                // Keep the replacement sphere at the cached position first;
                // the ordinary transform delta below moves it once if this
                // publication also changes the Thing pose.
                point.x += cached.x;
                point.y += cached.y;
                point.z += cached.z;
            }
            points = render::thing_overlay::finiteWireSegments(points);
            if (points.empty() || points.size() != static_cast<std::size_t>(range.count)
                || thingOverlayLineFirstVertex_ < 0
                || range.first > std::numeric_limits<int>::max() - thingOverlayLineFirstVertex_)
            {
                return fail(ThingTransformFastFallbackReason::lineRangeInvalid);
            }

            const int globalFirst = thingOverlayLineFirstVertex_ + range.first;
            if (static_cast<std::size_t>(globalFirst) + points.size() > lineVertices_.size())
            {
                return fail(ThingTransformFastFallbackReason::composedBufferRangeInvalid);
            }

            CollisionSpherePatch patch{};
            patch.localFirst = range.first;
            patch.globalFirst = globalFirst;
            patch.vertices.reserve(points.size());
            for (const TVector& point : points)
            {
                LineVertex vertex{};
                vertex.x = static_cast<float>(point.x);
                vertex.y = static_cast<float>(point.y);
                vertex.z = static_cast<float>(point.z);
                vertex.pickId = static_cast<float>(pickIdValue);
                patch.vertices.push_back(vertex);
            }
            collisionSpherePatches.push_back(std::move(patch));
        }
    }

    for (const CollisionSpherePatch& patch : collisionSpherePatches)
    {
        const std::size_t count = patch.vertices.size();
        std::copy(
            patch.vertices.begin(), patch.vertices.end(),
            thingMeshCache_.overlayLineVertices.begin() + patch.localFirst);
        std::copy(
            patch.vertices.begin(), patch.vertices.end(),
            lineVertices_.begin() + patch.globalFirst);
        lineDirty.push_back(DrawRange{
            patch.globalFirst,
            static_cast<int>(count),
        });
    }

    const auto maxLightMode = static_cast<TLightMode>(static_cast<std::int32_t>(lightingMode_));

    auto calcThingSectorColorMul = [&](const SectorThingLightCache& from, const SectorThingLightCache& to) -> QVector3D
    {
        if (!from.valid || !to.valid)
        {
            return QVector3D(1.0f, 1.0f, 1.0f);
        }
        return calcThingLightMul(from.ambient, to.ambient);
    };

    std::unordered_map<int, render::fast_update::ThingTransformDelta> deltas{};
    deltas.reserve(targetIndices.size());

    std::unordered_map<std::int32_t, QVector3D> sectorLightMul{};
    std::unordered_set<std::int32_t> sectorPointLightChanged{};
    std::unordered_set<std::int32_t> sectorTintChanged{};
    if (!targeted)
    {
        // Only a broad lighting publication compares every Sector cache.
        // Pure targeted Thing transforms resolve their old/new Sector light
        // directly below; reserving and initializing these containers to the
        // complete level size made every release O(all Sectors), including
        // authoring-hidden layers.
        if (thingSectorLightCache_.size() != level_->sectors.size())
        {
            thingSectorLightCache_.clear();
            thingSectorLightCache_.resize(level_->sectors.size());
            for (std::size_t i = 0; i < level_->sectors.size(); ++i)
            {
                SectorThingLightCache c{};
                const ThingSectorLighting lights = calcThingSectorLighting(
                    level_,
                    static_cast<std::int32_t>(i));
                c.ambient = lights.ambientLight;
                c.pointLight = level_->sectors[i].pointLight;
                c.editorSectorTint = render::classicEditorSectorTintScale(
                    level_, static_cast<std::int32_t>(i));
                c.valid = true;
                thingSectorLightCache_[i] = c;
            }
        }
        sectorLightMul.reserve(level_->sectors.size());
        sectorPointLightChanged.reserve(level_->sectors.size());
        sectorTintChanged.reserve(level_->sectors.size());
        for (std::size_t i = 0; i < level_->sectors.size(); ++i)
        {
            const std::int32_t sectorIndex = static_cast<std::int32_t>(i);

            SectorThingLightCache cur{};
            const ThingSectorLighting curLights = calcThingSectorLighting(level_, sectorIndex);
            cur.ambient = curLights.ambientLight;
            cur.pointLight = level_->sectors[i].pointLight;
            cur.editorSectorTint = render::classicEditorSectorTintScale(
                level_, sectorIndex);
            cur.valid = true;

            SectorThingLightCache& prev = thingSectorLightCache_[i];
            if (!prev.valid)
            {
                prev = cur;
                continue;
            }

            const bool changed =
                !render::fast_update::nearlySameRenderValue(prev.ambient, cur.ambient);
            const bool pointChanged =
                !render::fast_update::nearlySameRenderValue(prev.pointLight.color, cur.pointLight.color)
                || !render::fast_update::nearlySameTransform(prev.pointLight.position.x, cur.pointLight.position.x)
                || !render::fast_update::nearlySameTransform(prev.pointLight.position.y, cur.pointLight.position.y)
                || !render::fast_update::nearlySameTransform(prev.pointLight.position.z, cur.pointLight.position.z)
                || !render::fast_update::nearlySameTransform(prev.pointLight.minRange, cur.pointLight.minRange)
                || !render::fast_update::nearlySameTransform(prev.pointLight.maxRange, cur.pointLight.maxRange);
            const bool tintChanged =
                !render::fast_update::nearlySameRenderValue(
                    prev.editorSectorTint, cur.editorSectorTint);
            if (!changed && !pointChanged && !tintChanged)
            {
                continue;
            }

            if (changed)
            {
                sectorLightMul.emplace(sectorIndex, calcThingSectorColorMul(prev, cur));
            }
            if (pointChanged)
            {
                sectorPointLightChanged.emplace(sectorIndex);
            }
            if (tintChanged)
            {
                sectorTintChanged.emplace(sectorIndex);
            }
            prev = cur;
        }
    }

    for (const std::size_t i : targetIndices)
    {
        const Thing& t = level_->things[i];
        ThingRenderState& prev = thingRenderState_[i];

        if (!std::isfinite(t.x) || !std::isfinite(t.y) || !std::isfinite(t.z)
            || !std::isfinite(t.pch) || !std::isfinite(t.yaw) || !std::isfinite(t.rol))
        {
            return fail(ThingTransformFastFallbackReason::nonFiniteThingTransform);
        }

        if (!std::isfinite(prev.x) || !std::isfinite(prev.y) || !std::isfinite(prev.z)
            || !std::isfinite(prev.pch) || !std::isfinite(prev.yaw) || !std::isfinite(prev.rol))
        {
            return fail(ThingTransformFastFallbackReason::nonFiniteCachedTransform);
        }

        const bool xformChanged =
            !render::fast_update::nearlySameTransform(prev.x, t.x)
            || !render::fast_update::nearlySameTransform(prev.y, t.y)
            || !render::fast_update::nearlySameTransform(prev.z, t.z)
            || !render::fast_update::nearlySameTransform(prev.pch, t.pch)
            || !render::fast_update::nearlySameTransform(prev.yaw, t.yaw)
            || !render::fast_update::nearlySameTransform(prev.rol, t.rol);

        const bool sectorChanged = (prev.sector != t.sector);
        const bool runtimeLightChanged =
            prev.runtimeLightSignature != thingRuntimeLightSignature(t);

        const bool sectorLightChanged =
            !sectorChanged
            && (t.sector >= 0)
            && sectorLightMul.contains(t.sector);

        const bool pointLightChanged =
            !sectorChanged
            && level_->kind == ProjectType::IJIM
            && (t.sector >= 0)
            && sectorPointLightChanged.contains(t.sector);

        const bool tintChanged =
            !sectorChanged
            && (t.sector >= 0)
            && sectorTintChanged.contains(t.sector);

        if (!xformChanged && !sectorChanged && !sectorLightChanged
            && !pointLightChanged && !tintChanged && !runtimeLightChanged)
        {
            continue;
        }

        if (!isLayerVisible(t.layer) || !shouldShowThing(t))
        {
            // Hidden/non-rendered owners can retain an old id when layer
            // visibility changes without resetting the global pick table.
            // Their authoritative cached state is refreshed below, but they
            // have no mesh or aggregate bounds to patch.
            continue;
        }

        // The visible-owner mapping was validated above. Hidden owners have
        // already taken the cache-only branch and never reach this access.
        const std::uint32_t pickIdU32 = thingPickIds_[i];

        QMatrix4x4 dm;
        dm.setToIdentity();
        if (xformChanged)
        {
            TVector oldPyr{};
            oldPyr.pitch = prev.pch;
            oldPyr.yaw = prev.yaw;
            oldPyr.roll = prev.rol;

            TVector newPyr{};
            newPyr.pitch = t.pch;
            newPyr.yaw = t.yaw;
            newPyr.roll = t.rol;

            const TMatrix43 oldXf = TMatrix43::Create(TVector{ prev.x, prev.y, prev.z }, oldPyr);
            const TMatrix43 newXf = TMatrix43::Create(TVector{ t.x, t.y, t.z }, newPyr);
            const QMatrix4x4 oldQt = matrix43ToQt(oldXf);
            bool invOk = false;
            const QMatrix4x4 invOldQt = oldQt.inverted(&invOk);
            if (!invOk)
            {
                return fail(ThingTransformFastFallbackReason::nonInvertibleTransform);
            }

            const QMatrix4x4 newQt = matrix43ToQt(newXf);
            dm = newQt * invOldQt;
        }

        render::fast_update::ThingTransformDelta d{};
        d.xf = render::fast_update::affineFromMatrix(dm);
        d.recolor = sectorChanged || sectorLightChanged || runtimeLightChanged;
        if (sectorChanged)
        {
            d.colorMul = calcThingSectorLightingMul(level_, prev.sector, t.sector);
        }
        else if (sectorLightChanged)
        {
            d.colorMul = sectorLightMul[t.sector];
        }

        if (d.recolor)
        {
            const std::int32_t sectorIndex = t.sector;
            const ThingSectorLighting lights = calcThingSectorLighting(level_, sectorIndex);
            d.ambientLight = lights.ambientLight;
            const render::lighting::RuntimeThingLight selfLight = render::lighting::resolveRuntimeThingLight(
                *level_,
                templates_,
                t,
                static_cast<std::int32_t>(i),
                std::nullopt,
                render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
            if constexpr (render::lighting::kRuntimeDynamicLightPreviewEnabled)
            {
                d.ambientLight = render::lighting::applyRuntimeThingSelfLight(
                    level_->kind,
                    d.ambientLight,
                    selfLight,
                    thingLightRenderingEnabled_);
            }
        }

        d.updateSectorPointLight = level_->kind == ProjectType::IJIM && (sectorChanged || pointLightChanged);
        d.updateEditorSectorTint = sectorChanged || tintChanged;
        if (d.updateEditorSectorTint)
        {
            d.editorSectorTint = render::classicEditorSectorTintScale(
                level_, t.sector);
        }
        d.sectorLightOwner = (t.sector >= 0
                && static_cast<std::size_t>(t.sector) < level_->sectors.size())
            ? t.sector
            : -1;
        if (d.updateSectorPointLight
            && t.sector >= 0
            && static_cast<std::size_t>(t.sector) < level_->sectors.size())
        {
            d.sectorPointLight = level_->sectors[static_cast<std::size_t>(t.sector)].pointLight;
            const render::lighting::RuntimeSectorPointLight runtime =
                render::lighting::resolveRuntimeSectorPointLight(d.sectorPointLight);
            d.sectorPointLight.position = TVector{
                runtime.position.x(), runtime.position.y(), runtime.position.z() };
            d.sectorPointLight.color = ColorF{
                runtime.color.x(), runtime.color.y(), runtime.color.z(), 1.0f };
            d.sectorPointLight.minRange = runtime.minRadius;
            d.sectorPointLight.maxRange = runtime.maxRadius;
        }

        d.thingIndex = i;
        d.xformChanged = xformChanged;

        deltas.emplace(static_cast<int>(pickIdU32), d);
    }

    // Update cached thing transforms regardless of whether any renderable geometry changed.
    for (const std::size_t i : targetIndices)
    {
        const Thing& t = level_->things[i];
        ThingRenderState& st = thingRenderState_[i];
        st.x = t.x;
        st.y = t.y;
        st.z = t.z;
        st.pch = t.pch;
        st.yaw = t.yaw;
        st.rol = t.rol;
        st.sector = t.sector;
        st.runtimeLightSignature = thingRuntimeLightSignature(t);
        const render::thing_overlay::CollisionDescription collision =
            render::thing_overlay::resolveCollisionDescription(*level_, templates_, t);
        st.collisionOverlayShape = static_cast<std::uint8_t>(collision.shape);
        st.collisionOverlayRadius = collision.radius;
        st.collisionOverlayHasRadius = collision.hasRadius;
        st.collisionOverlayBroadPhaseRadius = collision.broadPhaseRadius;
    }

    if (deltas.empty())
    {
        if (lineDirty.empty())
        {
            return true;
        }

        if (glReady_)
        {
            makeCurrent();
            render::mergeDrawRanges(lineDirty);
            bool fallbackUpload = !lineVbo_ || lineVboAllocatedBytes_ <= 0
                || wireLineVertexCount_ + thingLineVertexCount_ + thingOverlayLineVertexCount_
                    != static_cast<int>(lineVertices_.size());
            if (!fallbackUpload)
            {
                lineVbo_->bind();
                for (const DrawRange& range : lineDirty)
                {
                    if (range.first < 0 || range.count <= 0
                        || static_cast<std::size_t>(range.first)
                            + static_cast<std::size_t>(range.count) > lineVertices_.size())
                    {
                        fallbackUpload = true;
                        break;
                    }
                    const std::int64_t offsetBytes =
                        static_cast<std::int64_t>(range.first) * static_cast<std::int64_t>(sizeof(LineVertex));
                    const std::int64_t bytes =
                        static_cast<std::int64_t>(range.count) * static_cast<std::int64_t>(sizeof(LineVertex));
                    if (offsetBytes < 0 || bytes <= 0
                        || offsetBytes + bytes > lineVboAllocatedBytes_
                        || offsetBytes > std::numeric_limits<int>::max()
                        || bytes > std::numeric_limits<int>::max())
                    {
                        fallbackUpload = true;
                        break;
                    }
                    lineVbo_->write(
                        static_cast<int>(offsetBytes),
                        lineVertices_.data() + range.first,
                        static_cast<int>(bytes));
                }
                lineVbo_->release();
            }

            if (fallbackUpload)
            {
                uploadMeshes();
            }
            doneCurrent();
        }
        return true;
    }

    // Non-rendered/hidden targets produce no deltas and do not need the dense
    // bounds mapping at all. Validate it only once visible geometry must be
    // patched, avoiding an unnecessary full rebuild for cache-only owners.
    if (thingBounds_.size() != level_->things.size())
    {
        return fail(ThingTransformFastFallbackReason::boundsMappingMismatch);
    }

    for (const auto& [pid, d] : deltas)
    {
        (void)pid;
        if (d.thingIndex >= thingBounds_.size())
        {
            return fail(ThingTransformFastFallbackReason::boundsOwnerOutOfRange);
        }

        ThingBounds& b = thingBounds_[d.thingIndex];
        if (!b.valid)
        {
            const Thing& t = level_->things[d.thingIndex];
            if (std::isfinite(t.x) && std::isfinite(t.y) && std::isfinite(t.z))
            {
                const QVector3D p(static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z));
                b.min = p;
                b.max = p;
                b.valid = true;
            }
        }

        if (d.xformChanged && b.valid)
        {
            render::fast_update::transformBounds(b.min, b.max, d.xf);
        }
    }

    auto applyDeltaToTriVertex = [&](TriBuild& build, const int vertexIndex, const render::fast_update::ThingTransformDelta& d, const bool allowRecolor) -> void
    {
        if (vertexIndex < 0 || static_cast<std::size_t>(vertexIndex) >= build.vertices.size())
        {
            return;
        }

        TriVertex& v = build.vertices[static_cast<std::size_t>(vertexIndex)];
        if (d.xformChanged)
        {
            QVector3D position(v.x, v.y, v.z);
            QVector3D normal(v.normalX, v.normalY, v.normalZ);
            QVector3D diffuseFaceAnchor(
                v.faceLightAnchorX,
                v.faceLightAnchorY,
                v.faceLightAnchorZ);
            render::fast_update::transformDynamicLightGeometry(
                position,
                normal,
                v.dynamicLightTarget > 2.5f ? &diffuseFaceAnchor : nullptr,
                d.xf);
            v.x = position.x();
            v.y = position.y();
            v.z = position.z();
            v.normalX = normal.x();
            v.normalY = normal.y();
            v.normalZ = normal.z();
            if (v.dynamicLightTarget > 2.5f)
            {
                v.faceLightAnchorX = diffuseFaceAnchor.x();
                v.faceLightAnchorY = diffuseFaceAnchor.y();
                v.faceLightAnchorZ = diffuseFaceAnchor.z();
            }
        }

        if (d.updateSectorPointLight && v.dynamicLightTarget > 1.5f)
        {
            v.sectorLightX = static_cast<float>(d.sectorPointLight.position.x);
            v.sectorLightY = static_cast<float>(d.sectorPointLight.position.y);
            v.sectorLightZ = static_cast<float>(d.sectorPointLight.position.z);
            v.sectorLightMinRadius = static_cast<float>(d.sectorPointLight.minRange);
            v.sectorLightR = d.sectorPointLight.color.r;
            v.sectorLightG = d.sectorPointLight.color.g;
            v.sectorLightB = d.sectorPointLight.color.b;
            v.sectorLightMaxRadius = static_cast<float>(d.sectorPointLight.maxRange);
            v.sectorLightOwner = static_cast<float>(d.sectorLightOwner);
        }

        if (d.updateEditorSectorTint)
        {
            v.editorSectorTintR = d.editorSectorTint.x();
            v.editorSectorTintG = d.editorSectorTint.y();
            v.editorSectorTintB = d.editorSectorTint.z();
        }

        if (allowRecolor && d.recolor)
        {
            if (static_cast<std::size_t>(vertexIndex) < build.shadeInputs.size())
            {
                const TriShadeInput& si = build.shadeInputs[static_cast<std::size_t>(vertexIndex)];
                const ColorF lit = polyCalcLitColor(
                    si.intensity,
                    si.lightMode,
                    d.ambientLight,
                    si.extraLight,
                    level_->kind,
                    maxLightMode,
                    v.dynamicLightTarget > 1.5f);
                v.r = lit.r;
                v.g = lit.g;
                v.b = lit.b;
                if (level_->kind != ProjectType::IJIM && v.dynamicLightTarget > 1.5f)
                {
                    v.sectorLightX = si.extraLight.r;
                    v.sectorLightY = si.extraLight.g;
                    v.sectorLightZ = si.extraLight.b;
                    v.sectorLightR = d.ambientLight.r;
                    v.sectorLightG = d.ambientLight.g;
                    v.sectorLightB = d.ambientLight.b;
                }
            }
            else
            {
                // Fallback: approximate light change via a multiplicative ratio.
                v.r = std::clamp(v.r * d.colorMul.x(), 0.0f, 1.0f);
                v.g = std::clamp(v.g * d.colorMul.y(), 0.0f, 1.0f);
                v.b = std::clamp(v.b * d.colorMul.z(), 0.0f, 1.0f);
            }
        }
    };

    auto applyDeltaToLineVertex = [&](LineVertex& v, const render::fast_update::ThingTransformDelta& d) -> void
    {
        if (!d.xformChanged)
        {
            return;
        }

        const QVector3D p(v.x, v.y, v.z);
        const QVector3D q = d.xf.mapPoint(p);
        v.x = q.x();
        v.y = q.y();
        v.z = q.z();

        if (v.planeValid > 0.5f)
        {
            const QVector3D planePoint(v.planeX, v.planeY, v.planeZ);
            const QVector3D transformedPlanePoint = d.xf.mapPoint(planePoint);
            v.planeX = transformedPlanePoint.x();
            v.planeY = transformedPlanePoint.y();
            v.planeZ = transformedPlanePoint.z();
        }
    };

    // Apply deltas to cached buffers using precomputed per-pick-id ranges, and mirror into composed buffers.
    std::vector<DrawRange> triDirty{};
    auto updateOverlayRanges = [&](
        const std::unordered_map<int, std::vector<DrawRange>>& rangesByPickId,
        const int pickId,
        const render::fast_update::ThingTransformDelta& delta) -> bool
    {
        if (!delta.xformChanged)
        {
            return true;
        }
        const auto rangeIt = rangesByPickId.find(pickId);
        if (rangeIt == rangesByPickId.end())
        {
            return true;
        }
        for (const DrawRange& range : rangeIt->second)
        {
            if (range.first < 0 || range.count <= 0
                || static_cast<std::size_t>(range.first + range.count)
                    > thingMeshCache_.overlayLineVertices.size())
            {
                return false;
            }
            for (int vertexIndex = 0; vertexIndex < range.count; ++vertexIndex)
            {
                applyDeltaToLineVertex(
                    thingMeshCache_.overlayLineVertices[
                        static_cast<std::size_t>(range.first + vertexIndex)],
                    delta);
            }

            const int globalFirst = thingOverlayLineFirstVertex_ + range.first;
            if (globalFirst < 0 || globalFirst + range.count > static_cast<int>(lineVertices_.size()))
            {
                return false;
            }
            std::copy(
                thingMeshCache_.overlayLineVertices.begin() + range.first,
                thingMeshCache_.overlayLineVertices.begin() + (range.first + range.count),
                lineVertices_.begin() + globalFirst);
            lineDirty.push_back(DrawRange{ globalFirst, range.count });
        }
        return true;
    };
    std::vector<DrawRange> pointDirty{};
    std::vector<DrawRange> spriteDirty{};
    std::unordered_set<std::uint32_t> affectedTriBuilds{};
    std::unordered_map<std::size_t, std::vector<std::size_t>> affectedSpriteBatches{};
    bool spriteTransformsChanged = false;
    affectedTriBuilds.reserve(deltas.size());

    if (thingTriBuildGlobalFirstVertex_.size() != thingMeshCache_.triBuilds.size())
    {
        return fail(ThingTransformFastFallbackReason::triBuildMappingMismatch);
    }

    for (const auto& [pid, d] : deltas)
    {
        // Triangles.
        if (const auto it = thingTriBuildRangesByPickId_.find(pid); it != thingTriBuildRangesByPickId_.end())
        {
            for (const TriBuildRange& r : it->second)
            {
                if (r.buildIndex >= thingMeshCache_.triBuilds.size())
                {
                    return fail(ThingTransformFastFallbackReason::triBuildRangeInvalid);
                }

                TriBuild& b = thingMeshCache_.triBuilds[static_cast<std::size_t>(r.buildIndex)];
                if (r.first < 0 || r.count <= 0 || (static_cast<std::size_t>(r.first + r.count) > b.vertices.size()))
                {
                    return fail(ThingTransformFastFallbackReason::triBuildRangeInvalid);
                }

                // Non-visual thing icon cubes must remain fully lit (sector light recolor is not applied).
                const bool allowRecolor = !(std::string_view(b.material).rfind(kThingIconMatPrefix, 0) == 0);

                for (int i = 0; i < r.count; ++i)
                {
                    const int vi = r.first + i;
                    applyDeltaToTriVertex(b, vi, d, allowRecolor);
                }

                const int globalBase = thingTriBuildGlobalFirstVertex_[static_cast<std::size_t>(r.buildIndex)];
                const int globalFirst = globalBase + r.first;
                if (globalFirst < 0 || globalFirst + r.count > static_cast<int>(triVertices_.size()))
                {
                    return fail(ThingTransformFastFallbackReason::composedBufferRangeInvalid);
                }

                std::copy(
                    b.vertices.begin() + r.first,
                    b.vertices.begin() + (r.first + r.count),
                    triVertices_.begin() + globalFirst);

                triDirty.push_back(DrawRange{ globalFirst, r.count });
                affectedTriBuilds.insert(r.buildIndex);
            }
        }

        // Lines.
        if (const auto it = thingLineRangesByPickId_.find(pid); it != thingLineRangesByPickId_.end())
        {
            for (const DrawRange& r : it->second)
            {
                if (r.first < 0 || r.count <= 0
                    || (static_cast<std::size_t>(r.first + r.count) > thingMeshCache_.lineVertices.size()))
                {
                    return fail(ThingTransformFastFallbackReason::lineRangeInvalid);
                }

                for (int i = 0; i < r.count; ++i)
                {
                    applyDeltaToLineVertex(thingMeshCache_.lineVertices[static_cast<std::size_t>(r.first + i)], d);
                }

                const int globalFirst = thingLineFirstVertex_ + r.first;
                if (globalFirst < 0 || globalFirst + r.count > static_cast<int>(lineVertices_.size()))
                {
                    return fail(ThingTransformFastFallbackReason::composedBufferRangeInvalid);
                }

                std::copy(
                    thingMeshCache_.lineVertices.begin() + r.first,
                    thingMeshCache_.lineVertices.begin() + (r.first + r.count),
                    lineVertices_.begin() + globalFirst);

                lineDirty.push_back(DrawRange{ globalFirst, r.count });
            }
        }

        if (!updateOverlayRanges(thingCollisionOverlayRangesByPickId_, pid, d)
            || !updateOverlayRanges(thingBroadPhaseTreeGuideRangesByPickId_, pid, d)
            || !updateOverlayRanges(thingVisualBoundsOverlayRangesByPickId_, pid, d))
        {
            return fail(ThingTransformFastFallbackReason::lineRangeInvalid);
        }

        // Regenerate only this Thing's six-vertex SPR card. Applying a generic
        // affine to an expanded card is incorrect for type-0 billboards and
        // type-2's engine basis, while rebuilding every sprite caused large
        // editor stalls on rotation or sector-light changes.
        if (const auto ownerIt = spriteInstanceIndicesByPickId_.find(pid);
            ownerIt != spriteInstanceIndicesByPickId_.end())
        {
                if (d.thingIndex >= level_->things.size()
                    || spriteTriRangeByInstanceIndex_.size() != spriteInstances_.size()
                    || spriteTriBatchByInstanceIndex_.size() != spriteInstances_.size()
                    || spriteInstanceBounds_.size() != spriteInstances_.size())
            {
                return fail(ThingTransformFastFallbackReason::triBuildMappingMismatch);
            }

            const Thing& thing = level_->things[d.thingIndex];
            if (d.thingIndex < spriteThingBounds_.size())
            {
                spriteThingBounds_[d.thingIndex] = ThingBounds{};
            }

            for (const std::size_t spriteIndex : ownerIt->second)
            {
                if (spriteIndex >= spriteInstances_.size())
                {
                    return fail(ThingTransformFastFallbackReason::triBuildRangeInvalid);
                }

                SpriteInstance& inst = spriteInstances_[spriteIndex];
                inst.position = QVector3D(
                    static_cast<float>(thing.x),
                    static_cast<float>(thing.y),
                    static_cast<float>(thing.z));
                inst.rollDeg = normalizeRollDeg(static_cast<float>(thing.rol));
                inst.sectorIndex = thing.sector;

                const render::lighting::RuntimeThingLight spriteSelfLight =
                    render::lighting::resolveRuntimeThingLight(
                        *level_, templates_, thing,
                        static_cast<std::int32_t>(d.thingIndex),
                        inst.sectorIndex,
                        render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
                const bool usesRegularSelfLight =
                    (level_->kind == ProjectType::IJIM
                        && spriteSelfLight.classification == render::lighting::ThingLightClass::Flat)
                    || (level_->kind != ProjectType::IJIM
                        && spriteSelfLight.classification == render::lighting::ThingLightClass::Dynamic);
                inst.regularSelfLight = usesRegularSelfLight
                    ? spriteSelfLight.color
                    : QVector3D{};

                TVector pyr{};
                pyr.pitch = thing.pch;
                pyr.yaw = thing.yaw;
                pyr.roll = thing.rol;
                TVector noRoll = pyr;
                noRoll.roll = 0.0;
                const SpriteBasis basis = makeSpriteBasis(noRoll);
                inst.basisRight = basis.right;
                inst.basisForward = basis.forward;
                inst.basisUp = basis.up;
                if (inst.type == 2)
                {
                    inst.basisForward = makeSpriteType2Forward(pyr);
                }

                const DrawRange range = spriteTriRangeByInstanceIndex_[spriteIndex];
                if (range.first < 0 || range.count != 6
                    || range.first + range.count > static_cast<int>(spriteTriVertices_.size()))
                {
                    return fail(ThingTransformFastFallbackReason::triBuildRangeInvalid);
                }

                std::array<TriVertex, 6> vertices{};
                if (!buildSpriteInstanceVertices(inst, vertices))
                {
                    return fail(ThingTransformFastFallbackReason::geometrySignatureChanged);
                }
                std::copy(
                    vertices.begin(), vertices.end(),
                    spriteTriVertices_.begin() + range.first);
                spriteInstanceBounds_[spriteIndex] = ThingBounds{};
                for (const TriVertex& vertex : vertices)
                {
                    render::includePoint(
                        spriteInstanceBounds_[spriteIndex],
                        vertex.x, vertex.y, vertex.z);
                }
                spriteDirty.push_back(range);
                spriteTransformsChanged = true;

                const std::size_t batchIndex = spriteTriBatchByInstanceIndex_[spriteIndex];
                if (batchIndex >= spriteTriBatches_.size())
                {
                    return fail(ThingTransformFastFallbackReason::triBuildMappingMismatch);
                }
                affectedSpriteBatches[batchIndex].push_back(spriteIndex);

                if (d.thingIndex < spriteThingBounds_.size())
                {
                    for (const TriVertex& vertex : vertices)
                    {
                        render::includePoint(
                            spriteThingBounds_[d.thingIndex],
                            vertex.x, vertex.y, vertex.z);
                    }
                }
            }
        }

        // Dots (thing points).
        if (thingViewMode_ == ThingViewMode::Dots)
        {
            if (thingPointIndexByThingIndex_.size() != level_->things.size())
            {
                return fail(ThingTransformFastFallbackReason::pointMappingInvalid);
            }

            if (d.thingIndex >= level_->things.size())
            {
                return fail(ThingTransformFastFallbackReason::pointMappingInvalid);
            }

            const Thing& t = level_->things[d.thingIndex];
            if (!std::isfinite(t.x) || !std::isfinite(t.y) || !std::isfinite(t.z))
            {
                return fail(ThingTransformFastFallbackReason::nonFiniteThingTransform);
            }

            const int pIndex = thingPointIndexByThingIndex_[d.thingIndex];
            if (pIndex < 0)
            {
                return fail(ThingTransformFastFallbackReason::pointMappingInvalid);
            }

            const std::size_t ofs = static_cast<std::size_t>(pIndex) * 3;
            if (ofs + 2 >= thingMeshCache_.pointPositions.size())
            {
                return fail(ThingTransformFastFallbackReason::pointMappingInvalid);
            }

            thingMeshCache_.pointPositions[ofs + 0] = static_cast<float>(t.x);
            thingMeshCache_.pointPositions[ofs + 1] = static_cast<float>(t.y);
            thingMeshCache_.pointPositions[ofs + 2] = static_cast<float>(t.z);

            const std::size_t globalFirstFloat = static_cast<std::size_t>(thingPointFirstVertex_) * 3 + ofs;
            if (globalFirstFloat + 2 >= pointPositions_.size())
            {
                return fail(ThingTransformFastFallbackReason::composedBufferRangeInvalid);
            }

            pointPositions_[globalFirstFloat + 0] = thingMeshCache_.pointPositions[ofs + 0];
            pointPositions_[globalFirstFloat + 1] = thingMeshCache_.pointPositions[ofs + 1];
            pointPositions_[globalFirstFloat + 2] = thingMeshCache_.pointPositions[ofs + 2];

            pointDirty.push_back(DrawRange{ thingPointFirstVertex_ + pIndex, 1 });
        }
    }

    // Interior owners update cached batch extrema in O(changed sprites). If a
    // current extreme owner moved, refitSpriteBatch performs one exact scan of
    // that material batch so translucent sorting remains correct.
    for (const auto& [batchIndex, spriteIndices] : affectedSpriteBatches)
    {
        refitSpriteBatch(batchIndex, &spriteIndices);
    }

    // Update the aggregate bounds without walking every Thing on every paced
    // property scrub. Only a changed owner that supplied an old extreme needs
    // an exact rescan; all other targeted changes can expand the cached union
    // directly in O(changed Things).
    bool rescanThingBounds = !targeted || !thingBoundsExtremeOwnersValid_;
    if (!rescanThingBounds)
    {
        for (const std::size_t thingIndex : targetIndices)
        {
            if (std::find(
                    thingBoundsExtremeOwners_.begin(),
                    thingBoundsExtremeOwners_.end(),
                    thingIndex)
                != thingBoundsExtremeOwners_.end())
            {
                rescanThingBounds = true;
                break;
            }
        }
    }

    QVector3D thingMin = thingMeshCache_.min;
    QVector3D thingMax = thingMeshCache_.max;
    bool thingHaveAny = thingMeshCache_.haveAny;
    if (rescanThingBounds)
    {
        thingMin = QVector3D(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max());
        thingMax = QVector3D(
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max());
        thingHaveAny = false;
        thingBoundsExtremeOwners_.fill(0u);
        for (std::size_t thingIndex = 0;
             thingIndex < thingBounds_.size();
             ++thingIndex)
        {
            const ThingBounds& b = thingBounds_[thingIndex];
            if (!b.valid)
            {
                continue;
            }
            if (!thingHaveAny)
            {
                thingMin = b.min;
                thingMax = b.max;
                thingBoundsExtremeOwners_.fill(thingIndex);
                thingHaveAny = true;
                continue;
            }
            if (b.min.x() < thingMin.x()) { thingMin.setX(b.min.x()); thingBoundsExtremeOwners_[0] = thingIndex; }
            if (b.min.y() < thingMin.y()) { thingMin.setY(b.min.y()); thingBoundsExtremeOwners_[1] = thingIndex; }
            if (b.min.z() < thingMin.z()) { thingMin.setZ(b.min.z()); thingBoundsExtremeOwners_[2] = thingIndex; }
            if (b.max.x() > thingMax.x()) { thingMax.setX(b.max.x()); thingBoundsExtremeOwners_[3] = thingIndex; }
            if (b.max.y() > thingMax.y()) { thingMax.setY(b.max.y()); thingBoundsExtremeOwners_[4] = thingIndex; }
            if (b.max.z() > thingMax.z()) { thingMax.setZ(b.max.z()); thingBoundsExtremeOwners_[5] = thingIndex; }
        }
        thingBoundsExtremeOwnersValid_ = thingHaveAny;
    }
    else
    {
        for (const std::size_t thingIndex : targetIndices)
        {
            const ThingBounds& b = thingBounds_[thingIndex];
            if (!b.valid)
            {
                continue;
            }
            if (!thingHaveAny)
            {
                thingMin = b.min;
                thingMax = b.max;
                thingBoundsExtremeOwners_.fill(thingIndex);
                thingHaveAny = true;
                continue;
            }
            if (b.min.x() < thingMin.x()) { thingMin.setX(b.min.x()); thingBoundsExtremeOwners_[0] = thingIndex; }
            if (b.min.y() < thingMin.y()) { thingMin.setY(b.min.y()); thingBoundsExtremeOwners_[1] = thingIndex; }
            if (b.min.z() < thingMin.z()) { thingMin.setZ(b.min.z()); thingBoundsExtremeOwners_[2] = thingIndex; }
            if (b.max.x() > thingMax.x()) { thingMax.setX(b.max.x()); thingBoundsExtremeOwners_[3] = thingIndex; }
            if (b.max.y() > thingMax.y()) { thingMax.setY(b.max.y()); thingBoundsExtremeOwners_[4] = thingIndex; }
            if (b.max.z() > thingMax.z()) { thingMax.setZ(b.max.z()); thingBoundsExtremeOwners_[5] = thingIndex; }
        }
        thingBoundsExtremeOwnersValid_ = thingHaveAny;
    }

    thingMeshCache_.min = thingMin;
    thingMeshCache_.max = thingMax;
    thingMeshCache_.haveAny = thingHaveAny;

    // Update per-build bounds centers for any builds that contain only one thing (common for translucent face batching).
    auto updateTriBuildBounds = [&deltas](TriBuild& b)
    {
        if (b.vertices.empty())
        {
            return;
        }

        const int pid0 = render::fast_update::pickIdKey(b.vertices.front().pickId);
        const int pid1 = render::fast_update::pickIdKey(b.vertices.back().pickId);
        if (pid0 == pid1)
        {
            if (const auto it = deltas.find(pid0); it != deltas.end())
            {
                render::fast_update::transformBounds(b.boundsMin, b.boundsMax, b.boundsCenter, it->second.xf);
            }
        }
    };
    if (targeted)
    {
        for (const std::uint32_t buildIndex : affectedTriBuilds)
        {
            if (buildIndex < thingMeshCache_.triBuilds.size())
            {
                updateTriBuildBounds(
                    thingMeshCache_.triBuilds[static_cast<std::size_t>(buildIndex)]);
            }
        }
    }
    else
    {
        for (TriBuild& b : thingMeshCache_.triBuilds)
        {
            updateTriBuildBounds(b);
        }
    }

    // Update per-batch bounds centers for any batches that contain only one thing (common for translucent face batching).
    auto updateTriBatchBounds = [&deltas, this](TriBatch& batch)
    {
        if (batch.kind != TriKind::Thing || batch.vertexCount <= 0 || batch.firstVertex < 0)
        {
            return;
        }
        const int first = batch.firstVertex;
        const int end = first + batch.vertexCount;
        if (end > static_cast<int>(triVertices_.size()))
        {
            return;
        }

        const int pid0 = render::fast_update::pickIdKey(triVertices_[static_cast<std::size_t>(first)].pickId);
        const int pid1 = render::fast_update::pickIdKey(triVertices_[static_cast<std::size_t>(end - 1)].pickId);
        if (pid0 != pid1)
        {
            return;
        }
        if (const auto it = deltas.find(pid0); it != deltas.end())
        {
            render::fast_update::transformBounds(batch.boundsMin, batch.boundsMax, batch.boundsCenter, it->second.xf);
        }
    };
    if (targeted)
    {
        for (const std::uint32_t buildIndex : affectedTriBuilds)
        {
            if (buildIndex >= thingTriBuildGlobalFirstVertex_.size())
            {
                continue;
            }
            const int firstVertex = thingTriBuildGlobalFirstVertex_[buildIndex];
            const auto batchIt = std::lower_bound(
                triBatches_.begin(),
                triBatches_.end(),
                firstVertex,
                [](const TriBatch& batch, const int first)
                {
                    return batch.firstVertex < first;
                });
            if (batchIt != triBatches_.end()
                && batchIt->kind == TriKind::Thing
                && batchIt->firstVertex == firstVertex)
            {
                updateTriBatchBounds(*batchIt);
            }
        }
    }
    else
    {
        for (TriBatch& batch : triBatches_)
        {
            updateTriBatchBounds(batch);
        }
    }

    // Update camera bounds center/radius in the same way rebuildMeshes() does.
    QVector3D minv(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    QVector3D maxv(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    bool haveAny = false;

    if (levelMeshCache_.valid && levelMeshCache_.haveAny)
    {
        render::fast_update::includeBounds(minv, maxv, haveAny, levelMeshCache_.min, levelMeshCache_.max);
    }
    if (thingMeshCache_.valid && thingMeshCache_.haveAny)
    {
        render::fast_update::includeBounds(minv, maxv, haveAny, thingMeshCache_.min, thingMeshCache_.max);
    }

    if (haveAny)
    {
        camera_.orbit.center = (minv + maxv) * 0.5f;
        camera_.orbit.radius = std::max((maxv - minv).length() * 0.5f, 1.0f);
    }

    // Avoid a full rebuildSelectionOverlays() here: selection overlays can include a full triangulation
    // of a selected thing's model for stencil outlining, which is expensive and unnecessary for pure
    // thing transforms. Instead, transform the existing overlay geometry in-place when it belongs to
    // moved/rotated things.
    if (!deltas.empty())
    {
        // Highlight lines for things (dots/boxes/wireframe highlight) carry the thing pickId.
        if (!highlightLineVertices_.empty())
        {
            for (LineVertex& v : highlightLineVertices_)
            {
                const int pid = render::fast_update::pickIdKey(v.pickId);
                if (pid <= 0)
                {
                    continue;
                }

                if (const auto it = deltas.find(pid); it != deltas.end())
                {
                    applyDeltaToLineVertex(v, it->second);
                }
            }
        }

        // Shaded-mode thing outline triangles are stored in world space; update them if the outlined thing moved.
        if (outlinedThing_ >= 0 && !outlineTriVertices_.empty()
            && static_cast<std::size_t>(outlinedThing_) < thingPickIds_.size())
        {
            const int pid = static_cast<int>(thingPickIds_[static_cast<std::size_t>(outlinedThing_)]);
            if (pid > 0)
            {
                if (const auto it = deltas.find(pid); it != deltas.end() && it->second.xformChanged)
                {
                    const render::fast_update::ThingTransformDelta& d = it->second;

                    for (TriVertex& v : outlineTriVertices_)
                    {
                        const QVector3D p(v.x, v.y, v.z);
                        const QVector3D q = d.xf.mapPoint(p);
                        v.x = q.x();
                        v.y = q.y();
                        v.z = q.z();
                    }

                    outlinedThingCenter_ = d.xf.mapPoint(outlinedThingCenter_);
                }
            }
        }
    }

    if (glReady_)
    {
        makeCurrent();

        bool fallbackUpload = false;

        render::mergeDrawRanges(triDirty);
        render::mergeDrawRanges(lineDirty);
        render::mergeDrawRanges(pointDirty);

        if (!triDirty.empty())
        {
            if (!triVbo_ || triVboAllocatedBytes_ <= 0 || triVertexCount_ != static_cast<int>(triVertices_.size()))
            {
                fallbackUpload = true;
            }
            else
            {
                triVbo_->bind();
                for (const DrawRange& r : triDirty)
                {
                    if (r.first < 0 || r.count <= 0)
                    {
                        continue;
                    }
                    const int offsetBytes = r.first * static_cast<int>(sizeof(TriVertex));
                    const int bytes = r.count * static_cast<int>(sizeof(TriVertex));
                    if (offsetBytes < 0 || offsetBytes + bytes > triVboAllocatedBytes_
                        || (r.first + r.count) > static_cast<int>(triVertices_.size()))
                    {
                        fallbackUpload = true;
                        break;
                    }
                    triVbo_->write(offsetBytes, triVertices_.data() + r.first, bytes);
                }
                triVbo_->release();
            }
        }

        if (!fallbackUpload && !lineDirty.empty())
        {
            if (!lineVbo_ || lineVboAllocatedBytes_ <= 0
                || wireLineVertexCount_ + thingLineVertexCount_ + thingOverlayLineVertexCount_
                    != static_cast<int>(lineVertices_.size()))
            {
                fallbackUpload = true;
            }
            else
            {
                lineVbo_->bind();
                for (const DrawRange& r : lineDirty)
                {
                    if (r.first < 0 || r.count <= 0)
                    {
                        continue;
                    }
                    const int offsetBytes = r.first * static_cast<int>(sizeof(LineVertex));
                    const int bytes = r.count * static_cast<int>(sizeof(LineVertex));
                    if (offsetBytes < 0 || offsetBytes + bytes > lineVboAllocatedBytes_
                        || (r.first + r.count) > static_cast<int>(lineVertices_.size()))
                    {
                        fallbackUpload = true;
                        break;
                    }
                    lineVbo_->write(offsetBytes, lineVertices_.data() + r.first, bytes);
                }
                lineVbo_->release();
            }
        }

        if (!fallbackUpload && !pointDirty.empty())
        {
            if (!pointVbo_ || pointVboAllocatedBytes_ <= 0 || (pointVertexCount_ * 3) != static_cast<int>(pointPositions_.size()))
            {
                fallbackUpload = true;
            }
            else
            {
                pointVbo_->bind();
                for (const DrawRange& r : pointDirty)
                {
                    if (r.first < 0 || r.count <= 0)
                    {
                        continue;
                    }
                    const int firstFloat = r.first * 3;
                    const int floatCount = r.count * 3;
                    const int offsetBytes = firstFloat * static_cast<int>(sizeof(float));
                    const int bytes = floatCount * static_cast<int>(sizeof(float));
                    if (offsetBytes < 0 || offsetBytes + bytes > pointVboAllocatedBytes_
                        || (firstFloat + floatCount) > static_cast<int>(pointPositions_.size()))
                    {
                        fallbackUpload = true;
                        break;
                    }
                    pointVbo_->write(offsetBytes, pointPositions_.data() + firstFloat, bytes);
                }
                pointVbo_->release();
            }
        }

        if (!fallbackUpload && spriteTransformsChanged)
        {
            if (!spriteTriVao_ || !spriteTriVbo_)
            {
                fallbackUpload = true;
            }
            else
            {
                spriteTriVao_->bind();
                spriteTriVbo_->bind();
                if (spriteDirty.empty())
                {
                    spriteTrianglesDirty_ = false;
                }
                else
                {
                    render::mergeDrawRanges(spriteDirty);
                    if (!spriteTriVbo_ || spriteTriVboAllocatedBytes_ <= 0
                        || spriteTriVertexCount_ != static_cast<int>(spriteTriVertices_.size()))
                    {
                        fallbackUpload = true;
                    }
                    else
                    {
                        for (const DrawRange& r : spriteDirty)
                        {
                            const int offsetBytes = r.first * static_cast<int>(sizeof(TriVertex));
                            const int bytes = r.count * static_cast<int>(sizeof(TriVertex));
                            if (r.first < 0 || r.count <= 0
                                || offsetBytes < 0
                                || offsetBytes + bytes > spriteTriVboAllocatedBytes_
                                || r.first + r.count > static_cast<int>(spriteTriVertices_.size()))
                            {
                                fallbackUpload = true;
                                break;
                            }
                            spriteTriVbo_->write(
                                offsetBytes,
                                spriteTriVertices_.data() + r.first,
                                bytes);
                        }
                    }
                }
                if (!fallbackUpload)
                {
                    spriteTrianglesDirty_ = false;
                }
                spriteTriVbo_->release();
                spriteTriVao_->release();
            }
        }

        // If any buffer write couldn't be performed safely, fall back to a full upload.
        if (fallbackUpload)
        {
            uploadMeshes();
        }
        else
        {
            uploadSelectionOverlays();
        }

        doneCurrent();
    }

    return true;
}

std::uint64_t GlView::placementPreviewLightingSignature(const Thing& thing) const
{
    std::uint64_t signature = kFnv1aOffset64;
    signature = hashAppendPod(signature, thing.sector);
    signature = hashAppendPod(signature, thingRuntimeLightSignature(thing));

    const ThingSectorLighting lighting = calcThingSectorLighting(level_, thing.sector);
    signature = hashAppendPod(signature, lighting.ambientLight.r);
    signature = hashAppendPod(signature, lighting.ambientLight.g);
    signature = hashAppendPod(signature, lighting.ambientLight.b);
    signature = hashAppendPod(signature, lighting.ambientLight.a);

    const QVector3D editorTint =
        render::classicEditorSectorTintScale(level_, thing.sector);
    signature = hashAppendPod(signature, editorTint.x());
    signature = hashAppendPod(signature, editorTint.y());
    signature = hashAppendPod(signature, editorTint.z());

    if (level_
        && level_->kind == ProjectType::IJIM
        && thing.sector >= 0
        && static_cast<std::size_t>(thing.sector) < level_->sectors.size())
    {
        const render::lighting::RuntimeSectorPointLight point =
            render::lighting::resolveRuntimeSectorPointLight(
                level_->sectors[static_cast<std::size_t>(thing.sector)].pointLight);
        signature = hashAppendPod(signature, point.position.x());
        signature = hashAppendPod(signature, point.position.y());
        signature = hashAppendPod(signature, point.position.z());
        signature = hashAppendPod(signature, point.color.x());
        signature = hashAppendPod(signature, point.color.y());
        signature = hashAppendPod(signature, point.color.z());
        signature = hashAppendPod(signature, point.minRadius);
        signature = hashAppendPod(signature, point.maxRadius);
    }
    return signature;
}

bool GlView::tryFastUpdatePlacementPreviewThings(
    const std::vector<Thing>& things,
    const bool invalid)
{
    if (!level_
        || !thingMeshCache_.valid
        || !placementPreviewRenderStateValid_
        || placementPreviewRenderedInvalid_ != invalid
        || things.size() != placementPreviewRenderedThings_.size()
        || things.size() != placementPreviewRenderRanges_.size()
        || things.size() != placementPreviewLodStates_.size()
        || things.size() != placementPreviewLightingSignatures_.size())
    {
        return false;
    }

    if (thingTriBuildGlobalFirstVertex_.size() != thingMeshCache_.triBuilds.size())
    {
        return false;
    }

    // An affine patch cannot change a preview model's topology. Use the
    // already-published model geoset counts, so ordinary cursor samples do
    // not resolve a template, model or resource. A threshold crossing falls
    // through to the existing one-shot ThingsOnly rebuild.
    if (thingModelLodEnabled_ && level_->kind != ProjectType::IJIM)
    {
        const ThingLodView lodView = thingLodView();
        for (std::size_t i = 0; i < things.size(); ++i)
        {
            const PlacementPreviewLodState& state =
                placementPreviewLodStates_[i];
            if (state.geosetCount > 1u && !state.archThing
                && selectedThingModelLod(
                    state.geosetCount, things[i], lodView)
                    != state.requestedLodIndex)
            {
                return false;
            }
        }
    }

    // Sprite material sorting invalidates construction-time vector offsets.
    // rebuildSpriteVertices publishes a stable owner-to-index map once, so
    // cursor motion touches only the transient preview sprites rather than
    // scanning every persistent sprite in a large level.
    if (placementPreviewSpriteIndices_.size() != things.size())
    {
        return false;
    }
    if (spriteTriRangeByInstanceIndex_.size() != spriteInstances_.size()
        || spriteTriBatchByInstanceIndex_.size() != spriteInstances_.size())
    {
        return false;
    }
    const auto& previewSpriteIndices = placementPreviewSpriteIndices_;
    for (std::size_t i = 0; i < things.size(); ++i)
    {
        if (previewSpriteIndices[i].size() != placementPreviewRenderRanges_[i].spriteCount)
        {
            return false;
        }
    }

    struct PreviewDelta
    {
        render::fast_update::Affine3f xf{};
        ColorF ambientLight{ ColorWhite };
        PointLight sectorPointLight{};
        std::int32_t sectorLightOwner{ -1 };
        QVector3D editorSectorTint{ 1.0f, 1.0f, 1.0f };
        bool xformChanged{};
        bool sectorChanged{};
        bool lightingChanged{};
    };
    std::vector<PreviewDelta> deltas(things.size());
    std::vector<std::uint64_t> currentLightingSignatures(things.size());
    bool anyChanged = false;

    for (std::size_t i = 0; i < things.size(); ++i)
    {
        const Thing& before = placementPreviewRenderedThings_[i];
        const Thing& after = things[i];
        const PlacementPreviewRenderRange& range = placementPreviewRenderRanges_[i];

        if (thingGeometrySignature(before) != thingGeometrySignature(after)
            || range.triBuildFirst + range.triBuildCount > thingMeshCache_.triBuilds.size()
            || range.lineFirst + range.lineCount > thingMeshCache_.lineVertices.size())
        {
            return false;
        }

        if (!std::isfinite(before.x) || !std::isfinite(before.y) || !std::isfinite(before.z)
            || !std::isfinite(before.pch) || !std::isfinite(before.yaw) || !std::isfinite(before.rol)
            || !std::isfinite(after.x) || !std::isfinite(after.y) || !std::isfinite(after.z)
            || !std::isfinite(after.pch) || !std::isfinite(after.yaw) || !std::isfinite(after.rol))
        {
            return false;
        }

        const bool xformChanged =
            !render::fast_update::nearlySameTransform(before.x, after.x)
            || !render::fast_update::nearlySameTransform(before.y, after.y)
            || !render::fast_update::nearlySameTransform(before.z, after.z)
            || !render::fast_update::nearlySameTransform(before.pch, after.pch)
            || !render::fast_update::nearlySameTransform(before.yaw, after.yaw)
            || !render::fast_update::nearlySameTransform(before.rol, after.rol);
        PreviewDelta& delta = deltas[i];
        delta.xformChanged = xformChanged;
        delta.sectorChanged = before.sector != after.sector;
        currentLightingSignatures[i] = placementPreviewLightingSignature(after);
        delta.lightingChanged =
            currentLightingSignatures[i] != placementPreviewLightingSignatures_[i];
        anyChanged = anyChanged
            || delta.xformChanged
            || delta.sectorChanged
            || delta.lightingChanged;
        if (!delta.xformChanged && !delta.sectorChanged && !delta.lightingChanged)
        {
            continue;
        }

        if (delta.xformChanged)
        {
            TVector oldPyr{};
            oldPyr.pitch = before.pch;
            oldPyr.yaw = before.yaw;
            oldPyr.roll = before.rol;
            TVector newPyr{};
            newPyr.pitch = after.pch;
            newPyr.yaw = after.yaw;
            newPyr.roll = after.rol;

            const QMatrix4x4 oldQt = matrix43ToQt(
                TMatrix43::Create(TVector{ before.x, before.y, before.z }, oldPyr));
            bool invertOk = false;
            const QMatrix4x4 inverseOld = oldQt.inverted(&invertOk);
            if (!invertOk)
            {
                return false;
            }
            const QMatrix4x4 newQt = matrix43ToQt(
                TMatrix43::Create(TVector{ after.x, after.y, after.z }, newPyr));
            delta.xf = render::fast_update::affineFromMatrix(newQt * inverseOld);
        }

        if (delta.sectorChanged || delta.lightingChanged)
        {
            const ThingSectorLighting lights = calcThingSectorLighting(level_, after.sector);
            delta.ambientLight = lights.ambientLight;
            const render::lighting::RuntimeThingLight selfLight =
                render::lighting::resolveRuntimeThingLight(
                    *level_,
                    templates_,
                    after,
                    -1,
                    after.sector,
                    render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
            if constexpr (render::lighting::kRuntimeDynamicLightPreviewEnabled)
            {
                delta.ambientLight = render::lighting::applyRuntimeThingSelfLight(
                    level_->kind,
                    delta.ambientLight,
                    selfLight,
                    thingLightRenderingEnabled_);
            }

            if (!placementPreviewRenderedInvalid_)
            {
                delta.editorSectorTint =
                    render::classicEditorSectorTintScale(level_, after.sector);
            }

            if (level_->kind == ProjectType::IJIM
                && after.sector >= 0
                && static_cast<std::size_t>(after.sector) < level_->sectors.size())
            {
                const render::lighting::RuntimeSectorPointLight runtime =
                    render::lighting::resolveRuntimeSectorPointLight(
                        level_->sectors[static_cast<std::size_t>(after.sector)].pointLight);
                delta.sectorPointLight.position = TVector{
                    runtime.position.x(), runtime.position.y(), runtime.position.z() };
                delta.sectorPointLight.color = ColorF{
                    runtime.color.x(), runtime.color.y(), runtime.color.z(), 1.0f };
                delta.sectorPointLight.minRange = runtime.minRadius;
                delta.sectorPointLight.maxRange = runtime.maxRadius;
                delta.sectorLightOwner = after.sector;
            }
        }
    }

    if (!anyChanged)
    {
        placementPreviewRenderedThings_ = things;
        placementPreviewLightingSignatures_ = std::move(currentLightingSignatures);
        ++placementPreviewFastUpdateCount_;
        return true;
    }

    auto transformTriVertex = [](TriVertex& vertex, const render::fast_update::Affine3f& xf)
    {
        QVector3D position(vertex.x, vertex.y, vertex.z);
        QVector3D normal(vertex.normalX, vertex.normalY, vertex.normalZ);
        QVector3D faceAnchor(
            vertex.faceLightAnchorX,
            vertex.faceLightAnchorY,
            vertex.faceLightAnchorZ);
        render::fast_update::transformDynamicLightGeometry(
            position,
            normal,
            vertex.dynamicLightTarget > 2.5f ? &faceAnchor : nullptr,
            xf);
        vertex.x = position.x();
        vertex.y = position.y();
        vertex.z = position.z();
        vertex.normalX = normal.x();
        vertex.normalY = normal.y();
        vertex.normalZ = normal.z();
        if (vertex.dynamicLightTarget > 2.5f)
        {
            vertex.faceLightAnchorX = faceAnchor.x();
            vertex.faceLightAnchorY = faceAnchor.y();
            vertex.faceLightAnchorZ = faceAnchor.z();
        }
    };

    auto transformLineVertex = [](LineVertex& vertex, const render::fast_update::Affine3f& xf)
    {
        const QVector3D point = xf.mapPoint(QVector3D(vertex.x, vertex.y, vertex.z));
        vertex.x = point.x();
        vertex.y = point.y();
        vertex.z = point.z();
        if (vertex.planeValid > 0.5f)
        {
            const QVector3D plane = xf.mapPoint(
                QVector3D(vertex.planeX, vertex.planeY, vertex.planeZ));
            vertex.planeX = plane.x();
            vertex.planeY = plane.y();
            vertex.planeZ = plane.z();
        }
    };

    std::vector<DrawRange> triDirty{};
    std::vector<DrawRange> lineDirty{};
    std::vector<DrawRange> spriteDirty{};
    std::unordered_map<std::size_t, std::vector<std::size_t>> affectedSpriteBatches{};
    bool spriteChanged = false;
    const auto maxLightMode =
        static_cast<TLightMode>(static_cast<std::int32_t>(lightingMode_));

    for (std::size_t i = 0; i < things.size(); ++i)
    {
        const PreviewDelta& delta = deltas[i];
        if (!delta.xformChanged && !delta.sectorChanged && !delta.lightingChanged)
        {
            continue;
        }

        const PlacementPreviewRenderRange& range = placementPreviewRenderRanges_[i];
        for (std::size_t buildIndex = range.triBuildFirst;
             buildIndex < range.triBuildFirst + range.triBuildCount;
             ++buildIndex)
        {
            TriBuild& build = thingMeshCache_.triBuilds[buildIndex];
            for (std::size_t vertexIndex = 0; vertexIndex < build.vertices.size(); ++vertexIndex)
            {
                TriVertex& vertex = build.vertices[vertexIndex];
                if (delta.xformChanged)
                {
                    transformTriVertex(vertex, delta.xf);
                }
                if (delta.sectorChanged || delta.lightingChanged)
                {
                    vertex.editorSectorTintR = delta.editorSectorTint.x();
                    vertex.editorSectorTintG = delta.editorSectorTint.y();
                    vertex.editorSectorTintB = delta.editorSectorTint.z();

                    if (level_->kind == ProjectType::IJIM
                        && vertex.dynamicLightTarget > 1.5f)
                    {
                        vertex.sectorLightX = static_cast<float>(delta.sectorPointLight.position.x);
                        vertex.sectorLightY = static_cast<float>(delta.sectorPointLight.position.y);
                        vertex.sectorLightZ = static_cast<float>(delta.sectorPointLight.position.z);
                        vertex.sectorLightMinRadius = static_cast<float>(delta.sectorPointLight.minRange);
                        vertex.sectorLightR = delta.sectorPointLight.color.r;
                        vertex.sectorLightG = delta.sectorPointLight.color.g;
                        vertex.sectorLightB = delta.sectorPointLight.color.b;
                        vertex.sectorLightMaxRadius = static_cast<float>(delta.sectorPointLight.maxRange);
                        vertex.sectorLightOwner = static_cast<float>(delta.sectorLightOwner);
                    }

                    if (!placementPreviewRenderedInvalid_
                        && vertexIndex < build.shadeInputs.size())
                    {
                        const TriShadeInput& shade = build.shadeInputs[vertexIndex];
                        const ColorF lit = polyCalcLitColor(
                            shade.intensity,
                            shade.lightMode,
                            delta.ambientLight,
                            shade.extraLight,
                            level_->kind,
                            maxLightMode,
                            vertex.dynamicLightTarget > 1.5f);
                        vertex.r = lit.r;
                        vertex.g = lit.g;
                        vertex.b = lit.b;
                        if (level_->kind != ProjectType::IJIM
                            && vertex.dynamicLightTarget > 1.5f)
                        {
                            vertex.sectorLightX = shade.extraLight.r;
                            vertex.sectorLightY = shade.extraLight.g;
                            vertex.sectorLightZ = shade.extraLight.b;
                            vertex.sectorLightR = delta.ambientLight.r;
                            vertex.sectorLightG = delta.ambientLight.g;
                            vertex.sectorLightB = delta.ambientLight.b;
                        }
                    }
                }
            }
            if (delta.xformChanged)
            {
                render::fast_update::transformBounds(
                    build.boundsMin, build.boundsMax, build.boundsCenter, delta.xf);
            }

            const int globalFirst = thingTriBuildGlobalFirstVertex_[buildIndex];
            const int count = static_cast<int>(build.vertices.size());
            if (globalFirst < 0 || count < 0
                || globalFirst + count > static_cast<int>(triVertices_.size()))
            {
                return false;
            }
            std::copy(build.vertices.begin(), build.vertices.end(), triVertices_.begin() + globalFirst);
            if (count > 0)
            {
                triDirty.push_back(DrawRange{ globalFirst, count });
            }

            const std::size_t batchIndex = levelMeshCache_.triBuilds.size() + buildIndex;
            if (batchIndex >= triBatches_.size())
            {
                return false;
            }
            TriBatch& batch = triBatches_[batchIndex];
            if (delta.xformChanged)
            {
                render::fast_update::transformBounds(
                    batch.boundsMin, batch.boundsMax, batch.boundsCenter, delta.xf);
            }
        }

        for (std::size_t lineIndex = range.lineFirst;
             lineIndex < range.lineFirst + range.lineCount;
             ++lineIndex)
        {
            if (delta.xformChanged)
            {
                transformLineVertex(thingMeshCache_.lineVertices[lineIndex], delta.xf);
            }
        }
        if (delta.xformChanged && range.lineCount > 0)
        {
            const int globalFirst = thingLineFirstVertex_ + static_cast<int>(range.lineFirst);
            const int count = static_cast<int>(range.lineCount);
            if (globalFirst < 0 || globalFirst + count > static_cast<int>(lineVertices_.size()))
            {
                return false;
            }
            std::copy(
                thingMeshCache_.lineVertices.begin() + static_cast<std::ptrdiff_t>(range.lineFirst),
                thingMeshCache_.lineVertices.begin() + static_cast<std::ptrdiff_t>(range.lineFirst + range.lineCount),
                lineVertices_.begin() + globalFirst);
            lineDirty.push_back(DrawRange{ globalFirst, count });
        }

        const Thing& thing = things[i];
        for (const std::size_t spriteIndex : previewSpriteIndices[i])
        {
            if (spriteIndex >= spriteInstances_.size())
            {
                return false;
            }
            SpriteInstance& instance = spriteInstances_[spriteIndex];
            instance.position = QVector3D(
                static_cast<float>(thing.x),
                static_cast<float>(thing.y),
                static_cast<float>(thing.z));
            instance.rollDeg = normalizeRollDeg(static_cast<float>(thing.rol));
            instance.sectorIndex = thing.sector;

            TVector pyr{};
            pyr.pitch = thing.pch;
            pyr.yaw = thing.yaw;
            pyr.roll = thing.rol;
            TVector noRoll = pyr;
            noRoll.roll = 0.0;
            const SpriteBasis basis = makeSpriteBasis(noRoll);
            instance.basisRight = basis.right;
            instance.basisForward = basis.forward;
            instance.basisUp = basis.up;
            if (instance.type == 2)
            {
                instance.basisForward = makeSpriteType2Forward(pyr);
            }

            // Sprite lighting is generated from the cached instance rather
            // than TriBuild shade inputs. Keep its authored self-light in
            // sync when a preview's light/typeflags value changes.
            if (delta.lightingChanged)
            {
                instance.regularSelfLight = QVector3D{};
                const render::lighting::RuntimeThingLight selfLight =
                    render::lighting::resolveRuntimeThingLight(
                        *level_,
                        templates_,
                        thing,
                        -1,
                        std::nullopt,
                        render::lighting::ThingLightOwnerPolicy::AuthoringPreview);
                if ((level_->kind == ProjectType::IJIM
                        && selfLight.classification == render::lighting::ThingLightClass::Flat)
                    || (level_->kind != ProjectType::IJIM
                        && selfLight.classification == render::lighting::ThingLightClass::Dynamic))
                {
                    instance.regularSelfLight = selfLight.color;
                }
            }

            const DrawRange spriteRange = spriteTriRangeByInstanceIndex_[spriteIndex];
            if (spriteRange.first < 0 || spriteRange.count != 6
                || spriteRange.first + spriteRange.count
                    > static_cast<int>(spriteTriVertices_.size()))
            {
                return false;
            }
            std::array<TriVertex, 6> vertices{};
            if (!buildSpriteInstanceVertices(instance, vertices))
            {
                return false;
            }
            std::copy(
                vertices.begin(), vertices.end(),
                spriteTriVertices_.begin() + spriteRange.first);
            if (spriteIndex >= spriteInstanceBounds_.size())
            {
                return false;
            }
            spriteInstanceBounds_[spriteIndex] = ThingBounds{};
            for (const TriVertex& vertex : vertices)
            {
                render::includePoint(
                    spriteInstanceBounds_[spriteIndex],
                    vertex.x, vertex.y, vertex.z);
            }
            spriteDirty.push_back(spriteRange);

            const std::size_t batchIndex = spriteTriBatchByInstanceIndex_[spriteIndex];
            if (batchIndex >= spriteTriBatches_.size())
            {
                return false;
            }
            affectedSpriteBatches[batchIndex].push_back(spriteIndex);
            spriteChanged = true;
        }
    }

    for (const auto& [batchIndex, spriteIndices] : affectedSpriteBatches)
    {
        refitSpriteBatch(batchIndex, &spriteIndices);
    }

    placementPreviewRenderedThings_ = things;
    placementPreviewLightingSignatures_ = std::move(currentLightingSignatures);
    ++placementPreviewFastUpdateCount_;

    if (!glReady_)
    {
        return true;
    }

    makeCurrent();
    bool fallbackUpload = false;
    render::mergeDrawRanges(triDirty);
    render::mergeDrawRanges(lineDirty);
    render::mergeDrawRanges(spriteDirty);

    if (!triDirty.empty())
    {
        if (!triVbo_ || triVboAllocatedBytes_ <= 0
            || triVertexCount_ != static_cast<int>(triVertices_.size()))
        {
            fallbackUpload = true;
        }
        else
        {
            triVbo_->bind();
            for (const DrawRange& range : triDirty)
            {
                const int offsetBytes = range.first * static_cast<int>(sizeof(TriVertex));
                const int bytes = range.count * static_cast<int>(sizeof(TriVertex));
                if (offsetBytes < 0 || offsetBytes + bytes > triVboAllocatedBytes_)
                {
                    fallbackUpload = true;
                    break;
                }
                triVbo_->write(offsetBytes, triVertices_.data() + range.first, bytes);
            }
            triVbo_->release();
        }
    }

    if (!fallbackUpload && !lineDirty.empty())
    {
        if (!lineVbo_ || lineVboAllocatedBytes_ <= 0
            || wireLineVertexCount_ + thingLineVertexCount_ + thingOverlayLineVertexCount_ != static_cast<int>(lineVertices_.size()))
        {
            fallbackUpload = true;
        }
        else
        {
            lineVbo_->bind();
            for (const DrawRange& range : lineDirty)
            {
                const int offsetBytes = range.first * static_cast<int>(sizeof(LineVertex));
                const int bytes = range.count * static_cast<int>(sizeof(LineVertex));
                if (offsetBytes < 0 || offsetBytes + bytes > lineVboAllocatedBytes_)
                {
                    fallbackUpload = true;
                    break;
                }
                lineVbo_->write(offsetBytes, lineVertices_.data() + range.first, bytes);
            }
            lineVbo_->release();
        }
    }

    if (!fallbackUpload && spriteChanged)
    {
        if (!spriteTriVao_ || !spriteTriVbo_)
        {
            fallbackUpload = true;
        }
        else
        {
            spriteTriVao_->bind();
            spriteTriVbo_->bind();
            if (spriteTriVboAllocatedBytes_ <= 0
                || spriteTriVertexCount_ != static_cast<int>(spriteTriVertices_.size()))
            {
                fallbackUpload = true;
            }
            else
            {
                for (const DrawRange& range : spriteDirty)
                {
                    const int offsetBytes =
                        range.first * static_cast<int>(sizeof(TriVertex));
                    const int bytes =
                        range.count * static_cast<int>(sizeof(TriVertex));
                    if (range.first < 0 || range.count <= 0
                        || offsetBytes < 0
                        || offsetBytes + bytes > spriteTriVboAllocatedBytes_
                        || range.first + range.count
                            > static_cast<int>(spriteTriVertices_.size()))
                    {
                        fallbackUpload = true;
                        break;
                    }
                    spriteTriVbo_->write(
                        offsetBytes,
                        spriteTriVertices_.data() + range.first,
                        bytes);
                }
            }
            if (!fallbackUpload) spriteTrianglesDirty_ = false;
            spriteTriVbo_->release();
            spriteTriVao_->release();
        }
    }

    if (fallbackUpload)
    {
        uploadMeshes();
    }
    doneCurrent();
    return true;
}

} // namespace sed
