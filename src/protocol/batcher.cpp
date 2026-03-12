#include "qf/protocol/batcher.hpp"

namespace qf::protocol {

Batcher::Batcher(size_t max_datagram_size)
    : max_size_(max_datagram_size), write_pos_(BATCH_HEADER_SIZE), msg_count_(0), seq_num_(0) {
    std::memset(buffer_, 0, sizeof(buffer_));
}

bool Batcher::add(const uint8_t* msg_data, size_t msg_len) {
    if (!msg_data || msg_len == 0) return false;

    // Check if message would exceed the datagram capacity
    if (write_pos_ + msg_len > max_size_) return false;

    // Also reject if a single message can never fit in an empty datagram
    if (BATCH_HEADER_SIZE + msg_len > max_size_) return false;

    std::memcpy(buffer_ + write_pos_, msg_data, msg_len);
    write_pos_ += msg_len;
    ++msg_count_;
    return true;
}

FlushResult Batcher::flush() {
    if (msg_count_ == 0) return {nullptr, 0};

    write_header();

    FlushResult result{buffer_, write_pos_};

    // Reset for next batch
    write_pos_ = BATCH_HEADER_SIZE;
    msg_count_ = 0;

    return result;
}

void Batcher::flush(const BatchCallback& callback) {
    if (msg_count_ == 0) return;

    write_header();
    callback(buffer_, write_pos_);

    // Reset for next batch
    write_pos_ = BATCH_HEADER_SIZE;
    msg_count_ = 0;
}

bool Batcher::add_and_flush(const uint8_t* msg_data, size_t msg_len, const BatchCallback& callback) {
    if (!msg_data || msg_len == 0) return false;

    // Message can never fit in any datagram
    if (BATCH_HEADER_SIZE + msg_len > max_size_) return false;

    // If adding this message would overflow, flush current batch first
    if (write_pos_ + msg_len > max_size_) {
        flush(callback);
    }

    std::memcpy(buffer_ + write_pos_, msg_data, msg_len);
    write_pos_ += msg_len;
    ++msg_count_;
    return true;
}

void Batcher::write_header() {
    BatchHeader header{};
    header.seq_num = seq_num_++;
    header.msg_count = msg_count_;
    header.total_len = static_cast<uint16_t>(write_pos_);
    std::memcpy(buffer_, &header, sizeof(BatchHeader));
}

}  // namespace qf::protocol
