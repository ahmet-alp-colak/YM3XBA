/**
 * @file SpotJammingPipeline.cpp
 * @brief Implementation of thread-safe spot jamming pipeline
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.1 Sürekli Karıştırma (Spot Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-06-29
 */

#include "SpotJammingPipeline.hpp"
#include <iostream>
#include <chrono>

namespace CognitiveRF {
namespace SpotJamming {

SpotJammingPipeline::SpotJammingPipeline(const PipelineConfig& config)
    : config_(config)
    , engine_(std::make_unique<SpotJammingEngine>(config))
{
    if (!config_.validate()) {
        throw std::invalid_argument("Invalid pipeline configuration");
    }
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Pipeline constructed" << std::endl;
    }
}

SpotJammingPipeline::~SpotJammingPipeline() {
    stop();
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Pipeline destroyed" << std::endl;
    }
}

bool SpotJammingPipeline::start() {
    if (running_.load()) {
        std::cerr << config_.log_prefix << " Pipeline already running" << std::endl;
        return false;
    }
    
    if (!initializeHardware()) {
        std::cerr << config_.log_prefix << " Hardware initialization failed" << std::endl;
        return false;
    }
    
    running_.store(true);
    processing_thread_ = std::thread(&SpotJammingPipeline::processingLoop, this);
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Pipeline started" << std::endl;
    }
    
    return true;
}

void SpotJammingPipeline::stop() {
    if (!running_.load()) {
        return;
    }
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Stopping pipeline..." << std::endl;
    }
    
    running_.store(false);
    command_queue_.shutdown();
    status_queue_.shutdown();
    
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
    
    shutdownHardware();
    

bool SpotJammingPipeline::submitCommand(const JammingCommand& command) {
    if (!command.isValid()) {
        std::cerr << config_.log_prefix << " Invalid command" << std::endl;
        return false;
    }
    
    command_queue_.push(command);
    return true;
}

bool SpotJammingPipeline::getStatus(JammingReport& report) {
    return status_queue_.try_pop(report);
}

void SpotJammingPipeline::setStatusCallback(std::function<void(const JammingReport&)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    status_callback_ = callback;
}

void SpotJammingPipeline::processingLoop() {
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Processing loop started" << std::endl;
    }
    
    while (running_.load()) {
        JammingCommand command;
        if (command_queue_.try_pop(command)) {
            executeCommand(command);
        }
        
        if (jamming_status_.load() == JammingStatus::ACTIVE) {
            generateAndTransmit();
        }
        
        publishStatus();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    stopJamming();
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Processing loop ended" << std::endl;
    }
}

bool SpotJammingPipeline::executeCommand(const JammingCommand& command) {
    switch (command.action) {
        case JammingCommand::Action::START:
            startJamming(command.target, command.duration_ms);
            return true;
        case JammingCommand::Action::STOP:
            stopJamming();
            return true;
        case JammingCommand::Action::PAUSE:
            pauseJamming();
            return true;
        case JammingCommand::Action::RESUME:
            resumeJamming();
            return true;
        default:
            return false;
    }
}

void SpotJammingPipeline::startJamming(const TargetIntelligence& target, uint32_t duration_ms) {
    if (jamming_status_.load() == JammingStatus::ACTIVE) {
        std::cerr << config_.log_prefix << " Jamming already active" << std::endl;
        return;
    }
    
    if (!transitionHardwareState(HardwareState::IDLE, HardwareState::JAMMING)) {
        std::cerr << config_.log_prefix << " Hardware state transition failed" << std::endl;
        return;
    }
    
    if (!engine_->initialize(target)) {
        std::cerr << config_.log_prefix << " Engine initialization failed" << std::endl;
        hw_state_.store(HardwareState::IDLE);
        return;
    }
    
    jamming_status_.store(JammingStatus::ACTIVE);
    samples_transmitted_.store(0);
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Jamming started on " 
                  << (target.center_freq_hz / 1e6) << " MHz" << std::endl;
    }
}

void SpotJammingPipeline::stopJamming() {
    if (jamming_status_.load() == JammingStatus::IDLE) {
        return;
    }
    
    jamming_status_.store(JammingStatus::STOPPING);
    hw_state_.store(HardwareState::IDLE);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    jamming_status_.store(JammingStatus::IDLE);
    
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Jamming stopped" << std::endl;
    }
}

void SpotJammingPipeline::pauseJamming() {
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Pause not yet implemented" << std::endl;
    }
}

void SpotJammingPipeline::resumeJamming() {
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Resume not yet implemented" << std::endl;
    }
}

bool SpotJammingPipeline::transitionHardwareState(HardwareState expected, HardwareState desired) {
    return hw_state_.compare_exchange_strong(expected, desired);
}

void SpotJammingPipeline::generateAndTransmit() {
    const size_t buffer_size = config_.hardware.tx_buffer_samples;
    IQBuffer tx_buffer;
    
    size_t generated = engine_->generateSamples(tx_buffer, buffer_size);
    if (generated == 0) {
        tx_errors_.fetch_add(1);
        return;
    }
    
    if (transmitBuffer(tx_buffer)) {
        samples_transmitted_.fetch_add(generated);
    } else {
        tx_errors_.fetch_add(1);
    }
}

void SpotJammingPipeline::publishStatus() {
    JammingReport report;
    report.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    report.status = jamming_status_.load();
    report.hw_state = hw_state_.load();
    report.samples_transmitted = samples_transmitted_.load();
    report.tx_errors = tx_errors_.load();
    
    status_queue_.push(report);
    
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (status_callback_) {
        status_callback_(report);
    }
}

bool SpotJammingPipeline::initializeHardware() {
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Initializing hardware (placeholder)" << std::endl;
    }
    return true;
}

void SpotJammingPipeline::shutdownHardware() {
    hw_state_.store(HardwareState::IDLE);
    if (config_.enable_logging) {
        std::cout << config_.log_prefix << " Hardware shutdown (placeholder)" << std::endl;
    }
}

bool SpotJammingPipeline::transmitBuffer(const IQBuffer& buffer) {
    (void)buffer;
    return true;
}

} // namespace SpotJamming
} // namespace CognitiveRF
