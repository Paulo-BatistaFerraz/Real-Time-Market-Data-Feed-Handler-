#include "qf/protocol/sequence.hpp"

namespace qf::protocol {

SequenceStatus SequenceTracker::check(uint64_t seq_num) {
    // First message: initialize expected to this sequence number
    if (!initialized_) {
        initialized_ = true;
        expected_ = seq_num + 1;
        return SequenceStatus::Ok;
    }

    if (seq_num == expected_) {
        // In order
        ++expected_;
        return SequenceStatus::Ok;
    }

    if (seq_num < expected_) {
        // Already seen this or an older one
        ++duplicate_count_;
        return SequenceStatus::Duplicate;
    }

    // seq_num > expected_ → gap detected
    uint64_t gap_size = seq_num - expected_;
    total_gap_count_ += gap_size;
    gaps_.push_back({expected_, seq_num - 1});

    if (gap_callback_) {
        gap_callback_(expected_, seq_num);
    }

    expected_ = seq_num + 1;
    return SequenceStatus::Gap;
}

void SequenceTracker::reset() {
    expected_ = 0;
    initialized_ = false;
    gaps_.clear();
    total_gap_count_ = 0;
    duplicate_count_ = 0;
}

}  // namespace qf::protocol
