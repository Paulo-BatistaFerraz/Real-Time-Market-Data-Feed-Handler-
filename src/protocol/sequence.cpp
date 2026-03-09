#include "mdfh/protocol/sequence.hpp"

namespace mdfh::protocol {

// TODO: Implement sequence tracking logic

SequenceStatus SequenceTracker::check(uint64_t seq_num) {
    (void)seq_num;
    return SequenceStatus::Ok;
}

void SequenceTracker::reset() {
    expected_ = 0;
    initialized_ = false;
    gaps_.clear();
    total_gap_count_ = 0;
    duplicate_count_ = 0;
}

}  // namespace mdfh::protocol
