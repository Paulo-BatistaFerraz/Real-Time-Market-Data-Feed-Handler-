// offset 0 size 2 -> field message_length (uint16_t)
#pragma once // pragma once is used to prevent multiple 
#include <cstdint> // uint16_t, uint8_t, uint64_t these are all 8 bits

namespace mdfh{ // 
    #pragma pack(push, 1) // ??push the current packing alignment, (this means that the struct will be packed with no padding -> all fields are packed sequentially->)
    struct MessageHeader{ // header of the message ( defines the structure of the message)
        uint16_t message_length; // total bytes including this header (2 bytes) 
        char message_type; // message type as a raw byte (1 byte)
        uint64_t timestamp; // nanoseconds since midnight
    };
    #pragma pack(pop) 
}
