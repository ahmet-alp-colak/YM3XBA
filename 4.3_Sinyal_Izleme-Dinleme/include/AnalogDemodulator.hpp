/**
 * @file AnalogDemodulator.hpp
 * @brief Analog signal demodulation (AM/FM)
 * @details Implements envelope detector and frequency discriminator
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme
 * 
 * Reference: pipeline.txt Section [15] - Analog Demodulation
 * Reference: 4.3_SinyalDinleme-Izleme.txt Section 4.3.2 (Analog Demodulation)
 * 
 * Mathematical Formulas:
 * - FM: Δθ[n] = arg(x[n] · x*[n-1])
 * - AM: y[n] = √(I²[n] + Q²[n])
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef ANALOG_DEMODULATOR_HPP
#define ANALOG_DEMODULATOR_HPP

#include "DataStructures.hpp"
#include <vector>
#include <complex>
#include <cmath>

namespace CognitiveRF {
namespace SignalMonitoring {

/**
 * @class AnalogDemodulator
 * @brief Demodulates analog AM/FM signals to PCM audio
 * 
 * Features:
 * - AM demodulation via envelope detector
 * - FM demodulation via frequency discriminator
 * - DC blocking filter
 * - Audio resampling to standard rates
 * - De-emphasis filter for FM
 */
class AnalogDemodulator {
public:
    /**
     * @struct DemodConfig
     * @brief Configuration for analog demodulation
     */
    struct DemodConfig {
        uint32_t input_sample_rate_hz = 20000000;   // 20 MSPS from SDR
        uint32_t audio_sample_rate_hz = 48000;      // Standard audio rate
        float audio_gain = 1.0f;                     // Output gain
        bool enable_dc_blocking = true;
        bool enable_deemphasis = true;               // For FM (75μs time constant)
        float deemphasis_tau_us = 75.0f;             // US standard
    };

    /**
     * @brief Constructor
     * @param config Demodulation configuration
     */
    explicit AnalogDemodulator(const DemodConfig& config = DemodConfig());
    
    /**
     * @brief Demodulate AM signal
     * @param iq_samples Input IQ samples
     * @return Demodulated audio structure
     * 
     * Algorithm: Envelope Detector
     * y[n] = √(I²[n] + Q²[n])
     */
    DemodulatedAudio demodulateAM(const std::vector<std::complex<float>>& iq_samples);
    
    /**
     * @brief Demodulate FM signal
     * @param iq_samples Input IQ samples
     * @return Demodulated audio structure
     * 
     * Algorithm: Frequency Discriminator
     * Δθ[n] = arg(x[n] · x*[n-1])
     */
    DemodulatedAudio demodulateFM(const std::vector<std::complex<float>>& iq_samples);
    
    /**
     * @brief Generic demodulation dispatcher
     * @param iq_samples Input IQ samples
     * @param modulation Modulation type (AM or FM)
     * @return Demodulated audio structure
     */
    DemodulatedAudio demodulate(
        const std::vector<std::complex<float>>& iq_samples,
        ModulationType modulation
    );
    
    /**
     * @brief Reset internal state (for streaming mode)
     */
    void reset();

private:
    /**
     * @brief Apply DC blocking filter
     * @param samples Audio samples
     */
    void applyDCBlockingFilter(std::vector<float>& samples);
    
    /**
     * @brief Apply de-emphasis filter (FM)
     * @param samples Audio samples
     */
    void applyDeemphasisFilter(std::vector<float>& samples);
    
    /**
     * @brief Resample audio to target rate
     * @param input Input samples at input_rate
     * @param input_rate Input sample rate
     * @param output_rate Output sample rate
     * @return Resampled audio
     */
    std::vector<float> resampleAudio(
        const std::vector<float>& input,
        uint32_t input_rate,
        uint32_t output_rate
    );
    
    /**
     * @brief Calculate audio SNR estimate
     * @param samples Audio samples
     * @return Estimated SNR in dB
     */
    float estimateAudioSNR(const std::vector<float>& samples) const;

    // Configuration
    DemodConfig config_;
    
    // State for streaming mode
    std::complex<float> last_fm_sample_;
    float dc_block_x_prev_;
    float dc_block_y_prev_;
    float deemph_y_prev_;
};

} // namespace SignalMonitoring
} // namespace CognitiveRF

#endif // ANALOG_DEMODULATOR_HPP
