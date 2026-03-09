#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <functional>
#include "mdfh/common/constants.hpp"

namespace mdfh::protocol {

// Batch header prepended to each UDP datagram
#pragma pack(push, 1)
struct BatchHeader {
    uint64_t seq_num;      // monotonic sequence number
    uint16_t msg_count;    // number of messages in this batch
    uint16_t total_len;    // total bytes including this header
};
#pragma pack(pop)

static_assert(sizeof(BatchHeader) == BATCH_HEADER_SIZE, "BatchHeader size mismatch");

// Callback invoked when a batch is ready to send
using BatchCallback = std::function<void(const uint8_t* data, size_t len)>;

class Batcher {
public:
    explicit Batcher(size_t max_datagram_size = MAX_DATAGRAM);

    // Add an encoded message to the current batch
    // Returns false if message is too large to fit even in an empty batch
    bool add(const uint8_t* msg_data, size_t msg_len);

    // Flush current batch (sends even if not full)
    void flush(const BatchCallback& callback);

    // Add message and auto-flush if batch is full
    bool add_and_flush(const uint8_t* msg_data, size_t msg_len, const BatchCallback& callback);

    uint64_t batches_sent() const { return seq_num_; }
    uint16_t current_msg_count() const { return msg_count_; }

private:
    uint8_t buffer_[MAX_DATAGRAM];
    size_t max_size_;
    size_t write_pos_;
    uint16_t msg_count_;
    uint64_t seq_num_;
};

}  // namespace mdfh::protocol
