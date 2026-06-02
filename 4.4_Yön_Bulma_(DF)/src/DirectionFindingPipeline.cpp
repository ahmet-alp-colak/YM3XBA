/**
 * @file DirectionFindingPipeline.cpp
 * @brief Implementation of Direction Finding Pipeline
 * @details Thread-safe asynchronous DF processing
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "DirectionFindingPipeline.hpp"
#include <iostream>
#include <chrono>

namespace CognitiveRF {
namespace DirectionFinding {

DirectionFindingPipeline::DirectionFindingPipeline(const PipelineConfig& config)
    : config_(config)
    , is_running_(false)
{
    // Create DF engine
    df_engine_ = std::make_unique<DirectionFindingEngine>(
        config_.df_config,
        config_.antenna_config,
        config_.antenna_array
    );
}

DirectionFindingPipeline::~DirectionFindingPipeline() {
    stop();
}

bool DirectionFindingPipeline::start() {
    if (is_running_) {
        return false;  // Already running
    }

    is_running_ = true;

    // Start processing thread
    processing_thread_ = std::make_unique<std::thread>(
        &DirectionFindingPipeline::processingThreadLoop, this
    );

    return true;
}

void DirectionFindingPipeline::stop() {
    if (!is_running_) {
        return;
    }

    is_running_ = false;

    // Notify thread to wake up
    input_queue_cv_.notify_one();

    // Wait for thread to finish
    if (processing_thread_ && processing_thread_->joinable()) {
        processing_thread_->join();
    }
}

bool DirectionFindingPipeline::submitIQData(
    const std::vector<std::complex<float>>& iq_data,
    float center_freq_hz,
    float snr_db,
    float rssi_dbm,
    uint64_t tracking_id
) {
    if (!is_running_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(input_queue_mutex_);

    // Check if queue is full
    if (input_queue_.size() >= config_.max_queue_size) {
        return false;
    }

    IQInputData input;
    input.iq_data = iq_data;
    input.center_freq_hz = center_freq_hz;
    input.snr_db = snr_db;
    input.rssi_dbm = rssi_dbm;
    input.tracking_id = tracking_id;
    input.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();

    input_queue_.push(input);

    // Notify processing thread
    input_queue_cv_.notify_one();

    return true;
}

bool DirectionFindingPipeline::getResult(DirectionFindingResult& result, uint32_t timeout_ms) {
    if (!is_running_) {
        return false;
    }

    std::unique_lock<std::mutex> lock(output_queue_mutex_);

    // Wait for result with timeout
    if (timeout_ms == 0) {
        output_queue_cv_.wait(lock, [this] { return !output_queue_.empty(); });
    } else {
        auto duration = std::chrono::milliseconds(timeout_ms);
        if (!output_queue_cv_.wait_for(lock, duration, [this] { return !output_queue_.empty(); })) {
            return false;  // Timeout
        }
    }

    if (output_queue_.empty()) {
        return false;
    }

    result = output_queue_.front();
    output_queue_.pop();

    return true;
}

void DirectionFindingPipeline::setResultCallback(const ResultCallback& callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    result_callback_ = callback;
}

const DirectionFindingStatistics& DirectionFindingPipeline::getStatistics() const {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    return statistics_;
}

void DirectionFindingPipeline::resetStatistics() {
    {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        statistics_ = DirectionFindingStatistics();
    }

    if (df_engine_) {
        df_engine_->resetStatistics();
    }
}

bool DirectionFindingPipeline::isInputQueueFull() const {
    std::lock_guard<std::mutex> lock(input_queue_mutex_);
    return input_queue_.size() >= config_.max_queue_size;
}

bool DirectionFindingPipeline::hasResult() const {
    std::lock_guard<std::mutex> lock(output_queue_mutex_);
    return !output_queue_.empty();
}

uint32_t DirectionFindingPipeline::getInputQueueSize() const {
    std::lock_guard<std::mutex> lock(input_queue_mutex_);
    return input_queue_.size();
}

uint32_t DirectionFindingPipeline::getOutputQueueSize() const {
    std::lock_guard<std::mutex> lock(output_queue_mutex_);
    return output_queue_.size();
}

void DirectionFindingPipeline::updateConfig(const PipelineConfig& config) {
    config_ = config;
    if (df_engine_) {
        df_engine_->updateConfig(config_.df_config);
    }
}

void DirectionFindingPipeline::processingThreadLoop() {
    while (is_running_) {
        IQInputData input;

        {
            std::unique_lock<std::mutex> lock(input_queue_mutex_);

            // Wait for data or stop signal
            input_queue_cv_.wait(lock, [this] {
                return !input_queue_.empty() || !is_running_;
            });

            if (!is_running_) {
                break;
            }

            if (input_queue_.empty()) {
                continue;
            }

            input = input_queue_.front();
            input_queue_.pop();
        }

        // Perform DF processing
        if (!df_engine_) {
            continue;
        }

        DirectionFindingResult result = df_engine_->performDirectionFinding(
            input.iq_data,
            input.center_freq_hz,
            input.snr_db,
            input.rssi_dbm
        );

        result.timestamp_us = input.timestamp_us;
        result.tracking_id = input.tracking_id;

        auto end_time = std::chrono::steady_clock::now();
        (void)end_time;  // Unused currently, but kept for future metrics

        // Update statistics
        updateStatistics(result);

        // Add to output queue
        {
            std::lock_guard<std::mutex> lock(output_queue_mutex_);

            // Drop oldest result if queue is full
            if (output_queue_.size() >= config_.max_queue_size) {
                output_queue_.pop();
            }

            output_queue_.push(result);
        }

        // Notify getResult() call
        output_queue_cv_.notify_one();

        // Call callback if enabled
        if (config_.enable_callbacks) {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (result_callback_) {
                result_callback_(result);
            }
        }
    }
}

void DirectionFindingPipeline::updateStatistics(const DirectionFindingResult& result) {
    if (!config_.enable_statistics) {
        return;
    }

    std::lock_guard<std::mutex> lock(statistics_mutex_);

    statistics_.total_processed++;

    if (result.is_valid) {
        statistics_.valid_aoa_estimates++;

        // Update confidence
        float alpha = 0.1f;  // EMA factor
        statistics_.avg_aoa_confidence = alpha * result.aoa_confidence +
            (1.0f - alpha) * statistics_.avg_aoa_confidence;

        // Update error statistics
        statistics_.avg_aoa_error_degrees = alpha * result.aoa_rms_error_degrees +
            (1.0f - alpha) * statistics_.avg_aoa_error_degrees;

        statistics_.min_aoa_error_degrees = std::min(
            statistics_.min_aoa_error_degrees,
            result.aoa_rms_error_degrees
        );
        statistics_.max_aoa_error_degrees = std::max(
            statistics_.max_aoa_error_degrees,
            result.aoa_rms_error_degrees
        );
    }
}

} // namespace DirectionFinding
} // namespace CognitiveRF
