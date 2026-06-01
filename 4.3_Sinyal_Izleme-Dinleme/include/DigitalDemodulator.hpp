/**
 * @file DigitalDemodulator.hpp
 * @brief Digital signal demodulation (PSK/FSK/OFDM)
 * @details Implements carrier/timing recovery and symbol slicing
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme
 * 
 * Reference: pipeline.txt Section [16] - Digital Demodulation
 * Reference: 4.3_SinyalDinleme-Izleme.txt Section 4.3.2 (Digital Demodulation)
 * 
 * Algorithms:
 * - Costas Loop: Carrier recovery for PSK
 * - Gardner Timing Recovery: Symbol timing synchronization
 * - Symbol Slicer: Hard decision decoding
 * 
 * Gardner Timing Error:
 * e[k] = y(kT - T/2) · [y(kT) - y(kT - T)]
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef DIGITAL_DEMODULATOR_HPP
#define DIGITAL_DEMODULATOR_HPP

#include "DataStructures.hpp"
#include <vector>
#include <complex>
#include <memory>

namespace CognitiveRF {
namespace SignalMonitoring {

/**
 * @class DigitalDemodulator
 * @brief Demodulates digital PSK/FSK/OFDM signals to bitstream
 * 
 * Features:
 * - BPSK/QPSK/OQPSK/8PSK demodulation
 * - 2FSK/4FSK/GMSK/GFSK/MSK demodulation
 * - OFDM symbol decoding
 * - Costas loop carrier recovery
 * - Gardner timing recovery
 * - Adaptive equalization
 */
class DigitalDemodulator {
public:
    /**
     * @struct DemodConfig
     * @brief Configuration for digital demodulation
     */
    struct DemodConfig {
        uint32_t sample_rate_hz = 20000000;
        uint32_t symbol_rate_hz = 9600;          // Default baud rate
        uint32_t samples_per_symbol = 8;
        
        // Costas loop parameters
        float costas_loop_bw = 0.01f;            // Normalized bandwidth
        float costas_damping = 0.707f;           // Damping factor (√2/2)
        
        // Gardner timing recovery
        float gardner_loop_bw = 0.01f;
        float gardner_damping = 0.707f;
        
        // Symbol decision
        float decision_threshold = 0.0f;
        
        // Sync detection
        uint32_t sync_threshold = 10;            // Consecutive good symbols
    };

    /**
     * @brief Constructor
     * @param config Demodulation configuration
     */
    explicit DigitalDemodulator(const DemodConfig& config = DemodConfig());
    
    /**
     * @brief Demodulate BPSK signal
     * @param iq_samples Input IQ samples
     * @return Demodulated bitstream
     */
    DemodulatedBitstream demodulateBPSK(const std::vector<std::complex<float>>& iq_samples);
    
    /**
     * @brief Demodulate QPSK signal
     * @param iq_samples Input IQ samples
     * @return Demodulated bitstream
     */
    DemodulatedBitstream demodulateQPSK(const std::vector<std::complex<float>>& iq_samples);
    
    /**
     * @brief Demodulate 8PSK signal
     * @param iq_samples Input IQ samples
     * @return Demodulated bitstream
     */
    DemodulatedBitstream demodulate8PSK(const std::vector<std::complex<float>>& iq_samples);
    
    /**
     * @brief Demodulate FSK signal (2FSK/4FSK)
     * @param iq_samples Input IQ samples
     * @param num_tones Number of FSK tones (2 or 4)
     * @return Demodulated bitstream
     */
    DemodulatedBitstream demodulateFSK(
        const std::vector<std::complex<float>>& iq_samples,
        uint32_t num_tones = 2
    );
    
    /**
     * @brief Demodulate GMSK signal
     * @param iq_samples Input IQ samples
     * @return Demodulated bitstream
     */
    DemodulatedBitstream demodulateGMSK(const std::vector<std::complex<float>>& iq_samples);
    
    /**
     * @brief Demodulate OFDM signal
     * @param iq_samples Input IQ samples
     * @return Demodulated bitstream
     */
    DemodulatedBitstream demodulateOFDM(const std::vector<std::complex<float>>& iq_samples);
    
    /**
     * @brief Generic demodulation dispatcher
     * @param iq_samples Input IQ samples
     * @param modulation Modulation type
     * @return Demodulated bitstream
     */
    DemodulatedBitstream demodulate(
        const std::vector<std::complex<float>>& iq_samples,
        ModulationType modulation
    );
    
    /**
     * @brief Check if carrier/timing sync is locked
     * @return True if synchronized
     */
    bool isSyncLocked() const { return sync_locked_; }
    
    /**
     * @brief Get current symbol timing error
     * @return Timing error estimate
     */
    float getTimingError() const { return timing_error_; }
    
    /**
     * @brief Reset internal state
     */
    void reset();

private:
    /**
     * @brief Apply Costas loop for carrier recovery
     * @param samples Input samples
     * @return Phase-corrected samples
     */
    std::vector<std::complex<float>> applyCostasLoop(
        const std::vector<std::complex<float>>& samples
    );
    
    /**
     * @brief Apply Gardner timing recovery
     * @param samples Input samples
     * @return Timing-synchronized symbols
     */
    std::vector<std::complex<float>> applyGardnerTiming(
        const std::vector<std::complex<float>>& samples
    );
    
    /**
     * @brief Slice symbols to bits (hard decision)
     * @param symbols Input symbols
     * @param constellation_size Number of constellation points
     * @return Bit sequence
     */
    std::vector<uint8_t> sliceSymbols(
        const std::vector<std::complex<float>>& symbols,
        uint32_t constellation_size
    );
    
    /**
     * @brief Estimate bit error rate
     * @param symbols Received symbols
     * @return Estimated BER [0.0, 1.0]
     */
    float estimateBER(const std::vector<std::complex<float>>& symbols) const;
    
    /**
     * @brief Detect synchronization
     * @param symbols Recent symbols
     * @return True if sync detected
     */
    bool detectSync(const std::vector<std::complex<float>>& symbols);

    // Configuration
    DemodConfig config_;
    
    // Costas loop state
    float costas_phase_;
    float costas_freq_;
    float costas_alpha_;
    float costas_beta_;
    
    // Gardner timing state
    float timing_phase_;
    float timing_freq_;
    float timing_alpha_;
    float timing_beta_;
    float timing_error_;
    
    // Symbol buffer for timing recovery
    std::vector<std::complex<float>> symbol_buffer_;
    
    // Sync state
    bool sync_locked_;
    uint32_t sync_counter_;
};

} // namespace SignalMonitoring
} // namespace CognitiveRF

#endif // DIGITAL_DEMODULATOR_HPP
