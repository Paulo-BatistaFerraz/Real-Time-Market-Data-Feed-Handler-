#pragma once

#include "qf/oms/oms_types.hpp"
#include "qf/strategy/strategy_types.hpp"
#include "qf/common/types.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace qf::oms {

// Result of pre-submission validation.
struct ValidationResult {
    bool        pass{false};
    std::string reason;
};

// Validates strategy Decisions before they enter the OMS pipeline.
// Checks quantity > 0, price > 0 for limit orders, and symbol is known.
class OrderValidator {
public:
    OrderValidator() = default;

    // Register a symbol as tradeable.
    void add_known_symbol(const Symbol& symbol);

    // Validate a Decision against all rules.
    ValidationResult validate(const strategy::Decision& decision) const;

    size_t known_symbol_count() const { return known_symbols_.size(); }

private:
    std::unordered_set<uint64_t> known_symbols_;
};

}  // namespace qf::oms
