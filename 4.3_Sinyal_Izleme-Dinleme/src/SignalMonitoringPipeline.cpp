/**
 * @file SignalMonitoringPipeline.cpp
 * @brief Implementation of main signal monitoring pipeline orchestrator
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "SignalMonitoringPipeline.hpp"
#include <chrono>
#include <iostream>

namespace CognitiveRF {
namespace SignalMonitoring {

// ============================================================================
// Constructor
// ============================================================================

SignalMonitoringPipeline::SignalMonitoringPipeline(const PipelineConfig& config)
    : config_(config)
    , running_(false)
    , input_queue_(config.input_queue_size)
    , output_queue_(config.output_queue_size)
    , total_processed_(0)
    , total_demodulated_(0)
    , total_payloads_extracted_(0)
    , last_stats_time_(std::chrono::steady_clock::now())
{
    // Initialize components
    tracker_ = std::make_unique<SignalTracker>(config_.tracking_config);
    analog_demod_ = std::make_unique<AnalogDemodulator>(config_.analog_config);
    digital_demod_ = std::make_unique<DigitalDemodulator>(config_.digital_config);
    payload_extractor_ = std::make_unique<PayloadExtractor>(config_.extractor_config);
}

// ============================================================================
// Destructor
// ============================================================================

SignalMonitoringPipeline::~SignalMonitoringPipeline()
{
    stop();
}

// ============================================================================
// Start Pipeline
// ============================================================================

bool SignalMonitoringPipeline::start()
{
    if (running_.load()) {
        return false; // Already running
    }
    
    running_.store(true);
    
    // Start processing thread
    processing_thread_ = std::make_unique<std::thread>(
        &SignalMonitoringPipeline::processingThread, this
    );
    
    return true;
}

// ============================================================================
// Stop Pipeline
// ============================================================================

void SignalMonitoringPipeline::stop()
{
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Wait for processing thread to finish
    if (processing_thread_ && processing_thread_->joinable()) {
        processing_thread_->join();
    }
    
    // Clear queues
    input_queue_.clear();
    output_queue_.clear();
}

// ============================================================================
// Submit IQ Data
// ============================================================================

bool SignalMonitoringPipeline::submitIQData(const IQDataBuffer& iq_buffer)
{
    if (!running_.load()) {
        return false;
    }
    
    return input_queue_.push(iq_buffer);
}

// ============================================================================
// Get Result
// ============================================================================

bool SignalMonitoringPipeline::getResult(MonitoredSignal& result, uint32_t timeout_ms)
{
    return output_queue_.pop(result, timeout_ms);
}

// ============================================================================
// Set Result Callback
// ============================================================================

void SignalMonitoringPipeline::setResultCallback(ResultCallback callback)
{
    std::lock_guard<std::mutex> lock(callback_mutex_);
    result_callback_ = callback;
}

// ============================================================================
// Get Statistics
// ============================================================================

SignalMonitoringPipeline::Statistics SignalMonitoringPipeline::getStatistics() const
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    Statistics stats;
    stats.total_processed = total_processed_;
    stats.total_demodulated = total_demodulated_;
    stats.total_payloads_extracted = total_payloads_extracted_;
    stats.queue_input_size = static_cast<uint32_t>(input_queue_.size());
    stats.queue_output_size = static_cast<uint32_t>(output_queue_.size());
    stats.active_tracked_signals = static_cast<uint32_t>(tracker_->getActiveSignals().size());
    
    // Calculate average processing time
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_stats_time_
    ).count();
    
    if (elapsed > 0 && total_processed_ > 0) {
        stats.avg_processing_time_ms = static_cast<float>(elapsed) / total_processed_;
    } else {
        stats.avg_processing_time_ms = 0.0f;
    }
    
    return stats;
}

// ============================================================================
// Processing Thread (Main Loop)
// ============================================================================

void SignalMonitoringPipeline::processingThread()
{
    while (running_.load()) {
        IQDataBuffer iq_buffer;
        
        // Wait for input data (100ms timeout)
        if (!input_queue_.pop(iq_buffer, 100)) {
            continue;
        }
        
        // Process the signal
        auto start_time = std::chrono::steady_clock::now();
        MonitoredSignal result = processSignal(iq_buffer);
        auto end_time = std::chrono::steady_clock::now();
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            total_processed_++;
            
            if (result.audio || result.bitstream) {
                total_demodulated_++;
            }
            
            if (result.payload) {
                total_payloads_extracted_++;
            }
        }
        
        // Push result to output queue
        output_queue_.push(result);
        
        // Call callback if set
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (result_callback_) {
                result_callback_(result);
            }
        }
    }
}

// ============================================================================
// Process Single Signal
// ============================================================================

MonitoredSignal SignalMonitoringPipeline::processSignal(const IQDataBuffer& iq_buffer)
{
    MonitoredSignal result;
    result.carrier_frequency_hz = iq_buffer.center_freq_hz;
    result.bandwidth_hz = iq_buffer.bandwidth_hz;
    result.modulation = iq_buffer.detected_modulation;
    result.signal_class = iq_buffer.signal_class;
    result.snr_db = iq_buffer.snr_db;
    result.rssi_dbm = iq_buffer.rssi_dbm;
    
    // Step 1: Signal Tracking (if enabled)
    if (config_.enable_tracking && tracker_) {
        auto tracked = tracker_->updateTracking(
            iq_buffer.center_freq_hz,
            iq_buffer.bandwidth_hz,
            iq_buffer.rssi_dbm,
            iq_buffer.signal_class
        );
        
        if (tracked) {
            result.fhss_suspected = tracked->fhss_suspected;
            result.occupancy_ratio = tracked->occupancy_ratio;
            result.first_seen_timestamp = tracked->first_seen_timestamp;
            result.last_seen_timestamp = tracked->last_seen_timestamp;
        }
    }
    
    // Step 2: Demodulation (if enabled)
    if (config_.enable_demodulation && !iq_buffer.iq_data.empty()) {
        // Determine if analog or digital modulation
        bool is_analog = (iq_buffer.detected_modulation == ModulationType::AM ||
                         iq_buffer.detected_modulation == ModulationType::FM ||
                         iq_buffer.detected_modulation == ModulationType::AM_DSB ||
                         iq_buffer.detected_modulation == ModulationType::AM_USB ||
                         iq_buffer.detected_modulation == ModulationType::AM_LSB ||
                         iq_buffer.detected_modulation == ModulationType::FM_NARROW ||
                         iq_buffer.detected_modulation == ModulationType::FM_WIDE);
        
        if (is_analog) {
            // Analog demodulation
            auto audio_result = demodulateAnalog(iq_buffer, iq_buffer.detected_modulation);
            if (audio_result) {
                result.audio = std::make_shared<DemodulatedAudio>(*audio_result);
            }
        } else {
            // Digital demodulation
            auto bitstream_result = demodulateDigital(iq_buffer, iq_buffer.detected_modulation);
            if (bitstream_result) {
                result.bitstream = std::make_shared<DemodulatedBitstream>(*bitstream_result);
                
                // Step 3: Payload Extraction (if enabled and bitstream available)
                if (config_.enable_payload_extraction && payload_extractor_) {
                    auto payload_result = payload_extractor_->extractPayload(
                        *bitstream_result,
                        iq_buffer.protocol_hint
                    );
                    
                    if (payload_result) {
                        result.payload = std::make_shared<ExtractedPayload>(*payload_result);
                    }
                }
            }
        }
    }
    
    return result;
}

// ============================================================================
// Analog Demodulation
// ============================================================================

std::optional<DemodulatedAudio> SignalMonitoringPipeline::demodulateAnalog(
    const IQDataBuffer& iq_buffer,
    ModulationType modulation)
{
    if (!analog_demod_) {
        return std::nullopt;
    }
    
    try {
        DemodulatedAudio audio = analog_demod_->demodulate(
            iq_buffer.iq_data,
            modulation
        );
        
        if (!audio.pcm_samples.empty()) {
            return audio;
        }
    }
    catch (const std::exception& e) {
        // Log error (in production, use proper logging)
        std::cerr << "Analog demodulation error: " << e.what() << std::endl;
    }
    
    return std::nullopt;
}

// ============================================================================
// Digital Demodulation
// ============================================================================

std::optional<DemodulatedBitstream> SignalMonitoringPipeline::demodulateDigital(
    const IQDataBuffer& iq_buffer,
    ModulationType modulation)
{
    if (!digital_demod_) {
        return std::nullopt;
    }
    
    try {
        DemodulatedBitstream bitstream = digital_demod_->demodulate(
            iq_buffer.iq_data,
            modulation
        );
        
        if (!bitstream.bits.empty()) {
            return bitstream;
        }
    }
    catch (const std::exception& e) {
        // Log error (in production, use proper logging)
        std::cerr << "Digital demodulation error: " << e.what() << std::endl;
    }
    
    return std::nullopt;
}

} // namespace SignalMonitoring
} // namespace CognitiveRF
