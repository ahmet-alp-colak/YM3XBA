/**
 * @file LookThroughEngine.hpp
 * @brief Core TDD-based look-through jamming engine
 * @details Implements energy detection, jamming signal generation, and
 *          adaptive sensing for time-division duplexing jamming operations.
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.2 Arabakışlı Karıştırma (Look-Through Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-07-12
 */

#ifndef COGNITIVE_RF_LOOK_THROUGH_ENGINE_HPP
#define COGNITIVE_RF_LOOK_THROUGH_ENGINE_HPP

#include "DataStructures.hpp"
#include <random>
#include <atomic>
#include <chrono>
#include <cmath>

namespace CognitiveRF {
namespace LookThroughJamming {

/**
 * @brief Look-Through Jamming Engine with TDD energy detection
 * 
 * Core processing engine responsible for:
 * 1. Energy detection during RX (sensing) phase
 * 2. Target activity decision based on adaptive thresholds
 * 3. Jamming I/Q signal generation during TX phase
 * 
 * Thread-safety: This class is NOT thread-safe. Use from single thread
 * or wrap with mutex protection.
 * 
 * Algorithm:
 * - RX Phase (10ms): Measure channel energy, compare to threshold
 * - Decision: If energy > threshold, target is active
 * - TX Phase (50-100ms): Generate and transmit jamming signal
 * - Repeat cycle until target goes inactive
 */
class LookThroughEngine {
public:
    /**
     * @brief Construct engine with configuration
     * @param config Pipeline configuration including TDD timing
     */
    explicit LookThroughEngine(const PipelineConfig& config);
    
    /**
     * @brief Destructor
     */
    ~LookThroughEngine();
    
    // Disable copy (contains random generator state)
    LookThroughEngine(const LookThroughEngine&) = delete;
    LookThroughEngine& operator=(const LookThroughEngine&) = delete;
    
    /**
     * @brief Initialize engine with target parameters
     * @param target Target intelligence data
     * @return true if initialization successful
     */
    bool initialize(const TargetIntelligence& target);
    
    /**
     * @brief Perform energy detection on received I/Q samples (RX phase)
     * @param iq_samples Received I/Q buffer from HackRF RX
     * @param result Output sensing result structure
     * @return true if sensing completed successfully
     * 
     * @details
     * Energy Detection Algorithm:
     * 1. Compute instantaneous power: P[n] = |I[n]|^2 + |Q[n]|^2
     * 2. Average power: P_avg = (1/N) * sum(P[n])
     * 3. Convert to dBm: P_dBm = 10*log10(P_avg) + calibration_offset
     * 4. Compare to threshold: detected = (P_dBm > noise_floor + threshold_db)
     */
    bool performEnergyDetection(const IQBuffer& iq_samples, 
                                 SensingResult& result);
    
    /**
     * @brief Generate jamming I/Q samples (TX phase)
     * @param output Output buffer (pre-allocated)
     * @param num_samples Number of samples to generate
     * @return Number of samples actually generated
     */
    size_t generateJammingSamples(IQBuffer& output, size_t num_samples);
    
    /**
     * @brief Update adaptive noise floor estimate
     * @param measured_power_dbm Latest power measurement from sensing
     * 
     * @details
     * Uses exponential moving average (EMA):
     * noise_floor_new = alpha * measured + (1-alpha) * noise_floor_old
     * where alpha = 0.1 (slow adaptation)
     */
    void updateNoiseFloor(float measured_power_dbm);
    
    /**
     * @brief Get current detection threshold
     * @return Threshold in dBm
     */
    float getDetectionThreshold() const;
    
    /**
     * @brief Update signal configuration (hot-swap)
     * @param new_config New signal configuration
     */
    void updateSignalConfig(const SignalConfig& new_config);
    
    /**
     * @brief Update TDD timing configuration
     * @param new_tdd New TDD timing parameters
     */
    void updateTDDTiming(const TDDTimingConfig& new_tdd);
    
    /**
     * @brief Get current generation statistics
     */
    struct Statistics {
        uint64_t total_sensing_cycles = 0;
        uint64_t total_detections = 0;
        uint64_t total_false_alarms = 0;
        
        uint64_t total_rx_samples = 0;
        uint64_t total_tx_samples = 0;
        
        float current_noise_floor_dbm = -100.0f;
        float average_detection_power_dbm = -100.0f;
        
        double detection_rate = 0.0;  // Ratio of detections to sensing cycles
    };
    Statistics getStatistics() const;
    
    /**
     * @brief Reset engine state
     */
    void reset();

private:
    // Configuration
    PipelineConfig config_;
    TargetIntelligence current_target_;
    
    // Adaptive threshold tracking
    float adaptive_noise_floor_dbm_ = -100.0f;
    static constexpr float NOISE_FLOOR_ALPHA = 0.1f;  // EMA coefficient
    
    // Signal generation state
    double phase_accumulator_ = 0.0;
    uint64_t tx_sample_counter_ = 0;
    uint64_t rx_sample_counter_ = 0;
    
    // Random number generation (for Gaussian noise)
    std::mt19937 rng_;
    std::normal_distribution<float> gaussian_dist_;
    
    // Statistics
    mutable Statistics stats_;
    
    // Private generation methods (same as Spot Jamming)
    void generateContinuousWave(IQBuffer& output, size_t num_samples);
    void generateGaussianNoise(IQBuffer& output, size_t num_samples);
    void generateModulatedNoise(IQBuffer& output, size_t num_samples);
    
    // Energy detection helpers
    float computeAveragePower(const IQBuffer& samples) const;
    float powerToDbm(float power_linear) const;
    void normalizeBuffer(IQBuffer& buffer) const;
    float computeInstantaneousAmplitude(double time_s) const;
};

} // namespace LookThroughJamming
} // namespace CognitiveRF

#endif // COGNITIVE_RF_LOOK_THROUGH_ENGINE_HPP