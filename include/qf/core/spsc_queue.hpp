#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>
#include "qf/common/constants.hpp"

namespace qf::core {

// Lock-free Single-Producer Single-Consumer ring buffer
// - Power-of-2 capacity with index masking (no modulo)
// - Cache-line aligned head/tail to prevent false sharing
// - acquire/release memory ordering (no seq_cst)
template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity > 0, "Capacity must be > 0");

public:
    SPSCQueue() : head_(0), tail_(0) {}

    // Producer: try to push an element. Returns false if full.
    bool try_push(const T& item) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & MASK;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;  // full
        }
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Producer: try to push by move
    bool try_push(T&& item) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & MASK;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        buffer_[head] = std::move(item);
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer: try to pop an element. Returns false if empty.
    bool try_pop(T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;  // empty
        }
        item = std::move(buffer_[tail]);
        tail_.store((tail + 1) & MASK, std::memory_order_release);
        return true;
    }

    // Batch push: returns number of items pushed
    size_t try_push_batch(const T* items, size_t count) {
        size_t pushed = 0;
        for (size_t i = 0; i < count; ++i) {
            if (!try_push(items[i])) break;
            ++pushed;
        }
        return pushed;
    }

    // Batch pop: returns number of items popped
    size_t try_pop_batch(T* items, size_t max_count) {
        size_t popped = 0;
        for (size_t i = 0; i < max_count; ++i) {
            if (!try_pop(items[i])) break;
            ++popped;
        }
        return popped;
    }

    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & MASK;
    }

    bool empty() const { return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire); }
    bool full() const { return size() == Capacity - 1; }

    static constexpr size_t capacity() { return Capacity - 1; }  // usable slots

private:
    static constexpr size_t MASK = Capacity - 1;

    T buffer_[Capacity];

    // Each on its own cache line to prevent false sharing
    alignas(CACHE_LINE) std::atomic<size_t> head_;
    alignas(CACHE_LINE) std::atomic<size_t> tail_;
};

}  // namespace qf::core
