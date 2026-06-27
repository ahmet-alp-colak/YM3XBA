/**
 * @file GeodeticUtils.cpp
 * @brief Implementation of geodetic utility functions
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "../include/GeodeticUtils.hpp"
#include <cmath>
#include <algorithm>

namespace CognitiveRF {
namespace Geolocation {

// Vincenty's Inverse Formula
std::tuple<double, double, double> GeodeticUtils::vincentyInverse(
    double lat1, double lon1,
    double lat2, double lon2
) {
    // Dereceyi radyana çevir
    const double phi1 = lat1 * DEG_TO_RAD;
    const double phi2 = lat2 * DEG_TO_RAD;
    const double lambda1 = lon1 * DEG_TO_RAD;
    const double lambda2 = lon2 * DEG_TO_RAD;
    
    const double L = lambda2 - lambda1;
    const double U1 = std::atan((1.0 - WGS84_F) * std::tan(phi1));
    const double U2 = std::atan((1.0 - WGS84_F) * std::tan(phi2));
    
    const double sinU1 = std::sin(U1);
    const double cosU1 = std::cos(U1);
    const double sinU2 = std::sin(U2);
    const double cosU2 = std::cos(U2);
    
    double lambda = L;
    double lambda_prev;
    double cos_sq_alpha, sin_sigma, cos_sigma, sigma, sin_alpha, cos2_sigma_m;
    int iter_count = 0;
    const int max_iters = 100;
    const double convergence_threshold = 1e-12;
    
    do {
        const double sin_lambda = std::sin(lambda);
        const double cos_lambda = std::cos(lambda);
        
        sin_sigma = std::sqrt((cosU2 * sin_lambda) * (cosU2 * sin_lambda) +
                              (cosU1 * sinU2 - sinU1 * cosU2 * cos_lambda) *
                              (cosU1 * sinU2 - sinU1 * cosU2 * cos_lambda));
        
        if (sin_sigma == 0.0) {
            // Coincident points
            return std::make_tuple(0.0, 0.0, 0.0);
        }
        
        cos_sigma = sinU1 * sinU2 + cosU1 * cosU2 * cos_lambda;
        sigma = std::atan2(sin_sigma, cos_sigma);
        sin_alpha = cosU1 * cosU2 * sin_lambda / sin_sigma;
        cos_sq_alpha = 1.0 - sin_alpha * sin_alpha;
        
        cos2_sigma_m = cos_sigma - 2.0 * sinU1 * sinU2 / cos_sq_alpha;
        if (std::isnan(cos2_sigma_m)) {
            cos2_sigma_m = 0.0;  // Equatorial line
        }
        
        const double C = WGS84_F / 16.0 * cos_sq_alpha * (4.0 + WGS84_F * (4.0 - 3.0 * cos_sq_alpha));
        lambda_prev = lambda;
        lambda = L + (1.0 - C) * WGS84_F * sin_alpha *
                 (sigma + C * sin_sigma * (cos2_sigma_m + C * cos_sigma *
                 (-1.0 + 2.0 * cos2_sigma_m * cos2_sigma_m)));
        
        iter_count++;
    } while (std::abs(lambda - lambda_prev) > convergence_threshold && iter_count < max_iters);
    
    const double u_sq = cos_sq_alpha * (WGS84_A * WGS84_A - WGS84_B * WGS84_B) / (WGS84_B * WGS84_B);
    const double A = 1.0 + u_sq / 16384.0 * (4096.0 + u_sq * (-768.0 + u_sq * (320.0 - 175.0 * u_sq)));
    const double B = u_sq / 1024.0 * (256.0 + u_sq * (-128.0 + u_sq * (74.0 - 47.0 * u_sq)));
    
    const double delta_sigma = B * sin_sigma * (cos2_sigma_m + B / 4.0 *
        (cos_sigma * (-1.0 + 2.0 * cos2_sigma_m * cos2_sigma_m) -
         B / 6.0 * cos2_sigma_m * (-3.0 + 4.0 * sin_sigma * sin_sigma) *
         (-3.0 + 4.0 * cos2_sigma_m * cos2_sigma_m)));
    
    const double distance = WGS84_B * A * (sigma - delta_sigma);
    
    // Forward azimuth
    const double alpha1 = std::atan2(cosU2 * std::sin(lambda),
                                     cosU1 * sinU2 - sinU1 * cosU2 * std::cos(lambda));
    
    // Back azimuth
    const double alpha2 = std::atan2(cosU1 * std::sin(lambda),
                                     -sinU1 * cosU2 + cosU1 * sinU2 * std::cos(lambda));
    
    return std::make_tuple(distance,
                          normalizeAzimuth(alpha1 * RAD_TO_DEG),
                          normalizeAzimuth(alpha2 * RAD_TO_DEG));
}

// Vincenty's Direct Formula
std::tuple<double, double, double> GeodeticUtils::vincentyDirect(
    double lat, double lon,
    double azimuth,
    double distance
) {
    const double phi1 = lat * DEG_TO_RAD;
    const double lambda1 = lon * DEG_TO_RAD;
    const double alpha1 = azimuth * DEG_TO_RAD;
    const double s = distance;
    
    const double U1 = std::atan((1.0 - WGS84_F) * std::tan(phi1));
    const double sigma1 = std::atan2(std::tan(U1), std::cos(alpha1));
    const double sin_alpha = std::cos(U1) * std::sin(alpha1);
    const double cos_sq_alpha = 1.0 - sin_alpha * sin_alpha;
    const double u_sq = cos_sq_alpha * (WGS84_A * WGS84_A - WGS84_B * WGS84_B) / (WGS84_B * WGS84_B);
    
    const double A = 1.0 + u_sq / 16384.0 * (4096.0 + u_sq * (-768.0 + u_sq * (320.0 - 175.0 * u_sq)));
    const double B = u_sq / 1024.0 * (256.0 + u_sq * (-128.0 + u_sq * (74.0 - 47.0 * u_sq)));
    
    double sigma = s / (WGS84_B * A);
    double sigma_prev;
    int iter_count = 0;
    const int max_iters = 100;
    
    do {
        const double cos2_sigma_m = std::cos(2.0 * sigma1 + sigma);
        const double sin_sigma = std::sin(sigma);
        const double cos_sigma = std::cos(sigma);
        
        const double delta_sigma = B * sin_sigma * (cos2_sigma_m + B / 4.0 *
            (cos_sigma * (-1.0 + 2.0 * cos2_sigma_m * cos2_sigma_m) -
             B / 6.0 * cos2_sigma_m * (-3.0 + 4.0 * sin_sigma * sin_sigma) *
             (-3.0 + 4.0 * cos2_sigma_m * cos2_sigma_m)));
        
        sigma_prev = sigma;
        sigma = s / (WGS84_B * A) + delta_sigma;
        iter_count++;
    } while (std::abs(sigma - sigma_prev) > 1e-12 && iter_count < max_iters);
    
    const double cos2_sigma_m = std::cos(2.0 * sigma1 + sigma);
    const double sin_sigma = std::sin(sigma);
    const double cos_sigma = std::cos(sigma);
    
    const double phi2 = std::atan2(
        std::sin(U1) * cos_sigma + std::cos(U1) * sin_sigma * std::cos(alpha1),
        (1.0 - WGS84_F) * std::sqrt(sin_alpha * sin_alpha +
        (std::sin(U1) * sin_sigma - std::cos(U1) * cos_sigma * std::cos(alpha1)) *
        (std::sin(U1) * sin_sigma - std::cos(U1) * cos_sigma * std::cos(alpha1)))
    );
    
    const double lambda = std::atan2(
        sin_sigma * std::sin(alpha1),
        std::cos(U1) * cos_sigma - std::sin(U1) * sin_sigma * std::cos(alpha1)
    );
    
    const double C = WGS84_F / 16.0 * cos_sq_alpha * (4.0 + WGS84_F * (4.0 - 3.0 * cos_sq_alpha));
    const double L = lambda - (1.0 - C) * WGS84_F * sin_alpha *
                     (sigma + C * sin_sigma * (cos2_sigma_m + C * cos_sigma *
                     (-1.0 + 2.0 * cos2_sigma_m * cos2_sigma_m)));
    
    const double lambda2 = lambda1 + L;
    
    const double alpha2 = std::atan2(sin_alpha,
                                     -std::sin(U1) * sin_sigma + std::cos(U1) * cos_sigma * std::cos(alpha1));
    
    return std::make_tuple(phi2 * RAD_TO_DEG,
                          lambda2 * RAD_TO_DEG,
                          normalizeAzimuth(alpha2 * RAD_TO_DEG));
}

// Lokal Kartezyen dönüşümü (basitleştirilmiş)
std::tuple<double, double> GeodeticUtils::geodeticToLocalCartesian(
    double ref_lat, double ref_lon,
    double lat, double lon
) {
    // Basitleştirilmiş düzlem projeksiyon (kısa mesafeler için)
    const double lat_rad = lat * DEG_TO_RAD;
    const double ref_lat_rad = ref_lat * DEG_TO_RAD;
    
    const double delta_lat = lat - ref_lat;
    const double delta_lon = lon - ref_lon;
    
    // Metre başına derece
    const double meters_per_deg_lat = 111132.92 - 559.82 * std::cos(2.0 * ref_lat_rad) +
                                      1.175 * std::cos(4.0 * ref_lat_rad);
    const double meters_per_deg_lon = 111412.84 * std::cos(ref_lat_rad) -
                                      93.5 * std::cos(3.0 * ref_lat_rad);
    
    const double y = delta_lat * meters_per_deg_lat;  // North
    const double x = delta_lon * meters_per_deg_lon;  // East
    
    return std::make_tuple(x, y);
}

// Lokal Kartezyen → WGS84
std::tuple<double, double> GeodeticUtils::localCartesianToGeodetic(
    double ref_lat, double ref_lon,
    double x_meters, double y_meters
) {
    const double ref_lat_rad = ref_lat * DEG_TO_RAD;
    
    const double meters_per_deg_lat = 111132.92 - 559.82 * std::cos(2.0 * ref_lat_rad) +
                                      1.175 * std::cos(4.0 * ref_lat_rad);
    const double meters_per_deg_lon = 111412.84 * std::cos(ref_lat_rad) -
                                      93.5 * std::cos(3.0 * ref_lat_rad);
    
    const double delta_lat = y_meters / meters_per_deg_lat;
    const double delta_lon = x_meters / meters_per_deg_lon;
    
    return std::make_tuple(ref_lat + delta_lat, ref_lon + delta_lon);
}

// Kesişim açısı
double GeodeticUtils::computeIntersectionAngle(double azimuth1, double azimuth2) {
    double diff = std::abs(angleDifference(azimuth1, azimuth2));
    return std::min(diff, 180.0 - diff);
}

// GDOP hesaplama
float GeodeticUtils::computeGDOP(const std::vector<LineOfBearing>& lobs) {
    if (lobs.size() < 2) return 999.0f;  // Geçersiz
    
    // Basitleştirilmiş GDOP: kesişim açısına bağlı
    // İdeal: 90° → GDOP=1.0, Kötü: 0° veya 180° → GDOP yüksek
    
    float min_angle = 180.0f;
    for (size_t i = 0; i < lobs.size(); i++) {
        for (size_t j = i + 1; j < lobs.size(); j++) {
            float angle = static_cast<float>(computeIntersectionAngle(
                lobs[i].bearing_degrees, lobs[j].bearing_degrees));
            min_angle = std::min(min_angle, angle);
        }
    }
    
    // GDOP = 1 / sin(min_angle)
    const float min_angle_rad = min_angle * static_cast<float>(DEG_TO_RAD);
    const float sin_angle = std::sin(min_angle_rad);
    
    if (sin_angle < 0.01f) return 999.0f;  // Neredeyse paralel
    
    return 1.0f / sin_angle;
}

// Azimut normalize
double GeodeticUtils::normalizeAzimuth(double azimuth) {
    double result = std::fmod(azimuth, 360.0);
    if (result < 0.0) result += 360.0;
    return result;
}

// Haversine mesafe
double GeodeticUtils::haversineDistance(double lat1, double lon1, double lat2, double lon2) {
    const double phi1 = lat1 * DEG_TO_RAD;
    const double phi2 = lat2 * DEG_TO_RAD;
    const double delta_phi = (lat2 - lat1) * DEG_TO_RAD;
    const double delta_lambda = (lon2 - lon1) * DEG_TO_RAD;
    
    const double a = std::sin(delta_phi / 2.0) * std::sin(delta_phi / 2.0) +
                    std::cos(phi1) * std::cos(phi2) *
                    std::sin(delta_lambda / 2.0) * std::sin(delta_lambda / 2.0);
    
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    
    return WGS84_A * c;
}

// Açı farkı
double GeodeticUtils::angleDifference(double angle1, double angle2) {
    double diff = angle2 - angle1;
    while (diff > 180.0) diff -= 360.0;
    while (diff < -180.0) diff += 360.0;
    return diff;
}

} // namespace Geolocation
} // namespace CognitiveRF
