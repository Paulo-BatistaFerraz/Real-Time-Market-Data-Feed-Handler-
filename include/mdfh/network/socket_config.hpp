#pragma once

#include <cstdint>
#include "mdfh/common/constants.hpp"

namespace mdfh::network {

struct SocketOptions {
    int      recv_buf_size = SOCKET_RECV_BUF;
    int      send_buf_size = SOCKET_SEND_BUF;
    int      multicast_ttl = MULTICAST_TTL;
    bool     reuse_addr    = true;
    bool     loopback      = true;   // receive own multicast (for local testing)
};

}  // namespace mdfh::network
