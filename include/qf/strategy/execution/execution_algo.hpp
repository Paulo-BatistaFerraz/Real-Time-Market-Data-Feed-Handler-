#pragma once

#include <cstdint>
#include <vector>
#include "qf/common/types.hpp"
#include "qf/strategy/strategy_types.hpp"

namespace qf::strategy {

// ChildOrder — a slice produced by an execution algorithm.
struct ChildOrder {
    Symbol    symbol;
    Side      side;
    Quantity  quantity;
    OrderType order_type;
    Price     limit_price;  // 0 for market orders
};

// ExecutionAlgo — base class for order-slicing algorithms.
//
// A parent order (Decision) is broken into smaller child orders that are
// released one at a time via next_slice().  Concrete implementations
// determine the scheduling strategy (equal time slices, volume-weighted,
// iceberg, etc.).
//
// Lifetime: create → call next_slice() in a loop → is_complete().
class ExecutionAlgo {
public:
    virtual ~ExecutionAlgo() = default;

    // Return the next child order to send.  Returns a slice with
    // quantity == 0 when no slice is ready yet (caller should wait).
    virtual ChildOrder next_slice() = 0;

    // Notify the algo that a child order was filled.
    virtual void on_fill(Quantity filled_qty) = 0;

    // True when the parent order is fully executed.
    virtual bool is_complete() const = 0;

    // --- Tracking accessors ---

    Quantity remaining_qty()  const { return remaining_qty_; }
    uint32_t slices_sent()    const { return slices_sent_; }
    uint32_t slices_filled()  const { return slices_filled_; }

protected:
    Symbol    symbol_;
    Side      side_;
    OrderType order_type_{OrderType::Market};
    Price     limit_price_{0};

    Quantity  total_qty_{0};
    Quantity  remaining_qty_{0};
    uint32_t  slices_sent_{0};
    uint32_t  slices_filled_{0};
    Quantity  filled_qty_{0};
};

// ---------------------------------------------------------------------------
// TWAP — Time-Weighted Average Price
//
// Splits the parent order into equal-sized slices distributed uniformly
// over a configurable number of intervals.  The last slice absorbs any
// remainder due to integer division.
// ---------------------------------------------------------------------------
class TwapAlgo : public ExecutionAlgo {
public:
    // num_slices — how many child orders to split into.
    TwapAlgo(const Decision& parent, uint32_t num_slices);

    ChildOrder next_slice() override;
    void       on_fill(Quantity filled_qty) override;
    bool       is_complete() const override;

private:
    uint32_t num_slices_;
    Quantity slice_size_;       // base slice (total / num_slices)
    Quantity last_slice_size_;  // last slice absorbs remainder
};

// ---------------------------------------------------------------------------
// VWAP — Volume-Weighted Average Price
//
// Front-loads slices according to an expected intra-day volume profile.
// The profile is a vector of relative weights (e.g. from historical volume
// buckets).  Each slice quantity is proportional to its weight.
// ---------------------------------------------------------------------------
class VwapAlgo : public ExecutionAlgo {
public:
    // volume_profile — relative weights per bucket (need not sum to 1).
    VwapAlgo(const Decision& parent,
             const std::vector<double>& volume_profile);

    ChildOrder next_slice() override;
    void       on_fill(Quantity filled_qty) override;
    bool       is_complete() const override;

private:
    std::vector<Quantity> slice_sizes_;  // pre-computed per-bucket qty
    uint32_t             current_idx_{0};
};

// ---------------------------------------------------------------------------
// Iceberg — shows a visible quantity, replenishes on fill.
//
// Only a configurable 'visible_qty' is shown at any time.  When the
// visible tranche is filled, a new tranche is sent until the parent order
// is fully executed.
// ---------------------------------------------------------------------------
class IcebergAlgo : public ExecutionAlgo {
public:
    // visible_qty — maximum quantity shown at once.
    IcebergAlgo(const Decision& parent, Quantity visible_qty);

    ChildOrder next_slice() override;
    void       on_fill(Quantity filled_qty) override;
    bool       is_complete() const override;

private:
    Quantity visible_qty_;
    Quantity current_tranche_remaining_{0};
    bool     needs_replenish_{true};
};

}  // namespace qf::strategy
