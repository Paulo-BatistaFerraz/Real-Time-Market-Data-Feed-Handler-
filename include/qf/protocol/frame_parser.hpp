#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include "qf/protocol/batcher.hpp"
#include "qf/protocol/messages.hpp"
#include "qf/common/error_codes.hpp"

namespace qf::protocol {

// Zero-copy view into a message within a datagram
struct MessageView {
    const uint8_t* data;   // pointer into original buffer
    size_t len;            // message_length from header
};

struct FrameParseResult {
    BatchHeader batch_header;
    std::vector<MessageView> messages;
    ErrorCode error;

    bool ok() const { return error == ErrorCode::Ok; }
};

class FrameParser {
public:
    // Zero-allocation callback-based parser (US-005)
    // Walks buffer by message_length field, calls callback(header, payload_ptr) for each message
    // Stops if remaining bytes < sizeof(MessageHeader) or message_length exceeds remaining
    // Returns number of messages successfully parsed
    template <typename Callback>
    static size_t parse(const uint8_t* buffer, size_t length, Callback&& callback) {
        if (!buffer || length == 0) return 0;

        size_t offset = 0;
        size_t msg_count = 0;

        while (offset + sizeof(qf::MessageHeader) <= length) {
            qf::MessageHeader header;
            std::memcpy(&header, buffer + offset, sizeof(qf::MessageHeader));

            // message_length must be at least the header size
            if (header.message_length < sizeof(qf::MessageHeader)) break;

            // message_length must fit in remaining buffer
            if (header.message_length > length - offset) break;

            const uint8_t* payload = buffer + offset + sizeof(qf::MessageHeader);
            callback(header, payload);

            offset += header.message_length;
            ++msg_count;
        }

        return msg_count;
    }

    // Parse a received datagram (with BatchHeader) into batch header + individual message views
    static FrameParseResult parse(const uint8_t* datagram, size_t len);

    // Parse without batch header (raw message stream)
    static std::vector<MessageView> parse_raw(const uint8_t* data, size_t len);
};

}  // namespace qf::protocol
