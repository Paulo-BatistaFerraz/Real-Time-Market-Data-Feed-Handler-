#pragma once

#include "qf/oms/order_router.hpp"
#include "qf/oms/sim_exchange/sim_exchange.hpp"

#include <string>

namespace qf::oms {

// Concrete ExchangeConnector that bridges OrderRouter to SimExchange.
// send_order() forwards to SimExchange::submit_order(), which generates
// fills asynchronously via the FillCallback wired at construction.
class SimExchangeConnector : public ExchangeConnector {
public:
    explicit SimExchangeConnector(SimExchange& exchange);

    const std::string& venue_name() const override;
    bool send_order(const OmsOrder& order) override;

    size_t orders_sent() const { return orders_sent_; }

private:
    SimExchange& exchange_;
    std::string  name_{"SimExchange"};
    size_t       orders_sent_{0};
};

}  // namespace qf::oms
