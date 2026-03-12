#include "qf/protocol/validator.hpp"
#include <cstring>

namespace qf::protocol {

namespace {

// Return the expected WIRE_SIZE for a given message_type char, or 0 if unknown.
uint16_t expected_wire_size(char type) {
    switch (static_cast<MessageType>(type)) {
        case MessageType::AddOrder:      return AddOrder::WIRE_SIZE;
        case MessageType::CancelOrder:   return CancelOrder::WIRE_SIZE;
        case MessageType::ExecuteOrder:  return ExecuteOrder::WIRE_SIZE;
        case MessageType::ReplaceOrder:  return ReplaceOrder::WIRE_SIZE;
        case MessageType::TradeMessage:  return TradeMessage::WIRE_SIZE;
        default:                         return 0;
    }
}

bool is_known_type(char type) {
    return expected_wire_size(type) != 0;
}

}  // anonymous namespace

ValidationResult Validator::validate(const uint8_t* buffer, size_t len) {
    // Need at least a full header to inspect
    if (len < sizeof(MessageHeader)) {
        return {ErrorCode::BufferTooSmall, "buffer shorter than message header"};
    }

    MessageHeader header;
    std::memcpy(&header, buffer, sizeof(header));

    // Check message_type is known
    if (!is_known_type(header.message_type)) {
        return {ErrorCode::UnknownMessageType, "unknown message type"};
    }

    // Check message_length matches expected for this type
    uint16_t expected = expected_wire_size(header.message_type);
    if (header.message_length != expected) {
        return {ErrorCode::InvalidLength, "message_length does not match expected size for type"};
    }

    // Check that the buffer actually contains enough bytes for the full message
    if (len < header.message_length) {
        return {ErrorCode::BufferTooSmall, "payload truncated"};
    }

    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_length(const MessageHeader& header) {
    if (!is_known_type(header.message_type)) {
        return {ErrorCode::UnknownMessageType, "unknown message type"};
    }

    uint16_t expected = expected_wire_size(header.message_type);
    if (header.message_length != expected) {
        return {ErrorCode::InvalidLength, "message_length does not match expected size for type"};
    }

    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_add_order(const AddOrder& msg) {
    if (msg.price == 0) {
        return {ErrorCode::InvalidPrice, "price must be > 0"};
    }
    if (msg.quantity == 0) {
        return {ErrorCode::InvalidQuantity, "quantity must be > 0"};
    }
    if (msg.side != static_cast<Side>(0x01) && msg.side != static_cast<Side>(0x02)) {
        return {ErrorCode::UnknownMessageType, "invalid side value"};
    }
    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_cancel_order(const CancelOrder& msg) {
    if (msg.order_id == 0) {
        return {ErrorCode::InvalidLength, "order_id must be > 0"};
    }
    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_execute_order(const ExecuteOrder& msg) {
    if (msg.order_id == 0) {
        return {ErrorCode::InvalidLength, "order_id must be > 0"};
    }
    if (msg.exec_quantity == 0) {
        return {ErrorCode::InvalidQuantity, "exec_quantity must be > 0"};
    }
    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_replace_order(const ReplaceOrder& msg) {
    if (msg.new_price == 0) {
        return {ErrorCode::InvalidPrice, "new_price must be > 0"};
    }
    if (msg.new_quantity == 0) {
        return {ErrorCode::InvalidQuantity, "new_quantity must be > 0"};
    }
    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_trade_message(const TradeMessage& msg) {
    if (msg.price == 0) {
        return {ErrorCode::InvalidPrice, "price must be > 0"};
    }
    if (msg.quantity == 0) {
        return {ErrorCode::InvalidQuantity, "quantity must be > 0"};
    }
    return {ErrorCode::Ok, nullptr};
}

bool Validator::check_timestamp(uint64_t timestamp) {
    if (timestamp < last_timestamp_) {
        return false;
    }
    last_timestamp_ = timestamp;
    return true;
}

}  // namespace qf::protocol
