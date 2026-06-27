/**
 * @file GeodeticUtils.hpp
 * @brief Geodetic utility functions for WGS84 coordinate system
 * @details Implements Vincenty's formulae for distance/azimuth calculations
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef GEOLOCATION_GEODETIC_UTILS_HPP
#define GEOLOCATION_GEODETIC_UTILS_HPP

#include "DataStructures.hpp"
#include <cmath>
#include <tuple>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace CognitiveRF {
namespace Geolocation {

/**
 * @class GeodeticUtils
 * @brief WGS84 jeodezik hesaplamalar
 * @details Vincenty formülleri ile yüksek hassasiyetli hesaplamalar
 */
class GeodeticUtils {
public:
    // WGS84 sabitleri
    static constexpr double WGS84_A = 6378137.0;              ///< Büyük yarı eksen (metre)
    static constexpr double WGS84_F = 1.0 / 298.257223563;    ///< Basıklık (flattening)
    static constexpr double WGS84_B = WGS84_A * (1.0 - WGS84_F); ///< Küçük yarı eksen
    static constexpr double WGS84_E2 = 2.0 * WGS84_F - WGS84_F * WGS84_F; ///< e²
    
    static constexpr double DEG_TO_RAD = M_PI / 180.0;
    static constexpr double RAD_TO_DEG = 180.0 / M_PI;
    
    /**
     * @brief İki coğrafi nokta arası mesafe ve azimut (Vincenty's Direct Formula)
     * @param lat1 Başlangıç enlemi (derece)
     * @param lon1 Başlangıç boylamı (derece)
     * @param lat2 Bitiş enlemi (derece)
     * @param lon2 Bitiş boylamı (derece)
     * @return tuple<distance_meters, forward_azimuth_deg, back_azimuth_deg>
     */
    static std::tuple<double, double, double> vincentyInverse(
        double lat1, double lon1,
        double lat2, double lon2
    );
    
    /**
     * @brief Başlangıç noktası + azimut + mesafe → hedef nokta (Vincenty's Direct Formula)
     * @param lat Başlangıç enlemi (derece)
     * @param lon Başlangıç boylamı (derece)
     * @param azimuth Azimut açısı (derece, True North)
     * @param distance Mesafe (metre)
     * @return tuple<target_lat_deg, target_lon_deg, back_azimuth_deg>
     */
    static std::tuple<double, double, double> vincentyDirect(
        double lat, double lon,
        double azimuth,
        double distance
    );
    
    /**
     * @brief WGS84 koordinatları lokal UTM/Kartezyen düzleme dönüştür
     * @param ref_lat Referans enlem (derece) - origin
     * @param ref_lon Referans boylam (derece) - origin
     * @param lat Hedef enlem (derece)
     * @param lon Hedef boylam (derece)
     * @return tuple<x_meters, y_meters> (East, North)
     */
    static std::tuple<double, double> geodeticToLocalCartesian(
        double ref_lat, double ref_lon,
        double lat, double lon
    );
    
    /**
     * @brief Lokal Kartezyen koordinatları WGS84'e dönüştür
     * @param ref_lat Referans enlem (derece) - origin
     * @param ref_lon Referans boylam (derece) - origin
     * @param x_meters X koordinatı (metre, East)
     * @param y_meters Y koordinatı (metre, North)
     * @return tuple<lat_deg, lon_deg>
     */
    static std::tuple<double, double> localCartesianToGeodetic(
        double ref_lat, double ref_lon,
        double x_meters, double y_meters
    );
    
    /**
     * @brief İki LOB arasındaki kesişim açısını hesapla
     * @param azimuth1 Birinci azimut (derece)
     * @param azimuth2 İkinci azimut (derece)
     * @return Kesişim açısı (derece) [0, 180]
     */
    static double computeIntersectionAngle(double azimuth1, double azimuth2);
    
    /**
     * @brief GDOP (Geometric Dilution of Precision) hesapla
     * @param lobs LOB vektörleri
     * @return GDOP değeri (düşük = iyi)
     */
    static float computeGDOP(const std::vector<LineOfBearing>& lobs);
    
    /**
     * @brief İki zaman etiketi arasındaki farkı hesapla
     * @param t1_ms Zaman 1 (milisaniye)
     * @param t2_ms Zaman 2 (milisaniye)
     * @return Mutlak fark (milisaniye)
     */
    static uint64_t computeTimestampDiff(uint64_t t1_ms, uint64_t t2_ms) {
        return (t1_ms > t2_ms) ? (t1_ms - t2_ms) : (t2_ms - t1_ms);
    }
    
    /**
     * @brief Azimut açısını normalize et [0, 360)
     * @param azimuth Açı (derece)
     * @return Normalize edilmiş açı [0, 360)
     */
    static double normalizeAzimuth(double azimuth);
    
    /**
     * @brief Haversine formülü ile basit mesafe hesaplama
     * @param lat1 Enlem 1 (derece)
     * @param lon1 Boylam 1 (derece)
     * @param lat2 Enlem 2 (derece)
     * @param lon2 Boylam 2 (derece)
     * @return Mesafe (metre)
     * @note Vincenty'den daha hızlı ama daha az hassas
     */
    static double haversineDistance(double lat1, double lon1, double lat2, double lon2);
    
private:
    /**
     * @brief İki açı arasındaki farkı hesapla [-180, 180]
     */
    static double angleDifference(double angle1, double angle2);
};

} // namespace Geolocation
} // namespace CognitiveRF

#endif // GEOLOCATION_GEODETIC_UTILS_HPP
