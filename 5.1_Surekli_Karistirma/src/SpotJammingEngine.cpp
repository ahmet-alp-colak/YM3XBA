/**
 * @file SpotJammingEngine.cpp
 * @brief Implementation of spot jamming signal generation engine
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.1 Sürekli Karıştırma (Spot Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-06-29
 */

#include "SpotJammingEngine.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace CognitiveRF {
namespace SpotJamming {

SpotJammingEngine::SpotJammingEngine(const PipelineConfig& config)
    : config_(config)
    , phase_accumulator_(0.0)
    , sample_counter_(0)
    , rng_(config.signal.noise_seed == 0 ? 
           std::random_device{}() : config.signal.noise_seed)
    , gaussian_dist_(0.0f, 1.0f)
{
    stats_.total_samples_generated = 0;
    stats_.average_power_dbm = -100.0;
    stats_.buffer_underruns = 0;
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Engine initialized" << std::endl;
    }
}

SpotJammingEngine::~SpotJammingEngine() {
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Engine destroyed. Total samples: " 
                  << stats_.total_samples_generated << std::endl;
    }
}

bool SpotJammingEngine::initialize(const TargetIntelligence& target) {
    if (!target.isValid()) {
        std::cerr << config_.log_prefix << " Invalid target parameters" << std::endl;
        return false;
    }
    
    current_target_ = target;
    phase_accumulator_ = 0.0;
    sample_counter_ = 0;
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Initialized for target ID=" << target.target_id
                  << " freq=" << (target.center_freq_hz / 1e6) << " MHz"
                  << " bw=" << (target.bandwidth_hz / 1e3) << " kHz" << std::endl;
    }
    
    return true;
}

size_t SpotJammingEngine::generateSamples(IQBuffer& output, size_t num_samples) {
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
        
    stats_.total_samples_generated += num_samples;
    sample_counter_ += num_samples;
    
    return num_samples;
}

void SpotJammingEngine::generateContinuousWave(IQBuffer& output, size_t num_samples) {
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

void SpotJammingEngine::generateGaussianNoise(IQBuffer& output, size_t num_samples) {
    const float amplitude = config_.signal.noise_amplitude;
    
    for (size_t i = 0; i < num_samples; ++i) {
        const float i_noise = gaussian_dist_(rng_) * amplitude;
        const float q_noise = gaussian_dist_(rng_) * amplitude;
        output[i] = IQSample(i_noise, q_noise);
    }
}

void SpotJammingEngine::generateModulatedNoise(IQBuffer& output, size_t num_samples) {
    const double sample_rate = config_.hardware.sample_rate_hz;
    const float noise_amp = config_.signal.noise_amplitude;
    
    for (size_t i = 0; i < num_samples; ++i) {
        const double time_s = static_cast<double>(sample_counter_ + i) / sample_rate;
        const float envelope = computeInstantaneousAmplitude(time_s);
        const float i_noise = gaussian_dist_(rng_) * noise_amp * envelope;
        const float q_noise = gaussian_dist_(rng_) * noise_amp * envelope;
        output[i] = IQSample(i_noise, q_noise);
    }
}

float SpotJammingEngine::computeInstantaneousAmplitude(double time_s) const {
    const double mod_rate = config_.signal.modulation_rate_hz;
    const double mod_phase = Constants::TWO_PI * mod_rate * time_s;
    return static_cast<float>(1.0 + 0.5 * std::sin(mod_phase));
}

void SpotJammingEngine::normalizeBuffer(IQBuffer& buffer) const {
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

void SpotJammingEngine::updateSignalConfig(const SignalConfig& new_config) {
    if (!new_config.validate()) {
        std::cerr << config_.log_prefix << " Invalid signal configuration" << std::endl;
        return;
    }
    config_.signal = new_config;
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Signal configuration updated" << std::endl;
    }
}

SpotJammingEngine::Statistics SpotJammingEngine::getStatistics() const {
    return stats_;
}

void SpotJammingEngine::reset() {
    phase_accumulator_ = 0.0;
    sample_counter_ = 0;
    stats_.total_samples_generated = 0;
    stats_.average_power_dbm = -100.0;
    stats_.buffer_underruns = 0;
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Engine reset" << std::endl;
    }
}

} // namespace SpotJamming
} // namespace CognitiveRF
