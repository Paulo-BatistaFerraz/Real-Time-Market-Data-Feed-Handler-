#pragma once

#include <cstdint>
#include <cstddef>
#include "qf/common/error_codes.hpp"
#include "qf/protocol/messages.hpp"

namespace qf::protocol {

struct ValidationResult {
    ErrorCode code;
    const char* detail;

    bool ok() const { return code == ErrorCode::Ok; }
    operator bool() const { return ok(); }
};

class Validator {
public:
    // Validate a raw message buffer before decoding
    static ValidationResult validate(const uint8_t* buffer, size_t len);

    // Validate message_length field matches expected size for message_type
    static ValidationResult validate_length(const MessageHeader& header);

    // Validate payload field ranges (price > 0, quantity > 0, etc.)
    static ValidationResult validate_add_order(const AddOrder& msg);
    static ValidationResult validate_cancel_order(const CancelOrder& msg);
    static ValidationResult validate_execute_order(const ExecuteOrder& msg);
    static ValidationResult validate_replace_order(const ReplaceOrder& msg);
    static ValidationResult validate_trade_message(const TradeMessage& msg);

    // Check timestamp monotonicity (call sequentially)
    bool check_timestamp(uint64_t timestamp);

private:
    uint64_t last_timestamp_ = 0;
};

}  // namespace qf::protocol
