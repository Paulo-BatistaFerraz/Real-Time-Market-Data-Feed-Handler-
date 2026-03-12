#pragma once

#include <atomic>
#include <functional>

namespace qf::utils {

// Install SIGINT/SIGTERM handler that sets an atomic flag
// Call this once at program startup
void install_signal_handler(std::atomic<bool>& running_flag);

// Alternative: install with callback
void install_signal_handler(std::function<void()> on_signal);

}  // namespace qf::utils
