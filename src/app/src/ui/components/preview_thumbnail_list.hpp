#pragma once

#include "ui/components/preview_worker_pool.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <QListWidget>
#include <QModelIndex>
#include <QPoint>
#include <QRect>
#include <QSignalBlocker>
#include <Qt>

namespace sed::ui::detail
{

inline constexpr int kPreviewThumbStateRole = Qt::UserRole + 214;
inline constexpr int kAssetPreviewSpinIntervalMs = 33;
inline constexpr int kAssetPreviewSpinCatchUpLimitMs = 100;
inline constexpr int kPreviewThumbnailPublishIntervalMs = 40;
inline constexpr int kPreviewThumbnailInteractionQuietMs = 96;
inline constexpr std::int64_t kPreviewThumbnailPublishBudgetNs = 2'000'000;

struct SharedModelThumbLoadTuning
{
    int visibleMargin{ 1 };
    int visibleBatchCount{ kDefaultPreviewPublishBatchCount };
    int offscreenBatchCount{ 0 };
    int startDelayMs{ 16 };
    int visibleDelayMs{ kPreviewThumbnailPublishIntervalMs };
    int offscreenDelayMs{ 120 };
    int maxPrepJobs{ 2 };
};

inline std::int64_t previewThumbnailMonotonicMs() noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

class PreviewThumbnailPublicationPacer final
{
public:
    void noteInteraction() noexcept
    {
        noteInteractionAt(previewThumbnailMonotonicMs());
    }

    void noteInteractionAt(const std::int64_t nowMs) noexcept
    {
        lastInteractionMs_ = nowMs;
    }

    [[nodiscard]] bool interactionActive() const noexcept
    {
        return interactionActiveAt(previewThumbnailMonotonicMs());
    }

    [[nodiscard]] bool interactionActiveAt(
        const std::int64_t nowMs) const noexcept
    {
        if (lastInteractionMs_ == kNoInteraction)
        {
            return false;
        }

        const std::int64_t elapsed = nowMs - lastInteractionMs_;
        return elapsed < 0 || elapsed < kPreviewThumbnailInteractionQuietMs;
    }

    [[nodiscard]] int delayUntilQuiet() const noexcept
    {
        return delayUntilQuietAt(previewThumbnailMonotonicMs());
    }

    [[nodiscard]] int delayUntilQuietAt(
        const std::int64_t nowMs) const noexcept
    {
        if (lastInteractionMs_ == kNoInteraction)
        {
            return 0;
        }

        const std::int64_t elapsed = nowMs - lastInteractionMs_;
        if (elapsed >= kPreviewThumbnailInteractionQuietMs)
        {
            return 0;
        }
        if (elapsed <= 0)
        {
            return kPreviewThumbnailInteractionQuietMs;
        }
        return static_cast<int>(kPreviewThumbnailInteractionQuietMs - elapsed);
    }

private:
    static constexpr std::int64_t kNoInteraction =
        std::numeric_limits<std::int64_t>::min();
    std::int64_t lastInteractionMs_{ kNoInteraction };
};

inline bool previewThumbnailBatchCanContinue(
    const int publishedCount,
    const int configuredLimit,
    const std::int64_t elapsedNs) noexcept
{
    const int safeLimit = std::max(0, configuredLimit);
    if (publishedCount >= safeLimit)
    {
        return false;
    }

    // Guarantee forward progress even if conversion of the first QImage in a
    // batch exceeds the normal GUI budget. Later icons wait for another turn.
    return publishedCount == 0
        || elapsedNs < kPreviewThumbnailPublishBudgetNs;
}

inline bool previewThumbnailQueueNeedsPolling(
    const bool visiblePending,
    const int activeWorkerJobs) noexcept
{
    // Every worker completion explicitly wakes its owning list. Polling while
    // jobs are active only repeats cache/filesystem checks and timer dispatch
    // on the GUI thread. Poll only when visible work remains but no worker can
    // provide a completion signal for it.
    return visiblePending && activeWorkerJobs <= 0;
}

inline std::vector<int> previewThumbnailPriorityRows(
    const int rowBegin,
    const int rowEnd,
    const int currentRow)
{
    std::vector<int> rows{};
    if (rowBegin < 0 || rowEnd < rowBegin)
    {
        return rows;
    }

    rows.reserve(static_cast<std::size_t>(rowEnd - rowBegin + 1));
    if (currentRow >= rowBegin && currentRow <= rowEnd)
    {
        rows.push_back(currentRow);
    }
    for (int row = rowBegin; row <= rowEnd; ++row)
    {
        if (row != currentRow)
        {
            rows.push_back(row);
        }
    }
    return rows;
}

inline int previewThumbnailPageStep(
    const int viewportExtent,
    const int rowExtent) noexcept
{
    const int safeViewportExtent = std::max(1, viewportExtent);
    if (rowExtent <= 0 || safeViewportExtent <= rowExtent)
    {
        return safeViewportExtent;
    }

    const int fullyVisibleRows = std::max(1, safeViewportExtent / rowExtent);
    return std::max(rowExtent, (fullyVisibleRows - 1) * rowExtent);
}

inline constexpr float assetPreviewSpinStepDegrees(
    const float speedDegPerSec,
    const std::int64_t elapsedMs) noexcept
{
    const std::int64_t boundedElapsed = std::clamp<std::int64_t>(
        elapsedMs,
        1,
        kAssetPreviewSpinCatchUpLimitMs);
    return std::max(0.0f, speedDegPerSec)
        * (static_cast<float>(boundedElapsed) / 1000.0f);
}

inline SharedModelThumbLoadTuning sharedModelThumbLoadTuning() noexcept
{
    SharedModelThumbLoadTuning tuning{};
    tuning.visibleBatchCount = previewPublishBatchCount();
    tuning.maxPrepJobs = previewWorkerCount();
    return tuning;
}

enum class PreviewThumbState : int
{
    none = 0,
    loading = 1,
    ready = 2,
    unavailable = 3,
};

/**
 * Publishes fixed-grid thumbnail data without routing DecorationRole through
 * QAbstractItemView::dataChanged(). The picker lists have explicit grid and
 * item sizes, so an icon cannot change geometry; repainting only the existing
 * tile avoids a full delayed-items layout for every completed worker result.
 */
inline void publishPreviewThumbnailItem(
    QListWidget* list,
    QListWidgetItem* item,
    const QIcon& icon,
    const int stateRole,
    const QVariant& state)
{
    if ( !list || !item || item->listWidget() != list || !list->model() )
    {
        return;
    }

    const QRect dirtyRect = list->visualItemRect(item);
    {
        const QSignalBlocker blockModelSignals(list->model());
        item->setIcon(icon);
        item->setData(stateRole, state);
    }

    if ( list->viewport() && dirtyRect.isValid() )
    {
        list->viewport()->update(dirtyRect);
    }
}

class PreviewThumbLoadQueue final
{
public:
    void clear()
    {
        pendingFlags_.clear();
        pendingCount_ = 0;
        scanCursor_ = 0;
        ++generation_;
    }

    void reset(const int rowCount, const std::vector<int>& pendingRows)
    {
        pendingFlags_.assign(static_cast<std::size_t>(std::max(0, rowCount)), std::uint8_t{ 0 });
        pendingCount_ = 0;
        scanCursor_ = 0;
        ++generation_;

        for (const int row : pendingRows)
        {
            markPending(row);
        }
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return pendingFlags_.empty();
    }

    [[nodiscard]] int pendingCount() const noexcept
    {
        return pendingCount_;
    }

    [[nodiscard]] bool hasRow(const int row) const noexcept
    {
        return row >= 0 && row < static_cast<int>(pendingFlags_.size());
    }

    [[nodiscard]] bool isPending(const int row) const noexcept
    {
        return hasRow(row) && pendingFlags_[static_cast<std::size_t>(row)] != 0;
    }

    bool markPending(const int row)
    {
        if (!hasRow(row))
        {
            return false;
        }

        std::uint8_t& flag = pendingFlags_[static_cast<std::size_t>(row)];
        if (flag != 0)
        {
            return false;
        }

        flag = std::uint8_t{ 1 };
        ++pendingCount_;
        return true;
    }

    bool finish(const int row)
    {
        if (!hasRow(row))
        {
            return false;
        }

        std::uint8_t& flag = pendingFlags_[static_cast<std::size_t>(row)];
        if (flag == 0)
        {
            return false;
        }

        flag = std::uint8_t{ 0 };
        if (pendingCount_ > 0)
        {
            --pendingCount_;
        }
        return true;
    }

    int nextScanRow(const int rowCount)
    {
        if (rowCount <= 0)
        {
            return -1;
        }
        if (scanCursor_ >= rowCount)
        {
            scanCursor_ = 0;
        }
        return scanCursor_++;
    }

    [[nodiscard]] std::uint64_t generation() const noexcept
    {
        return generation_;
    }

private:
    std::vector<std::uint8_t> pendingFlags_{};
    int pendingCount_{};
    int scanCursor_{};
    std::uint64_t generation_{};
};

inline bool visiblePreviewListRowRange(
    const QListWidget* list,
    int& rowBegin,
    int& rowEnd,
    const int insetPx = 4,
    int* probeCountForTest = nullptr)
{
    rowBegin = -1;
    rowEnd = -1;
    if (!list || list->count() <= 0 || !list->viewport())
    {
        return false;
    }

    const QRect viewportRect = list->viewport()->rect();
    if (viewportRect.isEmpty())
    {
        return false;
    }

    const int inset = std::max(0, insetPx);
    const int left = std::min(viewportRect.right(), viewportRect.left() + inset);
    const int right = std::max(viewportRect.left(), viewportRect.right() - inset);
    const int top = std::min(viewportRect.bottom(), viewportRect.top() + inset);
    const int bottom = std::max(viewportRect.top(), viewportRect.bottom() - inset);
    const int centerX = viewportRect.center().x();
    const int centerY = viewportRect.center().y();

    auto considerRow = [&](const int row)
    {
        if (row < 0 || row >= list->count())
        {
            return;
        }
        rowBegin = rowBegin < 0 ? row : std::min(rowBegin, row);
        rowEnd = rowEnd < 0 ? row : std::max(rowEnd, row);
    };

    auto probePoint = [&](const QPoint& point)
    {
        if (probeCountForTest)
        {
            ++*probeCountForTest;
        }
        const QModelIndex index = list->indexAt(point);
        if (index.isValid())
        {
            considerRow(index.row());
        }
    };

    const std::array<QPoint, 9> samplePoints = {
        QPoint(left, top),
        QPoint(centerX, top),
        QPoint(right, top),
        QPoint(left, centerY),
        QPoint(centerX, centerY),
        QPoint(right, centerY),
        QPoint(left, bottom),
        QPoint(centerX, bottom),
        QPoint(right, bottom),
    };
    for (const QPoint& point : samplePoints)
    {
        probePoint(point);
    }

    // Icon-mode gaps can make every corner/center sample miss. Probe a grid
    // whose cost is bounded by the viewport instead of calling visualItemRect
    // for every resource in a potentially multi-thousand-row model list.
    const QSize grid = list->gridSize();
    const int stepX = std::max(
        1,
        std::max(
            grid.width() > 0 ? grid.width() / 2 : 1,
            std::max(1, viewportRect.width() / 8)));
    const int stepY = std::max(
        1,
        std::max(
            grid.height() > 0 ? grid.height() / 2 : 1,
            std::max(1, viewportRect.height() / 8)));
    for (int y = top; y <= bottom; y += stepY)
    {
        for (int x = left; x <= right; x += stepX)
        {
            probePoint(QPoint(x, y));
        }
        if ((right - left) % stepX != 0)
        {
            probePoint(QPoint(right, y));
        }
    }
    if ((bottom - top) % stepY != 0)
    {
        for (int x = left; x <= right; x += stepX)
        {
            probePoint(QPoint(x, bottom));
        }
        if ((right - left) % stepX != 0)
        {
            probePoint(QPoint(right, bottom));
        }
    }

    return rowBegin >= 0 && rowEnd >= 0 && rowBegin <= rowEnd;
}

} // namespace sed::ui::detail
