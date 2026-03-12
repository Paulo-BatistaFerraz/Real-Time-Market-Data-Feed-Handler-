#include "qf/simulator/market_simulator.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>

namespace qf::simulator {

MarketSimulator::MarketSimulator(const SimConfig& config)
    : config_(config)
    , volatility_regime_(config.seed + 100)
    , news_generator_(config.seed + 200, config.symbols.size())
    , batcher_(MAX_DATAGRAM)
    , sender_(std::make_unique<network::MulticastSender>(
          io_ctx_, config.multicast_ip, config.port))
{
    // Create price walks for each symbol and store base volatilities
    for (const auto& sym : config.symbols) {
        price_walks_.emplace_back(sym.start_price, sym.volatility, sym.tick_size, config.seed);
        base_volatilities_.push_back(sym.volatility);
    }

    // Build symbol list
    std::vector<Symbol> symbols;
    for (const auto& sym : config.symbols) {
        symbols.emplace_back(sym.name.c_str());
    }

    matching_engine_ = std::make_unique<SimMatchingEngine>(book_);
    generator_ = std::make_unique<OrderGenerator>(symbols, price_walks_, book_, config.seed);
}

void MarketSimulator::run() {
    running_.store(true);

    std::cout << "[SIM] Starting Market Simulator\n"
              << "[SIM] Rate: " << config_.msg_rate << " msgs/sec\n"
              << "[SIM] Duration: " << config_.duration_sec << " sec\n"
              << "[SIM] Multicast: " << config_.multicast_ip << ":" << config_.port << "\n"
              << "[SIM] Symbols: " << config_.symbols.size() << "\n";

    const auto start = std::chrono::steady_clock::now();
    const auto end_time = start + std::chrono::seconds(config_.duration_sec);

    // Rate limiting: compute interval between messages
    const auto interval_ns = (config_.msg_rate > 0)
        ? std::chrono::nanoseconds(1'000'000'000ULL / config_.msg_rate)
        : std::chrono::nanoseconds(0);

    uint64_t messages_sent = 0;
    uint8_t encode_buf[MAX_DATAGRAM];
    auto next_send = start;

    auto send_callback = [this](const uint8_t* data, size_t len) {
        sender_->send(data, len);
    };

    while (running_.load(std::memory_order_relaxed)) {
        auto now = std::chrono::steady_clock::now();
        if (now >= end_time) break;

        // Update volatility regime and apply multiplier to all price walks
        volatility_regime_.update();
        double vol_mult = volatility_regime_.volatility_multiplier();
        for (size_t i = 0; i < price_walks_.size(); ++i) {
            price_walks_[i].set_volatility(base_volatilities_[i] * vol_mult);
        }

        // Check for news events and apply price jumps
        auto event = news_generator_.check();
        if (event) {
            apply_news_event(*event);
        }

        // Adjust effective interval based on regime rate multiplier
        double rate_mult = volatility_regime_.rate_multiplier();
        auto effective_interval = (rate_mult > 0.0)
            ? std::chrono::nanoseconds(
                  static_cast<uint64_t>(interval_ns.count() / rate_mult))
            : interval_ns;

        // Rate limiting: wait until next scheduled send time
        if (now < next_send) {
            auto wait = next_send - now;
            if (wait > std::chrono::microseconds(100)) {
                std::this_thread::sleep_for(wait);
            }
            continue;
        }

        // Generate next encoded message
        size_t bytes = generator_->next(encode_buf, sizeof(encode_buf));
        if (bytes == 0) {
            next_send += effective_interval;
            continue;
        }

        // Add to batcher — auto-flush when full
        batcher_.add_and_flush(encode_buf, bytes, send_callback);
        ++messages_sent;
        next_send += effective_interval;
    }

    // Flush any remaining messages in the batcher
    batcher_.flush(send_callback);

    running_.store(false);

    // Compute stats
    auto actual_end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(actual_end - start).count();

    stats_.total_messages = messages_sent;
    stats_.total_batches = batcher_.batches_sent();
    stats_.duration_sec = elapsed;
    stats_.actual_rate = (elapsed > 0.0) ? static_cast<double>(messages_sent) / elapsed : 0.0;

    print_summary();
}

void MarketSimulator::stop() {
    running_.store(false);
}

void MarketSimulator::apply_news_event(const NewsEvent& event) {
    if (event.scope == EventScope::Systemic) {
        for (auto& pw : price_walks_) {
            pw.apply_jump(event.price_jump);
        }
    } else if (event.symbol_index < price_walks_.size()) {
        price_walks_[event.symbol_index].apply_jump(event.price_jump);
    }
}

void MarketSimulator::print_summary() const {
    std::cout << "\n[SIM] ── Summary ──────────────────────────\n"
              << "[SIM] Total messages sent: " << stats_.total_messages << "\n"
              << "[SIM] Total batches sent:  " << stats_.total_batches << "\n"
              << "[SIM] Duration:            " << std::fixed << std::setprecision(2)
              << stats_.duration_sec << " sec\n"
              << "[SIM] Actual rate:         " << std::fixed << std::setprecision(1)
              << stats_.actual_rate << " msgs/sec\n"
              << "[SIM] ────────────────────────────────────\n";
}

}  // namespace qf::simulator
