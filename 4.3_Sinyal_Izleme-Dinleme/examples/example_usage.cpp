/**
 * @file example_usage.cpp
 * @brief Complete usage example for Signal Monitoring Pipeline
 * @details Demonstrates signal tracking, demodulation, and payload extraction
 * 
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.3 Sinyal İzleme/Dinleme
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "SignalMonitoringPipeline.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>

using namespace CognitiveRF::SignalMonitoring;

/**
 * @brief Generate simulated IQ data for testing
 */
IQDataBuffer generateTestSignal(ModulationType mod, double freq_hz, float snr_db) {
    IQDataBuffer buffer;
    buffer.center_freq_hz = static_cast<uint64_t>(freq_hz);
    buffer.sample_rate_hz = 20000000;  // 20 MSPS
    buffer.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
    buffer.detected_modulation = mod;
    buffer.signal_class = SignalClass::CONTINUOUS;
    buffer.snr_db = snr_db;
    
    // Generate 1024 samples of test signal
    buffer.iq_data.resize(1024);
    for (size_t i = 0; i < 1024; i++) {
        // Simple test signal: tone + noise
        float phase = 2.0f * M_PI * 1000.0f * i / buffer.sample_rate_hz;
        float signal_i = std::cos(phase);
        float signal_q = std::sin(phase);
        
        // Add noise based on SNR
        float noise_power = std::pow(10.0f, -snr_db / 20.0f);
        float noise_i = (rand() / (float)RAND_MAX - 0.5f) * noise_power;
        float noise_q = (rand() / (float)RAND_MAX - 0.5f) * noise_power;
        
        buffer.iq_data[i] = std::complex<float>(signal_i + noise_i, signal_q + noise_q);
    }
    
    return buffer;
}

/**
 * @brief Save audio to WAV file
 */
void saveAudioToWAV(const DemodulatedAudio& audio, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open " << filename << "\n";
        return;
    }
    
    // Simple WAV header (44 bytes)
    uint32_t sample_rate = audio.sample_rate_hz;
    uint32_t num_samples = audio.pcm_samples.size();
    uint32_t byte_rate = sample_rate * 2;  // 16-bit mono
    uint32_t data_size = num_samples * 2;
    
    file.write("RIFF", 4);
    uint32_t chunk_size = 36 + data_size;
    file.write(reinterpret_cast<char*>(&chunk_size), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    uint32_t fmt_size = 16;
    file.write(reinterpret_cast<char*>(&fmt_size), 4);
    uint16_t audio_format = 1;  // PCM
    file.write(reinterpret_cast<char*>(&audio_format), 2);
    uint16_t num_channels = 1;  // Mono
    file.write(reinterpret_cast<char*>(&num_channels), 2);
    file.write(reinterpret_cast<char*>(&sample_rate), 4);
    file.write(reinterpret_cast<char*>(&byte_rate), 4);
    uint16_t block_align = 2;
    file.write(reinterpret_cast<char*>(&block_align), 2);
    uint16_t bits_per_sample = 16;
    file.write(reinterpret_cast<char*>(&bits_per_sample), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<char*>(&data_size), 4);
    
    // Write audio samples (convert float to int16)
    for (float sample : audio.pcm_samples) {
        int16_t sample_int = static_cast<int16_t>(sample * 32767.0f);
        file.write(reinterpret_cast<char*>(&sample_int), 2);
    }
    
    std::cout << "✅ Audio saved to " << filename << "\n";
}

/**
 * @brief Example 1: Basic signal monitoring
 */
void example1_basic_monitoring() {
    std::cout << "\n========================================\n";
    std::cout << "Example 1: Basic Signal Monitoring\n";
    std::cout << "========================================\n\n";
    
    // Create pipeline with default configuration
    SignalMonitoringPipeline::PipelineConfig config;
    config.enable_tracking = true;
    config.enable_demodulation = true;
    config.enable_payload_extraction = false;  // Disable for this example
    
    SignalMonitoringPipeline pipeline(config);
    
    if (!pipeline.start()) {
        std::cerr << "Failed to start pipeline!\n";
        return;
    }
    
    std::cout << "Pipeline started successfully.\n\n";
    
    // Submit test FM signal
    auto iq_buffer = generateTestSignal(ModulationType::FM, 435000000, 20.0f);
    
    if (pipeline.submitIQData(iq_buffer)) {
        std::cout << "✅ IQ data submitted\n";
    }
    
    // Get result
    MonitoredSignal result;
    if (pipeline.getResult(result, 2000)) {  // 2 second timeout
        std::cout << "\n📡 Signal Monitored:\n";
        std::cout << "  Frequency: " << result.carrier_frequency_hz / 1e6 << " MHz\n";
        std::cout << "  Modulation: " << ModulationTypeToString(result.modulation) << "\n";
        std::cout << "  RSSI: " << result.rssi_dbm << " dBm\n";
        std::cout << "  SNR: " << result.snr_db << " dB\n";
        std::cout << "  Tracking ID: " << result.tracking_id << "\n";
        
        if (result.audio) {
            std::cout << "  Audio samples: " << result.audio->pcm_samples.size() << "\n";
            saveAudioToWAV(*result.audio, "output_fm.wav");
        }
    } else {
        std::cout << "⚠️ No result received (timeout)\n";
    }
    
    // Get statistics
    auto stats = pipeline.getStatistics();
    std::cout << "\n📊 Pipeline Statistics:\n";
    std::cout << "  Total processed: " << stats.total_processed << "\n";
    std::cout << "  Total demodulated: " << stats.total_demodulated << "\n";
    std::cout << "  Active tracked signals: " << stats.active_tracked_signals << "\n";
    
    pipeline.stop();
    std::cout << "\nPipeline stopped.\n";
}

/**
 * @brief Example 2: FHSS detection
 */
void example2_fhss_detection() {
    std::cout << "\n========================================\n";
    std::cout << "Example 2: FHSS Detection\n";
    std::cout << "========================================\n\n";
    
    SignalMonitoringPipeline pipeline;
    pipeline.start();
    
    // Simulate frequency hopping signal
    std::vector<double> frequencies = {
        435000000, 435500000, 436000000, 435250000, 435750000
    };
    
    std::cout << "Simulating frequency hopping signal...\n";
    
    for (size_t i = 0; i < frequencies.size(); i++) {
        auto iq_buffer = generateTestSignal(
            ModulationType::QPSK,
            frequencies[i],
            15.0f
        );
        iq_buffer.signal_class = SignalClass::BURST;
        
        pipeline.submitIQData(iq_buffer);
        std::cout << "  Hop " << (i+1) << ": " << frequencies[i] / 1e6 << " MHz\n";
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Check for FHSS detection
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    MonitoredSignal result;
    while (pipeline.getResult(result, 100)) {
        if (result.fhss_suspected) {
            std::cout << "\n🚨 FHSS DETECTED!\n";
            std::cout << "  Tracking ID: " << result.tracking_id << "\n";
            std::cout << "  Occupancy: " << (result.occupancy_ratio * 100) << "%\n";
            break;
        }
    }
    
    pipeline.stop();
}

/**
 * @brief Example 3: Real-time callback
 */
void example3_realtime_callback() {
    std::cout << "\n========================================\n";
    std::cout << "Example 3: Real-time Callback\n";
    std::cout << "========================================\n\n";
    
    SignalMonitoringPipeline pipeline;
    
    // Set callback for real-time notifications
    pipeline.setResultCallback([](const MonitoredSignal& signal) {
        std::cout << "📡 Callback: Signal at " 
                  << signal.carrier_frequency_hz / 1e6 << " MHz, "
                  << "SNR: " << signal.snr_db << " dB";
        
        if (signal.fhss_suspected) {
            std::cout << " [FHSS SUSPECTED]";
        }
        
        if (signal.payload) {
            std::cout << " [PAYLOAD: " << signal.payload->payload_data.size() << " bytes]";
        }
        
        std::cout << "\n";
    });
    
    pipeline.start();
    
    // Submit multiple signals
    for (int i = 0; i < 5; i++) {
        auto iq_buffer = generateTestSignal(
            ModulationType::FM,
            435000000 + i * 1000000,
            20.0f - i * 2
        );
        pipeline.submitIQData(iq_buffer);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    pipeline.stop();
}

/**
 * @brief Main function
 */
int main() {
    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << "  Signal Monitoring & Listening Pipeline - Examples\n";
    std::cout << "  Bilişsel RF Spektrum Haritalama Sistemi\n";
    std::cout << "═══════════════════════════════════════════════════════════\n";
    
    try {
        // Run examples
        example1_basic_monitoring();
        example2_fhss_detection();
        example3_realtime_callback();
        
        std::cout << "\n✅ All examples completed successfully!\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
