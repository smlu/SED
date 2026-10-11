#include "gl_view_common.hpp"
#include "render/selection/surface_selection_utils.hpp"
#include "render/upload/buffer_upload_utils.hpp"

namespace sed
{


void GlView::uploadMeshes(
    const LayerMeshProgressPulse& progressPulse)
{
    const auto reportProgress = [&progressPulse]()
    {
        if (progressPulse)
        {
            progressPulse();
        }
    };
    reportProgress();
    if (!glReady_ || !program_ || !lineVao_ || !lineVbo_ || !triVao_ || !triVbo_ || !spriteTriVao_ || !spriteTriVbo_ || !pointVao_ || !pointVbo_)
    {
        reportProgress();
        return;
    }

    ++meshUploadCount_;

    const auto writeBuffer = [&reportProgress](
        QOpenGLBuffer& buffer,
        const int destinationOffset,
        const void* source,
        const int byteCount)
    {
        render::upload::writeBufferInProgressChunks(
            destinationOffset,
            source,
            byteCount,
            [&buffer](const int offset, const void* bytes, const int count)
            {
                buffer.write(offset, bytes, count);
            },
            reportProgress);
    };

    triVao_->bind();
    triVbo_->bind();
    const std::optional<int> triBytesOpt =
        render::upload::safeByteCount(triVertices_.size() * sizeof(TriVertex));
    if (!triBytesOpt)
    {
        triVertexCount_ = 0;
        levelTriVertexCount_ = 0;
        thingTriVertexCount_ = 0;
        for (TriBatch& batch : triBatches_)
        {
            batch.firstVertex = 0;
            batch.vertexCount = 0;
        }
        thingTriBuildGlobalFirstVertex_.clear();
        levelPickRanges_.clear();
        thingPickRanges_.clear();
    }
    else
    {
        const int triBytes = *triBytesOpt;
        reportProgress();
        const bool triRealloc = render::upload::ensureBufferCapacity(*triVbo_, triVboAllocatedBytes_, triBytes);
        reportProgress();

        if (triBytes > 0)
        {
            const bool canPartialThings =
                !triRealloc
                && (lastMeshRebuildKind_ == MeshRebuildKind::ThingsOnly)
                && (levelTriVertexCount_ >= 0)
                && (thingTriVertexCount_ >= 0)
                && ((levelTriVertexCount_ + thingTriVertexCount_) == static_cast<int>(triVertices_.size()));

            if (canPartialThings)
            {
                const int thingOffsetBytes = levelTriVertexCount_ * static_cast<int>(sizeof(TriVertex));
                const int thingBytes = thingTriVertexCount_ * static_cast<int>(sizeof(TriVertex));
                if (thingBytes > 0)
                {
                    writeBuffer(
                        *triVbo_, thingOffsetBytes,
                        triVertices_.data() + levelTriVertexCount_,
                        thingBytes);
                }
            }
            else
            {
                writeBuffer(*triVbo_, 0, triVertices_.data(), triBytes);
            }
        }
        else
        {
            triVertexCount_ = 0;
            levelTriVertexCount_ = 0;
            thingTriVertexCount_ = 0;
            for (TriBatch& batch : triBatches_)
            {
                batch.firstVertex = 0;
                batch.vertexCount = 0;
            }
            thingTriBuildGlobalFirstVertex_.clear();
            levelPickRanges_.clear();
            thingPickRanges_.clear();
        }
    }

    program_->bind();
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(0, GL_FLOAT, static_cast<int>(offsetof(TriVertex, x)), 3, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(1);
    program_->setAttributeBuffer(1, GL_FLOAT, static_cast<int>(offsetof(TriVertex, u)), 2, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(2);
    program_->setAttributeBuffer(2, GL_FLOAT, static_cast<int>(offsetof(TriVertex, r)), 4, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(3);
    program_->setAttributeBuffer(3, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickId)), 1, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(4);
    program_->setAttributeBuffer(4, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickGeo)), 1, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(5);
    program_->setAttributeBuffer(5, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sky)), 1, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(6);
    program_->setAttributeBuffer(6, GL_FLOAT, static_cast<int>(offsetof(TriVertex, editorSectorTintR)), 3, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(8);
    program_->setAttributeBuffer(8, GL_FLOAT, static_cast<int>(offsetof(TriVertex, normalX)), 3, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(9);
    program_->setAttributeBuffer(9, GL_FLOAT, static_cast<int>(offsetof(TriVertex, dynamicLightTarget)), 1, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(10);
    program_->setAttributeBuffer(10, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sectorLightX)), 4, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(11);
    program_->setAttributeBuffer(11, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sectorLightR)), 4, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(12);
    program_->setAttributeBuffer(12, GL_FLOAT, static_cast<int>(offsetof(TriVertex, faceLightAnchorX)), 3, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(13);
    program_->setAttributeBuffer(13, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sectorLightOwner)), 1, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(14);
    program_->setAttributeBuffer(14, GL_FLOAT, static_cast<int>(offsetof(TriVertex, surfaceWireBarycentricX)), 3, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(15);
    program_->setAttributeBuffer(15, GL_FLOAT, static_cast<int>(offsetof(TriVertex, surfaceWireBoundaryX)), 4, static_cast<int>(sizeof(TriVertex)));
    program_->release();

    triVbo_->release();
    triVao_->release();
    // uploadMeshes writes the complete composed buffer, including any
    // renderer-only ARCH triangle patch. The sparse uploader no longer needs
    // to revisit those indices after this publication.
    architecturalLightingTriangleDirtyIndices_.clear();
    architecturalLightingTriangleDirtyIndexSet_.clear();
    reportProgress();

    if (translucentTriVao_ && translucentTriVbo_)
    {
        translucentTriVao_->bind();
        translucentTriVbo_->bind();

        program_->bind();
        program_->enableAttributeArray(0);
        program_->setAttributeBuffer(0, GL_FLOAT, static_cast<int>(offsetof(TriVertex, x)), 3, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(1);
        program_->setAttributeBuffer(1, GL_FLOAT, static_cast<int>(offsetof(TriVertex, u)), 2, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(2);
        program_->setAttributeBuffer(2, GL_FLOAT, static_cast<int>(offsetof(TriVertex, r)), 4, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(3);
        program_->setAttributeBuffer(3, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickId)), 1, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(4);
        program_->setAttributeBuffer(4, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickGeo)), 1, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(5);
        program_->setAttributeBuffer(5, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sky)), 1, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(6);
        program_->setAttributeBuffer(6, GL_FLOAT, static_cast<int>(offsetof(TriVertex, editorSectorTintR)), 3, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(8);
        program_->setAttributeBuffer(8, GL_FLOAT, static_cast<int>(offsetof(TriVertex, normalX)), 3, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(9);
        program_->setAttributeBuffer(9, GL_FLOAT, static_cast<int>(offsetof(TriVertex, dynamicLightTarget)), 1, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(10);
        program_->setAttributeBuffer(10, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sectorLightX)), 4, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(11);
        program_->setAttributeBuffer(11, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sectorLightR)), 4, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(12);
        program_->setAttributeBuffer(12, GL_FLOAT, static_cast<int>(offsetof(TriVertex, faceLightAnchorX)), 3, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(13);
        program_->setAttributeBuffer(13, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sectorLightOwner)), 1, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(14);
        program_->setAttributeBuffer(14, GL_FLOAT, static_cast<int>(offsetof(TriVertex, surfaceWireBarycentricX)), 3, static_cast<int>(sizeof(TriVertex)));

        program_->enableAttributeArray(15);
        program_->setAttributeBuffer(15, GL_FLOAT, static_cast<int>(offsetof(TriVertex, surfaceWireBoundaryX)), 4, static_cast<int>(sizeof(TriVertex)));
        program_->release();

        translucentTriVbo_->release();
        translucentTriVao_->release();
    }

    if (spriteTrianglesDirty_)
    {
        rebuildSpriteVertices(progressPulse);
    }

    spriteTriVao_->bind();
    spriteTriVbo_->bind();
    const std::optional<int> spriteBytesOpt =
        render::upload::safeByteCount(spriteTriVertices_.size() * sizeof(TriVertex));
    if (!spriteBytesOpt)
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
        const int spriteBytes = *spriteBytesOpt;
        reportProgress();
        render::upload::ensureBufferCapacity(*spriteTriVbo_, spriteTriVboAllocatedBytes_, spriteBytes);
        reportProgress();
        if (spriteBytes > 0)
        {
            writeBuffer(
                *spriteTriVbo_, 0, spriteTriVertices_.data(), spriteBytes);
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

    program_->bind();
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(0, GL_FLOAT, static_cast<int>(offsetof(TriVertex, x)), 3, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(1);
    program_->setAttributeBuffer(1, GL_FLOAT, static_cast<int>(offsetof(TriVertex, u)), 2, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(2);
    program_->setAttributeBuffer(2, GL_FLOAT, static_cast<int>(offsetof(TriVertex, r)), 4, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(3);
    program_->setAttributeBuffer(3, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickId)), 1, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(4);
    program_->setAttributeBuffer(4, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickGeo)), 1, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(5);
    program_->setAttributeBuffer(5, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sky)), 1, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(6);
    program_->setAttributeBuffer(6, GL_FLOAT, static_cast<int>(offsetof(TriVertex, editorSectorTintR)), 3, static_cast<int>(sizeof(TriVertex)));
    program_->release();

    spriteTriVbo_->release();
    spriteTriVao_->release();
    reportProgress();

    lineVao_->bind();
    lineVbo_->bind();
    const std::optional<int> lineBytesOpt =
        render::upload::safeByteCount(lineVertices_.size() * sizeof(LineVertex));
    if (!lineBytesOpt)
    {
        wireLineVertexCount_ = 0;
        thingLineFirstVertex_ = 0;
        thingLineVertexCount_ = 0;
        thingOverlayLineFirstVertex_ = 0;
        thingOverlayLineVertexCount_ = 0;
        thingCollisionOverlayLineRanges_.clear();
        thingBroadPhaseTreeGuideLineRanges_.clear();
        thingVisualBoundsOverlayLineRanges_.clear();
        thingTemplateCollisionBoundsGuideLineRanges_.clear();
        baseLineLayerRanges_.clear();
    }
    else
    {
        const int lineBytes = *lineBytesOpt;
        reportProgress();
        const bool lineRealloc = render::upload::ensureBufferCapacity(*lineVbo_, lineVboAllocatedBytes_, lineBytes);
        reportProgress();

        if (lineBytes > 0)
        {
            const bool canPartialThings =
                !lineRealloc
                && (lastMeshRebuildKind_ == MeshRebuildKind::ThingsOnly)
                && (thingLineFirstVertex_ >= 0)
                && (thingLineVertexCount_ >= 0)
                && (thingOverlayLineVertexCount_ >= 0)
                && ((thingLineFirstVertex_ + thingLineVertexCount_ + thingOverlayLineVertexCount_)
                    == static_cast<int>(lineVertices_.size()));

            if (canPartialThings)
            {
                const int thingOffsetBytes = thingLineFirstVertex_ * static_cast<int>(sizeof(LineVertex));
                const int thingBytes =
                    (thingLineVertexCount_ + thingOverlayLineVertexCount_)
                    * static_cast<int>(sizeof(LineVertex));
                if (thingBytes > 0)
                {
                    writeBuffer(
                        *lineVbo_, thingOffsetBytes,
                        lineVertices_.data() + thingLineFirstVertex_,
                        thingBytes);
                }
            }
            else
            {
                writeBuffer(*lineVbo_, 0, lineVertices_.data(), lineBytes);
            }
        }
        else
        {
            wireLineVertexCount_ = 0;
            thingLineFirstVertex_ = 0;
            thingLineVertexCount_ = 0;
            thingOverlayLineFirstVertex_ = 0;
            thingOverlayLineVertexCount_ = 0;
            thingCollisionOverlayLineRanges_.clear();
            thingVisualBoundsOverlayLineRanges_.clear();
            baseLineLayerRanges_.clear();
        }
    }

    program_->bind();
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(0, GL_FLOAT, static_cast<int>(offsetof(LineVertex, x)), 3, static_cast<int>(sizeof(LineVertex)));
    program_->enableAttributeArray(3);
    program_->setAttributeBuffer(3, GL_FLOAT, static_cast<int>(offsetof(LineVertex, pickId)), 1, static_cast<int>(sizeof(LineVertex)));
    program_->enableAttributeArray(6);
    program_->setAttributeBuffer(6, GL_FLOAT, static_cast<int>(offsetof(LineVertex, planeX)), 3, static_cast<int>(sizeof(LineVertex)));
    program_->enableAttributeArray(7);
    program_->setAttributeBuffer(7, GL_FLOAT, static_cast<int>(offsetof(LineVertex, planeValid)), 1, static_cast<int>(sizeof(LineVertex)));
    program_->release();

    lineVbo_->release();
    lineVao_->release();
    reportProgress();

    pointVao_->bind();
    pointVbo_->bind();
    const std::optional<int> pointBytesOpt =
        render::upload::safeByteCount(pointPositions_.size() * sizeof(float));
    if (!pointBytesOpt)
    {
        pointVertexCount_ = 0;
        levelPointVertexCount_ = 0;
        thingPointFirstVertex_ = 0;
        thingPointVertexCount_ = 0;
    }
    else
    {
        const int pointBytes = *pointBytesOpt;
        reportProgress();
        const bool pointRealloc = render::upload::ensureBufferCapacity(*pointVbo_, pointVboAllocatedBytes_, pointBytes);
        reportProgress();

        if (pointBytes > 0)
        {
            const bool canPartialThings =
                !pointRealloc
                && (lastMeshRebuildKind_ == MeshRebuildKind::ThingsOnly)
                && (thingPointFirstVertex_ >= 0)
                && (thingPointVertexCount_ >= 0)
                && (((thingPointFirstVertex_ + thingPointVertexCount_) * 3) == static_cast<int>(pointPositions_.size()));

            if (canPartialThings)
            {
                const int firstFloat = thingPointFirstVertex_ * 3;
                const int floatCount = thingPointVertexCount_ * 3;
                const int offsetBytes = firstFloat * static_cast<int>(sizeof(float));
                const int bytes = floatCount * static_cast<int>(sizeof(float));
                if (bytes > 0)
                {
                    writeBuffer(
                        *pointVbo_, offsetBytes,
                        pointPositions_.data() + firstFloat,
                        bytes);
                }
            }
            else
            {
                writeBuffer(*pointVbo_, 0, pointPositions_.data(), pointBytes);
            }
        }
        else
        {
            pointVertexCount_ = 0;
            levelPointVertexCount_ = 0;
            thingPointFirstVertex_ = 0;
            thingPointVertexCount_ = 0;
        }
    }

    program_->bind();
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * static_cast<int>(sizeof(float)));
    program_->release();

    pointVbo_->release();
    pointVao_->release();
    reportProgress();

    uploadSelectionOverlays(progressPulse);
    reportProgress();

    if (showTextures_ && (showSolid_ || thingViewMode_ == ThingViewMode::Shaded))
    {
        ensureTextures(progressPulse);
    }
    refreshAlphaAdjoinWireSupports(progressPulse);
    reportProgress();
}

void GlView::refreshAlphaAdjoinWireSupports(
    const LayerMeshProgressPulse& progressPulse)
{
    if (!alphaAdjoinWireSupportsDirty_)
    {
        return;
    }
    alphaAdjoinWireSupportsDirty_ = false;

    if (!levelMeshCache_.valid)
    {
        return;
    }

    const std::size_t levelLineCount = std::min(
        levelMeshCache_.lineVertices.size(),
        std::min(
            lineVertices_.size(),
            static_cast<std::size_t>(std::max(0, wireLineVertexCount_))));
    if (levelLineCount == 0)
    {
        rebuildSelectionOverlays();
        if (glReady_)
        {
            uploadSelectionOverlays(progressPulse);
        }
        return;
    }

    // Mesh caches remain the immutable baseline. Reapply the small material-
    // dependent classification to the composed level prefix only when an
    // actual texture/cache/view-state transition marks it dirty.
    std::copy_n(
        levelMeshCache_.lineVertices.begin(),
        static_cast<std::ptrdiff_t>(levelLineCount),
        lineVertices_.begin());

    if (showTextures_)
    {
        for (const AdjoinWireLineRange& adjoinRange : alphaAdjoinWireLineRanges_)
        {
            const auto texture = textures_.find(adjoinRange.textureKey);
            if (texture == textures_.end()
                || !texture->second.texture
                || texture->second.alphaBpp <= 0)
            {
                continue;
            }

            const DrawRange range = adjoinRange.range;
            if (!render::isValidDrawRange(range, levelLineCount))
            {
                continue;
            }

            const std::size_t first = static_cast<std::size_t>(range.first);
            const std::size_t end = first + static_cast<std::size_t>(range.count);
            for (std::size_t i = first; i < end; ++i)
            {
                lineVertices_[i].planeValid = 0.0f;
            }
        }
    }

    const std::optional<int> lineBytes =
        render::upload::safeByteCount(levelLineCount * sizeof(LineVertex));
    if (glReady_ && lineVbo_ && lineBytes && *lineBytes > 0)
    {
        lineVbo_->bind();
        lineVbo_->write(0, lineVertices_.data(), *lineBytes);
        lineVbo_->release();
    }

    rebuildSelectionOverlays();
    if (glReady_)
    {
        uploadSelectionOverlays(progressPulse);
    }
}

void GlView::uploadSelectionOverlays(
    const LayerMeshProgressPulse& progressPulse)
{
    const auto reportProgress = [&progressPulse]()
    {
        if (progressPulse)
        {
            progressPulse();
        }
    };
    reportProgress();
    if (!glReady_ || !program_ || !highlightLineVao_ || !highlightLineVbo_ || !outlineTriVao_ || !outlineTriVbo_
        || !highlightPointVao_ || !highlightPointVbo_)
    {
        reportProgress();
        return;
    }

    const auto writeBuffer = [&reportProgress](
        QOpenGLBuffer& buffer,
        const void* source,
        const int byteCount)
    {
        render::upload::writeBufferInProgressChunks(
            0,
            source,
            byteCount,
            [&buffer](const int offset, const void* bytes, const int count)
            {
                buffer.write(offset, bytes, count);
            },
            reportProgress);
    };

    highlightLineVao_->bind();
    highlightLineVbo_->bind();
    const std::optional<int> highlightLineBytesOpt =
        render::upload::safeByteCount(highlightLineVertices_.size() * sizeof(LineVertex));
    if (!highlightLineBytesOpt)
    {
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
    }
    else
    {
        const int highlightLineBytes = *highlightLineBytesOpt;
        reportProgress();
        render::upload::ensureBufferCapacity(*highlightLineVbo_, highlightLineVboAllocatedBytes_, highlightLineBytes);
        reportProgress();
        if (highlightLineBytes > 0)
        {
            writeBuffer(
                *highlightLineVbo_,
                highlightLineVertices_.data(),
                highlightLineBytes);
        }
        else
        {
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
        }
    }

    program_->bind();
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(0, GL_FLOAT, static_cast<int>(offsetof(LineVertex, x)), 3, static_cast<int>(sizeof(LineVertex)));
    program_->enableAttributeArray(3);
    program_->setAttributeBuffer(3, GL_FLOAT, static_cast<int>(offsetof(LineVertex, pickId)), 1, static_cast<int>(sizeof(LineVertex)));
    program_->enableAttributeArray(6);
    program_->setAttributeBuffer(6, GL_FLOAT, static_cast<int>(offsetof(LineVertex, planeX)), 3, static_cast<int>(sizeof(LineVertex)));
    program_->enableAttributeArray(7);
    program_->setAttributeBuffer(7, GL_FLOAT, static_cast<int>(offsetof(LineVertex, planeValid)), 1, static_cast<int>(sizeof(LineVertex)));
    program_->release();

    highlightLineVbo_->release();
    highlightLineVao_->release();
    reportProgress();

    outlineTriVao_->bind();
    outlineTriVbo_->bind();
    const std::optional<int> outlineTriBytesOpt =
        render::upload::safeByteCount(outlineTriVertices_.size() * sizeof(TriVertex));
    if (!outlineTriBytesOpt)
    {
        outlineTriVertexCount_ = 0;
        outlineTriMultiVertexCount_ = 0;
    }
    else
    {
        const int outlineTriBytes = *outlineTriBytesOpt;
        reportProgress();
        render::upload::ensureBufferCapacity(*outlineTriVbo_, outlineTriVboAllocatedBytes_, outlineTriBytes);
        reportProgress();
        if (outlineTriBytes > 0)
        {
            writeBuffer(
                *outlineTriVbo_, outlineTriVertices_.data(), outlineTriBytes);
        }
        else
        {
            outlineTriVertexCount_ = 0;
            outlineTriMultiVertexCount_ = 0;
        }
    }

    program_->bind();
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(0, GL_FLOAT, static_cast<int>(offsetof(TriVertex, x)), 3, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(1);
    program_->setAttributeBuffer(1, GL_FLOAT, static_cast<int>(offsetof(TriVertex, u)), 2, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(2);
    program_->setAttributeBuffer(2, GL_FLOAT, static_cast<int>(offsetof(TriVertex, r)), 4, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(3);
    program_->setAttributeBuffer(3, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickId)), 1, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(4);
    program_->setAttributeBuffer(4, GL_FLOAT, static_cast<int>(offsetof(TriVertex, pickGeo)), 1, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(5);
    program_->setAttributeBuffer(5, GL_FLOAT, static_cast<int>(offsetof(TriVertex, sky)), 1, static_cast<int>(sizeof(TriVertex)));

    program_->enableAttributeArray(6);
    program_->setAttributeBuffer(6, GL_FLOAT, static_cast<int>(offsetof(TriVertex, editorSectorTintR)), 3, static_cast<int>(sizeof(TriVertex)));
    program_->release();

    outlineTriVbo_->release();
    outlineTriVao_->release();
    reportProgress();

    highlightPointVao_->bind();
    highlightPointVbo_->bind();
    const std::optional<int> highlightPointBytesOpt =
        render::upload::safeByteCount(highlightPointPositions_.size() * sizeof(float));
    if (!highlightPointBytesOpt)
    {
        highlightPointVertexCount_ = 0;
    }
    else
    {
        const int highlightPointBytes = *highlightPointBytesOpt;
        reportProgress();
        render::upload::ensureBufferCapacity(*highlightPointVbo_, highlightPointVboAllocatedBytes_, highlightPointBytes);
        reportProgress();
        if (highlightPointBytes > 0)
        {
            writeBuffer(
                *highlightPointVbo_,
                highlightPointPositions_.data(),
                highlightPointBytes);
        }
        else
        {
            highlightPointVertexCount_ = 0;
        }
    }

    program_->bind();
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(0, GL_FLOAT, 0, 3, 3 * static_cast<int>(sizeof(float)));
    program_->release();

    highlightPointVbo_->release();
    highlightPointVao_->release();
    reportProgress();

    uploadArchitecturalLightingPointVertices();
    reportProgress();
}

void GlView::uploadArchitecturalLightingPointVertices()
{
    if (!glReady_ || !program_ || !architecturalLightingPointVao_
        || !architecturalLightingPointVbo_)
    {
        return;
    }

    architecturalLightingPointVao_->bind();
    architecturalLightingPointVbo_->bind();
    const std::optional<int> bytesOpt = render::upload::safeByteCount(
        architecturalLightingPointVertices_.size()
        * sizeof(ArchitecturalLightingPointVertex));
    if (!bytesOpt)
    {
        architecturalLightingPointCount_ = 0;
    }
    else
    {
        const int bytes = *bytesOpt;
        render::upload::ensureBufferCapacity(
            *architecturalLightingPointVbo_,
            architecturalLightingPointVboAllocatedBytes_,
            bytes);
        if (bytes > 0)
        {
            architecturalLightingPointVbo_->write(
                0,
                architecturalLightingPointVertices_.data(),
                bytes);
        }
        else
        {
            architecturalLightingPointCount_ = 0;
        }
    }

    program_->bind();
    program_->enableAttributeArray(0);
    program_->setAttributeBuffer(
        0,
        GL_FLOAT,
        static_cast<int>(offsetof(ArchitecturalLightingPointVertex, x)),
        3,
        static_cast<int>(sizeof(ArchitecturalLightingPointVertex)));
    program_->enableAttributeArray(2);
    program_->setAttributeBuffer(
        2,
        GL_FLOAT,
        static_cast<int>(offsetof(ArchitecturalLightingPointVertex, r)),
        3,
        static_cast<int>(sizeof(ArchitecturalLightingPointVertex)));
    program_->release();

    architecturalLightingPointVbo_->release();
    architecturalLightingPointVao_->release();
}

void GlView::updateSurfaceNormalArrowheadsForView(
    const QVector3D& eye,
    const QVector3D& target)
{
    if (surfaceNormalArrowheadsViewSerial_ == viewSerial_)
    {
        return;
    }
    if (surfaceNormalArrowheads_.empty())
    {
        surfaceNormalArrowheadsViewSerial_ = viewSerial_;
        return;
    }

    // Use GlView's shared, degeneracy-safe camera axes here.  The sprite
    // basis builder used while constructing meshes is intentionally local to
    // gl_view_mesh.cpp and is not available in this upload translation unit.
    const QVector3D viewRight = cameraRight();
    const QVector3D viewUp = cameraUp();
    const bool orthographic = camera_.projectionType()
        == app::ViewportCamera::ProjectionType::Orthographic;
    const QVector3D orthographicToCamera = eye - target;
    for (const SurfaceNormalArrowhead& arrow : surfaceNormalArrowheads_)
    {
        if (arrow.firstVertex < 0
            || static_cast<std::size_t>(arrow.firstVertex + 3)
                >= highlightLineVertices_.size())
        {
            continue;
        }
        const QVector3D toCamera =
            orthographic ? orthographicToCamera : eye - arrow.tip;
        const std::optional<QVector3D> side =
            render::surfaceNormalArrowheadSide(
                arrow.normal, toCamera, viewRight, viewUp);
        if (!side)
        {
            continue;
        }
        const QVector3D headBase =
            arrow.tip - arrow.normal * arrow.length;
        const QVector3D left = headBase + *side * arrow.width;
        const QVector3D right = headBase - *side * arrow.width;
        const auto setPosition = [this](const int index,
                                        const QVector3D& position) {
          LineVertex& vertex =
              highlightLineVertices_[static_cast<std::size_t>(index)];
          vertex.x = position.x();
          vertex.y = position.y();
          vertex.z = position.z();
        };
        setPosition(arrow.firstVertex, arrow.tip);
        setPosition(arrow.firstVertex + 1, left);
        setPosition(arrow.firstVertex + 2, arrow.tip);
        setPosition(arrow.firstVertex + 3, right);
    }

    if (!glReady_ || !highlightLineVbo_
        || surfaceNormalHighlightFirstVertex_ < 0
        || surfaceNormalHighlightVertexCount_ <= 0)
    {
        return;
    }
    const std::size_t first =
        static_cast<std::size_t>(surfaceNormalHighlightFirstVertex_);
    const std::size_t count =
        static_cast<std::size_t>(surfaceNormalHighlightVertexCount_);
    if (first > highlightLineVertices_.size()
        || count > highlightLineVertices_.size() - first)
    {
        return;
    }
    const std::optional<int> offsetBytes = render::upload::safeByteCount(
        first * sizeof(LineVertex));
    const std::optional<int> byteCount = render::upload::safeByteCount(
        count * sizeof(LineVertex));
    if (!offsetBytes || !byteCount
        || *offsetBytes > highlightLineVboAllocatedBytes_
        || *byteCount > highlightLineVboAllocatedBytes_ - *offsetBytes)
    {
        return;
    }
    highlightLineVbo_->bind();
    highlightLineVbo_->write(
        *offsetBytes, highlightLineVertices_.data() + first, *byteCount);
    highlightLineVbo_->release();
    surfaceNormalArrowheadsViewSerial_ = viewSerial_;
}

} // namespace sed
