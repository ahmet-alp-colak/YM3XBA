/**
 * @file TemporalStateEngine.hpp
 * @brief Temporal state tracking and adaptive noise floor estimation
 * @details Tracks signal likelihood history and computes robust noise floor
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#ifndef SIGNAL_DETECTION_TEMPORAL_STATE_ENGINE_HPP
#define SIGNAL_DETECTION_TEMPORAL_STATE_ENGINE_HPP

#include "DataStructures.hpp"
#include <unordered_map>
#include <cstdint>

namespace CognitiveRF {
namespace SignalDetection {

/**
 * @class TemporalStateEngine
 * @brief Manages temporal state for signal tracking
 * 
 * @details Features:
 * - Per-frequency temporal state tracking
 * - Likelihood history management
 * - Robust noise floor estimation using median + MAD
 * - Adaptive threshold computation
 * - State cleanup for old frequencies
 */
class TemporalStateEngine {
public:
    /**
     * @brief Constructor
     * @param history_size Maximum history size per frequency (default: 100)
     * @param cleanup_threshold_sec Age threshold for state cleanup (default: 300s)
     */
    explicit TemporalStateEngine(
        size_t history_size = 100,
        uint64_t cleanup_threshold_sec = 300
    );
    
    /**
     * @brief Update temporal state for a frequency
     * @param frequency_hz Frequency in Hz
     * @param likelihood Classification likelihood
     * @param timestamp_ms Current timestamp in milliseconds
     */
    void updateState(
        uint64_t frequency_hz,
        double likelihood,
        uint64_t timestamp_ms
    );
    
    /**
     * @brief Get temporal state for a frequency
     * @param frequency_hz Frequency in Hz
     * @return Pointer to state, or nullptr if not found
     */
    const TemporalState* getState(uint64_t frequency_hz) const;
    
    /**
     * @brief Get adaptive noise floor for a frequency
     * @param frequency_hz Frequency in Hz
     * @return Noise floor (0.0 if no history)
     */
    double getAdaptiveNoiseFloor(uint64_t frequency_hz) const;
    
    /**
     * @brief Get adaptive threshold (noise_floor + N*sigma)
     * @param frequency_hz Frequency in Hz
     * @param n_sigma Number of sigma for threshold (default: 3.0)
     * @return Adaptive threshold
     */
    double getAdaptiveThreshold(uint64_t frequency_hz, double n_sigma = 3.0) const;
    
    /**
     * @brief Apply temporal filter to classification result
     * @param frequency_hz Frequency in Hz
     * @param current_likelihood Current classification likelihood
     * @return Filtered likelihood
     */
    double applyTemporalFilter(uint64_t frequency_hz, double current_likelihood);
    
    /**
     * @brief Cleanup old states
     * @param current_timestamp_ms Current timestamp in milliseconds
     * @return Number of states removed
     */
    size_t cleanupOldStates(uint64_t current_timestamp_ms);
    
    /**
     * @brief Get total number of tracked frequencies
     * @return Number of frequencies
     */
    size_t getTrackedFrequencyCount() const { return states_.size(); }
    
    /**
     * @brief Clear all states
     */
    void clearAll();

private:
    std::unordered_map<uint64_t, TemporalState> states_;  ///< Frequency -> State map
    size_t history_size_;                                  ///< Max history per frequency
    uint64_t cleanup_threshold_sec_;                       ///< Cleanup age threshold
};

} // namespace SignalDetection
} // namespace CognitiveRF

#endif // SIGNAL_DETECTION_TEMPORAL_STATE_ENGINE_HPP
