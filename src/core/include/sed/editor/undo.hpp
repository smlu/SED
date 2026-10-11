#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sed::editor
{

template <typename T>
struct UndoRecord
{
    std::string label;
    std::uint32_t dirtyFlags{};
    T state;
};

template <typename T>
class UndoHistory
{
public:
    UndoHistory() = default;
    explicit UndoHistory(T initial) : current_(std::move(initial))
    {
    }

    const T& current() const noexcept
    {
        return current_;
    }

    T& current() noexcept
    {
        return current_;
    }

    void clear(T initial = T{})
    {
        current_ = std::move(initial);
        undo_.clear();
        redo_.clear();
    }

    bool canUndo() const noexcept
    {
        return !undo_.empty();
    }

    bool canRedo() const noexcept
    {
        return !redo_.empty();
    }

    std::string_view nextUndoLabel() const noexcept
    {
        return undo_.empty() ? std::string_view{} : std::string_view(undo_.back().label);
    }

    std::string_view nextRedoLabel() const noexcept
    {
        return redo_.empty() ? std::string_view{} : std::string_view(redo_.back().label);
    }

    void pushUndo(std::string label)
    {
        pushUndo(std::move(label), 0);
    }

    void pushUndo(std::string label, const std::uint32_t dirtyFlags)
    {
        undo_.push_back(UndoRecord<T>{ std::move(label), dirtyFlags, current_ });
        redo_.clear();
    }

    template <typename Fn>
    bool apply(std::string label, Fn&& mutate)
    {
        return apply(std::move(label), 0, std::forward<Fn>(mutate));
    }

    template <typename Fn>
    bool apply(std::string label, const std::uint32_t dirtyFlags, Fn&& mutate)
    {
        T before = current_;
        if (!mutate(current_))
        {
            current_ = std::move(before);
            return false;
        }

        undo_.push_back(UndoRecord<T>{ std::move(label), dirtyFlags, std::move(before) });
        redo_.clear();
        return true;
    }

    bool undo(std::uint32_t* outDirtyFlags = nullptr)
    {
        if (undo_.empty())
        {
            return false;
        }

        UndoRecord<T> record = std::move(undo_.back());
        undo_.pop_back();

        const std::uint32_t dirtyFlags = record.dirtyFlags;
        redo_.push_back(UndoRecord<T>{ std::move(record.label), dirtyFlags, std::move(current_) });
        current_ = std::move(record.state);

        if (outDirtyFlags)
        {
            *outDirtyFlags = dirtyFlags;
        }
        return true;
    }

    bool redo(std::uint32_t* outDirtyFlags = nullptr)
    {
        if (redo_.empty())
        {
            return false;
        }

        UndoRecord<T> record = std::move(redo_.back());
        redo_.pop_back();

        const std::uint32_t dirtyFlags = record.dirtyFlags;
        undo_.push_back(UndoRecord<T>{ std::move(record.label), dirtyFlags, std::move(current_) });
        current_ = std::move(record.state);

        if (outDirtyFlags)
        {
            *outDirtyFlags = dirtyFlags;
        }
        return true;
    }

private:
    T current_{};
    std::vector<UndoRecord<T>> undo_{};
    std::vector<UndoRecord<T>> redo_{};
};

} // namespace sed::editor
