# QuantFlow — Full-Stack Trading Platform Architecture

> **Target:** ~107,000 lines | C++17 backend + React/TS frontend | 10 subsystems | Production-grade

---

## What We're Building

A complete algorithmic trading platform — from raw market data to live dashboard with trading signals, backtesting, risk management, and order execution. The kind of system you'd find at a prop shop or quant hedge fund.

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                        QUANTFLOW                                                │
│                                                                                                 │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐  │
│  │ EXCHANGE │───▶│   FEED   │───▶│  SIGNAL  │───▶│ STRATEGY │───▶│   OMS    │───▶│   RISK   │  │
│  │   SIM    │    │ HANDLER  │    │  ENGINE  │    │  ENGINE  │    │  / EMS   │    │  ENGINE  │  │
│  │          │    │          │    │          │    │          │    │          │    │          │  │
│  │ fake     │    │ 4-thread │    │ signals  │    │ alpha →  │    │ route &  │    │ limits & │  │
│  │ exchange │    │ pipeline │    │ features │    │ position │    │ execute  │    │ exposure │  │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘    └──────────┘    └──────────┘  │
│       │               │               │               │               │               │        │
│       │               ▼               ▼               ▼               ▼               ▼        │
│       │          ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐                   │
│       │          │   DATA   │    │ BACKTEST │    │    FIX   │    │ MONITOR  │                   │
│       │          │ RECORDER │    │  ENGINE  │    │ GATEWAY  │    │ & ALERT  │                   │
│       │          │          │    │          │    │          │    │          │                   │
│       │          │ tick DB  │    │ replay & │    │ exchange │    │ health & │                   │
│       │          │ & replay │    │ simulate │    │ protocol │    │ perf     │                   │
│       │          └──────────┘    └──────────┘    └──────────┘    └──────────┘                   │
│       │               │                                               │                        │
│       │               ▼                                               ▼                        │
│       │          ┌─────────────────────────────────────────────────────────┐                    │
│       │          │              WEBSOCKET + REST API GATEWAY              │                    │
│       │          │                  ws://localhost:8080                    │                    │
│       └─────────▶│                 http://localhost:8081                   │                    │
│                  └─────────────────────────────────────────────────────────┘                    │
│                                              │                                                  │
│                                              ▼                                                  │
│                  ┌─────────────────────────────────────────────────────────┐                    │
│                  │                    WEB DASHBOARD                        │                    │
│                  │                 http://localhost:3000                    │                    │
│                  │                                                         │                    │
│                  │  Order Book  │ Price Chart │ Signals │ PnL │ Risk │ Sys │                    │
│                  └─────────────────────────────────────────────────────────┘                    │
└─────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## The 10 Subsystems

| # | Subsystem | What It Does | Language | Lines |
|---|-----------|-------------|----------|-------|
| 1 | **Feed Handler** | Receive, decode, build order book (existing, expanded) | C++ | ~15,000 |
| 2 | **Exchange Simulator** | Generate realistic market data (existing, expanded) | C++ | ~10,000 |
| 3 | **Signal Engine** | Compute trading signals from order book data | C++ | ~10,000 |
| 4 | **Strategy Engine** | Turn signals into trading decisions + position management | C++ | ~8,000 |
| 5 | **Order Management (OMS/EMS)** | Order lifecycle, routing, fill tracking | C++ | ~8,000 |
| 6 | **Risk Engine** | Position limits, exposure, drawdown, circuit breakers | C++ | ~6,000 |
| 7 | **Data Recorder & Replay** | Tick-level recording, historical replay, tick database | C++ | ~6,000 |
| 8 | **Backtest Engine** | Replay historical data through strategy, compute PnL | C++ | ~7,000 |
| 9 | **API Gateway** | WebSocket + REST server, bridges C++ ↔ browser | C++ | ~5,000 |
| 10 | **Web Dashboard** | Real-time trading visualization and control panel | React/TS | ~18,000 |
| — | **FIX Gateway** | FIX 4.4 protocol stub for exchange connectivity | C++ | ~4,000 |
| — | **Monitoring & Infra** | Logging, metrics, health checks, Docker, CI | C++/YAML | ~4,000 |
| — | **Tests & Benchmarks** | Unit, integration, stress, property-based, benchmarks | C++/TS | ~6,000 |
| | | **Total** | | **~107,000** |

---

## Complete Folder Tree

```
quantflow/
│
├── CMakeLists.txt                                    # Root build — all C++ targets
├── package.json                                      # Root — pnpm workspace for dashboard
├── Dockerfile                                        # Multi-stage: build C++ → runtime image
├── docker-compose.yml                                # Sim + Handler + Dashboard + Redis
├── .github/workflows/ci.yml                          # Build + test on push
│
│
│ ═══════════════════════════════════════════════════════════════════
│  SUBSYSTEM 1: FEED HANDLER (expanded from current)    ~15,000 lines
│ ═══════════════════════════════════════════════════════════════════
│
├── include/qf/
│   ├── common/
│   │   ├── types.hpp                                 # Price, Qty, OrderId, Symbol, Side
│   │   ├── clock.hpp                                 # now_ns(), nanos_since_midnight()
│   │   ├── constants.hpp                             # Network, queue, display constants
│   │   ├── aligned_alloc.hpp                         # Cache-line aligned allocation
│   │   ├── error_codes.hpp                           # Typed error enum
│   │   ├── config.hpp                                # Global config loader (YAML)
│   │   ├── logging.hpp                               # Structured logger (spdlog wrapper)
│   │   └── thread_utils.hpp                          # Thread naming, pinning, priority
│   │
│   ├── protocol/
│   │   ├── messages.hpp                              # MiniITCH message structs
│   │   ├── encoder.hpp                               # encode/decode/parse
│   │   ├── validator.hpp                              # Message integrity validation
│   │   ├── batcher.hpp                               # Pack messages into UDP datagrams
│   │   ├── frame_parser.hpp                          # Split datagram into messages
│   │   ├── sequence.hpp                              # Sequence tracking + gap detection
│   │   └── fix/                                      # ── FIX PROTOCOL ──
│   │       ├── fix_message.hpp                       # FIX 4.4 message builder/parser
│   │       ├── fix_fields.hpp                        # Tag definitions (35=, 49=, etc.)
│   │       ├── fix_session.hpp                       # Session layer (logon, heartbeat, seq)
│   │       └── fix_gateway.hpp                       # Full FIX gateway (send/receive orders)
│   │
│   ├── core/
│   │   ├── spsc_queue.hpp                            # Lock-free SPSC ring buffer
│   │   ├── mpsc_queue.hpp                            # Multi-producer single-consumer (for signals)
│   │   ├── object_pool.hpp                           # Pre-allocated object pool (zero-alloc)
│   │   ├── order.hpp                                 # Order struct
│   │   ├── order_store.hpp                           # Hash map of live orders
│   │   ├── price_level.hpp                           # Aggregated qty at price
│   │   ├── order_book.hpp                            # Full order book (bid/ask sides)
│   │   ├── book_manager.hpp                          # Multi-symbol book routing
│   │   ├── pipeline.hpp                              # 4-thread pipeline orchestrator
│   │   ├── pipeline_types.hpp                        # RawPacket, ParsedEvent, BookUpdate
│   │   ├── event_bus.hpp                             # Pub/sub for internal events
│   │   └── ring_buffer.hpp                           # Generic lock-free ring buffer
│   │
│   ├── network/
│   │   ├── multicast_sender.hpp                      # UDP multicast sender
│   │   ├── multicast_receiver.hpp                    # UDP multicast receiver
│   │   ├── tcp_server.hpp                            # Async TCP server (for FIX/API)
│   │   ├── tcp_client.hpp                            # Async TCP client (for FIX)
│   │   └── socket_config.hpp                         # Socket options
│   │
│
│ ═══════════════════════════════════════════════════════════════════
│  SUBSYSTEM 2: EXCHANGE SIMULATOR (expanded)            ~10,000 lines
│ ═══════════════════════════════════════════════════════════════════
│
│   ├── simulator/
│   │   ├── market_simulator.hpp                      # Top-level sim orchestrator
│   │   ├── scenario_loader.hpp                       # JSON config → SimConfig
│   │   ├── order_generator.hpp                       # Message generation with distributions
│   │   ├── price_walk.hpp                            # Ornstein-Uhlenbeck price model
│   │   ├── sim_order_book.hpp                        # Internal book for validation
│   │   ├── sim_matching_engine.hpp                   # Cross matching → trades
│   │   ├── news_event_generator.hpp                  # Simulate market-moving events
│   │   ├── volatility_regime.hpp                     # Switch between calm/volatile/crash
│   │   ├── participant_model.hpp                     # Model different trader types
│   │   │                                             #   (market maker, momentum, random)
│   │   ├── multi_asset_sim.hpp                       # Correlated multi-asset simulation
│   │   └── replay_publisher.hpp                      # Replay recorded data as live feed
│   │
│
│ ═══════════════════════════════════════════════════════════════════
│  SUBSYSTEM 3: SIGNAL ENGINE                            ~10,000 lines
│ ═══════════════════════════════════════════════════════════════════
│
│   ├── signals/
│   │   ├── signal_types.hpp                          # Signal, FeatureVector, SignalStrength
│   │   ├── signal_engine.hpp                         # Orchestrator: book → features → signals
│   │   ├── signal_registry.hpp                       # Register/discover signals dynamically
│   │   │
│   │   ├── features/                                 # ── RAW FEATURES (computed from book) ──
│   │   │   ├── feature_base.hpp                      # Abstract feature interface
│   │   │   ├── vwap.hpp                              # Volume-weighted average price
│   │   │   ├── twap.hpp                              # Time-weighted average price
│   │   │   ├── microprice.hpp                        # Bid/ask size-weighted midprice
│   │   │   ├── order_flow_imbalance.hpp              # Net buy vs sell pressure
│   │   │   ├── book_depth_ratio.hpp                  # Bid depth / ask depth ratio
│   │   │   ├── spread_tracker.hpp                    # Spread statistics (mean, std, z-score)
│   │   │   ├── trade_flow.hpp                        # Aggressor side detection
│   │   │   ├── volatility_estimator.hpp              # Realized vol (Parkinson, Yang-Zhang)
│   │   │   ├── momentum.hpp                          # Price momentum (short/medium/long)
│   │   │   ├── mean_reversion.hpp                    # Z-score of price vs moving average
│   │   │   └── tick_intensity.hpp                    # Message arrival rate changes
│   │   │
│   │   ├── indicators/                               # ── DERIVED INDICATORS ──
│   │   │   ├── indicator_base.hpp                    # Abstract indicator interface
│   │   │   ├── ema.hpp                               # Exponential moving average
│   │   │   ├── bollinger.hpp                         # Bollinger bands
│   │   │   ├── rsi.hpp                               # Relative strength index
│   │   │   ├── macd.hpp                              # Moving average convergence/divergence
│   │   │   └── zscore.hpp                            # Rolling z-score normalizer
│   │   │
│   │   ├── composite/                                # ── COMPOSITE SIGNALS ──
│   │   │   ├── alpha_combiner.hpp                    # Weighted combination of signals
│   │   │   ├── regime_detector.hpp                   # Detect market regime (trending/ranging)
│   │   │   └── signal_decay.hpp                      # Time-decay for signal strength
│   │   │
│   │   └── ml/                                       # ── ML INTEGRATION ──
│   │       ├── feature_store.hpp                     # Rolling window feature storage
│   │       ├── feature_normalizer.hpp                # Online standardization (Welford's)
│   │       ├── linear_model.hpp                      # Online linear regression (Ridge)
│   │       ├── model_loader.hpp                      # Load ONNX/custom model weights
│   │       └── prediction_cache.hpp                  # Cache predictions to avoid recompute
│   │
│
│ ═══════════════════════════════════════════════════════════════════
│  SUBSYSTEM 4: STRATEGY ENGINE                          ~8,000 lines
│ ═══════════════════════════════════════════════════════════════════
│
│   ├── strategy/
│   │   ├── strategy_types.hpp                        # Decision, Position, Fill, PnL
│   │   ├── strategy_base.hpp                         # Abstract strategy interface
│   │   ├── strategy_engine.hpp                       # Run strategies, manage lifecycle
│   │   ├── strategy_registry.hpp                     # Dynamic strategy loading
│   │   │
│   │   ├── strategies/                               # ── BUILT-IN STRATEGIES ──
│   │   │   ├── market_making.hpp                     # Quote both sides, earn spread
│   │   │   ├── momentum_follower.hpp                 # Follow strong directional moves
│   │   │   ├── mean_reversion_strat.hpp              # Fade extreme moves back to mean
│   │   │   ├── stat_arb.hpp                          # Pairs trading (correlated symbols)
│   │   │   └── signal_threshold.hpp                  # Generic: buy if signal > X, sell if < Y
│   │   │
│   │   ├── position/                                 # ── POSITION MANAGEMENT ──
│   │   │   ├── position_tracker.hpp                  # Track position per symbol
│   │   │   ├── pnl_calculator.hpp                    # Realized + unrealized PnL
│   │   │   ├── fill_tracker.hpp                      # Track order fills, avg price
│   │   │   └── portfolio.hpp                         # Multi-symbol portfolio state
│   │   │
│   │   └── execution/                                # ── EXECUTION LOGIC ──
│   │       ├── order_sizer.hpp                       # Kelly criterion, fixed fractional
│   │       ├── execution_algo.hpp                    # TWAP, VWAP, iceberg
│   │       └── slippage_model.hpp                    # Estimate market impact
│   │
│
│ ═══════════════════════════════════════════════════════════════════
│  SUBSYSTEM 5: ORDER MANAGEMENT SYSTEM (OMS/EMS)       ~8,000 lines
│ ═══════════════════════════════════════════════════════════════════
│
│   ├── oms/
│   │   ├── oms_types.hpp                             # OmsOrder, OrderState, FillReport
│   │   ├── order_manager.hpp                         # Order lifecycle state machine
│   │   ├── order_router.hpp                          # Route to correct exchange/venue
│   │   ├── order_validator.hpp                       # Pre-trade checks (size, price, symbol)
│   │   ├── fill_manager.hpp                          # Process fills, update positions
│   │   ├── order_book_manager.hpp                    # Track own orders in the book
│   │   ├── order_id_generator.hpp                    # Globally unique order IDs
│   │   │
│   │   ├── state_machine/                            # ── ORDER STATE MACHINE ──
│   │   │   ├── order_states.hpp                      # New→Sent→Acked→PartFill→Filled/Cancelled
│   │   │   ├── state_transitions.hpp                 # Valid transitions + guards
│   │   │   └── order_journal.hpp                     # Append-only order event log
│   │   │
│   │   └── sim_exchange/                             # ── SIMULATED EXCHANGE ──
│   │       ├── sim_exchange.hpp                      # Accept orders, simulate fills
│   │       ├── sim_fill_model.hpp                    # Latency, partial fills, rejects
│   │       └── sim_exchange_connector.hpp            # OMS ↔ sim exchange adapter
│   │
│
│ ═══════════════════════════════════════════════════════════════════
│  SUBSYSTEM 6: RISK ENGINE                              ~6,000 lines
│ ═══════════════════════════════════════════════════════════════════
│
│   ├── risk/
│   │   ├── risk_types.hpp                            # RiskCheck, Exposure, Limit, Breach
│   │   ├── risk_engine.hpp                           # Central risk orchestrator
│   │   ├── pre_trade_risk.hpp                        # Check BEFORE sending order
│   │   ├── post_trade_risk.hpp                       # Check AFTER fill received
│   │   │
│   │   ├── limits/                                   # ── RISK LIMITS ──
│   │   │   ├── position_limit.hpp                    # Max shares per symbol
│   │   │   ├── notional_limit.hpp                    # Max dollar exposure
│   │   │   ├── loss_limit.hpp                        # Max drawdown per day/session
│   │   │   ├── order_rate_limit.hpp                  # Max orders per second
│   │   │   └── concentration_limit.hpp               # Max % of portfolio in one symbol
│   │   │
│   │   ├── monitors/                                 # ── RISK MONITORS ──
│   │   │   ├── pnl_monitor.hpp                       # Track PnL vs limits
│   │   │   ├── exposure_monitor.hpp                  # Net/gross exposure tracking
│   │   │   ├── var_calculator.hpp                    # Value-at-Risk (parametric)
│   │   │   └── circuit_breaker.hpp                   # Kill switch: halt all trading
│   │   │
│   │   └── reporting/                                # ── RISK REPORTING ──
│   │       ├── risk_report.hpp                       # Snapshot of all risk metrics
│   │       └── risk_logger.hpp                       # Audit trail of all risk checks
│   │
│
│ ═══════════════════════════════════════════════════════════════════
│  SUBSYSTEM 7: DATA RECORDER & REPLAY                   ~6,000 lines
│ ═══════════════════════════════════════════════════════════════════
│
│   ├── data/
│   │   ├── data_types.hpp                            # TickRecord, Bar, MarketSnapshot
│   │   │
│   │   ├── recorder/                                 # ── LIVE RECORDING ──
│   │   │   ├── tick_recorder.hpp                     # Record every message to disk
│   │   │   ├── binary_writer.hpp                     # Fast binary format (custom)
│   │   │   ├── csv_writer.hpp                        # Human-readable CSV export
│   │   │   ├── compression.hpp                       # LZ4 compression for storage
│   │   │   └── rotation.hpp                          # Daily file rotation + naming
│   │   │
│   │   ├── storage/                                  # ── TICK DATABASE ──
│   │   │   ├── tick_store.hpp                        # Read/write tick data files
│   │   │   ├── bar_aggregator.hpp                    # Ticks → 1s/1m/5m/1h bars
│   │   │   ├── time_index.hpp                        # Binary search by timestamp
│   │   │   └── symbol_catalog.hpp                    # Index of available symbols/dates
│   │   │
│   │   └── replay/                                   # ── HISTORICAL REPLAY ──
│   │       ├── replay_engine.hpp                     # Play back recorded data as live
│   │       ├── replay_clock.hpp                      # Virtual clock (speed up/slow down)
│   │       ├── multi_stream_merger.hpp               # Merge multiple symbol streams by time
│   │       └── replay_publisher.hpp                  # Publish replayed data to pipeline
│   │
│
│ ═══════════════════════════════════════════════════════════════════
│  SUBSYSTEM 8: BACKTEST ENGINE                          ~7,000 lines
│ ═══════════════════════════════════════════════════════════════════
│
│   ├── backtest/
│   │   ├── backtest_types.hpp                        # BacktestConfig, BacktestResult
│   │   ├── backtest_engine.hpp                       # Orchestrate: replay → strategy → PnL
│   │   ├── backtest_runner.hpp                       # CLI runner with progress
│   │   │
│   │   ├── simulation/                               # ── MARKET SIMULATION ──
│   │   │   ├── fill_simulator.hpp                    # Simulate fills from historical book
│   │   │   ├── latency_simulator.hpp                 # Add realistic latency to decisions
│   │   │   ├── cost_model.hpp                        # Commission, fees, spread cost
│   │   │   └── market_impact.hpp                     # Temporary + permanent impact
│   │   │
│   │   ├── analytics/                                # ── PERFORMANCE ANALYTICS ──
│   │   │   ├── returns_calculator.hpp                # Daily/cumulative returns
│   │   │   ├── sharpe_ratio.hpp                      # Risk-adjusted return
│   │   │   ├── max_drawdown.hpp                      # Peak-to-trough analysis
│   │   │   ├── win_rate.hpp                          # Win/loss ratio, avg win/loss
│   │   │   ├── trade_statistics.hpp                  # Trade count, duration, PnL dist
│   │   │   └── equity_curve.hpp                      # Time series of portfolio value
│   │   │
│   │   └── optimization/                             # ── PARAMETER OPTIMIZATION ──
│   │       ├── grid_search.hpp                       # Brute-force parameter sweep
│   │       ├── walk_forward.hpp                      # Walk-forward validation
│   │       └── param_sensitivity.hpp                 # Sensitivity analysis
│   │
│
│ ═══════════════════════════════════════════════════════════════════
│  SUBSYSTEM 9: API GATEWAY (WebSocket + REST)           ~5,000 lines
│ ═══════════════════════════════════════════════════════════════════
│
│   ├── gateway/
│   │   ├── gateway_types.hpp                         # WsMessage, RestRequest, RestResponse
│   │   ├── websocket_server.hpp                      # uWebSockets-based WS server
│   │   ├── rest_server.hpp                           # HTTP REST API server
│   │   ├── message_serializer.hpp                    # C++ structs → JSON (nlohmann)
│   │   ├── throttle.hpp                              # Rate-limit updates to browser
│   │   ├── client_manager.hpp                        # Track connected WS clients
│   │   │
│   │   ├── channels/                                 # ── WEBSOCKET CHANNELS ──
│   │   │   ├── book_channel.hpp                      # ws://host/book — live order book
│   │   │   ├── trade_channel.hpp                     # ws://host/trades — trade feed
│   │   │   ├── signal_channel.hpp                    # ws://host/signals — signal updates
│   │   │   ├── pnl_channel.hpp                       # ws://host/pnl — PnL updates
│   │   │   ├── risk_channel.hpp                      # ws://host/risk — risk metrics
│   │   │   └── system_channel.hpp                    # ws://host/system — health/latency
│   │   │
│   │   └── endpoints/                                # ── REST ENDPOINTS ──
│   │       ├── orders_endpoint.hpp                   # POST /orders, GET /orders, DELETE
│   │       ├── positions_endpoint.hpp                # GET /positions
│   │       ├── strategies_endpoint.hpp               # GET/POST /strategies (enable/disable)
│   │       ├── backtest_endpoint.hpp                 # POST /backtest (run backtest)
│   │       ├── config_endpoint.hpp                   # GET/PUT /config
│   │       └── health_endpoint.hpp                   # GET /health
│   │
│
│ ═══════════════════════════════════════════════════════════════════
│  SUBSYSTEM 10: WEB DASHBOARD (React + TypeScript)      ~18,000 lines
│ ═══════════════════════════════════════════════════════════════════
│
│   └── consumer/                                     # ── ANALYTICS (C++) ──
│       ├── latency_histogram.hpp                     # Fixed-bucket latency histogram
│       ├── throughput_tracker.hpp                     # Rolling window throughput
│       ├── stats_collector.hpp                       # Aggregate all metrics
│       ├── console_display.hpp                       # Terminal display (fallback)
│       ├── csv_logger.hpp                            # CSV export
│       └── alert_monitor.hpp                         # Anomaly detection
│
├── dashboard/                                        # ── REACT FRONTEND ──
│   ├── package.json
│   ├── tsconfig.json
│   ├── vite.config.ts
│   ├── tailwind.config.ts
│   ├── index.html
│   │
│   ├── src/
│   │   ├── main.tsx                                  # App entry point
│   │   ├── App.tsx                                   # Layout + routing
│   │   │
│   │   ├── api/                                      # ── DATA LAYER ──
│   │   │   ├── websocket.ts                          # WebSocket connection manager
│   │   │   ├── rest.ts                               # REST API client (fetch wrapper)
│   │   │   ├── types.ts                              # TypeScript types matching C++ structs
│   │   │   └── hooks/
│   │   │       ├── useOrderBook.ts                   # Hook: live order book data
│   │   │       ├── useSignals.ts                     # Hook: live signal updates
│   │   │       ├── usePnL.ts                         # Hook: live PnL
│   │   │       ├── useRisk.ts                        # Hook: live risk metrics
│   │   │       ├── useSystemHealth.ts                # Hook: latency, throughput
│   │   │       └── useTrades.ts                      # Hook: live trade feed
│   │   │
│   │   ├── store/                                    # ── STATE MANAGEMENT (Zustand) ──
│   │   │   ├── marketStore.ts                        # Order book + trades state
│   │   │   ├── signalStore.ts                        # Signals state
│   │   │   ├── portfolioStore.ts                     # Positions + PnL state
│   │   │   ├── riskStore.ts                          # Risk metrics state
│   │   │   ├── strategyStore.ts                      # Strategy config + status
│   │   │   ├── systemStore.ts                        # System health state
│   │   │   └── backtestStore.ts                      # Backtest results state
│   │   │
│   │   ├── components/                               # ── UI COMPONENTS ──
│   │   │   │
│   │   │   ├── layout/
│   │   │   │   ├── Sidebar.tsx                       # Navigation sidebar
│   │   │   │   ├── Header.tsx                        # Top bar with system status
│   │   │   │   ├── StatusBar.tsx                     # Bottom bar: latency, throughput
│   │   │   │   └── Panel.tsx                         # Resizable panel container
│   │   │   │
│   │   │   ├── market/                               # ── MARKET DATA VIEWS ──
│   │   │   │   ├── OrderBookView.tsx                 # Live order book (bid/ask ladder)
│   │   │   │   ├── OrderBookDepthChart.tsx           # Depth chart (area graph)
│   │   │   │   ├── PriceChart.tsx                    # Candlestick chart (TradingView)
│   │   │   │   ├── TradeTickerView.tsx               # Scrolling trade feed (time & sales)
│   │   │   │   ├── SpreadChart.tsx                   # Bid-ask spread over time
│   │   │   │   └── SymbolSelector.tsx                # Dropdown to select active symbol
│   │   │   │
│   │   │   ├── signals/                              # ── SIGNAL VIEWS ──
│   │   │   │   ├── SignalDashboard.tsx               # All signals at a glance
│   │   │   │   ├── SignalCard.tsx                    # Individual signal (name, value, bar)
│   │   │   │   ├── SignalChart.tsx                   # Signal history over time
│   │   │   │   ├── SignalHeatmap.tsx                 # Symbol × signal matrix heatmap
│   │   │   │   └── AlphaComposite.tsx                # Combined alpha score visualization
│   │   │   │
│   │   │   ├── trading/                              # ── TRADING VIEWS ──
│   │   │   │   ├── OrderEntry.tsx                    # Manual order entry form
│   │   │   │   ├── ActiveOrders.tsx                  # Table of live orders
│   │   │   │   ├── OrderHistory.tsx                  # Filled/cancelled orders log
│   │   │   │   ├── PositionTable.tsx                 # Current positions per symbol
│   │   │   │   └── FillsTable.tsx                    # Recent fills with prices
│   │   │   │
│   │   │   ├── pnl/                                  # ── PNL VIEWS ──
│   │   │   │   ├── PnLSummary.tsx                    # Total PnL, realized, unrealized
│   │   │   │   ├── EquityCurve.tsx                   # Portfolio value over time
│   │   │   │   ├── PnLBySymbol.tsx                   # Bar chart: PnL per symbol
│   │   │   │   └── TradeStats.tsx                    # Win rate, avg win/loss, Sharpe
│   │   │   │
│   │   │   ├── risk/                                 # ── RISK VIEWS ──
│   │   │   │   ├── RiskDashboard.tsx                 # Overview of all risk metrics
│   │   │   │   ├── ExposureGauge.tsx                 # Net/gross exposure gauge
│   │   │   │   ├── DrawdownChart.tsx                 # Drawdown over time
│   │   │   │   ├── LimitUtilization.tsx              # How close to each limit
│   │   │   │   └── CircuitBreakerPanel.tsx           # Kill switch controls
│   │   │   │
│   │   │   ├── strategy/                             # ── STRATEGY VIEWS ──
│   │   │   │   ├── StrategyList.tsx                  # List of strategies (enable/disable)
│   │   │   │   ├── StrategyConfig.tsx                # Edit strategy parameters
│   │   │   │   └── StrategyPerformance.tsx           # Per-strategy PnL and metrics
│   │   │   │
│   │   │   ├── backtest/                             # ── BACKTEST VIEWS ──
│   │   │   │   ├── BacktestRunner.tsx                # Configure + launch backtest
│   │   │   │   ├── BacktestResults.tsx               # Results table + charts
│   │   │   │   ├── EquityCurveComparison.tsx         # Compare multiple backtest runs
│   │   │   │   └── ParameterHeatmap.tsx              # Grid search results heatmap
│   │   │   │
│   │   │   ├── system/                               # ── SYSTEM VIEWS ──
│   │   │   │   ├── LatencyHistogram.tsx              # Live latency distribution
│   │   │   │   ├── ThroughputGauge.tsx               # msgs/sec speedometer
│   │   │   │   ├── QueueDepthChart.tsx               # SPSC queue fill levels
│   │   │   │   ├── SystemHealth.tsx                  # CPU, memory, thread status
│   │   │   │   └── AlertFeed.tsx                     # Scrolling alert log
│   │   │   │
│   │   │   └── shared/                               # ── SHARED COMPONENTS ──
│   │   │       ├── DataTable.tsx                     # Generic sortable table
│   │   │       ├── MiniChart.tsx                     # Sparkline chart
│   │   │       ├── Gauge.tsx                         # Circular gauge
│   │   │       ├── HeatmapCell.tsx                   # Color-coded cell
│   │   │       ├── StatusBadge.tsx                   # Green/yellow/red status
│   │   │       └── TimeAgo.tsx                       # "2s ago" timestamp
│   │   │
│   │   ├── pages/                                    # ── PAGE LAYOUTS ──
│   │   │   ├── TradingPage.tsx                       # Main trading view (book + chart + orders)
│   │   │   ├── SignalsPage.tsx                       # All signals + heatmap
│   │   │   ├── PortfolioPage.tsx                     # Positions + PnL + equity curve
│   │   │   ├── RiskPage.tsx                          # Risk dashboard
│   │   │   ├── StrategyPage.tsx                      # Strategy management
│   │   │   ├── BacktestPage.tsx                      # Backtesting interface
│   │   │   └── SystemPage.tsx                        # System monitoring
│   │   │
│   │   ├── lib/                                      # ── UTILITIES ──
│   │   │   ├── formatters.ts                         # Format prices, numbers, latency
│   │   │   ├── colors.ts                             # Color scales for heatmaps
│   │   │   └── constants.ts                          # API URLs, refresh rates
│   │   │
│   │   └── styles/
│   │       └── globals.css                           # Tailwind + custom dark theme
│   │
│   └── public/
│       └── favicon.ico
│
│
│ ═══════════════════════════════════════════════════════════════════
│  SOURCE IMPLEMENTATIONS (src/)
│ ═══════════════════════════════════════════════════════════════════
│
├── src/
│   ├── handler/                                      # ── FEED HANDLER EXE ──
│   │   ├── main.cpp                                  # Entry: parse args → pipeline → run
│   │   ├── network_stage.cpp                         # Thread 1: receive → Q1
│   │   ├── parser_stage.cpp                          # Thread 2: Q1 → decode → Q2
│   │   ├── book_stage.cpp                            # Thread 3: Q2 → book → Q3
│   │   └── consumer_stage.cpp                        # Thread 4: Q3 → stats/display
│   │
│   ├── simulator/                                    # ── EXCHANGE SIMULATOR EXE ──
│   │   ├── main.cpp                                  # Entry: load config → run sim
│   │   ├── market_simulator.cpp
│   │   ├── scenario_loader.cpp
│   │   ├── order_generator.cpp
│   │   ├── price_walk.cpp
│   │   ├── sim_order_book.cpp
│   │   ├── sim_matching_engine.cpp
│   │   ├── news_event_generator.cpp
│   │   ├── volatility_regime.cpp
│   │   ├── participant_model.cpp
│   │   ├── multi_asset_sim.cpp
│   │   └── replay_publisher.cpp
│   │
│   ├── protocol/                                     # ── PROTOCOL IMPL ──
│   │   ├── validator.cpp
│   │   ├── batcher.cpp
│   │   ├── frame_parser.cpp
│   │   ├── sequence.cpp
│   │   └── fix/
│   │       ├── fix_message.cpp
│   │       ├── fix_session.cpp
│   │       └── fix_gateway.cpp
│   │
│   ├── core/                                         # ── CORE ENGINE IMPL ──
│   │   ├── order_store.cpp
│   │   ├── price_level.cpp
│   │   ├── order_book.cpp
│   │   ├── book_manager.cpp
│   │   ├── pipeline.cpp
│   │   └── event_bus.cpp
│   │
│   ├── signals/                                      # ── SIGNAL ENGINE IMPL ──
│   │   ├── signal_engine.cpp
│   │   ├── signal_registry.cpp
│   │   ├── features/
│   │   │   ├── vwap.cpp
│   │   │   ├── twap.cpp
│   │   │   ├── microprice.cpp
│   │   │   ├── order_flow_imbalance.cpp
│   │   │   ├── book_depth_ratio.cpp
│   │   │   ├── spread_tracker.cpp
│   │   │   ├── trade_flow.cpp
│   │   │   ├── volatility_estimator.cpp
│   │   │   ├── momentum.cpp
│   │   │   ├── mean_reversion.cpp
│   │   │   └── tick_intensity.cpp
│   │   ├── indicators/
│   │   │   ├── ema.cpp
│   │   │   ├── bollinger.cpp
│   │   │   ├── rsi.cpp
│   │   │   ├── macd.cpp
│   │   │   └── zscore.cpp
│   │   ├── composite/
│   │   │   ├── alpha_combiner.cpp
│   │   │   ├── regime_detector.cpp
│   │   │   └── signal_decay.cpp
│   │   └── ml/
│   │       ├── feature_store.cpp
│   │       ├── feature_normalizer.cpp
│   │       ├── linear_model.cpp
│   │       ├── model_loader.cpp
│   │       └── prediction_cache.cpp
│   │
│   ├── strategy/                                     # ── STRATEGY ENGINE IMPL ──
│   │   ├── strategy_engine.cpp
│   │   ├── strategy_registry.cpp
│   │   ├── strategies/
│   │   │   ├── market_making.cpp
│   │   │   ├── momentum_follower.cpp
│   │   │   ├── mean_reversion_strat.cpp
│   │   │   ├── stat_arb.cpp
│   │   │   └── signal_threshold.cpp
│   │   ├── position/
│   │   │   ├── position_tracker.cpp
│   │   │   ├── pnl_calculator.cpp
│   │   │   ├── fill_tracker.cpp
│   │   │   └── portfolio.cpp
│   │   └── execution/
│   │       ├── order_sizer.cpp
│   │       ├── execution_algo.cpp
│   │       └── slippage_model.cpp
│   │
│   ├── oms/                                          # ── OMS/EMS IMPL ──
│   │   ├── order_manager.cpp
│   │   ├── order_router.cpp
│   │   ├── order_validator.cpp
│   │   ├── fill_manager.cpp
│   │   ├── order_book_manager.cpp
│   │   ├── order_id_generator.cpp
│   │   ├── state_machine/
│   │   │   ├── order_states.cpp
│   │   │   ├── state_transitions.cpp
│   │   │   └── order_journal.cpp
│   │   └── sim_exchange/
│   │       ├── sim_exchange.cpp
│   │       ├── sim_fill_model.cpp
│   │       └── sim_exchange_connector.cpp
│   │
│   ├── risk/                                         # ── RISK ENGINE IMPL ──
│   │   ├── risk_engine.cpp
│   │   ├── pre_trade_risk.cpp
│   │   ├── post_trade_risk.cpp
│   │   ├── limits/
│   │   │   ├── position_limit.cpp
│   │   │   ├── notional_limit.cpp
│   │   │   ├── loss_limit.cpp
│   │   │   ├── order_rate_limit.cpp
│   │   │   └── concentration_limit.cpp
│   │   ├── monitors/
│   │   │   ├── pnl_monitor.cpp
│   │   │   ├── exposure_monitor.cpp
│   │   │   ├── var_calculator.cpp
│   │   │   └── circuit_breaker.cpp
│   │   └── reporting/
│   │       ├── risk_report.cpp
│   │       └── risk_logger.cpp
│   │
│   ├── data/                                         # ── DATA RECORDER IMPL ──
│   │   ├── recorder/
│   │   │   ├── tick_recorder.cpp
│   │   │   ├── binary_writer.cpp
│   │   │   ├── csv_writer.cpp
│   │   │   ├── compression.cpp
│   │   │   └── rotation.cpp
│   │   ├── storage/
│   │   │   ├── tick_store.cpp
│   │   │   ├── bar_aggregator.cpp
│   │   │   ├── time_index.cpp
│   │   │   └── symbol_catalog.cpp
│   │   └── replay/
│   │       ├── replay_engine.cpp
│   │       ├── replay_clock.cpp
│   │       ├── multi_stream_merger.cpp
│   │       └── replay_publisher.cpp
│   │
│   ├── backtest/                                     # ── BACKTEST ENGINE IMPL ──
│   │   ├── backtest_engine.cpp
│   │   ├── backtest_runner.cpp
│   │   ├── simulation/
│   │   │   ├── fill_simulator.cpp
│   │   │   ├── latency_simulator.cpp
│   │   │   ├── cost_model.cpp
│   │   │   └── market_impact.cpp
│   │   ├── analytics/
│   │   │   ├── returns_calculator.cpp
│   │   │   ├── sharpe_ratio.cpp
│   │   │   ├── max_drawdown.cpp
│   │   │   ├── win_rate.cpp
│   │   │   ├── trade_statistics.cpp
│   │   │   └── equity_curve.cpp
│   │   └── optimization/
│   │       ├── grid_search.cpp
│   │       ├── walk_forward.cpp
│   │       └── param_sensitivity.cpp
│   │
│   ├── gateway/                                      # ── API GATEWAY IMPL ──
│   │   ├── websocket_server.cpp
│   │   ├── rest_server.cpp
│   │   ├── message_serializer.cpp
│   │   ├── throttle.cpp
│   │   ├── client_manager.cpp
│   │   ├── channels/
│   │   │   ├── book_channel.cpp
│   │   │   ├── trade_channel.cpp
│   │   │   ├── signal_channel.cpp
│   │   │   ├── pnl_channel.cpp
│   │   │   ├── risk_channel.cpp
│   │   │   └── system_channel.cpp
│   │   └── endpoints/
│   │       ├── orders_endpoint.cpp
│   │       ├── positions_endpoint.cpp
│   │       ├── strategies_endpoint.cpp
│   │       ├── backtest_endpoint.cpp
│   │       ├── config_endpoint.cpp
│   │       └── health_endpoint.cpp
│   │
│   ├── consumer/                                     # ── ANALYTICS IMPL ──
│   │   ├── latency_histogram.cpp
│   │   ├── throughput_tracker.cpp
│   │   ├── stats_collector.cpp
│   │   ├── console_display.cpp
│   │   ├── csv_logger.cpp
│   │   └── alert_monitor.cpp
│   │
│   └── infra/                                        # ── INFRASTRUCTURE IMPL ──
│       ├── config.cpp                                # YAML config loading
│       ├── logging.cpp                               # spdlog structured logging
│       ├── metrics.cpp                               # Prometheus-style metrics
│       └── health_check.cpp                          # /health endpoint backing
│
│
│ ═══════════════════════════════════════════════════════════════════
│  TESTS
│ ═══════════════════════════════════════════════════════════════════
│
├── tests/
│   ├── protocol/
│   │   ├── test_encode_decode.cpp
│   │   ├── test_validator.cpp
│   │   ├── test_batcher.cpp
│   │   ├── test_frame_parser.cpp
│   │   ├── test_sequence.cpp
│   │   └── test_fix_message.cpp
│   │
│   ├── core/
│   │   ├── test_spsc_queue.cpp
│   │   ├── test_mpsc_queue.cpp
│   │   ├── test_object_pool.cpp
│   │   ├── test_order_store.cpp
│   │   ├── test_order_book.cpp
│   │   ├── test_book_manager.cpp
│   │   └── test_pipeline.cpp
│   │
│   ├── signals/
│   │   ├── test_vwap.cpp
│   │   ├── test_microprice.cpp
│   │   ├── test_order_flow_imbalance.cpp
│   │   ├── test_ema.cpp
│   │   ├── test_bollinger.cpp
│   │   ├── test_alpha_combiner.cpp
│   │   └── test_feature_normalizer.cpp
│   │
│   ├── strategy/
│   │   ├── test_market_making.cpp
│   │   ├── test_position_tracker.cpp
│   │   ├── test_pnl_calculator.cpp
│   │   └── test_order_sizer.cpp
│   │
│   ├── oms/
│   │   ├── test_order_manager.cpp
│   │   ├── test_state_machine.cpp
│   │   ├── test_order_journal.cpp
│   │   └── test_sim_exchange.cpp
│   │
│   ├── risk/
│   │   ├── test_position_limit.cpp
│   │   ├── test_loss_limit.cpp
│   │   ├── test_circuit_breaker.cpp
│   │   └── test_var_calculator.cpp
│   │
│   ├── data/
│   │   ├── test_tick_recorder.cpp
│   │   ├── test_bar_aggregator.cpp
│   │   ├── test_replay_engine.cpp
│   │   └── test_compression.cpp
│   │
│   ├── backtest/
│   │   ├── test_backtest_engine.cpp
│   │   ├── test_fill_simulator.cpp
│   │   ├── test_sharpe_ratio.cpp
│   │   └── test_max_drawdown.cpp
│   │
│   ├── integration/
│   │   ├── test_sim_to_handler.cpp
│   │   ├── test_signal_pipeline.cpp
│   │   ├── test_strategy_to_oms.cpp
│   │   ├── test_risk_blocks_order.cpp
│   │   └── test_full_loop.cpp
│   │
│   ├── simulator/
│   │   ├── test_scenario_loader.cpp
│   │   ├── test_order_generator.cpp
│   │   ├── test_price_walk.cpp
│   │   ├── test_sim_order_book.cpp
│   │   └── test_sim_matching_engine.cpp
│   │
│   └── consumer/
│       ├── test_latency_histogram.cpp
│       ├── test_throughput_tracker.cpp
│       └── test_alert_monitor.cpp
│
├── bench/
│   ├── bench_spsc_queue.cpp
│   ├── bench_order_book.cpp
│   ├── bench_encode_decode.cpp
│   ├── bench_signal_engine.cpp
│   ├── bench_pipeline.cpp
│   └── bench_backtest.cpp
│
│
│ ═══════════════════════════════════════════════════════════════════
│  CONFIG & INFRA
│ ═══════════════════════════════════════════════════════════════════
│
├── config/
│   ├── default_scenario.json
│   ├── stress_scenario.json
│   ├── replay_scenario.json
│   ├── quantflow.yaml                               # Master config (all subsystems)
│   ├── strategies/
│   │   ├── market_making.yaml                        # Market making parameters
│   │   ├── momentum.yaml                             # Momentum parameters
│   │   └── mean_reversion.yaml                       # Mean reversion parameters
│   └── risk/
│       └── risk_limits.yaml                          # All risk limits
│
├── scripts/
│   ├── start_all.sh                                  # Start sim + handler + dashboard
│   ├── record_session.sh                             # Record market data to disk
│   └── run_backtest.sh                               # CLI backtest runner
│
├── docs/
│   ├── ARCHITECTURE.md
│   ├── ARCHITECTURE_V2.md                            # ← YOU ARE HERE
│   ├── notas-pt.md
│   └── plans/
│
└── SENIOR_DEV_SUPPORT.md
```

---

## Data Flow — The Full Loop

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            THE FULL LOOP                                    │
│                                                                             │
│   MARKET DATA FLOW (left to right):                                         │
│                                                                             │
│   Exchange Sim ─── UDP ──▶ Feed Handler ──▶ Signal Engine ──▶ Strategy      │
│        │                       │                │                 │          │
│        │                       ▼                ▼                 ▼          │
│        │                  Data Recorder    Feature Store    Position Mgr     │
│        │                       │                                 │          │
│        │                       ▼                                 ▼          │
│        │                  Tick Database                     Order Sizer      │
│        │                       │                                 │          │
│        │                       ▼                                 ▼          │
│        │                  Replay Engine ─── (backtest) ──▶  OMS / EMS       │
│        │                                                         │          │
│   ORDER FLOW (right to left):                                    │          │
│        │                                                         ▼          │
│        │         ◀─── fill ─── Sim Exchange ◀── order ──── Risk Engine      │
│        │                           │                         │              │
│        │                           │                    PRE-TRADE           │
│        │                           ▼                    check before        │
│        │                      Fill Manager              every order         │
│        │                           │                                        │
│        │                           ▼                                        │
│        │                      PnL Calculator ──▶ Portfolio                   │
│        │                                                                    │
│   VISUALIZATION (everything flows to dashboard):                            │
│        │                                                                    │
│        ▼                                                                    │
│   API Gateway ─── WebSocket ──▶ React Dashboard                             │
│        │              │                │                                    │
│        │         book/trade       signal/pnl/risk                          │
│        │         channels          channels                                │
│        │                                                                    │
│        └── REST API ──▶ Order entry, strategy config, backtest launch       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Subsystem Deep Dives

### 3. Signal Engine (~10,000 lines)

The quant core. Computes everything from raw book data.

```
  BookUpdate arrives
       │
       ▼
  ┌─── Feature Extraction ─────────────────────────────┐
  │                                                     │
  │  VWAP ─────────┐                                    │
  │  TWAP ─────────┤                                    │
  │  Microprice ───┤                                    │
  │  OFI ──────────┤──▶ FeatureVector [15 dimensions]   │
  │  Depth Ratio ──┤                                    │
  │  Spread Z ─────┤                                    │
  │  Trade Flow ───┤                                    │
  │  Volatility ───┤                                    │
  │  Momentum ─────┤                                    │
  │  Mean Rev ─────┤                                    │
  │  Tick Rate ────┘                                    │
  │                                                     │
  └─────────────────────────────────────────────────────┘
       │
       ▼
  ┌─── Indicators ─────────────────────────────────────┐
  │  EMA(features, span=20/50/200)                      │
  │  Bollinger(price, 20, 2σ)                           │
  │  RSI(returns, 14)                                   │
  │  MACD(price, 12, 26, 9)                             │
  │  Z-Score(feature, window=100)                       │
  └─────────────────────────────────────────────────────┘
       │
       ▼
  ┌─── Composite Signals ─────────────────────────────┐
  │  AlphaCombiner: weighted sum → composite score     │
  │  RegimeDetector: trending vs ranging vs volatile   │
  │  SignalDecay: strength decays over time            │
  └────────────────────────────────────────────────────┘
       │
       ▼
  ┌─── ML Layer (optional) ───────────────────────────┐
  │  FeatureStore: rolling window of features          │
  │  FeatureNormalizer: online standardization         │
  │  LinearModel: online Ridge regression              │
  │  ModelLoader: load ONNX weights                    │
  └────────────────────────────────────────────────────┘
       │
       ▼
  Signal{symbol, type, strength: [-1,+1], direction, timestamp}
       │
       ▼
  Strategy Engine
```

### 4. Strategy Engine (~8,000 lines)

Turns signals into money.

```
  Signal arrives
       │
       ▼
  ┌─── Strategy ────────────────────────────────────────┐
  │                                                      │
  │  IF signal.strength > threshold AND regime == OK:    │
  │     desired_position = signal.strength × max_size    │
  │     current_position = PositionTracker.get(symbol)   │
  │     delta = desired_position - current_position      │
  │                                                      │
  │     IF delta > min_trade_size:                        │
  │        order = OrderSizer.size(delta, risk_budget)    │
  │        algo  = ExecutionAlgo.select(TWAP/VWAP/IOC)   │
  │        → send to OMS                                 │
  │                                                      │
  └──────────────────────────────────────────────────────┘
       │
       ▼
  OMS validates → Risk checks → Route → Fill → PnL update
```

**5 built-in strategies:**
| Strategy | Logic | When It Trades |
|----------|-------|---------------|
| Market Making | Quote bid+ask, earn spread | Always (mean-revert inventory) |
| Momentum | Follow strong moves | Signal crosses threshold |
| Mean Reversion | Fade extreme moves | Z-score > 2σ from mean |
| Stat Arb | Pairs: long cheap, short expensive | Spread diverges from historical |
| Signal Threshold | Generic: buy if α > X | Configurable |

### 5. OMS — Order State Machine (~8,000 lines)

Every order follows this lifecycle:

```
  ┌───────┐    send     ┌───────┐   ack    ┌────────┐
  │  NEW  │ ──────────▶ │ SENT  │ ───────▶ │ ACTIVE │
  └───────┘             └───────┘          └────────┘
                            │                  │  │
                         reject              fill │ cancel
                            │                  │  │
                            ▼                  ▼  ▼
                       ┌──────────┐    ┌────────────────┐
                       │ REJECTED │    │  PARTIAL FILL  │
                       └──────────┘    └────────────────┘
                                              │
                                         last fill
                                              │
                                              ▼
                                       ┌────────────┐
                                       │   FILLED   │
                                       └────────────┘

  Every transition is journaled (append-only log)
  Invalid transitions are rejected (state machine guards)
```

### 6. Risk Engine (~6,000 lines)

```
  Order from Strategy
       │
       ▼
  ┌─── PRE-TRADE RISK ─────────────────────────────────┐
  │                                                     │
  │  ✓ Position limit: current + order < max_shares     │
  │  ✓ Notional limit: exposure < max_dollars           │
  │  ✓ Loss limit: PnL > -max_daily_loss                │
  │  ✓ Order rate: < max_orders_per_second              │
  │  ✓ Concentration: position < max_pct_portfolio      │
  │                                                     │
  │  ALL PASS → forward to OMS                          │
  │  ANY FAIL → reject, log reason, alert               │
  └─────────────────────────────────────────────────────┘
       │
       ▼
  OMS sends order → Exchange fills → Fill arrives
       │
       ▼
  ┌─── POST-TRADE RISK ────────────────────────────────┐
  │                                                     │
  │  Update position, PnL, exposure                     │
  │  Check if any limit NOW breached                    │
  │  If drawdown > circuit_breaker → HALT ALL TRADING   │
  │                                                     │
  └─────────────────────────────────────────────────────┘

  ┌─── CIRCUIT BREAKER ────────────────────────────────┐
  │                                                     │
  │  TRIGGER: drawdown > 2% OR manual kill switch       │
  │  ACTION:  cancel all open orders                    │
  │           disable all strategies                    │
  │           alert via WebSocket + stderr              │
  │           require manual re-enable                  │
  │                                                     │
  └─────────────────────────────────────────────────────┘
```

### 8. Backtest Engine (~7,000 lines)

```
  Historical tick data
       │
       ▼
  ReplayEngine (virtual clock, configurable speed)
       │
       ▼
  Feed Handler (same code as live!) ──▶ Signal Engine ──▶ Strategy
       │                                                      │
       │                                                      ▼
       │                                               FillSimulator
       │                                               (simulates exchange)
       │                                                      │
       │                                                      ▼
       │                                               PnL Calculator
       │                                                      │
       ▼                                                      ▼
  ┌─── Analytics ──────────────────────────────────────────────┐
  │                                                             │
  │  Sharpe Ratio: annualized risk-adjusted return              │
  │  Max Drawdown: worst peak-to-trough                         │
  │  Win Rate: % profitable trades                              │
  │  Profit Factor: gross profit / gross loss                    │
  │  Avg Trade: average PnL per trade                           │
  │  Equity Curve: portfolio value over time                    │
  │                                                             │
  │  Walk-Forward: train on window → test on next window        │
  │  Grid Search: sweep parameters → find optimal               │
  │  Sensitivity: how fragile are the results?                  │
  │                                                             │
  └─────────────────────────────────────────────────────────────┘
```

**Key insight:** The backtest uses the SAME feed handler, signal engine, and strategy code as live trading. Only the data source (replay vs multicast) and execution (simulated vs real) differ.

---

## The Dashboard — What You Demo

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  QuantFlow                                    ● Connected   ↑ 150k msg/s   │
├────────┬────────────────────────────────────────────────────────────────────┤
│        │                                                                    │
│  📊    │  ┌─── AAPL ─── Order Book ────────┐  ┌─── Price Chart ──────────┐ │
│ Trading│  │  ASK                            │  │                          │ │
│        │  │  185.08  ████ 800               │  │    ╱╲    ╱╲             │ │
│  📈    │  │  185.07  ██████ 1200            │  │   ╱  ╲  ╱  ╲    ╱╲     │ │
│ Signals│  │  185.06  ████████████ 2500      │  │  ╱    ╲╱    ╲  ╱  ╲    │ │
│        │  │  ─── spread: 0.01 ───           │  │ ╱              ╲╱   ╲   │ │
│  💰    │  │  185.05  ██████████ 2000        │  │          ▲ BUY    ▼ SELL │ │
│  PnL   │  │  185.04  ████████ 1500          │  │   signals overlaid       │ │
│        │  │  185.03  ██████ 1100            │  │                          │ │
│  ⚠️    │  │  BID                            │  └──────────────────────────┘ │
│  Risk  │  └─────────────────────────────────┘                               │
│        │                                                                    │
│  🤖    │  ┌─── Signals ───────────────────────────────────────────────────┐ │
│Strategy│  │  VWAP Cross    ███████████░░░  +0.72  BUY                     │ │
│        │  │  Order Flow    ██████░░░░░░░  +0.45  BUY                     │ │
│  📊    │  │  Mean Rev      ░░░░░░░░░░░░░  -0.12  NEUTRAL                 │ │
│Backtest│  │  Momentum      ████████████░  +0.81  BUY                     │ │
│        │  │  Composite α   ████████████░  +0.68  → LONG 300 shares       │ │
│  🖥️    │  └───────────────────────────────────────────────────────────────┘ │
│ System │                                                                    │
│        │  ┌─── Performance ──────────┐  ┌─── Risk ──────────────────────┐  │
│        │  │  PnL: +$4,231            │  │  Position: 300/1000 (30%)     │  │
│        │  │  Sharpe: 2.4             │  │  Exposure: $55k/$200k        │  │
│        │  │  Win Rate: 61%           │  │  Drawdown: -0.3% (limit 2%)  │  │
│        │  │  Trades: 47              │  │  Order Rate: 12/s (limit 50) │  │
│        │  └──────────────────────────┘  └───────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────────────────┤
│  Latency: p50 1.2μs  p99 7.1μs  │  Throughput: 150k msg/s  │  Gaps: 0    │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Line Count Breakdown

| Category | Files | Lines |
|----------|-------|-------|
| **C++ Headers** (include/qf/) | ~95 | ~12,000 |
| **C++ Source** (src/) | ~110 | ~42,000 |
| **C++ Tests** (tests/) | ~50 | ~5,000 |
| **C++ Benchmarks** (bench/) | 6 | ~1,000 |
| **React/TS Dashboard** (dashboard/) | ~65 | ~18,000 |
| **Config/YAML** (config/) | ~8 | ~500 |
| **Build/CI/Docker** | ~5 | ~500 |
| **Documentation** | ~5 | ~3,000 |
| **FIX Protocol** | ~6 | ~4,000 |
| **Infrastructure** | ~4 | ~1,000 |
| **Total** | **~354 files** | **~107,000** |

---

## Executables — What Ships

| Executable | What It Does |
|-----------|-------------|
| `exchange_simulator` | Generates realistic market data over UDP multicast |
| `feed_handler` | Receives data, builds books, computes signals, runs strategies |
| `backtest_runner` | Replays historical data through strategies, outputs analytics |
| `dashboard` (npm) | React web app connecting via WebSocket |

```bash
# Terminal 1 — Simulator
./exchange_simulator --config config/default_scenario.json

# Terminal 2 — Handler + Signals + Strategy + API
./feed_handler --config config/quantflow.yaml --signals --strategy momentum --ws-port 8080

# Terminal 3 — Dashboard
cd dashboard && npm run dev
# → http://localhost:3000

# Terminal 4 — Backtest
./backtest_runner --data data/2026-03-01/ --strategy mean_reversion --config config/strategies/mean_reversion.yaml
```

---

## Interview Story

> "I built a complete algorithmic trading platform from scratch. Two C++ executables — an exchange simulator that generates 500k messages per second, and a feed handler with a 4-thread lock-free pipeline that processes them with sub-10μs latency. On top of that, I added a signal engine that computes 11 features from the order book in real-time, a strategy engine with 5 built-in strategies, an OMS with a full order state machine, a risk engine with pre-trade checks and a circuit breaker, a backtest engine that reuses the same live code on historical data, and a React dashboard that visualizes everything over WebSocket. 107,000 lines, all mine."

That's not a side project. That's a trading desk.
