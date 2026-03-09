#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "mdfh/protocol/batcher.hpp"
#include "mdfh/protocol/messages.hpp"
#include "mdfh/common/error_codes.hpp"

namespace mdfh::protocol {

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
    // Parse a received datagram into batch header + individual message views
    // Zero-copy: MessageView points into the original buffer
    static FrameParseResult parse(const uint8_t* datagram, size_t len);

    // Parse without batch header (raw message stream)
    static std::vector<MessageView> parse_raw(const uint8_t* data, size_t len);
};

}  // namespace mdfh::protocol
