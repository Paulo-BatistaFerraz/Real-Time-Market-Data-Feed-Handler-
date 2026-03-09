#pragma once

#include <string>
#include <cstdint>

namespace mdfh::utils {

// 1234567 → "1,234,567"
std::string format_number(uint64_t n);

// 1500000.0 → "1.5M/s"
std::string format_rate(double rate);

// 4523 ns → "4.5μs"
std::string format_latency_ns(uint64_t ns);

// 185.0500 → "185.0500" (4 decimal places)
std::string format_price(double price);

}  // namespace mdfh::utils
