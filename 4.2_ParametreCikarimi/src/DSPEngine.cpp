/**
 * @file DSPEngine.cpp
 * @brief Implementation of DSP engine
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "DSPEngine.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace CognitiveRF {

DSPEngine::DSPEngine(double sample_rate_hz, float noise_floor_dbm)
    : sample_rate_hz_(sample_rate_hz)
    , noise_floor_dbm_(noise_floor_dbm)
{
}

float DSPEngine::computeRSSI(const std::vector<std::complex<float>>& iq_samples) const {
    if (iq_samples.empty()) {
        return -120.0f;  // Very low power
    }
    
    // Compute average power: mean(|IQ|^2)
    float avg_power = computeAveragePower(iq_samples);
    
    // Convert to dBm
    return linearToDBm(avg_power);
}

float DSPEngine::computeSNR(
    const std::vector<std::complex<float>>& iq_samples,
    float noise_floor_dbm
) const {
    float signal_power_dbm = computeRSSI(iq_samples);
    
    // SNR = Signal - Noise (in dB scale)
    return signal_power_dbm - noise_floor_dbm;
}

double DSPEngine::estimateBandwidth(
    const std::vector<std::complex<float>>& iq_samples,
    float power_threshold_db
) const {
    if (iq_samples.size() < 64) {
        return 0.0;
    }
    
    // Compute PSD
    std::vector<float> psd = computePSD(iq_samples);
    
    // Find peak power
    float peak_power = *std::max_element(psd.begin(), psd.end());
    float threshold = peak_power * std::pow(10.0f, power_threshold_db / 10.0f);
    
    // Count bins above threshold
    size_t bins_above_threshold = std::count_if(
        psd.begin(),
        psd.end(),
        [threshold](float p) { return p > threshold; }
    );
    
    // Convert bins to bandwidth
    double bin_width = sample_rate_hz_ / static_cast<double>(psd.size());
    return static_cast<double>(bins_above_threshold) * bin_width;
}

float DSPEngine::estimateBaudRate(const std::vector<std::complex<float>>& iq_samples) const {
    if (iq_samples.size() < 128) {
        return 0.0f;
    }
    
    // Compute autocorrelation
    std::vector<float> autocorr = computeAutocorrelation(iq_samples);
    
    // Find first significant peak after zero lag (symbol period)
    // Skip first 10% to avoid DC component
    size_t start_idx = autocorr.size() / 10;
    
    auto peak_it = std::max_element(
        autocorr.begin() + start_idx,
        autocorr.end()
    );
    
    if (peak_it == autocorr.end()) {
        return 0.0f;
    }
    
    size_t peak_lag = std::distance(autocorr.begin(), peak_it);
    
    // Baud rate = sample_rate / symbol_period_samples
    if (peak_lag > 0) {
        return static_cast<float>(sample_rate_hz_ / static_cast<double>(peak_lag));
    }
    
    return 0.0f;
}

DSPMeasurements DSPEngine::measureAll(const std::vector<std::complex<float>>& iq_samples) const {
    DSPMeasurements measurements;
    
    measurements.rssi_dbm = computeRSSI(iq_samples);
    measurements.snr_db = computeSNR(iq_samples, noise_floor_dbm_);
    measurements.bandwidth_hz = estimateBandwidth(iq_samples);
    measurements.baud_rate_hz = estimateBaudRate(iq_samples);
    
    // Find peak frequency
    std::vector<float> psd = computePSD(iq_samples);
    measurements.peak_frequency_hz = findPeakFrequency(psd);
    
    return measurements;
}

float DSPEngine::computeAveragePower(const std::vector<std::complex<float>>& iq_samples) const {
    if (iq_samples.empty()) {
        return 0.0f;
    }
    
    // Compute mean of |IQ|^2
    float sum_power = 0.0f;
    for (const auto& sample : iq_samples) {
        float magnitude_sq = std::norm(sample);  // |z|^2 = real^2 + imag^2
        sum_power += magnitude_sq;
    }
    
    return sum_power / static_cast<float>(iq_samples.size());
}

float DSPEngine::linearToDBm(float power_linear) const {
    if (power_linear <= 0.0f) {
        return -120.0f;  // Very low power
    }
    
    // Convert to dB scale: 10 * log10(power)
    float power_db = 10.0f * std::log10(power_linear);
    
    // Add calibration offset (hardware-specific)
    return power_db + RSSI_CALIBRATION_OFFSET;
}

std::vector<float> DSPEngine::computePSD(const std::vector<std::complex<float>>& iq_samples) const {
    // Simplified PSD computation (magnitude spectrum)
    // In production, use FFTW3 with proper windowing and Welch method
    
    size_t N = iq_samples.size();
    std::vector<float> psd(N);
    
    // Compute magnitude spectrum (simplified FFT approximation)
    // This is a placeholder - real implementation should use FFTW3
    for (size_t k = 0; k < N; ++k) {
        std::complex<float> sum(0.0f, 0.0f);
        for (size_t n = 0; n < N; ++n) {
            float angle = -2.0f * M_PI * static_cast<float>(k * n) / static_cast<float>(N);
            std::complex<float> twiddle(std::cos(angle), std::sin(angle));
            sum += iq_samples[n] * twiddle;
        }
        psd[k] = std::norm(sum) / static_cast<float>(N);
    }
    
    return psd;
}

double DSPEngine::findPeakFrequency(const std::vector<float>& psd) const {
    if (psd.empty()) {
        return 0.0;
    }
    
    // Find index of maximum power
    auto max_it = std::max_element(psd.begin(), psd.end());
    size_t peak_bin = std::distance(psd.begin(), max_it);
    
    // Convert bin index to frequency offset
    double bin_width = sample_rate_hz_ / static_cast<double>(psd.size());
    double freq_offset = static_cast<double>(peak_bin) * bin_width;
    
    // Shift to centered spectrum [-Fs/2, Fs/2]
    if (freq_offset > sample_rate_hz_ / 2.0) {
        freq_offset -= sample_rate_hz_;
    }
    
    return freq_offset;
}

std::vector<float> DSPEngine::computeAutocorrelation(
    const std::vector<std::complex<float>>& iq_samples
) const {
    size_t N = iq_samples.size();
    size_t max_lag = std::min(N / 4, size_t(512));  // Limit computation
    
    std::vector<float> autocorr(max_lag);
    
    for (size_t lag = 0; lag < max_lag; ++lag) {
        std::complex<float> sum(0.0f, 0.0f);
        for (size_t n = 0; n < N - lag; ++n) {
            sum += iq_samples[n] * std::conj(iq_samples[n + lag]);
        }
        autocorr[lag] = std::abs(sum);
    }
    
    // Normalize
    if (autocorr[0] > 0.0f) {
        for (auto& val : autocorr) {
            val /= autocorr[0];
        }
    }
    
    return autocorr;
}

} // namespace CognitiveRF
