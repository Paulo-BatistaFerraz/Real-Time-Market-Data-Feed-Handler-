#include "qf/oms/sim_exchange/sim_exchange_connector.hpp"

namespace qf::oms {

SimExchangeConnector::SimExchangeConnector(SimExchange& exchange)
    : exchange_(exchange) {}

const std::string& SimExchangeConnector::venue_name() const {
    return name_;
}

bool SimExchangeConnector::send_order(const OmsOrder& order) {
    exchange_.submit_order(order);
    ++orders_sent_;
    return true;
}

}  // namespace qf::oms
