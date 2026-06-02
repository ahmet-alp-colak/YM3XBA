/**
 * @file example_usage.cpp
 * @brief Complete usage example for Direction Finding Pipeline
 * @details Demonstrates DF processing with various scenarios
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.4 Yön Bulma (Direction Finding)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "DirectionFindingPipeline.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <cmath>
#include <vector>
#include <random>

using namespace CognitiveRF::DirectionFinding;

/**
 * @brief Generate simulated IQ data for testing
 */
std::vector<std::complex<float>> generateTestSignal(
    double freq_offset_hz,
    uint32_t num_samples,
    float snr_db
) {
    std::vector<std::complex<float>> iq_data;
    iq_data.reserve(num_samples);

    std::mt19937 gen(std::random_device{}());
    std::normal_distribution<float> noise_dist(0.0f, 1.0f);

    for (uint32_t i = 0; i < num_samples; i++) {
        // Generate tone with frequency offset
        float phase = 2.0f * M_PI * freq_offset_hz * i / 20e6f;
        float signal_i = std::cos(phase);
        float signal_q = std::sin(phase);

        // Add noise based on SNR
        float noise_power = std::pow(10.0f, -snr_db / 20.0f);
        float noise_i = noise_dist(gen) * noise_power;
        float noise_q = noise_dist(gen) * noise_power;

        iq_data.push_back(std::complex<float>(signal_i + noise_i, signal_q + noise_q));
    }

    return iq_data;
}

/**
 * @brief Scenario 1: Basic Direction Finding
 */
void scenario1_basicDF() {
    std::cout << "\n========================================\n";
    std::cout << "Scenario 1: Basic Direction Finding\n";
    std::cout << "========================================\n";

    // Configure pipeline
    DirectionFindingPipeline::PipelineConfig config;
    config.sample_rate_hz = 20000000;
    config.df_config.center_frequency_hz = 435e6;  // 435 MHz
    config.df_config.aoa_rms_tolerance_degrees = 10.0f;
    config.antenna_config.num_antennas = 4;
    config.antenna_config.switching_frequency_hz = 10000;  // 10 kHz

    DirectionFindingPipeline pipeline(config);

    // Set callback
    pipeline.setResultCallback([](const DirectionFindingResult& result) {
        if (result.is_valid) {
            std::cout << "[✓] Valid AoA: " << std::fixed << std::setprecision(2)
                      << result.angle_of_arrival_degrees << "° "
                      << "(confidence: " << result.aoa_confidence * 100 << "%)\n";
        } else {
            std::cout << "[✗] Invalid: " << result.validation_notes << "\n";
        }
    });

    // Start pipeline
    if (!pipeline.start()) {
        std::cerr << "Failed to start pipeline\n";
        return;
    }

    std::cout << "Pipeline started.\n";

    // Submit multiple test signals from different angles
    const float test_angles[] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f};
    const float snr_levels[] = {20.0f, 15.0f, 10.0f};

    for (float angle : test_angles) {
        for (float snr : snr_levels) {
            // Generate test signal
            float freq_offset = (angle / 360.0f) * 10e3;  // Simulate angle as freq offset
            auto iq_data = generateTestSignal(freq_offset, 1024, snr);

            // Submit
            if (pipeline.submitIQData(iq_data, 435e6, snr, -50.0f)) {
                std::cout << "Submitted signal: AoA_test=" << angle << "°, SNR=" << snr << " dB\n";

                // Get result
                DirectionFindingResult result;
                if (pipeline.getResult(result, 100)) {
                    std::cout << "  → Estimated AoA: " << std::fixed << std::setprecision(1)
                              << result.angle_of_arrival_degrees << "° "
                              << "(RMS error: " << result.aoa_rms_error_degrees << "°)\n";
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    // Get statistics
    const auto& stats = pipeline.getStatistics();
    std::cout << "\n[STATS] Processing Summary:\n";
    std::cout << "  Total processed:    " << stats.total_processed << "\n";
    std::cout << "  Valid estimates:    " << stats.valid_aoa_estimates << "\n";
    std::cout << "  Average confidence: " << std::fixed << std::setprecision(2)
              << stats.avg_aoa_confidence * 100 << "%\n";
    std::cout << "  Average RMS error:  " << stats.avg_aoa_error_degrees << "°\n";

    pipeline.stop();
    std::cout << "Pipeline stopped.\n";
}

/**
 * @brief Scenario 2: FHSS Signal Direction Finding
 */
void scenario2_fhssDF() {
    std::cout << "\n========================================\n";
    std::cout << "Scenario 2: FHSS Signal Direction Finding\n";
    std::cout << "========================================\n";

    DirectionFindingPipeline::PipelineConfig config;
    config.df_config.center_frequency_hz = 2.4e9;  // 2.4 GHz (Bluetooth/WiFi range)
    config.antenna_array.diameter_meters = 0.2f;    // 200 mm array

    DirectionFindingPipeline pipeline(config);
    pipeline.start();

    std::cout << "Simulating FHSS signal detection and direction finding...\n";

    // Simulate frequency hopping
    const float hopping_frequencies[] = {2400e6, 2410e6, 2420e6, 2430e6, 2440e6};

    for (int hop = 0; hop < 3; hop++) {
        for (float freq : hopping_frequencies) {
            auto iq_data = generateTestSignal(1e3, 512, 15.0f);

            if (pipeline.submitIQData(iq_data, freq, 15.0f, -45.0f)) {
                std::cout << "Hop " << hop + 1 << ": Freq=" << freq / 1e6 << " MHz";

                DirectionFindingResult result;
                if (pipeline.getResult(result, 50)) {
                    std::cout << " → AoA=" << std::fixed << std::setprecision(1)
                              << result.angle_of_arrival_degrees << "°\n";
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    pipeline.stop();
}

/**
 * @brief Scenario 3: Real-time Callback Processing
 */
void scenario3_realtimeCallbacks() {
    std::cout << "\n========================================\n";
    std::cout << "Scenario 3: Real-time Callback Processing\n";
    std::cout << "========================================\n";

    DirectionFindingPipeline::PipelineConfig config;
    config.enable_callbacks = true;

    DirectionFindingPipeline pipeline(config);

    // Set up tracking callback
    int valid_count = 0;
    pipeline.setResultCallback([&](const DirectionFindingResult& result) {
        if (result.is_valid) {
            valid_count++;
            std::cout << "[Callback-" << valid_count << "] "
                      << "AoA=" << std::fixed << std::setprecision(1)
                      << result.angle_of_arrival_degrees << "° "
                      << "| SNR=" << result.snr_db << " dB "
                      << "| Conf=" << result.aoa_confidence * 100.0f << "%\n";
        }
    });

    pipeline.start();

    // Submit continuous stream
    for (int i = 0; i < 5; i++) {
        auto iq_data = generateTestSignal(i * 2000.0f, 256, 12.0f + i);
        pipeline.submitIQData(iq_data, 435e6, 12.0f + i, -50.0f + i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Allow time for callbacks
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    pipeline.stop();
    std::cout << "Callback test completed (" << valid_count << " valid results).\n";
}

/**
 * @brief Scenario 4: Direction Finding Accuracy vs SNR
 */
void scenario4_accuracyVsSNR() {
    std::cout << "\n========================================\n";
    std::cout << "Scenario 4: Accuracy vs SNR Analysis\n";
    std::cout << "========================================\n";

    DirectionFindingPipeline::PipelineConfig config;
    DirectionFindingPipeline pipeline(config);
    pipeline.start();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(10) << "SNR (dB)" << " | "
              << std::setw(12) << "Est. AoA (°)" << " | "
              << std::setw(12) << "RMS Error (°)" << " | "
              << std::setw(12) << "Confidence" << "\n";
    std::cout << std::string(60, '-') << "\n";

    // Test range of SNR levels
    for (float snr = -5.0f; snr <= 25.0f; snr += 5.0f) {
        auto iq_data = generateTestSignal(1000.0f, 1024, snr);

        if (pipeline.submitIQData(iq_data, 435e6, snr, -50.0f)) {
            DirectionFindingResult result;
            if (pipeline.getResult(result, 100)) {
                std::cout << std::setw(10) << snr << " | "
                          << std::setw(12) << result.angle_of_arrival_degrees << " | "
                          << std::setw(12) << result.aoa_rms_error_degrees << " | "
                          << std::setw(12) << result.aoa_confidence * 100.0f << "%\n";
            }
        }
    }

    pipeline.stop();
}

/**
 * @brief Main entry point
 */
int main() {
    std::cout << "\n";
    std::cout << "===============================================================\n";
    std::cout << "  Direction Finding (DF) Pipeline - Test Scenarios\n";
    std::cout << "  Bilişsel RF Spektrum Haritalama Sistemi\n";
    std::cout << "===============================================================\n";

    try {
        scenario1_basicDF();
        scenario2_fhssDF();
        scenario3_realtimeCallbacks();
        scenario4_accuracyVsSNR();

        std::cout << "\n===============================================================\n";
        std::cout << "✓ All scenarios completed successfully!\n";
        std::cout << "===============================================================\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
