/**
 * @file SignalDetectionEngine.cpp
 * @brief Implementation of signal detection engine
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#include "SignalDetectionEngine.hpp"
#include <chrono>

namespace CognitiveRF {
namespace SignalDetection {

SignalDetectionEngine::SignalDetectionEngine(const DetectionConfig& config)
    : config_(config)
{
    initializeModules();
}

void SignalDetectionEngine::initializeModules() {
    // Initialize Welch PSD processor
    psd_processor_ = std::make_unique<WelchPSDProcessor>(
        config_.fft_size,
        config_.overlap_size
    );
    
    // Initialize feature extractor
    feature_extractor_ = std::make_unique<FeatureExtractor>();
    
    // Initialize GMM classifier
    gmm_classifier_ = std::make_unique<GMMClassifier>(
        config_.gmm_max_iterations,
        config_.gmm_convergence_threshold
    );
    
    // Initialize temporal state engine
    temporal_engine_ = std::make_unique<TemporalStateEngine>(
        config_.temporal_history_size,
        config_.state_cleanup_threshold_sec
    );
}

bool SignalDetectionEngine::processSignal(
    const DetectionInput& input,
    DetectionOutput& output)
{
    // Validate input
    if (input.iq_samples.empty()) {
        return false;
    }
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    // Step 1: Compute Welch PSD
    std::vector<double> psd_linear;
    std::vector<double> sub_block_variances;
    
    if (!psd_processor_->computeWelchPSD(input.iq_samples, psd_linear, sub_block_variances)) {
        return false;
    }
    
    // Step 2: Extract features
    FeatureVector features = feature_extractor_->extractFeatures(psd_linear, sub_block_variances);
    
    // Step 3: GMM classification
    ClassificationResult classification;
    if (gmm_classifier_->isTrained()) {
        classification = gmm_classifier_->classify(features);
    } else {
        // If not trained, use simple threshold on mean power
        classification.likelihood = features.mean_power;
        classification.signal_class = (features.mean_power > config_.default_threshold) 
            ? SignalClass::SIGNAL 
            : SignalClass::NOISE;
    }
    
    // Step 4: Update temporal state
    temporal_engine_->updateState(
        input.center_frequency_hz,
        classification.likelihood,
        timestamp_ms
    );
    
    // Step 5: Apply temporal filtering
    double filtered_likelihood = temporal_engine_->applyTemporalFilter(
        input.center_frequency_hz,
        classification.likelihood
    );
    
    // Step 6: Make final decision
    SignalClass final_class = makeFinalDecision(classification, filtered_likelihood);
    
    // Step 7: Get adaptive threshold
    double adaptive_threshold = temporal_engine_->getAdaptiveThreshold(
        input.center_frequency_hz,
        config_.detection_n_sigma
    );
    
    // Populate output
    output.center_frequency_hz = input.center_frequency_hz;
    output.sample_rate_hz = input.sample_rate_hz;
    output.timestamp_ms = timestamp_ms;
    output.signal_class = final_class;
    output.raw_likelihood = classification.likelihood;
    output.filtered_likelihood = filtered_likelihood;
    output.adaptive_threshold = adaptive_threshold;
    output.features = features;
    output.is_classifier_trained = gmm_classifier_->isTrained();
    
    // Invoke callback if registered
    if (result_callback_) {
        result_callback_(output);
    }
    
    return true;
}

bool SignalDetectionEngine::trainClassifier(const std::vector<FeatureVector>& training_samples) {
    return gmm_classifier_->train(training_samples);
}

bool SignalDetectionEngine::isClassifierTrained() const {
    return gmm_classifier_->isTrained();
}

const GMMParameters& SignalDetectionEngine::getGMMParameters() const {
    return gmm_classifier_->getParameters();
}

const TemporalState* SignalDetectionEngine::getTemporalState(uint64_t frequency_hz) const {
    return temporal_engine_->getState(frequency_hz);
}

size_t SignalDetectionEngine::cleanupOldStates(uint64_t current_timestamp_ms) {
    return temporal_engine_->cleanupOldStates(current_timestamp_ms);
}

void SignalDetectionEngine::updateConfig(const DetectionConfig& config) {
    config_ = config;
    initializeModules();  // Re-initialize with new config
}

SignalClass SignalDetectionEngine::makeFinalDecision(
    const ClassificationResult& classification_result,
    double temporal_likelihood)
{
    // Use temporal filtered likelihood for decision
    if (config_.enable_temporal_filtering) {
        // Apply adaptive threshold if available
        double threshold = config_.default_threshold;
        
        if (gmm_classifier_->isTrained()) {
            // Use GMM-based decision
            return (temporal_likelihood > threshold) ? SignalClass::SIGNAL : SignalClass::NOISE;
        } else {
            // Use simple threshold
            return (temporal_likelihood > threshold) ? SignalClass::SIGNAL : SignalClass::NOISE;
        }
    } else {
        // Use raw classification result
        return classification_result.signal_class;
    }
}

} // namespace SignalDetection
} // namespace CognitiveRF
