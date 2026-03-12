#include "qf/core/pipeline.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <csignal>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

// ── Signal handling ─────────────────────────────────────────────
static std::atomic<bool> g_shutdown{false};
static std::mutex g_mtx;
static std::condition_variable g_cv;

static void signal_handler(int /*sig*/) {
    g_shutdown.store(true, std::memory_order_release);
    g_cv.notify_all();
}

// ── Helpers ─────────────────────────────────────────────────────
static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [--config <path>] [--display]\n"
              << "  --config <path>  JSON scenario config file\n"
              << "  --display        Enable real-time console display\n";
}

static qf::core::PipelineConfig load_config(const std::string& path) {
    qf::core::PipelineConfig cfg;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[FEED] WARNING: Cannot open config '" << path
                  << "', using defaults\n";
        return cfg;
    }

    nlohmann::json j;
    file >> j;

    if (j.contains("multicast_group"))
        cfg.multicast_group = j["multicast_group"].get<std::string>();
    if (j.contains("port"))
        cfg.port = j["port"].get<uint16_t>();
    if (j.contains("display"))
        cfg.enable_display = j["display"].get<bool>();
    if (j.contains("csv_logging"))
        cfg.enable_csv = j["csv_logging"].get<bool>();
    if (j.contains("csv_path"))
        cfg.csv_path = j["csv_path"].get<std::string>();

    return cfg;
}

static void print_banner(const qf::core::PipelineConfig& cfg) {
    std::cout << "╔══════════════════════════════════════════╗\n"
              << "║   Market Data Feed Handler v1.0          ║\n"
              << "╚══════════════════════════════════════════╝\n"
              << "  Multicast : " << cfg.multicast_group
              << ":" << cfg.port << "\n"
              << "  Display   : " << (cfg.enable_display ? "ON" : "OFF") << "\n"
              << "  CSV Log   : " << (cfg.enable_csv ? cfg.csv_path : "OFF") << "\n"
              << "  Press Ctrl+C to stop.\n\n";
}

// ── Main ────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    qf::core::PipelineConfig config;
    std::string config_path;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--display") {
            config.enable_display = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "[FEED] Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Load config from JSON file (if provided)
    if (!config_path.empty()) {
        config = load_config(config_path);
    }

    // --display flag overrides config file
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--display") {
            config.enable_display = true;
            break;
        }
    }

    print_banner(config);

    // Register signal handlers for clean shutdown
    std::signal(SIGINT, signal_handler);
#ifdef SIGTERM
    std::signal(SIGTERM, signal_handler);
#endif

    // Create and start the pipeline
    qf::core::Pipeline pipeline(config);
    pipeline.start();

    // Wait for shutdown signal
    {
        std::unique_lock<std::mutex> lock(g_mtx);
        g_cv.wait(lock, [] { return g_shutdown.load(std::memory_order_acquire); });
    }

    std::cout << "\n[FEED] Shutting down...\n";
    pipeline.stop();
    std::cout << "[FEED] Clean shutdown complete.\n";

    return 0;
}
