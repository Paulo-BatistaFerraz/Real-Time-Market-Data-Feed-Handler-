#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "qf/signals/features/feature_base.hpp"
#include "qf/signals/indicators/indicator_base.hpp"
#include "qf/signals/composite/alpha_combiner.hpp"

namespace qf::signals {

// SignalRegistry — central catalog of features, indicators, and composites.
//
// Allows registering and looking up signal components by name. The engine
// uses the registry to discover which components are available and active.
// Thread-safe: all accessors/mutators are guarded by a mutex.
class SignalRegistry {
public:
    SignalRegistry() = default;

    // --- Features ---
    void register_feature(const std::string& name, std::unique_ptr<FeatureBase> feature);
    FeatureBase* get_feature(const std::string& name) const;
    std::vector<std::string> feature_names() const;
    size_t feature_count() const;

    // --- Indicators ---
    // Indicators are keyed by "feature_name:indicator_name" to allow
    // multiple indicators on the same feature (e.g. "vwap:ema_20").
    void register_indicator(const std::string& key, std::unique_ptr<IndicatorBase> indicator);
    IndicatorBase* get_indicator(const std::string& key) const;
    std::vector<std::string> indicator_keys() const;
    size_t indicator_count() const;

    // --- Composites ---
    // Named alpha combiners that produce signals from feature vectors.
    void register_composite(const std::string& name, std::unique_ptr<AlphaCombiner> combiner);
    AlphaCombiner* get_composite(const std::string& name) const;
    std::vector<std::string> composite_names() const;
    size_t composite_count() const;

    // --- Activation ---
    // Only active features/indicators are evaluated during processing.
    void set_feature_active(const std::string& name, bool active);
    bool is_feature_active(const std::string& name) const;
    std::vector<std::string> active_feature_names() const;

    void set_indicator_active(const std::string& key, bool active);
    bool is_indicator_active(const std::string& key) const;
    std::vector<std::string> active_indicator_keys() const;

    void set_composite_active(const std::string& name, bool active);
    bool is_composite_active(const std::string& name) const;
    std::vector<std::string> active_composite_names() const;

    // Reset all registrations and activation state.
    void clear();

private:
    mutable std::mutex mutex_;

    std::unordered_map<std::string, std::unique_ptr<FeatureBase>> features_;
    std::unordered_map<std::string, std::unique_ptr<IndicatorBase>> indicators_;
    std::unordered_map<std::string, std::unique_ptr<AlphaCombiner>> composites_;

    std::unordered_map<std::string, bool> feature_active_;
    std::unordered_map<std::string, bool> indicator_active_;
    std::unordered_map<std::string, bool> composite_active_;
};

}  // namespace qf::signals
