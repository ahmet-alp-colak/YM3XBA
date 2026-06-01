/**
 * @file ModulationClassifier.cpp
 * @brief Implementation of AI modulation classifier
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "ModulationClassifier.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <map>

namespace CognitiveRF {

ModulationClassifier::ModulationClassifier(
    const std::string& model_path,
    size_t voting_window_size,
    const IQPreprocessingConfig& config
)
    : memory_info_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    , preproc_config_(config)
    , voting_window_size_(voting_window_size)
    , model_loaded_(false)
{
    try {
        // Initialize ONNX Runtime environment
        ort_env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ModulationClassifier");
        
        // Configure session options
        session_options_ = std::make_unique<Ort::SessionOptions>();
        session_options_->SetIntraOpNumThreads(4);  // Parallel execution within ops
        session_options_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
        // Load ONNX model
        ort_session_ = std::make_unique<Ort::Session>(*ort_env_, model_path.c_str(), *session_options_);
        
        // Get input/output names and shapes
        Ort::AllocatorWithDefaultOptions allocator;
        
        // Input metadata (expected: [1, 2, 1024])
        input_names_.push_back(ort_session_->GetInputNameAllocated(0, allocator).get());
        auto input_type_info = ort_session_->GetInputTypeInfo(0);
        auto tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
        input_shape_ = tensor_info.GetShape();
        
        // Output metadata
        output_names_.push_back(ort_session_->GetOutputNameAllocated(0, allocator).get());
        
        model_loaded_ = true;
        
    } catch (const Ort::Exception& e) {
        throw std::runtime_error(std::string("Failed to load ONNX model: ") + e.what());
    }
}

std::pair<ModulationType, float> ModulationClassifier::classify(
    const std::vector<std::complex<float>>& iq_samples,
    bool use_voting
)
{
    if (iq_samples.size() < IQ_SAMPLE_SIZE) {
        throw std::invalid_argument("Insufficient IQ samples (need at least 1024)");
    }
    
    // Get raw probabilities
    auto probabilities = getClassProbabilities(iq_samples);
    
    // Find most likely class
    size_t predicted_class = argmax(probabilities);
    ModulationType predicted_modulation = indexToModulationType(predicted_class);
    
    if (use_voting) {
        return applyTemporalVoting(predicted_modulation);
    } else {
        // Return raw confidence (max probability)
        return {predicted_modulation, probabilities[predicted_class]};
    }
}

std::vector<float> ModulationClassifier::getClassProbabilities(
    const std::vector<std::complex<float>>& iq_samples
)
{
    // Preprocess IQ data
    std::array<std::array<float, IQ_SAMPLE_SIZE>, NUM_CHANNELS> preprocessed;
    preprocessIQ(iq_samples, preprocessed);
    
    // Run inference
    return runInference(preprocessed);
}

void ModulationClassifier::preprocessIQ(
    const std::vector<std::complex<float>>& iq_samples,
    std::array<std::array<float, IQ_SAMPLE_SIZE>, NUM_CHANNELS>& output
)
{
    // Extract I and Q channels
    std::array<float, IQ_SAMPLE_SIZE> i_channel;
    std::array<float, IQ_SAMPLE_SIZE> q_channel;
    
    for (size_t n = 0; n < IQ_SAMPLE_SIZE; ++n) {
        i_channel[n] = iq_samples[n].real();
        q_channel[n] = iq_samples[n].imag();
    }
    
    // Step 1: DC Removal (remove mean)
    if (preproc_config_.enable_dc_removal) {
        float i_mean = computeMean(i_channel.data(), IQ_SAMPLE_SIZE);
        float q_mean = computeMean(q_channel.data(), IQ_SAMPLE_SIZE);
        
        for (size_t n = 0; n < IQ_SAMPLE_SIZE; ++n) {
            i_channel[n] -= i_mean;
            q_channel[n] -= q_mean;
        }
    }
    
    // Step 2: Z-Score Normalization
    if (preproc_config_.enable_normalization) {
        float i_mean = computeMean(i_channel.data(), IQ_SAMPLE_SIZE);
        float q_mean = computeMean(q_channel.data(), IQ_SAMPLE_SIZE);
        
        float i_std = computeStdDev(i_channel.data(), IQ_SAMPLE_SIZE, i_mean);
        float q_std = computeStdDev(q_channel.data(), IQ_SAMPLE_SIZE, q_mean);
        
        // Add epsilon for numerical stability
        i_std += preproc_config_.epsilon;
        q_std += preproc_config_.epsilon;
        
        for (size_t n = 0; n < IQ_SAMPLE_SIZE; ++n) {
            i_channel[n] = (i_channel[n] - i_mean) / i_std;
            q_channel[n] = (q_channel[n] - q_mean) / q_std;
        }
    }
    
    // Step 3: Optional Clipping
    if (preproc_config_.enable_clipping) {
        for (size_t n = 0; n < IQ_SAMPLE_SIZE; ++n) {
            i_channel[n] = std::clamp(i_channel[n], preproc_config_.clip_min, preproc_config_.clip_max);
            q_channel[n] = std::clamp(q_channel[n], preproc_config_.clip_min, preproc_config_.clip_max);
        }
    }
    
    // Copy to output (channel-first format: [I_channel, Q_channel])
    output[0] = i_channel;
    output[1] = q_channel;
}

std::vector<float> ModulationClassifier::runInference(
    const std::array<std::array<float, IQ_SAMPLE_SIZE>, NUM_CHANNELS>& preprocessed_data
)
{
    // Flatten data for ONNX input: [1, 2, 1024]
    std::vector<float> input_tensor_values(1 * NUM_CHANNELS * IQ_SAMPLE_SIZE);
    
    for (size_t c = 0; c < NUM_CHANNELS; ++c) {
        std::copy(
            preprocessed_data[c].begin(),
            preprocessed_data[c].end(),
            input_tensor_values.begin() + c * IQ_SAMPLE_SIZE
        );
    }
    
    // Create input tensor
    std::vector<int64_t> input_shape = {1, static_cast<int64_t>(NUM_CHANNELS), static_cast<int64_t>(IQ_SAMPLE_SIZE)};
    auto input_tensor = Ort::Value::CreateTensor<float>(
        memory_info_,
        input_tensor_values.data(),
        input_tensor_values.size(),
        input_shape.data(),
        input_shape.size()
    );
    
    // Run inference
    auto output_tensors = ort_session_->Run(
        Ort::RunOptions{nullptr},
        input_names_.data(),
        &input_tensor,
        1,
        output_names_.data(),
        1
    );
    
    // Extract output probabilities
    float* output_data = output_tensors[0].GetTensorMutableData<float>();
    auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
    size_t num_classes = output_shape[1];
    
    return std::vector<float>(output_data, output_data + num_classes);
}

std::pair<ModulationType, float> ModulationClassifier::applyTemporalVoting(
    ModulationType current_prediction
)
{
    // Add current prediction to history
    voting_history_.push_back(current_prediction);
    
    // Maintain window size
    if (voting_history_.size() > voting_window_size_) {
        voting_history_.pop_front();
    }
    
    // Count votes for each modulation type
    std::map<ModulationType, size_t> vote_counts;
    for (const auto& mod : voting_history_) {
        vote_counts[mod]++;
    }
    
    // Find winner (most votes)
    auto winner = std::max_element(
        vote_counts.begin(),
        vote_counts.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; }
    );
    
    // Calculate confidence: votes / total_votes
    float confidence = static_cast<float>(winner->second) / static_cast<float>(voting_history_.size());
    
    return {winner->first, confidence};
}

ModulationType ModulationClassifier::indexToModulationType(size_t class_index) const {
    // Map neural network output index to ModulationType enum
    // This mapping should match your training labels
    static const std::array<ModulationType, 14> index_to_type = {
        ModulationType::AM,      // 0
        ModulationType::FM,      // 1
        ModulationType::BPSK,    // 2
        ModulationType::QPSK,    // 3
        ModulationType::OQPSK,   // 4
        ModulationType::PSK8,    // 5
        ModulationType::QAM16,   // 6
        ModulationType::QAM64,   // 7
        ModulationType::FSK2,    // 8
        ModulationType::FSK4,    // 9
        ModulationType::GMSK,    // 10
        ModulationType::OFDM,    // 11
        ModulationType::GFSK,    // 12
        ModulationType::MSK      // 13
    };
    
    if (class_index >= index_to_type.size()) {
        return ModulationType::UNKNOWN;
    }
    
    return index_to_type[class_index];
}

size_t ModulationClassifier::argmax(const std::vector<float>& probabilities) const {
    return std::distance(
        probabilities.begin(),
        std::max_element(probabilities.begin(), probabilities.end())
    );
}

float ModulationClassifier::computeMean(const float* data, size_t size) const {
    float sum = std::accumulate(data, data + size, 0.0f);
    return sum / static_cast<float>(size);
}

float ModulationClassifier::computeStdDev(const float* data, size_t size, float mean) const {
    float variance = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        float diff = data[i] - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(size);
    return std::sqrt(variance);
}

} // namespace CognitiveRF
