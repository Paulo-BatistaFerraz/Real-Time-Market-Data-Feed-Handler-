# Real-Time Market Data Feed Handler — Design Document

**Date:** 2026-02-24
**Language:** C++17, CMake, Boost.Asio, Google Test
**Platform:** Windows (MSYS2)
**Target size:** 3-5k lines of authored code, ~25 files

---

## 1. Purpose

Simulates the full pipeline of how trading firms ingest exchange market data:
- An **exchange simulator** broadcasts binary order events over UDP multicast
- A **feed handler** receives, decodes, and reconstructs live order books in real time

Two separate executables communicating over the wire with a custom binary protocol.

## 2. Architecture

```
┌─────────────────────┐        UDP Multicast        ┌─────────────────────────────────────────────────┐
│  exchange_simulator  │  ────────────────────────►  │              feed_handler                       │
│  (separate .exe)     │     MiniITCH Protocol       │                                                │
│                      │                             │  ┌──────────┐    ┌──────────┐    ┌───────────┐ │
│  - Reads scenario    │                             │  │ Network  │───►│ Parser   │───►│ OrderBook │ │
│    config (JSON)     │                             │  │ Receiver │ Q1 │ Thread   │ Q2 │ Thread    │ │
│  - Encodes binary    │                             │  │ Thread   │    │          │    │           │ │
│    messages          │                             │  └──────────┘    └──────────┘    └───────────┘ │
│  - Blasts over UDP   │                             │                                      │        │
│    at configurable   │                             │                                      │ Q3     │
│    rate              │                             │                                      ▼        │
└─────────────────────┘                             │                               ┌───────────┐   │
                                                     │                               │ Consumer  │   │
                                                     │                               │ Thread    │   │
                                                     │                               │ (stats +  │   │
                                                     │                               │  display) │   │
                                                     │                               └───────────┘   │
                                                     └───────────────────────────────────────────────────┘

Q1, Q2, Q3 = lock-free SPSC ring buffers
```

### Feed Handler — 4 Threads

1. **Network Receiver** — Boost.Asio `async_receive_from`, timestamps each packet on arrival, pushes raw bytes into Q1
2. **Parser** — pulls raw bytes from Q1, decodes binary messages by type, produces typed structs, pushes into Q2
3. **Order Book Manager** — pulls parsed messages from Q2, maintains bid/ask depth per symbol, pushes book updates into Q3
4. **Consumer** — pulls from Q3, tracks latency (receive timestamp -> book update complete), prints stats + optional console view

### Exchange Simulator

- Reads JSON scenario config (symbols, rates, initial prices, seed)
- Maintains its own in-memory order book so cancels/executes reference valid order IDs
- Weighted random message distribution: ~40% Add, ~25% Cancel, ~20% Execute, ~10% Replace, ~5% Trade
- Prices walk randomly around initial price (+-small delta per tick)
- Batches messages into UDP datagrams up to ~1400 bytes
- Deterministic seed for reproducible benchmarks

## 3. Custom Binary Protocol — "MiniITCH"

Little-endian, fixed-width fields, no string parsing on hot path.

### Message Envelope (every message)

```
Offset  Size   Field
------  ----   -----
0       2      message_length (total bytes including header)
2       1      message_type (char: 'A','X','E','R','T')
3       8      timestamp (nanoseconds since midnight, uint64)
11      ...    payload (varies by type)
```

### Message Types

| Type         | Char | Purpose         | Payload Fields                                                                  | Payload Size |
|--------------|------|-----------------|---------------------------------------------------------------------------------|--------------|
| AddOrder     | 'A'  | New order       | order_id(8), side(1), symbol(8), price(4), quantity(4)                          | 25 bytes     |
| CancelOrder  | 'X'  | Remove order    | order_id(8)                                                                     | 8 bytes      |
| ExecuteOrder | 'E'  | Partial/full fill | order_id(8), exec_quantity(4)                                                 | 12 bytes     |
| ReplaceOrder | 'R'  | Modify price/qty | order_id(8), new_price(4), new_quantity(4)                                     | 16 bytes     |
| TradeMessage | 'T'  | Crossed trade   | symbol(8), price(4), quantity(4), buy_order_id(8), sell_order_id(8)             | 32 bytes     |

### Design Choices

- `price` is `uint32_t` representing price x 10000 (fixed-point, avoids floating point)
- `symbol` is 8 bytes, null-padded (like ITCH)
- `order_id` is `uint64_t` monotonically increasing
- Multiple messages batched per UDP datagram

## 4. Order Book Data Structure

### Per-price level

```cpp
struct PriceLevel {
    uint32_t price;
    uint32_t total_quantity;
    uint16_t order_count;
};
```

### Storage

- **Order lookup:** `std::unordered_map<uint64_t, Order>` — order_id to Order struct (symbol, side, price, remaining_quantity). `reserve()` upfront to avoid allocations on hot path.
- **Price levels:** `std::map<uint32_t, PriceLevel>` per side per symbol. Sorted iteration for top-of-book display.
- **Symbol registry:** `std::unordered_map<uint64_t, OrderBook>` — symbol as uint64_t key, zero-cost lookup.

### Update Flows

**AddOrder:** insert into order map -> find symbol book -> find/create price level -> increment qty/count -> push BookUpdate to Q3

**CancelOrder:** lookup order_id -> get symbol/side/price -> decrement price level -> remove if empty -> erase from order map -> push BookUpdate

**ExecuteOrder:** lookup order_id -> reduce remaining_quantity -> update price level -> remove order if fully filled -> push BookUpdate

**ReplaceOrder:** cancel old price level entry -> update order's price/qty -> add to new price level -> push BookUpdate

## 5. Lock-Free SPSC Ring Buffer

```cpp
template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    alignas(64) std::atomic<size_t> head_{0};  // written by producer
    char pad1_[64 - sizeof(std::atomic<size_t>)];
    alignas(64) std::atomic<size_t> tail_{0};  // written by consumer
    char pad2_[64 - sizeof(std::atomic<size_t>)];
    std::array<T, Capacity> buffer_;

public:
    bool try_push(const T& item);  // returns false if full
    bool try_pop(T& item);         // returns false if empty
};
```

- `alignas(64)` separates head/tail onto different cache lines (prevents false sharing)
- Power-of-2 capacity: `index & (Capacity - 1)` replaces modulo
- `std::memory_order_acquire/release` only, no `seq_cst`
- Zero dynamic allocation

### Queue Contents

| Queue | Producer -> Consumer    | Element Type                              | Capacity |
|-------|------------------------|-------------------------------------------|----------|
| Q1    | Network -> Parser      | RawPacket (byte buffer + timestamp + len) | 65536    |
| Q2    | Parser -> Book Manager | ParsedMessage (variant of message types)  | 65536    |
| Q3    | Book Manager -> Consumer | BookUpdate (symbol + top bid/ask + latency) | 16384  |

## 6. Consumer — Stats + Display

### Stats Collector (always running)

- End-to-end latency: receive timestamp -> book update complete
- Histogram: p50, p95, p99, p999
- Counters: messages/sec, packets/sec, book updates/sec
- Reports every 1 second to stdout

### Console Display (togglable)

```
====================================================================
  AAPL                    MSFT                 GOOGL
--------------------------------------------------------------------
  ASK  185.15  x  200     411.20  x  150      178.50  x  200
  ASK  185.10  x  500     411.15  x  400      178.45  x  350
  BID  185.05  x  300     411.00  x  600      178.30  x  500
  BID  185.00  x  450     410.95  x  250      178.25  x  150
--------------------------------------------------------------------
  Msgs/sec: 1,245,000 | p50: 0.4us | p99: 1.2us | p999: 3.8us
====================================================================
```

Refreshes every 100ms via `std::cout` clear + reprint. No ncurses dependency.

## 7. Folder Structure

```
market-data-feed-handler/
├── CMakeLists.txt
├── README.md
├── config/
│   └── scenario.json
├── include/
│   └── mdfh/
│       ├── protocol/
│       │   ├── messages.hpp
│       │   └── encoder.hpp
│       ├── network/
│       │   ├── multicast_sender.hpp
│       │   └── multicast_receiver.hpp
│       ├── core/
│       │   ├── spsc_queue.hpp
│       │   ├── order_book.hpp
│       │   └── order_store.hpp
│       ├── consumer/
│       │   ├── stats_collector.hpp
│       │   └── console_display.hpp
│       └── common/
│           ├── types.hpp
│           └── clock.hpp
├── src/
│   ├── simulator/
│   │   ├── main.cpp
│   │   ├── market_simulator.cpp
│   │   └── market_simulator.hpp
│   ├── handler/
│   │   ├── main.cpp
│   │   ├── network_stage.cpp
│   │   ├── parser_stage.cpp
│   │   ├── book_stage.cpp
│   │   └── consumer_stage.cpp
│   └── protocol/
│       └── encoder.cpp
├── tests/
│   ├── test_spsc_queue.cpp
│   ├── test_order_book.cpp
│   ├── test_protocol.cpp
│   └── test_encoder.cpp
└── bench/
    └── bench_pipeline.cpp
```

## 8. Dependencies

- **Boost.Asio** — UDP multicast networking
- **nlohmann/json** — scenario config parsing (header-only)
- **Google Test** — unit tests
- **CMake 3.16+** — build system

## 9. Interview Talking Points

- "I designed the wire format myself based on ITCH conventions — fixed-point prices, fixed-width fields, no string parsing on the hot path"
- "Four pipeline stages on separate threads connected by lock-free SPSC queues I wrote from scratch"
- "Cache-line padding on atomics to prevent false sharing, acquire/release memory ordering — no seq_cst"
- "Sub-microsecond p99 latency from packet arrival to book update, measured end-to-end"
- "Deterministic simulator with configurable seed for reproducible benchmarks"
- "I built both sides — the exchange simulator and the feed handler — they communicate over UDP multicast"
