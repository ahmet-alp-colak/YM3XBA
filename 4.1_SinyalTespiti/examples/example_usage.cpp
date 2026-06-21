/**
 * @file example_usage.cpp
 * @brief Example usage of Signal Detection module
 * @details Demonstrates both synchronous and asynchronous usage patterns
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#include "SignalDetectionEngine.hpp"
#include "SignalDetectionPipeline.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <cmath>

using namespace CognitiveRF::SignalDetection;

// Helper function to generate synthetic IQ samples
std::vector<IQSample> generateSyntheticSignal(
    size_t num_samples,
    double signal_amplitude = 1.0,
    double noise_amplitude = 0.1,
    double signal_frequency = 0.05)
{
    std::vector<IQSample> samples;
    samples.reserve(num_samples);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> noise(0.0, noise_amplitude);
    
    for (size_t i = 0; i < num_samples; i++) {
        // Signal component (complex sinusoid)
        double phase = 2.0 * PI * signal_frequency * i;
        double signal_i = signal_amplitude * std::cos(phase);
        double signal_q = signal_amplitude * std::sin(phase);
        
        // Add noise
        double noisy_i = signal_i + noise(gen);
        double noisy_q = signal_q + noise(gen);
        
        samples.emplace_back(noisy_i, noisy_q);
    }
    
    return samples;
}

// Helper function to generate synthetic noise
std::vector<IQSample> generateSyntheticNoise(
    size_t num_samples,
    double noise_amplitude = 0.1)
{
    std::vector<IQSample> samples;
    samples.reserve(num_samples);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> noise(0.0, noise_amplitude);
    
    for (size_t i = 0; i < num_samples; i++) {
        samples.emplace_back(noise(gen), noise(gen));
    }
    
    return samples;
}

// Print detection result
void printResult(const DetectionOutput& output) {
    std::cout << "\n========================================\n";
    std::cout << "Detection Result:\n";
    std::cout << "========================================\n";
    std::cout << "Frequency: " << output.center_frequency_hz / 1e6 << " MHz\n";
    std::cout << "Timestamp: " << output.timestamp_ms << " ms\n";
    std::cout << "Signal Class: " 
              << (output.signal_class == SignalClass::SIGNAL ? "SIGNAL" : "NOISE") << "\n";
    std::cout << "Raw Likelihood: " << std::fixed << std::setprecision(4) 
              << output.raw_likelihood << "\n";
    std::cout << "Filtered Likelihood: " << output.filtered_likelihood << "\n";
    std::cout << "Adaptive Threshold: " << output.adaptive_threshold << "\n";
    std::cout << "Classifier Trained: " << (output.is_classifier_trained ? "Yes" : "No") << "\n";
    
    std::cout << "\nFeatures:\n";
    std::cout << "  Max Power: " << output.features.max_power << "\n";
    std::cout << "  Mean Power: " << output.features.mean_power << "\n";
    std::cout << "  Variance: " << output.features.variance << "\n";
    std::cout << "  Skewness: " << output.features.skewness << "\n";
    std::cout << "  Kurtosis: " << output.features.kurtosis << "\n";
    std::cout << "  Energy Ratio: " << output.features.energy_ratio << "\n";
    std::cout << "  Peak-to-Avg: " << output.features.peak_to_avg_ratio << "\n";
    std::cout << "  Spectral Flatness: " << output.features.spectral_flatness << "\n";
    std::cout << "========================================\n";
}

// Example 1: Synchronous processing with SignalDetectionEngine
void example1_synchronous_processing() {
    std::cout << "\n\n";
    std::cout << "================================================\n";
    std::cout << "EXAMPLE 1: Synchronous Processing\n";
    std::cout << "================================================\n";
    
    // Create configuration
    DetectionConfig config;
    config.fft_size = 1024;
    config.overlap_size = 512;
    config.default_threshold = 0.5;
    config.enable_temporal_filtering = true;
    
    // Create detection engine
    SignalDetectionEngine engine(config);
    
    // Generate synthetic signal
    std::cout << "\n[1] Generating synthetic SIGNAL (1024 samples)...\n";
    auto signal_samples = generateSyntheticSignal(2048, 1.0, 0.1, 0.05);
    
    // Create input
    DetectionInput input;
    input.center_frequency_hz = 434000000;  // 434 MHz
    input.sample_rate_hz = 20000000;        // 20 MSPS
    input.iq_samples = signal_samples;
    
    // Process signal
    DetectionOutput output;
    std::cout << "[2] Processing signal...\n";
    if (engine.processSignal(input, output)) {
        printResult(output);
    } else {
        std::cout << "ERROR: Failed to process signal\n";
    }
    
    // Process noise
    std::cout << "\n[3] Generating synthetic NOISE (1024 samples)...\n";
    auto noise_samples = generateSyntheticNoise(2048, 0.1);
    input.iq_samples = noise_samples;
    
    std::cout << "[4] Processing noise...\n";
    if (engine.processSignal(input, output)) {
        printResult(output);
    }
}

// Example 2: GMM Training
void example2_gmm_training() {
    std::cout << "\n\n";
    std::cout << "================================================\n";
    std::cout << "EXAMPLE 2: GMM Classifier Training\n";
    std::cout << "================================================\n";
    
    // Create engine
    DetectionConfig config;
    SignalDetectionEngine engine(config);
    
    // Collect training features
    std::vector<FeatureVector> training_features;
    
    std::cout << "\n[1] Collecting training samples...\n";
    
    // Generate 50 signal samples
    for (int i = 0; i < 50; i++) {
        auto samples = generateSyntheticSignal(2048, 1.0, 0.1, 0.05);
        DetectionInput input;
        input.center_frequency_hz = 434000000;
        input.sample_rate_hz = 20000000;
        input.iq_samples = samples;
        
        DetectionOutput output;
        if (engine.processSignal(input, output)) {
            training_features.push_back(output.features);
        }
    }
    
    // Generate 50 noise samples
    for (int i = 0; i < 50; i++) {
        auto samples = generateSyntheticNoise(2048, 0.1);
        DetectionInput input;
        input.center_frequency_hz = 434000000;
        input.sample_rate_hz = 20000000;
        input.iq_samples = samples;
        
        DetectionOutput output;
        if (engine.processSignal(input, output)) {
            training_features.push_back(output.features);
        }
    }
    
    std::cout << "    Collected " << training_features.size() << " training samples\n";
    
    // Train GMM
    std::cout << "\n[2] Training GMM classifier...\n";
    if (engine.trainClassifier(training_features)) {
        std::cout << "    Training successful!\n";
        std::cout << "    Classifier is now trained.\n";
    } else {
        std::cout << "    Training failed!\n";
    }
    
    // Test with new data
    std::cout << "\n[3] Testing trained classifier...\n";
    auto test_signal = generateSyntheticSignal(2048, 1.0, 0.1, 0.05);
    DetectionInput input;
    input.center_frequency_hz = 434000000;
    input.sample_rate_hz = 20000000;
    input.iq_samples = test_signal;
    
    DetectionOutput output;
    if (engine.processSignal(input, output)) {
        printResult(output);
    }
}

// Example 3: Asynchronous Pipeline
void example3_asynchronous_pipeline() {
    std::cout << "\n\n";
    std::cout << "================================================\n";
    std::cout << "EXAMPLE 3: Asynchronous Pipeline\n";
    std::cout << "================================================\n";
    
    // Create configuration
    DetectionConfig config;
    config.fft_size = 1024;
    config.overlap_size = 512;
    
    // Create pipeline
    SignalDetectionPipeline pipeline(config, 100, 100);
    
    // Set result callback
    int result_count = 0;
    pipeline.setResultCallback([&result_count](const DetectionOutput& output) {
        std::cout << "\n[CALLBACK] Result received: "
                  << (output.signal_class == SignalClass::SIGNAL ? "SIGNAL" : "NOISE")
                  << " at " << output.center_frequency_hz / 1e6 << " MHz\n";
        result_count++;
    });
    
    // Start pipeline
    std::cout << "\n[1] Starting pipeline...\n";
    if (!pipeline.start()) {
        std::cout << "ERROR: Failed to start pipeline\n";
        return;
    }
    std::cout << "    Pipeline started successfully.\n";
    
    // Submit multiple samples
    std::cout << "\n[2] Submitting 10 samples for processing...\n";
    for (int i = 0; i < 10; i++) {
        DetectionInput input;
        input.center_frequency_hz = 434000000 + (i * 1000000);  // 434-443 MHz
        input.sample_rate_hz = 20000000;
        
        if (i % 2 == 0) {
            // Even: signal
            input.iq_samples = generateSyntheticSignal(2048, 1.0, 0.1, 0.05);
        } else {
            // Odd: noise
            input.iq_samples = generateSyntheticNoise(2048, 0.1);
        }
        
        if (pipeline.submitData(input)) {
            std::cout << "    Submitted sample " << (i + 1) << "\n";
        }
    }
    
    // Wait for processing
    std::cout << "\n[3] Waiting for results...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Get statistics
    auto stats = pipeline.getStatistics();
    std::cout << "\n[4] Pipeline Statistics:\n";
    std::cout << "    Total Processed: " << stats.total_processed << "\n";
    std::cout << "    Total Signals: " << stats.total_signals << "\n";
    std::cout << "    Total Noise: " << stats.total_noise << "\n";
    std::cout << "    Avg Processing Time: " << std::fixed << std::setprecision(2)
              << stats.avg_processing_time_ms << " ms\n";
    
    // Stop pipeline
    std::cout << "\n[5] Stopping pipeline...\n";
    pipeline.stop();
    std::cout << "    Pipeline stopped.\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Signal Detection Module - Example Usage\n";
    std::cout << "Module 4.1: Sinyal Tespiti\n";
    std::cout << "========================================\n";
    
    try {
        // Run examples
        example1_synchronous_processing();
        example2_gmm_training();
        example3_asynchronous_pipeline();
        
        std::cout << "\n\n";
        std::cout << "========================================\n";
        std::cout << "All examples completed successfully!\n";
        std::cout << "========================================\n";
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
