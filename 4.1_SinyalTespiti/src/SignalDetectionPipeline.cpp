/**
 * @file SignalDetectionPipeline.cpp
 * @brief Implementation of signal detection pipeline
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#include "SignalDetectionPipeline.hpp"
#include <chrono>

namespace CognitiveRF {
namespace SignalDetection {

SignalDetectionPipeline::SignalDetectionPipeline(
    const DetectionConfig& config,
    size_t input_queue_size,
    size_t output_queue_size)
    : config_(config)
    , input_queue_size_(input_queue_size)
    , output_queue_size_(output_queue_size)
{
    // Create detection engine
    engine_ = std::make_unique<SignalDetectionEngine>(config_);
}

SignalDetectionPipeline::~SignalDetectionPipeline() {
    stop();
}

bool SignalDetectionPipeline::start() {
    // Check if already running
    if (running_.load()) {
        return false;
    }
    
    // Set running flag
    running_.store(true);
    
    // Start processing thread
    processing_thread_ = std::thread(&SignalDetectionPipeline::processingLoop, this);
    
    return true;
}

void SignalDetectionPipeline::stop() {
    // Check if running
    if (!running_.load()) {
        return;
    }
    
    // Set running flag to false
    running_.store(false);
    
    // Shutdown queues to wake up processing thread
    input_queue_.shutdown();
    output_queue_.shutdown();
    
    // Wait for thread to finish
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
}

bool SignalDetectionPipeline::submitData(const DetectionInput& input) {
    // Check if pipeline is running
    if (!running_.load()) {
        return false;
    }
    
    // Check queue size limit
    if (input_queue_.size() >= input_queue_size_) {
        return false;  // Queue full
    }
    
    // Push to input queue
    return input_queue_.push(input);
}

bool SignalDetectionPipeline::getResult(DetectionOutput& output, int timeout_ms) {
    return output_queue_.pop(output, timeout_ms);
}

bool SignalDetectionPipeline::tryGetResult(DetectionOutput& output) {
    return output_queue_.try_pop(output);
}

void SignalDetectionPipeline::setResultCallback(
    std::function<void(const DetectionOutput&)> callback)
{
    result_callback_ = callback;
}

bool SignalDetectionPipeline::trainClassifier(
    const std::vector<FeatureVector>& training_samples)
{
    return engine_->trainClassifier(training_samples);
}

bool SignalDetectionPipeline::isClassifierTrained() const {
    return engine_->isClassifierTrained();
}

SignalDetectionPipeline::Statistics SignalDetectionPipeline::getStatistics() const {
    Statistics stats;
    stats.total_processed = total_processed_.load();
    stats.total_signals = total_signals_.load();
    stats.total_noise = total_noise_.load();
    
    if (stats.total_processed > 0) {
        stats.avg_processing_time_ms = 
            static_cast<double>(total_processing_time_ms_.load()) / stats.total_processed;
    }
    
    return stats;
}

void SignalDetectionPipeline::processingLoop() {
    DetectionInput input;
    DetectionOutput output;
    
    while (running_.load()) {
        // Wait for input data (blocking with 100ms timeout)
        if (!input_queue_.pop(input, 100)) {
            continue;  // Timeout or shutdown, check running flag
        }
        
        // Record start time
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Process signal
        bool success = engine_->processSignal(input, output);
        
        // Record end time
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        ).count();
        
        if (success) {
            // Update statistics
            total_processed_.fetch_add(1);
            total_processing_time_ms_.fetch_add(duration_ms);
            
            if (output.signal_class == SignalClass::SIGNAL) {
                total_signals_.fetch_add(1);
            } else {
                total_noise_.fetch_add(1);
            }
            
            // Push to output queue (with size limit check)
            if (output_queue_.size() < output_queue_size_) {
                output_queue_.push(output);
            }
            
            // Invoke callback if registered
            if (result_callback_) {
                result_callback_(output);
            }
        }
    }
}

} // namespace SignalDetection
} // namespace CognitiveRF
