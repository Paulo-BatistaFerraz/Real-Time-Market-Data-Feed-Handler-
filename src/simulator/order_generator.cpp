#include "qf/simulator/order_generator.hpp"
#include "qf/protocol/encoder.hpp"
#include "qf/common/clock.hpp"

namespace qf::simulator {

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
    int type = type_dist_(rng_);

    // If the book is empty, force an Add order since Cancel/Execute/Replace/Trade
    // all need existing orders to reference
    if (book_.empty() && type != 0) {
        type = 0;
    }

    size_t bytes = 0;
    switch (type) {
        case 0: bytes = generate_add(buffer, buffer_size); break;
        case 1: bytes = generate_cancel(buffer, buffer_size); break;
        case 2: bytes = generate_execute(buffer, buffer_size); break;
        case 3: bytes = generate_replace(buffer, buffer_size); break;
        case 4: {
            // Trade: pick orders from both sides if possible, otherwise fall back to Add
            auto bid_id = book_.random_live_order(Side::Buy, rng_);
            auto ask_id = book_.random_live_order(Side::Sell, rng_);
            if (bid_id && ask_id) {
                const auto* bid_order = book_.find(*bid_id);
                const auto* ask_order = book_.find(*ask_id);
                if (bid_order && ask_order) {
                    Quantity trade_qty = std::min(bid_order->remaining_quantity,
                                                  ask_order->remaining_quantity);
                    if (trade_qty == 0) trade_qty = 1;

                    TradeMessage msg;
                    msg.symbol = bid_order->symbol;
                    msg.price = ask_order->price;
                    msg.quantity = trade_qty;
                    msg.buy_order_id = *bid_id;
                    msg.sell_order_id = *ask_id;

                    auto ts = Clock::nanos_since_midnight();
                    bytes = protocol::Encoder::encode(msg, ts, buffer, buffer_size);
                    if (bytes > 0) {
                        // Execute both sides for the trade quantity
                        book_.execute(*bid_id, trade_qty);
                        book_.execute(*ask_id, trade_qty);
                        ++total_generated_;
                    }
                    return bytes;
                }
            }
            // Fall back to Add if we can't form a trade
            bytes = generate_add(buffer, buffer_size);
            break;
        }
        default: bytes = generate_add(buffer, buffer_size); break;
    }
    return bytes;
}

size_t OrderGenerator::generate_add(uint8_t* buffer, size_t buffer_size) {
    size_t sym_idx = symbol_dist_(rng_);
    std::uniform_int_distribution<int> side_dist(0, 1);

    AddOrder msg;
    msg.order_id = next_order_id_++;
    msg.side = side_dist(rng_) == 0 ? Side::Buy : Side::Sell;
    msg.symbol = symbols_[sym_idx];
    msg.price = price_walks_[sym_idx].next();
    msg.quantity = random_quantity();

    auto ts = Clock::nanos_since_midnight();
    size_t bytes = protocol::Encoder::encode(msg, ts, buffer, buffer_size);
    if (bytes > 0) {
        core::Order order(msg.order_id, msg.symbol, msg.side,
                          msg.price, msg.quantity, ts);
        book_.add(order);
        ++total_generated_;
    }
    return bytes;
}

size_t OrderGenerator::generate_cancel(uint8_t* buffer, size_t buffer_size) {
    auto id = book_.random_live_order(rng_);
    if (!id) return generate_add(buffer, buffer_size);

    CancelOrder msg;
    msg.order_id = *id;

    auto ts = Clock::nanos_since_midnight();
    size_t bytes = protocol::Encoder::encode(msg, ts, buffer, buffer_size);
    if (bytes > 0) {
        book_.cancel(*id);
        ++total_generated_;
    }
    return bytes;
}

size_t OrderGenerator::generate_execute(uint8_t* buffer, size_t buffer_size) {
    auto id = book_.random_live_order(rng_);
    if (!id) return generate_add(buffer, buffer_size);

    const auto* order = book_.find(*id);
    if (!order) return generate_add(buffer, buffer_size);

    // Execute a random portion of the remaining quantity
    std::uniform_int_distribution<Quantity> qty_dist(1, order->remaining_quantity);
    ExecuteOrder msg;
    msg.order_id = *id;
    msg.exec_quantity = qty_dist(rng_);

    auto ts = Clock::nanos_since_midnight();
    size_t bytes = protocol::Encoder::encode(msg, ts, buffer, buffer_size);
    if (bytes > 0) {
        book_.execute(*id, msg.exec_quantity);
        ++total_generated_;
    }
    return bytes;
}

size_t OrderGenerator::generate_replace(uint8_t* buffer, size_t buffer_size) {
    auto id = book_.random_live_order(rng_);
    if (!id) return generate_add(buffer, buffer_size);

    const auto* order = book_.find(*id);
    if (!order) return generate_add(buffer, buffer_size);

    // Find the symbol index for this order to get a new price from its PriceWalk
    size_t sym_idx = 0;
    for (size_t i = 0; i < symbols_.size(); ++i) {
        if (symbols_[i] == order->symbol) {
            sym_idx = i;
            break;
        }
    }

    ReplaceOrder msg;
    msg.order_id = *id;
    msg.new_price = price_walks_[sym_idx].next();
    msg.new_quantity = random_quantity();

    auto ts = Clock::nanos_since_midnight();
    size_t bytes = protocol::Encoder::encode(msg, ts, buffer, buffer_size);
    if (bytes > 0) {
        book_.replace(*id, msg.new_price, msg.new_quantity);
        ++total_generated_;
    }
    return bytes;
}

Quantity OrderGenerator::random_quantity() {
    std::uniform_int_distribution<Quantity> dist(config_.min_qty, config_.max_qty);
    return dist(rng_);
}

}  // namespace qf::simulator
