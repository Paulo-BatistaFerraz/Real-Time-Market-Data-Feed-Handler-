cmake_minimum_required(VERSION 3.16)
project(market-data-feed-handler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Fetch dependencies ---
include(FetchContent)

# Standalone Asio (same author as Boost.Asio, same API, no Boost dependency)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

# nlohmann/json (header-only)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# yaml-cpp (YAML parser)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG        0.8.0
    GIT_SHALLOW    TRUE
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# spdlog (structured logging)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# LZ4 (fast compression)
FetchContent_Declare(
    lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG        v1.10.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(lz4)

# LZ4 is not a CMake project — create a library target from the C source
add_library(lz4_lib STATIC ${lz4_SOURCE_DIR}/lib/lz4.c)
target_include_directories(lz4_lib PUBLIC ${lz4_SOURCE_DIR}/lib)

# Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Asio target (header-only, needs compile defs) ---
add_library(asio_lib INTERFACE)
target_include_directories(asio_lib INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio_lib INTERFACE ASIO_STANDALONE)

# Include paths
include_directories(${CMAKE_SOURCE_DIR}/include)

# --- feed_handler executable ---
add_executable(feed_handler
    src/handler/main.cpp
    src/handler/network_stage.cpp
    src/handler/parser_stage.cpp
    src/handler/book_stage.cpp
    src/handler/consumer_stage.cpp
    src/core/pipeline.cpp
    src/network/multicast_sender.cpp
    src/network/multicast_receiver.cpp
    src/protocol/frame_parser.cpp
    src/protocol/validator.cpp
    src/protocol/sequence.cpp
    src/protocol/batcher.cpp
    src/core/order_store.cpp
    src/core/order_book.cpp
    src/core/book_manager.cpp
    src/consumer/latency_histogram.cpp
    src/consumer/throughput_tracker.cpp
    src/consumer/stats_collector.cpp
    src/consumer/console_display.cpp
    src/consumer/csv_logger.cpp
    src/network/tcp_server.cpp
    src/network/tcp_client.cpp
    src/infra/config.cpp
    src/infra/logging.cpp
)
target_link_libraries(feed_handler PRIVATE asio_lib nlohmann_json::nlohmann_json yaml-cpp::yaml-cpp spdlog::spdlog ws2_32 mswsock)

# --- exchange_simulator executable ---
add_executable(exchange_simulator
    src/simulator/main.cpp
    src/simulator/scenario_loader.cpp
    src/simulator/price_walk.cpp
    src/simulator/order_generator.cpp
    src/simulator/sim_order_book.cpp
    src/simulator/sim_matching_engine.cpp
    src/simulator/market_simulator.cpp
    src/simulator/volatility_regime.cpp
    src/simulator/news_event_generator.cpp
    src/simulator/participant_model.cpp
    src/simulator/multi_asset_sim.cpp
    src/network/multicast_sender.cpp
    src/network/multicast_receiver.cpp
    src/protocol/batcher.cpp
    src/infra/config.cpp
    src/infra/logging.cpp
)
target_link_libraries(exchange_simulator PRIVATE asio_lib nlohmann_json::nlohmann_json yaml-cpp::yaml-cpp spdlog::spdlog ws2_32)

# --- tests ---
enable_testing()

add_executable(run_tests
    tests/test_stub.cpp
    tests/test_feature_store.cpp
    tests/test_vwap.cpp
    tests/test_twap.cpp
    tests/test_microprice.cpp
    tests/test_ema.cpp
    tests/test_bollinger.cpp
    tests/test_rsi.cpp
    tests/test_macd.cpp
    tests/test_zscore.cpp
    tests/test_order_flow_imbalance.cpp
    tests/test_alpha_combiner.cpp
    tests/test_feature_normalizer.cpp
    tests/test_signal_engine.cpp
    tests/test_market_making.cpp
    tests/test_position_tracker.cpp
    tests/test_pnl_calculator.cpp
    tests/test_order_sizer.cpp
    tests/core/test_order_store.cpp
    tests/core/test_order_book.cpp
    tests/simulator/test_price_walk.cpp
    tests/simulator/test_scenario_loader.cpp
    tests/simulator/test_order_generator.cpp
    tests/simulator/test_sim_matching_engine.cpp
    src/core/order_store.cpp
    src/core/order_book.cpp
    src/core/book_manager.cpp
    src/simulator/price_walk.cpp
    src/simulator/scenario_loader.cpp
    src/simulator/order_generator.cpp
    src/simulator/sim_order_book.cpp
    src/simulator/sim_matching_engine.cpp
    src/signals/features/vwap.cpp
    src/signals/features/twap.cpp
    src/signals/features/microprice.cpp
    src/signals/features/order_flow_imbalance.cpp
    src/signals/features/book_depth_ratio.cpp
    src/signals/features/spread_tracker.cpp
    src/signals/features/trade_flow.cpp
    src/signals/features/volatility_estimator.cpp
    src/signals/features/momentum.cpp
    src/signals/features/mean_reversion.cpp
    src/signals/features/tick_intensity.cpp
    src/signals/indicators/ema.cpp
    src/signals/indicators/bollinger.cpp
    src/signals/indicators/rsi.cpp
    src/signals/indicators/macd.cpp
    src/signals/indicators/zscore.cpp
    src/signals/composite/alpha_combiner.cpp
    src/signals/composite/signal_decay.cpp
    src/signals/composite/regime_detector.cpp
    src/signals/ml/feature_store.cpp
    src/signals/ml/feature_normalizer.cpp
    src/signals/ml/linear_model.cpp
    src/signals/ml/model_loader.cpp
    src/signals/ml/prediction_cache.cpp
    src/signals/signal_registry.cpp
    src/signals/signal_engine.cpp
    src/strategy/strategy_registry.cpp
    src/strategy/strategy_engine.cpp
    src/strategy/position/position_tracker.cpp
    src/strategy/position/fill_tracker.cpp
    src/strategy/position/pnl_calculator.cpp
    src/strategy/position/portfolio.cpp
    src/strategy/execution/order_sizer.cpp
    src/strategy/execution/slippage_model.cpp
    src/strategy/execution/execution_algo.cpp
    src/strategy/strategies/market_making.cpp
    src/strategy/strategies/momentum_follower.cpp
    src/strategy/strategies/mean_reversion_strat.cpp
    src/strategy/strategies/stat_arb.cpp
    src/strategy/strategies/signal_threshold.cpp
    src/oms/state_machine/state_transitions.cpp
    src/oms/state_machine/order_journal.cpp
    src/oms/order_manager.cpp
    src/oms/order_validator.cpp
    src/oms/order_router.cpp
    src/oms/fill_manager.cpp
    src/oms/order_id_generator.cpp
    src/oms/sim_exchange/sim_exchange.cpp
    src/oms/sim_exchange/sim_fill_model.cpp
    src/oms/sim_exchange/sim_exchange_connector.cpp
    src/risk/limits/position_limit.cpp
    src/risk/limits/notional_limit.cpp
    src/risk/limits/loss_limit.cpp
    src/risk/limits/order_rate_limit.cpp
    src/risk/limits/concentration_limit.cpp
    src/risk/pre_trade_risk.cpp
    src/risk/post_trade_risk.cpp
    src/risk/monitors/pnl_monitor.cpp
    src/risk/monitors/exposure_monitor.cpp
    src/risk/monitors/var_calculator.cpp
    src/risk/monitors/circuit_breaker.cpp
    src/risk/reporting/risk_report.cpp
    src/risk/reporting/risk_logger.cpp
    src/data/recorder/binary_writer.cpp
    src/data/recorder/tick_recorder.cpp
    src/data/recorder/csv_writer.cpp
    src/data/recorder/compression.cpp
    src/data/recorder/rotation.cpp
    src/data/storage/tick_store.cpp
    src/data/storage/time_index.cpp
    src/data/storage/bar_aggregator.cpp
    src/data/storage/symbol_catalog.cpp
    src/data/replay/replay_clock.cpp
    src/data/replay/replay_engine.cpp
    src/data/replay/multi_stream_merger.cpp
    src/data/replay/replay_publisher.cpp
    tests/test_circuit_breaker.cpp
    tests/test_position_limit.cpp
    tests/test_loss_limit.cpp
    tests/test_var_calculator.cpp
    tests/test_state_machine.cpp
    tests/test_order_manager.cpp
    tests/test_sim_exchange.cpp
    tests/test_tick_recorder.cpp
    tests/test_bar_aggregator.cpp
    tests/test_replay_engine.cpp
    tests/test_compression.cpp
    src/infra/config.cpp
    src/infra/logging.cpp
)
target_link_libraries(run_tests PRIVATE GTest::gtest GTest::gtest_main nlohmann_json::nlohmann_json yaml-cpp::yaml-cpp spdlog::spdlog lz4_lib)
add_test(NAME AllTests COMMAND run_tests)
