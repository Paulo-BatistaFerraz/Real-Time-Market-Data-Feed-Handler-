#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include "qf/common/types.hpp"

namespace qf::data {

// A calendar date encoded as YYYYMMDD (e.g. 20260312).
using DateKey = uint32_t;

// SymbolCatalog — indexes available symbols and the dates for which data exists.
// Useful for discovering what data is available before requesting replays.
class SymbolCatalog {
public:
    SymbolCatalog() = default;

    // Register that data exists for a symbol on a given date.
    void add_entry(const Symbol& symbol, DateKey date);

    // Get all dates with data for a given symbol (sorted ascending).
    std::vector<DateKey> get_available_dates(const Symbol& symbol) const;

    // Get all symbols that have at least one date with data.
    std::vector<Symbol> get_symbols() const;

    // Check if any data is registered for a specific symbol + date.
    bool has_data(const Symbol& symbol, DateKey date) const;

    // Total number of unique symbols in the catalog.
    size_t symbol_count() const { return catalog_.size(); }

    // Total number of (symbol, date) entries.
    size_t entry_count() const;

    // Remove all entries.
    void clear() { catalog_.clear(); }

private:
    // Key = Symbol::as_key(), Value = set of DateKeys.
    std::unordered_map<uint64_t, std::set<DateKey>> catalog_;

    // Keep a mapping from as_key() back to the Symbol for get_symbols().
    std::unordered_map<uint64_t, Symbol> symbol_map_;
};

}  // namespace qf::data
