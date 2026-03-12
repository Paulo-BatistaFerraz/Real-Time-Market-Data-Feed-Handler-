#pragma once

#include <cstdint>
#include "qf/signals/signal_types.hpp"

namespace qf::signals {

// Market regime classifications.
enum class MarketRegime : uint8_t {
    Trending  = 0,   // strong directional movement with positive autocorrelation
    Ranging   = 1,   // low directional strength, mean-reverting behavior
    Volatile  = 2    // high volatility, unstable price action
};

// Result of regime detection: the classified regime plus a confidence score.
struct RegimeResult {
    MarketRegime regime;
    double       confidence;  // [0, 1] — how clearly the market fits this regime
};

// RegimeDetector — classifies market conditions into Trending, Ranging, or Volatile
// using a combination of directional strength (ADX-like), volatility level, and
// autocorrelation of returns.
//
// Expected features in the FeatureVector:
//   "directional_strength"  — ADX-like measure of trend strength, [0, 1]
//   "volatility"            — normalized volatility level, [0, 1]
//   "autocorrelation"       — serial correlation of returns, [-1, 1]
//
// Hysteresis prevents regime flickering on borderline values: the current regime
// receives a configurable bonus when scoring, making it "sticky".
//
// Usage:
//   RegimeDetector detector;
//   RegimeResult result = detector.detect(features);
//   if (detector.is_regime(MarketRegime::Trending)) { /* enable trend strategy */ }
//
class RegimeDetector {
public:
    // Construct with default thresholds.
    // trend_threshold: minimum trending score to classify as Trending (default 0.5)
    // volatile_threshold: minimum volatile score to classify as Volatile (default 0.6)
    // hysteresis: bonus for the current regime to prevent flickering (default 0.1)
    explicit RegimeDetector(double trend_threshold = 0.5,
                            double volatile_threshold = 0.6,
                            double hysteresis = 0.1);

    // Detect the current market regime from features.
    // Updates internal state and returns the result.
    RegimeResult detect(const FeatureVector& features);

    // Query the last detected regime.
    MarketRegime current_regime() const { return current_regime_; }

    // Query the confidence of the last detection.
    double confidence() const { return current_confidence_; }

    // Check if the current regime matches the given regime.
    // Strategies use this to enable/disable themselves.
    bool is_regime(MarketRegime regime) const { return current_regime_ == regime; }

    // Configuration setters.
    void set_trend_threshold(double threshold);
    void set_volatile_threshold(double threshold);
    void set_hysteresis(double hysteresis);

    // Configuration getters.
    double trend_threshold() const { return trend_threshold_; }
    double volatile_threshold() const { return volatile_threshold_; }
    double hysteresis() const { return hysteresis_; }

    // Reset state to initial (Ranging with 0 confidence).
    void reset();

private:
    double trend_threshold_;
    double volatile_threshold_;
    double hysteresis_;

    MarketRegime current_regime_;
    double current_confidence_;
    bool   initialized_;

    // Compute raw score for each regime from features.
    double compute_trending_score(const FeatureVector& features) const;
    double compute_volatile_score(const FeatureVector& features) const;
    double compute_ranging_score(double trending_score, double volatile_score) const;

    // Clamp value to [lo, hi].
    static double clamp(double value, double lo, double hi);
};

}  // namespace qf::signals
