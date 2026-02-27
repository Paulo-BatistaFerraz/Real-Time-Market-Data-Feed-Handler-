// offset 0 size 2 -> field message_length (uint16_t)
#pragma once // pragma once is used to prevent multiple 
#include <cstdint> // uint16_t, uint8_t, uint64_t 
#include "mdfh/common/types.hpp"


/*

'A' -> AddOrder
'X' -> CancelOrder
'E' -> ExecuteOrder
'R' -> ReplaceOrder
'T' -> TradeMessage

*/


namespace mdfh{  // 
    #pragma pack(push, 1) // ??push the current packing alignment, (this means that the struct will be packed with no padding -> all fields are packed sequentially->)
    struct MessageHeader{ // header of the message ( defines the structure of the message)
        uint16_t message_length; // total bytes including this header (2 bytes) 
        char message_type; // message type as a raw byte (1 byte)
        uint64_t timestamp; // nanoseconds since midnight
    };

    enum class MessageType: char{ // classe chamada MessageType que é um char, enum e usado
        AddOrder = 'A', // represents different char for the message type
        CancelOrder = 'X',
        ExecuteOrder = 'E',
        ReplaceOrder = 'R',
        TradeMessage = 'T'
    };

    struct AddOrder{
        // payload of the message order_id(8 bytes), side(1 byte), symbol(8 bytes), price(4 bytes), quantity(4 bytes)
        static constexpr char TYPE = 'A'; // static -> constante, constexpr -> constante em tempo de compilação, char e o tipo da mensagem (constexpr -> runs at compile time) , if not constexpr, the compiler will not know the size of the message at compile time
        static constexpr uint16_t WIRE_SIZE = sizeof(MessageHeader) + sizeof(AddOrder);// tamanho da mensage da AddOrder (header + payload) -> 11 bytes + 25 bytes = 36 bytes
        
        OrderId order_id;
        Side side;
        Symbol symbol;
        Price price;
        Quantity quantity;

    };

    struct CancelOrder{
        static constexpr char TYPE = 'X';
        static constexpr uint16_t WIRE_SIZE = sizeof(MessageHeader) + sizeof(CancelOrder);
        OrderId order_id;
    };
    struct ExecuteOrder{
        static constexpr char TYPE = 'E';
        static constexpr uint16_t WIRE_SIZE = sizeof(MessageHeader) + sizeof(ExecuteOrder);
        OrderId order_id;
        Quantity exec_quantity;
    };
    struct ReplaceOrder{
        static constexpr char TYPE = 'R';
        static constexpr uint16_t WIRE_SIZE = sizeof(MessageHeader) + sizeof(ReplaceOrder);
        OrderId order_id;
        Price new_price;
        Quantity new_quantity;
    };
    struct TradeMessage{
        static constexpr char TYPE = 'T';
        static constexpr uint16_t WIRE_SIZE = sizeof(MessageHeader) + sizeof(TradeMessage);
        Symbol symbol;
        Price price;
        Quantity quantity;
        OrderId buy_order_id;
        OrderId sell_order_id;
    };
    #pragma pack(pop)


}

