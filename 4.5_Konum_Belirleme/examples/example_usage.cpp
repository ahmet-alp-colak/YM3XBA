/**
 * @file example_usage.cpp
 * @brief Example usage of Geolocation module
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * Scenarios:
 * 1. Two-station cross-fixing
 * 2. Pipeline mode with callbacks
 * 3. Statistics and performance monitoring
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "../include/GeolocationPipeline.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>

using namespace CognitiveRF::Geolocation;

// ============================================================================
// Helper Functions
// ============================================================================

void printResult(const GeolocationResult& result) {
    std::cout << "\n========================================\n";
    std::cout << "Geolocation Result:\n";
    std::cout << "========================================\n";
    std::cout << "Valid: " << (result.is_valid ? "YES" : "NO") << "\n";
    
    if (result.is_valid) {
        std::cout << std::fixed << std::setprecision(6);
        std::cout << "Target Position:\n";
        std::cout << "  Latitude:  " << result.target_position.latitude_deg << "°\n";
        std::cout << "  Longitude: " << result.target_position.longitude_deg << "°\n";
        std::cout << "  Accuracy:  " << result.target_position.accuracy_meters << " m\n";
        
        std::cout << "\nQuality Metrics:\n";
        std::cout << "  Confidence:     " << (result.confidence * 100.0f) << "%\n";
        std::cout << "  GDOP:           " << result.gdop << "\n";
        std::cout << "  RMS Residual:   " << result.residual_rms_meters << " m\n";
        std::cout << "  Converged:      " << (result.converged ? "YES" : "NO") << "\n";
        std::cout << "  Iterations:     " << result.iteration_count << "\n";
        
        std::cout << "\nUncertainty Ellipse:\n";
        std::cout << "  Semi-major: " << result.uncertainty.semi_major_axis_meters << " m\n";
        std::cout << "  Semi-minor: " << result.uncertainty.semi_minor_axis_meters << " m\n";
        std::cout << "  Orientation: " << result.uncertainty.orientation_degrees << "°\n";
        
        std::cout << "\nStations Used: " << result.num_stations_used << "\n";
    } else {
        std::cout << "Validation Notes: " << result.validation_notes << "\n";
    }
    std::cout << "========================================\n\n";
}

void printStatistics(const GeolocationStatistics& stats) {
    std::cout << "\n========================================\n";
    std::cout << "Performance Statistics:\n";
    std::cout << "========================================\n";
    std::cout << "Total Computations: " << stats.total_computations << "\n";
    std::cout << "Successful Fixes:   " << stats.successful_fixes << "\n";
    std::cout << "Failed Fixes:       " << stats.failed_fixes << "\n";
    
    if (stats.successful_fixes > 0) {
        std::cout << "\nProcessing Time:\n";
        std::cout << "  Average: " << stats.avg_processing_time_ms << " ms\n";
        std::cout << "  Min:     " << stats.min_processing_time_ms << " ms\n";
        std::cout << "  Max:     " << stats.max_processing_time_ms << " ms\n";
        
        std::cout << "\nQuality Metrics (Average):\n";
        std::cout << "  RMS Residual: " << stats.avg_residual_rms_meters << " m\n";
        std::cout << "  Confidence:   " << (stats.avg_confidence * 100.0f) << "%\n";
        std::cout << "  GDOP:         " << stats.avg_gdop << "\n";
    }
    std::cout << "========================================\n\n";
}

// ============================================================================
// Scenario 1: Two-Station Cross-Fixing
// ============================================================================

void scenario1_twoStationCrossFixing() {
    std::cout << "\n\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    std::cout << "  SCENARIO 1: Two-Station Cross-Fixing\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    
    // Create geolocation engine
    GeolocationConfig config;
    config.max_iterations = 100;
    config.convergence_tolerance_meters = 0.5f;
    
    GeolocationEngine engine(config);
    
    // Station 1 (Ankara)
    StationInfo station1(1, 39.9334, 32.8597, "Ankara_ED1");
    
    // Station 2 (İstanbul)
    StationInfo station2(2, 41.0082, 28.9784, "Istanbul_ED2");
    
    // Simulated target: somewhere between (approx. Eskişehir)
    // Real target: 39.7767° N, 30.5206° E
    
    // AoA measurements (simulated)
    AoAMeasurement meas1;
    meas1.station = station1;
    meas1.angle_degrees = 280.0f;  // ~West from Ankara
    meas1.confidence = 0.85f;
    meas1.rms_error_degrees = 10.0f;
    meas1.timestamp_ms = 1000000;
    meas1.center_frequency_hz = 435e6;
    
    AoAMeasurement meas2;
    meas2.station = station2;
    meas2.angle_degrees = 110.0f;  // ~East-Southeast from Istanbul
    meas2.confidence = 0.80f;
    meas2.rms_error_degrees = 10.0f;
    meas2.timestamp_ms = 1000050;  // 50ms difference
    meas2.center_frequency_hz = 435e6;
    
    std::vector<AoAMeasurement> measurements = {meas1, meas2};
    
    // Perform geolocation
    std::cout << "\nPerforming two-station cross-fixing...\n";
    GeolocationResult result = engine.performGeolocation(measurements);
    
    printResult(result);
    printStatistics(engine.getStatistics());
}

// ============================================================================
// Scenario 2: Pipeline Mode with Callbacks
// ============================================================================

void scenario2_pipelineMode() {
    std::cout << "\n\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    std::cout << "  SCENARIO 2: Pipeline Mode with Real-time Callbacks\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    
    // Configure pipeline
    GeolocationPipeline::PipelineConfig config;
    config.geolocation_config.max_iterations = 50;
    config.input_queue_size = 50;
    config.result_queue_size = 25;
    
    GeolocationPipeline pipeline(config);
    
    // Set callback
    pipeline.setResultCallback([](const GeolocationResult& result) {
        if (result.is_valid) {
            std::cout << "[CALLBACK] Target located at: "
                     << std::fixed << std::setprecision(6)
                     << result.target_position.latitude_deg << "°N, "
                     << result.target_position.longitude_deg << "°E "
                     << "(Confidence: " << (result.confidence * 100.0f) << "%)\n";
        } else {
            std::cout << "[CALLBACK] Fix failed: " << result.validation_notes << "\n";
        }
    });
    
    // Start pipeline
    std::cout << "\nStarting pipeline...\n";
    pipeline.start();
    
    // Simulate multiple measurements
    StationInfo station1(1, 40.0, 30.0, "Station_A");
    StationInfo station2(2, 40.0, 32.0, "Station_B");
    
    std::cout << "\nSubmitting 5 measurement batches...\n\n";
    
    for (int i = 0; i < 5; i++) {
        AoAMeasurement meas1;
        meas1.station = station1;
        meas1.angle_degrees = 45.0f + i * 2.0f;  // Varying angles
        meas1.confidence = 0.75f + i * 0.05f;
        meas1.rms_error_degrees = 10.0f;
        meas1.timestamp_ms = 2000000 + i * 100;
        
        AoAMeasurement meas2;
        meas2.station = station2;
        meas2.angle_degrees = 315.0f - i * 2.0f;
        meas2.confidence = 0.80f;
        meas2.rms_error_degrees = 10.0f;
        meas2.timestamp_ms = 2000000 + i * 100 + 10;
        
        pipeline.submitMeasurements({meas1, meas2});
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Stop pipeline
    std::cout << "\nStopping pipeline...\n";
    pipeline.stop();
    
    printStatistics(pipeline.getStatistics());
}

// ============================================================================
// Scenario 3: Geometry Analysis
// ============================================================================

void scenario3_geometryAnalysis() {
    std::cout << "\n\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    std::cout << "  SCENARIO 3: Geometry Quality Analysis\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    
    StationInfo ref_station(0, 40.0, 30.0, "Reference");
    StationInfo station1(1, 40.0, 30.0, "Station_1");
    StationInfo station2(2, 40.0, 31.0, "Station_2");
    
    // Test different intersection angles
    std::vector<float> angles = {30.0f, 45.0f, 60.0f, 90.0f, 120.0f, 150.0f};
    
    std::cout << "\nTesting different intersection angles:\n";
    std::cout << "----------------------------------------\n";
    
    for (float base_angle : angles) {
        AoAMeasurement meas1;
        meas1.station = station1;
        meas1.angle_degrees = base_angle;
        meas1.confidence = 0.85f;
        meas1.timestamp_ms = 3000000;
        
        AoAMeasurement meas2;
        meas2.station = station2;
        meas2.angle_degrees = 180.0f;  // West
        meas2.confidence = 0.85f;
        meas2.timestamp_ms = 3000000;
        
        LineOfBearing lob1 = LineOfBearingCalculator::createLOB(meas1, ref_station);
        LineOfBearing lob2 = LineOfBearingCalculator::createLOB(meas2, ref_station);
        
        double intersection_angle = GeodeticUtils::computeIntersectionAngle(
            lob1.bearing_degrees, lob2.bearing_degrees);
        
        float gdop = GeodeticUtils::computeGDOP({lob1, lob2});
        
        std::cout << "Angle: " << std::setw(6) << base_angle << "° "
                 << "→ Intersection: " << std::setw(6) << std::fixed << std::setprecision(1) 
                 << intersection_angle << "° "
                 << "→ GDOP: " << std::setw(6) << std::setprecision(2) << gdop
                 << " (" << (intersection_angle >= 60 && intersection_angle <= 120 ? "GOOD" : "POOR") << ")\n";
    }
    
    std::cout << "\nRecommendation: Aim for 60°-120° intersection angles for best accuracy.\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    std::cout << "  Bilişsel RF Spektrum Haritalama Sistemi\n";
    std::cout << "  Module 4.5: Konum Belirleme (Geolocation)\n";
    std::cout << "  Example Usage Scenarios\n";
    std::cout << "████████████████████████████████████████████████████████████████\n";
    
    try {
        scenario1_twoStationCrossFixing();
        scenario2_pipelineMode();
        scenario3_geometryAnalysis();
        
        std::cout << "\n";
        std::cout << "████████████████████████████████████████████████████████████████\n";
        std::cout << "  All scenarios completed successfully!\n";
        std::cout << "████████████████████████████████████████████████████████████████\n";
        std::cout << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
