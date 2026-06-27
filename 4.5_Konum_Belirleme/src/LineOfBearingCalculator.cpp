/**
 * @file LineOfBearingCalculator.cpp
 * @brief Implementation of LOB calculator
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "../include/LineOfBearingCalculator.hpp"
#include <cmath>

namespace CognitiveRF {
namespace Geolocation {

LineOfBearing LineOfBearingCalculator::createLOB(
    const AoAMeasurement& measurement,
    const StationInfo& reference_point
) {
    LineOfBearing lob;
    
    // İstasyon bilgilerini kopyala
    lob.station = measurement.station;
    lob.bearing_degrees = measurement.angle_degrees;
    lob.timestamp_ms = measurement.timestamp_ms;
    lob.confidence = measurement.confidence;
    
    // İstasyon konumunu lokal Kartezyen'e çevir
    auto [x, y] = GeodeticUtils::geodeticToLocalCartesian(
        reference_point.latitude_deg,
        reference_point.longitude_deg,
        measurement.station.latitude_deg,
        measurement.station.longitude_deg
    );
    
    lob.parametric.origin_x = x;
    lob.parametric.origin_y = y;
    
    // Azimut → yön vektörü
    auto [dx, dy] = azimuthToDirection(measurement.angle_degrees);
    lob.parametric.direction_x = dx;
    lob.parametric.direction_y = dy;
    
    return lob;
}

GeoCoordinate LineOfBearingCalculator::extendLOB(
    const LineOfBearing& lob,
    double distance_meters
) {
    // Parametrik doğru üzerinde t=distance kadar ilerle
    const double end_x = lob.parametric.origin_x + 
                        lob.parametric.direction_x * distance_meters;
    const double end_y = lob.parametric.origin_y + 
                        lob.parametric.direction_y * distance_meters;
    
    // Lokal Kartezyen → WGS84
    // Not: Reference point'i bilmiyoruz, istasyon konumunu kullan
    auto [lat, lon] = GeodeticUtils::localCartesianToGeodetic(
        lob.station.latitude_deg,
        lob.station.longitude_deg,
        end_x - lob.parametric.origin_x,
        end_y - lob.parametric.origin_y
    );
    
    return GeoCoordinate(lat, lon);
}

double LineOfBearingCalculator::perpendicularDistance(
    const LineOfBearing& lob,
    double point_x,
    double point_y
) {
    // Parametrik doğru: P = O + t*D
    // Dik uzaklık: ||(P - O) - ((P - O)·D)*D||
    
    const double ox = lob.parametric.origin_x;
    const double oy = lob.parametric.origin_y;
    const double dx = lob.parametric.direction_x;
    const double dy = lob.parametric.direction_y;
    
    // Vector from origin to point
    const double vx = point_x - ox;
    const double vy = point_y - oy;
    
    // Dot product (projection)
    const double dot = vx * dx + vy * dy;
    
    // Perpendicular component
    const double perp_x = vx - dot * dx;
    const double perp_y = vy - dot * dy;
    
    // Distance
    return std::sqrt(perp_x * perp_x + perp_y * perp_y);
}

bool LineOfBearingCalculator::areParallel(
    const LineOfBearing& lob1,
    const LineOfBearing& lob2,
    double tolerance
) {
    const double angle_diff = GeodeticUtils::computeIntersectionAngle(
        lob1.bearing_degrees,
        lob2.bearing_degrees
    );
    
    // Paralel ise açı farkı ~0° veya ~180°
    return (angle_diff < tolerance) || (angle_diff > (180.0 - tolerance));
}

bool LineOfBearingCalculator::validateLOB(
    const LineOfBearing& lob,
    const GeolocationConfig& config
) {
    // Temel kontroller
    if (lob.confidence < 0.0f || lob.confidence > 1.0f) {
        return false;
    }
    
    if (lob.bearing_degrees < 0.0f || lob.bearing_degrees >= 360.0f) {
        return false;
    }
    
    if (!lob.station.latitude_deg || !lob.station.longitude_deg) {
        return false;
    }
    
    // Yön vektörü normalize kontrolü
    const double dx = lob.parametric.direction_x;
    const double dy = lob.parametric.direction_y;
    const double magnitude = std::sqrt(dx * dx + dy * dy);
    
    if (std::abs(magnitude - 1.0) > 0.01) {
        return false;  // Normalize edilmemiş
    }
    
    return true;
}

std::tuple<double, double> LineOfBearingCalculator::azimuthToDirection(double azimuth_degrees) {
    // True North (0°) = +Y (North)
    // East (90°) = +X (East)
    // Trigonometrik: 0° = +X, 90° = +Y
    // Azimut: 0° = +Y, 90° = +X
    // Dönüşüm: azimut_rad = (90 - azimuth) * DEG_TO_RAD
    
    const double azimuth_rad = azimuth_degrees * GeodeticUtils::DEG_TO_RAD;
    
    // dx = sin(azimut), dy = cos(azimut)
    const double dx = std::sin(azimuth_rad);
    const double dy = std::cos(azimuth_rad);
    
    // Normalize (zaten normalize olmalı ama emin ol)
    const double magnitude = std::sqrt(dx * dx + dy * dy);
    
    return std::make_tuple(dx / magnitude, dy / magnitude);
}

} // namespace Geolocation
} // namespace CognitiveRF
