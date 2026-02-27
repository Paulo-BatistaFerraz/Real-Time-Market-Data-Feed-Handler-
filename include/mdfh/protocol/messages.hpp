// offset 0 size 2 -> field message_length (uint16_t)
#pragma once // pragma once is used to prevent multiple 
#include <cstdint> // uint16_t, uint8_t, uint64_t 
#include <variant>
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
        static constexpr size_t WIRE_SIZE = 36;
        
        OrderId order_id; // 8 bytes (64 bits)
        Side side;
        Symbol symbol; // 8 bytes (64 bits)
        Price price; // 4 bytes (32 bits)
        Quantity quantity; // 4 bytes (32 bits)
        // total size = 8 + 1 + 8 + 4 + 4 + sizeof(MessageHeader) = 36 bytes

    };
    static_assert(sizeof(MessageHeader) + sizeof(AddOrder) == AddOrder::WIRE_SIZE, "AddOrder WIRE_SIZE mismatch"); // static_assert is a compile time assertion that checks if the condition is true, if not, the compiler will throw an error

    struct CancelOrder{
        static constexpr char TYPE = 'X';
        static constexpr size_t WIRE_SIZE = 19; // as sizeof(MessageHeader) is 11 bytes, the total size of the message is 19 bytes
        OrderId order_id; // 8 bytes (64 bits)
    };
    static_assert(sizeof(MessageHeader) + sizeof(CancelOrder) == CancelOrder::WIRE_SIZE, "CancelOrder WIRE_SIZE mismatch");
    struct ExecuteOrder{
        static constexpr char TYPE = 'E';
        static constexpr size_t WIRE_SIZE = 23; // as sizeof(MessageHeader) is 11 bytes, the total size of the message is 23 bytes
        OrderId order_id; // 8 bytes (64 bits)
        Quantity exec_quantity; // 4 bytes (32 bits)
    };
    struct ReplaceOrder{
        static constexpr char TYPE = 'R';
        static constexpr size_t WIRE_SIZE = 27; // as sizeof(MessageHeader) is 11 bytes, the total size of the message is 27 bytes
        OrderId order_id; // 8 bytes (64 bits)
        Price new_price; // 4 bytes (32 bits)
        Quantity new_quantity; // 4 bytes (32 bits)
    };
    static_assert(sizeof(MessageHeader) + sizeof(ReplaceOrder) == ReplaceOrder::WIRE_SIZE, "ReplaceOrder WIRE_SIZE mismatch");
    struct TradeMessage{
        static constexpr char TYPE = 'T';
        static constexpr size_t WIRE_SIZE = 43; // as sizeof(MessageHeader) is 11 bytes, the total size of the message is 43 bytes
        Symbol symbol; // 8 bytes (64 bits)
        Price price; // 4 bytes (32 bits)
        Quantity quantity; // 4 bytes (32 bits)
        OrderId buy_order_id; // 8 bytes (64 bits)
        OrderId sell_order_id; // 8 bytes (64 bits)
    };
    static_assert(sizeof(MessageHeader) + sizeof(TradeMessage) == TradeMessage::WIRE_SIZE, "TradeMessage WIRE_SIZE mismatch");
    using ParsedMessage = std::variant<AddOrder, CancelOrder, ExecuteOrder, ReplaceOrder, TradeMessage>;
     // variant is a type that can hold any of the types in the list (AddOrder, CancelOrder, ExecuteOrder, ReplaceOrder, TradeMessage) and is used to store the message in the pipeline
    // the reason we have parsed message is because we want to be able to store the message in the pipeline and then process it later
    
    #pragma pack(pop)

    // encode referese -> btyes(para mandar pela rede)
    // decode e o processor inverso, de bytes para o struct
    // um byte e um char, 
    // buffer de bytes e um array de uint8_t - (uint8_t) representa um byte, (recebe valor de 0 a 255)
    

}

