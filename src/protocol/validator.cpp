#include "mdfh/protocol/validator.hpp"
#include <cstring>

namespace mdfh::protocol {

// TODO: Implement validation logic

ValidationResult Validator::validate(const uint8_t* buffer, size_t len) {
    (void)buffer; (void)len;
    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_length(const MessageHeader& header) {
    (void)header;
    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_add_order(const AddOrder& msg) {
    (void)msg;
    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_cancel_order(const CancelOrder& msg) {
    (void)msg;
    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_execute_order(const ExecuteOrder& msg) {
    (void)msg;
    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_replace_order(const ReplaceOrder& msg) {
    (void)msg;
    return {ErrorCode::Ok, nullptr};
}

ValidationResult Validator::validate_trade_message(const TradeMessage& msg) {
    (void)msg;
    return {ErrorCode::Ok, nullptr};
}

bool Validator::check_timestamp(uint64_t timestamp) {
    (void)timestamp;
    return true;
}

}  // namespace mdfh::protocol
