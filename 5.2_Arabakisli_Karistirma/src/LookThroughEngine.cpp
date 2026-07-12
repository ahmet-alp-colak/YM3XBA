/**
 * @file LookThroughEngine.cpp
 * @brief Implementation of Look-Through Jamming engine with energy detection
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.2 Arabakışlı Karıştırma (Look-Through Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-07-12
 */

#include "LookThroughEngine.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <numeric>

namespace CognitiveRF {
namespace LookThroughJamming {

LookThroughEngine::LookThroughEngine(const PipelineConfig& config)
    : config_(config)
    , adaptive_noise_floor_dbm_(config.tdd.noise_floor_dbm)
    , phase_accumulator_(0.0)
    , tx_sample_counter_(0)
    , rx_sample_counter_(0)
    , rng_(config.signal.noise_seed == 0 ? 
           std::random_device{}() : config.signal.noise_seed)
    , gaussian_dist_(0.0f, 1.0f)
{
    stats_.total_sensing_cycles = 0;
    stats_.total_detections = 0;
    stats_.total_false_alarms = 0;
    stats_.total_rx_samples = 0;
    stats_.total_tx_samples = 0;
    stats_.current_noise_floor_dbm = adaptive_noise_floor_dbm_;
    stats_.average_detection_power_dbm = -100.0f;
    stats_.detection_rate = 0.0;
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Engine initialized with TDD timing: "
                  << "RX=" << config_.tdd.sensing_window_ms << "ms, "
                  << "TX=" << config_.tdd.jamming_pulse_ms << "ms" << std::endl;
    }
}

LookThroughEngine::~LookThroughEngine() {
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Engine destroyed. "
                  << "Total cycles: " << stats_.total_sensing_cycles
                  << ", Detections: " << stats_.total_detections
                  << ", Detection rate: " << (stats_.detection_rate * 100.0) << "%" 
                  << std::endl;
    }
}

bool LookThroughEngine::initialize(const TargetIntelligence& target) {
    if (!target.isValid()) {
        std::cerr << config_.log_prefix << " Invalid target parameters" << std::endl;
        return false;
    }
    
    current_target_ = target;
    phase_accumulator_ = 0.0;
    tx_sample_counter_ = 0;
    rx_sample_counter_ = 0;
    
    // Initialize adaptive noise floor from target RSSI if available
    if (target.rssi_dbm > Constants::MIN_DETECTABLE_SIGNAL_DBM) {
        adaptive_noise_floor_dbm_ = target.rssi_dbm - 20.0f; // Estimate 20 dB below signal
    }
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Initialized for target ID=" << target.target_id
                  << " freq=" << (target.center_freq_hz / 1e6) << " MHz"
                  << " bw=" << (target.bandwidth_hz / 1e3) << " kHz"
                  << " adaptive_noise_floor=" << adaptive_noise_floor_dbm_ << " dBm"
                  << std::endl;
    }
    
    return true;
}

bool LookThroughEngine::performEnergyDetection(const IQBuffer& iq_samples, 
                                                 SensingResult& result) {
    if (iq_samples.empty()) {
        std::cerr << config_.log_prefix << " Empty IQ buffer for sensing" << std::endl;
        return false;
    }
    
    stats_.total_sensing_cycles++;
    stats_.total_rx_samples += iq_samples.size();
    rx_sample_counter_ += iq_samples.size();
    
    // Compute average power
    const float avg_power_linear = computeAveragePower(iq_samples);
    const float avg_power_dbm = powerToDbm(avg_power_linear);
    
    // Update adaptive noise floor if enabled
    if (config_.tdd.use_adaptive_threshold) {
        updateNoiseFloor(avg_power_dbm);
    }
    
    // Decision: Is target active?
    const float threshold_dbm = getDetectionThreshold();
    const bool detected = (avg_power_dbm > threshold_dbm);
    
    // Fill result structure
    result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    result.center_freq_hz = current_target_.center_freq_hz;
    result.measured_rssi_dbm = avg_power_dbm;
    result.measured_power_db = avg_power_dbm - adaptive_noise_floor_dbm_;
    result.noise_floor_dbm = adaptive_noise_floor_dbm_;
    result.target_detected = detected;
    result.activity = detected ? TargetActivityState::ACTIVE : TargetActivityState::INACTIVE;
    result.num_samples_processed = static_cast<uint32_t>(iq_samples.size());
    result.sensing_duration_ms = config_.tdd.sensing_window_ms;
    
    // Update statistics
    if (detected) {
        stats_.total_detections++;
        stats_.average_detection_power_dbm = 
            (stats_.average_detection_power_dbm * (stats_.total_detections - 1) + avg_power_dbm) 
            / stats_.total_detections;
    }
    
    stats_.detection_rate = static_cast<double>(stats_.total_detections) / 
                            static_cast<double>(stats_.total_sensing_cycles);
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Sensing: Power=" << avg_power_dbm << " dBm, "
                  << "Threshold=" << threshold_dbm << " dBm, "
                  << "Detected=" << (detected ? "YES" : "NO") << std::endl;
    }
    
    return true;
}

size_t LookThroughEngine::generateJammingSamples(IQBuffer& output, size_t num_samples) {
    if (num_samples == 0) {
        return 0;
    }
    
    output.resize(num_samples);
    
    switch (config_.signal.type) {
        case JammingType::CONTINUOUS_WAVE:
            generateContinuousWave(output, num_samples);
            break;
        case JammingType::GAUSSIAN_NOISE:
            generateGaussianNoise(output, num_samples);
            break;
        case JammingType::MODULATED_NOISE:
            generateModulatedNoise(output, num_samples);
            break;
        default:
            std::cerr << config_.log_prefix << " Unknown jamming type" << std::endl;
            return 0;
    }
    
    stats_.total_tx_samples += num_samples;
    tx_sample_counter_ += num_samples;
    
    return num_samples;
}

void LookThroughEngine::updateNoiseFloor(float measured_power_dbm) {
    // Exponential Moving Average (EMA) for adaptive noise floor tracking
    // Only update if measured power is below current estimate (to avoid tracking signals)
    if (measured_power_dbm < adaptive_noise_floor_dbm_ + 5.0f) {
        adaptive_noise_floor_dbm_ = NOISE_FLOOR_ALPHA * measured_power_dbm + 
                                     (1.0f - NOISE_FLOOR_ALPHA) * adaptive_noise_floor_dbm_;
    }
    
    stats_.current_noise_floor_dbm = adaptive_noise_floor_dbm_;
}

float LookThroughEngine::getDetectionThreshold() const {
    return adaptive_noise_floor_dbm_ + config_.tdd.detection_threshold_db;
}

void LookThroughEngine::updateSignalConfig(const SignalConfig& new_config) {
    if (!new_config.validate()) {
        std::cerr << config_.log_prefix << " Invalid signal configuration" << std::endl;
        return;
    }
    config_.signal = new_config;
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Signal configuration updated" << std::endl;
    }
}

void LookThroughEngine::updateTDDTiming(const TDDTimingConfig& new_tdd) {
    if (!new_tdd.validate()) {
        std::cerr << config_.log_prefix << " Invalid TDD timing configuration" << std::endl;
        return;
    }
    config_.tdd = new_tdd;
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " TDD timing updated: "
                  << "RX=" << new_tdd.sensing_window_ms << "ms, "
                  << "TX=" << new_tdd.jamming_pulse_ms << "ms" << std::endl;
    }
}

LookThroughEngine::Statistics LookThroughEngine::getStatistics() const {
    return stats_;
}

void LookThroughEngine::reset() {
    phase_accumulator_ = 0.0;
    tx_sample_counter_ = 0;
    rx_sample_counter_ = 0;
    adaptive_noise_floor_dbm_ = config_.tdd.noise_floor_dbm;
    
    stats_.total_sensing_cycles = 0;
    stats_.total_detections = 0;
    stats_.total_false_alarms = 0;
    stats_.total_rx_samples = 0;
    stats_.total_tx_samples = 0;
    stats_.current_noise_floor_dbm = adaptive_noise_floor_dbm_;
    stats_.average_detection_power_dbm = -100.0f;
    stats_.detection_rate = 0.0;
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Engine reset" << std::endl;
    }
}

// ============================================================================
// PRIVATE METHODS - JAMMING SIGNAL GENERATION
// ============================================================================

void LookThroughEngine::generateContinuousWave(IQBuffer& output, size_t num_samples) {
    const double sample_rate = config_.hardware.sample_rate_hz;
    const double freq_offset = config_.signal.cw_offset_hz;
    const float amplitude = config_.signal.cw_amplitude;
    const double omega = Constants::TWO_PI * freq_offset / sample_rate;
    
    for (size_t i = 0; i < num_samples; ++i) {
        const double phase = omega * static_cast<double>(i) + phase_accumulator_;
        const float i_sample = amplitude * std::cos(phase);
        const float q_sample = amplitude * std::sin(phase);
        output[i] = IQSample(i_sample, q_sample);
    }
    
    phase_accumulator_ += omega * static_cast<double>(num_samples);
    phase_accumulator_ = std::fmod(phase_accumulator_, Constants::TWO_PI);
}

void LookThroughEngine::generateGaussianNoise(IQBuffer& output, size_t num_samples) {
    const float amplitude = config_.signal.noise_amplitude;
    
    for (size_t i = 0; i < num_samples; ++i) {
        const float i_noise = gaussian_dist_(rng_) * amplitude;
        const float q_noise = gaussian_dist_(rng_) * amplitude;
        output[i] = IQSample(i_noise, q_noise);
    }
}

void LookThroughEngine::generateModulatedNoise(IQBuffer& output, size_t num_samples) {
    const double sample_rate = config_.hardware.sample_rate_hz;
    const float noise_amp = config_.signal.noise_amplitude;
    
    for (size_t i = 0; i < num_samples; ++i) {
        const double time_s = static_cast<double>(tx_sample_counter_ + i) / sample_rate;
        const float envelope = computeInstantaneousAmplitude(time_s);
        const float i_noise = gaussian_dist_(rng_) * noise_amp * envelope;
        const float q_noise = gaussian_dist_(rng_) * noise_amp * envelope;
        output[i] = IQSample(i_noise, q_noise);
    }
}

// ============================================================================
// PRIVATE METHODS - ENERGY DETECTION HELPERS
// ============================================================================

float LookThroughEngine::computeAveragePower(const IQBuffer& samples) const {
    if (samples.empty()) return 0.0f;
    
    // Compute instantaneous power: P[n] = |I[n]|^2 + |Q[n]|^2
    float power_sum = 0.0f;
    for (const auto& sample : samples) {
        const float i = sample.real();
        const float q = sample.imag();
        power_sum += (i * i + q * q);
    }
    
    // Average power
    return power_sum / static_cast<float>(samples.size());
}

float LookThroughEngine::powerToDbm(float power_linear) const {
    // Convert linear power to dBm
    // Assuming power is normalized: P_dBm = 10*log10(P_linear) + calibration_offset
    // For HackRF, typical calibration offset is around 0 dBm for full scale
    constexpr float CALIBRATION_OFFSET_DBM = 0.0f;
    
    if (power_linear <= 0.0f) {
        return Constants::MIN_DETECTABLE_SIGNAL_DBM;
    }
    
    const float power_dbm = 10.0f * std::log10(power_linear) + CALIBRATION_OFFSET_DBM;
    return power_dbm;
}

void LookThroughEngine::normalizeBuffer(IQBuffer& buffer) const {
    if (buffer.empty()) return;
    
    float max_mag = 0.0f;
    for (const auto& sample : buffer) {
        const float mag = std::abs(sample);
        if (mag > max_mag) max_mag = mag;
    }
    
    if (max_mag > 1.0f) {
        const float scale = 1.0f / max_mag;
        for (auto& sample : buffer) {
            sample *= scale;
        }
    }
}

float LookThroughEngine::computeInstantaneousAmplitude(double time_s) const {
    const double mod_rate = config_.signal.modulation_rate_hz;
    const double mod_phase = Constants::TWO_PI * mod_rate * time_s;
    return static_cast<float>(1.0 + 0.5 * std::sin(mod_phase));
}

} // namespace LookThroughJamming
} // namespace CognitiveRF