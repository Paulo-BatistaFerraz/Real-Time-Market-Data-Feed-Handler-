#pragma once

#include <memory>
#include <vector>
#include "qf/core/order_book.hpp"
#include "qf/core/pipeline_types.hpp"
#include "qf/signals/signal_types.hpp"
#include "qf/signals/signal_registry.hpp"

namespace qf::signals {

// SignalEngine — orchestrates the full signal pipeline:
//   BookUpdate → features → indicators → composites → Signals
//
// The engine owns a SignalRegistry that holds all registered features,
// indicators, and composites. On each BookUpdate it:
//   1. Looks up (or creates) the OrderBook for the symbol
//   2. Computes all active features → FeatureVector
//   3. Feeds feature values through active indicators → updated FeatureVector
//   4. Evaluates all active composites → one Signal per composite
//
// Thread-safe: process() can be called from the pipeline's book stage thread.
class SignalEngine {
public:
    SignalEngine();

    // Access the registry for registration and configuration.
    SignalRegistry& registry() { return registry_; }
    const SignalRegistry& registry() const { return registry_; }

    // Process a BookUpdate through the full pipeline.
    // Returns one Signal per active composite.
    std::vector<Signal> process(const core::BookUpdate& update, const core::OrderBook& book);

    // Get the trade history for a symbol (for external use / testing).
    TradeHistory& trade_history(const Symbol& symbol);
    const TradeHistory& trade_history(const Symbol& symbol) const;

    // Get the most recently computed feature vector.
    const FeatureVector& last_features() const { return last_features_; }

    // Reset all internal state (trade histories, indicator state).
    void reset();

private:
    SignalRegistry registry_;

    // Per-symbol trade history
    std::unordered_map<uint64_t, TradeHistory> trade_histories_;

    // Most recent feature vector (for diagnostics)
    FeatureVector last_features_;

    // Pipeline steps
    FeatureVector compute_features(const core::OrderBook& book, TradeHistory& trades, uint64_t timestamp);
    void apply_indicators(FeatureVector& fv);
    std::vector<Signal> evaluate_composites(const FeatureVector& fv, const Symbol& symbol);

    // Hash a symbol for the trade history map
    static uint64_t symbol_hash(const Symbol& symbol);
};

}  // namespace qf::signals
