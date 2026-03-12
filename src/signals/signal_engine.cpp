#include "qf/signals/signal_engine.hpp"

namespace qf::signals {

SignalEngine::SignalEngine() = default;

std::vector<Signal> SignalEngine::process(const core::BookUpdate& update, const core::OrderBook& book) {
    // Get or create trade history for this symbol
    uint64_t hash = symbol_hash(update.symbol);
    auto& trades = trade_histories_[hash];

    // Step 1: Compute all active features
    FeatureVector fv = compute_features(book, trades, update.book_timestamp);

    // Step 2: Apply active indicators to feature values
    apply_indicators(fv);

    // Cache for diagnostics
    last_features_ = fv;

    // Step 3: Evaluate active composites → Signals
    return evaluate_composites(fv, update.symbol);
}

TradeHistory& SignalEngine::trade_history(const Symbol& symbol) {
    return trade_histories_[symbol_hash(symbol)];
}

const TradeHistory& SignalEngine::trade_history(const Symbol& symbol) const {
    static const TradeHistory empty;
    uint64_t hash = symbol_hash(symbol);
    auto it = trade_histories_.find(hash);
    return (it != trade_histories_.end()) ? it->second : empty;
}

void SignalEngine::reset() {
    trade_histories_.clear();
    last_features_ = FeatureVector{};

    // Reset all indicators via the registry
    auto keys = registry_.active_indicator_keys();
    for (const auto& key : keys) {
        auto* ind = registry_.get_indicator(key);
        if (ind) ind->reset();
    }
}

FeatureVector SignalEngine::compute_features(const core::OrderBook& book,
                                              TradeHistory& trades,
                                              uint64_t timestamp) {
    FeatureVector fv;
    fv.timestamp = timestamp;

    auto names = registry_.active_feature_names();
    for (const auto& name : names) {
        auto* feature = registry_.get_feature(name);
        if (feature) {
            double val = feature->compute(book, trades);
            fv.set(name, val);
        }
    }

    return fv;
}

void SignalEngine::apply_indicators(FeatureVector& fv) {
    auto keys = registry_.active_indicator_keys();
    for (const auto& key : keys) {
        auto* indicator = registry_.get_indicator(key);
        if (!indicator) continue;

        // Key format: "feature_name:indicator_name"
        // Feed the feature value into the indicator and store result
        auto colon = key.find(':');
        if (colon == std::string::npos) continue;

        std::string feature_name = key.substr(0, colon);
        if (!fv.has(feature_name)) continue;

        double input = fv.get(feature_name);
        double output = indicator->update(input);

        // Store indicator output as a new feature: "feature_name:indicator_name"
        fv.set(key, output);
    }
}

std::vector<Signal> SignalEngine::evaluate_composites(const FeatureVector& fv,
                                                       const Symbol& symbol) {
    std::vector<Signal> signals;

    auto names = registry_.active_composite_names();
    signals.reserve(names.size());

    for (const auto& name : names) {
        auto* combiner = registry_.get_composite(name);
        if (!combiner) continue;

        double strength = combiner->combine(fv);

        Signal sig;
        sig.symbol = symbol;
        sig.type = SignalType::Composite;
        sig.strength = strength;
        sig.direction = (strength > 0.0) ? SignalDirection::Buy
                      : (strength < 0.0) ? SignalDirection::Sell
                      : SignalDirection::Neutral;
        sig.timestamp = fv.timestamp;

        signals.push_back(sig);
    }

    return signals;
}

uint64_t SignalEngine::symbol_hash(const Symbol& symbol) {
    return symbol.as_key();
}

}  // namespace qf::signals
