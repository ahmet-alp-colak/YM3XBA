/**
 * @file DSPEngine.hpp
 * @brief Digital Signal Processing engine for RF parameter measurement
 * @details Computes RSSI, SNR, bandwidth, and other physical signal parameters
 * 
 * Measurements:
 * - RSSI (Received Signal Strength Indicator) in dBm
 * - SNR (Signal-to-Noise Ratio) in dB
 * - Occupied bandwidth
 * - Symbol rate (Baud rate) for digital signals
 * 
 * References:
 * - Section 4.2.4: Sayısal Demodülasyon Motoru ve RF Parametre Ölçümü
 * - Pipeline Stage [16]: Parameter Extraction
 * 
 * Note: This is a wrapper/simulator for liquid-dsp functionality
 * In production, integrate actual liquid-dsp library calls
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef DSP_ENGINE_HPP
#define DSP_ENGINE_HPP

#include <complex>
#include <vector>
#include <cmath>

namespace CognitiveRF {

/**
 * @struct DSPMeasurements
 * @brief Container for DSP measurement results
 */
struct DSPMeasurements {
    float rssi_dbm;           ///< Received signal strength (dBm)
    float snr_db;             ///< Signal-to-noise ratio (dB)
    double bandwidth_hz;      ///< Occupied bandwidth (Hz)
    float baud_rate_hz;       ///< Symbol rate for digital signals
    float peak_frequency_hz;  ///< Peak frequency offset from center
};

/**
 * @class DSPEngine
 * @brief High-performance DSP operations for RF signal analysis
 * 
 * Features:
 * - Power spectral density (PSD) estimation
 * - RSSI calculation from IQ magnitude
 * - SNR estimation using noise floor
 * - Bandwidth measurement (occupied bandwidth method)
 * - Symbol rate estimation
 */
class DSPEngine {
public:
    /**
     * @brief Constructor
     * @param sample_rate_hz Sampling rate in Hz
     * @param noise_floor_dbm Reference noise floor in dBm (default: -100 dBm)
     */
    explicit DSPEngine(
        double sample_rate_hz = 20e6,
        float noise_floor_dbm = -100.0f
    );

    /**
     * @brief Compute RSSI from IQ samples
     * @param iq_samples Input IQ data
     * @return RSSI in dBm
     * 
     * Formula: RSSI = 10 * log10(mean(|IQ|^2)) + calibration_offset
     * 
     * @note Assumes 50 ohm impedance and appropriate calibration
     */
    float computeRSSI(const std::vector<std::complex<float>>& iq_samples) const;

    /**
     * @brief Estimate SNR from IQ samples
     * @param iq_samples Input IQ data
     * @param noise_floor_dbm Measured or estimated noise floor
     * @return SNR in dB
     * 
     * Formula: SNR = Signal_Power_dB - Noise_Floor_dB
     */
    float computeSNR(
        const std::vector<std::complex<float>>& iq_samples,
        float noise_floor_dbm
    ) const;

    /**
     * @brief Estimate occupied bandwidth
     * @param iq_samples Input IQ data
     * @param power_threshold_db Power threshold below peak (default: -20 dB)
     * @return Bandwidth in Hz
     * 
     * Method: Compute PSD, find bandwidth containing X% of power
     */
    double estimateBandwidth(
        const std::vector<std::complex<float>>& iq_samples,
        float power_threshold_db = -20.0f
    ) const;

    /**
     * @brief Estimate symbol rate (baud rate) for digital signals
     * @param iq_samples Input IQ data
     * @return Estimated baud rate in Hz (0 if analog or uncertain)
     * 
     * Method: Autocorrelation-based symbol timing estimation
     */
    float estimateBaudRate(const std::vector<std::complex<float>>& iq_samples) const;

    /**
     * @brief Perform complete DSP measurement suite
     * @param iq_samples Input IQ data
     * @return All DSP measurements
     */
    DSPMeasurements measureAll(const std::vector<std::complex<float>>& iq_samples) const;

    /**
     * @brief Update sample rate (when SDR configuration changes)
     * @param sample_rate_hz New sample rate in Hz
     */
    void setSampleRate(double sample_rate_hz) {
        sample_rate_hz_ = sample_rate_hz;
    }

    /**
     * @brief Update noise floor reference
     * @param noise_floor_dbm New noise floor in dBm
     */
    void setNoiseFloor(float noise_floor_dbm) {
        noise_floor_dbm_ = noise_floor_dbm;
    }

    /**
     * @brief Get current sample rate
     */
    double getSampleRate() const { return sample_rate_hz_; }

private:
    double sample_rate_hz_;    ///< Sampling rate (Hz)
    float noise_floor_dbm_;    ///< Reference noise floor (dBm)
    
    // Calibration constants
    static constexpr float RSSI_CALIBRATION_OFFSET = 0.0f;  ///< Hardware-specific calibration
    static constexpr float POWER_REFERENCE_DBM = 0.0f;      ///< Reference power level
    
    /**
     * @brief Compute average power of IQ samples
     * @param iq_samples Input IQ data
     * @return Average power (linear scale)
     */
    float computeAveragePower(const std::vector<std::complex<float>>& iq_samples) const;

    /**
     * @brief Convert linear power to dBm
     * @param power_linear Power in linear scale
     * @return Power in dBm
     */
    float linearToDBm(float power_linear) const;

    /**
     * @brief Compute power spectral density (simplified)
     * @param iq_samples Input IQ data
     * @return PSD vector (frequency domain power)
     * 
     * @note In production, use FFTW3 with Welch method for better accuracy
     */
    std::vector<float> computePSD(const std::vector<std::complex<float>>& iq_samples) const;

    /**
     * @brief Find peak frequency in PSD
     * @param psd Power spectral density
     * @return Frequency offset of peak (Hz)
     */
    double findPeakFrequency(const std::vector<float>& psd) const;

    /**
     * @brief Compute autocorrelation for symbol timing
     * @param iq_samples Input IQ data
     * @return Autocorrelation function
     */
    std::vector<float> computeAutocorrelation(
        const std::vector<std::complex<float>>& iq_samples
    ) const;
};

} // namespace CognitiveRF

#endif // DSP_ENGINE_HPP
