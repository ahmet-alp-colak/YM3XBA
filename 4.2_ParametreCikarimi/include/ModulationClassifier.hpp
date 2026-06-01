/**
 * @file ModulationClassifier.hpp
 * @brief AI-based modulation classification using ONNX Runtime
 * @details Deep learning inference for automatic modulation recognition (AMR)
 * 
 * Architecture: RFConformerNet (Multi-branch: ComplexAwareStem + FFTBranch)
 * Input: float32[2][1024] (I/Q channels)
 * Output: Modulation type probabilities
 * 
 * Performance (from training):
 * - Overall accuracy: 63.12%
 * - High SNR (>0dB): 93.78%
 * - Low SNR (<=0dB): 21.34%
 * 
 * References:
 * - Section 4.2.2: Yapay Zekâ Destekli Modülasyon Sınıflandırma
 * - Pipeline Stage [9]: IQ Preprocessing
 * - Pipeline Stage [10]: AI Modulation Classifier
 * - Pipeline Stage [11]: Temporal Voting
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef MODULATION_CLASSIFIER_HPP
#define MODULATION_CLASSIFIER_HPP

#include "DataStructures.hpp"
#include <onnxruntime_cxx_api.h>
#include <complex>
#include <vector>
#include <deque>
#include <memory>
#include <string>
#include <array>

namespace CognitiveRF {

/**
 * @struct IQPreprocessingConfig
 * @brief Configuration for IQ preprocessing pipeline
 */
struct IQPreprocessingConfig {
    bool enable_dc_removal = true;        ///< Remove DC offset
    bool enable_normalization = true;     ///< Apply Z-score normalization
    bool enable_clipping = false;         ///< Clip to [-4, 4] range
    float epsilon = 1e-8f;                ///< Numerical stability constant
    float clip_min = -4.0f;               ///< Clipping minimum
    float clip_max = 4.0f;                ///< Clipping maximum
};

/**
 * @class ModulationClassifier
 * @brief ONNX-based modulation classifier with preprocessing and temporal voting
 * 
 * Pipeline:
 * 1. IQ Preprocessing (DC removal, Z-score normalization)
 * 2. ONNX Inference (RFConformerNet)
 * 3. Temporal Voting (majority vote over N inferences)
 */
class ModulationClassifier {
public:
    /**
     * @brief Constructor
     * @param model_path Path to ONNX model file (.onnx)
     * @param voting_window_size Number of inferences to keep for temporal voting
     * @param config Preprocessing configuration
     */
    explicit ModulationClassifier(
        const std::string& model_path,
        size_t voting_window_size = 5,
        const IQPreprocessingConfig& config = IQPreprocessingConfig{}
    );

    /**
     * @brief Classify modulation type from IQ samples
     * @param iq_samples Raw IQ data (must be at least 1024 samples)
     * @param use_voting Apply temporal voting (recommended for stability)
     * @return Detected modulation type and confidence
     */
    std::pair<ModulationType, float> classify(
        const std::vector<std::complex<float>>& iq_samples,
        bool use_voting = true
    );

    /**
     * @brief Get raw inference probabilities (without voting)
     * @param iq_samples Raw IQ data
     * @return Vector of probabilities for each modulation class
     */
    std::vector<float> getClassProbabilities(
        const std::vector<std::complex<float>>& iq_samples
    );

    /**
     * @brief Reset temporal voting history
     */
    void resetVotingHistory() {
        voting_history_.clear();
    }

    /**
     * @brief Update preprocessing configuration
     * @param config New preprocessing settings
     */
    void setPreprocessingConfig(const IQPreprocessingConfig& config) {
        preproc_config_ = config;
    }

    /**
     * @brief Get current voting window size
     */
    size_t getVotingWindowSize() const { return voting_window_size_; }

    /**
     * @brief Check if model is loaded successfully
     */
    bool isModelLoaded() const { return model_loaded_; }

private:
    // ONNX Runtime components
    std::unique_ptr<Ort::Env> ort_env_;
    std::unique_ptr<Ort::Session> ort_session_;
    std::unique_ptr<Ort::SessionOptions> session_options_;
    Ort::MemoryInfo memory_info_;
    
    // Model metadata
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;
    std::vector<int64_t> input_shape_;   ///< Expected: [1, 2, 1024]
    
    // Preprocessing
    IQPreprocessingConfig preproc_config_;
    
    // Temporal voting
    size_t voting_window_size_;
    std::deque<ModulationType> voting_history_;
    
    // State
    bool model_loaded_;
    
    // Constants
    static constexpr size_t IQ_SAMPLE_SIZE = 1024;
    static constexpr size_t NUM_CHANNELS = 2;  // I and Q
    
    /**
     * @brief Preprocess IQ data for neural network input
     * @param iq_samples Raw IQ samples
     * @param output Preprocessed data in [2][1024] format (I channel, Q channel)
     * 
     * Steps:
     * 1. DC Removal: I -= mean(I), Q -= mean(Q)
     * 2. Z-Score Normalization: I_norm = (I - mean) / (std + epsilon)
     * 3. Optional Clipping: clip(x, -4, 4)
     */
    void preprocessIQ(
        const std::vector<std::complex<float>>& iq_samples,
        std::array<std::array<float, IQ_SAMPLE_SIZE>, NUM_CHANNELS>& output
    );

    /**
     * @brief Run ONNX inference
     * @param preprocessed_data Input tensor data [2][1024]
     * @return Output probabilities for each modulation class
     */
    std::vector<float> runInference(
        const std::array<std::array<float, IQ_SAMPLE_SIZE>, NUM_CHANNELS>& preprocessed_data
    );

    /**
     * @brief Apply temporal voting mechanism
     * @param current_prediction Latest inference result
     * @return Most voted modulation type and confidence score
     * 
     * Confidence = (votes for winner) / (total votes)
     */
    std::pair<ModulationType, float> applyTemporalVoting(ModulationType current_prediction);

    /**
     * @brief Convert class index to ModulationType enum
     * @param class_index Output index from neural network
     * @return Corresponding modulation type
     */
    ModulationType indexToModulationType(size_t class_index) const;

    /**
     * @brief Find argmax of probability vector
     * @param probabilities Class probabilities
     * @return Index of maximum probability
     */
    size_t argmax(const std::vector<float>& probabilities) const;

    /**
     * @brief Compute mean of float array
     */
    float computeMean(const float* data, size_t size) const;

    /**
     * @brief Compute standard deviation of float array
     */
    float computeStdDev(const float* data, size_t size, float mean) const;
};

} // namespace CognitiveRF

#endif // MODULATION_CLASSIFIER_HPP
