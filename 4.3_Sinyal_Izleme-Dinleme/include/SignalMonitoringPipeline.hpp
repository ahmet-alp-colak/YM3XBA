/**
 * @file SignalMonitoringPipeline.hpp
 * @brief Main orchestrator for signal monitoring and listening
 * @details Coordinates tracking, demodulation, and payload extraction
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme
 * 
 * Reference: pipeline.txt Sections [7, 14-17, 22]
 * Reference: 4.3_SinyalDinleme-Izleme.txt
 * 
 * Threading Architecture:
 * - Thread-7 (Demodulation): Liquid-DSP + Protocol Analysis
 * - Asynchronous processing with thread-safe queues
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef SIGNAL_MONITORING_PIPELINE_HPP
#define SIGNAL_MONITORING_PIPELINE_HPP

#include "DataStructures.hpp"
#include "SignalTracker.hpp"
#include "AnalogDemodulator.hpp"
#include "DigitalDemodulator.hpp"
#include "PayloadExtractor.hpp"

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <functional>

namespace CognitiveRF {
namespace SignalMonitoring {

/**
 * @class SignalMonitoringPipeline
 * @brief Main pipeline for signal monitoring and demodulation
 * 
 * Features:
 * - Asynchronous signal processing
 * - Thread-safe input/output queues
 * - Automatic modulation-based demodulator selection
 * - Real-time callback notifications
 * - Performance statistics tracking
 */
class SignalMonitoringPipeline {
public:
    /**
     * @struct PipelineConfig
     * @brief Configuration for monitoring pipeline
     */
    struct PipelineConfig {
        // Queue sizes
        uint32_t input_queue_size = 100;
        uint32_t output_queue_size = 100;
        
        // Processing parameters
        uint32_t sample_rate_hz = 20000000;
        bool enable_tracking = true;
        bool enable_demodulation = true;
        bool enable_payload_extraction = true;
        
        // Component configurations
        SignalTracker::TrackingConfig tracking_config;
        AnalogDemodulator::DemodConfig analog_config;
        DigitalDemodulator::DemodConfig digital_config;
        PayloadExtractor::ExtractorConfig extractor_config;
    };

    /**
     * @brief Constructor
     * @param config Pipeline configuration
     */
    explicit SignalMonitoringPipeline(const PipelineConfig& config = PipelineConfig());
    
    /**
     * @brief Destructor - ensures graceful shutdown
     */
    ~SignalMonitoringPipeline();
    
    /**
     * @brief Start the processing pipeline
     * @return True if started successfully
     */
    bool start();
    
    /**
     * @brief Stop the processing pipeline
     */
    void stop();
    
    /**
     * @brief Check if pipeline is running
     * @return True if active
     */
    bool isRunning() const { return running_.load(); }
    
    /**
     * @brief Submit IQ data for processing
     * @param iq_buffer IQ data with metadata
     * @return True if accepted (queue not full)
     */
    bool submitIQData(const IQDataBuffer& iq_buffer);
    
    /**
     * @brief Get processed result (blocking with timeout)
     * @param result Output parameter for result
     * @param timeout_ms Timeout in milliseconds (0 = no wait)
     * @return True if result available
     */
    bool getResult(MonitoredSignal& result, uint32_t timeout_ms = 0);
    
    /**
     * @brief Set callback for real-time notifications
     * @param callback Function called when signal is processed
     */
    using ResultCallback = std::function<void(const MonitoredSignal&)>;
    void setResultCallback(ResultCallback callback);
    
    /**
     * @brief Get pipeline statistics
     */
    struct Statistics {
        uint64_t total_processed;
        uint64_t total_demodulated;
        uint64_t total_payloads_extracted;
        uint32_t queue_input_size;
        uint32_t queue_output_size;
        uint32_t active_tracked_signals;
        float avg_processing_time_ms;
    };
    
    Statistics getStatistics() const;
    
    /**
     * @brief Get signal tracker reference
     * @return Signal tracker instance
     */
    SignalTracker& getTracker() { return *tracker_; }
    const SignalTracker& getTracker() const { return *tracker_; }

private:
    /**
     * @brief Main processing thread function
     */
    void processingThread();
    
    /**
     * @brief Process single IQ buffer
     * @param iq_buffer Input IQ data
     * @return Monitored signal result
     */
    MonitoredSignal processSignal(const IQDataBuffer& iq_buffer);
    
    /**
     * @brief Select and apply appropriate demodulator
     * @param iq_buffer Input IQ data
     * @param modulation Detected modulation type
     * @return Demodulation result
     */
    std::optional<DemodulatedAudio> demodulateAnalog(
        const IQDataBuffer& iq_buffer,
        ModulationType modulation
    );
    
    std::optional<DemodulatedBitstream> demodulateDigital(
        const IQDataBuffer& iq_buffer,
        ModulationType modulation
    );
    
    /**
     * @brief Thread-safe queue template
     */
    template<typename T>
    class ThreadSafeQueue {
    public:
        explicit ThreadSafeQueue(size_t max_size = 100) : max_size_(max_size) {}
        
        bool push(const T& item) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.size() >= max_size_) return false;
            queue_.push(item);
            cv_.notify_one();
            return true;
        }
        
        bool pop(T& item, uint32_t timeout_ms = 0) {
            std::unique_lock<std::mutex> lock(mutex_);
            if (timeout_ms == 0) {
                if (queue_.empty()) return false;
            } else {
                if (!cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                    [this] { return !queue_.empty(); })) {
                    return false;
                }
            }
            item = queue_.front();
            queue_.pop();
            return true;
        }
        
        size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size();
        }
        
        void clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            std::queue<T> empty;
            std::swap(queue_, empty);
        }
        
    private:
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::queue<T> queue_;
        size_t max_size_;
    };

    // Configuration
    PipelineConfig config_;
    
    // Components
    std::unique_ptr<SignalTracker> tracker_;
    std::unique_ptr<AnalogDemodulator> analog_demod_;
    std::unique_ptr<DigitalDemodulator> digital_demod_;
    std::unique_ptr<PayloadExtractor> payload_extractor_;
    
    // Threading
    std::atomic<bool> running_;
    std::unique_ptr<std::thread> processing_thread_;
    
    // Queues
    ThreadSafeQueue<IQDataBuffer> input_queue_;
    ThreadSafeQueue<MonitoredSignal> output_queue_;
    
    // Callback
    mutable std::mutex callback_mutex_;
    ResultCallback result_callback_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    uint64_t total_processed_;
    uint64_t total_demodulated_;
    uint64_t total_payloads_extracted_;
    std::chrono::steady_clock::time_point last_stats_time_;
};

} // namespace SignalMonitoring
} // namespace CognitiveRF

#endif // SIGNAL_MONITORING_PIPELINE_HPP
