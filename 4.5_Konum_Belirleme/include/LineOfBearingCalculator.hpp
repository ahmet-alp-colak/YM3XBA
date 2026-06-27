/**
 * @file LineOfBearingCalculator.hpp
 * @brief Line of Bearing (LOB) calculation from station + AoA
 * @details Converts geodetic station position and azimuth to parametric line
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef GEOLOCATION_LINE_OF_BEARING_CALCULATOR_HPP
#define GEOLOCATION_LINE_OF_BEARING_CALCULATOR_HPP

#include "DataStructures.hpp"
#include "GeodeticUtils.hpp"

namespace CognitiveRF {
namespace Geolocation {

/**
 * @class LineOfBearingCalculator
 * @brief LOB (Kerteriz Hattı) hesaplayıcı
 * @details İstasyon konumu + AoA → Parametrik doğru denklemi
 */
class LineOfBearingCalculator {
public:
    /**
     * @brief AoA ölçümünden LOB oluştur
     * @param measurement AoA measurement (station + angle)
     * @param reference_point Lokal Kartezyen origin (tipik: istasyon 1)
     * @return LineOfBearing yapısı
     */
    static LineOfBearing createLOB(
        const AoAMeasurement& measurement,
        const StationInfo& reference_point
    );
    
    /**
     * @brief LOB doğrusunu belirtilen mesafeye kadar uzat
     * @param lob Line of Bearing
     * @param distance_meters Uzatma mesafesi (metre)
     * @return GeoCoordinate (uç nokta)
     */
    static GeoCoordinate extendLOB(
        const LineOfBearing& lob,
        double distance_meters
    );
    
    /**
     * @brief Bir noktanın LOB'a dik uzaklığını hesapla
     * @param lob Line of Bearing
     * @param point Test noktası (Kartezyen)
     * @return Dik uzaklık (metre)
     */
    static double perpendicularDistance(
        const LineOfBearing& lob,
        double point_x,
        double point_y
    );
    
    /**
     * @brief İki LOB'un paralel olup olmadığını kontrol et
     * @param lob1 Birinci LOB
     * @param lob2 İkinci LOB
     * @param tolerance Paralel toleransı (derece)
     * @return true = paralel
     */
    static bool areParallel(
        const LineOfBearing& lob1,
        const LineOfBearing& lob2,
        double tolerance = 5.0
    );
    
    /**
     * @brief LOB'u validate et
     * @param lob Line of Bearing
     * @param config Geolocation config
     * @return true = geçerli
     */
    static bool validateLOB(
        const LineOfBearing& lob,
        const GeolocationConfig& config
    );
    
private:
    /**
     * @brief Azimut açısını yön vektörüne çevir
     * @param azimuth_degrees Azimut (derece, True North)
     * @return tuple<dx, dy> (normalize edilmiş)
     */
    static std::tuple<double, double> azimuthToDirection(double azimuth_degrees);
};

} // namespace Geolocation
} // namespace CognitiveRF

#endif // GEOLOCATION_LINE_OF_BEARING_CALCULATOR_HPP
