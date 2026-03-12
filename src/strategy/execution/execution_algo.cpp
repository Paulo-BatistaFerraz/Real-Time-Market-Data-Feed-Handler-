#include "qf/strategy/execution/execution_algo.hpp"

#include <algorithm>
#include <numeric>

namespace qf::strategy {

// ===========================================================================
// TwapAlgo
// ===========================================================================

TwapAlgo::TwapAlgo(const Decision& parent, uint32_t num_slices)
    : num_slices_(std::max(num_slices, 1u))
{
    symbol_      = parent.symbol;
    side_        = parent.side;
    order_type_  = parent.order_type;
    limit_price_ = parent.limit_price;
    total_qty_   = parent.quantity;
    remaining_qty_ = parent.quantity;

    slice_size_      = total_qty_ / num_slices_;
    last_slice_size_ = total_qty_ - slice_size_ * (num_slices_ - 1);
}

ChildOrder TwapAlgo::next_slice() {
    if (is_complete() || slices_sent_ >= num_slices_) {
        return ChildOrder{symbol_, side_, 0, order_type_, limit_price_};
    }

    bool is_last = (slices_sent_ == num_slices_ - 1);
    Quantity qty = is_last ? last_slice_size_ : slice_size_;

    // Don't overshoot remaining
    qty = std::min(qty, remaining_qty_);

    ++slices_sent_;

    return ChildOrder{symbol_, side_, qty, order_type_, limit_price_};
}

void TwapAlgo::on_fill(Quantity qty) {
    Quantity actual = std::min(qty, remaining_qty_);
    filled_qty_    += actual;
    remaining_qty_ -= actual;
    ++slices_filled_;
}

bool TwapAlgo::is_complete() const {
    return remaining_qty_ == 0;
}

// ===========================================================================
// VwapAlgo
// ===========================================================================

VwapAlgo::VwapAlgo(const Decision& parent,
                   const std::vector<double>& volume_profile)
{
    symbol_      = parent.symbol;
    side_        = parent.side;
    order_type_  = parent.order_type;
    limit_price_ = parent.limit_price;
    total_qty_   = parent.quantity;
    remaining_qty_ = parent.quantity;

    // Normalise weights and compute per-bucket quantities.
    double total_weight = std::accumulate(volume_profile.begin(),
                                          volume_profile.end(), 0.0);
    if (total_weight <= 0.0 || volume_profile.empty()) {
        // Fallback: single slice with entire quantity
        slice_sizes_.push_back(total_qty_);
        return;
    }

    Quantity allocated = 0;
    slice_sizes_.reserve(volume_profile.size());

    for (size_t i = 0; i < volume_profile.size(); ++i) {
        double frac = volume_profile[i] / total_weight;
        auto   qty  = static_cast<Quantity>(frac * total_qty_);
        slice_sizes_.push_back(qty);
        allocated += qty;
    }

    // Assign any remainder due to rounding to the first (largest) bucket.
    if (allocated < total_qty_ && !slice_sizes_.empty()) {
        slice_sizes_[0] += (total_qty_ - allocated);
    }
}

ChildOrder VwapAlgo::next_slice() {
    if (is_complete() || current_idx_ >= slice_sizes_.size()) {
        return ChildOrder{symbol_, side_, 0, order_type_, limit_price_};
    }

    Quantity qty = slice_sizes_[current_idx_];
    // Don't overshoot remaining
    qty = std::min(qty, remaining_qty_);

    ++current_idx_;
    ++slices_sent_;

    return ChildOrder{symbol_, side_, qty, order_type_, limit_price_};
}

void VwapAlgo::on_fill(Quantity qty) {
    Quantity actual = std::min(qty, remaining_qty_);
    filled_qty_    += actual;
    remaining_qty_ -= actual;
    ++slices_filled_;
}

bool VwapAlgo::is_complete() const {
    return remaining_qty_ == 0;
}

// ===========================================================================
// IcebergAlgo
// ===========================================================================

IcebergAlgo::IcebergAlgo(const Decision& parent, Quantity visible_qty)
    : visible_qty_(std::max(visible_qty, Quantity{1}))
{
    symbol_      = parent.symbol;
    side_        = parent.side;
    order_type_  = parent.order_type;
    limit_price_ = parent.limit_price;
    total_qty_   = parent.quantity;
    remaining_qty_ = parent.quantity;
}

ChildOrder IcebergAlgo::next_slice() {
    if (is_complete()) {
        return ChildOrder{symbol_, side_, 0, order_type_, limit_price_};
    }

    if (!needs_replenish_) {
        // Current tranche still active — caller should wait for fill.
        return ChildOrder{symbol_, side_, 0, order_type_, limit_price_};
    }

    Quantity qty = std::min(visible_qty_, remaining_qty_);
    current_tranche_remaining_ = qty;
    needs_replenish_ = false;
    ++slices_sent_;

    return ChildOrder{symbol_, side_, qty, order_type_, limit_price_};
}

void IcebergAlgo::on_fill(Quantity qty) {
    Quantity actual = std::min(qty, remaining_qty_);
    filled_qty_    += actual;
    remaining_qty_ -= actual;
    ++slices_filled_;

    if (qty >= current_tranche_remaining_) {
        current_tranche_remaining_ = 0;
        needs_replenish_ = true;
    } else {
        current_tranche_remaining_ -= qty;
    }
}

bool IcebergAlgo::is_complete() const {
    return remaining_qty_ == 0;
}

}  // namespace qf::strategy
