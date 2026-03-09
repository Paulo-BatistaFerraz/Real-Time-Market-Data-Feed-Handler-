#pragma once

#include <thread>
#include <cstdint>

namespace mdfh::utils {

// Best-effort thread pinning to a specific CPU core
// Logs warning on failure, does not throw
bool pin_thread(std::thread& t, uint32_t core_id);

// Pin the calling thread
bool pin_current_thread(uint32_t core_id);

// Get number of available cores
uint32_t available_cores();

}  // namespace mdfh::utils
