#pragma once

#include <random>
#include <vector>
#include <cstdint>
#include "mdfh/protocol/messages.hpp"
#include "mdfh/simulator/price_walk.hpp"
#include "mdfh/simulator/sim_order_book.hpp"

namespace mdfh::simulator {

struct GeneratorConfig {
    double add_weight     = 0.40;
    double cancel_weight  = 0.25;
    double execute_weight = 0.20;
    double replace_weight = 0.10;
    double trade_weight   = 0.05;
    uint32_t min_qty      = 10;
    uint32_t max_qty      = 1000;
};

class OrderGenerator {
public:
    OrderGenerator(const std::vector<Symbol>& symbols,
                   std::vector<PriceWalk>& price_walks,
                   SimOrderBook& book,
                   uint32_t seed,
                   const GeneratorConfig& config = {});

    // Generate next message and encode into buffer
    // Returns bytes written (0 on failure)
    size_t next(uint8_t* buffer, size_t buffer_size);

    uint64_t total_generated() const { return total_generated_; }

private:
    std::vector<Symbol> symbols_;
    std::vector<PriceWalk>& price_walks_;
    SimOrderBook& book_;
    GeneratorConfig config_;

    std::mt19937 rng_;
    std::discrete_distribution<int> type_dist_;
    std::uniform_int_distribution<size_t> symbol_dist_;
    OrderId next_order_id_ = 1;
    uint64_t total_generated_ = 0;

    size_t generate_add(uint8_t* buffer, size_t buffer_size);
    size_t generate_cancel(uint8_t* buffer, size_t buffer_size);
    size_t generate_execute(uint8_t* buffer, size_t buffer_size);
    size_t generate_replace(uint8_t* buffer, size_t buffer_size);
    Quantity random_quantity();
};

}  // namespace mdfh::simulator
