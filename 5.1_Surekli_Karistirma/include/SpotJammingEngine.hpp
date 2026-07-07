/**
 * @file SpotJammingEngine.hpp
 * @brief Core jamming signal generation engine
 * @details Implements mathematical synthesis of jamming waveforms including
 *          CW (Continuous Wave) and Gaussian noise for RF spectrum disruption.
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.1 Sürekli Karıştırma (Spot Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-06-29
 */

#ifndef COGNITIVE_RF_SPOT_JAMMING_ENGINE_HPP
#define COGNITIVE_RF_SPOT_JAMMING_ENGINE_HPP

#include "DataStructures.hpp"
#include <random>
#include <atomic>
#include <chrono>

namespace CognitiveRF {
namespace SpotJamming {

/**
 * @brief Spot Jamming Signal Generation Engine
 * 
 * Core processing engine responsible for generating jamming I/Q samples.
 * Supports multiple jamming types:
 * - Continuous Wave (CW): Pure sinusoid at target frequency
 * - Gaussian Noise: White noise covering target bandwidth
 * - Modulated Noise: Amplitude/frequency modulated noise
 * 
 * Thread-safety: This class is NOT thread-safe. Use from single thread
 * or wrap with mutex protection.
 */
class SpotJammingEngine {
public:
    /**
     * @brief Construct jamming engine with configuration
     * @param config Hardware and signal configuration
     */
    explicit SpotJammingEngine(const PipelineConfig& config);
    
    /**
     * @brief Destructor
     */
    ~SpotJammingEngine();
    
    // Disable copy (contains random generator state)
    SpotJammingEngine(const SpotJammingEngine&) = delete;
    SpotJammingEngine& operator=(const SpotJammingEngine&) = delete;
    
    /**
     * @brief Initialize engine with target parameters
     * @param target Target intelligence data
     * @return true if initialization successful
     */
    bool initialize(const TargetIntelligence& target);
    
    /**
     * @brief Generate one buffer of jamming I/Q samples
     * @param output Output buffer (pre-allocated)
     * @param num_samples Number of samples to generate
     * @return Number of samples actually generated
     */
    size_t generateSamples(IQBuffer& output, size_t num_samples);
    
    /**
     * @brief Update signal configuration (hot-swap)
     * @param new_config New signal configuration
     */
    void updateSignalConfig(const SignalConfig& new_config);
    
    /**
     * @brief Get current generation statistics
     * @return Statistics structure
     */
    struct Statistics {
        uint64_t total_samples_generated;
        double average_power_dbm;
        uint32_t buffer_underruns;
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
    
    // Signal generation state
    double phase_accumulator_ = 0.0;
    uint64_t sample_counter_ = 0;
    
    // Random number generation (for Gaussian noise)
    std::mt19937 rng_;
    std::normal_distribution<float> gaussian_dist_;
    
    // Statistics
    mutable Statistics stats_;
    
    // Private generation methods
    void generateContinuousWave(IQBuffer& output, size_t num_samples);
    void generateGaussianNoise(IQBuffer& output, size_t num_samples);
    void generateModulatedNoise(IQBuffer& output, size_t num_samples);
    
    // Helper methods
    float computeInstantaneousAmplitude(double time_s) const;
    void normalizeBuffer(IQBuffer& buffer) const;
};

} // namespace SpotJamming
} // namespace CognitiveRF

#endif // COGNITIVE_RF_SPOT_JAMMING_ENGINE_HPP
