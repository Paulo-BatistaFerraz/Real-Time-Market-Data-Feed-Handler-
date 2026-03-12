#include "qf/signals/composite/regime_detector.hpp"
#include <algorithm>
#include <cmath>

namespace qf::signals {

RegimeDetector::RegimeDetector(double trend_threshold,
                               double volatile_threshold,
                               double hysteresis)
    : trend_threshold_(trend_threshold)
    , volatile_threshold_(volatile_threshold)
    , hysteresis_(hysteresis)
    , current_regime_(MarketRegime::Ranging)
    , current_confidence_(0.0)
    , initialized_(false) {}

RegimeResult RegimeDetector::detect(const FeatureVector& features) {
    // Compute raw scores for each regime.
    double trend_score = compute_trending_score(features);
    double vol_score   = compute_volatile_score(features);
    double range_score = compute_ranging_score(trend_score, vol_score);

    // Apply hysteresis: the current regime gets a bonus to prevent flickering.
    double adj_trend = trend_score;
    double adj_vol   = vol_score;
    double adj_range = range_score;

    if (initialized_) {
        switch (current_regime_) {
            case MarketRegime::Trending: adj_trend += hysteresis_; break;
            case MarketRegime::Volatile: adj_vol   += hysteresis_; break;
            case MarketRegime::Ranging:  adj_range += hysteresis_; break;
        }
    }

    // Select the regime with the highest adjusted score.
    MarketRegime best = MarketRegime::Ranging;
    double best_score = adj_range;

    if (adj_trend > best_score) {
        best = MarketRegime::Trending;
        best_score = adj_trend;
    }
    if (adj_vol > best_score) {
        best = MarketRegime::Volatile;
        best_score = adj_vol;
    }

    // Confidence: based on the margin between the winning score and the runner-up,
    // normalized so that a large margin gives high confidence.
    // Use raw (unadjusted) scores for confidence to avoid hysteresis inflation.
    double scores[3] = { trend_score, range_score, vol_score };
    std::sort(scores, scores + 3, std::greater<double>());
    double margin = scores[0] - scores[1];  // gap between first and second
    double confidence = clamp(margin / 0.5, 0.0, 1.0);  // normalize: 0.5 gap → 100%

    // If the best score is very low, confidence should reflect that.
    double raw_best = std::max({trend_score, vol_score, range_score});
    if (raw_best < 0.2) {
        confidence *= raw_best / 0.2;
    }

    current_regime_     = best;
    current_confidence_ = confidence;
    initialized_        = true;

    return { current_regime_, current_confidence_ };
}

void RegimeDetector::set_trend_threshold(double threshold) {
    trend_threshold_ = clamp(threshold, 0.0, 1.0);
}

void RegimeDetector::set_volatile_threshold(double threshold) {
    volatile_threshold_ = clamp(threshold, 0.0, 1.0);
}

void RegimeDetector::set_hysteresis(double hysteresis) {
    hysteresis_ = clamp(hysteresis, 0.0, 0.5);
}

void RegimeDetector::reset() {
    current_regime_     = MarketRegime::Ranging;
    current_confidence_ = 0.0;
    initialized_        = false;
}

double RegimeDetector::compute_trending_score(const FeatureVector& features) const {
    // Directional strength (ADX-like): high values indicate strong trend.
    double dir_strength = clamp(features.get("directional_strength", 0.0), 0.0, 1.0);

    // Autocorrelation of returns: positive values reinforce trending detection.
    // Negative autocorrelation suggests mean-reversion (not trending).
    double autocorr = clamp(features.get("autocorrelation", 0.0), -1.0, 1.0);

    // Trending score: weighted combination.
    // Directional strength is the primary signal (70%), positive autocorrelation
    // provides confirmation (30%). Negative autocorrelation penalizes trending.
    double autocorr_contrib = (autocorr + 1.0) / 2.0;  // map [-1,1] → [0,1]
    double score = 0.7 * dir_strength + 0.3 * autocorr_contrib;

    return clamp(score, 0.0, 1.0);
}

double RegimeDetector::compute_volatile_score(const FeatureVector& features) const {
    // Volatility level: high values indicate volatile/unstable market.
    double volatility = clamp(features.get("volatility", 0.0), 0.0, 1.0);

    // Negative autocorrelation can amplify volatility detection (whipsaw behavior).
    double autocorr = clamp(features.get("autocorrelation", 0.0), -1.0, 1.0);
    double whipsaw_boost = (autocorr < 0.0) ? (-autocorr * 0.2) : 0.0;

    double score = volatility + whipsaw_boost;
    return clamp(score, 0.0, 1.0);
}

double RegimeDetector::compute_ranging_score(double trending_score, double volatile_score) const {
    // Ranging is the "absence" regime: low trend strength and low volatility.
    // Score inversely related to the other two regimes.
    double score = 1.0 - std::max(trending_score, volatile_score);
    return clamp(score, 0.0, 1.0);
}

double RegimeDetector::clamp(double value, double lo, double hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

}  // namespace qf::signals
