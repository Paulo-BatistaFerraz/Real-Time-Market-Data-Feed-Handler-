# Market Data Feed Handler — Full Architecture

> **Target:** ~20,000 lines of C++17 | Two executables | 4 subsystems | Lock-free pipeline

---

## What We're Building

```
┌──────────────────────┐         UDP Multicast          ┌──────────────────────────────────┐
│  EXCHANGE SIMULATOR  │ ──────────────────────────────▶ │        FEED HANDLER              │
│                      │      239.255.0.1:30001          │                                  │
│  Generates realistic │                                 │  Thread 1: Network Receiver      │
│  market data at      │                                 │       │  SPSC Queue #1            │
│  configurable rates  │                                 │       ▼                           │
│  (10k–500k msg/sec)  │                                 │  Thread 2: Protocol Parser       │
│                      │                                 │       │  SPSC Queue #2            │
│  Validates its own   │                                 │       ▼                           │
│  order book so every │                                 │  Thread 3: Order Book Engine     │
│  message is realistic│                                 │       │  SPSC Queue #3            │
│                      │                                 │       ▼                           │
│  Reads JSON scenario │                                 │  Thread 4: Analytics Consumer    │
│  files for replay    │                                 │                                  │
└──────────────────────┘                                 └──────────────────────────────────┘
```

---

## The 4 Subsystems

| # | Subsystem | What It Does | Lines |
|---|-----------|-------------|-------|
| 1 | **Protocol & Codec** | MiniITCH binary wire format — encode, decode, validate, batch | ~3,000 |
| 2 | **Core Engine** | SPSC queues, order book, pipeline orchestrator, thread management | ~6,000 |
| 3 | **Exchange Simulator** | Realistic market data generator with internal matching engine | ~5,000 |
| 4 | **Analytics & Consumer** | Latency histograms, throughput stats, console display, logging | ~4,000 |
| — | **Tests & Benchmarks** | Unit tests, integration tests, stress tests, microbenchmarks | ~2,000 |
| | | **Total** | **~20,000** |

---

## Complete Folder Tree

Every file listed below will be created. Files marked ✅ already exist.

```
market-data-feed-handler/
│
├── CMakeLists.txt ✅                          # Root build — fetches Asio, JSON, GTest
│
├── config/
│   ├── default_scenario.json                  # Default sim config (5 symbols, 10k msg/sec)
│   ├── stress_scenario.json                   # High-throughput test (500k msg/sec)
│   └── replay_scenario.json                   # Deterministic replay for integration tests
│
├── include/mdfh/
│   │
│   │── common/                                ── SHARED TYPES & UTILITIES ──
│   │   ├── types.hpp ✅                       # Price, Quantity, OrderId, Symbol, Side
│   │   ├── clock.hpp ✅                       # now_ns(), nanos_since_midnight()
│   │   ├── constants.hpp                      # Multicast IP, port, buffer sizes, queue caps
│   │   ├── aligned_alloc.hpp                  # Cache-line aligned allocation helper
│   │   └── error_codes.hpp                    # Typed error enum for all subsystems
│   │
│   │── protocol/                              ── SUBSYSTEM 1: PROTOCOL & CODEC ──
│   │   ├── messages.hpp ✅                    # MessageHeader + 5 message structs
│   │   ├── encoder.hpp ✅                     # encode<T>(), decode_header(), parse()
│   │   ├── validator.hpp                      # Validate message integrity (lengths, types, ranges)
│   │   ├── batcher.hpp                        # Pack multiple messages into UDP datagrams
│   │   ├── frame_parser.hpp                   # Split received datagram into individual messages
│   │   └── sequence.hpp                       # Sequence number tracking + gap detection
│   │
│   │── core/                                  ── SUBSYSTEM 2: CORE ENGINE ──
│   │   ├── spsc_queue.hpp                     # Lock-free single-producer single-consumer ring buffer
│   │   ├── order.hpp                          # Order struct (id, side, symbol, price, qty, timestamp)
│   │   ├── order_store.hpp                    # Hash map of live orders (OrderId → Order)
│   │   ├── price_level.hpp                    # Aggregated quantity at a single price
│   │   ├── order_book.hpp                     # Bid/ask sides with sorted price levels per symbol
│   │   ├── book_manager.hpp                   # Routes messages to correct symbol's order book
│   │   ├── pipeline.hpp                       # 4-thread pipeline orchestrator (create, run, stop)
│   │   └── pipeline_types.hpp                 # RawPacket, ParsedEvent, BookUpdate queue payloads
│   │
│   │── network/                               ── NETWORKING (shared by sim + handler) ──
│   │   ├── multicast_sender.hpp               # Asio UDP multicast sender
│   │   ├── multicast_receiver.hpp             # Asio UDP multicast receiver with timestamping
│   │   └── socket_config.hpp                  # Socket options: buffer sizes, TTL, reuse addr
│   │
│   │── simulator/                             ── SUBSYSTEM 3: EXCHANGE SIMULATOR ──
│   │   ├── market_simulator.hpp               # Top-level sim: init, run loop, shutdown
│   │   ├── scenario_loader.hpp                # Parse JSON config into SimConfig struct
│   │   ├── order_generator.hpp                # Generate realistic Add/Cancel/Execute/Replace
│   │   ├── price_walk.hpp                     # Random price walk model (mean-reverting)
│   │   ├── sim_order_book.hpp                 # Simulator's internal order book (validates msgs)
│   │   └── sim_matching_engine.hpp            # Match crossing orders → TradeMessage
│   │
│   │── consumer/                              ── SUBSYSTEM 4: ANALYTICS & CONSUMER ──
│   │   ├── latency_histogram.hpp              # Fixed-bucket histogram (p50, p95, p99, p99.9)
│   │   ├── throughput_tracker.hpp             # msgs/sec, packets/sec, bytes/sec rolling window
│   │   ├── stats_collector.hpp                # Aggregates all metrics from BookUpdates
│   │   ├── console_display.hpp                # Prints top-of-book + stats every 100ms
│   │   ├── csv_logger.hpp                     # Dumps per-second stats to CSV file
│   │   └── alert_monitor.hpp                  # Detects anomalies (gaps, latency spikes, stalls)
│   │
│   └── utils/                                 ── UTILITY HELPERS ──
│       ├── thread_pinning.hpp                 # Set thread affinity (best-effort, Windows/Linux)
│       ├── signal_handler.hpp                 # SIGINT/SIGTERM graceful shutdown
│       └── format_helpers.hpp                 # Human-readable numbers (1,234,567 / "1.2M msg/s")
│
├── src/
│   │── handler/                               ── FEED HANDLER EXECUTABLE ──
│   │   ├── main.cpp ✅                        # Entry point — parse args, build pipeline, run
│   │   ├── network_stage.cpp                  # Thread 1: receive packets, timestamp, push to Q1
│   │   ├── parser_stage.cpp                   # Thread 2: frame → decode → push ParsedEvents to Q2
│   │   ├── book_stage.cpp                     # Thread 3: apply to order book → push BookUpdates to Q3
│   │   └── consumer_stage.cpp                 # Thread 4: collect stats, display, log
│   │
│   │── simulator/                             ── EXCHANGE SIMULATOR EXECUTABLE ──
│   │   ├── main.cpp ✅                        # Entry point — load scenario, run simulator
│   │   ├── market_simulator.cpp               # Sim run loop: generate → validate → encode → send
│   │   ├── scenario_loader.cpp                # JSON → SimConfig
│   │   ├── order_generator.cpp                # Message generation logic + distribution weights
│   │   ├── price_walk.cpp                     # Random walk with mean reversion + volatility
│   │   ├── sim_order_book.cpp                 # Maintains live orders to validate cancels/executes
│   │   └── sim_matching_engine.cpp            # Cross matching: best bid >= best ask → trade
│   │
│   │── protocol/                              ── PROTOCOL IMPL (compiled, not header-only) ──
│   │   ├── validator.cpp                      # Range checks, type checks, length checks
│   │   ├── batcher.cpp                        # Fill datagrams up to MTU, flush on timer/full
│   │   ├── frame_parser.cpp                   # Walk datagram bytes, extract message boundaries
│   │   └── sequence.cpp                       # Track seq nums, detect/report gaps
│   │
│   │── core/                                  ── CORE ENGINE IMPL ──
│   │   ├── order_store.cpp                    # Pre-allocated hash map operations
│   │   ├── price_level.cpp                    # Add/remove quantity at price
│   │   ├── order_book.cpp                     # Full book operations: add, cancel, execute, replace
│   │   ├── book_manager.cpp                   # Symbol routing + book lifecycle
│   │   └── pipeline.cpp                       # Create threads, wire queues, run/stop
│   │
│   └── consumer/                              ── CONSUMER IMPL ──
│       ├── latency_histogram.cpp              # Bucket math, percentile calculation
│       ├── throughput_tracker.cpp              # Rolling window counters
│       ├── stats_collector.cpp                # Aggregate + snapshot
│       ├── console_display.cpp                # ANSI terminal rendering
│       ├── csv_logger.cpp                     # File I/O for stats export
│       └── alert_monitor.cpp                  # Gap detection, spike detection
│
├── tests/
│   ├── test_stub.cpp ✅                       # Placeholder (will be removed)
│   │
│   │── protocol/                              ── PROTOCOL TESTS ──
│   │   ├── test_encode_decode.cpp             # Round-trip: encode → decode → compare all 5 types
│   │   ├── test_validator.cpp                 # Bad lengths, bad types, out-of-range prices
│   │   ├── test_batcher.cpp                   # Fill datagram, flush, boundary conditions
│   │   ├── test_frame_parser.cpp              # Parse multi-message datagrams, partial reads
│   │   └── test_sequence.cpp                  # Gap detection, wraparound, duplicate handling
│   │
│   │── core/                                  ── CORE ENGINE TESTS ──
│   │   ├── test_spsc_queue.cpp                # Single-thread + concurrent producer/consumer
│   │   ├── test_order_store.cpp               # Insert, lookup, remove, capacity
│   │   ├── test_order_book.cpp                # Add/cancel/execute/replace, top-of-book, depth
│   │   ├── test_book_manager.cpp              # Multi-symbol routing, unknown symbol
│   │   └── test_pipeline.cpp                  # Wire up mock stages, verify data flows end-to-end
│   │
│   │── simulator/                             ── SIMULATOR TESTS ──
│   │   ├── test_scenario_loader.cpp           # Valid JSON, missing fields, defaults
│   │   ├── test_order_generator.cpp           # Distribution matches config, all types generated
│   │   ├── test_price_walk.cpp                # Deterministic seed → reproducible prices
│   │   ├── test_sim_order_book.cpp            # Validates cancels reject unknown orders
│   │   └── test_sim_matching_engine.cpp       # Crossing orders produce trades
│   │
│   │── consumer/                              ── CONSUMER TESTS ──
│   │   ├── test_latency_histogram.cpp         # Known inputs → verify p50/p95/p99
│   │   ├── test_throughput_tracker.cpp        # Rolling window math
│   │   └── test_alert_monitor.cpp             # Gap alerts, latency spike detection
│   │
│   └── integration/                           ── INTEGRATION TESTS ──
│       ├── test_sim_to_handler.cpp            # Full loop: sim → multicast → handler → verify book
│       └── test_deterministic_replay.cpp      # Same seed → same output, byte-for-byte
│
├── bench/                                     ── BENCHMARKS ──
│   ├── bench_spsc_queue.cpp                   # Throughput: msgs/sec through SPSC under contention
│   ├── bench_order_book.cpp                   # Order book ops/sec (add, cancel, execute)
│   ├── bench_encode_decode.cpp                # Codec throughput (millions of msg/sec)
│   └── bench_pipeline.cpp                     # End-to-end pipeline latency measurement
│
├── docs/
│   ├── ARCHITECTURE.md                        # ← YOU ARE HERE
│   ├── notas-pt.md ✅                         # Portuguese study notes
│   └── plans/
│       ├── 2026-02-24-feed-handler-design.md ✅
│       └── 2026-02-24-feed-handler-implementation.md ✅
│
└── SENIOR_DEV_SUPPORT.md ✅                   # Mentor persona doc
```

---

## Subsystem 1: Protocol & Codec (~3,000 lines)

**Purpose:** Define the binary wire format (MiniITCH), encode/decode messages, batch them into UDP datagrams, and detect gaps.

```
                    struct                  bytes                   UDP datagram
  Application  ──────────▶  Encoder  ──────────▶  Batcher  ──────────▶  Network
                                                   (≤1400 bytes)

  Network  ──────────▶  FrameParser  ──────────▶  Decoder  ──────────▶  ParsedMessage
   (raw bytes)          split msgs          struct out            std::variant
```

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `messages.hpp` ✅ | 95 | `#pragma pack(1)` structs: MessageHeader (11B), AddOrder, CancelOrder, ExecuteOrder, ReplaceOrder, TradeMessage. `ParsedMessage = std::variant<...>` |
| `encoder.hpp` ✅ | 105 | Template `encode<T>()` — memcpy struct into buffer. `decode_header()`, `decode_payload<T>()`, `peek_message_type()`, `parse()` → variant |
| `validator.hpp` + `.cpp` | ~300 | `validate(buffer, len) → ValidationResult`. Checks: message_length matches type, type is known, price > 0, quantity > 0, timestamp monotonic |
| `batcher.hpp` + `.cpp` | ~400 | `Batcher(max_datagram_size=1400)`. `add(buffer, len) → bool`. When full or `flush()` called, returns packed datagram. Prepends batch header: `{seq_num(8), msg_count(2), total_len(2)}` |
| `frame_parser.hpp` + `.cpp` | ~350 | `FrameParser::parse(datagram, len) → vector<MessageView>`. Walks bytes using message_length field. `MessageView = {ptr, len}` — zero-copy pointing into original buffer |
| `sequence.hpp` + `.cpp` | ~250 | `SequenceTracker`. Tracks expected seq_num. `check(seq) → {OK, GAP, DUPLICATE}`. Reports gap ranges. Handles uint64 wraparound |
| Tests (5 files) | ~800 | Round-trip encode/decode for all 5 types. Validator edge cases. Batcher fill/flush. Frame parser with multi-message datagrams. Sequence gap detection |

### Key Design Decisions
- **Zero-copy frame parsing** — `MessageView` points into the received buffer, no allocation
- **Batch header** has sequence numbers so handler can detect dropped UDP packets
- **Validator** runs before any struct decode — rejects garbage before it corrupts state

---

## Subsystem 2: Core Engine (~6,000 lines)

**Purpose:** Lock-free SPSC queues connecting 4 threads, order book data structures, and the pipeline orchestrator.

```
 ┌──────────────┐     Q1 (RawPacket)     ┌──────────────┐     Q2 (ParsedEvent)    ┌──────────────┐     Q3 (BookUpdate)    ┌──────────────┐
 │   NETWORK    │ ─────────────────────▶  │    PARSER    │ ──────────────────────▶  │  BOOK ENGINE │ ──────────────────────▶  │   CONSUMER   │
 │   THREAD     │     65,536 slots        │    THREAD    │     65,536 slots         │    THREAD    │     16,384 slots        │    THREAD    │
 └──────────────┘                         └──────────────┘                          └──────────────┘                         └──────────────┘
```

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `spsc_queue.hpp` | ~250 | `SPSCQueue<T, Capacity>`. Power-of-2 capacity, index masking. `alignas(64)` head/tail to prevent false sharing. `try_push()` / `try_pop()` with `acquire`/`release` atomics. `size()`, `empty()`, `full()`. Bulk `try_push_batch()` / `try_pop_batch()` |
| `pipeline_types.hpp` | ~150 | `RawPacket{uint8_t data[2048]; size_t len; uint64_t recv_timestamp}`. `ParsedEvent{ParsedMessage msg; uint64_t wire_timestamp; uint64_t recv_timestamp}`. `BookUpdate{Symbol; Side; Price best_bid/ask; Quantity bid_qty/ask_qty; uint64_t latency_ns; EventType}` |
| `order.hpp` | ~80 | `struct Order{OrderId id; Side side; Symbol symbol; Price price; Quantity quantity; uint64_t timestamp}` — stored in the order store |
| `order_store.hpp` + `.cpp` | ~400 | `OrderStore(capacity)`. Pre-allocated `unordered_map<OrderId, Order>` with `.reserve()`. `add()`, `find()`, `remove()`, `update_price_qty()`. Returns `Order*` for zero-copy access |
| `price_level.hpp` + `.cpp` | ~200 | `struct PriceLevel{Price price; Quantity total_qty; uint32_t order_count}`. `add_quantity()`, `remove_quantity()`, `is_empty()` |
| `order_book.hpp` + `.cpp` | ~800 | `OrderBook(Symbol)`. Bids: `std::map<Price, PriceLevel, std::greater<>>` (highest first). Asks: `std::map<Price, PriceLevel>` (lowest first). `add_order()`, `cancel_order()`, `execute_order()`, `replace_order()`. `best_bid()`, `best_ask()`, `spread()`, `depth(n)` returns top-n levels |
| `book_manager.hpp` + `.cpp` | ~400 | `BookManager`. `unordered_map<uint64_t, OrderBook>` keyed by `Symbol::as_key()`. `process(ParsedEvent) → BookUpdate`. Auto-creates book on first message for a symbol. `get_book(symbol)`, `all_symbols()`, `reset()` |
| `pipeline.hpp` + `.cpp` | ~700 | `Pipeline(config)`. Creates 3 SPSC queues + 4 `std::thread`s. `start()` — spawns threads, sets names. `stop()` — sets atomic `running_=false`, joins all. `is_running()`. Wires: NetworkStage → Q1 → ParserStage → Q2 → BookStage → Q3 → ConsumerStage |
| Tests (5 files) | ~1,200 | SPSC: single-thread fill/drain, concurrent ping-pong, capacity boundary. OrderStore: insert/find/remove 100k orders. OrderBook: add→cancel→verify empty, execute partial fill, replace price level move, best_bid/ask/spread. BookManager: multi-symbol routing. Pipeline: mock stages, data flow verification |
| Benchmarks (2 files) | ~400 | SPSC throughput (pinned threads), OrderBook ops/sec |

### Key Design Decisions
- **SPSC, not MPMC** — each queue has exactly 1 writer and 1 reader, no CAS loops needed
- **`alignas(64)`** on head/tail — each on its own cache line, prevents false sharing
- **`std::map` for price levels** — sorted iteration for top-of-book/depth without extra sort
- **`std::greater<>` for bids** — highest bid at `begin()`, lowest ask at `begin()`
- **Pre-allocated order store** — `.reserve()` avoids rehashing on hot path

---

## Subsystem 3: Exchange Simulator (~5,000 lines)

**Purpose:** Generate realistic, self-consistent market data. Every Cancel/Execute references a real order. Prices follow a random walk. Configurable via JSON.

```
  scenario.json ──▶ ScenarioLoader ──▶ SimConfig
                                           │
                                           ▼
                    ┌─────────────────────────────────┐
                    │       MarketSimulator            │
                    │                                  │
                    │  PriceWalk ──▶ OrderGenerator    │
                    │                    │             │
                    │                    ▼             │
                    │  SimOrderBook ◀── validate ──▶ SimMatchingEngine
                    │       │                         │
                    │       ▼                         ▼
                    │  Encoder ──▶ Batcher ──▶ MulticastSender
                    └─────────────────────────────────┘
```

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `scenario_loader.hpp` + `.cpp` | ~350 | `struct SimConfig{vector<SymbolConfig> symbols; uint32_t msg_rate; uint64_t duration_sec; uint32_t seed; string multicast_ip; uint16_t port}`. `SymbolConfig{string name; double start_price; double volatility; double tick_size}`. `load(path) → SimConfig`, defaults for missing fields |
| `price_walk.hpp` + `.cpp` | ~300 | `PriceWalk(start, volatility, tick_size, seed)`. `next() → Price`. Mean-reverting Ornstein-Uhlenbeck discretized: `price += theta*(mean - price)*dt + sigma*sqrt(dt)*N(0,1)`. Snaps to tick grid. Clamps to `[min_price, max_price]` |
| `order_generator.hpp` + `.cpp` | ~600 | `OrderGenerator(config, price_walk, rng)`. `next() → EncodedMessage`. Distribution: 40% Add, 25% Cancel, 20% Execute, 10% Replace, 5% Trade. Cancel/Execute pick random live order from SimOrderBook. Quantity follows log-normal. Buy/Sell 50/50 |
| `sim_order_book.hpp` + `.cpp` | ~500 | `SimOrderBook`. Tracks all live orders by ID. `add(order) → bool`, `cancel(id) → optional<Order>`, `execute(id, qty) → optional<Order>`, `replace(id, price, qty) → bool`. `random_live_order(rng) → optional<OrderId>`. `live_order_count()` |
| `sim_matching_engine.hpp` + `.cpp` | ~450 | `SimMatchingEngine(book)`. After each AddOrder, checks if best_bid >= best_ask. If yes → generates TradeMessage, removes/reduces both sides. Supports partial fills (match min quantity). Returns `vector<TradeMessage>` |
| `market_simulator.hpp` + `.cpp` | ~800 | `MarketSimulator(config)`. `run()` — main loop: generate messages at `msg_rate/sec` using steady_clock pacing. Encodes, batches, sends via multicast. Tracks total messages sent. `stop()` — atomic flag. Prints summary on exit: total msgs, duration, actual rate |
| Tests (5 files) | ~900 | ScenarioLoader: valid/invalid JSON. PriceWalk: deterministic seed, stays in bounds. OrderGenerator: distribution within ±5%, all types present. SimOrderBook: cancel unknown → fails. MatchingEngine: crossing produces trade |
| Config files (3) | ~100 | `default_scenario.json`, `stress_scenario.json`, `replay_scenario.json` |

### Key Design Decisions
- **Self-consistent** — simulator maintains its own order book so Cancel/Execute always reference real orders
- **Deterministic seed** — same seed → same message sequence → reproducible tests
- **Matching engine** — generates TradeMessages organically when orders cross (not random)
- **Rate pacing** — uses `steady_clock` to maintain target msg/sec without busy-spinning

---

## Subsystem 4: Analytics & Consumer (~4,000 lines)

**Purpose:** Measure latency, track throughput, display real-time stats, log to CSV, and alert on anomalies.

```
  BookUpdate  ──▶  StatsCollector
   from Q3              │
                        ├──▶ LatencyHistogram    ──▶ p50 / p95 / p99 / p99.9
                        ├──▶ ThroughputTracker   ──▶ msg/sec, packets/sec
                        ├──▶ ConsoleDisplay      ──▶ terminal output (100ms refresh)
                        ├──▶ CsvLogger           ──▶ stats_YYYYMMDD_HHMMSS.csv
                        └──▶ AlertMonitor        ──▶ stderr warnings
```

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `latency_histogram.hpp` + `.cpp` | ~400 | `LatencyHistogram(max_latency_us=10000, bucket_count=1000)`. Fixed-bucket histogram. `record(latency_ns)`. `percentile(p) → uint64_t ns`. `reset()`. `min()`, `max()`, `mean()`, `count()`. No dynamic allocation after construction |
| `throughput_tracker.hpp` + `.cpp` | ~300 | `ThroughputTracker(window_seconds=5)`. Circular buffer of per-second counts. `tick(count)`. `current_rate() → double`. `peak_rate()`. `total()`. Thread-safe snapshot via atomic swap |
| `stats_collector.hpp` + `.cpp` | ~400 | `StatsCollector`. Owns LatencyHistogram + ThroughputTracker. `process(BookUpdate)` — records latency, increments counters. `snapshot() → StatsSnapshot{p50, p95, p99, p999, msg_rate, book_updates, gap_count}`. Called by ConsoleDisplay and CsvLogger |
| `console_display.hpp` + `.cpp` | ~500 | `ConsoleDisplay(collector, books)`. 100ms refresh timer. Prints: top-of-book (best bid/ask/spread) for each symbol, latency percentiles, throughput, gap count. Uses ANSI escape codes for in-place update. `start()` / `stop()` with own thread |
| `csv_logger.hpp` + `.cpp` | ~250 | `CsvLogger(path, interval_sec=1)`. Writes header row on creation. Every `interval_sec`, appends: `timestamp, msg_count, msg_rate, p50_us, p95_us, p99_us, p999_us, gap_count`. Flushes after each write. `start()` / `stop()` |
| `alert_monitor.hpp` + `.cpp` | ~300 | `AlertMonitor(config)`. Rules: gap detected → warn, p99 > threshold → warn, msg_rate drops > 50% → warn, no messages for > 2s → stall alert. `check(snapshot) → vector<Alert>`. `Alert{level, message, timestamp}`. Writes to stderr |
| Tests (3 files) | ~600 | Histogram: known values → exact percentiles. ThroughputTracker: rolling window math. AlertMonitor: trigger each alert type |

### Key Design Decisions
- **Fixed-bucket histogram** — no heap allocation on hot path, O(1) record, O(N) percentile query
- **Console display** on its own thread — never blocks the pipeline's consumer thread
- **CSV logger** — post-mortem analysis in Excel/Python, one row per second

---

## Network Layer (shared)

| File | Lines | What It Does |
|------|-------|-------------|
| `multicast_sender.hpp` | ~200 | `MulticastSender(io_context, group_ip, port)`. `send(buffer, len)`. Sets TTL=1, SO_SNDBUF. Asio `udp::socket` with `async_send_to` |
| `multicast_receiver.hpp` | ~250 | `MulticastReceiver(io_context, group_ip, port, callback)`. Joins multicast group. `async_receive_from` loop. Stamps `recv_timestamp = Clock::now_ns()` before pushing to queue. SO_RCVBUF = 4MB |
| `socket_config.hpp` | ~100 | Reusable socket option helpers: set buffer sizes, reuse addr, multicast TTL, loopback enable/disable |

---

## Utility Helpers

| File | Lines | What It Does |
|------|-------|-------------|
| `constants.hpp` | ~60 | `MULTICAST_GROUP = "239.255.0.1"`, `MULTICAST_PORT = 30001`, `MAX_DATAGRAM = 1400`, `CACHE_LINE = 64`, queue capacities |
| `aligned_alloc.hpp` | ~80 | `aligned_new<T>(alignment)`, `aligned_delete<T>()`. Used for cache-line-aligned queue storage |
| `error_codes.hpp` | ~50 | `enum class ErrorCode{Ok, BufferTooSmall, UnknownMessageType, InvalidLength, GapDetected, QueueFull}` |
| `thread_pinning.hpp` | ~100 | `pin_thread(thread, core_id)`. Windows: `SetThreadAffinityMask`. Linux: `pthread_setaffinity_np`. Best-effort, logs warning on failure |
| `signal_handler.hpp` | ~80 | `install_signal_handler(callback)`. SIGINT/SIGTERM → sets atomic `running = false`. Cross-platform |
| `format_helpers.hpp` | ~100 | `format_number(1234567) → "1,234,567"`. `format_rate(1500000) → "1.5M/s"`. `format_latency_ns(4523) → "4.5μs"` |

---

## Pipeline Stage Implementations

| File | Lines | What It Does |
|------|-------|-------------|
| `network_stage.cpp` | ~200 | **Thread 1.** `MulticastReceiver` callback → copies packet + timestamp into `RawPacket` → `Q1.try_push()`. Drops if Q1 full (counts drops). Runs Asio `io_context` |
| `parser_stage.cpp` | ~300 | **Thread 2.** `Q1.try_pop()` → `FrameParser::parse()` → for each message: `Validator::validate()` → `Encoder::parse()` → `ParsedEvent` → `Q2.try_push()`. Drops invalid messages (counts errors) |
| `book_stage.cpp` | ~250 | **Thread 3.** `Q2.try_pop()` → `BookManager::process()` → `BookUpdate` (includes best bid/ask + latency) → `Q3.try_push()`. Latency = `Clock::now_ns() - recv_timestamp` |
| `consumer_stage.cpp` | ~200 | **Thread 4.** `Q3.try_pop()` → `StatsCollector::process()`. Periodically calls `ConsoleDisplay::refresh()` and `CsvLogger::write()`. Checks `AlertMonitor` |

---

## Executables — What Runs

### `exchange_simulator`
```
main.cpp → parse CLI args (--config, --seed, --duration)
         → ScenarioLoader::load(config_path)
         → MarketSimulator(config)
         → simulator.run()                     // blocks until duration or Ctrl+C
         → prints summary (total msgs, actual rate, duration)
```

### `feed_handler`
```
main.cpp → parse CLI args (--group, --port, --display, --csv)
         → Pipeline(config)
         → pipeline.start()                    // spawns 4 threads
         → wait for SIGINT
         → pipeline.stop()                     // joins threads
         → prints final stats (total msgs, avg latency, peak rate)
```

---

## Line Count Estimate

| Category | Files | Lines |
|----------|-------|-------|
| **Headers** (include/) | 30 | ~5,500 |
| **Source** (src/) | 18 | ~6,500 |
| **Tests** (tests/) | 20 | ~3,500 |
| **Benchmarks** (bench/) | 4 | ~800 |
| **Config** (config/) | 3 | ~100 |
| **Build** (CMakeLists.txt) | 1 | ~100 |
| **Docs** | 4 | ~3,500 |
| **Total** | **80 files** | **~20,000** |

---

## Build & Run

```bash
# Build everything
cmake -B build -G "MinGW Makefiles"
cmake --build build

# Run tests
./build/run_tests

# Run benchmarks
./build/bench_spsc_queue
./build/bench_order_book

# Start simulator (terminal 1)
./build/exchange_simulator --config config/default_scenario.json

# Start handler (terminal 2)
./build/feed_handler --display --csv stats.csv

# Stress test
./build/exchange_simulator --config config/stress_scenario.json --duration 60
```

---

## Interview Talking Points (per subsystem)

| Subsystem | Interview Topics |
|-----------|-----------------|
| Protocol | "Explain your wire format." — Fixed-size messages, `#pragma pack(1)`, `memcpy` encode/decode, zero-copy frame parsing, batch + sequence numbers for gap detection |
| Core Engine | "How is your queue lock-free?" — SPSC ring buffer, power-of-2 masking, `alignas(64)` false sharing prevention, `memory_order_acquire/release` not `seq_cst`. "Why `std::map` for price levels?" — Sorted iteration for depth, `O(log n)` insert, `std::greater<>` trick for bids |
| Simulator | "How do you ensure realistic data?" — Internal order book validates every cancel/execute, matching engine generates organic trades, mean-reverting price walk, configurable distribution weights |
| Analytics | "How do you measure latency?" — Timestamp at NIC receive (`Clock::now_ns()`), carried through pipeline, measured at consumer. Fixed-bucket histogram, no allocation on hot path. p99.9 < 10μs target |
