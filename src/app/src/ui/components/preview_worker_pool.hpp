#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

#include <QFuture>
#include <QString>
#include <QThread>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

namespace sed::ui::detail
{

inline constexpr int kMinPreviewWorkerCount = 1;
inline constexpr int kMaxPreviewWorkerCount = 16;
inline constexpr int kMinPreviewPublishBatchCount = 1;
inline constexpr int kMaxPreviewPublishBatchCount = 8;
inline constexpr int kDefaultPreviewPublishBatchCount = 2;

inline int clampPreviewWorkerCount(const int requested) noexcept
{
    return std::clamp(
        requested,
        kMinPreviewWorkerCount,
        kMaxPreviewWorkerCount);
}

inline int defaultPreviewWorkerCount() noexcept
{
    return std::clamp(QThread::idealThreadCount() - 1, 2, 4);
}

inline std::atomic<int>& previewWorkerCountStorage() noexcept
{
    static std::atomic<int> count{ defaultPreviewWorkerCount() };
    return count;
}

inline std::atomic<int>& previewPublishBatchCountStorage() noexcept
{
    static std::atomic<int> count{ kDefaultPreviewPublishBatchCount };
    return count;
}

inline int previewWorkerCount() noexcept
{
    return previewWorkerCountStorage().load(std::memory_order_relaxed);
}

inline int previewPublishBatchCount() noexcept
{
    return previewPublishBatchCountStorage().load(std::memory_order_relaxed);
}

class PreviewWorkerPoolService final
{
public:
    PreviewWorkerPoolService()
    {
        pool_.setObjectName(QStringLiteral("sed-preview-worker-pool"));
        pool_.setMaxThreadCount(previewWorkerCount());
        pool_.setThreadPriority(QThread::LowPriority);
    }

    QThreadPool& pool() noexcept
    {
        return pool_;
    }

private:
    QThreadPool pool_{};
};

inline QThreadPool& previewWorkerPool() noexcept
{
    static PreviewWorkerPoolService service{};
    return service.pool();
}

inline void setPreviewWorkerCount(const int requested)
{
    const int clamped = clampPreviewWorkerCount(requested);
    previewWorkerCountStorage().store(clamped, std::memory_order_relaxed);
    previewWorkerPool().setMaxThreadCount(clamped);
}

inline void setPreviewPublishBatchCount(const int requested) noexcept
{
    previewPublishBatchCountStorage().store(
        std::clamp(
            requested,
            kMinPreviewPublishBatchCount,
            kMaxPreviewPublishBatchCount),
        std::memory_order_relaxed);
}

template <typename Function, typename... Args>
inline auto runPreviewWorkerTask(Function&& function, Args&&... args)
{
    return QtConcurrent::run(
        &previewWorkerPool(),
        std::forward<Function>(function),
        std::forward<Args>(args)...);
}

struct PreviewWorkerPublicationGuard
{
    std::shared_ptr<std::atomic_bool> canceled{};
    std::uint64_t generation{};
};

inline PreviewWorkerPublicationGuard makePreviewWorkerPublicationGuard(
    const std::uint64_t generation)
{
    return PreviewWorkerPublicationGuard{
        .canceled = std::make_shared<std::atomic_bool>(false),
        .generation = generation,
    };
}

inline void cancelPreviewWorkerPublication(
    const PreviewWorkerPublicationGuard& guard) noexcept
{
    if (guard.canceled)
    {
        guard.canceled->store(true, std::memory_order_release);
    }
}

inline bool previewWorkerMayPublish(
    const PreviewWorkerPublicationGuard& guard,
    const std::uint64_t currentGeneration) noexcept
{
    return guard.canceled
        && !guard.canceled->load(std::memory_order_acquire)
        && guard.generation == currentGeneration;
}

/**
 * A stale worker result must not enter a newer cache generation, but its live
 * owner still needs a completion wake-up so it can queue replacement work.
 */
inline bool previewWorkerCompletionShouldWakeOwner(
    const PreviewWorkerPublicationGuard& guard,
    const std::uint64_t currentGeneration) noexcept
{
    (void)currentGeneration;
    return guard.canceled
        && !guard.canceled->load(std::memory_order_acquire);
}

} // namespace sed::ui::detail
