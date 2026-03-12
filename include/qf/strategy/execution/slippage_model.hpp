#pragma once

#include <unordered_map>
#include "qf/common/config.hpp"
#include "qf/common/types.hpp"

namespace qf::strategy {

// SlippageModel — estimates market impact using a linear impact model.
//
// Model:  slippage = alpha * (quantity / ADV)
//
//   alpha    — impact coefficient (default 0.1)
//   ADV      — average daily volume for the symbol
//   quantity — order size in lots
//
// The estimated fill price is shifted from the reference price by the
// slippage amount: worse for the aggressor (higher for buys, lower for
// sells).
//
// Thread-safety: const methods are safe for concurrent reads; mutators
// are not thread-safe and must be synchronised externally.
class SlippageModel {
public:
    explicit SlippageModel(double alpha = 0.1);

    // --- Setters -----------------------------------------------------------

    // Set the impact coefficient.
    void set_alpha(double alpha);

    // Set average daily volume for a specific symbol.
    void set_adv(const Symbol& symbol, double adv);

    // --- Core API ----------------------------------------------------------

    // Estimate slippage as a fraction of price.
    // Returns alpha * (quantity / ADV).
    double estimate_slippage(const Symbol& symbol, Quantity quantity) const;

    // Estimate the fill price after market impact.
    //   side            — Buy or Sell
    //   reference_price — mid / last price before the order
    //   quantity        — order size
    // Buy orders get a worse (higher) price; sell orders get a worse
    // (lower) price.
    Price estimated_fill_price(const Symbol& symbol,
                               Side side,
                               Price reference_price,
                               Quantity quantity) const;

    // --- Accessors ---------------------------------------------------------

    double alpha() const { return alpha_; }
    double adv(const Symbol& symbol) const;

    // --- Configuration -----------------------------------------------------

    void configure(const Config& config);

private:
    double alpha_;
    std::unordered_map<uint64_t, double> adv_;   // keyed by Symbol::as_key()

    static constexpr double DEFAULT_ADV = 1'000'000.0;
};

}  // namespace qf::strategy
