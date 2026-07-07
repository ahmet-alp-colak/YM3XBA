/**
 * @file SpotJammingPipeline.hpp
 * @brief Thread-safe pipeline orchestrator for spot jamming operations
 * @details Manages multi-threaded jamming pipeline with hardware state machine,
 *          command queue, and asynchronous status reporting.
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.1 Sürekli Karıştırma (Spot Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-06-29
 */

#ifndef COGNITIVE_RF_SPOT_JAMMING_PIPELINE_HPP
#define COGNITIVE_RF_SPOT_JAMMING_PIPELINE_HPP

#include "DataStructures.hpp"
#include "SpotJammingEngine.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <functional>
#include <memory>

namespace CognitiveRF {
namespace SpotJamming {

/**
 * @brief Thread-safe queue for inter-thread communication
 * @tparam T Data type to be queued
 */
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;
    ~ThreadSafeQueue() { shutdown(); }
    
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        cond_var_.notify_one();
    }
    
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty() || shutdown_; });
        
        if (shutdown_ && queue_.empty()) {
            return false;
        }
        
        item = queue_.front();
        queue_.pop();
        return true;
    }
    
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = queue_.front();
        queue_.pop();
        return true;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        cond_var_.notify_all();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    bool shutdown_ = false;
};

/**
 * @brief Main pipeline orchestrator for spot jamming operations
 * 
 * Manages the complete jamming workflow:
 * 1. Command reception (START/STOP/PAUSE/RESUME)
 * 2. Signal generation (via SpotJammingEngine)
 * 3. Hardware TX control (HackRF state machine)
 * 4. Status reporting
 * 
 * Thread architecture:
 * - Main thread: User interaction
 * - Processing thread: Command handling + signal generation
 * - (Optional) Hardware thread: TX buffer management
 */
class SpotJammingPipeline {
public:
    /**
     * @brief Construct pipeline with configuration
     * @param config Complete pipeline configuration
     */
    explicit SpotJammingPipeline(const PipelineConfig& config);
    
    /**
     * @brief Destructor - safely shuts down threads
     */
    ~SpotJammingPipeline();
    
    // Disable copy/move
    SpotJammingPipeline(const SpotJammingPipeline&) = delete;
    SpotJammingPipeline& operator=(const SpotJammingPipeline&) = delete;
    
    /**
     * @brief Start the pipeline (spawns processing thread)
     * @return true if started successfully
     */
    bool start();
    
    /**
     * @brief Stop the pipeline (graceful shutdown)
     */
    void stop();
    
    /**
     * @brief Submit jamming command (thread-safe)
     * @param command Command to execute
     * @return true if command was queued
     */
    bool submitCommand(const JammingCommand& command);
    
    /**
     * @brief Get latest status report (non-blocking)
     * @param report Output report structure
     * @return true if report available
     */
    bool getStatus(JammingReport& report);
    
    /**
     * @brief Set callback for status updates
     * @param callback Function called on each status update
     */
    void setStatusCallback(std::function<void(const JammingReport&)> callback);
    
    /**
     * @brief Check if pipeline is running
     * @return true if processing thread active
     */
    bool isRunning() const { return running_.load(); }
    
    /**
     * @brief Get current hardware state
     * @return Current hardware state (atomic read)
     */
    HardwareState getHardwareState() const { return hw_state_.load(); }

private:
    // Configuration
    PipelineConfig config_;
    
    // Core engine
    std::unique_ptr<SpotJammingEngine> engine_;
    
    // Threading
    std::atomic<bool> running_{false};
    std::thread processing_thread_;
    
    // Hardware state machine (atomic for race-free RX/TX switching)
    std::atomic<HardwareState> hw_state_{HardwareState::IDLE};
    
    // Communication queues
    ThreadSafeQueue<JammingCommand> command_queue_;
    ThreadSafeQueue<JammingReport> status_queue_;
    
    // Status callback
    std::mutex callback_mutex_;
    std::function<void(const JammingReport&)> status_callback_;
    
    // Current operation state
    std::atomic<JammingStatus> jamming_status_{JammingStatus::IDLE};
    std::atomic<uint64_t> samples_transmitted_{0};
    std::atomic<uint32_t> tx_errors_{0};
    
    // Private methods
    void processingLoop();
    bool executeCommand(const JammingCommand& command);
    void startJamming(const TargetIntelligence& target, uint32_t duration_ms);
    void stopJamming();
    void pauseJamming();
    void resumeJamming();
    
    bool transitionHardwareState(HardwareState expected, HardwareState desired);
    void generateAndTransmit();
    void publishStatus();
    
    // Hardware interface (placeholder - actual implementation needs libhackrf)
    bool initializeHardware();
    void shutdownHardware();
    bool transmitBuffer(const IQBuffer& buffer);
};

} // namespace SpotJamming
} // namespace CognitiveRF

#endif // COGNITIVE_RF_SPOT_JAMMING_PIPELINE_HPP
