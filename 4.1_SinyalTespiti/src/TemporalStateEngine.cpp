/**
 * @file TemporalStateEngine.cpp
 * @brief Implementation of temporal state tracking
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#include "TemporalStateEngine.hpp"
#include <algorithm>

namespace CognitiveRF {
namespace SignalDetection {

TemporalStateEngine::TemporalStateEngine(
    size_t history_size,
    uint64_t cleanup_threshold_sec)
    : history_size_(history_size)
    , cleanup_threshold_sec_(cleanup_threshold_sec)
{
}

void TemporalStateEngine::updateState(
    uint64_t frequency_hz,
    double likelihood,
    uint64_t timestamp_ms)
{
    // Get or create state for this frequency
    TemporalState& state = states_[frequency_hz];
    
    // Update timestamp
    state.last_update_timestamp_ms = timestamp_ms;
    
    // Update likelihood history
    state.updateHistory(likelihood);
    
    // Compute robust noise floor if enough history
    if (state.likelihood_history.size() >= 20) {
        state.computeRobustNoiseFloor();
    }
    
    // Limit history size
    while (state.likelihood_history.size() > history_size_) {
        state.likelihood_history.pop_front();
    }
}

const TemporalState* TemporalStateEngine::getState(uint64_t frequency_hz) const {
    auto it = states_.find(frequency_hz);
    if (it != states_.end()) {
        return &(it->second);
    }
    return nullptr;
}

double TemporalStateEngine::getAdaptiveNoiseFloor(uint64_t frequency_hz) const {
    const TemporalState* state = getState(frequency_hz);
    if (state && state->likelihood_history.size() >= 20) {
        return state->adaptive_noise_floor;
    }
    return 0.0;
}

double TemporalStateEngine::getAdaptiveThreshold(
    uint64_t frequency_hz,
    double n_sigma) const
{
    const TemporalState* state = getState(frequency_hz);
    if (state && state->likelihood_history.size() >= 20) {
        return state->adaptive_noise_floor + n_sigma * state->adaptive_noise_sigma;
    }
    return 0.5;  // Default threshold
}

double TemporalStateEngine::applyTemporalFilter(
    uint64_t frequency_hz,
    double current_likelihood)
{
    const TemporalState* state = getState(frequency_hz);
    
    if (!state || state->likelihood_history.empty()) {
        return current_likelihood;  // No history, return as-is
    }
    
    // Exponential moving average with alpha = 0.3
    double alpha = 0.3;
    double prev_avg = 0.0;
    
    // Compute average of recent history
    size_t window = std::min(static_cast<size_t>(10), state->likelihood_history.size());
    for (size_t i = state->likelihood_history.size() - window; 
         i < state->likelihood_history.size(); i++) {
        prev_avg += state->likelihood_history[i];
    }
    prev_avg /= window;
    
    // Apply EMA filter
    double filtered = alpha * current_likelihood + (1.0 - alpha) * prev_avg;
    
    return filtered;
}

size_t TemporalStateEngine::cleanupOldStates(uint64_t current_timestamp_ms) {
    size_t removed_count = 0;
    uint64_t threshold_ms = cleanup_threshold_sec_ * 1000;
    
    // Remove states that haven't been updated recently
    auto it = states_.begin();
    while (it != states_.end()) {
        if (current_timestamp_ms - it->second.last_update_timestamp_ms > threshold_ms) {
            it = states_.erase(it);
            removed_count++;
        } else {
            ++it;
        }
    }
    
    return removed_count;
}

void TemporalStateEngine::clearAll() {
    states_.clear();
}

} // namespace SignalDetection
} // namespace CognitiveRF
