/**
 * @file AntennaSwitcher.cpp
 * @brief Implementation of Antenna Switch Controller
 * @details Manages high-speed circular antenna switching
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "AntennaSwitcher.hpp"
#include <algorithm>
#include <iostream>
#include <thread>

namespace CognitiveRF {
namespace DirectionFinding {

AntennaSwitcher::AntennaSwitcher(
    const AntennaSwitchConfig& config,
    SwitchingMode mode
)
    : config_(config)
    , mode_(mode)
    , is_switching_(false)
    , current_antenna_index_(0)
    , last_switch_time_us_(0)
    , sequence_index_(0)
{
    initializeSwitchingSequence();
}

AntennaSwitcher::~AntennaSwitcher() {
    stopSwitching();
}

bool AntennaSwitcher::startSwitching() {
    if (is_switching_) {
        return true;  // Already running
    }

    is_switching_ = true;
    current_antenna_index_ = 0;
    sequence_index_ = 0;
    last_switch_time_us_ = 0;

    if (mode_ != SwitchingMode::SIMULATED) {
        // Real GPIO control would be here
        // For now, just simulate
    }

    // Start switching thread
    switching_thread_ = std::make_unique<std::thread>(
        &AntennaSwitcher::switchingThreadLoop, this
    );

    return true;
}

void AntennaSwitcher::stopSwitching() {
    is_switching_ = false;
    if (switching_thread_ && switching_thread_->joinable()) {
        switching_thread_->join();
    }
}

uint32_t AntennaSwitcher::getCurrentAntennaIndex() const {
    return current_antenna_index_.load();
}

std::string AntennaSwitcher::getCurrentAntennaLabel() const {
    uint32_t idx = getCurrentAntennaIndex();
    if (idx < config_.antenna_labels.size()) {
        return config_.antenna_labels[idx];
    }
    return "Ant" + std::to_string(idx);
}

ReferenceSignal AntennaSwitcher::getReferenceSignal(uint32_t num_samples) {
    ReferenceSignal ref_signal;
    ref_signal.sample_rate_hz = 20000000;  // 20 MSPS
    ref_signal.switching_frequency_hz = config_.switching_frequency_hz;

    const float period_samples = static_cast<float>(ref_signal.sample_rate_hz) /
                                 config_.switching_frequency_hz;
    const float half_period = period_samples / 2.0f;

    ref_signal.reference_waveform.resize(num_samples);
    ref_signal.reference_phase.resize(num_samples);

    for (uint32_t i = 0; i < num_samples; i++) {
        float pos_in_period = std::fmod(i, period_samples);

        // Kare dalga: ilk yarısı 1, ikinci yarısı 0
        ref_signal.reference_waveform[i] = (pos_in_period < half_period) ? 1.0f : 0.0f;

        // Faz: 0 to 2π (kare dalga periyotu boyunca)
        ref_signal.reference_phase[i] = 2.0f * M_PI * pos_in_period / period_samples;
    }

    return ref_signal;
}

std::vector<uint32_t> AntennaSwitcher::getSwitchingPattern(uint32_t num_samples) {
    std::vector<uint32_t> pattern;
    pattern.reserve(num_samples);

    const float sample_rate = 20000000.0f;  // 20 MSPS
    const float period_samples = sample_rate / config_.switching_frequency_hz;
    const uint32_t num_antennas = config_.num_antennas;
    const float samples_per_antenna = period_samples / num_antennas;

    for (uint32_t i = 0; i < num_samples; i++) {
        float phase = std::fmod(i, period_samples);
        uint32_t antenna_idx = static_cast<uint32_t>(phase / samples_per_antenna) % num_antennas;
        pattern.push_back(antenna_idx);
    }

    return pattern;
}

uint32_t AntennaSwitcher::getSwitchingFrequency() const {
    return config_.switching_frequency_hz;
}

uint32_t AntennaSwitcher::getSwitchingPeriod() const {
    return config_.switching_period_us;
}

void AntennaSwitcher::updateConfig(const AntennaSwitchConfig& config) {
    bool was_switching = is_switching_.load();
    if (was_switching) {
        stopSwitching();
    }

    config_ = config;
    initializeSwitchingSequence();

    if (was_switching) {
        startSwitching();
    }
}

void AntennaSwitcher::switchingThreadLoop() {
    const uint32_t switch_period_us = config_.switching_period_us;
    const uint32_t num_antennas = config_.num_antennas;

    while (is_switching_) {
        // Switch to next antenna
        uint32_t next_idx = (current_antenna_index_ + 1) % num_antennas;
        current_antenna_index_ = next_idx;

        // Actual GPIO control would happen here
        if (!isSimulated()) {
            controlGPIOSwitch(next_idx);
        }

        // Sleep for switch period
        std::this_thread::sleep_for(std::chrono::microseconds(switch_period_us));
    }
}

bool AntennaSwitcher::controlGPIOSwitch(uint32_t antenna_index) {
    // Placeholder for GPIO control
    // Real implementation would use libgpiod or similar
    if (antenna_index >= config_.num_antennas) {
        return false;
    }

    // TODO: Implement actual GPIO control
    // For now, just log
    // std::cout << "[GPIO] Switch to Antenna " << antenna_index << "\n";

    return true;
}

void AntennaSwitcher::initializeSwitchingSequence() {
    switching_sequence_.clear();
    switching_sequence_.reserve(config_.num_antennas);

    // Simple circular sequence: 0, 1, 2, 3, 0, 1, 2, 3, ...
    for (uint32_t i = 0; i < config_.num_antennas; i++) {
        switching_sequence_.push_back(i);
    }

    sequence_index_ = 0;
}

} // namespace DirectionFinding
} // namespace CognitiveRF
