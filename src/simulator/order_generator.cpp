#include "mdfh/simulator/order_generator.hpp"
#include "mdfh/protocol/encoder.hpp"
#include "mdfh/common/clock.hpp"

namespace mdfh::simulator {

// TODO: Implement message generation logic

OrderGenerator::OrderGenerator(const std::vector<Symbol>& symbols,
                               std::vector<PriceWalk>& price_walks,
                               SimOrderBook& book,
                               uint32_t seed,
                               const GeneratorConfig& config)
    : symbols_(symbols)
    , price_walks_(price_walks)
    , book_(book)
    , config_(config)
    , rng_(seed)
    , type_dist_({config.add_weight, config.cancel_weight,
                  config.execute_weight, config.replace_weight,
                  config.trade_weight})
    , symbol_dist_(0, symbols.empty() ? 0 : symbols.size() - 1) {}

size_t OrderGenerator::next(uint8_t* buffer, size_t buffer_size) {
    (void)buffer; (void)buffer_size;
    return 0;
}

size_t OrderGenerator::generate_add(uint8_t* buffer, size_t buffer_size) {
    (void)buffer; (void)buffer_size;
    return 0;
}

size_t OrderGenerator::generate_cancel(uint8_t* buffer, size_t buffer_size) {
    (void)buffer; (void)buffer_size;
    return 0;
}

size_t OrderGenerator::generate_execute(uint8_t* buffer, size_t buffer_size) {
    (void)buffer; (void)buffer_size;
    return 0;
}

size_t OrderGenerator::generate_replace(uint8_t* buffer, size_t buffer_size) {
    (void)buffer; (void)buffer_size;
    return 0;
}

Quantity OrderGenerator::random_quantity() {
    return 0;
}

}  // namespace mdfh::simulator
