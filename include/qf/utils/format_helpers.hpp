#pragma once

#include <string>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace qf::utils {

// 1234567 → "1,234,567"
inline std::string format_number(uint64_t n) {
    std::string s = std::to_string(n);
    int pos = static_cast<int>(s.length()) - 3;
    while (pos > 0) {
        s.insert(pos, ",");
        pos -= 3;
    }
    return s;
}

// 1500000.0 → "1.5M/s"
inline std::string format_rate(double rate) {
    std::ostringstream oss;
    if (rate >= 1'000'000.0) {
        oss << std::fixed << std::setprecision(1) << (rate / 1'000'000.0) << "M/s";
    } else if (rate >= 1'000.0) {
        oss << std::fixed << std::setprecision(1) << (rate / 1'000.0) << "K/s";
    } else {
        oss << std::fixed << std::setprecision(0) << rate << "/s";
    }
    return oss.str();
}

// 4523 ns → "4.5μs"
inline std::string format_latency_ns(uint64_t ns) {
    std::ostringstream oss;
    if (ns >= 1'000'000) {
        oss << std::fixed << std::setprecision(1) << (static_cast<double>(ns) / 1'000'000.0) << "ms";
    } else if (ns >= 1'000) {
        oss << std::fixed << std::setprecision(1) << (static_cast<double>(ns) / 1'000.0) << "us";
    } else {
        oss << ns << "ns";
    }
    return oss.str();
}

// 185.0500 → "185.0500" (4 decimal places)
inline std::string format_price(double price) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4) << price;
    return oss.str();
}

}  // namespace qf::utils
