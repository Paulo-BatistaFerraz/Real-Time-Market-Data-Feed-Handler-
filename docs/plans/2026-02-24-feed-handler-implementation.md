# Real-Time Market Data Feed Handler — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a two-executable system (exchange simulator + feed handler) communicating over UDP multicast with a custom binary protocol, reconstructing live order books in a 4-stage pipelined architecture.

**Architecture:** Exchange simulator encodes MiniITCH binary messages and blasts them over UDP multicast. Feed handler receives packets across a 4-thread pipeline (network → parser → book manager → consumer) connected by lock-free SPSC queues. Consumer tracks latency histograms and displays live top-of-book.

**Tech Stack:** C++17, CMake 3.16+, Boost.Asio (UDP multicast), nlohmann/json (config), Google Test (tests)

**Platform:** Windows (MSYS2/MinGW)

**Design doc:** `docs/plans/2026-02-24-feed-handler-design.md`

---

## Prerequisites

Install dependencies on MSYS2 (MINGW64 shell):

```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-boost mingw-w64-x86_64-nlohmann-json mingw-w64-x86_64-gtest
```

---

## Task 1: Project Skeleton — CMake + Directory Structure + Empty Mains

**Files:**
- Create: `CMakeLists.txt`
- Create: `include/mdfh/common/types.hpp`
- Create: `include/mdfh/common/clock.hpp`
- Create: `src/handler/main.cpp`
- Create: `src/simulator/main.cpp`
- Create: `config/scenario.json`

**Step 1: Create the top-level CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
project(market-data-feed-handler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
find_package(Boost REQUIRED COMPONENTS system)
find_package(nlohmann_json REQUIRED)
find_package(GTest REQUIRED)

# Include paths
include_directories(${CMAKE_SOURCE_DIR}/include)

# --- feed_handler executable ---
add_executable(feed_handler
    src/handler/main.cpp
)
target_link_libraries(feed_handler PRIVATE Boost::system ws2_32)

# --- exchange_simulator executable ---
add_executable(exchange_simulator
    src/simulator/main.cpp
)
target_link_libraries(exchange_simulator PRIVATE Boost::system nlohmann_json::nlohmann_json ws2_32)

# --- tests ---
enable_testing()

add_executable(run_tests
    tests/test_stub.cpp
)
target_link_libraries(run_tests PRIVATE GTest::GTest GTest::Main)
add_test(NAME AllTests COMMAND run_tests)
```

**Step 2: Create `include/mdfh/common/types.hpp`**

```cpp
#pragma once

#include <cstdint>
#include <cstring>

namespace mdfh {

enum class Side : uint8_t {
    Buy  = 0x01,
    Sell = 0x02
};

// Fixed-point price: actual_price = raw_price / 10000.0
// e.g., 1850500 = $185.0500
using Price = uint32_t;
using Quantity = uint32_t;
using OrderId = uint64_t;
using Timestamp = uint64_t;  // nanoseconds since midnight

constexpr size_t SYMBOL_LENGTH = 8;

struct Symbol {
    char data[SYMBOL_LENGTH];

    Symbol() { std::memset(data, 0, SYMBOL_LENGTH); }

    explicit Symbol(const char* str) {
        std::memset(data, 0, SYMBOL_LENGTH);
        std::strncpy(data, str, SYMBOL_LENGTH);
    }

    // Reinterpret as uint64 for O(1) hashing/comparison
    uint64_t as_key() const {
        uint64_t key;
        std::memcpy(&key, data, sizeof(key));
        return key;
    }

    bool operator==(const Symbol& other) const {
        return as_key() == other.as_key();
    }
};

struct SymbolHash {
    size_t operator()(const Symbol& s) const {
        return std::hash<uint64_t>{}(s.as_key());
    }
};

// Convert fixed-point price to display string
inline double price_to_double(Price p) {
    return static_cast<double>(p) / 10000.0;
}

inline Price double_to_price(double d) {
    return static_cast<Price>(d * 10000.0);
}

}  // namespace mdfh
```

**Step 3: Create `include/mdfh/common/clock.hpp`**

```cpp
#pragma once

#include <chrono>
#include <cstdint>

namespace mdfh {

struct Clock {
    using HiResClock = std::chrono::high_resolution_clock;

    static uint64_t now_ns() {
        auto now = HiResClock::now();
        auto since_epoch = now.time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch).count()
        );
    }

    // Nanoseconds since midnight (for protocol timestamps)
    static uint64_t nanos_since_midnight() {
        auto now = std::chrono::system_clock::now();
        auto today = std::chrono::floor<std::chrono::days>(now);
        auto since_midnight = now - today;
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(since_midnight).count()
        );
    }
};

}  // namespace mdfh
```

**Step 4: Create empty mains**

`src/handler/main.cpp`:
```cpp
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "[FEED] Market Data Feed Handler v0.1\n";
    return 0;
}
```

`src/simulator/main.cpp`:
```cpp
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "[SIM] Exchange Simulator v0.1\n";
    return 0;
}
```

**Step 5: Create stub test**

`tests/test_stub.cpp`:
```cpp
#include <gtest/gtest.h>

TEST(Stub, Compiles) {
    EXPECT_TRUE(true);
}
```

**Step 6: Create default scenario config**

`config/scenario.json`:
```json
{
    "multicast_address": "239.1.1.1",
    "port": 12345,
    "symbols": ["AAPL", "MSFT", "GOOGL", "TSLA", "AMZN"],
    "messages_per_second": 100000,
    "duration_seconds": 30,
    "seed": 42,
    "initial_prices": {
        "AAPL": 1850000,
        "MSFT": 4100000,
        "GOOGL": 1780000,
        "TSLA": 2500000,
        "AMZN": 1920000
    }
}
```

**Step 7: Build and verify**

```bash
cd D:/market-data-feed-handler
cmake -B build
cmake --build build
./build/feed_handler.exe
./build/exchange_simulator.exe
./build/run_tests.exe
```

Expected:
- `[FEED] Market Data Feed Handler v0.1`
- `[SIM] Exchange Simulator v0.1`
- `[  PASSED  ] 1 test`

**Step 8: Commit**

```bash
git add -A
git commit -m "feat: project skeleton with CMake, types, clock, empty mains"
```

---

## Task 2: Protocol Messages — MiniITCH Structs

**Files:**
- Create: `include/mdfh/protocol/messages.hpp`

**Step 1: Create `include/mdfh/protocol/messages.hpp`**

All message structs packed for wire format. Uses `#pragma pack(push, 1)` for zero-padding binary layout.

```cpp
#pragma once

#include "mdfh/common/types.hpp"
#include <cstdint>
#include <cstring>
#include <variant>

namespace mdfh::protocol {

// Message type identifiers
enum class MessageType : uint8_t {
    AddOrder     = 'A',
    CancelOrder  = 'X',
    ExecuteOrder = 'E',
    ReplaceOrder = 'R',
    TradeMessage = 'T'
};

#pragma pack(push, 1)

// 11-byte envelope, prefixed to every message on the wire
struct MessageHeader {
    uint16_t    message_length;  // total bytes including this header
    uint8_t     message_type;    // MessageType as raw byte
    Timestamp   timestamp;       // nanoseconds since midnight
};

// 'A' — New order placed on the book (25-byte payload)
struct AddOrder {
    static constexpr uint8_t TYPE = 'A';
    static constexpr uint16_t WIRE_SIZE = sizeof(MessageHeader) + 25;

    OrderId   order_id;
    uint8_t   side;          // Side::Buy or Side::Sell
    Symbol    symbol;
    Price     price;
    Quantity  quantity;
};

// 'X' — Order removed from the book (8-byte payload)
struct CancelOrder {
    static constexpr uint8_t TYPE = 'X';
    static constexpr uint16_t WIRE_SIZE = sizeof(MessageHeader) + 8;

    OrderId order_id;
};

// 'E' — Order partially or fully filled (12-byte payload)
struct ExecuteOrder {
    static constexpr uint8_t TYPE = 'E';
    static constexpr uint16_t WIRE_SIZE = sizeof(MessageHeader) + 12;

    OrderId   order_id;
    Quantity  exec_quantity;
};

// 'R' — Order price/quantity modified (16-byte payload)
struct ReplaceOrder {
    static constexpr uint8_t TYPE = 'R';
    static constexpr uint16_t WIRE_SIZE = sizeof(MessageHeader) + 16;

    OrderId   order_id;
    Price     new_price;
    Quantity  new_quantity;
};

// 'T' — Crossed trade (32-byte payload)
struct TradeMessage {
    static constexpr uint8_t TYPE = 'T';
    static constexpr uint16_t WIRE_SIZE = sizeof(MessageHeader) + 32;

    Symbol    symbol;
    Price     price;
    Quantity  quantity;
    OrderId   buy_order_id;
    OrderId   sell_order_id;
};

#pragma pack(pop)

// Variant for parsed messages flowing through the pipeline
using ParsedMessage = std::variant<AddOrder, CancelOrder, ExecuteOrder, ReplaceOrder, TradeMessage>;

}  // namespace mdfh::protocol
```

**Step 2: Write test for message sizes**

Replace `tests/test_stub.cpp` content, and create `tests/test_protocol.cpp`:

```cpp
#include <gtest/gtest.h>
#include "mdfh/protocol/messages.hpp"

using namespace mdfh::protocol;

TEST(Protocol, MessageHeaderSize) {
    EXPECT_EQ(sizeof(MessageHeader), 11);
}

TEST(Protocol, AddOrderPayloadSize) {
    EXPECT_EQ(sizeof(AddOrder::order_id) + sizeof(AddOrder::side) +
              sizeof(AddOrder::symbol) + sizeof(AddOrder::price) +
              sizeof(AddOrder::quantity), 25);
}

TEST(Protocol, CancelOrderPayloadSize) {
    EXPECT_EQ(sizeof(CancelOrder::order_id), 8);
}

TEST(Protocol, ExecuteOrderPayloadSize) {
    EXPECT_EQ(sizeof(ExecuteOrder::order_id) + sizeof(ExecuteOrder::exec_quantity), 12);
}

TEST(Protocol, ReplaceOrderPayloadSize) {
    EXPECT_EQ(sizeof(ReplaceOrder::order_id) + sizeof(ReplaceOrder::new_price) +
              sizeof(ReplaceOrder::new_quantity), 16);
}

TEST(Protocol, TradeMessagePayloadSize) {
    EXPECT_EQ(sizeof(TradeMessage::symbol) + sizeof(TradeMessage::price) +
              sizeof(TradeMessage::quantity) + sizeof(TradeMessage::buy_order_id) +
              sizeof(TradeMessage::sell_order_id), 32);
}

TEST(Protocol, WireSizesCorrect) {
    EXPECT_EQ(AddOrder::WIRE_SIZE, 36);
    EXPECT_EQ(CancelOrder::WIRE_SIZE, 19);
    EXPECT_EQ(ExecuteOrder::WIRE_SIZE, 23);
    EXPECT_EQ(ReplaceOrder::WIRE_SIZE, 27);
    EXPECT_EQ(TradeMessage::WIRE_SIZE, 43);
}
```

**Step 3: Update CMakeLists.txt tests section**

Replace the test target:
```cmake
add_executable(run_tests
    tests/test_protocol.cpp
)
target_link_libraries(run_tests PRIVATE GTest::GTest GTest::Main)
```

**Step 4: Build and run tests**

```bash
cmake --build build
./build/run_tests.exe
```

Expected: `[  PASSED  ] 7 tests`

**Step 5: Commit**

```bash
git add -A
git commit -m "feat: MiniITCH protocol message structs with size validation tests"
```

---

## Task 3: Encoder — Serialize and Deserialize Binary Messages

**Files:**
- Create: `include/mdfh/protocol/encoder.hpp`
- Create: `src/protocol/encoder.cpp`
- Create: `tests/test_encoder.cpp`

**Step 1: Write the encoder tests first**

`tests/test_encoder.cpp`:
```cpp
#include <gtest/gtest.h>
#include "mdfh/protocol/encoder.hpp"

using namespace mdfh;
using namespace mdfh::protocol;

TEST(Encoder, EncodeDecodeAddOrder) {
    AddOrder add{};
    add.order_id = 12345;
    add.side = static_cast<uint8_t>(Side::Buy);
    add.symbol = Symbol("AAPL");
    add.price = 1850500;  // $185.0500
    add.quantity = 300;

    uint8_t buffer[1500];
    size_t written = Encoder::encode(add, Clock::nanos_since_midnight(), buffer, sizeof(buffer));
    EXPECT_EQ(written, AddOrder::WIRE_SIZE);

    // Decode
    MessageHeader header;
    std::memcpy(&header, buffer, sizeof(header));
    EXPECT_EQ(header.message_type, AddOrder::TYPE);
    EXPECT_EQ(header.message_length, AddOrder::WIRE_SIZE);

    AddOrder decoded;
    Encoder::decode_payload(buffer + sizeof(MessageHeader), decoded);
    EXPECT_EQ(decoded.order_id, 12345);
    EXPECT_EQ(decoded.side, static_cast<uint8_t>(Side::Buy));
    EXPECT_EQ(decoded.symbol, Symbol("AAPL"));
    EXPECT_EQ(decoded.price, 1850500u);
    EXPECT_EQ(decoded.quantity, 300u);
}

TEST(Encoder, EncodeDecodeCancelOrder) {
    CancelOrder cancel{};
    cancel.order_id = 99999;

    uint8_t buffer[1500];
    size_t written = Encoder::encode(cancel, 0, buffer, sizeof(buffer));
    EXPECT_EQ(written, CancelOrder::WIRE_SIZE);

    MessageHeader header;
    std::memcpy(&header, buffer, sizeof(header));
    EXPECT_EQ(header.message_type, CancelOrder::TYPE);

    CancelOrder decoded;
    Encoder::decode_payload(buffer + sizeof(MessageHeader), decoded);
    EXPECT_EQ(decoded.order_id, 99999u);
}

TEST(Encoder, EncodeDecodeExecuteOrder) {
    ExecuteOrder exec{};
    exec.order_id = 12345;
    exec.exec_quantity = 100;

    uint8_t buffer[1500];
    size_t written = Encoder::encode(exec, 0, buffer, sizeof(buffer));
    EXPECT_EQ(written, ExecuteOrder::WIRE_SIZE);

    ExecuteOrder decoded;
    Encoder::decode_payload(buffer + sizeof(MessageHeader), decoded);
    EXPECT_EQ(decoded.order_id, 12345u);
    EXPECT_EQ(decoded.exec_quantity, 100u);
}

TEST(Encoder, EncodeDecodeReplaceOrder) {
    ReplaceOrder replace{};
    replace.order_id = 555;
    replace.new_price = 1860000;
    replace.new_quantity = 200;

    uint8_t buffer[1500];
    size_t written = Encoder::encode(replace, 0, buffer, sizeof(buffer));
    EXPECT_EQ(written, ReplaceOrder::WIRE_SIZE);

    ReplaceOrder decoded;
    Encoder::decode_payload(buffer + sizeof(MessageHeader), decoded);
    EXPECT_EQ(decoded.order_id, 555u);
    EXPECT_EQ(decoded.new_price, 1860000u);
    EXPECT_EQ(decoded.new_quantity, 200u);
}

TEST(Encoder, EncodeDecodeTradeMessage) {
    TradeMessage trade{};
    trade.symbol = Symbol("TSLA");
    trade.price = 2500000;
    trade.quantity = 50;
    trade.buy_order_id = 100;
    trade.sell_order_id = 200;

    uint8_t buffer[1500];
    size_t written = Encoder::encode(trade, 0, buffer, sizeof(buffer));
    EXPECT_EQ(written, TradeMessage::WIRE_SIZE);

    TradeMessage decoded;
    Encoder::decode_payload(buffer + sizeof(MessageHeader), decoded);
    EXPECT_EQ(decoded.symbol, Symbol("TSLA"));
    EXPECT_EQ(decoded.price, 2500000u);
    EXPECT_EQ(decoded.quantity, 50u);
    EXPECT_EQ(decoded.buy_order_id, 100u);
    EXPECT_EQ(decoded.sell_order_id, 200u);
}

TEST(Encoder, BatchMultipleMessages) {
    uint8_t buffer[1500];
    size_t offset = 0;

    AddOrder add{};
    add.order_id = 1;
    add.side = static_cast<uint8_t>(Side::Buy);
    add.symbol = Symbol("AAPL");
    add.price = 1850000;
    add.quantity = 100;
    offset += Encoder::encode(add, 0, buffer + offset, sizeof(buffer) - offset);

    CancelOrder cancel{};
    cancel.order_id = 1;
    offset += Encoder::encode(cancel, 0, buffer + offset, sizeof(buffer) - offset);

    EXPECT_EQ(offset, AddOrder::WIRE_SIZE + CancelOrder::WIRE_SIZE);

    // Walk through the buffer
    size_t pos = 0;
    MessageHeader h1;
    std::memcpy(&h1, buffer + pos, sizeof(h1));
    EXPECT_EQ(h1.message_type, AddOrder::TYPE);
    pos += h1.message_length;

    MessageHeader h2;
    std::memcpy(&h2, buffer + pos, sizeof(h2));
    EXPECT_EQ(h2.message_type, CancelOrder::TYPE);
    pos += h2.message_length;

    EXPECT_EQ(pos, offset);
}

TEST(Encoder, BufferTooSmallReturnsZero) {
    AddOrder add{};
    uint8_t buffer[5];  // way too small
    size_t written = Encoder::encode(add, 0, buffer, sizeof(buffer));
    EXPECT_EQ(written, 0u);
}

TEST(Encoder, ParseMessageType) {
    AddOrder add{};
    add.order_id = 1;
    add.side = static_cast<uint8_t>(Side::Buy);
    add.symbol = Symbol("AAPL");
    add.price = 1850000;
    add.quantity = 100;

    uint8_t buffer[1500];
    Encoder::encode(add, 0, buffer, sizeof(buffer));

    auto type = Encoder::peek_message_type(buffer);
    EXPECT_EQ(type, MessageType::AddOrder);
}
```

**Step 2: Run tests to verify they fail**

```bash
cmake --build build
```

Expected: compile error — `Encoder` class doesn't exist yet.

**Step 3: Write `include/mdfh/protocol/encoder.hpp`**

```cpp
#pragma once

#include "mdfh/protocol/messages.hpp"
#include "mdfh/common/clock.hpp"
#include <cstdint>
#include <cstring>

namespace mdfh::protocol {

class Encoder {
public:
    // Encode a message into buffer. Returns bytes written, or 0 if buffer too small.
    template <typename MsgT>
    static size_t encode(const MsgT& msg, Timestamp ts, uint8_t* buffer, size_t buffer_size) {
        constexpr uint16_t wire_size = MsgT::WIRE_SIZE;
        if (buffer_size < wire_size) return 0;

        MessageHeader header;
        header.message_length = wire_size;
        header.message_type = MsgT::TYPE;
        header.timestamp = ts;

        std::memcpy(buffer, &header, sizeof(header));
        std::memcpy(buffer + sizeof(header), &msg, wire_size - sizeof(header));
        return wire_size;
    }

    // Decode payload bytes into a message struct
    template <typename MsgT>
    static void decode_payload(const uint8_t* payload, MsgT& msg) {
        std::memcpy(&msg, payload, sizeof(msg));
    }

    // Read the header from raw bytes
    static MessageHeader decode_header(const uint8_t* buffer) {
        MessageHeader header;
        std::memcpy(&header, buffer, sizeof(header));
        return header;
    }

    // Peek at message type without full decode
    static MessageType peek_message_type(const uint8_t* buffer) {
        return static_cast<MessageType>(buffer[2]);
    }

    // Parse a complete message from buffer into a ParsedMessage variant.
    // buffer points to the start of the message (including header).
    // Returns the parsed message variant.
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
                // Unknown message type — return a default AddOrder (should not happen)
                return AddOrder{};
        }
    }
};

}  // namespace mdfh::protocol
```

**Step 4: Update CMakeLists.txt — add encoder test**

```cmake
add_executable(run_tests
    tests/test_protocol.cpp
    tests/test_encoder.cpp
)
target_link_libraries(run_tests PRIVATE GTest::GTest GTest::Main)
```

**Step 5: Build and run tests**

```bash
cmake --build build
./build/run_tests.exe
```

Expected: `[  PASSED  ] 15 tests` (7 protocol + 8 encoder)

**Step 6: Commit**

```bash
git add -A
git commit -m "feat: binary encoder/decoder with full roundtrip tests"
```

---

## Task 4: Lock-Free SPSC Queue

**Files:**
- Create: `include/mdfh/core/spsc_queue.hpp`
- Create: `tests/test_spsc_queue.cpp`

**Step 1: Write the SPSC queue tests first**

`tests/test_spsc_queue.cpp`:
```cpp
#include <gtest/gtest.h>
#include "mdfh/core/spsc_queue.hpp"
#include <thread>
#include <numeric>

using namespace mdfh;

TEST(SPSCQueue, PushPopSingleElement) {
    SPSCQueue<int, 16> q;
    EXPECT_TRUE(q.try_push(42));
    int val = 0;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 42);
}

TEST(SPSCQueue, PopFromEmptyReturnsFalse) {
    SPSCQueue<int, 16> q;
    int val = 0;
    EXPECT_FALSE(q.try_pop(val));
}

TEST(SPSCQueue, PushToFullReturnsFalse) {
    SPSCQueue<int, 4> q;  // capacity 4, usable slots = 3 (one sentinel)
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_TRUE(q.try_push(3));
    EXPECT_FALSE(q.try_push(4));  // full
}

TEST(SPSCQueue, FIFO_Order) {
    SPSCQueue<int, 16> q;
    for (int i = 0; i < 10; ++i)
        q.try_push(i);

    for (int i = 0; i < 10; ++i) {
        int val = -1;
        EXPECT_TRUE(q.try_pop(val));
        EXPECT_EQ(val, i);
    }
}

TEST(SPSCQueue, WrapAround) {
    SPSCQueue<int, 4> q;
    // Fill and drain several times to force wraparound
    for (int round = 0; round < 5; ++round) {
        EXPECT_TRUE(q.try_push(round * 10 + 1));
        EXPECT_TRUE(q.try_push(round * 10 + 2));
        EXPECT_TRUE(q.try_push(round * 10 + 3));

        int v;
        EXPECT_TRUE(q.try_pop(v)); EXPECT_EQ(v, round * 10 + 1);
        EXPECT_TRUE(q.try_pop(v)); EXPECT_EQ(v, round * 10 + 2);
        EXPECT_TRUE(q.try_pop(v)); EXPECT_EQ(v, round * 10 + 3);
    }
}

TEST(SPSCQueue, SizeMethod) {
    SPSCQueue<int, 16> q;
    EXPECT_EQ(q.size(), 0u);
    q.try_push(1);
    q.try_push(2);
    EXPECT_EQ(q.size(), 2u);
    int v;
    q.try_pop(v);
    EXPECT_EQ(q.size(), 1u);
}

TEST(SPSCQueue, ConcurrentProducerConsumer) {
    constexpr int COUNT = 1'000'000;
    SPSCQueue<int, 65536> q;

    std::thread producer([&]() {
        for (int i = 0; i < COUNT; ++i) {
            while (!q.try_push(i)) {}  // spin until space
        }
    });

    std::vector<int> received;
    received.reserve(COUNT);

    std::thread consumer([&]() {
        int val;
        while (static_cast<int>(received.size()) < COUNT) {
            if (q.try_pop(val)) {
                received.push_back(val);
            }
        }
    });

    producer.join();
    consumer.join();

    // Verify all values received in order
    ASSERT_EQ(received.size(), COUNT);
    for (int i = 0; i < COUNT; ++i) {
        EXPECT_EQ(received[i], i) << "Mismatch at index " << i;
    }
}
```

**Step 2: Run tests — expect compile failure**

```bash
cmake --build build
```

Expected: error — `spsc_queue.hpp` doesn't exist.

**Step 3: Implement `include/mdfh/core/spsc_queue.hpp`**

```cpp
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace mdfh {

template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");
    static_assert(Capacity >= 2, "Capacity must be at least 2");

public:
    SPSCQueue() = default;

    // Non-copyable, non-movable
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    bool try_push(const T& item) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & kMask;

        if (next == tail_.load(std::memory_order_acquire)) {
            return false;  // full
        }

        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool try_pop(T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_.load(std::memory_order_acquire)) {
            return false;  // empty
        }

        item = buffer_[tail];
        tail_.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail + Capacity) & kMask;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

private:
    static constexpr size_t kMask = Capacity - 1;

    // Cache-line separated to prevent false sharing
    alignas(64) std::atomic<size_t> head_{0};
    char pad1_[64 - sizeof(std::atomic<size_t>)];

    alignas(64) std::atomic<size_t> tail_{0};
    char pad2_[64 - sizeof(std::atomic<size_t>)];

    std::array<T, Capacity> buffer_;
};

}  // namespace mdfh
```

**Step 4: Update CMakeLists.txt — add queue test**

```cmake
add_executable(run_tests
    tests/test_protocol.cpp
    tests/test_encoder.cpp
    tests/test_spsc_queue.cpp
)
```

**Step 5: Build and run tests**

```bash
cmake --build build
./build/run_tests.exe
```

Expected: `[  PASSED  ] 22 tests` (7 protocol + 8 encoder + 7 queue)

The concurrent test passes 1M integers through the queue between two threads and verifies FIFO order.

**Step 6: Commit**

```bash
git add -A
git commit -m "feat: lock-free SPSC queue with cache-line padding and concurrent tests"
```

---

## Task 5: Order Store + Order Book

**Files:**
- Create: `include/mdfh/core/order_store.hpp`
- Create: `include/mdfh/core/order_book.hpp`
- Create: `tests/test_order_book.cpp`

**Step 1: Write order book tests first**

`tests/test_order_book.cpp`:
```cpp
#include <gtest/gtest.h>
#include "mdfh/core/order_book.hpp"

using namespace mdfh;

class OrderBookTest : public ::testing::Test {
protected:
    OrderBookManager manager;
    Symbol aapl{"AAPL"};
};

TEST_F(OrderBookTest, AddBuyOrder) {
    manager.add_order(1, Side::Buy, aapl, 1850000, 100);

    auto* book = manager.get_book(aapl);
    ASSERT_NE(book, nullptr);
    EXPECT_EQ(book->best_bid_price(), 1850000u);
    EXPECT_EQ(book->best_bid_quantity(), 100u);
}

TEST_F(OrderBookTest, AddSellOrder) {
    manager.add_order(1, Side::Sell, aapl, 1860000, 200);

    auto* book = manager.get_book(aapl);
    ASSERT_NE(book, nullptr);
    EXPECT_EQ(book->best_ask_price(), 1860000u);
    EXPECT_EQ(book->best_ask_quantity(), 200u);
}

TEST_F(OrderBookTest, MultipleBidLevelsSorted) {
    manager.add_order(1, Side::Buy, aapl, 1850000, 100);
    manager.add_order(2, Side::Buy, aapl, 1849000, 200);
    manager.add_order(3, Side::Buy, aapl, 1851000, 150);

    auto* book = manager.get_book(aapl);
    // Best bid should be highest price
    EXPECT_EQ(book->best_bid_price(), 1851000u);
}

TEST_F(OrderBookTest, MultipleAskLevelsSorted) {
    manager.add_order(1, Side::Sell, aapl, 1860000, 100);
    manager.add_order(2, Side::Sell, aapl, 1870000, 200);
    manager.add_order(3, Side::Sell, aapl, 1855000, 150);

    auto* book = manager.get_book(aapl);
    // Best ask should be lowest price
    EXPECT_EQ(book->best_ask_price(), 1855000u);
}

TEST_F(OrderBookTest, AggregateQuantityAtSamePrice) {
    manager.add_order(1, Side::Buy, aapl, 1850000, 100);
    manager.add_order(2, Side::Buy, aapl, 1850000, 250);

    auto* book = manager.get_book(aapl);
    EXPECT_EQ(book->best_bid_quantity(), 350u);
    EXPECT_EQ(book->bid_order_count(1850000), 2u);
}

TEST_F(OrderBookTest, CancelOrder) {
    manager.add_order(1, Side::Buy, aapl, 1850000, 100);
    manager.add_order(2, Side::Buy, aapl, 1850000, 200);
    manager.cancel_order(1);

    auto* book = manager.get_book(aapl);
    EXPECT_EQ(book->best_bid_quantity(), 200u);
    EXPECT_EQ(book->bid_order_count(1850000), 1u);
}

TEST_F(OrderBookTest, CancelLastOrderRemovesPriceLevel) {
    manager.add_order(1, Side::Buy, aapl, 1850000, 100);
    manager.cancel_order(1);

    auto* book = manager.get_book(aapl);
    EXPECT_EQ(book->best_bid_price(), 0u);  // no bids
}

TEST_F(OrderBookTest, ExecuteOrderPartialFill) {
    manager.add_order(1, Side::Buy, aapl, 1850000, 300);
    manager.execute_order(1, 100);

    auto* book = manager.get_book(aapl);
    EXPECT_EQ(book->best_bid_quantity(), 200u);
}

TEST_F(OrderBookTest, ExecuteOrderFullFillRemoves) {
    manager.add_order(1, Side::Buy, aapl, 1850000, 300);
    manager.execute_order(1, 300);

    auto* book = manager.get_book(aapl);
    EXPECT_EQ(book->best_bid_price(), 0u);  // fully filled, level gone
}

TEST_F(OrderBookTest, ReplaceOrder) {
    manager.add_order(1, Side::Buy, aapl, 1850000, 100);
    manager.replace_order(1, 1860000, 200);

    auto* book = manager.get_book(aapl);
    EXPECT_EQ(book->best_bid_price(), 1860000u);
    EXPECT_EQ(book->best_bid_quantity(), 200u);
    // Old level should be gone
    EXPECT_EQ(book->bid_order_count(1850000), 0u);
}

TEST_F(OrderBookTest, MultipleSymbols) {
    Symbol msft("MSFT");
    manager.add_order(1, Side::Buy, aapl, 1850000, 100);
    manager.add_order(2, Side::Buy, msft, 4100000, 200);

    auto* aapl_book = manager.get_book(aapl);
    auto* msft_book = manager.get_book(msft);

    EXPECT_EQ(aapl_book->best_bid_price(), 1850000u);
    EXPECT_EQ(msft_book->best_bid_price(), 4100000u);
}

TEST_F(OrderBookTest, CancelNonexistentOrderNoOp) {
    // Should not crash or throw
    manager.cancel_order(99999);
}

TEST_F(OrderBookTest, TopOfBookDepth) {
    // Add 5 bid levels
    for (uint32_t i = 0; i < 5; ++i) {
        manager.add_order(i + 1, Side::Buy, aapl, 1850000 - i * 1000, 100);
    }

    auto* book = manager.get_book(aapl);
    auto levels = book->get_bid_levels(3);
    ASSERT_EQ(levels.size(), 3u);
    // Should be sorted descending (best first)
    EXPECT_EQ(levels[0].price, 1850000u);
    EXPECT_EQ(levels[1].price, 1849000u);
    EXPECT_EQ(levels[2].price, 1848000u);
}
```

**Step 2: Run tests — expect compile failure**

```bash
cmake --build build
```

Expected: error — `order_book.hpp` doesn't exist.

**Step 3: Implement `include/mdfh/core/order_store.hpp`**

```cpp
#pragma once

#include "mdfh/common/types.hpp"
#include <unordered_map>

namespace mdfh {

struct Order {
    OrderId   order_id;
    Side      side;
    Symbol    symbol;
    Price     price;
    Quantity  remaining_quantity;
};

class OrderStore {
public:
    OrderStore() { orders_.reserve(100000); }

    void insert(const Order& order) {
        orders_[order.order_id] = order;
    }

    Order* find(OrderId id) {
        auto it = orders_.find(id);
        return (it != orders_.end()) ? &it->second : nullptr;
    }

    void erase(OrderId id) {
        orders_.erase(id);
    }

private:
    std::unordered_map<OrderId, Order> orders_;
};

}  // namespace mdfh
```

**Step 4: Implement `include/mdfh/core/order_book.hpp`**

```cpp
#pragma once

#include "mdfh/common/types.hpp"
#include "mdfh/core/order_store.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace mdfh {

struct PriceLevel {
    Price    price;
    Quantity total_quantity;
    uint16_t order_count;
};

class OrderBook {
public:
    explicit OrderBook(Symbol sym) : symbol_(sym) {}

    void add_level_quantity(Side side, Price price, Quantity qty) {
        auto& levels = (side == Side::Buy) ? bids_ : asks_;
        auto& lvl = levels[price];
        lvl.price = price;
        lvl.total_quantity += qty;
        lvl.order_count++;
    }

    void remove_level_quantity(Side side, Price price, Quantity qty) {
        auto& levels = (side == Side::Buy) ? bids_ : asks_;
        auto it = levels.find(price);
        if (it == levels.end()) return;

        auto& lvl = it->second;
        lvl.total_quantity = (qty >= lvl.total_quantity) ? 0 : lvl.total_quantity - qty;
        if (lvl.order_count > 0) lvl.order_count--;

        if (lvl.total_quantity == 0) {
            levels.erase(it);
        }
    }

    // Best bid = highest price
    Price best_bid_price() const {
        if (bids_.empty()) return 0;
        return bids_.rbegin()->first;
    }

    Quantity best_bid_quantity() const {
        if (bids_.empty()) return 0;
        return bids_.rbegin()->second.total_quantity;
    }

    // Best ask = lowest price
    Price best_ask_price() const {
        if (asks_.empty()) return 0;
        return asks_.begin()->first;
    }

    Quantity best_ask_quantity() const {
        if (asks_.empty()) return 0;
        return asks_.begin()->second.total_quantity;
    }

    uint16_t bid_order_count(Price price) const {
        auto it = bids_.find(price);
        return (it != bids_.end()) ? it->second.order_count : 0;
    }

    uint16_t ask_order_count(Price price) const {
        auto it = asks_.find(price);
        return (it != asks_.end()) ? it->second.order_count : 0;
    }

    // Get top N bid levels, sorted best (highest) first
    std::vector<PriceLevel> get_bid_levels(size_t n) const {
        std::vector<PriceLevel> result;
        size_t count = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend() && count < n; ++it, ++count) {
            result.push_back(it->second);
        }
        return result;
    }

    // Get top N ask levels, sorted best (lowest) first
    std::vector<PriceLevel> get_ask_levels(size_t n) const {
        std::vector<PriceLevel> result;
        size_t count = 0;
        for (auto it = asks_.begin(); it != asks_.end() && count < n; ++it, ++count) {
            result.push_back(it->second);
        }
        return result;
    }

    const Symbol& symbol() const { return symbol_; }

private:
    Symbol symbol_;
    std::map<Price, PriceLevel> bids_;   // sorted ascending, iterate rbegin for best
    std::map<Price, PriceLevel> asks_;   // sorted ascending, iterate begin for best
};


// Manages all order books across symbols
class OrderBookManager {
public:
    void add_order(OrderId id, Side side, const Symbol& sym, Price price, Quantity qty) {
        Order order{id, side, sym, price, qty};
        store_.insert(order);
        get_or_create_book(sym).add_level_quantity(side, price, qty);
    }

    void cancel_order(OrderId id) {
        Order* order = store_.find(id);
        if (!order) return;

        auto* book = get_book(order->symbol);
        if (book) {
            book->remove_level_quantity(order->side, order->price, order->remaining_quantity);
        }
        store_.erase(id);
    }

    void execute_order(OrderId id, Quantity exec_qty) {
        Order* order = store_.find(id);
        if (!order) return;

        auto* book = get_book(order->symbol);
        if (book) {
            book->remove_level_quantity(order->side, order->price, exec_qty);
        }

        if (exec_qty >= order->remaining_quantity) {
            store_.erase(id);
        } else {
            order->remaining_quantity -= exec_qty;
        }
    }

    void replace_order(OrderId id, Price new_price, Quantity new_qty) {
        Order* order = store_.find(id);
        if (!order) return;

        auto* book = get_book(order->symbol);
        if (book) {
            // Remove from old level
            book->remove_level_quantity(order->side, order->price, order->remaining_quantity);
            // Add to new level
            book->add_level_quantity(order->side, new_price, new_qty);
        }

        order->price = new_price;
        order->remaining_quantity = new_qty;
    }

    OrderBook* get_book(const Symbol& sym) {
        auto it = books_.find(sym.as_key());
        return (it != books_.end()) ? &it->second : nullptr;
    }

private:
    OrderBook& get_or_create_book(const Symbol& sym) {
        auto key = sym.as_key();
        auto it = books_.find(key);
        if (it == books_.end()) {
            it = books_.emplace(key, OrderBook(sym)).first;
        }
        return it->second;
    }

    OrderStore store_;
    std::unordered_map<uint64_t, OrderBook> books_;
};

}  // namespace mdfh
```

**Step 5: Update CMakeLists.txt — add order book test**

```cmake
add_executable(run_tests
    tests/test_protocol.cpp
    tests/test_encoder.cpp
    tests/test_spsc_queue.cpp
    tests/test_order_book.cpp
)
```

**Step 6: Build and run tests**

```bash
cmake --build build
./build/run_tests.exe
```

Expected: `[  PASSED  ] 35 tests` (7 + 8 + 7 + 13)

**Step 7: Commit**

```bash
git add -A
git commit -m "feat: order store and order book with full CRUD and depth query tests"
```

---

## Task 6: Multicast Networking — Sender + Receiver

**Files:**
- Create: `include/mdfh/network/multicast_sender.hpp`
- Create: `include/mdfh/network/multicast_receiver.hpp`

**Step 1: Implement `include/mdfh/network/multicast_sender.hpp`**

```cpp
#pragma once

#include <boost/asio.hpp>
#include <string>
#include <cstdint>

namespace mdfh::network {

namespace asio = boost::asio;
using udp = asio::ip::udp;

class MulticastSender {
public:
    MulticastSender(asio::io_context& io_ctx,
                    const std::string& multicast_addr,
                    uint16_t port)
        : socket_(io_ctx, udp::endpoint(udp::v4(), 0))
        , endpoint_(asio::ip::make_address(multicast_addr), port) {}

    // Send a datagram (synchronous, used by simulator)
    size_t send(const uint8_t* data, size_t length) {
        return socket_.send_to(asio::buffer(data, length), endpoint_);
    }

private:
    udp::socket socket_;
    udp::endpoint endpoint_;
};

}  // namespace mdfh::network
```

**Step 2: Implement `include/mdfh/network/multicast_receiver.hpp`**

```cpp
#pragma once

#include "mdfh/common/clock.hpp"
#include <boost/asio.hpp>
#include <functional>
#include <string>
#include <cstdint>
#include <array>

namespace mdfh::network {

namespace asio = boost::asio;
using udp = asio::ip::udp;

struct RawPacket {
    static constexpr size_t MAX_SIZE = 1500;

    std::array<uint8_t, MAX_SIZE> data;
    size_t   length;
    uint64_t receive_timestamp;  // nanoseconds, for latency measurement
};

class MulticastReceiver {
public:
    using PacketCallback = std::function<void(const RawPacket&)>;

    MulticastReceiver(asio::io_context& io_ctx,
                      const std::string& listen_addr,
                      const std::string& multicast_addr,
                      uint16_t port)
        : socket_(io_ctx)
        , callback_(nullptr) {
        // Open socket
        udp::endpoint listen_endpoint(asio::ip::make_address(listen_addr), port);
        socket_.open(listen_endpoint.protocol());
        socket_.set_option(asio::socket_base::reuse_address(true));
        socket_.bind(listen_endpoint);

        // Join multicast group
        socket_.set_option(asio::ip::multicast::join_group(
            asio::ip::make_address(multicast_addr)));
    }

    void set_callback(PacketCallback cb) {
        callback_ = std::move(cb);
    }

    void start() {
        do_receive();
    }

    void stop() {
        socket_.close();
    }

private:
    void do_receive() {
        socket_.async_receive_from(
            asio::buffer(recv_packet_.data),
            sender_endpoint_,
            [this](const boost::system::error_code& ec, size_t bytes_received) {
                if (!ec && bytes_received > 0) {
                    recv_packet_.length = bytes_received;
                    recv_packet_.receive_timestamp = Clock::now_ns();

                    if (callback_) {
                        callback_(recv_packet_);
                    }
                }
                if (socket_.is_open()) {
                    do_receive();
                }
            });
    }

    udp::socket socket_;
    udp::endpoint sender_endpoint_;
    RawPacket recv_packet_;
    PacketCallback callback_;
};

}  // namespace mdfh::network
```

**Step 3: Verify it compiles — update both executables in CMakeLists.txt**

Add to both `feed_handler` and `exchange_simulator` link lines:
```cmake
target_link_libraries(feed_handler PRIVATE Boost::system ws2_32)
target_link_libraries(exchange_simulator PRIVATE Boost::system nlohmann_json::nlohmann_json ws2_32)
```

Include the headers in the existing mains temporarily to verify compilation:

```bash
cmake --build build
```

Expected: compiles clean.

**Step 4: Commit**

```bash
git add -A
git commit -m "feat: multicast sender and receiver with Boost.Asio UDP"
```

---

## Task 7: Pipeline Stages — The 4 Feed Handler Threads

**Files:**
- Create: `src/handler/network_stage.hpp` + `src/handler/network_stage.cpp`
- Create: `src/handler/parser_stage.hpp` + `src/handler/parser_stage.cpp`
- Create: `src/handler/book_stage.hpp` + `src/handler/book_stage.cpp`
- Create: `src/handler/consumer_stage.hpp` + `src/handler/consumer_stage.cpp`

**Step 1: Define the pipeline data types**

Create `include/mdfh/core/pipeline_types.hpp`:
```cpp
#pragma once

#include "mdfh/common/types.hpp"
#include "mdfh/protocol/messages.hpp"
#include "mdfh/network/multicast_receiver.hpp"
#include <array>
#include <cstdint>

namespace mdfh {

// Q2: Parser → Book Manager
struct TimestampedMessage {
    protocol::ParsedMessage message;
    uint64_t receive_timestamp;  // from network stage
    Timestamp protocol_timestamp; // from message header
};

// Q3: Book Manager → Consumer
struct BookUpdate {
    Symbol   symbol;
    Price    best_bid;
    Quantity best_bid_qty;
    Price    best_ask;
    Quantity best_ask_qty;
    uint64_t receive_timestamp;  // original packet arrival time
    uint64_t book_update_timestamp;  // when book was updated
};

}  // namespace mdfh
```

**Step 2: Implement network stage**

`src/handler/network_stage.hpp`:
```cpp
#pragma once

#include "mdfh/network/multicast_receiver.hpp"
#include "mdfh/core/spsc_queue.hpp"
#include <string>
#include <thread>
#include <atomic>

namespace mdfh {

class NetworkStage {
public:
    using OutputQueue = SPSCQueue<network::RawPacket, 65536>;

    NetworkStage(const std::string& listen_addr,
                 const std::string& multicast_addr,
                 uint16_t port,
                 OutputQueue& output_queue);

    void start();
    void stop();

private:
    void run();

    std::string listen_addr_;
    std::string multicast_addr_;
    uint16_t port_;
    OutputQueue& output_queue_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace mdfh
```

`src/handler/network_stage.cpp`:
```cpp
#include "network_stage.hpp"
#include <iostream>

namespace mdfh {

NetworkStage::NetworkStage(const std::string& listen_addr,
                           const std::string& multicast_addr,
                           uint16_t port,
                           OutputQueue& output_queue)
    : listen_addr_(listen_addr)
    , multicast_addr_(multicast_addr)
    , port_(port)
    , output_queue_(output_queue) {}

void NetworkStage::start() {
    running_ = true;
    thread_ = std::thread(&NetworkStage::run, this);
}

void NetworkStage::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void NetworkStage::run() {
    boost::asio::io_context io_ctx;
    network::MulticastReceiver receiver(io_ctx, listen_addr_, multicast_addr_, port_);

    receiver.set_callback([this](const network::RawPacket& packet) {
        // Spin-push into queue — drop if full (production would log drops)
        output_queue_.try_push(packet);
    });

    receiver.start();

    while (running_) {
        io_ctx.run_for(std::chrono::milliseconds(100));
        io_ctx.restart();
    }

    receiver.stop();
}

}  // namespace mdfh
```

**Step 3: Implement parser stage**

`src/handler/parser_stage.hpp`:
```cpp
#pragma once

#include "mdfh/core/spsc_queue.hpp"
#include "mdfh/core/pipeline_types.hpp"
#include "mdfh/network/multicast_receiver.hpp"
#include <thread>
#include <atomic>

namespace mdfh {

class ParserStage {
public:
    using InputQueue = SPSCQueue<network::RawPacket, 65536>;
    using OutputQueue = SPSCQueue<TimestampedMessage, 65536>;

    ParserStage(InputQueue& input, OutputQueue& output);

    void start();
    void stop();

private:
    void run();

    InputQueue& input_;
    OutputQueue& output_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace mdfh
```

`src/handler/parser_stage.cpp`:
```cpp
#include "parser_stage.hpp"
#include "mdfh/protocol/encoder.hpp"
#include <cstring>

namespace mdfh {

ParserStage::ParserStage(InputQueue& input, OutputQueue& output)
    : input_(input), output_(output) {}

void ParserStage::start() {
    running_ = true;
    thread_ = std::thread(&ParserStage::run, this);
}

void ParserStage::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void ParserStage::run() {
    network::RawPacket packet;

    while (running_) {
        if (!input_.try_pop(packet)) {
            // Brief yield to avoid burning CPU when idle
            std::this_thread::yield();
            continue;
        }

        // Walk through batched messages in the datagram
        size_t offset = 0;
        while (offset + sizeof(protocol::MessageHeader) <= packet.length) {
            auto header = protocol::Encoder::decode_header(packet.data.data() + offset);

            if (header.message_length == 0 || offset + header.message_length > packet.length) {
                break;  // malformed
            }

            TimestampedMessage tmsg;
            tmsg.message = protocol::Encoder::parse(packet.data.data() + offset);
            tmsg.receive_timestamp = packet.receive_timestamp;
            tmsg.protocol_timestamp = header.timestamp;

            output_.try_push(tmsg);

            offset += header.message_length;
        }
    }
}

}  // namespace mdfh
```

**Step 4: Implement book stage**

`src/handler/book_stage.hpp`:
```cpp
#pragma once

#include "mdfh/core/spsc_queue.hpp"
#include "mdfh/core/pipeline_types.hpp"
#include "mdfh/core/order_book.hpp"
#include <thread>
#include <atomic>

namespace mdfh {

class BookStage {
public:
    using InputQueue = SPSCQueue<TimestampedMessage, 65536>;
    using OutputQueue = SPSCQueue<BookUpdate, 16384>;

    BookStage(InputQueue& input, OutputQueue& output);

    void start();
    void stop();

    // For stats: total messages processed
    uint64_t messages_processed() const { return messages_processed_.load(std::memory_order_relaxed); }

private:
    void run();
    void process_message(const TimestampedMessage& tmsg);
    void emit_book_update(const Symbol& sym, uint64_t receive_ts);

    InputQueue& input_;
    OutputQueue& output_;
    OrderBookManager book_manager_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> messages_processed_{0};
    std::thread thread_;
};

}  // namespace mdfh
```

`src/handler/book_stage.cpp`:
```cpp
#include "book_stage.hpp"
#include "mdfh/common/clock.hpp"

namespace mdfh {

BookStage::BookStage(InputQueue& input, OutputQueue& output)
    : input_(input), output_(output) {}

void BookStage::start() {
    running_ = true;
    thread_ = std::thread(&BookStage::run, this);
}

void BookStage::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void BookStage::run() {
    TimestampedMessage tmsg;

    while (running_) {
        if (!input_.try_pop(tmsg)) {
            std::this_thread::yield();
            continue;
        }

        process_message(tmsg);
        messages_processed_.fetch_add(1, std::memory_order_relaxed);
    }
}

void BookStage::process_message(const TimestampedMessage& tmsg) {
    std::visit([&](auto&& msg) {
        using T = std::decay_t<decltype(msg)>;

        if constexpr (std::is_same_v<T, protocol::AddOrder>) {
            book_manager_.add_order(
                msg.order_id,
                static_cast<Side>(msg.side),
                msg.symbol,
                msg.price,
                msg.quantity
            );
            emit_book_update(msg.symbol, tmsg.receive_timestamp);

        } else if constexpr (std::is_same_v<T, protocol::CancelOrder>) {
            // Need to find the symbol before cancel erases the order
            auto* store = book_manager_.get_book(Symbol{});  // handled inside
            book_manager_.cancel_order(msg.order_id);

        } else if constexpr (std::is_same_v<T, protocol::ExecuteOrder>) {
            book_manager_.execute_order(msg.order_id, msg.exec_quantity);

        } else if constexpr (std::is_same_v<T, protocol::ReplaceOrder>) {
            book_manager_.replace_order(msg.order_id, msg.new_price, msg.new_quantity);

        } else if constexpr (std::is_same_v<T, protocol::TradeMessage>) {
            // Trade messages are informational — no book update needed
            // Could emit trade event to consumer if desired
        }
    }, tmsg.message);
}

void BookStage::emit_book_update(const Symbol& sym, uint64_t receive_ts) {
    auto* book = book_manager_.get_book(sym);
    if (!book) return;

    BookUpdate update;
    update.symbol = sym;
    update.best_bid = book->best_bid_price();
    update.best_bid_qty = book->best_bid_quantity();
    update.best_ask = book->best_ask_price();
    update.best_ask_qty = book->best_ask_quantity();
    update.receive_timestamp = receive_ts;
    update.book_update_timestamp = Clock::now_ns();

    output_.try_push(update);
}

}  // namespace mdfh
```

**Step 5: Implement consumer stage**

`include/mdfh/consumer/stats_collector.hpp`:
```cpp
#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>
#include <numeric>

namespace mdfh {

class StatsCollector {
public:
    StatsCollector() { latencies_.reserve(1000000); }

    void record_latency(uint64_t latency_ns) {
        latencies_.push_back(latency_ns);
    }

    void record_message() { message_count_++; }
    void record_book_update() { update_count_++; }

    // Call once per reporting interval, returns stats and resets
    struct Stats {
        uint64_t p50_ns = 0;
        uint64_t p95_ns = 0;
        uint64_t p99_ns = 0;
        uint64_t p999_ns = 0;
        uint64_t messages = 0;
        uint64_t updates = 0;
    };

    Stats snapshot_and_reset() {
        Stats s;
        s.messages = message_count_;
        s.updates = update_count_;

        if (!latencies_.empty()) {
            std::sort(latencies_.begin(), latencies_.end());
            size_t n = latencies_.size();
            s.p50_ns  = latencies_[n * 50 / 100];
            s.p95_ns  = latencies_[n * 95 / 100];
            s.p99_ns  = latencies_[n * 99 / 100];
            s.p999_ns = latencies_[std::min(n - 1, n * 999 / 1000)];
        }

        latencies_.clear();
        message_count_ = 0;
        update_count_ = 0;
        return s;
    }

private:
    std::vector<uint64_t> latencies_;
    uint64_t message_count_ = 0;
    uint64_t update_count_ = 0;
};

}  // namespace mdfh
```

`include/mdfh/consumer/console_display.hpp`:
```cpp
#pragma once

#include "mdfh/common/types.hpp"
#include "mdfh/consumer/stats_collector.hpp"
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace mdfh {

struct DisplayBook {
    Symbol symbol;
    Price  best_bid = 0;
    Quantity bid_qty = 0;
    Price  best_ask = 0;
    Quantity ask_qty = 0;
};

class ConsoleDisplay {
public:
    void update_book(const Symbol& sym, Price bid, Quantity bid_qty,
                     Price ask, Quantity ask_qty) {
        auto key = sym.as_key();
        auto& db = books_[key];
        db.symbol = sym;
        db.best_bid = bid;
        db.bid_qty = bid_qty;
        db.best_ask = ask;
        db.ask_qty = ask_qty;
    }

    void render(const StatsCollector::Stats& stats) {
        // Clear screen (cross-platform enough for our purposes)
        std::cout << "\033[2J\033[H";

        std::cout << std::string(70, '=') << "\n";
        std::cout << "  MARKET DATA FEED HANDLER — LIVE\n";
        std::cout << std::string(70, '-') << "\n";

        std::cout << std::fixed << std::setprecision(4);

        for (auto& [key, db] : books_) {
            std::string sym(db.symbol.data, strnlen(db.symbol.data, 8));
            std::cout << "  " << std::left << std::setw(8) << sym
                      << "  BID: " << std::setw(10) << price_to_double(db.best_bid)
                      << " x " << std::setw(6) << db.bid_qty
                      << "  |  ASK: " << std::setw(10) << price_to_double(db.best_ask)
                      << " x " << std::setw(6) << db.ask_qty
                      << "\n";
        }

        std::cout << std::string(70, '-') << "\n";
        std::cout << "  Msgs/sec: " << stats.messages
                  << " | Updates/sec: " << stats.updates
                  << "\n";
        std::cout << "  Latency — p50: " << stats.p50_ns / 1000.0 << "us"
                  << " | p95: " << stats.p95_ns / 1000.0 << "us"
                  << " | p99: " << stats.p99_ns / 1000.0 << "us"
                  << " | p999: " << stats.p999_ns / 1000.0 << "us"
                  << "\n";
        std::cout << std::string(70, '=') << "\n";
        std::cout << std::flush;
    }

private:
    std::unordered_map<uint64_t, DisplayBook> books_;
};

}  // namespace mdfh
```

`src/handler/consumer_stage.hpp`:
```cpp
#pragma once

#include "mdfh/core/spsc_queue.hpp"
#include "mdfh/core/pipeline_types.hpp"
#include "mdfh/consumer/stats_collector.hpp"
#include "mdfh/consumer/console_display.hpp"
#include <thread>
#include <atomic>

namespace mdfh {

class ConsumerStage {
public:
    using InputQueue = SPSCQueue<BookUpdate, 16384>;

    explicit ConsumerStage(InputQueue& input, bool show_display = true);

    void start();
    void stop();

private:
    void run();

    InputQueue& input_;
    StatsCollector stats_;
    ConsoleDisplay display_;
    bool show_display_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace mdfh
```

`src/handler/consumer_stage.cpp`:
```cpp
#include "consumer_stage.hpp"
#include "mdfh/common/clock.hpp"
#include <chrono>

namespace mdfh {

ConsumerStage::ConsumerStage(InputQueue& input, bool show_display)
    : input_(input), show_display_(show_display) {}

void ConsumerStage::start() {
    running_ = true;
    thread_ = std::thread(&ConsumerStage::run, this);
}

void ConsumerStage::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void ConsumerStage::run() {
    using SteadyClock = std::chrono::steady_clock;
    auto last_report = SteadyClock::now();
    constexpr auto report_interval = std::chrono::seconds(1);

    BookUpdate update;

    while (running_) {
        if (input_.try_pop(update)) {
            uint64_t latency = update.book_update_timestamp - update.receive_timestamp;
            stats_.record_latency(latency);
            stats_.record_book_update();

            if (show_display_) {
                display_.update_book(update.symbol,
                                     update.best_bid, update.best_bid_qty,
                                     update.best_ask, update.best_ask_qty);
            }
        } else {
            std::this_thread::yield();
        }

        // Periodic reporting
        auto now = SteadyClock::now();
        if (now - last_report >= report_interval) {
            auto snapshot = stats_.snapshot_and_reset();
            if (show_display_) {
                display_.render(snapshot);
            }
            last_report = now;
        }
    }
}

}  // namespace mdfh
```

**Step 6: Update CMakeLists.txt — add stage sources to feed_handler**

```cmake
add_executable(feed_handler
    src/handler/main.cpp
    src/handler/network_stage.cpp
    src/handler/parser_stage.cpp
    src/handler/book_stage.cpp
    src/handler/consumer_stage.cpp
)
target_link_libraries(feed_handler PRIVATE Boost::system ws2_32)
```

**Step 7: Build**

```bash
cmake --build build
```

Expected: compiles clean.

**Step 8: Commit**

```bash
git add -A
git commit -m "feat: 4-stage feed handler pipeline — network, parser, book, consumer"
```

---

## Task 8: Exchange Simulator

**Files:**
- Create: `src/simulator/market_simulator.hpp`
- Create: `src/simulator/market_simulator.cpp`
- Modify: `src/simulator/main.cpp`

**Step 1: Implement `src/simulator/market_simulator.hpp`**

```cpp
#pragma once

#include "mdfh/common/types.hpp"
#include "mdfh/protocol/messages.hpp"
#include "mdfh/protocol/encoder.hpp"
#include "mdfh/network/multicast_sender.hpp"
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <set>

namespace mdfh {

struct SimConfig {
    std::string multicast_address;
    uint16_t port;
    std::vector<std::string> symbols;
    uint32_t messages_per_second;
    uint32_t duration_seconds;
    uint32_t seed;
    std::unordered_map<std::string, Price> initial_prices;
};

class MarketSimulator {
public:
    explicit MarketSimulator(const SimConfig& config);

    // Run the simulation — blocks for duration_seconds
    void run(network::MulticastSender& sender);

private:
    // Internal order tracking (so cancels/executes reference valid IDs)
    struct SimOrder {
        OrderId  id;
        Side     side;
        Symbol   symbol;
        Price    price;
        Quantity quantity;
    };

    void generate_add_order(uint8_t* buffer, size_t& offset, size_t max_size);
    void generate_cancel_order(uint8_t* buffer, size_t& offset, size_t max_size);
    void generate_execute_order(uint8_t* buffer, size_t& offset, size_t max_size);
    void generate_replace_order(uint8_t* buffer, size_t& offset, size_t max_size);
    void generate_trade_message(uint8_t* buffer, size_t& offset, size_t max_size);

    Symbol pick_random_symbol();
    Price jitter_price(Price base);
    OrderId pick_random_active_order();

    SimConfig config_;
    std::mt19937 rng_;
    OrderId next_order_id_ = 1;
    std::unordered_map<OrderId, SimOrder> active_orders_;
    std::vector<OrderId> active_order_ids_;  // for random selection
    std::unordered_map<uint64_t, Price> current_prices_;  // per symbol
};

}  // namespace mdfh
```

**Step 2: Implement `src/simulator/market_simulator.cpp`**

```cpp
#include "market_simulator.hpp"
#include "mdfh/common/clock.hpp"
#include <chrono>
#include <thread>
#include <iostream>
#include <algorithm>

namespace mdfh {

MarketSimulator::MarketSimulator(const SimConfig& config)
    : config_(config), rng_(config.seed) {
    // Initialize current prices
    for (auto& [sym_str, price] : config_.initial_prices) {
        Symbol sym(sym_str.c_str());
        current_prices_[sym.as_key()] = price;
    }
}

void MarketSimulator::run(network::MulticastSender& sender) {
    const auto start = std::chrono::steady_clock::now();
    const auto duration = std::chrono::seconds(config_.duration_seconds);
    const auto interval_ns = std::chrono::nanoseconds(1'000'000'000ULL / config_.messages_per_second);

    uint64_t total_sent = 0;
    uint8_t buffer[1400];

    std::cout << "[SIM] Starting simulation: " << config_.messages_per_second
              << " msg/sec for " << config_.duration_seconds << "s\n";

    // Message type weights: 40% Add, 25% Cancel, 20% Execute, 10% Replace, 5% Trade
    std::discrete_distribution<int> type_dist({40, 25, 20, 10, 5});

    auto next_send = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start < duration) {
        size_t offset = 0;

        // Batch messages into one datagram
        while (offset + 64 < sizeof(buffer)) {  // leave room for largest message
            int msg_type = type_dist(rng_);

            switch (msg_type) {
                case 0: generate_add_order(buffer, offset, sizeof(buffer)); break;
                case 1: generate_cancel_order(buffer, offset, sizeof(buffer)); break;
                case 2: generate_execute_order(buffer, offset, sizeof(buffer)); break;
                case 3: generate_replace_order(buffer, offset, sizeof(buffer)); break;
                case 4: generate_trade_message(buffer, offset, sizeof(buffer)); break;
            }
            total_sent++;

            // Check if we need to send based on rate
            next_send += interval_ns;
            if (std::chrono::steady_clock::now() < next_send) break;
        }

        if (offset > 0) {
            sender.send(buffer, offset);
        }

        // Throttle
        auto now = std::chrono::steady_clock::now();
        if (next_send > now) {
            std::this_thread::sleep_until(next_send);
        }
    }

    std::cout << "[SIM] Simulation complete. Total messages sent: " << total_sent << "\n";
}

void MarketSimulator::generate_add_order(uint8_t* buffer, size_t& offset, size_t max_size) {
    protocol::AddOrder msg{};
    msg.order_id = next_order_id_++;
    msg.symbol = pick_random_symbol();

    std::uniform_int_distribution<int> side_dist(0, 1);
    msg.side = side_dist(rng_) ? static_cast<uint8_t>(Side::Buy) : static_cast<uint8_t>(Side::Sell);

    Price base = current_prices_[msg.symbol.as_key()];
    msg.price = jitter_price(base);

    std::uniform_int_distribution<uint32_t> qty_dist(10, 1000);
    msg.quantity = qty_dist(rng_);

    size_t written = protocol::Encoder::encode(msg, Clock::nanos_since_midnight(),
                                                buffer + offset, max_size - offset);
    if (written > 0) {
        offset += written;

        // Track this order
        SimOrder so{msg.order_id, static_cast<Side>(msg.side), msg.symbol, msg.price, msg.quantity};
        active_orders_[msg.order_id] = so;
        active_order_ids_.push_back(msg.order_id);
    }
}

void MarketSimulator::generate_cancel_order(uint8_t* buffer, size_t& offset, size_t max_size) {
    OrderId id = pick_random_active_order();
    if (id == 0) {
        // No active orders — generate an add instead
        generate_add_order(buffer, offset, max_size);
        return;
    }

    protocol::CancelOrder msg{};
    msg.order_id = id;

    size_t written = protocol::Encoder::encode(msg, Clock::nanos_since_midnight(),
                                                buffer + offset, max_size - offset);
    if (written > 0) {
        offset += written;

        // Remove from tracking
        active_orders_.erase(id);
        active_order_ids_.erase(
            std::remove(active_order_ids_.begin(), active_order_ids_.end(), id),
            active_order_ids_.end());
    }
}

void MarketSimulator::generate_execute_order(uint8_t* buffer, size_t& offset, size_t max_size) {
    OrderId id = pick_random_active_order();
    if (id == 0) {
        generate_add_order(buffer, offset, max_size);
        return;
    }

    auto it = active_orders_.find(id);
    if (it == active_orders_.end()) return;

    protocol::ExecuteOrder msg{};
    msg.order_id = id;

    std::uniform_int_distribution<uint32_t> fill_dist(1, it->second.quantity);
    msg.exec_quantity = fill_dist(rng_);

    size_t written = protocol::Encoder::encode(msg, Clock::nanos_since_midnight(),
                                                buffer + offset, max_size - offset);
    if (written > 0) {
        offset += written;

        it->second.quantity -= msg.exec_quantity;
        if (it->second.quantity == 0) {
            active_order_ids_.erase(
                std::remove(active_order_ids_.begin(), active_order_ids_.end(), id),
                active_order_ids_.end());
            active_orders_.erase(it);
        }
    }
}

void MarketSimulator::generate_replace_order(uint8_t* buffer, size_t& offset, size_t max_size) {
    OrderId id = pick_random_active_order();
    if (id == 0) {
        generate_add_order(buffer, offset, max_size);
        return;
    }

    auto it = active_orders_.find(id);
    if (it == active_orders_.end()) return;

    protocol::ReplaceOrder msg{};
    msg.order_id = id;
    msg.new_price = jitter_price(it->second.price);

    std::uniform_int_distribution<uint32_t> qty_dist(10, 1000);
    msg.new_quantity = qty_dist(rng_);

    size_t written = protocol::Encoder::encode(msg, Clock::nanos_since_midnight(),
                                                buffer + offset, max_size - offset);
    if (written > 0) {
        offset += written;
        it->second.price = msg.new_price;
        it->second.quantity = msg.new_quantity;
    }
}

void MarketSimulator::generate_trade_message(uint8_t* buffer, size_t& offset, size_t max_size) {
    protocol::TradeMessage msg{};
    msg.symbol = pick_random_symbol();

    Price base = current_prices_[msg.symbol.as_key()];
    msg.price = base;  // trades at mid

    std::uniform_int_distribution<uint32_t> qty_dist(10, 500);
    msg.quantity = qty_dist(rng_);

    msg.buy_order_id = next_order_id_++;
    msg.sell_order_id = next_order_id_++;

    size_t written = protocol::Encoder::encode(msg, Clock::nanos_since_midnight(),
                                                buffer + offset, max_size - offset);
    if (written > 0) {
        offset += written;
        // Walk the price slightly
        std::uniform_int_distribution<int> walk(-50, 50);
        current_prices_[msg.symbol.as_key()] = base + walk(rng_);
    }
}

Symbol MarketSimulator::pick_random_symbol() {
    std::uniform_int_distribution<size_t> dist(0, config_.symbols.size() - 1);
    return Symbol(config_.symbols[dist(rng_)].c_str());
}

Price MarketSimulator::jitter_price(Price base) {
    // +/- 0.50 (5000 in fixed-point)
    std::uniform_int_distribution<int> jitter(-5000, 5000);
    int result = static_cast<int>(base) + jitter(rng_);
    return static_cast<Price>(std::max(1, result));
}

OrderId MarketSimulator::pick_random_active_order() {
    if (active_order_ids_.empty()) return 0;
    std::uniform_int_distribution<size_t> dist(0, active_order_ids_.size() - 1);
    return active_order_ids_[dist(rng_)];
}

}  // namespace mdfh
```

**Step 3: Update `src/simulator/main.cpp`**

```cpp
#include "market_simulator.hpp"
#include "mdfh/network/multicast_sender.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <string>

using json = nlohmann::json;

mdfh::SimConfig load_config(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open config: " + path);
    }

    json j;
    f >> j;

    mdfh::SimConfig config;
    config.multicast_address = j["multicast_address"];
    config.port = j["port"];
    config.messages_per_second = j["messages_per_second"];
    config.duration_seconds = j["duration_seconds"];
    config.seed = j["seed"];

    for (auto& sym : j["symbols"]) {
        config.symbols.push_back(sym.get<std::string>());
    }

    for (auto& [sym, price] : j["initial_prices"].items()) {
        config.initial_prices[sym] = price.get<mdfh::Price>();
    }

    return config;
}

int main(int argc, char* argv[]) {
    std::string config_path = "config/scenario.json";
    if (argc > 1) config_path = argv[1];

    try {
        auto config = load_config(config_path);

        std::cout << "[SIM] Exchange Simulator v0.1\n";
        std::cout << "[SIM] Config: " << config.symbols.size() << " symbols, "
                  << config.messages_per_second << " msg/sec, "
                  << config.duration_seconds << "s\n";
        std::cout << "[SIM] Broadcasting on " << config.multicast_address
                  << ":" << config.port << "\n";

        boost::asio::io_context io_ctx;
        mdfh::network::MulticastSender sender(io_ctx, config.multicast_address, config.port);

        mdfh::MarketSimulator simulator(config);
        simulator.run(sender);

    } catch (const std::exception& e) {
        std::cerr << "[SIM] Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

**Step 4: Update CMakeLists.txt — add simulator sources**

```cmake
add_executable(exchange_simulator
    src/simulator/main.cpp
    src/simulator/market_simulator.cpp
)
target_link_libraries(exchange_simulator PRIVATE Boost::system nlohmann_json::nlohmann_json ws2_32)
```

**Step 5: Build**

```bash
cmake --build build
```

Expected: compiles clean.

**Step 6: Commit**

```bash
git add -A
git commit -m "feat: exchange simulator with weighted random order generation"
```

---

## Task 9: Feed Handler Main — Wire the Pipeline

**Files:**
- Modify: `src/handler/main.cpp`

**Step 1: Implement the full feed handler main**

```cpp
#include "network_stage.hpp"
#include "parser_stage.hpp"
#include "book_stage.hpp"
#include "consumer_stage.hpp"
#include "mdfh/core/spsc_queue.hpp"
#include "mdfh/core/pipeline_types.hpp"
#include "mdfh/network/multicast_receiver.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <string>

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::string multicast_addr = "239.1.1.1";
    uint16_t port = 12345;
    std::string listen_addr = "0.0.0.0";
    bool show_display = true;

    // Simple arg parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--group" && i + 1 < argc) multicast_addr = argv[++i];
        else if (arg == "--port" && i + 1 < argc) port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (arg == "--listen" && i + 1 < argc) listen_addr = argv[++i];
        else if (arg == "--no-display") show_display = false;
    }

    std::signal(SIGINT, signal_handler);

    std::cout << "[FEED] Market Data Feed Handler v0.1\n";
    std::cout << "[FEED] Joining " << multicast_addr << ":" << port << "\n";
    std::cout << "[FEED] Pipeline: 4 threads (network → parser → book → consumer)\n";
    std::cout << "[FEED] Press Ctrl+C to stop\n\n";

    // Create queues
    static mdfh::SPSCQueue<mdfh::network::RawPacket, 65536> q1;  // network → parser
    static mdfh::SPSCQueue<mdfh::TimestampedMessage, 65536> q2;  // parser → book
    static mdfh::SPSCQueue<mdfh::BookUpdate, 16384> q3;          // book → consumer

    // Create stages
    mdfh::NetworkStage  network_stage(listen_addr, multicast_addr, port, q1);
    mdfh::ParserStage   parser_stage(q1, q2);
    mdfh::BookStage     book_stage(q2, q3);
    mdfh::ConsumerStage consumer_stage(q3, show_display);

    // Start pipeline (reverse order — consumer first, network last)
    consumer_stage.start();
    book_stage.start();
    parser_stage.start();
    network_stage.start();

    // Wait for signal
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n[FEED] Shutting down...\n";

    // Stop pipeline (forward order — network first, consumer last)
    network_stage.stop();
    parser_stage.stop();
    book_stage.stop();
    consumer_stage.stop();

    std::cout << "[FEED] Total messages processed: " << book_stage.messages_processed() << "\n";
    std::cout << "[FEED] Done.\n";

    return 0;
}
```

**Step 2: Build both executables**

```bash
cmake --build build
```

Expected: both `feed_handler.exe` and `exchange_simulator.exe` compile clean.

**Step 3: Smoke test — run them**

Terminal 1:
```bash
./build/feed_handler.exe --group 239.1.1.1 --port 12345
```

Terminal 2:
```bash
./build/exchange_simulator.exe config/scenario.json
```

Expected: feed handler displays live book updates, stats refresh every second.

**Step 4: Commit**

```bash
git add -A
git commit -m "feat: wire feed handler pipeline — full end-to-end system working"
```

---

## Task 10: Benchmark Binary

**Files:**
- Create: `bench/bench_pipeline.cpp`

**Step 1: Implement the benchmark**

`bench/bench_pipeline.cpp`:
```cpp
#include "mdfh/core/spsc_queue.hpp"
#include "mdfh/core/order_book.hpp"
#include "mdfh/protocol/messages.hpp"
#include "mdfh/protocol/encoder.hpp"
#include "mdfh/common/clock.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace mdfh;

void bench_spsc_queue() {
    constexpr int COUNT = 10'000'000;
    SPSCQueue<int, 65536> q;

    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&]() {
        for (int i = 0; i < COUNT; ++i)
            while (!q.try_push(i)) {}
    });

    std::thread consumer([&]() {
        int v;
        for (int i = 0; i < COUNT; ++i)
            while (!q.try_pop(v)) {}
    });

    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << "SPSC Queue: " << COUNT << " messages in " << ns / 1'000'000 << "ms"
              << " (" << ns / COUNT << " ns/msg, "
              << static_cast<double>(COUNT) * 1e9 / ns / 1e6 << "M msg/sec)\n";
}

void bench_encoder() {
    constexpr int COUNT = 10'000'000;
    uint8_t buffer[64];
    protocol::AddOrder msg{};
    msg.order_id = 1;
    msg.side = static_cast<uint8_t>(Side::Buy);
    msg.symbol = Symbol("AAPL");
    msg.price = 1850000;
    msg.quantity = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < COUNT; ++i) {
        protocol::Encoder::encode(msg, 0, buffer, sizeof(buffer));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << "Encoder: " << COUNT << " AddOrder encodes in " << ns / 1'000'000 << "ms"
              << " (" << ns / COUNT << " ns/encode, "
              << static_cast<double>(COUNT) * 1e9 / ns / 1e6 << "M encodes/sec)\n";
}

void bench_order_book() {
    constexpr int COUNT = 1'000'000;
    OrderBookManager manager;
    Symbol aapl("AAPL");

    // Pre-generate messages
    std::vector<uint64_t> latencies;
    latencies.reserve(COUNT);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < COUNT; ++i) {
        auto t0 = Clock::now_ns();
        manager.add_order(i + 1, Side::Buy, aapl,
                          1850000 + (i % 100) * 100,
                          100);
        auto t1 = Clock::now_ns();
        latencies.push_back(t1 - t0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();

    std::cout << "OrderBook: " << COUNT << " AddOrder ops in " << ns / 1'000'000 << "ms"
              << " (" << ns / COUNT << " ns/op, "
              << static_cast<double>(COUNT) * 1e9 / ns / 1e6 << "M ops/sec)\n";
    std::cout << "  Latency — p50: " << latencies[n * 50 / 100] << "ns"
              << " | p99: " << latencies[n * 99 / 100] << "ns"
              << " | p999: " << latencies[n * 999 / 1000] << "ns\n";
}

int main() {
    std::cout << "=== Market Data Feed Handler Benchmarks ===\n\n";
    bench_spsc_queue();
    std::cout << "\n";
    bench_encoder();
    std::cout << "\n";
    bench_order_book();
    std::cout << "\n=== Done ===\n";
    return 0;
}
```

**Step 2: Add to CMakeLists.txt**

```cmake
# --- benchmark ---
add_executable(bench_pipeline
    bench/bench_pipeline.cpp
)
target_link_libraries(bench_pipeline PRIVATE Boost::system ws2_32)
```

**Step 3: Build and run**

```bash
cmake --build build
./build/bench_pipeline.exe
```

Expected output (numbers will vary):
```
=== Market Data Feed Handler Benchmarks ===

SPSC Queue: 10000000 messages in XXms (XX ns/msg, XX.XM msg/sec)

Encoder: 10000000 AddOrder encodes in XXms (XX ns/encode, XX.XM encodes/sec)

OrderBook: 1000000 AddOrder ops in XXms (XX ns/op, XX.XM ops/sec)
  Latency — p50: XXns | p99: XXns | p999: XXns

=== Done ===
```

**Step 4: Commit**

```bash
git add -A
git commit -m "feat: benchmark suite for queue, encoder, and order book performance"
```

---

## Task 11: Fix Book Stage Cancel/Execute/Replace — Emit Updates for All Types

**Files:**
- Modify: `src/handler/book_stage.cpp`
- Modify: `include/mdfh/core/order_book.hpp` (add method to look up order symbol)

The current `book_stage.cpp` only emits a `BookUpdate` for `AddOrder`. We need to emit for all message types, which means we need the symbol from the order store before cancel/execute erase it.

**Step 1: Add `find_order` to `OrderBookManager`**

Add to `OrderBookManager` in `order_book.hpp`:

```cpp
    // Look up an order's symbol (needed before cancel/execute to emit update)
    const Order* find_order(OrderId id) const {
        return store_.find(id);
    }
```

And make `OrderStore::find` const-overloaded:

```cpp
    const Order* find(OrderId id) const {
        auto it = orders_.find(id);
        return (it != orders_.end()) ? &it->second : nullptr;
    }
```

**Step 2: Fix `book_stage.cpp` process_message**

```cpp
void BookStage::process_message(const TimestampedMessage& tmsg) {
    std::visit([&](auto&& msg) {
        using T = std::decay_t<decltype(msg)>;

        if constexpr (std::is_same_v<T, protocol::AddOrder>) {
            Symbol sym = msg.symbol;
            book_manager_.add_order(msg.order_id, static_cast<Side>(msg.side),
                                    sym, msg.price, msg.quantity);
            emit_book_update(sym, tmsg.receive_timestamp);

        } else if constexpr (std::is_same_v<T, protocol::CancelOrder>) {
            const auto* order = book_manager_.find_order(msg.order_id);
            Symbol sym;
            if (order) sym = order->symbol;
            book_manager_.cancel_order(msg.order_id);
            if (order) emit_book_update(sym, tmsg.receive_timestamp);

        } else if constexpr (std::is_same_v<T, protocol::ExecuteOrder>) {
            const auto* order = book_manager_.find_order(msg.order_id);
            Symbol sym;
            if (order) sym = order->symbol;
            book_manager_.execute_order(msg.order_id, msg.exec_quantity);
            if (order) emit_book_update(sym, tmsg.receive_timestamp);

        } else if constexpr (std::is_same_v<T, protocol::ReplaceOrder>) {
            const auto* order = book_manager_.find_order(msg.order_id);
            Symbol sym;
            if (order) sym = order->symbol;
            book_manager_.replace_order(msg.order_id, msg.new_price, msg.new_quantity);
            if (order) emit_book_update(sym, tmsg.receive_timestamp);

        } else if constexpr (std::is_same_v<T, protocol::TradeMessage>) {
            // Informational only — no book update
        }
    }, tmsg.message);
}
```

**Step 3: Build and run tests**

```bash
cmake --build build
./build/run_tests.exe
```

Expected: all tests pass.

**Step 4: Commit**

```bash
git add -A
git commit -m "fix: emit book updates for cancel, execute, and replace messages"
```

---

## Task 12: README

**Files:**
- Create: `README.md`

**Step 1: Write the README**

A strong README with architecture diagram, build instructions, usage, and benchmark results. Write this AFTER running benchmarks so you can include real numbers.

Key sections:
1. One-line description
2. Architecture diagram (ASCII from design doc)
3. Protocol overview (MiniITCH table)
4. Build instructions
5. Usage (two terminals)
6. Benchmark results (paste from bench run)
7. Design decisions (interview talking points)

**Step 2: Commit**

```bash
git add README.md
git commit -m "docs: README with architecture, build instructions, and benchmarks"
```

---

## Build Order Summary

| Task | Component | Depends On | Est. Files |
|------|-----------|-----------|------------|
| 1 | CMake + types + clock + empty mains | — | 6 |
| 2 | Protocol message structs | Task 1 | 1 |
| 3 | Encoder (serialize/deserialize) | Task 2 | 2 |
| 4 | SPSC Queue | Task 1 | 1 |
| 5 | Order Store + Order Book | Task 1 | 2 |
| 6 | Multicast Sender + Receiver | Task 1 | 2 |
| 7 | Pipeline stages (4 threads) | Tasks 3-6 | 10 |
| 8 | Exchange Simulator | Tasks 3, 6 | 2 |
| 9 | Feed Handler main (wire pipeline) | Task 7 | 1 |
| 10 | Benchmark suite | Tasks 3-5 | 1 |
| 11 | Bug fix: emit updates for all msg types | Task 7 | 2 |
| 12 | README | All | 1 |

**Total: ~25 files, 12 commits, each step compiles and runs.**
