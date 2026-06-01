/**
 * @file SignalTracker.cpp
 * @brief Implementation of signal tracking and FHSS detection
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "SignalTracker.hpp"
#include <algorithm>
#include <cmath>

namespace CognitiveRF {
namespace SignalMonitoring {

SignalTracker::SignalTracker(const TrackingConfig& config)
    : config_(config)
    , next_tracking_id_(1)
    , total_signals_tracked_(0)
{
}

uint32_t SignalTracker::updateTracking(
    double freq_hz,
    float power_dbm,
    double bandwidth_hz,
    SignalClass signal_class,
    ModulationType modulation)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t now = getCurrentTimestampUs();
    
    // Try to find matching existing signal
    uint32_t tracking_id = findMatchingSignal(freq_hz, power_dbm, bandwidth_hz);
    
    if (tracking_id == 0) {
        // New signal - create tracking entry
        tracking_id = next_tracking_id_++;
        total_signals_tracked_++;
        
        TrackedSignal& signal = tracked_signals_[tracking_id];
        signal.tracking_id = tracking_id;
        signal.first_seen_us = now;
        signal.last_seen_us = now;
        signal.signal_class = signal_class;
        signal.modulation = modulation;
        signal.detection_count = 1;
        signal.avg_power_dbm = power_dbm;
        signal.avg_bandwidth_hz = bandwidth_hz;
        
        signal.frequency_history.push_back(freq_hz);
        signal.frequency_timestamps.push_back(now);
        signal.power_history.push_back(power_dbm);
    } else {
        // Update existing signal
        TrackedSignal& signal = tracked_signals_[tracking_id];
        signal.last_seen_us = now;
        signal.detection_count++;
        
        // Update frequency history
        signal.frequency_history.push_back(freq_hz);
        signal.frequency_timestamps.push_back(now);
        if (signal.frequency_history.size() > config_.frequency_history_size) {
            signal.frequency_history.pop_front();
            signal.frequency_timestamps.pop_front();
        }
        
        // Update power history
        signal.power_history.push_back(power_dbm);
        if (signal.power_history.size() > config_.frequency_history_size) {
            signal.power_history.pop_front();
        }
        
        // Update averages
        float sum_power = 0.0f;
        for (float p : signal.power_history) {
            sum_power += p;
        }
        signal.avg_power_dbm = sum_power / signal.power_history.size();
        
        // Update occupancy
        signal.occupancy_ratio = calculateOccupancy(signal);
        
        // Check for FHSS
        if (signal.signal_class == SignalClass::BURST) {
            detectFHSS(signal);
        }
    }
    
    return tracking_id;
}

std::optional<TrackedSignal> SignalTracker::getTrackedSignal(uint32_t tracking_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = tracked_signals_.find(tracking_id);
    if (it != tracked_signals_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool SignalTracker::isFHSSSuspected(uint32_t tracking_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = tracked_signals_.find(tracking_id);
    if (it != tracked_signals_.end()) {
        return it->second.fhss_suspected;
    }
    return false;
}

std::unordered_map<uint32_t, TrackedSignal> SignalTracker::getActiveSignals() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tracked_signals_;
}

uint32_t SignalTracker::removeStaleSignals() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t now = getCurrentTimestampUs();
    uint32_t removed = 0;
    
    auto it = tracked_signals_.begin();
    while (it != tracked_signals_.end()) {
        if ((now - it->second.last_seen_us) > config_.signal_timeout_us) {
            it = tracked_signals_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    
    return removed;
}

SignalTracker::Statistics SignalTracker::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Statistics stats;
    stats.total_tracked = total_signals_tracked_;
    stats.active_signals = static_cast<uint32_t>(tracked_signals_.size());
    stats.fhss_suspected_count = 0;
    stats.burst_signals = 0;
    stats.continuous_signals = 0;
    
    for (const auto& [id, signal] : tracked_signals_) {
        if (signal.fhss_suspected) stats.fhss_suspected_count++;
        if (signal.signal_class == SignalClass::BURST) stats.burst_signals++;
        if (signal.signal_class == SignalClass::CONTINUOUS) stats.continuous_signals++;
    }
    
    return stats;
}

void SignalTracker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    tracked_signals_.clear();
    next_tracking_id_ = 1;
    total_signals_tracked_ = 0;
}

// Private methods

uint32_t SignalTracker::findMatchingSignal(
    double freq_hz,
    float power_dbm,
    double bandwidth_hz) const
{
    for (const auto& [id, signal] : tracked_signals_) {
        // Check if frequency matches (within tolerance)
        if (!signal.frequency_history.empty()) {
            double last_freq = signal.frequency_history.back();
            double freq_diff = std::abs(freq_hz - last_freq);
            
            if (freq_diff <= config_.frequency_tolerance_hz) {
                // Check power tolerance
                float power_diff = std::abs(power_dbm - signal.avg_power_dbm);
                if (power_diff <= config_.power_tolerance_db) {
                    return id;
                }
            }
        }
    }
    
    return 0;  // No match found
}

bool SignalTracker::detectFHSS(TrackedSignal& signal) {
    // Need sufficient history
    if (signal.frequency_history.size() < config_.min_hops_for_fhss) {
        return false;
    }
    
    // Count frequency changes
    uint32_t hop_count = 0;
    for (size_t i = 1; i < signal.frequency_history.size(); i++) {
        double freq_diff = std::abs(signal.frequency_history[i] - signal.frequency_history[i-1]);
        if (freq_diff > config_.frequency_tolerance_hz) {
            hop_count++;
        }
    }
    
    signal.frequency_hop_count = hop_count;
    
    // Calculate hop rate
    if (signal.frequency_timestamps.size() >= 2) {
        uint64_t time_span = signal.frequency_timestamps.back() - signal.frequency_timestamps.front();
        if (time_span > 0) {
            signal.hop_rate_hz = (float)hop_count / (time_span / 1000000.0f);  // Convert to seconds
        }
    }
    
    // FHSS detection criteria:
    // 1. Multiple frequency hops
    // 2. Power and bandwidth relatively stable
    // 3. Hop rate within expected range
    bool power_stable = true;
    if (signal.power_history.size() >= 3) {
        float power_variance = 0.0f;
        for (float p : signal.power_history) {
            float diff = p - signal.avg_power_dbm;
            power_variance += diff * diff;
        }
        power_variance /= signal.power_history.size();
        power_stable = (std::sqrt(power_variance) < config_.power_tolerance_db);
    }
    
    signal.fhss_suspected = (hop_count >= config_.min_hops_for_fhss) &&
                            power_stable &&
                            (signal.hop_rate_hz >= config_.min_hop_rate_hz) &&
                            (signal.hop_rate_hz <= config_.max_hop_rate_hz);
    
    return signal.fhss_suspected;
}

float SignalTracker::calculateOccupancy(const TrackedSignal& signal) const {
    if (signal.first_seen_us == signal.last_seen_us) {
        return 1.0f;
    }
    
    uint64_t total_time = signal.last_seen_us - signal.first_seen_us;
    uint64_t active_time = signal.detection_count * 100000;  // Assume ~100ms per detection
    
    float occupancy = static_cast<float>(active_time) / static_cast<float>(total_time);
    return std::min(1.0f, occupancy);
}

uint64_t SignalTracker::getCurrentTimestampUs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

} // namespace SignalMonitoring
} // namespace CognitiveRF
