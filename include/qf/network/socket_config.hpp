#pragma once

#include <cstdint>
#include <string>
#include "qf/common/constants.hpp"

namespace qf::network {

struct SocketOptions {
    int         recv_buf_size    = SOCKET_RECV_BUF;
    int         send_buf_size    = SOCKET_SEND_BUF;
    int         multicast_ttl    = MULTICAST_TTL;
    bool        reuse_addr       = true;
    bool        loopback         = true;   // receive own multicast (for local testing)
    std::string interface_addr   = "0.0.0.0";  // bind to specific NIC (default: any)
};

}  // namespace qf::network
