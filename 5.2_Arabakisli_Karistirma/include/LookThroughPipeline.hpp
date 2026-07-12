/**
 * @file LookThroughPipeline.hpp
 * @brief Thread-safe TDD pipeline orchestrator for Look-Through Jamming
 * @details Manages TDD state machine, hardware transitions, and coordinates
 *          sensing/jamming phases with atomic state protection.
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.2 Arabakışlı Karıştırma (Look-Through Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-07-12
 */

#ifndef COGNITIVE_RF_LOOK_THROUGH_PIPELINE_HPP
#define COGNITIVE_RF_LOOK_THROUGH_PIPELINE_HPP

#include "DataStructures.hpp"
#include "LookThroughEngine.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>

namespace CognitiveRF {
namespace LookThroughJamming {

/**
 * @brief Thread-safe queue for command/data transfer
 * 
 * SPSC (Single Producer Single Consumer) optimized queue
 * with atomic synchronization.
 */
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() : shutdown_(false) {}
    
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        cond_var_.notify_one();
    }
    
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { 
            return !queue_.empty() || shutdown_; 
        });
        
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
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        cond_var_.notify_all();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    bool shutdown_;
};

/**
 * @brief Look-Through Jamming Pipeline Orchestrator
 * 
 * Manages complete TDD cycle with atomic hardware state transitions:
 * 
 * Thread Architecture:
 * - ET_Thread_2 (Main Processing Loop): Executes TDD state machine
 * - ET_Thread_4 (Hardware TX Control): Handled by external HackRF manager
 * 
 * State Machine Flow:
 * 1. IDLE: Wait for start command
 * 2. SENSING: Switch to RX, perform energy detection
 * 3. Decision: Target active? -> JAMMING : continue SENSING
 * 4. JAMMING: Switch to TX, transmit jamming pulse
 * 5. Loop back to SENSING until target inactive or stop command
 * 
 * Race Condition Protection:
 * All hw_state transitions use std::atomic::compare_exchange_strong
 * to prevent simultaneous RX/TX activation.
 */
class LookThroughPipeline {
public:
    /**
     * @brief Construct pipeline with configuration
     * @param config Pipeline configuration
     */
    explicit LookThroughPipeline(const PipelineConfig& config);
    
    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~LookThroughPipeline();
    
    // Disable copy and move
    LookThroughPipeline(const LookThroughPipeline&) = delete;
    LookThroughPipeline& operator=(const LookThroughPipeline&) = delete;
    
    /**
     * @brief Start pipeline processing thread
     * @return true if started successfully
     */
    bool start();
    
    /**
     * @brief Stop pipeline gracefully
     */
    void stop();
    
    /**
     * @brief Submit jamming command (thread-safe)
     * @param command Jamming command (START/STOP/PAUSE/RESUME)
     * @return true if command queued successfully
     */
    bool submitCommand(const JammingCommand& command);
    
    /**
     * @brief Submit received IQ data for energy detection (thread-safe)
     * @param iq_samples Received samples from HackRF RX
     * @return true if data submitted successfully
     * 
     * @note Called by external HackRF RX callback during SENSING phase
     */
    bool submitRxData(const IQBuffer& iq_samples);
    
    /**
     * @brief Get jamming output buffer (thread-safe)
     * @param output Output buffer to fill with jamming samples
     * @return true if buffer available
     * 
     * @note Called by external HackRF TX callback during JAMMING phase
     */
    bool getTxBuffer(IQBuffer& output);
    
    /**
     * @brief Get current status report (thread-safe)
     * @param report Output status structure
     * @return true if report retrieved
     */
    bool getStatusReport(JammingReport& report) const;
    
    /**
     * @brief Set status report callback
     * @param callback Function called when status updates
     */
    void setStatusCallback(std::function<void(const JammingReport&)> callback);
    
    /**
     * @brief Get current hardware state (atomic read)
     * @return Current hardware state
     */
    HardwareState getHardwareState() const {
        return hw_state_.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Set hardware state (atomic write with race protection)
     * @param expected Expected current state
     * @param desired Desired new state
     * @return true if transition successful
     */
    bool setHardwareState(HardwareState expected, HardwareState desired) {
        return hw_state_.compare_exchange_strong(expected, desired,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire);
    }
    
    /**
     * @brief Check if pipeline is running
     */
    bool isRunning() const {
        return running_.load(std::memory_order_acquire);
    }

private:
    // Configuration
    PipelineConfig config_;
    
    // Processing engine
    std::unique_ptr<LookThroughEngine> engine_;
    
    // Threading
    std::atomic<bool> running_;
    std::thread processing_thread_;
    
    // Hardware state (atomic for race protection)
    std::atomic<HardwareState> hw_state_;
    
    // Command and data queues
    ThreadSafeQueue<JammingCommand> command_queue_;
    ThreadSafeQueue<IQBuffer> rx_data_queue_;
    ThreadSafeQueue<IQBuffer> tx_data_queue_;
    
    // Status reporting
    mutable std::mutex status_mutex_;
    JammingReport current_report_;
    std::function<void(const JammingReport&)> status_callback_;
    
    // Current target tracking
    mutable std::mutex target_mutex_;  // mutable for const methods
    TargetIntelligence active_target_;
    bool target_active_ = false;
    
    // TDD cycle timing
    uint64_t cycle_start_time_ms_ = 0;
    uint64_t last_rx_time_ms_ = 0;
    uint64_t last_tx_time_ms_ = 0;
    
    // Main processing loop (runs in ET_Thread_2)
    void processingLoop();
    
    // TDD state machine handlers
    void handleIdleState();
    void handleSensingState();
    void handleJammingState();
    
    // Command processing
    void processCommands();
    bool executeStartCommand(const JammingCommand& cmd);
    bool executeStopCommand();
    
    // Hardware transition helpers
    bool transitionToSensing();
    bool transitionToJamming();
    bool transitionToIdle();
    
    // Energy detection and decision
    bool performSensingPhase(SensingResult& result);
    bool shouldJam(const SensingResult& sensing) const;
    
    // Jamming generation
    bool generateJammingPulse();
    
    // Status update helpers
    void updateStatus();
    void notifyStatusUpdate();
    uint64_t getCurrentTimeMs() const;
    
    // Thermal and duty cycle monitoring
    bool checkThermalLimit() const;
    float computeDutyCycle() const;
    
    // Logging
    void log(const std::string& message) const;
};

} // namespace LookThroughJamming
} // namespace CognitiveRF

#endif // COGNITIVE_RF_LOOK_THROUGH_PIPELINE_HPP