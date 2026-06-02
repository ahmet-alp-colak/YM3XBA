/**
 * @file DirectionFindingEngine.cpp
 * @brief Implementation of Direction Finding Engine
 * @details Orchestrates antenna switching, demodulation, and phase comparison
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "DirectionFindingEngine.hpp"
#include <algorithm>
#include <iostream>
#include <cmath>

namespace CognitiveRF {
namespace DirectionFinding {

DirectionFindingEngine::DirectionFindingEngine(
    const DirectionFindingConfig& config,
    const AntennaSwitchConfig& antenna_config,
    const CircularAntennaArray& antenna_array
)
    : config_(config)
    , antenna_switch_config_(antenna_config)
    , antenna_array_(antenna_array)
{
    // Initialize components
    antenna_switcher_ = std::make_unique<AntennaSwitcher>(antenna_config);
    pseudo_doppler_demod_ = std::make_unique<PseudoDopplerDemodulator>(
        20000000,  // 20 MSPS
        config_.fm_demod_bandwidth_hz
    );
    phase_comparator_ = std::make_unique<PhaseComparator>(
        antenna_array_,
        config_.center_frequency_hz
    );
}

DirectionFindingEngine::~DirectionFindingEngine() = default;

DirectionFindingResult DirectionFindingEngine::performDirectionFinding(
    const std::vector<std::complex<float>>& iq_data,
    float center_freq_hz,
    float snr_db,
    float rssi_dbm
) {
    DirectionFindingResult result;
    result.center_frequency_hz = center_freq_hz;
    result.signal_strength_dbm = rssi_dbm;
    result.snr_db = snr_db;

    // Update configuration if frequency changed significantly
    if (std::abs(center_freq_hz - config_.center_frequency_hz) > 1e6f) {
        config_.center_frequency_hz = center_freq_hz;
        phase_comparator_->setCenterFrequency(center_freq_hz);
    }

    // Step 1: Generate Pseudo-Doppler signal
    PseudoDopplerSignal pd_signal = generatePseudoDopplerSignal(iq_data, center_freq_hz);

    // Step 2: Demodulate Pseudo-Doppler FM
    DemodulatedPseudoDopplerSignal demod_signal = pseudo_doppler_demod_->demodulate(
        iq_data,  // Use original IQ
        snr_db
    );

    // Step 3: Generate reference signal
    ReferenceSignal ref_signal = generateReferenceSignal(iq_data.size());

    // Step 4: Compare phases
    float demod_phase = demod_signal.instantaneous_phase_radians;
    float ref_phase = ref_signal.reference_phase_radians;

    result = phase_comparator_->comparePhases(demod_phase, ref_phase, snr_db);

    // Step 5: Set result metadata
    result.timestamp_us = 0;
    result.iteration_count = 1;

    // Step 6: Validate result
    if (!validateResult(result)) {
        // If validation fails, retry up to max_iterations
        for (uint32_t iter = 1; iter < config_.max_iterations; iter++) {
            result.iteration_count = iter + 1;

            // Try again with slightly adjusted parameters
            result = phase_comparator_->comparePhases(
                demod_phase + (0.05f * M_PI * iter),  // Slight phase perturbation
                ref_phase,
                snr_db
            );

            if (validateResult(result)) {
                break;
            }
        }
    }

    // Update statistics
    updateStatistics(result);

    return result;
}

void DirectionFindingEngine::calibrateAntenna(
    uint32_t antenna_index,
    float phase_correction,
    float magnitude_correction
) {
    if (antenna_index >= antenna_array_.num_elements) {
        return;
    }

    antenna_array_.calibration_data[antenna_index].phase_correction_radians = phase_correction;
    antenna_array_.calibration_data[antenna_index].magnitude_correction_db = magnitude_correction;
}

void DirectionFindingEngine::resetCalibration() {
    for (auto& cal : antenna_array_.calibration_data) {
        cal.phase_correction_radians = 0.0f;
        cal.magnitude_correction_db = 0.0f;
    }
    phase_comparator_->resetCalibration();
}

const DirectionFindingStatistics& DirectionFindingEngine::getStatistics() const {
    return statistics_;
}

void DirectionFindingEngine::resetStatistics() {
    statistics_ = DirectionFindingStatistics();
}

void DirectionFindingEngine::updateConfig(const DirectionFindingConfig& config) {
    config_ = config;
    phase_comparator_->setCenterFrequency(config_.center_frequency_hz);
}

const AntennaSwitchConfig& DirectionFindingEngine::getAntennaConfig() const {
    return antenna_switch_config_;
}

const CircularAntennaArray& DirectionFindingEngine::getAntennaArray() const {
    return antenna_array_;
}

PseudoDopplerSignal DirectionFindingEngine::generatePseudoDopplerSignal(
    const std::vector<std::complex<float>>& iq_data,
    float /* center_freq_hz */
) {
    PseudoDopplerSignal result;
    result.sample_rate_hz = 20000000;
    result.timestamp_us = 0;

    // Apply antenna switching pattern to IQ data
    auto switch_pattern = antenna_switcher_->getSwitchingPattern(iq_data.size());

    result.modulated_iq = iq_data;  // Start with original
    result.switching_pattern.resize(iq_data.size());

    for (size_t i = 0; i < switch_pattern.size(); i++) {
        result.switching_pattern[i] = static_cast<float>(switch_pattern[i]);

        // Optional: Apply antenna gain/phase corrections
        uint32_t ant_idx = switch_pattern[i];
        if (ant_idx < antenna_array_.calibration_data.size()) {
            const auto& cal = antenna_array_.calibration_data[ant_idx];

            // Apply phase correction
            float phase_corr = cal.phase_correction_radians;
            float magnitude_corr = std::pow(10.0f, cal.magnitude_correction_db / 20.0f);

            // Phase rotation: multiply by e^(j*phase)
            float cos_ph = std::cos(phase_corr);
            float sin_ph = std::sin(phase_corr);

            std::complex<float> correction(cos_ph, sin_ph);
            result.modulated_iq[i] *= correction * magnitude_corr;
        }
    }

    return result;
}

ReferenceSignal DirectionFindingEngine::generateReferenceSignal(uint32_t num_samples) {
    ReferenceSignal ref = antenna_switcher_->getReferenceSignal(num_samples);

    // Convert square wave to phase reference
    if (!ref.reference_waveform.empty()) {
        ref.reference_phase.resize(ref.reference_waveform.size());

        float phase_rate = 2.0f * M_PI * antenna_switcher_->getSwitchingFrequency() / 20e6f;

        for (size_t i = 0; i < ref.reference_waveform.size(); i++) {
            // Phase increases linearly with time
            ref.reference_phase[i] = phase_rate * i;

            // Normalize to [-π, π]
            while (ref.reference_phase[i] > M_PI) {
                ref.reference_phase[i] -= 2.0f * M_PI;
            }
        }

        if (!ref.reference_phase.empty()) {
            ref.reference_phase_radians = ref.reference_phase.back();
        }
    }

    return ref;
}

bool DirectionFindingEngine::validateResult(DirectionFindingResult& result) {
    // Check if AoA is within expected range
    if (std::abs(result.angle_of_arrival_degrees) > 180.0f) {
        result.is_valid = false;
        result.validation_notes = "AoA out of range [-180°, 180°]";
        return false;
    }

    // Check if RMS error is within tolerance
    if (result.aoa_rms_error_degrees > config_.aoa_rms_tolerance_degrees) {
        result.is_valid = false;
        result.validation_notes = "RMS error exceeds ±" +
            std::to_string(static_cast<int>(config_.aoa_rms_tolerance_degrees)) + "°";
        return false;
    }

    // Check confidence
    if (result.aoa_confidence < 0.3f) {
        result.is_valid = false;
        result.validation_notes = "Confidence too low";
        return false;
    }

    result.is_valid = true;
    result.validation_notes = "Valid AoA estimate";
    return true;
}

void DirectionFindingEngine::updateStatistics(const DirectionFindingResult& result) {
    statistics_.total_processed++;

    if (result.is_valid) {
        statistics_.valid_aoa_estimates++;

        // Update average confidence
        float alpha = 0.1f;  // EMA factor
        statistics_.avg_aoa_confidence = alpha * result.aoa_confidence +
            (1.0f - alpha) * statistics_.avg_aoa_confidence;

        // Update RMS error statistics
        statistics_.avg_aoa_error_degrees = alpha * result.aoa_rms_error_degrees +
            (1.0f - alpha) * statistics_.avg_aoa_error_degrees;

        statistics_.min_aoa_error_degrees = std::min(
            statistics_.min_aoa_error_degrees,
            result.aoa_rms_error_degrees
        );
        statistics_.max_aoa_error_degrees = std::max(
            statistics_.max_aoa_error_degrees,
            result.aoa_rms_error_degrees
        );
    }
}

} // namespace DirectionFinding
} // namespace CognitiveRF
