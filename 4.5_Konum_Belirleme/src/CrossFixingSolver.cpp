/**
 * @file CrossFixingSolver.cpp
 * @brief Implementation of cross-fixing algorithm
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#include "../include/CrossFixingSolver.hpp"
#include "../include/GeodeticUtils.hpp"
#include <cmath>
#include <limits>

namespace CognitiveRF {
namespace Geolocation {

std::optional<std::tuple<double, double>> CrossFixingSolver::computeIntersection(
    const LineOfBearing& lob1,
    const LineOfBearing& lob2
) {
    // Paralel kontrolü
    if (LineOfBearingCalculator::areParallel(lob1, lob2, 5.0)) {
        return std::nullopt;  // Paralel doğrular kesişmez
    }
    
    // Parametrik kesişim çöz
    auto result = solveParametricIntersection(
        lob1.parametric.origin_x, lob1.parametric.origin_y,
        lob1.parametric.direction_x, lob1.parametric.direction_y,
        lob2.parametric.origin_x, lob2.parametric.origin_y,
        lob2.parametric.direction_x, lob2.parametric.direction_y
    );
    
    if (!result) {
        return std::nullopt;
    }
    
    auto [t1, t2] = *result;
    
    // Kesişim noktasını hesapla (her iki doğrudan da)
    const double x1 = lob1.parametric.origin_x + t1 * lob1.parametric.direction_x;
    const double y1 = lob1.parametric.origin_y + t1 * lob1.parametric.direction_y;
    
    const double x2 = lob2.parametric.origin_x + t2 * lob2.parametric.direction_x;
    const double y2 = lob2.parametric.origin_y + t2 * lob2.parametric.direction_y;
    
    // Ortalama al (sayısal hata için)
    const double x = (x1 + x2) / 2.0;
    const double y = (y1 + y2) / 2.0;
    
    return std::make_tuple(x, y);
}

std::optional<std::tuple<double, double>> CrossFixingSolver::computeAverageIntersection(
    const std::vector<LineOfBearing>& lobs
) {
    if (lobs.size() < 2) {
        return std::nullopt;
    }
    
    if (lobs.size() == 2) {
        return computeIntersection(lobs[0], lobs[1]);
    }
    
    // N > 2: Tüm çiftlerin kesişimlerinin ortalaması
    std::vector<std::tuple<double, double>> intersections;
    
    for (size_t i = 0; i < lobs.size(); i++) {
        for (size_t j = i + 1; j < lobs.size(); j++) {
            auto intersection = computeIntersection(lobs[i], lobs[j]);
            if (intersection) {
                intersections.push_back(*intersection);
            }
        }
    }
    
    if (intersections.empty()) {
        return std::nullopt;
    }
    
    // Ortalama hesapla
    double sum_x = 0.0, sum_y = 0.0;
    for (const auto& [x, y] : intersections) {
        sum_x += x;
        sum_y += y;
    }
    
    return std::make_tuple(sum_x / intersections.size(),
                          sum_y / intersections.size());
}

float CrossFixingSolver::evaluateIntersectionQuality(
    const LineOfBearing& lob1,
    const LineOfBearing& lob2,
    const std::tuple<double, double>& intersection
) {
    // Kalite faktörleri:
    // 1. Kesişim açısı (90° = ideal)
    // 2. LOB güven skorları
    // 3. Kesişim noktasının istasyonlara uzaklığı
    
    const double angle = GeodeticUtils::computeIntersectionAngle(
        lob1.bearing_degrees, lob2.bearing_degrees);
    
    // Açı kalitesi: sin(angle) [0, 1]
    const double angle_rad = angle * GeodeticUtils::DEG_TO_RAD;
    const float angle_quality = static_cast<float>(std::sin(angle_rad));
    
    // Güven skoru ortalaması
    const float confidence = (lob1.confidence + lob2.confidence) / 2.0f;
    
    // Mesafe kontrolü (çok uzak kesişimler kötü)
    auto [x, y] = intersection;
    const double d1 = std::sqrt(
        std::pow(x - lob1.parametric.origin_x, 2) +
        std::pow(y - lob1.parametric.origin_y, 2)
    );
    const double d2 = std::sqrt(
        std::pow(x - lob2.parametric.origin_x, 2) +
        std::pow(y - lob2.parametric.origin_y, 2)
    );
    
    const double max_distance = 100000.0;  // 100 km
    const float distance_quality = 1.0f - static_cast<float>(
        std::min(std::max(d1, d2), max_distance) / max_distance
    );
    
    // Ağırlıklı ortalama
    return 0.5f * angle_quality + 0.3f * confidence + 0.2f * distance_quality;
}

std::tuple<bool, double, double> CrossFixingSolver::analyzeGeometry(
    const std::vector<LineOfBearing>& lobs
) {
    if (lobs.size() < 2) {
        return std::make_tuple(false, 0.0, 0.0);
    }
    
    // İki istasyon arası mesafe (baseline)
    const double dx = lobs[1].parametric.origin_x - lobs[0].parametric.origin_x;
    const double dy = lobs[1].parametric.origin_y - lobs[0].parametric.origin_y;
    const double baseline = std::sqrt(dx * dx + dy * dy);
    
    // Kesişim açısı
    const double angle = GeodeticUtils::computeIntersectionAngle(
        lobs[0].bearing_degrees, lobs[1].bearing_degrees);
    
    // Geçerlilik: açı yeterli mi, baseline makul mü
    const bool is_valid = (angle >= 30.0 && angle <= 150.0) &&
                         (baseline >= 100.0 && baseline <= 100000.0);
    
    return std::make_tuple(is_valid, angle, baseline);
}

std::optional<std::tuple<double, double>> CrossFixingSolver::solveParametricIntersection(
    double o1x, double o1y, double d1x, double d1y,
    double o2x, double o2y, double d2x, double d2y
) {
    // Parametrik doğrular:
    // L1: (x, y) = (o1x, o1y) + t1 * (d1x, d1y)
    // L2: (x, y) = (o2x, o2y) + t2 * (d2x, d2y)
    //
    // Kesişim: o1x + t1*d1x = o2x + t2*d2x
    //          o1y + t1*d1y = o2y + t2*d2y
    //
    // Matris formu: [d1x  -d2x] [t1] = [o2x - o1x]
    //               [d1y  -d2y] [t2]   [o2y - o1y]
    
    const double det = d1x * (-d2y) - d1y * (-d2x);
    
    if (std::abs(det) < 1e-10) {
        return std::nullopt;  // Paralel veya çakışık
    }
    
    const double dx = o2x - o1x;
    const double dy = o2y - o1y;
    
    // Cramer's rule
    const double t1 = (dx * (-d2y) - dy * (-d2x)) / det;
    const double t2 = (d1x * dy - d1y * dx) / det;
    
    return std::make_tuple(t1, t2);
}

} // namespace Geolocation
} // namespace CognitiveRF
