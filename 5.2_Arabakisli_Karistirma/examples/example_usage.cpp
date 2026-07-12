/**
 * @file example_usage.cpp
 * @brief Example usage of Look-Through Jamming module
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 5.2 Arabakışlı Karıştırma (Look-Through Jamming)
 * 
 * @author YM3XBA Development Team
 * @date 2026-07-12
 */

#include "LookThroughPipeline.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

using namespace CognitiveRF::LookThroughJamming;

// Global flag for signal handling
std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\n[SIGNAL] Caught signal " << signal << ", shutting down..." << std::endl;
    g_running.store(false);
}

// Status callback function
void onStatusUpdate(const JammingReport& report) {
    std::cout << "\n[STATUS UPDATE]" << std::endl;
    std::cout << "  Time: " << report.timestamp_ms << " ms" << std::endl;
    std::cout << "  Status: " << static_cast<int>(report.status) << std::endl;
    std::cout << "  HW State: " << static_cast<int>(report.hw_state) << std::endl;
    std::cout << "  Frequency: " << (report.active_freq_hz / 1e6) << " MHz" << std::endl;
    std::cout << "  Sensing cycles: " << report.sensing_cycles_completed << std::endl;
    std::cout << "  Jamming pulses: " << report.jamming_pulses_transmitted << std::endl;
    std::cout << "  Target detections: " << report.target_detections << std::endl;
    std::cout << "  Duty cycle: " << (report.duty_cycle_current * 100.0f) << "%" << std::endl;
    
    if (report.last_sensing.target_detected) {
        std::cout << "  Last sensing: TARGET DETECTED at " 
                  << report.last_sensing.measured_rssi_dbm << " dBm" << std::endl;
    }
}

int main(int argc, char** argv) {
    std::cout << "============================================================" << std::endl;
    std::cout << "Look-Through Jamming Module - Example Usage" << std::endl;
    std::cout << "Module 5.2: Arabakışlı Karıştırma" << std::endl;
    std::cout << "============================================================\n" << std::endl;
    
    // Install signal handler
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // ========================================================================
    // STEP 1: Configure pipeline
    // ========================================================================
    
    std::cout << "[STEP 1] Configuring pipeline..." << std::endl;
    
    PipelineConfig config;
    
    // Hardware configuration
    config.hardware.center_freq_hz = 433.92e6;  // 433.92 MHz (ISM band)
    config.hardware.sample_rate_hz = 20e6;      // 20 MSPS
    config.hardware.bandwidth_hz = 25e3;        // 25 kHz
    config.hardware.vga_gain_db = 20;           // TX gain
    config.hardware.rx_lna_gain_db = 24;        // RX gain for sensing
    config.hardware.rx_vga_gain_db = 16;
    config.hardware.tx_amp_enable = true;
    config.hardware.max_duty_cycle = 0.8f;
    config.hardware.thermal_limit_c = 70.0f;
    
    // Signal configuration
    config.signal.type = JammingType::GAUSSIAN_NOISE;
    config.signal.noise_amplitude = 0.6f;
    config.signal.noise_seed = 12345;  // For reproducibility
    
    // TDD timing configuration
    config.tdd.sensing_window_ms = 10;         // 10ms RX phase
    config.tdd.jamming_pulse_ms = 80;          // 80ms TX phase
    config.tdd.rx_tx_switching_delay_ms = 2;   // 2ms guard time
    config.tdd.noise_floor_dbm = -100.0f;
    config.tdd.detection_threshold_db = 10.0f; // 10 dB above noise
    config.tdd.use_adaptive_threshold = true;
    
    // Pipeline settings
    config.enable_logging = true;
    config.log_prefix = "[LookThrough]";
    
    // Validate configuration
    if (!config.validate()) {
        std::cerr << "[ERROR] Invalid configuration" << std::endl;
        return 1;
    }
    
    std::cout << "[OK] Configuration validated" << std::endl;
    
    // ========================================================================
    // STEP 2: Create and start pipeline
    // ========================================================================
    
    std::cout << "\n[STEP 2] Creating pipeline..." << std::endl;
    
    LookThroughPipeline pipeline(config);
    
    // Set status callback
    pipeline.setStatusCallback(onStatusUpdate);
    
    // Start pipeline
    if (!pipeline.start()) {
        std::cerr << "[ERROR] Failed to start pipeline" << std::endl;
        return 1;
    }
    
    std::cout << "[OK] Pipeline started" << std::endl;
    
    // ========================================================================
    // STEP 3: Create target intelligence (from ED pipeline)
    // ========================================================================
    
    std::cout << "\n[STEP 3] Creating target intelligence..." << std::endl;
    
    TargetIntelligence target;
    target.target_id = 1001;
    target.center_freq_hz = 433.92e6;  // 433.92 MHz
    target.bandwidth_hz = 25e3;        // 25 kHz
    target.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    target.rssi_dbm = -60.0f;          // Strong signal
    target.snr_db = 20.0f;
    target.priority = 1;
    
    std::cout << "[OK] Target created: ID=" << target.target_id 
              << ", Freq=" << (target.center_freq_hz / 1e6) << " MHz" << std::endl;
    
    // ========================================================================
    // STEP 4: Submit START command
    // ========================================================================
    
    std::cout << "\n[STEP 4] Starting jamming operation..." << std::endl;
    
    JammingCommand start_cmd;
    start_cmd.action = JammingCommand::Action::START;
    start_cmd.target = target;
    start_cmd.duration_ms = 0;  // Continuous until stopped
    
    if (!pipeline.submitCommand(start_cmd)) {
        std::cerr << "[ERROR] Failed to submit START command" << std::endl;
        pipeline.stop();
        return 1;
    }
    
    std::cout << "[OK] START command submitted" << std::endl;
    std::cout << "\nLook-Through jamming active. TDD cycle running:" << std::endl;
    std::cout << "  - RX Phase: " << config.tdd.sensing_window_ms << " ms" << std::endl;
    std::cout << "  - TX Phase: " << config.tdd.jamming_pulse_ms << " ms" << std::endl;
    std::cout << "  - Total cycle: " 
              << (config.tdd.sensing_window_ms + config.tdd.jamming_pulse_ms) 
              << " ms" << std::endl;
    
    // ========================================================================
    // STEP 5: Simulate RX data (in real system, this comes from HackRF)
    // ========================================================================
    
    std::cout << "\n[STEP 5] Simulating sensing phase..." << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;
    
    // Simulation loop
    uint32_t cycle_count = 0;
    const uint32_t max_cycles = 10;  // Run for 10 TDD cycles
    
    while (g_running.load() && cycle_count < max_cycles) {
        // Check current hardware state
        HardwareState hw_state = pipeline.getHardwareState();
        
        if (hw_state == HardwareState::SENSING) {
            // Simulate RX data (in real system, this comes from HackRF RX callback)
            IQBuffer rx_samples(8192);  // Typical buffer size
            
            // Simulate target signal: add some power
            const float signal_power = (cycle_count % 3 == 0) ? 0.5f : 0.01f;
            
            for (size_t i = 0; i < rx_samples.size(); ++i) {
                rx_samples[i] = IQSample(
                    signal_power * std::sin(2.0 * 3.14159 * i / 100.0),
                    signal_power * std::cos(2.0 * 3.14159 * i / 100.0)
                );
            }
            
            pipeline.submitRxData(rx_samples);
        }
        
        if (hw_state == HardwareState::JAMMING) {
            // Get TX buffer (in real system, this feeds HackRF TX callback)
            IQBuffer tx_buffer;
            if (pipeline.getTxBuffer(tx_buffer)) {
                // In real system: Send tx_buffer to HackRF
                std::cout << "[SIMULATED TX] Transmitted " << tx_buffer.size() 
                          << " jamming samples" << std::endl;
            }
            
            cycle_count++;
        }
        
        // Small sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    // ========================================================================
    // STEP 6: Stop jamming
    // ========================================================================
    
    std::cout << "\n[STEP 6] Stopping jamming operation..." << std::endl;
    
    JammingCommand stop_cmd;
    stop_cmd.action = JammingCommand::Action::STOP;
    pipeline.submitCommand(stop_cmd);
    
    // Wait for stop to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // ========================================================================
    // STEP 7: Get final status report
    // ========================================================================
    
    std::cout << "\n[STEP 7] Final status report..." << std::endl;
    
    JammingReport final_report;
    if (pipeline.getStatusReport(final_report)) {
        std::cout << "\n[FINAL REPORT]" << std::endl;
        std::cout << "  Total sensing cycles: " << final_report.sensing_cycles_completed << std::endl;
        std::cout << "  Total jamming pulses: " << final_report.jamming_pulses_transmitted << std::endl;
        std::cout << "  Target detections: " << final_report.target_detections << std::endl;
        std::cout << "  Detection rate: " 
                  << (100.0 * final_report.target_detections / final_report.sensing_cycles_completed) 
                  << "%" << std::endl;
        std::cout << "  Total RX samples: " << final_report.total_rx_samples << std::endl;
        std::cout << "  Total TX samples: " << final_report.total_tx_samples << std::endl;
        std::cout << "  RX errors: " << final_report.rx_errors << std::endl;
        std::cout << "  TX errors: " << final_report.tx_errors << std::endl;
        std::cout << "  State transition errors: " << final_report.state_transition_errors << std::endl;
        std::cout << "  Final duty cycle: " << (final_report.duty_cycle_current * 100.0f) << "%" << std::endl;
        std::cout << "  Operation duration: " << (final_report.operation_duration_ms / 1000.0) << " seconds" << std::endl;
    }
    
    // ========================================================================
    // STEP 8: Shutdown
    // ========================================================================
    
    std::cout << "\n[STEP 8] Shutting down..." << std::endl;
    pipeline.stop();
    
    std::cout << "\n============================================================" << std::endl;
    std::cout << "Look-Through Jamming Example Completed Successfully" << std::endl;
    std::cout << "============================================================" << std::endl;
    
    return 0;
}