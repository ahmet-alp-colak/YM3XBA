/**
 * @file DataStructures.cpp
 * @brief Implementation of TemporalState member functions
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#include "DataStructures.hpp"
#include <algorithm>
#include <cmath>

namespace CognitiveRF {
namespace SignalDetection {

void TemporalState::updateHistory(double likelihood) {
    likelihood_history.push_back(likelihood);
    if (likelihood_history.size() > 100) {  // LIKELIHOOD_HISTORY_SIZE
        likelihood_history.pop_front();
    }
}

void TemporalState::computeRobustNoiseFloor() {
    if (likelihood_history.size() < 20) return;
    
    std::vector<double> sorted_history(likelihood_history.begin(), likelihood_history.end());
    std::sort(sorted_history.begin(), sorted_history.end());
    
    // Median as robust noise floor estimate
    size_t mid = sorted_history.size() / 2;
    adaptive_noise_floor = sorted_history[mid];
    
    // MAD (Median Absolute Deviation) for robust sigma
    std::vector<double> abs_deviations;
    abs_deviations.reserve(sorted_history.size());
    for (double val : sorted_history) {
        abs_deviations.push_back(std::abs(val - adaptive_noise_floor));
    }
    std::sort(abs_deviations.begin(), abs_deviations.end());
    double mad = abs_deviations[abs_deviations.size() / 2];
    
    // Scale factor for normal distribution
    adaptive_noise_sigma = 1.4826 * mad;
    
    // Prevent collapse
    if (adaptive_noise_sigma < 0.1) {
        adaptive_noise_sigma = 0.1;
    }
}

} // namespace SignalDetection
} // namespace CognitiveRF
