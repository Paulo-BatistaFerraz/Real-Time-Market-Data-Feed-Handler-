#pragma once

#include <cstdint>
#include <unordered_map>
#include "qf/common/types.hpp"
#include "qf/oms/oms_types.hpp"

// Forward declarations to avoid header conflicts.
namespace qf::oms { class OrderManager; struct OMResult; }
namespace qf::strategy { class Portfolio; }

namespace qf::oms {

// Per-symbol fill statistics.
struct SymbolFillStats {
    uint64_t fill_count{0};
    uint64_t total_volume{0};       // sum of fill quantities
    double   total_notional{0.0};   // sum of (price * quantity) in double space

    double average_fill_price() const {
        return (total_volume > 0) ? (total_notional / total_volume) : 0.0;
    }
};

// FillManager — receives FillReports, forwards them to OrderManager and Portfolio,
// and tracks aggregate fill statistics.
class FillManager {
public:
    FillManager(OrderManager& order_mgr, strategy::Portfolio& portfolio);

    // Process a fill report: updates OrderManager, feeds Portfolio, tracks stats.
    OMResult on_fill(const FillReport& report);

    // --- Statistics ---
    uint64_t total_fills()  const { return total_fills_; }
    uint64_t total_volume() const { return total_volume_; }

    // Per-symbol stats keyed by Symbol::as_key().
    const SymbolFillStats& stats_for(const Symbol& symbol) const;

    // All per-symbol stats.
    const std::unordered_map<uint64_t, SymbolFillStats>& all_stats() const { return stats_; }

private:
    OrderManager&        order_mgr_;
    strategy::Portfolio& portfolio_;

    uint64_t total_fills_{0};
    uint64_t total_volume_{0};

    std::unordered_map<uint64_t, SymbolFillStats> stats_;

    static const SymbolFillStats empty_stats_;
};

}  // namespace qf::oms
