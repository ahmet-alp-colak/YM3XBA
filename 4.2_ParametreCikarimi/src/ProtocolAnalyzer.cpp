/**
 * @file ProtocolAnalyzer.cpp
 * @brief Implementation of protocol analyzer decision tree
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "ProtocolAnalyzer.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace CognitiveRF {

ProtocolAnalyzer::ProtocolAnalyzer(
    size_t fhss_detection_window,
    double fhss_hop_threshold
)
    : fhss_detection_window_(fhss_detection_window)
    , fhss_hop_threshold_(fhss_hop_threshold)
{
}

ProtocolAnalysisResult ProtocolAnalyzer::analyze(const ProtocolAnalysisInput& input) {
    ProtocolAnalysisResult result;
    
    // Step 1: Infer multiplexing method from signal class
    result.multiplexing = inferMultiplexing(input.signal_class);
    
    // Step 2: Match protocol using decision tree
    auto [protocol, confidence] = matchProtocol(
        input.modulation,
        input.signal_class,
        input.bandwidth_hz,
        result.multiplexing
    );
    result.protocol = protocol;
    result.confidence = confidence;
    
    // Step 3: ECCM detection
    result.fhss_suspected = detectFHSS();
    result.jamming_suspected = false;  // Placeholder for jamming detection logic
    
    return result;
}

void ProtocolAnalyzer::updateFrequencyTracking(double frequency, float power) {
    frequency_history_.push_back(frequency);
    power_history_.push_back(power);
    
    // Maintain window size
    if (frequency_history_.size() > fhss_detection_window_) {
        frequency_history_.pop_front();
        power_history_.pop_front();
    }
}

bool ProtocolAnalyzer::isFHSSDetected() const {
    return detectFHSS();
}

MultiplexingType ProtocolAnalyzer::inferMultiplexing(SignalClass signal_class) const {
    switch (signal_class) {
        case SignalClass::BURST:
            // Burst signals typically use TDMA or CSMA
            // Default to TDMA for tactical communications
            return MultiplexingType::TDMA;
            
        case SignalClass::CONTINUOUS:
            // Continuous signals typically use FDMA
            return MultiplexingType::FDMA;
            
        case SignalClass::NOISE:
        default:
            return MultiplexingType::UNKNOWN;
    }
}

std::pair<ProtocolType, float> ProtocolAnalyzer::matchProtocol(
    ModulationType modulation,
    SignalClass signal_class,
    double bandwidth_hz,
    MultiplexingType multiplexing
) const {
    // Decision tree for protocol matching
    // Returns (ProtocolType, confidence)
    
    // DMR: 4FSK + Burst (TDMA) + BW=12.5kHz
    if (modulation == ModulationType::FSK4 &&
        signal_class == SignalClass::BURST &&
        multiplexing == MultiplexingType::TDMA &&
        bandwidthMatches(bandwidth_hz, 12.5e3)) {
        return {ProtocolType::DMR, 0.95f};
    }
    
    // GSM: GMSK + Burst (TDMA) + BW=200kHz
    if (modulation == ModulationType::GMSK &&
        signal_class == SignalClass::BURST &&
        multiplexing == MultiplexingType::TDMA &&
        bandwidthMatches(bandwidth_hz, 200e3)) {
        return {ProtocolType::GSM, 0.90f};
    }
    
    // TETRA: QPSK + Continuous (FDMA) + BW=25kHz
    if (modulation == ModulationType::QPSK &&
        signal_class == SignalClass::CONTINUOUS &&
        multiplexing == MultiplexingType::FDMA &&
        bandwidthMatches(bandwidth_hz, 25e3)) {
        return {ProtocolType::TETRA, 0.92f};
    }
    
    // P25 Phase 1: QPSK + TDMA + BW=12.5kHz
    if (modulation == ModulationType::QPSK &&
        signal_class == SignalClass::BURST &&
        multiplexing == MultiplexingType::TDMA &&
        bandwidthMatches(bandwidth_hz, 12.5e3)) {
        return {ProtocolType::P25, 0.88f};
    }
    
    // DPMR: 4FSK + TDMA + BW=6.25kHz
    if (modulation == ModulationType::FSK4 &&
        signal_class == SignalClass::BURST &&
        multiplexing == MultiplexingType::TDMA &&
        bandwidthMatches(bandwidth_hz, 6.25e3)) {
        return {ProtocolType::DPMR, 0.85f};
    }
    
    // NXDN: 4FSK + TDMA + BW=12.5kHz (similar to DMR, lower confidence)
    if (modulation == ModulationType::FSK4 &&
        signal_class == SignalClass::BURST &&
        multiplexing == MultiplexingType::TDMA &&
        bandwidthMatches(bandwidth_hz, 12.5e3, 0.15)) {
        return {ProtocolType::NXDN, 0.75f};
    }
    
    // LTE: OFDM + Continuous + Wide bandwidth
    if (modulation == ModulationType::OFDM &&
        signal_class == SignalClass::CONTINUOUS &&
        bandwidth_hz > 1e6) {  // > 1 MHz
        return {ProtocolType::LTE, 0.80f};
    }
    
    // WiFi: OFDM + Burst/Continuous + 20/40/80 MHz
    if (modulation == ModulationType::OFDM &&
        (bandwidthMatches(bandwidth_hz, 20e6) ||
         bandwidthMatches(bandwidth_hz, 40e6) ||
         bandwidthMatches(bandwidth_hz, 80e6))) {
        return {ProtocolType::WIFI, 0.85f};
    }
    
    // Bluetooth: GFSK + Burst + BW~1MHz
    if (modulation == ModulationType::GFSK &&
        signal_class == SignalClass::BURST &&
        bandwidthMatches(bandwidth_hz, 1e6, 0.3)) {
        return {ProtocolType::BLUETOOTH, 0.75f};
    }
    
    // ZigBee: OQPSK + Burst + BW~2MHz
    if (modulation == ModulationType::OQPSK &&
        signal_class == SignalClass::BURST &&
        bandwidthMatches(bandwidth_hz, 2e6, 0.3)) {
        return {ProtocolType::ZIGBEE, 0.70f};
    }
    
    // No match found
    return {ProtocolType::UNKNOWN, 0.0f};
}

bool ProtocolAnalyzer::detectFHSS() const {
    // Need sufficient history
    if (frequency_history_.size() < 3 || power_history_.size() < 3) {
        return false;
    }
    
    // Criterion 1: Power must be stable (low variance)
    float power_variance = computePowerVariance();
    const float POWER_STABILITY_THRESHOLD = 5.0f;  // dB^2
    
    if (power_variance > POWER_STABILITY_THRESHOLD) {
        return false;  // Power not stable
    }
    
    // Criterion 2: Frequency must change significantly
    size_t hop_count = countFrequencyHops();
    const size_t MIN_HOPS_FOR_FHSS = 2;
    
    if (hop_count < MIN_HOPS_FOR_FHSS) {
        return false;  // Not enough frequency changes
    }
    
    // Both criteria met: FHSS suspected
    return true;
}

bool ProtocolAnalyzer::bandwidthMatches(
    double actual_bw,
    double expected_bw,
    double tolerance_percent
) const {
    double lower_bound = expected_bw * (1.0 - tolerance_percent);
    double upper_bound = expected_bw * (1.0 + tolerance_percent);
    return (actual_bw >= lower_bound) && (actual_bw <= upper_bound);
}

float ProtocolAnalyzer::computePowerVariance() const {
    if (power_history_.empty()) {
        return 0.0f;
    }
    
    // Compute mean
    float mean = std::accumulate(
        power_history_.begin(),
        power_history_.end(),
        0.0f
    ) / static_cast<float>(power_history_.size());
    
    // Compute variance
    float variance = 0.0f;
    for (float power : power_history_) {
        float diff = power - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(power_history_.size());
    
    return variance;
}

size_t ProtocolAnalyzer::countFrequencyHops() const {
    if (frequency_history_.size() < 2) {
        return 0;
    }
    
    size_t hop_count = 0;
    for (size_t i = 1; i < frequency_history_.size(); ++i) {
        double freq_change = std::abs(frequency_history_[i] - frequency_history_[i-1]);
        if (freq_change > fhss_hop_threshold_) {
            hop_count++;
        }
    }
    
    return hop_count;
}

} // namespace CognitiveRF
