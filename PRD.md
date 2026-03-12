# PRD: QuantFlow — Full-Stack Algorithmic Trading Platform

## Introduction

Build a complete algorithmic trading platform — from raw market data to live dashboard — with trading signals, backtesting, risk management, and order execution. Two C++ executables (exchange simulator + feed handler) communicate over UDP multicast with a custom binary protocol ("MiniITCH"). A React/TypeScript dashboard visualizes everything in real time over WebSocket.

The system comprises 10 subsystems: Feed Handler, Exchange Simulator, Signal Engine, Strategy Engine, OMS/EMS, Risk Engine, Data Recorder, Backtest Engine, API Gateway, and Web Dashboard. Plus a FIX protocol stub and supporting infrastructure.

**Target:** ~107,000 lines | C++17 backend + React/TS frontend | Production-grade

**Architecture reference:** `docs/ARCHITECTURE_V2.md`
**Design doc:** `docs/plans/2026-02-24-feed-handler-design.md`
**Current state:** Project skeleton built (Task 1 done). Headers and stub .cpp files exist for feed handler + simulator. All implementations are TODO stubs.

## Goals

- Sub-10μs p99 latency from UDP packet arrival to order book update
- 500k+ messages/second throughput in the feed handler pipeline
- 4-thread lock-free pipeline (network → parser → book → consumer)
- 11 real-time features computed from order book data
- 5 built-in trading strategies (market making, momentum, mean reversion, stat arb, signal threshold)
- Full order lifecycle state machine with append-only journal
- Pre-trade and post-trade risk checks with circuit breaker
- Tick-level data recording and historical replay
- Backtest engine reusing live code with simulated fills
- Real-time React dashboard over WebSocket
- FIX 4.4 protocol stub for exchange connectivity
- Deterministic simulator with configurable seed for reproducible benchmarks
- Comprehensive test suite (unit, integration, stress) and benchmarks

---

## User Stories

---

### PHASE 1: FEED HANDLER CORE

---

### US-001: Protocol Encoder — Binary Encode/Decode
**As a** developer, **I want** binary serialization for all MiniITCH message types **so that** the simulator and handler can communicate over the wire.

**Files:** `include/mdfh/protocol/encoder.hpp`, `src/protocol/encoder.cpp`

**Criteria:**
- [ ] `Encoder::encode<T>(msg, timestamp, buffer, buffer_size)` writes MessageHeader + payload bytes, returns total bytes written
- [ ] `Encoder::decode_payload<T>(buffer_ptr, out_msg)` reads payload struct from raw bytes after header
- [ ] `Encoder::peek_message_type(buffer)` returns `MessageType` from the 3rd byte without full decode
- [ ] Handles all 5 message types: AddOrder, CancelOrder, ExecuteOrder, ReplaceOrder, TradeMessage
- [ ] Returns 0 if buffer is too small for the message
- [ ] Little-endian byte order, `memcpy`-based (no UB from reinterpret_cast)
- [ ] Build passes

---

### US-002: Protocol Encoder Tests
**As a** developer, **I want** comprehensive encoder tests **so that** I can trust the binary serialization is correct.

**Files:** `tests/test_encoder.cpp`, update `CMakeLists.txt` test target

**Criteria:**
- [ ] Round-trip test for each message type: encode → decode → fields match
- [ ] Test batching multiple messages into one buffer and walking through them
- [ ] Test buffer-too-small returns 0
- [ ] Test `peek_message_type` returns correct enum
- [ ] Test timestamp is preserved through encode/decode
- [ ] All tests pass
- [ ] Build passes

---

### US-003: Lock-Free SPSC Queue
**As a** developer, **I want** a lock-free single-producer single-consumer ring buffer **so that** pipeline threads can pass data without locking.

**Files:** `include/mdfh/core/spsc_queue.hpp`

**Criteria:**
- [ ] Template class `SPSCQueue<T, Capacity>` with `Capacity` as power-of-2 (static_assert)
- [ ] `bool try_push(const T&)` — returns false if full
- [ ] `bool try_pop(T&)` — returns false if empty
- [ ] `alignas(64)` on head and tail atomics to prevent false sharing
- [ ] Padding bytes between head and tail cache lines
- [ ] Uses `std::memory_order_acquire` / `std::memory_order_release` only (no `seq_cst`)
- [ ] Index wrapping via `& (Capacity - 1)` bitmask (no modulo)
- [ ] Zero dynamic allocation — `std::array<T, Capacity>` storage
- [ ] Header-only implementation
- [ ] Build passes

---

### US-004: SPSC Queue Tests
**As a** developer, **I want** thorough SPSC queue tests **so that** I can trust correctness under concurrent use.

**Files:** `tests/test_spsc_queue.cpp`, update `CMakeLists.txt`

**Criteria:**
- [ ] Test push/pop single item round-trip
- [ ] Test fill to capacity → try_push returns false
- [ ] Test pop from empty → try_pop returns false
- [ ] Test wrap-around: push Capacity items, pop all, push again
- [ ] Test FIFO ordering: push 1..N, pop in same order
- [ ] Concurrent test: producer thread pushes N items, consumer thread pops N items, verify all received in order
- [ ] All tests pass
- [ ] Build passes

---

### US-005: Frame Parser — Split Datagrams into Messages
**As a** developer, **I want** to split a UDP datagram buffer into individual MiniITCH messages **so that** the parser stage can process them one by one.

**Files:** `include/mdfh/protocol/frame_parser.hpp`, `src/protocol/frame_parser.cpp`

**Criteria:**
- [ ] `FrameParser::parse(buffer, length, callback)` walks buffer by `message_length` field, calls `callback(header, payload_ptr)` for each message
- [ ] Stops if remaining bytes < `sizeof(MessageHeader)` or `message_length` exceeds remaining
- [ ] Returns number of messages successfully parsed
- [ ] Handles empty buffer (returns 0)
- [ ] Does not allocate — works entirely on the input buffer
- [ ] Build passes

---

### US-006: Message Batcher — Pack Messages into Datagrams
**As a** developer, **I want** to batch multiple encoded messages into UDP-sized datagrams **so that** the simulator can send efficiently.

**Files:** `include/mdfh/protocol/batcher.hpp`, `src/protocol/batcher.cpp`

**Criteria:**
- [ ] `Batcher` accumulates encoded messages into an internal buffer (max 1400 bytes per datagram)
- [ ] `add(encoded_bytes, length)` returns true if fits, false if datagram is full
- [ ] `flush()` returns pointer and size of the completed datagram, resets for next batch
- [ ] `is_empty()` returns true if no messages added since last flush
- [ ] Never exceeds configurable MTU (default 1400)
- [ ] Build passes

---

### US-007: Message Validator + Sequence Tracker
**As a** developer, **I want** message validation and gap detection **so that** corrupted or missing messages are caught.

**Files:** `include/mdfh/protocol/validator.hpp`, `include/mdfh/protocol/sequence.hpp`, `src/protocol/validator.cpp`, `src/protocol/sequence.cpp`

**Criteria:**
- [ ] `Validator::validate(header, payload, length)` checks: message_type is known, message_length matches expected for type, payload has enough bytes
- [ ] Returns `ValidationResult` enum: `Ok`, `UnknownType`, `LengthMismatch`, `Truncated`
- [ ] `SequenceTracker` tracks expected sequence number, detects gaps
- [ ] `SequenceTracker::check(seq_num)` returns `InOrder`, `Gap(expected, received)`, or `Duplicate`
- [ ] Gap callback: user-provided function called on gap detection
- [ ] Build passes

---

### US-008: Price Level + Order Struct
**As a** developer, **I want** core data structures for orders and price levels **so that** the order book can track market state.

**Files:** `include/mdfh/core/price_level.hpp`, `include/mdfh/core/order.hpp` (or update existing)

**Criteria:**
- [ ] `Order` struct: `order_id`, `symbol` (as Symbol), `side`, `price`, `remaining_quantity`
- [ ] `PriceLevel` struct: `price`, `total_quantity`, `order_count`
- [ ] `PriceLevel::add(quantity)` increments total_quantity and order_count
- [ ] `PriceLevel::remove(quantity)` decrements; returns true if level is now empty (count == 0)
- [ ] `PriceLevel::modify(old_qty, new_qty)` adjusts total_quantity
- [ ] All arithmetic uses unsigned integers, no negative quantities
- [ ] Build passes

---

### US-009: Order Store — Hash Map of Live Orders
**As a** developer, **I want** a fast order lookup table **so that** cancel/execute/replace can find orders by ID in O(1).

**Files:** `include/mdfh/core/order_store.hpp`, `src/core/order_store.cpp`

**Criteria:**
- [ ] `OrderStore` wraps `std::unordered_map<OrderId, Order>`
- [ ] `insert(Order)` adds order, returns false if ID already exists
- [ ] `find(OrderId)` returns pointer to Order or nullptr
- [ ] `erase(OrderId)` removes order, returns the removed Order (or nullopt)
- [ ] `reserve(n)` pre-allocates buckets to avoid rehashing on hot path
- [ ] `size()` and `empty()` accessors
- [ ] Build passes

---

### US-010: Order Book — Bid/Ask Sides Per Symbol
**As a** developer, **I want** a per-symbol order book with sorted bid/ask price levels **so that** I can reconstruct market depth in real time.

**Files:** `include/mdfh/core/order_book.hpp`, `src/core/order_book.cpp`

**Criteria:**
- [ ] `OrderBook` stores: `std::map<Price, PriceLevel>` for bids (descending) and asks (ascending)
- [ ] `add_order(order)` → inserts order into OrderStore, creates/updates PriceLevel
- [ ] `cancel_order(order_id)` → removes from OrderStore, decrements PriceLevel, removes level if empty
- [ ] `execute_order(order_id, exec_qty)` → reduces remaining_quantity, updates PriceLevel, removes order if fully filled
- [ ] `replace_order(order_id, new_price, new_qty)` → cancel old level entry, update order, add to new level
- [ ] `best_bid()` and `best_ask()` return top-of-book price/qty or nullopt if empty
- [ ] `top_n_levels(side, n)` returns vector of top N price levels for display
- [ ] Each method returns a `BookUpdate` struct with: symbol, best_bid, best_ask, timestamp
- [ ] Build passes

---

### US-011: Order Book Tests
**As a** developer, **I want** order book tests **so that** all update flows are verified.

**Files:** `tests/test_order_book.cpp`, update `CMakeLists.txt`

**Criteria:**
- [ ] Test add orders on both sides → best_bid/best_ask correct
- [ ] Test cancel order → level quantity decreases, level removed when empty
- [ ] Test execute order → partial fill reduces qty, full fill removes order
- [ ] Test replace order → old level updated, new level created
- [ ] Test multiple symbols don't interfere
- [ ] Test top_n_levels returns correct sorted levels
- [ ] Test empty book → best_bid/best_ask return nullopt
- [ ] All tests pass
- [ ] Build passes

---

### US-012: Book Manager — Multi-Symbol Routing
**As a** developer, **I want** a manager that routes parsed messages to the correct per-symbol order book **so that** the book stage can handle multiple symbols.

**Files:** `include/mdfh/core/book_manager.hpp`, `src/core/book_manager.cpp`

**Criteria:**
- [ ] `BookManager` stores `std::unordered_map<uint64_t, OrderBook>` keyed by `Symbol::as_key()`
- [ ] `process(ParsedMessage)` dispatches to correct OrderBook by symbol, returns BookUpdate
- [ ] Auto-creates OrderBook on first message for a new symbol
- [ ] `get_book(Symbol)` returns pointer to OrderBook or nullptr
- [ ] `get_all_symbols()` returns list of active symbols
- [ ] `get_snapshot()` returns top-of-book for all symbols (for display)
- [ ] Build passes

---

### US-013: Multicast Sender + Receiver (Asio)
**As a** developer, **I want** UDP multicast networking **so that** the simulator can broadcast and the handler can receive market data.

**Files:** `include/mdfh/network/multicast_sender.hpp`, `include/mdfh/network/multicast_receiver.hpp`, `src/network/` (implementations if not header-only)

**Criteria:**
- [ ] `MulticastSender` joins multicast group via Asio, provides `send(buffer, length)` method
- [ ] `MulticastReceiver` joins multicast group, calls user callback on each received datagram with `(buffer, length, timestamp)`
- [ ] Receiver timestamps each packet immediately on arrival via `Clock::now_ns()`
- [ ] Configurable multicast address, port, and network interface
- [ ] Uses standalone Asio (not Boost.Asio) — already in the project via FetchContent
- [ ] Socket options: `SO_REUSEADDR`, receive buffer size configurable
- [ ] Build passes

---

### US-014: Latency Histogram + Throughput Tracker
**As a** developer, **I want** performance measurement tools **so that** the consumer can report latency percentiles and message rates.

**Files:** `include/mdfh/consumer/latency_histogram.hpp`, `include/mdfh/consumer/throughput_tracker.hpp`, `src/consumer/latency_histogram.cpp`, `src/consumer/throughput_tracker.cpp`

**Criteria:**
- [ ] `LatencyHistogram` uses fixed buckets (0-100μs in 0.1μs steps) — no dynamic allocation
- [ ] `record(uint64_t latency_ns)` increments the correct bucket
- [ ] `percentile(double p)` returns p-th percentile (p50, p95, p99, p999)
- [ ] `reset()` clears all buckets for next reporting window
- [ ] `ThroughputTracker` tracks events per second over a rolling 1-second window
- [ ] `tick()` records one event
- [ ] `rate()` returns current events/second
- [ ] Both are thread-safe (called from consumer thread only, so single-threaded is fine)
- [ ] Build passes

---

### US-015: Stats Collector + Console Display + CSV Logger
**As a** developer, **I want** a consumer that aggregates stats and displays live market data **so that** I can monitor system performance.

**Files:** `include/mdfh/consumer/stats_collector.hpp`, `include/mdfh/consumer/console_display.hpp`, `include/mdfh/consumer/csv_logger.hpp`, `src/consumer/stats_collector.cpp`, `src/consumer/console_display.cpp`, `src/consumer/csv_logger.cpp`

**Criteria:**
- [ ] `StatsCollector` owns a LatencyHistogram and ThroughputTracker, aggregates BookUpdates
- [ ] Produces a `StatsSnapshot` every 1 second: p50, p95, p99, p999 latency, msgs/sec, packets/sec, book updates/sec
- [ ] `ConsoleDisplay` prints a formatted table of top-of-book for all symbols plus latency stats
- [ ] Display refreshes every 100ms via `std::cout` clear + reprint (no ncurses)
- [ ] `CsvLogger` writes StatsSnapshots to a CSV file for post-analysis
- [ ] Display is toggleable (can run headless with stats only)
- [ ] Build passes

---

### US-016: Pipeline Types — RawPacket, ParsedEvent, BookUpdate
**As a** developer, **I want** the types that flow through the SPSC queues **so that** pipeline stages have defined contracts.

**Files:** `include/mdfh/core/pipeline_types.hpp`

**Criteria:**
- [ ] `RawPacket`: `uint8_t data[1500]`, `size_t length`, `uint64_t receive_timestamp`
- [ ] `ParsedEvent`: `ParsedMessage message` (variant), `uint64_t receive_timestamp`
- [ ] `BookUpdate`: `Symbol symbol`, `std::optional<PriceLevel> best_bid`, `std::optional<PriceLevel> best_ask`, `uint64_t receive_timestamp`, `uint64_t book_timestamp`
- [ ] All types are trivially copyable or movable for queue efficiency
- [ ] Build passes

---

### US-017: Pipeline Stages — Network + Parser
**As a** developer, **I want** the first two pipeline threads **so that** raw UDP packets are received and decoded into typed messages.

**Files:** `src/handler/network_stage.cpp`, `src/handler/parser_stage.cpp`

**Criteria:**
- [ ] `NetworkStage::run(receiver, queue_out)` — loops: receive packet → timestamp → push RawPacket into Q1
- [ ] Handles queue-full by dropping packet and incrementing a drop counter
- [ ] `ParserStage::run(queue_in, queue_out)` — loops: pop RawPacket from Q1 → FrameParser splits into messages → validate each → push ParsedEvent into Q2
- [ ] Counts: messages parsed, messages dropped (validation failure), packets processed
- [ ] Both stages have a `stop()` method that sets an atomic flag to exit the loop
- [ ] Build passes

---

### US-018: Pipeline Stages — Book + Consumer + Orchestrator
**As a** developer, **I want** the remaining pipeline threads and orchestrator **so that** the full 4-stage pipeline runs end-to-end.

**Files:** `src/handler/book_stage.cpp`, `src/handler/consumer_stage.cpp`, `include/mdfh/core/pipeline.hpp`, `src/core/pipeline.cpp`

**Criteria:**
- [ ] `BookStage::run(queue_in, queue_out, book_manager)` — loops: pop ParsedEvent from Q2 → dispatch to BookManager → push BookUpdate into Q3
- [ ] `ConsumerStage::run(queue_in, stats_collector)` — loops: pop BookUpdate from Q3 → feed to StatsCollector → update display
- [ ] `Pipeline` owns: MulticastReceiver, 3 SPSC queues (Q1: 65536, Q2: 65536, Q3: 16384), BookManager, StatsCollector
- [ ] `Pipeline::start()` launches 4 `std::thread`s, one per stage
- [ ] `Pipeline::stop()` signals all stages to stop, joins all threads
- [ ] `Pipeline::get_stats()` returns current StatsSnapshot
- [ ] Build passes

---

### US-019: Feed Handler Main — Wire Everything
**As a** developer, **I want** the feed handler main.cpp to parse args and run the pipeline **so that** I can start the handler from the command line.

**Files:** `src/handler/main.cpp`

**Criteria:**
- [ ] Parses command-line args: `--config <path>` (JSON scenario config), `--display` (enable console display)
- [ ] Loads multicast address + port from config
- [ ] Creates Pipeline with configured receiver
- [ ] Calls `pipeline.start()`
- [ ] Registers signal handler (SIGINT/SIGTERM) to call `pipeline.stop()`
- [ ] Prints startup banner with config summary
- [ ] Waits for signal, then shuts down cleanly
- [ ] Build passes

---

### PHASE 2: EXCHANGE SIMULATOR

---

### US-020: Scenario Loader — JSON Config
**As a** developer, **I want** to load simulation parameters from JSON **so that** scenarios are configurable and reproducible.

**Files:** `include/mdfh/simulator/scenario_loader.hpp`, `src/simulator/scenario_loader.cpp`

**Criteria:**
- [ ] `ScenarioLoader::load(path)` reads JSON file and returns `SimConfig` struct
- [ ] `SimConfig` contains: multicast_address, port, symbols (vector), messages_per_second, duration_seconds, seed, initial_prices (map<string, Price>)
- [ ] Validates required fields are present, throws on missing
- [ ] Uses nlohmann/json (already a dependency)
- [ ] Works with existing `config/scenario.json`
- [ ] Build passes

---

### US-021: Price Walk — Ornstein-Uhlenbeck Model
**As a** developer, **I want** realistic price evolution **so that** simulated market data looks like real markets.

**Files:** `include/mdfh/simulator/price_walk.hpp`, `src/simulator/price_walk.cpp`

**Criteria:**
- [ ] `PriceWalk` implements Ornstein-Uhlenbeck mean-reverting random walk
- [ ] Constructor: `PriceWalk(initial_price, mean_reversion_speed, volatility, seed)`
- [ ] `next()` returns next price as `Price` (uint32_t fixed-point)
- [ ] Uses `std::mt19937` with configurable seed for determinism
- [ ] Prices stay within reasonable bounds (no negative prices, no 10x jumps)
- [ ] Price changes are small deltas per tick (realistic microstructure)
- [ ] Build passes

---

### US-022: Sim Order Book + Matching Engine
**As a** developer, **I want** an internal order book in the simulator **so that** cancels and executes reference valid order IDs.

**Files:** `include/mdfh/simulator/sim_order_book.hpp`, `include/mdfh/simulator/sim_matching_engine.hpp`, `src/simulator/sim_order_book.cpp`, `src/simulator/sim_matching_engine.cpp`

**Criteria:**
- [ ] `SimOrderBook` tracks live orders per symbol (order_id → {symbol, side, price, qty})
- [ ] `add_order()` inserts order, returns order_id
- [ ] `get_random_order()` returns a random live order ID (for cancel/execute/replace targets)
- [ ] `remove_order(id)` removes order
- [ ] `SimMatchingEngine` detects when a new order crosses the book (buy price >= best ask, or sell price <= best bid)
- [ ] On cross: generates a TradeMessage, removes/reduces the matched orders
- [ ] Build passes

---

### US-023: Order Generator — Weighted Random Messages
**As a** developer, **I want** realistic message generation **so that** the simulator produces a natural mix of market events.

**Files:** `include/mdfh/simulator/order_generator.hpp`, `src/simulator/order_generator.cpp`

**Criteria:**
- [ ] `OrderGenerator` produces messages with weighted distribution: ~40% Add, ~25% Cancel, ~20% Execute, ~10% Replace, ~5% Trade
- [ ] Uses `std::discrete_distribution` for message type selection
- [ ] Add orders: random side, symbol from config, price from PriceWalk, quantity from distribution
- [ ] Cancel/Execute/Replace: picks a valid order_id from SimOrderBook
- [ ] Monotonically increasing order_id counter
- [ ] Deterministic given seed
- [ ] Build passes

---

### US-024: Market Simulator Orchestrator
**As a** developer, **I want** the top-level simulator that ties everything together **so that** I can generate a full market data stream.

**Files:** `include/mdfh/simulator/market_simulator.hpp`, `src/simulator/market_simulator.cpp`

**Criteria:**
- [ ] `MarketSimulator` owns: PriceWalk per symbol, SimOrderBook, OrderGenerator, Batcher, MulticastSender
- [ ] `run(SimConfig)` — main loop: generate message → encode → batch → send when full
- [ ] Rate limiting: configurable messages_per_second, uses `std::this_thread::sleep_for` or busy-spin
- [ ] Runs for `duration_seconds` then stops
- [ ] Prints summary at end: total messages sent, duration, actual rate
- [ ] Supports graceful shutdown via signal handler
- [ ] Build passes

---

### US-025: Volatility Regime + News Events
**As a** developer, **I want** dynamic market conditions **so that** the simulator produces varying volatility and price jumps.

**Files:** `include/mdfh/simulator/volatility_regime.hpp`, `include/mdfh/simulator/news_event_generator.hpp`, `src/simulator/volatility_regime.cpp`, `src/simulator/news_event_generator.cpp`

**Criteria:**
- [ ] `VolatilityRegime` switches between states: `Calm`, `Volatile`, `Crash`
- [ ] Each state adjusts PriceWalk volatility parameter and message rate
- [ ] Transitions are probabilistic (Markov chain) with configurable transition matrix
- [ ] `NewsEventGenerator` fires random "events" that cause price jumps
- [ ] Events have magnitude (small/medium/large) and direction (up/down)
- [ ] Events affect one or all symbols depending on type
- [ ] Build passes

---

### US-026: Participant Model + Multi-Asset Sim
**As a** developer, **I want** different trader archetypes **so that** order flow patterns are realistic.

**Files:** `include/mdfh/simulator/participant_model.hpp`, `include/mdfh/simulator/multi_asset_sim.hpp`, `src/simulator/participant_model.cpp`, `src/simulator/multi_asset_sim.cpp`

**Criteria:**
- [ ] `ParticipantModel` defines trader types: MarketMaker (quotes both sides), MomentumTrader (follows trend), RandomTrader (noise)
- [ ] Each type has characteristic order size distribution, placement patterns, and cancel rates
- [ ] `MultiAssetSim` generates correlated price moves across symbols
- [ ] Uses configurable correlation matrix
- [ ] Correlation implemented via Cholesky decomposition of random normal draws
- [ ] Build passes

---

### US-027: Exchange Simulator Main — Wire Everything
**As a** developer, **I want** the simulator main.cpp to load config and run **so that** I can start broadcasting market data.

**Files:** `src/simulator/main.cpp`

**Criteria:**
- [ ] Parses `--config <path>` argument
- [ ] Loads SimConfig via ScenarioLoader
- [ ] Creates and runs MarketSimulator
- [ ] Signal handler for graceful shutdown
- [ ] Prints startup banner: symbols, target rate, duration, multicast address
- [ ] Prints summary on exit: messages sent, actual rate, duration
- [ ] Build passes

---

### US-028: Simulator Tests
**As a** developer, **I want** tests for simulator components **so that** I can trust the market data generation.

**Files:** `tests/test_scenario_loader.cpp`, `tests/test_price_walk.cpp`, `tests/test_order_generator.cpp`, `tests/test_sim_matching_engine.cpp`, update `CMakeLists.txt`

**Criteria:**
- [ ] Test ScenarioLoader parses valid JSON correctly
- [ ] Test ScenarioLoader throws on missing required fields
- [ ] Test PriceWalk produces deterministic sequence given same seed
- [ ] Test PriceWalk prices stay positive and within bounds
- [ ] Test OrderGenerator produces correct distribution (within tolerance)
- [ ] Test SimMatchingEngine detects crosses correctly
- [ ] All tests pass
- [ ] Build passes

---

### PHASE 3: INFRASTRUCTURE

---

### US-029: Namespace Migration — mdfh → qf
**As a** developer, **I want** to rename the namespace from `mdfh` to `qf` **so that** it reflects the full QuantFlow platform.

**Criteria:**
- [ ] Rename `include/mdfh/` directory to `include/qf/`
- [ ] Update all `#include "mdfh/..."` to `#include "qf/..."` across all files
- [ ] Update all `namespace mdfh` to `namespace qf` across all files
- [ ] Update CMakeLists.txt include paths
- [ ] Build passes
- [ ] All existing tests pass

---

### US-030: YAML Config Loader
**As a** developer, **I want** a YAML-based configuration system **so that** the full platform can be configured from a single file.

**Files:** `include/qf/common/config.hpp`, `src/infra/config.cpp`, `config/quantflow.yaml`

**Criteria:**
- [ ] Add yaml-cpp dependency via FetchContent in CMakeLists.txt
- [ ] `Config::load(path)` reads YAML file into a tree structure
- [ ] `Config::get<T>(key)` returns typed value with dot-notation (e.g., `"feed.multicast_address"`)
- [ ] `Config::get<T>(key, default)` returns default if key missing
- [ ] Create `config/quantflow.yaml` with sections for: feed, simulator, signals, strategy, risk, gateway, data
- [ ] Build passes

---

### US-031: Structured Logging
**As a** developer, **I want** structured logging **so that** I can debug and monitor all subsystems.

**Files:** `include/qf/common/logging.hpp`, `src/infra/logging.cpp`

**Criteria:**
- [ ] Add spdlog dependency via FetchContent in CMakeLists.txt
- [ ] `Logger::init(config)` sets up spdlog with console + optional file sinks
- [ ] Macros: `QF_LOG_DEBUG(...)`, `QF_LOG_INFO(...)`, `QF_LOG_WARN(...)`, `QF_LOG_ERROR(...)`
- [ ] Each log line includes: timestamp (μs precision), thread name, subsystem tag, message
- [ ] Log level configurable at runtime
- [ ] No logging on hot path by default (wrapped in level check)
- [ ] Build passes

---

### US-032: Thread Utilities
**As a** developer, **I want** thread naming and CPU pinning **so that** pipeline threads are identifiable and performant.

**Files:** `include/qf/common/thread_utils.hpp`

**Criteria:**
- [ ] `set_thread_name(name)` sets OS thread name (Windows: `SetThreadDescription`)
- [ ] `pin_to_core(core_id)` sets thread affinity to specific CPU core (Windows: `SetThreadAffinityMask`)
- [ ] `set_thread_priority(priority)` sets thread priority (Windows: `SetThreadPriority`)
- [ ] Graceful no-op on unsupported platforms
- [ ] Header-only implementation
- [ ] Build passes

---

### US-033: MPSC Queue + Object Pool
**As a** developer, **I want** additional concurrent data structures **so that** multiple signal producers can feed a single consumer and allocations are avoided.

**Files:** `include/qf/core/mpsc_queue.hpp`, `include/qf/core/object_pool.hpp`

**Criteria:**
- [ ] `MPSCQueue<T, Capacity>` — multi-producer single-consumer lock-free queue
- [ ] Uses atomic compare-and-swap for producers, simple load for consumer
- [ ] `ObjectPool<T, N>` — pre-allocated pool of N objects
- [ ] `acquire()` returns pointer to available object or nullptr
- [ ] `release(ptr)` returns object to pool
- [ ] Both are header-only, zero dynamic allocation
- [ ] Build passes

---

### US-034: Event Bus — Pub/Sub
**As a** developer, **I want** an internal event bus **so that** subsystems can communicate without tight coupling.

**Files:** `include/qf/core/event_bus.hpp`, `src/core/event_bus.cpp`

**Criteria:**
- [ ] `EventBus` allows subscribing callbacks to event types
- [ ] `subscribe<EventType>(callback)` registers a listener
- [ ] `publish(event)` dispatches to all registered listeners for that event type
- [ ] Thread-safe: subscriptions are registered at startup, publishing happens from any thread
- [ ] Uses `std::function` for callbacks, `std::unordered_map` by `typeid` for dispatch
- [ ] Build passes

---

### PHASE 4: SIGNAL ENGINE

---

### US-035: Signal Types + Feature Base Interface
**As a** developer, **I want** the signal engine type system **so that** features and signals have consistent interfaces.

**Files:** `include/qf/signals/signal_types.hpp`, `include/qf/signals/features/feature_base.hpp`, `include/qf/signals/indicators/indicator_base.hpp`

**Criteria:**
- [ ] `Signal` struct: symbol, type (enum), strength (double, -1 to +1), direction (Buy/Sell/Neutral), timestamp
- [ ] `FeatureVector` struct: array of named doubles (feature_name → value), timestamp
- [ ] `SignalStrength` enum: `Strong`, `Moderate`, `Weak`, `None`
- [ ] `FeatureBase` abstract class: `virtual double compute(const OrderBook&, const TradeHistory&) = 0`, `virtual std::string name() = 0`
- [ ] `IndicatorBase` abstract class: `virtual double update(double value) = 0`, `virtual double value() = 0`, `virtual void reset() = 0`
- [ ] Build passes

---

### US-036: VWAP Feature
**As a** developer, **I want** volume-weighted average price **so that** strategies can compare current price to fair value.

**Files:** `include/qf/signals/features/vwap.hpp`, `src/signals/features/vwap.cpp`, `tests/test_vwap.cpp`

**Criteria:**
- [ ] `VWAP` extends FeatureBase
- [ ] Tracks cumulative (price × quantity) and cumulative quantity from trades
- [ ] `compute()` returns sum(price*qty) / sum(qty)
- [ ] `reset()` clears accumulators (for daily reset)
- [ ] Test: known sequence of trades → correct VWAP
- [ ] Test: single trade → VWAP equals trade price
- [ ] Build passes

---

### US-037: TWAP + Microprice Features
**As a** developer, **I want** time-weighted average price and size-weighted microprice **so that** I have multiple fair-value estimates.

**Files:** `include/qf/signals/features/twap.hpp`, `include/qf/signals/features/microprice.hpp`, `src/signals/features/twap.cpp`, `src/signals/features/microprice.cpp`

**Criteria:**
- [ ] `TWAP` tracks time-weighted price over a configurable window
- [ ] Uses a rolling window of (price, timestamp) pairs
- [ ] `Microprice` computes bid/ask size-weighted mid: `(bid_price * ask_qty + ask_price * bid_qty) / (bid_qty + ask_qty)`
- [ ] Returns NaN if no bid or ask available
- [ ] Both extend FeatureBase
- [ ] Build passes

---

### US-038: Order Flow Imbalance + Book Depth Ratio
**As a** developer, **I want** order flow and depth features **so that** strategies can detect buying/selling pressure.

**Files:** `include/qf/signals/features/order_flow_imbalance.hpp`, `include/qf/signals/features/book_depth_ratio.hpp`, `src/signals/features/order_flow_imbalance.cpp`, `src/signals/features/book_depth_ratio.cpp`

**Criteria:**
- [ ] `OrderFlowImbalance` (OFI): tracks net buy vs sell volume over a rolling window
- [ ] `compute()` returns (buy_volume - sell_volume) / (buy_volume + sell_volume), range [-1, +1]
- [ ] `BookDepthRatio` computes total bid depth / total ask depth across top N levels
- [ ] Returns ratio > 1 when more bid depth (bullish), < 1 when more ask depth
- [ ] Configurable number of levels (default 5)
- [ ] Build passes

---

### US-039: Spread Tracker + Trade Flow
**As a** developer, **I want** spread and trade aggressor features **so that** I can detect market tightness and aggressor patterns.

**Files:** `include/qf/signals/features/spread_tracker.hpp`, `include/qf/signals/features/trade_flow.hpp`, `src/signals/features/spread_tracker.cpp`, `src/signals/features/trade_flow.cpp`

**Criteria:**
- [ ] `SpreadTracker` tracks rolling mean, std, and z-score of the bid-ask spread
- [ ] `compute()` returns spread z-score (how unusual is current spread)
- [ ] `TradeFlow` detects aggressor side by comparing trade price to mid
- [ ] Tracks ratio of buy-aggressor vs sell-aggressor trades
- [ ] Both use configurable rolling window sizes
- [ ] Build passes

---

### US-040: Volatility Estimator
**As a** developer, **I want** realized volatility estimates **so that** strategies can adapt to market conditions.

**Files:** `include/qf/signals/features/volatility_estimator.hpp`, `src/signals/features/volatility_estimator.cpp`

**Criteria:**
- [ ] Implements Parkinson estimator (high-low based): `vol = sqrt(1/(4*n*ln2) * sum((ln(high/low))^2))`
- [ ] Implements Yang-Zhang estimator (open-close-high-low)
- [ ] Configurable window size
- [ ] Annualization factor configurable
- [ ] Returns 0 if insufficient data
- [ ] Build passes

---

### US-041: Momentum + Mean Reversion Features
**As a** developer, **I want** directional and mean-reversion features **so that** strategies can detect trends and reversals.

**Files:** `include/qf/signals/features/momentum.hpp`, `include/qf/signals/features/mean_reversion.hpp`, `src/signals/features/momentum.cpp`, `src/signals/features/mean_reversion.cpp`

**Criteria:**
- [ ] `Momentum` computes price change over short (10), medium (50), long (200) tick windows
- [ ] Normalizes by dividing by rolling standard deviation (z-score of returns)
- [ ] `MeanReversion` computes z-score of current price vs rolling moving average
- [ ] Positive z-score = price above mean (potential sell), negative = below (potential buy)
- [ ] Both configurable window sizes
- [ ] Build passes

---

### US-042: Tick Intensity Feature
**As a** developer, **I want** message arrival rate tracking **so that** I can detect unusual activity.

**Files:** `include/qf/signals/features/tick_intensity.hpp`, `src/signals/features/tick_intensity.cpp`

**Criteria:**
- [ ] `TickIntensity` measures message arrival rate relative to rolling baseline
- [ ] `compute()` returns ratio of current rate to average rate (>1 = unusual activity)
- [ ] Uses exponentially weighted moving average for baseline
- [ ] Spike detection: returns magnitude of deviation from baseline
- [ ] Build passes

---

### US-043: EMA Indicator
**As a** developer, **I want** exponential moving average **so that** features can be smoothed.

**Files:** `include/qf/signals/indicators/ema.hpp`, `src/signals/indicators/ema.cpp`, `tests/test_ema.cpp`

**Criteria:**
- [ ] `EMA` extends IndicatorBase
- [ ] Constructor takes `span` (number of periods), computes alpha = 2/(span+1)
- [ ] `update(value)` applies EMA formula: `ema = alpha * value + (1 - alpha) * prev_ema`
- [ ] First value initializes EMA directly (no warm-up bias)
- [ ] `value()` returns current EMA
- [ ] Test: known sequence → matches hand-calculated EMA
- [ ] Build passes

---

### US-044: Bollinger Bands + RSI Indicators
**As a** developer, **I want** Bollinger Bands and RSI **so that** strategies have standard technical indicators.

**Files:** `include/qf/signals/indicators/bollinger.hpp`, `include/qf/signals/indicators/rsi.hpp`, `src/signals/indicators/bollinger.cpp`, `src/signals/indicators/rsi.cpp`

**Criteria:**
- [ ] `Bollinger` computes: middle (SMA), upper (middle + k*std), lower (middle - k*std)
- [ ] Constructor: `Bollinger(period=20, num_std=2.0)`
- [ ] `update(price)` updates rolling window, returns `{upper, middle, lower, %b, bandwidth}`
- [ ] `RSI` tracks average gain/loss over configurable period (default 14)
- [ ] `update(price)` returns RSI value 0-100
- [ ] RSI > 70 = overbought, < 30 = oversold
- [ ] Build passes

---

### US-045: MACD + Z-Score Indicators
**As a** developer, **I want** MACD and rolling z-score **so that** I can detect momentum shifts and normalize features.

**Files:** `include/qf/signals/indicators/macd.hpp`, `include/qf/signals/indicators/zscore.hpp`, `src/signals/indicators/macd.cpp`, `src/signals/indicators/zscore.cpp`

**Criteria:**
- [ ] `MACD` uses two EMAs (fast=12, slow=26) and signal EMA (period=9)
- [ ] `update(price)` returns `{macd_line, signal_line, histogram}`
- [ ] `ZScore` computes rolling z-score: `(value - mean) / std` over configurable window
- [ ] Uses Welford's online algorithm for mean and variance
- [ ] Returns 0 if insufficient data for standard deviation
- [ ] Build passes

---

### US-046: Alpha Combiner + Signal Decay
**As a** developer, **I want** composite signal generation **so that** multiple features are combined into a single trading signal.

**Files:** `include/qf/signals/composite/alpha_combiner.hpp`, `include/qf/signals/composite/signal_decay.hpp`, `src/signals/composite/alpha_combiner.cpp`, `src/signals/composite/signal_decay.cpp`

**Criteria:**
- [ ] `AlphaCombiner` takes weighted combination of normalized feature values
- [ ] Weights are configurable per feature
- [ ] Output clamped to [-1, +1] range
- [ ] `SignalDecay` applies exponential time-decay to signal strength
- [ ] Old signals fade to zero; only fresh signals drive decisions
- [ ] Configurable half-life in seconds
- [ ] Build passes

---

### US-047: Regime Detector
**As a** developer, **I want** market regime detection **so that** strategies adapt to trending vs ranging conditions.

**Files:** `include/qf/signals/composite/regime_detector.hpp`, `src/signals/composite/regime_detector.cpp`

**Criteria:**
- [ ] `RegimeDetector` classifies market into: `Trending`, `Ranging`, `Volatile`
- [ ] Uses combination of: ADX-like directional strength, volatility level, autocorrelation of returns
- [ ] `detect(features)` returns current regime with confidence score
- [ ] Strategies can query regime to enable/disable themselves
- [ ] Hysteresis: regime doesn't flicker on borderline values
- [ ] Build passes

---

### US-048: Feature Store + Feature Normalizer (ML Layer)
**As a** developer, **I want** rolling feature storage and online normalization **so that** ML models can consume standardized features.

**Files:** `include/qf/signals/ml/feature_store.hpp`, `include/qf/signals/ml/feature_normalizer.hpp`, `src/signals/ml/feature_store.cpp`, `src/signals/ml/feature_normalizer.cpp`

**Criteria:**
- [ ] `FeatureStore` stores rolling window of FeatureVectors (configurable depth)
- [ ] `push(FeatureVector)` adds new observation, evicts oldest if full
- [ ] `get_window(n)` returns last n observations as matrix (for model input)
- [ ] `FeatureNormalizer` uses Welford's online algorithm for running mean/variance
- [ ] `normalize(raw_value)` returns `(value - mean) / std`
- [ ] `update(value)` updates running statistics
- [ ] Build passes

---

### US-049: Linear Model + Model Loader + Prediction Cache
**As a** developer, **I want** online ML prediction **so that** signals can incorporate learned patterns.

**Files:** `include/qf/signals/ml/linear_model.hpp`, `include/qf/signals/ml/model_loader.hpp`, `include/qf/signals/ml/prediction_cache.hpp`, `src/signals/ml/linear_model.cpp`, `src/signals/ml/model_loader.cpp`, `src/signals/ml/prediction_cache.cpp`

**Criteria:**
- [ ] `LinearModel` implements online Ridge regression with configurable regularization
- [ ] `train(features, target)` updates weights incrementally
- [ ] `predict(features)` returns prediction
- [ ] `ModelLoader` loads pre-trained weights from a binary file (simple format: header + float array)
- [ ] `PredictionCache` caches recent predictions keyed by (symbol, timestamp_bucket) to avoid recompute
- [ ] LRU eviction when cache exceeds configurable size
- [ ] Build passes

---

### US-050: Signal Engine Orchestrator + Registry
**As a** developer, **I want** the signal engine to tie all features, indicators, and composites together **so that** BookUpdates produce Signals.

**Files:** `include/qf/signals/signal_engine.hpp`, `include/qf/signals/signal_registry.hpp`, `src/signals/signal_engine.cpp`, `src/signals/signal_registry.cpp`

**Criteria:**
- [ ] `SignalRegistry` allows registering features/indicators by name
- [ ] `register_feature(name, unique_ptr<FeatureBase>)`
- [ ] `SignalEngine` owns registry, runs the pipeline: BookUpdate → features → indicators → composite → Signal
- [ ] `process(BookUpdate)` returns vector of Signals (one per active composite)
- [ ] Configurable: which features/indicators are active
- [ ] Thread-safe: can be called from the pipeline's book stage thread
- [ ] Build passes

---

### US-051: Signal Engine Tests
**As a** developer, **I want** signal engine tests **so that** the feature → signal pipeline is verified.

**Files:** `tests/test_signal_engine.cpp`, `tests/test_microprice.cpp`, `tests/test_order_flow_imbalance.cpp`, `tests/test_bollinger.cpp`, `tests/test_alpha_combiner.cpp`, `tests/test_feature_normalizer.cpp`

**Criteria:**
- [ ] Test microprice computation with known bid/ask sizes
- [ ] Test OFI returns correct imbalance ratio
- [ ] Test Bollinger bands with known price sequence
- [ ] Test AlphaCombiner produces weighted sum within [-1, +1]
- [ ] Test FeatureNormalizer converges to correct mean/std
- [ ] Test full SignalEngine: feed BookUpdates → get Signals
- [ ] All tests pass
- [ ] Build passes

---

### PHASE 5: STRATEGY ENGINE

---

### US-052: Strategy Types + Base Interface + Registry
**As a** developer, **I want** the strategy framework **so that** trading strategies have a consistent interface.

**Files:** `include/qf/strategy/strategy_types.hpp`, `include/qf/strategy/strategy_base.hpp`, `include/qf/strategy/strategy_engine.hpp`, `include/qf/strategy/strategy_registry.hpp`

**Criteria:**
- [ ] `Decision` struct: symbol, side, quantity, order_type (Market/Limit), limit_price, urgency
- [ ] `StrategyBase` abstract: `virtual std::vector<Decision> on_signal(Signal, const Portfolio&) = 0`
- [ ] `virtual std::string name() = 0`, `virtual void configure(Config) = 0`
- [ ] `StrategyRegistry` allows registering strategies by name for dynamic selection
- [ ] `StrategyEngine` runs active strategies, collects Decisions, forwards to OMS
- [ ] Build passes

---

### US-053: Position Tracker + Fill Tracker
**As a** developer, **I want** position and fill tracking **so that** strategies know their current exposure.

**Files:** `include/qf/strategy/position/position_tracker.hpp`, `include/qf/strategy/position/fill_tracker.hpp`, `src/strategy/position/position_tracker.cpp`, `src/strategy/position/fill_tracker.cpp`

**Criteria:**
- [ ] `PositionTracker` tracks net position per symbol (positive = long, negative = short)
- [ ] `update(symbol, side, quantity)` adjusts position
- [ ] `get(symbol)` returns current position (0 if no position)
- [ ] `get_all()` returns map of all positions
- [ ] `FillTracker` records every fill with: timestamp, symbol, side, price, quantity
- [ ] `average_price(symbol)` returns volume-weighted average fill price
- [ ] Build passes

---

### US-054: PnL Calculator + Portfolio
**As a** developer, **I want** real-time PnL calculation **so that** I can track profitability.

**Files:** `include/qf/strategy/position/pnl_calculator.hpp`, `include/qf/strategy/position/portfolio.hpp`, `src/strategy/position/pnl_calculator.cpp`, `src/strategy/position/portfolio.cpp`

**Criteria:**
- [ ] `PnLCalculator` computes realized PnL (from closed trades) and unrealized PnL (from open positions at mark price)
- [ ] `on_fill(fill)` updates realized PnL using FIFO accounting
- [ ] `mark_to_market(symbol, current_price)` updates unrealized PnL
- [ ] `Portfolio` aggregates: PositionTracker + FillTracker + PnLCalculator
- [ ] `Portfolio::snapshot()` returns complete state: positions, fills, realized PnL, unrealized PnL, total PnL
- [ ] Build passes

---

### US-055: Order Sizer + Slippage Model
**As a** developer, **I want** intelligent order sizing and slippage estimation **so that** strategies trade appropriate quantities.

**Files:** `include/qf/strategy/execution/order_sizer.hpp`, `include/qf/strategy/execution/slippage_model.hpp`, `src/strategy/execution/order_sizer.cpp`, `src/strategy/execution/slippage_model.cpp`

**Criteria:**
- [ ] `OrderSizer` supports: fixed size, fixed fractional (% of portfolio), Kelly criterion
- [ ] `size(signal_strength, portfolio, risk_budget)` returns quantity
- [ ] Respects max position size limits
- [ ] `SlippageModel` estimates market impact based on order size relative to book depth
- [ ] Linear impact model: `slippage = alpha * (quantity / ADV)`
- [ ] Returns estimated fill price after impact
- [ ] Build passes

---

### US-056: Execution Algorithms (TWAP, VWAP, Iceberg)
**As a** developer, **I want** execution algorithms **so that** large orders are broken into smaller child orders.

**Files:** `include/qf/strategy/execution/execution_algo.hpp`, `src/strategy/execution/execution_algo.cpp`

**Criteria:**
- [ ] `ExecutionAlgo` base class with `next_slice()` returning next child order
- [ ] `TWAP` algo: splits parent order into equal slices over time horizon
- [ ] `VWAP` algo: front-loads slices based on expected volume profile
- [ ] `Iceberg` algo: shows only a configurable visible quantity, replenishes on fill
- [ ] Each algo tracks: remaining quantity, slices sent, slices filled
- [ ] `is_complete()` returns true when parent order fully executed
- [ ] Build passes

---

### US-057: Market Making Strategy
**As a** developer, **I want** a market making strategy **so that** the system can earn spread by quoting both sides.

**Files:** `include/qf/strategy/strategies/market_making.hpp`, `src/strategy/strategies/market_making.cpp`

**Criteria:**
- [ ] Quotes bid and ask prices symmetrically around fair value (microprice)
- [ ] Spread configurable (e.g., 2 ticks)
- [ ] Skews quotes based on inventory: if long, lower bid/ask to sell more
- [ ] Configurable: max_position, quote_size, inventory_skew_factor
- [ ] Cancels and replaces quotes when fair value moves
- [ ] Extends StrategyBase
- [ ] Build passes

---

### US-058: Momentum Follower Strategy
**As a** developer, **I want** a momentum strategy **so that** the system can follow strong directional moves.

**Files:** `include/qf/strategy/strategies/momentum_follower.hpp`, `src/strategy/strategies/momentum_follower.cpp`

**Criteria:**
- [ ] Buys when momentum signal crosses above threshold, sells when crosses below negative threshold
- [ ] Configurable: entry_threshold, exit_threshold, lookback_period, max_position
- [ ] Uses trailing stop-loss: exits if price retraces by configurable percentage
- [ ] Only trades when regime is `Trending` (from RegimeDetector)
- [ ] Extends StrategyBase
- [ ] Build passes

---

### US-059: Mean Reversion + Stat Arb Strategies
**As a** developer, **I want** mean reversion and pairs trading strategies **so that** the system can fade extreme moves and trade relative value.

**Files:** `include/qf/strategy/strategies/mean_reversion_strat.hpp`, `include/qf/strategy/strategies/stat_arb.hpp`, `src/strategy/strategies/mean_reversion_strat.cpp`, `src/strategy/strategies/stat_arb.cpp`

**Criteria:**
- [ ] `MeanReversion`: buys when z-score < -2σ, sells when > +2σ, exits at mean
- [ ] Configurable: z_entry, z_exit, lookback, max_position
- [ ] Only trades when regime is `Ranging`
- [ ] `StatArb`: tracks price ratio of two correlated symbols
- [ ] Trades the spread: long cheap / short expensive when spread diverges
- [ ] Configurable: symbol_pair, entry_zscore, exit_zscore, lookback
- [ ] Both extend StrategyBase
- [ ] Build passes

---

### US-060: Signal Threshold Strategy
**As a** developer, **I want** a generic threshold-based strategy **so that** any signal can drive trades with configurable rules.

**Files:** `include/qf/strategy/strategies/signal_threshold.hpp`, `src/strategy/strategies/signal_threshold.cpp`

**Criteria:**
- [ ] Configurable: signal_name, buy_threshold, sell_threshold, exit_threshold, max_position
- [ ] Buys when signal > buy_threshold, sells when signal < sell_threshold
- [ ] Exits when signal crosses back through exit_threshold
- [ ] Works with any registered signal
- [ ] Extends StrategyBase
- [ ] Build passes

---

### US-061: Strategy Engine Orchestrator + Tests
**As a** developer, **I want** the strategy engine to manage strategy lifecycle **so that** strategies can be started, stopped, and configured at runtime.

**Files:** `src/strategy/strategy_engine.cpp`, `src/strategy/strategy_registry.cpp`, `tests/test_market_making.cpp`, `tests/test_position_tracker.cpp`, `tests/test_pnl_calculator.cpp`, `tests/test_order_sizer.cpp`

**Criteria:**
- [ ] `StrategyEngine::on_signal(Signal)` feeds signal to all active strategies, collects Decisions
- [ ] `StrategyEngine::enable(name)` / `disable(name)` toggles strategies
- [ ] `StrategyEngine::configure(name, Config)` updates strategy parameters
- [ ] Test PositionTracker correctly tracks buys/sells
- [ ] Test PnLCalculator computes realized PnL on round-trip trades
- [ ] Test OrderSizer respects max position limits
- [ ] All tests pass
- [ ] Build passes

---

### PHASE 6: ORDER MANAGEMENT SYSTEM (OMS/EMS)

---

### US-062: OMS Types + Order States
**As a** developer, **I want** order lifecycle types **so that** orders have well-defined states and transitions.

**Files:** `include/qf/oms/oms_types.hpp`, `include/qf/oms/state_machine/order_states.hpp`

**Criteria:**
- [ ] `OmsOrder` struct: order_id, symbol, side, type (Market/Limit), price, quantity, filled_quantity, state, timestamps (created, sent, acked, last_fill)
- [ ] `OrderState` enum: `New`, `Sent`, `Acked`, `PartialFill`, `Filled`, `Cancelled`, `Rejected`
- [ ] `FillReport` struct: order_id, fill_price, fill_quantity, timestamp, is_final
- [ ] `OrderEvent` struct: order_id, from_state, to_state, timestamp, reason
- [ ] Build passes

---

### US-063: State Transitions + Guards
**As a** developer, **I want** a validated state machine **so that** invalid order transitions are impossible.

**Files:** `include/qf/oms/state_machine/state_transitions.hpp`, `src/oms/state_machine/state_transitions.cpp`

**Criteria:**
- [ ] `StateTransitions::is_valid(from, to)` returns true only for legal transitions
- [ ] Legal transitions: New→Sent, Sent→Acked, Sent→Rejected, Acked→PartialFill, Acked→Filled, Acked→Cancelled, PartialFill→PartialFill, PartialFill→Filled, PartialFill→Cancelled
- [ ] `transition(order, new_state, reason)` applies transition if valid, throws/returns error if not
- [ ] Every transition generates an OrderEvent
- [ ] Build passes

---

### US-064: Order Journal — Append-Only Log
**As a** developer, **I want** an audit trail of all order events **so that** every state change is recorded.

**Files:** `include/qf/oms/state_machine/order_journal.hpp`, `src/oms/state_machine/order_journal.cpp`

**Criteria:**
- [ ] `OrderJournal` stores append-only vector of OrderEvents
- [ ] `append(OrderEvent)` adds event with monotonic sequence number
- [ ] `get_history(order_id)` returns all events for an order
- [ ] `get_recent(n)` returns last n events across all orders
- [ ] Optional: flush to binary file for persistence
- [ ] Thread-safe (single writer, multiple readers)
- [ ] Build passes

---

### US-065: Order Manager — Lifecycle State Machine
**As a** developer, **I want** centralized order management **so that** order creation, fills, and cancels flow through one place.

**Files:** `include/qf/oms/order_manager.hpp`, `src/oms/order_manager.cpp`

**Criteria:**
- [ ] `OrderManager` owns: order store (map<OrderId, OmsOrder>), StateTransitions, OrderJournal
- [ ] `submit(Decision)` → creates OmsOrder in New state, assigns ID, validates, transitions to Sent
- [ ] `on_ack(order_id)` → transitions to Acked
- [ ] `on_fill(FillReport)` → updates filled_quantity, transitions to PartialFill or Filled
- [ ] `cancel(order_id)` → transitions to Cancelled
- [ ] `on_reject(order_id, reason)` → transitions to Rejected
- [ ] All transitions go through StateTransitions for validation
- [ ] Build passes

---

### US-066: Order Validator + Order Router
**As a** developer, **I want** pre-submission validation and routing **so that** invalid orders are caught early and routed correctly.

**Files:** `include/qf/oms/order_validator.hpp`, `include/qf/oms/order_router.hpp`, `src/oms/order_validator.cpp`, `src/oms/order_router.cpp`

**Criteria:**
- [ ] `OrderValidator::validate(Decision)` checks: quantity > 0, price > 0 for limit orders, symbol is known
- [ ] Returns `ValidationResult` with pass/fail and reason
- [ ] `OrderRouter` routes validated orders to the correct exchange connector
- [ ] Currently single destination (SimExchange), but interface supports multiple venues
- [ ] `route(OmsOrder)` sends order and returns routing confirmation
- [ ] Build passes

---

### US-067: Fill Manager + Order ID Generator
**As a** developer, **I want** fill processing and unique ID generation **so that** fills update positions and every order has a globally unique ID.

**Files:** `include/qf/oms/fill_manager.hpp`, `include/qf/oms/order_id_generator.hpp`, `src/oms/fill_manager.cpp`, `src/oms/order_id_generator.cpp`

**Criteria:**
- [ ] `FillManager` receives FillReports, updates OrderManager, feeds Portfolio
- [ ] `on_fill(FillReport)` → calls OrderManager::on_fill → calls Portfolio::on_fill
- [ ] Tracks total fills, total volume, average fill price per symbol
- [ ] `OrderIdGenerator` produces monotonically increasing uint64_t IDs
- [ ] Thread-safe via atomic increment
- [ ] Build passes

---

### US-068: Simulated Exchange + Fill Model
**As a** developer, **I want** a simulated exchange **so that** orders can be filled realistically without a real exchange.

**Files:** `include/qf/oms/sim_exchange/sim_exchange.hpp`, `include/qf/oms/sim_exchange/sim_fill_model.hpp`, `src/oms/sim_exchange/sim_exchange.cpp`, `src/oms/sim_exchange/sim_fill_model.cpp`

**Criteria:**
- [ ] `SimExchange` accepts orders and generates fills based on current market prices
- [ ] Market orders: fill immediately at best bid/ask
- [ ] Limit orders: fill when market price reaches limit price
- [ ] `SimFillModel` adds realistic behavior: configurable latency (simulated round-trip), partial fills (split large orders), random rejects (configurable rate)
- [ ] Generates FillReports and sends back to FillManager
- [ ] Build passes

---

### US-069: Sim Exchange Connector + OMS Integration
**As a** developer, **I want** the connector between OMS and SimExchange **so that** the order flow loop is complete.

**Files:** `include/qf/oms/sim_exchange/sim_exchange_connector.hpp`, `src/oms/sim_exchange/sim_exchange_connector.cpp`, `tests/test_order_manager.cpp`, `tests/test_state_machine.cpp`, `tests/test_sim_exchange.cpp`

**Criteria:**
- [ ] `SimExchangeConnector` implements the exchange interface that OrderRouter expects
- [ ] `send_order(OmsOrder)` → queues to SimExchange → receives fill asynchronously
- [ ] Callback-based: fill arrives → FillManager::on_fill
- [ ] Test: submit order → transitions through New → Sent → Acked → Filled
- [ ] Test: invalid transition (e.g., New → Filled directly) is rejected
- [ ] Test: SimExchange fills market order at best price
- [ ] All tests pass
- [ ] Build passes

---

### PHASE 7: RISK ENGINE

---

### US-070: Risk Types + Risk Engine Interface
**As a** developer, **I want** risk management types **so that** risk checks have consistent structures.

**Files:** `include/qf/risk/risk_types.hpp`, `include/qf/risk/risk_engine.hpp`

**Criteria:**
- [ ] `RiskCheck` enum: `PositionLimit`, `NotionalLimit`, `LossLimit`, `OrderRateLimit`, `ConcentrationLimit`
- [ ] `RiskResult` struct: check_type, passed (bool), current_value, limit_value, message
- [ ] `Exposure` struct: net_exposure, gross_exposure, per_symbol map
- [ ] `RiskEngine` interface: `check_pre_trade(Decision, Portfolio)` returns vector of RiskResults
- [ ] `check_post_trade(FillReport, Portfolio)` updates monitors
- [ ] Build passes

---

### US-071: Position Limit + Notional Limit
**As a** developer, **I want** position and notional exposure limits **so that** no single position gets too large.

**Files:** `include/qf/risk/limits/position_limit.hpp`, `include/qf/risk/limits/notional_limit.hpp`, `src/risk/limits/position_limit.cpp`, `src/risk/limits/notional_limit.cpp`

**Criteria:**
- [ ] `PositionLimit` checks: current_position + order_quantity <= max_shares_per_symbol
- [ ] Configurable per-symbol and global max
- [ ] `NotionalLimit` checks: current_exposure + (order_qty * price) <= max_dollars
- [ ] Tracks net and gross exposure
- [ ] Both return RiskResult with current vs limit values
- [ ] Build passes

---

### US-072: Loss Limit + Order Rate Limit + Concentration Limit
**As a** developer, **I want** loss, rate, and concentration limits **so that** runaway losses and excessive trading are prevented.

**Files:** `include/qf/risk/limits/loss_limit.hpp`, `include/qf/risk/limits/order_rate_limit.hpp`, `include/qf/risk/limits/concentration_limit.hpp`, `src/risk/limits/loss_limit.cpp`, `src/risk/limits/order_rate_limit.cpp`, `src/risk/limits/concentration_limit.cpp`

**Criteria:**
- [ ] `LossLimit` checks: daily PnL > -max_daily_loss (configurable)
- [ ] Tracks running PnL, resets at configurable interval
- [ ] `OrderRateLimit` checks: orders in last second < max_orders_per_second
- [ ] Uses sliding window counter
- [ ] `ConcentrationLimit` checks: position in symbol < max_pct of total portfolio
- [ ] All return RiskResult
- [ ] Build passes

---

### US-073: Pre-Trade Risk Orchestrator
**As a** developer, **I want** all risk checks run before every order **so that** no order bypasses risk controls.

**Files:** `include/qf/risk/pre_trade_risk.hpp`, `src/risk/pre_trade_risk.cpp`

**Criteria:**
- [ ] `PreTradeRisk::check(Decision, Portfolio)` runs all limit checks
- [ ] Returns `Approved` only if ALL checks pass
- [ ] Returns `Rejected(reasons)` if any check fails, with list of failed checks
- [ ] Order of checks: position → notional → loss → rate → concentration
- [ ] Short-circuits on first failure (configurable: fail-fast vs check-all)
- [ ] Logs every check result
- [ ] Build passes

---

### US-074: Post-Trade Risk + Monitors
**As a** developer, **I want** post-trade risk monitoring **so that** limits are checked after every fill.

**Files:** `include/qf/risk/post_trade_risk.hpp`, `include/qf/risk/monitors/pnl_monitor.hpp`, `include/qf/risk/monitors/exposure_monitor.hpp`, `src/risk/post_trade_risk.cpp`, `src/risk/monitors/pnl_monitor.cpp`, `src/risk/monitors/exposure_monitor.cpp`

**Criteria:**
- [ ] `PostTradeRisk::on_fill(FillReport, Portfolio)` updates all monitors
- [ ] Checks if any limit is NOW breached (may have been fine pre-trade)
- [ ] Returns list of breached limits
- [ ] `PnLMonitor` tracks running PnL, fires alert if approaching limit (80% threshold)
- [ ] `ExposureMonitor` tracks net/gross exposure per symbol and total
- [ ] Build passes

---

### US-075: VaR Calculator
**As a** developer, **I want** Value-at-Risk calculation **so that** I can quantify portfolio risk.

**Files:** `include/qf/risk/monitors/var_calculator.hpp`, `src/risk/monitors/var_calculator.cpp`

**Criteria:**
- [ ] Parametric VaR: assumes normal returns, VaR = position * volatility * z_score * sqrt(holding_period)
- [ ] Configurable: confidence level (95%, 99%), holding period (1 day), lookback for volatility
- [ ] Computes per-symbol and portfolio VaR
- [ ] Portfolio VaR accounts for correlations (if correlation matrix provided) or assumes independence
- [ ] Returns VaR in dollar terms
- [ ] Build passes

---

### US-076: Circuit Breaker
**As a** developer, **I want** an emergency kill switch **so that** all trading halts when drawdown exceeds limits.

**Files:** `include/qf/risk/monitors/circuit_breaker.hpp`, `src/risk/monitors/circuit_breaker.cpp`

**Criteria:**
- [ ] `CircuitBreaker` triggers when: drawdown > configurable threshold (default 2%) OR manual kill switch
- [ ] On trigger: cancels all open orders, disables all strategies, fires alert
- [ ] `trip()` activates the breaker
- [ ] `reset()` manually re-enables trading (requires explicit call)
- [ ] `is_tripped()` returns current state
- [ ] Fires event on EventBus when tripped
- [ ] Cannot be auto-reset — requires human intervention
- [ ] Build passes

---

### US-077: Risk Report + Logger + Tests
**As a** developer, **I want** risk reporting and an audit trail **so that** risk decisions are documented.

**Files:** `include/qf/risk/reporting/risk_report.hpp`, `include/qf/risk/reporting/risk_logger.hpp`, `src/risk/reporting/risk_report.cpp`, `src/risk/reporting/risk_logger.cpp`, `tests/test_position_limit.cpp`, `tests/test_loss_limit.cpp`, `tests/test_circuit_breaker.cpp`, `tests/test_var_calculator.cpp`

**Criteria:**
- [ ] `RiskReport::snapshot()` returns: all positions, exposures, PnL, limit utilizations, VaR, circuit breaker state
- [ ] `RiskLogger` logs every pre-trade check (passed/failed), every post-trade update, every breaker event
- [ ] Test: position limit blocks order exceeding max
- [ ] Test: loss limit blocks order when daily loss exceeded
- [ ] Test: circuit breaker trips on drawdown threshold
- [ ] Test: VaR calculation matches hand-computed value
- [ ] All tests pass
- [ ] Build passes

---

### PHASE 8: DATA RECORDER & REPLAY

---

### US-078: Data Types + Binary Writer
**As a** developer, **I want** tick data types and fast binary serialization **so that** market data can be recorded to disk efficiently.

**Files:** `include/qf/data/data_types.hpp`, `include/qf/data/recorder/binary_writer.hpp`, `src/data/recorder/binary_writer.cpp`

**Criteria:**
- [ ] `TickRecord` struct: timestamp, symbol, message_type, payload (raw bytes or parsed fields)
- [ ] `Bar` struct: symbol, open, high, low, close, volume, tick_count, start_time, end_time
- [ ] `MarketSnapshot` struct: symbol, best_bid, best_ask, last_trade_price, timestamp
- [ ] `BinaryWriter` writes TickRecords to a binary file with a header (magic number, version, record count)
- [ ] `write(TickRecord)` appends to file, returns offset
- [ ] Buffered writes (flush every N records or on close)
- [ ] Build passes

---

### US-079: Tick Recorder + CSV Writer
**As a** developer, **I want** live recording and CSV export **so that** data can be captured during simulation and analyzed.

**Files:** `include/qf/data/recorder/tick_recorder.hpp`, `include/qf/data/recorder/csv_writer.hpp`, `src/data/recorder/tick_recorder.cpp`, `src/data/recorder/csv_writer.cpp`

**Criteria:**
- [ ] `TickRecorder` subscribes to pipeline events, records every message via BinaryWriter
- [ ] `start(output_path)` begins recording
- [ ] `stop()` flushes and closes file, prints summary (records written, file size)
- [ ] `CsvWriter` exports tick data to CSV with columns: timestamp, symbol, type, price, quantity, order_id
- [ ] Can convert from binary format to CSV for analysis
- [ ] Build passes

---

### US-080: Compression + File Rotation
**As a** developer, **I want** compressed storage and daily rotation **so that** disk usage is manageable.

**Files:** `include/qf/data/recorder/compression.hpp`, `include/qf/data/recorder/rotation.hpp`, `src/data/recorder/compression.cpp`, `src/data/recorder/rotation.cpp`

**Criteria:**
- [ ] `Compression` wraps LZ4 (via FetchContent) for block compression
- [ ] `compress(input, input_size, output, output_capacity)` returns compressed size
- [ ] `decompress(input, input_size, output, output_capacity)` returns decompressed size
- [ ] `Rotation` manages daily file rotation: `data/YYYY-MM-DD/symbol.bin`
- [ ] Creates new file at midnight or on explicit rotate() call
- [ ] Cleans up files older than configurable retention days
- [ ] Build passes

---

### US-081: Tick Store + Time Index
**As a** developer, **I want** a tick database with time-based lookups **so that** historical data can be queried efficiently.

**Files:** `include/qf/data/storage/tick_store.hpp`, `include/qf/data/storage/time_index.hpp`, `src/data/storage/tick_store.cpp`, `src/data/storage/time_index.cpp`

**Criteria:**
- [ ] `TickStore` provides read/write access to tick data files
- [ ] `read_range(symbol, start_time, end_time)` returns vector of TickRecords in time range
- [ ] `TimeIndex` builds a binary-searchable index of timestamp → file offset
- [ ] Index stored as sorted array of (timestamp, offset) pairs
- [ ] `find(timestamp)` returns file offset via binary search — O(log n)
- [ ] Index can be built from scratch by scanning a tick file
- [ ] Build passes

---

### US-082: Bar Aggregator + Symbol Catalog
**As a** developer, **I want** tick-to-bar aggregation and a symbol index **so that** data can be consumed at different granularities.

**Files:** `include/qf/data/storage/bar_aggregator.hpp`, `include/qf/data/storage/symbol_catalog.hpp`, `src/data/storage/bar_aggregator.cpp`, `src/data/storage/symbol_catalog.cpp`

**Criteria:**
- [ ] `BarAggregator` converts ticks into OHLCV bars at configurable intervals (1s, 1m, 5m, 1h)
- [ ] `add_tick(price, quantity, timestamp)` updates current bar
- [ ] Emits completed bar when interval boundary crossed
- [ ] `SymbolCatalog` indexes available symbols and date ranges
- [ ] `get_available_dates(symbol)` returns vector of dates with data
- [ ] `get_symbols()` returns all symbols with data
- [ ] Build passes

---

### US-083: Replay Clock + Engine
**As a** developer, **I want** historical data replay **so that** recorded data can be played back as if live.

**Files:** `include/qf/data/replay/replay_clock.hpp`, `include/qf/data/replay/replay_engine.hpp`, `src/data/replay/replay_clock.cpp`, `src/data/replay/replay_engine.cpp`

**Criteria:**
- [ ] `ReplayClock` provides virtual time that can run faster/slower than real time
- [ ] `set_speed(multiplier)` — 1.0 = real-time, 10.0 = 10x, 0 = as-fast-as-possible
- [ ] `now()` returns current virtual timestamp
- [ ] `wait_until(timestamp)` blocks until virtual clock reaches timestamp
- [ ] `ReplayEngine` reads tick data from TickStore, publishes events at correct virtual timestamps
- [ ] Can feed directly into the pipeline (replacing MulticastReceiver)
- [ ] Build passes

---

### US-084: Multi-Stream Merger + Replay Publisher
**As a** developer, **I want** to merge multiple symbol streams and publish replayed data **so that** multi-symbol replay works correctly.

**Files:** `include/qf/data/replay/multi_stream_merger.hpp`, `include/qf/data/replay/replay_publisher.hpp`, `src/data/replay/multi_stream_merger.cpp`, `src/data/replay/replay_publisher.cpp`

**Criteria:**
- [ ] `MultiStreamMerger` merges tick streams from multiple symbols by timestamp
- [ ] Uses min-heap (priority queue) to emit events in global time order
- [ ] `ReplayPublisher` publishes merged events to the pipeline's Q1 (same interface as network receiver)
- [ ] Supports pause/resume/seek operations
- [ ] Build passes

---

### US-085: Data Recorder Tests
**As a** developer, **I want** data subsystem tests **so that** recording and replay are verified.

**Files:** `tests/test_tick_recorder.cpp`, `tests/test_bar_aggregator.cpp`, `tests/test_replay_engine.cpp`, `tests/test_compression.cpp`

**Criteria:**
- [ ] Test binary write → read round-trip preserves tick data
- [ ] Test bar aggregator produces correct OHLCV from known ticks
- [ ] Test replay engine emits events in timestamp order
- [ ] Test LZ4 compress → decompress round-trip
- [ ] All tests pass
- [ ] Build passes

---

### PHASE 9: BACKTEST ENGINE

---

### US-086: Backtest Types + Config
**As a** developer, **I want** backtest configuration **so that** backtest runs are parameterized.

**Files:** `include/qf/backtest/backtest_types.hpp`

**Criteria:**
- [ ] `BacktestConfig`: data_path, symbol_list, start_date, end_date, strategy_name, strategy_params, initial_capital, commission_rate
- [ ] `BacktestResult`: equity_curve, trade_list, final_pnl, sharpe, max_drawdown, win_rate, trade_count, duration
- [ ] `TradeRecord`: entry_time, exit_time, symbol, side, entry_price, exit_price, quantity, pnl
- [ ] Build passes

---

### US-087: Fill Simulator + Latency Simulator
**As a** developer, **I want** simulated fills for backtesting **so that** strategies can be tested on historical data.

**Files:** `include/qf/backtest/simulation/fill_simulator.hpp`, `include/qf/backtest/simulation/latency_simulator.hpp`, `src/backtest/simulation/fill_simulator.cpp`, `src/backtest/simulation/latency_simulator.cpp`

**Criteria:**
- [ ] `FillSimulator` simulates fills from historical book state
- [ ] Market orders fill at best bid/ask from historical data
- [ ] Limit orders fill when historical price crosses limit
- [ ] `LatencySimulator` adds configurable delay between decision and execution
- [ ] Models: fixed latency, uniform random, or log-normal distribution
- [ ] Decision at time T → fill at time T + latency (looks up price at fill time)
- [ ] Build passes

---

### US-088: Cost Model + Market Impact
**As a** developer, **I want** transaction cost modeling **so that** backtests reflect real-world trading costs.

**Files:** `include/qf/backtest/simulation/cost_model.hpp`, `include/qf/backtest/simulation/market_impact.hpp`, `src/backtest/simulation/cost_model.cpp`, `src/backtest/simulation/market_impact.cpp`

**Criteria:**
- [ ] `CostModel` computes: commission (per share or per trade), exchange fees, spread cost
- [ ] `total_cost(order)` returns total cost in dollars
- [ ] `MarketImpact` models temporary and permanent price impact
- [ ] Temporary: price moves against you during execution, reverts after
- [ ] Permanent: large orders shift the fair price
- [ ] Impact = alpha * sigma * sqrt(quantity / ADV)
- [ ] Build passes

---

### US-089: Returns Calculator + Equity Curve
**As a** developer, **I want** return and equity calculations **so that** backtest results can be analyzed.

**Files:** `include/qf/backtest/analytics/returns_calculator.hpp`, `include/qf/backtest/analytics/equity_curve.hpp`, `src/backtest/analytics/returns_calculator.cpp`, `src/backtest/analytics/equity_curve.cpp`

**Criteria:**
- [ ] `ReturnsCalculator` computes: daily returns, cumulative returns, log returns
- [ ] Handles gaps (non-trading periods)
- [ ] `EquityCurve` tracks portfolio value over time as vector of (timestamp, value) pairs
- [ ] `add_point(timestamp, value)` appends to curve
- [ ] `to_csv(path)` exports for plotting
- [ ] Build passes

---

### US-090: Sharpe Ratio + Max Drawdown
**As a** developer, **I want** core performance metrics **so that** strategy quality can be measured.

**Files:** `include/qf/backtest/analytics/sharpe_ratio.hpp`, `include/qf/backtest/analytics/max_drawdown.hpp`, `src/backtest/analytics/sharpe_ratio.cpp`, `src/backtest/analytics/max_drawdown.cpp`

**Criteria:**
- [ ] `SharpeRatio::compute(returns, risk_free_rate)` returns annualized Sharpe
- [ ] Annualization factor configurable (default 252 trading days)
- [ ] Handles edge cases: zero std → returns 0, insufficient data → returns NaN
- [ ] `MaxDrawdown::compute(equity_curve)` returns: max_drawdown_pct, max_drawdown_duration, peak_date, trough_date
- [ ] Build passes

---

### US-091: Win Rate + Trade Statistics
**As a** developer, **I want** trade-level statistics **so that** individual strategy behavior can be assessed.

**Files:** `include/qf/backtest/analytics/win_rate.hpp`, `include/qf/backtest/analytics/trade_statistics.hpp`, `src/backtest/analytics/win_rate.cpp`, `src/backtest/analytics/trade_statistics.cpp`

**Criteria:**
- [ ] `WinRate` computes: win count, loss count, win rate %, avg win size, avg loss size, profit factor (gross profit / gross loss)
- [ ] `TradeStatistics` computes: total trades, avg trade PnL, median trade PnL, std of trade PnL, max win, max loss, avg holding period
- [ ] Both operate on vector<TradeRecord>
- [ ] Build passes

---

### US-092: Grid Search + Walk-Forward
**As a** developer, **I want** parameter optimization **so that** strategies can be tuned on historical data.

**Files:** `include/qf/backtest/optimization/grid_search.hpp`, `include/qf/backtest/optimization/walk_forward.hpp`, `src/backtest/optimization/grid_search.cpp`, `src/backtest/optimization/walk_forward.cpp`

**Criteria:**
- [ ] `GridSearch` sweeps parameter ranges: for each combination, runs backtest, records result
- [ ] Input: vector of (param_name, min, max, step) ranges
- [ ] Output: sorted list of (params, sharpe, drawdown, trades)
- [ ] `WalkForward` splits data into train/test windows, optimizes on train, validates on test
- [ ] Configurable: window size, step size, optimization metric
- [ ] Build passes

---

### US-093: Parameter Sensitivity
**As a** developer, **I want** sensitivity analysis **so that** I know how fragile the optimized parameters are.

**Files:** `include/qf/backtest/optimization/param_sensitivity.hpp`, `src/backtest/optimization/param_sensitivity.cpp`

**Criteria:**
- [ ] `ParamSensitivity` varies each parameter ±10-50% around optimal, measures Sharpe change
- [ ] Identifies parameters that cause large Sharpe drops when varied (fragile)
- [ ] Output: per-parameter sensitivity score (0 = robust, 1 = fragile)
- [ ] Build passes

---

### US-094: Backtest Engine + Runner + Tests
**As a** developer, **I want** the backtest orchestrator **so that** I can run complete backtests from the command line.

**Files:** `include/qf/backtest/backtest_engine.hpp`, `include/qf/backtest/backtest_runner.hpp`, `src/backtest/backtest_engine.cpp`, `src/backtest/backtest_runner.cpp`, `tests/test_backtest_engine.cpp`, `tests/test_fill_simulator.cpp`, `tests/test_sharpe_ratio.cpp`, `tests/test_max_drawdown.cpp`

**Criteria:**
- [ ] `BacktestEngine` orchestrates: ReplayEngine → SignalEngine → Strategy → FillSimulator → Analytics
- [ ] Uses SAME code as live (same signal engine, same strategies)
- [ ] `BacktestRunner` provides CLI interface: `--data <path> --strategy <name> --config <yaml>`
- [ ] Prints results summary at end
- [ ] Test: run backtest with known data → verify expected trades and PnL
- [ ] Test: Sharpe ratio matches hand-calculated value
- [ ] Test: max drawdown detected correctly
- [ ] All tests pass
- [ ] Build passes

---

### PHASE 10: API GATEWAY

---

### US-095: Gateway Types + Message Serializer
**As a** developer, **I want** API data types and JSON serialization **so that** the C++ backend can communicate with the browser.

**Files:** `include/qf/gateway/gateway_types.hpp`, `include/qf/gateway/message_serializer.hpp`, `src/gateway/message_serializer.cpp`

**Criteria:**
- [ ] `WsMessage` struct: channel (string), payload (json), timestamp
- [ ] `RestRequest` struct: method, path, headers, body
- [ ] `RestResponse` struct: status_code, headers, body
- [ ] `MessageSerializer` converts C++ structs to JSON (nlohmann/json)
- [ ] Serializers for: BookUpdate, Signal, PnL snapshot, RiskReport, StatsSnapshot
- [ ] Compact JSON (no pretty printing for WebSocket, pretty for REST)
- [ ] Build passes

---

### US-096: WebSocket Server
**As a** developer, **I want** a WebSocket server **so that** the dashboard can receive real-time updates.

**Files:** `include/qf/gateway/websocket_server.hpp`, `src/gateway/websocket_server.cpp`

**Criteria:**
- [ ] Add uWebSockets dependency via FetchContent (or use standalone Asio WebSocket)
- [ ] `WebSocketServer::start(port)` listens for WebSocket connections
- [ ] `broadcast(channel, message)` sends to all clients subscribed to that channel
- [ ] Clients subscribe to channels by sending `{"subscribe": "channel_name"}`
- [ ] Supports multiple concurrent connections
- [ ] Auto-reconnect friendly (no state required from client)
- [ ] Build passes

---

### US-097: Client Manager + Throttle
**As a** developer, **I want** client tracking and rate limiting **so that** the server doesn't overwhelm slow clients.

**Files:** `include/qf/gateway/client_manager.hpp`, `include/qf/gateway/throttle.hpp`, `src/gateway/client_manager.cpp`, `src/gateway/throttle.cpp`

**Criteria:**
- [ ] `ClientManager` tracks connected clients and their subscriptions
- [ ] `on_connect(client_id)`, `on_disconnect(client_id)`, `on_subscribe(client_id, channel)`
- [ ] `get_subscribers(channel)` returns list of client IDs
- [ ] `Throttle` limits update frequency per channel (e.g., book updates max 10/sec to browser)
- [ ] `should_send(channel)` returns true if enough time has passed since last send
- [ ] Configurable per-channel rates
- [ ] Build passes

---

### US-098: Book Channel + Trade Channel
**As a** developer, **I want** WebSocket channels for order book and trade data **so that** the dashboard shows live market data.

**Files:** `include/qf/gateway/channels/book_channel.hpp`, `include/qf/gateway/channels/trade_channel.hpp`, `src/gateway/channels/book_channel.cpp`, `src/gateway/channels/trade_channel.cpp`

**Criteria:**
- [ ] `BookChannel` subscribes to BookUpdates from pipeline, serializes top-of-book for all symbols
- [ ] Sends JSON: `{symbol, bids: [{price, qty}], asks: [{price, qty}], spread, mid}`
- [ ] Throttled to configurable rate (default 10 updates/sec)
- [ ] `TradeChannel` subscribes to trade events, sends last N trades
- [ ] JSON: `{symbol, price, qty, side, timestamp}`
- [ ] Build passes

---

### US-099: Signal + PnL + Risk + System Channels
**As a** developer, **I want** WebSocket channels for signals, PnL, risk, and system health **so that** the dashboard has all data feeds.

**Files:** `include/qf/gateway/channels/signal_channel.hpp`, `include/qf/gateway/channels/pnl_channel.hpp`, `include/qf/gateway/channels/risk_channel.hpp`, `include/qf/gateway/channels/system_channel.hpp`, `src/gateway/channels/signal_channel.cpp`, `src/gateway/channels/pnl_channel.cpp`, `src/gateway/channels/risk_channel.cpp`, `src/gateway/channels/system_channel.cpp`

**Criteria:**
- [ ] `SignalChannel` sends current signal values for all active signals per symbol
- [ ] `PnLChannel` sends portfolio PnL snapshot: realized, unrealized, total, per-symbol
- [ ] `RiskChannel` sends risk report: positions, limits, utilizations, circuit breaker state
- [ ] `SystemChannel` sends system health: latency percentiles, throughput, queue depths, uptime
- [ ] All channels serialize via MessageSerializer and throttle independently
- [ ] Build passes

---

### US-100: REST Server + Endpoints
**As a** developer, **I want** a REST API **so that** the dashboard can send commands and fetch state.

**Files:** `include/qf/gateway/rest_server.hpp`, `src/gateway/rest_server.cpp`, `include/qf/gateway/endpoints/orders_endpoint.hpp`, `include/qf/gateway/endpoints/positions_endpoint.hpp`, `include/qf/gateway/endpoints/strategies_endpoint.hpp`, `include/qf/gateway/endpoints/backtest_endpoint.hpp`, `include/qf/gateway/endpoints/config_endpoint.hpp`, `include/qf/gateway/endpoints/health_endpoint.hpp`

**Criteria:**
- [ ] REST server listens on configurable port (default 8081)
- [ ] `POST /orders` — submit manual order (JSON body: symbol, side, qty, type, price)
- [ ] `GET /orders` — list active orders, `DELETE /orders/:id` — cancel order
- [ ] `GET /positions` — current positions and PnL
- [ ] `GET /strategies` — list strategies and status, `POST /strategies/:name/enable|disable`
- [ ] `POST /backtest` — launch backtest with config, `GET /backtest/:id` — get results
- [ ] `GET /config` — current config, `PUT /config` — update config
- [ ] `GET /health` — system health check
- [ ] CORS headers for browser access
- [ ] Build passes

---

### PHASE 11: WEB DASHBOARD

---

### US-101: Dashboard Scaffolding
**As a** developer, **I want** a React project setup **so that** I can build the trading dashboard.

**Files:** `dashboard/package.json`, `dashboard/tsconfig.json`, `dashboard/vite.config.ts`, `dashboard/tailwind.config.ts`, `dashboard/index.html`, `dashboard/src/main.tsx`, `dashboard/src/App.tsx`

**Criteria:**
- [ ] Vite + React 18 + TypeScript project
- [ ] Tailwind CSS configured with dark theme
- [ ] Dependencies: zustand, lightweight-charts (TradingView), recharts, lucide-react
- [ ] App.tsx renders placeholder with sidebar navigation
- [ ] `npm run dev` starts on port 3000
- [ ] `npm run build` produces production bundle
- [ ] TypeScript strict mode enabled

---

### US-102: TypeScript Types + API Constants
**As a** developer, **I want** TypeScript types matching the C++ backend **so that** the frontend has type safety.

**Files:** `dashboard/src/api/types.ts`, `dashboard/src/lib/constants.ts`

**Criteria:**
- [ ] Types for: OrderBookData, TradeData, SignalData, PnLData, RiskData, SystemHealthData, PositionData, OrderData
- [ ] Types for WebSocket messages: `{channel: string, payload: T}`
- [ ] Types for REST requests/responses matching each endpoint
- [ ] Constants: WS_URL, REST_URL, CHANNELS enum, refresh rates
- [ ] All types exported

---

### US-103: WebSocket Connection Manager
**As a** developer, **I want** a WebSocket client with auto-reconnect **so that** the dashboard stays connected to the backend.

**Files:** `dashboard/src/api/websocket.ts`

**Criteria:**
- [ ] `WebSocketManager` class: connect, disconnect, subscribe(channel, callback), unsubscribe
- [ ] Auto-reconnect with exponential backoff (1s, 2s, 4s, max 30s)
- [ ] Connection status: `connected`, `connecting`, `disconnected`
- [ ] Parses incoming JSON messages and dispatches to channel callbacks
- [ ] Singleton instance exported for use across components
- [ ] Handles connection errors gracefully (no unhandled exceptions)

---

### US-104: REST API Client + React Hooks (Market Data)
**As a** developer, **I want** REST client and market data hooks **so that** components can access backend data reactively.

**Files:** `dashboard/src/api/rest.ts`, `dashboard/src/api/hooks/useOrderBook.ts`, `dashboard/src/api/hooks/useTrades.ts`

**Criteria:**
- [ ] `rest.ts` — typed fetch wrapper: `get<T>(path)`, `post<T>(path, body)`, `del(path)`
- [ ] Error handling: throws typed errors with status code and message
- [ ] `useOrderBook()` — subscribes to book WebSocket channel, returns current order book state
- [ ] `useTrades()` — subscribes to trade channel, returns recent trades array
- [ ] Hooks auto-subscribe on mount, auto-unsubscribe on unmount

---

### US-105: React Hooks (Signals, PnL, Risk, System)
**As a** developer, **I want** hooks for remaining data feeds **so that** all dashboard components have reactive data.

**Files:** `dashboard/src/api/hooks/useSignals.ts`, `dashboard/src/api/hooks/usePnL.ts`, `dashboard/src/api/hooks/useRisk.ts`, `dashboard/src/api/hooks/useSystemHealth.ts`

**Criteria:**
- [ ] `useSignals()` — returns current signal values for all symbols
- [ ] `usePnL()` — returns portfolio PnL (realized, unrealized, total, per-symbol)
- [ ] `useRisk()` — returns risk metrics (positions, limits, exposures, circuit breaker)
- [ ] `useSystemHealth()` — returns latency, throughput, queue depths, uptime
- [ ] All hooks subscribe to respective WebSocket channels

---

### US-106: Zustand Stores (Market + Portfolio)
**As a** developer, **I want** centralized state management **so that** data is shared across components without prop drilling.

**Files:** `dashboard/src/store/marketStore.ts`, `dashboard/src/store/portfolioStore.ts`

**Criteria:**
- [ ] `marketStore` — holds: orderBookData (per symbol), recentTrades, selectedSymbol
- [ ] Actions: setOrderBook, addTrade, setSelectedSymbol
- [ ] `portfolioStore` — holds: positions (per symbol), pnl (realized/unrealized/total), fills
- [ ] Actions: updatePosition, updatePnL, addFill
- [ ] Both stores use Zustand with immer middleware for immutable updates

---

### US-107: Zustand Stores (Signal, Risk, Strategy, System, Backtest)
**As a** developer, **I want** remaining state stores **so that** all dashboard data has a home.

**Files:** `dashboard/src/store/signalStore.ts`, `dashboard/src/store/riskStore.ts`, `dashboard/src/store/strategyStore.ts`, `dashboard/src/store/systemStore.ts`, `dashboard/src/store/backtestStore.ts`

**Criteria:**
- [ ] `signalStore` — signal values per symbol, signal history
- [ ] `riskStore` — risk metrics, limit utilizations, circuit breaker state
- [ ] `strategyStore` — list of strategies, enabled/disabled status, per-strategy PnL
- [ ] `systemStore` — latency histogram, throughput, queue depths, connection status
- [ ] `backtestStore` — backtest configs, results, equity curves

---

### US-108: Layout Components
**As a** developer, **I want** the dashboard shell **so that** pages have consistent navigation and status indicators.

**Files:** `dashboard/src/components/layout/Sidebar.tsx`, `dashboard/src/components/layout/Header.tsx`, `dashboard/src/components/layout/StatusBar.tsx`, `dashboard/src/components/layout/Panel.tsx`

**Criteria:**
- [ ] `Sidebar` — vertical nav with icons for: Trading, Signals, Portfolio, Risk, Strategy, Backtest, System
- [ ] Active page highlighted, collapsible
- [ ] `Header` — top bar with: logo/title, connection status indicator, throughput counter
- [ ] `StatusBar` — bottom bar: latency (p50/p99), throughput, gap count, uptime
- [ ] `Panel` — resizable container component for dashboard panels
- [ ] Dark theme styling (dark backgrounds, light text, accent colors for buy/sell)

---

### US-109: OrderBookView
**As a** developer, **I want** a live order book visualization **so that** users can see market depth at a glance.

**Files:** `dashboard/src/components/market/OrderBookView.tsx`

**Criteria:**
- [ ] Displays bid/ask ladder: price, quantity, cumulative quantity for selected symbol
- [ ] Bids in green, asks in red, spread highlighted
- [ ] Quantity bars (horizontal) proportional to size
- [ ] Updates in real-time from useOrderBook hook
- [ ] Configurable number of visible levels (default 10)
- [ ] Spread displayed between bid/ask sections

---

### US-110: PriceChart (Candlestick)
**As a** developer, **I want** a candlestick price chart **so that** users can see price history.

**Files:** `dashboard/src/components/market/PriceChart.tsx`

**Criteria:**
- [ ] Uses TradingView lightweight-charts library
- [ ] Renders candlestick chart for selected symbol
- [ ] Real-time updates: new candles append, current candle updates
- [ ] Volume bars below candles
- [ ] Configurable timeframe (1s, 5s, 1m, 5m)
- [ ] Dark theme styling matching dashboard

---

### US-111: Depth Chart + Spread Chart + Trade Ticker + Symbol Selector
**As a** developer, **I want** additional market data views **so that** users have comprehensive market visibility.

**Files:** `dashboard/src/components/market/OrderBookDepthChart.tsx`, `dashboard/src/components/market/SpreadChart.tsx`, `dashboard/src/components/market/TradeTickerView.tsx`, `dashboard/src/components/market/SymbolSelector.tsx`

**Criteria:**
- [ ] `OrderBookDepthChart` — area chart showing cumulative depth on both sides
- [ ] `SpreadChart` — line chart of bid-ask spread over time
- [ ] `TradeTickerView` — scrolling list of recent trades (time & sales style): time, price, qty, side
- [ ] Color-coded: buy trades green, sell trades red
- [ ] `SymbolSelector` — dropdown to select active symbol, updates all market views
- [ ] All components use dark theme

---

### US-112: Signal Dashboard + Signal Card
**As a** developer, **I want** signal visualization **so that** users can see all trading signals at a glance.

**Files:** `dashboard/src/components/signals/SignalDashboard.tsx`, `dashboard/src/components/signals/SignalCard.tsx`

**Criteria:**
- [ ] `SignalDashboard` shows all active signals for selected symbol in a grid
- [ ] `SignalCard` displays: signal name, current value (-1 to +1), direction (BUY/SELL/NEUTRAL)
- [ ] Horizontal bar visualization: green for positive, red for negative
- [ ] Color intensity proportional to signal strength
- [ ] Updates in real-time from signalStore

---

### US-113: Signal Chart + Heatmap + Alpha Composite
**As a** developer, **I want** signal history and cross-symbol views **so that** users can analyze signal patterns.

**Files:** `dashboard/src/components/signals/SignalChart.tsx`, `dashboard/src/components/signals/SignalHeatmap.tsx`, `dashboard/src/components/signals/AlphaComposite.tsx`

**Criteria:**
- [ ] `SignalChart` — line chart of signal values over time (one line per signal)
- [ ] `SignalHeatmap` — matrix: rows = symbols, columns = signals, cells = color-coded values
- [ ] Green = positive (buy), red = negative (sell), intensity = magnitude
- [ ] `AlphaComposite` — combined alpha score visualization (weighted bar)
- [ ] Shows contribution of each signal to the composite

---

### US-114: Order Entry + Active Orders
**As a** developer, **I want** manual trading controls **so that** users can submit and monitor orders.

**Files:** `dashboard/src/components/trading/OrderEntry.tsx`, `dashboard/src/components/trading/ActiveOrders.tsx`

**Criteria:**
- [ ] `OrderEntry` — form: symbol, side (Buy/Sell buttons), quantity input, type (Market/Limit), limit price
- [ ] Submit calls `POST /orders`, shows success/error feedback
- [ ] Buy button green, sell button red
- [ ] `ActiveOrders` — table of live orders: id, symbol, side, type, price, qty, filled, state
- [ ] Cancel button per order (calls `DELETE /orders/:id`)
- [ ] Real-time status updates

---

### US-115: Order History + Position Table + Fills Table
**As a** developer, **I want** trading history views **so that** users can review past activity.

**Files:** `dashboard/src/components/trading/OrderHistory.tsx`, `dashboard/src/components/trading/PositionTable.tsx`, `dashboard/src/components/trading/FillsTable.tsx`

**Criteria:**
- [ ] `OrderHistory` — table of completed orders: filled, cancelled, rejected
- [ ] Sortable by time, symbol, PnL
- [ ] `PositionTable` — current positions: symbol, quantity, avg price, current price, unrealized PnL
- [ ] PnL color-coded green/red
- [ ] `FillsTable` — recent fills: time, symbol, side, price, quantity

---

### US-116: PnL Summary + Equity Curve
**As a** developer, **I want** PnL visualization **so that** users can track profitability.

**Files:** `dashboard/src/components/pnl/PnLSummary.tsx`, `dashboard/src/components/pnl/EquityCurve.tsx`

**Criteria:**
- [ ] `PnLSummary` — cards showing: total PnL, realized PnL, unrealized PnL, daily PnL
- [ ] Large numbers, green if positive, red if negative
- [ ] `EquityCurve` — line chart of portfolio value over time
- [ ] Uses recharts or lightweight-charts
- [ ] Drawdown highlighted as shaded area below peaks

---

### US-117: PnL By Symbol + Trade Stats
**As a** developer, **I want** per-symbol PnL and trade statistics **so that** users can identify best/worst performers.

**Files:** `dashboard/src/components/pnl/PnLBySymbol.tsx`, `dashboard/src/components/pnl/TradeStats.tsx`

**Criteria:**
- [ ] `PnLBySymbol` — horizontal bar chart: one bar per symbol, sorted by PnL
- [ ] Green bars for profitable, red for losing
- [ ] `TradeStats` — summary cards: win rate, avg win, avg loss, profit factor, Sharpe ratio, trade count

---

### US-118: Risk Dashboard + Exposure Gauge
**As a** developer, **I want** risk visualization **so that** users can monitor risk metrics.

**Files:** `dashboard/src/components/risk/RiskDashboard.tsx`, `dashboard/src/components/risk/ExposureGauge.tsx`

**Criteria:**
- [ ] `RiskDashboard` — overview of all risk metrics in a grid layout
- [ ] Shows limit utilization bars for each risk check
- [ ] `ExposureGauge` — circular gauge showing net and gross exposure as % of limits
- [ ] Green < 50%, yellow 50-80%, red > 80%

---

### US-119: Drawdown Chart + Limit Utilization + Circuit Breaker Panel
**As a** developer, **I want** detailed risk views **so that** users can manage risk actively.

**Files:** `dashboard/src/components/risk/DrawdownChart.tsx`, `dashboard/src/components/risk/LimitUtilization.tsx`, `dashboard/src/components/risk/CircuitBreakerPanel.tsx`

**Criteria:**
- [ ] `DrawdownChart` — area chart of drawdown over time (always negative, deeper = worse)
- [ ] Max drawdown point highlighted
- [ ] `LimitUtilization` — stacked bars showing current vs max for each risk limit
- [ ] `CircuitBreakerPanel` — large status indicator (GREEN = normal, RED = tripped)
- [ ] Manual trip/reset buttons (calls REST API)

---

### US-120: Strategy Views
**As a** developer, **I want** strategy management UI **so that** users can control and monitor strategies.

**Files:** `dashboard/src/components/strategy/StrategyList.tsx`, `dashboard/src/components/strategy/StrategyConfig.tsx`, `dashboard/src/components/strategy/StrategyPerformance.tsx`

**Criteria:**
- [ ] `StrategyList` — table of strategies: name, status (enabled/disabled toggle), PnL, trades
- [ ] Enable/disable calls `POST /strategies/:name/enable|disable`
- [ ] `StrategyConfig` — form to edit strategy parameters (key-value pairs)
- [ ] `StrategyPerformance` — per-strategy PnL chart and metrics

---

### US-121: Backtest Views
**As a** developer, **I want** backtesting UI **so that** users can configure and review backtests.

**Files:** `dashboard/src/components/backtest/BacktestRunner.tsx`, `dashboard/src/components/backtest/BacktestResults.tsx`, `dashboard/src/components/backtest/EquityCurveComparison.tsx`, `dashboard/src/components/backtest/ParameterHeatmap.tsx`

**Criteria:**
- [ ] `BacktestRunner` — form: strategy, date range, parameters → submit to `POST /backtest`
- [ ] Progress indicator while running
- [ ] `BacktestResults` — table of backtest runs: config, Sharpe, drawdown, trades, PnL
- [ ] `EquityCurveComparison` — overlay multiple backtest equity curves on one chart
- [ ] `ParameterHeatmap` — grid search results as heatmap (param1 × param2 → Sharpe)

---

### US-122: System Monitoring Views
**As a** developer, **I want** system health visualization **so that** users can monitor performance.

**Files:** `dashboard/src/components/system/LatencyHistogram.tsx`, `dashboard/src/components/system/ThroughputGauge.tsx`, `dashboard/src/components/system/QueueDepthChart.tsx`, `dashboard/src/components/system/SystemHealth.tsx`, `dashboard/src/components/system/AlertFeed.tsx`

**Criteria:**
- [ ] `LatencyHistogram` — bar chart of latency distribution with p50/p95/p99 lines
- [ ] `ThroughputGauge` — speedometer showing current msgs/sec
- [ ] `QueueDepthChart` — line chart of Q1/Q2/Q3 fill levels over time
- [ ] `SystemHealth` — CPU, memory, thread status indicators
- [ ] `AlertFeed` — scrolling log of system alerts (anomalies, gaps, errors)

---

### US-123: Shared Components
**As a** developer, **I want** reusable UI primitives **so that** the dashboard is consistent.

**Files:** `dashboard/src/components/shared/DataTable.tsx`, `dashboard/src/components/shared/MiniChart.tsx`, `dashboard/src/components/shared/Gauge.tsx`, `dashboard/src/components/shared/HeatmapCell.tsx`, `dashboard/src/components/shared/StatusBadge.tsx`, `dashboard/src/components/shared/TimeAgo.tsx`

**Criteria:**
- [ ] `DataTable` — generic sortable, filterable table component
- [ ] `MiniChart` — sparkline component for inline charts
- [ ] `Gauge` — circular gauge with configurable min/max/thresholds
- [ ] `HeatmapCell` — color-coded cell for heatmap grids
- [ ] `StatusBadge` — green/yellow/red status indicator with label
- [ ] `TimeAgo` — renders relative timestamp ("2s ago", "1m ago")

---

### US-124: Pages + Routing + Theme
**As a** developer, **I want** page layouts and routing **so that** the dashboard is navigable.

**Files:** `dashboard/src/pages/TradingPage.tsx`, `dashboard/src/pages/SignalsPage.tsx`, `dashboard/src/pages/PortfolioPage.tsx`, `dashboard/src/pages/RiskPage.tsx`, `dashboard/src/pages/StrategyPage.tsx`, `dashboard/src/pages/BacktestPage.tsx`, `dashboard/src/pages/SystemPage.tsx`, `dashboard/src/lib/formatters.ts`, `dashboard/src/lib/colors.ts`, `dashboard/src/styles/globals.css`

**Criteria:**
- [ ] `TradingPage` — main view: order book + price chart + order entry + active orders
- [ ] `SignalsPage` — signal dashboard + heatmap + charts
- [ ] `PortfolioPage` — positions + PnL + equity curve
- [ ] `RiskPage` — risk dashboard + drawdown + circuit breaker
- [ ] `StrategyPage` — strategy list + config + performance
- [ ] `BacktestPage` — runner + results + comparison
- [ ] `SystemPage` — latency + throughput + queues + health
- [ ] `formatters.ts` — format prices, numbers, percentages, latency, file sizes
- [ ] `colors.ts` — color scales for heatmaps, buy/sell colors, status colors
- [ ] `globals.css` — Tailwind base + dark theme custom properties
- [ ] React Router for navigation between pages

---

### PHASE 12: FIX PROTOCOL

---

### US-125: FIX Fields + Message Builder/Parser
**As a** developer, **I want** FIX 4.4 message handling **so that** the platform can communicate with exchanges.

**Files:** `include/qf/protocol/fix/fix_fields.hpp`, `include/qf/protocol/fix/fix_message.hpp`, `src/protocol/fix/fix_message.cpp`

**Criteria:**
- [ ] `fix_fields.hpp` defines FIX tag constants: `Tag::MsgType = 35`, `Tag::SenderCompID = 49`, etc.
- [ ] Covers: header tags (8, 9, 35, 49, 56, 34, 52), order tags (11, 54, 55, 38, 40, 44), execution tags (17, 20, 39, 150, 151)
- [ ] `FixMessage` class: build message by adding tag-value pairs
- [ ] `add_field(tag, value)` appends `tag=value|`
- [ ] `to_string()` produces complete FIX message with BeginString, BodyLength, CheckSum
- [ ] `parse(raw_string)` creates FixMessage from raw FIX string
- [ ] Checksum calculation (sum of bytes mod 256)
- [ ] Build passes

---

### US-126: FIX Session Layer
**As a** developer, **I want** FIX session management **so that** connections are maintained properly.

**Files:** `include/qf/protocol/fix/fix_session.hpp`, `src/protocol/fix/fix_session.cpp`

**Criteria:**
- [ ] `FixSession` manages: logon, logout, heartbeat, sequence numbers
- [ ] `logon()` sends Logon message (35=A) with SenderCompID, TargetCompID
- [ ] `heartbeat()` sends Heartbeat (35=0) at configurable interval
- [ ] Tracks inbound/outbound sequence numbers, detects gaps
- [ ] `send(FixMessage)` adds session-level fields and transmits
- [ ] `on_receive(raw)` parses, validates sequence, dispatches to handler
- [ ] Build passes

---

### US-127: FIX Gateway
**As a** developer, **I want** a FIX gateway **so that** orders can be sent/received via FIX protocol.

**Files:** `include/qf/protocol/fix/fix_gateway.hpp`, `src/protocol/fix/fix_gateway.cpp`

**Criteria:**
- [ ] `FixGateway` wraps FixSession + TCP client for full exchange connectivity
- [ ] `send_new_order(order)` builds NewOrderSingle (35=D) message
- [ ] `send_cancel(order_id)` builds OrderCancelRequest (35=F)
- [ ] `on_execution_report(msg)` parses ExecutionReport (35=8), extracts fill info
- [ ] Converts between OmsOrder and FIX messages
- [ ] Build passes

---

### US-128: TCP Server + Client (Async Asio)
**As a** developer, **I want** async TCP networking **so that** FIX and API gateway have a transport layer.

**Files:** `include/qf/network/tcp_server.hpp`, `include/qf/network/tcp_client.hpp`, `src/network/` (if not header-only)

**Criteria:**
- [ ] `TcpServer` — async Asio TCP server with accept loop
- [ ] Callback on new connection, callback on data received
- [ ] Supports multiple concurrent connections
- [ ] `TcpClient` — async Asio TCP client with connect/reconnect
- [ ] `connect(host, port)`, `send(data)`, `on_receive(callback)`
- [ ] Auto-reconnect on disconnect (configurable)
- [ ] Both use standalone Asio (consistent with rest of project)
- [ ] Build passes

---

### PHASE 13: INTEGRATION & POLISH

---

### US-129: Integration Test — Sim to Handler E2E
**As a** developer, **I want** an end-to-end test **so that** the simulator and handler work together.

**Files:** `tests/integration/test_sim_to_handler.cpp`

**Criteria:**
- [ ] Start simulator with known seed for 1 second, 1000 messages
- [ ] Start handler listening on same multicast group
- [ ] Verify handler receives and processes messages
- [ ] Verify order books are non-empty after test
- [ ] Verify latency histogram has data
- [ ] Test completes in < 10 seconds
- [ ] Build passes

---

### US-130: Integration Test — Signal Pipeline
**As a** developer, **I want** signal pipeline integration test **so that** BookUpdates produce Signals.

**Files:** `tests/integration/test_signal_pipeline.cpp`

**Criteria:**
- [ ] Feed synthetic BookUpdates into SignalEngine
- [ ] Verify VWAP, microprice, OFI signals are produced
- [ ] Verify signal values are within expected ranges
- [ ] Verify AlphaCombiner produces composite signal
- [ ] Build passes

---

### US-131: Integration Test — Strategy to OMS to Risk
**As a** developer, **I want** trading loop integration test **so that** signals → strategy → OMS → risk → fill works.

**Files:** `tests/integration/test_strategy_to_oms.cpp`, `tests/integration/test_risk_blocks_order.cpp`

**Criteria:**
- [ ] Feed signal to strategy → produces Decision → OMS submits → SimExchange fills
- [ ] Verify Portfolio updated with position and PnL
- [ ] Test risk block: set position limit low, submit large order, verify rejected
- [ ] Test circuit breaker: simulate large loss, verify breaker trips
- [ ] Build passes

---

### US-132: Integration Test — Full Loop
**As a** developer, **I want** the complete system integration test **so that** all subsystems work together.

**Files:** `tests/integration/test_full_loop.cpp`

**Criteria:**
- [ ] Start simulator → handler → signal engine → strategy → OMS → risk
- [ ] Run for 5 seconds
- [ ] Verify: book updates flowing, signals computed, trades executed, PnL tracked
- [ ] Verify: no crashes, no deadlocks, clean shutdown
- [ ] Build passes

---

### US-133: Benchmarks
**As a** developer, **I want** performance benchmarks **so that** I can measure and optimize throughput and latency.

**Files:** `bench/bench_spsc_queue.cpp`, `bench/bench_order_book.cpp`, `bench/bench_encode_decode.cpp`, `bench/bench_signal_engine.cpp`, `bench/bench_pipeline.cpp`, `bench/bench_backtest.cpp`

**Criteria:**
- [ ] SPSC queue: measure ops/sec for push/pop cycle
- [ ] Order book: measure add/cancel/execute ops/sec
- [ ] Encode/decode: measure messages/sec for all types
- [ ] Signal engine: measure features/sec from BookUpdate
- [ ] Pipeline: measure end-to-end latency with synthetic data
- [ ] Backtest: measure events/sec throughput
- [ ] Each benchmark runs for at least 1 second and prints results
- [ ] Build passes

---

### US-134: CMakeLists.txt — All Targets
**As a** developer, **I want** the build system updated for all subsystems **so that** everything compiles from one CMake invocation.

**Files:** `CMakeLists.txt`

**Criteria:**
- [ ] All new dependencies added via FetchContent: yaml-cpp, spdlog, lz4, uWebSockets (or Asio WebSocket)
- [ ] Targets: `feed_handler`, `exchange_simulator`, `backtest_runner`
- [ ] Test target includes all test files
- [ ] Benchmark target includes all bench files
- [ ] All targets link correct dependencies
- [ ] `cmake -B build -G "MinGW Makefiles" && cmake --build build` succeeds
- [ ] All tests pass

---

### US-135: Docker + Docker Compose
**As a** developer, **I want** containerized deployment **so that** the system is easy to run anywhere.

**Files:** `Dockerfile`, `docker-compose.yml`

**Criteria:**
- [ ] Multi-stage Dockerfile: build C++ in gcc image → copy binaries to slim runtime
- [ ] Dashboard built with `npm run build`, served by nginx
- [ ] `docker-compose.yml` defines services: simulator, handler, dashboard
- [ ] Services connected via Docker network
- [ ] Environment variables for config
- [ ] `docker-compose up` starts the full system

---

### US-136: GitHub Actions CI
**As a** developer, **I want** continuous integration **so that** PRs are tested automatically.

**Files:** `.github/workflows/ci.yml`

**Criteria:**
- [ ] Triggers on push and pull request to main
- [ ] Job 1: Build C++ on Ubuntu (gcc) — cmake, build, run tests
- [ ] Job 2: Build dashboard — npm install, npm run build, npm run lint
- [ ] Caches CMake build and node_modules for speed
- [ ] Fails on: test failure, build error, lint error

---

### US-137: Config Files
**As a** developer, **I want** example configuration files **so that** all subsystems are configurable.

**Files:** `config/quantflow.yaml`, `config/strategies/market_making.yaml`, `config/strategies/momentum.yaml`, `config/strategies/mean_reversion.yaml`, `config/risk/risk_limits.yaml`, `config/stress_scenario.json`, `config/replay_scenario.json`

**Criteria:**
- [ ] `quantflow.yaml` — master config with all subsystem settings
- [ ] Strategy YAMLs with tunable parameters
- [ ] `risk_limits.yaml` — all risk thresholds
- [ ] `stress_scenario.json` — high-rate simulation for stress testing
- [ ] `replay_scenario.json` — config for replaying recorded data
- [ ] All configs have comments explaining each field

---

### US-138: Scripts + README
**As a** developer, **I want** launch scripts and documentation **so that** the project is easy to use.

**Files:** `scripts/start_all.sh`, `scripts/record_session.sh`, `scripts/run_backtest.sh`, `README.md`

**Criteria:**
- [ ] `start_all.sh` — starts simulator, handler, and dashboard in separate terminals
- [ ] `record_session.sh` — starts handler with recording enabled
- [ ] `run_backtest.sh` — runs backtest with default config
- [ ] `README.md` — project overview, architecture diagram, build instructions, usage examples
- [ ] Includes: prerequisites, build steps, how to run, how to run tests, how to run benchmarks, config reference

---

## Non-Goals

- No real exchange connectivity (FIX gateway is a stub for protocol learning)
- No real money trading — simulation only
- No authentication or user management on the dashboard
- No mobile-responsive dashboard design
- No database (all data in memory or flat files)
- No cloud deployment (local Docker only)
- No real-time ML model training (only pre-trained model loading)
- No multi-node distributed architecture

## Technical Considerations

- **Existing code:** Feed handler skeleton exists with 36 header files and 37 stub .cpp files. Phase 1 stories implement logic in existing files.
- **Namespace migration:** Project currently uses `mdfh` namespace. US-029 migrates to `qf` for the full platform.
- **Build system:** Using standalone Asio (not Boost.Asio) via FetchContent. MinGW g++ 13.2 on Windows.
- **Dependencies added by phases:** yaml-cpp (Phase 3), spdlog (Phase 3), LZ4 (Phase 8), uWebSockets (Phase 10)
- **Dashboard:** Separate npm project in `dashboard/` directory, not part of CMake build.
- **Testing:** Google Test for C++, Vitest for TypeScript (dashboard).
- **Performance targets:** p99 < 10μs latency, > 500k msgs/sec throughput in feed handler pipeline.
