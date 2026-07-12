/**
 * @file LookThroughPipeline.cpp
 * @brief Implementation of TDD pipeline orchestrator with atomic state management
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.2 Arabakışlı Karıştırma (Look-Through Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-07-12
 */

#include "LookThroughPipeline.hpp"
#include <iostream>
#include <chrono>
#include <thread>

namespace CognitiveRF {
namespace LookThroughJamming {

LookThroughPipeline::LookThroughPipeline(const PipelineConfig& config)
    : config_(config)
    , engine_(std::make_unique<LookThroughEngine>(config))
    , running_(false)
    , hw_state_(HardwareState::IDLE)
    , target_active_(false)
    , cycle_start_time_ms_(0)
    , last_rx_time_ms_(0)
    , last_tx_time_ms_(0)
{
    // Initialize status report
    current_report_.status = JammingStatus::IDLE;
    current_report_.hw_state = HardwareState::IDLE;
    current_report_.timestamp_ms = getCurrentTimeMs();
    
    if (config_.enable_logging) {
        log("Pipeline constructed");
    }
}

LookThroughPipeline::~LookThroughPipeline() {
    stop();
    if (config_.enable_logging) {
        log("Pipeline destroyed");
    }
}

bool LookThroughPipeline::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        std::cerr << config_.log_prefix << " Pipeline already running" << std::endl;
        return false;
    }
    
    // Start processing thread
    processing_thread_ = std::thread(&LookThroughPipeline::processingLoop, this);
    
    log("Pipeline started");
    return true;
}

void LookThroughPipeline::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;  // Already stopped
    }
    
    // Shutdown queues
    command_queue_.shutdown();
    rx_data_queue_.shutdown();
    tx_data_queue_.shutdown();
    
    // Wait for processing thread
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
    
    // Force hardware to IDLE
    hw_state_.store(HardwareState::IDLE, std::memory_order_release);
    
    log("Pipeline stopped");
}

bool LookThroughPipeline::submitCommand(const JammingCommand& command) {
    if (!command.isValid()) {
        std::cerr << config_.log_prefix << " Invalid command" << std::endl;
        return false;
    }
    
    command_queue_.push(command);
    return true;
}

bool LookThroughPipeline::submitRxData(const IQBuffer& iq_samples) {
    if (hw_state_.load(std::memory_order_acquire) != HardwareState::SENSING) {
        return false;  // Not in RX mode
    }
    
    rx_data_queue_.push(iq_samples);
    return true;
}

bool LookThroughPipeline::getTxBuffer(IQBuffer& output) {
    return tx_data_queue_.try_pop(output);
}

bool LookThroughPipeline::getStatusReport(JammingReport& report) const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    report = current_report_;
    return true;
}

void LookThroughPipeline::setStatusCallback(std::function<void(const JammingReport&)> callback) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_callback_ = callback;
}

// ============================================================================
// MAIN PROCESSING LOOP (ET_Thread_2)
// ============================================================================

void LookThroughPipeline::processingLoop() {
    log("Processing loop started");
    
    while (running_.load(std::memory_order_acquire)) {
        // Process incoming commands
        processCommands();
        
        // Execute TDD state machine
        const HardwareState current_state = hw_state_.load(std::memory_order_acquire);
        
        switch (current_state) {
            case HardwareState::IDLE:
                handleIdleState();
                break;
                
            case HardwareState::SENSING:
                handleSensingState();
                break;
                
            case HardwareState::JAMMING:
                handleJammingState();
                break;
        }
        
        // Update status periodically
        updateStatus();
        
        // Small sleep to prevent CPU spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    log("Processing loop terminated");
}

// ============================================================================
// TDD STATE MACHINE HANDLERS
// ============================================================================

void LookThroughPipeline::handleIdleState() {
    // Wait for START command (processed in processCommands)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void LookThroughPipeline::handleSensingState() {
    // Perform energy detection (RX phase)
    SensingResult sensing_result;
    
    if (performSensingPhase(sensing_result)) {
        // Store latest sensing result
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            current_report_.last_sensing = sensing_result;
            current_report_.sensing_cycles_completed++;
            
            if (sensing_result.target_detected) {
                current_report_.target_detections++;
            }
        }
        
        // Decision: Should we jam?
        if (shouldJam(sensing_result)) {
            // Transition to JAMMING
            if (transitionToJamming()) {
                log("Target detected, switching to JAMMING");
            }
        } else {
            // Target inactive, continue sensing or return to IDLE
            std::lock_guard<std::mutex> lock(target_mutex_);
            if (!target_active_) {
                transitionToIdle();
                log("Target inactive, returning to IDLE");
            }
        }
    }
    
    // Wait for sensing window duration
    std::this_thread::sleep_for(
        std::chrono::milliseconds(config_.tdd.sensing_window_ms)
    );
}

void LookThroughPipeline::handleJammingState() {
    // Generate and transmit jamming pulse (TX phase)
    if (generateJammingPulse()) {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_report_.jamming_pulses_transmitted++;
    }
    
    // Wait for jamming pulse duration
    std::this_thread::sleep_for(
        std::chrono::milliseconds(config_.tdd.jamming_pulse_ms)
    );
    
    // Check thermal limits
    if (checkThermalLimit()) {
        log("Thermal limit reached, stopping jamming");
        transitionToIdle();
        return;
    }
    
    // After jamming pulse, return to SENSING to check if target still active
    if (transitionToSensing()) {
        log("Jamming pulse complete, returning to SENSING");
    }
}

// ============================================================================
// COMMAND PROCESSING
// ============================================================================

void LookThroughPipeline::processCommands() {
    JammingCommand cmd;
    while (command_queue_.try_pop(cmd)) {
        switch (cmd.action) {
            case JammingCommand::Action::START:
                if (executeStartCommand(cmd)) {
                    log("START command executed successfully");
                }
                break;
                
            case JammingCommand::Action::STOP:
                if (executeStopCommand()) {
                    log("STOP command executed successfully");
                }
                break;
                
            case JammingCommand::Action::PAUSE:
                // Transition to IDLE without clearing target
                transitionToIdle();
                log("PAUSE command executed");
                break;
                
            case JammingCommand::Action::RESUME:
                // Resume from IDLE to SENSING
                transitionToSensing();
                log("RESUME command executed");
                break;
        }
    }
}

bool LookThroughPipeline::executeStartCommand(const JammingCommand& cmd) {
    // Initialize engine with target
    if (!engine_->initialize(cmd.target)) {
        std::cerr << config_.log_prefix << " Failed to initialize engine" << std::endl;
        return false;
    }
    
    // Store active target
    {
        std::lock_guard<std::mutex> lock(target_mutex_);
        active_target_ = cmd.target;
        target_active_ = true;
    }
    
    // Update status
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_report_.target_id = cmd.target.target_id;
        current_report_.active_freq_hz = cmd.target.center_freq_hz;
        current_report_.active_bandwidth_hz = cmd.target.bandwidth_hz;
        current_report_.operation_start_ms = getCurrentTimeMs();
    }
    
    // Transition to SENSING to begin TDD cycle
    return transitionToSensing();
}

bool LookThroughPipeline::executeStopCommand() {
    // Clear target
    {
        std::lock_guard<std::mutex> lock(target_mutex_);
        target_active_ = false;
    }
    
    // Transition to IDLE
    return transitionToIdle();
}

// ============================================================================
// HARDWARE STATE TRANSITIONS (ATOMIC with RACE PROTECTION)
// ============================================================================

bool LookThroughPipeline::transitionToSensing() {
    // Attempt atomic transition to SENSING
    HardwareState expected = HardwareState::IDLE;
    if (hw_state_.compare_exchange_strong(expected, HardwareState::SENSING,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_report_.status = JammingStatus::SENSING_TARGET;
        current_report_.hw_state = HardwareState::SENSING;
        last_rx_time_ms_ = getCurrentTimeMs();
        return true;
    }
    
    // Try from JAMMING state as well (TDD cycle)
    expected = HardwareState::JAMMING;
    if (hw_state_.compare_exchange_strong(expected, HardwareState::SENSING,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_report_.status = JammingStatus::SENSING_TARGET;
        current_report_.hw_state = HardwareState::SENSING;
        last_rx_time_ms_ = getCurrentTimeMs();
        
        // Add switching delay
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.tdd.rx_tx_switching_delay_ms)
        );
        return true;
    }
    
    std::lock_guard<std::mutex> lock(status_mutex_);
    current_report_.state_transition_errors++;
    return false;
}

bool LookThroughPipeline::transitionToJamming() {
    // Can only transition from SENSING to JAMMING
    HardwareState expected = HardwareState::SENSING;
    if (hw_state_.compare_exchange_strong(expected, HardwareState::JAMMING,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_report_.status = JammingStatus::JAMMING_ACTIVE;
        current_report_.hw_state = HardwareState::JAMMING;
        last_tx_time_ms_ = getCurrentTimeMs();
        
        // Add switching delay
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.tdd.rx_tx_switching_delay_ms)
        );
        return true;
    }
    
    std::lock_guard<std::mutex> lock(status_mutex_);
    current_report_.state_transition_errors++;
    return false;
}

bool LookThroughPipeline::transitionToIdle() {
    // Can transition from any state to IDLE
    hw_state_.store(HardwareState::IDLE, std::memory_order_release);
    
    std::lock_guard<std::mutex> lock(status_mutex_);
    current_report_.status = JammingStatus::IDLE;
    current_report_.hw_state = HardwareState::IDLE;
    return true;
}

// ============================================================================
// ENERGY DETECTION AND DECISION
// ============================================================================

bool LookThroughPipeline::performSensingPhase(SensingResult& result) {
    // Wait for RX data from HackRF callback
    IQBuffer rx_buffer;
    if (!rx_data_queue_.try_pop(rx_buffer)) {
        // No data available yet, simulate with empty sensing
        result.target_detected = false;
        result.activity = TargetActivityState::UNKNOWN;
        return false;
    }
    
    // Perform energy detection
    if (!engine_->performEnergyDetection(rx_buffer, result)) {
        std::cerr << config_.log_prefix << " Energy detection failed" << std::endl;
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_report_.total_rx_samples += rx_buffer.size();
    }
    
    return true;
}

bool LookThroughPipeline::shouldJam(const SensingResult& sensing) const {
    // Jam if target detected AND target is supposed to be active
    std::lock_guard<std::mutex> lock(target_mutex_);
    return sensing.target_detected && target_active_;
}

// ============================================================================
// JAMMING GENERATION
// ============================================================================

bool LookThroughPipeline::generateJammingPulse() {
    // Calculate number of samples for jamming pulse duration
    const uint32_t samples_per_ms = static_cast<uint32_t>(
        config_.hardware.sample_rate_hz / 1000.0
    );
    const size_t num_samples = samples_per_ms * config_.tdd.jamming_pulse_ms;
    
    // Generate jamming samples
    IQBuffer tx_buffer;
    const size_t generated = engine_->generateJammingSamples(tx_buffer, num_samples);
    
    if (generated == 0) {
        std::cerr << config_.log_prefix << " Failed to generate jamming samples" << std::endl;
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_report_.tx_errors++;
        return false;
    }
    
    // Push to TX queue for HackRF callback
    tx_data_queue_.push(tx_buffer);
    
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_report_.total_tx_samples += generated;
    }
    
    return true;
}

// ============================================================================
// STATUS UPDATE AND MONITORING
// ============================================================================

void LookThroughPipeline::updateStatus() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    current_report_.timestamp_ms = getCurrentTimeMs();
    current_report_.operation_duration_ms = 
        current_report_.timestamp_ms - current_report_.operation_start_ms;
    
    // Update duty cycle
    current_report_.duty_cycle_current = computeDutyCycle();
    
    // Check thermal limit
    current_report_.thermal_limit_reached = checkThermalLimit();
    
    // Notify callback if set
    if (status_callback_) {
        notifyStatusUpdate();
    }
}

void LookThroughPipeline::notifyStatusUpdate() {
    // Call from within status_mutex_ lock
    if (status_callback_) {
        status_callback_(current_report_);
    }
}

uint64_t LookThroughPipeline::getCurrentTimeMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

bool LookThroughPipeline::checkThermalLimit() const {
    // In production, read actual temperature from HackRF
    // For now, estimate based on duty cycle
    const float duty_cycle = computeDutyCycle();
    
    if (!config_.enable_thermal_monitoring) {
        return false;
    }
    
    // Simple thermal model: assume 1% duty cycle = 0.5°C rise
    constexpr float AMBIENT_TEMP_C = 25.0f;
    const float estimated_temp = AMBIENT_TEMP_C + (duty_cycle * 50.0f);
    
    return estimated_temp > config_.hardware.thermal_limit_c;
}

float LookThroughPipeline::computeDutyCycle() const {
    // Duty cycle = TX_time / (TX_time + RX_time)
    const uint64_t current_time = getCurrentTimeMs();
    const uint64_t total_time = current_time - cycle_start_time_ms_;
    
    if (total_time == 0) {
        return 0.0f;
    }
    
    const uint64_t tx_time = last_tx_time_ms_ - cycle_start_time_ms_;
    return static_cast<float>(tx_time) / static_cast<float>(total_time);
}

void LookThroughPipeline::log(const std::string& message) const {
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " " << message << std::endl;
    }
}

} // namespace LookThroughJamming
} // namespace CognitiveRF