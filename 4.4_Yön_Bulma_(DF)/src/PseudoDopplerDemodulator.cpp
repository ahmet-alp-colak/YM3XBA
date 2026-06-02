/**
 * @file PseudoDopplerDemodulator.cpp
 * @brief Implementation of Pseudo-Doppler FM Demodulator
 * @details FM demodulation of antenna switching modulation
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "PseudoDopplerDemodulator.hpp"
#include <algorithm>
#include <iostream>
#include <cstring>

namespace CognitiveRF {
namespace DirectionFinding {

PseudoDopplerDemodulator::PseudoDopplerDemodulator(
    uint32_t sample_rate_hz,
    float bandwidth_hz
)
    : sample_rate_hz_(sample_rate_hz)
    , bandwidth_hz_(bandwidth_hz)
    , instantaneous_phase_(0.0f)
    , phase_rate_(0.0f)
    , avg_snr_db_(0.0f)
{
    // deque doesn't support reserve; size will expand dynamically
}

PseudoDopplerDemodulator::~PseudoDopplerDemodulator() = default;

DemodulatedPseudoDopplerSignal PseudoDopplerDemodulator::demodulate(
    const std::vector<std::complex<float>>& iq_data,
    float snr_db
) {
    if (iq_data.empty()) {
        throw std::invalid_argument("IQ data is empty");
    }

    DemodulatedPseudoDopplerSignal result;
    result.timestamp_us = 0;
    result.snr_db = snr_db;
    avg_snr_db_ = snr_db;

    result.demodulated_signal.reserve(iq_data.size());
    float cumulative_phase = instantaneous_phase_;

    // FM demodulation using frequency discriminator (phase derivative method)
    std::complex<float> prev_sample = iq_data[0];

    for (size_t i = 1; i < iq_data.size(); i++) {
        std::complex<float> curr_sample = iq_data[i];

        // Compute phase derivative: arg(x[n] * conj(x[n-1]))
        float phase_diff = computePhaseDerivative(prev_sample, curr_sample);

        // Unwrap phase
        float unwrapped_phase = unwrapPhase(phase_diff, cumulative_phase);
        cumulative_phase = unwrapped_phase;

        // Normalize to frequency deviation
        float freq_deviation = unwrapped_phase / (2.0f * M_PI);
        result.demodulated_signal.push_back(freq_deviation);

        // Track phase history
        phase_history_.push_back(unwrapped_phase);
        if (phase_history_.size() > MAX_HISTORY_SIZE) {
            phase_history_.pop_front();
        }

        iq_history_.push_back(curr_sample);
        if (iq_history_.size() > MAX_HISTORY_SIZE) {
            iq_history_.pop_front();
        }

        prev_sample = curr_sample;
    }

    instantaneous_phase_ = cumulative_phase;

    // Apply smoothing filter
    if (!result.demodulated_signal.empty()) {
        result.instantaneous_phase_radians = applyPhaseFilter(10);
        result.phase_rate_rad_per_sample = phase_history_.size() > 1 ?
            phase_history_.back() - phase_history_[phase_history_.size() - 2] : 0.0f;
    }

    // Copy phase history to result
    result.phase_history = phase_history_;

    return result;
}

float PseudoDopplerDemodulator::computePhaseDerivative(
    std::complex<float> iq_sample_prev,
    std::complex<float> iq_sample_curr
) const {
    // FM discriminator: phase = arg(curr * conj(prev))
    // = atan2(Im(curr * conj(prev)), Re(curr * conj(prev)))

    std::complex<float> product = iq_sample_curr * std::conj(iq_sample_prev);
    float phase = std::atan2(product.imag(), product.real());

    return phase;
}

float PseudoDopplerDemodulator::unwrapPhase(
    float phase,
    float previous_phase
) const {
    float diff = phase - previous_phase;

    // Wrap difference to [-π, π]
    while (diff > M_PI) {
        diff -= 2.0f * M_PI;
    }
    while (diff < -M_PI) {
        diff += 2.0f * M_PI;
    }

    return previous_phase + diff;
}

float PseudoDopplerDemodulator::normalizePhase(float phase) {
    // Normalize to [-π, π]
    while (phase > M_PI) {
        phase -= 2.0f * M_PI;
    }
    while (phase < -M_PI) {
        phase += 2.0f * M_PI;
    }
    return phase;
}

float PseudoDopplerDemodulator::applyPhaseFilter(size_t window_size) const {
    if (phase_history_.empty()) {
        return 0.0f;
    }

    size_t effective_window = std::min(window_size, phase_history_.size());
    float sum = 0.0f;

    for (size_t i = phase_history_.size() - effective_window; i < phase_history_.size(); i++) {
        sum += phase_history_[i];
    }

    return sum / effective_window;
}

std::vector<float> PseudoDopplerDemodulator::getPhaseHistoryWindow(size_t max_size) const {
    std::vector<float> window;

    size_t start = phase_history_.size() > max_size ?
        phase_history_.size() - max_size : 0;

    for (size_t i = start; i < phase_history_.size(); i++) {
        window.push_back(phase_history_[i]);
    }

    return window;
}

void PseudoDopplerDemodulator::reset() {
    phase_history_.clear();
    iq_history_.clear();
    instantaneous_phase_ = 0.0f;
    phase_rate_ = 0.0f;
    avg_snr_db_ = 0.0f;
}

} // namespace DirectionFinding
} // namespace CognitiveRF
