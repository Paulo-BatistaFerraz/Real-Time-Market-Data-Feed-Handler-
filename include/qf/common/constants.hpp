#pragma once

#include <cstddef>
#include <cstdint>

namespace qf {

// ── Network ──────────────────────────────────────────────
constexpr const char* MULTICAST_GROUP = "239.255.0.1";
constexpr uint16_t    MULTICAST_PORT  = 30001;
constexpr size_t      MAX_DATAGRAM    = 1400;   // bytes, fits in MTU (1500 - IP/UDP headers)
constexpr int         SOCKET_RECV_BUF = 4 * 1024 * 1024;  // 4 MB SO_RCVBUF
constexpr int         SOCKET_SEND_BUF = 2 * 1024 * 1024;  // 2 MB SO_SNDBUF
constexpr int         MULTICAST_TTL   = 1;                 // local network only

// ── SPSC Queue Capacities (must be power of 2) ──────────
constexpr size_t Q1_CAPACITY = 65536;   // Network → Parser     (RawPacket)
constexpr size_t Q2_CAPACITY = 65536;   // Parser  → Book Engine (ParsedEvent)
constexpr size_t Q3_CAPACITY = 16384;   // Book    → Consumer    (BookUpdate)

// ── Cache ────────────────────────────────────────────────
constexpr size_t CACHE_LINE = 64;       // bytes, x86 cache line size

// ── Receive Buffer ───────────────────────────────────────
constexpr size_t RAW_PACKET_BUF = 2048; // max bytes per received UDP packet

// ── Batch Header ─────────────────────────────────────────
//  ┌──────────────┬─────────────┬─────────────┐
//  │ seq_num (8B) │ msg_count(2)│ total_len(2)│ = 12 bytes
//  └──────────────┴─────────────┴─────────────┘
constexpr size_t BATCH_HEADER_SIZE = 12;
constexpr size_t MAX_PAYLOAD       = MAX_DATAGRAM - BATCH_HEADER_SIZE; // 1388 bytes for messages

// ── Order Book ───────────────────────────────────────────
constexpr size_t ORDER_STORE_INITIAL_CAPACITY = 100000; // pre-reserve hash map
constexpr size_t MAX_SYMBOLS                  = 64;     // max tracked instruments

// ── Consumer / Display ───────────────────────────────────
constexpr uint32_t DISPLAY_REFRESH_MS   = 100;   // console refresh interval
constexpr uint32_t CSV_LOG_INTERVAL_SEC = 1;     // one row per second
constexpr size_t   DEPTH_LEVELS         = 5;     // top-of-book levels to display

// ── Latency Histogram ────────────────────────────────────
constexpr uint64_t HISTOGRAM_MAX_US    = 10000;  // 10ms max tracked latency
constexpr size_t   HISTOGRAM_BUCKETS   = 1000;   // 10μs per bucket

// ── Simulator Defaults ───────────────────────────────────
constexpr uint32_t DEFAULT_MSG_RATE    = 10000;  // msgs/sec
constexpr uint64_t DEFAULT_DURATION    = 30;     // seconds
constexpr uint32_t DEFAULT_SEED        = 42;

// ── Fixed-Point Price ────────────────────────────────────
constexpr uint32_t PRICE_SCALE = 10000;          // 1 dollar = 10000 price units

}  // namespace qf
