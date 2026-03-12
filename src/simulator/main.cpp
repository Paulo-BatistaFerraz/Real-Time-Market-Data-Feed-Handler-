#include "qf/simulator/market_simulator.hpp"
#include "qf/simulator/scenario_loader.hpp"

#include <csignal>
#include <iomanip>
#include <iostream>
#include <string>

// ── Signal handling ─────────────────────────────────────────────
static qf::simulator::MarketSimulator* g_simulator = nullptr;

static void signal_handler(int /*sig*/) {
    std::cout << "\n[SIM] Caught signal, shutting down...\n";
    if (g_simulator) {
        g_simulator->stop();
    }
}

// ── Helpers ─────────────────────────────────────────────────────
static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [--config <path>]\n"
              << "  --config <path>  JSON scenario config file\n"
              << "  --help, -h       Show this help\n";
}

static void print_banner(const qf::simulator::SimConfig& cfg) {
    std::cout << "╔══════════════════════════════════════════╗\n"
              << "║   Exchange Simulator v1.0                ║\n"
              << "╚══════════════════════════════════════════╝\n"
              << "  Symbols   : ";

    for (size_t i = 0; i < cfg.symbols.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << cfg.symbols[i].name;
    }
    std::cout << " (" << cfg.symbols.size() << ")\n"
              << "  Rate      : " << cfg.msg_rate << " msgs/sec\n"
              << "  Duration  : " << cfg.duration_sec << " sec\n"
              << "  Multicast : " << cfg.multicast_ip << ":" << cfg.port << "\n"
              << "  Press Ctrl+C to stop.\n\n";
}

static void print_exit_summary(const qf::simulator::SimStats& st) {
    std::cout << "\n[SIM] ── Exit Summary ─────────────────────\n"
              << "[SIM] Messages sent : " << st.total_messages << "\n"
              << "[SIM] Actual rate   : " << std::fixed << std::setprecision(1)
              << st.actual_rate << " msgs/sec\n"
              << "[SIM] Duration      : " << std::fixed << std::setprecision(2)
              << st.duration_sec << " sec\n"
              << "[SIM] ────────────────────────────────────\n";
}

// ── Main ────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    std::string config_path;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--config") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "[SIM] Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Load config
    qf::simulator::SimConfig config;

    if (!config_path.empty()) {
        try {
            config = qf::simulator::ScenarioLoader::load(config_path);
            std::cout << "[SIM] Loaded scenario: " << config_path << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[SIM] Error loading scenario: " << e.what() << "\n";
            return 1;
        }
    } else {
        // Default scenario with two symbols
        config.symbols = {
            {"AAPL", 185.0, 0.02, 0.01},
            {"MSFT", 420.0, 0.015, 0.01}
        };
        std::cout << "[SIM] Using default scenario (2 symbols)\n";
    }

    // Print startup banner
    print_banner(config);

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
#ifdef SIGTERM
    std::signal(SIGTERM, signal_handler);
#endif

    // Create and run simulator
    qf::simulator::MarketSimulator simulator(config);
    g_simulator = &simulator;

    simulator.run();

    g_simulator = nullptr;

    // Print exit summary
    print_exit_summary(simulator.stats());

    std::cout << "[SIM] Clean shutdown complete.\n";
    return 0;
}
