/**
 * @file GeolocationPipeline.cpp
 * @brief Implementation of geolocation pipeline
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "../include/GeolocationPipeline.hpp"

namespace CognitiveRF {
namespace Geolocation {

GeolocationPipeline::GeolocationPipeline(const PipelineConfig& config)
    : config_(config)
    , engine_(config.geolocation_config)
    , running_(false)
{
}

GeolocationPipeline::~GeolocationPipeline() {
    stop();
}

void GeolocationPipeline::start() {
    if (running_.load()) {
        return;  // Zaten çalışıyor
    }
    
    running_.store(true);
    processing_thread_ = std::thread(&GeolocationPipeline::processingLoop, this);
}

void GeolocationPipeline::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    input_cv_.notify_all();
    
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
}

bool GeolocationPipeline::submitMeasurements(const std::vector<AoAMeasurement>& measurements) {
    std::unique_lock<std::mutex> lock(input_mutex_);
    
    if (input_queue_.size() >= config_.input_queue_size) {
        return false;  // Queue dolu
    }
    
    input_queue_.push(measurements);
    lock.unlock();
    input_cv_.notify_one();
    
    return true;
}

bool GeolocationPipeline::getResult(GeolocationResult& result, uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(result_mutex_);
    
    // Wait for result
    bool available = result_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
        [this]() { return !result_queue_.empty(); });
    
    if (!available) {
        return false;  // Timeout
    }
    
    result = result_queue_.front();
    result_queue_.pop();
    
    return true;
}

void GeolocationPipeline::setResultCallback(ResultCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    result_callback_ = callback;
}

GeolocationStatistics GeolocationPipeline::getStatistics() const {
    return engine_.getStatistics();
}

void GeolocationPipeline::processingLoop() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(input_mutex_);
        
        // Wait for input
        input_cv_.wait(lock, [this]() {
            return !input_queue_.empty() || !running_.load();
        });
        
        if (!running_.load()) {
            break;
        }
        
        // Get measurements
        std::vector<AoAMeasurement> measurements = input_queue_.front();
        input_queue_.pop();
        lock.unlock();
        
        // Perform geolocation
        GeolocationResult result = engine_.performGeolocation(measurements);
        
        // Push result to output queue
        {
            std::lock_guard<std::mutex> result_lock(result_mutex_);
            if (result_queue_.size() < config_.result_queue_size) {
                result_queue_.push(result);
                result_cv_.notify_one();
            }
        }
        
        // Callback notification
        {
            std::lock_guard<std::mutex> callback_lock(callback_mutex_);
            if (result_callback_) {
                result_callback_(result);
            }
        }
    }
}

} // namespace Geolocation
} // namespace CognitiveRF
