#include "qf/signals/ml/prediction_cache.hpp"

namespace qf::signals {

PredictionCache::PredictionCache(size_t max_size, uint64_t bucket_width_ns)
    : max_size_(max_size)
    , bucket_width_ns_(bucket_width_ns)
{}

PredictionCache::CacheKey PredictionCache::make_key(const Symbol& symbol, uint64_t timestamp_ns) const {
    return CacheKey{symbol, timestamp_ns / bucket_width_ns_};
}

void PredictionCache::put(const Symbol& symbol, uint64_t timestamp_ns, double prediction) {
    auto key = make_key(symbol, timestamp_ns);
    auto it = map_.find(key);

    if (it != map_.end()) {
        // Update existing entry and move to front
        it->second->second = prediction;
        lru_.splice(lru_.begin(), lru_, it->second);
        return;
    }

    // Evict LRU if at capacity
    if (map_.size() >= max_size_) {
        auto& back = lru_.back();
        map_.erase(back.first);
        lru_.pop_back();
    }

    // Insert new entry at front
    lru_.emplace_front(key, prediction);
    map_[key] = lru_.begin();
}

std::optional<double> PredictionCache::get(const Symbol& symbol, uint64_t timestamp_ns) {
    auto key = make_key(symbol, timestamp_ns);
    auto it = map_.find(key);

    if (it == map_.end()) {
        ++misses_;
        return std::nullopt;
    }

    ++hits_;
    // Move to front (most recently used)
    lru_.splice(lru_.begin(), lru_, it->second);
    return it->second->second;
}

void PredictionCache::clear() {
    lru_.clear();
    map_.clear();
    hits_ = 0;
    misses_ = 0;
}

}  // namespace qf::signals
