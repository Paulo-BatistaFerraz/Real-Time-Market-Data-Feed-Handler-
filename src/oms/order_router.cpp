#include "qf/oms/order_router.hpp"

namespace qf::oms {

// --- OrderRouter ---

void OrderRouter::set_default_connector(std::shared_ptr<ExchangeConnector> connector) {
    default_connector_ = std::move(connector);
}

void OrderRouter::map_symbol(const Symbol& symbol, std::shared_ptr<ExchangeConnector> connector) {
    symbol_connectors_[symbol.as_key()] = std::move(connector);
}

RoutingConfirmation OrderRouter::route(const OmsOrder& order) {
    RoutingConfirmation conf;
    conf.order_id = order.order_id;

    ExchangeConnector* connector = resolve(order.symbol);
    if (!connector) {
        conf.error = "No exchange connector available for symbol";
        return conf;
    }

    bool sent = connector->send_order(order);
    if (!sent) {
        conf.error = "Exchange connector rejected the order";
        return conf;
    }

    conf.routed = true;
    conf.venue  = connector->venue_name();
    return conf;
}

ExchangeConnector* OrderRouter::resolve(const Symbol& symbol) const {
    // Check symbol-specific mapping first
    auto it = symbol_connectors_.find(symbol.as_key());
    if (it != symbol_connectors_.end()) {
        return it->second.get();
    }
    // Fall back to default
    return default_connector_.get();
}

}  // namespace qf::oms
