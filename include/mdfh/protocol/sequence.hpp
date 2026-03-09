#pragma once

#include <cstdint>
#include <vector>

namespace mdfh::protocol {

enum class SequenceStatus {
    Ok,
    Gap,
    Duplicate
};

struct GapRange {
    uint64_t from;  // first missing seq_num
    uint64_t to;    // last missing seq_num (inclusive)
};

class SequenceTracker {
public:
    SequenceTracker() = default;

    // Check incoming sequence number against expected
    SequenceStatus check(uint64_t seq_num);

    // Get all detected gaps
    const std::vector<GapRange>& gaps() const { return gaps_; }

    // Total gap count (number of missing packets)
    uint64_t total_gaps() const { return total_gap_count_; }

    // Total duplicates received
    uint64_t total_duplicates() const { return duplicate_count_; }

    // Last successfully received sequence number
    uint64_t last_seq() const { return expected_ > 0 ? expected_ - 1 : 0; }

    // Reset state
    void reset();

private:
    uint64_t expected_ = 0;
    bool initialized_ = false;
    std::vector<GapRange> gaps_;
    uint64_t total_gap_count_ = 0;
    uint64_t duplicate_count_ = 0;
};

}  // namespace mdfh::protocol
