#pragma once

#include "qf/oms/oms_types.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace qf::oms {

// Confirmation returned after an order is routed to a venue.
struct RoutingConfirmation {
    bool        routed{false};
    OrderId     order_id{0};
    std::string venue;
    std::string error;
};

// Abstract interface for exchange connectivity.
class ExchangeConnector {
public:
    virtual ~ExchangeConnector() = default;

    // Name of the venue (e.g. "SimExchange").
    virtual const std::string& venue_name() const = 0;

    // Send an order to the venue. Returns true on success.
    virtual bool send_order(const OmsOrder& order) = 0;
};

// Routes validated OmsOrders to the correct ExchangeConnector.
// Currently defaults to a single destination (SimExchange),
// but the interface supports multiple venues keyed by symbol.
class OrderRouter {
public:
    OrderRouter() = default;

    // Set the default connector used when no symbol-specific mapping exists.
    void set_default_connector(std::shared_ptr<ExchangeConnector> connector);

    // Map a specific symbol to a specific connector.
    void map_symbol(const Symbol& symbol, std::shared_ptr<ExchangeConnector> connector);

    // Route an order to the appropriate venue and return a confirmation.
    RoutingConfirmation route(const OmsOrder& order);

private:
    std::shared_ptr<ExchangeConnector> default_connector_;
    std::unordered_map<uint64_t, std::shared_ptr<ExchangeConnector>> symbol_connectors_;

    // Resolve the connector for a given symbol.
    ExchangeConnector* resolve(const Symbol& symbol) const;
};

}  // namespace qf::oms
