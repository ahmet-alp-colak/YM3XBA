/**
 * @file GeolocationEngine.cpp
 * @brief Implementation of geolocation engine
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "../include/GeolocationEngine.hpp"
#include <chrono>
#include <algorithm>

namespace CognitiveRF {
namespace Geolocation {

GeolocationEngine::GeolocationEngine(const GeolocationConfig& config)
    : config_(config)
{
    // Reference station ilk ölçümde ayarlanacak
}

GeolocationResult GeolocationEngine::performGeolocation(
    const std::vector<AoAMeasurement>& measurements
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    GeolocationResult result;
    result.timestamp_ms = measurements.empty() ? 0 : measurements[0].timestamp_ms;
    
    // Validate measurements
    std::string error_msg;
    if (!validateMeasurements(measurements, error_msg)) {
        result.is_valid = false;
        result.validation_notes = error_msg;
        statistics_.failed_fixes++;
        return result;
    }
    
    // Reference station'ı ayarla (birinci istasyon)
    if (reference_station_.station_id == 0) {
        reference_station_ = measurements[0].station;
    }
    
    // LOB'ları oluştur
    std::vector<LineOfBearing> lobs = createLOBs(measurements);
    result.lobs = lobs;
    result.num_stations_used = static_cast<uint32_t>(lobs.size());
    
    // Geometri analizi
    auto [geom_valid, intersection_angle, baseline] = CrossFixingSolver::analyzeGeometry(lobs);
    if (!geom_valid) {
        result.is_valid = false;
        result.validation_notes = "Poor geometry: angle=" + std::to_string(intersection_angle) +
                                 ", baseline=" + std::to_string(baseline) + "m";
        statistics_.failed_fixes++;
        return result;
    }
    
    // Cross-fixing: Initial estimate
    auto intersection = CrossFixingSolver::computeIntersection(lobs[0], lobs[1]);
    if (!intersection) {
        result.is_valid = false;
        result.validation_notes = "Cross-fixing failed: parallel LOBs";
        statistics_.failed_fixes++;
        return result;
    }
    
    // Least Squares optimization
    auto [converged, final_pos, rms, iterations] = LeastSquaresOptimizer::optimize(
        lobs, *intersection, config_
    );
    
    result.converged = converged;
    result.iteration_count = iterations;
    result.residual_rms_meters = rms;
    
    if (!converged) {
        result.is_valid = false;
        result.validation_notes = "Least Squares did not converge";
        statistics_.failed_fixes++;
        return result;
    }
    
    // Lokal Kartezyen → WGS84
    auto [final_x, final_y] = final_pos;
    auto [lat, lon] = GeodeticUtils::localCartesianToGeodetic(
        reference_station_.latitude_deg,
        reference_station_.longitude_deg,
        final_x, final_y
    );
    
    result.target_position = GeoCoordinate(lat, lon, rms);
    result.target_position.accuracy_meters = rms;
    
    // Uncertainty ellipse
    result.uncertainty = LeastSquaresOptimizer::computeUncertaintyEllipse(lobs, final_pos);
    
    // GDOP
    result.gdop = GeodeticUtils::computeGDOP(lobs);
    
    // Confidence score
    float angle_quality = std::sin(intersection_angle * GeodeticUtils::DEG_TO_RAD);
    float convergence_quality = converged ? 1.0f : 0.0f;
    float rms_quality = std::max(0.0f, 1.0f - rms / config_.max_residual_rms_meters);
    result.confidence = 0.4f * angle_quality + 0.3f * convergence_quality + 0.3f * rms_quality;
    
    result.is_valid = result.target_position.isValid() && (rms < config_.max_residual_rms_meters);
    
    if (result.is_valid) {
        statistics_.successful_fixes++;
    } else {
        statistics_.failed_fixes++;
        result.validation_notes = "RMS residual too high: " + std::to_string(rms) + "m";
    }
    
    // Processing time
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    updateStatistics(result, elapsed_ms);
    
    return result;
}

void GeolocationEngine::updateConfig(const GeolocationConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

GeolocationStatistics GeolocationEngine::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return statistics_;
}

void GeolocationEngine::resetStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    statistics_.reset();
}

bool GeolocationEngine::validateMeasurements(
    const std::vector<AoAMeasurement>& measurements,
    std::string& error_msg
) {
    if (measurements.size() < 2) {
        error_msg = "Insufficient measurements (need >= 2)";
        return false;
    }
    
    // Timestamp kontrolü
    uint64_t max_diff = 0;
    for (size_t i = 1; i < measurements.size(); i++) {
        uint64_t diff = GeodeticUtils::computeTimestampDiff(
            measurements[0].timestamp_ms, measurements[i].timestamp_ms);
        max_diff = std::max(max_diff, diff);
    }
    
    if (max_diff > config_.max_timestamp_diff_ms) {
        error_msg = "Timestamp difference too large: " + std::to_string(max_diff) + "ms";
        return false;
    }
    
    // İstasyon mesafe kontrolü
    for (size_t i = 1; i < measurements.size(); i++) {
        double dist = GeodeticUtils::haversineDistance(
            measurements[0].station.latitude_deg, measurements[0].station.longitude_deg,
            measurements[i].station.latitude_deg, measurements[i].station.longitude_deg
        );
        
        if (dist > config_.max_station_distance_km * 1000.0) {
            error_msg = "Station distance too large: " + std::to_string(dist/1000.0) + "km";
            return false;
        }
    }
    
    return true;
}

std::vector<LineOfBearing> GeolocationEngine::createLOBs(
    const std::vector<AoAMeasurement>& measurements
) {
    std::vector<LineOfBearing> lobs;
    
    for (const auto& meas : measurements) {
        LineOfBearing lob = LineOfBearingCalculator::createLOB(meas, reference_station_);
        
        if (LineOfBearingCalculator::validateLOB(lob, config_)) {
            lobs.push_back(lob);
        }
    }
    
    return lobs;
}

void GeolocationEngine::updateStatistics(const GeolocationResult& result, double processing_time_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    statistics_.total_computations++;
    
    // Processing time
    statistics_.avg_processing_time_ms = 
        (statistics_.avg_processing_time_ms * (statistics_.total_computations - 1) + processing_time_ms) /
        statistics_.total_computations;
    
    statistics_.min_processing_time_ms = std::min(statistics_.min_processing_time_ms, processing_time_ms);
    statistics_.max_processing_time_ms = std::max(statistics_.max_processing_time_ms, processing_time_ms);
    
    if (result.is_valid) {
        // RMS
        statistics_.avg_residual_rms_meters =
            (statistics_.avg_residual_rms_meters * (statistics_.successful_fixes - 1) + result.residual_rms_meters) /
            statistics_.successful_fixes;
        
        // Confidence
        statistics_.avg_confidence =
            (statistics_.avg_confidence * (statistics_.successful_fixes - 1) + result.confidence) /
            statistics_.successful_fixes;
        
        // GDOP
        statistics_.avg_gdop =
            (statistics_.avg_gdop * (statistics_.successful_fixes - 1) + result.gdop) /
            statistics_.successful_fixes;
    }
}

} // namespace Geolocation
} // namespace CognitiveRF
