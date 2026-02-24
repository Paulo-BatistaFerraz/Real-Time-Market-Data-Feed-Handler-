cmake_minimum_required(VERSION 3.16)
project(market-data-feed-handler LANGUAGES CXX)

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
)
target_link_libraries(feed_handler PRIVATE asio_lib ws2_32)

# --- exchange_simulator executable ---
add_executable(exchange_simulator
    src/simulator/main.cpp
)
target_link_libraries(exchange_simulator PRIVATE asio_lib nlohmann_json::nlohmann_json ws2_32)

# --- tests ---
enable_testing()

add_executable(run_tests
    tests/test_stub.cpp
)
target_link_libraries(run_tests PRIVATE GTest::gtest GTest::gtest_main)
add_test(NAME AllTests COMMAND run_tests)
