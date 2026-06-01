/**
 * @file SignalCorrector.hpp
 * @brief CFO correction using complex frequency shift
 * @details Applies Kalman-filtered CFO to raw IQ data for frequency locking
 * 
 * Mathematical Operation:
 * x_corrected(n) = x_raw(n) * e^(-j * 2π * CFO * n / Fs)
 * 
 * Where:
 * - x_raw(n): Raw IQ sample at index n
 * - CFO: Carrier Frequency Offset in Hz (from Kalman filter)
 * - Fs: Sample rate in Hz
 * - j: Imaginary unit
 * 
 * References:
 * - Section 4.2.1: Complex Shift işlemi
 * - Pipeline Stage [8]: CFO Düzeltme
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef SIGNAL_CORRECTOR_HPP
#define SIGNAL_CORRECTOR_HPP

#include <complex>
#include <vector>
#include <cmath>

namespace CognitiveRF {

/**
 * @class SignalCorrector
 * @brief Applies frequency correction to IQ data streams
 * 
 * Performance optimizations:
 * - Pre-computes rotation phasor for entire block
 * - Uses std::complex multiplication (compiler-optimized)
 * - Supports in-place correction to minimize memory allocation
 */
class SignalCorrector {
public:
    /**
     * @brief Constructor
     * @param sample_rate_hz Sampling rate in Hz (e.g., 20e6 for 20 MSPS)
     */
    explicit SignalCorrector(double sample_rate_hz = 20e6);

    /**
     * @brief Apply CFO correction to IQ buffer (out-of-place)
     * @param input Raw IQ samples
     * @param cfo_hz Carrier frequency offset in Hz (from Kalman filter)
     * @param output Corrected IQ samples (resized automatically)
     * 
     * Formula: output[n] = input[n] * exp(-j * 2π * cfo_hz * n / sample_rate)
     * 
     * @note This is the zero-copy friendly version - caller manages buffers
     */
    void correctFrequency(
        const std::vector<std::complex<float>>& input,
        double cfo_hz,
        std::vector<std::complex<float>>& output
    );

    /**
     * @brief Apply CFO correction in-place
     * @param iq_data IQ samples to correct (modified in-place)
     * @param cfo_hz Carrier frequency offset in Hz
     * 
     * @note More memory efficient but modifies original data
     */
    void correctFrequencyInPlace(
        std::vector<std::complex<float>>& iq_data,
        double cfo_hz
    );

    /**
     * @brief Apply CFO correction with phase continuity
     * @param input Raw IQ samples
     * @param cfo_hz Carrier frequency offset in Hz
     * @param output Corrected IQ samples
     * 
     * @details Maintains phase continuity across multiple calls
     * Useful for streaming applications where buffers are processed sequentially
     */
    void correctFrequencyContinuous(
        const std::vector<std::complex<float>>& input,
        double cfo_hz,
        std::vector<std::complex<float>>& output
    );

    /**
     * @brief Reset phase accumulator for continuous correction
     */
    void resetPhase() { phase_accumulator_ = 0.0; }

    /**
     * @brief Update sample rate (e.g., when SDR configuration changes)
     * @param sample_rate_hz New sample rate in Hz
     */
    void setSampleRate(double sample_rate_hz) {
        sample_rate_hz_ = sample_rate_hz;
    }

    /**
     * @brief Get current sample rate
     * @return Sample rate in Hz
     */
    double getSampleRate() const { return sample_rate_hz_; }

private:
    double sample_rate_hz_;      ///< Sampling rate (Hz)
    double phase_accumulator_;   ///< Phase continuity for streaming mode

    /**
     * @brief Compute rotation phasor for given CFO
     * @param cfo_hz Frequency offset in Hz
     * @param sample_index Sample index n
     * @return Complex phasor e^(-j * 2π * cfo * n / Fs)
     */
    inline std::complex<float> computePhasor(double cfo_hz, size_t sample_index) const {
        // Phase = -2π * CFO * n / Fs
        double phase = -2.0 * M_PI * cfo_hz * static_cast<double>(sample_index) / sample_rate_hz_;
        return std::complex<float>(std::cos(phase), std::sin(phase));
    }
};

} // namespace CognitiveRF

#endif // SIGNAL_CORRECTOR_HPP
