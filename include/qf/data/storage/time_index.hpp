#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "qf/common/types.hpp"

namespace qf::data {

// An entry in the time index: maps a timestamp to its byte offset in a tick file.
#pragma pack(push, 1)
struct IndexEntry {
    Timestamp timestamp;   // nanoseconds since midnight
    uint64_t  offset;      // byte offset of the TickRecord in the file

    bool operator<(const IndexEntry& other) const {
        return timestamp < other.timestamp;
    }
};
#pragma pack(pop)

// TimeIndex — binary-searchable index of timestamp → file offset.
// Stored as a sorted array of (timestamp, offset) pairs.
// Supports O(log n) lookups via binary search.
class TimeIndex {
public:
    TimeIndex() = default;

    // Build the index by scanning a .qfbt tick file from scratch.
    // Returns the number of entries indexed.
    uint64_t build(const std::string& tick_file_path);

    // Find the file offset of the first record with timestamp >= target.
    // Returns the byte offset, or 0 if no matching record exists.
    uint64_t find(Timestamp target) const;

    // Find the file offset of the first record with timestamp > target.
    uint64_t upper_bound(Timestamp target) const;

    // Save the index to a binary file for later reuse.
    bool save(const std::string& index_path) const;

    // Load a previously saved index from a binary file.
    bool load(const std::string& index_path);

    // Number of entries in the index.
    size_t size() const { return entries_.size(); }

    // Whether the index is empty.
    bool empty() const { return entries_.empty(); }

    // Direct access to entries (for iteration / testing).
    const std::vector<IndexEntry>& entries() const { return entries_; }

    // Append a new entry. Caller must ensure timestamps are non-decreasing.
    void add_entry(Timestamp ts, uint64_t offset) {
        entries_.push_back({ts, offset});
    }

    // Clear all entries.
    void clear() { entries_.clear(); }

private:
    std::vector<IndexEntry> entries_;
};

}  // namespace qf::data
