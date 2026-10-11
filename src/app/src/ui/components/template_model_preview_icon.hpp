#pragma once

#include "gl_view.hpp"
#include "sed/assets/3do.hpp"
#include "sed/assets/cmp.hpp"
#include "sed/assets/mat.hpp"
#include "sed/assets/resource_locator.hpp"
#include "sed/assets/spr.hpp"
#include "sed/assets/tpl.hpp"
#include "sed/image.hpp"
#include "sed/math_utils.hpp"
#include "sed/text.hpp"
#include "ui/components/preview_resource_revision_utils.hpp"
#include "ui/components/preview_thumbnail_list.hpp"
#include "ui/components/bounded_two_queue_cache.hpp"
#include "ui/components/thing_preview_render_utils.hpp"
#include "render/texture/texture_alpha_utils.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <QApplication>
#include <QColor>
#include <QEventLoop>
#include <QFont>
#include <QFutureWatcher>
#include <QIcon>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>
#include <QRadialGradient>
#include <QString>
#include <QSize>
#include <QThread>
#include <QTransform>
#include <QVector3D>
#include <QtConcurrent/QtConcurrentRun>

namespace sed::ui::detail
{

// Asset read failures (for example a truncated mounted archive entry) are
// unavailable previews, just like decode failures. Keep them inside the UI
// and worker boundary so each caller can publish its normal negative cache.
inline std::optional<std::vector<std::byte>> tryReadPreviewResource(
    const assets::ResourceLocator* resources,
    const std::string_view name, const std::string_view extension)
{
    if (!resources) return std::nullopt;
    try { return resources->tryReadBinaryWithExt(name, extension); }
    catch (const std::exception&) { return std::nullopt; }
}

inline std::size_t previewImageMemoryWeight(const QImage& image) noexcept
{
    return image.isNull()
        ? 1u
        : static_cast<std::size_t>(std::max<qsizetype>(1, image.sizeInBytes()));
}

inline std::size_t previewModelMemoryWeight(
    const std::shared_ptr<const assets::Model3do>& model) noexcept
{
    if (!model)
    {
        return 1u;
    }

    std::size_t bytes = sizeof(assets::Model3do);
    for (const std::string& material : model->materials)
    {
        bytes += sizeof(std::string) + material.capacity();
    }
    bytes += model->meshSourceIndices.capacity() * sizeof(std::int32_t);
    for (const assets::Model3doMesh& mesh : model->meshes)
    {
        bytes += sizeof(assets::Model3doMesh) + mesh.name.capacity();
        bytes += mesh.vertices.capacity() * sizeof(assets::Model3doVertex);
        bytes += mesh.texVertices.capacity() * sizeof(assets::Model3doTexVertex);
        bytes += mesh.vertexNormals.capacity() * sizeof(TVector);
        bytes += mesh.faceNormals.capacity() * sizeof(TVector);
        bytes += mesh.faces.capacity() * sizeof(assets::Model3doFace);
        for (const assets::Model3doFace& face : mesh.faces)
        {
            bytes += face.vertices.capacity() * sizeof(std::int32_t);
            bytes += face.texVertices.capacity() * sizeof(std::int32_t);
        }
    }
    for (const assets::Model3doNode& node : model->nodes)
    {
        bytes += sizeof(assets::Model3doNode) + node.name.capacity();
    }
    return std::max<std::size_t>(bytes, 1u);
}

inline std::size_t previewSpriteMemoryWeight(
    const std::shared_ptr<const assets::SprFile>& sprite) noexcept
{
    return sprite
        ? sizeof(assets::SprFile) + sprite->material.capacity()
        : 1u;
}

enum class PreviewMaterialTransparency : std::uint8_t
{
    opaque = 0,
    cutout,
    blended,
};

struct PreviewMaterialTextureSource
{
    QImage image{};
    std::int32_t alphaBpp{};
    QVector3D solidColor{1.0f, 1.0f, 1.0f};
    std::optional<ClassicSolidLightRamp> classicSolidLightRamp{};
};

struct PreviewCmpSource
{
    assets::CmpPalette palette{};
    std::optional<assets::CmpFile> complete{};
};

inline std::size_t previewMaterialTextureMemoryWeight(
    const PreviewMaterialTextureSource& source) noexcept
{
    return previewImageMemoryWeight(source.image);
}

inline std::optional<PreviewMaterialTextureSource> loadPreviewMaterialTextureSource(
    const assets::ResourceLocator* resources,
    std::string_view cmpName,
    std::string_view materialName);

inline QPixmap makePreviewLoadingPixmap(const QSize& targetSize)
{
    const int width = std::max(48, targetSize.width() > 0 ? targetSize.width() : 96);
    const int height = std::max(48, targetSize.height() > 0 ? targetSize.height() : 96);

    QPixmap pix(width, height);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF card(1.5, 1.5, width - 3.0, height - 3.0);
    p.setPen(QPen(QColor(255, 255, 255, 28), 1.0));
    p.setBrush(QColor(34, 37, 42, 230));
    p.drawRoundedRect(card, 8.0, 8.0);

    const QRectF previewArea = card.adjusted(10.0, 10.0, -10.0, -24.0);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(54, 58, 66, 190));
    p.drawRoundedRect(previewArea, 6.0, 6.0);

    const qreal barHeight = std::clamp(previewArea.height() * 0.09, 5.0, 8.0);
    const QRectF track(
        previewArea.left() + std::max(8.0, previewArea.width() * 0.10),
        previewArea.bottom() - barHeight - std::max(8.0, previewArea.height() * 0.08),
        previewArea.width() - (std::max(8.0, previewArea.width() * 0.10) * 2.0),
        barHeight);
    p.setBrush(QColor(18, 21, 26, 220));
    p.drawRoundedRect(track, track.height() * 0.5, track.height() * 0.5);

    const QRectF fill(track.left() + 1.0, track.top() + 1.0, std::max(8.0, track.width() * 0.58), std::max(2.0, track.height() - 2.0));
    QLinearGradient grad(fill.topLeft(), fill.topRight());
    grad.setColorAt(0.0, QColor(88, 180, 255, 235));
    grad.setColorAt(1.0, QColor(115, 220, 175, 235));
    p.setBrush(grad);
    p.drawRoundedRect(fill, fill.height() * 0.5, fill.height() * 0.5);

    p.setPen(QColor(220, 225, 232, 180));
    QFont font = p.font();
    font.setPointSizeF(std::clamp(static_cast<qreal>(width) * 0.10, 7.0, 10.0));
    font.setBold(true);
    p.setFont(font);
    p.drawText(
        QRectF(card.left(), card.bottom() - 18.0, card.width(), 14.0),
        Qt::AlignHCenter | Qt::AlignVCenter,
        QStringLiteral("Loading"));

    return pix;
}

inline QPixmap makePreviewUnavailablePixmap(const QSize& targetSize)
{
    const int width = std::max(48, targetSize.width() > 0 ? targetSize.width() : 96);
    const int height = std::max(48, targetSize.height() > 0 ? targetSize.height() : 96);

    QPixmap pix(width, height);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF card(1.5, 1.5, width - 3.0, height - 3.0);
    p.setPen(QPen(QColor(255, 255, 255, 28), 1.0));
    p.setBrush(QColor(36, 38, 44, 228));
    p.drawRoundedRect(card, 8.0, 8.0);

    const QRectF iconArea = card.adjusted(14.0, 12.0, -14.0, -28.0);
    p.setPen(QPen(QColor(210, 214, 220, 120), 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(iconArea, 7.0, 7.0);

    p.setPen(QPen(QColor(235, 120, 120, 210), 3.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(iconArea.topLeft() + QPointF(4.0, 4.0), iconArea.bottomRight() - QPointF(4.0, 4.0));
    p.drawLine(iconArea.topRight() + QPointF(-4.0, 4.0), iconArea.bottomLeft() + QPointF(4.0, -4.0));

    p.setPen(QColor(220, 225, 232, 176));
    QFont font = p.font();
    font.setPointSizeF(std::clamp(static_cast<qreal>(width) * 0.095, 7.0, 10.0));
    font.setBold(true);
    p.setFont(font);
    p.drawText(
        QRectF(card.left(), card.bottom() - 20.0, card.width(), 14.0),
        Qt::AlignHCenter | Qt::AlignVCenter,
        QStringLiteral("No preview"));

    return pix;
}

inline QPixmap makeTemplatePreviewPixmap(const QImage& source, const QSize& targetSize)
{
    if (source.isNull())
    {
        return {};
    }

    const int width = std::max(64, targetSize.width());
    const int height = std::max(64, targetSize.height());
    QImage canvas(width, height, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(QColor(18, 20, 28));

    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    constexpr int cell = 12;
    for (int y = 0; y < height; y += cell)
    {
        for (int x = 0; x < width; x += cell)
        {
            const bool odd = ((x / cell) + (y / cell)) % 2 != 0;
            p.fillRect(QRect(x, y, cell, cell), odd ? QColor(44, 46, 50) : QColor(34, 36, 40));
        }
    }

    const int pad = std::max(10, std::min(width, height) / 14);
    const QSize fitSize(std::max(1, width - 2 * pad), std::max(1, height - 2 * pad));
    const QImage scaled = source.scaled(fitSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QPoint topLeft((width - scaled.width()) / 2, (height - scaled.height()) / 2);
    p.drawImage(topLeft, scaled);

    p.setPen(QPen(QColor(0, 0, 0, 120), 1.0));
    p.drawRect(canvas.rect().adjusted(0, 0, -1, -1));
    return QPixmap::fromImage(canvas);
}

inline BoundedTwoQueueCache<QImage>& sharedTemplateModelPreviewImageCache()
{
    static BoundedTwoQueueCache<QImage> cache(
        kRenderedModelPreviewBudget,
        previewImageMemoryWeight);
    return cache;
}

inline std::unordered_map<std::string, std::string>& sharedTemplateModelPreviewImageHintCache()
{
    static std::unordered_map<std::string, std::string> cache{};
    return cache;
}

inline std::mutex& sharedTemplateModelPreviewImageCacheMutex()
{
    static std::mutex mutex{};
    return mutex;
}

inline std::optional<QImage> findSharedTemplateModelPreviewImage(const std::string_view cacheKey)
{
    std::lock_guard<std::mutex> lock(sharedTemplateModelPreviewImageCacheMutex());
    QImage image{};
    if (!sharedTemplateModelPreviewImageCache().find(cacheKey, image)
        || image.isNull())
    {
        return std::nullopt;
    }
    return image;
}

inline void storeSharedTemplateModelPreviewImage(const std::string_view cacheKey, const QImage& image)
{
    if (image.isNull())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(sharedTemplateModelPreviewImageCacheMutex());
    sharedTemplateModelPreviewImageCache().insertOrAssign(
        std::string(cacheKey),
        image.copy());
}

inline void clearSharedTemplateModelPreviewImageCache()
{
    std::lock_guard<std::mutex> lock(sharedTemplateModelPreviewImageCacheMutex());
    sharedTemplateModelPreviewImageCache().clear();
    sharedTemplateModelPreviewImageHintCache().clear();
}

inline std::unordered_map<std::string, std::optional<PreviewCmpSource>>& sharedPreviewCmpCache()
{
    static std::unordered_map<std::string, std::optional<PreviewCmpSource>> cache{};
    return cache;
}

inline std::mutex& sharedPreviewCmpCacheMutex()
{
    static std::mutex mutex{};
    return mutex;
}

inline BoundedTwoQueueCache<PreviewMaterialTextureSource>& sharedPreviewMaterialImageCache()
{
    static BoundedTwoQueueCache<PreviewMaterialTextureSource> cache(
        kMaterialPreviewBudget,
        previewMaterialTextureMemoryWeight);
    return cache;
}

inline std::mutex& sharedPreviewMaterialImageCacheMutex()
{
    static std::mutex mutex{};
    return mutex;
}

inline std::optional<QImage> findSharedPreviewMaterialImage(const std::string_view cacheKey)
{
    std::lock_guard<std::mutex> lock(sharedPreviewMaterialImageCacheMutex());
    PreviewMaterialTextureSource source{};
    if (!sharedPreviewMaterialImageCache().find(cacheKey, source)
        || source.image.isNull())
    {
        return std::nullopt;
    }
    return source.image;
}

inline void storeSharedPreviewMaterialImage(const std::string_view cacheKey, const QImage& image)
{
    if (image.isNull())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(sharedPreviewMaterialImageCacheMutex());
    sharedPreviewMaterialImageCache().insertOrAssign(
        std::string(cacheKey),
        PreviewMaterialTextureSource{
            .image = image.copy(),
            .alphaBpp = render::inferTextureAlphaBpp(image),
        });
}

inline BoundedTwoQueueCache<QImage>& sharedPreviewSpriteImageCache()
{
    static BoundedTwoQueueCache<QImage> cache(
        kSpriteImagePreviewBudget,
        previewImageMemoryWeight);
    return cache;
}

inline std::mutex& sharedPreviewSpriteImageCacheMutex()
{
    static std::mutex mutex{};
    return mutex;
}

inline BoundedTwoQueueCache<std::shared_ptr<const assets::SprFile>>& sharedPreviewSpriteCache()
{
    static BoundedTwoQueueCache<std::shared_ptr<const assets::SprFile>> cache(
        kParsedSpritePreviewBudget,
        previewSpriteMemoryWeight);
    return cache;
}

inline std::mutex& sharedPreviewSpriteCacheMutex()
{
    static std::mutex mutex{};
    return mutex;
}

inline std::atomic<std::uint64_t>& sharedPreviewCacheEpoch()
{
    static std::atomic<std::uint64_t> epoch{ 1u };
    return epoch;
}

inline std::uint64_t currentSharedPreviewCacheEpoch() noexcept
{
    return sharedPreviewCacheEpoch().load(std::memory_order_acquire);
}

inline std::string previewResourceScopedCacheKey(
    const assets::ResourceLocator* resources,
    const std::string_view first,
    const std::string_view second = {})
{
    return std::string("e=")
        + std::to_string(currentSharedPreviewCacheEpoch())
        + "|"
        + previewResourceCacheIdentity(resources, first, second);
}

inline std::atomic<int>& sharedThumbnailRenderActiveCount()
{
    static std::atomic<int> count{ 0 };
    return count;
}

inline bool sharedThumbnailRenderBusy() noexcept
{
    return sharedThumbnailRenderActiveCount().load(std::memory_order_relaxed) > 0;
}

inline BoundedTwoQueueCache<std::shared_ptr<const assets::Model3do>>& sharedPreviewModelCache()
{
    static BoundedTwoQueueCache<std::shared_ptr<const assets::Model3do>> cache(
        kParsedModelPreviewBudget,
        previewModelMemoryWeight);
    return cache;
}

inline std::mutex& sharedPreviewModelCacheMutex()
{
    static std::mutex mutex{};
    return mutex;
}

inline std::unordered_map<std::string, std::string>& sharedPreviewModelCacheHintCache()
{
    static std::unordered_map<std::string, std::string> cache{};
    return cache;
}

inline std::unordered_map<std::string, std::optional<::sed::ThingBox>>& sharedPreviewModelBoxCache()
{
    static std::unordered_map<std::string, std::optional<::sed::ThingBox>> cache{};
    return cache;
}

inline std::mutex& sharedPreviewModelBoxCacheMutex()
{
    static std::mutex mutex{};
    return mutex;
}

inline std::unordered_map<std::string, std::string>& sharedPreviewModelBoxHintCache()
{
    static std::unordered_map<std::string, std::string> cache{};
    return cache;
}

inline std::unordered_set<std::string>& sharedPreviewModelRenderReadyCache()
{
    static std::unordered_set<std::string> cache{};
    return cache;
}

inline std::mutex& sharedPreviewModelRenderReadyCacheMutex()
{
    static std::mutex mutex{};
    return mutex;
}

inline std::string sharedPreviewModelHintKey(
    const assets::ResourceLocator* resources,
    const std::string_view kind,
    const std::string_view modelName)
{
    return std::string(kind)
        + ":epoch="
        + std::to_string(currentSharedPreviewCacheEpoch())
        + "|locator="
        + std::to_string(resources ? resources->previewCacheIdentity() : 0)
        + "|model="
        + ::sed::text::upperAscii(modelName);
}

inline std::string sharedPreviewModelHintKeyForCacheKey(
    const assets::ResourceLocator* resources,
    const std::string_view kind,
    const std::string_view modelName,
    const std::string_view cacheKey)
{
    const std::size_t epochEnd = cacheKey.find('|');
    if (cacheKey.starts_with("e=") && epochEnd > 2u)
    {
        return std::string(kind)
            + ":epoch="
            + std::string(cacheKey.substr(2u, epochEnd - 2u))
            + "|locator="
            + std::to_string(resources ? resources->previewCacheIdentity() : 0)
            + "|model="
            + ::sed::text::upperAscii(modelName);
    }
    return sharedPreviewModelHintKey(resources, kind, modelName);
}

inline bool findCachedSharedPreviewModel(
    const assets::ResourceLocator* resources,
    const std::string_view modelName,
    std::shared_ptr<const assets::Model3do>& model)
{
    if ( !resources ) return false;
    const std::string hintKey = sharedPreviewModelHintKey(
        resources, "parsed-model-hint", modelName);
    std::lock_guard<std::mutex> lock(sharedPreviewModelCacheMutex());
    const auto& hints = sharedPreviewModelCacheHintCache();
    const auto hint = hints.find(hintKey);
    return hint != hints.end()
        && sharedPreviewModelCache().find(hint->second, model);
}

inline std::shared_ptr<const assets::Model3do> loadSharedPreviewModel(
    const assets::ResourceLocator* resources,
    const std::string_view modelName)
{
    if (!resources)
    {
        return {};
    }

    std::shared_ptr<const assets::Model3do> cachedModel{};
    if (findCachedSharedPreviewModel(resources, modelName, cachedModel))
    {
        return cachedModel;
    }

    const std::string cacheKey = previewResourceScopedCacheKey(
        resources,
        modelName,
        previewResourceRevisionToken(resources, modelName, ".3do"));
    const std::string hintKey = sharedPreviewModelHintKeyForCacheKey(
        resources, "parsed-model-hint", modelName, cacheKey);

    std::shared_ptr<const assets::Model3do> model{};
    {
        // Distinct hint scopes may resolve the same exact disk revision. Reuse
        // its authoritative owner before paying for another read and parse.
        std::lock_guard<std::mutex> lock(sharedPreviewModelCacheMutex());
        if ( sharedPreviewModelCache().find(cacheKey, model) )
        {
            auto& hints = sharedPreviewModelCacheHintCache();
            if ( hints.size() >= 4096u && hints.find(hintKey) == hints.end() )
            {
                hints.erase(hints.begin());
            }
            hints[hintKey] = cacheKey;
            return model;
        }
    }
    try
    {
        if (const auto data = resources->tryReadBinaryWithExt(modelName, ".3do"))
            model = std::make_shared<assets::Model3do>(
                assets::loadFrom3DO(std::span<const std::byte>(*data)));
    }
    catch (const std::exception&)
    {
    }

    {
        std::lock_guard<std::mutex> lock(sharedPreviewModelCacheMutex());
        sharedPreviewModelCache().insertOrAssign(cacheKey, model);
        auto& hints = sharedPreviewModelCacheHintCache();
        if (hints.size() >= 4096u && hints.find(hintKey) == hints.end())
        {
            hints.erase(hints.begin());
        }
        hints[hintKey] = cacheKey;
    }

    return model;
}

inline std::optional<::sed::ThingBox> previewBoxForModel(const assets::Model3do& model)
{
    bool haveAny = false;
    ::sed::ThingBox box{};
    for (const assets::Model3doMesh& mesh : model.meshes)
    {
        for (const assets::Model3doVertex& vertex : mesh.vertices)
        {
            if (!haveAny)
            {
                box.Set(vertex.x, vertex.y, vertex.z, vertex.x, vertex.y, vertex.z);
                haveAny = true;
                continue;
            }

            box.x1 = std::min(box.x1, static_cast<double>(vertex.x));
            box.y1 = std::min(box.y1, static_cast<double>(vertex.y));
            box.z1 = std::min(box.z1, static_cast<double>(vertex.z));
            box.x2 = std::max(box.x2, static_cast<double>(vertex.x));
            box.y2 = std::max(box.y2, static_cast<double>(vertex.y));
            box.z2 = std::max(box.z2, static_cast<double>(vertex.z));
        }
    }

    if (!haveAny)
    {
        return std::nullopt;
    }
    return box;
}

inline bool findSharedPreviewModelBox(
    const assets::ResourceLocator* resources,
    const std::string_view modelName,
    std::optional<::sed::ThingBox>& box)
{
    if ( !resources ) return false;
    const std::string hintKey = sharedPreviewModelHintKey(
        resources, "model-box-hint", modelName);
    std::lock_guard<std::mutex> lock(sharedPreviewModelBoxCacheMutex());
    const auto& hints = sharedPreviewModelBoxHintCache();
    const auto hint = hints.find(hintKey);
    if (hint == hints.end())
    {
        return false;
    }
    const auto& cache = sharedPreviewModelBoxCache();
    const auto it = cache.find(hint->second);
    if (it == cache.end())
    {
        return false;
    }
    box = it->second;
    return true;
}

inline void storeSharedPreviewModelBox(
    const assets::ResourceLocator* resources,
    const std::string_view modelName,
    const std::optional<::sed::ThingBox>& box)
{
    const std::string cacheKey = previewResourceScopedCacheKey(
        resources,
        modelName,
        previewResourceRevisionToken(resources, modelName, ".3do"));
    const std::string hintKey = sharedPreviewModelHintKeyForCacheKey(
        resources, "model-box-hint", modelName, cacheKey);
    std::lock_guard<std::mutex> lock(sharedPreviewModelBoxCacheMutex());
    auto& cache = sharedPreviewModelBoxCache();
    if (cache.size() >= 256u && cache.find(cacheKey) == cache.end())
    {
        cache.erase(cache.begin());
    }
    cache[cacheKey] = box;
    auto& hints = sharedPreviewModelBoxHintCache();
    if (hints.size() >= 4096u && hints.find(hintKey) == hints.end())
    {
        hints.erase(hints.begin());
    }
    hints[hintKey] = cacheKey;
}

inline std::string sharedPreviewModelRenderReadyKey(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view modelName,
    const assets::Model3do& model)
{
    return previewResourceScopedCacheKey(
        resources,
        std::string("render-ready|") + ::sed::text::upperAscii(modelName),
        previewModelDependencyRevisionToken(
            resources, cmpName, modelName, model.materials));
}

inline void markSharedPreviewModelRenderReady(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view modelName,
    const assets::Model3do& model)
{
    const std::string key = sharedPreviewModelRenderReadyKey(
        resources, cmpName, modelName, model);
    std::lock_guard<std::mutex> lock(
        sharedPreviewModelRenderReadyCacheMutex());
    auto& cache = sharedPreviewModelRenderReadyCache();
    if (cache.size() >= 256u && cache.find(key) == cache.end())
    {
        cache.erase(cache.begin());
    }
    cache.insert(key);
}

inline bool isSharedPreviewModelRenderReady(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view modelName,
    const assets::Model3do& model)
{
    const std::string key = sharedPreviewModelRenderReadyKey(
        resources, cmpName, modelName, model);
    std::lock_guard<std::mutex> lock(
        sharedPreviewModelRenderReadyCacheMutex());
    return sharedPreviewModelRenderReadyCache().contains(key);
}

inline void clearSharedPreviewModelCaches()
{
    // The epoch keeps prep work that started before this invalidation in an
    // unreachable cache namespace even if it finishes after the clear.
    sharedPreviewCacheEpoch().fetch_add(1u, std::memory_order_acq_rel);
    {
        std::lock_guard<std::mutex> lock(sharedPreviewModelCacheMutex());
        sharedPreviewModelCache().clear();
        sharedPreviewModelCacheHintCache().clear();
    }
    {
        std::lock_guard<std::mutex> lock(sharedPreviewModelBoxCacheMutex());
        sharedPreviewModelBoxCache().clear();
        sharedPreviewModelBoxHintCache().clear();
    }
    {
        std::lock_guard<std::mutex> lock(
            sharedPreviewModelRenderReadyCacheMutex());
        sharedPreviewModelRenderReadyCache().clear();
    }
    clearSharedTemplateModelPreviewImageCache();
}

inline void clearSharedPreviewAssetCaches()
{
    clearSharedPreviewModelCaches();
    {
        std::lock_guard<std::mutex> lock(sharedPreviewCmpCacheMutex());
        sharedPreviewCmpCache().clear();
    }
    {
        std::lock_guard<std::mutex> lock(sharedPreviewMaterialImageCacheMutex());
        sharedPreviewMaterialImageCache().clear();
    }
    {
        std::lock_guard<std::mutex> lock(sharedPreviewSpriteImageCacheMutex());
        sharedPreviewSpriteImageCache().clear();
    }
    {
        std::lock_guard<std::mutex> lock(sharedPreviewSpriteCacheMutex());
        sharedPreviewSpriteCache().clear();
    }
}

inline std::optional<::sed::ThingBox> loadSharedPreviewModelBox(
    const assets::ResourceLocator* resources,
    const std::string_view modelName)
{
    // Keep one key for the whole operation. Recomputing it after parsing could
    // publish stale geometry into a newer invalidation epoch.
    const std::string cacheKey = previewResourceScopedCacheKey(
        resources,
        modelName,
        previewResourceRevisionToken(resources, modelName, ".3do"));
    const std::string hintKey = sharedPreviewModelHintKeyForCacheKey(
        resources, "model-box-hint", modelName, cacheKey);
    {
        std::lock_guard<std::mutex> lock(sharedPreviewModelBoxCacheMutex());
        const auto& cache = sharedPreviewModelBoxCache();
        if (const auto it = cache.find(cacheKey); it != cache.end())
        {
            return it->second;
        }
    }

    const std::shared_ptr<const assets::Model3do> model = loadSharedPreviewModel(resources, modelName);
    const std::optional<::sed::ThingBox> box = model ? previewBoxForModel(*model) : std::nullopt;
    {
        std::lock_guard<std::mutex> lock(sharedPreviewModelBoxCacheMutex());
        auto& cache = sharedPreviewModelBoxCache();
        if (cache.size() >= 256u && cache.find(cacheKey) == cache.end())
        {
            cache.erase(cache.begin());
        }
        cache[cacheKey] = box;
        auto& hints = sharedPreviewModelBoxHintCache();
        if (hints.size() >= 4096u && hints.find(hintKey) == hints.end())
        {
            hints.erase(hints.begin());
        }
        hints[hintKey] = cacheKey;
    }
    return box;
}

inline bool renderAndStoreSharedModelPreviewImage(
    const assets::ResourceLocator* resources,
    std::string_view cmpName,
    std::string_view modelName,
    const QSize& iconSize,
    bool wireframe,
    std::uint64_t renderEpoch);

template <typename ReadyFn>
// ReadyFn receives true only when the completed job still belongs to the
// current preview-cache generation. A false value is a stale completion wake:
// callers must not publish its result, but should schedule replacement work.
inline bool queueSharedPreviewModelBoxPrep(
    QObject* owner,
    std::shared_ptr<const assets::ResourceLocator> resources,
    const std::string& modelName,
    const std::string& cmpName,
    const QSize& iconSize,
    const bool wireframe,
    std::unordered_set<std::string>& inFlight,
    int& activeJobs,
    ReadyFn&& onReady)
{
    if (!owner || !resources)
    {
        return false;
    }
    if (activeJobs >= previewWorkerCount())
    {
        return false;
    }

    // Per-owner in-flight deduplication needs only the immutable cache epoch
    // and request identity. Exact 3DO/CMP/MAT revisions are resolved once by
    // the worker when it publishes the authoritative cache key; probing them
    // here would put filesystem and archive work back on the GUI thread.
    const std::string prepKey = std::string("preview-prep:epoch=")
        + std::to_string(currentSharedPreviewCacheEpoch())
        + "|locator=" + std::to_string(resources->previewCacheIdentity())
        + "|model=" + ::sed::text::upperAscii(modelName)
        + "|cmp=" + ::sed::text::upperAscii(cmpName)
        + "|wireframe=" + std::to_string(wireframe ? 1 : 0)
        + "|size=" + std::to_string(iconSize.width())
        + "x" + std::to_string(iconSize.height());
    if (!inFlight.insert(prepKey).second)
    {
        return false;
    }

    PreviewWorkerPublicationGuard publicationGuard{};
    std::unique_ptr<QFutureWatcher<std::optional<::sed::ThingBox>>> pendingWatcher;
    ++activeJobs;
    try
    {
        publicationGuard = makePreviewWorkerPublicationGuard(currentSharedPreviewCacheEpoch());
        pendingWatcher = std::make_unique<QFutureWatcher<std::optional<::sed::ThingBox>>>(owner);
        auto* watcher = pendingWatcher.get();
        // QObject emits destroyed before deleting its children, so a pending
        // watcher's context is alive for cancellation. A completed watcher's
        // deletion removes this connection and guard from a long-lived owner.
        QObject::connect(owner, &QObject::destroyed, watcher,
            [publicationGuard]()
            {
                cancelPreviewWorkerPublication(publicationGuard);
            });
        QObject::connect(watcher, &QFutureWatcher<std::optional<::sed::ThingBox>>::finished, owner, [watcher, prepKey, &inFlight, &activeJobs, publicationGuard, callback = std::forward<ReadyFn>(onReady)]() mutable
            {
                inFlight.erase(prepKey);
                if (activeJobs > 0)
                {
                    --activeJobs;
                }
                watcher->deleteLater();
                if (previewWorkerCompletionShouldWakeOwner(
                        publicationGuard, currentSharedPreviewCacheEpoch()))
                {
                    callback(previewWorkerMayPublish(
                        publicationGuard, currentSharedPreviewCacheEpoch()));
                }
            });
        // The locator is an immutable snapshot of one published resource
        // generation. Keep it alive in the worker instead of borrowing a UI-owned
        // ResourceLocator that Open/resource reload may replace while parsing.
        watcher->setFuture(runPreviewWorkerTask([
            resources = std::move(resources),
            modelNameCopy = modelName,
            cmpNameCopy = cmpName,
            iconSize,
            wireframe,
            publicationGuard]()
            {
                if (!previewWorkerMayPublish(
                        publicationGuard, currentSharedPreviewCacheEpoch()))
                {
                    return std::optional<::sed::ThingBox>{};
                }
                const std::optional<::sed::ThingBox> box =
                    loadSharedPreviewModelBox(resources.get(), modelNameCopy);
                std::shared_ptr<const assets::Model3do> model{};
                if (box
                    && previewWorkerMayPublish(
                        publicationGuard, currentSharedPreviewCacheEpoch())
                    && findCachedSharedPreviewModel(resources.get(), modelNameCopy, model)
                    && model)
                {
                    for (const std::string& material : model->materials)
                    {
                        (void)loadPreviewMaterialTextureSource(
                            resources.get(), cmpNameCopy, material);
                    }
                    if (previewWorkerMayPublish(
                            publicationGuard, currentSharedPreviewCacheEpoch())
                        && renderAndStoreSharedModelPreviewImage(
                            resources.get(),
                            cmpNameCopy,
                            modelNameCopy,
                            iconSize,
                            wireframe,
                            publicationGuard.generation))
                    {
                        markSharedPreviewModelRenderReady(
                            resources.get(), cmpNameCopy, modelNameCopy, *model);
                    }
                }
                return box;
            }));
        // Completion is delivered through the watcher's queued callout. Setup
        // never processes events, so ownership/accounting transfer together.
        pendingWatcher.release();
    }
    catch (...)
    {
        // Task launch may have succeeded before watcher attachment failed.
        // Reject publication first, then disconnect/delete the incomplete
        // watcher so no later completion can retire this reservation twice.
        cancelPreviewWorkerPublication(publicationGuard);
        pendingWatcher.reset();
        inFlight.erase(prepKey);
        --activeJobs;
        throw;
    }
    return true;
}

inline std::shared_ptr<const assets::SprFile> loadSharedPreviewSprite(
    const assets::ResourceLocator* resources,
    const std::string_view spriteName)
{
    if (!resources)
    {
        return {};
    }

    const std::string cacheKey = previewResourceScopedCacheKey(
        resources,
        spriteName,
        previewResourceRevisionToken(resources, spriteName, ".spr"));
    {
        std::lock_guard<std::mutex> lock(sharedPreviewSpriteCacheMutex());
        std::shared_ptr<const assets::SprFile> cached{};
        if (sharedPreviewSpriteCache().find(cacheKey, cached))
        {
            return cached;
        }
    }

    std::shared_ptr<const assets::SprFile> sprite{};
    if (const auto data = tryReadPreviewResource(resources, spriteName, ".spr"))
    {
        try
        {
            std::string buffer{};
            buffer.resize(data->size());
            std::copy_n(
                reinterpret_cast<const char*>(data->data()),
                static_cast<std::ptrdiff_t>(data->size()),
                buffer.data());
            std::istringstream stream(buffer, std::ios::binary);
            sprite = std::make_shared<assets::SprFile>(assets::loadFromSPR(stream));
        }
        catch (const std::exception&)
        {
        }
    }

    {
        std::lock_guard<std::mutex> lock(sharedPreviewSpriteCacheMutex());
        sharedPreviewSpriteCache().insertOrAssign(cacheKey, sprite);
    }

    return sprite;
}

/** Complete dependency revision for a decoded SPR preview image. */
inline std::string sharedPreviewSpriteDependencyRevisionToken(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view spriteName)
{
    std::string token("SPR_PREVIEW_V2");
    appendPreviewRevisionPart(
        token,
        "SPR",
        ::sed::text::upperAscii(spriteName)
            + "|"
            + previewResourceRevisionToken(resources, spriteName, ".spr"));
    appendPreviewRevisionPart(
        token,
        "CMP",
        ::sed::text::upperAscii(cmpName)
            + "|"
            + previewResourceRevisionToken(resources, cmpName, ".cmp"));

    const std::shared_ptr<const assets::SprFile> sprite =
        loadSharedPreviewSprite(resources, spriteName);
    const std::string materialName = sprite
        ? ::sed::text::trim(sprite->material)
        : std::string{};
    appendPreviewRevisionPart(
        token,
        "MAT",
        materialName.empty()
            ? std::string("MISSING")
            : ::sed::text::upperAscii(materialName)
                + "|"
                + previewResourceRevisionToken(resources, materialName, ".mat"));
    return token;
}

inline PreviewMaterialTransparency imageTransparencyMode(const QImage& image)
{
    if (image.isNull() || !image.hasAlphaChannel())
    {
        return PreviewMaterialTransparency::opaque;
    }

    bool sawCutout = false;
    bool sawBlended = false;
    for (int y = 0; y < image.height(); ++y)
    {
        const QRgb* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        if (!row)
        {
            continue;
        }
        for (int x = 0; x < image.width(); ++x)
        {
            const int alpha = qAlpha(row[x]);
            if (alpha <= 0)
            {
                sawCutout = true;
                continue;
            }
            if (alpha < 255)
            {
                sawBlended = true;
                break;
            }
        }
        if (sawBlended)
        {
            break;
        }
    }

    if (sawBlended)
    {
        return PreviewMaterialTransparency::blended;
    }
    if (sawCutout)
    {
        return PreviewMaterialTransparency::cutout;
    }
    return PreviewMaterialTransparency::opaque;
}

inline PreviewMaterialTransparency modelPreviewTransparencyMode(
    const PreviewMaterialTextureSource* source,
    const std::uint32_t faceFlags) noexcept
{
    if ((faceFlags & (FF_Transluent | FF_ZWriteDisabled)) != 0u)
    {
        return PreviewMaterialTransparency::blended;
    }
    if (!source || source->image.isNull() || source->alphaBpp <= 0)
    {
        return PreviewMaterialTransparency::opaque;
    }
    if (source->alphaBpp > 1)
    {
        return PreviewMaterialTransparency::blended;
    }
    if (source->alphaBpp == 1 || (faceFlags & FF_RD_AlphaRef) != 0u)
    {
        return PreviewMaterialTransparency::cutout;
    }
    return PreviewMaterialTransparency::opaque;
}

inline PreviewMaterialTransparency modelPreviewTransparencyMode(
    const PreviewMaterialTextureSource& source,
    const std::uint32_t faceFlags) noexcept
{
    return modelPreviewTransparencyMode(&source, faceFlags);
}

inline bool modelPreviewUvsAreNormalized(const assets::Model3do& model) noexcept
{
    // The 2.3 format is IJIM's normalized-UV variant. Classic JK/MotS 3DOs
    // store texture coordinates in pixels and the GL renderer divides them by
    // the decoded MAT dimensions before sampling.
    return std::abs(model.version - 2.3) <= 1.0e-6;
}

inline QPointF normalizedModelPreviewUv(
    const assets::Model3doTexVertex& uv,
    const QImage& texture,
    const bool alreadyNormalized) noexcept
{
    if (alreadyNormalized || texture.width() <= 0 || texture.height() <= 0)
    {
        return QPointF(uv.u, uv.v);
    }
    return QPointF(
        static_cast<qreal>(uv.u) / static_cast<qreal>(texture.width()),
        static_cast<qreal>(uv.v) / static_cast<qreal>(texture.height()));
}

template <typename EmitTriangle>
inline void triangulateModelPreviewPolyZigZag(
    const std::size_t vertexCount,
    EmitTriangle&& emitTriangle)
{
    if (vertexCount < 3u)
    {
        return;
    }
    if (vertexCount == 3u)
    {
        emitTriangle(0u, 1u, 2u);
        return;
    }

    const std::size_t triangleCount = vertexCount - 2u;
    std::size_t i0 = 0u;
    std::size_t i1 = 1u;
    std::size_t i2 = vertexCount - 1u;
    for (std::size_t triangle = 0u; triangle < triangleCount; ++triangle)
    {
        emitTriangle(i0, i1, i2);
        if ((triangle & 1u) == 0u)
        {
            i0 = i1;
            ++i1;
        }
        else
        {
            i0 = i2;
            --i2;
        }
    }
}

inline QImage qImageFromRgba8(const ::sed::Rgba8Image& img)
{
    if (img.empty())
    {
        return {};
    }

    QImage out(img.width, img.height, QImage::Format_RGBA8888);
    if (out.isNull())
    {
        return {};
    }

    const std::size_t byteCount =
        static_cast<std::size_t>(img.width) *
        static_cast<std::size_t>(img.height) * 4u;
    std::copy_n(
        reinterpret_cast<const std::byte*>(img.rgba.data()),
        byteCount,
        reinterpret_cast<std::byte*>(out.bits()));
    return out;
}

inline std::optional<PreviewCmpSource> loadPreviewCmp(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName)
{
    if (!resources)
    {
        return std::nullopt;
    }

    const std::string normalizedCmp = ::sed::text::trim(cmpName);
    if (normalizedCmp.empty())
    {
        return std::nullopt;
    }

    const std::string cacheKey = previewResourceScopedCacheKey(
        resources,
        normalizedCmp,
        previewResourceRevisionToken(resources, normalizedCmp, ".cmp"));
    {
        std::lock_guard<std::mutex> lock(sharedPreviewCmpCacheMutex());
        const auto& cache = sharedPreviewCmpCache();
        if (const auto it = cache.find(cacheKey); it != cache.end())
        {
            return it->second;
        }
    }

    const auto bytes = tryReadPreviewResource(resources, normalizedCmp, ".cmp");
    if (!bytes)
    {
        std::lock_guard<std::mutex> lock(sharedPreviewCmpCacheMutex());
        auto& cache = sharedPreviewCmpCache();
        if (cache.size() >= 32u && cache.find(cacheKey) == cache.end())
        {
            cache.erase(cache.begin());
        }
        cache[cacheKey] = std::nullopt;
        return std::nullopt;
    }

    const std::optional<assets::CmpPalette> palette =
        assets::parseCmpPalette(*bytes);
    const std::optional<PreviewCmpSource> cmpSource = palette
        ? std::optional<PreviewCmpSource>(PreviewCmpSource{
            .palette = *palette,
            .complete = assets::parseCmp(*bytes),
        })
        : std::nullopt;
    {
        std::lock_guard<std::mutex> lock(sharedPreviewCmpCacheMutex());
        auto& cache = sharedPreviewCmpCache();
        if (cache.size() >= 32u && cache.find(cacheKey) == cache.end())
        {
            cache.erase(cache.begin());
        }
        cache[cacheKey] = cmpSource;
    }
    return cmpSource;
}

inline std::string previewMaterialTextureCacheKey(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view materialName)
{
    return previewResourceScopedCacheKey(
        resources,
        std::string(cmpName) + "|" + previewResourceRevisionToken(resources, cmpName, ".cmp"),
        std::string(materialName) + "|" + previewResourceRevisionToken(resources, materialName, ".mat"));
}

inline std::optional<PreviewMaterialTextureSource> findCachedPreviewMaterialTextureSource(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view materialName)
{
    if (!resources || materialName.empty() || materialName == "-")
    {
        return std::nullopt;
    }

    const std::string cacheKey = previewMaterialTextureCacheKey(resources, cmpName, materialName);
    {
        std::lock_guard<std::mutex> lock(sharedPreviewMaterialImageCacheMutex());
        PreviewMaterialTextureSource cached{};
        if (sharedPreviewMaterialImageCache().find(cacheKey, cached))
        {
            if (cached.image.isNull())
            {
                return std::nullopt;
            }
            return cached;
        }
    }
    return std::nullopt;
}

inline std::optional<PreviewMaterialTextureSource> loadPreviewMaterialTextureSource(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view materialName)
{
    if (!resources || materialName.empty() || materialName == "-")
    {
        return std::nullopt;
    }

    const std::string cacheKey = previewMaterialTextureCacheKey(resources, cmpName, materialName);
    {
        std::lock_guard<std::mutex> lock(sharedPreviewMaterialImageCacheMutex());
        PreviewMaterialTextureSource cached{};
        if (sharedPreviewMaterialImageCache().find(cacheKey, cached))
        {
            return cached.image.isNull()
                ? std::nullopt
                : std::optional<PreviewMaterialTextureSource>(std::move(cached));
        }
    }

    auto matBytes = tryReadPreviewResource(resources, materialName, ".mat");
    if (!matBytes)
    {
        std::lock_guard<std::mutex> lock(sharedPreviewMaterialImageCacheMutex());
        sharedPreviewMaterialImageCache().insertOrAssign(cacheKey, PreviewMaterialTextureSource{});
        return std::nullopt;
    }

    const std::optional<PreviewCmpSource> cmp = loadPreviewCmp(resources, cmpName);

    try
    {
        assets::MatFile mat = assets::MatFile::parse(
            std::move(*matBytes),
            cmp ? &cmp->palette : nullptr);
        QImage image = qImageFromRgba8(mat.decodeCelRgba8(0));
        if (image.isNull())
        {
            std::lock_guard<std::mutex> lock(sharedPreviewMaterialImageCacheMutex());
            sharedPreviewMaterialImageCache().insertOrAssign(cacheKey, PreviewMaterialTextureSource{});
            return std::nullopt;
        }

        // Keep the decoded MAT in the same orientation as GlView's ordinary
        // resource path. The prepared selected-preview path passes this image
        // directly to QOpenGLTexture, so pre-mirroring it would invert the
        // texture only when a worker-populated cache was used.
        const sed::ColorInfo& colorInfo = mat.colorInfo();
        const bool indexed = colorInfo.mode == sed::ColorMode::Indexed
            || colorInfo.mode == sed::ColorMode::IndexedCols;
        const std::int32_t headerAlphaBpp = colorInfo.alphaBPP <= 0
            ? 0
            : static_cast<std::int32_t>(colorInfo.alphaBPP);
        const std::int32_t inferredAlphaBpp = render::inferTextureAlphaBpp(image);
        std::int32_t alphaBpp = headerAlphaBpp;
        if ((indexed || headerAlphaBpp == 1) && inferredAlphaBpp > 0)
        {
            alphaBpp = inferredAlphaBpp;
        }
        else if (alphaBpp == 0)
        {
            alphaBpp = inferredAlphaBpp;
        }

        std::optional<ClassicSolidLightRamp> classicSolidLightRamp{};
        if (cmp && cmp->complete)
        {
            const assets::CmpLightRamp ramp =
                assets::makeCmpLightRamp(
                    *cmp->complete, mat.cel(0).solidColorIndex);
            ClassicSolidLightRamp gpuRamp{};
            for (std::size_t level = 0; level < ramp.size(); ++level)
            {
                gpuRamp[level] = QVector3D(
                    static_cast<float>(ramp[level].r) / 255.0f,
                    static_cast<float>(ramp[level].g) / 255.0f,
                    static_cast<float>(ramp[level].b) / 255.0f);
            }
            classicSolidLightRamp = std::move(gpuRamp);
        }

        PreviewMaterialTextureSource source{
            .image = std::move(image),
            .alphaBpp = alphaBpp,
            .solidColor = QVector3D(
                static_cast<float>(mat.cel(0).solidColor.r) / 255.0f,
                static_cast<float>(mat.cel(0).solidColor.g) / 255.0f,
                static_cast<float>(mat.cel(0).solidColor.b) / 255.0f),
            .classicSolidLightRamp = std::move(classicSolidLightRamp),
        };
        {
            std::lock_guard<std::mutex> lock(sharedPreviewMaterialImageCacheMutex());
            sharedPreviewMaterialImageCache().insertOrAssign(cacheKey, source);
        }
        return source;
    }
    catch (const std::exception&)
    {
        std::lock_guard<std::mutex> lock(sharedPreviewMaterialImageCacheMutex());
        sharedPreviewMaterialImageCache().insertOrAssign(cacheKey, PreviewMaterialTextureSource{});
        return std::nullopt;
    }
}

inline std::optional<QImage> loadPreviewMaterialImage(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view materialName)
{
    const std::optional<PreviewMaterialTextureSource> source =
        loadPreviewMaterialTextureSource(resources, cmpName, materialName);
    return source ? std::optional<QImage>(source->image) : std::nullopt;
}

/**
 * Seed a preview GlView from immutable CPU assets already prepared by the
 * bounded thumbnail workers. Missing decoded MATs deliberately stay on the
 * normal fallback path. This helper does not read asset contents or decode
 * images; each material cache lookup still queries dependency file metadata.
 */
inline void primeSharedModelPreviewAssets(
    GlView& view,
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view modelName)
{
    std::shared_ptr<const assets::Model3do> model{};
    if (!resources
        || !findCachedSharedPreviewModel(resources, modelName, model)
        || !model)
    {
        return;
    }

    view.primeModel3do(modelName, model);
    for (const std::string& material : model->materials)
    {
        if (const std::optional<PreviewMaterialTextureSource> source =
                findCachedPreviewMaterialTextureSource(
                    resources, cmpName, material))
        {
            view.primeMaterialTexture(
                material,
                cmpName,
                source->image,
                source->alphaBpp,
                source->solidColor,
                source->classicSolidLightRamp);
        }
    }
}

inline std::optional<QImage> loadSharedPreviewSpriteImage(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view spriteName)
{
    if (!resources)
    {
        return std::nullopt;
    }

    const std::string cacheKey = previewResourceScopedCacheKey(
        resources,
        spriteName,
        sharedPreviewSpriteDependencyRevisionToken(
            resources, cmpName, spriteName));
    {
        std::lock_guard<std::mutex> lock(sharedPreviewSpriteImageCacheMutex());
        QImage cached{};
        if (sharedPreviewSpriteImageCache().find(cacheKey, cached))
        {
            if (cached.isNull())
            {
                return std::nullopt;
            }
            return cached;
        }
    }

    auto storeResult = [&](QImage image) -> std::optional<QImage>
    {
        std::lock_guard<std::mutex> lock(sharedPreviewSpriteImageCacheMutex());
        sharedPreviewSpriteImageCache().insertOrAssign(cacheKey, image);
        if (image.isNull())
        {
            return std::nullopt;
        }
        return image;
    };

    const std::shared_ptr<const assets::SprFile> sprite = loadSharedPreviewSprite(resources, spriteName);
    if (!sprite)
    {
        return storeResult(QImage{});
    }

    try
    {
        auto matBytes = resources->tryReadBinaryWithExt(sprite->material, ".mat");
        if (!matBytes)
        {
            return storeResult(QImage{});
        }

        const std::optional<PreviewCmpSource> cmp = loadPreviewCmp(resources, cmpName);
        assets::MatFile mat = assets::MatFile::parse(
            std::move(*matBytes),
            cmp ? &cmp->palette : nullptr);
        return storeResult(qImageFromRgba8(mat.decodeCelRgba8(0)));
    }
    catch (const std::exception&)
    {
        return storeResult(QImage{});
    }
}

struct ModelPreviewOrientation
{
    float yawDeg{};
    float pitchDeg{};
};

constexpr float kTemplatePreviewDefaultYawDeg = 160.0f;
constexpr float kTemplatePreviewDefaultPitchDeg = 14.0f;
constexpr float kTemplatePreviewDefaultRollDeg = 0.0f;
constexpr float kTemplatePreviewFitPadding = 1.24f;

inline QVector3D rotatePreviewVertex(QVector3D v, const float yawDeg, const float pitchDeg)
{
    constexpr float kPi = 3.14159265358979323846f;
    const float yaw = yawDeg * (kPi / 180.0f);
    const float pitch = pitchDeg * (kPi / 180.0f);

    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);

    const float x1 = (v.x() * cy) - (v.y() * sy);
    const float y1 = (v.x() * sy) + (v.y() * cy);
    const float z1 = v.z();

    const float x2 = (x1 * cp) + (z1 * sp);
    const float z2 = (-x1 * sp) + (z1 * cp);

    v.setX(x2);
    v.setY(y1);
    v.setZ(z2);
    return v;
}

inline ModelPreviewOrientation chooseModelPreviewOrientation(const std::vector<QVector3D>& vertices)
{
    (void)vertices;
    return ModelPreviewOrientation{ kTemplatePreviewDefaultYawDeg, kTemplatePreviewDefaultPitchDeg };
}

inline std::vector<QVector3D> modelPreviewWorldVertices(const assets::Model3do& model)
{
    std::vector<QVector3D> vertices{};
    vertices.reserve(256);

    for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
    {
        const assets::Model3doMesh& mesh = model.meshes[meshIndex];
        for (const assets::Model3doVertex& vertex : mesh.vertices)
        {
            vertices.push_back(QVector3D(
                static_cast<float>(vertex.x),
                static_cast<float>(vertex.y),
                static_cast<float>(vertex.z)));
        }
    }

    return vertices;
}

inline bool normalizePreviewVector(QVector3D& v)
{
    const float len2 = v.lengthSquared();
    if (!std::isfinite(len2) || len2 <= sed::math::kTinyEpsilonF)
    {
        return false;
    }
    v /= std::sqrt(len2);
    return true;
}

inline app::ViewportCamera makeTemplatePreviewCamera(
    const std::vector<QVector3D>& worldVerts,
    const int width,
    const int height)
{
    app::ViewportCamera camera{};
    camera.Reset();
    camera.SetProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
    camera.SetCanvas(width, height);
    camera.orbit.yawDeg = kTemplatePreviewDefaultYawDeg;
    camera.orbit.pitchDeg = kTemplatePreviewDefaultPitchDeg;
    camera.orbit.rollDeg = kTemplatePreviewDefaultRollDeg;
    camera.UpdateFromOrbit();

    if (worldVerts.empty())
    {
        return camera;
    }

    QVector3D minv = worldVerts.front();
    QVector3D maxv = worldVerts.front();
    for (const QVector3D& v : worldVerts)
    {
        minv.setX(std::min(minv.x(), v.x()));
        minv.setY(std::min(minv.y(), v.y()));
        minv.setZ(std::min(minv.z(), v.z()));
        maxv.setX(std::max(maxv.x(), v.x()));
        maxv.setY(std::max(maxv.y(), v.y()));
        maxv.setZ(std::max(maxv.z(), v.z()));
    }

    if ((maxv - minv).lengthSquared() <= 1.0e-6f)
    {
        maxv += QVector3D(0.5f, 0.5f, 0.5f);
        minv -= QVector3D(0.5f, 0.5f, 0.5f);
    }

    const QVector3D center = (minv + maxv) * 0.5f;
    const QVector3D halfExtents = (maxv - minv) * 0.5f * kTemplatePreviewFitPadding;
    minv = center - halfExtents;
    maxv = center + halfExtents;

    const QVector3D ext = maxv - minv;
    float radius = ext.length() * 0.5f;
    if (!std::isfinite(radius) || radius <= 1.0e-6f)
    {
        radius = 0.1f;
    }

    const float aspect = (height > 0) ? (static_cast<float>(width) / static_cast<float>(height)) : 1.0f;
    const float safeAspect = std::max(aspect, 1.0e-6f);
    const float hheight = static_cast<float>(height) * 0.5f;

    bool invOk = false;
    const QMatrix4x4 invView = camera.viewMatrix().inverted(&invOk);
    QVector3D right(1.0f, 0.0f, 0.0f);
    QVector3D up(0.0f, 1.0f, 0.0f);
    if (invOk)
    {
        const QVector4D r4 = invView.column(0);
        const QVector4D u4 = invView.column(1);
        right = QVector3D(r4.x(), r4.y(), r4.z());
        up = QVector3D(u4.x(), u4.y(), u4.z());
        if (!normalizePreviewVector(right))
        {
            right = QVector3D(1.0f, 0.0f, 0.0f);
        }
        if (!normalizePreviewVector(up))
        {
            up = QVector3D(0.0f, 1.0f, 0.0f);
        }
    }

    const std::array<QVector3D, 8> corners{
        QVector3D(minv.x(), minv.y(), minv.z()),
        QVector3D(minv.x(), minv.y(), maxv.z()),
        QVector3D(minv.x(), maxv.y(), minv.z()),
        QVector3D(minv.x(), maxv.y(), maxv.z()),
        QVector3D(maxv.x(), minv.y(), minv.z()),
        QVector3D(maxv.x(), minv.y(), maxv.z()),
        QVector3D(maxv.x(), maxv.y(), minv.z()),
        QVector3D(maxv.x(), maxv.y(), maxv.z()),
    };

    float maxAbsViewX = 0.01f;
    float maxAbsViewY = 0.01f;
    for (const QVector3D& corner : corners)
    {
        const QVector3D local = corner - center;
        maxAbsViewX = std::max(maxAbsViewX, std::abs(QVector3D::dotProduct(local, right)));
        maxAbsViewY = std::max(maxAbsViewY, std::abs(QVector3D::dotProduct(local, up)));
    }

    const float desiredHalfExtent = std::max(0.01f, std::max(maxAbsViewY, maxAbsViewX / safeAspect) * 1.003f);
    const float scale = (desiredHalfExtent > 1.0e-6f) ? (hheight / (safeAspect * desiredHalfExtent)) : camera.orthoScale();

    camera.orbit.pan = center - camera.orbit.center;
    camera.orbit.distance = std::max(camera.orbit.distance, std::max(radius * 1.02f, 0.01f));
    camera.SetOrthoScale(std::clamp(scale, 0.01f, 1.0e6f));
    camera.UpdateFromOrbit();
    return camera;
}

inline QColor baseColorForMaterialIndex(const int materialIndex)
{
    const int variant = std::abs(materialIndex) % 5;
    switch (variant)
    {
    case 1: return QColor(186, 196, 210);
    case 2: return QColor(200, 190, 176);
    case 3: return QColor(172, 186, 174);
    case 4: return QColor(196, 178, 188);
    default: return QColor(192, 198, 206);
    }
}

inline QTransform affineFromTriangles(
    const QPointF& s0,
    const QPointF& s1,
    const QPointF& s2,
    const QPointF& d0,
    const QPointF& d1,
    const QPointF& d2,
    bool* ok = nullptr)
{
    const qreal den =
        (s0.x() * (s1.y() - s2.y())) +
        (s1.x() * (s2.y() - s0.y())) +
        (s2.x() * (s0.y() - s1.y()));
    if (std::abs(den) <= 1.0e-9)
    {
        if (ok)
        {
            *ok = false;
        }
        return {};
    }

    const auto solveComponent = [&](const qreal v0, const qreal v1, const qreal v2)
    {
        const qreal a =
            ((v0 * (s1.y() - s2.y())) +
             (v1 * (s2.y() - s0.y())) +
             (v2 * (s0.y() - s1.y()))) / den;
        const qreal b =
            ((v0 * (s2.x() - s1.x())) +
             (v1 * (s0.x() - s2.x())) +
             (v2 * (s1.x() - s0.x()))) / den;
        const qreal c =
            ((v0 * ((s1.x() * s2.y()) - (s2.x() * s1.y()))) +
             (v1 * ((s2.x() * s0.y()) - (s0.x() * s2.y()))) +
             (v2 * ((s0.x() * s1.y()) - (s1.x() * s0.y())))) / den;
        return std::array<qreal, 3>{ a, b, c };
    };

    const auto x = solveComponent(d0.x(), d1.x(), d2.x());
    const auto y = solveComponent(d0.y(), d1.y(), d2.y());
    if (ok)
    {
        *ok = std::isfinite(x[0]) && std::isfinite(x[1]) && std::isfinite(x[2])
           && std::isfinite(y[0]) && std::isfinite(y[1]) && std::isfinite(y[2]);
    }
    return QTransform(x[0], y[0], x[1], y[1], x[2], y[2]);
}

struct TiledTriangleSource
{
    QImage image{};
    std::array<QPointF, 3> points{};
};

inline std::optional<TiledTriangleSource> buildTiledTriangleSource(
    const QImage& texture,
    const std::array<QPointF, 3>& uv)
{
    if (texture.isNull())
    {
        return std::nullopt;
    }

    qreal minU = uv[0].x();
    qreal maxU = uv[0].x();
    qreal minV = uv[0].y();
    qreal maxV = uv[0].y();
    for (const QPointF& p : uv)
    {
        minU = std::min(minU, p.x());
        maxU = std::max(maxU, p.x());
        minV = std::min(minV, p.y());
        maxV = std::max(maxV, p.y());
    }

    const qreal tileMinU = std::floor(minU);
    const qreal tileMaxU = std::ceil(maxU);
    const qreal tileMinV = std::floor(minV);
    const qreal tileMaxV = std::ceil(maxV);
    const int tilesU = std::max(1, static_cast<int>(tileMaxU - tileMinU));
    const int tilesV = std::max(1, static_cast<int>(tileMaxV - tileMinV));
    if (tilesU > 8 || tilesV > 8)
    {
        return std::nullopt;
    }

    QImage tiled(texture.width() * tilesU, texture.height() * tilesV, QImage::Format_RGBA8888);
    if (tiled.isNull())
    {
        return std::nullopt;
    }

    QPainter tilePainter(&tiled);
    tilePainter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (int ty = 0; ty < tilesV; ++ty)
    {
        for (int tx = 0; tx < tilesU; ++tx)
        {
            tilePainter.drawImage(tx * texture.width(), ty * texture.height(), texture);
        }
    }

    TiledTriangleSource out{};
    out.image = std::move(tiled);
    for (std::size_t i = 0; i < 3; ++i)
    {
        out.points[i] = QPointF(
            (uv[i].x() - tileMinU) * texture.width(),
            (uv[i].y() - tileMinV) * texture.height());
    }
    return out;
}

inline bool drawTexturedTriangle(
    QPainter& painter,
    const QImage& texture,
    const std::array<QPointF, 3>& uv,
    const std::array<QPointF, 3>& dst,
    const float shade)
{
    const std::optional<TiledTriangleSource> source = buildTiledTriangleSource(texture, uv);
    if (!source)
    {
        return false;
    }

    bool ok = false;
    const QTransform transform = affineFromTriangles(
        source->points[0],
        source->points[1],
        source->points[2],
        dst[0],
        dst[1],
        dst[2],
        &ok);
    if (!ok)
    {
        return false;
    }

    QPainterPath clip{};
    clip.moveTo(dst[0]);
    clip.lineTo(dst[1]);
    clip.lineTo(dst[2]);
    clip.closeSubpath();

    painter.save();
    painter.setClipPath(clip);
    painter.setTransform(transform, false);
    painter.drawImage(QPointF(0.0, 0.0), source->image);
    painter.restore();

    if (shade < 0.995f)
    {
        const int alpha = std::clamp(
            static_cast<int>(std::lround((1.0f - shade) * 180.0f)),
            0,
            180);
        if (alpha > 0)
        {
            painter.save();
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, alpha));
            painter.drawPath(clip);
            painter.restore();
        }
    }
    return true;
}

inline QImage makeTemplateModelPreviewImage(
    const assets::Model3do& model,
    const QSize& targetSize,
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const bool wireframe)
{
    const int width = std::max(40, targetSize.width());
    const int height = std::max(40, targetSize.height());
    QImage canvas(width, height, QImage::Format_ARGB32);
    canvas.fill(QColor(18, 20, 28));

    const std::vector<QVector3D> rawWorldVerts = modelPreviewWorldVertices(model);
    if (rawWorldVerts.empty())
    {
        return canvas;
    }

    const app::ViewportCamera camera = makeTemplatePreviewCamera(rawWorldVerts, width, height);
    const QMatrix4x4 viewMatrix = camera.viewMatrix();
    const QMatrix4x4 viewProjection = camera.projectionMatrix() * viewMatrix;

    auto project = [&](const QVector3D& world, QVector3D* outView = nullptr, bool* outValid = nullptr) -> QPointF
    {
        const QVector3D view = viewMatrix.map(world);
        if (outView)
        {
            *outView = view;
        }

        const QVector4D clip = viewProjection * QVector4D(world, 1.0f);
        const float w = clip.w();
        const bool valid = std::isfinite(w) && std::abs(w) > 1.0e-6f;
        if (outValid)
        {
            *outValid = valid;
        }
        if (!valid)
        {
            return QPointF(
                static_cast<qreal>(width) * 0.5,
                static_cast<qreal>(height) * 0.5);
        }

        const QVector3D ndc(
            clip.x() / w,
            clip.y() / w,
            clip.z() / w);
        return QPointF(
            (static_cast<qreal>(ndc.x()) * 0.5 + 0.5) * static_cast<qreal>(width),
            (1.0 - (static_cast<qreal>(ndc.y()) * 0.5 + 0.5)) * static_cast<qreal>(height));
    };

    struct DrawFace
    {
        QPolygonF poly{};
        float depth{};
        float shade{ 1.0f };
        QColor fill{};
        QColor outline{};
        int materialIndex{ -1 };
        bool textured{};
        PreviewMaterialTransparency transparency{ PreviewMaterialTransparency::opaque };
        std::array<QPointF, 3> screenPoints{};
        std::array<QPointF, 3> uvPoints{};
        std::array<float, 3> vertexDepths{};
    };

    std::vector<DrawFace> faces{};
    faces.reserve(128);

    std::vector<std::optional<PreviewMaterialTextureSource>> materialSources(model.materials.size());
    std::vector<std::uint8_t> materialResolved(model.materials.size(), 0u);
    auto tryMaterialSource = [&](const int materialIndex) -> const PreviewMaterialTextureSource*
    {
        if (materialIndex < 0 || static_cast<std::size_t>(materialIndex) >= model.materials.size())
        {
            return nullptr;
        }

        const std::size_t idx = static_cast<std::size_t>(materialIndex);
        if (materialResolved[idx] == 0u)
        {
            materialSources[idx] = loadPreviewMaterialTextureSource(
                resources, cmpName, model.materials[idx]);
            materialResolved[idx] = 1u;
        }

        if (!materialSources[idx] || materialSources[idx]->image.isNull())
        {
            return nullptr;
        }
        return &*materialSources[idx];
    };
    auto tryMaterialImage = [&](const int materialIndex) -> const QImage*
    {
        const PreviewMaterialTextureSource* source = tryMaterialSource(materialIndex);
        return source ? &source->image : nullptr;
    };

    const QVector3D lightDir = QVector3D(0.72f, -0.42f, 0.56f).normalized();
    const bool normalizedUvs = modelPreviewUvsAreNormalized(model);

    auto emitMeshFaces = [&](const assets::Model3doMesh& mesh)
    {
        if (mesh.geo == static_cast<std::int32_t>(TGeoMode::NotDrawn))
        {
            return;
        }

        for (const assets::Model3doFace& face : mesh.faces)
        {
            if (face.vertices.size() < 3)
            {
                continue;
            }

            std::vector<QVector3D> worldPoints{};
            worldPoints.reserve(face.vertices.size());
            std::vector<QPointF> screenPoints{};
            screenPoints.reserve(face.vertices.size());
            bool faceValid = true;
            for (const std::int32_t index : face.vertices)
            {
                if (index < 0 || static_cast<std::size_t>(index) >= mesh.vertices.size())
                {
                    faceValid = false;
                    break;
                }

                const assets::Model3doVertex& v = mesh.vertices[static_cast<std::size_t>(index)];
                const QVector3D world(
                    static_cast<float>(v.x),
                    static_cast<float>(v.y),
                    static_cast<float>(v.z));
                QVector3D view{};
                bool valid = false;
                const QPointF screen = project(world, &view, &valid);
                if (!valid || !std::isfinite(view.x()) || !std::isfinite(view.y()) || !std::isfinite(view.z()))
                {
                    faceValid = false;
                    break;
                }
                screenPoints.push_back(screen);
                worldPoints.push_back(world);
            }

            if (!faceValid
                || screenPoints.size() != face.vertices.size()
                || worldPoints.size() != face.vertices.size())
            {
                continue;
            }

            const QColor base = baseColorForMaterialIndex(face.material);
            std::vector<QPointF> faceUvs{};
            bool faceTextured = false;
            const PreviewMaterialTextureSource* materialSource = nullptr;
            if (!wireframe
                && face.geo >= static_cast<std::int32_t>(TGeoMode::Texture)
                && face.texVertices.size() == face.vertices.size()
                && (materialSource = tryMaterialSource(face.material)) != nullptr)
            {
                faceTextured = true;
                faceUvs.reserve(face.texVertices.size());
                for (const std::int32_t uvIndex : face.texVertices)
                {
                    if (uvIndex < 0 || static_cast<std::size_t>(uvIndex) >= mesh.texVertices.size())
                    {
                        faceTextured = false;
                        faceUvs.clear();
                        break;
                    }

                    const assets::Model3doTexVertex& tv = mesh.texVertices[static_cast<std::size_t>(uvIndex)];
                    faceUvs.push_back(normalizedModelPreviewUv(
                        tv, materialSource->image, normalizedUvs));
                }
            }

            triangulateModelPreviewPolyZigZag(
                worldPoints.size(),
                [&](const std::size_t index0, const std::size_t index1, const std::size_t index2)
            {
                DrawFace out{};
                out.materialIndex = face.material;
                out.screenPoints = {
                    screenPoints[index0],
                    screenPoints[index1],
                    screenPoints[index2],
                };
                out.poly << out.screenPoints[0] << out.screenPoints[1] << out.screenPoints[2];

                const QVector3D& w0 = worldPoints[index0];
                const QVector3D& w1 = worldPoints[index1];
                const QVector3D& w2 = worldPoints[index2];
                const QVector3D v0 = viewMatrix.map(w0);
                const QVector3D v1 = viewMatrix.map(w1);
                const QVector3D v2 = viewMatrix.map(w2);
                out.vertexDepths = {
                    v0.z(),
                    v1.z(),
                    v2.z(),
                };
                QVector3D normal = QVector3D::crossProduct(w1 - w0, w2 - w0);
                // 3DO actor parts use very small, densely tessellated faces.
                // Reject only numerically degenerate geometry here; the
                // screen-space rasterizer applies its own projected-area
                // threshold after the preview camera has fitted the model.
                if (normal.lengthSquared() <= sed::math::kTinyEpsilonF)
                {
                    return;
                }
                normal.normalize();

                const float lambert = std::max(0.0f, QVector3D::dotProduct(normal, lightDir));
                const float cameraFacing = std::abs(QVector3D::dotProduct(normal, QVector3D(1.0f, 0.0f, 0.0f)));
                out.shade = std::clamp(0.34f + (lambert * 0.50f) + (cameraFacing * 0.12f), 0.22f, 0.96f);

                if (wireframe)
                {
                    out.fill = QColor(0, 0, 0, 0);
                    out.outline = base.lighter(135);
                    out.outline.setAlpha(220);
                }
                else
                {
                    const float fillShade = std::clamp(out.shade, 0.18f, 0.95f);
                    out.fill = QColor(
                        std::clamp(static_cast<int>(std::lround(base.red() * fillShade)), 0, 255),
                        std::clamp(static_cast<int>(std::lround(base.green() * fillShade)), 0, 255),
                        std::clamp(static_cast<int>(std::lround(base.blue() * fillShade)), 0, 255),
                        235);
                    out.outline = base.lighter(145);
                    out.outline.setAlpha(200);
                }

                if (faceTextured && faceUvs.size() == worldPoints.size())
                {
                    out.textured = true;
                    // Asset previews use neutral white ambient in the
                    // interactive GlView. Preserve authored MAT color here as
                    // well; directional darkening made small child meshes
                    // such as actor heads disappear at list-icon resolution.
                    out.shade = 1.0f;
                    out.uvPoints = {
                        faceUvs[index0],
                        faceUvs[index1],
                        faceUvs[index2],
                    };
                }
                out.transparency = modelPreviewTransparencyMode(
                    materialSource, face.faceFlags);

                out.depth = (v0.z() + v1.z() + v2.z()) / 3.0f;
                faces.push_back(std::move(out));
            });
        }
    };

    for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
    {
        emitMeshFaces(model.meshes[meshIndex]);
    }

    std::stable_sort(faces.begin(), faces.end(), [](const DrawFace& a, const DrawFace& b)
        {
            const bool aTranslucent = a.transparency == PreviewMaterialTransparency::blended;
            const bool bTranslucent = b.transparency == PreviewMaterialTransparency::blended;
            if (aTranslucent != bTranslucent)
            {
                return !aTranslucent && bTranslucent;
            }
            return a.depth < b.depth;
        });

    {
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRectF shadowRect(
            width * 0.20,
            height * 0.80,
            width * 0.60,
            std::max(8.0, height * 0.08));
        QRadialGradient shadowGrad(shadowRect.center(), shadowRect.width() * 0.55, shadowRect.center());
        shadowGrad.setColorAt(0.0, QColor(0, 0, 0, 74));
        shadowGrad.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(shadowGrad);
        painter.drawEllipse(shadowRect);

        if (wireframe)
        {
            for (const DrawFace& face : faces)
            {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(face.outline, 1.35));
                painter.drawPolygon(face.poly);
            }
        }
    }

    if (!wireframe)
    {
        std::vector<float> depthBuffer(static_cast<std::size_t>(width * height), -std::numeric_limits<float>::infinity());

        auto wrapUnit = [](float value) -> float
        {
            value = value - std::floor(value);
            if (value < 0.0f)
            {
                value += 1.0f;
            }
            return value;
        };

        auto sampleTextureColor = [&](const QImage& texture, const QPointF& uv, const float shade) -> QColor
        {
            if (texture.isNull() || texture.width() <= 0 || texture.height() <= 0)
            {
                return QColor(255, 255, 255, 255);
            }

            const float u = wrapUnit(static_cast<float>(uv.x()));
            const float v = wrapUnit(static_cast<float>(uv.y()));
            const int tx = std::clamp(static_cast<int>(std::lround(u * static_cast<float>(texture.width() - 1))), 0, texture.width() - 1);
            const int ty = std::clamp(static_cast<int>(std::lround(v * static_cast<float>(texture.height() - 1))), 0, texture.height() - 1);
            const QColor texel = QColor::fromRgba(texture.pixel(tx, ty));

            return QColor(
                std::clamp(static_cast<int>(std::lround(texel.red() * shade)), 0, 255),
                std::clamp(static_cast<int>(std::lround(texel.green() * shade)), 0, 255),
                std::clamp(static_cast<int>(std::lround(texel.blue() * shade)), 0, 255),
                texel.alpha());
        };

        auto blendOver = [](const QColor& src, const QRgb dstPixel) -> QRgb
        {
            const float srcAlpha = static_cast<float>(src.alpha()) / 255.0f;
            if (srcAlpha <= 0.0f)
            {
                return dstPixel;
            }

            const QColor dst = QColor::fromRgba(dstPixel);
            const float dstAlpha = static_cast<float>(dst.alpha()) / 255.0f;
            const float outAlpha = srcAlpha + dstAlpha * (1.0f - srcAlpha);
            if (outAlpha <= 1.0e-6f)
            {
                return qRgba(0, 0, 0, 0);
            }

            const auto mix = [&](const int srcChannel, const int dstChannel) -> int
            {
                const float value =
                    ((static_cast<float>(srcChannel) * srcAlpha)
                        + (static_cast<float>(dstChannel) * dstAlpha * (1.0f - srcAlpha)))
                    / outAlpha;
                return std::clamp(static_cast<int>(std::lround(value)), 0, 255);
            };

            return qRgba(
                mix(src.red(), dst.red()),
                mix(src.green(), dst.green()),
                mix(src.blue(), dst.blue()),
                std::clamp(static_cast<int>(std::lround(outAlpha * 255.0f)), 0, 255));
        };

        auto rasterizeFace = [&](const DrawFace& face, const bool blendedPass)
        {
            const QPointF& p0 = face.screenPoints[0];
            const QPointF& p1 = face.screenPoints[1];
            const QPointF& p2 = face.screenPoints[2];

            const auto edge = [](const QPointF& a, const QPointF& b, const QPointF& c) -> float
            {
                return static_cast<float>(((c.x() - a.x()) * (b.y() - a.y())) - ((c.y() - a.y()) * (b.x() - a.x())));
            };

            const float area = edge(p0, p1, p2);
            if (std::abs(area) <= 1.0e-6f)
            {
                return;
            }

            const int minX = std::clamp(static_cast<int>(std::floor(std::min({ p0.x(), p1.x(), p2.x() }))), 0, width - 1);
            const int maxX = std::clamp(static_cast<int>(std::ceil(std::max({ p0.x(), p1.x(), p2.x() }))), 0, width - 1);
            const int minY = std::clamp(static_cast<int>(std::floor(std::min({ p0.y(), p1.y(), p2.y() }))), 0, height - 1);
            const int maxY = std::clamp(static_cast<int>(std::ceil(std::max({ p0.y(), p1.y(), p2.y() }))), 0, height - 1);

            const QImage* texture = face.textured ? tryMaterialImage(face.materialIndex) : nullptr;

            for (int py = minY; py <= maxY; ++py)
            {
                QRgb* row = reinterpret_cast<QRgb*>(canvas.scanLine(py));
                if (!row)
                {
                    continue;
                }

                for (int px = minX; px <= maxX; ++px)
                {
                    const QPointF sample(static_cast<qreal>(px) + 0.5, static_cast<qreal>(py) + 0.5);
                    const float w0 = edge(p1, p2, sample);
                    const float w1 = edge(p2, p0, sample);
                    const float w2 = edge(p0, p1, sample);

                    if ((area > 0.0f && (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f))
                        || (area < 0.0f && (w0 > 0.0f || w1 > 0.0f || w2 > 0.0f)))
                    {
                        continue;
                    }

                    const float invArea = 1.0f / area;
                    const float b0 = w0 * invArea;
                    const float b1 = w1 * invArea;
                    const float b2 = w2 * invArea;
                    const float depth =
                        (b0 * face.vertexDepths[0])
                        + (b1 * face.vertexDepths[1])
                        + (b2 * face.vertexDepths[2]);

                    const std::size_t bufferIndex = static_cast<std::size_t>(py * width + px);
                    if (!blendedPass)
                    {
                        if (depth < depthBuffer[bufferIndex])
                        {
                            continue;
                        }
                    }
                    else if (depth < depthBuffer[bufferIndex] - 1.0e-5f)
                    {
                        continue;
                    }

                    QColor src = face.fill;
                    if (texture)
                    {
                        const QPointF uv =
                            (face.uvPoints[0] * b0)
                            + (face.uvPoints[1] * b1)
                            + (face.uvPoints[2] * b2);
                        src = sampleTextureColor(*texture, uv, face.shade);
                    }

                    if (face.transparency == PreviewMaterialTransparency::cutout && src.alpha() < 128)
                    {
                        continue;
                    }

                    if (!blendedPass)
                    {
                        if (face.transparency == PreviewMaterialTransparency::blended)
                        {
                            continue;
                        }
                        row[px] = qRgba(src.red(), src.green(), src.blue(), face.transparency == PreviewMaterialTransparency::cutout ? 255 : src.alpha());
                        depthBuffer[bufferIndex] = depth;
                    }
                    else
                    {
                        if (face.transparency != PreviewMaterialTransparency::blended || src.alpha() <= 0)
                        {
                            continue;
                        }
                        row[px] = blendOver(src, row[px]);
                    }
                }
            }
        };

        for (const DrawFace& face : faces)
        {
            rasterizeFace(face, false);
        }
        for (const DrawFace& face : faces)
        {
            if (face.transparency == PreviewMaterialTransparency::blended)
            {
                rasterizeFace(face, true);
            }
        }
    }

    {
        QPainter painter(&canvas);
        painter.setPen(QPen(QColor(0, 0, 0, 120), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(canvas.rect().adjusted(0, 0, -1, -1));
    }
    return canvas;
}

inline QIcon makeTemplateModelPreviewIcon(
    const assets::Model3do& model,
    const QSize& targetSize,
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const bool wireframe)
{
    return QIcon(QPixmap::fromImage(
        makeTemplateModelPreviewImage(model, targetSize, resources, cmpName, wireframe)));
}

class GlViewModelThumbnailRenderer final
{
public:
    GlViewModelThumbnailRenderer(
        std::shared_ptr<const assets::ResourceLocator> resources,
        const ::sed::ProjectType projectType,
        std::string cmpName)
        : resources_(std::move(resources))
        , projectType_(projectType)
        , cmpName_(std::move(cmpName))
        , cacheEpoch_(currentSharedPreviewCacheEpoch())
    {
    }

    bool matchesConfiguration(
        const assets::ResourceLocator* resources,
        const ::sed::ProjectType projectType,
        const std::string_view cmpName) const noexcept
    {
        return resources_.get() == resources
            && projectType_ == projectType
            && ::sed::text::iequalsAscii(cmpName_, cmpName)
            && cacheEpoch_ == currentSharedPreviewCacheEpoch();
    }

    QImage renderModel(
        const std::string_view modelName,
        const QSize& targetSize,
        const bool wireframe)
    {
        if (!resources_)
        {
            return {};
        }

        const std::shared_ptr<const assets::Model3do> model =
            loadSharedPreviewModel(resources_.get(), modelName);
        const std::optional<::sed::ThingBox> box = model
            ? previewBoxForModel(*model)
            : std::nullopt;
        if (!model || !box)
        {
            return {};
        }

        return renderModel(modelName, model, *box, targetSize, wireframe);
    }

    QImage renderModel(
        const std::string_view modelName,
        std::shared_ptr<const assets::Model3do> preparedModel,
        const ::sed::ThingBox& box,
        const QSize& targetSize,
        const bool wireframe)
    {
        // QOpenGLWidget creation, repaint, and framebuffer reads are owned by
        // QApplication's GUI thread. Worker jobs stop at parsed 3DO data and
        // numeric bounds; fail closed if a future caller crosses that seam.
        if (!resources_ || !qApp
            || QThread::currentThread() != qApp->thread())
        {
            return {};
        }

        const QSize renderSize(
            std::max(40, targetSize.width()),
            std::max(40, targetSize.height()));
        ensureView(renderSize);
        refreshViewResources(modelName);
        view_->primeModel3do(modelName, preparedModel);
        for (const std::string& material : preparedModel->materials)
        {
            if (const std::optional<PreviewMaterialTextureSource> source =
                    findCachedPreviewMaterialTextureSource(
                        resources_.get(), cmpName_, material))
            {
                view_->primeMaterialTexture(
                    material,
                    cmpName_,
                    source->image,
                    source->alphaBpp,
                    source->solidColor,
                    source->classicSolidLightRamp);
            }
        }
        configureView(wireframe);
        buildPreviewLevel(std::string(modelName), box);
        fitToBox(box);

        struct RenderBusyScope final
        {
            RenderBusyScope()
            {
                sharedThumbnailRenderActiveCount().fetch_add(1, std::memory_order_relaxed);
            }
            ~RenderBusyScope()
            {
                sharedThumbnailRenderActiveCount().fetch_sub(1, std::memory_order_relaxed);
            }
        } renderBusyScope{};

        // grabFramebuffer() renders the current QOpenGLWidget state before
        // reading it. Calling repaint() first performs a redundant full pass
        // and doubles the longest UI-thread section while thumbnail batches
        // are active. Keep repaint() only as the null-frame recovery path.
        QImage frame = view_->grabFramebuffer();
        if (frame.isNull())
        {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            view_->repaint();
            frame = view_->grabFramebuffer();
        }
        return frame.isNull() ? QImage{} : frame.copy();
    }

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
    void primeMaterialForTest(
        const std::string_view material,
        const std::string_view colormap,
        const QImage& image,
        const std::int32_t alphaBpp)
    {
        ensureView(QSize(96, 96));
        view_->primeMaterialTexture(material, colormap, image, alphaBpp);
    }

    [[nodiscard]] std::uint64_t modelResourceReadCountForTest() const noexcept
    {
        return view_ ? view_->model3doResourceReadCountForTest() : 0u;
    }

    [[nodiscard]] std::uint64_t materialResourceReadCountForTest() const noexcept
    {
        return view_ ? view_->materialResourceReadCountForTest() : 0u;
    }

    [[nodiscard]] bool materialTextureMissingForTest(
        const std::string_view material,
        const std::string_view colormap) const noexcept
    {
        return view_ && view_->materialTextureMissingForTest(material, colormap);
    }

    [[nodiscard]] auto* resourceDiskWatcherForTest() const noexcept
    {
        return view_ ? view_->resourceDiskWatcherForTest() : nullptr;
    }
#endif

private:
    void ensureView(const QSize& renderSize)
    {
        if (!view_)
        {
            view_ = std::make_unique<GlView>();
            view_->setObjectName("hiddenModelThumbnailRenderer");
            view_->setAttribute(Qt::WA_DontShowOnScreen, true);
            view_->setResourceLocator(resources_.get());
            // The owning thumbnail cache publishes a new immutable resource
            // snapshot after its MainWindow watcher remounts archives. This
            // hidden renderer must retain loose dependency watches without
            // creating a competing archive retry loop.
            view_->setMountedArchiveWatchingEnabled(false);
            view_->setTemplateDatabase(&templates_);
            view_->setRenderPipeline(GlView::RenderPipeline::GamePreview);
            view_->setShowGrid(false);
            view_->setShowWireframe(false);
            view_->setShowVertices(false);
            view_->setShowSolid(true);
            view_->setShowTextures(true);
            view_->setShowThings(true);
            view_->setShowAxisWidget(false);
            view_->setShowToastOverlays(false);
            view_->setShowEditorCubes(false);
            view_->setShowPreviewCubes(false);
            view_->setShowThingGizmos(false);
            view_->setThingViewMode(GlView::ThingViewMode::Shaded);
            view_->setLightingMode(GlView::LightingMode::Gouraud);
            view_->setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
            view_->setViewportPickingEnabled(false);
            view_->setHoverPickEnabled(false);
            view_->setPreviewInteractionEnabled(false);
            view_->resize(renderSize);
            view_->show();
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }

        if (view_->size() != renderSize)
        {
            view_->resize(renderSize);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }

    void configureView(const bool wireframe)
    {
        if (!view_)
        {
            return;
        }

        // This renderer owns a dedicated GamePreview view and explicitly sets
        // every relevant option below.  Toggling through Editor merely to
        // reset the preview state rebuilt and uploaded the previous model
        // twice for every thumbnail.
        view_->setShowGrid(false);
        applyThingAssetPreviewRenderMode(*view_, wireframe ? -1 : 1);
        view_->setShowThings(true);
        view_->setShowEditorCubes(false);
        view_->setShowPreviewCubes(false);
        view_->setShowThingGizmos(false);
        view_->setLightingMode(GlView::LightingMode::Gouraud);
        view_->setCameraProjectionType(app::ViewportCamera::ProjectionType::Orthographic);
        view_->setViewportPickingEnabled(false);
        view_->setHoverPickEnabled(false);
        view_->setPreviewInteractionEnabled(false);
    }

    bool observeResourceRevision(
        const std::string_view kind,
        const std::string_view name,
        const std::string_view ext)
    {
        const std::string key = std::string(kind)
            + "|"
            + ::sed::text::upperAscii(previewResourceLookupName(name, ext));
        const std::string revision = previewResourceRevisionToken(resources_.get(), name, ext);
        const auto [it, inserted] = observedResourceRevisions_.try_emplace(key, revision);
        if (inserted || it->second == revision)
        {
            return false;
        }
        it->second = revision;
        return true;
    }

    void refreshViewResources(const std::string_view modelName)
    {
        if (!view_)
        {
            return;
        }

        bool changed = observeResourceRevision("3DO", modelName, ".3do");
        changed = observeResourceRevision("CMP", cmpName_, ".cmp") || changed;

        std::shared_ptr<const assets::Model3do> model{};
        if (!findCachedSharedPreviewModel(resources_.get(), modelName, model))
        {
            model = loadSharedPreviewModel(resources_.get(), modelName);
        }
        if (model)
        {
            for (const std::string& material : model->materials)
            {
                changed = observeResourceRevision("MAT", material, ".mat") || changed;
            }
        }

        if (changed)
        {
            // The hidden GlView deliberately persists between thumbnails.
            // Invalidate its parsed-model, palette, texture, and missing-asset
            // caches only when a dependency it has already observed changes.
            view_->reloadCachedResources();
        }
    }

    void buildPreviewLevel(const std::string& modelName, const ::sed::ThingBox& box)
    {
        const std::string previewName = "__thumb_preview__|" + ::sed::text::upperAscii(modelName);

        templates_.clear();
        ::sed::ThingTemplate previewTemplate{};
        previewTemplate.name = previewName;
        previewTemplate.bbox = box;

        TplValue modelValue{};
        modelValue.name = "model3d";
        modelValue.type = ValueType::str;
        modelValue.adjType = AdjType::model3do;
        modelValue.stringValue = modelName;
        previewTemplate.values.push_back(std::move(modelValue));
        (void)templates_.addOrUpdate(previewTemplate, true);

        level_.clear();
        level_.kind = projectType_;
        level_.masterCMP = cmpName_;
        level_.ppunit = 320.0;
        level_.templates.clear();
        if (const ::sed::ThingTemplate* tpl = templates_.find(previewName))
        {
            level_.templates.push_back(*tpl);
        }
        level_.sectors.push_back(::sed::Sector{});
        level_.sectors.back().layer = 0;
        level_.sectors.back().ambient = ::sed::ColorWhite;

        ::sed::Thing previewThing{};
        previewThing.name = previewName;
        previewThing.templateName = previewName;
        previewThing.sector = 0;
        previewThing.bbox = box;
        level_.things.clear();
        level_.things.push_back(previewThing);

        view_->setLevel(&level_, GlView::SetLevelMode::resetView);
    }

    void fitToBox(const ::sed::ThingBox& box)
    {
        if (!view_)
        {
            return;
        }

        QVector3D minv(
            static_cast<float>(std::min(box.x1, box.x2)),
            static_cast<float>(std::min(box.y1, box.y2)),
            static_cast<float>(std::min(box.z1, box.z2)));
        QVector3D maxv(
            static_cast<float>(std::max(box.x1, box.x2)),
            static_cast<float>(std::max(box.y1, box.y2)),
            static_cast<float>(std::max(box.z1, box.z2)));
        if ((maxv - minv).lengthSquared() <= 1.0e-6f)
        {
            maxv += QVector3D(0.5f, 0.5f, 0.5f);
            minv -= QVector3D(0.5f, 0.5f, 0.5f);
        }

        const QVector3D center = (minv + maxv) * 0.5f;
        const QVector3D halfExtents = (maxv - minv) * 0.5f * kTemplatePreviewFitPadding;
        minv = center - halfExtents;
        maxv = center + halfExtents;

        view_->setOrbitViewImmediate(
            kTemplatePreviewDefaultYawDeg,
            kTemplatePreviewDefaultPitchDeg,
            kTemplatePreviewDefaultRollDeg);
        view_->frameWorldBounds(minv, maxv, false);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        view_->setOrbitViewImmediate(
            kTemplatePreviewDefaultYawDeg,
            kTemplatePreviewDefaultPitchDeg,
            kTemplatePreviewDefaultRollDeg);
        view_->frameWorldBounds(minv, maxv, false);
    }

    std::unique_ptr<GlView> view_{};
    assets::Templates templates_{};
    ::sed::Level level_{};
    // Rendering is UI-thread work, but it pumps non-input Qt events while
    // sizing/fitting the hidden view. Owning the same immutable snapshot as
    // the prep worker keeps the locator valid across those event turns too.
    std::shared_ptr<const assets::ResourceLocator> resources_{};
    ::sed::ProjectType projectType_{ ::sed::ProjectType::JKDF2 };
    std::string cmpName_{};
    std::uint64_t cacheEpoch_{};
    std::unordered_map<std::string, std::string> observedResourceRevisions_{};
};

inline std::shared_ptr<GlViewModelThumbnailRenderer>
acquireSharedGlViewModelThumbnailRenderer(
    std::shared_ptr<const assets::ResourceLocator> resources,
    const ::sed::ProjectType projectType,
    const std::string_view cmpName)
{
    static std::weak_ptr<GlViewModelThumbnailRenderer> sharedRenderer{};
    if (const std::shared_ptr<GlViewModelThumbnailRenderer> renderer =
            sharedRenderer.lock();
        renderer
        && renderer->matchesConfiguration(resources.get(), projectType, cmpName))
    {
        return renderer;
    }

    auto renderer = std::make_shared<GlViewModelThumbnailRenderer>(
        std::move(resources), projectType, std::string(cmpName));
    sharedRenderer = renderer;
    return renderer;
}

enum class SharedModelPreviewIconLoadResult : std::uint8_t
{
    assigned = 0,
    // A worker-rendered QImage was converted to QIcon on this GUI turn.
    rendered,
    pending,
    unavailable,
};

inline std::string sharedModelPreviewIconCacheKey(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view modelName,
    const QSize& iconSize,
    const bool wireframe)
{
    std::vector<std::string> materials{};
    std::shared_ptr<const assets::Model3do> model{};
    if (findCachedSharedPreviewModel(resources, modelName, model) && model)
    {
        materials = model->materials;
    }

    return std::string("3do:epoch=")
        + std::to_string(currentSharedPreviewCacheEpoch())
        + "|model="
        + ::sed::text::upperAscii(modelName)
        + "|deps="
        + previewModelDependencyRevisionToken(resources, cmpName, modelName, materials)
        + "|renderer=worker-raster-supersample-v3"
        + "|wireframe="
        + std::to_string(wireframe ? 1 : 0)
        + "|size="
        + std::to_string(iconSize.width())
        + "x"
        + std::to_string(iconSize.height());
}

inline std::string sharedModelPreviewIconHintKey(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view modelName,
    const QSize& iconSize,
    const bool wireframe)
{
    return std::string("3do-hint:locator=")
        + std::to_string(resources ? resources->previewCacheIdentity() : 0)
        + "|model="
        + ::sed::text::upperAscii(modelName)
        + "|cmp="
        + ::sed::text::upperAscii(cmpName)
        + "|wireframe="
        + std::to_string(wireframe ? 1 : 0)
        + "|size="
        + std::to_string(iconSize.width())
        + "x"
        + std::to_string(iconSize.height());
}

inline bool storeAuthoritativeSharedModelPreviewImage(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view modelName,
    const QSize& iconSize,
    const bool wireframe,
    const std::uint64_t renderEpoch,
    const QImage& image)
{
    if (!resources || modelName.empty() || image.isNull()
        || renderEpoch != currentSharedPreviewCacheEpoch())
    {
        return false;
    }

    const std::string cacheKey = sharedModelPreviewIconCacheKey(
        resources,
        cmpName,
        modelName,
        iconSize,
        wireframe);
    storeSharedTemplateModelPreviewImage(cacheKey, image);
    const std::string hintKey = sharedModelPreviewIconHintKey(
        resources, cmpName, modelName, iconSize, wireframe);
    {
        std::lock_guard<std::mutex> lock(sharedTemplateModelPreviewImageCacheMutex());
        auto& hints = sharedTemplateModelPreviewImageHintCache();
        if (hints.size() >= 4096u && hints.find(hintKey) == hints.end())
        {
            hints.erase(hints.begin());
        }
        hints[hintKey] = cacheKey;
    }
    return true;
}

inline QSize authoritativeModelPreviewRenderSize(const QSize& iconSize)
{
    constexpr int kSupersample = 2;
    constexpr int kMaximumDimension = 512;
    return QSize(
        std::clamp(iconSize.width() * kSupersample, 40, kMaximumDimension),
        std::clamp(iconSize.height() * kSupersample, 40, kMaximumDimension));
}

inline QImage fitAuthoritativeModelPreviewImage(
    const QImage& source,
    const QSize& targetSize)
{
    if (source.isNull() || targetSize.isEmpty())
    {
        return {};
    }

    QImage canvas(
        std::max(1, targetSize.width()),
        std::max(1, targetSize.height()),
        QImage::Format_ARGB32_Premultiplied);
    canvas.fill(source.pixelColor(0, 0));

    const QImage fitted = source.scaled(
        canvas.size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(
        QPoint(
            (canvas.width() - fitted.width()) / 2,
            (canvas.height() - fitted.height()) / 2),
        fitted);
    return canvas;
}

inline bool renderAndStoreSharedModelPreviewImage(
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view modelName,
    const QSize& iconSize,
    const bool wireframe,
    const std::uint64_t renderEpoch)
{
    if (!resources || modelName.empty() || iconSize.isEmpty()
        || renderEpoch != currentSharedPreviewCacheEpoch())
    {
        return false;
    }

    std::shared_ptr<const assets::Model3do> model{};
    if (!findCachedSharedPreviewModel(resources, modelName, model) || !model)
    {
        return false;
    }

    const QSize renderSize = authoritativeModelPreviewRenderSize(iconSize);
    const QImage rendered = makeTemplateModelPreviewImage(
        *model,
        renderSize,
        resources,
        cmpName,
        wireframe);
    const QImage fitted = fitAuthoritativeModelPreviewImage(rendered, iconSize);
    return storeAuthoritativeSharedModelPreviewImage(
        resources,
        cmpName,
        modelName,
        iconSize,
        wireframe,
        renderEpoch,
        fitted);
}

inline void pruneSharedPreviewIconCacheForInsert(
    std::unordered_map<std::string, QIcon>& localCache,
    const std::string& cacheKey,
    const std::size_t maxEntries = 192u)
{
    if (localCache.size() >= maxEntries && localCache.find(cacheKey) == localCache.end())
    {
        localCache.erase(localCache.begin());
    }
}

inline bool findCachedSharedModelPreviewIcon(
    std::unordered_map<std::string, QIcon>& localCache,
    const assets::ResourceLocator* resources,
    const std::string_view cmpName,
    const std::string_view modelName,
    const QSize& iconSize,
    const bool wireframe,
    QIcon& outIcon,
    bool* revisionCheckedForTest = nullptr,
    bool* convertedSharedImage = nullptr)
{
    if (revisionCheckedForTest)
    {
        *revisionCheckedForTest = false;
    }
    if (convertedSharedImage)
    {
        *convertedSharedImage = false;
    }
    if (!resources || modelName.empty())
    {
        return false;
    }

    const std::string hintKey = sharedModelPreviewIconHintKey(
        resources, cmpName, modelName, iconSize, wireframe);
    std::optional<std::string> hintedCacheKey{};
    {
        std::lock_guard<std::mutex> lock(sharedTemplateModelPreviewImageCacheMutex());
        const auto& hints = sharedTemplateModelPreviewImageHintCache();
        if (const auto it = hints.find(hintKey); it != hints.end())
        {
            hintedCacheKey = it->second;
        }
    }
    if (!hintedCacheKey)
    {
        return false;
    }

    // The worker has already resolved every 3DO/CMP/MAT revision and stored
    // the resulting exact cache key together with the QImage. Rebuilding that
    // key here would synchronously stat every dependency on the GUI thread for
    // each completed row. The epoch is the publication gate: cache
    // invalidation clears the hint map and advances it, while this explicit
    // check also closes the small race after the hint was copied.
    const std::uint64_t expectedEpoch = currentSharedPreviewCacheEpoch();
    const std::string epochPrefix = std::string("3do:epoch=")
        + std::to_string(expectedEpoch)
        + "|";
    if (!hintedCacheKey->starts_with(epochPrefix))
    {
        return false;
    }
    if (const auto it = localCache.find(*hintedCacheKey);
        it != localCache.end())
    {
        outIcon = it->second;
        if (expectedEpoch != currentSharedPreviewCacheEpoch())
        {
            outIcon = {};
            return false;
        }
        return true;
    }

    if (const std::optional<QImage> sharedImage =
            findSharedTemplateModelPreviewImage(*hintedCacheKey))
    {
        outIcon = QIcon(QPixmap::fromImage(*sharedImage));
        if (expectedEpoch != currentSharedPreviewCacheEpoch())
        {
            outIcon = {};
            return false;
        }
        pruneSharedPreviewIconCacheForInsert(localCache, *hintedCacheKey);
        localCache[*hintedCacheKey] = outIcon;
        if (convertedSharedImage)
        {
            *convertedSharedImage = true;
        }
        return true;
    }

    return false;
}

template <typename QueuePrepFn>
inline SharedModelPreviewIconLoadResult loadSharedModelPreviewIcon(
    std::unordered_map<std::string, QIcon>& localCache,
    std::shared_ptr<GlViewModelThumbnailRenderer>& renderer,
    std::shared_ptr<const assets::ResourceLocator> resources,
    const ::sed::ProjectType projectType,
    const std::string_view cmpName,
    const std::string_view modelName,
    const QSize& iconSize,
    const bool wireframe,
    const int activePrepJobs,
    QueuePrepFn&& queuePrep,
    QIcon& outIcon,
    bool* revisionCheckedForTest = nullptr)
{
    // Take shared ownership by value: the caller's published snapshot member
    // can change while a queued worker retains the current resource generation.
    if (!resources || modelName.empty())
    {
        return SharedModelPreviewIconLoadResult::unavailable;
    }

    // The list-thumbnail image is rendered entirely in the dedicated preview
    // worker. Keep this legacy parameter only until callers no longer need to
    // carry renderer state for older cache generations.
    renderer.reset();
    (void)projectType;

    bool convertedSharedImage = false;
    if (findCachedSharedModelPreviewIcon(
            localCache,
            resources.get(),
            cmpName,
            modelName,
            iconSize,
            wireframe,
            outIcon,
            revisionCheckedForTest,
            &convertedSharedImage))
    {
        return convertedSharedImage
            ? SharedModelPreviewIconLoadResult::rendered
            : SharedModelPreviewIconLoadResult::assigned;
    }

    std::optional<::sed::ThingBox> preparedBox{};
    if (!findSharedPreviewModelBox(resources.get(), modelName, preparedBox))
    {
        const auto kThumbTuning = sharedModelThumbLoadTuning();
        if (activePrepJobs < kThumbTuning.maxPrepJobs)
        {
            queuePrep(std::string(modelName));
        }
        return SharedModelPreviewIconLoadResult::pending;
    }

    if (!preparedBox)
    {
        return SharedModelPreviewIconLoadResult::unavailable;
    }

    // Retry after preparation so another picker can publish the exact same
    // authoritative rendered image while this row was waiting.
    convertedSharedImage = false;
    if (findCachedSharedModelPreviewIcon(
            localCache,
            resources.get(),
            cmpName,
            modelName,
            iconSize,
            wireframe,
            outIcon,
            revisionCheckedForTest,
            &convertedSharedImage))
    {
        return convertedSharedImage
            ? SharedModelPreviewIconLoadResult::rendered
            : SharedModelPreviewIconLoadResult::assigned;
    }

    // Preparation now includes supersampled software rasterization and
    // revision-keyed QImage publication. The box hint above is also published
    // by that worker, so a remaining image miss only means rendering is still
    // in flight. Never rebuild the model dependency key or fall back to
    // GUI-thread OpenGL while polling this row.
    const auto kThumbTuning = sharedModelThumbLoadTuning();
    if (activePrepJobs < kThumbTuning.maxPrepJobs)
    {
        queuePrep(std::string(modelName));
    }
    return SharedModelPreviewIconLoadResult::pending;
}

} // namespace sed::ui::detail
