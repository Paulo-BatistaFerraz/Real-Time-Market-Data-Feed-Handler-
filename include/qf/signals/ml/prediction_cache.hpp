#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include "qf/common/types.hpp"

namespace qf::signals {

// PredictionCache — LRU cache for ML predictions, keyed by (symbol, timestamp_bucket).
//
// Avoids recomputing predictions when the same symbol is queried within the
// same time bucket. When the cache exceeds max_size, the least-recently-used
// entry is evicted.
class PredictionCache {
public:
    explicit PredictionCache(size_t max_size, uint64_t bucket_width_ns = 1'000'000'000ULL);

    // Insert or update a prediction. Evicts LRU if at capacity.
    void put(const Symbol& symbol, uint64_t timestamp_ns, double prediction);

    // Look up a cached prediction. Returns std::nullopt on cache miss.
    std::optional<double> get(const Symbol& symbol, uint64_t timestamp_ns);

    // Number of cached entries.
    size_t size() const { return map_.size(); }

    // Maximum cache capacity.
    size_t max_size() const { return max_size_; }

    // Bucket width in nanoseconds.
    uint64_t bucket_width_ns() const { return bucket_width_ns_; }

    // Cache hit count (since construction or last clear).
    uint64_t hits() const { return hits_; }

    // Cache miss count.
    uint64_t misses() const { return misses_; }

    // Clear all entries and reset stats.
    void clear();

private:
    struct CacheKey {
        Symbol   symbol;
        uint64_t bucket;

        bool operator==(const CacheKey& o) const {
            return symbol == o.symbol && bucket == o.bucket;
        }
    };

    struct CacheKeyHash {
        size_t operator()(const CacheKey& k) const {
            size_t h1 = std::hash<uint64_t>{}(k.symbol.as_key());
            size_t h2 = std::hash<uint64_t>{}(k.bucket);
            return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + 0x517cc1b727220a95ULL);
        }
    };

    using LruList    = std::list<std::pair<CacheKey, double>>;
    using LruIterMap = std::unordered_map<CacheKey, typename LruList::iterator, CacheKeyHash>;

    CacheKey make_key(const Symbol& symbol, uint64_t timestamp_ns) const;

    size_t   max_size_;
    uint64_t bucket_width_ns_;
    uint64_t hits_   = 0;
    uint64_t misses_ = 0;

    LruList    lru_;   // front = most recent, back = least recent
    LruIterMap map_;
};

}  // namespace qf::signals
