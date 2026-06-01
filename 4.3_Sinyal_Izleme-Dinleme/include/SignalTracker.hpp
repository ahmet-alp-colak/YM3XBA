/**
 * @file SignalTracker.hpp
 * @brief Signal tracking and FHSS detection system
 * @details Maintains temporal state space for signal continuity tracking
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme
 * 
 * Reference: pipeline.txt Section [7] - Signal Tracking Layer
 * Reference: 4.3_SinyalDinleme-Izleme.txt Section 4.3.1
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef SIGNAL_TRACKER_HPP
#define SIGNAL_TRACKER_HPP

#include "DataStructures.hpp"
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace CognitiveRF {
namespace SignalMonitoring {

/**
 * @class SignalTracker
 * @brief Tracks signals over time and detects FHSS patterns
 * 
 * Features:
 * - Temporal state space management
 * - Frequency history tracking
 * - FHSS (Frequency Hopping Spread Spectrum) detection
 * - Occupancy ratio calculation
 * - Signal continuity analysis
 */
class SignalTracker {
public:
    /**
     * @struct TrackingConfig
     * @brief Configuration for signal tracking
     */
    struct TrackingConfig {
        uint32_t max_tracked_signals = 1000;
        uint32_t frequency_history_size = 50;
        double frequency_tolerance_hz = 1000.0;  // 1 kHz tolerance for matching
        double power_tolerance_db = 3.0;         // 3 dB tolerance
        uint64_t signal_timeout_us = 5000000;    // 5 seconds
        
        // FHSS detection parameters
        uint32_t min_hops_for_fhss = 5;
        float min_hop_rate_hz = 1.0f;            // Minimum 1 hop/sec
        float max_hop_rate_hz = 1000.0f;         // Maximum 1000 hops/sec
    };

    /**
     * @brief Constructor
     * @param config Tracking configuration
     */
    explicit SignalTracker(const TrackingConfig& config = TrackingConfig());
    
    /**
     * @brief Update tracking state with new detection
     * @param freq_hz Center frequency
     * @param power_dbm Signal power
     * @param bandwidth_hz Bandwidth
     * @param signal_class Temporal behavior class
     * @param modulation Detected modulation
     * @return Tracking ID (existing or newly assigned)
     */
    uint32_t updateTracking(
        double freq_hz,
        float power_dbm,
        double bandwidth_hz,
        SignalClass signal_class,
        ModulationType modulation
    );
    
    /**
     * @brief Get tracked signal state
     * @param tracking_id Signal tracking ID
     * @return Optional tracked signal state
     */
    std::optional<TrackedSignal> getTrackedSignal(uint32_t tracking_id) const;
    
    /**
     * @brief Check if signal is suspected FHSS
     * @param tracking_id Signal tracking ID
     * @return True if FHSS pattern detected
     */
    bool isFHSSSuspected(uint32_t tracking_id) const;
    
    /**
     * @brief Get all active tracked signals
     * @return Map of tracking_id -> TrackedSignal
     */
    std::unordered_map<uint32_t, TrackedSignal> getActiveSignals() const;
    
    /**
     * @brief Remove stale signals (timeout cleanup)
     * @return Number of signals removed
     */
    uint32_t removeStaleSignals();
    
    /**
     * @brief Get tracking statistics
     */
    struct Statistics {
        uint32_t total_tracked;
        uint32_t active_signals;
        uint32_t fhss_suspected_count;
        uint32_t burst_signals;
        uint32_t continuous_signals;
    };
    
    Statistics getStatistics() const;
    
    /**
     * @brief Reset all tracking state
     */
    void reset();

private:
    /**
     * @brief Find matching signal in tracking database
     * @param freq_hz Frequency to match
     * @param power_dbm Power to match
     * @param bandwidth_hz Bandwidth to match
     * @return Tracking ID if found, 0 otherwise
     */
    uint32_t findMatchingSignal(double freq_hz, float power_dbm, double bandwidth_hz) const;
    
    /**
     * @brief Detect FHSS pattern in frequency history
     * @param signal Tracked signal to analyze
     * @return True if FHSS pattern detected
     */
    bool detectFHSS(TrackedSignal& signal);
    
    /**
     * @brief Calculate occupancy ratio
     * @param signal Tracked signal
     * @return Occupancy ratio [0.0, 1.0]
     */
    float calculateOccupancy(const TrackedSignal& signal) const;
    
    /**
     * @brief Get current timestamp in microseconds
     */
    uint64_t getCurrentTimestampUs() const;

    // Configuration
    TrackingConfig config_;
    
    // Tracking database
    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, TrackedSignal> tracked_signals_;
    uint32_t next_tracking_id_;
    
    // Statistics
    uint32_t total_signals_tracked_;
};

} // namespace SignalMonitoring
} // namespace CognitiveRF

#endif // SIGNAL_TRACKER_HPP
