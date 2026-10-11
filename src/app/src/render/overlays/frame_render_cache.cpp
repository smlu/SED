#include "render/gl_view_common.hpp"

#include "render/overlays/frame_pose_cache_utils.hpp"
#include "sed/level.hpp"
#include "sed/text.hpp"

#include <algorithm>
#include <limits>
#include <new>

namespace sed
{

GlView::FrameOwnerPoseCache* GlView::ensureFrameOwnerPoseCache(
    const std::int32_t thingIndex)
{
    if ( !level_ || thingIndex < 0
        || static_cast<std::size_t>(thingIndex) >= level_->things.size() )
    {
        return nullptr;
    }

    auto found = frameOwnerPoseCaches_.find(thingIndex);
    if ( found == frameOwnerPoseCaches_.end() )
    {
        while ( frameOwnerPoseCaches_.size() >= kMaximumFramePoseOwnerCaches )
        {
            auto oldest = frameOwnerPoseCaches_.end();
            for ( auto candidate = frameOwnerPoseCaches_.begin();
                candidate != frameOwnerPoseCaches_.end();
                ++candidate )
            {
                if ( oldest == frameOwnerPoseCaches_.end()
                    || candidate->second.lastUseOrdinal < oldest->second.lastUseOrdinal )
                {
                    oldest = candidate;
                }
            }
            if ( oldest == frameOwnerPoseCaches_.end() )
            {
                return nullptr;
            }

            const std::int32_t evictedOwner = oldest->first;
            if ( oldest->second.poseBytes <= framePoseCacheBytes_ )
            {
                framePoseCacheBytes_ -= oldest->second.poseBytes;
            }
            std::vector<render::frame_ghost::FramePose>{}.swap(
                oldest->second.poses);
            oldest->second.overflowFramePose.reset();
            std::vector<FrameGhostInstance>{}.swap(
                oldest->second.ghostInstances);
            for ( std::size_t slotIndex = 0u;
                slotIndex < frameGhostInstanceBufferSlots_.size();
                ++slotIndex )
            {
                if ( frameGhostInstanceBufferSlots_[slotIndex].ownerThingIndex
                    == evictedOwner )
                {
                    frameGhostInstanceBufferSlots_[slotIndex].ownerThingIndex = -1;
                }
            }
            frameOwnerPoseCaches_.erase(oldest);
        }

        try
        {
            found = frameOwnerPoseCaches_.try_emplace(thingIndex).first;
        }
        catch ( const std::bad_alloc& )
        {
            return nullptr;
        }
    }

    FrameOwnerPoseCache& ownerCache = found->second;
    ownerCache.lastUseOrdinal = ++frameRenderCacheUseOrdinal_;
    if ( ownerCache.posesValid )
    {
        return &ownerCache;
    }

    render::frame_ghost::ParsedFramePoses parsed{};
    try
    {
        parsed = render::frame_ghost::parseFramePoses(
            level_->things[static_cast<std::size_t>(thingIndex)]);
    }
    catch ( const std::bad_alloc& )
    {
        ownerCache.posesValid = true;
        ownerCache.posesComplete = false;
        ownerCache.ghostInstancesDirty = true;
        return &ownerCache;
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    frameOverlayWorkForTest_.frameValueParseCalls += parsed.parseCalls;
    frameOverlayWorkForTest_.validFramePosesParsed += parsed.poses.size();
#endif

    std::vector<FrameGhostInstance> instances{};
    try
    {
        instances.reserve(parsed.poses.size());
        for ( const render::frame_ghost::FramePose& pose : parsed.poses )
        {
            const auto matrix = render::frame_ghost::framePoseMatrix(pose);
            if ( !matrix )
            {
                continue;
            }
            instances.push_back(FrameGhostInstance{ *matrix, pose.valueIndex });
        }
    }
    catch ( const std::bad_alloc& )
    {
        std::vector<FrameGhostInstance>{}.swap(instances);
        parsed.complete = false;
    }

    const auto allocationBytes = [](
        const std::size_t capacity,
        const std::size_t elementBytes) noexcept
    {
        if ( capacity > std::numeric_limits<std::size_t>::max() / elementBytes )
        {
            return std::numeric_limits<std::size_t>::max();
        }
        return capacity * elementBytes;
    };
    const auto requiredCacheBytes = [&allocationBytes](
        const std::vector<render::frame_ghost::FramePose>& poses,
        const std::vector<FrameGhostInstance>& frameInstances) noexcept
    {
        const std::size_t poseBytes = allocationBytes(
            poses.capacity(),
            sizeof(render::frame_ghost::FramePose));
        const std::size_t instanceBytes = allocationBytes(
            frameInstances.capacity(),
            sizeof(FrameGhostInstance));
        if ( instanceBytes > std::numeric_limits<std::size_t>::max() - poseBytes )
        {
            return std::numeric_limits<std::size_t>::max();
        }
        return poseBytes + instanceBytes;
    };
    std::size_t requiredBytes = requiredCacheBytes(parsed.poses, instances);
    if ( requiredBytes > kMaximumFramePoseCacheBytes )
    {
        std::vector<render::frame_ghost::FramePose>{}.swap(parsed.poses);
        std::vector<FrameGhostInstance>{}.swap(instances);
        requiredBytes = 0u;
        parsed.complete = false;
    }

    while ( requiredBytes > 0u
        && framePoseCacheBytes_ > kMaximumFramePoseCacheBytes - requiredBytes )
    {
        auto oldest = frameOwnerPoseCaches_.end();
        for ( auto candidate = frameOwnerPoseCaches_.begin();
            candidate != frameOwnerPoseCaches_.end();
            ++candidate )
        {
            if ( candidate->first == thingIndex
                || candidate->second.poseBytes == 0u )
            {
                continue;
            }
            if ( oldest == frameOwnerPoseCaches_.end()
                || candidate->second.lastUseOrdinal < oldest->second.lastUseOrdinal )
            {
                oldest = candidate;
            }
        }
        if ( oldest == frameOwnerPoseCaches_.end() )
        {
            std::vector<render::frame_ghost::FramePose>{}.swap(parsed.poses);
            std::vector<FrameGhostInstance>{}.swap(instances);
            requiredBytes = 0u;
            parsed.complete = false;
            break;
        }

        if ( oldest->second.poseBytes <= framePoseCacheBytes_ )
        {
            framePoseCacheBytes_ -= oldest->second.poseBytes;
        }
        oldest->second.poseBytes = 0u;
        std::vector<render::frame_ghost::FramePose>{}.swap(
            oldest->second.poses);
        oldest->second.overflowFramePose.reset();
        oldest->second.failedOverflowValueIndex = -1;
        oldest->second.posesValid = false;
        oldest->second.posesComplete = false;
        std::vector<FrameGhostInstance>{}.swap(
            oldest->second.ghostInstances);
        oldest->second.ghostInstancesDirty = true;
        oldest->second.instanceBufferSlot = -1;
        for ( std::size_t slotIndex = 0u;
            slotIndex < frameGhostInstanceBufferSlots_.size();
            ++slotIndex )
        {
            if ( frameGhostInstanceBufferSlots_[slotIndex].ownerThingIndex
                == oldest->first )
            {
                frameGhostInstanceBufferSlots_[slotIndex].ownerThingIndex = -1;
            }
        }
    }

    ownerCache.poses = std::move(parsed.poses);
    ownerCache.ghostInstances = std::move(instances);
    ownerCache.overflowFramePose.reset();
    ownerCache.failedOverflowValueIndex = -1;
    ownerCache.poseBytes = requiredBytes;
    ownerCache.posesValid = true;
    ownerCache.posesComplete = parsed.complete;
    ownerCache.ghostInstancesDirty = true;
    framePoseCacheBytes_ += requiredBytes;
    return &ownerCache;
}

std::shared_ptr<const assets::Model3do> GlView::ensureFrameOwnerModel(
    FrameOwnerPoseCache& ownerCache,
    const Thing& thing)
{
    if ( ownerCache.modelResolved )
    {
        return ownerCache.model;
    }

    if ( !level_ )
    {
        ownerCache.modelResolved = true;
        return {};
    }

    // The ordinary Thing renderer gives a supported SPR visual precedence
    // over its 3DO. Keep a local or inherited supported sprite from acquiring
    // an extra wire ghost at every stored Frame.
    const auto ordinarySpriteRenderType = [this](const int storedType)
    {
        return level_->kind != ProjectType::IJIM
                && storedType >= 0
                && storedType <= 2
            ? 0
            : storedType;
    };
    if ( const TplValue* spriteValue = sed::level::thingFindValue(
            *level_, templates_, thing, "sprite") )
    {
        const std::string spriteName = spriteValue->stringValue.empty()
            ? spriteValue->asString()
            : spriteValue->stringValue;
        if ( !spriteName.empty() )
        {
            if ( const assets::SprFile* sprite = loadSprite(spriteName);
                sprite
                && (ordinarySpriteRenderType(sprite->type) != 2
                    || level_->kind == ProjectType::IJIM) )
            {
                ownerCache.modelName = "spr:" + spriteName;
                ownerCache.model.reset();
                ownerCache.modelResolved = true;
                return {};
            }
        }
    }

    const TplValue* modelValue = level_
        ? sed::level::thingFindValue(*level_, templates_, thing, "model3d")
        : nullptr;
    if ( !modelValue )
    {
        modelValue = level_
            ? sed::level::thingFindValue(*level_, templates_, thing, "model3do")
            : nullptr;
    }
    const std::string modelName = modelValue
        ? (modelValue->stringValue.empty()
            ? modelValue->asString()
            : modelValue->stringValue)
        : std::string{};

    ownerCache.modelName = modelName;
    ownerCache.model.reset();
    ownerCache.modelResolved = true;
    if ( modelName.empty() || modelName == "-" )
    {
        return {};
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    ++frameOverlayWorkForTest_.modelCacheResolveCalls;
#endif
    ownerCache.model = loadModel3do(modelName);
    return ownerCache.model;
}

GlView::FrameModelTopologyCache* GlView::ensureFrameModelTopologyCache(
    const std::shared_ptr<const assets::Model3do>& model)
{
    if ( !model )
    {
        return nullptr;
    }

    auto found = frameModelTopologyCaches_.find(model.get());
    if ( found != frameModelTopologyCaches_.end() )
    {
        found->second.lastUseOrdinal = ++frameRenderCacheUseOrdinal_;
        return &found->second;
    }

    FrameModelTopologyCache candidate{};
    candidate.model = model;
    candidate.lastUseOrdinal = ++frameRenderCacheUseOrdinal_;
    try
    {
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        candidate.topology = render::frame_ghost::buildModelTopology(
            *model,
            frameGhostTopologyEdgeLimitForTest_);
#else
        candidate.topology = render::frame_ghost::buildModelTopology(*model);
#endif
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        ++frameOverlayWorkForTest_.topologyCacheBuilds;
        frameOverlayWorkForTest_.sourceFaceEdgeVisits +=
            candidate.topology.sourceFaceEdgeVisits;
        frameOverlayWorkForTest_.uniqueGhostEdgesBuilt +=
            candidate.topology.edges.size();
#endif
        if ( candidate.topology.withinBudget )
        {
            candidate.lineVertices =
                render::frame_ghost::buildLocalLineVertices(
                    *model,
                    candidate.topology);
        }
        for ( const assets::Model3doMesh& mesh : model->meshes )
        {
            for ( const assets::Model3doVertex& vertex : mesh.vertices )
            {
                if ( !std::isfinite(vertex.x)
                    || !std::isfinite(vertex.y)
                    || !std::isfinite(vertex.z) )
                {
                    continue;
                }
                const render::frame_ghost::LocalVertex position{
                    static_cast<float>(vertex.x),
                    static_cast<float>(vertex.y),
                    static_cast<float>(vertex.z),
                };
                if ( !std::isfinite(position[0])
                    || !std::isfinite(position[1])
                    || !std::isfinite(position[2]) )
                {
                    continue;
                }
                if ( !candidate.haveFiniteBounds )
                {
                    candidate.boundsMin = position;
                    candidate.boundsMax = position;
                    candidate.haveFiniteBounds = true;
                    continue;
                }
                for ( std::size_t axis = 0u; axis < position.size(); ++axis )
                {
                    candidate.boundsMin[axis] = std::min(
                        candidate.boundsMin[axis],
                        position[axis]);
                    candidate.boundsMax[axis] = std::max(
                        candidate.boundsMax[axis],
                        position[axis]);
                }
            }
        }
    }
    catch ( const std::bad_alloc& )
    {
        candidate.topology.withinBudget = false;
        std::vector<render::frame_ghost::EdgeRef>{}.swap(
            candidate.topology.edges);
        std::vector<render::frame_ghost::LocalVertex>{}.swap(
            candidate.lineVertices);
    }

    const auto cacheBytes = [](const FrameModelTopologyCache& value) noexcept
    {
        constexpr std::size_t max = std::numeric_limits<std::size_t>::max();
        if ( value.topology.edges.capacity() > max / sizeof(
                render::frame_ghost::EdgeRef) )
        {
            return max;
        }
        const std::size_t edgeBytes = value.topology.edges.capacity()
            * sizeof(render::frame_ghost::EdgeRef);
        if ( value.lineVertices.capacity() > (max - edgeBytes) / sizeof(
                render::frame_ghost::LocalVertex) )
        {
            return max;
        }
        return edgeBytes + value.lineVertices.capacity()
            * sizeof(render::frame_ghost::LocalVertex);
    };
    std::size_t requiredBytes = cacheBytes(candidate);
    if ( requiredBytes > kMaximumFrameTopologyCacheBytes )
    {
        candidate.topology.withinBudget = false;
        std::vector<render::frame_ghost::EdgeRef>{}.swap(
            candidate.topology.edges);
        std::vector<render::frame_ghost::LocalVertex>{}.swap(
            candidate.lineVertices);
        requiredBytes = cacheBytes(candidate);
    }

    while ( frameModelTopologyCaches_.size() >= kMaximumFrameTopologyCaches
        || frameTopologyCacheBytes_ > kMaximumFrameTopologyCacheBytes - requiredBytes )
    {
        auto oldest = frameModelTopologyCaches_.end();
        for ( auto cache = frameModelTopologyCaches_.begin();
            cache != frameModelTopologyCaches_.end();
            ++cache )
        {
            if ( oldest == frameModelTopologyCaches_.end()
                || cache->second.lastUseOrdinal < oldest->second.lastUseOrdinal )
            {
                oldest = cache;
            }
        }
        if ( oldest == frameModelTopologyCaches_.end() )
        {
            break;
        }
        const std::size_t oldBytes = cacheBytes(oldest->second);
        if ( oldBytes <= frameTopologyCacheBytes_ )
        {
            frameTopologyCacheBytes_ -= oldBytes;
        }
        frameModelTopologyCaches_.erase(oldest);
    }

    if ( requiredBytes > kMaximumFrameTopologyCacheBytes
        || frameModelTopologyCaches_.size() >= kMaximumFrameTopologyCaches
        || frameTopologyCacheBytes_ > kMaximumFrameTopologyCacheBytes - requiredBytes )
    {
        // Keep a small negative cache entry so an oversized immutable model is
        // not rescanned on every paint; this model's ghost edges are omitted.
        candidate.topology.withinBudget = false;
        std::vector<render::frame_ghost::EdgeRef>{}.swap(
            candidate.topology.edges);
        std::vector<render::frame_ghost::LocalVertex>{}.swap(
            candidate.lineVertices);
        requiredBytes = cacheBytes(candidate);
    }

    try
    {
        const auto [inserted, didInsert] = frameModelTopologyCaches_.try_emplace(
            model.get(),
            std::move(candidate));
        if ( !didInsert )
        {
            return &inserted->second;
        }
        const std::size_t storedBytes = cacheBytes(inserted->second);
        if ( storedBytes <= kMaximumFrameTopologyCacheBytes
            && frameTopologyCacheBytes_ <= kMaximumFrameTopologyCacheBytes - storedBytes )
        {
            frameTopologyCacheBytes_ += storedBytes;
        }
        else
        {
            inserted->second.topology.withinBudget = false;
            std::vector<render::frame_ghost::EdgeRef>{}.swap(
                inserted->second.topology.edges);
            std::vector<render::frame_ghost::LocalVertex>{}.swap(
                inserted->second.lineVertices);
        }
        return &inserted->second;
    }
    catch ( const std::bad_alloc& )
    {
        return nullptr;
    }
}

const render::frame_ghost::FramePose* GlView::findFramePose(
    const std::int32_t thingIndex,
    const std::int32_t valueIndex)
{
    if ( valueIndex < 0 || !level_
        || thingIndex < 0
        || static_cast<std::size_t>(thingIndex) >= level_->things.size() )
    {
        return nullptr;
    }

    FrameOwnerPoseCache* ownerCache = ensureFrameOwnerPoseCache(thingIndex);
    if ( !ownerCache )
    {
        return nullptr;
    }
    ownerCache->lastUseOrdinal = ++frameRenderCacheUseOrdinal_;
    const auto findByIndex = [valueIndex](
        const std::vector<render::frame_ghost::FramePose>& poses)
    {
        return std::find_if(
            poses.begin(),
            poses.end(),
            [valueIndex](const render::frame_ghost::FramePose& pose)
            {
                return pose.valueIndex == valueIndex;
            });
    };

    auto cached = findByIndex(ownerCache->poses);
    if ( cached != ownerCache->poses.end() )
    {
        return &*cached;
    }
    if ( ownerCache->overflowFramePose
        && ownerCache->overflowFramePose->valueIndex == valueIndex )
    {
        return &*ownerCache->overflowFramePose;
    }
    if ( ownerCache->failedOverflowValueIndex == valueIndex
        || ownerCache->posesComplete )
    {
        return nullptr;
    }

    const Thing& thing = level_->things[static_cast<std::size_t>(thingIndex)];
    if ( static_cast<std::size_t>(valueIndex) >= thing.values.size() )
    {
        return nullptr;
    }
    const TplValue& value = thing.values[static_cast<std::size_t>(valueIndex)];
    if ( value.type != ValueType::frame
        || !sed::text::iequalsAscii(value.name, "frame") )
    {
        return nullptr;
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    ++frameOverlayWorkForTest_.framePickValueParseCalls;
    ++frameOverlayWorkForTest_.frameValueParseCalls;
#endif
    render::frame_ghost::FramePose pose{};
    pose.valueIndex = valueIndex;
    if ( !value.tryGetFrame(
            pose.x,
            pose.y,
            pose.z,
            pose.pitch,
            pose.yaw,
            pose.roll)
        || !std::isfinite(pose.x)
        || !std::isfinite(pose.y)
        || !std::isfinite(pose.z) )
    {
        ownerCache->failedOverflowValueIndex = valueIndex;
        return nullptr;
    }
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    ++frameOverlayWorkForTest_.validFramePosesParsed;
#endif
    pose.finiteOrientation = std::isfinite(pose.pitch)
        && std::isfinite(pose.yaw)
        && std::isfinite(pose.roll);

    // The path cache has a hard bound. Direct selection of a valid frame past
    // that bound still works through one indexed value. The one-entry cache
    // never grows with authored input size and is replaced on selection change.
    ownerCache->failedOverflowValueIndex = -1;
    ownerCache->overflowFramePose = pose;
    return &*ownerCache->overflowFramePose;
}

void GlView::invalidateFramePoseCaches(
    const std::vector<std::int32_t>& thingIndices,
    const bool invalidateModelIdentity) noexcept
{
    for ( const std::int32_t thingIndex : thingIndices )
    {
        const auto found = frameOwnerPoseCaches_.find(thingIndex);
        if ( found == frameOwnerPoseCaches_.end() )
        {
            continue;
        }
        FrameOwnerPoseCache& ownerCache = found->second;
        if ( ownerCache.poseBytes <= framePoseCacheBytes_ )
        {
            framePoseCacheBytes_ -= ownerCache.poseBytes;
        }
        ownerCache.poseBytes = 0u;
        std::vector<render::frame_ghost::FramePose>{}.swap(ownerCache.poses);
        ownerCache.overflowFramePose.reset();
        ownerCache.failedOverflowValueIndex = -1;
        ownerCache.posesValid = false;
        ownerCache.posesComplete = false;
        std::vector<FrameGhostInstance>{}.swap(ownerCache.ghostInstances);
        ownerCache.ghostInstancesDirty = true;
        if ( invalidateModelIdentity )
        {
            ownerCache.modelResolved = false;
            ownerCache.modelName.clear();
            ownerCache.model.reset();
        }
    }
}

void GlView::invalidateAllFrameRenderCaches() noexcept
{
    frameOwnerPoseCaches_.clear();
    framePoseCacheBytes_ = 0u;
    frameModelTopologyCaches_.clear();
    frameTopologyCacheBytes_ = 0u;
    frameGhostUploadedModel_.reset();
    for ( FrameGhostInstanceBufferSlot& slot : frameGhostInstanceBufferSlots_ )
    {
        slot.ownerThingIndex = -1;
        slot.lastUseOrdinal = 0u;
    }
    ++frameRenderCacheUseOrdinal_;
}

} // namespace sed
