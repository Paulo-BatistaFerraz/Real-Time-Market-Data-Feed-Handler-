#include "qf/simulator/participant_model.hpp"
#include "qf/protocol/encoder.hpp"
#include "qf/common/clock.hpp"
#include <algorithm>
#include <cmath>

namespace qf::simulator {

// --- Default profiles for each trader type ---

TraderProfile default_market_maker_profile() {
    TraderProfile p;
    p.type            = TraderType::MarketMaker;
    p.size_mean       = 200.0;
    p.size_stddev     = 80.0;
    p.min_qty         = 50;
    p.max_qty         = 2000;
    p.add_weight      = 0.50;   // Heavy quoting
    p.cancel_weight   = 0.30;   // High cancel rate (requoting)
    p.execute_weight  = 0.05;
    p.replace_weight  = 0.10;   // Frequent price updates
    p.trade_weight    = 0.05;
    p.spread_ticks_mean   = 2.0;    // Tight spreads
    p.spread_ticks_stddev = 1.0;
    p.cancel_rate     = 0.40;   // 40% cancel rate
    return p;
}

TraderProfile default_momentum_profile() {
    TraderProfile p;
    p.type            = TraderType::MomentumTrader;
    p.size_mean       = 300.0;
    p.size_stddev     = 150.0;
    p.min_qty         = 100;
    p.max_qty         = 5000;
    p.add_weight      = 0.55;   // Aggressive adding
    p.cancel_weight   = 0.15;   // Moderate cancel
    p.execute_weight  = 0.15;
    p.replace_weight  = 0.10;
    p.trade_weight    = 0.05;
    p.spread_ticks_mean   = 1.0;    // Aggressive, near top-of-book
    p.spread_ticks_stddev = 0.5;
    p.cancel_rate     = 0.15;   // 15% cancel rate
    return p;
}

TraderProfile default_random_profile() {
    TraderProfile p;
    p.type            = TraderType::RandomTrader;
    p.size_mean       = 100.0;
    p.size_stddev     = 50.0;
    p.min_qty         = 10;
    p.max_qty         = 1000;
    p.add_weight      = 0.40;
    p.cancel_weight   = 0.25;
    p.execute_weight  = 0.20;
    p.replace_weight  = 0.10;
    p.trade_weight    = 0.05;
    p.spread_ticks_mean   = 5.0;    // Wide, random placement
    p.spread_ticks_stddev = 3.0;
    p.cancel_rate     = 0.10;   // 10% cancel rate
    return p;
}

ParticipantConfig default_participant_config() {
    ParticipantConfig c;
    c.num_market_makers    = 2;
    c.num_momentum_traders = 3;
    c.num_random_traders   = 5;
    c.market_maker_profile = default_market_maker_profile();
    c.momentum_profile     = default_momentum_profile();
    c.random_profile       = default_random_profile();
    return c;
}

// --- Participant ---

Participant::Participant(uint32_t id, const TraderProfile& profile,
                         const std::vector<Symbol>& symbols,
                         std::vector<PriceWalk>& price_walks,
                         SimOrderBook& book,
                         uint32_t seed)
    : id_(id)
    , profile_(profile)
    , symbols_(symbols)
    , price_walks_(price_walks)
    , book_(book)
    , rng_(seed)
    , type_dist_({profile.add_weight, profile.cancel_weight,
                  profile.execute_weight, profile.replace_weight,
                  profile.trade_weight})
    , symbol_dist_(0, symbols.empty() ? 0 : symbols.size() - 1)
    , size_dist_(profile.size_mean, profile.size_stddev)
    , next_order_id_(static_cast<OrderId>(id) * 1000000 + 1)
{
    // Initialize previous prices for momentum tracking
    prev_prices_.reserve(price_walks_.size());
    for (size_t i = 0; i < price_walks_.size(); ++i) {
        prev_prices_.push_back(price_walks_[i].current_double());
    }
}

size_t Participant::next(uint8_t* buffer, size_t buffer_size) {
    int type = type_dist_(rng_);

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
                        book_.execute(*bid_id, trade_qty);
                        book_.execute(*ask_id, trade_qty);
                        ++total_generated_;
                    }
                    return bytes;
                }
            }
            bytes = generate_add(buffer, buffer_size);
            break;
        }
        default: bytes = generate_add(buffer, buffer_size); break;
    }
    return bytes;
}

size_t Participant::generate_add(uint8_t* buffer, size_t buffer_size) {
    size_t sym_idx = symbol_dist_(rng_);

    AddOrder msg;
    msg.order_id = next_order_id_++;
    msg.side = choose_side(sym_idx);
    msg.symbol = symbols_[sym_idx];
    msg.price = price_walks_[sym_idx].next();
    msg.quantity = random_quantity();

    auto ts = Clock::nanos_since_midnight();
    size_t bytes = protocol::Encoder::encode(msg, ts, buffer, buffer_size);
    if (bytes > 0) {
        core::Order order(msg.order_id, msg.symbol, msg.side,
                          msg.price, msg.quantity, ts);
        book_.add(order);

        // Update momentum tracking
        prev_prices_[sym_idx] = price_walks_[sym_idx].current_double();

        ++total_generated_;
    }
    return bytes;
}

size_t Participant::generate_cancel(uint8_t* buffer, size_t buffer_size) {
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

size_t Participant::generate_execute(uint8_t* buffer, size_t buffer_size) {
    auto id = book_.random_live_order(rng_);
    if (!id) return generate_add(buffer, buffer_size);

    const auto* order = book_.find(*id);
    if (!order) return generate_add(buffer, buffer_size);

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

size_t Participant::generate_replace(uint8_t* buffer, size_t buffer_size) {
    auto id = book_.random_live_order(rng_);
    if (!id) return generate_add(buffer, buffer_size);

    const auto* order = book_.find(*id);
    if (!order) return generate_add(buffer, buffer_size);

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

Quantity Participant::random_quantity() {
    double raw = size_dist_(rng_);
    auto qty = static_cast<Quantity>(std::max(raw, static_cast<double>(profile_.min_qty)));
    return std::min(qty, profile_.max_qty);
}

Side Participant::choose_side(size_t symbol_idx) {
    switch (profile_.type) {
        case TraderType::MarketMaker: {
            // Market makers quote both sides roughly equally
            std::uniform_int_distribution<int> side_dist(0, 1);
            return side_dist(rng_) == 0 ? Side::Buy : Side::Sell;
        }
        case TraderType::MomentumTrader: {
            // Follow the trend: buy if price is rising, sell if falling
            double current = price_walks_[symbol_idx].current_double();
            double prev = prev_prices_[symbol_idx];
            if (current > prev) {
                // Price rising — buy with 80% probability
                std::uniform_real_distribution<double> p(0.0, 1.0);
                return p(rng_) < 0.80 ? Side::Buy : Side::Sell;
            } else if (current < prev) {
                // Price falling — sell with 80% probability
                std::uniform_real_distribution<double> p(0.0, 1.0);
                return p(rng_) < 0.80 ? Side::Sell : Side::Buy;
            }
            // Flat — random
            std::uniform_int_distribution<int> side_dist(0, 1);
            return side_dist(rng_) == 0 ? Side::Buy : Side::Sell;
        }
        case TraderType::RandomTrader:
        default: {
            std::uniform_int_distribution<int> side_dist(0, 1);
            return side_dist(rng_) == 0 ? Side::Buy : Side::Sell;
        }
    }
}

// --- ParticipantModel ---

ParticipantModel::ParticipantModel(const std::vector<Symbol>& symbols,
                                   std::vector<PriceWalk>& price_walks,
                                   SimOrderBook& book,
                                   uint32_t seed,
                                   const ParticipantConfig& config)
    : rng_(seed)
{
    uint32_t participant_id = 0;
    uint32_t participant_seed = seed + 1000;

    // Create market makers
    for (uint32_t i = 0; i < config.num_market_makers; ++i) {
        participants_.emplace_back(participant_id++, config.market_maker_profile,
                                   symbols, price_walks, book, participant_seed++);
    }

    // Create momentum traders
    for (uint32_t i = 0; i < config.num_momentum_traders; ++i) {
        participants_.emplace_back(participant_id++, config.momentum_profile,
                                   symbols, price_walks, book, participant_seed++);
    }

    // Create random traders
    for (uint32_t i = 0; i < config.num_random_traders; ++i) {
        participants_.emplace_back(participant_id++, config.random_profile,
                                   symbols, price_walks, book, participant_seed++);
    }

    if (!participants_.empty()) {
        participant_dist_ = std::uniform_int_distribution<size_t>(0, participants_.size() - 1);
    }
}

size_t ParticipantModel::next(uint8_t* buffer, size_t buffer_size) {
    if (participants_.empty()) return 0;
    size_t idx = participant_dist_(rng_);
    return participants_[idx].next(buffer, buffer_size);
}

uint64_t ParticipantModel::total_generated() const {
    uint64_t total = 0;
    for (const auto& p : participants_) {
        total += p.total_generated();
    }
    return total;
}

}  // namespace qf::simulator
