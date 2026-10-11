#include "render/gl_view_common.hpp"
#include "render/upload/buffer_upload_utils.hpp"

#include "render/overlays/frame_ghost_topology.hpp"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

namespace sed
{
namespace
{

constexpr std::size_t kFrameGhostClipPlaneCount = 7u;
constexpr float kFrameGhostPositiveWGuard = 1.0e-5f;
constexpr float kFrameGhostClipTolerance = 1.0e-5f;

void restoreCapability(
    QOpenGLFunctions& functions,
    const GLenum capability,
    const GLboolean wasEnabled) noexcept
{
    if ( wasEnabled == GL_TRUE )
    {
        functions.glEnable(capability);
    }
    else
    {
        functions.glDisable(capability);
    }
}

struct FrameGhostGlState
{
    FrameGhostGlState(
        QOpenGLFunctions& openGlFunctions,
        QOpenGLExtraFunctions& extraOpenGlFunctions) noexcept
        : functions(openGlFunctions), extra(extraOpenGlFunctions)
    {
        functions.glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        functions.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
        functions.glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
        functions.glGetIntegerv(GL_BLEND_SRC_RGB, &blendSourceRgb);
        functions.glGetIntegerv(GL_BLEND_DST_RGB, &blendDestinationRgb);
        functions.glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSourceAlpha);
        functions.glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestinationAlpha);
        functions.glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
        functions.glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
        functions.glGetFloatv(GL_LINE_WIDTH, &lineWidth);
        functions.glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        functions.glGetBooleanv(GL_COLOR_WRITEMASK, colorMask.data());

        depthTest = functions.glIsEnabled(GL_DEPTH_TEST);
        stencilTest = functions.glIsEnabled(GL_STENCIL_TEST);
        scissorTest = functions.glIsEnabled(GL_SCISSOR_TEST);
        blend = functions.glIsEnabled(GL_BLEND);
        cullFace = functions.glIsEnabled(GL_CULL_FACE);
        for ( std::size_t plane = 0u; plane < clipDistanceEnabled.size(); ++plane )
        {
            clipDistanceEnabled[plane] = functions.glIsEnabled(
                GL_CLIP_DISTANCE0 + static_cast<GLenum>(plane));
        }
    }

    ~FrameGhostGlState() noexcept
    {
        extra.glBindVertexArray(static_cast<GLuint>(vertexArray));
        functions.glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
        functions.glUseProgram(static_cast<GLuint>(program));
        functions.glBlendFuncSeparate(
            static_cast<GLenum>(blendSourceRgb),
            static_cast<GLenum>(blendDestinationRgb),
            static_cast<GLenum>(blendSourceAlpha),
            static_cast<GLenum>(blendDestinationAlpha));
        functions.glBlendEquationSeparate(
            static_cast<GLenum>(blendEquationRgb),
            static_cast<GLenum>(blendEquationAlpha));
        functions.glLineWidth(static_cast<GLfloat>(lineWidth));
        functions.glDepthMask(depthMask);
        functions.glColorMask(
            colorMask[0], colorMask[1], colorMask[2], colorMask[3]);

        restoreCapability(functions, GL_DEPTH_TEST, depthTest);
        restoreCapability(functions, GL_STENCIL_TEST, stencilTest);
        restoreCapability(functions, GL_SCISSOR_TEST, scissorTest);
        restoreCapability(functions, GL_BLEND, blend);
        restoreCapability(functions, GL_CULL_FACE, cullFace);
        for ( std::size_t plane = 0u; plane < clipDistanceEnabled.size(); ++plane )
        {
            restoreCapability(
                functions,
                GL_CLIP_DISTANCE0 + static_cast<GLenum>(plane),
                clipDistanceEnabled[plane]);
        }
    }

    QOpenGLFunctions& functions;
    QOpenGLExtraFunctions& extra;
    GLint program{};
    GLint vertexArray{};
    GLint arrayBuffer{};
    GLint blendSourceRgb{};
    GLint blendDestinationRgb{};
    GLint blendSourceAlpha{};
    GLint blendDestinationAlpha{};
    GLint blendEquationRgb{};
    GLint blendEquationAlpha{};
    GLfloat lineWidth{ 1.0f };
    GLboolean depthMask{ GL_TRUE };
    std::array<GLboolean, 4u> colorMask{ GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
    GLboolean depthTest{};
    GLboolean stencilTest{};
    GLboolean scissorTest{};
    GLboolean blend{};
    GLboolean cullFace{};
    std::array<GLboolean, kFrameGhostClipPlaneCount> clipDistanceEnabled{};
};

template <typename FrameGhostInstance>
[[nodiscard]] bool isFrameGhostInstanceVisible(
    const FrameGhostInstance& instance,
    const QMatrix4x4& mvp,
    const render::frame_ghost::LocalVertex& boundsMin,
    const render::frame_ghost::LocalVertex& boundsMax) noexcept
{
    const float* const viewProjection = mvp.constData();
    const auto& model = instance.matrix;
    std::array<bool, kFrameGhostClipPlaneCount> allOutside{};
    allOutside.fill(true);

    for ( unsigned int corner = 0u; corner < 8u; ++corner )
    {
        const float x = (corner & 1u) != 0u ? boundsMax[0] : boundsMin[0];
        const float y = (corner & 2u) != 0u ? boundsMax[1] : boundsMin[1];
        const float z = (corner & 4u) != 0u ? boundsMax[2] : boundsMin[2];

        const float worldX = model[0] * x + model[4] * y + model[8] * z + model[12];
        const float worldY = model[1] * x + model[5] * y + model[9] * z + model[13];
        const float worldZ = model[2] * x + model[6] * y + model[10] * z + model[14];
        const float worldW = model[3] * x + model[7] * y + model[11] * z + model[15];

        const float clipX = viewProjection[0] * worldX
            + viewProjection[4] * worldY
            + viewProjection[8] * worldZ
            + viewProjection[12] * worldW;
        const float clipY = viewProjection[1] * worldX
            + viewProjection[5] * worldY
            + viewProjection[9] * worldZ
            + viewProjection[13] * worldW;
        const float clipZ = viewProjection[2] * worldX
            + viewProjection[6] * worldY
            + viewProjection[10] * worldZ
            + viewProjection[14] * worldW;
        const float clipW = viewProjection[3] * worldX
            + viewProjection[7] * worldY
            + viewProjection[11] * worldZ
            + viewProjection[15] * worldW;
        if ( !std::isfinite(clipX) || !std::isfinite(clipY)
            || !std::isfinite(clipZ) || !std::isfinite(clipW) )
        {
            return false;
        }

        const std::array<float, kFrameGhostClipPlaneCount> distances{
            clipX + clipW,
            clipW - clipX,
            clipY + clipW,
            clipW - clipY,
            clipZ + clipW,
            clipW - clipZ,
            clipW - kFrameGhostPositiveWGuard,
        };
        for ( std::size_t plane = 0u; plane < distances.size(); ++plane )
        {
            const float tolerance = plane + 1u == distances.size()
                ? 0.0f
                : kFrameGhostClipTolerance;
            if ( distances[plane] >= -tolerance )
            {
                allOutside[plane] = false;
            }
        }
    }

    return std::none_of(allOutside.begin(), allOutside.end(), [](const bool outside)
    {
        return outside;
    });
}

} // namespace

void GlView::initializeFrameGhostOpenGlResources()
{
    QOpenGLContext* const activeContext = QOpenGLContext::currentContext();
    if ( !activeContext || activeContext != context() )
    {
        return;
    }

    auto candidateProgram = std::make_unique<QOpenGLShaderProgram>();
    constexpr const char* vertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_local_position;
layout(location = 1) in vec4 a_pose_column_0;
layout(location = 2) in vec4 a_pose_column_1;
layout(location = 3) in vec4 a_pose_column_2;
layout(location = 4) in vec4 a_pose_column_3;

uniform mat4 u_mvp;

void main()
{
    mat4 pose = mat4(
        a_pose_column_0,
        a_pose_column_1,
        a_pose_column_2,
        a_pose_column_3);
    vec4 clip_position = u_mvp * pose * vec4(a_local_position, 1.0);
    gl_Position = clip_position;
    gl_ClipDistance[0] = clip_position.x + clip_position.w;
    gl_ClipDistance[1] = clip_position.w - clip_position.x;
    gl_ClipDistance[2] = clip_position.y + clip_position.w;
    gl_ClipDistance[3] = clip_position.w - clip_position.y;
    gl_ClipDistance[4] = clip_position.z + clip_position.w;
    gl_ClipDistance[5] = clip_position.w - clip_position.z;
    gl_ClipDistance[6] = clip_position.w - 1.0e-5;
}
)GLSL";
    constexpr const char* fragmentShader = R"GLSL(
#version 330 core
uniform vec4 u_color;
out vec4 frag_color;

void main()
{
    frag_color = u_color;
}
)GLSL";

    if ( !candidateProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader)
        || !candidateProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader)
        || !candidateProgram->link() )
    {
        Q_EMIT warningMessage(
            QString("Failed to compile/link Frame ghost shader: %1")
                .arg(candidateProgram->log()));
        return;
    }

    auto candidateVao = std::make_unique<QOpenGLVertexArrayObject>();
    auto candidateLineVbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    std::array<std::unique_ptr<QOpenGLBuffer>, 64u> candidateInstanceVbos{};
    if ( !candidateVao->create() || !candidateLineVbo->create() )
    {
        Q_EMIT warningMessage(QStringLiteral("Failed to create Frame ghost OpenGL buffers."));
        return;
    }
    candidateLineVbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    for ( auto& instanceVbo : candidateInstanceVbos )
    {
        instanceVbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
        if ( !instanceVbo->create() )
        {
            Q_EMIT warningMessage(QStringLiteral("Failed to create Frame ghost instance buffers."));
            return;
        }
        instanceVbo->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    }

    QOpenGLExtraFunctions* const extra = activeContext->extraFunctions();
    QOpenGLFunctions* const functions = activeContext->functions();
    if ( !extra || !functions )
    {
        Q_EMIT warningMessage(QStringLiteral("Frame ghost OpenGL functions are unavailable."));
        return;
    }
    extra->initializeOpenGLFunctions();

    FrameGhostGlState state(*functions, *extra);

    candidateVao->bind();
    if ( !candidateLineVbo->bind() )
    {
        Q_EMIT warningMessage(QStringLiteral("Failed to bind Frame ghost OpenGL buffers."));
        return;
    }
    extra->glEnableVertexAttribArray(0u);
    extra->glVertexAttribPointer(
        0u,
        3,
        GL_FLOAT,
        GL_FALSE,
        static_cast<GLsizei>(sizeof(render::frame_ghost::LocalVertex)),
        nullptr);
    candidateLineVbo->release();
    candidateVao->release();

    frameGhostProgram_ = std::move(candidateProgram);
    frameGhostLineVao_ = std::move(candidateVao);
    frameGhostLineVbo_ = std::move(candidateLineVbo);
    frameGhostInstanceVbos_ = std::move(candidateInstanceVbos);
    frameGhostLineVboAllocatedBytes_ = 0;
    frameGhostInstanceVboAllocatedBytes_.fill(0);
    frameGhostGpuBytes_ = 0u;
    frameGhostLineVertexCount_ = 0u;
    frameGhostUploadedModel_.reset();
    for ( FrameGhostInstanceBufferSlot& slot : frameGhostInstanceBufferSlots_ )
    {
        slot = {};
        slot.ownerThingIndex = -1;
    }
    for ( auto& entry : frameOwnerPoseCaches_ )
    {
        entry.second.instanceBufferSlot = -1;
        entry.second.ghostInstancesDirty = true;
    }
}

void GlView::releaseFrameGhostOpenGlResources() noexcept
{
    frameGhostLineVao_.reset();
    frameGhostLineVbo_.reset();
    for ( auto& instanceVbo : frameGhostInstanceVbos_ )
    {
        instanceVbo.reset();
    }
    frameGhostProgram_.reset();

    frameGhostLineVboAllocatedBytes_ = 0;
    frameGhostInstanceVboAllocatedBytes_.fill(0);
    frameGhostGpuBytes_ = 0u;
    frameGhostLineVertexCount_ = 0u;
    frameGhostUploadedModel_.reset();
    for ( FrameGhostInstanceBufferSlot& slot : frameGhostInstanceBufferSlots_ )
    {
        slot = {};
        slot.ownerThingIndex = -1;
    }
    for ( auto& entry : frameOwnerPoseCaches_ )
    {
        entry.second.instanceBufferSlot = -1;
        entry.second.ghostInstancesDirty = true;
    }
}

void GlView::drawFrameGhostBatch(
    FrameOwnerPoseCache& ownerCache,
    const std::int32_t activeValueIndex,
    const QMatrix4x4& mvp)
{
    std::size_t nonActiveInstanceCount = 0u;
    for ( const FrameGhostInstance& instance : ownerCache.ghostInstances )
    {
        if ( instance.valueIndex != activeValueIndex )
        {
            ++nonActiveInstanceCount;
        }
    }
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    frameOverlayWorkForTest_.lastGhostInstanceCount = nonActiveInstanceCount;
#endif

    if ( nonActiveInstanceCount == 0u || !ownerCache.model
        || ownerCache.ghostInstances.empty() )
    {
        return;
    }

    QOpenGLContext* const activeContext = QOpenGLContext::currentContext();
    if ( !activeContext || activeContext != context() )
    {
        return;
    }
    if ( !frameGhostProgram_ || !frameGhostLineVao_ || !frameGhostLineVbo_ )
    {
        return;
    }

    FrameModelTopologyCache* const topologyCache =
        ensureFrameModelTopologyCache(ownerCache.model);
    if ( !topologyCache || !topologyCache->topology.withinBudget
        || !topologyCache->haveFiniteBounds
        || topologyCache->lineVertices.empty()
        || (topologyCache->lineVertices.size() % 2u) != 0u )
    {
        return;
    }

    const std::size_t vertexCount = topologyCache->lineVertices.size();
    if ( vertexCount > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())
        || vertexCount > std::numeric_limits<std::size_t>::max()
            / sizeof(render::frame_ghost::LocalVertex) )
    {
        return;
    }
    const std::size_t lineBytes = vertexCount * sizeof(render::frame_ghost::LocalVertex);
    if ( lineBytes > render::frame_ghost::kMaximumGhostLineBufferBytes )
    {
        return;
    }
    const std::optional<int> lineByteCount = render::upload::safeByteCount(lineBytes);
    if ( !lineByteCount )
    {
        return;
    }

    std::int32_t ownerThingIndex = -1;
    for ( const auto& entry : frameOwnerPoseCaches_ )
    {
        if ( &entry.second == &ownerCache )
        {
            ownerThingIndex = entry.first;
            break;
        }
    }
    if ( ownerThingIndex < 0 )
    {
        return;
    }

    QOpenGLFunctions* const functions = activeContext->functions();
    QOpenGLExtraFunctions* const extra = activeContext->extraFunctions();
    if ( !functions || !extra )
    {
        return;
    }
    extra->initializeOpenGLFunctions();

    FrameGhostGlState state(*functions, *extra);

    const auto clearInstanceBufferSlot = [&](const std::size_t slotIndex) -> bool
    {
        if ( slotIndex >= frameGhostInstanceVbos_.size()
            || !frameGhostInstanceVbos_[slotIndex] )
        {
            return false;
        }

        QOpenGLBuffer& buffer = *frameGhostInstanceVbos_[slotIndex];
        if ( !buffer.bind() )
        {
            return false;
        }
        buffer.allocate(nullptr, 0);
        buffer.release();

        const std::size_t bytes = static_cast<std::size_t>(
            std::max(frameGhostInstanceVboAllocatedBytes_[slotIndex], 0));
        frameGhostGpuBytes_ = bytes <= frameGhostGpuBytes_
            ? frameGhostGpuBytes_ - bytes
            : 0u;
        frameGhostInstanceVboAllocatedBytes_[slotIndex] = 0;

        FrameGhostInstanceBufferSlot& slot = frameGhostInstanceBufferSlots_[slotIndex];
        const std::int32_t previousOwner = slot.ownerThingIndex;
        if ( previousOwner >= 0 )
        {
            const auto found = frameOwnerPoseCaches_.find(previousOwner);
            if ( found != frameOwnerPoseCaches_.end() )
            {
                found->second.instanceBufferSlot = -1;
                found->second.ghostInstancesDirty = true;
            }
        }
        slot = {};
        slot.ownerThingIndex = -1;
        return true;
    };

    const auto makeRoomForGpuBytes = [&](const std::size_t additionalBytes,
                                         const std::int32_t protectedSlot) -> bool
    {
        if ( additionalBytes > kMaximumFrameGhostGpuBytes )
        {
            return false;
        }
        while ( frameGhostGpuBytes_ > kMaximumFrameGhostGpuBytes - additionalBytes )
        {
            std::size_t oldestSlot = frameGhostInstanceBufferSlots_.size();
            std::uint64_t oldestUse = std::numeric_limits<std::uint64_t>::max();
            for ( std::size_t slotIndex = 0u;
                slotIndex < frameGhostInstanceBufferSlots_.size();
                ++slotIndex )
            {
                if ( static_cast<std::int32_t>(slotIndex) == protectedSlot
                    || frameGhostInstanceVboAllocatedBytes_[slotIndex] <= 0 )
                {
                    continue;
                }
                if ( oldestSlot == frameGhostInstanceBufferSlots_.size()
                    || frameGhostInstanceBufferSlots_[slotIndex].lastUseOrdinal < oldestUse )
                {
                    oldestSlot = slotIndex;
                    oldestUse = frameGhostInstanceBufferSlots_[slotIndex].lastUseOrdinal;
                }
            }
            if ( oldestSlot == frameGhostInstanceBufferSlots_.size()
                || !clearInstanceBufferSlot(oldestSlot) )
            {
                return false;
            }
        }
        return true;
    };

    const bool modelIdentityChanged = frameGhostUploadedModel_ != topologyCache->model;
    const bool localVerticesChanged = modelIdentityChanged
        || frameGhostLineVertexCount_ != vertexCount;
    if ( localVerticesChanged )
    {
        const std::size_t allocatedBytes = static_cast<std::size_t>(
            std::max(frameGhostLineVboAllocatedBytes_, 0));
        const std::size_t additionalBytes = lineBytes > allocatedBytes
            ? lineBytes - allocatedBytes
            : 0u;
        if ( !makeRoomForGpuBytes(additionalBytes, -1)
            || !frameGhostLineVbo_->bind() )
        {
            return;
        }

        if ( additionalBytes > 0u )
        {
            frameGhostLineVbo_->allocate(
                topologyCache->lineVertices.data(),
                *lineByteCount);
            frameGhostGpuBytes_ += additionalBytes;
            frameGhostLineVboAllocatedBytes_ = *lineByteCount;
        }
        else
        {
            frameGhostLineVbo_->write(
                0,
                topologyCache->lineVertices.data(),
                *lineByteCount);
        }
        frameGhostLineVbo_->release();
        frameGhostLineVertexCount_ = vertexCount;
        frameGhostUploadedModel_ = topologyCache->model;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        ++frameOverlayWorkForTest_.persistentBatchUploads;
#endif
    }

    std::int32_t instanceSlot = ownerCache.instanceBufferSlot;
    if ( instanceSlot < 0
        || static_cast<std::size_t>(instanceSlot) >= frameGhostInstanceBufferSlots_.size()
        || frameGhostInstanceBufferSlots_[static_cast<std::size_t>(instanceSlot)].ownerThingIndex
            != ownerThingIndex )
    {
        instanceSlot = -1;
        for ( std::size_t slotIndex = 0u;
            slotIndex < frameGhostInstanceBufferSlots_.size();
            ++slotIndex )
        {
            if ( frameGhostInstanceBufferSlots_[slotIndex].ownerThingIndex == ownerThingIndex )
            {
                instanceSlot = static_cast<std::int32_t>(slotIndex);
                break;
            }
        }

        if ( instanceSlot < 0 )
        {
            for ( std::size_t slotIndex = 0u;
                slotIndex < frameGhostInstanceBufferSlots_.size();
                ++slotIndex )
            {
                if ( frameGhostInstanceBufferSlots_[slotIndex].ownerThingIndex < 0 )
                {
                    instanceSlot = static_cast<std::int32_t>(slotIndex);
                    break;
                }
            }
        }

        if ( instanceSlot < 0 )
        {
            std::size_t oldestSlot = frameGhostInstanceBufferSlots_.size();
            for ( std::size_t slotIndex = 0u;
                slotIndex < frameGhostInstanceBufferSlots_.size();
                ++slotIndex )
            {
                if ( oldestSlot == frameGhostInstanceBufferSlots_.size()
                    || frameGhostInstanceBufferSlots_[slotIndex].lastUseOrdinal
                        < frameGhostInstanceBufferSlots_[oldestSlot].lastUseOrdinal )
                {
                    oldestSlot = slotIndex;
                }
            }
            if ( oldestSlot == frameGhostInstanceBufferSlots_.size() )
            {
                return;
            }
            const std::int32_t previousOwner =
                frameGhostInstanceBufferSlots_[oldestSlot].ownerThingIndex;
            if ( previousOwner >= 0 )
            {
                const auto found = frameOwnerPoseCaches_.find(previousOwner);
                if ( found != frameOwnerPoseCaches_.end() )
                {
                    found->second.instanceBufferSlot = -1;
                    found->second.ghostInstancesDirty = true;
                }
            }
            instanceSlot = static_cast<std::int32_t>(oldestSlot);
        }

        ownerCache.instanceBufferSlot = instanceSlot;
        ownerCache.ghostInstancesDirty = true;
        frameGhostInstanceBufferSlots_[static_cast<std::size_t>(instanceSlot)].ownerThingIndex =
            ownerThingIndex;
    }

    const std::size_t instanceSlotIndex = static_cast<std::size_t>(instanceSlot);
    frameGhostInstanceBufferSlots_[instanceSlotIndex].lastUseOrdinal =
        ownerCache.lastUseOrdinal;
    if ( !frameGhostInstanceVbos_[instanceSlotIndex] )
    {
        return;
    }

    if ( ownerCache.ghostInstances.size() > std::numeric_limits<std::size_t>::max()
            / sizeof(FrameGhostInstance)
        || ownerCache.ghostInstances.size()
            > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) )
    {
        return;
    }
    const std::size_t instanceBytes =
        ownerCache.ghostInstances.size() * sizeof(FrameGhostInstance);
    const std::optional<int> instanceByteCount =
        render::upload::safeByteCount(instanceBytes);
    if ( !instanceByteCount || instanceBytes == 0u )
    {
        return;
    }

    if ( ownerCache.ghostInstancesDirty )
    {
        const std::size_t allocatedBytes = static_cast<std::size_t>(
            std::max(frameGhostInstanceVboAllocatedBytes_[instanceSlotIndex], 0));
        const std::size_t additionalBytes = instanceBytes > allocatedBytes
            ? instanceBytes - allocatedBytes
            : 0u;
        if ( !makeRoomForGpuBytes(additionalBytes, instanceSlot)
            || !frameGhostInstanceVbos_[instanceSlotIndex]->bind() )
        {
            return;
        }
        if ( additionalBytes > 0u )
        {
            frameGhostInstanceVbos_[instanceSlotIndex]->allocate(
                ownerCache.ghostInstances.data(),
                *instanceByteCount);
            frameGhostGpuBytes_ += additionalBytes;
            frameGhostInstanceVboAllocatedBytes_[instanceSlotIndex] = *instanceByteCount;
        }
        else
        {
            frameGhostInstanceVbos_[instanceSlotIndex]->write(
                0,
                ownerCache.ghostInstances.data(),
                *instanceByteCount);
        }
        frameGhostInstanceVbos_[instanceSlotIndex]->release();
        ownerCache.ghostInstancesDirty = false;
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        ++frameOverlayWorkForTest_.persistentBatchUploads;
#endif
    }

    if ( frameGhostUploadedModel_ != topologyCache->model
        || frameGhostLineVertexCount_ != vertexCount
        || frameGhostGpuBytes_ > kMaximumFrameGhostGpuBytes )
    {
        return;
    }

    const std::size_t matrixStride = sizeof(FrameGhostInstance);
    if ( matrixStride > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) )
    {
        return;
    }

    constexpr std::array<GLenum, kFrameGhostClipPlaneCount> clipPlanes{
        GL_CLIP_DISTANCE0,
        GL_CLIP_DISTANCE0 + 1u,
        GL_CLIP_DISTANCE0 + 2u,
        GL_CLIP_DISTANCE0 + 3u,
        GL_CLIP_DISTANCE0 + 4u,
        GL_CLIP_DISTANCE0 + 5u,
        GL_CLIP_DISTANCE0 + 6u,
    };
    functions->glDisable(GL_DEPTH_TEST);
    functions->glDepthMask(GL_FALSE);
    functions->glDisable(GL_STENCIL_TEST);
    functions->glDisable(GL_SCISSOR_TEST);
    functions->glDisable(GL_CULL_FACE);
    functions->glEnable(GL_BLEND);
    functions->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    functions->glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    functions->glLineWidth(1.0f);
    functions->glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    for ( const GLenum clipPlane : clipPlanes )
    {
        functions->glEnable(clipPlane);
    }

    if ( !frameGhostProgram_->bind() )
    {
        return;
    }
    frameGhostLineVao_->bind();
    frameGhostProgram_->setUniformValue("u_mvp", mvp);
    frameGhostProgram_->setUniformValue(
        "u_color",
        QVector4D(200.0f / 255.0f, 210.0f / 255.0f, 220.0f / 255.0f, 80.0f / 255.0f));

    QOpenGLBuffer& instanceVbo = *frameGhostInstanceVbos_[instanceSlotIndex];
    if ( !instanceVbo.bind() )
    {
        return;
    }

    const auto flushVisibleRun = [&](const std::size_t firstInstance,
                                     const std::size_t runInstanceCount) -> void
    {
        if ( runInstanceCount == 0u )
        {
            return;
        }
        const std::size_t instanceOffset = firstInstance * sizeof(FrameGhostInstance);
        for ( GLuint column = 0u; column < 4u; ++column )
        {
            const GLuint attribute = 1u + column;
            const std::size_t attributeOffset = instanceOffset
                + offsetof(FrameGhostInstance, matrix)
                + static_cast<std::size_t>(column) * 4u * sizeof(float);
            extra->glEnableVertexAttribArray(attribute);
            extra->glVertexAttribPointer(
                attribute,
                4,
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLsizei>(matrixStride),
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(attributeOffset)));
            extra->glVertexAttribDivisor(attribute, 1u);
        }

        extra->glDrawArraysInstanced(
            GL_LINES,
            0,
            static_cast<GLsizei>(vertexCount),
            static_cast<GLsizei>(runInstanceCount));
#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
        ++frameOverlayWorkForTest_.ghostDrawPasses;
        frameOverlayWorkForTest_.ghostInstancesSubmitted += runInstanceCount;
#endif
    };

    std::size_t runFirstInstance = 0u;
    std::size_t runInstanceCount = 0u;
    for ( std::size_t instanceIndex = 0u;
        instanceIndex < ownerCache.ghostInstances.size();
        ++instanceIndex )
    {
        const FrameGhostInstance& instance = ownerCache.ghostInstances[instanceIndex];
        const bool visible = instance.valueIndex != activeValueIndex
            && isFrameGhostInstanceVisible(
                instance,
                mvp,
                topologyCache->boundsMin,
                topologyCache->boundsMax);
        if ( visible )
        {
            if ( runInstanceCount == 0u )
            {
                runFirstInstance = instanceIndex;
            }
            ++runInstanceCount;
        }
        else
        {
            flushVisibleRun(runFirstInstance, runInstanceCount);
            runInstanceCount = 0u;
        }
    }
    flushVisibleRun(runFirstInstance, runInstanceCount);

    instanceVbo.release();
}

} // namespace sed
