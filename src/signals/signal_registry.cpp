#include "qf/signals/signal_registry.hpp"

namespace qf::signals {

// --- Features ---

void SignalRegistry::register_feature(const std::string& name, std::unique_ptr<FeatureBase> feature) {
    std::lock_guard<std::mutex> lock(mutex_);
    features_[name] = std::move(feature);
    feature_active_[name] = true;  // active by default
}

FeatureBase* SignalRegistry::get_feature(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = features_.find(name);
    return (it != features_.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> SignalRegistry::feature_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(features_.size());
    for (const auto& [name, _] : features_) {
        names.push_back(name);
    }
    return names;
}

size_t SignalRegistry::feature_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return features_.size();
}

// --- Indicators ---

void SignalRegistry::register_indicator(const std::string& key, std::unique_ptr<IndicatorBase> indicator) {
    std::lock_guard<std::mutex> lock(mutex_);
    indicators_[key] = std::move(indicator);
    indicator_active_[key] = true;
}

IndicatorBase* SignalRegistry::get_indicator(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = indicators_.find(key);
    return (it != indicators_.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> SignalRegistry::indicator_keys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> keys;
    keys.reserve(indicators_.size());
    for (const auto& [key, _] : indicators_) {
        keys.push_back(key);
    }
    return keys;
}

size_t SignalRegistry::indicator_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return indicators_.size();
}

// --- Composites ---

void SignalRegistry::register_composite(const std::string& name, std::unique_ptr<AlphaCombiner> combiner) {
    std::lock_guard<std::mutex> lock(mutex_);
    composites_[name] = std::move(combiner);
    composite_active_[name] = true;
}

AlphaCombiner* SignalRegistry::get_composite(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = composites_.find(name);
    return (it != composites_.end()) ? it->second.get() : nullptr;
}

std::vector<std::string> SignalRegistry::composite_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    names.reserve(composites_.size());
    for (const auto& [name, _] : composites_) {
        names.push_back(name);
    }
    return names;
}

size_t SignalRegistry::composite_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return composites_.size();
}

// --- Activation ---

void SignalRegistry::set_feature_active(const std::string& name, bool active) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (features_.find(name) != features_.end()) {
        feature_active_[name] = active;
    }
}

bool SignalRegistry::is_feature_active(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = feature_active_.find(name);
    return (it != feature_active_.end()) ? it->second : false;
}

std::vector<std::string> SignalRegistry::active_feature_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& [name, active] : feature_active_) {
        if (active) names.push_back(name);
    }
    return names;
}

void SignalRegistry::set_indicator_active(const std::string& key, bool active) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (indicators_.find(key) != indicators_.end()) {
        indicator_active_[key] = active;
    }
}

bool SignalRegistry::is_indicator_active(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = indicator_active_.find(key);
    return (it != indicator_active_.end()) ? it->second : false;
}

std::vector<std::string> SignalRegistry::active_indicator_keys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> keys;
    for (const auto& [key, active] : indicator_active_) {
        if (active) keys.push_back(key);
    }
    return keys;
}

void SignalRegistry::set_composite_active(const std::string& name, bool active) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (composites_.find(name) != composites_.end()) {
        composite_active_[name] = active;
    }
}

bool SignalRegistry::is_composite_active(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = composite_active_.find(name);
    return (it != composite_active_.end()) ? it->second : false;
}

std::vector<std::string> SignalRegistry::active_composite_names() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& [name, active] : composite_active_) {
        if (active) names.push_back(name);
    }
    return names;
}

void SignalRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    features_.clear();
    indicators_.clear();
    composites_.clear();
    feature_active_.clear();
    indicator_active_.clear();
    composite_active_.clear();
}

}  // namespace qf::signals
