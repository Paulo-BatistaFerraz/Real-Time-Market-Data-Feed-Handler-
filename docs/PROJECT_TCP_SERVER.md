# StormSocket — High-Performance TCP Server with HFT Extensions

> **Target:** ~12,000 lines of C++17 | Two phases: HTTP server foundations → HFT-grade optimizations
>
> **Phase 1:** ~8,000 lines — Production TCP server (epoll, thread pool, lock-free IPC, RAII)
> **Phase 2:** ~4,000 lines — HFT extensions (binary protocol, order book, arena allocator, busy-poll, io_uring, nanosecond profiling)

---

## What We're Building

**Phase 1** — A production-grade TCP server that handles **10,000+ concurrent connections** using event-driven I/O (epoll) with edge-triggered notifications, a lock-free message queue between I/O and worker threads, and RAII resource management with zero leaks. This teaches how every high-performance server works (Nginx, Redis, Node.js) from scratch.

**Phase 2** — Extend the server with trading infrastructure concepts: replace HTTP with a zero-copy binary protocol, add a limit order book matching engine, eliminate heap allocation with arena allocators, replace epoll with busy-poll and io_uring for kernel-bypass-lite I/O, and instrument everything with nanosecond-precision latency tracking. This bridges the gap from "systems programmer" to "low-latency engineer."

```
Phase 1: HTTP Server                     Phase 2: HFT Extensions
─────────────────────                    ──────────────────────────

     10,000+ clients                     Trading clients (binary protocol)
          │                                        │
          ▼                                        ▼
┌────────────────────┐              ┌──────────────────────────┐
│   ACCEPT THREAD    │              │   BUSY-POLL / IO_URING   │
│                    │              │                          │
│  epoll_wait()      │              │  no syscalls in hot path │
│  accept() new conn │              │  spin on NIC ring buffer │
│  register fd       │              │  zero-copy receive       │
└────────┬───────────┘              └────────────┬─────────────┘
         │                                       │
         ▼                                       ▼
┌────────────────────┐              ┌──────────────────────────┐
│   I/O REACTOR      │              │   BINARY PARSER          │
│   (epoll loop)     │              │                          │
│                    │              │  reinterpret_cast<Msg*>  │
│  edge-triggered    │              │  fixed-size structs      │
│  non-blocking read │              │  zero parsing overhead   │
│  parse request     │              └────────────┬─────────────┘
│  push to queue     │                           │
└────────┬───────────┘                           ▼
         │                          ┌──────────────────────────┐
 lock-free MPSC queue               │   ORDER BOOK ENGINE      │
         │                          │                          │
  ┌──────┼──────┐                   │  price levels (sorted)   │
  ▼      ▼      ▼                   │  match buy/sell orders   │
┌────┐ ┌────┐ ┌────┐               │  emit fills/cancels      │
│ W1 │ │ W2 │ │ WN │               └────────────┬─────────────┘
└────┘ └────┘ └────┘                             │
  │      │      │                                ▼
  └──────┼──────┘               ┌──────────────────────────────┐
         │                      │   LATENCY TRACKER            │
   response queue               │                              │
         │                      │  rdtsc timestamps per stage  │
         ▼                      │  p50/p95/p99 histograms      │
┌────────────────────┐          │  wire-to-decision: ~5 μs     │
│   I/O REACTOR      │          └──────────────────────────────┘
│  flush response    │
└────────────────────┘
```

---

## Architecture Overview

### Phase 1 — HTTP Server Foundations (~8,000 lines)

| # | Layer | What It Does | Lines |
|---|-------|-------------|-------|
| 1 | **Socket & RAII** | File descriptor wrappers, socket options, address helpers | ~800 |
| 2 | **Event Loop (epoll)** | Edge-triggered reactor, non-blocking I/O, connection lifecycle | ~1,500 |
| 3 | **Thread Pool & Lock-Free Queue** | Worker threads, MPSC queue with CAS, task scheduling | ~1,200 |
| 4 | **Protocol (HTTP/1.1)** | Request parser, response builder, routing, static files | ~1,800 |
| 5 | **Server & Config** | Server orchestrator, config loading, graceful shutdown | ~800 |
| -- | **Tests & Benchmarks** | Unit, integration, stress, load testing | ~1,900 |
| | | **Phase 1 Total** | **~8,000** |

### Phase 2 — HFT Extensions (~4,000 lines)

| # | Layer | What It Does | Lines |
|---|-------|-------------|-------|
| 6 | **Binary Protocol** | Fixed-size message structs, zero-copy parsing, no string scanning | ~600 |
| 7 | **Order Book Engine** | Limit order book, price levels, matching engine, fill/cancel events | ~1,200 |
| 8 | **Arena Allocator** | Pre-allocated memory pools, zero malloc in hot path, slab allocation | ~500 |
| 9 | **Busy-Poll & io_uring** | Kernel-bypass-lite I/O, submission/completion rings, CPU pinning | ~800 |
| 10 | **Latency Infrastructure** | rdtsc nanosecond timestamps, per-stage tracking, histogram reporting | ~500 |
| -- | **Tests & Benchmarks** | HFT-specific tests, latency benchmarks, correctness proofs | ~400 |
| | | **Phase 2 Total** | **~4,000** |
| | | **Grand Total** | **~12,000** |

---

## Complete Folder Tree

```
stormsocket/
│
├── CMakeLists.txt                                    # Build: GTest, Google Benchmark, threads, liburing
│
├── include/storm/
│   │
│   ├── core/                                         ── LAYER 1: SOCKET & RAII ──
│   │   ├── types.hpp                                 # ConnectionId, Port, BufferView, Result<T>
│   │   ├── file_descriptor.hpp                       # RAII wrapper for fd (close on destroy)
│   │   ├── socket.hpp                                # TCP socket: create, bind, listen, accept
│   │   ├── socket_options.hpp                        # SO_REUSEADDR, SO_REUSEPORT, TCP_NODELAY
│   │   ├── address.hpp                               # IPv4/IPv6 address + port wrapper
│   │   ├── buffer.hpp                                # Read/write buffer with automatic resize
│   │   ├── connection.hpp                            # Connection state: fd, buffers, state, metadata
│   │   └── connection_pool.hpp                       # Pre-allocated connection objects (slab alloc)
│   │
│   ├── reactor/                                      ── LAYER 2: EVENT LOOP (EPOLL) ──
│   │   ├── epoll_reactor.hpp                         # epoll_create, epoll_ctl, epoll_wait loop
│   │   ├── event_handler.hpp                         # Callback interface: on_read, on_write, on_close
│   │   ├── acceptor.hpp                              # Accept new connections, register with epoll
│   │   ├── io_handler.hpp                            # Non-blocking read/write with edge-triggered
│   │   ├── timer.hpp                                 # timerfd-based periodic events (keepalive, stats)
│   │   └── signal_handler.hpp                        # signalfd for SIGINT/SIGTERM graceful shutdown
│   │
│   ├── threading/                                    ── LAYER 3: THREAD POOL & LOCK-FREE QUEUE ──
│   │   ├── mpsc_queue.hpp                            # Lock-free multi-producer single-consumer queue
│   │   ├── spsc_queue.hpp                            # Lock-free single-producer single-consumer queue
│   │   ├── thread_pool.hpp                           # Fixed-size worker pool with task stealing
│   │   ├── task.hpp                                  # Task wrapper: request + connection ref
│   │   └── worker.hpp                                # Worker thread: pop task → process → push response
│   │
│   ├── http/                                         ── LAYER 4: HTTP/1.1 PROTOCOL ──
│   │   ├── request.hpp                               # Parsed HTTP request (method, path, headers, body)
│   │   ├── request_parser.hpp                        # Incremental HTTP parser (handles partial reads)
│   │   ├── response.hpp                              # HTTP response builder (status, headers, body)
│   │   ├── response_writer.hpp                       # Serialize response to buffer
│   │   ├── router.hpp                                # URL routing: path → handler function
│   │   ├── handler.hpp                               # Handler interface: (Request) → Response
│   │   ├── static_file_handler.hpp                   # Serve files from directory (sendfile())
│   │   ├── status_codes.hpp                          # HTTP status codes (200, 404, 500, etc.)
│   │   └── mime_types.hpp                            # File extension → Content-Type mapping
│   │
│   ├── server/                                       ── LAYER 5: SERVER & CONFIG ──
│   │   ├── server.hpp                                # Top-level: create reactor + pool, run, stop
│   │   ├── server_config.hpp                         # Config struct: port, workers, max_conn, timeouts
│   │   ├── config_loader.hpp                         # Parse YAML/JSON config file
│   │   └── stats.hpp                                 # Live stats: connections, requests, latency, errors
│   │
│   ├── trading/                                      ── LAYER 6: BINARY PROTOCOL ──
│   │   ├── messages.hpp                              # Fixed-size message structs (NewOrder, Cancel, Fill)
│   │   ├── message_parser.hpp                        # reinterpret_cast parser, zero-copy, length-prefixed
│   │   ├── message_writer.hpp                        # Serialize structs directly to wire
│   │   └── protocol_handler.hpp                      # Binary protocol I/O handler (replaces HTTP path)
│   │
│   ├── orderbook/                                    ── LAYER 7: ORDER BOOK ENGINE ──
│   │   ├── order.hpp                                 # Order struct: id, side, price, qty, timestamp
│   │   ├── price_level.hpp                           # Orders at same price (intrusive linked list)
│   │   ├── order_book.hpp                            # Bid/ask sides, add/cancel/match operations
│   │   ├── matching_engine.hpp                       # Match crossing orders, emit fills
│   │   └── book_event.hpp                            # Fill, Cancel, Trade event types
│   │
│   ├── memory/                                       ── LAYER 8: ARENA ALLOCATOR ──
│   │   ├── arena.hpp                                 # Bump allocator: pre-allocate, no free, reset all
│   │   ├── pool_allocator.hpp                        # Fixed-size object pool (orders, connections)
│   │   └── aligned_alloc.hpp                         # Cache-line aligned allocation helpers
│   │
│   ├── fast_io/                                      ── LAYER 9: BUSY-POLL & IO_URING ──
│   │   ├── busy_poll_reactor.hpp                     # Spin-loop reactor: poll without syscalls
│   │   ├── io_uring_reactor.hpp                      # io_uring submission/completion ring wrapper
│   │   ├── cpu_affinity.hpp                          # Pin thread to isolated core
│   │   └── reactor_factory.hpp                       # Choose epoll vs busy-poll vs io_uring at runtime
│   │
│   ├── latency/                                      ── LAYER 10: LATENCY INFRASTRUCTURE ──
│   │   ├── timestamp.hpp                             # rdtsc cycle counter, ns conversion
│   │   ├── latency_tracker.hpp                       # Per-stage timestamps: wire → parse → process → send
│   │   ├── histogram.hpp                             # HDR histogram: p50, p95, p99, p99.9
│   │   └── latency_reporter.hpp                      # Periodic latency stats dump
│   │
│   └── utils/                                        ── UTILITIES ──
│       ├── logger.hpp                                # Structured logging (level, timestamp, thread_id)
│       ├── error.hpp                                 # Error type with errno translation
│       ├── defer.hpp                                 # RAII defer macro (cleanup on scope exit)
│       └── metrics.hpp                               # Atomic counters for connections, requests, bytes
│
├── src/
│   ├── main.cpp                                      # Entry: parse args → load config → server.run()
│   │
│   ├── core/
│   │   ├── file_descriptor.cpp                       # RAII fd: close(), release(), duplicate()
│   │   ├── socket.cpp                                # create_tcp(), bind(), listen(), accept4()
│   │   ├── socket_options.cpp                        # Apply options: nodelay, reuseaddr, keepalive
│   │   ├── address.cpp                               # Resolve hostname, format for logging
│   │   ├── buffer.cpp                                # Dynamic buffer: append, consume, compact
│   │   ├── connection.cpp                            # Connection lifecycle: init, reset, state transitions
│   │   └── connection_pool.cpp                       # Pre-allocate N connections, recycle on close
│   │
│   ├── reactor/
│   │   ├── epoll_reactor.cpp                         # Main event loop: epoll_wait → dispatch handlers
│   │   ├── acceptor.cpp                              # Accept loop: accept4(NONBLOCK) → register fd
│   │   ├── io_handler.cpp                            # Edge-triggered: read until EAGAIN, write until done
│   │   ├── timer.cpp                                 # timerfd_create → periodic stats/keepalive
│   │   └── signal_handler.cpp                        # signalfd → graceful shutdown on SIGINT
│   │
│   ├── threading/
│   │   ├── mpsc_queue.cpp                            # CAS-based enqueue, single-consumer dequeue
│   │   ├── thread_pool.cpp                           # Spawn workers, distribute tasks, join on shutdown
│   │   └── worker.cpp                                # Worker loop: wait → process → respond
│   │
│   ├── http/
│   │   ├── request_parser.cpp                        # State-machine parser: METHOD → PATH → HEADERS → BODY
│   │   ├── response.cpp                              # Build response: status + headers + body
│   │   ├── response_writer.cpp                       # Serialize to wire format
│   │   ├── router.cpp                                # Trie-based URL matching with path parameters
│   │   ├── static_file_handler.cpp                   # open() → sendfile() → close(), directory listing
│   │   └── mime_types.cpp                            # Extension lookup table
│   │
│   ├── server/
│   │   ├── server.cpp                                # Wire everything: reactor + pool + router → run()
│   │   ├── config_loader.cpp                         # Parse YAML config, apply defaults
│   │   └── stats.cpp                                 # Collect and snapshot live metrics
│   │
│   ├── trading/
│   │   ├── message_parser.cpp                        # Cast bytes to struct, validate length prefix
│   │   ├── message_writer.cpp                        # memcpy struct to wire buffer
│   │   └── protocol_handler.cpp                      # Binary protocol dispatch: msg type → handler
│   │
│   ├── orderbook/
│   │   ├── order_book.cpp                            # Add/cancel orders, maintain sorted price levels
│   │   └── matching_engine.cpp                       # Price-time priority matching, generate fills
│   │
│   ├── memory/
│   │   ├── arena.cpp                                 # Bump allocator implementation
│   │   └── pool_allocator.cpp                        # Free-list object pool
│   │
│   ├── fast_io/
│   │   ├── busy_poll_reactor.cpp                     # Spin-loop: check fd readability without syscall
│   │   ├── io_uring_reactor.cpp                      # Setup rings, submit reads/writes, reap completions
│   │   └── cpu_affinity.cpp                          # sched_setaffinity, isolcpus integration
│   │
│   └── latency/
│       ├── timestamp.cpp                             # rdtsc calibration, TSC → nanoseconds
│       ├── latency_tracker.cpp                       # Stamp each processing stage
│       ├── histogram.cpp                             # Record values, compute percentiles
│       └── latency_reporter.cpp                      # Periodic dump to log/file
│
├── tests/
│   ├── core/
│   │   ├── test_file_descriptor.cpp                  # RAII: fd closes on destroy, move semantics work
│   │   ├── test_buffer.cpp                           # Append, consume, compact, resize
│   │   ├── test_connection.cpp                       # State transitions, reset reuse
│   │   └── test_connection_pool.cpp                  # Allocate, return, exhaust pool
│   │
│   ├── reactor/
│   │   ├── test_epoll_reactor.cpp                    # Register/unregister fds, event dispatch
│   │   └── test_timer.cpp                            # Timer fires at expected intervals
│   │
│   ├── threading/
│   │   ├── test_mpsc_queue.cpp                       # Multi-producer concurrent push, single pop
│   │   ├── test_spsc_queue.cpp                       # Producer/consumer throughput
│   │   └── test_thread_pool.cpp                      # Submit tasks, verify all complete
│   │
│   ├── http/
│   │   ├── test_request_parser.cpp                   # Valid requests, partial reads, malformed input
│   │   ├── test_response.cpp                         # Build response, verify wire format
│   │   ├── test_router.cpp                           # Path matching, parameters, 404 fallback
│   │   └── test_static_file_handler.cpp              # Serve file, 404 for missing, mime types
│   │
│   ├── trading/                                      # ── Phase 2 tests ──
│   │   ├── test_messages.cpp                         # Struct sizes, packing, endianness
│   │   └── test_message_parser.cpp                   # Parse valid, reject malformed, partial messages
│   │
│   ├── orderbook/
│   │   ├── test_order_book.cpp                       # Add/cancel, best bid/ask, price level ordering
│   │   └── test_matching_engine.cpp                  # Cross orders → fills, partial fills, self-trade prevention
│   │
│   ├── memory/
│   │   ├── test_arena.cpp                            # Allocate, reset, alignment guarantees
│   │   └── test_pool_allocator.cpp                   # Alloc/free cycle, exhaustion, reuse
│   │
│   ├── fast_io/
│   │   └── test_io_uring_reactor.cpp                 # Submit/reap cycle, multi-fd, error paths
│   │
│   ├── latency/
│   │   ├── test_timestamp.cpp                        # rdtsc monotonicity, calibration accuracy
│   │   └── test_histogram.cpp                        # Percentile correctness, edge cases
│   │
│   └── integration/
│       ├── test_echo_server.cpp                      # Connect → send → receive echo → verify
│       ├── test_concurrent_clients.cpp               # 1000 clients sending simultaneously
│       ├── test_graceful_shutdown.cpp                 # SIGINT during active connections → clean close
│       ├── test_resource_leaks.cpp                   # Run under Valgrind, verify zero leaks
│       └── test_trading_e2e.cpp                      # Binary client → order → match → fill → verify
│
├── bench/
│   ├── bench_mpsc_queue.cpp                          # Queue throughput: push/pop ops per second
│   ├── bench_request_parser.cpp                      # Parse throughput: requests per second
│   ├── bench_connection_throughput.cpp                # End-to-end: connect → request → response → close
│   ├── bench_concurrent_load.cpp                     # 10k connections, sustained request rate
│   ├── bench_order_book.cpp                          # Orders/sec: add, cancel, match throughput
│   ├── bench_arena_vs_malloc.cpp                     # Arena allocator vs new/delete: ops/sec, latency
│   ├── bench_binary_vs_http.cpp                      # Binary protocol vs HTTP parsing: throughput comparison
│   └── bench_latency_pipeline.cpp                    # Full pipeline wire-to-wire latency distribution
│
├── config/
│   ├── default.yaml                                  # Default server config (HTTP mode)
│   ├── stress.yaml                                   # High-connection stress config
│   └── trading.yaml                                  # Trading mode: binary protocol, busy-poll, pinned cores
│
├── scripts/
│   ├── load_test.sh                                  # wrk / ab load testing commands
│   ├── valgrind_check.sh                             # Run with Valgrind + ASan
│   └── latency_report.sh                             # Parse latency logs, generate p50/p95/p99 summary
│
└── docs/
    └── ARCHITECTURE.md                               # Design doc
```

---

# PHASE 1: HTTP Server Foundations

---

## Layer 1: Socket & RAII (~800 lines)

Everything wraps a raw file descriptor. No fd ever escapes without RAII.

```cpp
// FileDescriptor — the foundation. Every socket, epoll, timer, signal uses this.
class FileDescriptor {
    int fd_ = -1;
public:
    explicit FileDescriptor(int fd);
    ~FileDescriptor();                      // calls close(fd_)
    FileDescriptor(FileDescriptor&&);       // move OK
    FileDescriptor(const FileDescriptor&) = delete;  // no copy

    int get() const;
    int release();                          // give up ownership
    void reset(int new_fd = -1);            // close old, take new
    explicit operator bool() const;
};
// Guarantee: if you have a FileDescriptor, the fd will be closed.
// Verified: Valgrind shows zero fd leaks across all tests.
```

**Connection state machine:**
```
  ┌───────────┐   accept()   ┌───────────┐   headers    ┌───────────┐
  │  ACCEPTED │ ───────────▶ │  READING  │ ──────────▶  │  ROUTING  │
  └───────────┘              └───────────┘              └───────────┘
                                  │                          │
                              EAGAIN                     dispatched
                              (partial)                  to worker
                                  │                          │
                                  ▼                          ▼
                             ┌───────────┐             ┌───────────┐
                             │  WAITING  │             │ PROCESSING│
                             │ (epoll)   │             │ (worker)  │
                             └───────────┘             └───────────┘
                                                            │
                                                        response ready
                                                            │
                                                            ▼
                                                       ┌───────────┐
                          close()                      │  WRITING  │
                    ┌──────────────────────────────────│           │
                    │                                  └───────────┘
                    ▼                                       │
               ┌───────────┐                           EAGAIN
               │  CLOSED   │                           (partial)
               │ (recycle) │                               │
               └───────────┘                               ▼
                                                      ┌───────────┐
                                                      │  WAITING  │
                                                      │  (epoll)  │
                                                      └───────────┘
```

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `types.hpp` | ~60 | `ConnectionId`, `Port`, `BufferView{ptr, len}`, `Result<T>` with error |
| `file_descriptor.hpp/cpp` | ~120 | RAII fd wrapper: close on destroy, move-only, no copy |
| `socket.hpp/cpp` | ~150 | `create_tcp()`, `bind()`, `listen()`, `accept4(SOCK_NONBLOCK)`, `set_nonblocking()` |
| `socket_options.hpp/cpp` | ~80 | `set_reuseaddr()`, `set_reuseport()`, `set_nodelay()`, `set_keepalive()`, `set_sndbuf()` |
| `address.hpp/cpp` | ~80 | `Address(ip, port)`, `resolve(hostname)`, `to_string()`, `sockaddr_in` conversion |
| `buffer.hpp/cpp` | ~150 | Dynamic buffer: `append(data, len)`, `consume(n)`, `compact()`, `data()`, `readable()`, `writable()` |
| `connection.hpp/cpp` | ~120 | `Connection{fd, read_buf, write_buf, state, remote_addr, created_at}` |
| `connection_pool.hpp/cpp` | ~100 | Pre-allocate N connections, `acquire() → Connection*`, `release(conn)` |

---

## Layer 2: Event Loop — epoll (~1,500 lines)

The heart. One thread runs the epoll loop, dispatching events to handlers.

```
  ┌─────────────────────────────────────────────────────────────┐
  │                    EPOLL REACTOR                             │
  │                                                             │
  │  epoll_create1(EPOLL_CLOEXEC)                               │
  │       │                                                     │
  │       ▼                                                     │
  │  ┌─── Event Loop ──────────────────────────────────────┐    │
  │  │                                                      │    │
  │  │  while (running_) {                                  │    │
  │  │      n = epoll_wait(epfd, events, MAX, timeout);     │    │
  │  │      for (i = 0; i < n; i++) {                       │    │
  │  │          if (events[i] is listen_fd)                  │    │
  │  │              → acceptor.on_accept()                   │    │
  │  │          else if (events[i] is timerfd)               │    │
  │  │              → timer.on_tick()                        │    │
  │  │          else if (events[i] is signalfd)              │    │
  │  │              → signal.on_signal() → stop             │    │
  │  │          else if (EPOLLIN)                            │    │
  │  │              → io_handler.on_readable(fd)             │    │
  │  │          else if (EPOLLOUT)                           │    │
  │  │              → io_handler.on_writable(fd)             │    │
  │  │          else if (EPOLLERR | EPOLLHUP)                │    │
  │  │              → connection.close()                     │    │
  │  │      }                                               │    │
  │  │  }                                                   │    │
  │  └──────────────────────────────────────────────────────┘    │
  │                                                             │
  └─────────────────────────────────────────────────────────────┘
```

**Edge-triggered vs Level-triggered:**
```
  Level-triggered (default):
    epoll says "fd is readable" every time you call epoll_wait
    even if you didn't read yet → wastes CPU cycles

  Edge-triggered (EPOLLET — what we use):
    epoll says "fd BECAME readable" only once
    you MUST read until EAGAIN or you miss data forever
    → faster, but requires careful non-blocking read loop:

    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) { buffer.append(buf, n); continue; }
        if (n == 0) { close(fd); break; }          // client disconnected
        if (errno == EAGAIN) break;                 // no more data right now
        if (errno == EINTR) continue;               // interrupted, retry
        perror("read"); close(fd); break;           // real error
    }
```

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `epoll_reactor.hpp/cpp` | ~400 | `EpollReactor()`: create epoll, `add_fd(fd, events)`, `modify_fd()`, `remove_fd()`, `run()` main loop, `stop()` |
| `event_handler.hpp` | ~50 | `interface EventHandler { on_readable(fd), on_writable(fd), on_error(fd) }` |
| `acceptor.hpp/cpp` | ~200 | `Acceptor(listen_fd, reactor)`: `accept4(SOCK_NONBLOCK | SOCK_CLOEXEC)`, register new fd with `EPOLLIN | EPOLLET`, create Connection |
| `io_handler.hpp/cpp` | ~400 | `IoHandler(reactor, queue)`: edge-triggered read loop (read until EAGAIN), parse request, push to worker queue. Write loop: flush write buffer, re-arm EPOLLOUT if partial |
| `timer.hpp/cpp` | ~150 | `Timer(interval_ms)`: `timerfd_create()`, `timerfd_settime()`, periodic callback for stats/keepalive/timeout cleanup |
| `signal_handler.hpp/cpp` | ~100 | `SignalHandler()`: block SIGINT/SIGTERM, `signalfd()`, read signal → set `running_ = false` |

---

## Layer 3: Thread Pool & Lock-Free Queue (~1,200 lines)

The I/O thread and worker threads communicate through **lock-free queues** — no mutex, no contention.

```
  I/O Thread                          Worker Threads
  ┌─────────────┐                     ┌─────────────┐
  │ parse       │                     │ process     │
  │ request     │                     │ request     │
  │             │                     │             │
  │ Task{       │    MPSC Queue       │ build       │
  │  conn_id,   │ ─────────────────▶  │ response    │
  │  request    │  (lock-free CAS)    │             │
  │ }           │                     │ Task{       │
  └─────────────┘                     │  conn_id,   │
                                      │  response   │
                     SPSC Queue       │ }           │
  ┌─────────────┐ ◀───────────────── └─────────────┘
  │ write       │  (one per worker)
  │ response    │
  │ back to     │
  │ client      │
  └─────────────┘
```

**MPSC Queue (Multi-Producer Single-Consumer):**
```
  Multiple workers push responses simultaneously
  I/O thread pops them one at a time

  push(item):
      new_node = allocate(item)
      loop:
          old_head = head_.load(acquire)
          new_node->next = old_head
          if head_.compare_exchange_weak(old_head, new_node, release):
              break    // success, no mutex needed

  pop():
      loop:
          old_head = head_.load(acquire)
          if old_head == nullptr: return empty
          if head_.compare_exchange_weak(old_head, old_head->next, release):
              return old_head->data
```

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `mpsc_queue.hpp` | ~200 | Lock-free multi-producer single-consumer queue using atomic CAS. `push()` from any thread, `pop()` from consumer thread. Node-based (linked list). ABA prevention with tagged pointers |
| `spsc_queue.hpp` | ~180 | Lock-free single-producer single-consumer ring buffer. Power-of-2 capacity, index masking, `alignas(64)` head/tail. Used for per-worker response queues |
| `thread_pool.hpp/cpp` | ~300 | `ThreadPool(num_workers)`. `start()` spawns workers, `submit(task)` pushes to MPSC queue. `stop()` signals + joins. Optional work-stealing between workers |
| `task.hpp` | ~80 | `struct Task { ConnectionId conn_id; Request request; }`. `struct ResponseTask { ConnectionId conn_id; Response response; }` |
| `worker.hpp/cpp` | ~200 | Worker thread loop: `pop task → route(request) → handler(request) → build response → push response back`. Backpressure: yield if response queue full |

---

## Layer 4: HTTP/1.1 Protocol (~1,800 lines)

Incremental parser handles partial reads (TCP doesn't guarantee full messages).

```
  Raw bytes from socket
       │
       ▼
  ┌─── Request Parser (state machine) ──────────────────────┐
  │                                                          │
  │  State: METHOD → reading "GET "                          │
  │  State: PATH → reading "/api/users HTTP/1.1\r\n"        │
  │  State: HEADERS → reading "Host: ...\r\n" until "\r\n"  │
  │  State: BODY → reading Content-Length bytes               │
  │  State: COMPLETE → Request is ready                       │
  │                                                          │
  │  Handles:                                                │
  │    - Partial reads (TCP fragmentation)                   │
  │    - Pipelined requests (multiple requests per conn)     │
  │    - Malformed input (reject with 400 Bad Request)       │
  │    - Header size limits (prevent memory exhaustion)       │
  │                                                          │
  └──────────────────────────────────────────────────────────┘
       │
       ▼
  Request { method: GET, path: "/api/users", headers: {...}, body: "" }
       │
       ▼
  ┌─── Router ──────────────────────────────────────────────┐
  │                                                          │
  │  GET  /              → index_handler                     │
  │  GET  /api/health    → health_handler                    │
  │  GET  /api/stats     → stats_handler                     │
  │  GET  /static/*      → static_file_handler               │
  │  POST /api/echo      → echo_handler                      │
  │  *    *              → 404_handler                        │
  │                                                          │
  └──────────────────────────────────────────────────────────┘
       │
       ▼
  Response { status: 200, headers: {...}, body: "..." }
       │
       ▼
  ┌─── Response Writer ─────────────────────────────────────┐
  │                                                          │
  │  "HTTP/1.1 200 OK\r\n"                                   │
  │  "Content-Type: application/json\r\n"                    │
  │  "Content-Length: 42\r\n"                                 │
  │  "Connection: keep-alive\r\n"                            │
  │  "\r\n"                                                  │
  │  "{\"status\":\"ok\",\"connections\":1234}"              │
  │                                                          │
  └──────────────────────────────────────────────────────────┘
```

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `request.hpp` | ~80 | `struct Request { Method method; string path; Headers headers; string body; string query_string; }`. `enum Method { GET, POST, PUT, DELETE, HEAD, OPTIONS }` |
| `request_parser.hpp/cpp` | ~400 | Incremental state-machine parser. `feed(data, len) → ParseResult{Complete, Incomplete, Error}`. Handles partial TCP reads across multiple `feed()` calls. Max header size = 8KB, max body = 1MB |
| `response.hpp/cpp` | ~200 | `Response::ok(body)`, `Response::not_found()`, `Response::json(data)`, `Response::file(path)`. Builder pattern: `Response().status(200).header("X-Custom", "val").body(data)` |
| `response_writer.hpp/cpp` | ~150 | `serialize(Response) → Buffer`. Wire format: status line + headers + CRLF + body. Handles chunked encoding for streaming |
| `router.hpp/cpp` | ~250 | `Router::get("/path", handler)`, `Router::post(...)`. Trie-based path matching. Path parameters: `/users/:id` → extracts `id`. Middleware chain support |
| `handler.hpp` | ~40 | `using Handler = std::function<Response(const Request&)>`. Simple, testable |
| `static_file_handler.hpp/cpp` | ~200 | Serve files from directory. `sendfile()` for zero-copy. Directory listing. ETag caching. Range requests for large files |
| `status_codes.hpp` | ~60 | `status_text(200) → "OK"`, `status_text(404) → "Not Found"`. All standard HTTP status codes |
| `mime_types.hpp/cpp` | ~80 | `.html → text/html`, `.json → application/json`, `.css → text/css`. Lookup table |

---

## Layer 5: Server & Config (~800 lines)

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `server.hpp/cpp` | ~300 | `Server(config)`. `start()`: create listen socket → acceptor → reactor → thread pool → register routes → run. `stop()`: signal reactor → drain queues → join threads → close listener. Graceful shutdown: finish in-flight requests before closing |
| `server_config.hpp` | ~80 | `struct ServerConfig { port, num_workers, max_connections, read_timeout_ms, write_timeout_ms, max_header_size, max_body_size, static_dir, log_level }` |
| `config_loader.hpp/cpp` | ~120 | Parse YAML/JSON config file. Environment variable override: `STORM_PORT=8080`. Defaults for all fields |
| `stats.hpp/cpp` | ~200 | `struct Stats { atomic<uint64_t> total_connections, active_connections, total_requests, total_bytes_in, total_bytes_out, total_errors }`. `snapshot() → StatsSnapshot`. Latency histogram (p50/p95/p99) |

---

## Utilities (~300 lines)

| File | Lines | What It Does |
|------|-------|-------------|
| `logger.hpp/cpp` | ~120 | `LOG_INFO("msg", key, val)`. Structured JSON logging. Levels: DEBUG, INFO, WARN, ERROR. Thread-safe |
| `error.hpp` | ~60 | `Error{code, message}`. `Error::from_errno()` translates errno to human-readable. `Result<T> = variant<T, Error>` |
| `defer.hpp` | ~30 | `DEFER { cleanup_code; };` — runs on scope exit. Used for fd cleanup in non-RAII paths |
| `metrics.hpp` | ~80 | Atomic counters with `increment()`, `get()`, `reset()`. Used by stats |

---

# PHASE 2: HFT Extensions

> Phase 2 takes the working HTTP server and layers trading-grade optimizations on top.
> Each extension directly replaces or augments a Phase 1 component, so you can
> A/B benchmark the before and after.

---

## Layer 6: Binary Protocol (~600 lines)

Replace HTTP text parsing with fixed-size structs. **Zero parsing cost.**

```
  Phase 1 (HTTP):                        Phase 2 (Binary):
  ─────────────────                      ────────────────────

  "POST /api/order HTTP/1.1\r\n"         ┌──────────────────────┐
  "Content-Type: application/json\r\n"   │ len: 4 bytes         │
  "Content-Length: 89\r\n"               │ type: 1 byte ('N')   │
  "\r\n"                                 │ order_id: 8 bytes    │
  "{\"side\":\"buy\",                    │ symbol_id: 4 bytes   │
    \"symbol\":\"AAPL\",                 │ side: 1 byte         │
    \"price\":185.50,                    │ price: 4 bytes       │
    \"qty\":100}"                        │ quantity: 4 bytes    │
                                         └──────────────────────┘
  ~200 bytes, state machine parse          22 bytes, reinterpret_cast
  ~500 ns to parse                         ~5 ns to parse
```

```cpp
// The message structs — packed, fixed-size, no strings
struct __attribute__((packed)) MessageHeader {
    uint32_t length;        // total message length including header
    uint8_t  type;          // 'N' = NewOrder, 'C' = Cancel, 'F' = Fill
};

struct __attribute__((packed)) NewOrderMessage {
    MessageHeader header;
    uint64_t order_id;
    uint32_t symbol_id;     // numeric symbol lookup, not "AAPL" string
    uint8_t  side;          // 0 = buy, 1 = sell
    int32_t  price;         // fixed-point: price * 10000 (185.50 → 1855000)
    uint32_t quantity;
};

struct __attribute__((packed)) CancelMessage {
    MessageHeader header;
    uint64_t order_id;
};

struct __attribute__((packed)) FillMessage {
    MessageHeader header;
    uint64_t order_id;
    uint64_t match_id;
    int32_t  fill_price;
    uint32_t fill_qty;
    uint64_t timestamp_ns;
};

// "Parsing" — cast the pointer, done
const auto* msg = reinterpret_cast<const NewOrderMessage*>(buffer.data());
// No state machine. No string scanning. No branching.
```

**Why this matters:** HTTP parsing is the #1 CPU consumer in Phase 1. Replacing it with `reinterpret_cast` eliminates parsing entirely. This is exactly what real exchange protocols (ITCH, OUCH, BATS PITCH) do.

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `messages.hpp` | ~150 | All message struct definitions. `static_assert(sizeof(NewOrderMessage) == 26)` to catch packing errors. Endianness helpers |
| `message_parser.hpp/cpp` | ~200 | Read length prefix → validate bounds → `reinterpret_cast`. Handles partial messages (TCP fragmentation still happens). Reject oversized/malformed |
| `message_writer.hpp/cpp` | ~100 | `serialize(msg) → memcpy to buffer`. Fill in length prefix. Zero-copy where possible |
| `protocol_handler.hpp/cpp` | ~150 | Binary-mode I/O handler. Replaces HTTP `IoHandler` path. Dispatches by `header.type`: 'N' → order book, 'C' → cancel |

---

## Layer 7: Order Book Engine (~1,200 lines)

A limit order book — the core data structure of every exchange and every HFT firm.

```
  Incoming NewOrder(BUY, AAPL, 185.50, 100)
       │
       ▼
  ┌─── Order Book (AAPL) ──────────────────────────────────┐
  │                                                          │
  │  ASK (sell) side          │  BID (buy) side              │
  │  (ascending price)       │  (descending price)          │
  │                          │                              │
  │  186.00 ── [sell 200]    │  185.50 ── [buy 100] ← NEW  │
  │  185.75 ── [sell 150]    │  185.25 ── [buy 300]         │
  │  185.50 ── [sell 50]     │  185.00 ── [buy 500]         │
  │           ↑              │                              │
  │       best ask           │       best bid ↑             │
  │                          │                              │
  └──────────────────────────────────────────────────────────┘
       │
       ▼
  Does best_ask (185.50) <= new buy price (185.50)?
  YES → MATCH! Fill 50 shares at 185.50
       │
       ▼
  Remaining: buy 50 shares at 185.50 rests on the book
```

```cpp
// Order: one entry in the book
struct Order {
    uint64_t id;
    uint32_t symbol_id;
    Side     side;              // BUY or SELL
    int32_t  price;             // fixed-point
    uint32_t remaining_qty;
    uint64_t timestamp_ns;      // for price-time priority
    Order*   next;              // intrusive linked list at same price level
    Order*   prev;
};

// PriceLevel: all orders at one price, doubly-linked for O(1) remove
struct PriceLevel {
    int32_t     price;
    uint32_t    total_qty;      // sum of all orders at this price
    uint32_t    order_count;
    Order*      head;           // oldest order (first to fill)
    Order*      tail;           // newest order
    PriceLevel* next;           // next worse price level
    PriceLevel* prev;           // next better price level
};

// OrderBook: sorted price levels, bid and ask side
class OrderBook {
    PriceLevel* best_bid_;      // highest buy price
    PriceLevel* best_ask_;      // lowest sell price

public:
    MatchResult add_order(Order& order);
    void        cancel_order(uint64_t order_id);
    Spread      spread() const;  // best_ask - best_bid
};

// MatchingEngine: price-time priority
// 1. Match against opposite side while price crosses
// 2. Oldest order at best price fills first
// 3. Remaining quantity rests on the book
```

**Why intrusive linked lists?** No `std::list` (heap allocs per node). Orders are pre-allocated from the arena (Layer 8). The `next`/`prev` pointers live inside the `Order` struct itself — zero allocation to add/remove.

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `order.hpp` | ~80 | `Order` struct with intrusive list pointers. `Side` enum. Fixed-point price utilities |
| `price_level.hpp/cpp` | ~200 | Intrusive doubly-linked list of orders. `add_order()`, `remove_order()`, `total_qty()`. O(1) insert/remove |
| `order_book.hpp/cpp` | ~500 | Bid/ask sides as sorted price level chains. `add_order()` → check for match → rest remainder. `cancel_order()` → O(1) remove via order pointer. `best_bid()`, `best_ask()`, `spread()` |
| `matching_engine.hpp/cpp` | ~300 | Wraps `OrderBook`. `process_order(NewOrderMessage) → vector<Fill>`. Price-time priority matching. Handles partial fills. Self-trade prevention (optional) |
| `book_event.hpp` | ~80 | `Fill{buyer_id, seller_id, price, qty, timestamp}`, `Cancel{order_id, reason}`, `Trade{symbol, price, qty}` event types |

---

## Layer 8: Arena Allocator (~500 lines)

Replace `new`/`delete` with pre-allocated memory. **Zero heap allocation in the hot path.**

```
  Phase 1 (malloc):                      Phase 2 (arena):
  ─────────────────                      ────────────────────

  auto* conn = new Connection();         auto* conn = arena.allocate<Connection>();
  // malloc → brk/mmap syscall           // bump a pointer, no syscall
  // ~100-500 ns                         // ~2 ns

  delete conn;                           arena.reset();
  // free → coalesce → maybe munmap      // set offset to 0, done
  // ~50-200 ns                          // ~1 ns
```

```cpp
// Arena: bump allocator — fast, no fragmentation, bulk free
class Arena {
    alignas(64) char* pool_;
    size_t capacity_;
    size_t offset_ = 0;

public:
    explicit Arena(size_t capacity)
        : pool_(static_cast<char*>(std::aligned_alloc(64, capacity)))
        , capacity_(capacity) {}

    ~Arena() { std::free(pool_); }

    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        size_t aligned = align_up(sizeof(T), alignof(T));
        void* ptr = pool_ + offset_;
        offset_ += aligned;
        return new (ptr) T(std::forward<Args>(args)...);  // placement new
    }

    void reset() { offset_ = 0; }  // "free" everything at once
};

// PoolAllocator: fixed-size object pool — for orders, connections
// Free-list based: allocate = pop head, deallocate = push head
// No fragmentation, no syscalls, O(1) alloc and free
template<typename T>
class PoolAllocator {
    struct FreeNode { FreeNode* next; };
    FreeNode* free_list_ = nullptr;
    Arena arena_;

public:
    T* allocate();              // pop from free list, or bump from arena
    void deallocate(T* ptr);    // push onto free list
};
```

**Where it's used:**
- `Order` objects: pre-allocate 100,000 orders, recycle on cancel/fill
- `Connection` objects: already done in Phase 1's `ConnectionPool`, now backed by arena
- `PriceLevel` objects: pre-allocate per-symbol, recycle when empty

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `arena.hpp/cpp` | ~200 | Bump allocator with `allocate<T>(args...)`, `reset()`, `bytes_used()`. Cache-line aligned. Grows by doubling if exhausted (configurable: grow vs fail) |
| `pool_allocator.hpp/cpp` | ~200 | Free-list object pool. `allocate() → T*`, `deallocate(T*)`. Falls back to arena. `pre_allocate(n)` warms the pool |
| `aligned_alloc.hpp` | ~80 | `align_up(size, alignment)`, `is_cache_aligned(ptr)`, `alignas(64)` wrappers |

---

## Layer 9: Busy-Poll & io_uring (~800 lines)

Replace `epoll_wait()` (kernel syscall, context switch) with **zero-syscall I/O**.

```
  Phase 1 (epoll):                       Phase 2 (io_uring):
  ─────────────────                      ────────────────────

  epoll_wait()   ← syscall               SQ ring ← userspace write
  read()         ← syscall               kernel picks up from ring
  write()        ← syscall               CQ ring ← userspace read
  = 3+ syscalls per request              = 0 syscalls in hot path

  Context switch cost: ~1-5 μs           Shared memory rings: ~100 ns
```

```
  Phase 1 (epoll):                       Phase 2 (busy-poll):
  ─────────────────                      ────────────────────

  epoll_wait(timeout)                    while (true) {
  // thread sleeps until event           //   check fd readability
  // kernel wakes it up                  //   never sleep
  // context switch penalty              //   never yield CPU
                                         //   burn entire core
  wake-up latency: 5-50 μs              // }
                                         // latency: ~100 ns
                                         // tradeoff: wastes a core
```

```cpp
// BusyPollReactor — spin-loop, no syscalls
class BusyPollReactor {
    int fd_;
    bool running_ = true;

public:
    void run() {
        set_thread_affinity(config_.pinned_core);  // isolate this core

        while (running_) {
            // Check for data without blocking
            struct pollfd pfd = { fd_, POLLIN, 0 };
            int ret = poll(&pfd, 1, 0);  // timeout=0: non-blocking check

            if (ret > 0 && (pfd.revents & POLLIN)) {
                handle_data();
            }
            // No sleep, no yield — spin forever
            // This core does nothing else
        }
    }
};

// io_uring — submission/completion rings in shared memory
class IoUringReactor {
    struct io_uring ring_;

public:
    void submit_read(int fd, void* buf, size_t len) {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        io_uring_prep_read(sqe, fd, buf, len, 0);
        io_uring_submit(&ring_);  // one syscall batches many ops
    }

    void reap_completions() {
        struct io_uring_cqe* cqe;
        while (io_uring_peek_cqe(&ring_, &cqe) == 0) {
            handle_completion(cqe);
            io_uring_cqe_seen(&ring_, cqe);
        }
    }
};

// ReactorFactory — choose at runtime via config
std::unique_ptr<Reactor> create_reactor(const ServerConfig& config) {
    switch (config.reactor_mode) {
        case ReactorMode::EPOLL:     return std::make_unique<EpollReactor>();
        case ReactorMode::BUSY_POLL: return std::make_unique<BusyPollReactor>();
        case ReactorMode::IO_URING:  return std::make_unique<IoUringReactor>();
    }
}
```

**CPU Pinning:**
```cpp
// Pin thread to specific core — prevents OS from migrating it
void set_thread_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
}

// Why: CPU migration invalidates L1/L2 cache (~5 μs penalty)
// HFT threads NEVER move between cores
```

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `busy_poll_reactor.hpp/cpp` | ~250 | Spin-loop reactor. `poll()` with timeout=0. Never blocks. Configurable spin strategy (busy-wait vs pause instruction) |
| `io_uring_reactor.hpp/cpp` | ~350 | io_uring setup: `io_uring_queue_init()`. Submit reads/writes to SQ ring. Reap completions from CQ ring. Batched submission for throughput |
| `cpu_affinity.hpp/cpp` | ~100 | `set_thread_affinity(core)`, `get_current_core()`, `isolate_core()` helper. NUMA-aware: pin to same NUMA node as NIC |
| `reactor_factory.hpp` | ~50 | Factory pattern: config → reactor type. Polymorphic `Reactor` interface shared by all three backends |

---

## Layer 10: Latency Infrastructure (~500 lines)

HFT lives and dies by measurement. Timestamp **every stage** at nanosecond precision.

```
  Message arrives on wire
       │
       ├── t0: wire_arrive      ← rdtsc()
       │
       ▼
  Parse binary message
       │
       ├── t1: parse_complete   ← rdtsc()
       │
       ▼
  Order book lookup + match
       │
       ├── t2: book_update      ← rdtsc()
       │
       ▼
  Strategy decision
       │
       ├── t3: decision         ← rdtsc()
       │
       ▼
  Serialize + send response
       │
       ├── t4: wire_sent        ← rdtsc()
       │
       ▼
  Total: t4 - t0 = 3.2 μs (target: < 10 μs)
```

```cpp
// rdtsc — CPU cycle counter, ~1 ns resolution, no syscall
inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return static_cast<uint64_t>(hi) << 32 | lo;
}

// Why not clock_gettime()? It's a syscall (~25 ns overhead).
// rdtsc reads the CPU timestamp counter directly (~1 ns).
// Must calibrate once at startup: measure cycles/second.

struct LatencyTracker {
    uint64_t t_wire_arrive;
    uint64_t t_parse_complete;
    uint64_t t_book_update;
    uint64_t t_decision;
    uint64_t t_wire_sent;

    uint64_t total_cycles() const { return t_wire_sent - t_wire_arrive; }
    uint64_t parse_ns()    const { return cycles_to_ns(t_parse_complete - t_wire_arrive); }
    uint64_t book_ns()     const { return cycles_to_ns(t_book_update - t_parse_complete); }
    uint64_t decision_ns() const { return cycles_to_ns(t_decision - t_book_update); }
    uint64_t send_ns()     const { return cycles_to_ns(t_wire_sent - t_decision); }
};

// HDR Histogram — accurate percentiles even with millions of samples
class Histogram {
    std::array<uint64_t, NUM_BUCKETS> buckets_{};

public:
    void record(uint64_t value_ns);
    uint64_t percentile(double p) const;  // p50, p95, p99, p99.9

    void report() const {
        // Output:
        // wire-to-wire latency:
        //   p50:   2,100 ns (2.1 μs)
        //   p95:   4,800 ns (4.8 μs)
        //   p99:   8,200 ns (8.2 μs)
        //   p99.9: 15,600 ns (15.6 μs)
    }
};
```

### Files

| File | Lines | What It Does |
|------|-------|-------------|
| `timestamp.hpp/cpp` | ~120 | `rdtsc()`, `rdtscp()` (serializing variant), TSC calibration: `measure_tsc_frequency()` at startup, `cycles_to_ns()` conversion |
| `latency_tracker.hpp/cpp` | ~120 | `LatencyTracker` struct with per-stage timestamps. `stamp(Stage)` records rdtsc. `report()` computes per-stage breakdown |
| `histogram.hpp/cpp` | ~180 | HDR histogram. `record(ns)`, `percentile(0.99)`, `reset()`, `merge(other)`. Log-linear bucketing for wide value range |
| `latency_reporter.hpp/cpp` | ~80 | Periodic reporter: every N seconds, snapshot all histograms, log to file/stdout. Formats as table with p50/p95/p99/p99.9 |

---

## What It Looks Like Running

### Phase 1: HTTP Mode

```bash
$ ./stormsocket --config config/default.yaml

[2026-03-09 14:30:00] [INFO] StormSocket v1.0
[2026-03-09 14:30:00] [INFO] Mode: HTTP
[2026-03-09 14:30:00] [INFO] Reactor: epoll (edge-triggered)
[2026-03-09 14:30:00] [INFO] Listening on 0.0.0.0:8080
[2026-03-09 14:30:00] [INFO] Workers: 8 threads
[2026-03-09 14:30:00] [INFO] Max connections: 10,000
[2026-03-09 14:30:00] [INFO] Routes:
[2026-03-09 14:30:00] [INFO]   GET  /              → index
[2026-03-09 14:30:00] [INFO]   GET  /api/health    → health
[2026-03-09 14:30:00] [INFO]   GET  /api/stats     → stats
[2026-03-09 14:30:00] [INFO]   POST /api/echo      → echo
[2026-03-09 14:30:00] [INFO]   GET  /static/*      → files
[2026-03-09 14:30:00] [INFO] Ready.

$ wrk -t4 -c1000 -d30s http://localhost:8080/api/health

Running 30s test @ http://localhost:8080/api/health
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.23ms    0.89ms   12.5ms   85.3%
    Req/Sec    52.3k      4.1k     67.2k    71.4%
  6,276,000 requests in 30s, 1.2GB read
Requests/sec: 209,200
Transfer/sec:   41.2MB
```

### Phase 2: Trading Mode

```bash
$ ./stormsocket --config config/trading.yaml

[2026-03-09 14:30:00] [INFO] StormSocket v1.0
[2026-03-09 14:30:00] [INFO] Mode: TRADING (binary protocol)
[2026-03-09 14:30:00] [INFO] Reactor: io_uring (submission/completion rings)
[2026-03-09 14:30:00] [INFO] Allocator: arena (pre-allocated 256 MB)
[2026-03-09 14:30:00] [INFO] Listening on 0.0.0.0:9090
[2026-03-09 14:30:00] [INFO] I/O thread pinned to core 2
[2026-03-09 14:30:00] [INFO] Worker thread pinned to core 3
[2026-03-09 14:30:00] [INFO] Order pool: 100,000 pre-allocated orders
[2026-03-09 14:30:00] [INFO] Symbols loaded: 500
[2026-03-09 14:30:00] [INFO] TSC frequency: 3.40 GHz (calibrated)
[2026-03-09 14:30:00] [INFO] Ready.

# After running trading load test:
$ curl http://localhost:8081/api/stats
{
  "mode": "trading",
  "reactor": "io_uring",
  "orders": {
    "received": 5420000,
    "filled": 3180000,
    "cancelled": 890000,
    "rejected": 12
  },
  "latency": {
    "wire_to_parse_ns":  { "p50": 45,   "p95": 120,  "p99": 380   },
    "parse_to_book_ns":  { "p50": 180,  "p95": 450,  "p99": 1200  },
    "book_to_send_ns":   { "p50": 95,   "p95": 280,  "p99": 650   },
    "wire_to_wire_ns":   { "p50": 2100, "p95": 4800, "p99": 8200  }
  },
  "memory": {
    "arena_used_mb": 42,
    "arena_capacity_mb": 256,
    "malloc_calls_hot_path": 0
  },
  "uptime_sec": 30
}
```

### Side-by-Side Comparison

```
Metric                    Phase 1 (HTTP)      Phase 2 (Trading)     Speedup
──────────────────────    ──────────────      ─────────────────     ───────
Parse latency             ~500 ns             ~45 ns                11x
Wire-to-wire p50          ~1.2 ms             ~2.1 μs               571x
Wire-to-wire p99          ~5.4 ms             ~8.2 μs               658x
Malloc calls/request      3-5                 0                     ∞
Syscalls/request          3+ (epoll+r+w)      0 (io_uring batch)    ∞
Message size              ~200 bytes          22 bytes              9x
```

---

## Line Count

| Category | Phase 1 | Phase 2 | Total |
|----------|---------|---------|-------|
| **Headers** | ~1,800 | ~1,200 | ~3,000 |
| **Source** | ~3,200 | ~1,600 | ~4,800 |
| **Tests** | ~1,500 | ~500 | ~2,000 |
| **Benchmarks** | ~400 | ~400 | ~800 |
| **Config/Scripts** | ~100 | ~50 | ~150 |
| **Total** | **~8,000** | **~4,000** | **~12,000** |

---

## Interview Talking Points

### Phase 1 — Systems Programming

| Question | Your Answer |
|----------|------------|
| "Why epoll over poll/select?" | "O(1) readiness notification vs O(n) scan. epoll_wait returns only ready fds, not all fds. Scales to 10k+ connections." |
| "Why edge-triggered?" | "Only notifies on state change, not on state. Fewer epoll_wait wakeups under high load. Requires non-blocking read-until-EAGAIN discipline." |
| "How is your queue lock-free?" | "MPSC uses atomic CAS on a linked list head pointer. No mutex, no spinlock. Compare-exchange-weak in a loop. Tagged pointers prevent ABA." |
| "How do you prevent resource leaks?" | "RAII FileDescriptor wraps every fd. Connection pool pre-allocates and recycles. Verified with Valgrind (zero leaks) and AddressSanitizer (zero errors)." |
| "What's your p99 latency?" | "Phase 1: under 5ms at 200k req/sec with 1000 concurrent connections. The lock-free queue eliminated mutex contention that was causing 60% higher tail latency." |
| "How do you handle partial reads?" | "Incremental state-machine parser. `feed()` accepts arbitrary byte chunks. Parser resumes from last state on next `feed()`. No assumption about TCP segment boundaries." |

### Phase 2 — Low-Latency / HFT

| Question | Your Answer |
|----------|------------|
| "Why replace HTTP with binary?" | "HTTP parsing was ~500 ns per message — 25% of total latency. Binary structs are `reinterpret_cast`, effectively 0 ns. Same approach as exchange protocols like ITCH/OUCH." |
| "How does your order book work?" | "Price-time priority with intrusive linked lists. Orders pre-allocated from an arena, so add/remove is O(1) with zero heap allocation. Doubly-linked at each price level for O(1) cancel." |
| "Why an arena allocator?" | "`malloc` is 100-500 ns with potential syscalls. Arena bump-allocation is ~2 ns — just increment a pointer. The pool allocator adds O(1) free-list recycling for fixed-size objects like orders." |
| "How do you avoid syscalls?" | "io_uring uses shared memory rings between userspace and kernel. I submit reads/writes by writing to the submission ring (no syscall), and reap completions by reading the completion ring (no syscall). Batch submission amortizes the one `io_uring_enter` call." |
| "Why busy-poll? Isn't that wasteful?" | "Yes, it burns an entire core. But `epoll_wait` has 5-50 μs wake-up latency from context switching. Busy-poll gives ~100 ns response time. In HFT, one core dedicated to I/O is worth the tradeoff." |
| "How do you measure latency?" | "`rdtsc` reads the CPU cycle counter in ~1 ns (no syscall, unlike `clock_gettime`). I stamp every processing stage — wire arrival, parse, book update, decision, wire send — and feed into an HDR histogram for p50/p95/p99/p99.9." |
| "What did you learn about optimization?" | "Phase 1 → Phase 2 was a 600x latency reduction. The three biggest wins: (1) eliminate parsing (11x), (2) eliminate malloc (removed variance), (3) eliminate syscalls (removed kernel transition cost). Each optimization was measured individually with A/B benchmarks." |

---

## Dependencies

| Library | Purpose | How | Phase |
|---------|---------|-----|-------|
| Linux kernel | epoll, timerfd, signalfd, accept4 | System headers | 1 |
| pthreads | Thread creation, affinity | `-lpthread` | 1 |
| Google Test | Unit tests | FetchContent | 1+2 |
| Google Benchmark | Performance benchmarks | FetchContent | 1+2 |
| (optional) spdlog | Structured logging | FetchContent | 1 |
| liburing | io_uring wrapper | FetchContent or system package | 2 |

---

## The Story This Project Tells

> "I built a 10k-connection HTTP server from scratch with epoll, lock-free queues, and RAII —
> then I extended it with a binary protocol, an order book matching engine, arena allocation,
> io_uring, and nanosecond latency profiling. The result: **600x latency reduction** from 5ms
> to single-digit microseconds, with **zero heap allocations** and **zero syscalls** in the hot path.
> Every optimization was measured individually so I can explain exactly why it matters and by how much."
