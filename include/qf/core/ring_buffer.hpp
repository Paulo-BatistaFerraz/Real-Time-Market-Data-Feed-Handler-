#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>
#include "qf/common/constants.hpp"

namespace qf::core {

// ── Access-pattern policies ─────────────────────────────────────────
// Each policy defines how head_ and tail_ are read/advanced by
// producers and consumers. The RingBuffer delegates to these.

struct SPSCPolicy {
    // Producer claims next slot — single producer, no CAS needed
    static bool claim_head(std::atomic<size_t>& head,
                           std::atomic<size_t>& /*committed*/,
                           const std::atomic<size_t>& tail,
                           size_t mask, size_t& slot) {
        const size_t h = head.load(std::memory_order_relaxed);
        const size_t next = (h + 1) & mask;
        if (next == tail.load(std::memory_order_acquire)) {
            return false;  // full
        }
        slot = h;
        head.store(next, std::memory_order_release);
        return true;
    }

    // Single producer — commit is a no-op (head advance *is* the commit)
    static void commit(std::atomic<size_t>& /*committed*/,
                       size_t /*slot*/, size_t /*next*/, size_t /*mask*/) {}

    // Consumer advances tail — single consumer, no CAS needed
    static bool claim_tail(std::atomic<size_t>& tail,
                           const std::atomic<size_t>& head,
                           const std::atomic<size_t>& /*committed*/,
                           size_t mask, size_t& slot) {
        const size_t t = tail.load(std::memory_order_relaxed);
        if (t == head.load(std::memory_order_acquire)) {
            return false;  // empty
        }
        slot = t;
        tail.store((t + 1) & mask, std::memory_order_release);
        return true;
    }

    // Readable count: distance from tail to head
    static size_t readable(const std::atomic<size_t>& head,
                           const std::atomic<size_t>& tail,
                           const std::atomic<size_t>& /*committed*/,
                           size_t mask) {
        return (head.load(std::memory_order_acquire)
              - tail.load(std::memory_order_acquire)) & mask;
    }

    static bool is_empty(const std::atomic<size_t>& head,
                         const std::atomic<size_t>& tail,
                         const std::atomic<size_t>& /*committed*/) {
        return head.load(std::memory_order_acquire)
            == tail.load(std::memory_order_acquire);
    }
};

struct MPSCPolicy {
    // Producer claims next slot via CAS — multiple producers
    static bool claim_head(std::atomic<size_t>& head,
                           std::atomic<size_t>& /*committed*/,
                           const std::atomic<size_t>& tail,
                           size_t mask, size_t& slot) {
        size_t h = head.load(std::memory_order_relaxed);
        for (;;) {
            const size_t next = (h + 1) & mask;
            if (next == tail.load(std::memory_order_acquire)) {
                return false;  // full
            }
            if (head.compare_exchange_weak(h, next,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                slot = h;
                return true;
            }
            // CAS failed — h updated by another producer, retry
        }
    }

    // Spin until prior producers finish, then advance committed_
    static void commit(std::atomic<size_t>& committed,
                       size_t slot, size_t /*next*/, size_t mask) {
        const size_t next = (slot + 1) & mask;
        while (committed.load(std::memory_order_acquire) != slot) {
            // spin — prior producer is mid-write
        }
        committed.store(next, std::memory_order_release);
    }

    // Consumer advances tail — single consumer only
    static bool claim_tail(std::atomic<size_t>& tail,
                           const std::atomic<size_t>& /*head*/,
                           const std::atomic<size_t>& committed,
                           size_t mask, size_t& slot) {
        const size_t t = tail.load(std::memory_order_relaxed);
        if (t == committed.load(std::memory_order_acquire)) {
            return false;  // empty (only read up to committed)
        }
        slot = t;
        tail.store((t + 1) & mask, std::memory_order_release);
        return true;
    }

    // Readable count: distance from tail to committed
    static size_t readable(const std::atomic<size_t>& /*head*/,
                           const std::atomic<size_t>& tail,
                           const std::atomic<size_t>& committed,
                           size_t mask) {
        return (committed.load(std::memory_order_acquire)
              - tail.load(std::memory_order_acquire)) & mask;
    }

    static bool is_empty(const std::atomic<size_t>& /*head*/,
                         const std::atomic<size_t>& tail,
                         const std::atomic<size_t>& committed) {
        return committed.load(std::memory_order_acquire)
            == tail.load(std::memory_order_acquire);
    }
};

struct MPMCPolicy {
    // Producer claims next slot via CAS — multiple producers
    static bool claim_head(std::atomic<size_t>& head,
                           std::atomic<size_t>& /*committed*/,
                           const std::atomic<size_t>& tail,
                           size_t mask, size_t& slot) {
        size_t h = head.load(std::memory_order_relaxed);
        for (;;) {
            const size_t next = (h + 1) & mask;
            if (next == tail.load(std::memory_order_acquire)) {
                return false;  // full
            }
            if (head.compare_exchange_weak(h, next,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                slot = h;
                return true;
            }
        }
    }

    // Same commit logic as MPSC — ordered commit via spinning
    static void commit(std::atomic<size_t>& committed,
                       size_t slot, size_t /*next*/, size_t mask) {
        const size_t next = (slot + 1) & mask;
        while (committed.load(std::memory_order_acquire) != slot) {
            // spin — prior producer is mid-write
        }
        committed.store(next, std::memory_order_release);
    }

    // Consumer claims tail via CAS — multiple consumers
    static bool claim_tail(std::atomic<size_t>& tail,
                           const std::atomic<size_t>& /*head*/,
                           const std::atomic<size_t>& committed,
                           size_t mask, size_t& slot) {
        size_t t = tail.load(std::memory_order_relaxed);
        for (;;) {
            if (t == committed.load(std::memory_order_acquire)) {
                return false;  // empty
            }
            if (tail.compare_exchange_weak(t, (t + 1) & mask,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                slot = t;
                return true;
            }
        }
    }

    // Readable count: distance from tail to committed
    static size_t readable(const std::atomic<size_t>& /*head*/,
                           const std::atomic<size_t>& tail,
                           const std::atomic<size_t>& committed,
                           size_t mask) {
        return (committed.load(std::memory_order_acquire)
              - tail.load(std::memory_order_acquire)) & mask;
    }

    static bool is_empty(const std::atomic<size_t>& /*head*/,
                         const std::atomic<size_t>& tail,
                         const std::atomic<size_t>& committed) {
        return committed.load(std::memory_order_acquire)
            == tail.load(std::memory_order_acquire);
    }
};

// ── RingBuffer ──────────────────────────────────────────────────────
// Generic lock-free ring buffer parameterised by access policy.
// - Power-of-2 capacity with index masking (no modulo)
// - Cache-line aligned atomics to prevent false sharing
// - Zero dynamic allocation — fixed buffer in-place
// - Policy selects SPSC, MPSC, or MPMC access pattern
template <typename T, size_t Capacity, typename Policy = SPSCPolicy>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity > 0, "Capacity must be > 0");

public:
    RingBuffer() : head_(0), tail_(0), committed_(0) {}

    // Non-copyable, non-movable (contains atomics)
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    // Push by const ref. Returns false if full.
    bool try_push(const T& item) {
        size_t slot;
        if (!Policy::claim_head(head_, committed_, tail_, MASK, slot)) {
            return false;
        }
        buffer_[slot] = item;
        Policy::commit(committed_, slot, (slot + 1) & MASK, MASK);
        return true;
    }

    // Push by move. Returns false if full.
    bool try_push(T&& item) {
        size_t slot;
        if (!Policy::claim_head(head_, committed_, tail_, MASK, slot)) {
            return false;
        }
        buffer_[slot] = std::move(item);
        Policy::commit(committed_, slot, (slot + 1) & MASK, MASK);
        return true;
    }

    // Pop a single element. Returns false if empty.
    bool try_pop(T& item) {
        size_t slot;
        if (!Policy::claim_tail(tail_, head_, committed_, MASK, slot)) {
            return false;
        }
        item = std::move(buffer_[slot]);
        return true;
    }

    // Batch pop: returns number of items popped (up to max_count)
    size_t try_pop_batch(T* items, size_t max_count) {
        size_t popped = 0;
        for (size_t i = 0; i < max_count; ++i) {
            if (!try_pop(items[i])) break;
            ++popped;
        }
        return popped;
    }

    // Approximate count of readable items (racy but useful for diagnostics)
    size_t size() const {
        return Policy::readable(head_, tail_, committed_, MASK);
    }

    bool empty() const { return Policy::is_empty(head_, tail_, committed_); }
    bool full() const { return size() == Capacity - 1; }

    static constexpr size_t capacity() { return Capacity - 1; }  // usable slots

private:
    static constexpr size_t MASK = Capacity - 1;

    T buffer_[Capacity];

    // Each atomic on its own cache line to prevent false sharing
    alignas(CACHE_LINE) std::atomic<size_t> head_;
    alignas(CACHE_LINE) std::atomic<size_t> tail_;
    alignas(CACHE_LINE) std::atomic<size_t> committed_;  // used by MPSC/MPMC policies
};

}  // namespace qf::core
