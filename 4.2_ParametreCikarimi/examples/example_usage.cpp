/**
 * @file example_usage.cpp
 * @brief Example usage of Cognitive RF Parameter Extraction Pipeline
 * @details Demonstrates complete workflow from IQ data to decoded signal
 * 
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "ParameterExtractionPipeline.hpp"
#include "PersistenceLogger.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <random>

using namespace CognitiveRF;

/**
 * @brief Generate synthetic IQ data for testing
 * @param frequency Signal frequency offset from center
 * @param sample_rate Sampling rate
 * @param num_samples Number of samples to generate
 * @return Synthetic IQ buffer
 */
IQDataBuffer generateSyntheticIQ(double frequency, double sample_rate, size_t num_samples) {
    IQDataBuffer buffer;
    buffer.center_freq_hz = 435000000;  // 435 MHz
    buffer.sample_rate_hz = static_cast<uint32_t>(sample_rate);
    buffer.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    buffer.iq_data.resize(num_samples);
    
    // Generate QPSK-like signal with noise
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> noise(0.0f, 0.1f);
    
    for (size_t n = 0; n < num_samples; ++n) {
        double phase = 2.0 * M_PI * frequency * static_cast<double>(n) / sample_rate;
        
        // QPSK symbols (simplified)
        float symbol_i = (n % 4 < 2) ? 1.0f : -1.0f;
        float symbol_q = ((n % 4) % 2 == 0) ? 1.0f : -1.0f;
        
        // Add carrier and noise
        float i = symbol_i * std::cos(phase) + noise(gen);
        float q = symbol_q * std::sin(phase) + noise(gen);
        
        buffer.iq_data[n] = std::complex<float>(i, q);
    }
    
    return buffer;
}

/**
 * @brief Print decoded signal information
 */
void printDecodedSignal(const DecodedSignal& signal) {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           DECODED SIGNAL PARAMETERS                        ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    
    std::cout << "║ Timestamp:      " << std::setw(40) << signal.timestamp_us << " µs ║\n";
    std::cout << "║ Frequency:      " << std::setw(35) << std::fixed << std::setprecision(2) 
              << signal.carrier_frequency_hz / 1e6 << " MHz ║\n";
    std::cout << "║ Bandwidth:      " << std::setw(35) << signal.bandwidth_hz / 1e3 << " kHz ║\n";
    
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ RSSI:           " << std::setw(35) << signal.rssi_dbm << " dBm ║\n";
    std::cout << "║ SNR:            " << std::setw(35) << signal.snr_db << " dB  ║\n";
    std::cout << "║ Baud Rate:      " << std::setw(35) << signal.baud_rate_hz << " Hz  ║\n";
    
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ Modulation:     " << std::setw(40) << ModulationTypeToString(signal.modulation) << " ║\n";
    std::cout << "║ Protocol:       " << std::setw(40) << ProtocolTypeToString(signal.protocol) << " ║\n";
    std::cout << "║ Multiplexing:   " << std::setw(40) << MultiplexingTypeToString(signal.multiplexing) << " ║\n";
    std::cout << "║ Signal Class:   " << std::setw(40) << static_cast<int>(signal.signal_class) << " ║\n";
    
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ Confidence:     " << std::setw(34) << std::setprecision(1) 
              << signal.confidence * 100.0f << " %  ║\n";
    
    if (signal.fhss_suspected) {
        std::cout << "║ ⚠️  FHSS ALARM:  Frequency hopping detected!              ║\n";
    }
    if (signal.jamming_suspected) {
        std::cout << "║ ⚠️  JAMMING:     Electronic interference suspected!       ║\n";
    }
    
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
}

/**
 * @brief Main example program
 */
int main(int argc, char* argv[]) {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   Cognitive RF Spectrum Mapping System                     ║\n";
    std::cout << "║   Hybrid Parameter Extraction Pipeline - Example           ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    // ========================================================================
    // 1. Configure Pipeline
    // ========================================================================
    
    std::cout << "[1/5] Configuring pipeline...\n";
    
    PipelineConfig config;
    config.onnx_model_path = "models/rfconformer.onnx";  // Update with actual path
    config.sample_rate_hz = 20e6;  // 20 MSPS
    config.noise_floor_dbm = -100.0f;
    config.voting_window_size = 5;
    config.max_queue_size = 100;
    config.enable_cfo_correction = true;
    config.enable_temporal_voting = true;
    
    std::cout << "  ✓ Sample Rate: " << config.sample_rate_hz / 1e6 << " MSPS\n";
    std::cout << "  ✓ Voting Window: " << config.voting_window_size << " frames\n";
    std::cout << "  ✓ CFO Correction: " << (config.enable_cfo_correction ? "Enabled" : "Disabled") << "\n";
    
    // ========================================================================
    // 2. Initialize Pipeline
    // ========================================================================
    
    std::cout << "\n[2/5] Initializing pipeline...\n";
    
    ParameterExtractionPipeline pipeline(config);
    
    // Set callback for real-time notifications
    pipeline.setResultCallback([](const DecodedSignal& signal) {
        if (signal.fhss_suspected) {
            std::cout << "  🚨 FHSS ALERT at " << signal.carrier_frequency_hz / 1e6 << " MHz\n";
        }
    });
    
    pipeline.start();
    std::cout << "  ✓ Pipeline started\n";
    
    // ========================================================================
    // 3. Initialize Logger
    // ========================================================================
    
    std::cout << "\n[3/5] Initializing persistence logger...\n";
    
    LoggerConfig logger_config;
    logger_config.database_path = "signal_records.db";
    logger_config.batch_size = 50;
    logger_config.flush_interval_ms = 1000;
    
    PersistenceLogger logger(logger_config);
    logger.start();
    std::cout << "  ✓ Logger started (database: " << logger_config.database_path << ")\n";
    
    // ========================================================================
    // 4. Process Signals
    // ========================================================================
    
    std::cout << "\n[4/5] Processing signals...\n\n";
    
    // Simulate processing multiple signals
    const int NUM_SIGNALS = 5;
    
    for (int i = 0; i < NUM_SIGNALS; ++i) {
        std::cout << "Processing signal " << (i + 1) << "/" << NUM_SIGNALS << "...\n";
        
        // Generate synthetic IQ data
        double freq_offset = 1e6 * (i + 1);  // 1 MHz, 2 MHz, etc.
        IQDataBuffer iq_buffer = generateSyntheticIQ(freq_offset, config.sample_rate_hz, 1024);
        
        // Submit to pipeline
        if (pipeline.submitIQData(iq_buffer)) {
            std::cout << "  ✓ IQ data submitted (offset: " << freq_offset / 1e6 << " MHz)\n";
        } else {
            std::cout << "  ✗ Failed to submit IQ data (queue full?)\n";
        }
        
        // Wait for processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Get result
        DecodedSignal result;
        if (pipeline.getResultBlocking(result, 2000)) {
            printDecodedSignal(result);
            
            // Log to database
            logger.logSignal(result);
            std::cout << "  ✓ Logged to database\n";
        } else {
            std::cout << "  ✗ Timeout waiting for result\n";
        }
        
        std::cout << "\n" << std::string(60, '-') << "\n\n";
    }
    
    // ========================================================================
    // 5. Statistics and Cleanup
    // ========================================================================
    
    std::cout << "[5/5] Statistics and cleanup...\n\n";
    
    std::cout << "Pipeline Statistics:\n";
    std::cout << "  • Input queue size:  " << pipeline.getInputQueueSize() << "\n";
    std::cout << "  • Output queue size: " << pipeline.getOutputQueueSize() << "\n";
    std::cout << "  • Running:           " << (pipeline.isRunning() ? "Yes" : "No") << "\n";
    
    std::cout << "\nLogger Statistics:\n";
    std::cout << "  • Total logged:      " << logger.getTotalLogged() << " records\n";
    std::cout << "  • Pending:           " << logger.getPendingCount() << " records\n";
    
    // Flush and stop
    std::cout << "\nShutting down...\n";
    logger.flush();
    logger.stop();
    pipeline.stop();
    
    std::cout << "  ✓ Pipeline stopped\n";
    std::cout << "  ✓ Logger stopped\n";
    
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   Example completed successfully!                          ║\n";
    std::cout << "║   Check signal_records.db for logged data                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    return 0;
}
