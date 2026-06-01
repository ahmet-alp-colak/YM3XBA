/**
 * @file ParameterExtractionPipeline.cpp
 * @brief Implementation of main orchestrator pipeline
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "ParameterExtractionPipeline.hpp"
#include <chrono>
#include <cmath>

namespace CognitiveRF {

ParameterExtractionPipeline::ParameterExtractionPipeline(const PipelineConfig& config)
    : config_(config)
    , running_(false)
    , processed_count_(0)
    , error_count_(0)
{
    // Initialize processing modules
    kalman_tracker_ = std::make_unique<KalmanCFOTracker>();
    signal_corrector_ = std::make_unique<SignalCorrector>(config.sample_rate_hz);
    dsp_engine_ = std::make_unique<DSPEngine>(config.sample_rate_hz, config.noise_floor_dbm);
    protocol_analyzer_ = std::make_unique<ProtocolAnalyzer>();
    
    // Initialize AI classifier (may throw if model not found)
    try {
        modulation_classifier_ = std::make_unique<ModulationClassifier>(
            config.onnx_model_path,
            config.voting_window_size
        );
    } catch (const std::exception& e) {
        // Log error but continue - classifier will be null
        // In production, use proper logging framework
        modulation_classifier_ = nullptr;
    }
}

ParameterExtractionPipeline::~ParameterExtractionPipeline() {
    stop();
}

void ParameterExtractionPipeline::start() {
    if (running_) {
        return;  // Already running
    }
    
    running_ = true;
    processing_thread_ = std::make_unique<std::thread>(
        &ParameterExtractionPipeline::processingLoop,
        this
    );
}

void ParameterExtractionPipeline::stop() {
    if (!running_) {
        return;  // Not running
    }
    
    running_ = false;
    input_queue_.shutdown();
    output_queue_.shutdown();
    
    if (processing_thread_ && processing_thread_->joinable()) {
        processing_thread_->join();
    }
}

bool ParameterExtractionPipeline::submitIQData(const IQDataBuffer& iq_buffer) {
    if (!running_) {
        return false;
    }
    
    // Check queue size limit
    if (input_queue_.size() >= config_.max_queue_size) {
        return false;  // Queue full
    }
    
    input_queue_.push(iq_buffer);
    return true;
}

bool ParameterExtractionPipeline::getResult(DecodedSignal& result) {
    return output_queue_.try_pop(result);
}

bool ParameterExtractionPipeline::getResultBlocking(DecodedSignal& result, int timeout_ms) {
    // Implement timeout using try_pop with sleep
    auto start_time = std::chrono::steady_clock::now();
    
    while (running_) {
        if (output_queue_.try_pop(result)) {
            return true;
        }
        
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();
        
        if (elapsed >= timeout_ms) {
            return false;  // Timeout
        }
        
        // Sleep briefly to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return false;
}

void ParameterExtractionPipeline::processingLoop() {
    while (running_) {
        IQDataBuffer iq_buffer;
        
        // Block until data available or shutdown
        if (!input_queue_.pop(iq_buffer)) {
            break;  // Shutdown signal
        }
        
        try {
            // Process buffer through complete pipeline
            DecodedSignal result = processBuffer(iq_buffer);
            
            // Publish result
            output_queue_.push(result);
            
            // Call callback if registered
            if (result_callback_) {
                result_callback_(result);
            }
            
            processed_count_++;
            
        } catch (const std::exception& e) {
            // Log error and continue
            error_count_++;
        }
    }
}

DecodedSignal ParameterExtractionPipeline::processBuffer(const IQDataBuffer& iq_buffer) {
    DecodedSignal result;
    
    // Set timestamp and frequency
    result.timestamp_us = iq_buffer.timestamp_us;
    result.carrier_frequency_hz = static_cast<double>(iq_buffer.center_freq_hz);
    
    // Step 1: CFO Estimation and Correction (if enabled)
    std::vector<std::complex<float>> corrected_iq = iq_buffer.iq_data;
    
    if (config_.enable_cfo_correction && kalman_tracker_) {
        // Estimate CFO from raw IQ
        double measured_cfo = estimateCFO(iq_buffer.iq_data);
        
        // Update Kalman filter
        kalman_tracker_->predict();
        kalman_tracker_->update(measured_cfo);
        
        // Get filtered CFO
        double filtered_cfo = kalman_tracker_->getCFO();
        
        // Apply correction
        signal_corrector_->correctFrequency(
            iq_buffer.iq_data,
            filtered_cfo,
            corrected_iq
        );
        
        // Update carrier frequency with CFO correction
        result.carrier_frequency_hz += filtered_cfo;
    }
    
    // Step 2: DSP Measurements
    if (dsp_engine_) {
        DSPMeasurements dsp_measurements = dsp_engine_->measureAll(corrected_iq);
        
        result.rssi_dbm = dsp_measurements.rssi_dbm;
        result.snr_db = dsp_measurements.snr_db;
        result.bandwidth_hz = dsp_measurements.bandwidth_hz;
        result.baud_rate_hz = dsp_measurements.baud_rate_hz;
    }
    
    // Step 3: Modulation Classification (AI)
    if (modulation_classifier_ && modulation_classifier_->isModelLoaded()) {
        auto [modulation, confidence] = modulation_classifier_->classify(
            corrected_iq,
            config_.enable_temporal_voting
        );
        
        result.modulation = modulation;
        result.confidence = confidence;
    } else {
        result.modulation = ModulationType::UNKNOWN;
        result.confidence = 0.0f;
    }
    
    // Step 4: Protocol Analysis
    if (protocol_analyzer_) {
        ProtocolAnalysisInput protocol_input;
        protocol_input.modulation = result.modulation;
        protocol_input.signal_class = result.signal_class;  // Should be set by temporal state engine
        protocol_input.bandwidth_hz = result.bandwidth_hz;
        protocol_input.carrier_frequency_hz = result.carrier_frequency_hz;
        protocol_input.rssi_dbm = result.rssi_dbm;
        
        // Update frequency tracking for FHSS detection
        protocol_analyzer_->updateFrequencyTracking(
            result.carrier_frequency_hz,
            result.rssi_dbm
        );
        
        ProtocolAnalysisResult protocol_result = protocol_analyzer_->analyze(protocol_input);
        
        result.protocol = protocol_result.protocol;
        result.multiplexing = protocol_result.multiplexing;
        result.fhss_suspected = protocol_result.fhss_suspected;
        result.jamming_suspected = protocol_result.jamming_suspected;
    }
    
    return result;
}

double ParameterExtractionPipeline::estimateCFO(
    const std::vector<std::complex<float>>& iq_data
) {
    // Simple CFO estimation using phase difference method
    // In production, use more sophisticated methods (Costas loop, FFT peak, etc.)
    
    if (iq_data.size() < 2) {
        return 0.0;
    }
    
    // Compute average phase difference between consecutive samples
    double phase_sum = 0.0;
    size_t count = 0;
    
    for (size_t i = 1; i < std::min(iq_data.size(), size_t(1024)); ++i) {
        // Phase difference = arg(sample[i] * conj(sample[i-1]))
        std::complex<float> product = iq_data[i] * std::conj(iq_data[i-1]);
        double phase_diff = std::arg(product);
        
        phase_sum += phase_diff;
        count++;
    }
    
    if (count == 0) {
        return 0.0;
    }
    
    // Average phase difference per sample
    double avg_phase_diff = phase_sum / static_cast<double>(count);
    
    // Convert to frequency: f = phase_diff * sample_rate / (2π)
    double cfo_hz = avg_phase_diff * config_.sample_rate_hz / (2.0 * M_PI);
    
    return cfo_hz;
}

} // namespace CognitiveRF
