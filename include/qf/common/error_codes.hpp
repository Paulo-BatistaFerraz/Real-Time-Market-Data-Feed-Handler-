#pragma once

#include <cstdint>

namespace qf {

enum class ErrorCode : uint8_t {
    Ok = 0,
    BufferTooSmall,
    UnknownMessageType,
    InvalidLength,
    InvalidPrice,
    InvalidQuantity,
    TimestampOutOfOrder,
    GapDetected,
    DuplicateSequence,
    QueueFull,
    OrderNotFound,
    BookNotFound,
    ConfigLoadFailed,
    NetworkError,
    ShutdownRequested
};

inline const char* error_to_string(ErrorCode ec) {
    switch (ec) {
        case ErrorCode::Ok:                  return "Ok";
        case ErrorCode::BufferTooSmall:      return "BufferTooSmall";
        case ErrorCode::UnknownMessageType:  return "UnknownMessageType";
        case ErrorCode::InvalidLength:       return "InvalidLength";
        case ErrorCode::InvalidPrice:        return "InvalidPrice";
        case ErrorCode::InvalidQuantity:     return "InvalidQuantity";
        case ErrorCode::TimestampOutOfOrder: return "TimestampOutOfOrder";
        case ErrorCode::GapDetected:         return "GapDetected";
        case ErrorCode::DuplicateSequence:   return "DuplicateSequence";
        case ErrorCode::QueueFull:           return "QueueFull";
        case ErrorCode::OrderNotFound:       return "OrderNotFound";
        case ErrorCode::BookNotFound:        return "BookNotFound";
        case ErrorCode::ConfigLoadFailed:    return "ConfigLoadFailed";
        case ErrorCode::NetworkError:        return "NetworkError";
        case ErrorCode::ShutdownRequested:   return "ShutdownRequested";
        default:                             return "Unknown";
    }
}

}  // namespace qf
