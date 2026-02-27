#pragma once // prevent multiple inclusion
#include "mdfh/protocol/messages.hpp" // define the structs
#include "mdfh/common/clock.hpp" // define the clock
#include <cstdint> // uint8_t, uint16_t, uint32_t, uint64_t
#include <cstring> // memcpy, memset


template <typename MsgT> // template de tipo generico para o encoder
static size_t encode(const MsgT& msg, Timestamp ts, uint8_t* buffer, size_t buffer_size){ // statico porque nao depende de instancia da classe, size_t porque o retorno e o numero de bytes escritos
    // uint8_t* buffer referece a um array de bytes qual o primeiro elemento do array e buffer[0] e o ultimo e buffer[buffer_size - 1], uint8_t e um byte, 8 bits, 0 a 255, 1 byte = 8 bits
    // estamos pontando para o primeiro elemento do array, buffer[0] e o ultimo e buffer[buffer_size - 1]
    // const MsgT& msg referece a uma mensagem do tipo MsgT, e msg e uma referencia para a mensagem, nao uma copia, e const porque nao queremos modificar a mensagem
    // Timestamp ts e o timestamp da mensagem, e e um uint64_t, 64 bits, 0 a 18446744073709551615, 8 bytes, nanoseconds e passado

    constexpr uint16_t wire_size = MsgT::WIRE_SIZE; // tamanho da mensagem em bytes
    // no tempo de compilacao depois que o compilador compila o codigo, o wire_size e substituido pelo valor do wire_size do tipo MsgT vem de messages.hpp
    if (buffer_size < wire_size) return 0; // se o buffer for menor que o tamanho da mensagem, retorna 0, explicando que o buffer e pequeno demais para a mensagem

    MessageHeader header; // header da mensagem
    header.message_length = wire_size; // tamanho da mensagem
    header.message_type = MsgT::TYPE; // tipo da mensagem
    header.timestamp = ts; // timestamp da mensagem

    std::memcpy(buffer, &header, sizeof(header)); // copia o header para o buffer
    std::memcpy(buffer + sizeof(header), &msg, wire_size - sizeof(header)); // copia o payload para o buffer
    return wire_size; // retorna o numero de bytes escritos
}



namespace mdfh::protocol{
    class Encoder{
        public:
    };

}