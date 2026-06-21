/**
 * @file SignalDetectionEngine.hpp
 * @brief Main signal detection engine integrating all processing modules
 * @details Orchestrates PSD, feature extraction, GMM classification, and temporal filtering
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#ifndef SIGNAL_DETECTION_ENGINE_HPP
#define SIGNAL_DETECTION_ENGINE_HPP

#include "DataStructures.hpp"
#include "WelchPSDProcessor.hpp"
#include "FeatureExtractor.hpp"
#include "GMMClassifier.hpp"
#include "TemporalStateEngine.hpp"
#include <memory>
#include <vector>
#include <functional>

namespace CognitiveRF {
namespace SignalDetection {

/**
 * @class SignalDetectionEngine
 * @brief Core signal detection engine
 * 
 * @details Processing pipeline:
 * 1. Welch PSD computation (WelchPSDProcessor)
 * 2. Feature extraction (FeatureExtractor)
 * 3. GMM classification (GMMClassifier)
 * 4. Temporal filtering (TemporalStateEngine)
 * 5. Signal decision
 * 
 * This is a synchronous, single-threaded processing engine.
 * For multi-threaded operation, use SignalDetectionPipeline wrapper.
 */
class SignalDetectionEngine {
public:
    /**
     * @brief Constructor
     * @param config Engine configuration
     */
    explicit SignalDetectionEngine(const DetectionConfig& config = DetectionConfig());
    
    /**
     * @brief Destructor
     */
    ~SignalDetectionEngine() = default;
    
    /**
     * @brief Process IQ samples for signal detection
     * @param input Input data with IQ samples and metadata
     * @param output Output detection result
     * @return true if processing succeeded, false otherwise
     */
    bool processSignal(const DetectionInput& input, DetectionOutput& output);
    
    /**
     * @brief Train GMM classifier with accumulated features
     * @param training_samples Training feature vectors
     * @return true if training succeeded, false otherwise
     */
    bool trainClassifier(const std::vector<FeatureVector>& training_samples);
    
    /**
     * @brief Check if classifier is trained
     * @return true if trained, false otherwise
     */
    bool isClassifierTrained() const;
    
    /**
     * @brief Get current GMM parameters
     * @return GMM parameters
     */
    const GMMParameters& getGMMParameters() const;
    
    /**
     * @brief Get temporal state for a frequency
     * @param frequency_hz Frequency in Hz
     * @return Pointer to temporal state, or nullptr if not found
     */
    const TemporalState* getTemporalState(uint64_t frequency_hz) const;
    
    /**
     * @brief Cleanup old temporal states
     * @param current_timestamp_ms Current timestamp in milliseconds
     * @return Number of states removed
     */
    size_t cleanupOldStates(uint64_t current_timestamp_ms);
    
    /**
     * @brief Get configuration
     * @return Current configuration
     */
    const DetectionConfig& getConfig() const { return config_; }
    
    /**
     * @brief Update configuration (re-initializes modules if needed)
     * @param config New configuration
     */
    void updateConfig(const DetectionConfig& config);
    
    /**
     * @brief Set result callback for asynchronous notification
     * @param callback Callback function
     */
    void setResultCallback(std::function<void(const DetectionOutput&)> callback) {
        result_callback_ = callback;
    }

private:
    /**
     * @brief Initialize processing modules
     */
    void initializeModules();
    
    /**
     * @brief Make final signal decision based on classification and temporal state
     * @param classification_result GMM classification result
     * @param temporal_likelihood Temporal filtered likelihood
     * @return Final signal class
     */
    SignalClass makeFinalDecision(
        const ClassificationResult& classification_result,
        double temporal_likelihood
    );
    
    DetectionConfig config_;                                 ///< Engine configuration
    
    // Processing modules
    std::unique_ptr<WelchPSDProcessor> psd_processor_;       ///< PSD processor
    std::unique_ptr<FeatureExtractor> feature_extractor_;    ///< Feature extractor
    std::unique_ptr<GMMClassifier> gmm_classifier_;          ///< GMM classifier
    std::unique_ptr<TemporalStateEngine> temporal_engine_;   ///< Temporal state engine
    
    // Result callback
    std::function<void(const DetectionOutput&)> result_callback_;
};

} // namespace SignalDetection
} // namespace CognitiveRF

#endif // SIGNAL_DETECTION_ENGINE_HPP
