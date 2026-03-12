#include "qf/strategy/execution/order_sizer.hpp"

namespace qf::strategy {

OrderSizer::OrderSizer(SizingMethod method) : method_(method) {}

void OrderSizer::set_fixed_size(Quantity size) { fixed_size_ = size; }
void OrderSizer::set_fractional_pct(double pct) { fractional_pct_ = pct; }
void OrderSizer::set_max_position(Quantity max_pos) { max_position_ = max_pos; }

Quantity OrderSizer::size(double signal_strength,
                          const PortfolioView& portfolio,
                          const RiskBudget& risk_budget) const {
    const double abs_str = std::abs(signal_strength);
    if (abs_str < 1e-9) {
        return 0;
    }

    Quantity raw = 0;
    switch (method_) {
        case SizingMethod::Fixed:
            raw = size_fixed(abs_str);
            break;
        case SizingMethod::FixedFractional:
            raw = size_fractional(abs_str, portfolio);
            break;
        case SizingMethod::Kelly:
            raw = size_kelly(abs_str, portfolio, risk_budget);
            break;
    }

    return apply_limits(raw);
}

void OrderSizer::configure(const Config& config) {
    const auto method_str = config.get<std::string>("order_sizer.method", "fixed");
    if (method_str == "fixed_fractional") {
        method_ = SizingMethod::FixedFractional;
    } else if (method_str == "kelly") {
        method_ = SizingMethod::Kelly;
    } else {
        method_ = SizingMethod::Fixed;
    }

    fixed_size_      = config.get<uint32_t>("order_sizer.fixed_size", 100);
    fractional_pct_  = config.get<double>("order_sizer.fractional_pct", 0.02);
    max_position_    = config.get<uint32_t>("order_sizer.max_position", 10000);
}

// --- Private helpers -------------------------------------------------------

Quantity OrderSizer::size_fixed(double abs_strength) const {
    // Scale fixed size by signal strength.
    return static_cast<Quantity>(std::round(fixed_size_ * abs_strength));
}

Quantity OrderSizer::size_fractional(double abs_strength,
                                     const PortfolioView& portfolio) const {
    if (portfolio.cash <= 0.0) {
        return 0;
    }
    // Fraction of cash scaled by signal strength.
    const double notional = portfolio.cash * fractional_pct_ * abs_strength;
    // Convert notional to quantity (assume unit price = $1 for simplicity;
    // caller should divide by price if needed).
    return static_cast<Quantity>(std::max(0.0, std::floor(notional)));
}

Quantity OrderSizer::size_kelly(double abs_strength,
                                const PortfolioView& portfolio,
                                const RiskBudget& budget) const {
    if (portfolio.cash <= 0.0) {
        return 0;
    }

    const double p = budget.win_rate;
    const double q = 1.0 - p;
    const double b = budget.win_loss_ratio;

    // Kelly fraction: f* = p - q/b  (simplified Kelly formula)
    // When b == 0 or formula yields <= 0, no bet.
    if (b < 1e-9) {
        return 0;
    }
    double kelly_f = p - (q / b);
    if (kelly_f <= 0.0) {
        return 0;
    }

    // Cap Kelly fraction by max portfolio risk.
    kelly_f = std::min(kelly_f, budget.max_portfolio_risk);

    // Scale by signal strength.
    const double notional = portfolio.cash * kelly_f * abs_strength;
    return static_cast<Quantity>(std::max(0.0, std::floor(notional)));
}

Quantity OrderSizer::apply_limits(Quantity raw_qty) const {
    return std::min(raw_qty, max_position_);
}

}  // namespace qf::strategy
