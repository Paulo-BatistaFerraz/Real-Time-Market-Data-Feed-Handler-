#include "qf/data/storage/symbol_catalog.hpp"

namespace qf::data {

void SymbolCatalog::add_entry(const Symbol& symbol, DateKey date) {
    uint64_t key = symbol.as_key();
    catalog_[key].insert(date);
    symbol_map_[key] = symbol;
}

std::vector<DateKey> SymbolCatalog::get_available_dates(const Symbol& symbol) const {
    auto it = catalog_.find(symbol.as_key());
    if (it == catalog_.end()) {
        return {};
    }
    // std::set is already sorted ascending.
    return {it->second.begin(), it->second.end()};
}

std::vector<Symbol> SymbolCatalog::get_symbols() const {
    std::vector<Symbol> result;
    result.reserve(symbol_map_.size());
    for (const auto& [key, sym] : symbol_map_) {
        result.push_back(sym);
    }
    return result;
}

bool SymbolCatalog::has_data(const Symbol& symbol, DateKey date) const {
    auto it = catalog_.find(symbol.as_key());
    if (it == catalog_.end()) {
        return false;
    }
    return it->second.count(date) > 0;
}

size_t SymbolCatalog::entry_count() const {
    size_t total = 0;
    for (const auto& [key, dates] : catalog_) {
        total += dates.size();
    }
    return total;
}

}  // namespace qf::data
