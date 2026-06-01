/**
 * @file ParameterExtractionPipeline.hpp
 * @brief Main orchestrator for hybrid parameter extraction system
 * @details Coordinates all modules in thread-safe asynchronous pipeline
 * 
 * Architecture:
 * - Thread-safe input/output queues
 * - Asynchronous processing pipeline
 * - Module orchestration (Kalman, AI, Protocol, DSP)
 * - Result publishing for UI consumption
 * 
 * Threading Model:
 * - Input: Receives IQ data from SDR capture thread
 * - Processing: Runs parameter extraction pipeline
 * - Output: Publishes results to UI thread via thread-safe queue
 * 
 * References:
 * - Section 4.2: Parametre Çıkarımı
 * - Pipeline Stage [20]: Threading Mimarisi
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef PARAMETER_EXTRACTION_PIPELINE_HPP
#define PARAMETER_EXTRACTION_PIPELINE_HPP

#include "DataStructures.hpp"
#include "KalmanCFOTracker.hpp"
#include "SignalCorrector.hpp"
#include "ModulationClassifier.hpp"
#include "ProtocolAnalyzer.hpp"
#include "DSPEngine.hpp"

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <functional>

namespace CognitiveRF {

/**
 * @class ThreadSafeQueue
 * @brief Thread-safe queue for inter-thread communication
 * @tparam T Data type to store in queue
 * 
 * Features:
 * - Mutex-protected push/pop operations
 * - Condition variable for blocking wait
 * - Non-blocking try_pop for polling
 */
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;
    
    /**
     * @brief Push item to queue (thread-safe)
     * @param item Item to push
     */
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        cond_var_.notify_one();
    }
    
    /**
     * @brief Push item to queue (move semantics)
     * @param item Item to push
     */
    void push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        cond_var_.notify_one();
    }
    
    /**
     * @brief Pop item from queue (blocking)
     * @param item Output item
     * @return true if item retrieved, false if queue is shutting down
     */
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
        
        if (shutdown_ && queue_.empty()) {
            return false;
        }
        
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * @brief Try to pop item (non-blocking)
     * @param item Output item
     * @return true if item retrieved, false if queue empty
     */
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * @brief Get queue size
     * @return Number of items in queue
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    /**
     * @brief Check if queue is empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    /**
     * @brief Signal shutdown (unblocks waiting threads)
     */
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        cond_var_.notify_all();
    }
    
private:
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    std::queue<T> queue_;
    bool shutdown_ = false;
};

/**
 * @struct PipelineConfig
 * @brief Configuration for parameter extraction pipeline
 */
struct PipelineConfig {
    std::string onnx_model_path;           ///< Path to ONNX model
    double sample_rate_hz = 20e6;          ///< SDR sample rate
    float noise_floor_dbm = -100.0f;       ///< Reference noise floor
    size_t voting_window_size = 5;         ///< Temporal voting window
    size_t max_queue_size = 100;           ///< Maximum queue size
    bool enable_cfo_correction = true;     ///< Enable Kalman CFO tracking
    bool enable_temporal_voting = true;    ///< Enable voting for stability
};

/**
 * @class ParameterExtractionPipeline
 * @brief Main orchestrator for hybrid AI+DSP parameter extraction
 * 
 * Pipeline Flow:
 * 1. Receive IQ data from input queue
 * 2. Apply Kalman-filtered CFO correction
 * 3. Run DSP measurements (RSSI, SNR, bandwidth)
 * 4. Classify modulation (AI inference)
 * 5. Analyze protocol (decision tree)
 * 6. Publish complete DecodedSignal to output queue
 * 
 * Thread Safety:
 * - Input/output queues are thread-safe
 * - Internal processing is single-threaded per pipeline instance
 * - Multiple pipeline instances can run in parallel for different signals
 */
class ParameterExtractionPipeline {
public:
    /**
     * @brief Constructor
     * @param config Pipeline configuration
     */
    explicit ParameterExtractionPipeline(const PipelineConfig& config);
    
    /**
     * @brief Destructor - stops processing thread
     */
    ~ParameterExtractionPipeline();
    
    /**
     * @brief Start processing thread
     */
    void start();
    
    /**
     * @brief Stop processing thread
     */
    void stop();
    
    /**
     * @brief Submit IQ data for processing
     * @param iq_buffer IQ data buffer
     * @return true if accepted, false if queue full
     */
    bool submitIQData(const IQDataBuffer& iq_buffer);
    
    /**
     * @brief Get processed result (non-blocking)
     * @param result Output decoded signal
     * @return true if result available, false if queue empty
     */
    bool getResult(DecodedSignal& result);
    
    /**
     * @brief Get processed result (blocking with timeout)
     * @param result Output decoded signal
     * @param timeout_ms Timeout in milliseconds
     * @return true if result retrieved, false if timeout
     */
    bool getResultBlocking(DecodedSignal& result, int timeout_ms = 1000);
    
    /**
     * @brief Check if pipeline is running
     */
    bool isRunning() const { return running_; }
    
    /**
     * @brief Get input queue size
     */
    size_t getInputQueueSize() const { return input_queue_.size(); }
    
    /**
     * @brief Get output queue size
     */
    size_t getOutputQueueSize() const { return output_queue_.size(); }
    
    /**
     * @brief Register callback for real-time result notification
     * @param callback Function to call when result is ready
     */
    void setResultCallback(std::function<void(const DecodedSignal&)> callback) {
        result_callback_ = callback;
    }

private:
    // Configuration
    PipelineConfig config_;
    
    // Processing modules
    std::unique_ptr<KalmanCFOTracker> kalman_tracker_;
    std::unique_ptr<SignalCorrector> signal_corrector_;
    std::unique_ptr<ModulationClassifier> modulation_classifier_;
    std::unique_ptr<ProtocolAnalyzer> protocol_analyzer_;
    std::unique_ptr<DSPEngine> dsp_engine_;
    
    // Thread-safe queues
    ThreadSafeQueue<IQDataBuffer> input_queue_;
    ThreadSafeQueue<DecodedSignal> output_queue_;
    
    // Threading
    std::unique_ptr<std::thread> processing_thread_;
    std::atomic<bool> running_;
    
    // Callback
    std::function<void(const DecodedSignal&)> result_callback_;
    
    // Statistics
    std::atomic<uint64_t> processed_count_;
    std::atomic<uint64_t> error_count_;
    
    /**
     * @brief Main processing loop (runs in separate thread)
     */
    void processingLoop();
    
    /**
     * @brief Process single IQ buffer through complete pipeline
     * @param iq_buffer Input IQ data
     * @return Decoded signal with all parameters
     */
    DecodedSignal processBuffer(const IQDataBuffer& iq_buffer);
    
    /**
     * @brief Estimate CFO from IQ data (for Kalman filter input)
     * @param iq_data IQ samples
     * @return Estimated frequency offset in Hz
     */
    double estimateCFO(const std::vector<std::complex<float>>& iq_data);
};

} // namespace CognitiveRF

#endif // PARAMETER_EXTRACTION_PIPELINE_HPP
