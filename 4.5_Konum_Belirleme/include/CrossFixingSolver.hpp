/**
 * @file CrossFixingSolver.hpp
 * @brief Cross-fixing algorithm for 2 LOB intersection
 * @details Geometric intersection of two lines of bearing
 *
 * System: Bilişsel RF Spektrum Haritalama Sistemi
 * Module: 4.5 Konum Belirleme (Geolocation)
 *
 * @author Senior C++ Software Architect
 * @date 2026
 */

#ifndef GEOLOCATION_CROSS_FIXING_SOLVER_HPP
#define GEOLOCATION_CROSS_FIXING_SOLVER_HPP

#include "DataStructures.hpp"
#include "LineOfBearingCalculator.hpp"
#include <vector>
#include <optional>

namespace CognitiveRF {
namespace Geolocation {

/**
 * @class CrossFixingSolver
 * @brief Çapraz Kesiştirme Çözücüsü
 * @details İki LOB doğrusunun kesişim noktasını hesaplar
 */
class CrossFixingSolver {
public:
    /**
     * @brief İki LOB'un kesişim noktasını hesapla
     * @param lob1 Birinci LOB
     * @param lob2 İkinci LOB
     * @return optional<tuple<x, y>> Kesişim noktası (Kartezyen metre)
     */
    static std::optional<std::tuple<double, double>> computeIntersection(
        const LineOfBearing& lob1,
        const LineOfBearing& lob2
    );
    
    /**
     * @brief Çoklu LOB'lardan ortalama kesişim hesapla
     * @param lobs LOB vektörü (N >= 2)
     * @return optional<tuple<x, y>> Ortalama kesişim noktası
     * @note İki LOB için direkt kesişim, N>2 için tüm çiftlerin ortalaması
     */
    static std::optional<std::tuple<double, double>> computeAverageIntersection(
        const std::vector<LineOfBearing>& lobs
    );
    
    /**
     * @brief Kesişimin kalitesini değerlendir
     * @param lob1 Birinci LOB
     * @param lob2 İkinci LOB
     * @param intersection Kesişim noktası
     * @return Kalite skoru [0.0, 1.0] (1.0 = mükemmel)
     */
    static float evaluateIntersectionQuality(
        const LineOfBearing& lob1,
        const LineOfBearing& lob2,
        const std::tuple<double, double>& intersection
    );
    
    /**
     * @brief Kesişim geometrisini analiz et
     * @param lobs LOB vektörü
     * @return tuple<is_valid, intersection_angle, baseline_distance>
     */
    static std::tuple<bool, double, double> analyzeGeometry(
        const std::vector<LineOfBearing>& lobs
    );

private:
    /**
     * @brief Parametrik doğru kesişimi (2D)
     * @details L1: P1 = O1 + t1*D1, L2: P2 = O2 + t2*D2
     * @return optional<tuple<t1, t2>> Parametreler
     */
    static std::optional<std::tuple<double, double>> solveParametricIntersection(
        double o1x, double o1y, double d1x, double d1y,
        double o2x, double o2y, double d2x, double d2y
    );
};

} // namespace Geolocation
} // namespace CognitiveRF

#endif // GEOLOCATION_CROSS_FIXING_SOLVER_HPP
