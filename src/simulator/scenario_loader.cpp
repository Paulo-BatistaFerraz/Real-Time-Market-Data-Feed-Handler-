#include "mdfh/simulator/scenario_loader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace mdfh::simulator {

// TODO: Implement JSON loading logic

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
    (void)json_str;
    return {};
}

}  // namespace mdfh::simulator
