#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>
#include "qf/common/constants.hpp"

namespace qf::core {

// Lock-free Multi-Producer Single-Consumer ring buffer
// - Power-of-2 capacity with index masking (no modulo)
// - Producers use atomic CAS on head_ to claim a slot
// - Consumer uses simple load on tail_ (single reader)
// - Cache-line aligned head/tail/commit to prevent false sharing
// - Zero dynamic allocation — fixed buffer in-place
template <typename T, size_t Capacity>
class MPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity > 0, "Capacity must be > 0");

public:
    MPSCQueue() : head_(0), tail_(0), committed_(0) {}

    // Producer (thread-safe, multiple callers): try to push an element.
    // Returns false if full.
    bool try_push(const T& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        for (;;) {
            const size_t next = (head + 1) & MASK;
            // Check if full: next slot would collide with tail
            if (next == tail_.load(std::memory_order_acquire)) {
                return false;
            }
            // CAS to claim the slot at 'head'
            if (head_.compare_exchange_weak(head, next,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                // Slot claimed — write the data
                buffer_[head] = item;
                // Wait for prior producers to finish their writes, then advance committed_
                // This ensures the consumer sees items in order
                while (committed_.load(std::memory_order_acquire) != head) {
                    // spin — brief; prior producer is mid-write
                }
                committed_.store(next, std::memory_order_release);
                return true;
            }
            // CAS failed — head was updated by another producer, retry with new value
        }
    }

    // Producer (thread-safe): try to push by move
    bool try_push(T&& item) {
        size_t head = head_.load(std::memory_order_relaxed);
        for (;;) {
            const size_t next = (head + 1) & MASK;
            if (next == tail_.load(std::memory_order_acquire)) {
                return false;
            }
            if (head_.compare_exchange_weak(head, next,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                buffer_[head] = std::move(item);
                while (committed_.load(std::memory_order_acquire) != head) {
                }
                committed_.store(next, std::memory_order_release);
                return true;
            }
        }
    }

    // Consumer (single thread only): try to pop an element.
    // Returns false if empty.
    bool try_pop(T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        // Only read up to committed_ (not head_) to ensure data is fully written
        if (tail == committed_.load(std::memory_order_acquire)) {
            return false;  // empty
        }
        item = std::move(buffer_[tail]);
        tail_.store((tail + 1) & MASK, std::memory_order_release);
        return true;
    }

    // Consumer: batch pop, returns number of items popped
    size_t try_pop_batch(T* items, size_t max_count) {
        size_t popped = 0;
        for (size_t i = 0; i < max_count; ++i) {
            if (!try_pop(items[i])) break;
            ++popped;
        }
        return popped;
    }

    size_t size() const {
        const size_t committed = committed_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (committed - tail) & MASK;
    }

    bool empty() const {
        return committed_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    bool full() const { return size() == Capacity - 1; }

    static constexpr size_t capacity() { return Capacity - 1; }

private:
    static constexpr size_t MASK = Capacity - 1;

    T buffer_[Capacity];

    // head_: next slot to claim (CAS by producers)
    alignas(CACHE_LINE) std::atomic<size_t> head_;
    // tail_: next slot to read  (updated by single consumer)
    alignas(CACHE_LINE) std::atomic<size_t> tail_;
    // committed_: all slots up to this index have data written
    alignas(CACHE_LINE) std::atomic<size_t> committed_;
};

}  // namespace qf::core
