#include "mdfh/protocol/batcher.hpp"

namespace mdfh::protocol {

// TODO: Implement batching logic

Batcher::Batcher(size_t max_datagram_size)
    : max_size_(max_datagram_size), write_pos_(BATCH_HEADER_SIZE), msg_count_(0), seq_num_(0) {
    std::memset(buffer_, 0, sizeof(buffer_));
}

bool Batcher::add(const uint8_t* msg_data, size_t msg_len) {
    (void)msg_data; (void)msg_len;
    return false;
}

void Batcher::flush(const BatchCallback& callback) {
    (void)callback;
}

bool Batcher::add_and_flush(const uint8_t* msg_data, size_t msg_len, const BatchCallback& callback) {
    (void)msg_data; (void)msg_len; (void)callback;
    return false;
}

}  // namespace mdfh::protocol
