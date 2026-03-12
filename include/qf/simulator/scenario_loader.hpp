#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "qf/common/constants.hpp"
#include "qf/common/types.hpp"

namespace qf::simulator {

struct SymbolConfig {
    std::string name;
    double start_price;
    double volatility;
    double tick_size;
};

struct SimConfig {
    std::vector<SymbolConfig> symbols;
    uint32_t    msg_rate     = DEFAULT_MSG_RATE;
    uint64_t    duration_sec = DEFAULT_DURATION;
    uint32_t    seed         = DEFAULT_SEED;
    std::string multicast_ip = MULTICAST_GROUP;
    uint16_t    port         = MULTICAST_PORT;
    std::map<std::string, Price> initial_prices;
};

class ScenarioLoader {
public:
    // Load config from JSON file. Fills defaults for missing fields.
    static SimConfig load(const std::string& path);

    // Load from JSON string (for testing)
    static SimConfig parse(const std::string& json_str);
};

}  // namespace qf::simulator
