/**
 * @file SignalDetectionPipeline.hpp
 * @brief Thread-safe asynchronous pipeline wrapper for signal detection
 * @details Multi-threaded pipeline with input/output queues
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.1 Sinyal Tespiti (Signal Detection)
 * 
 * @author CognitiveRF Development Team
 * @date 2026-06-21
 */

#ifndef SIGNAL_DETECTION_PIPELINE_HPP
#define SIGNAL_DETECTION_PIPELINE_HPP

#include "DataStructures.hpp"
#include "SignalDetectionEngine.hpp"
#include "ThreadSafeQueue.hpp"
#include <thread>
#include <atomic>
#include <memory>
#include <functional>

namespace CognitiveRF {
namespace SignalDetection {

/**
 * @class SignalDetectionPipeline
 * @brief Asynchronous multi-threaded signal detection pipeline
 * 
 * @details Architecture:
 * - Input Queue: Thread-safe queue for incoming IQ data
 * - Processing Thread: Runs SignalDetectionEngine
 * - Output Queue: Thread-safe queue for detection results
 * - Callback: Optional result notification
 * 
 * Usage pattern:
 * 1. Create pipeline with configuration
 * 2. Start processing thread
 * 3. Submit IQ data via submitData()
 * 4. Retrieve results via getResult() or callback
 * 5. Stop pipeline gracefully
 */
class SignalDetectionPipeline {
public:
    /**
     * @brief Constructor
     * @param config Detection configuration
     * @param input_queue_size Maximum input queue size (default: 100)
     * @param output_queue_size Maximum output queue size (default: 100)
     */
    explicit SignalDetectionPipeline(
        const DetectionConfig& config = DetectionConfig(),
        size_t input_queue_size = 100,
        size_t output_queue_size = 100
    );
    
    /**
     * @brief Destructor - automatically stops pipeline
     */
    ~SignalDetectionPipeline();
    
    /**
     * @brief Start processing thread
     * @return true if started successfully, false otherwise
     */
    bool start();
    
    /**
     * @brief Stop processing thread gracefully
     */
    void stop();
    
    /**
     * @brief Check if pipeline is running
     * @return true if running, false otherwise
     */
    bool isRunning() const { return running_.load(); }
    
    /**
     * @brief Submit data for processing (non-blocking)
     * @param input Input data with IQ samples
     * @return true if submitted, false if queue is full or pipeline stopped
     */
    bool submitData(const DetectionInput& input);
    
    /**
     * @brief Get processing result (blocking with timeout)
     * @param output Output detection result
     * @param timeout_ms Timeout in milliseconds (0 = wait forever)
     * @return true if result retrieved, false if timeout or no data
     */
    bool getResult(DetectionOutput& output, int timeout_ms = 100);
    
    /**
     * @brief Try to get result without blocking
     * @param output Output detection result
     * @return true if result retrieved, false if no data
     */
    bool tryGetResult(DetectionOutput& output);
    
    /**
     * @brief Set result callback for asynchronous notification
     * @param callback Callback function
     */
    void setResultCallback(std::function<void(const DetectionOutput&)> callback);
    
    /**
     * @brief Train GMM classifier
     * @param training_samples Training feature vectors
     * @return true if training succeeded, false otherwise
     */
    bool trainClassifier(const std::vector<FeatureVector>& training_samples);
    
    /**
     * @brief Check if classifier is trained
     * @return true if trained, false otherwise
     */
    bool isClassifierTrained() const;
    
    /**
     * @brief Get input queue size
     * @return Number of items in input queue
     */
    size_t getInputQueueSize() const { return input_queue_.size(); }
    
    /**
     * @brief Get output queue size
     * @return Number of items in output queue
     */
    size_t getOutputQueueSize() const { return output_queue_.size(); }
    
    /**
     * @brief Get processing statistics
     * @return Statistics
     */
    struct Statistics {
        uint64_t total_processed = 0;      ///< Total samples processed
        uint64_t total_signals = 0;        ///< Total signals detected
        uint64_t total_noise = 0;          ///< Total noise samples
        double avg_processing_time_ms = 0; ///< Average processing time
    };
    
    Statistics getStatistics() const;

private:
    /**
     * @brief Main processing loop (runs in separate thread)
     */
    void processingLoop();
    
    DetectionConfig config_;                           ///< Configuration
    size_t input_queue_size_;                          ///< Input queue size limit
    size_t output_queue_size_;                         ///< Output queue size limit
    
    // Processing engine
    std::unique_ptr<SignalDetectionEngine> engine_;    ///< Detection engine
    
    // Threading
    std::atomic<bool> running_{false};                 ///< Running flag
    std::thread processing_thread_;                    ///< Processing thread
    
    // Thread-safe queues
    ThreadSafeQueue<DetectionInput> input_queue_;      ///< Input queue
    ThreadSafeQueue<DetectionOutput> output_queue_;    ///< Output queue
    
    // Callback
    std::function<void(const DetectionOutput&)> result_callback_;
    
    // Statistics
    mutable std::atomic<uint64_t> total_processed_{0};
    mutable std::atomic<uint64_t> total_signals_{0};
    mutable std::atomic<uint64_t> total_noise_{0};
    mutable std::atomic<uint64_t> total_processing_time_ms_{0};
};

} // namespace SignalDetection
} // namespace CognitiveRF

#endif // SIGNAL_DETECTION_PIPELINE_HPP
