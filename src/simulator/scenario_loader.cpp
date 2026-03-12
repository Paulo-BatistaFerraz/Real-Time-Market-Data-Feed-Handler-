#include "qf/simulator/scenario_loader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace qf::simulator {

using json = nlohmann::json;

SimConfig ScenarioLoader::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + path);
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return parse(content);
}

SimConfig ScenarioLoader::parse(const std::string& json_str) {
    json j;
    try {
        j = json::parse(json_str);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("Invalid JSON: ") + e.what());
    }

    // Validate required fields
    if (!j.contains("symbols") || !j["symbols"].is_array()) {
        throw std::runtime_error("Missing or invalid required field: symbols");
    }
    if (j["symbols"].empty()) {
        throw std::runtime_error("symbols array must not be empty");
    }
    if (!j.contains("initial_prices") || !j["initial_prices"].is_object()) {
        throw std::runtime_error("Missing or invalid required field: initial_prices");
    }

    SimConfig config;

    // Optional fields with defaults
    if (j.contains("multicast_address")) {
        config.multicast_ip = j["multicast_address"].get<std::string>();
    }
    if (j.contains("port")) {
        config.port = j["port"].get<uint16_t>();
    }
    if (j.contains("messages_per_second")) {
        config.msg_rate = j["messages_per_second"].get<uint32_t>();
    }
    if (j.contains("duration_seconds")) {
        config.duration_sec = j["duration_seconds"].get<uint64_t>();
    }
    if (j.contains("seed")) {
        config.seed = j["seed"].get<uint32_t>();
    }

    // Parse initial_prices map
    for (auto& [symbol, price] : j["initial_prices"].items()) {
        config.initial_prices[symbol] = price.get<Price>();
    }

    // Build SymbolConfig entries from symbols + initial_prices
    for (const auto& sym_name : j["symbols"]) {
        std::string name = sym_name.get<std::string>();

        SymbolConfig sc;
        sc.name = name;

        auto it = config.initial_prices.find(name);
        if (it != config.initial_prices.end()) {
            sc.start_price = static_cast<double>(it->second);
        } else {
            throw std::runtime_error("No initial price for symbol: " + name);
        }

        // Defaults for simulator parameters not in the JSON
        sc.volatility = 0.001;
        sc.tick_size  = 100.0;  // 1 cent in fixed-point (10000 = $1)

        config.symbols.push_back(std::move(sc));
    }

    return config;
}

}  // namespace qf::simulator
