#pragma once

#include <cstddef>
#include <functional>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace sed::ui::detail
{

inline constexpr std::size_t kPreviewCacheMiB = 1024u * 1024u;
inline constexpr std::size_t kRenderedModelPreviewBudget = 192u * kPreviewCacheMiB;
inline constexpr std::size_t kParsedModelPreviewBudget = 384u * kPreviewCacheMiB;
inline constexpr std::size_t kMaterialPreviewBudget = 192u * kPreviewCacheMiB;
inline constexpr std::size_t kSpriteImagePreviewBudget = 96u * kPreviewCacheMiB;
inline constexpr std::size_t kParsedSpritePreviewBudget = 8u * kPreviewCacheMiB;

// A small 2Q cache for preview assets. New entries are probationary and a
// second access promotes them to the protected queue. This approximates K=2
// recency without keeping an unbounded access history or paying K-LRU's scan
// and metadata costs on the thumbnail hot path.
template <typename Value>
class BoundedTwoQueueCache final
{
public:
    using WeightFunction = std::function<std::size_t(const Value&)>;

    explicit BoundedTwoQueueCache(
        const std::size_t maximumWeight,
        WeightFunction weightFunction)
        : maximumWeight_(maximumWeight)
        , weightFunction_(std::move(weightFunction))
    {
    }

    bool find(const std::string_view key, Value& value)
    {
        const auto it = entries_.find(std::string(key));
        if ( it == entries_.end() )
        {
            return false;
        }

        touch(it->first, it->second);
        value = it->second.value;
        return true;
    }

    void insertOrAssign(std::string key, Value value)
    {
        const std::size_t weight = weightFunction_
            ? weightFunction_(value)
            : sizeof(Value);
        if ( maximumWeight_ == 0u || weight > maximumWeight_ )
        {
            erase(key);
            return;
        }

        if ( const auto existing = entries_.find(key);
            existing != entries_.end() )
        {
            currentWeight_ -= existing->second.weight;
            existing->second.value = std::move(value);
            existing->second.weight = weight;
            currentWeight_ += weight;
            touch(existing->first, existing->second);
            evictToBudget();
            return;
        }

        probationary_.push_front(key);
        Entry entry{};
        entry.value = std::move(value);
        entry.weight = weight;
        entry.protectedEntry = false;
        entry.position = probationary_.begin();
        currentWeight_ += weight;
        entries_.emplace(std::move(key), std::move(entry));
        evictToBudget();
    }

    void clear()
    {
        entries_.clear();
        probationary_.clear();
        protected_.clear();
        currentWeight_ = 0u;
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return entries_.size();
    }

    [[nodiscard]] std::size_t currentWeight() const noexcept
    {
        return currentWeight_;
    }

    [[nodiscard]] std::size_t maximumWeight() const noexcept
    {
        return maximumWeight_;
    }

private:
    struct Entry
    {
        Value value{};
        std::size_t weight{};
        bool protectedEntry{};
        typename std::list<std::string>::iterator position{};
    };

    void touch(const std::string& key, Entry& entry)
    {
        if ( entry.protectedEntry )
        {
            protected_.splice(protected_.begin(), protected_, entry.position);
            entry.position = protected_.begin();
            return;
        }

        probationary_.erase(entry.position);
        protected_.push_front(key);
        entry.protectedEntry = true;
        entry.position = protected_.begin();
    }

    void erase(const std::string_view key)
    {
        const auto it = entries_.find(std::string(key));
        if ( it == entries_.end() )
        {
            return;
        }

        currentWeight_ -= it->second.weight;
        if ( it->second.protectedEntry )
        {
            protected_.erase(it->second.position);
        }
        else
        {
            probationary_.erase(it->second.position);
        }
        entries_.erase(it);
    }

    void evictToBudget()
    {
        while ( currentWeight_ > maximumWeight_ && !entries_.empty() )
        {
            if ( !probationary_.empty() )
            {
                erase(probationary_.back());
                continue;
            }
            erase(protected_.back());
        }
    }

    std::size_t maximumWeight_{};
    WeightFunction weightFunction_{};
    std::size_t currentWeight_{};
    std::list<std::string> probationary_{};
    std::list<std::string> protected_{};
    std::unordered_map<std::string, Entry> entries_{};
};

} // namespace sed::ui::detail
