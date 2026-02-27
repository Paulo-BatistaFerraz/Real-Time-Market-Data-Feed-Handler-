// offset 0 size 2 -> field message_length (uint16_t)
#pragma once // pragma once is used to prevent multiple 
#include <cstdint> // uint16_t, uint8_t, uint64_t 
#include "mdfh/common/types.hpp"

namespace mdfh{ // 
    #pragma pack(push, 1) // ??push the current packing alignment, (this means that the struct will be packed with no padding -> all fields are packed sequentially->)
    struct MessageHeader{ // header of the message ( defines the structure of the message)
        uint16_t message_length; // total bytes including this header (2 bytes) 
        char message_type; // message type as a raw byte (1 byte)
        uint64_t timestamp; // nanoseconds since midnight
    };

    struct AddOrder{
        // payload of the message order_id(8 bytes), side(1 byte), symbol(8 bytes), price(4 bytes), quantity(4 bytes)
        OrderId order_id;
        Side side;
        Symbol symbol;
        Price price;
        Quantity quantity;
    };
    struct CancelOrder{
        OrderId order_id;
    };
    struct ExecuteOrder{
        OrderId order_id;
        Quantity exec_quantity;
    };
    struct ReplaceOrder{
        OrderId order_id;
        Price new_price;
        Quantity new_quantity;
    };
    struct TradeMessage{
        Symbol symbol;
        Price price;
        Quantity quantity;
        OrderId buy_order_id;
        OrderId sell_order_id;
    };
    #pragma pack(pop)


}

