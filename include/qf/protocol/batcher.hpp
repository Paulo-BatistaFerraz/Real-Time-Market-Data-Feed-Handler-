#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <functional>
#include "qf/common/constants.hpp"

namespace qf::protocol {

// Batch header prepended to each UDP datagram
#pragma pack(push, 1)
struct BatchHeader {
    uint64_t seq_num;      // monotonic sequence number
    uint16_t msg_count;    // number of messages in this batch
    uint16_t total_len;    // total bytes including this header
};
#pragma pack(pop)

static_assert(sizeof(BatchHeader) == BATCH_HEADER_SIZE, "BatchHeader size mismatch");

// Result of a flush operation — pointer into internal buffer (valid until next add/flush)
struct FlushResult {
    const uint8_t* data;
    size_t size;
};

// Callback invoked when a batch is ready to send
using BatchCallback = std::function<void(const uint8_t* data, size_t len)>;

class Batcher {
public:
    explicit Batcher(size_t max_datagram_size = MAX_DATAGRAM);

    // Add an encoded message to the current batch
    // Returns true if the message was added, false if the datagram is full
    // (message exceeds remaining space or is too large for any empty datagram)
    bool add(const uint8_t* msg_data, size_t msg_len);

    // Flush current batch — returns pointer and size of completed datagram, resets for next batch
    // Returned pointer is valid until the next call to add() or flush()
    // Returns {nullptr, 0} if batch is empty
    FlushResult flush();

    // Flush current batch via callback (sends even if not full)
    void flush(const BatchCallback& callback);

    // Add message and auto-flush if batch is full
    bool add_and_flush(const uint8_t* msg_data, size_t msg_len, const BatchCallback& callback);

    // Returns true if no messages have been added since the last flush
    bool is_empty() const { return msg_count_ == 0; }

    uint64_t batches_sent() const { return seq_num_; }
    uint16_t current_msg_count() const { return msg_count_; }

private:
    void write_header();

    uint8_t buffer_[MAX_DATAGRAM];
    size_t max_size_;
    size_t write_pos_;
    uint16_t msg_count_;
    uint64_t seq_num_;
};

}  // namespace qf::protocol
