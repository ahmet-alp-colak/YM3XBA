/**
 * @file PhaseComparator.cpp
 * @brief Implementation of Phase Comparison and AoA Estimation
 * @details Converts phase differences to angle of arrival
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "PhaseComparator.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace CognitiveRF {
namespace DirectionFinding {

PhaseComparator::PhaseComparator(
    const CircularAntennaArray& antenna_array,
    float center_frequency_hz
)
    : antenna_array_(antenna_array)
    , center_frequency_hz_(center_frequency_hz)
    , phase_to_angle_scale_(90.0f / (2.0f * M_PI))  // 4 antennas: 360°/4 = 90°
    , phase_to_angle_offset_(0.0f)
{
    computeTransformCoefficients();
    updateTransformForFrequency();
}

PhaseComparator::~PhaseComparator() = default;

DirectionFindingResult PhaseComparator::comparePhases(
    float demodulated_phase,
    float reference_phase,
    float snr_db
) {
    DirectionFindingResult result;
    result.timestamp_us = 0;
    result.snr_db = snr_db;
    result.demodulated_phase_radians = demodulated_phase;
    result.reference_phase_radians = reference_phase;

    // Normalize phases to [-π, π]
    // Implement inline normalization
    auto normalize = [](float p) {
        while (p > M_PI) p -= 2.0f * M_PI;
        while (p < -M_PI) p += 2.0f * M_PI;
        return p;
    };

    float norm_demod = normalize(demodulated_phase);
    float norm_ref = normalize(reference_phase);

    // Compute phase difference
    float phase_diff = norm_demod - norm_ref;

    // Unwrap phase difference
    if (phase_diff > M_PI) {
        phase_diff -= 2.0f * M_PI;
    } else if (phase_diff < -M_PI) {
        phase_diff += 2.0f * M_PI;
    }

    result.phase_difference_radians = phase_diff;

    // Convert to AoA (degrees)
    float aoa = phaseToAngle(phase_diff);
    result.angle_of_arrival_degrees = aoa;

    // Ensure result is in [-180°, 180°]
    while (result.angle_of_arrival_degrees > 180.0f) {
        result.angle_of_arrival_degrees -= 360.0f;
    }
    while (result.angle_of_arrival_degrees < -180.0f) {
        result.angle_of_arrival_degrees += 360.0f;
    }

    // Track history
    aoa_history_.push_back(aoa);
    phase_diff_history_.push_back(phase_diff);

    if (aoa_history_.size() > MAX_HISTORY_SIZE) {
        aoa_history_.pop_front();
        phase_diff_history_.pop_front();
    }

    // Compute confidence
    result.aoa_confidence = computeConfidence(snr_db);
    confidence_history_.push_back(result.aoa_confidence);
    if (confidence_history_.size() > MAX_HISTORY_SIZE) {
        confidence_history_.pop_front();
    }

    // Estimate RMS error
    result.aoa_rms_error_degrees = estimateRMSError(snr_db);

    // Validation: check if within tolerance
    result.is_valid = (result.aoa_rms_error_degrees <= 10.0f);

    if (result.is_valid) {
        result.validation_notes = "RMS error within ±10° tolerance";
    } else {
        result.validation_notes = "RMS error exceeds tolerance";
    }

    return result;
}

float PhaseComparator::phaseToAngle(float phase_difference) const {
    // Linear transformation: φ [rad] → θ [degrees]
    // For 4-element circular array:
    // θ = (φ / 2π) * (360° / 4) = (φ / 2π) * 90°
    return phase_difference * phase_to_angle_scale_;
}

float PhaseComparator::angleToPhase(float angle_degrees) const {
    return angle_degrees / phase_to_angle_scale_;
}

float PhaseComparator::computeConfidence(float snr_db) const {
    return logisticConfidence(snr_db);
}

float PhaseComparator::estimateRMSError(float snr_db) const {
    // SNR-based error model (empirical)
    // Higher SNR → Lower error
    // Error [degrees] = a / sqrt(SNR_linear)
    // where a is a calibration constant

    if (snr_db < -20.0f) {
        return 45.0f;  // Very poor SNR
    }

    float snr_linear = std::pow(10.0f, snr_db / 10.0f);
    const float calibration_constant = 15.0f;  // Empirically determined

    float error = calibration_constant / std::sqrt(snr_linear);

    // Clamp to reasonable range
    return std::min(45.0f, std::max(1.0f, error));
}

float PhaseComparator::getAverageAoA(size_t window_size) const {
    if (aoa_history_.empty()) {
        return 0.0f;
    }

    size_t effective_window = std::min(window_size, aoa_history_.size());
    float sum = 0.0f;

    for (size_t i = aoa_history_.size() - effective_window; i < aoa_history_.size(); i++) {
        sum += aoa_history_[i];
    }

    return sum / effective_window;
}

void PhaseComparator::resetCalibration() {
    clearHistory();
    computeTransformCoefficients();
}

void PhaseComparator::clearHistory() {
    aoa_history_.clear();
    phase_diff_history_.clear();
    confidence_history_.clear();
}

void PhaseComparator::computeTransformCoefficients() {
    // For 4-element circular array:
    // Scale: 360° / (4 * 2π) = 90° / 2π ≈ 14.324
    phase_to_angle_scale_ = 360.0f / (antenna_array_.num_elements * 2.0f * M_PI);

    // Offset can be adjusted based on array orientation
    // For now, assume 0° offset
    phase_to_angle_offset_ = 0.0f;

    // Apply antenna calibration
    if (!antenna_array_.calibration_data.empty()) {
        // Average phase calibration across elements
        float avg_phase_correction = 0.0f;
        for (const auto& cal : antenna_array_.calibration_data) {
            avg_phase_correction += cal.phase_correction_radians;
        }
        avg_phase_correction /= antenna_array_.calibration_data.size();

        // Convert phase correction to angle offset
        phase_to_angle_offset_ = phaseToAngle(avg_phase_correction);
    }
}

void PhaseComparator::updateTransformForFrequency() {
    // For pseudo-Doppler DF with 4-element array,
    // the phase-to-angle relationship is frequency-independent
    // (by design of the Pseudo-Doppler method)
    //
    // In more general DF systems, this would depend on wavelength.
    // For now, we keep the transform constant.

    // Optional: Validate frequency is reasonable
    if (center_frequency_hz_ < 1e6f || center_frequency_hz_ > 6e9f) {
        // Outside expected range, but don't fail
    }
}

float PhaseComparator::logisticConfidence(float snr_db) {
    // Logistic confidence model:
    // C(SNR) = 1 / (1 + exp(-k * (SNR - SNR_mid)))
    // k = 0.5 (slope), SNR_mid = 10 dB (inflection point)

    const float k = 0.5f;
    const float snr_mid = 10.0f;
    const float exponent = -k * (snr_db - snr_mid);

    float confidence = 1.0f / (1.0f + std::exp(exponent));

    // Clamp to [0, 1]
    return std::max(0.0f, std::min(1.0f, confidence));
}

} // namespace DirectionFinding
} // namespace CognitiveRF
