/**
 * @file example_usage.cpp
 * @brief Example usage of Spot Jamming (Sürekli Karıştırma) module
 * 
 * This example demonstrates:
 * 1. Pipeline configuration
 * 2. Starting jamming operations
 * 3. Real-time status monitoring
 * 4. Graceful shutdown
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.1 Sürekli Karıştırma (Spot Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-06-29
 */

#include "DataStructures.hpp"
#include "SpotJammingPipeline.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace CognitiveRF::SpotJamming;

void printSeparator() {
    std::cout << std::string(80, '=') << std::endl;
}

void printStatus(const JammingReport& report) {
    std::cout << "\n[STATUS UPDATE]" << std::endl;
    std::cout << "  Timestamp: " << report.timestamp_ms << " ms" << std::endl;
    std::cout << "  Status: " << static_cast<int>(report.status) << std::endl;
    std::cout << "  HW State: " << static_cast<int>(report.hw_state) << std::endl;
    std::cout << "  Samples TX: " << report.samples_transmitted << std::endl;
    std::cout << "  TX Errors: " << report.tx_errors << std::endl;
    std::cout << "  Active: " << (report.isActive() ? "YES" : "NO") << std::endl;
}

int main() {
    printSeparator();
    std::cout << "5.1 SPOT JAMMING (SÜREKLİ KARIŞTIRMA) - EXAMPLE USAGE" << std::endl;
    printSeparator();
    
    std::cout << "\n[STEP 1] Configuring pipeline..." << std::endl;
    
    PipelineConfig config;
    config.hardware.center_freq_hz = 433.92e6;
    config.hardware.sample_rate_hz = 20e6;
    config.hardware.bandwidth_hz = 25e3;
    config.hardware.lna_gain_db = 0;
    config.hardware.vga_gain_db = 20;
    config.hardware.tx_amp_enable = true;
    config.hardware.tx_buffer_samples = 8192;
    
    config.signal.type = JammingType::GAUSSIAN_NOISE;
    config.signal.noise_amplitude = 0.7f;
    config.signal.noise_seed = 0;
    
    config.queue_max_size = 100;
    config.processing_timeout_ms = 1000;
    config.enable_logging = true;
    
    if (!config.validate()) {
        std::cerr << "ERROR: Invalid configuration!" << std::endl;
        return 1;
    }
    
    std::cout << "  Frequency: " << (config.hardware.center_freq_hz / 1e6) << " MHz" << std::endl;
    std::cout << "  Bandwidth: " << (config.hardware.bandwidth_hz / 1e3) << " kHz" << std::endl;
    std::cout << "  Jamming Type: GAUSSIAN_NOISE" << std::endl;
    
    std::cout << "\n[STEP 2] Creating pipeline..." << std::endl;
    
    SpotJammingPipeline pipeline(config);
    
    pipeline.setStatusCallback([](const JammingReport& report) {
        static uint64_t last_samples = 0;
        if (report.samples_transmitted > last_samples + 1000000) {
            std::cout << "  [CALLBACK] Transmitted: " << report.samples_transmitted << " samples" << std::endl;
            last_samples = report.samples_transmitted;
        }
    });
    
    if (!pipeline.start()) {
        std::cerr << "ERROR: Failed to start pipeline!" << std::endl;
        return 1;
    }
    
    std::cout << "  Pipeline started successfully" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "\n[STEP 3] Simulating target intelligence from ED pipeline..." << std::endl;
    
    TargetIntelligence target;
    target.target_id = 12345;
    target.center_freq_hz = 433.92e6;
    target.bandwidth_hz = 25e3;
    target.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    target.rssi_dbm = -65.0f;
    target.snr_db = 15.0f;
    target.priority = 100;
    
    std::cout << "  Target ID: " << target.target_id << std::endl;
    std::cout << "  Frequency: " << (target.center_freq_hz / 1e6) << " MHz" << std::endl;
    std::cout << "  RSSI: " << target.rssi_dbm << " dBm" << std::endl;
    std::cout << "  SNR: " << target.snr_db << " dB" << std::endl;
    
    std::cout << "\n[STEP 4] Starting jamming operation..." << std::endl;
    
    JammingCommand start_cmd;
    start_cmd.action = JammingCommand::Action::START;
    start_cmd.target = target;
    start_cmd.duration_ms = 0;
    
    if (!pipeline.submitCommand(start_cmd)) {
        std::cerr << "ERROR: Failed to submit START command!" << std::endl;
        pipeline.stop();
        return 1;
    }
    
    std::cout << "  START command submitted" << std::endl;
    
    std::cout << "\n[STEP 5] Monitoring jamming operation for 5 seconds..." << std::endl;
    printSeparator();
    
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        JammingReport report;
        if (pipeline.getStatus(report)) {
            printStatus(report);
        }
    }
    
    printSeparator();
    std::cout << "\n[STEP 6] Stopping jamming operation..." << std::endl;
    
    JammingCommand stop_cmd;
    stop_cmd.action = JammingCommand::Action::STOP;
    
    if (!pipeline.submitCommand(stop_cmd)) {
        std::cerr << "ERROR: Failed to submit STOP command!" << std::endl;
    }
    
    std::cout << "  STOP command submitted" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    std::cout << "\n[STEP 7] Shutting down pipeline..." << std::endl;
    pipeline.stop();
    
    JammingReport final_report;
    if (pipeline.getStatus(final_report)) {
        std::cout << "\n[FINAL STATISTICS]" << std::endl;
        std::cout << "  Total samples transmitted: " << final_report.samples_transmitted << std::endl;
        std::cout << "  Total TX errors: " << final_report.tx_errors << std::endl;
        
        if (final_report.samples_transmitted > 0) {
            double duration_s = 5.0;
            double throughput_msps = (final_report.samples_transmitted / duration_s) / 1e6;
            std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                      << throughput_msps << " MSPS" << std::endl;
        }
    }
    
    printSeparator();
    std::cout << "\n[SUCCESS] Example completed successfully!" << std::endl;
    printSeparator();
    
    return 0;
}
