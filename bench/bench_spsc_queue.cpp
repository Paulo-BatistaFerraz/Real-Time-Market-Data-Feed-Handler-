#include <iostream>
#include <thread>
#include <chrono>
#include "mdfh/core/spsc_queue.hpp"

using namespace mdfh::core;

// TODO: SPSC queue throughput benchmark
// Producer pushes N items, consumer pops, measure msgs/sec

int main() {
    std::cout << "[BENCH] SPSC Queue Throughput\n";
    std::cout << "[BENCH] TODO: implement\n";
    return 0;
}
