#pragma once // prevent multiple inclusion
#include "mdfh/protocol/messages.hpp" // define the structs
#include "mdfh/common/clock.hpp" // define the clock
#include <cstdint> // uint8_t, uint16_t, uint32_t, uint64_t
#include <cstring> // memcpy, memset




// const MsgT& referece a qualquer tipo de mensage generica, AddOrder, CancelOrder, ExecuteOrder, ReplaceOrder, TradeMessage
// ts representa o Timestamp vem do common/clock.hpp
// buffer e um espaco de memoria alocado para a mensagem, e um array de bytes, uint8_t* buffer, size_t buffer_size e o tamanho do array de bytes
// buffer_size e o tamanho do array de bytes
// o endereco do buffer e o endereco do primeiro elemento do array, buffer[0]
// buffer[0] representa nesse caso o header da mensage que estar enserida no payload da mensagem, payload e o corpo da mensagem
// header e o cabeceiro da mensage, e payload e o corpo da mensagem

// decoder referece em palavras simples

namespace mdfh::protocol{
    class Encoder{
        public:
            template <typename MsgT> // template de tipo generico para o encoder
                static size_t encode(const MsgT& msg, Timestamp ts, uint8_t* buffer, size_t buffer_size){ // statico porque nao depende de instancia da classe, size_t porque o retorno e o numero de bytes escritos
            // uint8_t* buffer referece a um array de bytes qual o primeiro elemento do array e buffer[0] e o ultimo e buffer[buffer_size - 1], uint8_t e um byte, 8 bits, 0 a 255, 1 byte = 8 bits
            // estamos pontando para o primeiro elemento do array, buffer[0] e o ultimo e buffer[buffer_size - 1]
            // const MsgT& msg referece a uma mensagem do tipo MsgT, e msg e uma referencia para a mensagem, nao uma copia, e const porque nao queremos modificar a mensagem
            // Timestamp ts e o timestamp da mensagem, e e um uint64_t, 64 bits, 0 a 18446744073709551615, 8 bytes, nanoseconds e passado
        
             constexpr uint16_t wire_size = MsgT::WIRE_SIZE; // tamanho da mensagem em bytes
            // tempo de compilacao
            // no tempo de compilacao depois que o compilador compila o codigo, o wire_size e substituido pelo valor do wire_size do tipo MsgT vem de messages.hpp
            if (buffer_size < wire_size) return 0; // se o buffer for menor que o tamanho da mensagem, retorna 0, explicando que o buffer e pequeno demais para a mensagem
        
            MessageHeader header; // header da mensagem
            header.message_length = wire_size; // tamanho da mensagem
            header.message_type = MsgT::TYPE; // tipo da mensagem, // TYPE representa o tipo da mensagem
            header.timestamp = ts; // timestamp da mensagem
        
            std::memcpy(buffer, &header, sizeof(header)); // copia o header para o buffer , (destino, fonte, tamanho do bloco a copiar)
            std::memcpy(buffer + sizeof(header), &msg, wire_size - sizeof(header)); // copia o payload para o buffer
            
            return wire_size; // retorna o numero de bytes escritos
        }

        // decode_payload: copia bytes do buffer de volta pra um struct (inverso do encode)
        template <typename MsgT>
        static void decode_payload(const uint8_t* payload, MsgT& msg) {
            std::memcpy(&msg, payload, sizeof(msg)); // copia do buffer pro struct
        }

        // decode_header: lê o header de um buffer de bytes
        static MessageHeader decode_header(const uint8_t* buffer) {
            MessageHeader header;
            std::memcpy(&header, buffer, sizeof(header));
            return header; // retorna o header exemplo: MessageHeader header; header.message_length = 36; header.message_type = 'A'; header.timestamp = 0;
            
        }

        // peek_message_type: olha o tipo da mensagem sem decodificar tudo
        static MessageType peek_message_type(const uint8_t* buffer) {
            return static_cast<MessageType>(buffer[2]); // byte 2 = message_type
        }

        // parse: decodifica uma mensagem completa (header + payload) e retorna como ParsedMessage
        static ParsedMessage parse(const uint8_t* buffer) {
            auto header = decode_header(buffer);
            const uint8_t* payload = buffer + sizeof(MessageHeader);
            switch (static_cast<MessageType>(header.message_type)) {
                case MessageType::AddOrder: {
                    AddOrder msg;
                    decode_payload(payload, msg);
                    return msg;
                }
                case MessageType::CancelOrder: {
                    CancelOrder msg;
                    decode_payload(payload, msg);
                    return msg;
                }
                case MessageType::ExecuteOrder: {
                    ExecuteOrder msg;
                    decode_payload(payload, msg);
                    return msg;
                }
                case MessageType::ReplaceOrder: {
                    ReplaceOrder msg;
                    decode_payload(payload, msg);
                    return msg;
                }
                case MessageType::TradeMessage: {
                    TradeMessage msg;
                    decode_payload(payload, msg);
                    return msg;
                }
                default:
                    return AddOrder{};
            }
        }
    };
    

}

