#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

namespace qf::core {

// Pre-allocated fixed-size object pool
// - Zero dynamic allocation after construction
// - Lock-free acquire/release via atomic free-list index stack
// - Objects are stored in a flat array; acquire() returns a pointer
// - Thread-safe for concurrent acquire/release
template <typename T, size_t N>
class ObjectPool {
    static_assert(N > 0, "Pool size must be > 0");

public:
    ObjectPool() {
        // Build the free list: each slot points to the next free slot
        for (size_t i = 0; i < N - 1; ++i) {
            free_list_[i].next = static_cast<uint32_t>(i + 1);
        }
        free_list_[N - 1].next = INVALID;
        head_.store(0, std::memory_order_relaxed);
    }

    // Acquire an object from the pool. Returns nullptr if pool is exhausted.
    T* acquire() {
        uint64_t old_head = head_.load(std::memory_order_acquire);
        for (;;) {
            uint32_t idx = index_of(old_head);
            if (idx == INVALID) {
                return nullptr;  // pool exhausted
            }
            uint32_t next = free_list_[idx].next;
            uint32_t tag = tag_of(old_head) + 1;
            uint64_t new_head = make_tagged(next, tag);
            if (head_.compare_exchange_weak(old_head, new_head,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return reinterpret_cast<T*>(&storage_[idx]);
            }
            // CAS failed, old_head updated — retry
        }
    }

    // Release an object back to the pool. ptr must have been obtained from acquire().
    void release(T* ptr) {
        auto* raw = reinterpret_cast<Storage*>(ptr);
        size_t idx = static_cast<size_t>(raw - storage_);
        uint64_t old_head = head_.load(std::memory_order_acquire);
        for (;;) {
            free_list_[idx].next = index_of(old_head);
            uint32_t tag = tag_of(old_head) + 1;
            uint64_t new_head = make_tagged(static_cast<uint32_t>(idx), tag);
            if (head_.compare_exchange_weak(old_head, new_head,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return;
            }
        }
    }

    static constexpr size_t pool_size() { return N; }

private:
    static constexpr uint32_t INVALID = 0xFFFFFFFF;

    // Tagged pointer helpers to prevent ABA problem
    static uint32_t index_of(uint64_t tagged) { return static_cast<uint32_t>(tagged & 0xFFFFFFFF); }
    static uint32_t tag_of(uint64_t tagged) { return static_cast<uint32_t>(tagged >> 32); }
    static uint64_t make_tagged(uint32_t idx, uint32_t tag) {
        return (static_cast<uint64_t>(tag) << 32) | idx;
    }

    // Storage: aligned raw bytes so we don't require T to be default-constructible
    using Storage = typename std::aligned_storage<sizeof(T), alignof(T)>::type;
    Storage storage_[N];

    // Free-list node: just an index to the next free slot
    struct FreeNode {
        uint32_t next;
    };
    FreeNode free_list_[N];

    // Atomic tagged head of free list (index + ABA tag)
    alignas(64) std::atomic<uint64_t> head_;
};

}  // namespace qf::core
